#pragma once

#include "CoreBypass.h"
#include "CoreObjects.h"
#include "CoreRuntime.h"
#include "CoreValidation.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"
#include "spoof/spoofcall.h"
#include "../SectionProfiler.h"

#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace CoreControl {

// Disk-logging for IssueOrder is OFF by default. Previously AppendIssueOrderDebug()
// opened/wrote/closed a file (CreateFileA + WriteFile + CloseHandle) synchronously on
// the game/render thread every single time an order was blocked or failed, with NO
// throttling on those two paths (only the "ok" path was rate-limited to 2s). While
// Combo/Harass/LaneClear/LastHit (Space/C/V/X) is held, Orbwalk() attempts an
// Attack/Move order almost every tick, so any transient CanIssueOrder()/write-phase
// failure turned into dozens of blocking disk I/O calls per second — this is the
// actual cause of the 10-15 FPS drop, not CPU work. Flip this to true only for
// short debugging sessions.
inline constexpr bool kIssueOrderDebugEnabled = false;
// Even when the flag above is enabled for debugging, never write more than once per
// this many ms for a given message class, so enabling it can't reintroduce the stall.
inline constexpr DWORD kIssueOrderDebugThrottleMs = 1000;

inline constexpr float DefaultAttackDelaySeconds = 0.625f;
inline constexpr float DefaultAttackWindupSeconds = 0.300f;
inline constexpr int AttackWindupIdBase = 0x40;
inline constexpr int AttackWindupIdScanCount = 9;
inline constexpr uintptr_t AttackWindupProfileFirstOffset = 0x350;
inline constexpr uintptr_t AttackWindupProfileStride = sizeof(float);

enum OrderType : std::uint8_t {
    Hold = 1,
    MoveTo = 2,
    AttackUnit = 3,
    AutoAttackPet = 4,
    AutoAttack = 5,
    MovePet = 6,
    AttackMove = 7,
    Stop = 10,
};

using FnIssueOrder = void(__fastcall*)(
    uintptr_t unit,
    int order,
    Vec3* position,
    uintptr_t target,
    bool isAttackCommand,
    bool isPetCommand,
    bool triggerEvent);

inline bool IsFinite(float value) {
    return std::isfinite(value);
}

inline bool IsSaneSeconds(float value) {
    return IsFinite(value) && value > 0.0f && value < 10.0f;
}

inline bool IsSaneRadius(float value) {
    return IsFinite(value) && value >= 0.0f && value < 10000.0f;
}

inline bool IsSanePosition(const Vec3& value) {
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z) &&
           value.x > -30000.0f && value.x < 30000.0f &&
           value.y > -5000.0f && value.y < 5000.0f &&
           value.z > -30000.0f && value.z < 30000.0f &&
           !value.IsZero();
}

// `channel` groups messages that should share one throttle bucket (e.g. all "blocked"
// logs vs all "failed" logs), so a debugging session can't spam disk I/O even if
// kIssueOrderDebugEnabled is turned on.
inline void AppendIssueOrderDebug(const char* text, int channel = 0) {
    if (!kIssueOrderDebugEnabled || !text || !*text) {
        return;
    }

    static DWORD s_lastWriteTick[4] = {};
    const int slot = (channel >= 0 && channel < 4) ? channel : 0;
    const DWORD now = GetTickCount();
    if (s_lastWriteTick[slot] != 0 && now - s_lastWriteTick[slot] < kIssueOrderDebugThrottleMs) {
        return;
    }
    s_lastWriteTick[slot] = now;

    HANDLE hFile = CreateFileA(
        "C:\\Users\\Public\\ns_orbwalker_debug.txt",
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(hFile, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
    CloseHandle(hFile);
}

inline const char* OrderName(OrderType order) {
    switch (order) {
    case Hold: return "Hold";
    case MoveTo: return "MoveTo";
    case AttackUnit: return "AttackUnit";
    case AutoAttackPet: return "AutoAttackPet";
    case AutoAttack: return "AutoAttack";
    case MovePet: return "MovePet";
    case AttackMove: return "AttackMove";
    case Stop: return "Stop";
    default: return "Unknown";
    }
}

inline bool IsPetOrder(OrderType order) {
    return order == AutoAttackPet || order == MovePet;
}

inline bool NeedsTarget(OrderType order) {
    return order == AttackUnit || order == AutoAttack || order == AutoAttackPet;
}

inline bool IsAttackCommand(OrderType order) {
    return order == AttackUnit ||
           order == AutoAttack ||
           order == AutoAttackPet ||
           order == AttackMove;
}

inline uintptr_t LocalPlayerAddress() {
    CoreRuntime::RefreshReadState();
    return CoreRuntime::GetContext().localPlayer;
}

inline float GameTimeSeconds() {
    CoreRuntime::RefreshReadState();
    return CoreRuntime::GetContext().gameTime;
}

inline float GameTimeMs() {
    return GameTimeSeconds() * 1000.0f;
}

inline Vec3 ReadLivePosition(uintptr_t object) {
    return ::Core::Objects::ReadPosition(object);
}

inline Vec3 LocalPlayerPosition() {
    return ReadLivePosition(LocalPlayerAddress());
}

inline float GetBoundingRadius(uintptr_t object) {
    const float radius = ::Core::Objects::ReadBoundingRadius(object);
    return IsSaneRadius(radius) ? radius : 0.0f;
}

inline int GetPing() {
    auto& ctx = CoreRuntime::g_ctx;
    if (ctx.cachedPing > 0) {
        return ctx.cachedPing;
    }

    CoreRuntime::RefreshReadState();
    if (ctx.getPingFn && Globals::IsValidPtr(ctx.netInstance)) {
        using FnGetPing = int(__fastcall*)(uintptr_t);
        __try {
            const int ping = reinterpret_cast<FnGetPing>(ctx.getPingFn)(ctx.netInstance);
            if (ping > 0 && ping < 5000) {
                ctx.cachedPing = ping;
                return ping;
            }
        } __except (1) {}
    }

    return 30;
}

inline float ReadAttackDelayFor(uintptr_t object) {
    auto& ctx = CoreRuntime::g_ctx;
    if (!Globals::IsValidPtr(object) || !ctx.getAttackDelayFn) {
        return 0.0f;
    }

    using FnAttackDelay = float(__fastcall*)(uintptr_t);
    __try {
        const float value = reinterpret_cast<FnAttackDelay>(ctx.getAttackDelayFn)(object);
        return IsSaneSeconds(value) ? value : 0.0f;
    } __except (1) {
        return 0.0f;
    }
}

inline uintptr_t GetAttackWindupProfileRoot(uintptr_t object) {
    if (!Globals::IsValidPtr(object)) {
        return 0;
    }

    const auto vtable = Globals::Read<uintptr_t>(object);
    if (!Globals::IsValidPtr(vtable)) {
        return 0;
    }

    const auto profileFn = Globals::Read<uintptr_t>(vtable + 0x1F8);
    if (!Globals::IsValidPtr(profileFn)) {
        return 0;
    }

    using FnGetAttackProfile = uintptr_t(__fastcall*)(uintptr_t);
    __try {
        return reinterpret_cast<FnGetAttackProfile>(profileFn)(object);
    } __except (1) {
        return 0;
    }
}

inline int ResolveAttackWindupId(uintptr_t object) {
    const auto profileRoot = GetAttackWindupProfileRoot(object);
    if (!Globals::IsValidPtr(profileRoot)) {
        return AttackWindupIdBase;
    }

    for (int index = 0; index < AttackWindupIdScanCount; ++index) {
        const auto value = Globals::Read<float>(
            profileRoot + AttackWindupProfileFirstOffset +
            (static_cast<uintptr_t>(index) * AttackWindupProfileStride));
        if (value != 0.0f) {
            return AttackWindupIdBase + index;
        }
    }

    return AttackWindupIdBase;
}

inline float ReadAttackWindupFor(uintptr_t object, int attackId) {
    auto& ctx = CoreRuntime::g_ctx;
    if (!Globals::IsValidPtr(object) || !ctx.getAttackWindupFn) {
        return 0.0f;
    }

    using FnAttackWindup = float(__fastcall*)(uintptr_t, int);
    __try {
        const float value = reinterpret_cast<FnAttackWindup>(ctx.getAttackWindupFn)(object, attackId);
        return IsSaneSeconds(value) ? value : 0.0f;
    } __except (1) {
        return 0.0f;
    }
}

inline float ReadAttackWindupFor(uintptr_t object) {
    return ReadAttackWindupFor(object, ResolveAttackWindupId(object));
}

inline float GetAttackDelay() {
    auto& ctx = CoreRuntime::g_ctx;
    const float live = ReadAttackDelayFor(ctx.localPlayer);
    if (live > 0.0f) {
        ctx.cachedAttackDelay = live;
        return live;
    }
    return ctx.cachedAttackDelay > 0.0f ? ctx.cachedAttackDelay : DefaultAttackDelaySeconds;
}

inline float GetAttackDelay(uintptr_t object) {
    NS_PROFILE("cc.GetAttackDelay(obj)");
    const auto& ctx = CoreRuntime::GetContext();
    if (!Globals::IsValidPtr(object) || object == ctx.localPlayer) {
        return GetAttackDelay();
    }

    const float value = ReadAttackDelayFor(object);
    return value > 0.0f ? value : GetAttackDelay();
}

inline float GetAttackWindup() {
    auto& ctx = CoreRuntime::g_ctx;
    const float live = ReadAttackWindupFor(ctx.localPlayer);
    if (live > 0.0f) {
        ctx.cachedAttackWindup = live;
        return live;
    }
    return ctx.cachedAttackWindup > 0.0f ? ctx.cachedAttackWindup : DefaultAttackWindupSeconds;
}

inline float GetAttackWindup(uintptr_t object) {
    NS_PROFILE("cc.GetAttackWindup(obj)");
    const auto& ctx = CoreRuntime::GetContext();
    if (!Globals::IsValidPtr(object) || object == ctx.localPlayer) {
        return GetAttackWindup();
    }

    const float value = ReadAttackWindupFor(object);
    return value > 0.0f ? value : GetAttackWindup();
}

inline float GetAttackDelayMs(uintptr_t object = 0) {
    return GetAttackDelay(object ? object : CoreRuntime::GetContext().localPlayer) * 1000.0f;
}

inline float GetAttackWindupMs(uintptr_t object = 0) {
    return GetAttackWindup(object ? object : CoreRuntime::GetContext().localPlayer) * 1000.0f;
}

inline float ReadAttackDelayNow() {
    return ReadAttackDelayFor(CoreRuntime::GetContext().localPlayer);
}

inline float ReadAttackWindupNow() {
    return ReadAttackWindupFor(CoreRuntime::GetContext().localPlayer);
}

inline bool CanIssueOrder() {
    const auto mask = CoreValidation::GetMask();
    const auto& ctx = CoreRuntime::GetContext();
    constexpr std::uint32_t needed =
        CoreValidation::Validation_IssueOrder |
        CoreValidation::Validation_LocalPlayer |
        CoreValidation::Validation_InputReady |
        CoreValidation::Validation_SpoofTrampoline;

    return (mask & needed) == needed &&
           Globals::IsValidPtr(ctx.localPlayer) &&
           Globals::IsValidPtr(ctx.issueOrderFn) &&
           Globals::IsValidPtr(ctx.spoofTrampoline);
}

inline bool PatchHudAttackInput(const Vec3& position,
                                uintptr_t target,
                                uintptr_t& hudInput,
                                Vec3& originalMousePos,
                                std::uint32_t& originalSelectedNetId) {
    const auto& ctx = CoreRuntime::GetContext();
    if (!Globals::IsValidPtr(target) || !Globals::IsValidPtr(ctx.hudInstance)) {
        return false;
    }

    hudInput = Globals::Read<uintptr_t>(ctx.hudInstance + Offset::HudRuntime::Input);
    if (!Globals::IsValidPtr(hudInput)) {
        return false;
    }

    originalMousePos = Globals::Read<Vec3>(hudInput + Offset::HudRuntime::MouseWorldPos);
    originalSelectedNetId =
        Globals::Read<std::uint32_t>(hudInput + Offset::HudInputLayout::SelectedObjNetId);

    const std::uint32_t targetNetId =
        Globals::Read<std::uint32_t>(target + Offset::All::NetId);
    if (targetNetId == 0 || targetNetId == 0xFFFFFFFFu) {
        hudInput = 0;
        return false;
    }

    const bool wroteMouse =
        Globals::Write<Vec3>(hudInput + Offset::HudRuntime::MouseWorldPos, position);
    const bool wroteSelected = Globals::Write<std::uint32_t>(
        hudInput + Offset::HudInputLayout::SelectedObjNetId,
        targetNetId);
    return wroteMouse && wroteSelected;
}

inline void RestoreHudAttackInput(uintptr_t hudInput,
                                  const Vec3& originalMousePos,
                                  std::uint32_t originalSelectedNetId) {
    if (!Globals::IsValidPtr(hudInput)) {
        return;
    }

    Globals::Write<Vec3>(hudInput + Offset::HudRuntime::MouseWorldPos, originalMousePos);
    Globals::Write<std::uint32_t>(
        hudInput + Offset::HudInputLayout::SelectedObjNetId,
        originalSelectedNetId);
}

inline bool IssueOrder(OrderType order,
                       const Vec3& requestedPosition,
                       uintptr_t target = 0,
                       bool triggerEvent = true) {
    auto& ctx = CoreRuntime::g_ctx;
    if (!CoreRuntime::IsWritePhase() || !CanIssueOrder()) {
        // This is the hot path while an orbwalking key (Space/C/V/X) is held: Orbwalk()
        // tries to issue an order almost every tick, and any transient write-phase /
        // validation miss used to land here. Building+writing a debug string on every
        // such miss (formerly unconditionally) is what caused the FPS drop, so the
        // string is only ever built when debugging is explicitly enabled.
        if constexpr (kIssueOrderDebugEnabled) {
            char buf[384] = {};
            std::snprintf(
                buf,
                sizeof(buf),
                "[NightSharp][IssueOrder] blocked order=%u name=%s phase=%u validation=0x%08X local=0x%llX fn=0x%llX spoof=0x%llX target=0x%llX pos=%.1f %.1f %.1f\r\n",
                static_cast<unsigned>(order),
                OrderName(order),
                ctx.currentPhase,
                ctx.validationMask,
                static_cast<unsigned long long>(ctx.localPlayer),
                static_cast<unsigned long long>(ctx.issueOrderFn),
                static_cast<unsigned long long>(ctx.spoofTrampoline),
                static_cast<unsigned long long>(target),
                requestedPosition.x,
                requestedPosition.y,
                requestedPosition.z);
            AppendIssueOrderDebug(buf, /*channel=*/0);
        }
        CoreValidation::MarkIssueOrderResult(false);
        return false;
    }

    if (NeedsTarget(order) && !Globals::IsValidPtr(target)) {
        if constexpr (kIssueOrderDebugEnabled) {
            AppendIssueOrderDebug("[NightSharp][IssueOrder] blocked missing target\r\n", /*channel=*/0);
        }
        CoreValidation::MarkIssueOrderResult(false);
        return false;
    }

    Vec3 position = requestedPosition;
    if (NeedsTarget(order) && Globals::IsValidPtr(target)) {
        const Vec3 targetPosition = ReadLivePosition(target);
        if (IsSanePosition(targetPosition)) {
            position = targetPosition;
        }
    }
    if (!IsSanePosition(position)) {
        position = ReadLivePosition(ctx.localPlayer);
    }

    const bool isAttack = IsAttackCommand(order);
    const bool isPet = IsPetOrder(order);
    bool ok = false;
    bool bypassTouched = false;
    bool patchedHudInput = false;
    uintptr_t hudInput = 0;
    Vec3 originalMousePos = {};
    std::uint32_t originalSelectedNetId = 0;

    __try {
        if (isAttack && NeedsTarget(order)) {
            patchedHudInput = PatchHudAttackInput(
                position,
                target,
                hudInput,
                originalMousePos,
                originalSelectedNetId);
        }

        CoreBypass::PrepareIssueOrder(static_cast<std::uint8_t>(order));
        bypassTouched = true;

        spoof_call(
            reinterpret_cast<void*>(ctx.spoofTrampoline),
            reinterpret_cast<FnIssueOrder>(ctx.issueOrderFn),
            ctx.localPlayer,
            static_cast<int>(order),
            &position,
            target,
            isAttack,
            isPet,
            triggerEvent);
        ok = true;
    } __except (1) {
        ok = false;
    }

    if (patchedHudInput) {
        RestoreHudAttackInput(hudInput, originalMousePos, originalSelectedNetId);
    }

    if (bypassTouched) {
        __try {
            CoreBypass::FinalizeIssueOrder(static_cast<std::uint8_t>(order));
        } __except (1) {}
    }

    if (!ok) {
        if constexpr (kIssueOrderDebugEnabled) {
            char buf[384] = {};
            std::snprintf(
                buf,
                sizeof(buf),
                "[NightSharp][IssueOrder] failed order=%u name=%s phase=%u validation=0x%08X local=0x%llX fn=0x%llX spoof=0x%llX target=0x%llX pos=%.1f %.1f %.1f\r\n",
                static_cast<unsigned>(order),
                OrderName(order),
                ctx.currentPhase,
                ctx.validationMask,
                static_cast<unsigned long long>(ctx.localPlayer),
                static_cast<unsigned long long>(ctx.issueOrderFn),
                static_cast<unsigned long long>(ctx.spoofTrampoline),
                static_cast<unsigned long long>(target),
                position.x,
                position.y,
                position.z);
            AppendIssueOrderDebug(buf, /*channel=*/1);
        }
    } else if (NeedsTarget(order)) {
        if constexpr (kIssueOrderDebugEnabled) {
            char buf[384] = {};
            std::snprintf(
                buf,
                sizeof(buf),
                "[NightSharp][IssueOrder] ok order=%u name=%s phase=%u validation=0x%08X local=0x%llX target=0x%llX pos=%.1f %.1f %.1f hudPatch=%d\r\n",
                static_cast<unsigned>(order),
                OrderName(order),
                ctx.currentPhase,
                ctx.validationMask,
                static_cast<unsigned long long>(ctx.localPlayer),
                static_cast<unsigned long long>(target),
                position.x,
                position.y,
                position.z,
                patchedHudInput ? 1 : 0);
            AppendIssueOrderDebug(buf, /*channel=*/2);
        }
    }

    CoreValidation::MarkIssueOrderResult(ok);
    return ok;
}

inline bool MoveToPos(const Vec3& position, bool triggerEvent = true) {
    return IssueOrder(MoveTo, position, 0, triggerEvent);
}

inline bool AttackObject(uintptr_t target,
                         const Vec3& fallbackPosition,
                         bool triggerEvent = true) {
    return IssueOrder(AttackUnit, fallbackPosition, target, triggerEvent);
}

inline bool AttackMoveTo(const Vec3& position, bool triggerEvent = true) {
    return IssueOrder(AttackMove, position, 0, triggerEvent);
}

inline bool IssueMove(const Vec3& position, bool triggerEvent = true) {
    NS_PROFILE("cc.IssueMove");
    return MoveToPos(position, triggerEvent);
}

inline bool IssueAttackMove(const Vec3& position, bool triggerEvent = true) {
    NS_PROFILE("cc.IssueAttackMove");
    return AttackMoveTo(position, triggerEvent);
}

inline bool IssueAttack(uintptr_t target,
                        const Vec3& fallbackPosition,
                        bool triggerEvent = true) {
    NS_PROFILE("cc.IssueAttack");
    return AttackObject(target, fallbackPosition, triggerEvent);
}

inline bool HoldPosition(bool triggerEvent = true) {
    NS_PROFILE("cc.HoldPosition");
    return IssueOrder(Hold, LocalPlayerPosition(), 0, triggerEvent);
}

inline bool StopMoving(bool triggerEvent = true) {
    NS_PROFILE("cc.StopMoving");
    return IssueOrder(Stop, LocalPlayerPosition(), 0, triggerEvent);
}

} // namespace CoreControl