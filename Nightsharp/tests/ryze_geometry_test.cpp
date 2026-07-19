#include "../plugins/Champion/KuroAIO/AI/Controllers/AIRyzeGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Ryze::Geometry;

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

bool Near(float left, float right, float epsilon = 0.04f) {
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
    body.Champion = champion;
    body.Minion = !champion;
    body.Health = 500.0f;
    body.MaximumHealth = 500.0f;
    return body;
}

QContext GoodQ() {
    QContext context{};
    context.Ready = context.HasMana = context.TargetValid = true;
    context.IntendedFirstBody = context.HighConfidence = true;
    context.CursorAgrees = true;
    context.CollisionConfidence = 0.9f;
    return context;
}

WContext GoodW() {
    WContext context{};
    context.Ready = context.HasMana = context.TargetValid = true;
    context.InRange = context.FollowupQReady = true;
    context.TargetCommitted = true;
    return context;
}

EContext GoodE() {
    EContext context{};
    context.Ready = context.HasMana = context.TargetValid = true;
    context.InRange = context.QReady = true;
    return context;
}

ComboContext Combo() {
    ComboContext context{};
    context.QReady = context.WReady = context.EReady = true;
    context.TargetValid = context.CleanQ = context.SafeToCommit = true;
    context.TargetDistance = 500.0f;
    context.CurrentMana = 2000.0f;
    context.Costs = { 40.0f, 50.0f, 35.0f, 100.0f };
    return context;
}

WarpContext GoodWarp() {
    WarpContext context{};
    context.Ready = context.HasMana = context.ManualAuthorized = true;
    context.OriginValid = context.DestinationValid = true;
    context.DestinationNavigable = context.DestinationHasVision = true;
    context.CursorAgrees = true;
    context.Distance = 1800.0f;
    context.AlliesAtDestination = 1;
    context.AlliesInPortal = 1;
    return context;
}

} // namespace

int main() {
    // Live 16.14 data with Riot 26.3 and 26.12 reconciliation.
    Require(Near(ArcaneMasteryMaximumMana(1000.0f, 200.0f), 1200.0f),
            "passive must add ten percent maximum mana per 100 AP");
    Require(Near(ArcaneMasteryMaximumMana(-50.0f, 200.0f), 0.0f),
            "passive must clamp malformed negative mana");
    Require(Near(ArcaneMasteryMaximumMana(1000.0f, -50.0f), 1000.0f),
            "passive must not subtract mana for malformed AP");
    Require(Near(QRawDamage(1, 100.0f, 1000.0f), 150.0f),
            "rank-one Q must use 75 base, 55 percent AP and two percent bonus mana");
    Require(Near(QRawDamage(5, 100.0f, 1000.0f), 230.0f),
            "rank-five Q must use 155 base");
    Require(Near(QRawDamage(0, 999.0f, 9999.0f), 0.0f),
            "unlearned Q must deal no simulated damage");
    Require(Near(WRawDamage(1, 100.0f, 1000.0f), 150.0f),
            "W must use the live 26.3 60 percent AP and three percent bonus mana");
    Require(Near(WRawDamage(5, 100.0f, 1000.0f), 270.0f),
            "rank-five W must use 180 base");
    Require(Near(WRawDamage(0, 999.0f, 9999.0f), 0.0f),
            "unlearned W must deal no simulated damage");
    Require(Near(ERawDamage(1, 100.0f, 1000.0f), 130.0f),
            "E must use 60 base, 50 percent AP and two percent bonus mana");
    Require(Near(ERawDamage(5, 100.0f, 1000.0f), 250.0f),
            "rank-five E must use 180 base");
    Require(Near(ERawDamage(0, 999.0f, 9999.0f), 0.0f),
            "unlearned E must deal no simulated damage");
    Require(Near(FluxQBonusPercent(0), 25.0f) &&
                Near(FluxQBonusPercent(1), 50.0f) &&
                Near(FluxQBonusPercent(2), 75.0f) &&
                Near(FluxQBonusPercent(3), 100.0f),
            "Flux Q amplifier must be 25/50/75/100 by R rank including pre-six");
    Require(Near(FluxedQRawDamage(1, 0, 100.0f, 1000.0f), 187.5f),
            "pre-six Fluxed Q must still gain 25 percent damage");
    Require(Near(FluxedQRawDamage(1, 3, 100.0f, 1000.0f), 300.0f),
            "rank-three Realm Warp must double Fluxed Q damage");
    Require(Near(TwoRuneMoveSpeedPercent(1), 28.0f) &&
                Near(TwoRuneMoveSpeedPercent(5), 44.0f),
            "two-rune Q speed must scale 28 through 44 percent");
    Require(Near(TwoRuneMoveSpeedPercent(0), 0.0f),
            "unlearned Q must not fabricate movement speed");
    Require(Near(kWSlowPercent, 50.0f) && Near(kWCrowdControlSeconds, 1.5f),
            "Rune Prison slow/root must retain live magnitude and duration");

    // Rune ledger: W/E reset Q and add/refresh, Q consumes all.
    RuneLedger runes{};
    runes = AddRune(runes, 10.0f);
    Require(runes.Stacks == 1 && Near(runes.ExpiresAt, 14.0f),
            "first W/E cast must add one four-second rune");
    runes = AddRune(runes, 11.0f);
    Require(runes.Stacks == 2 && Near(runes.ExpiresAt, 15.0f),
            "second W/E must cap at two and refresh duration");
    runes = AddRune(runes, 12.0f);
    Require(runes.Stacks == 2 && Near(runes.ExpiresAt, 16.0f),
            "additional reset at cap must refresh without a third rune");
    RuneSpend spend = SpendRunesWithQ(runes, 12.1f);
    Require(spend.Consumed == 2 && spend.GrantsMoveSpeed,
            "Q at two runes must consume both and grant speed");
    Require(spend.After.Stacks == 0 && Near(spend.After.ExpiresAt, 0.0f),
            "Q must clear the rune ledger");
    runes = AddRune({}, 20.0f);
    spend = SpendRunesWithQ(runes, 20.1f);
    Require(spend.Consumed == 1 && !spend.GrantsMoveSpeed,
            "one-rune Q must consume it without speed");
    Require(NormalizeRunes({ 2, 24.0f }, 24.0f).Stacks == 0,
            "runes expire exactly at the four-second boundary");
    Require(NormalizeRunes({ 9, 30.0f }, 25.0f).Stacks == 2,
            "rune count must clamp to the live maximum");
    Require(NormalizeRunes({ -2, 30.0f }, 25.0f).Stacks == 0,
            "negative rune telemetry must clamp to zero");

    // Branch definitions and exact live-mana multiplication.
    const ManaCosts costs{ 40.0f, 50.0f, 35.0f, 100.0f };
    Require(DefinitionFor(ComboBranch::MaximumDpsQEQWQEQ).Count == 7,
            "maximum DPS must publish seven casts");
    Require(Near(BranchMana(ComboBranch::MaximumDpsQEQWQEQ, costs), 280.0f),
            "QEQWQEQ must price four Q, two E and one W");
    Require(Near(BranchMana(ComboBranch::FastRootEWQ, costs), 125.0f),
            "EWQ must price exactly one of each basic spell");
    Require(Near(BranchMana(ComboBranch::ImmediateRootWQEQ, costs), 165.0f),
            "WQEQ must price two Q, W and E");
    Require(Near(BranchMana(ComboBranch::RootedBurstQEWQ, costs), 165.0f),
            "QEWQ must price two Q, E and W");
    Require(Near(BranchMana(ComboBranch::TripleQNoRootQWQEQ, costs), 205.0f),
            "QWQEQ must price three Q, W and E");
    Require(Near(BranchMana(ComboBranch::SlowSpeedWEQ, costs), 125.0f),
            "WEQ must price one of each basic spell");
    Require(Near(BranchMana(ComboBranch::FastTradeQEQ, costs), 115.0f),
            "QEQ must price two Q plus E");
    Require(Near(BranchMana(ComboBranch::FluxBurstEQ, costs), 75.0f),
            "EQ must price E plus Q");
    Require(Near(BranchMana(ComboBranch::ClearEEQ, costs), 110.0f),
            "EEQ must price two E plus Q");
    Require(Near(BranchMana(ComboBranch::None, costs), 0.0f),
            "empty branch must cost no mana");

    // Moving first-body Overload geometry.
    const Vec3 origin{ 0.0f, 0.0f, 0.0f };
    const Vec3 aim{ 1000.0f, 0.0f, 0.0f };
    QBody target = Body(10, 700.0f, 0.0f, 40.0f, true);
    QContact contact = ContactWithQBody(origin, aim, target);
    Require(contact.Hit && contact.BodyId == 10,
            "stationary champion in Q corridor must be contacted");
    Require(contact.ProjectileSeconds > 0.30f &&
                contact.CastElapsedSeconds > contact.ProjectileSeconds,
            "Q contact must separate 0.25 cast time from flight");
    Require(contact.MissileDistance < kQRange,
            "ordinary contact must occur before Q endpoint");
    target.Position.z = 100.0f;
    Require(!ContactWithQBody(origin, aim, target).Hit,
            "body outside missile plus bounding radius must miss");
    target.Position.z = 94.0f;
    Require(ContactWithQBody(origin, aim, target).Hit,
            "body touching Q combined radii must hit");
    QBody entering = Body(11, 620.0f, 250.0f, 30.0f);
    entering.Velocity = Vec3{ 0.0f, 0.0f, -420.0f };
    Require(ContactWithQBody(origin, aim, entering).Hit,
            "moving minion entering the lane at intercept must block Q");
    QBody leaving = Body(12, 620.0f, 0.0f, 30.0f);
    leaving.Velocity = Vec3{ 0.0f, 0.0f, 800.0f };
    Require(!ContactWithQBody(origin, aim, leaving).Hit,
            "minion leaving during cast delay must not be a static blocker");
    QBody edge = Body(13, 1040.0f, 0.0f, 45.0f, true);
    Require(ContactWithQBody(origin, aim, edge).Hit,
            "Q may touch a bounding edge at its 1000-unit endpoint");
    edge.Position.x = 1101.0f;
    edge.Radius = 40.0f;
    Require(!ContactWithQBody(origin, aim, edge).Hit,
            "Q must not invent Blitzcrank-style lollipop reach");
    QBody behind = Body(14, -100.0f, 0.0f, 35.0f);
    Require(!ContactWithQBody(origin, aim, behind).Hit,
            "Q cannot collide behind Ryze");
    QBody invalid = Body(15, 500.0f);
    invalid.Targetable = false;
    Require(!ContactWithQBody(origin, aim, invalid).Hit,
            "untargetable units cannot block Q");
    invalid = Body(16, 500.0f);
    invalid.Hostile = false;
    Require(!ContactWithQBody(origin, aim, invalid).Hit,
            "allied units cannot block Q");
    invalid = Body(0, 500.0f);
    Require(!ContactWithQBody(origin, aim, invalid).Hit,
            "zero-id bodies must be rejected");

    QBody minion = Body(20, 400.0f);
    target = Body(21, 800.0f, 0.0f, 40.0f, true);
    QContact first = FirstQContact(origin, aim, { target, minion });
    Require(first.Hit && first.BodyId == 20,
            "nearest minion must win Q first-body regardless of vector order");
    Require(!QHitsIntendedFirst(origin, aim, { target, minion }, 21),
            "blocked champion Q must never be authorized");
    Require(QHitsIntendedFirst(origin, aim, { target }, 21, &first) &&
                first.BodyIndex == 0,
            "clean champion must be returned as intended first body");
    QBody crossing = Body(22, 480.0f, 280.0f, 30.0f);
    crossing.Velocity = Vec3{ 0.0f, 0.0f, -520.0f };
    first = FirstQContact(origin, aim, { target, crossing });
    Require(first.Hit && first.BodyId == 22,
            "moving crossing blocker must beat later target");
    QBody tieHigh = Body(31, 500.0f);
    QBody tieLow = Body(30, 500.0f);
    first = FirstQContact(origin, aim, { tieHigh, tieLow });
    Require(first.BodyId == 30,
            "equal Q contacts must tie-break by network id");
    Require(!FirstQContact(origin, origin, { target }).Hit,
            "degenerate Q direction must miss safely");

    // Flux application, one-hop Q spread, large-unit radii and expiry.
    QBody primary = Body(100, 400.0f);
    QBody close = Body(101, 720.0f);
    QBody far = Body(102, 810.0f);
    QBody champion = Body(103, 700.0f, 120.0f, 40.0f, true);
    std::vector<QBody> cluster{ primary, close, far, champion };
    std::vector<int> marked = SpellFluxMarkedIds(100, cluster);
    Require(ContainsId(marked, 100) && ContainsId(marked, 101) &&
                ContainsId(marked, 103),
            "E must mark primary and nearby hostile units");
    Require(!ContainsId(marked, 102),
            "small-unit E spread must stop beyond 350 plus target radius");
    cluster[0].Large = true;
    marked = SpellFluxMarkedIds(100, cluster);
    Require(ContainsId(marked, 102),
            "large E primary must use the live 400-unit spread radius");
    for (auto& body : cluster) body.FluxExpiresAt = 10.0f;
    cluster[0].Large = false;
    std::vector<int> victims = FluxedQVictimIds(100, cluster, 6.0f);
    Require(ContainsId(victims, 100) && ContainsId(victims, 101) &&
                ContainsId(victims, 103),
            "Fluxed Q must hit all nearby marked units in one hop");
    Require(!ContainsId(victims, 102),
            "small Q detonation cannot jump outside 350 plus radius");
    cluster[0].Large = true;
    victims = FluxedQVictimIds(100, cluster, 6.0f);
    Require(ContainsId(victims, 102),
            "large Q primary must use the live 500-unit spread radius");
    Require(FluxedQVictimIds(100, cluster, 10.0f).empty(),
            "Flux expires exactly at four-second endpoint");
    cluster[0].FluxExpiresAt = 0.0f;
    Require(FluxedQVictimIds(100, cluster, 6.0f).empty(),
            "unmarked primary cannot trigger Flux Q splash");
    Require(SpellFluxMarkedIds(999, cluster).empty(),
            "missing E primary must return no marks");
    Require(FluxedQVictimIds(999, cluster, 6.0f).empty(),
            "missing Q primary must return no victims");

    // E-on-wave bridge into a priority champion and wave planners.
    QBody bridgeMinion = Body(200, 500.0f);
    bridgeMinion.Health = 300.0f;
    QBody bridgeDetonation = Body(201, 610.0f, 40.0f);
    bridgeDetonation.Health = 420.0f;
    QBody bridgeVictim = Body(202, 800.0f, 20.0f, 40.0f, true);
    QBody extra = Body(203, 650.0f, -80.0f);
    std::vector<QBody> bridgeBodies{
        bridgeMinion, bridgeDetonation, bridgeVictim, extra,
    };
    FluxBridgePlan bridge = BestFluxBridge(
        origin, 202, bridgeBodies, 120.0f, true);
    Require(bridge.Valid && bridge.PriorityVictimId == 202,
            "E bridge must find a wave route into the champion");
    Require(bridge.EPrimaryId != 202 && bridge.QDetonationId != 202,
            "indirect bridge must cast through non-champion bodies");
    Require(bridge.MarkedUnits >= 3 && bridge.QVictims >= 2,
            "bridge must publish mark and Q splash coverage");
    bridgeBodies[0].Position.x = 600.0f;
    bridgeBodies[1].Position.x = 650.0f;
    bridge = BestFluxBridge(origin, 202, bridgeBodies, 120.0f, true);
    Require(!bridge.Valid,
            "E primary beyond 550 plus bounding edge must not bridge");
    bridgeBodies = { bridgeMinion, bridgeDetonation, bridgeVictim, extra };
    bridgeBodies[0].Health = 80.0f;
    bridgeBodies[1].Health = 80.0f;
    bridgeBodies[3].Health = 80.0f;
    Require(!BestFluxBridge(origin, 202, bridgeBodies, 120.0f, true).Valid,
            "bridge must reject when E removes every possible Q detonator");
    Require(!BestFluxBridge(origin, 999, bridgeBodies, 120.0f, true).Valid,
            "bridge requires a real priority champion");

    std::vector<QBody> wave;
    for (int index = 0; index < 6; ++index) {
        QBody unit = Body(300 + index, 430.0f + index * 42.0f,
                          (index % 2 == 0 ? -45.0f : 45.0f));
        unit.Health = 400.0f;
        wave.push_back(unit);
    }
    WaveFluxPlan wavePlan = BestWaveFluxPlan(origin, wave, 110.0f, 4);
    Require(wavePlan.Valid && wavePlan.QVictims >= 4,
            "grouped wave must produce an E-Q detonation plan");
    Require(wavePlan.MarkedUnits >= wavePlan.QVictims,
            "Q victims must be a subset of E-marked units");
    Require(!BestWaveFluxPlan(origin, wave, 110.0f, 7).Valid,
            "wave planner must honor configured minimum hits");
    wave[0].Large = true;
    wave[0].Health = 150.0f;
    wavePlan = BestWaveFluxPlan(origin, wave, 110.0f, 3);
    Require(wavePlan.Valid && wavePlan.PreservesLargeMinion,
            "planner must not casually ruin a large-minion last hit");
    for (auto& unit : wave) unit.Position.x += 700.0f;
    Require(!BestWaveFluxPlan(origin, wave, 110.0f, 2).Valid,
            "wave outside E range must not be planned");

    // Q cast policy.
    QContext q = GoodQ();
    Require(EvaluateQ(q).Cast,
            "clean high-confidence direct Q must cast");
    QContext unavailableQ = q;
    unavailableQ.Ready = false;
    Require(!EvaluateQ(unavailableQ).Cast,
            "unready Q must hold");
    QContext noManaQ = q;
    noManaQ.HasMana = false;
    Require(!EvaluateQ(noManaQ).Cast,
            "Q must honor live mana");
    QContext blockedQ = q;
    blockedQ.IntendedFirstBody = false;
    Require(!EvaluateQ(blockedQ).Cast,
            "Q must reject an unintended first body");
    QContext wallQ = q;
    wallQ.ProjectileWallBlocked = true;
    Require(!EvaluateQ(wallQ).Cast,
            "projectile walls must deny Overload");
    QContext shieldQ = q;
    shieldQ.TargetSpellShield = true;
    Require(!EvaluateQ(shieldQ).Cast,
            "Q must not be fed into a spell shield");
    QContext immuneQ = q;
    immuneQ.TargetImmune = true;
    Require(!EvaluateQ(immuneQ).Cast,
            "Q must not cast into immunity");
    QContext windupQ = q;
    windupQ.PlayerAttackWindingUp = true;
    Require(!EvaluateQ(windupQ).Cast,
            "ordinary Q must preserve an attack windup");
    windupQ.Lethal = true;
    windupQ.Purpose = QPurpose::Kill;
    Require(EvaluateQ(windupQ).Cast,
            "lethal Q may override windup preservation");
    QContext lowPredictionQ = q;
    lowPredictionQ.HighConfidence = false;
    Require(!EvaluateQ(lowPredictionQ).Cast,
            "ordinary moving target requires branch prediction threshold");
    lowPredictionQ.TargetImmobile = true;
    Require(EvaluateQ(lowPredictionQ).Cast,
            "immobile target is a guaranteed Q exception");
    QContext cursorQ = q;
    cursorQ.CursorAgrees = false;
    Require(!EvaluateQ(cursorQ).Cast,
            "ordinary Q must agree with player cursor");
    cursorQ.Purpose = QPurpose::Peel;
    cursorQ.Reactive = true;
    Require(EvaluateQ(cursorQ).Cast,
            "urgent peel Q may override cursor alignment");
    QContext preserveQ = q;
    preserveQ.Purpose = QPurpose::Harass;
    preserveQ.RuneStacks = 2;
    preserveQ.PreserveTwoRuneSpeed = true;
    Require(!EvaluateQ(preserveQ).Cast,
            "harass must be able to preserve charged speed Q");
    preserveQ.SpeedNeeded = true;
    Require(EvaluateQ(preserveQ).Cast,
            "speed need authorizes consuming two-rune Q");
    QContext splashQ = q;
    splashQ.TargetFluxed = true;
    splashQ.PriorityVictimHitByFluxSpread = true;
    splashQ.FluxVictims = 4;
    splashQ.Purpose = QPurpose::FluxBurst;
    Require(EvaluateQ(splashQ).Score > EvaluateQ(q).Score + 400.0f,
            "multi-body Flux Q must materially outrank raw Q");

    // Rune Prison decisions distinguish slow from root.
    WContext w = GoodW();
    Require(EvaluateW(w).Cast,
            "committed target permits W reset/slow");
    WContext rootW = w;
    rootW.RootRequired = true;
    Require(!EvaluateW(rootW).Cast,
            "root-required W must not pretend an unfluxed slow is a root");
    rootW.TargetFluxed = true;
    rootW.TargetMobilityReady = true;
    rootW.Purpose = WPurpose::ImmediateRoot;
    Require(EvaluateW(rootW).Cast,
            "W must immediately root a mobile Flux target");
    WContext shieldW = rootW;
    shieldW.TargetSpellShield = true;
    Require(!EvaluateW(shieldW).Cast,
            "W root must not be spent into spell shield");
    WContext immuneW = rootW;
    immuneW.TargetImmune = true;
    Require(!EvaluateW(immuneW).Cast,
            "W must respect immunity");
    WContext rangeW = rootW;
    rangeW.InRange = false;
    Require(!EvaluateW(rangeW).Cast,
            "W must use real point-target range");
    WContext windupW = w;
    windupW.PlayerAttackWindingUp = true;
    Require(!EvaluateW(windupW).Cast,
            "ordinary W must preserve auto windup");
    windupW.Reactive = true;
    windupW.Purpose = WPurpose::Peel;
    Require(EvaluateW(windupW).Cast,
            "urgent peel W may preempt an attack");
    WContext overlapW = rootW;
    overlapW.TargetAlreadyHardCrowdControlled = true;
    overlapW.Purpose = WPurpose::RootSetup;
    overlapW.Lethal = false;
    Require(!EvaluateW(overlapW).Cast,
            "W must avoid wasteful hard-CC overlap");
    overlapW.Lethal = true;
    Require(EvaluateW(overlapW).Cast,
            "lethal W can overlap existing crowd control");

    // Spell Flux decisions distinguish root setup, bridge and damage resets.
    EContext e = GoodE();
    Require(EvaluateE(e).Cast,
            "Q-ready direct E reset must cast");
    EContext rootE = e;
    rootE.Purpose = EPurpose::RootSetup;
    rootE.WReady = false;
    Require(!EvaluateE(rootE).Cast,
            "E root setup requires W available");
    rootE.WReady = true;
    rootE.TargetMobilityReady = true;
    Require(EvaluateE(rootE).Cast,
            "E-W setup must cast against mobile in-range target");
    EContext bridgeE = e;
    bridgeE.Purpose = EPurpose::FluxBridge;
    bridgeE.PriorityVictimWillBeMarked = true;
    bridgeE.MarkedUnits = 4;
    Require(EvaluateE(bridgeE).Cast,
            "verified multi-unit E bridge must cast");
    bridgeE.TargetWillSurviveE = false;
    Require(!EvaluateE(bridgeE).Cast,
            "bridge must not remove its only Q detonation body");
    bridgeE.TargetWillSurviveE = true;
    bridgeE.PriorityVictimWillBeMarked = false;
    Require(!EvaluateE(bridgeE).Cast,
            "bridge without priority victim mark must hold");
    EContext shieldE = e;
    shieldE.TargetSpellShield = true;
    Require(!EvaluateE(shieldE).Cast,
            "E must respect spell shield");
    EContext windupE = e;
    windupE.PlayerAttackWindingUp = true;
    Require(!EvaluateE(windupE).Cast,
            "ordinary E must preserve attack windup");
    windupE.Reactive = true;
    windupE.Purpose = EPurpose::PeelSetup;
    Require(EvaluateE(windupE).Cast,
            "urgent peel E may preempt attack windup");

    // One-trick combo branch selection and reserves.
    ComboContext combo = Combo();
    combo.PeelUrgent = true;
    combo.TargetFluxed = true;
    ComboDecision decision = SelectComboBranch(combo);
    Require(decision.Branch == ComboBranch::ImmediateRootWQEQ &&
                decision.RootBranch,
            "existing Flux peel must start with immediate W root");
    combo = Combo();
    combo.PeelUrgent = true;
    decision = SelectComboBranch(combo);
    Require(decision.Branch == ComboBranch::KiteEWQ,
            "unfluxed peel must build E-W-Q root and speed");
    combo = Combo();
    combo.FullDpsWindow = true;
    combo.TargetCommitted = true;
    decision = SelectComboBranch(combo);
    Require(decision.Branch == ComboBranch::MaximumDpsQEQWQEQ,
            "safe committed target must receive four-Q maximum DPS");
    combo = Combo();
    combo.TargetMobilityReady = true;
    decision = SelectComboBranch(combo);
    Require(decision.Branch == ComboBranch::RootedBurstQEWQ,
            "mobile target must receive travel-Q into E-W root branch");
    combo = Combo();
    combo.Lethal = true;
    combo.TargetMobilityReady = false;
    decision = SelectComboBranch(combo);
    Require(decision.Branch == ComboBranch::TripleQNoRootQWQEQ,
            "verified lethal can trade root for three Q casts");
    combo = Combo();
    combo.SpeedNeeded = true;
    combo.TargetDistance = 800.0f;
    decision = SelectComboBranch(combo);
    Require(decision.Branch == ComboBranch::SlowSpeedWEQ,
            "speed branch outside EW peel condition must use W-E-Q");
    combo = Combo();
    combo.Harass = true;
    decision = SelectComboBranch(combo);
    Require(decision.Branch == ComboBranch::FastTradeQEQ &&
                decision.PreservesW,
            "harass must use Q-E-Q and preserve W");
    combo = Combo();
    combo.QReady = false;
    Require(SelectComboBranch(combo).Branch == ComboBranch::FluxBurstEQ,
            "E must reset a cooling Q and preserve the real E-Q window");
    combo = Combo();
    combo.QReady = false;
    combo.PeelUrgent = true;
    Require(SelectComboBranch(combo).Branch == ComboBranch::KiteEWQ,
            "E-W must reset a cooling Q before the two-rune speed cast");
    combo = Combo();
    combo.QReady = false;
    combo.TargetFluxed = true;
    combo.TargetMobilityReady = true;
    Require(SelectComboBranch(combo).Branch == ComboBranch::ImmediateRootWQEQ,
            "Flux W must reset a cooling Q before the W-Q-E-Q punish");
    combo = Combo();
    combo.CurrentMana = 100.0f;
    combo.ReservedMana = 50.0f;
    combo.Harass = true;
    Require(SelectComboBranch(combo).Branch == ComboBranch::None,
            "harass must preserve configured peel reserve");
    combo = Combo();
    combo.Clear = true;
    combo.CleanQ = false;
    decision = SelectComboBranch(combo);
    Require(decision.Branch == ComboBranch::FluxBurstEQ ||
                decision.Branch == ComboBranch::ClearEEQ,
            "clear context must use a real E-Q family rather than W");
    combo = Combo();
    combo.TargetValid = false;
    Require(SelectComboBranch(combo).Branch == ComboBranch::None,
            "missing target must yield no combo branch");

    // Auto weave gates preserve instant reset rhythm and player ownership.
    AutoWeaveContext weave{};
    weave.TargetValid = weave.InAttackRange = weave.AttackReady = weave.Safe = true;
    weave.MillisecondsUntilNextReset = 500;
    Require(ShouldWeaveAuto(weave),
            "safe rooted/stable downtime should permit an auto weave");
    AutoWeaveContext escapeWeave = weave;
    escapeWeave.TargetCanInstantEscape = true;
    Require(!ShouldWeaveAuto(escapeWeave),
            "do not weave while an unrooted target can instantly escape");
    escapeWeave.TargetRooted = true;
    Require(ShouldWeaveAuto(escapeWeave),
            "root duration creates a safe auto weave window");
    AutoWeaveContext bufferWeave = weave;
    bufferWeave.BufferWindowActive = true;
    Require(!ShouldWeaveAuto(bufferWeave),
            "never insert auto into W/E-to-buffered-Q window");
    AutoWeaveContext lethalWeave = weave;
    lethalWeave.NextSpellLethal = true;
    Require(!ShouldWeaveAuto(lethalWeave),
            "do not delay a lethal spell for an auto");
    AutoWeaveContext peelWeave = weave;
    peelWeave.NextSpellCriticalPeel = true;
    Require(!ShouldWeaveAuto(peelWeave),
            "do not delay critical peel for an auto");
    AutoWeaveContext cooldownWeave = weave;
    cooldownWeave.MillisecondsUntilNextReset = 100;
    Require(!ShouldWeaveAuto(cooldownWeave),
            "near-ready reset should keep spell cadence");
    cooldownWeave.PlayerIssuedAttack = true;
    Require(ShouldWeaveAuto(cooldownWeave),
            "explicit player attack keeps ownership despite short reset");
    AutoWeaveContext unsafeWeave = weave;
    unsafeWeave.Safe = false;
    Require(!ShouldWeaveAuto(unsafeWeave),
            "unsafe position must not invite an auto weave");

    // Realm Warp: player authorization, 1000-3000 clamp and no-grief arrival.
    Vec3 warp = ClampWarpDestination(origin, Vec3{ 1800.0f, 0.0f, 0.0f });
    Require(Near(warp.x, 1800.0f),
            "in-range cursor destination must remain exact");
    warp = ClampWarpDestination(origin, Vec3{ 4000.0f, 0.0f, 0.0f });
    Require(Near(warp.x, 3000.0f),
            "far cursor must clamp to 3000 Realm Warp range");
    Require(ClampWarpDestination(origin, Vec3{ 900.0f, 0.0f, 0.0f }).IsZero(),
            "cursor inside 1000 minimum must not fabricate a warp");
    Require(ClampWarpDestination(origin, origin).IsZero(),
            "degenerate cursor vector must not warp");
    Require(PortalContains(origin, Vec3{ 360.0f, 0.0f, 0.0f }),
            "unit center inside 365 portal must teleport");
    Require(PortalContains(origin, Vec3{ 400.0f, 0.0f, 0.0f }, 40.0f),
            "portal occupancy must include unit bounding radius");
    Require(!PortalContains(origin, Vec3{ 450.0f, 0.0f, 0.0f }, 40.0f),
            "unit outside portal plus radius must remain behind");
    WarpContext warpContext = GoodWarp();
    Require(EvaluateWarp(warpContext).Cast,
            "player-authorized visible safe warp must cast");
    WarpContext noManual = warpContext;
    noManual.ManualAuthorized = false;
    Require(!EvaluateWarp(noManual).Cast,
            "ordinary Realm Warp must remain player-authorized");
    noManual.Purpose = WarpPurpose::EmergencyEscape;
    noManual.AutomaticEmergencyOptIn = true;
    noManual.PlayerInLethalDanger = true;
    Require(EvaluateWarp(noManual).Cast,
            "explicit opt-in lethal emergency may self-warp");
    WarpContext tooNear = warpContext;
    tooNear.Distance = 999.0f;
    Require(!EvaluateWarp(tooNear).Cast,
            "warp policy must enforce minimum range");
    WarpContext tooFar = warpContext;
    tooFar.Distance = 3001.0f;
    Require(!EvaluateWarp(tooFar).Cast,
            "warp policy must enforce maximum range");
    WarpContext terrainWarp = warpContext;
    terrainWarp.DestinationNavigable = false;
    Require(!EvaluateWarp(terrainWarp).Cast,
            "Realm Warp must reject wall endpoint");
    WarpContext rootedWarp = warpContext;
    rootedWarp.PlayerRootedOrGrounded = true;
    Require(!EvaluateWarp(rootedWarp).Cast,
            "rooted or grounded Ryze cannot start Realm Warp");
    WarpContext interruptWarp = warpContext;
    interruptWarp.IncomingInterruptLikely = true;
    Require(!EvaluateWarp(interruptWarp).Cast,
            "likely interrupt must preserve R unless escape is lethal");
    interruptWarp.PlayerInLethalDanger = true;
    Require(EvaluateWarp(interruptWarp).Cast,
            "lethal manual escape may accept interrupt risk");
    WarpContext turretWarp = warpContext;
    turretWarp.DestinationUnderEnemyTurret = true;
    Require(!EvaluateWarp(turretWarp).Cast,
            "safe default must reject enemy-turret arrival");
    turretWarp.AllowUnsafeManual = true;
    Require(EvaluateWarp(turretWarp).Cast,
            "explicit unsafe-manual override may accept turret arrival");
    WarpContext numbersWarp = warpContext;
    numbersWarp.AlliesAtDestination = 0;
    numbersWarp.EnemiesAtDestination = 2;
    Require(!EvaluateWarp(numbersWarp).Cast,
            "warp must reject losing arrival numbers");
    WarpContext blindWarp = warpContext;
    blindWarp.DestinationHasVision = false;
    blindWarp.EnemiesAtDestination = 1;
    Require(!EvaluateWarp(blindWarp).Cast,
            "warp must reject blind arrival with known enemy");
    WarpContext channelWarp = warpContext;
    channelWarp.AlliesInPortal = 2;
    channelWarp.AllyChannelWouldBeBroken = true;
    Require(!EvaluateWarp(channelWarp).Cast,
            "Realm Warp must not abduct an allied protected channel");
    WarpContext cursorWarp = warpContext;
    cursorWarp.CursorAgrees = false;
    Require(!EvaluateWarp(cursorWarp).Cast,
            "manual warp must still agree with current cursor");
    WarpContext noManaWarp = warpContext;
    noManaWarp.HasMana = false;
    Require(!EvaluateWarp(noManaWarp).Cast,
            "Realm Warp must use live 100-mana availability");

    std::cout << "ALL AIRYZE GEOMETRY TESTS PASSED ("
              << ScenarioCount << " scenarios)\n";
    return 0;
}
