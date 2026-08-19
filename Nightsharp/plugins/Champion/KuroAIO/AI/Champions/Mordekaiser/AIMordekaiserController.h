#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIMordekaiserGeometry.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Mordekaiser {

using namespace Geometry;
using ControllerHelpers::AP;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Now;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;
inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int PassiveStacks = 0;
inline bool PassiveActive = false;
inline int PassiveExpireTick = 0;
inline int PassivePulseTick = 0;
inline float WStored = 0.0f;
inline float WShield = 0.0f;
inline bool WShieldActive = false;
inline int WCastTick = 0;
inline int RTargetId = 0;
inline int RCastTick = 0;
inline int RExpireTick = 0;
inline bool RActive = false;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;

inline bool Ready(int slot, Mode mode) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode);
}
inline bool Throttle(int slot, int delay = 70) {
    return slot >= 0 && slot < 4 && Now() - LastCastTick[slot] >= delay;
}
inline bool Protected(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}
inline bool InRange(const AIHeroClient& target, float range) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target, range + target.BoundingRadius()) &&
        player.Position().Distance2D(target.Position()) <= range + target.BoundingRadius();
}
inline float QDamage(const AIHeroClient& target, bool isolated = false) {
    const auto player = GameObjects::Player();
    return player.IsValid() ? player.CalculateMagicDamage(target,
        QRawDamage(SpellRank(0), player.TotalAttackDamage(), player.AP(), isolated)) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    const int rank = std::clamp(SpellRank(2), 1, 5);
    const float raw = RankValue(rank, {70.0f, 90.0f, 110.0f, 130.0f, 150.0f}) +
        0.60f * std::max(0.0f, player.AP());
    return player.IsValid() ? player.CalculateMagicDamage(target, raw) : 0.0f;
}
inline float RealmThreat(const AIHeroClient& target) {
    return QDamage(target, true) + EDamage(target);
}
inline bool ResourceReady(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (mode == Mode::Harass) return player.HealthPercent() >= Slider(WMenu, "HarassHP", 32);
    return true;
}
inline bool PreserveWindup(bool reactive, bool lethal = false) {
    return PreserveAttack(reactive, lethal) && Bool(TacticsMenu, "PreserveAA", true);
}
inline bool IsWRecast() {
    return WShieldActive || RuntimeNameContains(1, "recast") || RuntimeNameContains(1, "indestructible2");
}
inline bool IsRealmRuntime() {
    return RuntimeNameContains(3, "mordekaiserr") || RuntimeNameContains(3, "realmofdeath");
}

inline bool BuildQOtherBodies(const AIHeroClient& target,
                              std::array<Vec3, 16>& positions,
                              std::array<float, 16>& radii) {
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (count >= static_cast<int>(positions.size()) || !Engine::ValidEnemy(enemy) ||
            enemy.NetworkId() == target.NetworkId()) continue;
        positions[count] = PredictPosition(enemy, kQDelay);
        radii[count++] = enemy.BoundingRadius();
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (count >= static_cast<int>(positions.size()) || !minion.IsValid() || minion.IsDead() ||
            !minion.IsTargetable()) continue;
        positions[count] = PredictPosition(minion, kQDelay);
        radii[count++] = minion.BoundingRadius();
    }
    return count > 0;
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !InRange(target, kQRange) ||
        !Ready(0, mode) || !Throttle(0) || PreserveWindup(reactive, Lethal(target, QDamage(target)))) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vec3 aim = prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero()
        ? prediction.GetCastPosition() : PredictPosition(target, kQDelay);
    if (!aim.IsValid() || aim.IsZero() || !QHits(player.Position(), aim,
            PredictPosition(target, kQDelay), target.BoundingRadius())) return false;
    std::array<Vec3, 16> bodies{};
    std::array<float, 16> radii{};
    int bodyCount = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (bodyCount >= static_cast<int>(bodies.size()) || !Engine::ValidEnemy(enemy) ||
            enemy.NetworkId() == target.NetworkId()) continue;
        bodies[bodyCount] = PredictPosition(enemy, kQDelay);
        radii[bodyCount++] = enemy.BoundingRadius();
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (bodyCount >= static_cast<int>(bodies.size()) || !minion.IsValid() || minion.IsDead() ||
            !minion.IsTargetable()) continue;
        bodies[bodyCount] = PredictPosition(minion, kQDelay);
        radii[bodyCount++] = minion.BoundingRadius();
    }
    const bool isolated = IsolatedQ(aim, PredictPosition(target, kQDelay), target.BoundingRadius(),
                                    bodies, radii, bodyCount);
    const bool lethal = Lethal(target, QDamage(target, isolated));
    if (!reactive && prediction.GetCastPosition().IsValid() && prediction.Hitchance < SDK::HitChance::High && !lethal) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    PassiveStacks = std::min(3, PassiveStacks + 1);
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 80) || PreserveWindup(reactive)) return false;
    const int rank = std::clamp(SpellRank(1), 1, 5);
    const bool recast = IsWRecast();
    if (recast) {
        const bool emergency = reactive || player.HealthPercent() <= Slider(WMenu, "HealHP", 48);
        if (!emergency && mode != Mode::Combo) return false;
        if (WShield <= 0.0f && !player.HasBuff("MordekaiserW")) return false;
        if (!Engine::ControllerCastSelf(1)) return false;
        WShield = 0.0f; WStored = 0.0f; WShieldActive = false; WCastTick = Now(); LastCastTick[1] = Now();
        (void)rank; (void)target;
        return true;
    }
    const float minimum = static_cast<float>(Slider(WMenu, "MinimumStored", 40));
    if (WStored < minimum && !reactive && player.HealthPercent() > Slider(WMenu, "ShieldHP", 42)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    WShield = WShieldConversion(rank, WStored);
    WShieldActive = true; WCastTick = Now(); LastCastTick[1] = Now();
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !InRange(target, kERange) ||
        !Ready(2, mode) || !Throttle(2) || PlayerMobilityLocked() ||
        PreserveWindup(reactive, Lethal(target, EDamage(target)))) return false;
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    const Vec3 aim = prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero()
        ? prediction.GetCastPosition() : PredictPosition(target, kETravelSeconds);
    if (!aim.IsValid() || aim.IsZero() || !ConeContains(player.Position(), aim,
            PredictPosition(target, kETravelSeconds), target.BoundingRadius()) ||
        !ProjectileReaches(player.Position(), aim) || ProjectileWallBlocksFromPlayer(aim, kEWidth * 0.5f)) return false;
    const bool lethal = Lethal(target, EDamage(target));
    const bool defensive = reactive || player.HealthPercent() <= Slider(EMenu, "PeelHP", 45);
    if (!defensive && !lethal && Engine::UnderEnemyTurret(player.Position())) return false;
    if (!defensive && !lethal && Engine::CountEnemiesAt(player.Position(), 650.0f) > Slider(EMenu, "MaxCommitEnemies", 2)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    LastCastTick[2] = Now(); PassiveStacks = std::min(3, PassiveStacks + 1);
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false, bool manual = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || Protected(target) || !InRange(target, kRRange) ||
        !Ready(3, mode) || !Throttle(3, 120) || PlayerMobilityLocked() ||
        IsRealmRuntime() || RActive) return false;
    const int id = static_cast<int>(target.NetworkId());
    const bool low = target.HealthPercent() <= Slider(RMenu, "TargetHP", 60);
    const bool lethal = Lethal(target, RealmThreat(target));
    const bool defensive = reactive || player.HealthPercent() <= Slider(RMenu, "DefensiveHP", 34);
    RealmCommitContext context{true, true, false, false,
        Engine::UnderEnemyTurret(player.Position()), lethal, defensive, manual, low, false,
        Engine::CountEnemiesAt(player.Position(), 650.0f), Slider(RMenu, "MaxCommitEnemies", 2)};
    if (!SafeRealmCommit(context)) return false;
    if (PreserveWindup(reactive, lethal)) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    RTargetId = id; RCastTick = Now(); RExpireTick = RealmExpireTick(RCastTick); RActive = true;
    LastCastTick[3] = Now();
    return true;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, QDamage(target, true)) && CastQ(target, mode, true)) return true;
    if (Lethal(target, EDamage(target)) && CastE(target, mode, true)) return true;
    return false;
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastQ(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || !ResourceReady(Mode::Harass)) return;
    if (CastE(target, Mode::Harass)) return;
    (void)CastQ(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastW(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastE(target, Mode::Flee, true);
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    PassiveActive = player.HasBuff("MordekaiserPassive") || (PassiveActive && PassiveExpireTick > now);
    if (!PassiveActive && PassiveStacks >= 3) PassiveStacks = 0;
    if (WShieldActive && now - WCastTick > 4200) { WShieldActive = false; WShield = 0.0f; }
    if (RActive && (now >= RExpireTick || RTargetId == 0)) { RActive = false; RTargetId = RExpireTick = 0; }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid()) continue;
        if (enemy.HasBuff("MordekaiserR") || enemy.HasBuff("MordekaiserRealm")) {
            RTargetId = static_cast<int>(enemy.NetworkId()); RActive = true;
            RExpireTick = std::max(RExpireTick, now + kRDurationMs);
        }
    }
    if (PassiveActive && PassivePulseTick <= now) PassivePulseTick = now + static_cast<int>(kPassivePulseSeconds * 1000.0f);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const auto target = PreferredEnemyTarget(selected, mode == Mode::Flee ? 1000.0f : kRRange + 60.0f);
    if (ManualOwnershipUntil > Now()) return true;
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target)) {
        if (CastW(target, mode, true)) return true;
        if (CastE(target, mode, true)) return true;
    }
    if (!RActive && mode != Mode::LaneClear && mode != Mode::Jungle && mode != Mode::LastHit &&
        TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1000.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
        if (!PassiveActive && Engine::CountEnemiesAt(GameObjects::Player().Position(), 450.0f) > 0)
            (void)CastQ(target, mode);
        if (!WShieldActive) (void)Engine::TryFarm(mode);
        break;
    case Mode::LastHit:
        if (!WShieldActive) (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) && IncomingHardCCUntil > Now()) (void)CastE(target, mode, true);
        else if (Engine::ValidEnemy(target) && !RActive) (void)CastR(target, mode, true);
        break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            if (!Engine::WasControllerCast(slot)) ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 650);
            LastCastTick[slot] = now;
            if (slot == 0 || slot == 2) PassiveStacks = std::min(3, PassiveStacks + 1);
            if (slot == 1 && IsWRecast()) { WShieldActive = false; WShield = 0.0f; WStored = 0.0f; }
            if (slot == 3) { RCastTick = now; RExpireTick = RealmExpireTick(now); }
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(IncomingHardCCUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    WStored += WStoredResource(0.0f, 90.0f);
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int id = args.Sender.IsValid() ? static_cast<int>(args.Sender.NetworkId) : 0;
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "MordekaiserPassive")) {
            PassiveActive = true; PassiveStacks = 3; PassiveExpireTick = Now() + kPassiveDurationMs;
        }
        if (Engine::TextContains(args.BuffName, "MordekaiserW")) WShieldActive = true;
    }
    if (Engine::TextContains(args.BuffName, "MordekaiserR") || Engine::TextContains(args.BuffName, "RealmOfDeath")) {
        RTargetId = id; RActive = true; RCastTick = Now(); RExpireTick = RealmExpireTick(RCastTick);
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "MordekaiserPassive")) { PassiveActive = false; PassiveStacks = 0; PassiveExpireTick = 0; }
    if (Engine::TextContains(args.BuffName, "MordekaiserW")) { WShieldActive = false; WShield = 0.0f; }
    if (Engine::TextContains(args.BuffName, "MordekaiserR") || Engine::TextContains(args.BuffName, "RealmOfDeath")) { RActive = false; RTargetId = RExpireTick = 0; }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    LastAutoTargetId = static_cast<int>(args.Target.NetworkId()); LastAutoTick = Now();
    PassiveStacks = std::min(3, PassiveStacks + 1);
    WStored += WStoredResource(35.0f, 0.0f);
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId != 0 ? args.TargetNetworkId : args.Target.NetworkId);
        LastAutoTick = Now(); PassiveStacks = std::min(3, PassiveStacks + 1);
        WStored += WStoredResource(35.0f, 0.0f);
    }
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs&) { IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 650); }
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs&) { IncomingHardCCUntil = std::max(IncomingHardCCUntil, Now() + 700); }
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFB94A4Au, 1.0f, 48);
    Drawing::DrawCircle(player.Position(), kERange, 0xFF6F3FAEu, 1.0f, 48);
    if (PassiveActive) Drawing::DrawCircle(player.Position(), kPassiveRadius, 0xFFCA4A4Au, 1.0f, 48);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("MordekaiserTactics", "Mordekaiser darkness tactics"));
    TacticsMenu->Add(new MenuBool("PreserveAA", "Preserve attack windup", true));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual spell (ms)", 650, 200, 1400));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Obliterate"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Indestructible"));
    WMenu->Add(new MenuSlider("MinimumStored", "Minimum stored damage", 40, 0, 300));
    WMenu->Add(new MenuSlider("ShieldHP", "Shield below player HP (%)", 42, 15, 80));
    WMenu->Add(new MenuSlider("HealHP", "Heal below player HP (%)", 48, 15, 85));
    WMenu->Add(new MenuSlider("HarassHP", "Harass minimum HP (%)", 32, 5, 90));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Death's Grasp"));
    EMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at origin", 2, 1, 5));
    EMenu->Add(new MenuSlider("PeelHP", "Peel below player HP (%)", 45, 15, 80));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Realm of Death"));
    RMenu->Add(new MenuSlider("TargetHP", "Target health threshold (%)", 60, 20, 90));
    RMenu->Add(new MenuSlider("DefensiveHP", "Defensive realm below HP (%)", 34, 15, 70));
    RMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum nearby enemies", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("MordekaiserFarm", "Darkness farming"));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("MordekaiserCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q, E and passive ranges", false));
}
inline void OnLoad() {
    LastCastTick.fill(0); LastAutoTargetId = LastAutoTick = 0; PassiveStacks = 0;
    PassiveActive = false; PassiveExpireTick = PassivePulseTick = 0; WStored = WShield = 0.0f;
    WShieldActive = false; WCastTick = 0; RTargetId = RCastTick = RExpireTick = 0; RActive = false;
    ManualOwnershipUntil = IncomingThreatUntil = IncomingHardCCUntil = 0;
}
inline void OnUnload() { TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr; OnLoad(); }

inline constexpr const char* Scenarios[] = {
    "Pin Darkness Rise aura radius, pulse damage, movement speed and four-second buff lifetime to Riot 26.15 / CommunityDragon 16.15",
    "Build passive activation from observed Q, E, spell and attack events, then reconcile MordekaiserPassive by polling",
    "Use Obliterate prediction, 675 reach, target radius and isolated-body geometry before applying the 40 percent isolated multiplier",
    "Preserve a valuable ordinary auto-attack windup unless a reactive or lethal branch is being taken",
    "Accumulate Indestructible's 35 percent dealt and 15 percent received stored resource from events and convert it into rank-scaled shield or heal",
    "Use W recast only with a real shield or emergency health gate and clear stale shield state from events and polling",
    "Aim Death's Grasp through the live cone, 700 reach, projectile travel and wall test before recording a pull commitment",
    "Reject E while mobility-locked, under unsafe turret pressure, or over the configured nearby-enemy limit",
    "Prefer the selected enemy, then orbwalker target, then the controller selector fallback",
    "Commit Realm of Death only for a low-health, lethal, defensive or explicitly manual target with range and protection gates",
    "Track Realm of Death target id and seven-second duration from cast, buff add/remove and polling reconciliation",
    "Never recast Realm of Death onto an already tracked target or while the player is mobility-locked",
    "React to incoming hard crowd control with Indestructible and Death's Grasp before ordinary combo work",
    "Combo sequences E pull, isolated Q, W conversion and safe R commitment",
    "Harass uses E/Q without consuming an unearned realm commitment or stored shield",
    "LaneClear and Jungle use Q and shared farm policy while avoiding unsafe R decisions",
    "LastHit delegates health-predicted farm policy and never starts a Realm of Death",
    "Flee prioritizes emergency Indestructible and reactive Death's Grasp peel",
    "Automatic mode is defense or low-target execution only and never starts a neutral engage",
    "Draw range coaching without changing cast decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Mordekaiser;
    controller.ControllerId = "champion.kuroaio.ai.mordekaiser.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIMordekaiser.md";
    controller.ImplementationSummary = "Darkness Rise state, isolated Obliterate, stored Indestructible conversion, collision-safe Death's Grasp and seven-second Realm tracking.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Mordekaiser
