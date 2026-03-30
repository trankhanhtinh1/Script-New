#pragma once

#include "CoreNavGrid.h"
#include "CoreRuntime.h"

#include <Windows.h>

namespace CoreGame {

    enum MapId : int {
        Map_Unknown = 0,
        Map_SummonersRift = 11,
        Map_TwistedTreeline = 10,
        Map_HowlingAbyss = 12
    };

    enum InputBlockFlags : uint32_t {
        InputBlock_None       = 0,
        InputBlock_NotInGame  = 1u << 0,
        InputBlock_NotFocused = 1u << 1,
        InputBlock_ChatMemory = 1u << 2,
        InputBlock_ChatKey    = 1u << 3,
        InputBlock_ShopOpen   = 1u << 4
    };

    struct InputDebugState {
        bool inGame = false;
        bool focused = false;
        bool chatMemory = false;
        bool chatKeyboard = false;
        bool shopOpen = false;
        uint8_t chatPrimary = 0;
        uint8_t chatEditing = 0;
        uint8_t chatFocused = 0;
        uint32_t blockMask = InputBlock_None;
    };

    inline InputDebugState g_inputDebug = {};

    inline bool IsInGame() {
        return CoreRuntime::IsReady();
    }

    inline bool IsChatOpen() {
        const auto& ctx = CoreRuntime::GetContext();
        g_inputDebug.chatPrimary = 0;
        g_inputDebug.chatEditing = 0;
        g_inputDebug.chatFocused = 0;
        g_inputDebug.chatMemory = false;

        uintptr_t chatClient = ctx.chatClient;
        if (!Globals::IsValidPtr(chatClient) && ctx.moduleBase) {
            chatClient = Globals::Read<uintptr_t>(ctx.moduleBase + Offset::Global::ChatInstance);
        }
        if (!Globals::IsValidPtr(chatClient)) {
            return false;
        }

        __try {
            const auto primary = Globals::Read<uint8_t>(chatClient + Offset::Hud::ChatOpen);
            const auto editing = Globals::Read<uint8_t>(chatClient + 0x68);
            const auto focused = Globals::Read<uint8_t>(chatClient + 0x6C);

            g_inputDebug.chatPrimary = primary;
            g_inputDebug.chatEditing = editing;
            g_inputDebug.chatFocused = focused;

            const bool sane =
                primary <= 1 &&
                editing <= 1 &&
                focused <= 1;
            if (!sane) {
                return false;
            }

            g_inputDebug.chatMemory = (primary != 0) || (editing != 0) || (focused != 0);
            return g_inputDebug.chatMemory;
        }
        __except (1) {
            return false;
        }
    }

    inline bool IsGameFocused() {
        HWND fg = GetForegroundWindow();
        if (!fg) {
            return false;
        }

        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    inline bool IsChatOpenByKeyboard() {
        static bool chatIsOpen = false;
        static DWORD lastToggleTick = 0;

        if (!IsGameFocused()) {
            return false;
        }

        const DWORD now = GetTickCount();
        if ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) {
            if (now - lastToggleTick > 300) {
                chatIsOpen = !chatIsOpen;
                lastToggleTick = now;
            }
        }

        if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) {
            chatIsOpen = false;
        }

        if (chatIsOpen && ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0)) {
            chatIsOpen = false;
        }

        return chatIsOpen;
    }

    inline bool IsShopOpen() {
        const auto& ctx = CoreRuntime::GetContext();
        uintptr_t shopInstance = ctx.shopInstance;
        uintptr_t openWindowsArray = ctx.openWindowsArray;
        uint32_t openWindowsCount = ctx.openWindowsCount;

        if (!Globals::IsValidPtr(shopInstance) && ctx.moduleBase) {
            shopInstance = Globals::Read<uintptr_t>(ctx.moduleBase + Offset::Global::ShopInstance);
        }
        if (!Globals::IsValidPtr(openWindowsArray) && ctx.moduleBase) {
            openWindowsArray = Globals::Read<uintptr_t>(ctx.moduleBase + Offset::Global::OpenWindowsArray);
            openWindowsCount = Globals::Read<uint32_t>(ctx.moduleBase + Offset::Global::OpenWindowsCount);
        }

        if (!Globals::IsValidPtr(shopInstance) ||
            !Globals::IsValidPtr(openWindowsArray) ||
            openWindowsCount == 0 ||
            openWindowsCount > 64) {
            return false;
        }

        for (uint32_t i = 0; i < openWindowsCount; ++i) {
            const auto entry = Globals::Read<uintptr_t>(openWindowsArray + static_cast<uintptr_t>(i * sizeof(uintptr_t)));
            if (entry == shopInstance) {
                return true;
            }
        }
        return false;
    }

    inline MapId GetMapId() {
        const auto navGrid = CoreNavGrid::Get();
        if (!navGrid.IsValid()) {
            return Map_Unknown;
        }

        if (navGrid.width > 280 && navGrid.width < 310 &&
            navGrid.height > 285 && navGrid.height < 310) {
            return Map_SummonersRift;
        }

        if (navGrid.width > 240 && navGrid.width < 270 &&
            navGrid.height > 238 && navGrid.height < 265) {
            return Map_HowlingAbyss;
        }

        if (navGrid.width > 300 && navGrid.width < 320 &&
            navGrid.height > 300 && navGrid.height < 320) {
            return Map_TwistedTreeline;
        }

        return Map_Unknown;
    }

    inline bool ShouldProcessInput() {
        g_inputDebug = {};
        g_inputDebug.inGame = IsInGame();
        if (!g_inputDebug.inGame) {
            g_inputDebug.blockMask |= InputBlock_NotInGame;
            return false;
        }

        g_inputDebug.focused = IsGameFocused();
        if (!g_inputDebug.focused) {
            g_inputDebug.blockMask |= InputBlock_NotFocused;
        }

        g_inputDebug.chatMemory = IsChatOpen();
        if (g_inputDebug.chatMemory) {
            g_inputDebug.blockMask |= InputBlock_ChatMemory;
        }

        g_inputDebug.chatKeyboard = IsChatOpenByKeyboard();
        if (g_inputDebug.chatKeyboard) {
            g_inputDebug.blockMask |= InputBlock_ChatKey;
        }

        g_inputDebug.shopOpen = IsShopOpen();
        if (g_inputDebug.shopOpen) {
            g_inputDebug.blockMask |= InputBlock_ShopOpen;
        }

        return g_inputDebug.blockMask == InputBlock_None;
    }

    inline uint32_t GetInputBlockMask() {
        return g_inputDebug.blockMask;
    }

    inline InputDebugState GetInputDebugState() {
        return g_inputDebug;
    }

} // namespace CoreGame
