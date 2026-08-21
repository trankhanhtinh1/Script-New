#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIUrgotGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>

namespace Plugins::KuroAIO::AI::Controllers::Urgot {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::Lethal;
using ControllerHelpers::Now;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;
inline std::array<int, kLegCount> LegReadyTick{};
inline std::array<int, 4> LastCastTick{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int QMarkedTargetId = 0;
inline int QMarkExpireTick = 0;
inline int WTargetId = 0;
inline int WCastTick = 0;
inline int WStopTick = 0;
inline bool WActive = false;
inline int ETargetId = 0;
inline int EStunExpireTick = 0;
inline int RTargetId = 0;
inline int RCastTick = 0;
inline int RExpireTick = 0;
inline bool RHooked = false;
inline bool RRecastReady = false;
inline bool RChanneling = false;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;

inline bool ResourceReady(Mode mode) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (mode == Mode::Harass) return player.ManaPercent() >= Slider(WMenu, "HarassMana", 45);
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit)
        return player.ManaPercent() >= Slider(FarmMenu, "FarmMana", 35);
    return player.ManaPercent() >= 5.0f;
}

inline bool Ready(int slot, Mode mode) {
    return slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot] &&
        Engine::RuntimeSpells[slot]->IsReady() && SpellEnabled(slot, mode) &&
        ResourceReady(mode);
}

inline bool Throttle(int slot, int delay = 75) {
    return slot >= 0 && slot < 4 && Now() - LastCastTick[slot] >= delay;
}

inline bool ProtectedTarget(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target) || target.HasBuff("UrgotRImmune");
}

inline bool Marked(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
        (MarkActive(static_cast<int>(target.NetworkId()), QMarkedTargetId, Now(), QMarkExpireTick) ||
         target.HasBuff("UrgotWTarget") || target.HasBuff("UrgotQSlow"));
}

inline bool PreserveWindup(bool reactive, bool lethal = false) {
    return PreserveAttackWindup(Orbwalker::IsWindingUp(), reactive, lethal) &&
        Bool(TacticsMenu, "PreserveAA", true);
}

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() ? player.CalculatePhysicalDamage(target,
        QRawDamage(SpellRank(0), player.BonusAttackDamage(), target.MaxHealth())) : 0.0f;
}
inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() ? player.CalculatePhysicalDamage(target,
        WPurgeShotRawDamage(SpellRank(1), player.TotalAttackDamage())) : 0.0f;
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() ? player.CalculatePhysicalDamage(target,
        ERawDamage(SpellRank(2), player.BonusAttackDamage())) : 0.0f;
}
inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() ? player.CalculatePhysicalDamage(target,
        RRawDamage(SpellRank(3), player.BonusAttackDamage())) : 0.0f;
}

inline bool LiveWall(const Vec3& point) { return SDK::NavMesh::IsWall(point); }
inline bool TerrainBlocked(const Vec3& origin, const Vec3& endpoint) {
    return TerrainBlocks(origin, endpoint, &LiveWall);
}

inline CollisionResult FirstQCollision(const Vec3& origin, const Vec3& endpoint,
                                       const AIHeroClient& intended) {
    std::array<CollisionResult, 16> candidates{};
    int count = 0;
    auto append = [&](const Vec3& position, float radius, int id) {
        if (count >= static_cast<int>(candidates.size()) || !position.IsValid() || id == 0) return;
        candidates[count++] = {SegmentHits(origin, endpoint, position,
            kQWidth * 0.5f, radius), id, position};
    };
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (minion.IsValid() && !minion.IsDead() && minion.IsTargetable())
            append(PredictPosition(minion, kQDelay), minion.BoundingRadius(),
                   static_cast<int>(minion.NetworkId()));
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kQRange + 100.0f)) continue;
        append(PredictPosition(enemy, kQDelay), enemy.BoundingRadius(),
               static_cast<int>(enemy.NetworkId()));
    }
    (void)intended;
    return FirstCollision(origin, endpoint, candidates);
}

inline bool CastQ(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || ProtectedTarget(target) || !Ready(0, mode) || !Throttle(0) ||
        PreserveWindup(reactive, Lethal(target, QDamage(target))) ||
        player.Position().Distance2D(target.Position()) > kQRange + target.BoundingRadius()) return false;
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vec3 aim = prediction.GetCastPosition().IsValid() &&
        !prediction.GetCastPosition().IsZero() ? prediction.GetCastPosition() : PredictPosition(target, kQDelay);
    if (!aim.IsValid() || aim.IsZero() ||
        prediction.GetCastPosition().IsValid() && prediction.Hitchance < SDK::HitChance::High ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kQWidth * 0.5f)) return false;
    const auto first = FirstQCollision(player.Position(), aim, target);
    if (!QCollisionOwned(first, static_cast<int>(target.NetworkId())) && !reactive &&
        !Lethal(target, QDamage(target))) return false;
    if (!Engine::ControllerCastPosition(0, aim)) return false;
    LastCastTick[0] = Now();
    QMarkedTargetId = static_cast<int>(target.NetworkId());
    QMarkExpireTick = Now() + 3500;
    return true;
}

inline bool ToggleW(Mode mode, bool forceOff = false, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1, 80)) return false;
    if (!forceOff && PreserveWindup(reactive)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastCastTick[1] = Now();
    WCastTick = Now();
    WActive = !WActive;
    WStopTick = WActive ? Now() + Slider(WMenu, "MaxActiveMs", 1800) : 0;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    const bool inRange = Engine::ValidEnemy(target, kWRange + 70.0f);
    const bool markedTarget = Marked(target);
    const bool passiveReady = AnyLegReady(LegReadyTick, Now());
    const bool farmValue = mode == Mode::LaneClear || mode == Mode::Jungle;
    const bool use = ShouldUseW(WActive, inRange, markedTarget, reactive,
                                player.IsValid() && player.HealthPercent() <= 45.0f,
                                farmValue || passiveReady);
    if (WActive) {
        if (!use || (WStopTick > 0 && Now() >= WStopTick)) return ToggleW(mode, true, reactive);
        WTargetId = Engine::ValidEnemy(target) ? static_cast<int>(target.NetworkId()) : WTargetId;
        return true;
    }
    if (!use || !Ready(1, mode) || !Throttle(1, 80)) return false;
    if (Engine::ControllerCastSelf(1)) {
        LastCastTick[1] = Now(); WCastTick = Now(); WActive = true;
        WTargetId = Engine::ValidEnemy(target) ? static_cast<int>(target.NetworkId()) : 0;
        WStopTick = Now() + Slider(WMenu, "MaxActiveMs", 1800);
        return true;
    }
    return false;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PlayerMobilityLocked() || ProtectedTarget(target) || !Ready(2, mode) || !Throttle(2) ||
        (Engine::CountEnemiesAt(player.Position(), 650.0f) > Slider(EMenu, "MaxCommitEnemies", 2) && !reactive)) return false;
    const auto prediction = Engine::RuntimeSpells[2]->GetPrediction(target);
    const Vec3 aim = prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero()
        ? prediction.GetCastPosition() : PredictPosition(target, kEDelay);
    const Vec3 endpoint = ClampDashEndpoint(player.Position(), aim);
    if (!endpoint.IsValid() || endpoint.IsZero() || TerrainBlocked(player.Position(), endpoint) ||
        SDK::NavMesh::IsWall(endpoint) || player.Position().Distance2D(endpoint) < 80.0f) return false;
    std::array<CollisionResult, 16> candidates{};
    int count = 0;
    auto append = [&](const Vec3& position, float radius, int id) {
        if (count < static_cast<int>(candidates.size())) candidates[count++] =
            {SegmentHits(player.Position(), endpoint, position, kEWidth * 0.5f, radius), id, position};
    };
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (minion.IsValid() && !minion.IsDead() && minion.IsTargetable())
            append(PredictPosition(minion, kEDelay), minion.BoundingRadius(), static_cast<int>(minion.NetworkId()));
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy, kERange + 100.0f))
            append(PredictPosition(enemy, kEDelay), enemy.BoundingRadius(), static_cast<int>(enemy.NetworkId()));
    }
    const auto first = FirstCollision(player.Position(), endpoint, candidates);
    if (!QCollisionOwned(first, static_cast<int>(target.NetworkId()))) return false;
    const bool lethal = Lethal(target, EDamage(target));
    const bool defensive = reactive || player.HealthPercent() <= Slider(EMenu, "ShieldBelowHP", 48);
    if (!DashEndpointSafe(player.Position(), endpoint, true, true,
        Engine::UnderEnemyTurret(endpoint), lethal, defensive,
        Engine::CountEnemiesAt(endpoint, 450.0f), Slider(EMenu, "MaxEndpointEnemies", 2))) return false;
    if (!Engine::ControllerCastPosition(2, endpoint)) return false;
    LastCastTick[2] = Now(); ETargetId = static_cast<int>(target.NetworkId());
    EStunExpireTick = Now() + kEStunDurationMs;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || ProtectedTarget(target) || !Ready(3, mode) || !Throttle(3, 120)) return false;
    const int id = static_cast<int>(target.NetworkId());
    if (RHooked && RTargetId == id && RRecastReady) {
        if (!RCanRecast(true, true, target.HealthPercent(),
                        static_cast<float>(Slider(RMenu, "ExecuteHP", 25)))) return false;
        if (Engine::ControllerCastSelf(3)) { LastCastTick[3] = Now(); RChanneling = true; return true; }
        return false;
    }
    if (PreserveWindup(reactive, Lethal(target, RDamage(target)))) return false;
    const float damage = RDamage(target);
    const bool lethal = Lethal(target, damage);
    const bool initialGate = RInitialCastAllowed(target.HealthPercent(), damage, target.Health(),
        target.AllShield(), static_cast<float>(Slider(RMenu, "InitialHP", 35)));
    if (!ShouldCastR({true, true, true, initialGate, false, lethal}) && !reactive) return false;
    const auto prediction = Engine::RuntimeSpells[3]->GetPrediction(target);
    const Vec3 aim = prediction.GetCastPosition().IsValid() && !prediction.GetCastPosition().IsZero()
        ? prediction.GetCastPosition() : PredictPosition(target, kRDelay);
    if (!aim.IsValid() || aim.IsZero() ||
        prediction.GetCastPosition().IsValid() && prediction.Hitchance < SDK::HitChance::High) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    LastCastTick[3] = Now(); RCastTick = Now(); RExpireTick = Now() + kRRecastWindowMs;
    RTargetId = id; RHooked = false; RRecastReady = false; RChanneling = false;
    return true;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (RHooked && RRecastReady && RCanRecast(true, true, target.HealthPercent())) return CastR(target, mode, true);
    if (Lethal(target, EDamage(target)) && CastE(target, mode)) return true;
    return Lethal(target, RDamage(target)) && CastR(target, mode, true);
}

inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (!Marked(target) && CastQ(target, Mode::Combo)) return;
    if (CastE(target, Mode::Combo)) return;
    if (CastW(target, Mode::Combo)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || !ResourceReady(Mode::Harass)) return;
    if (CastQ(target, Mode::Harass)) return;
    (void)CastW(target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) && CastE(target, Mode::Flee, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastW(target, Mode::Flee, true);
}

inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    WActive = player.HasBuff("UrgotW") || (WActive && now - WCastTick < 600);
    if (QMarkExpireTick <= now) QMarkedTargetId = QMarkExpireTick = 0;
    if (RExpireTick > 0 && now > RExpireTick && !RChanneling) {
        RHooked = RRecastReady = false; RTargetId = RExpireTick = 0;
    }
    bool observedHook = false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        const int id = static_cast<int>(enemy.NetworkId());
        if (enemy.HasBuff("UrgotR") || enemy.HasBuff("UrgotRFear")) {
            observedHook = true; RTargetId = id; RHooked = true;
        }
        if (enemy.HasBuff("UrgotRRecast") || enemy.HasBuff("UrgotRGrab")) {
            observedHook = true; RTargetId = id; RHooked = RRecastReady = true;
        }
        if (enemy.HasBuff("UrgotQSlow") || enemy.HasBuff("UrgotWTarget")) {
            QMarkedTargetId = id; QMarkExpireTick = now + 3500;
        }
    }
    if (observedHook) RExpireTick = now + kRRecastWindowMs;
    for (int i = 0; i < kLegCount; ++i) {
        const char* active = i == 0 ? "UrgotPassiveZone1Active" : i == 1 ? "UrgotPassiveZone2Active" :
            i == 2 ? "UrgotPassiveZone3Active" : i == 3 ? "UrgotPassiveZone4Active" :
            i == 4 ? "UrgotPassiveZone5Active" : "UrgotPassiveZone6Active";
        if (player.HasBuff(active)) LegReadyTick[i] = now + 1000;
    }
    for (auto& tick : LegReadyTick) if (tick < now - 60000) tick = now;
}

inline bool OnUpdate(Mode mode, const AIHeroClient&) {
    ReconcileState();
    const auto target = Engine::SelectTarget(
        mode == Mode::Flee ? 1000.0f : kRRange + 100.0f);
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target)) {
        if (CastE(target, mode, true)) return true;
        if (CastW(target, mode, true)) return true;
    }
    if (mode != Mode::LaneClear && mode != Mode::Jungle && mode != Mode::LastHit &&
        TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
        if (ResourceReady(mode) && Engine::CountEnemiesAt(GameObjects::Player().Position(), kWRange) == 0)
            (void)CastW(target, mode);
        if (!WActive) (void)Engine::TryFarm(mode);
        break;
    case Mode::LastHit:
        if (ResourceReady(mode) && Engine::CountEnemiesAt(GameObjects::Player().Position(), 850.0f) == 0)
            (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(target)) (void)CastE(target, mode, true);
        else if (Engine::ValidEnemy(target) && RHooked && RRecastReady) (void)CastR(target, mode, true);
        break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot < 0 || slot > 3) return;
        LastCastTick[slot] = now;
        if (slot == 1) { if (!Engine::WasControllerCast(slot)) WActive = !WActive; WCastTick = now; }
        if (slot == 3) { RCastTick = now; RExpireTick = now + kRRecastWindowMs; }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl) IncomingHardCCUntil = std::max(IncomingHardCCUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int id = args.Sender.IsValid() ? static_cast<int>(args.Sender.NetworkId) : 0;
    if (Engine::TextContains(args.BuffName, "UrgotW")) WActive = true;
    if (Engine::TextContains(args.BuffName, "UrgotQ") || Engine::TextContains(args.BuffName, "UrgotWTarget")) {
        QMarkedTargetId = id; QMarkExpireTick = Now() + 3500;
    }
    if (Engine::TextContains(args.BuffName, "UrgotR") || Engine::TextContains(args.BuffName, "UrgotRFear")) {
        RTargetId = id; RHooked = true; RRecastReady = Engine::TextContains(args.BuffName, "Recast");
        RExpireTick = Now() + kRRecastWindowMs;
    }
    if (Engine::TextContains(args.BuffName, "UrgotEStun")) EStunExpireTick = Now() + kEStunDurationMs;
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "UrgotW")) WActive = false;
    if (Engine::TextContains(args.BuffName, "UrgotR") || Engine::TextContains(args.BuffName, "UrgotRFear")) {
        RHooked = RRecastReady = RChanneling = false; RTargetId = RExpireTick = 0;
    }
    if (Engine::TextContains(args.BuffName, "UrgotQ") || Engine::TextContains(args.BuffName, "UrgotWTarget"))
        QMarkedTargetId = QMarkExpireTick = 0;
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid()) LastAutoTargetId = static_cast<int>(args.Target.NetworkId());
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    const int leg = DirectionalLeg(player.Position(), args.Target.Position(), 0, now);
    if (leg >= 0 && leg < kLegCount) LegReadyTick[leg] = now + 7000;
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
}
inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId != 0 ? args.TargetNetworkId : args.Target.NetworkId);
        LastAutoTick = Now();
    }
}
inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)args;
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 650);
}
inline void OnInterruptable(const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    (void)args;
    IncomingHardCCUntil = std::max(IncomingHardCCUntil, Now() + 700);
}
inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQRange, 0xFFDC6633u, 1.0f, 48);
    Drawing::DrawCircle(player.Position(), kERange, 0xFFAA44DDu, 1.0f, 36);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("UrgotOneTrick", "Urgot shotgun tactics"));
    TacticsMenu->Add(new MenuBool("PreserveAA", "Preserve passive leg windup", true));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Corrosive Charge"));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Purge"));
    WMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 45, 10, 90));
    WMenu->Add(new MenuSlider("MaxActiveMs", "Maximum automatic W time (ms)", 1800, 300, 5000));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Disdain"));
    EMenu->Add(new MenuSlider("MaxCommitEnemies", "Maximum enemies at origin", 2, 1, 5));
    EMenu->Add(new MenuSlider("MaxEndpointEnemies", "Maximum enemies at endpoint", 2, 1, 5));
    EMenu->Add(new MenuSlider("ShieldBelowHP", "Use E defensively below HP", 48, 15, 80));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Fear Beyond Death"));
    RMenu->Add(new MenuSlider("InitialHP", "Initial R target HP percent", 35, 15, 70));
    RMenu->Add(new MenuSlider("ExecuteHP", "R recast execute HP percent", 25, 10, 30));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("UrgotFarm", "Leg-aware farming"));
    FarmMenu->Add(new MenuSlider("FarmMana", "Farm mana percent", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("UrgotCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw Q and E ranges", false));
}

inline void OnLoad() {
    LegReadyTick.fill(0); LastCastTick.fill(0); LastAutoTargetId = LastAutoTick = 0;
    QMarkedTargetId = QMarkExpireTick = WTargetId = WCastTick = WStopTick = 0;
    WActive = false; ETargetId = EStunExpireTick = 0; RTargetId = RCastTick = RExpireTick = 0;
    IncomingThreatUntil = IncomingHardCCUntil = 0;
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    OnLoad();
}

inline constexpr const char* Scenarios[] = {
    "Pin Corrosive Charge, Purge, Disdain and Fear Beyond Death to Riot 26.15 / CommunityDragon 16.15",
    "Track six passive shotgun legs and poll zone buffs without inventing a ready leg",
    "Start each leg cooldown from observed attacks and preserve a valuable passive window",
    "Use the autonomous Engine target for mark, pressure and execute decisions",
    "Treat Purge as a toggle with explicit target lock, range, farm value and stop gates",
    "Do not cast over an ordinary AA windup unless reactive or lethal",
    "Use Disdain's real 475 range and shield value for engage or threat peel",
    "Reject Disdain through sampled terrain, wall endpoints, unsafe turrets or excess enemies",
    "Require the intended champion to be the first predicted Disdain collision",
    "Track Disdain stun and displacement from events plus polling",
    "Open Fear Beyond Death only through target-health and initial-damage gates",
    "Reject Disdain while Urgot is grounded, stunned or otherwise mobility-locked",
    "Require the R recast hook, recast readiness and 25 percent execute threshold",
    "Reconcile R projectile, hook, fear and channel states from buff polling",
    "Reject invulnerable, immune, spell-shielded or untargetable targets",
    "React to incoming hard crowd control with shielded Disdain before ordinary combo work",
    "Combo prioritizes Q mark, safe E flip, marked W pressure and R execute",
    "Harass spends Q and short W only above the configured mana reserve",
    "LaneClear uses W only for meaningful nearby waves and otherwise shared farm policy",
    "Jungle uses W on a valuable camp but never spends E for routine clear",
    "LastHit uses shared health-predicted farm policy and never R",
    "Flee uses reactive Disdain peel and avoids an unsolicited R channel",
    "Automatic mode is defense/execute only and never starts a fresh engage",
    "Do not automate movement, items or summoner spells",
    "Draw range coaching without changing cast decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Urgot;
    controller.ControllerId = "champion.kuroaio.ai.urgot.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIUrgot.md";
    controller.ImplementationSummary =
        "Leg cooldown polling, collision-safe Q/E, Purge toggle ownership and health-gated Fear Beyond Death recast.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Urgot
