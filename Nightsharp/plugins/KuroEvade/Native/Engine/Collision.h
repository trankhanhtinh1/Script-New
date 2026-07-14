#pragma once

// Collision.cs adapter for the replacement engine.  It predicts the first
// blocker on the projectile route and shortens CollisionEnd without coupling
// collision decisions to the old KuroEvade planner.

#include "Skillshot.h"

#include "../../../../Core/CoreNavGrid.h"
#include "../../../../SDK/GameObjects/YasuoWallTracker.h"

#include <algorithm>
#include <cfloat>
#include <unordered_set>

namespace Plugins::KuroEvade {

class SourceCollision final {
public:
    static bool Update(SourceSkillshot& skillshot,
                       bool minionCollision,
                       bool heroCollision,
                       bool yasuoCollision) {
        if (!skillshot.Native || !skillshot.IsLine() ||
            skillshot.Data.Runtime.CollisionObjects.empty()) {
            return false;
        }

        const Vec2 from = skillshot.IsFiniteMissile()
            ? skillshot.MissilePosition(0)
            : skillshot.Native->StartPosition;
        const Vec2 originalEnd = skillshot.OriginalEnd.IsZero()
            ? skillshot.Native->EndPosition
            : skillshot.OriginalEnd;
        if (from.IsZero() || originalEnd.IsZero()) {
            return false;
        }

        float bestDistance = FLT_MAX;
        Vec2 bestPoint;
        std::unordered_set<int> visited;

        const auto has = [&](SDK::CollisionableObjects type) {
            return std::find(skillshot.Data.Runtime.CollisionObjects.begin(),
                             skillshot.Data.Runtime.CollisionObjects.end(), type) !=
                   skillshot.Data.Runtime.CollisionObjects.end();
        };

        const auto considerPoint = [&](const Vec2& point, float radius) {
            const auto projection = SourceGeometry::ProjectOn(point, from, originalEnd);
            if (!projection.IsOnSegment ||
                projection.SegmentPoint.Distance(point) > skillshot.RawRadius() + radius) {
                return;
            }
            const float distance = from.Distance(projection.SegmentPoint);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestPoint = projection.SegmentPoint + skillshot.Direction() * 30.0f;
            }
        };

        const auto predictUnit = [&](const SDK::AIBaseClient& unit) {
            if (!unit.IsValid() || unit.IsDead() || unit.NetworkId() == 0 ||
                !visited.insert(unit.NetworkId()).second) {
                return;
            }
            Vec2 position = unit.ServerPosition().To2D();
            const float speed = std::max(1.0f,
                static_cast<float>(std::max(1, skillshot.SpellData().MissileSpeed)));
            const float travelSeconds = from.Distance(position) / speed;
            const auto waypoints3 = unit.GetWaypoints();
            if (waypoints3.size() >= 2 && unit.MoveSpeed() > 0.0f) {
                std::vector<Vec2> waypoints;
                waypoints.reserve(waypoints3.size());
                for (const Vec3& waypoint : waypoints3) {
                    waypoints.push_back(waypoint.To2D());
                }
                position = SourceGeometry::PositionAfter(
                    waypoints, unit.MoveSpeed() * travelSeconds);
            }
            considerPoint(position, std::max(0.0f, unit.BoundingRadius() - 10.0f));
        };

        if (minionCollision && has(SDK::CollisionableObjects::Minions)) {
            if (!skillshot.Data.CollisionExceptMini) {
                if (skillshot.Native->Caster.IsAlly()) {
                    for (const auto& minion : SDK::GameObjects::EnemyMinions()) {
                        predictUnit(SDK::AIBaseClient(minion.Handle()));
                    }
                } else {
                    for (const auto& minion : SDK::GameObjects::AllyMinions()) {
                        predictUnit(SDK::AIBaseClient(minion.Handle()));
                    }
                }
            }
            for (const auto& minion : SDK::GameObjects::Jungle()) {
                predictUnit(SDK::AIBaseClient(minion.Handle()));
            }
        }

        if (heroCollision && has(SDK::CollisionableObjects::Heroes)) {
            if (skillshot.Native->Caster.IsAlly()) {
                for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
                    predictUnit(SDK::AIBaseClient(hero.Handle()));
                }
            } else {
                for (const auto& hero : SDK::GameObjects::AllyHeroes()) {
                    const SDK::AIBaseClient unit(hero.Handle());
                    if (unit.NetworkId() !=
                        SDK::ObjectManager::Player().NetworkId()) {
                        predictUnit(unit);
                    }
                }
            }
        }

        if (yasuoCollision && has(SDK::CollisionableObjects::YasuoWall)) {
            for (const auto& wall : SDK::YasuoWallTracker::ActiveWalls()) {
                const Vec2 wallStart = wall.start.To2D();
                const Vec2 wallEnd = wall.end.To2D();
                if (SourceGeometry::SegmentDistance(
                        from, originalEnd, wallStart, wallEnd) >
                    skillshot.RawRadius() + 75.0f) {
                    continue;
                }
                const auto intersection = SourceGeometry::SegmentIntersection(
                    from, originalEnd, wallStart, wallEnd);
                const Vec2 point = intersection.Intersects
                    ? intersection.Point
                    : SourceGeometry::ProjectOn(
                        SourceGeometry::ProjectOn(from, wallStart, wallEnd).SegmentPoint,
                        from, originalEnd).SegmentPoint;
                const float distance = from.Distance(point);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestPoint = point;
                }
            }
        }

        const Vec2 nextEnd = bestDistance == FLT_MAX ? originalEnd : bestPoint;
        if (skillshot.CollisionEnd.DistanceSqr(nextEnd) <= 1.0f &&
            skillshot.Native->EndPosition.DistanceSqr(nextEnd) <= 1.0f) {
            return bestDistance != FLT_MAX;
        }

        skillshot.CollisionEnd = nextEnd;
        skillshot.Native->EndPosition = nextEnd;
        skillshot.Native->Direction =
            (skillshot.Native->EndPosition - skillshot.Native->StartPosition).Normalized();
        SpecialSpells::RefreshSkillshotGeometry(*skillshot.Native);
        return bestDistance != FLT_MAX;
    }
};

} // namespace Plugins::KuroEvade
