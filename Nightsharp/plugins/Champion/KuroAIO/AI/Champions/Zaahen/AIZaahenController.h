#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIZaahenGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Zaahen {

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

inline int PassiveStacks = 0;
inline int PassiveLastDamageTick = 0;
inline int PassiveExpireTick = 0;
inline bool PassiveReviveReady = false;
inline bool PassiveReviving = false;
inline bool PassiveReviveCooldown = false;
inline bool QArmed = false;
inline bool QRecastReady = false;
inline int QCastTick = 0;
inline int QRecastUnlockTick = 0;
inline int QExpireTick = 0;
inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int LastTargetId = 0;
inline Mode LastMode = Mode::None;
inline Vector3 LastWAim{};
inline Vector3 LastEAim{};
inline Vector3 LastRAim{};

using ControllerHelpers::Now;
inline bool Ready(int slot, Mode mode, bool allowWindup = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        (allowWindup || !Orbwalker::IsWindingUp());
}
inline bool Throttle(int slot, int delay = 80) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}
using ControllerHelpers::Protected;
inline bool ManaOkay(int slot, Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (mode == Mode::Harass) return player.ManaPercent() >= Slider(QMenu, "HarassMana", 42);
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit)
        return player.ManaPercent() >= Slider(FarmMenu, "Mana", 35);
    return true;
}
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::BonusAttackDamage;
inline float QDamage(const AIHeroClient& target, bool recast = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = recast ? QRecastRawDamage(SpellRank(0), player.TotalAttackDamage()) :
                              QInitialRawDamage(SpellRank(0), player.TotalAttackDamage());
    return player.CalculatePhysicalDamage(target, raw);
}
inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, WInitialRawDamage(SpellRank(1), BonusAttackDamage()) +
            WSecondaryRawDamage(SpellRank(1), BonusAttackDamage())) : 0.0f;
}
inline float EDamage(const AIHeroClient& target, bool sweet = true) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, ERawDamage(SpellRank(2), BonusAttackDamage(), sweet)) : 0.0f;
}
inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, RRawDamage(SpellRank(3), BonusAttackDamage())) : 0.0f;
}
using ControllerHelpers::Lethal;
inline bool PassiveMaxed() { return PassiveStacks >= kPassiveMaximumStacks; }
inline bool InRevive() { return PassiveReviving || (GameObjects::Player().IsValid() &&
    GameObjects::Player().HasBuff("ZaahenPassiveRevive")); }

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || QArmed || !ManaOkay(0, mode) ||
        !Ready(0, mode, true) || !Throttle(0) || PreserveAttack(reactive)) return false;
    if (!ControllerHelpers::InAutoAttackRange(target, kQBonusRange + 15.0f)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    const int now = Now();
    QArmed = true;
    QRecastReady = false;
    QCastTick = now;
    QRecastUnlockTick = now + static_cast<int>(kQRecastDelaySeconds * 1000.0f);
    QExpireTick = now + static_cast<int>(kQWindowSeconds * 1000.0f);
    LastCastTick[0] = now;
    LastTargetId = static_cast<int>(target.NetworkId());
    return true;
}
inline bool CastQRecast(const AIHeroClient& target, Mode mode, bool reactive = false) {
    if (!QArmed || !QRecastReady || Now() < QRecastUnlockTick ||
        Now() > QExpireTick || Protected(target) || !ManaOkay(0, mode) ||
        !Ready(0, mode, true) || !Throttle(0, 40) || PreserveAttack(reactive)) return false;
    if (!ControllerHelpers::InAutoAttackRange(target, kQBonusRange + 20.0f)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    QArmed = false;
    QRecastReady = false;
    QCastTick = QRecastUnlockTick = QExpireTick = 0;
    LastCastTick[0] = Now();
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !ManaOkay(1, mode) ||
        !Ready(1, mode, reactive) || !Throttle(1) || PreserveAttack(reactive)) return false;
    const Vector3 aim = PredictPosition(target, kWDelay);
    if (!WHits(player.Position(), aim, target.Position(), target.BoundingRadius()) ||
        SDK::NavMesh::IsWallBetween(player.Position(), aim, kWHalfWidth)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    LastWAim = aim;
    LastCastTick[1] = Now();
    LastTargetId = static_cast<int>(target.NetworkId());
    return true;
}
inline bool SafeDashEndpoint(const Vector3& endpoint, bool defensive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || endpoint.IsZero() || !endpoint.IsValid() ||
        SDK::NavMesh::IsWall(endpoint)) return false;
    if (!defensive && Engine::UnderEnemyTurret(endpoint) &&
        !Engine::UnderEnemyTurret(player.Position())) return false;
    return Engine::CountEnemiesAt(endpoint, 250.0f) <= Slider(EMenu, "MaxEndpointEnemies", 2);
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !ManaOkay(2, mode) || !Ready(2, mode, reactive) ||
        !Throttle(2) || PreserveAttack(reactive) || Protected(target)) return false;
    const bool defensive = reactive || mode == Mode::Flee ||
        player.HealthPercent() <= Slider(EMenu, "DefensiveHealth", 30);
    const Vector3 endpoint = ClampDash(player.Position(), PredictPosition(target, 0.10f));
    const bool sweet = InSweetSpot(endpoint, PredictPosition(target, 0.10f), target.BoundingRadius());
    const DashContext context{true, endpoint.IsValid() && !endpoint.IsZero(),
        SafeDashEndpoint(endpoint, defensive), sweet,
        Engine::UnderEnemyTurret(endpoint) && !Engine::UnderEnemyTurret(player.Position()),
        Engine::UnderEnemyTurret(player.Position()), Engine::CountEnemiesAt(endpoint, 250.0f),
        Slider(EMenu, "MaxEndpointEnemies", 2), defensive,
        lethal || Lethal(target, EDamage(target, true))};
    if (!ShouldRush(context)) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    LastEAim = endpoint;
    LastCastTick[2] = Now();
    LastTargetId = static_cast<int>(target.NetworkId());
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !ManaOkay(3, mode) ||
        !Ready(3, mode, reactive) || !Throttle(3, 140) || PreserveAttack(reactive)) return false;
    const bool defensive = reactive || mode == Mode::Flee ||
        player.HealthPercent() <= Slider(RMenu, "DefensiveHealth", 30);
    const Vector3 aim = RLanding(player.Position(), PredictPosition(target, kRDelay));
    if (aim.IsZero() || SDK::NavMesh::IsWall(aim)) return false;
    int hits = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy, kRRange + 100.0f) &&
            aim.Distance2D(PredictPosition(enemy, kRDelay)) <= kRRadius + enemy.BoundingRadius()) ++hits;
    }
    const UltimateContext context{true, true, true,
        SafeDashEndpoint(aim, defensive), player.HealthPercent() <= 30.0f,
        Lethal(target, RDamage(target)), defensive, hits,
        Slider(RMenu, "MinimumTargets", 2)};
    if (!ShouldDeliver(context)) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    LastRAim = aim;
    LastCastTick[3] = Now();
    LastTargetId = static_cast<int>(target.NetworkId());
    return true;
}
inline void RefreshPassive() {
    const auto player = GameObjects::Player();
    const int now = Now();
    if (!player.IsValid()) return;
    const int observed = ControllerHelpers::MaximumBuffCount(player,
        {"ZaahenPassive", "ZaahenPassiveBuff", "ZaahenDetermination"});
    if (observed > 0) {
        PassiveStacks = ClampPassiveStacks(observed);
        PassiveLastDamageTick = now;
        PassiveExpireTick = now + static_cast<int>(kPassiveStackSeconds * 1000.0f);
    } else if (PassiveStacks > 0 && PassiveLastDamageTick > 0) {
        const float elapsed = static_cast<float>(now - PassiveLastDamageTick) / 1000.0f;
        PassiveStacks = PassiveStacksAfterElapsed(PassiveStacks, elapsed);
        if (PassiveStacks == 0) PassiveExpireTick = 0;
    }
    PassiveReviveReady = player.HasBuff("ZaahenPassiveReviveReady");
    PassiveReviving = player.HasBuff("ZaahenPassiveRevive");
    PassiveReviveCooldown = player.HasBuff("ZaahenPassiveReviveCooldown");
}
inline void ReconcileState() {
    const int now = Now();
    RefreshPassive();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (QArmed && now > QExpireTick) {
        QArmed = false;
        QRecastReady = false;
        QCastTick = QRecastUnlockTick = QExpireTick = 0;
    }
    if (QArmed && now >= QRecastUnlockTick && LastAutoTargetId != 0) QRecastReady = true;
    if (PassiveExpireTick > 0 && now > PassiveExpireTick && PassiveStacks <= 0) PassiveLastDamageTick = 0;
}
inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target) || Protected(target)) return false;
    if (QRecastReady && Lethal(target, QDamage(target, true)) && CastQRecast(target, mode, true)) return true;
    if (Lethal(target, WDamage(target)) && CastW(target, mode, true)) return true;
    if (Lethal(target, EDamage(target, true)) && CastE(target, mode, true, true)) return true;
    return Lethal(target, RDamage(target)) && CastR(target, mode, true);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (QRecastReady && CastQRecast(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (!QArmed && CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || !ManaOkay(1, Mode::Harass)) return;
    if (QRecastReady && CastQRecast(target, Mode::Harass)) return;
    if (CastW(target, Mode::Harass)) return;
    if (!QArmed) (void)CastQ(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastE(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastW(target, Mode::Flee, true);
}
inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    LastMode = mode;
    ReconcileState();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const AIHeroClient target = Engine::SelectTarget(
        mode == Mode::Flee ? 1000.0f : 900.0f);
    if (InRevive()) return true;
    if (Engine::ValidEnemy(target) &&
        (IncomingThreatUntil > Now() || IncomingHardCCUntil > Now()) &&
        CastW(target, mode, true)) return true;
    if (Engine::ValidEnemy(target) && TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer({}, 1000.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (ManaOkay(2, mode)) (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic: {
        const bool defensive = IncomingThreatUntil > Now() || IncomingHardCCUntil > Now() ||
            player.HealthPercent() <= Slider(RMenu, "DefensiveHealth", 30);
        const bool kill = Engine::ValidEnemy(target) &&
            (Lethal(target, EDamage(target, true)) || Lethal(target, RDamage(target)));
        if (AutomaticAllowed({defensive, IncomingHardCCUntil > Now(), kill, false})) {
            if (defensive && Engine::ValidEnemy(target) && CastE(target, mode, true, kill)) return true;
            if (Engine::ValidEnemy(target)) (void)CastR(target, mode, true);
        }
        break;
    }
    default: break;
    }
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[slot] = now;
            if (slot == 0) {
                if (QArmed && now >= QRecastUnlockTick) QRecastReady = true;
                else if (!QArmed) {
                    QArmed = true;
                    QCastTick = now;
                    QRecastUnlockTick = now + static_cast<int>(kQRecastDelaySeconds * 1000.0f);
                    QExpireTick = now + static_cast<int>(kQWindowSeconds * 1000.0f);
                }
            }
        }
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
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (ControllerHelpers::TextContainsAny(args.BuffName,
        {"ZaahenPassiveReviveReady", "ZaahenPassiveReady"})) PassiveReviveReady = true;
    if (ControllerHelpers::TextContainsAny(args.BuffName, {"ZaahenPassiveRevive"})) PassiveReviving = true;
    RefreshPassive();
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid() || !IsLocalPlayer(args.Sender)) return;
    if (ControllerHelpers::TextContainsAny(args.BuffName, {"ZaahenPassiveReviveReady"})) PassiveReviveReady = false;
    if (ControllerHelpers::TextContainsAny(args.BuffName, {"ZaahenPassiveReviveCooldown"})) PassiveReviveCooldown = false;
    if (ControllerHelpers::TextContainsAny(args.BuffName, {"ZaahenPassiveRevive"})) PassiveReviving = false;
    RefreshPassive();
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
    LastAutoTick = Now();
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick)) return;
    const auto target = args.Target;
    if (target.IsValid() && target.IsHero()) {
        PassiveStacks = std::min(kPassiveMaximumStacks, PassiveStacks + 1);
        PassiveLastDamageTick = Now();
        PassiveExpireTick = Now() + static_cast<int>(kPassiveStackSeconds * 1000.0f);
        if (QArmed && Now() >= QRecastUnlockTick) QRecastReady = true;
    }
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kWRange, 0xFFBB77FFu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFCC4477u, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kERange, 0xFFAA66CCu, 1.0f, 32);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("ZaahenTactics", "Zaahen determination tactics"));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Darkin Glaive"));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 42, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Dreaded Return"));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Aureate Rush"));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum endpoint enemies", 2, 1, 5));
    EMenu->Add(new MenuSlider("DefensiveHealth", "Defensive health percent", 30, 5, 70));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Grim Deliverance"));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum landing targets", 2, 1, 5));
    RMenu->Add(new MenuSlider("DefensiveHealth", "Defensive health percent", 30, 5, 70));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("ZaahenFarm", "Farm resources"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum farm mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("ZaahenCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw W, E and R ranges", false));
}
inline void OnLoad() {
    PassiveStacks = PassiveLastDamageTick = PassiveExpireTick = 0;
    PassiveReviveReady = PassiveReviving = PassiveReviveCooldown = false;
    QArmed = QRecastReady = false;
    QCastTick = QRecastUnlockTick = QExpireTick = 0;
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = 0;
    IncomingThreatUntil = IncomingHardCCUntil = LastTargetId = 0;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
}
inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Reconcile Determination stacks, five-second refresh and half-second falloff",
    "Respect twelve-stack bonus AD multiplier and revive-ready/revive/cooldown buffs",
    "Use Q first-cast empowerment and 1.5-second recast attack reset windows",
    "Preserve the engine-selected target and never cancel an ordinary AA windup",
    "Use W 850-range line, collision, final stun and 225-unit pull geometry",
    "Use E 350-range dash with 200-375 outer sweet spot and safe endpoint limits",
    "Reject E endpoints through walls, enemy turrets and excessive enemy count",
    "Use R 600-range dash, 550 landing zone, damage reduction and champion healing",
    "Require lethal, defensive or configured multi-target value before R landing",
    "Track cooldowns and event-reconciled target state with polling",
    "Use the engine-selected target with orbwalker attack timing",
    "Combo sequences W pull, Q attacks, E sweet spot and R commitment",
    "Harass uses W and Q while respecting mana and avoiding unsolicited R",
    "LaneClear Jungle and LastHit delegate to shared farm policy",
    "Flee uses defensive E and W peel without turret diving",
    "Automatic mode only reacts to defense, hard crowd control or kill secure",
    "Reject invulnerable, protected and spell-shielded targets",
    "Use endpoint safety gates without forced movement",
    "Never automate items, summoners or unrelated movement decisions",
    "Draw range guidance without changing decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Zaahen;
    controller.ControllerId = "champion.kuroaio.ai.zaahen.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIZaahen.md";
    controller.ImplementationSummary =
        "Determination and revive reconciliation, Q attack-reset sequencing, W pull geometry, E sweet-spot safety and R landing commitment in an owned loop.";
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

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<
        &LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Zaahen
