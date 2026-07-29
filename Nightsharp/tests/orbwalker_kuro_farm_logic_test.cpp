#include "../plugins/Core/OrbwalkerKuro/OrbwalkerFarmLogic.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace OrbwalkerKuro::FarmLogic;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

bool Near(float left, float right, float epsilon = 0.001f) {
    return std::fabs(left - right) <= epsilon;
}

} // namespace

int main() {
    const float coefficient25 = SolveLinearHazardCoefficient(0.25f);
    Require(Near(coefficient25, 0.0847441f, 0.00001f),
            "25 percent crit must use the calibrated linear-hazard coefficient");
    Require(Near(LinearHazardLongRunRate(coefficient25), 0.25f, 0.00001f),
            "discrete streak probabilities must preserve long-run crit chance");
    Require(Near(LinearHazardProbability(coefficient25, 0), 0.0847441f, 0.00001f) &&
                Near(LinearHazardProbability(coefficient25, 5), 0.508465f, 0.0001f) &&
                Near(LinearHazardProbability(coefficient25, 10), 0.932185f, 0.0001f) &&
                Near(LinearHazardProbability(coefficient25, 11), 1.0f),
            "25 percent next-crit hazard must increase across a non-crit drought");

    Require(IsCriticalAttackName("CaitlynCritAttack") &&
                IsCriticalAttackName("JinxAttackCrit") &&
                IsCriticalAttackName("basicCRITICALattack") &&
                !IsCriticalAttackName("CaitlynBasicAttack") &&
                !IsCriticalAttackName(nullptr),
            "OnProcessSpell name classifier must separate critattack variants");

    CritSequenceTracker tracker;
    Require(Near(tracker.PredictNextCritProbability(0.50f), 0.302103f, 0.0001f),
            "an empty tracker must start from the calibrated prior");
    tracker.Observe(0.50f, false);
    Require(tracker.ConsecutiveNonCrits() == 1 &&
                tracker.AttemptsAt(0.50f, 0) == 1 &&
                tracker.CritsAt(0.50f, 0) == 0 &&
                tracker.PredictNextCritProbability(0.50f) > 0.59f,
            "a non-crit must advance the discrete drought state");
    tracker.Observe(0.50f, false);
    Require(tracker.ConsecutiveNonCrits() == 2 &&
                tracker.PredictNextCritProbability(0.50f) > 0.89f,
            "two misses at 50 percent must make the next crit highly likely");
    tracker.Observe(0.50f, true);
    Require(tracker.ConsecutiveNonCrits() == 0 &&
                tracker.AttemptsAt(0.50f, 2) == 1 &&
                tracker.CritsAt(0.50f, 2) == 1,
            "a crit must close the cycle and reset the drought");

    Require(ShouldApplyPredictedCritDamage(0.90f, 0.50f, false) &&
                !ShouldApplyPredictedCritDamage(0.899f, 0.50f, false) &&
                !ShouldApplyPredictedCritDamage(0.95f, 0.50f, true) &&
                ShouldApplyPredictedCritDamage(1.0f, 0.50f, true) &&
                ShouldApplyPredictedCritDamage(1.0f, 1.0f, true),
            "normal minions use 90 percent confidence while cannon crits require certainty");

    Require(IsInsideLastHitDamageWindow(96.0f, 100.0f) &&
                !IsInsideLastHitDamageWindow(97.0f, 100.0f) &&
                !IsInsideLastHitDamageWindow(0.0f, 100.0f),
            "last-hit eligibility must require positive health below safe attack damage");
    Require(Near(LastHitBoundarySafety(50.0f, 100.0f), 47.0f),
            "window safety must measure distance to the nearest health boundary");

    LastHitWindowCandidate current;
    current.valid = true;
    current.closingWindowMs = 500;
    current.boundarySafety = 20.0f;
    current.predictedHealth = 40.0f;
    current.stableOrder = 0;

    LastHitWindowCandidate urgent = current;
    urgent.closingWindowMs = 300;
    urgent.boundarySafety = 5.0f;
    urgent.stableOrder = 1;
    Require(PreferLastHitCandidate(urgent, current),
            "the ordinary minion whose last-hit window closes first must win");

    LastHitWindowCandidate saferTie = current;
    saferTie.closingWindowMs = 450;
    saferTie.boundarySafety = 30.0f;
    saferTie.stableOrder = 2;
    Require(PreferLastHitCandidate(saferTie, current),
            "deadlines within 75 ms must prefer the safer damage window");

    LastHitWindowCandidate noDeadline = current;
    noDeadline.closingWindowMs = std::numeric_limits<int>::max();
    Require(!PreferLastHitCandidate(noDeadline, current),
            "a finite closing window must stay ahead of a non-urgent minion");

    std::cout << "ALL ORBWALKER KURO FARM LOGIC TESTS PASSED\n";
    return 0;
}
