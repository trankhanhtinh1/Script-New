#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIAkshanGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Akshan {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::AutoAttackRange;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::NameEquals;
using ControllerHelpers::NearTerrain;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::UnitByNetworkId;
using ControllerHelpers::ValidHostileUnit;

// The controller intentionally models player-facing states rather than a
// Q-W-E-R priority list.  Akshan's next correct action depends on whether his
// second shot is still cancellable, which Q leg is live, whether E is a hook,
// orbit or dismount, and how many R bullets survive the current blocker line.
enum class Sequence : int {
    None,
    PreserveSecondShot,
    CancelSecondShotForSpeed,
    ExtendedQ,
    QReturnCoach,
    HookPending,
    DamageSwing,
    EscapeSwing,
    RAngleSwing,
    ResetSwing,
    ComeuppanceChannel,
    ScoundrelCleanup,
};

enum class Posture : int {
    Neutral,
    Trade,
    MarksmanDps,
    Assassinate,
    ScoundrelCleanup,
    Channeling,
    Escape,
};

enum class PassiveShotPolicy : int {
    Neutral,
    PreserveDamage,
    PreserveProc,
    PreserveLastHit,
    CancelForSpeed,
    CancelForSafety,
};

enum class EPhase : int {
    Ready,
    HookAttached,
    Swinging,
    Cooldown,
};

enum class SwingPurpose : int {
    None,
    Damage,
    Escape,
    RReposition,
    ResetCleanup,
    Jungle,
};

enum class RReleaseReason : int {
    None,
    Lethal,
    TargetEnteringCover,
    BlockerCleared,
    FullMagazine,
    Manual,
};

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* RogueMenu = nullptr;
inline Menu* SwingMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Sequence ActiveSequence = Sequence::None;
inline Posture CurrentPosture = Posture::Neutral;
inline PassiveShotPolicy CurrentShotPolicy = PassiveShotPolicy::Neutral;
inline SwingPurpose ActiveSwingPurpose = SwingPurpose::None;
inline RReleaseReason LastRReleaseReason = RReleaseReason::None;

inline int LastPrimaryAttackTick = 0;
inline int LastSecondShotTick = 0;
inline int LastAutoTargetId = 0;
inline int PendingSecondShotTargetId = 0;
inline int PendingSecondShotUntil = 0;
inline bool PendingSecondShot = false;
inline int LastCombatTick = 0;
inline int LastDamagedTargetId = 0;
inline int LastDamagedTargetUntil = 0;

inline bool QActive = false;
inline bool QReturning = false;
inline int QCastTick = 0;
inline int QLastSeenTick = 0;
inline int QMissileNetworkId = 0;
inline int QTargetId = 0;
inline int LastQExtensionHits = 0;
inline Vector3 QCastOrigin = {};
inline Vector3 QPlannedEnd = {};
inline Vector3 QMissilePosition = {};
inline Vector3 QReturnCoachPoint = {};
inline float QReturnCoachScore = 0.0f;

inline bool WCamouflaged = false;
inline int WCastTick = 0;
inline int WLastSeenTick = 0;
inline int ScoundrelTargetId = 0;
inline int ScoundrelVictimCount = 0;

inline EPhase CurrentEPhase = EPhase::Cooldown;
inline Vector3 PlannedAnchor = {};
inline Vector3 PlannedE2Cursor = {};
inline Vector3 PlannedDismount = {};
inline SwingDirection PlannedSwingDirection = SwingDirection::Clockwise;
inline int PlannedSwingTargetId = 0;
inline int E1CastTick = 0;
inline int E2CastTick = 0;
inline int E3CastTick = 0;
inline int E3UnlockTick = 0;
inline int PlannedDismountTick = 0;
inline int PlannedSwingShots = 0;
inline float PlannedSwingScore = -FLT_MAX;

inline bool RChannelActive = false;
inline bool RReleaseIssued = false;
inline int RChannelStartTick = 0;
inline int RTargetId = 0;
inline int RObservedBullets = 0;
inline int RLastBlockerId = 0;
inline int RMinionBulletsLost = 0;
inline int RReleaseTick = 0;
inline Vector3 RTrackedTargetPosition = {};
inline Vector3 RLastOpenSource = {};

inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};
inline int CommittedEnemyId = 0;
inline int CommittedEnemyUntil = 0;
inline int IncomingLineThreatUntil = 0;
inline int IncomingHardCCUntil = 0;

inline constexpr float kQCastRange = 850.0f;
inline constexpr float kQScriptRange = 750.0f;
inline constexpr float kQExtension = 500.0f;
inline constexpr float kQWidth = 70.0f;
inline constexpr float kQOutboundSpeed = 1500.0f;
inline constexpr float kQReturnSpeed = 2400.0f;
inline constexpr float kERange = 800.0f;
inline constexpr float kESwingSpeed = 1200.0f;
inline constexpr float kE3Range = 350.0f;
inline constexpr float kEShotSearchRange = 800.0f;
inline constexpr float kRRange = 2500.0f;
inline constexpr float kRMissileWidth = 40.0f;
inline constexpr int kPassiveDebuffMs = 4500;
inline constexpr int kEDismountLockMs = 500;
inline constexpr int kRChannelMs = 2500;

inline bool CastThrottleReady(int index, bool fastFollowup = false) {
    return ControllerHelpers::CastThrottleReady(
        index, 50, fastFollowup ? 16 : -1);
}

inline bool EHookAttached() {
    return RuntimeNameContains(2, "AkshanE2");
}

inline bool ESwinging() {
    return RuntimeNameContains(2, "AkshanE3");
}

inline bool EFirstCastReady() {
    return Engine::RuntimeSpells[2] && Engine::RuntimeSpells[2]->IsReady() &&
           !EHookAttached() && !ESwinging();
}

inline bool RReleaseAvailable() {
    return RuntimeNameContains(3, "AkshanRCancel");
}

inline bool TargetCannotBeAttacked(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzE") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("EliseSpiderE") || target.HasBuff("zhonyasringshield") ||
           target.HasBuff("BardRStasis") || target.HasBuff("KayleR") ||
           target.HasBuff("kindredrnodeathbuff") ||
           target.HasBuff("ChronoShift");
}

inline bool HasDashStopperAt(const Vector3& position) {
    return ControllerHelpers::HasReadyDashHazardAt(position);
}

inline bool SafePoint(const Vector3& position,
                      const AIHeroClient& target,
                      bool lethal,
                      bool escape,
                      int enemyAllowance = -1) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !position.IsValid() || position.IsZero() ||
        SDK::NavMesh::IsWall(position)) {
        return false;
    }
    const int maximumEnemies = enemyAllowance >= 0
        ? enemyAllowance
        : Slider(SwingMenu, "MaxCommitEnemies", 2);
    const int enemies = Engine::CountEnemiesAt(position, 650.0f);
    const int allies = Engine::CountAlliesAt(position, 700.0f);
    if (!lethal && enemies > std::max(1, maximumEnemies) &&
        allies + (escape ? 1 : 0) < enemies) {
        return false;
    }
    if (!escape && Engine::UnderEnemyTurret(position) &&
        !(lethal && Bool(SwingMenu, "AllowLethalDive", false))) {
        return false;
    }
    if (!escape && Bool(SwingMenu, "RespectLockdown", true) &&
        ControllerHelpers::HasReadyPointClickThreatAt(position)) {
        return false;
    }
    if (!lethal && HasDashStopperAt(position)) return false;
    if (target.IsValid() && !escape &&
        position.Distance2D(target.Position()) < 130.0f &&
        player.HealthPercent() < 55.0f) {
        return false;
    }
    return true;
}

inline int PassiveStacks(const AIBaseClient& target) {
    return ControllerHelpers::MaximumBuffCount(
        target, { "AkshanPassiveDebuff", "akshanpassivedebuff" });
}

inline int ScoundrelStacks(const AIHeroClient& target) {
    return ControllerHelpers::MaximumBuffCount(
        target, { "AkshanWHuntMark", "akshanwhuntmark" });
}

inline bool HasPriorityMark(const AIBaseClient& target) {
    if (!target.IsValid()) return false;
    const int id = static_cast<int>(target.NetworkId());
    return PassiveStacks(target) > 0 ||
           (id != 0 && id == LastDamagedTargetId &&
            Now() <= LastDamagedTargetUntil);
}

inline float TotalCritMultiplier(const AIHeroClient& player) {
    // Patch 26.1 raised base crit damage to 200%; current Infinity Edge adds
    // 30% bonus crit damage.  Akshan E/R consume only a fraction of that bonus.
    return player.HasItem(3031) ? 2.30f : 2.00f;
}

inline float QDamage(const AIBaseClient& target, bool nonChampion = false) {
    const auto player = ObjectManager::Player();
    const int rank = SpellRank(0);
    if (!player.IsValid() || !target.IsValid() || rank <= 0) return 0.0f;
    static constexpr float bases[] = {
        0.0f, 45.0f, 75.0f, 105.0f, 135.0f, 165.0f
    };
    float raw = bases[std::clamp(rank, 0, 5)] +
                0.70f * std::max(0.0f, player.BonusAttackDamage());
    if (nonChampion) {
        static constexpr float modifiers[] = {
            0.0f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f
        };
        raw *= modifiers[std::clamp(rank, 0, 5)];
    }
    return player.CalculatePhysicalDamage(target, raw);
}

inline float PassiveProcDamage(const AIBaseClient& target) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    float base = 15.0f;
    if (player.Level() >= 16) base = 150.0f;
    else if (player.Level() >= 11) base = 80.0f;
    else if (player.Level() >= 6) base = 40.0f;
    return player.CalculateMagicDamage(
        target, base + 0.60f * std::max(0.0f, player.AP()));
}

inline float PassiveSecondShotDamage(const AIBaseClient& target) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const float expectedCrit = 1.0f + std::clamp(player.Crit(), 0.0f, 1.0f) *
        std::max(0.0f, TotalCritMultiplier(player) - 1.0f);
    const bool champion = Engine::ValidEnemy(HeroByNetworkId(
        static_cast<int>(target.NetworkId())));
    const float attackDamageRatio = champion ? 0.50f : 1.00f;
    return player.CalculatePhysicalDamage(
        target, attackDamageRatio *
            std::max(0.0f, player.TotalAttackDamage()) * expectedCrit);
}

inline float EShotDamage(const AIBaseClient& target) {
    const auto player = ObjectManager::Player();
    const int rank = SpellRank(2);
    if (!player.IsValid() || !target.IsValid() || rank <= 0) return 0.0f;
    static constexpr float bases[] = {
        0.0f, 8.0f, 16.0f, 24.0f, 32.0f, 40.0f
    };
    const float bonusAttackSpeed = std::max(0.0f, player.AttackSpeedMod() - 1.0f);
    const float expectedCrit = 1.0f + std::clamp(player.Crit(), 0.0f, 1.0f) *
        0.50f * std::max(0.0f, TotalCritMultiplier(player) - 1.0f);
    const float raw = (bases[std::clamp(rank, 0, 5)] +
        0.25f * std::max(0.0f, player.TotalAttackDamage())) *
        (1.0f + 0.30f * bonusAttackSpeed) * expectedCrit;
    return player.CalculatePhysicalDamage(target, raw);
}

inline bool LethalWith(const AIHeroClient& target, float damage,
                       float margin = 0.94f) {
    return Engine::ValidEnemy(target) && damage > 0.0f &&
           damage * margin >= target.Health() + target.AllShield();
}

inline PassiveShotPolicy ChooseSecondShotPolicy(const AIBaseClient& target,
                                                Mode mode) {
    const int now = Now();
    if (!target.IsValid() || mode == Mode::Flee ||
        IncomingHardCCUntil > now || IncomingLineThreatUntil > now) {
        return mode == Mode::Flee
            ? PassiveShotPolicy::CancelForSpeed
            : PassiveShotPolicy::CancelForSafety;
    }
    const AIHeroClient hero = HeroByNetworkId(
        static_cast<int>(target.NetworkId()));
    if (Engine::ValidEnemy(hero)) {
        if (PassiveStacks(target) >= 2) return PassiveShotPolicy::PreserveProc;
        if (mode == Mode::Combo || mode == Mode::Harass) {
            return PassiveShotPolicy::PreserveDamage;
        }
    }
    if ((mode == Mode::LaneClear || mode == Mode::LastHit) &&
        target.Health() <= PassiveSecondShotDamage(target) * 1.08f) {
        return PassiveShotPolicy::PreserveLastHit;
    }
    return PassiveShotPolicy::CancelForSpeed;
}

inline bool HoldPendingSecondShot() {
    if (!PendingSecondShot || Now() > PendingSecondShotUntil) return false;
    return CurrentShotPolicy == PassiveShotPolicy::PreserveDamage ||
           CurrentShotPolicy == PassiveShotPolicy::PreserveProc ||
           CurrentShotPolicy == PassiveShotPolicy::PreserveLastHit;
}

inline AIHeroClient FindScoundrelTarget(float range = kRRange) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return {};
    AIHeroClient best = {};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range)) continue;
        const int victims = ScoundrelStacks(enemy);
        if (victims <= 0) continue;
        float score = static_cast<float>(victims) * 520.0f;
        score += (100.0f - enemy.HealthPercent()) * 7.0f;
        score -= player.Position().Distance2D(enemy.Position()) * 0.16f;
        score -= static_cast<float>(Engine::CountEnemiesAt(enemy.Position(), 700.0f)) *
                 260.0f;
        score += static_cast<float>(Engine::CountAlliesAt(enemy.Position(), 750.0f)) *
                 150.0f;
        if (ControllerHelpers::HasReadyPointClickThreatAt(enemy.Position())) {
            score -= 460.0f;
        }
        if (score > bestScore) {
            bestScore = score;
            best = enemy;
        }
    }
    return best;
}

inline Posture DeterminePosture(const AIHeroClient& selected) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return Posture::Neutral;
    if (Engine::CurrentMode() == Mode::Flee ||
        player.HealthPercent() <= Slider(SwingMenu, "EscapeHP", 28)) {
        return Posture::Escape;
    }
    if (RChannelActive || RReleaseAvailable()) return Posture::Channeling;
    const AIHeroClient scoundrel = FindScoundrelTarget();
    if (Engine::ValidEnemy(scoundrel) && ScoundrelStacks(scoundrel) > 0 &&
        scoundrel.HealthPercent() <= Slider(RogueMenu, "CleanupHP", 52)) {
        return Posture::ScoundrelCleanup;
    }
    if (Engine::ValidEnemy(selected)) {
        if (selected.HealthPercent() <= Slider(SwingMenu, "AssassinateHP", 58)) {
            return Posture::Assassinate;
        }
        if (InAutoAttackRange(selected, 30.0f)) return Posture::MarksmanDps;
        if (Engine::CurrentMode() == Mode::Harass) return Posture::Trade;
    }
    return Posture::Neutral;
}

inline bool IsQMissileName(const char* spellName, const char* missileName) {
    return ControllerHelpers::TextContainsAny(
               missileName, { "AkshanQMissile", "AkshanQMis" }) ||
           ControllerHelpers::TextContainsAny(
               spellName, { "AkshanQMissile", "AkshanQ" });
}

inline bool IsQReturnName(const char* spellName, const char* missileName) {
    return ControllerHelpers::AnyTextContains(
        { spellName, missileName },
        { "AkshanQMissileReturn", "return" });
}

inline void RefreshTrackedQ() {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    bool found = false;
    for (const auto& missile : GameObjects::Missiles()) {
        if (!missile.IsValid() ||
            missile.CasterNetworkId() != player.NetworkId()) {
            continue;
        }
        const std::string spellName = missile.SpellName();
        const std::string missileName = missile.MissileName();
        if (!IsQMissileName(spellName.c_str(), missileName.c_str())) continue;
        found = true;
        QActive = true;
        QReturning = IsQReturnName(spellName.c_str(), missileName.c_str());
        QMissileNetworkId = missile.NetworkId();
        QMissilePosition = missile.Position();
        QLastSeenTick = Now();
    }
    if (!found && QActive && QLastSeenTick > 0 &&
        Now() - QLastSeenTick > 180 && QCastTick > 0 &&
        Now() - QCastTick > 500) {
        QActive = false;
        QReturning = false;
        QMissileNetworkId = 0;
        QMissilePosition = {};
        QReturnCoachPoint = {};
        QReturnCoachScore = 0.0f;
    }
}

inline void AddUniqueDirection(std::vector<Vector3>& directions,
                               const Vector3& direction) {
    const Vector3 normalized = Direction2D({}, direction);
    if (normalized.IsZero()) return;
    for (const auto& existing : directions) {
        if (existing.Dot(normalized) > 0.9985f) return;
    }
    directions.push_back(normalized);
}

inline std::vector<QPathUnit> BuildQPathUnits(
    const AIHeroClient& target,
    const Vector3& targetPrediction) {
    const auto player = ObjectManager::Player();
    std::vector<QPathUnit> result;
    if (!player.IsValid()) return result;
    const int targetId = target.IsValid()
        ? static_cast<int>(target.NetworkId()) : 0;
    auto append = [&](const AIBaseClient& unit, const Vector3& position) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(position) > 5000.0f) {
            return;
        }
        const int id = static_cast<int>(unit.NetworkId());
        for (const auto& present : result) {
            if (id != 0 && present.NetworkId == id) return;
        }
        result.push_back({
            position,
            std::max(20.0f, unit.BoundingRadius()),
            id,
            true,
        });
    };
    for (const auto& minion : GameObjects::EnemyLaneMinions()) {
        append(AIBaseClient(minion.Handle()), minion.Position());
    }
    for (const auto& monster : GameObjects::Jungle()) {
        append(AIBaseClient(monster.Handle()), monster.Position());
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        append(enemy, static_cast<int>(enemy.NetworkId()) == targetId
            ? targetPrediction : enemy.Position());
    }
    return result;
}

struct QAimPlan {
    bool Valid = false;
    Vector3 Direction = {};
    Vector3 CastPosition = {};
    Vector3 OutwardEnd = {};
    int ExtensionHits = 0;
    int TotalHits = 0;
    bool ReturnHit = false;
    float Score = -FLT_MAX;
};

inline QAimPlan BestQAim(const AIHeroClient& target,
                         bool allowExtension,
                         bool requireReturn = false) {
    QAimPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, 5000.0f)) return best;

    const float initialDelay = 0.25f +
        player.Position().Distance2D(target.Position()) / kQOutboundSpeed;
    const Vector3 targetPrediction = PredictPosition(target, initialDelay);
    const auto units = BuildQPathUnits(target, targetPrediction);
    std::vector<Vector3> directions;
    AddUniqueDirection(directions,
        Direction2D(player.Position(), targetPrediction));
    for (float degrees : { -7.0f, -4.0f, 4.0f, 7.0f }) {
        const Vector3 direct = Direction2D(player.Position(), targetPrediction);
        AddUniqueDirection(directions,
            Rotate2D(direct, degrees * kPi / 180.0f));
    }
    if (allowExtension) {
        for (const auto& unit : units) {
            if (unit.NetworkId == static_cast<int>(target.NetworkId())) continue;
            const float distance = player.Position().Distance2D(unit.Position);
            if (distance <= kQCastRange + unit.Radius) {
                AddUniqueDirection(directions,
                    Direction2D(player.Position(), unit.Position));
            }
        }
    }

    for (const auto& direction : directions) {
        const auto outbound = SimulateQOutbound(
            player.Position(), direction, units,
            static_cast<int>(target.NetworkId()),
            kQScriptRange, kQExtension, kQWidth,
            allowExtension ? 5000.0f : kQCastRange);
        if (!outbound.TargetHit) continue;
        const float returnSeconds = outbound.Reach / kQOutboundSpeed +
            outbound.End.Distance2D(player.Position()) / kQReturnSpeed;
        const Vector3 returnPrediction = PredictPosition(
            target, 0.25f + returnSeconds);
        const auto returnHit = QReturnIntersection(
            outbound.End, player.Position(), returnPrediction,
            target.BoundingRadius(), kQWidth);
        if (requireReturn && !returnHit.Hits) continue;
        float score = 1000.0f;
        score += static_cast<float>(outbound.ExtensionHits) * 58.0f;
        score += static_cast<float>(outbound.TotalHits) * 12.0f;
        if (returnHit.Hits) score += 360.0f;
        score -= returnHit.Distance * 0.30f;
        score -= std::fabs(Cross2D(
            Direction2D(player.Position(), targetPrediction), direction)) * 35.0f;
        if (score > best.Score) {
            best.Valid = true;
            best.Direction = direction;
            best.CastPosition = player.Position() + direction * kQCastRange;
            best.OutwardEnd = outbound.End;
            best.ExtensionHits = outbound.ExtensionHits;
            best.TotalHits = outbound.TotalHits;
            best.ReturnHit = returnHit.Hits;
            best.Score = score;
        }
    }
    return best;
}

inline void RefreshQReturnCoach(const AIHeroClient& fallback) {
    QReturnCoachPoint = {};
    QReturnCoachScore = 0.0f;
    if (!QActive || !QReturning || !QMissilePosition.IsValid() ||
        QMissilePosition.IsZero()) {
        return;
    }
    AIHeroClient target = HeroByNetworkId(QTargetId);
    if (!Engine::ValidEnemy(target)) target = fallback;
    if (!Engine::ValidEnemy(target)) return;
    const auto player = ObjectManager::Player();
    std::array<Vector3, 9> candidates = {};
    candidates[0] = player.Position();
    const Vector3 cursorDirection = Direction2D(player.Position(), Game::CursorPos());
    for (int i = 1; i < 9; ++i) {
        const float distance = 55.0f * static_cast<float>((i + 1) / 2);
        const float sign = i % 2 == 0 ? -1.0f : 1.0f;
        candidates[i] = player.Position() +
            Rotate2D(cursorDirection, sign * 18.0f * kPi / 180.0f) * distance;
    }
    float best = -FLT_MAX;
    for (const auto& candidate : candidates) {
        if (!candidate.IsValid() || candidate.IsZero() ||
            SDK::NavMesh::IsWall(candidate)) {
            continue;
        }
        const float travel = QMissilePosition.Distance2D(candidate) / kQReturnSpeed;
        const Vector3 predicted = PredictPosition(target, travel);
        const auto hit = QReturnIntersection(
            QMissilePosition, candidate, predicted,
            target.BoundingRadius(), kQWidth);
        float score = hit.Hits ? 1000.0f : 0.0f;
        score -= hit.Distance * 2.0f;
        score -= candidate.Distance2D(Game::CursorPos()) * 0.08f;
        if (score > best) {
            best = score;
            QReturnCoachPoint = candidate;
            QReturnCoachScore = hit.Hits ? 1.0f : 0.0f;
        }
    }
    if (QReturnCoachScore > 0.0f) ActiveSequence = Sequence::QReturnCoach;
}

inline bool CastQ(const AIHeroClient& target,
                  Mode mode,
                  bool allowExtension,
                  bool requireReturn = false,
                  bool reactive = false) {
    if (!Engine::ValidEnemy(target, 5000.0f) || TargetCannotBeAttacked(target) ||
        !Engine::RuntimeSpells[0] || !Engine::RuntimeSpells[0]->IsReady() ||
        !SpellEnabled(0, mode) || !CastThrottleReady(0, reactive) ||
        (!reactive && !Engine::CanAct(false)) || HoldPendingSecondShot()) {
        return false;
    }
    const float reserve = mode == Mode::Harass
        ? static_cast<float>(Slider(QMenu, "HarassReserve", 100))
        : 0.0f;
    if (CurrentResource() < SpellCost(0) + reserve) return false;
    const QAimPlan plan = BestQAim(target, allowExtension, requireReturn);
    if (!plan.Valid) return false;
    QTargetId = static_cast<int>(target.NetworkId());
    QCastOrigin = ObjectManager::Player().Position();
    QPlannedEnd = plan.OutwardEnd;
    LastQExtensionHits = plan.ExtensionHits;
    if (Engine::ControllerCastPosition(0, plan.CastPosition)) {
        QCastTick = Now();
        QActive = true;
        QReturning = false;
        LastDamagedTargetId = QTargetId;
        LastDamagedTargetUntil = Now() + kPassiveDebuffMs;
        ActiveSequence = plan.ExtensionHits > 0
            ? Sequence::ExtendedQ : Sequence::None;
        return true;
    }
    QTargetId = 0;
    return false;
}

inline bool TryAutomaticW(const AIHeroClient& selected, Mode mode) {
    if (!Bool(RogueMenu, "AutoRoamW", true) || WCamouflaged ||
        !Engine::RuntimeSpells[1] || !Engine::RuntimeSpells[1]->IsReady() ||
        !SpellEnabled(1, Mode::Automatic) || !CastThrottleReady(1) ||
        RChannelActive || EHookAttached() || ESwinging()) {
        return false;
    }
    const auto player = ObjectManager::Player();
    const int now = Now();
    if (!player.IsValid() || Engine::CountEnemiesAt(player.Position(), 1150.0f) > 0 ||
        now - LastCombatTick < Slider(RogueMenu, "OutOfCombatMs", 2600)) {
        return false;
    }
    AIHeroClient scoundrel = FindScoundrelTarget(5500.0f);
    if (!Engine::ValidEnemy(scoundrel)) return false;
    const Vector3 towardTarget = Direction2D(player.Position(), scoundrel.Position());
    const Vector3 towardCursor = Direction2D(player.Position(), Game::CursorPos());
    if (towardTarget.IsZero() || towardCursor.IsZero() ||
        towardTarget.Dot(towardCursor) < 0.25f) {
        return false;
    }
    if (!NearTerrain(player.Position(), 190.0f) &&
        player.Position().Distance2D(scoundrel.Position()) < 1500.0f) {
        return false;
    }
    const float comboReserve = SpellCost(0) + SpellCost(2) +
        (SpellRank(3) > 0 ? SpellCost(3) : 0.0f);
    if (CurrentResource() < SpellCost(1) +
        comboReserve * Slider(RogueMenu, "ManaReservePercent", 55) / 100.0f) {
        return false;
    }
    ScoundrelTargetId = static_cast<int>(scoundrel.NetworkId());
    ScoundrelVictimCount = ScoundrelStacks(scoundrel);
    if (Engine::ControllerCastSelf(1)) {
        WCastTick = now;
        WCamouflaged = true;
        WLastSeenTick = now;
        return true;
    }
    return false;
}

struct SwingPlan {
    bool Valid = false;
    Vector3 Anchor = {};
    Vector3 E2Cursor = {};
    Vector3 Dismount = {};
    SwingDirection Direction = SwingDirection::Clockwise;
    float DurationSeconds = 0.0f;
    float ClosestTargetDistance = FLT_MAX;
    int ExpectedShots = 0;
    float Score = -FLT_MAX;
};

inline int LastSwingPlanTick = 0;
inline int LastSwingPlanTargetId = 0;
inline SwingPurpose LastSwingPlanPurpose = SwingPurpose::None;
inline SwingPlan CachedSwingPlan = {};

inline bool FindWallAnchor(const Vector3& source,
                           const Vector3& direction,
                           Vector3& anchor) {
    const Vector3 normalized = Direction2D({}, direction);
    if (normalized.IsZero()) return false;
    Vector3 hit = {};
    if (!SDK::NavMesh::FindWallCollision(
            source, source + normalized * kERange, hit, 10.0f) ||
        !hit.IsValid() || hit.IsZero()) {
        return false;
    }
    const float distance = source.Distance2D(hit);
    if (distance < 95.0f || distance > kERange + 20.0f) return false;
    hit.y = SDK::NavMesh::GetHeightForPosition(hit);
    anchor = hit;
    return true;
}

inline Vector3 E2CursorForDirection(const Vector3& source,
                                    const Vector3& anchor,
                                    SwingDirection direction) {
    const Vector3 facing = Direction2D(source, anchor);
    if (facing.IsZero()) return {};
    const float radians = direction == SwingDirection::Clockwise
        ? 0.5f * kPi : -0.5f * kPi;
    return source + Rotate2D(facing, radians) * 520.0f;
}

inline int NearbyNonTargetUnits(const AIHeroClient& target,
                                float range = kEShotSearchRange) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return 0;
    int count = 0;
    for (const auto& minion : GameObjects::EnemyLaneMinions()) {
        if (minion.IsValid() && !minion.IsDead() && minion.IsTargetable() &&
            player.Position().Distance2D(minion.Position()) <= range) {
            ++count;
        }
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (monster.IsValid() && !monster.IsDead() && monster.IsTargetable() &&
            player.Position().Distance2D(monster.Position()) <= range) {
            ++count;
        }
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            static_cast<int>(enemy.NetworkId()) ==
                static_cast<int>(target.NetworkId())) {
            continue;
        }
        if (player.Position().Distance2D(enemy.Position()) <= range) ++count;
    }
    return count;
}

inline Vector3 BestDismountFrom(const Vector3& swingPosition,
                                const AIHeroClient& target,
                                SwingPurpose purpose,
                                bool lethal) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return {};
    std::array<Vector3, 11> candidates = {};
    std::size_t count = 0;
    candidates[count++] = SwingDismountPoint(
        swingPosition, Game::CursorPos(), kE3Range);
    if (target.IsValid()) {
        candidates[count++] = SwingDismountPoint(
            swingPosition, target.Position(), kE3Range);
        candidates[count++] = SwingDismountPoint(
            swingPosition,
            swingPosition + Direction2D(target.Position(), swingPosition) * kE3Range,
            kE3Range);
    }
    for (int i = 0; i < 8 && count < candidates.size(); ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / 8.0f;
        candidates[count++] = {
            swingPosition.x + std::cos(angle) * kE3Range,
            swingPosition.y,
            swingPosition.z + std::sin(angle) * kE3Range,
        };
    }
    Vector3 best = {};
    float bestScore = -FLT_MAX;
    for (std::size_t i = 0; i < count; ++i) {
        Vector3 candidate = candidates[i];
        candidate.y = SDK::NavMesh::GetHeightForPosition(candidate);
        const bool escape = purpose == SwingPurpose::Escape;
        if (!SafePoint(candidate, target, lethal, escape)) continue;
        float score = -candidate.Distance2D(Game::CursorPos()) * 0.22f;
        score += static_cast<float>(Engine::CountAlliesAt(candidate, 700.0f)) * 150.0f;
        score -= static_cast<float>(Engine::CountEnemiesAt(candidate, 650.0f)) * 210.0f;
        if (target.IsValid()) {
            const float distance = candidate.Distance2D(target.Position());
            if (escape) score += distance * 0.62f;
            else score -= std::fabs(distance - 500.0f) * 0.32f;
        }
        if (purpose == SwingPurpose::RReposition) {
            score += candidate.Distance2D(player.Position()) * 0.16f;
        }
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}

inline SwingPlan ComputeSwingPlan(const AIHeroClient& target,
                                  SwingPurpose purpose,
                                  bool lethal) {
    SwingPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return best;
    const Vector3 source = player.Position();
    const bool escape = purpose == SwingPurpose::Escape;
    const bool damagePlan = purpose == SwingPurpose::Damage ||
                            purpose == SwingPurpose::ResetCleanup;
    if (!escape && !Engine::ValidEnemy(target)) return best;
    if (damagePlan && !HasPriorityMark(target) &&
        NearbyNonTargetUnits(target) > 0) {
        // E fires at the nearest visible unit unless a recent Akshan hit marks
        // the intended champion.  Never spend the swing into a minion lottery.
        return best;
    }

    const Vector3 targetStart = target.IsValid()
        ? target.Position() : source;
    const Vector3 targetFuture = target.IsValid()
        ? PredictPosition(target, 1.25f) : targetStart;
    std::vector<Vector3> anchorDirections;
    const Vector3 cursorDirection = Direction2D(source, Game::CursorPos());
    AddUniqueDirection(anchorDirections, cursorDirection);
    if (target.IsValid()) {
        const Vector3 toTarget = Direction2D(source, targetStart);
        AddUniqueDirection(anchorDirections, toTarget);
        AddUniqueDirection(anchorDirections, Rotate2D(toTarget, 0.5f * kPi));
        AddUniqueDirection(anchorDirections, Rotate2D(toTarget, -0.5f * kPi));
        AddUniqueDirection(anchorDirections, Rotate2D(toTarget, 0.75f * kPi));
        AddUniqueDirection(anchorDirections, Rotate2D(toTarget, -0.75f * kPi));
    }
    constexpr int radialSamples = 48;
    for (int i = 0; i < radialSamples; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) /
                            static_cast<float>(radialSamples);
        AddUniqueDirection(anchorDirections,
            Vector3{ std::cos(angle), 0.0f, std::sin(angle) });
    }

    std::vector<Vector3> anchors;
    for (const auto& direction : anchorDirections) {
        Vector3 anchor = {};
        if (!FindWallAnchor(source, direction, anchor)) continue;
        bool duplicate = false;
        for (const auto& present : anchors) {
            if (present.Distance2D(anchor) < 42.0f) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) anchors.push_back(anchor);
    }

    const float horizon = purpose == SwingPurpose::Jungle
        ? 5.0f : static_cast<float>(Slider(SwingMenu, "MaxSwingMs", 2200)) / 1000.0f;
    constexpr float step = 0.05f;
    for (const auto& anchor : anchors) {
        const float radius = SwingRadius(source, anchor);
        if (radius < 115.0f || radius > kERange + 25.0f) continue;
        for (const SwingDirection direction : {
                 SwingDirection::Clockwise,
                 SwingDirection::CounterClockwise }) {
            float safeDuration = 0.0f;
            float exposureDuration = 0.0f;
            float closest = FLT_MAX;
            bool collidedTarget = false;
            bool collidedOther = false;
            Vector3 lastSafe = source;
            for (float time = step; time <= horizon + 0.001f; time += step) {
                const Vector3 position = SwingPoint(
                    anchor, source, direction, time, kESwingSpeed);
                if (!position.IsValid() || position.IsZero() ||
                    (time > 0.10f && SDK::NavMesh::IsWall(position))) {
                    break;
                }
                bool otherCollision = false;
                for (const auto& enemy : GameObjects::EnemyHeroes()) {
                    if (!Engine::ValidEnemy(enemy)) continue;
                    const bool intended = target.IsValid() &&
                        enemy.NetworkId() == target.NetworkId();
                    const Vector3 enemyPosition = intended
                        ? targetStart + (targetFuture - targetStart) *
                            std::clamp(time / 1.25f, 0.0f, 1.0f)
                        : enemy.Position();
                    const float collision = std::max(25.0f, enemy.BoundingRadius()) + 50.0f;
                    if (position.Distance2D(enemyPosition) <= collision) {
                        if (intended) collidedTarget = true;
                        else otherCollision = true;
                        break;
                    }
                }
                if (otherCollision) {
                    collidedOther = true;
                    break;
                }
                const Vector3 predictedTarget = target.IsValid()
                    ? targetStart + (targetFuture - targetStart) *
                        std::clamp(time / 1.25f, 0.0f, 1.0f)
                    : targetStart;
                const float targetDistance = position.Distance2D(predictedTarget);
                closest = std::min(closest, targetDistance);
                if (target.IsValid() && targetDistance <= kEShotSearchRange) {
                    exposureDuration += step;
                }
                lastSafe = position;
                safeDuration = time;
                if (collidedTarget) break;
            }
            if (safeDuration < 0.36f || collidedOther ||
                (collidedTarget && safeDuration < 0.52f)) {
                continue;
            }
            const int shots = target.IsValid()
                ? EstimatedSwingShots(
                    exposureDuration, 0.20f, true, true)
                : 0;
            Vector3 dismount = BestDismountFrom(
                lastSafe, target, purpose, lethal);
            if (dismount.IsZero()) continue;

            float score = 0.0f;
            if (damagePlan) {
                score += static_cast<float>(shots) * 145.0f;
                score -= closest * 0.18f;
                score += std::min(radius, 650.0f) * 0.24f;
                if (collidedTarget) score -= 120.0f;
                if (HasPriorityMark(target)) score += 210.0f;
            } else if (escape) {
                score += lastSafe.Distance2D(targetStart) * 0.52f;
                score -= dismount.Distance2D(Game::CursorPos()) * 0.28f;
                score += safeDuration * 90.0f;
            } else if (purpose == SwingPurpose::RReposition) {
                const auto oldLine = ProjectPointToSegment2D(
                    dismount, source, targetStart);
                score += oldLine.Distance * 1.15f;
                score -= dismount.Distance2D(Game::CursorPos()) * 0.10f;
                score += safeDuration * 55.0f;
            } else {
                score += static_cast<float>(shots) * 100.0f;
                score += safeDuration * 65.0f;
            }
            score += static_cast<float>(Engine::CountAlliesAt(dismount, 700.0f)) * 90.0f;
            score -= static_cast<float>(Engine::CountEnemiesAt(dismount, 650.0f)) * 145.0f;
            if (score > best.Score) {
                best.Valid = true;
                best.Anchor = anchor;
                best.E2Cursor = E2CursorForDirection(source, anchor, direction);
                best.Dismount = dismount;
                best.Direction = direction;
                best.DurationSeconds = std::clamp(
                    safeDuration - (collidedTarget ? 0.10f : 0.0f),
                    0.52f, horizon);
                best.ClosestTargetDistance = closest;
                best.ExpectedShots = shots;
                best.Score = score;
            }
        }
    }
    return best;
}

inline SwingPlan GetSwingPlan(const AIHeroClient& target,
                              SwingPurpose purpose,
                              bool lethal,
                              bool forceRefresh = false) {
    const int targetId = target.IsValid()
        ? static_cast<int>(target.NetworkId()) : 0;
    if (!forceRefresh && LastSwingPlanTick > 0 &&
        Now() - LastSwingPlanTick < 260 &&
        LastSwingPlanTargetId == targetId &&
        LastSwingPlanPurpose == purpose) {
        return CachedSwingPlan;
    }
    CachedSwingPlan = ComputeSwingPlan(target, purpose, lethal);
    LastSwingPlanTick = Now();
    LastSwingPlanTargetId = targetId;
    LastSwingPlanPurpose = purpose;
    return CachedSwingPlan;
}

inline bool CanCommitSwing(const AIHeroClient& target,
                           SwingPurpose purpose,
                           bool lethal) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) ||
        TargetCannotBeAttacked(target)) {
        return false;
    }
    if (purpose == SwingPurpose::Damage ||
        purpose == SwingPurpose::ResetCleanup) {
        if (IncomingHardCCUntil > Now() && !lethal) return false;
        if (!HasPriorityMark(target) && NearbyNonTargetUnits(target) > 0) {
            return false;
        }
        if (player.HealthPercent() < Slider(SwingMenu, "MinCommitHP", 34) &&
            !lethal) {
            return false;
        }
        const int enemies = Engine::CountEnemiesAt(target.Position(), 700.0f);
        const int allies = Engine::CountAlliesAt(target.Position(), 750.0f);
        if (!lethal && enemies > Slider(SwingMenu, "MaxCommitEnemies", 2) &&
            allies < enemies) {
            return false;
        }
    }
    return true;
}

inline bool CastE1(const AIHeroClient& target,
                   SwingPurpose purpose,
                   Mode mode,
                   bool lethal,
                   bool reactive = false) {
    if (!EFirstCastReady() || !SpellEnabled(2, mode) ||
        !CastThrottleReady(2, reactive) || HoldPendingSecondShot() ||
        (!reactive && !Engine::CanAct(false))) {
        return false;
    }
    if (purpose != SwingPurpose::Escape &&
        !CanCommitSwing(target, purpose, lethal)) {
        return false;
    }
    if (CurrentResource() < SpellCost(2) +
        (purpose == SwingPurpose::Damage
            ? static_cast<float>(Slider(SwingMenu, "ManaAfterE", 40))
            : 0.0f)) {
        return false;
    }
    const SwingPlan plan = GetSwingPlan(target, purpose, lethal, reactive);
    if (!plan.Valid) return false;
    if ((purpose == SwingPurpose::Damage ||
         purpose == SwingPurpose::ResetCleanup) &&
        plan.ExpectedShots < Slider(SwingMenu, "MinimumDamageShots", 3) &&
        !lethal) {
        return false;
    }

    PlannedAnchor = plan.Anchor;
    PlannedE2Cursor = plan.E2Cursor;
    PlannedDismount = plan.Dismount;
    PlannedSwingDirection = plan.Direction;
    PlannedSwingTargetId = target.IsValid()
        ? static_cast<int>(target.NetworkId()) : 0;
    PlannedSwingShots = plan.ExpectedShots;
    PlannedSwingScore = plan.Score;
    ActiveSwingPurpose = purpose;
    ActiveSequence = Sequence::HookPending;
    // Prevent only the orbwalker's next synthetic move from instantly turning
    // E1 into E2.  A physical player command remains authoritative.
    Orbwalker::SetMovePauseTime(130 + Game::Ping() / 2);
    if (Engine::ControllerCastPosition(2, plan.Anchor)) {
        E1CastTick = Now();
        CurrentEPhase = EPhase::HookAttached;
        return true;
    }
    ActiveSwingPurpose = SwingPurpose::None;
    ActiveSequence = Sequence::None;
    PlannedAnchor = {};
    return false;
}

inline bool StartPlannedSwing() {
    if (!EHookAttached() || PlannedE2Cursor.IsZero() ||
        !CastThrottleReady(2, true)) {
        return false;
    }
    // E2 direction is locked by the cursor side at this exact cast.  Use the
    // researched side point, then immediately return movement authority.
    if (Engine::ControllerCastPosition(2, PlannedE2Cursor)) {
        E2CastTick = Now();
        E3UnlockTick = E2CastTick + kEDismountLockMs;
        const float duration = CachedSwingPlan.Valid
            ? CachedSwingPlan.DurationSeconds : 0.75f;
        PlannedDismountTick = E2CastTick +
            static_cast<int>(duration * 1000.0f);
        CurrentEPhase = EPhase::Swinging;
        switch (ActiveSwingPurpose) {
        case SwingPurpose::Escape:
            ActiveSequence = Sequence::EscapeSwing;
            break;
        case SwingPurpose::RReposition:
            ActiveSequence = Sequence::RAngleSwing;
            break;
        case SwingPurpose::ResetCleanup:
            ActiveSequence = Sequence::ResetSwing;
            break;
        default:
            ActiveSequence = Sequence::DamageSwing;
            break;
        }
        return true;
    }
    return false;
}

inline Vector3 ChooseLiveDismount(const AIHeroClient& target,
                                  bool lethal) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return {};
    Vector3 planned = PlannedDismount;
    if (planned.IsValid() && !planned.IsZero() &&
        SafePoint(planned, target, lethal,
                  ActiveSwingPurpose == SwingPurpose::Escape)) {
        return planned;
    }
    return BestDismountFrom(
        player.Position(), target, ActiveSwingPurpose, lethal);
}

inline bool DismountSwing(const AIHeroClient& fallback,
                          bool forced = false) {
    if (!ESwinging() || Now() < E3UnlockTick ||
        !CastThrottleReady(2, true)) {
        return false;
    }
    AIHeroClient target = HeroByNetworkId(PlannedSwingTargetId);
    if (!Engine::ValidEnemy(target)) target = fallback;
    const bool lethal = Engine::ValidEnemy(target) &&
        LethalWith(target, EShotDamage(target) * 1.2f);
    const auto player = ObjectManager::Player();
    const Vector3 future = SwingPoint(
        PlannedAnchor, player.Position(), PlannedSwingDirection,
        0.12f, kESwingSpeed);
    const bool wallSoon = future.IsValid() && !future.IsZero() &&
                          SDK::NavMesh::IsWall(future);
    const bool danger = IncomingHardCCUntil > Now() ||
        player.HealthPercent() <= Slider(SwingMenu, "EmergencyDismountHP", 24);
    const bool timer = Now() >= PlannedDismountTick;
    if (!forced && !timer && !wallSoon && !danger) return false;
    const Vector3 destination = ChooseLiveDismount(target, lethal);
    if (destination.IsZero()) return false;
    PlannedDismount = destination;
    if (Engine::ControllerCastPosition(2, destination)) {
        E3CastTick = Now();
        CurrentEPhase = EPhase::Cooldown;
        ActiveSwingPurpose = SwingPurpose::None;
        ActiveSequence = Sequence::None;
        return true;
    }
    return false;
}

struct RuntimeRBlocker {
    RBlocker Geometry = {};
    bool ConsumesOneBullet = false;
    bool HardBlocker = false;
    float Health = 0.0f;
};

struct RLineAnalysis {
    int StoredBullets = 0;
    int MinionBulletsLost = 0;
    int BulletsReachingTarget = 0;
    bool HardBlocked = false;
    int FirstBlockerId = 0;
    float FirstBlockerFraction = 1.0f;
};

inline std::vector<RuntimeRBlocker> CollectRBlockers(
    const AIHeroClient& target) {
    std::vector<RuntimeRBlocker> blockers;
    const int targetId = target.IsValid()
        ? static_cast<int>(target.NetworkId()) : 0;
    auto appendMinion = [&](const AIMinionClient& minion) {
        if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable()) return;
        blockers.push_back({
            { minion.Position(), std::max(20.0f, minion.BoundingRadius()),
              static_cast<int>(minion.NetworkId()) },
            true, false, minion.Health(),
        });
    };
    for (const auto& minion : GameObjects::EnemyMinions()) appendMinion(minion);
    for (const auto& monster : GameObjects::Jungle()) appendMinion(monster);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            static_cast<int>(enemy.NetworkId()) == targetId) {
            continue;
        }
        blockers.push_back({
            { enemy.Position(), std::max(25.0f, enemy.BoundingRadius()),
              static_cast<int>(enemy.NetworkId()) },
            false, true, enemy.Health(),
        });
    }
    for (const auto& turret : GameObjects::EnemyTurrets()) {
        if (!turret.IsValid() || turret.IsDead() || !turret.IsTargetable()) continue;
        blockers.push_back({
            { turret.Position(), std::max(75.0f, turret.BoundingRadius()),
              static_cast<int>(turret.NetworkId()) },
            false, true, turret.Health(),
        });
    }
    return blockers;
}

inline RLineAnalysis AnalyzeRLine(const Vector3& source,
                                  const AIHeroClient& target,
                                  const Vector3& targetPosition,
                                  int storedBullets) {
    RLineAnalysis result{};
    result.StoredBullets = std::max(0, storedBullets);
    result.BulletsReachingTarget = result.StoredBullets;
    if (!source.IsValid() || source.IsZero() || !Engine::ValidEnemy(target) ||
        !targetPosition.IsValid() || targetPosition.IsZero()) {
        result.HardBlocked = true;
        result.BulletsReachingTarget = 0;
        return result;
    }
    struct Ordered {
        RuntimeRBlocker Blocker = {};
        float Fraction = 1.0f;
    };
    std::vector<Ordered> ordered;
    const float distance = source.Distance2D(targetPosition);
    const float targetEntry = distance > 1.0f
        ? std::clamp((distance - target.BoundingRadius()) / distance, 0.0f, 1.0f)
        : 0.0f;
    for (const auto& blocker : CollectRBlockers(target)) {
        const auto projection = ProjectPointToSegment2D(
            blocker.Geometry.Position, source, targetPosition);
        const float hitRadius = blocker.Geometry.Radius + kRMissileWidth * 0.5f;
        if (projection.T <= 0.001f || projection.T >= targetEntry ||
            projection.Distance > hitRadius) {
            continue;
        }
        ordered.push_back({ blocker, projection.T });
    }
    std::sort(ordered.begin(), ordered.end(),
        [](const Ordered& left, const Ordered& right) {
            return left.Fraction < right.Fraction;
        });
    for (const auto& entry : ordered) {
        if (result.FirstBlockerId == 0) {
            result.FirstBlockerId = entry.Blocker.Geometry.NetworkId;
            result.FirstBlockerFraction = entry.Fraction;
        }
        if (entry.Blocker.HardBlocker) {
            result.HardBlocked = true;
            result.BulletsReachingTarget = 0;
            break;
        }
        if (entry.Blocker.ConsumesOneBullet &&
            result.BulletsReachingTarget > 0) {
            --result.BulletsReachingTarget;
            ++result.MinionBulletsLost;
        }
    }
    return result;
}

inline float RVolleyDamage(const AIHeroClient& target, int bullets) {
    const auto player = ObjectManager::Player();
    const int rank = SpellRank(3);
    if (!player.IsValid() || !Engine::ValidEnemy(target) ||
        rank <= 0 || bullets <= 0) {
        return 0.0f;
    }
    float projectedHealth = target.Health() + target.AllShield();
    const float maximumHealth = std::max(1.0f, target.MaxHealth());
    float total = 0.0f;
    int passiveStacks = PassiveStacks(target);
    for (int i = 0; i < bullets; ++i) {
        const float healthPercent = std::clamp(
            projectedHealth * 100.0f / maximumHealth, 0.0f, 100.0f);
        const float raw = RRawDamagePerBullet(
            rank, player.TotalAttackDamage(),
            std::clamp(player.Crit(), 0.0f, 1.0f),
            TotalCritMultiplier(player), healthPercent);
        const float dealt = player.CalculatePhysicalDamage(target, raw);
        total += dealt;
        projectedHealth = std::max(0.0f, projectedHealth - dealt);
        ++passiveStacks;
        if (passiveStacks >= 3) {
            const float proc = PassiveProcDamage(target);
            total += proc;
            projectedHealth = std::max(0.0f, projectedHealth - proc);
            passiveStacks = 0;
        }
    }
    return total;
}

inline bool CastRRelease(RReleaseReason reason) {
    if (!RReleaseAvailable() || !Engine::RuntimeSpells[3] ||
        !Engine::RuntimeSpells[3]->IsReady() ||
        !CastThrottleReady(3, true)) {
        return false;
    }
    if (Engine::ControllerCastSelf(3)) {
        RReleaseIssued = true;
        RReleaseTick = Now();
        LastRReleaseReason = reason;
        return true;
    }
    return false;
}

inline bool CanStartR(const AIHeroClient& target,
                      bool requireLethal) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kRRange) ||
        TargetCannotBeAttacked(target) || SpellRank(3) <= 0 ||
        !Engine::RuntimeSpells[3] || !Engine::RuntimeSpells[3]->IsReady() ||
        RReleaseAvailable() || HoldPendingSecondShot()) {
        return false;
    }
    if (CurrentResource() < SpellCost(3)) return false;
    const int maximumBullets = MaximumRBullets(SpellRank(3));
    const Vector3 predicted = PredictPosition(target, 1.25f);
    const RLineAnalysis line = AnalyzeRLine(
        player.Position(), target, predicted, maximumBullets);
    if (line.HardBlocked || line.BulletsReachingTarget <= 0) return false;
    const float damage = RVolleyDamage(target, line.BulletsReachingTarget);
    const bool lethal = LethalWith(target, damage, 0.90f);
    if (requireLethal && !lethal) return false;
    if (!lethal && target.HealthPercent() >
        Slider(UltimateMenu, "StartRTargetHP", 48)) {
        return false;
    }
    if (!lethal && InAutoAttackRange(target, 60.0f) &&
        player.HealthPercent() > target.HealthPercent()) {
        return false;
    }
    if (Engine::UnderEnemyTurret(target.Position()) &&
        line.MinionBulletsLost >= maximumBullets - 1) {
        return false;
    }
    return true;
}

inline bool CastR1(const AIHeroClient& target,
                   Mode mode,
                   bool requireLethal,
                   bool manual = false) {
    if (!SpellEnabled(3, mode) || !CastThrottleReady(3) ||
        (!manual && !Engine::CanAct(false)) ||
        !CanStartR(target, requireLethal)) {
        return false;
    }
    RTargetId = static_cast<int>(target.NetworkId());
    RTrackedTargetPosition = target.Position();
    RReleaseIssued = false;
    LastRReleaseReason = RReleaseReason::None;
    if (Engine::ControllerCastUnit(3, target)) {
        RChannelActive = true;
        RChannelStartTick = Now();
        RObservedBullets = 1;
        LastDamagedTargetId = RTargetId;
        LastDamagedTargetUntil = Now() + kPassiveDebuffMs;
        ActiveSequence = Sequence::ComeuppanceChannel;
        CurrentPosture = Posture::Channeling;
        return true;
    }
    RTargetId = 0;
    return false;
}

inline bool TargetLikelyEnteringCover(const AIHeroClient& target,
                                      const Vector3& currentSource,
                                      const RLineAnalysis& currentLine) {
    if (!Engine::ValidEnemy(target) || currentLine.HardBlocked) return false;
    const Vector3 futureTarget = PredictPosition(target, 0.35f);
    const auto future = AnalyzeRLine(
        currentSource, target, futureTarget,
        std::max(1, RObservedBullets));
    return future.HardBlocked ||
           future.BulletsReachingTarget < currentLine.BulletsReachingTarget;
}

inline void ClearRState() {
    RChannelActive = false;
    RReleaseIssued = false;
    RChannelStartTick = 0;
    RTargetId = 0;
    RObservedBullets = 0;
    RLastBlockerId = 0;
    RMinionBulletsLost = 0;
    RTrackedTargetPosition = {};
    RLastOpenSource = {};
    if (ActiveSequence == Sequence::ComeuppanceChannel ||
        ActiveSequence == Sequence::RAngleSwing) {
        ActiveSequence = Sequence::None;
    }
}

inline bool HandleRChannel(const AIHeroClient& fallback, Mode mode) {
    if (!RChannelActive && !RReleaseAvailable()) return false;
    if (!RChannelActive) {
        RChannelActive = true;
        if (RChannelStartTick <= 0) RChannelStartTick = Now();
    }
    AIHeroClient target = HeroByNetworkId(RTargetId);
    if (!Engine::ValidEnemy(target)) target = fallback;
    if (!Engine::ValidEnemy(target)) return true;

    const auto player = ObjectManager::Player();
    const int elapsedMs = std::max(0, Now() - RChannelStartTick);
    RObservedBullets = StoredRBullets(
        SpellRank(3), static_cast<float>(elapsedMs) / 1000.0f);
    RTrackedTargetPosition = PredictPosition(target, 0.12f);
    const RLineAnalysis line = AnalyzeRLine(
        player.Position(), target, RTrackedTargetPosition,
        RObservedBullets);
    RLastBlockerId = line.FirstBlockerId;
    RMinionBulletsLost = line.MinionBulletsLost;
    if (!line.HardBlocked && line.BulletsReachingTarget > 0) {
        RLastOpenSource = player.Position();
    }
    const float damage = RVolleyDamage(target, line.BulletsReachingTarget);
    const bool lethal = LethalWith(target, damage, 0.94f);

    if (EHookAttached()) {
        (void)StartPlannedSwing();
        return true;
    }

    if (ESwinging()) {
        if (lethal && !line.HardBlocked && elapsedMs >= 500) {
            // A legacy E/R edge can issue an attack order and break the orbit
            // if R is released inside AA range.  Dismount first in that case.
            if (InAutoAttackRange(target, 15.0f) && Now() >= E3UnlockTick) {
                (void)DismountSwing(target, true);
                return true;
            }
            (void)CastRRelease(RReleaseReason::BlockerCleared);
            return true;
        }
        if (elapsedMs >= kRChannelMs - 220 && Now() >= E3UnlockTick) {
            (void)DismountSwing(target, true);
        }
        return true;
    }

    if (lethal && !line.HardBlocked && elapsedMs >= 500) {
        (void)CastRRelease(RReleaseReason::Lethal);
        return true;
    }
    if (elapsedMs >= 500 && !line.HardBlocked &&
        TargetLikelyEnteringCover(target, player.Position(), line) &&
        line.BulletsReachingTarget >=
            Slider(UltimateMenu, "EarlyReleaseMinBullets", 2)) {
        (void)CastRRelease(RReleaseReason::TargetEnteringCover);
        return true;
    }
    const bool shouldReposition =
        Bool(UltimateMenu, "UseEForRBlocker", true) &&
        (line.HardBlocked ||
         line.MinionBulletsLost >= Slider(UltimateMenu, "RepositionLostBullets", 2)) &&
        elapsedMs >= Slider(UltimateMenu, "RHookAfterMs", 850) &&
        elapsedMs <= Slider(UltimateMenu, "RHookBeforeMs", 1750) &&
        EFirstCastReady() && SpellEnabled(2, mode);
    if (shouldReposition) {
        (void)CastE1(target, SwingPurpose::RReposition, mode, lethal, true);
        return true;
    }
    if (elapsedMs >= kRChannelMs - 170 && !line.HardBlocked &&
        line.BulletsReachingTarget > 0) {
        (void)CastRRelease(RReleaseReason::FullMagazine);
    }
    return true;
}

inline AIHeroClient ResolveCombatTarget(const AIHeroClient& selected) {
    if (Engine::ValidEnemy(selected)) return selected;
    AIHeroClient planned = HeroByNetworkId(PlannedSwingTargetId);
    if (Engine::ValidEnemy(planned)) return planned;
    if (CurrentPosture == Posture::ScoundrelCleanup) {
        AIHeroClient scoundrel = FindScoundrelTarget(5000.0f);
        if (Engine::ValidEnemy(scoundrel)) return scoundrel;
    }
    return Engine::SelectTarget(5000.0f);
}

inline float ShortCombatDamage(const AIHeroClient& target,
                               int expectedEShots = 0,
                               bool includeR = false) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0.0f;
    float damage = SDK::Damage::GetAutoAttackDamage(player, target, true) +
                   PassiveSecondShotDamage(target);
    int stacks = PassiveStacks(target) + 2;
    if (stacks >= 3) {
        damage += PassiveProcDamage(target);
        stacks %= 3;
    }
    if (Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->IsReady()) {
        damage += QDamage(target) * 2.0f;
        stacks += 2;
        if (stacks >= 3) damage += PassiveProcDamage(target);
    }
    if (expectedEShots > 0) {
        damage += EShotDamage(target) * static_cast<float>(expectedEShots);
        damage += PassiveProcDamage(target) *
            static_cast<float>((stacks + expectedEShots) / 3);
    }
    if (includeR && Engine::RuntimeSpells[3] &&
        Engine::RuntimeSpells[3]->IsReady()) {
        damage += RVolleyDamage(target, MaximumRBullets(SpellRank(3)));
    }
    return damage;
}

inline bool TryKillSecure(const AIHeroClient& preferred, Mode mode) {
    if (!Bool(Engine::AutomaticMenu, "KillSecure", true)) return false;
    std::vector<AIHeroClient> enemies;
    if (Engine::ValidEnemy(preferred)) enemies.push_back(preferred);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 5000.0f)) continue;
        bool exists = false;
        for (const auto& present : enemies) {
            if (present.NetworkId() == enemy.NetworkId()) {
                exists = true;
                break;
            }
        }
        if (!exists) enemies.push_back(enemy);
    }
    std::sort(enemies.begin(), enemies.end(),
        [](const AIHeroClient& left, const AIHeroClient& right) {
            const int leftScoundrel = ScoundrelStacks(left);
            const int rightScoundrel = ScoundrelStacks(right);
            if (leftScoundrel != rightScoundrel) {
                return leftScoundrel > rightScoundrel;
            }
            return left.HealthPercent() < right.HealthPercent();
        });
    for (const auto& enemy : enemies) {
        if (TargetCannotBeAttacked(enemy)) continue;
        if (Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->IsReady() &&
            LethalWith(enemy, QDamage(enemy) * 2.0f) &&
            CastQ(enemy, Mode::Automatic, true, false, true)) {
            return true;
        }
        if (CanStartR(enemy, true) &&
            CastR1(enemy, Mode::Automatic, true)) {
            return true;
        }
        if (EFirstCastReady() && HasPriorityMark(enemy)) {
            const SwingPurpose purpose = ScoundrelStacks(enemy) > 0
                ? SwingPurpose::ResetCleanup : SwingPurpose::Damage;
            const SwingPlan plan = GetSwingPlan(enemy, purpose, true);
            const float damage = EShotDamage(enemy) *
                static_cast<float>(std::max(1, plan.ExpectedShots));
            if (plan.Valid && LethalWith(enemy, damage) &&
                CastE1(enemy, purpose, mode == Mode::None
                    ? Mode::Automatic : mode, true, true)) {
                return true;
            }
        }
    }
    return false;
}

inline bool TryGapcloserResponse(const AIHeroClient& fallback, Mode mode) {
    if (GapcloserTargetId == 0 || Now() > GapcloserExpireTick) return false;
    AIHeroClient threat = HeroByNetworkId(GapcloserTargetId);
    if (!Engine::ValidEnemy(threat)) threat = fallback;
    if (!Engine::ValidEnemy(threat)) return false;
    if (EFirstCastReady() &&
        CastE1(threat, SwingPurpose::Escape,
               mode == Mode::None ? Mode::Automatic : mode,
               false, true)) {
        GapcloserTargetId = 0;
        return true;
    }
    if (Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->IsReady() &&
        CastQ(threat, mode == Mode::None ? Mode::Automatic : mode,
              false, false, true)) {
        GapcloserTargetId = 0;
        return true;
    }
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || TargetCannotBeAttacked(target)) return false;
    const auto player = ObjectManager::Player();
    const int now = Now();
    const bool inAa = InAutoAttackRange(target, 25.0f);
    const bool recentlyCompletedDouble = LastSecondShotTick > 0 &&
        now - LastSecondShotTick <= Slider(PassiveMenu, "WeaveWindowMs", 430);

    // The dominant 26.1+ damage pattern is AA-AA-Q.  Let the orbwalker begin
    // the attack and protect its second shot before inserting Q.
    if (inAa && Orbwalker::CanAttack() && !recentlyCompletedDouble &&
        !Engine::IsHardCrowdControlled(target)) {
        return false;
    }
    if (recentlyCompletedDouble &&
        CastQ(target, Mode::Combo, true, false)) {
        return true;
    }
    if (!inAa && Engine::RuntimeSpells[0] &&
        Engine::RuntimeSpells[0]->IsReady() &&
        CastQ(target, Mode::Combo, true, false)) {
        return true;
    }

    const bool scoundrel = ScoundrelStacks(target) > 0;
    const SwingPurpose purpose = scoundrel
        ? SwingPurpose::ResetCleanup : SwingPurpose::Damage;
    if (EFirstCastReady() && HasPriorityMark(target) &&
        (target.Position().Distance2D(player.Position()) > AutoAttackRange(target) - 60.0f ||
         target.HealthPercent() <= Slider(SwingMenu, "SwingTargetHP", 45) ||
         scoundrel)) {
        const SwingPlan plan = GetSwingPlan(target, purpose, false);
        const float swingDamage = EShotDamage(target) *
            static_cast<float>(std::max(1, plan.ExpectedShots));
        const bool lethal = LethalWith(target, swingDamage +
            (Engine::RuntimeSpells[0] && Engine::RuntimeSpells[0]->IsReady()
                ? QDamage(target) : 0.0f));
        if (CastE1(target, purpose, Mode::Combo, lethal)) return true;
    }

    const float range = player.Position().Distance2D(target.Position());
    if ((range > AutoAttackRange(target) + 80.0f ||
         target.HealthPercent() <= Slider(UltimateMenu, "StartRTargetHP", 48)) &&
        CastR1(target, Mode::Combo, false)) {
        return true;
    }
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || TargetCannotBeAttacked(target)) return false;
    const int now = Now();
    const bool completedDouble = LastSecondShotTick > 0 &&
        now - LastSecondShotTick <= Slider(PassiveMenu, "WeaveWindowMs", 430);
    if (InAutoAttackRange(target, 20.0f) && Orbwalker::CanAttack() &&
        !completedDouble) {
        return false;
    }
    if (completedDouble || !InAutoAttackRange(target, 40.0f)) {
        return CastQ(target, Mode::Harass, true,
                     Bool(QMenu, "HarassRequireReturn", false));
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& fallback) {
    AIHeroClient threat = NearestEnemyToPlayer(fallback, 1400.0f);
    if (EHookAttached()) return StartPlannedSwing();
    if (ESwinging()) return DismountSwing(threat, Now() >= PlannedDismountTick);
    if (Engine::ValidEnemy(threat) && EFirstCastReady() &&
        CastE1(threat, SwingPurpose::Escape, Mode::Flee, false, true)) {
        return true;
    }
    if (Engine::ValidEnemy(threat) && Engine::RuntimeSpells[0] &&
        Engine::RuntimeSpells[0]->IsReady() &&
        CastQ(threat, Mode::Flee, false, false, true)) {
        return true;
    }
    const auto player = ObjectManager::Player();
    if (Bool(RogueMenu, "FleeW", true) && !WCamouflaged &&
        Engine::RuntimeSpells[1] && Engine::RuntimeSpells[1]->IsReady() &&
        CastThrottleReady(1) && NearTerrain(player.Position(), 190.0f) &&
        Engine::CountEnemiesAt(player.Position(), 800.0f) == 0 &&
        Now() - LastCombatTick > 900) {
        if (Engine::ControllerCastSelf(1)) {
            WCastTick = Now();
            WCamouflaged = true;
            return true;
        }
    }
    return false;
}

struct FarmQPlan {
    bool Valid = false;
    Vector3 CastPosition = {};
    Vector3 OutwardEnd = {};
    int UniqueHits = 0;
    int LastHits = 0;
    int ExtensionHits = 0;
    float Score = -FLT_MAX;
};

inline FarmQPlan BestFarmQ(const std::vector<AIBaseClient>& units,
                           bool lastHitOnly) {
    FarmQPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || units.empty()) return best;
    std::vector<QPathUnit> pathUnits;
    std::vector<Vector3> directions;
    for (const auto& unit : units) {
        if (!ValidHostileUnit(unit, 5000.0f)) continue;
        pathUnits.push_back({
            unit.Position(), std::max(20.0f, unit.BoundingRadius()),
            static_cast<int>(unit.NetworkId()), true,
        });
        if (player.Position().Distance2D(unit.Position()) <=
            kQCastRange + unit.BoundingRadius()) {
            AddUniqueDirection(directions,
                Direction2D(player.Position(), unit.Position()));
        }
    }
    for (const auto& direction : directions) {
        const auto outbound = SimulateQOutbound(
            player.Position(), direction, pathUnits, 0,
            kQScriptRange, kQExtension, kQWidth, 5000.0f);
        if (outbound.TotalHits <= 0) continue;
        int uniqueHits = 0;
        int lastHits = 0;
        for (const auto& unit : units) {
            if (!ValidHostileUnit(unit, 5000.0f)) continue;
            const QPathUnit pathUnit{
                unit.Position(), std::max(20.0f, unit.BoundingRadius()),
                static_cast<int>(unit.NetworkId()), true,
            };
            const auto outHit = QLineIntersection(
                player.Position(), direction, pathUnit, kQWidth);
            const bool outboundHit = outHit.Hits && outHit.Forward >= 0.0f &&
                outHit.Forward <= outbound.Reach + pathUnit.Radius;
            const auto backHit = QReturnIntersection(
                outbound.End, player.Position(), unit.Position(),
                unit.BoundingRadius(), kQWidth);
            if (!outboundHit && !backHit.Hits) continue;
            ++uniqueHits;
            const int passes = (outboundHit ? 1 : 0) + (backHit.Hits ? 1 : 0);
            const float damage = QDamage(unit, true) * static_cast<float>(passes);
            const int arrivalMs = static_cast<int>(
                (0.25f + std::max(0.0f, outHit.Forward) / kQOutboundSpeed) *
                1000.0f);
            const float health = SDK::HealthPrediction::GetPrediction(
                unit, std::clamp(arrivalMs, 250, 2200));
            if (health > 0.0f && damage >= health) ++lastHits;
        }
        float score = static_cast<float>(uniqueHits) * 115.0f +
                      static_cast<float>(lastHits) * 155.0f +
                      static_cast<float>(outbound.ExtensionHits) * 30.0f;
        if (lastHitOnly && lastHits == 0) score -= 10000.0f;
        if (score > best.Score) {
            best.Valid = true;
            best.CastPosition = player.Position() + direction * kQCastRange;
            best.OutwardEnd = outbound.End;
            best.UniqueHits = uniqueHits;
            best.LastHits = lastHits;
            best.ExtensionHits = outbound.ExtensionHits;
            best.Score = score;
        }
    }
    return best;
}

inline bool TryFarm(bool lastHitOnly) {
    const Mode mode = lastHitOnly ? Mode::LastHit : Mode::LaneClear;
    auto lane = Engine::ClearUnits(false);
    auto jungle = Engine::ClearUnits(true);
    const bool useJungle = lane.empty() && !jungle.empty();
    auto& units = useJungle ? jungle : lane;
    if (units.empty() || !Engine::RuntimeSpells[0] ||
        !Engine::RuntimeSpells[0]->IsReady() || !SpellEnabled(0, mode) ||
        !CastThrottleReady(0) || HoldPendingSecondShot()) {
        return false;
    }
    if (ObjectManager::Player().ManaPercent() <
        Slider(FarmMenu, "FarmMana", 48)) {
        return false;
    }
    const FarmQPlan plan = BestFarmQ(units, lastHitOnly);
    const int minimum = useJungle ? 1 :
        (lastHitOnly ? 1 : Slider(FarmMenu, "QMinions", 3));
    if (!plan.Valid || plan.UniqueHits < minimum ||
        (lastHitOnly && plan.LastHits <= 0)) {
        return false;
    }
    QTargetId = 0;
    QCastOrigin = ObjectManager::Player().Position();
    QPlannedEnd = plan.OutwardEnd;
    LastQExtensionHits = plan.ExtensionHits;
    if (Engine::ControllerCastPosition(0, plan.CastPosition)) {
        QCastTick = Now();
        QActive = true;
        QReturning = false;
        ActiveSequence = plan.ExtensionHits > 0
            ? Sequence::ExtendedQ : Sequence::None;
        return true;
    }
    return false;
}

inline bool IsWCamouflageBuffName(const char* name) {
    return NameEquals(name, "AkshanW") ||
           NameEquals(name, "AkshanWStealth") ||
           NameEquals(name, "AkshanCamouflage");
}

inline bool IsRChannelBuffName(const char* name) {
    return NameEquals(name, "AkshanR") ||
           NameEquals(name, "AkshanRChannel") ||
           Engine::TextContains(name, "AkshanRChannel");
}

inline bool IsPassiveSecondAttackName(const char* name) {
    return Engine::TextContains(name, "AkshanPassiveAttack");
}

inline bool IsSwingAttackName(const char* name) {
    return Engine::TextContains(name, "AkshanEAttack");
}

inline bool HasWCamouflageBuff() {
    const auto player = ObjectManager::Player();
    return player.IsValid() &&
        (player.HasBuff("AkshanW") ||
         player.HasBuff("AkshanWStealth") ||
         player.HasBuff("AkshanCamouflage"));
}

inline bool HasRChannelBuff() {
    const auto player = ObjectManager::Player();
    return ControllerHelpers::HasAnyBuff(
        player, { "AkshanR", "AkshanRChannel" });
}

inline void ClearSwingState() {
    CurrentEPhase = EFirstCastReady() ? EPhase::Ready : EPhase::Cooldown;
    ActiveSwingPurpose = SwingPurpose::None;
    PlannedAnchor = {};
    PlannedE2Cursor = {};
    PlannedDismount = {};
    PlannedSwingTargetId = 0;
    PlannedSwingShots = 0;
    PlannedSwingScore = -FLT_MAX;
    PlannedDismountTick = 0;
    E3UnlockTick = 0;
    CachedSwingPlan = {};
    LastSwingPlanTick = 0;
    LastSwingPlanTargetId = 0;
    LastSwingPlanPurpose = SwingPurpose::None;
    if (ActiveSequence == Sequence::HookPending ||
        ActiveSequence == Sequence::DamageSwing ||
        ActiveSequence == Sequence::EscapeSwing ||
        ActiveSequence == Sequence::RAngleSwing ||
        ActiveSequence == Sequence::ResetSwing) {
        ActiveSequence = Sequence::None;
    }
}

inline void RefreshState() {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    const int now = Now();

    RefreshTrackedQ();
    if (PendingSecondShot && now > PendingSecondShotUntil) {
        PendingSecondShot = false;
        PendingSecondShotTargetId = 0;
        CurrentShotPolicy = PassiveShotPolicy::Neutral;
        if (ActiveSequence == Sequence::PreserveSecondShot ||
            ActiveSequence == Sequence::CancelSecondShotForSpeed) {
            ActiveSequence = Sequence::None;
        }
    }
    if (LastDamagedTargetId != 0 && now > LastDamagedTargetUntil) {
        LastDamagedTargetId = 0;
        LastDamagedTargetUntil = 0;
    }

    if (HasWCamouflageBuff()) {
        WCamouflaged = true;
        WLastSeenTick = now;
    } else if (WCamouflaged && now - WLastSeenTick > 260 &&
               now - WCastTick > 700) {
        WCamouflaged = false;
    }

    AIHeroClient scoundrel = FindScoundrelTarget(5500.0f);
    if (Engine::ValidEnemy(scoundrel)) {
        ScoundrelTargetId = static_cast<int>(scoundrel.NetworkId());
        ScoundrelVictimCount = ScoundrelStacks(scoundrel);
    } else {
        ScoundrelTargetId = 0;
        ScoundrelVictimCount = 0;
    }

    if (ESwinging()) {
        CurrentEPhase = EPhase::Swinging;
        if (E2CastTick <= 0) {
            E2CastTick = now;
            E3UnlockTick = now + kEDismountLockMs;
        }
    } else if (EHookAttached()) {
        CurrentEPhase = EPhase::HookAttached;
    } else if (EFirstCastReady()) {
        if (CurrentEPhase != EPhase::Ready &&
            now - std::max(E1CastTick, E3CastTick) > 220) {
            ClearSwingState();
        }
        CurrentEPhase = EPhase::Ready;
    } else if (CurrentEPhase == EPhase::HookAttached &&
               now - E1CastTick <= 320) {
        // Spell-name replication can lag briefly after the hook is fired.
    } else if (CurrentEPhase == EPhase::Swinging &&
               now - E2CastTick <= 260) {
        // Likewise retain orbit ownership until the E3 runtime arrives.
    } else {
        if ((E3CastTick > 0 && now - E3CastTick > 180) ||
            (E1CastTick > 0 && now - E1CastTick > 2300)) {
            ClearSwingState();
        }
        CurrentEPhase = EPhase::Cooldown;
    }

    const bool liveR = RReleaseAvailable() || HasRChannelBuff();
    if (liveR) {
        RChannelActive = true;
        if (RChannelStartTick <= 0) RChannelStartTick = now;
    } else if (RChannelActive) {
        const bool released = RReleaseIssued && RReleaseTick > 0 &&
            now - RReleaseTick > 260;
        const bool expired = RChannelStartTick > 0 &&
            now - RChannelStartTick > kRChannelMs + 520;
        if (released || expired) ClearRState();
    }

    if (GapcloserTargetId != 0 && now > GapcloserExpireTick) {
        GapcloserTargetId = 0;
        GapcloserEnd = {};
    }
    if (CommittedEnemyId != 0 && now > CommittedEnemyUntil) {
        CommittedEnemyId = 0;
    }
    if (IncomingLineThreatUntil > 0 && now > IncomingLineThreatUntil) {
        IncomingLineThreatUntil = 0;
    }
    if (IncomingHardCCUntil > 0 && now > IncomingHardCCUntil) {
        IncomingHardCCUntil = 0;
    }

    RefreshQReturnCoach(HeroByNetworkId(QTargetId));
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshState();
    CurrentPosture = DeterminePosture(selected);
    const AIHeroClient target = ResolveCombatTarget(selected);

    // A recast state owns the decision loop.  Issuing unrelated commands here
    // can release E in the wrong direction or cancel Comeuppance.
    if (RChannelActive || RReleaseAvailable()) {
        (void)HandleRChannel(target, mode == Mode::None
            ? Mode::Automatic : mode);
        return true;
    }
    if (EHookAttached() || CurrentEPhase == EPhase::HookAttached) {
        (void)StartPlannedSwing();
        return true;
    }
    if (ESwinging() || CurrentEPhase == EPhase::Swinging) {
        (void)DismountSwing(target, false);
        return true;
    }

    if (mode == Mode::Flee) {
        (void)TryFlee(target);
        return true;
    }
    if (TryGapcloserResponse(target, mode)) return true;
    if (HoldPendingSecondShot()) return true;
    if (TryKillSecure(target, mode)) return true;

    if (Key(Engine::AutomaticMenu, "ManualR", false) &&
        Engine::ValidEnemy(target)) {
        (void)CastR1(target, Mode::Automatic, false, true);
        return true;
    }
    if (mode == Mode::Combo) {
        (void)TryCombo(target);
        return true;
    }
    if (mode == Mode::Harass) {
        (void)TryHarass(target);
        return true;
    }
    if (mode == Mode::LaneClear || mode == Mode::LastHit) {
        (void)TryFarm(mode == Mode::LastHit);
        return true;
    }
    (void)TryAutomaticW(target, mode);
    return true;
}

inline void ObservePrimaryAttack(const AIBaseClient& target, Mode mode) {
    const int now = Now();
    LastPrimaryAttackTick = now;
    LastAutoTargetId = target.IsValid()
        ? static_cast<int>(target.NetworkId()) : 0;
    PendingSecondShot = target.IsValid();
    PendingSecondShotTargetId = LastAutoTargetId;
    PendingSecondShotUntil = now + std::clamp(620 + Game::Ping(), 480, 920);
    CurrentShotPolicy = ChooseSecondShotPolicy(target, mode);
    if (CurrentShotPolicy == PassiveShotPolicy::PreserveDamage ||
        CurrentShotPolicy == PassiveShotPolicy::PreserveProc ||
        CurrentShotPolicy == PassiveShotPolicy::PreserveLastHit) {
        ActiveSequence = Sequence::PreserveSecondShot;
    } else {
        ActiveSequence = Sequence::CancelSecondShotForSpeed;
    }
}

inline void ObserveSecondAttack(const AIBaseClient& target) {
    const int now = Now();
    LastSecondShotTick = now;
    LastAutoTargetId = target.IsValid()
        ? static_cast<int>(target.NetworkId()) : LastAutoTargetId;
    PendingSecondShot = false;
    PendingSecondShotTargetId = 0;
    PendingSecondShotUntil = 0;
    CurrentShotPolicy = PassiveShotPolicy::Neutral;
    if (ActiveSequence == Sequence::PreserveSecondShot ||
        ActiveSequence == Sequence::CancelSecondShotForSpeed) {
        ActiveSequence = Sequence::None;
    }
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !args.Sender.IsValid()) return;
    const int now = Now();

    if (!IsLocalPlayer(args.Sender)) {
        const auto threat = AnalyzeEnemyCast(
            args, 240.0f, 105.0f, 250, 250, 240, 1500, 420);
        if (!threat.Valid) return;
        if (threat.Committed) {
            CommittedEnemyId = static_cast<int>(threat.Enemy.NetworkId());
            CommittedEnemyUntil = threat.CommitmentUntilTick;
        }
        if (threat.CrossesPlayer) {
            IncomingLineThreatUntil = threat.LineThreatUntilTick;
            if (threat.LikelyHardCrowdControl) {
                IncomingHardCCUntil = now + 620;
            }
        }
        if (threat.TargetsPlayer) LastCombatTick = now;
        return;
    }

    const int targetId = static_cast<int>(args.TargetNetworkId);
    const AIBaseClient attackTarget = UnitByNetworkId(targetId);
    const bool swingAttack = IsSwingAttackName(args.SpellName);
    const bool secondAttack = IsPassiveSecondAttackName(args.SpellName);
    if (swingAttack) {
        LastDamagedTargetId = targetId;
        LastDamagedTargetUntil = now + kPassiveDebuffMs;
        LastCombatTick = now;
    } else if (args.IsAutoAttack) {
        if (secondAttack) ObserveSecondAttack(attackTarget);
        else ObservePrimaryAttack(attackTarget, Engine::CurrentMode());
        const AIHeroClient hero = HeroByNetworkId(targetId);
        if (Engine::ValidEnemy(hero)) {
            LastDamagedTargetId = targetId;
            LastDamagedTargetUntil = now + kPassiveDebuffMs;
            LastCombatTick = now;
        }
    }

    if (args.Slot == 0) {
        QCastTick = now;
        QActive = true;
        QReturning = false;
    } else if (args.Slot == 1) {
        WCastTick = now;
        WLastSeenTick = now;
        WCamouflaged = true;
    } else if (args.Slot == 2) {
        if (Engine::TextContains(args.SpellName, "AkshanE3")) {
            E3CastTick = now;
            CurrentEPhase = EPhase::Cooldown;
            ActiveSwingPurpose = SwingPurpose::None;
            if (!RChannelActive) ActiveSequence = Sequence::None;
        } else if (Engine::TextContains(args.SpellName, "AkshanE2")) {
            E2CastTick = now;
            E3UnlockTick = now + kEDismountLockMs;
            CurrentEPhase = EPhase::Swinging;
        } else {
            E1CastTick = now;
            CurrentEPhase = EPhase::HookAttached;
        }
    } else if (args.Slot == 3) {
        if (Engine::TextContains(args.SpellName, "AkshanRCancel")) {
            RReleaseIssued = true;
            RReleaseTick = now;
        } else {
            RChannelActive = true;
            RReleaseIssued = false;
            RChannelStartTick = now;
            RObservedBullets = 1;
            if (targetId != 0) RTargetId = targetId;
            ActiveSequence = Sequence::ComeuppanceChannel;
        }
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (args.IsAutoAttack || IsSwingAttackName(args.SpellName)) {
        LastAutoTargetId = static_cast<int>(args.TargetNetworkId);
        const AIHeroClient hero = HeroByNetworkId(LastAutoTargetId);
        if (Engine::ValidEnemy(hero)) LastCombatTick = Now();
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (IsWCamouflageBuffName(args.BuffName)) {
            WCamouflaged = true;
            WLastSeenTick = now;
        }
        if (IsRChannelBuffName(args.BuffName)) {
            RChannelActive = true;
            if (RChannelStartTick <= 0) RChannelStartTick = now;
            ActiveSequence = Sequence::ComeuppanceChannel;
        }
        return;
    }

    const int id = static_cast<int>(args.Sender.NetworkId);
    if (NameEquals(args.BuffName, "AkshanPassiveDebuff")) {
        LastDamagedTargetId = id;
        LastDamagedTargetUntil = now + kPassiveDebuffMs;
    }
    if (NameEquals(args.BuffName, "AkshanWHuntMark")) {
        const AIHeroClient enemy = HeroByNetworkId(id);
        if (Engine::ValidEnemy(enemy)) {
            ScoundrelTargetId = id;
            ScoundrelVictimCount = std::max(1, ScoundrelStacks(enemy));
        }
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        if (IsWCamouflageBuffName(args.BuffName)) {
            WCamouflaged = false;
            WLastSeenTick = Now();
        }
        if (IsRChannelBuffName(args.BuffName) && !RReleaseAvailable()) {
            ClearRState();
        }
        return;
    }
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id == ScoundrelTargetId &&
        NameEquals(args.BuffName, "AkshanWHuntMark")) {
        ScoundrelTargetId = 0;
        ScoundrelVictimCount = 0;
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender) &&
        IsWCamouflageBuffName(args.BuffName)) {
        WCamouflaged = true;
        WLastSeenTick = now;
    }
    if (IsLocalPlayer(args.Sender) && IsRChannelBuffName(args.BuffName)) {
        RChannelActive = true;
        if (RChannelStartTick <= 0) RChannelStartTick = now;
    }
    if (!IsLocalPlayer(args.Sender) &&
        NameEquals(args.BuffName, "AkshanPassiveDebuff")) {
        LastDamagedTargetId = static_cast<int>(args.Sender.NetworkId);
        LastDamagedTargetUntil = now + kPassiveDebuffMs;
    }
    if (!IsLocalPlayer(args.Sender) &&
        NameEquals(args.BuffName, "AkshanWHuntMark")) {
        const int id = static_cast<int>(args.Sender.NetworkId);
        const AIHeroClient enemy = HeroByNetworkId(id);
        if (Engine::ValidEnemy(enemy)) {
            ScoundrelTargetId = id;
            ScoundrelVictimCount = std::max(1, ScoundrelStacks(enemy));
        }
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (RChannelActive || RReleaseAvailable() ||
        EHookAttached() || ESwinging() ||
        CurrentEPhase == EPhase::HookAttached ||
        CurrentEPhase == EPhase::Swinging) {
        args.Process = false;
        return;
    }
    if (!args.Target.IsValid() || !HoldPendingSecondShot()) return;
    const int requested = static_cast<int>(args.Target.NetworkId());
    if (PendingSecondShotTargetId != 0 &&
        requested != PendingSecondShotTargetId) {
        // Preserve the intended proc/last-hit instead of allowing the
        // orbwalker to retarget Akshan's queued second shot.
        args.Process = false;
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    int attackTick = 0;
    if (!CaptureAfterAttack(args, LastAutoTargetId, attackTick)) return;
    const AIHeroClient hero = HeroByNetworkId(LastAutoTargetId);
    if (Engine::ValidEnemy(hero)) LastCombatTick = attackTick;
}

inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (ControllerHelpers::CaptureGapcloser(
            args, GapcloserTargetId, GapcloserEnd,
            GapcloserExpireTick, 500.0f, 760)) {
        IncomingHardCCUntil = std::max(IncomingHardCCUntil, Now() + 420);
    }
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || Now() - QCastTick > 1300) return;
    if (Engine::TextContains(args.Sender.Name, "AkshanQMissile") ||
        Engine::TextContains(args.Sender.CharacterName, "AkshanQMissile")) {
        QActive = true;
        QMissileNetworkId = static_cast<int>(args.Sender.NetworkId);
        QMissilePosition = args.Sender.Position;
        QLastSeenTick = Now();
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() ||
        static_cast<int>(args.Sender.NetworkId) != QMissileNetworkId) {
        return;
    }
    if (QReturning) {
        QActive = false;
        QReturning = false;
        QMissileNetworkId = 0;
        QMissilePosition = {};
        QReturnCoachPoint = {};
        QReturnCoachScore = 0.0f;
    }
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args) ||
        !IsQMissileName(args.SpellName, args.MissileName)) {
        return;
    }
    QActive = true;
    QReturning = IsQReturnName(args.SpellName, args.MissileName);
    QMissileNetworkId = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    QMissilePosition = args.Sender.Position.IsValid()
        ? args.Sender.Position : args.StartPosition;
    QLastSeenTick = Now();
    if (QCastTick <= 0) QCastTick = Now();
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args) ||
        !IsQMissileName(args.SpellName, args.MissileName)) {
        return;
    }
    const int id = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    if ((id == QMissileNetworkId ||
         IsQReturnName(args.SpellName, args.MissileName)) &&
        (QReturning || IsQReturnName(args.SpellName, args.MissileName))) {
        QActive = false;
        QReturning = false;
        QMissileNetworkId = 0;
        QMissilePosition = {};
        QReturnCoachPoint = {};
        QReturnCoachScore = 0.0f;
        QTargetId = 0;
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::Trade: return "trade";
    case Posture::MarksmanDps: return "marksman-dps";
    case Posture::Assassinate: return "assassinate";
    case Posture::ScoundrelCleanup: return "scoundrel";
    case Posture::Channeling: return "channel";
    case Posture::Escape: return "escape";
    default: return "neutral";
    }
}

inline const char* ShotPolicyName(PassiveShotPolicy policy) {
    switch (policy) {
    case PassiveShotPolicy::PreserveDamage: return "hold-damage";
    case PassiveShotPolicy::PreserveProc: return "hold-proc";
    case PassiveShotPolicy::PreserveLastHit: return "hold-last-hit";
    case PassiveShotPolicy::CancelForSpeed: return "cancel-speed";
    case PassiveShotPolicy::CancelForSafety: return "cancel-safety";
    default: return "neutral";
    }
}

inline const char* EPhaseName(EPhase phase) {
    switch (phase) {
    case EPhase::Ready: return "ready";
    case EPhase::HookAttached: return "hook";
    case EPhase::Swinging: return "orbit";
    default: return "cooldown";
    }
}

inline void OnDraw() {
    if (!CoachMenu || !ObjectManager::Player().IsValid()) return;
    const auto player = ObjectManager::Player();
    if (Bool(CoachMenu, "DrawQ", true) && QActive &&
        QMissilePosition.IsValid() && !QMissilePosition.IsZero()) {
        Drawing::DrawCircle(QMissilePosition, 34.0f,
            QReturning ? 0xFFFFC857u : 0xFF42D9D0u, 2.0f, 32);
        const Vector3 endpoint = QReturning
            ? player.Position() : QPlannedEnd;
        if (endpoint.IsValid() && !endpoint.IsZero()) {
            Drawing::DrawLine(QMissilePosition, endpoint,
                QReturning ? 0xAAFFC857u : 0xAA42D9D0u, 2.0f);
        }
    }
    if (Bool(CoachMenu, "DrawQReturnCoach", true) && QReturning &&
        QReturnCoachPoint.IsValid() && !QReturnCoachPoint.IsZero()) {
        Drawing::DrawLine(player.Position(), QReturnCoachPoint,
                          0xFF6BFFA8u, 2.0f);
        Drawing::DrawCircle(QReturnCoachPoint, 38.0f,
                            0xFF6BFFA8u, 2.0f, 28);
    }
    if (Bool(CoachMenu, "DrawSwing", true) &&
        PlannedAnchor.IsValid() && !PlannedAnchor.IsZero()) {
        const float radius = PlannedAnchor.Distance2D(player.Position());
        Drawing::DrawCircle(PlannedAnchor, std::max(25.0f, radius),
                            0xAA35C8D0u, 1.7f, 72);
        Drawing::DrawLine(player.Position(), PlannedAnchor,
                          0xFF35C8D0u, 2.0f);
        if (PlannedDismount.IsValid() && !PlannedDismount.IsZero()) {
            Drawing::DrawCircle(PlannedDismount, 42.0f,
                                0xFFE7B84Du, 2.0f, 30);
        }
    }
    AIHeroClient rTarget = HeroByNetworkId(RTargetId);
    if (Bool(CoachMenu, "DrawR", true) && RChannelActive &&
        Engine::ValidEnemy(rTarget)) {
        Drawing::DrawLine(player.Position(), RTrackedTargetPosition,
                          RLastBlockerId == 0 ? 0xFF63FF88u : 0xFFFF6B6Bu,
                          2.2f);
        const AIBaseClient blocker = UnitByNetworkId(RLastBlockerId);
        if (blocker.IsValid()) {
            Drawing::DrawCircle(blocker.Position(),
                std::max(35.0f, blocker.BoundingRadius() + 20.0f),
                0xFFFF6B6Bu, 2.0f, 36);
        }
    }
    const AIHeroClient scoundrel = HeroByNetworkId(ScoundrelTargetId);
    if (Bool(CoachMenu, "DrawScoundrel", true) &&
        Engine::ValidEnemy(scoundrel)) {
        Drawing::DrawCircle(scoundrel.Position(),
            scoundrel.BoundingRadius() + 75.0f, 0xFFFFC247u, 2.0f, 42);
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen = {};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char state[300] = {};
            _snprintf_s(state, sizeof(state), _TRUNCATE,
                "Akshan one-trick | %s | P %s | Q %s +%d | E %s shots %d | R %d/%d lost %d | rogue %d",
                PostureName(CurrentPosture), ShotPolicyName(CurrentShotPolicy),
                QReturning ? "return" : (QActive ? "out" : "ready"),
                LastQExtensionHits, EPhaseName(CurrentEPhase), PlannedSwingShots,
                RObservedBullets, MaximumRBullets(SpellRank(3)),
                RMinionBulletsLost, ScoundrelVictimCount);
            Drawing::DrawText(screen.x - 200.0f, screen.y - 122.0f,
                              0xFFFFE8A8u, state);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "AkshanOneTrick", "Akshan one-trick mechanics"));

    PassiveMenu = TacticsMenu->AddSubMenu(new Menu(
        "DirtyFighting", "Dirty Fighting shot ownership"));
    PassiveMenu->Add(new MenuSlider(
        "WeaveWindowMs", "Q weave window after second shot (ms)", 430, 180, 850));
    PassiveMenu->Add(new MenuSeparator(
        "ShotOwnership", "Champion damage/procs and last hits preserve shot two; danger/flee leaves its cancellation to player movement."));

    QMenu = TacticsMenu->AddSubMenu(new Menu(
        "Avengerang", "Extended outbound and moving return"));
    QMenu->Add(new MenuSlider(
        "HarassReserve", "Mana reserved after harass Q", 100, 0, 300));
    QMenu->Add(new MenuBool(
        "HarassRequireReturn", "Require planned return hit in harass", false));
    QMenu->Add(new MenuSeparator(
        "ReturnOwnership", "Return-Q movement is coached visually and never overrides player pathing."));

    RogueMenu = TacticsMenu->AddSubMenu(new Menu(
        "GoingRogue", "Scoundrel hunt and camouflage economy"));
    RogueMenu->Add(new MenuBool(
        "AutoRoamW", "Use W only for a safe cursor-aligned Scoundrel roam", true));
    RogueMenu->Add(new MenuSlider(
        "OutOfCombatMs", "Required time out of combat before roam W (ms)", 2600, 1000, 6000));
    RogueMenu->Add(new MenuSlider(
        "ManaReservePercent", "Percent of Q/E/R mana retained after W", 55, 0, 100));
    RogueMenu->Add(new MenuSlider(
        "CleanupHP", "Scoundrel cleanup posture below target HP (%)", 52, 10, 90));
    RogueMenu->Add(new MenuBool(
        "FleeW", "Use W in flee only after immediate pressure is gone", true));
    RogueMenu->Add(new MenuSeparator(
        "NoCombatW", "Going Rogue is not treated as a rote combat steroid."));

    SwingMenu = TacticsMenu->AddSubMenu(new Menu(
        "HeroicSwing", "Three-cast wall/orbit/dismount solver"));
    SwingMenu->Add(new MenuSlider(
        "MinimumDamageShots", "Minimum estimated shots for nonlethal E", 3, 1, 12));
    SwingMenu->Add(new MenuSlider(
        "MaxSwingMs", "Maximum planned combat orbit (ms)", 2200, 650, 5000));
    SwingMenu->Add(new MenuSlider(
        "SwingTargetHP", "Allow aggressive E below target HP (%)", 45, 10, 90));
    SwingMenu->Add(new MenuSlider(
        "ManaAfterE", "Mana retained after damage E", 40, 0, 220));
    SwingMenu->Add(new MenuSlider(
        "MinCommitHP", "Minimum own HP for nonlethal swing (%)", 34, 10, 80));
    SwingMenu->Add(new MenuSlider(
        "AssassinateHP", "Assassination posture below target HP (%)", 58, 15, 100));
    SwingMenu->Add(new MenuSlider(
        "EscapeHP", "Escape posture below own HP (%)", 28, 10, 70));
    SwingMenu->Add(new MenuSlider(
        "EmergencyDismountHP", "Emergency E3 below own HP (%)", 24, 5, 65));
    SwingMenu->Add(new MenuSlider(
        "MaxCommitEnemies", "Maximum enemies at a nonlethal route", 2, 1, 5));
    SwingMenu->Add(new MenuBool(
        "RespectLockdown", "Reject landings into ready point-click lockdown", true));
    SwingMenu->Add(new MenuBool(
        "AllowLethalDive", "Permit a verified lethal turret dismount", false));
    SwingMenu->Add(new MenuSeparator(
        "MarkedTarget", "Damage E requires Akshan's priority mark when other units could steal its shots."));

    UltimateMenu = TacticsMenu->AddSubMenu(new Menu(
        "Comeuppance", "Ammo, blocker, and release state machine"));
    UltimateMenu->Add(new MenuSlider(
        "StartRTargetHP", "Nonlethal R only below target HP (%)", 48, 10, 90));
    UltimateMenu->Add(new MenuSlider(
        "EarlyReleaseMinBullets", "Minimum bullets for cover-aware early release", 2, 1, 7));
    UltimateMenu->Add(new MenuBool(
        "UseEForRBlocker", "Use E during R only to open the firing angle", true));
    UltimateMenu->Add(new MenuSlider(
        "RepositionLostBullets", "Reposition after this many minion-blocked bullets", 2, 1, 6));
    UltimateMenu->Add(new MenuSlider(
        "RHookAfterMs", "Earliest R-channel E hook (ms)", 850, 400, 1700));
    UltimateMenu->Add(new MenuSlider(
        "RHookBeforeMs", "Latest R-channel E hook (ms)", 1750, 900, 2300));
    UltimateMenu->Add(new MenuSeparator(
        "NoEShotsDuringR", "E is angle-only while channeling: Heroic Swing does not fire its attacks during R."));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "AkshanFarm", "Two-leg and extension-aware Q farming"));
    FarmMenu->Add(new MenuSlider(
        "QMinions", "Minimum unique lane units hit by Q", 3, 1, 8));
    FarmMenu->Add(new MenuSlider(
        "FarmMana", "Minimum mana for farming (%)", 48, 0, 100));
    FarmMenu->Add(new MenuSeparator(
        "NoFarmSwing", "W, E, and Comeuppance are never spent for routine waveclear."));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "AkshanCoach", "Live return, swing, blocker, and Scoundrel coaching"));
    CoachMenu->Add(new MenuBool("DrawQ", "Draw tracked Q leg", true));
    CoachMenu->Add(new MenuBool(
        "DrawQReturnCoach", "Draw player-owned return-Q alignment point", true));
    CoachMenu->Add(new MenuBool("DrawSwing", "Draw E anchor, orbit, and E3 point", true));
    CoachMenu->Add(new MenuBool("DrawR", "Draw R line and first blocker", true));
    CoachMenu->Add(new MenuBool("DrawScoundrel", "Highlight prioritized Scoundrel", true));
    CoachMenu->Add(new MenuBool("DrawState", "Draw one-trick state machine", true));
}

inline void OnLoad() {
    ActiveSequence = Sequence::None;
    CurrentPosture = Posture::Neutral;
    CurrentShotPolicy = PassiveShotPolicy::Neutral;
    ActiveSwingPurpose = SwingPurpose::None;
    LastRReleaseReason = RReleaseReason::None;
    LastPrimaryAttackTick = 0;
    LastSecondShotTick = 0;
    LastAutoTargetId = 0;
    PendingSecondShotTargetId = 0;
    PendingSecondShotUntil = 0;
    PendingSecondShot = false;
    LastCombatTick = 0;
    LastDamagedTargetId = 0;
    LastDamagedTargetUntil = 0;
    QActive = false;
    QReturning = false;
    QCastTick = 0;
    QLastSeenTick = 0;
    QMissileNetworkId = 0;
    QTargetId = 0;
    LastQExtensionHits = 0;
    QCastOrigin = {};
    QPlannedEnd = {};
    QMissilePosition = {};
    QReturnCoachPoint = {};
    QReturnCoachScore = 0.0f;
    WCamouflaged = HasWCamouflageBuff();
    WCastTick = 0;
    WLastSeenTick = WCamouflaged ? Now() : 0;
    ScoundrelTargetId = 0;
    ScoundrelVictimCount = 0;
    CurrentEPhase = EFirstCastReady() ? EPhase::Ready : EPhase::Cooldown;
    PlannedAnchor = {};
    PlannedE2Cursor = {};
    PlannedDismount = {};
    PlannedSwingDirection = SwingDirection::Clockwise;
    PlannedSwingTargetId = 0;
    E1CastTick = 0;
    E2CastTick = 0;
    E3CastTick = 0;
    E3UnlockTick = 0;
    PlannedDismountTick = 0;
    PlannedSwingShots = 0;
    PlannedSwingScore = -FLT_MAX;
    LastSwingPlanTick = 0;
    LastSwingPlanTargetId = 0;
    LastSwingPlanPurpose = SwingPurpose::None;
    CachedSwingPlan = {};
    ClearRState();
    GapcloserTargetId = 0;
    GapcloserExpireTick = 0;
    GapcloserEnd = {};
    CommittedEnemyId = 0;
    CommittedEnemyUntil = 0;
    IncomingLineThreatUntil = 0;
    IncomingHardCCUntil = 0;
    RefreshState();
}

inline void OnUnload() {
    TacticsMenu = nullptr;
    PassiveMenu = nullptr;
    QMenu = nullptr;
    RogueMenu = nullptr;
    SwingMenu = nullptr;
    UltimateMenu = nullptr;
    FarmMenu = nullptr;
    CoachMenu = nullptr;
    ActiveSequence = Sequence::None;
    ClearRState();
    ClearSwingState();
}

inline constexpr const char* Scenarios[] = {
    "Distinguish Akshan basic attacks from the AkshanPassiveAttack second shot",
    "Preserve the second shot for champion damage during a trade",
    "Preserve the second shot when it completes a three-hit Dirty Fighting proc",
    "Preserve the second shot when health prediction makes it a lane last hit",
    "Leave second-shot cancellation to player movement during explicit flee",
    "Allow a safety cancellation when an incoming hard-CC or spell line is detected",
    "Prevent the orbwalker from retargeting a reserved second shot onto a minion",
    "Hold spell insertion until a reserved second shot has actually fired or expired",
    "Insert Q inside the configured post-second-shot AA-AA-Q weave window",
    "Model the patch 26.1 full bonus-crit scaling on Akshan's second shot",
    "Use current 45-to-165 plus 70-percent bonus-AD Avengerang damage",
    "Separate Q's 750 script reach from its 850 cast input range",
    "Extend Q by 500 units for every sequential unit intersection",
    "Chain multiple Q extensions rather than assuming only one range increase",
    "Predict the intended champion independently from extension minions",
    "Score outbound and returning Q as separate possible hits",
    "Home returning Q to Akshan's live position instead of its cast origin",
    "Draw a lateral return-Q alignment point without issuing movement",
    "Require a projected return hit for harass only when configured",
    "Reserve mana after harass Q instead of emptying the combat budget",
    "Track outbound and return missiles through dedicated lifecycle callbacks",
    "Recover missed Q callbacks by scanning live local missiles",
    "Use a recent Q hit as E's champion-priority mark",
    "Read Scoundrel marks and their victim count from live target buffs",
    "Prefer a low-health Scoundrel without ignoring enemy density and lockdown",
    "Cast W only after a configurable out-of-combat interval",
    "Require the player's cursor to agree with an automatic Scoundrel roam",
    "Use nearby terrain to preserve long camouflage when approaching a hunt",
    "Reserve a configurable share of Q-E-R mana before spending W",
    "Never treat Going Rogue as a rote in-combat damage steroid",
    "Maintain W state indefinitely while its real camouflage buff remains live",
    "Use flee W only after immediate enemies are gone and terrain can sustain it",
    "Scan real NavMesh walls rather than inventing an E anchor",
    "Simulate clockwise and counter-clockwise Heroic Swing orbits",
    "Favor practical perpendicular wall entries while rejecting tiny hook radii",
    "Predict a moving target throughout the sampled E orbit",
    "Reject damage E when minions or champions can steal unmarked E shots",
    "Estimate E shot count from orbit exposure and current attack-speed scaling",
    "Stop an orbit simulation at champion collision",
    "Stop an orbit simulation when its next sample enters terrain",
    "Choose E3 from multiple cursor, target, escape, and radial candidates",
    "Respect E3's fixed 0.5-second recast lock",
    "Emergency-dismount on incoming hard CC or critically low health",
    "Reject E landings into Poppy W, Taliyah E, or Cassiopeia W dash denial",
    "Reject nonlethal E landings under turret or ready point-click lockdown",
    "Pause only the orbwalker's next synthetic move after E1 attaches",
    "Keep physical player attack and movement commands authoritative during E",
    "Use a Scoundrel takedown/reset route only when its orbit is safe",
    "Prefer a large safe orbit while allowing a shorter lethal swing",
    "Track E1 hook, E2 orbit, and E3 dismount as three distinct runtime phases",
    "Store exactly 5, 6, or 7 Comeuppance bullets by rank and channel time",
    "Recalculate each R bullet with the target's newly missing health",
    "Include Dirty Fighting procs produced by sequential R bullets",
    "Treat every intervening minion as consuming one stored R bullet",
    "Treat an intervening enemy champion as a hard R blocker",
    "Treat an intervening turret as a hard R blocker",
    "Release early when a currently open target is about to enter cover",
    "Use E during R only inside a bounded blocker-reposition window",
    "Never count Heroic Swing attacks while Comeuppance is channeling",
    "Dismount before lethal R release when an orbit ends inside attack range",
    "Release a full open magazine near channel expiry",
    "Do not start R into untargetability, invulnerability, or an empty firing line",
    "Answer a directed gapcloser with a safe E escape or reactive Q",
    "Treat interruptable channels as commitment because Akshan has no hard interrupt",
    "Farm Q with unique two-leg hits, extension count, and arrival health prediction",
    "Never spend W, E, or R for routine waveclear",
    "Re-plan around manual Q-W-E-R while retaining observed recast and missile state",
    "Prioritize patch 26.1-plus autos and Q instead of the obsolete E-first damage identity",
    "Coordinate through state holds and coaching without taking permanent movement control",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Akshan";
    controller.ControllerId = "champion.kuroaio.ai.akshan.onetrick";
    controller.KitRevision = "League 26.14 / CommunityDragon 16.14";
    controller.ResearchArtifact = "AI/Research/AIAkshan.md";
    controller.ImplementationSummary =
        "Passive second-shot ownership, multi-extension/two-leg Q routing, "
        "safe Scoundrel camouflage, sampled wall/orbit/dismount E geometry, "
        "and sequential-ammo blocker-aware R release and E repositioning.";
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
    // Akshan has no hard interrupt.  The channel is still a high-value
    // commitment window for Q/E/R, but the controller never pretends to CC it.
    controller.OnInterruptable =
        &ControllerHelpers::CaptureInterruptableEvent<
            &CommittedEnemyId, &CommittedEnemyUntil, 900, 250, 5000>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Akshan
