#pragma once

#include "CoreRuntime.h"
#include "Offsets.h"
#include "Vector.h"

#include <cstdint>

namespace CoreSpellCastInfo {

    struct CastRef {
        uintptr_t address = 0;

        bool IsValid() const {
            return Globals::IsValidPtr(address);
        }

        uintptr_t GetSpellData() const {
            return Globals::Read<uintptr_t>(address + Offset::SpellCastInfo::SpellData);
        }

        int GetSourceIndex() const {
            return Globals::Read<int>(address + Offset::SpellCastInfo::SrcIndex);
        }

        int GetTargetIndex() const {
            return Globals::Read<int>(address + Offset::SpellCastInfo::TargetIndex);
        }

        Vec3 GetStartPos() const {
            return Globals::Read<Vec3>(address + Offset::SpellCastInfo::StartPos);
        }

        Vec3 GetEndPos() const {
            return Globals::Read<Vec3>(address + Offset::SpellCastInfo::EndPos);
        }

        Vec3 GetCastPos() const {
            return Globals::Read<Vec3>(address + Offset::SpellCastInfo::CastPos);
        }

        float GetCastDelay() const {
            return Globals::Read<float>(address + Offset::SpellCastInfo::CastDelay);
        }

        int GetSlot() const {
            return Globals::Read<int>(address + Offset::SpellCastInfo::Slot);
        }

        bool IsSpell() const {
            return Globals::Read<uint8_t>(address + Offset::SpellCastInfo::IsSpell) != 0;
        }

        bool IsSpecialAttack() const {
            return Globals::Read<uint8_t>(address + Offset::SpellCastInfo::IsSpecialAttack) != 0;
        }

        bool IsAutoAttack() const {
            return Globals::Read<uint8_t>(address + Offset::SpellCastInfo::IsAuto) != 0;
        }

        /// Read missile speed from SpellData resource.
        /// Matches EnsoulSharp: args.SData.MissileSpeed
        /// NOTE: ResMissileSpeed offset is STALE in 26.7 (CE+IDA verified).
        /// Clamps garbage to 0.0f so callers fall back to static DB values.
        float GetMissileSpeed() const {
            const auto spellData = GetSpellData();
            if (!Globals::IsValidPtr(spellData)) {
                return 0.0f;
            }
            const auto resource = Globals::Read<uintptr_t>(spellData + Offset::SpellBook::DataResourceBase);
            if (!Globals::IsValidPtr(resource)) {
                return 0.0f;
            }
            const float raw = Globals::Read<float>(resource + Offset::SpellBook::ResMissileSpeed);
            if (!(raw == raw) || raw < 0.0f || raw > 50000.0f) return 0.0f;
            return raw;
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

            return Globals::ReadRuntimeStringField(spellData + Offset::SpellBook::DataSpellName, out, maxOut);
        }
    };

    inline CastRef GetActive(uintptr_t obj) {
        if (!Globals::IsValidPtr(obj)) {
            return {};
        }
        return { Globals::Read<uintptr_t>(obj + Offset::SpellBook::ActiveSpellCastObject) };
    }

    inline CastRef GetMissileCast(uintptr_t missile) {
        if (!Globals::IsValidPtr(missile)) {
            return {};
        }
        return { missile + Offset::Missile::CastInfoBase };
    }

} // namespace CoreSpellCastInfo
