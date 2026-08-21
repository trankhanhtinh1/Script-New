#pragma once

#include "../../../../SDK/SDK.h"

#include <algorithm>

namespace Plugins::KuroAIO {

inline bool Bool(Menu* menu, const char* key, bool fallback = true) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuBool>(key);
    return item ? item->Value : fallback;
}

inline int Slider(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuSlider>(key);
    return item ? item->Value : fallback;
}

inline int List(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuList>(key);
    return item ? item->Index : fallback;
}


inline void SetBool(Menu* menu, const char* key, bool value) {
    if (auto* item = menu ? menu->Get<MenuBool>(key) : nullptr) {
        item->Value = value;
    }
}

inline void SetSlider(Menu* menu, const char* key, int value) {
    if (auto* item = menu ? menu->Get<MenuSlider>(key) : nullptr) {
        item->Value = std::clamp(value, item->MinValue, item->MaxValue);
    }
}


} // namespace Plugins::KuroAIO
