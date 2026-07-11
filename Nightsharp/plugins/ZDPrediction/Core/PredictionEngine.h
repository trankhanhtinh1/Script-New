#pragma once

#include "PredictionConfig.h"
#include "../Analysis/AoeOptimizer.h"
#include "../Analysis/ConfidenceEvaluator.h"
#include "../Analysis/MovementPatternAnalyzer.h"
#include "../Analysis/StateAnalyzer.h"
#include "../Math/Kinematics.h"
#include "../Tracking/MovementTracker.h"
#include "../../../sdk/SDK.h"

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ZDPrediction {

struct PredictionStatistics {
    std::uint64_t total = 0;
    std::uint64_t dash = 0;
    std::uint64_t immobile = 0;
    std::uint64_t collision = 0;
    std::uint64_t aoe = 0;
};

class PredictionEngine final : public SDK::Prediction::IPrediction {
public:
    void SetConfig(const PredictionConfig& config) {
        config_ = config;
    }

    PredictionConfig GetConfig() const {
        return config_;
    }

    PredictionStatistics Statistics() const {
        return {
            total_.load(std::memory_order_relaxed),
            dash_.load(std::memory_order_relaxed),
            immobile_.load(std::memory_order_relaxed),
            collision_.load(std::memory_order_relaxed),
            aoe_.load(std::memory_order_relaxed)
        };
    }

    SDK::PredictionOutput GetPrediction(SDK::PredictionInput input) override {
        return GetPrediction(input, true, true);
    }

    SDK::PredictionOutput GetPrediction(SDK::PredictionInput input,
                                        bool firstTime,
                                        bool checkCollision) override {
        ++total_;
        if (firstTime) {
            input.Delay += static_cast<float>(SDK::Game::Ping()) / 2000.0f + 0.033f;
        }
        if (input.AoE && config_.useAoe) return PredictAoe(input, checkCollision);
        return PredictSingle(input, checkCollision);
    }

private:
    PredictionConfig config_;
    std::atomic<std::uint64_t> total_ = 0;
    std::atomic<std::uint64_t> dash_ = 0;
    std::atomic<std::uint64_t> immobile_ = 0;
    std::atomic<std::uint64_t> collision_ = 0;
    std::atomic<std::uint64_t> aoe_ = 0;

    static Math::Vector2 ToMath(const Vec2& point) {
        return {static_cast<double>(point.x), static_cast<double>(point.y)};
    }

    static Math::Vector2 ToMath(const Vec3& point) {
        return ToMath(point.To2D());
    }

    static Vec3 ToWorld(const Math::Vector2& point, float height) {
        return Vec3::From2D(Vec2(static_cast<float>(point.x), static_cast<float>(point.y)), height);
    }

    static Vec3 ResolvePosition(const SDK::AIBaseClient& unit) {
        Vec3 position = unit.ServerPosition();
        if (!position.IsValid() || position.IsZero()) position = unit.Position();
        return position;
    }

    static bool IsInstant(float speed) {
        return !std::isfinite(speed) || speed >= FLT_MAX * 0.5f || speed >= 1e12f;
    }

    static bool IsUsable(const SDK::AIBaseClient& unit, double stasisTime) {
        if (!unit.IsValid() || (unit.IsDead() && !unit.IsZombie())) return false;
        const Vec3 position = ResolvePosition(unit);
        if (!position.IsValid() || position.IsZero()) return false;
        if (unit.IsHero() && !unit.IsVisible()) return false;
        return unit.IsTargetable() || stasisTime > 0.0;
    }

    SDK::PredictionOutput Empty(const SDK::PredictionInput& input,
                                SDK::HitChance hitChance = SDK::HitChance::None) const {
        SDK::PredictionOutput output;
        output.Input = input;
        output.Hitchance = hitChance;
        return output;
    }

    double ArrivalTime(const SDK::PredictionInput& input,
                       const Math::Vector2& position) const {
        if (IsInstant(input.Speed)) return std::max(0.0f, input.Delay);
        return std::max(0.0f, input.Delay) +
            Math::Distance(ToMath(input.ResolveFrom()), position) /
            std::max(1.0, static_cast<double>(input.Speed));
    }

    SDK::PredictionOutput PredictSingle(SDK::PredictionInput input,
                                        bool checkCollision) {
        const double stasis = StateAnalyzer::RemainingStasisTime(input.Unit);
        if (!IsUsable(input.Unit, stasis)) return Empty(input);

        const Vec3 unitPosition3D = ResolvePosition(input.Unit);
        const Vec3 source3D = input.ResolveFrom();
        if (!source3D.IsValid() || source3D.IsZero()) return Empty(input);
        const Math::Vector2 unitPosition = ToMath(unitPosition3D);
        const Math::Vector2 source = ToMath(source3D);
        const bool usesPlayerSource = !input.From.IsValid() || input.From.IsZero();
        if (usesPlayerSource) {
            const SDK::AIHeroClient player = SDK::ObjectManager::Player();
            const double playerSpeed = player.IsValid()
                ? static_cast<double>(player.MoveSpeed())
                : 0.0;
            if (MovementTracker::ObserveSourcePosition(source, playerSpeed, 120)) {
                return Empty(input);
            }
        }
        if (stasis > 0.0) {
            const double arrival = ArrivalTime(input, unitPosition);
            const double window = std::max(0.04, static_cast<double>(input.RealRadius()) /
                std::max(1.0, static_cast<double>(input.Unit.MoveSpeed())));
            if (std::abs(arrival - stasis) <= window) {
                SDK::PredictionOutput output = Empty(input, SDK::HitChance::Immobile);
                output.SetCastPosition(unitPosition3D);
                output.SetUnitPosition(unitPosition3D);
                ++immobile_;
                return ApplyRangeAndCollision(std::move(output), input, checkCollision);
            }
            return Empty(input);
        }

        MovementSnapshot movement = MovementTracker::Snapshot(input.Unit, config_.historyWindowMs);
        if (!movement.valid) return Empty(input);
        LimitPath(movement.path);

        const double immobile = StateAnalyzer::RemainingImmobileTime(input.Unit);
        if (immobile > 0.0) {
            const double arrival = ArrivalTime(input, unitPosition);
            const double escapeWindow = static_cast<double>(input.RealRadius()) /
                std::max(1.0, static_cast<double>(input.Unit.MoveSpeed()));
            if (arrival <= immobile + escapeWindow) {
                SDK::PredictionOutput output = Empty(input, SDK::HitChance::Immobile);
                output.SetCastPosition(unitPosition3D);
                output.SetUnitPosition(unitPosition3D);
                ++immobile_;
                return ApplyRangeAndCollision(std::move(output), input, checkCollision);
            }
        }

        if (SDK::Extensions::IsDashing(input.Unit)) {
            SDK::PredictionOutput dashOutput = PredictDash(input, movement, checkCollision);
            if (dashOutput.Hitchance == SDK::HitChance::Dash) return dashOutput;
        }

        if (input.Unit.IsHero() &&
            (movement.positionDiscontinuity || !movement.historyReliable)) {
            return Empty(input);
        }

        const double effectiveSpeed = StateAnalyzer::EffectiveMoveSpeed(input.Unit, movement, config_);
        const double approximateTravelTime = ArrivalTime(input, unitPosition);
        const double requiredDirectionCommitment =
            MovementPatternAnalyzer::RequiredDirectionCommitment(
                approximateTravelTime,
                movement.directionReversalCount);
        if (movement.directionStableSeconds < requiredDirectionCommitment) {
            return Empty(input);
        }
        const MovementPatternMetrics patternMetrics = {
            movement.directionStability,
            movement.speedStability,
            movement.displacementEfficiency,
            movement.pathChangesPerSecond,
            movement.directionReversalsPerSecond,
            movement.directionReversalCount,
            movement.pathAgeSeconds,
            movement.angularVelocity
        };
        MovementModelPolicy modelPolicy = MovementPatternAnalyzer::Evaluate(patternMetrics);
        if (!movement.moving) {
            modelPolicy.pathWeight = 0.0;
            modelPolicy.accelerationWeight = 0.0;
            modelPolicy.velocityScale = 0.0;
            modelPolicy.centerPull = 0.0;
        }
        const Math::Vector2 modelPosition = movement.moving
            ? MovementPatternAnalyzer::StabilizedPosition(
                unitPosition, movement.recentCenter, modelPolicy)
            : unitPosition;
        const Math::Vector2 modelVelocity = movement.moving
            ? MovementPatternAnalyzer::StabilizedVelocity(movement.velocity, modelPolicy)
            : Math::Vector2{};
        if (movement.path.empty()) movement.path.push_back(unitPosition);
        if (movement.path.size() == 1 && movement.velocity.Length() > 20.0) {
            movement.path.push_back(unitPosition + movement.velocity.Normalized() *
                std::max(300.0, effectiveSpeed * MaximumTime()));
        }

        const double projectileSpeed = IsInstant(input.Speed)
            ? std::numeric_limits<double>::infinity()
            : static_cast<double>(input.Speed);
        const double launchDelay = std::max(0.0f, input.Delay);
        Math::InterceptSolution pathIntercept;
        if (movement.path.size() > 1 && effectiveSpeed > 1.0) {
            pathIntercept = Math::SolvePathIntercept(source,
                                                     movement.path,
                                                     effectiveSpeed,
                                                     projectileSpeed,
                                                     launchDelay,
                                                     MaximumTime());
        }
        const Math::InterceptSolution velocityIntercept = Math::SolveLinearIntercept(
            source,
            modelPosition,
            modelVelocity,
            projectileSpeed,
            launchDelay,
            MaximumTime());

        const double accelerationMagnitude = movement.acceleration.Length();
        Math::InterceptSolution accelerationIntercept;
        const bool accelerationUsable = config_.useAcceleration && movement.moving &&
            movement.directionReversalCount < 2 &&
            modelPolicy.jukeScore < 0.55 && movement.directionStability >= 0.45 &&
            movement.displacementEfficiency >= 0.35 &&
            accelerationMagnitude >= 30.0 && accelerationMagnitude <= 900.0;
        if (accelerationUsable) {
            const bool turning = movement.velocity.Length() > 40.0 &&
                std::abs(movement.angularVelocity) >= 0.08 &&
                std::abs(movement.angularVelocity) <= 4.0;
            accelerationIntercept = turning
                ? Math::SolveTurnIntercept(
                    source,
                    modelPosition,
                    modelVelocity,
                    movement.angularVelocity,
                    projectileSpeed,
                    launchDelay,
                    MaximumTime())
                : Math::SolveAcceleratedIntercept(
                    source,
                    modelPosition,
                    modelVelocity,
                    movement.acceleration * modelPolicy.velocityScale,
                    projectileSpeed,
                    launchDelay,
                    MaximumTime());
        }

        Math::InterceptSolution intercept = SelectIntercept(
            pathIntercept,
            velocityIntercept,
            accelerationIntercept,
            modelPolicy,
            config_.usePathHistory,
            accelerationUsable,
            std::max(160.0, static_cast<double>(input.RealRadius()) * 2.5));

        if (!intercept.valid || !intercept.position.IsFinite()) {
            return Empty(input, SDK::HitChance::Low);
        }

        const double maximumDisplacement = std::max(
            static_cast<double>(input.RealRadius()) + 35.0,
            effectiveSpeed * std::max(0.0, intercept.time) *
                modelPolicy.displacementScale + 35.0);
        intercept.position = MovementPatternAnalyzer::ClampDisplacement(
            unitPosition, intercept.position, maximumDisplacement);

        if (SDK::NavMesh::IsWall(ToWorld(intercept.position, unitPosition3D.y))) {
            const Math::Vector2 pathFallback = Math::PositionOnPath(
                movement.path, effectiveSpeed, std::max(0.0, intercept.time - 0.10));
            const Math::Vector2 earlier = MovementPatternAnalyzer::ClampDisplacement(
                unitPosition, pathFallback, maximumDisplacement);
            if (!SDK::NavMesh::IsWall(ToWorld(earlier, unitPosition3D.y))) intercept.position = earlier;
        }

        double wallRestriction = 0.0;
        if (config_.useWallAnalysis) {
            const double escapeRadius = std::max(60.0, static_cast<double>(input.RealRadius()) +
                effectiveSpeed * static_cast<double>(config_.reactionTimeMs) / 1000.0);
            wallRestriction = StateAnalyzer::WallRestriction(intercept.position, escapeRadius);
        }

        ConfidenceContext confidence;
        confidence.unit = &input.Unit;
        confidence.movement = &movement;
        confidence.source = source;
        confidence.predicted = intercept.position;
        confidence.travelTime = intercept.time;
        confidence.effectiveMoveSpeed = effectiveSpeed;
        confidence.radius = std::max(1.0f, input.RealRadius());
        confidence.reactionTime = static_cast<double>(config_.reactionTimeMs) / 1000.0;
        confidence.wallRestriction = wallRestriction;
        confidence.instantProjectile = IsInstant(input.Speed);
        confidence.jukeScore = modelPolicy.jukeScore;

        const double confidenceScore = ConfidenceEvaluator::Score(confidence);
        SDK::PredictionOutput output = Empty(
            input, ConfidenceEvaluator::ToHitChance(confidenceScore, config_));
        Math::Vector2 castPosition = intercept.position;
        if (input.ChoiceCloserPosition && movement.moving && !modelVelocity.IsZero()) {
            castPosition -= modelVelocity.Normalized() *
                std::min(static_cast<double>(input.RealRadius()) * 0.35, 45.0) *
                    (1.0 - modelPolicy.jukeScore);
        }
        output.SetCastPosition(ToWorld(castPosition, unitPosition3D.y));
        output.SetUnitPosition(ToWorld(intercept.position, unitPosition3D.y));
        return ApplyRangeAndCollision(std::move(output), input, checkCollision);
    }

    SDK::PredictionOutput PredictDash(SDK::PredictionInput input,
                                      const MovementSnapshot& movement,
                                      bool checkCollision) {
        const auto dashInfo = SDK::Extensions::GetDashInfo(input.Unit);
        if (!dashInfo.IsDash || dashInfo.Speed <= 1.0f) return Empty(input);

        std::vector<Math::Vector2> path;
        path.push_back(ToMath(ResolvePosition(input.Unit)));
        for (int index = 0; index < dashInfo.PathCount; ++index) {
            const Math::Vector2 point = ToMath(dashInfo.Path[index]);
            if (path.empty() || Math::DistanceSquared(path.back(), point) > 4.0) path.push_back(point);
        }
        if (path.size() == 1 && dashInfo.EndPos.IsValid()) path.push_back(ToMath(dashInfo.EndPos));
        if (path.size() <= 1) return Empty(input);

        const Math::InterceptSolution intercept = Math::SolvePathIntercept(
            ToMath(input.ResolveFrom()),
            path,
            dashInfo.Speed,
            IsInstant(input.Speed) ? std::numeric_limits<double>::infinity() : input.Speed,
            std::max(0.0f, input.Delay),
            MaximumTime());
        const double remaining = dashInfo.EndTick > 0
            ? std::max(0.0, static_cast<double>(dashInfo.EndTick - SDK::Variables::TickCount()) / 1000.0)
            : Math::PathLength(path) / dashInfo.Speed;
        if (!intercept.valid || intercept.time > remaining + 0.08) return Empty(input);

        const Vec3 current = ResolvePosition(input.Unit);
        SDK::PredictionOutput output = Empty(input, SDK::HitChance::Dash);
        output.SetCastPosition(ToWorld(intercept.position, current.y));
        output.SetUnitPosition(ToWorld(intercept.position, current.y));
        ++dash_;
        return ApplyRangeAndCollision(std::move(output), input, checkCollision);
    }

    SDK::PredictionOutput PredictAoe(SDK::PredictionInput input,
                                     bool checkCollision) {
        SDK::PredictionInput singleInput = input;
        singleInput.AoE = false;
        SDK::PredictionOutput main = PredictSingle(singleInput, false);
        main.Input = input;
        if (main.Hitchance < SDK::HitChance::Medium || !input.Unit.IsHero()) return main;

        const Math::Vector2 source = ToMath(input.ResolveFrom());
        std::vector<AoePoint> points;
        std::unordered_map<int, SDK::AIHeroClient> heroes;
        const bool finiteRange = std::isfinite(input.Range) && input.Range < FLT_MAX * 0.5f;
        const double searchRange = finiteRange
            ? static_cast<double>(input.Range + input.RealRadius() + 250.0f)
            : 5000.0;

        for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
            if (!hero.IsValid() || hero.IsDead() || !hero.IsVisible()) continue;
            if (Math::Distance(source, ToMath(ResolvePosition(hero))) > searchRange) continue;
            SDK::PredictionOutput predicted;
            if (hero.NetworkId() == input.Unit.NetworkId()) {
                predicted = main;
            } else {
                SDK::PredictionInput extraInput = singleInput;
                extraInput.Unit = hero;
                predicted = PredictSingle(extraInput, false);
            }
            if (predicted.Hitchance < SDK::HitChance::Medium) continue;
            points.push_back({hero.NetworkId(), ToMath(predicted.GetUnitPosition())});
            heroes.emplace(hero.NetworkId(), hero);
        }
        if (points.size() <= 1) return ApplyRangeAndCollision(std::move(main), input, checkCollision);

        const double range = finiteRange ? input.Range : 5000.0;
        AoeSolution solution;
        if (SDK::IsCircleSpellType(input.Type)) {
            solution = AoeOptimizer::Circle(source, points, input.RealRadius(), range);
        } else if (SDK::IsConeSpellType(input.Type)) {
            const double angleDegrees = std::clamp(static_cast<double>(input.Radius), 10.0, 160.0);
            solution = AoeOptimizer::Cone(source, points, angleDegrees * Math::Pi / 180.0, range);
        } else if (SDK::IsLineSpellType(input.Type)) {
            solution = AoeOptimizer::Line(source, points, input.RealRadius(), range);
        }
        if (!solution.valid || solution.hitIds.size() <= 1) {
            return ApplyRangeAndCollision(std::move(main), input, checkCollision);
        }

        main.SetCastPosition(ToWorld(solution.castPosition, main.GetCastPosition().y));
        main.AoeTargetsHit.clear();
        for (const int id : solution.hitIds) {
            const auto iterator = heroes.find(id);
            if (iterator != heroes.end()) main.AoeTargetsHit.push_back(iterator->second);
        }
        main.AoeTargetsHitCount = static_cast<int>(main.AoeTargetsHit.size());
        ++aoe_;
        return ApplyRangeAndCollision(std::move(main), input, checkCollision);
    }

    SDK::PredictionOutput ApplyRangeAndCollision(SDK::PredictionOutput output,
                                                  const SDK::PredictionInput& input,
                                                  bool checkCollision) {
        const bool finiteRange = std::isfinite(input.Range) && input.Range < FLT_MAX * 0.5f;
        if (finiteRange && output.Hitchance > SDK::HitChance::OutOfRange) {
            const double allowed = static_cast<double>(input.Range) *
                std::clamp(static_cast<double>(config_.maximumRangePercent) / 100.0, 0.1, 1.0) +
                (SDK::IsCircleSpellType(input.Type) ? input.RealRadius() : 0.0f);
            if (input.ResolveRangeCheckFrom().Distance2D(output.GetUnitPosition()) > allowed) {
                output.Hitchance = SDK::HitChance::OutOfRange;
                return output;
            }
        }

        if (checkCollision && config_.useCollision && input.Collision &&
            output.Hitchance > SDK::HitChance::None) {
            std::vector<Vec3> positions = {output.GetCastPosition()};
            std::vector<SDK::AIBaseClient> objects = SDK::Collision::GetCollision(positions, input);
            objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const auto& object) {
                return !object.IsValid() || object.NetworkId() == input.Unit.NetworkId();
            }), objects.end());
            if (static_cast<float>(objects.size()) > input.MaxCollisionCount) {
                output.CollisionObjects = objects;
                output.SetOriginHitchance(output.Hitchance);
                output.Hitchance = SDK::HitChance::Collision;
                ++collision_;
            }
        }
        return output;
    }

    static Math::InterceptSolution SelectIntercept(
        const Math::InterceptSolution& path,
        const Math::InterceptSolution& velocity,
        const Math::InterceptSolution& acceleration,
        const MovementModelPolicy& policy,
        bool allowPath,
        bool allowAcceleration,
        double divergenceLimit) {
        struct Candidate {
            const Math::InterceptSolution* solution = nullptr;
            double weight = 0.0;
        };
        Candidate candidates[3] = {
            {&path, allowPath ? policy.pathWeight : 0.0},
            {&velocity, policy.velocityWeight},
            {&acceleration, allowAcceleration ? policy.accelerationWeight : 0.0}
        };
        const Math::InterceptSolution* reference = velocity.valid ? &velocity :
            ((allowPath && path.valid) ? &path : nullptr);
        Candidate best;
        for (Candidate candidate : candidates) {
            if (candidate.weight <= 0.0 || !candidate.solution || !candidate.solution->valid ||
                !candidate.solution->position.IsFinite()) continue;
            if (reference && candidate.solution != reference) {
                const double divergence = Math::Distance(
                    candidate.solution->position, reference->position);
                if (divergence > divergenceLimit) {
                    candidate.weight *= std::max(0.05, divergenceLimit / divergence);
                }
            }
            if (!best.solution || candidate.weight > best.weight) best = candidate;
        }
        return best.solution ? *best.solution : Math::InterceptSolution{};
    }

    void LimitPath(std::vector<Math::Vector2>& path) const {
        const std::size_t maximum = static_cast<std::size_t>(
            std::max(2, config_.maximumPathSegments));
        if (path.size() > maximum) path.resize(maximum);
    }

    double MaximumTime() const {
        return std::clamp(static_cast<double>(config_.maximumPredictionMs) / 1000.0, 0.5, 12.0);
    }
};

}
