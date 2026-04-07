#pragma once

#include "CoreBypass.h"
#include "CoreGame.h"
#include "CoreObjects.h"
#include "CoreSpellBook.h"
#include "CoreView.h"
#include "CoreValidation.h"
#include <cstdio>
#include <xmmintrin.h>

namespace CoreControl {

    inline constexpr int AttackWindupIdBase = 0x40;
    inline constexpr int AttackWindupIdScanCount = 9;
    inline constexpr uintptr_t AttackWindupProfileFirstOffset = 0x350;
    inline constexpr uintptr_t AttackWindupProfileStride = sizeof(float);
    inline constexpr int CastSpellModeNormal = 1;
    inline constexpr int CastSpellModeSmart = 2;
    inline constexpr int CastSpellPhasePress = 1;
    // BB8A20 is the legacy target-commit stub the old write-path still calls.
    // The real CastSpellSafe body lives elsewhere and is tracked by the generator.
    inline constexpr uintptr_t CastSpellTargetCommitRva = 0xBB8A20;
    inline constexpr uintptr_t CastSpellTargetFollowRva = 0xBBB7D0;
    inline constexpr uintptr_t GameObjectSpellDataOffset = 0x60;
    inline constexpr uintptr_t GameObjectTargetFollowFlagOffset = 0x3BF;

    inline void AppendIssueOrderDebug(const char* text) {
        if (!text || !*text) {
            return;
        }

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
        WriteFile(hFile, text, (DWORD)lstrlenA(text), &written, nullptr);
        CloseHandle(hFile);
    }

    enum OrderType : uint8_t {
        Hold = 1,
        MoveTo = 2,
        AttackUnit = 3,
        AutoAttackPet = 4,
        AutoAttack = 5,
        MovePet = 6,
        AttackMove = 7,
        Stop = 10
    };

    inline int GetPing() {
        // cachedPing is never populated, read from game memory
        const auto& ctx = CoreRuntime::GetContext();
        if (ctx.cachedPing > 0) return ctx.cachedPing;
        // Fallback: return a reasonable default
        return 30;
    }

    inline float ReadAttackDelayFor(uintptr_t object) {
        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(object) || !ctx.getAttackDelayFn) {
            return 0.0f;
        }

        using fnAttackDelay = float(__cdecl*)(uintptr_t);
        __try {
            const float value = reinterpret_cast<fnAttackDelay>(ctx.getAttackDelayFn)(object);
            return (value > 0.0f && value < 10.0f) ? value : 0.0f;
        }
        __except (1) {
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

        using fnGetAttackProfile = uintptr_t(__fastcall*)(uintptr_t);
        __try {
            return reinterpret_cast<fnGetAttackProfile>(profileFn)(object);
        }
        __except (1) {
            return 0;
        }
    }

    inline int ResolveAttackWindupId(uintptr_t object) {
        // MCP IDA shows sub_53A380 consumes an attack-id in the 0x40..0x51 range,
        // and the client's generic selector (sub_28BCE0) resolves the default/basic
        // entry by scanning the first non-zero profile weight at root+0x350..0x370.
        const auto profileRoot = GetAttackWindupProfileRoot(object);
        if (!Globals::IsValidPtr(profileRoot)) {
            return AttackWindupIdBase;
        }

        for (int index = 0; index < AttackWindupIdScanCount; ++index) {
            const auto value = Globals::Read<float>(
                profileRoot + AttackWindupProfileFirstOffset + (static_cast<uintptr_t>(index) * AttackWindupProfileStride));
            if (value != 0.0f) {
                return AttackWindupIdBase + index;
            }
        }

        return AttackWindupIdBase;
    }

    inline float ReadAttackWindupFor(uintptr_t object, int attackId) {
        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(object) || !ctx.getAttackWindupFn) {
            return 0.0f;
        }

        using fnAttackWindup = float(__cdecl*)(uintptr_t, int);
        __try {
            const float value = reinterpret_cast<fnAttackWindup>(ctx.getAttackWindupFn)(object, attackId);
            return (value > 0.0f && value < 10.0f) ? value : 0.0f;
        }
        __except (1) {
            return 0.0f;
        }
    }

    inline float ReadAttackWindupFor(uintptr_t object) {
        return ReadAttackWindupFor(object, ResolveAttackWindupId(object));
    }

    inline float GetAttackDelay() {
        // Call the actual game function to get real attack delay.
        const float live = ReadAttackDelayFor(CoreRuntime::GetContext().localPlayer);
        return live > 0.0f ? live : 0.625f; // fallback: typical AA delay
    }

    inline float GetAttackWindup() {
        // Call the actual game function to get real windup.
        const float live = ReadAttackWindupFor(CoreRuntime::GetContext().localPlayer);
        return live > 0.0f ? live : 0.3f; // fallback: typical windup
    }

    inline float GetAttackDelay(uintptr_t object) {
        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(object) || object == ctx.localPlayer) {
            return GetAttackDelay();
        }

        const float value = ReadAttackDelayFor(object);
        return value > 0.0f ? value : GetAttackDelay();
    }

    inline float GetAttackWindup(uintptr_t object) {
        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(object) || object == ctx.localPlayer) {
            return GetAttackWindup();
        }

        const float value = ReadAttackWindupFor(object);
        return value > 0.0f ? value : GetAttackWindup();
    }

    inline float ReadAttackDelayNow() {
        return ReadAttackDelayFor(CoreRuntime::GetContext().localPlayer);
    }

    inline float ReadAttackWindupNow() {
        return ReadAttackWindupFor(CoreRuntime::GetContext().localPlayer);
    }

    inline bool CanIssueOrder() {
        const auto mask = CoreValidation::GetMask();
        return (mask & (CoreValidation::Validation_IssueOrder |
                        CoreValidation::Validation_LocalPlayer |
                        CoreValidation::Validation_InputReady |
                        CoreValidation::Validation_SpoofTrampoline)) ==
               (CoreValidation::Validation_IssueOrder |
                CoreValidation::Validation_LocalPlayer |
                CoreValidation::Validation_InputReady |
                 CoreValidation::Validation_SpoofTrampoline);
    }

    inline bool CanCastSpell() {
        const auto mask = CoreValidation::GetMask();
        const bool validationReady =
            (mask & (CoreValidation::Validation_LocalPlayer |
                     CoreValidation::Validation_InputReady |
                     CoreValidation::Validation_SpoofTrampoline |
                     CoreValidation::Validation_SpellBook |
                     CoreValidation::Validation_HudSpellInfo |
                     CoreValidation::Validation_CastCallableApproved |
                     CoreValidation::Validation_CastHelperPresent)) ==
            (CoreValidation::Validation_LocalPlayer |
             CoreValidation::Validation_InputReady |
             CoreValidation::Validation_SpoofTrampoline |
             CoreValidation::Validation_SpellBook |
             CoreValidation::Validation_HudSpellInfo |
             CoreValidation::Validation_CastCallableApproved |
             CoreValidation::Validation_CastHelperPresent);
        return validationReady && CoreRuntime::GetContext().castSpellFn != 0;
    }

    inline bool CanUpdateChargedSpell() {
        return CanCastSpell() && CoreRuntime::GetContext().updateChargedSpellFn != 0;
    }

    inline bool IssueOrder(OrderType order, const Vec3& position, uintptr_t target = 0, bool primaryFlag = false, bool secondaryFlag = false, bool tertiaryFlag = false) {
        auto& ctx = CoreRuntime::g_ctx;
        if (!CoreRuntime::IsWritePhase() || !CanIssueOrder()) {
            char buf[320] = {};
            std::snprintf(
                buf, sizeof(buf),
                "[NightSharp][IssueOrder] blocked order=%d phase=%u canIssue=%d validation=0x%08X local=0x%llX fn=0x%llX spoof=0x%llX flags=%d/%d/%d pos=%.1f %.1f %.1f target=0x%llX\r\n",
                (int)order,
                ctx.currentPhase,
                CanIssueOrder() ? 1 : 0,
                ctx.validationMask,
                (unsigned long long)ctx.localPlayer,
                (unsigned long long)ctx.issueOrderFn,
                (unsigned long long)ctx.spoofTrampoline,
                primaryFlag ? 1 : 0,
                secondaryFlag ? 1 : 0,
                tertiaryFlag ? 1 : 0,
                position.x, position.y, position.z,
                (unsigned long long)target);
            AppendIssueOrderDebug(buf);
            CoreValidation::MarkIssueOrderResult(false);
            return false;
        }

        Vec3 pos = position;
        using fnIssueOrderLegacy = int64_t(__cdecl*)(uintptr_t, int, Vec3*, uintptr_t, bool, bool);

        bool ok = false;
        __try {
            const bool isAttack = (order == OrderType::AttackUnit);
            (void)primaryFlag;
            (void)secondaryFlag;
            (void)tertiaryFlag;

            CoreBypass::PrepareIssueOrder(static_cast<uint8_t>(order));
            const auto result = spoof_call(
                reinterpret_cast<void*>(ctx.spoofTrampoline),
                reinterpret_cast<fnIssueOrderLegacy>(ctx.issueOrderFn),
                ctx.localPlayer,
                static_cast<int>(order),
                &pos,
                target,
                isAttack,
                false);
            ok = true;
            (void)result;
        }
        __except (1) {
            ok = false;
        }

        if (!ok) {
            char buf[320] = {};
            std::snprintf(
                buf, sizeof(buf),
                "[NightSharp][IssueOrder] failed order=%d phase=%u validation=0x%08X local=0x%llX fn=0x%llX spoof=0x%llX flags=%d/%d/%d pos=%.1f %.1f %.1f target=0x%llX\r\n",
                (int)order,
                ctx.currentPhase,
                ctx.validationMask,
                (unsigned long long)ctx.localPlayer,
                (unsigned long long)ctx.issueOrderFn,
                (unsigned long long)ctx.spoofTrampoline,
                primaryFlag ? 1 : 0,
                secondaryFlag ? 1 : 0,
                tertiaryFlag ? 1 : 0,
                position.x, position.y, position.z,
                (unsigned long long)target);
            AppendIssueOrderDebug(buf);
        }
        else if (order == OrderType::AttackUnit) {
            static DWORD s_lastOkLog = 0;
            const DWORD okNow = GetTickCount();
            if ((okNow - s_lastOkLog) > 2000) {
                s_lastOkLog = okNow;
                char buf[320] = {};
                std::snprintf(
                    buf, sizeof(buf),
                    "[NightSharp][IssueOrder] ok order=%d phase=%u validation=0x%08X local=0x%llX fn=0x%llX spoof=0x%llX pos=%.1f %.1f %.1f target=0x%llX\r\n",
                    (int)order,
                    ctx.currentPhase,
                    ctx.validationMask,
                    (unsigned long long)ctx.localPlayer,
                    (unsigned long long)ctx.issueOrderFn,
                    (unsigned long long)ctx.spoofTrampoline,
                    position.x, position.y, position.z,
                    (unsigned long long)target);
                AppendIssueOrderDebug(buf);
            }
        }

        CoreValidation::MarkIssueOrderResult(ok);
        return ok;
    }

    inline bool MoveToPos(const Vec3& position) {
        return IssueOrder(OrderType::MoveTo, position, 0, false, false, false);
    }

    inline bool AttackObject(uintptr_t target, const Vec3& fallbackPosition) {
        return IssueOrder(OrderType::AttackUnit, fallbackPosition, target, true, false, false);
    }

    inline bool AttackMoveTo(const Vec3& position) {
        return IssueOrder(OrderType::AttackMove, position, 0, false, false, false);
    }

    inline bool PrepareSpellInput(int slotId, const Vec3& start, const Vec3& end, uint32_t targetNetId = 0) {
        const auto slot = CoreSpellBook::GetSlot(CoreRuntime::GetContext().localPlayer, slotId);
        if (!slot.IsValid()) {
            return false;
        }
        return slot.SetInputData(targetNetId, start, end);
    }

    // ── CastState (HudSpellInfo) struct layout from IDA ──
    // castState + 0x08 = target object ptr (QWORD)
    // castState + 0x2C = position data area
    // castState + 0x40 = target position X (float)
    // castState + 0x44 = target position Y (float)
    // castState + 0x48 = target position Z (float)
    // castState + 0x68 = spell slot ID (DWORD)
    // castState + 0x6C = cast flag (byte)
    inline constexpr uintptr_t CastStateQueuedTargetObjPtr = 0x08;
    inline constexpr uintptr_t CastStateQueuedPos = 0x2C;
    inline constexpr uintptr_t CastStateModeFlag = 0x28;
    inline constexpr uintptr_t CastStateActiveTargetObjPtr = 0x38;
    inline constexpr uintptr_t CastStateActiveTargetPosX = 0x40;
    inline constexpr uintptr_t CastStateActiveTargetPosY = 0x44;
    inline constexpr uintptr_t CastStateActiveTargetPosZ = 0x48;
    inline constexpr uintptr_t CastStateFallbackTargetObjPtr = 0x70;
    inline constexpr uintptr_t CastStateSlotId = 0x68;
    inline constexpr uintptr_t CastStateFlag = 0x6C;

    inline bool WriteCastStatePos(uintptr_t castState, uintptr_t offset, const Vec3& value) {
        if (!Globals::IsValidPtr(castState)) {
            return false;
        }

        bool ok = true;
        ok &= Globals::Write<float>(castState + offset + 0x0, value.x);
        ok &= Globals::Write<float>(castState + offset + 0x4, value.y);
        ok &= Globals::Write<float>(castState + offset + 0x8, value.z);
        return ok;
    }

    inline bool ShouldRunTargetFollow(uintptr_t targetObject) {
        if (!Globals::IsValidPtr(targetObject)) {
            return false;
        }

        const auto spellData = Globals::Read<uintptr_t>(targetObject + GameObjectSpellDataOffset);
        if (!Globals::IsValidPtr(spellData)) {
            return false;
        }

        return Globals::Read<uint8_t>(spellData + GameObjectTargetFollowFlagOffset) != 0;
    }

    inline bool InvokeCastSpellTargetFlow(int slotId, const Vec3& end, uint32_t targetNetId) {
        auto& ctx = CoreRuntime::g_ctx;
        const auto castTargetCommitFn = CoreRuntime::ResolveRva(CastSpellTargetCommitRva);
        const auto castTargetFollowFn = CoreRuntime::ResolveRva(CastSpellTargetFollowRva);
        if (!Globals::IsValidPtr(ctx.spoofTrampoline) ||
            !Globals::IsValidPtr(castTargetCommitFn) ||
            !Globals::IsValidPtr(castTargetFollowFn)) {
            AppendIssueOrderDebug("[CastTarget] FAIL: target flow fn/trampoline missing\r\n");
            return false;
        }

        const auto castState = CoreView::GetHudSpellInfo();
        if (!Globals::IsValidPtr(castState)) {
            AppendIssueOrderDebug("[CastTarget] FAIL: castState invalid\r\n");
            return false;
        }

        const auto targetRef = CoreObjects::FindByNetId(static_cast<int>(targetNetId));
        const auto targetObject = targetRef.IsValid() ? targetRef.address : 0;
        if (!Globals::IsValidPtr(targetObject)) {
            AppendIssueOrderDebug("[CastTarget] FAIL: target object invalid\r\n");
            return false;
        }

        const auto origQueuedTargetObj = Globals::Read<uintptr_t>(castState + CastStateQueuedTargetObjPtr);
        const auto origQueuedPos = Globals::Read<Vec3>(castState + CastStateQueuedPos);
        const auto origModeFlag = Globals::Read<uint8_t>(castState + CastStateModeFlag);
        const auto origActiveTargetObj = Globals::Read<uintptr_t>(castState + CastStateActiveTargetObjPtr);
        const auto origActivePosX = Globals::Read<float>(castState + CastStateActiveTargetPosX);
        const auto origActivePosY = Globals::Read<float>(castState + CastStateActiveTargetPosY);
        const auto origActivePosZ = Globals::Read<float>(castState + CastStateActiveTargetPosZ);
        const auto origFallbackTargetObj = Globals::Read<uintptr_t>(castState + CastStateFallbackTargetObjPtr);
        const auto origSlot = Globals::Read<uint32_t>(castState + CastStateSlotId);
        const auto origFlag = Globals::Read<uint8_t>(castState + CastStateFlag);

        Globals::Write<uintptr_t>(castState + CastStateQueuedTargetObjPtr, targetObject);
        WriteCastStatePos(castState, CastStateQueuedPos, end);
        Globals::Write<uint8_t>(castState + CastStateModeFlag, 1);
        Globals::Write<uintptr_t>(castState + CastStateActiveTargetObjPtr, targetObject);
        Globals::Write<float>(castState + CastStateActiveTargetPosX, end.x);
        Globals::Write<float>(castState + CastStateActiveTargetPosY, end.y);
        Globals::Write<float>(castState + CastStateActiveTargetPosZ, end.z);
        Globals::Write<uintptr_t>(castState + CastStateFallbackTargetObjPtr, targetObject);
        Globals::Write<uint32_t>(castState + CastStateSlotId, static_cast<uint32_t>(slotId));
        Globals::Write<uint8_t>(castState + CastStateFlag, 1);

        const auto hudInput = Globals::IsValidPtr(ctx.hudInstance)
            ? Globals::Read<uintptr_t>(ctx.hudInstance + Offset::Hud::Input) : 0;
        Vec3 origMousePos = {};
        uint32_t origSelectedNetId = 0;
        if (Globals::IsValidPtr(hudInput)) {
            origMousePos = Globals::Read<Vec3>(hudInput + Offset::Hud::MouseWorldPos);
            origSelectedNetId = Globals::Read<uint32_t>(hudInput + Offset::Hud::SelectedObjNetId);
            Globals::Write<Vec3>(hudInput + Offset::Hud::MouseWorldPos, end);
            Globals::Write<uint32_t>(hudInput + Offset::Hud::SelectedObjNetId, targetNetId);
        }

        {
            char dbg[512] = {};
            std::snprintf(dbg, sizeof(dbg),
                "[CastTarget] slot=%d castState=0x%llX targetNetId=%u targetObj=0x%llX commit=0x%llX follow=0x%llX pos=(%.1f %.1f %.1f)\r\n",
                slotId,
                (unsigned long long)castState,
                targetNetId,
                (unsigned long long)targetObject,
                (unsigned long long)castTargetCommitFn,
                (unsigned long long)castTargetFollowFn,
                end.x,
                end.y,
                end.z);
            AppendIssueOrderDebug(dbg);
        }

        using fnCastTargetCommit = void(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
        using fnCastTargetFollow = void(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);

        bool ok = false;
        __try {
            CoreBypass::PrepareCastSpell();
            spoof_call(
                reinterpret_cast<void*>(ctx.spoofTrampoline),
                reinterpret_cast<fnCastTargetCommit>(castTargetCommitFn),
                castState,
                targetObject,
                targetObject,
                castState);

            if (ShouldRunTargetFollow(targetObject)) {
                spoof_call(
                    reinterpret_cast<void*>(ctx.spoofTrampoline),
                    reinterpret_cast<fnCastTargetFollow>(castTargetFollowFn),
                    castState,
                    targetObject,
                    targetObject,
                    castState);
            }
            ok = true;
        }
        __except (1) {
            ok = false;
        }

        Globals::Write<uintptr_t>(castState + CastStateQueuedTargetObjPtr, origQueuedTargetObj);
        WriteCastStatePos(castState, CastStateQueuedPos, origQueuedPos);
        Globals::Write<uint8_t>(castState + CastStateModeFlag, origModeFlag);
        Globals::Write<uintptr_t>(castState + CastStateActiveTargetObjPtr, origActiveTargetObj);
        Globals::Write<float>(castState + CastStateActiveTargetPosX, origActivePosX);
        Globals::Write<float>(castState + CastStateActiveTargetPosY, origActivePosY);
        Globals::Write<float>(castState + CastStateActiveTargetPosZ, origActivePosZ);
        Globals::Write<uintptr_t>(castState + CastStateFallbackTargetObjPtr, origFallbackTargetObj);
        Globals::Write<uint32_t>(castState + CastStateSlotId, origSlot);
        Globals::Write<uint8_t>(castState + CastStateFlag, origFlag);

        if (Globals::IsValidPtr(hudInput)) {
            Globals::Write<Vec3>(hudInput + Offset::Hud::MouseWorldPos, origMousePos);
            Globals::Write<uint32_t>(hudInput + Offset::Hud::SelectedObjNetId, origSelectedNetId);
        }

        CoreBypass::ClearCastSpellFlag();
        return ok;
    }

    inline bool InvokeCastSpellSafe(int slotId, const Vec3& start, const Vec3& end, uint32_t targetNetId, int castMode) {
        auto& ctx = CoreRuntime::g_ctx;
        if (!ctx.castSpellFn || !Globals::IsValidPtr(ctx.spoofTrampoline)) {
            AppendIssueOrderDebug("[CastSafe] FAIL: no fn or no trampoline\r\n");
            return false;
        }

        // ── Get hudSpellInfo = *(HudInstance + 0x68) ──
        const auto hudSpellInfo = CoreView::GetHudSpellInfo();
        if (!Globals::IsValidPtr(hudSpellInfo)) {
            AppendIssueOrderDebug("[CastSafe] FAIL: hudSpellInfo invalid\r\n");
            return false;
        }

        // ── Get spellInputPtr from spell slot (CastSpellSafe→sub_BA0450 matches arg2 with *(slot+0x130)) ──
        const auto slot = CoreSpellBook::GetSlot(ctx.localPlayer, slotId);
        if (!slot.IsValid()) {
            AppendIssueOrderDebug("[CastSafe] FAIL: spell slot invalid\r\n");
            return false;
        }
        const auto spellInputPtr = slot.GetSpellInput();
        if (!Globals::IsValidPtr(spellInputPtr)) {
            AppendIssueOrderDebug("[CastSafe] FAIL: spellInputPtr invalid\r\n");
            return false;
        }

        // ── Prepare position data (lives in SpellInfo 0x128, not SpellInput 0x130) ──
        const auto posStruct = slot.GetSpellInfo();
        Vec3 origInputStart = {}, origInputEnd = {};
        uint32_t origInputTargetNetId = 0;
        if (Globals::IsValidPtr(posStruct)) {
            origInputStart = Globals::Read<Vec3>(posStruct + Offset::SpellInputLayout::InputStartPos);
            origInputEnd = Globals::Read<Vec3>(posStruct + Offset::SpellInputLayout::InputEndPos);
            origInputTargetNetId = Globals::Read<uint32_t>(posStruct + Offset::SpellInputLayout::InputTargetNetId);
            Globals::Write<Vec3>(posStruct + Offset::SpellInputLayout::InputStartPos, start);
            Globals::Write<Vec3>(posStruct + Offset::SpellInputLayout::InputEndPos, end);
            Globals::Write<uint32_t>(posStruct + Offset::SpellInputLayout::InputTargetNetId, targetNetId);
        }

        // ── Also write HudInput→MouseWorldPos for packet position ──
        const auto hudInput = Globals::IsValidPtr(ctx.hudInstance)
            ? Globals::Read<uintptr_t>(ctx.hudInstance + Offset::Hud::Input) : 0;
        Vec3 origMousePos = {};
        uint32_t origSelectedNetId = 0;
        if (Globals::IsValidPtr(hudInput)) {
            origMousePos = Globals::Read<Vec3>(hudInput + Offset::Hud::MouseWorldPos);
            origSelectedNetId = Globals::Read<uint32_t>(hudInput + Offset::Hud::SelectedObjNetId);
            Globals::Write<Vec3>(hudInput + Offset::Hud::MouseWorldPos, end);
            Globals::Write<uint32_t>(hudInput + Offset::Hud::SelectedObjNetId, targetNetId);
        }

        {
            char dbg[512] = {};
            std::snprintf(dbg, sizeof(dbg),
                "[CastSafe] slot=%d hudSpellInfo=0x%llX spellInputPtr=0x%llX castFn=0x%llX targetNetId=%u pos=(%.1f %.1f %.1f)\r\n",
                slotId, (unsigned long long)hudSpellInfo, (unsigned long long)spellInputPtr,
                (unsigned long long)ctx.castSpellFn, targetNetId, end.x, end.y, end.z);
            AppendIssueOrderDebug(dbg);
        }

        // ── Call CastSpellSafe via spoof_call ──
        using fnCastSpellSafe = void(__fastcall*)(uintptr_t, uintptr_t);

        bool ok = false;
        __try {
            CoreBypass::PrepareCastSpell();
            spoof_call(
                reinterpret_cast<void*>(ctx.spoofTrampoline),
                reinterpret_cast<fnCastSpellSafe>(ctx.castSpellFn),
                hudSpellInfo,
                spellInputPtr);
            ok = true;
        }
        __except (1) {
            ok = false;
        }

        // ── Restore position data ──
        if (Globals::IsValidPtr(posStruct)) {
            Globals::Write<Vec3>(posStruct + Offset::SpellInputLayout::InputStartPos, origInputStart);
            Globals::Write<Vec3>(posStruct + Offset::SpellInputLayout::InputEndPos, origInputEnd);
            Globals::Write<uint32_t>(posStruct + Offset::SpellInputLayout::InputTargetNetId, origInputTargetNetId);
        }

        // ── Restore HudInput ──
        if (Globals::IsValidPtr(hudInput)) {
            Globals::Write<Vec3>(hudInput + Offset::Hud::MouseWorldPos, origMousePos);
            Globals::Write<uint32_t>(hudInput + Offset::Hud::SelectedObjNetId, origSelectedNetId);
        }

        CoreBypass::ClearCastSpellFlag();
        return ok;
    }

    inline bool CastSpellPacket(int slotId, const Vec3& start, const Vec3& end, uint32_t targetNetId = 0) {
        auto& ctx = CoreRuntime::g_ctx;
        if (!CoreRuntime::IsWritePhase() || !CanCastSpell() || !Globals::IsValidPtr(ctx.localPlayer) || !Globals::IsValidPtr(ctx.spoofTrampoline)) {
            CoreValidation::MarkCastResult(false);
            return false;
        }

        const auto slot = CoreSpellBook::GetSlot(ctx.localPlayer, slotId);
        if (!slot.IsValid()) {
            CoreValidation::MarkCastResult(false);
            return false;
        }

        const auto spellInput = slot.GetSpellInput();
        if (!Globals::IsValidPtr(spellInput)) {
            CoreValidation::MarkCastResult(false);
            return false;
        }

        // ── Save position originals (from SpellInfo 0x128) ──
        const auto posPtr = slot.GetSpellInfo();
        const auto origStartPos = Globals::IsValidPtr(posPtr) ? Globals::Read<Vec3>(posPtr + Offset::SpellBook::InputStartPos) : Vec3{};
        const auto origEndPos = Globals::IsValidPtr(posPtr) ? Globals::Read<Vec3>(posPtr + Offset::SpellBook::InputEndPos) : Vec3{};
        const auto origEndPos2 = Globals::IsValidPtr(posPtr) ? Globals::Read<Vec3>(posPtr + Offset::SpellBook::InputEndPos + sizeof(Vec3)) : Vec3{};
        const auto origEndPos3 = Globals::IsValidPtr(posPtr) ? Globals::Read<Vec3>(posPtr + Offset::SpellBook::InputEndPos + sizeof(Vec3) * 2) : Vec3{};
        const int castMode = (targetNetId != 0) ? CastSpellModeNormal : CastSpellModeSmart;

        // ── Write SpellInput ──
        if (!slot.SetInputData(targetNetId, start, end)) {
            CoreValidation::MarkCastResult(false);
            return false;
        }

        bool ok = false;
        if (targetNetId != 0) {
            ok = InvokeCastSpellTargetFlow(slotId, end, targetNetId);
            AppendIssueOrderDebug(ok
                ? "[CastTarget] returned OK\r\n"
                : "[CastTarget] failed, fallback to CastSpellSafe\r\n");
        }

        if (!ok) {
            // ── Call CastSpellSafe (handles castState + HudInput internally) ──
            ok = InvokeCastSpellSafe(slotId, start, end, targetNetId, castMode);
            AppendIssueOrderDebug(ok
                ? "[CastSafe] returned OK\r\n"
                : "[CastSafe] failed (SEH/crash)\r\n");
        }

        // ── Restore position data ──
        if (Globals::IsValidPtr(posPtr)) {
            Globals::Write<Vec3>(posPtr + Offset::SpellBook::InputStartPos, origStartPos);
            Globals::Write<Vec3>(posPtr + Offset::SpellBook::InputEndPos, origEndPos);
            Globals::Write<Vec3>(posPtr + Offset::SpellBook::InputEndPos + sizeof(Vec3), origEndPos2);
            Globals::Write<Vec3>(posPtr + Offset::SpellBook::InputEndPos + sizeof(Vec3) * 2, origEndPos3);
        }

        CoreValidation::MarkCastResult(ok);
        return ok;
    }

    inline bool UpdateChargedSpell(int slotId, const Vec3& position, bool releaseCast) {
        auto& ctx = CoreRuntime::g_ctx;
        if (!CoreRuntime::IsWritePhase() || !CanUpdateChargedSpell() || !Globals::IsValidPtr(ctx.localPlayer) || !ctx.updateChargedSpellFn || !Globals::IsValidPtr(ctx.spoofTrampoline)) {
            CoreValidation::MarkCastResult(false);
            return false;
        }

        const auto spellBook = CoreSpellBook::GetSpellBook(ctx.localPlayer);
        const auto slot = CoreSpellBook::GetSlot(ctx.localPlayer, slotId);
        if (!Globals::IsValidPtr(spellBook) || !slot.IsValid()) {
            CoreValidation::MarkCastResult(false);
            return false;
        }

        Vec3 pos = position;
        using fnUpdateChargedSpell = void(__fastcall*)(uintptr_t, uintptr_t, int, Vec3*, bool);
        bool ok = false;
        __try {
            CoreBypass::MainloopCheck();
            spoof_call(
                reinterpret_cast<void*>(ctx.spoofTrampoline),
                reinterpret_cast<fnUpdateChargedSpell>(ctx.updateChargedSpellFn),
                spellBook,
                slot.address,
                slotId,
                &pos,
                releaseCast);
            ok = true;
        }
        __except (1) {
            ok = false;
        }

        CoreValidation::MarkCastResult(ok);
        return ok;
    }

} // namespace CoreControl
