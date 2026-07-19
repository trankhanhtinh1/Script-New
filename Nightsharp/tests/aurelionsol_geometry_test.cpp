#include "../plugins/Champion/KuroAIO/AI/Controllers/AIAurelionSolGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::AurelionSol::Geometry;

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
    Require(Near(LevelScaledCastRange(1), 750.0f) &&
                Near(LevelScaledCastRange(18), 920.0f),
            "Q and grounded E must scale from 750 to 920 by champion level");
    Require(Near(ERange(18, true), 1100.0f),
            "E must use the dedicated 1100 cast range during Astral Flight");
    Require(Near(QMaximumChannelSeconds(1), 3.25f) &&
                QMaximumChannelSeconds(5) >= 160.0f &&
                QMaximumChannelSeconds(1, true) >= 160.0f,
            "rank-five or flying Q must remove the ordinary 3.25 second cap");
    Require(Near(QInitialManaCost(1), 30.0f) &&
                Near(QManaPerSecond(1), 35.0f) &&
                Near(QExpectedManaCost(1, 1.0f), 65.0f),
            "Q must reserve both its initial and continuous live mana costs");
    Require(Near(QReleaseCooldownSeconds(0.20f), 1.0f) &&
                Near(QReleaseCooldownSeconds(0.25f), 3.0f),
            "a sub-quarter-second Q tap must use its special one-second lockout");
    Require(Near(QDamagePerSecond(1, 100.0f), 100.0f) &&
                Near(QBurstRawDamage(1, 100.0f, 100, 2000.0f), 152.0f),
            "Q DPS and one-second burst must use current 55/30 AP ratios and Stardust health damage");
    Require(Near(QBurstPercentHealthDamage(1000, 100000.0f, true), 300.0f),
            "Q percent-health damage must cap at 300 against monsters");
    Require(Near(WQDamageMultiplier(1), 1.08f) &&
                Near(WQDamageMultiplier(5), 1.12f),
            "W must amplify flat Q damage by eight through twelve percent");

    const Vec3 origin{ 0.0f, 0.0f, 0.0f };
    const Vec3 direction{ 1.0f, 0.0f, 0.0f };
    std::vector<BeamUnit> beam = {
        { Vec3{ 620.0f, 0.0f, 0.0f }, 35.0f, 10, false, true },
        { Vec3{ 665.0f, 0.0f, 105.0f }, 120.0f, 20, false, true },
        { Vec3{ 810.0f, 0.0f, 0.0f }, 65.0f, 30, true, true },
    };
    Require(FirstBeamCollisionIndex(origin, direction, beam, 920.0f) == 1,
            "Q first body must order capsule entry, not unit-center distance");
    Require(!BeamFirstHitsId(origin, direction, beam, 920.0f, 30),
            "Q must not claim a champion through a closer minion or monster");
    beam[0].Valid = beam[1].Valid = false;
    Require(BeamFirstHitsId(origin, direction, beam, 920.0f, 30),
            "Q may channel only after the requested champion becomes first body");

    QContactState contact{};
    contact = AdvanceQContact(contact, 30, 0.65f, true);
    contact = AdvanceQContact(contact, 30, 0.40f, true);
    Require(contact.Bursts == 1 && Near(contact.ContinuousSeconds, 0.05f),
            "Q burst clock must carry remainder after one continuous second");
    contact = AdvanceQContact(contact, 31, 0.95f, true);
    Require(contact.TargetId == 31 && contact.Bursts == 0 &&
                Near(contact.ContinuousSeconds, 0.95f),
            "Q burst clock must reset immediately when the primary body changes");
    contact = AdvanceQContact(contact, 31, 0.10f, false);
    Require(contact.TargetId == 0 && contact.Bursts == 0,
            "Q contact loss must erase partial progress rather than bank a burst");
    Require(QStardustFromBursts(3) == 6,
            "each current champion Q burst must grant exactly two Stardust");

    QStartContext start{};
    start.TargetValid = start.RequestedTargetFirst = start.CursorAgrees = true;
    start.ExpectedContactSeconds = 1.05f;
    start.AvailableMana = 140.0f;
    start.RequiredMana = 80.0f;
    Require(ShouldStartQ(start),
            "Q may start when blocker, cursor, contact and reserve all agree");
    start.CursorAgrees = false;
    Require(!ShouldStartQ(start),
            "controller must not steer against the player's cursor intention");
    QStopContext stop{};
    stop.ControllerOwned = true;
    stop.PrimaryContact = false;
    stop.NoContactSeconds = 0.20f;
    stop.BurstDueSeconds = 0.55f;
    Require(ShouldStopQ(stop),
            "controller-owned Q must release after unrecoverable contact loss");
    stop.ControllerOwned = false;
    Require(!ShouldStopQ(stop),
            "manual Q remains player-owned even when the controller dislikes it");

    Require(Near(WRange(0), 1500.0f) && Near(WRange(100), 2250.0f),
            "W range must gain exactly 7.5 units per Stardust");
    Require(Near(WFlightSpeed(60.0f, false), 400.0f) &&
                Near(WFlightSpeed(60.0f, true), 200.0f),
            "channeling Q must halve Astral Flight speed");
    Require(Near(WRemainingCooldownAfterTakedown(20.0f), 2.0f),
            "a qualifying takedown must refund ninety percent of remaining W cooldown");

    FlightContext direct{};
    direct.Origin = origin;
    direct.Destination = Vec3{ 1200.0f, 0.0f, 0.0f };
    direct.Target = Vec3{ 900.0f, 0.0f, 0.0f };
    direct.QRange = 800.0f;
    direct.DirectDive = true;
    direct.Samples = {
        { Vec3{ 300.0f, 0.0f, 0.0f }, 600.0f, 1, 0, true,
          false, false, false, false, false },
        { Vec3{ 700.0f, 0.0f, 0.0f }, 200.0f, 2, 0, true,
          false, true, true, false, false },
        { Vec3{ 1100.0f, 0.0f, 0.0f }, 200.0f, 3, 0, true,
          true, true, true, false, false },
    };
    FlightContext offset = direct;
    offset.Destination = Vec3{ 900.0f, 0.0f, 650.0f };
    offset.DirectDive = false;
    offset.Samples = {
        { Vec3{ 250.0f, 0.0f, 180.0f }, 675.0f, 1, 1, true,
          false, false, false, false, true },
        { Vec3{ 540.0f, 0.0f, 390.0f }, 530.0f, 1, 1, true,
          false, false, false, false, true },
        { Vec3{ 850.0f, 0.0f, 610.0f }, 612.0f, 1, 1, true,
          false, false, false, false, true },
    };
    Require(FlightRouteScore(offset) > FlightRouteScore(direct) + 1500.0f,
            "offset terrain-separated W must beat a direct flight into CC and turret");
    Require(ShouldStopFlight({ true, true, false, true, false, false, false, true }),
            "W must recast before its endpoint enters an enemy turret");
    Require(!ShouldStopFlight({ true, false, false, true, false, false, false, true }),
            "W cannot request an impossible recast during its first half second");

    Require(Near(SingularityRadius(0), 275.0f) &&
                Near(SingularityRadius(100), 322.91f, 0.05f),
            "E outer radius must preserve area and add 900 area units per Stardust");
    Require(Near(SingularityInnerRadius(0), 120.0f) &&
                Near(SingularityInnerRadius(100), 141.88f, 0.05f),
            "E execute center must use the live 120 base and 180 area-per-stack fields");
    Require(Near(SingularityExecutePercent(100), 7.6f),
            "E execute threshold must gain 2.6 percentage points per 100 Stardust");
    Require(SingularityExecutes(75.0f, 1000.0f, false, true, 100) &&
                !SingularityExecutes(75.0f, 1000.0f, true, true, 100),
            "E may execute a 7.5 percent ordinary target but never an epic monster");
    Require(Near(SingularityDamagePerSecond(5, 100.0f), 42.0f),
            "E must use current 30 plus twelve percent AP per second");
    Require(DeathStardust(EUnitKind::SmallMinion) == 1 &&
                DeathStardust(EUnitKind::LargeMinion) == 2 &&
                DeathStardust(EUnitKind::Champion) == 2 &&
                DeathStardust(EUnitKind::EpicMonster) == 2,
            "current E death values are one for small bodies and two for all listed large bodies");
    Require(CannonWaveStardust() == 8,
            "six ordinary minions plus one cannon must yield eight E Stardust");

    std::vector<SingularityUnit> field = {
        { Vec3{ 40.0f, 0.0f, 0.0f }, 65.0f, 50.0f, 1000.0f,
          2.0f, 2.4f, EUnitKind::Champion, false, true, false, true },
        { Vec3{ 160.0f, 0.0f, 0.0f }, 35.0f, 10.0f, 300.0f,
          1.0f, 0.0f, EUnitKind::LargeMinion, true, false, false, true },
        { Vec3{ 250.0f, 0.0f, 0.0f }, 35.0f, 200.0f, 300.0f,
          1.0f, 0.0f, EUnitKind::SmallMinion, false, false, false, true },
    };
    const auto e = EvaluateSingularity(origin, 100, field, true);
    Require(e.Champions == 1 && e.ExpectedDeaths == 2 &&
                e.ExpectedStardust == 6 && e.Executions == 1,
            "E planner must combine full champion seconds, cannon death and center execute stacks");

    Require(Near(FallingStarRadius(0), 275.0f) &&
                Near(SkiesDescendRadius(0), 388.91f, 0.05f) &&
                SkiesDescendRadius(200) > FallingStarRadius(200),
            "empowered R must double base area and grow by 1500 area units per Stardust");
    Require(Near(FallingStarRawDamage(3, 100.0f), 425.0f) &&
                Near(SkiesDescendDirectRawDamage(3, 100.0f), 531.25f) &&
                Near(SkiesDescendShockwaveRawDamage(3, 100.0f), 382.5f),
            "R must apply current 75 AP ratio, 1.25 direct and 0.9 shockwave multipliers");
    Require(ResolveUltimateImpactCenter(
                Vec3{ 900.0f, 0.0f, 0.0f },
                Vec3{ 430.0f, 0.0f, 0.0f }, true).x == 430.0f,
            "projectile interception must relocate the R impact instead of pretending it disappears");

    std::vector<UltimateUnit> ultimate = {
        { Vec3{ 100.0f, 0.0f, 0.0f }, 65.0f, 2.0f,
          true, false, true, false, false, false, true, true },
        { Vec3{ 300.0f, 0.0f, 0.0f }, 65.0f, 1.0f,
          true, false, false, true, false, false, false, true },
        { Vec3{ 900.0f, 0.0f, 0.0f }, 65.0f, 1.5f,
          true, false, false, false, false, true, false, true },
        { Vec3{ 1200.0f, 0.0f, 0.0f }, 100.0f, 1.2f,
          false, true, false, false, false, false, false, true },
        { Vec3{ 700.0f, 0.0f, 0.0f }, 35.0f, 0.2f,
          false, false, false, false, false, false, false, true },
    };
    const auto regular = EvaluateUltimate(origin, 0, false, ultimate);
    Require(regular.DirectHits == 2 && regular.ExpectedStardust == 5,
            "regular R direct coverage must count a shielded champion but not award its Stardust");
    const auto skies = EvaluateUltimate(origin, 0, true, ultimate);
    Require(skies.DirectHits == 2 && skies.ShockwaveDamageHits == 2 &&
                skies.ShockwaveSlowHits == 3,
            "empowered R shockwave must damage champions/epics, slow all non-direct enemies and never double-hit direct victims");
    Require(!UltimateCanInterrupt(0.8f, false) &&
                UltimateCanInterrupt(1.2f, false) &&
                !UltimateCanInterrupt(1.5f, true),
            "R interrupt logic must compare remaining channel against the correct 1.25 or 2 second impact delay");

    CalamityState calamity{};
    calamity = AdvanceCalamity(calamity, 74, false);
    Require(!calamity.Ready && calamity.Progress == 74,
            "R upgrade must not round 74 accumulated Stardust up to ready");
    calamity = AdvanceCalamity(calamity, 5, false);
    Require(calamity.Ready && calamity.Progress == 75,
            "R upgrade progress must clamp and become ready at 75");
    calamity = AdvanceCalamity(calamity, 0, true);
    Require(!calamity.Ready && calamity.Progress == 0,
            "casting empowered R must consume only its post-R-learning progress");
    Require(ShouldSpendEmpoweredUltimate(skies, true, false, false, 3) &&
                !ShouldSpendEmpoweredUltimate(regular, false, false, false, 3),
            "empowered R may spend for objective shockwave value while an ordinary single catch cannot satisfy that policy");

    std::cout << "ALL AURELION SOL GEOMETRY TESTS PASSED\n";
    return 0;
}
