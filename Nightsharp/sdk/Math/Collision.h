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
#include "../Core/Variables.h"
#include "../Extensions/Unit.h"
#include "../GameObjects/GameObjects.h"
#include "../GameObjects/ObjectManager.h"
#include "HealthPrediction.h"
#include "Prediction.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

namespace SDK::Collision {

inline bool WillDead(const PredictionInput& input,
                     const AIBaseClient& minion,
                     float distance);
inline bool HasYasuoWindWallCollision(const Vector3& start,
                                      const Vector3& end,
                                      float extraRadius);

namespace detail {

inline bool Initialized = false;
inline bool YasuoInGame = false;
inline bool SamiraInGame = false;
inline bool MelInGame = false;
inline int WallCastT = 0;
inline std::vector<EffectEmitter> Windwalls;

inline std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline bool IsChampionInGame(const char* championName) {
    const auto player = ObjectManager::Player();
    const GameObjectTeam playerTeam = player.IsValid()
        ? player.Team()
        : GameObjectTeam::Unknown;
    for (const auto& hero : ObjectManager::Get<AIHeroClient>()) {
        if (hero.IsValid() &&
            (playerTeam == GameObjectTeam::Unknown ||
             hero.Team() != playerTeam) &&
            hero.CharacterName() == championName) {
            return true;
        }
    }
    return false;
}

inline void RefreshChampionFlags() {
    YasuoInGame = IsChampionInGame("Yasuo");
    SamiraInGame = IsChampionInGame("Samira");
    MelInGame = IsChampionInGame("Mel");
}

inline bool IsWindWallName(const std::string& name) {
    const auto lower = ToLower(name);
    return lower.find("yasuo") != std::string::npos &&
           lower.find("_w_windwall") != std::string::npos;
}

inline bool IsWindWallEmitter(const GameObject& object) {
    return object.IsValid() &&
           object.Type() == ::Core::Objects::ObjectType::EffectEmitter &&
           IsWindWallName(object.Name());
}

inline void AddWindwall(const EffectEmitter& emitter) {
    if (!emitter.IsValid()) {
        return;
    }
    const int networkId = emitter.NetworkId();
    const auto exists = std::find_if(Windwalls.begin(), Windwalls.end(), [&](const EffectEmitter& entry) {
        return entry.IsValid() && entry.NetworkId() == networkId;
    });
    if (exists == Windwalls.end()) {
        Windwalls.push_back(emitter);
        WallCastT = Variables::TickCount();
    }
}

inline void RefreshWindwalls() {
    Windwalls.erase(
        std::remove_if(Windwalls.begin(), Windwalls.end(), [](const EffectEmitter& emitter) {
            return !emitter.IsValid() || emitter.IsDead() || !IsWindWallName(emitter.Name());
        }),
        Windwalls.end());

    for (const auto& emitter : ObjectManager::Get<EffectEmitter>()) {
        if (IsWindWallEmitter(emitter)) {
            AddWindwall(emitter);
        }
    }

    // If collision is initialized after the wall emitter has already been
    // created, we do not receive the OnCreate/OnDoCast event. Keep a bounded
    // fallback so current active walls are still respected.
    if (!Windwalls.empty() && WallCastT == 0) {
        WallCastT = Variables::TickCount();
    }
}

inline void Initialize() {
    if (Initialized) {
        return;
    }

    Initialized = true;
    RefreshChampionFlags();
    RefreshWindwalls();
}

inline bool ContainsCollisionObject(const PredictionInput& input, CollisionableObjects object) {
    if (object == CollisionableObjects::Building) {
        return false;
    }
    return input.CollisionObjects.contains(object);
}

inline float CheckRange(const PredictionInput& input) {
    return std::min(input.Range + input.Radius + 100.0f, 2000.0f);
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
    return type == ::Core::Objects::ObjectType::AITurretClient ||
           type == ::Core::Objects::ObjectType::BarracksDampenerClient ||
           type == ::Core::Objects::ObjectType::HQClient;
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
        char buffer[96] = {};
        const uintptr_t address = minion.Address();
        if (::Core::Objects::ReadCharacterName(address, buffer, static_cast<int>(sizeof(buffer))) ||
            ::Core::Objects::ReadName(address, buffer, static_cast<int>(sizeof(buffer)))) {
            name = buffer;
        }
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

    if (HasFlag(minion.GetMinionType(), MinionTypes::Ward) || minion.IsPlant()) {
        return false;
    }

    const GameObjectTeam team = minion.Team();
    if (team == GameObjectTeam::Neutral) {
        return !minion.IsPet() && minion.IsJungle() && minion.MaxHealth() > 6.0f;
    }

    return team != GameObjectTeam::Unknown && minion.MaxHealth() > 0.0f;
}

inline bool IsValidCollisionTarget(const GameObject& object,
                                   const PredictionInput& input,
                                   float range) {
    if (object.Compare(input.Unit)) {
        return false;
    }

    if (IsStructureObject(object)) {
        return false;
    }

    if (Extensions::IsValidTarget(object, range, true, input.ResolveRangeCheckFrom())) {
        return true;
    }

    // Same defensive fallback as Movement::IsPredictionTargetUsable: the
    // current NightSharp object layer can misreport Visible/Invulnerable for
    // targetable, rendered practice/custom units. Collision still needs to
    // consider those objects if their position/team/targetable state is sane.
    if (!object.IsValid() || (object.IsDead() && !object.IsZombie()) ||
        !object.IsTargetable()) {
        return false;
    }

    if (object.IsHero() && !object.IsVisible()) {
        return false;
    }

    const auto player = ObjectManager::Player();
    if (player.IsValid() && player.Team() == object.Team()) {
        return false;
    }

    const Vector3 position = object.Position();
    if (!position.IsValid() || position.IsZero()) {
        return false;
    }

    const Vector3 origin = input.ResolveRangeCheckFrom();
    return range >= FLT_MAX || origin.IsZero() ||
           position.DistanceSqr2D(origin) < range * range;
}

inline bool IsValidMinionCollisionTarget(const AIMinionClient& minion,
                                         const PredictionInput& input,
                                         float range) {
    if (minion.Compare(input.Unit)) {
        return false;
    }

    if (!IsCollisionMinionCandidate(minion)) {
        return false;
    }

    if (Extensions::IsValidTarget(minion, range, true, input.ResolveRangeCheckFrom())) {
        return true;
    }

    const auto player = ObjectManager::Player();
    if (player.IsValid() && player.Team() == minion.Team()) {
        return false;
    }

    const Vector3 position = minion.Position();
    if (!position.IsValid() || position.IsZero()) {
        return false;
    }

    const Vector3 origin = input.ResolveRangeCheckFrom();
    return range >= FLT_MAX || origin.IsZero() ||
           position.DistanceSqr2D(origin) <= range * range;
}

inline float DistanceSquaredToSegmentOnly(const Vec2& point,
                                          const Vec2& segmentStart,
                                          const Vec2& segmentEnd) {
    const auto projection = Prediction::Vec2Ext::ProjectOn(point, segmentStart, segmentEnd);
    if (!projection.IsOnSegment) {
        return std::numeric_limits<float>::max();
    }
    return point.DistanceSqr(projection.SegmentPoint);
}

inline bool IsPointNearProjection(const Vec2& point,
                                  const Vec2& segmentStart,
                                  const Vec2& segmentEnd,
                                  float radius) {
    const auto projection = Prediction::Vec2Ext::ProjectOn(point, segmentStart, segmentEnd);
    return projection.IsOnSegment &&
           (projection.SegmentPoint.Distance(point) <= radius ||
            projection.LinePoint.Distance(point) <= radius);
}

inline void AddIfUnique(std::vector<AIBaseClient>& result, const GameObject& object) {
    if (!object.IsValid() || IsStructureObject(object)) {
        return;
    }

    const int networkId = object.NetworkId();
    const auto exists = std::find_if(result.begin(), result.end(), [&](const AIBaseClient& entry) {
        return entry.IsValid() && entry.NetworkId() == networkId;
    });
    if (exists == result.end()) {
        result.emplace_back(object.Handle());
    }
}

inline void AddPlayerSentinel(std::vector<AIBaseClient>& result) {
    AddIfUnique(result, ObjectManager::Player());
}

inline bool ShouldStopCollisionScan(const std::vector<AIBaseClient>& result,
                                    const PredictionInput& input) {
    int blockingCount = 0;
    const int targetNetworkId = input.Unit.IsValid()
        ? input.Unit.NetworkId()
        : 0;
    for (const auto& object : result) {
        if (!object.IsValid()) {
            continue;
        }
        if (targetNetworkId != 0 && object.NetworkId() == targetNetworkId) {
            continue;
        }
        ++blockingCount;
    }

    if (blockingCount <= 0) {
        return false;
    }

    if (input.MaxCollisionCount <= 0.0f) {
        return true;
    }

    return static_cast<float>(blockingCount) > input.MaxCollisionCount;
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

inline int GetWindWallLevel(const EffectEmitter& emitter) {
    if (!YasuoInGame || !emitter.IsValid()) {
        return -5;
    }

    const auto lower = ToLower(emitter.Name());
    if (lower.find("windwall5") != std::string::npos) return 5;
    if (lower.find("windwall4") != std::string::npos) return 4;
    if (lower.find("windwall3") != std::string::npos) return 3;
    if (lower.find("windwall2") != std::string::npos) return 2;
    return 1;
}

inline bool IsWallCastActive() {
    RefreshChampionFlags();
    if (!YasuoInGame) {
        return false;
    }

    RefreshWindwalls();
    return !Windwalls.empty() &&
           Variables::TickCount() - WallCastT <= 4000;
}

inline bool SegmentIntersectsWindwall(const Vector3& start,
                                      const Vector3& end,
                                      float widthBase,
                                      float extraRadius) {
    Initialize();
    if (!IsWallCastActive()) {
        return false;
    }

    const Vec2 start2D = start.To2D();
    const Vec2 end2D = end.To2D();
    for (const auto& emitter : Windwalls) {
        if (!emitter.IsValid() || emitter.IsDead()) {
            continue;
        }

        const int level = GetWindWallLevel(emitter);
        const float wallWidth = widthBase + 50.0f * static_cast<float>(level) + extraRadius;

        Vec2 wallDir = emitter.Direction().To2D();
        if (wallDir.LengthSqr() < 0.001f) {
            // Fallback for builds where EffectEmitter.Direction is not filled:
            // use a stable horizontal segment centered on the particle.
            wallDir = Vec2(1.0f, 0.0f);
        } else {
            // The legacy NightSharp code used the perpendicular of Direction
            // for windwall span. Keep that behavior because EffectEmitter does
            // not expose the DLL's Orientation.M11/M13 matrix.
            wallDir = Vec2(-wallDir.y, wallDir.x).Normalized();
        }

        const Vec2 wallCenter = emitter.Position().To2D();
        const Vec2 wallStart = wallCenter + wallDir * (wallWidth * 0.5f);
        const Vec2 wallEnd = wallStart - wallDir * wallWidth;
        if (Prediction::Vec2Ext::Intersection(wallStart, wallEnd, end2D, start2D).Valid) {
            return true;
        }
    }
    return false;
}

inline bool HasCircularShieldCollision(const char* championName,
                                       std::initializer_list<const char*> buffNames,
                                       const Vector3& start,
                                       const Vector3& end,
                                       float baseRadius,
                                       float extraRadius) {
    const float radius = baseRadius + extraRadius;
    const Vec2 start2D = start.To2D();
    const Vec2 end2D = end.To2D();

    const auto player = ObjectManager::Player();
    const GameObjectTeam playerTeam = player.IsValid()
        ? player.Team()
        : GameObjectTeam::Unknown;
    for (const auto& hero : ObjectManager::Get<AIHeroClient>()) {
        if (playerTeam != GameObjectTeam::Unknown &&
            hero.Team() == playerTeam) {
            continue;
        }
        if (!Extensions::IsValidTarget(hero) || hero.CharacterName() != championName) {
            continue;
        }

        AIBaseClient unit(hero.Handle());
        if (!HasAnyBuff(unit, buffNames)) {
            continue;
        }

        const Vector3 shieldPosition = ServerPositionOrPosition(unit);
        if (shieldPosition.Distance(start) <= radius ||
            end.Distance(shieldPosition) <= radius) {
            return true;
        }

        if (DistanceSquaredToSegmentOnly(
                shieldPosition.To2D(),
                start2D,
                end2D) <= radius * radius) {
            return true;
        }
    }
    return false;
}

inline bool HasSamiraCollision(const Vector3& start, const Vector3& end, float extraRadius) {
    RefreshChampionFlags();
    return SamiraInGame &&
           HasCircularShieldCollision(
               "Samira",
               { "SamiraW", "SamiraWBuff" },
               start,
               end,
               325.0f,
               extraRadius);
}

inline bool HasMelCollision(const Vector3& start, const Vector3& end, float extraRadius) {
    RefreshChampionFlags();
    return MelInGame &&
           HasCircularShieldCollision(
               "Mel",
               { "MelW", "MelWBuff", "MelRebuttal" },
               start,
               end,
               175.0f,
               extraRadius);
}

inline void ProcessHeroes(std::vector<AIBaseClient>& result,
                          const Vector3& position,
                          PredictionInput input) {
    const float range = CheckRange(input);
    const Vector3 from = input.ResolveFrom();
    const Vec2 position2D = position.To2D();
    const Vec2 from2D = from.To2D();

    for (const auto& hero : ObjectManager::Get<AIHeroClient>()) {
        if (!IsValidCollisionTarget(hero, input, range)) {
            continue;
        }

        AIBaseClient heroUnit(hero.Handle());
        PredictionInput heroInput = input;
        heroInput.Unit = heroUnit;
        const auto prediction = Prediction::Movement::GetPrediction(heroInput, false, false);
        const float radius = input.Radius + 50.0f + hero.BoundingRadius();

        if (DistanceSquaredToSegmentOnly(
                prediction.GetUnitPosition().To2D(),
                from2D,
                position2D) <= radius * radius) {
            AddIfUnique(result, hero);
        }
    }
}

inline void ProcessMinionList(std::vector<AIBaseClient>& result,
                              const std::vector<AIMinionClient>& minions,
                              const Vector3& position,
                              const PredictionInput& input,
                              int stationaryPadding) {
    const float range = CheckRange(input);
    const Vector3 from = input.ResolveFrom();
    const Vec2 from2D = from.To2D();
    const Vec2 position2D = position.To2D();

    for (const auto& minion : minions) {
        if (!IsValidMinionCollisionTarget(minion, input, range)) {
            continue;
        }

        AIBaseClient minionUnit(minion.Handle());
        PredictionInput minionInput = input;
        minionInput.Unit = minionUnit;
        const Vector3 livePosition = minion.Position();
        const Vector3 serverPosition = ServerPositionOrPosition(minionUnit);
        const Vector3 basePosition = livePosition.IsValid() && !livePosition.IsZero()
            ? livePosition
            : serverPosition;
        const float distanceFromStart = basePosition.Distance(from);

        if (WillDead(input, minionUnit, distanceFromStart)) {
            continue;
        }

        const auto minionPrediction = Prediction::Movement::GetPrediction(minionInput, false, false);
        Vector3 collisionPosition = minionPrediction.GetUnitPosition();
        if (!collisionPosition.IsValid() || collisionPosition.IsZero()) {
            collisionPosition = basePosition;
        }

        const float padding = minion.IsJungle()
            ? 20.0f
            : static_cast<float>(stationaryPadding);
        const float radius = input.Radius + padding + minion.BoundingRadius();

        if (DistanceSquaredToSegmentOnly(
                collisionPosition.To2D(),
                from2D,
                position2D) <= radius * radius) {
            AddIfUnique(result, minion);
        }
    }
}

inline std::vector<AIMinionClient> SnapshotCollisionMinions() {
    static std::vector<AIMinionClient> cached;
    static int lastRefreshTick = 0;

    const int now = Variables::TickCount();
    if (now - lastRefreshTick < 50 && !cached.empty()) {
        return cached;
    }

    std::vector<AIMinionClient> result;
    result.reserve(64);

    auto addUnique = [&](const AIMinionClient& minion) {
        if (!IsCollisionMinionCandidate(minion)) {
            return;
        }
        const int networkId = minion.NetworkId();
        const uintptr_t address = minion.Address();
        const auto exists = std::find_if(result.begin(), result.end(), [&](const AIMinionClient& entry) {
            if (!entry.IsValid()) {
                return false;
            }
            if (networkId != 0 && networkId != -1 && entry.NetworkId() == networkId) {
                return true;
            }
            return address != 0 && entry.Address() == address;
        });
        if (exists == result.end()) {
            result.push_back(minion);
        }
    };

    for (const auto& minion : SDK::ObjectManager::Get<AIMinionClient>()) {
        addUnique(minion);
    }

    for (const auto& minion : SDK::GameObjects::EnemyMinions()) {
        addUnique(minion);
    }
    for (const auto& minion : SDK::GameObjects::Jungle()) {
        addUnique(minion);
    }

    // If the typed minion manager/list is stale or incomplete, fall back to
    // the object array and classify there. This mirrors EnsoulSharp's
    // GameObjects source more closely while keeping the scan bounded/cached.
    if (result.size() < 12) {
        const auto player = SDK::ObjectManager::Player();
        const auto playerTeam = player.IsValid()
            ? player.Team()
            : GameObjectTeam::Unknown;
        for (const auto& object : SDK::ObjectManager::Get<GameObject>()) {
            if (!object.IsValid() || !object.IsMinion()) {
                continue;
            }
            const auto team = object.Team();
            if (playerTeam != GameObjectTeam::Unknown &&
                team == playerTeam) {
                continue;
            }
            addUnique(AIMinionClient(object.Handle()));
        }
    }

    cached = result;
    lastRefreshTick = now;
    return cached;
}

inline void ProcessBuildings(std::vector<AIBaseClient>& result,
                             const Vector3& position,
                             const PredictionInput& input) {
    const float range = CheckRange(input);
    const Vector3 from = input.ResolveFrom();
    const Vec2 position2D = position.To2D();
    const Vec2 from2D = from.To2D();

    for (const auto& turret : GameObjects::Turrets()) {
        if (!IsValidCollisionTarget(turret, input, range)) {
            continue;
        }

        const float radius = input.Radius + turret.BoundingRadius() + 50.0f;
        if (IsPointNearProjection(position2D, from2D, turret.Position().To2D(), radius)) {
            AddIfUnique(result, turret);
        }
    }

    for (const auto& inhibitor : GameObjects::EnemyInhibitors()) {
        if (!IsValidCollisionTarget(inhibitor, input, range)) {
            continue;
        }

        const float radius = input.Radius + inhibitor.BoundingRadius() + 50.0f;
        if (IsPointNearProjection(position2D, from2D, inhibitor.Position().To2D(), radius)) {
            // DLL adds GameObjects.Player for inhibitor/building sentinel.
            AddPlayerSentinel(result);
        }
    }
}

inline void ProcessWalls(std::vector<AIBaseClient>& result,
                         const Vector3& position,
                         const PredictionInput& input) {
    const Vector3 from = input.ResolveFrom();
    const float step = position.Distance(from) / 10.0f;

    for (int i = 0; i < 9; ++i) {
        const Vec2 sample = from.To2D().Extend(position.To2D(), step * static_cast<float>(i));
        if (HasFlag(NavMesh::GetCollisionFlags(sample.x, sample.y), CollisionFlags::Wall)) {
            AddPlayerSentinel(result);
            return;
        }
    }
}

inline void ProcessProjectileWalls(std::vector<AIBaseClient>& result,
                                   const Vector3& position,
                                   const PredictionInput& input) {
    const Vector3 from = input.ResolveFrom();

    if (ContainsCollisionObject(input, CollisionableObjects::YasuoWall) &&
        SegmentIntersectsWindwall(from, position, 350.0f, 0.0f)) {
        AddPlayerSentinel(result);
    }

    if ((ContainsCollisionObject(input, CollisionableObjects::YasuoWall) ||
         ContainsCollisionObject(input, CollisionableObjects::SamiraWall)) &&
        HasSamiraCollision(from, position, input.Radius)) {
        AddPlayerSentinel(result);
    }

    if ((ContainsCollisionObject(input, CollisionableObjects::YasuoWall) ||
         ContainsCollisionObject(input, CollisionableObjects::MelWall)) &&
        HasMelCollision(from, position, input.Radius)) {
        AddPlayerSentinel(result);
    }
}

} // namespace detail

inline void Initialize() {
    detail::Initialize();
}

// ============================================================================
// GetCollision — DLL-equivalent signature:
//   EnsoulSharp.SDK.Collisions.GetCollision(List<Vector3>, PredictionInput)
// ============================================================================
inline std::vector<AIBaseClient> GetCollision(const std::vector<Vector3>& positions,
                                              PredictionInput input) {
    Initialize();

    std::vector<AIBaseClient> result;
    std::vector<AIMinionClient> minions;
    if (detail::ContainsCollisionObject(input, CollisionableObjects::Minions)) {
        minions = detail::SnapshotCollisionMinions();
    }

    for (const auto& position : positions) {
        if (!position.IsValid() || position.IsZero()) {
            continue;
        }

        if (detail::ContainsCollisionObject(input, CollisionableObjects::Heroes)) {
            detail::ProcessHeroes(result, position, input);
            if (detail::ShouldStopCollisionScan(result, input)) {
                return result;
            }
        }

        if (detail::ContainsCollisionObject(input, CollisionableObjects::Minions)) {
            detail::ProcessMinionList(result, minions, position, input, 15);
            if (detail::ShouldStopCollisionScan(result, input)) {
                return result;
            }
        }

        if (detail::ContainsCollisionObject(input, CollisionableObjects::Building)) {
            detail::ProcessBuildings(result, position, input);
            if (detail::ShouldStopCollisionScan(result, input)) {
                return result;
            }
        }

        if (detail::ContainsCollisionObject(input, CollisionableObjects::Walls)) {
            detail::ProcessWalls(result, position, input);
            if (detail::ShouldStopCollisionScan(result, input)) {
                return result;
            }
        }

        detail::ProcessProjectileWalls(result, position, input);
        if (detail::ShouldStopCollisionScan(result, input)) {
            return result;
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
    Initialize();
    return detail::HasSamiraCollision(start, end, extraRadius);
}

inline bool HasMelWallCollision(const Vector3& start,
                                const Vector3& end,
                                float extraRadius = 0.0f) {
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
    Initialize();
    return detail::HasSamiraCollision(start, end, extraRadius) ||
           detail::HasMelCollision(start, end, extraRadius) ||
           detail::SegmentIntersectsWindwall(start, end, 250.0f, extraRadius);
}

inline bool IsCollision(const Vector3& position, float radius = 50.0f) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) {
        return false;
    }

    const Vector3 from = detail::ServerPositionOrPosition(player);
    return HasYasuoWindWallCollision(from, position, radius) ||
           HasSamiraWallCollision(from, position, radius) ||
           HasMelWallCollision(from, position, radius);
}

// ============================================================================
// WillDead — DLL private method exposed for parity/testing.
// ============================================================================
inline bool WillDead(const PredictionInput& input,
                     const AIBaseClient& minion,
                     float distance) {
    if (!minion.IsValid()) {
        return false;
    }

    HealthPrediction::Initialize();

    float time = input.Delay;
    if (std::abs(input.Speed - FLT_MAX) >= FLT_EPSILON && input.Speed > 0.0f) {
        time = (distance / input.Speed) + input.Delay;
    }

    const int predictionTime = static_cast<int>(time * 1000.0f) - Game::Ping();
    return HealthPrediction::GetPrediction(minion, predictionTime, 0) <= 0.0f;
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
    return IsCollision(std::vector<Vector3>{ to }, input);
}

} // namespace SDK::Collision

namespace SDK {
namespace Collisions = Collision;
} // namespace SDK
