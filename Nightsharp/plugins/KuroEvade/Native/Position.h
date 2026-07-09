#pragma once

#include "EvadeHelper.h"
#include "ObjectCache.h"
#include "Situation.h"

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Plugins::KuroEvade {

struct Position final {
    using SkillshotList = EvadeHelper::SkillshotList;

    static int CheckPosDangerLevel(const Vec2& pos,
                                   float extraBuffer,
                                   const EvadeSettings& settings,
                                   const SkillshotList& skillshots) {
        int danger = 0;
        EvadeHelper helper(settings);
        for (const auto& spell : skillshots) {
            if (spell && helper.ShouldConsiderSpell(*spell) &&
                EvadeHelper::InSkillShot(*spell, pos, ObjectCache::PlayerCache().boundingRadius + extraBuffer)) {
                danger += EvadeHelper::DangerValue(*spell);
            }
        }
        return danger;
    }

    static bool InSkillShot(const Vec2& position,
                            const SDK::Skillshot& spell,
                            float radius,
                            bool /*predictCollision*/ = true) {
        return EvadeHelper::InSkillShot(spell, position, radius);
    }

    static bool IsLeftOfLineSegment(const Vec2& pos, const Vec2& start, const Vec2& end) {
        return (end.x - start.x) * (pos.y - start.y) -
               (end.y - start.y) * (pos.x - start.x) > 0.0f;
    }

    static float GetDistanceToTurrets(const Vec2& pos) {
        return Situation::DistanceToEnemyTurret(pos);
    }

    static float GetDistanceToChampions(const Vec2& pos) {
        return Situation::DistanceToEnemyChampion(pos);
    }

    static bool HasExtraAvoidDistance(const Vec2& pos,
                                      float extraBuffer,
                                      const EvadeSettings& settings,
                                      const SkillshotList& skillshots) {
        EvadeHelper helper(settings);
        for (const auto& spell : skillshots) {
            if (!spell || !SDK::IsLineSpellType(spell->SData.SpellType)) {
                continue;
            }
            if (helper.ShouldConsiderSpell(*spell) &&
                EvadeHelper::InSkillShot(*spell, pos, ObjectCache::PlayerCache().boundingRadius + extraBuffer)) {
                return true;
            }
        }
        return false;
    }

    static float GetEnemyPositionValue(const Vec2& pos, const EvadeSettings& settings) {
        if (!settings.PreventEnemy) {
            return 0.0f;
        }

        float value = 0.0f;
        for (const auto& enemy : SDK::GameObjects::EnemyHeroes()) {
            if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) {
                continue;
            }
            const float distance = enemy.ServerPosition().To2D().Distance(pos);
            if (distance < settings.MinComfortZone) {
                value += 2.0f * (settings.MinComfortZone - distance);
            }
        }
        return value;
    }

    static float GetPositionValue(const Vec2& pos, const EvadeSettings& settings) {
        float value = pos.Distance(SDK::Game::CursorPos().To2D());
        if (settings.PreventTower) {
            const float turretRadius = 875.0f + ObjectCache::PlayerCache().boundingRadius;
            const float turretDistance = GetDistanceToTurrets(pos);
            if (turretDistance < turretRadius) {
                value += 5.0f * (turretRadius - turretDistance);
            }
        }
        return value + GetEnemyPositionValue(pos, settings);
    }

    static bool CheckDangerousPos(const Vec2& pos,
                                  float extraBuffer,
                                  const EvadeSettings& settings,
                                  const SkillshotList& skillshots,
                                  bool checkOnlyDangerous = false) {
        EvadeHelper helper(settings);
        for (const auto& spell : skillshots) {
            if (!spell || !helper.ShouldConsiderSpell(*spell)) {
                continue;
            }
            if (checkOnlyDangerous && EvadeHelper::DangerValue(*spell) < 3) {
                continue;
            }
            if (EvadeHelper::InSkillShot(*spell, pos, ObjectCache::PlayerCache().boundingRadius + extraBuffer)) {
                return true;
            }
        }
        return false;
    }

    static std::vector<const SDK::Skillshot*> CheckSkillshotPos(const Vec2& pos,
                                                                float extraBuffer,
                                                                const EvadeSettings& settings,
                                                                const SkillshotList& skillshots,
                                                                bool checkOnlyDangerous = false) {
        std::vector<const SDK::Skillshot*> result;
        EvadeHelper helper(settings);
        for (const auto& spell : skillshots) {
            if (!spell || !helper.ShouldConsiderSpell(*spell)) {
                continue;
            }
            if (checkOnlyDangerous && EvadeHelper::DangerValue(*spell) < 3) {
                continue;
            }
            if (EvadeHelper::InSkillShot(*spell, pos, ObjectCache::PlayerCache().boundingRadius + extraBuffer)) {
                result.push_back(spell.get());
            }
        }
        return result;
    }

    static std::vector<Vec2> GetSurroundingPositions(const Vec2& center,
                                                     int maxPosToCheck = 150,
                                                     int posRadius = 25) {
        std::vector<Vec2> result;
        int checked = 0;
        int radiusIndex = 0;
        constexpr double twoPi = 6.28318530717958647692;
        while (checked < maxPosToCheck) {
            ++radiusIndex;
            const int curRadius = radiusIndex * 2 * posRadius;
            const int circleChecks = std::max(2,
                static_cast<int>(std::ceil(twoPi * curRadius / (2.0 * posRadius))));
            for (int i = 1; i < circleChecks && checked < maxPosToCheck; ++i) {
                ++checked;
                const double rad = twoPi / static_cast<double>(circleChecks - 1) * i;
                result.emplace_back(
                    std::floor(center.x + curRadius * static_cast<float>(std::cos(rad))),
                    std::floor(center.y + curRadius * static_cast<float>(std::sin(rad))));
            }
        }
        return result;
    }
};

} // namespace Plugins::KuroEvade
