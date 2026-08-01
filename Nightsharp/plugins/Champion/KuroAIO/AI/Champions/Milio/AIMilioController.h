#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIMilioGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Milio {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::HasAnyBuff;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocks;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Now;
using ControllerHelpers::Ready;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int SelectedAllyId = 0;
inline int ManualOwnershipUntil = 0;
inline int EnemyThreatUntil = 0;
inline int HardCcThreatUntil = 0;
inline int InterruptUntil = 0;
inline int ECharges = kEMaxCharges;
inline bool PassivePrimed = false;
inline Vector3 LastQAim{};
inline Vector3 LastCamp{};

inline bool Throttle(int slot, int delay = 55) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline AIHeroClient SelectEnemy(const AIHeroClient& selected, float range) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const auto orb = ControllerHelpers::OrbwalkerHeroTarget(range);
    if (Engine::ValidEnemy(orb, range)) return orb;
    return Engine::SelectTarget(range);
}

inline AIHeroClient SelectAlly(bool includePlayer = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    const auto ally = SelectProtectionAlly(
        kWRange, SelectedAllyId, SelectedAllyId == 0 ? 0 : Now() + 300,
        280.0f, 560.0f);
    if (Engine::ValidAlly(ally, kWRange) &&
        (includePlayer || ally.NetworkId() != player.NetworkId())) {
        SelectedAllyId = static_cast<int>(ally.NetworkId());
        return ally;
    }
    if (includePlayer) return player;
    return {};
}

inline bool AllyHardCc(const AIHeroClient& ally) {
    return HasAnyBuff(ally, {"stun", "Stun", "snare", "Snare", "root", "Root",
        "airborne", "Airborne", "knockup", "Knockup", "knockback", "Knockback",
        "fear", "Fear", "taunt", "Taunt", "charm", "Charm", "suppress",
        "Suppression", "silence", "Silence", "MilioR"});
}

inline bool AllyThreatened(const AIHeroClient& ally) {
    if (!Engine::ValidAlly(ally) || ally.IsDead()) return false;
    return AllyHardCc(ally) || Engine::CountEnemiesAt(ally.Position(), 650.0f) > 0 ||
        (ally.HealthPercent() <= Slider(RMenu, "ThreatHealth", 70));
}

inline bool SafeSupportCast(const AIHeroClient& ally, bool reactive) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidAlly(ally)) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), 500.0f);
    if (!reactive && enemies > Slider(TacticsMenu, "MaxNearbyEnemies", 3)) return false;
    if (!reactive && Engine::UnderEnemyTurret(player.Position()) &&
        enemies > Slider(TacticsMenu, "MaxTurretEnemies", 2)) return false;
    return true;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kQRange) ||
        !Ready(0, mode) || !Throttle(0) || PreserveAttack(reactive)) return false;
    const float travel = QTravelSeconds(player.Position().Distance2D(target.Position()));
    const Vec3 predicted = PredictPosition(target, travel);
    const Vec3 aim = QEndpoint(player.Position(), predicted);
    if (!QTargetReachable(player.Position(), predicted, target.BoundingRadius()) ||
        !QProjectileContacts(player.Position(), aim, predicted, target.BoundingRadius()) ||
        ProjectileWallBlocks(player.Position(), aim, kQWidth * 0.5f)) return false;
    const int nearby = Engine::CountEnemiesAt(player.Position(), 450.0f);
    if (!reactive && nearby > Slider(QMenu, "MaxNearbyEnemies", 2)) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    LastQAim = aim;
    PassivePrimed = true;
    return true;
}

inline bool CastW(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 90) ||
        PreserveAttack(reactive)) return false;
    const auto ally = SelectAlly(false);
    if (!Engine::ValidAlly(ally, kWRange) || !WCastInRange(player.Position(), ally.Position()) ||
        ally.HealthPercent() > Slider(WMenu, "AllyHealthThreshold", 72)) return false;
    if (!SafeSupportCast(ally, reactive)) return false;
    if (!Engine::ControllerCastUnit(1, ally)) return false;
    LastCastTick[1] = Now();
    LastCamp = ally.Position();
    PassivePrimed = true;
    return true;
}

inline int ReadECharges() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return ECharges;
    const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::E);
    if (spell.IsValid() && spell.MaxAmmo() == kEMaxCharges && spell.Ammo() >= 0)
        return std::clamp(spell.Ammo(), 0, kEMaxCharges);
    return ECharges;
}

inline bool CastE(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2, mode) || !Throttle(2, 65) ||
        PreserveAttack(reactive)) return false;
    ECharges = ReadECharges();
    if (!EChargesAvailable(ECharges)) return false;
    const auto ally = SelectAlly(true);
    if (!Engine::ValidAlly(ally, kERange) || !WCastInRange(player.Position(), ally.Position())) return false;
    const bool hardCc = AllyHardCc(ally) || HardCcThreatUntil > Now();
    const bool incoming = Engine::CountEnemiesAt(ally.Position(), 500.0f) > 0 ||
        EnemyThreatUntil > Now();
    const bool fleeing = mode == Mode::Flee;
    if (!EShieldWorthwhile(ally.HealthPercent(), hardCc, incoming, fleeing,
                           static_cast<float>(Slider(EMenu, "HealthThreshold", 82)))) return false;
    if (!SafeSupportCast(ally, reactive || hardCc)) return false;
    if (!Engine::ControllerCastUnit(2, ally)) return false;
    LastCastTick[2] = Now();
    ECharges = EChargesAfterCast(ECharges);
    PassivePrimed = true;
    return true;
}

inline bool RCandidate(const AIHeroClient& ally, bool& cleanse) {
    cleanse = false;
    if (!Engine::ValidAlly(ally) || ally.IsDead()) return false;
    if (!RInRange(GameObjects::Player().Position(), ally.Position())) return false;
    cleanse = AllyHardCc(ally);
    const bool threatened = AllyThreatened(ally) || EnemyThreatUntil > Now();
    return RShouldCleanse(cleanse, threatened, ally.HealthPercent()) ||
        RHealWorthwhile(ally.HealthPercent(), threatened,
                        Engine::CountEnemiesAt(ally.Position(), 650.0f),
                        Slider(RMenu, "MinimumThreatEnemies", 1));
}

inline AIHeroClient SelectRAlly(bool& cleanse) {
    const auto player = GameObjects::Player();
    AIHeroClient best{};
    float score = -1.0f;
    bool bestCleanse = false;
    auto consider = [&](const AIHeroClient& ally) {
        bool candidateCleanse = false;
        if (!RCandidate(ally, candidateCleanse)) return;
        const float urgency = (100.0f - ally.HealthPercent()) * 2.0f +
            static_cast<float>(Engine::CountEnemiesAt(ally.Position(), 650.0f)) * 24.0f +
            (candidateCleanse ? 180.0f : 0.0f);
        if (!best.IsValid() || urgency > score) {
            best = ally;
            score = urgency;
            bestCleanse = candidateCleanse;
        }
    };
    if (player.IsValid()) consider(player);
    for (const auto& ally : GameObjects::AllyHeroes()) consider(ally);
    cleanse = bestCleanse;
    return best;
}

inline bool CastR(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 140) ||
        PreserveAttack(reactive)) return false;
    bool cleanse = false;
    const auto ally = SelectRAlly(cleanse);
    if (!Engine::ValidAlly(ally) || !RInRange(player.Position(), ally.Position())) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), kRRadius);
    const bool urgent = reactive || cleanse || ally.HealthPercent() <=
        Slider(RMenu, "EmergencyHealth", 38);
    if (!urgent && enemies > Slider(RMenu, "MaxNearbyEnemies", 3)) return false;
    if (!urgent && Engine::UnderEnemyTurret(player.Position()) &&
        enemies > Slider(TacticsMenu, "MaxTurretEnemies", 2)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastCastTick[3] = Now();
    PassivePrimed = true;
    return true;
}

inline void Combo(const AIHeroClient& target) {
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(Mode::Combo)) return;
    if (CastE(Mode::Combo)) return;
    (void)CastR(Mode::Combo);
}

inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(QMenu, "HarassMana", 52)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(Mode::Harass);
}

inline void Flee(const AIHeroClient& pursuer) {
    if (CastE(Mode::Flee, true)) return;
    if (CastW(Mode::Flee, true)) return;
    (void)CastR(Mode::Flee, true);
    (void)pursuer;
}

inline void Farm(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(FarmMenu, "Mana", 42)) return;
    if (mode == Mode::Jungle || mode == Mode::LaneClear || mode == Mode::LastHit)
        (void)Engine::TryFarm(mode);
}

inline void Automatic(const AIHeroClient& selected) {
    if (CastR(Mode::Automatic, true)) return;
    if (CastE(Mode::Automatic, true)) return;
    if (CastW(Mode::Automatic, true)) return;
    if (Engine::ValidEnemy(selected) && HardCcThreatUntil > Now())
        (void)CastQ(selected, Mode::Automatic, true);
}

inline void ReconcileState() {
    const int now = Now();
    if (ManualOwnershipUntil <= now) ManualOwnershipUntil = 0;
    if (EnemyThreatUntil <= now) EnemyThreatUntil = 0;
    if (HardCcThreatUntil <= now) HardCcThreatUntil = 0;
    if (InterruptUntil <= now) InterruptUntil = 0;
    ECharges = ReadECharges();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (HasAnyBuff(player, {"MilioPassive", "MilioPBuff"})) PassivePrimed = true;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    if (ManualOwnershipUntil > Now()) return true;
    const auto target = SelectEnemy(selected, mode == Mode::Flee ? 1000.0f : kQRange);
    if (mode == Mode::Automatic) Automatic(target);
    else if (mode == Mode::Combo) Combo(target);
    else if (mode == Mode::Harass) Harass(target);
    else if (mode == Mode::Flee) Flee(NearestEnemyToPlayer(target, 1000.0f));
    else if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) Farm(mode);
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[slot] = now;
            if (slot == 2) {
                const int observed = ReadECharges();
                ECharges = Engine::WasControllerCast(2)
                    ? observed
                    : (observed == ECharges ? EChargesAfterCast(ECharges) : observed);
            }
            if (slot == 0 || slot == 1 || slot == 2 || slot == 3) PassivePrimed = true;
            if (!Engine::WasControllerCast(slot))
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    EnemyThreatUntil = std::max(EnemyThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl)
        HardCcThreatUntil = std::max(HardCcThreatUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick)) return;
    if (PassivePrimed) PassivePrimed = false;
}

inline void OnBuffState(const SDK::Events::BuffEventArgs& args, bool added) {
    if (!args.Sender.IsValid()) return;
    if (Engine::TextContains(args.BuffName, "MilioPassive") ||
        Engine::TextContains(args.BuffName, "MilioPBuff")) PassivePrimed = added;
    if (Engine::TextContains(args.BuffName, "MilioE")) ECharges = std::clamp(args.Count, 0, kEMaxCharges);
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) { OnBuffState(args, true); }
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) { OnBuffState(args, false); }
inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) { OnBuffState(args, true); }

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    const auto player = GameObjects::Player();
    if (player.IsValid() && (args.IsDirectedToPlayer ||
        (args.End.IsValid() && args.End.Distance2D(player.Position()) < 475.0f))) {
        EnemyThreatUntil = std::max(EnemyThreatUntil, Now() + 750);
        HardCcThreatUntil = std::max(HardCcThreatUntil, Now() + 500);
    }
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    const int remaining = args.EndTime > Game::Time()
        ? static_cast<int>((args.EndTime - Game::Time()) * 1000.0f) : 900;
    InterruptUntil = Now() + std::clamp(remaining, 250, 5000);
    HardCcThreatUntil = std::max(HardCcThreatUntil, InterruptUntil);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFFFA347u, 1.2f, 36);
    Drawing::DrawCircle(player.Position(), kWRange, 0xFF67D5FFu, 1.0f, 32);
    Drawing::DrawCircle(player.Position(), kRRadius, 0xFFB67CFFu, 1.0f, 32);
    if (LastCamp.IsValid() && !LastCamp.IsZero())
        Drawing::DrawCircle(LastCamp, kWRadius, 0xFF67D5FFu, 1.0f, 28);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("MilioTactics", "Milio support tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 560, 180, 1400));
    TacticsMenu->Add(new MenuSlider("MaxNearbyEnemies", "Maximum nearby enemies for support casts", 3, 0, 5));
    TacticsMenu->Add(new MenuSlider("MaxTurretEnemies", "Maximum enemies under turret", 2, 0, 5));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Ultra Mega Fire Kick"));
    QMenu->Add(new MenuSlider("MaxNearbyEnemies", "Maximum nearby enemies for Q", 2, 0, 5));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 52, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Cozy Campfire"));
    WMenu->Add(new MenuSlider("AllyHealthThreshold", "Campfire ally health threshold", 72, 20, 95));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Warm Hugs charges"));
    EMenu->Add(new MenuSlider("HealthThreshold", "Shield ally below health %", 82, 20, 95));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Breath of Life cleanse and heal"));
    RMenu->Add(new MenuSlider("ThreatHealth", "Threatened ally health %", 70, 20, 95));
    RMenu->Add(new MenuSlider("EmergencyHealth", "Emergency heal health %", 38, 10, 70));
    RMenu->Add(new MenuSlider("MinimumThreatEnemies", "Minimum nearby enemies for threat", 1, 0, 5));
    RMenu->Add(new MenuSlider("MaxNearbyEnemies", "Maximum nearby enemies when non-urgent", 3, 0, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("MilioFarm", "Resource-safe farm"));
    FarmMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 42, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("MilioCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q/W/R and Cozy Campfire ranges", false));
}

inline void OnLoad() {
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = SelectedAllyId = 0;
    ManualOwnershipUntil = EnemyThreatUntil = HardCcThreatUntil = InterruptUntil = 0;
    ECharges = kEMaxCharges;
    PassivePrimed = false;
    LastQAim = LastCamp = {};
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    LastQAim = LastCamp = {};
    PassivePrimed = false;
}

inline constexpr const char* Scenarios[] = {
    "Pin Fired Up passive base, AP and level-scaled AD empowerment to Riot 26.15 and CommunityDragon 16.15",
    "Reconcile passive empowerment and Warm Hugs charge state from buff events and polling",
    "Predict Ultra Mega Fire Kick contact with 1200 speed and reject projectile-wall paths",
    "Model Q first-contact collision, 140 kick displacement and 250 landing splash boundary",
    "Respect selected enemy first, then orbwalker target, then engine fallback",
    "Place Cozy Campfire on a valid ally within 650 cast range and keep the 350 camp radius useful",
    "Use Warm Hugs as a two-charge shield only for missing health, incoming damage, hard crowd control or flee",
    "Reconcile E ammo from runtime spell state and consume exactly one charge per cast",
    "Use Breath of Life only when an in-range ally needs cleanse, healing or threat relief",
    "Reserve R for hard crowd control or threatened low-health allies and apply ally threat policy",
    "Reject non-urgent casts under unsafe turret or excessive enemy density",
    "Preserve auto attack windup unless an urgent defensive reaction is active",
    "Yield to manual Q W E R ownership before polling decisions resume",
    "Automatic mode is defensive and never starts a fresh engage without a threat window",
    "Combo prioritizes Q setup, W camp sustain, E shield and R cleanse/heal",
    "Harass uses predicted Q before W sustain and never spends R offensively",
    "LaneClear Jungle and LastHit retain resource-safe farm policy without support spell waste",
    "Flee prioritizes E mobility shield, W camp repositioning and defensive R",
    "Capture enemy cast, gapcloser and interruptable hard-CC threats for reactive policy",
    "Draw Q/W/R and last Cozy Campfire range without changing decisions",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Milio;
    controller.ControllerId = "champion.kuroaio.ai.milio.support";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIMilio.md";
    controller.ImplementationSummary =
        "Passive empowerment tracking, collision-safe Q kick prediction, ally Cozy Campfire placement, "
        "two-charge Warm Hugs shields and threat-aware Breath of Life cleanse/heal.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Milio
