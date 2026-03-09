#pragma once
#include "sdk/GameObjects/GameObjects.h"
#include "sdk/UI/MenuUI.h"
#include "sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "sdk/Wrappers/Spells/SpellCaster.h"
#include "sdk/Wrappers/TargetSelector/TargetSelector.h"
#include <cmath>
#include <vector>

namespace SDK {
namespace DashUtils {

    struct DashConfig {
        int DashMode = 2;                  // 0=Cursor, 1=Side, 2=Safe
        int EnemyCheck = 3;
        bool WallCheck = true;
        bool TurretCheck = true;
        bool RequireAARange = true;
        int SafeCirclePoints = 15;
        float SafeEnemyRange = 350.0f;
        float EnemyCheckRange = 600.0f;
        float PlayerEnemyCheckRange = 400.0f;
        float SideTargetExtraRange = 800.0f;
    };

    inline std::vector<Vec3> GetCirclePoints(int count, float radius, const Vec3& center) {
        std::vector<Vec3> points;
        if (count <= 0 || radius <= 0.0f) {
            return points;
        }

        points.reserve((size_t)count);
        constexpr float kTwoPi = 6.28318530718f;
        const float step = kTwoPi / (float)count;
        for (int i = 0; i < count; ++i) {
            const float angle = step * (float)i;
            points.emplace_back(
                center.x + radius * cosf(angle),
                center.y,
                center.z + radius * sinf(angle));
        }
        return points;
    }

    inline bool InAARange(const Vec3& point, const DashConfig& cfg) {
        if (!cfg.RequireAARange) {
            return true;
        }

        const auto& player = GameObjects::Player;
        if (!player.IsValid()) {
            return false;
        }

        GameObject target;
        if (Orbwalker::ForceTarget.IsValid() &&
            Orbwalker::ForceTarget.IsHero() &&
            !Orbwalker::ForceTarget.IsDead()) {
            target = Orbwalker::ForceTarget;
        } else if (Orbwalker::LastTarget.IsValid() &&
                   Orbwalker::LastTarget.IsHero() &&
                   !Orbwalker::LastTarget.IsDead()) {
            target = Orbwalker::LastTarget;
        }

        if (target.IsValid()) {
            return point.Distance2D(target.GetPosition()) < player.GetAttackRange();
        }

        return GameObjects::CountEnemyHeroesInRange(player.GetAttackRange(), point) > 0;
    }

    inline bool IsGoodPosition(const Vec3& dashPos, const SpellCaster& dashSpell, const DashConfig& cfg) {
        const auto& player = GameObjects::Player;
        if (!player.IsValid()) {
            return false;
        }

        if (cfg.WallCheck) {
            const float segment = dashSpell.Range / 5.0f;
            for (int i = 1; i <= 5; ++i) {
                if (GameObject::IsWallAt(player.GetPosition().Extend(dashPos, i * segment))) {
                    return false;
                }
            }
        }

        if (cfg.TurretCheck && GameObjects::IsUnderEnemyTurret(dashPos)) {
            return false;
        }

        const int enemyCountDashPos = GameObjects::CountEnemyHeroesInRange(cfg.EnemyCheckRange, dashPos);
        if (cfg.EnemyCheck > enemyCountDashPos) {
            return true;
        }

        const int enemyCountPlayer = GameObjects::CountEnemyHeroesInRange(cfg.PlayerEnemyCheckRange, player.GetPosition());
        return enemyCountDashPos <= enemyCountPlayer;
    }

    inline Vec3 CastDash(const SpellCaster& dashSpell, bool asap = false, const DashConfig& cfg = DashConfig()) {
        const auto& player = GameObjects::Player;
        if (!player.IsValid() || !dashSpell.IsReady()) {
            return Vec3();
        }

        const Vec3 playerPos = player.GetPosition();
        const Vec3 mousePos = Game::GetMouseWorldPos();
        if (!mousePos.IsValid() || mousePos.IsZero()) {
            return Vec3();
        }

        Vec3 bestPoint = Vec3();

        if (cfg.DashMode == 0) {
            bestPoint = playerPos.Extend(mousePos, dashSpell.Range);
        } else if (cfg.DashMode == 1) {
            GameObject orbTarget;
            if (Orbwalker::ForceTarget.IsValid() &&
                Orbwalker::ForceTarget.IsHero() &&
                !Orbwalker::ForceTarget.IsDead()) {
                orbTarget = Orbwalker::ForceTarget;
            } else {
                orbTarget = TargetSelector::GetTarget(dashSpell.Range + cfg.SideTargetExtraRange, DamageType::Physical);
            }

            if (orbTarget.IsValid() && orbTarget.IsHero()) {
                const Vec2 start(playerPos.x, playerPos.z);
                const Vec2 end(orbTarget.GetPosition().x, orbTarget.GetPosition().z);
                const Vec2 dir = (end - start).Normalized();
                const Vec2 pDir = dir.Perpendicular();
                const float distance = playerPos.Distance2D(orbTarget.GetPosition());

                const Vec2 rightEndPos = end + pDir * distance;
                const Vec2 leftEndPos = end - pDir * distance;

                const Vec3 rEndPos(rightEndPos.x, playerPos.y, rightEndPos.y);
                const Vec3 lEndPos(leftEndPos.x, playerPos.y, leftEndPos.y);

                if (mousePos.Distance2D(rEndPos) < mousePos.Distance2D(lEndPos)) {
                    bestPoint = playerPos.Extend(rEndPos, dashSpell.Range);
                } else {
                    bestPoint = playerPos.Extend(lEndPos, dashSpell.Range);
                }
            }
        } else {
            const auto points = GetCirclePoints(cfg.SafeCirclePoints, dashSpell.Range, playerPos);
            bestPoint = playerPos.Extend(mousePos, dashSpell.Range);
            int enemies = GameObjects::CountEnemyHeroesInRange(cfg.SafeEnemyRange, bestPoint);

            for (const auto& point : points) {
                const int count = GameObjects::CountEnemyHeroesInRange(cfg.SafeEnemyRange, point);
                if (!InAARange(point, cfg)) {
                    continue;
                }

                if (GameObjects::IsUnderAllyTurret(point)) {
                    bestPoint = point;
                    enemies = count - 1;
                } else if (count < enemies) {
                    enemies = count;
                    bestPoint = point;
                } else if (count == enemies && mousePos.Distance2D(point) < mousePos.Distance2D(bestPoint)) {
                    enemies = count;
                    bestPoint = point;
                }
            }
        }

        if (bestPoint.IsZero()) {
            return Vec3();
        }

        const bool isGoodPos = IsGoodPosition(bestPoint, dashSpell, cfg);
        if (asap && isGoodPos) {
            return bestPoint;
        }
        if (isGoodPos && InAARange(bestPoint, cfg)) {
            return bestPoint;
        }
        return Vec3();
    }

} // namespace DashUtils
} // namespace SDK

