#pragma once

#include "CoreBypass.h"
#include "CoreEvadeState.h"
#include "CoreRuntime.h"
#include "CoreValidation.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"
#include "spoof/spoofcall.h"
#include "../CrashTrace.h"

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
    inline constexpr std::uint8_t SlotItem1 = 6;
    inline constexpr std::uint8_t SlotItem6 = 11;
    inline constexpr std::uint8_t SlotTrinket = 12;

    enum class CastKind : std::uint8_t {
        Position,
        Target,
        Self,
        Vector,
        ChargeBegin,
        ChargeRelease,
        Item,
        ItemPosition,
        ItemTarget,
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
        MissingMouseInput,
        PositionProjectionFailed,
        CanCastRejected,
        NativeException,
        Throttled,
        MissingChargeState,
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
    // (e.g. space) doesn't spam the native cast every frame. Matches the SDK Spell wrapper.
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
        case CastKind::Item: return "item";
        case CastKind::ItemPosition: return "item-position";
        case CastKind::ItemTarget: return "item-target";
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
        case CastFailure::MissingMouseInput: return "missing-mouse-input";
        case CastFailure::PositionProjectionFailed: return "position-projection-failed";
        case CastFailure::CanCastRejected: return "can-cast-rejected";
        case CastFailure::NativeException: return "native-exception";
        case CastFailure::Throttled: return "throttled";
        case CastFailure::MissingChargeState: return "missing-charge-state";
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

        inline bool IsSupportedSlot(std::uint8_t slot) {
            return slot <= SlotTrinket;
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
            static int lastTick[13] = {};
            const int idx = slot < 13 ? slot : 12;
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

        inline bool Reject(CastKind kind,
            std::uint8_t slot,
            CastFailure failure) {
            g_lastTrace = {};
            g_lastTrace.kind = kind;
            g_lastTrace.slot = slot;
            Fail(failure);
            return false;
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
            if (CoreEvadeState::AreSpellCastsBlocked(
                    static_cast<int>(ctx.gameTime * 1000.0f),
                    static_cast<int>(slot))) {
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
            if (Globals::IsValidPtr(g_lastTrace.hud)) {
                g_lastTrace.castContext = Globals::Read<uintptr_t>(
                    g_lastTrace.hud + Offset::HudRuntime::SpellInfo);
            } else {
                g_lastTrace.castContext = 0;
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
                runtimeSlot = spoof_call_hybrid(
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
            const Vec3& endPosition) {
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
                NightSharpDebug::CrashTrace::Record(
                    nscrash::TraceTag::SpellCast,
                    static_cast<std::uint64_t>(slot),
                    static_cast<std::uint64_t>(kind));
                g_lastTrace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;

                g_lastTrace.canCastAccepted = spoof_call_hybrid(
                    reinterpret_cast<void*>(g_lastTrace.spoofTrampoline),
                    reinterpret_cast<FnCanCastCheck>(g_lastTrace.canCastCheck),
                    g_lastTrace.castContext,
                    g_lastTrace.spellInput,
                    std::uint32_t{0});

                if (g_lastTrace.canCastAccepted) {
                    g_lastTrace.nativeResult = spoof_call_hybrid(
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

            if (!endPosition.IsValid() || endPosition.IsZero()) {
                Fail(CastFailure::InvalidPosition);
                return false;
            }

            const Vec3 actualStart = (!startPosition.IsValid() || startPosition.IsZero())
                ? endPosition
                : startPosition;

            g_lastTrace.startPosition = actualStart;
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

            const NativeVec3Xyz start{ actualStart.x, actualStart.y, actualStart.z };
            const NativeVec3Xyz end{ endPosition.x, endPosition.y, endPosition.z };

            bool nativeException = false;
            bool bypassTouched = false;

            __try {
                NightSharpDebug::CrashTrace::Record(
                    nscrash::TraceTag::SpellCast,
                    static_cast<std::uint64_t>(slot),
                    static_cast<std::uint64_t>(CastKind::Vector));
                g_lastTrace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;

                spoof_call_hybrid(
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

        inline bool DispatchItem(CastKind kind,
            std::uint8_t slot,
            const Vec3* position,
            uintptr_t target) {
            if (!ResolveCommon(kind, slot)) {
                return false;
            }

            if (!ThrottleReserve(slot)) {
                Fail(CastFailure::Throttled);
                return false;
            }

            const Vec3* cursorPosition = position;
            Vec3 targetPosition = {};
            if (target) {
                if (!Globals::IsValidPtr(target)) {
                    Fail(CastFailure::InvalidTarget);
                    return false;
                }
                targetPosition = Globals::Read<Vec3>(target + Offset::All::Position);
                if (!targetPosition.IsValid()) {
                    Fail(CastFailure::InvalidTarget);
                    return false;
                }
                cursorPosition = &targetPosition;
                g_lastTrace.endPosition = targetPosition;
            } else if (position) {
                if (!position->IsValid()) {
                    Fail(CastFailure::InvalidPosition);
                    return false;
                }
                g_lastTrace.endPosition = *position;
            } else {
                g_lastTrace.endPosition = g_lastTrace.startPosition;
            }

            if (!Globals::IsValidPtr(g_lastTrace.castContext)) {
                Fail(CastFailure::MissingCastContext);
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
            if (cursorPosition && !virtualCursor.Apply(*cursorPosition)) {
                return false;
            }

            __try {
                g_lastTrace.bypassPrepared = CoreBypass::PrepareCastSpell();
                bypassTouched = true;
                spoof_call_hybrid(
                    reinterpret_cast<void*>(g_lastTrace.spoofTrampoline),
                    reinterpret_cast<FnHudSpellHandler>(g_lastTrace.castSpellSafe),
                    g_lastTrace.castContext,
                    static_cast<std::int64_t>(slot),
                    kHudModeSmartCast,
                    kHudKeyPress);
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

    } // namespace detail

    inline bool IsSpellCastSlot(std::uint8_t slot) {
        return slot <= SlotSummonerF;
    }

    inline bool IsItemCastSlot(std::uint8_t slot) {
        return slot >= SlotItem1 && slot <= SlotTrinket;
    }

    // Position casts call native CastSpellVector with verbatim world coordinates (bypasses HUD).
    inline bool CastPositionSpell(std::uint8_t slot, const Vec3& position) {
        if (!IsSpellCastSlot(slot)) {
            return detail::Reject(CastKind::Position, slot, CastFailure::InvalidSlot);
        }
        return detail::DispatchVector(slot, {}, position);
    }

    inline bool CastVectorSpell(std::uint8_t slot,
        const Vec3& startPosition,
        const Vec3& endPosition) {
        if (!IsSpellCastSlot(slot)) {
            return detail::Reject(CastKind::Vector, slot, CastFailure::InvalidSlot);
        }
        return detail::DispatchVector(slot, startPosition, endPosition);
    }

    inline bool CastSelfSpell(std::uint8_t slot) {
        if (!IsSpellCastSlot(slot)) {
            return detail::Reject(CastKind::Self, slot, CastFailure::InvalidSlot);
        }
        return detail::CastValidated(CastKind::Self, slot, {});
    }

    inline bool CastItem(std::uint8_t slot) {
        if (!IsItemCastSlot(slot)) {
            return detail::Reject(CastKind::Item, slot, CastFailure::InvalidSlot);
        }
        return detail::DispatchItem(CastKind::Item, slot, nullptr, 0);
    }

    inline bool CastItemPosition(std::uint8_t slot, const Vec3& position) {
        if (!IsItemCastSlot(slot)) {
            return detail::Reject(CastKind::ItemPosition, slot, CastFailure::InvalidSlot);
        }
        return detail::DispatchItem(CastKind::ItemPosition, slot, &position, 0);
    }

    inline bool CastItemOnTarget(std::uint8_t slot, uintptr_t target) {
        if (!IsItemCastSlot(slot)) {
            return detail::Reject(CastKind::ItemTarget, slot, CastFailure::InvalidSlot);
        }
        return detail::DispatchItem(CastKind::ItemTarget, slot, nullptr, target);
    }

} // namespace CoreCastSpell
