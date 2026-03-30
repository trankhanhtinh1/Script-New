#pragma once

#include "../../Enumerations/CollisionableObjects.h"
#include "../../Enumerations/HitChance.h"
#include "../../Enumerations/SkillshotType.h"
#include "../../Core/Objects.h"

#include <cfloat>
#include <vector>

namespace SDK {

struct PredictionInput {
    bool AoE = false;
    Vector3 From = {};
    Vector3 RangeCheckFrom = {};
    float Range = FLT_MAX;
    float Speed = FLT_MAX;
    float Delay = 0.25f;
    float Radius = 0.0f;
    SkillshotType Type = SkillshotType::Line;
    bool Collision = false;
    bool UseBoundingRadius = true;
    CollisionableObjects CollisionObjects = CollisionableObjects::Minions | CollisionableObjects::YasuoWall;
    AIBaseClient Unit = {};

    float RealRadius() const {
        return UseBoundingRadius && Unit.IsValid() ? (Radius + Unit.BoundingRadius()) : Radius;
    }
};

struct PredictionOutput {
    Vector3 CastPosition = {};
    Vector3 UnitPosition = {};
    HitChance Hitchance = HitChance::Impossible;
    std::vector<GameObject> CollisionObjects = {};
    std::vector<AIHeroClient> AoeTargetsHit = {};
    int AoeHitCount = 0;
    PredictionInput Input = {};

    bool IsValid() const {
        return Hitchance != HitChance::Impossible && Hitchance != HitChance::OutOfRange;
    }

    int AoeTargetsHitCount() const {
        return (std::max)(AoeHitCount, static_cast<int>(AoeTargetsHit.size()));
    }
};

} // namespace SDK
