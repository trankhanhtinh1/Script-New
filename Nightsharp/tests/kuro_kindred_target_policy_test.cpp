#include "../plugins/Champion/KuroAIO/Champion/KindredTargetPolicy.h"

#include <cassert>

using namespace Plugins::KuroAIO::Kindred::TargetPolicy;
int main() {
    MechanicFacts marked{};
    marked.EMarked = true;
    marked.ETracker = true;
    marked.EStacks = 3;
    assert(Score(marked) > Score(MechanicFacts{}));
    assert(!RZoneBlocksAction(marked, false));

    MechanicFacts inR{};
    inR.RZone = true;
    assert(RZoneBlocksAction(inR, false));
    assert(RZoneBlocksAction(inR, false));
    assert(!RZoneBlocksAction(inR, true));

    assert(PreferTwoAttackAlternate(301.0f, 100.0f, 190.0f, 100.0f));
    assert(!PreferTwoAttackAlternate(300.0f, 100.0f, 190.0f, 100.0f));
    assert(!PreferTwoAttackAlternate(301.0f, 0.0f, 190.0f, 100.0f));
    return 0;
}
