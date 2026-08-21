#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AIVayneGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Vayne {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureLocalAutoAttackEvent;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::Now;

inline Menu* TacticsMenu = nullptr;
inline Menu* TumbleMenu = nullptr;
inline Menu* CondemnMenu = nullptr;
inline Menu* FinalHourMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int ObservedBoltTargetId = 0;
inline int ObservedBoltStacks = 0;
inline int OwnedFocusTargetId = 0;
inline int OwnedFocusUntil = 0;
inline bool RPosture = false;
inline Mode LastMode = Mode::None;

inline Vec2 ToVec2(const Vector3& value) {
    return {value.x, value.z};
}

inline int BoltStacks(const AIBaseClient& target) {
    if (!target.IsValid()) return 0;
    return SilverBoltStacks(ControllerHelpers::MaximumBuffCount(
        target, { "VayneSilveredBolts", "vaynesilveredbolts" }));
}

inline void RefreshBoltObservation(const AIHeroClient& target) {
    if (!target.IsValid()) {
        ObservedBoltTargetId = 0;
        ObservedBoltStacks = 0;
        return;
    }
    ObservedBoltTargetId = static_cast<int>(target.NetworkId());
    ObservedBoltStacks = BoltStacks(target);
}

inline bool FinalHourActive() {
    const auto player = GameObjects::Player();
    return RPosture || (player.IsValid() &&
        ControllerHelpers::HasAnyBuff(
            player, { "VayneInquisition", "vayneinquisition" }));
}

inline bool PreserveAttack(bool reactive = false) {
    return !reactive && Orbwalker::IsWindingUp() &&
           Orbwalker::AttackCastDelayRemaining() > 25;
}

inline void ReconcileFinalHourPosture() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) {
        RPosture = false;
        return;
    }
    const bool buffed = player.HasBuff("VayneInquisition") ||
        player.HasBuff("vayneinquisition");
    if (buffed) {
        RPosture = true;
    } else if (RPosture && LastRCastTick > 0 &&
               Now() - LastRCastTick > 350) {
        RPosture = false;
    }
}

inline AIHeroClient SelectTarget(const AIHeroClient& preferred) {
    if (Engine::ValidEnemy(preferred, 1100.0f)) return preferred;
    const auto orb = ControllerHelpers::OrbwalkerHeroTarget(1100.0f);
    if (orb.IsValid()) return orb;
    return ControllerHelpers::NearestEnemyToPlayer({}, 1100.0f);
}

inline bool SafeTumbleEndpoint(const AIHeroClient& target,
                               const Vector3& endpoint) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero() ||
        SDK::NavMesh::IsWall(endpoint)) return false;
    const bool underTurret = Engine::UnderEnemyTurret(endpoint);
    const bool playerUnderTurret = Engine::UnderEnemyTurret(player.Position());
    const bool lethal = target.IsValid() &&
        AutoDamage(target) + SpellDamage(0, target) >=
            target.Health() + target.AllShield();
    return TurretDiveAllowed(underTurret, playerUnderTurret, lethal, false);
}

inline Vector3 TumbleEndpoint(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    Vector3 cursor = Game::CursorPos();
    if (!cursor.IsValid() || cursor.IsZero()) {
        cursor = target.IsValid()
            ? ControllerHelpers::PredictPosition(target, 0.20f)
            : player.Position();
    }
    Vector3 direction = cursor - player.Position();
    direction.y = 0.0f;
    const float length = std::sqrt(
        direction.x * direction.x + direction.z * direction.z);
    if (length <= 0.01f) return {};
    const float distance = std::min(300.0f, length);
    return player.Position() + direction * (distance / length);
}

inline bool CondemnWallStun(const AIHeroClient& target,
                            Vector3* wallContact = nullptr) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return false;
    const Vector3 predicted = ControllerHelpers::PredictPosition(target, 0.16f);
    Vector3 direction = predicted - player.Position();
    direction.y = 0.0f;
    const float length = std::sqrt(
        direction.x * direction.x + direction.z * direction.z);
    if (length <= 0.01f) return false;
    direction = direction * (1.0f / length);
    const Vector3 desired = predicted + direction * kCondemnPushDistance;
    Vector3 wall{};
    if (!SDK::NavMesh::FindWallCollision(predicted, desired, wall, 10.0f) ||
        !wall.IsValid() || wall.IsZero()) return false;
    if (wallContact) *wallContact = wall;
    return CondemnWallAngleAllows(
        ToVec2(player.Position()), ToVec2(predicted), ToVec2(wall),
        65.0f, 32.0f, kCondemnPushDistance);
}

inline bool CondemnPeel(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid() &&
        player.Position().Distance2D(target.Position()) <= 550.0f &&
        (target.IsDashing() ||
         Engine::CountEnemiesAt(player.Position(), 475.0f) >= 2);
}

inline bool CastCondemn(const AIHeroClient& target, Mode mode,
                        bool reactive = false) {
    if (!Engine::ValidEnemy(target, 575.0f) || !CanUse(2, mode, reactive) ||
        Now() - LastECastTick < 45) return false;
    const bool wallStun = CondemnWallStun(target);
    const bool peel = CondemnPeel(target) ||
        (GapcloserExpireTick >= Now() &&
         static_cast<int>(target.NetworkId()) == GapcloserTargetId);
    if (!wallStun && !peel) return false;
    if (!reactive && PreserveAttack() && !wallStun) return false;
    if (ControllerHelpers::ProjectileWallBlocksFromPlayer(
            target.Position(), 0.0f)) return false;
    LastECastTick = Now();
    ObservedBoltStacks = BoltStacks(target);
    return true;
}

inline bool CastTumble(const AIHeroClient& target, Mode mode,
                       bool flee = false, bool reactive = false) {
    if (!CanUse(0, mode, reactive || flee) || Now() - LastQCastTick < 55 ||
        PreserveAttack(reactive)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const Vector3 endpoint = TumbleEndpoint(target);
    if (!SafeTumbleEndpoint(target, endpoint)) return false;
    const bool attackReset = target.IsValid() && InAutoAttackRange(target) &&
        LastAfterAttackTargetId == static_cast<int>(target.NetworkId()) &&
        Now() - LastAfterAttackTick <= 450;
    const bool reposition = target.IsValid() &&
        player.Position().Distance2D(target.Position()) > 475.0f;
    if (!flee && !attackReset && !reposition && !FinalHourActive()) return false;
    if (!Engine::ControllerCastPosition(0, endpoint)) return false;
    LastQCastTick = Now();
    return true;
}

inline bool CastFinalHour(const AIHeroClient& target, Mode mode) {
    if (!target.IsValid() || (mode != Mode::Combo && mode != Mode::Automatic) ||
        !CanUse(3, mode) || FinalHourActive() || Now() - LastRCastTick < 100) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool lethal = AutoDamage(target) * 3.0f + SpellDamage(2, target) >=
        target.Health() + target.AllShield();
    const bool committed = Engine::CountEnemiesAt(player.Position(), 850.0f) >= 2 ||
        target.HealthPercent() <= 67.0f;
    if (!committed || !TurretDiveAllowed(
            Engine::UnderEnemyTurret(player.Position()),
            Engine::UnderEnemyTurret(player.Position()), lethal, false)) {
        return false;
    }
    if (PreserveAttack() && !lethal) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastRCastTick = Now();
    RPosture = true;
    return true;
}

inline MarksmanTargeting::TargetContext TargetFacts(
    const AIHeroClient& target, Mode mode) {
    const auto player = GameObjects::Player();
    const bool attack = OrbwalkerAttackRoute(target);
    const bool e = CanUse(2, mode) &&
        (CondemnWallStun(target) || CondemnPeel(target));
    const bool q = CanUse(0, mode) && (attack || FinalHourActive());
    const bool r = CanUse(3, mode) &&
        (target.HealthPercent() <= 67.0f ||
         (player.IsValid() && Engine::CountEnemiesAt(player.Position(), 850.0f) >= 2));
    const std::array<bool, 4> reachable = {q, false, e, r};
    auto context = BaseTargetContext(
        target, EstimatedDamage(target, reachable, attack ? 2 : 0));
    context.AutoReachable = attack;
    context.SetupReachable = q;
    context.DirectSpellReachable = e;
    context.ExecuteReachable = r;
    context.ProjectileBlocked = !attack && !q && !e && !r;
    context.Priority += static_cast<float>(SilverBoltStacks(BoltStacks(target))) * 80.0f;
    return context;
}

inline AIHeroClient SmartTarget(const AIHeroClient& preferred, Mode mode) {
    return ControllerHelpers::SelectReachableEnemy(
        preferred, 1100.0f,
        [mode](const AIHeroClient& target) { return TargetFacts(target, mode); });
}

inline bool TryAutomatic(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastCondemn(target, Mode::Automatic, true)) return true;
    if (CastFinalHour(target, Mode::Automatic)) return true;
    return CastTumble(target, Mode::Automatic, false, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    ReconcileFinalHourPosture();
    const auto target = SmartTarget(SelectTarget(preferred), mode);
    RefreshBoltObservation(target);
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return Engine::TryFarm(mode);
    if (mode == Mode::Flee) {
        if (CastTumble(target, mode, true, true)) return true;
        return CastCondemn(target, mode, true);
    }
    if (!Engine::ValidEnemy(target)) return false;
    if (mode == Mode::Automatic) return TryAutomatic(target);
    if (CastCondemn(target, mode, false)) return true;
    if (mode == Mode::Combo && CastFinalHour(target, mode)) return true;
    return CastTumble(target, mode, false, false);
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !args.Sender.IsValid() ||
        static_cast<int>(args.Sender.NetworkId) !=
            static_cast<int>(player.NetworkId())) return;
    if (args.IsAutoAttack) return;
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q)) LastQCastTick = Now();
    if (args.Slot == static_cast<int>(SDK::SpellSlot::E)) LastECastTick = Now();
    if (args.Slot == static_cast<int>(SDK::SpellSlot::R)) {
        LastRCastTick = Now();
        RPosture = true;
    }
}

inline void ObserveBuff(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (Engine::TextContains(args.BuffName, "silveredbolts")) {
        ObservedBoltTargetId = id;
        ObservedBoltStacks = added ? SilverBoltStacks(args.Count) : 0;
    }
    const auto player = GameObjects::Player();
    if (player.IsValid() && id == static_cast<int>(player.NetworkId()) &&
        Engine::TextContains(args.BuffName, "inquisition")) {
        RPosture = added;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || !args.Target.IsHero()) return;
    const AIHeroClient target(args.Target.Handle());
    RefreshBoltObservation(target);
    if (LastMode == Mode::Combo) (void)CastFinalHour(target, LastMode);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    LastAfterAttackTick = Now();
    LastAfterAttackTargetId = args.Target.IsValid()
        ? static_cast<int>(args.Target.NetworkId()) : 0;
    if (args.Target.IsValid() && args.Target.IsHero()) {
        const AIHeroClient target(args.Target.Handle());
        RefreshBoltObservation(target);
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("VayneMechanics", "Vayne Mechanics"));
    TumbleMenu = TacticsMenu->AddSubMenu(new Menu("TumblePolicy", "Tumble / Attack Windup"));
    TumbleMenu->Add(new MenuSeparator("AfterAttack", "Prefer Tumble after an attack reset"));
    CondemnMenu = TacticsMenu->AddSubMenu(new Menu("CondemnPolicy", "Condemn Wall Angle"));
    CondemnMenu->Add(new MenuSeparator("WallAngle", "Require terrain contact or peel"));
    FinalHourMenu = TacticsMenu->AddSubMenu(new Menu("FinalHourPolicy", "Final Hour Posture"));
    FinalHourMenu->Add(new MenuSeparator("Stealth", "Use R for committed stealth"));
}

inline void OnLoad() {
    LastQCastTick = LastECastTick = LastRCastTick = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    ObservedBoltTargetId = ObservedBoltStacks = 0;
    OwnedFocusTargetId = OwnedFocusUntil = 0;
    RPosture = false;
    LastMode = Mode::None;
}

inline void OnUnload() {
    ClearTemporaryOrbwalkerFocus(OwnedFocusTargetId, OwnedFocusUntil);
    TacticsMenu = TumbleMenu = CondemnMenu = FinalHourMenu = nullptr;
    RPosture = false;
    LastMode = Mode::None;
}

inline constexpr const char* Scenarios[] = {
    "Reconcile Silver Bolts from the live VayneSilveredBolts target marker",
    "Clamp observed Silver Bolts to the two pre-proc stacks",
    "Preserve the autonomous and orbwalker targets through target selection",
    "Tumble after an attack reset instead of cancelling a meaningful AA windup",
    "Reject Tumble endpoints in terrain or under a new enemy turret",
    "Allow an R posture Tumble to create a stealth re-engage or escape angle",
    "Detect Condemn terrain contact along the full 475-unit knockback ray",
    "Require a safe Condemn wall angle rather than firing blind point-click E",
    "Use Condemn immediately for a committed gapcloser or close-range peel",
    "Enter Final Hour only for a committed multi-enemy or low-health exchange",
    "Reject turret dives unless already under turret or the sequence is lethal",
    "Use Q/E conservatively in Harass, Flee and Automatic modes",
    "Delegate LaneClear, Jungle and LastHit to the shared farm route",
    "Reconcile R posture from cast events, buff events and polling",
    "Clear owned orbwalker focus and transient state on unload",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Vayne;
    controller.ControllerId = "champion.kuroaio.ai.vayne.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIVayne.md";
    controller.ImplementationSummary =
        "Silver Bolts reconciliation, terrain-aware Condemn angle validation, "
        "attack-windup-safe Tumble, Final Hour stealth posture and turret-safe targeting.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &CaptureLocalAutoAttackEvent<
        &LastAfterAttackTargetId, &LastAfterAttackTick>;
    controller.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&ObserveBuff, true>;
    controller.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&ObserveBuff, false>;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 575, 700>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Vayne
