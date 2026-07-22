#pragma once

#include "Threat.h"
#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <string>

namespace ZDEvade {

struct TrackedSpell {
    SpellData info;
    Vec2 startPos = {};
    Vec2 endPos = {};
    Vec2 direction = {};
    int startTime = 0;
    int missileStartTime = 0;
    int endTime = 0;
    bool isMissile = false;
    SDK::MissileClient missile;
    int spellId = 0;
    bool expired = false;
    uint32_t casterNetworkId = 0;
    int slot = -1;

    float Radius() const { return info.radius; }
    float InnerRadius() const { return std::clamp(info.innerRadius, 0.0f, Radius()); }
    float Range() const { return info.range; }
    float MissileSpeed() const { return info.projectileSpeed > 0 ? info.projectileSpeed : FLT_MAX; }
    int Delay() const { return info.spellDelay; }
    int DangerValue() const { return info.dangerlevel; }
    ZDSpellType Type() const { return info.spellType; }
    std::string SpellName() const { return info.spellName; }

    bool HasExpired() const {
        if (expired) return true;
        const int now = SDK::Variables::TickCount();
        if (TickAfter(now, SaturatingTickAdd(endTime, 500))) return true;
        if (isMissile && !missile.IsValid()) return true;
        return false;
    }

    Vec2 GetMissilePosition(int afterTime = 0) const {
        if (!isMissile || !missile.IsValid()) return startPos;
        const int baseTime = missileStartTime > 0
            ? missileStartTime
            : SaturatingTickAdd(startTime, info.spellDelay);
        const int sampleTick = SaturatingTickAdd(
            SDK::Variables::TickCount(), afterTime);
        const std::int64_t elapsed = std::max<std::int64_t>(
            0, TickDifference(sampleTick, baseTime));
        const float speed = std::max(1.0f, info.projectileSpeed);
        const float distance = static_cast<float>(elapsed) * speed / 1000.0f;
        return startPos + direction * distance;
    }
};

} // namespace ZDEvade
