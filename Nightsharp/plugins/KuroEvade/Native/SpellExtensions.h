#pragma once

#include "EvadeHelper.h"
#include "MathUtilsCPA.h"
#include "Spell.h"

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <climits>

namespace Plugins::KuroEvade {

struct SpellExtensions final {
    static float GetSpellRadius(const SDK::Skillshot& spell, float extraSpellRadius = 0.0f) {
        if (spell.SData.SpellType == SDK::SpellType::SkillshotRing) {
            return static_cast<float>(spell.SData.Radius) + extraSpellRadius;
        }
        return static_cast<float>(spell.SData.Radius) + extraSpellRadius;
    }

    static int GetSpellDangerLevel(const SDK::Skillshot& spell) {
        return EvadeHelper::DangerValue(spell);
    }

    static const char* GetSpellDangerString(int dangerLevel) {
        switch (dangerLevel) {
        case 1: return "Low";
        case 3: return "High";
        case 4: return "Extreme";
        default: return "Normal";
        }
    }

    static bool HasProjectile(const SDK::Skillshot& spell) {
        return spell.SData.MissileSpeed > 0 && spell.SData.MissileSpeed != INT_MAX;
    }

    static Vec2 GetSpellProjection(const SDK::Skillshot& spell,
                                   const Vec2& pos,
                                   bool predictPos = false) {
        if (SDK::IsCircleSpellType(spell.SData.SpellType)) {
            return spell.EndPosition;
        }
        Vec2 start = spell.StartPosition;
        if (predictPos) {
            if (const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(&spell)) {
                start = missile->GetMissilePosition(0);
            }
        }
        bool onSegment = false;
        return MathUtilsCPA::ProjectOn(pos, start, spell.EndPosition, onSegment);
    }

    static bool CanHeroEvade(const SDK::Skillshot& spell,
                             const SDK::AIBaseClient& hero,
                             float& evadeTime,
                             float& spellHitTime) {
        const Vec2 heroPos = hero.ServerPosition().To2D();
        const float moveSpeed = std::max(50.0f, hero.MoveSpeed());
        const float radius = static_cast<float>(spell.SData.Radius);

        if (SDK::IsLineSpellType(spell.SData.SpellType)) {
            bool onSegment = false;
            const Vec2 segmentPoint = MathUtilsCPA::ProjectOn(
                heroPos, spell.StartPosition, spell.EndPosition, onSegment);
            evadeTime = 1000.0f *
                (radius - heroPos.Distance(segmentPoint) + hero.BoundingRadius()) / moveSpeed;
            spellHitTime = EvadeHelper::SpellHitTime(spell, segmentPoint);
            return spellHitTime > evadeTime;
        }

        if (SDK::IsCircleSpellType(spell.SData.SpellType)) {
            evadeTime = 1000.0f *
                (radius - heroPos.Distance(spell.EndPosition) + hero.BoundingRadius()) / moveSpeed;
            spellHitTime = EvadeHelper::SpellHitTime(spell, heroPos);
            return spellHitTime > evadeTime;
        }

        evadeTime = FLT_MAX;
        spellHitTime = FLT_MAX;
        return false;
    }
};

} // namespace Plugins::KuroEvade
