#pragma once

#include "../AIChampionEngine.h"
#include "../AIMarksmanControllerHelpers.h"
#include "AITwitchGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>

namespace Plugins::KuroAIO::AI::Controllers::Twitch {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::Now;

inline Menu* TacticsMenu = nullptr;
inline Menu* VenomMenu = nullptr;
inline Menu* AmbushMenu = nullptr;
inline Menu* SprayMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int IncomingDamageUntil = 0;
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline Mode LastMode = Mode::None;
inline AIHeroClient LastSmartTarget = {};

inline Vec2 ToVec2(const Vector3& value) {
    return {value.x, value.z};
}

inline int PoisonStacks(const AIBaseClient& target) {
    return target.IsValid()
        ? VenomStacks(target.GetBuffCount("TwitchDeadlyVenom"))
        : 0;
}

inline float PoisonRemainingMs(const AIBaseClient& target) {
    return BuffRemainingMs(target, "TwitchDeadlyVenom");
}

inline bool Hidden() {
    const auto player = GameObjects::Player();
    return player.IsValid() && player.HasBuff("TwitchHideInShadows");
}

inline bool SprayActive() {
    const auto player = GameObjects::Player();
    return player.IsValid() && player.HasBuff("TwitchUlt");
}

inline float ContaminateDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const int rank = ControllerHelpers::SpellRank(2);
    const int stacks = PoisonStacks(target);
    const float physical = ContaminatePhysicalRaw(
        rank, stacks, player.BonusAttackDamage());
    const float magical = ContaminateMagicRaw(stacks, player.AP());
    return SDK::Damage::CalculateMixedDamage(
        player, target, physical, magical);
}

inline bool SafeAdditionalAuto(const AIHeroClient& target) {
    return LocalAttackReadySoon(target, 260) &&
           !IsEscaping(target, 0.45f) &&
           target.Health() + target.AllShield() > AutoDamage(target);
}

inline bool EscapingContaminateRange(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return false;
    const float distance = player.Position().Distance2D(target.Position());
    const Vector3 predicted = ControllerHelpers::PredictPosition(target, 0.60f);
    return distance >= 1050.0f && predicted.IsValid() &&
        player.Position().Distance2D(predicted) >= 1190.0f;
}

inline bool CastQ(const AIHeroClient& target,
                  Mode mode,
                  bool flee) {
    if (!CanUse(0, mode, flee) ||
        !CastThrottlePassed(LastQCastTick, 90)) return false;
    const auto player = GameObjects::Player();
    const float distance = Engine::ValidEnemy(target)
        ? player.Position().Distance2D(target.Position()) : FLT_MAX;
    const float attackRange = Engine::ValidEnemy(target)
        ? ControllerHelpers::AutoAttackRange(target) : 0.0f;
    const bool approach = Engine::ValidEnemy(target, 1150.0f) &&
        distance > attackRange + 70.0f &&
        distance <= 1100.0f &&
        Engine::CountEnemiesAt(player.Position(), 450.0f) == 0;
    const bool fleeWindow = flee &&
        Engine::CountEnemiesAt(player.Position(), 375.0f) == 0;
    if (!ShouldAmbush(
            true, Hidden(), IncomingDamageUntil >= Now(),
            approach, fleeWindow)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    LastQCastTick = Now();
    return true;
}

inline CaskContext BuildCaskContext(
    const AIHeroClient& target,
    Mode mode,
    bool flee,
    bool predictionHits,
    bool projectileWall) {
    CaskContext context{};
    context.PredictionHits = predictionHits;
    context.ProjectileWall = projectileWall;
    context.AttackAvailable = LocalAttackReadySoon(target, 290);
    context.TargetEscaping = IsEscaping(target, 0.45f);
    context.Immobilized = IsImmobile(target);
    context.Flee = flee;
    context.AddsFirstStack = PoisonStacks(target) == 0 &&
        !InAutoAttackRange(target);
    context.ContaminateLethal = PoisonStacks(target) > 0 &&
        CanUse(2, mode) && ContaminateDamage(target) >=
            target.Health() + target.AllShield();
    return context;
}

inline bool CaskResourceOkay(const AIHeroClient& target, bool flee) {
    return flee || !ControllerHelpers::Ready(2) || PoisonStacks(target) <= 0 ||
           ControllerHelpers::HasResourceFor({1, 2});
}

inline bool CastW(const AIHeroClient& target,
                  Mode mode,
                  bool flee) {
    if (!CanUse(1, mode, flee) || !Engine::ValidEnemy(target, 990.0f) ||
        !CastThrottlePassed(LastWCastTick, 45)) return false;
    SDK::PredictionOutput prediction{};
    const bool hit = PredictionHits(
        1, target, SDK::HitChance::High, false, &prediction);
    const bool projectileWall =
        PredictionProjectileWall(1, prediction, 75.0f);
    const auto context = BuildCaskContext(
        target, mode, flee, hit, projectileWall);
    if (!CaskResourceOkay(target, flee)) return false;
    if (!ShouldThrowCask(context) ||
        !Engine::ControllerCastPosition(1, prediction.GetCastPosition())) {
        return false;
    }
    LastWCastTick = Now();
    return true;
}

inline ContaminateContext BuildContaminateContext(
    const AIHeroClient& target) {
    ContaminateContext context{};
    context.Stacks = PoisonStacks(target);
    context.Lethal = ContaminateDamage(target) >=
        target.Health() + target.AllShield();
    context.EscapingRange = EscapingContaminateRange(target);
    context.SafeAdditionalAuto = SafeAdditionalAuto(target);
    const float remaining = PoisonRemainingMs(target);
    const float expiryWindow = static_cast<float>(std::max(
        520, Orbwalker::AttackCooldownRemaining() +
                 SDK::Game::Ping() + 220));
    context.PoisonExpiring = remaining > 0.0f &&
        remaining <= expiryWindow;
    return context;
}

inline AIHeroClient BestContaminateTarget(Mode mode) {
    if (!CanUse(2, mode) || !Engine::RuntimeSpells[2]) return {};
    const auto player = GameObjects::Player();
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1200.0f) ||
            ControllerHelpers::HasSpellShieldOrImmunity(enemy)) continue;
        const int stacks = PoisonStacks(enemy);
        if (stacks <= 0) continue;
        const auto context = BuildContaminateContext(enemy);
        if (!ShouldContaminate(context)) continue;
        float score = context.Lethal ? 1000.0f : 0.0f;
        score += static_cast<float>(stacks) * 120.0f;
        score -= enemy.HealthPercent();
        if (context.EscapingRange) score += 180.0f;
        if (LastSmartTarget.IsValid() &&
            LastSmartTarget.NetworkId() == enemy.NetworkId()) score += 110.0f;
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    return best;
}

inline bool CastE(Mode mode) {
    if (!CanUse(2, mode) ||
        !CastThrottlePassed(LastECastTick, 35)) return false;
    const auto target = BestContaminateTarget(mode);
    if (!target.IsValid() || !Engine::ControllerCastSelf(2)) return false;
    LastECastTick = Now();
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    return true;
}

inline int CountPiercingTargets(const AIHeroClient& primary) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(primary)) return 0;
    const Vector3 aim = ControllerHelpers::PredictPosition(primary, 0.20f);
    if (!aim.IsValid() || aim.IsZero()) return 0;
    const float lineRange = player.AttackRange() + 600.0f;
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        const Vector3 predicted = ControllerHelpers::PredictPosition(
            enemy, 0.20f);
        if (ControllerHelpers::ProjectileWallBlocksFromPlayer(
                predicted, 0.0f)) continue;
        if (SprayLineContains(
                ToVec2(player.Position()), ToVec2(aim), ToVec2(predicted),
                lineRange, 60.0f + enemy.BoundingRadius())) {
            ++count;
        }
    }
    return count;
}

inline SprayContext BuildSprayContext(const AIHeroClient& target) {
    SprayContext context{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return context;
    const float distance = player.Position().Distance2D(target.Position());
    const float normalRange = ControllerHelpers::AutoAttackRange(target);
    const float extendedRange = normalRange + 300.0f;
    context.Ready = ControllerHelpers::Ready(3);
    context.AlreadyActive = SprayActive();
    context.ProjectileWall = OrbwalkerAttackProjectileBlocked(target);
    context.AttackIntent = distance <= extendedRange &&
        !context.ProjectileWall;
    context.NeedsBonusRange = distance > normalRange &&
        distance <= extendedRange;
    context.LethalAttackWindow =
        AutoDamage(target) * 3.0f + ContaminateDamage(target) >=
            target.Health() + target.AllShield();
    context.PiercingTargets = CountPiercingTargets(target);
    context.MinimumEnemies = Slider(SprayMenu, "MinimumEnemies", 2);
    return context;
}

inline bool CastR(const AIHeroClient& target, Mode mode) {
    if (!CanUse(3, mode, true) || !Engine::ValidEnemy(target) ||
        !CastThrottlePassed(LastRCastTick, 100)) return false;
    const float normalRange = ControllerHelpers::AutoAttackRange(target);
    const float extendedRange = normalRange + 300.0f;
    const auto context = BuildSprayContext(target);
    if (!ShouldSprayAndPray(context) ||
        !Engine::ControllerCastSelf(3)) return false;
    LastRCastTick = Now();
    (void)SetTemporaryOrbwalkerFocus(
        target, extendedRange, 1000,
        OwnedFocusTargetId, OwnedFocusUntil);
    return true;
}

inline MarksmanTargeting::TargetContext TargetFacts(
    const AIHeroClient& target,
    Mode mode) {
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const bool attack = OrbwalkerAttackRoute(target);
    const bool activeSprayAttack = SprayActive() &&
        OrbwalkerAttackRoute(target);
    const bool spraySetup = CanUse(3, mode, true) &&
        ShouldSprayAndPray(BuildSprayContext(target));
    const bool extendedAttack = activeSprayAttack || spraySetup;
    SDK::PredictionOutput wPrediction{};
    const bool wHit = CanUse(1, mode) && distance <= 990.0f &&
        PredictionHits(1, target, SDK::HitChance::High, false, &wPrediction);
    const bool wWall = wHit &&
        PredictionProjectileWall(1, wPrediction, 75.0f);
    const bool w = wHit && CaskResourceOkay(target, false) &&
        ShouldThrowCask(BuildCaskContext(
            target, mode, false, wHit, wWall));
    const bool e = CanUse(2, mode) && distance <= 1200.0f &&
        !ControllerHelpers::HasSpellShieldOrImmunity(target) &&
        ShouldContaminate(BuildContaminateContext(target));
    const float attackRange = ControllerHelpers::AutoAttackRange(target);
    const bool approach = distance > attackRange + 70.0f &&
        distance <= 1100.0f &&
        Engine::CountEnemiesAt(player.Position(), 450.0f) == 0;
    const bool q = CanUse(0, mode) && ShouldAmbush(
        true, Hidden(), IncomingDamageUntil >= Now(), approach, false);
    const std::array<bool, 4> reachable = {false, w, e, false};
    const int expectedAutos = (attack || extendedAttack)
        ? (spraySetup ? 3 : 2) : 0;
    auto context = BaseTargetContext(target, EstimatedDamage(
        target, reachable, expectedAutos));
    context.AutoReachable = attack;
    context.SetupReachable = extendedAttack || q || w;
    context.DirectSpellReachable = e;
    context.ProjectileBlocked = !attack && !extendedAttack && !q && !w && !e &&
        (OrbwalkerAttackProjectileBlocked(target) || wWall);
    if (PoisonStacks(target) > 0) {
        context.Priority += static_cast<float>(PoisonStacks(target)) * 55.0f;
    }
    return context;
}

inline AIHeroClient SelectSmartTarget(const AIHeroClient& preferred,
                                      Mode mode) {
    LastSmartTarget = ControllerHelpers::SelectReachableEnemy(
        preferred, 1210.0f,
        [mode](const AIHeroClient& enemy) {
            return TargetFacts(enemy, mode);
        });
    return LastSmartTarget;
}

inline bool ImmediateAttackKill(const AIHeroClient& target) {
    return ImmediateAttackKillRoute(target);
}

inline AIHeroClient ProtectedImmediateAttackKill() {
    const auto selected = ControllerHelpers::PlayerSelectedEnemy(950.0f);
    if (ImmediateAttackKill(selected)) return selected;
    const auto orbTarget = ControllerHelpers::OrbwalkerHeroTarget(950.0f);
    return ImmediateAttackKill(orbTarget) ? orbTarget : AIHeroClient{};
}

inline void RefreshVenomFocus(Mode mode,
                              const AIHeroClient& preferred) {
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    auto protectedKill = ProtectedImmediateAttackKill();
    if (!protectedKill.IsValid() && ImmediateAttackKill(preferred)) {
        protectedKill = preferred;
    }
    auto owned = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 950.0f);
    if (!combat || !owned.IsValid() || PoisonStacks(owned) <= 0 ||
        PoisonStacks(owned) >= 6 ||
        !InAutoAttackRange(owned) ||
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
        if (!Engine::ValidEnemy(enemy, 950.0f)) continue;
        const int stacks = PoisonStacks(enemy);
        const bool protectsDifferentKill = protectedKill.IsValid() &&
            protectedKill.NetworkId() != enemy.NetworkId();
        if (!ShouldMaintainVenomFocus(
                stacks, InAutoAttackRange(enemy), protectsDifferentKill)) {
            continue;
        }
        float score = static_cast<float>(stacks) * 145.0f -
            enemy.HealthPercent();
        if (preferred.IsValid() &&
            preferred.NetworkId() == enemy.NetworkId()) score += 190.0f;
        if (ContaminateDamage(enemy) >=
            enemy.Health() + enemy.AllShield()) score += 350.0f;
        if (AutoDamage(enemy) >= enemy.Health() + enemy.AllShield()) {
            score += 520.0f;
        }
        const float remaining = PoisonRemainingMs(enemy);
        if (remaining > 0.0f && remaining <= 900.0f) {
            score += 300.0f + (900.0f - remaining) * 0.20f;
        }
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    if (best.IsValid()) {
        const float range = ControllerHelpers::AutoAttackRange(best);
        (void)SetTemporaryOrbwalkerFocus(
            best, range, 850, OwnedFocusTargetId, OwnedFocusUntil);
    }
}

inline bool TryAntiGapcloser() {
    if (GapcloserExpireTick < Now()) return false;
    const auto target = ControllerHelpers::HeroByNetworkId(GapcloserTargetId);
    return Engine::ValidEnemy(target, 990.0f) &&
           CastW(target, Mode::Flee, true);
}

inline bool TryKillSecure() {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    return CastE(Mode::Automatic);
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastE(mode)) return true;
    if (mode == Mode::Combo && CastR(target, mode)) return true;
    if (CastW(target, mode, false)) return true;
    return CastQ(target, mode, false);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    if (!combat) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    }
    if (TryAntiGapcloser()) return true;
    if (TryKillSecure()) return true;
    if (combat) {
        const auto target = SelectSmartTarget(preferred, mode);
        RefreshVenomFocus(mode, target);
        return TryCombat(target, mode);
    }
    if (mode == Mode::Flee) {
        const auto threat = ControllerHelpers::NearestEnemyToPlayer(
            preferred, 990.0f);
        if (Engine::ValidEnemy(threat) &&
            CastW(threat, Mode::Flee, true)) return true;
        return CastQ(threat, Mode::Flee, true);
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return Engine::TryFarm(mode);
    return false;
}

inline void ObserveSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (ControllerHelpers::IsLocalPlayer(args.Sender)) {
        if (args.IsAutoAttack) return;
        const int now = Now();
        if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
            ControllerHelpers::SpellEventNameContainsAny(
                args, {"twitchhideinshadows"})) {
            LastQCastTick = now;
        } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
                   ControllerHelpers::SpellEventNameContainsAny(
                       args, {"twitchvenomcask"})) {
            LastWCastTick = now;
        } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
                   ControllerHelpers::SpellEventNameContainsAny(
                       args, {"twitchexpunge"})) {
            LastECastTick = now;
        } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
                   ControllerHelpers::SpellEventNameContainsAny(
                       args, {"twitchfullautomatic"})) {
            LastRCastTick = now;
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    const auto player = GameObjects::Player();
    if (analysis.Valid &&
        (analysis.TargetsPlayer || analysis.CrossesPlayer ||
         (analysis.Committed && player.IsValid() &&
          analysis.Enemy.Position().Distance2D(player.Position()) <= 450.0f))) {
        IncomingDamageUntil = std::max(
            IncomingDamageUntil,
            std::max(analysis.CommitmentUntilTick,
                     analysis.LineThreatUntilTick));
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    auto focus = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 950.0f);
    if (focus.IsValid()) {
        if (!RedirectBeforeAttackToFocus(args, focus)) {
            ClearTemporaryOrbwalkerFocus(
                OwnedFocusTargetId, OwnedFocusUntil);
            focus = {};
        }
    }
    if (!focus.IsValid() && args.Target.IsValid() && args.Target.IsHero()) {
        focus = AIHeroClient(args.Target.Handle());
    }
    if (focus.IsValid() && LastMode == Mode::Combo) {
        (void)CastR(focus, LastMode);
    }
}


inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(
        args, GapcloserTargetId, GapcloserEndpoint,
        GapcloserExpireTick, 760.0f, 900);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "TwitchMechanics", "Twitch Mechanics"));
    VenomMenu = TacticsMenu->AddSubMenu(new Menu(
        "VenomLogic", "Deadly Venom / Contaminate"));
    VenomMenu->Add(new MenuSeparator(
        "HoldE", "E waits for lethal, 6 stacks or range loss"));
    AmbushMenu = TacticsMenu->AddSubMenu(new Menu(
        "AmbushLogic", "Ambush Safety"));
    AmbushMenu->Add(new MenuSeparator(
        "Incoming", "Q rejects a known incoming damage window"));
    SprayMenu = TacticsMenu->AddSubMenu(new Menu(
        "SprayLogic", "Spray and Pray"));
    SprayMenu->Add(new MenuSlider(
        "MinimumEnemies", "Minimum targets in the piercing line", 2, 1, 5));
}

inline void OnLoad() {
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    GapcloserTargetId = GapcloserExpireTick = IncomingDamageUntil = 0;
    GapcloserEndpoint = {};
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    TacticsMenu = VenomMenu = AmbushMenu = SprayMenu = nullptr;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Reject targets outside every current attack/W/E/Q-approach route",
    "Calculate Contaminate from the live one-to-six venom stack count",
    "Hold E while another safe attack can add useful venom",
    "Cast E on lethal, six stacks or imminent 1200-range loss",
    "Force a reachable one-to-five-stack venom target through the orbwalker",
    "Redirect BeforeAttack to the owned venom target",
    "Clear focus after each attack, E cast, mode exit and unload",
    "Throw W only when attacks cannot catch the target or while fleeing",
    "Reject Q when a tracked incoming damage window would cancel Ambush",
    "Use Q for a safe approach outside attack range or safe flee window",
    "Activate R before an attack needing +300 range or a multi-target window",
    "Use W immediately against a committed gapcloser",
    "Reject W and R attack plans whose projectile route crosses a wall",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Twitch";
    controller.ControllerId = "champion.kuroaio.ai.twitch.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AITwitch.md";
    controller.ImplementationSummary =
        "Stack-aware mixed E damage, venom orbwalker focus, range-loss execute, "
        "safe Q approach, predictive W peel and BeforeAttack R activation.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &ObserveSpell;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack =
        &CaptureAfterAttackAndReleaseOwnedFocusEvent<
            &LastAfterAttackTargetId, &LastAfterAttackTick,
            &OwnedFocusTargetId, &OwnedFocusUntil>;
    controller.OnGapcloser = &OnGapcloser;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Twitch
