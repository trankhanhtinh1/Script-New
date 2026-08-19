#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AIZeriGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>

namespace Plugins::KuroAIO::AI::Controllers::Zeri {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::SpellEventNameContainsAny;

inline Menu* TacticsMenu = nullptr;
inline Menu* ShotMenu = nullptr;
inline Menu* SurgeMenu = nullptr;
inline Menu* CrashMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline int OverchargeStacks = 0;
inline int OverchargeExpireTick = 0;
inline bool ChargedShot = false;
inline bool RActive = false;
inline Mode LastMode = Mode::None;
inline AIHeroClient LastSmartTarget = {};

inline bool HasChargedShotBuff(const AIHeroClient& player) {
    return player.IsValid() &&
        (player.HasBuff("ZeriQPassiveReady") ||
         player.HasBuff("ZeriQCharge") ||
         player.HasBuff("ZeriPassiveReady"));
}

inline bool LiveRBuff(const AIHeroClient& player) {
    return player.IsValid() &&
        (player.HasBuff("ZeriR") || player.HasBuff("ZeriRBuff") ||
         player.HasBuff("ZeriROvercharge"));
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    const int now = Now();
    if (!player.IsValid()) {
        ChargedShot = false;
        RActive = false;
        OverchargeStacks = 0;
        OverchargeExpireTick = 0;
        return;
    }
    ChargedShot = HasChargedShotBuff(player);
    const bool liveR = LiveRBuff(player);
    if (liveR) {
        RActive = true;
        const int observed = player.GetBuffCount("ZeriROvercharge");
        if (observed > 0) OverchargeStacks = ClampOvercharge(observed);
        OverchargeExpireTick = std::max(
            OverchargeExpireTick, RefreshOverchargeExpiry(now));
    } else if (RActive && now >= OverchargeExpireTick) {
        RActive = false;
        OverchargeStacks = 0;
        OverchargeExpireTick = 0;
    }
    if (OwnedFocusTargetId != 0 && now > OwnedFocusUntil) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline bool QPrediction(const AIHeroClient& target,
                        SDK::PredictionOutput* output = nullptr) {
    if (!Engine::RuntimeSpells[0] || !target.IsValid()) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(
        0, target, SDK::HitChance::High, true, &prediction) &&
        !PredictionProjectileWall(0, prediction, kQWidth * 0.5f);
    if (output) *output = prediction;
    return hit;
}

inline bool WPrediction(const AIHeroClient& target,
                        SDK::PredictionOutput* output = nullptr,
                        bool* throughWall = nullptr) {
    if (!Engine::RuntimeSpells[1] || !target.IsValid()) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(
        1, target, SDK::HitChance::High, true, &prediction);
    const auto player = GameObjects::Player();
    const bool wall = player.IsValid() && prediction.GetCastPosition().IsValid() &&
        SDK::NavMesh::IsWallBetween(
            player.Position(), prediction.GetCastPosition(), kWWidth * 0.25f);
    if (throughWall) *throughWall = wall;
    if (output) *output = prediction;
    // A terrain laser is intentional; ordinary projectile walls remain unsafe.
    return hit && (wall || !PredictionProjectileWall(1, prediction, kWWidth * 0.5f));
}

inline bool AttackRoute(const AIHeroClient& target) {
    return OrbwalkerAttackRoute(target) &&
           !ControllerHelpers::ProjectileWallBlocksFromPlayer(
               target.Position(), 0.0f);
}

inline MarksmanTargeting::TargetContext TargetFacts(
    const AIHeroClient& target, Mode mode) {
    const auto player = GameObjects::Player();
    const float distance = player.IsValid()
        ? player.Position().Distance2D(target.Position()) : FLT_MAX;
    const bool attack = AttackRoute(target);
    SDK::PredictionOutput qPrediction{};
    SDK::PredictionOutput wPrediction{};
    bool wall = false;
    const bool q = CanUse(0, mode, true) &&
        InReach(distance, kQRange, target.BoundingRadius()) &&
        QPrediction(target, &qPrediction);
    const bool w = CanUse(1, mode) &&
        InReach(distance, kWRange, target.BoundingRadius()) &&
        WPrediction(target, &wPrediction, &wall);
    const bool r = RActive || (CanUse(3, mode) &&
        Engine::CountEnemiesAt(player.Position(), kRRange) >=
            Slider(CrashMenu, "MinimumEnemies", 2));
    const bool e = CanUse(2, mode, true) &&
        (distance > 650.0f || IsEscaping(target));
    std::array<bool, 4> reachable{q, w, e, r};
    auto context = BaseTargetContext(
        target, EstimatedDamage(target, reachable, attack ? 1 : 0));
    context.AutoReachable = attack;
    context.DirectSpellReachable = q || w;
    context.SetupReachable = e || r;
    context.ExecuteReachable = context.Killable && (q || w || r);
    context.ProjectileBlocked = !attack && !q && !w;
    (void)wall;
    return context;
}

inline AIHeroClient SelectSmartTarget(const AIHeroClient& preferred, Mode mode) {
    LastSmartTarget = ControllerHelpers::SelectReachableEnemy(
        preferred, kWRange + 50.0f,
        [mode](const AIHeroClient& enemy) {
            return TargetFacts(enemy, mode);
        });
    return LastSmartTarget;
}

inline bool SafeDashEndpoint(const Vector3& endpoint,
                             const AIHeroClient& threat,
                             bool flee,
                             bool lethal) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || endpoint.IsZero() || !endpoint.IsValid()) return false;
    DashSafety context{};
    context.EndpointValid = player.Position().Distance2D(endpoint) <= kERange + 15.0f;
    context.Walkable = !SDK::NavMesh::IsWall(endpoint);
    context.TerrainInteraction = SDK::NavMesh::IsWallBetween(
        player.Position(), endpoint, 25.0f);
    context.TurretSafe = !Engine::UnderEnemyTurret(endpoint) ||
        Engine::UnderEnemyTurret(player.Position()) || lethal;
    context.ThreatSafe = !ControllerHelpers::HasReadyPointClickThreatAt(endpoint) &&
        !ControllerHelpers::HasReadyDashHazardAt(endpoint);
    context.DirectionUseful = !threat.IsValid() || flee ||
        endpoint.Distance2D(threat.Position()) >=
            player.Position().Distance2D(threat.Position()) + 30.0f;
    context.Flee = flee;
    context.Lethal = lethal;
    context.EnemiesAtEndpoint = Engine::CountEnemiesAt(endpoint, 500.0f);
    context.MaximumEnemies = Slider(SurgeMenu, "MaximumEnemies", 1);
    return MayDash(context);
}

inline bool CastQ(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target, kQRange + 80.0f) ||
        !CanUse(0, mode, true) || !CastThrottlePassed(LastQCastTick, 28)) {
        return false;
    }
    SDK::PredictionOutput prediction{};
    if (!QPrediction(target, &prediction)) return false;
    const bool attackWindow = LocalAttackReadySoon(target, 180);
    const auto shot = ReconcileShotState(
        ChargedShot, attackWindow, ControllerHelpers::PlayerMobilityLocked());
    if (shot.Charged && !ShouldPreserveShot(shot, true, false)) return false;
    if (!Engine::ControllerCastPosition(0, prediction.GetCastPosition())) return false;
    LastQCastTick = Now();
    ChargedShot = false;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target, kWRange + 80.0f) ||
        !CanUse(1, mode) || !CastThrottlePassed(LastWCastTick, 55)) return false;
    SDK::PredictionOutput prediction{};
    bool wall = false;
    if (!WPrediction(target, &prediction, &wall)) return false;
    if (!Engine::ControllerCastPosition(1, prediction.GetCastPosition())) return false;
    LastWCastTick = Now();
    return true;
}

inline bool CastE(const AIHeroClient& threat,
                  Mode mode,
                  bool flee,
                  bool lethal = false) {
    if (!CanUse(2, mode, true) || ControllerHelpers::PlayerMobilityLocked() ||
        !CastThrottlePassed(LastECastTick, 70)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const Vector3 requested = Engine::BestSafePosition(
        Engine::ResolvedSpecs[2], threat,
        flee ? AimPolicy::SafeCursor : AimPolicy::AwayFromThreat);
    const Vector3 endpoint = ClampDashEndpoint(player.Position(), requested);
    if (!SafeDashEndpoint(endpoint, threat, flee, lethal) ||
        !Engine::ControllerCastPosition(2, endpoint)) return false;
    LastECastTick = Now();
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    return true;
}

inline bool CastR(const AIHeroClient& target,
                  Mode mode,
                  bool manual = false,
                  bool reactive = false) {
    if (!Engine::RuntimeSpells[3] || !CanUse(3, mode, true) ||
        !CastThrottlePassed(LastRCastTick, 100)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int nearby = Engine::CountEnemiesAt(player.Position(), kRRange);
    const bool lethal = Engine::RuntimeSpells[3]->GetDamage(target) >=
        target.Health() + target.AllShield();
    const bool safe = Engine::PositionDangerScore(
        player.Position(), target, Engine::ResolvedSpecs[3]) > -10000.0f;
    const bool committed = LastAfterAttackTargetId ==
        static_cast<int>(target.NetworkId()) && Now() - LastAfterAttackTick < 700;
    if (!ShouldCastUltimate(Engine::RuntimeSpells[3]->IsReady(), RActive,
                            manual || reactive, committed, lethal, safe, nearby,
                            Slider(CrashMenu, "MinimumEnemies", 2))) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastRCastTick = Now();
    RActive = true;
    OverchargeStacks = 0;
    OverchargeExpireTick = Now() + kOverchargeDurationMs;
    (void)SetTemporaryOrbwalkerFocus(
        target, kRRange, 900, OwnedFocusTargetId, OwnedFocusUntil);
    return true;
}

inline bool TryManualR(const AIHeroClient& preferred) {
    if (!ManualUltimatePressed()) return false;
    const auto target = SelectSmartTarget(preferred, Mode::Automatic);
    return Engine::ValidEnemy(target, kRRange) && CastR(target, Mode::Automatic, true);
}

inline bool TryReactive() {
    if (InterruptExpireTick >= Now()) {
        const auto target = HeroByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(target, kWRange) && CastW(target, Mode::Automatic)) return true;
    }
    if (GapcloserExpireTick >= Now()) {
        const auto target = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(target, kRRange) &&
            CastR(target, Mode::Automatic, false, true)) return true;
        if (Engine::ValidEnemy(target, kWRange) && CastW(target, Mode::Automatic)) return true;
    }
    return false;
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    const auto target = SelectSmartTarget(preferred, Mode::Automatic);
    if (!Engine::ValidEnemy(target, kWRange)) return false;
    if (SpellDamage(0, target) >= target.Health() + target.AllShield() &&
        CastQ(target, Mode::Automatic)) return true;
    return SpellDamage(1, target) >= target.Health() + target.AllShield() &&
           CastW(target, Mode::Automatic);
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target, kWRange)) return false;
    const auto player = GameObjects::Player();
    if (player.IsValid() && !RActive && CanUse(3, mode, true) &&
        Engine::CountEnemiesAt(player.Position(), kRRange) >=
            Slider(CrashMenu, "MinimumEnemies", 2) &&
        CastR(target, mode)) return true;
    if (CastQ(target, mode)) return true;
    if (CastW(target, mode)) return true;
    if (player.IsValid() &&
        player.Position().Distance2D(target.Position()) > 700.0f &&
        CastE(target, mode, false)) return true;
    return false;
}

inline bool TryFlee(const AIHeroClient& preferred) {
    const auto threat = ControllerHelpers::NearestEnemyToPlayer(preferred, kWRange);
    if (!Engine::ValidEnemy(threat)) return false;
    if (CastE(threat, Mode::Flee, true)) return true;
    return CastW(threat, Mode::Flee);
}

inline bool TryFarm(Mode mode) {
    const bool lastHit = mode == Mode::LastHit;
    const bool jungle = !lastHit && !Engine::ClearUnits(true).empty() &&
        Engine::ClearUnits(false).empty();
    if (CanUse(1, mode) && Engine::TryFarmSpell(1, jungle, lastHit)) return true;
    return Engine::TryFarmSpell(0, jungle, lastHit);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    ReconcileState();
    if (TryManualR(preferred)) return true;
    if (TryReactive()) return true;
    if (TryKillSecure(preferred)) return true;
    if (mode == Mode::Flee) return TryFlee(preferred);
    if (mode == Mode::Combo || mode == Mode::Harass) {
        const auto target = SelectSmartTarget(preferred, mode);
        return TryCombat(target, mode);
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        return TryFarm(mode);
    }
    return false;
}

inline void ObserveLocalSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (args.IsAutoAttack) return;
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        SpellEventNameContainsAny(args, {"zeriq", "burstfire"})) {
        LastQCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               SpellEventNameContainsAny(args, {"zeriw", "ultrashock"})) {
        LastWCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               SpellEventNameContainsAny(args, {"zerie", "sparksurge"})) {
        LastECastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               SpellEventNameContainsAny(args, {"zerir", "lightningcrash"})) {
        LastRCastTick = now;
        RActive = true;
        OverchargeStacks = 0;
        OverchargeExpireTick = now + kOverchargeDurationMs;
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAfterAttackTargetId, LastAfterAttackTick)) return;
    if (RActive) {
        OverchargeStacks = AddOvercharge(OverchargeStacks);
        OverchargeExpireTick = RefreshOverchargeExpiry(Now());
    }
    const auto target = HeroByNetworkId(LastAfterAttackTargetId);
    if (Engine::ValidEnemy(target, kWRange)) {
        (void)SetTemporaryOrbwalkerFocus(
            target, kWRange, 520, OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    const auto focus = OwnedOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil, kWRange);
    if (focus.IsValid() && RedirectBeforeAttackToFocus(args, focus)) return;
    if (ChargedShot && LastQCastTick > 0 && Now() - LastQCastTick < 100) {
        args.Process = false;
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("ZeriMechanics", "Zeri Mechanics"));
    ShotMenu = TacticsMenu->AddSubMenu(new Menu("BurstFire", "Burst Fire"));
    ShotMenu->Add(new MenuSeparator("ChargedPolicy", "Preserve charged passive shot and AA windup"));
    SurgeMenu = TacticsMenu->AddSubMenu(new Menu("SparkSurge", "Spark Surge"));
    SurgeMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at dash endpoint", 1, 0, 3));
    CrashMenu = TacticsMenu->AddSubMenu(new Menu("LightningCrash", "Lightning Crash"));
    CrashMenu->Add(new MenuSlider("MinimumEnemies", "Minimum enemies for automatic R", 2, 1, 5));
}

inline void OnLoad() {
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    InterruptTargetId = InterruptExpireTick = 0;
    GapcloserEndpoint = {};
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    OverchargeStacks = OverchargeExpireTick = 0;
    ChargedShot = RActive = false;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    TacticsMenu = ShotMenu = SurgeMenu = CrashMenu = nullptr;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Reconcile charged/basic Q state from passive buffs every update before choosing an action",
    "Preserve a charged passive shot and never replace a real AA windup with speculative W",
    "Require Q prediction to hit the first collision body and reject projectile-wall routes",
    "Use W's widened terrain laser path only when a wall is intentionally crossed",
    "Reject W when a minion or collision body is predicted before the selected hero",
    "Keep selected-target, orbwalker-target and owned-focus identities aligned",
    "Build Lightning Crash overcharge from successful R and observed follow-up attacks",
    "Expire overcharge stacks from the live buff window instead of stale local state",
    "Cast R automatically only for a safe committed multi-target or lethal window",
    "Allow manual R through the same target validity and safety checks",
    "Use Spark Surge only toward a walkable endpoint with no point-click or dash hazard",
    "Permit a terrain dash as a deliberate interaction while rejecting turret traps",
    "Use E for flee before W peel and never dash while movement is locked",
    "Use W and Q for lane clear, jungle and last-hit through the shared farm route",
    "Respond to interrupt and gapcloser events with W/R before normal combat",
    "Release temporary orbwalker focus when a target leaves reach or its lifetime ends",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Zeri;
    controller.ControllerId = "champion.kuroaio.ai.zeri.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIZeri.md";
    controller.ImplementationSummary =
        "Owns Zeri's Q-as-basic-shot state, first-body prediction and terrain-aware W, overcharge R reconciliation, safe E dash planning, orbwalker focus and full combat/farm/flee/automatic modes.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &ObserveLocalSpell;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 900, 850>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1500, 250, 5000>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Zeri
