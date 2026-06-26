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
    Vec3 From = {};
    Vec3 RangeCheckFrom = {};
    float Range = FLT_MAX;
    float Speed = FLT_MAX;
    float Delay = 0.0f;
    float Radius = 1.0f;
    SkillshotType Type = SkillshotType::SkillshotLine;
    bool Collision = false;
    bool UseBoundingRadius = true;
    CollisionableObjects CollisionObjects = CollisionableObjects::Minions | CollisionableObjects::YasuoWall;
    AIBaseClient Unit = {};

    float RealRadius() const {
        return UseBoundingRadius && Unit.IsValid() ? (Radius + Unit.BoundingRadius()) : Radius;
    }
};

struct PredictionOutput {
    Vec3 CastPosition = {};
    Vec3 UnitPosition = {};
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
