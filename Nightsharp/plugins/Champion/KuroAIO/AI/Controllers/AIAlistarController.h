#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIAlistarGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Alistar {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasResourceFor;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NameEquals;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::RemainingMilliseconds;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::UnitByNetworkId;

enum class Sequence : int {
    None,
    BufferedWQ,
    WallPinChain,
    PeelHeadbutt,
    QRepositionInsec,
    TrampleFourStack,
    TrampleStunReady,
    TurretDive,
    EscapeHeadbutt,
};

enum class Posture : int {
    Neutral,
    Peel,
    Engage,
    Insec,
    Disrupt,
    Dive,
    Escape,
};

enum class HeadbuttPurpose : int {
    None,
    BufferedEngage,
    Peel,
    Insec,
    WallPin,
    Interrupt,
    Escape,
};

enum class UltimateReason : int {
    None,
    SuppressionCleanse,
    CriticalCleanse,
    ComboCleanse,
    IncomingBurst,
    MultiEnemyTank,
    TurretAggro,
    FleeTank,
};

struct KnockbackPlan {
    Vector3 TargetAtImpact = {};
    Vector3 DesiredEndpoint = {};
    Vector3 EffectiveEndpoint = {};
    Vector3 WallPoint = {};
    bool HasWall = false;
    bool PinsToWall = false;
    float WallThickness = 0.0f;
    float EffectiveTravel = 0.0f;
};

inline Menu* TacticsMenu = nullptr;
inline Menu* RoleMenu = nullptr;
inline Menu* HeadbuttMenu = nullptr;
inline Menu* PulverizeMenu = nullptr;
inline Menu* TrampleMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Sequence ActiveSequence = Sequence::None;
inline Posture CurrentPosture = Posture::Neutral;
inline HeadbuttPurpose CurrentHeadbuttPurpose = HeadbuttPurpose::None;
inline UltimateReason LastUltimateReason = UltimateReason::None;

inline int WTargetId = 0;
inline int WCastTick = 0;
inline int WExpectedContactTick = 0;
inline int WSequenceExpireTick = 0;
inline bool WDashActive = false;
inline bool WQBufferWanted = false;
inline bool WQBuffered = false;
inline Vector3 WCastOrigin = {};
inline KnockbackPlan LastKnockbackPlan = {};

inline int QCastTick = 0;
inline int QPrimaryTargetId = 0;
inline int InsecTargetId = 0;
inline int InsecExpireTick = 0;
inline Vector3 InsecGoal = {};
inline Vector3 InsecCoachPoint = {};

inline bool EActive = false;
inline bool EAttackReady = false;
inline int ECastTick = 0;
inline int EExpireTick = 0;
inline int EEmpowerExpireTick = 0;
inline int EObservedStacks = 0;
inline int EStunTargetId = 0;
inline int ForcedStunTargetId = 0;

inline int PassiveStacks = 0;
inline int PassiveLastProcTick = 0;
inline int PassiveCooldownUntil = 0;
inline int PassiveHealAllyId = 0;

inline bool RActive = false;
inline int RCastTick = 0;
inline int RExpireTick = 0;

inline int ProtectedAllyId = 0;
inline int PeelThreatId = 0;
inline int TargetedAllyThreatId = 0;
inline int TargetedAllyThreatUntil = 0;
inline int CommittedEnemyId = 0;
inline int CommittedEnemyUntil = 0;
inline int IncomingThreatUntil = 0;
inline float RecentIncomingDamage = 0.0f;
inline int TurretAggroUntil = 0;
inline int TurretShotsObserved = 0;

inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;

inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;

inline constexpr float kQRadius = 375.0f;
inline constexpr float kWRange = 650.0f;
inline constexpr float kERadius = 350.0f;
inline constexpr float kPassiveRadius = 950.0f;
inline constexpr int kWStateGraceMs = 700;
inline constexpr int kInsecWindowMs = 1150;
inline constexpr int kEDurationMs = 5000;
inline constexpr int kEEmpowerDurationMs = 6000;
inline constexpr int kRDurationMs = 7000;

inline bool CastThrottleReady(int index, bool fastFollowup = false) {
    return ControllerHelpers::CastThrottleReady(
        index, 42, fastFollowup ? 0 : -1);
}

inline bool TargetDisplacementImmune(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return true;
    // Narrow live-buff list.  W still stuns several displacement-immune casts,
    // but spending the engage for no reposition/Q guarantee is normally bad.
    static constexpr std::array<const char*, 15> buffs = {
        "OlafRagnarok", "SionR", "MalphiteR", "ViR",
        "WarwickR", "HecarimUlt", "VolibearR",
        "ShyvanaTransform", "OrnnW", "UdyrE2",
        "KSanteW", "KSanteW_AllOut", "SettR",
        "BriarE", "GalioE",
    };
    for (const char* buff : buffs) {
        if (target.HasBuff(buff)) return true;
    }
    return false;
}

inline bool TargetRejectsHeadbutt(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           HasSpellShieldOrImmunity(target) ||
           target.HasBuff("FioraW") ||
           target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzE") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("EliseSpiderE") ||
           target.HasBuff("BardRStasis") ||
           TargetDisplacementImmune(target);
}

inline AIHeroClient SelectProtectedAlly() {
    return SelectProtectionAlly(
        1600.0f, TargetedAllyThreatId, TargetedAllyThreatUntil);
}

inline float PeelThreatScore(const AIHeroClient& enemy,
                             const AIHeroClient& ally) {
    if (!Engine::ValidEnemy(enemy) || !Engine::ValidAlly(ally)) {
        return -FLT_MAX;
    }
    const float distance = enemy.Position().Distance2D(ally.Position());
    if (distance > 900.0f) return -FLT_MAX;
    float score = 950.0f - distance;
    score += enemy.TotalAttackDamage() * 0.65f + enemy.AP() * 0.35f;
    if (enemy.IsDashing()) {
        const Vector3 end = enemy.PathEnd();
        if (end.IsValid() &&
            end.Distance2D(ally.Position()) < distance) {
            score += 420.0f;
        }
    }
    if (static_cast<int>(enemy.NetworkId()) == TargetedAllyThreatId &&
        Now() <= TargetedAllyThreatUntil) {
        score += 600.0f;
    }
    if (Engine::IsHardCrowdControlled(ally)) score += 240.0f;
    if (enemy.AttackRange() < 325.0f && distance < 425.0f) score += 190.0f;
    return score;
}

inline AIHeroClient SelectPeelThreat(const AIHeroClient& ally) {
    if (!Engine::ValidAlly(ally)) return {};
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const float score = PeelThreatScore(enemy, ally);
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    const float threshold = static_cast<float>(
        Slider(RoleMenu, "PeelThreatScore", 620));
    return bestScore >= threshold ? best : AIHeroClient{};
}

inline AIHeroClient ProtectedAlly() {
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (static_cast<int>(ally.NetworkId()) == ProtectedAllyId &&
            Engine::ValidAlly(ally, 1800.0f)) {
            return ally;
        }
    }
    return {};
}

inline Vector3 AlliedDisplacementGoal(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    Vector3 bestTurret{};
    float bestTurretDistance = FLT_MAX;
    for (const auto& turret : GameObjects::AllyTurrets()) {
        if (!turret.IsValid() || turret.IsDead()) continue;
        const float distance = target.IsValid()
            ? turret.Position().Distance2D(target.Position())
            : turret.Position().Distance2D(player.Position());
        if (distance <= 1800.0f && distance < bestTurretDistance) {
            bestTurret = turret.Position();
            bestTurretDistance = distance;
        }
    }
    if (bestTurret.IsValid() && !bestTurret.IsZero()) return bestTurret;

    std::vector<Vec3> points;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally, 1600.0f) ||
            static_cast<int>(ally.NetworkId()) == player.NetworkId()) {
            continue;
        }
        points.push_back(ally.Position());
    }
    return AveragePoint(points);
}

inline float WallThicknessFrom(const Vector3& wallPoint,
                               const Vector3& direction,
                               float maximum = 420.0f) {
    if (!wallPoint.IsValid() || direction.IsZero()) return 0.0f;
    constexpr float step = 12.0f;
    float thickness = 0.0f;
    bool entered = false;
    for (float distance = 0.0f; distance <= maximum; distance += step) {
        const Vector3 sample = wallPoint + direction * distance;
        if (SDK::NavMesh::IsWall(sample)) {
            entered = true;
            thickness = distance + step;
        } else if (entered) {
            break;
        }
    }
    return thickness;
}

inline KnockbackPlan BuildKnockbackPlan(const AIBaseClient& target,
                                        bool qBuffered,
                                        const Vector3& castOrigin = {}) {
    KnockbackPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return plan;
    const Vector3 origin = castOrigin.IsValid() && !castOrigin.IsZero()
        ? castOrigin
        : player.Position();
    const float centerDistance = origin.Distance2D(target.Position());
    const float travel = HeadbuttTravelSeconds(
        centerDistance, player.BoundingRadius(), target.BoundingRadius());
    plan.TargetAtImpact = PredictPosition(target, travel);
    plan.DesiredEndpoint = KnockbackEndpoint(
        origin, plan.TargetAtImpact, qBuffered);
    plan.EffectiveEndpoint = plan.DesiredEndpoint;

    Vector3 wall{};
    if (!qBuffered && SDK::NavMesh::FindWallCollision(
            plan.TargetAtImpact, plan.DesiredEndpoint, wall, 10.0f)) {
        plan.HasWall = true;
        plan.WallPoint = wall;
        const Vector3 direction = SharedGeometry::Direction2D(
            plan.TargetAtImpact, plan.DesiredEndpoint);
        plan.WallThickness = WallThicknessFrom(wall, direction);
        const float required = std::max(
            105.0f, target.BoundingRadius() * 2.0f + 35.0f);
        plan.PinsToWall = plan.WallThickness >= required ||
                          SDK::NavMesh::IsWall(plan.DesiredEndpoint);
        if (plan.PinsToWall) {
            plan.EffectiveEndpoint = StopBeforeWall(
                plan.TargetAtImpact, plan.DesiredEndpoint, wall);
        }
    }
    plan.EffectiveTravel = plan.TargetAtImpact.Distance2D(
        plan.EffectiveEndpoint);
    return plan;
}

inline bool IsUnderEnemyTurret(const Vector3& position) {
    return Engine::UnderEnemyTurret(position);
}

inline float QDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    static constexpr float base[] = { 0.0f, 60.0f, 100.0f, 140.0f, 180.0f, 220.0f };
    const int rank = std::clamp(SpellRank(0), 0, 5);
    return rank > 0 && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, base[rank] + 0.80f * player.AP())
        : 0.0f;
}

inline float WDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    static constexpr float base[] = { 0.0f, 55.0f, 110.0f, 165.0f, 220.0f, 275.0f };
    const int rank = std::clamp(SpellRank(1), 0, 5);
    return rank > 0 && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, base[rank] + player.AP())
        : 0.0f;
}

inline float ETotalDamage(const AIHeroClient& target,
                          int expectedPulses = 10) {
    const auto player = GameObjects::Player();
    static constexpr float base[] = { 0.0f, 80.0f, 110.0f, 140.0f, 170.0f, 200.0f };
    const int rank = std::clamp(SpellRank(2), 0, 5);
    if (rank <= 0 || !Engine::ValidEnemy(target)) return 0.0f;
    const float full = base[rank] + 0.70f * player.AP();
    return player.CalculateMagicDamage(
        target,
        full * static_cast<float>(std::clamp(expectedPulses, 0, 10)) / 10.0f);
}

inline float EEmpoweredDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(
              target, EmpoweredTrampleRawDamage(player.Level()))
        : 0.0f;
}

inline float FullComboDamage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const auto player = GameObjects::Player();
    float damage = SDK::Damage::GetAutoAttackDamage(player, target, false);
    if (Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->IsReady()) {
        damage += QDamage(target);
    }
    if (Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsReady()) {
        damage += WDamage(target);
    }
    if (Engine::RuntimeSpells[2] && Engine::RuntimeSpells[2]->IsReady()) {
        damage += ETotalDamage(target, 6) + EEmpoweredDamage(target);
    }
    return damage;
}

inline bool SoloKillable(const AIHeroClient& target) {
    return Engine::ValidEnemy(target) &&
           FullComboDamage(target) * 0.90f >= target.Health();
}

inline bool AttackCanUseEStun(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    const float range = player.AttackRange() + target.BoundingRadius() +
                        (EAttackReady ? 50.0f : 0.0f);
    return player.Position().Distance2D(target.Position()) <= range + 15.0f;
}

inline float StunTargetScore(const AIHeroClient& enemy,
                             const AIHeroClient& protectedAlly,
                             const AIHeroClient& peelThreat) {
    if (!Engine::ValidEnemy(enemy)) return -FLT_MAX;
    float score = (100.0f - enemy.HealthPercent()) * 3.0f +
                  enemy.TotalAttackDamage() * 0.55f + enemy.AP() * 0.40f;
    if (enemy.AttackRange() >= 450.0f) score += 110.0f;
    if (enemy.NetworkId() == InterruptTargetId &&
        Now() <= InterruptExpireTick) score += 1200.0f;
    if (peelThreat.IsValid() && enemy.NetworkId() == peelThreat.NetworkId()) {
        score += 900.0f;
    }
    if (protectedAlly.IsValid()) {
        score += std::max(
            0.0f,
            650.0f - enemy.Position().Distance2D(protectedAlly.Position()));
    }
    if (enemy.IsDashing()) score += 170.0f;
    return score;
}

inline AIHeroClient SelectEStunTarget(const AIHeroClient& selected,
                                      const AIHeroClient& protectedAlly,
                                      const AIHeroClient& peelThreat) {
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!AttackCanUseEStun(enemy)) continue;
        float score = StunTargetScore(enemy, protectedAlly, peelThreat);
        if (selected.IsValid() && enemy.NetworkId() == selected.NetworkId()) {
            score += 120.0f;
        }
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    return best;
}

inline void ClearForcedStunTarget() {
    if (ForcedStunTargetId != 0) {
        Orbwalker::ForceTarget(AttackableUnit());
        ForcedStunTargetId = 0;
    }
}

inline bool TryPrimeEStun(const AIHeroClient& selected,
                          const AIHeroClient& protectedAlly,
                          const AIHeroClient& peelThreat) {
    if (!EActive && !EAttackReady) {
        ClearForcedStunTarget();
        return false;
    }
    AIHeroClient target = SelectEStunTarget(
        selected, protectedAlly, peelThreat);
    if (!Engine::ValidEnemy(target)) {
        ClearForcedStunTarget();
        return false;
    }

    bool readyOnImpact = EAttackReady || EObservedStacks >= 5;
    if (!readyOnImpact && EObservedStacks == 4 &&
        Bool(TrampleMenu, "FourStackAttack", true)) {
        const int elapsed = std::max(0, Now() - ECastTick);
        const int nextPulse = MillisecondsToNextTramplePulse(elapsed);
        const AttackableUnit attackTarget(target.Handle());
        const int impact = static_cast<int>(std::max(
            0.0f, SDK::Utils::AutoAttack::GetTimeToHit(attackTarget)));
        const bool remains = target.Position().Distance2D(
            GameObjects::Player().Position()) <=
            kERadius + target.BoundingRadius();
        readyOnImpact = FourStackAttackWillStun(
            EObservedStacks, nextPulse, impact, remains);
    }
    if (!readyOnImpact) return false;

    EStunTargetId = static_cast<int>(target.NetworkId());
    ActiveSequence = EAttackReady
        ? Sequence::TrampleStunReady
        : Sequence::TrampleFourStack;
    if (Bool(TrampleMenu, "ForceBestStunTarget", true)) {
        Orbwalker::ForceTarget(AttackableUnit(target.Handle()));
        ForcedStunTargetId = EStunTargetId;
    }
    return true;
}

inline bool CastQ(Mode mode,
                  const AIHeroClient& target = {},
                  bool reactive = false,
                  bool allowDuringAttack = false) {
    if (!Engine::RuntimeSpells[0] || !Engine::RuntimeSpells[0]->IsReady() ||
        !SpellEnabled(0, mode) || !CastThrottleReady(0, reactive)) {
        return false;
    }
    if (!reactive && !allowDuringAttack && Orbwalker::IsWindingUp() &&
        Orbwalker::AttackCastDelayRemaining() > 25) {
        return false;
    }
    if (!reactive && EAttackReady && AttackCanUseEStun(target)) return false;
    if (Engine::ControllerCastSelf(0)) {
        QCastTick = Now();
        QPrimaryTargetId = target.IsValid()
            ? static_cast<int>(target.NetworkId())
            : 0;
        return true;
    }
    return false;
}

inline bool TryBufferQ(Mode mode = Mode::Combo) {
    if (!WQBufferWanted || WQBuffered ||
        Now() > WSequenceExpireTick ||
        !Engine::RuntimeSpells[0] || !Engine::RuntimeSpells[0]->IsReady() ||
        !SpellEnabled(0, mode)) {
        return false;
    }
    // No generic humanizer here: Headbutt's game-side buffer is the mechanic.
    if (Engine::ControllerCastSelf(0)) {
        WQBuffered = true;
        QCastTick = Now();
        QPrimaryTargetId = WTargetId;
        ActiveSequence = Sequence::BufferedWQ;
        return true;
    }
    return false;
}

inline bool CastE(Mode mode,
                  const AIHeroClient& target = {},
                  bool reactive = false) {
    if (!Engine::RuntimeSpells[2] || !Engine::RuntimeSpells[2]->IsReady() ||
        !SpellEnabled(2, mode) || !CastThrottleReady(2, reactive) ||
        EActive || EAttackReady) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (target.IsValid() &&
        player.Position().Distance2D(target.Position()) >
            kERadius + target.BoundingRadius() + 45.0f &&
        !reactive) {
        return false;
    }
    if (Engine::ControllerCastSelf(2)) {
        EActive = true;
        EAttackReady = false;
        ECastTick = Now();
        EExpireTick = ECastTick + kEDurationMs;
        EObservedStacks = 0;
        return true;
    }
    return false;
}

inline bool CanHeadbuttUnit(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid() || target.IsDead() ||
        !target.IsTargetable() || PlayerMobilityLocked() ||
        player.Position().Distance2D(target.Position()) >
            kWRange + target.BoundingRadius() + 20.0f) {
        return false;
    }
    if (target.IsHero()) {
        const AIHeroClient hero(target.Address());
        if (TargetRejectsHeadbutt(hero)) return false;
    }
    return true;
}

inline bool CastW(const AIBaseClient& target,
                  HeadbuttPurpose purpose,
                  Mode mode,
                  bool bufferQ,
                  bool reactive = false) {
    if (!Engine::RuntimeSpells[1] || !Engine::RuntimeSpells[1]->IsReady() ||
        !SpellEnabled(1, mode) || !CastThrottleReady(1, reactive) ||
        !CanHeadbuttUnit(target)) {
        return false;
    }
    if (Bool(HeadbuttMenu, "RespectDashHazards", true) &&
        HasReadyDashHazardAt(target.Position())) {
        return false;
    }
    if (bufferQ && (!Engine::RuntimeSpells[0] ||
                    !Engine::RuntimeSpells[0]->IsReady() ||
                    !SpellEnabled(0, mode) || !HasResourceFor({ 0, 1 }))) {
        return false;
    }

    const auto player = GameObjects::Player();
    const Vector3 origin = player.Position();
    const KnockbackPlan plan = BuildKnockbackPlan(target, bufferQ, origin);
    const int contactMs = static_cast<int>(std::ceil(
        HeadbuttTravelSeconds(
            origin.Distance2D(target.Position()),
            player.BoundingRadius(), target.BoundingRadius()) * 1000.0f));

    if (!Engine::ControllerCastUnit(1, target)) return false;

    WTargetId = static_cast<int>(target.NetworkId());
    WCastTick = Now();
    WExpectedContactTick = WCastTick + std::max(20, contactMs);
    WSequenceExpireTick = WExpectedContactTick + kWStateGraceMs;
    WDashActive = true;
    WQBufferWanted = bufferQ;
    WQBuffered = false;
    WCastOrigin = origin;
    CurrentHeadbuttPurpose = purpose;
    LastKnockbackPlan = plan;
    switch (purpose) {
    case HeadbuttPurpose::Peel:
        ActiveSequence = Sequence::PeelHeadbutt;
        break;
    case HeadbuttPurpose::WallPin:
        ActiveSequence = Sequence::WallPinChain;
        break;
    case HeadbuttPurpose::Insec:
        ActiveSequence = Sequence::QRepositionInsec;
        break;
    case HeadbuttPurpose::Escape:
        ActiveSequence = Sequence::EscapeHeadbutt;
        break;
    default:
        ActiveSequence = bufferQ ? Sequence::BufferedWQ : Sequence::None;
        break;
    }
    if (bufferQ) (void)TryBufferQ(mode);
    return true;
}

inline bool UltimateReady() {
    return SpellRank(3) > 0 && Engine::RuntimeSpells[3] &&
           Engine::RuntimeSpells[3]->IsReady() && !RActive &&
           CurrentResource() + 0.5f >= SpellCost(3);
}

inline bool CastR(UltimateReason reason, Mode mode, bool reactive = false) {
    if (!UltimateReady() || !SpellEnabled(3, mode)) return false;
    if (!reactive && !CastThrottleReady(3)) return false;
    if (Engine::ControllerCastSelf(3)) {
        RActive = true;
        RCastTick = Now();
        RExpireTick = RCastTick + kRDurationMs;
        LastUltimateReason = reason;
        return true;
    }
    return false;
}

inline bool PlayerHasDangerousCC() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
        (Engine::IsHardCrowdControlled(player) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Silence) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Polymorph) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Grounded) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Disarm));
}

inline bool ShouldCleanseBuff(const SDK::Events::BuffEventArgs& args,
                              UltimateReason& reason) {
    reason = UltimateReason::None;
    if (!UltimateReady()) return false;
    const auto player = GameObjects::Player();
    const auto type = static_cast<SDK::BuffType>(args.Type);
    const int duration = RemainingMilliseconds(
        args.EndTime, 450, 0, 5000);
    const int enemies = Engine::CountEnemiesAt(player.Position(), 825.0f);
    const bool inCriticalCombo = ActiveSequence == Sequence::BufferedWQ ||
        ActiveSequence == Sequence::WallPinChain ||
        ActiveSequence == Sequence::QRepositionInsec || EAttackReady;
    const bool dangerous = enemies >= 2 || TurretAggroUntil >= Now() ||
        RecentIncomingDamage >= player.Health() * 0.22f ||
        player.HealthPercent() <=
            static_cast<float>(Slider(UltimateMenu, "CleanseHp", 58));

    if (type == SDK::BuffType::Suppression) {
        reason = UltimateReason::SuppressionCleanse;
        return Bool(UltimateMenu, "CleanseSuppression", true);
    }
    if (type == SDK::BuffType::Charm || type == SDK::BuffType::Taunt ||
        type == SDK::BuffType::Fear || type == SDK::BuffType::Flee ||
        type == SDK::BuffType::Asleep || type == SDK::BuffType::Polymorph) {
        reason = UltimateReason::CriticalCleanse;
        return Bool(UltimateMenu, "CleanseHardCC", true) &&
               (dangerous || inCriticalCombo || duration >= 700);
    }
    if (type == SDK::BuffType::Stun || type == SDK::BuffType::Snare ||
        type == SDK::BuffType::Silence) {
        const int minimum = Slider(UltimateMenu, "MinimumCCMs", 650);
        reason = inCriticalCombo
            ? UltimateReason::ComboCleanse
            : UltimateReason::CriticalCleanse;
        return Bool(UltimateMenu, "CleanseHardCC", true) &&
               duration >= minimum && (dangerous || inCriticalCombo);
    }
    if (type == SDK::BuffType::Knockup ||
        type == SDK::BuffType::Knockback) {
        // R removes the disabling component, not the forced movement itself.
        reason = UltimateReason::CriticalCleanse;
        return Bool(UltimateMenu, "CleanseAirborneInLethalDanger", true) &&
               (TurretAggroUntil >= Now() ||
                RecentIncomingDamage >= player.Health() * 0.55f);
    }
    return false;
}

inline bool TryRDefense(Mode mode) {
    if (!UltimateReady()) return false;
    const auto player = GameObjects::Player();
    const int enemies = Engine::CountEnemiesAt(player.Position(), 850.0f);
    if (TurretAggroUntil >= Now() &&
        Bool(UltimateMenu, "UseAfterTurretAggro", true) &&
        (TurretShotsObserved >= Slider(UltimateMenu, "TurretShotsBeforeR", 1) ||
         player.HealthPercent() <=
             static_cast<float>(Slider(UltimateMenu, "TurretRHp", 72)))) {
        return CastR(UltimateReason::TurretAggro, mode);
    }
    const float expected = RecentIncomingDamage;
    if (IncomingThreatUntil >= Now() &&
        expected >= player.Health() *
            static_cast<float>(Slider(UltimateMenu, "BurstPercent", 42)) /
            100.0f) {
        return CastR(UltimateReason::IncomingBurst, mode);
    }
    if (enemies >= Slider(UltimateMenu, "TankMinimumEnemies", 3) &&
        player.HealthPercent() <=
            static_cast<float>(Slider(UltimateMenu, "TankHp", 56))) {
        return CastR(UltimateReason::MultiEnemyTank, mode);
    }
    if (mode == Mode::Flee && enemies >= 1 &&
        player.HealthPercent() <=
            static_cast<float>(Slider(UltimateMenu, "FleeHp", 34))) {
        return CastR(UltimateReason::FleeTank, mode);
    }
    return false;
}

inline bool HasCursorConsent(const AIHeroClient& target) {
    if (!Bool(RoleMenu, "RespectCursor", true)) return true;
    const auto player = GameObjects::Player();
    const Vector3 toTarget = SharedGeometry::Direction2D(
        player.Position(), target.Position());
    const Vector3 toCursor = SharedGeometry::Direction2D(
        player.Position(), Game::CursorPos());
    if (toTarget.IsZero() || toCursor.IsZero()) return true;
    return toTarget.Dot(toCursor) >= -0.10f;
}

inline bool EngageSafetyAllows(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    const int enemies = Engine::CountEnemiesAt(target.Position(), 725.0f);
    const int maximum = Slider(RoleMenu, "MaxEngageEnemies", 3);
    if (enemies > maximum && player.HealthPercent() < 78.0f && !RActive) {
        return false;
    }
    if (IsUnderEnemyTurret(target.Position())) {
        const bool allowed = Bool(Engine::ComboMenu, "AllowTurretDive", false);
        if (!allowed || (!RActive && !UltimateReady()) ||
            CountAlliedFollowup(target.Position(), 900.0f) <= 0) {
            return false;
        }
    }
    return HasCursorConsent(target);
}

inline bool CanStandardEngage(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, kWRange + 80.0f) ||
        TargetRejectsHeadbutt(target) || !EngageSafetyAllows(target) ||
        !HasResourceFor({ 0, 1, 2 })) {
        return false;
    }
    if (Bool(RoleMenu, "DoNotOverlapCC", true) &&
        Engine::IsHardCrowdControlled(target) && !target.IsDashing()) {
        return false;
    }
    const int followup = CountAlliedFollowup(target.Position(), 875.0f);
    return followup >= Slider(RoleMenu, "MinimumFollowup", 1) ||
           SoloKillable(target);
}

inline bool TryStandardWQ(const AIHeroClient& target) {
    if (!Bool(HeadbuttMenu, "UseWQ", true) ||
        !CanStandardEngage(target)) {
        return false;
    }
    return CastW(target, HeadbuttPurpose::BufferedEngage,
                 Mode::Combo, true);
}

inline bool TryAoeQ(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    const int hits = Engine::CountEnemiesAt(player.Position(), kQRadius + 65.0f);
    const int required = mode == Mode::Combo
        ? Slider(PulverizeMenu, "AoeEnemies", 2)
        : 1;
    if (hits < required) return false;
    AIHeroClient target = Engine::SelectTarget(kQRadius + 90.0f);
    if (CastQ(mode, target, reactive)) {
        CurrentPosture = hits >= 2 ? Posture::Disrupt : CurrentPosture;
        return true;
    }
    return false;
}

inline bool TryStartEAtContact(const AIHeroClient& target,
                               Mode mode,
                               bool reactive = false) {
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const bool stable = Engine::IsHardCrowdControlled(target) ||
                        target.IsDashing() ||
                        target.NetworkId() == WTargetId;
    return distance <= kERadius + target.BoundingRadius() +
                           (stable ? 55.0f : 0.0f) &&
           CastE(mode, target, reactive);
}

inline bool TryTimedWallFollowup() {
    if (ActiveSequence != Sequence::WallPinChain ||
        CurrentHeadbuttPurpose != HeadbuttPurpose::WallPin ||
        Now() > WSequenceExpireTick + 750) {
        return false;
    }
    const AIHeroClient target = HeroByNetworkId(WTargetId);
    if (!Engine::ValidEnemy(target)) return false;
    if (Now() >= WExpectedContactTick - 40 &&
        TryStartEAtContact(target, Mode::Combo, true)) {
        return true;
    }
    const int delay = Slider(HeadbuttMenu, "WallQDelay", 470);
    const bool escaping = target.IsDashing() ||
        target.Position().Distance2D(GameObjects::Player().Position()) >
            kQRadius - 30.0f;
    if (Now() >= WExpectedContactTick + delay || escaping) {
        return CastQ(Mode::Combo, target, true, true);
    }
    return false;
}

inline bool TryWallPin(const AIHeroClient& target) {
    if (!Bool(HeadbuttMenu, "UseWallPin", true) ||
        !Engine::ValidEnemy(target, kWRange + 70.0f) ||
        TargetRejectsHeadbutt(target) || !EngageSafetyAllows(target)) {
        return false;
    }
    const KnockbackPlan plan = BuildKnockbackPlan(target, false);
    if (!plan.PinsToWall ||
        plan.EffectiveTravel <
            static_cast<float>(Slider(HeadbuttMenu, "WallMinTravel", 90)) ||
        plan.EffectiveTravel >
            static_cast<float>(Slider(HeadbuttMenu, "WallMaxTravel", 520))) {
        return false;
    }
    // Keep full W displacement and layer E/Q after the 0.75s Headbutt disable.
    return CastW(target, HeadbuttPurpose::WallPin,
                 Mode::Combo, false);
}

inline bool TryInsec(const AIHeroClient& selected) {
    if (!Bool(HeadbuttMenu, "UseInsec", true) ||
        Now() > InsecExpireTick || InsecTargetId == 0) {
        return false;
    }
    AIHeroClient target = HeroByNetworkId(InsecTargetId);
    if (!Engine::ValidEnemy(target, kWRange + 80.0f) ||
        TargetRejectsHeadbutt(target)) {
        return false;
    }
    const Vector3 goal = AlliedDisplacementGoal(target);
    if (!goal.IsValid() || goal.IsZero()) return false;

    const auto player = GameObjects::Player();
    const Vector3 awayFromGoal = SharedGeometry::Direction2D(
        goal, target.Position());
    InsecGoal = goal;
    InsecCoachPoint = target.Position() + awayFromGoal *
        static_cast<float>(Slider(HeadbuttMenu, "InsecStandDistance", 175));
    const KnockbackPlan plan = BuildKnockbackPlan(target, false);
    const float gain = TowardPointGain(
        plan.TargetAtImpact, plan.EffectiveEndpoint, goal);
    const int elapsed = Now() - QCastTick;
    if (elapsed < Slider(HeadbuttMenu, "InsecWalkDelay", 360) ||
        gain < static_cast<float>(Slider(HeadbuttMenu, "InsecMinimumGain", 260)) ||
        player.Position().Distance2D(InsecCoachPoint) > 260.0f) {
        return false;
    }
    (void)selected;
    CurrentPosture = Posture::Insec;
    return CastW(target, HeadbuttPurpose::Insec,
                 Mode::Combo, false);
}

inline bool TryPeel(const AIHeroClient& ally,
                    const AIHeroClient& threat) {
    if (!Engine::ValidAlly(ally) || !Engine::ValidEnemy(threat)) {
        return false;
    }
    CurrentPosture = Posture::Peel;
    const auto player = GameObjects::Player();
    if (EAttackReady && AttackCanUseEStun(threat)) return true;
    if (player.Position().Distance2D(threat.Position()) <=
            kQRadius + threat.BoundingRadius() &&
        Bool(PulverizeMenu, "PeelQ", true) &&
        CastQ(Mode::Automatic, threat, true, true)) {
        return true;
    }
    if (TryStartEAtContact(threat, Mode::Automatic, true)) return true;
    if (!Bool(HeadbuttMenu, "PeelW", true) ||
        !Engine::ValidEnemy(threat, kWRange + 80.0f)) {
        return false;
    }
    const KnockbackPlan plan = BuildKnockbackPlan(threat, false);
    const float separation = PeelSeparationGain(
        ally.Position(), plan.TargetAtImpact, plan.EffectiveEndpoint);
    if (separation <
        static_cast<float>(Slider(HeadbuttMenu, "PeelMinimumGain", 170))) {
        return false;
    }
    return CastW(threat, HeadbuttPurpose::Peel,
                 Mode::Automatic, false, true);
}

inline bool TryInterrupt() {
    if (InterruptTargetId == 0 || Now() > InterruptExpireTick) return false;
    AIHeroClient target = HeroByNetworkId(InterruptTargetId);
    if (!Engine::ValidEnemy(target, kWRange + 90.0f)) return false;
    if (EAttackReady && AttackCanUseEStun(target)) return true;
    const auto player = GameObjects::Player();
    if (player.Position().Distance2D(target.Position()) <=
            kQRadius + target.BoundingRadius() &&
        Bool(PulverizeMenu, "InterruptQ", true) &&
        CastQ(Mode::Automatic, target, true, true)) {
        InterruptTargetId = 0;
        return true;
    }
    if (Bool(HeadbuttMenu, "InterruptW", true) &&
        CastW(target, HeadbuttPurpose::Interrupt,
              Mode::Automatic, false, true)) {
        InterruptTargetId = 0;
        return true;
    }
    return false;
}

inline bool PassiveHealUrgent(AIHeroClient& ally) {
    ally = {};
    if (!Bool(RoleMenu, "UseCCForPassiveHeal", true) ||
        PassiveStacks < 6 || Now() < PassiveCooldownUntil) {
        return false;
    }
    float lowest = 101.0f;
    for (const auto& candidate : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(candidate, kPassiveRadius) ||
            candidate.HealthPercent() >= lowest) {
            continue;
        }
        ally = candidate;
        lowest = candidate.HealthPercent();
    }
    return ally.IsValid() && lowest <=
        static_cast<float>(Slider(RoleMenu, "PassiveHealAllyHp", 42));
}

inline bool TryPassiveHealSetup(const AIHeroClient& selected) {
    AIHeroClient lowAlly{};
    if (!PassiveHealUrgent(lowAlly)) return false;
    PassiveHealAllyId = static_cast<int>(lowAlly.NetworkId());
    const auto player = GameObjects::Player();
    AIHeroClient target = Engine::ValidEnemy(selected, kQRadius + 80.0f)
        ? selected
        : Engine::SelectTarget(kQRadius + 80.0f);
    if (!Engine::ValidEnemy(target) ||
        HasSpellShieldOrImmunity(target) ||
        player.Position().Distance2D(target.Position()) >
            kQRadius + target.BoundingRadius()) {
        return false;
    }
    return CastQ(Mode::Automatic, target, true, true);
}

inline bool ValidEscapeUnit(const AIBaseClient& unit) {
    const auto player = GameObjects::Player();
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f &&
           unit.IsTargetable() && player.IsValid() &&
           unit.NetworkId() != player.NetworkId() &&
           player.Position().Distance2D(unit.Position()) <=
               kWRange + unit.BoundingRadius();
}

inline AIBaseClient BestEscapeHeadbuttUnit(const AIHeroClient& pursuer) {
    const auto player = GameObjects::Player();
    const Vector3 cursor = Game::CursorPos();
    AIBaseClient best{};
    float bestScore = -FLT_MAX;
    auto consider = [&](const AIBaseClient& unit) {
        if (!ValidEscapeUnit(unit) ||
            IsUnderEnemyTurret(unit.Position()) ||
            HasReadyDashHazardAt(unit.Position())) {
            return;
        }
        if (unit.IsHero()) {
            const AIHeroClient hero(unit.Address());
            if (TargetRejectsHeadbutt(hero)) return;
        }
        float score = player.Position().Distance2D(cursor) -
                      unit.Position().Distance2D(cursor);
        score += static_cast<float>(
            Engine::CountEnemiesAt(player.Position(), 650.0f) -
            Engine::CountEnemiesAt(unit.Position(), 650.0f)) * 210.0f;
        if (pursuer.IsValid()) {
            score += unit.Position().Distance2D(pursuer.Position()) -
                     player.Position().Distance2D(pursuer.Position());
        }
        if (score > bestScore) {
            best = unit;
            bestScore = score;
        }
    };
    for (const auto& minion : GameObjects::EnemyLaneMinions()) {
        consider(AIBaseClient(minion.Handle()));
    }
    for (const auto& monster : GameObjects::Jungle()) {
        consider(AIBaseClient(monster.Handle()));
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        consider(AIBaseClient(enemy.Handle()));
    }
    return bestScore >=
        static_cast<float>(Slider(HeadbuttMenu, "EscapeMinimumGain", 130))
        ? best
        : AIBaseClient{};
}

inline bool TryFlee(const AIHeroClient& selected) {
    CurrentPosture = Posture::Escape;
    AIHeroClient pursuer = NearestEnemyToPlayer(selected, 1100.0f);
    const auto player = GameObjects::Player();
    if (Engine::ValidEnemy(pursuer) &&
        player.Position().Distance2D(pursuer.Position()) <=
            kQRadius + pursuer.BoundingRadius() &&
        Bool(PulverizeMenu, "FleeQ", true) &&
        CastQ(Mode::Flee, pursuer, true, true)) {
        return true;
    }
    if (TryRDefense(Mode::Flee)) return true;
    if (Bool(HeadbuttMenu, "EscapeW", true)) {
        const AIBaseClient unit = BestEscapeHeadbuttUnit(pursuer);
        if (unit.IsValid() &&
            CastW(unit, HeadbuttPurpose::Escape,
                  Mode::Flee, false, true)) {
            return true;
        }
    }
    if (Engine::ValidEnemy(pursuer) &&
        TryStartEAtContact(pursuer, Mode::Flee, true)) {
        return true;
    }
    return false;
}

inline bool TryCloseQInsec(const AIHeroClient& target) {
    if (!Bool(HeadbuttMenu, "UseInsec", true) ||
        !Engine::ValidEnemy(target)) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (player.Position().Distance2D(target.Position()) >
        kQRadius + target.BoundingRadius()) {
        return false;
    }
    const Vector3 goal = AlliedDisplacementGoal(target);
    if (!goal.IsValid() || goal.IsZero()) return false;
    const KnockbackPlan current = BuildKnockbackPlan(target, false);
    const float gain = TowardPointGain(
        current.TargetAtImpact, current.EffectiveEndpoint, goal);
    const bool alreadyAligned = gain >=
        static_cast<float>(Slider(HeadbuttMenu, "InsecMinimumGain", 260));
    if (alreadyAligned) {
        return CastW(target, HeadbuttPurpose::Insec,
                     Mode::Combo, false);
    }
    if (CastQ(Mode::Combo, target)) {
        InsecTargetId = static_cast<int>(target.NetworkId());
        InsecExpireTick = Now() + kInsecWindowMs;
        ActiveSequence = Sequence::QRepositionInsec;
        return true;
    }
    return false;
}

inline bool TryCombo(const AIHeroClient& selected) {
    AIHeroClient target = Engine::ValidEnemy(selected)
        ? selected
        : Engine::SelectTarget(kWRange + 120.0f);
    if (!Engine::ValidEnemy(target)) return false;
    CurrentPosture = IsUnderEnemyTurret(target.Position())
        ? Posture::Dive
        : Posture::Engage;
    if (CurrentPosture == Posture::Dive) ActiveSequence = Sequence::TurretDive;

    if (TryAoeQ(Mode::Combo)) return true;
    if (TryInsec(target)) return true;

    const KnockbackPlan full = BuildKnockbackPlan(target, false);
    const Vector3 goal = AlliedDisplacementGoal(target);
    const float insecGain = goal.IsValid()
        ? TowardPointGain(full.TargetAtImpact, full.EffectiveEndpoint, goal)
        : -FLT_MAX;
    if (insecGain >=
        static_cast<float>(Slider(HeadbuttMenu, "InsecMinimumGain", 260)) &&
        CastW(target, HeadbuttPurpose::Insec,
              Mode::Combo, false)) {
        return true;
    }
    if (TryWallPin(target)) return true;
    if (TryStandardWQ(target)) return true;
    if (TryCloseQInsec(target)) return true;
    if (TryStartEAtContact(target, Mode::Combo)) return true;
    return false;
}

inline bool TryHarass(const AIHeroClient& selected) {
    if (!Engine::ValidEnemy(selected)) return false;
    const auto player = GameObjects::Player();
    if (Engine::ManaPercent(player) <
        static_cast<float>(Slider(RoleMenu, "HarassMana", 52))) {
        return false;
    }
    // One-trick lane pattern: Q-E short trade and retain W to peel/exit.
    if (player.Position().Distance2D(selected.Position()) <=
            kQRadius + selected.BoundingRadius() &&
        CastQ(Mode::Harass, selected)) {
        return true;
    }
    if (TryStartEAtContact(selected, Mode::Harass)) return true;

    // Full W is allowed only when it throws the target under allied control;
    // ordinary WQ poke burns both peel cooldowns and is intentionally absent.
    const Vector3 goal = AlliedDisplacementGoal(selected);
    const KnockbackPlan plan = BuildKnockbackPlan(selected, false);
    if (goal.IsValid() &&
        TowardPointGain(plan.TargetAtImpact, plan.EffectiveEndpoint, goal) >=
            static_cast<float>(Slider(HeadbuttMenu, "HarassWMinimumGain", 380))) {
        return CastW(selected, HeadbuttPurpose::Insec,
                     Mode::Harass, false);
    }
    return false;
}

inline bool TryJungle() {
    if (!Bool(FarmMenu, "JungleAbilities", false) ||
        Engine::CountEnemiesAt(GameObjects::Player().Position(), 1200.0f) > 0) {
        return false;
    }
    AIBaseClient best{};
    float bestHealth = 0.0f;
    for (const auto& monster : GameObjects::Jungle()) {
        if (!monster.IsValid() || monster.IsDead() ||
            !monster.IsTargetable() || monster.Health() <= bestHealth ||
            monster.DistanceToPlayer() > kQRadius + 80.0f) {
            continue;
        }
        best = AIBaseClient(monster.Handle());
        bestHealth = monster.Health();
    }
    if (!best.IsValid()) return false;
    if (Bool(FarmMenu, "JungleE", true) &&
        Engine::RuntimeSpells[2] && Engine::RuntimeSpells[2]->IsReady() &&
        SpellEnabled(2, Mode::LaneClear) && CastThrottleReady(2) &&
        Engine::ControllerCastSelf(2)) {
        EActive = true;
        ECastTick = Now();
        EExpireTick = ECastTick + kEDurationMs;
        return true;
    }
    return Bool(FarmMenu, "JungleQ", false) &&
           CastQ(Mode::LaneClear, {}, false);
}

inline AIHeroClient BestQVictim() {
    const auto player = GameObjects::Player();
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            player.Position().Distance2D(enemy.Position()) >
                kQRadius + enemy.BoundingRadius()) {
            continue;
        }
        float score = (100.0f - enemy.HealthPercent()) * 2.0f +
                      enemy.TotalAttackDamage() * 0.35f + enemy.AP() * 0.25f;
        if (enemy.NetworkId() == Engine::LockedTargetNetworkId) score += 140.0f;
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    return best;
}

inline void RefreshState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();

    const bool liveE = player.HasBuff("AlistarE") ||
                       player.HasBuff("alistarE");
    const bool liveEAttack = player.HasBuff("AlistarEAttack") ||
                             player.HasBuff("alistareattack");
    if (liveE) EActive = true;
    EAttackReady = liveEAttack || EObservedStacks >= 5;
    const int buffStacks = ControllerHelpers::MaximumBuffCount(
        player, { "AlistarE", "alistarE" });
    if (buffStacks > 0) EObservedStacks = std::clamp(buffStacks, 0, 5);
    if (EActive && buffStacks <= 0 && ECastTick > 0) {
        bool contact = false;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (Engine::ValidEnemy(enemy) &&
                player.Position().Distance2D(enemy.Position()) <=
                    kERadius + enemy.BoundingRadius()) {
                contact = true;
                break;
            }
        }
        if (contact) {
            EObservedStacks = std::max(
                EObservedStacks,
                TrampleStacksFromContinuousContact(
                    static_cast<float>(now - ECastTick) / 1000.0f));
        }
    }
    if (!liveE && EActive && now > EExpireTick) EActive = false;
    if (!liveEAttack && !EActive && now > EEmpowerExpireTick) {
        EAttackReady = false;
        EObservedStacks = 0;
        EStunTargetId = 0;
        ClearForcedStunTarget();
    }

    RActive = player.HasBuff("FerociousHowl") ||
              player.HasBuff("ferocioushowl") ||
              (RActive && now <= RExpireTick);
    if (RActive && RExpireTick <= now) RExpireTick = now + 250;
    if (RActive && now > RCastTick + kRDurationMs + 250) RActive = false;

    PassiveStacks = std::clamp(
        ControllerHelpers::MaximumBuffCount(
            player, { "AlistarPassiveStacks", "alistarpassivestacks" }),
        0, 7);

    if (WDashActive && !player.IsDashing() &&
        now > WExpectedContactTick + 120) {
        WDashActive = false;
    }
    if (now > WSequenceExpireTick + 800 &&
        ActiveSequence != Sequence::QRepositionInsec &&
        ActiveSequence != Sequence::TrampleFourStack &&
        ActiveSequence != Sequence::TrampleStunReady) {
        WQBufferWanted = false;
        WQBuffered = false;
        CurrentHeadbuttPurpose = HeadbuttPurpose::None;
        ActiveSequence = Sequence::None;
    }
    if (now > InsecExpireTick) {
        InsecTargetId = 0;
        InsecGoal = {};
        InsecCoachPoint = {};
        if (ActiveSequence == Sequence::QRepositionInsec) {
            ActiveSequence = Sequence::None;
        }
    }
    if (now > TargetedAllyThreatUntil) TargetedAllyThreatId = 0;
    if (now > IncomingThreatUntil) RecentIncomingDamage = 0.0f;
    if (now > TurretAggroUntil) TurretShotsObserved = 0;
    if (now > InterruptExpireTick) InterruptTargetId = 0;
}

inline Posture ChoosePosture(Mode mode,
                             const AIHeroClient& target,
                             const AIHeroClient& ally,
                             const AIHeroClient& threat) {
    if (mode == Mode::Flee) return Posture::Escape;
    if (Engine::ValidAlly(ally) && Engine::ValidEnemy(threat) &&
        Bool(RoleMenu, "ProtectCarry", true)) {
        return Posture::Peel;
    }
    if (ActiveSequence == Sequence::QRepositionInsec &&
        Now() <= InsecExpireTick) {
        return Posture::Insec;
    }
    if (Engine::ValidEnemy(target) &&
        IsUnderEnemyTurret(target.Position())) {
        return Posture::Dive;
    }
    if (Engine::CountEnemiesAt(GameObjects::Player().Position(),
                           kQRadius + 65.0f) >= 2) {
        return Posture::Disrupt;
    }
    return mode == Mode::Combo ? Posture::Engage : Posture::Neutral;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshState();
    const AIHeroClient ally = SelectProtectedAlly();
    ProtectedAllyId = ally.IsValid() ? static_cast<int>(ally.NetworkId()) : 0;
    const AIHeroClient peelThreat = SelectPeelThreat(ally);
    PeelThreatId = peelThreat.IsValid()
        ? static_cast<int>(peelThreat.NetworkId())
        : 0;
    CurrentPosture = ChoosePosture(
        mode, selected, ally, peelThreat);

    if (WQBufferWanted && !WQBuffered && TryBufferQ(mode)) return true;
    if (TryTimedWallFollowup()) return true;
    if (TryRDefense(mode)) return true;
    if (mode == Mode::Flee) {
        (void)TryFlee(selected);
        return true;
    }
    if (TryInterrupt()) return true;
    if (TryPrimeEStun(selected, ally, peelThreat)) return true;
    if (Bool(RoleMenu, "ProtectCarry", true) &&
        TryPeel(ally, peelThreat)) {
        return true;
    }
    if (TryPassiveHealSetup(selected)) return true;

    if (mode == Mode::Combo) {
        (void)TryCombo(selected);
    } else if (mode == Mode::Harass) {
        (void)TryHarass(selected);
    } else if (mode == Mode::LaneClear || mode == Mode::Jungle) {
        (void)TryJungle();
    }
    return true;
}

inline bool ManualWShouldBuffer(const AIHeroClient& target) {
    if (!Bool(HeadbuttMenu, "AssistManualWQ", true) ||
        !Engine::ValidEnemy(target) ||
        !Engine::RuntimeSpells[0] || !Engine::RuntimeSpells[0]->IsReady()) {
        return false;
    }
    const KnockbackPlan plan = BuildKnockbackPlan(target, false, WCastOrigin);
    if (plan.PinsToWall) return false;
    const AIHeroClient ally = ProtectedAlly();
    if (ally.IsValid() &&
        PeelSeparationGain(ally.Position(), plan.TargetAtImpact,
                           plan.EffectiveEndpoint) >=
            static_cast<float>(Slider(HeadbuttMenu, "PeelMinimumGain", 170))) {
        return false;
    }
    const Vector3 goal = AlliedDisplacementGoal(target);
    if (goal.IsValid() &&
        TowardPointGain(plan.TargetAtImpact, plan.EffectiveEndpoint, goal) >=
            static_cast<float>(Slider(HeadbuttMenu, "InsecMinimumGain", 260))) {
        return false;
    }
    return CountAlliedFollowup(target.Position(), 875.0f) >= 1 ||
           SoloKillable(target);
}

inline void ObserveLocalW(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    WCastOrigin = args.StartPosition.IsValid() && !args.StartPosition.IsZero()
        ? args.StartPosition
        : player.Position();
    WCastTick = Now();
    WTargetId = static_cast<int>(args.TargetNetworkId != 0
        ? args.TargetNetworkId
        : args.Target.NetworkId);
    const AIBaseClient target = UnitByNetworkId(WTargetId);
    if (target.IsValid()) {
        const int contactMs = static_cast<int>(std::ceil(
            HeadbuttTravelSeconds(
                WCastOrigin.Distance2D(target.Position()),
                player.BoundingRadius(), target.BoundingRadius()) * 1000.0f));
        WExpectedContactTick = WCastTick + std::max(20, contactMs);
        WSequenceExpireTick = WExpectedContactTick + kWStateGraceMs;
        WDashActive = true;
    }

    if (!Engine::WasControllerCast(1) && target.IsValid() && target.IsHero()) {
        const AIHeroClient hero(target.Address());
        WQBufferWanted = ManualWShouldBuffer(hero);
        WQBuffered = false;
        CurrentHeadbuttPurpose = WQBufferWanted
            ? HeadbuttPurpose::BufferedEngage
            : HeadbuttPurpose::None;
        if (WQBufferWanted) {
            // Cast inside the event boundary so the engine's manual-input
            // arbitration does not make a legitimate W-Q buffer arrive late.
            (void)TryBufferQ(Mode::Combo);
        }
    }
}

inline void ObserveEnemyCast(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (args.Sender.Type ==
            ::Core::Objects::ObjectType::AITurretClient &&
        args.IsAutoAttack &&
        args.TargetNetworkId == static_cast<std::uint32_t>(player.NetworkId())) {
        TurretAggroUntil = Now() + 1800;
        ++TurretShotsObserved;
        RecentIncomingDamage = std::max(
            RecentIncomingDamage, player.MaxHealth() * 0.18f);
        IncomingThreatUntil = Now() + 1800;
        return;
    }

    const auto analysis = AnalyzeEnemyCast(args, 220.0f, 115.0f);
    if (!analysis.Valid) return;
    const AIHeroClient enemy = analysis.Enemy;
    const std::uint32_t targetId = args.TargetNetworkId != 0
        ? args.TargetNetworkId
        : args.Target.NetworkId;
    if (targetId != 0 &&
        targetId == static_cast<std::uint32_t>(ProtectedAllyId)) {
        TargetedAllyThreatId = static_cast<int>(enemy.NetworkId());
        TargetedAllyThreatUntil = Now() + 1100;
    }
    if (analysis.TargetsPlayer || analysis.CrossesPlayer) {
        float damage = 0.0f;
        if (args.IsAutoAttack) {
            damage = SDK::Damage::GetAutoAttackDamage(enemy, player, true);
        } else if (args.Slot >= 0 && args.Slot < 4) {
            damage = SDK::Damage::GetSpellDamage(
                enemy, player, Engine::SlotFromIndex(args.Slot),
                SDK::DamageStage::Default);
        }
        if (!std::isfinite(damage) || damage <= 0.0f) {
            damage = 70.0f + enemy.TotalAttackDamage() * 0.65f +
                     enemy.AP() * 0.35f;
        }
        if (Now() > IncomingThreatUntil) RecentIncomingDamage = 0.0f;
        RecentIncomingDamage += damage;
        IncomingThreatUntil = Now() + 1250;
    }
    if (analysis.Committed) {
        CommittedEnemyId = static_cast<int>(enemy.NetworkId());
        CommittedEnemyUntil = analysis.CommitmentUntilTick;
    }
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (!IsLocalPlayer(args.Sender)) {
        ObserveEnemyCast(args);
        return;
    }

    const int now = Now();
    if (args.IsAutoAttack) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
        LastAutoTick = now;
        return;
    }
    if (args.Slot == 0 || Engine::TextContains(args.SpellName, "Pulverize")) {
        QCastTick = now;
        QPrimaryTargetId = 0;
        const AIHeroClient victim = BestQVictim();
        if (Engine::ValidEnemy(victim)) {
            QPrimaryTargetId = static_cast<int>(victim.NetworkId());
            if (!(CurrentHeadbuttPurpose == HeadbuttPurpose::BufferedEngage &&
                  now - WCastTick <= 800)) {
                InsecTargetId = QPrimaryTargetId;
                InsecExpireTick = now + kInsecWindowMs;
                ActiveSequence = Sequence::QRepositionInsec;
            }
        }
        if (WQBufferWanted && now - WCastTick <= 800) {
            WQBuffered = true;
            ActiveSequence = Sequence::BufferedWQ;
        }
    } else if (args.Slot == 1 ||
               Engine::TextContains(args.SpellName, "Headbutt")) {
        ObserveLocalW(args);
    } else if (args.Slot == 2 ||
               Engine::TextContains(args.SpellName, "AlistarE")) {
        EActive = true;
        EAttackReady = false;
        ECastTick = now;
        EExpireTick = now + kEDurationMs;
        EObservedStacks = 0;
    } else if (args.Slot == 3 ||
               Engine::TextContains(args.SpellName, "FerociousHowl")) {
        RActive = true;
        RCastTick = now;
        RExpireTick = now + kRDurationMs;
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick)) return;
    if (EAttackReady && LastAutoTargetId == EStunTargetId) {
        EAttackReady = false;
        EActive = false;
        EObservedStacks = 0;
        EStunTargetId = 0;
        ClearForcedStunTarget();
        ActiveSequence = Sequence::None;
    }
}

inline void UpdateLocalBuffState(const SDK::Events::BuffEventArgs& args,
                                 bool removed) {
    if (NameEquals(args.BuffName, "AlistarE")) {
        EActive = !removed;
        if (!removed) {
            ECastTick = ECastTick > 0 ? ECastTick : Now();
            EExpireTick = Now() + RemainingMilliseconds(
                args.EndTime, kEDurationMs, 0, 6000);
            EObservedStacks = std::clamp(args.Count, 0, 5);
        }
    } else if (NameEquals(args.BuffName, "AlistarEAttack")) {
        EAttackReady = !removed;
        EObservedStacks = removed ? 0 : 5;
        EEmpowerExpireTick = removed ? 0 :
            Now() + RemainingMilliseconds(
                args.EndTime, kEEmpowerDurationMs, 0, 7000);
        if (removed) ClearForcedStunTarget();
    } else if (NameEquals(args.BuffName, "AlistarPassiveStacks")) {
        PassiveStacks = removed ? 0 : std::clamp(args.Count, 0, 7);
    } else if (NameEquals(args.BuffName, "AlistarPassiveHeal")) {
        if (!removed) {
            PassiveLastProcTick = Now();
            PassiveCooldownUntil = Now() + 3000;
            PassiveStacks = 0;
        }
    } else if (NameEquals(args.BuffName, "FerociousHowl")) {
        RActive = !removed;
        if (!removed) {
            RCastTick = RCastTick > 0 ? RCastTick : Now();
            RExpireTick = Now() + RemainingMilliseconds(
                args.EndTime, kRDurationMs, 0, 8000);
        }
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    UpdateLocalBuffState(args, false);
    UltimateReason reason{};
    if (ShouldCleanseBuff(args, reason)) {
        (void)CastR(reason, Mode::Automatic, true);
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) UpdateLocalBuffState(args, false);
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) UpdateLocalBuffState(args, true);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || (!EActive && !EAttackReady)) return;
    const AIBaseClient target(args.Target.Handle());
    if (!target.IsHero()) {
        if (EAttackReady || EObservedStacks >= 4) {
            args.Process = false;
        }
        return;
    }
    const AIHeroClient hero(target.Address());
    if (EStunTargetId != 0 &&
        static_cast<int>(hero.NetworkId()) != EStunTargetId &&
        Bool(TrampleMenu, "HoldForPriorityChampion", true)) {
        args.Process = false;
        return;
    }
    if (!EAttackReady && EObservedStacks == 4 &&
        Bool(TrampleMenu, "FourStackAttack", true)) {
        const int elapsed = std::max(0, Now() - ECastTick);
        const int nextPulse = MillisecondsToNextTramplePulse(elapsed);
        const int impact = static_cast<int>(std::max(
            0.0f, SDK::Utils::AutoAttack::GetTimeToHit(args.Target)));
        const bool remains = hero.Position().Distance2D(
            GameObjects::Player().Position()) <=
            kERadius + hero.BoundingRadius();
        if (!FourStackAttackWillStun(
                EObservedStacks, nextPulse, impact, remains)) {
            args.Process = false;
        }
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick)) return;
    if (EAttackReady && LastAutoTargetId == EStunTargetId) {
        EAttackReady = false;
        EActive = false;
        EObservedStacks = 0;
        EStunTargetId = 0;
        ClearForcedStunTarget();
        ActiveSequence = Sequence::None;
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::Peel: return "peel";
    case Posture::Engage: return "engage";
    case Posture::Insec: return "insec";
    case Posture::Disrupt: return "disrupt";
    case Posture::Dive: return "dive";
    case Posture::Escape: return "escape";
    default: return "neutral";
    }
}

inline const char* HeadbuttName(HeadbuttPurpose purpose) {
    switch (purpose) {
    case HeadbuttPurpose::BufferedEngage: return "WQ";
    case HeadbuttPurpose::Peel: return "peel";
    case HeadbuttPurpose::Insec: return "insec";
    case HeadbuttPurpose::WallPin: return "wall";
    case HeadbuttPurpose::Interrupt: return "interrupt";
    case HeadbuttPurpose::Escape: return "escape";
    default: return "none";
    }
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (Bool(CoachMenu, "DrawQ", true)) {
        Drawing::DrawCircle(player.Position(), kQRadius,
                            0x887C63FFu, 1.5f, 64);
    }
    if (Bool(CoachMenu, "DrawKnockback", true) &&
        LastKnockbackPlan.TargetAtImpact.IsValid() &&
        Now() <= WSequenceExpireTick + 650) {
        Drawing::DrawLine(
            LastKnockbackPlan.TargetAtImpact,
            LastKnockbackPlan.EffectiveEndpoint,
            LastKnockbackPlan.PinsToWall ? 0xFFFFB347u : 0xFFB68CFFu,
            2.5f);
        Drawing::DrawCircle(
            LastKnockbackPlan.EffectiveEndpoint, 48.0f,
            LastKnockbackPlan.PinsToWall ? 0xFFFFB347u : 0xFFB68CFFu,
            2.0f, 40);
    }
    if (Bool(CoachMenu, "DrawInsec", true) &&
        InsecCoachPoint.IsValid() && Now() <= InsecExpireTick) {
        Drawing::DrawCircle(InsecCoachPoint, 55.0f,
                            0xFF55EEAAu, 2.5f, 48);
        if (InsecGoal.IsValid()) {
            Drawing::DrawLine(InsecCoachPoint, InsecGoal,
                              0xAA55EEAAu, 2.0f);
        }
    }
    if (Bool(CoachMenu, "DrawPeel", true)) {
        const AIHeroClient ally = ProtectedAlly();
        const AIHeroClient threat = HeroByNetworkId(PeelThreatId);
        if (ally.IsValid()) {
            Drawing::DrawCircle(ally.Position(), 110.0f,
                                0xAA55CCFFu, 2.0f, 48);
        }
        if (ally.IsValid() && threat.IsValid()) {
            Drawing::DrawLine(ally.Position(), threat.Position(),
                              0xFFFF6677u, 2.5f);
        }
    }
    if (Bool(CoachMenu, "DrawEStun", true) && EStunTargetId != 0) {
        const AIHeroClient target = HeroByNetworkId(EStunTargetId);
        if (target.IsValid()) {
            Drawing::DrawCircle(target.Position(),
                                target.BoundingRadius() + 55.0f,
                                0xFFFFE066u, 3.0f, 48);
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char state[256]{};
            _snprintf_s(
                state, sizeof(state), _TRUNCATE,
                "Alistar one-trick | %s | W %s%s | E %d/5%s | P %d/7 | R %s",
                PostureName(CurrentPosture),
                HeadbuttName(CurrentHeadbuttPurpose),
                WQBuffered ? "+Q" : "",
                EObservedStacks,
                EAttackReady ? " READY" : "",
                PassiveStacks,
                RActive ? "active" : "ready/hold");
            Drawing::DrawText(screen.x - 155.0f, screen.y - 115.0f,
                              0xFFEBD9FFu, state);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "AlistarOneTrick", "Alistar one-trick mechanics"));

    RoleMenu = TacticsMenu->AddSubMenu(new Menu(
        "Role", "Carry protection and engage consent"));
    RoleMenu->Add(new MenuBool("ProtectCarry", "Peel carry before dive", true));
    RoleMenu->Add(new MenuSlider("PeelThreatScore", "Minimum carry-threat score", 620, 250, 1100));
    RoleMenu->Add(new MenuSlider("MinimumFollowup", "Min allies for W-Q", 1, 0, 4));
    RoleMenu->Add(new MenuSlider("MaxEngageEnemies", "Max enemies at engage", 3, 1, 5));
    RoleMenu->Add(new MenuBool("RespectCursor", "Do not hard-engage opposite", true));
    RoleMenu->Add(new MenuBool("DoNotOverlapCC", "Wait instead of overlapping", true));
    RoleMenu->Add(new MenuBool("UseCCForPassiveHeal", "At 6 passive stacks, CC to", true));
    RoleMenu->Add(new MenuSlider("PassiveHealAllyHp", "Ally HP passive setup (%)", 42, 15, 75));
    RoleMenu->Add(new MenuSlider("HarassMana", "Harass minimum mana (%)", 52, 20, 90));

    HeadbuttMenu = TacticsMenu->AddSubMenu(new Menu(
        "Headbutt", "Headbutt displacement branches"));
    HeadbuttMenu->Add(new MenuBool("UseWQ", "Use buffered W-Q for", true));
    HeadbuttMenu->Add(new MenuBool("AssistManualWQ", "Buffer Q after manual W", true));
    HeadbuttMenu->Add(new MenuBool("PeelW", "W push diver away", true));
    HeadbuttMenu->Add(new MenuSlider("PeelMinimumGain", "Minimum peel separation gain", 170, 60, 500));
    HeadbuttMenu->Add(new MenuBool("UseWallPin", "Full W vs thick terrain", true));
    HeadbuttMenu->Add(new MenuSlider("WallMinTravel", "Min wall-push dist", 90, 20, 300));
    HeadbuttMenu->Add(new MenuSlider("WallMaxTravel", "Max wall-push distance", 520, 180, 700));
    HeadbuttMenu->Add(new MenuSlider("WallQDelay", "Q wall-contact delay (ms)", 470, 180, 650));
    HeadbuttMenu->Add(new MenuBool("UseInsec", "Q, reposition, then W toward", true));
    HeadbuttMenu->Add(new MenuSlider("InsecMinimumGain", "Min W teamward gain", 260, 100, 600));
    HeadbuttMenu->Add(new MenuSlider("InsecStandDistance", "Behind-target dist", 175, 120, 280));
    HeadbuttMenu->Add(new MenuSlider("InsecWalkDelay", "Earliest W after Q (ms)", 360, 180, 700));
    HeadbuttMenu->Add(new MenuSlider("HarassWMinimumGain", "Harass W teamward gain", 380, 150, 650));
    HeadbuttMenu->Add(new MenuBool("InterruptW", "W interrupt channels", true));
    HeadbuttMenu->Add(new MenuBool("EscapeW", "W flee cursor unit", true));
    HeadbuttMenu->Add(new MenuSlider("EscapeMinimumGain", "Minimum escape W route gain", 130, 30, 500));
    HeadbuttMenu->Add(new MenuBool("RespectDashHazards", "Avoid dash zones", true));

    PulverizeMenu = TacticsMenu->AddSubMenu(new Menu(
        "Pulverize", "Pulverize timing and reactions"));
    PulverizeMenu->Add(new MenuBool("PeelQ", "Q diver before W", true));
    PulverizeMenu->Add(new MenuBool("InterruptQ", "Q interrupt in radius", true));
    PulverizeMenu->Add(new MenuBool("FleeQ", "Q pursuers on flee", true));
    PulverizeMenu->Add(new MenuSlider("AoeEnemies", "Min enemies AoE Q", 2, 1, 5));
    PulverizeMenu->Add(new MenuSeparator(
        "BufferRule", "Buffered Q is immediate;"));

    TrampleMenu = TacticsMenu->AddSubMenu(new Menu(
        "Trample", "Trample pulse and empowered-AA ownership"));
    TrampleMenu->Add(new MenuBool("FourStackAttack", "Start AA at 4 stacks when", true));
    TrampleMenu->Add(new MenuBool("ForceBestStunTarget", "Force orbwalker only to the", true));
    TrampleMenu->Add(new MenuBool("HoldForPriorityChampion", "Block a lower-priority", true));
    TrampleMenu->Add(new MenuSeparator(
        "NoMinionWaste", "At 4/5 stacks, minion"));

    UltimateMenu = TacticsMenu->AddSubMenu(new Menu(
        "UnbreakableWill", "Cleanse and damage-reduction economy"));
    UltimateMenu->Add(new MenuBool("CleanseSuppression", "Immediately R a suppression", true));
    UltimateMenu->Add(new MenuBool("CleanseHardCC", "R hard CC under danger", true));
    UltimateMenu->Add(new MenuSlider("MinimumCCMs", "Min CC duration (ms)", 650, 250, 1400));
    UltimateMenu->Add(new MenuSlider("CleanseHp", "Hard-CC cleanse danger HP (%)", 58, 20, 90));
    UltimateMenu->Add(new MenuBool("CleanseAirborneInLethalDanger", "R airborne: lethal/turret", true));
    UltimateMenu->Add(new MenuSlider("BurstPercent", "R when recent burst is this", 42, 15, 90));
    UltimateMenu->Add(new MenuSlider("TankMinimumEnemies", "Min enemies tank R", 3, 1, 5));
    UltimateMenu->Add(new MenuSlider("TankHp", "Multi-enemy tank R HP (%)", 56, 20, 90));
    UltimateMenu->Add(new MenuBool("UseAfterTurretAggro", "Delay R dive for aggro", true));
    UltimateMenu->Add(new MenuSlider("TurretShotsBeforeR", "Turret attacks for R", 1, 1, 3));
    UltimateMenu->Add(new MenuSlider("TurretRHp", "Emergency dive R HP (%)", 72, 30, 95));
    UltimateMenu->Add(new MenuSlider("FleeHp", "Flee tank R HP (%)", 34, 10, 70));
    UltimateMenu->Add(new MenuSeparator(
        "TrueDamage", "R reduces physical/magic"));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Support-safe farming"));
    FarmMenu->Add(new MenuBool("JungleAbilities", "Abilities on jungle", false));
    FarmMenu->Add(new MenuBool("JungleE", "Use E on jungle when enabled", true));
    FarmMenu->Add(new MenuBool("JungleQ", "Use Q on jungle when enabled", false));
    FarmMenu->Add(new MenuSeparator(
        "NoLaneFarm", "Q/W/E never take lane minions."));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "One-trick visual coaching"));
    CoachMenu->Add(new MenuBool("DrawQ", "Draw Pulverize radius", false));
    CoachMenu->Add(new MenuBool("DrawKnockback", "Draw W landing", false));
    CoachMenu->Add(new MenuBool("DrawInsec", "Draw reposition/W goal", false));
    CoachMenu->Add(new MenuBool("DrawPeel", "Mark ally/diver", false));
    CoachMenu->Add(new MenuBool("DrawEStun", "Mark empow-AA target", false));
    CoachMenu->Add(new MenuBool("DrawState", "Draw state/branch", false));
}

inline void OnLoad() {
    ActiveSequence = Sequence::None;
    CurrentPosture = Posture::Neutral;
    CurrentHeadbuttPurpose = HeadbuttPurpose::None;
    LastUltimateReason = UltimateReason::None;
    WTargetId = WCastTick = WExpectedContactTick = WSequenceExpireTick = 0;
    WDashActive = WQBufferWanted = WQBuffered = false;
    WCastOrigin = {};
    LastKnockbackPlan = {};
    QCastTick = QPrimaryTargetId = InsecTargetId = InsecExpireTick = 0;
    InsecGoal = InsecCoachPoint = {};
    EActive = EAttackReady = false;
    ECastTick = EExpireTick = EEmpowerExpireTick = EObservedStacks = 0;
    EStunTargetId = ForcedStunTargetId = 0;
    PassiveStacks = PassiveLastProcTick = PassiveCooldownUntil = 0;
    PassiveHealAllyId = 0;
    RActive = false;
    RCastTick = RExpireTick = 0;
    ProtectedAllyId = PeelThreatId = TargetedAllyThreatId = 0;
    TargetedAllyThreatUntil = CommittedEnemyId = CommittedEnemyUntil = 0;
    IncomingThreatUntil = TurretAggroUntil = TurretShotsObserved = 0;
    RecentIncomingDamage = 0.0f;
    GapcloserTargetId = GapcloserExpireTick = InterruptTargetId = 0;
    GapcloserEnd = {};
    InterruptExpireTick = LastAutoTargetId = LastAutoTick = 0;
    RefreshState();
}

inline void OnUnload() {
    ClearForcedStunTarget();
    TacticsMenu = RoleMenu = HeadbuttMenu = PulverizeMenu = nullptr;
    TrampleMenu = UltimateMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Route every combat tick through peel, engage, insec, disrupt, dive or escape posture",
    "Select a protected ally from damage, range, health and nearby threat rather than nearest ally",
    "Abandon an engage when the protected carry is actively being dived",
    "Identify ally-targeted hostile casts even when the gapcloser event only reports player dashes",
    "Pulverize a close diver before spending Headbutt",
    "Use full Headbutt only when it increases separation from the protected ally",
    "Reject a peel Headbutt that would knock the diver closer to the carry",
    "Reserve empowered Trample AA for the highest-value peel threat",
    "Interrupt an in-range channel with Q",
    "Interrupt an out-of-Q channel with full W rather than a needless W-Q",
    "Buffer Q immediately after a supported standard Headbutt engage",
    "Model Headbutt travel from the exposed radius gap at base speed 1200",
    "Account for target and Alistar radii in W contact timing",
    "Model normal Headbutt as 700 displacement",
    "Model game-side W-Q buffer as 200 displacement",
    "Keep full W displacement for peel instead of blindly buffering Q",
    "Keep full W displacement for insec instead of blindly buffering Q",
    "Keep full W displacement for wall pin instead of blindly buffering Q",
    "Infer whether a manual W wants Q by checking peel, wall and insec value",
    "Issue manual-W Q inside the event window before manual-input arbitration delays it",
    "Reject W-Q without an allied follow-up unless Alistar's own conservative damage is lethal",
    "Reject an engage opposite the player's cursor",
    "Reject an over-numbered engage while R is unavailable",
    "Reject a turret engage without R and an ally able to follow",
    "Delay dive R until turret aggro is observed",
    "Count observed turret attacks before spending R",
    "Start R early only when current HP crosses the configured dive emergency threshold",
    "Use R for recent incoming burst relative to current health",
    "Use R as a multi-enemy tank window only below a configured HP threshold",
    "Never pretend R mitigates true damage",
    "Cast R directly from buff events because the normal update loop is disabled by CC",
    "Always permit suppression cleanse when configured",
    "Cleanse charm, taunt, fear, flee, sleep and polymorph under meaningful danger",
    "Cleanse long stun/root/silence only when danger or a critical combo justifies R",
    "Avoid wasting R on a trivial slow",
    "Avoid wasting R on ordinary airborne movement that it cannot cancel",
    "Allow airborne R only under lethal burst or turret danger",
    "Track the full seven-second Unbreakable Will window from the live buff",
    "Detect spell shields and spell immunity before W engage",
    "Reject parry, untargetable and narrow displacement-immune states",
    "Avoid ready Poppy, Taliyah and Cassiopeia dash hazards",
    "Find the first NavMesh wall along the 700-unit W displacement",
    "Measure wall thickness instead of treating every thin wall as a pin",
    "Stop the visual W endpoint before a wall that cannot be surpassed",
    "Start E on wall contact while retaining Q for a true CC chain",
    "Delay Q after wall contact so it lands as Headbutt disable expires",
    "Fire wall-chain Q early if the target starts escaping",
    "Cast close Q first when the player needs time to walk behind for an insec",
    "Draw the exact behind-target position required to W toward the team",
    "Require real displacement gain toward ally centroid or allied turret before insec W",
    "Continue after a player-created Q-Flash by observing Q and recomputing the W angle",
    "Allow W-Q-Flash relocation without ever casting Flash for the player",
    "Use Q AoE disrupt when multiple enemies enter the 375 effect radius",
    "Use the target bounding radius when deciding Q contact",
    "Start E only when a champion is in or stably entering the 350 pulse radius",
    "Track Trample from live AlistarE and AlistarEAttack buffs",
    "Fallback to deterministic 0.5-second pulse tracking when buff count is absent",
    "Cap Trample at five champion-contact stacks",
    "Begin an AA at four stacks when pulse five occurs before attack impact",
    "Block the four-stack AA when the target will leave Trample before pulse five",
    "Block minion attacks at four/five stacks so the champion stun is preserved",
    "Force only the chosen E-stun champion and release force-target after consumption",
    "Choose E stun target independently from original W-Q target",
    "Prefer an interrupting target, then a diver, then a high-value carry for E stun",
    "Use current 20 plus 15 per champion level empowered-E damage",
    "Use current Q 80 percent AP and W 100 percent AP data rather than stale SDK rows",
    "At six passive stacks, use safe champion CC to trigger a low-ally heal",
    "Track passive's seven-stack threshold and three-second post-heal cooldown",
    "Never invent an active cast for automatic Triumphant Roar",
    "Use full W on a minion, monster or champion aligned with the flee cursor",
    "Reject an escape W that ends under an enemy turret or in a dash hazard",
    "Q a close pursuer before choosing an escape Headbutt unit",
    "Use E for ghosted flee pressure only when a pursuer is actually in contact",
    "Preserve W as the disengage tool during an ordinary Q-E harass trade",
    "Permit harass W only when it produces a large displacement toward allied control",
    "Never cast lane-farm spells automatically",
    "Keep jungle spell usage explicitly opt-in",
    "Never issue movement or Flash input; expose reposition geometry to the player",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Alistar";
    controller.ControllerId = "champion.kuroaio.ai.alistar.onetrick";
    controller.KitRevision = "League 26.14 / CommunityDragon 16.14";
    controller.ResearchArtifact = "AI/Research/AIAlistar.md";
    controller.ImplementationSummary =
        "Peel-first six-posture controller, radius-correct W-Q buffering, "
        "full-displacement wall/peel/insec branches, ally follow-up and turret "
        "coordination, pulse-accurate four-stack E AA priming, passive-heal "
        "setup, and buff-event R cleanse/tank economy.";
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
            &GapcloserExpireTick, 500, 800>;
    controller.OnInterruptable =
        &ControllerHelpers::CaptureInterruptableEvent<
            &InterruptTargetId, &InterruptExpireTick, 900, 120, 2200>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Alistar
