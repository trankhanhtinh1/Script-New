#pragma once
// =============================================================================
// CoreSkinChanger - Local-only visual skin override (R3nzSkin-style)
// =============================================================================
// Safety analysis (May 07/2026):
//   * Writes target ONLY local hero (AIHeroClient heap object) - no enemy/ally.
//   * R3nzSkin's `AIBaseCommon::change_skin` writes BOTH:
//       (1) hero + 0x1334  (xor_value<int32> SkinId, encrypted)
//       (2) stack->base_skin.skin  (raw int32)
//     and then calls `CharacterDataStack::update(true)`.
//     Writing only (2) — what Nightsharp did before — leaves (1) holding the
//     old encrypted skin id. Some animation paths read (1), some read (2);
//     the desync silently corrupts model resource lookups and the game
//     crashes on the next character-data refresh (death, recall, transform).
//   * We do NOT call PreloadCharacterWithSkinID (that path emits a network
//     packet).
//   * No image-section writes -> immune to CRC scan.
//   * SEH-safe via Globals::Read/Write.
//   * Idempotent: only writes when target SkinID differs from current.
//   * Visual only: server stays authoritative for actual game state.
//
// Field layout (offset.h SkinRuntime), verified against R3nzSkin + IDA (May 2026):
//   hero + 0x1334               : encrypted SkinId (xor_value<int32>)
//   hero + 0x4100               : CharacterDataStack
//   stack + 0x18                : base_skin CharacterStackData
//     base_skin + 0x00          : model.str (const char*)
//     base_skin + 0x08          : model.length (int32)
//     base_skin + 0x0C          : model.capacity (int32)
//     base_skin + 0x20          : skin id (int32)
//     base_skin + 0x84          : gear (int8)
//   base + 0x20BEF0             : CharacterDataStack::update(stack, change)
//   base + 0x22A1D0             : CharacterDataStack::push(this, model, skin, ...)
// =============================================================================

#include "Globals.h"
#include "CoreRuntime.h"
#include "LeagueObfuscation.h"
#include "offset.h"

#include <cstdint>
#include <cstring>

namespace CoreSkinChanger {

    // Track last applied id to skip redundant flag writes.
    inline int s_lastApplied = -1;
    inline int s_originalSkinId = -1;
    inline bool s_active = false;

    inline uintptr_t ResolveCharacterDataStack(uintptr_t local) {
        if (!Globals::IsValidPtr(local)) return 0;

        const auto stack = local + Offset::SkinRuntime::CharacterDataStack;
        const auto modelPtr = Globals::Read<uintptr_t>(
            stack + Offset::SkinRuntime::CharacterDataStackBaseSkin +
            Offset::SkinRuntime::CharacterStackModelPtr);
        const int modelLen = Globals::Read<int>(
            stack + Offset::SkinRuntime::CharacterDataStackBaseSkin +
            Offset::SkinRuntime::CharacterStackModelLen);
        const int modelCap = Globals::Read<int>(
            stack + Offset::SkinRuntime::CharacterDataStackBaseSkin +
            Offset::SkinRuntime::CharacterStackModelCap);

        if (!Globals::IsValidPtr(modelPtr)) return 0;
        if (modelLen <= 0 || modelLen > 64) return 0;
        if (modelCap < modelLen || modelCap > 128) return 0;

        return stack;
    }

    inline int ReadStackSkin(uintptr_t stack) {
        return Globals::Read<int>(
            stack + Offset::SkinRuntime::CharacterDataStackBaseSkin +
            Offset::SkinRuntime::CharacterStackSkin);
    }

    inline bool WriteStackSkin(uintptr_t stack, int skinId) {
        return Globals::Write<int>(
            stack + Offset::SkinRuntime::CharacterDataStackBaseSkin +
            Offset::SkinRuntime::CharacterStackSkin,
            skinId);
    }

    // Read the champion's internal model name (e.g. "Aatrox", "MissFortune").
    // Falls back to empty string on any read failure.
    inline bool ReadModelName(uintptr_t stack, char* out, int maxOut) {
        if (!out || maxOut <= 1) return false;
        out[0] = 0;
        const auto base = stack + Offset::SkinRuntime::CharacterDataStackBaseSkin;
        const auto strPtr = Globals::Read<uintptr_t>(
            base + Offset::SkinRuntime::CharacterStackModelPtr);
        if (!Globals::IsValidPtr(strPtr)) return false;
        return Globals::ReadCString(strPtr, out, maxOut);
    }

    inline bool ModelEquals(const char* model, const char* candidate) {
        if (!model || !candidate) return false;
        return std::strcmp(model, candidate) == 0;
    }

    // Encrypted skin id write to hero + 0x1334. The game already initialised
    // the xor_value<int32> block at champion spawn, so we just encrypt and
    // rotate. If `isInit` is false (block not initialised) we silently
    // skip — Encrypt() returns early without writing.
    inline bool WriteEncryptedSkinId(uintptr_t hero, int skinId) {
        const auto fieldAddr = hero + Offset::SkinRuntime::AiBaseSkinId;
        __try {
            auto block = Globals::Read<LeagueObfuscation<int>>(fieldAddr);
            if (!block.isInit) return false;
            Encrypt<int>(block, skinId);
            return Globals::Write<LeagueObfuscation<int>>(fieldAddr, block);
        }
        __except (1) {
            return false;
        }
    }

    inline int ReadEncryptedSkinId(uintptr_t hero) {
        const auto fieldAddr = hero + Offset::SkinRuntime::AiBaseSkinId;
        __try {
            const auto block = Globals::Read<LeagueObfuscation<int>>(fieldAddr);
            if (!block.isInit) return -1;
            return Decrypt<int>(block);
        }
        __except (1) {
            return -1;
        }
    }

    inline bool UpdateCharacterDataStack(uintptr_t stack) {
        if (!Globals::IsValidPtr(stack) || !Globals::base) return false;

        using update_t = __int64(__fastcall*)(uintptr_t, bool);
        const auto update = reinterpret_cast<update_t>(
            Globals::base + Offset::SkinRuntime::CharacterDataStackUpdate);

        __try {
            update(stack, true);
            return true;
        }
        __except (1) {
            return false;
        }
    }

    // CharacterDataStack::push(this, model, skin, 0, false, false, false,
    //                          false, true, false, -1, "", 0, "", 0, false, 1)
    // Used by R3nzSkin for Lux skin 7 / Sona skin 6 (special skin states
    // requiring an extra stack entry on top of base_skin).
    inline bool PushCharacterDataStack(uintptr_t stack, const char* model, int skinId) {
        if (!Globals::IsValidPtr(stack) || !Globals::base || !model) return false;

        using push_t = __int64(__fastcall*)(
            std::uintptr_t, const char*, std::int32_t, std::int32_t,
            bool, bool, bool, bool, bool, bool, std::int8_t,
            const char*, std::int32_t, const char*, std::int32_t,
            bool, std::int32_t);

        const auto push = reinterpret_cast<push_t>(
            Globals::base + Offset::SkinRuntime::CharacterDataStackPush);

        __try {
            push(stack, model, skinId, 0,
                 false, false, false, false, true, false, (std::int8_t)-1,
                 "", 0, "", 0, false, 1);
            return true;
        }
        __except (1) {
            return false;
        }
    }

    inline bool WriteGear(uintptr_t stack, int8_t value) {
        return Globals::Write<int8_t>(
            stack + Offset::SkinRuntime::CharacterDataStackBaseSkin +
            Offset::SkinRuntime::CharacterStackGear,
            value);
    }

    inline int8_t ReadGear(uintptr_t stack) {
        return Globals::Read<int8_t>(
            stack + Offset::SkinRuntime::CharacterDataStackBaseSkin +
            Offset::SkinRuntime::CharacterStackGear);
    }

    // Clear the std::vector<CharacterStackData> at offset 0 of the stack.
    // We just zero the begin/end/cap pointers — the game's vector code
    // tolerates an empty-but-no-allocation state (R3nzSkin does this via
    // `stack.clear()`; the C++ allocator path is overkill for our use).
    // Note: this leaks the previous backing buffer if non-null, but the
    // total leak is bounded (one CharacterStackData = 0x88 bytes per
    // skin-change event); acceptable for a session.
    inline bool ClearStackVector(uintptr_t stack) {
        if (!Globals::IsValidPtr(stack)) return false;
        const auto end = Globals::Read<uintptr_t>(
            stack + Offset::SkinRuntime::CharacterDataStackEnd);
        const auto begin = Globals::Read<uintptr_t>(
            stack + Offset::SkinRuntime::CharacterDataStackBegin);
        if (end == begin) return true;  // already empty
        // Move end pointer back to begin -> size becomes 0 without
        // touching the allocation. Game iterates begin..end so this is
        // semantically equivalent to clear().
        return Globals::Write<uintptr_t>(
            stack + Offset::SkinRuntime::CharacterDataStackEnd, begin);
    }

    // Mirror R3nzSkin::AIBaseCommon::checkSpecialSkins.
    // Returns true if special handling already pushed the skin (caller must
    // skip the regular update path).
    inline bool ApplySpecialSkinHandling(
        uintptr_t stack, const char* model, int skinId)
    {
        if (!model) return false;

        // Cosmetic-gear champions: skin id ranges trigger the unlocked variant.
        if (ModelEquals(model, "Katarina") && skinId >= 29 && skinId <= 36) {
            WriteGear(stack, 0);
            return false;
        }
        if (ModelEquals(model, "Renekton") && skinId >= 26 && skinId <= 32) {
            WriteGear(stack, 0);
            return false;
        }
        if (ModelEquals(model, "MissFortune") && skinId == 16) {
            WriteGear(stack, 0);
            return false;
        }

        // Lux/Sona special skins push a stack entry instead of update().
        if (ModelEquals(model, "Lux") || ModelEquals(model, "Sona")) {
            const bool luxSpecial = (skinId == 7 && ModelEquals(model, "Lux"));
            const bool sonaSpecial = (skinId == 6 && ModelEquals(model, "Sona"));
            if (luxSpecial || sonaSpecial) {
                ClearStackVector(stack);
                PushCharacterDataStack(stack, model, skinId);
                return true;  // pushed → skip update
            }
            ClearStackVector(stack);
            return false;  // fall through to regular update
        }

        // Other champions: ensure gear is reset to -1 (no cosmetic variant)
        // unless we're playing Kayn (gear field carries the form transform).
        if (!ModelEquals(model, "Kayn")) {
            const auto curGear = ReadGear(stack);
            if (curGear != (int8_t)-1) {
                WriteGear(stack, (int8_t)-1);
            }
        }
        return false;
    }

    // Force-apply skin id to local hero. Returns true on successful write.
    // skinId < 0  -> no-op (handle as "disabled").
    inline bool Apply(int skinId) {
        if (skinId < 0) return false;

        const auto local = CoreRuntime::GetContext().localPlayer;
        if (!Globals::IsValidPtr(local)) return false;

        const auto stack = ResolveCharacterDataStack(local);
        if (!stack) return false;

        const auto curId = ReadStackSkin(stack);
        if (curId == skinId && s_lastApplied == skinId) {
            // Already applied - nothing to do.
            return true;
        }

        if (!s_active) {
            s_originalSkinId = curId;
            s_active = true;
        }

        // R3nzSkin order: encrypt SkinId first, then write base_skin.skin,
        // then run special-skin / update logic.
        WriteEncryptedSkinId(local, skinId);

        if (!WriteStackSkin(stack, skinId)) {
            return false;
        }

        char model[64] = {0};
        const bool haveModel = ReadModelName(stack, model, sizeof(model));

        const bool pushed = haveModel
            ? ApplySpecialSkinHandling(stack, model, skinId)
            : false;

        if (!pushed) {
            if (!UpdateCharacterDataStack(stack)) {
                return false;
            }
        }

        s_lastApplied = skinId;
        return true;
    }

    // Reset tracker - call on game leave so re-enable forces a fresh apply.
    inline void Reset() {
        s_lastApplied = -1;
        s_originalSkinId = -1;
        s_active = false;
    }

    inline void Restore() {
        if (!s_active || s_originalSkinId < 0) {
            Reset();
            return;
        }

        const auto local = CoreRuntime::GetContext().localPlayer;
        if (!Globals::IsValidPtr(local)) {
            Reset();
            return;
        }

        const auto stack = ResolveCharacterDataStack(local);
        if (stack) {
            // Restore both the encrypted SkinId and base_skin.skin in the
            // same order as Apply() to keep them in sync.
            WriteEncryptedSkinId(local, s_originalSkinId);
            if (WriteStackSkin(stack, s_originalSkinId)) {
                ClearStackVector(stack);  // drop any push() entry
                UpdateCharacterDataStack(stack);
            }
        }
        Reset();
    }

    // One-shot tick driver. Call from main update loop.
    inline void Tick(bool enabled, int skinId) {
        if (!enabled) {
            Restore();
            return;
        }
        Apply(skinId);
    }

} // namespace CoreSkinChanger
