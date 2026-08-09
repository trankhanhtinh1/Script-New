#pragma once

#include "FsPredAoe.h"
#include "FsPredAimMath.h"
#include "FsPredMotionState.h"
#include "FsPredPolicy.h"
#include "FsPredUnitTracker.h"

#include "../../../sdk/Extensions/AIBaseClientExtensions.h"
#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/Math/Prediction/Movement.h"
#include "../../../sdk/Wrappers/Spells/Spell.h"
#include "../../../sdk/Math/Collision.h"
#include "../../../sdk/Utils/MathUtils.h"
#include "../../../core/CoreNavGrid.h"
#include "../../../core/CoreBuffs.h"
#include "../../../SectionProfiler.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <limits>
#include <vector>

namespace Plugins::FsPred {

struct FsPredDebugPrediction {
    std::uint32_t NetworkId = 0;
    int SlotIndex = -1;
    int Tick = 0;
    SDK::Vector3 CastPosition{};
    SDK::Vector3 UnitPosition{};
    SDK::HitChance Hitchance = SDK::HitChance::None;
};

class FsPredEngine final : public SDK::Prediction::IPrediction {
public:
    FsPredEngine() = default;

    void SetConfig(const FsPredConfig& config) {
        if (!config.RecordDebugPredictions &&
            config_.RecordDebugPredictions) {
            ClearDebugPredictions();
        }
        config_ = config;
    }

    const FsPredConfig& Config() const {
        return config_;
    }

    const std::array<FsPredDebugPrediction, 40>&
    DebugPredictions() const {
        return debugPredictions_;
    }

    void ClearDebugPredictions() {
        debugPredictions_ = {};
    }

    SDK::PredictionOutput GetPrediction(SDK::PredictionInput input) override {
        return GetPrediction(input, true, true);
    }

    SDK::PredictionOutput GetPrediction(
        SDK::PredictionInput input,
        bool ft,
        bool checkCollision) override {
        NS_PROFILE("FsPred.GetPrediction");
        if (!SDK::Prediction::Movement::IsPredictionTargetUsable(input.Unit)) {
            SDK::PredictionOutput empty;
            empty.Input = input;
            return empty;
        }
        UnitTracker::Update();

        if (config_.UseDefaultSdk) {
            if (auto* sdkPred = SDK::Prediction::GetSDKPrediction()) {
                return sdkPred->GetPrediction(input, ft, checkCollision);
            }
        }

        ResolveImplicitCastOrigin(input);
        const int slotIndex = input.Spell
            ? ChampionSlotIndex(input.Spell->Slot)
            : -1;
        const ResolvedPredictionPolicy policy =
            ResolvePredictionPolicy(config_, slotIndex);
        ApplyInputCalibration(input, policy.Calibration);

        const int hardRangePercent =
            std::clamp(config_.HardRangePercent, 0, 100);
        if (hardRangePercent < 100 &&
            input.Range > 0.0f &&
            input.Range != FLT_MAX) {
            input.Range *= static_cast<float>(hardRangePercent) / 100.0f;
        }

        const SDK::Vector3 unitPosition = ServerPositionOrPosition(input.Unit);
        if (input.Range != FLT_MAX &&
            unitPosition.DistanceSqr2D(input.ResolveRangeCheckFrom()) >
                std::pow(input.Range * 1.5f, 2.0f)) {
            SDK::PredictionOutput empty;
            empty.Input = input;
            return empty;
        }

        input.Delay = AimMath::EffectiveDelay(
            input.Delay,
            SDK::Game::Ping(),
            config_.GlobalExtraDelayMs,
            ft);

        SDK::PredictionOutput output;
        if (ft && input.AoE) {
            output = AoePrediction::GetPrediction(
                input,
                [this, policy](
                    SDK::PredictionInput candidate,
                    bool,
                    bool) {
                    return this->GetPredictionInternal(
                        candidate,
                        policy);
                },
                policy.Weights.AoeSecondaryMinimum);
        } else {
            output = GetPredictionInternal(input, policy);
        }
        output = FinalizePrediction(
            std::move(output),
            input,
            checkCollision);
        RecordDebugPrediction(output, input, slotIndex);
        return output;
    }

private:
    static SDK::Vector3 ServerPositionOrPosition(
        const SDK::AIBaseClient& unit) {
        if (!unit.IsValid()) {
            return {};
        }

        const SDK::Vector3 serverPosition = unit.ServerPosition();
        const SDK::Vector3 clientPosition = unit.Position();
        const bool isCasting =
            unit.Spellbook().IsWindingUp() ||
            unit.Spellbook().IsCastingSpell() ||
            unit.Spellbook().IsChanneling();
        const bool isDashing = SDK::Extensions::IsDashing(unit);
        const bool isMoving = unit.IsMoving();
        if (!isMoving || isCasting || isDashing) {
            return AimMath::SelectHybridUnitPosition(
                serverPosition,
                clientPosition,
                isMoving,
                isCasting,
                isDashing,
                std::span<const SDK::Vector3>{});
        }

        const auto& path = unit.CachedWaypoints();
        return AimMath::SelectHybridUnitPosition(
            serverPosition,
            clientPosition,
            isMoving,
            false,
            false,
            std::span<const SDK::Vector3>(path.data(), path.size()));
    }

    static void ResolveImplicitCastOrigin(
        SDK::PredictionInput& input) {
        if (input.From.IsValid() && !input.From.IsZero()) {
            return;
        }

        const SDK::AIHeroClient player = SDK::GameObjects::Player();
        if (!player.IsValid()) {
            return;
        }
        input.From = AimMath::SelectCastOrigin(
            input.From,
            player.Position(),
            player.ServerPosition());
    }

    static void ApplyInputCalibration(
        SDK::PredictionInput& input,
        const SlotCalibration& calibration) {
        const SpellCalibrationValues calibrated = ApplySlotCalibration(
            { input.Delay, input.Speed, input.Radius },
            calibration);
        input.Delay = calibrated.DelaySeconds;
        input.Speed = calibrated.Speed;
        input.Radius = calibrated.Radius;
    }

    SDK::PredictionOutput GetPredictionInternal(
        SDK::PredictionInput input,
        const ResolvedPredictionPolicy& policy) {
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

        if ((output.Hitchance == SDK::HitChance::High ||
             output.Hitchance == SDK::HitChance::VeryHigh) &&
            config_.RecheckHitchance &&
            policy.AntiBaitEnabled) {
            output = WayPointAnalysis(
                std::move(output),
                input,
                policy);
        }
        return output;
    }

    SDK::PredictionOutput FinalizePrediction(
        SDK::PredictionOutput output,
        const SDK::PredictionInput& input,
        bool checkCollision) {
        output.Input = input;
        ApplyFinalRangeValidation(output, input);

        if (!checkCollision ||
            !input.Collision ||
            output.Hitchance == SDK::HitChance::None ||
            output.Hitchance == SDK::HitChance::OutOfRange) {
            return output;
        }

        output.CollisionObjects = SDK::Collision::GetCollision(
            output.GetCastPosition(),
            input);
        if (SDK::Collision::ExceedsCollisionAllowance(
                output.CollisionObjects.size(),
                input.MaxCollisionCount)) {
            output.SetOriginHitchance(output.Hitchance);
            output.Hitchance = SDK::HitChance::Collision;
        }
        return output;
    }

    void RecordDebugPrediction(
        const SDK::PredictionOutput& output,
        const SDK::PredictionInput& input,
        int slotIndex) {
        if (!config_.RecordDebugPredictions ||
            !input.Unit.IsValid() ||
            !output.GetCastPosition().IsValid() ||
            output.GetCastPosition().IsZero() ||
            !output.GetUnitPosition().IsValid() ||
            output.GetUnitPosition().IsZero()) {
            return;
        }

        const std::uint32_t networkId = input.Unit.NetworkId();
        FsPredDebugPrediction* selected = nullptr;
        for (auto& record : debugPredictions_) {
            if (record.NetworkId == networkId &&
                record.SlotIndex == slotIndex) {
                selected = &record;
                break;
            }
            if (!selected && record.NetworkId == 0) {
                selected = &record;
            }
        }
        if (!selected) {
            selected = &*std::min_element(
                debugPredictions_.begin(),
                debugPredictions_.end(),
                [](const FsPredDebugPrediction& left,
                   const FsPredDebugPrediction& right) {
                    return left.Tick < right.Tick;
                });
        }

        *selected = {
            networkId,
            slotIndex,
            SDK::Variables::TickCount(),
            output.GetCastPosition(),
            output.GetUnitPosition(),
            output.Hitchance
        };
    }

    struct PathCursor {
        SDK::Vector2 Position{};
        std::size_t NextIndex = 0;
        bool HasRoute = false;
    };

    static std::size_t FirstRemainingWaypoint(
        const SDK::Vector3& start,
        std::span<const SDK::Vector3> path) {
        if (path.empty()) {
            return 0;
        }

        const SDK::Vector2 start2D = start.To2D();
        float bestDistance = std::numeric_limits<float>::max();
        std::size_t bestIndex = 0;
        SDK::Vector2 previous{};
        bool hasPrevious = false;
        for (std::size_t index = 0; index < path.size(); ++index) {
            if (!path[index].IsValid()) {
                continue;
            }
            const SDK::Vector2 point = path[index].To2D();
            const float pointDistance = point.DistanceSqr(start2D);
            if (pointDistance < bestDistance) {
                bestDistance = pointDistance;
                bestIndex = index;
            }
            if (hasPrevious) {
                const float segmentDistance = DistanceSquaredToSegment(
                    start2D,
                    previous,
                    point);
                if (segmentDistance < bestDistance) {
                    bestDistance = segmentDistance;
                    bestIndex = index;
                }
            }
            previous = point;
            hasPrevious = true;
        }
        return bestIndex;
    }

    static float RemainingPathLength(
        const SDK::Vector3& start,
        std::span<const SDK::Vector3> path,
        std::size_t firstIndex) {
        SDK::Vector2 current = start.To2D();
        float length = 0.0f;
        for (std::size_t index = firstIndex; index < path.size(); ++index) {
            if (!path[index].IsValid()) {
                continue;
            }
            const SDK::Vector2 waypoint = path[index].To2D();
            length += current.Distance(waypoint);
            current = waypoint;
        }
        return length;
    }

    static PathCursor CutPathDistance(
        const SDK::Vector3& start,
        std::span<const SDK::Vector3> path,
        std::size_t firstIndex,
        float distance) {
        PathCursor cursor{};
        cursor.Position = start.To2D();
        cursor.NextIndex = firstIndex;
        float remaining = std::max(0.0f, distance);

        for (std::size_t index = firstIndex; index < path.size(); ++index) {
            if (!path[index].IsValid()) {
                continue;
            }
            const SDK::Vector2 waypoint = path[index].To2D();
            const float segmentLength = cursor.Position.Distance(waypoint);
            if (segmentLength <= 1.0e-3f) {
                cursor.Position = waypoint;
                cursor.NextIndex = index + 1;
                continue;
            }

            cursor.HasRoute = true;
            if (remaining < segmentLength) {
                cursor.Position = cursor.Position +
                    (waypoint - cursor.Position).Normalized() * remaining;
                cursor.NextIndex = index;
                return cursor;
            }
            remaining -= segmentLength;
            cursor.Position = waypoint;
            cursor.NextIndex = index + 1;
        }
        return cursor;
    }

    static SDK::Vector3 LastValidPathPosition(
        const SDK::Vector3& fallback,
        std::span<const SDK::Vector3> path) {
        SDK::Vector3 result = fallback;
        for (const SDK::Vector3& waypoint : path) {
            if (waypoint.IsValid() && !waypoint.IsZero()) {
                result = waypoint;
            }
        }
        return result;
    }

    static float ProjectileArrivalTime(
        const SDK::PredictionInput& input,
        const SDK::Vector3& position) {
        const auto travel = AimMath::ProjectileTravelTime(
            input.ResolveFrom().Distance2D(position),
            input.Speed);
        return travel.has_value()
            ? std::max(0.0f, input.Delay) + *travel
            : std::numeric_limits<float>::infinity();
    }

    SDK::PredictionOutput GetDashingPrediction(SDK::PredictionInput& input) {
        constexpr int kDashEndToleranceMs = 80;

        SDK::PredictionOutput output;
        output.Input = input;
        const auto dash = SDK::Extensions::GetDashInfo(input.Unit);
        const SDK::Vector3 start = ServerPositionOrPosition(input.Unit);
        const int pathCount = std::clamp(dash.PathCount, 0, 32);
        const std::span<const SDK::Vector3> dashPath(dash.Path, pathCount);
        const SDK::Vector3 end = LastValidPathPosition(
            dash.EndPos.IsValid() && !dash.EndPos.IsZero()
                ? dash.EndPos
                : start,
            dashPath);

        if (!std::isfinite(dash.Speed) || dash.Speed <= 0.0f ||
            dashPath.empty()) {
            output.SetCastPosition(end);
            output.SetUnitPosition(end);
            output.Hitchance = SDK::HitChance::Medium;
            return output;
        }

        SDK::PredictionOutput pathPrediction = GetPositionOnPath(
            input,
            dashPath,
            dash.Speed);
        const float arrival = ProjectileArrivalTime(
            input,
            pathPrediction.GetCastPosition());

        float dashRemaining = 0.0f;
        const int now = SDK::Variables::TickCount();
        if (dash.EndTick > now) {
            dashRemaining =
                static_cast<float>(dash.EndTick - now) / 1000.0f;
        } else {
            const std::size_t first = FirstRemainingWaypoint(start, dashPath);
            dashRemaining = RemainingPathLength(start, dashPath, first) /
                dash.Speed;
        }

        if (AimMath::ArrivesDuringDash(
                arrival,
                dashRemaining,
                kDashEndToleranceMs) &&
            pathPrediction.Hitchance >= SDK::HitChance::Medium) {
            pathPrediction.Hitchance = SDK::HitChance::Dash;
            return pathPrediction;
        }

        output.SetCastPosition(end);
        output.SetUnitPosition(end);
        output.Hitchance = SDK::HitChance::Medium;
        return output;
    }

    SDK::PredictionOutput GetImmobilePrediction(
        const SDK::PredictionInput& input,
        double remainingImmobileT) {
        SDK::PredictionOutput output;
        output.Input = input;

        const SDK::Vector3 position = ServerPositionOrPosition(input.Unit);
        const float arrival = ProjectileArrivalTime(input, position);
        if (!std::isfinite(arrival)) {
            output.SetCastPosition(position);
            output.SetUnitPosition(position);
            output.Hitchance = SDK::HitChance::Medium;
            return output;
        }

        const double radiusWindow =
            static_cast<double>(input.RealRadius()) /
            static_cast<double>(std::max(1.0f, input.Unit.MoveSpeed()));
        if (AimMath::IsImmobileAtImpact(
                arrival,
                remainingImmobileT,
                radiusWindow)) {
            output.SetCastPosition(position);
            output.SetUnitPosition(position);
            output.Hitchance = SDK::HitChance::Immobile;
            return output;
        }

        const float freeMovementSeconds = static_cast<float>(std::max(
            0.0,
            static_cast<double>(arrival) - remainingImmobileT));
        const auto& waypoints = input.Unit.CachedWaypoints();
        const auto advance = AimMath::AdvancePath(
            position,
            input.Unit.MoveSpeed(),
            freeMovementSeconds,
            std::span<const SDK::Vector3>(
                waypoints.data(),
                waypoints.size()));
        const SDK::Vector3 leadPosition =
            advance.HasRoute ? advance.Position : position;
        output.SetCastPosition(leadPosition);
        output.SetUnitPosition(leadPosition);
        output.Hitchance =
            advance.HasRoute && advance.ReachedRequestedDistance
            ? SDK::HitChance::High
            : SDK::HitChance::Medium;
        return output;
    }

    SDK::PredictionOutput GetStandardPrediction(SDK::PredictionInput& input) {
        const auto& waypoints = input.Unit.CachedWaypoints();
        return GetPositionOnPath(
            input,
            std::span<const SDK::Vector3>(
                waypoints.data(),
                waypoints.size()),
            input.Unit.MoveSpeed());
    }

    SDK::PredictionOutput GetPositionOnPath(
        SDK::PredictionInput& input,
        std::span<const SDK::Vector3> path,
        float speed = -1.0f) {
        SDK::PredictionOutput output;
        output.Input = input;
        const SDK::Vector3 serverPosition =
            ServerPositionOrPosition(input.Unit);
        const bool isDashing = SDK::Extensions::IsDashing(input.Unit);

        speed = std::abs(speed + 1.0f) < 0.0001f
            ? input.Unit.MoveSpeed()
            : speed;
        if (!isDashing &&
            serverPosition.DistanceSqr2D(input.ResolveFrom()) <
                250.0f * 250.0f) {
            speed *= 1.5f;
        }
        if (!std::isfinite(speed) || speed <= 0.0f) {
            output.SetUnitPosition(serverPosition);
            output.SetCastPosition(serverPosition);
            output.Hitchance = SDK::HitChance::Medium;
            return output;
        }

        // A completed short click can leave the old nav suffix visible for one
        // frame. Never consume that stale route after movement has stopped.
        if (!isDashing && !input.Unit.IsMoving()) {
            output.SetUnitPosition(serverPosition);
            output.SetCastPosition(serverPosition);
            output.Hitchance = SDK::HitChance::High;
            return output;
        }

        const std::size_t firstWaypoint =
            FirstRemainingWaypoint(serverPosition, path);
        const float pathLength =
            RemainingPathLength(serverPosition, path, firstWaypoint);
        if (pathLength <= 1.0f) {
            output.SetUnitPosition(serverPosition);
            output.SetCastPosition(serverPosition);
            output.Hitchance = input.Unit.IsMoving()
                ? SDK::HitChance::Medium
                : SDK::HitChance::VeryHigh;
            return output;
        }

        if (!isDashing &&
            (input.Unit.Spellbook().IsWindingUp() ||
             input.Unit.Spellbook().IsCastingSpell() ||
             input.Unit.Spellbook().IsChanneling())) {
            output.SetUnitPosition(serverPosition);
            output.SetCastPosition(serverPosition);
            output.Hitchance = SDK::HitChance::High;
            return output;
        }

        const float delay = std::max(0.0f, input.Delay);
        const float realRadius = std::isfinite(input.RealRadius())
            ? std::max(0.0f, input.RealRadius())
            : 0.0f;

        if (input.Speed == FLT_MAX) {
            const float castDistance =
                std::max(0.0f, delay * speed - realRadius);
            if (castDistance <= pathLength) {
                const PathCursor cast = CutPathDistance(
                    serverPosition,
                    path,
                    firstWaypoint,
                    castDistance);
                const PathCursor unit = CutPathDistance(
                    serverPosition,
                    path,
                    firstWaypoint,
                    std::min(pathLength, castDistance + realRadius));
                output.SetCastPosition(SDK::Vector3::From2D(
                    cast.Position,
                    serverPosition.y));
                output.SetUnitPosition(SDK::Vector3::From2D(
                    unit.Position,
                    serverPosition.y));
                output.Hitchance = SDK::HitChance::High;
                return output;
            }
        } else {
            if (!std::isfinite(input.Speed) || input.Speed <= 0.0f) {
                output.SetUnitPosition(serverPosition);
                output.SetCastPosition(serverPosition);
                output.Hitchance = SDK::HitChance::Medium;
                return output;
            }

            float cutDistance =
                std::max(0.0f, delay * speed - realRadius);
            if ((SDK::IsLineSpellType(input.Type) ||
                 SDK::IsConeSpellType(input.Type)) &&
                input.ResolveFrom().DistanceSqr2D(serverPosition) <
                    200.0f * 200.0f) {
                cutDistance = delay * speed;
            }

            if (cutDistance <= pathLength) {
                PathCursor cursor = CutPathDistance(
                    serverPosition,
                    path,
                    firstWaypoint,
                    cutDistance);
                float segmentStartTime = 0.0f;
                for (std::size_t index = cursor.NextIndex;
                     index < path.size();
                     ++index) {
                    if (!path[index].IsValid()) {
                        continue;
                    }
                    const SDK::Vector2 segmentEnd = path[index].To2D();
                    const float segmentLength =
                        cursor.Position.Distance(segmentEnd);
                    if (segmentLength <= 1.0e-3f) {
                        cursor.Position = segmentEnd;
                        continue;
                    }

                    const float segmentDuration = segmentLength / speed;
                    const SDK::Vector2 direction =
                        (segmentEnd - cursor.Position).Normalized();
                    const SDK::Vector2 movementOrigin =
                        cursor.Position -
                        direction * (speed * segmentStartTime);
                    const auto solution =
                        SDK::Prediction::Vec2Ext::VectorMovementCollision(
                            movementOrigin,
                            segmentEnd,
                            speed,
                            input.ResolveFrom().To2D(),
                            input.Speed,
                            segmentStartTime);
                    const float contactTime = solution.CollisionTime;
                    const SDK::Vector2 contactPosition =
                        solution.CollisionPosition;
                    if (contactPosition.IsValid() &&
                        contactTime >= segmentStartTime &&
                        contactTime <=
                            segmentStartTime + segmentDuration) {
                        const float contactProgress =
                            (contactPosition - cursor.Position).Dot(direction);
                        constexpr float kSegmentTolerance = 1.0e-2f;
                        if (std::isfinite(contactProgress) &&
                            contactProgress >= -kSegmentTolerance &&
                            contactProgress <=
                                segmentLength + kSegmentTolerance) {
                            const SDK::Vector2 boundedContact =
                                cursor.Position +
                                direction * std::clamp(
                                    contactProgress,
                                    0.0f,
                                    segmentLength);
                            if (boundedContact.DistanceSqr(segmentEnd) <
                                20.0f * 20.0f) {
                                break;
                            }
                            output.SetCastPosition(SDK::Vector3::From2D(
                                boundedContact,
                                serverPosition.y));
                            output.SetUnitPosition(SDK::Vector3::From2D(
                                boundedContact -
                                    direction * realRadius,
                                serverPosition.y));
                            output.Hitchance = SDK::HitChance::High;
                            return output;
                        }
                    }

                    segmentStartTime += segmentDuration;
                    cursor.Position = segmentEnd;
                }
            }
        }

        const SDK::Vector3 lastPosition =
            LastValidPathPosition(serverPosition, path);
        output.SetCastPosition(lastPosition);
        output.SetUnitPosition(lastPosition);
        output.Hitchance = SDK::HitChance::Medium;
        return output;
    }

    static float TerrainEscapeOpenFraction(
        const SDK::PredictionInput& input,
        const SDK::Vector3& impactPosition,
        const PredictionHorizonFacts& horizon,
        const AntiBaitWeights& weights) {
        if (!weights.TerrainCorridorBoost ||
            !impactPosition.IsValid() ||
            impactPosition.IsZero() ||
            !std::isfinite(horizon.ArrivalSeconds) ||
            !std::isfinite(horizon.MoveSpeed) ||
            horizon.MoveSpeed <= 0.0f) {
            return 1.0f;
        }

        const float reactionSeconds =
            static_cast<float>(std::clamp(
                weights.ReactionFloorMs,
                100,
                250)) / 1000.0f;
        const float steeringDistance =
            std::max(0.0f, horizon.ArrivalSeconds - reactionSeconds) *
            horizon.MoveSpeed;
        const float requiredEscape = std::max(
            100.0f,
            std::max(0.0f, horizon.EffectiveRadius) * 1.25f);
        if (steeringDistance < requiredEscape) {
            return 1.0f;
        }
        const float sampleDistance = std::min(
            steeringDistance,
            std::max(300.0f, requiredEscape));

        const CoreNavGrid::GridRef grid = CoreNavGrid::Get();
        if (!grid.IsValid()) {
            return 1.0f;
        }

        const auto blocked = [&](const SDK::Vector2& direction) {
            const SDK::Vector3 endpoint = SDK::Vector3::From2D(
                impactPosition.To2D() + direction * sampleDistance,
                impactPosition.y);
            return grid.IsWallBetween(
                       impactPosition,
                       endpoint,
                       15.0f) ||
                   !grid.IsWalkable(endpoint);
        };

        int sampleCount = 0;
        int blockedCount = 0;
        if (SDK::IsCircleSpellType(input.Type)) {
            constexpr float kDiagonal = 0.70710678118f;
            const std::array<SDK::Vector2, 8> kDirections{
                SDK::Vector2{ 1.0f, 0.0f },
                SDK::Vector2{ kDiagonal, kDiagonal },
                SDK::Vector2{ 0.0f, 1.0f },
                SDK::Vector2{ -kDiagonal, kDiagonal },
                SDK::Vector2{ -1.0f, 0.0f },
                SDK::Vector2{ -kDiagonal, -kDiagonal },
                SDK::Vector2{ 0.0f, -1.0f },
                SDK::Vector2{ kDiagonal, -kDiagonal }
            };
            sampleCount = static_cast<int>(kDirections.size());
            for (const SDK::Vector2& direction : kDirections) {
                if (blocked(direction)) {
                    ++blockedCount;
                }
            }
        } else if (SDK::IsLineSpellType(input.Type) ||
                   SDK::IsConeSpellType(input.Type)) {
            const SDK::Vector2 projectileDirection =
                (impactPosition - input.ResolveFrom()).To2D().Normalized();
            if (projectileDirection.IsZero()) {
                return 1.0f;
            }
            const SDK::Vector2 lateral{
                -projectileDirection.y,
                projectileDirection.x
            };
            sampleCount = 2;
            blockedCount =
                static_cast<int>(blocked(lateral)) +
                static_cast<int>(blocked(lateral * -1.0f));
        } else {
            return 1.0f;
        }
        return EscapeOpenFraction(sampleCount, blockedCount);
    }

    static SDK::PredictionOutput WayPointAnalysis(
        SDK::PredictionOutput result,
        const SDK::PredictionInput& input,
        const ResolvedPredictionPolicy& policy) {
        if (!input.Unit.IsHero() ||
            input.Radius == 1.0f ||
            result.Hitchance < SDK::HitChance::Low) {
            return result;
        }

        // Recheck confidence without mutating the already bounded geometry.
        const MotionFacts facts = UnitTracker::GetMotionFacts(input.Unit);
        SDK::HitChance confidence = ClassifyMotion(
            facts,
            policy.Weights,
            policy.AntiBaitEnabled);
        const SDK::Vector3 serverPosition =
            ServerPositionOrPosition(input.Unit);
        const SDK::Vector3 castPosition = result.GetCastPosition();
        const float projectileDistance =
            input.ResolveFrom().Distance2D(castPosition);
        const auto projectileTravel = AimMath::ProjectileTravelTime(
            projectileDistance,
            input.Speed);
        const float arrival = ProjectileArrivalTime(
            input,
            result.GetUnitPosition());
        const float effectiveRadius = input.RealRadius();

        if (facts.IsCasting &&
            (!std::isfinite(arrival) || arrival > 0.30f)) {
            confidence = std::min(confidence, SDK::HitChance::Medium);
        }

        PredictionHorizonFacts horizon{};
        horizon.ArrivalSeconds = arrival;
        horizon.ProjectileTravelSeconds = projectileTravel.value_or(
            std::numeric_limits<float>::infinity());
        horizon.CastDistance =
            input.ResolveRangeCheckFrom().Distance2D(castPosition);
        horizon.SpellRange = input.Range;
        horizon.MoveSpeed = input.Unit.MoveSpeed();
        horizon.EffectiveRadius = effectiveRadius;
        const float openEscapeFraction = TerrainEscapeOpenFraction(
            input,
            result.GetUnitPosition(),
            horizon,
            policy.Weights);
        confidence = ApplyPredictionHorizonConfidence(
            confidence,
            facts,
            horizon,
            policy.Weights,
            policy.AntiBaitEnabled,
            policy.IgnoreMaxRangePenalty,
            openEscapeFraction);

        const auto& path = input.Unit.CachedWaypoints();
        if (!path.empty()) {
            const SDK::Vector3 endpoint =
                LastValidPathPosition(
                    serverPosition,
                    std::span<const SDK::Vector3>(path.data(), path.size()));
            const float endpointThreshold =
                75.0f + std::max(0.0f, effectiveRadius);
            if (serverPosition.DistanceSqr2D(endpoint) <=
                endpointThreshold * endpointThreshold) {
                confidence = std::min(
                    confidence,
                    SDK::HitChance::Medium);
            }
        }

        result.Hitchance = confidence;
        return result;
    }

    static float DistanceSquaredToSegment(
        const SDK::Vector2& point,
        const SDK::Vector2& start,
        const SDK::Vector2& end) {
        const SDK::Vector2 segment = end - start;
        const float lengthSquared = segment.LengthSqr();
        if (lengthSquared <= 1.0e-6f) {
            return point.DistanceSqr(start);
        }
        const float projection = std::clamp(
            (point - start).Dot(segment) / lengthSquared,
            0.0f,
            1.0f);
        return point.DistanceSqr(start + segment * projection);
    }

    static bool ClampLineEndpointToCastRange(
        const SDK::Vector3& originalEndpoint,
        const SDK::Vector3& projectileFrom,
        const SDK::Vector3& rangeFrom,
        float range,
        SDK::Vector3& clampedEndpoint) {
        const SDK::Vector2 ray = originalEndpoint.To2D() - projectileFrom.To2D();
        const float rayLength = ray.Length();
        if (rayLength <= 1.0e-4f) {
            return false;
        }

        const SDK::Vector2 direction = ray / rayLength;
        const SDK::Vector2 offset = projectileFrom.To2D() - rangeFrom.To2D();
        const float along = offset.Dot(direction);
        const float discriminant =
            along * along - (offset.LengthSqr() - range * range);
        if (discriminant < 0.0f) {
            return false;
        }

        const float exitDistance = -along + std::sqrt(discriminant);
        if (exitDistance < 0.0f) {
            return false;
        }
        const float clampedDistance = std::min(rayLength, exitDistance);
        clampedEndpoint = SDK::Vector3::From2D(
            projectileFrom.To2D() + direction * clampedDistance,
            originalEndpoint.y);
        return clampedEndpoint.IsValid() && !clampedEndpoint.IsZero();
    }

    // Establishes the final finite spell geometry. No caller may mutate either
    // position after this returns; collision consumes this exact cast endpoint.
    void ApplyFinalRangeValidation(
        SDK::PredictionOutput& output,
        const SDK::PredictionInput& input) {
        if (output.Hitchance == SDK::HitChance::None ||
            output.Hitchance == SDK::HitChance::OutOfRange) {
            return;
        }

        SDK::Vector3 castPosition = output.GetCastPosition();
        const SDK::Vector3 unitPosition = output.GetUnitPosition();
        if (!castPosition.IsValid() || castPosition.IsZero() ||
            !unitPosition.IsValid() || unitPosition.IsZero()) {
            output.Hitchance = SDK::HitChance::None;
            return;
        }

        const float realRadius = input.RealRadius();
        if (!std::isfinite(realRadius) || realRadius < 0.0f) {
            output.Hitchance = SDK::HitChance::None;
            return;
        }

        if (input.Range != FLT_MAX && input.Range > 0.0f) {
            const SDK::Vector3 rangeFrom = input.ResolveRangeCheckFrom();
            if (output.Hitchance >= SDK::HitChance::High &&
                rangeFrom.DistanceSqr2D(unitPosition) >
                    std::pow(input.Range + realRadius * 0.75f, 2.0f)) {
                output.Hitchance = SDK::HitChance::Medium;
            }

            if (rangeFrom.DistanceSqr2D(castPosition) >
                input.Range * input.Range) {
                if (!SDK::IsLineSpellType(input.Type) ||
                    !ClampLineEndpointToCastRange(
                        castPosition,
                        input.ResolveFrom(),
                        rangeFrom,
                        input.Range,
                        castPosition)) {
                    output.Hitchance = SDK::HitChance::OutOfRange;
                    return;
                }
                output.SetCastPosition(castPosition);
            }

            const float rangeExtension =
                SDK::IsCircleSpellType(input.Type) ? realRadius : 0.0f;
            if (rangeFrom.DistanceSqr2D(unitPosition) >
                std::pow(input.Range + rangeExtension, 2.0f)) {
                output.Hitchance = SDK::HitChance::OutOfRange;
                return;
            }
        }

        if (SDK::IsLineSpellType(input.Type) &&
            DistanceSquaredToSegment(
                unitPosition.To2D(),
                input.ResolveFrom().To2D(),
                castPosition.To2D()) >
                realRadius * realRadius + 1.0e-3f) {
            output.Hitchance = SDK::HitChance::OutOfRange;
            return;
        }
        if (SDK::IsCircleSpellType(input.Type) &&
            unitPosition.DistanceSqr2D(castPosition) >
                realRadius * realRadius + 1.0e-3f) {
            output.Hitchance = SDK::HitChance::OutOfRange;
        }
    }

    static double UnitIsImmobileUntil(const SDK::AIBaseClient& unit) {
        const float gameTime = SDK::Game::Time();
        const auto* snapshot =
            CoreBuffs::GetOrBuildFrameBuffSnapshot(
                unit.Address(),
                gameTime);
        return RemainingImmobilitySeconds(snapshot, gameTime);
    }

    FsPredConfig config_;
    std::array<FsPredDebugPrediction, 40> debugPredictions_{};
};

} // namespace Plugins::FsPred
