#pragma once

#include <cstdint>

// Lightweight, allocation-free handoff between KuroEvade and KuroOrbwalker.
// Both systems run on the game thread, so a tiny shared state is sufficient.

namespace Plugins::KuroCombatCoordination {

enum class EvadePhase {
    Idle,
    WindupHold,
    Dodging,
    PathRecovery,
    SafePositionHold
};

class Coordinator final {
public:
    static void Publish(EvadePhase phase,
                        int now,
                        bool blockNewAttacks = true) {
        if (phase != EvadePhase::Idle) {
            if (s_phase == EvadePhase::Idle) {
                ++s_generation;
            }
            s_phase = phase;
            s_blockNewAttacks = blockNewAttacks;
            s_lastPublishTick = now;
            return;
        }

        if (s_phase != EvadePhase::Idle) {
            s_releaseTick = now;
        }
        s_phase = EvadePhase::Idle;
        s_blockNewAttacks = false;
        s_lastPublishTick = now;
    }

    static void Reset() {
        s_phase = EvadePhase::Idle;
        s_lastPublishTick = 0;
        s_releaseTick = 0;
        s_blockNewAttacks = false;
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
            return s_blockNewAttacks;
        }

        // A short attack grace prevents a fresh attack order from replacing
        // KuroEvade's restored move. Movement can keep the full user setting.
        const int attackGraceMs = handoffGraceMs < 40 ? handoffGraceMs : 40;
        return IsInsideReleaseGrace(now, attackGraceMs);
    }

    static EvadePhase Phase(int now) {
        return EvadeOwnsActions(now) ? s_phase : EvadePhase::Idle;
    }

    static bool AllowsStationaryAttacks(int now) {
        return IsFresh(now) && s_phase == EvadePhase::SafePositionHold;
    }

    static int Generation() {
        return s_generation;
    }

    static const char* PhaseName(EvadePhase phase) {
        switch (phase) {
        case EvadePhase::WindupHold: return "finishing attack windup";
        case EvadePhase::Dodging: return "evade movement";
        case EvadePhase::PathRecovery: return "path recovery";
        case EvadePhase::SafePositionHold: return "safe hold + attack";
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
    static inline bool s_blockNewAttacks = false;
};

} // namespace Plugins::KuroCombatCoordination

// Focus is a coordination hint, not a second target selector.  Champion
// logic may acquire a temporary lease here; the selector only receives the
// resulting identity through a TargetRequest and remains the owner of legality
// and ranking.  Keeping this state beside the evade handoff makes suspension,
// expiry, and manual override semantics explicit and allocation-free.
namespace Plugins::AICombatTargetCoordinator {

enum class LeaseStrength {
    Soft,
    Hard,
};

enum class LeaseStatus {
    Inactive,
    Active,
    Suspended,
    Terminal,
};

struct FocusLeaseSnapshot {
    std::uint32_t OwnerId = 0;
    int TargetNetworkId = 0;
    LeaseStrength Strength = LeaseStrength::Soft;
    LeaseStatus Status = LeaseStatus::Inactive;
    int AcquiredTick = 0;
    int ExpiresTick = 0;
    int SuspendedTick = 0;
    int Revision = 0;
    int OwnerPriority = 0;
    bool ManualOverride = false;
    int ManualTargetNetworkId = 0;
    bool SuspendedByManualOverride = false;
};

class FocusLease final {
public:
    static bool Acquire(std::uint32_t ownerId,
                        int targetNetworkId,
                        LeaseStrength strength,
                        int now,
                        int ttlTicks,
                        int ownerPriority = 0) {
        if (ownerId == 0 || targetNetworkId <= 0 || ttlTicks <= 0) {
            return false;
        }
        Tick(now);
        if (s_manualOverride) {
            return false;
        }

        const bool replace =
            s_lease.Status == LeaseStatus::Inactive ||
            s_lease.Status == LeaseStatus::Terminal ||
            s_lease.OwnerId == ownerId ||
            (strength == LeaseStrength::Hard &&
             s_lease.Strength == LeaseStrength::Soft) ||
            ownerPriority > s_lease.OwnerPriority;
        if (!replace) {
            return false;
        }

        s_lease.OwnerId = ownerId;
        s_lease.TargetNetworkId = targetNetworkId;
        s_lease.Strength = strength;
        s_lease.Status = LeaseStatus::Active;
        s_lease.AcquiredTick = now;
        s_lease.ExpiresTick = now + ttlTicks;
        s_lease.SuspendedTick = 0;
        s_lease.OwnerPriority = ownerPriority;
        s_lease.SuspendedByManualOverride = false;
        ++s_lease.Revision;
        return true;
    }

    static bool Refresh(std::uint32_t ownerId, int now, int ttlTicks) {
        if (ownerId == 0 || ttlTicks <= 0) return false;
        Tick(now);
        if (s_lease.OwnerId != ownerId ||
            (s_lease.Status != LeaseStatus::Active &&
             s_lease.Status != LeaseStatus::Suspended)) {
            return false;
        }
        s_lease.ExpiresTick = now + ttlTicks;
        if (!s_manualOverride && s_lease.Status == LeaseStatus::Suspended &&
            s_lease.SuspendedByManualOverride) {
            s_lease.Status = LeaseStatus::Active;
            s_lease.SuspendedByManualOverride = false;
        }
        return true;
    }

    // Suspension deliberately retains owner and target identity so a blocked
    // action can be retried or restored after a short-lived state change.
    static bool Suspend(std::uint32_t ownerId, int now) {
        Tick(now);
        if (ownerId == 0 || s_lease.OwnerId != ownerId ||
            s_lease.Status != LeaseStatus::Active) {
            return false;
        }
        s_lease.Status = LeaseStatus::Suspended;
        s_lease.SuspendedTick = now;
        s_lease.SuspendedByManualOverride = false;
        ++s_lease.Revision;
        return true;
    }

    static bool Restore(std::uint32_t ownerId,
                        int targetNetworkId,
                        int now,
                        int ttlTicks) {
        if (ownerId == 0 || targetNetworkId <= 0 || ttlTicks <= 0) {
            return false;
        }
        Tick(now);
        if (s_lease.OwnerId != ownerId ||
            s_lease.Status != LeaseStatus::Suspended) {
            return false;
        }
        // A restored lease may intentionally switch identity, but never
        // silently resurrect an expired/terminal lease.
        s_lease.TargetNetworkId = targetNetworkId;
        s_lease.Status = s_manualOverride ? LeaseStatus::Suspended : LeaseStatus::Active;
        s_lease.SuspendedByManualOverride = s_manualOverride;
        s_lease.AcquiredTick = now;
        s_lease.ExpiresTick = now + ttlTicks;
        s_lease.SuspendedTick = 0;
        ++s_lease.Revision;
        return true;
    }

    static bool Release(std::uint32_t ownerId) {
        if (ownerId == 0 || s_lease.OwnerId != ownerId) return false;
        s_lease = FocusLeaseSnapshot{};
        return true;
    }

    static bool BlockedTarget(int targetNetworkId, int now) {
        Tick(now);
        if (targetNetworkId <= 0 || s_lease.TargetNetworkId != targetNetworkId ||
            s_lease.Status != LeaseStatus::Active) {
            return false;
        }
        s_lease.Status = LeaseStatus::Suspended;
        s_lease.SuspendedTick = now;
        s_lease.SuspendedByManualOverride = false;
        ++s_lease.Revision;
        return true;
    }

    static void SetManualOverride(bool enabled, int targetNetworkId = 0) {
        s_manualOverride = enabled;
        s_manualTargetNetworkId = enabled ? targetNetworkId : 0;
        s_lease.ManualOverride = s_manualOverride;
        s_lease.ManualTargetNetworkId = s_manualTargetNetworkId;
        if (enabled && s_lease.Status == LeaseStatus::Active) {
            s_lease.Status = LeaseStatus::Suspended;
            s_lease.SuspendedByManualOverride = true;
        }
        if (!enabled && s_lease.Status == LeaseStatus::Suspended &&
            s_lease.SuspendedByManualOverride && s_lease.ExpiresTick > 0) {
            s_lease.Status = LeaseStatus::Active;
            s_lease.SuspendedByManualOverride = false;
        }
        ++s_lease.Revision;
    }

    static void Tick(int now) {
        if (now <= 0 || s_lease.ExpiresTick <= 0) return;
        if (now >= s_lease.ExpiresTick &&
            s_lease.Status != LeaseStatus::Inactive &&
            s_lease.Status != LeaseStatus::Terminal) {
            s_lease.Status = LeaseStatus::Terminal;
            ++s_lease.Revision;
        }
    }

    static FocusLeaseSnapshot Snapshot(int now = 0) {
        if (now > 0) Tick(now);
        FocusLeaseSnapshot snapshot = s_lease;
        snapshot.ManualOverride = s_manualOverride;
        snapshot.ManualTargetNetworkId = s_manualTargetNetworkId;
        return snapshot;
    }

    static bool IsActive(int now = 0) {
        const auto snapshot = Snapshot(now);
        return snapshot.Status == LeaseStatus::Active && !snapshot.ManualOverride;
    }

    static int PreferredTargetNetworkId(int now = 0) {
        const auto snapshot = Snapshot(now);
        return IsActive(now) ? snapshot.TargetNetworkId : 0;
    }

    static int HardTargetNetworkId(int now = 0) {
        const auto snapshot = Snapshot(now);
        return IsActive(now) && snapshot.Strength == LeaseStrength::Hard
            ? snapshot.TargetNetworkId
            : 0;
    }

    static bool ManualOverrideActive() { return s_manualOverride; }
    static int ManualTargetNetworkId() { return s_manualTargetNetworkId; }

    static void Reset() {
        s_lease = FocusLeaseSnapshot{};
        s_manualOverride = false;
        s_manualTargetNetworkId = 0;
    }

private:
    static inline FocusLeaseSnapshot s_lease = {};
    static inline bool s_manualOverride = false;
    static inline int s_manualTargetNetworkId = 0;
};

} // namespace Plugins::AICombatTargetCoordinator
