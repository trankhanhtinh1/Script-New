#pragma once

#include "CoreCastSpell.h"

#include <cmath>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// CoreNewCastSpell — cast paths that never touch the HUD cursor.
//
// The HUD path the game itself uses is:
//     HudSpellHandler (sub_BD01F0)
//       -> CanCastCheck(ctx, spellInput, 0)  (sub_BF6430)
//            -> PrimeCastPosition (sub_BCB770)   <-- raycasts the mouse cursor
//       -> CastSpellSafe (sub_BE3300)
//       -> InitChargeState (sub_BE4560)          [charged spells only]
//
// PrimeCastPosition is the single place the mouse enters the pipeline: it writes
// the cursor's world position into the cast provider, and every downstream packet
// reads its coordinates from there. Anything routed through the HUD therefore aims
// at the cursor, not at our prediction.
//
// The methods below skip CanCastCheck/PrimeCastPosition entirely and hand the
// native senders explicit world coordinates.
//
// All native offsets used here were verified against the running client
// (see Offset::ControlRuntime for the per-function reverse notes).
// ─────────────────────────────────────────────────────────────────────────────

namespace CoreNewCastSpell {

    // GetOwnerSlotIndex — sub_BCD280(spellInput). Scans the 64 spellbook slots and
    // returns the index whose *(slot + 0x130) == spellInput, or -1 when not found.
    // Returns int, NOT a byte: truncating the -1 sentinel to 0xFF yields a bogus slot.
    using FnGetOwnerSlotIndex = std::int32_t(__fastcall*)(uintptr_t spellInput);

    // UpdateChargeableSpell — sub_98E310. Writes the three floats at *position
    // straight into the charge packet, so the aim is exactly what we pass.
    //   releaseFlag = 0 -> charge begin / re-aim
    //   releaseFlag = 1 -> release
    // slotIndex is passed in r8d as a full 32-bit int.
    using FnUpdateChargeableSpell = std::int64_t(__fastcall*)(
        uintptr_t spellbook,
        uintptr_t spellSlot,
        std::int32_t slotIndex,
        const CoreCastSpell::NativeVec3Xyz* position,
        std::uint8_t releaseFlag);

    // UpdateChargeAim — sub_98EA10. Opcode-1195 packet carrying just the position
    // the charge is aimed at. The client's charge tick spams this every frame with
    // the HUD cursor, which is what keeps a charge visually glued to the mouse; we
    // send our own so the predicted position is what the server last heard.
    using FnUpdateChargeAim = std::int64_t(__fastcall*)(
        uintptr_t spellbook,
        const CoreCastSpell::NativeVec3Xyz* position);

    // InitChargeState — sub_BE4560(castContext, spellInput). Stores the spell input
    // at castContext + 0x38 and seeds the charge timers from the *player's own*
    // position (localPlayer + 0x23C) plus the spell's charge durations. No cursor.
    using FnInitChargeState = void(__fastcall*)(
        uintptr_t castContext,
        uintptr_t spellInput);

    // Cast provider block, reached as *(spellSlot + 0x128) + 8.
    // CastSpellSafe derives the very same pointer (sub_4AEA30 returns
    // *(slot + 0x128), and the packet builders receive that + 8), so priming this
    // is what PrimeCastPosition would otherwise do from the cursor.
    //
    // The provider is polymorphic: the packet builder sub_9876B0 does not read
    // fixed offsets, it fetches each position through a vtable getter. All three
    // getters are plain `return this + N`:
    //     vtable +0x20 -> this + 0x1C   start position
    //     vtable +0x30 -> this + 0x28   cast position
    //     vtable +0x40 -> this + 0x34   end position   <-- sets the shot direction
    // sub_974430 then receives them in that order, which lines up with the
    // (start, start, end) shape CastSpellVector passes for a skillshot.
    // The three vectors are contiguous, so one 0x24-byte write covers them.
    struct TargetProviderIds {
        std::uint32_t sourceObjectIndex = 0;
        std::uint32_t targetObjectIndex = 0;
    };

    struct TargetProviderVectors {
        CoreCastSpell::NativeVec3Xyz startPosition = {};
        CoreCastSpell::NativeVec3Xyz castPosition = {};
        CoreCastSpell::NativeVec3Xyz endPosition = {};
    };

    static_assert(sizeof(TargetProviderIds) == 0x8,
                  "TargetProviderIds must stay two dwords at provider + 0x08");
    static_assert(sizeof(TargetProviderVectors) == 0x24,
                  "TargetProviderVectors must stay three Vec3 at provider + 0x1C");

    namespace detail {

        // castContext layout touched by the charge state machine. Mirrors the
        // fields InitChargeState writes and HudSpellHandler clears on release.
        inline constexpr uintptr_t kChargePosition = 0x40;
        inline constexpr uintptr_t kChargeGraceTime = 0x4C;
        inline constexpr uintptr_t kChargeMinTime = 0x54;
        inline constexpr uintptr_t kChargeFullTime = 0x58;
        // InitChargeState seeds +0x54/+0x58/+0x5C with gameTime plus the spell's
        // charge durations. Those durations are 0 for Xerath Q (spellData +0x358,
        // +0x35C, +0x360 all read 0 live), so the field holds the game time at
        // which the charge opened. Unlike +0x54/+0x58 it survives the end-of-charge
        // reset, which is what makes it usable for recovering an ongoing hold.
        inline constexpr uintptr_t kChargeStartTime = 0x5C;

        // Provider sub-blocks, relative to *(spellSlot + 0x128) + 8.
        inline constexpr uintptr_t kProviderIds = 0x08;
        inline constexpr uintptr_t kProviderVectors = 0x1C;

        // The client's "no position" sentinel: three 0x7F7FFFFF floats, read live
        // from qword_1E49A00 / dword_1E49A08.
        inline constexpr std::uint32_t kFloatMaxBits = 0x7F7FFFFF;

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

        inline bool RequireCastContext(const CoreCastSpell::CastTrace& trace) {
            if (!Globals::IsValidPtr(trace.castContext)) {
                Fail(CoreCastSpell::CastFailure::MissingCastContext);
                return false;
            }
            return true;
        }

        // Provider object that CastSpellSafe hands to the packet builder.
        inline uintptr_t ResolveProvider(uintptr_t spellSlot) {
            const uintptr_t spellInfo = Globals::Read<uintptr_t>(
                spellSlot + Offset::SpellSlotLayout::SlotSpellInfo);
            if (!Globals::IsValidPtr(spellInfo)) {
                return 0;
            }
            return spellInfo + 0x8;
        }

        // Points the provider at an explicit world position, the way
        // PrimeCastPosition would have done from the cursor. Returns the previous
        // contents so the caller can put them back once the packet is out.
        inline bool PrimeProvider(uintptr_t provider,
                                  std::uint32_t sourceObjectIndex,
                                  std::uint32_t targetObjectIndex,
                                  const Vec3& start,
                                  const Vec3& destination,
                                  TargetProviderIds& savedIds,
                                  TargetProviderVectors& savedVectors) {
            savedIds = Globals::Read<TargetProviderIds>(provider + kProviderIds);
            savedVectors = Globals::Read<TargetProviderVectors>(
                provider + kProviderVectors);

            const TargetProviderIds ids{ sourceObjectIndex, targetObjectIndex };
            const TargetProviderVectors vectors{
                { start.x, start.y, start.z },
                { destination.x, destination.y, destination.z },
                { destination.x, destination.y, destination.z }
            };

            return Globals::Write<TargetProviderIds>(provider + kProviderIds, ids) &&
                   Globals::Write<TargetProviderVectors>(
                       provider + kProviderVectors, vectors);
        }

        inline void RestoreProvider(uintptr_t provider,
                                    const TargetProviderIds& savedIds,
                                    const TargetProviderVectors& savedVectors) {
            (void)Globals::Write<TargetProviderIds>(provider + kProviderIds, savedIds);
            (void)Globals::Write<TargetProviderVectors>(
                provider + kProviderVectors, savedVectors);
        }

        inline uintptr_t ReadChargeInput(uintptr_t castContext) {
            if (!Globals::IsValidPtr(castContext)) {
                return 0;
            }
            return Globals::Read<uintptr_t>(
                castContext + CoreCastSpell::kHudChargeSpellInput);
        }

        // Mirrors the reset HudSpellHandler performs once a charge ends, so the
        // client does not keep believing a charge is still open. It deliberately
        // leaves +0x50/+0x5C/+0x60 alone — the game leaves those stale too.
        inline void ResetHudChargeState(uintptr_t castContext) {
            if (!Globals::IsValidPtr(castContext)) {
                return;
            }

            (void)Globals::Write<uintptr_t>(
                castContext + CoreCastSpell::kHudChargeSpellInput, 0);
            (void)Globals::Write<std::uint32_t>(
                castContext + kChargePosition + 0x0, kFloatMaxBits);
            (void)Globals::Write<std::uint32_t>(
                castContext + kChargePosition + 0x4, kFloatMaxBits);
            (void)Globals::Write<std::uint32_t>(
                castContext + kChargePosition + 0x8, kFloatMaxBits);
            (void)Globals::Write<std::uint32_t>(
                castContext + kChargeGraceTime, kFloatMaxBits);
            (void)Globals::Write<std::uint32_t>(
                castContext + kChargeMinTime, kFloatMaxBits);
            (void)Globals::Write<std::uint32_t>(
                castContext + kChargeFullTime, kFloatMaxBits);
        }

        // slotIndex the charge packet expects. GetOwnerSlotIndex is authoritative;
        // for Q/W/E/R it resolves to the slot itself, so that is the fallback.
        inline std::int32_t ResolveSlotIndex(const CoreCastSpell::CastTrace& trace,
                                             uintptr_t spellInput,
                                             std::uint8_t slot) {
            const uintptr_t fn = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::GetOwnerSlotIndex);
            if (!Globals::IsValidPtr(fn) || !Globals::IsValidPtr(spellInput)) {
                return static_cast<std::int32_t>(slot);
            }

            std::int32_t resolved = -1;
            __try {
                resolved = spoof_call(
                    reinterpret_cast<void*>(trace.spoofTrampoline),
                    reinterpret_cast<FnGetOwnerSlotIndex>(fn),
                    spellInput);
            }
            __except (1) {
                resolved = -1;
            }

            if (resolved < 0 || resolved > 63) {
                return static_cast<std::int32_t>(slot);
            }
            return resolved;
        }

        // Counters the cursor stream the client's own charge tick puts on the wire.
        // Best effort: a failure here only costs aim fidelity during the charge, the
        // begin and release packets carry their coordinates themselves.
        inline void SendChargeAim(const CoreCastSpell::CastTrace& trace,
                                  const Vec3& position) {
            const uintptr_t aimFn = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::UpdateChargeAim);
            if (!Globals::IsValidPtr(aimFn) ||
                !Globals::IsValidPtr(trace.spellbook)) {
                return;
            }

            const CoreCastSpell::NativeVec3Xyz nativePosition{
                position.x,
                position.y,
                position.z
            };

            __try {
                (void)spoof_call(
                    reinterpret_cast<void*>(trace.spoofTrampoline),
                    reinterpret_cast<FnUpdateChargeAim>(aimFn),
                    trace.spellbook,
                    &nativePosition);
            }
            __except (1) {}
        }

        // Fires sub_98E310 with verbatim world coordinates.
        inline bool SendChargePacket(CoreCastSpell::CastTrace& trace,
                                     std::int32_t slotIndex,
                                     const Vec3& position,
                                     bool release,
                                     std::int64_t& nativeResult) {
            const uintptr_t updateChargeFn = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::UpdateChargeableSpell);
            trace.castSpellSafe = updateChargeFn;
            if (!Globals::IsValidPtr(updateChargeFn)) {
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

            __try {
                NightSharpDebug::CrashTrace::Record(
                    nscrash::TraceTag::SpellCast,
                    static_cast<std::uint64_t>(trace.slot),
                    static_cast<std::uint64_t>(trace.kind));
                trace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;

                nativeResult = spoof_call(
                    reinterpret_cast<void*>(trace.spoofTrampoline),
                    reinterpret_cast<FnUpdateChargeableSpell>(updateChargeFn),
                    trace.spellbook,
                    trace.spellSlot,
                    slotIndex,
                    &nativePosition,
                    static_cast<std::uint8_t>(release ? 1 : 0));
            }
            __except (1) {
                nativeException = true;
            }

            CoreCastSpell::detail::ClearBypassFlag(bypassTouched);

            if (nativeException) {
                Fail(CoreCastSpell::CastFailure::NativeException);
                return false;
            }
            return true;
        }

        // Opens the cast the way HudSpellHandler case 5 does, minus the cursor:
        // prime the provider ourselves, CastSpellSafe, then InitChargeState.
        // sub_BE2E20 (the HUD's "select pending spell") is deliberately skipped —
        // besides writing castContext + 0x08 it builds the spell indicator, which is
        // the overlay that visibly tracks the mouse while a charge is held.
        inline bool OpenChargeCast(CoreCastSpell::CastTrace& trace,
                                   const Vec3& position,
                                   std::int64_t& nativeResult) {
            const uintptr_t castFn = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::CastSpellSafe);
            const uintptr_t initChargeFn = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::InitChargeState);
            if (!Globals::IsValidPtr(castFn) ||
                !Globals::IsValidPtr(initChargeFn)) {
                Fail(CoreCastSpell::CastFailure::MissingNativeFunction);
                return false;
            }

            const uintptr_t provider = ResolveProvider(trace.spellSlot);
            if (!Globals::IsValidPtr(provider)) {
                Fail(CoreCastSpell::CastFailure::MissingSpellInput);
                return false;
            }
            trace.runtimeInput = provider;

            const std::uint32_t sourceObjectIndex = Globals::Read<std::uint32_t>(
                trace.localPlayer + Offset::All::Index);

            TargetProviderIds originalIds{};
            TargetProviderVectors originalVectors{};
            if (!PrimeProvider(provider,
                               sourceObjectIndex,
                               0,
                               trace.startPosition,
                               position,
                               originalIds,
                               originalVectors)) {
                Fail(CoreCastSpell::CastFailure::MissingSpellInput);
                return false;
            }

            bool nativeException = false;
            bool bypassTouched = false;

            __try {
                NightSharpDebug::CrashTrace::Record(
                    nscrash::TraceTag::SpellCast,
                    static_cast<std::uint64_t>(trace.slot),
                    static_cast<std::uint64_t>(CoreCastSpell::CastKind::ChargeBegin));
                trace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;

                nativeResult = spoof_call(
                    reinterpret_cast<void*>(trace.spoofTrampoline),
                    reinterpret_cast<CoreCastSpell::FnCastSpellSafe>(castFn),
                    trace.castContext,
                    trace.spellInput);

                spoof_call(
                    reinterpret_cast<void*>(trace.spoofTrampoline),
                    reinterpret_cast<FnInitChargeState>(initChargeFn),
                    trace.castContext,
                    trace.spellInput);
            }
            __except (1) {
                nativeException = true;
            }

            CoreCastSpell::detail::ClearBypassFlag(bypassTouched);
            RestoreProvider(provider, originalIds, originalVectors);

            if (nativeException) {
                Fail(CoreCastSpell::CastFailure::NativeException);
                return false;
            }
            return true;
        }

        inline bool BeginChargeSpellMethod1(std::uint8_t slot,
                                            const Vec3& position) {
            if (!CoreCastSpell::detail::ResolveCommon(
                    CoreCastSpell::CastKind::ChargeBegin,
                    slot)) {
                return false;
            }

            auto& trace = CoreCastSpell::g_lastTrace;
            trace.endPosition = position;
            trace.canCastCheck = 0;
            trace.virtualCursorApplied = false;
            trace.virtualCursorScreen = {};

            if (!RequireCastContext(trace)) {
                return false;
            }

            if (!position.IsValid() || position.IsZero()) {
                Fail(CoreCastSpell::CastFailure::InvalidPosition);
                return false;
            }

            // Already charging this slot: this call is a re-aim, not a new cast.
            // Re-aiming must stay unthrottled so callers can track a moving
            // prediction every tick.
            const bool alreadyCharging =
                ReadChargeInput(trace.castContext) == trace.spellInput;

            // No rate limiter here. The charge state itself is the guard: a second
            // begin cannot slip through while castContext + 0x38 is set, and once
            // the charge is released the spell's own cooldown gates the next one.
            // Borrowing CoreCastSpell's 120 ms fire-cast throttle would instead
            // swallow legitimate back-to-back casts whenever the cooldown is short.
            if (!alreadyCharging) {
                std::int64_t openResult = 0;
                if (!OpenChargeCast(trace, position, openResult)) {
                    return false;
                }

                if (ReadChargeInput(trace.castContext) != trace.spellInput) {
                    Fail(CoreCastSpell::CastFailure::MissingChargeState);
                    return false;
                }
            }

            const std::int32_t slotIndex =
                ResolveSlotIndex(trace, trace.spellInput, slot);

            std::int64_t nativeResult = 0;
            if (!SendChargePacket(trace, slotIndex, position, false, nativeResult)) {
                return false;
            }
            SendChargeAim(trace, position);

            trace.canCastAccepted = true;
            trace.nativeResult = nativeResult;
            trace.failure = CoreCastSpell::CastFailure::None;
            trace.success = true;
            CoreValidation::MarkCastResult(true);
            return true;
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
            trace.canCastCheck = 0;
            trace.virtualCursorApplied = false;
            trace.virtualCursorScreen = {};

            if (!RequireCastContext(trace)) {
                return false;
            }

            if (!position.IsValid() || position.IsZero()) {
                Fail(CoreCastSpell::CastFailure::InvalidPosition);
                return false;
            }

            const uintptr_t chargeInput = ReadChargeInput(trace.castContext);
            if (chargeInput != trace.spellInput) {
                Fail(CoreCastSpell::CastFailure::MissingChargeState);
                return false;
            }
            trace.runtimeInput = chargeInput;

            const std::int32_t slotIndex =
                ResolveSlotIndex(trace, chargeInput, slot);

            // Land the aim first so the last thing the server hears before the
            // release is our position, not whatever the cursor stream just sent.
            SendChargeAim(trace, position);

            std::int64_t nativeResult = 0;
            if (!SendChargePacket(trace, slotIndex, position, true, nativeResult)) {
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

        // Target cast. CanCastCheck's explicit-target branch (a3 != 0) is the only
        // native path that resolves a target without raycasting the cursor, but it
        // rejects most spells up front — sub_925DD0(slot) has to report 1 or 6.
        // So instead of asking it for permission, prime the provider with the
        // source/target object indices plus the target's position and let
        // CastSpellSafe build the packet from that.
        inline bool CastTargetSpellMethod2(std::uint8_t slot,
                                           uintptr_t target) {
            if (!CoreCastSpell::detail::ResolveCommon(
                    CoreCastSpell::CastKind::Target,
                    slot)) {
                return false;
            }

            auto& trace = CoreCastSpell::g_lastTrace;
            trace.virtualCursorApplied = false;
            trace.virtualCursorScreen = {};

            if (!RequireCastContext(trace)) {
                return false;
            }

            if (!Globals::IsValidPtr(target)) {
                Fail(CoreCastSpell::CastFailure::InvalidTarget);
                return false;
            }

            if (!CoreCastSpell::detail::ThrottleReserve(slot)) {
                Fail(CoreCastSpell::CastFailure::Throttled);
                return false;
            }

            const std::uint32_t sourceObjectIndex = Globals::Read<std::uint32_t>(
                trace.localPlayer + Offset::All::Index);
            trace.targetNetworkId = Globals::Read<std::uint32_t>(
                target + Offset::All::NetworkId);
            trace.targetObjectIndex = Globals::Read<std::uint32_t>(
                target + Offset::All::Index);
            trace.endPosition = Globals::Read<Vec3>(
                target + Offset::All::Position);

            if (trace.targetNetworkId == 0 ||
                trace.targetNetworkId == 0xFFFFFFFFu ||
                trace.targetObjectIndex == 0 ||
                !trace.endPosition.IsValid()) {
                Fail(CoreCastSpell::CastFailure::InvalidTarget);
                return false;
            }

            // Diagnostics only — the cast never consults the HUD picker.
            if (Globals::IsValidPtr(trace.hud)) {
                const uintptr_t targeting = Globals::Read<uintptr_t>(
                    trace.hud + Offset::HudRuntime::SpellTargeting);
                if (Globals::IsValidPtr(targeting)) {
                    trace.hudTargetState = Globals::Read<std::uint32_t>(
                        targeting + Offset::HudSpellTargetingLayout::State);
                    trace.hudTargetObjectIndex = Globals::Read<std::uint32_t>(
                        targeting + Offset::HudSpellTargetingLayout::ObjectIndex);
                }
            }

            const uintptr_t provider = ResolveProvider(trace.spellSlot);
            if (!Globals::IsValidPtr(provider)) {
                Fail(CoreCastSpell::CastFailure::MissingSpellInput);
                return false;
            }

            const uintptr_t castFn = CoreRuntime::ResolveRva(
                Offset::ControlRuntime::CastSpellSafe);
            trace.castSpellSafe = castFn;
            trace.canCastCheck = 0;
            trace.runtimeInput = provider;
            if (!Globals::IsValidPtr(castFn)) {
                Fail(CoreCastSpell::CastFailure::MissingNativeFunction);
                return false;
            }

            TargetProviderIds originalIds{};
            TargetProviderVectors originalVectors{};
            if (!PrimeProvider(provider,
                               sourceObjectIndex,
                               trace.targetObjectIndex,
                               trace.startPosition,
                               trace.endPosition,
                               originalIds,
                               originalVectors)) {
                Fail(CoreCastSpell::CastFailure::MissingSpellInput);
                return false;
            }

            bool nativeException = false;
            bool bypassTouched = false;
            std::int64_t nativeResult = 0;

            __try {
                NightSharpDebug::CrashTrace::Record(
                    nscrash::TraceTag::SpellCast,
                    static_cast<std::uint64_t>(slot),
                    static_cast<std::uint64_t>(CoreCastSpell::CastKind::Target));
                trace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;

                nativeResult = spoof_call(
                    reinterpret_cast<void*>(trace.spoofTrampoline),
                    reinterpret_cast<CoreCastSpell::FnCastSpellSafe>(castFn),
                    trace.castContext,
                    trace.spellInput);
            }
            __except (1) {
                nativeException = true;
            }

            CoreCastSpell::detail::ClearBypassFlag(bypassTouched);
            RestoreProvider(provider, originalIds, originalVectors);

            if (nativeException) {
                Fail(CoreCastSpell::CastFailure::NativeException);
                return false;
            }

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

    // castContext while the client still believes this slot is charging, else 0.
    // This is the only trustworthy charge signal: the champion buff outlives the
    // release by a few frames, so gating on the buff makes the next input take the
    // "already charging" branch and fail with missing-charge-state.
    inline uintptr_t ChargingContext(std::int32_t slot) {
        if (slot < 0 || slot > CoreCastSpell::SlotSummonerF) {
            return 0;
        }

        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(ctx.localPlayer) ||
            !Globals::IsValidPtr(ctx.hudInstance)) {
            return 0;
        }

        const uintptr_t castContext = Globals::Read<uintptr_t>(
            ctx.hudInstance + Offset::HudRuntime::SpellInfo);
        const uintptr_t chargeInput = detail::ReadChargeInput(castContext);
        if (!Globals::IsValidPtr(chargeInput)) {
            return 0;
        }

        const uintptr_t spellSlot = CoreCastSpell::detail::ResolveSpellSlot(
            ctx.localPlayer + Offset::SpellRuntime::SpellBookOffset,
            static_cast<std::uint8_t>(slot));
        if (!Globals::IsValidPtr(spellSlot)) {
            return 0;
        }

        if (chargeInput != Globals::Read<uintptr_t>(
                spellSlot + Offset::SpellSlotLayout::SlotSpellInput)) {
            return 0;
        }

        return castContext;
    }

    inline bool IsCharging(std::int32_t slot) {
        return ChargingContext(slot) != 0;
    }

    // Game time at which the current charge opened, or 0 when that cannot be
    // established. Lets a caller recover how long a charge someone else started —
    // the player's own Q key, say — has already been held.
    //
    // Reads +0x5C rather than +0x58: the end-of-charge reset leaves +0x54/+0x58 at
    // the FLT_MAX sentinel, and FLT_MAX passes an isfinite() check, so trusting
    // those fields yields a nonsense timestamp. The result is range-checked against
    // the clock for the same reason — a sentinel or a stale value from an earlier
    // charge must report "unknown" instead of a bogus elapsed time.
    inline float ChargeStartTime(std::int32_t slot) {
        const uintptr_t castContext = ChargingContext(slot);
        if (castContext == 0) {
            return 0.0f;
        }

        const float startTime = Globals::Read<float>(
            castContext + detail::kChargeStartTime);
        const float now = CoreRuntime::GetContext().gameTime;
        if (!std::isfinite(startTime) || startTime <= 0.0f || now <= 0.0f) {
            return 0.0f;
        }
        if (startTime > now || (now - startTime) > 10.0f) {
            return 0.0f;
        }
        return startTime;
    }

    inline bool BeginChargeSpell(std::uint8_t slot, const Vec3& position) {
        return detail::BeginChargeSpellMethod1(slot, position);
    }

    inline bool ReleaseChargeSpellMethod1(std::uint8_t slot,
                                          const Vec3& position) {
        return detail::ReleaseChargeSpellMethod1(slot, position);
    }

    inline bool CastTargetSpellMethod2(std::int32_t slot, uintptr_t target) {
        if (slot < 0 || slot > CoreCastSpell::SlotSummonerF) {
            CoreCastSpell::g_lastTrace = {};
            CoreCastSpell::g_lastTrace.kind = CoreCastSpell::CastKind::Target;
            CoreCastSpell::g_lastTrace.slot =
                static_cast<std::uint8_t>(slot < 0 ? 0 : slot);
            detail::Fail(CoreCastSpell::CastFailure::InvalidSlot);
            return false;
        }

        detail::ScopedWritePhase writePhase;
        return detail::CastTargetSpellMethod2(
            static_cast<std::uint8_t>(slot),
            target);
    }

    // Charge entry point. releaseCast = false either opens the charge or, when the
    // slot is already charging, just re-aims it; releaseCast = true fires it.
    inline bool UpdateChargedSpellMethod1(std::int32_t slot,
                                          const Vec3& position,
                                          bool releaseCast) {
        if (slot < 0 || slot > CoreCastSpell::SlotSummonerF) {
            CoreCastSpell::g_lastTrace = {};
            CoreCastSpell::g_lastTrace.kind = releaseCast
                ? CoreCastSpell::CastKind::ChargeRelease
                : CoreCastSpell::CastKind::ChargeBegin;
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
