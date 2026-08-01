#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIDrMundoGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::DrMundo {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CaptureInterruptableEvent;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Now;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Protected;

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int PlayerOverrideUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int PassiveCooldownUntil = 0;
inline int LastPassivePickupTick = 0;
inline bool PassiveReady = true;
inline bool WActive = false;
inline bool WOwned = false;
inline bool WTenacityMode = false;
inline int WStartTick = 0;
inline bool RActive = false;
inline bool ROwned = false;
inline int RStartTick = 0;
inline Mode LastMode = Mode::None;

inline bool Ready(int slot, Mode mode) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() &&
        (mode == Mode::None || SpellEnabled(slot, mode));
}
inline bool Throttle(int slot, int delay = 100) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
inline bool Lethal(const AIHeroClient& target, int slot) {
    return Engine::ValidEnemy(target) && slot >= 0 && slot < 4 &&
        Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->GetDamage(target) >=
            target.Health() + target.AllShield();
}
inline bool RuntimeWActive() {
    const auto player = GameObjects::Player();
    return player.IsValid() && (player.HasBuff("DrMundoW") ||
        player.HasBuff("DrMundoWActive") || player.HasBuff("DrMundoWRecast"));
}
inline bool RuntimeRActive() {
    const auto player = GameObjects::Player();
    return player.IsValid() && (player.HasBuff("DrMundoR") ||
        player.HasBuff("DrMundoRBuff") || player.HasBuff("DrMundoRHeal"));
}
inline bool RuntimePassiveCooldown() {
    const auto player = GameObjects::Player();
    return player.IsValid() && (player.HasBuff("DrMundoPCooldown") ||
        player.HasBuff("DrMundoPImmunity"));
}
inline bool HasAntiGrievousWounds() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return true;
    return player.HasBuff("GrievousWounds") || player.HasBuff("MortalWounds") ||
        player.HasBuff("gwounds") || player.HasBuff("healingreduction") ||
        player.HasBuff("SummonerExhaustDamageReduction");
}
inline bool CanSpendHealth(int slot, bool emergency = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int rank = SpellRank(slot);
    float cost = 0.0f;
    if (slot == 0) cost = QHealthCost(rank);
    else if (slot == 1) cost = player.Health() * kWHealthCostPercent;
    else if (slot == 2) cost = EHealthCost(rank);
    else return true;
    const float floor = slot == 1 ? (PassiveReady ? 18.0f : 24.0f)
                                  : (PassiveReady ? 24.0f : 30.0f);
    return SafeHealthSpend(player.Health(), player.MaxHealth(), cost, floor, emergency);
}
inline bool MinionBlocksQ(const Vec3& origin, const Vec3& aim,
                          const AIHeroClient& target) {
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable() ||
            minion.NetworkId() == target.NetworkId()) continue;
        const float targetDistance = origin.Distance2D(target.Position());
        if (origin.Distance2D(minion.Position()) >= targetDistance) continue;
        if (QPathHits(origin, aim, minion.Position(), minion.BoundingRadius())) return true;
    }
    return false;
}
inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (RuntimeWActive()) WActive = true;
    else if (WActive && now - WStartTick > 420) { WActive = false; WOwned = false; }
    if (RuntimeRActive()) RActive = true;
    else if (RActive && now - RStartTick > 1250) { RActive = false; ROwned = false; }
    const bool cooldown = RuntimePassiveCooldown();
    if (cooldown) {
        PassiveReady = false;
        if (PassiveCooldownUntil <= now)
            PassiveCooldownUntil = now + static_cast<int>(
                PassiveCooldownSeconds(player.Level()) * 1000.0f);
    } else if (PassiveCooldownUntil > 0 && now >= PassiveCooldownUntil) {
        PassiveCooldownUntil = 0;
        PassiveReady = true;
    } else if (!cooldown && PassiveCooldownUntil == 0) PassiveReady = true;
    if (IncomingThreatUntil <= now) IncomingThreatUntil = 0;
    if (IncomingHardCcUntil <= now) IncomingHardCcUntil = 0;
    if (InterruptExpireTick <= now) InterruptTargetId = 0;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 80.0f) ||
        !Ready(0, mode) || !Throttle(0) || Protected(target) ||
        PreserveAttack(reactive) || !CanSpendHealth(0, reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vec3 aim = prediction.GetCastPosition().IsValid()
        ? prediction.GetCastPosition() : PredictPosition(target, kQDelay);
    const bool predictionAccepted = !Bool(QMenu, "RequireHighHitChance", true) ||
        prediction.Hitchance >= SDK::HitChance::High;
    if (!aim.IsValid() || aim.IsZero() ||
        player.Position().Distance2D(aim) > kQRange + target.BoundingRadius() ||
        !predictionAccepted || MinionBlocksQ(player.Position(), aim, target) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQHalfWidth) ||
        SDK::NavMesh::IsWallBetween(player.Position(), aim, kQHalfWidth)) return false;
    const bool turretRisk = Engine::UnderEnemyTurret(aim) &&
        !Engine::UnderEnemyTurret(player.Position());
    if (turretRisk && !reactive) return false;
    const QCastContext context{true, true, true, predictionAccepted, false, false,
        turretRisk, Engine::ValidEnemy(target), CanSpendHealth(0, reactive)};
    if (!ShouldCastQ(context)) return false;
    if (!Engine::ControllerCastPosition(0, ClampQEndpoint(player.Position(), aim))) return false;
    LastCastTick[0] = Now();
    return true;
}
inline bool StartW(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive) || !CanSpendHealth(1, reactive)) return false;
    const int nearby = Engine::CountEnemiesAt(player.Position(), kWRadius);
    const bool threat = reactive || IncomingThreatUntil > Now() ||
        player.HealthPercent() <= 62.0f || nearby >= 2;
    WTenacityMode = Bool(WMenu, "TenacityToggle", false) ||
        IncomingHardCcUntil > Now();
    const WCastContext context{true, WActive, false, true, CanSpendHealth(1, reactive),
        !WOwned && PlayerOverrideUntil > Now(), threat, false};
    if (!ShouldCastW(context)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WActive = true; WOwned = true; WStartTick = Now(); LastCastTick[1] = Now();
    return true;
}
inline bool RecastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !WActive || !Ready(1, mode) || !Throttle(1) ||
        !Engine::ValidEnemy(target, kWRecastRange + 80.0f) ||
        !WInRange(player.Position(), PredictPosition(target, 0.10f), target.BoundingRadius()) ||
        PreserveAttack(reactive)) return false;
    const bool lethal = Lethal(target, 1);
    if (PreferWTenacity(WActive,
            !WOwned && PlayerOverrideUntil > Now(),
            IncomingHardCcUntil > Now(), lethal) && WTenacityMode) return false;
    const WCastContext context{true, true, true, true, true,
        !WOwned && PlayerOverrideUntil > Now(), reactive || IncomingThreatUntil > Now(), lethal};
    if (!ShouldCastW(context) || !Engine::ControllerCastSelf(1)) return false;
    WActive = false; WOwned = false; LastCastTick[1] = Now();
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange + 80.0f) ||
        !EInRange(player.Position(), PredictPosition(target, 0.05f), target.BoundingRadius()) ||
        !Ready(2, mode) || !Throttle(2) || !CanSpendHealth(2, reactive) ||
        Protected(target) || PreserveAttack(reactive)) return false;
    if (Engine::UnderEnemyTurret(target.Position()) &&
        !Engine::UnderEnemyTurret(player.Position()) && !reactive) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    LastCastTick[2] = Now();
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 180) ||
        PreserveAttack(reactive) || !SpellEnabled(3, mode)) return false;
    const float hp = player.HealthPercent();
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRRadius);
    const bool threat = reactive || IncomingThreatUntil > Now() ||
        IncomingHardCcUntil > Now();
    const bool emergency = hp <= 18.0f || IncomingHardCcUntil > Now();
    if (!SafeRegeneration(hp, HasAntiGrievousWounds(), threat, enemies,
                           static_cast<float>(Slider(RMenu, "CastHealth", 52))) && !emergency) return false;
    if (!Engine::UnderEnemyTurret(player.Position()) && enemies >
        Slider(RMenu, "MaximumEnemies", 3) && hp > 36.0f && !emergency) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RActive = true; ROwned = true; RStartTick = Now(); LastCastTick[3] = Now();
    return true;
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (WActive && RecastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (StartW(Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (WActive && RecastW(target, Mode::Harass)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (CastE(target, Mode::Harass)) return;
    (void)StartW(Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (CastR(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target) && CastQ(target, Mode::Flee, true)) return;
    if (WActive && Engine::ValidEnemy(target)) (void)RecastW(target, Mode::Flee, true);
}
inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() ||
        player.HealthPercent() < Slider(FarmMenu, "MinimumHealth", 42)) return;
    if (Ready(0, mode) && CanSpendHealth(0) && Engine::TryFarm(mode))
        LastCastTick[0] = Now();
}
inline bool TryInterrupt(const AIHeroClient& fallback, Mode mode) {
    if (InterruptTargetId == 0 || InterruptExpireTick <= Now()) return false;
    const auto interrupt = HeroByNetworkId(InterruptTargetId);
    const auto target = Engine::ValidEnemy(interrupt, kQRange) ? interrupt : fallback;
    return Engine::ValidEnemy(target) && CastQ(target, mode, true);
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const auto target = ControllerHelpers::PreferredEnemyTarget(selected,
        mode == Mode::Flee ? 1050.0f : kQRange);
    if (PlayerOverrideUntil > Now()) return true;
    if (TryInterrupt(target, mode)) return true;
    const auto player = GameObjects::Player();
    if (player.IsValid() && (IncomingThreatUntil > Now() ||
        player.HealthPercent() <= Slider(RMenu, "CastHealth", 52)) &&
        CastR(target, mode, true)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1050.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Automatic:
        if (player.IsValid() && player.HealthPercent() <= 42.0f)
            (void)CastR(target, mode, true);
        else if (Engine::ValidEnemy(target)) {
            if (WActive) (void)RecastW(target, mode);
            else (void)CastQ(target, mode, true);
        }
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
        const bool ours = Engine::WasControllerCast(slot);
        if (!ours) PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        LastCastTick[slot] = now;
        if (slot == 1) { WActive = !WActive; WOwned = ours; WStartTick = now; }
        if (slot == 3) { RActive = true; ROwned = ours; RStartTick = now; }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCcUntil = std::max(
        IncomingHardCcUntil, std::max(analysis.CommitmentUntilTick,
                                       analysis.LineThreatUntilTick));
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    (void)CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "DrMundoW")) WActive = true;
    if (Engine::TextContains(args.BuffName, "DrMundoR")) RActive = true;
    if (Engine::TextContains(args.BuffName, "DrMundoP") &&
        !Engine::TextContains(args.BuffName, "DrMundoPCooldown") &&
        !Engine::TextContains(args.BuffName, "DrMundoPImmunity")) {
        LastPassivePickupTick = Now();
        if (PassiveCooldownUntil > Now())
            PassiveCooldownUntil = std::max(Now(),
                PassiveCooldownUntil - static_cast<int>(kPassivePickupRefundSeconds * 1000.0f));
    }
    if (Engine::TextContains(args.BuffName, "DrMundoPCooldown") ||
        Engine::TextContains(args.BuffName, "DrMundoPImmunity")) {
        PassiveReady = false; PassiveCooldownUntil = Now() + static_cast<int>(
            PassiveCooldownSeconds(GameObjects::Player().Level()) * 1000.0f);
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "DrMundoW")) WActive = false;
    if (Engine::TextContains(args.BuffName, "DrMundoR")) RActive = false;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    if ((WActive && WOwned && Bool(WMenu, "PreserveBurn", true)) ||
        (PlayerOverrideUntil > Now() && Bool(TacticsMenu, "ProtectManual", true))) args.Process = false;
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFF55CC77u, 1.3f, 40);
    Drawing::DrawCircle(player.Position(), kWRadius, 0xFFCC8844u, 1.2f, 40);
    Drawing::DrawCircle(player.Position(), kRRadius, 0xFFDD4466u, 1.2f, 40);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("DrMundoOneTrick", "Dr. Mundo health tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuBool("ProtectManual", "Protect manual windups", true));
    PassiveMenu = TacticsMenu->AddSubMenu(new Menu("Passive", "Infected Bones cooldown"));
    PassiveMenu->Add(new MenuBool("TrackCanister", "Track canister cooldown/refund", true));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Infected Bonesaw"));
    QMenu->Add(new MenuBool("RequireHighHitChance", "Require high prediction", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Heart Zapper burn/recast"));
    WMenu->Add(new MenuBool("PreserveBurn", "Protect attack during W burn", true));
    WMenu->Add(new MenuBool("TenacityToggle", "Hold W burn for tenacity policy", false));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Blunt Force Trauma"));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Maximum Dosage regeneration"));
    RMenu->Add(new MenuSlider("CastHealth", "Cast below health percent", 52, 15, 80));
    RMenu->Add(new MenuSlider("MaximumEnemies", "Maximum nearby enemies", 3, 1, 5));
    RMenu->Add(new MenuBool("AntiGrievousEmergency", "Permit emergency anti-grievous R", true));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("DrMundoFarm", "Health-cost farm policy"));
    FarmMenu->Add(new MenuSlider("MinimumHealth", "Minimum farming health percent", 42, 10, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("DrMundoCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W/R ranges", false));
}
inline void OnLoad() {
    LastCastTick.fill(0); LastAutoTargetId = LastAutoTick = PlayerOverrideUntil = 0;
    IncomingThreatUntil = IncomingHardCcUntil = InterruptTargetId = InterruptExpireTick = 0;
    PassiveCooldownUntil = LastPassivePickupTick = 0; PassiveReady = true;
    WActive = WOwned = WTenacityMode = RActive = ROwned = false;
    WStartTick = RStartTick = 0; LastMode = Mode::None;
}
inline void OnUnload() {
    OnLoad();
}

inline constexpr const char* Scenarios[] = {
    "Track DrMundoP canister immunity and cooldown from buffs, polling and pickup refund",
    "Preserve the health-state model while reconciling manual and controller spell ownership",
    "Cast Q only with predicted 1050-range reach, 60 width and high hitchance",
    "Account for Q current-health percent damage, slow, refund and health cost",
    "Reject Q through a nearer minion, projectile wall, navmesh wall or turret-only endpoint",
    "Protect ordinary auto-attack windup unless a reactive cleaver is justified",
    "Toggle W only when its 8 percent current-health cost is affordable",
    "Toggle W burn versus tenacity-preservation policy under hard crowd control",
    "Use W recast for lethal or incoming-threat gray-health conversion, never manual burn",
    "Use E as a health-cost attack reset and scale its damage by missing health",
    "Reject E when health cost would cross the conservative survival floor",
    "Reconcile R regeneration buff and cast only through low-health or incoming-threat gates",
    "Reject ordinary R while anti-grievous wounds or excessive nearby enemies make healing unsafe",
    "Permit emergency R against hard crowd control and lethal incoming pressure",
    "Apply turret, target validity, selected-target and protected-target policies",
    "Support Combo Harass LaneClear Jungle LastHit Flee and Automatic modes distinctly",
    "Capture enemy threat and interrupt windows through process-spell callbacks",
    "Keep pure percent-health formulas, reach, collision and safety boundaries in geometry",
    "Keep ChampionController ABI and shared catalog registration untouched",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::DrMundo;
    controller.ControllerId = "champion.kuroaio.ai.drmundo.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIDrMundo.md";
    controller.ImplementationSummary =
        "Infected Bones cooldown tracking, percent-current-health Q, W burn/recast,"
        " missing-health E empowerment, and anti-grievous health-regeneration R safety.";
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
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnInterruptable = &CaptureInterruptableEvent<
        &InterruptTargetId, &InterruptExpireTick, 1100, 220, 5000>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::DrMundo
