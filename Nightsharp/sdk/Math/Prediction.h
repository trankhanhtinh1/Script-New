#pragma once
#include "GameObject.h"
#include "AiManager.h"
#include "BuffManager.h"
#include "GameObjects.h"
#include "Enums.h"
#include "GamePath.h"
#include "HealthPrediction.h"
#include "Collisions.h"
#include <cmath>

// ============================================================================
// Prediction - Movement prediction for skillshots
// Reference: EnsoulSharp.SDK/Core/Math/Prediction/
//
// Integrates:
//   - GamePath::PathTracker for juking detection / HitChance refinement
//   - Cluster (separate file) for AoE multi-target prediction
//   - Advanced Collision via Collisions.h
// ============================================================================

namespace SDK {

    // ========================================================================
    // Prediction Result
    // ========================================================================
    struct PredictionResult {
        Vec3 CastPosition;      // Where to cast
        Vec3 UnitPosition;      // Predicted unit position
        HitChance Hitchance;    // Estimated hit chance
        int AoEHitCount;        // Number of targets hit (AoE)

        PredictionResult()
            : CastPosition(), UnitPosition(), Hitchance(HitChance::None), AoEHitCount(0) {}
    };

    // ========================================================================
    // Prediction Input
    // ========================================================================
    struct PredictionInput {
        Vec3 From;              // Source position
        Vec3 RangeCheckFrom;    // Range check origin (if different from From)
        float Range;
        float Speed;            // Missile speed (0 = instant)
        float Delay;            // Cast delay (seconds)
        float Width;            // Skillshot width/radius
        float Radius;           // Alias for Width (circle AoE)
        SkillshotType Type;
        bool CollisionCheck;    // Check for minion collision
        int CollisionFlags;     // CollisionCheckFlags bitmask

        bool Aoe;               // AoE spell (for cluster prediction)

        PredictionInput()
            : From(), RangeCheckFrom(), Range(0), Speed(0), Delay(0.25f), Width(0), Radius(0),
              Type(SkillshotType::Line), CollisionCheck(false), CollisionFlags(0), Aoe(false) {}

        float RealRadius() const { return Radius > 0 ? Radius : Width; }
    };

    // ========================================================================
    // Prediction Engine
    // ========================================================================
    class Prediction {
    public:
        // ====================================================================
        // Get predicted position for a target
        // ====================================================================
        static PredictionResult GetPrediction(const GameObject& target,
                                               const PredictionInput& input) {
            PredictionResult result;

            if (!target.IsValid() || !target.IsAlive()) {
                result.Hitchance = HitChance::Impossible;
                return result;
            }

            Vec3 from = input.From.IsZero() ? GameObjects::Player.GetPosition() : input.From;
            Vec3 rangeCheckFrom = input.RangeCheckFrom.IsZero() ? from : input.RangeCheckFrom;
            const float spellRadius = input.RealRadius();
            const float realRadius = spellRadius + target.GetBoundingRadius();
            auto finalizePrediction = [&](PredictionResult currentResult) -> PredictionResult {
                if (input.Range > 0.0f) {
                    if ((int)currentResult.Hitchance >= (int)HitChance::High) {
                        const float edgeRange = input.Range + realRadius * 0.75f;
                        if (rangeCheckFrom.DistanceSqr2D(target.GetPosition()) > edgeRange * edgeRange) {
                            currentResult.Hitchance = HitChance::Medium;
                        }
                    }

                    const float extraRange = (input.Type == SkillshotType::Circle) ? realRadius : 0.0f;
                    const float maxUnitRange = input.Range + extraRange;
                    if (rangeCheckFrom.DistanceSqr2D(currentResult.UnitPosition) > maxUnitRange * maxUnitRange) {
                        currentResult.Hitchance = HitChance::OutOfRange;
                        return currentResult;
                    }

                    if (rangeCheckFrom.DistanceSqr2D(currentResult.CastPosition) > input.Range * input.Range) {
                        if (currentResult.Hitchance != HitChance::OutOfRange) {
                            Vec3 dir = (currentResult.UnitPosition - rangeCheckFrom).Normalized2D();
                            currentResult.CastPosition = rangeCheckFrom + dir * input.Range;
                        } else {
                            return currentResult;
                        }
                    }
                }

                if (input.CollisionCheck && (int)currentResult.Hitchance >= (int)HitChance::Medium) {
                    int flags = 0;
                    if (input.CollisionFlags & CollisionMinions)     flags |= Collisions::CheckMinions;
                    if (input.CollisionFlags & CollisionHeroes)      flags |= Collisions::CheckHeroes;
                    if (input.CollisionFlags & CollisionWalls)       flags |= Collisions::CheckWalls;
                    if (input.CollisionFlags & CollisionYasuoWall)   flags |= Collisions::CheckProjectileBlockers;
                    if (input.CollisionFlags & CollisionBraumShield) flags |= Collisions::CheckProjectileBlockers;

                    if (flags != 0) {
                        auto collision = Collisions::GetCollision(
                            from,
                            currentResult.CastPosition,
                            spellRadius,
                            input.Speed,
                            input.Delay,
                            flags,
                            target);
                        if (collision.HasCollision) {
                            currentResult.Hitchance = HitChance::Collision;
                        }
                    }
                }

                return currentResult;
            };

            // Check range
            float distToTarget = from.Distance2D(target.GetPosition());
            if (input.Range > 0.0f) {
                const float maxRange = input.Range * 1.5f;
                if (rangeCheckFrom.DistanceSqr2D(target.GetPosition()) > maxRange * maxRange) {
                    result.Hitchance = HitChance::OutOfRange;
                    return result;
                }
            } else if (distToTarget > input.Range + spellRadius) {
                result.Hitchance = HitChance::OutOfRange;
                return result;
            }

            AiManager ai(target.address);
            BuffManager buffs(target.address);

            // Calculate total delay (cast delay + travel time)
            float totalDelay = input.Delay;
            if (input.Speed > 0) {
                totalDelay += distToTarget / input.Speed;
            }

            // ================================================================
            // Immobile check (CC'd targets)
            // ================================================================
            if (buffs.IsImmobile()) {
                result.CastPosition = target.GetPosition();
                result.UnitPosition = target.GetPosition();
                result.Hitchance = HitChance::Immobile;
                return finalizePrediction(result);
            }

            // ================================================================
            // Dashing check
            // ================================================================
            if (ai.IsDashing()) {
                Vec3 dashEnd = ai.GetPathEnd();
                float dashSpeed = ai.GetDashSpeed();
                if (dashSpeed > 0) {
                    Vec3 dashDir = dashEnd - target.GetPosition();
                    float dashDist = dashDir.Length2D();
                    float timeToDashEnd = dashDist / dashSpeed;

                    if (timeToDashEnd <= totalDelay) {
                        // Target will reach dash end before spell arrives
                        result.CastPosition = dashEnd;
                        result.UnitPosition = dashEnd;
                        result.Hitchance = HitChance::Dashing;
                        return finalizePrediction(result);
                    }
                }
                // Predict along dash path
                Vec3 predicted = ai.PredictPosition(totalDelay, dashSpeed);
                result.CastPosition = predicted;
                result.UnitPosition = predicted;
                result.Hitchance = HitChance::Dashing;
                return finalizePrediction(result);
            }

            // ================================================================
            // Not moving - high chance
            // ================================================================
            if (!ai.IsMoving()) {
                result.CastPosition = target.GetPosition();
                result.UnitPosition = target.GetPosition();
                result.Hitchance = HitChance::VeryHigh;
                return finalizePrediction(result);
            }

            // ================================================================
            // Moving - predict position along path
            // ================================================================
            float moveSpeed = target.GetMoveSpeed();
            Vec3 predicted = ai.PredictPositionPath(totalDelay, moveSpeed);

            // Update distance for iterative prediction
            float newDist = from.Distance2D(predicted);
            if (input.Speed > 0) {
                // Iterative prediction (2 iterations)
                for (int i = 0; i < 2; i++) {
                    float newDelay = input.Delay + newDist / input.Speed;
                    predicted = ai.PredictPositionPath(newDelay, moveSpeed);
                    newDist = from.Distance2D(predicted);
                }
            }

            result.CastPosition = predicted;
            result.UnitPosition = predicted;

            // Check if predicted position is in range
            if (input.Range > 0.0f && newDist > input.Range + spellRadius) {
                result.Hitchance = HitChance::OutOfRange;
                return result;
            }

            // ================================================================
            // Estimate hit chance based on angle change + path history
            // ================================================================
            Vec3 dir1 = target.GetPosition() - from;
            Vec3 dir2 = predicted - from;
            float dot = dir1.x * dir2.x + dir1.z * dir2.z;
            float len1 = dir1.Length2D();
            float len2 = dir2.Length2D();
            float cosAngle = (len1 > 0 && len2 > 0) ? dot / (len1 * len2) : 1.0f;

            // Movement perpendicular to cast direction = harder to predict
            float moveDist = moveSpeed * totalDelay;
            float hitbox = realRadius;

            if (totalDelay < 0.25f) {
                result.Hitchance = HitChance::VeryHigh;
            } else if (moveDist < hitbox * 0.5f) {
                result.Hitchance = HitChance::VeryHigh;
            } else if (cosAngle > 0.95f) {
                result.Hitchance = HitChance::High;
            } else if (cosAngle > 0.7f) {
                result.Hitchance = HitChance::Medium;
            } else {
                result.Hitchance = HitChance::Low;
            }

            // ================================================================
            // Refine HitChance using PathTracker (juking detection)
            // ================================================================
            if (target.IsHero()) {
                // If hero is juking (many path changes), reduce confidence
                int pathChanges = GamePath::PathTracker::GetPathChangeCount(target, 1.0f);
                float angleVar = GamePath::PathTracker::GetPathAngleVariance(target, 1.0f);

                if (pathChanges >= 4 && angleVar > 0.8f) {
                    // Very erratic movement - strongly reduce
                    if ((int)result.Hitchance > (int)HitChance::Low)
                        result.Hitchance = HitChance::Low;
                } else if (pathChanges >= 3 && angleVar > 0.5f) {
                    // Moderate juking - reduce by one level
                    if ((int)result.Hitchance > (int)HitChance::Medium)
                        result.Hitchance = HitChance::Medium;
                } else if (pathChanges <= 1) {
                    // Steady movement - slightly boost
                    if (result.Hitchance == HitChance::Medium)
                        result.Hitchance = HitChance::High;
                }

                // Use mean speed for more accurate prediction
                float meanSpeed = GamePath::PathTracker::GetMeanSpeed(target, 1.0f);
                if (meanSpeed < moveSpeed * 0.3f) {
                    // Hero is effectively standing still (stuttering)
                    if ((int)result.Hitchance < (int)HitChance::High)
                        result.Hitchance = HitChance::High;
                }
            }

            return finalizePrediction(result);
        }

        // ====================================================================
        // Simplified prediction API
        // ====================================================================
        static PredictionResult GetPrediction(const GameObject& target,
                                               float range, float speed,
                                               float delay, float width,
                                               SkillshotType type = SkillshotType::Line) {
            PredictionInput input;
            input.Range = range;
            input.Speed = speed;
            input.Delay = delay;
            input.Width = width;
            input.Type = type;
            return GetPrediction(target, input);
        }

        // ====================================================================
        // Quick: Will hit?
        // ====================================================================
        static bool WillHit(const GameObject& target, const PredictionInput& input,
                           HitChance minChance = HitChance::High) {
            auto result = GetPrediction(target, input);
            return (int)result.Hitchance >= (int)minChance;
        }

        // ====================================================================
        // Collision check - static (no prediction)
        // ====================================================================
        static bool HasCollision(const Vec3& from, const Vec3& to, float width) {
            return Collisions::HasMinionCollision(from, to, width);
        }

        // ====================================================================
        // Collision check - with prediction (moving minions/heroes)
        // ====================================================================
        static bool HasCollisionPredicted(const Vec3& from, const Vec3& to,
                                           float width, float speed, float delay) {
            return Collisions::HasMinionCollisionPredicted(from, to, width, speed, delay);
        }

        // ====================================================================
        // Projectile blocker check (Yasuo W, Samira W, Braum E, Mel W)
        // ====================================================================
        static bool HasProjectileBlockerCollision(const Vec3& from, const Vec3& to) {
            return Collisions::HasProjectileBlockerCollision(from, to);
        }

        // ====================================================================
        // Yasuo WindWall only (backwards compatible)
        // ====================================================================
        static bool HasYasuoWindWallCollision(const Vec3& from, const Vec3& to) {
            return Collisions::HasYasuoWindWallCollision(from, to);
        }

        // ====================================================================
        // Full collision check - returns detailed result
        // ====================================================================
        static Collisions::CollisionResult GetCollision(const Vec3& from, const Vec3& to,
                                                         float width, float speed = 0.0f,
                                                         float delay = 0.25f,
                                                         int flags = Collisions::CheckAll,
                                                         const GameObject& exclude = GameObject()) {
            return Collisions::GetCollision(from, to, width, speed, delay, flags, exclude);
        }

        // ====================================================================
        // AoE prediction - Simple version (backwards compatible)
        // For advanced AoE, use SDK::Cluster::GetAoEPrediction() directly
        // ====================================================================
        static PredictionResult GetAoEPrediction(const PredictionInput& input,
                                                  float aoeRadius) {
            PredictionResult bestResult;
            bestResult.AoEHitCount = 0;

            Vec3 from = input.From.IsZero() ? GameObjects::Player.GetPosition() : input.From;

            struct PredTarget {
                GameObject obj;
                Vec3 predictedPos;
            };
            std::vector<PredTarget> targets;

            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) continue;
                float dist = from.Distance2D(hero.GetPosition());
                if (dist > input.Range + aoeRadius) continue;

                auto pred = GetPrediction(hero, input);
                if ((int)pred.Hitchance >= (int)HitChance::Low) {
                    targets.push_back({ hero, pred.CastPosition });
                }
            }

            if (targets.empty()) return bestResult;
            if (targets.size() == 1) {
                auto pred = GetPrediction(targets[0].obj, input);
                pred.AoEHitCount = 1;
                return pred;
            }

            for (auto& t : targets) {
                Vec3 castPos = t.predictedPos;
                if (from.Distance2D(castPos) > input.Range) continue;

                int hitCount = 0;
                for (auto& other : targets) {
                    if (castPos.Distance2D(other.predictedPos) <= aoeRadius + other.obj.GetBoundingRadius())
                        hitCount++;
                }

                if (hitCount > bestResult.AoEHitCount) {
                    bestResult.CastPosition = castPos;
                    bestResult.UnitPosition = t.predictedPos;
                    bestResult.AoEHitCount = hitCount;
                    bestResult.Hitchance = HitChance::High;
                }
            }

            return bestResult;
        }

        // ====================================================================
        // Advanced AoE prediction - delegates to Cluster
        // Must include Cluster.h AFTER Prediction.h in SDK.h
        // Usage: auto result = Prediction::GetAdvancedAoEPrediction(input, target);
        // ====================================================================
        // (Cluster::GetAoEPrediction is available directly - see Cluster.h)

        // ====================================================================
        // Get predicted health of target at arrival time of spell
        // Useful for execute spells (Cho R, Garen R, etc.)
        // Uses HealthPrediction for accurate incoming damage tracking
        // ====================================================================
        static float GetPredictedHealth(const GameObject& target,
                                         float delay, float speed,
                                         const Vec3& from = Vec3()) {
            Vec3 origin = from.IsZero() ? GameObjects::Player.GetPosition() : from;
            float dist = origin.Distance2D(target.GetPosition());
            float totalDelay = delay;
            if (speed > 0) totalDelay += dist / speed;
            float totalDelayMs = totalDelay * 1000.0f;
            return HealthPrediction::GetPrediction(target, totalDelayMs);
        }
    };

} // namespace SDK
