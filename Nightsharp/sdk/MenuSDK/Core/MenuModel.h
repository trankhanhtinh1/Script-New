#pragma once

#include "MenuNode.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace NightSharp::Menu {

using MenuPath = std::vector<int>;

class MenuModel {
public:
    MenuNodeHandle FindRoot(const std::string& nodeId) const {
        const auto found = std::find_if(
            roots.begin(),
            roots.end(),
            [&nodeId](const MenuNodeHandle& node) {
                return node && node->id == nodeId;
            });
        return found == roots.end() ? nullptr : *found;
    }

    MenuNodeHandle AddRootHandle(std::string nodeId, std::string nodeLabel) {
        if (MenuNodeHandle existing = FindRoot(nodeId)) {
            return existing;
        }
        MenuNodeHandle root = std::make_shared<MenuNode>(
            std::move(nodeId),
            std::move(nodeLabel));
        roots.push_back(root);
        return root;
    }

    MenuNode& AddRoot(std::string nodeId, std::string nodeLabel) {
        return *AddRootHandle(std::move(nodeId), std::move(nodeLabel));
    }

    bool RemoveRoot(const std::string& nodeId) {
        const auto before = roots.size();
        std::erase_if(roots, [&nodeId](const MenuNodeHandle& node) {
            return node && node->id == nodeId;
        });
        return roots.size() != before;
    }

    MenuNode* Resolve(const MenuPath& path) {
        if (path.empty() ||
            path.front() < 0 ||
            path.front() >= static_cast<int>(roots.size())) {
            return nullptr;
        }

        MenuNodeHandle node = roots[path.front()];
        for (std::size_t depth = 1; depth < path.size(); ++depth) {
            const int index = path[depth];
            if (!node ||
                index < 0 ||
                index >= static_cast<int>(node->children.size())) {
                return nullptr;
            }
            node = node->children[index];
        }
        return node.get();
    }

    const MenuNode* Resolve(const MenuPath& path) const {
        return const_cast<MenuModel*>(this)->Resolve(path);
    }

    std::vector<MenuNodeHandle> roots;
};

}
