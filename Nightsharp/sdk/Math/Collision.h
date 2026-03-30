#pragma once

#include "../Core/Objects.h"
#include "Prediction.h"

#include <vector>

namespace SDK::Collision {

    inline bool HasLineCollision(const Vector3& from,
                                 const Vector3& to,
                                 float radius,
                                 const GameObject& ignored = GameObject()) {
        const AIBaseClient ignoredUnit = ignored.IsValid() ? AIBaseClient(ignored.Address()) : AIBaseClient();
        return !Prediction::Movement::CollectLineCollisions(
            from,
            to,
            radius,
            ignoredUnit,
            CollisionObjects::Minions | CollisionObjects::YasuoWall | CollisionObjects::BraumShield).empty();
    }

    inline std::vector<GameObject> GetCollision(const AIBaseClient& target, const PredictionInput& input) {
        auto effectiveInput = input;
        effectiveInput.Unit = target;
        return Prediction::GetPrediction(target, effectiveInput).CollisionObjects;
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

            if (input.Collision && input.Type == SkillshotType::Line) {
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
