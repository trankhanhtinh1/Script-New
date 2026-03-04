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

    // Is chat open?
    inline bool IsChatOpen() {
        uintptr_t chatInst = Globals::Read<uintptr_t>(Globals::base + Offset::Global::ChatInstance);
        if (!Globals::IsValidPtr(chatInst)) return false;
        return Globals::Read<unsigned char>(chatInst + Offset::Hud::ChatOpen) != 0;
    }

    // Is game focused?
    inline bool IsGameFocused() {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    // Should process input (game focused + chat closed)
    inline bool ShouldProcessInput() {
        return IsGameFocused() && !IsChatOpen();
    }

} // namespace Game
} // namespace SDK
