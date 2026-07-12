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
#include <string>
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
    std::uint64_t rejected = 0;
    std::uint64_t noSolution = 0;
};

class PredictionEngine final : public SDK::Prediction::IPrediction {
public:
    void SetConfig(const PredictionConfig& config) {
        config_ = config;
        config_.reactionTimeMs = std::clamp(config_.reactionTimeMs, 0, 1000);
        config_.historyWindowMs = std::clamp(config_.historyWindowMs, 180, 1200);
        config_.maximumPredictionMs = std::clamp(config_.maximumPredictionMs, 250, 12000);
        config_.maximumPathSegments = std::clamp(config_.maximumPathSegments, 2, 64);
        config_.maximumRangePercent = std::clamp(config_.maximumRangePercent, 10.0f, 100.0f);
        config_.highThreshold = std::clamp(config_.highThreshold, 0.35f, 0.95f);
        config_.veryHighThreshold = std::clamp(
            config_.veryHighThreshold,
            config_.highThreshold + 0.01f,
            0.99f);
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
            aoe_.load(std::memory_order_relaxed),
            rejected_.load(std::memory_order_relaxed),
            noSolution_.load(std::memory_order_relaxed)
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
            input.Delay = std::max(0.0f, input.Delay) +
                static_cast<float>(SDK::Game::Ping()) / 2000.0f + 0.06f;
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
    std::atomic<std::uint64_t> rejected_ = 0;
    std::atomic<std::uint64_t> noSolution_ = 0;

    static int ChanceValue(SDK::HitChance value) {
        return static_cast<int>(value);
    }

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

    static bool IsFiniteRange(float range) {
        return std::isfinite(range) && range < FLT_MAX * 0.5f && range > 0.0f;
    }

    static bool IsInstant(float speed) {
        return Math::IsInstantSpeed(static_cast<double>(speed));
    }

    static SDK::PredictionOutput Empty(const SDK::PredictionInput& input,
                                       SDK::HitChance hitChance = SDK::HitChance::None) {
        SDK::PredictionOutput output;
        output.Input = input;
        output.Hitchance = hitChance;
        return output;
    }

    static SDK::PredictionOutput AtPosition(const SDK::PredictionInput& input,
                                            const Vec3& position,
                                            SDK::HitChance hitChance) {
        SDK::PredictionOutput output = Empty(input, hitChance);
        if (position.IsValid() && !position.IsZero()) {
            output.SetCastPosition(position);
            output.SetUnitPosition(position);
        }
        return output;
    }

    static double ArrivalTime(const SDK::PredictionInput& input,
                              const Math::Vector2& position) {
        const Math::Vector2 source = ToMath(input.ResolveFrom());
        if (IsInstant(input.Speed)) return std::max(0.0f, input.Delay);
        return std::max(0.0, static_cast<double>(input.Delay)) +
            Math::Distance(source, position) /
            std::max(1.0, static_cast<double>(input.Speed));
    }

    static bool IsYuumiAttached(const SDK::AIBaseClient& unit) {
        if (!unit.IsHero() || unit.CharacterName() != "Yuumi") return false;
        for (const auto& hero : SDK::GameObjects::Heroes()) {
            if (!hero.IsValid() || hero.NetworkId() == unit.NetworkId()) continue;
            if (hero.Team() != unit.Team() || !hero.IsTargetable()) continue;
            if (hero.HasBuff("YuumiWAlly") && hero.Distance(unit) <= 50.0f) return true;
        }
        return false;
    }

    SDK::PredictionOutput PredictSingle(SDK::PredictionInput input,
                                        bool checkCollision) {
        const double stasis = StateAnalyzer::RemainingStasisTime(input.Unit);
        if (IsYuumiAttached(input.Unit) || !StateAnalyzer::IsUsable(input.Unit, stasis)) {
            ++rejected_;
            return Empty(input);
        }

        const Vec3 unitPosition3D = ResolvePosition(input.Unit);
        const Vec3 source3D = input.ResolveFrom();
        if (!unitPosition3D.IsValid() || unitPosition3D.IsZero() ||
            !source3D.IsValid() || source3D.IsZero()) {
            ++rejected_;
            return Empty(input);
        }

        const Math::Vector2 unitPosition = ToMath(unitPosition3D);
        const Math::Vector2 source = ToMath(source3D);
        if (IsFiniteRange(input.Range) &&
            Math::DistanceSquared(ToMath(input.ResolveRangeCheckFrom()), unitPosition) >
                static_cast<double>(input.Range * 1.5f) * static_cast<double>(input.Range * 1.5f)) {
            ++rejected_;
            return Empty(input, SDK::HitChance::OutOfRange);
        }

        if (stasis > 0.0) {
            const double arrival = ArrivalTime(input, unitPosition);
            const double window = std::max(0.04,
                static_cast<double>(input.RealRadius()) /
                std::max(1.0, static_cast<double>(input.Unit.MoveSpeed())));
            if (std::abs(arrival - stasis) <= window) {
                ++immobile_;
                return ApplyRangeAndCollision(
                    AtPosition(input, unitPosition3D, SDK::HitChance::Immobile),
                    input,
                    checkCollision);
            }
            if (!input.Unit.IsTargetable() || (input.Unit.IsHero() && !input.Unit.IsVisible())) {
                ++rejected_;
                return Empty(input);
            }
        }

        MovementSnapshot movement = MovementTracker::Snapshot(input.Unit, config_.historyWindowMs);
        const double rebirth = StateAnalyzer::RemainingRebirthTime(input.Unit, movement);
        if (rebirth > 0.0 && (!input.Unit.IsTargetable() || input.Unit.IsDead())) {
            const double arrival = ArrivalTime(input, unitPosition);
            if (arrival <= rebirth + 0.12) {
                ++immobile_;
                return ApplyRangeAndCollision(
                    AtPosition(input, unitPosition3D, SDK::HitChance::Immobile),
                    input,
                    checkCollision);
            }
        }
        if (!movement.valid) {
            ++rejected_;
            return Empty(input);
        }

        LimitPath(movement.path);
        if (SDK::Extensions::IsDashing(input.Unit)) {
            SDK::PredictionOutput dashOutput = PredictDash(input, movement, checkCollision);
            if (ChanceValue(dashOutput.Hitchance) == ChanceValue(SDK::HitChance::Dash)) {
                return dashOutput;
            }
        }

        const double immobile = StateAnalyzer::RemainingImmobileTime(input.Unit);
        if (immobile > 0.0) {
            const double arrival = ArrivalTime(input, unitPosition);
            const double escapeWindow = static_cast<double>(input.RealRadius()) /
                std::max(1.0, static_cast<double>(input.Unit.MoveSpeed()));
            if (arrival <= immobile + escapeWindow) {
                ++immobile_;
                return ApplyRangeAndCollision(
                    AtPosition(input, unitPosition3D, SDK::HitChance::Immobile),
                    input,
                    checkCollision);
            }
        }

        const double effectiveSpeed = StateAnalyzer::EffectiveMoveSpeed(input.Unit, movement, config_);
        const double approximateTravelTime = ArrivalTime(input, unitPosition);
        const double requiredCommitment = MovementPatternAnalyzer::RequiredDirectionCommitment(
            approximateTravelTime,
            movement.directionReversalCount);
        const bool historyLimited = input.Unit.IsHero() &&
            (!movement.historyReliable || movement.positionDiscontinuity);
        if (movement.moving && historyLimited &&
            movement.directionStableSeconds < requiredCommitment && movement.path.empty()) {
            ++rejected_;
            return Empty(input, SDK::HitChance::Low);
        }

        MovementPatternMetrics metrics{
            movement.directionStability,
            movement.speedStability,
            movement.displacementEfficiency,
            movement.pathChangesPerSecond,
            movement.directionReversalsPerSecond,
            movement.directionReversalCount,
            movement.pathAgeSeconds,
            movement.angularVelocity
        };
        MovementModelPolicy policy = MovementPatternAnalyzer::Evaluate(metrics);
        if (!movement.moving) {
            policy.pathWeight = 0.0;
            policy.accelerationWeight = 0.0;
            policy.velocityWeight = 0.0;
            policy.velocityScale = 0.0;
            policy.centerPull = 0.0;
        }

        const Math::Vector2 modelPosition = movement.moving
            ? MovementPatternAnalyzer::StabilizedPosition(unitPosition, movement.recentCenter, policy)
            : unitPosition;
        const Math::Vector2 modelVelocity = movement.moving
            ? MovementPatternAnalyzer::StabilizedVelocity(movement.velocity, policy)
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
        if (config_.usePathHistory && movement.path.size() > 1 && effectiveSpeed > 1.0) {
            pathIntercept = Math::SolvePathIntercept(
                source,
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

        Math::InterceptSolution accelerationIntercept;
        const double accelerationMagnitude = movement.acceleration.Length();
        const bool accelerationUsable = config_.useAcceleration && movement.moving &&
            movement.directionReversalCount < 2 && policy.jukeScore < 0.55 &&
            movement.directionStability >= 0.45 && movement.displacementEfficiency >= 0.35 &&
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
                    movement.acceleration,
                    projectileSpeed,
                    launchDelay,
                    MaximumTime());
        }

        Math::InterceptSolution intercept = SelectIntercept(
            pathIntercept,
            velocityIntercept,
            accelerationIntercept,
            policy,
            config_.usePathHistory,
            accelerationUsable,
            std::max(160.0, static_cast<double>(input.RealRadius()) * 2.5));

        if (!intercept.valid || !intercept.position.IsFinite()) {
            ++noSolution_;
            SDK::PredictionOutput fallback = AtPosition(
                input,
                unitPosition3D,
                movement.moving ? SDK::HitChance::Low : SDK::HitChance::Medium);
            return ApplyRangeAndCollision(std::move(fallback), input, checkCollision);
        }

        const double maximumDisplacement = std::max(
            static_cast<double>(input.RealRadius()) + 35.0,
            effectiveSpeed * std::max(0.0, intercept.time) * policy.displacementScale + 35.0);
        intercept.position = MovementPatternAnalyzer::ClampDisplacement(
            unitPosition,
            intercept.position,
            maximumDisplacement);

        if (StateAnalyzer::IsWall(intercept.position, unitPosition3D.y)) {
            const Math::Vector2 earlier = MovementPatternAnalyzer::ClampDisplacement(
                unitPosition,
                Math::PositionOnPath(movement.path, effectiveSpeed,
                    std::max(0.0, intercept.time - 0.10)),
                maximumDisplacement);
            if (!StateAnalyzer::IsWall(earlier, unitPosition3D.y)) {
                intercept.position = earlier;
            } else {
                intercept.position = unitPosition;
            }
        }

        double wallRestriction = 0.0;
        if (config_.useWallAnalysis) {
            const double escapeRadius = std::max(
                60.0,
                static_cast<double>(input.RealRadius()) +
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
        confidence.historyLimited = historyLimited;

        SDK::PredictionOutput output = Empty(
            input,
            ConfidenceEvaluator::ToHitChance(ConfidenceEvaluator::Score(confidence), config_));
        Math::Vector2 castPosition = intercept.position;
        if (input.ChoiceCloserPosition && movement.moving && !modelVelocity.IsZero()) {
            castPosition -= modelVelocity.Normalized() *
                std::min(static_cast<double>(input.RealRadius()) * 0.35, 45.0) *
                (1.0 - policy.jukeScore);
        }
        output.SetCastPosition(ToWorld(castPosition, unitPosition3D.y));
        output.SetUnitPosition(ToWorld(intercept.position, unitPosition3D.y));
        return ApplyRangeAndCollision(std::move(output), input, checkCollision);
    }

    SDK::PredictionOutput PredictDash(SDK::PredictionInput input,
                                      const MovementSnapshot& movement,
                                      bool checkCollision) {
        (void)movement;
        const auto dashInfo = SDK::Extensions::GetDashInfo(input.Unit);
        if (!dashInfo.IsDash) return Empty(input);

        const Vec3 current3D = ResolvePosition(input.Unit);
        std::vector<Math::Vector2> path;
        const Math::Vector2 current = ToMath(current3D);
        if (current.IsFinite()) path.push_back(current);
        for (int index = 0; index < dashInfo.PathCount; ++index) {
            const Math::Vector2 point = ToMath(dashInfo.Path[index]);
            if (point.IsFinite() && (path.empty() || Math::DistanceSquared(path.back(), point) > 4.0)) {
                path.push_back(point);
            }
        }
        if (path.size() == 1 && dashInfo.EndPos.IsValid()) {
            path.push_back(ToMath(dashInfo.EndPos));
        }
        if (path.size() <= 1) return Empty(input);

        const double dashSpeed = std::max(1.0, static_cast<double>(dashInfo.Speed));
        const double projectileSpeed = IsInstant(input.Speed)
            ? std::numeric_limits<double>::infinity()
            : static_cast<double>(input.Speed);
        const Math::InterceptSolution intercept = Math::SolvePathIntercept(
            ToMath(input.ResolveFrom()),
            path,
            dashSpeed,
            projectileSpeed,
            std::max(0.0f, input.Delay),
            MaximumTime());
        const int now = SDK::Variables::TickCount();
        const double remaining = dashInfo.IsBlink
            ? 0.12
            : (dashInfo.EndTick > now
                ? static_cast<double>(dashInfo.EndTick - now) / 1000.0
                : Math::PathLength(path) / dashSpeed);
        if (!intercept.valid || intercept.time > remaining + 0.08) return Empty(input);

        SDK::PredictionOutput output = AtPosition(
            input,
            ToWorld(intercept.position, current3D.y),
            SDK::HitChance::Dash);
        ++dash_;
        return ApplyRangeAndCollision(std::move(output), input, checkCollision);
    }

    SDK::PredictionOutput PredictAoe(SDK::PredictionInput input,
                                     bool checkCollision) {
        SDK::PredictionInput singleInput = input;
        singleInput.AoE = false;
        SDK::PredictionOutput main = PredictSingle(singleInput, false);
        main.Input = input;
        if (ChanceValue(main.Hitchance) < ChanceValue(SDK::HitChance::Medium) || !input.Unit.IsHero()) {
            return ApplyRangeAndCollision(std::move(main), input, checkCollision);
        }

        const Math::Vector2 source = ToMath(input.ResolveFrom());
        const bool finiteRange = IsFiniteRange(input.Range);
        const double searchRange = finiteRange
            ? static_cast<double>(input.Range + input.RealRadius() + 250.0f)
            : 5000.0;
        const int primaryId = input.Unit.NetworkId();
        std::vector<AoePoint> points;
        std::unordered_map<int, SDK::AIHeroClient> heroes;
        points.push_back({primaryId, ToMath(main.GetUnitPosition()), true});
        heroes.emplace(primaryId, SDK::AIHeroClient(input.Unit.Handle()));

        for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
            if (!hero.IsValid() || hero.IsDead() || !hero.IsVisible() ||
                hero.NetworkId() == primaryId) continue;
            if (Math::Distance(source, ToMath(ResolvePosition(hero))) > searchRange) continue;
            SDK::PredictionInput extraInput = singleInput;
            extraInput.Unit = hero;
            const SDK::PredictionOutput predicted = PredictSingle(extraInput, false);
            if (ChanceValue(predicted.Hitchance) < ChanceValue(SDK::HitChance::Medium)) continue;
            points.push_back({hero.NetworkId(), ToMath(predicted.GetUnitPosition()), false});
            heroes.emplace(hero.NetworkId(), hero);
        }

        if (points.size() <= 1) return ApplyRangeAndCollision(std::move(main), input, checkCollision);
        const double range = finiteRange ? static_cast<double>(input.Range) : 5000.0;
        AoeSolution solution;
        if (SDK::IsCircleSpellType(input.Type)) {
            solution = AoeOptimizer::Circle(source, points, input.RealRadius(), range, primaryId);
        } else if (SDK::IsConeSpellType(input.Type)) {
            solution = AoeOptimizer::Cone(source, points, ConeAngleRadians(input), range, primaryId);
        } else if (SDK::IsLineSpellType(input.Type)) {
            solution = AoeOptimizer::Line(source, points, input.RealRadius(), range, primaryId);
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

    double ConeAngleRadians(const SDK::PredictionInput& input) const {
        double degrees = 45.0;
        if (input.Spell) {
            const std::string spellName = input.Spell->Instance().Name();
            if (const auto* data = SDK::SpellDatabase::GetByName(spellName)) {
                if (data->Angle > 1 && data->Angle < 180) degrees = static_cast<double>(data->Angle);
            }
        }
        return std::clamp(degrees, 10.0, 170.0) * Math::Pi / 180.0;
    }

    SDK::PredictionOutput ApplyRangeAndCollision(SDK::PredictionOutput output,
                                                  const SDK::PredictionInput& input,
                                                  bool checkCollision) {
        const bool finiteRange = IsFiniteRange(input.Range);
        if (finiteRange && ChanceValue(output.Hitchance) > ChanceValue(SDK::HitChance::OutOfRange)) {
            const Vec3 rangeFrom = input.ResolveRangeCheckFrom();
            const Vec3 unitPosition = output.GetUnitPosition();
            const double allowed = static_cast<double>(input.Range) *
                std::clamp(static_cast<double>(config_.maximumRangePercent) / 100.0, 0.10, 1.0) +
                (SDK::IsCircleSpellType(input.Type) ? input.RealRadius() : 0.0f);
            if (!rangeFrom.IsValid() || rangeFrom.Distance2D(unitPosition) > allowed) {
                output.Hitchance = SDK::HitChance::OutOfRange;
                return output;
            }

            const Vec3 castPosition = output.GetCastPosition();
            if (rangeFrom.Distance2D(castPosition) > input.Range &&
                ChanceValue(output.Hitchance) > ChanceValue(SDK::HitChance::OutOfRange)) {
                const Math::Vector2 from = ToMath(rangeFrom);
                const Math::Vector2 cast = ToMath(castPosition);
                const Math::Vector2 delta = cast - from;
                if (!delta.IsZero()) {
                    output.SetCastPosition(ToWorld(
                        from + delta.Normalized() * static_cast<double>(input.Range),
                        castPosition.y));
                }
            }
        }

        if (checkCollision && config_.useCollision && input.Collision &&
            ChanceValue(output.Hitchance) > ChanceValue(SDK::HitChance::OutOfRange)) {
            const std::vector<Vec3> positions = {
                output.GetCastPosition(),
                output.GetUnitPosition(),
                ResolvePosition(input.Unit)
            };
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
        const Math::InterceptSolution* best = nullptr;
        double bestWeight = -1.0;
        for (Candidate candidate : candidates) {
            if (candidate.weight <= 0.0 || !candidate.solution ||
                !candidate.solution->valid || !candidate.solution->position.IsFinite()) continue;
            double weight = candidate.weight;
            if (reference && candidate.solution != reference) {
                const double divergence = Math::Distance(
                    candidate.solution->position,
                    reference->position);
                if (divergence > divergenceLimit) {
                    weight *= std::max(0.05, divergenceLimit / divergence);
                }
            }
            if (!best || weight > bestWeight) {
                best = candidate.solution;
                bestWeight = weight;
            }
        }
        return best ? *best : Math::InterceptSolution{};
    }

    void LimitPath(std::vector<Math::Vector2>& path) const {
        const std::size_t maximum = static_cast<std::size_t>(
            std::max(2, config_.maximumPathSegments));
        if (path.size() > maximum) path.resize(maximum);
    }

    double MaximumTime() const {
        return std::clamp(static_cast<double>(config_.maximumPredictionMs) / 1000.0, 0.25, 12.0);
    }
};

}
