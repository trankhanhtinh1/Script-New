#pragma once

// ============================================================================
// Collision.h — Collision detection for skillshots
// Ported from EnsoulSharp.SDK / Core/Math/Collision.cs + DLL additions
//
// DLL additions over source:
//   - HasYasuoWindWallCollision (with extraRadius overload)
//   - HasSamiraWallCollision (Samira W "Blade Whirl" projectile block)
//   - HasMelWallCollision (Mel W "Rebuttal" projectile block/reflect)
//   - IsCollision (check if position collides with wall/particle)
//   - WillDead (check if target will die from damage)
// ============================================================================

#include "../Core/Objects.h"
#include "../GameObjects/GameObjects.h"
#include "Prediction.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace SDK::Collision {

// ============================================================================
// HasYasuoWindWallCollision — check if skillshot path intersects Yasuo wall
// DLL: Collisions.HasYasuoWindWallCollision(from, to, extraRadius)
// ============================================================================
inline bool HasYasuoWindWallCollision(const Vector3& from,
                                       const Vector3& to,
                                       float extraRadius = 0.0f) {
    bool hasYasuo = false;
    for (const auto& hero : GameObjects::EnemyHeroes()) {
        if (hero.IsValid() && !hero.IsDead()
            && hero.CharacterName() == "Yasuo") {
            hasYasuo = true;
            break;
        }
    }
    if (!hasYasuo) return false;

    Vec2 from2D = from.To2D();
    Vec2 to2D = to.To2D();

    for (const auto& emitter : GameObjects::ParticleEmitters()) {
        if (!emitter.IsValid()) continue;
        const std::string name = emitter.Name();
        if (name.find("Yasuo") == std::string::npos
            || name.find("_w_windwall") == std::string::npos
            || name.find("enemy") == std::string::npos)
            continue;

        int level = 1;
        if (!name.empty()) {
            auto c = name.back();
            if (c >= '1' && c <= '9') level = c - '0';
        }
        float wallWidth = 250.0f + 50.0f * static_cast<float>(level) + extraRadius * 2.0f;
        Vec2 wallPos = emitter.Position().To2D();
        Vec2 wallDir = emitter.Direction().To2D();
        if (wallDir.LengthSqr() < 0.001f) continue;
        Vec2 perp(-wallDir.y, wallDir.x);
        perp = perp.Normalized();
        Vec2 wallStart = wallPos + perp * (wallWidth * 0.5f);
        Vec2 wallEnd = wallPos - perp * (wallWidth * 0.5f);

        auto inter = Vec2Ext::Intersection(from2D, to2D, wallStart, wallEnd);
        if (inter.Valid) return true;
    }
    return false;
}

// ============================================================================
// HasSamiraWallCollision — check if skillshot path intersects Samira W
// Samira's W "Blade Whirl" blocks projectiles in ~260 radius for ~1s
// ============================================================================
inline bool HasSamiraWallCollision(const Vector3& from,
                                    const Vector3& to,
                                    float extraRadius = 0.0f) {
    Vec2 from2D = from.To2D();
    Vec2 to2D = to.To2D();

    for (const auto& hero : GameObjects::EnemyHeroes()) {
        if (!hero.IsValid() || hero.IsDead()) continue;
        if (hero.CharacterName() != "Samira") continue;
        if (!hero.HasBuff("SamiraW") && !hero.HasBuff("SamiraWBuff"))
            continue;

        Vec2 samiraPos = hero.Position().To2D();
        auto proj = Vec2Ext::ProjectOn(samiraPos, from2D, to2D);
        float distToLine = proj.IsOnSegment
            ? samiraPos.Distance(proj.SegmentPoint)
            : std::min(samiraPos.Distance(from2D), samiraPos.Distance(to2D));
        if (distToLine <= 260.0f + extraRadius) return true;
    }
    return false;
}

// ============================================================================
// HasMelWallCollision — check if skillshot path intersects Mel W barrier
// Mel's W "Rebuttal" creates a 175-radius barrier that destroys/reflects
// projectiles for ~0.75s
// ============================================================================
inline bool HasMelWallCollision(const Vector3& from,
                                 const Vector3& to,
                                 float extraRadius = 0.0f) {
    Vec2 from2D = from.To2D();
    Vec2 to2D = to.To2D();

    for (const auto& hero : GameObjects::EnemyHeroes()) {
        if (!hero.IsValid() || hero.IsDead()) continue;
        if (hero.CharacterName() != "Mel") continue;
        if (!hero.HasBuff("MelW") && !hero.HasBuff("MelWBuff")
            && !hero.HasBuff("MelRebuttal"))
            continue;

        Vec2 melPos = hero.Position().To2D();
        auto proj = Vec2Ext::ProjectOn(melPos, from2D, to2D);
        float distToLine = proj.IsOnSegment
            ? melPos.Distance(proj.SegmentPoint)
            : std::min(melPos.Distance(from2D), melPos.Distance(to2D));
        if (distToLine <= 175.0f + extraRadius) return true;
    }
    return false;
}

// ============================================================================
// IsCollision — check if a position collides with any wall
// DLL: Collisions.IsCollision(position, radius)
// ============================================================================
inline bool IsCollision(const Vector3& position, float radius = 50.0f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const Vector3 from = player.Position();

    return HasYasuoWindWallCollision(from, position, radius)
        || HasSamiraWallCollision(from, position, radius)
        || HasMelWallCollision(from, position, radius);
}

// ============================================================================
// WillDead — check if target will die from damage
// DLL: Collisions.WillDead(target, damage)
// ============================================================================
inline bool WillDead(const AIBaseClient& target, float damage) {
    if (!target.IsValid() || target.IsDead()) return true;
    return target.Health() <= damage;
}

// ============================================================================
// HasLineCollision — check if line from→to collides with any object
// ============================================================================
inline bool HasLineCollision(const Vector3& from,
                             const Vector3& to,
                             float radius,
                             const GameObject& ignored = GameObject()) {
    const AIBaseClient ignoredUnit = ignored.IsValid() ? AIBaseClient(ignored.Address()) : AIBaseClient();
    auto collisions = Prediction::Movement::CollectLineCollisions(
        from,
        to,
        radius,
        ignoredUnit,
        CollisionableObjects::Minions | CollisionableObjects::YasuoWall
        | CollisionableObjects::BraumShield | CollisionableObjects::SamiraWall
        | CollisionableObjects::MelWall);
    if (!collisions.empty()) return true;

    return HasYasuoWindWallCollision(from, to, radius)
        || HasSamiraWallCollision(from, to, radius)
        || HasMelWallCollision(from, to, radius);
}

    inline std::vector<AIBaseClient> GetCollision(const AIBaseClient& target, const PredictionInput& input) {
        auto effectiveInput = input;
        effectiveInput.Unit = target;
        auto predOut = Prediction::GetPrediction(target, effectiveInput);
        return predOut.CollisionObjects;
    }

    inline std::vector<AIBaseClient> GetCollision(const std::vector<Vector3>& positions, const PredictionInput& input) {
        std::vector<AIBaseClient> result = {};
        std::vector<int> seenNetIds = {};

        const Vector3 source = Prediction::Movement::ResolveFrom(input);
        const auto addIfUnique = [&](const GameObject& obj) {
            if (!obj.IsValid()) {
                return;
            }
            const int netId = obj.NetworkId();
            if (std::find(seenNetIds.begin(), seenNetIds.end(), netId) != seenNetIds.end()) {
                return;
            }
            seenNetIds.push_back(netId);
            result.emplace_back(obj.Address());
        };

        for (const auto& position : positions) {
            if (position.IsZero()) {
                continue;
            }

            if (input.Collision && input.Type == SkillshotType::SkillshotLine) {
                const auto collisions = Prediction::Movement::CollectLineCollisions(
                    source,
                    position,
                    std::max(0.0f, input.Radius),
                    input.Unit,
                    input.CollisionObjects);
                for (const auto& obj : collisions) {
                    addIfUnique(obj);
                }
            }
        }

        return result;
    }

    inline bool HasCollision(const AIBaseClient& target, const PredictionInput& input) {
        return !GetCollision(target, input).empty();
    }

    inline bool HasCollision(const std::vector<Vector3>& positions, const PredictionInput& input) {
        return !GetCollision(positions, input).empty();
    }

} // namespace SDK::Collision
