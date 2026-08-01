#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIMonkeyKingGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::MonkeyKing {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline CloneState Clone{};
inline SpinPosture RPosture = SpinPosture::Idle;
inline int QCastTick = 0;
inline int WCastTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int RChannelStartTick = 0;
inline int QArmorUntilTick = 0;
inline int QTargetId = 0;
inline int RTargetId = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int PlayerOverrideUntil = 0;
inline bool RManual = false;
inline Vector3 LastDashEndpoint{};

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 70) {
    const int cast = slot == 0 ? QCastTick : slot == 1 ? WCastTick :
        slot == 2 ? ECastTick : RCastTick;
    return Now() - cast >= delay;
}
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
inline bool InterruptActive() { return InterruptExpireTick > Now() && InterruptTargetId != 0; }
inline AIHeroClient InterruptTarget() { return HeroByNetworkId(InterruptTargetId); }
using ControllerHelpers::TotalAttackDamage;
using ControllerHelpers::BonusAttackDamage;
using ControllerHelpers::AP;
inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target,
            QRawDamage(SpellRank(0), BonusAttackDamage())) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target,
            ERawDamage(SpellRank(2), AP())) : 0.0f;
}
inline float RDamage(const AIHeroClient& target, int ticks = kRMaximumTicks) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, RTotalRawDamage(
            SpellRank(3), TotalAttackDamage(), target.MaxHealth(), ticks)) : 0.0f;
}
using ControllerHelpers::Lethal;
inline bool CloneIsActive() { return CloneActive(Clone, Now()); }
inline bool UnderTurret(const Vector3& position, bool lethal) {
    return Engine::UnderEnemyTurret(position) && !lethal;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 70.0f) ||
        !Ready(0, mode) || !Throttle(0, 45) || Protected(target) ||
        PreserveAttack(reactive) || RPosture != SpinPosture::Idle) return false;
    const bool lethal = Lethal(target, QDamage(target));
    if (player.Position().Distance2D(target.Position()) >
        kQRange + target.BoundingRadius() || (!reactive && !lethal &&
        Orbwalker::IsWindingUp() && Bool(QMenu, "PreserveWindup", true))) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    QCastTick = Now();
    QTargetId = static_cast<int>(target.NetworkId());
    QArmorUntilTick = QCastTick + 3000;
    return true;
}

inline bool CastW(Mode mode, bool fleeing = false, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 90) ||
        PreserveAttack(reactive) || RPosture != SpinPosture::Idle) return false;
    const bool pressured = Engine::CountEnemiesAt(player.Position(), 500.0f) >
        Engine::CountAlliesAt(player.Position(), 550.0f) + 1;
    if (!fleeing && !reactive && !pressured &&
        Engine::UnderEnemyTurret(player.Position())) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WCastTick = Now();
    RecordClone(Clone, -1, player.Position(), WCastTick,
                Slider(WMenu, "CloneDurationMs", static_cast<int>(kCloneDurationMs)));
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool fleeing = false,
                  bool reactive = false, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange + 45.0f) ||
        !Ready(2, mode) || !Throttle(2, 70) || Protected(target) ||
        PreserveAttack(reactive) || RPosture != SpinPosture::Idle) return false;
    const Vector3 endpoint = ClampDashEndpoint(player.Position(),
        PredictPosition(target, 0.20f));
    if (endpoint.IsZero() || !endpoint.IsValid() || SDK::NavMesh::IsWall(endpoint) ||
        SDK::NavMesh::IsWallBetween(player.Position(), endpoint, 45.0f)) return false;
    const bool targetValid = endpoint.Distance2D(target.Position()) <=
        kEDashRadius + target.BoundingRadius() + 80.0f;
    const DashContext context{ true, targetValid, true,
        UnderTurret(endpoint, lethal || fleeing), false, fleeing, lethal,
        Engine::CountEnemiesAt(endpoint, 300.0f),
        Slider(EMenu, "MaxEndpointEnemies", 2) };
    if (!ShouldNimbusStrike(context)) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    ECastTick = Now();
    LastDashEndpoint = endpoint;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool defensive = false,
                  bool manual = false, bool interrupt = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kRRadius + 70.0f) ||
        Protected(target) || RPosture != SpinPosture::Idle ||
        !Ready(3, mode) || !Throttle(3, 120) || PreserveAttack(defensive)) return false;
    const bool lethal = Lethal(target, RDamage(target));
    int targets = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && SpinHits(player.Position(),
            PredictPosition(enemy, 0.20f), enemy.BoundingRadius())) ++targets;
    }
    const SpinContext context{ true, true,
        SpinHits(player.Position(), PredictPosition(target, 0.20f), target.BoundingRadius()),
        interrupt, defensive, lethal, manual, Orbwalker::IsWindingUp(),
        UnderTurret(player.Position(), lethal || defensive), targets,
        Slider(RMenu, "MinimumTargets", 2) };
    if (!ShouldStartSpin(context)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RCastTick = RChannelStartTick = Now();
    RPosture = SpinPosture::Channeling;
    RTargetId = static_cast<int>(target.NetworkId());
    RManual = manual;
    return true;
}

inline bool RecastR(const AIHeroClient& target, bool interrupt = false,
                    bool defensive = false) {
    if (RPosture != SpinPosture::Channeling || RManual) return false;
    const auto player = GameObjects::Player();
    const bool valid = player.IsValid() && Engine::ValidEnemy(target, kRRadius + 100.0f);
    const bool lethal = valid && Lethal(target, RDamage(target));
    const int elapsed = Now() - RChannelStartTick;
    const bool lost = !valid || !SpinHits(player.Position(),
        PredictPosition(target, 0.20f), target.BoundingRadius());
    const SpinContext context{ true, valid, !lost, interrupt, defensive, lethal,
        false, false, false, 0, 2 };
    if (!ShouldContinueSpin(context, RPosture, elapsed) &&
        !ShouldRecastSpin(RPosture, elapsed, lost, interrupt, lethal)) return false;
    if (!ShouldRecastSpin(RPosture, elapsed, lost, interrupt, lethal)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RCastTick = Now();
    RPosture = SpinPosture::RecastReady;
    return true;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, QDamage(target)) && CastQ(target, mode, true)) return true;
    if (Lethal(target, EDamage(target)) && CastE(target, mode, false, true, true)) return true;
    return Lethal(target, RDamage(target)) && CastR(target, mode, false, false, false);
}
inline bool TryInterrupt(const AIHeroClient& fallback) {
    const AIHeroClient interrupt = InterruptActive() ? InterruptTarget() : fallback;
    return Engine::ValidEnemy(interrupt, kRRadius + 75.0f) &&
        CastR(interrupt, Mode::Automatic, true, false, true);
}
inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastE(target, Mode::Combo)) return true;
    if (CastQ(target, Mode::Combo)) return true;
    if (CastW(Mode::Combo)) return true;
    return CastR(target, Mode::Combo, false, false, false);
}
inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || GameObjects::Player().ManaPercent() <
        Slider(TacticsMenu, "HarassMana", 45)) return false;
    if (CastQ(target, Mode::Harass)) return true;
    return Bool(WMenu, "HarassDecoy", false) && CastW(Mode::Harass);
}
inline bool TryFlee(const AIHeroClient& threat) {
    if (RPosture == SpinPosture::Channeling) return RecastR(threat, true, true);
    if (CastW(Mode::Flee, true, true)) return true;
    if (Engine::ValidEnemy(threat)) return CastE(threat, Mode::Flee, true, true, false);
    return false;
}
inline bool TryFarm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "Mana", 42)) return false;
    return Engine::TryFarm(mode);
}

inline void ReconcileState() {
    const int now = Now();
    if (Clone.ExpireTick > 0 && now > Clone.ExpireTick) Clone = {};
    if (QArmorUntilTick > 0 && now > QArmorUntilTick) {
        QArmorUntilTick = 0;
        QTargetId = 0;
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("MonkeyKingSpinToWin") || player.HasBuff("MonkeyKingR")) {
        if (RPosture == SpinPosture::Idle) RChannelStartTick = now;
        RPosture = SpinPosture::Channeling;
    } else if (RPosture != SpinPosture::Idle && now - RCastTick > 2500) {
        RPosture = SpinPosture::Idle;
        RManual = false;
        RTargetId = 0;
    }
    if (player.HasBuff("MonkeyKingDecoy") || player.HasBuff("MonkeyKingW")) {
        if (!CloneIsActive()) RecordClone(Clone, -1, player.Position(), now,
            Slider(WMenu, "CloneDurationMs", static_cast<int>(kCloneDurationMs)));
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (RPosture == SpinPosture::Channeling && mode != Mode::Flee) {
        const AIHeroClient spinTarget = ControllerHelpers::PreferredEnemyTarget(selected, kRRadius + 100.0f);
        if (RecastR(spinTarget, InterruptActive(), false)) return true;
        return true;
    }
    if (PlayerOverrideUntil > Now()) return true;
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, kERange + 80.0f);
    if (TryInterrupt(target)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target); break;
    case Mode::Harass: (void)TryHarass(target); break;
    case Mode::Flee: (void)TryFlee(NearestEnemyToPlayer(target, 900.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)TryFarm(mode); break;
    case Mode::Automatic:
        if (AutomaticAllowed({ false, InterruptActive(),
            Engine::ValidEnemy(target) && Lethal(target, QDamage(target)), false,
            PlayerOverrideUntil > Now()})) (void)TryInterrupt(target);
        break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) return;
    const int slot = static_cast<int>(args.Slot);
    if (slot < 0 || slot > 3) return;
    const bool owned = Engine::WasControllerCast(slot);
    if (!owned) PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
    if (slot == 0) QCastTick = now;
    else if (slot == 1) {
        WCastTick = now;
        RecordClone(Clone, -1, args.Sender.Position, now,
            Slider(WMenu, "CloneDurationMs", static_cast<int>(kCloneDurationMs)));
    } else if (slot == 2) ECastTick = now;
    else {
        RCastTick = now;
        RManual = !owned;
        if (RPosture == SpinPosture::Channeling) RPosture = SpinPosture::RecastReady;
        else RPosture = SpinPosture::Channeling;
        if (RPosture == SpinPosture::Channeling) RChannelStartTick = now;
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "MonkeyKingQArmorReduction") ||
            Engine::TextContains(args.BuffName, "MonkeyKingQDebuff")) {
            QTargetId = static_cast<int>(args.Sender.NetworkId);
            QArmorUntilTick = now + 3000;
        }
        return;
    }
    if (Engine::TextContains(args.BuffName, "MonkeyKingDecoy") ||
        Engine::TextContains(args.BuffName, "MonkeyKingW")) {
        RecordClone(Clone, Clone.NetworkId == 0 ? -1 : Clone.NetworkId,
            args.Sender.Position, now,
            ControllerHelpers::RemainingMilliseconds(args.EndTime,
                static_cast<int>(kCloneDurationMs), 250, 4000));
    }
    if (Engine::TextContains(args.BuffName, "MonkeyKingSpinToWin") ||
        Engine::TextContains(args.BuffName, "MonkeyKingR")) {
        RPosture = SpinPosture::Channeling;
        if (RChannelStartTick == 0) RChannelStartTick = now;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (!IsLocalPlayer(args.Sender)) {
        if ((Engine::TextContains(args.BuffName, "MonkeyKingQArmorReduction") ||
             Engine::TextContains(args.BuffName, "MonkeyKingQDebuff")) &&
            QTargetId == static_cast<int>(args.Sender.NetworkId)) {
            QArmorUntilTick = 0;
            QTargetId = 0;
        }
        return;
    }
    if (Engine::TextContains(args.BuffName, "MonkeyKingSpinToWin") ||
        Engine::TextContains(args.BuffName, "MonkeyKingR")) {
        RPosture = SpinPosture::Idle;
        RManual = false;
        RTargetId = 0;
    }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (RPosture == SpinPosture::Channeling) args.Process = false;
    if (args.Target.IsValid()) QTargetId = static_cast<int>(args.Target.NetworkId());
}
inline bool IsCloneObject(const char* name) {
    return Engine::TextContains(name, "MonkeyKingClone") ||
           Engine::TextContains(name, "MonkeyKingDecoy") ||
           Engine::TextContains(name, "WukongClone");
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !ControllerHelpers::ObjectEventIsAllied(args) ||
        (!IsCloneObject(args.Sender.Name) &&
         !IsCloneObject(args.Sender.CharacterName))) return;
    RecordClone(Clone, static_cast<int>(args.Sender.NetworkId), args.Sender.Position, Now(),
        Slider(WMenu, "CloneDurationMs", static_cast<int>(kCloneDurationMs)));
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && CloneMatches(static_cast<int>(args.Sender.NetworkId), Clone))
        Clone = {};
}
inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawRanges", false)) return;
    Drawing::DrawCircle(player.Position(), kERange, 0xFF65B6FFu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kRRadius, 0xFFB76DFFu, 1.5f, 36);
    if (CloneIsActive()) Drawing::DrawCircle(Clone.Position, 55.0f, 0xFFCCAA55u, 1.5f, 24);
    if (!LastDashEndpoint.IsZero()) Drawing::DrawCircle(LastDashEndpoint, kEDashRadius,
        0xFF65B6FFu, 1.0f, 24);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("MonkeyKingOneTrick", "Wukong diver tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 45, 10, 90));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Crushing Blow"));
    QMenu->Add(new MenuBool("PreserveWindup", "Preserve nonlethal attack windup", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Decoy tracking"));
    WMenu->Add(new MenuBool("HarassDecoy", "Permit Decoy in harass", false));
    WMenu->Add(new MenuSlider("CloneDurationMs", "Clone fallback duration (ms)", 2500, 1000, 3500));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Nimbus safety"));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 2, 0, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Cyclone posture"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum nonlethal spin targets", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("MonkeyKingFarm", "Conservative farm"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 42, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("MonkeyKingCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw E/R and clone ranges", false));
}
inline void OnLoad() {
    Clone = {};
    RPosture = SpinPosture::Idle;
    QCastTick = WCastTick = ECastTick = RCastTick = RChannelStartTick = 0;
    QArmorUntilTick = 0;
    QTargetId = RTargetId = LastAutoTargetId = LastAutoTick = 0;
    InterruptTargetId = InterruptExpireTick = PlayerOverrideUntil = 0;
    RManual = false;
    LastDashEndpoint = {};
    ReconcileState();
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    Clone = {};
    RPosture = SpinPosture::Idle;
}

inline constexpr const char* Scenarios[] = {
    "Pin every numeric decision to Riot 26.15 and CommunityDragon 16.15",
    "Track Decoy stealth and clone lifetime through buff, object and polling reconciliation",
    "Never treat an unconfirmed clone as a real enemy or selected target",
    "Use Q's empowered attack reset only inside the observed attack range",
    "Record Q armor reduction target ownership for the next attack window",
    "Preserve a nonlethal AA windup unless a reactive or lethal cast is justified",
    "Use Nimbus Strike only on a reachable target and a valid dash endpoint",
    "Reject E through walls, under a new enemy turret, or into excessive enemies",
    "Permit defensive or lethal E routes only when the endpoint remains observable",
    "Count E endpoint enemies before committing to an engage",
    "Model Cyclone as a two-second channel with half-second tick posture",
    "Track first Cyclone cast and recast from process-spell and buff events",
    "Reconcile Cyclone state by polling when an event is missed",
    "Hold attacks during an owned Cyclone channel to preserve spin ticks",
    "Recast Cyclone after a tick when target loss, interrupt, lethal or expiry demands it",
    "Reject ordinary single-target nonlethal Cyclone casts",
    "Permit Cyclone for multi-target, lethal, defensive and interrupt behavior",
    "Preserve manually started Cyclone channels and do not automate their recast",
    "Keep selected target before orbwalker and selector fallback",
    "Use interrupt event capture to select a committed channel target",
    "Automatic mode reacts only to interrupt, defense or verified kill-secure state",
    "Combo uses E entry, Q armor reduction, Decoy posture and Cyclone follow-up",
    "Harass prioritizes Q and keeps Decoy optional",
    "LaneClear delegates only shared farm policy",
    "Jungle delegates shared farm policy without inventing objective casts",
    "LastHit delegates shared farm policy and does not spend Cyclone",
    "Flee uses Decoy first, then defensive Nimbus Strike",
    "Never automate Flash, Ignite, Smite, items or movement ownership",
    "Reject invulnerable, protected and spell-shielded targets",
    "Yield after a manually observed Q, W, E or R cast",
    "Draw clone, dash and spin geometry without changing gameplay decisions",
    "Keep profile metadata separate from the owned decision loop",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::MonkeyKing;
    controller.ControllerId = "champion.kuroaio.ai.monkeyking.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIMonkeyKing.md";
    controller.ImplementationSummary =
        "Clone/object reconciliation, Q armor-shred attack reset, endpoint-safe Nimbus Strike and owned Cyclone channel/recast posture.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<
        &InterruptTargetId, &InterruptExpireTick, 1200, 180, 4500>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::MonkeyKing
