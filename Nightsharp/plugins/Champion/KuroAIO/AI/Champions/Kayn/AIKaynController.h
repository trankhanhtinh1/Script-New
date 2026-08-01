#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIKayn.h"
#include "AIKaynGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Kayn {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline OrbState Orbs{};
inline Form CurrentForm = Form::Untransformed;
inline bool Transforming = false;
inline bool RHostActive = false;
inline int RHostId = 0;
inline int RMarkedTargetId = 0;
inline int TransformStartTick = 0;
inline int RCastTick = 0;
inline int RHostUntil = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline Vector3 IncomingThreatEndpoint{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline std::array<int, 4> LastCastTick{};

inline bool PreserveAttack(bool reactive, bool lethal = false) {
    return !reactive && !lethal && Orbwalker::IsWindingUp() &&
        Bool(TacticsMenu, "PreserveAttacks", true);
}

inline bool TargetDamageable(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) && !target.IsInvulnerable() &&
        !HasSpellShieldOrImmunity(target);
}

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        (reactive || Now() - LastCastTick[static_cast<std::size_t>(slot)] >= 35);
}

inline bool ResourceGate(Mode mode, bool urgent = false) {
    if (urgent || mode == Mode::Combo || mode == Mode::Flee || mode == Mode::Automatic) return true;
    const float floor = static_cast<float>(
        mode == Mode::Harass ? Slider(TacticsMenu, "HarassMana", 45) :
        Slider(FarmMenu, "FarmMana", 25));
    return PlayerManaPercent() >= floor;
}

inline bool LethalPhysical(const AIHeroClient& target, float rawDamage) {
    const auto player = GameObjects::Player();
    return player.IsValid() && TargetDamageable(target) &&
        player.CalculatePhysicalDamage(target, rawDamage) >= target.Health() + target.AllShield();
}

inline float SpellDamage(const AIHeroClient& target, int slot) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !TargetDamageable(target)) return 0.0f;
    const int rank = SpellRank(slot);
    if (slot == 0) {
        float damage = QRawDamage(rank, player.TotalAttackDamage());
        if (CurrentForm == Form::DarkinSlayer) damage += QDarkinBonusDamage(target.MaxHealth(), player.BonusAttackDamage());
        return player.CalculatePhysicalDamage(target, damage);
    }
    if (slot == 1) return player.CalculatePhysicalDamage(target, WRawDamage(rank, player.BonusAttackDamage()));
    if (slot == 3) {
        float damage = RRawDamage(rank, player.TotalAttackDamage());
        if (CurrentForm == Form::DarkinSlayer) damage += RDarkinDamage(target.MaxHealth(), player.BonusAttackDamage());
        return player.CalculatePhysicalDamage(target, damage);
    }
    return 0.0f;
}

inline bool SafeEndpoint(const Vector3& endpoint, bool defensive, bool lethal,
                         int maximumEnemies = 2) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint)) return false;
    const MobilityContext context{
        true, true, true,
        Engine::UnderEnemyTurret(endpoint) && !Engine::UnderEnemyTurret(player.Position()),
        Engine::UnderEnemyTurret(player.Position()),
        Engine::CountEnemiesAt(endpoint, 450.0f), maximumEnemies, defensive, lethal, CurrentForm};
    return MobilityAllowed(context);
}

inline bool QDashPathOpen(const Vector3& origin, const Vector3& endpoint) {
    const float distance = origin.Distance2D(endpoint);
    if (distance <= 1.0f) return false;
    const Vector3 direction = Direction2D(origin, endpoint);
    if (direction.IsZero()) return false;
    for (float offset = 24.0f; offset < distance; offset += 24.0f)
        if (SDK::NavMesh::IsWall(origin + direction * offset)) return false;
    return true;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode, reactive) || !ResourceGate(mode, reactive) ||
        !TargetDamageable(target) || !Engine::ValidEnemy(target, kQRange + 100.0f)) return false;
    const Vector3 predicted = PredictPosition(target, 0.15f);
    const Vector3 endpoint = ClampDashEndpoint(player.Position(), predicted, kQRange);
    if (endpoint.IsZero() || !QDashPathOpen(player.Position(), endpoint)) return false;
    const bool lethal = LethalPhysical(target, SpellDamage(target, 0));
    if (!QSlashHits(endpoint, PredictPosition(target, 0.15f), target.BoundingRadius()) ||
        PreserveAttack(reactive, lethal) || !SafeEndpoint(endpoint, player.HealthPercent() < 34.0f, lethal,
                                                          Slider(TacticsMenu, "MaxDashEnemies", 2))) return false;
    if (!Engine::ControllerCastPosition(0, endpoint)) return false;
    LastCastTick[0] = Now();
    LastAutoTargetId = static_cast<int>(target.NetworkId());
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode, reactive) || !ResourceGate(mode, reactive) ||
        !TargetDamageable(target) || PreserveAttack(reactive) ||
        !Engine::ValidEnemy(target, WRange(CurrentForm) + target.BoundingRadius())) return false;
    Vector3 aim = PredictPosition(target, 0.55f);
    if (Engine::RuntimeSpells[1]) {
        const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(target);
        if (prediction.Hitchance < SDK::HitChance::High || prediction.GetCastPosition().IsZero() ||
            !prediction.CollisionObjects.empty()) return false;
        aim = prediction.GetCastPosition();
    }
    if (aim.IsZero() || player.Position().Distance2D(aim) > WRange(CurrentForm) + target.BoundingRadius() ||
        !WLineHits(player.Position(), aim, PredictPosition(target, 0.55f), target.BoundingRadius(), CurrentForm) ||
        ProjectileWallBlocksFromPlayer(aim, kWHalfWidth)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastCastTick[1] = Now();
    LastAutoTargetId = static_cast<int>(target.NetworkId());
    return true;
}

inline bool PathHasWall(const Vector3& origin, const Vector3& endpoint) {
    const float distance = origin.Distance2D(endpoint);
    if (distance <= 1.0f) return false;
    const Vector3 direction = Direction2D(origin, endpoint);
    if (direction.IsZero()) return false;
    for (float offset = 24.0f; offset <= distance; offset += 24.0f)
        if (SDK::NavMesh::IsWall(origin + direction * offset)) return true;
    return false;
}

inline bool CastE(Mode mode, bool reactive = false, bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode, reactive) || !ResourceGate(mode, reactive) ||
        PreserveAttack(reactive) || player.HasBuff("KaynE")) return false;
    Vector3 destination = Game::CursorPos();
    if (!destination.IsValid() || destination.IsZero()) return false;
    destination = ClampDashEndpoint(player.Position(), destination, kERange);
    const bool defensive = fleeing || player.HealthPercent() <= 34.0f;
    const int maximumEnemies = Slider(TacticsMenu, "MaxDashEnemies", 2);
    const WallTraversalContext context{
        true, PathTouchesWall(player.Position(), destination,
                              PathHasWall(player.Position(), destination)),
        true, !SDK::NavMesh::IsWall(destination),
        Engine::UnderEnemyTurret(destination) && !Engine::UnderEnemyTurret(player.Position()),
        Engine::UnderEnemyTurret(player.Position()),
        Engine::CountEnemiesAt(destination, 450.0f), maximumEnemies, defensive};
    if (!WallTraversalAllowed(context)) return false;
    if (!Engine::ControllerCastPosition(2, destination)) return false;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastREntry(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || RHostActive || !Ready(3, mode, reactive) || !ResourceGate(mode, reactive) ||
        !TargetDamageable(target) || !target.HasBuff("KaynREnemyMark")) return false;
    const float range = RHostRange(CurrentForm);
    const bool lethal = LethalPhysical(target, SpellDamage(target, 3));
    const REntryContext context{
        true, true, true, false,
        player.Position().Distance2D(target.Position()) <= range + target.BoundingRadius(),
        lethal, player.HealthPercent() <= 30.0f,
        Engine::UnderEnemyTurret(target.Position()) && !Engine::UnderEnemyTurret(player.Position())};
    if (!REntryAllowed(context) || PreserveAttack(reactive, lethal)) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    RHostActive = true;
    RHostId = static_cast<int>(target.NetworkId());
    RMarkedTargetId = RHostId;
    RCastTick = Now();
    RHostUntil = RCastTick + static_cast<int>(kRInfestDurationSeconds * 1000.0f);
    LastCastTick[3] = RCastTick;
    return true;
}

inline bool CastRRecast(Mode mode, bool reactive = false, bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !RHostActive || !Ready(3, mode, reactive) || !ResourceGate(mode, reactive)) return false;
    const auto host = HeroByNetworkId(RHostId);
    const bool hostValid = Engine::ValidEnemy(host, RHostRange(CurrentForm) + 100.0f);
    const bool lethal = hostValid && LethalPhysical(host, SpellDamage(host, 3));
    const bool defensive = fleeing || player.HealthPercent() <= 30.0f;
    const Vector3 endpoint = ClampDashEndpoint(player.Position(), Game::CursorPos(), RJumpOutRange(CurrentForm));
    const RRecastContext context{
        true, true, !hostValid, !endpoint.IsZero(), endpoint.IsValid() && !SDK::NavMesh::IsWall(endpoint),
        hostValid && Engine::UnderEnemyTurret(endpoint) && !Engine::UnderEnemyTurret(player.Position()),
        lethal, defensive, Now() + 350 >= RHostUntil};
    if (!RRecastAllowed(context) || PreserveAttack(reactive, lethal)) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    RHostActive = false;
    RHostId = 0;
    RHostUntil = 0;
    RCastTick = Now();
    LastCastTick[3] = RCastTick;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!TargetDamageable(target)) return;
    if (CastRRecast(Mode::Combo)) return;
    const bool lethal = LethalPhysical(target, SpellDamage(target, 0));
    if (CastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastREntry(target, Mode::Combo)) return;
    (void)lethal;
}

inline void Harass(const AIHeroClient& target) {
    if (!TargetDamageable(target) || PlayerManaPercent() < Slider(TacticsMenu, "HarassMana", 45)) return;
    if (CastW(target, Mode::Harass)) return;
    (void)CastQ(target, Mode::Harass);
}

inline void Farm(Mode mode, const AIHeroClient& target) {
    if (!ResourceGate(mode) || PlayerManaPercent() < Slider(FarmMenu, "FarmMana", 25)) return;
    if (Engine::ValidEnemy(target) && CastQ(target, mode)) return;
    if (Engine::ValidEnemy(target) && CastW(target, mode)) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (CastE(Mode::Flee, true, true)) return;
    if (RHostActive && CastRRecast(Mode::Flee, true, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastQ(target, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const bool threat = IncomingThreatUntil >= Now();
    if (threat && CastE(Mode::Automatic, true, true)) return;
    if (RHostActive && (player.HealthPercent() <= 28.0f || Now() + 350 >= RHostUntil) &&
        CastRRecast(Mode::Automatic, true, true)) return;
    if (Engine::ValidEnemy(target) && LethalPhysical(target, SpellDamage(target, 0)))
        (void)CastQ(target, Mode::Automatic, true);
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const bool assassin = player.HasBuff("KaynAssReady");
    const bool slayer = player.HasBuff("KaynSlayReady");
    const Form observed = ResolveForm(assassin, slayer);
    if (observed != Form::Untransformed) {
        CurrentForm = observed;
        Transforming = false;
        Orbs.CurrentForm = observed;
        TransformStartTick = 0;
    } else if (player.HasBuff("KaynTransforming")) {
        Transforming = true;
        if (TransformStartTick == 0) TransformStartTick = now;
        Orbs.Transforming = true;
    } else if (Transforming && now - TransformStartTick > kTransformingTimeoutMs) {
        Transforming = false;
        Orbs.Transforming = false;
        TransformStartTick = 0;
    }
    if (RHostActive && (RHostUntil > 0 && now > RHostUntil + 250)) {
        RHostActive = false;
        RHostId = 0;
    }
    RMarkedTargetId = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        if (enemy.HasBuff("KaynREnemyMark")) RMarkedTargetId = static_cast<int>(enemy.NetworkId());
        if (enemy.HasBuff("KaynRHost")) {
            RHostActive = true;
            RHostId = static_cast<int>(enemy.NetworkId());
            RHostUntil = std::max(RHostUntil, now + 250);
        }
    }
    if (IncomingThreatUntil < now) {
        IncomingThreatUntil = 0;
        IncomingThreatTargetId = 0;
        IncomingThreatEndpoint = {};
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = PreferredEnemyTarget(selected, std::max(kRAssassinRange, WRange(CurrentForm)) + 100.0f);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode, target); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Kayn form tactics"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Kayn farming"));
    TacticsMenu->Add(new MenuSlider("MaxDashEnemies", "Maximum enemies at dash endpoint", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Minimum harass mana percent", 45, 0, 100));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Manual cast protection (ms)", 650, 0, 2000));
    TacticsMenu->Add(new MenuBool("PreserveAttacks", "Preserve attack windup", true));
    FarmMenu->Add(new MenuSlider("FarmMana", "Minimum farm mana percent", 25, 0, 100));
}

inline void OnLoad() {
    TacticsMenu = FarmMenu = nullptr;
    Orbs = {};
    CurrentForm = Form::Untransformed;
    Transforming = RHostActive = false;
    RHostId = RMarkedTargetId = TransformStartTick = RCastTick = RHostUntil = 0;
    ManualOwnershipUntil = IncomingThreatUntil = IncomingThreatTargetId = 0;
    IncomingThreatEndpoint = {};
    LastAutoTargetId = LastAutoTick = 0;
    LastCastTick.fill(0);
}

inline void OnUnload() { OnLoad(); }

inline void ObserveOrbTarget(const AIBaseClient& target) {
    if (CurrentForm != Form::Untransformed || Transforming ||
        !target.IsValid() || !target.IsHero()) return;
    const auto player = GameObjects::Player();
    const OrbKind kind = player.Position().Distance2D(target.Position()) > 400.0f
        ? OrbKind::Ranged : OrbKind::Melee;
    Orbs = AddOrb(Orbs, kind);
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot < 0 || slot > 3) return;
        if (!Engine::WasControllerCast(slot))
            ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 650);
        LastCastTick[static_cast<std::size_t>(slot)] = now;
        if (slot == 3 && !RHostActive && args.TargetNetworkId != 0) {
            RHostId = static_cast<int>(args.TargetNetworkId);
            RHostActive = true;
            RHostUntil = now + static_cast<int>(kRInfestDurationSeconds * 1000.0f);
        }
        if ((slot == 0 || slot == 1) && args.Target.IsValid())
            ObserveOrbTarget(ControllerHelpers::UnitByNetworkId(
                static_cast<int>(args.Target.NetworkId)));
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatTargetId = static_cast<int>(args.Sender.NetworkId);
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
        IncomingThreatEndpoint = args.EndPosition;
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return;
    LastAutoTargetId = static_cast<int>(
        args.TargetNetworkId != 0 ? args.TargetNetworkId : args.Target.NetworkId);
    LastAutoTick = Now();
    ObserveOrbTarget(ControllerHelpers::UnitByNetworkId(
        static_cast<int>(args.TargetNetworkId != 0
            ? args.TargetNetworkId : args.Target.NetworkId)));
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const bool local = IsLocalPlayer(args.Sender);
    if (local) {
        if (Engine::TextContains(args.BuffName, "KaynTransforming")) {
            Transforming = true;
            TransformStartTick = Now();
            Orbs.Transforming = true;
        } else if (Engine::TextContains(args.BuffName, "KaynAssReady")) {
            CurrentForm = Orbs.CurrentForm = Form::ShadowAssassin;
            Transforming = Orbs.Transforming = false;
        } else if (Engine::TextContains(args.BuffName, "KaynSlayReady")) {
            CurrentForm = Orbs.CurrentForm = Form::DarkinSlayer;
            Transforming = Orbs.Transforming = false;
        } else if (Engine::TextContains(args.BuffName, "KaynRHost")) {
            RHostActive = true;
            RHostUntil = Now() + static_cast<int>(kRInfestDurationSeconds * 1000.0f);
        }
    } else if (Engine::TextContains(args.BuffName, "KaynREnemyMark")) {
        RMarkedTargetId = static_cast<int>(args.Sender.NetworkId);
    } else if (Engine::TextContains(args.BuffName, "KaynRHost")) {
        RHostActive = true;
        RHostId = static_cast<int>(args.Sender.NetworkId);
        RHostUntil = Now() + static_cast<int>(kRInfestDurationSeconds * 1000.0f);
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "KaynTransforming")) Transforming = Orbs.Transforming = false;
        if (Engine::TextContains(args.BuffName, "KaynRHost")) RHostActive = false;
    } else if (Engine::TextContains(args.BuffName, "KaynREnemyMark") &&
               RMarkedTargetId == static_cast<int>(args.Sender.NetworkId)) {
        RMarkedTargetId = 0;
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (args.EndTime <= Game::Time()) OnBuffRemove(args);
    else OnBuffAdd(args);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
        LastAutoTick = Now();
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, IncomingThreatTargetId, IncomingThreatEndpoint,
                           IncomingThreatUntil, kERange, 1100);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, IncomingThreatTargetId, IncomingThreatUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "Riot 26.15 and CommunityDragon 16.15 Kayn route values",
    "Track melee and ranged passive orbs before transformation",
    "Reconcile KaynTransforming and KaynAssReady/KaynSlayReady form transitions by polling and buffs",
    "Preserve manual ownership and attack windup around every cast",
    "Dash Q to a bounded endpoint, slash the predicted target and reject blocked or unsafe routes",
    "Predict W line impact, reject collision and projectile-wall failures, and model Darkin knockup",
    "Traverse E only across a real wall path with walkable, turret and enemy-count endpoint gates",
    "Use Shadow Assassin and Darkin Slayer range, damage and mobility differences",
    "Enter R only on a marked, damageable host within form-aware range",
    "Recast R for lethal, defensive, expiring-host or fleeing exits with safe landing checks",
    "Reconcile host, mark, transformation and threat state from events and polling",
    "Prefer selected target then orbwalker target before engine fallback",
    "Cover Combo, Harass, LaneClear, Jungle, LastHit, Flee and conservative Automatic modes",
    "Apply mana, cooldown, turret, collision, wall and nearby-enemy safety policy",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Kayn;
    controller.ControllerId = "champion.kuroaio.ai.kayn.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIKayn.md";
    controller.ImplementationSummary =
        "Form/orb and transformation reconciliation, Q dash/slash, prediction-aware W knockup, "
        "wall-gated E traversal and marked-host R entry/recast with form-aware safety.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &OnDoCast;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &OnBuffUpdate;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Kayn
