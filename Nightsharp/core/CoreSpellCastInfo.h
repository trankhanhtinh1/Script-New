#pragma once

// ============================================================================
// CoreSpellCastInfo.h - SpellCastInfo struct wrappers
// ----------------------------------------------------------------------------
// The game exposes TWO different SpellCastInfo layouts (see offset.h):
//
//   1. `SpellCastInfoLayout`        - pointed to by hero->spellBook->activeSpellCast
//                                     and used by GetSpellCastInfo(slot).
//                                     Verified for build 26.6.
//                                     Has: SpellSlot / State / StartTime / EndTime /
//                                          ChannelStart / ChannelEnd / StartPosition /
//                                          EndPosition / TargetNetId / CasterNetId.
//
//   2. `SpellCastInfoEventLayout`   - embedded inside MissileClient at CastInfoBase
//                                     (0x2C0) and delivered in OnProcessSpell / event
//                                     callbacks. Older OLD-build offsets. Has:
//                                          SpellData / SrcIndex / TargetIndex / StartPos /
//                                          EndPos / CastPos / CastDelay / IsSpell /
//                                          IsSpecialAttack / IsAuto / Slot.
//
// The old NightSharp CoreSpellCastInfo mixed both layouts on the same pointer,
// which was a latent bug. This port keeps them cleanly separated: `CastRef`
// reads the active-cast layout, `MissileCastRef` reads the event-style layout
// embedded in a missile object.
// ============================================================================

#include "CoreRuntime.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"

#include <cstdint>

namespace CoreSpellCastInfo {

    // ── Active-cast reference (SpellBook::activeSpellCast → NEW layout) ──

    struct CastRef {
        uintptr_t address = 0;

        bool IsValid() const {
            return Globals::IsValidPtr(address);
        }

        int GetSlot() const {
            return static_cast<int>(Globals::Read<uint8_t>(address + Offset::SpellCastInfoLayout::SpellSlot));
        }

        uint32_t GetState() const {
            return Globals::Read<uint32_t>(address + Offset::SpellCastInfoLayout::State);
        }

        float GetStartTime() const {
            return Globals::Read<float>(address + Offset::SpellCastInfoLayout::StartTime);
        }

        float GetEndTime() const {
            return Globals::Read<float>(address + Offset::SpellCastInfoLayout::EndTime);
        }

        float GetChannelStart() const {
            return Globals::Read<float>(address + Offset::SpellCastInfoLayout::ChannelStart);
        }

        float GetChannelEnd() const {
            return Globals::Read<float>(address + Offset::SpellCastInfoLayout::ChannelEnd);
        }

        Vec3 GetStartPos() const {
            return Globals::Read<Vec3>(address + Offset::SpellCastInfoLayout::StartPosition);
        }

        Vec3 GetEndPos() const {
            return Globals::Read<Vec3>(address + Offset::SpellCastInfoLayout::EndPosition);
        }

        uint32_t GetTargetNetId() const {
            return Globals::Read<uint32_t>(address + Offset::SpellCastInfoLayout::TargetNetId);
        }

        uint32_t GetCasterNetId() const {
            return Globals::Read<uint32_t>(address + Offset::SpellCastInfoLayout::CasterNetId);
        }

        float GetRemaining(float gameTime) const {
            const float endTime = GetEndTime();
            return endTime > gameTime ? (endTime - gameTime) : 0.0f;
        }

        bool IsChanneling() const {
            return GetChannelEnd() > 0.0f;
        }

        // ── Phase 2 (Apr 26/2026) backward-compat shims ──────────────────────
        // Pre-Phase-2 SDK code (SpellCastTracker, AIBaseClient) expects these
        // methods on `CastRef`. After the layout split they were moved to
        // `MissileCastRef`. The shims below either compute from NEW layout
        // fields (CastDelay = End - Start) or fall back to neutral defaults
        // when the data isn't accessible from active-cast layout. This keeps
        // the SDK chain compiling while the real fix (rewire SDK callsites
        // to MissileCastRef where appropriate) is staged in Phase 2.10+.

        // Cast delay derived from cast window (ms semantics of game match SDK
        // expectations of "delay until projectile launch / damage").
        float GetCastDelay() const {
            const float start = GetStartTime();
            const float end   = GetEndTime();
            return end > start ? (end - start) : 0.0f;
        }

        // CastPos for active-cast = the destination point, same as GetEndPos.
        Vec3 GetCastPos() const { return GetEndPos(); }

        // Active-cast doesn't expose IsAuto/IsSpecial flags directly. Slot
        // sentinel (64 = AA) is checked by SpellCastTracker via slotParam.
        bool IsAutoAttack()    const { return false; }
        bool IsSpecialAttack() const { return false; }

        // Target index ~ targetNetId (different scaling but populates field).
        int GetTargetIndex() const { return static_cast<int>(GetTargetNetId()); }

        // Active-cast missile speed: walk through SpellBook->Slot[GetSlot()]
        // ->SpellData->Resource->ResMissileSpeed when needed. Currently SDK
        // only uses this for orbwalker windup which has its own fallback,
        // so returning 0 here is safe (treated as "instant").
        float GetMissileSpeed() const { return 0.0f; }

        // Active-cast spell name lookup via SpellBook is non-trivial (need
        // slot lookup + spell data read). SDK's only callsite uses this for
        // process-spell logging; safe fallback writes empty string.
        bool ReadSpellName(char* out, int maxOut) const {
            if (out && maxOut > 0) out[0] = 0;
            return false;
        }
    };

    // ── Event / missile-embedded cast (MissileClient::CastInfoBase → OLD layout) ──

    struct MissileCastRef {
        uintptr_t address = 0;

        bool IsValid() const {
            return Globals::IsValidPtr(address);
        }

        uintptr_t GetSpellData() const {
            return Globals::Read<uintptr_t>(address + Offset::SpellCastInfoEventLayout::SpellData);
        }

        int GetSourceIndex() const {
            return Globals::Read<int>(address + Offset::SpellCastInfoEventLayout::SrcIndex);
        }

        int GetTargetIndex() const {
            return Globals::Read<int>(address + Offset::SpellCastInfoEventLayout::TargetIndex);
        }

        Vec3 GetStartPos() const {
            return Globals::Read<Vec3>(address + Offset::SpellCastInfoEventLayout::StartPos);
        }

        Vec3 GetEndPos() const {
            return Globals::Read<Vec3>(address + Offset::SpellCastInfoEventLayout::EndPos);
        }

        Vec3 GetCastPos() const {
            return Globals::Read<Vec3>(address + Offset::SpellCastInfoEventLayout::CastPos);
        }

        float GetCastDelay() const {
            return Globals::Read<float>(address + Offset::SpellCastInfoEventLayout::CastDelay);
        }

        int GetSlot() const {
            return Globals::Read<int>(address + Offset::SpellCastInfoEventLayout::Slot);
        }

        bool IsSpell() const {
            return Globals::Read<uint8_t>(address + Offset::SpellCastInfoEventLayout::IsSpell) != 0;
        }

        bool IsSpecialAttack() const {
            return Globals::Read<uint8_t>(address + Offset::SpellCastInfoEventLayout::IsSpecialAttack) != 0;
        }

        bool IsAutoAttack() const {
            return Globals::Read<uint8_t>(address + Offset::SpellCastInfoEventLayout::IsAuto) != 0;
        }

        // Read missile speed from SpellData resource.
        // Match chain: cast.SpellData -> SpellData.DataResource -> resource.ResMissileSpeed.
        float GetMissileSpeed() const {
            const auto spellData = GetSpellData();
            if (!Globals::IsValidPtr(spellData)) {
                return 0.0f;
            }
            const auto resource = Globals::Read<uintptr_t>(spellData + Offset::SpellDataLayout::DataResource);
            if (!Globals::IsValidPtr(resource)) {
                return 0.0f;
            }
            return Globals::Read<float>(resource + Offset::SpellDataResourceLayout::ResMissileSpeed);
        }

        bool ReadSpellName(char* out, int maxOut) const {
            if (!out || maxOut <= 0) {
                return false;
            }

            const auto spellData = GetSpellData();
            if (!Globals::IsValidPtr(spellData)) {
                out[0] = 0;
                return false;
            }

            return Globals::ReadRuntimeStringField(spellData + Offset::SpellDataLayout::DataSpellName, out, maxOut);
        }
    };

    // ── Accessors ──

    inline CastRef GetActive(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) {
            return {};
        }
        return { Globals::Read<uintptr_t>(obj + Offset::SpellRuntime::ActiveSpellCast) };
    }

    inline MissileCastRef GetMissileCast(uintptr_t missile) {
        if (!Globals::IsValidPtr(missile)) {
            return {};
        }
        return { missile + Offset::MissileClient::CastInfoBase };
    }

} // namespace CoreSpellCastInfo
