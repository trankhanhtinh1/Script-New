#pragma once

#include "MenuBuilder.h"

#include <string>
#include <utility>

namespace NightSharp::Menu {

class MenuRegistry {
public:
    MenuModel& Model() {
        return model_;
    }

    const MenuModel& Model() const {
        return model_;
    }

    MenuBuilder Root(std::string id, std::string label) {
        return MenuBuilder(model_.AddRootHandle(
            std::move(id),
            std::move(label)));
    }

    MenuBuilder Plugin(
        std::string pluginId,
        std::string pluginLabel,
        std::string groupId = "plugins",
        std::string groupLabel = "Plugins") {
        MenuNodeHandle group = model_.AddRootHandle(
            std::move(groupId),
            std::move(groupLabel));
        return MenuBuilder(group->AddChildHandle(
            std::move(pluginId),
            std::move(pluginLabel)));
    }

    MenuNodeHandle FindPlugin(
        const std::string& pluginId,
        const std::string& groupId = "plugins") const {
        MenuNodeHandle group = model_.FindRoot(groupId);
        return group ? group->FindChild(pluginId) : nullptr;
    }

    bool RemovePlugin(
        const std::string& pluginId,
        const std::string& groupId = "plugins") {
        MenuNodeHandle group = model_.FindRoot(groupId);
        return group && group->RemoveChild(pluginId);
    }

private:
    MenuModel model_;
};

}
