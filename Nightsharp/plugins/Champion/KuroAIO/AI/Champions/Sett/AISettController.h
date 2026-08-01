#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AISett.h"
#include "AISettGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Sett {

using namespace Geometry;
using ::Plugins::KuroAIO::Bool;
using ::Plugins::KuroAIO::Slider;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::TextContainsAny;

// The Q buff is target-neutral in the runtime, so retain the attack target
// observed at arming time and reject unrelated attack events.

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline QState CurrentQ = QState::Ready;
inline WState CurrentW = WState::Ready;
inline float Grit = 0.0f;
inline float ActiveWShield = 0.0f;
inline int QHitsRemaining = 0;
inline int QExpireTick = 0;
inline int QTargetId = 0;
inline int LastQConsumeTick = 0;
inline int WShieldExpireTick = 0;
inline int WCastTick = 0;
inline int LastAutoTargetId = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingThreatTargetId = 0;
inline Vector3 IncomingThreatEndpoint{};
inline Vector3 LastRLanding{};
inline int RTargetId = 0;
inline std::array<int, 4> LastCastTick{};

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    if (slot < 0 || slot >= 4 || !Engine::RuntimeSpells[slot] ||
        !Engine::RuntimeSpells[slot]->IsReady() || !SpellEnabled(slot, mode)) return false;
    return reactive || ControllerHelpers::CastThrottleReady(LastCastTick, slot, 35);
}

inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline float MaxGrit() {
    const auto player = GameObjects::Player();
    return player.IsValid() ? GritCap(player.MaxHealth()) : 0.0f;
}

inline float QDamageEstimate(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(target,
        QDamage(SpellRank(0), target.MaxHealth(), player.BonusAttackDamage()));
}

inline float WDamageEstimate(const AIHeroClient& target, bool center) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = WDamage(SpellRank(1), player.BonusAttackDamage(), Grit, center);
    return center ? raw : player.CalculatePhysicalDamage(target, raw);
}

inline float EDamageEstimate(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    static constexpr std::array<float, 5> base{50.0f, 70.0f, 90.0f, 110.0f, 130.0f};
    const int rank = std::clamp(SpellRank(2), 1, 5);
    return player.CalculatePhysicalDamage(target,
        base[static_cast<std::size_t>(rank - 1)] + 0.60f * player.BonusAttackDamage());
}

inline float RDamageEstimate(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(target,
        RDamage(SpellRank(3), target.MaxHealth(), player.BonusAttackDamage()));
}

inline bool SafeEndpoint(const Vector3& endpoint, bool lethal, bool fleeing) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero() || SDK::NavMesh::IsWall(endpoint))
        return false;
    if (Engine::UnderEnemyTurret(endpoint) && !Engine::UnderEnemyTurret(player.Position()) &&
        !lethal && !fleeing) return false;
    return lethal || fleeing || Engine::CountEnemiesAt(endpoint, kRLandingRadius) <=
        Slider(TacticsMenu, "MaximumEnemies", 3);
}

inline AIHeroClient OppositeSideTarget(const AIHeroClient& primary, float range) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    const Vector3 direction = Direction2D(player.Position(), primary.Position());
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range) || enemy.NetworkId() == primary.NetworkId()) continue;
        if (direction.IsZero() || (enemy.Position() - player.Position()).Dot(direction) < 0.0f)
            return enemy;
    }
    return {};
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !Ready(0, mode, reactive) ||
        !ControllerHelpers::InAutoAttackRange(target) ||
        ControllerHelpers::PreserveAttack(reactive) || QHitsRemaining > 0) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    CurrentQ = QState::Armed;
    QHitsRemaining = 2;
    QTargetId = static_cast<int>(target.NetworkId());
    QExpireTick = Now() + 4000;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, 805.0f) || Protected(target) ||
        !Ready(1, mode, reactive) || ControllerHelpers::PreserveAttack(reactive)) return false;
    const Vector3 aim = PredictPosition(target, 0.75f);
    if (!aim.IsValid() || aim.IsZero()) return false;
    const bool center = WCenterHit(player.Position(), aim, target.Position(), target.BoundingRadius());
    const float damage = center ? WDamageEstimate(target, true) : WDamageEstimate(target, false);
    const bool lethal = damage >= target.Health() + target.AllShield();
    const bool threatened = IncomingThreatUntil >= Now() ||
        player.HealthPercent() <= Slider(TacticsMenu, "HaymakerHealth", 58) ||
        Engine::CountEnemiesAt(player.Position(), 425.0f) >= 2;
    if (!WCastAllowed(Grit, player.MaxHealth(), true, threatened, lethal)) return false;
    if (!Engine::ControllerCastPosition(1, aim)) return false;
    CurrentW = WState::Fired;
    WCastTick = LastCastTick[1] = Now();
    WShieldExpireTick = WCastTick + 3000;
    ActiveWShield = WShield(Grit, player.MaxHealth());
    Grit = 0.0f;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange) || Protected(target) ||
        !Ready(2, mode, reactive) || ControllerHelpers::PreserveAttack(reactive)) return false;
    const Vector3 aim = PredictPosition(target, 0.25f);
    const AIHeroClient second = OppositeSideTarget(target, kERange);
    const EOutcome outcome = FacebreakerOutcome(player.Position(), aim, target.Position(),
                                                second.IsValid() ? second.Position() : Vector3{});
    if (outcome == EOutcome::Miss) return false;
    const bool lethal = EDamageEstimate(target) >= target.Health() + target.AllShield();
    if (!SafeEndpoint(player.Position(), lethal, mode == Mode::Flee)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kRRange) || Protected(target) ||
        !Ready(3, mode, reactive) || ControllerHelpers::PreserveAttack(reactive)) return false;
    const Vector3 endpoint = RLandingEndpoint(player.Position(), target.Position());
    const float damage = RDamageEstimate(target);
    const bool lethal = damage >= target.Health() + target.AllShield();
    const bool fleeing = mode == Mode::Flee;
    if (!RCommitAllowed(true, false, endpoint.IsValid() && !endpoint.IsZero(),
                        SDK::NavMesh::IsWall(endpoint), Engine::UnderEnemyTurret(endpoint),
                        Engine::CountEnemiesAt(endpoint, kRCraterRadius),
                        Slider(TacticsMenu, "MaximumEnemies", 3), lethal, fleeing) ||
        !SafeEndpoint(endpoint, lethal, fleeing)) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    LastRLanding = endpoint;
    RTargetId = static_cast<int>(target.NetworkId());
    LastCastTick[3] = Now();
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, 805.0f)) return;
    if (CastR(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    (void)CastE(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kERange)) return;
    if (CastQ(target, Mode::Harass)) return;
    if (CastE(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}

inline void Farm(Mode mode, const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.HealthPercent() < Slider(FarmMenu, "MinimumHealth", 35)) return;
    if (mode == Mode::Jungle && Engine::ValidEnemy(target, kERange) && CastE(target, mode)) return;
    if (Engine::ValidEnemy(target, kQRange) && CastQ(target, mode)) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target, kERange) && CastE(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target, 805.0f) && CastW(target, Mode::Flee, true)) return;
    (void)CastQ(target, Mode::Flee, true);
}

inline void Automatic(const AIHeroClient& target) {
    if (ManualOwnershipUntil > Now()) return;
    if (Engine::ValidEnemy(target)) {
        if (IncomingThreatUntil >= Now() && CastW(target, Mode::Automatic, true)) return;
        if (CastR(target, Mode::Automatic, true)) return;
        if (GameObjects::Player().HealthPercent() <= 30.0f) (void)CastW(target, Mode::Automatic, true);
    }
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (QHitsRemaining > 0 && now > QExpireTick) {
        QHitsRemaining = 0;
        QTargetId = 0;
        CurrentQ = QState::Expired;
    }
    if (CurrentQ == QState::Expired) CurrentQ = QState::Ready;
    if (CurrentW == WState::Fired && now > WShieldExpireTick) {
        CurrentW = WState::Expired;
        ActiveWShield = 0.0f;
    }
    if (CurrentW == WState::Expired) CurrentW = WState::Ready;
    if (IncomingThreatUntil < now) {
        IncomingThreatUntil = 0;
        IncomingThreatTargetId = 0;
        IncomingThreatEndpoint = {};
    }
    if (Grit > 0.0f && now > WCastTick + 250)
        Grit = DecayGrit(Grit, 0.016f);
    const float cap = MaxGrit();
    if (cap > 0.0f) Grit = std::min(Grit, cap);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = PreferredEnemyTarget(selected, mode == Mode::Flee ? 900.0f : 805.0f);
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
    TacticsMenu = root->AddSubMenu(new Menu("SettGrit", "Sett grit tactics"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("SettFarm", "Sett farm safety"));
    TacticsMenu->Add(new MenuSlider("HaymakerHealth", "Use Haymaker under health percent", 58, 1, 100));
    TacticsMenu->Add(new MenuSlider("MaximumEnemies", "Maximum enemies at slam endpoint", 3, 0, 5));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Manual cast protection (ms)", 700, 0, 2000));
    TacticsMenu->Add(new MenuBool("PreserveAttacks", "Preserve valuable attack windups", true));
    FarmMenu->Add(new MenuSlider("MinimumHealth", "Minimum health percent for farm casts", 35, 1, 100));
}

inline void OnLoad() {
    CurrentQ = QState::Ready;
    CurrentW = WState::Ready;
    Grit = 0.0f;
    ActiveWShield = 0.0f;
    QHitsRemaining = QExpireTick = QTargetId = LastQConsumeTick = 0;
    WShieldExpireTick = WCastTick = LastAutoTargetId = 0;
    ManualOwnershipUntil = IncomingThreatUntil = IncomingThreatTargetId = 0;
    IncomingThreatEndpoint = LastRLanding = {};
    RTargetId = 0;
    LastCastTick.fill(0);
}

inline void OnUnload() {
    TacticsMenu = nullptr;
    FarmMenu = nullptr;
    OnLoad();
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            if (!Engine::WasControllerCast(slot))
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 700);
            LastCastTick[static_cast<std::size_t>(slot)] = now;
            if (slot == 0) {
                CurrentQ = QState::Armed;
                QHitsRemaining = 2;
                QTargetId = 0;
                LastQConsumeTick = 0;
                QExpireTick = now + 4000;
            }
            if (slot == 1) {
                CurrentW = WState::Fired;
                WCastTick = now;
                WShieldExpireTick = now + 3000;
                const auto player = GameObjects::Player();
                ActiveWShield = player.IsValid() ? WShield(Grit, player.MaxHealth()) : 0.0f;
                Grit = 0.0f;
            }
            if (slot == 3) RTargetId = static_cast<int>(args.TargetNetworkId);
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) {
        IncomingThreatTargetId = static_cast<int>(args.Sender.NetworkId);
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
        IncomingThreatEndpoint = args.EndPosition;
        const auto player = GameObjects::Player();
        if (player.IsValid())
            Grit = AddGrit(Grit, player.MaxHealth() * 0.05f, player.MaxHealth());
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack || QHitsRemaining <= 0) return;
    LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
    if (QTargetId != 0 && LastAutoTargetId != 0 && QTargetId != LastAutoTargetId) return;
    LastQConsumeTick = Now();
    --QHitsRemaining;
    if (QHitsRemaining <= 0) {
        QTargetId = 0;
        CurrentQ = QState::Ready;
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (TextContainsAny(args.BuffName, {"settq", "settqms"})) {
        CurrentQ = QState::Armed;
        QHitsRemaining = std::max(QHitsRemaining, 2);
        QExpireTick = Now() + 4000;
    }
    if (TextContainsAny(args.BuffName, {"settwshield", "settw"})) {
        CurrentW = WState::Fired;
        WShieldExpireTick = Now() + 3000;
        const auto player = GameObjects::Player();
        ActiveWShield = player.IsValid() ? WShield(Grit, player.MaxHealth()) : ActiveWShield;
    }
    if (TextContainsAny(args.BuffName, {"settwpassivebuff", "settgrit"}) && args.Count > 0)
        Grit = std::min(MaxGrit(), static_cast<float>(args.Count));
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (TextContainsAny(args.BuffName, {"settq", "settqms"})) {
        QHitsRemaining = 0;
        QTargetId = 0;
        CurrentQ = QState::Ready;
    }
    if (TextContainsAny(args.BuffName, {"settwshield", "settw"})) {
        CurrentW = WState::Ready;
        ActiveWShield = 0.0f;
    }
    if (TextContainsAny(args.BuffName, {"settwpassivebuff", "settgrit"})) Grit = 0.0f;
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (args.EndTime <= Game::Time()) OnBuffRemove(args);
    else OnBuffAdd(args);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) {
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
        if (QTargetId == 0 && QHitsRemaining > 0) QTargetId = LastAutoTargetId;
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || QHitsRemaining <= 0) return;
    const int targetId = static_cast<int>(args.Target.NetworkId());
    if (QTargetId != 0 && QTargetId != targetId) return;
    if (LastQConsumeTick > 0 && Now() - LastQConsumeTick < 100) return;
    LastQConsumeTick = Now();
    --QHitsRemaining;
    LastAutoTargetId = targetId;
    if (QHitsRemaining <= 0) {
        QTargetId = 0;
        CurrentQ = QState::Ready;
    }
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
    "Pit Grit stores and decays incoming damage with a 50 percent max-health cap",
    "Knuckle Down arms two empowered attacks and preserves the attack-reset ownership window",
    "Haymaker converts Grit into a shield and true center damage with side damage fallback",
    "Facebreaker pulls targets from opposite sides and distinguishes stun from one-side slow",
    "Showstopper computes the target-directed slam endpoint and rejects unsafe landings",
    "selected target precedence with orbwalker fallback and prediction-aware reach checks",
    "attack windup preservation, manual ownership and event plus polling reconciliation",
    "wall, projectile, turret and nearby-enemy safety gates protect W/E/R commitments",
    "resource, cooldown, true-damage, shield and missing-health lethal boundaries",
    "combo, harass, lane clear, jungle, last-hit, flee and automatic mode policies",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Sett;
    controller.ControllerId = "champion.kuroaio.ai.sett.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AISett.md";
    controller.ImplementationSummary =
        "Grit resource ledger with decay and cap, two-hit Q reset ownership, true-center Haymaker "
        "damage/shield accounting, opposite-side Facebreaker displacement, and safe Showstopper slam endpoints.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Sett
