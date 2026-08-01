#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIVolibear.h"
#include "AIVolibearGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Volibear {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::Slider;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;

using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline PassiveState CurrentPassive = PassiveState::Inactive;
inline QState CurrentQ = QState::Ready;
inline WState CurrentW = WState::Ready;
inline EState CurrentE = EState::Ready;
inline RState CurrentR = RState::Ready;
inline int PassiveStacks = 0;
inline int PassiveTick = 0;
inline int QCastTick = 0;
inline int WCastTick = 0;
inline int WMarkTargetId = 0;
inline int WMarkTick = 0;
inline int ECastTick = 0;
inline int RCastTick = 0;
inline int RTargetId = 0;
inline int RDisableUntil = 0;
inline int ManualOwnershipUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline Vector3 ECenter{};
inline Vector3 REndpoint{};
inline std::array<int, 4> LastCastTick{};

inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        (reactive || Now() - LastCastTick[static_cast<std::size_t>(slot)] >= 45);
}

inline bool PreserveAttack(bool reactive) {
    return !reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline bool ManaGate(Mode mode, bool urgent) {
    if (urgent) return true;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int threshold = mode == Mode::Harass ? Slider(TacticsMenu, "HarassMana", 45) :
        (mode == Mode::Jungle ? Slider(FarmMenu, "JungleMana", 25) :
         (mode == Mode::LaneClear || mode == Mode::LastHit ?
              Slider(FarmMenu, "LaneMana", 35) : 0));
    return player.ManaPercent() >= static_cast<float>(threshold);
}

inline int ObservedPassiveStacks(const AIHeroClient& player) {
    if (!player.IsValid()) return 0;
    const int buffStacks = std::max(0, player.GetBuffCount("VolibearP"));
    return ClampPassiveStacks(std::max(PassiveStacks, buffStacks));
}

inline bool Marked(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) && WMarkTargetId != 0 &&
        static_cast<int>(target.NetworkId()) == WMarkTargetId &&
        WMarkActive(WMarkTargetId, WMarkTargetId, Now() - WMarkTick);
}

inline float DamageFor(const AIHeroClient& target, int slot) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    if (slot == 0) return player.CalculatePhysicalDamage(target,
        QDamage(SpellRank(0), player.TotalAttackDamage(), player.BonusAttackDamage()));
    if (slot == 1) return player.CalculatePhysicalDamage(target,
        WDamage(SpellRank(1), player.BonusAttackDamage(), Marked(target)));
    if (slot == 2) return player.CalculateMagicDamage(target,
        EDamage(SpellRank(2), player.AP(), player.MaxHealth()));
    return player.CalculateMagicDamage(target,
        RDamage(SpellRank(3), player.AP(), player.MaxHealth()));
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || PreserveAttack(reactive) ||
        !ManaGate(mode, reactive) || !Ready(0, mode, reactive) ||
        CurrentQ != QState::Ready) return false;
    const Vector3 aim = PredictPosition(target, 0.15f);
    const float distance = player.Position().Distance2D(aim);
    if (!aim.IsValid() || aim.IsZero() ||
        (distance > kQRange + target.BoundingRadius() && !fleeing)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    CurrentQ = QState::Charging;
    QCastTick = LastCastTick[0] = Now();
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || PreserveAttack(reactive) ||
        !ManaGate(mode, reactive) || !Ready(1, mode, reactive) ||
        player.Position().Distance2D(target.Position()) > kWRange + target.BoundingRadius()) return false;
    const bool recast = Marked(target);
    if (!recast && target.HealthPercent() > Slider(TacticsMenu, "WFirstHealth", 95) &&
        mode == Mode::Harass) return false;
    if (!Engine::ControllerCastUnit(1, target)) return false;
    WCastTick = LastCastTick[1] = Now();
    WMarkTargetId = static_cast<int>(target.NetworkId());
    WMarkTick = WCastTick;
    CurrentW = WState::Marked;
    (void)recast;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PreserveAttack(reactive) || !ManaGate(mode, reactive) ||
        !Ready(2, mode, reactive)) return false;
    const bool hasTarget = Engine::ValidEnemy(target, kERange + 50.0f);
    const Vector3 center = hasTarget && !fleeing ? PredictPosition(target, 1.0f) : player.Position();
    if (!center.IsValid() || center.IsZero() ||
        player.Position().Distance2D(center) > kERange + 50.0f) return false;
    const bool defensive = player.HealthPercent() <= Slider(TacticsMenu, "ShieldHealth", 55);
    if (!hasTarget && !defensive && !fleeing) return false;
    if (!Engine::ControllerCastPosition(2, center)) return false;

    ECenter = center;
    ECastTick = LastCastTick[2] = Now();
    CurrentE = EState::LightningPending;
    return true;
}

inline bool SafeREndpoint(const Vector3& endpoint, const AIHeroClient& target,
                          bool lethal, bool defensive, bool fleeing) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !endpoint.IsValid() || endpoint.IsZero()) return false;
    if (!RReachable(player.Position(), endpoint, target.BoundingRadius())) return false;
    const bool wall = SDK::NavMesh::IsWall(endpoint);
    const bool enemyTurret = Engine::UnderEnemyTurret(endpoint) &&
        !Engine::UnderEnemyTurret(player.Position());
    const int enemies = Engine::CountEnemiesAt(endpoint, kRRadius);
    return REndpointSafe(wall, enemyTurret, enemies,
        Slider(TacticsMenu, "MaxCommitEnemies", 2), lethal, defensive, fleeing, true);
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool fleeing = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || PreserveAttack(reactive) ||
        !ManaGate(mode, reactive) || !Ready(3, mode, reactive) || CurrentR != RState::Ready) return false;
    const Vector3 aim = fleeing ?
        player.Position() + SharedGeometry::Direction2D(target.Position(), player.Position()) * kRRange :
        PredictPosition(target, 0.65f);
    if (!aim.IsValid() || aim.IsZero()) return false;
    const float distance = player.Position().Distance2D(aim);
    const Vector3 endpoint = player.Position().Extend(aim, std::min(kRRange, distance));
    if (!RLandingCovers(endpoint, target.Position(), target.BoundingRadius())) return false;
    const bool lethal = Lethal(target, DamageFor(target, 3));
    const bool defensive = player.HealthPercent() <= Slider(TacticsMenu, "DefensiveHealth", 35);
    if (!SafeREndpoint(endpoint, target, lethal, defensive, fleeing)) return false;
    const bool turretEndpoint = Engine::UnderEnemyTurret(endpoint);
    const int enemies = Engine::CountEnemiesAt(endpoint, kRRadius);
    if (turretEndpoint && !RDisableAllowed(true, true, enemies,
            Slider(TacticsMenu, "MaxCommitEnemies", 2), lethal, defensive, fleeing)) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    CurrentR = RState::Leaping;
    RCastTick = LastCastTick[3] = Now();
    RTargetId = static_cast<int>(target.NetworkId());
    REndpoint = endpoint;
    RDisableUntil = RCastTick + kRDisableDurationMs;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.Position().Distance2D(target.Position()) > kWRange && CastR(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    (void)CastW(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || !ManaGate(Mode::Harass, false)) return;
    if (Marked(target) && CastW(target, Mode::Harass)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastE(target, Mode::Harass);
}

inline void Farm(Mode mode, const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !ManaGate(mode, mode == Mode::LastHit)) return;
    if (mode == Mode::Jungle) {
        const auto objective = SelectJungleTarget(kWRange, 0.15f, 100000.0f);
        if (objective.IsValid() && Engine::ControllerCastUnit(1, objective)) {
            LastCastTick[1] = WCastTick = Now();
            return;
        }
    }
    if (Engine::ValidEnemy(target) && (CastQ(target, mode) || CastW(target, mode))) return;
    (void)Engine::TryFarm(mode);
}

inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastR(target, Mode::Flee, true, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastQ(target, Mode::Flee, true, true);
    (void)CastE(target, Mode::Flee, true, true);
}

inline void Automatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return;
    if (player.HealthPercent() <= Slider(TacticsMenu, "ShieldHealth", 55) &&
        CastE(target, Mode::Automatic, true)) return;
    if (player.HealthPercent() <= Slider(TacticsMenu, "DefensiveHealth", 35))
        (void)CastR(target, Mode::Automatic, true, true);
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    const int now = Now();
    PassiveStacks = ObservedPassiveStacks(player);
    if (PassiveStacks == 0 || now - PassiveTick > kPassiveDurationMs) {
        PassiveStacks = 0;
        CurrentPassive = PassiveState::Inactive;
    } else {
        CurrentPassive = EvaluatePassive(PassiveStacks, now - PassiveTick);
    }
    if (CurrentQ == QState::Charging && !QMovementActive(CurrentQ, now - QCastTick))
        CurrentQ = QState::Ready;
    if (CurrentQ == QState::Stunned && !QStunActive(CurrentQ, now - QCastTick))
        CurrentQ = QState::Ready;
    if (WMarkTargetId != 0 && now - WMarkTick >= kWMarkDurationMs) {
        WMarkTargetId = 0;
        CurrentW = WState::Ready;
    } else if (WMarkTargetId != 0) CurrentW = WState::RecastReady;
    if (CurrentE == EState::LightningPending &&
        ELightningActive(CurrentE, now - ECastTick) &&
        ECanShield(ECenter, player.Position(), now - ECastTick)) {
        CurrentE = EState::Shielded;
    }
    if ((CurrentE == EState::LightningPending || CurrentE == EState::Shielded) &&
        now - ECastTick > kELightningDelayMs + kEShieldDurationMs) {
        CurrentE = EState::Ready;
        ECenter = {};
    }
    if (CurrentR == RState::Leaping && now - RCastTick > kRLeapMs) CurrentR = RState::Landed;
    if (CurrentR == RState::Landed && now - RCastTick > kRLeapMs + 500) {
        CurrentR = RState::Ready;
        RTargetId = 0;
        REndpoint = {};
    }
    if (RDisableUntil != 0 && !RDisableActive(now - RCastTick)) RDisableUntil = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = PreferredEnemyTarget(selected,
        mode == Mode::Flee ? kRRange + 100.0f : kERange + 100.0f);
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
    TacticsMenu = root->AddSubMenu(new Menu("Volibear storm tactics"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Volibear farming"));
    TacticsMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at R endpoint", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("ShieldHealth", "E defensive shield health percent", 55, 0, 100));
    TacticsMenu->Add(new MenuSlider("DefensiveHealth", "Emergency R health percent", 35, 0, 100));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 45, 0, 100));
    TacticsMenu->Add(new MenuSlider("WFirstHealth", "Harass W first-hit target health", 95, 0, 100));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Manual cast protection (ms)", 650, 0, 2000));
    TacticsMenu->Add(new MenuBool("PreserveAttacks", "Preserve attack windup", true));
    FarmMenu->Add(new MenuSlider("LaneMana", "Lane clear mana percent", 35, 0, 100));
    FarmMenu->Add(new MenuSlider("JungleMana", "Jungle mana percent", 25, 0, 100));
}

inline void OnLoad() {
    CurrentPassive = PassiveState::Inactive;
    CurrentQ = QState::Ready;
    CurrentW = WState::Ready;
    CurrentE = EState::Ready;
    CurrentR = RState::Ready;
    PassiveStacks = 0;
    PassiveTick = QCastTick = WCastTick = WMarkTick = ECastTick = RCastTick = 0;
    WMarkTargetId = RTargetId = RDisableUntil = ManualOwnershipUntil = 0;
    LastAutoTargetId = LastAutoTick = 0;
    ECenter = {};
    REndpoint = {};
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
        if (args.Slot >= 0 && args.Slot < 4) {
            if (!Engine::WasControllerCast(args.Slot))
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 650);
            LastCastTick[static_cast<std::size_t>(args.Slot)] = now;
            if (args.Slot == 0) { CurrentQ = QState::Charging; QCastTick = now; }
            else if (args.Slot == 1) { WCastTick = now; }
            else if (args.Slot == 2) { CurrentE = EState::LightningPending; ECastTick = now; }
            else { CurrentR = RState::Leaping; RCastTick = now; }
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer)) ManualOwnershipUntil = 0;
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return;
    LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
    LastAutoTick = Now();
    if (QCanStun(CurrentQ, Now() - QCastTick, true, false))
        CurrentQ = QState::Stunned;
    PassiveStacks = PassiveStacksAfterHit(PassiveStacks, true);
    PassiveTick = LastAutoTick;
    CurrentPassive = EvaluatePassive(PassiveStacks, 0);
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender)) {
        if (ControllerHelpers::AnyTextContains({args.BuffName}, {"volibearp", "relentlessstorm"})) {
            PassiveStacks = ClampPassiveStacks(std::max(PassiveStacks, args.Count));
            PassiveTick = Now();
            CurrentPassive = EvaluatePassive(PassiveStacks, 0);
        }
        if (ControllerHelpers::AnyTextContains({args.BuffName}, {"volibearq", "thunderingsmash"})) {
            CurrentQ = QState::Charging;
            QCastTick = Now();
        }
        if (ControllerHelpers::AnyTextContains({args.BuffName}, {"volibeare", "skysplitter"})) {
            CurrentE = EState::Shielded;
            ECastTick = Now() - kELightningDelayMs;
        }
        if (ControllerHelpers::AnyTextContains({args.BuffName}, {"volibearr", "stormbringer"})) {
            CurrentR = RState::Leaping;
            RCastTick = Now();
        }
    } else if (WMarkTargetId == id && ControllerHelpers::AnyTextContains(
                   {args.BuffName}, {"volibearw", "frenziedmaul", "wounded"})) {
        WMarkTick = Now();
        CurrentW = WState::RecastReady;
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender) && ControllerHelpers::AnyTextContains(
            {args.BuffName}, {"volibearp", "relentlessstorm"})) {
        PassiveStacks = 0;
        CurrentPassive = PassiveState::Inactive;
    }
    if (IsLocalPlayer(args.Sender) && ControllerHelpers::AnyTextContains(
            {args.BuffName}, {"volibeare", "skysplitter"})) {
        CurrentE = EState::Ready;
        ECenter = {};
    }
    if (WMarkTargetId == id && ControllerHelpers::AnyTextContains(
            {args.BuffName}, {"volibearw", "frenziedmaul", "wounded"})) {
        WMarkTargetId = 0;
        CurrentW = WState::Ready;
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
    Vector3 endpoint = args.End;
    (void)CaptureGapcloser(args, WMarkTargetId, endpoint, ManualOwnershipUntil, kRRange, 1000);
}

inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, WMarkTargetId, ManualOwnershipUntil, 900, 250, 5000);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName},
                                           {"volibearr", "stormbringer", "volibeare"})) {
        if (args.Sender.NetworkId != 0) RTargetId = static_cast<int>(args.Sender.NetworkId);
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.NetworkId != 0 && static_cast<int>(args.Sender.NetworkId) == RTargetId)
        RTargetId = 0;
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (ControllerHelpers::AnyTextContains({args.SpellName, args.MissileName},
                                           {"volibeare", "sky splitter"})) {
        ECastTick = Now();
    }
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "Relentless Storm five-stack lightning and stack expiry reconciliation",
    "Thundering Smash movement chase, attack windup ownership and stun contact",
    "Frenzied Maul first bite mark, wounded recast damage and heal window",
    "Sky Splitter prediction, lightning delay, shield radius and damage gating",
    "Stormbringer leap range, landing coverage and turret disable duration",
    "Stormbringer endpoint wall, turret and enemy-count safety with lethal/defensive exceptions",
    "Mana, cooldown, damage and shield-aware cast policy across modes",
    "Selected target precedence followed by orbwalker target fallback",
    "Combo, harass, lane clear, jungle, last-hit, flee and automatic policies",
    "Polling reconciliation across spell, buff, object and missile callbacks",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Volibear;
    controller.ControllerId = "champion.kuroaio.ai.volibear.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIVolibear.md";
    controller.ImplementationSummary =
        "Owns Volibear passive stacks, Q movement/stun, W wounded mark/recast, E lightning/shield "
        "and R turret-disable leap state with endpoint safety and polling reconciliation.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Volibear
