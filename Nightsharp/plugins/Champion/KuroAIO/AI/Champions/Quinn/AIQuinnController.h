#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIMarksmanControllerHelpers.h"
#include "AIQuinnGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Quinn {

using namespace Geometry;
using namespace MarksmanControllerHelpers;
using ControllerHelpers::CaptureLocalAutoAttackEvent;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::OrbwalkerHeroTarget;
using ControllerHelpers::PlayerSelectedEnemy;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;

inline Menu* TacticsMenu = nullptr;
inline Menu* HarrierMenu = nullptr;
inline Menu* ScoutingMenu = nullptr;
inline Menu* VaultMenu = nullptr;
inline Menu* BehindLinesMenu = nullptr;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAttackTick = 0;
inline int LastAttackTargetId = 0;
inline int HarrierTargetId = 0;
inline int HarrierStacks = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline bool RevealObserved = false;
inline bool RActive = false;
inline Mode LastMode = Mode::None;

inline bool NameContains(const char* value, const char* token) {
    return Engine::TextContains(value, token);
}

inline bool IsRBuffName(const char* name) {
    return NameContains(name, "quinnr") ||
           NameContains(name, "quinnbehindenemylines") ||
           NameContains(name, "quinnrchannel");
}

inline bool IsHarrierName(const char* name) {
    return NameContains(name, "quinnpassivemarked") ||
           NameContains(name, "quinnharrier");
}

inline float SpellDamage(int index, const AIHeroClient& target) {
    if (!target.IsValid() || index < 0 || index >= 4 ||
        !Engine::RuntimeSpells[index]) return 0.0f;
    return std::max(0.0f, Engine::RuntimeSpells[index]->GetDamage(target));
}

inline float AutoDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return std::max(0.0f, player.CalculatePhysicalDamage(
        target, player.TotalAttackDamage()));
}

inline bool LethalWith(const AIHeroClient& target, int spellIndex,
                       bool includeHarrier = true) {
    if (!target.IsValid()) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    float damage = SpellDamage(spellIndex, target) + AutoDamage(target);
    if (includeHarrier && HarrierTargetId == static_cast<int>(target.NetworkId())) {
        damage += player.CalculatePhysicalDamage(target,
            HarrierBonusDamage(player.Level(), player.BonusAttackDamage()));
    }
    return damage >= target.Health() + target.AllShield();
}

inline bool CanUse(int index, Mode mode, bool reactive = false) {
    if (!ControllerHelpers::ControllerSpellAvailable(index, mode,
            reactive || index == 2 || index == 3)) return false;
    if (PlayerMobilityLocked()) return false;
    return true;
}

inline AIHeroClient SelectTarget(const AIHeroClient& preferred) {
    const auto selected = PlayerSelectedEnemy(1300.0f);
    if (Engine::ValidEnemy(selected, 1300.0f)) return selected;
    const auto orb = OrbwalkerHeroTarget(1300.0f);
    if (Engine::ValidEnemy(orb, 1300.0f)) return orb;
    if (Engine::ValidEnemy(preferred, 1300.0f)) return preferred;
    return NearestEnemyToPlayer({}, 1300.0f);
}

inline bool IsMarked(const AIHeroClient& target) {
    return target.IsValid() && (target.HasBuff("QuinnPassiveMarked") ||
        target.HasBuff("quinnpassivemarked") || target.HasBuff("QuinnHarrier"));
}

inline void ReconcileHarrier(const AIHeroClient& target) {
    if (!target.IsValid()) {
        HarrierTargetId = 0;
        HarrierStacks = 0;
        return;
    }
    if (IsMarked(target)) {
        HarrierTargetId = static_cast<int>(target.NetworkId());
        HarrierStacks = 1;
    } else if (HarrierTargetId == static_cast<int>(target.NetworkId())) {
        HarrierTargetId = 0;
        HarrierStacks = 0;
    }
}

inline bool HasHiddenEnemy() {
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsEnemy() ||
            !enemy.IsTargetable() || enemy.IsVisible()) continue;
        return true;
    }
    return false;
}

inline bool SafeVault(const AIHeroClient& target, bool escapeRoute,
                      Vector3* landingOut = nullptr) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kVaultRange)) return false;
    const Vector3 predicted = PredictPosition(target, 0.22f);
    const float distance = player.Position().Distance2D(predicted);
    if (!VaultReachable(distance, target.BoundingRadius())) return false;
    const Vec2 landing2d = VaultLanding(
        {player.Position().x, player.Position().z},
        {predicted.x, predicted.z});
    const Vector3 destination{
        landing2d.x, player.Position().y, landing2d.y};
    if (!destination.IsValid() || destination.IsZero()) return false;
    const bool lethal = LethalWith(target, 2);
    const int enemies = Engine::CountEnemiesAt(destination, 525.0f);
    if (!UnsafeMobilityAllowed(
            SDK::NavMesh::IsWall(destination),
            Engine::UnderEnemyTurret(destination),
            Engine::UnderEnemyTurret(player.Position()), lethal, escapeRoute,
            enemies, 2, player.HealthPercent())) return false;
    if (landingOut) *landingOut = destination;
    return true;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!Engine::ValidEnemy(target, kQRange) || !CanUse(0, mode, reactive) ||
        Now() - LastQCastTick < 70) return false;
    if (!reactive && Orbwalker::IsWindingUp() &&
        Orbwalker::AttackCastDelayRemaining() > 25) return false;
    const auto predicted = Engine::RuntimeSpells[0]->GetPrediction(target);
    if (static_cast<int>(predicted.Hitchance) <
            static_cast<int>(reactive ? SDK::HitChance::High : SDK::HitChance::High) &&
        !target.IsDashing() && !Engine::IsHardCrowdControlled(target)) return false;
    if (!ControllerHelpers::ProjectileWallBlocksFromPlayer(
            predicted.GetCastPosition(), kQHalfWidth)) {
        if (!Engine::ControllerCastPredicted(0, target, SDK::HitChance::High)) return false;
    } else {
        return false;
    }
    LastQCastTick = Now();
    return true;
}

inline bool CastScouting(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!CanUse(1, mode, reactive) || Now() - LastWCastTick < 120) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.HealthPercent() <= 24.0f ||
        Engine::UnderEnemyTurret(player.Position())) return false;
    const bool hidden = HasHiddenEnemy();
    const bool objective = Engine::CountEnemiesAt(player.Position(), 1400.0f) == 0 &&
        target.IsValid();
    if (!ScoutingWorthwhile(hidden, objective,
            Engine::CountEnemiesAt(player.Position(), 1100.0f), false)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastWCastTick = Now();
    RevealObserved = true;
    return true;
}

inline bool CastVault(const AIHeroClient& target, Mode mode,
                      bool escapeRoute = false, bool reactive = false) {
    if (!Engine::ValidEnemy(target, kVaultRange) || !CanUse(2, mode, reactive) ||
        Now() - LastECastTick < 100) return false;
    if (!escapeRoute && !reactive && Orbwalker::IsWindingUp() &&
        Orbwalker::AttackCastDelayRemaining() > 25) return false;
    Vector3 landing{};
    if (!SafeVault(target, escapeRoute, &landing)) return false;
    const bool gapcloser = GapcloserExpireTick >= Now() &&
        GapcloserTargetId == static_cast<int>(target.NetworkId());
    const bool peel = escapeRoute || reactive || gapcloser || target.IsDashing() ||
        Engine::CountEnemiesAt(GameObjects::Player().Position(), 450.0f) >= 2;
    const bool marked = IsMarked(target) ||
        HarrierTargetId == static_cast<int>(target.NetworkId());
    if (!peel && !marked && !LethalWith(target, 2)) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    LastECastTick = Now();
    return true;
}

inline RPhase CurrentRPhase() {
    const auto player = GameObjects::Player();
    const bool buff = player.IsValid() && (player.HasBuff("QuinnR") ||
        player.HasBuff("QuinnRChannel") || player.HasBuff("QuinnBehindEnemyLines"));
    const bool runtime = Engine::IsRuntimeRecast(3);
    const int elapsed = LastRCastTick > 0 ? Now() - LastRCastTick : -1;
    return ReconcileRPhase(buff || RActive, runtime, LastRCastTick > 0, elapsed);
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool escapeRoute = false) {
    if (!CanUse(3, mode, escapeRoute) || Now() - LastRCastTick < 180) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const RPhase phase = CurrentRPhase();
    const bool lethal = target.IsValid() && LethalWith(target, 3);
    if (phase == RPhase::Recast) {
        if (!RRecastSafe(Now() - LastRCastTick,
                Engine::UnderEnemyTurret(player.Position()), lethal, escapeRoute,
                Engine::CountEnemiesAt(player.Position(), 650.0f), 2,
                player.HealthPercent())) return false;
        if (!lethal && !escapeRoute && !target.IsValid()) return false;
        if (!Engine::ControllerCastSelf(3)) return false;
        RActive = false;
        LastRCastTick = Now();
        return true;
    }
    if (phase != RPhase::Ready || PlayerMobilityLocked() ||
        Engine::UnderEnemyTurret(player.Position())) return false;
    const bool committed = escapeRoute ||
        (target.IsValid() && player.Position().Distance2D(target.Position()) > 850.0f);
    if (!committed || Engine::CountEnemiesAt(player.Position(), 500.0f) > 0) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastRCastTick = Now();
    RActive = true;
    return true;
}

inline bool TryAutomatic(const AIHeroClient& target) {
    if (CastVault(target, Mode::Automatic, false, true)) return true;
    if (CastQ(target, Mode::Automatic, true)) return true;
    if (CastScouting(target, Mode::Automatic, true)) return true;
    return CastR(target, Mode::Automatic, false);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& preferred) {
    LastMode = mode;
    const auto target = SelectTarget(preferred);
    ReconcileHarrier(target);
    if (LastWCastTick > 0 && Now() - LastWCastTick > kRevealDurationMs) RevealObserved = false;

    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        return Engine::TryFarm(mode);
    }
    if (mode == Mode::Flee) {
        if (CastVault(target, mode, true, true)) return true;
        return CastR(target, mode, true);
    }
    if (mode == Mode::Automatic) return TryAutomatic(target);
    if (!Engine::ValidEnemy(target, 1300.0f)) {
        return mode == Mode::Combo ? CastScouting(target, mode) : false;
    }
    if (mode == Mode::Combo && CastR(target, mode, false)) return true;
    if (CastVault(target, mode, false, false)) return true;
    if (CastQ(target, mode, false)) return true;
    return CastScouting(target, mode, false);
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!ControllerHelpers::IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (args.IsAutoAttack) return;
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q)) LastQCastTick = now;
    if (args.Slot == static_cast<int>(SDK::SpellSlot::W)) LastWCastTick = now;
    if (args.Slot == static_cast<int>(SDK::SpellSlot::E)) LastECastTick = now;
    if (args.Slot == static_cast<int>(SDK::SpellSlot::R)) {
        LastRCastTick = now;
        if (Engine::TextContains(args.SpellName, "final") ||
            Engine::IsRuntimeRecast(3)) RActive = false;
        else RActive = true;
    }
}

inline void OnBuff(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    const auto player = GameObjects::Player();
    if (IsHarrierName(args.BuffName) && added) {
        HarrierTargetId = id;
        HarrierStacks = 1;
    } else if (IsHarrierName(args.BuffName) && !added && id == HarrierTargetId) {
        HarrierTargetId = 0;
        HarrierStacks = 0;
    }
    if (player.IsValid() && id == static_cast<int>(player.NetworkId()) &&
        IsRBuffName(args.BuffName)) RActive = added;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || !args.Target.IsHero()) return;
    const AIHeroClient target(args.Target.Handle());
    ReconcileHarrier(target);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    LastAttackTick = Now();
    LastAttackTargetId = args.Target.IsValid()
        ? static_cast<int>(args.Target.NetworkId()) : 0;
    if (LastAttackTargetId == HarrierTargetId) {
        HarrierStacks = ConsumeHarrier(HarrierStacks, true);
        HarrierTargetId = 0;
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("QuinnMechanics", "Quinn Mechanics"));
    HarrierMenu = TacticsMenu->AddSubMenu(new Menu("Harrier", "Harrier marks"));
    HarrierMenu->Add(new MenuSeparator("Consume", "Consume Harrier on the selected attack target"));
    ScoutingMenu = TacticsMenu->AddSubMenu(new Menu("Scouting", "Heightened Senses scouting"));
    ScoutingMenu->Add(new MenuSeparator("Reveal", "Reveal hidden enemies before facechecking"));
    VaultMenu = TacticsMenu->AddSubMenu(new Menu("Vault", "Vault displacement safety"));
    VaultMenu->Add(new MenuSeparator("Landing", "Reject walls, unsafe enemy counts and turret landings"));
    BehindLinesMenu = TacticsMenu->AddSubMenu(new Menu("BehindEnemyLines", "Behind Enemy Lines"));
    BehindLinesMenu->Add(new MenuSeparator("Movement", "Keep R movement and recast safety player-owned"));
}

inline void OnLoad() {
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAttackTick = LastAttackTargetId = 0;
    HarrierTargetId = HarrierStacks = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    RevealObserved = false;
    RActive = false;
    LastMode = Mode::None;
}

inline void OnUnload() {
    TacticsMenu = HarrierMenu = ScoutingMenu = VaultMenu = BehindLinesMenu = nullptr;
    HarrierTargetId = HarrierStacks = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    RevealObserved = false;
    RActive = false;
    LastMode = Mode::None;
}

inline constexpr const char* Scenarios[] = {
    "Reconcile Harrier from QuinnPassiveMarked and polling before choosing a target",
    "Prefer the selected enemy, then the orbwalker's hero target, then a nearest fallback",
    "Consume one Harrier mark only after the recorded attack lands on that target",
    "Preserve a meaningful auto-attack windup before offensive Q or E",
    "Cast Blinding Assault through prediction with the first collision as the blind target",
    "Reject a Q line whose projectile is blocked by navmesh terrain",
    "Use Heightened Senses to reveal hidden enemies and avoid blind facechecks",
    "Keep scouting resource and cooldown gates separate from passive attack speed value",
    "Compute Vault landing displacement behind the target rather than dashing blindly",
    "Reject Vault into navmesh walls, unsafe enemy counts or a new enemy turret",
    "Allow Vault turret commitment only when already under turret, lethal or escaping",
    "Use Vault reactively against a gapcloser for peel before routine damage",
    "Reconcile Behind Enemy Lines from cast events, buff events and runtime recast state",
    "Start R only outside immediate combat and never while under an enemy turret",
    "Recast Skystrike only after the minimum flight window and safety tail",
    "Reject unsafe R recasts at low health into crowded or turret positions",
    "Keep R movement player-owned instead of issuing speculative cursor movement",
    "Route LaneClear, Jungle and LastHit to the shared health/resource farm planner",
    "Use conservative Q/E branches in Harass and Automatic modes",
    "Clear all owned mark, reveal, Vault and R state on unload",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Quinn;
    controller.ControllerId = "champion.kuroaio.ai.quinn.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIQuinn.md";
    controller.ImplementationSummary =
        "Harrier reconciliation, collision-aware Blinding Assault, scouting reveal, "
        "safe Vault displacement and turret-gated Behind Enemy Lines recasts.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &CaptureLocalAutoAttackEvent<&LastAttackTargetId, &LastAttackTick>;
    controller.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&OnBuff, true>;
    controller.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&OnBuff, false>;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<
        &GapcloserTargetId, &GapcloserEndpoint, &GapcloserExpireTick, 675, 700>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Quinn
