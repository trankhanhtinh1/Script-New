#pragma once
#include "GameObject.h"
#include "AiManager.h"
#include "BuffManager.h"
#include "GameObjects.h"
#include "Enums.h"
#include "GamePath.h"
#include "HealthPrediction.h"
#include "Collisions.h"
// Note: Events/Dash.h is NOT included here to avoid circular dependency
// (Prediction.h is in Phase 3, Dash.h is Phase 5 in SDK.h).
// We use AiManager directly for dash info instead.
#include <cmath>
#include <cfloat>
#include <algorithm>

// ============================================================================
// Prediction - Movement prediction for skillshots
// Reference: EnsoulSharp.SDK/Core/Math/Prediction/Movement.cs
//
// 1-to-1 port of C# Movement class:
//   - GetPrediction(input, ft, checkCollision)
//   - GetDashingPrediction(input)
//   - GetImmobilePrediction(input, remainingImmobileT)
//   - GetPositionOnPath(input, path, speed)       ← CORE
//   - GetStandardPrediction(input)
//   - GetAdvancedPrediction(input)
//   - PositionAfter(unit, t, speed)
//   - UnitIsImmobileUntil(unit)
//   - VectorMovementCollision
//   - CutPath
// ============================================================================

namespace SDK {

    // ========================================================================
    // PredictionOutput — matches C# PredictionOutput
    // ========================================================================
    struct PredictionResult {
        Vec3 CastPosition;
        Vec3 UnitPosition;
        HitChance Hitchance;
        int AoEHitCount;
        std::vector<GameObject> AoeTargetsHit;  // C# List<AIHeroClient>
        std::vector<GameObject> CollisionObjects; // C# List<AIBaseClient>

        PredictionResult()
            : CastPosition(), UnitPosition(), Hitchance(HitChance::Impossible), AoEHitCount(0) {}
    };

    // ========================================================================
    // PredictionInput — matches C# PredictionInput
    // ========================================================================
    struct PredictionInput {
        Vec3 From;              // Default = ObjectManager.Player.Position
        Vec3 RangeCheckFrom;    // Default = From
        float Range;            // C#: float.MaxValue
        float Speed;            // C#: float.MaxValue (instant)
        float Delay;            // Cast delay (seconds)
        float Radius;           // C#: default 1f
        float Width;            // Alias (for backwards compat)
        SkillshotType Type;
        bool Collision;          // C# name: Collision
        bool CollisionCheck;     // Alias for Collision (backwards compat)
        int CollisionFlags;
        bool AoE;               // C# name: AoE
        bool Aoe;               // Alias (backwards compat)
        bool UseBoundingRadius;  // C#: default true

        PredictionInput()
            : From(), RangeCheckFrom(), Range(FLT_MAX), Speed(FLT_MAX), Delay(0.0f),
              Radius(1.0f), Width(0.0f),
              Type(SkillshotType::Line),
              Collision(false), CollisionCheck(false), CollisionFlags(0),
              AoE(false), Aoe(false), UseBoundingRadius(true) {}

        // C#: internal float RealRadius => UseBoundingRadius ? Radius + Unit.BoundingRadius : Radius;
        // Note: BoundingRadius is added externally since we don't store Unit here
        float RealRadius() const { return Radius > 0 ? Radius : Width; }

        // Resolve collision flag from both fields
        bool HasCollision() const { return Collision || CollisionCheck; }
        bool HasAoE() const { return AoE || Aoe; }
    };

    // ========================================================================
    // Prediction Engine — 1:1 port of C# Movement class
    // ========================================================================
    class Prediction {
    public:
        // ====================================================================
        // Public overloads — matches C# Movement.GetPrediction overloads
        // ====================================================================

        // C#: GetPrediction(unit, delay)
        static PredictionResult GetPrediction(const GameObject& unit, float delay) {
            PredictionInput input;
            input.Delay = delay;
            return GetPrediction(input, unit, true, true);
        }

        // C#: GetPrediction(unit, delay, radius)
        static PredictionResult GetPrediction(const GameObject& unit, float delay, float radius) {
            PredictionInput input;
            input.Delay = delay;
            input.Radius = radius;
            return GetPrediction(input, unit, true, true);
        }

        // C#: GetPrediction(unit, delay, radius, speed)
        static PredictionResult GetPrediction(const GameObject& unit, float delay, float radius, float speed) {
            PredictionInput input;
            input.Delay = delay;
            input.Radius = radius;
            input.Speed = speed;
            return GetPrediction(input, unit, true, true);
        }

        // C#: GetPrediction(input) — main public API
        static PredictionResult GetPrediction(const PredictionInput& input, const GameObject& unit) {
            return GetPrediction(input, unit, true, true);
        }

        // Backwards compat: (target, input) style
        static PredictionResult GetPrediction(const GameObject& target, const PredictionInput& input) {
            return GetPrediction(input, target, true, true);
        }

        // Simplified API (backwards compat)
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
            return GetPrediction(input, target, true, true);
        }

        // ====================================================================
        // Core: GetPrediction(input, ft, checkCollision)
        // Reference: Movement.cs line 332
        // ====================================================================
        static PredictionResult GetPrediction(PredictionInput input, const GameObject& unit,
                                               bool ft, bool checkCollision) {
            if (!unit.IsValid() || !unit.IsAlive()) {
                return PredictionResult();
            }

            // Resolve From / RangeCheckFrom
            Vec3 from = ResolveFrom(input);
            input.From = from;
            Vec3 rangeCheckFrom = ResolveRangeCheckFrom(input);

            // C#: if (ft) { input.Delay += (Game.Ping / 2000f) + 0.06f; }
            if (ft) {
                input.Delay += Game::GetPing() / 2000.0f + 0.06f;

                // C#: if (input.AoE) return Cluster.GetAoEPrediction(input);
                // AoE is handled by Cluster.h — callers use Cluster::GetAoEPrediction directly
                // We leave this for the caller to check (Cluster.h includes Prediction.h)
            }

            // C#: Target too far away
            if (!IsMaxRange(input.Range)) {
                float maxRange = input.Range * 1.5f;
                if (rangeCheckFrom.DistanceSqr2D(unit.GetPosition()) > maxRange * maxRange) {
                    PredictionResult r;
                    r.Hitchance = HitChance::OutOfRange;
                    return r;
                }
            }

            PredictionResult result;
            bool usedSpecial = false;

            // C#: if (unit.IsDashing()) → GetDashingPrediction
            if (unit.IsDashing()) {
                result = GetDashingPrediction(input, unit);
                usedSpecial = true;
            } else {
                // C#: var remainingImmobileT = UnitIsImmobileUntil(unit);
                float remainingImmobileT = UnitIsImmobileUntil(unit);
                if (remainingImmobileT >= 0.0f) {
                    result = GetImmobilePrediction(input, unit, remainingImmobileT);
                    usedSpecial = true;
                }
            }

            // C#: if (result == null) result = GetAdvancedPrediction(input);
            // We use GetStandardPrediction which delegates to GetPositionOnPath (the real C# flow)
            if (!usedSpecial || result.Hitchance == HitChance::Impossible) {
                result = GetStandardPrediction(input, unit);
            }

            // C#: Check range — demote High→Medium, OutOfRange, clamp cast
            if (!IsMaxRange(input.Range)) {
                float realRadius = GetRealRadius(input, unit);

                if ((int)result.Hitchance >= (int)HitChance::High) {
                    float edgeRange = input.Range + realRadius * 3.0f / 4.0f;
                    if (rangeCheckFrom.DistanceSqr2D(unit.GetPosition()) > edgeRange * edgeRange) {
                        result.Hitchance = HitChance::Medium;
                    }
                }

                float extraRange = (input.Type == SkillshotType::Circle) ? realRadius : 0.0f;
                float maxUnitRange = input.Range + extraRange;
                if (rangeCheckFrom.DistanceSqr2D(result.UnitPosition) > maxUnitRange * maxUnitRange) {
                    result.Hitchance = HitChance::OutOfRange;
                }

                if (rangeCheckFrom.DistanceSqr2D(result.CastPosition) > input.Range * input.Range) {
                    if (result.Hitchance != HitChance::OutOfRange) {
                        Vec3 dir = (result.UnitPosition - rangeCheckFrom).Normalized2D();
                        result.CastPosition = rangeCheckFrom + dir * input.Range;
                    } else {
                        result.Hitchance = HitChance::OutOfRange;
                    }
                }
            }

            // C#: Check for collision
            if (checkCollision && input.HasCollision()) {
                std::vector<Vec3> positions = { result.UnitPosition, result.CastPosition, unit.GetPosition() };
                auto collisionObjects = GetCollisionObjects(positions, input, unit);
                if (!collisionObjects.empty()) {
                    result.CollisionObjects = collisionObjects;
                    result.Hitchance = HitChance::Collision;
                }
            }

            return result;
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
        // Collision convenience wrappers
        // ====================================================================
        static bool HasCollision(const Vec3& from, const Vec3& to, float width) {
            return Collisions::HasMinionCollision(from, to, width);
        }

        static bool HasCollisionPredicted(const Vec3& from, const Vec3& to,
                                           float width, float speed, float delay) {
            return Collisions::HasMinionCollisionPredicted(from, to, width, speed, delay);
        }

        static bool HasProjectileBlockerCollision(const Vec3& from, const Vec3& to) {
            return Collisions::HasProjectileBlockerCollision(from, to);
        }

        static bool HasYasuoWindWallCollision(const Vec3& from, const Vec3& to) {
            return Collisions::HasYasuoWindWallCollision(from, to);
        }

        static Collisions::CollisionResult GetCollision(const Vec3& from, const Vec3& to,
                                                         float width, float speed = 0.0f,
                                                         float delay = 0.25f,
                                                         int flags = Collisions::CheckAll,
                                                         const GameObject& exclude = GameObject()) {
            return Collisions::GetCollision(from, to, width, speed, delay, flags, exclude);
        }

        // ====================================================================
        // Health prediction convenience
        // ====================================================================
        static float GetPredictedHealth(const GameObject& target,
                                         float delay, float speed,
                                         const Vec3& from = Vec3()) {
            Vec3 origin = from.IsZero() ? GameObjects::Player.GetPosition() : from;
            float dist = origin.Distance2D(target.GetPosition());
            float totalDelay = delay;
            if (!IsInstantSpeed(speed)) totalDelay += dist / speed;
            float totalDelayMs = totalDelay * 1000.0f;
            return HealthPrediction::GetPrediction(target, totalDelayMs);
        }

        // ====================================================================
        // VectorMovementCollision — CORE MATH
        // Reference: SharpDX Vector2 extension in C#
        // Solves: when does missile from pos2 at speed2 intercept unit
        //         moving from startPos1→endPos1 at speed1?
        // Returns: {time, collision_point}
        // ====================================================================
        static std::pair<float, Vec3> VectorMovementCollision(const Vec3& startPos1,
                                                               const Vec3& endPos1,
                                                               float speed1,
                                                               const Vec3& startPos2,
                                                               float speed2,
                                                               float delay = 0.0f) {
            if (speed1 <= 0.0f || speed2 <= 0.0f) {
                return { -1.0f, Vec3() };
            }

            const float sP1x = startPos1.x;
            const float sP1y = startPos1.z;
            const float eP1x = endPos1.x;
            const float eP1y = endPos1.z;
            const float sP2x = startPos2.x;
            const float sP2y = startPos2.z;

            const float d = eP1x - sP1x;
            const float e = eP1y - sP1y;
            const float dist = sqrtf(d * d + e * e);
            if (dist < 0.0001f) {
                return { -1.0f, Vec3() };
            }

            const float t1 = dist / speed1;
            const float S = speed1;
            const float K = d / dist;
            const float L = e / dist;

            const float a = (S * S) - (speed2 * speed2);
            const float b = -2.0f * (sP1x * S * K - sP2x * S * K + sP1y * S * L - sP2y * S * L);
            const float c = (sP1x - sP2x) * (sP1x - sP2x) + (sP1y - sP2y) * (sP1y - sP2y);

            if (fabsf(a) < 0.0001f) {
                if (fabsf(b) > 0.0001f) {
                    const float t = -c / b;
                    if (t >= delay && t <= t1) {
                        return { t, Vec3(sP1x + t * S * K, startPos1.y, sP1y + t * S * L) };
                    }
                }
                return { -1.0f, Vec3() };
            }

            const float disc = b * b - 4.0f * a * c;
            if (disc < 0.0f) {
                return { -1.0f, Vec3() };
            }

            const float sqrtDisc = sqrtf(disc);
            const float t1Sol = (-b - sqrtDisc) / (2.0f * a);
            const float t2Sol = (-b + sqrtDisc) / (2.0f * a);
            const float t = (t1Sol >= delay && t1Sol <= t1)
                ? t1Sol
                : ((t2Sol >= delay && t2Sol <= t1) ? t2Sol : -1.0f);

            if (t >= 0.0f) {
                return { t, Vec3(sP1x + t * S * K, startPos1.y, sP1y + t * S * L) };
            }
            return { -1.0f, Vec3() };
        }

        // ====================================================================
        // CutPath — Reference: C# path.CutPath(distance)
        // ====================================================================
        static std::vector<Vec3> CutPath(const std::vector<Vec3>& path, float distance) {
            if (path.size() <= 1 || distance <= 0.0f) {
                return path;
            }

            std::vector<Vec3> result;
            float remaining = distance;

            for (size_t i = 0; i + 1 < path.size(); i++) {
                const float segLen = path[i].Distance2D(path[i + 1]);
                if (remaining > segLen) {
                    remaining -= segLen;
                    continue;
                }

                const Vec3 startPoint = path[i].Extend(path[i + 1], remaining);
                result.push_back(startPoint);
                for (size_t j = i + 1; j < path.size(); j++) {
                    result.push_back(path[j]);
                }
                break;
            }

            if (result.empty()) {
                result.push_back(path.back());
            }
            return result;
        }

        // ====================================================================
        // UnitIsImmobileUntil — Reference: Movement.cs line 486
        // Returns remaining CC time (>=0 = immobile, <0 = not immobile)
        // Checks: Charm, Knockup, Stun, Suppression, Snare
        // ====================================================================
        static float UnitIsImmobileUntil(const GameObject& unit) {
            const float now = Game::GetTime();
            float maxEnd = -1.0f;
            BuffManager bm(unit.address);

            bm.ForEach([&](Buff& buff) {
                if (!buff.IsActive()) return;

                const BuffType t = buff.GetType();
                if (t != BuffType::Charm &&
                    t != BuffType::Knockup &&
                    t != BuffType::Stun &&
                    t != BuffType::Suppression &&
                    t != BuffType::Snare) {
                    return;
                }

                maxEnd = std::max(maxEnd, buff.GetEndTime());
            });

            return maxEnd - now;
        }

    private:
        // ====================================================================
        // GetDashingPrediction — Reference: Movement.cs line 130
        // ====================================================================
        static PredictionResult GetDashingPrediction(PredictionInput input,
                                                      const GameObject& unit) {
            PredictionResult result;

            // C#: input.Delay += 0.1f;
            input.Delay += 0.1f;

            // Get dash info directly from AiManager (avoids Dash.h dependency)
            AiManager ai(unit.address);
            Vec3 curPos = unit.GetPosition();
            Vec3 dashEnd = ai.GetPathEnd();
            if (dashEnd.IsZero()) dashEnd = curPos;

            float dashSpeed = ai.GetDashSpeed();
            if (dashSpeed < 100.0f) {
                dashSpeed = std::max(unit.GetMoveSpeed(), 1000.0f);
            }

            // Detect blink: very high speed or near-instant dash
            float dashDist = curPos.Distance2D(dashEnd);
            float dashDuration = (dashSpeed > 0.0f) ? (dashDist / dashSpeed) : 0.0f;
            bool isBlink = (dashSpeed > 5000.0f || dashDuration < 0.01f);

            // C#: if (!dashData.IsBlink) { ... normal dashes ... }
            if (!isBlink) {
                // Mid air: GetPositionOnPath with dash path
                std::vector<Vec3> dashPath = { curPos, dashEnd };
                auto dashPred = GetPositionOnPath(input, unit, dashPath, dashSpeed);

                if ((int)dashPred.Hitchance >= (int)HitChance::High) {
                    dashPred.CastPosition = dashPred.UnitPosition;
                    dashPred.Hitchance = HitChance::Dashing;
                    return dashPred;
                }

                // At the end of the dash
                if (dashDist > 200.0f) {
                    float timeToPoint = input.Delay;
                    if (!IsInstantSpeed(input.Speed)) {
                        timeToPoint += input.From.Distance2D(dashEnd) / input.Speed;
                    }
                    float arrivalTime = unit.DistanceTo(dashEnd) / std::max(dashSpeed, 1.0f)
                        + GetRealRadius(input, unit) / std::max(unit.GetMoveSpeed(), 1.0f);

                    if (timeToPoint <= arrivalTime) {
                        result.CastPosition = dashEnd;
                        result.UnitPosition = dashEnd;
                        result.Hitchance = HitChance::Dashing;
                        return result;
                    }
                }

                // C#: result.CastPosition = dashData.Path.Last().ToVector3();
                result.CastPosition = dashEnd;
                result.UnitPosition = dashEnd;
            }

            // C#: return result; (HitChance remains Impossible for blinks)
            return result;
        }

        // ====================================================================
        // GetImmobilePrediction — Reference: Movement.cs line 184
        // ====================================================================
        static PredictionResult GetImmobilePrediction(const PredictionInput& input,
                                                       const GameObject& unit,
                                                       float remainingImmobileT) {
            PredictionResult result;
            const float moveSpeed = std::max(unit.GetMoveSpeed(), 1.0f);
            const float dist = unit.GetPosition().Distance2D(input.From);

            // C#: var timeToReachTargetPosition = input.Delay + (input.Unit.Distance(input.From) / input.Speed);
            float timeToReach = input.Delay;
            if (!IsInstantSpeed(input.Speed)) {
                timeToReach += dist / input.Speed;
            }

            const float realRadius = GetRealRadius(input, unit);

            result.CastPosition = unit.GetPosition();
            result.UnitPosition = unit.GetPosition();

            // C#: if (timeToReach <= remainingImmobileT + (input.RealRadius / input.Unit.MoveSpeed))
            if (timeToReach <= remainingImmobileT + realRadius / moveSpeed) {
                result.Hitchance = HitChance::Immobile;
            } else {
                result.Hitchance = HitChance::High;
            }

            return result;
        }

        // ====================================================================
        // GetStandardPrediction — Reference: Movement.cs line 431
        // ====================================================================
        static PredictionResult GetStandardPrediction(const PredictionInput& input,
                                                       const GameObject& unit) {
            float speed = unit.GetMoveSpeed();

            // C#: if (input.Unit.DistanceSquared(input.From) < 200 * 200) speed /= 1.5f;
            if (unit.GetPosition().DistanceSqr2D(input.From) < 200.0f * 200.0f) {
                speed /= 1.5f;
            }

            // C#: var result = GetPositionOnPath(input, input.Unit.GetWaypoints(), speed);
            auto path = unit.GetWaypoints();
            if (path.empty()) {
                path.push_back(unit.GetServerPosition());
            }
            return GetPositionOnPath(input, unit, path, speed);
        }

        // ====================================================================
        // GetPositionOnPath — CORE ALGORITHM
        // Reference: Movement.cs line 213
        // Handles both instant-speed and missile-speed skillshots
        // ====================================================================
        static PredictionResult GetPositionOnPath(const PredictionInput& input,
                                                   const GameObject& unit,
                                                   std::vector<Vec3> path,
                                                   float speed = -1.0f) {
            PredictionResult result;
            const float targetSpeed = (speed < 0.0f) ? unit.GetMoveSpeed() : speed;

            // C#: if (path.Count <= 1)
            if (path.size() <= 1) {
                Vec3 pos = path.empty() ? unit.GetPosition() : path[0];
                result.CastPosition = pos;
                result.UnitPosition = pos;
                result.Hitchance = HitChance::VeryHigh;
                return result;
            }

            const float realRadius = GetRealRadius(input, unit);
            const float pathLength = Geometry::PathLength(path);
            const float triggerDistance = std::max(0.0f, input.Delay * targetSpeed - realRadius);

            // ============================================================
            // Case 1: Instant-speed skillshots (speed == FLT_MAX)
            // C#: if (pLength >= tDistance && Math.Abs(input.Speed - float.MaxValue) < float.Epsilon)
            // ============================================================
            if (pathLength >= triggerDistance && IsInstantSpeed(input.Speed)) {
                float remaining = triggerDistance;
                for (size_t i = 0; i + 1 < path.size(); i++) {
                    const Vec3 a = path[i];
                    const Vec3 b = path[i + 1];
                    const float segLen = a.Distance2D(b);

                    if (segLen >= remaining) {
                        const Vec3 dir = (b - a).Normalized2D();
                        const Vec3 cp = a + dir * remaining;

                        // C#: var p = a + direction * ((i == path.Count - 2) ? Min(tDistance+RealRadius, d) : tDistance+RealRadius);
                        const float push = (i == path.size() - 2)
                            ? std::min(remaining + realRadius, segLen)
                            : (remaining + realRadius);
                        const Vec3 unitPos = a + dir * push;

                        result.CastPosition = cp;
                        result.UnitPosition = unitPos;

                        // C#: GamePath.PathTracker.GetCurrentPath(input.Unit).Time < 0.1d ? VeryHigh : High
                        result.Hitchance = GetPathHitchance(unit);
                        return result;
                    }

                    remaining -= segLen;
                }
            }

            // ============================================================
            // Case 2: Missile-speed skillshots (speed != FLT_MAX)
            // C#: if (pLength >= tDistance && Math.Abs(input.Speed - float.MaxValue) > float.Epsilon)
            // Uses VectorMovementCollision to solve interception
            // ============================================================
            if (pathLength >= triggerDistance && !IsInstantSpeed(input.Speed) && input.Speed > 0.0f) {
                // C#: path = path.CutPath(Max(0, delay*speed - RealRadius));
                float d = triggerDistance;
                if ((input.Type == SkillshotType::Line || input.Type == SkillshotType::Cone) &&
                    input.From.DistanceSqr2D(path[0]) < 200.0f * 200.0f) {
                    d = input.Delay * targetSpeed;
                }

                path = CutPath(path, std::max(0.0f, d));
                float tT = 0.0f;

                for (size_t i = 0; i + 1 < path.size(); i++) {
                    Vec3 a = path[i];
                    const Vec3 b = path[i + 1];
                    const float segLen = a.Distance2D(b);
                    const float segTime = segLen / std::max(targetSpeed, 1.0f);
                    const Vec3 dir = (b - a).Normalized2D();

                    // C#: a = a - (speed * tT * direction);
                    a = a - dir * (targetSpeed * tT);

                    // C#: var sol = a.VectorMovementCollision(b, speed, input.From, input.Speed, tT);
                    auto solution = VectorMovementCollision(a, b, targetSpeed, input.From, input.Speed, tT);
                    const float t = solution.first;
                    Vec3 pos = solution.second;

                    if (pos.IsValid() && t >= tT && t <= tT + segTime) {
                        if (pos.Distance2D(b) < 20.0f) {
                            break; // At segment end — try next
                        }

                        // C#: var p = pos + (input.RealRadius * direction);
                        Vec3 unitPos = pos + dir * realRadius;

                        // C#: Line skillshot angle adjustment
                        if (input.Type == SkillshotType::Line) {
                            Vec2 fromPos2d = input.From.To2D();
                            Vec2 p2d = unitPos.To2D();
                            Vec2 a2d = a.To2D();
                            Vec2 b2d = b.To2D();
                            float alpha = fabsf(Geometry::RadToDeg(
                                (fromPos2d - p2d).AngleBetween(a2d - b2d)));
                            if (alpha > 30.0f && alpha < 180.0f - 30.0f) {
                                float sinBeta = realRadius / p2d.Distance(fromPos2d);
                                if (sinBeta > -1.0f && sinBeta < 1.0f) {
                                    float beta = asinf(sinBeta);
                                    Vec2 cp1 = fromPos2d + (p2d - fromPos2d).Rotated(beta);
                                    Vec2 cp2 = fromPos2d + (p2d - fromPos2d).Rotated(-beta);
                                    Vec2 pos2d = pos.To2D();
                                    pos = (cp1.DistanceSqr(pos2d) < cp2.DistanceSqr(pos2d))
                                        ? Vec3::From2D(cp1, pos.y) : Vec3::From2D(cp2, pos.y);
                                }
                            }
                        }

                        result.CastPosition = pos;
                        result.UnitPosition = unitPos;
                        result.Hitchance = GetPathHitchance(unit);
                        return result;
                    }

                    tT += segTime;
                }
            }

            // Fallback: end of path
            result.CastPosition = path.back();
            result.UnitPosition = path.back();
            result.Hitchance = HitChance::Medium;
            return result;
        }

        // ====================================================================
        // GetAdvancedPrediction — Reference: Movement.cs line 102
        // Used as fallback when standard prediction doesn't apply
        // ====================================================================
        static PredictionResult GetAdvancedPrediction(const PredictionInput& input,
                                                       const GameObject& unit) {
            float speed = input.Speed;
            if (IsInstantSpeed(speed)) {
                speed = 90000.0f;
            }

            // C#: var position = PositionAfter(unit, 1, unit.MoveSpeed - 100);
            Vec2 position = PositionAfter(unit, 1.0f, unit.GetMoveSpeed() - 100.0f);
            // C#: var prediction = position + (speed * (input.Delay / 1000));
            // Note: In C# this seems like a bug (delay is in seconds but /1000 again)
            // We keep it exactly as C# does it
            Vec2 prediction = position + Vec2(speed * (input.Delay / 1000.0f), 0.0f);

            PredictionResult result;
            result.UnitPosition = Vec3(position.x, unit.GetPosition().y, position.y);
            result.CastPosition = Vec3(prediction.x, unit.GetPosition().y, prediction.y);
            result.Hitchance = HitChance::High;
            return result;
        }

        // ====================================================================
        // PositionAfter — Reference: Movement.cs line 457
        // Returns position on path after t seconds at given speed
        // ====================================================================
        static Vec2 PositionAfter(const GameObject& unit, float t, float speed = FLT_MAX) {
            float distance = t * speed;
            auto path = unit.GetWaypoints();
            if (path.empty()) return unit.GetPosition().To2D();

            // Convert Vec3 path to Vec2 for 2D walk
            for (size_t i = 0; i + 1 < path.size(); i++) {
                Vec2 a = path[i].To2D();
                Vec2 b = path[i + 1].To2D();
                float d = a.Distance(b);

                if (d < distance) {
                    distance -= d;
                } else {
                    return a + (b - a).Normalized() * distance;
                }
            }

            return path.back().To2D();
        }

        // ====================================================================
        // Helpers
        // ====================================================================

        static bool IsInstantSpeed(float speed) {
            return speed >= FLT_MAX * 0.5f;
        }

        static bool IsMaxRange(float range) {
            return range >= FLT_MAX * 0.5f;
        }

        static Vec3 ResolveFrom(const PredictionInput& input) {
            if (!input.From.IsZero()) return input.From;
            if (GameObjects::Player.IsValid()) return GameObjects::Player.GetPosition();
            return Vec3();
        }

        static Vec3 ResolveRangeCheckFrom(const PredictionInput& input) {
            if (!input.RangeCheckFrom.IsZero()) return input.RangeCheckFrom;
            return ResolveFrom(input);
        }

        // C#: RealRadius => UseBoundingRadius ? Radius + Unit.BoundingRadius : Radius
        static float GetRealRadius(const PredictionInput& input, const GameObject& unit) {
            float r = input.RealRadius();
            if (input.UseBoundingRadius) {
                r += unit.GetBoundingRadius();
            }
            return r;
        }

        // C#: GamePath.PathTracker.GetCurrentPath(input.Unit).Time < 0.1d ? VeryHigh : High
        static HitChance GetPathHitchance(const GameObject& unit) {
            auto path = GamePath::PathTracker::GetCurrentPath(unit);
            float age = path.Time(Game::GetTime());
            return age < 0.1f ? HitChance::VeryHigh : HitChance::High;
        }

        // ====================================================================
        // Collision check — Reference: Movement.cs line 412-418
        // ====================================================================
        static std::vector<GameObject> GetCollisionObjects(const std::vector<Vec3>& positions,
                                                            const PredictionInput& input,
                                                            const GameObject& unit) {
            std::vector<GameObject> collisionObjects;
            Vec3 from = ResolveFrom(input);
            float width = input.RealRadius();

            int flags = 0;
            if (input.CollisionFlags & CollisionMinions)     flags |= Collisions::CheckMinions;
            if (input.CollisionFlags & CollisionHeroes)      flags |= Collisions::CheckHeroes;
            if (input.CollisionFlags & CollisionWalls)       flags |= Collisions::CheckWalls;
            if (input.CollisionFlags & CollisionYasuoWall)   flags |= Collisions::CheckProjectileBlockers;
            if (input.CollisionFlags & CollisionBraumShield) flags |= Collisions::CheckProjectileBlockers;

            if (flags == 0) {
                flags = Collisions::CheckMinions | Collisions::CheckProjectileBlockers;
            }

            for (const auto& p : positions) {
                auto col = Collisions::GetCollision(from, p, width, input.Speed, input.Delay, flags, unit);
                if (col.HasCollision) {
                    for (auto& obj : col.CollidingObjects) {
                        if (obj.GetNetId() != unit.GetNetId()) {
                            collisionObjects.push_back(obj);
                        }
                    }
                }
            }
            return collisionObjects;
        }
    };

} // namespace SDK
