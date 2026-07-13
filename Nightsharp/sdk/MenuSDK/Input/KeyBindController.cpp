#include "KeyBindController.h"

#include <cstdio>

namespace NightSharp::Menu {

void KeyBindController::BeginCapture(const MenuItemHandle& item) {
    activeItem_ = item;
}

void KeyBindController::Cancel() {
    activeItem_.reset();
}

void KeyBindController::Reset() {
    activeItem_.reset();
}

bool KeyBindController::IsCapturing(const MenuItem& item) const {
    const MenuItemHandle active = activeItem_.lock();
    return active && active.get() == &item;
}

bool KeyBindController::HandleMessage(UINT message, WPARAM wParam) {
    MenuItemHandle item = activeItem_.lock();
    if (!item) {
        return false;
    }

    int capturedKey = 0;
    switch (message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wParam == VK_ESCAPE) {
            Cancel();
            return true;
        }
        capturedKey = wParam == VK_BACK ? 0 : static_cast<int>(wParam);
        break;
    case WM_LBUTTONDOWN:
        capturedKey = VK_LBUTTON;
        break;
    case WM_RBUTTONDOWN:
        capturedKey = VK_RBUTTON;
        break;
    case WM_MBUTTONDOWN:
        capturedKey = VK_MBUTTON;
        break;
    case WM_XBUTTONDOWN:
        capturedKey = HIWORD(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
        break;
    default:
        return false;
    }

    item->SetKey(capturedKey);
    Cancel();
    return true;
}

std::string KeyBindController::KeyName(int key) {
    switch (key) {
    case 0:
        return "None";
    case VK_LBUTTON:
        return "Mouse 1";
    case VK_RBUTTON:
        return "Mouse 2";
    case VK_MBUTTON:
        return "Mouse 3";
    case VK_XBUTTON1:
        return "Mouse 4";
    case VK_XBUTTON2:
        return "Mouse 5";
    default:
        break;
    }

    UINT scanCode = MapVirtualKeyA(static_cast<UINT>(key), MAPVK_VK_TO_VSC);
    if (key == VK_LEFT || key == VK_UP || key == VK_RIGHT || key == VK_DOWN ||
        key == VK_PRIOR || key == VK_NEXT || key == VK_END || key == VK_HOME ||
        key == VK_INSERT || key == VK_DELETE || key == VK_DIVIDE ||
        key == VK_NUMLOCK) {
        scanCode |= 0x100;
    }

    char name[64] = {};
    if (GetKeyNameTextA(
            static_cast<LONG>(scanCode << 16),
            name,
            static_cast<int>(sizeof(name))) > 0) {
        return name;
    }

    char fallback[16] = {};
    std::snprintf(fallback, sizeof(fallback), "VK %02X", key & 0xFF);
    return fallback;
}

}
