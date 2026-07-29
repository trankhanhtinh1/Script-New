#include "../plugins/Champion/KuroAIO/AI/Controllers/AIApheliosGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Aphelios::Geometry;

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
    Require(WeaponFromRuntimeName("ApheliosCalibrumQ") == Weapon::Calibrum &&
                WeaponFromRuntimeName("apheliosoffhandbuffcrescendum") ==
                    Weapon::Crescendum,
            "runtime names must identify weapons case-insensitively");
    Require(IsCalibrumMarkBuffName(
                "ApheliosCalibrumBonusRangeBuff") &&
                IsCalibrumMarkBuffName(
                    "aphelioscalibrumbonusrangedebuff") &&
                !IsCalibrumMarkBuffName("ApheliosGravitumDebuff"),
            "both SDK Calibrum mark aliases must map to one mark state");
    Require(ObservedWeaponAmmo(true, 37, false, -1) == 37 &&
                ObservedWeaponAmmo(false, -1, true, 12) == 12 &&
                ObservedWeaponAmmo(true, 4, true, 31) == 31 &&
                ObservedWeaponAmmo(true, 99, false, -1) == -1,
            "manager/off-hand ammo stacks must reconcile and reject invalid counters");

    WeaponState state{};
    Require(HasUniqueWeapons(state) && state.QueueKnown,
            "default state must contain all five unique guns");
    SwapHands(state);
    Require(state.Main == Weapon::Severum &&
                state.Offhand == Weapon::Calibrum,
            "Phase must exchange only main and off-hand");
    SetAmmo(state, Weapon::Severum, 7);
    const auto transition = ConsumeMainAmmo(state, 10);
    Require(transition.Depleted && transition.SpentWeapon == Weapon::Severum &&
                state.Main == Weapon::Calibrum &&
                state.Offhand == Weapon::Gravitum &&
                state.Queue[2] == Weapon::Severum &&
                AmmoOf(state, Weapon::Severum) == 50,
            "low-ammo Q must rotate off-hand, queue head and exhausted gun exactly");

    WeaponState unknown = state;
    unknown.QueueKnown = false;
    const Weapon before = unknown.Main;
    SetAmmo(unknown, before, 1);
    const auto uncertain = ConsumeMainAmmo(unknown, 10);
    Require(!uncertain.Depleted && unknown.Main == before,
            "an unknown queue must not invent a depletion transition");

    Require(SameCycleModuloRotation(
                StandardCycle,
                { Weapon::Infernum, Weapon::Severum,
                  Weapon::Crescendum, Weapon::Calibrum,
                  Weapon::Gravitum }) &&
                !SameCycleModuloRotation(StandardCycle, GreenBlueCycle),
            "cycle comparison must allow rotation but distinguish green-blue order");

    CombatContext duel{};
    duel.PlayerHealthPercent = 70.0f;
    duel.TargetDistance = 330.0f;
    duel.CanCommitClose = true;
    duel.Chakrams = 4;
    Require(PairSynergyScore(
                Weapon::Severum, Weapon::Crescendum, duel) >
            PairSynergyScore(
                Weapon::Calibrum, Weapon::Severum, duel),
            "red-white must outscore the weak green-red pair in a close duel");
    CombatContext catchContext{};
    catchContext.NeedCatch = true;
    catchContext.TargetDistance = 1100.0f;
    Require(PairSynergyScore(
                Weapon::Calibrum, Weapon::Gravitum, catchContext) >
            PairSynergyScore(
                Weapon::Gravitum, Weapon::Crescendum, catchContext),
            "green-purple must win a long-range catch over purple-white");
    CombatContext grouped{};
    grouped.GroupedEnemies = 4;
    grouped.ObjectiveActive = true;
    Require(WeaponTacticalScore(Weapon::Infernum, grouped) >
            WeaponTacticalScore(Weapon::Calibrum, grouped),
            "Infernum must win a grouped objective context");

    WeaponState survival{};
    survival.Main = Weapon::Severum;
    survival.Offhand = Weapon::Crescendum;
    survival.Queue = {
        Weapon::Calibrum, Weapon::Gravitum, Weapon::Infernum,
    };
    CombatContext lowHealth{};
    lowHealth.PlayerHealthPercent = 25.0f;
    Require(ChooseRotationPlan(survival, lowHealth) ==
                RotationPlan::HoldSurvivalGun &&
                ShouldHoldWeapon(Weapon::Severum, lowHealth, 20),
            "rotation policy must hold Severum during a lethal-health window");
    CombatContext objective{};
    objective.ObjectiveSoon = true;
    Require(ShouldHoldWeapon(Weapon::Crescendum, objective, 20),
            "rotation policy must hold useful white ammo before an objective");

    Require(LowAmmoAbilitySwaps(1) && LowAmmoAbilitySwaps(10) &&
                !LowAmmoAbilitySwaps(11),
            "low-ammo swap window must be one through ten ammo");
    Require(LevelBreakpointIndex(1) == 0 &&
                LevelBreakpointIndex(13) == 6 &&
                LevelBreakpointIndex(18) == 6,
            "weapon ratios must use seven odd-level breakpoints capped at thirteen");

    Require(Near(CalibrumQRawDamage(13, 100.0f, 50.0f), 270.0f) &&
                Near(CalibrumMarkRawDamage(100.0f), 30.0f),
            "Calibrum Q and mark must use current level-thirteen and 15-percent ratios");
    Require(Near(SeverumQPerHitRawDamage(13, 200.0f), 82.0f) &&
                SeverumQAttackCount(100.0f) == 8,
            "Severum Q must reach 41-percent total AD and scale attack count");
    Require(Near(GravitumQRawDamage(13, 100.0f, 50.0f), 225.0f),
            "Gravitum Q must reach 140 plus 50-percent bAD plus 70-percent AP");
    Require(Near(InfernumQRawDamage(13, 100.0f, 50.0f), 166.0f),
            "Infernum Q must reach 110 plus 21-percent bAD plus 70-percent AP");
    Require(Near(CrescendumSentryRawDamage(13, 100.0f, 50.0f), 202.0f),
            "Sentry must reach 125 plus 52-percent bAD plus 50-percent AP");
    Require(Near(MoonlightVigilRawDamage(3, 100.0f, 50.0f), 295.0f),
            "R must use 225 plus 20-percent bAD plus 100-percent AP");
    Require(Near(UltimateWeaponBonus(Weapon::Calibrum, 3, 100.0f), 110.0f) &&
                Near(UltimateWeaponBonus(Weapon::Severum, 3, 100.0f), 450.0f) &&
                Near(UltimateWeaponBonus(Weapon::Infernum, 3, 100.0f), 175.0f) &&
                Near(UltimateWeaponBonus(Weapon::Gravitum, 3, 100.0f), 1.35f),
            "all non-white R weapon bonuses must match current live values");

    Require(Near(CalibrumQImpactSeconds(900.0f), 0.85f),
            "Moonshot impact must include cast time and 1800-speed travel");
    const Vec3 origin{ 0.0f, 0.0f, 0.0f };
    const Vec3 greenEnd{ 1450.0f, 0.0f, 0.0f };
    Require(CalibrumQHits(
                origin, greenEnd, Vec3{ 900.0f, 0.0f, 70.0f }, 45.0f) &&
                !CalibrumQHits(
                    origin, greenEnd, Vec3{ 900.0f, 0.0f, 90.0f }, 45.0f),
            "Moonshot line must combine 30 half-width with gameplay radius");
    const Vec3 blueAim{ 850.0f, 0.0f, 0.0f };
    Require(InfernumQHits(
                origin, blueAim, Vec3{ 700.0f, 0.0f, 250.0f }, 45.0f) &&
                !InfernumQHits(
                    origin, blueAim, Vec3{ 700.0f, 0.0f, 430.0f }, 45.0f),
            "Duskwave must use cone geometry rather than a line");
    std::vector<AreaUnit> cone = {
        { Vec3{ 600.0f, 0.0f, 80.0f }, 45.0f, 1.0f,
          true, false, false, false, true },
        { Vec3{ 700.0f, 0.0f, -120.0f }, 45.0f, 1.5f,
          false, true, false, false, true },
        { Vec3{ -400.0f, 0.0f, 0.0f }, 45.0f, 5.0f,
          false, false, false, false, true },
    };
    Require(InfernumQScore(origin, blueAim, cone) > 5.0f,
            "Duskwave scoring must reward the primary and grouped in-cone targets only");

    Require(Near(MiniChakramBonusRatio(1), 0.15f) &&
                Near(MiniChakramBonusRatio(3), 0.405f) &&
                MiniChakramBonusRatio(12) > MiniChakramBonusRatio(6),
            "mini-chakram ratios must diminish from fifteen toward five percent");
    Require(CrescendumDpsScore(250.0f, 4, false) >
                CrescendumDpsScore(600.0f, 4, false) &&
                CrescendumDpsScore(600.0f, 4, true) >
                    CrescendumDpsScore(600.0f, 4, false),
            "white DPS must reward close returns and movement toward the blade");

    SentryContext sentry{};
    sentry.Player = origin;
    sentry.Position = Vec3{ 450.0f, 0.0f, 0.0f };
    sentry.PredictedTarget = Vec3{ 700.0f, 0.0f, 0.0f };
    sentry.TargetRadius = 50.0f;
    sentry.ExpectedTargets = 2;
    sentry.CalibrumOffhand = true;
    Require(SentryPlacementScore(sentry) > 8.0f,
            "safe Calibrum-offhand Sentry contact must be valuable");
    sentry.GivesEnemyDashTarget = true;
    Require(SentryPlacementScore(sentry) < -900.0f,
            "a Sentry that gives an enemy dash target must be rejected");
    sentry.GivesEnemyDashTarget = false;
    sentry.UnderEnemyTurret = true;
    Require(SentryPlacementScore(sentry) < -900.0f,
            "an enemy-turret Sentry must be rejected");

    Require(MoonlightVigilPathHits(
                origin, Vec3{ 1300.0f, 0.0f, 0.0f },
                Vec3{ 700.0f, 0.0f, 90.0f }, 40.0f) &&
                MoonlightVigilExplosionHits(
                    Vec3{ 700.0f, 0.0f, 0.0f },
                    Vec3{ 900.0f, 0.0f, 0.0f }, 20.0f),
            "R must retain separate line collision and first-hit explosion geometry");
    Require(Near(MoonlightVigilImpactSeconds(800.0f), 1.30f),
            "R impact must include half-second cast and 1000-speed travel");

    UltimateContext teamfight{};
    teamfight.HitCount = 4;
    teamfight.PriorityHits = 2;
    teamfight.ObjectiveFight = true;
    Require(UltimateVariantScore(Weapon::Infernum, teamfight) >
            UltimateVariantScore(Weapon::Calibrum, teamfight),
            "Infernum R must win a four-target objective explosion");
    UltimateContext emergency{};
    emergency.PlayerHealthPercent = 18.0f;
    emergency.HitCount = 1;
    emergency.EnemyDiving = true;
    emergency.NeedPeel = true;
    Require(ChooseUltimateWeapon(
                Weapon::Infernum, Weapon::Severum, emergency) ==
                    Weapon::Severum,
            "Severum R must beat blue damage during a lethal dive");
    UltimateContext pick{};
    pick.HitCount = 1;
    pick.NeedCatch = true;
    pick.TargetEscaping = true;
    Require(ChooseUltimateWeapon(
                Weapon::Calibrum, Weapon::Gravitum, pick) ==
                    Weapon::Gravitum,
            "Gravitum R must beat Calibrum for a guaranteed escaping-target catch");

    CombatContext swapCombo{};
    swapCombo.GroupedEnemies = 3;
    Require(ChooseLowAmmoCombo(
                Weapon::Infernum, Weapon::Gravitum,
                Weapon::Crescendum, 8, swapCombo) ==
                    LowAmmoCombo::InfernumIntoIncoming &&
                ChooseLowAmmoCombo(
                    Weapon::Infernum, Weapon::Gravitum,
                    Weapon::Crescendum, 11, swapCombo) ==
                    LowAmmoCombo::None,
            "blue-to-white combo must require a real low-ammo transition");
    CombatContext rootCombo{};
    rootCombo.NeedCatch = true;
    Require(ChooseLowAmmoCombo(
                Weapon::Severum, Weapon::Gravitum,
                Weapon::Crescendum, 10, rootCombo) ==
                    LowAmmoCombo::SeverumGravitumRootIntoIncoming,
            "red-purple must root before the incoming white gun");

    Require(CanFitPreMarkAuto({ 0.30f, 0.65f, 4.5f }) &&
                !CanFitPreMarkAuto({ 0.70f, 0.65f, 4.5f }),
            "pre-mark attack coaching must respect the mark arrival reset window");
    Require(GravitumRootValue(0, 0, true, true) < -900.0f &&
                GravitumRootValue(2, 1, true, false) > 8.0f,
            "Binding Eclipse must reject zero marks and reward a valuable interrupt root");

    std::cout << "ALL APHELIOS GEOMETRY TESTS PASSED\n";
    return 0;
}
