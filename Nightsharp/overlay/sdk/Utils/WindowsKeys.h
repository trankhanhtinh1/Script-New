#pragma once

#include "../Core/Game.h"
#include "../Enumerations/WindowsMessages.h"
#include "Cursor.h"

#include <Windows.h>
#include <cstdint>

namespace SDK::Core::Utils {

class WindowsKeys {
public:
    explicit WindowsKeys(SDK::Game::WndEventArgs& args)
        : args_(args),
          CursorPos(Cursor::Position()) {}

    char Char() const {
        return static_cast<char>(args_.WParam);
    }

    int Key() const {
        return static_cast<int>(args_.WParam);
    }

    WindowsMessages Msg() const {
        return static_cast<WindowsMessages>(args_.Msg);
    }

    bool Process() const {
        return args_.Process;
    }

    void Process(bool value) {
        args_.Process = value;
    }

    int SideButton() const {
        const std::uint16_t high = HIWORD(static_cast<DWORD_PTR>(args_.WParam));
        if (high == XBUTTON1) {
            return VK_XBUTTON1;
        }
        if (high == XBUTTON2) {
            return VK_XBUTTON2;
        }
        return 0;
    }

    int SingleKey() const {
        return static_cast<int>(args_.WParam);
    }

    std::uint32_t WParam() const {
        return static_cast<std::uint32_t>(args_.WParam);
    }

    Vec2 CursorPos = {};

private:
    SDK::Game::WndEventArgs& args_;
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using WindowsKeys = ::SDK::Core::Utils::WindowsKeys;
} // namespace SDK::Utils
