#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Seraphine {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        (void)context;
        (void)result;
        return false;
    }

    static void Update(SDK::Skillshot& skillshot) {
        if (_stricmp(skillshot.SData.SpellName.c_str(), "SeraphineR") != 0) {
            return;
        }

        auto* missileSkillshot = dynamic_cast<SDK::SkillshotMissile*>(&skillshot);
        if (!missileSkillshot || !missileSkillshot->Missile.IsValid()) {
            return;
        }

        const Vec2 missileStart = missileSkillshot->Missile.StartPosition().To2D();
        const Vec2 missileEnd = missileSkillshot->Missile.EndPosition().To2D();
        Vec2 direction = (missileEnd - missileStart).Normalized();
        if (direction.IsZero()) {
            direction = skillshot.Direction;
        }
        if (direction.IsZero()) {
            return;
        }

        const float dynamicRange = std::max(
            static_cast<float>(skillshot.SData.Range),
            missileStart.Distance(missileEnd));
        const Vec2 newEnd = skillshot.StartPosition + direction * dynamicRange;
        if (newEnd.DistanceSqr(skillshot.EndPosition) <= 1.0f) {
            return;
        }

        skillshot.EndPosition = newEnd;
        RefreshLineGeometry(skillshot);
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
