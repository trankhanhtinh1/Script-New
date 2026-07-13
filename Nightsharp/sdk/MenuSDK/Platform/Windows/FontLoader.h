#pragma once

#include "imgui.h"

#include <string>

namespace NightSharp::Menu {

struct FontLoadResult {
    ImFont* font = nullptr;
    std::string family;
    bool fromWindows = false;
};

FontLoadResult LoadWindowsFont(ImFontAtlas* atlas, float pixelSize);

}
