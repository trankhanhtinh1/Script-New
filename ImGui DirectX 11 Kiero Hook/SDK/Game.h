#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "core/Vector.h"
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

    // Is chat input open? (player is typing in chat box)
    // Verified via IDA: sub_3B4E00 sets *(byte*)(ChatClient + 0x10) = 1 on open, 0 on close.
    // ChatClient global at base+0x1D8D240, flag at ChatClient+0x10.
    inline bool IsChatOpen() {
        __try {
            uintptr_t chatClient = Globals::Read<uintptr_t>(Globals::base + Offset::Global::ChatClient);
            if (!Globals::IsValidPtr(chatClient)) return false;
            unsigned char val = Globals::Read<unsigned char>(chatClient + Offset::Hud::ChatOpen);
            return val == 1;
        } __except(1) {
            return false;
        }
    }

    // Is shop window open?
    // Verified via IDA: sub_129FD80 (IsWindowOpen) scans OpenWindowsArray for ShopInstance ptr.
    // - ShopInstance: base+0x1D8D258 → ptr to shop window object
    // - OpenWindowsArray: base+0x1E3DC58 → ptr to array of open window ptrs
    // - OpenWindowsCount: base+0x1E3DC60 → dword count
    // Returns true if ShopInstance is found in the open windows array.
    inline bool IsShopOpen() {
        __try {
            uintptr_t shopInst = Globals::Read<uintptr_t>(Globals::base + Offset::Global::ShopInstance);
            if (!shopInst || !Globals::IsValidPtr(shopInst)) return false;

            uintptr_t arrayPtr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::OpenWindowsArray);
            int count = Globals::Read<int>(Globals::base + Offset::Global::OpenWindowsCount);

            if (!Globals::IsValidPtr(arrayPtr) || count <= 0 || count > 64) return false;

            for (int i = 0; i < count; i++) {
                uintptr_t entry = Globals::Read<uintptr_t>(arrayPtr + (uintptr_t)i * 8);
                if (entry == shopInst) return true;
            }
            return false;
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

    // Should process input (in game + game focused + chat/shop closed)
    // All offsets verified via IDA Pro MCP decompilation:
    //   - ChatOpen: ChatClient(0x1D8D240)+0x10 — sub_3B4E00 confirmed
    //   - ShopOpen: OpenWindowsArray scan — sub_129FD80/sub_BC58F0 confirmed
    inline bool ShouldProcessInput() {
        return IsInGame() && IsGameFocused() && !IsChatOpen() && !IsShopOpen();
    }

} // namespace Game
} // namespace SDK
