#pragma once

#include "../../../SDK/SDK.h"
#include "../../../SDK/UI/IMenu/Menu.h"

#include <string>
#include <unordered_map>

namespace Plugins::KuroEvade {

struct MenuCache final {
    Menu* Root = nullptr;
    std::unordered_map<std::string, SDK::UI::MenuItem*> Cache;

    MenuCache() = default;
    explicit MenuCache(Menu* menu)
        : Root(menu) {
        AddMenuToCache(menu);
    }

    void AddMenuToCache(Menu* menu) {
        if (!menu) {
            return;
        }
        Root = Root ? Root : menu;
        for (int i = 0; i < menu->Components.size(); ++i) {
            SDK::UI::AMenuComponent* component = menu->Components[i];
            if (auto* child = dynamic_cast<Menu*>(component)) {
                AddMenuToCache(child);
            } else if (auto* item = dynamic_cast<SDK::UI::MenuItem*>(component)) {
                AddMenuItemToCache(item);
            }
        }
    }

    void AddMenuItemToCache(SDK::UI::MenuItem* item) {
        if (!item) {
            return;
        }
        Cache.emplace(item->Name.c_str(), item);
    }

    SDK::UI::MenuItem* Find(const std::string& name) const {
        const auto it = Cache.find(name);
        return it == Cache.end() ? nullptr : it->second;
    }
};

} // namespace Plugins::KuroEvade
