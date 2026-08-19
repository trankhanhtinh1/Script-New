#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AITrundleGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace Plugins::KuroAIO::AI::Controllers::Trundle {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline ZoneState Domain{};
inline Vec3 LastPillarPosition{};
inline int LastCastTick[4]{};
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int QStealTargetId = 0;
inline int QStealExpireTick = 0;
inline int RTargetId = 0;
inline int RExpireTick = 0;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
using ControllerHelpers::Ready;
inline bool Throttle(int slot, int delay = 80) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Lethal;
inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target,
            QRawDamage(SpellRank(0), player.TotalAttackDamage())) : 0.0f;
}
inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target,
            RRawDamage(SpellRank(3), target.Health())) : 0.0f;
}
inline bool InDomain(const AIHeroClient& target) {
    return ZoneActive(Domain, Now()) &&
        ZoneContains(Domain.Center, target.Position(), kWRadius + target.BoundingRadius());
}
inline bool SafePlacement(const Vec3& point, bool defensive = false) {
    if (!point.IsValid() || point.IsZero() || SDK::NavMesh::IsWall(point)) return false;
    if (!defensive && Engine::UnderEnemyTurret(point) &&
        !Engine::UnderEnemyTurret(GameObjects::Player().Position())) return false;
    return Engine::CountEnemiesAt(point, 250.0f) <= Slider(EMenu, "MaxEnemies", 2);
}
inline Vec3 DomainAim(const AIHeroClient& target, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    if (Engine::ValidEnemy(target)) {
        const Vec3 predicted = PredictPosition(target, 0.25f);
        return ClampZoneCast(player.Position(), predicted);
    }
    const Vec3 cursor = Game::CursorPos();
    return ClampZoneCast(player.Position(), defensive ? player.Position() : cursor);
}
inline Vec3 PillarAim(const AIHeroClient& target, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    const Vec3 center = Engine::ValidEnemy(target) ? PredictPosition(target, 0.25f) :
        Game::CursorPos();
    Vec3 direction = Direction2D(player.Position(), center);
    if (direction.IsZero()) direction = Direction2D(player.Position(), Game::CursorPos());
    if (direction.IsZero()) return {};
    const float offset = Engine::ValidEnemy(target) ?
        target.BoundingRadius() + kEPillarRadius + 24.0f : kEPillarRadius;
    const Vec3 requested = center + direction * (defensive ? -offset : offset);
    return requested;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + target.BoundingRadius()) ||
        !Ready(0, mode) || !Throttle(0) || Protected(target) || PreserveAttack(reactive, true)) return false;
    if (!Engine::ControllerCastUnit(0, target)) return false;
    LastCastTick[0] = Now();
    QStealTargetId = static_cast<int>(target.NetworkId());
    QStealExpireTick = Now() + 5000;
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive)) return false;
    const Vec3 aim = DomainAim(target, defensive);
    if (!InRange(player.Position(), aim, kWRange) || !SafePlacement(aim, defensive)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastCastTick[1] = Now();
    RecordZone(Domain, aim, LastCastTick[1]);
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2) || PreserveAttack(reactive)) return false;
    const Vec3 aim = PillarAim(target, reactive);
    if (!PillarPlacementValid(player.Position(), aim, SDK::NavMesh::IsWall(aim),
        Engine::UnderEnemyTurret(aim), Engine::CountEnemiesAt(aim, 250.0f),
        Slider(EMenu, "MaxEnemies", 2))) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now();
    LastPillarPosition = aim;
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool defensive = false, bool manual = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kRRange) ||
        !Ready(3, mode) || !Throttle(3, 120) || Protected(target) ||
        PreserveAttack(reactive)) return false;
    const RTargetPolicy policy{
        true, true, false, Lethal(target, RDamage(target)),
        target.HealthPercent() <= Slider(RMenu, "TargetHP", 65),
        static_cast<int>(target.NetworkId()) == Engine::LockedTargetNetworkId || manual,
        Engine::UnderEnemyTurret(player.Position()), defensive,
        Engine::CountEnemiesAt(player.Position(), 550.0f), Slider(RMenu, "MaxNearby", 3)};
    if (!ShouldCastR(policy)) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    LastCastTick[3] = Now();
    RTargetId = static_cast<int>(target.NetworkId());
    RExpireTick = Now() + 4000;
    return true;
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Ready(0, mode) && Lethal(target, QDamage(target)) && CastQ(target, mode, true)) return true;
    return Ready(3, mode) && Lethal(target, RDamage(target)) && CastR(target, mode, true);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (!InDomain(target) && CastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(WMenu, "HarassMana", 48)) return;
    if (!InDomain(target) && CastW(target, Mode::Harass)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastE(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastE(target, Mode::Flee, true)) return;
    (void)CastW(target, Mode::Flee, true, true);
}
inline void ReconcileState() {
    const int now = Now();
    if (Domain.ExpireTick > 0 && now > Domain.ExpireTick) Domain = {};
    if (QStealExpireTick <= now) QStealTargetId = 0;
    if (RExpireTick <= now) RTargetId = 0;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("TrundlePainTolerance") || player.HasBuff("TrundleRBuff")) {
        RTargetId = RTargetId == 0 ? 1 : RTargetId;
        RExpireTick = now + 3500;
    }
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const auto player = GameObjects::Player();
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, mode == Mode::Flee ? 1000.0f : kRRange);
    if (ManualOwnershipUntil > Now()) return true;
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target) && CastE(target, mode, true)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1000.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (player.IsValid() && player.ManaPercent() >= Slider(FarmMenu, "Mana", 42))
            (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (AutomaticAllowed({IncomingThreatUntil > Now(), Lethal(target, RDamage(target)),
            player.IsValid() && player.HealthPercent() <= Slider(RMenu, "DefensiveHP", 38),
            ManualOwnershipUntil > Now()}))
            (void)CastR(target, mode, true, true);
        break;
    default: break;
    }
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot < 0 || slot > 3) return;
        if (!Engine::WasControllerCast(slot)) ManualOwnershipUntil = now +
            Slider(TacticsMenu, "ManualOwnershipMs", 560);
        LastCastTick[slot] = now;
        if (slot == 1 && args.EndPosition.IsValid() && !args.EndPosition.IsZero())
            RecordZone(Domain, args.EndPosition, now);
        if (slot == 2 && args.EndPosition.IsValid() && !args.EndPosition.IsZero())
            LastPillarPosition = args.EndPosition;
        if (slot == 3) { RTargetId = 0; RExpireTick = now + 3500; }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl)
        IncomingHardCCUntil = std::max(IncomingHardCCUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const std::string name = args.BuffName;
    if (name.find("TrundleW") != std::string::npos || name.find("trundledesecrate") != std::string::npos)
        Domain.ExpireTick = std::max(Domain.ExpireTick, Now() + 1000);
    if (name.find("TrundleTrollSmash") != std::string::npos ||
        name.find("trundlechomp") != std::string::npos)
        QStealExpireTick = std::max(QStealExpireTick, Now() + 5000);
    if (name.find("TrundleR") != std::string::npos || name.find("trundler") != std::string::npos)
        RExpireTick = Now() + 3500;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    const std::string name = args.BuffName;
    if (name.find("TrundleTrollSmash") != std::string::npos ||
        name.find("trundlechomp") != std::string::npos) {
        QStealTargetId = 0;
        QStealExpireTick = 0;
    }
    if (name.find("TrundleR") != std::string::npos || name.find("trundler") != std::string::npos) {
        RTargetId = 0;
        RExpireTick = 0;
    }
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF5599DDu, 1.5f, 32);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFAA6688u, 1.0f, 32);
    if (ZoneActive(Domain, Now())) Drawing::DrawCircle(Domain.Center, kWRadius,
        0xFF4499DDu, 1.5f, 40);
    if (!LastPillarPosition.IsZero()) Drawing::DrawCircle(LastPillarPosition,
        kEPillarRadius, 0xFF88CCFFu, 1.5f, 32);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("TrundleOneTrick", "Trundle tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Chomp"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Frozen Domain"));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 48, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Pillar"));
    EMenu->Add(new MenuSlider("MaxEnemies", "Maximum enemies at pillar", 2, 0, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Subjugate"));
    RMenu->Add(new MenuSlider("TargetHP", "Cast below target HP percent", 65, 10, 95));
    RMenu->Add(new MenuSlider("DefensiveHP", "Defensive player HP percent", 38, 10, 80));
    RMenu->Add(new MenuSlider("MaxNearby", "Maximum nearby enemies", 3, 0, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("TrundleFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 42, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("TrundleCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W/E/R zones", false));
}
inline void OnLoad() {
    Domain = {};
    LastPillarPosition = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    ManualOwnershipUntil = IncomingThreatUntil = IncomingHardCCUntil = 0;
    LastAutoTargetId = LastAutoTick = QStealTargetId = QStealExpireTick = 0;
    RTargetId = RExpireTick = 0;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    Domain = {};
    LastPillarPosition = {};
}
inline constexpr const char* Scenarios[] = {
    "Pin Trundle values to Riot 26.15 and CommunityDragon 16.15",
    "Preserve selected target before orbwalker and selector fallback",
    "Use Q as an attack reset and steal attack damage from the bitten target",
    "Do not cancel a normal attack unless Q reset or reactive safety justifies it",
    "Cast W on a predicted target point and reconcile domain expiry from events and polling",
    "Use Frozen Domain for combo speed, harass mana discipline and defensive flee",
    "Place E only at a reachable, non-wall, low-risk terrain point",
    "Use E pillar radius for displacement, path blocking and anti-gapclose peel",
    "Reject pillar placements under a new enemy turret or excessive enemy count",
    "Use R Subjugate only on a protected-valid, reachable primary or defensive target",
    "Prefer low-health, lethal and high-value target stat-drain turns",
    "Never spend R on an unreachable, invulnerable, spell-shielded or turret-risk target",
    "Track Q steal, W domain, E pillar and R drain through events plus reconciliation",
    "Respond to incoming hard crowd control with E peel without stealing movement ownership",
    "Combo W then Q, use E displacement and reserve R for a committed target",
    "Harass W/Q conservatively and never start an unsolicited all-in ultimate",
    "LaneClear, Jungle and LastHit use shared farm policy after mana guard",
    "Flee prioritizes pillar peel and defensive domain rather than target chase",
    "Automatic mode is limited to threat, defensive or kill-secure R decisions",
    "Yield after observed manual Q, W, E or R ownership",
    "Keep controller loop and profile spell metadata independently auditable",
    "Draw ranges and state without changing gameplay decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Trundle;
    controller.ControllerId = "champion.kuroaio.ai.trundle.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AITrundle.md";
    controller.ImplementationSummary =
        "Q reset and AD steal, W domain state, collision-safe E pillar and conservative R stat-drain target policy.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;

    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Trundle
