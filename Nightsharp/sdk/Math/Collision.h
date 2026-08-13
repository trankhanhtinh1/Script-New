#pragma once

// ============================================================================
// Collision.h — collision detection for skillshots
// ----------------------------------------------------------------------------
// Runtime source-of-truth: EnsoulSharp.SDK.dll / EnsoulSharp.SDK.Collisions.
// NightSharp additions:
//   - SDK::Collisions namespace alias for DLL parity.
//   - CollisionObjectsBridge support: DLL-style arrays + old bit flags.
//   - MelW projectile barrier handling (EnsoulSharp.SDK.dll predates Mel).
// ============================================================================

#include "../Core/Game.h"
#include "../Core/NavMesh.h"
#include "../Core/Objects.h"
#include "../../core/CoreNavGrid.h"
#include "../Core/Variables.h"
#include "../Enumerations/SpellSlot.h"
#include "../Events/Events.h"
#include "../Extensions/AIBaseClientExtensions.h"
#include "../Extensions/Unit.h"
#include "../GameObjects/GameObjects.h"
#include "../GameObjects/ObjectManager.h"
#include "../GameObjects/YasuoWallTracker.h"
#include "HealthPrediction.h"
#include "Prediction.h"
#include "MovingProjectileCollision.h"
#include "../../SectionProfiler.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace SDK::Collision {

inline bool WillDead(const PredictionInput& input,
                     const AIBaseClient& minion,
                     float distance);
inline bool HasYasuoWindWallCollision(const Vector3& start,
                                      const Vector3& end,
                                      float extraRadius);
inline bool HasProjectileWallCollision(const Vector3& start,
                                       const Vector3& end,
                                       float extraRadius);

namespace detail {

inline bool Initialized = false;

inline std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}


inline void Initialize() {
    if (Initialized) {
        YasuoWallTracker::EnsureInitialized();
        return;
    }
    Initialized = true;
    YasuoWallTracker::EnsureInitialized();
}

inline bool ContainsCollisionObject(const PredictionInput& input, CollisionableObjects object) {
    return input.CollisionObjects.contains(object);
}


inline float UnitBoundingRadius(const AIBaseClient& unit) {
    return unit.IsValid() ? unit.BoundingRadius() : 0.0f;
}

inline Vector3 ServerPositionOrPosition(const AIBaseClient& unit) {
    if (!unit.IsValid()) {
        return Vector3();
    }

    const auto serverPosition = unit.ServerPosition();
    return serverPosition.IsValid() && !serverPosition.IsZero()
        ? serverPosition
        : unit.Position();
}

inline bool IsStructureObject(const GameObject& object) {
    const auto type = object.Type();
    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    // return type == ::Core::Objects::ObjectType::AITurretClient ||
    //        type == ::Core::Objects::ObjectType::BarracksDampenerClient ||
    //        type == ::Core::Objects::ObjectType::HQClient;
    // REMOVED: Turret/Inhibitor/Nexus disabled
    (void)type;
    return false;
}

inline bool ContainsAnyLower(const std::string& value,
                             std::initializer_list<const char*> needles) {
    for (const auto* needle : needles) {
        if (needle && value.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

inline std::string RuntimeMinionName(const AIMinionClient& minion) {
    std::string name = minion.CharacterName();
    if (name.empty()) {
        name = minion.Name();
    }
    return ToLower(std::move(name));
}

inline bool IsJungleCompanionObject(const AIMinionClient& minion) {
    const std::string name = RuntimeMinionName(minion);
    return ContainsAnyLower(name, {
        "junglepet",
        "jungle_pet",
        "junglecompanion",
        "scorchclaw",
        "gustwalker",
        "mosstomper"
    });
}

inline bool IsCollisionMinionCandidate(const AIMinionClient& minion) {
    if (!minion.IsValid() || (minion.IsDead() && !minion.IsZombie())) {
        return false;
    }

    if (IsStructureObject(minion)) {
        return false;
    }

    if (IsJungleCompanionObject(minion) || !minion.IsTargetable()) {
        return false;
    }

    if (HasFlag(minion.GetMinionType(), MinionTypes::Ward)) {
        return false;
    }

    const GameObjectTeam team = minion.Team();
    if (team == GameObjectTeam::Neutral) {
        if (minion.IsPlant()) {
            return minion.MaxHealth() > 0.0f;
        }
        return !minion.IsPet() && minion.IsJungle() && minion.MaxHealth() > 6.0f;
    }

    return team != GameObjectTeam::Unknown && minion.MaxHealth() > 0.0f;
}

inline bool IsValidCollisionTarget(const GameObject& object,
                                   const PredictionInput& input) {
    if (object.Compare(input.Unit) ||
        !object.IsValid() ||
        (object.IsDead() && !object.IsZombie()) ||
        !object.IsTargetable()) {
        return false;
    }

    // A visible physical hero still intercepts a projectile while gameplay
    // invulnerability prevents damage, so collision intentionally does not use
    // Extensions::IsValidTarget here.
    if (object.IsHero() && !object.IsVisible()) {
        return false;
    }

    const auto player = ObjectManager::Player();
    if (player.IsValid() && player.Team() == object.Team()) {
        return false;
    }

    const Vector3 position = ServerPositionOrPosition(AIBaseClient(object.Handle()));
    return position.IsValid() && !position.IsZero();
}

inline bool IsValidMinionCollisionTarget(const AIMinionClient& minion,
                                         const PredictionInput& input) {
    if (minion.Compare(input.Unit) || !IsCollisionMinionCandidate(minion)) {
        return false;
    }

    const auto player = ObjectManager::Player();
    if (player.IsValid() && player.Team() == minion.Team()) {
        return false;
    }

    const Vector3 position = ServerPositionOrPosition(AIBaseClient(minion.Handle()));
    return position.IsValid() && !position.IsZero();
}

inline float DistanceSquaredToSegment(const Vec2& point,
                                      const Vec2& segmentStart,
                                      const Vec2& segmentEnd) {
    const Vec2 segment = segmentEnd - segmentStart;
    const float lengthSquared = segment.LengthSqr();
    if (lengthSquared <= 1.0e-6f) {
        return point.DistanceSqr(segmentStart);
    }
    const float projection = std::clamp(
        (point - segmentStart).Dot(segment) / lengthSquared,
        0.0f,
        1.0f);
    return point.DistanceSqr(segmentStart + segment * projection);
}
// Kept for non-collision geometry callers that intentionally reject endpoint
// caps (for example Viktor laser target counting).
inline float DistanceSquaredToSegmentOnly(const Vec2& point,
                                          const Vec2& segmentStart,
                                          const Vec2& segmentEnd) {
    const Vec2 segment = segmentEnd - segmentStart;
    const float lengthSquared = segment.LengthSqr();
    if (lengthSquared <= 1.0e-6f) {
        return std::numeric_limits<float>::max();
    }
    const float projection =
        (point - segmentStart).Dot(segment) / lengthSquared;
    if (projection < 0.0f || projection > 1.0f) {
        return std::numeric_limits<float>::max();
    }
    return point.DistanceSqr(segmentStart + segment * projection);
}

inline float ResolveBlockerSpeed(const AIBaseClient& unit) {
    if (!unit.IsValid() || !SDK::CanMove(unit)) {
        return 0.0f;
    }
    if (Extensions::IsDashing(unit)) {
        const auto dash = Extensions::GetDashInfo(unit);
        if (std::isfinite(dash.Speed) && dash.Speed > 0.0f) {
            return dash.Speed;
        }
    }
    const float speed = unit.MoveSpeed();
    return std::isfinite(speed) && speed > 0.0f ? speed : 0.0f;
}

inline float ProjectileLifetime(const PredictionInput& input,
                                const Vector3& endpoint) {
    const float delay = std::isfinite(input.Delay)
        ? std::max(0.0f, input.Delay)
        : 0.0f;
    if (input.Speed == FLT_MAX) {
        return delay;
    }
    if (!std::isfinite(input.Speed) || input.Speed <= 0.0f) {
        return -1.0f;
    }
    return delay + input.ResolveFrom().Distance2D(endpoint) / input.Speed;
}

inline bool PassesReachableBroadPhase(const Vector3& blockerPosition,
                                      float blockerSpeed,
                                      float combinedRadius,
                                      const Vector3& from,
                                      const Vector3& endpoint,
                                      float projectileLifetime) {
    if (projectileLifetime < 0.0f || !std::isfinite(projectileLifetime)) {
        return false;
    }
    const float reachableRadius = combinedRadius +
        blockerSpeed * projectileLifetime;
    return DistanceSquaredToSegment(
               blockerPosition.To2D(),
               from.To2D(),
               endpoint.To2D()) <= reachableRadius * reachableRadius;
}

inline std::optional<float> FindUnitContact(const AIBaseClient& unit,
                                            const PredictionInput& input,
                                            const Vector3& endpoint,
                                            float combinedRadius,
                                            float blockerSpeed) {
    const Vector3 blockerPosition = ServerPositionOrPosition(unit);
    const auto& waypoints = unit.CachedWaypoints();
    return MovingProjectileCollision::FirstContactTime(
        input.ResolveFrom(),
        endpoint,
        input.Delay,
        input.Speed,
        combinedRadius,
        blockerPosition,
        blockerSpeed,
        std::span<const Vec3>(waypoints.data(), waypoints.size()));
}

inline bool AddIfUnique(std::vector<AIBaseClient>& result, const GameObject& object) {
    if (!object.IsValid()) {
        return false;
    }

    const int networkId = object.NetworkId();
    const auto exists = std::find_if(result.begin(), result.end(), [&](const AIBaseClient& entry) {
        return entry.IsValid() && entry.NetworkId() == networkId;
    });
    if (exists != result.end()) {
        return false;
    }
    result.emplace_back(object.Handle());
    return true;
}

inline void AddPlayerSentinel(std::vector<AIBaseClient>& result) {
    (void)AddIfUnique(result, ObjectManager::Player());
}

inline bool ShouldStopCollisionScan(const std::vector<AIBaseClient>& result,
                                    const PredictionInput& input) {
    return ExceedsCollisionAllowance(result.size(), input.MaxCollisionCount);
}

inline bool HasAnyBuff(const AIBaseClient& unit, std::initializer_list<const char*> names) {
    if (!unit.IsValid()) {
        return false;
    }
    for (const auto* name : names) {
        if (name && unit.HasBuff(name)) {
            return true;
        }
    }
    return false;
}

// This path serves the local player's own skillshots (Spell::GetPrediction ->
// GetCollision), so only a wall owned by an enemy Yasuo stops them. Our own wall,
// or an ally's, lets our projectiles through.
inline bool SegmentIntersectsYasuoWall(const Vector3& start,
                                       const Vector3& end,
                                       float projectileRadius) {
    if (!start.IsValid() || !end.IsValid() ||
        start.IsZero() || end.IsZero() ||
        !std::isfinite(projectileRadius) || projectileRadius < 0.0f) {
        return false;
    }
    return YasuoWallTracker::Intersects(
        start,
        end,
        projectileRadius,
        YasuoWallTracker::WallOwner::Enemy);
}

inline bool HasCircularShieldCollision(const char* championName,
                                       std::initializer_list<const char*> buffNames,
                                       const Vector3& start,
                                       const Vector3& end,
                                       float baseRadius,
                                       float extraRadius) {
    if (!championName || !start.IsValid() || !end.IsValid() ||
        start.IsZero() || end.IsZero() ||
        !std::isfinite(baseRadius) || baseRadius < 0.0f ||
        !std::isfinite(extraRadius) || extraRadius < 0.0f) {
        return false;
    }

    const float radius = baseRadius + extraRadius;
    if (!std::isfinite(radius)) {
        return false;
    }
    const Vec2 start2D = start.To2D();
    const Vec2 end2D = end.To2D();
    const std::string targetName = ToLower(championName);

    for (const auto& hero : GameObjects::EnemyHeroesFrame()) {
        if (!hero.IsValid() ||
            (hero.IsDead() && !hero.IsZombie()) ||
            !hero.IsVisible() ||
            !hero.IsTargetable() ||
            ToLower(hero.CharacterName()) != targetName) {
            continue;
        }

        AIBaseClient unit(hero.Handle());
        if (!HasAnyBuff(unit, buffNames)) {
            continue;
        }

        const Vector3 shieldPosition = ServerPositionOrPosition(unit);
        if (!shieldPosition.IsValid() || shieldPosition.IsZero()) {
            continue;
        }
        if (DistanceSquaredToSegment(
                shieldPosition.To2D(),
                start2D,
                end2D) <= radius * radius) {
            return true;
        }
    }
    return false;
}
inline bool HasSamiraCollision(const Vector3& start, const Vector3& end, float extraRadius) {
    return HasCircularShieldCollision(
        "Samira",
        { "SamiraW", "SamiraWBuff" },
        start,
        end,
        325.0f,
        extraRadius);
}

inline bool HasMelCollision(const Vector3& start, const Vector3& end, float extraRadius) {
    return HasCircularShieldCollision(
        "Mel",
        { "MelW", "MelWBuff", "MelWReflect", "MelRebuttal" },
        start,
        end,
        175.0f,
        extraRadius);
}

inline bool IsDeadAtContact(const AIBaseClient& minion, float contactTime) {
    if (!minion.IsValid() || !std::isfinite(contactTime) || contactTime < 0.0f) {
        return false;
    }

    HealthPrediction::Initialize();
    const double predictionMs = std::clamp(
        static_cast<double>(contactTime) * 1000.0 -
            static_cast<double>(Game::Ping()),
        0.0,
        static_cast<double>(std::numeric_limits<int>::max()));
    return HealthPrediction::GetPrediction(
               minion,
               static_cast<int>(predictionMs),
               0) <= 0.0f;
}

inline bool ProcessHeroes(std::vector<AIBaseClient>& result,
                          const Vector3& endpoint,
                          const PredictionInput& input) {
    const Vector3 from = input.ResolveFrom();
    const float projectileLifetime = ProjectileLifetime(input, endpoint);
    for (const auto& hero : GameObjects::EnemyHeroesFrame()) {
        if (!IsValidCollisionTarget(hero, input)) {
            continue;
        }

        AIBaseClient unit(hero.Handle());
        const Vector3 blockerPosition = ServerPositionOrPosition(unit);
        const float blockerSpeed = ResolveBlockerSpeed(unit);
        const float radius = input.Radius + 50.0f + hero.BoundingRadius();
        if (!PassesReachableBroadPhase(
                blockerPosition,
                blockerSpeed,
                radius,
                from,
                endpoint,
                projectileLifetime)) {
            continue;
        }

        if (FindUnitContact(unit, input, endpoint, radius, blockerSpeed).has_value() &&
            AddIfUnique(result, hero) &&
            ShouldStopCollisionScan(result, input)) {
            return true;
        }
    }
    return false;
}

inline bool ProcessMinionList(std::vector<AIBaseClient>& result,
                              const std::vector<AIMinionClient>& minions,
                              const Vector3& endpoint,
                              const PredictionInput& input) {
    const Vector3 from = input.ResolveFrom();
    const float projectileLifetime = ProjectileLifetime(input, endpoint);
    for (const auto& minion : minions) {
        if (!IsValidMinionCollisionTarget(minion, input)) {
            continue;
        }

        AIBaseClient unit(minion.Handle());
        const Vector3 blockerPosition = ServerPositionOrPosition(unit);
        const float blockerSpeed = ResolveBlockerSpeed(unit);
        const float padding = minion.IsJungle() ? 20.0f : 15.0f;
        const float radius = input.Radius + padding + minion.BoundingRadius();
        if (!PassesReachableBroadPhase(
                blockerPosition,
                blockerSpeed,
                radius,
                from,
                endpoint,
                projectileLifetime)) {
            continue;
        }

        const auto contact = FindUnitContact(
            unit,
            input,
            endpoint,
            radius,
            blockerSpeed);
        if (!contact.has_value() || IsDeadAtContact(unit, *contact)) {
            continue;
        }

        if (AddIfUnique(result, minion) &&
            ShouldStopCollisionScan(result, input)) {
            return true;
        }
    }
    return false;
}

inline const std::vector<AIMinionClient>& SnapshotCollisionMinions() {
    static thread_local std::vector<AIMinionClient> cached;
    static thread_local int cachedFrame = 0;
    static thread_local bool hasCachedFrame = false;

    const int frame = ::CoreAiManager::FrameCacheKey();
    if (hasCachedFrame && cachedFrame == frame) {
        return cached;
    }

    cached.clear();
    if (cached.capacity() < 64) {
        cached.reserve(64);
    }

    const auto addUnique = [&](const AIMinionClient& minion) {
        if (!IsCollisionMinionCandidate(minion)) {
            return;
        }
        const int networkId = minion.NetworkId();
        const uintptr_t address = minion.Address();
        const auto exists = std::find_if(cached.begin(), cached.end(), [&](const AIMinionClient& entry) {
            if (!entry.IsValid()) {
                return false;
            }
            if (networkId != 0 && networkId != -1 && entry.NetworkId() == networkId) {
                return true;
            }
            return address != 0 && entry.Address() == address;
        });
        if (exists == cached.end()) {
            cached.push_back(minion);
        }
    };

    const auto addList = [&](const auto& list) {
        for (const auto& minion : list) {
            addUnique(minion);
        }
    };
    addList(GameObjects::EnemyMinionsFrame());
    addList(GameObjects::EnemySpecialMinionsFrame());
    addList(GameObjects::EnemyPetsFrame());
    addList(GameObjects::EnemyClonesFrame());
    addList(GameObjects::JungleFrame());
    addList(GameObjects::PlantsFrame());

    cachedFrame = frame;
    hasCachedFrame = true;
    return cached;
}

inline void ProcessBuildings(std::vector<AIBaseClient>& result,
                             const Vector3& position,
                             const PredictionInput& input) {
    // Building collision remains the canonical SDK no-op.
    (void)result;
    (void)position;
    (void)input;
}

inline bool ProcessWalls(std::vector<AIBaseClient>& result,
                         const Vector3& endpoint,
                         const PredictionInput& input) {
    Vector3 hitPoint{};
    if (!::CoreNavGrid::FindWallCollision(
            input.ResolveFrom(),
            endpoint,
            hitPoint)) {
        return false;
    }
    return AddIfUnique(result, ObjectManager::Player()) &&
           ShouldStopCollisionScan(result, input);
}

inline bool ProcessProjectileWalls(std::vector<AIBaseClient>& result,
                                   const Vector3& endpoint,
                                   const PredictionInput& input) {
    if (!input.Collision) {
        return false;
    }
    const Vector3 from = input.ResolveFrom();

    if (ContainsCollisionObject(input, CollisionableObjects::YasuoWall) &&
        SegmentIntersectsYasuoWall(from, endpoint, input.Radius) &&
        AddIfUnique(result, ObjectManager::Player()) &&
        ShouldStopCollisionScan(result, input)) {
        return true;
    }
    if (ContainsCollisionObject(input, CollisionableObjects::SamiraWall) &&
        HasSamiraCollision(from, endpoint, input.Radius) &&
        AddIfUnique(result, ObjectManager::Player()) &&
        ShouldStopCollisionScan(result, input)) {
        return true;
    }
    if (ContainsCollisionObject(input, CollisionableObjects::MelWall) &&
        HasMelCollision(from, endpoint, input.Radius) &&
        AddIfUnique(result, ObjectManager::Player()) &&
        ShouldStopCollisionScan(result, input)) {
        return true;
    }
    return false;
}

inline bool ScanEndpoint(std::vector<AIBaseClient>& result,
                         const Vector3& endpoint,
                         const PredictionInput& input) {
    if (!endpoint.IsValid() || endpoint.IsZero()) {
        return false;
    }

    if (ContainsCollisionObject(input, CollisionableObjects::Heroes) &&
        ProcessHeroes(result, endpoint, input)) {
        return true;
    }
    if (ContainsCollisionObject(input, CollisionableObjects::Minions) &&
        ProcessMinionList(result, SnapshotCollisionMinions(), endpoint, input)) {
        return true;
    }
    if (ContainsCollisionObject(input, CollisionableObjects::Building)) {
        ProcessBuildings(result, endpoint, input);
    }
    if (ContainsCollisionObject(input, CollisionableObjects::Walls) &&
        ProcessWalls(result, endpoint, input)) {
        return true;
    }
    return ProcessProjectileWalls(result, endpoint, input);
}

} // namespace detail

inline void Initialize() {
    detail::Initialize();
}

// ============================================================================
// GetCollision — every overload delegates to the same endpoint scan.
// ============================================================================
inline std::vector<AIBaseClient> GetCollision(
    const Vector3& endpoint,
    const PredictionInput& input) {
    NS_PROFILE("FsPred.Collision");
    Initialize();
    std::vector<AIBaseClient> result;
    (void)detail::ScanEndpoint(result, endpoint, input);
    return result;
}

inline std::vector<AIBaseClient> GetCollision(
    const std::vector<Vector3>& positions,
    PredictionInput input) {
    Initialize();
    std::vector<AIBaseClient> result;
    for (const auto& endpoint : positions) {
        if (detail::ScanEndpoint(result, endpoint, input)) {
            break;
        }
    }
    return result;
}

inline std::vector<AIBaseClient> GetCollision(const AIBaseClient& target,
                                              PredictionInput input) {
    input.Unit = target;
    auto prediction = Prediction::GetPrediction(target, input);
    return prediction.CollisionObjects;
}

inline bool IsCollision(const std::vector<Vector3>& positions, PredictionInput input) {
    return !GetCollision(positions, input).empty();
}

inline bool HasCollision(const AIBaseClient& target, PredictionInput input) {
    return !GetCollision(target, input).empty();
}

inline bool HasCollision(const std::vector<Vector3>& positions, PredictionInput input) {
    return IsCollision(positions, input);
}

// ============================================================================
// Projectile-wall helpers
// ============================================================================
inline bool HasSamiraWallCollision(const Vector3& start,
                                   const Vector3& end,
                                   float extraRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() ||
        start.IsZero() || end.IsZero() ||
        !std::isfinite(extraRadius) || extraRadius < 0.0f) {
        return false;
    }
    Initialize();
    return detail::HasSamiraCollision(start, end, extraRadius);
}

inline bool HasMelWallCollision(const Vector3& start,
                                const Vector3& end,
                                float extraRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() ||
        start.IsZero() || end.IsZero() ||
        !std::isfinite(extraRadius) || extraRadius < 0.0f) {
        return false;
    }
    Initialize();
    return detail::HasMelCollision(start, end, extraRadius);
}

inline bool HasYasuoWindWallCollision(const Vector3& start,
                                      const Vector3& end) {
    return HasYasuoWindWallCollision(start, end, 50.0f);
}

inline bool HasYasuoWindWallCollision(const Vector3& start,
                                      const Vector3& end,
                                      float extraRadius) {
    if (!start.IsValid() || !end.IsValid() ||
        start.IsZero() || end.IsZero() ||
        !std::isfinite(extraRadius) || extraRadius < 0.0f) {
        return false;
    }
    Initialize();
    return detail::SegmentIntersectsYasuoWall(start, end, extraRadius);
}

inline bool HasProjectileWallCollision(const Vector3& start,
                                       const Vector3& end,
                                       float extraRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() ||
        start.IsZero() || end.IsZero() ||
        !std::isfinite(extraRadius) || extraRadius < 0.0f) {
        return false;
    }
    Initialize();
    return detail::SegmentIntersectsYasuoWall(start, end, extraRadius) ||
           detail::HasSamiraCollision(start, end, extraRadius) ||
           detail::HasMelCollision(start, end, extraRadius);
}

inline bool IsCollision(const Vector3& position, float radius = 50.0f) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) {
        return false;
    }

    return HasProjectileWallCollision(
        detail::ServerPositionOrPosition(player),
        position,
        radius);
}

// ============================================================================
// WillDead — DLL private method exposed for parity/testing.
// ============================================================================
inline bool WillDead(const PredictionInput& input,
                     const AIBaseClient& minion,
                     float distance) {
    if (!minion.IsValid() || !std::isfinite(input.Delay) ||
        !std::isfinite(distance) || distance < 0.0f) {
        return false;
    }

    float contactTime = std::max(0.0f, input.Delay);
    if (input.Speed != FLT_MAX) {
        if (!std::isfinite(input.Speed) || input.Speed <= 0.0f) {
            return false;
        }
        contactTime += distance / input.Speed;
    }
    return detail::IsDeadAtContact(minion, contactTime);
}

// Legacy helper kept for existing NightSharp callers.
inline bool WillDead(const AIBaseClient& target, float damage) {
    if (!target.IsValid() || target.IsDead()) {
        return true;
    }
    return target.Health() <= damage;
}

inline bool HasLineCollision(const Vector3& from,
                             const Vector3& to,
                             float radius,
                             const GameObject& ignored = GameObject()) {
    PredictionInput input;
    input.Unit = ignored.IsValid() ? AIBaseClient(ignored.Handle()) : AIBaseClient();
    input.From = from;
    input.Radius = radius;
    input.Range = from.Distance(to);
    input.Collision = true;
    input.Type = SpellType::Line;
    input.CollisionObjects =
        CollisionableObjects::Minions |
        CollisionableObjects::Heroes |
        CollisionableObjects::YasuoWall |
        CollisionableObjects::SamiraWall |
        CollisionableObjects::MelWall;
    return !GetCollision(to, input).empty();
}

} // namespace SDK::Collision

namespace SDK {
namespace Collisions = Collision;
} // namespace SDK
