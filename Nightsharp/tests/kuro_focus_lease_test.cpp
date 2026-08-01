#include "../core/KuroCombatCoordinator.h"

#include <cassert>

using Plugins::AICombatTargetCoordinator::FocusLease;
using Plugins::AICombatTargetCoordinator::LeaseStatus;
using Plugins::AICombatTargetCoordinator::LeaseStrength;

int main() {
    FocusLease::Reset();

    assert(FocusLease::Acquire(7, 101, LeaseStrength::Hard, 100, 500, 10));
    assert(FocusLease::IsActive(120));
    assert(FocusLease::HardTargetNetworkId(120) == 101);

    assert(FocusLease::BlockedTarget(101, 140));
    assert(FocusLease::Snapshot(140).Status == LeaseStatus::Suspended);
    assert(FocusLease::PreferredTargetNetworkId(140) == 0);

    assert(FocusLease::Restore(7, 101, 160, 300));
    assert(FocusLease::PreferredTargetNetworkId(160) == 101);

    FocusLease::SetManualOverride(true, 202);
    assert(!FocusLease::IsActive(170));
    assert(FocusLease::ManualTargetNetworkId() == 202);
    FocusLease::SetManualOverride(false);
    assert(FocusLease::IsActive(180));

    assert(FocusLease::BlockedTarget(101, 190));
    FocusLease::SetManualOverride(true, 303);
    FocusLease::SetManualOverride(false);
    assert(FocusLease::Snapshot(200).Status == LeaseStatus::Suspended);
    assert(FocusLease::Restore(7, 101, 210, 300));
    assert(FocusLease::IsActive(210));

    FocusLease::Tick(520);
    assert(FocusLease::Snapshot(520).Status == LeaseStatus::Terminal);
    assert(FocusLease::PreferredTargetNetworkId(520) == 0);

    FocusLease::Reset();
    return 0;
}
