#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIAniviaGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Anivia {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::CursorDirectionAgrees;
using ControllerHelpers::EnemyFlashReady;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::HasNearbyJungleTarget;
using ControllerHelpers::HasCurrentResource;
using ControllerHelpers::HasResourceFor;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::NameEquals;
using ControllerHelpers::Now;
using ControllerHelpers::ObjectEventIsAllied;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PredictionAtLeast;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContains;
using ControllerHelpers::SpellRank;
using ControllerHelpers::UnitByNetworkId;
using ControllerHelpers::ValidHostileUnitInGameplayRange;

enum class Sequence : int {
    None,
    HoldFlashFrost,
    QDoubleHit,
    QSpellShieldBreak,
    PrecastFrostbite,
    WallRedirect,
    StormGrow,
    FirstEmpoweredFrostbite,
    HoldQForExit,
    SecondEmpoweredFrostbite,
    RelocateStorm,
    PeelChain,
    JungleCycle,
    Egg,
};

enum class Posture : int {
    Neutral,
    LaneControl,
    Catch,
    Zone,
    Peel,
    Disengage,
    Siege,
    Objective,
    Egg,
};

enum class QPurpose : int {
    None,
    DoubleHit,
    Catch,
    WallForced,
    StormExit,
    Peel,
    Interrupt,
    AntiGapcloser,
    SpellShieldBreak,
    Lethal,
    Farm,
    Jungle,
};

enum class WallPurpose : int {
    None,
    KeepInStorm,
    ForceQPath,
    Catch,
    SplitTeam,
    Peel,
    AntiGapcloser,
    Interrupt,
    Disengage,
};

enum class StormPurpose : int {
    None,
    Catch,
    Zone,
    Peel,
    Disengage,
    Teamfight,
    DoubleFrostbite,
    Waveclear,
    Jungle,
    Objective,
    Manual,
};

struct FlashFrostPlan {
    Vector3 Aim = {};
    Vector3 TargetPosition = {};
    Vector3 Direction = {};
    int TargetId = 0;
    SDK::HitChance Hitchance = SDK::HitChance::None;
    QPurpose Purpose = QPurpose::None;
    float ImpactSeconds = 0.0f;
    float Score = -FLT_MAX;
    bool TargetCommitted = false;
    bool ShieldBreak = false;
    bool Guaranteed = false;
    bool Valid = false;
};

struct WallPlan {
    Vector3 Center = {};
    Vector3 PredictedTarget = {};
    Vector3 DisplacedTarget = {};
    WallSegment Segment = {};
    int TargetId = 0;
    WallPurpose Purpose = WallPurpose::None;
    float Score = -FLT_MAX;
    bool PushesTarget = false;
    bool BlocksEscape = false;
    bool KeepsInStorm = false;
    bool AllyUnsafe = false;
    bool TurretAggro = false;
    bool Valid = false;
};

struct StormPlan {
    Vector3 Center = {};
    std::array<int, 10> HitIds = {};
    int TargetId = 0;
    int HitCount = 0;
    int PriorityHits = 0;
    StormPurpose Purpose = StormPurpose::None;
    float Score = -FLT_MAX;
    bool IncludesSelected = false;
    bool IncludesProtectedThreat = false;
    bool Valid = false;
};

struct ChillMark {
    int NetworkId = 0;
    int ExpireTick = 0;
    bool Confirmed = false;
};

inline Menu* TacticsMenu = nullptr;
inline Menu* FlashFrostMenu = nullptr;
inline Menu* WallMenu = nullptr;
inline Menu* FrostbiteMenu = nullptr;
inline Menu* StormMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Sequence ActiveSequence = Sequence::None;
inline Posture CurrentPosture = Posture::Neutral;
inline QPurpose LastQPurpose = QPurpose::None;
inline WallPurpose LastWallPurpose = WallPurpose::None;
inline StormPurpose LastStormPurpose = StormPurpose::None;

inline bool QActive = false;
inline bool QMissileObserved = false;
inline bool QDetonationRequested = false;
inline int QMissileNetworkId = 0;
inline int QCastTick = 0;
inline int QLastSeenTick = 0;
inline int QTargetId = 0;
inline int QShieldBreakTargetId = 0;
inline int QDetonationTick = 0;
inline int QAutoEndTick = 0;
inline Vector3 QOrigin = {};
inline Vector3 QDirection = {};
inline Vector3 QCastEnd = {};
inline Vector3 QMissilePosition = {};
inline std::array<int, 12> QPassedIds = {};
inline FlashFrostPlan LastQPlan = {};

inline int WCastTick = 0;
inline int WTargetId = 0;
inline WallPlan LastWallPlan = {};

inline int ECastTick = 0;
inline int EImpactTick = 0;
inline int ETargetId = 0;
inline bool EWaitingForChill = false;
inline int EmpoweredECount = 0;
inline int LastEmpoweredETick = 0;
inline int ESequenceTargetId = 0;

inline bool RActive = false;
inline bool RObjectObserved = false;
inline bool RWasManual = false;
inline int RObjectNetworkId = 0;
inline int RCastTick = 0;
inline int RLastSeenTick = 0;
inline int RLastContactTick = 0;
inline int RNoContactSince = 0;
inline int RTargetId = 0;
inline int RRelocateTargetId = 0;
inline Vector3 RCenter = {};
inline Vector3 RRelocateCenter = {};
inline StormPlan LastStormPlan = {};

inline std::array<ChillMark, 24> ChillMarks = {};
inline bool RebirthReady = false;
inline bool RebirthCooldown = false;
inline bool EggActive = false;
inline int EggUntilTick = 0;
inline int RebirthCooldownUntil = 0;

inline int ProtectedAllyId = 0;
inline int PeelThreatId = 0;
inline int TargetedAllyThreatId = 0;
inline int TargetedAllyThreatUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int IncomingThreatUntil = 0;
inline float RecentIncomingPressure = 0.0f;

inline constexpr int kChillDurationMs = 3000;
inline constexpr int kStormFullMs = 1500;
inline constexpr int kStormMinimumRecastMs = 1000;
inline constexpr int kWallDurationMs = 5000;
inline constexpr int kEggDurationMs = 6000;

inline bool PlayerCannotRecastQ() {
    const auto player = ObjectManager::Player();
    return !player.IsValid() || Engine::IsPlayerCrowdControlled(player) ||
           EggActive;
}

inline bool IsSpellShielded(const AIBaseClient& target) {
    return target.IsValid() &&
           SDK::HasBuffOfType(target, SDK::BuffType::SpellShield);
}

inline void MarkChill(int networkId,
                      int durationMs = kChillDurationMs,
                      bool confirmed = false) {
    if (networkId == 0) return;
    const int expiry = Now() + std::max(80, durationMs);
    ChillMark* empty = nullptr;
    for (auto& mark : ChillMarks) {
        if (mark.NetworkId == networkId) {
            mark.ExpireTick = std::max(mark.ExpireTick, expiry);
            mark.Confirmed = mark.Confirmed || confirmed;
            return;
        }
        if (!empty && (mark.NetworkId == 0 || mark.ExpireTick < Now())) {
            empty = &mark;
        }
    }
    if (!empty) empty = &ChillMarks.front();
    *empty = ChillMark{ networkId, expiry, confirmed };
}

inline int TrackedChillUntil(int networkId, bool requireConfirmed = false) {
    for (const auto& mark : ChillMarks) {
        if (mark.NetworkId == networkId && mark.ExpireTick >= Now() &&
            (!requireConfirmed || mark.Confirmed)) {
            return mark.ExpireTick;
        }
    }
    return 0;
}

inline bool HasLiveChill(const AIBaseClient& target,
                         bool requireConfirmed = false) {
    if (!target.IsValid()) return false;
    const int id = static_cast<int>(target.NetworkId());
    if (TrackedChillUntil(id, requireConfirmed) > Now()) return true;
    return target.HasBuff("chilled") ||
           target.HasBuff("Chilled") ||
           target.HasBuff("ChilledAniviaUlt") ||
           target.HasBuff("aniviachilled");
}

inline bool QPassedTarget(int networkId) {
    return std::find(QPassedIds.begin(), QPassedIds.end(), networkId) !=
           QPassedIds.end();
}

inline void RememberQPass(int networkId) {
    if (networkId == 0 || QPassedTarget(networkId)) return;
    for (auto& id : QPassedIds) {
        if (id == 0) {
            id = networkId;
            return;
        }
    }
    QPassedIds.back() = networkId;
}

inline Vector3 TargetVelocity(const AIBaseClient& target,
                              float horizon = 0.30f) {
    if (!target.IsValid()) return {};
    const float safeHorizon = std::max(0.10f, horizon);
    Vector3 velocity = (PredictPosition(target, safeHorizon) -
                        target.Position()) / safeHorizon;
    velocity.y = 0.0f;
    const float speed = velocity.Length2D();
    const float cap = target.IsDashing() ? 2200.0f : 725.0f;
    if (speed > cap && speed > 0.001f) velocity = velocity / speed * cap;
    return velocity;
}

inline float TargetPriority(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const float offense = target.TotalAttackDamage() * 0.0045f +
                          target.AP() * 0.0042f;
    const float range = std::clamp(
        target.AttackRange() / 700.0f, 0.0f, 1.35f);
    const float wounded = (100.0f - target.HealthPercent()) * 0.012f;
    return 0.75f + offense + range + wounded;
}

inline float QDamage(const AIBaseClient& target,
                     bool passHit = true,
                     bool explosionHit = true) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return player.CalculateMagicDamage(
        target, FlashFrostRawDamage(
            SpellRank(0), player.AP(), passHit, explosionHit));
}

inline float EDamage(const AIBaseClient& target, bool empowered) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return player.CalculateMagicDamage(
        target, FrostbiteRawDamage(
            SpellRank(2), player.AP(), empowered));
}

inline float StormTickDamage(const AIBaseClient& target,
                             bool full) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return player.CalculateMagicDamage(
        target, StormRawDamagePerTick(
            SpellRank(3), player.AP(), full));
}

inline float ConservativeComboDamage(const AIHeroClient& target,
                                     bool includeStorm = true) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const auto player = ObjectManager::Player();
    float damage = SDK::Damage::GetAutoAttackDamage(player, target, true);
    if ((Ready(0) && !QActive) || QActive) {
        damage += QDamage(target, true, true);
    }
    if (Ready(2)) damage += EDamage(target, true);
    if (includeStorm && (RActive || Ready(3))) {
        damage += StormTickDamage(target, true) * 3.0f;
    }
    return damage * 0.86f;
}

inline bool IsQMissileName(const char* spellName,
                           const char* missileName) {
    return ControllerHelpers::TextContainsAny(
               missileName, { "FlashFrost", "cryo_FlashFrost" }) ||
           ControllerHelpers::TextContainsAny(
               spellName, { "FlashFrostSpell", "FlashFrost" });
}

inline bool IsQDetonateName(const char* spellName) {
    return Engine::TextContains(spellName, "FlashFrostSpell2") ||
           Engine::TextContains(spellName, "FlashFrostDetonate");
}

inline bool IsStormName(const char* first, const char* second = nullptr) {
    return ControllerHelpers::AnyTextContains(
        { first, second }, { "cryo_storm", "GlacialStorm" });
}

inline void ClearQState() {
    QActive = false;
    QMissileObserved = false;
    QMissileNetworkId = 0;
    QLastSeenTick = 0;
    QTargetId = 0;
    QShieldBreakTargetId = 0;
    QDetonationRequested = false;
    QOrigin = {};
    QDirection = {};
    QCastEnd = {};
    QMissilePosition = {};
    QPassedIds.fill(0);
    if (ActiveSequence == Sequence::QDoubleHit ||
        ActiveSequence == Sequence::QSpellShieldBreak ||
        ActiveSequence == Sequence::PrecastFrostbite) {
        ActiveSequence = Sequence::None;
    }
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
        QMissileObserved = true;
        QMissileNetworkId = missile.NetworkId();
        QMissilePosition = missile.Position();
        QLastSeenTick = Now();
        if (QOrigin.IsZero()) QOrigin = missile.StartPosition();
        if (QCastEnd.IsZero()) QCastEnd = missile.EndPosition();
        if (QDirection.IsZero()) {
            QDirection = Direction2D(QOrigin, QCastEnd);
        }
    }

    if (QActive && !found && !QMissileObserved && QCastTick > 0) {
        const float released = std::max(
            0.0f, static_cast<float>(Now() - QCastTick - 250) / 1000.0f);
        QMissilePosition = FlashFrostPosition(
            QOrigin, QDirection, released);
    }
    if (!found && QMissileObserved && QLastSeenTick > 0 &&
        Now() - QLastSeenTick > 180 && Now() - QCastTick > 430) {
        ClearQState();
        return;
    }
    if (QActive && QAutoEndTick > 0 && Now() > QAutoEndTick + 180) {
        ClearQState();
        return;
    }

    if (!QActive || QOrigin.IsZero() || QDirection.IsZero() ||
        QMissilePosition.IsZero()) {
        return;
    }
    const float travelled = std::clamp(
        AlongRay(QOrigin, QDirection, QMissilePosition),
        0.0f, kFlashFrostRange);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) || QPassedTarget(enemy.NetworkId())) {
            continue;
        }
        const Vector3 predicted = PredictPosition(enemy, 0.045f);
        if (FlashFrostPassHits(
                QOrigin, QDirection, travelled,
                predicted, enemy.BoundingRadius()) &&
            FlashFrostOvershoot(
                QOrigin, QDirection, QMissilePosition, predicted) >= 3.0f) {
            RememberQPass(static_cast<int>(enemy.NetworkId()));
            MarkChill(static_cast<int>(enemy.NetworkId()),
                      kChillDurationMs - 70, false);
        }
    }
}

inline bool TargetCommittedToLine(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = ObjectManager::Player();
    if (target.IsDashing() || Engine::IsHardCrowdControlled(target) ||
        static_cast<int>(target.NetworkId()) == GapcloserTargetId ||
        static_cast<int>(target.NetworkId()) == InterruptTargetId) {
        return true;
    }
    if (RActive && StormHits(
            RCenter, target.Position(),
            static_cast<float>(std::max(0, Now() - RCastTick)) / 1000.0f,
            target.BoundingRadius())) {
        return true;
    }
    if (Now() - WCastTick <= 900 &&
        static_cast<int>(target.NetworkId()) == WTargetId) {
        return true;
    }
    const Vector3 pathEnd = target.PathEnd();
    if (pathEnd.IsValid() && !pathEnd.IsZero()) {
        const Vector3 movement = Direction2D(target.Position(), pathEnd);
        const Vector3 towardPlayer = Direction2D(
            target.Position(), player.Position());
        if (!movement.IsZero() && !towardPlayer.IsZero() &&
            movement.Dot(towardPlayer) >= 0.72f) {
            return true;
        }
    }
    return target.Position().Distance2D(player.Position()) <=
           std::max(360.0f, target.AttackRange() + 90.0f);
}

inline FlashFrostPlan BuildQPlan(const AIHeroClient& target,
                                QPurpose purpose,
                                bool reactive = false) {
    FlashFrostPlan plan{};
    const auto player = ObjectManager::Player();
    if (!Engine::ValidEnemy(target, kFlashFrostRange + 80.0f) ||
        QActive || !Ready(0) || IsCommonUntargetableOrImmune(target) ||
        !Engine::RuntimeSpells[0]) {
        return plan;
    }

    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    const Vector3 predicted = prediction.GetUnitPosition().IsValid() &&
            !prediction.GetUnitPosition().IsZero()
        ? prediction.GetUnitPosition()
        : target.Position();
    Vector3 aim = prediction.GetCastPosition().IsValid() &&
            !prediction.GetCastPosition().IsZero()
        ? prediction.GetCastPosition()
        : predicted;
    const float distance = player.Position().Distance2D(aim);
    if (distance > kFlashFrostRange + target.BoundingRadius()) return plan;

    const bool committed = TargetCommittedToLine(target);
    const bool shield = IsSpellShielded(target);
    SDK::HitChance required = reactive
        ? SDK::HitChance::Medium
        : SDK::HitChance::VeryHigh;
    if (committed || purpose == QPurpose::WallForced ||
        purpose == QPurpose::StormExit ||
        purpose == QPurpose::SpellShieldBreak) {
        required = SDK::HitChance::High;
    }
    if (!PredictionAtLeast(prediction, required) &&
        !Engine::IsHardCrowdControlled(target) && !target.IsDashing()) {
        return plan;
    }

    if (!reactive && purpose != QPurpose::Lethal &&
        purpose != QPurpose::WallForced &&
        purpose != QPurpose::StormExit &&
        !committed &&
        Bool(FlashFrostMenu, "HoldPressure", true)) {
        const float holdRange = static_cast<float>(
            Slider(FlashFrostMenu, "HoldBeyond", 720));
        if (distance >= holdRange || EnemyFlashReady(target)) return plan;
    }
    if (!reactive && !CursorDirectionAgrees(aim) &&
        purpose != QPurpose::Peel && purpose != QPurpose::Interrupt) {
        return plan;
    }
    if (shield && !Bool(FlashFrostMenu, "BreakSpellShield", true) &&
        purpose != QPurpose::Peel && purpose != QPurpose::Interrupt) {
        return plan;
    }

    const Vector3 direction = Direction2D(player.Position(), aim);
    if (direction.IsZero()) return plan;
    aim = player.Position() + direction * kFlashFrostRange;
    float score = TargetPriority(target) * 190.0f;
    score += committed ? 230.0f : 0.0f;
    score += target.IsDashing() ? 210.0f : 0.0f;
    score += Engine::IsHardCrowdControlled(target) ? 180.0f : 0.0f;
    score += (100.0f - target.HealthPercent()) * 2.0f;
    score -= distance * 0.10f;
    if (shield) score += 85.0f;

    plan.Aim = aim;
    plan.TargetPosition = predicted;
    plan.Direction = direction;
    plan.TargetId = static_cast<int>(target.NetworkId());
    plan.Hitchance = prediction.Hitchance;
    plan.Purpose = shield ? QPurpose::SpellShieldBreak : purpose;
    plan.ImpactSeconds = FlashFrostTravelSeconds(
        std::min(distance, kFlashFrostRange));
    plan.Score = score;
    plan.TargetCommitted = committed;
    plan.ShieldBreak = shield;
    plan.Guaranteed = Engine::IsHardCrowdControlled(target) ||
                      target.IsDashing() ||
                      prediction.Hitchance == SDK::HitChance::Immobile ||
                      prediction.Hitchance == SDK::HitChance::Dash;
    plan.Valid = true;
    return plan;
}

inline bool CastFlashFrost(const FlashFrostPlan& plan,
                           Mode mode,
                           bool reactive = false) {
    if (!plan.Valid || QActive || !Ready(0) ||
        !SpellEnabled(0, mode) ||
        !CastThrottleReady(0, reactive)) {
        return false;
    }
    const auto player = ObjectManager::Player();
    if (!HasCurrentResource(SpellCost(0)) ||
        (Orbwalker::IsWindingUp() &&
         Bool(Engine::HumanMenu, "PreserveAttacks", true) && !reactive)) {
        return false;
    }

    if (!Engine::ControllerCastPosition(0, plan.Aim)) return false;
    QActive = true;
    QMissileObserved = false;
    QDetonationRequested = false;
    QCastTick = Now();
    QLastSeenTick = QCastTick;
    QTargetId = plan.TargetId;
    QShieldBreakTargetId = plan.ShieldBreak ? plan.TargetId : 0;
    QOrigin = player.Position();
    QDirection = plan.Direction;
    QCastEnd = plan.Aim;
    QMissilePosition = QOrigin;
    QPassedIds.fill(0);
    QAutoEndTick = QCastTick + static_cast<int>(
        std::ceil((kFlashFrostCastSeconds +
                   kFlashFrostRange / kFlashFrostSpeed) * 1000.0f));
    LastQPurpose = plan.Purpose;
    LastQPlan = plan;
    ActiveSequence = plan.ShieldBreak
        ? Sequence::QSpellShieldBreak
        : Sequence::QDoubleHit;
    return true;
}

inline float QExplosionScore(const AIHeroClient& enemy,
                             const Vector3& predicted,
                             bool& doubleHit,
                             bool& shieldResolved) {
    doubleHit = false;
    shieldResolved = false;
    if (!Engine::ValidEnemy(enemy) ||
        !FlashFrostExplosionHits(
            QMissilePosition, predicted, enemy.BoundingRadius()) ||
        IsCommonUntargetableOrImmune(enemy)) {
        return -FLT_MAX;
    }
    const int id = static_cast<int>(enemy.NetworkId());
    doubleHit = QPassedTarget(id) || DoubleHitDetonationWindow(
        QOrigin, QDirection, QMissilePosition,
        predicted, enemy.BoundingRadius(),
        static_cast<float>(Slider(FlashFrostMenu, "PassOvershoot", 8)));
    shieldResolved = id == QShieldBreakTargetId && !IsSpellShielded(enemy);
    if (IsSpellShielded(enemy) && !shieldResolved) return -FLT_MAX;

    float score = TargetPriority(enemy) * 170.0f;
    score += doubleHit ? 330.0f : 80.0f;
    score += shieldResolved ? 260.0f : 0.0f;
    score += static_cast<int>(enemy.NetworkId()) == QTargetId ? 150.0f : 0.0f;
    score += enemy.IsDashing() ? 180.0f : 0.0f;
    score += Engine::IsHardCrowdControlled(enemy) ? -55.0f : 45.0f;
    score += QDamage(enemy, false, true) >=
             enemy.Health() + enemy.AllShield()
        ? 520.0f : 0.0f;
    return score;
}

inline bool CastQDetonate(int primaryId,
                          bool fastFollowup = true) {
    if (!QActive || PlayerCannotRecastQ() ||
        !CastThrottleReady(0, fastFollowup)) {
        return false;
    }
    if (!Engine::ControllerCastSelf(0)) return false;
    QDetonationRequested = true;
    QDetonationTick = Now();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        const Vector3 predicted = PredictPosition(enemy, 0.035f);
        if (FlashFrostExplosionHits(
                QMissilePosition, predicted, enemy.BoundingRadius()) &&
            !IsSpellShielded(enemy)) {
            MarkChill(static_cast<int>(enemy.NetworkId()),
                      kChillDurationMs, false);
        }
    }
    if (primaryId != 0) QTargetId = primaryId;
    return true;
}

inline bool TryDetonateFlashFrost() {
    if (!QActive || QMissilePosition.IsZero() ||
        QDirection.IsZero() || PlayerCannotRecastQ()) {
        return false;
    }

    const float travelled = std::clamp(
        AlongRay(QOrigin, QDirection, QMissilePosition),
        0.0f, kFlashFrostRange);
    int bestId = 0;
    float bestScore = -FLT_MAX;
    bool bestDouble = false;
    int hits = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        const Vector3 predicted = PredictPosition(enemy, 0.055f);
        bool doubleHit = false;
        bool shieldResolved = false;
        const float score = QExplosionScore(
            enemy, predicted, doubleHit, shieldResolved);
        if (score <= -FLT_MAX * 0.5f) continue;
        ++hits;
        if (score > bestScore) {
            bestScore = score;
            bestId = static_cast<int>(enemy.NetworkId());
            bestDouble = doubleHit;
        }
    }

    const AIHeroClient primary = HeroByNetworkId(bestId);
    const bool emergency =
        (bestId != 0 && bestId == GapcloserTargetId) ||
        (bestId != 0 && bestId == InterruptTargetId) ||
        travelled >= kFlashFrostRange - 135.0f ||
        (QAutoEndTick > 0 && QAutoEndTick - Now() <= 145);
    const bool aoe = hits >= Slider(FlashFrostMenu, "DetonateAoe", 2);
    const bool lethal = Engine::ValidEnemy(primary) &&
        QDamage(primary, false, true) >=
            primary.Health() + primary.AllShield();
    const bool shieldBreak = bestId != 0 &&
        bestId == QShieldBreakTargetId && !IsSpellShielded(primary);

    if (bestId != 0 &&
        (bestDouble || emergency || aoe || lethal || shieldBreak)) {
        return CastQDetonate(bestId, true);
    }

    const AIHeroClient intended = HeroByNetworkId(QTargetId);
    if (Engine::ValidEnemy(intended)) {
        const Vector3 predicted = PredictPosition(intended, 0.06f);
        const bool recoverable = DetonationStillRecoverable(
            QOrigin, QDirection, QMissilePosition, predicted,
            TargetVelocity(intended), intended.BoundingRadius(), 0.06f);
        if (!recoverable && FlashFrostExplosionHits(
                QMissilePosition, predicted, intended.BoundingRadius())) {
            return CastQDetonate(QTargetId, true);
        }
    }
    return false;
}

inline float StormElapsedSeconds() {
    return RActive && RCastTick > 0
        ? static_cast<float>(std::max(0, Now() - RCastTick)) / 1000.0f
        : 0.0f;
}

inline bool StormFullyFormed() {
    return RActive && StormIsFull(StormElapsedSeconds());
}

inline void ClearStormState(bool preserveRelocation = false) {
    RActive = false;
    RObjectObserved = false;
    RWasManual = false;
    RObjectNetworkId = 0;
    RLastSeenTick = 0;
    RLastContactTick = 0;
    RNoContactSince = 0;
    RTargetId = 0;
    RCenter = {};
    if (!preserveRelocation) {
        RRelocateTargetId = 0;
        RRelocateCenter = {};
    }
    if (ActiveSequence == Sequence::StormGrow ||
        ActiveSequence == Sequence::FirstEmpoweredFrostbite ||
        ActiveSequence == Sequence::HoldQForExit ||
        ActiveSequence == Sequence::SecondEmpoweredFrostbite) {
        ActiveSequence = Sequence::None;
    }
}

inline void RefreshTrackedStorm() {
    const auto player = ObjectManager::Player();
    bool found = false;
    for (const auto& object : ObjectManager::AllObjects()) {
        if (!object.IsValid() ||
            (player.IsValid() && object.Team() != player.Team())) {
            continue;
        }
        const std::string name = object.Name();
        const std::string character = object.CharacterName();
        if (!IsStormName(name.c_str(), character.c_str())) continue;
        found = true;
        RActive = true;
        RObjectObserved = true;
        RObjectNetworkId = object.NetworkId();
        RCenter = object.Position();
        RLastSeenTick = Now();
        if (RCastTick <= 0) RCastTick = Now();
        break;
    }
    if (!found && RObjectObserved && RLastSeenTick > 0 &&
        Now() - RLastSeenTick > 220) {
        ClearStormState(true);
    }
}

inline std::vector<StormUnit> BuildStormUnits(float delaySeconds) {
    std::vector<StormUnit> units;
    units.reserve(GameObjects::EnemyHeroes().size());
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        units.push_back(StormUnit{
            PredictPosition(enemy, delaySeconds),
            enemy.BoundingRadius(),
            TargetPriority(enemy),
            Engine::IsHardCrowdControlled(enemy),
            enemy.IsDashing(),
            true,
        });
    }
    return units;
}

inline void AddUniqueStormCandidate(std::vector<Vector3>& candidates,
                                    const Vector3& candidate) {
    if (!candidate.IsValid() || candidate.IsZero()) return;
    for (const auto& existing : candidates) {
        if (existing.Distance2D(candidate) <= 45.0f) return;
    }
    candidates.push_back(candidate);
}

inline StormPlan BuildStormPlan(const AIHeroClient& selected,
                                StormPurpose purpose,
                                const Vector3& forcedCenter = {}) {
    StormPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return best;

    std::vector<Vector3> candidates;
    if (forcedCenter.IsValid() && !forcedCenter.IsZero()) {
        AddUniqueStormCandidate(candidates, forcedCenter);
    }
    if (Engine::ValidEnemy(selected)) {
        const Vector3 velocity = TargetVelocity(selected, 0.35f);
        AddUniqueStormCandidate(candidates, selected.Position());
        AddUniqueStormCandidate(candidates, PredictPosition(selected, 0.25f));
        AddUniqueStormCandidate(candidates, LeadStormCenter(
            selected.Position(), velocity, 0.48f,
            static_cast<float>(Slider(StormMenu, "MaximumLead", 190))));
        if (selected.PathEnd().IsValid() && !selected.PathEnd().IsZero()) {
            AddUniqueStormCandidate(
                candidates,
                selected.Position().Extend(selected.PathEnd(), 150.0f));
        }
    }
    if (GapcloserTargetId != 0 && GapcloserExpireTick >= Now()) {
        AddUniqueStormCandidate(candidates, GapcloserEnd);
    }
    const auto enemies = GameObjects::EnemyHeroes();
    for (std::size_t i = 0; i < enemies.size(); ++i) {
        if (!Engine::ValidEnemy(enemies[i])) continue;
        const Vector3 first = PredictPosition(enemies[i], 0.35f);
        for (std::size_t j = i + 1; j < enemies.size(); ++j) {
            if (!Engine::ValidEnemy(enemies[j])) continue;
            const Vector3 second = PredictPosition(enemies[j], 0.35f);
            if (first.Distance2D(second) <= 760.0f) {
                AddUniqueStormCandidate(candidates, (first + second) * 0.5f);
            }
        }
    }

    const auto immediateUnits = BuildStormUnits(0.25f);
    const auto fullUnits = BuildStormUnits(1.50f);
    for (Vector3 candidate : candidates) {
        candidate.y = SDK::NavMesh::GetHeightForPosition(candidate);
        if (!candidate.IsValid() || candidate.IsZero() ||
            SDK::NavMesh::IsWall(candidate) ||
            player.Position().Distance2D(candidate) >
                kStormCastRange + 35.0f) {
            continue;
        }
        const float immediate = StormScore(candidate, 0.0f, immediateUnits);
        const float full = StormScore(candidate, kStormGrowSeconds, fullUnits);
        float score = immediate * 150.0f + full * 210.0f;
        StormPlan plan{};
        plan.Center = candidate;
        plan.TargetId = Engine::ValidEnemy(selected)
            ? static_cast<int>(selected.NetworkId()) : 0;
        plan.Purpose = purpose;
        for (const auto& enemy : enemies) {
            if (!Engine::ValidEnemy(enemy)) continue;
            const Vector3 predicted = PredictPosition(enemy, 1.50f);
            if (!StormHits(candidate, predicted, kStormGrowSeconds,
                           enemy.BoundingRadius())) {
                continue;
            }
            if (plan.HitCount < static_cast<int>(plan.HitIds.size())) {
                plan.HitIds[plan.HitCount] =
                    static_cast<int>(enemy.NetworkId());
            }
            ++plan.HitCount;
            if (TargetPriority(enemy) >= 1.75f) ++plan.PriorityHits;
            if (Engine::ValidEnemy(selected) &&
                enemy.NetworkId() == selected.NetworkId()) {
                plan.IncludesSelected = true;
            }
            if (static_cast<int>(enemy.NetworkId()) == PeelThreatId) {
                plan.IncludesProtectedThreat = true;
            }
        }
        score += static_cast<float>(plan.HitCount) * 95.0f;
        score += static_cast<float>(plan.PriorityHits) * 115.0f;
        score += plan.IncludesSelected ? 145.0f : 0.0f;
        score += plan.IncludesProtectedThreat ? 210.0f : 0.0f;
        score += CountAlliedFollowup(candidate, 850.0f) * 45.0f;
        score -= candidate.Distance2D(Game::CursorPos()) *
                 (purpose == StormPurpose::Disengage ? 0.02f : 0.055f);
        if (Engine::UnderEnemyTurret(candidate) &&
            purpose != StormPurpose::Peel &&
            purpose != StormPurpose::Disengage) {
            score -= 260.0f;
        }
        plan.Score = score;
        plan.Valid = plan.HitCount > 0 ||
                     purpose == StormPurpose::Waveclear ||
                     purpose == StormPurpose::Jungle ||
                     purpose == StormPurpose::Objective;
        if (plan.Valid && (!best.Valid || plan.Score > best.Score)) {
            best = plan;
        }
    }
    return best;
}

inline float StormManaReserve(Mode mode) {
    float reserve = static_cast<float>(
        Slider(StormMenu, "FlatReserve", 90));
    if (Ready(0)) reserve += SpellCost(0);
    if (Ready(2)) reserve += SpellCost(2);
    if (mode == Mode::Flee && Ready(1)) reserve += SpellCost(1);
    return reserve;
}

inline bool CastStorm(const StormPlan& plan,
                      Mode mode,
                      bool reactive = false) {
    if (!plan.Valid || RActive || SpellRank(3) <= 0 ||
        !Ready(3) || !SpellEnabled(3, mode) ||
        !CastThrottleReady(3, reactive)) {
        return false;
    }
    const float reserve = StormManaReserve(mode);
    if (!HasCurrentResource(SpellCost(3) + reserve * 0.35f)) return false;
    if (!reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true)) {
        return false;
    }
    if (!Engine::ControllerCastPosition(3, plan.Center)) return false;
    RActive = true;
    RObjectObserved = false;
    RWasManual = false;
    RCastTick = Now();
    RLastSeenTick = RCastTick;
    RLastContactTick = RCastTick;
    RNoContactSince = 0;
    RTargetId = plan.TargetId;
    RCenter = plan.Center;
    LastStormPlan = plan;
    LastStormPurpose = plan.Purpose;
    ActiveSequence = Sequence::StormGrow;
    return true;
}

inline int StormChampionContacts(float predictionDelay = 0.12f) {
    if (!RActive || RCenter.IsZero()) return 0;
    int count = 0;
    const float elapsed = StormElapsedSeconds() +
                          std::max(0.0f, predictionDelay);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && StormHits(
                RCenter, PredictPosition(enemy, predictionDelay), elapsed,
                enemy.BoundingRadius())) {
            ++count;
        }
    }
    return count;
}

inline bool StormContainsUnit(const AIBaseClient& target,
                              float delaySeconds = 0.0f,
                              bool requireFull = false) {
    if (!RActive || !target.IsValid() || RCenter.IsZero()) return false;
    const float elapsed = StormElapsedSeconds() +
                          std::max(0.0f, delaySeconds);
    if (requireFull && !StormIsFull(elapsed)) return false;
    return StormHits(RCenter, PredictPosition(target, delaySeconds),
                     elapsed, target.BoundingRadius());
}

inline int ScheduledStormChillTick(const AIBaseClient& target,
                                   int impactTick) {
    if (!RActive || !target.IsValid() || RCenter.IsZero()) return 0;
    const int fullTick = RCastTick + kStormFullMs;
    if (fullTick > impactTick - 35) return 0;
    const float untilFull = static_cast<float>(
        std::max(0, fullTick - Now())) / 1000.0f;
    return StormHits(
               RCenter, PredictPosition(target, untilFull),
               kStormGrowSeconds, target.BoundingRadius())
        ? fullTick : 0;
}

inline bool CancelStorm(bool forRelocation = false) {
    if (!RActive || Now() - RCastTick < kStormMinimumRecastMs ||
        !CastThrottleReady(3, true)) {
        return false;
    }
    if (!Engine::ControllerCastSelf(3)) return false;
    ClearStormState(forRelocation);
    if (forRelocation) ActiveSequence = Sequence::RelocateStorm;
    return true;
}

inline bool TryRelocateStorm(const AIHeroClient& selected, Mode mode) {
    if (RActive || RRelocateCenter.IsZero() ||
        RRelocateTargetId == 0 || !Ready(3)) {
        return false;
    }
    const AIHeroClient target = HeroByNetworkId(RRelocateTargetId);
    if (!Engine::ValidEnemy(target)) {
        RRelocateCenter = {};
        RRelocateTargetId = 0;
        return false;
    }
    StormPlan plan = BuildStormPlan(
        target, StormPurpose::DoubleFrostbite, RRelocateCenter);
    if (CastStorm(plan, mode, false)) {
        RRelocateCenter = {};
        RRelocateTargetId = 0;
        return true;
    }
    return false;
}

inline int ScheduledQChillTick(const AIBaseClient& target,
                               int impactTick) {
    if (!QActive || !target.IsValid() || QMissilePosition.IsZero()) return 0;
    const Vector3 predicted = PredictPosition(
        target, std::max(0.0f,
            static_cast<float>(impactTick - Now()) / 1000.0f));
    if (!FlashFrostExplosionHits(
            QMissilePosition, predicted, target.BoundingRadius()) ||
        IsSpellShielded(target)) {
        return 0;
    }
    return Now() + 55;
}

inline bool FrostbiteExpectedEmpowered(const AIBaseClient& target,
                                       int impactTick,
                                       int& scheduledChillTick) {
    scheduledChillTick = 0;
    if (!target.IsValid()) return false;
    const int id = static_cast<int>(target.NetworkId());
    int chillUntil = TrackedChillUntil(id, false);
    if (HasLiveChill(target, true) && chillUntil <= Now()) {
        chillUntil = impactTick + 120;
    }
    scheduledChillTick = ScheduledQChillTick(target, impactTick);
    if (scheduledChillTick == 0) {
        scheduledChillTick = ScheduledStormChillTick(target, impactTick);
    }
    return FrostbiteWillBeEmpowered(
        Now(), impactTick, chillUntil, scheduledChillTick, 35);
}

inline bool CastFrostbite(const AIBaseClient& target,
                          Mode mode,
                          bool allowOrdinaryLethal = true,
                          bool reactive = false) {
    if (!target.IsValid() || !Ready(2) || !SpellEnabled(2, mode) ||
        !CastThrottleReady(2, reactive) ||
        ObjectManager::Player().Position().Distance2D(target.Position()) >
            kFrostbiteRange + target.BoundingRadius() ||
        IsCommonUntargetableOrImmune(target) || IsSpellShielded(target)) {
        return false;
    }
    const float impactSeconds = FrostbiteImpactSeconds(
        ObjectManager::Player().Position().Distance2D(target.Position()));
    const int impactTick = Now() + static_cast<int>(
        std::ceil(impactSeconds * 1000.0f));
    int scheduledChill = 0;
    const bool empowered = FrostbiteExpectedEmpowered(
        target, impactTick, scheduledChill);
    const bool ordinaryLethal = allowOrdinaryLethal &&
        EDamage(target, false) >= target.Health() + target.AllShield();
    const bool ordinaryPoke = mode == Mode::Harass &&
        Bool(FrostbiteMenu, "OrdinaryPoke", false) &&
        ControllerHelpers::PlayerManaPercent() >= Slider(FrostbiteMenu, "OrdinaryPokeMana", 78) &&
        InAutoAttackRange(target, 25.0f);
    if (!empowered && !ordinaryLethal && !ordinaryPoke) return false;
    if (!HasCurrentResource(SpellCost(2))) return false;
    if (!reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true)) {
        return false;
    }
    if (!Engine::ControllerCastUnit(2, target)) return false;
    ECastTick = Now();
    EImpactTick = impactTick;
    ETargetId = static_cast<int>(target.NetworkId());
    EWaitingForChill = empowered && scheduledChill > Now();
    if (empowered) {
        if (ESequenceTargetId != ETargetId ||
            Now() - LastEmpoweredETick > 6500) {
            ESequenceTargetId = ETargetId;
            EmpoweredECount = 0;
        }
        ++EmpoweredECount;
        LastEmpoweredETick = Now();
        ActiveSequence = EmpoweredECount == 1
            ? Sequence::FirstEmpoweredFrostbite
            : Sequence::SecondEmpoweredFrostbite;
    }
    return true;
}

inline bool TryManageStorm(const AIHeroClient& selected, Mode mode) {
    if (!RActive) return TryRelocateStorm(selected, mode);

    const int contacts = StormChampionContacts(0.12f);
    if (contacts > 0) {
        RLastContactTick = Now();
        RNoContactSince = 0;
    } else if (RNoContactSince == 0) {
        RNoContactSince = Now();
    }

    if (StormFullyFormed()) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (Engine::ValidEnemy(enemy) &&
                StormContainsUnit(enemy, 0.0f, true)) {
                MarkChill(static_cast<int>(enemy.NetworkId()),
                          1550, false);
            }
        }
    }

    const float reserve = StormManaReserve(mode);
    const bool pendingERace = EWaitingForChill &&
        EImpactTick >= Now() && EImpactTick - Now() <= 900;
    const bool manaEmergency = StormManaAfter(
        CurrentResource(), SpellRank(3), 0.65f) < reserve;
    if (manaEmergency && !pendingERace) {
        return CancelStorm(false);
    }

    const int grace = Slider(StormMenu, "NoContactGrace", 520);
    const bool emptyTooLong = RNoContactSince > 0 &&
        Now() - RNoContactSince >= grace;
    if (emptyTooLong && !pendingERace &&
        LastStormPurpose != StormPurpose::Waveclear &&
        LastStormPurpose != StormPurpose::Jungle &&
        LastStormPurpose != StormPurpose::Objective) {
        if (Engine::ValidEnemy(selected) &&
            Bool(StormMenu, "Relocate", true) &&
            Now() - RCastTick >= kStormFullMs) {
            const StormPlan replacement = BuildStormPlan(
                selected, StormPurpose::DoubleFrostbite);
            const float improvement = replacement.Valid
                ? replacement.Score - LastStormPlan.Score : -FLT_MAX;
            if (replacement.Valid &&
                improvement >= Slider(StormMenu, "RelocateGain", 180)) {
                RRelocateCenter = replacement.Center;
                RRelocateTargetId = static_cast<int>(selected.NetworkId());
                return CancelStorm(true);
            }
        }
        return CancelStorm(false);
    }

    if (RWasManual && Bool(StormMenu, "RespectManualStorm", true) &&
        Now() - RCastTick < 2200) {
        return false;
    }
    return false;
}

inline bool WallSeparates(const WallSegment& wall,
                          const Vector3& first,
                          const Vector3& second) {
    if (!wall.Valid || !first.IsValid() || !second.IsValid()) return false;
    const float firstSide = WallSignedSide(wall, first);
    const float secondSide = WallSignedSide(wall, second);
    return firstSide * secondSide < 0.0f &&
           WallBlocksPath(wall, first, second, 20.0f);
}

inline bool WallEndangersAllies(const WallSegment& wall,
                                WallPurpose purpose,
                                const AIHeroClient& protectedAlly,
                                const AIHeroClient& threat) {
    if (!wall.Valid) return true;
    const auto player = ObjectManager::Player();
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally, 1500.0f)) continue;
        const Vector3 predicted = PredictPosition(ally, 0.30f);
        if (WallContains(wall, predicted, ally.BoundingRadius() + 12.0f)) {
            return true;
        }
        const bool underPressure = Engine::CountEnemiesAt(
            predicted, 475.0f) > 0 || ally.HealthPercent() <= 38.0f;
        if (!underPressure) continue;
        const bool intentionalPeel = purpose == WallPurpose::Peel &&
            Engine::ValidAlly(protectedAlly) &&
            ally.NetworkId() == protectedAlly.NetworkId() &&
            Engine::ValidEnemy(threat) &&
            WallSeparates(wall, predicted, threat.Position());
        if (!intentionalPeel && WallBlocksPath(
                wall, predicted, player.Position(),
                ally.BoundingRadius() * 0.45f)) {
            return true;
        }
        if (ally.IsDashing() && ally.PathEnd().IsValid() &&
            !ally.PathEnd().IsZero() && WallBlocksPath(
                wall, ally.Position(), ally.PathEnd(),
                ally.BoundingRadius() * 0.50f)) {
            return true;
        }
    }
    return false;
}

inline void AddUniqueWallCandidate(std::vector<Vector3>& candidates,
                                   const Vector3& candidate) {
    if (!candidate.IsValid() || candidate.IsZero()) return;
    for (const auto& existing : candidates) {
        if (existing.Distance2D(candidate) <= 24.0f) return;
    }
    candidates.push_back(candidate);
}

inline WallPlan BuildWallPlan(const AIHeroClient& target,
                              WallPurpose purpose,
                              const AIHeroClient& protectedAlly = {},
                              const AIHeroClient& threat = {},
                              const Vector3& forcedCenter = {}) {
    WallPlan best{};
    const auto player = ObjectManager::Player();
    const int rank = SpellRank(1);
    if (!player.IsValid() || !Engine::ValidEnemy(target) || rank <= 0) {
        return best;
    }
    const Vector3 predicted = PredictPosition(target, 0.25f);
    const Vector3 casterDirection = Direction2D(player.Position(), predicted);
    if (casterDirection.IsZero()) return best;
    const Vector3 tangent{ -casterDirection.z, 0.0f, casterDirection.x };
    const Vector3 velocity = TargetVelocity(target, 0.35f);
    const Vector3 escapeDirection = target.PathEnd().IsValid() &&
            !target.PathEnd().IsZero()
        ? Direction2D(predicted, target.PathEnd())
        : Direction2D({}, velocity);

    std::vector<Vector3> candidates;
    if (forcedCenter.IsValid() && !forcedCenter.IsZero()) {
        AddUniqueWallCandidate(candidates, forcedCenter);
    }
    for (const float normalOffset : { -95.0f, -48.0f, 0.0f, 48.0f, 95.0f }) {
        AddUniqueWallCandidate(
            candidates, predicted + casterDirection * normalOffset);
    }
    AddUniqueWallCandidate(candidates, predicted + tangent * 55.0f);
    AddUniqueWallCandidate(candidates, predicted - tangent * 55.0f);
    if (!escapeDirection.IsZero()) {
        AddUniqueWallCandidate(
            candidates, predicted + escapeDirection * 105.0f);
        AddUniqueWallCandidate(
            candidates, predicted - escapeDirection * 55.0f);
    }
    if (RActive && !RCenter.IsZero()) {
        const Vector3 towardStorm = Direction2D(predicted, RCenter);
        AddUniqueWallCandidate(
            candidates, predicted - towardStorm * 75.0f);
        AddUniqueWallCandidate(
            candidates, predicted + towardStorm * 45.0f);
    }
    if (purpose == WallPurpose::Peel &&
        Engine::ValidAlly(protectedAlly)) {
        const Vector3 awayFromAlly = Direction2D(
            protectedAlly.Position(), predicted);
        AddUniqueWallCandidate(
            candidates, predicted - awayFromAlly * 70.0f);
        AddUniqueWallCandidate(
            candidates, (protectedAlly.Position() + predicted) * 0.5f);
    }

    for (Vector3 center : candidates) {
        center.y = SDK::NavMesh::GetHeightForPosition(center);
        if (!center.IsValid() || center.IsZero() ||
            player.Position().Distance2D(center) >
                kCrystallizeRange + 25.0f) {
            continue;
        }
        const WallSegment segment = BuildWallSegment(
            player.Position(), center, rank);
        if (!segment.Valid) continue;
        const bool pushes = WallContains(
            segment, predicted, target.BoundingRadius());
        const Vector3 displaced = pushes
            ? WallDisplacementDestination(
                  segment, predicted, target.BoundingRadius())
            : predicted;
        Vector3 pathEnd = target.PathEnd();
        if (!pathEnd.IsValid() || pathEnd.IsZero()) {
            pathEnd = predicted + velocity * 1.05f;
        }
        const bool blocksEscape = pathEnd.IsValid() &&
            !pathEnd.IsZero() && WallBlocksPath(
                segment, predicted, pathEnd,
                target.BoundingRadius() * 0.45f);
        const bool beforeStorm = RActive && StormHits(
            RCenter, predicted, std::max(
                StormElapsedSeconds(), kStormGrowSeconds),
            target.BoundingRadius());
        const bool afterStorm = RActive && StormHits(
            RCenter, displaced, std::max(
                StormElapsedSeconds(), kStormGrowSeconds),
            target.BoundingRadius());
        const bool keepsInStorm = RActive && afterStorm &&
            (!beforeStorm || displaced.Distance2D(RCenter) <=
                              predicted.Distance2D(RCenter) + 25.0f);
        const bool allyUnsafe = WallEndangersAllies(
            segment, purpose, protectedAlly, threat);
        const bool turretAggro = pushes &&
            Engine::UnderEnemyTurret(player.Position());

        float score = TargetPriority(target) * 145.0f;
        score += pushes ? 135.0f : 0.0f;
        score += blocksEscape ? 245.0f : 0.0f;
        score += keepsInStorm ? 390.0f : 0.0f;
        score += purpose == WallPurpose::Interrupt && pushes ? 360.0f : 0.0f;
        score += purpose == WallPurpose::Peel &&
                 Engine::ValidAlly(protectedAlly) &&
                 WallSeparates(segment, protectedAlly.Position(), predicted)
            ? 420.0f : 0.0f;
        score += purpose == WallPurpose::ForceQPath && blocksEscape
            ? 210.0f : 0.0f;
        score -= allyUnsafe ? 5000.0f : 0.0f;
        score -= turretAggro && purpose != WallPurpose::Peel &&
                 purpose != WallPurpose::Disengage
            ? 1600.0f : 0.0f;
        score -= center.Distance2D(predicted) * 0.20f;

        WallPlan plan{};
        plan.Center = center;
        plan.PredictedTarget = predicted;
        plan.DisplacedTarget = displaced;
        plan.Segment = segment;
        plan.TargetId = static_cast<int>(target.NetworkId());
        plan.Purpose = purpose;
        plan.Score = score;
        plan.PushesTarget = pushes;
        plan.BlocksEscape = blocksEscape;
        plan.KeepsInStorm = keepsInStorm;
        plan.AllyUnsafe = allyUnsafe;
        plan.TurretAggro = turretAggro;
        const bool purposeSatisfied =
            (purpose == WallPurpose::KeepInStorm && keepsInStorm) ||
            (purpose == WallPurpose::ForceQPath && blocksEscape) ||
            (purpose == WallPurpose::Interrupt && pushes) ||
            (purpose == WallPurpose::Peel &&
                (pushes || blocksEscape || WallSeparates(
                    segment, protectedAlly.Position(), predicted))) ||
            (purpose == WallPurpose::AntiGapcloser &&
                (pushes || blocksEscape)) ||
            (purpose == WallPurpose::Disengage &&
                (pushes || blocksEscape)) ||
            (purpose == WallPurpose::Catch &&
                (pushes || blocksEscape)) ||
            (purpose == WallPurpose::SplitTeam && blocksEscape);
        plan.Valid = purposeSatisfied && !allyUnsafe &&
            (!turretAggro || purpose == WallPurpose::Peel ||
             purpose == WallPurpose::Disengage);
        if (plan.Valid && (!best.Valid || plan.Score > best.Score)) {
            best = plan;
        }
    }
    return best;
}

inline bool CastWall(const WallPlan& plan,
                     Mode mode,
                     bool reactive = false) {
    if (!plan.Valid || !Ready(1) || !SpellEnabled(1, mode) ||
        !CastThrottleReady(1, reactive) ||
        !HasCurrentResource(SpellCost(1))) {
        return false;
    }
    if (!reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true)) {
        return false;
    }
    if (!Engine::ControllerCastPosition(1, plan.Center)) return false;
    WCastTick = Now();
    WTargetId = plan.TargetId;
    LastWallPlan = plan;
    LastWallPurpose = plan.Purpose;
    ActiveSequence = plan.Purpose == WallPurpose::Peel ||
            plan.Purpose == WallPurpose::Disengage
        ? Sequence::PeelChain
        : Sequence::WallRedirect;
    return true;
}

inline AIHeroClient ProtectedAlly() {
    const auto player = ObjectManager::Player();
    AIHeroClient remembered = HeroByNetworkId(ProtectedAllyId);
    if (Engine::ValidAlly(remembered, 1350.0f)) return remembered;
    AIHeroClient selected = SelectProtectionAlly(
        1350.0f, true, TargetedAllyThreatId, 0.88f);
    if (Engine::ValidAlly(selected)) {
        ProtectedAllyId = static_cast<int>(selected.NetworkId());
        return selected;
    }
    return player;
}

inline float PeelThreatScore(const AIHeroClient& enemy,
                             const AIHeroClient& ally) {
    if (!Engine::ValidEnemy(enemy) || !Engine::ValidAlly(ally)) {
        return -FLT_MAX;
    }
    const float distance = enemy.Position().Distance2D(ally.Position());
    if (distance > 760.0f) return -FLT_MAX;
    float score = 820.0f - distance;
    score += TargetPriority(enemy) * 125.0f;
    score += enemy.IsDashing() ? 240.0f : 0.0f;
    score += Engine::IsHardCrowdControlled(enemy) ? -90.0f : 0.0f;
    score += static_cast<int>(enemy.NetworkId()) == TargetedAllyThreatId
        ? 260.0f : 0.0f;
    score += (100.0f - ally.HealthPercent()) * 2.2f;
    return score;
}

inline AIHeroClient SelectPeelThreat(const AIHeroClient& ally) {
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const float score = PeelThreatScore(enemy, ally);
        if (score > bestScore) {
            bestScore = score;
            best = enemy;
        }
    }
    if (Engine::ValidEnemy(best)) {
        PeelThreatId = static_cast<int>(best.NetworkId());
    } else {
        PeelThreatId = 0;
    }
    return best;
}

inline bool TryInterrupt() {
    if (InterruptTargetId == 0 || InterruptExpireTick < Now()) return false;
    const AIHeroClient target = HeroByNetworkId(InterruptTargetId);
    if (!Engine::ValidEnemy(target)) return false;

    if (QActive && FlashFrostExplosionHits(
            QMissilePosition, PredictPosition(target, 0.04f),
            target.BoundingRadius()) && !IsSpellShielded(target)) {
        return CastQDetonate(InterruptTargetId, true);
    }
    if (Bool(WallMenu, "Interrupt", true) && Ready(1)) {
        const WallPlan wall = BuildWallPlan(
            target, WallPurpose::Interrupt);
        if (wall.Valid && CastWall(wall, Mode::Automatic, true)) return true;
    }
    if (Bool(FlashFrostMenu, "Interrupt", true) && !QActive && Ready(0)) {
        const FlashFrostPlan q = BuildQPlan(
            target, QPurpose::Interrupt, true);
        const int impactTick = Now() + static_cast<int>(
            std::ceil(q.ImpactSeconds * 1000.0f));
        if (q.Valid && impactTick <= InterruptExpireTick + 80 &&
            CastFlashFrost(q, Mode::Automatic, true)) {
            return true;
        }
    }
    return false;
}

inline bool TryAntiGapcloser() {
    if (GapcloserTargetId == 0 || GapcloserExpireTick < Now()) return false;
    const AIHeroClient target = HeroByNetworkId(GapcloserTargetId);
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = ObjectManager::Player();
    const bool threatensPlayer = GapcloserEnd.IsValid() &&
        GapcloserEnd.Distance2D(player.Position()) <= 465.0f;
    if (!threatensPlayer) return false;

    if (QActive && FlashFrostExplosionHits(
            QMissilePosition, GapcloserEnd, target.BoundingRadius()) &&
        !IsSpellShielded(target)) {
        return CastQDetonate(GapcloserTargetId, true);
    }
    if (Bool(FlashFrostMenu, "AntiGapcloser", true) && !QActive && Ready(0)) {
        const FlashFrostPlan q = BuildQPlan(
            target, QPurpose::AntiGapcloser, true);
        if (q.Valid && CastFlashFrost(q, Mode::Automatic, true)) return true;
    }
    if (Bool(WallMenu, "AntiGapcloser", true) && Ready(1)) {
        const WallPlan wall = BuildWallPlan(
            target, WallPurpose::AntiGapcloser, {}, {}, GapcloserEnd);
        if (wall.Valid && CastWall(wall, Mode::Automatic, true)) return true;
    }
    if (!RActive && Ready(3) && Bool(StormMenu, "AntiGapcloser", true)) {
        const StormPlan storm = BuildStormPlan(
            target, StormPurpose::Disengage, player.Position());
        if (storm.Valid && CastStorm(storm, Mode::Automatic, true)) return true;
    }
    return false;
}

inline bool TryPeel(const AIHeroClient& ally,
                    const AIHeroClient& threat,
                    Mode mode) {
    if (!Engine::ValidAlly(ally) || !Engine::ValidEnemy(threat)) {
        return false;
    }
    const float distance = threat.Position().Distance2D(ally.Position());
    if (distance > 690.0f) return false;

    if (QActive && FlashFrostExplosionHits(
            QMissilePosition, PredictPosition(threat, 0.05f),
            threat.BoundingRadius()) && !IsSpellShielded(threat)) {
        return CastQDetonate(static_cast<int>(threat.NetworkId()), true);
    }
    if (!QActive && Ready(0) && Bool(FlashFrostMenu, "Peel", true)) {
        const FlashFrostPlan q = BuildQPlan(
            threat, QPurpose::Peel, true);
        if (q.Valid && CastFlashFrost(q, Mode::Automatic, true)) return true;
    }
    if (Ready(1) && Bool(WallMenu, "Peel", true)) {
        const WallPlan wall = BuildWallPlan(
            threat, WallPurpose::Peel, ally, threat);
        if (wall.Valid && CastWall(wall, Mode::Automatic, true)) return true;
    }
    if (!RActive && Ready(3) && Bool(StormMenu, "Peel", true) &&
        (ally.HealthPercent() <= Slider(StormMenu, "PeelAllyHp", 48) ||
         threat.IsDashing() || distance <= 280.0f)) {
        const StormPlan storm = BuildStormPlan(
            threat, StormPurpose::Peel,
            (ally.Position() + threat.Position()) * 0.5f);
        if (storm.Valid && CastStorm(storm, Mode::Automatic, true)) return true;
    }
    if (StormContainsUnit(threat, 0.0f, true)) {
        return CastFrostbite(threat, mode, true, true);
    }
    return false;
}

inline bool TargetNearStormExit(const AIHeroClient& target) {
    if (!StormContainsUnit(target, 0.15f, false)) return true;
    const Vector3 predicted = PredictPosition(target, 0.45f);
    const float futureRadius = StormRadius(
        StormElapsedSeconds() + 0.45f) + target.BoundingRadius();
    return predicted.Distance2D(RCenter) >= futureRadius - 85.0f;
}

inline bool TryWallForFlyingQ(const AIHeroClient& target, Mode mode) {
    if (!QActive || !Ready(1) || !Engine::ValidEnemy(target) ||
        !Bool(WallMenu, "ForceQPath", true) ||
        Now() - WCastTick < 500) {
        return false;
    }
    WallPlan wall = BuildWallPlan(target, WallPurpose::ForceQPath);
    if (!wall.Valid) return false;
    const bool displacedIntoPass = FlashFrostPassHits(
        QOrigin, QDirection, kFlashFrostRange,
        wall.DisplacedTarget, target.BoundingRadius());
    const bool displacedIntoExplosion = FlashFrostExplosionHits(
        QMissilePosition, wall.DisplacedTarget, target.BoundingRadius());
    if (!displacedIntoPass && !displacedIntoExplosion &&
        !wall.BlocksEscape) {
        return false;
    }
    return CastWall(wall, mode, false);
}

inline bool TryStormSequence(const AIHeroClient& target, Mode mode) {
    if (!RActive || !Engine::ValidEnemy(target)) return false;

    if (EWaitingForChill && QActive) {
        if (TryDetonateFlashFrost()) return true;
    }

    if (Ready(2) && CastFrostbite(target, mode, true, false)) {
        return true;
    }

    if (Ready(1) && TargetNearStormExit(target) &&
        Bool(WallMenu, "KeepInStorm", true)) {
        const WallPlan wall = BuildWallPlan(
            target, WallPurpose::KeepInStorm);
        if (wall.Valid && CastWall(wall, mode, false)) return true;
    }

    const bool firstEmpowered = ESequenceTargetId == target.NetworkId() &&
        EmpoweredECount == 1 && Now() - LastEmpoweredETick <= 5200;
    if (firstEmpowered && !Ready(2) && !TargetNearStormExit(target) &&
        Bool(FlashFrostMenu, "HoldForSecondE", true)) {
        ActiveSequence = Sequence::HoldQForExit;
        return false;
    }

    if (!QActive && Ready(0)) {
        const QPurpose purpose = TargetNearStormExit(target)
            ? QPurpose::StormExit : QPurpose::Catch;
        if (!firstEmpowered || TargetNearStormExit(target) ||
            !Bool(FlashFrostMenu, "HoldForSecondE", true)) {
            const FlashFrostPlan q = BuildQPlan(target, purpose, false);
            if (q.Valid && CastFlashFrost(q, mode, false)) return true;
        }
    }
    if (QActive && TryWallForFlyingQ(target, mode)) return true;
    return false;
}

inline bool TryCombo(const AIHeroClient& selected) {
    const auto player = ObjectManager::Player();
    AIHeroClient target = selected;
    if (!Engine::ValidEnemy(target, 1175.0f)) {
        target = ControllerHelpers::NearestEnemyToPlayer(selected, 1175.0f);
    }
    if (!Engine::ValidEnemy(target)) return false;

    if (RActive && TryStormSequence(target, Mode::Combo)) return true;
    if (QActive) {
        if (TryWallForFlyingQ(target, Mode::Combo)) return true;
        if (HasLiveChill(target) && Ready(2) &&
            CastFrostbite(target, Mode::Combo, true, false)) {
            return true;
        }
        return false;
    }

    const bool preSix = SpellRank(3) <= 0;
    const bool lethalQ = Ready(0) &&
        QDamage(target, true, true) >=
            target.Health() + target.AllShield();
    if (lethalQ) {
        const FlashFrostPlan q = BuildQPlan(
            target, QPurpose::Lethal, false);
        if (q.Valid && CastFlashFrost(q, Mode::Combo, false)) return true;
    }

    if (!preSix && !RActive && Ready(3) &&
        Bool(StormMenu, "Combo", true) &&
        player.Position().Distance2D(target.Position()) <=
            kStormCastRange + target.BoundingRadius() &&
        HasResourceFor({ 3, 2 },
                   StormManaPerSecond(SpellRank(3)) * 1.75f)) {
        const StormPurpose purpose =
            Engine::CountEnemiesAt(target.Position(), 650.0f) >= 2
            ? StormPurpose::Teamfight
            : StormPurpose::DoubleFrostbite;
        const StormPlan storm = BuildStormPlan(target, purpose);
        const float minimum = static_cast<float>(
            Slider(StormMenu, "MinimumScore", 360));
        if (storm.Valid && storm.IncludesSelected &&
            storm.Score >= minimum &&
            CastStorm(storm, Mode::Combo, false)) {
            return true;
        }
    }

    if (Ready(1) && Ready(0) &&
        Bool(WallMenu, "CatchBeforeQ", true) &&
        TargetCommittedToLine(target)) {
        const WallPlan wall = BuildWallPlan(
            target, WallPurpose::ForceQPath);
        if (wall.Valid && wall.BlocksEscape &&
            CastWall(wall, Mode::Combo, false)) {
            return true;
        }
    }

    if (Ready(0)) {
        const QPurpose purpose = Now() - WCastTick <= 850 &&
                WTargetId == target.NetworkId()
            ? QPurpose::WallForced : QPurpose::Catch;
        const FlashFrostPlan q = BuildQPlan(target, purpose, false);
        if (q.Valid && CastFlashFrost(q, Mode::Combo, false)) return true;
        ActiveSequence = Sequence::HoldFlashFrost;
    }

    if (Ready(2) && CastFrostbite(
            target, Mode::Combo, true, false)) {
        return true;
    }
    return false;
}

inline bool TryHarass(const AIHeroClient& selected) {
    if (!Engine::ValidEnemy(selected, 1125.0f)) return false;
    if (ControllerHelpers::PlayerManaPercent() < Slider(FlashFrostMenu, "HarassMana", 52)) {
        return false;
    }
    if (RActive) {
        if (CastFrostbite(selected, Mode::Harass, false, false)) return true;
        if (TryStormSequence(selected, Mode::Harass)) return true;
    }
    if (QActive) {
        if (TryWallForFlyingQ(selected, Mode::Harass) &&
            ConservativeComboDamage(selected, false) >=
                selected.Health() + selected.AllShield()) {
            return true;
        }
        return CastFrostbite(selected, Mode::Harass, false, false);
    }
    if (HasLiveChill(selected) && Ready(2) &&
        CastFrostbite(selected, Mode::Harass, false, false)) {
        return true;
    }
    if (Ready(0) && Bool(FlashFrostMenu, "Harass", true)) {
        const FlashFrostPlan q = BuildQPlan(
            selected, QPurpose::DoubleHit, false);
        if (q.Valid && (q.TargetCommitted || q.Guaranteed) &&
            CastFlashFrost(q, Mode::Harass, false)) {
            return true;
        }
    }
    if (!RActive && Ready(3) && Bool(StormMenu, "Harass", false) &&
        ControllerHelpers::PlayerManaPercent() >= Slider(StormMenu, "HarassMana", 82)) {
        const StormPlan storm = BuildStormPlan(
            selected, StormPurpose::Catch);
        if (storm.Valid && storm.IncludesSelected &&
            CastStorm(storm, Mode::Harass, false)) {
            return true;
        }
    }
    return CastFrostbite(selected, Mode::Harass, false, false);
}

inline AIHeroClient NearestPursuer() {
    const auto player = ObjectManager::Player();
    AIHeroClient best{};
    float bestDistance = FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1150.0f)) continue;
        const float distance = enemy.Position().Distance2D(player.Position());
        if (distance < bestDistance) {
            bestDistance = distance;
            best = enemy;
        }
    }
    return best;
}

inline bool TryFlee(const AIHeroClient& selected) {
    AIHeroClient pursuer = Engine::ValidEnemy(selected)
        ? selected : NearestPursuer();
    if (!Engine::ValidEnemy(pursuer)) return false;
    const auto player = ObjectManager::Player();

    if (QActive) {
        if (FlashFrostExplosionHits(
                QMissilePosition, PredictPosition(pursuer, 0.05f),
                pursuer.BoundingRadius()) &&
            !IsSpellShielded(pursuer)) {
            return CastQDetonate(static_cast<int>(pursuer.NetworkId()), true);
        }
        return false;
    }
    if (Ready(0) && Bool(FlashFrostMenu, "Flee", true)) {
        const FlashFrostPlan q = BuildQPlan(
            pursuer, QPurpose::Peel, true);
        if (q.Valid && CastFlashFrost(q, Mode::Flee, true)) return true;
    }
    if (Ready(1) && Bool(WallMenu, "Flee", true)) {
        Vector3 barrier = (player.Position() + pursuer.Position()) * 0.5f;
        const WallPlan wall = BuildWallPlan(
            pursuer, WallPurpose::Disengage, player, pursuer, barrier);
        if (wall.Valid && CastWall(wall, Mode::Flee, true)) return true;
    }
    if (!RActive && Ready(3) && Bool(StormMenu, "Flee", true)) {
        const Vector3 center = player.Position().Extend(
            pursuer.Position(), 145.0f);
        const StormPlan storm = BuildStormPlan(
            pursuer, StormPurpose::Disengage, center);
        if (storm.Valid && CastStorm(storm, Mode::Flee, true)) return true;
    }
    return RActive && StormContainsUnit(pursuer, 0.0f, true) &&
           CastFrostbite(pursuer, Mode::Flee, true, true);
}

inline bool TryDetonateFarmQ(Mode mode) {
    if (!QActive || QMissilePosition.IsZero() || PlayerCannotRecastQ()) {
        return false;
    }
    int hits = 0;
    int lastHits = 0;
    int bestId = 0;
    float bestHealth = FLT_MAX;
    auto inspect = [&](const AIBaseClient& unit) {
        if (!ValidHostileUnitInGameplayRange(unit, 1400.0f)) return;
        const Vector3 predicted = PredictPosition(unit, 0.05f);
        if (!FlashFrostExplosionHits(
                QMissilePosition, predicted, unit.BoundingRadius())) {
            return;
        }
        ++hits;
        const float health = Engine::RuntimeSpells[0]
            ? Engine::RuntimeSpells[0]->GetHealthPrediction(unit)
            : unit.Health();
        if (QDamage(unit, false, true) >= health) ++lastHits;
        if (health < bestHealth) {
            bestHealth = health;
            bestId = static_cast<int>(unit.NetworkId());
        }
    };
    if (mode == Mode::Jungle) {
        for (const auto& monster : GameObjects::Jungle()) inspect(monster);
    } else {
        for (const auto& minion : GameObjects::EnemyMinions()) inspect(minion);
    }
    const int minimum = mode == Mode::Jungle
        ? 1 : Slider(FarmMenu, "QDetonateHits", 2);
    if (hits >= minimum || lastHits >= 1 ||
        (QAutoEndTick > 0 && QAutoEndTick - Now() <= 150)) {
        return CastQDetonate(bestId, true);
    }
    return false;
}

inline FlashFrostPlan BuildFarmQPlan(Mode mode) {
    FlashFrostPlan best{};
    const auto player = ObjectManager::Player();
    auto evaluate = [&](const AIBaseClient& anchor) {
        if (!ValidHostileUnitInGameplayRange(
                anchor, kFlashFrostRange + 80.0f)) return;
        const Vector3 predicted = PredictPosition(anchor, 0.45f);
        const Vector3 direction = Direction2D(player.Position(), predicted);
        if (direction.IsZero()) return;
        int hits = 0;
        int valuable = 0;
        auto count = [&](const AIBaseClient& unit) {
            if (!ValidHostileUnitInGameplayRange(unit, 1300.0f)) return;
            const Vector3 unitPrediction = PredictPosition(unit, 0.55f);
            if (FlashFrostPassHits(
                    player.Position(), direction, kFlashFrostRange,
                    unitPrediction, unit.BoundingRadius())) {
                ++hits;
                if (IsEpicMonster(unit) || unit.MaxHealth() >= 1800.0f) {
                    ++valuable;
                }
            }
        };
        if (mode == Mode::Jungle) {
            for (const auto& unit : GameObjects::Jungle()) count(unit);
        } else {
            for (const auto& unit : GameObjects::EnemyMinions()) count(unit);
        }
        const int minimum = mode == Mode::Jungle
            ? 1 : Slider(FarmMenu, "QMinimumHits", 3);
        const float score = static_cast<float>(hits) * 120.0f +
                            static_cast<float>(valuable) * 260.0f;
        if (hits < minimum || (best.Valid && score <= best.Score)) return;
        best.Aim = player.Position() + direction * kFlashFrostRange;
        best.TargetPosition = predicted;
        best.Direction = direction;
        best.TargetId = static_cast<int>(anchor.NetworkId());
        best.Hitchance = SDK::HitChance::High;
        best.Purpose = mode == Mode::Jungle
            ? QPurpose::Jungle : QPurpose::Farm;
        best.ImpactSeconds = FlashFrostTravelSeconds(
            player.Position().Distance2D(predicted));
        best.Score = score;
        best.TargetCommitted = true;
        best.Guaranteed = true;
        best.Valid = true;
    };
    if (mode == Mode::Jungle) {
        for (const auto& unit : GameObjects::Jungle()) evaluate(unit);
    } else {
        for (const auto& unit : GameObjects::EnemyMinions()) evaluate(unit);
    }
    return best;
}

inline StormPlan BuildFarmStormPlan(Mode mode) {
    StormPlan best{};
    const auto player = ObjectManager::Player();
    auto evaluate = [&](const AIBaseClient& anchor) {
        if (!ValidHostileUnitInGameplayRange(
                anchor, kStormCastRange + 100.0f)) return;
        const Vector3 center = PredictPosition(anchor, 0.25f);
        if (player.Position().Distance2D(center) > kStormCastRange + 25.0f) {
            return;
        }
        StormPlan plan{};
        plan.Center = center;
        plan.TargetId = static_cast<int>(anchor.NetworkId());
        plan.Purpose = mode == Mode::Jungle
            ? (IsEpicMonster(anchor)
                ? StormPurpose::Objective : StormPurpose::Jungle)
            : StormPurpose::Waveclear;
        auto count = [&](const AIBaseClient& unit) {
            if (ValidHostileUnitInGameplayRange(unit, 1250.0f) && StormHits(
                    center, PredictPosition(unit, 0.65f),
                    kStormGrowSeconds, unit.BoundingRadius())) {
                ++plan.HitCount;
                if (IsEpicMonster(unit)) ++plan.PriorityHits;
            }
        };
        if (mode == Mode::Jungle) {
            for (const auto& unit : GameObjects::Jungle()) count(unit);
        } else {
            for (const auto& unit : GameObjects::EnemyMinions()) count(unit);
        }
        plan.Score = plan.HitCount * 125.0f +
                     plan.PriorityHits * 500.0f;
        const int minimum = mode == Mode::Jungle
            ? Slider(FarmMenu, "RMinimumJungle", 2)
            : Slider(FarmMenu, "RMinimumLane", 5);
        plan.Valid = plan.HitCount >= minimum || plan.PriorityHits > 0;
        if (plan.Valid && (!best.Valid || plan.Score > best.Score)) best = plan;
    };
    if (mode == Mode::Jungle) {
        for (const auto& unit : GameObjects::Jungle()) evaluate(unit);
    } else {
        for (const auto& unit : GameObjects::EnemyMinions()) evaluate(unit);
    }
    return best;
}

inline AIBaseClient BestFarmFrostbiteTarget(Mode mode, bool lastHitOnly) {
    AIBaseClient best{};
    float bestScore = -FLT_MAX;
    auto evaluate = [&](const AIBaseClient& unit) {
        if (!ValidHostileUnitInGameplayRange(
                unit, kFrostbiteRange + 40.0f)) return;
        const float impact = FrostbiteImpactSeconds(
            ObjectManager::Player().Position().Distance2D(unit.Position()));
        const float health = Engine::RuntimeSpells[2]
            ? Engine::RuntimeSpells[2]->GetHealthPrediction(unit)
            : unit.Health();
        const bool chilled = HasLiveChill(unit);
        const float damage = EDamage(unit, chilled);
        const bool lethal = damage >= health;
        if (lastHitOnly && !lethal) return;
        float score = lethal ? 600.0f : 0.0f;
        score += chilled ? 260.0f : -140.0f;
        score += IsEpicMonster(unit) ? 900.0f : 0.0f;
        score += unit.MaxHealth() * 0.015f;
        score -= impact * 15.0f;
        if (score > bestScore) {
            bestScore = score;
            best = unit;
        }
    };
    if (mode == Mode::Jungle) {
        for (const auto& unit : GameObjects::Jungle()) evaluate(unit);
    } else {
        for (const auto& unit : GameObjects::EnemyMinions()) evaluate(unit);
    }
    return best;
}

inline bool TryFarm(Mode mode) {
    const bool jungle = mode == Mode::Jungle;
    const bool lastHit = mode == Mode::LastHit;
    const int manaFloor = jungle
        ? Slider(FarmMenu, "JungleMana", 30)
        : Slider(FarmMenu, "LaneMana", 55);
    if (ControllerHelpers::PlayerManaPercent() < manaFloor) return false;

    if (QActive && TryDetonateFarmQ(mode)) return true;
    if (RActive) {
        const AIBaseClient eTarget = BestFarmFrostbiteTarget(
            mode, lastHit || !jungle);
        if (eTarget.IsValid() && Bool(FarmMenu, "UseE", true) &&
            CastFrostbite(eTarget, mode, true, false)) {
            return true;
        }
        int contacts = 0;
        if (jungle) {
            for (const auto& unit : GameObjects::Jungle()) {
                if (ValidHostileUnitInGameplayRange(unit, 1200.0f) &&
                    StormContainsUnit(unit, 0.12f, false)) ++contacts;
            }
        } else {
            for (const auto& unit : GameObjects::EnemyMinions()) {
                if (ValidHostileUnitInGameplayRange(unit, 1200.0f) &&
                    StormContainsUnit(unit, 0.12f, false)) ++contacts;
            }
        }
        const int stopBelow = jungle ? 1 : 2;
        if (contacts < stopBelow &&
            Now() - RCastTick >= kStormMinimumRecastMs) {
            return CancelStorm(false);
        }
        return false;
    }

    if (!lastHit && Ready(3) &&
        Bool(FarmMenu, jungle ? "JungleR" : "LaneR", jungle) &&
        HasCurrentResource(SpellCost(3) + StormManaReserve(mode))) {
        const StormPlan storm = BuildFarmStormPlan(mode);
        if (storm.Valid && CastStorm(storm, mode, false)) return true;
    }
    const AIBaseClient eTarget = BestFarmFrostbiteTarget(
        mode, lastHit || !jungle);
    if (eTarget.IsValid() && Bool(FarmMenu, "UseE", true) &&
        CastFrostbite(eTarget, mode, true, false)) {
        return true;
    }
    if (!lastHit && Ready(0) && Bool(FarmMenu, "UseQ", true)) {
        const FlashFrostPlan q = BuildFarmQPlan(mode);
        if (q.Valid && CastFlashFrost(q, mode, false)) return true;
    }
    return false;
}

inline bool TryAutomaticKillSecure() {
    if (!Bool(FrostbiteMenu, "KillSecure", true)) return false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1125.0f) ||
            IsCommonUntargetableOrImmune(enemy)) {
            continue;
        }
        if (Ready(2) &&
            EDamage(enemy, HasLiveChill(enemy)) >=
                enemy.Health() + enemy.AllShield() &&
            CastFrostbite(enemy, Mode::Automatic, true, true)) {
            return true;
        }
        if (!QActive && Ready(0) &&
            QDamage(enemy, true, true) >=
                enemy.Health() + enemy.AllShield()) {
            const FlashFrostPlan q = BuildQPlan(
                enemy, QPurpose::Lethal, true);
            if (q.Valid && CastFlashFrost(
                    q, Mode::Automatic, true)) {
                return true;
            }
        }
    }
    return false;
}

inline Posture ChoosePosture(Mode mode,
                             const AIHeroClient& selected,
                             const AIHeroClient& ally,
                             const AIHeroClient& threat) {
    if (EggActive) return Posture::Egg;
    if (mode == Mode::Flee) return Posture::Disengage;
    if (Engine::ValidAlly(ally) && Engine::ValidEnemy(threat) &&
        threat.Position().Distance2D(ally.Position()) <= 690.0f) {
        return Posture::Peel;
    }
    if (RActive) return Posture::Zone;
    if (mode == Mode::LaneClear || mode == Mode::LastHit) {
        if (HasNearbyJungleTarget(850.0f)) {
            for (const auto& monster : GameObjects::Jungle()) {
                if (IsEpicMonster(monster) &&
                    ValidHostileUnitInGameplayRange(monster, 1100.0f)) {
                    return Posture::Objective;
                }
            }
        }
        return Posture::LaneControl;
    }
    if (mode == Mode::Combo && Engine::ValidEnemy(selected)) {
        return Engine::UnderEnemyTurret(selected.Position())
            ? Posture::Siege : Posture::Catch;
    }
    if (mode == Mode::Harass) return Posture::LaneControl;
    return Posture::Neutral;
}

inline void RefreshPassiveState() {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    RebirthReady = player.HasBuff("RebirthReady") ||
                   player.HasBuff("rebirthready") ||
                   player.HasBuff("AniviaRebirthReady");
    RebirthCooldown = player.HasBuff("RebirthCooldown") ||
                      player.HasBuff("rebirthcooldown") ||
                      RebirthCooldownUntil > Now();
    const bool eggBuff = player.HasBuff("Rebirth") ||
                         player.HasBuff("AniviaEgg") ||
                         player.HasBuff("EggTimer");
    if (eggBuff) {
        EggActive = true;
        if (EggUntilTick < Now()) EggUntilTick = Now() + kEggDurationMs;
    } else if (EggActive && EggUntilTick > 0 && Now() > EggUntilTick + 180) {
        EggActive = false;
        EggUntilTick = 0;
    }
}

inline void RefreshState() {
    RefreshPassiveState();
    RefreshTrackedQ();
    RefreshTrackedStorm();
    const int now = Now();
    for (auto& mark : ChillMarks) {
        if (mark.ExpireTick < now) mark = {};
    }
    if (GapcloserExpireTick < now) GapcloserTargetId = 0;
    if (InterruptExpireTick < now) InterruptTargetId = 0;
    if (TargetedAllyThreatUntil < now) TargetedAllyThreatId = 0;
    if (IncomingThreatUntil < now) RecentIncomingPressure *= 0.84f;
    if (EWaitingForChill && (EImpactTick < now || ETargetId == 0)) {
        EWaitingForChill = false;
    }
    if (EImpactTick > 0 && now > EImpactTick + 220) {
        EImpactTick = 0;
        ETargetId = 0;
    }
    if (LastEmpoweredETick > 0 && now - LastEmpoweredETick > 6800) {
        EmpoweredECount = 0;
        ESequenceTargetId = 0;
    }
    if (QDetonationRequested && now - QDetonationTick > 240 &&
        !QMissileObserved) {
        ClearQState();
    }
    if (RebirthCooldownUntil > 0 && now > RebirthCooldownUntil) {
        RebirthCooldownUntil = 0;
        RebirthCooldown = false;
    }
    if (EggActive) ActiveSequence = Sequence::Egg;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshState();
    if (EggActive) return false;

    const AIHeroClient ally = ProtectedAlly();
    const AIHeroClient threat = Engine::ValidAlly(ally)
        ? SelectPeelThreat(ally) : AIHeroClient{};
    CurrentPosture = ChoosePosture(mode, selected, ally, threat);

    if (QActive && TryDetonateFlashFrost()) return true;
    if (TryInterrupt()) return true;
    if (TryAntiGapcloser()) return true;
    if (Engine::ValidAlly(ally) && Engine::ValidEnemy(threat) &&
        Bool(TacticsMenu, "PeelBeforeDamage", true) &&
        TryPeel(ally, threat, mode == Mode::None
            ? Mode::Automatic : mode)) {
        return true;
    }
    if (TryManageStorm(selected, mode == Mode::None
            ? Mode::Automatic : mode)) {
        return true;
    }
    if (TryAutomaticKillSecure()) return true;

    if (mode == Mode::Flee) return TryFlee(selected);
    if (mode == Mode::Combo) return TryCombo(selected);
    if (mode == Mode::Harass) return TryHarass(selected);
    if (mode == Mode::LaneClear) {
        if (HasNearbyJungleTarget(850.0f) &&
            TryFarm(Mode::Jungle)) return true;
        return TryFarm(Mode::LaneClear);
    }
    if (mode == Mode::LastHit) return TryFarm(Mode::LastHit);
    return false;
}

inline Vector3 EventCastPosition(
    const SDK::Events::ProcessSpellEventArgs& args,
    const Vector3& fallback = {}) {
    if (args.CastPosition.IsValid() && !args.CastPosition.IsZero()) {
        return args.CastPosition;
    }
    if (args.EndPosition.IsValid() && !args.EndPosition.IsZero()) {
        return args.EndPosition;
    }
    return fallback;
}

inline void ObserveQDetonation() {
    QDetonationRequested = true;
    QDetonationTick = Now();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) || IsSpellShielded(enemy)) continue;
        if (FlashFrostExplosionHits(
                QMissilePosition, PredictPosition(enemy, 0.035f),
                enemy.BoundingRadius())) {
            MarkChill(static_cast<int>(enemy.NetworkId()),
                      kChillDurationMs, false);
        }
    }
}

inline void ObserveManualQ(
    const SDK::Events::ProcessSpellEventArgs& args,
    bool controllerOwned) {
    if (QActive || IsQDetonateName(args.SpellName) ||
        ControllerHelpers::SpellEventNameContainsAny(
            args, { "FlashFrostSpell2", "Detonate" })) {
        ObserveQDetonation();
        return;
    }
    const auto player = ObjectManager::Player();
    const Vector3 origin = args.StartPosition.IsValid() &&
            !args.StartPosition.IsZero()
        ? args.StartPosition : player.Position();
    Vector3 end = EventCastPosition(args, origin);
    Vector3 direction = Direction2D(origin, end);
    if (direction.IsZero()) return;
    end = origin + direction * kFlashFrostRange;
    QActive = true;
    QMissileObserved = false;
    QDetonationRequested = false;
    QCastTick = Now();
    QLastSeenTick = QCastTick;
    QTargetId = args.TargetNetworkId != 0
        ? static_cast<int>(args.TargetNetworkId)
        : static_cast<int>(args.Target.NetworkId);
    QShieldBreakTargetId = 0;
    QOrigin = origin;
    QDirection = direction;
    QCastEnd = end;
    QMissilePosition = origin;
    QPassedIds.fill(0);
    QAutoEndTick = QCastTick + static_cast<int>(
        std::ceil((kFlashFrostCastSeconds +
                   kFlashFrostRange / kFlashFrostSpeed) * 1000.0f));
    LastQPurpose = QPurpose::None;
    ActiveSequence = Sequence::QDoubleHit;
    if (!controllerOwned) LastQPlan = {};
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const bool ours = args.Slot >= 0 && args.Slot < 4 &&
                      Engine::WasControllerCast(args.Slot);
    if (args.Slot == 0 || SpellEventNameContains(args, "FlashFrost")) {
        ObserveManualQ(args, ours);
        return;
    }
    if (args.Slot == 1 || SpellEventNameContains(args, "Crystallize")) {
        WCastTick = Now();
        WTargetId = args.TargetNetworkId != 0
            ? static_cast<int>(args.TargetNetworkId)
            : static_cast<int>(args.Target.NetworkId);
        if (!ours) {
            LastWallPlan = {};
            LastWallPlan.Center = EventCastPosition(args);
            LastWallPurpose = WallPurpose::None;
        }
        return;
    }
    if (args.Slot == 2 || SpellEventNameContains(args, "Frostbite")) {
        ECastTick = Now();
        ETargetId = args.TargetNetworkId != 0
            ? static_cast<int>(args.TargetNetworkId)
            : static_cast<int>(args.Target.NetworkId);
        const AIBaseClient target = UnitByNetworkId(ETargetId);
        EImpactTick = ECastTick + static_cast<int>(std::ceil(
            FrostbiteImpactSeconds(target.IsValid()
                ? ObjectManager::Player().Position().Distance2D(target.Position())
                : 450.0f) * 1000.0f));
        if (!ours) EWaitingForChill = false;
        return;
    }
    if (args.Slot == 3 || SpellEventNameContains(args, "GlacialStorm")) {
        if (RActive && Now() - RCastTick >= 400) {
            ClearStormState(false);
            return;
        }
        RActive = true;
        RObjectObserved = false;
        RWasManual = !ours;
        RCastTick = Now();
        RLastSeenTick = RCastTick;
        RLastContactTick = RCastTick;
        RNoContactSince = 0;
        RTargetId = args.TargetNetworkId != 0
            ? static_cast<int>(args.TargetNetworkId)
            : static_cast<int>(args.Target.NetworkId);
        RCenter = EventCastPosition(args, ObjectManager::Player().Position());
        LastStormPurpose = ours ? LastStormPurpose : StormPurpose::Manual;
        ActiveSequence = Sequence::StormGrow;
    }
}

inline void ObserveEnemyCast(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid() || IsLocalPlayer(args.Sender)) return;
    const auto player = ObjectManager::Player();
    const std::uint32_t targetId = args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally) ||
            ally.NetworkId() == player.NetworkId() ||
            targetId != static_cast<std::uint32_t>(ally.NetworkId())) {
            continue;
        }
        TargetedAllyThreatId = static_cast<int>(args.Sender.NetworkId);
        TargetedAllyThreatUntil = Now() + std::clamp(
            ControllerHelpers::NormalizedCastDelayMs(args.CastDelay, 250) +
                750,
            550, 1900);
        ProtectedAllyId = static_cast<int>(ally.NetworkId());
        break;
    }

    const auto analysis = AnalyzeEnemyCast(
        args, 220.0f, 110.0f, 320, 250, 220, 1600, 500);
    if (!analysis.Valid) return;
    if (analysis.TargetsPlayer || analysis.CrossesPlayer) {
        IncomingThreatUntil = std::max(
            IncomingThreatUntil,
            std::max(analysis.CommitmentUntilTick,
                     analysis.LineThreatUntilTick));
        RecentIncomingPressure = std::min(
            player.MaxHealth(), RecentIncomingPressure +
            (args.IsAutoAttack
                ? SDK::Damage::GetAutoAttackDamage(
                      analysis.Enemy, player, true)
                : player.MaxHealth() * 0.14f));
    }
}

inline void OnDoCast(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick)) return;
    if (!IsLocalPlayer(args.Sender)) ObserveEnemyCast(args);
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args,
                            bool added) {
    if (!args.Sender.IsValid() || !args.BuffName[0]) return;
    const int senderId = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "RebirthReady")) {
            RebirthReady = added;
            if (added) RebirthCooldown = false;
            return;
        }
        if (Engine::TextContains(args.BuffName, "RebirthCooldown")) {
            RebirthCooldown = added;
            if (added) {
                RebirthReady = false;
                RebirthCooldownUntil = args.EndTime > Game::Time()
                    ? Now() + static_cast<int>(
                        (args.EndTime - Game::Time()) * 1000.0f)
                    : Now() + 240000;
            }
            return;
        }
        if ((NameEquals(args.BuffName, "Rebirth") ||
             Engine::TextContains(args.BuffName, "AniviaEgg") ||
             Engine::TextContains(args.BuffName, "EggTimer")) &&
            !Engine::TextContains(args.BuffName, "Ready") &&
            !Engine::TextContains(args.BuffName, "Cooldown")) {
            EggActive = added;
            EggUntilTick = added
                ? (args.EndTime > Game::Time()
                    ? Now() + static_cast<int>(
                        (args.EndTime - Game::Time()) * 1000.0f)
                    : Now() + kEggDurationMs)
                : 0;
            if (added) {
                RebirthReady = false;
                ActiveSequence = Sequence::Egg;
                if (RActive) ClearStormState(false);
            }
            return;
        }
    }

    if (Engine::TextContains(args.BuffName, "Chilled") ||
        NameEquals(args.BuffName, "chilled") ||
        Engine::TextContains(args.BuffName, "AniviaUlt")) {
        if (added) {
            const int duration = args.EndTime > Game::Time()
                ? static_cast<int>((args.EndTime - Game::Time()) * 1000.0f)
                : kChillDurationMs;
            MarkChill(senderId, duration, true);
        } else {
            for (auto& mark : ChillMarks) {
                if (mark.NetworkId == senderId) mark = {};
            }
        }
    }
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !ObjectEventIsAllied(args)) return;
    if (IsStormName(args.Sender.Name, args.Sender.CharacterName)) {
        if (!RActive || RCenter.IsZero() ||
            args.Sender.Position.Distance2D(RCenter) <= 280.0f) {
            RActive = true;
            RObjectObserved = true;
            RObjectNetworkId = static_cast<int>(args.Sender.NetworkId);
            RCenter = args.Sender.Position;
            RLastSeenTick = Now();
            if (RCastTick <= 0) RCastTick = Now();
        }
        return;
    }
    if ((Engine::TextContains(args.Sender.Name, "cryo_FlashFrost") ||
         Engine::TextContains(args.Sender.CharacterName, "FlashFrost")) &&
        QActive && Now() - QCastTick <= 1600) {
        QMissileObserved = true;
        QMissileNetworkId = static_cast<int>(args.Sender.NetworkId);
        QMissilePosition = args.Sender.Position;
        QLastSeenTick = Now();
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id == RObjectNetworkId ||
        (ObjectEventIsAllied(args) &&
         IsStormName(args.Sender.Name, args.Sender.CharacterName))) {
        ClearStormState(true);
    }
    if (id == QMissileNetworkId ||
        Engine::TextContains(args.Sender.Name, "cryo_FlashFrost")) {
        ClearQState();
    }
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args) ||
        !IsQMissileName(args.SpellName, args.MissileName)) {
        return;
    }
    QActive = true;
    QMissileObserved = true;
    QMissileNetworkId = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    QOrigin = args.StartPosition.IsValid() && !args.StartPosition.IsZero()
        ? args.StartPosition : QOrigin;
    QCastEnd = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
        ? args.EndPosition : QCastEnd;
    QDirection = Direction2D(QOrigin, QCastEnd);
    QMissilePosition = args.Sender.Position.IsValid()
        ? args.Sender.Position : QOrigin;
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
    if (id == QMissileNetworkId || QMissileNetworkId == 0) ClearQState();
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!QActive || QMissilePosition.IsZero() || QTargetId == 0) return;
    const AIHeroClient target = HeroByNetworkId(QTargetId);
    if (!Engine::ValidEnemy(target)) return;
    const Vector3 predicted = PredictPosition(target, 0.10f);
    const bool detonationNow = DoubleHitDetonationWindow(
        QOrigin, QDirection, QMissilePosition,
        predicted, target.BoundingRadius(),
        static_cast<float>(Slider(FlashFrostMenu, "PassOvershoot", 8)));
    const bool race = EWaitingForChill && EImpactTick - Now() <= 300;
    if ((detonationNow || race) &&
        Bool(FlashFrostMenu, "ProtectDetonation", true)) {
        args.Process = false;
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::LaneControl: return "lane-control";
    case Posture::Catch: return "catch";
    case Posture::Zone: return "zone";
    case Posture::Peel: return "peel";
    case Posture::Disengage: return "disengage";
    case Posture::Siege: return "siege";
    case Posture::Objective: return "objective";
    case Posture::Egg: return "egg";
    default: return "neutral";
    }
}

inline const char* SequenceName(Sequence sequence) {
    switch (sequence) {
    case Sequence::HoldFlashFrost: return "hold-Q";
    case Sequence::QDoubleHit: return "Q-pass->burst";
    case Sequence::QSpellShieldBreak: return "Q-shield->burst";
    case Sequence::PrecastFrostbite: return "E-before-Chill";
    case Sequence::WallRedirect: return "wall-redirect";
    case Sequence::StormGrow: return "R-growing";
    case Sequence::FirstEmpoweredFrostbite: return "E1";
    case Sequence::HoldQForExit: return "hold-Q-for-exit";
    case Sequence::SecondEmpoweredFrostbite: return "E2";
    case Sequence::RelocateStorm: return "relocate-R";
    case Sequence::PeelChain: return "peel-chain";
    case Sequence::JungleCycle: return "jungle";
    case Sequence::Egg: return "rebirth";
    default: return "none";
    }
}

inline const char* WallPurposeName(WallPurpose purpose) {
    switch (purpose) {
    case WallPurpose::KeepInStorm: return "keep-in-R";
    case WallPurpose::ForceQPath: return "force-Q-path";
    case WallPurpose::Catch: return "catch";
    case WallPurpose::SplitTeam: return "split";
    case WallPurpose::Peel: return "peel";
    case WallPurpose::AntiGapcloser: return "anti-gap";
    case WallPurpose::Interrupt: return "interrupt";
    case WallPurpose::Disengage: return "disengage";
    default: return "hold";
    }
}

inline const char* StormPurposeName(StormPurpose purpose) {
    switch (purpose) {
    case StormPurpose::Catch: return "catch";
    case StormPurpose::Zone: return "zone";
    case StormPurpose::Peel: return "peel";
    case StormPurpose::Disengage: return "disengage";
    case StormPurpose::Teamfight: return "teamfight";
    case StormPurpose::DoubleFrostbite: return "double-E";
    case StormPurpose::Waveclear: return "wave";
    case StormPurpose::Jungle: return "jungle";
    case StormPurpose::Objective: return "objective";
    case StormPurpose::Manual: return "manual";
    default: return "hold";
    }
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;

    if (Bool(CoachMenu, "DrawQ", true)) {
        Drawing::DrawCircle(player.Position(), kFlashFrostRange,
                            0x5578DDF5u, 1.2f, 80);
        if (QActive) {
            Drawing::DrawLine(QOrigin, QCastEnd,
                              0xAA78DDF5u, 1.8f);
            if (QMissilePosition.IsValid() && !QMissilePosition.IsZero()) {
                Drawing::DrawCircle(QMissilePosition,
                                    kFlashFrostExplosionRadius,
                                    0xAA8DEBFFu, 2.0f, 56);
            }
        } else if (LastQPlan.Valid && Now() - QCastTick <= 900) {
            Drawing::DrawLine(player.Position(), LastQPlan.Aim,
                              0x9978DDF5u, 1.6f);
        }
    }
    if (Bool(CoachMenu, "DrawWall", true) &&
        LastWallPlan.Segment.Valid &&
        Now() - WCastTick <= kWallDurationMs) {
        Drawing::DrawLine(LastWallPlan.Segment.Start,
                          LastWallPlan.Segment.End,
                          LastWallPlan.AllyUnsafe
                              ? 0xFFFF5D73u : 0xFF83E9FFu,
                          5.0f);
        Drawing::DrawCircle(LastWallPlan.Center, 35.0f,
                            0xFF83E9FFu, 2.0f, 30);
    }
    if (Bool(CoachMenu, "DrawStorm", true)) {
        Drawing::DrawCircle(player.Position(), kStormCastRange,
                            0x445F8DE8u, 1.0f, 72);
        if (RActive && RCenter.IsValid() && !RCenter.IsZero()) {
            const float radius = StormRadius(StormElapsedSeconds());
            Drawing::DrawCircle(RCenter, radius,
                                StormFullyFormed()
                                    ? 0xCC74DFFFu : 0xAA557FCFu,
                                StormFullyFormed() ? 2.8f : 1.8f, 72);
            Drawing::DrawCircle(RCenter, kStormFullRadius,
                                0x5574DFFFu, 1.0f, 72);
        } else if (LastStormPlan.Valid) {
            Drawing::DrawCircle(LastStormPlan.Center,
                                kStormFullRadius,
                                0x5574DFFFu, 1.2f, 72);
        }
    }
    if (Bool(CoachMenu, "DrawChill", false)) {
        for (const auto& mark : ChillMarks) {
            if (mark.NetworkId == 0 || mark.ExpireTick < Now()) continue;
            const AIBaseClient unit = UnitByNetworkId(mark.NetworkId);
            if (unit.IsValid()) {
                Drawing::DrawCircle(
                    unit.Position(), unit.BoundingRadius() + 25.0f,
                    mark.Confirmed ? 0xFF9CF6FFu : 0x889CF6FFu,
                    mark.Confirmed ? 2.2f : 1.2f, 38);
            }
        }
    }
    if (Bool(CoachMenu, "DrawPeel", true)) {
        const AIHeroClient ally = ProtectedAlly();
        const AIHeroClient threat = HeroByNetworkId(PeelThreatId);
        if (Engine::ValidAlly(ally)) {
            Drawing::DrawCircle(ally.Position(), 100.0f,
                                0xAA68E8FFu, 1.8f, 42);
        }
        if (Engine::ValidAlly(ally) && Engine::ValidEnemy(threat)) {
            Drawing::DrawLine(ally.Position(), threat.Position(),
                              0xFFFF657Bu, 2.2f);
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char state[420]{};
            const float stormMana = RActive
                ? StormManaAfter(CurrentResource(), SpellRank(3), 1.0f)
                : CurrentResource();
            _snprintf_s(
                state, sizeof(state), _TRUNCATE,
                "Anivia one-trick | %s | %s | Q %s %.0f | W %s | E %d%s | R %s %.1fs %.0fm | P %s",
                PostureName(CurrentPosture), SequenceName(ActiveSequence),
                QActive ? "FLIGHT" : "hold",
                QActive && !QMissilePosition.IsZero()
                    ? AlongRay(QOrigin, QDirection, QMissilePosition) : 0.0f,
                WallPurposeName(LastWallPurpose),
                EmpoweredECount, EWaitingForChill ? " race" : "",
                StormPurposeName(LastStormPurpose),
                StormElapsedSeconds(), stormMana,
                EggActive ? "EGG" : (RebirthReady ? "ready" :
                    (RebirthCooldown ? "cooldown" : "unknown")));
            Drawing::DrawText(screen.x - 240.0f, screen.y - 122.0f,
                              0xFFD8F7FFu, state);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "AniviaOneTrick", "Anivia one-trick mechanics"));
    TacticsMenu->Add(new MenuBool(
        "PeelBeforeDamage", "Protect a pressured ally before starting damage", true));
    TacticsMenu->Add(new MenuSeparator(
        "Ownership", "Movement, attack-move, Flash and player positioning always remain manual."));

    FlashFrostMenu = TacticsMenu->AddSubMenu(new Menu(
        "FlashFrost", "Q pressure, pass-through and detonation"));
    FlashFrostMenu->Add(new MenuBool(
        "HoldPressure", "Hold Q until the target commits or the line is forced", true));
    FlashFrostMenu->Add(new MenuSlider(
        "HoldBeyond", "Hold ordinary Q beyond this range", 720, 350, 1000));
    FlashFrostMenu->Add(new MenuSlider(
        "PassOvershoot", "Minimum Q overshoot before double-hit detonation", 8, 0, 80));
    FlashFrostMenu->Add(new MenuSlider(
        "DetonateAoe", "Detonate Q for this many explosion targets", 2, 1, 5));
    FlashFrostMenu->Add(new MenuBool(
        "BreakSpellShield", "Use Q pass to consume an ordinary spell shield, then explode", true));
    FlashFrostMenu->Add(new MenuBool(
        "HoldForSecondE", "After E1, keep Q as exit peel/setup for E2", true));
    FlashFrostMenu->Add(new MenuBool(
        "ProtectDetonation", "Cancel only an AA that would lose a Q/E timing window", true));
    FlashFrostMenu->Add(new MenuBool(
        "Interrupt", "Use Q to interrupt when impact arrives in time", true));
    FlashFrostMenu->Add(new MenuBool(
        "AntiGapcloser", "Use Q against a committed gapcloser", true));
    FlashFrostMenu->Add(new MenuBool(
        "Peel", "Use Q to peel the protected ally", true));
    FlashFrostMenu->Add(new MenuBool(
        "Harass", "Use Q harass only on committed/guaranteed lines", true));
    FlashFrostMenu->Add(new MenuSlider(
        "HarassMana", "Minimum mana for Q harass (%)", 52, 0, 100));
    FlashFrostMenu->Add(new MenuBool(
        "Flee", "Use Q to stun a pursuer while fleeing", true));

    WallMenu = TacticsMenu->AddSubMenu(new Menu(
        "Crystallize", "W displacement, path forcing and ally safety"));
    WallMenu->Add(new MenuBool(
        "KeepInStorm", "Wall an exit only when it keeps the target in R", true));
    WallMenu->Add(new MenuBool(
        "ForceQPath", "Use W during Q flight to remove the safe dodge path", true));
    WallMenu->Add(new MenuBool(
        "CatchBeforeQ", "W first only on a committed target with a real block", true));
    WallMenu->Add(new MenuBool(
        "Peel", "Split a diver from the protected ally", true));
    WallMenu->Add(new MenuBool(
        "AntiGapcloser", "Place W across a committed dash exit", true));
    WallMenu->Add(new MenuBool(
        "Interrupt", "Displace channeling enemies with W", true));
    WallMenu->Add(new MenuBool(
        "Flee", "Place an ally-safe barrier while fleeing", true));
    WallMenu->Add(new MenuSeparator(
        "Safety", "Every W candidate rejects ally overlap, endangered ally paths and accidental turret aggro."));

    FrostbiteMenu = TacticsMenu->AddSubMenu(new Menu(
        "Frostbite", "E impact-time Chill and double-E sequencing"));
    FrostbiteMenu->Add(new MenuBool(
        "KillSecure", "Automatically use conservatively lethal Q or E", true));
    FrostbiteMenu->Add(new MenuBool(
        "OrdinaryPoke", "Allow unempowered AA-E poke at very high mana", false));
    FrostbiteMenu->Add(new MenuSlider(
        "OrdinaryPokeMana", "Mana for optional unempowered E poke (%)", 78, 50, 100));
    FrostbiteMenu->Add(new MenuSeparator(
        "ImpactRule", "E is empowered only when Chill exists at missile impact, including scheduled Q/R races."));

    StormMenu = TacticsMenu->AddSubMenu(new Menu(
        "GlacialStorm", "R growth, placement, mana and relocation"));
    StormMenu->Add(new MenuBool(
        "Combo", "Open combo with a scored, movement-led R", true));
    StormMenu->Add(new MenuSlider(
        "MinimumScore", "Minimum combo R placement score", 360, 100, 1200));
    StormMenu->Add(new MenuSlider(
        "MaximumLead", "Maximum movement lead for R center", 190, 0, 300));
    StormMenu->Add(new MenuSlider(
        "FlatReserve", "Mana kept after R drain", 90, 0, 350));
    StormMenu->Add(new MenuSlider(
        "NoContactGrace", "No-contact grace before ending R (ms)", 520, 150, 1400));
    StormMenu->Add(new MenuBool(
        "Relocate", "End an empty mature R for a substantially better center", true));
    StormMenu->Add(new MenuSlider(
        "RelocateGain", "Required score gain to relocate R", 180, 50, 600));
    StormMenu->Add(new MenuBool(
        "RespectManualStorm", "Preserve a player-cast R through its first setup window", true));
    StormMenu->Add(new MenuBool(
        "Peel", "Use R for critical ally peel", true));
    StormMenu->Add(new MenuSlider(
        "PeelAllyHp", "Protected ally HP for peel R (%)", 48, 10, 90));
    StormMenu->Add(new MenuBool(
        "AntiGapcloser", "Drop R on a committed close dash", true));
    StormMenu->Add(new MenuBool(
        "Harass", "Allow R harass at exceptional mana", false));
    StormMenu->Add(new MenuSlider(
        "HarassMana", "Minimum mana for R harass (%)", 82, 55, 100));
    StormMenu->Add(new MenuBool(
        "Flee", "Leave R between Anivia and a pursuer", true));

    PassiveMenu = TacticsMenu->AddSubMenu(new Menu(
        "Rebirth", "Passive awareness and player cooperation"));
    PassiveMenu->Add(new MenuSeparator(
        "Egg", "All casts stop during the six-second egg; movement and bait decisions remain yours."));
    PassiveMenu->Add(new MenuSeparator(
        "Priority", "Chronoshift and Guardian Angel resolve before Rebirth; this controller never forces a bait."));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Mana-aware lane, jungle and objective control"));
    FarmMenu->Add(new MenuBool(
        "UseQ", "Use Q on a multi-unit farm line", true));
    FarmMenu->Add(new MenuSlider(
        "QMinimumHits", "Minimum lane units on Q pass", 3, 1, 8));
    FarmMenu->Add(new MenuSlider(
        "QDetonateHits", "Minimum lane units for Q explosion", 2, 1, 8));
    FarmMenu->Add(new MenuBool(
        "UseE", "Use E for chilled jungle targets or predicted last hits", true));
    FarmMenu->Add(new MenuBool(
        "LaneR", "Use R for a large lane wave", false));
    FarmMenu->Add(new MenuSlider(
        "RMinimumLane", "Minimum lane units for R", 5, 3, 10));
    FarmMenu->Add(new MenuBool(
        "JungleR", "Use R in multi-unit camps and on epic monsters", true));
    FarmMenu->Add(new MenuSlider(
        "RMinimumJungle", "Minimum jungle units for R", 2, 1, 6));
    FarmMenu->Add(new MenuSlider(
        "LaneMana", "Minimum lane farm mana (%)", 55, 0, 100));
    FarmMenu->Add(new MenuSlider(
        "JungleMana", "Minimum jungle farm mana (%)", 30, 0, 100));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "One-trick state visualization"));
    CoachMenu->Add(new MenuBool(
        "DrawQ", "Draw Q path, live missile and explosion", true));
    CoachMenu->Add(new MenuBool(
        "DrawWall", "Draw the last W occupied segment", true));
    CoachMenu->Add(new MenuBool(
        "DrawStorm", "Draw R cast range, growth and full radius", true));
    CoachMenu->Add(new MenuBool(
        "DrawChill", "Draw confirmed and scheduled Chill", false));
    CoachMenu->Add(new MenuBool(
        "DrawPeel", "Draw protected ally and current diver", true));
    CoachMenu->Add(new MenuBool(
        "DrawState", "Draw posture, sequence, Q/E/R and passive clocks", true));
}

inline void OnLoad() {
    ActiveSequence = Sequence::None;
    CurrentPosture = Posture::Neutral;
    LastQPurpose = QPurpose::None;
    LastWallPurpose = WallPurpose::None;
    LastStormPurpose = StormPurpose::None;
    ClearQState();
    QCastTick = QDetonationTick = QAutoEndTick = 0;
    LastQPlan = {};
    WCastTick = WTargetId = 0;
    LastWallPlan = {};
    ECastTick = EImpactTick = ETargetId = 0;
    EWaitingForChill = false;
    EmpoweredECount = LastEmpoweredETick = ESequenceTargetId = 0;
    ClearStormState(false);
    RCastTick = 0;
    LastStormPlan = {};
    ChillMarks.fill({});
    RebirthReady = RebirthCooldown = EggActive = false;
    EggUntilTick = RebirthCooldownUntil = 0;
    ProtectedAllyId = PeelThreatId = 0;
    TargetedAllyThreatId = TargetedAllyThreatUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEnd = {};
    InterruptTargetId = InterruptExpireTick = 0;
    LastAutoTargetId = LastAutoTick = 0;
    IncomingThreatUntil = 0;
    RecentIncomingPressure = 0.0f;
    RefreshState();
}

inline void OnUnload() {
    TacticsMenu = FlashFrostMenu = WallMenu = FrostbiteMenu = nullptr;
    StormMenu = PassiveMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Route decisions through lane-control, catch, zone, peel, disengage, siege, objective or egg posture",
    "Preserve player ownership of movement, attack-move, Hold, Stop, Flash and positioning",
    "Prefer the player's selected target without inventing movement toward it",
    "Protect a pressured carry before starting a fresh damage sequence",
    "Continue a player-cast Q and R instead of replacing the player's setup",
    "Yield to the shared manual-input arbitration window",
    "Track Flash Frost through dedicated missile create and delete events",
    "Recover Flash Frost from the live missile collection after a missed lifecycle event",
    "Use the allied cryo Flash Frost particle only as a fallback",
    "Reconstruct Q origin, direction and maximum endpoint from a manual cast",
    "Extrapolate Q at 950 speed only until its real missile is observed",
    "Use current 1075 effective Q range and 0.25-second cast time",
    "Use current 220 full Q line width plus each target gameplay radius",
    "Use current 225 Q detonation radius plus each target gameplay radius",
    "Treat Q pass damage and explosion damage as separate instances",
    "Apply Chill from the pass even before Q is detonated",
    "Prefer hitting both Q instances whenever geometry permits",
    "Wait until the missile center passes the target before ordinary detonation",
    "Expose a configurable small overshoot instead of detonating at target center",
    "Forecast target and missile movement before deciding the explosion can still recover",
    "Detonate before the explosion window becomes unrecoverable",
    "Detonate near maximum travel rather than losing the stun to automatic expiry",
    "Detonate immediately when the explosion is conservatively lethal",
    "Detonate for a configured multi-target explosion",
    "Detonate on a committed gapcloser entering the explosion",
    "Detonate on a channel before its interrupt window closes",
    "Do not attempt Q recast while stunned, silenced, polymorphed or in egg",
    "Do not treat an invulnerable or untargetable unit as a Q detonation hit",
    "Do not detonate into a still-active ordinary spell shield",
    "Let Q pass consume an ordinary spell shield before using the explosion",
    "Remember which intended target owned the spell-shield-break sequence",
    "Avoid claiming the Black Shield interaction is identical to an ordinary spell shield",
    "Require very-high prediction for an unforced proactive Q",
    "Lower Q prediction only for a committed, wall-forced, storm-slowed or reactive line",
    "Treat dashes, hard CC, close commitment and movement toward Anivia as real Q commitment",
    "Hold Q at long range instead of repeatedly donating a slow projectile",
    "Hold Q against a Flash-ready target unless the line is forced",
    "Reject proactive Q opposite the player's cursor",
    "Ignore cursor agreement only for peel and interrupt emergencies",
    "Hold Q after the first empowered E while the target remains safely in R",
    "Spend held Q when the target approaches the R exit",
    "Spend held Q when it creates the next empowered E window",
    "Use Q during flee only on a real pursuer",
    "Cancel only an auto attack that would lose an immediate Q detonation or E-Chill race",
    "Preserve all ordinary auto weaves before, between and after spells",
    "Use current Q pass damage of 50 to 130 plus 25 percent AP",
    "Use current Q explosion damage of 60 to 200 plus 45 percent AP",
    "Use current Q cooldown data rather than the stale local OKTW values",
    "Model Crystallize as four through eight real wall segments by rank",
    "Use current outer segment distance of 400 through 800 by rank",
    "Include the two 100-radius end caps in W's occupied width",
    "Produce effective W occupied widths of 600 through 1000",
    "Orient W perpendicular to the caster-to-center direction",
    "Evaluate W collision as a capsule, not a decorative line",
    "Include champion gameplay radius in wall overlap",
    "Forecast the target 0.25 seconds to W creation",
    "Evaluate both possible wall-side outcomes through offset candidates",
    "Model the 120-unit champion displacement plus wall and target clearance",
    "Offset direct wall casts so an exact side tie is not the plan's foundation",
    "Use W on top of a channeling target to interrupt through displacement",
    "Use W to block the movement path around an in-flight Q",
    "Require a real blocked path, push or separation before spending W",
    "Use W to force a predictable Q dodge route",
    "Use W to hold a target in a growing or full R",
    "Reward W only when the displaced target is no farther from R",
    "Reject W that pushes a target out of R",
    "Use W to split a diver from the protected ally",
    "Use W across a committed gapcloser endpoint",
    "Use W between Anivia and a pursuer while fleeing",
    "Reject a W whose occupied capsule overlaps an allied champion",
    "Reject W across a pressured ally's route back to Anivia",
    "Reject W across an allied dash path",
    "Allow a peel wall that intentionally separates ally and diver",
    "Reject accidental W champion displacement under an enemy turret",
    "Remember that W's zero proc damage can draw turret aggression",
    "Never auto-cast a decorative wall with no tactical purpose",
    "Draw the real occupied W segment for player verification",
    "Use current 1000 W cast range and 17-second cooldown semantics",
    "Track Chill from live buff add, update and remove events",
    "Keep confirmed and geometry-scheduled Chill as distinct confidence states",
    "Expire Q Chill after the current three-second duration",
    "Refresh R Chill only while the full storm actually covers the unit",
    "Calculate E impact as 0.25 cast plus distance divided by 1600",
    "Check Chill at E missile impact rather than E cast time",
    "Reject an E whose Chill expires before impact with a safety margin",
    "Allow E shortly before Q detonation when Q Chill arrives before E",
    "Queue Q detonation after a player-visible E-before-Chill race",
    "Allow E before full R only when the 1.5-second full tick wins the impact race",
    "Do not empower E from a growing non-full R",
    "Use ordinary E only for conservative lethal damage by default",
    "Keep unempowered AA-E poke explicitly opt-in and high-mana",
    "Refuse E into spell shield, immunity, stasis or untargetability",
    "Use current E range including gameplay radius",
    "Use current E missile speed of 1600",
    "Use current E damage of 55 to 155 plus 55 percent AP",
    "Double E raw damage only for impact-time Chill",
    "Count the first empowered E in a target-scoped sequence",
    "Reset double-E state when the target changes or the window expires",
    "After E1, prefer W control and held Q over immediate redundant CC",
    "Cast E2 when cooldown returns and a new Chill window exists",
    "Support longer R-W-E-Q-E and R-E-Q-E rotations without hard-coded button spam",
    "Track Glacial Storm through allied cryo_storm object lifecycle",
    "Recover Glacial Storm from the live game-object collection",
    "Continue from cast center when the storm object callback is delayed",
    "Use current 750 R cast range",
    "Lead R ahead of movement rather than placing only at current position",
    "Cap R movement lead to avoid pathological prediction",
    "Evaluate current, cast-delay and 1.5-second target positions",
    "Evaluate pair midpoints for multi-target R placement",
    "Score R by target priority, control state, dash and allied follow-up",
    "Require the selected target inside an ordinary proactive combo R",
    "Use a separate immediate center for peel and disengage R",
    "Use current initial R radius of 200",
    "Grow R linearly to current full radius 400 over 1.5 seconds",
    "Include gameplay radius at both growing and full storm edges",
    "Track the immediate and half-second R tick cadence",
    "Use current rank mana drain of 35, 45 and 55 per second",
    "Reserve Q and E mana while sustaining R",
    "End R before its drain consumes the configured combat reserve",
    "Never end R before the one-second recast lock",
    "Keep R through a pending E whose empowerment depends on full storm",
    "Apply a no-contact grace instead of flickering R on one prediction miss",
    "End an empty R after its grace when no farm or objective purpose remains",
    "Preserve a player-cast R through its first setup window",
    "End a manual R later only for real no-contact or mana pressure",
    "Relocate R only after maturity and a substantial scored improvement",
    "Carry the original target across an R relocation sequence",
    "Respect R cooldown before placing the replacement storm",
    "Place replacement R slightly ahead of the target's route",
    "Use W to keep a target inside R before considering relocation",
    "Use Q as exit denial before abandoning a productive storm",
    "Draw both current growing radius and eventual full R radius",
    "Use current ordinary R tick damage of 15 to 30 plus 6.25 percent AP",
    "Multiply full R tick damage by exactly three",
    "Stop all spell input during Rebirth egg",
    "Track RebirthReady and RebirthCooldown separately",
    "Track the six-second egg window from buff duration with a fallback clock",
    "Remember the current 240-second Rebirth cooldown without pretending CDR changes it",
    "Do not force movement or passive bait when Rebirth is ready",
    "Acknowledge Chronoshift and Guardian Angel priority without overriding the player",
    "Use Q then E for a committed pre-six short trade",
    "Hold Q during extended pre-six AA-E pressure until a real line appears",
    "Use Q-W-E wall catch only when mana and geometry justify it",
    "Open post-six catch with led R when the placement score is sufficient",
    "Use R-W-E while saving Q as peel in the double-E branch",
    "Freestyle spell order from current control state instead of fixed Q-W-E-R order",
    "Peel with Q first when its travel and explosion can stop the diver",
    "Peel with W when it creates actual ally-threat separation",
    "Use R peel only for a critical ally, committed dash or point-blank threat",
    "Use E on a peeled threat only when full R or Q supplies impact-time Chill",
    "Continue manual Q detonation logic even outside an orbwalker combat mode",
    "Continue manual R mana and contact management outside combat mode",
    "Kill-secure with E only from conservative impact-time damage",
    "Kill-secure with Q only through a predicted real line",
    "Do not spend R merely because one low target exists",
    "Build lane Q lines through multiple predicted minions",
    "Detonate farm Q for multiple explosion hits or a predicted last hit",
    "Use E on lane minions only for predicted last hits",
    "Require an explicit high mana floor for lane spell farming",
    "Keep lane R opt-in and require a large wave",
    "End farm R when the remaining wave no longer occupies it",
    "Infer jungle farming from nearby neutral monsters during LaneClear",
    "Prefer chilled or epic jungle targets for E",
    "Use Q through multi-unit jungle lines",
    "Use R in multi-unit camps only above the configured count",
    "Allow R on a single epic monster as an objective exception",
    "Keep a separate lower jungle mana threshold",
    "Never invent movement between camps or toward a wave",
    "Expose posture, sequence, Q flight, W purpose, E count, R clock, mana and passive state",
    "Never fall back to generic spell ordering because this controller owns the full decision loop",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Anivia";
    controller.ControllerId = "champion.kuroaio.ai.anivia.onetrick";
    controller.KitRevision = "League 26.14 / CommunityDragon 16.14";
    controller.ResearchArtifact = "AI/Research/AIAnivia.md";
    controller.ImplementationSummary =
        "Nine-posture control mage with real Q missile/pass/explosion and "
        "spell-shield sequencing, ally-safe ranked wall geometry, impact-time "
        "E Chill races and double-E state, growing/contact/mana-aware R "
        "placement and relocation, plus Rebirth suspension.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell =
        &ControllerHelpers::DispatchLocalOrOtherSpellEvent<
            &ObserveLocalSpell, &ObserveEnemyCast>;
    controller.OnDoCast = &OnDoCast;
    controller.OnBuffAdd =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnBuffRemove =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;
    controller.OnBuffUpdate =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack =
        &ControllerHelpers::CaptureAfterAttackEvent<
            &LastAutoTargetId, &LastAutoTick>;
    controller.OnGapcloser =
        &ControllerHelpers::CaptureGapcloserEvent<
            &GapcloserTargetId, &GapcloserEnd,
            &GapcloserExpireTick, 560, 760>;
    controller.OnInterruptable =
        &ControllerHelpers::CaptureInterruptableEvent<
            &InterruptTargetId, &InterruptExpireTick, 1100, 250, 5000>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Anivia
