#pragma once
// =============================================================================
// CoreZoomHack - Camera zoom unlock (heap-only writes, no image-section poke)
// =============================================================================
// Safety analysis (May 07/2026):
//   * ZoomConfig la heap object allocated boi sub_168680 (qword_1E49380 init).
//   * Tat ca writes nham vao heap memory -> Vanguard CRC scan KHONG cover heap.
//   * Hardcoded address 0x1920A24 (HardcodedMaxZoom in .rdata) BI BO QUA -
//     image-section co CRC, viec ghi se trigger detect.
//   * Idempotent (chi write khi value khac) -> minimize syscall noise.
//   * Writes go through PackmanHook DirectSyscall::WriteVirtualMemoryDirect,
//     with Globals::Write fallback.
//   * Restorable: cache original Min/Max tai lan enable dau tien.
//
// MAY 2026 BEHAVIOUR — minimap click + HP bar fix:
//
//   1) Minimap click fix:
//      Previous code wrote CurrentZoom (camera+0x324) every tick when the
//      active zoom exceeded our cap, fighting the game's own camera-
//      transition state machine → minimap click was processed but the
//      camera never navigated. New behaviour: leave CurrentZoom alone.
//
//   2) HP bar fix (May 07/2026):
//      Two ZoomConfig pointers exist on the camera:
//        cfg    @ camera+0x310 (ZoomFallbackPtr)  — the WHEEL CAP.
//        altCfg @ camera+0x3D0 (ZoomConfigPtr)    — the TARGET ZOOM.
//
//      sub_BDF5D0 (called from HUD event handlers in-game) reads
//      altCfg.MaxZoom unconditionally and force-sets currentZoom to
//      max(cfg.MinZoom, min(cfg.MaxZoom, altCfg.MaxZoom)). If we write
//      altCfg.MaxZoom = 30000, the next event triggers `currentZoom =
//      30000` → camera shoots out, hero becomes microscopic, HP bars
//      get culled → the user reported "as soon as I toggle ON, HP bar
//      disappears".
//
//      Fix: write ONLY cfg.MaxZoom (the wheel cap) and leave altCfg
//      untouched. Scroll-wheel input flows through sub_BCDC00 which
//      clamps to [cfg.MinZoom, cfg.MaxZoom=kZoomCap] → wheel can scroll
//      out to kZoomCap. sub_BDF5D0 sees altCfg.MaxZoom = vanilla → only
//      forces a vanilla zoom snap on rare events, never an extreme one.
//
//   3) Slider removed: ON = scroll-wheel zoom up to kZoomCap, OFF = vanilla.
//
// Resolution chain (tick-fresh tu CoreRuntime::TickRead):
// IDA 26.6:
//   Camera global qword_1E21698, fallback HUDInstance + 0x18
//   Camera      -> +CurrentZoom     (0x324) : float (NOT WRITTEN ANYMORE)
//   Camera      -> +ZoomFallbackPtr (0x310) -> wheel-cap config (WRITE MAX)
//   Camera      -> +ZoomConfigPtr   (0x3D0) -> target-zoom config (DO NOT WRITE)
//   ZoomConfig  -> +ZC_MinZoom      (0x24)  : float (NOT WRITTEN — keep vanilla floor)
//   ZoomConfig  -> +ZC_MaxZoom      (0x28)  : float (raised to kZoomCap on cfg only)
//   Camera      -> +0x344/+0x345 are left untouched. Forcing 0x345 breaks
//                  minimap click routing on the current client.
// =============================================================================

#include "Globals.h"
#include "CoreRuntime.h"
#include "PackmanHook.h"
#include "offset.h"

#include <cmath>
#include <cstdint>
#include <type_traits>

namespace CoreZoomHack {

    // Hard cap installed when the toggle is ON.
    //
    // VALUE TUNING (May 07/2026 — verified live in 26.7 with CE MCP):
    //   * Vanilla cfg.MaxZoom on 26.7 = 2250.0 (NOT 1750 as older notes said).
    //   * Vanilla cfg.MinZoom = 1000.0.
    //   * On 26.7 cfg pointer (camera+0x310) and altCfg pointer (camera+0x3D0)
    //     refer to the SAME ZoomConfig object. Writing cfg.MaxZoom therefore
    //     also raises altCfg.MaxZoom — there is no separate "wheel cap" we
    //     can isolate from the HUD's projection math.
    //   * HP bar position is influenced by currentZoom AND by how far it sits
    //     from cfg.MaxZoom. Empirical thresholds (tested in-game):
    //         3000 → HP bar identical to vanilla.
    //         3500 → HP bar identical to vanilla. (sweet spot, picked.)
    //         4000 → HP bar visibly drifts up by a few pixels.
    //         5000 → HP bar drifts noticeably (the "slightly high" report).
    //        30000 → HP bar pushed near the very top of screen.
    //   * 3500 = ~1.55x vanilla. Modest scroll-out boost without ANY visible
    //     HUD displacement, matching what other public LoL scripts ship.
    inline constexpr float kZoomCap = 3500.0f;

    inline float    s_origMax       = -1.0f;
    inline bool     s_active        = false;

    template <typename T>
    inline bool PackmanWrite(uintptr_t addr, const T& value) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "CoreZoomHack::PackmanWrite requires trivially copyable values");
        if (!Globals::IsValidPtr(addr)) return false;

        __try {
            if (DirectSyscall::WriteVirtualMemoryDirect(
                    reinterpret_cast<void*>(addr),
                    &value,
                    sizeof(T))) {
                return true;
            }
            return Globals::Write<T>(addr, value);
        }
        __except (1) {
            return false;
        }
    }

    inline uintptr_t ResolveZoomConfig(uintptr_t camera, uintptr_t fieldOffset) {
        if (!Globals::IsValidPtr(camera)) return 0;
        const auto cfg = Globals::ReadPtr(camera + fieldOffset);
        return Globals::IsValidPtr(cfg) ? cfg : 0;
    }

    inline bool IsUsableCamera(uintptr_t camera) {
        if (!Globals::IsValidPtr(camera)) return false;

        const float current = Globals::Read<float>(camera + Offset::ZoomRuntime::CurrentZoom);
        if (!(current > 100.0f && current < 50000.0f)) return false;

        const auto cfg = ResolveZoomConfig(camera, Offset::ZoomRuntime::ZoomFallbackPtr);
        if (!Globals::IsValidPtr(cfg)) return false;

        const float minZoom = Globals::Read<float>(cfg + Offset::ZoomRuntime::ZC_MinZoom);
        const float maxZoom = Globals::Read<float>(cfg + Offset::ZoomRuntime::ZC_MaxZoom);
        return minZoom >= 0.0f && minZoom < maxZoom && maxZoom < 50000.0f;
    }

    inline uintptr_t ResolveCamera() {
        const auto& ctx = CoreRuntime::GetContext();
        if (Globals::IsValidPtr(ctx.hudInstance)) {
            const auto camera = Globals::ReadPtr(
                ctx.hudInstance + Offset::ZoomRuntime::HudToCameraPtr);
            if (IsUsableCamera(camera)) {
                return camera;
            }
        }

        if (Globals::base && Offset::ZoomRuntime::CameraInstance != 0) {
            const auto direct =
                Globals::ReadPtr(Globals::base + Offset::ZoomRuntime::CameraInstance);
            return IsUsableCamera(direct) ? direct : 0;
        }
        return 0;
    }

    inline void WriteZoomLimit(uintptr_t cfg, float maxZoom) {
        if (!Globals::IsValidPtr(cfg)) return;
        const auto curMax = Globals::Read<float>(cfg + Offset::ZoomRuntime::ZC_MaxZoom);
        if (std::fabs(curMax - maxZoom) > 0.5f) {
            PackmanWrite<float>(cfg + Offset::ZoomRuntime::ZC_MaxZoom, maxZoom);
        }
    }

    // Snapshot original cfg.MaxZoom once (idempotent). On 26.7 vanilla is
    // ~2250 and our hack writes kZoomCap (3500); we cache anything that
    // looks distinctly smaller than the hacked value so re-toggling can
    // restore the true vanilla number.
    inline void CacheOriginals(uintptr_t cfg) {
        if (s_active) return;
        if (Globals::IsValidPtr(cfg)) {
            const auto cur = Globals::Read<float>(cfg + Offset::ZoomRuntime::ZC_MaxZoom);
            // Cache anything in (100, kZoomCap - small_epsilon).
            // - lower bound guards against reading garbage / uninit cfg
            // - upper bound prevents recording the hacked value if Enable()
            //   is somehow called twice in a row without a Disable()
            if (cur > 100.0f && cur < kZoomCap - 50.0f) {
                s_origMax = cur;
            }
        }
        s_active = true;
    }

    // Toggle ON: raise the wheel zoom cap (cfg.MaxZoom) so the mouse wheel
    // can scroll past the vanilla ≈2250 cap. We do NOT write currentZoom,
    // so the camera stays where the player left it; only the upper clamp
    // moves. NOTE: on 26.7, camera+0x310 (cfg) and camera+0x3D0 (altCfg)
    // share the same backing object, so this single write covers both.
    inline void Enable() {
        const auto camera = ResolveCamera();
        if (!camera) return;

        const auto cfg = ResolveZoomConfig(camera, Offset::ZoomRuntime::ZoomFallbackPtr);
        if (!cfg) return;

        CacheOriginals(cfg);

        // sub_BCDC00 (mouse wheel handler) reads cfg.MaxZoom (camera+0x310)
        // for its upper clamp. Raising this to kZoomCap lets the wheel
        // scroll out further than the vanilla ≈2250 cap.
        WriteZoomLimit(cfg, kZoomCap);
    }

    // Toggle OFF: restore vanilla cfg.MaxZoom captured at first Enable().
    inline void Disable() {
        if (!s_active) return;
        const auto camera = ResolveCamera();
        if (!camera) {
            s_active  = false;
            s_origMax = -1.0f;
            return;
        }

        const auto cfg = ResolveZoomConfig(camera, Offset::ZoomRuntime::ZoomFallbackPtr);
        if (Globals::IsValidPtr(cfg) && s_origMax > 0.0f) {
            PackmanWrite<float>(cfg + Offset::ZoomRuntime::ZC_MaxZoom, s_origMax);
        }
        s_active  = false;
        s_origMax = -1.0f;
    }

    // One-shot tick driver. Call from main update loop.
    // The legacy `maxZoom` argument is ignored — we always install kZoomCap.
    inline void Tick(bool enabled, float /*ignored*/ = 0.0f) {
        if (enabled) {
            Enable();
        } else {
            Disable();
        }
    }

} // namespace CoreZoomHack
