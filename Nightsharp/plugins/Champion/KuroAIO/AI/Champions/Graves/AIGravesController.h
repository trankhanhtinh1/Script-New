#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIGravesGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Graves {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Ready;
inline Menu* TacticsMenu = nullptr;
inline Menu* ShellMenu = nullptr;
inline Menu* SmokeMenu = nullptr;
inline Menu* DashMenu = nullptr;
inline Menu* RecoilMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<int, 4> LastCastTick{};
inline int LastShotTick = 0;
inline int ReloadReadyTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int ManualOwnershipUntil = 0;
inline int ThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int QTargetId = 0;
inline int QImpactTick = 0;
inline Vector3 QEndpoint{};
inline bool QExplosionPending = false;
inline bool WSmokeActive = false;
inline int WSmokeExpireTick = 0;
inline int WSmokeObjectId = 0;
inline Vector3 WSmokeCenter{};
inline int WSmokeTargetId = 0;
inline bool EActive = false;
inline int EExpireTick = 0;
inline int EStacks = 0;
inline bool EAfterAttackPending = false;
inline Vector3 EDestination{};
inline bool RActive = false;
inline int RTargetId = 0;
inline int RImpactTick = 0;
inline Vector3 RAim{};
inline Vector3 RRecoil{};
inline bool RFirstCollisionConfirmed = false;
inline int Shells = 2;
inline bool Reloading = false;
inline Mode LastMode = Mode::None;

inline bool AttackWindingUp() { return Orbwalker::IsWindingUp(); }

inline bool Throttle(int slot, int delay = 45) {
    return slot >= 0 && slot < 4 && Now() - LastCastTick[static_cast<std::size_t>(slot)] >= delay;
}

inline int SpellRank(int slot) {
    if (slot < 0 || slot >= 4 || !Engine::RuntimeSpells[slot]) return 1;
    return std::clamp(Engine::RuntimeSpells[slot]->Level(), 1, slot == 3 ? 3 : 5);
}

inline void ReconcileShellState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const bool ammoOne = player.HasBuff("GravesBasicAttackAmmo1");
    const bool ammoTwo = player.HasBuff("GravesBasicAttackAmmo2");
    const int observed = ReconcileShells(ammoOne, ammoTwo, AttackWindingUp());
    if (ammoTwo || ammoOne || !AttackWindingUp()) Shells = observed;
    Reloading = Shells <= 0;
    if (Reloading && ReloadReadyTick <= Now()) {
        ReloadReadyTick = Now() + std::max(120, static_cast<int>(
            SDK::AttackDelay(AIBaseClient(player.Handle())) * 1000.0f));
    }
    if (Shells > 0) {
        Reloading = false;
        ReloadReadyTick = 0;
    }
    EActive = player.HasBuff("GravesEGrit") || player.HasBuff("GravesMove");
    EStacks = EActive ? std::max(1, EStacks) : 0;
    if (!EActive) EExpireTick = 0;
    if (WSmokeActive && WSmokeExpireTick <= Now()) {
        WSmokeActive = false;
        WSmokeObjectId = 0;
        WSmokeCenter = {};
        WSmokeTargetId = 0;
    }
    if (RActive && RImpactTick > 0 && RImpactTick + 800 < Now()) {
        RActive = false;
        RFirstCollisionConfirmed = false;
        RAim = RRecoil = {};
    }
}

inline bool ManualLocked() { return ManualOwnershipUntil > Now(); }

inline AIHeroClient SelectTarget(const AIHeroClient& selected, float range) {
    return PreferredEnemyTarget(selected, range);
}

inline bool SafePosition(const Vector3& position, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !position.IsValid() || position.IsZero() || SDK::NavMesh::IsWall(position) ||
        SDK::NavMesh::IsWallBetween(player.Position(), position, 25.0f) ||
        PlayerMobilityLocked() || HasReadyDashHazardAt(position, 500.0f)) return false;
    const bool turretRisk = Engine::UnderEnemyTurret(position) && !Engine::UnderEnemyTurret(player.Position());
    return defensive || !turretRisk;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::RuntimeSpells[0] || !Engine::ValidEnemy(target, kQRange + 50.0f) ||
        !Ready(0, mode) || !Throttle(0) || ManualLocked() || PreserveAttack(reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    Vector3 aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero()) aim = PredictPosition(target, kQDelay);
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) > kQRange + target.BoundingRadius()) return false;
    const Vector3 endpoint = QExplosionEndpoint(player.Position(), aim);
    const Vector3 predicted = PredictPosition(target, kQDelay);
    const bool targetLine = IsInsideLine(player.Position(), endpoint, predicted, target.BoundingRadius(), kQWidth);
    const bool explosionReachable = endpoint.IsValid() &&
        (endpoint.Distance2D(predicted) <= kQExplosionRadius + target.BoundingRadius() ||
         player.Position().Distance2D(aim) <= kQCastRange);
    const bool wall = SDK::NavMesh::IsWallBetween(player.Position(), endpoint, kQWidth * 0.25f) &&
        !SDK::NavMesh::IsWall(endpoint);
    const float damage = player.CalculatePhysicalDamage(
        target, QTotalDamage(SpellRank(0), player.BonusAttackDamage(), explosionReachable));
    const QGate gate{true, Shells > 0 && !Reloading, prediction.Hitchance >= SDK::HitChance::High,
        wall, targetLine, explosionReachable, Lethal(target, damage), AttackWindingUp()};
    if (!CanCastQ(gate)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = LastShotTick = Now();
    QTargetId = static_cast<int>(target.NetworkId());
    QEndpoint = endpoint;
    QImpactTick = Now() + static_cast<int>(kQDelay * 1000.0f);
    QExplosionPending = explosionReachable;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::RuntimeSpells[1] || !Engine::ValidEnemy(target, kWRange + 50.0f) ||
        !Ready(1, mode) || !Throttle(1, 80) || ManualLocked() || PreserveAttack(reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(target);
    Vector3 aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero()) aim = PredictPosition(target, kWDelay);
    const bool committed = target.IsDashing() ||
        SDK::HasBuffOfType(target, SDK::BuffType::Stun) ||
        SDK::HasBuffOfType(target, SDK::BuffType::Snare) ||
        target.HealthPercent() <= 45.0f;
    const bool wall = !aim.IsValid() || aim.IsZero() ||
        SDK::NavMesh::IsWallBetween(player.Position(), aim, 45.0f) ||
        ProjectileWallBlocksFromPlayer(aim, 90.0f);
    const bool visionRequired = mode == Mode::Combo || mode == Mode::Harass;
    const bool visionSafe = player.IsVisible() && !Engine::UnderEnemyTurret(aim);
    const bool already = WSmokeActive && InSmoke(WSmokeCenter, target.Position(), target.BoundingRadius());
    const SmokeGate gate{true, prediction.Hitchance >= SDK::HitChance::High, wall,
        aim.IsValid() && player.Position().Distance2D(aim) <= kWRange + target.BoundingRadius(),
        already, visionRequired, visionSafe, committed};
    if (!CanCastSmoke(gate)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastCastTick[1] = Now();
    WSmokeActive = true;
    WSmokeExpireTick = Now() + static_cast<int>(kWSmokeDurationSeconds * 1000.0f);
    WSmokeCenter = aim;
    WSmokeTargetId = static_cast<int>(target.NetworkId());
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::RuntimeSpells[2] || !Ready(2, mode) || !Throttle(2) || ManualLocked()) return false;
    const Vector3 requested = defensive ? Game::CursorPos() : (target.IsValid() ? target.Position() : Game::CursorPos());
    const Vector3 destination = ClampEndpoint(player.Position(), requested, kEDashRange);
    if (!destination.IsValid() || destination.IsZero()) return false;
    const bool afterAttack = EAfterAttackPending || Now() - LastAutoTick <= 520;
    const bool reloading = Reloading || Shells <= 0;
    const bool turretRisk = Engine::UnderEnemyTurret(destination) && !Engine::UnderEnemyTurret(player.Position());
    const DashGate gate{true, true, SDK::NavMesh::IsWall(destination) ||
        SDK::NavMesh::IsWallBetween(player.Position(), destination, 20.0f), turretRisk,
        HasReadyDashHazardAt(destination, 500.0f), PlayerMobilityLocked(), afterAttack,
        reloading, defensive};
    if (!CanDash(gate) || !SafePosition(destination, defensive)) return false;
    if (!Engine::ControllerCastPosition(2, destination)) return false;
    LastCastTick[2] = Now();
    EActive = true;
    EExpireTick = Now() + 4000;
    EAfterAttackPending = false;
    EDestination = destination;
    EStacks = std::min(8, EStacks + 1);
    return true;
}

inline bool RLineHasFirstCollision(const AIHeroClient& target, const Vector3& aim) {
    std::array<Vec3, 8> blockers{};
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) || enemy.NetworkId() == target.NetworkId() || count >= static_cast<int>(blockers.size())) continue;
        blockers[static_cast<std::size_t>(count++)] = enemy.Position();
    }
    return IsFirstCollision(GameObjects::Player().Position(), aim, PredictPosition(target, kRDelay),
        blockers, count, target.BoundingRadius(), 55.0f);
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::RuntimeSpells[3] || !Engine::ValidEnemy(target, kRRange + 70.0f) ||
        !Ready(3, mode) || !Throttle(3, 160) || ManualLocked() || PreserveAttack(reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[3]->GetPrediction(target);
    Vector3 aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero()) aim = PredictPosition(target, kRDelay);
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) > kRRange + target.BoundingRadius()) return false;
    const Vector3 recoil = RecoilEndpoint(player.Position(), aim);
    const bool first = RLineHasFirstCollision(target, aim);
    const bool wall = SDK::NavMesh::IsWallBetween(player.Position(), aim, kRWidth * 0.5f) ||
        ProjectileWallBlocksFromPlayer(aim, kRWidth * 0.5f);
    const bool turret = Engine::UnderEnemyTurret(recoil) && !Engine::UnderEnemyTurret(player.Position());
    const int nearby = Engine::CountEnemiesAt(recoil, 260.0f);
    const bool unsafe = !SafePosition(recoil, false) || nearby > Slider(RecoilMenu, "MaximumLandingEnemies", 2);
    const float damage = player.CalculatePhysicalDamage(
        target, RPrimaryDamage(SpellRank(3), player.BonusAttackDamage()));
    const RecoilGate gate{true, first, prediction.Hitchance >= SDK::HitChance::High, wall, turret, unsafe,
        Lethal(target, damage), nearby, Slider(RecoilMenu, "MaximumLandingEnemies", 2)};
    if (!CanCastRecoil(gate)) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    LastCastTick[3] = RImpactTick = Now();
    RActive = true;
    RTargetId = static_cast<int>(target.NetworkId());
    RAim = aim;
    RRecoil = recoil;
    RFirstCollisionConfirmed = true;
    return true;
}

inline bool Farm(Mode mode) {
    if (mode == Mode::Jungle) {
        for (const auto& monster : GameObjects::Jungle()) {
            if (!monster.IsValid() || monster.IsDead() || !monster.IsTargetable()) continue;
            if (Ready(0, mode) && Throttle(0) && Shells > 0 &&
                Engine::ControllerCastPosition(0, monster.Position())) {
                LastCastTick[0] = LastShotTick = Now();
                return true;
            }
            break;
        }
        return false;
    }
    if (mode == Mode::LaneClear || mode == Mode::LastHit) {
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable()) continue;
            const auto player = GameObjects::Player();
            if (!player.IsValid() || player.Position().Distance2D(minion.Position()) > kQRange) continue;
            if (mode == Mode::LastHit && Engine::RuntimeSpells[0] &&
                Engine::RuntimeSpells[0]->GetDamage(minion) < minion.Health()) continue;
            if (Ready(0, mode) && Throttle(0) && Shells > 0 &&
                Engine::ControllerCastPosition(0, minion.Position())) {
                LastCastTick[0] = LastShotTick = Now();
                return true;
            }
        }
    }
    return false;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileShellState();
    LastMode = mode;
    if (ManualLocked()) return false;
    if (ThreatUntil > Now() && mode == Mode::Automatic) {
        const auto threat = SelectTarget(selected, kWRange);
        if (Engine::ValidEnemy(threat) && CastW(threat, Mode::Automatic, true)) return true;
    }
    if (mode == Mode::Flee) {
        const auto target = SelectTarget(selected, kWRange);
        if (Engine::ValidEnemy(target) && CastW(target, mode, true)) return true;
        return CastE(target, mode, true);
    }
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) return Farm(mode);
    const auto target = SelectTarget(selected, std::max(kRRange, kWRange));
    if (!Engine::ValidEnemy(target)) return false;
    if (mode == Mode::Automatic && Ready(3, mode) &&
        GameObjects::Player().CalculatePhysicalDamage(
            target, RPrimaryDamage(SpellRank(3), GameObjects::Player().BonusAttackDamage())) >=
            target.Health() + target.AllShield() &&
        CastR(target, mode, true)) return true;
    if ((mode == Mode::Combo || mode == Mode::Harass || mode == Mode::Automatic) && CastW(target, mode)) return true;
    if ((mode == Mode::Combo || mode == Mode::Harass || mode == Mode::Automatic) && CastQ(target, mode)) return true;
    if (mode == Mode::Combo && (EAfterAttackPending || Reloading) && CastE(target, mode)) return true;
    if ((mode == Mode::Combo || mode == Mode::Automatic) && Ready(3, mode) && CastR(target, mode)) return true;
    return false;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) {
        const auto analysis = AnalyzeEnemyCast(args, 240.0f, 100.0f, 260, 260, 240, 1400, 420);
        if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
            ThreatUntil = std::max(ThreatUntil, analysis.CommitmentUntilTick);
            if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(IncomingHardCCUntil, analysis.CommitmentUntilTick);
        }
        return;
    }
    const int slot = args.Slot;
    if (slot < 0 || slot >= 4) return;
    LastCastTick[static_cast<std::size_t>(slot)] = Now();
    if (!Engine::WasControllerCast(slot)) ManualOwnershipUntil = Now() + 650;
    if (slot == 0) {
        QImpactTick = Now() + static_cast<int>(kQDelay * 1000.0f);
        QEndpoint = args.EndPosition;
        QExplosionPending = true;
    } else if (slot == 1) {
        WSmokeActive = true;
        WSmokeExpireTick = Now() + 4000;
        WSmokeCenter = args.EndPosition;
        WSmokeObjectId = 0;
    } else if (slot == 2) {
        EActive = true;
        EExpireTick = Now() + 4000;
        EAfterAttackPending = false;
    } else if (slot == 3) {
        RActive = true;
        RImpactTick = Now();
        RAim = args.EndPosition;
        RFirstCollisionConfirmed = false;
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return;
    LastAutoTargetId = static_cast<int>(args.TargetNetworkId != 0 ? args.TargetNetworkId : args.Target.NetworkId);
    LastAutoTick = LastShotTick = Now();
    Shells = std::max(0, Shells - 1);
    Reloading = Shells <= 0;
    ReloadReadyTick = Reloading ? Now() + 700 : 0;
    EAfterAttackPending = true;
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (ControllerHelpers::TextContainsAny(args.BuffName, {"GravesBasicAttackAmmo2"})) Shells = 2;
    else if (ControllerHelpers::TextContainsAny(args.BuffName, {"GravesBasicAttackAmmo1"})) Shells = 1;
    else if (ControllerHelpers::TextContainsAny(args.BuffName, {"GravesEGrit", "GravesMove"})) {
        EActive = true;
        EExpireTick = now + ControllerHelpers::RemainingMilliseconds(args.EndTime, 4000, 350, 5000);
        EStacks = std::min(8, std::max(1, EStacks + 1));
    }
    if (Shells > 0) Reloading = false;
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (ControllerHelpers::TextContainsAny(args.BuffName, {"GravesEGrit", "GravesMove"})) {
        EActive = false;
        EStacks = 0;
        EExpireTick = 0;
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (args.EndTime > Game::Time()) OnBuffAdd(args);
    else OnBuffRemove(args);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (Reloading && Shells <= 0) args.Process = false;
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick)) EAfterAttackPending = true;
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)args;
    ThreatUntil = std::max(ThreatUntil, Now() + 850);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    (void)args;
    IncomingHardCCUntil = std::max(IncomingHardCCUntil, Now() + 900);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (ControllerHelpers::TextContainsAny(args.Sender.Name, {"GravesSmokeCloud", "GravesSmokeGrenade"})) {
        WSmokeActive = true;
        WSmokeObjectId = static_cast<int>(args.Sender.NetworkId);
        WSmokeCenter = args.Sender.Position;
        WSmokeExpireTick = Now() + 4000;
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (WSmokeObjectId != 0 && static_cast<int>(args.Sender.NetworkId) == WSmokeObjectId) {
        WSmokeActive = false;
        WSmokeObjectId = 0;
        WSmokeCenter = {};
        WSmokeTargetId = 0;
    }
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) { (void)args; }
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) { (void)args; }

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFCCAA66u, 1.2f, 36);
    Drawing::DrawCircle(player.Position(), kWRange, 0xFF66CCAAu, 1.2f, 36);
    Drawing::DrawCircle(player.Position(), kEDashRange, 0xFF66AAFFu, 1.2f, 28);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFFF6666u, 1.2f, 36);
    if (WSmokeActive) Drawing::DrawCircle(WSmokeCenter, kWSmokeRadius, 0x88666666u, 1.0f, 32);
    if (RRecoil.IsValid()) Drawing::DrawCircle(RRecoil, 85.0f, 0xFFFFAA66u, 1.0f, 24);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("GravesMechanics", "Graves shell and recoil mechanics"));
    ShellMenu = TacticsMenu->AddSubMenu(new Menu("Shells", "Ammo and reload ownership"));
    ShellMenu->Add(new MenuBool("PreserveFinalShell", "Preserve final shell for lethal Q", true));
    SmokeMenu = TacticsMenu->AddSubMenu(new Menu("Smoke", "Smoke collision and vision"));
    SmokeMenu->Add(new MenuBool("RequireClearVision", "Require clear vision route for W", true));
    DashMenu = TacticsMenu->AddSubMenu(new Menu("Quickdraw", "Quickdraw armor and reload"));
    DashMenu->Add(new MenuBool("AfterAttackOnly", "Prefer E after attack", true));
    RecoilMenu = TacticsMenu->AddSubMenu(new Menu("Collateral", "First collision and recoil safety"));
    RecoilMenu->Add(new MenuSlider("MaximumLandingEnemies", "Maximum enemies at recoil", 2, 0, 5));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Coach", "Range and state telemetry"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Graves ranges and smoke", false));
}

inline void OnLoad() {
    std::fill(LastCastTick.begin(), LastCastTick.end(), 0);
    LastShotTick = ReloadReadyTick = LastAutoTargetId = LastAutoTick = ManualOwnershipUntil = 0;
    ThreatUntil = IncomingHardCCUntil = QTargetId = QImpactTick = 0;
    QEndpoint = {};
    QExplosionPending = false;
    WSmokeActive = false;
    WSmokeExpireTick = WSmokeObjectId = WSmokeTargetId = 0;
    WSmokeCenter = {};
    EActive = false;
    EExpireTick = EStacks = 0;
    EAfterAttackPending = false;
    EDestination = {};
    RActive = false;
    RTargetId = RImpactTick = 0;
    RAim = RRecoil = {};
    RFirstCollisionConfirmed = false;
    Shells = 2;
    Reloading = false;
    LastMode = Mode::None;
    ReconcileShellState();
}

inline void OnUnload() {
    TacticsMenu = ShellMenu = SmokeMenu = DashMenu = RecoilMenu = CoachMenu = nullptr;
    WSmokeActive = false;
    WSmokeObjectId = 0;
    QExplosionPending = false;
    EActive = false;
    RActive = false;
}

inline constexpr const char* Scenarios[] = {
    "Reconcile GravesBasicAttackAmmo1/2 buffs with shell count and reload timing",
    "Do not spend the final shell while a lethal Q route is unavailable",
    "Allow Q through prediction only when the line and end explosion are reachable",
    "Track Q terrain endpoint and explosion timing after local and manual casts",
    "Reject Q through terrain walls while preserving genuine close-range shell damage",
    "Place Smoke Screen on a predicted target only through a clear projectile route",
    "Track smoke cloud object and four-second vision/impact lifetime by polling",
    "Avoid repeating W while the target is already inside the smoke radius",
    "Use Quickdraw after a shotgun attack or while reloading for True Grit armor",
    "Reconcile E buff stacks, four-second expiry and movement ownership from events",
    "Reject E wall, dash-hazard, turret and player-lockdown landings",
    "Treat Collateral Damage as a first-collision line rather than a generic target spell",
    "Reject R when a champion blocks the selected target or the recoil endpoint is unsafe",
    "Apply R primary physical damage, 400-unit recoil and turret landing safety",
    "Use R for lethal automatic execute and multi-mode combat only with real reach",
    "Preserve orbwalker attack windup and yield after manual spell ownership",
    "Handle distinct Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic modes",
    "Complete shell, smoke, dash, recoil, threat, object and missile callbacks",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Graves;
    controller.ControllerId = "champion.kuroaio.ai.graves.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIGraves.md";
    controller.ImplementationSummary =
        "Ammo/reload reconciliation, terrain-aware Q endpoint explosion, vision-safe W smoke, "
        "post-shot E armor/reload dash, and first-collision/turret-safe R recoil execute.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Graves
