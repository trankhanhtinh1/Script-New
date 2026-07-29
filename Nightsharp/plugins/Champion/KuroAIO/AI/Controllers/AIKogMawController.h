#pragma once

#include "../AIChampionEngine.h"
#include "../AIMarksmanControllerHelpers.h"
#include "AIKogMawGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>

namespace Plugins::KuroAIO::AI::Controllers::KogMaw {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::Now;
using ControllerHelpers::PredictionAtLeast;

inline Menu* TacticsMenu = nullptr;
inline Menu* BarrageMenu = nullptr;
inline Menu* ArtilleryMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline Mode LastMode = Mode::None;
inline AIHeroClient LastSmartTarget = {};

inline bool BarrageActive() {
    const auto player = GameObjects::Player();
    return player.IsValid() && player.HasBuff("KogMawBioArcaneBarrage");
}

inline int ArtilleryCostStacks() {
    const auto player = GameObjects::Player();
    return player.IsValid()
        ? std::max(0, player.GetBuffCount("kogmawlivingartillerycost"))
        : 0;
}

inline float FutureAttackReach(const AIBaseClient& target) {
    if (!target.IsValid()) return 0.0f;
    const float bonus = BarrageActive()
        ? 0.0f
        : BarrageBonusRange(ControllerHelpers::SpellRank(1));
    return ControllerHelpers::AutoAttackRange(target, bonus);
}

inline void RefreshDynamicRanges() {
    if (Engine::RuntimeSpells[3]) {
        const float range = ArtilleryRange(ControllerHelpers::SpellRank(3));
        if (range > 0.0f) Engine::RuntimeSpells[3]->Range = range;
    }
}

inline bool PredictionFor(int index,
                          const AIBaseClient& target,
                          SDK::HitChance chance,
                          bool requireNoCollision,
                          SDK::PredictionOutput& output) {
    return PredictionHits(
        index, target, chance, requireNoCollision, &output);
}

inline bool AttackWillReachAfterW(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid() ||
        OrbwalkerAttackProjectileBlocked(target)) return false;
    return player.Position().Distance2D(target.Position()) <=
           FutureAttackReach(target);
}

inline BarrageContext BuildBarrageContext(const AIHeroClient& target,
                                          bool attackIntent) {
    BarrageContext context{};
    context.Ready = ControllerHelpers::Ready(1);
    context.AlreadyActive = BarrageActive();
    context.TargetValid = Engine::ValidEnemy(target);
    context.TargetInEmpoweredRange = AttackWillReachAfterW(target);
    context.AttackIntent = attackIntent;
    context.TargetKillableByAttack = LocalAttackReadySoon(target, 180) &&
        AutoDamage(target) >= target.Health() + target.AllShield();
    return context;
}

inline SpittleContext BuildSpittleContext(
    const AIHeroClient& target,
    const SDK::PredictionOutput& prediction,
    bool predictionHits) {
    SpittleContext context{};
    context.PredictionHits = predictionHits;
    context.Collision = !prediction.CollisionObjects.empty();
    context.ProjectileWall = prediction.GetCastPosition().IsValid() &&
        PredictionProjectileWall(0, prediction, 70.0f);
    context.AttackAvailable = LocalAttackReadySoon(target, 190);
    context.Lethal = SpellDamage(0, target) >=
        target.Health() + target.AllShield();
    context.Immobilized = IsImmobile(target);
    context.OutsideAttackRange = !InAutoAttackRange(target);
    return context;
}

inline OozeContext BuildOozeContext(
    const AIHeroClient& target,
    const SDK::PredictionOutput& prediction,
    bool predictionHits,
    bool gapcloser) {
    OozeContext context{};
    context.PredictionHits = predictionHits;
    context.ProjectileWall = prediction.GetCastPosition().IsValid() &&
        PredictionProjectileWall(2, prediction, 120.0f);
    context.AttackAvailable = LocalAttackReadySoon(target, 240);
    context.Lethal = SpellDamage(2, target) >=
        target.Health() + target.AllShield();
    context.Escaping = IsEscaping(target);
    context.Gapcloser = gapcloser;
    context.Immobilized = IsImmobile(target);
    return context;
}

inline ArtilleryContext BuildArtilleryContext(
    const AIHeroClient& target,
    const SDK::PredictionOutput& prediction,
    bool predictionHits,
    bool manual) {
    ArtilleryContext context{};
    const auto player = GameObjects::Player();
    const float range = Engine::RuntimeSpells[3]
        ? Engine::RuntimeSpells[3]->CurrentRange() : 0.0f;
    context.PredictionVeryHigh = predictionHits;
    context.InRange = player.IsValid() &&
        prediction.GetCastPosition().IsValid() &&
        player.Position().Distance2D(prediction.GetCastPosition()) <= range;
    context.AttackAvailable = LocalAttackReadySoon(target, 230);
    context.Lethal = SpellDamage(3, target) >=
        target.Health() + target.AllShield();
    context.LowHealth = target.HealthPercent() <=
        static_cast<float>(Slider(ArtilleryMenu, "TargetHp", 42));
    context.SlowedOrImmobile = IsImmobile(target) ||
        SDK::HasBuffOfType(target, SDK::BuffType::Slow);
    context.Escaping = IsEscaping(target, 0.60f);
    context.CostStacks = ArtilleryCostStacks();
    context.MaximumStacks = manual
        ? 10 : Slider(ArtilleryMenu, "MaxStacks", 2);
    return context;
}

inline bool CastW(const AIHeroClient& target,
                  Mode mode,
                  bool attackIntent) {
    if (!CanUse(1, mode, true) ||
        !CastThrottlePassed(LastWCastTick, 80)) return false;
    const auto context = BuildBarrageContext(target, attackIntent);
    if (!ShouldActivateBarrage(context)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastWCastTick = Now();
    (void)SetTemporaryOrbwalkerFocus(
        target, FutureAttackReach(target) + 45.0f, 1200,
        OwnedFocusTargetId, OwnedFocusUntil);
    return true;
}

inline bool CastQ(const AIHeroClient& target, Mode mode) {
    if (!CanUse(0, mode) || !Engine::ValidEnemy(target, 1240.0f) ||
        ControllerHelpers::HasSpellShieldOrImmunity(target) ||
        !CastThrottlePassed(LastQCastTick, 35)) return false;
    SDK::PredictionOutput prediction{};
    const bool predictionHits = PredictionFor(
        0, target, SDK::HitChance::High, true, prediction);
    const auto context = BuildSpittleContext(
        target, prediction, predictionHits);
    if (!ShouldCastSpittle(context) ||
        !Engine::ControllerCastPosition(0, prediction.GetCastPosition())) {
        return false;
    }
    LastQCastTick = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target,
                  Mode mode,
                  bool gapcloser) {
    if (!CanUse(2, mode, gapcloser) ||
        !Engine::ValidEnemy(target, 1240.0f) ||
        ControllerHelpers::HasSpellShieldOrImmunity(target) ||
        !CastThrottlePassed(LastECastTick, 40)) return false;
    SDK::PredictionOutput prediction{};
    const bool predictionHits = PredictionFor(
        2, target, SDK::HitChance::High, false, prediction);
    const auto context = BuildOozeContext(
        target, prediction, predictionHits, gapcloser);
    if (!ShouldCastOoze(context) ||
        !Engine::ControllerCastPosition(2, prediction.GetCastPosition())) {
        return false;
    }
    LastECastTick = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target,
                  Mode mode,
                  bool manual = false) {
    if (!Engine::RuntimeSpells[3] ||
        !Engine::RuntimeSpells[3]->IsReady() ||
        !Engine::ValidEnemy(target) ||
        ControllerHelpers::HasSpellShieldOrImmunity(target) ||
        !CastThrottlePassed(LastRCastTick, 55)) return false;
    if (!manual && !CanUse(3, mode)) return false;
    const float range = Engine::RuntimeSpells[3]->CurrentRange();
    const auto player = GameObjects::Player();
    if (!player.IsValid() ||
        player.Position().Distance2D(target.Position()) >
            range + target.BoundingRadius()) return false;

    SDK::PredictionOutput prediction{};
    const bool predictionHits = PredictionFor(
        3, target, SDK::HitChance::VeryHigh, false, prediction);
    const auto context = BuildArtilleryContext(
        target, prediction, predictionHits, manual);
    if (!manual && !ShouldCastArtillery(context)) return false;
    if (manual && (!context.PredictionVeryHigh || !context.InRange)) {
        return false;
    }
    if (!Engine::ControllerCastPosition(3, prediction.GetCastPosition())) {
        return false;
    }
    LastRCastTick = Now();
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
    const bool futureAttack = BarrageActive()
        ? AttackWillReachAfterW(target)
        : (CanUse(1, mode, true) && ShouldActivateBarrage(
              BuildBarrageContext(target, true)));

    SDK::PredictionOutput qPrediction{};
    const bool qPredictionHits = !spellShield && CanUse(0, mode) &&
        distance <= 1240.0f &&
        PredictionFor(0, target, SDK::HitChance::High, true, qPrediction);
    const bool q = qPredictionHits && ShouldCastSpittle(
        BuildSpittleContext(target, qPrediction, qPredictionHits));
    SDK::PredictionOutput ePrediction{};
    const bool ePredictionHits = !spellShield && CanUse(2, mode) &&
        distance <= 1240.0f &&
        PredictionFor(2, target, SDK::HitChance::High, false, ePrediction);
    const bool e = ePredictionHits && ShouldCastOoze(
        BuildOozeContext(target, ePrediction, ePredictionHits, false));
    SDK::PredictionOutput rPrediction{};
    const float rRange = Engine::RuntimeSpells[3]
        ? Engine::RuntimeSpells[3]->CurrentRange() : 0.0f;
    const bool rPredictionHits = !spellShield && CanUse(3, mode) &&
        distance <= rRange + target.BoundingRadius() &&
        PredictionFor(
            3, target, SDK::HitChance::VeryHigh, false, rPrediction);
    const bool r = rPredictionHits && ShouldCastArtillery(
        BuildArtilleryContext(target, rPrediction, rPredictionHits, false));

    const std::array<bool, 4> reachable = {q, false, e, r};
    const int expectedAutos = (attack || futureAttack)
        ? (IsEscaping(target, 0.45f) ? 1 : 2) : 0;
    auto context = BaseTargetContext(
        target, EstimatedDamage(
            target, reachable, expectedAutos));
    context.AutoReachable = attack;
    context.SetupReachable = futureAttack;
    context.DirectSpellReachable = q || e;
    context.ExecuteReachable = r;
    context.ProjectileBlocked = !attack && !futureAttack && !e && !r &&
        qPrediction.GetCastPosition().IsValid() && !q;
    return context;
}

inline MarksmanTargeting::TargetContext KillSecureFacts(
    const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const float health = target.Health() + target.AllShield();
    const bool spellShield =
        ControllerHelpers::HasSpellShieldOrImmunity(target);

    SDK::PredictionOutput qPrediction{};
    const bool qHit = !spellShield && CanUse(0, Mode::Automatic) &&
        SpellDamage(0, target) >= health && distance <= 1240.0f &&
        PredictionFor(0, target, SDK::HitChance::High, true, qPrediction);
    const bool q = qHit && ShouldCastSpittle(
        BuildSpittleContext(target, qPrediction, qHit));

    SDK::PredictionOutput ePrediction{};
    const bool eHit = !spellShield && CanUse(2, Mode::Automatic) &&
        SpellDamage(2, target) >= health && distance <= 1240.0f &&
        PredictionFor(2, target, SDK::HitChance::High, false, ePrediction);
    const bool e = eHit && ShouldCastOoze(
        BuildOozeContext(target, ePrediction, eHit, false));

    SDK::PredictionOutput rPrediction{};
    const float rRange = Engine::RuntimeSpells[3]
        ? Engine::RuntimeSpells[3]->CurrentRange() : 0.0f;
    const bool rHit = !spellShield && CanUse(3, Mode::Automatic) &&
        SpellDamage(3, target) >= health &&
        distance <= rRange + target.BoundingRadius() &&
        PredictionFor(
            3, target, SDK::HitChance::VeryHigh, false, rPrediction);
    const bool r = rHit && ShouldCastArtillery(
        BuildArtilleryContext(target, rPrediction, rHit, false));

    auto context = BaseTargetContext(
        target, std::max({q ? SpellDamage(0, target) : 0.0f,
                          e ? SpellDamage(2, target) : 0.0f,
                          r ? SpellDamage(3, target) : 0.0f}));
    context.DirectSpellReachable = q || e;
    context.ExecuteReachable = r;
    context.ProjectileBlocked = !e && !r && qHit && !q;
    return context;
}

inline AIHeroClient SelectSmartTarget(const AIHeroClient& preferred,
                                      Mode mode) {
    const float range = std::max(
        1240.0f,
        Engine::RuntimeSpells[3]
            ? Engine::RuntimeSpells[3]->CurrentRange() + 50.0f
            : 1240.0f);
    LastSmartTarget = ControllerHelpers::SelectReachableEnemy(
        preferred, range,
        [mode](const AIHeroClient& enemy) {
            return TargetFacts(enemy, mode);
        });
    return LastSmartTarget;
}

inline void RefreshOrbwalkerFocus(Mode mode,
                                  const AIHeroClient& target) {
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    const bool smartAttackKill = ImmediateAttackKillRoute(target);
    auto owned = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 1000.0f);
    const bool barrageWindow = BarrageActive() ||
        (LastWCastTick > 0 && Now() - LastWCastTick <= 350);
    if (!combat || !owned.IsValid() ||
        !AttackWillReachAfterW(owned) ||
        (!barrageWindow && !ControllerHelpers::Ready(1)) ||
        (smartAttackKill && owned.NetworkId() != target.NetworkId())) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
        owned = {};
    }
    if (smartAttackKill) {
        if (!owned.IsValid()) {
            (void)ForceImmediateAttackKill(
                target, 450, OwnedFocusTargetId, OwnedFocusUntil);
        }
        return;
    }
    if (owned.IsValid() || !combat || !BarrageActive() ||
        !Engine::ValidEnemy(target) ||
        !AttackWillReachAfterW(target)) return;
    (void)SetTemporaryOrbwalkerFocus(
        target, FutureAttackReach(target) + 45.0f, 850,
        OwnedFocusTargetId, OwnedFocusUntil);
}

inline bool TryAntiGapcloser() {
    if (GapcloserExpireTick < Now()) return false;
    const auto target = ControllerHelpers::HeroByNetworkId(GapcloserTargetId);
    return Engine::ValidEnemy(target, 1240.0f) &&
           CastE(target, Mode::Automatic, true);
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    const float range = std::max(
        1240.0f,
        Engine::RuntimeSpells[3]
            ? Engine::RuntimeSpells[3]->CurrentRange() + 50.0f
            : 1240.0f);
    const auto target = ControllerHelpers::SelectReachableEnemy(
        preferred, range,
        [](const AIHeroClient& enemy) {
            return KillSecureFacts(enemy);
        });
    if (!Engine::ValidEnemy(target)) return false;
    if (SpellDamage(0, target) >= target.Health() + target.AllShield() &&
        CastQ(target, Mode::Automatic)) return true;
    if (SpellDamage(2, target) >= target.Health() + target.AllShield() &&
        CastE(target, Mode::Automatic, false)) return true;
    return CastR(target, Mode::Automatic, false);
}

inline bool TryCombat(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastW(target, mode, true)) return true;
    if ((IsEscaping(target, 0.45f) || IsImmobile(target)) &&
        CastE(target, mode, false)) return true;
    if (CastQ(target, mode)) return true;
    if (CastE(target, mode, false)) return true;
    return CastR(target, mode, false);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    RefreshDynamicRanges();
    const bool combat = mode == Mode::Combo || mode == Mode::Harass;
    if (!combat) {
        ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    }
    if (ManualUltimatePressed()) {
        const float range = Engine::RuntimeSpells[3]
            ? Engine::RuntimeSpells[3]->CurrentRange() + 75.0f : 0.0f;
        const auto manual = ControllerHelpers::NearestEnemyToPlayer(
            preferred, range);
        if (Engine::ValidEnemy(manual) &&
            CastR(manual, Mode::Automatic, true)) return true;
    }
    if (TryAntiGapcloser()) return true;
    if (TryKillSecure(preferred)) return true;
    if (combat) {
        const auto target = SelectSmartTarget(preferred, mode);
        RefreshOrbwalkerFocus(mode, target);
        return TryCombat(target, mode);
    }
    if (mode == Mode::Flee) {
        const auto target = ControllerHelpers::NearestEnemyToPlayer(
            preferred, 1240.0f);
        return Engine::ValidEnemy(target) &&
               CastE(target, Mode::Flee, true);
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
        ControllerHelpers::SpellEventNameContainsAny(args, {"kogmawq"})) {
        LastQCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"kogmawbioarcanebarrage"})) {
        LastWCastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"kogmawvoidooze"})) {
        LastECastTick = now;
    } else if (args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
               ControllerHelpers::SpellEventNameContainsAny(
                   args, {"kogmawlivingartillery"})) {
        LastRCastTick = now;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    auto focus = OwnedOrbwalkerFocus(
        OwnedFocusTargetId, OwnedFocusUntil, 1000.0f);
    if (focus.IsValid()) {
        const float bonus = BarrageActive()
            ? 0.0f
            : BarrageBonusRange(ControllerHelpers::SpellRank(1));
        if (!RedirectBeforeAttackToFocus(args, focus, bonus)) {
            ClearTemporaryOrbwalkerFocus(
                OwnedFocusTargetId, OwnedFocusUntil);
            focus = {};
        }
    }
    if (!focus.IsValid() && args.Target.IsValid() && args.Target.IsHero()) {
        focus = AIHeroClient(args.Target.Handle());
    }
    if (focus.IsValid() &&
        (LastMode == Mode::Combo || LastMode == Mode::Harass)) {
        (void)CastW(focus, LastMode, true);
    }
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
        GapcloserExpireTick, 780.0f, 900);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "KogMawMechanics", "Kog'Maw Mechanics"));
    BarrageMenu = TacticsMenu->AddSubMenu(new Menu(
        "BarrageLogic", "Bio-Arcane Barrage"));
    BarrageMenu->Add(new MenuSeparator(
        "RealRoute", "W requires a real empowered attack route"));
    ArtilleryMenu = TacticsMenu->AddSubMenu(new Menu(
        "ArtilleryLogic", "Living Artillery"));
    ArtilleryMenu->Add(new MenuSlider(
        "MaxStacks", "Maximum R cost stacks", 2, 0, 6));
    ArtilleryMenu->Add(new MenuSlider(
        "TargetHp", "R target maximum HP percent", 42, 10, 100));
}

inline void OnLoad() {
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    LastMode = Mode::None;
    LastSmartTarget = {};
    RefreshDynamicRanges();
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    TacticsMenu = BarrageMenu = ArtilleryMenu = nullptr;
    LastMode = Mode::None;
    LastSmartTarget = {};
}

inline constexpr const char* Scenarios[] = {
    "Reject targets outside every live attack/Q/E/R route",
    "Update Living Artillery range to 1300/1550/1800 by rank",
    "Activate W only when its rank-scaled bonus range creates an attack",
    "Temporarily focus the W target and release focus after its attack",
    "Reject W attacks and owned focus across a projectile wall",
    "Redirect BeforeAttack to the owned focus without stealing newer focus",
    "Preserve a normal attack before Q or E unless the spell is lethal",
    "Reject Q collision through minions, champions and projectile walls",
    "Use E on escaping, immobilized or committed gapcloser targets",
    "Prefer low-health/CC artillery targets outside attack range",
    "Ignore the normal R stack cap when the shot is lethal",
    "Clear all owned orbwalker state on mode exit and unload",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "KogMaw";
    controller.ControllerId = "champion.kuroaio.ai.kogmaw.onetrick";
    controller.KitRevision = "CommunityDragon current / TestOrbwalker port";
    controller.ResearchArtifact =
        "C:/Users/funny/Downloads/TestOrbwalker/TestOrbwalker/AllChampions/PortKogMaw.cs";
    controller.ImplementationSummary =
        "Dynamic W/R reach, attack-preserving Q/E, stack-aware missing-health "
        "artillery, anti-gapcloser ooze and owned orbwalker focus lifecycle.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::KogMaw
