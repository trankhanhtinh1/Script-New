#pragma once
/*
 * NightSharp SDK / UI / IMenu — full ImGui port of EnsoulSharp.SDK.Core.UI.IMenu
 *
 * One header-only file that mirrors the EnsoulSharp menu API 1:1, minus the
 * Customizer/Skins layers (ImGui handles theming for us).
 *
 *   - AMenuComponent (Abstracts/)        ← base for everything
 *     - Menu                              ← sub-menu container
 *     - RadioMenu                         ← exclusive bool group
 *     - MenuItem (abstract leaf)          ← every value-bearing entry
 *       - MenuBool       (Values/)
 *       - MenuSlider     (Values/)
 *       - MenuSliderF    (Values/) — float slider
 *       - MenuKeyBind    (Values/)
 *       - MenuList       (Values/)        — non-templated, holds string options
 *       - MenuButton     (Values/)
 *       - MenuColor      (Values/)
 *       - MenuSeparator  (Values/)
 *       - MenuSliderButton (Values/)
 *
 *   - MenuManager: singleton owning all root Menus, drives draw + WndProc.
 *
 * Plugin usage (mirrors EnsoulSharp):
 *
 *   using namespace SDK::UI;
 *   auto* root  = new Menu("yasuo", "Yasuo - NightSharp", true);
 *   auto* combo = root->AddSubMenu(new Menu("combo", "Combo"));
 *   combo->Add(new MenuBool("useQ", "Use Q", true));
 *   combo->Add(new MenuKeyBind("flyhack", "Fly Hack",
 *                               SDK::Keys::T, SDK::KeyBindType::Toggle))
 *        ->AddPermashow();        // append to PermaShow registry
 *   root->Attach();
 *
 *   // read values
 *   if (combo->Get<MenuKeyBind>("flyhack")->Active) { ... }
 */

#include <Windows.h>
#include <cstdint>
#include <cstddef>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "../../imgui/imgui.h"
#include "../Enumerations/KeyBindType.h"

namespace SDK { namespace UI {

    // ---------- Forward declarations ----------
    class AMenuComponent;
    class Menu;
    class MenuItem;
    class MenuManager;
    class MenuValueChangedEventArgs;
    class MenuBool;
    class MenuSlider;
    class MenuSliderF;
    class MenuKeyBind;
    class MenuList;
    class MenuButton;
    class MenuRuntime;
    class MenuColor;
    class MenuSeparator;
    class MenuSliderButton;

    // ---------- Enums ----------
    enum class MenuValueType : int {
        None      = 0,
        Boolean   = 1,
        Slider    = 2,
        SliderF   = 3,
        KeyBind   = 4,
        List      = 5,
        Button    = 6,
        Color     = 7,
        Separator = 8,
        SliderBtn = 9,
        Runtime   = 10,
    };

    using KeyBindType = ::SDK::KeyBindType;

    // Set by the overlay WndProc (which can see Game::IsChatOpen()) right before
    // it dispatches key input. While true (chat box open) key presses must NOT
    // toggle/activate keybinds — the user is typing, not playing. Kept as a plain
    // flag to avoid a UI->Game include dependency here.
    inline bool g_KeybindInputBlocked = false;

    // Config-persistence hooks (installed by ConfigStore). Kept as plain function
    // pointers so the UI layer has zero dependency on the config layer.
    //   g_MenuValueChangedHook — fired whenever any MenuItem value changes, so the
    //       config store can debounce-save the owning root menu.
    //   g_MenuAttachedHook     — fired when a root Menu attaches to MenuManager, so
    //       the config store can apply previously-saved values to the fresh menu.
    //   g_MenuRemovedHook      — fired before a root is removed, so pending native
    //       menu values are flushed before the plugin destroys its tree.
    inline void (*g_MenuValueChangedHook)(MenuItem*) = nullptr;
    inline void (*g_MenuAttachedHook)(Menu*)        = nullptr;
    inline void (*g_MenuRemovedHook)(Menu*)         = nullptr;
    inline void (*g_MenuSystemResetHook)()          = nullptr;

    // NightSharp sidebar-like chrome for functional items (MenuBool, MenuList, …).
    // Enabled by NightSharpMenu while drawing the content panel.
    struct FunctionalMenuStyle {
        bool  enabled     = false;
        float itemHeight  = 30.0f;
        float padX        = 12.0f;
        ImU32 colItem     = IM_COL32(18, 20, 30, 118);
        ImU32 colItemHover  = IM_COL32(52, 48, 82, 215);
        ImU32 colItemActive = IM_COL32(82, 66, 132, 232);
        ImU32 colBorder   = IM_COL32(88, 100, 148, 180);
        ImU32 colText     = IM_COL32(255, 255, 255, 255);
        ImU32 colTextDim  = IM_COL32(185, 185, 205, 255);
        ImU32 colAccent   = IM_COL32(120, 235, 120, 255);
    };
    inline FunctionalMenuStyle g_FunctionalMenuStyle;

    struct FunctionalMenuRowState {
        ImVec2 origin{};
        float  width  = 0.0f;
        float  height = 0.0f;
    };
    inline FunctionalMenuRowState g_FunctionalMenuRow{};
    inline int g_FunctionalMenuRowNest = 0;
    inline int g_FunctionalMenuDepth = 0;

    inline void BeginFunctionalMenuRow(const void* id, bool isActive = false) {
        if (!g_FunctionalMenuStyle.enabled) {
            return;
        }

        auto& style = g_FunctionalMenuStyle;
        ImGui::PushID(id);
        ++g_FunctionalMenuRowNest;

        const float w = ImGui::GetContentRegionAvail().x;
        const float h = style.itemHeight > 1.0f ? style.itemHeight : 30.0f;
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1(p0.x + w, p0.y + h);

        g_FunctionalMenuRow.origin = p0;
        g_FunctionalMenuRow.width = w;
        g_FunctionalMenuRow.height = h;

        const bool hovered = ImGui::IsMouseHoveringRect(p0, p1, false);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float indentAmount = 16.0f;
        const float currentIndent = static_cast<float>(g_FunctionalMenuDepth) * indentAmount;

        if (dl) {
            // Draw background rectangle only on hover or active (same as sidebar)
            if (hovered || isActive) {
                const ImU32 bg = isActive
                    ? style.colItemActive
                    : style.colItemHover;
                dl->AddRectFilled(p0, p1, bg, 0.0f);
            } else {
                dl->AddRectFilled(p0, p1, style.colItem, 0.0f);
            }

            // Bottom border line (same as sidebar bottom line)
            dl->AddLine(ImVec2(p0.x, p1.y), ImVec2(p1.x, p1.y), style.colBorder, 1.0f);

            // Left indicator line when active (same as sidebar active indicator)
            if (isActive) {
                float lineX = p0.x + currentIndent + 1.0f;
                dl->AddLine(ImVec2(lineX, p0.y + 2.0f), ImVec2(lineX, p1.y - 2.0f), style.colAccent, 2.0f);
            }

            // Draw vertical hierarchy guides
            for (int i = 0; i < g_FunctionalMenuDepth; ++i) {
                float guideX = p0.x + style.padX + static_cast<float>(i) * indentAmount + 6.0f;
                dl->AddLine(ImVec2(guideX, p0.y), ImVec2(guideX, p1.y), (style.colBorder & 0x00FFFFFF) | 0x40000000, 1.0f);
            }
        }

        // Vertically center controls inside the fixed-height cell.
        const float frameH = ImGui::GetFrameHeight();
        float innerY = p0.y + (h - frameH) * 0.5f;
        if (innerY < p0.y + 1.0f) {
            innerY = p0.y + 1.0f;
        }
        ImGui::SetCursorScreenPos(ImVec2(p0.x + style.padX + currentIndent, innerY));
        ImGui::BeginGroup();
        const float innerW = w - style.padX * 2.0f - currentIndent;
        ImGui::PushItemWidth(innerW > 40.0f ? innerW : 40.0f);
    }

    inline void EndFunctionalMenuRow() {
        if (!g_FunctionalMenuStyle.enabled || g_FunctionalMenuRowNest <= 0) {
            return;
        }

        ImGui::PopItemWidth();
        ImGui::EndGroup();

        const ImVec2 p0 = g_FunctionalMenuRow.origin;
        const float w = g_FunctionalMenuRow.width;
        const float h = g_FunctionalMenuRow.height;
        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + h));
        ImGui::Dummy(ImVec2(w > 0.0f ? w : 1.0f, 0.0f));

        --g_FunctionalMenuRowNest;
        ImGui::PopID();
    }

    // True when the caller should open its own cell (e.g. Core panel DrawOnOffEditor).
    inline bool FunctionalMenuRowNeedsSelfWrap() {
        return g_FunctionalMenuStyle.enabled && g_FunctionalMenuRowNest == 0;
    }

    inline bool DrawStateToggleButton(const char* id,
                                      const char* label,
                                      bool active,
                                      bool positive,
                                      const ImVec2& size) {
        ImVec4 activeBase = positive
            ? ImVec4(0.18f, 0.55f, 0.28f, 0.98f)
            : ImVec4(0.65f, 0.22f, 0.24f, 0.98f);
        ImVec4 activeHover = positive
            ? ImVec4(0.22f, 0.64f, 0.32f, 1.0f)
            : ImVec4(0.75f, 0.27f, 0.29f, 1.0f);
        ImVec4 activePress = positive
            ? ImVec4(0.15f, 0.48f, 0.24f, 1.0f)
            : ImVec4(0.58f, 0.18f, 0.20f, 1.0f);
        ImVec4 inactiveBase = ImVec4(0.14f, 0.16f, 0.24f, 0.95f);
        ImVec4 inactiveHover = ImVec4(0.20f, 0.24f, 0.34f, 0.98f);
        ImVec4 inactivePress = ImVec4(0.24f, 0.28f, 0.40f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, active ? activeBase : inactiveBase);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? activeHover : inactiveHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active ? activePress : inactivePress);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        bool clicked = ImGui::Button(label, size);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
        return clicked;
    }

    inline bool DrawOnOffEditor(const char* label, bool& value, const char* idSuffix = nullptr) {
        bool changed = false;
        const bool selfRow = FunctionalMenuRowNeedsSelfWrap();
        if (selfRow) {
            BeginFunctionalMenuRow(idSuffix ? static_cast<const void*>(idSuffix) : static_cast<const void*>(label));
        }

        ImGui::PushID(idSuffix ? idSuffix : label);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label ? label : "");
        ImGui::SameLine();

        const float totalWidth = 86.0f;
        const float targetX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - totalWidth;
        if (targetX > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(targetX);
        }

        if (DrawStateToggleButton("##on", "On", value, true, ImVec2(40.0f, 0.0f)) && !value) {
            value = true;
            changed = true;
        }
        ImGui::SameLine(0.0f, 6.0f);
        if (DrawStateToggleButton("##off", "Off", !value, false, ImVec2(40.0f, 0.0f)) && value) {
            value = false;
            changed = true;
        }

        ImGui::PopID();
        if (selfRow) {
            EndFunctionalMenuRow();
        }
        return changed;
    }

    inline float ClampFloat(float value, float minValue, float maxValue) {
        if (maxValue < minValue) {
            maxValue = minValue;
        }
        if (value < minValue) {
            return minValue;
        }
        if (value > maxValue) {
            return maxValue;
        }
        return value;
    }

    inline float MenuControlWidth(float preferredWidth) {
        const float avail = ImGui::GetContentRegionAvail().x;
        if (avail <= 0.0f) {
            return preferredWidth;
        }
        if (avail < 120.0f) {
            return avail;
        }
        return ClampFloat(preferredWidth, 120.0f, avail);
    }

    inline float BeginMenuValueRow(const char* label, float preferredControlWidth = 280.0f) {
        ImGui::AlignTextToFramePadding();

        const char* text = label ? label : "";
        const float startX = ImGui::GetCursorPosX();
        const float avail = ImGui::GetContentRegionAvail().x;
        const float controlWidth = MenuControlWidth(preferredControlWidth);
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float labelLimit = avail - controlWidth - spacing;
        const bool sameLine =
            labelLimit >= 80.0f &&
            ImGui::CalcTextSize(text).x <= labelLimit;

        if (sameLine) {
            ImGui::TextUnformatted(text);
            ImGui::SameLine();
        } else {
            ImGui::PushTextWrapPos(startX + avail);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
        }

        const float targetX = startX + avail - controlWidth;
        if (targetX > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(targetX);
        }
        return controlWidth;
    }

    // ---------- Tiny POD vector (no STL) ----------
    template <typename T>
    class TinyVec {
    public:
        TinyVec() : m_data(nullptr), m_size(0), m_cap(0) {}
        ~TinyVec() { clear(); ::operator delete(m_data); }

        TinyVec(const TinyVec&) = delete;
        TinyVec& operator=(const TinyVec&) = delete;

        int  size() const { return m_size; }
        bool empty() const { return m_size == 0; }
        T&   operator[](int i) { return m_data[i]; }
        const T& operator[](int i) const { return m_data[i]; }

        void push_back(const T& v) {
            ensure(m_size + 1);
            new (&m_data[m_size]) T(v);
            ++m_size;
        }

        void erase(int i) {
            if (i < 0 || i >= m_size) return;
            m_data[i].~T();
            for (int k = i; k < m_size - 1; ++k) {
                new (&m_data[k]) T(m_data[k + 1]);
                m_data[k + 1].~T();
            }
            --m_size;
        }

        void clear() {
            for (int i = 0; i < m_size; ++i) m_data[i].~T();
            m_size = 0;
        }

    private:
        void ensure(int need) {
            if (need <= m_cap) return;
            int newCap = m_cap ? m_cap : 4;
            while (newCap < need) newCap *= 2;
            T* newData = (T*)::operator new(sizeof(T) * (size_t)newCap);
            for (int i = 0; i < m_size; ++i) {
                new (&newData[i]) T(m_data[i]);
                m_data[i].~T();
            }
            ::operator delete(m_data);
            m_data = newData;
            m_cap  = newCap;
        }

        T*  m_data;
        int m_size;
        int m_cap;
    };

    // ---------- Tiny string ----------
    class TinyString {
    public:
        TinyString() { m_buf[0] = 0; }
        TinyString(const char* s) { assign(s); }
        TinyString(const TinyString& o) { assign(o.c_str()); }
        TinyString& operator=(const TinyString& o) { if (this != &o) assign(o.c_str()); return *this; }
        TinyString& operator=(const char* s) { assign(s); return *this; }

        void assign(const char* s) {
            if (!s) { m_buf[0] = 0; return; }
            int i = 0;
            while (s[i] && i < 127) { m_buf[i] = s[i]; ++i; }
            m_buf[i] = 0;
        }

        const char* c_str() const { return m_buf; }
        bool empty() const { return m_buf[0] == 0; }

        bool equals(const char* s) const {
            if (!s) return false;
            int i = 0;
            while (m_buf[i] && s[i]) {
                if (m_buf[i] != s[i]) return false;
                ++i;
            }
            return m_buf[i] == s[i];
        }

    private:
        char m_buf[128];
    };

    // ============================================================
    // AMenuComponent — base
    // ============================================================
    class AMenuComponent {
    public:
        TinyString Name;
        TinyString DisplayName;
        TinyString UniqueString;
        AMenuComponent* Parent  = nullptr;
        bool   Root             = false;
        bool   Visible          = true;
        // EnsoulSharp menus start collapsed and open one sibling branch at a time.
        bool   Toggled          = false;
        bool   HaveCustomColor  = false;
        ImU32  FontColor        = IM_COL32(255, 255, 255, 255);
        bool   HaveAnimatedGradient = false;
        ImU32  GradientColorFrom = IM_COL32(255, 170, 64, 255);
        ImU32  GradientColorTo   = IM_COL32(156, 64, 255, 255);
        float  GradientSpeed     = 1.0f;

        AMenuComponent() = default;
        AMenuComponent(const char* name, const char* displayName, const char* uniqueString = "")
            : Name(name), DisplayName(displayName), UniqueString(uniqueString) {}
        virtual ~AMenuComponent() = default;

        virtual MenuValueType Kind() const { return MenuValueType::None; }
        virtual bool IsMenu() const { return false; }

        AMenuComponent* SetFontColor(ImU32 color) {
            HaveCustomColor = true;
            HaveAnimatedGradient = false;
            FontColor = color;
            return this;
        }

        AMenuComponent* ClearFontColor() {
            HaveCustomColor = false;
            return this;
        }

        AMenuComponent* SetAnimatedGradientText(ImU32 colorFrom,
                                                ImU32 colorTo,
                                                float speed = 1.0f) {
            HaveAnimatedGradient = true;
            GradientColorFrom = colorFrom;
            GradientColorTo = colorTo;
            GradientSpeed = speed > 0.01f ? speed : 0.01f;
            return this;
        }

        AMenuComponent* ClearAnimatedGradientText() {
            HaveAnimatedGradient = false;
            return this;
        }

        virtual void DrawImGui() = 0;          // render this node into the current ImGui context
        virtual void RestoreDefault() {}
        virtual void Save(void*) {}            // future-proof: optional persistence
        virtual void Load(void*) {}
    };

    // ============================================================
    // MenuValueChangedEventArgs
    // ============================================================
    class MenuValueChangedEventArgs {
    public:
        Menu*     Source = nullptr;
        MenuItem* Item   = nullptr;
        MenuValueChangedEventArgs(Menu* s, MenuItem* i) : Source(s), Item(i) {}
    };

    // ============================================================
    // MenuItem (abstract leaf)
    // ============================================================
    class MenuItem : public AMenuComponent {
    public:
        // EnsoulSharp-style ValueChanged callback. Plugins can set this directly.
        // void(*ValueChanged)(MenuItem* sender) — POD callback for manual-map safety.
        typedef void (*ValueChangedFn)(MenuItem* sender, void* userData);
        ValueChangedFn ValueChanged    = nullptr;
        void*          ValueChangedUd  = nullptr;
        TinyString     Tooltip;

        MenuItem() = default;
        MenuItem(const char* name, const char* displayName, const char* uniqueString = "")
            : AMenuComponent(name, displayName, uniqueString) {}

        // Cast helper used everywhere (MenuItem*->As<MenuBool>()->Value).
        template <typename T> T* As() { return static_cast<T*>(this); }
        template <typename T> const T* As() const { return static_cast<const T*>(this); }

        // Convenience for plugins (mirrors `comboMenu["useQ"].GetValue<MenuBool>()`).
        template <typename T> T* GetValue() { return static_cast<T*>(this); }
        template <typename T> const T* GetValue() const { return static_cast<const T*>(this); }

        MenuItem* SetTooltip(const char* tooltip) {
            Tooltip = tooltip ? tooltip : "";
            return this;
        }

        // Append this item to PermaShow. Returns `this` for chaining.
        // Implementation lives in PermaShow.h to avoid include cycles; declared
        // here as inline so the chained call compiles in any TU.
        MenuItem* AddPermashow(const char* customName = nullptr,
                               unsigned int color    = 0xFFFFFFFF);
        MenuItem* Permashow(const char* customName = nullptr,
                            unsigned int color    = 0xFFFFFFFF) {
            return AddPermashow(customName, color);
        }

        // Remove this item from PermaShow.
        MenuItem* RemovePermashow();

        void FireValueChanged() {
            if (ValueChanged) ValueChanged(this, ValueChangedUd);
            if (g_MenuValueChangedHook) g_MenuValueChangedHook(this);
        }

        void DrawTooltipIfHovered() const {
            if (!Tooltip.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", Tooltip.c_str());
            }
        }
    };

    // ============================================================
    // Menu — container of components
    // ============================================================
    class Menu : public AMenuComponent {
    public:
        TinyVec<AMenuComponent*> Components;
        bool OwnsComponents = true;   // delete children in dtor
        bool HaveLogo = false;
        ImTextureID MenuLogo = nullptr;
        TinyString MenuLogoKey;
        float MenuLogoWidth = 24.0f;
        float MenuLogoHeight = 24.0f;

        // Optional aggregate handler for any value change in the subtree.
        typedef void (*MenuValueChangedFn)(MenuValueChangedEventArgs args, void* ud);
        MenuValueChangedFn MenuValueChanged   = nullptr;
        void*              MenuValueChangedUd = nullptr;

        Menu() = default;
        Menu(const char* name, const char* displayName, bool root = false, const char* uniqueString = "")
            : AMenuComponent(name, displayName, uniqueString) {
            Root = root;
        }

        ~Menu() override {
            if (OwnsComponents) {
                for (int i = 0; i < Components.size(); ++i) {
                    delete Components[i];
                }
            }
            Components.clear();
        }

        bool IsMenu() const override { return true; }

        Menu* SetLogo(ImTextureID texture, float width = 24.0f, float height = 24.0f) {
            HaveLogo = texture != nullptr;
            MenuLogo = texture;
            MenuLogoKey = "";
            MenuLogoWidth = width > 0.0f ? width : 24.0f;
            MenuLogoHeight = height > 0.0f ? height : 24.0f;
            return this;
        }

        Menu* SetLogo(const char* iconKey, float width = 24.0f, float height = 24.0f) {
            HaveLogo = iconKey && iconKey[0];
            MenuLogo = nullptr;
            MenuLogoKey = iconKey ? iconKey : "";
            MenuLogoWidth = width > 0.0f ? width : 24.0f;
            MenuLogoHeight = height > 0.0f ? height : 24.0f;
            return this;
        }

        Menu* ClearLogo() {
            HaveLogo = false;
            MenuLogo = nullptr;
            MenuLogoKey = "";
            return this;
        }

        static Menu* Create(const char* name, const char* displayName, bool attach = true);

        // Add any component (Menu / MenuItem). Returns the same pointer for chaining.
        template <typename T>
        T* Add(T* component) {
            if (!component) return nullptr;
            // Reject duplicate names — keep the existing one to mirror EnsoulSharp.
            for (int i = 0; i < Components.size(); ++i) {
                if (Components[i]->Name.equals(component->Name.c_str())) {
                    return dynamic_cast<T*>(Components[i]);
                }
            }
            component->Parent = this;
            Components.push_back(component);
            return component;
        }

        template <typename T, typename... Args>
        T* Add(Args&&... args) {
            static_assert(std::is_base_of<AMenuComponent, T>::value,
                          "Menu::Add<T> expects a menu component type");
            return Add(new T(std::forward<Args>(args)...));
        }

        template <typename T>
        T* AddItem(T* component) { return Add(component); }

        template <typename T, typename... Args>
        T* AddItem(Args&&... args) { return Add<T>(std::forward<Args>(args)...); }

        // Convenience overload to keep `menu->Add(new MenuBool(...))` reading nicely
        // when the user wants the typed pointer back.
        Menu* AddSubMenu(Menu* sub) { return Add(sub); }
        Menu* AddSubMenu(const char* name, const char* displayName = nullptr) {
            if (Menu* existing = GetSubMenu(name)) return existing;
            return AddSubMenu(new Menu(name, displayName && displayName[0] ? displayName : name));
        }
        Menu* AddSubMenu(const std::string& name, const std::string& displayName) {
            return AddSubMenu(name.c_str(), displayName.c_str());
        }
        Menu* AddSubMenu(const std::string& name) {
            return AddSubMenu(name.c_str(), name.c_str());
        }

        // Lookup by name. Returns nullptr if not found.
        AMenuComponent* Find(const char* name) {
            if (!name) return nullptr;
            for (int i = 0; i < Components.size(); ++i) {
                if (Components[i]->Name.equals(name)) return Components[i];
            }
            return nullptr;
        }

        const AMenuComponent* Find(const char* name) const {
            if (!name) return nullptr;
            for (int i = 0; i < Components.size(); ++i) {
                if (Components[i]->Name.equals(name)) return Components[i];
            }
            return nullptr;
        }

        AMenuComponent* operator[](const char* name) { return Find(name); }
        const AMenuComponent* operator[](const char* name) const { return Find(name); }

        // Typed lookup. T must derive from MenuItem or Menu.
        template <typename T>
        T* Get(const char* name) {
            return dynamic_cast<T*>(Find(name));
        }

        template <typename T>
        const T* Get(const char* name) const {
            return dynamic_cast<const T*>(Find(name));
        }

        MenuItem* Item(const char* name);
        const MenuItem* Item(const char* name) const;
        Menu* GetSubMenu(const char* name);
        const Menu* GetSubMenu(const char* name) const;
        Menu* SubMenu(const char* name) { return GetSubMenu(name); }
        const Menu* SubMenu(const char* name) const { return GetSubMenu(name); }
        bool HasItem(const char* name) const { return Item(name) != nullptr || GetSubMenu(name) != nullptr; }

        bool GetBoolValue(const char* name, bool fallback = false) const;
        int GetSliderValue(const char* name, int fallback = 0) const;
        float GetSliderFValue(const char* name, float fallback = 0.0f) const;
        bool GetKeyBindValue(const char* name, bool fallback = false) const;
        int GetListIndex(const char* name, int fallback = 0) const;
        unsigned int GetColorValue(const char* name, unsigned int fallback = 0xFFFFFFFFu) const;

        // Attach to MenuManager — only legal for root menus with no parent.
        Menu* Attach();

        // Render menu and all children into the current ImGui content panel.
        void DrawImGui() override;

        // Render the items (used by MenuManager when this is the active root).
        void DrawChildren();

        // Bubble a child value change up to handlers.
        void FireMenuValueChanged(MenuItem* sender) {
            if (MenuValueChanged) MenuValueChanged(MenuValueChangedEventArgs(this, sender), MenuValueChangedUd);
            if (Parent && Parent->IsMenu()) static_cast<Menu*>(Parent)->FireMenuValueChanged(sender);
        }
    };

    // ============================================================
    // RadioMenu — when one MenuBool turns on, all siblings turn off.
    // ============================================================
    class RadioMenu : public Menu {
    public:
        RadioMenu(const char* name, const char* displayName, bool root = false, const char* uniqueString = "")
            : Menu(name, displayName, root, uniqueString) {}

        // Called from MenuBool::Set when our parent is a RadioMenu.
        void EnforceRadio(MenuItem* changed);
    };

    // ============================================================
    // Values/MenuBool
    // ============================================================
    class MenuBool : public MenuItem {
    public:
        union {
            bool Value;
            bool Enabled;
        };
        bool Original;

        MenuBool(const char* name, const char* displayName, bool value = true, const char* uniqueString = "")
            : MenuItem(name, displayName, uniqueString), Value(value), Original(value) {}

        MenuValueType Kind() const override { return MenuValueType::Boolean; }
        void RestoreDefault() override { Value = Original; }

        MenuBool* SetValue(bool v) {
            Set(v);
            Original = v;
            return this;
        }

        void Set(bool v) {
            if (v == Value) return;
            Value = v;
            FireValueChanged();
            if (Parent && Parent->IsMenu()) {
                Menu* p = static_cast<Menu*>(Parent);
                p->FireMenuValueChanged(this);
                // If parent is a RadioMenu, enforce single-on.
                if (RadioMenu* r = dynamic_cast<RadioMenu*>(p)) r->EnforceRadio(this);
            }
        }

        void DrawImGui() override {
            ImGui::PushID(this);
            bool v = Value;
            if (DrawOnOffEditor(DisplayName.c_str(), v, Name.c_str())) Set(v);
            DrawTooltipIfHovered();
            ImGui::PopID();
        }
    };

    inline void RadioMenu::EnforceRadio(MenuItem* changed) {
        MenuBool* trigger = dynamic_cast<MenuBool*>(changed);
        if (!trigger || !trigger->Value) return;
        for (int i = 0; i < Components.size(); ++i) {
            MenuBool* b = dynamic_cast<MenuBool*>(Components[i]);
            if (b && b != trigger && b->Value) b->Set(false);
        }
    }

    // ============================================================
    // Values/MenuSlider (int)
    // ============================================================
    class MenuSlider : public MenuItem {
    public:
        int  Value;
        int  MinValue;
        int  MaxValue;
        int  Original;
        bool Interacting = false;

        MenuSlider(const char* name, const char* displayName,
                   int value = 0, int minValue = 0, int maxValue = 100,
                   const char* uniqueString = "")
            : MenuItem(name, displayName, uniqueString),
              Value(value < minValue ? minValue : (value > maxValue ? maxValue : value)),
              MinValue(minValue), MaxValue(maxValue), Original(value) {}

        MenuValueType Kind() const override { return MenuValueType::Slider; }
        void RestoreDefault() override { Set(Original); }

        MenuSlider* SetValue(int v) {
            Set(v);
            Original = Value;
            return this;
        }

        void Set(int v) {
            if (v < MinValue) v = MinValue;
            if (v > MaxValue) v = MaxValue;
            if (v == Value) return;
            Value = v;
            FireValueChanged();
            if (Parent && Parent->IsMenu()) static_cast<Menu*>(Parent)->FireMenuValueChanged(this);
        }

        void DrawImGui() override {
            ImGui::PushID(this);
            int v = Value;
            const float width = BeginMenuValueRow(DisplayName.c_str(), 280.0f);
            ImGui::SetNextItemWidth(width);
            if (ImGui::SliderInt("##value", &v, MinValue, MaxValue, "%d")) Set(v);
            Interacting = ImGui::IsItemActive();
            DrawTooltipIfHovered();
            ImGui::PopID();
        }
    };

    // ============================================================
    // Values/MenuSliderF (float)
    // ============================================================
    class MenuSliderF : public MenuItem {
    public:
        float Value;
        float MinValue;
        float MaxValue;
        float Original;
        bool  Interacting = false;

        MenuSliderF(const char* name, const char* displayName,
                    float value = 0.f, float minValue = 0.f, float maxValue = 1.f,
                    const char* uniqueString = "")
            : MenuItem(name, displayName, uniqueString),
              Value(value < minValue ? minValue : (value > maxValue ? maxValue : value)),
              MinValue(minValue), MaxValue(maxValue), Original(value) {}

        MenuValueType Kind() const override { return MenuValueType::SliderF; }
        void RestoreDefault() override { Set(Original); }

        MenuSliderF* SetValue(float v) {
            Set(v);
            Original = Value;
            return this;
        }

        void Set(float v) {
            if (v < MinValue) v = MinValue;
            if (v > MaxValue) v = MaxValue;
            if (v == Value) return;
            Value = v;
            FireValueChanged();
            if (Parent && Parent->IsMenu()) static_cast<Menu*>(Parent)->FireMenuValueChanged(this);
        }

        void DrawImGui() override {
            ImGui::PushID(this);
            float v = Value;
            const float width = BeginMenuValueRow(DisplayName.c_str(), 280.0f);
            ImGui::SetNextItemWidth(width);
            if (ImGui::SliderFloat("##value", &v, MinValue, MaxValue, "%.2f")) Set(v);
            Interacting = ImGui::IsItemActive();
            DrawTooltipIfHovered();
            ImGui::PopID();
        }
    };

    // ============================================================
    // Values/MenuKeyBind
    // ============================================================
    class MenuKeyBind : public MenuItem {
    public:
        int          Key;          // virtual-key code
        KeyBindType  Type;
        bool         Active;       // current active state (toggle or pressed)
        bool         Interacting;  // user is rebinding
        int          Original;
        bool         OriginalActive;
        bool         WasDown;

        MenuKeyBind(const char* name, const char* displayName,
                    int key, KeyBindType type)
            : MenuKeyBind(name, displayName, key, type, false, "") {}

        MenuKeyBind(const char* name, const char* displayName,
                    int key, KeyBindType type, bool defaultActive,
                    const char* uniqueString = "")
            : MenuItem(name, displayName, uniqueString),
              Key(key), Type(type), Active(defaultActive), Interacting(false),
              Original(key), OriginalActive(defaultActive), WasDown(false) {}

        MenuKeyBind(const char* name, const char* displayName,
                    int key, KeyBindType type, const char* uniqueString)
            : MenuKeyBind(name, displayName, key, type, false, uniqueString) {}

        MenuValueType Kind() const override { return MenuValueType::KeyBind; }
        void RestoreDefault() override { Key = Original; SetActive(OriginalActive); }

        MenuKeyBind* SetValue(bool active) {
            SetActive(active);
            OriginalActive = Active;
            return this;
        }

        void SetActive(bool v) {
            if (v == Active) return;
            Active = v;
            FireValueChanged();
            if (Parent && Parent->IsMenu()) static_cast<Menu*>(Parent)->FireMenuValueChanged(this);
        }

        void SetKey(int vkCode) {
            if (vkCode <= 0 || vkCode == Key) return;
            Key = vkCode;
            WasDown = false;
            FireValueChanged();
            if (Parent && Parent->IsMenu()) static_cast<Menu*>(Parent)->FireMenuValueChanged(this);
        }

        // Process a key event from the global WndProc. Returns true if handled.
        bool OnKey(int vkCode, bool down) {
            if (Interacting && down) {
                if (vkCode != VK_ESCAPE) {
                    SetKey(vkCode);
                }
                Interacting = false;
                return true;
            }
            if (vkCode != Key) return false;
            const bool firstDown = down && !WasDown;
            WasDown = down;

            // While the chat box is open, ignore key input for activation so
            // typing does not (a) toggle keybinds like Fly Hack / Auto W, or
            // (b) hold a Press keybind (combo) active. A Press keybind can never
            // be active while input is blocked, which also releases a key that
            // was held before chat opened (fixes the stuck-combo auto-cast).
            const bool inputBlocked = g_KeybindInputBlocked;
            if (Type == KeyBindType::Press) {
                const bool wantActive = down && !inputBlocked;
                if (Active != wantActive) SetActive(wantActive);
            } else { // Toggle on key-down only
                if (firstDown && !inputBlocked) SetActive(!Active);
            }
            return false; // do not consume so the host can still see it
        }

        static const char* TypeToText(KeyBindType type) {
            if (type == KeyBindType::Toggle) return "Toggle";
            return "Press";
        }

        static const char* VkToText(int vk) {
            static char buf[8];
            // Letters / digits use the ASCII directly.
            if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
                buf[0] = (char)vk; buf[1] = 0; return buf;
            }
            switch (vk) {
                case VK_SPACE:    return "Space";
                case VK_RETURN:   return "Enter";
                case VK_LSHIFT:   return "LShift";
                case VK_RSHIFT:   return "RShift";
                case VK_SHIFT:    return "Shift";
                case VK_CONTROL:  return "Ctrl";
                case VK_LCONTROL: return "LCtrl";
                case VK_RCONTROL: return "RCtrl";
                case VK_MENU:     return "Alt";
                case VK_TAB:      return "Tab";
                case VK_CAPITAL:  return "Caps";
                case VK_LBUTTON:  return "LMB";
                case VK_RBUTTON:  return "RMB";
                case VK_MBUTTON:  return "MMB";
                case VK_XBUTTON1: return "XB1";
                case VK_XBUTTON2: return "XB2";
                case VK_F1: return "F1"; case VK_F2: return "F2";
                case VK_F3: return "F3"; case VK_F4: return "F4";
                case VK_F5: return "F5"; case VK_F6: return "F6";
                case VK_F7: return "F7"; case VK_F8: return "F8";
                case VK_F9: return "F9"; case VK_F10: return "F10";
                case VK_F11: return "F11"; case VK_F12: return "F12";
                default: { ::wsprintfA(buf, "0x%X", vk); return buf; }
            }
        }

        void DrawImGui() override {
            ImGui::PushID(this);

            const float width = BeginMenuValueRow(DisplayName.c_str(), 300.0f);
            const float gap = 6.0f;
            const float typeWidth = width >= 260.0f ? 72.0f : 58.0f;
            const float activeWidth = width >= 260.0f ? 50.0f : 44.0f;
            const float keyWidth = width - typeWidth - activeWidth - gap * 2.0f;

            ImGui::BeginGroup();
            ImGui::PushStyleColor(
                ImGuiCol_Button,
                Interacting ? ImVec4(0.20f, 0.39f, 0.72f, 0.95f)
                            : ImVec4(0.10f, 0.17f, 0.28f, 0.78f));
            ImGui::PushStyleColor(
                ImGuiCol_ButtonHovered,
                Interacting ? ImVec4(0.28f, 0.50f, 0.88f, 1.0f)
                            : ImVec4(0.16f, 0.28f, 0.46f, 0.92f));
            ImGui::PushStyleColor(
                ImGuiCol_ButtonActive,
                Interacting ? ImVec4(0.16f, 0.32f, 0.60f, 1.0f)
                            : ImVec4(0.20f, 0.36f, 0.58f, 1.0f));
            if (ImGui::Button(Interacting ? "Press key..." : VkToText(Key), ImVec2(keyWidth, 0.0f))) {
                Interacting = true;
            }
            ImGui::PopStyleColor(3);
            DrawTooltipIfHovered();

            ImGui::SameLine(0.0f, gap);
            ImGui::Button(TypeToText(Type), ImVec2(typeWidth, 0.0f));

            ImGui::SameLine(0.0f, gap);
            if (DrawStateToggleButton("##active", Active ? "ON" : "OFF", Active, true, ImVec2(activeWidth, 0.0f))) {
                SetActive(!Active);
            }
            ImGui::EndGroup();

            ImGui::PopID();
        }
    };

    // ============================================================
    // Values/MenuList — string options, untemplated to keep ABI simple
    // ============================================================
    class MenuList : public MenuItem {
    public:
        TinyVec<TinyString> Options;
        int Index;
        int Original;

        MenuList(const char* name, const char* displayName,
                 const char** options, int optCount, int defaultIndex = 0,
                 const char* uniqueString = "")
            : MenuItem(name, displayName, uniqueString), Index(defaultIndex), Original(defaultIndex) {
            for (int i = 0; i < optCount; ++i) Options.push_back(TinyString(options[i]));
            if (Index < 0) Index = 0;
            if (optCount > 0 && Index >= optCount) Index = optCount - 1;
        }

        MenuList(const char* name, const char* displayName,
                 const std::vector<std::string>& options, int defaultIndex = 0,
                 const char* uniqueString = "")
            : MenuItem(name, displayName, uniqueString), Index(defaultIndex), Original(defaultIndex) {
            for (const auto& option : options) Options.push_back(TinyString(option.c_str()));
            if (Index < 0) Index = 0;
            if (!Options.empty() && Index >= Options.size()) Index = Options.size() - 1;
        }

        MenuList(const char* name, const char* displayName,
                 std::initializer_list<const char*> options, int defaultIndex = 0,
                 const char* uniqueString = "")
            : MenuItem(name, displayName, uniqueString), Index(defaultIndex), Original(defaultIndex) {
            for (const char* option : options) Options.push_back(TinyString(option));
            if (Index < 0) Index = 0;
            if (!Options.empty() && Index >= Options.size()) Index = Options.size() - 1;
        }

        MenuValueType Kind() const override { return MenuValueType::List; }
        void RestoreDefault() override { Set(Original); }

        const char* SelectedValue() const {
            if (Options.empty() || Index < 0 || Index >= Options.size()) return "";
            return Options[Index].c_str();
        }

        MenuList* SetValue(int idx) {
            Set(idx);
            Original = Index;
            return this;
        }

        void Set(int idx) {
            if (Options.empty()) return;
            if (idx < 0) idx = 0;
            if (idx >= Options.size()) idx = Options.size() - 1;
            if (idx == Index) return;
            Index = idx;
            FireValueChanged();
            if (Parent && Parent->IsMenu()) static_cast<Menu*>(Parent)->FireMenuValueChanged(this);
        }

        void DrawImGui() override {
            ImGui::PushID(this);

            if (Options.empty()) {
                const float controlWidth = BeginMenuValueRow(DisplayName.c_str(), 212.0f);
                ImGui::TextDisabled("-");
                DrawTooltipIfHovered();
                ImGui::PopID();
                return;
            }

            if (Index < 0) {
                Index = 0;
            }
            if (Index >= Options.size()) {
                Index = Options.size() - 1;
            }

            const float controlWidth = BeginMenuValueRow(DisplayName.c_str(), 212.0f);
            ImGui::SetNextItemWidth(controlWidth);

            const bool styled = g_FunctionalMenuStyle.enabled;
            if (styled) {
                ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(8.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(g_FunctionalMenuStyle.colBorder));
                ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
            }

            if (ImGui::BeginCombo("##combo", SelectedValue(), ImGuiComboFlags_None)) {
                const bool applyItemStyle = g_FunctionalMenuStyle.enabled;
                if (applyItemStyle) {
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
                }

                for (int i = 0; i < Options.size(); ++i) {
                    const bool isSelected = (Index == i);
                    ImVec2 size = applyItemStyle ? ImVec2(0.0f, g_FunctionalMenuStyle.itemHeight) : ImVec2(0.0f, 0.0f);
                    if (ImGui::Selectable(Options[i].c_str(), isSelected, ImGuiSelectableFlags_None, size)) {
                        Set(i);
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                if (applyItemStyle) {
                    ImGui::PopStyleVar();
                }
                ImGui::EndCombo();
            }

            if (styled) {
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);
            }

            DrawTooltipIfHovered();
            ImGui::PopID();
        }
    };

    // ============================================================
    // Values/MenuButton
    // ============================================================
    class MenuButton : public MenuItem {
    public:
        TinyString  ButtonText;
        typedef void (*ActionFn)(MenuButton* sender, void* ud);
        ActionFn    Action     = nullptr;
        void*       ActionUd   = nullptr;
        std::function<void()> Callback;

        MenuButton(const char* name, const char* displayName, const char* buttonText,
                   const char* uniqueString = "")
            : MenuItem(name, displayName, uniqueString), ButtonText(buttonText) {}

        MenuButton(const char* name, const char* displayName, const char* buttonText,
                   ActionFn action, void* userData = nullptr,
                   const char* uniqueString = "")
            : MenuItem(name, displayName, uniqueString), ButtonText(buttonText),
              Action(action), ActionUd(userData) {}

        template <typename Fn,
                  typename = std::enable_if_t<!std::is_convertible_v<Fn, const char*>>>
        MenuButton(const char* name, const char* displayName, const char* buttonText,
                   Fn&& fn)
            : MenuItem(name, displayName, ""), ButtonText(buttonText),
              Callback(std::forward<Fn>(fn)) {}

        MenuValueType Kind() const override { return MenuValueType::Button; }

        void DrawImGui() override {
            ImGui::PushID(this);
            const float width = BeginMenuValueRow(DisplayName.c_str(), 136.0f);
            DrawTooltipIfHovered();
            if (ImGui::Button(ButtonText.c_str(), ImVec2(width, 0.0f))) {
                if (Callback) Callback();
                else if (Action) Action(this, ActionUd);
                FireValueChanged();
            }
            ImGui::PopID();
        }
    };

    // ============================================================
    // Values/MenuRuntime — compatibility leaf for legacy RuntimeMenu callbacks.
    // The EnsoulSharp theme opens the callback in a flat, adjacent panel.
    // New plugins should still prefer native MenuItem components.
    // ============================================================
    class MenuRuntime : public MenuItem {
    public:
        typedef void (*DrawFn)(void* userData);
        DrawFn DrawCallback = nullptr;
        void* DrawUserData = nullptr;
        bool Open = false;
        float MinimumWidth = 300.0f;

        MenuRuntime(const char* name,
                    const char* displayName,
                    DrawFn callback,
                    void* userData = nullptr,
                    float minimumWidth = 300.0f,
                    const char* uniqueString = "")
            : MenuItem(name, displayName, uniqueString),
              DrawCallback(callback),
              DrawUserData(userData),
              MinimumWidth(minimumWidth > 200.0f ? minimumWidth : 200.0f) {}

        MenuValueType Kind() const override { return MenuValueType::Runtime; }

        void DrawImGui() override {
            if (DrawCallback) DrawCallback(DrawUserData);
        }
    };

    // ============================================================
    // Values/MenuColor
    // ============================================================
    class MenuColor : public MenuItem {
    public:
        // Color as ImU32 (AABBGGRR — same packing as ImGui).
        unsigned int Value;
        unsigned int Original;
        bool         Active = false;

        MenuColor(const char* name, const char* displayName,
                  unsigned int color = 0xFFFFFFFFu, const char* uniqueString = "")
            : MenuItem(name, displayName, uniqueString), Value(color), Original(color) {}

        MenuColor(const char* name, const char* displayName,
                  float r, float g, float b, float a = 1.0f,
                  const char* uniqueString = "")
            : MenuItem(name, displayName, uniqueString),
              Value(ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a))),
              Original(Value) {}

        MenuValueType Kind() const override { return MenuValueType::Color; }
        void RestoreDefault() override { Set(Original); }

        unsigned int GetImU32() const { return Value; }

        MenuColor* SetValue(unsigned int v) {
            Set(v);
            Original = Value;
            return this;
        }

        void Set(unsigned int v) {
            if (v == Value) return;
            Value = v;
            FireValueChanged();
            if (Parent && Parent->IsMenu()) static_cast<Menu*>(Parent)->FireMenuValueChanged(this);
        }

        void DrawImGui() override {
            ImGui::PushID(this);
            ImVec4 c = ImGui::ColorConvertU32ToFloat4(Value);
            const float width = BeginMenuValueRow(DisplayName.c_str(), 136.0f);
            ImGui::SetNextItemWidth(width);
            if (ImGui::ColorEdit4("##cl", &c.x,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
                Set(ImGui::ColorConvertFloat4ToU32(c));
            }
            DrawTooltipIfHovered();
            ImGui::PopID();
        }
    };

    // ============================================================
    // Values/MenuSeparator
    // ============================================================
    class MenuSeparator : public MenuItem {
    public:
        MenuSeparator(const char* name, const char* displayName = "",
                      const char* uniqueString = "")
            : MenuItem(name, displayName[0] ? displayName : name, uniqueString) {}

        MenuValueType Kind() const override { return MenuValueType::Separator; }

        void DrawImGui() override {
            if (!DisplayName.empty()) {
                ImGui::TextColored(ImVec4(0.55f, 0.78f, 1.0f, 1.0f), "%s", DisplayName.c_str());
                DrawTooltipIfHovered();
            }
            ImGui::Separator();
        }
    };

    // ============================================================
    // Values/MenuSliderButton — int slider + boolean enable
    // ============================================================
    class MenuSliderButton : public MenuItem {
    public:
        union {
            int Value;
            int SValue;
        };
        int  MinValue;
        int  MaxValue;
        union {
            bool Enabled;
            bool BValue;
        };
        int  OriginalSlider;
        bool OriginalBool;

        MenuSliderButton(const char* name, const char* displayName,
                         int value = 0, int minValue = 0, int maxValue = 100,
                         bool bValue = false, const char* uniqueString = "")
            : MenuItem(name, displayName, uniqueString),
              Value(value < minValue ? minValue : (value > maxValue ? maxValue : value)),
              MinValue(minValue), MaxValue(maxValue), Enabled(bValue),
              OriginalSlider(value), OriginalBool(bValue) {}

        MenuValueType Kind() const override { return MenuValueType::SliderBtn; }

        int EffectiveValue() const { return (Enabled && Value != MinValue) ? Value : -1; }
        int GetValue() const { return Value; }

        MenuSliderButton* SetValue(int v) {
            if (v < MinValue) v = MinValue;
            if (v > MaxValue) v = MaxValue;
            if (v != Value) {
                Value = v;
                FireValueChanged();
                if (Parent && Parent->IsMenu()) static_cast<Menu*>(Parent)->FireMenuValueChanged(this);
            }
            OriginalSlider = Value;
            return this;
        }

        MenuSliderButton* SetEnabled(bool enabled) {
            if (enabled != Enabled) {
                Enabled = enabled;
                FireValueChanged();
                if (Parent && Parent->IsMenu()) static_cast<Menu*>(Parent)->FireMenuValueChanged(this);
            }
            OriginalBool = Enabled;
            return this;
        }

        void RestoreDefault() override {
            Value = OriginalSlider; Enabled = OriginalBool;
            FireValueChanged();
            if (Parent && Parent->IsMenu()) static_cast<Menu*>(Parent)->FireMenuValueChanged(this);
        }

        void DrawImGui() override {
            ImGui::PushID(this);
            const float width = BeginMenuValueRow(DisplayName.c_str(), 320.0f);
            const float checkboxWidth = 24.0f;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            bool b = Enabled;
            if (ImGui::Checkbox("##en", &b)) {
                Enabled = b;
                FireValueChanged();
                if (Parent && Parent->IsMenu()) static_cast<Menu*>(Parent)->FireMenuValueChanged(this);
            }
            DrawTooltipIfHovered();
            ImGui::SameLine(0.0f, spacing);
            int v = Value;
            float sliderWidth = width - checkboxWidth - spacing;
            if (sliderWidth < 64.0f) {
                sliderWidth = 64.0f;
            }
            ImGui::SetNextItemWidth(sliderWidth);
            if (ImGui::SliderInt("##sl", &v, MinValue, MaxValue)) {
                if (v < MinValue) v = MinValue;
                if (v > MaxValue) v = MaxValue;
                if (v != Value) {
                    Value = v;
                    FireValueChanged();
                    if (Parent && Parent->IsMenu()) static_cast<Menu*>(Parent)->FireMenuValueChanged(this);
                }
            }
            ImGui::PopID();
        }
    };

    // ============================================================
    // MenuManager — singleton root list + WndProc dispatcher
    // ============================================================
    class MenuManager {
    public:
        static MenuManager& Instance() {
            static MenuManager s;
            return s;
        }

        TinyVec<Menu*> Menus;

        // Called by Menu::Attach.
        void Add(Menu* m) {
            for (int i = 0; i < Menus.size(); ++i) if (Menus[i] == m) return;
            Menus.push_back(m);
        }

        void Remove(Menu* m) {
            for (int i = 0; i < Menus.size(); ++i) {
                if (Menus[i] == m) {
                    if (g_MenuRemovedHook) {
                        g_MenuRemovedHook(m);
                    }
                    Menus.erase(i);
                    return;
                }
            }
        }

        void Clear() {
            if (g_MenuRemovedHook) {
                for (int i = 0; i < Menus.size(); ++i) {
                    if (Menus[i]) {
                        g_MenuRemovedHook(Menus[i]);
                    }
                }
            }
            Menus.clear();
        }

        // Walk every MenuKeyBind in a subtree and let it process this key event.
        bool DispatchKey(int vkCode, bool down) {
            bool handled = false;
            const bool captureOnly = HasKeyBindCapture();
            for (int i = 0; i < Menus.size(); ++i) {
                handled = DispatchKeyInternal(Menus[i], vkCode, down, captureOnly) || handled;
            }
            return handled;
        }

        bool HasKeyBindCapture() const {
            for (int i = 0; i < Menus.size(); ++i) {
                if (HasKeyBindCaptureInternal(Menus[i])) {
                    return true;
                }
            }
            return false;
        }

        bool TranslateInput(UINT msg, WPARAM wParam, LPARAM, int& vkCode, bool& down) const {
            vkCode = 0;
            down = false;

            switch (msg) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                vkCode = static_cast<int>(wParam);
                down = true;
                return true;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                vkCode = static_cast<int>(wParam);
                down = false;
                return true;
            case WM_LBUTTONDOWN:
                vkCode = VK_LBUTTON;
                down = true;
                return true;
            case WM_LBUTTONUP:
                vkCode = VK_LBUTTON;
                down = false;
                return true;
            case WM_RBUTTONDOWN:
                vkCode = VK_RBUTTON;
                down = true;
                return true;
            case WM_RBUTTONUP:
                vkCode = VK_RBUTTON;
                down = false;
                return true;
            case WM_MBUTTONDOWN:
                vkCode = VK_MBUTTON;
                down = true;
                return true;
            case WM_MBUTTONUP:
                vkCode = VK_MBUTTON;
                down = false;
                return true;
            case WM_XBUTTONDOWN:
                vkCode = HIWORD(static_cast<DWORD_PTR>(wParam)) == XBUTTON2
                    ? VK_XBUTTON2
                    : VK_XBUTTON1;
                down = true;
                return true;
            case WM_XBUTTONUP:
                vkCode = HIWORD(static_cast<DWORD_PTR>(wParam)) == XBUTTON2
                    ? VK_XBUTTON2
                    : VK_XBUTTON1;
                down = false;
                return true;
            default:
                return false;
            }
        }

        bool DispatchInput(UINT msg, WPARAM wParam, LPARAM lParam) {
            int vkCode = 0;
            bool down = false;
            if (!TranslateInput(msg, wParam, lParam, vkCode, down)) {
                return false;
            }
            return DispatchKey(vkCode, down);
        }

        bool DispatchCapturedInput(UINT msg, WPARAM wParam, LPARAM lParam) {
            return HasKeyBindCapture() && DispatchInput(msg, wParam, lParam);
        }

    private:
        MenuManager() = default;

        bool DispatchKeyInternal(AMenuComponent* node, int vkCode, bool down, bool captureOnly) {
            if (!node) return false;
            if (MenuKeyBind* k = dynamic_cast<MenuKeyBind*>(node)) {
                if (captureOnly && !k->Interacting) {
                    return false;
                }
                return k->OnKey(vkCode, down);
            }
            bool handled = false;
            if (Menu* m = dynamic_cast<Menu*>(node)) {
                for (int i = 0; i < m->Components.size(); ++i) {
                    handled = DispatchKeyInternal(m->Components[i], vkCode, down, captureOnly) || handled;
                }
            }
            return handled;
        }

        bool HasKeyBindCaptureInternal(const AMenuComponent* node) const {
            if (!node) return false;
            if (const MenuKeyBind* k = dynamic_cast<const MenuKeyBind*>(node)) {
                return k->Interacting;
            }
            if (const Menu* m = dynamic_cast<const Menu*>(node)) {
                for (int i = 0; i < m->Components.size(); ++i) {
                    if (HasKeyBindCaptureInternal(m->Components[i])) {
                        return true;
                    }
                }
            }
            return false;
        }
    };

    // ---------- Menu helpers + DrawImGui implementations ----------
    inline Menu* Menu::Create(const char* name, const char* displayName, bool attach) {
        Menu* menu = new Menu(name, displayName, true);
        if (attach) menu->Attach();
        return menu;
    }

    inline MenuItem* Menu::Item(const char* name) {
        return dynamic_cast<MenuItem*>(Find(name));
    }

    inline const MenuItem* Menu::Item(const char* name) const {
        return dynamic_cast<const MenuItem*>(Find(name));
    }

    inline Menu* Menu::GetSubMenu(const char* name) {
        return const_cast<Menu*>(static_cast<const Menu*>(this)->GetSubMenu(name));
    }

    inline const Menu* Menu::GetSubMenu(const char* name) const {
        if (!name || !name[0]) return nullptr;

        const char* slash = name;
        while (*slash && *slash != '/') ++slash;
        if (!*slash) return dynamic_cast<const Menu*>(Find(name));

        char head[128] = {};
        int i = 0;
        const char* p = name;
        while (p < slash && i < 127) head[i++] = *p++;
        head[i] = 0;

        const Menu* sub = dynamic_cast<const Menu*>(Find(head));
        return sub ? sub->GetSubMenu(slash + 1) : nullptr;
    }

    inline bool Menu::GetBoolValue(const char* name, bool fallback) const {
        if (const auto* item = dynamic_cast<const MenuBool*>(Item(name))) return item->Value;
        return fallback;
    }

    inline int Menu::GetSliderValue(const char* name, int fallback) const {
        if (const auto* item = dynamic_cast<const MenuSlider*>(Item(name))) return item->Value;
        if (const auto* item = dynamic_cast<const MenuSliderButton*>(Item(name))) return item->Value;
        return fallback;
    }

    inline float Menu::GetSliderFValue(const char* name, float fallback) const {
        if (const auto* item = dynamic_cast<const MenuSliderF*>(Item(name))) return item->Value;
        return fallback;
    }

    inline bool Menu::GetKeyBindValue(const char* name, bool fallback) const {
        if (const auto* item = dynamic_cast<const MenuKeyBind*>(Item(name))) return item->Active;
        return fallback;
    }

    inline int Menu::GetListIndex(const char* name, int fallback) const {
        if (const auto* item = dynamic_cast<const MenuList*>(Item(name))) return item->Index;
        return fallback;
    }

    inline unsigned int Menu::GetColorValue(const char* name, unsigned int fallback) const {
        if (const auto* item = dynamic_cast<const MenuColor*>(Item(name))) return item->Value;
        return fallback;
    }

    inline Menu* Menu::Attach() {
        if (Parent != nullptr || !Root) return this; // EnsoulSharp throws; we just no-op
        MenuManager::Instance().Add(this);
        if (g_MenuAttachedHook) g_MenuAttachedHook(this);
        return this;
    }

    inline void Menu::DrawChildren() {
        for (int i = 0; i < Components.size(); ++i) {
            AMenuComponent* c = Components[i];
            if (!c || !c->Visible) {
                continue;
            }

            // Nested menus draw their own sidebar-style header row.
            if (g_FunctionalMenuStyle.enabled && !c->IsMenu()) {
                BeginFunctionalMenuRow(c);
                c->DrawImGui();
                EndFunctionalMenuRow();
            } else {
                c->DrawImGui();
            }
        }
    }

    inline void Menu::DrawImGui() {
        // Sub-menu: collapsing header. Root menus draw children directly because
        // NightSharpMenu already provides the outer panel.
        if (Root) {
            DrawChildren();
            return;
        }

        // Sidebar-style expandable row (same chrome as MenuBool / MenuList cells).
        if (g_FunctionalMenuStyle.enabled) {
            BeginFunctionalMenuRow(this, Toggled);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(DisplayName.c_str());
            {
                const char* arrow = Toggled ? "v" : ">";
                const float arrowW = ImGui::CalcTextSize(arrow).x;
                const float targetX =
                    ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - arrowW;
                if (targetX > ImGui::GetCursorPosX()) {
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(targetX);
                    ImGui::TextColored(
                        ImGui::ColorConvertU32ToFloat4(g_FunctionalMenuStyle.colTextDim),
                        "%s",
                        arrow);
                }
            }
            // Click anywhere on the cell toggles open/closed.
            if (ImGui::IsMouseHoveringRect(
                    g_FunctionalMenuRow.origin,
                    ImVec2(
                        g_FunctionalMenuRow.origin.x + g_FunctionalMenuRow.width,
                        g_FunctionalMenuRow.origin.y + g_FunctionalMenuRow.height),
                    false) &&
                ImGui::IsMouseClicked(0) &&
                !ImGui::IsAnyItemActive()) {
                Toggled = !Toggled;
            }
            EndFunctionalMenuRow();

            if (Toggled) {
                g_FunctionalMenuDepth++;
                DrawChildren();
                g_FunctionalMenuDepth--;
            }
            return;
        }

        ImGui::PushID(this);
        if (ImGui::CollapsingHeader(DisplayName.c_str(),
                ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(8.0f);
            DrawChildren();
            ImGui::Unindent(8.0f);
        }
        ImGui::PopID();
    }

}} // namespace SDK::UI

namespace SDK {

    namespace MenuUI {
        using AMenuComponent = ::SDK::UI::AMenuComponent;
        using MenuItem = ::SDK::UI::MenuItem;
        using MenuManager = ::SDK::UI::MenuManager;
        using MenuValueChangedEventArgs = ::SDK::UI::MenuValueChangedEventArgs;
        using RadioMenu = ::SDK::UI::RadioMenu;

        class Menu : public ::SDK::UI::Menu {
        public:
            Menu() = default;
            Menu(const char* name)
                : Menu(name, name) {}
            Menu(const char* name, const char* displayName, bool root = false,
                 const char* uniqueString = "")
                : ::SDK::UI::Menu(name, displayName, root, uniqueString) {}

            static Menu* Create(const char* name, const char* displayName, bool attach = true) {
                Menu* menu = new Menu(name, displayName, true);
                if (attach) menu->Attach();
                return menu;
            }

            Menu* AddSubMenu(Menu* sub) {
                return dynamic_cast<Menu*>(::SDK::UI::Menu::AddSubMenu(sub));
            }

            ::SDK::UI::Menu* AddSubMenu(::SDK::UI::Menu* sub) {
                return ::SDK::UI::Menu::AddSubMenu(sub);
            }

            Menu* AddSubMenu(const char* name, const char* displayName = nullptr) {
                if (Menu* existing = GetSubMenu(name)) return existing;
                return AddSubMenu(new Menu(name, displayName && displayName[0] ? displayName : name));
            }

            Menu* AddSubMenu(const std::string& name, const std::string& displayName) {
                return AddSubMenu(name.c_str(), displayName.c_str());
            }

            Menu* AddSubMenu(const std::string& name) {
                return AddSubMenu(name.c_str(), name.c_str());
            }

            Menu* GetSubMenu(const char* name) {
                return dynamic_cast<Menu*>(::SDK::UI::Menu::GetSubMenu(name));
            }

            const Menu* GetSubMenu(const char* name) const {
                return dynamic_cast<const Menu*>(::SDK::UI::Menu::GetSubMenu(name));
            }

            Menu* SubMenu(const char* name) { return GetSubMenu(name); }
            const Menu* SubMenu(const char* name) const { return GetSubMenu(name); }
        };

        using MenuBool = ::SDK::UI::MenuBool;
        using MenuSlider = ::SDK::UI::MenuSlider;
        using MenuSliderF = ::SDK::UI::MenuSliderF;
        using MenuKeyBind = ::SDK::UI::MenuKeyBind;
        using MenuList = ::SDK::UI::MenuList;
        using MenuButton = ::SDK::UI::MenuButton;
        using MenuRuntime = ::SDK::UI::MenuRuntime;
        using MenuColor = ::SDK::UI::MenuColor;
        using MenuSeparator = ::SDK::UI::MenuSeparator;
        using MenuSliderButton = ::SDK::UI::MenuSliderButton;
        using KeyBindType = ::SDK::KeyBindType;
    }

    using AMenuComponent = ::SDK::UI::AMenuComponent;
    using Menu = ::SDK::MenuUI::Menu;
    using MenuItem = ::SDK::UI::MenuItem;
    using MenuManager = ::SDK::UI::MenuManager;
    using MenuValueChangedEventArgs = ::SDK::UI::MenuValueChangedEventArgs;
    using RadioMenu = ::SDK::UI::RadioMenu;

    using MenuBool = ::SDK::UI::MenuBool;
    using MenuSlider = ::SDK::UI::MenuSlider;
    using MenuSliderF = ::SDK::UI::MenuSliderF;
    using MenuKeyBind = ::SDK::UI::MenuKeyBind;
    using MenuList = ::SDK::UI::MenuList;
    using MenuButton = ::SDK::UI::MenuButton;
    using MenuRuntime = ::SDK::UI::MenuRuntime;
    using MenuColor = ::SDK::UI::MenuColor;
    using MenuSeparator = ::SDK::UI::MenuSeparator;
    using MenuSliderButton = ::SDK::UI::MenuSliderButton;
}

namespace SDK::UI::IMenu {
    using AMenuComponent = ::SDK::UI::AMenuComponent;
    using Menu = ::SDK::UI::Menu;
    using MenuItem = ::SDK::UI::MenuItem;
    using MenuManager = ::SDK::UI::MenuManager;
    using MenuValueChangedEventArgs = ::SDK::UI::MenuValueChangedEventArgs;
    using RadioMenu = ::SDK::UI::RadioMenu;

    using MenuBool = ::SDK::UI::MenuBool;
    using MenuSlider = ::SDK::UI::MenuSlider;
    using MenuSliderF = ::SDK::UI::MenuSliderF;
    using MenuKeyBind = ::SDK::UI::MenuKeyBind;
    using MenuList = ::SDK::UI::MenuList;
    using MenuButton = ::SDK::UI::MenuButton;
    using MenuRuntime = ::SDK::UI::MenuRuntime;
    using MenuColor = ::SDK::UI::MenuColor;
    using MenuSeparator = ::SDK::UI::MenuSeparator;
    using MenuSliderButton = ::SDK::UI::MenuSliderButton;

    namespace Values {
        using MenuBool = ::SDK::UI::MenuBool;
        using MenuSlider = ::SDK::UI::MenuSlider;
        using MenuSliderF = ::SDK::UI::MenuSliderF;
        using MenuKeyBind = ::SDK::UI::MenuKeyBind;
        using MenuList = ::SDK::UI::MenuList;
        using MenuButton = ::SDK::UI::MenuButton;
        using MenuRuntime = ::SDK::UI::MenuRuntime;
        using MenuColor = ::SDK::UI::MenuColor;
        using MenuSeparator = ::SDK::UI::MenuSeparator;
        using MenuSliderButton = ::SDK::UI::MenuSliderButton;
    }
}

#include "PermaShow.h"

#ifndef NIGHTSHARP_SDK_UI_NO_GLOBAL_ALIASES
using AMenuComponent = ::SDK::AMenuComponent;
using Menu = ::SDK::Menu;
using MenuItem = ::SDK::MenuItem;
using MenuManager = ::SDK::MenuManager;
using MenuValueChangedEventArgs = ::SDK::MenuValueChangedEventArgs;
using RadioMenu = ::SDK::RadioMenu;

using MenuBool = ::SDK::MenuBool;
using MenuSlider = ::SDK::MenuSlider;
using MenuSliderF = ::SDK::MenuSliderF;
using MenuKeyBind = ::SDK::MenuKeyBind;
using MenuList = ::SDK::MenuList;
using MenuButton = ::SDK::MenuButton;
using MenuRuntime = ::SDK::MenuRuntime;
using MenuColor = ::SDK::MenuColor;
using MenuSeparator = ::SDK::MenuSeparator;
using MenuSliderButton = ::SDK::MenuSliderButton;
using KeyBindType = ::SDK::KeyBindType;
#endif
