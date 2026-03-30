#pragma once

#include "KeyConvert.h"
#include "Cursor.h"

#include <Windows.h>

#include <cstdint>
#include <string>

namespace SDK::Utils {

enum class WindowsMessages : uint32_t {
    KeyDown = WM_KEYDOWN,
    KeyUp = WM_KEYUP,
    SysKeyDown = WM_SYSKEYDOWN,
    SysKeyUp = WM_SYSKEYUP,
    Char = WM_CHAR,
    MouseMove = WM_MOUSEMOVE,
    LButtonDown = WM_LBUTTONDOWN,
    LButtonUp = WM_LBUTTONUP,
    RButtonDown = WM_RBUTTONDOWN,
    RButtonUp = WM_RBUTTONUP,
    MButtonDown = WM_MBUTTONDOWN,
    MButtonUp = WM_MBUTTONUP,
    XButtonDown = WM_XBUTTONDOWN,
    XButtonUp = WM_XBUTTONUP,
    MouseWheel = WM_MOUSEWHEEL
};

struct WndEventArgs {
    uint32_t Msg = 0;
    uintptr_t WParam = 0;
    intptr_t LParam = 0;
    bool Process = true;
};

class WindowsKeys {
public:
    using Keys = uint32_t;

    explicit WindowsKeys(WndEventArgs& args)
        : m_args(args)
        , m_cursor(Utils::Cursor::Position()) {}

    char Char() const {
        return static_cast<char>(m_args.WParam & 0xFF);
    }

    std::string CharText() const {
        const char ch = Char();
        return ch != 0 ? std::string(1, ch) : std::string();
    }

    Vector2 Cursor() const {
        return m_cursor;
    }

    Keys SingleKey() const {
        return static_cast<Keys>(m_args.WParam);
    }

    std::string SingleKeyText() const {
        return KeyConvert::KeyToText(SingleKey());
    }

    Keys Key() const {
        const Keys single = SingleKey();
        const Keys modifiers = ModifierKeys();
        return single != modifiers ? (single | modifiers) : single;
    }

    std::string KeyText() const {
        return KeyConvert::KeyToText(SingleKey());
    }

    WindowsMessages Msg() const {
        return static_cast<WindowsMessages>(m_args.Msg);
    }

    bool IsKeyMessage() const {
        switch (Msg()) {
        case WindowsMessages::KeyDown:
        case WindowsMessages::KeyUp:
        case WindowsMessages::SysKeyDown:
        case WindowsMessages::SysKeyUp:
        case WindowsMessages::Char:
            return true;
        default:
            return false;
        }
    }

    bool IsMouseMessage() const {
        switch (Msg()) {
        case WindowsMessages::MouseMove:
        case WindowsMessages::LButtonDown:
        case WindowsMessages::LButtonUp:
        case WindowsMessages::RButtonDown:
        case WindowsMessages::RButtonUp:
        case WindowsMessages::MButtonDown:
        case WindowsMessages::MButtonUp:
        case WindowsMessages::XButtonDown:
        case WindowsMessages::XButtonUp:
        case WindowsMessages::MouseWheel:
            return true;
        default:
            return false;
        }
    }

    bool Process() const {
        return m_args.Process;
    }

    void SetProcess(bool value) const {
        m_args.Process = value;
    }

    Keys SideButton() const {
        const auto hi = HIBYTE(HIWORD(static_cast<DWORD>(m_args.WParam)));
        if (hi == 1) {
            return VK_XBUTTON1;
        }
        if (hi == 2) {
            return VK_XBUTTON2;
        }
        return 0;
    }

    uint32_t WParam() const {
        return static_cast<uint32_t>(m_args.WParam);
    }

    int WheelDelta() const {
        return GET_WHEEL_DELTA_WPARAM(static_cast<WPARAM>(m_args.WParam));
    }

    bool Alt() const {
        return (::GetKeyState(VK_MENU) & 0x8000) != 0;
    }

    bool Ctrl() const {
        return (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
    }

    bool Shift() const {
        return (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
    }

    bool NoModifiers() const {
        return !Alt() && !Ctrl() && !Shift();
    }

private:
    static Keys ModifierKeys() {
        Keys keyData = 0;
        if ((::GetKeyState(VK_SHIFT) & 0x8000) != 0) {
            keyData |= static_cast<Keys>(VK_SHIFT);
        }
        if ((::GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            keyData |= static_cast<Keys>(VK_CONTROL);
        }
        if ((::GetKeyState(VK_MENU) & 0x8000) != 0) {
            keyData |= static_cast<Keys>(VK_MENU);
        }
        return keyData;
    }

    WndEventArgs& m_args;
    Vector2 m_cursor = {};
};

} // namespace SDK::Utils
