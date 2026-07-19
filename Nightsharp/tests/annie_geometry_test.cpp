#include "../plugins/Champion/KuroAIO/AI/Controllers/AIAnnieGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Annie::Geometry;

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
    const auto ordinaryQ = SimulatePyromania(3, {
        { 100, 0, PassiveSpell::Q, PassiveEventKind::QDamageLanding },
    });
    Require(ordinaryQ.Consumer == PassiveSpell::None &&
                ordinaryQ.FinalStacks == 4,
            "three-stack Q must prime on impact but must not stun itself");

    const auto hiddenQ = SimulatePyromania(3, {
        { 40, 0, PassiveSpell::E, PassiveEventKind::GainOnCast },
        { 100, 0, PassiveSpell::Q, PassiveEventKind::QDamageLanding },
    });
    Require(hiddenQ.Consumer == PassiveSpell::Q && hiddenQ.StunApplied &&
                hiddenQ.FinalStacks == 0,
            "Q then E before impact must convert a hidden third stack into Q stun");

    const auto delayedConsumer = SimulatePyromania(2, {
        { 10, 0, PassiveSpell::W, PassiveEventKind::GainOnCast },
        { 20, 0, PassiveSpell::E, PassiveEventKind::GainOnCast },
        { 110, 0, PassiveSpell::Q, PassiveEventKind::QDamageLanding },
        { 250, 0, PassiveSpell::W, PassiveEventKind::DamageLanding },
    });
    Require(delayedConsumer.Consumer == PassiveSpell::Q &&
                delayedConsumer.ConsumeTick == 110,
            "the first primed damage landing must own the stun independent of cast order");

    const auto wSteals = SimulatePyromania(3, {
        { 0, 0, PassiveSpell::W, PassiveEventKind::GainOnCast },
        { 250, 0, PassiveSpell::W, PassiveEventKind::DamageLanding },
        { 300, 0, PassiveSpell::Q, PassiveEventKind::QDamageLanding },
    });
    Require(wSteals.Consumer == PassiveSpell::W && wSteals.FinalStacks == 1,
            "a primed W landing before Q must consume stun and let later Q rebuild one stack");

    const auto shieldedQ = SimulatePyromania(4, {
        { 100, 0, PassiveSpell::Q, PassiveEventKind::QDamageLanding,
          true, false },
    });
    Require(shieldedQ.Consumer == PassiveSpell::Q &&
                shieldedQ.StunBlocked && shieldedQ.FinalStacks == 0,
            "spell shield must block but consume the primed Q stun");

    Require(Near(DisintegrateImpactSeconds(525.0f), 0.625f),
            "Q impact must include 0.25 cast time and 1400-speed travel");
    Require(Near(DisintegrateRawDamage(5, 100.0f), 340.0f),
            "Q must use live rank-five 260 plus 80 percent AP");
    const auto refund = ResolveDisintegrate(120.0f, 5, 4.0f, true);
    Require(Near(refund.ManaAfter, 200.0f) &&
                Near(refund.CooldownSeconds, 2.0f),
            "Q kill must refund its live mana cost and halve cooldown");
    const auto noRefund = ResolveDisintegrate(120.0f, 5, 4.0f, false);
    Require(Near(noRefund.ManaAfter, 120.0f) &&
                Near(noRefund.CooldownSeconds, 4.0f),
            "nonlethal Q must keep ordinary mana and cooldown");

    const Vec3 origin{ 0.0f, 0.0f, 0.0f };
    const Vec3 aim{ 600.0f, 0.0f, 0.0f };
    Require(IncinerateHits(origin, aim, Vec3{ 500.0f, 0.0f, 220.0f }, 45.0f) &&
                !IncinerateHits(origin, aim, Vec3{ 500.0f, 0.0f, 300.0f }, 45.0f),
            "W cone must use its live angular edge plus gameplay radius");
    Require(IncinerateHits(origin, aim, Vec3{ -20.0f, 0.0f, 0.0f }, 35.0f) &&
                !IncinerateHits(origin, aim, Vec3{ -60.0f, 0.0f, 0.0f }, 35.0f),
            "W apex may clip a target slightly behind but not a detached circle");
    const Vec3 blinked{ 400.0f, 0.0f, 0.0f };
    Require(IncinerateResolveOrigin(origin, blinked, 180, 250).x == 400.0f &&
                IncinerateResolveOrigin(origin, blinked, 280, 250).x == 0.0f,
            "W must originate at post-Flash position only when relocation precedes resolve");
    Require(Near(IncinerateRawDamage(5, 100.0f), 310.0f),
            "W must use live rank-five 230 plus 80 percent AP");

    std::vector<ConeUnit> coneUnits = {
        { Vec3{ 420.0f, 0.0f, 40.0f }, 50.0f, 1.0f,
          true, false, false, true },
        { Vec3{ 500.0f, 0.0f, -90.0f }, 50.0f, 1.4f,
          false, false, true, true },
        { Vec3{ -300.0f, 0.0f, 0.0f }, 50.0f, 2.0f,
          false, false, false, true },
    };
    Require(IncinerateScore(origin, aim, coneUnits) > 5.0f,
            "W score must reward primary and dashing targets inside the cone only");

    Require(Near(MoltenShieldAmount(5, 100.0f), 240.0f) &&
                Near(MoltenShieldReactionRawDamage(5, 100.0f), 105.0f),
            "E shield and one-per-enemy reaction must retain separate live ratios");
    Require(Near(MoltenShieldMoveSpeedPercent(1), 20.0f) &&
                Near(MoltenShieldMoveSpeedPercent(18), 50.0f) &&
                Near(MoltenShieldMoveSpeedAt(0.75f, 18), 25.0f),
            "E move speed must scale by champion level and decay over 1.5 seconds");

    Require(TibbersSummonHits(origin, Vec3{ 300.0f, 0.0f, 0.0f }, 50.0f) &&
                !TibbersSummonHits(origin, Vec3{ 301.0f, 0.0f, 0.0f }, 50.0f),
            "R initial hit must use 250 radius plus gameplay radius");
    Require(Near(SummonTibbersRawDamage(3, 100.0f), 475.0f),
            "R summon must use live 400 plus 75 percent AP");
    Require(Near(TibbersAuraRawDamagePerSecond(3, 100.0f), 20.0f) &&
                Near(TibbersAuraRawDamagePerTick(3, 100.0f), 5.0f) &&
                TibbersAuraTickCount(1.0f) == 4,
            "Tibbers aura must preserve per-second damage and quarter-second ticks");
    Require(TibbersAuraHits(
                origin, Vec3{ 400.0f, 0.0f, 0.0f }, 50.0f) &&
                !TibbersAuraHits(
                    origin, Vec3{ 401.0f, 0.0f, 0.0f }, 50.0f),
            "Tibbers aura must use its live 350 radius plus gameplay radius");
    Require(Near(TibbersAttackRawDamage(3, 100.0f), 70.0f),
            "Tibbers attack must use live 60 plus 10 percent AP");
    Require(Near(TibbersEnrageAttackSpeed(0), 1.736f) &&
                Near(TibbersEnrageAttackSpeed(4), 0.739f) &&
                Near(TibbersEnrageAttackSpeedMultiplier(0), 2.7776f) &&
                Near(TibbersEnrageAttackSpeedMultiplier(4), 1.1824f) &&
                Near(TibbersEnrageAttackSpeedMultiplier(5), 1.0f),
            "Tibbers must use the five live enrage attack-speed stages");

    std::vector<CircleUnit> circleUnits = {
        { Vec3{ 100.0f, 0.0f, 0.0f }, 50.0f, 1.0f,
          true, false, false, false, true },
        { Vec3{ 220.0f, 0.0f, 0.0f }, 50.0f, 1.5f,
          false, false, true, false, true },
        { Vec3{ 100.0f, 0.0f, 100.0f }, 50.0f, 2.0f,
          false, false, false, true, true },
    };
    Require(TibbersSummonScore(origin, circleUnits, false) >
                TibbersSummonScore(origin, circleUnits, true),
            "primed R placement must penalize wasting its stun into spell shield");

    Require(ChoosePetCommand({ true, true, true, false, false, false,
                               true, 100.0f, 200.0f, 100.0f }) ==
                PetCommand::Hold,
            "soft pet autopilot must yield during the manual ownership lock");
    Require(ChoosePetCommand({ true, false, true, true, false, false,
                               true, 100.0f, 200.0f, 100.0f }) ==
                PetCommand::Hold,
            "pet must not dive an enemy turret without explicit permission");
    Require(ChoosePetCommand({ true, false, true, false, false, false,
                               true, 100.0f, 200.0f, 100.0f }) ==
                PetCommand::Attack,
            "an enraged safe Tibbers must immediately attack the combat target");
    Require(ChoosePetCommand({ true, false, false, false, false, false,
                               false, 10.0f, 300.0f, 0.0f }) ==
                PetCommand::MoveToOwner &&
                ChoosePetCommand({ true, false, false, false, false, true,
                                   false, 80.0f, 400.0f, 0.0f }) ==
                    PetCommand::MoveToZone,
            "pet policy must recover critical health and allow deliberate zoning");

    std::cout << "ALL ANNIE GEOMETRY TESTS PASSED\n";
    return 0;
}
