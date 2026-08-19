#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIEvelynnGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Evelynn {

using namespace Geometry;
using ControllerHelpers::CaptureBeforeAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Ready;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<int, 4> LastCastTick{};
inline int ManualOverrideUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int WTargetId = 0;
inline int WCastTick = 0;
inline int QTargetId = 0;
inline int ETargetId = 0;
inline int RTargetId = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline bool PassiveUnlocked = false;
inline bool DemonShade = false;
inline bool WMarked = false;
inline bool WArmed = false;
inline bool QPrimed = false;
inline bool EmpoweredE = false;
inline bool RExecuteVisible = false;
inline Mode LastMode = Mode::None;

inline bool Buff(const AIBaseClient& unit, const char* token) {
    return unit.IsValid() && token && unit.HasBuff(token);
}

inline bool IsDemonShade(const AIHeroClient& player) {
    return player.IsValid() &&
        (Buff(player, "EvelynnPassiveDemonShade") ||
         Buff(player, "EvelynnPassiveDemonCloak") ||
         Buff(player, "EvelynnPassive") || Buff(player, "EvelynnDemonShade"));
}

inline bool IsAllureMark(const AIBaseClient& unit) {
    return unit.IsValid() &&
        (Buff(unit, "EvelynnW") || Buff(unit, "EvelynnWWarning") ||
         Buff(unit, "EvelynnWMark") || Buff(unit, "EvelynnWCharm"));
}

inline bool IsEmpoweredWhiplash(const AIHeroClient& player) {
    return player.IsValid() &&
        (Buff(player, "EvelynnE2") || Buff(player, "EvelynnE2Ready") ||
         Buff(player, "EvelynnEReady") || Buff(player, "EvelynnEHeartbreaker"));
}

inline bool TargetById(const AIHeroClient& target, int id) {
    return Engine::ValidEnemy(target) && id != 0 &&
        static_cast<int>(target.NetworkId()) == id;
}

inline bool TargetProtected(const AIHeroClient& target) {
    return Protected(target) || HasSpellShieldOrImmunity(target) ||
           ControllerHelpers::IsCommonUntargetableOrImmune(target);
}

inline bool CanActNow(bool reactive = false) {
    return reactive || !PlayerMobilityLocked();
}

inline bool CastReady(int slot, Mode mode, int delay = 55,
                      bool reactive = false) {
    return Ready(slot, mode) && CastThrottleReady(LastCastTick, slot, delay) &&
        (reactive || !PreserveAttack(false));
}

inline bool MarkStillRelevant(const AIHeroClient& target) {
    if (!TargetById(target, WTargetId)) return false;
    return IsAllureMark(target) || (WMarked && WCastTick > 0 &&
        Now() - WCastTick <= Slider(WMenu, "MarkLifetimeMs", 5000));
}

inline bool CharmArmed(const AIHeroClient& target) {
    if (!MarkStillRelevant(target)) return false;
    const int armMs = static_cast<int>(1000.0f * std::clamp(
        Slider(WMenu, "CharmSeconds", 25) / 10.0f, 1.0f, 2.5f));
    return WArmed || Now() - WCastTick >= armMs;
}

inline bool SafeEndpoint(const Vec3& endpoint, bool escaping,
                         bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || endpoint.IsZero() || !endpoint.IsValid() ||
        SDK::NavMesh::IsWall(endpoint)) return false;
    const bool endpointTurret = Engine::UnderEnemyTurret(endpoint);
    const bool originTurret = Engine::UnderEnemyTurret(player.Position());
    if (endpointTurret && !originTurret && !escaping && !lethal) return false;
    const int enemies = Engine::CountEnemiesAt(endpoint, 300.0f);
    const int maximum = Slider(RMenu, escaping ? "MaxEscapeEnemies" : "MaxEndpointEnemies",
                               escaping ? 1 : 2);
    return enemies <= std::max(0, maximum);
}

inline bool RIsLethal(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return false;
    const bool threshold = RExecuteReady(target.HealthPercent(),
                                         static_cast<float>(Slider(RMenu, "ExecuteHP", 30)));
    const bool damage = Engine::RuntimeSpells[3] &&
        Engine::RuntimeSpells[3]->GetDamage(target) >=
            target.Health() + target.AllShield();
    return threshold || damage;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kWRange) ||
        TargetProtected(target) || !CastReady(1, mode, 90, reactive) ||
        !CanActNow(reactive)) return false;
    if (MarkStillRelevant(target)) return false;
    if (!WTargetReachable(player.Position(), target.Position(), kWRange,
                          target.BoundingRadius())) return false;
    if (!Engine::ControllerCastUnit(1, target)) return false;
    WTargetId = static_cast<int>(target.NetworkId());
    WCastTick = Now();
    WMarked = true;
    WArmed = false;
    LastCastTick[1] = Now();
    return true;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQInitialRange + 50.0f) ||
        TargetProtected(target) || !CastReady(0, mode, 45, reactive) ||
        !CanActNow(reactive)) return false;
    const Vector3 predicted = PredictPosition(target, 0.25f);
    if (!predicted.IsValid() || predicted.IsZero()) return false;
    if (QPrimed) {
        if (!QRecastHits(player.Position(), predicted, target.BoundingRadius())) return false;
        if (Engine::ControllerCastPosition(0, predicted)) {
            LastCastTick[0] = Now();
            QTargetId = static_cast<int>(target.NetworkId());
            QPrimed = false;
            return true;
        }
        return false;
    }
    if (player.Position().Distance2D(predicted) > kQInitialRange + target.BoundingRadius() ||
        !QInitialHits(player.Position(), predicted, target.Position(), target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(predicted, kQWidth * 0.5f)) {
        return false;
    }
    if (Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        if (!ControllerHelpers::PredictionAtLeast(prediction, SDK::HitChance::High) ||
            prediction.GetCastPosition().IsZero()) return false;
    }
    if (!Engine::ControllerCastPosition(0, predicted)) return false;
    LastCastTick[0] = Now();
    QTargetId = static_cast<int>(target.NetworkId());
    QPrimed = true;
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange + 60.0f) ||
        TargetProtected(target) || !CastReady(2, mode, 55, reactive) ||
        !CanActNow(reactive) || !EEntryReachable(player.Position(), target.Position(),
                                                  target.BoundingRadius())) return false;
    if (!reactive && !CharmArmed(target)) return false;
    const Vector3 entry = EEntryPoint(player.Position(), target.Position());
    if (entry.IsZero() || SDK::NavMesh::IsWall(entry) ||
        Engine::UnderEnemyTurret(entry) && !Engine::UnderEnemyTurret(player.Position()) ||
        Engine::CountEnemiesAt(entry, 280.0f) > Slider(EMenu, "MaxEntryEnemies", 2)) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    ETargetId = static_cast<int>(target.NetworkId());
    EmpoweredE = false;
    LastCastTick[2] = Now();
    return true;
}

inline bool CastRExecute(const AIHeroClient& target, Mode mode,
                         bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kRRange + 60.0f) ||
        TargetProtected(target) || !CastReady(3, mode, 120, reactive) ||
        !CanActNow(reactive) || !RIsLethal(target)) return false;
    const Vector3 predicted = PredictPosition(target, 0.35f);
    if (!InRange(player.Position(), predicted, kRRange, target.BoundingRadius())) return false;
    if (!SafeEndpoint(predicted, false, true)) return false;
    if (!Engine::ControllerCastPosition(3, predicted)) return false;
    RTargetId = static_cast<int>(target.NetworkId());
    RExecuteVisible = true;
    LastCastTick[3] = Now();
    return true;
}

inline bool CastREscape(const AIHeroClient& threat, Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !CastThrottleReady(LastCastTick, 3, 120) ||
        !CanActNow(true) || !Engine::ValidEnemy(threat, 900.0f)) return false;
    const Vector3 endpoint = EscapePoint(player.Position(), threat.Position());
    if (!SafeEscapeVector(player.Position(), threat.Position(), endpoint) ||
        player.Position().Distance2D(endpoint) > kRRange ||
        !SafeEndpoint(endpoint, true)) return false;
    if (!Engine::ControllerCastPosition(3, endpoint)) return false;
    RTargetId = 0;
    LastCastTick[3] = Now();
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (!MarkStillRelevant(target) && CastW(target, Mode::Combo)) return;
    if (MarkStillRelevant(target) && !CharmArmed(target)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    (void)CastRExecute(target, Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(WMenu, "HarassMana", 45) ||
        !Engine::ValidEnemy(target)) return;
    if (!MarkStillRelevant(target) && CastW(target, Mode::Harass)) return;
    if (MarkStillRelevant(target) && !CharmArmed(target)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastE(target, Mode::Harass);
}

inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) &&
        (GameObjects::Player().HealthPercent() <= Slider(RMenu, "EscapeHP", 35) ||
         IncomingHardCCUntil > Now())) {
        if (CastREscape(target, Mode::Flee)) return;
    }
    if (Engine::ValidEnemy(target) && CharmArmed(target))
        (void)CastE(target, Mode::Flee, true);
}

inline bool Automatic(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (IncomingHardCCUntil > Now() && CastREscape(target, Mode::Automatic)) return true;
    return CastRExecute(target, Mode::Automatic, true);
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    PassiveUnlocked = player.Level() >= 6;
    DemonShade = PassiveUnlocked && IsDemonShade(player);
    EmpoweredE = IsEmpoweredWhiplash(player);
    if (WTargetId != 0) {
        const auto marked = GameObjects::GetUnitByNetworkId<AIHeroClient>(WTargetId);
        WMarked = marked.IsValid() && IsAllureMark(marked) && WCastTick + 5500 > now;
    }
    WArmed = WMarked && WCastTick > 0 &&
        WCastTick + static_cast<int>(1000.0f * std::clamp(
            Slider(WMenu, "CharmSeconds", 25) / 10.0f, 1.0f, 2.5f)) <= now;
    if (!WMarked) { WTargetId = 0; WCastTick = 0; WArmed = false; }
    if (QPrimed && QTargetId == 0) QPrimed = false;
    RExecuteVisible = RExecuteVisible && LastCastTick[3] + 900 > now;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    if (ManualOverrideUntil > Now()) return true;
    const float range = mode == Mode::Flee ? 950.0f : kWRange;
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, range);
    if (mode == Mode::Automatic) { (void)Automatic(target); return true; }
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (GameObjects::Player().ManaPercent() >= Slider(FarmMenu, "Mana", 35))
            (void)Engine::TryFarm(mode);
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
        if (slot >= 0 && slot < 4) {
            if (!Engine::WasControllerCast(slot)) ManualOverrideUntil = now +
                Slider(TacticsMenu, "ManualOwnershipMs", 650);
            LastCastTick[static_cast<std::size_t>(slot)] = now;
            if (slot == 0) QPrimed = Engine::TextContains(args.SpellName, "EvelynnQ") &&
                !Engine::TextContains(args.SpellName, "Q2");
            if (slot == 1) { WMarked = true; WCastTick = now; }
            if (slot == 2) EmpoweredE = false;
            if (slot == 3) RExecuteVisible = false;
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(
        IncomingHardCCUntil, std::max(analysis.CommitmentUntilTick,
                                      analysis.LineThreatUntilTick));
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args, bool added) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "EvelynnPassive")) DemonShade = added && PassiveUnlocked;
        if (Engine::TextContains(args.BuffName, "EvelynnE2") || Engine::TextContains(args.BuffName, "EvelynnEReady"))
            EmpoweredE = added;
        return;
    }
    if (Engine::TextContains(args.BuffName, "EvelynnW")) {
        WTargetId = static_cast<int>(args.Sender.NetworkId);
        WMarked = added;
        WArmed = added && WCastTick > 0 && now - WCastTick >=
            static_cast<int>(1000.0f * std::clamp(Slider(WMenu, "CharmSeconds", 25) / 10.0f, 1.0f, 2.5f));
        if (!added) { WTargetId = 0; WCastTick = 0; WArmed = false; }
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) { UpdateBuffState(args, true); }
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) { UpdateBuffState(args, false); }

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureBeforeAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)ControllerHelpers::CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    (void)ControllerHelpers::CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs&) {}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs&) {}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQInitialRange, 0x336F4FFFu, 1.2f, 64);
    Drawing::DrawCircle(player.Position(), kRRange, 0x33FF4F9Fu, 1.2f, 64);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("EvelynnOneTrick", "Evelynn ambush tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 650, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Hate Spike"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Allure charm"));
    WMenu->Add(new MenuSlider("CharmSeconds", "Charm arm time (tenths sec)", 25, 10, 25));
    WMenu->Add(new MenuSlider("MarkLifetimeMs", "Keep observed mark (ms)", 5000, 1500, 7000));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 45, 10, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Whiplash entry"));
    EMenu->Add(new MenuSlider("MaxEntryEnemies", "Maximum enemies at entry", 2, 1, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Last Caress"));
    RMenu->Add(new MenuSlider("ExecuteHP", "Execute health percent", 30, 10, 45));
    RMenu->Add(new MenuSlider("EscapeHP", "Escape health percent", 35, 10, 70));
    RMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum execute endpoint enemies", 2, 1, 5));
    RMenu->Add(new MenuSlider("MaxEscapeEnemies", "Maximum escape endpoint enemies", 1, 0, 3));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("EvelynnFarm", "Safe farm"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum farm mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("EvelynnCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and R ranges", false));
}

inline void OnLoad() {
    LastCastTick.fill(0);
    ManualOverrideUntil = LastAutoTargetId = LastAutoTick = WTargetId = WCastTick =
        QTargetId = ETargetId = RTargetId = IncomingThreatUntil = IncomingHardCCUntil = 0;
    PassiveUnlocked = DemonShade = WMarked = WArmed = QPrimed = EmpoweredE = RExecuteVisible = false;
    LastMode = Mode::None;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    PassiveUnlocked = DemonShade = WMarked = WArmed = QPrimed = EmpoweredE = RExecuteVisible = false;
}

inline constexpr const char* Scenarios[] = {
    "Pin all mechanics to Riot 26.15 and CommunityDragon 16.15",
    "Gate Demon Shade camouflage decisions behind level six and reconcile passive buffs by event and polling",
    "Preserve selected target before orbwalker fallback and reject protected or untargetable targets",
    "Prime Allure only on a reachable target and wait the full charm arm interval before entry",
    "Aim the Hate Spike opener with prediction, line width, collision and projectile-wall checks",
    "Track Hate Spike's initial cast and 550-range recast independently",
    "Preserve ordinary attack windup and manual casts before nonreactive spell ownership",
    "Use empowered Whiplash only as a safe 210-range entry through an armed charm",
    "Reject Whiplash entry through walls, turrets, mobility lock or excessive endpoint enemies",
    "Spend Last Caress only on a protected-safe execute inside the 30 percent threshold",
    "Use Last Caress as a directional escape only when low health or hard crowd control demands it",
    "Reject escape endpoints through walls, enemy turrets and unsafe enemy counts",
    "Automatic mode is restricted to lethal execute or hard crowd-control escape",
    "Combo prioritizes Allure, charm wait, Hate Spike, empowered Whiplash and execute",
    "Harass preserves mana and avoids unarmed Whiplash commits",
    "LaneClear delegates to neutral farm policy while Jungle and LastHit preserve mode-specific resources",
    "Flee prioritizes safe Last Caress escape before reactive Whiplash",
    "Yield after observed manual Q, W, E or R ownership",
    "Expose Q and R ranges without changing gameplay decisions",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Evelynn;
    controller.ControllerId = "champion.kuroaio.ai.evelynn.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIEvelynn.md";
    controller.ImplementationSummary =
        "Demon Shade level gate, stateful Allure charm wait, Hate Spike recast tracking, empowered Whiplash entry and conservative Last Caress execute/escape loop.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Evelynn
