#pragma once

#include "../AIChampionEngine.h"
#include "../AIMarksmanControllerHelpers.h"
#include "AICaitlynGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace Plugins::KuroAIO::AI::Controllers::Caitlyn {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::Now;
using ControllerHelpers::PredictionAtLeast;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::SpellEventNameContainsAny;

inline Menu* TacticsMenu = nullptr;
inline Menu* TrapMenu = nullptr;
inline Menu* NetMenu = nullptr;
inline Menu* UltimateMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastTrapTargetId = 0;
inline int LastNetTargetId = 0;
inline int NetFollowupUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline AIHeroClient LastSmartTarget = {};

inline bool Trapped(const AIBaseClient& target) {
    return target.IsValid() &&
        target.HasBuff("caitlynyordletrapinternal");
}

inline bool HeadshotRangeReady(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Trapped(target) &&
        player.HasBuff("caitlynheadshotrangecheck");
}

inline bool RecentlyAttacked(const AIBaseClient& target,
                             int windowMs = 460) {
    return RecentlyAttackedTarget(
        target, LastAfterAttackTargetId, LastAfterAttackTick, windowMs);
}

inline bool PredictionFor(int index,
                          const AIBaseClient& target,
                          SDK::HitChance chance,
                          bool noCollision,
                          SDK::PredictionOutput& output) {
    return PredictionHits(index, target, chance, noCollision, &output);
}

inline bool RHasChampionBlocker(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return true;
    const Vector3 source = player.Position();
    const Vector3 end = target.Position();
    const float targetDistance = source.Distance2D(end);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            enemy.NetworkId() == target.NetworkId()) {
            continue;
        }
        const auto projection = SharedGeometry::ProjectPointToSegment2D(
            enemy.Position(), source, end);
        const float along = source.Distance2D(projection.Closest);
        if (projection.T > 0.001f && projection.T < 0.999f &&
            along + enemy.BoundingRadius() < targetDistance &&
            projection.Distance <= enemy.BoundingRadius() + 85.0f) {
            return true;
        }
    }
    return false;
}

inline bool ChannelSafe() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
           Engine::CountEnemiesAt(player.Position(), 950.0f) == 0 &&
           !HasReadyPointClickThreatAt(player.Position());
}

inline void RefreshOrbwalkerFocus(Mode mode,
                                  const AIHeroClient& preferred) {
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    auto owned = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, kHeadshotTrapRange);
    if (!combat || !owned.IsValid() || !HeadshotRangeReady(owned)) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
        owned = {};
    }
    if (owned.IsValid()) return;

    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kHeadshotTrapRange) ||
            !HeadshotRangeReady(enemy)) continue;
        float score = 100.0f - enemy.HealthPercent();
        if (preferred.IsValid() &&
            preferred.NetworkId() == enemy.NetworkId()) score += 180.0f;
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    if (best.IsValid()) {
        (void)SetTemporaryOrbwalkerFocus(
            best, kHeadshotTrapRange, 900,
            OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline bool NetPlan(const AIHeroClient& target,
                    bool defensive,
                    SDK::PredictionOutput* output = nullptr,
                    Vector3* landing = nullptr) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, 840.0f) ||
        !Engine::RuntimeSpells[2]) {
        return false;
    }
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionFor(
        2, target, SDK::HitChance::High, true, prediction) &&
        !PredictionProjectileWall(2, prediction, 70.0f);
    const Vector3 recoil = RecoilPosition(
        player.Position(), prediction.GetCastPosition());
    const bool landingSafe = recoil.IsValid() && !recoil.IsZero() &&
        Engine::PositionDangerScore(
            recoil, target, Engine::ResolvedSpecs[2]) > -10000.0f &&
        !HasReadyPointClickThreatAt(recoil);
    const Vector3 future = ControllerHelpers::PredictPosition(target, 0.50f);
    const float aaReach = player.AttackRange() + target.BoundingRadius() + 150.0f;
    const bool stillReachable = future.IsValid() &&
        (future.Distance2D(recoil) <= aaReach ||
         future.Distance2D(recoil) <= Engine::ResolvedSpecs[0].Range);
    const float followup = SpellDamage(2, target) +
                           AutoDamage(target) * 2.0f;
    const bool lethal = followup >= target.Health() + target.AllShield();
    NetContext context{};
    context.PredictionHits = hit;
    context.LandingSafe = landingSafe;
    context.Defensive = defensive;
    context.TargetStillReachable = stillReachable;
    context.LethalFollowup = lethal;
    context.AttackAvailable = LocalAttackAvailable(target);
    if (output) *output = prediction;
    if (landing) *landing = recoil;
    return ShouldCastNet(context);
}

inline bool TrapPlan(const AIHeroClient& target,
                     bool committed,
                     SDK::PredictionOutput* output = nullptr) {
    if (!Engine::ValidEnemy(target, 850.0f) || !Engine::RuntimeSpells[1]) {
        return false;
    }
    SDK::PredictionOutput prediction{};
    const bool predictionHits = PredictionFor(
        1, target, SDK::HitChance::High, false, prediction);
    const bool sameRecent = LastTrapTargetId ==
        static_cast<int>(target.NetworkId()) &&
        Now() - LastWCastTick < Slider(
            TrapMenu, "RepeatMs", 2400);
    TrapContext context{};
    context.InRange = predictionHits;
    context.AmmoReady = Engine::RuntimeSpells[1]->IsReady();
    context.AlreadyTrapped = Trapped(target);
    context.TrapAlreadyNear = sameRecent;
    context.Immobilized = IsImmobile(target);
    context.Dashing = target.IsDashing();
    context.Committed = committed;
    context.NetFollowup = LastNetTargetId ==
        static_cast<int>(target.NetworkId()) && Now() <= NetFollowupUntil;
    if (output) *output = prediction;
    return ShouldPlaceTrap(context);
}

inline MarksmanTargeting::TargetContext TargetFacts(
    const AIHeroClient& target,
    Mode mode) {
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const bool attack = OrbwalkerAttackRoute(target) ||
        (ControllerHelpers::OrbwalkerTargets(target, kHeadshotTrapRange) &&
         HeadshotRangeReady(target) && distance <= kHeadshotTrapRange);

    SDK::PredictionOutput qPrediction{};
    const bool q = CanUse(0, mode) &&
        PredictionFor(0, target, SDK::HitChance::High, false, qPrediction) &&
        !PredictionProjectileWall(0, qPrediction, 60.0f) &&
        distance <= Engine::RuntimeSpells[0]->CurrentRange() + 40.0f;

    SDK::PredictionOutput wPrediction{};
    const bool w = CanUse(1, mode) &&
        TrapPlan(target, IsImmobile(target) || target.IsDashing(), &wPrediction);

    const bool e = CanUse(2, mode) && NetPlan(target, false);
    const float rDamage = SpellDamage(3, target);
    const bool rLethal = rDamage >= target.Health() + target.AllShield();
    const bool r = Engine::RuntimeSpells[3] &&
        Engine::RuntimeSpells[3]->IsReady() &&
        distance <= Engine::RuntimeSpells[3]->CurrentRange() &&
        rLethal && ChannelSafe() && !RHasChampionBlocker(target);

    const std::array<bool, 4> reachable = {q, false, e, r};
    auto context = BaseTargetContext(
        target, EstimatedDamage(target, reachable, attack ? 2 : 0));
    context.AutoReachable = attack;
    context.DirectSpellReachable = q || e;
    context.SetupReachable = w;
    context.ExecuteReachable = r;
    context.ProjectileBlocked = !attack && !e && !r && !q &&
        qPrediction.GetCastPosition().IsValid();
    return context;
}

inline AIHeroClient SelectSmartTarget(const AIHeroClient& preferred,
                                      Mode mode) {
    MarksmanTargeting::TargetEvaluation evaluation{};
    const auto target = ControllerHelpers::SelectReachableEnemy(
        preferred, 2050.0f,
        [mode](const AIHeroClient& enemy) {
            return TargetFacts(enemy, mode);
        },
        &evaluation);
    LastSmartTarget = target;
    return target;
}

inline bool CastTrap(const AIHeroClient& target,
                     Mode mode,
                     bool committed) {
    if (!CanUse(1, mode) ||
        !CastThrottlePassed(LastWCastTick, 45)) return false;
    SDK::PredictionOutput prediction{};
    if (!TrapPlan(target, committed, &prediction)) return false;
    if (Engine::ControllerCastPosition(1, prediction.GetCastPosition())) {
        LastWCastTick = Now();
        LastTrapTargetId = static_cast<int>(target.NetworkId());
        return true;
    }
    return false;
}

inline bool CastNet(const AIHeroClient& target,
                    Mode mode,
                    bool defensive) {
    if (!CanUse(2, mode, defensive) ||
        !CastThrottlePassed(LastECastTick, 45)) return false;
    SDK::PredictionOutput prediction{};
    if (!NetPlan(target, defensive, &prediction)) return false;
    if (Engine::ControllerCastPosition(2, prediction.GetCastPosition())) {
        LastECastTick = Now();
        LastNetTargetId = static_cast<int>(target.NetworkId());
        NetFollowupUntil = Now() + 850;
        return true;
    }
    return false;
}

inline bool CastQ(const AIHeroClient& target, Mode mode) {
    if (!CanUse(0, mode) || !CastThrottlePassed(LastQCastTick, 24) ||
        !Engine::ValidEnemy(target, 1290.0f)) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionFor(
        0, target, SDK::HitChance::High, false, prediction);
    PeacemakerContext context{};
    context.InRange = hit;
    context.PredictionHits = hit;
    context.ProjectileWall =
        PredictionProjectileWall(0, prediction, 60.0f);
    context.AttackAvailable = LocalAttackAvailable(target);
    context.AttackWindingUp = Orbwalker::IsWindingUp();
    context.TargetImmobile = IsImmobile(target) || Trapped(target);
    context.Lethal = SpellDamage(0, target) >=
        target.Health() + target.AllShield();
    if (!ShouldCastPeacemaker(context)) return false;
    if (Engine::ControllerCastPosition(0, prediction.GetCastPosition())) {
        LastQCastTick = Now();
        return true;
    }
    return false;
}

inline bool CastR(const AIHeroClient& target, bool manual) {
    if (!Engine::RuntimeSpells[3] ||
        !Engine::RuntimeSpells[3]->IsReady() ||
        !CastThrottlePassed(LastRCastTick, 180) ||
        !Engine::ValidEnemy(target,
            Engine::RuntimeSpells[3]->CurrentRange())) {
        return false;
    }
    const float rDamage = SpellDamage(3, target);
    UltimateContext context{};
    context.Manual = manual;
    context.Lethal = rDamage >= target.Health() + target.AllShield();
    context.InRange = true;
    context.ChannelSafe = ChannelSafe();
    context.TargetCanBeDamaged = !IsCommonUntargetableOrImmune(target);
    context.BetterLocalAction = InAutoAttackRange(target, 100.0f) ||
        (CanUse(0, Mode::Combo) &&
         SpellDamage(0, target) >= target.Health() + target.AllShield());
    context.ProjectileWall = RHasChampionBlocker(target) ||
        ProjectileWallBlocksFromPlayer(target.Position(), 85.0f);
    if (!ShouldCastUltimate(context)) return false;
    if (Engine::ControllerCastUnit(3, target)) {
        LastRCastTick = Now();
        return true;
    }
    return false;
}

inline bool TryAntiGapcloser() {
    if (GapcloserExpireTick < Now()) return false;
    const auto target = ControllerHelpers::HeroByNetworkId(GapcloserTargetId);
    if (!Engine::ValidEnemy(target, 900.0f)) return false;
    if (CastTrap(target, Mode::Automatic, true)) return true;
    return CastNet(target, Mode::Automatic, true);
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    const auto target = SelectSmartTarget(preferred, Mode::Automatic);
    if (!Engine::ValidEnemy(target)) return false;
    if (SpellDamage(0, target) >= target.Health() + target.AllShield() &&
        CastQ(target, Mode::Automatic)) return true;
    if (SpellDamage(2, target) + AutoDamage(target) >=
            target.Health() + target.AllShield() &&
        CastNet(target, Mode::Automatic, false)) return true;
    return CastR(target, false);
}

inline bool TryManualR(const AIHeroClient& preferred) {
    if (!ManualUltimatePressed()) return false;
    const auto target = SelectSmartTarget(preferred, Mode::Automatic);
    return Engine::ValidEnemy(target) && CastR(target, true);
}

inline bool TryCombo(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    const bool committed = IsImmobile(target) || target.IsDashing() ||
        (IsEscaping(target) && InAutoAttackRange(target, 125.0f));
    if (CastTrap(target, mode, committed)) return true;
    if (CastNet(target, mode, false)) return true;
    if (CastQ(target, mode)) return true;
    return false;
}

inline bool TryFlee(const AIHeroClient& preferred) {
    const auto target = ControllerHelpers::NearestEnemyToPlayer(preferred, 900.0f);
    if (!Engine::ValidEnemy(target)) return false;
    if (CastTrap(target, Mode::Flee, true)) return true;
    return CastNet(target, Mode::Flee, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    RefreshOrbwalkerFocus(mode, preferred);
    if (TryManualR(preferred)) return true;
    if (TryAntiGapcloser()) return true;
    if (TryKillSecure(preferred)) return true;
    if (mode == Mode::Flee) return TryFlee(preferred);
    if (mode == Mode::Combo || mode == Mode::Harass) {
        const auto target = SelectSmartTarget(preferred, mode);
        return TryCombo(target, mode);
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
        SpellEventNameContainsAny(args, {"caitlynpiltoverpeacemaker"})) {
        LastQCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               SpellEventNameContainsAny(args, {"caitlynyordletrap"})) {
        LastWCastTick = now;
        LastTrapTargetId = static_cast<int>(args.TargetNetworkId);
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               SpellEventNameContainsAny(args, {"caitlynentrapment"})) {
        LastECastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               SpellEventNameContainsAny(args, {"caitlynaceinthehole"})) {
        LastRCastTick = now;
    }
}


inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    const auto focus = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, kHeadshotTrapRange);
    if (!focus.IsValid() || !HeadshotRangeReady(focus) ||
        !RedirectBeforeAttackToFocus(args, focus, 700.0f)) {
        ClearTemporaryOrbwalkerFocus(
            OwnedFocusTargetId, OwnedFocusUntil);
    }
}


inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "CaitlynMechanics", "Caitlyn Mechanics"));
    TrapMenu = TacticsMenu->AddSubMenu(new Menu("TrapLogic", "Trap Logic"));
    TrapMenu->Add(new MenuSlider(
        "RepeatMs", "Do not repeat W on same target (ms)",
        2400, 1000, 4000));
    NetMenu = TacticsMenu->AddSubMenu(new Menu("NetLogic", "Net/Recoil Logic"));
    NetMenu->Add(new MenuSeparator(
        "SafeLanding", "E always requires a safe recoil landing"));
    UltimateMenu = TacticsMenu->AddSubMenu(new Menu(
        "AceLogic", "Ace in the Hole"));
    UltimateMenu->Add(new MenuSeparator(
        "CleanChannel", "R always requires an isolated safe channel"));
}

inline void OnLoad() {
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    LastTrapTargetId = LastNetTargetId = NetFollowupUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    LastSmartTarget = {};
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    TacticsMenu = TrapMenu = NetMenu = UltimateMenu = nullptr;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Reject selected targets outside every current AA/Q/W/E/R route",
    "Keep selected-target priority only when that target is reachable",
    "Prefer trapped headshot reach up to the observed range-check state",
    "Temporarily force trapped headshot targets through the orbwalker",
    "Release owned orbwalker focus immediately after the headshot attack",
    "Do not cast Q during an attack windup or over a ready normal attack",
    "Allow lethal or immobilized Q to consume genuine attack downtime",
    "Place W on immobile, dashing, committed or freshly netted targets",
    "Suppress repeated W casts on the same target during trap arm time",
    "Reject E when minions, champions or projectile walls block the net",
    "Reject E when recoil lands in wall or point-click lockdown range",
    "Require E recoil to retain AA/Q follow-up unless it is defensive",
    "Use W then E against a committed gapcloser before ordinary damage",
    "Reject R while any local attack/Q action is better",
    "Reject R when another champion can intercept the shot",
    "Reject R while a nearby enemy can punish the channel",
    "Allow manual R only after the same reach and safety validation",
    "Run Q/W/E decisions at cooldown pace with 20-45 ms local throttles",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Caitlyn";
    controller.ControllerId = "champion.kuroaio.ai.caitlyn.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AICaitlyn.md";
    controller.ImplementationSummary =
        "Attack-preserving Q; commitment/spacing trap planner; collision-aware "
        "E with recoil landing danger and retained follow-up; trapped-headshot "
        "reach; body-block and channel-safe R execute; reachable target rescoring.";
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
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 760, 900>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Caitlyn
