#pragma once

#include "ItemKind.h"

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace NightSharp::Menu {

class MenuItem {
public:
    using ChangeHandler = std::function<void(MenuItem&)>;

    MenuItem(std::string itemId, std::string itemLabel, ItemKind itemKind)
        : id(std::move(itemId)),
          label(std::move(itemLabel)),
          kind(itemKind) {}

    bool SetChecked(bool next) {
        if (value == next) {
            return false;
        }
        value = next;
        Notify();
        return true;
    }

    bool SetInteger(int next) {
        next = std::clamp(next, minimum, maximum);
        if (integer == next) {
            return false;
        }
        integer = next;
        Notify();
        return true;
    }

    bool SetSelected(int next) {
        if (options.empty()) {
            next = 0;
        } else {
            next = std::clamp(next, 0, static_cast<int>(options.size()) - 1);
        }
        if (selected == next) {
            return false;
        }
        selected = next;
        Notify();
        return true;
    }

    bool SetKey(int next) {
        if (key == next) {
            return false;
        }
        key = next;
        Notify();
        return true;
    }

    bool SetColor(const std::array<float, 4>& next) {
        if (color == next) {
            return false;
        }
        color = next;
        Notify();
        return true;
    }

    void Subscribe(ChangeHandler handler) {
        if (handler) {
            subscribers_.push_back(std::move(handler));
        }
    }

    void Notify() {
        if (onChanged) {
            onChanged(*this);
        }
        for (ChangeHandler& handler : subscribers_) {
            handler(*this);
        }
    }

    std::string id;
    std::string label;
    ItemKind kind = ItemKind::Toggle;
    bool enabled = true;
    bool visible = true;
    bool value = false;
    bool loaded = false;
    bool loadAlways = false;
    int integer = 0;
    int minimum = 0;
    int maximum = 100;
    int selected = 0;
    int key = 0;
    std::array<float, 4> color = { 1.0f, 1.0f, 1.0f, 1.0f };
    std::vector<std::string> options;
    std::string actionLabel;
    ChangeHandler onChanged;

private:
    std::vector<ChangeHandler> subscribers_;
};

}
