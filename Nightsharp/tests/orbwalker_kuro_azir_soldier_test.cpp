#include "../plugins/Core/OrbwalkerKuro/AzirSoldierSupport.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace OrbwalkerKuro::AzirSoldierSupport;

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
    Require(IsAzirChampionName("AZIR") &&
                IsSandSoldierName("AzirSoldier") &&
                !IsSandSoldierName("AzirSoldierRMissile"),
            "champion and live W soldier aliases must stay distinct from R missiles");
    Require(IsSoldierAttackSpellName("AzirBasicAttackSoldier") &&
                IsSoldierAttackSpellName("azirsoldierbasicattack") &&
                !IsSoldierAttackSpellName("AzirSoldierRMissile"),
            "both player-command and soldier-side attack events must be recognized");

    const Point2 azir{ 0.0f, 0.0f };
    const Point2 edgeSoldier{ 660.0f, 0.0f };
    const Point2 staleSoldier{ 660.1f, 0.0f };
    Require(IsCommandable(azir, edgeSoldier) &&
                !IsCommandable(azir, staleSoldier),
            "a soldier is commandable through the exact 660 tether only");

    const Point2 soldier{ 500.0f, 0.0f };
    Require(CanReachPrimaryTarget(soldier, { 900.0f, 0.0f }, 50.0f) &&
                !CanReachPrimaryTarget(soldier, { 900.1f, 0.0f }, 50.0f),
            "350 attack range must honor target bounding boxes without using spear overflow");
    Require(CanCommandAttack(
                azir, soldier, { 850.0f, 0.0f }, 0.0f,
                TargetKind::OrdinaryUnit),
            "a target reachable only through a live soldier must be attackable");
    Require(!CanCommandAttack(
                azir, soldier, { 850.0f, 0.0f }, 0.0f,
                TargetKind::Structure) &&
                !CanCommandAttack(
                    azir, soldier, { 850.0f, 0.0f }, 0.0f,
                    TargetKind::WardOrTrap),
            "soldiers must never inflate structure, ward, or trap attack range");

    Require(IsWardOrTrapName("YellowTrinket") &&
                IsWardOrTrapName("TeemoMushroom") &&
                IsWardOrTrapName("JhinLotusTrap") &&
                !IsWardOrTrapName("GangplankBarrel"),
            "trap filtering must retain the historical soldier-barrel interaction");

    Require(Near(SoldierBaseDamage(9, 1), 50.0f) &&
                Near(SoldierBaseDamage(10, 1), 58.0f) &&
                Near(SoldierBaseDamage(18, 5), 182.0f),
            "live 26.6 level scaling must add eight damage at levels 10 through 18");
    Require(Near(SoldierApRatio(1), 0.35f) &&
                Near(SoldierApRatio(5), 0.65f) &&
                Near(SoldierRawDamage(18, 5, 100.0f), 247.0f),
            "live W rank damage and AP ratios must match CommunityDragon 16.14");
    Require(Near(MultiSoldierDamageMultiplier(1), 1.0f) &&
                Near(MultiSoldierDamageMultiplier(2), 1.25f) &&
                Near(MultiSoldierDamageMultiplier(3), 1.50f),
            "each additional soldier must contribute exactly twenty-five percent");
    Require(Near(SecondaryLineDamageMultiplier(1), 0.20f) &&
                Near(SecondaryLineDamageMultiplier(9), 0.28f) &&
                Near(SecondaryLineDamageMultiplier(18), 1.0f),
            "secondary spear bodies must use their separate level scaling");

    std::cout << "ALL ORBWALKER KURO AZIR SOLDIER TESTS PASSED\n";
    return 0;
}
