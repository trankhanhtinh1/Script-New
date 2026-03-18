#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "core/Vector.h"
#include "Enums.h"
#include <string>
#include <cmath>
#include "sdk/Utils/DebugConsole.h"

// ============================================================================
// Game.h — Game State Utilities
// Reference: EnsoulSharp Game class + Script-New-main/SDK/Game.h
// ============================================================================

namespace SDK {

    // Global event tick counter — incremented each script tick
    inline int EventSystemTick = 0;

namespace Game {

    // ========================================================================
    // Script Tick Rate Limiter
    // Reference: Script-New "update limit tick for script"
    // Logic runs at 45 FPS (fixed), render runs at full framerate.
    // Benefits: ~6x less CPU, ~6x less memory reads, safer vs anti-cheat.
    // ========================================================================
    inline int ScriptFrameId = 0;
    inline double LastScriptTickWallClock = 0.0;
    inline constexpr double ScriptTickRate = 45.0;
    inline constexpr double ScriptTickIntervalMs = 1000.0 / ScriptTickRate;  // ~22.2ms

    inline int GetScriptFrameId() {
        return ScriptFrameId;
    }

    inline void AdvanceScriptFrame(double nowMs) {
        ++ScriptFrameId;
        LastScriptTickWallClock = nowMs;
    }

    inline bool ShouldRunScriptTick(double nowMs) {
        return (nowMs - LastScriptTickWallClock) >= ScriptTickIntervalMs;
    }

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
    // Additional known offsets across patches: +0x68 (is editing), +0x6C (has focus)
    inline bool IsChatOpen() {
        __try {
            uintptr_t chatClient = Globals::Read<uintptr_t>(Globals::base + Offset::Global::ChatClient);
            if (!Globals::IsValidPtr(chatClient)) {
                // ChatClient ptr invalid — try ChatInstance as backup pointer
                chatClient = Globals::Read<uintptr_t>(Globals::base + Offset::Global::ChatInstance);
                if (!Globals::IsValidPtr(chatClient)) return false;
            }

            // Primary: ChatClient + 0x10 (chat open byte)
            unsigned char val = Globals::Read<unsigned char>(chatClient + Offset::Hud::ChatOpen);
            if (val != 0) return true;

            // Backup offsets used in some patches:
            // +0x68 — some versions use this for "is editing" 
            // +0x6C — some versions use this for "chat has focus"
            unsigned char val2 = Globals::Read<unsigned char>(chatClient + 0x68);
            if (val2 != 0) return true;

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

    // Alternative: keyboard-based chat detection
    // When memory pointers fail (like ChatClient pointing to NULL in current patch),
    // this heuristic tracks the ENTER and ESC keys to simulate the chat open state.
    // It is primarily used to stop the Orbwalker from moving while the player is typing.
    inline bool IsChatOpenByKeyboard() {
        static bool chatIsOpen = false;
        static int lastToggleTick = 0;
        int currentTick = (int)::GetTickCount64(); // Windows API tick count (64-bit to avoid overflow)

        // Ensure game is focused before intercepting keys
        if (!IsGameFocused()) return false;

        // Toggle chat state on Enter key press (with 300ms debounce)
        // 0x8000 checks if key is currently pressed
        if ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) {
            if (currentTick - lastToggleTick > 300) {
                chatIsOpen = !chatIsOpen;
                lastToggleTick = currentTick;
            }
        }

        // Escape always forces chat closed
        if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) {
            chatIsOpen = false;
            // No strict debounce needed because Esc only turns it off
        }

        // Right-Click usually indicates the user is commanding the champion,
        // which implies they are likely NOT in the middle of typing a chat message.
        // Very useful as a self-correcting mechanic if the state gets de-synced.
        if (chatIsOpen && ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0)) {
            chatIsOpen = false;
        }

        return chatIsOpen;
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

    // Should process input (in game + game focused + chat/shop closed)
    // All offsets verified via IDA Pro MCP decompilation:
    //   - ChatOpen: ChatClient(0x1D8D240)+0x10 — sub_3B4E00 confirmed
    //   - ShopOpen: OpenWindowsArray scan — sub_129FD80/sub_BC58F0 confirmed
    //   - ChatOpen: DISABLED (ChatClient 0x1DB43E0 is WRONG — always reads non-zero at +0x10)
    inline bool ShouldProcessInput() {
        if (!IsInGame()) return false;
        if (!IsGameFocused()) return false;
        // NOTE: IsChatOpen() DISABLED — ChatClient offset is wrong, always returns true!
        // Only use keyboard-based detection until correct ChatClient offset is found.
        // bool chatMem = IsChatOpen();  // BROKEN: offset 0x1DB43E0 is not the real ChatClient
        bool chatKb = IsChatOpenByKeyboard();
        bool shopOpen = IsShopOpen();
        if (chatKb || shopOpen) {
            // Throttled debug
            static int s_dbgTick = 0;
            int nowT = (int)::GetTickCount64();
            if (nowT - s_dbgTick > 2000) {
                s_dbgTick = nowT;
                DEBUG_LOG_TAG("INPUT", "ShouldProcessInput=FALSE: chatKb=%d shopOpen=%d", chatKb, shopOpen);
            }
            return false;
        }
        return true;
    }

} // namespace Game
} // namespace SDK
