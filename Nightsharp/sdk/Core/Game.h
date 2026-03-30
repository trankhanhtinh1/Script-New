#pragma once

#include "../../core/CoreAPI.h"
#include "../../menu/MenuUI.h"
#include "Objects.h"

#include <Windows.h>

namespace SDK {

namespace Game {

    using UpdateHandler = void(*)();
    using DrawHandler = void(*)();

    inline MenuUI::FixedList<UpdateHandler, 64> g_onUpdate = {};
    inline MenuUI::FixedList<DrawHandler, 64> g_onDraw = {};

    inline bool AddOnUpdate(UpdateHandler handler) {
        return handler && g_onUpdate.push_back(handler);
    }

    inline bool OnUpdate(UpdateHandler handler) {
        return AddOnUpdate(handler);
    }

    inline bool AddOnDraw(DrawHandler handler) {
        return handler && g_onDraw.push_back(handler);
    }

    inline bool OnDraw(DrawHandler handler) {
        return AddOnDraw(handler);
    }

    inline void DispatchUpdate() {
        for (const auto& cb : g_onUpdate) {
            if (cb) {
                cb();
            }
        }
    }

    inline void DispatchDraw() {
        for (const auto& cb : g_onDraw) {
            if (cb) {
                cb();
            }
        }
    }

    inline float Time() {
        return CoreAPI::Game::GetTime();
    }

    inline int Ping() {
        return CoreAPI::Control::GetPing();
    }

    inline int TickCount() {
        return static_cast<int>(::GetTickCount());
    }

    inline Vector3 CursorPos() {
        return CoreAPI::View::GetMouseWorldPos();
    }

    inline bool IsChatOpen() {
        return CoreAPI::Game::IsChatOpen();
    }

    inline bool IsShopOpen() {
        return CoreAPI::Game::IsShopOpen();
    }

    inline bool IsChatOpenByKeyboard() {
        return CoreAPI::Game::IsChatOpenByKeyboard();
    }

    inline bool IsFocused() {
        return CoreAPI::Game::IsGameFocused();
    }

    inline uint32_t InputBlockMask() {
        return CoreAPI::Game::GetInputBlockMask();
    }

    inline bool CanProcessInput() {
        return CoreAPI::Game::ShouldProcessInput();
    }

    inline bool ShouldProcessInput() {
        return CanProcessInput();
    }
}

namespace Variables {
    inline int TickCount() { return Game::TickCount(); }
}

namespace MenuGUI {
    inline bool IsChatOpen() { return Game::IsChatOpen(); }
}

} // namespace SDK
