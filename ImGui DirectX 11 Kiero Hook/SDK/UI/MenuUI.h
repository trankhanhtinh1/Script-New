#pragma once
#include "imgui/imgui.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <algorithm>

// ============================================================================
// MenuUI — EnsoulSharp-compatible menu system for scripts
// Reference: EnsoulSharp.SDK.MenuUI (Menu, MenuBool, MenuSlider, MenuList, etc.)
//
// Usage (in script/plugin):
//   auto menu = SDK::MenuUI::Menu::Create("MyScript", "My Script");
//   menu->Add<SDK::MenuUI::MenuBool>("Enable", "Enable Script", true);
//   menu->Add<SDK::MenuUI::MenuSlider>("Range", "Cast Range", 900, 0, 2000);
//   menu->Add<SDK::MenuUI::MenuList>("Mode", "Target Mode", {"Smart","LowHP","Priority"}, 0);
//   menu->Add<SDK::MenuUI::MenuKeyBind>("Combo", "Combo Key", VK_SPACE, SDK::MenuUI::KeyBindType::Press);
//
//   // Reading values:
//   bool enabled = menu->Get<SDK::MenuUI::MenuBool>("Enable")->Enabled;
//   int range = menu->Get<SDK::MenuUI::MenuSlider>("Range")->Value;
//   int mode = menu->Get<SDK::MenuUI::MenuList>("Mode")->Index;
//   bool comboActive = menu->Get<SDK::MenuUI::MenuKeyBind>("Combo")->Active;
//
//   // In OnMenu() callback:
//   menu->Draw();
// ============================================================================

namespace SDK {
namespace MenuUI {

    // ========================================================================
    // KeyBind types
    // ========================================================================
    enum class KeyBindType {
        Press,   // Active while held
        Toggle   // Toggles on/off on press
    };

    // ========================================================================
    // MenuValueChangedEventArgs — Event data for value change callbacks
    // Source: EnsoulSharp.SDK/Core/UI/IMenu/MenuValueChangedEventArgs.cs
    // ========================================================================
    class Menu; // Forward declaration
    class MenuItem; // Forward declaration
    class RadioMenu; // Forward declaration

    struct MenuValueChangedEventArgs {
        Menu* OwnerMenu;        // The menu that contains the changed item
        MenuItem* ChangedItem;  // The MenuItem that changed value
        std::string ItemName;   // InternalName of the changed item

        MenuValueChangedEventArgs() : OwnerMenu(nullptr), ChangedItem(nullptr) {}
        MenuValueChangedEventArgs(Menu* menu, MenuItem* item, const std::string& name)
            : OwnerMenu(menu), ChangedItem(item), ItemName(name) {}
    };

    using OnValueChangedFn = std::function<void(const MenuValueChangedEventArgs&)>;

    // ========================================================================
    // Base MenuItem
    // ========================================================================
    class MenuItem {
    public:
        std::string InternalName;
        std::string DisplayName;
        std::string Tooltip;            // Hover tooltip text

        virtual ~MenuItem() = default;
        virtual void Draw() = 0;

        MenuItem(const std::string& name, const std::string& display)
            : InternalName(name), DisplayName(display) {}

        /// Set tooltip text (fluent API)
        MenuItem& SetTooltip(const std::string& tooltip) {
            Tooltip = tooltip;
            return *this;
        }

        /// Register a value change callback (EnsoulSharp: MenuItem.ValueChanged)
        void OnValueChanged(OnValueChangedFn callback) {
            m_valueChangedCallbacks.push_back(callback);
        }

        /// Draw tooltip if hovered (call after Draw() in each subclass if needed)
        void DrawTooltip() const {
            if (!Tooltip.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", Tooltip.c_str());
            }
        }

    protected:
        std::vector<OnValueChangedFn> m_valueChangedCallbacks;

        /// Fire value changed event — call from subclass Draw() when value changes
        void FireValueChanged() {
            MenuValueChangedEventArgs args(nullptr, this, InternalName);
            for (auto& cb : m_valueChangedCallbacks) {
                cb(args);
            }
        }
    };

    // ========================================================================
    // MenuBool
    // ========================================================================
    class MenuBool : public MenuItem {
    public:
        bool Enabled;

        MenuBool(const std::string& name, const std::string& display, bool defaultValue = true)
            : MenuItem(name, display), Enabled(defaultValue), m_prev(defaultValue) {}

        void Draw() override {
            ImGui::Checkbox(DisplayName.c_str(), &Enabled);
            if (Enabled != m_prev) { m_prev = Enabled; FireValueChanged(); }
            DrawTooltip();
        }
    private:
        bool m_prev;
    };

    // ========================================================================
    // MenuSlider (int)
    // ========================================================================
    class MenuSlider : public MenuItem {
    public:
        int Value;
        int MinValue;
        int MaxValue;

        MenuSlider(const std::string& name, const std::string& display,
                   int defaultValue, int minVal, int maxVal)
            : MenuItem(name, display), Value(defaultValue), MinValue(minVal), MaxValue(maxVal), m_prev(defaultValue) {}

        void Draw() override {
            ImGui::SliderInt(DisplayName.c_str(), &Value, MinValue, MaxValue);
            if (Value != m_prev) { m_prev = Value; FireValueChanged(); }
            DrawTooltip();
        }
    private:
        int m_prev;
    };

    // ========================================================================
    // MenuSliderF (float)
    // ========================================================================
    class MenuSliderF : public MenuItem {
    public:
        float Value;
        float MinValue;
        float MaxValue;

        MenuSliderF(const std::string& name, const std::string& display,
                    float defaultValue, float minVal, float maxVal)
            : MenuItem(name, display), Value(defaultValue), MinValue(minVal), MaxValue(maxVal), m_prev(defaultValue) {}

        void Draw() override {
            ImGui::SliderFloat(DisplayName.c_str(), &Value, MinValue, MaxValue);
            if (Value != m_prev) { m_prev = Value; FireValueChanged(); }
            DrawTooltip();
        }
    private:
        float m_prev;
    };

    // ========================================================================
    // MenuList (dropdown / combo box)
    // ========================================================================
    class MenuList : public MenuItem {
    public:
        int Index;
        std::vector<std::string> Items;

        MenuList(const std::string& name, const std::string& display,
                 const std::vector<std::string>& items, int defaultIndex = 0)
            : MenuItem(name, display), Items(items), Index(defaultIndex), m_prev(defaultIndex) {}

        void Draw() override {
            if (ImGui::BeginCombo(DisplayName.c_str(), 
                (Index >= 0 && Index < (int)Items.size()) ? Items[Index].c_str() : "")) {
                for (int i = 0; i < (int)Items.size(); i++) {
                    bool selected = (i == Index);
                    if (ImGui::Selectable(Items[i].c_str(), selected))
                        Index = i;
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (Index != m_prev) { m_prev = Index; FireValueChanged(); }
            DrawTooltip();
        }
    private:
        int m_prev;
    };

    // ========================================================================
    // MenuColor
    // ========================================================================
    class MenuColor : public MenuItem {
    public:
        float Color[4]; // RGBA 0-1

        MenuColor(const std::string& name, const std::string& display,
                  float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f)
            : MenuItem(name, display) {
            Color[0] = r; Color[1] = g; Color[2] = b; Color[3] = a;
        }

        ImU32 GetImU32() const {
            return IM_COL32((int)(Color[0]*255), (int)(Color[1]*255),
                            (int)(Color[2]*255), (int)(Color[3]*255));
        }

        void Draw() override {
            ImGui::ColorEdit4(DisplayName.c_str(), Color, ImGuiColorEditFlags_AlphaBar);
            DrawTooltip();
        }
    };

    // ========================================================================
    // MenuKeyBind
    // ========================================================================
    class MenuKeyBind : public MenuItem {
    public:
        int Key;
        KeyBindType Type;
        bool Active;

        MenuKeyBind(const std::string& name, const std::string& display,
                    int key, KeyBindType type, bool defaultActive = false)
            : MenuItem(name, display), Key(key), Type(type), Active(defaultActive) {}

        void Update() {
            if (Type == KeyBindType::Press) {
                Active = (GetAsyncKeyState(Key) & 0x8000) != 0;
            } else { // Toggle
                static bool wasDown = false;
                bool isDown = (GetAsyncKeyState(Key) & 0x8000) != 0;
                if (isDown && !wasDown) Active = !Active;
                wasDown = isDown;
            }
        }

        void Draw() override {
            const char* keyName = GetKeyName(Key);
            char label[256];
            snprintf(label, sizeof(label), "%s [%s]%s", DisplayName.c_str(), keyName,
                     Type == KeyBindType::Toggle ? (Active ? " [ON]" : " [OFF]") : "");
            if (Type == KeyBindType::Toggle) {
                ImGui::Checkbox(label, &Active);
            } else {
                ImGui::Text("%s", label);
            }
        }

    private:
        static const char* GetKeyName(int vk) {
            switch (vk) {
            case VK_SPACE: return "Space";
            case VK_LBUTTON: return "LB";
            case VK_RBUTTON: return "RB";
            case VK_MBUTTON: return "MMB";
            case VK_LSHIFT: return "LShift";
            case VK_LCONTROL: return "LCtrl";
            case VK_CAPITAL: return "CapsLock";
            case VK_TAB: return "Tab";
            default:
                if (vk >= 'A' && vk <= 'Z') {
                    static char buf[2];
                    buf[0] = (char)vk; buf[1] = 0;
                    return buf;
                }
                return "?";
            }
        }
    };

    // ========================================================================
    // MenuInputText — Text input field (EnsoulSharp: MenuInputText)
    // ========================================================================
    class MenuInputText : public MenuItem {
    public:
        std::string Text;
        int MaxLength;

        MenuInputText(const std::string& name, const std::string& display,
                      const std::string& defaultText = "", int maxLen = 256)
            : MenuItem(name, display), Text(defaultText), MaxLength(maxLen) {
            m_buf.resize(maxLen + 1, '\0');
            if (!defaultText.empty()) {
                strncpy_s(m_buf.data(), m_buf.size(), defaultText.c_str(), maxLen);
            }
        }

        void Draw() override {
            if (ImGui::InputText(DisplayName.c_str(), m_buf.data(), m_buf.size())) {
                Text = m_buf.data();
            }
        }

    private:
        std::vector<char> m_buf;
    };

    // ========================================================================
    // MenuSeparator
    // ========================================================================
    class MenuSeparator : public MenuItem {
    public:
        MenuSeparator(const std::string& name = "", const std::string& display = "")
            : MenuItem(name, display) {}

        void Draw() override {
            if (!DisplayName.empty()) {
                ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "%s", DisplayName.c_str());
            }
            ImGui::Separator();
        }
    };

    // ========================================================================
    // Menu (container — can hold items + sub-menus)
    // ========================================================================
    class Menu : public MenuItem, public std::enable_shared_from_this<Menu> {
    public:
        Menu(const std::string& name, const std::string& display, bool isRoot = false)
            : MenuItem(name, display), m_isRoot(isRoot) {}

        // Add a menu item by type
        template<typename T, typename... Args>
        T* Add(Args&&... args) {
            auto item = std::make_shared<T>(std::forward<Args>(args)...);
            T* ptr = item.get();
            m_items.push_back(item);
            m_lookup[item->InternalName] = item;
            return ptr;
        }

        // Add a sub-menu
        std::shared_ptr<Menu> AddSubMenu(const std::string& name, const std::string& display) {
            auto sub = std::make_shared<Menu>(name, display, false);
            m_items.push_back(sub);
            m_lookup[name] = sub;
            return sub;
        }

        // Get item by internal name
        template<typename T>
        T* Get(const std::string& name) {
            auto it = m_lookup.find(name);
            if (it != m_lookup.end()) {
                return dynamic_cast<T*>(it->second.get());
            }
            return nullptr;
        }

        // Get sub-menu
        Menu* GetSubMenu(const std::string& name) {
            return Get<Menu>(name);
        }

        // Check if item exists
        MenuItem* operator[](const std::string& name) {
            auto it = m_lookup.find(name);
            return (it != m_lookup.end()) ? it->second.get() : nullptr;
        }

        // Update all keybinds recursively
        void UpdateKeyBinds() {
            for (auto& item : m_items) {
                auto* kb = dynamic_cast<MenuKeyBind*>(item.get());
                if (kb) kb->Update();
                auto* sub = dynamic_cast<Menu*>(item.get());
                if (sub) sub->UpdateKeyBinds();
            }
        }

        // Draw menu (ImGui)
        void Draw() override {
            if (m_isRoot) {
                // Root menu: draw all items directly
                for (auto& item : m_items) {
                    item->Draw();
                }
            } else {
                // Sub-menu: collapsing header
                if (ImGui::CollapsingHeader(DisplayName.c_str())) {
                    ImGui::Indent(10.0f);
                    for (auto& item : m_items) {
                        item->Draw();
                    }
                    ImGui::Unindent(10.0f);
                }
            }
        }

        const std::vector<std::shared_ptr<MenuItem>>& GetItems() const { return m_items; }

        // Factory: create a root menu
        static std::shared_ptr<Menu> Create(const std::string& name, const std::string& display) {
            auto menu = std::make_shared<Menu>(name, display, true);
            GetGlobalMenus().push_back(menu);
            return menu;
        }

        // Attach — register to global list (E# compatible: Menu(...).Attach())
        std::shared_ptr<Menu> Attach() {
            auto self = shared_from_this();
            GetGlobalMenus().push_back(self);
            return self;
        }

        // Remove from global menus
        static void Remove(const std::string& name) {
            auto& menus = GetGlobalMenus();
            menus.erase(std::remove_if(menus.begin(), menus.end(),
                [&](const std::shared_ptr<Menu>& m) { return m->InternalName == name; }),
                menus.end());
        }

        // Dispose — remove this menu from global list and clear items (EnsoulSharp: Menu.Dispose)
        void Dispose() {
            Remove(InternalName);
            m_items.clear();
            m_lookup.clear();
        }

        // Convenience: Item<T>("name") — alias for Get<T>("name")
        template<typename T>
        T* Item(const std::string& name) {
            return Get<T>(name);
        }

        // Add a RadioMenu (radio-button group) — defined after RadioMenu class
        inline std::shared_ptr<RadioMenu> AddRadioMenu(const std::string& name, const std::string& display);

        // Register menu-level OnValueChanged callback (fires for any child item)
        // Source: EnsoulSharp Menu.MenuValueChanged event
        void OnMenuValueChanged(OnValueChangedFn callback) {
            m_menuValueChangedCallbacks.push_back(callback);
        }

        // Fire menu-level value changed event
        void FireMenuValueChanged(MenuItem* item) {
            MenuValueChangedEventArgs args(this, item, item ? item->InternalName : "");
            for (auto& cb : m_menuValueChangedCallbacks) {
                cb(args);
            }
        }

        // Count items
        int Count() const { return (int)m_items.size(); }

        // Get all globally registered menus
        static std::vector<std::shared_ptr<Menu>>& GetGlobalMenus() {
            static std::vector<std::shared_ptr<Menu>> s_menus;
            return s_menus;
        }

        // Update all global keybinds
        static void UpdateAllKeyBinds() {
            for (auto& menu : GetGlobalMenus()) {
                menu->UpdateKeyBinds();
            }
        }

    private:
        bool m_isRoot;
        std::vector<std::shared_ptr<MenuItem>> m_items;
        std::unordered_map<std::string, std::shared_ptr<MenuItem>> m_lookup;
        std::vector<OnValueChangedFn> m_menuValueChangedCallbacks;
    };

    // ========================================================================
    // MenuButton
    // ========================================================================
    class MenuButton : public MenuItem {
    public:
        std::string ButtonText;
        std::function<void()> OnClick;

        MenuButton(const std::string& name, const std::string& display,
                   const std::string& btnText, std::function<void()> onClick = nullptr)
            : MenuItem(name, display), ButtonText(btnText), OnClick(onClick) {}

        void Draw() override {
            ImGui::Text("%s", DisplayName.c_str());
            ImGui::SameLine();
            if (ImGui::Button(ButtonText.c_str())) {
                if (OnClick) OnClick();
            }
        }
    };

    // ========================================================================
    // MenuSliderButton — Slider with an enable/disable button
    // Source: EnsoulSharp.SDK/Core/UI/IMenu/Values/MenuSliderButton.cs
    // ========================================================================
    // Usage:
    //   menu->Add<MenuSliderButton>("Range", "Cast Range", 900, 0, 2000, true);
    //   auto* sb = menu->Get<MenuSliderButton>("Range");
    //   int val = sb->Value;       // -1 if disabled or at min, else SValue
    //   int raw = sb->SValue;      // Always the slider value
    //   bool on = sb->BValue;      // Button enabled state
    // ========================================================================
    class MenuSliderButton : public MenuItem {
    public:
        int SValue;         // Slider value (always accessible)
        int MinValue;
        int MaxValue;
        bool BValue;        // Button enabled/disabled

        /// Value property: returns SValue if BValue && SValue != MinValue, else -1
        int Value() const {
            return (SValue != MinValue && BValue) ? SValue : -1;
        }

        MenuSliderButton(const std::string& name, const std::string& display,
                         int value = 0, int minVal = 0, int maxVal = 100,
                         bool bValue = false)
            : MenuItem(name, display), SValue(value), MinValue(minVal), MaxValue(maxVal),
              BValue(bValue), m_prevS(value), m_prevB(bValue) {
            // Clamp initial value
            if (SValue < MinValue) SValue = MinValue;
            if (SValue > MaxValue) SValue = MaxValue;
        }

        void Draw() override {
            // Draw slider and button on same line
            ImGui::PushID(InternalName.c_str());

            // Slider portion (takes most width)
            float avail = ImGui::GetContentRegionAvail().x;
            float btnWidth = 50.0f;
            ImGui::SetNextItemWidth(avail - btnWidth - 10.0f);
            ImGui::SliderInt("##slider", &SValue, MinValue, MaxValue);

            // Button portion
            ImGui::SameLine();
            if (BValue) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.45f, 0.8f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.55f, 0.9f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            }
            if (ImGui::Button(BValue ? "ON" : "OFF", ImVec2(btnWidth, 0))) {
                BValue = !BValue;
            }
            ImGui::PopStyleColor(2);

            ImGui::PopID();

            // Show display name as label
            ImGui::SameLine();
            ImGui::Text("%s", DisplayName.c_str());

            // Fire change event
            if (SValue != m_prevS || BValue != m_prevB) {
                m_prevS = SValue;
                m_prevB = BValue;
                FireValueChanged();
            }
            DrawTooltip();
        }

    private:
        int m_prevS;
        bool m_prevB;
    };

    // ========================================================================
    // RadioMenu — Menu with radio-button behavior (only one bool active)
    // Source: EnsoulSharp.SDK/Core/UI/IMenu/RadioMenu.cs
    // ========================================================================
    // Usage:
    //   auto radio = menu->AddRadioMenu("TargetMode", "Target Mode");
    //   radio->Add<MenuBool>("smart", "Smart", true);
    //   radio->Add<MenuBool>("lowHP", "Lowest HP", false);
    //   radio->Add<MenuBool>("closest", "Closest", false);
    //   // When "lowHP" is enabled, "smart" and "closest" auto-disable
    //
    //   std::string active = radio->GetActiveItem(); // "smart"
    //   int activeIdx = radio->GetActiveIndex();     // 0
    // ========================================================================
    class RadioMenu : public MenuItem, public std::enable_shared_from_this<RadioMenu> {
    public:
        RadioMenu(const std::string& name, const std::string& display)
            : MenuItem(name, display) {}

        // Add a radio option (MenuBool)
        MenuBool* AddOption(const std::string& name, const std::string& display, bool defaultValue = false) {
            auto item = std::make_shared<MenuBool>(name, display, defaultValue);
            MenuBool* ptr = item.get();
            m_options.push_back(item);

            // Register value changed callback for radio behavior
            ptr->OnValueChanged([this, name](const MenuValueChangedEventArgs& args) {
                auto* changedBool = dynamic_cast<MenuBool*>(args.ChangedItem);
                if (changedBool && changedBool->Enabled) {
                    // Disable all others
                    for (auto& opt : m_options) {
                        if (opt->InternalName != name) {
                            opt->Enabled = false;
                        }
                    }
                }
            });

            return ptr;
        }

        /// Get the internal name of the currently active option
        std::string GetActiveItem() const {
            for (auto& opt : m_options) {
                if (opt->Enabled) return opt->InternalName;
            }
            return "";
        }

        /// Get the index of the currently active option (-1 if none)
        int GetActiveIndex() const {
            for (int i = 0; i < (int)m_options.size(); i++) {
                if (m_options[i]->Enabled) return i;
            }
            return -1;
        }

        /// Set active option by name
        void SetActive(const std::string& name) {
            for (auto& opt : m_options) {
                opt->Enabled = (opt->InternalName == name);
            }
        }

        /// Set active option by index
        void SetActive(int index) {
            for (int i = 0; i < (int)m_options.size(); i++) {
                m_options[i]->Enabled = (i == index);
            }
        }

        void Draw() override {
            if (ImGui::CollapsingHeader(DisplayName.c_str())) {
                ImGui::Indent(10.0f);
                // Draw as radio buttons for clearer UX
                for (int i = 0; i < (int)m_options.size(); i++) {
                    bool active = m_options[i]->Enabled;
                    if (ImGui::RadioButton(m_options[i]->DisplayName.c_str(), active)) {
                        // Set this one active, disable all others
                        for (int j = 0; j < (int)m_options.size(); j++) {
                            m_options[j]->Enabled = (j == i);
                        }
                        FireValueChanged();
                    }
                }
                ImGui::Unindent(10.0f);
            }
        }

        const std::vector<std::shared_ptr<MenuBool>>& GetOptions() const { return m_options; }

    private:
        std::vector<std::shared_ptr<MenuBool>> m_options;
    };

    // ========================================================================
    // Menu::AddRadioMenu implementation (needs full RadioMenu definition)
    // ========================================================================
    inline std::shared_ptr<RadioMenu> Menu::AddRadioMenu(const std::string& name, const std::string& display) {
        auto radio = std::make_shared<RadioMenu>(name, display);
        m_items.push_back(radio);
        m_lookup[name] = radio;
        return radio;
    }

    // ========================================================================
    // MenuCustomizer — Theme/appearance customization
    // Source: EnsoulSharp.SDK/Core/UI/IMenu/Customizer/MenuCustomizer.cs
    // ========================================================================
    // Usage:
    //   MenuCustomizer::Init();  // Call once
    //   MenuCustomizer::Draw();  // Call in menu render loop
    // ========================================================================
    class MenuCustomizer {
    public:
        // Customizable properties
        static inline float FontScale = 1.0f;           // ImGui font scale
        static inline float MenuAlpha = 0.95f;           // Menu background alpha
        static inline float ItemSpacing = 4.0f;          // Spacing between items
        static inline float Rounding = 5.0f;             // Corner rounding
        static inline bool LockPosition = false;         // Lock menu position
        static inline float AccentColor[4] = {0.0f, 0.47f, 0.84f, 1.0f}; // Accent color (RGBA)
        static inline float BgColor[4] = {0.08f, 0.08f, 0.12f, 0.95f};   // Background color

        static void Init() {
            // Apply defaults from current ImGui style
            auto& style = ImGui::GetStyle();
            FontScale = ImGui::GetIO().FontGlobalScale;
            MenuAlpha = style.Alpha;
            ItemSpacing = style.ItemSpacing.y;
            Rounding = style.WindowRounding;
        }

        /// Apply current customization to ImGui style
        static void Apply() {
            auto& style = ImGui::GetStyle();
            ImGui::GetIO().FontGlobalScale = FontScale;
            style.Alpha = MenuAlpha;
            style.ItemSpacing.y = ItemSpacing;
            style.WindowRounding = Rounding;
            style.FrameRounding = Rounding * 0.6f;
            style.GrabRounding = Rounding * 0.4f;

            // Apply accent color
            ImVec4 accent(AccentColor[0], AccentColor[1], AccentColor[2], AccentColor[3]);
            ImVec4 accentHover(
                std::min(1.0f, AccentColor[0] + 0.1f),
                std::min(1.0f, AccentColor[1] + 0.1f),
                std::min(1.0f, AccentColor[2] + 0.1f),
                AccentColor[3]);
            ImVec4 accentActive(
                std::min(1.0f, AccentColor[0] + 0.2f),
                std::min(1.0f, AccentColor[1] + 0.2f),
                std::min(1.0f, AccentColor[2] + 0.2f),
                AccentColor[3]);

            style.Colors[ImGuiCol_Header] = accent;
            style.Colors[ImGuiCol_HeaderHovered] = accentHover;
            style.Colors[ImGuiCol_HeaderActive] = accentActive;
            style.Colors[ImGuiCol_CheckMark] = accent;
            style.Colors[ImGuiCol_SliderGrab] = accent;
            style.Colors[ImGuiCol_SliderGrabActive] = accentActive;
            style.Colors[ImGuiCol_Button] = accent;
            style.Colors[ImGuiCol_ButtonHovered] = accentHover;
            style.Colors[ImGuiCol_ButtonActive] = accentActive;
            style.Colors[ImGuiCol_FrameBg] = ImVec4(BgColor[0], BgColor[1], BgColor[2], 0.5f);
            style.Colors[ImGuiCol_WindowBg] = ImVec4(BgColor[0], BgColor[1], BgColor[2], BgColor[3]);
        }

        /// Draw the customizer UI (add to menu)
        static void Draw() {
            if (ImGui::CollapsingHeader("Menu Customizer")) {
                ImGui::Indent(10.0f);
                bool changed = false;

                changed |= ImGui::SliderFloat("Font Scale", &FontScale, 0.7f, 1.5f);
                changed |= ImGui::SliderFloat("Menu Alpha", &MenuAlpha, 0.3f, 1.0f);
                changed |= ImGui::SliderFloat("Item Spacing", &ItemSpacing, 0.0f, 12.0f);
                changed |= ImGui::SliderFloat("Rounding", &Rounding, 0.0f, 15.0f);
                changed |= ImGui::Checkbox("Lock Position", &LockPosition);
                changed |= ImGui::ColorEdit4("Accent Color", AccentColor, ImGuiColorEditFlags_AlphaBar);
                changed |= ImGui::ColorEdit4("Background", BgColor, ImGuiColorEditFlags_AlphaBar);

                if (ImGui::Button("Apply")) {
                    Apply();
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset")) {
                    FontScale = 1.0f;
                    MenuAlpha = 0.95f;
                    ItemSpacing = 4.0f;
                    Rounding = 5.0f;
                    LockPosition = false;
                    AccentColor[0] = 0.0f; AccentColor[1] = 0.47f;
                    AccentColor[2] = 0.84f; AccentColor[3] = 1.0f;
                    BgColor[0] = 0.08f; BgColor[1] = 0.08f;
                    BgColor[2] = 0.12f; BgColor[3] = 0.95f;
                    Apply();
                }

                if (changed) Apply();

                ImGui::Unindent(10.0f);
            }
        }
    };

    // ========================================================================
    // PermaShow — Always-visible keybind/toggle status overlay
    // ========================================================================
    // Usage:
    //   PermaShow::Add("Combo", "Combo", &comboKeybind->Active);
    //   PermaShow::Add("Evade", "Evade", &evadeEnabled->Enabled);
    //   PermaShow::Render(); // Call each frame
    //   PermaShow::Remove("Combo");
    // ========================================================================
    class PermaShow {
    public:
        struct Entry {
            std::string Name;
            std::string DisplayName;
            bool* ValuePtr;            // Points to MenuBool::Enabled or MenuKeyBind::Active
            ImU32 OnColor;
            ImU32 OffColor;
        };

        static void Add(const std::string& name, const std::string& display, bool* valuePtr,
                         ImU32 onColor = IM_COL32(0, 200, 100, 255),
                         ImU32 offColor = IM_COL32(200, 60, 60, 255)) {
            // Replace if already exists
            Remove(name);
            s_entries.push_back({ name, display, valuePtr, onColor, offColor });
        }

        static void Remove(const std::string& name) {
            s_entries.erase(
                std::remove_if(s_entries.begin(), s_entries.end(),
                    [&](const Entry& e) { return e.Name == name; }),
                s_entries.end());
        }

        static void Clear() { s_entries.clear(); }

        static void SetPosition(float x, float y) { s_posX = x; s_posY = y; }

        // Render the PermaShow overlay
        static void Render() {
            if (s_entries.empty()) return;

            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            if (!dl) return;

            float x = s_posX;
            float y = s_posY;
            float itemHeight = 22.0f;
            float padding = 6.0f;
            float nameWidth = 120.0f;
            float statusWidth = 40.0f;
            float totalWidth = nameWidth + statusWidth + padding * 3;
            float totalHeight = itemHeight * (float)s_entries.size() + padding * 2;

            // Background
            dl->AddRectFilled(
                ImVec2(x, y),
                ImVec2(x + totalWidth, y + totalHeight),
                IM_COL32(20, 20, 30, 200), 4.0f);

            // Border
            dl->AddRect(
                ImVec2(x, y),
                ImVec2(x + totalWidth, y + totalHeight),
                IM_COL32(80, 80, 120, 180), 4.0f);

            float cy = y + padding;
            for (auto& entry : s_entries) {
                bool isOn = entry.ValuePtr ? *entry.ValuePtr : false;

                // Name
                dl->AddText(ImVec2(x + padding, cy + 2.0f),
                           IM_COL32(220, 220, 220, 255),
                           entry.DisplayName.c_str());

                // Status indicator
                float sx = x + nameWidth + padding * 2;
                ImU32 statusColor = isOn ? entry.OnColor : entry.OffColor;
                dl->AddRectFilled(
                    ImVec2(sx, cy + 2.0f),
                    ImVec2(sx + statusWidth, cy + itemHeight - 2.0f),
                    statusColor, 3.0f);

                const char* statusText = isOn ? "ON" : "OFF";
                ImVec2 textSize = ImGui::CalcTextSize(statusText);
                dl->AddText(
                    ImVec2(sx + (statusWidth - textSize.x) * 0.5f,
                           cy + (itemHeight - textSize.y) * 0.5f),
                    IM_COL32(255, 255, 255, 255), statusText);

                cy += itemHeight;
            }
        }

    private:
        static inline std::vector<Entry> s_entries;
        static inline float s_posX = 10.0f;
        static inline float s_posY = 300.0f;
    };

    // ========================================================================
    // Notification — Timed popup notifications (EnsoulSharp.SDK compatible)
    // ========================================================================
    // Source: EnsoulSharp.SDK/Core/UI/Notifications/Notification.cs
    //
    // Usage:
    //   Notifications::Add("SpellReady", "Flash is Ready!", NotificationType::Info, 3.0f);
    //   Notifications::Add("Kill", "Enemy Killed!", NotificationType::Success, 2.5f);
    //   Notifications::Render(); // Call each frame
    // ========================================================================

    enum class NotificationType {
        Info,       // Blue
        Success,    // Green
        Warning,    // Yellow/Orange
        Error,      // Red
        Custom      // User-defined color
    };

    class Notification {
    public:
        std::string Id;
        std::string Header;
        std::string Body;
        NotificationType Type;
        ImU32 CustomColor;

        float Duration;         // Total display time (seconds)
        float StartTime;        // Game time when created
        float FadeInTime;       // Fade-in duration
        float FadeOutTime;      // Fade-out duration

        bool IsActive = true;

        Notification() : Type(NotificationType::Info), CustomColor(0), Duration(3.0f),
                         StartTime(0), FadeInTime(0.3f), FadeOutTime(0.5f) {}

        Notification(const std::string& id, const std::string& header,
                     const std::string& body = "", NotificationType type = NotificationType::Info,
                     float duration = 3.0f)
            : Id(id), Header(header), Body(body), Type(type), CustomColor(0),
              Duration(duration), StartTime(0), FadeInTime(0.3f), FadeOutTime(0.5f) {}

        // Get elapsed time
        float GetElapsed(float currentTime) const {
            return currentTime - StartTime;
        }

        // Get current alpha (0-1) based on fade in/out
        float GetAlpha(float currentTime) const {
            float elapsed = currentTime - StartTime;
            if (elapsed < 0) return 0.0f;

            // Fade in
            if (elapsed < FadeInTime) {
                return elapsed / FadeInTime;
            }

            // Fade out (at end of duration)
            float fadeOutStart = Duration - FadeOutTime;
            if (elapsed > fadeOutStart) {
                float fadeProgress = (elapsed - fadeOutStart) / FadeOutTime;
                return 1.0f - std::min(1.0f, fadeProgress);
            }

            return 1.0f;
        }

        // Is expired?
        bool IsExpired(float currentTime) const {
            return (currentTime - StartTime) >= Duration;
        }

        // Get header color based on type
        ImU32 GetHeaderColor(float alpha) const {
            int a = (int)(alpha * 255.0f);
            switch (Type) {
            case NotificationType::Info:    return IM_COL32(60, 140, 230, a);
            case NotificationType::Success: return IM_COL32(40, 200, 80, a);
            case NotificationType::Warning: return IM_COL32(230, 180, 40, a);
            case NotificationType::Error:   return IM_COL32(220, 50, 50, a);
            case NotificationType::Custom:  {
                // Extract RGBA, apply alpha
                int r = (CustomColor >> 0) & 0xFF;
                int g = (CustomColor >> 8) & 0xFF;
                int b = (CustomColor >> 16) & 0xFF;
                return IM_COL32(r, g, b, a);
            }
            default: return IM_COL32(150, 150, 150, a);
            }
        }

        // Get icon text based on type
        const char* GetIcon() const {
            switch (Type) {
            case NotificationType::Info:    return "[i]";
            case NotificationType::Success: return "[+]";
            case NotificationType::Warning: return "[!]";
            case NotificationType::Error:   return "[X]";
            default: return "[-]";
            }
        }
    };

    // ========================================================================
    // Notifications — Static manager for all on-screen notifications
    // ========================================================================
    class Notifications {
    public:
        // ====================================================================
        // Add a notification
        // ====================================================================
        static void Add(const std::string& id, const std::string& header,
                         NotificationType type = NotificationType::Info,
                         float duration = 3.0f) {
            Add(id, header, "", type, duration);
        }

        static void Add(const std::string& id, const std::string& header,
                         const std::string& body,
                         NotificationType type = NotificationType::Info,
                         float duration = 3.0f) {
            // Remove existing notification with same ID
            Remove(id);

            Notification notif(id, header, body, type, duration);
            notif.StartTime = GetTime();
            s_notifications.push_back(notif);

            // Limit max notifications
            while (s_notifications.size() > s_maxVisible) {
                s_notifications.erase(s_notifications.begin());
            }
        }

        // Add with custom color
        static void AddCustom(const std::string& id, const std::string& header,
                               const std::string& body, ImU32 color,
                               float duration = 3.0f) {
            Remove(id);
            Notification notif(id, header, body, NotificationType::Custom, duration);
            notif.CustomColor = color;
            notif.StartTime = GetTime();
            s_notifications.push_back(notif);
        }

        // ====================================================================
        // Remove by ID
        // ====================================================================
        static void Remove(const std::string& id) {
            s_notifications.erase(
                std::remove_if(s_notifications.begin(), s_notifications.end(),
                    [&](const Notification& n) { return n.Id == id; }),
                s_notifications.end());
        }

        // ====================================================================
        // Clear all
        // ====================================================================
        static void Clear() { s_notifications.clear(); }

        // ====================================================================
        // Configuration
        // ====================================================================
        static void SetPosition(float x, float y) { s_posX = x; s_posY = y; }
        static void SetMaxVisible(int max) { s_maxVisible = max; }
        static void SetEnabled(bool enabled) { s_enabled = enabled; }

        // ====================================================================
        // Render — Call each frame
        // ====================================================================
        static void Render() {
            if (!s_enabled || s_notifications.empty()) return;

            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            if (!dl) return;

            float currentTime = GetTime();

            // Remove expired notifications
            s_notifications.erase(
                std::remove_if(s_notifications.begin(), s_notifications.end(),
                    [currentTime](const Notification& n) { return n.IsExpired(currentTime); }),
                s_notifications.end());

            // Render from top-right (newest at bottom)
            float x = s_posX;
            if (x < 0) {
                ImVec2 displaySize = ImGui::GetIO().DisplaySize;
                x = displaySize.x - s_width - 10.0f;
            }
            float y = s_posY;

            // Calculate total height to position from top-right
            float itemSpacing = 4.0f;

            for (size_t i = 0; i < s_notifications.size(); i++) {
                auto& notif = s_notifications[i];
                float alpha = notif.GetAlpha(currentTime);
                if (alpha <= 0.01f) continue;

                // Calculate dimensions
                float width = s_width;
                float headerHeight = 24.0f;
                float bodyHeight = notif.Body.empty() ? 0.0f : 20.0f;
                float totalHeight = headerHeight + bodyHeight + 8.0f;
                float padding = 8.0f;

                int bgAlpha = (int)(alpha * 200.0f);
                int textAlpha = (int)(alpha * 255.0f);

                // Slide-in from right
                float slideOffset = 0.0f;
                float elapsed = notif.GetElapsed(currentTime);
                if (elapsed < notif.FadeInTime) {
                    slideOffset = (1.0f - elapsed / notif.FadeInTime) * width;
                }

                float drawX = x + slideOffset;

                // Background
                dl->AddRectFilled(
                    ImVec2(drawX, y),
                    ImVec2(drawX + width, y + totalHeight),
                    IM_COL32(25, 25, 35, bgAlpha), 5.0f);

                // Left accent bar (colored by type)
                dl->AddRectFilled(
                    ImVec2(drawX, y),
                    ImVec2(drawX + 4.0f, y + totalHeight),
                    notif.GetHeaderColor(alpha), 5.0f);

                // Border
                dl->AddRect(
                    ImVec2(drawX, y),
                    ImVec2(drawX + width, y + totalHeight),
                    IM_COL32(60, 60, 80, bgAlpha), 5.0f);

                // Icon + Header
                float textY = y + 4.0f;
                char headerBuf[512];
                snprintf(headerBuf, sizeof(headerBuf), "%s %s",
                         notif.GetIcon(), notif.Header.c_str());
                dl->AddText(ImVec2(drawX + padding + 2.0f, textY),
                           IM_COL32(240, 240, 240, textAlpha), headerBuf);

                // Body text (if present)
                if (!notif.Body.empty()) {
                    dl->AddText(ImVec2(drawX + padding + 2.0f, textY + headerHeight - 2.0f),
                               IM_COL32(180, 180, 200, textAlpha), notif.Body.c_str());
                }

                // Progress bar at bottom
                float progress = elapsed / notif.Duration;
                float barWidth = width * (1.0f - std::min(1.0f, progress));
                if (barWidth > 0) {
                    dl->AddRectFilled(
                        ImVec2(drawX, y + totalHeight - 2.0f),
                        ImVec2(drawX + barWidth, y + totalHeight),
                        notif.GetHeaderColor(alpha * 0.7f));
                }

                y += totalHeight + itemSpacing;
            }
        }

        // ====================================================================
        // Count active notifications
        // ====================================================================
        static int Count() { return (int)s_notifications.size(); }

    private:
        static float GetTime() {
            // Use system clock as fallback if Game::GetTime() is unavailable
            static auto start = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration<float>(now - start).count();
        }

        static inline std::vector<Notification> s_notifications;
        static inline float s_posX = -1.0f;       // -1 = auto (top-right)
        static inline float s_posY = 50.0f;
        static inline float s_width = 280.0f;
        static inline int s_maxVisible = 8;
        static inline bool s_enabled = true;
    };

} // namespace MenuUI
} // namespace SDK
