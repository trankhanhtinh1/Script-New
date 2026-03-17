#pragma once
#include <vector>
#include <cmath>
#include <cfloat>
#include "../../GameObjects/GameObjects.h"
#include "../../Math/MathUtils.h"
#include "ObjectCache.h"
#include "PositionInfo.h"
#include "../Spells/Spell.h"
#include "../Utils/EvadeUtils.h"


namespace EzEvade {

    namespace Position {

        // isLeftOfLineSegment  (C# lines 84-87)
        inline bool IsLeftOfLineSegment(const Vec2& pos, const Vec2& start, const Vec2& end) {
            return ((end.x - start.x) * (pos.y - start.y) -
                    (end.y - start.y) * (pos.x - start.x)) > 0;
        }

        // InSkillShot  (C# lines 34-82)
        inline bool InSkillShot(const Vec2& position, const Spell& spell,
                                float radius, bool predictCollision = true) {
            if (spell.spellType == SpellType::Line)
            {
                Vec2 spellPos = spell.currentSpellPosition;
                Vec2 spellEndPos = predictCollision ?
                    spell.GetSpellEndPosition() : spell.endPos;

                auto projection = Vec2_ProjectOn(position, spellPos, spellEndPos);
                return projection.isOnSegment &&
                       projection.segmentPoint.Distance(position) <= spell.radius + radius;
            }

            if (spell.spellType == SpellType::Circular)
            {
                if (spell.info.spellName == "VeigarEventHorizon")
                {
                    return position.Distance(spell.endPos) <=
                                spell.radius + radius - ObjectCache::myHeroCache.boundingRadius
                        && position.Distance(spell.endPos) >=
                                spell.radius + radius - ObjectCache::myHeroCache.boundingRadius - 125;
                }
                if (spell.info.spellName == "DariusCleave")
                {
                    return position.Distance(spell.endPos) <=
                                spell.radius + radius - ObjectCache::myHeroCache.boundingRadius
                        && position.Distance(spell.endPos) >=
                                spell.radius + radius - ObjectCache::myHeroCache.boundingRadius - 220;
                }

                return position.Distance(spell.endPos) <=
                    spell.radius + radius - ObjectCache::myHeroCache.boundingRadius;
            }

            if (spell.spellType == SpellType::Arc)
            {
                if (IsLeftOfLineSegment(position, spell.startPos, spell.endPos))
                    return false;

                float spellRange = spell.startPos.Distance(spell.endPos);
                Vec2 midPoint = spell.startPos + spell.direction * (spellRange / 2.0f);

                return position.Distance(midPoint) <=
                    spell.radius + radius - ObjectCache::myHeroCache.boundingRadius;
            }

            if (spell.spellType == SpellType::Cone)
            {
                return !IsLeftOfLineSegment(position, spell.cnStart, spell.cnLeft) &&
                       !IsLeftOfLineSegment(position, spell.cnLeft,  spell.cnRight) &&
                       !IsLeftOfLineSegment(position, spell.cnRight, spell.cnStart);
            }

            return false;
        }

        // CheckPosDangerLevel  (C# lines 19-32)
        inline int CheckPosDangerLevel(const Vec2& pos, float extraBuffer,
                                        const std::map<int, Spell>& spells) {
            int dangerlevel = 0;
            for (const auto& entry : spells)
            {
                const Spell& spell = entry.second;
                if (InSkillShot(pos, spell,
                    ObjectCache::myHeroCache.boundingRadius + extraBuffer))
                {
                    dangerlevel += spell.dangerlevel;
                }
            }
            return dangerlevel;
        }

        // GetDistanceToTurrets  (C# lines 89-113)
        inline float GetDistanceToTurrets(const Vec2& pos) {
            float minDist = FLT_MAX;
            for (const auto& entry : ObjectCache::turrets)
            {
                auto* turret = entry.second;
                if (turret == nullptr || !turret->IsValid() || !turret->IsAlive())
                    continue;
                if (turret->GetTeam() == SDK::GameObjects::Player.GetTeam())
                    continue;
                float distToTurret = pos.Distance(turret->GetPosition().To2D());
                minDist = std::min(minDist, distToTurret);
            }
            return minDist;
        }

        // GetDistanceToChampions  (C# lines 115-131)
        inline float GetDistanceToChampions(const Vec2& pos) {
            float minDist = FLT_MAX;
            for (const auto& hero : SDK::GameObjects::EnemyHeroes)
            {
                if (hero.IsValid() && hero.IsAlive() && hero.IsVisible())
                {
                    Vec2 heroPos = hero.GetPosition().To2D();
                    float dist = heroPos.Distance(pos);
                    minDist = std::min(minDist, dist);
                }
            }
            return minDist;
        }

        // HasExtraAvoidDistance  (C# lines 133-148)
        inline bool HasExtraAvoidDistance(const Vec2& pos, float extraBuffer,
                                          const std::map<int, Spell>& spells) {
            for (const auto& entry : spells)
            {
                const Spell& spell = entry.second;
                if (spell.spellType == SpellType::Line)
                {
                    if (InSkillShot(pos, spell,
                        ObjectCache::myHeroCache.boundingRadius + extraBuffer))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        // GetEnemyPositionValue  (C# lines 150-174)
        inline float GetEnemyPositionValue(const Vec2& pos) {
            float posValue = 0;
            bool preventNearEnemy = ObjectCache::GetBool("PreventDodgingNearEnemy");

            if (preventNearEnemy)
            {
                float minComfortDistance = (float)ObjectCache::GetSlider("MinComfortZone");

                for (const auto& hero : SDK::GameObjects::EnemyHeroes)
                {
                    if (hero.IsValid() && hero.IsAlive() && hero.IsVisible())
                    {
                        Vec2 heroPos = hero.GetPosition().To2D();
                        float dist = heroPos.Distance(pos);

                        if (minComfortDistance > dist)
                        {
                            posValue += 2 * (minComfortDistance - dist);
                        }
                    }
                }
            }

            return posValue;
        }

        // GetPositionValue  (C# lines 176-192)
        inline float GetPositionValue(const Vec2& pos, const Vec2& cursorPos) {
            float posValue = pos.Distance(cursorPos);
            bool preventUnderTower = ObjectCache::GetBool("PreventDodgingUnderTower");

            if (preventUnderTower)
            {
                float turretRange = 875 + ObjectCache::myHeroCache.boundingRadius;
                float distanceToTurrets = GetDistanceToTurrets(pos);

                if (turretRange > distanceToTurrets)
                {
                    posValue += 5 * (turretRange - distanceToTurrets);
                }
            }

            return posValue;
        }

        // CheckDangerousPos  (C# lines 194-211)
        inline bool CheckDangerousPos(const Vec2& pos, float extraBuffer,
                                       const std::map<int, Spell>& spells,
                                       bool checkOnlyDangerous = false) {
            for (const auto& entry : spells)
            {
                const Spell& spell = entry.second;
                if (checkOnlyDangerous && spell.dangerlevel < 3)
                    continue;
                if (InSkillShot(pos, spell,
                    ObjectCache::myHeroCache.boundingRadius + extraBuffer))
                {
                    return true;
                }
            }
            return false;
        }

        // GetSurroundingPositions  (C# lines 213-244)
        inline std::vector<Vec2> GetSurroundingPositions(
            int maxPosToCheck = 150, int posRadius = 25) {

            std::vector<Vec2> positions;
            int posChecked = 0;
            int radiusIndex = 0;

            Vec2 heroPoint = ObjectCache::myHeroCache.serverPos2D;
            constexpr float PI = 3.14159265358979323846f;

            while (posChecked < maxPosToCheck)
            {
                radiusIndex++;
                int curRadius = radiusIndex * (2 * posRadius);
                int curCircleChecks = (int)std::ceil(
                    (2.0 * PI * (double)curRadius) / (2.0 * (double)posRadius));

                for (int i = 1; i < curCircleChecks; i++)
                {
                    posChecked++;
                    double cRadians = (2.0 * PI / (curCircleChecks - 1)) * i;

                    Vec2 pos;
                    pos.x = std::floor(heroPoint.x + curRadius * (float)std::cos(cRadians));
                    pos.y = std::floor(heroPoint.y + curRadius * (float)std::sin(cRadians));

                    positions.push_back(pos);
                }
            }

            return positions;
        }

        // IsNearEnemy (used by EvadeHelper — checks if pos is within range of enemy champs)
        inline bool IsNearEnemy(const Vec2& pos, float range) {
            for (const auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (hero.IsValid() && hero.IsAlive() && hero.IsVisible()) {
                    if (hero.GetPosition().To2D().Distance(pos) <= range)
                        return true;
                }
            }
            return false;
        }

        // Convenience overloads that auto-supply SpellDetector::spells
        // Uses extern forward-decl to avoid circular includes
        namespace detail {
            // SpellDetector::spells is defined in SpellDetector.cpp
            std::map<int, Spell>& GetDetectorSpells();
        }

        // 2-arg overload: CheckDangerousPos(pos, extraBuffer)
        inline bool CheckDangerousPos(const Vec2& pos, float extraBuffer) {
            return CheckDangerousPos(pos, extraBuffer, detail::GetDetectorSpells());
        }

        // 1-arg overload: GetPositionValue(pos) — uses cached server pos as fallback cursor
        inline float GetPositionValue(const Vec2& pos) {
            Vec2 cursorPos = ObjectCache::myHeroCache.serverPos2D;
            return GetPositionValue(pos, cursorPos);
        }

        // 2-arg overload: HasExtraAvoidDistance(pos, extraBuffer)
        inline bool HasExtraAvoidDistance(const Vec2& pos, float extraBuffer) {
            return HasExtraAvoidDistance(pos, extraBuffer, detail::GetDetectorSpells());
        }

    } // namespace Position

} // namespace EzEvade
