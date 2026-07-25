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

        inline void LogCoreRelease(const char* message) {
            HANDLE hFile = CreateFileA(
                "C:\\Users\\Public\\nightsharp_core_release_log.txt",
                FILE_APPEND_DATA,
                FILE_SHARE_READ,
                nullptr,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD written = 0;
                WriteFile(hFile, message, static_cast<DWORD>(std::strlen(message)), &written, nullptr);
                CloseHandle(hFile);
            }
        }

        inline bool ReleaseChargeSpellMethod1(std::uint8_t slot,
                                              const Vec3& position) {
            if (!CoreCastSpell::detail::ResolveCommon(
                    CoreCastSpell::CastKind::ChargeRelease,
                    slot)) {
                LogCoreRelease("[CoreNewCastSpell] ResolveCommon failed for ReleaseChargeSpellMethod1\r\n");
                return false;
            }

            auto& trace = CoreCastSpell::g_lastTrace;
            trace.endPosition = position;

            if (!position.IsValid() || position.IsZero()) {
                Fail(CoreCastSpell::CastFailure::InvalidPosition);
                LogCoreRelease("[CoreNewCastSpell] Invalid position for ReleaseChargeSpellMethod1\r\n");
                return false;
            }

            const CoreCastSpell::NativeVec3Xyz nativePosition{
                position.x,
                position.y,
                position.z
            };

            const uintptr_t releaseActiveChargeFn = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::ReleaseActiveCharge);
            const uintptr_t castSpellSafeFn = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::CastSpellSafe);
            const uintptr_t updateChargeFn = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::UpdateChargeableSpell);

            char buf[512] = {};
            std::snprintf(
                buf,
                sizeof(buf),
                "[CoreNewCastSpell] ReleaseAttempt tick=%lu slot=%d pos=%.1f,%.1f,%.1f relChargeFn=0x%llX castSafeFn=0x%llX updateChargeFn=0x%llX spellbook=0x%llX slotObj=0x%llX\r\n",
                GetTickCount(),
                slot,
                position.x,
                position.y,
                position.z,
                static_cast<unsigned long long>(releaseActiveChargeFn),
                static_cast<unsigned long long>(castSpellSafeFn),
                static_cast<unsigned long long>(updateChargeFn),
                static_cast<unsigned long long>(trace.spellbook),
                static_cast<unsigned long long>(trace.spellSlot));
            LogCoreRelease(buf);

            bool nativeException = false;
            bool bypassTouched = false;
            std::int64_t nativeResult = 0;

            __try {
                trace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;

                if (Globals::IsValidPtr(releaseActiveChargeFn)) {
                    using FnReleaseActiveCharge = std::int64_t(__fastcall*)(
                        uintptr_t spellbook,
                        std::uint32_t slotIndex,
                        const CoreCastSpell::NativeVec3Xyz* position);

                    nativeResult = spoof_call(
                        reinterpret_cast<void*>(trace.spoofTrampoline),
                        reinterpret_cast<FnReleaseActiveCharge>(releaseActiveChargeFn),
                        trace.spellbook,
                        static_cast<std::uint32_t>(slot),
                        &nativePosition);

                    std::snprintf(buf, sizeof(buf), "[CoreNewCastSpell] ReleaseActiveCharge res=%lld\r\n", static_cast<long long>(nativeResult));
                    LogCoreRelease(buf);
                }
                else if (Globals::IsValidPtr(castSpellSafeFn)) {
                    using FnCastSpellSafe = std::int64_t(__fastcall*)(
                        uintptr_t spellbook,
                        uintptr_t spellSlot,
                        std::int32_t slot,
                        const CoreCastSpell::NativeVec3Xyz* startPosition,
                        const CoreCastSpell::NativeVec3Xyz* endPosition,
                        uintptr_t targetNetId);

                    nativeResult = spoof_call(
                        reinterpret_cast<void*>(trace.spoofTrampoline),
                        reinterpret_cast<FnCastSpellSafe>(castSpellSafeFn),
                        trace.spellbook,
                        trace.spellSlot,
                        static_cast<std::int32_t>(slot),
                        &nativePosition,
                        &nativePosition,
                        static_cast<uintptr_t>(0));

                    std::snprintf(buf, sizeof(buf), "[CoreNewCastSpell] CastSpellSafe res=%lld\r\n", static_cast<long long>(nativeResult));
                    LogCoreRelease(buf);
                }
            }
            __except (1) {
                nativeException = true;
                LogCoreRelease("[CoreNewCastSpell] NativeException caught during release\r\n");
            }

            CoreCastSpell::detail::ClearBypassFlag(bypassTouched);

            if (nativeException) {
                Fail(CoreCastSpell::CastFailure::NativeException);
                return false;
            }

            trace.canCastAccepted = true;
            trace.nativeResult = nativeResult;
            trace.failure = CoreCastSpell::CastFailure::None;
            trace.success = true;
            CoreValidation::MarkCastResult(true);
            LogCoreRelease("[CoreNewCastSpell] Release method finished OK\r\n");
            return true;
        }

        inline bool CastPositionSpellNative(std::uint8_t slot, const Vec3& position) {
            if (!CoreCastSpell::detail::ResolveCommon(
                    CoreCastSpell::CastKind::Position,
                    slot)) {
                return false;
            }

            if (!CoreCastSpell::detail::ThrottleReserve(slot)) {
                Fail(CoreCastSpell::CastFailure::Throttled);
                return false;
            }

            if (!position.IsValid() || position.IsZero()) {
                Fail(CoreCastSpell::CastFailure::InvalidPosition);
                return false;
            }

            auto& trace = CoreCastSpell::g_lastTrace;
            trace.endPosition = position;
            trace.virtualCursorApplied = false;
            trace.virtualCursorScreen = {};

            const uintptr_t castFn = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::CastSpellVector);
            if (!Globals::IsValidPtr(castFn)) {
                Fail(CoreCastSpell::CastFailure::MissingNativeFunction);
                return false;
            }
            trace.castSpellSafe = castFn;

            const uintptr_t visionPtr = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::CastVisionIndexDefault);
            const std::int32_t visionFlag = Globals::IsValidPtr(visionPtr)
                ? Globals::Read<std::int32_t>(visionPtr)
                : 0;

            const CoreCastSpell::NativeVec3Xyz start{
                position.x,
                position.y,
                position.z
            };
            const CoreCastSpell::NativeVec3Xyz end{
                position.x,
                position.y,
                position.z
            };

            bool nativeException = false;
            bool bypassTouched = false;

            __try {
                NightSharpDebug::CrashTrace::Record(
                    nscrash::TraceTag::SpellCast,
                    static_cast<std::uint64_t>(slot),
                    static_cast<std::uint64_t>(CoreCastSpell::CastKind::Position));
                trace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;

                spoof_call(
                    reinterpret_cast<void*>(trace.spoofTrampoline),
                    reinterpret_cast<CoreCastSpell::FnCastSpellVector>(castFn),
                    trace.spellbook,
                    trace.spellSlot,
                    static_cast<std::int32_t>(slot),
                    &start,
                    &end,
                    visionFlag);

                trace.canCastAccepted = true;
                trace.nativeResult = 1;
            }
            __except (1) {
                nativeException = true;
            }

            CoreCastSpell::detail::ClearBypassFlag(bypassTouched);

            if (nativeException) {
                Fail(CoreCastSpell::CastFailure::NativeException);
                return false;
            }

            trace.failure = CoreCastSpell::CastFailure::None;
            trace.success = true;
            CoreValidation::MarkCastResult(true);
            return true;
        }

        inline bool ExecuteTargetCastNativeDispatch(
            CoreCastSpell::CastTrace& trace,
            uintptr_t castVectorFn,
            std::uint8_t slot,
            const CoreCastSpell::NativeVec3Xyz* start,
            const CoreCastSpell::NativeVec3Xyz* end,
            std::uint32_t targetNetId,
            bool& bypassTouched) {
            bool nativeException = false;
            __try {
                NightSharpDebug::CrashTrace::Record(
                    nscrash::TraceTag::SpellCast,
                    static_cast<std::uint64_t>(slot),
                    static_cast<std::uint64_t>(CoreCastSpell::CastKind::Target));
                trace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;

                using FnCastSpellVector = void(__fastcall*)(
                    uintptr_t spellbook,
                    uintptr_t slotObject,
                    std::int32_t slot,
                    const CoreCastSpell::NativeVec3Xyz* startPosition,
                    const CoreCastSpell::NativeVec3Xyz* endPosition,
                    std::uint32_t targetNetId);

                spoof_call(
                    reinterpret_cast<void*>(trace.spoofTrampoline),
                    reinterpret_cast<FnCastSpellVector>(castVectorFn),
                    trace.spellbook,
                    trace.spellSlot,
                    static_cast<std::int32_t>(slot),
                    start,
                    end,
                    targetNetId);
            }
            __except (1) {
                nativeException = true;
            }
            return !nativeException;
        }

        inline bool CastTargetSpellNative(std::uint8_t slot, uintptr_t target) {
            if (!CoreCastSpell::detail::ResolveCommon(
                    CoreCastSpell::CastKind::Target,
                    slot)) {
                return false;
            }

            if (!CoreCastSpell::detail::ThrottleReserve(slot)) {
                Fail(CoreCastSpell::CastFailure::Throttled);
                return false;
            }

            uintptr_t targetAddress = 0;
            std::uint32_t targetNetId = 0;

            if (Globals::IsValidPtr(target)) {
                targetAddress = target;
                targetNetId = Globals::Read<std::uint32_t>(targetAddress + Offset::All::NetworkId);
            } else if (target != 0 && target != 0xFFFFFFFFu) {
                targetNetId = static_cast<std::uint32_t>(target);
                targetAddress = ::Core::ObjectManager::FindByNetworkId(targetNetId);
            }

            if (targetNetId == 0 || targetNetId == 0xFFFFFFFFu) {
                Fail(CoreCastSpell::CastFailure::InvalidTarget);
                return false;
            }

            auto& trace = CoreCastSpell::g_lastTrace;
            trace.targetNetworkId = targetNetId;
            if (Globals::IsValidPtr(targetAddress)) {
                trace.endPosition = Globals::Read<Vec3>(targetAddress + Offset::All::Position);
                trace.targetObjectIndex = Globals::Read<std::uint32_t>(targetAddress + Offset::All::Index);
            } else {
                trace.endPosition = trace.startPosition;
            }

            trace.virtualCursorApplied = false;
            trace.virtualCursorScreen = {};

            const uintptr_t castVectorFn =
                CoreRuntime::ResolveRva(Offset::ControlRuntime::CastSpellVector);

            trace.canCastCheck = 0;
            trace.castSpellSafe = castVectorFn;

            if (!Globals::IsValidPtr(castVectorFn)) {
                Fail(CoreCastSpell::CastFailure::MissingNativeFunction);
                return false;
            }

            const CoreCastSpell::NativeVec3Xyz start{
                trace.startPosition.x,
                trace.startPosition.y,
                trace.startPosition.z
            };

            const CoreCastSpell::NativeVec3Xyz end{
                trace.endPosition.x,
                trace.endPosition.y,
                trace.endPosition.z
            };

            bool bypassTouched = false;

            const bool dispatchSuccess = ExecuteTargetCastNativeDispatch(
                trace,
                castVectorFn,
                slot,
                &start,
                &end,
                targetNetId,
                bypassTouched);

            CoreCastSpell::detail::ClearBypassFlag(bypassTouched);

            if (!dispatchSuccess) {
                Fail(CoreCastSpell::CastFailure::NativeException);
                return false;
            }

            trace.canCastAccepted = true;
            trace.nativeResult = 1;
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
        return detail::CastPositionSpellNative(slot, position);
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
        if (releaseCast) {
            return detail::ReleaseChargeSpellMethod1(static_cast<std::uint8_t>(slot), position);
        }
        return detail::CastPositionSpellNative(static_cast<std::uint8_t>(slot), position);
    }

    inline bool CastTargetSpellMethod2(std::int32_t slot, uintptr_t target) {
        if (slot < 0 || slot > CoreCastSpell::SlotTrinket) {
            CoreCastSpell::g_lastTrace = {};
            CoreCastSpell::g_lastTrace.slot =
                static_cast<std::uint8_t>(slot < 0 ? 0 : slot);
            detail::Fail(CoreCastSpell::CastFailure::InvalidSlot);
            return false;
        }

        detail::ScopedWritePhase writePhase;
        return detail::CastTargetSpellNative(
            static_cast<std::uint8_t>(slot),
            target);
    }

    inline bool CastPositionSpellMethod2(std::int32_t slot, const Vec3& position) {
        if (slot < 0 || slot > CoreCastSpell::SlotTrinket) {
            CoreCastSpell::g_lastTrace = {};
            CoreCastSpell::g_lastTrace.slot =
                static_cast<std::uint8_t>(slot < 0 ? 0 : slot);
            detail::Fail(CoreCastSpell::CastFailure::InvalidSlot);
            return false;
        }

        detail::ScopedWritePhase writePhase;
        return detail::CastPositionSpellNative(
            static_cast<std::uint8_t>(slot),
            position);
    }

} // namespace CoreNewCastSpell

namespace CoreCastSpell {
    inline bool CastTargetSpell(std::uint8_t slot, uintptr_t target) {
        return CoreNewCastSpell::CastTargetSpellMethod2(
            static_cast<std::int32_t>(slot),
            target);
    }
}
