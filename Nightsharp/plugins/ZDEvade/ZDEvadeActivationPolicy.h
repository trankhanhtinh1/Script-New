#pragma once

namespace ZDEvade {

struct OtherEvadeState {
    bool kuroLoaded = false;
    bool ezLoaded = false;
};

enum class OtherEvadeReason {
    None,
    KuroEvade,
    EzEvade,
    Multiple,
};

struct OtherEvadeDecision {
    bool suspended = false;
    bool suspendNow = false;
    bool releaseNow = false;
    OtherEvadeReason reason = OtherEvadeReason::None;
};

constexpr OtherEvadeReason GetOtherEvadeReason(
    const OtherEvadeState& state) {
    if (state.kuroLoaded && state.ezLoaded) {
        return OtherEvadeReason::Multiple;
    }
    if (state.kuroLoaded) return OtherEvadeReason::KuroEvade;
    if (state.ezLoaded) return OtherEvadeReason::EzEvade;
    return OtherEvadeReason::None;
}

constexpr bool CanActivateZDEvade(const OtherEvadeState& state) {
    return GetOtherEvadeReason(state) == OtherEvadeReason::None;
}

constexpr OtherEvadeDecision DecideOtherEvadeState(
    const OtherEvadeState& state,
    bool wasSuspended,
    bool allowRelease) {
    const OtherEvadeReason reason = GetOtherEvadeReason(state);
    const bool otherEvadeLoaded = reason != OtherEvadeReason::None;
    const bool releaseNow =
        !otherEvadeLoaded && wasSuspended && allowRelease;
    return {
        otherEvadeLoaded || (wasSuspended && !releaseNow),
        otherEvadeLoaded && !wasSuspended,
        releaseNow,
        reason,
    };
}

constexpr const char* OtherEvadeReasonName(OtherEvadeReason reason) {
    switch (reason) {
    case OtherEvadeReason::KuroEvade:
        return "KuroEvade is loaded";
    case OtherEvadeReason::EzEvade:
        return "EzEvade is loaded";
    case OtherEvadeReason::Multiple:
        return "KuroEvade and EzEvade are loaded";
    default:
        return "None";
    }
}

} // namespace ZDEvade
