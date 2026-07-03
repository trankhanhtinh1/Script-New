#pragma once

#include "CoreMap.h"
#include "CoreRuntime.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"
#include "../DebugLog.h"

#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace CoreGame {

enum class MapId : int {
    Unknown = 0,
    TwistedTreeline = 10,
    SummonersRift = 11,
    HowlingAbyss = 12,
};

enum class PingCategory : int {
    Ping = 0,
    RadialDanger = 2,
    EnemyMissing = 3,
    OnMyWay = 4,
    Caution = 5,
    AssistMe = 6,
    EnemyVision = 7,
    VisionCleared = 13,
    NeedVision = 14,
    Push = 15,
    AllIn = 16,
    Retreat = 18,
    Bait = 19,
    Hold = 20,
};

enum class EmoteId : int {
    Dance = 0,
    Taunt = 1,
    Laugh = 2,
    Joke = 3,
    Toggle = 4,
};

enum class SummonerEmoteSlot : int {
    North = 0,
    East = 1,
    South = 2,
    West = 3,
    Center = 4,
    Spawn = 5,
    Victory = 6,
    FirstBlood = 7,
    Ace = 8,
    Mastery = 9,
};

enum InputBlockFlags : std::uint32_t {
    InputBlock_None       = 0,
    InputBlock_NotInGame  = 1u << 0,
    InputBlock_NotFocused = 1u << 1,
    InputBlock_ChatMemory = 1u << 2,
    InputBlock_ChatKey    = 1u << 3,
    InputBlock_ShopOpen   = 1u << 4,
};

struct InputDebugState {
    bool inGame = false;
    bool focused = false;
    bool chatMemory = false;
    bool chatKeyboard = false;
    bool shopOpen = false;
    std::uint8_t chatPrimary = 0;
    std::uint8_t chatEditing = 0;
    std::uint8_t chatFocused = 0;
    std::uint32_t blockMask = InputBlock_None;
};

inline InputDebugState g_inputDebug = {};

inline bool IsGameFocused();

namespace detail {
    inline bool IsPlausibleGameText(const char* text) {
        if (!text || !*text) {
            return false;
        }

        std::size_t len = 0;
        for (; text[len] && len <= 4096; ++len) {}
        return len > 0 && len <= 4096;
    }

    inline std::wstring Utf8ToWide(const char* text) {
        if (!text || !*text) {
            return {};
        }

        int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
        UINT codePage = CP_UTF8;
        DWORD flags = MB_ERR_INVALID_CHARS;
        if (required <= 0) {
            codePage = CP_ACP;
            flags = 0;
            required = MultiByteToWideChar(codePage, flags, text, -1, nullptr, 0);
        }
        if (required <= 1) {
            return {};
        }

        std::wstring out(static_cast<std::size_t>(required - 1), L'\0');
        MultiByteToWideChar(codePage, flags, text, -1, out.data(), required);
        return out;
    }

    inline bool ReadClipboardText(std::wstring& out) {
        out.clear();
        HANDLE data = GetClipboardData(CF_UNICODETEXT);
        if (!data) {
            return true;
        }

        const wchar_t* locked = static_cast<const wchar_t*>(GlobalLock(data));
        if (!locked) {
            return false;
        }
        out = locked;
        GlobalUnlock(data);
        return true;
    }

    inline bool SetClipboardText(const std::wstring& text) {
        const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory) {
            return false;
        }

        void* locked = GlobalLock(memory);
        if (!locked) {
            GlobalFree(memory);
            return false;
        }
        std::memcpy(locked, text.c_str(), static_cast<std::size_t>(bytes));
        GlobalUnlock(memory);

        EmptyClipboard();
        if (!SetClipboardData(CF_UNICODETEXT, memory)) {
            GlobalFree(memory);
            return false;
        }
        return true;
    }

    inline void SendKey(WORD vk, bool down) {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        if (!down) {
            input.ki.dwFlags = KEYEVENTF_KEYUP;
        }
        SendInput(1, &input, sizeof(input));
    }

    inline bool SendCtrlKey(WORD vk) {
        if (!vk) {
            return false;
        }

        INPUT input[4] = {};
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = VK_CONTROL;
        input[1].type = INPUT_KEYBOARD;
        input[1].ki.wVk = vk;
        input[2].type = INPUT_KEYBOARD;
        input[2].ki.wVk = vk;
        input[2].ki.dwFlags = KEYEVENTF_KEYUP;
        input[3].type = INPUT_KEYBOARD;
        input[3].ki.wVk = VK_CONTROL;
        input[3].ki.dwFlags = KEYEVENTF_KEYUP;
        return SendInput(4, input, sizeof(INPUT)) == 4;
    }

    inline bool PasteChatLine(const char* text, bool sendToAll) {
        if (!IsPlausibleGameText(text) || !IsGameFocused()) {
            return false;
        }

        std::string line = text;
        if (sendToAll &&
            line.rfind("/all ", 0) != 0 &&
            line.rfind("/allchat ", 0) != 0) {
            line.insert(0, "/all ");
        }

        std::wstring wide = Utf8ToWide(line.c_str());
        if (wide.empty()) {
            return false;
        }

        const HWND window = GetForegroundWindow();
        if (!OpenClipboard(window)) {
            return false;
        }

        std::wstring previous;
        const bool hadPrevious = ReadClipboardText(previous);
        if (!SetClipboardText(wide)) {
            CloseClipboard();
            return false;
        }
        CloseClipboard();

        SendKey(VK_RETURN, true);
        SendKey(VK_RETURN, false);
        Sleep(15);
        SendCtrlKey('V');
        Sleep(15);
        SendKey(VK_RETURN, true);
        SendKey(VK_RETURN, false);

        if (OpenClipboard(window)) {
            if (hadPrevious) {
                SetClipboardText(previous);
            } else {
                EmptyClipboard();
            }
            CloseClipboard();
        }
        return true;
    }

    inline bool LogOnce(const char* key, const char* message) {
        struct Entry {
            const char* key = nullptr;
            bool logged = false;
        };
        static Entry entries[16] = {};

        for (auto& entry : entries) {
            if (entry.key == key || (entry.key && key && std::strcmp(entry.key, key) == 0)) {
                if (!entry.logged) {
                    NightSharpDebug::Logf("%s", message);
                    entry.logged = true;
                    return true;
                }
                return false;
            }
        }

        for (auto& entry : entries) {
            if (!entry.key) {
                entry.key = key;
                entry.logged = true;
                NightSharpDebug::Logf("%s", message);
                return true;
            }
        }
        return false;
    }
} // namespace detail

inline float GetTime() {
    auto& ctx = CoreRuntime::g_ctx;
    if ((ctx.statusMask & CoreRuntime::Status_GameTimeReady) == 0) {
        (void)CoreRuntime::RefreshReadState();
    }
    return ctx.gameTime >= 0.0f && ctx.gameTime < 1000000.0f
        ? ctx.gameTime
        : 0.0f;
}

inline int GetPing() {
    auto& ctx = CoreRuntime::g_ctx;
    if (ctx.cachedPing > 0 && ctx.cachedPing < 5000) {
        return ctx.cachedPing;
    }

    (void)CoreRuntime::RefreshReadState();
    if (!ctx.getPingFn || !Globals::IsValidPtr(ctx.netInstance)) {
        return 0;
    }

    using GetPingFn = int(__fastcall*)(uintptr_t);
    __try {
        const int ping = reinterpret_cast<GetPingFn>(ctx.getPingFn)(ctx.netInstance);
        if (ping > 0 && ping < 5000) {
            ctx.cachedPing = ping;
            return ping;
        }
    }
    __except (1) {}
    return 0;
}

inline bool IsInGame() {
    (void)CoreRuntime::RefreshReadState();
    const auto& ctx = CoreRuntime::GetContext();
    return Globals::IsValidPtr(ctx.localPlayer) &&
           Globals::IsValidPtr(ctx.objectManager) &&
           GetTime() > 0.0f;
}

inline bool IsGameFocused() {
    const HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return false;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(foreground, &processId);
    return processId == GetCurrentProcessId();
}

inline bool IsChatOpen() {
    g_inputDebug.chatPrimary = 0;
    g_inputDebug.chatEditing = 0;
    g_inputDebug.chatFocused = 0;
    g_inputDebug.chatMemory = false;

    const auto& ctx = CoreRuntime::GetContext();
    uintptr_t chatView = ctx.chatViewController;
    if (!Globals::IsValidPtr(chatView) && ctx.moduleBase) {
        chatView = Globals::Read<uintptr_t>(
            ctx.moduleBase + Offset::GameRuntime::ChatViewController);
    }
    if (!Globals::IsValidPtr(chatView)) {
        return false;
    }

    __try {
        const auto panelOpen =
            Globals::Read<std::uint8_t>(chatView + Offset::ChatViewControllerLayout::PrimaryOpen);
        const auto inputActive =
            Globals::Read<std::uint8_t>(chatView + Offset::ChatViewControllerLayout::InputActive);

        g_inputDebug.chatPrimary = panelOpen;
        g_inputDebug.chatEditing = inputActive;
        if (inputActive > 1) {
            return false;
        }

        g_inputDebug.chatMemory = inputActive != 0;
        return g_inputDebug.chatMemory;
    }
    __except (1) {
        return false;
    }
}

inline bool IsOpenChat() {
    return IsChatOpen();
}

// Chat-open keyboard state — updated by SDK::Game WndProc handler (no polling)
inline bool g_chatOpenByKeyboard = false;

inline bool IsChatOpenByKeyboard() {
    if (!IsGameFocused()) {
        return false;
    }
    return g_chatOpenByKeyboard;
}

inline bool IsShopOpen() {
    const auto& ctx = CoreRuntime::GetContext();
    uintptr_t shop = ctx.shopInstance;
    uintptr_t windows = ctx.openWindowsArray;
    std::uint32_t count = ctx.openWindowsCount;

    if (!Globals::IsValidPtr(shop) && ctx.moduleBase) {
        shop = Globals::Read<uintptr_t>(
            ctx.moduleBase + Offset::GameRuntime::ShopInstance);
    }
    if (!Globals::IsValidPtr(windows) && ctx.moduleBase) {
        windows = Globals::Read<uintptr_t>(
            ctx.moduleBase + Offset::GameRuntime::OpenWindowsArray);
        count = Globals::Read<std::uint32_t>(
            ctx.moduleBase + Offset::GameRuntime::OpenWindowsCount);
    }

    if (!Globals::IsValidPtr(shop) ||
        !Globals::IsValidPtr(windows) ||
        count == 0 ||
        count > 64) {
        return false;
    }

    for (std::uint32_t i = 0; i < count; ++i) {
        const auto entry = Globals::Read<uintptr_t>(
            windows + static_cast<uintptr_t>(i) * sizeof(uintptr_t));
        if (entry == shop) {
            return true;
        }
    }
    return false;
}

inline bool IsOpenShop() {
    return IsShopOpen();
}

inline MapId GetMapId() {
    switch (::CoreMap::GetMapId()) {
    case ::CoreMap::MapId::SummonersRift:
        return MapId::SummonersRift;
    case ::CoreMap::MapId::HowlingAbyss:
        return MapId::HowlingAbyss;
    case ::CoreMap::MapId::TwistedTreeline:
        return MapId::TwistedTreeline;
    default:
        return MapId::Unknown;
    }
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

inline std::uint32_t GetInputBlockMask() {
    return g_inputDebug.blockMask;
}

inline InputDebugState GetInputDebugState() {
    return g_inputDebug;
}

inline bool Print(const char* text, bool /*triggerEvent*/ = true, std::uint32_t /*flags*/ = 0) {
    if (!text) {
        text = "";
    }

    NightSharpDebug::Logf("[Game.Print] %s", text);
    detail::LogOnce(
        "print-native-missing",
        "[CoreGame] ChatViewController.DisplayChat native offset is not verified; Game.Print is routed to NightSharpDebug.");
    return false;
}

inline bool Say(const char* text, bool sendToAll = false, bool /*triggerEvent*/ = true) {
    if (!detail::IsPlausibleGameText(text)) {
        return false;
    }

    detail::LogOnce(
        "say-native-missing",
        "[CoreGame] MultiplayerClient.SendChat native offset is not verified; Game.Say uses focused-window clipboard fallback.");
    return detail::PasteChatLine(text, sendToAll);
}

inline bool SendPing(PingCategory /*pingType*/, const Vec2& /*position*/, std::uint32_t /*targetNetworkId*/ = 0) {
    detail::LogOnce(
        "sendping-native-missing",
        "[CoreGame] SmartPingClientManager.CallCurrentPing native offset is not verified; Game.SendPing is disabled.");
    return false;
}

inline bool SendPing(PingCategory pingType, const Vec3& position, std::uint32_t targetNetworkId = 0) {
    return SendPing(pingType, Vec2(position.x, position.z), targetNetworkId);
}

inline bool ShowPing(PingCategory /*pingType*/,
                     const Vec2& /*position*/,
                     std::uint32_t /*targetIndex*/,
                     std::uint32_t /*sourceIndex*/,
                     bool /*playAudio*/ = true,
                     bool /*showChat*/ = true) {
    detail::LogOnce(
        "showping-native-missing",
        "[CoreGame] MenuGUI.PingMiniMap native offset is not verified; Game.ShowPing is disabled.");
    return false;
}

inline bool ShowPing(PingCategory pingType,
                     const Vec3& position,
                     std::uint32_t targetIndex,
                     std::uint32_t sourceIndex,
                     bool playAudio = true,
                     bool showChat = true) {
    return ShowPing(pingType, Vec2(position.x, position.z), targetIndex, sourceIndex, playAudio, showChat);
}

inline bool SendEmote(EmoteId emoteId) {
    WORD key = 0;
    switch (emoteId) {
    case EmoteId::Joke:
        key = '1';
        break;
    case EmoteId::Taunt:
        key = '2';
        break;
    case EmoteId::Dance:
        key = '3';
        break;
    case EmoteId::Laugh:
        key = '4';
        break;
    case EmoteId::Toggle:
        key = '5';
        break;
    default:
        return false;
    }
    return IsGameFocused() && detail::SendCtrlKey(key);
}

inline bool SendMasteryBadge() {
    return IsGameFocused() && detail::SendCtrlKey('6');
}

inline bool SendSummonerEmote(SummonerEmoteSlot slot) {
    if (slot == SummonerEmoteSlot::Mastery) {
        return SendMasteryBadge();
    }

    detail::LogOnce(
        "summoner-emote-native-missing",
        "[CoreGame] EmoteRadialViewController.FireEventForSlot native offset is not verified; non-mastery summoner emotes are disabled.");
    return false;
}

} // namespace CoreGame
