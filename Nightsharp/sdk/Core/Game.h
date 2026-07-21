#pragma once

#include "../../Core/CoreGame.h"
#include "../../Core/CoreRuntime.h"
#include "../../Core/CoreMap.h"
#include "../../Core/Globals.h"
#include "../../Core/Vector.h"
#include "../../DebugLog.h"
#include "../Events/Events.h"
#include "../GameObjects/ObjectManager.h"
#include "Objects.h"

#include <DirectXMath.h>
#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace SDK::Game {

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

namespace detail {
    template <typename Handler, int MaxHandlers = 64>
    struct HandlerList {
        Handler handlers[MaxHandlers] = {};
        int count = 0;

        bool Add(Handler handler) {
            if (!handler) {
                return false;
            }
            for (int i = 0; i < count; ++i) {
                if (handlers[i] == handler) {
                    return true;
                }
            }
            if (count >= MaxHandlers) {
                return false;
            }
            handlers[count++] = handler;
            return true;
        }

        bool Remove(Handler handler) {
            if (!handler) {
                return false;
            }
            for (int i = 0; i < count; ++i) {
                if (handlers[i] != handler) {
                    continue;
                }
                for (int j = i; j + 1 < count; ++j) {
                    handlers[j] = handlers[j + 1];
                }
                handlers[--count] = nullptr;
                return true;
            }
            return false;
        }

        void Clear() {
            for (int i = 0; i < count; ++i) {
                handlers[i] = nullptr;
            }
            count = 0;
        }
    };
} // namespace detail

struct WndEventArgs {
    HWND HWnd = nullptr;
    std::uint32_t Msg = 0;
    std::uintptr_t WParam = 0;
    std::intptr_t LParam = 0;
    bool Process = true;
};

using PingCategory = ::CoreGame::PingCategory;
using EmoteId = ::CoreGame::EmoteId;
using SummonerEmoteSlot = ::CoreGame::SummonerEmoteSlot;

struct GameSendChatEventArgs {
    const char* Msg = "";
    bool SendToAll = false;
    bool Process = true;
};

struct GameDisplayChatEventArgs {
    const char* Msg = "";
    int Flags = 0;
    bool Process = true;
};

using SendChatEventArgs = GameSendChatEventArgs;
using DisplayChatEventArgs = GameDisplayChatEventArgs;

using UpdateHandler = void(*)();
using WndProcHandler = void(*)(WndEventArgs&);
using SendChatHandler = void(*)(GameSendChatEventArgs&);
using DisplayChatHandler = void(*)(GameDisplayChatEventArgs&);

// Forward declarations (defined below) — needed for circular include safety
inline bool AddOnWndProc(WndProcHandler handler);
inline bool RemoveOnWndProc(WndProcHandler handler);

namespace detail {
    inline HandlerList<UpdateHandler> UpdateHandlers;
    inline HandlerList<WndProcHandler> WndProcHandlers;
    inline HandlerList<SendChatHandler> SendChatHandlers;
    inline HandlerList<DisplayChatHandler> DisplayChatHandlers;
    inline bool UpdateBridgeInstalled = false;
    inline InputDebugState InputDebug = {};

    // Chat-open state tracked from WndProc messages (replaces per-frame GetAsyncKeyState polling)
    inline bool ChatOpenByKeyboard = false;
    inline DWORD ChatLastToggleTick = 0;
    inline bool ChatWndProcInstalled = false;

    inline void ChatWndProcHandler(WndEventArgs& args) {
        if (args.Msg == WM_KEYDOWN) {
            const DWORD now = GetTickCount();
            if (args.WParam == VK_RETURN && now - ChatLastToggleTick > 300) {
                ChatOpenByKeyboard = !ChatOpenByKeyboard;
                ::CoreGame::g_chatOpenByKeyboard = ChatOpenByKeyboard;
                ChatLastToggleTick = now;
            }
            if (args.WParam == VK_ESCAPE) {
                ChatOpenByKeyboard = false;
                ::CoreGame::g_chatOpenByKeyboard = false;
            }
        }
        if (args.Msg == WM_RBUTTONDOWN && ChatOpenByKeyboard) {
            ChatOpenByKeyboard = false;
            ::CoreGame::g_chatOpenByKeyboard = false;
        }
    }
 
    inline bool IsFiniteFloat(float value) {
        return std::isfinite(value);
    }

    inline float AbsFloat(float value) {
        return value < 0.0f ? -value : value;
    }

    inline bool IsPlausibleWorldVec3(const Vec3& value) {
        return IsFiniteFloat(value.x) && IsFiniteFloat(value.y) && IsFiniteFloat(value.z) &&
               value.x >= -30000.0f && value.x <= 30000.0f &&
               value.y >= -5000.0f && value.y <= 5000.0f &&
               value.z >= -30000.0f && value.z <= 30000.0f &&
               (AbsFloat(value.x) > 1.0f || AbsFloat(value.z) > 1.0f);
    }

    inline bool IsPlausibleScreenVec2(const Vec2& value, const Vec2& size = {}) {
        if (!IsFiniteFloat(value.x) || !IsFiniteFloat(value.y)) {
            return false;
        }

        const float maxX = size.x > 0.0f ? size.x * 2.0f : 8192.0f;
        const float maxY = size.y > 0.0f ? size.y * 2.0f : 8192.0f;
        return value.x >= -maxX && value.x <= maxX &&
               value.y >= -maxY && value.y <= maxY &&
               (AbsFloat(value.x) > 0.5f || AbsFloat(value.y) > 0.5f);
    }

    inline void OnCoreUpdate(const SDK::Events::GameUpdateEventArgs&) {
        for (int i = 0; i < UpdateHandlers.count; ++i) {
            if (auto handler = UpdateHandlers.handlers[i]) {
                __try {
                    handler();
                } __except (1) {}
            }
        }
    }

    inline void EnsureUpdateBridge() {
        if (UpdateBridgeInstalled) {
            return;
        }
        UpdateBridgeInstalled = true;
        SDK::Events::AddOnGameUpdate(&OnCoreUpdate);
    }

    inline bool DispatchSendChat(GameSendChatEventArgs& args) {
        for (int i = 0; i < SendChatHandlers.count; ++i) {
            if (auto handler = SendChatHandlers.handlers[i]) {
                __try {
                    handler(args);
                } __except (1) {}
            }
        }
        return args.Process;
    }

    inline bool DispatchDisplayChat(GameDisplayChatEventArgs& args) {
        for (int i = 0; i < DisplayChatHandlers.count; ++i) {
            if (auto handler = DisplayChatHandlers.handlers[i]) {
                __try {
                    handler(args);
                } __except (1) {}
            }
        }
        return args.Process;
    }

    inline HWND FindProcessWindow() {
        struct EnumData {
            DWORD pid;
            HWND result;
        } data = { GetCurrentProcessId(), nullptr };

        EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL {
            auto* d = reinterpret_cast<EnumData*>(lParam);
            DWORD pid = 0;
            GetWindowThreadProcessId(hWnd, &pid);
            if (pid != d->pid || !IsWindowVisible(hWnd)) {
                return TRUE;
            }
            if ((GetWindowLongPtrW(hWnd, GWL_STYLE) & WS_CHILD) != 0) {
                return TRUE;
            }

            RECT rc = {};
            GetClientRect(hWnd, &rc);
            if ((rc.right - rc.left) <= 0 || (rc.bottom - rc.top) <= 0) {
                return TRUE;
            }

            d->result = hWnd;
            return FALSE;
        }, reinterpret_cast<LPARAM>(&data));

        return data.result;
    }

    inline bool GetRendererSize(Vec2& out) {
        out = {};
        const auto& ctx = CoreRuntime::GetContext();

        if (Globals::IsValidPtr(ctx.renderer)) {
            __try {
                const int width = Globals::Read<int>(
                    ctx.renderer + Offset::D3D::ScreenWidth);
                const int height = Globals::Read<int>(
                    ctx.renderer + Offset::D3D::ScreenHeight);
                if (width > 0 && height > 0 && width < 20000 && height < 20000) {
                    out = { static_cast<float>(width), static_cast<float>(height) };
                    return true;
                }
            } __except (1) {
                out = {};
            }
        }

        HWND gameWindow = FindProcessWindow();
        if (gameWindow) {
            RECT rc = {};
            if (GetClientRect(gameWindow, &rc) &&
                rc.right > rc.left &&
                rc.bottom > rc.top) {
                out = {
                    static_cast<float>(rc.right - rc.left),
                    static_cast<float>(rc.bottom - rc.top)
                };
                return true;
            }
        }

        const int width = GetSystemMetrics(SM_CXSCREEN);
        const int height = GetSystemMetrics(SM_CYSCREEN);
        if (width > 0 && height > 0) {
            out = { static_cast<float>(width), static_cast<float>(height) };
            return true;
        }
        return false;
    }

    inline bool ReadViewProjection(float out[16]) {
        if (!out) {
            return false;
        }

        const auto& ctx = CoreRuntime::GetContext();
        const uintptr_t inst = ctx.viewProjInstance;
        if (!Globals::IsValidPtr(inst)) {
            return false;
        }

        float view[16] = {};
        float proj[16] = {};
        __try {
            for (int i = 0; i < 16; ++i) {
                view[i] = Globals::Read<float>(inst + static_cast<uintptr_t>(i * sizeof(float)));
                proj[i] = Globals::Read<float>(
                    inst + Offset::DrawingMatrixRuntime::ProjMatrixRelative +
                    static_cast<uintptr_t>(i * sizeof(float)));
            }
        } __except (1) {
            return false;
        }

        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += view[row * 4 + k] * proj[k * 4 + col];
                }
                out[row * 4 + col] = sum;
            }
        }
        return true;
    }

    inline float PlayerPlaneY() {
        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(ctx.localPlayer)) {
            return 0.0f;
        }

        __try {
            const Vec3 position = Globals::Read<Vec3>(ctx.localPlayer + Offset::All::Position);
            if (position.IsValid() && std::isfinite(position.y)) {
                return position.y;
            }
        } __except (1) {}
        return 0.0f;
    }

    inline bool ScreenToWorldOnPlane(const Vec2& screen, float planeY, Vec3& out) {
        out = {};

        Vec2 size = {};
        float matrix[16] = {};
        if (!GetRendererSize(size) ||
            !IsPlausibleScreenVec2(screen, size) ||
            !ReadViewProjection(matrix)) {
            return false;
        }

        using namespace DirectX;
        XMMATRIX viewProj(
            matrix[0], matrix[1], matrix[2], matrix[3],
            matrix[4], matrix[5], matrix[6], matrix[7],
            matrix[8], matrix[9], matrix[10], matrix[11],
            matrix[12], matrix[13], matrix[14], matrix[15]);

        XMVECTOR det = XMVectorZero();
        XMMATRIX inv = XMMatrixInverse(&det, viewProj);
        if (XMVectorGetX(det) == 0.0f) {
            return false;
        }

        const float ndcX = (screen.x / size.x) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (screen.y / size.y) * 2.0f;

        XMVECTOR nearClip = XMVectorSet(ndcX, ndcY, 0.0f, 1.0f);
        XMVECTOR farClip = XMVectorSet(ndcX, ndcY, 1.0f, 1.0f);
        XMVECTOR nearWorld = XMVector3TransformCoord(nearClip, inv);
        XMVECTOR farWorld = XMVector3TransformCoord(farClip, inv);

        Vec3 nearPoint{
            XMVectorGetX(nearWorld),
            XMVectorGetY(nearWorld),
            XMVectorGetZ(nearWorld)
        };
        Vec3 farPoint{
            XMVectorGetX(farWorld),
            XMVectorGetY(farWorld),
            XMVectorGetZ(farWorld)
        };

        if (!IsPlausibleWorldVec3(nearPoint) && !IsPlausibleWorldVec3(farPoint)) {
            return false;
        }

        const Vec3 dir = farPoint - nearPoint;
        if (AbsFloat(dir.y) < 0.0001f) {
            out = farPoint;
            return IsPlausibleWorldVec3(out);
        }

        const float t = (planeY - nearPoint.y) / dir.y;
        out = nearPoint + dir * t;
        out.y = planeY;
        return IsPlausibleWorldVec3(out);
    }

    inline bool ReadHudMouseWorld(Vec3& out) {
        out = {};
        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(ctx.hudInstance)) {
            return false;
        }

        __try {
            const uintptr_t hudInput = Globals::Read<uintptr_t>(ctx.hudInstance + Offset::HudRuntime::Input);
            if (Globals::IsValidPtr(hudInput)) {
                const Vec3 mouseWorld = Globals::Read<Vec3>(hudInput + Offset::HudRuntime::MouseWorldPos);
                if (IsPlausibleWorldVec3(mouseWorld)) {
                    out = mouseWorld;
                    return true;
                }
            }
        } __except (1) {
            out = {};
        }
        return false;
    }

    inline bool ReadCursorGlobals(Vec3& out) {
        out = {};
        const auto& ctx = CoreRuntime::GetContext();
        if (!ctx.cursorInstanceGlobal) {
            return false;
        }

        __try {
            // Patch 15.x (verified IDA 13337): cursorInstanceGlobal points
            // directly to the cursor world Vec3 (qword_1F76630 = x,y;
            // dword_1F76638 = z). Read it directly — no pointer chase.
            const Vec3 direct = Globals::Read<Vec3>(ctx.cursorInstanceGlobal);
            if (IsPlausibleWorldVec3(direct)) {
                out = direct;
                return true;
            }
        } __except (1) {
            out = {};
        }
        return false;
    }

    inline bool GetMouseScreenPos(Vec2& out) {
        out = {};
        const auto& ctx = CoreRuntime::GetContext();
        Vec2 size = {};
        (void)GetRendererSize(size);

        if (Globals::IsValidPtr(ctx.mouseScreenVec2) && ctx.mouseScreenVec2 != ctx.objectManager) {
            __try {
                // Native getter sub_5999D0 returns the two int32 fields at
                // MouseInput + 0x0C and MouseInput + 0x10.
                const Vec2 candidate(
                    static_cast<float>(Globals::Read<std::int32_t>(
                        ctx.mouseScreenVec2 + Offset::MouseInputLayout::ScreenX)),
                    static_cast<float>(Globals::Read<std::int32_t>(
                        ctx.mouseScreenVec2 + Offset::MouseInputLayout::ScreenY)));
                if (IsPlausibleScreenVec2(candidate, size)) {
                    out = candidate;
                    return true;
                }
            } __except (1) {
                out = {};
            }
        }

        POINT pt = {};
        if (GetCursorPos(&pt)) {
            HWND gameWindow = FindProcessWindow();
            if (gameWindow) {
                ScreenToClient(gameWindow, &pt);
            }
            const Vec2 candidate(static_cast<float>(pt.x), static_cast<float>(pt.y));
            if (IsPlausibleScreenVec2(candidate, size)) {
                out = candidate;
                return true;
            }
        }
        return false;
    }

    inline bool ReadCursorRaw(Vec3& out) {
        out = {};
        (void)CoreRuntime::RefreshReadState();

        if (ReadHudMouseWorld(out) || ReadCursorGlobals(out)) {
            return true;
        }

        Vec2 screen{};
        if (GetMouseScreenPos(screen)) {
            return ScreenToWorldOnPlane(screen, PlayerPlaneY(), out);
        }
        return false;
    }

    inline float ReadTime() {
        auto& ctx = CoreRuntime::g_ctx;
        if ((ctx.statusMask & CoreRuntime::Status_GameTimeReady) == 0) {
            (void)CoreRuntime::RefreshReadState();
        }
        return ctx.gameTime >= 0.0f && ctx.gameTime < 1000000.0f
            ? ctx.gameTime
            : 0.0f;
    }

    inline int ReadPing() {
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
            const int ping =
                reinterpret_cast<GetPingFn>(ctx.getPingFn)(ctx.netInstance);
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
               ReadTime() > 0.0f;
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
        InputDebug.chatPrimary = 0;
        InputDebug.chatEditing = 0;
        InputDebug.chatFocused = 0;
        InputDebug.chatMemory = false;

        const auto& ctx = CoreRuntime::GetContext();
        uintptr_t chatController = ctx.chatViewController;
        if (!Globals::IsValidPtr(chatController)) {
            chatController = CoreRuntime::ReadGlobalPtr(ctx.chatViewControllerGlobal);
        }
        if (!Globals::IsValidPtr(chatController)) {
            return false;
        }

        __try {
            const auto panelOpen = Globals::Read<std::uint8_t>(
                chatController + Offset::ChatViewControllerLayout::PrimaryOpen);
            const auto inputActive = Globals::Read<std::uint8_t>(
                chatController + Offset::ChatViewControllerLayout::InputActive);

            InputDebug.chatPrimary = panelOpen;
            InputDebug.chatEditing = inputActive;
            if (inputActive > 1) {
                return false;
            }

            InputDebug.chatMemory = inputActive != 0;
            return InputDebug.chatMemory;
        }
        __except (1) {
            return false;
        }
    }

    inline bool IsChatOpenByKeyboard() {
        if (!detail::ChatWndProcInstalled) {
            AddOnWndProc(&detail::ChatWndProcHandler);
            detail::ChatWndProcInstalled = true;
        }
        if (!IsGameFocused()) {
            return false;
        }
        return detail::ChatOpenByKeyboard;
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
            if (Globals::Read<uintptr_t>(
                    windows + static_cast<uintptr_t>(i) * sizeof(uintptr_t)) == shop) {
                return true;
            }
        }
        return false;
    }

    inline bool ShouldProcessInput() {
        InputDebug = {};
        InputDebug.inGame = IsInGame();
        if (!InputDebug.inGame) {
            InputDebug.blockMask |= InputBlock_NotInGame;
            return false;
        }

        InputDebug.focused = IsGameFocused();
        if (!InputDebug.focused) {
            InputDebug.blockMask |= InputBlock_NotFocused;
        }

        InputDebug.chatMemory = IsChatOpen();
        if (InputDebug.chatMemory) {
            InputDebug.blockMask |= InputBlock_ChatMemory;
        }

        InputDebug.chatKeyboard = IsChatOpenByKeyboard();
        if (InputDebug.chatKeyboard) {
            InputDebug.blockMask |= InputBlock_ChatKey;
        }

        InputDebug.shopOpen = IsShopOpen();
        if (InputDebug.shopOpen) {
            InputDebug.blockMask |= InputBlock_ShopOpen;
        }
        return InputDebug.blockMask == InputBlock_None;
    }
} // namespace detail

inline float Time() {
    return ::CoreGame::GetTime();
}

inline int Ping() {
    return ::CoreGame::GetPing();
}

inline int TickCount() {
    return static_cast<int>(Time() * 1000.0f);
}

inline Vec3 CursorPosRaw() {
    Vec3 out = {};
    return detail::ReadCursorRaw(out) ? out : Vec3{};
}

inline Vec3 CursorPos() {
    return CursorPosRaw();
}

inline Vec3 CursorPosition() {
    return CursorPosRaw();
}

inline bool IsReady() {
    return ::CoreGame::IsInGame();
}

inline bool IsChatOpen() {
    return ::CoreGame::IsChatOpen();
}

inline bool IsOpenChat() {
    return IsChatOpen();
}

inline bool IsShopOpen() {
    return ::CoreGame::IsShopOpen();
}

inline bool IsOpenShop() {
    return IsShopOpen();
}

inline bool IsFocused() {
    return ::CoreGame::IsGameFocused();
}

inline bool ShouldProcessInput() {
    return ::CoreGame::ShouldProcessInput();
}

inline bool CanProcessInput() {
    return ShouldProcessInput();
}

inline std::uint32_t InputBlockMask() {
    return ::CoreGame::GetInputBlockMask();
}

inline int MapId() {
    return static_cast<int>(::CoreGame::GetMapId());
}

inline bool AddOnUpdate(UpdateHandler handler) {
    detail::EnsureUpdateBridge();
    return detail::UpdateHandlers.Add(handler);
}

inline bool RemoveOnUpdate(UpdateHandler handler) {
    return detail::UpdateHandlers.Remove(handler);
}

inline bool AddOnWndProc(WndProcHandler handler) {
    return detail::WndProcHandlers.Add(handler);
}

inline bool RemoveOnWndProc(WndProcHandler handler) {
    return detail::WndProcHandlers.Remove(handler);
}

inline bool AddOnSendChat(SendChatHandler handler) {
    return detail::SendChatHandlers.Add(handler);
}

inline bool RemoveOnSendChat(SendChatHandler handler) {
    return detail::SendChatHandlers.Remove(handler);
}

inline bool AddOnDisplayChat(DisplayChatHandler handler) {
    return detail::DisplayChatHandlers.Add(handler);
}

inline bool RemoveOnDisplayChat(DisplayChatHandler handler) {
    return detail::DisplayChatHandlers.Remove(handler);
}

inline bool DispatchWndProc(HWND hWnd, std::uint32_t msg, std::uintptr_t wParam, std::intptr_t lParam) {
    WndEventArgs args{};
    args.HWnd = hWnd;
    args.Msg = msg;
    args.WParam = wParam;
    args.LParam = lParam;
    args.Process = true;

    for (int i = 0; i < detail::WndProcHandlers.count; ++i) {
        if (auto handler = detail::WndProcHandlers.handlers[i]) {
            __try {
                handler(args);
            } __except (1) {}
        }
    }
    return args.Process;
}

inline void Reset() {
    if (detail::UpdateBridgeInstalled) {
        SDK::Events::RemoveOnGameUpdate(&detail::OnCoreUpdate);
    }
    detail::UpdateHandlers.Clear();
    detail::WndProcHandlers.Clear();
    detail::SendChatHandlers.Clear();
    detail::DisplayChatHandlers.Clear();
    detail::UpdateBridgeInstalled = false;
    detail::InputDebug = {};
}

inline bool Print(const char* text, bool triggerEvent = true, int flags = 0) {
    if (!text) {
        text = "";
    }

    if (triggerEvent) {
        GameDisplayChatEventArgs args{};
        args.Msg = text;
        args.Flags = flags;
        args.Process = true;
        if (!detail::DispatchDisplayChat(args)) {
            return false;
        }
    }

    return ::CoreGame::Print(text, triggerEvent, static_cast<std::uint32_t>(flags));
}

template <typename... Args>
inline bool Print(const char* fmt, Args... args) {
    char buffer[1024] = {};
    if (fmt) {
        _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args...);
    }
    return Print(buffer, true);
}

inline bool Say(const char* text, bool sendToAll = false, bool triggerEvent = true) {
    if (!text) {
        text = "";
    }

    if (triggerEvent) {
        GameSendChatEventArgs args{};
        args.Msg = text;
        args.SendToAll = sendToAll;
        args.Process = true;
        if (!detail::DispatchSendChat(args)) {
            return false;
        }
    }

    return ::CoreGame::Say(text, sendToAll, triggerEvent);
}

template <typename... Args>
inline bool Say(const char* fmt, bool sendToAll, bool triggerEvent, Args... args) {
    char buffer[1024] = {};
    if (fmt) {
        _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args...);
    }
    return Say(buffer, sendToAll, triggerEvent);
}

inline bool SendPing(PingCategory pingType, const Vector2& position) {
    return ::CoreGame::SendPing(pingType, position, 0);
}

inline bool SendPing(PingCategory pingType, const Vector3& position) {
    return ::CoreGame::SendPing(pingType, position, 0);
}

inline bool SendPing(PingCategory pingType, const GameObject& target) {
    if (!target.IsValid()) {
        return false;
    }
    return ::CoreGame::SendPing(
        pingType,
        target.Position(),
        static_cast<std::uint32_t>(target.NetworkId()));
}

inline bool ShowPing(PingCategory pingType,
                     const Vector2& position,
                     bool playAudio = true,
                     bool showChat = true) {
    const AIHeroClient player = SDK::ObjectManager::Player();
    const auto sourceIndex = player.IsValid()
        ? static_cast<std::uint32_t>(player.Index())
        : 0u;
    return ::CoreGame::ShowPing(pingType, position, 0, sourceIndex, playAudio, showChat);
}

inline bool ShowPing(PingCategory pingType,
                     const Vector3& position,
                     bool playAudio = true,
                     bool showChat = true) {
    const AIHeroClient player = SDK::ObjectManager::Player();
    const auto sourceIndex = player.IsValid()
        ? static_cast<std::uint32_t>(player.Index())
        : 0u;
    return ::CoreGame::ShowPing(pingType, position, 0, sourceIndex, playAudio, showChat);
}

inline bool ShowPing(PingCategory pingType,
                     const GameObject& target,
                     bool playAudio = true,
                     bool showChat = true) {
    if (!target.IsValid()) {
        return false;
    }

    const AIHeroClient player = SDK::ObjectManager::Player();
    const auto sourceIndex = player.IsValid()
        ? static_cast<std::uint32_t>(player.Index())
        : 0u;
    return ::CoreGame::ShowPing(
        pingType,
        target.Position(),
        static_cast<std::uint32_t>(target.Index()),
        sourceIndex,
        playAudio,
        showChat);
}

inline bool ShowPing(PingCategory pingType,
                     const GameObject& source,
                     const GameObject& target,
                     bool playAudio = true,
                     bool showChat = true) {
    if (!source.IsValid() || !target.IsValid()) {
        return false;
    }
    return ::CoreGame::ShowPing(
        pingType,
        target.Position(),
        static_cast<std::uint32_t>(target.Index()),
        static_cast<std::uint32_t>(source.Index()),
        playAudio,
        showChat);
}

inline bool SendEmote(EmoteId emoteId) {
    return ::CoreGame::SendEmote(emoteId);
}

inline bool SendMasteryBadge() {
    return ::CoreGame::SendMasteryBadge();
}

inline bool SendSummonerEmote(SummonerEmoteSlot emoteSlot) {
    return ::CoreGame::SendSummonerEmote(emoteSlot);
}

struct UpdateEventSlot {
    bool operator+=(UpdateHandler handler) const { return AddOnUpdate(handler); }
    bool operator-=(UpdateHandler handler) const { return RemoveOnUpdate(handler); }
    bool operator()(UpdateHandler handler) const { return AddOnUpdate(handler); }
};

struct WndProcEventSlot {
    bool operator+=(WndProcHandler handler) const { return AddOnWndProc(handler); }
    bool operator-=(WndProcHandler handler) const { return RemoveOnWndProc(handler); }
    bool operator()(WndProcHandler handler) const { return AddOnWndProc(handler); }
};

struct SendChatEventSlot {
    bool operator+=(SendChatHandler handler) const { return AddOnSendChat(handler); }
    bool operator-=(SendChatHandler handler) const { return RemoveOnSendChat(handler); }
    bool operator()(SendChatHandler handler) const { return AddOnSendChat(handler); }
};

struct DisplayChatEventSlot {
    bool operator+=(DisplayChatHandler handler) const { return AddOnDisplayChat(handler); }
    bool operator-=(DisplayChatHandler handler) const { return RemoveOnDisplayChat(handler); }
    bool operator()(DisplayChatHandler handler) const { return AddOnDisplayChat(handler); }
};

inline UpdateEventSlot OnUpdate{};
inline WndProcEventSlot OnWndProc{};
inline SendChatEventSlot OnSendChat{};
inline DisplayChatEventSlot OnDisplayChat{};

} // namespace SDK::Game

namespace SDK::MenuGUI {
    inline bool IsChatOpen() { return SDK::Game::IsChatOpen(); }
    inline bool IsShopOpen() { return SDK::Game::IsShopOpen(); }
    inline bool IsOpenChat() { return SDK::Game::IsOpenChat(); }
    inline bool IsOpenShop() { return SDK::Game::IsOpenShop(); }
} // namespace SDK::MenuGUI
