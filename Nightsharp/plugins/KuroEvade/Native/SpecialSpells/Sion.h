#pragma once

#include "SpecialSpellCommon.h"

#include <unordered_map>

namespace Plugins::KuroEvade::SpecialSpells {

struct Sion {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        (void)context;
        (void)result;
        return false;
    }

    static bool Update(SDK::Skillshot& skillshot) {
        if (_stricmp(skillshot.SData.SpellName.c_str(), "SionR") != 0) {
            return true;
        }

        const int now = SDK::Variables::TickCount();
        const int casterId = skillshot.Caster.NetworkId();
        auto& firstSeen = FirstSeen();
        if (casterId != 0 && firstSeen.find(casterId) == firstSeen.end()) {
            firstSeen[casterId] = now;
        }

        const SDK::AIBaseClient caster = skillshot.Caster;
        if (!caster.IsValid() || caster.IsDead()) {
            firstSeen.erase(casterId);
            return false;
        }

        const int elapsed = casterId != 0 ? now - firstSeen[casterId] : now - skillshot.StartTime;
        if (!caster.HasBuff("SionR") && elapsed > 600) {
            firstSeen.erase(casterId);
            return false;
        }

        const Vec2 facing = caster.Direction().To2D().Normalized();
        Vec2 direction(-facing.y, facing.x);
        if (direction.IsZero()) {
            direction = skillshot.Direction;
        }
        if (direction.IsZero()) {
            return true;
        }

        skillshot.SData.MissileSpeed = std::max(1, static_cast<int>(caster.MoveSpeed()));
        skillshot.StartTime = now - skillshot.SData.Delay;
        skillshot.StartPosition = caster.Position().To2D();
        skillshot.EndPosition = skillshot.StartPosition + direction * 1000.0f;
        RefreshLineGeometry(skillshot);
        return true;
    }

    static void Clear() {
        FirstSeen().clear();
    }

private:
    static std::unordered_map<int, int>& FirstSeen() {
        static std::unordered_map<int, int> firstSeen;
        return firstSeen;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
