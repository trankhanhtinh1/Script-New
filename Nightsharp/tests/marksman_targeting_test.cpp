#include "../plugins/Champion/KuroAIO/AI/AIMarksmanTargeting.h"

#include <cstdlib>
#include <iostream>

using namespace Plugins::KuroAIO::AI::MarksmanTargeting;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

TargetContext Base() {
    TargetContext context{};
    context.Distance = 700.0f;
    context.MaximumReach = 1200.0f;
    context.HealthPercent = 60.0f;
    context.EffectiveHealth = 1000.0f;
    context.EstimatedDamage = 420.0f;
    return context;
}

} // namespace

int main() {
    auto unreachable = Base();
    unreachable.Selected = true;
    Require(!EvaluateTarget(unreachable).Valid,
            "a selected target with no damage route must be rejected");

    auto immune = Base();
    immune.AutoReachable = true;
    immune.DamageImmune = true;
    Require(!EvaluateTarget(immune).Valid,
            "an invulnerable or stasis target must be rejected");

    auto direct = Base();
    direct.DirectSpellReachable = true;
    auto setup = Base();
    setup.SetupReachable = true;
    Require(BetterTarget(EvaluateTarget(direct), EvaluateTarget(setup)),
            "a spell that can hit now must beat speculative setup reach");

    auto clean = direct;
    auto blocked = direct;
    blocked.ProjectileBlocked = true;
    Require(BetterTarget(EvaluateTarget(clean), EvaluateTarget(blocked)),
            "a clean firing line must beat an otherwise equal blocked route");

    auto ordinary = direct;
    auto lethal = direct;
    lethal.Killable = true;
    lethal.EstimatedDamage = 1100.0f;
    Require(BetterTarget(EvaluateTarget(lethal), EvaluateTarget(ordinary)),
            "a reachable lethal target must receive execute priority");

    auto selected = direct;
    selected.Selected = true;
    Require(BetterTarget(EvaluateTarget(selected), EvaluateTarget(direct)),
            "player selection must win when both targets are actually reachable");

    auto orbwalker = direct;
    orbwalker.OrbwalkerTarget = true;
    Require(BetterTarget(EvaluateTarget(orbwalker), EvaluateTarget(direct)),
            "orbwalker attack target must anchor a reachable spell sequence");

    auto tooFar = direct;
    tooFar.Distance = 1300.0f;
    Require(!EvaluateTarget(tooFar).Valid,
            "targets outside the declared real reach must be rejected");

    std::cout << "Marksman targeting tests passed\n";
    return 0;
}
