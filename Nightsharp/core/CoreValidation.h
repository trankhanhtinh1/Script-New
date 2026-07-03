#pragma once

#include "CoreBypass.h"
#include "CoreRuntime.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"

#include <Windows.h>
#include <cmath>
#include <cstdint>

namespace CoreValidation {

    enum Flags : uint32_t {
        Validation_None                  = 0,
        Validation_LocalPlayer           = 1u << 0,
        Validation_DetectionWatcher      = 1u << 1,
        Validation_SpoofTrampoline       = 1u << 2,
        Validation_ViewProjection        = 1u << 3,
        Validation_Renderer              = 1u << 4,
        Validation_Ping                  = 1u << 5,
        Validation_AttackDelay           = 1u << 6,
        Validation_AttackWindupEstimated = 1u << 7,
        Validation_IssueOrder            = 1u << 8,
        Validation_SpellBook             = 1u << 9,
        Validation_Buffs                 = 1u << 10,
        Validation_CastHelperPresent     = 1u << 12,
        Validation_HudSpellInfo          = 1u << 13,
        Validation_ObjectManagers        = 1u << 14,
        Validation_ObjectEnumeration     = 1u << 15,
        Validation_CastCallableApproved  = 1u << 16,
        Validation_NavGrid               = 1u << 17,
        Validation_InputReady            = 1u << 18,
    };

    inline bool ReadViewProjection(float out[16]) {
        if (!out) {
            return false;
        }

        const auto instance = CoreRuntime::GetContext().viewProjInstance;
        if (!Globals::IsValidPtr(instance)) {
            return false;
        }

        __try {
            for (int i = 0; i < 16; ++i) {
                const float view = Globals::Read<float>(
                    instance + static_cast<uintptr_t>(i) * sizeof(float));
                const float projection = Globals::Read<float>(
                    instance + Offset::DrawingMatrixRuntime::ProjMatrixRelative +
                    static_cast<uintptr_t>(i) * sizeof(float));
                if (!std::isfinite(view) || !std::isfinite(projection)) {
                    return false;
                }
                out[i] = view;
            }
            return true;
        }
        __except (1) {
            return false;
        }
    }

    inline bool GetRendererSize(Vec2& out) {
        out = {};
        const auto renderer = CoreRuntime::GetContext().renderer;
        if (!Globals::IsValidPtr(renderer)) {
            return false;
        }

        __try {
            const int width = Globals::Read<int>(renderer + 0xC);
            const int height = Globals::Read<int>(renderer + 0x10);
            if (width > 0 && height > 0 && width < 20000 && height < 20000) {
                out = { static_cast<float>(width), static_cast<float>(height) };
                return true;
            }
        }
        __except (1) {}
        return false;
    }

    inline bool IsChatOpen() {
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
            const auto inputActive = Globals::Read<std::uint8_t>(
                chatView + Offset::ChatViewControllerLayout::InputActive);

            if (inputActive > 1) {
                return false;
            }
            return inputActive != 0;
        }
        __except (1) {
            return false;
        }
    }

    inline bool IsOpenChat() {
        return IsChatOpen();
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

    inline bool IsOpenShop() {
        return IsShopOpen();
    }

    inline bool ShouldProcessInput() {
        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(ctx.localPlayer) ||
            !Globals::IsValidPtr(ctx.objectManager) ||
            ctx.gameTime <= 0.0f) {
            return false;
        }

        const HWND foreground = GetForegroundWindow();
        if (foreground) {
            DWORD processId = 0;
            GetWindowThreadProcessId(foreground, &processId);
            if (processId != GetCurrentProcessId()) {
                return false;
            }
        }

        return !IsChatOpen() && !IsShopOpen();
    }

    inline uintptr_t GetHudSpellInfo() {
        const auto hud = CoreRuntime::GetContext().hudInstance;
        return Globals::IsValidPtr(hud)
            ? Globals::Read<uintptr_t>(hud + Offset::HudRuntime::SpellInfo)
            : 0;
    }

    inline void MarkIssueOrderResult(bool ok) {
        ++CoreRuntime::g_ctx.issueOrderAttempts;
        if (ok) {
            ++CoreRuntime::g_ctx.issueOrderSuccesses;
            CoreRuntime::g_ctx.lastIssueOrderResult = 1;
        } else {
            CoreRuntime::g_ctx.lastIssueOrderResult = -1;
        }
    }

    inline void MarkCastResult(bool ok) {
        ++CoreRuntime::g_ctx.castAttempts;
        if (ok) {
            ++CoreRuntime::g_ctx.castSuccesses;
            CoreRuntime::g_ctx.lastCastResult = 1;
        } else {
            CoreRuntime::g_ctx.lastCastResult = -1;
        }
    }

    inline uint32_t Refresh() {
        auto& ctx = CoreRuntime::g_ctx;
        static uint32_t s_cachedMask = Validation_None;
        static DWORD s_lastRefresh = 0;

        const DWORD now = GetTickCount();
        if (s_lastRefresh != 0 && now - s_lastRefresh < 50) {
            ctx.validationMask = s_cachedMask;
            return ctx.validationMask;
        }
        s_lastRefresh = now;

        CoreRuntime::EnsureInitialized();
        CoreRuntime::RefreshReadState();
        ctx.validationMask = Validation_None;

        if (Globals::IsValidPtr(ctx.localPlayer)) {
            ctx.validationMask |= Validation_LocalPlayer;
        }

        __try {
            if (Globals::IsValidPtr(CoreBypass::ResolveDetectionWatcher2())) {
                ctx.validationMask |= Validation_DetectionWatcher;
            }
        }
        __except (1) {}

        __try {
            if (Globals::IsValidPtr(CoreBypass::ResolveSpoofTrampoline())) {
                ctx.validationMask |= Validation_SpoofTrampoline;
            }
        }
        __except (1) {}

        float viewProjection[16] = {};
        if (ReadViewProjection(viewProjection)) {
            ctx.validationMask |= Validation_ViewProjection;
        }

        Vec2 rendererSize = {};
        if (GetRendererSize(rendererSize)) {
            ctx.validationMask |= Validation_Renderer;
        }

        if (ctx.getPingFn && Globals::IsValidPtr(ctx.netInstance)) {
            ctx.validationMask |= Validation_Ping;
        }

        if (ctx.getAttackDelayFn && Globals::IsValidPtr(ctx.localPlayer)) {
            ctx.validationMask |= Validation_AttackDelay;
        }

        if (ctx.getAttackWindupFn && Globals::IsValidPtr(ctx.localPlayer)) {
            ctx.validationMask |= Validation_AttackWindupEstimated;
        }

        if (Globals::IsValidPtr(ctx.heroManager) &&
            Globals::IsValidPtr(ctx.minionManager) &&
            Globals::IsValidPtr(ctx.turretManager) &&
            Globals::IsValidPtr(ctx.missileManager)) {
            ctx.validationMask |= Validation_ObjectManagers;
        }

        if (Globals::IsValidPtr(ctx.navGrid)) {
            ctx.validationMask |= Validation_NavGrid;
        }

        __try {
            if (Globals::IsValidPtr(ctx.hudInstance)) {
                if (Globals::IsValidPtr(GetHudSpellInfo())) {
                    ctx.validationMask |= Validation_HudSpellInfo;
                }
                const auto hudInput = Globals::Read<uintptr_t>(ctx.hudInstance + Offset::HudRuntime::Input);
                if (Globals::IsValidPtr(hudInput) &&
                    ShouldProcessInput()) {
                    ctx.validationMask |= Validation_InputReady;
                }
            }
        }
        __except (1) {}

        constexpr uint32_t issueOrderDeps =
            Validation_DetectionWatcher |
            Validation_SpoofTrampoline;
        if ((ctx.validationMask & issueOrderDeps) == issueOrderDeps && ctx.issueOrderFn) {
            ctx.validationMask |= Validation_IssueOrder;
        }

        s_cachedMask = ctx.validationMask;
        return ctx.validationMask;
    }

    inline uint32_t GetMask() {
        return Refresh();
    }

} // namespace CoreValidation
