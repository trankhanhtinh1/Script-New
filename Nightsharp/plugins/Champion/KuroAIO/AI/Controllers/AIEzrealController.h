#pragma once

#include "../AIChampionEngine.h"
#include "../AIMarksmanControllerHelpers.h"
#include "AIEzrealGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Ezreal {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::Now;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::SpellEventNameContainsAny;

inline Menu* TacticsMenu = nullptr;
inline Menu* ComboLogicMenu = nullptr;
inline Menu* ShiftMenu = nullptr;
inline Menu* UltimateMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastWTargetId = 0;
inline int PendingWUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline AIHeroClient LastSmartTarget = {};

inline bool HasFlux(const AIBaseClient& target) {
    return target.IsValid() && target.HasBuff("ezrealwattach");
}

inline bool RecentlyAttacked(const AIBaseClient& target,
                             int windowMs = 440) {
    return RecentlyAttackedTarget(
        target, LastAfterAttackTargetId, LastAfterAttackTick, windowMs);
}

inline bool ClearQPrediction(const AIHeroClient& target,
                             SDK::PredictionOutput* output = nullptr,
                             SDK::HitChance chance = SDK::HitChance::High) {
    SDK::PredictionOutput prediction{};
    const bool valid = PredictionHits(
        0, target, chance, true, &prediction) &&
        !PredictionProjectileWall(0, prediction, 60.0f);
    if (output) *output = prediction;
    return valid;
}

inline bool ClearWPrediction(const AIHeroClient& target,
                             SDK::PredictionOutput* output = nullptr) {
    SDK::PredictionOutput prediction{};
    const bool valid = PredictionHits(
        1, target, SDK::HitChance::High, false, &prediction) &&
        !PredictionProjectileWall(1, prediction, 80.0f);
    if (output) *output = prediction;
    return valid;
}

inline void RefreshOrbwalkerFocus(Mode mode,
                                  const AIHeroClient& preferred) {
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    auto owned = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 850.0f);
    if (!combat || !owned.IsValid() || !HasFlux(owned) ||
        !InAutoAttackRange(owned)) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
        owned = {};
    }
    if (owned.IsValid()) return;

    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 850.0f) ||
            !HasFlux(enemy) || !InAutoAttackRange(enemy)) continue;
        float score = 100.0f - enemy.HealthPercent();
        if (preferred.IsValid() &&
            preferred.NetworkId() == enemy.NetworkId()) score += 170.0f;
        if (LastWTargetId == static_cast<int>(enemy.NetworkId())) {
            score += 120.0f;
        }
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    if (best.IsValid()) {
        (void)SetTemporaryOrbwalkerFocus(
            best, 850.0f, 1100,
            OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline Vector3 OffensiveShiftPoint(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return {};
    const float distance = player.Position().Distance2D(target.Position());
    const float desired = Engine::ActiveProfile
        ? Engine::ActiveProfile->PreferredCombatDistance : 800.0f;
    const float travel = std::clamp(
        distance - desired, 0.0f,
        Engine::ResolvedSpecs[2].DashDistance);
    if (travel < 60.0f) return {};
    return Engine::Extend(player.Position(), target.Position(), travel);
}

inline Vector3 DefensiveShiftPoint(const AIHeroClient& threat) {
    return Engine::BestSafePosition(
        Engine::ResolvedSpecs[2], threat, AimPolicy::AwayFromThreat);
}

inline bool ShiftPlan(const AIHeroClient& target,
                      bool defensive,
                      Vector3* destination = nullptr) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return false;
    const Vector3 point = defensive
        ? DefensiveShiftPoint(target)
        : OffensiveShiftPoint(target);
    if (destination) *destination = point;
    if (!point.IsValid() || point.IsZero()) return false;

    const bool walkable = !SDK::NavMesh::IsWall(point);
    const bool safe = walkable &&
        Engine::PositionDangerScore(
            point, target, Engine::ResolvedSpecs[2]) > -10000.0f &&
        !HasReadyPointClickThreatAt(point) &&
        !HasReadyDashHazardAt(point);
    const Vector3 predicted = ControllerHelpers::PredictPosition(target, 0.65f);
    const float aaRange = player.AttackRange() + target.BoundingRadius();
    const bool createsFollowup = predicted.IsValid() &&
        (predicted.Distance2D(point) <= aaRange ||
         predicted.Distance2D(point) <= Engine::ResolvedSpecs[0].Range);
    const bool lethal = SpellDamage(2, target) +
        (ClearQPrediction(target) ? SpellDamage(0, target) : 0.0f) >=
        target.Health() + target.AllShield();

    BlinkContext context{};
    context.DestinationSafe = safe;
    context.DestinationWalkable = walkable;
    context.Defensive = defensive;
    context.Lethal = lethal;
    context.CreatesFollowup = createsFollowup;
    context.AttackAlreadyAvailable = LocalAttackAvailable(target);
    context.EnemiesAtDestination = Engine::CountEnemiesAt(point, 650.0f);
    return ShouldBlink(context);
}

inline MarksmanTargeting::TargetContext TargetFacts(
    const AIHeroClient& target,
    Mode mode,
    bool allowLongR) {
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const bool attack = OrbwalkerAttackRoute(target);
    const bool q = CanUse(0, mode) && ClearQPrediction(target) &&
        distance <= Engine::RuntimeSpells[0]->CurrentRange() + 30.0f;
    const bool w = CanUse(1, mode) && ClearWPrediction(target) &&
        !HasFlux(target) && (q || attack);
    const bool e = CanUse(2, mode) && ShiftPlan(target, false);

    SDK::PredictionOutput rPrediction{};
    const bool rLine = allowLongR && Engine::RuntimeSpells[3] &&
        Engine::RuntimeSpells[3]->IsReady() &&
        PredictionHits(3, target, SDK::HitChance::VeryHigh,
                       false, &rPrediction) &&
        !PredictionProjectileWall(3, rPrediction, 160.0f);
    const bool r = rLine && SpellDamage(3, target) >=
        target.Health() + target.AllShield();

    const std::array<bool, 4> reachable = {q, w, e, r};
    auto context = BaseTargetContext(
        target, EstimatedDamage(target, reachable, attack ? 2 : 0));
    context.AutoReachable = attack;
    context.DirectSpellReachable = q;
    context.SetupReachable = w || e;
    context.ExecuteReachable = r;
    context.ProjectileBlocked = !attack && !q && !w && !e && !r;
    return context;
}

inline AIHeroClient SelectSmartTarget(const AIHeroClient& preferred,
                                      Mode mode,
                                      bool allowLongR = false) {
    const float searchRange = allowLongR ? 20000.0f : 1725.0f;
    const auto target = ControllerHelpers::SelectReachableEnemy(
        preferred, searchRange,
        [mode, allowLongR](const AIHeroClient& enemy) {
            return TargetFacts(enemy, mode, allowLongR);
        });
    LastSmartTarget = target;
    return target;
}

inline bool CastQ(const AIHeroClient& target, Mode mode) {
    if (!CanUse(0, mode) || !CastThrottlePassed(LastQCastTick, 20) ||
        !Engine::ValidEnemy(target, 1240.0f)) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = ClearQPrediction(target, &prediction);
    MysticShotContext context{};
    context.InRange = hit;
    context.PredictionHits = hit;
    context.Collision = !prediction.CollisionObjects.empty();
    context.ProjectileWall =
        PredictionProjectileWall(0, prediction, 60.0f);
    context.AttackWindingUp = Orbwalker::IsWindingUp();
    context.AttackAvailable = LocalAttackAvailable(target);
    context.AfterAttack = RecentlyAttacked(target);
    context.Marked = HasFlux(target);
    context.Lethal = SpellDamage(0, target) >=
        target.Health() + target.AllShield();
    if (!ShouldCastMysticShot(context)) return false;
    if (Engine::ControllerCastPosition(0, prediction.GetCastPosition())) {
        LastQCastTick = Now();
        return true;
    }
    return false;
}

inline bool CastW(const AIHeroClient& target, Mode mode) {
    if (!CanUse(1, mode) || !CastThrottlePassed(LastWCastTick, 24) ||
        !Engine::ValidEnemy(target, 1240.0f)) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = ClearWPrediction(target, &prediction);
    const bool qCanDetonate = CanUse(0, mode, true) &&
        ClearQPrediction(target);
    FluxContext context{};
    context.InRange = hit;
    context.PredictionHits = hit;
    context.ProjectileWall =
        PredictionProjectileWall(1, prediction, 80.0f);
    context.AlreadyMarked = HasFlux(target);
    context.AttackWindingUp = Orbwalker::IsWindingUp();
    context.QCanDetonate = qCanDetonate;
    context.AutoCanDetonate = InAutoAttackRange(target) &&
        (Orbwalker::CanAttack() || RecentlyAttacked(target));
    context.LethalSequence = SpellDamage(1, target) +
        (qCanDetonate ? SpellDamage(0, target) : AutoDamage(target)) >=
        target.Health() + target.AllShield();
    if (!ShouldCastFlux(context)) return false;
    if (Engine::ControllerCastPosition(1, prediction.GetCastPosition())) {
        LastWCastTick = Now();
        LastWTargetId = static_cast<int>(target.NetworkId());
        PendingWUntil = Now() + 4300;
        return true;
    }
    return false;
}

inline bool CastE(const AIHeroClient& target,
                  Mode mode,
                  bool defensive) {
    if (!CanUse(2, mode, defensive) ||
        !CastThrottlePassed(LastECastTick, 70)) return false;
    Vector3 destination{};
    if (!ShiftPlan(target, defensive, &destination)) return false;
    if (Engine::ControllerCastPosition(2, destination)) {
        LastECastTick = Now();
        return true;
    }
    return false;
}

inline bool CastR(const AIHeroClient& target, bool manual) {
    if (!Engine::RuntimeSpells[3] ||
        !Engine::RuntimeSpells[3]->IsReady() ||
        !Engine::ValidEnemy(target, 20000.0f) ||
        !CastThrottlePassed(LastRCastTick, 180)) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(
        3, target, SDK::HitChance::VeryHigh, false, &prediction);
    const auto player = GameObjects::Player();
    BarrageContext context{};
    context.Manual = manual;
    context.Lethal = SpellDamage(3, target) >=
        target.Health() + target.AllShield();
    context.PredictionVeryHigh = hit;
    context.ProjectileWall =
        PredictionProjectileWall(3, prediction, 160.0f);
    context.LocalEnemyNearby = ControllerHelpers::HasEnemyChampionNear(850.0f);
    context.BetterLocalAction = ClearQPrediction(target) &&
        player.Position().Distance2D(target.Position()) <= 1200.0f;
    context.Distance = GameObjects::Player().Position().Distance2D(
        target.Position());
    if (!ShouldCastBarrage(context)) return false;
    if (Engine::ControllerCastPosition(3, prediction.GetCastPosition())) {
        LastRCastTick = Now();
        return true;
    }
    return false;
}

inline bool TryManualR(const AIHeroClient& preferred) {
    if (!ManualUltimatePressed()) return false;
    const auto selected = ControllerHelpers::PlayerSelectedEnemy(20000.0f);
    const auto target = SelectSmartTarget(
        selected.IsValid() ? selected : preferred,
        Mode::Automatic, true);
    return Engine::ValidEnemy(target) && CastR(target, true);
}

inline bool TryAntiGapcloser() {
    if (GapcloserExpireTick < Now()) return false;
    const auto target = ControllerHelpers::HeroByNetworkId(GapcloserTargetId);
    return Engine::ValidEnemy(target, 950.0f) &&
           CastE(target, Mode::Automatic, true);
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    auto target = SelectSmartTarget(preferred, Mode::Automatic, false);
    if (Engine::ValidEnemy(target)) {
        if (SpellDamage(0, target) >= target.Health() + target.AllShield() &&
            CastQ(target, Mode::Automatic)) return true;
        if (SpellDamage(2, target) >= target.Health() + target.AllShield() &&
            CastE(target, Mode::Automatic, false)) return true;
    }
    target = SelectSmartTarget(preferred, Mode::Automatic, true);
    return Engine::ValidEnemy(target) && CastR(target, false);
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (HasFlux(target) ||
        (LastWTargetId == static_cast<int>(target.NetworkId()) &&
         Now() <= PendingWUntil)) {
        if (CastQ(target, mode)) return true;
    }
    if (CastW(target, mode)) return true;
    if (CastQ(target, mode)) return true;
    return CastE(target, mode, false);
}

inline bool TryFlee(const AIHeroClient& preferred) {
    const auto target = ControllerHelpers::NearestEnemyToPlayer(preferred, 950.0f);
    return Engine::ValidEnemy(target) && CastE(target, Mode::Flee, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    RefreshOrbwalkerFocus(mode, preferred);
    if (TryManualR(preferred)) return true;
    if (TryAntiGapcloser()) return true;
    if (TryKillSecure(preferred)) return true;
    if (mode == Mode::Flee) return TryFlee(preferred);
    if (mode == Mode::Combo || mode == Mode::Harass) {
        const auto target = SelectSmartTarget(preferred, mode, false);
        return TryCombat(target, mode);
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) {
        return Engine::TryFarm(mode);
    }
    return false;
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (args.IsAutoAttack) return;
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        SpellEventNameContainsAny(args, {"ezrealq", "mysticshot"})) {
        LastQCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               SpellEventNameContainsAny(args, {"ezrealw", "essenceflux"})) {
        LastWCastTick = now;
        LastWTargetId = static_cast<int>(args.TargetNetworkId);
        PendingWUntil = now + 4300;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               SpellEventNameContainsAny(args, {"ezreale", "arcaneshift"})) {
        LastECastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               SpellEventNameContainsAny(args, {"ezrealr", "trueshot"})) {
        LastRCastTick = now;
    }
}


inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    const auto focus = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 850.0f);
    if (!focus.IsValid() || !HasFlux(focus) ||
        !RedirectBeforeAttackToFocus(args, focus)) {
        ClearTemporaryOrbwalkerFocus(
            OwnedFocusTargetId, OwnedFocusUntil);
    }
}


inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "EzrealMechanics", "Ezreal Mechanics"));
    ComboLogicMenu = TacticsMenu->AddSubMenu(new Menu(
        "FluxChains", "W Detonation"));
    ComboLogicMenu->Add(new MenuSeparator(
        "RequireDetonation", "W always requires a Q/AA detonation route"));
    ShiftMenu = TacticsMenu->AddSubMenu(new Menu(
        "ShiftLogic", "Arcane Shift"));
    ShiftMenu->Add(new MenuSeparator(
        "DefensiveFirst", "E is defensive or confirmed-lethal only"));
    UltimateMenu = TacticsMenu->AddSubMenu(new Menu(
        "BarrageLogic", "Trueshot Barrage"));
    UltimateMenu->Add(new MenuSeparator(
        "NoLocalR", "R is blocked while a local fight is active"));
}

inline void OnLoad() {
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    LastWTargetId = PendingWUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    LastSmartTarget = {};
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    TacticsMenu = ComboLogicMenu = ShiftMenu = UltimateMenu = nullptr;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Reject any selected target whose Q is blocked and has no alternate route",
    "Prefer the player's selected target only while it remains truly reachable",
    "Spam Q on the first legal cooldown frame without cancelling an auto",
    "Fire Q immediately in the after-attack window",
    "Treat minion, champion and projectile-wall collision as a failed Q route",
    "Do not spend W unless Q or an auto can detonate the mark",
    "Force a reachable W-marked enemy as the orbwalker AA target",
    "Release owned orbwalker focus immediately after W is detonated",
    "Detonate an observed W mark before opening a new sequence",
    "Avoid repeatedly applying W while the current mark is active",
    "Use E defensively against a committed gapcloser",
    "Use offensive E only for a confirmed lethal sequence",
    "Reject E landing in walls, multi-enemy danger or point-click lockdown",
    "Require an E landing to retain an AA or Q follow-up route",
    "Prefer local Q over channeling R",
    "Reject automatic R while a local enemy can punish the channel",
    "Require very-high R prediction and a clear projectile-wall path",
    "Allow manual R after reach, prediction and local-action checks",
    "Use Q for last hit/clear through the shared health prediction path",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Ezreal";
    controller.ControllerId = "champion.kuroaio.ai.ezreal.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIEzreal.md";
    controller.ImplementationSummary =
        "20 ms collision-aware Q loop with AA preservation; W-Q/AA mark "
        "detonation state; lethal-only offensive and threat-aware defensive E; "
        "very-high-confidence isolated R; route-scored target reselection.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &ObserveLocalSpell;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack =
        &CaptureAfterAttackAndReleaseOwnedFocusEvent<
            &LastAfterAttackTargetId, &LastAfterAttackTick,
            &OwnedFocusTargetId, &OwnedFocusUntil>;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 720, 850>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Ezreal
