#pragma once

#include "CoreBypass.h"
#include "CoreEvadeState.h"
#include "CoreRuntime.h"
#include "CoreValidation.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"
#include "spoof/spoofcall.h"

#include <chrono>
#include <cmath>
#include <cstdint>

namespace CoreCastSpell {

    inline constexpr std::uint8_t SlotQ = 0;
    inline constexpr std::uint8_t SlotW = 1;
    inline constexpr std::uint8_t SlotE = 2;
    inline constexpr std::uint8_t SlotR = 3;
    inline constexpr std::uint8_t SlotSummonerD = 4;
    inline constexpr std::uint8_t SlotSummonerF = 5;

    enum class CastKind : std::uint8_t {
        Position,
        Target,
        Self,
        Vector,
        ChargeBegin,
        ChargeRelease,
    };

    enum class CastFailure : std::uint8_t {
        None,
        NotWritePhase,
        RuntimeUnavailable,
        InvalidSlot,
        InvalidPosition,
        InvalidTarget,
        MissingSpellbook,
        MissingSpellSlot,
        MissingSpellInput,
        MissingHud,
        MissingCastContext,
        MissingNativeFunction,
        MissingSpoofTrampoline,
        MissingChargeState,
        MissingMouseInput,
        PositionProjectionFailed,
        CanCastRejected,
        NativeException,
        Throttled,
    };

    struct CastTrace {
        CastKind kind = CastKind::Position;
        CastFailure failure = CastFailure::None;
        bool success = false;
        bool bypassPrepared = false;
        bool canCastAccepted = false;
        bool virtualCursorApplied = false;
        std::uint8_t slot = 0;
        std::uint32_t targetNetworkId = 0;
        std::uint32_t targetObjectIndex = 0;
        std::uint32_t hudTargetObjectIndex = 0;
        std::uint32_t hudTargetState = 0;
        uintptr_t localPlayer = 0;
        uintptr_t spellbook = 0;
        uintptr_t spellSlot = 0;
        uintptr_t spellInput = 0;
        uintptr_t runtimeInput = 0;
        uintptr_t hud = 0;
        uintptr_t castContext = 0;
        uintptr_t canCastCheck = 0;
        uintptr_t castSpellSafe = 0;
        uintptr_t spoofTrampoline = 0;
        uintptr_t worldToScreenFn = 0;
        uintptr_t worldToScreenContext = 0;
        std::int64_t nativeResult = 0;
        Vec3 startPosition = {};
        Vec3 endPosition = {};
        Vec2 virtualCursorScreen = {};
    };

    inline CastTrace g_lastTrace = {};

    using FnCanCastCheck = bool(__fastcall*)(
        uintptr_t castContext,
        uintptr_t spellInput,
        std::uint32_t targetNetworkId);

    using FnCastSpellSafe = std::int64_t(__fastcall*)(
        uintptr_t castContext,
        uintptr_t spellInput);

    using FnHudSpellHandler = void(__fastcall*)(
        uintptr_t castContext,
        std::int64_t slotIndex,
        std::int32_t castMode,
        std::int64_t keyState);

    // Native two-position (vector) cast — sub_97A980. Reads x/z from each Vec3.
    struct NativeVec3Xyz { float x, y, z; };
    using FnCastSpellVector = void(__fastcall*)(
        uintptr_t spellbook,
        uintptr_t slotObject,
        std::int32_t slot,
        const NativeVec3Xyz* startPosition,
        const NativeVec3Xyz* endPosition,
        std::int32_t visionFlag);

    inline constexpr std::int32_t kHudModeSmartCast = 2;
    inline constexpr std::int64_t kHudKeyPress = 1;
    inline constexpr std::int64_t kHudKeyRelease = 2;
    inline constexpr uintptr_t kHudChargeSpellInput = 0x38;

    // Minimum interval between fire-casts of the same slot, so holding a combo key
    // (e.g. space) doesn't spam the native cast every frame. Charge begin/release
    // are exempt (they have their own state gating). Matches the SDK Spell wrapper.
    inline constexpr int kCastThrottleMs = 120;

    inline const CastTrace& LastTrace() {
        return g_lastTrace;
    }

    inline const char* CastKindName(CastKind kind) {
        switch (kind) {
        case CastKind::Position: return "position";
        case CastKind::Target: return "target";
        case CastKind::Self: return "self";
        case CastKind::Vector: return "vector";
        case CastKind::ChargeBegin: return "charge-begin";
        case CastKind::ChargeRelease: return "charge-release";
        default: return "unknown";
        }
    }

    inline const char* CastFailureName(CastFailure failure) {
        switch (failure) {
        case CastFailure::None: return "none";
        case CastFailure::NotWritePhase: return "not-write-phase";
        case CastFailure::RuntimeUnavailable: return "runtime-unavailable";
        case CastFailure::InvalidSlot: return "invalid-slot";
        case CastFailure::InvalidPosition: return "invalid-position";
        case CastFailure::InvalidTarget: return "invalid-target";
        case CastFailure::MissingSpellbook: return "missing-spellbook";
        case CastFailure::MissingSpellSlot: return "missing-spell-slot";
        case CastFailure::MissingSpellInput: return "missing-spell-input";
        case CastFailure::MissingHud: return "missing-hud";
        case CastFailure::MissingCastContext: return "missing-cast-context";
        case CastFailure::MissingNativeFunction: return "missing-native-function";
        case CastFailure::MissingSpoofTrampoline: return "missing-spoof-trampoline";
        case CastFailure::MissingChargeState: return "missing-charge-state";
        case CastFailure::MissingMouseInput: return "missing-mouse-input";
        case CastFailure::PositionProjectionFailed: return "position-projection-failed";
        case CastFailure::CanCastRejected: return "can-cast-rejected";
        case CastFailure::NativeException: return "native-exception";
        case CastFailure::Throttled: return "throttled";
        default: return "unknown";
        }
    }

    namespace detail {

        inline void Fail(CastFailure failure);

        class ScopedVirtualCursor {
        public:
            bool Apply(const Vec3& worldPosition) {
                const auto& ctx = CoreRuntime::GetContext();
                if (!Globals::IsValidPtr(ctx.mouseScreenVec2)) {
                    Fail(CastFailure::MissingMouseInput);
                    return false;
                }

                Vec2 screen{};
                if (!WorldToScreen(worldPosition, screen)) {
                    Fail(CastFailure::PositionProjectionFailed);
                    return false;
                }

                const auto x = static_cast<std::int32_t>(std::lround(screen.x));
                const auto y = static_cast<std::int32_t>(std::lround(screen.y));
                address_ = ctx.mouseScreenVec2;
                originalX_ = Globals::Read<std::int32_t>(
                    address_ + Offset::MouseInputLayout::ScreenX);
                originalY_ = Globals::Read<std::int32_t>(
                    address_ + Offset::MouseInputLayout::ScreenY);
                applied_ = true;

                if (!Globals::Write<std::int32_t>(
                    address_ + Offset::MouseInputLayout::ScreenX, x) ||
                    !Globals::Write<std::int32_t>(
                        address_ + Offset::MouseInputLayout::ScreenY, y)) {
                    Restore();
                    Fail(CastFailure::MissingMouseInput);
                    return false;
                }

                g_lastTrace.virtualCursorApplied = true;
                g_lastTrace.virtualCursorScreen = screen;
                return true;
            }

            void Restore() {
                if (!applied_ || !Globals::IsValidPtr(address_)) {
                    return;
                }

                (void)Globals::Write<std::int32_t>(
                    address_ + Offset::MouseInputLayout::ScreenX, originalX_);
                (void)Globals::Write<std::int32_t>(
                    address_ + Offset::MouseInputLayout::ScreenY, originalY_);
                applied_ = false;
            }

        private:
            static bool WorldToScreen(const Vec3& world, Vec2& screen) {
                screen = {};
                g_lastTrace.worldToScreenFn =
                    CoreRuntime::GetContext().worldToScreenFn;
                const uintptr_t rootGlobal = CoreRuntime::ResolveRva(
                    Offset::DrawingRuntime::ViewProjectionRoot);
                const uintptr_t root =
                    Globals::Read<uintptr_t>(rootGlobal);
                g_lastTrace.worldToScreenContext =
                    Globals::IsValidPtr(root)
                    ? root +
                    Offset::DrawingRuntime::WorldToScreenContextOffset
                    : 0;

                if (!Globals::IsValidPtr(g_lastTrace.worldToScreenFn) ||
                    !Globals::IsValidPtr(g_lastTrace.worldToScreenContext) ||
                    !world.IsValid()) {
                    return false;
                }

                struct NativeVec3 {
                    float x;
                    float y;
                    float z;
                };

                __try {
                    NativeVec3 input{ world.x, world.y, world.z };
                    NativeVec3 output{};
                    using Fn = bool(__fastcall*)(
                        uintptr_t,
                        const NativeVec3*,
                        NativeVec3*);
                    const bool ok = reinterpret_cast<Fn>(
                        g_lastTrace.worldToScreenFn)(
                            g_lastTrace.worldToScreenContext,
                            &input,
                            &output);
                    if (!std::isfinite(output.x) ||
                        !std::isfinite(output.y)) {
                        return false;
                    }
                    screen = { output.x, output.y };
                    return ok || !screen.IsZero();
                }
                __except (1) {
                    screen = {};
                    return false;
                }
            }

            uintptr_t address_ = 0;
            std::int32_t originalX_ = 0;
            std::int32_t originalY_ = 0;
            bool applied_ = false;
        };

        class ScopedRuntimeSpellInput {
        public:
            bool Apply(uintptr_t input,
                const Vec3& startPosition,
                const Vec3& endPosition,
                std::uint32_t targetNetworkId) {
                if (!Globals::IsValidPtr(input)) {
                    Fail(CastFailure::MissingSpellInput);
                    return false;
                }

                address_ = input;
                originalTargetNetworkId_ = Globals::Read<std::uint32_t>(
                    address_ + Offset::SpellInputLayout::InputTargetNetId);
                originalStartPosition_ = Globals::Read<Vec3>(
                    address_ + Offset::SpellInputLayout::InputStartPos);
                originalEndPosition_ = Globals::Read<Vec3>(
                    address_ + Offset::SpellInputLayout::InputEndPos);
                originalEndPosition2_ = Globals::Read<Vec3>(
                    address_ + Offset::SpellInputLayout::InputEndPos + sizeof(Vec3));
                originalEndPosition3_ = Globals::Read<Vec3>(
                    address_ + Offset::SpellInputLayout::InputEndPos + sizeof(Vec3) * 2);
                applied_ = true;

                bool ok = true;
                ok &= Globals::Write<std::uint32_t>(
                    address_ + Offset::SpellInputLayout::InputTargetNetId,
                    targetNetworkId);
                ok &= Globals::Write<Vec3>(
                    address_ + Offset::SpellInputLayout::InputStartPos,
                    startPosition);
                ok &= Globals::Write<Vec3>(
                    address_ + Offset::SpellInputLayout::InputEndPos,
                    endPosition);
                ok &= Globals::Write<Vec3>(
                    address_ + Offset::SpellInputLayout::InputEndPos + sizeof(Vec3),
                    endPosition);
                ok &= Globals::Write<Vec3>(
                    address_ + Offset::SpellInputLayout::InputEndPos + sizeof(Vec3) * 2,
                    endPosition);

                if (!ok) {
                    Restore();
                    Fail(CastFailure::MissingSpellInput);
                    return false;
                }

                return true;
            }

            void Restore() {
                if (!applied_ || !Globals::IsValidPtr(address_)) {
                    return;
                }

                (void)Globals::Write<std::uint32_t>(
                    address_ + Offset::SpellInputLayout::InputTargetNetId,
                    originalTargetNetworkId_);
                (void)Globals::Write<Vec3>(
                    address_ + Offset::SpellInputLayout::InputStartPos,
                    originalStartPosition_);
                (void)Globals::Write<Vec3>(
                    address_ + Offset::SpellInputLayout::InputEndPos,
                    originalEndPosition_);
                (void)Globals::Write<Vec3>(
                    address_ + Offset::SpellInputLayout::InputEndPos + sizeof(Vec3),
                    originalEndPosition2_);
                (void)Globals::Write<Vec3>(
                    address_ + Offset::SpellInputLayout::InputEndPos + sizeof(Vec3) * 2,
                    originalEndPosition3_);
                applied_ = false;
            }

        private:
            uintptr_t address_ = 0;
            std::uint32_t originalTargetNetworkId_ = 0;
            Vec3 originalStartPosition_ = {};
            Vec3 originalEndPosition_ = {};
            Vec3 originalEndPosition2_ = {};
            Vec3 originalEndPosition3_ = {};
            bool applied_ = false;
        };

        inline bool IsSupportedSlot(std::uint8_t slot) {
            return slot <= SlotSummonerF;
        }

        inline int NowMs() {
            return static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                .count());
        }

        // Per-slot fire-cast rate limiter (delay action). Reserve-style: returns false
        // if the same slot fired within kCastThrottleMs, otherwise records `now` and
        // returns true. Prevents cast spam when a combo key (space) is held down.
        inline bool ThrottleReserve(std::uint8_t slot) {
            static int lastTick[8] = {};
            const int idx = slot < 8 ? slot : 7;
            const int now = NowMs();
            if (lastTick[idx] != 0 && now - lastTick[idx] < kCastThrottleMs) {
                return false;
            }
            lastTick[idx] = now;
            return true;
        }

        inline void Fail(CastFailure failure) {
            g_lastTrace.failure = failure;
            g_lastTrace.success = false;
            CoreValidation::MarkCastResult(false);
        }

        inline uintptr_t ResolveSpellSlot(uintptr_t spellbook, std::uint8_t slot) {
            if (!Globals::IsValidPtr(spellbook)) {
                return 0;
            }

            return Globals::Read<uintptr_t>(
                spellbook +
                Offset::SpellBookLayout::SpellSlotArray +
                static_cast<uintptr_t>(slot) * sizeof(uintptr_t));
        }

        inline bool ResolveCommon(CastKind kind, std::uint8_t slot) {
            g_lastTrace = {};
            g_lastTrace.kind = kind;
            g_lastTrace.slot = slot;

            if (!CoreRuntime::IsWritePhase()) {
                Fail(CastFailure::NotWritePhase);
                return false;
            }

            if (!CoreRuntime::EnsureInitialized() || !CoreRuntime::RefreshReadState()) {
                Fail(CastFailure::RuntimeUnavailable);
                return false;
            }

            if (!IsSupportedSlot(slot)) {
                Fail(CastFailure::InvalidSlot);
                return false;
            }

            const auto& ctx = CoreRuntime::GetContext();
            if (CoreEvadeState::AreSpellCastsBlocked(static_cast<int>(ctx.gameTime * 1000.0f))) {
                Fail(CastFailure::CanCastRejected);
                return false;
            }
            g_lastTrace.localPlayer = ctx.localPlayer;
            if (!Globals::IsValidPtr(g_lastTrace.localPlayer)) {
                Fail(CastFailure::RuntimeUnavailable);
                return false;
            }

            g_lastTrace.startPosition =
                Globals::Read<Vec3>(g_lastTrace.localPlayer + Offset::All::Position);

            g_lastTrace.spellbook =
                g_lastTrace.localPlayer + Offset::SpellRuntime::SpellBookOffset;
            if (!Globals::IsValidPtr(g_lastTrace.spellbook)) {
                Fail(CastFailure::MissingSpellbook);
                return false;
            }

            g_lastTrace.spellSlot = ResolveSpellSlot(g_lastTrace.spellbook, slot);
            if (!Globals::IsValidPtr(g_lastTrace.spellSlot)) {
                Fail(CastFailure::MissingSpellSlot);
                return false;
            }

            g_lastTrace.spellInput = Globals::Read<uintptr_t>(
                g_lastTrace.spellSlot + Offset::SpellSlotLayout::SlotSpellInput);
            if (!Globals::IsValidPtr(g_lastTrace.spellInput)) {
                Fail(CastFailure::MissingSpellInput);
                return false;
            }

            g_lastTrace.hud = ctx.hudInstance;
            if (!Globals::IsValidPtr(g_lastTrace.hud)) {
                Fail(CastFailure::MissingHud);
                return false;
            }

            g_lastTrace.castContext = Globals::Read<uintptr_t>(
                g_lastTrace.hud + Offset::HudRuntime::SpellInfo);
            if (!Globals::IsValidPtr(g_lastTrace.castContext)) {
                Fail(CastFailure::MissingCastContext);
                return false;
            }

            g_lastTrace.spoofTrampoline = CoreBypass::ResolveSpoofTrampoline();
            if (!Globals::IsValidPtr(g_lastTrace.spoofTrampoline)) {
                Fail(CastFailure::MissingSpoofTrampoline);
                return false;
            }

            return true;
        }

        inline uintptr_t ResolveRuntimeSpellInput() {
            const uintptr_t resolveOwnerSlot =
                CoreRuntime::ResolveRva(Offset::ControlRuntime::FindOwnerSlot);
            if (!Globals::IsValidPtr(resolveOwnerSlot)) {
                Fail(CastFailure::MissingNativeFunction);
                return 0;
            }

            uintptr_t runtimeSlot = 0;
            __try {
                using FnResolveOwnerSlot = uintptr_t(__fastcall*)(uintptr_t, uintptr_t);
                runtimeSlot = spoof_call(
                    reinterpret_cast<void*>(g_lastTrace.spoofTrampoline),
                    reinterpret_cast<FnResolveOwnerSlot>(resolveOwnerSlot),
                    g_lastTrace.castContext,
                    g_lastTrace.spellInput);
            }
            __except (1) {
                runtimeSlot = 0;
            }

            if (!Globals::IsValidPtr(runtimeSlot)) {
                Fail(CastFailure::MissingSpellSlot);
                return 0;
            }

            const uintptr_t runtimeInput = Globals::Read<uintptr_t>(
                runtimeSlot + Offset::SpellSlotLayout::SlotSpellInfo);
            g_lastTrace.runtimeInput = runtimeInput;
            if (!Globals::IsValidPtr(runtimeInput) ||
                runtimeInput == g_lastTrace.spellInput) {
                Fail(CastFailure::MissingSpellInput);
                return 0;
            }

            return runtimeInput;
        }

        inline void ClearBypassFlag(bool bypassTouched) {
            if (!bypassTouched) {
                return;
            }

            __try {
                CoreBypass::ClearCastSpellFlag();
            }
            __except (1) {}
        }

        inline bool CastValidated(CastKind kind,
            std::uint8_t slot,
            const Vec3& endPosition,
            uintptr_t target) {
            if (!ResolveCommon(kind, slot)) {
                return false;
            }

            if (!ThrottleReserve(slot)) {
                Fail(CastFailure::Throttled);
                return false;
            }

            if (kind == CastKind::Self) {
                g_lastTrace.endPosition = g_lastTrace.startPosition;
            }
            else if (kind == CastKind::Target) {
                if (!Globals::IsValidPtr(target)) {
                    Fail(CastFailure::InvalidTarget);
                    return false;
                }

                g_lastTrace.targetNetworkId =
                    Globals::Read<std::uint32_t>(target + Offset::All::NetworkId);
                g_lastTrace.endPosition =
                    Globals::Read<Vec3>(target + Offset::All::Position);

                if (g_lastTrace.targetNetworkId == 0 ||
                    g_lastTrace.targetNetworkId == 0xFFFFFFFFu) {
                    Fail(CastFailure::InvalidTarget);
                    return false;
                }
            }
            else {
                g_lastTrace.endPosition = endPosition;
                if (!endPosition.IsValid()) {
                    Fail(CastFailure::InvalidPosition);
                    return false;
                }
            }

            g_lastTrace.canCastCheck =
                CoreRuntime::ResolveRva(Offset::ControlRuntime::CanCastCheck);
            g_lastTrace.castSpellSafe =
                CoreRuntime::ResolveRva(Offset::ControlRuntime::CastSpellSafe);
            if (!Globals::IsValidPtr(g_lastTrace.canCastCheck) ||
                !Globals::IsValidPtr(g_lastTrace.castSpellSafe)) {
                Fail(CastFailure::MissingNativeFunction);
                return false;
            }

            bool nativeException = false;
            bool bypassTouched = false;
            ScopedVirtualCursor virtualCursor;
            if (kind == CastKind::Position &&
                !virtualCursor.Apply(g_lastTrace.endPosition)) {
                return false;
            }

            __try {
                g_lastTrace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;

                g_lastTrace.canCastAccepted = spoof_call(
                    reinterpret_cast<void*>(g_lastTrace.spoofTrampoline),
                    reinterpret_cast<FnCanCastCheck>(g_lastTrace.canCastCheck),
                    g_lastTrace.castContext,
                    g_lastTrace.spellInput,
                    g_lastTrace.targetNetworkId);

                if (g_lastTrace.canCastAccepted) {
                    g_lastTrace.nativeResult = spoof_call(
                        reinterpret_cast<void*>(g_lastTrace.spoofTrampoline),
                        reinterpret_cast<FnCastSpellSafe>(g_lastTrace.castSpellSafe),
                        g_lastTrace.castContext,
                        g_lastTrace.spellInput);
                }
            }
            __except (1) {
                nativeException = true;
            }

            ClearBypassFlag(bypassTouched);
            virtualCursor.Restore();

            if (nativeException) {
                Fail(CastFailure::NativeException);
                return false;
            }

            if (!g_lastTrace.canCastAccepted) {
                Fail(CastFailure::CanCastRejected);
                return false;
            }

            g_lastTrace.failure = CastFailure::None;
            g_lastTrace.success = true;
            CoreValidation::MarkCastResult(true);
            return true;
        }

        // Two-position (vector) cast — Viktor E. Calls the game's native
        // CastSpellVector (sub_97A980) directly with explicit world start/end coords.
        // This is the native entry EnsoulSharp's Spellbook.CastSpell(slot, start, end)
        // forwards to: it builds the opcode-271 cast packet itself and does NOT go
        // through CanCastCheck, so it isn't rejected like the old runtime-input path.
        // Coordinates are sent verbatim (no cursor raycast), so aim is exact.
        inline bool DispatchVector(std::uint8_t slot,
            const Vec3& startPosition,
            const Vec3& endPosition) {
            if (!ResolveCommon(CastKind::Vector, slot)) {
                return false;
            }

            if (!ThrottleReserve(slot)) {
                Fail(CastFailure::Throttled);
                return false;
            }

            if (!startPosition.IsValid() || !endPosition.IsValid() ||
                startPosition.IsZero() || endPosition.IsZero()) {
                Fail(CastFailure::InvalidPosition);
                return false;
            }

            g_lastTrace.startPosition = startPosition;
            g_lastTrace.endPosition = endPosition;

            const uintptr_t castFn =
                CoreRuntime::ResolveRva(Offset::ControlRuntime::CastSpellVector);
            if (!Globals::IsValidPtr(castFn)) {
                Fail(CastFailure::MissingNativeFunction);
                return false;
            }
            g_lastTrace.castSpellSafe = castFn;

            // Default fog/vision index the normal cast path passes when there is no
            // special vision source (sub_97A0B0's `dword_1F18A30` fallback).
            const std::int32_t visionFlag = Globals::Read<std::int32_t>(
                CoreRuntime::ResolveRva(Offset::ControlRuntime::CastVisionIndexDefault));

            const NativeVec3Xyz start{ startPosition.x, startPosition.y, startPosition.z };
            const NativeVec3Xyz end{ endPosition.x, endPosition.y, endPosition.z };

            bool nativeException = false;
            bool bypassTouched = false;

            __try {
                g_lastTrace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;

                spoof_call(
                    reinterpret_cast<void*>(g_lastTrace.spoofTrampoline),
                    reinterpret_cast<FnCastSpellVector>(castFn),
                    g_lastTrace.spellbook,   // book (localPlayer + SpellBookOffset)
                    g_lastTrace.spellSlot,   // slotObj = GetSpellSlot(book, slot)
                    static_cast<std::int32_t>(slot),
                    &start,
                    &end,
                    visionFlag);

                g_lastTrace.canCastAccepted = true;
                g_lastTrace.nativeResult = 1;
            }
            __except (1) {
                nativeException = true;
            }

            ClearBypassFlag(bypassTouched);

            if (nativeException) {
                Fail(CastFailure::NativeException);
                return false;
            }

            g_lastTrace.failure = CastFailure::None;
            g_lastTrace.success = true;
            CoreValidation::MarkCastResult(true);
            return true;
        }

        inline bool DispatchTargetViaHud(std::uint8_t slot, uintptr_t target) {
            if (!ResolveCommon(CastKind::Target, slot)) {
                return false;
            }

            if (!ThrottleReserve(slot)) {
                Fail(CastFailure::Throttled);
                return false;
            }

            if (!Globals::IsValidPtr(target)) {
                Fail(CastFailure::InvalidTarget);
                return false;
            }

            g_lastTrace.targetNetworkId =
                Globals::Read<std::uint32_t>(target + Offset::All::NetworkId);
            g_lastTrace.targetObjectIndex =
                Globals::Read<std::uint32_t>(target + Offset::All::Index);
            g_lastTrace.endPosition =
                Globals::Read<Vec3>(target + Offset::All::Position);

            if (g_lastTrace.targetNetworkId == 0 ||
                g_lastTrace.targetNetworkId == 0xFFFFFFFFu ||
                g_lastTrace.targetObjectIndex == 0 ||
                !g_lastTrace.endPosition.IsValid()) {
                Fail(CastFailure::InvalidTarget);
                return false;
            }

            const uintptr_t targeting = Globals::Read<uintptr_t>(
                g_lastTrace.hud + Offset::HudRuntime::SpellTargeting);
            if (!Globals::IsValidPtr(targeting)) {
                Fail(CastFailure::MissingHud);
                return false;
            }

            g_lastTrace.hudTargetState = Globals::Read<std::uint32_t>(
                targeting + Offset::HudSpellTargetingLayout::State);
            g_lastTrace.hudTargetObjectIndex = Globals::Read<std::uint32_t>(
                targeting + Offset::HudSpellTargetingLayout::ObjectIndex);

            // Jax Q's live classifier does not accept C12220's explicit-network-ID
            // branch. The real Smart Cast path calls C12220(..., 0), which performs
            // the cursor pick during BF1EF0. The cached HUD index may still be zero
            // before the key event, so it is diagnostic only. Never write it.

            g_lastTrace.canCastCheck =
                CoreRuntime::ResolveRva(Offset::ControlRuntime::CanCastCheck);
            g_lastTrace.castSpellSafe =
                CoreRuntime::ResolveRva(Offset::ControlRuntime::HudSpellHandler);
            if (!Globals::IsValidPtr(g_lastTrace.canCastCheck) ||
                !Globals::IsValidPtr(g_lastTrace.castSpellSafe)) {
                Fail(CastFailure::MissingNativeFunction);
                return false;
            }

            bool nativeException = false;
            bool bypassTouched = false;
            ScopedVirtualCursor virtualCursor;
            if (!virtualCursor.Apply(g_lastTrace.endPosition)) {
                return false;
            }

            __try {
                g_lastTrace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;

                spoof_call(
                    reinterpret_cast<void*>(g_lastTrace.spoofTrampoline),
                    reinterpret_cast<FnHudSpellHandler>(g_lastTrace.castSpellSafe),
                    g_lastTrace.castContext,
                    static_cast<std::int64_t>(slot),
                    kHudModeSmartCast,
                    kHudKeyPress);

                // BF1EF0 is void. It internally runs C12220(..., 0) and the regular
                // dispatcher; spell events are the authoritative runtime proof.
                g_lastTrace.canCastAccepted = true;
                g_lastTrace.nativeResult = 1;
            }
            __except (1) {
                nativeException = true;
            }

            ClearBypassFlag(bypassTouched);
            virtualCursor.Restore();

            if (nativeException) {
                Fail(CastFailure::NativeException);
                return false;
            }

            g_lastTrace.failure = CastFailure::None;
            g_lastTrace.success = true;
            CoreValidation::MarkCastResult(true);
            return true;
        }

        inline bool DispatchCharge(CastKind kind,
            std::uint8_t slot,
            const Vec3& position,
            std::int64_t keyState) {
            if (!ResolveCommon(kind, slot)) {
                return false;
            }

            if (!position.IsValid()) {
                Fail(CastFailure::InvalidPosition);
                return false;
            }
            g_lastTrace.endPosition = position;

            const uintptr_t chargeBefore = Globals::Read<uintptr_t>(
                g_lastTrace.castContext + kHudChargeSpellInput);
            if (keyState == kHudKeyRelease &&
                chargeBefore != g_lastTrace.spellInput) {
                Fail(CastFailure::MissingChargeState);
                return false;
            }

            g_lastTrace.castSpellSafe =
                CoreRuntime::ResolveRva(Offset::ControlRuntime::HudSpellHandler);
            if (!Globals::IsValidPtr(g_lastTrace.castSpellSafe)) {
                Fail(CastFailure::MissingNativeFunction);
                return false;
            }

            bool nativeException = false;
            bool bypassTouched = false;
            ScopedVirtualCursor virtualCursor;
            if (!virtualCursor.Apply(position)) {
                return false;
            }

            __try {
                g_lastTrace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;

                // This mirrors the game's Smart/Quick Cast hotkey path. The handler
                // reads the live HUD cursor; callers should pass the same position
                // they currently expose through their cursor service.
                spoof_call(
                    reinterpret_cast<void*>(g_lastTrace.spoofTrampoline),
                    reinterpret_cast<FnHudSpellHandler>(g_lastTrace.castSpellSafe),
                    g_lastTrace.castContext,
                    static_cast<std::int64_t>(slot),
                    kHudModeSmartCast,
                    keyState);

                g_lastTrace.canCastAccepted = true;
                g_lastTrace.nativeResult = 1;
            }
            __except (1) {
                nativeException = true;
            }

            ClearBypassFlag(bypassTouched);
            virtualCursor.Restore();

            if (nativeException) {
                Fail(CastFailure::NativeException);
                return false;
            }

            const uintptr_t chargeAfter = Globals::Read<uintptr_t>(
                g_lastTrace.castContext + kHudChargeSpellInput);
            if (keyState == kHudKeyPress &&
                chargeAfter != g_lastTrace.spellInput) {
                Fail(CastFailure::CanCastRejected);
                return false;
            }
            if (keyState == kHudKeyRelease &&
                chargeAfter == g_lastTrace.spellInput) {
                Fail(CastFailure::CanCastRejected);
                return false;
            }

            g_lastTrace.failure = CastFailure::None;
            g_lastTrace.success = true;
            CoreValidation::MarkCastResult(true);
            return true;
        }

    } // namespace detail

    // Position casts use the native HUD cursor selected by CanCastCheck. No
    // SpellInput fields are modified.
    inline bool CastPositionSpell(std::uint8_t slot, const Vec3& position) {
        return detail::CastValidated(CastKind::Position, slot, position, 0);
    }

    inline bool CastVectorSpell(std::uint8_t slot,
        const Vec3& startPosition,
        const Vec3& endPosition) {
        return detail::DispatchVector(slot, startPosition, endPosition);
    }

    // Targeted casts mirror Smart Cast key press. BF1EF0 performs the actual
    // cursor pick during the event; the target argument is used for validation
    // and diagnostics, not for writing internal HUD state.
    inline bool CastTargetSpell(std::uint8_t slot, uintptr_t target) {
        return detail::DispatchTargetViaHud(slot, target);
    }

    inline bool CastSelfSpell(std::uint8_t slot) {
        return detail::CastValidated(CastKind::Self, slot, {}, 0);
    }

    // Charge begin/release mirror the real Smart Cast key events:
    // sub_BF1EF0(context, slot, mode=2, keyState=1/2).
    inline bool BeginChargeSpell(std::uint8_t slot, const Vec3& position) {
        return detail::DispatchCharge(
            CastKind::ChargeBegin,
            slot,
            position,
            kHudKeyPress);
    }

    inline bool ReleaseChargeSpell(std::uint8_t slot, const Vec3& position) {
        return detail::DispatchCharge(
            CastKind::ChargeRelease,
            slot,
            position,
            kHudKeyRelease);
    }

} // namespace CoreCastSpell