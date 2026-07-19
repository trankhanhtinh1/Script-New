#include "../plugins/Champion/KuroAIO/AI/Controllers/AITaliyahGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Taliyah::Geometry;

namespace {

int ScenarioCount = 0;

void Require(bool condition, const char* message) {
    ++ScenarioCount;
    if (!condition) {
        std::cerr << "FAILED [" << ScenarioCount << "]: "
                  << message << '\n';
        std::exit(1);
    }
}

bool Near(float left, float right, float epsilon = 0.05f) {
    return std::fabs(left - right) <= epsilon;
}

QBody Body(int id,
           float x,
           float z = 0.0f,
           float radius = 35.0f,
           bool champion = false) {
    QBody body{};
    body.Id = id;
    body.Position = Vec3{ x, 0.0f, z };
    body.Radius = radius;
    body.Health = body.MaximumHealth = 500.0f;
    body.Champion = champion;
    body.Minion = !champion;
    return body;
}

QContext GoodQ() {
    QContext context{};
    context.Ready = context.HasMana = context.TargetValid = true;
    context.InRange = context.CleanContact = context.HighConfidence = true;
    context.CursorAgrees = true;
    context.CollisionConfidence = 0.9f;
    return context;
}

WContext GoodW() {
    WContext context{};
    context.Ready = context.HasMana = context.TargetValid = true;
    context.InRange = context.CenterValid = context.DirectionValid = true;
    context.HighConfidence = context.TargetCommitted = true;
    context.CursorAgrees = true;
    context.MineContacts = 2;
    context.Distance = 600.0f;
    return context;
}

EContext GoodE() {
    EContext context{};
    context.Ready = context.HasMana = context.TargetValid = true;
    context.InRange = context.CastPositionValid = true;
    context.TargetCommitted = context.WReady = context.WWillCrossMines = true;
    context.QReady = true;
    context.ExpectedInitialHits = 1;
    context.ExpectedMineContacts = 2;
    return context;
}

ComboContext GoodCombo() {
    ComboContext context{};
    context.TargetValid = context.QReady = context.WReady = true;
    context.EReady = context.WCanHit = context.ECanHit = true;
    context.BoulderCanHit = context.Safe = true;
    context.CurrentMana = 1000.0f;
    context.Costs = { 55.0f, 40.0f, 90.0f, 100.0f };
    return context;
}

WallContext GoodWall() {
    WallContext context{};
    context.Ready = context.HasMana = context.ManualAuthorized = true;
    context.OriginValid = context.EndpointValid = true;
    context.CursorAgrees = context.RouteNavigable = true;
    context.Distance = 2200.0f;
    return context;
}

} // namespace

int main() {
    // Current 16.14 / Riot 26.2 and 26.9 data.
    Require(Near(QRockRawDamage(1, 100.0f), 105.0f),
            "rank-one Q must use 55 base and 50 percent AP");
    Require(Near(QRockRawDamage(5, 100.0f), 175.0f),
            "rank-five Q must use 125 base");
    Require(Near(QRockRawDamage(0, 999.0f), 0.0f),
            "unlearned Q must deal no simulated damage");
    Require(Near(QRockRawDamage(1, -100.0f), 55.0f),
            "negative AP telemetry must clamp");
    Require(Near(QVolleyRawDamage(1, 100.0f), 273.0f),
            "five-rock Q must total 2.6 rock payloads");
    Require(Near(QBigRawDamage(1, 100.0f), 189.0f),
            "worked-ground boulder must total 1.8 rock payloads");
    Require(QVolleyRawDamage(5, 300.0f) > QBigRawDamage(5, 300.0f),
            "full volley must remain higher total champion damage");
    Require(Near(QMonsterFlatDamage(1), 20.0f) &&
                Near(QMonsterFlatDamage(5), 40.0f),
            "26.9 monster flat damage must scale 20 through 40");
    Require(Near(QMonsterVolleyRawDamage(1, 0.0f), 243.0f),
            "monster volley must add flat damage to all five rocks");
    Require(Near(QMonsterBigRawDamage(1, 0.0f), 135.0f),
            "monster boulder must multiply rock plus flat modifier by 1.8");
    Require(Near(QBigSlowPercent(1), 20.0f) &&
                Near(QBigSlowPercent(5), 40.0f),
            "boulder slow must scale 20 through 40 percent");
    Require(Near(QBigSlowPercent(0), 0.0f),
            "unlearned Q must not fabricate a slow");
    Require(Near(ERawInitialDamage(1, 100.0f), 120.0f),
            "E initial hit must use 60 base and 60 percent AP");
    Require(Near(ERawInitialDamage(5, 100.0f), 300.0f),
            "rank-five E initial hit must use 240 base");
    Require(Near(ERawMineDamage(1, 100.0f), 55.0f),
            "E mine must use 25 base and 30 percent AP");
    Require(Near(ERawMineDamage(5, 100.0f), 115.0f),
            "rank-five E mine must use 85 base");
    Require(Near(EMineDamageMultiplier(0), 0.0f),
            "zero mine contacts must add no damage");
    Require(Near(EMineDamageMultiplier(1), 1.0f),
            "first E mine must deal full damage");
    Require(Near(EMineDamageMultiplier(2), 1.75f),
            "second E mine must retain 75 percent");
    Require(Near(EMineDamageMultiplier(3), 2.25f),
            "third E mine must retain 50 percent");
    Require(Near(EMineDamageMultiplier(4), 2.50f),
            "four E mines must reach the live 2.5 cap");
    Require(Near(EMineDamageMultiplier(12), 2.50f),
            "extra telemetry contacts must not exceed the damage cap");
    Require(Near(ERawDetonationDamage(1, 100.0f, 4), 137.5f),
            "four mines must multiply the rank-one payload by 2.5");
    Require(Near(ERawTotalDamage(1, 100.0f, 4, false), 257.5f),
            "E total must include initial plus detonations");
    Require(Near(ERawTotalDamage(1, 100.0f, 4, true), 579.375f),
            "26.2 monster modifier must be 225 percent");
    Require(Near(QCooldownSeconds(1), 7.0f) &&
                Near(QCooldownSeconds(5), 3.0f),
            "Q cooldown must scale 7 through 3 seconds");
    Require(Near(WorkedGroundQCooldownSeconds(1), 3.5f) &&
                Near(WorkedGroundQCooldownSeconds(5), 1.5f),
            "worked-ground Q must halve live cooldown");
    Require(Near(WorkedGroundQCooldownSeconds(0), 0.0f),
            "unlearned Q must have no simulated cooldown");
    Require(Near(RRange(1), 2500.0f) && Near(RRange(2), 4500.0f) &&
                Near(RRange(3), 6500.0f),
            "R range must use all three live ranks");
    Require(Near(kWImpactSeconds, 0.75f),
            "W planner must include cast plus eruption delay");
    Require(Near(kEStunSeconds, 0.75f) &&
                Near(kEMaximumStunSeconds, 2.0f),
            "E stun and cap must match live data");

    // Accelerating normal Q versus fixed-speed big Q.
    Require(Near(QProjectileDistance(0.0f, false), 0.0f),
            "normal Q must start at zero travel");
    Require(Near(QProjectileDistance(0.1f, false), 335.0f),
            "normal Q distance must include deceleration");
    Require(Near(QProjectileTravelSeconds(335.0f, false), 0.1f, 0.001f),
            "normal Q travel inverse must recover time");
    Require(QProjectileTravelSeconds(1000.0f, false) > 0.37f &&
                QProjectileTravelSeconds(1000.0f, false) < 0.38f,
            "normal Q must reach 1000 after about 0.376 seconds");
    Require(Near(QProjectileDistance(0.5f, false), 1000.0f),
            "normal Q distance must clamp to cast range");
    Require(Near(QProjectileTravelSeconds(2000.0f, false),
                 QProjectileTravelSeconds(1000.0f, false)),
            "normal Q inverse must clamp malformed range");
    Require(Near(QProjectileDistance(0.25f, true), 500.0f),
            "big Q must travel at fixed 2000 speed");
    Require(Near(QProjectileTravelSeconds(1000.0f, true), 0.5f),
            "big Q must take half a second to max range");
    Require(QProjectileTravelSeconds(800.0f, true) >
                QProjectileTravelSeconds(800.0f, false),
            "big Q must be slower than the accelerating normal rock");

    const Vec3 origin{ 0.0f, 0.0f, 0.0f };
    const Vec3 aim{ 1000.0f, 0.0f, 0.0f };
    QBody target = Body(10, 700.0f, 0.0f, 40.0f, true);
    QContact contact = ContactWithQBody(origin, aim, target, false);
    Require(contact.Hit && contact.BodyId == 10,
            "stationary champion in normal Q corridor must be hit");
    Require(contact.ProjectileSeconds < 0.30f &&
                contact.CastElapsedSeconds > contact.ProjectileSeconds,
            "normal Q contact must separate cast delay from fast flight");
    Require(contact.MissileDistance < 700.0f,
            "capsule edge must be contacted before its center");
    QContact bigContact = ContactWithQBody(origin, aim, target, true);
    Require(bigContact.Hit &&
                bigContact.ProjectileSeconds > contact.ProjectileSeconds,
            "fixed-speed boulder must contact the same body later");
    target.Position.z = 100.0f;
    Require(!ContactWithQBody(origin, aim, target).Hit,
            "body outside combined Q radius must miss");
    target.Position.z = 89.0f;
    Require(ContactWithQBody(origin, aim, target).Hit,
            "body touching combined Q radius must hit");
    QBody entering = Body(11, 620.0f, 240.0f, 30.0f);
    entering.Velocity = Vec3{ 0.0f, 0.0f, -520.0f };
    Require(ContactWithQBody(origin, aim, entering).Hit,
            "moving blocker entering the lane must be resolved");
    QBody leaving = Body(12, 620.0f, 0.0f, 30.0f);
    leaving.Velocity = Vec3{ 0.0f, 0.0f, 900.0f };
    Require(!ContactWithQBody(origin, aim, leaving).Hit,
            "blocker leaving during cast delay must not remain static");
    QBody edge = Body(13, 1040.0f, 0.0f, 40.0f, true);
    Require(ContactWithQBody(origin, aim, edge).Hit,
            "Q endpoint may touch a champion bounding edge");
    edge.Position.x = 1100.0f;
    Require(!ContactWithQBody(origin, aim, edge).Hit,
            "Q must not invent lollipop range");
    QBody behind = Body(14, -100.0f);
    Require(!ContactWithQBody(origin, aim, behind).Hit,
            "Q cannot collide behind Taliyah");
    QBody invalid = Body(15, 500.0f);
    invalid.Targetable = false;
    Require(!ContactWithQBody(origin, aim, invalid).Hit,
            "untargetable units cannot block Q");
    invalid = Body(16, 500.0f);
    invalid.Hostile = false;
    Require(!ContactWithQBody(origin, aim, invalid).Hit,
            "allies cannot block Q");
    invalid = Body(0, 500.0f);
    Require(!ContactWithQBody(origin, aim, invalid).Hit,
            "zero-id bodies must be rejected");
    Require(!ContactWithQBody(origin, origin, Body(17, 500.0f)).Hit,
            "degenerate Q aim must fail safely");

    QBody minion = Body(20, 430.0f);
    target = Body(21, 800.0f, 0.0f, 40.0f, true);
    QContact first = FirstQContact(origin, aim, { target, minion });
    Require(first.Hit && first.BodyId == 20,
            "nearest first body must win regardless of vector order");
    Require(!QHitsIntended(origin, aim, { target, minion }, 21,
                           false, false),
            "direct-only normal Q must reject a blocker");
    QBody splashTarget = Body(22, 430.0f, 190.0f, 30.0f, true);
    Require(QHitsIntended(origin, aim, { minion, splashTarget }, 22,
                          false, true, &first),
            "normal Q may bridge through first-body 175 AoE");
    Require(first.BodyId == 20,
            "AoE bridge must preserve the actual first body");
    splashTarget.Position.z = 245.0f;
    Require(!QHitsIntended(origin, aim, { minion, splashTarget }, 22,
                           false, true),
            "normal Q must reject targets outside first-body splash");
    splashTarget.Position.z = 245.0f;
    Require(QHitsIntended(origin, aim, { minion, splashTarget }, 22,
                          true, true),
            "big Q 225 AoE may bridge a wider minion angle");
    std::vector<int> victims = QSplashVictimIds(
        FirstQContact(origin, aim, { minion, Body(23, 430.0f, 100.0f) }),
        { minion, Body(23, 430.0f, 100.0f), Body(24, 900.0f, 0.0f) },
        false);
    Require(victims.size() == 2 && victims[0] == 20 && victims[1] == 23,
            "splash victim list must include only bodies around impact");
    QBody tieHigh = Body(31, 500.0f);
    QBody tieLow = Body(30, 500.0f);
    Require(FirstQContact(origin, aim, { tieHigh, tieLow }).BodyId == 30,
            "equal Q contacts must tie-break by network id");

    // Worked Ground state and exact boundary behavior.
    std::vector<WorkedGroundZone> zones;
    const Vec3 castPoint{ 500.0f, 0.0f, 500.0f };
    QCastTransition transition = ApplyQCastToWorkedGround(
        zones, castPoint, 35.0f, 10.0f, 1);
    Require(transition.Form == QForm::Volley &&
                transition.CreatedZoneId == 1 && zones.size() == 1,
            "fresh-ground Q must create one 30-second zone");
    Require(ActiveWorkedGround(zones[0], 39.999f),
            "worked ground must remain active before expiry");
    Require(!ActiveWorkedGround(zones[0], 40.0f),
            "worked ground must expire exactly at 30 seconds");
    Require(WorkedGroundAt(zones, castPoint, 20.0f, 0.0f) == 1,
            "zone center must be recognized");
    Require(WorkedGroundAt(zones,
              Vec3{ 899.0f, 0.0f, 500.0f }, 20.0f, 0.0f) == 1,
            "point inside 400 radius must be recognized");
    Require(WorkedGroundAt(zones,
              Vec3{ 901.0f, 0.0f, 500.0f }, 20.0f, 0.0f) == 0,
            "point outside radius must not fabricate big Q");
    Require(WorkedGroundAt(zones,
              Vec3{ 925.0f, 0.0f, 500.0f }, 20.0f, 30.0f) == 1,
            "player capsule may overlap the zone edge");
    transition = ApplyQCastToWorkedGround(
        zones, castPoint, 35.0f, 20.0f, 2);
    Require(transition.Form == QForm::Boulder && transition.ZoneId == 1,
            "Q while standing in worked ground must become a boulder");
    Require(zones[0].Consumed,
            "boulder Q must consume its specific zone");
    NormalizeWorkedGround(zones, 20.1f);
    Require(zones.empty(),
            "consumed zones must leave the active ledger");
    Require(AddWorkedGround(zones, castPoint, 30.0f, 7, true) == 7 &&
                zones[0].Confirmed,
            "confirmed runtime zones must preserve provenance");
    Require(ConsumeWorkedGround(zones, 7),
            "existing zone must be consumable once");
    Require(!ConsumeWorkedGround(zones, 7),
            "consumed zone must not be consumed twice");
    Require(AddWorkedGround(zones, {}, 30.0f, 8) == 0,
            "zero center must not create phantom ground");
    zones.clear();
    AddWorkedGround(zones, Vec3{ 500.0f, 0.0f, 500.0f }, 10.0f, 9);
    AddWorkedGround(zones, Vec3{ 550.0f, 0.0f, 500.0f }, 10.0f, 8);
    Require(WorkedGroundAt(zones,
              Vec3{ 525.0f, 0.0f, 500.0f }, 20.0f) == 8,
            "equal overlap distance must deterministically choose lower id");
    NormalizeWorkedGround(zones, 40.0f);
    Require(zones.empty(),
            "normalization must remove expired zones");

    // Six-row, 22-mine geometry and sequence timing.
    Minefield field = BuildMinefield(
        Vec3{ 100.0f, 0.0f, 100.0f },
        Vec3{ 1050.0f, 0.0f, 100.0f }, 10.0f);
    Require(field.Valid && field.Count == 22,
            "E must reconstruct exactly 22 mines");
    Require(field.Mines[0].Row == 0 && field.Mines[1].Row == 0 &&
                field.Mines[2].Row == 1,
            "first E row must contain two mines then four-per-row begins");
    Require(field.Mines[21].Row == 5 && field.Mines[21].Column == 3,
            "last E mine must be row six column four");
    Require(Near(field.Mines[0].SpawnAt, 10.25f),
            "first row must spawn after the E cast");
    Require(Near(field.Mines[2].SpawnAt - field.Mines[0].SpawnAt, 0.17f),
            "second row must follow after 0.17 seconds");
    Require(Near(field.Mines[21].SpawnAt, 11.10f),
            "sixth row must respect five row intervals");
    Require(Near(field.Mines[21].ExpiresAt - field.Mines[21].SpawnAt, 4.0f),
            "each mine must last four seconds from its own spawn");
    Require(field.Mines[0].Position.x < field.Mines[21].Position.x,
            "rows must extend outward from Taliyah");
    Require(!BuildMinefield(origin, origin).Valid,
            "degenerate E direction must fail safely");
    Require(PointInMinefieldEnvelope(field,
              Vec3{ 600.0f, 0.0f, 100.0f }),
            "field centerline must be inside E envelope");
    Require(!PointInMinefieldEnvelope(field,
              Vec3{ 600.0f, 0.0f, 600.0f }),
            "far lateral point must be outside E envelope");
    Require(PointInMinefieldEnvelope(field,
              Vec3{ 600.0f, 0.0f, 320.0f }, 50.0f),
            "target capsule may overlap a minefield edge");
    Require(!PointInMinefieldEnvelope(Minefield{}, castPoint),
            "invalid minefield envelope must reject safely");

    // E-first then W: W resolves after all rows exist.
    const Vec3 shoveStart{ 830.0f, 0.0f, 100.0f };
    const Vec3 shoveBack{ 430.0f, 0.0f, 100.0f };
    MineContactSummary contacts = CountMineContacts(
        field, shoveStart, shoveBack, 35.0f, 11.25f, 0.5f);
    Require(contacts.Contacts >= 2,
            "E-first backward W must cross multiple spawned rows");
    Require(contacts.DistinctRows >= contacts.Contacts,
            "mine contacts must track distinct rows");
    Require(contacts.DamageMultiplier >= 1.75f,
            "multi-row shove must receive falloff-aware damage");
    Require(contacts.FirstPathT <= contacts.LastPathT,
            "mine contact order must be monotonic");
    Require(contacts.Contacts <= 4,
            "mine summary must cap damaging contacts at four");

    // W-first then E: at W impact only early rows are available.
    Minefield lateField = BuildMinefield(
        Vec3{ 100.0f, 0.0f, 100.0f },
        Vec3{ 1050.0f, 0.0f, 100.0f }, 20.25f);
    MineContactSummary earlyContacts = CountMineContacts(
        lateField, shoveStart, shoveBack, 35.0f, 20.75f, 0.5f);
    MineContactSummary matureContacts = CountMineContacts(
        lateField, shoveStart, shoveBack, 35.0f, 21.50f, 0.5f);
    Require(earlyContacts.Contacts < matureContacts.Contacts,
            "W-E fast sequence must expose fewer rows than mature E-W");
    Require(CountMineContacts(field, shoveStart, shoveStart,
                              35.0f, 11.0f, 0.5f).Contacts == 0,
            "zero-length displacement must not detonate mines");
    Require(CountMineContacts(Minefield{}, shoveStart, shoveBack,
                              35.0f, 11.0f, 0.5f).Contacts == 0,
            "invalid field must not detonate mines");
    EMine future = field.Mines[21];
    Require(!SegmentTouchesMine(shoveStart, shoveBack, future, 35.0f,
                                10.0f, 0.1f),
            "mine cannot detonate before its row exists");
    Require(SegmentTouchesMine(shoveStart, shoveBack,
                               field.Mines[12], 35.0f, 11.2f, 0.5f),
            "spawned mine intersected by the shove must detonate");
    Require(!SegmentTouchesMine(shoveStart, shoveBack,
                                field.Mines[12], 35.0f, 16.0f, 0.5f),
            "expired mine must not detonate");

    Vec3 destination = WDestination(
        Vec3{ 500.0f, 0.0f, 500.0f }, Vec3{ 10.0f, 0.0f, 0.0f });
    Require(Near(destination.x, 900.0f) && Near(destination.z, 500.0f),
            "W destination must normalize direction and move 400 units");
    destination = WDestination(
        Vec3{ 500.0f, 0.0f, 500.0f }, Vec3{ -1.0f, 0.0f, 1.0f });
    Require(Near(destination.Distance2D(Vec3{ 500.0f, 0.0f, 500.0f }),
                 400.0f),
            "diagonal W must still throw exactly 400 units");
    Require(WDestination(castPoint, {}).IsZero(),
            "zero W direction must fail safely");

    // Spell policy gates.
    QContext q = GoodQ();
    Require(EvaluateQ(q).Cast,
            "clean high-confidence volley must cast");
    q.Ready = false;
    Require(!EvaluateQ(q).Cast,
            "unready Q must not cast");
    q = GoodQ(); q.HasMana = false;
    Require(!EvaluateQ(q).Cast,
            "Q must respect mana");
    q = GoodQ(); q.CleanContact = false;
    Require(!EvaluateQ(q).Cast,
            "blocked Q without splash bridge must reject");
    q.AoeBridge = true;
    Require(EvaluateQ(q).Cast,
            "verified AoE bridge may cast");
    q = GoodQ(); q.ProjectileWallBlocked = true;
    Require(!EvaluateQ(q).Cast,
            "Wind Wall-style interception must reject Q");
    q = GoodQ(); q.TargetSpellShield = true;
    Require(!EvaluateQ(q).Cast,
            "spell shield must deny ordinary Q");
    q = GoodQ(); q.TargetImmune = true;
    Require(!EvaluateQ(q).Cast,
            "immunity must deny Q");
    q = GoodQ(); q.HighConfidence = false;
    Require(!EvaluateQ(q).Cast,
            "low-confidence ordinary Q must wait");
    q.TargetDashing = true;
    Require(EvaluateQ(q).Cast,
            "known dash path may relax ordinary prediction");
    q = GoodQ(); q.PlayerAttackWindingUp = true;
    Require(!EvaluateQ(q).Cast,
            "nonlethal Q must preserve attack windup");
    q.Lethal = true;
    Require(EvaluateQ(q).Cast,
            "lethal Q may cancel low-value windup");
    q = GoodQ(); q.Form = QForm::Boulder; q.FullVolleyPreferred = true;
    Require(!EvaluateQ(q).Cast,
            "jungle sustained DPS must preserve full volley");
    q.Purpose = QPurpose::BoulderSetup;
    Require(EvaluateQ(q).Cast,
            "boulder setup may override sustained preference");
    q = GoodQ(); q.CursorAgrees = false;
    Require(!EvaluateQ(q).Cast,
            "ordinary poke must cooperate with player direction");
    q.Lethal = true;
    Require(EvaluateQ(q).Cast,
            "exact lethal may ignore cursor drift");
    q = GoodQ(); q.Form = QForm::Boulder; q.ComboFollowupReady = true;
    Require(EvaluateQ(q).Score > EvaluateQ(GoodQ()).Score,
            "boulder that unlocks W-E must outscore plain poke");

    WContext w = GoodW();
    Require(EvaluateW(w).Cast,
            "reliable W through two mine rows must cast");
    w.Ready = false;
    Require(!EvaluateW(w).Cast,
            "unready W must reject");
    w = GoodW(); w.DirectionValid = false;
    Require(!EvaluateW(w).Cast,
            "invalid vector direction must reject W");
    w = GoodW(); w.TargetSpellShield = true;
    Require(!EvaluateW(w).Cast,
            "spell shield must deny W");
    w = GoodW(); w.DestinationTerrain = true;
    Require(!EvaluateW(w).Cast,
            "W must not plan an invalid destination");
    w = GoodW(); w.PushesThreatTowardCarry = true;
    Require(!EvaluateW(w).Cast,
            "ordinary W must not deliver a diver to the carry");
    w = GoodW(); w.PushesTowardEnemySafety = true;
    Require(!EvaluateW(w).Cast,
            "nonlethal W must not save target under its turret");
    w.LethalCombo = true;
    Require(EvaluateW(w).Cast,
            "lethal mine conversion may accept target safety direction");
    w = GoodW(); w.Purpose = WPurpose::PeelPlayer;
    w.ImprovesPeelDistance = false;
    Require(!EvaluateW(w).Cast,
            "peel W must actually increase separation");
    w.ImprovesPeelDistance = true;
    Require(EvaluateW(w).Cast,
            "correct away-vector peel must cast");
    w = GoodW(); w.HighConfidence = w.TargetCommitted = false;
    Require(!EvaluateW(w).Cast,
            "W must wait for mobility commitment");
    w.TargetSlowedByBoulder = true;
    Require(EvaluateW(w).Cast,
            "big-Q slow must unlock W reliability");
    w = GoodW(); w.MineContacts = 0;
    Require(!EvaluateW(w).Cast,
            "combo W without a mine path must wait");
    w.Purpose = WPurpose::Interrupt; w.Reactive = true;
    Require(EvaluateW(w).Cast,
            "reactive interrupt W does not require mine damage");
    w = GoodW(); w.CursorAgrees = false; w.AlliedFollowup = 0;
    Require(!EvaluateW(w).Cast,
            "unforced offensive vector must respect player cursor");
    w.AlliedFollowup = 2;
    Require(EvaluateW(w).Cast,
            "clear allied follow-up may override small cursor disagreement");
    w = GoodW(); w.Purpose = WPurpose::Interrupt;
    w.PushesTowardEnemySafety = true;
    Require(EvaluateW(w).Cast,
            "timely channel interruption must outrank ordinary shove safety");

    EContext e = GoodE();
    Require(EvaluateE(e).Cast,
            "E with a verified W path must cast");
    e.Ready = false;
    Require(!EvaluateE(e).Cast,
            "unready E must reject");
    e = GoodE(); e.TargetImmune = true;
    Require(!EvaluateE(e).Cast,
            "immune target must deny E damage plan");
    e = GoodE(); e.WWillCrossMines = false;
    Require(!EvaluateE(e).Cast,
            "combo E without a W crossing must reject");
    e = GoodE(); e.TargetDashing = true; e.WWillCrossMines = false;
    e.Purpose = EPurpose::DashPunish;
    Require(EvaluateE(e).Cast,
            "anti-dash E must not wait for W");
    e = GoodE(); e.Purpose = EPurpose::Wave;
    e.ExpectedInitialHits = 1;
    Require(!EvaluateE(e).Cast,
            "lane E must not spend 90 mana on one minion");
    e.ExpectedInitialHits = 4;
    Require(EvaluateE(e).Cast,
            "multi-minion E wave value may cast");
    e = GoodE(); e.Purpose = EPurpose::Jungle;
    e.ExpectedInitialHits = 1;
    e.WReady = e.WWillCrossMines = false;
    e.TargetCommitted = e.TargetHasReadyDash = false;
    Require(EvaluateE(e).Cast,
            "single jungle monster may justify E's live 225-percent modifier");
    e.HoldForChampion = true;
    Require(!EvaluateE(e).Cast,
            "wave E must be held during a champion contest");
    e = GoodE(); e.WReady = false; e.WWillCrossMines = false;
    e.TargetCommitted = false; e.TargetHasReadyDash = false;
    Require(!EvaluateE(e).Cast,
            "E must preserve denial when no conversion exists");
    e.TargetHasReadyDash = true;
    Require(EvaluateE(e).Cast,
            "ready enemy dash gives E standalone denial value");
    e = GoodE(); e.TargetSpellShield = true; e.TargetDashing = false;
    Require(!EvaluateE(e).Cast,
            "ordinary E must not feed a spell shield");
    e.TargetDashing = true; e.Purpose = EPurpose::DashPunish;
    Require(EvaluateE(e).Cast,
            "dash path zone may be cast despite a spell shield");

    const ManaCosts costs{ 55.0f, 40.0f, 90.0f, 100.0f };
    Require(DefinitionFor(ComboBranch::BoulderWEQ).Count == 4,
            "big-Q branch must publish Q-W-E-Q");
    Require(DefinitionFor(ComboBranch::FastWEQ).Slots[0] == 1 &&
                DefinitionFor(ComboBranch::FastWEQ).Slots[1] == 2,
            "fast branch must begin W-E");
    Require(DefinitionFor(ComboBranch::ControlledEWQ).Slots[0] == 2 &&
                DefinitionFor(ComboBranch::ControlledEWQ).Slots[1] == 1,
            "controlled branch must begin E-W");
    Require(Near(BranchMana(ComboBranch::BoulderWEQ, costs, true), 195.0f),
            "big-Q W-E-Q must price 10 + W + E + normal Q");
    Require(Near(BranchMana(ComboBranch::BoulderWEQ, costs, false), 240.0f),
            "non-boulder pricing must retain both normal Q costs");
    Require(Near(BranchMana(ComboBranch::FastWEQ, costs), 185.0f),
            "W-E-Q must price one of each basic spell");
    Require(Near(BranchMana(ComboBranch::ControlledEWQ, costs), 185.0f),
            "E-W-Q must price the same spells in different order");
    Require(Near(BranchMana(ComboBranch::EQPoke, costs), 145.0f),
            "E-Q poke must price E plus Q");
    Require(Near(BranchMana(ComboBranch::QPoke, costs), 55.0f),
            "Q poke must price one Q");
    Require(Near(BranchMana(ComboBranch::None, costs), 0.0f),
            "empty branch must cost no mana");

    ComboContext combo = GoodCombo();
    combo.TargetDashing = true;
    Require(ChooseComboBranch(combo) == ComboBranch::DashPunishEWQ,
            "active dash must prioritize E punishment");
    combo = GoodCombo(); combo.OnWorkedGround = true;
    Require(ChooseComboBranch(combo) == ComboBranch::BoulderWEQ,
            "clean worked-ground target must select big-Q W-E-Q");
    combo = GoodCombo(); combo.OnWorkedGround = false;
    combo.FastFollowupWindow = true;
    Require(ChooseComboBranch(combo) == ComboBranch::FastWEQ,
            "ally follow-up window must select fast W-E-Q");
    combo = GoodCombo(); combo.OnWorkedGround = false;
    combo.TargetHasReadyDash = true;
    Require(ChooseComboBranch(combo) == ComboBranch::ControlledEWQ,
            "ready enemy dash must select E-W control");
    combo = GoodCombo(); combo.WReady = false;
    Require(ChooseComboBranch(combo) == ComboBranch::EQPoke,
            "without W the planner may use E-Q");
    combo = GoodCombo(); combo.WReady = combo.EReady = false;
    Require(ChooseComboBranch(combo) == ComboBranch::QPoke,
            "Q-only state must select Q poke");
    combo = GoodCombo(); combo.Safe = false;
    Require(ChooseComboBranch(combo) == ComboBranch::None,
            "unsafe target must reject every offensive branch");
    combo = GoodCombo(); combo.CurrentMana = 20.0f;
    Require(ChooseComboBranch(combo) == ComboBranch::None,
            "branch selection must respect live mana");
    combo = GoodCombo(); combo.OnWorkedGround = true;
    combo.PreserveWForPeel = true; combo.TargetHasReadyDash = true;
    Require(ChooseComboBranch(combo) == ComboBranch::EQPoke,
            "protected-carry threat must preserve W from offensive branches");

    // Manual-only Weaver's Wall partition policy.
    const Vec3 wallStart{ 0.0f, 0.0f, 0.0f };
    const Vec3 wallEnd{ 2000.0f, 0.0f, 0.0f };
    Require(SignedWallSide(wallStart, wallEnd,
                           Vec3{ 500.0f, 0.0f, 100.0f }) > 0.0f,
            "positive Z point must be on wall left side");
    Require(SignedWallSide(wallStart, wallEnd,
                           Vec3{ 500.0f, 0.0f, -100.0f }) < 0.0f,
            "negative Z point must be on wall right side");
    std::vector<WallUnit> wallUnits = {
        { Vec3{ 300.0f, 0.0f, 300.0f }, true, false, false, false, false },
        { Vec3{ 500.0f, 0.0f, -300.0f }, true, false, false, false, false },
        { Vec3{ 900.0f, 0.0f, 100.0f }, false, true, false, true, false },
        { Vec3{ 1200.0f, 0.0f, -400.0f }, false, true, false, false, false },
    };
    WallSplit split = AnalyzeWallSplit(wallStart, wallEnd, wallUnits);
    Require(split.AlliesLeft == 1 && split.AlliesRight == 1,
            "wall split must count allies on both sides");
    Require(split.EnemiesLeft == 1 && split.EnemiesRight == 1,
            "wall split must count enemies on both sides");
    Require(split.EnemiesKnockedAside == 1,
            "only enemy within near-wall radius should be displaced");
    Require(split.PriorityEnemySeparated,
            "priority enemy opposite an ally must be recognized as separated");
    std::vector<WallUnit> priorityFirst = {
        { Vec3{ 900.0f, 0.0f, 100.0f }, false, true, false, true, false },
        { Vec3{ 500.0f, 0.0f, -300.0f }, true, false, false, false, false },
    };
    Require(AnalyzeWallSplit(wallStart, wallEnd, priorityFirst)
                .PriorityEnemySeparated,
            "wall separation must not depend on unit iteration order");
    wallUnits.push_back(
        { Vec3{ 700.0f, 0.0f, 50.0f }, true, false, true, false, false });
    wallUnits.push_back(
        { Vec3{ 800.0f, 0.0f, -40.0f }, true, false, false, false, true });
    split = AnalyzeWallSplit(wallStart, wallEnd, wallUnits);
    Require(split.ProtectedAlliesNearWall == 1,
            "wall must detect protected ally near formation path");
    Require(split.ChannelingAlliesNearWall == 1,
            "wall must detect allied channel near formation path");

    WallContext wall = GoodWall();
    Require(EvaluateWall(wall).Cast,
            "clean player-authorized wall may cast");
    wall.ManualAuthorized = false;
    Require(!EvaluateWall(wall).Cast,
            "controller must never automate R without player key");
    wall = GoodWall(); wall.Distance = 500.0f;
    Require(!EvaluateWall(wall).Cast,
            "short low-value wall must reject");
    wall = GoodWall(); wall.PlayerRecentlyDamaged = true;
    Require(!EvaluateWall(wall).Cast,
            "three-second damage lockout must reject R");
    wall = GoodWall(); wall.PlayerImmobilized = true;
    Require(!EvaluateWall(wall).Cast,
            "immobilized Taliyah cannot start R");
    wall = GoodWall(); wall.InterruptThreat = true;
    Require(!EvaluateWall(wall).Cast,
            "ready interrupt in channel range must reject R");
    wall = GoodWall(); wall.RouteNavigable = false;
    Require(!EvaluateWall(wall).Cast,
            "invalid route must reject manual wall");
    wall = GoodWall(); wall.Split.ProtectedAlliesNearWall = 1;
    Require(!EvaluateWall(wall).Cast,
            "wall must not displace the protected carry");
    wall = GoodWall(); wall.Split.ChannelingAlliesNearWall = 1;
    Require(!EvaluateWall(wall).Cast,
            "wall must not disrupt an allied channel");
    wall = GoodWall(); wall.Purpose = WallPurpose::Objective;
    wall.ObjectiveSecuredSide = false;
    Require(!EvaluateWall(wall).Cast,
            "objective wall must actually partition the approach");
    wall.ObjectiveSecuredSide = true;
    Require(EvaluateWall(wall).Cast,
            "verified objective partition may cast");
    wall = GoodWall(); wall.Purpose = WallPurpose::Escape;
    wall.EscapeSeparatesPursuers = false;
    Require(!EvaluateWall(wall).Cast,
            "escape wall must separate a pursuer");
    wall.EscapeSeparatesPursuers = true;
    Require(EvaluateWall(wall).Cast,
            "verified escape partition may cast");

    std::cout << "ALL AITALIYAH GEOMETRY TESTS PASSED ("
              << ScenarioCount << " scenarios)\n";
    return 0;
}
