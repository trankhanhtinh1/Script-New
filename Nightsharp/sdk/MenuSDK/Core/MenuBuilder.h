#pragma once

#include "MenuModel.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace NightSharp::Menu {

class MenuBuilder {
public:
    MenuBuilder() = default;
    explicit MenuBuilder(MenuNodeHandle node)
        : node_(std::move(node)) {}

    bool IsValid() const {
        return static_cast<bool>(node_);
    }

    MenuNodeHandle Node() const {
        return node_;
    }

    MenuBuilder Section(std::string id, std::string label) const {
        return node_
            ? MenuBuilder(node_->AddChildHandle(std::move(id), std::move(label)))
            : MenuBuilder();
    }

    MenuItemHandle Checkbox(
        std::string id,
        std::string label,
        bool initialValue = false) const {
        MenuItemHandle item = Add(std::move(id), std::move(label), ItemKind::Toggle);
        if (item) {
            item->value = initialValue;
        }
        return item;
    }

    MenuItemHandle Slider(
        std::string id,
        std::string label,
        int initialValue,
        int minimum,
        int maximum) const {
        MenuItemHandle item = Add(std::move(id), std::move(label), ItemKind::Slider);
        if (item) {
            item->minimum = minimum;
            item->maximum = maximum;
            item->integer = std::clamp(initialValue, minimum, maximum);
        }
        return item;
    }

    MenuItemHandle Dropdown(
        std::string id,
        std::string label,
        std::vector<std::string> options,
        int selected = 0) const {
        MenuItemHandle item = Add(std::move(id), std::move(label), ItemKind::List);
        if (item) {
            item->options = std::move(options);
            item->selected = item->options.empty()
                ? 0
                : std::clamp(selected, 0, static_cast<int>(item->options.size()) - 1);
        }
        return item;
    }

    MenuItemHandle KeyBind(
        std::string id,
        std::string label,
        int initialKey = 0) const {
        MenuItemHandle item = Add(std::move(id), std::move(label), ItemKind::KeyBind);
        if (item) {
            item->key = initialKey;
        }
        return item;
    }

    MenuItemHandle ColorPicker(
        std::string id,
        std::string label,
        std::array<float, 4> initialColor) const {
        MenuItemHandle item = Add(std::move(id), std::move(label), ItemKind::Color);
        if (item) {
            item->color = initialColor;
        }
        return item;
    }

    MenuItemHandle Button(
        std::string id,
        std::string label,
        std::string actionLabel = "Run") const {
        MenuItemHandle item = Add(std::move(id), std::move(label), ItemKind::Action);
        if (item) {
            item->actionLabel = std::move(actionLabel);
        }
        return item;
    }

private:
    MenuItemHandle Add(
        std::string id,
        std::string label,
        ItemKind kind) const {
        return node_
            ? node_->AddItemHandle(std::move(id), std::move(label), kind)
            : nullptr;
    }

    MenuNodeHandle node_;
};

}
