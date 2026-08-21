#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIMalphiteGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Malphite {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Protected;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;
inline MalphiteState State{};
inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int GapcloserTargetId = 0;
inline Vector3 GapcloserEnd{};
inline int GapcloserExpireTick = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline Mode LastMode = Mode::None;

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() &&
        (reactive || ControllerHelpers::SpellEnabled(slot, mode));
}
inline bool Throttle(int slot, int delay = 55) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
inline bool HasManaFor(int slot, float reserve = 0.0f) {
    return ControllerHelpers::CurrentResource() + 0.5f >=
        SpellCost(slot) + std::max(0.0f, reserve);
}
inline bool ShieldBuffPresent(const AIHeroClient& player) {
    return player.IsValid() &&
        (player.HasBuff("MalphiteShield") || player.HasBuff("malphiteshield"));
}
inline bool GraniteShieldReady() {
    const auto p = GameObjects::Player();
    if (!p.IsValid()) return false;
    return State.ShieldActive ||
        PassiveReady(Now(), State.LastShieldBreakTick, p.Level());
}
inline bool RUnderTurretGate(const Vector3& impact, bool lethal, bool defensive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !impact.IsValid() || impact.IsZero()) return false;
    if (SDK::NavMesh::IsWall(impact)) return false;
    return !Engine::UnderEnemyTurret(impact) ||
        Engine::UnderEnemyTurret(player.Position()) || lethal || defensive;
}
inline float QDamage(const AIHeroClient& target) {
    const auto p = GameObjects::Player();
    return p.IsValid() && Engine::ValidEnemy(target)
        ? p.CalculateMagicDamage(target, QRawDamage(SpellRank(0), p.AP())) : 0.0f;
}
inline float WDamage(const AIHeroClient& target, bool primary = true) {
    const auto p = GameObjects::Player();
    return p.IsValid() && Engine::ValidEnemy(target)
        ? p.CalculatePhysicalDamage(target, WRawDamage(SpellRank(1), p.AP(),
            EffectiveArmor(p.Armor(), SpellRank(1), GraniteShieldReady()), primary))
        : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto p = GameObjects::Player();
    return p.IsValid() && Engine::ValidEnemy(target)
        ? p.CalculatePhysicalDamage(target, ERawDamage(SpellRank(2), p.AP(),
            EffectiveArmor(p.Armor(), SpellRank(1), GraniteShieldReady())))
        : 0.0f;
}
inline float RDamage(const AIHeroClient& target) {
    const auto p = GameObjects::Player();
    return p.IsValid() && Engine::ValidEnemy(target)
        ? p.CalculateMagicDamage(target, RRawDamage(SpellRank(3), p.AP())) : 0.0f;
}
inline bool UnsafeMobility(const Vector3& impact, bool defensive) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || PlayerMobilityLocked()) return true;
    if (!defensive && p.IsDashing()) return true;
    return impact.IsValid() && !impact.IsZero() && SDK::NavMesh::IsWall(impact);
}
inline Vector3 PredictedQAim(const AIHeroClient& target) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || !target.IsValid()) return {};
    return PredictPosition(target, kQDelay +
        p.Position().Distance2D(target.Position()) / kQSpeed);
}
inline Vector3 PredictedRAim(const AIHeroClient& target) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || !target.IsValid()) return {};
    return ClampRAim(p.Position(), PredictPosition(target, kRDelay +
        p.Position().Distance2D(target.Position()) / kRSpeed));
}
inline int PredictedRHits(const Vector3& impact) {
    int hits = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) &&
            RImpactContains(impact, PredictPosition(enemy, kRDelay), enemy.BoundingRadius())) ++hits;
    }
    return hits;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || Protected(target) || !Ready(0, mode, reactive) ||
        !Throttle(0) || !HasManaFor(0, SpellCost(3) * 0.35f) ||
        (!reactive && ControllerHelpers::PreserveAttack(false))) return false;
    const Vector3 aim = PredictedQAim(target);
    if (!QReachable(p.Position(), aim, target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQMissileRadius)) return false;
    if (Engine::ControllerCastPredicted(0, target, SDK::HitChance::High)) {
        LastCastTick[0] = Now();
        State.QStealTargetId = static_cast<int>(target.NetworkId());
        State.QStealExpireTick = Now() + 3000;
        return true;
    }
    return false;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || !Engine::ValidEnemy(target) || !InAutoAttackRange(target) ||
        !Ready(1, mode, reactive) || !Throttle(1, 45) || !HasManaFor(1) ||
        (!reactive && ControllerHelpers::PreserveAttack(false))) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    State.WResetExpireTick = Now() + static_cast<int>(kWResetWindowSeconds * 1000.0f);
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool defensive = false) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || Protected(target) || !Ready(2, mode, reactive) ||
        !Throttle(2) || !HasManaFor(2) ||
        !SafeEPosition(p.Position(), target.Position(), target.BoundingRadius()) ||
        (!reactive && ControllerHelpers::PreserveAttack(false))) return false;
    if (!defensive && !Engine::UnderEnemyTurret(p.Position()) &&
        Engine::UnderEnemyTurret(target.Position())) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    LastCastTick[2] = Now();
    State.ECrippleExpireTick = Now() + 3000;
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool defensive = false) {
    const auto p = GameObjects::Player();
    if (!p.IsValid() || Protected(target) || !Ready(3, mode, reactive) ||
        !Throttle(3, 120) || !HasManaFor(3) ||
        (!reactive && ControllerHelpers::PreserveAttack(false))) return false;
    const Vector3 impact = PredictedRAim(target);
    if (!RProjectileClear(p.Position(), impact) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(impact, kRMissileRadius)) return false;
    const int hits = PredictedRHits(impact);
    const bool lethal = Lethal(target, RDamage(target));
    const int enemies = Engine::CountEnemiesAt(impact, kRRadius);
    const int maximum = Slider(UltimateMenu, "MaximumEnemies", 3);
    const int minimum = Slider(UltimateMenu, "MinimumTargets", 2);
    if (!RUnderTurretGate(impact, lethal, defensive) ||
        !RCommitAllowed(lethal, defensive, hits, minimum, enemies, maximum,
            Engine::UnderEnemyTurret(impact), Engine::UnderEnemyTurret(p.Position()),
            UnsafeMobility(impact, defensive), false)) return false;
    if (!Engine::ControllerCastPosition(3, impact)) return false;
    LastCastTick[3] = Now();
    State.RImpactTick = Now() + static_cast<int>(ImpactTravelSeconds(p.Position(), impact) * 1000.0f);
    return true;
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, RDamage(target)) && CastR(target, mode, true)) return true;
    if (Lethal(target, EDamage(target)) && CastE(target, mode, true, true)) return true;
    if (Lethal(target, QDamage(target)) && CastQ(target, mode, true)) return true;
    return Lethal(target, WDamage(target)) && CastW(target, mode, true);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastR(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo, false, false)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastQ(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || PlayerManaPercent() <
        Slider(TacticsMenu, "HarassMana", 55)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (InAutoAttackRange(target)) {
        if (CastW(target, Mode::Harass)) return;
        (void)CastE(target, Mode::Harass);
    }
}
inline void Flee(const AIHeroClient& threat) {
    if (!Engine::ValidEnemy(threat)) return;
    if (CastQ(threat, Mode::Flee, true)) return;
    (void)CastE(threat, Mode::Flee, true, true);
}
inline void ReconcileState() {
    const int now = Now();
    ExpireState(State, now);
    const auto p = GameObjects::Player();
    if (!p.IsValid()) return;
    ReconcilePassive(State, ShieldBuffPresent(p), now);
    if (p.HasBuff("MalphiteQ") || p.HasBuff("SeismicShardBuff")) {
        State.QStealExpireTick = std::max(State.QStealExpireTick, now + 250);
    }
    if (p.HasBuff("MalphiteW") || p.HasBuff("MalphiteObduracyEffect"))
        State.WResetExpireTick = std::max(State.WResetExpireTick, now + 300);
    if (p.HasBuff("MalphiteE")) State.ECrippleExpireTick = std::max(State.ECrippleExpireTick, now + 250);
}
inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    LastMode = mode;
    ReconcileState();
    const auto target = Engine::SelectTarget(kRRange);
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target)) {
        if (CastE(target, mode, true, true)) return true;
        if (IncomingHardCCUntil > Now() && CastR(target, mode, true, true)) return true;
    }
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 900.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (PlayerManaPercent() >= Slider(FarmMenu, "Mana", 42)) {
            const auto farmTarget = NearestEnemyToPlayer({}, 500.0f);
            if (Engine::ValidEnemy(farmTarget) && InAutoAttackRange(farmTarget))
                (void)CastW(farmTarget, mode, true);
            (void)Engine::TryFarm(mode);
        }
        break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) &&
            (IncomingThreatUntil > Now() || Lethal(target, RDamage(target))))
            (void)CastR(target, mode, true, IncomingThreatUntil > Now());
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
        LastCastTick[slot] = now;
        if (slot == 0 && args.TargetNetworkId != 0) {
            State.QStealTargetId = static_cast<int>(args.TargetNetworkId);
            State.QStealExpireTick = now + 3000;
        }
        if (slot == 1) State.WResetExpireTick = now + 450;
        if (slot == 2) State.ECrippleExpireTick = now + 3000;
        if (slot == 3) State.RImpactTick = now + 1500;
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl)
        IncomingHardCCUntil = std::max(IncomingHardCCUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "MalphiteShield")) State.ShieldActive = true;
    if (Engine::TextContains(args.BuffName, "SeismicShard")) State.QStealExpireTick = Now() + 3000;
    if (Engine::TextContains(args.BuffName, "Obduracy")) State.WResetExpireTick = Now() + 450;
    if (Engine::TextContains(args.BuffName, "Landslide")) State.ECrippleExpireTick = Now() + 3000;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "MalphiteShield")) {
        State.ShieldActive = false;
        State.LastShieldBreakTick = Now();
    }
    if (Engine::TextContains(args.BuffName, "SeismicShard")) State.QStealExpireTick = 0;
    if (Engine::TextContains(args.BuffName, "Obduracy")) State.WResetExpireTick = 0;
    if (Engine::TextContains(args.BuffName, "Landslide")) State.ECrippleExpireTick = 0;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || (LastMode != Mode::Combo && LastMode != Mode::Harass)) return;
    const AIHeroClient target(args.Target.Handle());
    if (Engine::ValidEnemy(target) && State.WResetExpireTick <= Now())
        (void)CastW(target, LastMode, true);
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto p = GameObjects::Player();
    if (!p.IsValid()) return;
    Drawing::DrawCircle(p.Position(), kQRange, 0xFFCC8844u, 1.0f, 40);
    Drawing::DrawCircle(p.Position(), kERadius, 0xFFAA55CCu, 1.0f, 40);
    Drawing::DrawCircle(p.Position(), kRRange, 0xFFEE8844u, 1.0f, 48);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::MissileEventIsLocal(args) &&
        (Engine::TextContains(args.SpellName, "SeismicShard") ||
         Engine::TextContains(args.MissileName, "Malphite_Q_mis")))
        State.QStealExpireTick = std::max(State.QStealExpireTick, Now() + 3000);
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("MalphiteTactics", "Malphite armor tactics"));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 55, 10, 90));
    UltimateMenu = TacticsMenu->AddSubMenu(new Menu("Ultimate", "Unstoppable Force gates"));
    UltimateMenu->Add(new MenuSlider("MinimumTargets", "Minimum predicted R targets", 2, 1, 5));
    UltimateMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at landing", 3, 1, 6));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Farm", "Farm resource reserve"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 42, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("Coach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Malphite ranges", false));
}
inline void OnLoad() {
    State = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    IncomingThreatUntil = IncomingHardCCUntil = 0;
    GapcloserTargetId = InterruptTargetId = 0;
    GapcloserEnd = {};
    GapcloserExpireTick = InterruptExpireTick = 0;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = UltimateMenu = FarmMenu = CoachMenu = nullptr;
    State = {};
}

inline constexpr const char* Scenarios[] = {
    "Use Riot 26.15 and CommunityDragon 16.15 values for Granite Shield, Q, W, E and R",
    "Reconcile Granite Shield buff and its level 1/7/13 8/7/6-second refresh",
    "Reserve mana and health-safe positioning before spending armor engage",
    "Predict Seismic Shard at 625 range, 1200 speed and track its three-second speed steal",
    "Reject Q projectile paths blocked by terrain and protected targets",
    "Cast Thunderclap as a real 350-radius attack reset without canceling windup",
    "Apply W bonus armor multiplier only through observed shield-aware armor state",
    "Use Ground Slam in 400 radius for attack-speed cripple and peel",
    "Predict Unstoppable Force impact travel and count targets inside 270 radius",
    "Reject R landing on walls, unsafe mobility, enemy turrets or excessive enemy density",
    "Allow R single-target kill-secure or defensive interrupt only through explicit gates",
    "Use autonomous Engine target selection for combat decisions",
    "Combo prioritizes multi-target/lethal R, then E cripple, W reset and Q chase",
    "Harass uses Q first and only W/E inside actual attack or spell reach",
    "LaneClear Jungle and LastHit retain a mana floor and armor-aware W weave",
    "Flee uses Q movement-steal slow and Ground Slam peel without blind R commits",
    "Automatic mode permits only lethal or incoming-threat R and reactive E",
    "Track Q missiles, AA targets, gapcloser and interrupt windows without generic spell logic",
    "Never automate movement, items or summoner spells",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Malphite;
    controller.ControllerId = "champion.kuroaio.ai.malphite.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIMalphite.md";
    controller.ImplementationSummary =
        "Granite Shield refresh reconciliation, Q movement-speed steal prediction, W attack reset and armor multiplier, E attack-speed cripple, and collision/turret/multi-target gated Unstoppable Force.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnGapcloser = &ControllerHelpers::CaptureGapcloserEvent<&GapcloserTargetId, &GapcloserEnd, &GapcloserExpireTick, 650, 900>;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1000, 250, 5000>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Malphite
