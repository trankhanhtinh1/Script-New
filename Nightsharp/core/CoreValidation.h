#pragma once

#include "CrashTelemetry.h"
#include "CoreBypass.h"
#include "CoreObjects.h"
#include "CoreSpellBook.h"
#include "CoreAi.h"
#include "CoreBuffs.h"
#include "CoreGame.h"
#include "CoreNavGrid.h"
#include "CoreView.h"

namespace CoreValidation {

    enum Flags : uint32_t {
        Validation_None                   = 0,
        Validation_LocalPlayer            = 1u << 0,
        Validation_DetectionWatcher       = 1u << 1,
        Validation_SpoofTrampoline        = 1u << 2,
        Validation_ViewProjection         = 1u << 3,
        Validation_Renderer               = 1u << 4,
        Validation_Ping                   = 1u << 5,
        Validation_AttackDelay            = 1u << 6,
        Validation_AttackWindupEstimated  = 1u << 7,
        Validation_IssueOrder             = 1u << 8,
        Validation_SpellBook              = 1u << 9,
        Validation_Buffs                  = 1u << 10,
        Validation_AiManager              = 1u << 11,
        Validation_CastHelperPresent      = 1u << 12,
        Validation_HudSpellInfo           = 1u << 13,
        Validation_ObjectManagers         = 1u << 14,
        Validation_ObjectEnumeration      = 1u << 15,
        Validation_CastCallableApproved   = 1u << 16,
        Validation_NavGrid                = 1u << 17,
        Validation_InputReady             = 1u << 18
    };

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

    inline void TraceStage(const char* stage) {
        CrashTelemetry::SetStage(stage);
        if (CoreRuntime::g_ctx.refreshGeneration <= 2 && stage && *stage) {
            char buffer[256] = {};
            std::snprintf(buffer, sizeof(buffer), "[NightSharp] %s\r\n", stage);
            CrashTelemetry::AppendStageLine(buffer);
        }
    }

    inline uint32_t Refresh() {
        auto& ctx = CoreRuntime::g_ctx;
        ctx.validationMask = Validation_None;

        TraceStage("CoreValidation::Refresh::LocalPlayer");
        if (Globals::IsValidPtr(ctx.localPlayer)) {
            ctx.validationMask |= Validation_LocalPlayer;
        }

        TraceStage("CoreValidation::Refresh::DetectionWatcher");
        __try {
            if (Globals::IsValidPtr(CoreBypass::ResolveDetectionWatcher2())) {
                ctx.validationMask |= Validation_DetectionWatcher;
            }
        } __except (1) {
        }

        TraceStage("CoreValidation::Refresh::SpoofTrampoline");
        __try {
            if (Globals::IsValidPtr(CoreBypass::ResolveSpoofTrampoline())) {
                ctx.validationMask |= Validation_SpoofTrampoline;
            }
        } __except (1) {
        }

        TraceStage("CoreValidation::Refresh::ViewProjection");
        __try {
            float matrix[16] = {};
            if (CoreView::ReadViewProjection(matrix)) {
                ctx.validationMask |= Validation_ViewProjection;
            }
        } __except (1) {
        }

        TraceStage("CoreValidation::Refresh::Renderer");
        __try {
            Vec2 rendererSize = {};
            if (CoreView::GetRendererSize(rendererSize)) {
                ctx.validationMask |= Validation_Renderer;
            }
        } __except (1) {
        }

        TraceStage("CoreValidation::Refresh::HudSpellInfo");
        __try {
            if (Globals::IsValidPtr(CoreView::GetHudSpellInfo())) {
                ctx.validationMask |= Validation_HudSpellInfo;
            }
        } __except (1) {
        }

        TraceStage("CoreValidation::Refresh::CastHelper");
        if (ctx.castSpellFn) {
            ctx.validationMask |= Validation_CastHelperPresent;
        }

        TraceStage("CoreValidation::Refresh::InputReady");
        __try {
            if (CoreGame::ShouldProcessInput()) {
                ctx.validationMask |= Validation_InputReady;
            }
        } __except (1) {
        }

        TraceStage("CoreValidation::Refresh::Ping");
        if (ctx.getPingFn && Globals::IsValidPtr(ctx.netInstance)) {
            ctx.validationMask |= Validation_Ping;
        }

        TraceStage("CoreValidation::Refresh::AttackDelay");
        if (ctx.getAttackDelayFn && Globals::IsValidPtr(ctx.localPlayer)) {
            ctx.validationMask |= Validation_AttackDelay;
        }

        TraceStage("CoreValidation::Refresh::AttackWindup");
        if (ctx.getAttackWindupFn && Globals::IsValidPtr(ctx.localPlayer)) {
            ctx.validationMask |= Validation_AttackWindupEstimated;
        }

        TraceStage("CoreValidation::Refresh::SpellBook");
        __try {
            auto slotQ = CoreSpellBook::GetSlot(ctx.localPlayer, CoreSpellBook::Slot_Q);
            if (slotQ.IsValid() && Globals::IsValidPtr(slotQ.GetSpellInput()) && Globals::IsValidPtr(slotQ.GetSpellInfo())) {
                ctx.validationMask |= Validation_SpellBook;
            }
        } __except (1) {
        }

        TraceStage("CoreValidation::Refresh::ObjectManagers");
        if (Globals::IsValidPtr(ctx.heroManager) &&
            Globals::IsValidPtr(ctx.minionManager) &&
            Globals::IsValidPtr(ctx.turretManager) &&
            Globals::IsValidPtr(ctx.missileManager)) {
            ctx.validationMask |= Validation_ObjectManagers;
        }

        TraceStage("CoreValidation::Refresh::NavGrid");
        __try {
            if (CoreNavGrid::Get().IsValid()) {
                ctx.validationMask |= Validation_NavGrid;
            }
        } __except (1) {
        }

        TraceStage("CoreValidation::Refresh::Buffs");
        __try {
            uintptr_t buffs[64] = {};
            if (CoreBuffs::Enumerate(ctx.localPlayer, buffs, 64) >= 0) {
                ctx.validationMask |= Validation_Buffs;
            }
        } __except (1) {
        }

        TraceStage("CoreValidation::Refresh::ObjectEnumeration");
        __try {
            uintptr_t objects[8] = {};
            if (CoreObjects::EnumerateAllObjects(objects, static_cast<int>(sizeof(objects) / sizeof(objects[0]))) >= 0) {
                ctx.validationMask |= Validation_ObjectEnumeration;
            }
        } __except (1) {
        }

        TraceStage("CoreValidation::Refresh::AiManager");
        __try {
            auto ai = CoreAi::Get(ctx.localPlayer);
            if (ai.IsValid()) {
                ctx.validationMask |= Validation_AiManager;
            }
        } __except (1) {
        }

        TraceStage("CoreValidation::Refresh::IssueOrderReady");
        if ((ctx.validationMask & (Validation_SpoofTrampoline | Validation_DetectionWatcher)) ==
            (Validation_SpoofTrampoline | Validation_DetectionWatcher) &&
            ctx.issueOrderFn) {
            ctx.validationMask |= Validation_IssueOrder;
        }

        TraceStage("CoreValidation::Refresh::CastCallableApproved");
        if ((ctx.validationMask & (Validation_SpoofTrampoline |
                                   Validation_DetectionWatcher |
                                   Validation_SpellBook |
                                   Validation_HudSpellInfo)) ==
            (Validation_SpoofTrampoline |
             Validation_DetectionWatcher |
             Validation_SpellBook |
             Validation_HudSpellInfo) &&
            ctx.castSpellFn) {
            ctx.validationMask |= Validation_CastCallableApproved;
        }

        TraceStage("CoreValidation::Refresh::Done");
        return ctx.validationMask;
    }

    inline uint32_t GetMask() {
        return CoreRuntime::GetContext().validationMask;
    }

} // namespace CoreValidation
