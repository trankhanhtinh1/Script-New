#pragma once

#include "MenuItem.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace NightSharp::Menu {

class MenuNode;
using MenuItemHandle = std::shared_ptr<MenuItem>;
using MenuNodeHandle = std::shared_ptr<MenuNode>;

class MenuNode {
public:
    MenuNode(std::string nodeId, std::string nodeLabel)
        : id(std::move(nodeId)),
          label(std::move(nodeLabel)) {}

    MenuNodeHandle FindChild(const std::string& nodeId) const {
        const auto found = std::find_if(
            children.begin(),
            children.end(),
            [&nodeId](const MenuNodeHandle& child) {
                return child && child->id == nodeId;
            });
        return found == children.end() ? nullptr : *found;
    }

    MenuItemHandle FindItem(const std::string& itemId) const {
        const auto found = std::find_if(
            items.begin(),
            items.end(),
            [&itemId](const MenuItemHandle& item) {
                return item && item->id == itemId;
            });
        return found == items.end() ? nullptr : *found;
    }

    MenuNodeHandle AddChildHandle(std::string nodeId, std::string nodeLabel) {
        if (MenuNodeHandle existing = FindChild(nodeId)) {
            return existing;
        }
        MenuNodeHandle child = std::make_shared<MenuNode>(
            std::move(nodeId),
            std::move(nodeLabel));
        children.push_back(child);
        return child;
    }

    MenuNode& AddChild(std::string nodeId, std::string nodeLabel) {
        return *AddChildHandle(std::move(nodeId), std::move(nodeLabel));
    }

    MenuItemHandle AddItemHandle(
        std::string itemId,
        std::string itemLabel,
        ItemKind itemKind) {
        if (MenuItemHandle existing = FindItem(itemId)) {
            return existing;
        }
        MenuItemHandle item = std::make_shared<MenuItem>(
            std::move(itemId),
            std::move(itemLabel),
            itemKind);
        items.push_back(item);
        return item;
    }

    MenuItem& AddItem(
        std::string itemId,
        std::string itemLabel,
        ItemKind itemKind) {
        return *AddItemHandle(
            std::move(itemId),
            std::move(itemLabel),
            itemKind);
    }

    bool RemoveChild(const std::string& nodeId) {
        const auto before = children.size();
        std::erase_if(children, [&nodeId](const MenuNodeHandle& child) {
            return child && child->id == nodeId;
        });
        return children.size() != before;
    }

    bool RemoveItem(const std::string& itemId) {
        const auto before = items.size();
        std::erase_if(items, [&itemId](const MenuItemHandle& item) {
            return item && item->id == itemId;
        });
        return items.size() != before;
    }

    std::string id;
    std::string label;
    std::string secondaryLabel;
    bool enabled = true;
    bool visible = true;
    std::vector<MenuNodeHandle> children;
    std::vector<MenuItemHandle> items;
};

}
