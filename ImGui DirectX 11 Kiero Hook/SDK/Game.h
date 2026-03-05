#pragma once
#include "../core/Globals.h"
#include "../core/Offsets.h"
#include "../core/Vector.h"
#include "Enums.h"
#include <string>
#include <cmath>

// ============================================================================
// Game.h — Game State Utilities
// Reference: EnsoulSharp Game class + Script-New-main/SDK/Game.h
// ============================================================================

namespace SDK {
namespace Game {

    // Get current game time (seconds)
    inline float GetTime() {
        float t = Globals::Read<float>(Globals::base + Offset::Global::GameTime);
        if (t > 0.0f && t < 100000.0f)
            return t;
        // Fallback: GameTime might be ptr → dereference
        uintptr_t ptr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::GameTime);
        if (Globals::IsValidPtr(ptr))
            return Globals::Read<float>(ptr);
        return 0.0f;
    }

    // Get tick count based on game time
    inline int GetTickCount() {
        return (int)(GetTime() * 1000.0f);
    }

    // Get mouse world position
    inline Vec3 GetMouseWorldPos() {
        uintptr_t hud = Globals::Read<uintptr_t>(Globals::base + Offset::Global::HudInstance);
        if (!Globals::IsValidPtr(hud)) return Vec3{};
        uintptr_t input = Globals::Read<uintptr_t>(hud + Offset::Hud::Input);
        if (!Globals::IsValidPtr(input)) return Vec3{};
        return Globals::Read<Vec3>(input + Offset::Hud::MouseWorldPos);
    }

    // Get ping (ms)
    inline float GetPing() {
        typedef float(__cdecl* fnGetPing)();
        static uintptr_t addr = Globals::base + Offset::Function::GetPing;
        __try {
            return ((fnGetPing)addr)();
        } __except(1) {
            return 0.0f;
        }
    }

    // Module base
    inline uintptr_t GetModuleBase() {
        return Globals::base;
    }

    // Is player in an active game? (LocalPlayer address is valid)
    inline bool IsInGame() {
        if (!Globals::base) return false;
        uintptr_t localPlayer = Globals::Read<uintptr_t>(Globals::base + Offset::Global::LocalPlayer);
        return Globals::IsValidPtr(localPlayer);
    }

    // Is chat open?
    inline bool IsChatOpen() {
        __try {
            uintptr_t chatInst = Globals::Read<uintptr_t>(Globals::base + Offset::Global::ChatInstance);
            if (!Globals::IsValidPtr(chatInst)) return false;
            unsigned char val = Globals::Read<unsigned char>(chatInst + Offset::Hud::ChatOpen);
            return val == 1; // Only true if exactly 1, not garbage
        } __except(1) {
            return false; // On crash, assume chat is closed (safe default)
        }
    }

    // Is shop open?
    // NOTE: Offset 0x1D77448 found via IDA "ShopOpen" xref but NOT yet verified in-game.
    // Reading wrong data will block all input! Disabled from ShouldProcessInput until verified.
    inline bool IsShopOpen() {
        __try {
            int shopState = Globals::Read<int>(Globals::base + Offset::Global::ShopOpen);
            // Only consider shop open if value is exactly 1 (not random garbage)
            return shopState == 1;
        } __except(1) {
            return false;
        }
    }

    // Is game focused?
    inline bool IsGameFocused() {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    // Should process input (in game + game focused + chat closed)
    // NOTE: IsShopOpen() disabled until offset verified — was blocking all input
    inline bool ShouldProcessInput() {
        return IsInGame() && IsGameFocused() && !IsChatOpen();
        // TODO: Re-enable after verifying ShopOpen offset in-game:
        // return IsInGame() && IsGameFocused() && !IsChatOpen() && !IsShopOpen();
    }

} // namespace Game
} // namespace SDK
