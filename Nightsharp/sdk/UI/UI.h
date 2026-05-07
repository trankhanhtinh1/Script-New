#pragma once

#include "../../menu/MenuUI.h"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

namespace SDK::UI {

    class MenuEntry {
    public:
        MenuEntry() = default;
        explicit MenuEntry(MenuItem* item)
            : m_item(item) {}

        bool IsValid() const {
            return m_item != nullptr;
        }

        MenuItem* Raw() const {
            return m_item;
        }

        operator MenuItem* () const {
            return m_item;
        }

        template<typename T>
        T* GetValue() const {
            return dynamic_cast<T*>(m_item);
        }

        bool Enabled(bool fallback = false) const {
            if (const auto* item = dynamic_cast<MenuBool*>(m_item)) {
                return item->Enabled;
            }
            return fallback;
        }

        int Value(int fallback = 0) const {
            if (const auto* item = dynamic_cast<MenuSlider*>(m_item)) {
                return item->Value;
            }
            if (const auto* item = dynamic_cast<MenuList*>(m_item)) {
                return item->Index;
            }
            if (const auto* item = dynamic_cast<MenuKeyBind*>(m_item)) {
                return item->Key;
            }
            return fallback;
        }

        float ValueF(float fallback = 0.0f) const {
            if (const auto* item = dynamic_cast<MenuUI::MenuSliderF*>(m_item)) {
                return item->Value;
            }
            return fallback;
        }

        int Index(int fallback = 0) const {
            if (const auto* item = dynamic_cast<MenuList*>(m_item)) {
                return item->Index;
            }
            return fallback;
        }

        bool Active(bool fallback = false) const {
            if (const auto* item = dynamic_cast<MenuKeyBind*>(m_item)) {
                return item->Active;
            }
            return fallback;
        }

        void SetEnabled(bool value) const {
            if (auto* item = dynamic_cast<MenuBool*>(m_item)) {
                item->Enabled = value;
            }
        }

        void SetValue(int value) const {
            if (auto* item = dynamic_cast<MenuSlider*>(m_item)) {
                item->SetValue(value);
                return;
            }
            if (auto* item = dynamic_cast<MenuList*>(m_item)) {
                item->SetIndex(value);
            }
        }

        void SetValueF(float value) const {
            if (auto* item = dynamic_cast<MenuUI::MenuSliderF*>(m_item)) {
                item->Value = std::clamp(value, item->MinValue, item->MaxValue);
            }
        }

        void SetIndex(int value) const {
            if (auto* item = dynamic_cast<MenuList*>(m_item)) {
                item->SetIndex(value);
            }
        }

        void SetActive(bool value) const {
            if (auto* item = dynamic_cast<MenuKeyBind*>(m_item)) {
                item->Active = value;
            }
        }

    private:
        MenuItem* m_item = nullptr;
    };

    class MenuNode {
    public:
        MenuNode() = default;
        MenuNode(Menu* menu, bool isRoot)
            : m_menu(menu), m_isRoot(isRoot) {}

        bool IsValid() const {
            return m_menu != nullptr;
        }

        Menu* Raw() const {
            return m_menu;
        }

        operator Menu* () const {
            return m_menu;
        }

        Menu* Attach() const {
            if (m_isRoot && m_menu && !MenuManager::Contains(m_menu)) {
                m_menu->Attach();
            }
            return m_menu;
        }

        MenuNode AddMenu(const std::string& name, const std::string& display) const {
            return MenuNode(m_menu ? m_menu->AddSubMenu(name, display) : nullptr, false);
        }

        MenuNode Add(const MenuNode& child) const {
            if (!m_menu || !child.Raw()) {
                return MenuNode();
            }
            m_menu->Add(static_cast<MenuItem*>(child.Raw()));
            return child;
        }

        MenuEntry Add(const MenuEntry& entry) const {
            if (!m_menu || !entry.Raw()) {
                return MenuEntry();
            }
            m_menu->Add(entry.Raw());
            return entry;
        }

        MenuNode SubMenu(const std::string& name) const {
            return MenuNode(m_menu ? m_menu->GetSubMenu(name) : nullptr, false);
        }

        MenuEntry operator[](const std::string& name) const {
            return MenuEntry(m_menu ? (*m_menu)[name] : nullptr);
        }

        MenuEntry Entry(const std::string& name) const {
            return (*this)[name];
        }

        MenuBool* AddBool(const std::string& name, const std::string& display, bool value = true) const {
            return m_menu ? m_menu->Add<MenuBool>(name, display, value) : nullptr;
        }

        MenuSlider* AddSlider(const std::string& name, const std::string& display, int value = 0, int minValue = 0, int maxValue = 100) const {
            return m_menu ? m_menu->Add<MenuSlider>(name, display, value, minValue, maxValue) : nullptr;
        }

        MenuUI::MenuSliderButton* AddSliderButton(const std::string& name, const std::string& display, int value = 0, int minValue = 0, int maxValue = 100, bool buttonValue = false) const {
            return m_menu ? m_menu->Add<MenuUI::MenuSliderButton>(name, display, value, minValue, maxValue, buttonValue) : nullptr;
        }

        MenuUI::MenuSliderF* AddSliderF(const std::string& name, const std::string& display, float value = 0.0f, float minValue = 0.0f, float maxValue = 100.0f) const {
            return m_menu ? m_menu->Add<MenuUI::MenuSliderF>(name, display, value, minValue, maxValue) : nullptr;
        }

        MenuKeyBind* AddKeyBind(const std::string& name, const std::string& display, int key, KeyBindType type = KeyBindType::Press) const {
            return m_menu ? m_menu->Add<MenuKeyBind>(name, display, key, type) : nullptr;
        }

        MenuList* AddList(const std::string& name, const std::string& display, const std::vector<std::string>& values, int index = 0) const {
            return m_menu ? m_menu->Add<MenuList>(name, display, values, index) : nullptr;
        }

        MenuList* AddList(const std::string& name, const std::string& display, std::initializer_list<const char*> values, int index = 0) const {
            std::vector<std::string> items;
            items.reserve(values.size());
            for (const auto* value : values) {
                items.emplace_back(value ? value : "");
            }
            return AddList(name, display, items, index);
        }

        MenuSeparator* AddSeparator(const std::string& name, const std::string& display) const {
            return m_menu ? m_menu->Add<MenuSeparator>(name, display) : nullptr;
        }

        ::SDK::MenuUI::RadioMenu* AddRadioMenu(const std::string& name, const std::string& display) const {
            return m_menu ? m_menu->AddRadioMenu(name, display) : nullptr;
        }

        MenuButton* AddButton(const std::string& name, const std::string& display, const std::string& text, std::function<void()> onClick = nullptr) const {
            return m_menu ? m_menu->Add<MenuButton>(name, display, text, onClick) : nullptr;
        }

        MenuColor* AddColor(const std::string& name, const std::string& display, const ImVec4& color) const {
            return m_menu ? m_menu->Add<MenuColor>(name, display, color.x, color.y, color.z, color.w) : nullptr;
        }

        bool Bool(const std::string& name, bool fallback = false) const {
            return m_menu ? m_menu->GetBoolValue(name, fallback) : fallback;
        }

        int Slider(const std::string& name, int fallback = 0) const {
            return m_menu ? m_menu->GetSliderValue(name, fallback) : fallback;
        }

        float SliderF(const std::string& name, float fallback = 0.0f) const {
            return m_menu ? m_menu->GetSliderFValue(name, fallback) : fallback;
        }

        int ListIndex(const std::string& name, int fallback = 0) const {
            return m_menu ? m_menu->GetListIndex(name, fallback) : fallback;
        }

        bool KeyActive(const std::string& name, bool fallback = false) const {
            return m_menu ? m_menu->GetKeyBindValue(name, fallback) : fallback;
        }

        // Phase 2 (Apr 26/2026): forwards used by sdk/Wrappers/TargetSelector.
        bool HasItem(const std::string& name) const {
            return m_menu ? m_menu->HasItem(name) : false;
        }

    private:
        Menu* m_menu = nullptr;
        bool m_isRoot = false;
    };

    inline MenuNode CreateMenu(const std::string& name, const std::string& display) {
        return MenuNode(Menu::Create(name, display), true);
    }

    inline MenuNode Wrap(Menu* menu, bool isRoot = false) {
        return MenuNode(menu, isRoot);
    }

} // namespace SDK::UI

namespace SDK {
    using Menu = MenuUI::Menu;
    using MenuItem = MenuUI::MenuItem;
    using MenuValueChangedEventArgs = MenuUI::MenuValueChangedEventArgs;
    using MenuBool = MenuUI::MenuBool;
    using MenuSlider = MenuUI::MenuSlider;
    using MenuSliderButton = MenuUI::MenuSliderButton;
    using MenuSliderF = MenuUI::MenuSliderF;
    using MenuKeyBind = MenuUI::MenuKeyBind;
    using MenuList = MenuUI::MenuList;
    using MenuButton = MenuUI::MenuButton;
    using MenuColor = MenuUI::MenuColor;
    using MenuSeparator = MenuUI::MenuSeparator;
    using RadioMenu = MenuUI::RadioMenu;
    using MenuCustomizer = MenuUI::MenuCustomizer;
    using Notification = MenuUI::Notification;
    using Notifications = MenuUI::Notifications;
    using NotificationType = MenuUI::NotificationType;
    using KeyBindType = MenuUI::KeyBindType;
}

namespace SDK::MenuUI {
    using Menu = ::SDK::Menu;
    using MenuItem = ::SDK::MenuItem;
    using MenuValueChangedEventArgs = ::SDK::MenuValueChangedEventArgs;
    using MenuBool = ::SDK::MenuBool;
    using MenuSlider = ::SDK::MenuSlider;
    using MenuSliderButton = ::SDK::MenuSliderButton;
    using MenuSliderF = ::SDK::MenuSliderF;
    using MenuKeyBind = ::SDK::MenuKeyBind;
    using MenuList = ::SDK::MenuList;
    using MenuButton = ::SDK::MenuButton;
    using MenuColor = ::SDK::MenuColor;
    using MenuSeparator = ::SDK::MenuSeparator;
    using RadioMenu = ::SDK::RadioMenu;
    using MenuCustomizer = ::SDK::MenuCustomizer;
    using Notification = ::SDK::Notification;
    using Notifications = ::SDK::Notifications;
    using NotificationType = ::SDK::NotificationType;
    using KeyBindType = ::SDK::KeyBindType;
    using MenuNode = ::SDK::UI::MenuNode;
    using MenuEntry = ::SDK::UI::MenuEntry;
}
