#pragma once
#include "../imgui/imgui.h"
#include "MenuTheme.h"
#include "Translations.h"
#include <Windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <new>

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
//   // Short helpers (recommended for frequent access):
//   int rangeFast = menu->GetSliderValue("Range", 900);
//   bool enabledFast = menu->GetBoolValue("Enable", true);
// ============================================================================

namespace SDK {
namespace MenuUI {

    template<typename T, int Capacity>
    class FixedList {
    public:
        T* begin() { return m_items; }
        T* end() { return m_items + m_count; }
        const T* begin() const { return m_items; }
        const T* end() const { return m_items + m_count; }
        const T* cbegin() const { return m_items; }
        const T* cend() const { return m_items + m_count; }

        bool push_back(const T& value) {
            if (m_count >= Capacity) {
                return false;
            }
            m_items[m_count++] = value;
            return true;
        }

        void clear() {
            for (int i = 0; i < m_count; ++i) {
                m_items[i] = T{};
            }
            m_count = 0;
        }

        size_t size() const { return (size_t)m_count; }
        bool empty() const { return m_count == 0; }

        T& operator[](size_t index) { return m_items[index]; }
        const T& operator[](size_t index) const { return m_items[index]; }

    private:
        T m_items[Capacity]{};
        int m_count = 0;
    };

    inline void MenuDebugLog(const char* msg) {
        OutputDebugStringA(msg);
        HANDLE hFile = CreateFileA("C:\\Users\\Public\\nightsharp_orb_crash.txt",
            FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hFile, msg, (DWORD)lstrlenA(msg), &written, nullptr);
            WriteFile(hFile, "\r\n", 2, &written, nullptr);
            CloseHandle(hFile);
        }
    }

    inline void MenuStageTrace(const char* msg) {
        HANDLE hFile = CreateFileA("C:\\Users\\Public\\ns_stage.txt",
            FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hFile, msg, (DWORD)lstrlenA(msg), &written, nullptr);
            CloseHandle(hFile);
        }
    }

    inline bool IsOrbwalkerTraceItem(const std::string& name) {
        return name == "drawAARange" ||
               name == "drawAARangeEnemy" ||
               name == "drawExtraHoldPosition" ||
               name == "drawKillableMinion" ||
               name == "drawKillableMinionFade" ||
               name == "enabledOption" ||
               name == "comboKey" ||
               name == "hybridKey" ||
               name == "laneclearKey" ||
               name == "lasthitKey" ||
               name == "fleeKey";
    }

    // ========================================================================
    // KeyBind types
    // ========================================================================
    enum class KeyBindType {
        Press,   // Active while held
        Toggle,  // Toggles on/off on press
        Hold     // Legacy alias, treated same as Press
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

        static void* operator new(size_t sz) noexcept {
            return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz);
        }

        static void operator delete(void* ptr) noexcept {
            if (ptr) {
                HeapFree(GetProcessHeap(), 0, ptr);
            }
        }

        virtual ~MenuItem() = default;
        virtual void Draw() {}
        virtual float EstimateMinWidth() const { return ImGui::CalcTextSize(Translations::T(DisplayName.c_str())).x + 100.0f; }

        MenuItem(const std::string& name, const std::string& display)
            : InternalName(name), DisplayName(display) {}

        template<typename T>
        T* GetValue() {
            return dynamic_cast<T*>(this);
        }

        template<typename T>
        const T* GetValue() const {
            return dynamic_cast<const T*>(this);
        }

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
        const char* GetDisplayText() const {
            return Translations::T(DisplayName.c_str());
        }

        void DrawTooltip() const {
            if (!Tooltip.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", Tooltip.c_str());
            }
        }

        void NotifyValueChanged() { FireValueChanged(); }

    protected:
        FixedList<OnValueChangedFn, 8> m_valueChangedCallbacks;

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
        bool& Value;

        MenuBool(const std::string& name, const std::string& display, bool defaultValue = true)
            : MenuItem(name, display), Enabled(defaultValue), Value(Enabled), m_prev(defaultValue), m_default(defaultValue) {}

        void Toggle() {
            Enabled = !Enabled;
        }

        void ResetDefault() {
            Enabled = m_default;
        }

        void CheckChanged() {
            if (Enabled != m_prev) {
                if (IsOrbwalkerTraceItem(InternalName)) {
                    char buf[192] = {};
                    wsprintfA(buf, "[NightSharp] MenuBool change begin name=%s value=%d\r\n",
                        InternalName.c_str(), Enabled ? 1 : 0);
                    MenuStageTrace(buf);
                }
                m_prev = Enabled;
                FireValueChanged();
                if (IsOrbwalkerTraceItem(InternalName)) {
                    char buf[192] = {};
                    wsprintfA(buf, "[NightSharp] MenuBool change end name=%s value=%d\r\n",
                        InternalName.c_str(), Enabled ? 1 : 0);
                    MenuStageTrace(buf);
                }
            }
        }

        float EstimateMinWidth() const override {
            float labelW = ImGui::CalcTextSize(Translations::T(DisplayName.c_str())).x;
            float pad2 = ImGui::GetStyle().FramePadding.x * 2.0f;
            float onW = ImGui::CalcTextSize(Translations::T("On")).x + pad2;
            float offW = ImGui::CalcTextSize(Translations::T("Off")).x + pad2;
            float btnW = (onW > offW ? onW : offW);
            return labelW + 20.0f + btnW;
        }
    private:
        bool m_prev;
        bool m_default;
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
            : MenuItem(name, display), Value(defaultValue), MinValue(minVal), MaxValue(maxVal), m_prev(defaultValue), m_default(defaultValue) {}

        int GetValue() const { return Value; }
        void SetValue(int value) { Value = std::clamp(value, MinValue, MaxValue); }
        void ResetDefault() { Value = m_default; }

        float EstimateMinWidth() const override {
            float labelW = ImGui::CalcTextSize(Translations::T(DisplayName.c_str())).x;
            char valBuf[32];
            snprintf(valBuf, sizeof(valBuf), "%d", MaxValue);
            float valW = ImGui::CalcTextSize(valBuf).x;
            return labelW + 20.0f + valW + 100.0f;
        }
    private:
        int m_prev;
        int m_default;
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
            : MenuItem(name, display), Value(defaultValue), MinValue(minVal), MaxValue(maxVal), m_prev(defaultValue), m_default(defaultValue) {}

        void ResetDefault() { Value = m_default; }

        float EstimateMinWidth() const override {
            float labelW = ImGui::CalcTextSize(Translations::T(DisplayName.c_str())).x;
            char valBuf[32];
            snprintf(valBuf, sizeof(valBuf), "%.1f", (double)MaxValue);
            float valW = ImGui::CalcTextSize(valBuf).x;
            return labelW + 20.0f + valW + 100.0f;
        }
    private:
        float m_prev;
        float m_default;
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
            : MenuItem(name, display), Items(items), Index(defaultIndex), m_prev(defaultIndex), m_default(defaultIndex) {}

        void SetIndex(int index) {
            if (Items.empty()) {
                Index = 0;
                return;
            }
            Index = std::clamp(index, 0, (int)Items.size() - 1);
        }

        const char* GetSelectedString() const {
            if (Items.empty()) {
                return "";
            }
            int safeIndex = std::clamp(Index, 0, (int)Items.size() - 1);
            return Items[safeIndex].c_str();
        }

        float EstimateMinWidth() const override {
            float labelW = ImGui::CalcTextSize(Translations::T(DisplayName.c_str())).x;
            float maxItemW = 0.0f;
            for (auto& item : Items) {
                float w = ImGui::CalcTextSize(item.c_str()).x;
                if (w > maxItemW) maxItemW = w;
            }
            float dropW = maxItemW + 12.0f;
            return labelW + 20.0f + dropW + 8.0f;
        }
    private:
        int m_prev;
        int m_default;
    };

    // ========================================================================
    // MenuColor
    // ========================================================================
    class MenuColor : public MenuItem {
    public:
        float Color[4];

        MenuColor(const std::string& name, const std::string& display,
                  float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f)
            : MenuItem(name, display) {
            Color[0] = r; Color[1] = g; Color[2] = b; Color[3] = a;
            m_default[0] = r; m_default[1] = g; m_default[2] = b; m_default[3] = a;
        }

        void ResetDefault() {
            Color[0] = m_default[0]; Color[1] = m_default[1];
            Color[2] = m_default[2]; Color[3] = m_default[3];
            NotifyValueChanged();
        }

        ImU32 GetImU32() const {
            return IM_COL32((int)(Color[0]*255), (int)(Color[1]*255),
                            (int)(Color[2]*255), (int)(Color[3]*255));
        }

    private:
        float m_default[4];
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
            : MenuItem(name, display), Key(key), Type(type), Active(defaultActive),
              m_prevKey(key), m_prevType(type), m_prevActive(defaultActive) {}

        void Update() {
            const bool prevActive = Active;
            if (!IsCurrentProcessForeground()) {
                if (Type == KeyBindType::Press) {
                    Active = false;
                }
                m_wasDown = false;
                goto finalize_trace;
            }

            if (Key <= 0) {
                if (Type == KeyBindType::Press) {
                    Active = false;
                }
                goto finalize_trace;
            }

            if (Type == KeyBindType::Press || Type == KeyBindType::Hold) {
                Active = (GetAsyncKeyState(Key) & 0x8000) != 0;
            } else { // Toggle
                bool isDown = (GetAsyncKeyState(Key) & 0x8000) != 0;
                if (isDown && !m_wasDown) {
                    Active = !Active;
                }
                m_wasDown = isDown;
            }

        finalize_trace:
            if (prevActive != Active && IsOrbwalkerTraceItem(InternalName)) {
                char buf[192] = {};
                wsprintfA(buf, "[NightSharp] MenuKeyBind active name=%s active=%d key=%d type=%d\r\n",
                    InternalName.c_str(), Active ? 1 : 0, Key, (int)Type);
                MenuStageTrace(buf);
            }
        }

        float EstimateMinWidth() const override {
            float labelW = ImGui::CalcTextSize(Translations::T(DisplayName.c_str())).x;
            float pad2 = ImGui::GetStyle().FramePadding.x * 2.0f;
            float modeTextW = ImGui::CalcTextSize(Translations::T("Press")).x;
            float toggleTextW = ImGui::CalcTextSize(Translations::T("Toggle")).x;
            if (toggleTextW > modeTextW) modeTextW = toggleTextW;
            float arrowW = 18.0f;
            float modeTotalW = arrowW + 4.0f + modeTextW + pad2 + 4.0f + arrowW;
            float pressKeyW = ImGui::CalcTextSize(Translations::T("Press key...")).x + pad2;
            float keyNameW = ImGui::CalcTextSize(GetKeyName(Key)).x + pad2;
            float keyBtnW = (pressKeyW > keyNameW ? pressKeyW : keyNameW);
            return labelW + 20.0f + modeTotalW + 6.0f + keyBtnW + 8.0f;
        }

    public:
        static bool IsCurrentProcessForeground() {
            HWND fg = GetForegroundWindow();
            if (!fg) {
                return false;
            }
            DWORD pid = 0;
            GetWindowThreadProcessId(fg, &pid);
            return pid == GetCurrentProcessId();
        }

        bool IsListening() const { return m_listening; }

        void StartListening() {
            m_listening = true;
            m_listenDebounceFrames = 4;
        }

        void PollListeningKey() {
            if (!m_listening) return;
            if (m_listenDebounceFrames > 0) { --m_listenDebounceFrames; return; }
            for (int vk = 1; vk < 256; ++vk) {
                if ((GetAsyncKeyState(vk) & 0x8000) == 0) continue;
                if (vk == VK_ESCAPE) { m_listening = false; return; }
                Key = vk; m_listening = false; m_wasDown = true; return;
            }
        }

        static const char* GetKeyName(int vk) {
            static char nameBuf[64];
            switch (vk) {
            case 0: return "None";
            case VK_SPACE: return "Space";
            case VK_LBUTTON: return "Mouse1";
            case VK_RBUTTON: return "Mouse2";
            case VK_MBUTTON: return "MMB";
            case VK_LSHIFT: return "LShift";
            case VK_RSHIFT: return "RShift";
            case VK_LCONTROL: return "LCtrl";
            case VK_RCONTROL: return "RCtrl";
            case VK_LMENU: return "LAlt";
            case VK_RMENU: return "RAlt";
            case VK_CAPITAL: return "CapsLock";
            case VK_TAB: return "Tab";
            case VK_RETURN: return "Enter";
            case VK_ESCAPE: return "Esc";
            case VK_BACK: return "Backspace";
            case VK_DELETE: return "Delete";
            case VK_INSERT: return "Insert";
            case VK_HOME: return "Home";
            case VK_END: return "End";
            case VK_PRIOR: return "PageUp";
            case VK_NEXT: return "PageDown";
            case VK_LEFT: return "Left";
            case VK_RIGHT: return "Right";
            case VK_UP: return "Up";
            case VK_DOWN: return "Down";
            case VK_OEM_3: return "`";
            default:
                if (vk >= '0' && vk <= '9') { nameBuf[0] = (char)vk; nameBuf[1] = 0; return nameBuf; }
                if (vk >= 'A' && vk <= 'Z') { nameBuf[0] = (char)vk; nameBuf[1] = 0; return nameBuf; }
                if (vk >= VK_F1 && vk <= VK_F12) { snprintf(nameBuf, sizeof(nameBuf), "F%d", (vk - VK_F1) + 1); return nameBuf; }
                UINT scanCode = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
                if (scanCode != 0) {
                    LONG lParam = (LONG)(scanCode << 16);
                    if (IsExtendedKey(vk)) lParam |= 1 << 24;
                    int len = GetKeyNameTextA(lParam, nameBuf, (int)sizeof(nameBuf));
                    if (len > 0) return nameBuf;
                }
                return "?";
            }
        }

        void CheckConfigChanged() {
            bool configChanged = (Key != m_prevKey || Type != m_prevType);
            bool toggleStateChanged = (Type == KeyBindType::Toggle && Active != m_prevActive);
            if (configChanged || toggleStateChanged) {
                m_prevKey = Key;
                m_prevType = Type;
                m_prevActive = Active;
                FireValueChanged();
            } else if (Type == KeyBindType::Press) {
                m_prevActive = Active;
            }
        }

    private:
        bool m_wasDown = false;
        bool m_listening = false;
        int m_listenDebounceFrames = 0;
        int m_prevKey = 0;
        KeyBindType m_prevType = KeyBindType::Press;
        bool m_prevActive = false;

        static bool IsExtendedKey(int vk) {
            switch (vk) {
            case VK_INSERT:
            case VK_DELETE:
            case VK_HOME:
            case VK_END:
            case VK_PRIOR:
            case VK_NEXT:
            case VK_LEFT:
            case VK_RIGHT:
            case VK_UP:
            case VK_DOWN:
            case VK_DIVIDE:
            case VK_RCONTROL:
            case VK_RMENU:
            case VK_NUMLOCK:
                return true;
            default:
                return false;
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
    };

    // ========================================================================
    // Menu (container — can hold items + sub-menus)
    // ========================================================================
    class Menu : public MenuItem {
    public:
        using OnChildAddedFn = void(*)(Menu* root, MenuItem* newChild);
        struct MenuStoreView {
            Menu** Items;
            int* Count;
            int Capacity;

            Menu** begin() { return Items; }
            Menu** end() { return Items + *Count; }
            Menu* const* begin() const { return Items; }
            Menu* const* end() const { return Items + *Count; }
            size_t size() const { return (size_t)(*Count); }

            Menu* operator[](int index) const {
                return (index >= 0 && index < *Count) ? Items[index] : nullptr;
            }

            bool Push(Menu* menu) {
                if (!menu || *Count >= Capacity) {
                    return false;
                }
                Items[*Count] = menu;
                ++(*Count);
                return true;
            }

            void Clear() {
                for (int i = 0; i < *Count; ++i) {
                    Items[i] = nullptr;
                }
                *Count = 0;
            }

            void RemoveByName(const std::string& name) {
                int write = 0;
                for (int read = 0; read < *Count; ++read) {
                    Menu* menu = Items[read];
                    if (menu && menu->InternalName == name) {
                        continue;
                    }
                    Items[write++] = menu;
                }
                for (int i = write; i < *Count; ++i) {
                    Items[i] = nullptr;
                }
                *Count = write;
            }
        };

        static constexpr int kMaxGlobalMenus = 128;
        static constexpr int kMaxMenuItems = 256;
        static inline Menu* s_globalMenus[kMaxGlobalMenus] = {};
        static inline int s_globalMenuCount = 0;

        Menu(const std::string& name, const std::string& display, bool isRoot = false)
            : MenuItem(name, display), m_isRoot(isRoot) {}

        // Add a menu item by type
        template<typename T, typename... Args>
        T* Add(Args&&... args) {
            void* raw = MenuItem::operator new(sizeof(T));
            if (!raw) {
                return nullptr;
            }
            auto* item = ::new(raw) T(std::forward<Args>(args)...);
            return static_cast<T*>(AttachItem(item));
        }

        // Add a sub-menu
        Menu* AddSubMenu(const std::string& name, const std::string& display) {
            MenuDebugLog("MenuUI::Menu::AddSubMenu raw alloc begin");
            void* raw = MenuItem::operator new(sizeof(Menu));
            if (!raw) {
                MenuDebugLog("MenuUI::Menu::AddSubMenu raw alloc failed");
                return nullptr;
            }
            MenuDebugLog("MenuUI::Menu::AddSubMenu raw alloc OK");
            auto* sub = ::new(raw) Menu(name, display, false);
            MenuDebugLog("MenuUI::Menu::AddSubMenu ctor OK");
            AttachItem(sub);
            return sub;
        }

        void Add(MenuItem* item) {
            if (!item) return;
            AttachItem(item);
        }

        // Get item by internal name
        template<typename T>
        T* Get(const std::string& name) {
            for (auto* item : m_items) {
                if (item && item->InternalName == name) {
                    return dynamic_cast<T*>(item);
                }
            }
            return nullptr;
        }

        template<typename T>
        const T* Get(const std::string& name) const {
            for (auto* item : m_items) {
                if (item && item->InternalName == name) {
                    return dynamic_cast<const T*>(item);
                }
            }
            return nullptr;
        }

        // Get sub-menu
        Menu* GetSubMenu(const std::string& name) {
            return Get<Menu>(name);
        }

        // Check if item exists
        MenuItem* operator[](const std::string& name) {
            for (auto* item : m_items) {
                if (item && item->InternalName == name) {
                    return item;
                }
            }
            return nullptr;
        }

        const MenuItem* operator[](const std::string& name) const {
            for (auto* item : m_items) {
                if (item && item->InternalName == name) {
                    return item;
                }
            }
            return nullptr;
        }

        // Value helpers for script-style access (safe fallback on missing/invalid type)
        int GetSliderValue(const std::string& name, int fallback = 0) const {
            const auto* slider = Get<MenuSlider>(name);
            return slider ? slider->Value : fallback;
        }

        float GetSliderFValue(const std::string& name, float fallback = 0.0f) const {
            const auto* slider = Get<MenuSliderF>(name);
            return slider ? slider->Value : fallback;
        }

        bool GetBoolValue(const std::string& name, bool fallback = false) const {
            const auto* item = Get<MenuBool>(name);
            return item ? item->Enabled : fallback;
        }

        bool GetKeyBindValue(const std::string& name, bool fallback = false) const {
            const auto* item = Get<MenuKeyBind>(name);
            return item ? item->Active : fallback;
        }

        int GetListIndex(const std::string& name, int fallback = 0) const {
            const auto* item = Get<MenuList>(name);
            return item ? item->Index : fallback;
        }

        // Update all keybinds recursively
        void UpdateKeyBinds() {
            for (auto& item : m_items) {
                auto* kb = dynamic_cast<MenuKeyBind*>(item);
                if (kb) kb->Update();
                auto* sub = dynamic_cast<Menu*>(item);
                if (sub) sub->UpdateKeyBinds();
            }
        }

        const FixedList<MenuItem*, kMaxMenuItems>& GetItems() const { return m_items; }

        Menu* FindSection(const std::string& sectionKey) {
            for (auto& item : m_items) {
                auto* sub = dynamic_cast<Menu*>(item);
                if (sub && sub->InternalName == sectionKey) return sub;
            }
            return nullptr;
        }

        void GetStandaloneItems(std::vector<MenuItem*>& out) const {
            out.clear();
            for (auto& item : m_items) {
                if (!dynamic_cast<Menu*>(item)) out.push_back(item);
            }
        }

        std::vector<std::pair<std::string, std::string>> GetRootSections() const {
            std::vector<std::pair<std::string, std::string>> sections;
            if (!m_isRoot) {
                return sections;
            }

            std::vector<MenuItem*> standaloneItems;
            std::vector<Menu*> subMenus;
            const bool onlyKeyBinds = SplitRootItems(standaloneItems, subMenus);

            if (!standaloneItems.empty()) {
                sections.push_back({
                    "__root_items",
                    onlyKeyBinds ? std::string(Translations::T("Keys")) : std::string(Translations::T("General"))
                });
            }

            for (auto* sub : subMenus) {
                sections.push_back({ sub->InternalName, std::string(Translations::T(sub->DisplayName.c_str())) });
            }

            return sections;
        }

        int EstimateRowCount() const {
            int rows = 0;
            for (const auto& item : m_items) {
                if (auto* sub = dynamic_cast<Menu*>(item)) {
                    rows += 1 + sub->EstimateRowCount();
                } else {
                    rows += 1;
                }
            }
            return std::max(rows, 1);
        }

        int EstimateRootSectionRowCount(const std::string& sectionKey) const {
            if (!m_isRoot) {
                return EstimateRowCount() + 1;
            }

            std::vector<MenuItem*> standaloneItems;
            std::vector<Menu*> subMenus;
            const bool onlyKeyBinds = SplitRootItems(standaloneItems, subMenus);
            (void)onlyKeyBinds;

            if (sectionKey == "__root_items") {
                return std::max((int)standaloneItems.size(), 1);
            }

            for (auto* sub : subMenus) {
                if (sub->InternalName == sectionKey) {
                    return std::max(sub->EstimateRowCount(), 1);
                }
            }

            if (!standaloneItems.empty()) {
                return std::max(2 + (int)standaloneItems.size(), 2);
            }

            return 2;
        }

        float EstimateMinWidth() const override {
            float titleW = ImGui::CalcTextSize(Translations::T(DisplayName.c_str())).x;
            float maxW = titleW;
            for (const auto& item : m_items) {
                if (!item) continue;
                float w = item->EstimateMinWidth() + 10.0f;
                if (w > maxW) maxW = w;
            }
            return maxW;
        }

        float EstimateRootSectionWidth(const std::string& sectionKey) const {
            if (!m_isRoot) return EstimateMinWidth();

            std::vector<MenuItem*> standaloneItems;
            std::vector<Menu*> subMenus;
            SplitRootItems(standaloneItems, subMenus);

            if (sectionKey == "__root_items") {
                float maxW = 0.0f;
                for (auto* item : standaloneItems) {
                    if (!item) continue;
                    float w = item->EstimateMinWidth();
                    if (w > maxW) maxW = w;
                }
                return maxW;
            }

            for (auto* sub : subMenus) {
                if (sub->InternalName == sectionKey) {
                    return sub->EstimateMinWidth();
                }
            }

            return 200.0f;
        }

        // Factory: create a root menu
        static Menu* Create(const std::string& name, const std::string& display) {
            MenuDebugLog("MenuUI::Menu::Create begin");
            MenuDebugLog("MenuUI::Menu::Create raw alloc begin");
            void* raw = MenuItem::operator new(sizeof(Menu));
            if (!raw) {
                MenuDebugLog("MenuUI::Menu::Create raw alloc failed");
                return nullptr;
            }
            MenuDebugLog("MenuUI::Menu::Create raw alloc OK");
            auto* menu = ::new(raw) Menu(name, display, true);
            MenuDebugLog("MenuUI::Menu::Create ctor OK");
            auto menus = GetGlobalMenus();
            MenuDebugLog("MenuUI::Menu::Create global store resolved");
            menus.Push(menu);
            MenuDebugLog("MenuUI::Menu::Create push_back OK");
            return menu;
        }

        // Attach — register to global list (E# compatible: Menu(...).Attach())
        Menu* Attach() {
            auto menus = GetGlobalMenus();
            menus.Push(this);
            return this;
        }

        // Remove from global menus
        static void Remove(const std::string& name) {
            auto menus = GetGlobalMenus();
            menus.RemoveByName(name);
        }

        // Dispose — remove this menu from global list and clear items (EnsoulSharp: Menu.Dispose)
        void Dispose() {
            Remove(InternalName);
            for (auto* item : m_items) {
                delete item;
            }
            m_items.clear();
            Components.clear();
            ComponentCount = 0;
        }

        // Convenience: Item<T>("name") — alias for Get<T>("name")
        template<typename T>
        T* Item(const std::string& name) {
            return Get<T>(name);
        }

        template<typename T>
        T* GetValue(const std::string& name) {
            return Get<T>(name);
        }

        template<typename T>
        const T* GetValue(const std::string& name) const {
            return Get<T>(name);
        }

        // Add a RadioMenu (radio-button group) — defined after RadioMenu class
        inline RadioMenu* AddRadioMenu(const std::string& name, const std::string& display);

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
        static MenuStoreView GetGlobalMenus() {
            return MenuStoreView{ s_globalMenus, &s_globalMenuCount, kMaxGlobalMenus };
        }

        // Update all global keybinds
        static void UpdateAllKeyBinds() {
            for (auto& menu : GetGlobalMenus()) {
                if (menu) menu->UpdateKeyBinds();
            }
        }

        bool GetVisible() const { return Visible; }
        void SetVisible(bool visible) { Visible = visible; }
        bool GetToggled() const { return Toggled; }
        void SetToggled(bool toggled) { Toggled = toggled; }

        OnChildAddedFn OnChildAdded = nullptr;
        bool Visible = true;
        bool Toggled = true;
        FixedList<MenuItem*, kMaxMenuItems> Components;
        int ComponentCount = 0;

    private:
        MenuItem* AttachItem(MenuItem* item) {
            if (!item) {
                return nullptr;
            }
            MenuItem* ptr = item;
            MenuDebugLog("MenuUI::Menu::AttachItem push m_items begin");
            if (!m_items.push_back(item)) {
                MenuDebugLog("MenuUI::Menu::AttachItem push m_items failed");
                return nullptr;
            }
            MenuDebugLog("MenuUI::Menu::AttachItem push m_items OK");
            if (!Components.push_back(ptr)) {
                MenuDebugLog("MenuUI::Menu::AttachItem push Components failed");
                return nullptr;
            }
            MenuDebugLog("MenuUI::Menu::AttachItem push Components OK");
            ComponentCount = (int)Components.size();

            MenuDebugLog("MenuUI::Menu::AttachItem callback hookup begin");
            if (auto* sub = dynamic_cast<Menu*>(ptr)) {
                sub->OnMenuValueChanged([this](const MenuValueChangedEventArgs& args) {
                    this->FireMenuValueChanged(args.ChangedItem);
                });
            } else {
                ptr->OnValueChanged([this](const MenuValueChangedEventArgs& args) {
                    this->FireMenuValueChanged(args.ChangedItem);
                });
            }
            MenuDebugLog("MenuUI::Menu::AttachItem callback hookup OK");

            if (OnChildAdded) {
                MenuDebugLog("MenuUI::Menu::AttachItem OnChildAdded begin");
                OnChildAdded(this, ptr);
                MenuDebugLog("MenuUI::Menu::AttachItem OnChildAdded OK");
            }

            return ptr;
        }

        bool SplitRootItems(std::vector<MenuItem*>& standaloneItems,
                            std::vector<Menu*>& subMenus) const {
            standaloneItems.clear();
            subMenus.clear();
            standaloneItems.reserve(m_items.size());
            subMenus.reserve(m_items.size());

            for (auto& item : m_items) {
                if (auto* sub = dynamic_cast<Menu*>(item)) {
                    subMenus.push_back(sub);
                } else {
                    standaloneItems.push_back(item);
                }
            }

            bool onlyKeyBinds = !standaloneItems.empty();
            for (auto& item : standaloneItems) {
                if (!dynamic_cast<MenuKeyBind*>(item)) {
                    onlyKeyBinds = false;
                    break;
                }
            }

            return onlyKeyBinds;
        }

        bool m_isRoot;
        FixedList<MenuItem*, kMaxMenuItems> m_items;
        FixedList<OnValueChangedFn, 32> m_menuValueChangedCallbacks;
        std::string m_activeRootSection;
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
            ImGui::Text("%s", GetDisplayText());
            ImGui::SameLine();
            if (ImGui::Button(ButtonText.c_str())) {
                if (OnClick) OnClick();
            }
        }

        void Click() {
            if (OnClick) OnClick();
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
        bool& BoolValue;

        /// Value property: returns SValue if BValue && SValue != MinValue, else -1
        int Value() const {
            return (SValue != MinValue && BValue) ? SValue : -1;
        }

        MenuSliderButton(const std::string& name, const std::string& display,
                         int value = 0, int minVal = 0, int maxVal = 100,
                         bool bValue = false)
            : MenuItem(name, display), SValue(value), MinValue(minVal), MaxValue(maxVal),
              BValue(bValue), BoolValue(BValue), m_prevS(value), m_prevB(bValue) {
            // Clamp initial value
            if (SValue < MinValue) SValue = MinValue;
            if (SValue > MaxValue) SValue = MaxValue;
        }

        void Draw() override {
            // Draw slider and button on same line
            ImGui::PushID(InternalName.c_str());

            // Slider portion (takes most width)
            float avail = ImGui::GetContentRegionAvail().x;
            float pad2 = ImGui::GetStyle().FramePadding.x * 2.0f;
            float onW = ImGui::CalcTextSize("ON").x + pad2;
            float offW = ImGui::CalcTextSize("OFF").x + pad2;
            float btnWidth = (onW > offW ? onW : offW);
            ImGui::SetNextItemWidth(avail - btnWidth - 10.0f);
            ImGui::SliderInt("##slider", &SValue, MinValue, MaxValue);

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
            ImGui::Text("%s", GetDisplayText());

            // Fire change event
            if (SValue != m_prevS || BValue != m_prevB) {
                m_prevS = SValue;
                m_prevB = BValue;
                FireValueChanged();
            }
            DrawTooltip();
        }

        int GetValue() const { return SValue; }
        void SetValue(int value) { SValue = std::clamp(value, MinValue, MaxValue); }

        float EstimateMinWidth() const override {
            float labelW = ImGui::CalcTextSize(Translations::T(DisplayName.c_str())).x;
            float pad2 = ImGui::GetStyle().FramePadding.x * 2.0f;
            float onW = ImGui::CalcTextSize("ON").x + pad2;
            float offW = ImGui::CalcTextSize("OFF").x + pad2;
            float btnW = (onW > offW ? onW : offW);
            char valBuf[32];
            snprintf(valBuf, sizeof(valBuf), "%d", MaxValue);
            float valW = ImGui::CalcTextSize(valBuf).x;
            return labelW + 20.0f + valW + 100.0f + 10.0f + btnW;
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
            if (ImGui::CollapsingHeader(GetDisplayText())) {
                ImGui::Indent(10.0f);
                for (int i = 0; i < (int)m_options.size(); i++) {
                    bool active = m_options[i]->Enabled;
                    if (ImGui::RadioButton(m_options[i]->GetDisplayText(), active)) {
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
    inline RadioMenu* Menu::AddRadioMenu(const std::string& name, const std::string& display) {
        auto* radioPtr = new RadioMenu(name, display);
        m_items.push_back(radioPtr);
        radioPtr->OnValueChanged([this, radioPtr](const MenuValueChangedEventArgs& args) {
            this->FireMenuValueChanged(args.ChangedItem ? args.ChangedItem : radioPtr);
        });
        return radioPtr;
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
            if (ImGui::CollapsingHeader(Translations::T("Menu Customizer"))) {
                ImGui::Indent(10.0f);
                bool changed = false;

                changed |= ImGui::SliderFloat(Translations::T("Font Scale"), &FontScale, 0.7f, 1.5f);
                changed |= ImGui::SliderFloat(Translations::T("Menu Alpha"), &MenuAlpha, 0.3f, 1.0f);
                changed |= ImGui::SliderFloat(Translations::T("Item Spacing"), &ItemSpacing, 0.0f, 12.0f);
                changed |= ImGui::SliderFloat(Translations::T("Rounding"), &Rounding, 0.0f, 15.0f);
                changed |= ImGui::Checkbox(Translations::T("Lock Position"), &LockPosition);
                changed |= ImGui::ColorEdit4(Translations::T("Accent Color"), AccentColor, ImGuiColorEditFlags_AlphaBar);
                changed |= ImGui::ColorEdit4(Translations::T("Background"), BgColor, ImGuiColorEditFlags_AlphaBar);

                if (ImGui::Button(Translations::T("Apply"))) {
                    Apply();
                }
                ImGui::SameLine();
                if (ImGui::Button(Translations::T("Reset"))) {
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

            float nameWidth = 0.0f;
            for (auto& entry : s_entries) {
                float w = ImGui::CalcTextSize(entry.DisplayName.c_str()).x;
                if (w > nameWidth) nameWidth = w;
            }
            float onW = ImGui::CalcTextSize("ON").x;
            float offW = ImGui::CalcTextSize("OFF").x;
            float statusTextMax = (onW > offW ? onW : offW);
            float statusWidth = statusTextMax + padding * 2;
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

using AMenuComponent = MenuUI::MenuItem;
using MenuItem = MenuUI::MenuItem;
using Menu = MenuUI::Menu;
using MenuBool = MenuUI::MenuBool;
using MenuSlider = MenuUI::MenuSlider;
using MenuSliderButton = MenuUI::MenuSliderButton;
using MenuKeyBind = MenuUI::MenuKeyBind;
using MenuList = MenuUI::MenuList;
using MenuColor = MenuUI::MenuColor;
using MenuButton = MenuUI::MenuButton;
using MenuSeparator = MenuUI::MenuSeparator;
using KeyBindType = MenuUI::KeyBindType;

struct MenuManagerState {
    bool MenuVisible = true;
    void (*OnMenuAttached)(Menu*) = nullptr;
};

    namespace MenuManager {
    inline MenuManagerState* g_state = nullptr;
    inline constexpr int kMaxExtraMenus = 128;
    inline Menu* g_extraMenus[kMaxExtraMenus] = {};
    inline int g_extraMenuCount = 0;

    inline Menu::MenuStoreView GetExtraMenus() {
        return Menu::MenuStoreView{ g_extraMenus, &g_extraMenuCount, kMaxExtraMenus };
    }

    inline bool Contains(Menu* menu) {
        if (!menu) return false;
        for (const auto& owned : MenuUI::Menu::GetGlobalMenus()) {
            if (owned == menu) {
                return true;
            }
        }
        for (auto* extra : GetExtraMenus()) {
            if (extra == menu) {
                return true;
            }
        }
        return false;
    }

    inline void Init() {
        static MenuManagerState s_state;
        g_state = &s_state;
    }

    inline void Shutdown() {
        auto extras = GetExtraMenus();
        extras.Clear();
        g_state = nullptr;
    }

    inline void SetMenuVisible(bool visible) {
        if (g_state) {
            g_state->MenuVisible = visible;
        }
    }

    inline bool Add(Menu* menu) {
        if (!menu || Contains(menu)) {
            return false;
        }
        auto extras = GetExtraMenus();
        if (!extras.Push(menu)) {
            return false;
        }
        if (g_state && g_state->OnMenuAttached) {
            g_state->OnMenuAttached(menu);
        }
        return true;
    }

    inline int GetMenuCount() {
        return (int)MenuUI::Menu::GetGlobalMenus().size() + (int)GetExtraMenus().size();
    }

    inline Menu* GetMenu(int index) {
        auto globals = MenuUI::Menu::GetGlobalMenus();
        if (index >= 0 && index < (int)globals.size()) {
            return globals[index];
        }

        index -= (int)globals.size();
        auto extras = GetExtraMenus();
        if (index >= 0 && index < (int)extras.size()) {
            return extras[index];
        }

        return nullptr;
    }
}
} // namespace SDK
