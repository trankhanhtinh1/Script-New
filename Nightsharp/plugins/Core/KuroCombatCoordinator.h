#pragma once

// Lightweight, allocation-free handoff between KuroEvade and KuroOrbwalker.
// Both systems run on the game thread, so a tiny shared state is sufficient.

namespace Plugins::KuroCombatCoordination {

enum class EvadePhase {
    Idle,
    WindupHold,
    Dodging,
    PathRecovery
};

class Coordinator final {
public:
    static void Publish(EvadePhase phase, int now) {
        if (phase != EvadePhase::Idle) {
            if (s_phase == EvadePhase::Idle) {
                ++s_generation;
            }
            s_phase = phase;
            s_lastPublishTick = now;
            return;
        }

        if (s_phase != EvadePhase::Idle) {
            s_releaseTick = now;
        }
        s_phase = EvadePhase::Idle;
        s_lastPublishTick = now;
    }

    static void Reset() {
        s_phase = EvadePhase::Idle;
        s_lastPublishTick = 0;
        s_releaseTick = 0;
        ++s_generation;
    }

    static bool EvadeOwnsActions(int now) {
        return IsFresh(now) && s_phase != EvadePhase::Idle;
    }

    static bool BlocksMovement(int now, int handoffGraceMs) {
        if (EvadeOwnsActions(now)) {
            return true;
        }
        return IsInsideReleaseGrace(now, handoffGraceMs);
    }

    static bool BlocksNewAttacks(int now, int handoffGraceMs) {
        if (EvadeOwnsActions(now)) {
            return true;
        }

        // A short attack grace prevents a fresh attack order from replacing
        // KuroEvade's restored move. Movement can keep the full user setting.
        const int attackGraceMs = handoffGraceMs < 40 ? handoffGraceMs : 40;
        return IsInsideReleaseGrace(now, attackGraceMs);
    }

    static EvadePhase Phase(int now) {
        return EvadeOwnsActions(now) ? s_phase : EvadePhase::Idle;
    }

    static int Generation() {
        return s_generation;
    }

    static const char* PhaseName(EvadePhase phase) {
        switch (phase) {
        case EvadePhase::WindupHold: return "finishing attack windup";
        case EvadePhase::Dodging: return "evade movement";
        case EvadePhase::PathRecovery: return "path recovery";
        default: return "idle";
        }
    }

private:
    static bool IsFresh(int now) {
        const int age = now - s_lastPublishTick;
        return s_lastPublishTick > 0 && age >= 0 && age <= kStaleTimeoutMs;
    }

    static bool IsInsideReleaseGrace(int now, int graceMs) {
        if (graceMs <= 0 || s_releaseTick <= 0) {
            return false;
        }
        const int elapsed = now - s_releaseTick;
        return elapsed >= 0 && elapsed < graceMs;
    }

    static constexpr int kStaleTimeoutMs = 250;
    static inline EvadePhase s_phase = EvadePhase::Idle;
    static inline int s_lastPublishTick = 0;
    static inline int s_releaseTick = 0;
    static inline int s_generation = 0;
};

} // namespace Plugins::KuroCombatCoordination
