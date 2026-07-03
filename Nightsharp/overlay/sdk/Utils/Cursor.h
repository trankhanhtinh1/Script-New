#pragma once

#include "../Core/Game.h"
#include "../UI/Drawing.h"

#include <Windows.h>
#include <windowsx.h>

namespace SDK::Core::Utils {

class Cursor {
public:
    static Vec2 GameScreenPosition() {
        Vec2 screen{};
        (void)SDK::Drawing::WorldToScreen(SDK::Game::CursorPosRaw(), screen);
        return screen;
    }

    static bool IsOverHUD() {
        return Position() != GameScreenPosition();
    }

    static Vec2 Position() {
        EnsureInstalled();
        if (posX_ == 0 && posY_ == 0) {
            POINT pt{};
            if (GetCursorPos(&pt)) {
                posX_ = pt.x;
                posY_ = pt.y;
            }
        }
        return Vec2(static_cast<float>(posX_), static_cast<float>(posY_));
    }

private:
    static inline int posX_ = 0;
    static inline int posY_ = 0;
    static inline bool installed_ = false;

    static void Game_OnWndProc(SDK::Game::WndEventArgs& args) {
        if (args.Msg == WM_MOUSEMOVE ||
            args.Msg == WM_LBUTTONDOWN ||
            args.Msg == WM_RBUTTONDOWN ||
            args.Msg == WM_MBUTTONDOWN) {
            const LPARAM lParam = static_cast<LPARAM>(args.LParam);
            posX_ = GET_X_LPARAM(lParam);
            posY_ = GET_Y_LPARAM(lParam);
        }
    }

    static void EnsureInstalled() {
        if (!installed_) {
            installed_ = true;
            SDK::Game::AddOnWndProc(&Game_OnWndProc);
        }
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using Cursor = ::SDK::Core::Utils::Cursor;
} // namespace SDK::Utils
