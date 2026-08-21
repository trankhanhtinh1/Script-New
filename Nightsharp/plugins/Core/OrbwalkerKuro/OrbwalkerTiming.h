#pragma once

namespace OrbwalkerKuro::Timing {

// The client can cancel an attack during the last few milliseconds of its
// release.  At low latency the local clock is close enough to the server clock
// that we must keep the cursor order back for the remaining part of this
// fixed 30 ms safety window.  Higher latency already contributes that delay.
inline constexpr int kLowPingReleaseWindowMs = 30;
inline constexpr int kSettPunchStateTimeoutMs = 2000;

constexpr int ReleaseMarginMs(int pingMs) noexcept {
    if (pingMs <= 0) {
        return kLowPingReleaseWindowMs;
    }
    return pingMs < kLowPingReleaseWindowMs
        ? kLowPingReleaseWindowMs - pingMs
        : 0;
}

constexpr int ReleaseSafeTick(int releaseTick, int pingMs) noexcept {
    return releaseTick + ReleaseMarginMs(pingMs);
}

// Sett's empowered right punch follows a left punch after the current windup
// and one eighth of the normal attack cycle.  A right-to-left transition uses
// the ordinary cycle.  Keeping this calculation pure prevents callers from
// applying the low-ping release margin a second time.
constexpr float SettPunchCooldownMs(bool nextPunchIsRight,
                                    float attackWindupMs,
                                    float attackCycleMs) noexcept {
    const float windup = attackWindupMs > 0.0f ? attackWindupMs : 0.0f;
    const float cycle = attackCycleMs > 0.0f ? attackCycleMs : 0.0f;
    return nextPunchIsRight ? windup + cycle / 8.0f : cycle;
}

} // namespace OrbwalkerKuro::Timing
