#pragma once

#include "Spell.h"
#include "SpellDetector.h"

#include <cfloat>

namespace Plugins::KuroEvade {

struct SpellTester final {
    static Spell FromSkillshot(const SDK::Skillshot& skillshot) {
        return Spell(skillshot);
    }

    static int CountDangerousAt(const SpellDetector::SkillshotList& skillshots,
                                const Vec2& position,
                                float radius) {
        int count = 0;
        for (const auto& skillshot : skillshots) {
            if (skillshot && EvadeHelper::InSkillShot(*skillshot, position, radius)) {
                ++count;
            }
        }
        return count;
    }

    static const SDK::Skillshot* ClosestHit(const SpellDetector::SkillshotList& skillshots,
                                            const Vec2& position,
                                            float radius) {
        const SDK::Skillshot* best = nullptr;
        float bestHitTime = FLT_MAX;
        for (const auto& skillshot : skillshots) {
            if (!skillshot || !EvadeHelper::InSkillShot(*skillshot, position, radius)) {
                continue;
            }
            const float hitTime = EvadeHelper::SpellHitTime(*skillshot, position);
            if (hitTime < bestHitTime) {
                bestHitTime = hitTime;
                best = skillshot.get();
            }
        }
        return best;
    }
};

} // namespace Plugins::KuroEvade
