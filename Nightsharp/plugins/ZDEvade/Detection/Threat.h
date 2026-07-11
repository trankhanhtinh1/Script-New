#pragma once

#include "../Database/SpellData.h"
#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ZDEvade {

struct Threat {
    int id = -1;
    int revision = 0;
    const SpellData* data = nullptr;
    int dangerOverride = 0;
    Vec2 startPos = {};
    Vec2 endPos = {};
    Vec2 direction = {};
    int startTick = 0;
    int launchTick = 0;
    int endTick = 0;
    std::uintptr_t castIdentity = 0;
    std::uint32_t casterNetworkId = 0;
    std::uint32_t missileNetworkId = 0;
    std::uint32_t sourceObjectNetworkId = 0;
    int slot = -1;
    Vec2 observedHead = {};
    int observedTick = 0;
    float observedSpeed = 0.0f;
    float positionUncertainty = 0.0f;
    float radiusOverride = -1.0f;
    int delayOverride = -1;
    bool missileBound = false;
    bool objectBound = false;
    bool expired = false;

    bool HasData() const { return data != nullptr; }
    float Radius() const {
        if (radiusOverride >= 0.0f) return radiusOverride;
        return data ? std::max(0.0f, data->radius) : 0.0f;
    }
    float InnerRadius() const {
        return data ? std::clamp(data->innerRadius, 0.0f, Radius()) : 0.0f;
    }
    float Range() const { return data ? std::max(0.0f, data->range) : 0.0f; }
    float Speed() const {
        if (std::isfinite(observedSpeed) && observedSpeed > 1.0f && observedSpeed < 100000.0f)
            return observedSpeed;
        return data ? data->projectileSpeed : 0.0f;
    }
    float PositionUncertainty() const {
        return std::isfinite(positionUncertainty)
            ? std::clamp(positionUncertainty, 0.0f, 80.0f)
            : 0.0f;
    }
    int Danger() const {
        return std::max(1, dangerOverride > 0
            ? dangerOverride
            : data ? data->dangerlevel : 1);
    }
    ZDSpellType Type() const { return data ? data->spellType : ZDSpellType::Line; }
    int Delay() const {
        if (delayOverride >= 0) return delayOverride;
        return data ? std::max(0, data->spellDelay) : 0;
    }
    int ExtraEndTime() const { return data ? std::max(0, data->extraEndTime) : 0; }
    float Angle() const { return data ? data->angle : 0.0f; }
    bool DefaultOff() const { return data && data->defaultOff; }
    const std::string& SpellName() const {
        static const std::string empty;
        return data ? data->spellName : empty;
    }

    bool HasTravelSpeed() const {
        return !objectBound && data && std::isfinite(data->projectileSpeed) &&
               data->projectileSpeed > 1.0f &&
               data->projectileSpeed < 100000.0f;
    }

    Vec2 HeadAtTick(int tick) const {
        if (Type() != ZDSpellType::Line && Type() != ZDSpellType::Arc) return startPos;
        if (!HasTravelSpeed()) return startPos;
        const float totalDistance = startPos.Distance(endPos);
        if (observedTick > 0 && observedHead.IsValid() && !observedHead.IsZero()) {
            const float observedDistance = std::clamp(
                (observedHead - startPos).Dot(direction),
                0.0f,
                totalDistance);
            const float elapsedSeconds = static_cast<float>(tick - observedTick) / 1000.0f;
            const float distance = std::clamp(
                observedDistance + elapsedSeconds * Speed(),
                0.0f,
                totalDistance);
            return startPos + direction * distance;
        }
        const int baseTick = missileBound && launchTick > 0
            ? launchTick
            : startTick + Delay();
        const float elapsedSeconds = static_cast<float>(std::max(0, tick - baseTick)) / 1000.0f;
        const float distance = std::clamp(elapsedSeconds * Speed(), 0.0f, totalDistance);
        return startPos + direction * distance;
    }

    bool IsExpiredAt(int tick) const {
        if (objectBound) return expired;
        return expired || tick > endTick + 250;
    }
};

}
