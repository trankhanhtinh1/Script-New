#pragma once

#include "../../../../Core/Objects.h"

namespace SDK::SpellTracker::Skillshots {

class _ZiggsR {
public:
    static float RadiusForDistance(float distance) {
        if (distance <= 1400.0f) {
            return 550.0f;
        }
        if (distance <= 2300.0f) {
            return 650.0f;
        }
        return 750.0f;
    }

    static float RadiusForTarget(const AIBaseClient& source, const Vector3& castPosition) {
        return RadiusForDistance(source.Distance(castPosition));
    }
};

} // namespace SDK::SpellTracker::Skillshots
