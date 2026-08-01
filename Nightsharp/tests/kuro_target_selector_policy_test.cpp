#include "../plugins/Core/KuroTargetSelector/KuroTargetSelectorPolicy.h"

#include <cassert>

using namespace SDK::KuroTargetSelector;

int main() {
    assert(KuroTargetSelectorPolicy::ProfileFor(TargetPurpose::AutoAttack) ==
           TargetProfile::AutoAttack);
    assert(KuroTargetSelectorPolicy::ProfileFor(TargetPurpose::Execute) ==
           TargetProfile::Execute);
    assert(KuroTargetSelectorPolicy::ProfileFor(TargetPurpose::FleeThreat) ==
           TargetProfile::FleeThreat);

    ScoreBreakdown breakdown{};
    breakdown.Add("bounded", "bounded contribution", 5000.0f, -10.0f, 25.0f);
    assert(breakdown.Total == 25.0f);
    assert(breakdown.Count == 1);

    TargetRequest ignored{};
    ignored.AddIgnoredTarget(22);
    ignored.AddIgnoredTarget(22);
    assert(ignored.IsIgnoredTarget(22));
    assert(ignored.IgnoredTargetCount == 1);

    TargetRequest interrupt{};
    interrupt.Purpose = TargetPurpose::Interrupt;
    TargetFacts facts{};
    facts.NetworkId = 11;
    assert(KuroTargetSelectorPolicy::ValidatePurpose(interrupt, facts) ==
           RejectReason::PurposeRejected);
    facts.IsChanneling = true;
    assert(KuroTargetSelectorPolicy::ValidatePurpose(interrupt, facts) ==
           RejectReason::None);
    return 0;
}
