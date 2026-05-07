#pragma once

#include "../../Enumerations/OrbwalkerMode.h"
#include "../../Enumerations/OrbwalkingType.h"
#include "../../GameObjects/ObjectManager.h"

namespace SDK {

struct OrbwalkingActionArgs {
    Vector3 Position = {};
    bool Process = true;
    AIBaseClient Sender = {};
    GameObject Target = {};
    OrbwalkingType Type = OrbwalkingType::None;
};

} // namespace SDK
