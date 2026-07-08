#pragma once

#include "CoreCastSpell.h"

#include <cstdint>

namespace CoreNewCastSpell {

    // Method 1: release charge spells through the native charge packet sender
    // with an explicit world position. This deliberately bypasses the HUD
    // hotkey handler and therefore never asks PrimeCastPosition to raycast the
    // current mouse cursor.
    inline constexpr uintptr_t kInvalidChargeVec2Rva = 0x1E0C820;
    inline constexpr uintptr_t kInvalidChargeZ_Rva = 0x1E0C828;
    inline constexpr std::uint32_t kFloatMaxBits = 0x7F7FFFFF;

    using FnGetOwnerSlotIndex = std::uint8_t(__fastcall*)(uintptr_t spellInput);

    using FnUpdateChargeableSpell = std::int64_t(__fastcall*)(
        uintptr_t spellbook,
        uintptr_t spellSlot,
        std::uint8_t slotIndex,
        const CoreCastSpell::NativeVec3Xyz* position,
        std::uint8_t releaseFlag);

    namespace detail {

        class ScopedWritePhase {
        public:
            ScopedWritePhase() {
                started_ = !CoreRuntime::IsWritePhase();
                if (started_) {
                    CoreRuntime::BeginWritePhase();
                }
            }

            ~ScopedWritePhase() {
                if (started_) {
                    CoreRuntime::EndWritePhase();
                }
            }

        private:
            bool started_ = false;
        };

        inline void Fail(CoreCastSpell::CastFailure failure) {
            CoreCastSpell::detail::Fail(failure);
        }

        inline void ResetHudChargeState(uintptr_t castContext) {
            if (!Globals::IsValidPtr(castContext)) {
                return;
            }

            (void)Globals::Write<uintptr_t>(
                castContext + CoreCastSpell::kHudChargeSpellInput,
                0);

            const uintptr_t invalidVec2 =
                CoreRuntime::ResolveRva(kInvalidChargeVec2Rva);
            const uintptr_t invalidZ =
                CoreRuntime::ResolveRva(kInvalidChargeZ_Rva);
            if (Globals::IsValidPtr(invalidVec2) &&
                Globals::IsValidPtr(invalidZ)) {
                const auto xy = Globals::Read<std::uint64_t>(invalidVec2);
                const auto z = Globals::Read<std::uint32_t>(invalidZ);
                (void)Globals::Write<std::uint64_t>(castContext + 0x40, xy);
                (void)Globals::Write<std::uint32_t>(castContext + 0x48, z);
            }
            else {
                (void)Globals::Write<std::uint32_t>(
                    castContext + 0x40,
                    kFloatMaxBits);
                (void)Globals::Write<std::uint32_t>(
                    castContext + 0x44,
                    kFloatMaxBits);
                (void)Globals::Write<std::uint32_t>(
                    castContext + 0x48,
                    kFloatMaxBits);
            }

            (void)Globals::Write<std::uint32_t>(
                castContext + 0x4C,
                kFloatMaxBits);
            (void)Globals::Write<std::uint32_t>(
                castContext + 0x54,
                kFloatMaxBits);
            (void)Globals::Write<std::uint32_t>(
                castContext + 0x58,
                kFloatMaxBits);
        }

        inline bool ReleaseChargeSpellMethod1(std::uint8_t slot,
                                              const Vec3& position) {
            if (!CoreCastSpell::detail::ResolveCommon(
                    CoreCastSpell::CastKind::ChargeRelease,
                    slot)) {
                return false;
            }

            auto& trace = CoreCastSpell::g_lastTrace;
            trace.endPosition = position;
            trace.virtualCursorApplied = false;
            trace.virtualCursorScreen = {};

            if (!position.IsValid() || position.IsZero()) {
                Fail(CoreCastSpell::CastFailure::InvalidPosition);
                return false;
            }

            const uintptr_t chargeInput = Globals::Read<uintptr_t>(
                trace.castContext + CoreCastSpell::kHudChargeSpellInput);
            if (chargeInput != trace.spellInput) {
                Fail(CoreCastSpell::CastFailure::MissingChargeState);
                return false;
            }

            const uintptr_t updateChargeFn = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::UpdateChargeableSpell);
            const uintptr_t ownerSlotIndexFn = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::GetOwnerSlotIndex);
            trace.castSpellSafe = updateChargeFn;
            trace.canCastCheck = ownerSlotIndexFn;
            trace.runtimeInput = chargeInput;

            if (!Globals::IsValidPtr(updateChargeFn) ||
                !Globals::IsValidPtr(ownerSlotIndexFn)) {
                Fail(CoreCastSpell::CastFailure::MissingNativeFunction);
                return false;
            }

            const CoreCastSpell::NativeVec3Xyz nativePosition{
                position.x,
                position.y,
                position.z
            };

            bool nativeException = false;
            bool bypassTouched = false;
            std::uint8_t ownerSlotIndex = 0;
            std::int64_t nativeResult = 0;

            __try {
                trace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;

                ownerSlotIndex = spoof_call(
                    reinterpret_cast<void*>(trace.spoofTrampoline),
                    reinterpret_cast<FnGetOwnerSlotIndex>(ownerSlotIndexFn),
                    chargeInput);

                nativeResult = spoof_call(
                    reinterpret_cast<void*>(trace.spoofTrampoline),
                    reinterpret_cast<FnUpdateChargeableSpell>(updateChargeFn),
                    trace.spellbook,
                    trace.spellSlot,
                    ownerSlotIndex,
                    &nativePosition,
                    static_cast<std::uint8_t>(1));
            }
            __except (1) {
                nativeException = true;
            }

            CoreCastSpell::detail::ClearBypassFlag(bypassTouched);

            if (nativeException) {
                Fail(CoreCastSpell::CastFailure::NativeException);
                return false;
            }

            ResetHudChargeState(trace.castContext);

            trace.canCastAccepted = true;
            trace.nativeResult = nativeResult;
            trace.failure = CoreCastSpell::CastFailure::None;
            trace.success = true;
            CoreValidation::MarkCastResult(true);
            return true;
        }

    } // namespace detail

    inline const CoreCastSpell::CastTrace& LastTrace() {
        return CoreCastSpell::LastTrace();
    }

    inline bool BeginChargeSpell(std::uint8_t slot, const Vec3& position) {
        return CoreCastSpell::BeginChargeSpell(slot, position);
    }

    inline bool ReleaseChargeSpellMethod1(std::uint8_t slot,
                                          const Vec3& position) {
        return detail::ReleaseChargeSpellMethod1(slot, position);
    }

    inline bool UpdateChargedSpellMethod1(std::int32_t slot,
                                          const Vec3& position,
                                          bool releaseCast) {
        if (slot < 0 || slot > CoreCastSpell::SlotSummonerF) {
            CoreCastSpell::g_lastTrace = {};
            CoreCastSpell::g_lastTrace.slot =
                static_cast<std::uint8_t>(slot < 0 ? 0 : slot);
            detail::Fail(CoreCastSpell::CastFailure::InvalidSlot);
            return false;
        }

        detail::ScopedWritePhase writePhase;
        return releaseCast
            ? ReleaseChargeSpellMethod1(
                static_cast<std::uint8_t>(slot),
                position)
            : BeginChargeSpell(
                static_cast<std::uint8_t>(slot),
                position);
    }

} // namespace CoreNewCastSpell
