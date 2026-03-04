#pragma once
#include "../imgui/imgui.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

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
    // Base MenuItem
    // ========================================================================
    class MenuItem {
    public:
        std::string InternalName;
        std::string DisplayName;
        virtual ~MenuItem() = default;
        virtual void Draw() = 0;

        MenuItem(const std::string& name, const std::string& display)
            : InternalName(name), DisplayName(display) {}
    };

    // ========================================================================
    // MenuBool
    // ========================================================================
    class MenuBool : public MenuItem {
    public:
        bool Enabled;

        MenuBool(const std::string& name, const std::string& display, bool defaultValue = true)
            : MenuItem(name, display), Enabled(defaultValue) {}

        void Draw() override {
            ImGui::Checkbox(DisplayName.c_str(), &Enabled);
        }
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
            : MenuItem(name, display), Value(defaultValue), MinValue(minVal), MaxValue(maxVal) {}

        void Draw() override {
            ImGui::SliderInt(DisplayName.c_str(), &Value, MinValue, MaxValue);
        }
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
            : MenuItem(name, display), Value(defaultValue), MinValue(minVal), MaxValue(maxVal) {}

        void Draw() override {
            ImGui::SliderFloat(DisplayName.c_str(), &Value, MinValue, MaxValue);
        }
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
            : MenuItem(name, display), Items(items), Index(defaultIndex) {}

        void Draw() override {
            // Build items for ImGui combo
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
        }
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
    };

} // namespace MenuUI
} // namespace SDK
