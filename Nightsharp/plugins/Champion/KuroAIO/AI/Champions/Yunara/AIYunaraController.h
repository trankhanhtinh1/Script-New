#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AIYunaraGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>

namespace Plugins::KuroAIO::AI::Controllers::Yunara {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::RuntimeNameContains;

inline Menu* TacticsMenu = nullptr;
inline Menu* TranscendMenu = nullptr;
inline Menu* MobilityMenu = nullptr;

inline bool Transcendent = false;
inline bool QActive = false;
inline int TranscendentUntil = 0;
inline int QActiveUntil = 0;
inline int ObservedQResource = 0;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int PendingRuinTargetId = 0;
inline int PendingRuinUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserUntil = 0;
inline Vector3 GapcloserEndpoint = {};
inline Mode LastMode = Mode::None;

inline bool RuntimeW2() {
    return RuntimeNameContains(1, "YunaraW2");
}

inline bool RuntimeE2() {
    return RuntimeNameContains(2, "YunaraE2");
}

inline bool ProjectileAttackBlocked(const AIBaseClient& target) {
    return target.IsValid() &&
        ControllerHelpers::ProjectileWallBlocksFromPlayer(
            target.Position(), 0.0f);
}

inline bool TargetUsable(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
           !ControllerHelpers::HasSpellShieldOrImmunity(target) &&
           !ControllerHelpers::IsCommonUntargetableOrImmune(target);
}

inline float RuinDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const float raw = ArcOfRuinRaw(
        ControllerHelpers::SpellRank(3), player.BonusAttackDamage(),
        player.AP());
    return player.CalculatePhysicalDamage(target, raw);
}

inline bool AttackRoute(const AIBaseClient& target) {
    return Orbwalker::AttackEnabled() && target.IsValid() &&
           target.IsEnemy() && target.IsTargetable() &&
           InAutoAttackRange(target) && !ProjectileAttackBlocked(target);
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) {
        Transcendent = QActive = false;
        return;
    }

    const bool liveTranscend = RuntimeW2() || RuntimeE2() ||
        player.HasBuff("YunaraR");
    if (liveTranscend) {
        Transcendent = true;
        if (TranscendentUntil <= now) {
            TranscendentUntil = now + kRDurationMs;
        }
    } else if (Transcendent && now > TranscendentUntil) {
        Transcendent = false;
        TranscendentUntil = 0;
    }

    const bool liveQ = player.HasBuff("YunaraQ");
    if (liveQ || Transcendent) {
        QActive = true;
        if (QActiveUntil <= now) {
            QActiveUntil = Transcendent
                ? TranscendentUntil : now + kQDurationMs;
        }
    } else if (QActive && now > QActiveUntil) {
        QActive = false;
        QActiveUntil = 0;
    }
    if (!Transcendent && PendingRuinUntil < now) {
        PendingRuinTargetId = PendingRuinUntil = 0;
    }
}

inline bool CastQForAttack(const AIBaseClient& target, Mode mode) {
    const bool ready = CanUse(0, mode, true);
    if (!MayActivateQ(ready, QActive || Transcendent,
                      AttackRoute(target),
                      ProjectileAttackBlocked(target)) ||
        !CastThrottlePassed(LastQCastTick, 70)) {
        return false;
    }
    if (!Engine::ControllerCastSelf(0)) return false;
    LastQCastTick = Now();
    QActive = true;
    QActiveUntil = LastQCastTick + kQDurationMs;
    ObservedQResource = 0;
    return true;
}

inline WContext BuildWContext(const AIHeroClient& target,
                              Mode mode,
                              bool peel,
                              SDK::PredictionOutput& prediction) {
    WContext context{};
    const bool empowered = Transcendent || RuntimeW2();
    context.Ready = CanUse(1, mode, peel);
    context.Empowered = empowered;
    context.PredictionAccepted = context.Ready && TargetUsable(target) &&
        PredictionHits(1, target, SDK::HitChance::High,
                       !empowered, &prediction);
    context.TargetFirst = empowered || prediction.CollisionObjects.empty();
    context.ProjectileWall = !empowered &&
        PredictionProjectileWall(1, prediction, 30.0f);
    context.AttackReadySoon = LocalAttackReadySoon(target, 240);
    context.AfterAttack = LastAfterAttackTargetId ==
            static_cast<int>(target.NetworkId()) &&
        Now() - LastAfterAttackTick <= 420;
    const float damage = empowered
        ? RuinDamage(target) : SpellDamage(1, target);
    context.Lethal = damage >= target.Health() + target.AllShield();
    context.Peel = peel;
    return context;
}

inline bool CastW(const AIHeroClient& target,
                  Mode mode,
                  bool peel = false) {
    if (!TargetUsable(target) ||
        !CastThrottlePassed(LastWCastTick, 90)) return false;
    SDK::PredictionOutput prediction{};
    const auto context = BuildWContext(target, mode, peel, prediction);
    if (!MayCastW(context) ||
        !Engine::ControllerCastPosition(1, prediction.GetCastPosition())) {
        return false;
    }
    LastWCastTick = Now();
    if (context.Empowered && PendingRuinTargetId ==
            static_cast<int>(target.NetworkId())) {
        PendingRuinTargetId = PendingRuinUntil = 0;
    }
    return true;
}

inline bool SafeDashEndpoint(const Vector3& endpoint,
                             const AIHeroClient& threat,
                             bool flee,
                             bool lethal) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero()) {
        return false;
    }
    DashContext context{};
    context.Empowered = Transcendent || RuntimeE2();
    context.EndpointValid = player.Position().Distance2D(endpoint) <=
        kE2Range + 15.0f;
    context.Walkable = !SDK::NavMesh::IsWall(endpoint) &&
        !SDK::NavMesh::IsWallBetween(player.Position(), endpoint, 25.0f);
    context.TurretSafe = !Engine::UnderEnemyTurret(endpoint) ||
        Engine::UnderEnemyTurret(player.Position()) || lethal;
    context.LockdownSafe = !HasReadyPointClickThreatAt(endpoint) &&
        !HasReadyDashHazardAt(endpoint) &&
        Engine::PositionDangerScore(
            endpoint, threat, Engine::ResolvedSpecs[2]) > -10000.0f;
    context.DirectionUseful = !threat.IsValid() || flee ||
        endpoint.Distance2D(threat.Position()) >=
            player.Position().Distance2D(threat.Position()) + 35.0f;
    context.Flee = flee;
    context.LethalReposition = lethal;
    context.EnemiesAtEndpoint = Engine::CountEnemiesAt(endpoint, 625.0f);
    context.MaximumEnemies = Slider(
        MobilityMenu, "MaximumEnemies", 1);
    return MayDash(context);
}

inline bool CastEmpoweredE(const AIHeroClient& threat,
                           Mode mode,
                           bool flee,
                           bool lethal = false) {
    if (!(Transcendent || RuntimeE2()) || !CanUse(2, mode, true) ||
        ControllerHelpers::PlayerMobilityLocked() ||
        !CastThrottlePassed(LastECastTick, 90)) return false;
    const Vector3 requested = Engine::BestSafePosition(
        Engine::ResolvedSpecs[2], threat,
        flee ? AimPolicy::SafeCursor : AimPolicy::AwayFromThreat);
    const Vector3 endpoint = ClampDashEndpoint(
        GameObjects::Player().Position(), requested);
    if (!SafeDashEndpoint(endpoint, threat, flee, lethal) ||
        !Engine::ControllerCastPosition(2, endpoint)) return false;
    LastECastTick = Now();
    return true;
}

inline bool CastBaseE(Mode mode) {
    if (Transcendent || RuntimeE2() || !CanUse(2, mode, true) ||
        ControllerHelpers::PlayerMobilityLocked() ||
        !CastThrottlePassed(LastECastTick, 90)) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    LastECastTick = Now();
    return true;
}

inline UltimateContext BuildRContext(const AIHeroClient& target) {
    UltimateContext context{};
    const auto player = GameObjects::Player();
    context.Ready = Engine::RuntimeSpells[3] &&
        Engine::RuntimeSpells[3]->IsReady();
    context.AlreadyActive = Transcendent || RuntimeW2() || RuntimeE2();
    if (!player.IsValid() || !TargetUsable(target)) return context;
    const float distance = player.Position().Distance2D(target.Position());
    context.TargetReachable = distance <= kWRange + target.BoundingRadius() ||
        AttackRoute(target);
    context.PlayerSafe = player.HealthPercent() >=
        Slider(TranscendMenu, "MinimumHealth", 32) &&
        !Engine::UnderEnemyTurret(player.Position());
    context.NearbyEnemies = Engine::CountEnemiesAt(
        player.Position(), 850.0f);
    context.MaximumEnemies = Slider(
        TranscendMenu, "MaximumEnemies", 2);
    context.CommittedCombat = LastMode == Mode::Combo &&
        (AttackRoute(target) || distance <= 850.0f) &&
        target.Health() + target.AllShield() > AutoDamage(target) * 1.5f;
    context.EmpoweredWLethal = CanUse(1, Mode::Automatic, true) &&
        RuinDamage(target) >= target.Health() + target.AllShield();
    return context;
}

inline bool CastR(const AIHeroClient& target,
                  Mode mode) {
    if (!Engine::RuntimeSpells[3] ||
        !Engine::RuntimeSpells[3]->IsReady() ||
        !CastThrottlePassed(LastRCastTick, 100)) return false;
    if (!CanUse(3, mode, true)) return false;
    const auto context = BuildRContext(target);
    if (!MayTranscend(context) || !Engine::ControllerCastSelf(3)) {
        return false;
    }
    LastRCastTick = Now();
    Transcendent = true;
    TranscendentUntil = LastRCastTick + kRDurationMs;
    QActive = true;
    QActiveUntil = TranscendentUntil;
    if (context.EmpoweredWLethal && target.IsValid()) {
        PendingRuinTargetId = static_cast<int>(target.NetworkId());
        PendingRuinUntil = LastRCastTick + 1000;
    }
    return true;
}

inline MarksmanTargeting::TargetContext TargetFacts(
    const AIHeroClient& target, Mode mode) {
    const bool attack = AttackRoute(target);
    SDK::PredictionOutput prediction{};
    const auto w = BuildWContext(target, mode, false, prediction);
    const bool wReachable = MayCastW(w);
    float estimated = attack ? AutoDamage(target) * 2.0f : 0.0f;
    if (wReachable) estimated += w.Empowered
        ? RuinDamage(target) : SpellDamage(1, target);
    auto context = BaseTargetContext(target, estimated);
    context.AutoReachable = attack;
    context.DirectSpellReachable = wReachable;
    context.ExecuteReachable = wReachable && w.Lethal;
    context.ProjectileBlocked = !attack && !wReachable &&
        (ProjectileAttackBlocked(target) || w.ProjectileWall);
    return context;
}

inline AIHeroClient SelectTarget(Mode mode) {
    return ControllerHelpers::SelectReachableEnemy(
        {}, kWRange + 100.0f,
        [mode](const AIHeroClient& enemy) {
            return TargetFacts(enemy, mode);
        });
}

inline bool TryPendingRuin() {
    if (PendingRuinUntil < Now()) {
        PendingRuinTargetId = PendingRuinUntil = 0;
        return false;
    }
    const auto target = HeroByNetworkId(PendingRuinTargetId);
    return TargetUsable(target) && CastW(target, Mode::Automatic);
}

inline bool TryGapcloser() {
    if (GapcloserUntil < Now()) return false;
    const auto threat = HeroByNetworkId(GapcloserTargetId);
    if (!TargetUsable(threat)) return false;
    if ((Transcendent || RuntimeE2()) &&
        CastEmpoweredE(threat, Mode::Automatic, true)) return true;
    if (CastW(threat, Mode::Automatic, true)) return true;
    return CastBaseE(Mode::Automatic);
}

inline bool TryKillSecure() {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    const auto target = SelectTarget(Mode::Automatic);
    if (!TargetUsable(target)) return false;
    SDK::PredictionOutput prediction{};
    auto w = BuildWContext(target, Mode::Automatic, false, prediction);
    if (w.Lethal && CastW(target, Mode::Automatic)) return true;
    const auto r = BuildRContext(target);
    return r.EmpoweredWLethal && CastR(target, Mode::Automatic);
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    LastMode = mode;
    ReconcileState();
    const auto target = SelectTarget(
        mode == Mode::None ? Mode::Automatic : mode);
    if (TryPendingRuin() || TryGapcloser() || TryKillSecure()) {
        return true;
    }

    if (mode == Mode::Combo) {
        if (CastR(target, mode)) return true;
        if (CastW(target, mode)) return true;
        const auto player = GameObjects::Player();
        if (TargetUsable(target) && player.IsValid() &&
            player.Position().Distance2D(target.Position()) < 330.0f &&
            CastEmpoweredE(target, mode, false, true)) return true;
    } else if (mode == Mode::Harass) {
        if (CastW(target, mode)) return true;
        if (TargetUsable(target) && IsEscaping(target) &&
            !LocalAttackReadySoon(target, 260)) return CastBaseE(mode);
    } else if (mode == Mode::Flee) {
        const auto threat = ControllerHelpers::NearestEnemyToPlayer(
            {}, kWRange + 100.0f);
        if (CastEmpoweredE(threat, mode, true)) return true;
        if (CastW(threat, mode, true)) return true;
        return CastBaseE(mode);
    } else if (mode == Mode::LaneClear || mode == Mode::Jungle ||
               mode == Mode::LastHit) {
        return Engine::TryFarm(mode);
    }
    return false;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    (void)CastQForAttack(AIBaseClient(args.Target.Handle()), LastMode);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(
            args, LastAfterAttackTargetId, LastAfterAttackTick)) return;
    ObservedQResource = AddQResource(
        ObservedQResource, args.Target.IsHero());
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    int index = -1;
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        ControllerHelpers::SpellEventNameContainsAny(args, {"YunaraQ"})) {
        index = 0;
        LastQCastTick = now;
        QActive = true;
        QActiveUntil = now + kQDurationMs;
        ObservedQResource = 0;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"YunaraW", "YunaraW2"})) {
        index = 1;
        LastWCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"YunaraE", "YunaraE2"})) {
        index = 2;
        LastECastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               ControllerHelpers::SpellEventNameContainsAny(args, {"YunaraR"})) {
        index = 3;
        LastRCastTick = now;
        Transcendent = QActive = true;
        TranscendentUntil = QActiveUntil = now + kRDurationMs;
    }
}

inline void UpdateBuffState(
    const SDK::Events::BuffEventArgs& args, bool added) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (Engine::TextContains(args.BuffName, "YunaraR")) {
        Transcendent = added;
        TranscendentUntil = added
            ? std::max(now + 100, static_cast<int>(args.EndTime * 1000.0f)) : 0;
        if (added) {
            QActive = true;
            QActiveUntil = TranscendentUntil;
        }
    } else if (Engine::TextContains(args.BuffName, "YunaraQ")) {
        if (args.Count > 1) {
            ObservedQResource = ClampQResource(args.Count);
        } else {
            QActive = added;
            QActiveUntil = added
                ? std::max(now + 100, static_cast<int>(args.EndTime * 1000.0f)) : 0;
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "YunaraMechanics", "Yunara Mechanics"));
    TranscendMenu = TacticsMenu->AddSubMenu(new Menu(
        "TranscendLogic", "Transcend One's Self"));
    TranscendMenu->Add(new MenuSlider(
        "MinimumHealth", "Min health for automatic R", 32, 10, 80));
    TranscendMenu->Add(new MenuSlider(
        "MaximumEnemies", "Max nearby enemies for auto R", 2, 1, 5));
    MobilityMenu = TacticsMenu->AddSubMenu(new Menu(
        "ShadowSafety", "Untouchable Shadow Safety"));
    MobilityMenu->Add(new MenuSlider(
        "MaximumEnemies", "Maximum enemies at E landing", 1, 0, 3));
    MobilityMenu->Add(new MenuSeparator(
        "Conservative", "Unknown R/E blocks dash"));
}

inline void OnLoad() {
    Transcendent = QActive = false;
    TranscendentUntil = QActiveUntil = ObservedQResource = 0;
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    PendingRuinTargetId = PendingRuinUntil = 0;
    GapcloserTargetId = GapcloserUntil = 0;
    GapcloserEndpoint = {};
    LastMode = Mode::None;
    ReconcileState();
}

inline void OnUnload() {
    TacticsMenu = TranscendMenu = MobilityMenu = nullptr;
    Transcendent = QActive = false;
    PendingRuinTargetId = PendingRuinUntil = 0;
    LastMode = Mode::None;
}

inline constexpr const char* Scenarios[] = {
    "Use Riot 26.15 and CommunityDragon 16.15 as the kit authority",
    "Observe the eight-point Q resource without casting from guessed stacks",
    "Activate Q only for a real unblocked orbwalker attack",
    "Reconcile the five-second Q buff from cast, buff and polling state",
    "Treat base W as a 1150-range colliding projectile",
    "Reject base W when another body is first or a projectile wall intervenes",
    "Treat empowered W2 as the instant 90-width Arc of Ruin form",
    "Use post-attack W weaving without stealing a ready basic attack",
    "Activate R only for committed combat or an empowered-W kill",
    "Queue lethal Arc of Ruin after the R state becomes observable",
    "Reconcile the fifteen-second R state from W2, E2, buff and cast events",
    "Use base E movement speed for pursuit or retreat without inventing a dash",
    "Use empowered E2 only to a clamped walkable 450-unit endpoint",
    "Reject E2 into new turret aggro, lockdown or excess enemies",
    "Use W/E conservatively against a committed gapcloser",
    "Use the engine-selected target with threat-aware gapcloser policy",
    "Reject Yunara projectile attacks across a projectile wall",
    "Run Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic policies",
    "Reconcile player Q/W/E/R casts from events without ownership delays",
    "Never automate summoners, items or an unobservable empowered form",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Yunara;
    controller.ControllerId = "champion.kuroaio.ai.yunara.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIYunara.md";
    controller.ImplementationSummary =
        "Q attack ownership, base/empowered W collision split, conservative "
        "R reconciliation and threat-checked 450-unit empowered E spacing.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &ObserveLocalSpell;
    controller.OnBuffAdd =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnBuffRemove =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserUntil, 700, 850>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Yunara
