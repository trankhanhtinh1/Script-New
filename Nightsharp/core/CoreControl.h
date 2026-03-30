#pragma once

#include "CoreBypass.h"
#include "CoreGame.h"
#include "CoreObjects.h"
#include "CoreSpellBook.h"
#include "CoreView.h"
#include "CoreValidation.h"
#include <cstdio>

namespace CoreControl {

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

    inline float ReadAttackWindupFor(uintptr_t object) {
        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(object) || !ctx.getAttackWindupFn) {
            return 0.0f;
        }

        using fnAttackWindup = float(__cdecl*)(uintptr_t, int);
        __try {
            const float value = reinterpret_cast<fnAttackWindup>(ctx.getAttackWindupFn)(object, 0x40);
            return (value > 0.0f && value < 10.0f) ? value : 0.0f;
        }
        __except (1) {
            return 0.0f;
        }
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

        const auto spellInfo = slot.GetSpellInfo();
        const auto spellInput = slot.GetSpellInput();
        const auto hudSpellInfo = CoreView::GetHudSpellInfo();
        if (!Globals::IsValidPtr(spellInfo) || !Globals::IsValidPtr(spellInput) || !Globals::IsValidPtr(hudSpellInfo) || !ctx.castSpellFn) {
            CoreValidation::MarkCastResult(false);
            return false;
        }

        const auto playerPos = CoreObjects::GetLocalPlayer().GetPosition();
        const auto origStartPos = Globals::Read<Vec3>(spellInput + Offset::SpellBook::InputStartPos);
        const auto origEndPos = Globals::Read<Vec3>(spellInput + Offset::SpellBook::InputEndPos);
        const auto origEndPos2 = Globals::Read<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3));
        const auto origEndPos3 = Globals::Read<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3) * 2);

        const auto hud = ctx.hudInstance;
        const auto hudInput = Globals::IsValidPtr(hud) ? Globals::Read<uintptr_t>(hud + Offset::Hud::Input) : 0;
        const auto origMouse = Globals::IsValidPtr(hudInput)
            ? Globals::Read<Vec3>(hudInput + Offset::Hud::MouseWorldPos)
            : Vec3{};

        if (!slot.SetInputData(targetNetId, playerPos, end)) {
            CoreValidation::MarkCastResult(false);
            return false;
        }

        if (Globals::IsValidPtr(hudInput)) {
            Globals::Write<Vec3>(hudInput + Offset::Hud::MouseWorldPos, end);
        }

        bool ok = false;
        using fnCastSpellLegacy = void(__fastcall*)(uintptr_t, uintptr_t);
        __try {
            CoreBypass::PrepareCastSpell();
            spoof_call(
                reinterpret_cast<void*>(ctx.spoofTrampoline),
                reinterpret_cast<fnCastSpellLegacy>(ctx.castSpellFn),
                hudSpellInfo,
                spellInfo);
            ok = true;
        }
        __except (1) {
            ok = false;
        }

        Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputStartPos, origStartPos);
        Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos, origEndPos);
        Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3), origEndPos2);
        Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3) * 2, origEndPos3);
        if (Globals::IsValidPtr(hudInput)) {
            Globals::Write<Vec3>(hudInput + Offset::Hud::MouseWorldPos, origMouse);
        }

        if (!ok) {
            CoreBypass::ClearCastSpellFlag();
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
