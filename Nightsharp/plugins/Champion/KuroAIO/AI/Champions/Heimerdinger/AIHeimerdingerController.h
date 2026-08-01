#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIHeimerdingerGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Heimerdinger {

using namespace Geometry;
using ControllerHelpers::AP;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::Protected;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Now;

inline Menu* TacticsMenu = nullptr;
inline Menu* TurretMenu = nullptr;
inline Menu* RocketMenu = nullptr;
inline Menu* GrenadeMenu = nullptr;
inline Menu* UpgradeMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<TurretObject, 3> Turrets{};
inline int ObservedQAmmo = 0;
inline int ObservedQMaxAmmo = kMaximumTurrets;
inline bool QAmmoObserved = false;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int PlayerOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline bool RActive = false;
inline int RExpireTick = 0;
inline UpgradeChoice ActiveUpgrade = UpgradeChoice::None;
inline Vector3 LastQPosition{};
inline Vector3 LastWPosition{};
inline Vector3 LastEPosition{};
inline Vector3 LastRPosition{};

inline bool Ready(int slot, Mode mode) {
    return ControllerHelpers::ControllerSpellAvailable(slot, mode);
}
inline bool Throttle(int slot, int delay = 60) {
    const int ticks[] = {LastQCastTick, LastWCastTick, LastECastTick, LastRCastTick};
    return slot >= 0 && slot < 4 && ticks[slot] + delay <= Now();
}
inline bool Lethal(const AIHeroClient& target, float rawDamage) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target) &&
           player.CalculateMagicDamage(target, rawDamage) >=
               target.Health() + target.AllShield();
}
inline int RuntimeQAmmo() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::Q);
    if (spell.IsValid() && spell.MaxAmmo() > 0 && spell.MaxAmmo() <= 3 &&
        spell.Ammo() >= 0) {
        ObservedQMaxAmmo = std::clamp(spell.MaxAmmo(), 1, kMaximumTurrets);
        ObservedQAmmo = std::clamp(spell.Ammo(), 0, ObservedQMaxAmmo);
        QAmmoObserved = true;
    } else if (!QAmmoObserved) {
        ObservedQAmmo = Ready(0, Mode::Automatic) ? 1 : 0;
    }
    return std::clamp(ObservedQAmmo, 0, ObservedQMaxAmmo);
}
inline int LiveTurretCount() {
    int count = 0;
    for (const auto& turret : Turrets) {
        if (turret.Valid) ++count;
    }
    return count;
}
inline TurretZone CurrentZone() {
    TurretZone zone{};
    Vec3 sum{};
    int count = 0;
    for (const auto& turret : Turrets) {
        if (!turret.Valid) continue;
        sum = sum + turret.Position;
        ++count;
        zone.HasSuperTurret = zone.HasSuperTurret || turret.Super;
    }
    if (count > 0) {
        zone.Center = sum / static_cast<float>(count);
        zone.TurretCount = count;
        zone.ReadyTurretCount = count;
    } else {
        const auto player = GameObjects::Player();
        zone.Center = player.IsValid() ? player.Position() : Vec3{};
    }
    return zone;
}
inline void ReconcileState() {
    const int now = Now();
    RuntimeQAmmo();
    for (auto& turret : Turrets) {
        if (turret.Valid && turret.ExpireTick > 0 && now > turret.ExpireTick) {
            turret = {};
        }
    }
    if (RActive && now > RExpireTick) {
        RActive = false;
        RExpireTick = 0;
        ActiveUpgrade = UpgradeChoice::None;
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("HeimerdingerR") || player.HasBuff("HeimerdingerRActive")) {
        RActive = true;
        RExpireTick = std::max(RExpireTick, now + 1200);
    }
    if (player.HasBuff("HeimerdingerQAmmo")) {
        ObservedQAmmo = std::clamp(player.GetBuffCount("HeimerdingerQAmmo"), 0,
                                   ObservedQMaxAmmo);
        QAmmoObserved = true;
    }
}
inline bool SafePlacement(const Vector3& requested, bool defensive = false,
                          bool lethal = false, bool superTurret = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || requested.IsZero() || !requested.IsValid()) return false;
    const float range = superTurret ? kRPlacementRange : kQPlacementRange;
    const PlacementContext context{
        true,
        !SDK::NavMesh::IsWall(requested),
        player.Position().Distance2D(requested) <= range + 12.0f,
        SDK::NavMesh::IsWall(requested),
        Engine::UnderEnemyTurret(requested),
        Engine::CountEnemiesAt(requested, 420.0f) >
            Slider(TurretMenu, "MaxPlacementEnemies", 2),
        Orbwalker::IsWindingUp(),
        LiveTurretCount(),
        superTurret ? kRMaximumTurrets : std::max(1, ObservedQMaxAmmo)};
    return ShouldPlaceTurret(context, defensive, lethal);
}
inline Vector3 Predicted(const AIHeroClient& target, int slot, float delay) {
    if (!Engine::ValidEnemy(target)) return {};
    Vector3 aim = PredictPosition(target, delay);
    if (slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot]) {
        const auto prediction = Engine::RuntimeSpells[slot]->GetPrediction(target);
        if (prediction.Hitchance >= SDK::HitChance::High &&
            prediction.GetCastPosition().IsValid() &&
            !prediction.GetCastPosition().IsZero()) {
            aim = prediction.GetCastPosition();
        }
    }
    return aim;
}
inline bool HasRocketCollision(const AIHeroClient& target, const Vector3& aim) {
    std::array<Vec3, 40> blockers{};
    std::size_t count = 0;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (count >= blockers.size()) break;
        if (minion.IsValid() && !minion.IsDead() &&
            minion.NetworkId() != target.NetworkId()) {
            blockers[count++] = PredictPosition(minion, kWDelay);
        }
    }
    return !RocketLineClear(GameObjects::Player().Position(), aim,
                            blockers.data(), count, target.BoundingRadius());
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool defensive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || RuntimeQAmmo() <= 0 || !Ready(0, mode) ||
        !Throttle(0) || !Engine::CanAct(false) || Orbwalker::IsWindingUp() ||
        Protected(target)) return false;
    const Vector3 requested = Engine::ValidEnemy(target)
        ? target.Position() : Game::CursorPos();
    const Vector3 position = ClampPlacement(player.Position(), requested);
    const PlacementContext context{
        true,
        !SDK::NavMesh::IsWall(position),
        player.Position().Distance2D(position) <= kQPlacementRange + 12.0f,
        SDK::NavMesh::IsWall(position),
        Engine::UnderEnemyTurret(position),
        Engine::CountEnemiesAt(position, 420.0f) >
            Slider(TurretMenu, "MaxPlacementEnemies", 2),
        Orbwalker::IsWindingUp(), LiveTurretCount(), ObservedQMaxAmmo};
    if (!ShouldPlaceTurret(context, defensive, lethal) ||
        !Engine::ControllerCastPosition(0, position)) return false;
    LastQPosition = position;
    LastQCastTick = Now();
    ObservedQAmmo = std::max(0, RuntimeQAmmo() - 1);
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || !Ready(1, mode) ||
        !Throttle(1, reactive ? 25 : 70) || Protected(target)) return false;
    const Vector3 aim = Predicted(target, 1, kWDelay);
    if (aim.IsZero() || player.Position().Distance2D(aim) >
        kWRange + target.BoundingRadius() || HasRocketCollision(target, aim)) return false;
    const bool wallBlocked = ControllerHelpers::ProjectileWallBlocksFromPlayer(
        aim, kWRocketWidth * 0.5f);
    const RocketContext context{
        true, true, !aim.IsZero(), wallBlocked,
        lethal || Lethal(target, WRawFiveRocketDamage(SpellRank(1), AP())), reactive,
        false, Orbwalker::IsWindingUp()};
    if (!ShouldFireRockets(context) || !Engine::ControllerCastPosition(1, aim)) return false;
    LastWPosition = aim;
    LastWCastTick = Now();
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool defensive = false, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || !Ready(2, mode) ||
        !Throttle(2, reactive ? 25 : 75) || Protected(target)) return false;
    const Vector3 aim = Predicted(target, 2, kEDelay);
    const bool center = GrenadeCenterHit(aim, PredictPosition(target, kEDelay),
                                         target.BoundingRadius());
    const bool wallBlocked = ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 12.0f);
    const GrenadeContext context{
        true, true, !aim.IsZero(), wallBlocked, center,
        Engine::UnderEnemyTurret(aim), defensive,
        lethal || Lethal(target, ERawDamage(SpellRank(2), AP()))};
    if (aim.IsZero() || player.Position().Distance2D(aim) > kERange + 30.0f ||
        !ShouldThrowGrenade(context) || !Engine::ControllerCastPosition(2, aim)) return false;
    LastEPosition = aim;
    LastECastTick = Now();
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool defensive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 120)) return false;
    if (!RActive) {
        if (Engine::ValidEnemy(target) && !defensive && !lethal &&
            target.HealthPercent() > Slider(UpgradeMenu, "StartUpgradeBelowHP", 48)) return false;
        if (!Engine::ControllerCastSelf(3)) return false;
        RActive = true;
        RExpireTick = Now() + 1200;
        LastRCastTick = Now();
        ActiveUpgrade = ChooseUpgrade(
            false, Engine::CountEnemiesAt(player.Position(), 650.0f) >= 2,
            Engine::ValidEnemy(target) && Lethal(target, RRawTurretDamage(SpellRank(3), AP())),
            defensive, LiveTurretCount(), 2);
        return true;
    }
    Vector3 aim = player.Position();
    if (ActiveUpgrade == UpgradeChoice::Turret) {
        aim = ClampPlacement(player.Position(), Engine::ValidEnemy(target)
            ? target.Position() : Game::CursorPos(), kRPlacementRange);
        const PlacementContext context{
            true, !SDK::NavMesh::IsWall(aim),
            player.Position().Distance2D(aim) <= kRPlacementRange + 20.0f,
            SDK::NavMesh::IsWall(aim), Engine::UnderEnemyTurret(aim),
            Engine::CountEnemiesAt(aim, 500.0f) > 1, Orbwalker::IsWindingUp(),
            LiveTurretCount(), kRMaximumTurrets};
        if (!ShouldPlaceTurret(context, defensive, lethal)) return false;
    } else if (Engine::ValidEnemy(target)) {
        aim = ActiveUpgrade == UpgradeChoice::Grenade
            ? Predicted(target, 2, kEDelay) : Predicted(target, 1, kWDelay);
    }
    if (aim.IsZero() || aim.Distance2D(player.Position()) > kRPlacementRange + 20.0f ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 15.0f)) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    LastRPosition = aim;
    LastRCastTick = Now();
    RActive = false;
    RExpireTick = 0;
    ActiveUpgrade = UpgradeChoice::None;
    return true;
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (LiveTurretCount() < 2 && CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo, false, false)) return;
    (void)CastR(target, Mode::Combo, false,
                Lethal(target, RRawGrenadeDamage(SpellRank(3), AP())));
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(TacticsMenu, "HarassMana", 45)) return;
    if (LiveTurretCount() == 0 && CastQ(target, Mode::Harass)) return;
    if (CastE(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass, false, false);
}
inline bool CastFarmSpell(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const auto& units = mode == Mode::Jungle
        ? GameObjects::Jungle() : GameObjects::EnemyMinions();
    AIBaseClient best{};
    int count = 0;
    Vector3 center{};
    for (const auto& unit : units) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(unit.Position()) > kWRange) continue;
        if (!best.IsValid()) best = unit;
        center = center + unit.Position();
        ++count;
    }
    if (!best.IsValid() || count == 0) return false;
    center = center / static_cast<float>(count);
    if (LiveTurretCount() < 2 && RuntimeQAmmo() > 0 && Ready(0, mode)) {
        const Vector3 position = ClampPlacement(player.Position(), center);
        const PlacementContext context{
            true, !SDK::NavMesh::IsWall(position),
            player.Position().Distance2D(position) <= kQPlacementRange + 12.0f,
            SDK::NavMesh::IsWall(position), false, false, Orbwalker::IsWindingUp(),
            LiveTurretCount(), ObservedQMaxAmmo};
        if (ShouldPlaceTurret(context, true, false) &&
            Engine::ControllerCastPosition(0, position)) {
            LastQPosition = position;
            LastQCastTick = Now();
            ObservedQAmmo = std::max(0, RuntimeQAmmo() - 1);
            return true;
        }
    }
    if (count >= Slider(FarmMenu, "MinimumRocketUnits", 3) && Ready(1, mode) &&
        Throttle(1, 70) && !ControllerHelpers::ProjectileWallBlocksFromPlayer(center, 30.0f) &&
        Engine::ControllerCastPosition(1, center)) {
        LastWPosition = center;
        LastWCastTick = Now();
        return true;
    }
    return false;
}
inline void Flee(const AIHeroClient& threat) {
    const auto zone = CurrentZone();
    const auto player = GameObjects::Player();
    const Vector3 cursor = Game::CursorPos();
    if (player.IsValid() && FleeZoneSafe(zone, player.Position(), cursor,
        Engine::CountEnemiesAt(player.Position(), 700.0f)) && RuntimeQAmmo() > 0 &&
        CastQ(threat, Mode::Flee, true, false)) return;
    if (Engine::ValidEnemy(threat) && CastE(threat, Mode::Flee, true, true, false)) return;
    if (Engine::ValidEnemy(threat)) (void)CastW(threat, Mode::Flee, true, false);
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const AIHeroClient target = PreferredEnemyTarget(selected, kWRange);
    if (PlayerOverrideUntil > Now()) return true;
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target) &&
        CastE(target, mode, true, true, false)) return true;
    if (Engine::ValidEnemy(target) &&
        Lethal(target, ERawDamage(SpellRank(2), AP())) &&
        CastE(target, mode, true, false, true)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (GameObjects::Player().ManaPercent() >= Slider(FarmMenu, "Mana", 42))
            (void)CastFarmSpell(mode);
        break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1100.0f)); break;
    case Mode::Automatic:
        if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(target))
            (void)CastE(target, mode, true, true, false);
        else if (Engine::ValidEnemy(target) &&
                 Lethal(target, ERawDamage(SpellRank(2), AP())))
            (void)CastE(target, mode, true, false, true);
        break;
    default: break;
    }
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = args.Slot;
        if (slot >= 0 && slot < 4) {
            if (!Engine::WasControllerCast(slot)) {
                PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
            }
            if (slot == 0) ObservedQAmmo = std::max(0, RuntimeQAmmo() - 1);
            if (slot == 3) {
                RActive = !RActive;
                RExpireTick = RActive ? now + 1200 : 0;
            }
            if (slot == 3) LastRCastTick = now;
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) {
        IncomingHardCCUntil = std::max(IncomingHardCCUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "HeimerdingerR")) {
        RActive = true;
        RExpireTick = Now() + 1200;
    }
    if (Engine::TextContains(args.BuffName, "HeimerdingerQAmmo")) {
        ObservedQAmmo = std::clamp(args.Count, 0, ObservedQMaxAmmo);
        QAmmoObserved = true;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "HeimerdingerR")) {
        RActive = false;
        ActiveUpgrade = UpgradeChoice::None;
    }
}
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) { OnBuffAdd(args); }
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    (void)ControllerHelpers::CaptureBeforeAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)ControllerHelpers::CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    (void)ControllerHelpers::CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs&) {
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 500);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs&) {
    IncomingHardCCUntil = std::max(IncomingHardCCUntil, Now() + 700);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!args.Sender.IsValid() || !player.IsValid() ||
        args.Sender.Team != static_cast<std::uint32_t>(player.Team()) ||
        (!Engine::TextContains(args.Sender.Name, "Heimer") &&
         !Engine::TextContains(args.Sender.CharacterName, "Turret"))) return;
    for (auto& turret : Turrets) {
        if (turret.Valid && turret.NetworkId == static_cast<int>(args.Sender.NetworkId)) return;
        if (!turret.Valid) {
            turret.NetworkId = static_cast<int>(args.Sender.NetworkId);
            turret.Position = args.Sender.Position;
            turret.CreatedTick = turret.LastSeenTick = Now();
            turret.ExpireTick = Now() + 480000;
            turret.Valid = true;
            turret.Super = Engine::TextContains(args.Sender.Name, "Super") ||
                           Engine::TextContains(args.Sender.CharacterName, "Super");
            return;
        }
    }
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    for (auto& turret : Turrets) {
        if (turret.Valid && turret.NetworkId == id) turret = {};
    }
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kWRange, 0xFF66CCFFu, 1.0f, 40);
    Drawing::DrawCircle(player.Position(), kERange, 0xFFFF8844u, 1.0f, 40);
    for (const auto& turret : Turrets) {
        if (turret.Valid) Drawing::DrawCircle(turret.Position, kQTurretAttackRadius, 0xFFAA66FFu, 1.0f, 32);
    }
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("HeimerTactics", "Turret zone and manual ownership"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 45, 10, 90));
    TurretMenu = TacticsMenu->AddSubMenu(new Menu("Turrets", "H-28G ammo and zone safety"));
    TurretMenu->Add(new MenuSlider("MaxPlacementEnemies", "Maximum enemies at placement", 2, 0, 5));
    RocketMenu = TacticsMenu->AddSubMenu(new Menu("Rockets", "Five-rocket collision policy"));
    GrenadeMenu = TacticsMenu->AddSubMenu(new Menu("Grenade", "Center-stun policy"));
    UpgradeMenu = TacticsMenu->AddSubMenu(new Menu("Upgrade", "UPGRADE!!! choice"));
    UpgradeMenu->Add(new MenuSlider("StartUpgradeBelowHP", "Start upgrade below target HP percent", 48, 10, 90));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Turret zone waveclear"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 42, 0, 90));
    FarmMenu->Add(new MenuSlider("MinimumRocketUnits", "Minimum rocket farm units", 3, 1, 8));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Coach", "Range and turret drawing"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw rocket, grenade and turret ranges", false));
}
inline void OnLoad() {
    Turrets = {};
    ObservedQAmmo = 0;
    ObservedQMaxAmmo = 3;
    QAmmoObserved = false;
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAutoTargetId = LastAutoTick = PlayerOverrideUntil = IncomingThreatUntil = IncomingHardCCUntil = 0;
    RActive = false;
    RExpireTick = 0;
    ActiveUpgrade = UpgradeChoice::None;
    LastQPosition = LastWPosition = LastEPosition = LastRPosition = {};
}
inline void OnUnload() {
    TacticsMenu = TurretMenu = RocketMenu = GrenadeMenu = UpgradeMenu = FarmMenu = CoachMenu = nullptr;
    Turrets = {};
    RActive = false;
    ActiveUpgrade = UpgradeChoice::None;
}
inline constexpr const char* Scenarios[] = {
    "Pin H-28G turret, Electron Grenade and UPGRADE!!! values to Riot 26.15 / CommunityDragon 16.15",
    "Track allied turret object creation and deletion, including super turret identity and expiry",
    "Reconcile Q turret ammo from runtime MaxAmmo/Ammo, buff observations and polling fallback",
    "Place Q only on valid, reachable, non-wall positions with turret count and turret-zone safety gates",
    "Keep the player in an existing 900 unit turret zone before committing when possible",
    "Use five-rocket W prediction with projectile-wall and real 1325 unit reach checks",
    "Use E prediction and require the 100 unit center hit for the 1.5 second grenade stun",
    "Reject nondefensive grenade or turret placements beneath enemy turrets or unsafe enemy density",
    "Choose upgraded turret for defensive zone, rockets for multi-target pressure, and grenade for stun or execute",
    "Preserve selected target before orbwalker fallback and refuse protected or untargetable targets",
    "Preserve AA windup for ordinary Q/E/W casts while permitting only reactive or lethal interruption",
    "Reconcile R active/recast state from spell events, buffs and polling and yield manual ownership",
    "Combo establishes turret zone, lands grenade stun, fires rockets, then uses an upgrade with value",
    "Harass uses turret setup and center grenade without spending upgrade for routine poke",
    "LaneClear, Jungle and LastHit use turret placement plus rocket cluster policy and mana reserve",
    "Flee peels with center grenade, rockets and only safe turret-zone reinforcement",
    "Automatic mode is limited to hard-CC defense and lethal grenade secure",
    "Never automate summoner spells, items, movement or unsafe turret dives",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Heimerdinger;
    controller.ControllerId = "champion.kuroaio.ai.heimerdinger.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIHeimerdinger.md";
    controller.ImplementationSummary = "Turret-ammo and object-aware zone controller with grenade center-stun, collision-safe rockets, upgrade selection, manual ownership and combat/farm/flee policies.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Heimerdinger
