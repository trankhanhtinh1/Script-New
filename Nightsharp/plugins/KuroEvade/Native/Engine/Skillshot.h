#pragma once

// Native model of Skillshot.cs from the supplied Evade project.  It wraps the
// NightSharp SDK shape (kept for special-spell geometry and ImGui drawing) and
// owns the timing/path-safety behaviour used by the replacement engine.

#include "../Config/EvadeConfig.h"
#include "Geometry.h"
#include "../Database/SpellData.h"
#include "../SpecialSpells/SpecialSpellCommon.h"

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace Plugins::KuroEvade {

enum class SourceDetectionType {
    ProcessSpell,
    MissileCreate,
    ObjectCreate,
    Simulated,
};

enum class SourceCollisionKind : std::uint8_t {
    None,
    Unit,
    ProjectileWall,
    Terrain,
};

struct SourcePathIntersection {
    Vec2 ComingFrom;
    Vec2 Point;
    float Distance = 0.0f;
    int Time = 0;
    bool Valid = false;
};

struct SourceSafePathResult {
    bool IsSafe = true;
    SourcePathIntersection Intersection;
};

class SourceSkillshot final {
public:
    using NativePtr = std::shared_ptr<SDK::Skillshot>;

    SourceSkillshot(NativePtr native,
                    const Database::SpellData& data,
                    SourceDetectionType detectionType,
                    int id)
        : Native(std::move(native)),
          Data(data),
          DetectionType(detectionType),
          Id(id) {
        if (Native) {
            OriginalEnd = Native->EndPosition;
            CollisionEnd = Native->EndPosition;
        }
    }

    NativePtr Native;
    Database::SpellData Data;
    SourceDetectionType DetectionType = SourceDetectionType::ProcessSpell;
    Vec2 OriginalEnd;
    Vec2 CollisionEnd;
    Vec2 CollisionUnitCenter;
    Vec2 CollisionExplosionCenter;
    SourceCollisionKind CollisionKind = SourceCollisionKind::None;
    int CollisionUnitNetworkId = 0;
    int CollisionHitCount = 0;
    bool CollisionStopped = false;
    float CollisionEndExplosionRadius = 0.0f;
    int CollisionEndExplosionDelay = -1;
    bool ProjectileTerminated = false;
    int ProjectileTerminationTick = 0;
    // Multi-hit missiles (Lux Q, Veigar Q) keep flying after an impact.  The
    // detector therefore carries already-consumed targets across collision
    // refreshes instead of treating every new missile position as hit zero.
    std::vector<std::pair<int, float>> PendingUnitCollisions;
    std::vector<int> ConsumedCollisionUnits;
    Vec2 LastConsumedCollisionPoint;
    // Terrain is static for a fixed route. Cache its first contact so global
    // projectiles do not rescan thousands of nav-grid samples every 50 ms.
    bool TerrainCollisionCached = false;
    Vec2 TerrainCollisionPathStart;
    Vec2 TerrainCollisionPathEnd;
    Vec2 TerrainCollisionPoint;
    float TerrainCollisionProbeRadius = -1.0f;
    int Id = 0;
    int TrapObjectId = 0;
    int MissileNetworkId = 0;
    bool ForceDisabled = false;
    bool Persistent = false;
    bool FromFog = false;
    float EvadeTime = -FLT_MAX;
    float SpellHitTime = -FLT_MAX;

    bool IsValid() const {
        return Native != nullptr;
    }

    int StartTick() const {
        return Native ? Native->StartTime : 0;
    }

    const SDK::SpellDatabaseEntry& SpellData() const {
        static const SDK::SpellDatabaseEntry empty;
        return Native ? Native->SData : empty;
    }

    Vec2 Start() const {
        return Native ? Native->StartPosition : Vec2();
    }

    Vec2 End() const {
        return Native ? Native->EndPosition : Vec2();
    }

    Vec2 Direction() const {
        return Native ? Native->Direction : Vec2();
    }

    bool HasMissile() const {
        return Native && Native->HasMissile();
    }

    bool IsLine() const {
        return Native && SDK::IsLineSpellType(Native->SData.SpellType);
    }

    bool IsCircle() const {
        return Native && SDK::IsCircleSpellType(Native->SData.SpellType);
    }

    bool IsCone() const {
        return Native &&
            (Native->SData.SpellType == SDK::SpellType::SkillshotCone ||
             Native->SData.SpellType == SDK::SpellType::SkillshotMissileCone);
    }

    bool IsRing() const {
        return Native && Native->SData.SpellType == SDK::SpellType::SkillshotRing;
    }

    bool IsArc() const {
        return Native &&
            (Native->SData.SpellType == SDK::SpellType::SkillshotArc ||
             Native->SData.SpellType == SDK::SpellType::SkillshotMissileArc);
    }

    bool IsFiniteMissile() const {
        return Native && Native->SData.MissileSpeed > 0 &&
            Native->SData.MissileSpeed != INT_MAX && HasMissile();
    }

    int ExtraDurationMs() const {
        return std::max(0, static_cast<int>(Data.ExtraEndTime));
    }

    float RawRadius() const {
        if (!Native) {
            return 0.0f;
        }
        return static_cast<float>(std::max(0, Native->SData.Radius));
    }

    float EffectiveRadius(const EvadeSettings& settings,
                          float unitRadius = 0.0f) const {
        return RawRadius() + std::max(0.0f, unitRadius) +
            static_cast<float>(std::max(0, settings.SkillShotsExtraRadius + settings.ExtraEvadeDistance));
    }

    // A projectile-intercepting wall truncates a line missile because the
    // travelled segment before the wall is still dangerous. Endpoint shapes
    // are different: intercepted lobbed/circle projectiles never create their
    // landing payload, so moving the destination circle onto the wall would
    // invent a false hazard there.
    bool ProjectileWallSuppressesEndpointHazard() const {
        return Native && !IsLine() && CollisionStopped &&
            CollisionKind == SourceCollisionKind::ProjectileWall &&
            !Data.EndExplosionOnProjectileWall;
    }

    bool HasEndExplosionArea() const {
        if (!Native || !Data.HasEndExplosion || Data.SecondaryRadius <= 0.0f ||
            (CollisionKind == SourceCollisionKind::ProjectileWall &&
             !Data.EndExplosionOnProjectileWall)) {
            return false;
        }
        const float explosionTravel = Data.EndExplosionAtUnitCenter &&
                !CollisionUnitCenter.IsZero()
            ? Native->StartPosition.Distance(CollisionUnitCenter)
            : TravelDistance();
        if (explosionTravel + 0.01f <
            std::max(0.0f, Data.EndExplosionMinimumTravelDistance)) {
            return false;
        }
        if (Data.EndExplosionRequiresUnitCollision) {
            return CollisionStopped &&
                CollisionKind == SourceCollisionKind::Unit;
        }
        if (Data.EndExplosionRequiresCollision) {
            return CollisionStopped &&
                CollisionKind != SourceCollisionKind::None;
        }
        return true;
    }

    Vec2 EndExplosionCenter() const {
        if (!Native) {
            return {};
        }
        Vec2 center;
        if (!CollisionExplosionCenter.IsZero()) {
            center = CollisionExplosionCenter;
        } else if (Data.EndExplosionAtUnitCenter &&
            !CollisionUnitCenter.IsZero()) {
            center = CollisionUnitCenter;
        } else {
            center = CollisionEnd.IsZero()
                ? Native->EndPosition
                : CollisionEnd;
        }
        if (Data.EndExplosionCenterOffset != 0.0f) {
            Vec2 direction = Native->Direction.Normalized();
            if (direction.IsZero()) {
                direction = (OriginalEnd - Native->StartPosition).Normalized();
            }
            center = center + direction * Data.EndExplosionCenterOffset;
        }
        return center;
    }

    float EndExplosionRadius(const EvadeSettings& settings,
                             float unitRadius = 0.0f) const {
        const float radius = EndExplosionBaseRadius();
        return radius + std::max(0.0f, unitRadius) +
            static_cast<float>(std::max(0, settings.SkillShotsExtraRadius + settings.ExtraEvadeDistance));
    }

    float EndExplosionBaseRadius() const {
        if (CollisionEndExplosionRadius > 0.0f) {
            return CollisionEndExplosionRadius;
        }
        const float travel = TravelDistance();
        if (Data.EndExplosionRadiusFar > 0.0f &&
            Data.EndExplosionFarTravelDistance > 0.0f &&
            travel > Data.EndExplosionFarTravelDistance) {
            return Data.EndExplosionRadiusFar;
        }
        if (Data.EndExplosionRadiusMedium > 0.0f &&
            Data.EndExplosionMediumTravelDistance > 0.0f &&
            travel >= Data.EndExplosionMediumTravelDistance) {
            return Data.EndExplosionRadiusMedium;
        }
        return std::max(0.0f, Data.SecondaryRadius);
    }

    int EndExplosionDelayMs() const {
        return CollisionEndExplosionDelay >= 0
            ? CollisionEndExplosionDelay
            : std::max(0, Data.EndExplosionDelay);
    }

    bool EndExplosionContains(const Vec2& point,
                              float unitRadius,
                              const EvadeSettings& settings) const {
        const float padding = std::max(0.0f, unitRadius) +
            static_cast<float>(std::max(0, settings.SkillShotsExtraRadius + settings.ExtraEvadeDistance));
        return EndExplosionSignedPenetration(point, padding) >= 0.0f;
    }

    float EndExplosionSignedPenetration(const Vec2& point,
                                        float padding = 0.0f) const {
        if (!HasEndExplosionArea()) {
            return -FLT_MAX;
        }
        padding = std::max(0.0f, padding);
        const Vec2 center = EndExplosionCenter();
        float penetration = EndExplosionBaseRadius() + padding -
            point.Distance(center);
        if (!Data.EndExplosionCross || !Native) {
            return penetration;
        }

        Vec2 direction = Native->Direction.Normalized();
        if (direction.IsZero()) {
            direction = (OriginalEnd - Native->StartPosition).Normalized();
        }
        if (direction.IsZero()) {
            return penetration;
        }
        const Vec2 side = SourceGeometry::Perpendicular(direction);
        const auto includeCapsule = [&](const Vec2& end, float radius) {
            if (radius <= 0.0f || center.DistanceSqr(end) <= 0.01f) {
                return;
            }
            penetration = std::max(penetration,
                radius + padding - SourceGeometry::PointSegmentDistance(
                    point, center, end));
        };
        includeCapsule(center + direction *
            std::max(0.0f, Data.EndExplosionForwardLength),
            Data.EndExplosionLongitudinalRadius);
        includeCapsule(center - direction *
            std::max(0.0f, Data.EndExplosionBackwardLength),
            Data.EndExplosionLongitudinalRadius);
        includeCapsule(center + side *
            std::max(0.0f, Data.EndExplosionSideLength),
            Data.EndExplosionSideRadius);
        includeCapsule(center - side *
            std::max(0.0f, Data.EndExplosionSideLength),
            Data.EndExplosionSideRadius);
        return penetration;
    }

    std::vector<std::vector<Vec2>> EndExplosionPolygons(
            float padding = 0.0f) const {
        std::vector<std::vector<Vec2>> result;
        if (!HasEndExplosionArea()) {
            return result;
        }
        padding = std::max(0.0f, padding);
        const Vec2 center = EndExplosionCenter();
        result.push_back(SourceGeometry::CirclePoints(
            center, EndExplosionBaseRadius() + padding, 32));
        if (!Data.EndExplosionCross || !Native) {
            return result;
        }

        Vec2 direction = Native->Direction.Normalized();
        if (direction.IsZero()) {
            direction = (OriginalEnd - Native->StartPosition).Normalized();
        }
        if (direction.IsZero()) {
            return result;
        }
        const Vec2 side = SourceGeometry::Perpendicular(direction);
        const auto addCapsule = [&](const Vec2& end, float radius) {
            if (radius > 0.0f && center.DistanceSqr(end) > 0.01f) {
                result.push_back(SourceGeometry::CapsulePoints(
                    center, end, radius + padding));
            }
        };
        addCapsule(center + direction *
            std::max(0.0f, Data.EndExplosionForwardLength),
            Data.EndExplosionLongitudinalRadius);
        addCapsule(center - direction *
            std::max(0.0f, Data.EndExplosionBackwardLength),
            Data.EndExplosionLongitudinalRadius);
        addCapsule(center + side *
            std::max(0.0f, Data.EndExplosionSideLength),
            Data.EndExplosionSideRadius);
        addCapsule(center - side *
            std::max(0.0f, Data.EndExplosionSideLength),
            Data.EndExplosionSideRadius);
        return result;
    }

    Vec2 EffectiveEnd(const EvadeSettings& settings) const {
        if (!Native) return {};
        const Vec2 end = CollisionEnd.IsZero() ? Native->EndPosition : CollisionEnd;
        const bool collisionShortened = !OriginalEnd.IsZero() &&
            !CollisionEnd.IsZero() && CollisionEnd.DistanceSqr(OriginalEnd) > 1.0f;
        if (!IsLine() || collisionShortened || settings.SkillShotsExtraRange <= 0) {
            return end;
        }
        Vec2 direction = Native->Direction;
        if (direction.IsZero()) direction = (end - Native->StartPosition).Normalized();
        return end + direction * static_cast<float>(settings.SkillShotsExtraRange);
    }

    float TravelDistance() const {
        return Native ? Native->StartPosition.Distance(CollisionEnd.IsZero()
            ? Native->EndPosition : CollisionEnd) : 0.0f;
    }

    int EndTick() const {
        if (!Native) {
            return 0;
        }
        if (Persistent) {
            return INT_MAX;
        }
        // Once the SDK reports missile deletion, ImpactTick uses that
        // authoritative moment. This prevents delayed attached explosions
        // from expiring according to an older predicted travel time.
        const int impactTick = ImpactTick();
        const int explosionTail = HasEndExplosionArea()
            ? EndExplosionDelayMs() + std::max(
                ExtraDurationMs(), std::max(0, Data.EndExplosionDuration))
            : 0;
        return impactTick + std::max(ExtraDurationMs(), explosionTail) + 100;
    }

    int ImpactTick() const {
        if (!Native) {
            return 0;
        }
        if (ProjectileTerminationTick > 0) {
            return ProjectileTerminationTick;
        }
        if (Native->SData.MissileAccel != 0) {
            return Native->StartTime + 5000;
        }
        const float speed = Native->SData.MissileSpeed <= 0 ||
                            Native->SData.MissileSpeed == INT_MAX
            ? 100000000.0f
            : static_cast<float>(Native->SData.MissileSpeed);
        return Native->StartTime + std::max(0, Native->SData.Delay) +
            static_cast<int>(1000.0f * TravelDistance() / speed);
    }

    int EndExplosionImpactTick() const {
        return ImpactTick() + EndExplosionDelayMs();
    }

    bool IsActive(int now = SDK::Variables::TickCount()) const {
        return Native && (Persistent || now <= EndTick());
    }

    Vec2 MissilePosition(int afterTimeMs = 0) const {
        if (!Native) {
            return {};
        }
        if (ProjectileTerminated) {
            return CollisionEnd.IsZero() ? Native->EndPosition : CollisionEnd;
        }
        if (const auto* missile = dynamic_cast<const SDK::SkillshotMissile*>(Native.get())) {
            if (Native->SData.MissileAccel != 0) {
                const Vec2 predicted = missile->GetMissilePosition(afterTimeMs);
                return Native->StartPosition + Native->Direction * std::clamp(
                    Native->StartPosition.Distance(predicted),
                    0.0f, TravelDistance());
            }
            if (missile->Missile.IsValid()) {
                const Vec2 current = missile->Missile.Position().To2D();
                const Vec2 end = missile->Missile.EndPosition().To2D();
                const float speed = std::max(
                    1.0f, static_cast<float>(Native->SData.MissileSpeed));
                const float distance = std::min(current.Distance(end),
                    speed * std::max(0, afterTimeMs) / 1000.0f);
                const Vec2 direction = (end - current).Normalized();
                if (!direction.IsZero()) {
                    return current + direction * distance;
                }
            }
            return missile->GetMissilePosition(afterTimeMs);
        }

        const int elapsed = std::max(0,
            SDK::Variables::TickCount() + afterTimeMs - Native->StartTime -
            std::max(0, Native->SData.Delay));
        const float speed = Native->SData.MissileSpeed <= 0 ||
                            Native->SData.MissileSpeed == INT_MAX
            ? 0.0f
            : static_cast<float>(Native->SData.MissileSpeed);
        const float distance = std::clamp(
            speed * static_cast<float>(elapsed) / 1000.0f,
            0.0f, TravelDistance());
        return Native->StartPosition + Native->Direction * distance;
    }

    std::vector<Vec2> PolygonPoints() const {
        return Native ? SourceGeometry::ToPoints(Native->Path) : std::vector<Vec2>();
    }

    bool ContainsStatic(const Vec2& point,
                        float unitRadius,
                        const EvadeSettings& settings) const {
        if (!Native || ProjectileWallSuppressesEndpointHazard()) {
            return false;
        }
        const float radius = EffectiveRadius(settings, unitRadius);
        if (IsLine()) {
            const Vec2 lineStart = IsFiniteMissile() ? MissilePosition(0) : Native->StartPosition;
            return (!ProjectileTerminated &&
                    SourceGeometry::PointSegmentDistance(
                        point, lineStart, EffectiveEnd(settings)) <= radius) ||
                   EndExplosionContains(point, unitRadius, settings);
        }
        if (IsCircle()) {
            return point.Distance(CollisionEnd.IsZero() ? Native->EndPosition : CollisionEnd) <= radius;
        }
        if (IsRing()) {
            const float distance = point.Distance(Native->EndPosition);
            const float padding = unitRadius + settings.SkillShotsExtraRadius + settings.ExtraEvadeDistance;
            const float outer = RawRadius() +
                static_cast<float>(Native->SData.RingRadius) + padding;
            const float inner = std::max(0.0f,
                RawRadius() - static_cast<float>(Native->SData.RingRadius) - padding);
            return distance >= inner && distance <= outer;
        }

        const auto polygon = PolygonPoints();
        return SourceGeometry::DistanceToPolygon(point, polygon) <=
            std::max(0.0f, unitRadius + settings.SkillShotsExtraRadius + settings.ExtraEvadeDistance);
    }

    float HitTime(const Vec2& point,
                  const EvadeSettings& settings,
                  int now = SDK::Variables::TickCount()) const {
        if (!Native || ProjectileWallSuppressesEndpointHazard()) {
            return FLT_MAX;
        }
        const float latency = static_cast<float>(std::max(0, SDK::Game::Ping())) * 0.5f;
        if (IsLine() && IsFiniteMissile()) {
            const Vec2 missile = MissilePosition(0);
            float best = FLT_MAX;
            if (!ProjectileTerminated && SourceGeometry::PointSegmentDistance(
                    point, missile, EffectiveEnd(settings)) <=
                EffectiveRadius(settings)) {
                const Vec2 projection = SourceGeometry::ProjectOn(
                    point, missile, EffectiveEnd(settings)).SegmentPoint;
                const float forward = (projection - missile).Dot(Native->Direction);
                if (forward >= -RawRadius()) {
                    best = std::max(0.0f,
                        1000.0f * std::max(0.0f, forward) /
                            static_cast<float>(std::max(1, Native->SData.MissileSpeed)) -
                        latency);
                }
            }
            if (EndExplosionContains(point, 0.0f, settings)) {
                best = std::min(best, std::max(0.0f,
                    static_cast<float>(EndExplosionImpactTick() - now) - latency));
            }
            return best;
        }
        const int impactTick = ImpactTick();
        if (now > impactTick) {
            if (now <= EndTick() && ContainsStatic(point, 0.0f, settings)) {
                return 0.0f;
            }
            return FLT_MAX;
        }
        return std::max(0.0f, static_cast<float>(impactTick - now) - latency);
    }

    bool CanHeroEvade(const SDK::AIHeroClient& hero,
                      const EvadeSettings& settings,
                      float* evadeTimeOut = nullptr,
                      float* hitTimeOut = nullptr) const {
        if (!Native || !hero.IsValid()) {
            return false;
        }
        const Vec2 heroPos = hero.Position().To2D();
        const float speed = std::max(50.0f, hero.MoveSpeed());
        float distanceOutside = 0.0f;
        if (IsLine()) {
            const float linePenetration = ProjectileTerminated
                ? 0.0f
                : std::max(0.0f,
                    EffectiveRadius(settings, hero.BoundingRadius()) -
                    SourceGeometry::PointSegmentDistance(heroPos,
                        IsFiniteMissile() ? MissilePosition(0) :
                            Native->StartPosition, CollisionEnd.IsZero()
                            ? Native->EndPosition : CollisionEnd));
            const float explosionPenetration = HasEndExplosionArea()
                ? std::max(0.0f, EndExplosionSignedPenetration(
                    heroPos, hero.BoundingRadius() +
                        static_cast<float>(std::max(
                            0, settings.SkillShotsExtraRadius))))
                : 0.0f;
            distanceOutside = std::max(linePenetration, explosionPenetration);
        } else if (IsCircle()) {
            distanceOutside = std::max(0.0f,
                EffectiveRadius(settings, hero.BoundingRadius()) -
                heroPos.Distance(CollisionEnd.IsZero() ? Native->EndPosition : CollisionEnd));
        } else {
            const auto polygon = PolygonPoints();
            distanceOutside = SourceGeometry::PointInPolygon(heroPos, polygon)
                ? std::max(15.0f, hero.BoundingRadius())
                : 0.0f;
        }
        const float evadeTime = 1000.0f * distanceOutside / speed;
        const float hitTime = HitTime(heroPos, settings);
        if (evadeTimeOut) {
            *evadeTimeOut = evadeTime;
        }
        if (hitTimeOut) {
            *hitTimeOut = hitTime;
        }
        return hitTime > evadeTime;
    }

    bool ContainsAt(const Vec2& point,
                    float unitRadius,
                    int afterTimeMs,
                    const EvadeSettings& settings) const {
        if (!Native || afterTimeMs < 0) {
            return false;
        }
        const int absoluteTick = SDK::Variables::TickCount() + afterTimeMs;
        if (!Persistent && absoluteTick > EndTick()) {
            return false;
        }

        if (IsLine() && IsFiniteMissile()) {
            const int launchTick = Native->StartTime + std::max(0, Native->SData.Delay);
            if (absoluteTick < launchTick) {
                return false;
            }
            const int impactTick = ImpactTick();
            const int explosionTick = EndExplosionImpactTick();
            const int tolerance = 35;
            if (absoluteTick + tolerance >= explosionTick &&
                absoluteTick <= EndTick() &&
                EndExplosionContains(point, unitRadius, settings)) {
                return true;
            }
            if (ProjectileTerminated) {
                return false;
            }
            if (absoluteTick > impactTick + tolerance) {
                return false;
            }
            const Vec2 missile = MissilePosition(afterTimeMs);
            return point.Distance(missile) <= EffectiveRadius(
                settings, unitRadius);
        }

        const int activeStartTick = Native->StartTime + std::max(0, Native->SData.Delay) - 100;
        const int activeEndTick = EndTick();
        if (absoluteTick < activeStartTick || absoluteTick > activeEndTick) {
            return false;
        }
        return ContainsStatic(point, unitRadius, settings);
    }

    SourceSafePathResult IsSafePath(const std::vector<Vec2>& path,
                                    int timeOffset,
                                    float speed,
                                    int delay,
                                    float unitRadius,
                                    const EvadeSettings& settings) const {
        SourceSafePathResult result;
        if (!Native || path.size() < 2) {
            result.IsSafe = !ContainsStatic(path.empty() ? Vec2() : path.front(),
                                            unitRadius, settings);
            return result;
        }

        speed = std::max(50.0f, speed);
        delay = std::max(0, delay);
        timeOffset = std::max(0, timeOffset) + std::max(0, SDK::Game::Ping()) / 2;
        const float length = SourceGeometry::PathLength(path);
        const int arrival = delay + static_cast<int>(1000.0f * length / speed);
        int horizon = std::min(6000, std::max(arrival + timeOffset,
            std::min(INT_MAX / 2, EndTick() - SDK::Variables::TickCount() + timeOffset)));
        horizon = std::max(horizon, delay);

        const int step = IsFiniteMissile() ? 18 : 25;
        Vec2 previous = path.front();
        for (int t = 0; t <= horizon; t += step) {
            const float travelled = t <= delay
                ? 0.0f
                : speed * static_cast<float>(t - delay) / 1000.0f;
            const Vec2 position = SourceGeometry::PositionAfter(path, travelled);
            if (ContainsAt(position, unitRadius, t, settings)) {
                result.IsSafe = false;
                result.Intersection.ComingFrom = previous;
                result.Intersection.Point = position;
                result.Intersection.Distance = std::min(length, travelled);
                result.Intersection.Time = t;
                result.Intersection.Valid = !position.IsZero();
                return result;
            }
            previous = position;
        }

        // Preserve the source's conservative route-change offset by testing
        // both sides of the predicted collision window.
        for (int delta : { -timeOffset, timeOffset }) {
            const int t = std::clamp(arrival + delta, 0, horizon);
            const float travelled = t <= delay
                ? 0.0f
                : speed * static_cast<float>(t - delay) / 1000.0f;
            if (ContainsAt(SourceGeometry::PositionAfter(path, travelled),
                           unitRadius, t, settings)) {
                result.IsSafe = false;
                return result;
            }
        }
        return result;
    }

    bool IsAboutToHit(int timeMs,
                      const SDK::AIBaseClient& unit,
                      const EvadeSettings& settings) const {
        if (!Native || !unit.IsValid()) {
            return false;
        }
        const Vec2 position = unit.Position().To2D();
        if (IsLine() && IsFiniteMissile()) {
            const int now = SDK::Variables::TickCount();
            const int untilImpact = std::max(0, ImpactTick() - now);
            if (!ProjectileTerminated && now <= ImpactTick()) {
                const Vec2 from = MissilePosition(0);
                const Vec2 to = MissilePosition(std::min(
                    std::max(0, timeMs), untilImpact));
                if (SourceGeometry::PointSegmentDistance(position, from, to) <=
                    EffectiveRadius(settings, unit.BoundingRadius())) {
                    return true;
                }
            }
            return EndExplosionImpactTick() - now <=
                       std::max(0, timeMs) &&
                   EndExplosionContains(position, unit.BoundingRadius(), settings);
        }
        return HitTime(position, settings) <= static_cast<float>(std::max(0, timeMs)) &&
            ContainsStatic(position, unit.BoundingRadius(), settings);
    }

    std::vector<std::vector<Vec2>> EvadeBoundaries(float unitRadius,
                                                    float extraEvadeDistance,
                                                    const EvadeSettings& settings) const {
        std::vector<std::vector<Vec2>> result;
        if (!Native || ProjectileWallSuppressesEndpointHazard()) {
            return result;
        }
        const float padding = std::max(0.0f, unitRadius) +
            std::max(0.0f, extraEvadeDistance) +
            static_cast<float>(std::max(0, settings.SkillShotsExtraRadius));
        if (IsLine()) {
            if (!ProjectileTerminated) {
                result.push_back(SourceGeometry::RectanglePoints(
                    IsFiniteMissile() ? MissilePosition(0) : Native->StartPosition,
                    EffectiveEnd(settings),
                    RawRadius() + padding));
            }
            if (HasEndExplosionArea()) {
                auto explosion = EndExplosionPolygons(padding);
                result.insert(result.end(),
                    std::make_move_iterator(explosion.begin()),
                    std::make_move_iterator(explosion.end()));
            }
        } else if (IsCircle()) {
            result.push_back(SourceGeometry::CirclePoints(
                CollisionEnd.IsZero() ? Native->EndPosition : CollisionEnd,
                RawRadius() + padding));
        } else if (IsRing()) {
            result.push_back(SourceGeometry::CirclePoints(
                Native->EndPosition, RawRadius() +
                    static_cast<float>(Native->SData.RingRadius) + padding));
            const float inner = std::max(5.0f,
                RawRadius() - static_cast<float>(Native->SData.RingRadius) - padding);
            result.push_back(SourceGeometry::CirclePoints(Native->EndPosition, inner));
        } else if (IsCone()) {
            result.push_back(SourceGeometry::SectorPoints(
                Native->StartPosition, Native->Direction,
                static_cast<float>(std::max(1, Native->SData.Angle)) *
                    SourceGeometry::Pi / 180.0f,
                static_cast<float>(std::max(1, Native->SData.Range)), padding));
        } else {
            auto polygon = PolygonPoints();
            if (!polygon.empty()) {
                // Arc and other uncommon SDK polygons are already built by
                // the retained special-spell geometry. Push vertices away
                // from their centroid to obtain the source evade polygon.
                Vec2 center;
                for (const Vec2& point : polygon) {
                    center = center + point;
                }
                center = center / static_cast<float>(polygon.size());
                for (Vec2& point : polygon) {
                    Vec2 direction = (point - center).Normalized();
                    if (!direction.IsZero()) {
                        point = point + direction * padding;
                    }
                }
                result.push_back(std::move(polygon));
            }
        }
        return result;
    }
};

using SourceSkillshotPtr = std::shared_ptr<SourceSkillshot>;
using SourceSkillshotList = std::vector<SourceSkillshotPtr>;

} // namespace Plugins::KuroEvade
