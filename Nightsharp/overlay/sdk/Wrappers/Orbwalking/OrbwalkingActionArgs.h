#pragma once

#include "../../Enumerations/OrbwalkingType.h"
#include "../../GameObjects/GameObjects.h"

namespace SDK {

struct OrbwalkingActionArgs {
    Vector3 Position = {};
    bool Process = true;
    AIBaseClient Sender = {};
    AttackableUnit Target = {};
    OrbwalkingType Type = OrbwalkingType::None;
};

} // namespace SDK
