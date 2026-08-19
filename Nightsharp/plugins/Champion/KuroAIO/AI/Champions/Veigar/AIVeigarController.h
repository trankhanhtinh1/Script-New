#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "../../Profiles/AIVeigar.h"
#include "AIVeigarGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Veigar {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::AP;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Ready;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::SpellCost;

inline Menu* TacticsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline int PassiveStacks = 0;
inline int LastPassiveObservationTick = 0;
inline int LastCastTick[4] = {};
inline int LastAutoTargetId = 0;
inline int ManualOverrideUntil = 0;
inline int IncomingThreatTargetId = 0;
inline int IncomingThreatUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserUntil = 0;
inline int InterruptTargetId = 0;
inline int InterruptUntil = 0;
inline int CageTargetId = 0;
inline int CageExpireTick = 0;
inline int CageStunnedTargetId = 0;
inline int MeteorTargetId = 0;
inline Vector3 IncomingThreatEndpoint{};
inline int MeteorImpactTick = 0;
inline int LastMeteorImpactTick = -100000;
inline Vector3 CageCenter{};
inline Vector3 MeteorCenter{};

inline bool TargetBlocked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || IsCommonUntargetableOrImmune(target) ||
           target.IsInvulnerable() || target.HasBuff("SivirE") ||
           target.HasBuff("NocturneShroudofDarkness") ||
           target.HasBuff("MorganaE") || target.HasBuff("BlackShield") ||
           target.HasBuff("BansheesVeil") || target.HasBuff("EdgeOfNight") ||
           target.HasBuff("VladimirSanguinePool") || target.HasBuff("KayleR");
}

inline bool CastReady(int slot, Mode mode, bool reactive = false) {
    return slot >= 0 && slot < 4 && Ready(slot, mode) &&
           (reactive || LastCastTick[slot] + 40 <= Now());
}

inline bool PreserveAttack(int slot, bool reactive) {
    return !reactive && slot != 3 && Orbwalker::IsWindingUp() &&
           ControllerHelpers::Bool(Engine::HumanMenu, "PreserveAttacks", true);
}

inline float QDamage(const AIBaseClient& target) {
    if (!target.IsValid()) return 0.0f;
    const float runtime = Engine::RuntimeSpells[0]
        ? std::max(0.0f, Engine::RuntimeSpells[0]->GetDamage(target)) : 0.0f;
    return runtime > 1.0f ? runtime : QRawDamage(SpellRank(0), AP());
}
inline float WDamage(const AIBaseClient& target) {
    if (!target.IsValid()) return 0.0f;
    const float runtime = Engine::RuntimeSpells[1]
        ? std::max(0.0f, Engine::RuntimeSpells[1]->GetDamage(target)) : 0.0f;
    return runtime > 1.0f ? runtime : WRawDamage(SpellRank(1), AP());
}
inline float RDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const float runtime = Engine::RuntimeSpells[3]
        ? std::max(0.0f, Engine::RuntimeSpells[3]->GetDamage(target)) : 0.0f;
    return runtime > 1.0f ? runtime : RRawDamage(SpellRank(3), AP(), target.HealthPercent());
}
inline bool LethalWith(const AIHeroClient& target, float damage) {
    return Engine::ValidEnemy(target) && damage >= target.Health() + target.AllShield();
}
inline bool CageStunned(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
        static_cast<int>(target.NetworkId()) == CageStunnedTargetId;
}

inline Vector3 CageCenterFor(const AIHeroClient& target, const Vector3& player) {
    if (!Engine::ValidEnemy(target) || player.IsZero()) return {};
    const Vector3 direction = Direction2D(player, target.Position());
    if (direction.IsZero()) return {};
    const float edgeOffset = std::max(150.0f, kECageRadius -
        std::clamp(target.BoundingRadius(), 25.0f, 80.0f));
    Vector3 center = target.Position() - direction * edgeOffset;
    if (player.Distance2D(center) > kERange) {
        center = player + direction * std::min(kERange - 5.0f, player.Distance2D(target.Position()));
    }
    return center;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) ||
        !Engine::ValidEnemy(target, kQRange + 50.0f) ||
        !CastReady(0, mode, reactive) || PreserveAttack(0, reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 aim = prediction.GetCastPosition().IsValid() &&
            !prediction.GetCastPosition().IsZero()
        ? prediction.GetCastPosition() : PredictPosition(target, kQDelay);
    if (aim.IsZero() || player.Position().Distance2D(aim) > kQRange + target.BoundingRadius() ||
        !QLineHits(player.Position(), aim, aim, target.BoundingRadius()) ||
        !prediction.CollisionObjects.empty() ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) ||
        !Engine::ValidEnemy(target, kERange + kECageRadius) ||
        !CastReady(2, mode, reactive) || PreserveAttack(2, reactive)) return false;
    const Vector3 center = CageCenterFor(target, player.Position());
    if (!CageSafePlacement(center, player.Position(),
                           Engine::CountEnemiesAt(center, 650.0f),
                           ControllerHelpers::Slider(TacticsMenu, "MaxCageEnemies", 2),
                           Engine::UnderEnemyTurret(center), reactive)) return false;
    if (!Engine::ControllerCastPosition(2, center)) return false;
    LastCastTick[2] = Now();
    CageCenter = center;
    CageTargetId = static_cast<int>(target.NetworkId());
    CageExpireTick = Now() + 3000;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) ||
        !Engine::ValidEnemy(target, kWRange + 50.0f) ||
        !CastReady(1, mode, reactive) || PreserveAttack(1, reactive)) return false;
    if (!WChargeReady(Now(), LastMeteorImpactTick,
                      8.0f * WCooldownMultiplier(PassiveStacks), kWImpactDelay)) return false;
    const Vector3 predicted = PredictPosition(target, kWImpactDelay);
    if (predicted.IsZero() || player.Position().Distance2D(predicted) > kWRange + 50.0f ||
        !WImpactHits(predicted, predicted, target.BoundingRadius())) return false;
    const bool controlled = CageStunned(target) ||
        (CageTargetId == static_cast<int>(target.NetworkId()) && CageExpireTick > Now());
    if (!controlled && mode != Mode::Flee && !reactive) return false;
    if (!Engine::ControllerCastPosition(1, predicted)) return false;
    LastCastTick[1] = Now();
    LastMeteorImpactTick = Now();
    MeteorImpactTick = Now() + static_cast<int>(kWImpactDelay * 1000.0f);
    MeteorTargetId = static_cast<int>(target.NetworkId());
    MeteorCenter = predicted;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetBlocked(target) ||
        !Engine::ValidEnemy(target, kRRange) || !CastReady(3, mode, reactive)) return false;
    const bool lethal = LethalWith(target, RDamage(target));
    const bool executeWindow = target.HealthPercent() <=
        ControllerHelpers::Slider(UltimateMenu, "RTargetHP", 58);
    const int nearby = Engine::CountEnemiesAt(target.Position(), 650.0f);
    const ExecuteContext context{
        true, true, player.Position().Distance2D(target.Position()) <= kRRange,
        false,
        lethal, CageStunned(target), Orbwalker::IsWindingUp(),
        Engine::UnderEnemyTurret(target.Position()), nearby,
        ControllerHelpers::Slider(TacticsMenu, "MaxCommitEnemies", 2)};
    if ((!lethal && !executeWindow && !CageStunned(target)) ||
        !ShouldCastExecute(context)) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    LastCastTick[3] = Now();
    return true;
}

struct FarmLinePlan {
    Vector3 Aim{};
    int Hits = 0;
    int LastHits = 0;
    float Score = -FLT_MAX;
    bool Valid = false;
};

inline FarmLinePlan BestQFarmLine(bool jungle, bool lastHitOnly) {
    FarmLinePlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !CastReady(0, lastHitOnly ? Mode::LastHit :
        (jungle ? Mode::Jungle : Mode::LaneClear))) return best;
    const auto& units = jungle ? GameObjects::Jungle() : GameObjects::EnemyMinions();
    for (const auto& candidate : units) {
        if (!candidate.IsValid() || candidate.IsDead() || !candidate.IsTargetable() ||
            player.Position().Distance2D(candidate.Position()) > kQRange) continue;
        const Vector3 direction = Direction2D(player.Position(), candidate.Position());
        if (direction.IsZero()) continue;
        const Vector3 aim = player.Position() + direction * kQRange;
        int hits = 0;
        int lastHits = 0;
        for (const auto& unit : units) {
            if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
                !QLineHits(player.Position(), aim, unit.Position(), unit.BoundingRadius())) continue;
            ++hits;
            const float damage = player.CalculateMagicDamage(unit, QDamage(unit));
            if (damage >= unit.Health()) ++lastHits;
        }
        if (lastHitOnly && lastHits == 0) continue;
        const float score = static_cast<float>(lastHits) * 160.0f +
            static_cast<float>(hits) * (lastHitOnly ? 15.0f : 85.0f);
        if (!best.Valid || score > best.Score) best = {aim, hits, lastHits, score, true};
    }
    return best;
}

inline bool TryFarm(Mode mode) {
    const bool jungle = mode == Mode::Jungle;
    const bool lastHit = mode == Mode::LastHit;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || (!lastHit && player.ManaPercent() <
        ControllerHelpers::Slider(FarmMenu, jungle ? "JungleMana" : "LaneMana", 42))) return false;
    const FarmLinePlan plan = BestQFarmLine(jungle, lastHit);
    const int minimum = lastHit ? 1 : ControllerHelpers::Slider(FarmMenu, "QMinions", 3);
    if (plan.Valid && plan.Hits >= minimum && (!lastHit || plan.LastHits > 0) &&
        !PreserveAttack(0, false) &&
        !ControllerHelpers::ProjectileWallBlocksFromPlayer(plan.Aim, kQWidth * 0.5f) &&
        Engine::ControllerCastPosition(0, plan.Aim)) {
        LastCastTick[0] = Now();
        return true;
    }
    if (lastHit) return false;
    if (!Ready(1, mode) || player.ManaPercent() <
        ControllerHelpers::Slider(FarmMenu, "WMana", 58)) return false;
    const auto& units = jungle ? GameObjects::Jungle() : GameObjects::EnemyMinions();
    for (const auto& unit : units) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(unit.Position()) > kWRange ||
            player.CalculateMagicDamage(unit, WDamage(unit)) < unit.Health()) continue;
        const Vector3 impact = PredictPosition(unit, kWImpactDelay);
        if (impact.IsZero() || Engine::UnderEnemyTurret(impact) ||
            !Engine::ControllerCastPosition(1, impact)) continue;
        LastCastTick[1] = Now();
        LastMeteorImpactTick = Now();
        MeteorImpactTick = Now() + 1200;
        return true;
    }
    return false;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastR(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastQ(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || GameObjects::Player().ManaPercent() <
        ControllerHelpers::Slider(TacticsMenu, "HarassMana", 45)) return;
    if (CastE(target, Mode::Harass)) return;
    if (CageStunned(target)) (void)CastW(target, Mode::Harass);
    else (void)CastQ(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target, 850.0f)) {
        if (CastE(target, Mode::Flee, true)) return;
        (void)CastQ(target, Mode::Flee, true);
    }
}
inline void Automatic(const AIHeroClient& target) {
    if (IncomingThreatUntil >= Now() || GapcloserUntil >= Now()) {
        const AIHeroClient threat = HeroByNetworkId(
            IncomingThreatTargetId != 0 ? IncomingThreatTargetId : GapcloserTargetId);
        if (Engine::ValidEnemy(threat, 900.0f) && CastE(threat, Mode::Automatic, true)) return;
    }
    if (Engine::ValidEnemy(target) && LethalWith(target, RDamage(target)))
        (void)CastR(target, Mode::Automatic, true);
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    const int now = Now();
    if (CageExpireTick < now) { CageCenter = {}; CageTargetId = 0; CageStunnedTargetId = 0; }
    if (MeteorImpactTick > 0 && MeteorImpactTick <= now) {
        MeteorImpactTick = 0; MeteorTargetId = 0; MeteorCenter = {};
    }
    if (IncomingThreatUntil < now) { IncomingThreatTargetId = 0; IncomingThreatEndpoint = {}; }
    if (GapcloserUntil < now) { GapcloserTargetId = 0; IncomingThreatEndpoint = {}; }
    if (InterruptUntil < now) InterruptTargetId = 0;
    if (!player.IsValid()) return;
    if (player.HasBuff("VeigarPassive")) LastPassiveObservationTick = now;
    if (PassiveStacks > 10000) PassiveStacks = 10000;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOverrideUntil > Now()) return true;
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, kRRange);
    if (mode == Mode::Flee) Flee(target);
    else if (mode == Mode::Automatic) Automatic(target);
    else if (mode == Mode::Combo) Combo(target);
    else if (mode == Mode::Harass) Harass(target);
    else if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit)
        (void)TryFarm(mode);
    return true;
}

inline void OnLoad() {
    PassiveStacks = 0; LastPassiveObservationTick = 0; ManualOverrideUntil = 0;
    IncomingThreatTargetId = IncomingThreatUntil = GapcloserTargetId = GapcloserUntil = 0;
    InterruptTargetId = InterruptUntil = CageTargetId = CageExpireTick = 0;
    CageStunnedTargetId = MeteorTargetId = MeteorImpactTick = 0;
    LastMeteorImpactTick = -100000; CageCenter = {}; MeteorCenter = {}; IncomingThreatEndpoint = {};
    for (int& tick : LastCastTick) tick = 0;
}
inline void OnUnload() { TacticsMenu = nullptr; FarmMenu = nullptr; UltimateMenu = nullptr; OnLoad(); }

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        const bool owned = args.Slot >= 0 && args.Slot < 4 &&
            Engine::WasControllerCast(static_cast<int>(args.Slot));
        if (!owned) ManualOverrideUntil = Now() + ControllerHelpers::Slider(TacticsMenu, "ManualOwnershipMs", 550);
        if (args.Slot >= 0 && args.Slot < 4) {
            LastCastTick[static_cast<std::size_t>(args.Slot)] = Now();
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args, 220.0f, 110.0f, 250, 260, 260, 1500, 450);
    if (analysis.Valid && analysis.Enemy.IsValid()) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatUntil = std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
    }
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack && args.Target.IsValid())
        LastAutoTargetId = static_cast<int>(args.Target.NetworkId);
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "veigarpassive")) {
            PassiveStacks = std::max(PassiveStacks, std::clamp(args.Count, 1, 10000));
            LastPassiveObservationTick = Now();
        }
        return;
    }
    if (Engine::TextContains(args.BuffName, "veigareventhorizonstun"))
        CageStunnedTargetId = static_cast<int>(args.Sender.NetworkId);
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && Engine::TextContains(args.BuffName, "veigarpassive"))
        LastPassiveObservationTick = Now();
    if (static_cast<int>(args.Sender.NetworkId) == CageStunnedTargetId &&
        Engine::TextContains(args.BuffName, "veigareventhorizonstun")) CageStunnedTargetId = 0;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(args, GapcloserTargetId, IncomingThreatEndpoint, GapcloserUntil, 850.0f, 1200);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(args, InterruptTargetId, InterruptUntil, 900, 250, 5000);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && ControllerHelpers::ObjectEventIsAllied(args) &&
        Engine::TextContains(args.Sender.Name, "veigareventhorizon")) {
        CageCenter = args.Sender.Position; CageExpireTick = Now() + 3000;
    }
}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && !CageCenter.IsZero() &&
        args.Sender.Position.Distance2D(CageCenter) < 500.0f) {
        CageCenter = {}; CageExpireTick = 0; CageTargetId = 0;
    }
}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && Engine::TextContains(args.Sender.Name, "veigardarkmatter"))
        MeteorImpactTick = Now() + 1200;
}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (args.Sender.IsValid() && Engine::TextContains(args.Sender.Name, "veigardarkmatter")) {
        MeteorImpactTick = 0; LastMeteorImpactTick = Now(); MeteorCenter = {};
    }
}
inline void OnDraw() {}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("VeigarTactics", "AP stacks and cage policy"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 550, 150, 1200));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 45, 0, 100));
    TacticsMenu->Add(new MenuSlider("MaxCageEnemies", "Maximum enemies at cage center", 2, 1, 5));
    TacticsMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies for execute", 2, 1, 5));
    FarmMenu = root->AddSubMenu(new Menu("VeigarFarm", "Q line and meteor farming"));
    FarmMenu->Add(new MenuSlider("QMinions", "Minimum Q line hits", 3, 1, 8));
    FarmMenu->Add(new MenuSlider("LaneMana", "Lane clear mana percent", 42, 0, 100));
    FarmMenu->Add(new MenuSlider("JungleMana", "Jungle mana percent", 32, 0, 100));
    FarmMenu->Add(new MenuSlider("WMana", "Meteor farm mana percent", 58, 0, 100));
    UltimateMenu = root->AddSubMenu(new Menu("VeigarUltimate", "Primordial Burst execute"));
    UltimateMenu->Add(new MenuSlider("RTargetHP", "Allow R below target health percent", 58, 1, 100));
}

inline constexpr const char* Scenarios[] = {
    "Passive AP stack polling, Q minion kill and champion takedown reconciliation",
    "Q line prediction, first collision rejection and projectile-wall check",
    "W meteor impact delay, passive cooldown reduction and cage-stun setup",
    "E Event Horizon edge placement, terrain, turret and enemy-count safety",
    "R missing-health execute damage and shield-aware lethal gate",
    "Combo cage-safe priority and manual auto-attack windup protection",
    "Harass Q poke, lane clear Q line, jungle meteor and Q LastHit",
    "Flee peel cage and Automatic gapcloser or interrupt response",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Veigar;
    controller.ControllerId = "champion.kuroaio.ai.veigar.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIVeigar.md";
    controller.ImplementationSummary =
        "Passive AP stack telemetry, Q line last-hit planner, delayed meteor state, cage-edge setup and missing-health execute policy.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Veigar
