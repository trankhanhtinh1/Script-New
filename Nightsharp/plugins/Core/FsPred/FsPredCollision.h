#pragma once

#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/GameObjects/ObjectManager.h"
#include "../../../sdk/Math/Collision.h"
#include "../../../sdk/Math/HealthPrediction.h"
#include "../../../sdk/Math/Prediction/Movement.h"
#include "../../../sdk/Utils/MathUtils.h"
#include "../../../core/CoreNavGrid.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Plugins::FsPred {

class Collision {
public:
    // Cheap predicted unit position along its current path at impact time.
    // Used for collision checks ONLY — avoids re-running the full prediction
    // pipeline (buff enumeration, waypoint analysis, range checks, ...) for
    // every minion/hero near the ray on every frame. Uses the frame-cached
    // waypoints so a collision pass costs zero heap allocations.
    static SDK::Vector2 PredictUnitPosition(const SDK::PredictionInput& input) {
        if (!input.Unit.IsValid()) return {};

        float travel = input.Delay;
        if (std::abs(input.Speed - FLT_MAX) > 0.0001f) {
            travel += (input.Unit.Position().Distance(input.ResolveFrom())) / std::max(1.0f, input.Speed);
        }

        const auto& wps = input.Unit.CachedWaypoints();
        if (wps.empty()) {
            return input.Unit.Position().To2D();
        }

        SDK::Vector2 from = wps[0].To2D();
        for (std::size_t i = 0; i + 1 < wps.size(); ++i) {
            const SDK::Vector2 to = wps[i + 1].To2D();
            const float d = from.Distance(to);
            if (travel < d) {
                return from + (to - from).Normalized() * travel;
            }
            travel -= d;
            from = to;
        }
        return from;
    }

    static std::vector<SDK::AIBaseClient> GetCollision(
        const std::vector<SDK::Vector3>& positions,
        const SDK::PredictionInput& input) {

        std::vector<SDK::AIBaseClient> result;
        const auto player = SDK::ObjectManager::Player();

        // Fetch the zero-allocation per-frame snapshots once per call.
        const auto& enemyMinions = SDK::GameObjects::EnemyMinionsFrame();
        const auto& neutrals = SDK::GameObjects::MinionsFrame();
        const auto& enemyHeroes = SDK::GameObjects::EnemyHeroesFrame();

        for (const auto& v : positions) {
            const float checkRange = std::min(input.Range + input.Radius + 100.0f, 2000.0f);

            for (const auto objectType : input.CollisionObjects) {
                if (objectType == SDK::CollisionableObjects::Minions) {
                    // Enemy Minions
                    for (const auto& minion : enemyMinions) {
                        if (!minion.IsValid() || minion.IsAlly()) continue;
                        if (!SDK::Extensions::IsValidTarget(minion, checkRange, true, input.ResolveRangeCheckFrom())) continue;

                        const float distance = minion.Position().Distance(input.ResolveFrom());
                        SDK::PredictionInput minionInput = input;
                        minionInput.Unit = minion;

                        const SDK::Vector2 pred2D = PredictUnitPosition(minionInput);
                        const float distSqr = DistancedSqr(pred2D, input.ResolveFrom().To2D(), v.To2D(), true);

                        const float extra = minion.IsMoving() ? 50.0f : 15.0f;
                        const float maxDistSqr = std::pow(input.Radius + extra + minion.BoundingRadius(), 2.0f);

                        if (distSqr <= maxDistSqr && !MinionIsDead(input, minion, distance)) {
                            result.push_back(minion);
                        }
                    }

                    // Neutral Minions / Jungle
                    for (const auto& minion : neutrals) {
                        if (!minion.IsValid() || minion.IsAlly() || minion.IsEnemy()) continue;
                        if (!SDK::Extensions::IsValidTarget(minion, checkRange, true, input.ResolveRangeCheckFrom())) continue;

                        SDK::PredictionInput minionInput = input;
                        minionInput.Unit = minion;

                        const SDK::Vector2 pred2D = PredictUnitPosition(minionInput);
                        const float distSqr = DistancedSqr(pred2D, input.ResolveFrom().To2D(), v.To2D(), true);

                        const float maxDistSqr = std::pow(input.Radius + 15.0f + minion.BoundingRadius(), 2.0f);
                        if (distSqr <= maxDistSqr) {
                            result.push_back(minion);
                        }
                    }
                } else if (objectType == SDK::CollisionableObjects::Heroes) {
                    for (const auto& hero : enemyHeroes) {
                        if (!hero.IsValid()) continue;
                        if (!SDK::Extensions::IsValidTarget(hero, checkRange, true, input.ResolveRangeCheckFrom())) continue;

                        SDK::PredictionInput heroInput = input;
                        heroInput.Unit = hero;

                        const SDK::Vector2 pred2D = PredictUnitPosition(heroInput);
                        const float distSqr = DistancedSqr(pred2D, input.ResolveFrom().To2D(), v.To2D(), true);

                        const float maxDistSqr = std::pow(input.Radius + 50.0f + hero.BoundingRadius(), 2.0f);
                        if (distSqr <= maxDistSqr) {
                            result.push_back(hero);
                        }
                    }
                } else if (objectType == SDK::CollisionableObjects::Walls) {
                    const SDK::Vector2 from2D = input.ResolveFrom().To2D();
                    const SDK::Vector2 v2D = v.To2D();
                    const float step = from2D.Distance(v2D) / 20.0f;
                    if (step > 0.0f) {
                        for (int j = 0; j < 20; ++j) {
                            const SDK::Vector2 point = from2D + (v2D - from2D).Normalized() * (step * static_cast<float>(j));
                            if (::CoreNavGrid::IsWall({ point.x, 0.0f, point.y })) {
                                if (player.IsValid()) result.push_back(player);
                                break;
                            }
                        }
                    }
                } else if (objectType == SDK::CollisionableObjects::YasuoWall) {
                    const auto sdkCol = SDK::Collision::GetCollision({ v }, input);
                    for (const auto& obj : sdkCol) {
                        result.push_back(obj);
                    }
                }
            }
        }

        // Deduplicate output list
        std::vector<SDK::AIBaseClient> uniqueResult;
        uniqueResult.reserve(result.size());
        for (const auto& obj : result) {
            if (!obj.IsValid()) continue;
            const bool found = std::any_of(uniqueResult.begin(), uniqueResult.end(), [&](const SDK::AIBaseClient& existing) {
                return existing.NetworkId() == obj.NetworkId();
            });
            if (!found) {
                uniqueResult.push_back(obj);
            }
        }
        return uniqueResult;
    }

private:
    static float DistancedSqr(const SDK::Vector2& point, const SDK::Vector2& segmentStart, const SDK::Vector2& segmentEnd, bool onlyIfOnSegment = false) {
        const auto proj = SDK::Prediction::Vec2Ext::ProjectOn(point, segmentStart, segmentEnd);
        if (onlyIfOnSegment && !proj.IsOnSegment) {
            return FLT_MAX;
        }
        return point.DistanceSquared(proj.SegmentPoint);
    }

    static bool MinionIsDead(const SDK::PredictionInput& input, const SDK::AIBaseClient& minion, float distance) {
        float travelTime = distance / input.Speed + input.Delay;
        if (std::abs(input.Speed - FLT_MAX) < 0.0001f) {
            travelTime = input.Delay;
        }
        const int timeMs = static_cast<int>(travelTime * 1000.0f) - SDK::Game::Ping();
        SDK::HealthPrediction::Initialize();
        const float predictedHp = SDK::HealthPrediction::GetPrediction(minion, timeMs, 0);
        return predictedHp <= 0.0f;
    }
};

} // namespace Plugins::FsPred
