#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIAmbessaGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Ambessa {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::AutoAttackRange;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NameEquals;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Ready;
using ControllerHelpers::RemainingMilliseconds;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

enum class Sequence : int {
    None,
    Q1EdgeTrade,
    QuickQE,
    EnergyWeave,
    WCounter,
    EDoubleStrike,
    Q2FirstTarget,
    RIsolation,
    RFollowup,
    EscapeChain,
    JungleWeave,
    PlayerOwnedNoDash,
};

enum class Posture : int {
    Neutral,
    Space,
    ShortTrade,
    AllIn,
    Chase,
    Kite,
    Isolate,
    Escape,
    Jungle,
};

enum class DashChoice : int {
    PlayerOwned,
    NoDash,
    TowardTarget,
    SideStep,
    Backward,
    Cursor,
    Escape,
};

enum class UltimateReason : int {
    None,
    Execute,
    IsolateCarry,
    Interrupt,
    LongCatch,
    UnstoppableDodge,
    EscapeReposition,
};

struct Q2CastPlan {
    Vector3 Aim = {};
    int SelectedFirstId = 0;
    int HitCount = 0;
    bool IntendedFirst = false;
    bool Valid = false;
};

struct UltimatePlan {
    Vector3 Aim = {};
    Vector3 Landing = {};
    int IntendedTargetId = 0;
    int SelectedTargetId = 0;
    int EnemiesAtLanding = 0;
    int AlliesAtLanding = 0;
    float Score = -FLT_MAX;
    bool IntendedSelected = false;
    bool Safe = false;
    bool Valid = false;
};

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Sequence ActiveSequence = Sequence::None;
inline Posture CurrentPosture = Posture::Neutral;
inline DashChoice PlannedDashChoice = DashChoice::PlayerOwned;
inline UltimateReason LastUltimateReason = UltimateReason::None;

inline int PassiveStacks = 0;
inline int PassiveExpireTick = 0;
inline int LastPassiveAttackTick = 0;
inline bool PassiveDashActive = false;
inline bool PassiveDashObserved = false;
inline int PassiveDashStartTick = 0;
inline int PassiveDashEndTick = 0;
inline int PendingDashSlot = -1;
inline int PendingDashWindowUntil = 0;
inline Vector3 PendingDashOrigin = {};
inline Vector3 PlannedDashEndpoint = {};
inline Vector3 LastObservedPosition = {};

inline bool Q2Ready = false;
inline int Q2ExpireTick = 0;
inline int QCastTick = 0;
inline int QCastStage = 0;
inline int QTargetId = 0;
inline Vector3 LastQDirection = {};
inline Q1Region LastQ1Region = Q1Region::Miss;
inline Q2CastPlan LastQ2Plan = {};

inline bool WBraceActive = false;
inline bool WShieldActive = false;
inline bool WDamageEmpowered = false;
inline int WCastTick = 0;
inline int WBraceEndTick = 0;
inline int WShieldExpireTick = 0;
inline int WTargetId = 0;

inline int ECastTick = 0;
inline int ESecondWindowUntil = 0;
inline bool ESecondStrikeExpected = false;
inline bool ESecondStrikeObserved = false;
inline int ETargetId = 0;

inline bool RActive = false;
inline int RCastTick = 0;
inline int RLockoutUntil = 0;
inline int RTargetId = 0;
inline UltimatePlan LastRPlan = {};

inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline int IncomingImpactTick = 0;
inline int CommittedEnemyId = 0;
inline float IncomingDamage = 0.0f;
inline bool IncomingFromTurret = false;

inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;

inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int WeaveTargetId = 0;
inline int WeaveWaitUntil = 0;
inline int LastDecisionTargetId = 0;

inline constexpr int kQRecastMs = 4000;
inline constexpr int kPassiveStackMs = 4000;
inline constexpr int kDashBufferMs = 275;
inline constexpr int kWDashBufferMs = 250;
inline constexpr int kELockoutMs = 225;
inline constexpr int kWBraceMs = 500;
inline constexpr int kWShieldMs = 1500;
inline constexpr int kRCastMs = 700;
inline constexpr int kRSuppressAndStunMs = 1250;

inline bool HasEnergy(float amount, float reserve = 0.0f) {
    return CurrentResource(200.0f) + 0.5f >=
        std::max(0.0f, amount) + std::max(0.0f, reserve);
}

inline bool Q2IsLive() {
    const auto player = GameObjects::Player();
    return (Q2Ready && Now() <= Q2ExpireTick) ||
           RuntimeNameContains(0, "AmbessaQ2") ||
           (player.IsValid() && player.HasBuff("AmbessaQEmpowerReady"));
}

inline float EnhancedAttackRange(const AIBaseClient& target) {
    return AutoAttackRange(
        target, PassiveStacks > 0 ? 75.0f : 0.0f);
}

inline bool InEnhancedAttackRange(const AIBaseClient& target,
                                  float padding = 0.0f) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid() &&
        player.Position().Distance2D(target.Position()) <=
            EnhancedAttackRange(target) + padding;
}

inline bool CursorConsentsTo(const Vector3& position,
                             float minimumDot = -0.10f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !position.IsValid()) return false;
    const Vector3 targetDirection = SharedGeometry::Direction2D(
        player.Position(), position);
    const Vector3 cursorDirection = SharedGeometry::Direction2D(
        player.Position(), Game::CursorPos());
    return !targetDirection.IsZero() && !cursorDirection.IsZero() &&
           targetDirection.Dot(cursorDirection) >= minimumDot;
}

inline Vector3 ClipPassiveDashToTerrain(const Vector3& origin,
                                        const Vector3& desired) {
    if (!origin.IsValid() || !desired.IsValid()) return origin;
    const Vector3 direction = SharedGeometry::Direction2D(origin, desired);
    const float distance = origin.Distance2D(desired);
    if (direction.IsZero() || distance < 1.0f) return origin;
    Vector3 last = origin;
    for (float travel = 18.0f; travel <= distance; travel += 18.0f) {
        Vector3 sample = origin + direction * travel;
        sample.y = origin.y;
        if (SDK::NavMesh::IsWall(sample)) return last;
        last = sample;
    }
    return desired;
}

inline bool DashEndpointSafe(const Vector3& endpoint,
                             const AIHeroClient& target,
                             bool allowTurret = false) {
    if (!endpoint.IsValid() || endpoint.IsZero() ||
        SDK::NavMesh::IsWall(endpoint)) {
        return false;
    }
    if (Bool(PassiveMenu, "RespectDashHazards", true) &&
        HasReadyDashHazardAt(endpoint)) {
        return false;
    }
    if (!allowTurret && Engine::UnderEnemyTurret(endpoint)) return false;
    const int maximum = Slider(
        PassiveMenu, "MaxDashEnemies", 2);
    if (Engine::CountEnemiesAt(endpoint, 525.0f) > maximum) return false;
    if (target.IsValid() &&
        Engine::PositionDangerScore(
            endpoint, target, Engine::ResolvedSpecs[2]) < -1200.0f) {
        return false;
    }
    return true;
}

inline Vector3 DirectionEndpoint(const Vector3& origin,
                                 const Vector3& desired) {
    return ClipPassiveDashToTerrain(
        origin, PassiveDashEndpoint(origin, desired));
}

inline DashChoice ChooseDashChoice(const AIHeroClient& target,
                                   Posture posture) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PlayerMobilityLocked()) {
        return DashChoice::NoDash;
    }
    const Vector3 cursorEndpoint = DirectionEndpoint(
        player.Position(), Game::CursorPos());
    if (!DashEndpointSafe(cursorEndpoint, target, false)) {
        return DashChoice::NoDash;
    }
    if (Bool(PassiveMenu, "BaitAntiDash", true) &&
        HasReadyDashHazardAt(player.Position())) {
        return DashChoice::NoDash;
    }
    if (posture == Posture::Escape) return DashChoice::Escape;
    if (posture == Posture::Kite) return DashChoice::Backward;
    if (!target.IsValid()) return DashChoice::Cursor;

    const float distance = player.Position().Distance2D(target.Position());
    if (posture == Posture::Chase && distance > 360.0f) {
        return DashChoice::TowardTarget;
    }
    if ((posture == Posture::ShortTrade || posture == Posture::Space) &&
        distance >= kQ1InnerRadius && distance <= kQ1OuterRadius + 45.0f) {
        return Bool(PassiveMenu, "KeepQSweetspot", true)
            ? DashChoice::NoDash
            : DashChoice::SideStep;
    }
    if (posture == Posture::AllIn && distance > 300.0f) {
        return DashChoice::TowardTarget;
    }
    if (distance <= 225.0f && !Engine::IsHardCrowdControlled(target)) {
        return DashChoice::SideStep;
    }
    return DashChoice::Cursor;
}

inline Vector3 EndpointForDashChoice(DashChoice choice,
                                     const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    const Vector3 origin = player.Position();
    if (choice == DashChoice::NoDash ||
        choice == DashChoice::PlayerOwned) {
        return origin;
    }
    Vector3 desired = Game::CursorPos();
    if (target.IsValid()) {
        const Vector3 toward = SharedGeometry::Direction2D(
            origin, target.Position());
        if (choice == DashChoice::TowardTarget) {
            desired = target.Position();
        } else if (choice == DashChoice::Backward) {
            desired = origin - toward * kPassiveDashMaximum;
        } else if (choice == DashChoice::SideStep) {
            const Vector3 cursorDirection = SharedGeometry::Direction2D(
                origin, Game::CursorPos());
            const float side = SharedGeometry::Cross2D(
                toward, cursorDirection) >= 0.0f ? 1.0f : -1.0f;
            desired = origin + SharedGeometry::Rotate2D(
                toward, side * SharedGeometry::kPi * 0.5f) *
                kPassiveDashMaximum;
        }
    }
    return DirectionEndpoint(origin, desired);
}

inline void PlanPlayerOwnedDash(int slot,
                                const AIHeroClient& target,
                                Posture posture) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    PlannedDashChoice = ChooseDashChoice(target, posture);
    PlannedDashEndpoint = EndpointForDashChoice(
        PlannedDashChoice, target);
    PendingDashSlot = slot;
    PendingDashOrigin = player.Position();
    PendingDashWindowUntil = Now() +
        (slot == 1 ? kWDashBufferMs : kDashBufferMs) + 260;
    PassiveDashObserved = false;
    if (PlannedDashChoice == DashChoice::NoDash) {
        ActiveSequence = Sequence::PlayerOwnedNoDash;
    }
}

inline bool PlayerPathAgreesWithPlannedDash() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PlannedDashChoice == DashChoice::NoDash ||
        !PlannedDashEndpoint.IsValid()) {
        return false;
    }
    Vector3 intended = player.PathEnd();
    if (!intended.IsValid() || intended.IsZero()) intended = Game::CursorPos();
    const Vector3 expectedDirection = SharedGeometry::Direction2D(
        PendingDashOrigin, PlannedDashEndpoint);
    const Vector3 inputDirection = SharedGeometry::Direction2D(
        PendingDashOrigin, intended);
    return !expectedDirection.IsZero() && !inputDirection.IsZero() &&
           expectedDirection.Dot(inputDirection) >= 0.55f;
}

inline bool TargetRejectsUltimate(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target)) {
        return true;
    }
    static constexpr std::array<const char*, 18> buffs = {
        "FioraW", "OlafRagnarok", "SionR", "MalphiteR", "ViR",
        "WarwickR", "HecarimUlt", "VolibearR", "ShyvanaTransform",
        "OrnnW", "UdyrE2", "KSanteW", "KSanteW_AllOut", "SettR",
        "BriarE", "GalioE", "MorganaE", "BlackShield",
    };
    for (const char* buff : buffs) {
        if (target.HasBuff(buff)) return true;
    }
    return false;
}

inline float Q1Damage(const AIHeroClient& target, bool outerEdge) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = Q1RawDamage(
        SpellRank(0), player.BonusAttackDamage(), target.MaxHealth(),
        outerEdge, false, player.Level());
    return player.CalculatePhysicalDamage(target, raw);
}

inline float Q2Damage(const AIHeroClient& target, bool firstTarget) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    const float raw = Q2RawDamage(
        SpellRank(0), player.BonusAttackDamage(), target.MaxHealth(),
        firstTarget, false, player.Level());
    return player.CalculatePhysicalDamage(target, raw);
}

inline float WDamage(const AIHeroClient& target, bool empowered) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(
        target, WRawDamage(
            SpellRank(1), player.BonusAttackDamage(), empowered));
}

inline float EDamage(const AIHeroClient& target, int hits) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(
        target, ERawDamage(
            SpellRank(2), player.BonusAttackDamage(), hits));
}

inline float RDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return player.CalculatePhysicalDamage(
        target, RRawDamage(SpellRank(3), player.BonusAttackDamage()));
}

inline float PassiveAttackDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    return SDK::Damage::GetAutoAttackDamage(player, target, true) +
        player.CalculatePhysicalDamage(
            target, PassiveBonusRawDamage(
                player.Level(), player.BonusAttackDamage()));
}

inline float DamageWithoutR(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    float damage = PassiveStacks > 0
        ? PassiveAttackDamage(target)
        : SDK::Damage::GetAutoAttackDamage(
              GameObjects::Player(), target, true);
    if (Ready(0)) {
        damage += Q2IsLive()
            ? Q2Damage(target, true)
            : Q1Damage(target, true) + Q2Damage(target, true);
    }
    if (Ready(1)) damage += WDamage(target, true);
    if (Ready(2)) damage += EDamage(target, 2);
    return damage;
}

inline float FullComboDamage(const AIHeroClient& target) {
    return DamageWithoutR(target) + (Ready(3) ? RDamage(target) : 0.0f);
}

inline bool ConservativeKillable(const AIHeroClient& target,
                                 bool includeR = false) {
    if (!Engine::ValidEnemy(target)) return false;
    const float damage = includeR
        ? FullComboDamage(target)
        : DamageWithoutR(target);
    return damage * 0.88f >= target.Health();
}

inline bool ShouldWeaveAuto(const AIHeroClient& target,
                            bool allowBurstSkip = true) {
    if (PassiveStacks <= 0 || !InEnhancedAttackRange(target, 20.0f)) {
        return false;
    }
    if (Q2IsLive() && Q2ExpireTick > 0 && Q2ExpireTick - Now() <= 330) {
        return false;
    }
    if (allowBurstSkip && Bool(PassiveMenu, "SkipAAForBurst", true) &&
        (target.Health() <= EDamage(target, 1) + Q2Damage(target, false) ||
         IncomingDamage >= GameObjects::Player().Health() * 0.35f)) {
        return false;
    }
    const float refund = PassiveEnergyRefund(
        GameObjects::Player().Level());
    return CurrentResource(200.0f) <= 200.0f - refund + 8.0f ||
           Bool(PassiveMenu, "AlwaysWeaveInRange", true);
}

inline void AppendLineUnit(std::vector<LineTarget>& targets,
                           const AIBaseClient& unit) {
    if (!unit.IsValid() || unit.IsDead() || !unit.IsEnemy() ||
        !unit.IsTargetable()) {
        return;
    }
    targets.push_back(LineTarget{
        unit.Position(), unit.BoundingRadius(),
        static_cast<int>(unit.NetworkId()), true,
    });
}

inline std::vector<LineTarget> Q2CollisionUnits() {
    std::vector<LineTarget> result;
    result.reserve(64);
    for (const auto& minion : GameObjects::EnemyMinions()) {
        AppendLineUnit(result, minion);
    }
    for (const auto& monster : GameObjects::Jungle()) {
        AppendLineUnit(result, monster);
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        AppendLineUnit(result, enemy);
    }
    return result;
}

inline Q2CastPlan BuildQ2Plan(const AIHeroClient& target,
                              float delaySeconds = 0.225f) {
    Q2CastPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, 710.0f)) return best;
    const Vector3 predicted = PredictPosition(target, delaySeconds);
    const Vector3 direct = SharedGeometry::Direction2D(
        player.Position(), predicted);
    if (direct.IsZero()) return best;
    const auto units = Q2CollisionUnits();
    static constexpr std::array<float, 9> offsets = {
        0.0f, 0.045f, -0.045f, 0.085f, -0.085f,
        0.13f, -0.13f, 0.18f, -0.18f,
    };
    float bestScore = -FLT_MAX;
    for (const float offset : offsets) {
        const Vector3 direction = SharedGeometry::Rotate2D(direct, offset);
        if (!Q2Hits(player.Position(), direction, predicted,
                    target.BoundingRadius())) {
            continue;
        }
        const int first = FirstQ2TargetIndex(
            player.Position(), direction, units);
        const int selectedId = first >= 0
            ? units[static_cast<std::size_t>(first)].Id
            : 0;
        int hits = 0;
        for (const auto& unit : units) {
            if (unit.Valid && Q2Hits(
                    player.Position(), direction,
                    unit.Position, unit.Radius)) {
                ++hits;
            }
        }
        const bool intendedFirst =
            selectedId == static_cast<int>(target.NetworkId());
        float score = (intendedFirst ? 1000.0f : 0.0f) +
                      static_cast<float>(hits) * 12.0f -
                      std::fabs(offset) * 260.0f;
        if (score > bestScore) {
            bestScore = score;
            best.Aim = player.Position() + direction * kQ2Range;
            best.SelectedFirstId = selectedId;
            best.HitCount = hits;
            best.IntendedFirst = intendedFirst;
            best.Valid = true;
        }
    }
    return best;
}

inline std::vector<LineTarget> PredictedRChampions(float delaySeconds) {
    std::vector<LineTarget> result;
    result.reserve(8);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1350.0f)) continue;
        result.push_back(LineTarget{
            PredictPosition(enemy, delaySeconds), enemy.BoundingRadius(),
            static_cast<int>(enemy.NetworkId()), true,
        });
    }
    return result;
}

inline UltimatePlan BuildUltimatePlan(const AIHeroClient& intended,
                                       bool defensive = false) {
    UltimatePlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || TargetRejectsUltimate(intended) ||
        player.Position().Distance2D(intended.Position()) >
            kRRange + intended.BoundingRadius()) {
        return best;
    }
    const auto champions = PredictedRChampions(0.70f);
    Vector3 intendedPosition = PredictPosition(intended, 0.70f);
    const Vector3 direct = SharedGeometry::Direction2D(
        player.Position(), intendedPosition);
    if (direct.IsZero()) return best;
    static constexpr std::array<float, 13> offsets = {
        0.0f, 0.035f, -0.035f, 0.070f, -0.070f,
        0.105f, -0.105f, 0.145f, -0.145f,
        0.19f, -0.19f, 0.24f, -0.24f,
    };
    for (const float offset : offsets) {
        const Vector3 direction = SharedGeometry::Rotate2D(direct, offset);
        if (!LineHits(player.Position(), direction, kRRange, kRHalfWidth,
                      intendedPosition, intended.BoundingRadius())) {
            continue;
        }
        const int selectedIndex = FarthestRTargetIndex(
            player.Position(), direction, champions);
        if (selectedIndex < 0) continue;
        const int selectedId = champions[
            static_cast<std::size_t>(selectedIndex)].Id;
        const AIHeroClient selected = HeroByNetworkId(selectedId);
        if (!Engine::ValidEnemy(selected)) continue;
        // R is a blink and is not clipped like Drakehound's Step.  Evaluate
        // the predicted behind-target point directly; if it is terrain the
        // candidate remains unsafe instead of inventing a passive-style stop.
        const Vector3 selectedPosition = champions[
            static_cast<std::size_t>(selectedIndex)].Position;
        const Vector3 landing = RLandingPoint(
            player.Position(), selectedPosition);
        const int enemies = Engine::CountEnemiesAt(landing, 650.0f);
        const int allies = Engine::CountAlliesAt(landing, 850.0f);
        const bool selectedIntended =
            selectedId == static_cast<int>(intended.NetworkId());
        const bool turret = Engine::UnderEnemyTurret(landing);
        const bool lethalDive = selectedIntended &&
            ConservativeKillable(intended, true) &&
            Bool(Engine::ComboMenu, "AllowTurretDive", true);
        const bool safe = !SDK::NavMesh::IsWall(landing) &&
            (defensive || enemies <= Slider(RMenu, "MaxLandingEnemies", 2)) &&
            (!turret || lethalDive) &&
            (defensive || allies > 0 || enemies <= 1 ||
             ConservativeKillable(intended, true));
        float score = (selectedIntended ? 2000.0f : -1800.0f) +
                      static_cast<float>(allies) * 145.0f -
                      static_cast<float>(enemies) * 220.0f -
                      std::fabs(offset) * 180.0f;
        if (turret && !lethalDive) score -= 2000.0f;
        if (selected.AttackRange() >= 450.0f) score += 110.0f;
        if (selected.HealthPercent() <= 45.0f) score += 150.0f;
        if (!safe) score -= 1200.0f;
        if (!best.Valid || score > best.Score) {
            best.Aim = player.Position() + direction * kRRange;
            best.Landing = landing;
            best.IntendedTargetId = static_cast<int>(intended.NetworkId());
            best.SelectedTargetId = selectedId;
            best.EnemiesAtLanding = enemies;
            best.AlliesAtLanding = allies;
            best.Score = score;
            best.IntendedSelected = selectedIntended;
            best.Safe = safe;
            best.Valid = true;
        }
    }
    return best;
}

inline bool CastQ(const AIHeroClient& target,
                  Mode mode,
                  bool forceBeforeExpiry = false,
                  bool allowBodyHit = false,
                  bool fastFollowup = false) {
    if (!Ready(0) || !SpellEnabled(0, mode) ||
        !CastThrottleReady(0, fastFollowup) || !HasEnergy(70.0f)) {
        return false;
    }
    if (!fastFollowup && Orbwalker::IsWindingUp() &&
        Orbwalker::AttackCastDelayRemaining() > 25) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, 720.0f)) return false;

    const bool second = Q2IsLive();
    Vector3 aim{};
    if (second) {
        const Q2CastPlan plan = BuildQ2Plan(target);
        if (!plan.Valid) return false;
        const bool expiring = forceBeforeExpiry ||
            (Q2ExpireTick > 0 && Q2ExpireTick - Now() <=
                Slider(QMenu, "Q2ExpiryMs", 380));
        const bool halfLethal = Q2Damage(target, false) >=
            target.Health() + target.AllShield();
        if (!plan.IntendedFirst && !expiring && !halfLethal &&
            Bool(QMenu, "RequireFirstTarget", true)) {
            LastQ2Plan = plan;
            return false;
        }
        aim = plan.Aim;
        LastQ2Plan = plan;
    } else {
        const Vector3 predicted = PredictPosition(target, 0.225f);
        const Vector3 direction = SharedGeometry::Direction2D(
            player.Position(), predicted);
        if (direction.IsZero()) return false;
        const Q1Region region = ClassifyQ1(
            player.Position(), direction, predicted, target.BoundingRadius());
        const bool lethalBody = Q1Damage(target, false) >=
            target.Health() + target.AllShield();
        if (region == Q1Region::Miss ||
            (region == Q1Region::Inner && !allowBodyHit && !lethalBody &&
             Bool(QMenu, "RequireEdge", true))) {
            LastQ1Region = region;
            return false;
        }
        aim = player.Position() + direction * kQ1OuterRadius;
        LastQ1Region = region;
        LastQDirection = direction;
    }

    if (!Engine::ControllerCastPosition(0, aim)) return false;
    QCastTick = Now();
    QCastStage = second ? 2 : 1;
    QTargetId = static_cast<int>(target.NetworkId());
    if (second) {
        Q2Ready = false;
        Q2ExpireTick = 0;
        ActiveSequence = Sequence::Q2FirstTarget;
    } else {
        ActiveSequence = Sequence::Q1EdgeTrade;
    }
    PlanPlayerOwnedDash(0, target, CurrentPosture);
    return true;
}

inline bool CastW(const AIHeroClient& target,
                  Mode mode,
                  bool reactive = false) {
    if (!Ready(1) || !SpellEnabled(1, mode) ||
        !CastThrottleReady(1, reactive) || !HasEnergy(70.0f) ||
        WBraceActive) {
        return false;
    }
    if (!reactive && Orbwalker::IsWindingUp() &&
        Orbwalker::AttackCastDelayRemaining() > 25) {
        return false;
    }
    if (!Engine::ControllerCastSelf(1)) return false;
    WCastTick = Now();
    WBraceEndTick = WCastTick + kWBraceMs;
    WShieldExpireTick = WCastTick + kWShieldMs;
    WBraceActive = true;
    WShieldActive = true;
    WDamageEmpowered = reactive && IncomingThreatUntil >= Now();
    WTargetId = target.IsValid()
        ? static_cast<int>(target.NetworkId())
        : 0;
    ActiveSequence = Sequence::WCounter;
    PlanPlayerOwnedDash(1, target, CurrentPosture);
    return true;
}

inline bool CastE(const AIHeroClient& target,
                  Mode mode,
                  bool reactive = false,
                  bool fastFollowup = false) {
    if (!Ready(2) || !SpellEnabled(2, mode) ||
        !CastThrottleReady(2, reactive || fastFollowup) ||
        !HasEnergy(70.0f)) {
        return false;
    }
    if (!reactive && !fastFollowup && Orbwalker::IsWindingUp() &&
        Orbwalker::AttackCastDelayRemaining() > 25) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (target.IsValid() && !reactive &&
        player.Position().Distance2D(target.Position()) >
            kERadius + target.BoundingRadius() + 55.0f) {
        return false;
    }
    if (!Engine::ControllerCastSelf(2)) return false;
    ECastTick = Now();
    ESecondWindowUntil = ECastTick + kELockoutMs + kDashBufferMs + 350;
    ESecondStrikeExpected = false;
    ESecondStrikeObserved = false;
    ETargetId = target.IsValid()
        ? static_cast<int>(target.NetworkId())
        : 0;
    ActiveSequence = fastFollowup
        ? Sequence::QuickQE
        : Sequence::EDoubleStrike;
    PlanPlayerOwnedDash(2, target, CurrentPosture);
    ESecondStrikeExpected =
        PlannedDashChoice != DashChoice::NoDash &&
        PlayerPathAgreesWithPlannedDash();
    return true;
}

inline bool CastR(const AIHeroClient& target,
                  UltimateReason reason,
                  Mode mode,
                  bool defensive = false,
                  bool reactive = false) {
    if (!Ready(3) || SpellRank(3) <= 0 || !SpellEnabled(3, mode) ||
        !CastThrottleReady(3, reactive) || PlayerMobilityLocked()) {
        return false;
    }
    UltimatePlan plan = BuildUltimatePlan(target, defensive);
    if (!plan.Valid || !plan.IntendedSelected ||
        (!plan.Safe && !defensive)) {
        LastRPlan = plan;
        return false;
    }
    if (!Engine::ControllerCastPosition(3, plan.Aim)) return false;
    RCastTick = Now();
    RLockoutUntil = RCastTick + kRCastMs + kRSuppressAndStunMs;
    RTargetId = static_cast<int>(target.NetworkId());
    RActive = true;
    LastRPlan = plan;
    LastUltimateReason = reason;
    ActiveSequence = Sequence::RIsolation;
    PlanPlayerOwnedDash(3, target, CurrentPosture);
    return true;
}

inline bool ThreatJustifiesW(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || IncomingThreatUntil < Now()) return false;
    const float shield = WShieldRaw(
        player.Level(), player.BonusAttackDamage());
    const float threshold = player.MaxHealth() *
        static_cast<float>(Slider(WMenu, "DamagePercent", 11)) / 100.0f;
    const bool targetCanBeHit = target.IsValid() &&
        (player.Position().Distance2D(target.Position()) <=
             kWRadius + target.BoundingRadius() + 50.0f ||
         (PlannedDashEndpoint.IsValid() &&
          PlannedDashEndpoint.Distance2D(target.Position()) <=
              kWRadius + target.BoundingRadius()));
    return IncomingDamage >= threshold ||
           IncomingDamage >= shield * 0.38f || IncomingFromTurret ||
           (IncomingHardCcUntil >= Now() && targetCanBeHit);
}

inline bool TryEmergencyW(const AIHeroClient& fallback, Mode mode) {
    if (!Bool(WMenu, "ReactiveShield", true) || !Ready(1)) return false;
    AIHeroClient threat = HeroByNetworkId(CommittedEnemyId);
    if (!Engine::ValidEnemy(threat)) threat = fallback;
    if (!ThreatJustifiesW(threat)) return false;
    if (IncomingImpactTick > Now() +
            Slider(WMenu, "MaximumLeadMs", 520)) {
        return false;
    }
    return CastW(threat, mode, true);
}

inline bool TryQuickAnimationCancel(const AIHeroClient& fallback,
                                    Mode mode) {
    if (!Bool(PassiveMenu, "UseQuickQE", true) || QCastTick <= 0 ||
        QCastStage != 1 || Now() - QCastTick <
            Slider(PassiveMenu, "QuickQEDelay", 185) ||
        Now() - QCastTick > 430 || !Ready(2)) {
        return false;
    }
    AIHeroClient target = HeroByNetworkId(QTargetId);
    if (!Engine::ValidEnemy(target)) target = fallback;
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const bool burst = ConservativeKillable(target, false) ||
        target.HealthPercent() <= Slider(PassiveMenu, "QuickQEHp", 38) ||
        IncomingThreatUntil >= Now();
    const bool close = distance <= kERadius + target.BoundingRadius() + 35.0f;
    if (!burst || !close || ShouldWeaveAuto(target, true)) return false;
    // Casting E during the Q forgiveness window deliberately replaces the
    // queued movement order.  This is the fast no-dash QE branch demonstrated
    // by Ambessa specialists; the following E owns a fresh player dash window.
    PlannedDashChoice = DashChoice::NoDash;
    ActiveSequence = Sequence::QuickQE;
    return CastE(target, mode, false, true);
}

inline bool TryQ2(const AIHeroClient& target, Mode mode) {
    if (!Q2IsLive() || !Ready(0) || !Engine::ValidEnemy(target)) return false;
    const bool expiring = Q2ExpireTick > 0 &&
        Q2ExpireTick - Now() <= Slider(QMenu, "Q2ExpiryMs", 380);
    if (ShouldWeaveAuto(target, false) && !expiring) {
        WeaveTargetId = static_cast<int>(target.NetworkId());
        WeaveWaitUntil = std::min(
            Q2ExpireTick > 0 ? Q2ExpireTick - 300 : Now() + 420,
            Now() + 520);
        ActiveSequence = Sequence::EnergyWeave;
        return false;
    }
    return CastQ(target, mode, expiring, true, expiring);
}

inline bool TryInterrupt() {
    if (!Bool(RMenu, "Interrupt", true) || InterruptExpireTick < Now() ||
        !Ready(3)) {
        return false;
    }
    const AIHeroClient target = HeroByNetworkId(InterruptTargetId);
    if (!Engine::ValidEnemy(target, kRRange + 80.0f)) return false;
    return CastR(
        target, UltimateReason::Interrupt, Mode::Automatic, false, true);
}

inline bool TryManualR(const AIHeroClient& fallback) {
    if (!Key(Engine::AutomaticMenu, "ManualR", false) || !Ready(3)) {
        return false;
    }
    AIHeroClient target = fallback;
    if (!Engine::ValidEnemy(target, kRRange + 80.0f)) {
        target = Engine::SelectTarget(kRRange + 80.0f);
    }
    return Engine::ValidEnemy(target) && CastR(
        target, UltimateReason::LongCatch, Mode::Automatic, false, true);
}

inline bool TrySmartR(const AIHeroClient& target, Mode mode) {
    if (!Ready(3) || !Engine::ValidEnemy(target) ||
        TargetRejectsUltimate(target)) {
        return false;
    }
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const bool execute = Bool(RMenu, "Execute", true) &&
        RDamage(target) >= target.Health() + target.AllShield() &&
        DamageWithoutR(target) < target.Health() + target.AllShield();
    if (execute) {
        return CastR(target, UltimateReason::Execute, mode);
    }
    if (mode != Mode::Combo) return false;
    const bool carry = target.AttackRange() >= 450.0f ||
                       target.AP() >= 220.0f ||
                       target.TotalAttackDamage() >= 190.0f;
    const bool isolate = Bool(RMenu, "IsolateCarry", true) && carry &&
        target.HealthPercent() <= Slider(RMenu, "IsolateHp", 72) &&
        (Engine::CountAlliesAt(target.Position(), 900.0f) >= 1 ||
         ConservativeKillable(target, true));
    if (isolate && CursorConsentsTo(target.Position(), -0.05f)) {
        CurrentPosture = Posture::Isolate;
        return CastR(target, UltimateReason::IsolateCarry, mode);
    }
    const bool catchTarget = Bool(RMenu, "LongCatch", true) &&
        distance > 650.0f &&
        distance <= kRRange + target.BoundingRadius() &&
        target.HealthPercent() <= Slider(RMenu, "CatchHp", 58) &&
        CursorConsentsTo(target.Position(), 0.25f);
    return catchTarget && CastR(
        target, UltimateReason::LongCatch, mode);
}

inline Posture SelectPosture(Mode mode, const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (mode == Mode::Flee) return Posture::Escape;
    if (mode == Mode::Jungle ||
        ((mode == Mode::LaneClear || mode == Mode::None) &&
         !Engine::ValidEnemy(target, 900.0f))) {
        return Posture::Jungle;
    }
    if (!player.IsValid() || !Engine::ValidEnemy(target)) {
        return Posture::Neutral;
    }
    const float distance = player.Position().Distance2D(target.Position());
    if (mode == Mode::Harass) {
        return distance < 245.0f ? Posture::Kite : Posture::ShortTrade;
    }
    if (player.HealthPercent() <=
            Slider(TacticsMenu, "KiteHp", 34) &&
        !ConservativeKillable(target, false)) {
        return Posture::Kite;
    }
    if (ConservativeKillable(target, true) ||
        target.HealthPercent() <= Slider(TacticsMenu, "AllInHp", 54)) {
        return Posture::AllIn;
    }
    if (distance > 500.0f) return Posture::Chase;
    if (distance >= kQ1InnerRadius - target.BoundingRadius() &&
        distance <= kQ1OuterRadius + target.BoundingRadius()) {
        return Posture::Space;
    }
    return Posture::ShortTrade;
}

inline bool TryReactiveGapcloser(const AIHeroClient& fallback) {
    if (GapcloserExpireTick < Now()) return false;
    AIHeroClient target = HeroByNetworkId(GapcloserTargetId);
    if (!Engine::ValidEnemy(target)) target = fallback;
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    if (Ready(1) && IncomingThreatUntil >= Now() &&
        Bool(WMenu, "ShieldGapcloser", true)) {
        return CastW(target, Mode::Automatic, true);
    }
    if (Ready(2) && Bool(EMenu, "SlowGapcloser", true) &&
        (GapcloserEnd.Distance2D(player.Position()) <=
             kERadius + target.BoundingRadius() ||
         player.Position().Distance2D(target.Position()) <=
             kERadius + target.BoundingRadius())) {
        CurrentPosture = Posture::Kite;
        return CastE(target, Mode::Automatic, true);
    }
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (TryQuickAnimationCancel(target, Mode::Combo)) return true;
    if (TryQ2(target, Mode::Combo)) return true;
    if (ShouldWeaveAuto(target, true)) {
        WeaveTargetId = static_cast<int>(target.NetworkId());
        WeaveWaitUntil = Now() + 520;
        ActiveSequence = Sequence::EnergyWeave;
        return false;
    }
    if (TrySmartR(target, Mode::Combo)) return true;

    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    if (Ready(2) && Bool(EMenu, "UseInCombo", true) &&
        distance <= kERadius + target.BoundingRadius() + 25.0f &&
        (CurrentPosture == Posture::AllIn ||
         CurrentPosture == Posture::Kite ||
         !Ready(0))) {
        return CastE(target, Mode::Combo);
    }
    if (Ready(0) && !Q2IsLive()) {
        const bool allowBody = CurrentPosture == Posture::AllIn &&
            (ConservativeKillable(target, false) || distance < 250.0f);
        if (CastQ(target, Mode::Combo, false, allowBody)) return true;
    }
    if (Ready(1) && Bool(WMenu, "UseInAllIn", true) &&
        distance <= kWRadius + target.BoundingRadius() &&
        CurrentPosture == Posture::AllIn &&
        (IncomingThreatUntil >= Now() ||
         player.HealthPercent() <= Slider(WMenu, "AllInHp", 58))) {
        return CastW(target, Mode::Combo, IncomingThreatUntil >= Now());
    }
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) ||
        CurrentResource(200.0f) < Slider(TacticsMenu, "HarassEnergy", 125)) {
        return false;
    }
    if (TryQuickAnimationCancel(target, Mode::Harass)) return true;
    if (TryQ2(target, Mode::Harass)) return true;
    if (ShouldWeaveAuto(target, false)) {
        WeaveTargetId = static_cast<int>(target.NetworkId());
        WeaveWaitUntil = Now() + 430;
        ActiveSequence = Sequence::EnergyWeave;
        return false;
    }
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    if (Ready(0) && !Q2IsLive()) {
        return CastQ(target, Mode::Harass, false, false);
    }
    if (Ready(2) && Bool(EMenu, "UseInHarass", true) &&
        distance <= kERadius + target.BoundingRadius() &&
        CurrentPosture == Posture::Kite) {
        return CastE(target, Mode::Harass);
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& fallback) {
    const AIHeroClient pursuer = NearestEnemyToPlayer(fallback, 1000.0f);
    CurrentPosture = Posture::Escape;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (Engine::ValidEnemy(pursuer) && Ready(2) &&
        Bool(EMenu, "UseInFlee", true) &&
        player.Position().Distance2D(pursuer.Position()) <=
            kERadius + pursuer.BoundingRadius() + 25.0f) {
        ActiveSequence = Sequence::EscapeChain;
        return CastE(pursuer, Mode::Flee, true);
    }
    if (Ready(1) && Bool(WMenu, "UseInFlee", true) &&
        (IncomingThreatUntil >= Now() ||
         player.HealthPercent() <= Slider(WMenu, "FleeHp", 32))) {
        return CastW(pursuer, Mode::Flee, true);
    }
    if (Ready(0) && Bool(QMenu, "UseForFleeStep", true) &&
        HasEnergy(70.0f, static_cast<float>(
            Slider(PassiveMenu, "FleeReserve", 70)))) {
        AIHeroClient castTarget = pursuer;
        if (Engine::ValidEnemy(castTarget, 700.0f)) {
            return CastQ(castTarget, Mode::Flee, false, true);
        }
    }
    return false;
}

inline bool TryJungle() {
    if (!Bool(FarmMenu, "JungleAbilities", true)) return false;
    const AIMinionClient monster =
        ControllerHelpers::SelectJungleTarget(720.0f, 0.15f);
    if (!monster.IsValid()) return false;
    const AIBaseClient unit(monster.Address());
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(monster.Position());
    if (PassiveStacks > 0 && InEnhancedAttackRange(unit, 20.0f)) {
        ActiveSequence = Sequence::JungleWeave;
        return false;
    }
    if (Q2IsLive() && Ready(0) && SpellEnabled(0, Mode::LaneClear) &&
        HasEnergy(70.0f)) {
        const Vector3 direction = SharedGeometry::Direction2D(
            player.Position(), monster.Position());
        if (!direction.IsZero() && Engine::ControllerCastPosition(
                0, player.Position() + direction * kQ2Range)) {
            Q2Ready = false;
            Q2ExpireTick = 0;
            QCastStage = 2;
            QCastTick = Now();
            QTargetId = static_cast<int>(monster.NetworkId());
            PlanPlayerOwnedDash(0, {}, Posture::Jungle);
            return true;
        }
    }
    if (Ready(1) && Bool(FarmMenu, "JungleW", true) &&
        IsEpicMonster(monster) && IncomingThreatUntil >= Now() &&
        distance <= kWRadius + monster.BoundingRadius()) {
        return CastW({}, Mode::LaneClear, true);
    }
    if (Ready(2) && Bool(FarmMenu, "JungleE", true) &&
        distance <= kERadius + monster.BoundingRadius() &&
        HasEnergy(70.0f, 55.0f)) {
        if (Engine::ControllerCastSelf(2)) {
            ECastTick = Now();
            ESecondWindowUntil = ECastTick + 800;
            ETargetId = static_cast<int>(monster.NetworkId());
            ActiveSequence = Sequence::JungleWeave;
            PlanPlayerOwnedDash(2, {}, Posture::Jungle);
            return true;
        }
    }
    if (Ready(0) && !Q2IsLive() && Bool(FarmMenu, "JungleQ", true) &&
        distance <= kQ1OuterRadius + monster.BoundingRadius() &&
        HasEnergy(70.0f, 55.0f)) {
        const Vector3 direction = SharedGeometry::Direction2D(
            player.Position(), monster.Position());
        if (!direction.IsZero() && Engine::ControllerCastPosition(
                0, player.Position() + direction * kQ1OuterRadius)) {
            QCastTick = Now();
            QCastStage = 1;
            QTargetId = static_cast<int>(monster.NetworkId());
            ActiveSequence = Sequence::JungleWeave;
            PlanPlayerOwnedDash(0, {}, Posture::Jungle);
            return true;
        }
    }
    return false;
}

struct LaneQPlan {
    Vector3 Aim = {};
    int Hits = 0;
    int LastHits = 0;
    bool Valid = false;
};

inline LaneQPlan BestLaneQPlan(bool secondCast, bool lastHitOnly) {
    LaneQPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return best;
    const float rawFallback = secondCast
        ? Q2RawDamage(SpellRank(0), player.BonusAttackDamage(), 500.0f,
                      false)
        : Q1RawDamage(SpellRank(0), player.BonusAttackDamage(), 500.0f,
                      false);
    for (const auto& candidate : GameObjects::EnemyMinions()) {
        if (!candidate.IsValid() || candidate.IsDead() ||
            !candidate.IsTargetable()) {
            continue;
        }
        const Vector3 direction = SharedGeometry::Direction2D(
            player.Position(), candidate.Position());
        if (direction.IsZero()) continue;
        int hits = 0;
        int lastHits = 0;
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!minion.IsValid() || minion.IsDead() ||
                !minion.IsTargetable()) {
                continue;
            }
            bool hit = secondCast
                ? Q2Hits(player.Position(), direction, minion.Position(),
                         minion.BoundingRadius())
                : ClassifyQ1(player.Position(), direction, minion.Position(),
                             minion.BoundingRadius()) != Q1Region::Miss;
            if (!hit) continue;
            ++hits;
            const float damage = player.CalculatePhysicalDamage(
                minion, rawFallback);
            if (damage >= minion.Health()) ++lastHits;
        }
        const int score = lastHitOnly ? lastHits * 100 + hits : hits * 100 + lastHits;
        const int bestScore = lastHitOnly
            ? best.LastHits * 100 + best.Hits
            : best.Hits * 100 + best.LastHits;
        if (!best.Valid || score > bestScore) {
            best.Aim = player.Position() + direction *
                (secondCast ? kQ2Range : kQ1OuterRadius);
            best.Hits = hits;
            best.LastHits = lastHits;
            best.Valid = true;
        }
    }
    return best;
}

inline bool TryLaneFarm(Mode mode) {
    if (!Ready(0) || !SpellEnabled(0, Mode::LaneClear) ||
        !HasEnergy(70.0f, static_cast<float>(
            Slider(FarmMenu, "LaneReserve", 70))) ||
        PassiveStacks > 0) {
        return false;
    }
    const bool lastHitOnly = mode == Mode::LastHit;
    const bool second = Q2IsLive();
    const LaneQPlan plan = BestLaneQPlan(second, lastHitOnly);
    if (!plan.Valid) return false;
    const int minimum = lastHitOnly
        ? Slider(FarmMenu, "MinimumLastHits", 1)
        : Slider(FarmMenu, "MinimumLaneHits", 3);
    const int value = lastHitOnly ? plan.LastHits : plan.Hits;
    if (value < minimum || !CastThrottleReady(0)) return false;
    if (!Engine::ControllerCastPosition(0, plan.Aim)) return false;
    QCastTick = Now();
    QCastStage = second ? 2 : 1;
    if (second) {
        Q2Ready = false;
        Q2ExpireTick = 0;
    }
    PlanPlayerOwnedDash(0, {}, Posture::Space);
    return true;
}

inline void RefreshState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;

    if (PassiveExpireTick > 0 && now > PassiveExpireTick) PassiveStacks = 0;
    if (Q2ExpireTick > 0 && now > Q2ExpireTick) {
        Q2Ready = false;
        Q2ExpireTick = 0;
    }
    if (RuntimeNameContains(0, "AmbessaQ2") ||
        player.HasBuff("AmbessaQEmpowerReady")) {
        Q2Ready = true;
        if (Q2ExpireTick <= now) Q2ExpireTick = now + kQRecastMs;
    }
    if (WBraceActive && now > WBraceEndTick) WBraceActive = false;
    if (WShieldActive && now > WShieldExpireTick) WShieldActive = false;
    if (RActive && now > RLockoutUntil) RActive = false;
    if (IncomingThreatUntil < now) {
        IncomingDamage = 0.0f;
        IncomingFromTurret = false;
        CommittedEnemyId = 0;
    }
    if (InterruptExpireTick < now) InterruptTargetId = 0;
    if (GapcloserExpireTick < now) GapcloserTargetId = 0;

    const bool liveDash = player.IsDashing() ||
        player.HasBuff("AmbessaPassiveDash");
    const float moved = LastObservedPosition.IsValid()
        ? player.Position().Distance2D(LastObservedPosition)
        : 0.0f;
    if (liveDash && !PassiveDashActive) {
        PassiveDashStartTick = now;
        PassiveDashObserved = true;
        if (PendingDashSlot == 2 && now <= ESecondWindowUntil) {
            ESecondStrikeExpected = true;
            ActiveSequence = Sequence::EDoubleStrike;
        }
    }
    if (!liveDash && PassiveDashActive) PassiveDashEndTick = now;
    PassiveDashActive = liveDash;
    if (PendingDashSlot >= 0 && now <= PendingDashWindowUntil &&
        moved > 42.0f) {
        PassiveDashObserved = true;
        if (PendingDashSlot == 2) ESecondStrikeExpected = true;
    }
    if (PendingDashSlot >= 0 && now > PendingDashWindowUntil) {
        if (PendingDashSlot == 2 && !PassiveDashObserved) {
            ESecondStrikeExpected = false;
        }
        PendingDashSlot = -1;
    }
    LastObservedPosition = player.Position();

    if (WeaveTargetId != 0 &&
        (PassiveStacks <= 0 || now > WeaveWaitUntil ||
         !Engine::ValidEnemy(HeroByNetworkId(WeaveTargetId)))) {
        WeaveTargetId = 0;
        if (ActiveSequence == Sequence::EnergyWeave) {
            ActiveSequence = Sequence::None;
        }
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshState();
    AIHeroClient target = selected;
    if (!Engine::ValidEnemy(target)) {
        target = Engine::SelectTarget();
    }
    LastDecisionTargetId = target.IsValid()
        ? static_cast<int>(target.NetworkId())
        : 0;
    CurrentPosture = SelectPosture(mode, target);

    if (TryManualR(target) || TryInterrupt() ||
        TryEmergencyW(target, Mode::Automatic) ||
        TryReactiveGapcloser(target)) {
        return true;
    }
    if (mode == Mode::Flee) return TryFlee(target);
    if (mode == Mode::Combo) return TryCombo(target);
    if (mode == Mode::Harass) return TryHarass(target);
    if (mode == Mode::LaneClear || mode == Mode::LastHit) {
        if (TryJungle()) return true;
        return TryLaneFarm(mode);
    }
    if (mode == Mode::None) return TryJungle();
    return false;
}

inline void ObserveIncomingCast(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();

    // if (args.Sender.Type == ::Core::Objects::ObjectType::AITurretClient &&
    //     args.IsAutoAttack &&
    //     (args.TargetNetworkId ==
    //          static_cast<std::uint32_t>(player.NetworkId()) ||
    //      args.Target.NetworkId ==
    //          static_cast<std::uint32_t>(player.NetworkId()))) {
    //     IncomingFromTurret = true;
    //     IncomingDamage = std::max(
    //         IncomingDamage, player.MaxHealth() * 0.16f);
    //     IncomingThreatUntil = now + 1400;
    //     IncomingImpactTick = now + 420;
    //     if (Bool(WMenu, "ReactiveShield", true)) {
    //         (void)TryEmergencyW({}, Mode::Automatic);
    //     }
    //     return;
    // }

    const auto analysis = AnalyzeEnemyCast(
        args, 220.0f, 110.0f, 210, 250, 180, 1500, 520);
    if (!analysis.Valid) return;
    if (analysis.Committed) {
        CommittedEnemyId = static_cast<int>(analysis.Enemy.NetworkId());
    }
    if (!analysis.TargetsPlayer && !analysis.CrossesPlayer) return;

    float damage = 0.0f;
    if (args.IsAutoAttack) {
        damage = SDK::Damage::GetAutoAttackDamage(
            analysis.Enemy, player, true);
    } else if (args.Slot >= 0 && args.Slot < 4) {
        damage = SDK::Damage::GetSpellDamage(
            analysis.Enemy, player, Engine::SlotFromIndex(args.Slot),
            SDK::DamageStage::Default);
    }
    if (!std::isfinite(damage) || damage <= 0.0f) {
        damage = 65.0f + analysis.Enemy.TotalAttackDamage() * 0.68f +
                 analysis.Enemy.AP() * 0.42f;
    }
    if (IncomingThreatUntil < now) IncomingDamage = 0.0f;
    IncomingDamage += damage;
    IncomingThreatUntil = now + 1300;
    const int castMs = ControllerHelpers::NormalizedCastDelayMs(
        args.CastDelay, args.IsAutoAttack ? 320 : 250);
    IncomingImpactTick = now + std::clamp(castMs, 40, 1400);
    if (analysis.LikelyHardCrowdControl) {
        IncomingHardCcUntil = std::max(
            IncomingHardCcUntil, now + 720);
    }

    const bool rDodge = Bool(RMenu, "UnstoppableDodge", true) &&
        analysis.LikelyHardCrowdControl && Ready(3) &&
        IncomingImpactTick <= now +
            Slider(RMenu, "DodgeLeadMs", 760) &&
        (player.HealthPercent() <= Slider(RMenu, "DodgeHp", 58) ||
         damage >= player.Health() * 0.30f);
    if (rDodge && !PlayerMobilityLocked()) {
        AIHeroClient target = analysis.Enemy;
        if (!Engine::ValidEnemy(target, kRRange + 80.0f)) {
            target = Engine::SelectTarget(kRRange + 80.0f);
        }
        if (Engine::ValidEnemy(target) &&
            CastR(target, UltimateReason::UnstoppableDodge,
                  Mode::Automatic, true, true)) {
            return;
        }
    }
    if (Bool(WMenu, "ReactiveShield", true)) {
        (void)TryEmergencyW(analysis.Enemy, Mode::Automatic);
    }
}

inline void ObserveLocalAbility(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const int now = Now();
    const auto player = GameObjects::Player();
    const int targetId = static_cast<int>(
        args.TargetNetworkId != 0
            ? args.TargetNetworkId
            : args.Target.NetworkId);

    if (args.IsAutoAttack) {
        LastAutoTargetId = targetId;
        LastAutoTick = now;
        return;
    }

    if (args.Slot == 0 ||
        Engine::TextContains(args.SpellName, "AmbessaQ") ||
        Engine::TextContains(args.ScriptName, "AmbessaQ")) {
        const bool wasSecond =
            Engine::TextContains(args.SpellName, "AmbessaQ2") ||
            Engine::TextContains(args.ScriptName, "AmbessaQ2") ||
            (Q2Ready && now <= Q2ExpireTick);
        QCastTick = now;
        QCastStage = wasSecond ? 2 : 1;
        QTargetId = targetId != 0 ? targetId : LastDecisionTargetId;
        if (args.StartPosition.IsValid() && args.EndPosition.IsValid()) {
            LastQDirection = SharedGeometry::Direction2D(
                args.StartPosition, args.EndPosition);
        }
        if (wasSecond) {
            Q2Ready = false;
            Q2ExpireTick = 0;
            ActiveSequence = Sequence::Q2FirstTarget;
        } else {
            ActiveSequence = Sequence::Q1EdgeTrade;
        }
        if (!Engine::WasControllerCast(0)) {
            PlanPlayerOwnedDash(
                0, HeroByNetworkId(QTargetId), CurrentPosture);
        }
    } else if (args.Slot == 1 ||
               Engine::TextContains(args.SpellName, "AmbessaW") ||
               Engine::TextContains(args.ScriptName, "AmbessaW")) {
        WCastTick = now;
        WBraceEndTick = now + kWBraceMs;
        WShieldExpireTick = now + kWShieldMs;
        WBraceActive = true;
        WShieldActive = true;
        WDamageEmpowered = IncomingThreatUntil >= now;
        WTargetId = targetId != 0 ? targetId : LastDecisionTargetId;
        ActiveSequence = Sequence::WCounter;
        if (!Engine::WasControllerCast(1)) {
            PlanPlayerOwnedDash(
                1, HeroByNetworkId(WTargetId), CurrentPosture);
        }
    } else if (args.Slot == 2 ||
               Engine::TextContains(args.SpellName, "AmbessaE") ||
               Engine::TextContains(args.ScriptName, "AmbessaE")) {
        if (ECastTick > 0 && now - ECastTick <= 900) {
            ESecondStrikeObserved = true;
        } else {
            ECastTick = now;
            ESecondWindowUntil = now + 800;
            ESecondStrikeExpected = false;
            ESecondStrikeObserved = false;
            ETargetId = targetId != 0 ? targetId : LastDecisionTargetId;
            ActiveSequence = Sequence::EDoubleStrike;
            if (!Engine::WasControllerCast(2)) {
                PlanPlayerOwnedDash(
                    2, HeroByNetworkId(ETargetId), CurrentPosture);
            }
        }
    } else if (args.Slot == 3 ||
               Engine::TextContains(args.SpellName, "AmbessaR") ||
               Engine::TextContains(args.ScriptName, "AmbessaR")) {
        RCastTick = now;
        RLockoutUntil = now + kRCastMs + kRSuppressAndStunMs;
        RActive = true;
        RTargetId = targetId != 0 ? targetId : LastDecisionTargetId;
        ActiveSequence = Sequence::RIsolation;
        if (!Engine::WasControllerCast(3)) {
            PlanPlayerOwnedDash(
                3, HeroByNetworkId(RTargetId), CurrentPosture);
        }
    } else {
        return;
    }

    // Live buff events replace this fallback count.  It exists because some
    // SDK builds omit a one-frame buff add when the cast and attack land in
    // the same event batch.
    PassiveStacks = std::min(3, PassiveStacks + 1);
    PassiveExpireTick = now + kPassiveStackMs + kRCastMs;
    if (!player.IsValid()) PassiveStacks = 0;
}

inline void OnProcessSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        ObserveLocalAbility(args);
    } else {
        ObserveIncomingCast(args);
    }
}

inline void OnDoCast(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick)) return;
    const int now = LastAutoTick;
    if ((LastPassiveAttackTick <= 0 ||
         now - LastPassiveAttackTick > 80) && PassiveStacks > 0) {
        --PassiveStacks;
    }
    LastPassiveAttackTick = now;
    if (WeaveTargetId == 0 || WeaveTargetId == LastAutoTargetId) {
        WeaveTargetId = 0;
        WeaveWaitUntil = 0;
        if (ActiveSequence == Sequence::EnergyWeave ||
            ActiveSequence == Sequence::JungleWeave) {
            ActiveSequence = Sequence::None;
        }
    }
}

inline void UpdatePlayerBuff(
    const SDK::Events::BuffEventArgs& args,
    bool removed) {
    const int now = Now();
    if (NameEquals(args.BuffName, "AmbessaPassiveAttackEmpower") ||
        NameEquals(args.BuffName, "AmbessaPassive")) {
        PassiveStacks = removed ? 0 : std::clamp(args.Count, 1, 3);
        PassiveExpireTick = removed ? 0 : now + RemainingMilliseconds(
            args.EndTime, kPassiveStackMs, 0, 5000);
    } else if (NameEquals(args.BuffName, "AmbessaPassiveDash")) {
        PassiveDashActive = !removed;
        if (!removed) {
            PassiveDashObserved = true;
            PassiveDashStartTick = now;
            if (PendingDashSlot == 2 && now <= ESecondWindowUntil) {
                ESecondStrikeExpected = true;
            }
        } else {
            PassiveDashEndTick = now;
        }
    } else if (NameEquals(args.BuffName, "AmbessaQEmpowerReady")) {
        Q2Ready = !removed;
        Q2ExpireTick = removed ? 0 : now + RemainingMilliseconds(
            args.EndTime, kQRecastMs, 0, 4600);
    } else if (NameEquals(args.BuffName, "AmbessaW") ||
               NameEquals(args.BuffName, "AmbessaWShield")) {
        WShieldActive = !removed;
        if (!removed) {
            WShieldExpireTick = now + RemainingMilliseconds(
                args.EndTime, kWShieldMs, 0, 1800);
        }
    } else if (NameEquals(args.BuffName, "AmbessaRBuffSuppressing") ||
               NameEquals(args.BuffName, "AmbessaRPassive")) {
        if (NameEquals(args.BuffName, "AmbessaRBuffSuppressing")) {
            RActive = !removed;
            if (!removed) {
                RLockoutUntil = std::max(
                    RLockoutUntil,
                    now + RemainingMilliseconds(
                        args.EndTime, 850, 0, 1800));
            }
        }
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        UpdatePlayerBuff(args, false);
        return;
    }
    if (NameEquals(args.BuffName, "AmbessaRSuppressionDebuff") &&
        args.Sender.IsValid()) {
        RTargetId = static_cast<int>(args.Sender.NetworkId);
        RActive = true;
        ActiveSequence = Sequence::RFollowup;
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) UpdatePlayerBuff(args, false);
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        UpdatePlayerBuff(args, true);
        return;
    }
    if (NameEquals(args.BuffName, "AmbessaRSuppressionDebuff") &&
        args.Sender.IsValid() &&
        static_cast<int>(args.Sender.NetworkId) == RTargetId) {
        RActive = false;
        ActiveSequence = Sequence::RFollowup;
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    const AIBaseClient attackTarget(args.Target.Handle());
    if (!attackTarget.IsValid()) return;

    AIHeroClient desired = HeroByNetworkId(
        WeaveTargetId != 0 ? WeaveTargetId : LastDecisionTargetId);
    if (PassiveStacks > 0 && Engine::ValidEnemy(desired) &&
        InEnhancedAttackRange(desired, 25.0f)) {
        if (!attackTarget.IsHero() ||
            static_cast<int>(attackTarget.NetworkId()) !=
                static_cast<int>(desired.NetworkId())) {
            if (Bool(PassiveMenu, "ProtectCombatStacks", true)) {
                args.Process = false;
                return;
            }
        }
    }

    if (Q2IsLive() && Q2ExpireTick > 0 &&
        Q2ExpireTick - Now() <=
            static_cast<int>(std::max(
                0.0f, SDK::Utils::AutoAttack::GetTimeToHit(args.Target))) +
                Slider(QMenu, "Q2ExpiryMs", 380) &&
        Engine::ValidEnemy(desired) &&
        Bool(QMenu, "ProtectExpiringQ2", true)) {
        args.Process = false;
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick)) return;
    const int now = LastAutoTick;
    if ((LastPassiveAttackTick <= 0 ||
         now - LastPassiveAttackTick > 80) && PassiveStacks > 0) {
        --PassiveStacks;
    }
    LastPassiveAttackTick = now;
    if (WeaveTargetId == 0 || WeaveTargetId == LastAutoTargetId) {
        WeaveTargetId = 0;
        WeaveWaitUntil = 0;
        if (ActiveSequence == Sequence::EnergyWeave ||
            ActiveSequence == Sequence::JungleWeave) {
            ActiveSequence = Sequence::None;
        }
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::Space: return "space";
    case Posture::ShortTrade: return "short-trade";
    case Posture::AllIn: return "all-in";
    case Posture::Chase: return "chase";
    case Posture::Kite: return "kite";
    case Posture::Isolate: return "isolate";
    case Posture::Escape: return "escape";
    case Posture::Jungle: return "jungle";
    default: return "neutral";
    }
}

inline const char* DashName(DashChoice choice) {
    switch (choice) {
    case DashChoice::NoDash: return "NO-DASH";
    case DashChoice::TowardTarget: return "forward";
    case DashChoice::SideStep: return "side";
    case DashChoice::Backward: return "back";
    case DashChoice::Cursor: return "cursor";
    case DashChoice::Escape: return "escape";
    default: return "player";
    }
}

inline const char* UltimateReasonName(UltimateReason reason) {
    switch (reason) {
    case UltimateReason::Execute: return "execute";
    case UltimateReason::IsolateCarry: return "isolate";
    case UltimateReason::Interrupt: return "interrupt";
    case UltimateReason::LongCatch: return "catch";
    case UltimateReason::UnstoppableDodge: return "CC-dodge";
    case UltimateReason::EscapeReposition: return "escape";
    default: return "hold";
    }
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;

    if (Bool(CoachMenu, "DrawQ1", true)) {
        Drawing::DrawCircle(player.Position(), kQ1InnerRadius,
                            0x668D2735u, 1.3f, 64);
        Drawing::DrawCircle(player.Position(), kQ1OuterRadius,
                            0xCCEF5B68u, 2.0f, 64);
    }
    if (Bool(CoachMenu, "DrawDash", true) &&
        PlannedDashEndpoint.IsValid() &&
        PendingDashWindowUntil >= Now()) {
        const std::uint32_t color =
            PlannedDashChoice == DashChoice::NoDash
                ? 0xFFFFD166u
                : (DashEndpointSafe(
                       PlannedDashEndpoint,
                       HeroByNetworkId(LastDecisionTargetId), false)
                       ? 0xFF55E69Bu
                       : 0xFFFF5263u);
        Drawing::DrawLine(
            player.Position(), PlannedDashEndpoint, color, 2.5f);
        Drawing::DrawCircle(
            PlannedDashEndpoint, 44.0f, color, 2.2f, 40);
    }
    if (Bool(CoachMenu, "DrawQ2", true) &&
        LastQ2Plan.Valid && LastQ2Plan.Aim.IsValid()) {
        Drawing::DrawLine(
            player.Position(), LastQ2Plan.Aim,
            LastQ2Plan.IntendedFirst ? 0xFF58D68Du : 0xFFFFA94Du,
            2.0f);
    }
    if (Bool(CoachMenu, "DrawR", true) &&
        LastRPlan.Valid && LastRPlan.Aim.IsValid()) {
        Drawing::DrawLine(
            player.Position(), LastRPlan.Aim,
            LastRPlan.IntendedSelected && LastRPlan.Safe
                ? 0xFFDA70D6u
                : 0xFFFF5F6Du,
            2.4f);
        Drawing::DrawCircle(
            LastRPlan.Landing, 65.0f,
            LastRPlan.Safe ? 0xFFDA70D6u : 0xFFFF5F6Du,
            2.3f, 44);
        const AIHeroClient selected = HeroByNetworkId(
            LastRPlan.SelectedTargetId);
        if (selected.IsValid()) {
            Drawing::DrawCircle(
                selected.Position(), selected.BoundingRadius() + 48.0f,
                LastRPlan.IntendedSelected
                    ? 0xFFDA70D6u
                    : 0xFFFF5F6Du,
                2.6f, 48);
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char state[320]{};
            _snprintf_s(
                state, sizeof(state), _TRUNCATE,
                "Ambessa one-trick | %s | E %.0f | P %d%s | Q%s %dms | dash %s%s | R %s",
                PostureName(CurrentPosture), CurrentResource(200.0f), PassiveStacks,
                PassiveStacks > 0 ? " AA" : "",
                Q2IsLive() ? "2" : "1",
                Q2IsLive() && Q2ExpireTick > 0
                    ? std::max(0, Q2ExpireTick - Now())
                    : 0,
                DashName(PlannedDashChoice),
                PassiveDashObserved ? " observed" : "",
                UltimateReasonName(LastUltimateReason));
            Drawing::DrawText(
                screen.x - 205.0f, screen.y - 118.0f,
                0xFFFFD8DCu, state);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "AmbessaOneTrick", "Ambessa one-trick mechanics"));
    TacticsMenu->Add(new MenuSlider(
        "AllInHp", "All-in posture HP (%)", 54, 15, 90));
    TacticsMenu->Add(new MenuSlider(
        "KiteHp", "Player HP for kite posture (%)", 34, 10, 70));
    TacticsMenu->Add(new MenuSlider(
        "HarassEnergy", "Harass minimum energy", 125, 70, 200));
    TacticsMenu->Add(new MenuSeparator(
        "Ownership",
        "Abilities are assisted;"));

    PassiveMenu = TacticsMenu->AddSubMenu(new Menu(
        "DrakehoundStep", "Dash/no-dash and empowered-AA economy"));
    PassiveMenu->Add(new MenuBool(
        "AlwaysWeaveInRange", "Weave empowered AA when a", true));
    PassiveMenu->Add(new MenuBool(
        "SkipAAForBurst", "Skip AA for QE/Q2", true));
    PassiveMenu->Add(new MenuBool(
        "ProtectCombatStacks", "No stacks on wrong unit", true));
    PassiveMenu->Add(new MenuBool(
        "KeepQSweetspot", "Choose no-dash when moving", true));
    PassiveMenu->Add(new MenuBool(
        "UseQuickQE", "specialist no-dash QE", true));
    PassiveMenu->Add(new MenuSlider(
        "QuickQEDelay", "Earliest E after Q (ms)", 185, 150, 260));
    PassiveMenu->Add(new MenuSlider(
        "QuickQEHp", "Quick QE branch HP (%)", 38, 10, 75));
    PassiveMenu->Add(new MenuBool(
        "RespectDashHazards", "Avoid ready anti-dash zones", true));
    PassiveMenu->Add(new MenuBool(
        "BaitAntiDash", "Prefer no-dash while", true));
    PassiveMenu->Add(new MenuSlider(
        "MaxDashEnemies", "Max enemies around a", 2, 1, 5));
    PassiveMenu->Add(new MenuSlider(
        "FleeReserve", "Energy after flee spell", 70, 0, 140));
    PassiveMenu->Add(new MenuSeparator(
        "HoldStop",
        "The coach marks a no-dash"));

    QMenu = TacticsMenu->AddSubMenu(new Menu(
        "CunningSweep", "Q1 blade edge and Q2 first-target control"));
    QMenu->Add(new MenuBool(
        "RequireEdge", "Hold Q1 body hit unless it", true));
    QMenu->Add(new MenuBool(
        "RequireFirstTarget", "Hold Q2 when another unit", true));
    QMenu->Add(new MenuSlider(
        "Q2ExpiryMs", "Force Q2 before expiry (ms)", 380, 180, 700));
    QMenu->Add(new MenuBool(
        "ProtectExpiringQ2", "Block AA whose impact would", true));
    QMenu->Add(new MenuBool(
        "UseForFleeStep", "Q flee Step", true));
    QMenu->Add(new MenuSeparator(
        "QGeometry",
        "Q1: 180 degree, 275 body/400"));

    WMenu = TacticsMenu->AddSubMenu(new Menu(
        "Repudiation", "Shield interception and empowered slam"));
    WMenu->Add(new MenuBool(
        "ReactiveShield", "Time W into champion, turret", true));
    WMenu->Add(new MenuSlider(
        "DamagePercent", "Min incoming dmg (%)", 11, 4, 40));
    WMenu->Add(new MenuSlider(
        "MaximumLeadMs", "Max W lead before predicted", 520, 120, 900));
    WMenu->Add(new MenuBool(
        "ShieldGapcloser", "Shield a committed gapcloser", true));
    WMenu->Add(new MenuBool(
        "UseInAllIn", "W all-in vs counter-dmg", true));
    WMenu->Add(new MenuSlider(
        "AllInHp", "Proactive W HP (%)", 58, 20, 90));
    WMenu->Add(new MenuBool(
        "UseInFlee", "Use W on real flee damage", true));
    WMenu->Add(new MenuSlider(
        "FleeHp", "Emergency flee W HP (%)", 32, 10, 70));
    WMenu->Add(new MenuSeparator(
        "Empower",
        "W is counted as 150% damage"));

    EMenu = TacticsMenu->AddSubMenu(new Menu(
        "Lacerate", "Slow, reposition and real second-strike state"));
    EMenu->Add(new MenuBool("UseInCombo", "Use E for contact/all-in", true));
    EMenu->Add(new MenuBool("UseInHarass", "E slow harass target", true));
    EMenu->Add(new MenuBool("UseInFlee", "E pursuer on player dash", true));
    EMenu->Add(new MenuBool("SlowGapcloser", "E a gapcloser at its endpoint", true));
    EMenu->Add(new MenuSeparator(
        "SecondHit",
        "The second E is projected"));

    RMenu = TacticsMenu->AddSubMenu(new Menu(
        "PublicExecution", "Farthest-target selection and landing safety"));
    RMenu->Add(new MenuBool("Execute", "R when R is uniquely to", true));
    RMenu->Add(new MenuBool("IsolateCarry", "R carry all-in", true));
    RMenu->Add(new MenuSlider("IsolateHp", "Carry HP for isolation (%)", 72, 25, 100));
    RMenu->Add(new MenuBool("LongCatch", "R distant cursor catch", true));
    RMenu->Add(new MenuSlider("CatchHp", "Target HP for long catch (%)", 58, 20, 90));
    RMenu->Add(new MenuBool("Interrupt", "R an interruptible channel", true));
    RMenu->Add(new MenuBool(
        "UnstoppableDodge", "Use R cast/lockout to ignore", true));
    RMenu->Add(new MenuSlider(
        "DodgeLeadMs", "Max CC lead for R (ms)", 760, 180, 1000));
    RMenu->Add(new MenuSlider(
        "DodgeHp", "Player HP for R CC-dodge (%)", 58, 20, 90));
    RMenu->Add(new MenuSlider(
        "MaxLandingEnemies", "Max enemies R land", 2, 1, 5));
    RMenu->Add(new MenuSeparator(
        "FarthestRule",
        "Every aim is simulated vs"));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Energy-aware lane and jungle routing"));
    FarmMenu->Add(new MenuBool("JungleAbilities", "Use abilities in jungle", true));
    FarmMenu->Add(new MenuBool("JungleQ", "Use both Q casts on jungle", true));
    FarmMenu->Add(new MenuBool("JungleE", "E with kite dash", true));
    FarmMenu->Add(new MenuBool("JungleW", "W counter epic monster", true));
    FarmMenu->Add(new MenuSlider("LaneReserve", "Lane-clear energy reserve", 70, 0, 140));
    FarmMenu->Add(new MenuSlider("MinimumLaneHits", "Minimum Q lane hits", 3, 1, 8));
    FarmMenu->Add(new MenuSlider("MinimumLastHits", "Minimum Q last hits", 1, 1, 5));
    FarmMenu->Add(new MenuSeparator(
        "MonsterData",
        "Monster Q adds current 75"));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "One-trick visual coaching"));
    CoachMenu->Add(new MenuBool("DrawQ1", "Draw Q1 body/edge", false));
    CoachMenu->Add(new MenuBool("DrawQ2", "Draw Q2 first-target plan", false));
    CoachMenu->Add(new MenuBool("DrawDash", "Draw Step endpoint", false));
    CoachMenu->Add(new MenuBool("DrawR", "Draw R line/landing", false));
    CoachMenu->Add(new MenuBool("DrawState", "Draw energy/branch", false));
}

inline void OnLoad() {
    ActiveSequence = Sequence::None;
    CurrentPosture = Posture::Neutral;
    PlannedDashChoice = DashChoice::PlayerOwned;
    LastUltimateReason = UltimateReason::None;
    PassiveStacks = PassiveExpireTick = LastPassiveAttackTick = 0;
    PassiveDashActive = PassiveDashObserved = false;
    PassiveDashStartTick = PassiveDashEndTick = 0;
    PendingDashSlot = -1;
    PendingDashWindowUntil = 0;
    PendingDashOrigin = PlannedDashEndpoint = {};
    LastObservedPosition = GameObjects::Player().Position();
    Q2Ready = false;
    Q2ExpireTick = QCastTick = QCastStage = QTargetId = 0;
    LastQDirection = {};
    LastQ1Region = Q1Region::Miss;
    LastQ2Plan = {};
    WBraceActive = WShieldActive = WDamageEmpowered = false;
    WCastTick = WBraceEndTick = WShieldExpireTick = WTargetId = 0;
    ECastTick = ESecondWindowUntil = ETargetId = 0;
    ESecondStrikeExpected = ESecondStrikeObserved = false;
    RActive = false;
    RCastTick = RLockoutUntil = RTargetId = 0;
    LastRPlan = {};
    IncomingThreatUntil = IncomingHardCcUntil = IncomingImpactTick = 0;
    CommittedEnemyId = 0;
    IncomingDamage = 0.0f;
    IncomingFromTurret = false;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEnd = {};
    InterruptTargetId = InterruptExpireTick = 0;
    LastAutoTargetId = LastAutoTick = WeaveTargetId = WeaveWaitUntil = 0;
    LastDecisionTargetId = 0;
    RefreshState();
}

inline void OnUnload() {
    TacticsMenu = PassiveMenu = QMenu = WMenu = EMenu = nullptr;
    RMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Route every combat tick through space, short-trade, all-in, chase, kite, isolate, escape or jungle posture",
    "Treat movement, attack orders, Hold, Stop and Flash as player-owned inputs",
    "Plan a passive endpoint without issuing a movement command",
    "Use the latest player path/cursor direction to validate an expected Step",
    "Choose a deliberate no-dash branch when movement would lose Q1 blade-edge spacing",
    "Choose a deliberate no-dash branch while a ready anti-dash zone controls the endpoint",
    "Avoid ready Poppy W, Taliyah E and Cassiopeia W dash hazards",
    "Reject a passive endpoint inside terrain",
    "Clip a passive endpoint at the first NavMesh wall because Step cannot cross terrain",
    "Reject a passive endpoint under an enemy turret outside a lethal approved dive",
    "Reject a passive endpoint surrounded by more enemies than configured",
    "Keep the 175 minimum and 350 maximum passive-dash travel model",
    "Use current level 6, 11 and 16 passive dash-speed breakpoints",
    "Track the live AmbessaPassiveDash buff rather than assuming every spell caused movement",
    "Fallback to position displacement only inside the actual post-cast dash window",
    "Never count E's second strike when no passive dash was observed",
    "Count E's second strike after the live dash buff even if the dash is interrupted",
    "Track up to three Medarda Maxim stacks from the live empowered-attack buff",
    "Expire passive stacks after the current four-second duration",
    "Use current passive damage of 5-30 plus 25 percent bonus AD",
    "Use 40/55/70 energy refund breakpoints at levels 1/7/13",
    "Plan a three-spell sequence around real sequential energy rather than mana percent",
    "Wait for an empowered AA when its refund is needed for the next basic spell",
    "Skip the empowered AA when a lethal QE/Q2 window would otherwise be lost",
    "Preserve a combat passive stack from being spent on a minion while the selected champion is in range",
    "Allow ordinary player orbwalking after the controller decides to wait for an AA",
    "Respect the uncancellable empowered-attack windup by not casting over it",
    "Model Q1 as a 180-degree semicircle rather than a generic 650 range spell",
    "Model Q1 body radius as 275 and blade edge as 275-400",
    "Include target gameplay radius at the edge of Q1",
    "Require a Q1 blade-edge hit in normal spacing posture",
    "Allow a Q1 body hit only when lethal or an explicit close all-in demands speed",
    "Predict target position at Q1's 0.225-second lockout",
    "Aim Q1 from Ambessa's live end-of-lockout facing direction",
    "Track Q2 availability from AmbessaQEmpowerReady and the runtime spell name",
    "Track the current four-second Q2 recast window",
    "Model Q2 as a 650 line with 40 half-width",
    "Collect heroes, lane minions and jungle monsters as possible Q2 first-hit blockers",
    "Try multiple Q2 angles instead of blindly casting through a minion",
    "Require the intended champion to be the first Q2 unit in ordinary combat",
    "Permit a blocked half-damage Q2 when that half is still lethal",
    "Force the best valid Q2 before its recast window expires",
    "Block an AA whose impact would make an available Q2 expire",
    "Use current Q percent-health base after the 26.10 buff",
    "Use 3 percent per 100 bonus AD on Q1 max-health damage",
    "Use 4 percent per 100 bonus AD on Q2 max-health damage",
    "Halve the complete Q1 package on body hits",
    "Halve the complete Q2 package after the first enemy",
    "Use current 75 flat Q monster bonus from patch 26.10",
    "Cap Q monster max-health damage at 100-300 by champion level",
    "Enter the specialist quick-QE branch only when burst speed is valuable",
    "Cast E near the end of Q lockout to replace the queued dash with the ability input",
    "Give the following E its own fresh player-owned dash decision",
    "Do not label every Q-E chain an animation cancel when the timing window was missed",
    "Use E's 99-percent decaying slow to hold contact or disengage",
    "Use E at a directed gapcloser's endpoint",
    "Use E before a player-directed escape Step",
    "Reject proactive E when the target is outside its 325 effect radius",
    "Use current flat 50 percent bonus-AD scaling on every E strike",
    "Time W into a committed champion spell instead of spending it on cooldown",
    "Time W into a turret attack during a real dive",
    "Time W into an epic-monster hit when jungle W is enabled",
    "Estimate incoming targeted autos and spells from their live caster",
    "Estimate crossing line threats with target radius and cast delay",
    "Use W's 50-320 level shield plus 150 percent bonus AD in the threat threshold",
    "Treat W as empowered only after a valid incoming threat was observed",
    "Never assume minion chip empowers W",
    "Keep W available during ordinary short trades unless damage is actually incoming",
    "Allow all-in W when current HP and counter-damage justify it",
    "Track W's 0.5-second brace and 1.5-second shield separately",
    "Account for W's shorter 0.25-second passive forgiveness window",
    "Let a late player dash move the W slam to its real endpoint",
    "Model R with the current 0.70-second cast time from patch 26.9",
    "Enumerate every enemy champion inside each candidate R line",
    "Select the farthest R champion, never the nearest or selected by assumption",
    "Try multiple R aim angles to isolate the intended target",
    "Reject R when a farther champion would steal the cast",
    "Predict all R candidates at the end of the 0.70-second cast",
    "Evaluate safety behind the seized target rather than at their current center",
    "Reject an unsafe ordinary R landing with too many enemies",
    "Require ally follow-up or conservative solo lethality for an isolation R",
    "Respect the player's cursor before a long catch or isolation R",
    "Reject spell shields, spell immunity, parry and suppression-immune states before R",
    "Account for R failure when suppression cannot be applied",
    "Use R as an execute only when non-R damage is insufficient",
    "Prefer high-value ranged carries for an isolation R",
    "Use R to interrupt a dangerous channel only when the line selects that channeler",
    "Use R cast/lockout displacement immunity against a dangerous predicted CC",
    "Never attempt the defensive R dodge after Ambessa is already rooted or grounded",
    "Use current 150/250/350 plus 80 percent bonus-AD R damage",
    "Use current 15/17.5/20 percent R ability healing from patch 26.10",
    "Use only 25 percent R-heal effectiveness against monsters",
    "Treat R healing as extended-fight sustain rather than guaranteed pre-mitigation health",
    "Continue a player-cast Q, W, E or R by observing the local spell event",
    "Yield briefly to manual spell input through the shared engine arbitration",
    "Do not reset a manual Q2 just because the controller did not request it",
    "Reconstruct passive stacks when a cast and auto land in the same event batch",
    "Use a Q1-on-unit setup to unlock Q2 for chase without claiming the unit is the champion",
    "Weave passive autos between jungle spells to avoid an energy stall",
    "Prefer the highest-health or epic jungle target",
    "Use Q2 first-hit ordering on jungle monsters",
    "Use lane Q only above an explicit energy reserve",
    "Require multiple lane hits outside LastHit mode",
    "Require predicted lethal minions in LastHit mode",
    "Avoid lane spell farming while passive combat stacks are waiting",
    "Draw Q1 inner and blade-edge radii separately",
    "Draw whether Q2's intended target is actually first",
    "Draw the player-owned passive endpoint and mark unsafe/no-dash branches",
    "Draw R aim, actual farthest selected champion and behind-target landing",
    "Expose posture, energy, stacks, Q stage, dash observation and R reason to the player",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Ambessa;
    controller.ControllerId = "champion.kuroaio.ai.ambessa.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIAmbessa.md";
    controller.ImplementationSummary =
        "Eight-posture energy skirmisher with player-owned dash/no-dash "
        "planning, Q1 edge and Q2 first-unit geometry, animation-cancel and "
        "auto-refund branches, threat-timed W, observed-only double E, and "
        "multi-angle farthest-target R isolation/CC-dodge safety.";
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
    controller.OnGapcloser =
        &ControllerHelpers::CaptureGapcloserEvent<
            &GapcloserTargetId, &GapcloserEnd,
            &GapcloserExpireTick, 520, 850>;
    controller.OnInterruptable =
        &ControllerHelpers::CaptureInterruptableEvent<
            &InterruptTargetId, &InterruptExpireTick, 950, 140, 2200>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Ambessa
