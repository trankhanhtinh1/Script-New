#pragma once

#include "../AIChampionEngine.h"
#include "../AIMarksmanControllerHelpers.h"
#include "AITristanaGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>

namespace Plugins::KuroAIO::AI::Controllers::Tristana {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::Now;

inline Menu* TacticsMenu = nullptr;
inline Menu* ChargeMenu = nullptr;
inline Menu* JumpMenu = nullptr;
inline Menu* BusterMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int PendingRapidFireTargetId = 0;
inline int PendingRapidFireUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline Mode LastMode = Mode::None;
inline AIHeroClient LastSmartTarget = {};

inline int ExplosiveChargeStacks(const AIBaseClient& target) {
    return target.IsValid() && target.HasBuff("TristanaECharge")
        ? ChargeStacks(target.GetBuffCount("TristanaECharge"))
        : 0;
}

inline bool HasExplosiveCharge(const AIBaseClient& target) {
    return target.IsValid() && target.HasBuff("TristanaECharge");
}

inline float ExplosiveChargeRemainingMs(const AIBaseClient& target) {
    return BuffRemainingMs(target, "TristanaECharge");
}

inline float ExplosiveChargeDamage(const AIBaseClient& target,
                                   int bonusStacks = 0) {
    const auto player = GameObjects::Player();
    const int rank = ControllerHelpers::SpellRank(2);
    if (!player.IsValid() || !target.IsValid() || rank <= 0 ||
        !HasExplosiveCharge(target)) {
        return 0.0f;
    }
    const int stacks = ChargeStacks(
        ExplosiveChargeStacks(target) + std::max(0, bonusStacks));
    const float critMultiplier = std::max(
        1.0f, ::CoreAIHeroClient::CritDamageMultiplier(player.Address()));
    const float raw = ExplosiveChargeRaw(
        rank, stacks, player.BonusAttackDamage(), player.AP(),
        player.Crit(), critMultiplier);
    return player.CalculatePhysicalDamage(target, raw);
}

inline float TargetedRange() {
    const auto player = GameObjects::Player();
    return player.IsValid()
        ? DynamicTargetedRange(player.AttackRange())
        : 550.0f;
}

inline void RefreshDynamicRanges() {
    // Spell::CastOnUnit performs a center-distance check, while the live game
    // data marks E/R as using bounding boxes.  Keep a neutral display/default
    // allowance here; each actual cast below installs the exact target radius.
    const float range = TargetedRange() + 65.0f;
    if (Engine::RuntimeSpells[2]) Engine::RuntimeSpells[2]->Range = range;
    if (Engine::RuntimeSpells[3]) Engine::RuntimeSpells[3]->Range = range;
}

inline void PrepareTargetedCastRange(int index,
                                     const AIBaseClient& target) {
    if (index < 0 || index >= 4 || !Engine::RuntimeSpells[index] ||
        !target.IsValid()) return;
    Engine::RuntimeSpells[index]->Range =
        TargetedRange() + std::max(0.0f, target.BoundingRadius()) + 2.0f;
}

inline bool TargetedSpellReach(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid() &&
        player.Position().Distance2D(target.Position()) <=
            TargetedRange() + target.BoundingRadius();
}

inline bool CastQ(const AIHeroClient& target, Mode mode) {
    if (!CanUse(0, mode, true) || !Engine::ValidEnemy(target) ||
        !InAutoAttackRange(target) ||
        !CastThrottlePassed(LastQCastTick, 75)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    LastQCastTick = Now();
    PendingRapidFireTargetId = PendingRapidFireUntil = 0;
    return true;
}

inline bool CastE(const AIHeroClient& target,
                  Mode mode,
                  bool attackIntent) {
    if (!CanUse(2, mode, true) || !TargetedSpellReach(target) ||
        ControllerHelpers::HasSpellShieldOrImmunity(target) ||
        !CastThrottlePassed(LastECastTick, 80)) return false;
    const bool projectileWall = TargetProjectileWall(2, target, 45.0f);
    if (!ShouldCastExplosiveCharge(
            true, true, attackIntent, HasExplosiveCharge(target),
            projectileWall)) {
        return false;
    }
    PrepareTargetedCastRange(2, target);
    if (!Engine::ControllerCastUnit(2, target)) return false;
    LastECastTick = Now();
    PendingRapidFireTargetId = static_cast<int>(target.NetworkId());
    PendingRapidFireUntil = Now() + 420;
    (void)SetTemporaryOrbwalkerFocus(
        target, ControllerHelpers::AutoAttackRange(target), 1000,
        OwnedFocusTargetId, OwnedFocusUntil);
    return true;
}

inline bool SafeJumpLanding(const Vector3& position,
                            const AIHeroClient& target,
                            int maximumEnemies) {
    if (!position.IsValid() || position.IsZero() ||
        SDK::NavMesh::IsWall(position) ||
        Engine::CountEnemiesAt(position, 625.0f) > maximumEnemies ||
        HasReadyPointClickThreatAt(position) ||
        HasReadyDashHazardAt(position)) return false;
    return Engine::PositionDangerScore(
         position, target, Engine::ResolvedSpecs[1]) > -10000.0f;
}

inline JumpContext BuildJumpContext(const AIHeroClient& target,
                                    const SDK::PredictionOutput& prediction,
                                    int maximumEnemies,
                                    bool flee = false) {
    JumpContext context{};
    const Vector3 landing = prediction.GetCastPosition();
    context.PredictionHits = ControllerHelpers::PredictionAtLeast(
        prediction, SDK::HitChance::High);
    context.LandingSafe = SafeJumpLanding(
        landing, target, maximumEnemies);
    const bool detonatesCharge = HasExplosiveCharge(target) &&
        WillDetonateCharge(ExplosiveChargeStacks(target), 1);
    context.Lethal = SpellDamage(1, target) +
        (detonatesCharge
             ? ExplosiveChargeDamage(target, 1) : 0.0f) >=
        target.Health() + target.AllShield();
    context.Flee = flee;
    context.BetterAttack = LocalAttackReadySoon(target, 260);
    context.EnemiesAtLanding = Engine::CountEnemiesAt(landing, 625.0f);
    context.MaximumEnemies = maximumEnemies;
    return context;
}

inline bool CastWOnTarget(const AIHeroClient& target,
                          Mode mode) {
    if (!CanUse(1, mode) || !Engine::ValidEnemy(target, 940.0f) ||
        ControllerHelpers::HasSpellShieldOrImmunity(target) ||
        !CastThrottlePassed(LastWCastTick, 80)) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(
        1, target, SDK::HitChance::High, false, &prediction);
    const Vector3 landing = prediction.GetCastPosition();
    const int maximumEnemies = Slider(JumpMenu, "MaximumEnemies", 1);
    auto context = BuildJumpContext(target, prediction, maximumEnemies);
    context.PredictionHits = context.PredictionHits && hit;
    if (!ShouldRocketJump(context) ||
        !Engine::ControllerCastPosition(1, landing)) return false;
    LastWCastTick = Now();
    return true;
}

inline bool CastWFlee(const AIHeroClient& threat) {
    if (!CanUse(1, Mode::Flee, true) ||
        !CastThrottlePassed(LastWCastTick, 80)) return false;
    const Vector3 landing = Engine::BestSafePosition(
        Engine::ResolvedSpecs[1], threat, AimPolicy::SafeCursor);
    const int maximumEnemies = Slider(JumpMenu, "MaximumEnemies", 1);
    JumpContext context{};
    context.PredictionHits = landing.IsValid() && !landing.IsZero();
    context.LandingSafe = SafeJumpLanding(
        landing, threat, maximumEnemies);
    context.Flee = true;
    context.EnemiesAtLanding = Engine::CountEnemiesAt(landing, 625.0f);
    context.MaximumEnemies = maximumEnemies;
    if (!ShouldRocketJump(context) ||
        !Engine::ControllerCastPosition(1, landing)) return false;
    LastWCastTick = Now();
    return true;
}

inline BusterContext BuildBusterContext(const AIHeroClient& target,
                                        Mode mode,
                                        bool gapcloser) {
    BusterContext context{};
    if (!TargetedSpellReach(target)) return context;
    const float health = target.Health() + target.AllShield();
    const float rDamage = SpellDamage(3, target);
    const bool detonatesCharge = HasExplosiveCharge(target) &&
        WillDetonateCharge(ExplosiveChargeStacks(target), 1);
    const float chargeDamage = detonatesCharge
        ? ExplosiveChargeDamage(target, 1) : 0.0f;
    context.InRange = true;
    context.Lethal = rDamage >= health;
    context.DetonationLethal = detonatesCharge &&
        rDamage + chargeDamage >= health;
    context.Gapcloser = gapcloser;
    context.SelfPeel = gapcloser ||
        (mode == Mode::Flee &&
         GameObjects::Player().Position().Distance2D(target.Position()) <
             475.0f);
    context.AttackAvailable = LocalAttackReadySoon(target, 220);
    context.ProjectileWall = TargetProjectileWall(3, target, 55.0f);
    return context;
}

inline bool CastR(const AIHeroClient& target,
                  Mode mode,
                  bool gapcloser,
                  bool manual) {
    if (!Engine::RuntimeSpells[3] ||
        !Engine::RuntimeSpells[3]->IsReady() ||
        !TargetedSpellReach(target) ||
        ControllerHelpers::HasSpellShieldOrImmunity(target) ||
        !CastThrottlePassed(LastRCastTick, 90)) return false;
    if (!manual && !CanUse(3, mode, gapcloser)) return false;
    const auto context = BuildBusterContext(target, mode, gapcloser);
    if (context.ProjectileWall) return false;
    if (!manual && !ShouldCastBusterShot(context)) return false;
    if (manual && !context.InRange) return false;
    PrepareTargetedCastRange(3, target);
    if (!Engine::ControllerCastUnit(3, target)) return false;
    LastRCastTick = Now();
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    return true;
}

inline MarksmanTargeting::TargetContext TargetFacts(
    const AIHeroClient& target,
    Mode mode) {
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const bool attack = OrbwalkerAttackRoute(target);
    const bool spellShield =
        ControllerHelpers::HasSpellShieldOrImmunity(target);
    const bool eWall = TargetProjectileWall(2, target, 45.0f);
    const bool e = CanUse(2, mode, true) && TargetedSpellReach(target) &&
        attack && !spellShield && !eWall && !HasExplosiveCharge(target);
    SDK::PredictionOutput wPrediction{};
    const bool wReady = CanUse(1, mode) && distance <= 940.0f &&
        PredictionHits(1, target, SDK::HitChance::High, false, &wPrediction);
    const bool w = wReady && !spellShield && ShouldRocketJump(BuildJumpContext(
        target, wPrediction, Slider(JumpMenu, "MaximumEnemies", 1)));
    const bool rReady = CanUse(3, mode) && TargetedSpellReach(target) &&
        !spellShield;
    const auto rContext = BuildBusterContext(target, mode, false);
    const bool r = rReady && ShouldCastBusterShot(rContext);
    const std::array<bool, 4> reachable = {false, w, e, r};
    int expectedAutos = 0;
    if (attack) {
        expectedAutos = HasExplosiveCharge(target)
            ? std::max(1, 4 - ExplosiveChargeStacks(target))
            : (e ? 4 : 2);
    }
    float estimated = AutoDamage(target) *
        static_cast<float>(expectedAutos);
    if (w) estimated += SpellDamage(1, target);
    if (e) estimated += SpellDamage(2, target);
    if (r) {
        const bool detonatesCharge = HasExplosiveCharge(target) &&
            WillDetonateCharge(ExplosiveChargeStacks(target), 1);
        estimated += SpellDamage(3, target) +
            (detonatesCharge
                 ? ExplosiveChargeDamage(target, 1) : 0.0f);
    }
    auto context = BaseTargetContext(target, estimated);
    context.AutoReachable = attack;
    context.SetupReachable = e;
    context.DirectSpellReachable = w;
    context.ExecuteReachable = r;
    context.ProjectileBlocked = !attack && !w && !e && !r &&
        (OrbwalkerAttackProjectileBlocked(target) || eWall ||
         rContext.ProjectileWall);
    if (HasExplosiveCharge(target)) {
        context.Priority += 165.0f +
            static_cast<float>(ExplosiveChargeStacks(target)) * 65.0f;
    }
    return context;
}

inline AIHeroClient SelectSmartTarget(const AIHeroClient& preferred,
                                      Mode mode) {
    LastSmartTarget = ControllerHelpers::SelectReachableEnemy(
        preferred, 950.0f,
        [mode](const AIHeroClient& enemy) {
            return TargetFacts(enemy, mode);
        });
    return LastSmartTarget;
}

inline bool ImmediateAttackKill(const AIHeroClient& target) {
    return ImmediateAttackKillRoute(target);
}

inline AIHeroClient ProtectedImmediateAttackKill() {
    const auto selected = ControllerHelpers::PlayerSelectedEnemy(850.0f);
    if (ImmediateAttackKill(selected)) return selected;
    const auto orbTarget = ControllerHelpers::OrbwalkerHeroTarget(850.0f);
    return ImmediateAttackKill(orbTarget) ? orbTarget : AIHeroClient{};
}

inline void RefreshChargeFocus(Mode mode,
                               const AIHeroClient& preferred) {
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    auto protectedKill = ProtectedImmediateAttackKill();
    if (!protectedKill.IsValid() && ImmediateAttackKill(preferred)) {
        protectedKill = preferred;
    }
    auto owned = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 850.0f);
    if (!combat || !owned.IsValid() || !HasExplosiveCharge(owned) ||
        ExplosiveChargeStacks(owned) >= 4 || !InAutoAttackRange(owned) ||
        (protectedKill.IsValid() &&
         protectedKill.NetworkId() != owned.NetworkId())) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
        owned = {};
    }
    if (owned.IsValid() || !combat) return;
    if (protectedKill.IsValid()) {
        (void)ForceImmediateAttackKill(
            protectedKill, 450, OwnedFocusTargetId, OwnedFocusUntil);
        return;
    }

    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 850.0f) ||
            !InAutoAttackRange(enemy)) continue;
        const int stacks = ExplosiveChargeStacks(enemy);
        const bool protectsDifferentKill = protectedKill.IsValid() &&
            protectedKill.NetworkId() != enemy.NetworkId();
        if (!ShouldFocusCharge(
                HasExplosiveCharge(enemy), stacks, true,
                protectsDifferentKill)) continue;
        const bool oneAutoKills = AutoDamage(enemy) >=
            enemy.Health() + enemy.AllShield();
        float score = static_cast<float>(stacks) * 160.0f -
            enemy.HealthPercent();
        if (oneAutoKills) score += 520.0f;
        const float remaining = ExplosiveChargeRemainingMs(enemy);
        if (remaining > 0.0f && remaining <= 900.0f) {
            score += 360.0f + (900.0f - remaining) * 0.25f;
        }
        if (preferred.IsValid() &&
            preferred.NetworkId() == enemy.NetworkId()) score += 200.0f;
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    if (best.IsValid()) {
        (void)SetTemporaryOrbwalkerFocus(
            best, ControllerHelpers::AutoAttackRange(best), 850,
            OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline bool TryPendingRapidFire() {
    if (PendingRapidFireUntil < Now()) {
        PendingRapidFireTargetId = PendingRapidFireUntil = 0;
        return false;
    }
    const auto target = ControllerHelpers::HeroByNetworkId(
        PendingRapidFireTargetId);
    return Engine::ValidEnemy(target) && CastQ(target, LastMode);
}

inline bool TryAntiGapcloser() {
    if (GapcloserExpireTick < Now()) return false;
    const auto target = ControllerHelpers::HeroByNetworkId(GapcloserTargetId);
    return Engine::ValidEnemy(target) &&
           CastR(target, Mode::Automatic, true, false);
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    const auto target = ControllerHelpers::SelectReachableEnemy(
        preferred, 950.0f,
        [](const AIHeroClient& enemy) {
            auto context = TargetFacts(enemy, Mode::Automatic);
            context.AutoReachable = false;
            context.SetupReachable = false;
            return context;
        });
    if (!Engine::ValidEnemy(target)) return false;
    if (CastR(target, Mode::Automatic, false, false)) return true;
    return CastWOnTarget(target, Mode::Automatic);
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (TryPendingRapidFire()) return true;
    if (CastR(target, mode, false, false)) return true;
    return CastWOnTarget(target, mode);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    RefreshDynamicRanges();
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    if (!combat) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
        PendingRapidFireTargetId = PendingRapidFireUntil = 0;
    }
    if (ManualUltimatePressed()) {
        const auto target = ControllerHelpers::NearestEnemyToPlayer(
            preferred, TargetedRange() + 125.0f);
        if (Engine::ValidEnemy(target) &&
            CastR(target, Mode::Automatic, false, true)) return true;
    }
    if (TryAntiGapcloser()) return true;
    if (TryKillSecure(preferred)) return true;
    if (combat) {
        const auto target = SelectSmartTarget(preferred, mode);
        RefreshChargeFocus(mode, target);
        return TryCombat(target, mode);
    }
    if (mode == Mode::Flee) {
        const auto threat = ControllerHelpers::NearestEnemyToPlayer(
            preferred, 950.0f);
        if (Engine::ValidEnemy(threat) &&
            CastR(threat, Mode::Flee, true, false)) return true;
        return CastWFlee(threat);
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return Engine::TryFarm(mode);
    return false;
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender) || args.IsAutoAttack) {
        return;
    }
    const int now = Now();
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        ControllerHelpers::SpellEventNameContainsAny(args, {"tristanaq"})) {
        LastQCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"tristanaw"})) {
        LastWCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"tristanae"})) {
        LastECastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"tristanar"})) {
        LastRCastTick = now;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    auto focus = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 850.0f);
    if (focus.IsValid() &&
        (!HasExplosiveCharge(focus) ||
         !RedirectBeforeAttackToFocus(args, focus))) {
        ClearTemporaryOrbwalkerFocus(
            OwnedFocusTargetId, OwnedFocusUntil);
        focus = {};
    }
    if (!focus.IsValid() && args.Target.IsValid() && args.Target.IsHero()) {
        focus = AIHeroClient(args.Target.Handle());
    }
    if (!focus.IsValid() ||
        (LastMode != Mode::Combo && LastMode != Mode::Harass)) return;
    if (CastE(focus, LastMode, true)) return;
    (void)CastQ(focus, LastMode);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(
        args, LastAfterAttackTargetId, LastAfterAttackTick);
    if (LastAfterAttackTargetId == OwnedFocusTargetId) {
        ClearTemporaryOrbwalkerFocus(
            OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(
        args, GapcloserTargetId, GapcloserEndpoint,
        GapcloserExpireTick, 700.0f, 900);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "TristanaMechanics", "Tristana Mechanics"));
    ChargeMenu = TacticsMenu->AddSubMenu(new Menu(
        "ExplosiveChargeLogic", "Explosive Charge / Orbwalker"));
    ChargeMenu->Add(new MenuSeparator(
        "Focus", "Focus reachable E targets until detonation"));
    JumpMenu = TacticsMenu->AddSubMenu(new Menu(
        "RocketJumpLogic", "Rocket Jump Safety"));
    JumpMenu->Add(new MenuSlider(
        "MaximumEnemies", "Maximum enemies at W landing", 1, 0, 3));
    BusterMenu = TacticsMenu->AddSubMenu(new Menu(
        "BusterShotLogic", "Buster Shot"));
    BusterMenu->Add(new MenuSeparator(
        "Reserve", "R is reserved for lethal detonation or peel"));
}

inline void OnLoad() {
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    PendingRapidFireTargetId = PendingRapidFireUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    LastMode = Mode::None;
    LastSmartTarget = {};
    RefreshDynamicRanges();
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    PendingRapidFireTargetId = PendingRapidFireUntil = 0;
    TacticsMenu = ChargeMenu = JumpMenu = BusterMenu = nullptr;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Refresh E/R range from current attack range instead of a fixed level table",
    "Cast E on the real orbwalker target in BeforeAttack",
    "Queue Q immediately after an E BeforeAttack cast",
    "Force a reachable one-to-three-stack E target through the orbwalker",
    "Redirect BeforeAttack to the owned charged target",
    "Release focus after each attack and rebuild only while E remains valuable",
    "Reserve R for lethal damage, E detonation or close self-peel",
    "Use R immediately against a committed gapcloser",
    "Reject E, R and forced attacks across a projectile wall",
    "Reject Rocket Jump when a normal attack is better",
    "Reject Rocket Jump into wall, lockdown or excess enemies",
    "Use a safe cursor-side Rocket Jump while fleeing",
    "Clear all forced target state on mode exit and unload",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Tristana";
    controller.ControllerId = "champion.kuroaio.ai.tristana.onetrick";
    controller.KitRevision = "CommunityDragon current / TestOrbwalker port";
    controller.ResearchArtifact =
        "C:/Users/funny/Downloads/TestOrbwalker/TestOrbwalker/AllChampions/Tristana.cs";
    controller.ImplementationSummary =
        "Dynamic attack-linked E/R range, BeforeAttack E-Q sequencing, charged "
        "target focus, safe lethal-only W and detonation/peel R policy.";
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
    controller.OnGapcloser = &OnGapcloser;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Tristana
