#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "../Profiles/AIDarius.h"
#include "AIDariusGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Darius {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::Bool;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::MaximumBuffCount;
using ControllerHelpers::Now;
using ControllerHelpers::OrbwalkerHeroTarget;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::Protected;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::Slider;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline std::array<int, 32> HemoIds{};
inline std::array<int, 32> HemoStacks{};
inline std::array<int, 32> HemoExpiry{};
inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int ManualOwnershipUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserUntil = 0;
inline Vector3 GapcloserEndpoint{};
inline int InterruptTargetId = 0;
inline int InterruptUntil = 0;
inline bool NoxianMight = false;
inline int NoxianMightUntil = 0;
inline bool RInFlight = false;
inline bool RControllerOwned = false;
inline bool RResetReady = false;
inline int RMissileId = 0;
inline int RTargetId = 0;
inline int RCastTick = 0;
inline int RResetUntil = 0;

inline int HemoIndex(int id) {
    if (id == 0) return -1;
    for (std::size_t i = 0; i < HemoIds.size(); ++i)
        if (HemoIds[i] == id) return static_cast<int>(i);
    for (std::size_t i = 0; i < HemoIds.size(); ++i) {
        if (HemoIds[i] == 0) {
            HemoIds[i] = id;
            return static_cast<int>(i);
        }
    }
    return -1;
}
inline void ClearHemo(int id) {
    const int index = HemoIndex(id);
    if (index >= 0) {
        HemoStacks[static_cast<std::size_t>(index)] = 0;
        HemoExpiry[static_cast<std::size_t>(index)] = 0;
    }
}
inline int StackCount(const AIHeroClient& target) {
    if (!target.IsValid()) return 0;
    const int id = static_cast<int>(target.NetworkId());
    const int index = HemoIndex(id);
    const int live = MaximumBuffCount(target, {
        "DariusHemo", "DariusHemoMarker", "DariusHemoVisual", "DariusHemoMax"});
    if (live > 0) {
        if (index >= 0) {
            HemoStacks[static_cast<std::size_t>(index)] =
                std::clamp(live, 0, kMaximumHemorrhageStacks);
            HemoExpiry[static_cast<std::size_t>(index)] = Now() + kHemorrhageDurationMs;
        }
        return std::clamp(live, 0, kMaximumHemorrhageStacks);
    }
    if (index >= 0 && HemoExpiry[static_cast<std::size_t>(index)] >= Now())
        return std::clamp(HemoStacks[static_cast<std::size_t>(index)], 0, kMaximumHemorrhageStacks);
    ClearHemo(id);
    return 0;
}
inline void ObserveHemo(int id, int increment = 1) {
    const int index = HemoIndex(id);
    if (index < 0) return;
    HemoStacks[static_cast<std::size_t>(index)] = std::clamp(
        HemoStacks[static_cast<std::size_t>(index)] + increment, 0, kMaximumHemorrhageStacks);
    HemoExpiry[static_cast<std::size_t>(index)] = Now() + kHemorrhageDurationMs;
}
inline bool MightBuffPresent() {
    const auto player = GameObjects::Player();
    return player.IsValid() && (player.HasBuff("DariusNoxianMight") ||
        player.HasBuff("DariusHemoMax") || player.HasBuff("DariusHemoMarker"));
}
inline bool MightActive() {
    return NoxianMight || NoxianMightUntil >= Now() || MightBuffPresent();
}
inline float EffectiveBonusAttackDamage() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0.0f;
    const float base = player.BonusAttackDamage();
    return (MightActive() && !MightBuffPresent())
        ? base + NoxianMightBonusAttackDamage(player.Level()) : base;
}
inline float EffectiveTotalAttackDamage() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0.0f;
    const float base = player.TotalAttackDamage();
    return (MightActive() && !MightBuffPresent())
        ? base + NoxianMightBonusAttackDamage(player.Level()) : base;
}
inline bool Ready(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() &&
        (reactive || SpellEnabled(slot, mode)) &&
        ControllerHelpers::HasCurrentResource(ControllerHelpers::SpellCost(slot)) &&
        (reactive || Now() - LastCastTick[static_cast<std::size_t>(slot)] >= 45);
}
inline bool CanAct(bool reactive) {
    const auto player = GameObjects::Player();
    return player.IsValid() && !Engine::IsPlayerCrowdControlled(player) &&
        (reactive || !ControllerHelpers::PreserveAttack(false));
}
inline float QDamage(const AIHeroClient& target, bool outer = true) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = outer ? QOuterRawDamage(SpellRank(0), EffectiveTotalAttackDamage()) :
        QInnerRawDamage(SpellRank(0), EffectiveTotalAttackDamage());
    return player.CalculatePhysicalDamage(target, raw);
}
inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, WRawDamage(SpellRank(1), EffectiveTotalAttackDamage())) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculatePhysicalDamage(target, ERawDamage(SpellRank(2), EffectiveBonusAttackDamage())) : 0.0f;
}
inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? RRawDamage(SpellRank(3), EffectiveBonusAttackDamage(), StackCount(target)) : 0.0f;
}
inline bool SafeCommit(const AIHeroClient& target, bool lethal, bool reactive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool turret = Engine::UnderEnemyTurret(target.Position()) &&
        !Engine::UnderEnemyTurret(player.Position());
    return Geometry::SafeCommit(false, turret,
        Engine::CountEnemiesAt(target.Position(), 500.0f),
        Slider(TacticsMenu, "MaxCommitEnemies", 2), lethal,
        reactive || player.HealthPercent() <= Slider(TacticsMenu, "DefensiveHealth", 30), false);
}
inline AIHeroClient StackAwareTarget(const AIHeroClient& selected, float range) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const auto orb = OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orb, range)) return orb;
    const auto player = GameObjects::Player();
    AIHeroClient best{};
    int bestScore = -1000000;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range)) continue;
        const bool execute = RExecute(RDamage(enemy), enemy.Health(), enemy.AllShield());
        const int score = TargetPriority(StackCount(enemy), execute, false,
            player.IsValid() ? player.Position().Distance2D(enemy.Position()) : 0.0f);
        if (!best.IsValid() || score > bestScore) { best = enemy; bestScore = score; }
    }
    return best;
}
inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange + 40.0f) ||
        !Ready(0, mode, reactive) || !CanAct(reactive) || Protected(target) ||
        !InQOuterEdge(player.Position(), PredictPosition(target, kQDelay), target.BoundingRadius()) ||
        (!reactive && ControllerHelpers::PreserveAttack(false))) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    LastCastTick[0] = Now();
    ObserveHemo(static_cast<int>(target.NetworkId()));
    return true;
}
inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kWRange + 40.0f) ||
        !InAutoRange(player.Position(), PredictPosition(target, 0.05f), target.BoundingRadius()) ||
        !Ready(1, mode, reactive) || !CanAct(reactive) || Protected(target)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    ObserveHemo(static_cast<int>(target.NetworkId()));
    return true;
}
inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange + 50.0f) ||
        !Ready(2, mode, reactive) || !CanAct(reactive) || Protected(target)) return false;
    const Vector3 aim = PredictPosition(target, 0.25f);
    const bool lethal = Lethal(target, EDamage(target));
    if (!InECone(player.Position(), aim, aim, target.BoundingRadius()) ||
        !SafeCommit(target, lethal, reactive) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, 20.0f)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now();
    ObserveHemo(static_cast<int>(target.NetworkId()));
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kRRange + 40.0f) ||
        !RReachable(player.Position(), PredictPosition(target, 0.05f), target.BoundingRadius()) ||
        !Ready(3, mode, reactive) || !CanAct(reactive) || Protected(target)) return false;
    const float damage = RDamage(target);
    const bool execute = RExecute(damage, target.Health(), target.AllShield());
    if (!execute || !SafeCommit(target, true, reactive)) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    LastCastTick[3] = RCastTick = Now();
    RTargetId = static_cast<int>(target.NetworkId());
    RInFlight = true;
    RControllerOwned = true;
    RResetReady = false;
    RResetUntil = Now() + 1800;
    return true;
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastR(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastQ(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastE(target, Mode::Harass)) return;
    if (CastW(target, Mode::Harass)) return;
    (void)CastQ(target, Mode::Harass);
}
inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "Mana", 35)) return;
    if (mode == Mode::Jungle) {
        const auto monster = SelectJungleTarget(kAaRange + 80.0f, 0.10f, 100000.0f);
        if (monster.IsValid() && InRange(player.Position(), monster.Position(), kWRange, monster.BoundingRadius()) &&
            Ready(1, mode) && CanAct(false)) {
            if (Engine::ControllerCastSelf(1)) LastCastTick[1] = Now();
        }
    }
    (void)Engine::TryFarm(mode);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target, kERange) && CastE(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target, kQRange) && CastQ(target, Mode::Flee, true)) return;
    (void)CastR(target, Mode::Flee, true);
}
inline void Automatic(const AIHeroClient& target) {
    if (GapcloserTargetId != 0 && GapcloserUntil >= Now()) {
        const auto threat = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(threat, kERange) && CastE(threat, Mode::Automatic, true)) return;
    }
    if (InterruptTargetId != 0 && InterruptUntil >= Now()) {
        const auto threat = HeroByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(threat, kERange) && CastE(threat, Mode::Automatic, true)) return;
    }
    if (Engine::ValidEnemy(target, kRRange)) (void)CastR(target, Mode::Automatic, true);
}
inline void ReconcileState() {
    const int now = Now();
    bool maxStackObserved = false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        maxStackObserved = maxStackObserved || StackCount(enemy) >= kMaximumHemorrhageStacks;
    }
    const auto player = GameObjects::Player();
    if (player.IsValid() && (MightBuffPresent() || maxStackObserved)) {
        NoxianMight = true;
        NoxianMightUntil = now + kNoxianMightDurationMs;
    } else if (NoxianMightUntil < now) NoxianMight = false;
    if (RInFlight && RTargetId != 0) {
        const auto target = HeroByNetworkId(RTargetId);
        if (!target.IsValid() && RControllerOwned && RResetUntil >= now) {
            RResetReady = true;
            RInFlight = false;
        } else if (RResetUntil < now) {
            RInFlight = false;
            RResetReady = false;
        }
    }
    if (GapcloserUntil < now) GapcloserTargetId = 0;
    if (InterruptUntil < now) InterruptTargetId = 0;
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const AIHeroClient target = StackAwareTarget(selected, mode == Mode::Flee ? 900.0f : kERange + 40.0f);
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: Farm(mode); break;
    case Mode::Flee: Flee(target); break;
    case Mode::Automatic: Automatic(target); break;
    default: break;
    }
    return true;
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("Darius tactics"));
    TacticsMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at Apprehend commit", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("DefensiveHealth", "Defensive health threshold", 30, 1, 100));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana reserve", 45, 0, 100));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("Darius farming"));
    FarmMenu->Add(new MenuSlider("Mana", "Farm mana reserve", 35, 0, 100));
}
inline void OnLoad() {
    HemoIds.fill(0); HemoStacks.fill(0); HemoExpiry.fill(0); LastCastTick.fill(0);
    LastAutoTargetId = LastAutoTick = ManualOwnershipUntil = 0;
    GapcloserTargetId = GapcloserUntil = InterruptTargetId = InterruptUntil = 0;
    GapcloserEndpoint = {}; NoxianMight = false; NoxianMightUntil = 0;
    RInFlight = RControllerOwned = RResetReady = false;
    RTargetId = RCastTick = RResetUntil = RMissileId = 0;
}
inline void OnUnload() { TacticsMenu = FarmMenu = nullptr; OnLoad(); }
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot < 0 || slot > 3) return;
        if (!Engine::WasControllerCast(slot)) ManualOwnershipUntil = now + 650;
        LastCastTick[static_cast<std::size_t>(slot)] = now;
        if (slot == 3) { RInFlight = true; RControllerOwned = Engine::WasControllerCast(3); RTargetId = static_cast<int>(args.TargetNetworkId); RResetUntil = now + 1800; }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (analysis.Valid && (analysis.TargetsPlayer || analysis.CrossesPlayer))
        InterruptTargetId = static_cast<int>(args.Sender.NetworkId), InterruptUntil =
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
        LastAutoTick = Now();
        if (LastAutoTargetId != 0) ObserveHemo(LastAutoTargetId);
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (Engine::TextContains(args.BuffName, "DariusHemo")) ObserveHemo(id);
    if (IsLocalPlayer(args.Sender) && (Engine::TextContains(args.BuffName, "NoxianMight") ||
        Engine::TextContains(args.BuffName, "DariusHemoMax"))) {
        NoxianMight = true; NoxianMightUntil = Now() + kNoxianMightDurationMs;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (Engine::TextContains(args.BuffName, "DariusHemo")) ClearHemo(static_cast<int>(args.Sender.NetworkId));
    if (IsLocalPlayer(args.Sender) &&
        (Engine::TextContains(args.BuffName, "NoxianMight") ||
         Engine::TextContains(args.BuffName, "DariusHemoMax"))) NoxianMight = false;
}
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (args.EndTime <= Game::Time()) {
        OnBuffRemove(args);
        return;
    }
    if (!args.Sender.IsValid()) return;
    if (Engine::TextContains(args.BuffName, "DariusHemo")) {
        const int id = static_cast<int>(args.Sender.NetworkId);
        const int index = HemoIndex(id);
        if (index >= 0) {
            HemoExpiry[static_cast<std::size_t>(index)] = Now() + kHemorrhageDurationMs;
            const auto target = HeroByNetworkId(id);
            const int live = MaximumBuffCount(target, {
                "DariusHemo", "DariusHemoMarker", "DariusHemoVisual", "DariusHemoMax"});
            if (live > 0)
                HemoStacks[static_cast<std::size_t>(index)] =
                    std::clamp(live, 0, kMaximumHemorrhageStacks);
        }
        return;
    }
    OnBuffAdd(args);
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
    LastAutoTick = Now();
    const AIHeroClient target(args.Target.Handle());
    if (Engine::ValidEnemy(target) && (Orbwalker::IsWindingUp() || !Ready(1, Mode::Combo)))
        (void)CastW(target, Mode::Combo, true);
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, GapcloserTargetId, GapcloserEndpoint, GapcloserUntil, kERange, 1100);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, InterruptTargetId, InterruptUntil,
                         static_cast<int>(kERange), 250, 5000);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (Engine::TextContains(args.SpellName, "DariusExecute") ||
        Engine::TextContains(args.MissileName, "DariusExecute")) {
        RMissileId = args.MissileNetworkId != 0
            ? static_cast<int>(args.MissileNetworkId)
            : static_cast<int>(args.Sender.NetworkId);
    }
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    if (id == RMissileId) RMissileId = 0;
}
inline void OnDraw() {}

inline constexpr const char* Scenarios[] = {
    "Hemorrhage stack tracking and five-stack Noxian Might ownership",
    "Decimate prediction, outer-edge damage and missing-health heal",
    "Crippling Strike attack reset, bonus damage and one-second slow",
    "Apprehend cone pull, wall-safe reach and armor penetration state",
    "Noxian Guillotine true-damage execute with stack multiplier",
    "R kill reset and Noxian Might state reconciled by events and polling",
    "selected target then orbwalker precedence with stack-aware fallback",
    "attack-windup preservation and manual spell ownership protection",
    "turret, wall, enemy-count and low-health commit boundaries",
    "combo, harass, lane clear, jungle, last-hit, flee and automatic routes",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Darius";
    controller.ControllerId = "champion.kuroaio.ai.darius.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIDarius.md";
    controller.ImplementationSummary =
        "Owns Hemorrhage/Noxian Might state, outer-edge Decimate healing, reset/slow weave, "
        "Apprehend pull and penetration, and stack-scaled Guillotine execute/reset decisions.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Darius
