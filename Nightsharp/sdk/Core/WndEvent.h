#pragma once

#include <Windows.h>

#include <cstdint>

namespace SDK::Game {

struct WndEventArgs {
    HWND HWnd = nullptr;
    std::uint32_t Msg = 0;
    std::uintptr_t WParam = 0;
    std::intptr_t LParam = 0;
    bool Process = true;
};

using WndProcHandler = void(*)(WndEventArgs&);

inline bool AddOnWndProc(WndProcHandler handler);
inline bool RemoveOnWndProc(WndProcHandler handler);

}
