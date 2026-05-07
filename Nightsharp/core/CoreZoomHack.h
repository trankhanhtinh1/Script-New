#pragma once
// =============================================================================
// CoreZoomHack - Camera zoom unlock (heap-only writes, no image-section poke)
// =============================================================================
// Safety analysis (Apr 25/2026):
//   * ZoomConfig la heap object allocated boi sub_168680 (qword_1E49380 init).
//   * Tat ca writes nham vao heap memory -> Vanguard CRC scan KHONG cover heap.
//   * Hardcoded address 0x1920A24 (HardcodedMaxZoom in .rdata) BI BO QUA -
//     image-section co CRC, viec ghi se trigger detect.
//   * Idempotent (chi write khi value khac) -> minimize syscall noise.
//   * SEH-safe via Globals::Read/Write.
//   * Restorable: cache original Min/Max + clamp flag tai lan enable dau tien.
//
// Resolution chain (tick-fresh tu CoreRuntime::TickRead):
//   HUDInstance -> +HudToCameraPtr (0x18) -> Camera
//   Camera      -> +ZoomConfigPtr  (0x3D0) -> ZoomConfig
//   ZoomConfig  -> +ZC_MinZoom     (0x24)  : float
//   ZoomConfig  -> +ZC_MaxZoom     (0x28)  : float
//   ZoomConfig  -> +DisableZoomClamp (0x345) : uint8 boolean
// =============================================================================

#include "Globals.h"
#include "CoreRuntime.h"
#include "offset.h"

#include <cstdint>

namespace CoreZoomHack {

    inline float    s_origMin       = -1.0f;
    inline float    s_origMax       = -1.0f;
    inline uint8_t  s_origClampFlag = 0xFF;
    inline bool     s_active        = false;

    // Resolve tu HUD instance ra ZoomConfig pointer. Tra 0 neu chain invalid.
    inline uintptr_t ResolveZoomConfig() {
        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(ctx.hudInstance)) return 0;

        const auto camera = Globals::ReadPtr(
            ctx.hudInstance + Offset::ZoomRuntime::HudToCameraPtr);
        if (!Globals::IsValidPtr(camera)) return 0;

        const auto zoomCfg = Globals::ReadPtr(
            camera + Offset::ZoomRuntime::ZoomConfigPtr);
        return Globals::IsValidPtr(zoomCfg) ? zoomCfg : 0;
    }

    // Snapshot original values once (idempotent)
    inline void CacheOriginals(uintptr_t cfg) {
        if (s_active) return;
        s_origMin       = Globals::Read<float>(cfg + Offset::ZoomRuntime::ZC_MinZoom);
        s_origMax       = Globals::Read<float>(cfg + Offset::ZoomRuntime::ZC_MaxZoom);
        s_origClampFlag = Globals::Read<uint8_t>(cfg + Offset::ZoomRuntime::DisableZoomClamp);
        s_active        = true;
    }

    // Apply zoom override. maxZoom in raw units (default vanilla cap ~ 1750).
    // Common safe values: 2000..6000.
    inline void Enable(float maxZoom) {
        const auto cfg = ResolveZoomConfig();
        if (!cfg) return;

        CacheOriginals(cfg);

        // Idempotent writes - only update if value differs (avoid syscall spam).
        const auto curMax = Globals::Read<float>(cfg + Offset::ZoomRuntime::ZC_MaxZoom);
        if (curMax != maxZoom) {
            Globals::Write<float>(cfg + Offset::ZoomRuntime::ZC_MaxZoom, maxZoom);
        }

        const auto curFlag = Globals::Read<uint8_t>(
            cfg + Offset::ZoomRuntime::DisableZoomClamp);
        if (curFlag != 1) {
            Globals::Write<uint8_t>(
                cfg + Offset::ZoomRuntime::DisableZoomClamp, 1);
        }
    }

    // Restore vanilla zoom limits.
    inline void Disable() {
        if (!s_active) return;
        const auto cfg = ResolveZoomConfig();
        if (!cfg) {
            // Resolution failed (game state changed). Just clear cache;
            // values will reset naturally on next ZoomConfig reallocation.
            s_active = false;
            return;
        }

        Globals::Write<float>(cfg + Offset::ZoomRuntime::ZC_MinZoom, s_origMin);
        Globals::Write<float>(cfg + Offset::ZoomRuntime::ZC_MaxZoom, s_origMax);
        Globals::Write<uint8_t>(
            cfg + Offset::ZoomRuntime::DisableZoomClamp, s_origClampFlag);
        s_active = false;
    }

    // One-shot tick driver. Call from main update loop.
    inline void Tick(bool enabled, float maxZoom) {
        if (enabled) {
            Enable(maxZoom);
        } else {
            Disable();
        }
    }

} // namespace CoreZoomHack
