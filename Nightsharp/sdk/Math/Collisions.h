#pragma once
#include "GameObjects.h"
#include "GameObject.h"
// (removed stray '/re' that was causing syntax errors)

#include "AiManager.h"
#include "core/Vector.h"
#include <vector>
#include <string>
#include <cmath>
#include <regex>

// ============================================================================
// Collisions — Skillshot collision detection utilities
// Reference: EnsoulSharp.SDK/Core/Math/Collision.cs
//
// Includes:
//   - Yasuo Wind Wall collision check
//   - Samira W (Blade Whirl) projectile block
//   - Mel W (Spell Shield / Inspiring Force) shield
//   - Braum E (Unbreakable) projectile block
//   - Minion collision (static + predicted)
//   - Hero collision (static + predicted)
//   - Wall/terrain collision (NavMesh step-check)
//   - Full GetCollision() returning list of blocking objects
// ============================================================================

namespace SDK {

// Forward declaration for Prediction (used in predicted collision)
struct PredictionInput;
struct PredictionResult;

namespace Collisions {

    // ====================================================================
    // Wind Wall–like ability names (projectile blockers)
    // ====================================================================
    // Yasuo W:  "YasuoWMovingWall" / "WindWall"
    // Samira W: "SamiraW" / "SamiraWBladeWhirl"
    // Braum E:  "BraumShieldRaise" / "BraumE"
    // Mel W:    "MelW" / "MelInspiring"
    // ====================================================================

    // Champions whose AAs are blocked by Wind Wall (most ranged champs)
    inline bool IsWindWallBlockedChampion(const std::string& champName) {
        static const char* notBlocked[] = {
            "Azir",    // Soldier attacks go through
            "Thresh",  // Flay auto is special
            nullptr
        };
        for (const char** p = notBlocked; *p; p++) {
            if (_stricmp(champName.c_str(), *p) == 0) return false;
        }
        return true;
    }

    // ====================================================================
    // Check for ANY projectile-blocking ability between two positions
    // (Yasuo W, Samira W, Braum E, Mel W)
    // ====================================================================
    inline bool HasProjectileBlockerCollision(const Vec3& from, const Vec3& to) {
        Vec2 lineStart = from.To2D();
        Vec2 lineEnd = to.To2D();

        // Patterns for projectile-blocking objects
        // Yasuo: YasuoWMovingWall, Yasuo_.*_w_windwall_enemy
        // Samira: SamiraW
        // Braum: BraumShieldRaise, BraumE
        // Mel: MelW, MelInspiring (Mel's W creates a zone that blocks projectiles)

        auto checkObjectList = [&](const std::vector<GameObject>& list) -> bool {
            for (auto& obj : list) {
                if (!obj.IsValid() || !obj.IsAlive()) continue;
                std::string name = obj.GetName();
                if (name.empty()) continue;

                float wallHalfWidth = 0.0f;
                bool isBlocker = false;

                // ----- Yasuo Wind Wall -----
                if (name.find("YasuoWMovingWall") != std::string::npos ||
                    name.find("WindWall") != std::string::npos) {
                    // Wall width scales: 250 + 50 * level (approx 300-450)
                    wallHalfWidth = 225.0f; // ~450/2 at max
                    isBlocker = true;
                }
                // ----- Samira W (Blade Whirl) -----
                // Creates a circular zone ~325 radius that destroys projectiles
                else if (name.find("SamiraW") != std::string::npos ||
                         name.find("BladeWhirl") != std::string::npos) {
                    // Samira W is circular, radius ~325
                    Vec2 objPos = obj.GetPosition().To2D();
                    float samiraRadius = 325.0f;
                    if (Geometry::LineCircleIntersects(lineStart, lineEnd, objPos, samiraRadius)) {
                        return true;
                    }
                    continue;
                }
                // ----- Braum E (Unbreakable) -----
                // Shield in front of Braum, blocks projectiles in a cone
                else if (name.find("BraumShield") != std::string::npos ||
                         name.find("BraumE") != std::string::npos) {
                    wallHalfWidth = 175.0f; // Shield width
                    isBlocker = true;
                }
                // ----- Mel W (Inspiring Force) -----
                // Creates a shield zone ~300 radius
                else if (name.find("MelW") != std::string::npos ||
                         name.find("MelInspiring") != std::string::npos) {
                    Vec2 objPos = obj.GetPosition().To2D();
                    float melRadius = 300.0f;
                    if (Geometry::LineCircleIntersects(lineStart, lineEnd, objPos, melRadius)) {
                        return true;
                    }
                    continue;
                }

                if (isBlocker) {
                    Vec2 wallCenter = obj.GetPosition().To2D();
                    float dist = Geometry::PointToSegmentDistance(wallCenter, lineStart, lineEnd);
                    if (dist <= wallHalfWidth + 50.0f) {
                        return true;
                    }
                }
            }
            return false;
        };

        if (checkObjectList(GameObjects::AllMinions)) return true;
        if (checkObjectList(GameObjects::Pets)) return true;
        return false;
    }

    // ====================================================================
    // Yasuo Wind Wall only (backwards compatible)
    // ====================================================================
    inline bool HasYasuoWindWallCollision(const Vec3& from, const Vec3& to) {
        Vec2 lineStart = from.To2D();
        Vec2 lineEnd = to.To2D();

        auto check = [&](const std::vector<GameObject>& list) -> bool {
            for (auto& obj : list) {
                if (!obj.IsValid() || !obj.IsAlive()) continue;
                std::string name = obj.GetName();
                if (name.find("YasuoWMovingWall") == std::string::npos &&
                    name.find("WindWall") == std::string::npos)
                    continue;

                Vec2 wallCenter = obj.GetPosition().To2D();
                float wallHalfWidth = 225.0f;
                float dist = Geometry::PointToSegmentDistance(wallCenter, lineStart, lineEnd);
                if (dist <= wallHalfWidth + 50.0f) return true;
            }
            return false;
        };

        if (check(GameObjects::AllMinions)) return true;
        if (check(GameObjects::Pets)) return true;
        return false;
    }

    // ====================================================================
    // Check if there's a blocking ability specifically for the given champ
    // ====================================================================
    inline bool HasEnemyProjectileBlocker(const Vec3& from, const Vec3& to) {
        // Check if any enemy hero has an active projectile-blocking ability
        // between from→to

        // First check static objects (wind wall, etc.)
        if (HasProjectileBlockerCollision(from, to)) return true;

        // Then check enemy hero buff states
        Vec2 lineStart = from.To2D();
        Vec2 lineEnd = to.To2D();

        for (auto& hero : GameObjects::EnemyHeroes) {
            if (!hero.IsValid() || !hero.IsAlive()) continue;
            std::string champ = hero.GetChampionName();

            // Samira: Check if she's in W animation (has "SamiraW" buff)
            if (champ == "Samira") {
                BuffManager buffs(hero.address);
                if (buffs.HasBuff("SamiraW") || buffs.HasBuff("samiraW")) {
                    Vec2 heroPos = hero.GetPosition().To2D();
                    if (Geometry::LineCircleIntersects(lineStart, lineEnd, heroPos, 325.0f)) {
                        return true;
                    }
                }
            }

            // Braum: Check if he's holding shield (has "BraumShieldRaise" buff)
            if (champ == "Braum") {
                BuffManager buffs(hero.address);
                if (buffs.HasBuff("BraumShieldRaise") || buffs.HasBuff("braumeshieldbuff")) {
                    Vec2 heroPos = hero.GetPosition().To2D();
                    // Braum shield blocks in a ~120° cone in front of him
                    float dist = Geometry::PointToSegmentDistance(heroPos, lineStart, lineEnd);
                    if (dist <= 200.0f) {
                        return true;
                    }
                }
            }

            // Mel: Check if W zone is active
            if (champ == "Mel") {
                BuffManager buffs(hero.address);
                if (buffs.HasBuff("MelW") || buffs.HasBuff("MelInspiringForce")) {
                    Vec2 heroPos = hero.GetPosition().To2D();
                    if (Geometry::LineCircleIntersects(lineStart, lineEnd, heroPos, 300.0f)) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    // ====================================================================
    // AA blocked by wind wall–like abilities
    // ====================================================================
    inline bool IsAutoAttackBlockedByWindWall(const GameObject& source, const GameObject& target) {
        if (source.IsMelee()) return false;

        std::string champName = source.GetChampionName();
        if (!champName.empty() && !IsWindWallBlockedChampion(champName))
            return false;

        return HasProjectileBlockerCollision(source.GetPosition(), target.GetPosition());
    }

    // ====================================================================
    // Minion Collision — Static (no prediction)
    // ====================================================================
    inline bool IsPointForwardOnSegment(const Vec2& point, const Vec2& segStart, const Vec2& segEnd) {
        Vec2 seg = segEnd - segStart;
        if (seg.LengthSqr() <= 0.0001f) {
            return false;
        }
        return (point - segStart).Dot(seg) > 0.0f;
    }

    inline bool HasMinionCollision(const Vec3& from, const Vec3& to, float width,
                                    const std::vector<GameObject>& ignore = {}) {
        Vec2 lineStart = from.To2D();
        Vec2 lineEnd = to.To2D();
        const float collisionRadius = (width > 0.0f) ? width : 0.0f;
        const Vec2 lineDir = lineEnd - lineStart;
        if (lineDir.LengthSqr() <= 0.0001f) {
            return false;
        }

        auto checkList = [&](const std::vector<GameObject>& list) -> bool {
            for (auto& obj : list) {
                if (!obj.IsValid() || !obj.IsAlive()) continue;
                bool skip = false;
                for (auto& ig : ignore) {
                    if (ig.address == obj.address) { skip = true; break; }
                }
                if (skip) continue;

                Vec2 objPos = obj.GetPosition().To2D();
                if (!IsPointForwardOnSegment(objPos, lineStart, lineEnd)) continue;
                if (objPos.Distance(lineEnd) <= obj.GetBoundingRadius() + 10.0f) continue;

                float hitbox = obj.GetBoundingRadius() + collisionRadius + 15.0f;
                float dist = Geometry::PointToSegmentDistance(objPos, lineStart, lineEnd);
                if (dist <= hitbox) return true;
            }
            return false;
        };

        if (checkList(GameObjects::EnemyMinions)) return true;
        if (checkList(GameObjects::JungleMinions)) return true;
        return false;
    }

    // ====================================================================
    // Minion Collision — With Prediction (moving minions)
    // ====================================================================
    inline bool HasMinionCollisionPredicted(const Vec3& from, const Vec3& to,
                                             float width, float speed, float delay) {
        Vec2 lineStart = from.To2D();
        Vec2 lineEnd = to.To2D();
        const float collisionRadius = (width > 0.0f) ? width : 0.0f;
        const Vec2 lineDir = lineEnd - lineStart;
        if (lineDir.LengthSqr() <= 0.0001f) {
            return false;
        }

        auto checkList = [&](const std::vector<GameObject>& list) -> bool {
            for (auto& obj : list) {
                if (!obj.IsValid() || !obj.IsAlive()) continue;

                // Estimate when the skillshot would reach this minion
                Vec2 minionPos = obj.GetPosition().To2D();
                if (!IsPointForwardOnSegment(minionPos, lineStart, lineEnd)) continue;
                Vec2 closest = Geometry::ClosestPointOnSegment(minionPos, lineStart, lineEnd);
                float distToLine = closest.Distance(lineStart);

                float timeToReach = delay;
                if (speed > 0.0f) timeToReach += distToLine / speed;

                // Predict minion position at that time
                AiManager ai(obj.address);
                Vec3 predicted;
                if (ai.IsValid() && ai.IsMoving()) {
                    predicted = ai.PredictPositionPath(timeToReach, obj.GetMoveSpeed());
                } else {
                    predicted = obj.GetPosition();
                }

                Vec2 predPos = predicted.To2D();
                if (!IsPointForwardOnSegment(predPos, lineStart, lineEnd)) continue;
                if (predPos.Distance(lineEnd) <= obj.GetBoundingRadius() + 10.0f) continue;

                float hitbox = obj.GetBoundingRadius() + collisionRadius + 15.0f;
                float dist = Geometry::PointToSegmentDistance(predPos, lineStart, lineEnd);
                if (dist <= hitbox) return true;
            }
            return false;
        };

        if (checkList(GameObjects::EnemyMinions)) return true;
        if (checkList(GameObjects::JungleMinions)) return true;
        return false;
    }

    // ====================================================================
    // Hero Collision — Static
    // ====================================================================
    inline bool HasHeroCollision(const Vec3& from, const Vec3& to, float width,
                                  const GameObject& excludeTarget = GameObject()) {
        Vec2 lineStart = from.To2D();
        Vec2 lineEnd = to.To2D();
        const float collisionRadius = (width > 0.0f) ? width : 0.0f;
        const Vec2 lineDir = lineEnd - lineStart;
        if (lineDir.LengthSqr() <= 0.0001f) {
            return false;
        }

        for (auto& hero : GameObjects::EnemyHeroes) {
            if (!hero.IsValid() || !hero.IsAlive()) continue;
            if (hero.address == excludeTarget.address) continue;

            Vec2 heroPos = hero.GetPosition().To2D();
            if (!IsPointForwardOnSegment(heroPos, lineStart, lineEnd)) continue;

            float hitbox = hero.GetBoundingRadius() + collisionRadius + 10.0f;
            float dist = Geometry::PointToSegmentDistance(heroPos, lineStart, lineEnd);
            if (dist <= hitbox) return true;
        }
        return false;
    }

    // ====================================================================
    // Hero Collision — With Prediction
    // ====================================================================
    inline bool HasHeroCollisionPredicted(const Vec3& from, const Vec3& to,
                                           float width, float speed, float delay,
                                           const GameObject& excludeTarget = GameObject()) {
        Vec2 lineStart = from.To2D();
        Vec2 lineEnd = to.To2D();
        const float collisionRadius = (width > 0.0f) ? width : 0.0f;
        const Vec2 lineDir = lineEnd - lineStart;
        if (lineDir.LengthSqr() <= 0.0001f) {
            return false;
        }

        for (auto& hero : GameObjects::EnemyHeroes) {
            if (!hero.IsValid() || !hero.IsAlive()) continue;
            if (hero.address == excludeTarget.address) continue;

            Vec2 heroPos = hero.GetPosition().To2D();
            if (!IsPointForwardOnSegment(heroPos, lineStart, lineEnd)) continue;
            Vec2 closest = Geometry::ClosestPointOnSegment(heroPos, lineStart, lineEnd);
            float distToLine = closest.Distance(lineStart);

            float timeToReach = delay;
            if (speed > 0.0f) timeToReach += distToLine / speed;

            AiManager ai(hero.address);
            Vec3 predicted;
            if (ai.IsValid() && ai.IsMoving()) {
                predicted = ai.PredictPositionPath(timeToReach, hero.GetMoveSpeed());
            } else {
                predicted = hero.GetPosition();
            }

            Vec2 predPos = predicted.To2D();
            if (!IsPointForwardOnSegment(predPos, lineStart, lineEnd)) continue;

            float hitbox = hero.GetBoundingRadius() + collisionRadius + 10.0f;
            float dist = Geometry::PointToSegmentDistance(predPos, lineStart, lineEnd);
            if (dist <= hitbox) return true;
        }
        return false;
    }

    // ====================================================================
    // Wall/Terrain Collision
    // Samples points along the line and checks NavMesh collision flags.
    // Note: Requires NavMesh access — currently approximates with range check.
    // ====================================================================
    inline bool HasWallCollision(const Vec3& from, const Vec3& to, int sampleCount = 20) {
        // Without NavMesh API, we can use a heuristic:
        // Very long skillshots that go far from the map center likely hit walls.
        // This is a placeholder — full implementation needs NavMesh::GetCollisionFlags.

        // For now, check if the line exceeds map boundaries
        auto navGrid = SDK::NavGrid::Get();
        if (!navGrid.IsValid()) {
            return false;
        }

        const float distance = from.Distance2D(to);
        float stepSize = distance / static_cast<float>((sampleCount > 0) ? sampleCount : 20);
        if (stepSize < 15.0f) {
            stepSize = 15.0f;
        } else if (stepSize > 60.0f) {
            stepSize = 60.0f;
        }

        return navGrid.IsWallBetween(from, to, stepSize);
    }

    // ====================================================================
    // Comprehensive GetCollision — Returns list of all blocking objects
    // Reference: EnsoulSharp.SDK/Core/Math/Collision.cs
    // ====================================================================
    enum CollisionCheckFlags {
        CheckMinions      = (1 << 0),
        CheckHeroes       = (1 << 1),
        CheckWalls        = (1 << 2),
        CheckYasuoWall    = (1 << 3),
        CheckProjectileBlockers = (1 << 4),  // Yasuo + Samira + Braum + Mel

        CheckAll = CheckMinions | CheckHeroes | CheckWalls | CheckProjectileBlockers
    };

    struct CollisionResult {
        bool HasCollision = false;
        std::vector<GameObject> CollidingObjects;

        // Convenience
        bool CollidesWithMinion() const {
            for (auto& o : CollidingObjects) {
                if (!o.IsHero()) return true;
            }
            return false;
        }
        bool CollidesWithHero() const {
            for (auto& o : CollidingObjects) {
                if (o.IsHero()) return true;
            }
            return false;
        }
    };

    inline CollisionResult GetCollision(const Vec3& from, const Vec3& to,
                                         float width, float speed = 0.0f,
                                         float delay = 0.25f,
                                         int flags = CheckAll,
                                         const GameObject& excludeTarget = GameObject()) {
        CollisionResult result;

        // -- Projectile blockers (Yasuo W, Samira W, Braum E, Mel W) --
        if (flags & CheckProjectileBlockers) {
            if (HasProjectileBlockerCollision(from, to)) {
                result.HasCollision = true;
                // We can't easily identify which specific object, so mark collision
                return result;
            }
            if (HasEnemyProjectileBlocker(from, to)) {
                result.HasCollision = true;
                return result;
            }
        } else if (flags & CheckYasuoWall) {
            if (HasYasuoWindWallCollision(from, to)) {
                result.HasCollision = true;
                return result;
            }
        }

        Vec2 lineStart = from.To2D();
        Vec2 lineEnd = to.To2D();
        const Vec2 lineDir = lineEnd - lineStart;
        if (lineDir.LengthSqr() <= 0.0001f) {
            return result;
        }

        // -- Minion collision --
        if (flags & CheckMinions) {
            auto checkMinions = [&](const std::vector<GameObject>& list) {
                for (auto& obj : list) {
                    if (!obj.IsValid() || !obj.IsAlive()) continue;
                    if (obj.address == excludeTarget.address) continue;

                    Vec3 predicted = obj.GetPosition();
                    if (speed > 0.0f) {
                        AiManager ai(obj.address);
                        if (ai.IsValid() && ai.IsMoving()) {
                            Vec2 minionPos = obj.GetPosition().To2D();
                            Vec2 closest = Geometry::ClosestPointOnSegment(minionPos, lineStart, lineEnd);
                            float distOnLine = closest.Distance(lineStart);
                            float t = delay + distOnLine / speed;
                            predicted = ai.PredictPositionPath(t, obj.GetMoveSpeed());
                        }
                    }

                    Vec2 predPos = predicted.To2D();
                    if (!IsPointForwardOnSegment(predPos, lineStart, lineEnd)) continue;

                    float hitbox = obj.GetBoundingRadius() + width + 10.0f;
                    if (Geometry::PointToSegmentDistance(predPos, lineStart, lineEnd) <= hitbox) {
                        result.CollidingObjects.push_back(obj);
                    }
                }
            };
            checkMinions(GameObjects::EnemyMinions);
            checkMinions(GameObjects::JungleMinions);
        }

        // -- Hero collision --
        if (flags & CheckHeroes) {
            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive()) continue;
                if (hero.address == excludeTarget.address) continue;

                Vec3 predicted = hero.GetPosition();
                if (speed > 0.0f) {
                    AiManager ai(hero.address);
                    if (ai.IsValid() && ai.IsMoving()) {
                        Vec2 heroPos = hero.GetPosition().To2D();
                        Vec2 closest = Geometry::ClosestPointOnSegment(heroPos, lineStart, lineEnd);
                        float distOnLine = closest.Distance(lineStart);
                        float t = delay + distOnLine / speed;
                        predicted = ai.PredictPositionPath(t, hero.GetMoveSpeed());
                    }
                }

                Vec2 predPos = predicted.To2D();
                if (!IsPointForwardOnSegment(predPos, lineStart, lineEnd)) continue;

                float hitbox = hero.GetBoundingRadius() + width + 10.0f;
                if (Geometry::PointToSegmentDistance(predPos, lineStart, lineEnd) <= hitbox) {
                    result.CollidingObjects.push_back(hero);
                }
            }
        }

        // -- Wall collision --
        if (flags & CheckWalls) {
            if (HasWallCollision(from, to)) {
                result.HasCollision = true;
            }
        }

        if (!result.CollidingObjects.empty()) {
            result.HasCollision = true;
        }
        return result;
    }

} // namespace Collisions
} // namespace SDK
