#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AISkarnerGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Skarner {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
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
inline QState QStage = QState::Ready;
inline ImpaleState RStage = ImpaleState::Idle;
inline std::array<int, kRMaximumTargets> ImpaleTargetIds{};
inline int EStunTargetId = 0;
inline int EStunExpireTick = 0;
inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastModeTick = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline float LastObservedMana = 0.0f;
inline Mode LastMode = Mode::None;

using ControllerHelpers::Now;
inline bool ResourceReady(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (mode == Mode::Harass)
        return player.ManaPercent() >= Slider(WMenu, "HarassMana", 48);
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit)
        return player.ManaPercent() >= Slider(FarmMenu, "Mana", 35);
    return player.ManaPercent() > 5.0f;
}
inline bool Ready(int slot, Mode mode) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        ResourceReady(mode);
}
inline bool Throttle(int slot, int delay = 70) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target) || target.HasBuff("SkarnerRImmune");
}
inline bool PreserveAttack(bool reactive) {
    return !reactive && Orbwalker::IsWindingUp() &&
        Bool(TacticsMenu, "PreserveAttacks", true);
}
inline bool LiveWall(const Vec3& position) { return SDK::NavMesh::IsWall(position); }
inline bool TerrainBlocked(const Vec3& origin, const Vec3& endpoint) {
    return TerrainBlocks(origin, endpoint, &LiveWall);
}
using ControllerHelpers::Lethal;
inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target,
            QRawDamage(SpellRank(0), player.BonusAttackDamage(), target.MaxHealth())) : 0.0f;
}
inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, WRawDamage(SpellRank(1), player.AP())) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, ERawDamage(SpellRank(2), player.BonusAttackDamage())) : 0.0f;
}
inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, RRawDamagePerPass(SpellRank(3), player.BonusAttackDamage())) : 0.0f;
}

inline Vector3 Aim(const AIHeroClient& target, int slot, float delay) {
    if (!Engine::ValidEnemy(target)) return {};
    Vector3 aim = PredictPosition(target, delay);
    if (slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot]) {
        const auto prediction = Engine::RuntimeSpells[slot]->GetPrediction(target);
        if (prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero() &&
            prediction.Hitchance >= SDK::HitChance::High)
            aim = prediction.GetCastPosition();
    }
    return aim;
}
inline int PredictedRHits(const Vector3& origin, const Vector3& endpoint) {
    int hits = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (hits >= kRMaximumTargets || !Engine::ValidEnemy(enemy, kRRange + 100.0f)) continue;
        if (SegmentHits(origin, endpoint, PredictPosition(enemy, kRDelay),
                        kRWidth * 0.5f, enemy.BoundingRadius())) ++hits;
    }
    return hits;
}
inline bool FirstEnemyCollisionIs(const Vector3& origin, const Vector3& endpoint,
                                  const AIHeroClient& target) {
    std::array<CollisionResult, 8> candidates{};
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (count >= static_cast<int>(candidates.size()) || !Engine::ValidEnemy(enemy, kERange + 100.0f)) continue;
        const auto position = PredictPosition(enemy, 0.20f);
        candidates[count++] = {SegmentHits(origin, endpoint, position,
            kEWidth * 0.5f, enemy.BoundingRadius()),
            static_cast<int>(enemy.NetworkId()), position};
    }
    const auto first = FirstCollision(origin, endpoint, candidates);
    return first.Hit && first.NetworkId == static_cast<int>(target.NetworkId());
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || QStage != QState::Ready || !Ready(0, mode) ||
        !Throttle(0) || PreserveAttack(reactive) ||
        (Engine::ValidEnemy(target) && player.Position().Distance2D(target.Position()) > kQRange + target.BoundingRadius()))
        return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    QStage = QStateAfterCast();
    LastCastTick[0] = Now();
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1) || PreserveAttack(reactive)) return false;
    const int nearby = Engine::CountEnemiesAt(player.Position(), kWRadius);
    if (!reactive && nearby <= 0 && !Engine::ValidEnemy(target, kWRadius + 80.0f)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(2, mode) || !Throttle(2) ||
        PreserveAttack(reactive)) return false;
    const Vector3 aim = Aim(target, 2, 0.20f);
    const Vector3 endpoint = PredictedEndpoint(player.Position(), aim);
    if (!endpoint.IsValid() || endpoint.IsZero() || player.Position().Distance2D(endpoint) > kERange + 1.0f ||
        TerrainBlocked(player.Position(), endpoint) || !FirstEnemyCollisionIs(player.Position(), endpoint, target)) return false;
    const bool lethal = Lethal(target, EDamage(target));
    const EContext context{true, true, aim.IsValid(), true,
        DashEndpointSafe(player.Position(), endpoint, true, !SDK::NavMesh::IsWall(endpoint),
            Engine::UnderEnemyTurret(player.Position()), Engine::UnderEnemyTurret(endpoint),
            Engine::CountEnemiesAt(endpoint, 325.0f), Slider(EMenu, "MaxEndpointEnemies", 2),
            lethal, reactive), reactive, lethal,
        Engine::CountEnemiesAt(endpoint, kEStunRadius)};
    EStunTargetId = static_cast<int>(target.NetworkId());
    EStunExpireTick = Now() + kEStunDurationMs;
    LastCastTick[2] = Now();
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool manual = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || RStage != ImpaleState::Idle ||
        !Ready(3, mode) || !Throttle(3, 120) || PreserveAttack(reactive)) return false;
    const Vector3 aim = Aim(target, 3, kRDelay);
    const Vector3 endpoint = PredictedEndpoint(player.Position(), aim, kRRange);
    if (!endpoint.IsValid() || endpoint.IsZero() || TerrainBlocked(player.Position(), endpoint)) return false;
    const int hits = PredictedRHits(player.Position(), endpoint);
    const bool lethal = Lethal(target, RDamage(target));
    const RContext context{true, true, aim.IsValid(), true, Orbwalker::IsWindingUp(),
        lethal, reactive, manual, hits, Slider(RMenu, "MinimumTargets", 2)};
    if (!ShouldImpale(context) || !Engine::ControllerCastPosition(3, endpoint)) return false;
    int index = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (index >= kRMaximumTargets || !Engine::ValidEnemy(enemy, kRRange + 100.0f)) continue;
        if (SegmentHits(player.Position(), endpoint, PredictPosition(enemy, kRDelay),
                        kRWidth * 0.5f, enemy.BoundingRadius()))
            ImpaleTargetIds[index++] = static_cast<int>(enemy.NetworkId());
    }
    while (index < kRMaximumTargets) ImpaleTargetIds[index++] = 0;
    RStage = ImpaleState::CastPending;
    LastCastTick[3] = Now();
    return true;
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, EDamage(target)) && CastE(target, mode)) return true;
    if (Lethal(target, WDamage(target)) && CastW(target, mode)) return true;
    return Lethal(target, RDamage(target)) && CastR(target, mode);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(WMenu, "HarassMana", 48)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (CastW(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target) && CastE(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastR(target, Mode::Flee, true, true);
}
inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    LastObservedMana = player.ManaPercent();
    if (player.HasBuff("SkarnerQ") || player.HasBuff("SkarnerQEmpowered")) QStage = QState::Empowered;
    if (player.HasBuff("SkarnerQRock") || player.HasBuff("SkarnerQ3")) QStage = QState::RockReady;
    if (QStage != QState::Ready && now > LastCastTick[0] + static_cast<int>(kQAttackWindow * 1000.0f)) QStage = QState::Ready;
    bool impaleObserved = player.HasBuff("SkarnerR") || player.HasBuff("SkarnerImpale");
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        if (enemy.HasBuff("SkarnerImpale") || enemy.HasBuff("SkarnerR")) impaleObserved = true;
        if (enemy.HasBuff("SkarnerEStun") || enemy.HasBuff("SkarnerStun")) {
            EStunTargetId = static_cast<int>(enemy.NetworkId());
            EStunExpireTick = now + kEStunDurationMs;
        }
    }
    if (RStage == ImpaleState::CastPending && impaleObserved) RStage = ImpaleState::Active;
    if (RStage != ImpaleState::Idle && now > LastCastTick[3] + kRDurationMs + 700) {
        RStage = ImpaleState::Released;
        RStage = ImpaleState::Idle;
        ImpaleTargetIds.fill(0);
    }
    if (EStunExpireTick <= now) EStunTargetId = EStunExpireTick = 0;
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    LastModeTick = Now();
    ReconcileState();
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, mode == Mode::Flee ? 1000.0f : kERange + 100.0f);
    if (ManualOwnershipUntil > Now()) return true;
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target)) {
        if (CastW(target, mode, true)) return true;
        if (CastE(target, mode, true)) return true;
    }
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1000.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (GameObjects::Player().ManaPercent() >= Slider(FarmMenu, "Mana", 35))
            (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (AutomaticAllowed({IncomingThreatUntil > Now(), IncomingHardCCUntil > Now(),
            Engine::ValidEnemy(target) && Lethal(target, RDamage(target)), false,
            ManualOwnershipUntil > Now()})) {
            if (IncomingThreatUntil > Now() && CastW(target, mode, true)) return true;
            (void)CastR(target, mode, true);
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
        if (!Engine::WasControllerCast(slot)) ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        LastCastTick[slot] = now;
        if (slot == 0) QStage = QStateAfterCast();
        if (slot == 3) RStage = ImpaleState::CastPending;
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(
        IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int id = args.Sender.IsValid() ? static_cast<int>(args.Sender.NetworkId) : 0;
    if (Engine::TextContains(args.BuffName, "SkarnerQRock") || Engine::TextContains(args.BuffName, "SkarnerQ3")) QStage = QState::RockReady;
    if (Engine::TextContains(args.BuffName, "SkarnerEStun") || Engine::TextContains(args.BuffName, "SkarnerStun")) {
        EStunTargetId = id; EStunExpireTick = Now() + kEStunDurationMs;
    }
    if (Engine::TextContains(args.BuffName, "SkarnerImpale") || Engine::TextContains(args.BuffName, "SkarnerR")) {
        for (auto& target : ImpaleTargetIds) if (!target) { target = id; break; }
        RStage = ImpaleState::Active;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "SkarnerQ")) QStage = QState::Ready;
    if (Engine::TextContains(args.BuffName, "SkarnerImpale") || Engine::TextContains(args.BuffName, "SkarnerR")) {
        RStage = ImpaleState::Released; ImpaleTargetIds.fill(0);
    }
    if (Engine::TextContains(args.BuffName, "SkarnerEStun") || Engine::TextContains(args.BuffName, "SkarnerStun")) EStunTargetId = EStunExpireTick = 0;
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (QStage != QState::Ready && args.Target.IsValid())
        QStage = QStateAfterAttack(QStage);
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kERange, 0xFF66CCFFu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFFFAA44u, 1.5f, 32);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("SkarnerOneTrick", "Skarner crystal tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuBool("PreserveAttacks", "Preserve AA windup", true));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Shattered Earth"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Seismic Bastion"));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 48, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Ixtal's Impact"));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 2, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Impale"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum predicted impale targets", 2, 1, 3));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("SkarnerFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("SkarnerCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw E and R ranges", false));
}
inline void OnLoad() {
    QStage = QState::Ready; RStage = ImpaleState::Idle; ImpaleTargetIds.fill(0);
    EStunTargetId = EStunExpireTick = ManualOwnershipUntil = 0;
    IncomingThreatUntil = IncomingHardCCUntil = LastAutoTargetId = LastAutoTick = 0;
    LastModeTick = 0; LastObservedMana = 0.0f; LastMode = Mode::None;
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    ImpaleTargetIds.fill(0); QStage = QState::Ready; RStage = ImpaleState::Idle;
}

inline constexpr const char* Scenarios[] = {
    "Pin mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Reconcile Q empowered and third-attack rock states from buffs, attacks and polling",
    "Preserve Q's five-second attack window and do not overwrite a player-owned cast",
    "Use W shield and radius damage only when proximity is relevant or incoming danger is observed",
    "Predict E endpoint with live delay and speed before committing the dash",
    "Reject E through sampled terrain and reject endpoint walls or unsafe turrets",
    "Require E collision ownership by the first predicted enemy on the line",
    "Track E stun target and expiry from events plus enemy buff polling",
    "Preserve selected target before orbwalker target and selector fallback",
    "Predict R line contact and cap impale target set at three champions",
    "Reserve nonlethal R for configured multi-target value",
    "Allow lethal, defensive, interrupt and explicit manual-assist R exceptions",
    "Reconcile R cast-pending, active suppression and released states",
    "Track each impaled target by network id and clear stale entries",
    "Apply AA windup preservation except reactive, lethal or manual ownership paths",
    "Reject invulnerable, untargetable, spell-shielded or uncertain targets",
    "Reconcile mana observations and runtime cooldown readiness before every cast",
    "Combo orders Q, W, collision-safe E, then multi-target R",
    "Harass preserves a configurable mana reserve and never starts a fresh R engage",
    "LaneClear and Jungle delegate to the shared farm policy after mana reserve checks",
    "LastHit delegates only to shared farm policy and never spends R",
    "Flee uses W peel, safe E collision and manual-assist R only",
    "Automatic mode performs defense, interrupt or kill secure without fresh engage",
    "Never automate movement ownership, items or summoner spells",
    "Draw E and R safety ranges without modifying decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Skarner";
    controller.ControllerId = "champion.kuroaio.ai.skarner.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AISkarner.md";
    controller.ImplementationSummary =
        "Q state and attack reconciliation, terrain-safe E collision, tracked stun and conservative multi-target Impale policy.";
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
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Skarner
