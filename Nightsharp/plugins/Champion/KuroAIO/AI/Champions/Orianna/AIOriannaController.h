#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIOriannaGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Orianna {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* BallMenu = nullptr;
inline Menu* ProtectMenu = nullptr;
inline Menu* ShockwaveMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline BallState Ball = {};
inline int LastCastTick[4]{};
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int PassiveTargetId = 0;
inline int PassiveStacks = 0;
inline int PassiveExpireTick = 0;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Ready;

inline bool Throttle(int slot, int delay = 70) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

using ControllerHelpers::Protected;

using ControllerHelpers::AP;

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target,
            QRawDamage(SpellRank(0), AP())) : 0.0f;
}
inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target,
            WRawDamage(SpellRank(1), AP())) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target,
            ERawDamage(SpellRank(2), AP())) : 0.0f;
}
inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target,
            RRawDamage(SpellRank(3), AP())) : 0.0f;
}
using ControllerHelpers::Lethal;

inline Vector3 AttachedPosition(int networkId) {
    const auto player = GameObjects::Player();
    if (player.IsValid() && networkId == static_cast<int>(player.NetworkId()))
        return player.Position();
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (Engine::ValidAlly(ally) &&
            networkId == static_cast<int>(ally.NetworkId())) return ally.Position();
    }
    return {};
}

inline Vector3 BallPosition() {
    if (Ball.AttachedNetworkId != 0) {
        const Vector3 attached = AttachedPosition(Ball.AttachedNetworkId);
        if (attached.IsValid() && !attached.IsZero()) return attached;
    }
    return Ball.Position;
}

inline void AttachBallTo(int networkId, const Vector3& position) {
    Ball.Position = position;
    Ball.AttachedNetworkId = networkId;
    Ball.PendingPosition = {};
    Ball.PendingAttachedNetworkId = 0;
    Ball.ArrivalTick = 0;
    Ball.InTransit = false;
}

inline void ReconcileBall() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    ReconcileBallTransit(Ball, Now());
    if (player.HasBuff("orianaghostself") || player.HasBuff("OriannaGhostSelf")) {
        AttachBallTo(static_cast<int>(player.NetworkId()), player.Position());
        return;
    }
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally)) continue;
        if (ally.HasBuff("orianaghost") || ally.HasBuff("OriannaGhost")) {
            AttachBallTo(static_cast<int>(ally.NetworkId()), ally.Position());
            return;
        }
    }
    const Vector3 ball = BallPosition();
    if (!Ball.InTransit && (!ball.IsValid() || ball.IsZero() ||
        player.Position().Distance2D(ball) > kBallLeashRange + 100.0f)) {
        AttachBallTo(static_cast<int>(player.NetworkId()), player.Position());
    }
}

inline AIHeroClient AutonomousTarget(float range) {
    const auto orbwalker = ControllerHelpers::OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orbwalker, range)) return orbwalker;
    return Engine::SelectTarget(range);
}

inline int PredictedRHits(const Vector3& ball) {
    std::vector<Vector3> predicted;
    predicted.reserve(8);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        const Vector3 point = PredictPosition(enemy, kRDelaySeconds);
        if (point.IsValid() && !point.IsZero()) predicted.push_back(point);
    }
    return ShockwaveHitCount(ball, predicted, 65.0f);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool defensive = false) {
    const auto player = GameObjects::Player();
    const Vector3 ball = BallPosition();
    if (!player.IsValid() || !ball.IsValid() || Ball.InTransit ||
        !Ready(0, mode) || !Throttle(0) || Protected(target)) return false;
    if (Orbwalker::IsWindingUp() && !defensive && !Lethal(target, QDamage(target)))
        return false;
    SDK::PredictionOutput prediction{};
    Vector3 desired = PredictPosition(target,
        TravelSeconds(ball, target.Position(), kQSpeed));
    if (Engine::RuntimeSpells[0]) {
        prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        if (prediction.Hitchance < (defensive ? SDK::HitChance::Medium
                                             : SDK::HitChance::High)) return false;
        const Vector3 cast = prediction.GetCastPosition();
        if (cast.IsValid() && !cast.IsZero()) desired = cast;
    }
    if (!desired.IsValid() || desired.IsZero()) return false;
    const Vector3 destination = ClampQDestination(player.Position(), desired);
    if (!destination.IsValid() || destination.IsZero() ||
        SDK::NavMesh::IsWall(destination) ||
        ControllerHelpers::ProjectileWallBlocks(ball, destination, kQWidth))
        return false;
    if (!Engine::ControllerCastPosition(0, destination)) return false;
    LastCastTick[0] = Now();
    const int travel = static_cast<int>(std::ceil(
        TravelSeconds(ball, destination, kQSpeed) * 1000.0f));
    BeginBallTransit(Ball, destination, 0, LastCastTick[0], travel);
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool defensive = false) {
    const Vector3 ball = BallPosition();
    if (!ball.IsValid() || Ball.InTransit || !Ready(1, mode) || !Throttle(1) ||
        Protected(target)) return false;
    const Vector3 predicted = PredictPosition(target, 0.05f);
    if (!CircleContains(ball, predicted, kWRadius, target.BoundingRadius()))
        return false;
    if (Orbwalker::IsWindingUp() && !defensive && !Lethal(target, WDamage(target)))
        return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& ally,
                  Mode mode,
                  bool incomingThreat,
                  bool deliverCombo = false) {
    const auto player = GameObjects::Player();
    const Vector3 ball = BallPosition();
    if (!player.IsValid() || !Engine::ValidAlly(ally) || !ball.IsValid() ||
        Ball.InTransit || !Ready(2, mode) || !Throttle(2) ||
        player.Position().Distance2D(ally.Position()) > kERange + ally.BoundingRadius())
        return false;
    bool pathHit = false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && BallPathHits(ball, ally.Position(),
            PredictPosition(enemy, TravelSeconds(ball, ally.Position(), kESpeed)),
            kEWidth, enemy.BoundingRadius())) {
            pathHit = true;
            break;
        }
    }
    ProtectContext context{};
    context.Ready = true;
    context.AllyValid = true;
    context.AllyInRange = true;
    context.BallPathKnown = true;
    context.IncomingThreat = incomingThreat;
    context.AllyLow = ally.HealthPercent() <= Slider(ProtectMenu, "AllyHP", 38);
    context.DeliversCombo = deliverCombo;
    context.PathHitsEnemy = pathHit;
    context.WouldAbandonBetterBall = !deliverCombo && !incomingThreat &&
        Engine::CountEnemiesAt(ball, kWRadius) >= 2;
    if (!ShouldCastProtect(context) ||
        !Engine::ControllerCastUnit(2, AIBaseClient(ally.Address()))) return false;
    LastCastTick[2] = Now();
    const int travel = static_cast<int>(std::ceil(
        TravelSeconds(ball, ally.Position(), kESpeed) * 1000.0f));
    BeginBallTransit(Ball, ally.Position(), static_cast<int>(ally.NetworkId()),
        LastCastTick[2], travel);
    return true;
}

inline bool CastSelfE(Mode mode, bool incomingThreat) {
    const auto player = GameObjects::Player();
    return player.IsValid() && CastE(AIHeroClient(player.Address()), mode,
                                     incomingThreat, false);
}

inline bool CastR(const AIHeroClient& target,
                  Mode mode,
                  bool defensive = false) {
    const Vector3 ball = BallPosition();
    if (!ball.IsValid() || Protected(target)) return false;
    const Vector3 predicted = PredictPosition(target, kRDelaySeconds);
    ShockwaveContext context{};
    context.Ready = Ready(3, mode) && Throttle(3, 100);
    context.BallKnown = ball.IsValid() && !ball.IsZero();
    context.BallInTransit = Ball.InTransit;
    context.IntendedTargetInside = CircleContains(ball, predicted, kRRadius,
                                                   target.BoundingRadius());
    context.TargetProtected = Protected(target);
    context.AttackWindingUp = Orbwalker::IsWindingUp();
    context.Lethal = Lethal(target, RDamage(target));
    context.Defensive = defensive;
    context.PredictedHits = PredictedRHits(ball);
    context.MinimumHits = Slider(ShockwaveMenu, "MinimumTargets", 2);
    if (!ShouldCastShockwave(context) || !Engine::ControllerCastSelf(3))
        return false;
    LastCastTick[3] = Now();
    return true;
}

inline AIHeroClient BestProtectAlly(const AIHeroClient& fallback) {
    AIHeroClient best{};
    float score = -FLT_MAX;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally)) continue;
        const float candidate = (100.0f - ally.HealthPercent()) * 8.0f +
            static_cast<float>(Engine::CountEnemiesAt(ally.Position(), 650.0f)) * 250.0f;
        if (candidate > score) { score = candidate; best = ally; }
    }
    return best.IsValid() ? best : fallback;
}


inline bool TryReactive(const AIHeroClient& target, Mode mode) {
    const bool threatened = IncomingThreatUntil > Now();
    const bool hardCC = IncomingHardCCUntil > Now();
    if (threatened && CastSelfE(mode, true)) return true;
    const auto gapcloser = HeroByNetworkId(GapcloserTargetId);
    if (GapcloserExpireTick > Now() && Engine::ValidEnemy(gapcloser)) {
        if (CastR(gapcloser, mode, true)) return true;
        if (CastW(gapcloser, mode, true)) return true;
    }
    const auto interrupt = HeroByNetworkId(InterruptTargetId);
    if (InterruptExpireTick > Now() && Engine::ValidEnemy(interrupt) &&
        CastR(interrupt, mode, true)) return true;
    return hardCC && Engine::ValidEnemy(target) && CastR(target, mode, true);
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, WDamage(target)) && CastW(target, mode)) return true;
    if (Lethal(target, QDamage(target)) && CastQ(target, mode)) return true;
    return Lethal(target, RDamage(target)) && CastR(target, mode);
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    const Vector3 ball = BallPosition();
    if (CircleContains(ball, PredictPosition(target, kRDelaySeconds),
                       kRRadius, target.BoundingRadius()) &&
        CastR(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (IncomingThreatUntil > Now()) (void)CastSelfE(Mode::Combo, true);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(BallMenu, "HarassMana", 48))
        return;
    if (CastW(target, Mode::Harass)) return;
    (void)CastQ(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& threat) {
    if (CastSelfE(Mode::Flee, true)) return;
    if (CastW(threat, Mode::Flee, true)) return;
    (void)CastR(threat, Mode::Flee, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    LastMode = mode;
    ReconcileBall();
    const AIHeroClient target = AutonomousTarget(1500.0f);
    if (TryReactive(target, mode) || TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 950.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (GameObjects::Player().ManaPercent() >= Slider(FarmMenu, "Mana", 35))
            (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic: {
        const bool kill = Engine::ValidEnemy(target) &&
            (Lethal(target, QDamage(target)) || Lethal(target, WDamage(target)) ||
             Lethal(target, RDamage(target)));
        if (AutomaticAllowed({IncomingThreatUntil > Now(),
                              IncomingHardCCUntil > Now(),
                              InterruptExpireTick > Now(), kill, false})) {
            (void)TryReactive(target, mode);
            (void)TryKillSecure(target, mode);
        }
        break;
    }
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot < 0 || slot > 3) return;
        LastCastTick[slot] = now;
        const auto player = GameObjects::Player();
        if (slot == 0 && player.IsValid()) {
            const Vector3 destination = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
                ? args.EndPosition : args.CastPosition;
            const Vector3 ball = BallPosition();
            BeginBallTransit(Ball, destination, 0, now,
                static_cast<int>(std::ceil(TravelSeconds(ball, destination, kQSpeed) * 1000.0f)));
        } else if (slot == 2 && player.IsValid()) {
            const int targetId = static_cast<int>(args.TargetNetworkId != 0
                ? args.TargetNetworkId : args.Target.NetworkId);
            const Vector3 destination = AttachedPosition(targetId);
            if (destination.IsValid() && !destination.IsZero()) {
                BeginBallTransit(Ball, destination, targetId, now,
                    static_cast<int>(std::ceil(TravelSeconds(BallPosition(), destination, kESpeed) * 1000.0f)));
            }
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) {
        IncomingHardCCUntil = std::max(IncomingHardCCUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    }
}

inline void UpdateAttachment(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!Engine::TextContains(args.BuffName, "orianaghost") &&
        !Engine::TextContains(args.BuffName, "OriannaGhost")) return;
    if (!added) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    const Vector3 position = AttachedPosition(id);
    if (position.IsValid() && !position.IsZero()) AttachBallTo(id, position);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    const int id = static_cast<int>(args.Target.NetworkId());
    if (id != PassiveTargetId || PassiveExpireTick < Now()) {
        PassiveTargetId = id;
        PassiveStacks = 0;
    }
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick)) return;
    const int id = static_cast<int>(args.Target.NetworkId());
    if (id == PassiveTargetId && PassiveExpireTick >= Now())
        PassiveStacks = std::min(2, PassiveStacks + 1);
    else {
        PassiveTargetId = id;
        PassiveStacks = 0;
    }
    PassiveExpireTick = Now() + 4000;
}
inline void OnDraw() {
    const auto player = GameObjects::Player();
    const Vector3 ball = BallPosition();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawBall", false)) return;
    Drawing::DrawCircle(ball, kWRadius, 0xFF77CCFFu, 1.5f, 36);
    Drawing::DrawCircle(ball, kRRadius, 0xFFC2A5FFu, 1.5f, 44);
    Drawing::DrawLine(player.Position(), ball, 0xFF99DDEEu, 1.2f);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("OriannaOneTrick", "Orianna Ball control"));
    BallMenu = TacticsMenu->AddSubMenu(new Menu("Ball", "Q/W Ball routing"));
    BallMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 48, 10, 90));
    ProtectMenu = TacticsMenu->AddSubMenu(new Menu("Protect", "E ally protection"));
    ProtectMenu->Add(new MenuSlider("AllyHP", "Protect ally below HP percent", 38, 10, 80));
    ShockwaveMenu = TacticsMenu->AddSubMenu(new Menu("Shockwave", "R Ball timing"));
    ShockwaveMenu->Add(new MenuSlider("MinimumTargets", "Minimum nonlethal targets", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("OriannaFarm", "Farm resource policy"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("OriannaCoach", "Ball visualization"));
    CoachMenu->Add(new MenuBool("DrawBall", "Draw Ball W/R zones", false));
}

inline void OnLoad() {
    const auto player = GameObjects::Player();
    Ball = {};
    if (player.IsValid()) AttachBallTo(static_cast<int>(player.NetworkId()), player.Position());
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    IncomingThreatUntil = IncomingHardCCUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = InterruptTargetId = InterruptExpireTick = 0;
    GapcloserEndpoint = {};
    LastAutoTargetId = LastAutoTick = PassiveTargetId = PassiveStacks = PassiveExpireTick = 0;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = BallMenu = ProtectMenu = ShockwaveMenu = FarmMenu = CoachMenu = nullptr;
    Ball = {};
}

inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Track Ball attachment with buff events and polling reconciliation",
    "Track Q and E Ball transit until calculated arrival",
    "Return Ball conservatively when its observed position exceeds leash range",
    "Never cast W or R from an unknown or in-transit Ball position",
    "Clamp Command Attack input to the live 815 player cast range",
    "Route Q damage and collision from the Ball rather than Orianna",
    "Apply 30 percent Q damage reduction after the first contact",
    "Use live Q base damage and 55 percent AP scaling",
    "Reject Q through projectile walls or invalid terrain",
    "Use W only when the target overlaps the 225 Ball radius",
    "Use live W damage and rank-based slow and haste",
    "Use E on a real allied target within 1095 range",
    "Track E damage along the Ball-to-ally segment",
    "Use live E shield damage and resistance values",
    "Do not abandon a multi-enemy Ball for nonurgent shielding",
    "Permit E delivery for an observed ally engage",
    "Predict Shockwave contact at the 0.75 second delay",
    "Use the live 415 Shockwave radius and current damage",
    "Require the intended target inside Shockwave",
    "Reserve nonlethal Shockwave for configured multi-target value",
    "Allow lethal defensive interrupt Shockwave",
    "Reject Shockwave during AA windup unless commitment is justified",
    "Use autonomous orbwalker and engine target policy",
    "Preserve Clockwork Windup target and repeated-attack stacks",
    "Combo uses current Ball contact before repositioning",
    "Harass preserves mana and never spends unsolicited R",
    "LaneClear Jungle and LastHit use shared farm logic",
    "Flee shields Orianna then uses Ball-centered peel",
    "Automatic mode permits only defense interrupt or kill secure",
    "Reconcile observed Q W E or R events",
    "Never issue Flash, summoner spells, items or movement",
    "Keep profile metadata separate from the decision loop",
    "Reject protected invulnerable and spell-shielded targets",
    "Expose Ball W and R zones without changing gameplay state",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Orianna;
    controller.ControllerId = "champion.kuroaio.ai.orianna.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIOrianna.md";
    controller.ImplementationSummary =
        "Event-reconciled Ball attachment/transit, Ball-origin Q/E geometry, ally shielding, AA cooperation and predictive Shockwave policy.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    controller.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&UpdateAttachment, true>;
    controller.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&UpdateAttachment, false>;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 900, 1000>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1400, 250, 5000>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Orianna
