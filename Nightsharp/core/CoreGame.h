#pragma once

#include "CoreBypass.h"
#include "CoreMap.h"
#include "CoreRuntime.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"
#include "spoof/spoofcall.h"
#include "../DebugLog.h"

#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace CoreGame {

enum class MapId : int {
    Unknown = 0,
    SummonersRiftOriginalSummer = 1,
    SummonersRiftOriginalAutumn = 2,
    ProvingGrounds = 3,
    TwistedTreelineOriginal = 4,
    CrystalScar = 8,
    TwistedTreeline = 10,
    SummonersRift = 11,
    HowlingAbyss = 12,
    ButchersBridge = 14,
    CosmicRuins = 16,
    ValoranCityPark = 18,
    Substructure43 = 19,
    CrashSite = 20,
    NexusBlitz = 21,
    TeamfightTactics = 22,
    Arena = 30,
    Swarm = 33,
    Bandlewood = 35,
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

    inline constexpr int ChatChannelAll = 1;
    inline constexpr int ChatChannelTeam = 2;
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
    const auto trampoline = CoreBypass::ResolveSpoofTrampoline();
    __try {
        const int ping = Globals::IsValidPtr(trampoline)
            ? spoof_call_hybrid(reinterpret_cast<void*>(trampoline), reinterpret_cast<GetPingFn>(ctx.getPingFn), ctx.netInstance)
            : reinterpret_cast<GetPingFn>(ctx.getPingFn)(ctx.netInstance);
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

// Focus state pushed by the overlay's WndProc hook (WM_ACTIVATEAPP). When set,
// IsGameFocused() reads this flag instead of polling GetForegroundWindow() — a
// USER32 call that intermittently BLOCKS for several ms when the foreground
// window's thread is busy, spiking the per-tick key-check cost (measured: the
// champion's `dispatch-keys` oscillated 3 -> 120 ms/s, and the orbwalker handler
// stayed 5-6 ms/fire, both from the ~12 GetForegroundWindow calls/tick that
// Key()/ShouldProcessInput() issue). WM_ACTIVATEAPP updates this instantly with
// zero syscalls. g_windowFocusTracked stays false until the overlay seeds it, so
// there is always a safe fallback.
inline bool g_windowFocusTracked = false;
inline bool g_windowFocused = true;

inline void SetWindowFocused(bool focused) {
    g_windowFocused = focused;
    g_windowFocusTracked = true;
}

inline bool IsGameFocused() {
    if (g_windowFocusTracked) {
        return g_windowFocused;
    }

    // Fallback before the WndProc hook seeds the flag: still cache within the
    // current GetTickCount() window (~15 ms) to avoid one syscall per Key() call.
    static thread_local DWORD s_tick = 0;
    static thread_local bool s_focused = false;
    const DWORD now = GetTickCount();
    if (now != s_tick) {
        s_tick = now;
        const HWND foreground = GetForegroundWindow();
        if (!foreground) {
            s_focused = false;
        } else {
            DWORD processId = 0;
            GetWindowThreadProcessId(foreground, &processId);
            s_focused = (processId == GetCurrentProcessId());
        }
    }
    return s_focused;
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
    case ::CoreMap::MapId::SummonersRiftOriginalSummer:
        return MapId::SummonersRiftOriginalSummer;
    case ::CoreMap::MapId::SummonersRiftOriginalAutumn:
        return MapId::SummonersRiftOriginalAutumn;
    case ::CoreMap::MapId::ProvingGrounds:
        return MapId::ProvingGrounds;
    case ::CoreMap::MapId::TwistedTreelineOriginal:
        return MapId::TwistedTreelineOriginal;
    case ::CoreMap::MapId::CrystalScar:
        return MapId::CrystalScar;
    case ::CoreMap::MapId::SummonersRift:
        return MapId::SummonersRift;
    case ::CoreMap::MapId::HowlingAbyss:
        return MapId::HowlingAbyss;
    case ::CoreMap::MapId::TwistedTreeline:
        return MapId::TwistedTreeline;
    case ::CoreMap::MapId::ButchersBridge:
        return MapId::ButchersBridge;
    case ::CoreMap::MapId::CosmicRuins:
        return MapId::CosmicRuins;
    case ::CoreMap::MapId::ValoranCityPark:
        return MapId::ValoranCityPark;
    case ::CoreMap::MapId::Substructure43:
        return MapId::Substructure43;
    case ::CoreMap::MapId::CrashSite:
        return MapId::CrashSite;
    case ::CoreMap::MapId::NexusBlitz:
        return MapId::NexusBlitz;
    case ::CoreMap::MapId::TeamfightTactics:
        return MapId::TeamfightTactics;
    case ::CoreMap::MapId::Arena:
        return MapId::Arena;
    case ::CoreMap::MapId::Swarm:
        return MapId::Swarm;
    case ::CoreMap::MapId::Bandlewood:
        return MapId::Bandlewood;
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

inline bool Print(const char* text, bool /*triggerEvent*/ = true, std::uint32_t flags = 0) {
    if (!text) {
        text = "";
    }

    NightSharpDebug::Logf("[Game.Print] %s", text);

    auto& ctx = CoreRuntime::g_ctx;
    if (ctx.moduleBase) {
        // sub_B62E90 dispatcher: pre-game queues the line, in-game tail-calls
        // sub_B62690 to render it. 'this' is the chat container pointer.
        const uintptr_t container = Globals::Read<uintptr_t>(
            ctx.moduleBase + Offset::GameRuntime::ChatMessageInstance);
        if (Globals::IsValidPtr(container)) {
            using DisplayChatFn = void(__fastcall*)(uintptr_t, const char*, int);
            const auto fn = reinterpret_cast<DisplayChatFn>(
                ctx.moduleBase + Offset::GameRuntime::PrintChat);
            const auto trampoline = CoreBypass::ResolveSpoofTrampoline();
            __try {
                if (Globals::IsValidPtr(trampoline)) {
                    spoof_call_hybrid(reinterpret_cast<void*>(trampoline), fn, container, text, static_cast<int>(flags));
                } else {
                    fn(container, text, static_cast<int>(flags));
                }
                return true;
            }
            __except (1) {}
        }
    }

    detail::LogOnce(
        "print-native-missing",
        "[CoreGame] ChatViewController.DisplayChat container/offset unavailable; Game.Print is routed to NightSharpDebug.");
    return false;
}

inline bool Say(const char* text, bool sendToAll = false, bool /*triggerEvent*/ = true) {
    if (!detail::IsPlausibleGameText(text)) {
        return false;
    }

    auto& ctx = CoreRuntime::g_ctx;
    if (!ctx.moduleBase || !Globals::IsValidPtr(ctx.netInstance) || !ctx.sendChatFn) {
        (void)CoreRuntime::RefreshReadState();
    }

    uintptr_t client = ctx.netInstance;
    if (!Globals::IsValidPtr(client) && ctx.netInstanceGlobal) {
        client = Globals::Read<uintptr_t>(ctx.netInstanceGlobal);
    }

    const uintptr_t fnAddr = ctx.sendChatFn
        ? ctx.sendChatFn
        : (ctx.moduleBase ? ctx.moduleBase + Offset::GameRuntime::SendChat : 0);
    const bool canCallNative =
        Globals::IsValidPtr(client) &&
        Globals::IsExecutablePtrCached(fnAddr, 16);

    if (canCallNative) {
        using SendChatFn = char(__fastcall*)(uintptr_t, const char*, int);
        const int channel = sendToAll ? detail::ChatChannelAll : detail::ChatChannelTeam;
        const auto fn = reinterpret_cast<SendChatFn>(fnAddr);
        const auto trampoline = CoreBypass::ResolveSpoofTrampoline();

        __try {
            const char result = Globals::IsValidPtr(trampoline)
                ? spoof_call_hybrid(reinterpret_cast<void*>(trampoline), fn, client, text, channel)
                : fn(client, text, channel);
            return result != 0;
        }
        __except (1) {
            detail::LogOnce(
                "say-native-exception",
                "[CoreGame] Native Game.Say threw an exception; message was not sent.");
            return false;
        }
    }

    detail::LogOnce(
        "say-native-unavailable",
        "[CoreGame] MultiplayerClient.SendChat unavailable; Game.Say uses focused-window clipboard fallback.");
    return detail::PasteChatLine(text, sendToAll);
}

inline bool Say(const std::string& text, bool sendToAll = false, bool triggerEvent = true) {
    return Say(text.c_str(), sendToAll, triggerEvent);
}

inline bool SendPing(PingCategory /*pingType*/, const Vec2& /*position*/, std::uint32_t /*targetNetworkId*/ = 0) {
    detail::LogOnce(
        "sendping-native-missing",
        "[CoreGame] SmartPingClientManager.CallCurrentPing native offset is not verified; Game.SendPing is disabled.");
    return false;
}

inline bool SendPing(PingCategory pingType, const Vec3& position, std::uint32_t targetNetworkId = 0) {
    const bool result = SendPing(pingType, Vec2(position.x, position.z), targetNetworkId);
    return result;
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
    const bool result = IsGameFocused() && detail::SendCtrlKey(key);
    return result;
}

inline bool SendMasteryBadge() {
    const bool result = IsGameFocused() && detail::SendCtrlKey('6');
    return result;
}

inline bool SendSummonerEmote(SummonerEmoteSlot slot) {
    if (slot == SummonerEmoteSlot::Mastery) {
        const bool mastery = SendMasteryBadge();
        return mastery;
    }

    detail::LogOnce(
        "summoner-emote-native-missing",
        "[CoreGame] EmoteRadialViewController.FireEventForSlot native offset is not verified; non-mastery summoner emotes are disabled.");
    return false;
}

} // namespace CoreGame
