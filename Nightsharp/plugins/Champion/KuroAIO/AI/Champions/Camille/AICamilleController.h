#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AICamilleGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Camille {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;

enum class PassiveShieldType : std::uint8_t { None, Physical, Magical };

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;
inline PassiveShieldType ShieldType = PassiveShieldType::None;
inline QStage CurrentQStage = QStage::Idle;
inline Mode LastKnownMode = Mode::None;
inline int PassiveReadyAtTick = 0;
inline int PassiveShieldExpireTick = 0;
inline int QPrimeTick = 0;
inline int QRecastExpireTick = 0;
inline int WallHangExpireTick = 0;
inline int ArenaExpireTick = 0;
inline int ArenaTargetId = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int IncomingHardCCUntil = 0;
inline int PlayerOverrideUntil = 0;
inline std::array<int, 4> CastTicks = {};
inline bool PassiveReady = true;
inline bool QFullyPrimed = false;
inline bool WallAttached = false;
inline bool ArenaActive = false;
inline Vector3 WallAnchor = {};
inline Vector3 PlannedLanding = {};
inline Vector3 ArenaCenter = {};

// Hookshot planning fans out over NavMesh wall rays.  The update path can ask
// for the same plan more than once (combo/harass/flee checks), so retain one
// movement-aware result briefly and avoid rebuilding all rays every tick.
inline int HookshotPlanCacheTick = 0;
inline int HookshotPlanCacheTargetId = 0;
inline bool HookshotPlanCacheFleeing = false;
inline bool HookshotPlanCacheLethal = false;
inline Vector3 HookshotPlanCacheSource = {};
inline Vector3 HookshotPlanCacheRequested = {};
inline Vector3 HookshotPlanCacheCursor = {};
inline WallCandidate HookshotPlanCache = {};

using ControllerHelpers::Now;
inline int SpellRank(int index) {
    if (index < 0 || index >= 4 || !Engine::RuntimeSpells[index]) return 1;
    return std::clamp(Engine::RuntimeSpells[index]->Level(), 1, index == 3 ? 3 : 5);
}
using ControllerHelpers::Ready;
inline bool Throttle(int index, int milliseconds) {
    return index >= 0 && index < 4 && Now() - CastTicks[index] >= milliseconds;
}
inline bool HasResource(int index, float reserve = 0.0f) {
    if (index == 0 && CurrentQStage == QStage::SecondWindow) return true;
    return CurrentResource() >= SpellCost(index) + std::max(0.0f, reserve);
}
inline bool TargetCannotBeDamaged(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           target.HasBuff("SivirE") || target.HasBuff("NocturneShroudofDarkness") ||
           target.HasBuff("MorganaE") || target.HasBuff("BlackShield") ||
           target.HasBuff("BansheesVeil") || target.HasBuff("EdgeOfNight") ||
           target.HasBuff("VladimirSanguinePool") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("KayleR") || target.HasBuff("kindredrnodeathbuff");
}
inline bool InAttackRange(const AIHeroClient& target, float allowance = 0.0f) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target) &&
           player.Position().Distance2D(target.Position()) <= player.AttackRange() +
               player.BoundingRadius() + target.BoundingRadius() + allowance;
}
inline float Q1Damage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(target,
        Q1BonusRawDamage(SpellRank(0), player.TotalAttackDamage()));
}
inline float Q2Damage(const AIHeroClient& target, bool primed) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const auto split = Q2BonusDamageSplit(
        SpellRank(0), player.Level(), player.TotalAttackDamage(), primed);
    return player.CalculatePhysicalDamage(target, split.Physical) + split.True;
}
inline float WDamage(const AIHeroClient& target, bool outer = true) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(target, WRawDamage(
        SpellRank(1), player.BonusAttackDamage(), target.MaxHealth(), outer));
}
inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(target,
        ERawDamage(SpellRank(2), player.BonusAttackDamage()));
}
inline float ROnHitDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculateMagicDamage(target,
        ROnHitRawDamage(SpellRank(3), target.Health()));
}
inline float AutoDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? SDK::Damage::GetAutoAttackDamage(player, target, true) : 0.0f;
}
using ControllerHelpers::Lethal;
inline bool PassiveShieldActive() {
    return ShieldType != PassiveShieldType::None && PassiveShieldExpireTick > Now();
}
inline void ResetQState() {
    CurrentQStage = QStage::Idle;
    QPrimeTick = QRecastExpireTick = 0;
    QFullyPrimed = false;
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    const bool physical = player.HasBuff("CamillePassiveShieldPhysical");
    const bool magical = player.HasBuff("CamillePassiveShieldMagic");
    if (physical || magical) {
        ShieldType = physical ? PassiveShieldType::Physical : PassiveShieldType::Magical;
        PassiveReady = false;
        PassiveShieldExpireTick = std::max(PassiveShieldExpireTick, now + 180);
    } else if (PassiveShieldExpireTick <= now) ShieldType = PassiveShieldType::None;
    if (player.HasBuff("CamillePassiveReady")) PassiveReady = true;
    if (player.HasBuff("CamillePassiveCooldown")) PassiveReady = false;
    if (!PassiveReady && PassiveReadyAtTick > 0 && now >= PassiveReadyAtTick &&
        !player.HasBuff("CamillePassiveCooldown")) PassiveReady = true;
    if (player.HasBuff("CamilleQPrimingComplete")) {
        CurrentQStage = QStage::SecondWindow;
        QFullyPrimed = true;
        if (QRecastExpireTick == 0) QRecastExpireTick = now + 1800;
    } else if (player.HasBuff("CamilleQPrimingStart")) {
        CurrentQStage = QStage::SecondWindow;
        QFullyPrimed = QPrimeTick > 0 && now >= QPrimeTick;
    } else if (player.HasBuff("CamilleQ") && CurrentQStage == QStage::Idle) {
        CurrentQStage = QStage::FirstAttackArmed;
    }
    if (CurrentQStage == QStage::SecondWindow) {
        if (QPrimeTick > 0 && now >= QPrimeTick) QFullyPrimed = true;
        if (QRecastExpireTick > 0 && now >= QRecastExpireTick) ResetQState();
    }
    WallAttached = player.HasBuff("CamilleEOnWall") ||
                   (WallAttached && now < WallHangExpireTick);
    if (WallHangExpireTick > 0 && now >= WallHangExpireTick) {
        WallAttached = false;
        WallHangExpireTick = 0;
    }
    if (player.HasBuff("CamilleR") || player.HasBuff("CamilleRTether")) {
        ArenaActive = true;
        ArenaExpireTick = std::max(ArenaExpireTick, now + 180);
    } else if (ArenaExpireTick > 0 && now >= ArenaExpireTick) {
        ArenaActive = false;
        ArenaTargetId = 0;
        ArenaCenter = {};
    }
}

inline bool CastQReset(const AIHeroClient& target, Mode mode, bool afterAttack) {
    if (!Engine::ValidEnemy(target) || TargetCannotBeDamaged(target) ||
        !Ready(0, mode) || !Throttle(0, 45) || !HasResource(0) ||
        !InAttackRange(target, kQBonusAttackRange + 25.0f)) return false;
    const bool second = CurrentQStage == QStage::SecondWindow;
    QResetContext context{};
    context.Stage = CurrentQStage;
    context.AfterAttack = afterAttack;
    context.FullyPrimed = QFullyPrimed;
    context.RecastExpiring = second && QRecastExpireTick > 0 &&
                             QRecastExpireTick - Now() <= 350;
    context.LethalWithoutPrime = Lethal(target,
        (second ? Q2Damage(target, false) : Q1Damage(target)) + AutoDamage(target));
    if (!MayActivateQ(context) || !Engine::ControllerCastSelf(0)) return false;
    CastTicks[0] = Now();
    CurrentQStage = second ? QStage::SecondAttackArmed : QStage::FirstAttackArmed;
    return true;
}

inline bool CastW(const AIHeroClient& target, Mode mode, bool defensive = false) {
    if (!Engine::ValidEnemy(target, kWRange + 45.0f) || TargetCannotBeDamaged(target) ||
        !Ready(1, mode) || !Throttle(1, 90) ||
        !HasResource(1, mode == Mode::Harass ? static_cast<float>(Slider(WMenu, "HarassReserve", 85)) : 0.0f)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(target);
    const Vector3 predicted = prediction.GetCastPosition().IsValid() &&
        !prediction.GetCastPosition().IsZero() ? prediction.GetCastPosition() :
        PredictPosition(target, kWDelay);
    if (!predicted.IsValid() || predicted.IsZero() ||
        !WContains(player.Position(), predicted, predicted, target.BoundingRadius(), true) ||
        prediction.Hitchance < SDK::HitChance::High) return false;
    if (Orbwalker::IsWindingUp() && !defensive && !Lethal(target, WDamage(target))) return false;
    if (!Engine::ControllerCastPosition(1, predicted)) return false;
    CastTicks[1] = Now();
    return true;
}

inline void AddDirection(std::vector<Vector3>& directions, const Vector3& direction) {
    const Vector3 normalized = SharedGeometry::Direction2D({}, direction);
    if (normalized.IsZero()) return;
    for (const auto& existing : directions) if (existing.Dot(normalized) > 0.995f) return;
    directions.push_back(normalized);
}
inline WallCandidate BuildHookshotPlan(const AIHeroClient& target, bool fleeing, bool lethal) {
    WallCandidate none{};
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return none;
    const Vector3 source = player.Position();
    const Vector3 requested = fleeing ? Game::CursorPos() :
        (Engine::ValidEnemy(target) ? PredictPosition(target, 0.75f) : Vector3{});
    if (!requested.IsValid() || requested.IsZero()) return none;
    const Vector3 cursor = Game::CursorPos();
    const int now = Now();
    const int targetId = Engine::ValidEnemy(target)
        ? static_cast<int>(target.NetworkId()) : 0;
    if (HookshotPlanCacheTick > 0 && now >= HookshotPlanCacheTick &&
        now - HookshotPlanCacheTick <= 260 &&
        HookshotPlanCacheTargetId == targetId &&
        HookshotPlanCacheFleeing == fleeing &&
        HookshotPlanCacheLethal == lethal &&
        source.Distance2D(HookshotPlanCacheSource) <= 28.0f &&
        requested.Distance2D(HookshotPlanCacheRequested) <= 36.0f &&
        cursor.Distance2D(HookshotPlanCacheCursor) <= 36.0f) {
        return HookshotPlanCache;
    }
    const bool sourceUnderTurret = Engine::UnderEnemyTurret(source);
    std::vector<Vector3> directions;
    directions.reserve(24);
    AddDirection(directions, requested - source);
    const Vector3 toward = SharedGeometry::Direction2D(source, requested);
    if (!toward.IsZero()) {
        AddDirection(directions, SharedGeometry::Rotate2D(toward, 0.35f));
        AddDirection(directions, SharedGeometry::Rotate2D(toward, -0.35f));
        AddDirection(directions, SharedGeometry::Rotate2D(toward, 0.70f));
        AddDirection(directions, SharedGeometry::Rotate2D(toward, -0.70f));
    }
    // Sixteen angular rays retain the requested/side bias above while
    // avoiding 32 additional native FindWallCollision calls for every plan.
    constexpr int kRadialDirections = 16;
    for (int i = 0; i < kRadialDirections; ++i) {
        const float angle = 2.0f * SharedGeometry::kPi * static_cast<float>(i) /
            static_cast<float>(kRadialDirections);
        AddDirection(directions, Vector3{ std::cos(angle), 0.0f, std::sin(angle) });
    }
    std::vector<WallCandidate> candidates;
    candidates.reserve(directions.size());
    for (const auto& direction : directions) {
        Vector3 anchor{};
        if (!SDK::NavMesh::FindWallCollision(source, source + direction * kEWallRange,
                anchor, 8.0f) || !anchor.IsValid() || anchor.IsZero()) continue;
        const float anchorDistance = source.Distance2D(anchor);
        if (anchorDistance < 90.0f || anchorDistance > kEWallRange + 20.0f) continue;
        anchor.y = SDK::NavMesh::GetHeightForPosition(anchor);
        Vector3 landing = ClampHookshotLanding(anchor, requested, !fleeing);
        if (!landing.IsValid() || landing.IsZero()) continue;
        landing.y = SDK::NavMesh::GetHeightForPosition(landing);
        HookshotContext safety{};
        safety.AnchorValid = safety.LandingValid = true;
        safety.LandingWalkable = !SDK::NavMesh::IsWall(landing);
        safety.NewTurretDive = Engine::UnderEnemyTurret(landing) && !sourceUnderTurret;
        safety.PointClickThreat = HasReadyPointClickThreatAt(landing);
        safety.DashHazard = HasReadyDashHazardAt(landing);
        safety.TargetReachable = fleeing || (Engine::ValidEnemy(target) &&
            anchor.Distance2D(requested) <= kE2ChampionRange + target.BoundingRadius());
        safety.Lethal = lethal;
        safety.Fleeing = fleeing;
        safety.NearbyEnemies = Engine::CountEnemiesAt(landing, 575.0f);
        safety.MaximumEnemies = Slider(EMenu, "MaxLandingEnemies", 2);
        WallCandidate candidate{};
        candidate.Anchor = anchor;
        candidate.Landing = landing;
        candidate.Safety = safety;
        candidate.TargetDistance = Engine::ValidEnemy(target) ? landing.Distance2D(requested) : 0.0f;
        candidate.CursorDistance = landing.Distance2D(cursor);
        candidates.push_back(candidate);
    }
    const WallCandidate result = SelectWallCandidate(candidates, fleeing);
    HookshotPlanCacheTick = now;
    HookshotPlanCacheTargetId = targetId;
    HookshotPlanCacheFleeing = fleeing;
    HookshotPlanCacheLethal = lethal;
    HookshotPlanCacheSource = source;
    HookshotPlanCacheRequested = requested;
    HookshotPlanCacheCursor = cursor;
    HookshotPlanCache = result;
    return result;
}

inline bool CastE(const AIHeroClient& target, Mode mode, bool fleeing = false) {
    if (!Ready(2, mode) || !Throttle(2, 85)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (WallAttached) {
        const Vector3 requested = fleeing ? Game::CursorPos() : PredictPosition(target, 0.18f);
        if (!requested.IsValid() || requested.IsZero()) return false;
        const Vector3 landing = ClampHookshotLanding(
            WallAnchor.IsZero() ? player.Position() : WallAnchor, requested,
            !fleeing && Engine::ValidEnemy(target));
        if (!landing.IsValid() || landing.IsZero() || SDK::NavMesh::IsWall(landing) ||
            (Engine::UnderEnemyTurret(landing) && !Engine::UnderEnemyTurret(player.Position()) &&
             !(Engine::ValidEnemy(target) && Lethal(target, EDamage(target) + AutoDamage(target))))) return false;
        if (!Engine::ControllerCastPosition(2, requested)) return false;
        CastTicks[2] = Now();
        WallAttached = false;
        PlannedLanding = landing;
        return true;
    }
    if (!HasResource(2, mode == Mode::Harass ? static_cast<float>(Slider(EMenu, "HarassReserve", 110)) : 0.0f) ||
        (!fleeing && TargetCannotBeDamaged(target))) return false;
    const bool lethal = !fleeing && Lethal(target,
        EDamage(target) + Q1Damage(target) + AutoDamage(target));
    const WallCandidate plan = BuildHookshotPlan(target, fleeing, lethal);
    if (plan.Anchor.IsZero() || !Engine::ControllerCastPosition(2, plan.Anchor)) return false;
    CastTicks[2] = Now();
    WallAnchor = plan.Anchor;
    PlannedLanding = plan.Landing;
    WallAttached = true;
    WallHangExpireTick = Now() + 1100;
    return true;
}

inline bool CastR(const AIHeroClient& target, Mode mode, bool defensive = false) {
    if (!Engine::ValidEnemy(target, kRRange + 35.0f) || TargetCannotBeDamaged(target) ||
        !Ready(3, mode) || !Throttle(3, 140) || !HasResource(3)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const float followup = AutoDamage(target) * 2.0f + Q1Damage(target) +
        Q2Damage(target, true) + ROnHitDamage(target) * 2.0f;
    const bool lethal = Lethal(target, followup);
    const int enemies = Engine::CountEnemiesAt(target.Position(), kRArenaRadius + 225.0f);
    ArenaContext context{};
    context.TargetValid = context.TargetDamageable = true;
    context.TargetInRange = player.Position().Distance2D(target.Position()) <=
                            kRRange + target.BoundingRadius();
    context.NewTurretDive = Engine::UnderEnemyTurret(target.Position()) &&
                            !Engine::UnderEnemyTurret(player.Position());
    context.IncomingHardCrowdControl = defensive || IncomingHardCCUntil > Now();
    context.TargetIsolated = enemies <= 1;
    context.LethalFollowup = lethal;
    context.AllySupport = Engine::CountAlliesAt(target.Position(), 700.0f) > 0;
    context.EnemiesNearArena = enemies;
    context.MaximumEnemies = Slider(RMenu, "MaxArenaEnemies", 2);
    if (!ArenaSafe(context) || (!defensive && target.HealthPercent() >
        Slider(RMenu, "TargetHP", 48) && !lethal && !context.TargetIsolated)) return false;
    if (!Engine::ControllerCastUnit(3, target)) return false;
    CastTicks[3] = Now();
    ArenaActive = true;
    ArenaTargetId = static_cast<int>(target.NetworkId());
    ArenaCenter = target.Position();
    ArenaExpireTick = Now() + static_cast<int>(RDurationSeconds(SpellRank(3)) * 1000.0f);
    return true;
}

inline bool TryKillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, WDamage(target)) && CastW(target, mode)) return true;
    return WallAttached && Lethal(target, EDamage(target)) && CastE(target, mode);
}
inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    if (IncomingHardCCUntil > Now() && CastR(target, Mode::Combo, true)) return true;
    if (distance > kWOuterMinimumRange && CastW(target, Mode::Combo)) return true;
    if (WallAttached && CastE(target, Mode::Combo)) return true;
    if (distance > player.AttackRange() + 120.0f && CastE(target, Mode::Combo)) return true;
    const float withoutR = EDamage(target) + WDamage(target) + Q1Damage(target) +
        Q2Damage(target, true) + AutoDamage(target) * 2.0f;
    return !Lethal(target, withoutR) && CastR(target, Mode::Combo);
}
inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const float distance = GameObjects::Player().Position().Distance2D(target.Position());
    if (distance >= kWOuterMinimumRange && CastW(target, Mode::Harass)) return true;
    if (!PassiveReady && !PassiveShieldActive()) return false;
    return Bool(EMenu, "HarassHookshot", false) && distance > 500.0f &&
           CastE(target, Mode::Harass);
}
inline bool TryFarm(Mode mode) {
    return CurrentResource() >= Slider(FarmMenu, "ManaReserve", 90) && Engine::TryFarm(mode);
}
inline bool TryFlee(const AIHeroClient& threat) {
    if (CastE(threat, Mode::Flee, true)) return true;
    return Engine::ValidEnemy(threat) && CastW(threat, Mode::Flee, true);
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastKnownMode = mode;
    ReconcileState();
    if (PlayerOverrideUntil > Now()) return true;
    AIHeroClient target = selected;
    if (!Engine::ValidEnemy(target)) target = Engine::SelectTarget(1550.0f);
    const AIHeroClient threat = NearestEnemyToPlayer(target, 1100.0f);
    if (mode == Mode::Flee) { (void)TryFlee(threat); return true; }
    if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(threat, kRRange + 35.0f) &&
        CastR(threat, mode == Mode::Automatic ? Mode::Automatic : Mode::Combo, true)) return true;
    if (TryKillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: (void)TryCombo(target); break;
    case Mode::Harass: (void)TryHarass(target); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit: (void)TryFarm(mode); break;
    case Mode::Automatic:
        if (Engine::ValidEnemy(target) && AutomaticAllowed({ IncomingHardCCUntil > Now(),
                Lethal(target, WDamage(target)), false }))
            (void)TryKillSecure(target, Mode::Automatic);
        break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!GameObjects::Player().IsValid() || !args.Sender.IsValid()) return;
    const int now = Now();
    if (!IsLocalPlayer(args.Sender)) {
        const auto threat = ControllerHelpers::AnalyzeEnemyCast(
            args, 220.0f, 115.0f, 250, 280, 260, 1500, 450);
        if (threat.Valid && threat.CrossesPlayer && threat.LikelyHardCrowdControl)
            IncomingHardCCUntil = std::max(IncomingHardCCUntil, now + 700);
        return;
    }
    const int slot = args.Slot;
    if (slot < 0 || slot >= 4) return;
    const bool owned = Engine::WasControllerCast(slot);
    if (!owned) PlayerOverrideUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 560);
    CastTicks[slot] = now;
    if (slot == 0) {
        const bool second = CurrentQStage == QStage::SecondWindow ||
                            Engine::TextContains(args.SpellName, "CamilleQ2");
        CurrentQStage = second ? QStage::SecondAttackArmed : QStage::FirstAttackArmed;
    } else if (slot == 2) {
        if (Engine::TextContains(args.SpellName, "Dash2") ||
            Engine::TextContains(args.SpellName, "CamilleE2")) WallAttached = false;
        else { WallAttached = true; WallHangExpireTick = now + 1100; }
    } else if (slot == 3) {
        ArenaActive = true;
        ArenaExpireTick = now + static_cast<int>(RDurationSeconds(SpellRank(3)) * 1000.0f);
    }
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (Engine::TextContains(args.BuffName, "CamillePassiveShieldPhysical") ||
        Engine::TextContains(args.BuffName, "CamillePassiveShieldMagic")) {
        ShieldType = Engine::TextContains(args.BuffName, "Physical") ?
            PassiveShieldType::Physical : PassiveShieldType::Magical;
        PassiveReady = false;
        PassiveShieldExpireTick = now + ControllerHelpers::RemainingMilliseconds(
            args.EndTime, 2000, 100, 2600);
        PassiveReadyAtTick = now + static_cast<int>(
            PassiveCooldownSeconds(GameObjects::Player().Level()) * 1000.0f);
    } else if (Engine::TextContains(args.BuffName, "CamillePassiveReady")) PassiveReady = true;
    else if (Engine::TextContains(args.BuffName, "CamilleQPrimingComplete")) {
        CurrentQStage = QStage::SecondWindow; QFullyPrimed = true;
    } else if (Engine::TextContains(args.BuffName, "CamilleQPrimingStart")) {
        CurrentQStage = QStage::SecondWindow;
        QPrimeTick = now + kQPrimeMilliseconds;
        QRecastExpireTick = now + kQRecastMilliseconds;
        QFullyPrimed = false;
    } else if (Engine::TextContains(args.BuffName, "CamilleEOnWall")) {
        WallAttached = true;
        WallAnchor = GameObjects::Player().Position();
        WallHangExpireTick = now + ControllerHelpers::RemainingMilliseconds(args.EndTime, 1000, 100, 1400);
    } else if (Engine::TextContains(args.BuffName, "CamilleR") ||
               Engine::TextContains(args.BuffName, "CamilleRTether")) {
        ArenaActive = true;
        ArenaExpireTick = now + ControllerHelpers::RemainingMilliseconds(args.EndTime,
            static_cast<int>(RDurationSeconds(SpellRank(3)) * 1000.0f), 100, 4500);
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (Engine::TextContains(args.BuffName, "CamillePassiveShield")) {
        ShieldType = PassiveShieldType::None; PassiveShieldExpireTick = 0;
    } else if (Engine::TextContains(args.BuffName, "CamilleQPriming")) {
        if (CurrentQStage == QStage::SecondWindow) ResetQState();
    } else if (Engine::TextContains(args.BuffName, "CamilleEOnWall")) {
        WallAttached = false; WallHangExpireTick = 0;
    } else if (Engine::TextContains(args.BuffName, "CamilleR") ||
               Engine::TextContains(args.BuffName, "CamilleRTether")) {
        ArenaActive = false; ArenaTargetId = 0; ArenaCenter = {};
    }
}
inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (args.Target.IsValid() && PlayerOverrideUntil > Now()) args.Process = true;
}
inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick)) return;
    const AIHeroClient target = HeroByNetworkId(LastAutoTargetId);
    if (CurrentQStage == QStage::FirstAttackArmed) {
        CurrentQStage = QStage::SecondWindow;
        QPrimeTick = Now() + kQPrimeMilliseconds;
        QRecastExpireTick = Now() + kQRecastMilliseconds;
        QFullyPrimed = false;
        return;
    }
    if (CurrentQStage == QStage::SecondAttackArmed) { ResetQState(); return; }
    if ((LastKnownMode == Mode::Combo || LastKnownMode == Mode::Harass) &&
        Engine::ValidEnemy(target)) (void)CastQReset(target, LastKnownMode, true);
}
inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Bool(CoachMenu, "DrawRanges", false)) return;
    Drawing::DrawCircle(player.Position(), kWRange, 0x336CC8FFu, 1.2f, 56);
    Drawing::DrawCircle(player.Position(), kEWallRange, 0x3348E5A5u, 1.2f, 64);
    if (!WallAnchor.IsZero()) {
        Drawing::DrawLine(player.Position(), WallAnchor, 0xCC48E5A5u, 2.0f);
        Drawing::DrawCircle(WallAnchor, 45.0f, 0xCC48E5A5u, 1.8f, 24);
    }
    if (!PlannedLanding.IsZero())
        Drawing::DrawCircle(PlannedLanding, kECollisionRadius, 0xCCF1C75Bu, 1.8f, 32);
    if (ArenaActive && !ArenaCenter.IsZero())
        Drawing::DrawCircle(ArenaCenter, kRArenaRadius, 0xCCE879F9u, 2.0f, 64);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("CamilleOneTrick", "Camille precision diver"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 560, 180, 1200));
    QMenu = TacticsMenu->AddSubMenu(new Menu("CamilleQ", "Precision Protocol resets"));
    QMenu->Add(new MenuBool("WaitEmpoweredQ2", "Wait for empowered second Q", true));
    WMenu = TacticsMenu->AddSubMenu(new Menu("CamilleW", "Tactical Sweep outer cone"));
    WMenu->Add(new MenuSlider("HarassReserve", "Harass mana reserve", 85, 0, 220));
    EMenu = TacticsMenu->AddSubMenu(new Menu("CamilleE", "Hookshot endpoint safety"));
    EMenu->Add(new MenuSlider("MaxLandingEnemies", "Maximum enemies at landing", 2, 1, 5));
    EMenu->Add(new MenuSlider("HarassReserve", "Harass hookshot mana reserve", 110, 0, 260));
    EMenu->Add(new MenuBool("HarassHookshot", "Allow passive-backed harass hookshot", false));
    RMenu = TacticsMenu->AddSubMenu(new Menu("CamilleR", "Hextech Ultimatum arena"));
    RMenu->Add(new MenuSlider("TargetHP", "Ordinary R target HP threshold", 48, 10, 100));
    RMenu->Add(new MenuSlider("MaxArenaEnemies", "Maximum enemies near arena", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("CamilleFarm", "Conservative farm"));
    FarmMenu->Add(new MenuSlider("ManaReserve", "Mana reserve", 90, 0, 260));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("CamilleCoach", "Route visualization"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw W/E and active plans", false));
}
inline void OnLoad() {
    ShieldType = PassiveShieldType::None;
    CurrentQStage = QStage::Idle;
    LastKnownMode = Mode::None;
    PassiveReadyAtTick = PassiveShieldExpireTick = QPrimeTick = QRecastExpireTick = 0;
    WallHangExpireTick = ArenaExpireTick = ArenaTargetId = 0;
    LastAutoTargetId = LastAutoTick = IncomingHardCCUntil = PlayerOverrideUntil = 0;
    CastTicks.fill(0);
    PassiveReady = true;
    QFullyPrimed = WallAttached = ArenaActive = false;
    WallAnchor = PlannedLanding = ArenaCenter = {};
    HookshotPlanCacheTick = 0;
    HookshotPlanCacheTargetId = 0;
    HookshotPlanCacheFleeing = false;
    HookshotPlanCacheLethal = false;
    HookshotPlanCacheSource = HookshotPlanCacheRequested = HookshotPlanCacheCursor = {};
    HookshotPlanCache = {};
    ReconcileState();
}
inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    ResetQState();
    WallAttached = ArenaActive = false;
    HookshotPlanCacheTick = 0;
    HookshotPlanCacheTargetId = 0;
    HookshotPlanCacheFleeing = false;
    HookshotPlanCacheLethal = false;
    HookshotPlanCacheSource = HookshotPlanCacheRequested = HookshotPlanCacheCursor = {};
    HookshotPlanCache = {};
}

inline constexpr const char* Scenarios[] = {
    "Use Riot 26.15 and CommunityDragon 16.15 Summoner's Rift data",
    "Track Adaptive Defenses readiness, cooldown and physical or magical shield",
    "Use the twenty-percent maximum-health shield and level breakpoint cooldowns",
    "Track both Q reset arms, the 1.5-second prime and 3.5-second recast window",
    "Cast each Q immediately after an attack and preserve empowered Q2 true damage",
    "Predict Tactical Sweep and require the target in its outer cone",
    "Find a real NavMesh wall endpoint before committing Hookshot",
    "Require selected-target reachability and a walkable safe Wall Dive landing",
    "Use 800 E2 range toward champions and 400 range otherwise",
    "Reject new turret dives, lockdown and crowded offensive E landings",
    "Evaluate Hextech Ultimatum's 425-radius arena and follow-up damage",
    "Allow R untargetability only against observed hard crowd control or safe all-ins",
    "Cover Combo, Harass, LaneClear, Jungle, LastHit, Flee and Automatic modes",
    "Reconcile passive, Q, E and R state through buff polling and events",
    "Respect cooperative selected targeting before fallback selection",
    "Preserve cooldown, mana reserve, prediction, collision and manual ownership",
    "Never automate summoners or item actives",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Camille;
    controller.ControllerId = "champion.kuroaio.ai.camille.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AICamille.md";
    controller.ImplementationSummary =
        "Adaptive shield state, two-part Q reset timing, predicted outer W cone, "
        "real-wall Hookshot routing and arena-safe Hextech Ultimatum ownership.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardLocalActiveBuffEvent<&OnBuffAdd>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Camille
