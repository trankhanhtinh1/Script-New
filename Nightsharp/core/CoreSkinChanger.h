#pragma once
// =============================================================================
// CoreSkinChanger - Local-only visual skin override
// =============================================================================
// Safety analysis (Apr 25/2026):
//   * Writes target ONLY local hero (AIHeroClient heap object) - no enemy/ally.
//   * Skin loader reads SkinChangeFlag each frame; setting it triggers a
//     model reload via existing engine path. We do NOT call
//     PreloadCharacterWithSkinID (that path emits a network packet).
//   * No image-section writes -> immune to CRC scan.
//   * SEH-safe via Globals::Read/Write.
//   * Idempotent: only writes when target SkinID differs from current.
//   * Visual only: server stays authoritative for actual game state.
//
// Field layout (offset.h SkinRuntime):
//   hero + 0x1440 SkinNetID       : int32 - skin id (0 = base)
//   hero + 0x1448 SkinName        : char[20] - cached skin internal name
//   hero + 0x1468 ModelName       : char[20] - cached model name
//   hero + 0x1488 SkinChangeFlag  : uint8 - set 1 to request reload
//   hero + 0x148C SkinParam1      : reserved
//   hero + 0x1490 SkinParam2      : reserved
// =============================================================================

#include "Globals.h"
#include "CoreRuntime.h"
#include "offset.h"

#include <cstdint>

namespace CoreSkinChanger {

    // Track last applied id to skip redundant flag writes.
    inline int s_lastApplied = -1;

    // Force-apply skin id to local hero. Returns true on successful write.
    // skinId < 0  -> no-op (handle as "disabled").
    inline bool Apply(int skinId) {
        if (skinId < 0) return false;

        const auto local = CoreRuntime::GetContext().localPlayer;
        if (!Globals::IsValidPtr(local)) return false;

        const auto curId = Globals::Read<int>(
            local + Offset::SkinRuntime::SkinNetID);
        if (curId == skinId && s_lastApplied == skinId) {
            // Already applied - nothing to do.
            return true;
        }

        // 1) Write new skin id
        if (!Globals::Write<int>(local + Offset::SkinRuntime::SkinNetID, skinId)) {
            return false;
        }

        // 2) Trigger reload by raising flag (engine clears it next tick)
        Globals::Write<uint8_t>(local + Offset::SkinRuntime::SkinChangeFlag, 1);

        s_lastApplied = skinId;
        return true;
    }

    // Reset tracker - call on game leave so re-enable forces a fresh apply.
    inline void Reset() {
        s_lastApplied = -1;
    }

    // One-shot tick driver. Call from main update loop.
    //   enabled == false -> no-op (skin stays whatever was last applied;
    //                       game restores on respawn / champion select).
    inline void Tick(bool enabled, int skinId) {
        if (!enabled) return;
        Apply(skinId);
    }

} // namespace CoreSkinChanger
