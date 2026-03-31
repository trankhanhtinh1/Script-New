#pragma once

#include "MenuUI.h"

namespace MenuRendererImGui {

    // Current tree renders SDK menus through SDK::MenuUI::Menu directly.
    // Keep this header as a compatibility stub so stale includes and the
    // project browser do not fail on the old host-side renderer API.
    inline void RenderSDKMenuTree() {}

} // namespace MenuRendererImGui
    