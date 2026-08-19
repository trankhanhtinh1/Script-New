#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIKarthusGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Karthus {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::HasCurrentResource;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NameEquals;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::Ready;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContains;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Now;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<int, 4> LastCastTick = {};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int ManualOwnershipUntil = 0;
inline int DeathTick = 0;
inline int RCastTick = 0;
inline int RChannelEndTick = 0;
inline int RTargetId = 0;
inline int WCastTick = 0;
inline int WTargetId = 0;
inline int ECastTick = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline bool DefileActive = false;
inline bool DeathPassive = false;
inline bool RChanneling = false;

inline bool RuntimeHasBuff(const AIHeroClient& unit,
                           std::initializer_list<const char*> names) {
    if (!unit.IsValid()) return false;
    for (const char* name : names) {
        if (name && unit.HasBuff(name)) return true;
    }
    return false;
}

inline bool IsProtectedTarget(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsDead() ||
           HasSpellShieldOrImmunity(target);
}

inline bool CastReadyFor(int slot, Mode mode, bool reactive = false,
                         bool lethal = false) {
    if (slot < 0 || slot >= 4 || !SpellEnabled(slot, mode) ||
        !Ready(slot, mode) || !CastThrottleReady(LastCastTick, slot,
                                                   reactive ? 0 : 55)) {
        return false;
    }
    if (!reactive && !lethal && PreserveAttack(reactive, lethal)) return false;
    if (!reactive && !lethal && PlayerMobilityLocked()) return false;
    return true;
}

inline bool ManaReserveAllows(int slot, Mode mode, float extraReserve = 0.0f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const float reservePercent = static_cast<float>(Slider(
        EMenu, mode == Mode::Harass ? "HarassMana" : "ManaReserve", 30));
    const float reserve = std::max(extraReserve,
        player.MaxMana() * reservePercent * 0.01f);
    return HasCurrentResource(SpellCost(slot) + reserve);
}

inline Vector3 PredictedAim(const AIHeroClient& target, int slot,
                            float extraDelay) {
    if (!Engine::ValidEnemy(target)) return {};
    Vector3 aim = PredictPosition(target, extraDelay);
    if (slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot]) {
        const auto prediction = Engine::RuntimeSpells[slot]->GetPrediction(target);
        if (static_cast<int>(prediction.Hitchance) >=
                static_cast<int>(SDK::HitChance::High) &&
            prediction.GetCastPosition().IsValid() &&
            !prediction.GetCastPosition().IsZero()) {
            aim = prediction.GetCastPosition();
        }
    }
    return aim;
}

inline int EnemyBodiesAt(const Vector3& center, float radius) {
    if (!center.IsValid()) return 0;
    const float area = std::max(0.0f, radius);
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) &&
            enemy.Position().Distance2D(center) <=
                area + enemy.BoundingRadius()) {
            ++count;
        }
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (minion.IsValid() && !minion.IsDead() &&
            minion.Position().Distance2D(center) <=
                area + minion.BoundingRadius()) {
            ++count;
        }
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (monster.IsValid() && !monster.IsDead() &&
            monster.Position().Distance2D(center) <=
                area + monster.BoundingRadius()) {
            ++count;
        }
    }
    return count;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (IsProtectedTarget(target) || !CastReadyFor(0, mode, reactive, lethal) ||
        !ManaReserveAllows(0, mode) ||
        player.Position().Distance2D(target.Position()) >
            kQRange + target.BoundingRadius()) return false;
    const Vector3 aim = ClampCastPoint(player.Position(),
        PredictedAim(target, 0, kQCastSeconds));
    if (!QCenterHits(aim, PredictPosition(target, kQCastSeconds),
                     target.BoundingRadius())) return false;
    const int nearby = std::max(0, EnemyBodiesAt(aim, kQRadius) - 1);
    const bool isolated = QIsIsolated(aim, target.Position(),
                                      target.BoundingRadius(), nearby);
    const bool predictedLethal = Engine::RuntimeSpells[0] &&
        Engine::RuntimeSpells[0]->GetDamage(target) * (isolated ? 2.0f : 1.0f) >=
            target.Health() + target.AllShield();
    if (lethal && !predictedLethal) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    LastAutoTick = std::max(LastAutoTick, LastCastTick[0]);
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (IsProtectedTarget(target) || !CastReadyFor(1, mode, reactive) ||
        !ManaReserveAllows(1, mode, 0.0f) ||
        player.Position().Distance2D(target.Position()) >
            kWRange + target.BoundingRadius()) return false;
    const Vector3 predicted = PredictPosition(target, kWCastSeconds);
    const Vec3 toward = Direction2D(player.Position(), predicted);
    if (toward.IsZero()) return false;
    const Vector3 center = ClampCastPoint(player.Position(),
        predicted - toward * 120.0f, kWRange - 1.0f);
    const Vec3 perpendicular = SharedGeometry::Rotate2D(toward,
        SharedGeometry::kPi * 0.5f);
    const WallPlan wall = BuildWall(center, perpendicular, SpellRank(1));
    if (!wall.Valid || Engine::UnderEnemyTurret(center) && !reactive) return false;
    if (!Engine::ControllerCastPosition(1, center)) return false;
    LastCastTick[1] = Now();
    WCastTick = LastCastTick[1];
    WTargetId = static_cast<int>(target.NetworkId());
    return true;
}

inline int DefileContactCount() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    return Engine::CountEnemiesAt(player.Position(), kERadius);
}

inline bool CastDefile(bool enable, Mode mode, bool reactive = false,
                       bool lethal = false) {
    if (!CastReadyFor(2, mode, reactive, lethal) ||
        (!DeathPassive && !ManaReserveAllows(2, mode,
            static_cast<float>(Slider(EMenu, "FlatReserve", 90))))) return false;
    if (enable == DefileActive) return false;
    const int contacts = DefileContactCount();
    if (enable && contacts <= 0 && !lethal && !reactive) return false;
    const auto player = GameObjects::Player();
    const float seconds = static_cast<float>(
        Slider(EMenu, "ForecastSeconds", 1));
    if (enable && !DeathPassive && !ShouldEnableDefile(
            CurrentResource(), SpellRank(2), seconds,
            static_cast<float>(Slider(EMenu, "FlatReserve", 90)),
            contacts > 0, lethal)) return false;
    if (!Engine::ControllerCastSelf(2)) return false;
    LastCastTick[2] = Now();
    DefileActive = enable;
    (void)player;
    return true;
}

inline bool CanStartRequiem(const AIHeroClient& target, Mode mode,
                            bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!Engine::ValidEnemy(target) || HasSpellShieldOrImmunity(target) ||
        !CastReadyFor(3, mode, reactive) ||
        !RequiemCanStart(CurrentResource(), SpellCost(3), true,
                         player.IsDead(), DeathPassive, RChanneling)) return false;
    if (!DeathPassive && PlayerManaPercent() <
            Slider(RMenu, "MinimumMana", 52)) return false;
    if (!reactive && !DeathPassive &&
        (Engine::UnderEnemyTurret(player.Position()) ||
         Engine::CountEnemiesAt(player.Position(), 700.0f) >
             Slider(RMenu, "MaxChannelEnemies", 2))) return false;
    return true;
}

inline bool CastRequiem(const AIHeroClient& target, Mode mode,
                        bool reactive = false, bool lethal = false) {
    if (!CanStartRequiem(target, mode, reactive)) return false;
    const float damage = Engine::RuntimeSpells[3]
        ? Engine::RuntimeSpells[3]->GetDamage(target)
        : RequiemRawDamage(SpellRank(3), GameObjects::Player().AP());
    const bool execute = damage >= target.Health() + target.AllShield();
    const bool interruptOnly = reactive && damage < target.Health() + target.AllShield();
    const bool multi = Engine::CountEnemiesAt(target.Position(), 650.0f) >=
        Slider(RMenu, "MinimumTargets", 2);
    if (lethal && !execute) return false;
    if (!lethal && !RequiemShouldCommit(damage, target.Health(),
            target.AllShield(), HasSpellShieldOrImmunity(target), multi,
            interruptOnly)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RChanneling = true;
    RCastTick = Now();
    RChannelEndTick = RCastTick + static_cast<int>(kRChannelSeconds * 1000.0f);
    RTargetId = static_cast<int>(target.NetworkId());
    LastCastTick[3] = RCastTick;
    (void)execute;
    return true;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (CastQ(target, mode, true, true)) return true;
    return CastRequiem(target, mode, true, true);
}

inline bool TryFarm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool jungle = mode == Mode::Jungle;
    const float minimumMana = static_cast<float>(Slider(
        FarmMenu, jungle ? "JungleMana" : "LaneMana", jungle ? 28 : 46));
    if (PlayerManaPercent() < minimumMana && mode != Mode::LastHit) return false;
    if (mode == Mode::LastHit) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!minion.IsValid() || minion.IsDead() ||
                player.Position().Distance2D(minion.Position()) > kQRange) continue;
            const Vector3 aim = minion.Position();
            const int nearby = std::max(0, EnemyBodiesAt(aim, kQRadius) - 1);
            const float damage = Engine::RuntimeSpells[0]
                ? Engine::RuntimeSpells[0]->GetDamage(minion) *
                    (nearby == 0 ? 2.0f : 1.0f) : 0.0f;
            if (damage >= minion.Health() && CastReadyFor(0, mode) &&
                ManaReserveAllows(0, mode) && Engine::ControllerCastPosition(0, aim)) {
                LastCastTick[0] = Now();
                return true;
            }
        }
        return false;
    }
    if (!SpellEnabled(0, mode) || !CastReadyFor(0, mode) ||
        !ManaReserveAllows(0, mode)) return false;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (minion.IsValid() && !minion.IsDead() &&
            player.Position().Distance2D(minion.Position()) <= kQRange) {
            if (Engine::ControllerCastPosition(0, minion.Position())) {
                LastCastTick[0] = Now();
                return true;
            }
        }
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (monster.IsValid() && !monster.IsDead() &&
            player.Position().Distance2D(monster.Position()) <= kQRange) {
            if (Engine::ControllerCastPosition(0, monster.Position())) {
                LastCastTick[0] = Now();
                return true;
            }
        }
    }
    return false;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    const Mode decisionMode = mode == Mode::None ? Mode::Automatic : mode;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int now = Now();
    if (RuntimeHasBuff(player, {"KarthusDefile", "KarthusDefileActive"})) DefileActive = true;
    if (RuntimeHasBuff(player, {"KarthusDeathDefied", "KarthusDeathDefiedBuff", "Death_Defied"})) {
        if (!DeathPassive) DeathTick = now;
        DeathPassive = true;
    }
    if (!player.IsDead() && DeathPassive && now > DeathTick + 7000) DeathPassive = false;
    if (player.IsDead() && !DeathPassive && DeathTick == 0) {
        DeathTick = now;
        DeathPassive = true;
    }
    RChanneling = RuntimeHasBuff(player, {"KarthusFallenOne", "KarthusFallenOneChannel"}) ||
                  (RChanneling && now < RChannelEndTick);
    if (RChanneling && now >= RChannelEndTick) RChanneling = false;
    if (DefileActive && !DeathPassive &&
        (DefileContactCount() == 0 || CurrentResource() <=
            Slider(EMenu, "FlatReserve", 90))) {
        (void)CastDefile(false, decisionMode, true);
    }
    if (ManualOwnershipUntil > now) return true;
    const AIHeroClient target = PreferredEnemyTarget(
        selected, decisionMode == Mode::Flee ? 1100.0f : 10000.0f);
    if (RChanneling) return true;
    if (TryKillSecure(target, decisionMode)) return true;
    if (decisionMode == Mode::Flee) {
        if (Engine::ValidEnemy(target) &&
            CastW(target, decisionMode, true)) return true;
        return Engine::ValidEnemy(target) &&
            CastDefile(true, decisionMode, true);
    }
    if (decisionMode == Mode::LaneClear ||
        decisionMode == Mode::Jungle ||
        decisionMode == Mode::LastHit) {
        return TryFarm(decisionMode);
    }
    if (!Engine::ValidEnemy(target)) return false;
    if (decisionMode == Mode::Combo || decisionMode == Mode::Automatic) {
        if (CastW(target, decisionMode)) return true;
        if (CastQ(target, decisionMode)) return true;
        if (CastDefile(true, decisionMode)) return true;
        return false;
    }
    if (decisionMode == Mode::Harass) {
        if (PlayerManaPercent() < Slider(TacticsMenu, "HarassMana", 55)) return false;
        if (CastW(target, decisionMode)) return true;
        return CastQ(target, decisionMode);
    }
    return false;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[slot] = Now();
            if (!Engine::WasControllerCast(slot)) ManualOwnershipUntil = Now() +
                Slider(TacticsMenu, "ManualOwnershipMs", 560);
            if (slot == 2 && !Engine::WasControllerCast(slot)) DefileActive = !DefileActive;
            if (slot == 3) {
                RChanneling = true;
                RCastTick = Now();
                RChannelEndTick = RCastTick + 3000;
            }
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args, 220.0f, 100.0f, 300, 250, 220, 1500, 500);
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

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!args.Sender.IsValid() || !args.BuffName[0]) return;
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "Defile")) DefileActive = added;
    if (Engine::TextContains(args.BuffName, "DeathDefied") ||
        Engine::TextContains(args.BuffName, "Death_Defied")) {
        DeathPassive = added;
        if (added) DeathTick = Now();
        else DeathTick = 0;
    }
    if (Engine::TextContains(args.BuffName, "FallenOne")) {
        RChanneling = added;
        if (added) {
            RCastTick = Now();
            RChannelEndTick = RCastTick + 3000;
        }
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) { UpdateBuffState(args, true); }
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) { UpdateBuffState(args, false); }
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (RChanneling && args.Target.IsValid()) args.Process = false;
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)args;
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    (void)args;
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) { (void)args; }
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) { (void)args; }
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) { (void)args; }
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) { (void)args; }

inline void OnDraw() {
    if (!CoachMenu || !Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFB070FFu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kERadius, DefileActive ? 0xFFFF5522u : 0xFFAA66CCu, 1.0f, 40);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("KarthusOneTrick", "Karthus state tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 560, 180, 1200));
    TacticsMenu->Add(new MenuSlider("HarassMana", "Minimum harass mana (%)", 55, 10, 95));
    QMenu = TacticsMenu->AddSubMenu(new Menu("LayWaste", "Isolated Q geometry"));
    QMenu->Add(new MenuBool("PreferIsolated", "Prefer isolated double damage", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("WallOfPain", "Wall slow and MR shred"));
    WMenu->Add(new MenuBool("Peel", "Use W for peel", true));
    EMenu = TacticsMenu->AddSubMenu(new Menu("Defile", "Toggle and mana reserve"));
    EMenu->Add(new MenuSlider("ManaReserve", "Minimum mana reserve (%)", 30, 0, 80));
    EMenu->Add(new MenuSlider("HarassMana", "Harass reserve (%)", 58, 0, 95));
    EMenu->Add(new MenuSlider("FlatReserve", "Flat emergency reserve", 90, 0, 500));
    EMenu->Add(new MenuSlider("ForecastSeconds", "Defile mana forecast (s)", 1, 0, 5));
    RMenu = TacticsMenu->AddSubMenu(new Menu("Requiem", "Protected global channel"));
    RMenu->Add(new MenuSlider("MinimumMana", "Minimum mana (%)", 52, 0, 95));
    RMenu->Add(new MenuSlider("MaxChannelEnemies", "Max nearby enemies while channeling", 2, 0, 5));
    RMenu->Add(new MenuSlider("MinimumTargets", "Minimum multi-target R count", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("KarthusFarm", "Q farming and Defile sustain"));
    FarmMenu->Add(new MenuSlider("LaneMana", "Lane-clear mana (%)", 46, 0, 95));
    FarmMenu->Add(new MenuSlider("JungleMana", "Jungle mana (%)", 28, 0, 95));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("KarthusCoach", "State visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and Defile ranges", false));
}

inline void OnLoad() {
    LastCastTick = {};
    LastAutoTargetId = LastAutoTick = ManualOwnershipUntil = 0;
    DeathTick = RCastTick = RChannelEndTick = RTargetId = 0;
    WCastTick = WTargetId = ECastTick = 0;
    IncomingThreatUntil = IncomingHardCcUntil = 0;
    DefileActive = DeathPassive = RChanneling = false;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    OnLoad();
}

inline constexpr const char* Scenarios[] = {
    "Pin Summoner's Rift Karthus to Riot 26.15 and CommunityDragon PC 16.15",
    "Aim Lay Waste from predicted target position within the real 875 range",
    "Double Lay Waste damage only when no second enemy body overlaps the 160 radius",
    "Keep ground-targeted Q prediction honest without inventing missile collision",
    "Place Wall of Pain as a perpendicular finite segment rather than a generic slow",
    "Apply rank-dependent wall length and slow plus the current 25 percent MR shred",
    "Reject Wall of Pain casts that place the corridor under an enemy turret",
    "Track Defile toggle from local cast events, buff events and polling reconciliation",
    "Enable Defile only for live champion contact or a lethal close-range window",
    "Stop Defile when contact is lost or the configured flat mana reserve is reached",
    "Forecast Defile mana per second and preserve Q/W/R resources",
    "Track Death Defied's seven-second post-death spell window",
    "Permit Q W E and Requiem during the death passive without normal movement logic",
    "Start Requiem as a protected three-second stationary channel",
    "Require exact mitigated lethal damage or a meaningful multi-target outcome for R",
    "Reject Requiem under turret or excessive nearby enemies unless lethal/reactive",
    "Reconcile Requiem channel completion and manual cast ownership from events and polling",
    "Prefer the selected target before global fallback target selection",
    "Preserve auto-attack windup and yield briefly after manual spell input",
    "Use incoming cast telemetry to record hard-CC pressure without taking movement ownership",
    "Combo uses W corridor, isolated Q, contact Defile and exact Requiem gates",
    "Harass uses W/Q with an independent mana floor and no forced Defile drain",
    "LaneClear Jungle and LastHit use Q health and isolation checks without generic ordering",
    "Flee uses W peel and only a reactive Defile escape toggle",
    "Automatic mode remains conservative and never invents a turret dive",
    "Reject protected, dead or invulnerable targets before offensive casts",
    "Draw live Q and Defile boundaries without changing gameplay decisions",
    "Do not automate movement, Flash, items or orbwalker target ownership",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Karthus;
    controller.ControllerId = "champion.kuroaio.ai.karthus.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIKarthus.md";
    controller.ImplementationSummary =
        "Isolated Q double-hit geometry, finite MR-shredding Wall of Pain, "
        "contact and mana-reserved Defile toggle, and death-passive-aware "
        "three-second Requiem channel with event/poll reconciliation.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Karthus
