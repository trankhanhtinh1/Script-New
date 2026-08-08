#pragma once

#include "FsPredAoe.h"
#include "FsPredCollision.h"
#include "FsPredUnitTracker.h"

#include "../../../sdk/Extensions/AIBaseClientExtensions.h"
#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/Math/Prediction/Movement.h"
#include "../../../sdk/Utils/MathUtils.h"
#include "../../../core/CoreNavGrid.h"
#include "../../../core/CoreBuffs.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace Plugins::FsPred {

struct FsPredConfig {
    int maxRangePercent = 100;
    int extraDelayMs = 10;
    bool recheckHitchance = true;
    bool useDefaultSdk = false;
};

class FsPredEngine final : public SDK::Prediction::IPrediction {
public:
    FsPredEngine() = default;

    void SetConfig(const FsPredConfig& config) {
        config_ = config;
    }

    const FsPredConfig& Config() const {
        return config_;
    }

    SDK::PredictionOutput GetPrediction(SDK::PredictionInput input) override {
        return GetPrediction(input, true, true);
    }

    SDK::PredictionOutput GetPrediction(SDK::PredictionInput input, bool ft, bool checkCollision) override {
        UnitTracker::Update();

        if (!input.Unit.IsValid()) {
            SDK::PredictionOutput empty;
            empty.Input = input;
            return empty;
        }

        if (config_.useDefaultSdk) {
            if (auto* sdkPred = SDK::Prediction::GetSDKPrediction()) {
                return sdkPred->GetPrediction(input, ft, checkCollision);
            }
        }

        // Apply range modifier setting if defined
        if (config_.maxRangePercent < 100 && input.Range > 0.0f && std::abs(input.Range - FLT_MAX) > 0.0001f) {
            input.Range = input.Range * (static_cast<float>(config_.maxRangePercent) / 100.0f);
        }

        const SDK::Vector3 unitPos = input.Unit.Position();
        const SDK::Vector3 rangeFrom = input.ResolveRangeCheckFrom();

        // Target too far away check
        if (std::abs(input.Range - FLT_MAX) > 0.0001f &&
            unitPos.DistanceSquared(rangeFrom) > std::pow(input.Range * 1.5f, 2.0f)) {
            SDK::PredictionOutput output;
            output.Input = input;
            return output;
        }

        if (ft) {
            // Always use the real prediction calculator with one consistent
            // latency-compensation path.
            input.Delay += (static_cast<float>(SDK::Game::Ping()) / 1000.0f)
                + (static_cast<float>(config_.extraDelayMs) / 1000.0f);

            if (input.AoE) {
                SDK::PredictionOutput output = AoePrediction::GetPrediction(input, [this](SDK::PredictionInput in, bool f, bool c) {
                    return this->GetPredictionInternal(in, f, c);
                });

                // AoE already chose its geometry. Do not move that cast point with
                // wall heuristics afterwards; only reject invalid final positions.
                ApplyRangeChecks(output, input);
                return output;
            }
        }

        return GetPredictionInternal(input, ft, checkCollision);
    }

private:
    SDK::PredictionOutput GetPredictionInternal(SDK::PredictionInput input, bool ft, bool checkCollision) {
        (void)ft;
        SDK::PredictionOutput output;
        output.Input = input;

        bool hasResult = false;
        if (SDK::Extensions::IsDashing(input.Unit)) {
            output = GetDashingPrediction(input);
            hasResult = true;
        } else if (!SDK::CanMove(input.Unit)) {
            const double remainingImmobileT = UnitIsImmobileUntil(input.Unit);
            if (remainingImmobileT >= 0.0) {
                output = GetImmobilePrediction(input, remainingImmobileT);
                hasResult = true;
            }
        }

        if (!hasResult) {
            output = GetStandardPrediction(input);
        }

        // Wall adjustment is only a refinement for already-good predictions.
        // It must never promote a Low/Medium result into a castable High result.
        if (input.Unit.IsHero() && input.Radius > 1.0f &&
            output.Hitchance >= SDK::HitChance::High &&
            output.Hitchance <= SDK::HitChance::VeryHigh) {
            ApplyWallAdjust(output, input);
        }

        // Range checks: demote with certainty, never fix up a wrong prediction.
        ApplyRangeChecks(output, input);

        // Hitchance refinement based on waypoints
        if (output.Hitchance == SDK::HitChance::High || output.Hitchance == SDK::HitChance::VeryHigh) {
            const auto& waypoints3D = input.Unit.CachedWaypoints();
            if ((waypoints3D.size() > 1) != input.Unit.IsMoving()) {
                output.Hitchance = SDK::HitChance::Medium;
            } else if (!waypoints3D.empty()) {
                const SDK::Vector3 lastWp = waypoints3D.back();
                const float distWpToUnit = lastWp.Distance(input.Unit.Position());
                const float distFromToUnit = input.ResolveFrom().Distance(input.Unit.Position());
                const float distWpToFrom = lastWp.Distance(input.ResolveFrom());

                float speedDelay = (std::abs(input.Speed - FLT_MAX) < 0.0001f) ? 0.0f : (distFromToUnit / input.Speed);
                const float totalDelay = speedDelay + input.Delay;
                float fixRange = input.Unit.MoveSpeed() * totalDelay * 0.35f;
                if (SDK::IsCircleSpellType(input.Type)) {
                    fixRange -= input.Radius / 2.0f;
                }

                if (distWpToFrom <= distFromToUnit && distFromToUnit > input.Range - fixRange) {
                    output.Hitchance = SDK::HitChance::Medium;
                }
                if (distWpToUnit > 0.0f && distWpToUnit < 100.0f) {
                    output.Hitchance = SDK::HitChance::Medium;
                }
            }
        }

        // Collision validation
        if (checkCollision && input.Collision) {
            const std::vector<SDK::Vector3> positions = {
                output.GetUnitPosition(),
                output.GetCastPosition()
            };

            // Uses a cheap position-only prediction internally — no recursion
            // back into the full pipeline per minion/hero (see FsPredCollision.h).
            auto collisionObjects = Collision::GetCollision(positions, input);

            // Exclude input unit from collision hits
            const std::uint32_t targetId = input.Unit.NetworkId();
            collisionObjects.erase(
                std::remove_if(collisionObjects.begin(), collisionObjects.end(), [targetId](const SDK::AIBaseClient& obj) {
                    return !obj.IsValid() || obj.NetworkId() == targetId;
                }),
                collisionObjects.end());

            output.CollisionObjects = collisionObjects;
            if (!output.CollisionObjects.empty()) {
                output.Hitchance = SDK::HitChance::Collision;
            }
        }

        // WayPointAnalysis refinement
        if ((output.Hitchance == SDK::HitChance::High || output.Hitchance == SDK::HitChance::VeryHigh) && config_.recheckHitchance) {
            output = WayPointAnalysis(output, input);
        }

        // Final range + geometry validation. WayPointAnalysis / dash / immobile may
        // have replaced the cast position — nothing may move it past this point.
        ApplyFinalRangeValidation(output, input);

        return output;
    }

    SDK::PredictionOutput GetDashingPrediction(SDK::PredictionInput& input) {
        SDK::PredictionOutput output;
        output.Input = input;

        const auto dashInfo = SDK::Extensions::GetDashInfo(input.Unit);
        const SDK::Vector3 startPos = input.Unit.Position();
        const SDK::Vector3 endPos = dashInfo.EndPos.IsValid() ? dashInfo.EndPos : startPos;

        const std::vector<SDK::Vector2> dashPath = { startPos.To2D(), endPos.To2D() };
        auto pathPred = GetPositionOnPath(input, dashPath, dashInfo.Speed);

        const auto proj = SDK::Prediction::Vec2Ext::ProjectOn(pathPred.GetUnitPosition().To2D(), startPos.To2D(), endPos.To2D());
        if (pathPred.Hitchance >= SDK::HitChance::High && proj.SegmentPoint.DistanceSquared(startPos.To2D()) < 200.0f * 200.0f) {
            pathPred.SetCastPosition(pathPred.GetUnitPosition());
            pathPred.Hitchance = SDK::HitChance::Dash;
            return pathPred;
        }

        const float pathLength = SDK::Utils::MathUtils::PathLength(dashPath);
        const float timeToPoint = (input.Delay * 0.5f) + (input.ResolveFrom().Distance(endPos) / input.Speed) - 0.25f;
        const float timeForUnit = (input.Unit.Position().Distance(endPos) / std::max(1.0f, dashInfo.Speed)) + (input.RealRadius() / std::max(1.0f, input.Unit.MoveSpeed()));

        if (pathLength > 200.0f && timeToPoint <= timeForUnit && endPos.IsValid() && !endPos.IsZero()) {
            output.SetCastPosition(endPos);
            output.SetUnitPosition(endPos);
            output.Hitchance = SDK::HitChance::Dash;
            return output;
        }

        const auto& waypoints = input.Unit.CachedWaypoints();
        const SDK::Vector3 lastWp = waypoints.empty() ? endPos : waypoints.back();
        output.SetCastPosition(lastWp);
        output.SetUnitPosition(lastWp);
        return output;
    }

    SDK::PredictionOutput GetImmobilePrediction(const SDK::PredictionInput& input, double remainingImmobileT) {
        SDK::PredictionOutput output;
        output.Input = input;

        const SDK::Vector3 pos = input.Unit.Position();
        const float timeToReach = input.Delay + (input.Unit.Distance(input.ResolveFrom()) / input.Speed);
        const double threshold = remainingImmobileT + static_cast<double>(input.RealRadius() / std::max(1.0f, input.Unit.MoveSpeed()));

        if (static_cast<double>(timeToReach) <= threshold) {
            output.SetCastPosition(pos);
            output.SetUnitPosition(pos);
            output.Hitchance = SDK::HitChance::Immobile;
            return output;
        }

        // Projectile arrives AFTER the CC ends — the target gets to run away.
        // Lead the remaining free movement instead of shooting at the old spot.
        const double freeMoveT = std::max(0.0, static_cast<double>(timeToReach) - remainingImmobileT);
        SDK::Vector3 leadPos = pos;
        const auto& waypoints = input.Unit.CachedWaypoints();
        if (!waypoints.empty()) {
            const SDK::Vector3 dir = (waypoints.back() - pos);
            if (dir.LengthSqr2D() > 1.0f) {
                const SDK::Vector2 dir2D = dir.To2D().Normalized();
                const float leadDist = static_cast<float>(freeMoveT) * input.Unit.MoveSpeed();
                leadPos = SDK::Vector3::From2D(pos.To2D() + dir2D * leadDist);
            }
        }

        output.SetCastPosition(leadPos);
        output.SetUnitPosition(leadPos);
        output.Hitchance = SDK::HitChance::High;
        return output;
    }

    SDK::PredictionOutput GetStandardPrediction(SDK::PredictionInput& input) {
        // thread_local scratch: GetPrediction is called many times per frame,
        // so avoid a fresh heap allocation for the 2D path on every call.
        thread_local std::vector<SDK::Vector2> path;
        path.clear();
        const auto& waypoints3D = input.Unit.CachedWaypoints();
        path.reserve(waypoints3D.size());
        for (const auto& wp : waypoints3D) {
            path.push_back(wp.To2D());
        }
        return GetPositionOnPath(input, std::move(path), input.Unit.MoveSpeed());
    }

    SDK::PredictionOutput GetPositionOnPath(SDK::PredictionInput& input, std::vector<SDK::Vector2> path, float speed = -1.0f) {
        SDK::PredictionOutput output;
        output.Input = input;

        if (!SDK::Extensions::IsDashing(input.Unit)) {
            if (input.Unit.Position().DistanceSquared(input.ResolveFrom()) < 200.0f * 200.0f) {
                speed /= 1.5f;
            }
            if (path.size() <= 1) {
                output.SetUnitPosition(input.Unit.Position());
                output.SetCastPosition(input.Unit.Position());
                // No usable path while the SDK still reports movement is uncertain,
                // not deterministic.
                output.Hitchance = input.Unit.IsMoving()
                    ? SDK::HitChance::Medium
                    : SDK::HitChance::VeryHigh;
                return output;
            }

            if (input.Unit.Spellbook().IsWindingUp() || input.Unit.Spellbook().IsCastingSpell() || input.Unit.Spellbook().IsChanneling()) {
                output.SetUnitPosition(input.Unit.Position());
                output.SetCastPosition(input.Unit.Position());
                output.Hitchance = SDK::HitChance::High;
                return output;
            }
        }

        speed = (std::abs(speed - (-1.0f)) < 0.0001f) ? input.Unit.MoveSpeed() : speed;
        float pathLength = SDK::Utils::MathUtils::PathLength(path);

        // Skillshot with delay only (speed == FLT_MAX)
        if (pathLength >= input.Delay * speed - input.RealRadius() && std::abs(input.Speed - FLT_MAX) < 0.0001f) {
                float tDistance = input.Delay * speed - input.RealRadius();

                for (std::size_t i = 0; i + 1 < path.size(); ++i) {
                    const SDK::Vector2 a = path[i];
                    const SDK::Vector2 b = path[i + 1];
                    const float d = a.Distance(b);

                    if (d >= tDistance) {
                        const SDK::Vector2 direction = (b - a).Normalized();
                        const SDK::Vector2 cp = a + direction * tDistance;
                        const float pDist = (i == path.size() - 2) ? std::min(tDistance + input.RealRadius(), d) : (tDistance + input.RealRadius());
                        const SDK::Vector2 p = a + direction * pDist;

                        output.SetCastPosition(SDK::Vector3::From2D(cp));
                        output.SetUnitPosition(SDK::Vector3::From2D(p));
                        output.Hitchance = SDK::HitChance::High;
                        return output;
                    }
                    tDistance -= d;
                }
            }

        // Skillshot with delay and speed
        if (pathLength >= input.Delay * speed - input.RealRadius() && std::abs(input.Speed - FLT_MAX) > 0.0001f) {
                float d = input.Delay * speed - input.RealRadius();
                if ((SDK::IsLineSpellType(input.Type) || SDK::IsConeSpellType(input.Type)) &&
                    input.ResolveFrom().DistanceSquared(input.Unit.Position()) < 200.0f * 200.0f) {
                    d = input.Delay * speed;
                }

                path = SDK::Utils::MathUtils::CutPath(path, std::max(0.0f, d));
                float tT = 0.0f;

                for (std::size_t i = 0; i + 1 < path.size(); ++i) {
                    SDK::Vector2 a = path[i];
                    SDK::Vector2 b = path[i + 1];
                    const float tB = a.Distance(b) / speed;
                    const SDK::Vector2 direction = (b - a).Normalized();
                    a = a - direction * (speed * tT);

                    const auto sol = SDK::Prediction::Vec2Ext::VectorMovementCollision(a, b, speed, input.ResolveFrom().To2D(), input.Speed, tT);
                    const float t = sol.CollisionTime;
                    const SDK::Vector2 pos = sol.CollisionPosition;

                    if (pos.IsValid() && t >= tT && t <= tT + tB) {
                        if (pos.DistanceSquared(b) < 20.0f) break;
                        const SDK::Vector2 p = pos + direction * input.RealRadius();

                        output.SetCastPosition(SDK::Vector3::From2D(pos));
                        output.SetUnitPosition(SDK::Vector3::From2D(p));
                        output.Hitchance = SDK::HitChance::High;
                        return output;
                    }
                    tT += tB;
                }
            }

        const SDK::Vector2 lastPoint = path.empty() ? input.Unit.Position().To2D() : path.back();
        output.SetCastPosition(SDK::Vector3::From2D(lastPoint));
        output.SetUnitPosition(SDK::Vector3::From2D(lastPoint));
        output.Hitchance = SDK::HitChance::Low;
        return output;
    }

    SDK::PredictionOutput WayPointAnalysis(SDK::PredictionOutput result, const SDK::PredictionInput& input) {
        // This pass should refine confidence, not manufacture certainty.
        if (!input.Unit.IsHero() || input.Radius == 1.0f) {
            return result;
        }

        if (input.Unit.HasBuff("Recall")) {
            result.Hitchance = SDK::HitChance::VeryHigh;
            result.SetCastPosition(input.Unit.Position());
            result.SetUnitPosition(input.Unit.Position());
            return result;
        }

        if (UnitTracker::GetLastVisibleTime(input.Unit) < 100.0) {
            result.Hitchance = std::min(result.Hitchance, SDK::HitChance::Medium);
            return result;
        }

        const auto& waypoints3D = input.Unit.CachedWaypoints();
        const float distFromToUnit = input.ResolveFrom().Distance(input.Unit.Position());
        const float speedDelay = (std::abs(input.Speed - FLT_MAX) < 0.0001f)
            ? 0.0f
            : (distFromToUnit / std::max(1.0f, input.Speed));
        const float totalDelay = speedDelay + input.Delay;

        // Actual movement lock is strong evidence. Windup alone is weaker because
        // many windups end long before a slow projectile arrives.
        if (!SDK::CanMove(input.Unit)) {
            result.Hitchance = SDK::HitChance::VeryHigh;
            return result;
        }
        if (input.Unit.Spellbook().IsWindingUp()) {
            result.Hitchance = (totalDelay <= 0.30f)
                ? std::max(result.Hitchance, SDK::HitChance::High)
                : std::min(result.Hitchance, SDK::HitChance::Medium);
            return result;
        }

        // A very recent stop is useful only for short-arrival spells. Do not aim at
        // the current position with VeryHigh confidence when the target can move
        // again long before impact.
        if (!input.Unit.IsMoving() && UnitTracker::GetLastStopMoveTime(input.Unit) < 100.0) {
            result.SetCastPosition(input.Unit.Position());
            result.SetUnitPosition(input.Unit.Position());
            result.Hitchance = (totalDelay <= 0.30f)
                ? SDK::HitChance::High
                : SDK::HitChance::Medium;
            return result;
        }

        // Fresh direction changes, click-spam and repeated reversals are all
        // uncertainty signals. They may only reduce confidence.
        const bool recentPathChange = UnitTracker::GetLastNewPathTime(input.Unit) < 140.0;
        const bool movementSpam = UnitTracker::SpamSamePlace(input.Unit);
        const bool reversing = UnitTracker::IsReversing(input.Unit);
        if (recentPathChange || movementSpam || reversing) {
            result.Hitchance = std::min(result.Hitchance, SDK::HitChance::Medium);
            return result;
        }

        if (waypoints3D.empty()) {
            result.Hitchance = input.Unit.IsMoving()
                ? std::min(result.Hitchance, SDK::HitChance::Medium)
                : std::max(result.Hitchance, SDK::HitChance::High);
            return result;
        }

        const SDK::Vector3 lastWaypoint = waypoints3D.back();
        const float distUnitToWp = lastWaypoint.Distance(input.Unit.Position());
        const float distFromToWp = lastWaypoint.Distance(input.ResolveFrom());

        // The target is about to reach the end of its current path, so extrapolating
        // beyond that endpoint is fragile.
        if (distUnitToWp > 0.0f && distUnitToWp < 75.0f + input.Radius) {
            result.Hitchance = std::min(result.Hitchance, SDK::HitChance::Medium);
            return result;
        }

        const double pathAgeMs = UnitTracker::GetLastNewPathTime(input.Unit);
        const bool stableHeading = pathAgeMs > 300.0;

        if (distUnitToWp > 0.0f && stableHeading && totalDelay <= 0.35f) {
            const SDK::Vector2 moveDir = lastWaypoint.To2D() - input.Unit.Position().To2D();
            const SDK::Vector2 toCaster = input.ResolveFrom().To2D() - input.Unit.Position().To2D();
            const float angle = SDK::Prediction::Vec2Ext::AngleBetween(moveDir, toCaster);

            // Moving almost directly toward/away from the caster is somewhat easier
            // geometrically, but it is not enough evidence for VeryHigh by itself.
            if (angle < 20.0f || angle > 160.0f) {
                result.Hitchance = std::max(result.Hitchance, SDK::HitChance::High);
            }

            int wallCount = 0;
            const SDK::Vector2 unitPos2D = input.Unit.Position().To2D();
            for (int k = 0; k < 10; ++k) {
                const float a = static_cast<float>(k) * (2.0f * 3.14159265358979323846f / 10.0f);
                const SDK::Vector2 pt = unitPos2D + SDK::Vector2(std::cos(a) * 200.0f, std::sin(a) * 200.0f);
                if (::CoreNavGrid::IsWall({ pt.x, 0.0f, pt.y })) ++wallCount;
            }

            // Nearby walls constrain options, but three sampled wall points do not
            // make a moving target deterministic.
            if (wallCount > 2) {
                result.Hitchance = std::max(result.Hitchance, SDK::HitChance::High);
            }
        }

        // Close range reduces projectile travel uncertainty. Keep it High at most;
        // never promote a normal moving target to VeryHigh from distance alone.
        if (distFromToUnit < 250.0f || input.Unit.MoveSpeed() < 250.0f) {
            result.Hitchance = std::max(result.Hitchance, SDK::HitChance::High);
        }

        // A waypoint close to the caster is actually an endpoint-risk signal, not a
        // reason to become more confident.
        if (distFromToWp < 250.0f && distUnitToWp < 250.0f) {
            result.Hitchance = std::min(result.Hitchance, SDK::HitChance::Medium);
        }

        return result;
    }

    void ApplyWallAdjust(SDK::PredictionOutput& output, const SDK::PredictionInput& input) {
        if (!input.Unit.IsHero() || input.Radius <= 1.0f ||
            output.Hitchance < SDK::HitChance::High ||
            output.Hitchance > SDK::HitChance::VeryHigh) return;

        float moveOutWall = input.Unit.BoundingRadius() + input.Radius / 2.0f + 10.0f;
        if (SDK::IsCircleSpellType(input.Type)) {
            moveOutWall = input.Unit.BoundingRadius();
        }

        const SDK::Vector3 wallPoint = GetWallPoint(output.GetCastPosition(), moveOutWall);
        if (!wallPoint.IsZero() && wallPoint.IsValid()) {
            output.Hitchance = SDK::HitChance::High;
            output.SetCastPosition(wallPoint.Extend(output.GetCastPosition(), moveOutWall));
        }
    }

    // Preliminary range check: demote confidence, never prestretch the prediction.
    void ApplyRangeChecks(SDK::PredictionOutput& output, const SDK::PredictionInput& input) {
        if (std::abs(input.Range - FLT_MAX) < 0.0001f || input.Range <= 0.0f) return;

        const SDK::Vector3 rangeFrom = input.ResolveRangeCheckFrom();
        if (output.Hitchance >= SDK::HitChance::High &&
            rangeFrom.DistanceSquared(output.GetUnitPosition()) > std::pow(input.Range + input.RealRadius() * 0.75f, 2.0f)) {
            output.Hitchance = SDK::HitChance::Medium;
        }

        ApplyFinalRangeValidation(output, input);
    }

    // Final range + geometry validation, executed as the LAST step of every path
    // (standard, dash, immobile, AoE). If the final cast/unit position lies
    // outside the spell range, the prediction is rejected — it is never
    // "rescued" by clamping the position back onto the max range.
    //
    // Exception: line spells travel along a fixed direction, so the ENDPOINT may
    // be pulled back onto the max range without changing the tested ray, but the
    // target position is then re-validated against the effective line.
    void ApplyFinalRangeValidation(SDK::PredictionOutput& output, const SDK::PredictionInput& input) {
        if (std::abs(input.Range - FLT_MAX) < 0.0001f || input.Range <= 0.0f) return;
        if (output.Hitchance <= SDK::HitChance::OutOfRange) return;

        const SDK::Vector3 rangeFrom = input.ResolveRangeCheckFrom();
        const float circleExtra = SDK::IsCircleSpellType(input.Type) ? input.RealRadius() : 0.0f;
        const float maxCastSqr = std::pow(input.Range, 2.0f);

        const float castDistSqr = rangeFrom.DistanceSquared(output.GetCastPosition());
        if (castDistSqr > maxCastSqr) {
            if (SDK::IsLineSpellType(input.Type)) {
                // Keep the tested ray, shorten the endpoint to the max range.
                const SDK::Vector2 dir2D = output.GetCastPosition().To2D() - rangeFrom.To2D();
                if (!dir2D.IsZero()) {
                    const SDK::Vector2 clamped2D = rangeFrom.To2D() + dir2D.Normalized() * input.Range;
                    output.SetCastPosition(SDK::Vector3::From2D(clamped2D));
                }
            } else {
                output.Hitchance = SDK::HitChance::OutOfRange;
                return;
            }
        }

        const float maxUnitSqr = std::pow(input.Range + circleExtra, 2.0f);
        if (rangeFrom.DistanceSquared(output.GetUnitPosition()) > maxUnitSqr) {
            output.Hitchance = SDK::HitChance::OutOfRange;
        }
    }

    SDK::Vector3 GetWallPoint(const SDK::Vector3& from, float range) {
        constexpr int count = 30;
        SDK::Vector2 circlePoints[count];

        for (int i = 0; i < count; ++i) {
            const float a = static_cast<float>(i) * (2.0f * 3.14159265358979323846f / static_cast<float>(count));
            circlePoints[i] = from.To2D() + SDK::Vector2(std::cos(a) * range, std::sin(a) * range);
        }

        SDK::Vector2 first{};
        SDK::Vector2 last{};

        for (int i = 0; i < count; ++i) {
            if (::CoreNavGrid::IsWall({ circlePoints[i].x, 0.0f, circlePoints[i].y })) {
                if (first.IsZero()) {
                    const int nextIdx = (i == count - 1) ? 0 : (i + 1);
                    if (!::CoreNavGrid::IsWall({ circlePoints[nextIdx].x, 0.0f, circlePoints[nextIdx].y })) first = circlePoints[i];
                }
                if (last.IsZero()) {
                    const int prevIdx = (i == 0) ? (count - 1) : (i - 1);
                    if (!::CoreNavGrid::IsWall({ circlePoints[prevIdx].x, 0.0f, circlePoints[prevIdx].y })) last = circlePoints[i];
                }
            }
        }

        if (!first.IsZero() && !last.IsZero()) {
            return SDK::Vector3::From2D((first + last) * 0.5f);
        }
        return {};
    }

    double UnitIsImmobileUntil(const SDK::AIBaseClient& unit) {
        const float gameTime = SDK::Game::Time();
        double maxEndTime = 0.0;

        // Build once per unit per frame (cached); every buff query this frame
        // reuses the same snapshot instead of re-scanning up to 256 buff entries.
        const auto* snapshot = CoreBuffs::GetOrBuildFrameBuffSnapshot(unit.Address(), gameTime);
        if (!snapshot) return 0.0;

        for (int i = 0; i < snapshot->count; ++i) {
            const auto& entry = snapshot->entries[i];
            if (!entry.isActive) continue;

            switch (entry.type) {
            case SDK::Prediction::BuffType::Charm:
            case SDK::Prediction::BuffType::Knockup:
            case SDK::Prediction::BuffType::Stun:
            case SDK::Prediction::BuffType::Suppression:
            case SDK::Prediction::BuffType::Snare:
            case SDK::Prediction::BuffType::Asleep:
                if (gameTime <= entry.endTime && entry.endTime > maxEndTime) {
                    maxEndTime = entry.endTime;
                }
                break;
            default:
                break;
            }
        }
        return maxEndTime - gameTime;
    }

    FsPredConfig config_;
};

} // namespace Plugins::FsPred
