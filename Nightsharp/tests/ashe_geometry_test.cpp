#include "../plugins/Champion/KuroAIO/AI/Controllers/AIAsheGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Ashe::Geometry;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

bool Near(float left, float right, float epsilon = 0.01f) {
    return std::fabs(left - right) <= epsilon;
}

} // namespace

int main() {
    Require(VolleyArrowCount(1) == 7 && VolleyArrowCount(3) == 9 &&
                VolleyArrowCount(5) == 11,
            "Volley must use the live 7/8/9/10/11 rank progression");
    Require(Near(VolleyHalfAngleRadians(1) * 180.0f / kPi, 15.0f) &&
                Near(VolleyHalfAngleRadians(5) * 180.0f / kPi, 25.0f),
            "Volley rays must remain five degrees apart around the center ray");

    const Vec3 origin{ 0.0f, 0.0f, 0.0f };
    const Vec3 forward{ 1.0f, 0.0f, 0.0f };
    const Vec3 left = VolleyRayDirection(forward, 1, 0);
    const Vec3 center = VolleyRayDirection(forward, 1, 3);
    const Vec3 right = VolleyRayDirection(forward, 1, 6);
    Require(left.z < -0.20f && Near(center.x, 1.0f) && right.z > 0.20f,
            "rank-one Volley must expose symmetric edge and center rays");

    std::vector<VolleyUnit> blocked = {
        { Vec3{ 350.0f, 0.0f, 0.0f }, 35.0f, 0.5f, 300.0f,
          1, false, true, false, false, false, true },
        { Vec3{ 850.0f, 0.0f, 0.0f }, 55.0f, 3.0f, 900.0f,
          2, true, false, true, false, false, true },
    };
    const VolleyRayHit first = FirstVolleyRayHit(
        origin, forward, 3, blocked);
    Require(first.Valid && first.UnitId == 1,
            "each Volley ray must stop on its nearest unit, not the selected hero");
    const VolleyEvaluation blockedPlan = EvaluateVolley(
        origin, forward, 1, blocked, 2);
    Require(!blockedPlan.HitsPrimary,
            "a minion covering the relevant ray must invalidate a fake line hit");

    std::vector<VolleyUnit> sideThread = blocked;
    sideThread[1].Position = Vec3{ 850.0f, 0.0f, 220.0f };
    const VolleyEvaluation threaded = EvaluateVolley(
        origin, forward, 1, sideThread, 2);
    Require(threaded.HitsPrimary && threaded.PrimaryRay > 3,
            "a side Volley ray must be able to thread around the center blocker");

    std::vector<VolleyUnit> duplicate = {
        { Vec3{ 500.0f, 0.0f, 0.0f }, 180.0f, 2.0f, 900.0f,
          7, true, false, true, false, false, true },
    };
    const VolleyEvaluation oneDamage = EvaluateVolley(
        origin, forward, 5, duplicate, 7);
    Require(oneDamage.BlockedRays > 1 && oneDamage.UniqueHits == 1 &&
                oneDamage.ChampionHits == 1,
            "one large unit may intercept several arrows but only count once");

    Require(Near(QFlurryAttackRatio(1), 1.10f) &&
                Near(QFlurryAttackRatio(5), 1.30f) &&
                Near(QBonusAttackSpeedPercent(5), 60.0f),
            "Ranger's Focus must use the live 26.10 ratios and attack speed");
    Require(Near(FrostSlowPercent(1), 20.0f) &&
                Near(FrostSlowPercent(18), 30.0f) &&
                Near(EmpoweredFrostSlowPercent(18), 60.0f),
            "Frost Shot normal and critical slows must scale by champion level");
    Require(Near(VolleyRawDamage(3, 80.0f), 210.0f) &&
                Near(ArrowRawDamage(2, 100.0f), 520.0f),
            "W and R damage must match pinned CommunityDragon 16.14");

    Require(Near(ArrowTravelSeconds(0.0f), 0.25f) &&
                Near(ArrowTravelSeconds(5400.0f), 3.25f) &&
                ArrowTravelSeconds(9000.0f) > 4.9f,
            "Crystal Arrow travel must integrate acceleration then max speed");
    Require(Near(ArrowStunSeconds(0.0f), 1.0f) &&
                Near(ArrowStunSeconds(1400.0f), 2.25f) &&
                Near(ArrowStunSeconds(5000.0f), 3.5f),
            "Crystal Arrow stun must clamp from one to three-and-a-half seconds");

    std::vector<ArrowUnit> arrowUnits = {
        { Vec3{ 900.0f, 0.0f, 30.0f }, 55.0f, 2.0f, 10, 1,
          false, false, false, false, true, true },
        { Vec3{ 1600.0f, 0.0f, 0.0f }, 55.0f, 5.0f, 20, 2,
          true, false, false, true, false, true },
        { Vec3{ 1050.0f, 0.0f, 260.0f }, 55.0f, 2.0f, 30, 0,
          false, false, false, false, false, true },
    };
    const ArrowEvaluation firstChampion = EvaluateArrowLine(
        origin, forward, arrowUnits, 20, 2500.0f);
    Require(firstChampion.Valid && firstChampion.FirstHitId == 10 &&
                !firstChampion.FirstHitPrimary &&
                firstChampion.ExplosionHits == 2,
            "R must hit the first champion and explode there, never pass to selection");

    arrowUnits[0].Position.z = 260.0f;
    const ArrowEvaluation selectedFirst = EvaluateArrowLine(
        origin, forward, arrowUnits, 20, 2500.0f);
    Require(selectedFirst.FirstHitId == 20 && selectedFirst.FirstHitPrimary &&
                selectedFirst.FirstHitKillable,
            "R selection becomes valid only when it is the first champion capsule");
    Require(Near(ArrowPathAlignment(
                     forward, Vec3{ -1.0f, 0.0f, 0.0f }), 1.0f) &&
                Near(ArrowPathAlignment(
                     forward, Vec3{ 0.0f, 0.0f, 1.0f }), 0.0f),
            "parallel target paths must score safer than perpendicular jukes");

    FocusContext focus{};
    focus.FocusStacks = 4;
    focus.CastReady = true;
    focus.JustAttacked = true;
    focus.ChampionTarget = true;
    focus.TargetFrosted = true;
    focus.ExpectedFollowupAttacks = 3;
    focus.ManaAfterCast = 50.0f;
    Require(ShouldActivateFocus(focus),
            "Q must reset a real extended champion trade after an attack");
    focus.JustAttacked = false;
    Require(!ShouldActivateFocus(focus),
            "Q must not be dumped before the attack that creates its reset value");
    focus.JustAttacked = true;
    focus.TargetHasPerHitFlatReduction = true;
    focus.ExpectedFollowupAttacks = 2;
    Require(!ShouldActivateFocus(focus),
            "Q must respect five-instance flat physical reduction in a short trade");
    focus.LethalWindow = true;
    Require(ShouldActivateFocus(focus),
            "a confirmed lethal window may override the flat-reduction hold");

    FocusContext jungle = focus;
    jungle.ChampionTarget = false;
    jungle.JungleTarget = true;
    jungle.TargetHasPerHitFlatReduction = false;
    jungle.LethalWindow = false;
    jungle.ExpectedFollowupAttacks = 2;
    Require(!ShouldActivateFocus(jungle),
            "Q must be held when an ordinary camp cannot sustain the flurry");
    jungle.ExpectedFollowupAttacks = 3;
    Require(ShouldActivateFocus(jungle),
            "Q may reset into a durable ordinary jungle camp");

    FocusContext wave = jungle;
    wave.JungleTarget = false;
    wave.WaveTarget = true;
    wave.ExpectedFollowupAttacks = 3;
    Require(!ShouldActivateFocus(wave),
            "Q must not be wasted on a nearly finished lane wave");
    wave.ExpectedFollowupAttacks = 4;
    Require(ShouldActivateFocus(wave),
            "Q may reset while a large safe wave still offers four attacks");

    std::vector<ScoutLandmark> camps = {
        { Vec3{ 500.0f, 0.0f, 40.0f }, 2.0f, 1,
          ScoutKind::Camp, false, false, true },
        { Vec3{ 1000.0f, 0.0f, -50.0f }, 2.0f, 2,
          ScoutKind::Camp, false, true, true },
        { Vec3{ 1800.0f, 0.0f, 700.0f }, 3.0f, 3,
          ScoutKind::Objective, false, true, true },
        { Vec3{ 800.0f, 0.0f, 60.0f }, 2.0f, 4,
          ScoutKind::Camp, true, false, true },
    };
    const ScoutEvaluation route = EvaluateHawkshot(
        origin, Vec3{ 1600.0f, 0.0f, 0.0f }, camps);
    Require(route.Valid && route.Covered >= 3 && route.PriorityCovered >= 1 &&
                route.RecentRepeats == 1,
            "Hawkshot must value a multi-camp line while penalizing repeats");
    Require(HawkshotCoversPoint(
                origin, Vec3{ 1600.0f, 0.0f, 0.0f },
                Vec3{ 1600.0f, 0.0f, 900.0f }) &&
                !HawkshotCoversPoint(
                    origin, Vec3{ 1600.0f, 0.0f, 0.0f },
                    Vec3{ 500.0f, 0.0f, 700.0f }),
            "Hawkshot destination burst and path vision require distinct radii");
    Require(Near(HawkshotTravelSeconds(1400.0f), 1.0f),
            "Hawkshot travel must use the live 1400 speed");

    std::cout << "Ashe geometry/state tests passed\n";
    return 0;
}
