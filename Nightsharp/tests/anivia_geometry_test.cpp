#include "../plugins/Champion/KuroAIO/AI/Controllers/AIAniviaGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Anivia::Geometry;

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
    const Vec3 origin{ 0.0f, 0.0f, 0.0f };
    const Vec3 direction{ 1.0f, 0.0f, 0.0f };

    Require(FlashFrostPassHits(
                origin, direction, 700.0f,
                Vec3{ 650.0f, 0.0f, 159.0f }, 50.0f) &&
                !FlashFrostPassHits(
                    origin, direction, 700.0f,
                    Vec3{ 650.0f, 0.0f, 161.0f }, 50.0f),
            "Q pass must use current 220 full width plus gameplay radius");
    Require(FlashFrostExplosionHits(
                Vec3{ 720.0f, 0.0f, 0.0f },
                Vec3{ 995.0f, 0.0f, 0.0f }, 50.0f) &&
                !FlashFrostExplosionHits(
                    Vec3{ 720.0f, 0.0f, 0.0f },
                    Vec3{ 996.0f, 0.0f, 0.0f }, 50.0f),
            "Q explosion must use 225 radius plus gameplay radius");
    Require(Near(FlashFrostTravelSeconds(950.0f), 1.25f),
            "Q impact clock must add 0.25 cast and 950-speed travel");

    const Vec3 target{ 650.0f, 0.0f, 0.0f };
    Require(DoubleHitDetonationWindow(
                origin, direction, Vec3{ 670.0f, 0.0f, 0.0f },
                target, 55.0f) &&
                !DoubleHitDetonationWindow(
                    origin, direction, Vec3{ 640.0f, 0.0f, 0.0f },
                    target, 55.0f),
            "Q should detonate after the pass, not before target center");
    Require(DetonationStillRecoverable(
                origin, direction, Vec3{ 670.0f, 0.0f, 0.0f },
                target, Vec3{ 0.0f, 0.0f, 0.0f }, 55.0f) &&
                !DetonationStillRecoverable(
                    origin, direction, Vec3{ 920.0f, 0.0f, 0.0f },
                    target, Vec3{ -300.0f, 0.0f, 0.0f }, 55.0f),
            "Q recast forecast must distinguish recoverable and lost explosions");

    Require(Near(FrostbiteImpactSeconds(800.0f), 0.75f),
            "E impact clock must add cast time and 1600-speed missile travel");
    Require(FrostbiteWillBeEmpowered(1000, 1750, 1800) &&
                !FrostbiteWillBeEmpowered(1000, 1750, 1760) &&
                FrostbiteWillBeEmpowered(1000, 1750, 0, 1680),
            "E must evaluate Chill at impact and allow a scheduled Q/R Chill race");

    Require(Near(FlashFrostPassRawDamage(5, 100.0f), 155.0f) &&
                Near(FlashFrostExplosionRawDamage(5, 100.0f), 245.0f) &&
                Near(FlashFrostRawDamage(5, 100.0f, true, true), 400.0f),
            "Q must keep pass and explosion damage as separate current instances");
    Require(Near(FrostbiteRawDamage(5, 100.0f, false), 210.0f) &&
                Near(FrostbiteRawDamage(5, 100.0f, true), 420.0f),
            "E must use current 155 plus 55 percent AP and double only when Chilled");

    Require(CrystallizeSegments(1) == 4 &&
                CrystallizeSegments(5) == 8 &&
                Near(CrystallizeOccupiedWidth(1), 600.0f) &&
                Near(CrystallizeOccupiedWidth(5), 1000.0f),
            "W ranks must model four-to-eight segments and occupied end caps");
    const WallSegment wall = BuildWallSegment(
        origin, Vec3{ 500.0f, 0.0f, 0.0f }, 3);
    Require(wall.Valid &&
                Near(wall.Start.z, -300.0f) &&
                Near(wall.End.z, 300.0f),
            "W must be perpendicular to the caster-to-center direction");
    Require(WallContains(wall, Vec3{ 590.0f, 0.0f, 350.0f }, 50.0f) &&
                !WallContains(wall, Vec3{ 651.0f, 0.0f, 350.0f }, 50.0f),
            "W collision must include segment and gameplay radii");
    const Vec3 pushed = WallDisplacementDestination(
        wall, Vec3{ 530.0f, 0.0f, 0.0f }, 50.0f);
    Require(pushed.x > 760.0f,
            "W displacement must clear the wall on the target's existing side");
    Require(WallBlocksPath(
                wall, Vec3{ 0.0f, 0.0f, 0.0f },
                Vec3{ 1000.0f, 0.0f, 0.0f }, 35.0f) &&
                !WallBlocksPath(
                    wall, Vec3{ 0.0f, 0.0f, 500.0f },
                    Vec3{ 1000.0f, 0.0f, 500.0f }, 35.0f),
            "W ally-safety must detect only paths crossing the occupied wall");

    Require(Near(StormRadius(0.0f), 200.0f) &&
                Near(StormRadius(0.75f), 300.0f) &&
                Near(StormRadius(1.5f), 400.0f) &&
                StormIsFull(1.49f),
            "R must grow linearly from 200 to 400 over 1.5 seconds");
    Require(StormHits(
                origin, Vec3{ 450.0f, 0.0f, 0.0f }, 1.5f, 50.0f) &&
                !StormHits(
                    origin, Vec3{ 451.0f, 0.0f, 0.0f }, 1.5f, 50.0f),
            "full R must use 400 radius plus gameplay radius");
    Require(StormTickCount(0.0f) == 1 &&
                StormTickCount(0.49f) == 1 &&
                StormTickCount(0.50f) == 2,
            "R must preserve its immediate and half-second tick cadence");
    Require(Near(StormManaPerSecond(1), 35.0f) &&
                Near(StormManaPerSecond(3), 55.0f) &&
                Near(StormManaAfter(200.0f, 3, 2.0f), 90.0f),
            "R mana forecast must use current rank drain");
    Require(Near(StormRawDamagePerTick(3, 100.0f, false), 36.25f) &&
                Near(StormRawDamagePerTick(3, 100.0f, true), 108.75f),
            "full R tick must deal exactly three times growing-storm damage");

    const Vec3 lead = LeadStormCenter(
        Vec3{ 500.0f, 0.0f, 0.0f },
        Vec3{ 400.0f, 0.0f, 0.0f }, 0.75f, 190.0f);
    Require(Near(lead.x, 690.0f),
            "R lead must project movement but cap pathological over-leading");
    std::vector<StormUnit> units = {
        { Vec3{ 100.0f, 0.0f, 0.0f }, 50.0f, 1.0f,
          false, false, true },
        { Vec3{ 250.0f, 0.0f, 0.0f }, 50.0f, 1.6f,
          true, false, true },
        { Vec3{ 390.0f, 0.0f, 0.0f }, 50.0f, 1.0f,
          false, true, true },
    };
    Require(StormScore(origin, 1.5f, units) > 4.2f,
            "R placement score must reward valuable, controlled and dashing targets");

    std::cout << "ALL ANIVIA GEOMETRY TESTS PASSED\n";
    return 0;
}
