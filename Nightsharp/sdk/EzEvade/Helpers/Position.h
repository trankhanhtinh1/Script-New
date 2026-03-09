#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Helpers/ObjectCache.h"
#include "sdk/EzEvade/Helpers/Situation.h"
#include "sdk/EzEvade/Helpers/PositionInfo.h"
#include "sdk/EzEvade/Spells/SpellRuntime.h"
#include "sdk/EzEvade/Spells/Spell.h"
#include <cfloat>
#include <cmath>
#include <vector>

namespace EzEvade {
namespace Position {

inline bool IsLeftOfLineSegment(const Vec2& pos, const Vec2& start, const Vec2& end) {
    return ((end.x - start.x) * (pos.y - start.y) - (end.y - start.y) * (pos.x - start.x)) > 0.0f;
}

inline bool InSkillShot(const Vec2& position, const Spell& spell, float radius, bool predictCollision = true) {
    if (!spell.Info) return false;

    if (spell.Type == SpellType::Line) {
        const Vec2 spellPos = spell.CurrentSpellPosition;
        const Vec2 spellEndPos = predictCollision ? SpellExtensions::GetSpellEndPosition(spell) : spell.EndPos;
        auto projection = SDK::GeometryAdv::ProjectOn(position, spellPos, spellEndPos);
        return projection.IsOnSegment && projection.SegmentPoint.Distance(position) <= spell.Radius + radius;
    }

    if (spell.Type == SpellType::Circular) {
        if (spell.Info->spellName == "VeigarEventHorizon") {
            const float d = position.Distance(spell.EndPos);
            return d <= spell.Radius + radius - ObjectCache::MyHeroCache.BoundingRadius
                && d >= spell.Radius + radius - ObjectCache::MyHeroCache.BoundingRadius - 125.0f;
        }
        if (spell.Info->spellName == "DariusCleave") {
            const float d = position.Distance(spell.EndPos);
            return d <= spell.Radius + radius - ObjectCache::MyHeroCache.BoundingRadius
                && d >= spell.Radius + radius - ObjectCache::MyHeroCache.BoundingRadius - 220.0f;
        }
        return position.Distance(spell.EndPos) <= spell.Radius + radius - ObjectCache::MyHeroCache.BoundingRadius;
    }

    if (spell.Type == SpellType::Arc) {
        if (IsLeftOfLineSegment(position, spell.StartPos, spell.EndPos)) {
            return false;
        }
        const float spellRange = spell.StartPos.Distance(spell.EndPos);
        const Vec2 midPoint = spell.StartPos + spell.Direction * (spellRange / 2.0f);
        return position.Distance(midPoint) <= spell.Radius + radius - ObjectCache::MyHeroCache.BoundingRadius;
    }

    if (spell.Type == SpellType::Cone) {
        return !IsLeftOfLineSegment(position, spell.CnStart, spell.CnLeft)
            && !IsLeftOfLineSegment(position, spell.CnLeft, spell.CnRight)
            && !IsLeftOfLineSegment(position, spell.CnRight, spell.CnStart);
    }

    return false;
}

inline int CheckPosDangerLevel(const Vec2& pos, float extraBuffer) {
    int dangerLevel = 0;
    for (const auto* spell : SpellRuntime::ActiveSpells()) {
        if (!spell) continue;
        if (InSkillShot(pos, *spell, ObjectCache::MyHeroCache.BoundingRadius + extraBuffer)) {
            dangerLevel += spell->Dangerlevel;
        }
    }
    return dangerLevel;
}

inline float GetDistanceToTurrets(const Vec2& pos) {
    float minDist = FLT_MAX;
    const auto& me = SDK::GameObjects::Player;
    if (!me.IsValid()) return minDist;

    for (const auto& [id, turret] : ObjectCache::Turrets) {
        (void)id;
        if (!turret.IsValid() || turret.IsDead()) continue;
        if (turret.GetTeam() == me.GetTeam()) continue;
        minDist = std::min(minDist, pos.Distance(turret.GetPosition().To2D()));
    }
    return minDist;
}

inline float GetDistanceToChampions(const Vec2& pos) {
    float minDist = FLT_MAX;
    for (const auto& hero : SDK::GameObjects::EnemyHeroes) {
        if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) continue;
        minDist = std::min(minDist, hero.GetServerPosition().To2D().Distance(pos));
    }
    return minDist;
}

inline bool HasExtraAvoidDistance(const Vec2& pos, float extraBuffer) {
    for (const auto* spell : SpellRuntime::ActiveSpells()) {
        if (!spell) continue;
        if (spell->Type != SpellType::Line) continue;
        if (InSkillShot(pos, *spell, ObjectCache::MyHeroCache.BoundingRadius + extraBuffer)) {
            return true;
        }
    }
    return false;
}

inline float GetEnemyPositionValue(const Vec2& pos) {
    float posValue = 0.0f;
    if (ObjectCache::Menu.GetBool("PreventDodgingNearEnemy", true)) {
        const float minComfortDistance = (float)ObjectCache::Menu.GetSlider("MinComfortZone", 550);
        for (const auto& hero : SDK::GameObjects::EnemyHeroes) {
            if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) continue;
            const float dist = hero.GetServerPosition().To2D().Distance(pos);
            if (minComfortDistance > dist) {
                posValue += 2.0f * (minComfortDistance - dist);
            }
        }
    }
    return posValue;
}

inline float GetPositionValue(const Vec2& pos) {
    const Vec3 mouseWorld = SDK::Game::GetMouseWorldPos();
    float posValue = pos.Distance(mouseWorld.To2D());

    if (ObjectCache::Menu.GetBool("PreventDodgingUnderTower", false)) {
        const float turretRange = 875.0f + ObjectCache::MyHeroCache.BoundingRadius;
        const float distanceToTurrets = GetDistanceToTurrets(pos);
        if (turretRange > distanceToTurrets) {
            posValue += 5.0f * (turretRange - distanceToTurrets);
        }
    }

    return posValue;
}

inline bool CheckDangerousPos(const Vec2& pos, float extraBuffer, bool checkOnlyDangerous = false) {
    for (const auto* spell : SpellRuntime::ActiveSpells()) {
        if (!spell) continue;
        if (checkOnlyDangerous && spell->Dangerlevel < 3) continue;
        if (InSkillShot(pos, *spell, ObjectCache::MyHeroCache.BoundingRadius + extraBuffer)) {
            return true;
        }
    }
    return false;
}

inline std::vector<Vec2> GetSurroundingPositions(int maxPosToCheck = 150, int posRadius = 25) {
    constexpr double kTwoPi = 6.28318530717958647692;
    std::vector<Vec2> positions;
    int posChecked = 0;
    int radiusIndex = 0;

    const Vec2 heroPoint = ObjectCache::MyHeroCache.ServerPos2D;
    while (posChecked < maxPosToCheck) {
        radiusIndex++;
        const int curRadius = radiusIndex * (2 * posRadius);
        const int curCircleChecks = (int)ceil((kTwoPi * (double)curRadius) / (2.0 * (double)posRadius));

        for (int i = 1; i < curCircleChecks; i++) {
            posChecked++;
            const float radians = (float)((kTwoPi / (curCircleChecks - 1)) * i);
            const Vec2 p(
                floorf(heroPoint.x + (float)curRadius * cosf(radians)),
                floorf(heroPoint.y + (float)curRadius * sinf(radians))
            );
            positions.push_back(p);
            if (posChecked >= maxPosToCheck) {
                break;
            }
        }
    }

    return positions;
}

} // namespace Position
} // namespace EzEvade
