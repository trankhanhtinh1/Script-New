#pragma once

#include "../Evade/EvadeRoutingPolicy.h"

#include <cstdint>

namespace ZDEvade {

inline constexpr std::uint32_t kStrictSafeTargetFootprintColor =
    0xFF00FF66u;
inline constexpr std::uint32_t kFallbackTargetFootprintColor =
    0xFFFF3300u;

struct LockedTargetVisualDispatch {
    float footprintRadius = kMinimumHeroRadius;
    std::uint32_t color = kFallbackTargetFootprintColor;
};

inline LockedTargetVisualDispatch GetLockedTargetVisualDispatch(
        bool strictSafe,
        float runtimeBoundingRadius) {
    return {
        SanitizeHeroRadius(runtimeBoundingRadius),
        strictSafe
            ? kStrictSafeTargetFootprintColor
            : kFallbackTargetFootprintColor,
    };
}

} // namespace ZDEvade
