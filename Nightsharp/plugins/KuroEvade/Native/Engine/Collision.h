#pragma once

// Collision policy for the replacement engine. Unit blockers are predicted
// and ordered along the projectile path, allowing database-defined multi-hit
// projectiles. Terrain/projectile walls remain terminal and end explosions are
// retained by SourceSkillshot at the exact collision point.

#include "Skillshot.h"

#include "../../../../Core/CoreNavGrid.h"
#include "../../../../SDK/GameObjects/YasuoWallTracker.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <initializer_list>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Plugins::KuroEvade {

struct SourceCollisionEvent {
    float Distance = FLT_MAX;
    Vec2 Point;
    SourceCollisionKind Kind = SourceCollisionKind::None;
    int NetworkId = 0;
    Vec2 UnitCenter;
    bool UnitIsChampion = false;
};

struct SourceCollisionSecondaryProfile {
    float BounceDistance = 0.0f;
    float ExplosionRadius = 0.0f;
    int ExplosionDelay = 0;
};

struct SourceCollisionResolution {
    bool Stopped = false;
    SourceCollisionEvent Stop;
    int UnitHits = 0;
};

class SourceCollision final {
public:
    static bool IsTerminalBeforeContinuation(SourceCollisionKind kind) {
        return kind == SourceCollisionKind::ProjectileWall ||
               kind == SourceCollisionKind::Terrain;
    }

    // Returns the first point where a finite segment enters a circular
    // projectile barrier. Kept deterministic/public for benchmark coverage.
    static bool FirstCircleContact(const Vec2& from,
                                   const Vec2& end,
                                   const Vec2& center,
                                   float radius,
                                   Vec2& point,
                                   float& distance) {
        point = {};
        distance = FLT_MAX;
        const Vec2 direction = (end - from).Normalized();
        const float length = from.Distance(end);
        if (from.IsZero() || end.IsZero() || center.IsZero() ||
            direction.IsZero() || length <= 0.0f || radius <= 0.0f) {
            return false;
        }

        const auto projection = SourceGeometry::ProjectOn(center, from, end);
        const float lateral = projection.SegmentPoint.Distance(center);
        if (lateral > radius) {
            return false;
        }
        const float centreDistance = std::clamp(
            (projection.SegmentPoint - from).Dot(direction), 0.0f, length);
        const float entryOffset = std::sqrt(std::max(
            0.0f, radius * radius - lateral * lateral));
        distance = std::clamp(centreDistance - entryOffset, 0.0f, length);
        point = from + direction * distance;
        return !point.IsZero() && point.IsValid();
    }

    static bool IsNearActiveCircularBarrier(
            const SDK::AIBaseClient& caster,
            const Vec2& point,
            float projectileRadius,
            bool includeSamira,
            bool includeMel) {
        if (point.IsZero() || !point.IsValid()) {
            return false;
        }
        for (const auto& hero : SDK::ObjectManager::Get<SDK::AIHeroClient>()) {
            const SDK::AIBaseClient barrier(hero.Handle());
            if (!IsActiveOpposingBarrier(caster, barrier)) {
                continue;
            }
            const std::string champion = barrier.CharacterName();
            float radius = 0.0f;
            bool active = false;
            if (includeSamira && _stricmp(champion.c_str(), "Samira") == 0) {
                radius = 325.0f;
                active = HasAnyBuff(barrier, { "SamiraW", "SamiraWBuff" });
            } else if (includeMel &&
                       _stricmp(champion.c_str(), "Mel") == 0) {
                radius = 175.0f;
                active = HasAnyBuff(
                    barrier,
                    { "MelW", "MelWBuff", "MelWReflect", "MelRebuttal" });
            }
            if (!active) {
                continue;
            }
            const Vec2 center = barrier.Position().To2D();
            if (!center.IsZero() &&
                center.Distance(point) <= radius +
                    std::max(0.0f, projectileRadius)) {
                return true;
            }
        }
        return false;
    }

    static SourceCollisionSecondaryProfile ResolveSecondaryProfile(
            const Database::SpellData& data,
            bool unitIsChampion) {
        SourceCollisionSecondaryProfile profile;
        profile.BounceDistance = unitIsChampion ||
                data.CollisionBounceDistanceNonChampion <= 0.0f
            ? std::max(0.0f, data.CollisionBounceDistance)
            : data.CollisionBounceDistanceNonChampion;
        profile.ExplosionRadius = !unitIsChampion &&
                data.EndExplosionRadiusNonChampion > 0.0f
            ? data.EndExplosionRadiusNonChampion
            : std::max(0.0f, data.SecondaryRadius);
        profile.ExplosionDelay = !unitIsChampion &&
                data.EndExplosionDelayNonChampion >= 0
            ? data.EndExplosionDelayNonChampion
            : std::max(0, data.EndExplosionDelay);
        return profile;
    }

    static void ApplyUnitImpactProfile(
            SourceSkillshot& skillshot,
            const SourceCollisionEvent& event) {
        skillshot.CollisionUnitNetworkId = event.NetworkId;
        skillshot.CollisionUnitCenter = event.UnitCenter;
        skillshot.CollisionExplosionCenter = {};
        skillshot.CollisionEndExplosionRadius = 0.0f;
        skillshot.CollisionEndExplosionDelay = -1;
        if (!skillshot.Native || !skillshot.Data.HasEndExplosion) {
            return;
        }

        const SourceCollisionSecondaryProfile profile =
            ResolveSecondaryProfile(skillshot.Data, event.UnitIsChampion);
        skillshot.CollisionEndExplosionRadius = event.UnitIsChampion &&
                (skillshot.Data.EndExplosionRadiusMedium > 0.0f ||
                 skillshot.Data.EndExplosionRadiusFar > 0.0f)
            ? skillshot.EndExplosionBaseRadius()
            : profile.ExplosionRadius;
        skillshot.CollisionEndExplosionDelay = profile.ExplosionDelay;
        if (profile.BounceDistance <= 0.0f) {
            return;
        }
        const Vec2 direction = !skillshot.Native->Direction.IsZero()
            ? skillshot.Native->Direction.Normalized()
            : (skillshot.OriginalEnd -
                skillshot.Native->StartPosition).Normalized();
        const Vec2 origin = !event.UnitCenter.IsZero()
            ? event.UnitCenter
            : event.Point;
        if (!direction.IsZero() && !origin.IsZero()) {
            skillshot.CollisionExplosionCenter = origin +
                direction * profile.BounceDistance;
        }
    }

    // Public and deterministic so Benchmarking can regression-test the same
    // policy used in game without constructing SDK game objects.
    static SourceCollisionResolution Resolve(
            std::vector<SourceCollisionEvent> events,
            int targetLimit) {
        SourceCollisionResolution result;
        targetLimit = std::max(1, targetLimit);
        std::sort(events.begin(), events.end(),
            [](const SourceCollisionEvent& lhs,
               const SourceCollisionEvent& rhs) {
                if (lhs.Distance < rhs.Distance) {
                    return true;
                }
                if (lhs.Distance > rhs.Distance) {
                    return false;
                }
                // A terminal surface wins a bit-identical contact tie. Keep
                // the comparator strictly ordered; epsilon comparisons here
                // would be non-transitive and make std::sort undefined.
                const bool lhsTerminal =
                    lhs.Kind != SourceCollisionKind::Unit;
                const bool rhsTerminal =
                    rhs.Kind != SourceCollisionKind::Unit;
                if (lhsTerminal != rhsTerminal) {
                    return lhsTerminal;
                }
                return lhs.NetworkId < rhs.NetworkId;
            });

        for (const SourceCollisionEvent& event : events) {
            if (!std::isfinite(event.Distance) || event.Distance < 0.0f ||
                event.Point.IsZero()) {
                continue;
            }
            if (event.Kind != SourceCollisionKind::Unit) {
                result.Stopped = true;
                result.Stop = event;
                return result;
            }
            ++result.UnitHits;
            if (result.UnitHits >= targetLimit) {
                result.Stopped = true;
                result.Stop = event;
                return result;
            }
        }
        return result;
    }

    static bool Update(SourceSkillshot& skillshot,
                        bool minionCollision,
                        bool heroCollision,
                        bool projectileWallCollision) {
        if (!skillshot.Native || !skillshot.IsLine() ||
            skillshot.Data.Runtime.CollisionObjects.empty()) {
            return false;
        }
        if (skillshot.ProjectileTerminated) {
            return skillshot.CollisionStopped;
        }

        const Vec2 from = skillshot.IsFiniteMissile()
            ? skillshot.MissilePosition(0)
            : skillshot.Native->StartPosition;
        const Vec2 pathStart = skillshot.Native->StartPosition;
        const Vec2 authoredEnd = skillshot.OriginalEnd.IsZero()
            ? skillshot.Native->EndPosition
            : skillshot.OriginalEnd;
        const bool continuationMode =
            skillshot.Data.CollisionInitialRange > 0.0f &&
            (skillshot.Data.CollisionContinuationDistance > 0.0f ||
             skillshot.Data.CollisionContinuationRange >
                 skillshot.Data.CollisionInitialRange);
        // Collision probing always uses the authored projectile width. A
        // transformed continuation (Lissandra Q) may widen its danger shape,
        // but that width must not retroactively enlarge the initial hit test.
        const float collisionProbeRadius = std::max(0.0f,
            continuationMode
                ? static_cast<float>(skillshot.Data.Runtime.Radius)
                : skillshot.RawRadius());
        const Vec2 authoredDirection = (authoredEnd - pathStart).Normalized();
        const float continuationScanRange = continuationMode
            ? std::max(
                skillshot.Data.CollisionInitialRange +
                    std::max(0.0f,
                        skillshot.Data.CollisionContinuationDistance),
                skillshot.Data.CollisionContinuationRange)
            : pathStart.Distance(authoredEnd);
        const Vec2 originalEnd = continuationMode &&
            !authoredDirection.IsZero()
            ? pathStart + authoredDirection * continuationScanRange
            : authoredEnd;
        const Vec2 routeDirection = (originalEnd - from).Normalized();
        const Vec2 fullDirection = (originalEnd - pathStart).Normalized();
        if (from.IsZero() || originalEnd.IsZero() || routeDirection.IsZero() ||
            fullDirection.IsZero()) {
            return false;
        }

        const float currentProgress = std::clamp(
            (SourceGeometry::ProjectOn(from, pathStart, originalEnd).SegmentPoint -
                pathStart).Dot(fullDirection),
            0.0f, pathStart.Distance(originalEnd));
        const int targetLimit = std::max(1,
            skillshot.Data.CollisionTargetLimit);
        if ((targetLimit > 1 || continuationMode) &&
            !skillshot.PendingUnitCollisions.empty()) {
            std::sort(skillshot.PendingUnitCollisions.begin(),
                skillshot.PendingUnitCollisions.end(),
                [](const auto& lhs, const auto& rhs) {
                    return lhs.second < rhs.second;
                });
            for (const auto& contact : skillshot.PendingUnitCollisions) {
                if (contact.second > currentProgress + 6.0f) {
                    break;
                }
                if (std::find(skillshot.ConsumedCollisionUnits.begin(),
                              skillshot.ConsumedCollisionUnits.end(),
                              contact.first) ==
                    skillshot.ConsumedCollisionUnits.end()) {
                    skillshot.ConsumedCollisionUnits.push_back(contact.first);
                    skillshot.LastConsumedCollisionPoint = pathStart +
                        fullDirection * contact.second;
                }
            }
        }

        std::vector<SourceCollisionEvent> events;
        std::vector<std::pair<int, float>> pendingUnitCollisions;
        std::unordered_set<int> visited;

        const auto has = [&](SDK::CollisionableObjects type) {
            return std::find(skillshot.Data.Runtime.CollisionObjects.begin(),
                             skillshot.Data.Runtime.CollisionObjects.end(), type) !=
                   skillshot.Data.Runtime.CollisionObjects.end();
        };
        const auto hasAuthored = [&](Database::CollisionObjectType type) {
            return std::find(skillshot.Data.CollisionObjects.begin(),
                             skillshot.Data.CollisionObjects.end(), type) !=
                   skillshot.Data.CollisionObjects.end();
        };

        const auto addUnit = [&](const SDK::AIBaseClient& unit) {
            if (!unit.IsValid() || unit.IsDead() || unit.NetworkId() == 0 ||
                unit.NetworkId() == skillshot.Native->Caster.NetworkId() ||
                !visited.insert(unit.NetworkId()).second) {
                return;
            }
            if (std::find(skillshot.ConsumedCollisionUnits.begin(),
                          skillshot.ConsumedCollisionUnits.end(),
                          unit.NetworkId()) !=
                skillshot.ConsumedCollisionUnits.end()) {
                return;
            }

            Vec2 position = unit.Position().To2D();
            const auto waypoints3 = unit.GetWaypoints();
            std::vector<Vec2> waypoints;
            if (waypoints3.size() >= 2 && unit.MoveSpeed() > 0.0f) {
                waypoints.reserve(waypoints3.size());
                for (const Vec3& waypoint : waypoints3) {
                    waypoints.push_back(waypoint.To2D());
                }
                const float missileSpeed = std::max(1.0f,
                    static_cast<float>(std::max(
                        1, skillshot.SpellData().MissileSpeed)));
                // Two fixed-point iterations account for the fact that the
                // unit's future position also changes projectile travel time.
                for (int iteration = 0; iteration < 2; ++iteration) {
                    const auto projected = SourceGeometry::ProjectOn(
                        position, from, originalEnd);
                    const float travelSeconds = from.Distance(
                        projected.SegmentPoint) / missileSpeed;
                    position = SourceGeometry::PositionAfter(
                        waypoints, unit.MoveSpeed() * travelSeconds);
                }
            }

            const auto projection = SourceGeometry::ProjectOn(
                position, from, originalEnd);
            if (!projection.IsOnSegment) {
                return;
            }
            const float blockerRadius = std::max(
                0.0f, unit.BoundingRadius() - 10.0f);
            const float combinedRadius = collisionProbeRadius + blockerRadius;
            const float lateral = projection.SegmentPoint.Distance(position);
            if (lateral > combinedRadius) {
                return;
            }

            const float centreDistance = from.Distance(projection.SegmentPoint);
            const float entryOffset = std::sqrt(std::max(
                0.0f, combinedRadius * combinedRadius - lateral * lateral));
            const float contactDistance = std::clamp(
                centreDistance - entryOffset, 0.0f, from.Distance(originalEnd));
            const Vec2 contactPoint = from + routeDirection * contactDistance;
            events.push_back({ contactDistance, contactPoint,
                SourceCollisionKind::Unit, unit.NetworkId(), position,
                unit.IsHero() });
            pendingUnitCollisions.emplace_back(unit.NetworkId(), std::clamp(
                (contactPoint - pathStart).Dot(fullDirection),
                0.0f, pathStart.Distance(originalEnd)));
        };

        if (minionCollision && has(SDK::CollisionableObjects::Minions)) {
            const bool laneMinions = hasAuthored(
                Database::CollisionObjectType::EnemyMinions);
            const bool largeMonstersOnly = hasAuthored(
                    Database::CollisionObjectType::EnemyLargeMonsters) &&
                !laneMinions;
            if (laneMinions && !skillshot.Data.CollisionExceptMini) {
                if (skillshot.Native->Caster.IsAlly()) {
                    for (const auto& minion : SDK::GameObjects::EnemyMinions()) {
                        addUnit(SDK::AIBaseClient(minion.Handle()));
                    }
                } else {
                    for (const auto& minion : SDK::GameObjects::AllyMinions()) {
                        addUnit(SDK::AIBaseClient(minion.Handle()));
                    }
                }
            }
            for (const auto& minion : SDK::GameObjects::Jungle()) {
                if (largeMonstersOnly) {
                    const SDK::JungleType type = minion.GetJungleType();
                    if (type != SDK::JungleType::Large &&
                        type != SDK::JungleType::Epic &&
                        type != SDK::JungleType::Legendary) {
                        continue;
                    }
                }
                addUnit(SDK::AIBaseClient(minion.Handle()));
            }
        }

        if (heroCollision && has(SDK::CollisionableObjects::Heroes)) {
            if (skillshot.Native->Caster.IsAlly()) {
                for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
                    addUnit(SDK::AIBaseClient(hero.Handle()));
                }
            } else {
                const int playerId = SDK::ObjectManager::Player().NetworkId();
                for (const auto& hero : SDK::GameObjects::AllyHeroes()) {
                    const SDK::AIBaseClient unit(hero.Handle());
                    // The local player is the prospective victim, not a
                    // blocker. Other allies can consume collision hits.
                    if (unit.NetworkId() != playerId) {
                        addUnit(unit);
                    }
                }
            }
        }

        if (has(SDK::CollisionableObjects::Walls)) {
            float height = 0.0f;
            if (skillshot.Native->Caster.IsValid()) {
                height = skillshot.Native->Caster.Position().y;
            } else if (SDK::ObjectManager::Player().IsValid()) {
                height = SDK::ObjectManager::Player().Position().y;
            }
            const bool cacheMatches = skillshot.TerrainCollisionCached &&
                skillshot.TerrainCollisionPathStart.DistanceSqr(pathStart) <=
                    1.0f &&
                skillshot.TerrainCollisionPathEnd.DistanceSqr(originalEnd) <=
                    1.0f &&
                std::abs(skillshot.TerrainCollisionProbeRadius -
                    collisionProbeRadius) <= 0.01f;
            if (!cacheMatches) {
                skillshot.TerrainCollisionCached = true;
                skillshot.TerrainCollisionPathStart = pathStart;
                skillshot.TerrainCollisionPathEnd = originalEnd;
                skillshot.TerrainCollisionProbeRadius =
                    collisionProbeRadius;
                skillshot.TerrainCollisionPoint = {};
                SourceGeometry::FirstTerrainCollision(
                    pathStart, originalEnd, height, collisionProbeRadius,
                    skillshot.TerrainCollisionPoint);
            }
            const Vec2 terrainPoint = skillshot.TerrainCollisionPoint;
            const auto terrainProjection = SourceGeometry::ProjectOn(
                terrainPoint, from, originalEnd);
            if (!terrainPoint.IsZero() && terrainProjection.IsOnSegment &&
                (terrainPoint - from).Dot(routeDirection) >= -1.0f) {
                events.push_back({ from.Distance(terrainPoint), terrainPoint,
                    SourceCollisionKind::Terrain, 0 });
            }
        }

        if (projectileWallCollision &&
            has(SDK::CollisionableObjects::YasuoWall)) {
            for (const auto& wall : SDK::YasuoWallTracker::ActiveWalls()) {
                const SDK::GameObject wallObject(wall.main);
                const SDK::GameObjectTeam wallTeam = wallObject.IsValid()
                    ? wallObject.Team()
                    : SDK::GameObjectTeam::Unknown;
                const SDK::GameObjectTeam casterTeam =
                    skillshot.Native->Caster.IsValid()
                    ? skillshot.Native->Caster.Team()
                    : SDK::GameObjectTeam::Unknown;
                // A wind wall only destroys projectiles from the opposing
                // team. The shared SDK tracker intentionally tracks both
                // teams, so ownership must be filtered here.
                if (wallTeam != SDK::GameObjectTeam::Unknown &&
                    casterTeam != SDK::GameObjectTeam::Unknown &&
                    wallTeam == casterTeam) {
                    continue;
                }
                const Vec2 wallStart = wall.start.To2D();
                const Vec2 wallEnd = wall.end.To2D();
                if (SourceGeometry::SegmentDistance(
                        from, originalEnd, wallStart, wallEnd) >
                    collisionProbeRadius + 75.0f) {
                    continue;
                }
                const auto intersection = SourceGeometry::SegmentIntersection(
                    from, originalEnd, wallStart, wallEnd);
                const Vec2 point = intersection.Intersects
                    ? intersection.Point
                    : SourceGeometry::ProjectOn(
                        SourceGeometry::ProjectOn(
                            from, wallStart, wallEnd).SegmentPoint,
                        from, originalEnd).SegmentPoint;
                events.push_back({ from.Distance(point), point,
                    SourceCollisionKind::ProjectileWall, 0 });
            }
        }
        if (projectileWallCollision &&
            has(SDK::CollisionableObjects::SamiraWall)) {
            AppendCircularBarrierEvents(
                skillshot.Native->Caster, from, originalEnd,
                collisionProbeRadius, "Samira",
                { "SamiraW", "SamiraWBuff" }, 325.0f, events);
        }
        if (projectileWallCollision &&
            has(SDK::CollisionableObjects::MelWall)) {
            AppendCircularBarrierEvents(
                skillshot.Native->Caster, from, originalEnd,
                collisionProbeRadius, "Mel",
                { "MelW", "MelWBuff", "MelWReflect", "MelRebuttal" },
                175.0f, events);
        }

        const int consumedHits = continuationMode
            ? static_cast<int>(skillshot.ConsumedCollisionUnits.size())
            : std::min(targetLimit,
                static_cast<int>(skillshot.ConsumedCollisionUnits.size()));
        SourceCollisionResolution resolution;
        Vec2 naturalEndOverride;
        bool continuationActive = false;
        if (continuationMode) {
            std::sort(events.begin(), events.end(),
                [&](const SourceCollisionEvent& lhs,
                    const SourceCollisionEvent& rhs) {
                    return (lhs.Point - pathStart).Dot(fullDirection) <
                           (rhs.Point - pathStart).Dot(fullDirection);
                });
            const float initialEnd = std::min(continuationScanRange,
                std::max(0.0f, skillshot.Data.CollisionInitialRange));
            float firstHitDistance = -1.0f;
            int firstHitNetworkId = 0;

            if (consumedHits > 0 &&
                !skillshot.LastConsumedCollisionPoint.IsZero()) {
                firstHitDistance = std::clamp(
                    (skillshot.LastConsumedCollisionPoint - pathStart).Dot(
                        fullDirection), 0.0f, continuationScanRange);
            } else {
                for (const SourceCollisionEvent& event : events) {
                    const float distance = std::clamp(
                        (event.Point - pathStart).Dot(fullDirection),
                        0.0f, continuationScanRange);
                    if (distance > initialEnd) {
                        break;
                    }
                    if (IsTerminalBeforeContinuation(event.Kind)) {
                        resolution.Stopped = true;
                        resolution.Stop = event;
                        break;
                    }
                    if (event.Kind == SourceCollisionKind::Unit) {
                        firstHitDistance = distance;
                        firstHitNetworkId = event.NetworkId;
                        break;
                    }
                }
            }

            if (!resolution.Stopped && firstHitDistance < 0.0f) {
                naturalEndOverride = pathStart + fullDirection * initialEnd;
            } else if (!resolution.Stopped) {
                continuationActive = true;
                resolution.UnitHits = consumedHits > 0 ? consumedHits : 1;
                const float continuationEnd = std::min(
                    continuationScanRange,
                    skillshot.Data.CollisionContinuationRange >
                            skillshot.Data.CollisionInitialRange
                        ? skillshot.Data.CollisionContinuationRange
                        : firstHitDistance + std::max(0.0f,
                            skillshot.Data.CollisionContinuationDistance));
                naturalEndOverride = pathStart +
                    fullDirection * continuationEnd;

                for (const SourceCollisionEvent& event : events) {
                    const float distance = std::clamp(
                        (event.Point - pathStart).Dot(fullDirection),
                        0.0f, continuationScanRange);
                    if (distance <= firstHitDistance + 1.0f ||
                        (firstHitNetworkId != 0 &&
                         event.NetworkId == firstHitNetworkId)) {
                        continue;
                    }
                    if (distance > continuationEnd) {
                        break;
                    }
                    const bool terminalWall =
                        event.Kind == SourceCollisionKind::ProjectileWall;
                    const bool secondTarget =
                        event.Kind == SourceCollisionKind::Unit &&
                        skillshot.Data.CollisionContinuationStopsOnSecondTarget;
                    const bool terrain =
                        event.Kind == SourceCollisionKind::Terrain &&
                        skillshot.Data.CollisionContinuationStopsOnTerrain;
                    if (!terminalWall && !secondTarget && !terrain) {
                        continue;
                    }
                    resolution.Stopped = true;
                    resolution.Stop = event;
                    naturalEndOverride = {};
                    if (secondTarget) {
                        ++resolution.UnitHits;
                    }
                    break;
                }
            }
        } else if (consumedHits >= targetLimit &&
            !skillshot.LastConsumedCollisionPoint.IsZero()) {
            resolution.Stopped = true;
            resolution.Stop = { pathStart.Distance(
                skillshot.LastConsumedCollisionPoint),
                skillshot.LastConsumedCollisionPoint,
                SourceCollisionKind::Unit, 0 };
        } else {
            resolution = Resolve(std::move(events),
                std::max(1, targetLimit - consumedHits));
        }
        if (!continuationMode) {
            resolution.UnitHits += consumedHits;
        }
        skillshot.PendingUnitCollisions = std::move(pendingUnitCollisions);
        const Vec2 nextEnd = resolution.Stopped
            ? resolution.Stop.Point
            : (!naturalEndOverride.IsZero()
                ? naturalEndOverride : originalEnd);
        const SourceCollisionKind nextKind = resolution.Stopped
            ? resolution.Stop.Kind
            : SourceCollisionKind::None;
        const Vec2 nextUnitCenter = resolution.Stopped &&
            resolution.Stop.Kind == SourceCollisionKind::Unit
            ? resolution.Stop.UnitCenter
            : Vec2();
        const int nextRadius = static_cast<int>(std::lround(
            continuationActive &&
                    skillshot.Data.CollisionContinuationRadius > 0.0f
                ? skillshot.Data.CollisionContinuationRadius
                : static_cast<float>(skillshot.Data.Runtime.Radius)));
        const Vec2 previousExplosionCenter =
            skillshot.CollisionExplosionCenter;
        const float previousExplosionRadius =
            skillshot.CollisionEndExplosionRadius;
        const int previousExplosionDelay =
            skillshot.CollisionEndExplosionDelay;
        const int previousUnitNetworkId =
            skillshot.CollisionUnitNetworkId;
        const bool geometryChanged =
            skillshot.CollisionEnd.DistanceSqr(nextEnd) > 1.0f ||
            skillshot.CollisionUnitCenter.DistanceSqr(nextUnitCenter) > 1.0f ||
            skillshot.Native->EndPosition.DistanceSqr(nextEnd) > 1.0f ||
            skillshot.CollisionKind != nextKind ||
            skillshot.CollisionHitCount != resolution.UnitHits ||
            skillshot.CollisionStopped != resolution.Stopped ||
            skillshot.Native->SData.Radius != nextRadius;

        skillshot.CollisionEnd = nextEnd;
        skillshot.CollisionUnitCenter = nextUnitCenter;
        skillshot.CollisionKind = nextKind;
        skillshot.CollisionHitCount = resolution.UnitHits;
        skillshot.CollisionStopped = resolution.Stopped;
        if (resolution.Stopped &&
            resolution.Stop.Kind == SourceCollisionKind::Unit) {
            ApplyUnitImpactProfile(skillshot, resolution.Stop);
        } else {
            skillshot.CollisionUnitNetworkId = 0;
            skillshot.CollisionExplosionCenter = {};
            skillshot.CollisionEndExplosionRadius = 0.0f;
            skillshot.CollisionEndExplosionDelay = -1;
        }
        skillshot.Native->SData.Radius = std::max(0, nextRadius);
        const bool changed = geometryChanged ||
            previousExplosionCenter.DistanceSqr(
                skillshot.CollisionExplosionCenter) > 1.0f ||
            std::abs(previousExplosionRadius -
                skillshot.CollisionEndExplosionRadius) > 0.01f ||
            previousExplosionDelay !=
                skillshot.CollisionEndExplosionDelay ||
            previousUnitNetworkId != skillshot.CollisionUnitNetworkId;
        if (!changed) {
            return resolution.Stopped;
        }

        skillshot.Native->EndPosition = nextEnd;
        const Vec2 direction = (nextEnd - skillshot.Native->StartPosition).Normalized();
        if (!direction.IsZero()) {
            skillshot.Native->Direction = direction;
        }
        SpecialSpells::RefreshSkillshotGeometry(*skillshot.Native);
        return resolution.Stopped;
    }

    static bool HasAnyBuff(const SDK::AIBaseClient& unit,
                           std::initializer_list<const char*> names) {
        if (!unit.IsValid()) {
            return false;
        }
        for (const char* name : names) {
            if (name && unit.HasBuff(name)) {
                return true;
            }
        }
        return false;
    }

    static bool IsActiveOpposingBarrier(const SDK::AIBaseClient& caster,
                                        const SDK::AIBaseClient& barrier) {
        if (!barrier.IsValid() || barrier.IsDead() ||
            barrier.NetworkId() == 0 ||
            (caster.IsValid() &&
             barrier.NetworkId() == caster.NetworkId())) {
            return false;
        }
        if (!caster.IsValid()) {
            return true;
        }
        const SDK::GameObjectTeam casterTeam = caster.Team();
        const SDK::GameObjectTeam barrierTeam = barrier.Team();
        if (casterTeam != SDK::GameObjectTeam::Unknown &&
            barrierTeam != SDK::GameObjectTeam::Unknown) {
            return casterTeam != barrierTeam;
        }
        return caster.IsAlly() != barrier.IsAlly();
    }

    static void AppendCircularBarrierEvents(
            const SDK::AIBaseClient& caster,
            const Vec2& from,
            const Vec2& end,
            float projectileRadius,
            const char* championName,
            std::initializer_list<const char*> buffNames,
            float barrierRadius,
            std::vector<SourceCollisionEvent>& events) {
        for (const auto& hero : SDK::ObjectManager::Get<SDK::AIHeroClient>()) {
            const SDK::AIBaseClient barrier(hero.Handle());
            if (!IsActiveOpposingBarrier(caster, barrier) ||
                _stricmp(barrier.CharacterName().c_str(), championName) != 0 ||
                !HasAnyBuff(barrier, buffNames)) {
                continue;
            }
            const Vec2 center = barrier.Position().To2D();
            Vec2 contact;
            float distance = FLT_MAX;
            if (FirstCircleContact(
                    from, end, center,
                    barrierRadius + std::max(0.0f, projectileRadius),
                    contact, distance)) {
                events.push_back({ distance, contact,
                    SourceCollisionKind::ProjectileWall, 0 });
            }
        }
    }
};

} // namespace Plugins::KuroEvade
