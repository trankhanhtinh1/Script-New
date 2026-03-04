#pragma once
#include "GameObject.h"
#include "AiManager.h"
#include "BuffManager.h"
#include "GameObjects.h"
#include "Enums.h"
#include <cmath>

// ============================================================================
// Prediction — Movement prediction for skillshots
// Reference: EnsoulSharp.SDK/Core/Math/Prediction/
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
        float Range;
        float Speed;            // Missile speed (0 = instant)
        float Delay;            // Cast delay (seconds)
        float Width;            // Skillshot width/radius
        SkillshotType Type;
        bool CollisionCheck;    // Check for minion collision

        PredictionInput()
            : From(), Range(0), Speed(0), Delay(0.25f), Width(0),
              Type(SkillshotType::Line), CollisionCheck(false) {}
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

            // Check range
            float distToTarget = from.Distance2D(target.GetPosition());
            if (distToTarget > input.Range + input.Width) {
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
                return result;
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
                        return result;
                    }
                }
                // Predict along dash path
                Vec3 predicted = ai.PredictPosition(totalDelay, dashSpeed);
                result.CastPosition = predicted;
                result.UnitPosition = predicted;
                result.Hitchance = HitChance::Dashing;
                return result;
            }

            // ================================================================
            // Not moving — high chance
            // ================================================================
            if (!ai.IsMoving()) {
                result.CastPosition = target.GetPosition();
                result.UnitPosition = target.GetPosition();
                result.Hitchance = HitChance::VeryHigh;
                return result;
            }

            // ================================================================
            // Moving — predict position along path
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
            if (newDist > input.Range + input.Width) {
                result.Hitchance = HitChance::OutOfRange;
                return result;
            }

            // ================================================================
            // Estimate hit chance based on angle change
            // ================================================================
            Vec3 dir1 = target.GetPosition() - from;
            Vec3 dir2 = predicted - from;
            float dot = dir1.x * dir2.x + dir1.z * dir2.z;
            float len1 = dir1.Length2D();
            float len2 = dir2.Length2D();
            float cosAngle = (len1 > 0 && len2 > 0) ? dot / (len1 * len2) : 1.0f;

            // Movement perpendicular to cast direction = harder to predict
            float moveDist = moveSpeed * totalDelay;
            float hitbox = input.Width / 2.0f + target.GetBoundingRadius();

            if (totalDelay < 0.25f) {
                result.Hitchance = HitChance::VeryHigh;
            } else if (moveDist < hitbox * 0.5f) {
                result.Hitchance = HitChance::VeryHigh;
            } else if (cosAngle > 0.95f) {
                // Moving mostly towards/away — medium difficulty
                result.Hitchance = HitChance::High;
            } else if (cosAngle > 0.7f) {
                result.Hitchance = HitChance::Medium;
            } else {
                // Large angle change — low confidence
                result.Hitchance = HitChance::Low;
            }

            return result;
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
        // Collision check — are there minions between source and target?
        // ====================================================================
        static bool HasCollision(const Vec3& from, const Vec3& to, float width) {
            Vec3 dir = to - from;
            float dist = dir.Length2D();
            if (dist < 1.0f) return false;

            Vec3 norm = Vec3(-dir.z / dist, 0, dir.x / dist); // perpendicular

            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsAlive() || !minion.IsVisible()) continue;
                Vec3 mPos = minion.GetPosition();

                // Project minion onto line
                Vec3 toMinion = mPos - from;
                float proj = toMinion.x * dir.x / dist + toMinion.z * dir.z / dist;
                if (proj < 0 || proj > dist) continue;

                // Distance from line
                float perpDist = std::abs(toMinion.x * norm.x + toMinion.z * norm.z);
                float totalWidth = width / 2.0f + minion.GetBoundingRadius();

                if (perpDist <= totalWidth) return true;
            }
            return false;
        }
    };

} // namespace SDK
