#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIAnnieGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Annie {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::EnemyFlashReady;
using ControllerHelpers::HasCurrentResource;
using ControllerHelpers::HasResourceFor;
using ControllerHelpers::HasNearbyJungleTarget;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::NameEquals;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::ObjectEventIsAllied;
using ControllerHelpers::PredictPosition;
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
    HoldThree,
    HiddenQPrime,
    TwoStackRace,
    QStunCatch,
    QShieldBreak,
    WStunCone,
    RStunEngage,
    ManualWFlash,
    ManualRFlash,
    BurstFollowup,
    PeelChain,
    ShieldReaction,
    ShieldEngage,
    FarmRefund,
    JungleCycle,
    TibbersEnrage,
    TibbersZone,
    TibbersRecall,
};

enum class Posture : int {
    Neutral,
    LaneControl,
    ShortTrade,
    Catch,
    Ambush,
    Teamfight,
    Peel,
    Disengage,
    Siege,
    Objective,
    TibbersControl,
};

enum class StunIntent : int {
    None,
    PointCatch,
    HiddenPointCatch,
    AoeCone,
    AoeSummon,
    AntiGapcloser,
    Interrupt,
    Peel,
    Flee,
    ShieldBreak,
};

enum class ShieldReason : int {
    None,
    IncomingBurst,
    IncomingAttack,
    IncomingHardCc,
    AllyEngage,
    SelfTrade,
    FleeSpeed,
    PrimeHiddenStun,
    StackBuilding,
    TibbersSupport,
};

enum class PetPurpose : int {
    None,
    EnrageFocus,
    FollowTarget,
    Peel,
    Zone,
    Vision,
    Objective,
    ReturnFromTurret,
    RecoverLeash,
    Manual,
};

struct QPlan {
    AIBaseClient Target = {};
    int TargetId = 0;
    int ImpactTick = 0;
    float Damage = 0.0f;
    StunIntent Intent = StunIntent::None;
    bool WillStun = false;
    bool SpellShieldBreak = false;
    bool ProjectileBlocked = false;
    bool Lethal = false;
    bool FarmRefund = false;
    bool Valid = false;
};

struct ConePlan {
    Vector3 Aim = {};
    std::array<int, 10> HitIds = {};
    int HitCount = 0;
    int PriorityHits = 0;
    int PrimaryId = 0;
    float Score = -FLT_MAX;
    StunIntent Intent = StunIntent::None;
    bool IncludesPrimary = false;
    bool WillStun = false;
    bool Valid = false;
};

struct SummonPlan {
    Vector3 Center = {};
    std::array<int, 10> HitIds = {};
    int HitCount = 0;
    int PriorityHits = 0;
    int PrimaryId = 0;
    float Score = -FLT_MAX;
    StunIntent Intent = StunIntent::None;
    bool IncludesPrimary = false;
    bool IncludesProtectedThreat = false;
    bool WillStun = false;
    bool Valid = false;
};

struct IncomingThreat {
    int SourceId = 0;
    int TargetId = 0;
    int ImpactTick = 0;
    int ExpireTick = 0;
    float EstimatedDamage = 0.0f;
    bool HardCrowdControl = false;
    bool AutoAttack = false;
    bool LineThreat = false;
};

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* DisintegrateMenu = nullptr;
inline Menu* IncinerateMenu = nullptr;
inline Menu* ShieldMenu = nullptr;
inline Menu* TibbersMenu = nullptr;
inline Menu* PetMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Sequence ActiveSequence = Sequence::None;
inline Posture CurrentPosture = Posture::Neutral;
inline StunIntent ReservedStunIntent = StunIntent::None;
inline ShieldReason LastShieldReason = ShieldReason::None;
inline PetPurpose LastPetPurpose = PetPurpose::None;

inline int PassiveStacks = 4;
inline bool PassivePrimed = true;
inline bool PassiveBuffConfirmed = false;
inline int PassiveLastObservedTick = 0;
inline int PassiveLastConsumeTick = 0;
inline int PassiveLastGainTick = 0;

inline bool QPending = false;
inline bool QMissileObserved = false;
inline int QCastTick = 0;
inline int QImpactTick = 0;
inline int QTargetId = 0;
inline int QMissileNetworkId = 0;
inline int QOriginalStacks = 0;
inline bool QWasManual = false;
inline bool QExpectedStun = false;
inline bool QExpectedShieldBreak = false;
inline Vector3 QOrigin = {};
inline Vector3 QLastPosition = {};
inline QPlan LastQPlan = {};

inline bool WPending = false;
inline int WCastTick = 0;
inline int WResolveTick = 0;
inline int WPrimaryId = 0;
inline int WCastStartingStacks = 0;
inline bool WWasManual = false;
inline bool WExpectedStun = false;
inline Vector3 WCastOrigin = {};
inline Vector3 WAim = {};
inline ConePlan LastConePlan = {};

inline bool RPending = false;
inline int RCastTick = 0;
inline int RResolveTick = 0;
inline int RPrimaryId = 0;
inline int RCastStartingStacks = 0;
inline bool RWasManual = false;
inline bool RExpectedStun = false;
inline SummonPlan LastSummonPlan = {};

inline bool TibbersActive = false;
inline int TibbersNetworkId = 0;
inline int TibbersSpawnTick = 0;
inline int TibbersExpireTick = 0;
inline int TibbersEnrageUntil = 0;
inline int TibbersEnrageAttacks = 0;
inline int TibbersLastOrderTick = 0;
inline int TibbersLastTargetId = 0;
inline int ManualPetLockUntil = 0;
inline int ControllerPetOrderUntil = 0;
inline Vector3 TibbersLastPosition = {};

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
inline int LastAfterAttackTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int ManualFlashUntil = 0;
inline int ManualFlashTick = 0;
inline std::array<IncomingThreat, 28> IncomingThreats = {};

inline constexpr int kStunMinimumMs = 1250;
inline constexpr int kPassiveReconcileMs = 420;
inline constexpr int kPetManualLockMs = 1350;
inline constexpr int kPetOrderThrottleMs = 575;

inline bool IsSpellShielded(const AIBaseClient& target) {
    return target.IsValid() &&
        (SDK::HasBuffOfType(target, SDK::BuffType::SpellShield) ||
         target.HasBuff("SivirE") ||
         target.HasBuff("NocturneShroudofDarkness") ||
         target.HasBuff("BansheesVeil") ||
         target.HasBuff("EdgeOfNight"));
}

inline AIHeroClient AllyByNetworkId(int networkId) {
    if (networkId == 0) return {};
    const auto hero = GameObjects::GetUnitByNetworkId<AIHeroClient>(networkId);
    return Engine::ValidAlly(hero) ? hero : AIHeroClient{};
}

inline AIMinionClient Tibbers() {
    return TibbersNetworkId != 0
        ? GameObjects::GetUnitByNetworkId<AIMinionClient>(TibbersNetworkId)
        : AIMinionClient{};
}

inline bool IsTibbersName(const char* first, const char* second = nullptr) {
    return ControllerHelpers::AnyTextContains(
        { first, second }, { "annietibbers", "AnnieTibbers" });
}

inline float TargetPriority(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const float offense = target.TotalAttackDamage() * 0.0045f +
                          target.AP() * 0.0043f;
    const float range = std::clamp(target.AttackRange() / 700.0f,
                                   0.0f, 1.4f);
    const float missing = (100.0f - target.HealthPercent()) * 0.010f;
    return 1.0f + offense + range + missing;
}

inline float QDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    return target.IsValid() && player.IsValid()
        ? player.CalculateMagicDamage(
              target, DisintegrateRawDamage(
                          SpellRank(0), player.AP()))
        : 0.0f;
}

inline float WDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    return target.IsValid() && player.IsValid()
        ? player.CalculateMagicDamage(
              target, IncinerateRawDamage(
                          SpellRank(1), player.AP()))
        : 0.0f;
}

inline float RDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    return target.IsValid() && player.IsValid()
        ? player.CalculateMagicDamage(
              target, SummonTibbersRawDamage(
                          SpellRank(3), player.AP()))
        : 0.0f;
}

inline float TibbersContactDamage(const AIBaseClient& target,
                                  float seconds = 1.0f,
                                  int attacks = 1) {
    const auto player = GameObjects::Player();
    if (!target.IsValid() || !player.IsValid()) return 0.0f;
    const float rawAura = TibbersAuraRawDamagePerTick(
        SpellRank(3), player.AP()) *
        static_cast<float>(TibbersAuraTickCount(seconds));
    const float rawAttacks = TibbersAttackRawDamage(
        SpellRank(3), player.AP()) *
        static_cast<float>(std::max(0, attacks));
    return player.CalculateMagicDamage(target, rawAura + rawAttacks);
}

inline float ConservativeComboDamage(const AIHeroClient& target,
                                     bool includeUltimate = true) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const auto player = GameObjects::Player();
    float damage = SDK::Damage::GetAutoAttackDamage(player, target, true);
    if (Ready(0)) damage += QDamage(target);
    if (Ready(1)) damage += WDamage(target);
    if (includeUltimate && Ready(3) && !TibbersActive) {
        damage += RDamage(target) + TibbersContactDamage(target, 0.75f, 1);
    } else if (TibbersActive) {
        damage += TibbersContactDamage(target, 0.75f, 1);
    }
    return damage;
}

inline void SetPassiveStacks(int stacks, bool confirmed = false) {
    PassiveStacks = std::clamp(stacks, 0, 4);
    PassivePrimed = PassiveStacks >= 4;
    PassiveBuffConfirmed = confirmed && PassivePrimed;
    PassiveLastObservedTick = Now();
}

inline void GainPassiveStack() {
    if (PassiveStacks < 4) {
        ++PassiveStacks;
        PassiveLastGainTick = Now();
    }
    PassivePrimed = PassiveStacks >= 4;
}

inline void ConsumePassive(PassiveSpell source,
                           int targetId = 0,
                           bool stunApplied = true) {
    if (PassiveStacks < 4 && !PassivePrimed) return;
    PassiveStacks = 0;
    PassivePrimed = false;
    PassiveBuffConfirmed = false;
    PassiveLastConsumeTick = Now();
    if (stunApplied && targetId != 0) {
        const AIHeroClient hero = ControllerHelpers::HeroByNetworkId(targetId);
        if (Engine::ValidEnemy(hero)) {
            TibbersEnrageUntil = std::max(
                TibbersEnrageUntil, Now() +
                    static_cast<int>(kTibbersEnrageSeconds * 1000.0f));
            TibbersEnrageAttacks = 0;
            ActiveSequence = TibbersActive
                ? Sequence::TibbersEnrage : ActiveSequence;
        }
    }
    (void)source;
}

inline bool AoeStunReserved() {
    return ReservedStunIntent == StunIntent::AoeCone ||
           ReservedStunIntent == StunIntent::AoeSummon ||
           ReservedStunIntent == StunIntent::Peel ||
           ReservedStunIntent == StunIntent::Flee;
}

inline int EarliestPendingConsumerTick(PassiveSpell ignored = PassiveSpell::None) {
    int result = 0;
    auto take = [&](PassiveSpell spell, bool pending, int tick) {
        if (!pending || spell == ignored || tick <= 0) return;
        if (result == 0 || tick < result) result = tick;
    };
    take(PassiveSpell::Q, QPending, QImpactTick);
    take(PassiveSpell::W, WPending, WResolveTick);
    take(PassiveSpell::R, RPending, RResolveTick);
    return result;
}

inline bool CandidateOwnsStun(PassiveSpell candidate, int resolveTick) {
    if (!PassivePrimed) return false;
    const int earlier = EarliestPendingConsumerTick(candidate);
    return earlier == 0 || resolveTick <= earlier + 20;
}

inline bool ProjectileWallBlocksQ(const AIBaseClient& target) {
    if (!target.IsValid()) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return true;
    const float distance = player.Position().Distance2D(target.Position());
    const Vector3 predicted = PredictPosition(
        target, DisintegrateImpactSeconds(distance));
    const Vector3 endpoint = predicted.IsValid() && !predicted.IsZero()
        ? predicted : target.Position();
    return ControllerHelpers::ProjectileWallBlocksFromPlayer(endpoint, 32.0f);
}

inline QPlan BuildQPlan(const AIBaseClient& target,
                        StunIntent intent,
                        bool farmRefund = false,
                        bool reactive = false) {
    QPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid() || target.IsDead() ||
        !target.IsEnemy() || !target.IsTargetable() || !Ready(0) ||
        QPending || IsCommonUntargetableOrImmune(target)) {
        return plan;
    }
    const float distance = player.Position().Distance2D(target.Position());
    if (distance > kDisintegrateRange + target.BoundingRadius()) return plan;

    const bool shield = IsSpellShielded(target);
    const bool blocked = ProjectileWallBlocksQ(target);
    const int impact = Now() + static_cast<int>(std::ceil(
        DisintegrateImpactSeconds(distance) * 1000.0f));
    const bool ownsStun = PassivePrimed &&
                          CandidateOwnsStun(PassiveSpell::Q, impact);
    const bool hiddenPrime = PassiveStacks == 3 && Ready(2) &&
                             HasResourceFor({ 0, 2 }) &&
                             !shield && !farmRefund;
    const float damage = QDamage(target);
    const bool lethal = !shield && !blocked &&
        damage >= target.Health() + target.AllShield();

    if (blocked && !reactive) return plan;
    if (shield && ownsStun &&
        intent != StunIntent::ShieldBreak && !lethal) {
        return plan;
    }

    plan.Target = target;
    plan.TargetId = static_cast<int>(target.NetworkId());
    plan.ImpactTick = impact;
    plan.Damage = damage;
    plan.Intent = shield ? StunIntent::ShieldBreak :
        (hiddenPrime ? StunIntent::HiddenPointCatch : intent);
    plan.WillStun = ownsStun || hiddenPrime;
    plan.SpellShieldBreak = shield;
    plan.ProjectileBlocked = blocked;
    plan.Lethal = lethal;
    plan.FarmRefund = farmRefund;
    plan.Valid = !blocked;
    return plan;
}

inline bool CastDisintegrate(const QPlan& plan,
                             Mode mode,
                             bool reactive = false) {
    if (!plan.Valid || !plan.Target.IsValid() || QPending || !Ready(0) ||
        !SpellEnabled(0, mode) ||
        !CastThrottleReady(0, 34, reactive ? 0 : -1) ||
        !HasCurrentResource(SpellCost(0))) {
        return false;
    }
    if (!reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true) &&
        !plan.Lethal) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (!Engine::ControllerCastUnit(0, plan.Target)) return false;

    QPending = true;
    QMissileObserved = false;
    QCastTick = Now();
    QImpactTick = plan.ImpactTick;
    QTargetId = plan.TargetId;
    QMissileNetworkId = 0;
    QOriginalStacks = PassiveStacks;
    QWasManual = false;
    QExpectedStun = plan.WillStun;
    QExpectedShieldBreak = plan.SpellShieldBreak;
    QOrigin = player.Position();
    QLastPosition = QOrigin;
    LastQPlan = plan;
    ReservedStunIntent = plan.Intent;
    if (plan.FarmRefund) {
        ActiveSequence = Sequence::FarmRefund;
    } else if (plan.SpellShieldBreak) {
        ActiveSequence = Sequence::QShieldBreak;
    } else if (plan.Intent == StunIntent::HiddenPointCatch) {
        ActiveSequence = Sequence::HiddenQPrime;
    } else if (plan.WillStun) {
        ActiveSequence = Sequence::QStunCatch;
    } else if (QOriginalStacks == 2 && !plan.FarmRefund &&
               !plan.SpellShieldBreak && Ready(1) && Ready(2)) {
        ActiveSequence = Sequence::TwoStackRace;
    }
    return true;
}

inline std::vector<ConeUnit> ConeUnits(float delaySeconds,
                                      int primaryId) {
    std::vector<ConeUnit> units;
    units.reserve(GameObjects::EnemyHeroes().size());
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 950.0f) ||
            IsCommonUntargetableOrImmune(enemy)) {
            continue;
        }
        units.push_back({
            PredictPosition(enemy, delaySeconds),
            enemy.BoundingRadius(),
            TargetPriority(enemy),
            static_cast<int>(enemy.NetworkId()) == primaryId,
            Engine::IsHardCrowdControlled(enemy),
            enemy.IsDashing(),
            true,
        });
    }
    return units;
}

inline ConePlan BuildConePlan(const AIHeroClient& primary,
                              StunIntent intent,
                              bool reactive = false) {
    ConePlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1) || WPending) return best;

    const int primaryId = Engine::ValidEnemy(primary)
        ? static_cast<int>(primary.NetworkId()) : 0;
    std::vector<Vector3> candidates;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 760.0f) ||
            IsCommonUntargetableOrImmune(enemy)) {
            continue;
        }
        const Vector3 predicted = PredictPosition(enemy, 0.25f);
        candidates.push_back(predicted);
    }
    const std::size_t individualCount = candidates.size();
    for (std::size_t left = 0; left < individualCount; ++left) {
        for (std::size_t right = left + 1; right < individualCount; ++right) {
            candidates.push_back((candidates[left] + candidates[right]) * 0.5f);
            if (candidates.size() >= 24) break;
        }
        if (candidates.size() >= 24) break;
    }
    if (reactive && GapcloserEnd.IsValid() && !GapcloserEnd.IsZero()) {
        candidates.push_back(GapcloserEnd);
    }

    const int resolveTick = Now() + 250;
    const int stacksAfterCast = std::min(4, PassiveStacks + 1);
    const int earlier = EarliestPendingConsumerTick(PassiveSpell::W);
    const bool willStun = stacksAfterCast >= 4 &&
                          (earlier == 0 || resolveTick <= earlier + 20);
    const auto units = ConeUnits(0.25f, primaryId);

    for (const Vector3& candidate : candidates) {
        if (!candidate.IsValid() || candidate.IsZero()) continue;
        const Vector3 direction = SharedGeometry::Direction2D(
            player.Position(), candidate);
        if (direction.IsZero()) continue;
        const Vector3 aim = player.Position() + direction * kIncinerateRange;

        ConePlan plan{};
        plan.Aim = aim;
        plan.PrimaryId = primaryId;
        plan.Intent = intent;
        plan.WillStun = willStun;
        int write = 0;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!Engine::ValidEnemy(enemy, 850.0f) ||
                IsCommonUntargetableOrImmune(enemy)) {
                continue;
            }
            const Vector3 predicted = PredictPosition(enemy, 0.25f);
            if (!IncinerateHits(player.Position(), aim, predicted,
                                enemy.BoundingRadius())) {
                continue;
            }
            ++plan.HitCount;
            const int id = static_cast<int>(enemy.NetworkId());
            if (write < static_cast<int>(plan.HitIds.size())) {
                plan.HitIds[static_cast<std::size_t>(write++)] = id;
            }
            if (id == primaryId) plan.IncludesPrimary = true;
            if (TargetPriority(enemy) >= 2.5f || enemy.IsDashing()) {
                ++plan.PriorityHits;
            }
        }
        plan.Score = IncinerateScore(player.Position(), aim, units) * 100.0f;
        plan.Score += willStun ? plan.HitCount * 80.0f : 0.0f;
        plan.Score += plan.IncludesPrimary ? 110.0f : 0.0f;
        if (intent == StunIntent::AntiGapcloser ||
            intent == StunIntent::Interrupt ||
            intent == StunIntent::Peel || intent == StunIntent::Flee) {
            plan.Score += plan.IncludesPrimary ? 180.0f : 0.0f;
        }
        plan.Valid = plan.HitCount > 0 &&
            (primaryId == 0 || plan.IncludesPrimary || reactive);
        if (plan.Valid && (!best.Valid || plan.Score > best.Score)) best = plan;
    }
    return best;
}

inline bool CastIncinerate(const ConePlan& plan,
                           Mode mode,
                           bool reactive = false) {
    if (!plan.Valid || !Ready(1) || WPending ||
        !SpellEnabled(1, mode) ||
        !CastThrottleReady(1, 34, reactive ? 0 : -1) ||
        !HasCurrentResource(SpellCost(1))) {
        return false;
    }
    if (!reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true)) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (!Engine::ControllerCastPosition(1, plan.Aim)) return false;

    WPending = true;
    WCastTick = Now();
    WResolveTick = WCastTick + 250;
    WPrimaryId = plan.PrimaryId;
    WCastStartingStacks = PassiveStacks;
    WWasManual = false;
    WExpectedStun = plan.WillStun;
    WCastOrigin = player.Position();
    WAim = plan.Aim;
    LastConePlan = plan;
    GainPassiveStack();
    ReservedStunIntent = plan.Intent;
    if (plan.WillStun) ActiveSequence = Sequence::WStunCone;
    return true;
}

inline std::vector<CircleUnit> SummonUnits(float delaySeconds,
                                          int primaryId) {
    std::vector<CircleUnit> units;
    units.reserve(GameObjects::EnemyHeroes().size());
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1050.0f) ||
            IsCommonUntargetableOrImmune(enemy)) {
            continue;
        }
        units.push_back({
            PredictPosition(enemy, delaySeconds),
            enemy.BoundingRadius(),
            TargetPriority(enemy),
            static_cast<int>(enemy.NetworkId()) == primaryId,
            Engine::IsHardCrowdControlled(enemy),
            enemy.IsDashing(),
            IsSpellShielded(enemy),
            true,
        });
    }
    return units;
}

inline SummonPlan BuildSummonPlan(const AIHeroClient& primary,
                                  StunIntent intent,
                                  bool reactive = false) {
    SummonPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3) || TibbersActive || RPending) {
        return best;
    }
    const int primaryId = Engine::ValidEnemy(primary)
        ? static_cast<int>(primary.NetworkId()) : 0;
    const int resolveTick = Now() + 250;
    const int stacksAfterCast = std::min(4, PassiveStacks + 1);
    const int earlier = EarliestPendingConsumerTick(PassiveSpell::R);
    const bool willStun = stacksAfterCast >= 4 &&
                          (earlier == 0 || resolveTick <= earlier + 20);
    std::vector<Vector3> candidates;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kTibbersCastRange + 360.0f) ||
            IsCommonUntargetableOrImmune(enemy)) {
            continue;
        }
        candidates.push_back(PredictPosition(enemy, 0.25f));
        candidates.push_back(PredictPosition(enemy, 0.40f));
    }
    const std::size_t individualCount = candidates.size();
    for (std::size_t left = 0; left < individualCount; ++left) {
        for (std::size_t right = left + 1; right < individualCount; ++right) {
            candidates.push_back((candidates[left] + candidates[right]) * 0.5f);
            if (candidates.size() >= 32) break;
        }
        if (candidates.size() >= 32) break;
    }
    if (reactive && GapcloserEnd.IsValid() && !GapcloserEnd.IsZero()) {
        candidates.push_back(GapcloserEnd);
    }

    for (Vector3 center : candidates) {
        if (!center.IsValid() || center.IsZero()) continue;
        const float distance = player.Position().Distance2D(center);
        if (distance > kTibbersCastRange) {
            const Vector3 direction = SharedGeometry::Direction2D(
                player.Position(), center);
            center = player.Position() + direction * kTibbersCastRange;
        }
        const auto units = SummonUnits(0.25f, primaryId);
        SummonPlan plan{};
        plan.Center = center;
        plan.PrimaryId = primaryId;
        plan.Intent = intent;
        plan.WillStun = willStun;
        plan.Score = TibbersSummonScore(center, units, willStun) * 100.0f;
        int write = 0;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!Engine::ValidEnemy(enemy, 1050.0f) ||
                IsCommonUntargetableOrImmune(enemy)) {
                continue;
            }
            const Vector3 predicted = PredictPosition(enemy, 0.25f);
            if (!TibbersSummonHits(center, predicted,
                                   enemy.BoundingRadius())) {
                continue;
            }
            ++plan.HitCount;
            const int id = static_cast<int>(enemy.NetworkId());
            if (write < static_cast<int>(plan.HitIds.size())) {
                plan.HitIds[static_cast<std::size_t>(write++)] = id;
            }
            if (id == primaryId) plan.IncludesPrimary = true;
            if (id == PeelThreatId) plan.IncludesProtectedThreat = true;
            if (TargetPriority(enemy) >= 2.5f || enemy.IsDashing()) {
                ++plan.PriorityHits;
            }
        }
        plan.Score += willStun ? plan.HitCount * 95.0f : 0.0f;
        plan.Score += plan.IncludesPrimary ? 130.0f : 0.0f;
        plan.Score += plan.IncludesProtectedThreat ? 190.0f : 0.0f;
        if (primaryId != 0) {
            const AIHeroClient target = ControllerHelpers::HeroByNetworkId(primaryId);
            if (Engine::ValidEnemy(target) &&
                RDamage(target) >= target.Health() + target.AllShield()) {
                plan.Score += 280.0f;
            }
        }
        plan.Valid = plan.HitCount > 0 &&
            (primaryId == 0 || plan.IncludesPrimary || reactive);
        if (plan.Valid && (!best.Valid || plan.Score > best.Score)) best = plan;
    }
    return best;
}

inline bool CastSummonTibbers(const SummonPlan& plan,
                              Mode mode,
                              bool reactive = false) {
    if (!plan.Valid || TibbersActive || RPending || !Ready(3) ||
        !SpellEnabled(3, mode) ||
        !CastThrottleReady(3, 34, reactive ? 0 : -1) ||
        !HasCurrentResource(SpellCost(3))) {
        return false;
    }
    if (!reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true)) {
        return false;
    }
    if (!Engine::ControllerCastPosition(3, plan.Center)) return false;

    RPending = true;
    RCastTick = Now();
    RResolveTick = RCastTick + 250;
    RPrimaryId = plan.PrimaryId;
    RCastStartingStacks = PassiveStacks;
    RWasManual = false;
    RExpectedStun = plan.WillStun;
    LastSummonPlan = plan;
    GainPassiveStack();
    ReservedStunIntent = plan.Intent;
    ActiveSequence = plan.WillStun
        ? Sequence::RStunEngage : Sequence::BurstFollowup;
    return true;
}

inline bool CastMoltenShield(const AIBaseClient& ally,
                             Mode mode,
                             ShieldReason reason,
                             bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!ally.IsValid() || ally.IsDead() || !ally.IsAlly() || !Ready(2) ||
        !SpellEnabled(2, mode) ||
        !CastThrottleReady(2, 34, reactive ? 0 : -1) ||
        !HasCurrentResource(SpellCost(2)) ||
        player.Position().Distance2D(ally.Position()) >
            kMoltenShieldRange + ally.BoundingRadius()) {
        return false;
    }
    if (!Engine::ControllerCastUnit(2, ally)) return false;
    GainPassiveStack();
    LastShieldReason = reason;
    if (reason == ShieldReason::PrimeHiddenStun) {
        ActiveSequence = Sequence::HiddenQPrime;
        QExpectedStun = true;
    } else if (reason == ShieldReason::IncomingBurst ||
               reason == ShieldReason::IncomingAttack ||
               reason == ShieldReason::IncomingHardCc) {
        ActiveSequence = Sequence::ShieldReaction;
    } else if (reason == ShieldReason::AllyEngage) {
        ActiveSequence = Sequence::ShieldEngage;
    }
    return true;
}

inline bool TryPrimePendingQ(Mode mode) {
    if (!QPending || QExpectedShieldBreak || PassiveStacks != 3 ||
        !Ready(2) || QImpactTick - Now() < 45 ||
        !Bool(PassiveMenu, "HiddenQPrime", true)) {
        return false;
    }
    const AIHeroClient target = ControllerHelpers::HeroByNetworkId(QTargetId);
    if (!Engine::ValidEnemy(target) || IsSpellShielded(target)) return false;

    AIBaseClient shieldTarget = GameObjects::Player();
    const AIHeroClient protectedAlly = AllyByNetworkId(ProtectedAllyId);
    if (Engine::ValidAlly(protectedAlly) &&
        protectedAlly.Position().Distance2D(
            GameObjects::Player().Position()) <= kMoltenShieldRange &&
        TargetedAllyThreatUntil >= Now()) {
        shieldTarget = protectedAlly;
    }
    return CastMoltenShield(
        shieldTarget, mode == Mode::None ? Mode::Automatic : mode,
        ShieldReason::PrimeHiddenStun, true);
}

inline void StoreIncomingThreat(const IncomingThreat& incoming) {
    if (incoming.SourceId == 0 || incoming.TargetId == 0) return;
    IncomingThreat* empty = nullptr;
    for (auto& threat : IncomingThreats) {
        if (threat.SourceId == incoming.SourceId &&
            threat.TargetId == incoming.TargetId &&
            std::abs(threat.ImpactTick - incoming.ImpactTick) <= 140) {
            threat = incoming;
            return;
        }
        if (!empty && (threat.SourceId == 0 || threat.ExpireTick < Now())) {
            empty = &threat;
        }
    }
    if (!empty) empty = &IncomingThreats.front();
    *empty = incoming;
}

inline void RecordIncomingThreats(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !args.Sender.IsValid() ||
        IsLocalPlayer(args.Sender)) {
        return;
    }
    const AIHeroClient enemy = Engine::EnemyByNetworkId(
        static_cast<int>(args.Sender.NetworkId));
    if (!Engine::ValidEnemy(enemy)) return;

    const std::uint32_t directTarget = args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId;
    const int castMs = ControllerHelpers::NormalizedCastDelayMs(
        args.CastDelay, args.IsAutoAttack ? 180 : 250);
    const float speed = args.MissileSpeed > 1.0f
        ? args.MissileSpeed : 0.0f;
    const Vector3 start = args.StartPosition.IsValid() &&
            !args.StartPosition.IsZero()
        ? args.StartPosition : enemy.Position();
    Vector3 end = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
        ? args.EndPosition : args.CastPosition;
    const bool hasLine = end.IsValid() && !end.IsZero() &&
                         start.Distance2D(end) >= 220.0f;
    const bool hardCc = ControllerHelpers::LikelyHardCrowdControlSpell(args);

    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally) ||
            ally.Position().Distance2D(player.Position()) > 1050.0f) {
            continue;
        }
        const bool targeted = directTarget ==
            static_cast<std::uint32_t>(ally.NetworkId());
        bool lineThreat = false;
        if (!targeted && hasLine) {
            const auto projection = SharedGeometry::ProjectPointToSegment2D(
                ally.Position(), start, end);
            lineThreat = projection.Distance <=
                ally.BoundingRadius() + (hardCc ? 115.0f : 78.0f);
        }
        if (!targeted && !lineThreat) continue;

        float travelMs = 0.0f;
        if (speed > 1.0f) {
            travelMs = start.Distance2D(ally.Position()) / speed * 1000.0f;
        }
        const int impact = Now() + std::clamp(
            castMs + static_cast<int>(travelMs), 35, 2200);
        const float damage = args.IsAutoAttack
            ? SDK::Damage::GetAutoAttackDamage(enemy, ally, true)
            : ally.MaxHealth() * (hardCc ? 0.16f : 0.11f);
        StoreIncomingThreat({
            static_cast<int>(enemy.NetworkId()),
            static_cast<int>(ally.NetworkId()),
            impact,
            impact + 420,
            damage,
            hardCc,
            args.IsAutoAttack,
            lineThreat,
        });
        if (ally.NetworkId() != player.NetworkId()) {
            TargetedAllyThreatId = static_cast<int>(enemy.NetworkId());
            TargetedAllyThreatUntil = impact + 450;
            ProtectedAllyId = static_cast<int>(ally.NetworkId());
        }
    }
}

inline AIHeroClient ProtectedAlly() {
    AIHeroClient current = AllyByNetworkId(ProtectedAllyId);
    const AIHeroClient ranked = SelectProtectionAlly(
        1050.0f, ProtectedAllyId, TargetedAllyThreatUntil);
    if (Engine::ValidAlly(ranked)) current = ranked;
    if (Engine::ValidAlly(current)) {
        ProtectedAllyId = static_cast<int>(current.NetworkId());
    }
    return current;
}

inline AIHeroClient SelectPeelThreat(const AIHeroClient& ally) {
    if (!Engine::ValidAlly(ally)) return {};
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            enemy.Position().Distance2D(ally.Position()) > 760.0f) {
            continue;
        }
        const Vector3 pathEnd = enemy.PathEnd();
        const bool approaching = pathEnd.IsValid() && !pathEnd.IsZero() &&
            pathEnd.Distance2D(ally.Position()) + 65.0f <
                enemy.Position().Distance2D(ally.Position());
        float score = TargetPriority(enemy) * 120.0f;
        score += enemy.IsDashing() ? 280.0f : 0.0f;
        score += approaching ? 150.0f : 0.0f;
        score += Engine::IsHardCrowdControlled(ally) ? 180.0f : 0.0f;
        score += static_cast<int>(enemy.NetworkId()) ==
                TargetedAllyThreatId ? 220.0f : 0.0f;
        score -= enemy.Position().Distance2D(ally.Position()) * 0.22f;
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    if (Engine::ValidEnemy(best)) {
        PeelThreatId = static_cast<int>(best.NetworkId());
    }
    return best;
}

inline bool TryReactiveShield(Mode mode) {
    if (!Ready(2) || !Bool(ShieldMenu, "Reactive", true)) return false;
    IncomingThreat* best = nullptr;
    float bestScore = -FLT_MAX;
    for (auto& threat : IncomingThreats) {
        if (threat.SourceId == 0 || threat.ExpireTick < Now() ||
            threat.ImpactTick < Now() - 80 ||
            threat.ImpactTick > Now() + 620) {
            continue;
        }
        const AIHeroClient ally = AllyByNetworkId(threat.TargetId);
        if (!Engine::ValidAlly(ally) ||
            GameObjects::Player().Position().Distance2D(ally.Position()) >
                kMoltenShieldRange + ally.BoundingRadius()) {
            continue;
        }
        const bool lethalish = threat.EstimatedDamage >=
            ally.Health() + ally.AllShield() * 0.60f;
        float score = threat.EstimatedDamage;
        score += threat.HardCrowdControl ? ally.MaxHealth() * 0.20f : 0.0f;
        score += lethalish ? ally.MaxHealth() * 0.60f : 0.0f;
        score += threat.TargetId == ProtectedAllyId
            ? ally.MaxHealth() * 0.10f : 0.0f;
        score -= static_cast<float>(std::max(0,
            threat.ImpactTick - Now())) * 0.08f;
        if (score > bestScore) {
            best = &threat;
            bestScore = score;
        }
    }
    if (!best) return false;

    const AIHeroClient ally = AllyByNetworkId(best->TargetId);
    const bool emergency = best->HardCrowdControl ||
        best->EstimatedDamage >= ally.Health() * 0.42f ||
        ally.HealthPercent() <= Slider(ShieldMenu, "EmergencyHp", 38);
    if (PassiveStacks == 3 && AoeStunReserved() && !emergency &&
        Bool(ShieldMenu, "PreserveThree", true)) {
        return false;
    }
    const ShieldReason reason = best->HardCrowdControl
        ? ShieldReason::IncomingHardCc
        : (best->AutoAttack
            ? ShieldReason::IncomingAttack
            : ShieldReason::IncomingBurst);
    if (CastMoltenShield(ally,
            mode == Mode::None ? Mode::Automatic : mode,
            reason, true)) {
        best->ExpireTick = Now() - 1;
        return true;
    }
    return false;
}

inline void RefreshTibbers() {
    AIMinionClient pet = Tibbers();
    if (!pet.IsValid() || pet.IsDead()) {
        for (const auto& candidate : GameObjects::AllyPets()) {
            const std::string character = candidate.CharacterName();
            const std::string name = candidate.Name();
            if (candidate.IsValid() && !candidate.IsDead() &&
                IsTibbersName(character.c_str(), name.c_str())) {
                pet = candidate;
                TibbersNetworkId = static_cast<int>(candidate.NetworkId());
                break;
            }
        }
    }
    if (pet.IsValid() && !pet.IsDead()) {
        if (!TibbersActive) {
            TibbersActive = true;
            TibbersSpawnTick = Now();
            TibbersExpireTick = Now() +
                static_cast<int>(kTibbersLifetimeSeconds * 1000.0f);
            TibbersEnrageUntil = std::max(
                TibbersEnrageUntil,
                Now() + static_cast<int>(kTibbersEnrageSeconds * 1000.0f));
            TibbersEnrageAttacks = 0;
        }
        TibbersLastPosition = pet.Position();
        return;
    }
    if (TibbersActive &&
        (TibbersExpireTick <= Now() || TibbersNetworkId != 0)) {
        TibbersActive = false;
        TibbersNetworkId = 0;
        TibbersExpireTick = 0;
        TibbersEnrageUntil = 0;
        TibbersLastTargetId = 0;
        LastPetPurpose = PetPurpose::None;
    }
}

inline bool PetTargetSafe(const AIBaseClient& target) {
    if (!target.IsValid() || target.IsDead() || !target.IsEnemy() ||
        !target.IsTargetable()) {
        return false;
    }
    const bool diveAllowed = Bool(PetMenu, "AllowPetDive", false) &&
                             target.HealthPercent() <=
                                 Slider(PetMenu, "PetDiveTargetHp", 22);
    return !Engine::UnderEnemyTurret(target.Position()) || diveAllowed;
}

inline bool IssuePetAttack(const AIBaseClient& target,
                           PetPurpose purpose) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !PetTargetSafe(target) ||
        Now() - TibbersLastOrderTick < kPetOrderThrottleMs) {
        return false;
    }
    ControllerPetOrderUntil = Now() + 220;
    if (!SDK::IssueOrder(player, SDK::GameObjectOrder::PetAttack, target)) {
        return false;
    }
    TibbersLastOrderTick = Now();
    TibbersLastTargetId = static_cast<int>(target.NetworkId());
    LastPetPurpose = purpose;
    return true;
}

inline bool IssuePetMove(const Vector3& destination,
                         PetPurpose purpose) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !destination.IsValid() || destination.IsZero() ||
        Now() - TibbersLastOrderTick < kPetOrderThrottleMs) {
        return false;
    }
    ControllerPetOrderUntil = Now() + 220;
    if (!SDK::IssueOrder(player, SDK::GameObjectOrder::PetMove, destination)) {
        return false;
    }
    TibbersLastOrderTick = Now();
    TibbersLastTargetId = 0;
    LastPetPurpose = purpose;
    return true;
}

inline bool TryPetMicro(const AIHeroClient& selected, Mode mode) {
    if (!TibbersActive || !Bool(PetMenu, "SoftAutopilot", true)) return false;
    const AIMinionClient pet = Tibbers();
    const auto player = GameObjects::Player();
    if (!pet.IsValid() || pet.IsDead() || !player.IsValid()) return false;
    if (Now() < ManualPetLockUntil ||
        Now() - TibbersLastOrderTick < kPetOrderThrottleMs) {
        return false;
    }

    const bool allowDiveToggle = Bool(PetMenu, "AllowPetDive", false);
    const bool underTurret = Engine::UnderEnemyTurret(pet.Position());
    const float distanceToOwner = pet.Position().Distance2D(player.Position());
    if (underTurret && !allowDiveToggle) {
        ActiveSequence = Sequence::TibbersRecall;
        return IssuePetMove(
            player.Position(), PetPurpose::ReturnFromTurret);
    }

    AIHeroClient target = selected;
    const AIHeroClient protectedAlly = ProtectedAlly();
    const AIHeroClient peel = SelectPeelThreat(protectedAlly);
    if (Engine::ValidEnemy(peel) &&
        (mode == Mode::Flee || protectedAlly.HealthPercent() <= 55.0f)) {
        target = peel;
    }
    const bool targetValid = Engine::ValidEnemy(target);
    const bool targetUnderTurret = targetValid &&
        Engine::UnderEnemyTurret(target.Position());
    const bool diveAllowed = allowDiveToggle && targetValid &&
        target.HealthPercent() <= Slider(PetMenu, "PetDiveTargetHp", 22);
    const bool enraged = TibbersEnrageUntil >= Now();
    const PetCommand command = ChoosePetCommand({
        true,
        Now() < ManualPetLockUntil,
        targetValid,
        targetUnderTurret,
        diveAllowed,
        false,
        enraged,
        pet.HealthPercent(),
        distanceToOwner,
        targetValid
            ? pet.Position().Distance2D(target.Position()) : 0.0f,
    },
        static_cast<float>(Slider(PetMenu, "ReturnLeash", 1050)),
        static_cast<float>(Slider(PetMenu, "CriticalLeash", 1450)));
    if (command == PetCommand::MoveToOwner) {
        ActiveSequence = Sequence::TibbersRecall;
        return IssuePetMove(player.Position(), PetPurpose::RecoverLeash);
    }
    if (command == PetCommand::Attack && targetValid &&
        PetTargetSafe(target)) {
        ActiveSequence = enraged
            ? Sequence::TibbersEnrage : Sequence::TibbersZone;
        return IssuePetAttack(target, Engine::ValidEnemy(peel) &&
            target.NetworkId() == peel.NetworkId()
                ? PetPurpose::Peel
                : (enraged ? PetPurpose::EnrageFocus
                           : PetPurpose::FollowTarget));
    }
    return false;
}

inline void ResolveQImpact(bool likelyHit = true) {
    if (!QPending) return;
    const AIBaseClient target = UnitByNetworkId(QTargetId);
    const bool targetValid = target.IsValid() && !target.IsDead() &&
                             target.IsEnemy() && target.IsTargetable() &&
                             !IsCommonUntargetableOrImmune(target);
    if (likelyHit && targetValid) {
        const bool shield = IsSpellShielded(target);
        if (PassivePrimed) {
            ConsumePassive(PassiveSpell::Q, QTargetId, !shield);
        } else {
            GainPassiveStack();
        }
    }
    QPending = false;
    QMissileObserved = false;
    QMissileNetworkId = 0;
    QImpactTick = 0;
    QExpectedStun = false;
    QExpectedShieldBreak = false;
    if (ActiveSequence == Sequence::QStunCatch ||
        ActiveSequence == Sequence::HiddenQPrime ||
        ActiveSequence == Sequence::QShieldBreak) {
        ActiveSequence = Sequence::BurstFollowup;
    }
}

inline void ResolvePendingSpells() {
    const int now = Now();
    if (QPending && now >= QImpactTick + 80) {
        ResolveQImpact(true);
    }
    if (WPending && now >= WResolveTick) {
        const int flashTick = ManualFlashTick >= WCastTick &&
                ManualFlashTick <= WResolveTick
            ? ManualFlashTick : 0;
        const Vector3 resolveOrigin = IncinerateResolveOrigin(
            WCastOrigin, GameObjects::Player().Position(),
            flashTick, WResolveTick);
        AIHeroClient consumerTarget{};
        AIHeroClient shieldedFallback{};
        float consumerScore = -FLT_MAX;
        float fallbackScore = -FLT_MAX;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!Engine::ValidEnemy(enemy) ||
                !IncinerateHits(resolveOrigin, WAim,
                                PredictPosition(enemy, 0.02f),
                                enemy.BoundingRadius())) {
                continue;
            }
            float score = TargetPriority(enemy);
            if (static_cast<int>(enemy.NetworkId()) == WPrimaryId) score += 2.0f;
            if (score > fallbackScore) {
                shieldedFallback = enemy;
                fallbackScore = score;
            }
            if (!IsSpellShielded(enemy) && score > consumerScore) {
                consumerTarget = enemy;
                consumerScore = score;
            }
        }
        const bool anyStunned = Engine::ValidEnemy(consumerTarget);
        if (!anyStunned) consumerTarget = shieldedFallback;
        if (Engine::ValidEnemy(consumerTarget) && PassivePrimed) {
            ConsumePassive(
                PassiveSpell::W,
                static_cast<int>(consumerTarget.NetworkId()),
                anyStunned);
        }
        WPending = false;
        WExpectedStun = false;
        if (ActiveSequence == Sequence::WStunCone ||
            ActiveSequence == Sequence::ManualWFlash) {
            ActiveSequence = Sequence::BurstFollowup;
        }
    }
    if (RPending && now >= RResolveTick) {
        AIHeroClient consumerTarget{};
        AIHeroClient shieldedFallback{};
        float consumerScore = -FLT_MAX;
        float fallbackScore = -FLT_MAX;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!Engine::ValidEnemy(enemy) ||
                !TibbersSummonHits(LastSummonPlan.Center,
                                   PredictPosition(enemy, 0.02f),
                                   enemy.BoundingRadius())) {
                continue;
            }
            float score = TargetPriority(enemy);
            if (static_cast<int>(enemy.NetworkId()) == RPrimaryId) score += 2.0f;
            if (score > fallbackScore) {
                shieldedFallback = enemy;
                fallbackScore = score;
            }
            if (!IsSpellShielded(enemy) && score > consumerScore) {
                consumerTarget = enemy;
                consumerScore = score;
            }
        }
        const bool anyStunned = Engine::ValidEnemy(consumerTarget);
        if (!anyStunned) consumerTarget = shieldedFallback;
        if (Engine::ValidEnemy(consumerTarget) && PassivePrimed) {
            ConsumePassive(
                PassiveSpell::R,
                static_cast<int>(consumerTarget.NetworkId()),
                anyStunned);
        }
        RPending = false;
        RExpectedStun = false;
        TibbersActive = true;
        TibbersSpawnTick = now;
        TibbersExpireTick = now +
            static_cast<int>(kTibbersLifetimeSeconds * 1000.0f);
        TibbersEnrageUntil = now +
            static_cast<int>(kTibbersEnrageSeconds * 1000.0f);
        TibbersEnrageAttacks = 0;
        ActiveSequence = Sequence::TibbersEnrage;
    }
}

inline void RefreshPassiveFromBuff() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (player.HasBuff("pyromania_particle") ||
        player.HasBuff("anniepassiveprimed")) {
        if (!PassivePrimed || PassiveStacks != 4) SetPassiveStacks(4, true);
        PassiveBuffConfirmed = true;
    }
}

inline bool IsQMissileName(const char* first,
                           const char* second = nullptr) {
    return ControllerHelpers::AnyTextContains(
        { first, second }, { "Disintegrate", "AnnieQ" });
}

inline void RefreshTrackedQ() {
    if (!QPending) return;
    for (const auto& missile : GameObjects::Missiles()) {
        if (!missile.IsValid()) continue;
        const bool idMatch = QMissileNetworkId != 0 &&
            static_cast<int>(missile.NetworkId()) == QMissileNetworkId;
        const bool casterMatch = missile.CasterNetworkId() ==
            GameObjects::Player().NetworkId();
        const std::string spell = missile.SpellName();
        const std::string name = missile.MissileName();
        if (idMatch || (casterMatch &&
            IsQMissileName(spell.c_str(), name.c_str()))) {
            QMissileObserved = true;
            QMissileNetworkId = static_cast<int>(missile.NetworkId());
            QLastPosition = missile.Position();
            if (missile.TargetNetworkId() != 0) {
                QTargetId = missile.TargetNetworkId();
            }
            return;
        }
    }
}

inline void RefreshState() {
    RefreshTrackedQ();
    ResolvePendingSpells();
    RefreshPassiveFromBuff();
    RefreshTibbers();
    const int now = Now();
    if (GapcloserExpireTick < now) GapcloserTargetId = 0;
    if (InterruptExpireTick < now) InterruptTargetId = 0;
    if (TargetedAllyThreatUntil < now) TargetedAllyThreatId = 0;
    if (ManualFlashUntil < now) ManualFlashUntil = 0;
    if (TibbersEnrageUntil < now) TibbersEnrageAttacks = 5;
    for (auto& threat : IncomingThreats) {
        if (threat.ExpireTick < now) threat = {};
    }
    if (!QPending && !WPending && !RPending &&
        ActiveSequence == Sequence::BurstFollowup &&
        now - std::max({ QCastTick, WCastTick, RCastTick }) > 1800) {
        ActiveSequence = PassiveStacks == 3
            ? Sequence::HoldThree : Sequence::None;
        ReservedStunIntent = StunIntent::None;
    }
}

inline bool TryTwoStackLandingRace(Mode mode) {
    if (!QPending || QOriginalStacks != 2 ||
        ActiveSequence != Sequence::TwoStackRace ||
        QExpectedShieldBreak || QImpactTick <= Now() + 70) {
        return false;
    }
    const AIHeroClient target = ControllerHelpers::HeroByNetworkId(QTargetId);
    if (!Engine::ValidEnemy(target) || IsSpellShielded(target)) return false;

    if (PassiveStacks == 2 && Ready(2)) {
        return CastMoltenShield(
            GameObjects::Player(), mode,
            ShieldReason::PrimeHiddenStun, true);
    }
    if (PassiveStacks == 3 && Ready(1) &&
        target.Position().Distance2D(GameObjects::Player().Position()) <=
            kIncinerateRange + target.BoundingRadius()) {
        ConePlan cone = BuildConePlan(target, StunIntent::AoeCone, true);
        if (!cone.Valid || !cone.IncludesPrimary) return false;
        const int wLanding = Now() + 250;
        if (QImpactTick < wLanding - 20) {
            QExpectedStun = true;
            cone.WillStun = false;
            ReservedStunIntent = StunIntent::HiddenPointCatch;
        } else {
            cone.WillStun = true;
            QExpectedStun = false;
            ReservedStunIntent = StunIntent::AoeCone;
        }
        return CastIncinerate(cone, mode, true);
    }
    return false;
}

inline bool TargetCommitted(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    if (target.IsDashing() || Engine::IsHardCrowdControlled(target)) return true;
    const Vector3 end = target.PathEnd();
    if (end.IsValid() && !end.IsZero()) {
        const Vector3 movement = SharedGeometry::Direction2D(
            target.Position(), end);
        const Vector3 toward = SharedGeometry::Direction2D(
            target.Position(), player.Position());
        if (!movement.IsZero() && !toward.IsZero() &&
            movement.Dot(toward) >= 0.64f) {
            return true;
        }
    }
    return target.Position().Distance2D(player.Position()) <=
        std::max(390.0f, target.AttackRange() + 80.0f);
}

inline bool TryInterrupt() {
    if (InterruptTargetId == 0 || InterruptExpireTick < Now()) return false;
    const AIHeroClient target = ControllerHelpers::HeroByNetworkId(
        InterruptTargetId);
    if (!Engine::ValidEnemy(target, 900.0f)) return false;

    if (PassivePrimed && Ready(0) &&
        Bool(DisintegrateMenu, "Interrupt", true)) {
        const QPlan q = BuildQPlan(target, StunIntent::Interrupt, false, true);
        if (q.Valid && q.WillStun &&
            q.ImpactTick <= InterruptExpireTick + 80 &&
            CastDisintegrate(q, Mode::Automatic, true)) {
            return true;
        }
    }
    if (PassiveStacks >= 3 && Ready(1) &&
        Bool(IncinerateMenu, "Interrupt", true)) {
        ConePlan w = BuildConePlan(target, StunIntent::Interrupt, true);
        if (w.Valid && w.IncludesPrimary && w.WillStun &&
            Now() + 250 <= InterruptExpireTick + 110 &&
            CastIncinerate(w, Mode::Automatic, true)) {
            return true;
        }
    }
    if (!TibbersActive && PassiveStacks >= 3 && Ready(3) &&
        Bool(TibbersMenu, "Interrupt", true)) {
        SummonPlan r = BuildSummonPlan(target, StunIntent::Interrupt, true);
        if (r.Valid && r.IncludesPrimary && r.WillStun &&
            (r.HitCount >= 2 || target.HealthPercent() <= 42.0f) &&
            CastSummonTibbers(r, Mode::Automatic, true)) {
            return true;
        }
    }
    return false;
}

inline bool TryAntiGapcloser() {
    if (GapcloserTargetId == 0 || GapcloserExpireTick < Now()) return false;
    const AIHeroClient target = ControllerHelpers::HeroByNetworkId(
        GapcloserTargetId);
    if (!Engine::ValidEnemy(target, 850.0f)) return false;

    if (PassiveStacks >= 3 && Ready(1) &&
        Bool(IncinerateMenu, "AntiGapcloser", true)) {
        ConePlan w = BuildConePlan(target, StunIntent::AntiGapcloser, true);
        if (w.Valid && w.IncludesPrimary && w.WillStun &&
            CastIncinerate(w, Mode::Automatic, true)) {
            return true;
        }
    }
    if (PassivePrimed && Ready(0) &&
        Bool(DisintegrateMenu, "AntiGapcloser", true)) {
        const QPlan q = BuildQPlan(
            target, StunIntent::AntiGapcloser, false, true);
        if (q.Valid && q.WillStun &&
            CastDisintegrate(q, Mode::Automatic, true)) {
            return true;
        }
    }
    const auto player = GameObjects::Player();
    const bool critical = target.Position().Distance2D(player.Position()) <= 310.0f ||
                          player.HealthPercent() <= 38.0f;
    if (critical && !TibbersActive && PassiveStacks >= 3 && Ready(3) &&
        Bool(TibbersMenu, "AntiGapcloser", true)) {
        SummonPlan r = BuildSummonPlan(
            target, StunIntent::AntiGapcloser, true);
        if (r.Valid && r.IncludesPrimary && r.WillStun &&
            CastSummonTibbers(r, Mode::Automatic, true)) {
            return true;
        }
    }
    return false;
}

inline bool TryPeel(const AIHeroClient& ally,
                    const AIHeroClient& threat,
                    Mode mode) {
    if (!Engine::ValidAlly(ally) || !Engine::ValidEnemy(threat)) return false;
    const float separation = ally.Position().Distance2D(threat.Position());
    const bool committed = threat.IsDashing() || separation <=
        std::max(330.0f, threat.AttackRange() + 65.0f);
    if (!committed) return false;

    if (Ready(2) && Bool(ShieldMenu, "Peel", true) &&
        (ally.HealthPercent() <= Slider(ShieldMenu, "PeelHp", 58) ||
         threat.IsDashing()) &&
        CastMoltenShield(ally,
            mode == Mode::None ? Mode::Automatic : mode,
            ShieldReason::IncomingBurst, true)) {
        return true;
    }
    if (PassiveStacks >= 3 && Ready(1) &&
        Bool(IncinerateMenu, "Peel", true)) {
        ConePlan w = BuildConePlan(threat, StunIntent::Peel, true);
        if (w.Valid && w.IncludesPrimary && w.WillStun &&
            CastIncinerate(w,
                mode == Mode::None ? Mode::Automatic : mode, true)) {
            ActiveSequence = Sequence::PeelChain;
            return true;
        }
    }
    if (PassivePrimed && Ready(0) &&
        Bool(DisintegrateMenu, "Peel", true)) {
        QPlan q = BuildQPlan(threat, StunIntent::Peel, false, true);
        if (q.Valid && q.WillStun &&
            CastDisintegrate(q,
                mode == Mode::None ? Mode::Automatic : mode, true)) {
            ActiveSequence = Sequence::PeelChain;
            return true;
        }
    }
    if (!TibbersActive && PassiveStacks >= 3 && Ready(3) &&
        Bool(TibbersMenu, "Peel", true) &&
        (ally.HealthPercent() <= Slider(TibbersMenu, "PeelAllyHp", 45) ||
         Engine::CountEnemiesAt(ally.Position(), kTibbersRadius + 90.0f) >= 2)) {
        SummonPlan r = BuildSummonPlan(threat, StunIntent::Peel, true);
        if (r.Valid && r.IncludesPrimary && r.WillStun &&
            CastSummonTibbers(r,
                mode == Mode::None ? Mode::Automatic : mode, true)) {
            ActiveSequence = Sequence::PeelChain;
            return true;
        }
    }
    return false;
}

inline bool TryAoeSetup(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target) || PassiveStacks != 2 || !Ready(2)) {
        return false;
    }
    const SummonPlan r = BuildSummonPlan(
        target, StunIntent::AoeSummon, false);
    const ConePlan w = BuildConePlan(target, StunIntent::AoeCone, false);
    const bool rWanted = r.Valid && r.HitCount >=
        Slider(TibbersMenu, "StunMinimumTargets", 2) &&
        r.Score >= Slider(TibbersMenu, "MinimumScore", 430);
    const bool wWanted = w.Valid && w.HitCount >=
        Slider(IncinerateMenu, "StunMinimumTargets", 2);
    const bool rFeasible = rWanted && HasResourceFor({ 2, 3 });
    const bool wFeasible = wWanted && HasResourceFor({ 2, 1 });
    if (!rFeasible && !wFeasible) return false;
    ReservedStunIntent = rFeasible &&
        (!wFeasible || r.Score > w.Score + 80.0f)
        ? StunIntent::AoeSummon : StunIntent::AoeCone;
    return CastMoltenShield(
        GameObjects::Player(), mode,
        ShieldReason::PrimeHiddenStun, false);
}

inline bool TryUseReservedAoe(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target) || PassiveStacks < 3) return false;
    if (ReservedStunIntent == StunIntent::AoeSummon &&
        !TibbersActive && Ready(3)) {
        SummonPlan r = BuildSummonPlan(
            target, StunIntent::AoeSummon, false);
        if (r.Valid && r.WillStun && r.HitCount >=
                Slider(TibbersMenu, "StunMinimumTargets", 2) &&
            r.Score >= Slider(TibbersMenu, "MinimumScore", 430) &&
            CastSummonTibbers(r, mode)) {
            return true;
        }
    }
    if (ReservedStunIntent == StunIntent::AoeCone && Ready(1)) {
        ConePlan w = BuildConePlan(target, StunIntent::AoeCone, false);
        if (w.Valid && w.WillStun && w.HitCount >=
                Slider(IncinerateMenu, "StunMinimumTargets", 2) &&
            CastIncinerate(w, mode)) {
            return true;
        }
    }
    return false;
}

inline bool TryBurstFollowup(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if ((QPending && QExpectedStun) ||
        (WPending && WExpectedStun) ||
        (RPending && RExpectedStun)) {
        return false;
    }
    if (Ready(1)) {
        ConePlan w = BuildConePlan(target, StunIntent::None, false);
        if (w.Valid && w.IncludesPrimary &&
            (Engine::IsHardCrowdControlled(target) ||
             TargetCommitted(target) || WDamage(target) >=
                target.Health() + target.AllShield()) &&
            CastIncinerate(w, mode)) {
            return true;
        }
    }
    if (Ready(0)) {
        QPlan q = BuildQPlan(target, StunIntent::PointCatch);
        if (q.Valid && (q.Lethal || TargetCommitted(target) ||
            Engine::IsHardCrowdControlled(target)) &&
            CastDisintegrate(q, mode)) {
            return true;
        }
    }
    if (Ready(2) && InAutoAttackRange(target, 30.0f) &&
        Now() - LastAutoTick <= 340 &&
        Bool(ShieldMenu, "TradeShield", true)) {
        return CastMoltenShield(
            GameObjects::Player(), mode,
            ShieldReason::SelfTrade, false);
    }
    return false;
}

inline bool TryCombo(const AIHeroClient& selected) {
    if (!Engine::ValidEnemy(selected)) return false;
    if (TryTwoStackLandingRace(Mode::Combo)) return true;
    if (TryPrimePendingQ(Mode::Combo)) return true;
    if (QPending || WPending || RPending) return false;
    if (IsSpellShielded(selected) && Ready(0) &&
        Bool(DisintegrateMenu, "BreakSpellShield", true) &&
        (!PassivePrimed ||
         Bool(DisintegrateMenu, "SpendStunOnShield", false))) {
        QPlan breaker = BuildQPlan(
            selected, StunIntent::ShieldBreak, false, false);
        if (breaker.Valid &&
            CastDisintegrate(breaker, Mode::Combo)) {
            return true;
        }
    }
    if (TryUseReservedAoe(selected, Mode::Combo)) return true;
    if (TryAoeSetup(selected, Mode::Combo)) return true;

    const SummonPlan r = BuildSummonPlan(
        selected, StunIntent::AoeSummon, false);
    const ConePlan w = BuildConePlan(
        selected, StunIntent::AoeCone, false);
    const bool rStun = r.Valid && r.WillStun &&
        r.HitCount >= Slider(TibbersMenu, "StunMinimumTargets", 2) &&
        r.Score >= Slider(TibbersMenu, "MinimumScore", 430);
    const bool wStun = w.Valid && w.WillStun &&
        w.HitCount >= Slider(IncinerateMenu, "StunMinimumTargets", 2);
    if (rStun && (!wStun || r.Score >= w.Score + 90.0f) &&
        Bool(TibbersMenu, "Combo", true) &&
        CastSummonTibbers(r, Mode::Combo)) {
        return true;
    }
    if (wStun && Bool(IncinerateMenu, "Combo", true) &&
        CastIncinerate(w, Mode::Combo)) {
        return true;
    }

    if (PassivePrimed && Ready(0)) {
        QPlan q = BuildQPlan(selected, StunIntent::PointCatch);
        if (q.Valid && q.WillStun &&
            (!EnemyFlashReady(selected) || TargetCommitted(selected) ||
             selected.Position().Distance2D(
                 GameObjects::Player().Position()) <= 500.0f) &&
            CastDisintegrate(q, Mode::Combo)) {
            return true;
        }
    }

    if (!TibbersActive && Ready(3) && Bool(TibbersMenu, "Combo", true) &&
        r.Valid && (r.HitCount >= 2 ||
            ConservativeComboDamage(selected, true) >=
                selected.Health() + selected.AllShield() ||
            CountAlliedFollowup(selected.Position(), 850.0f, true) >= 2) &&
        r.Score >= Slider(TibbersMenu, "NonStunMinimumScore", 520) &&
        CastSummonTibbers(r, Mode::Combo)) {
        return true;
    }

    if (PassiveStacks == 3 && Ready(0) && Ready(2)) {
        QPlan q = BuildQPlan(selected, StunIntent::HiddenPointCatch);
        if (q.Valid && TargetCommitted(selected) &&
            CastDisintegrate(q, Mode::Combo)) {
            return true;
        }
    }
    if (PassiveStacks == 2 && Ready(0) && Ready(1) && Ready(2) &&
        HasResourceFor({ 0, 1, 2 }) &&
        selected.Position().Distance2D(GameObjects::Player().Position()) <=
            kIncinerateRange + selected.BoundingRadius()) {
        QPlan q = BuildQPlan(selected, StunIntent::HiddenPointCatch);
        if (q.Valid && TargetCommitted(selected) &&
            CastDisintegrate(q, Mode::Combo)) {
            ActiveSequence = Sequence::TwoStackRace;
            return true;
        }
    }
    return TryBurstFollowup(selected, Mode::Combo);
}

inline bool TryHarass(const AIHeroClient& selected) {
    if (!Engine::ValidEnemy(selected) ||
        ControllerHelpers::PlayerManaPercent() < Slider(DisintegrateMenu, "HarassMana", 42)) {
        return false;
    }
    if (TryPrimePendingQ(Mode::Harass)) return true;
    if (QPending || WPending || RPending) return false;

    if (PassiveStacks == 3 && Ready(0) && Ready(2) &&
        Bool(PassiveMenu, "HiddenQPrime", true) && TargetCommitted(selected)) {
        QPlan q = BuildQPlan(selected, StunIntent::HiddenPointCatch);
        if (q.Valid && CastDisintegrate(q, Mode::Harass)) return true;
    }
    if (PassivePrimed && Bool(PassiveMenu, "HoldStunHarass", true) &&
        !TargetCommitted(selected)) {
        return false;
    }
    if (Ready(0) && Bool(DisintegrateMenu, "Harass", true) &&
        (TargetCommitted(selected) || Now() - LastAutoTick <= 320)) {
        QPlan q = BuildQPlan(selected, StunIntent::PointCatch);
        if (q.Valid && CastDisintegrate(q, Mode::Harass)) return true;
    }
    if (Ready(1) && Bool(IncinerateMenu, "Harass", false) &&
        ControllerHelpers::PlayerManaPercent() >= Slider(IncinerateMenu, "HarassMana", 68)) {
        ConePlan w = BuildConePlan(selected, StunIntent::None);
        if (w.Valid && w.IncludesPrimary && w.HitCount >= 2 &&
            CastIncinerate(w, Mode::Harass)) {
            return true;
        }
    }
    if (Ready(2) && Bool(ShieldMenu, "TradeShield", true) &&
        Now() - LastAutoTick <= 300 && InAutoAttackRange(selected, 35.0f)) {
        return CastMoltenShield(
            GameObjects::Player(), Mode::Harass,
            ShieldReason::SelfTrade, false);
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& selected) {
    const AIHeroClient pursuer = NearestEnemyToPlayer(selected, 900.0f);
    if (Ready(2) && Bool(ShieldMenu, "Flee", true) &&
        CastMoltenShield(GameObjects::Player(), Mode::Flee,
                         ShieldReason::FleeSpeed, true)) {
        return true;
    }
    if (!Engine::ValidEnemy(pursuer)) return false;
    if (PassiveStacks >= 3 && Ready(1) &&
        Bool(IncinerateMenu, "Flee", true)) {
        ConePlan w = BuildConePlan(pursuer, StunIntent::Flee, true);
        if (w.Valid && w.IncludesPrimary && w.WillStun &&
            CastIncinerate(w, Mode::Flee, true)) {
            return true;
        }
    }
    if (PassivePrimed && Ready(0) &&
        Bool(DisintegrateMenu, "Flee", true)) {
        QPlan q = BuildQPlan(pursuer, StunIntent::Flee, false, true);
        if (q.Valid && q.WillStun &&
            CastDisintegrate(q, Mode::Flee, true)) {
            return true;
        }
    }
    if (!TibbersActive && PassiveStacks >= 3 && Ready(3) &&
        Bool(TibbersMenu, "Flee", true) &&
        GameObjects::Player().HealthPercent() <= 42.0f) {
        SummonPlan r = BuildSummonPlan(pursuer, StunIntent::Flee, true);
        if (r.Valid && r.IncludesPrimary && r.WillStun &&
            CastSummonTibbers(r, Mode::Flee, true)) {
            return true;
        }
    }
    return false;
}

inline AIBaseClient BestQFarmTarget(Mode mode) {
    AIBaseClient best{};
    float bestMargin = FLT_MAX;
    auto evaluate = [&](const AIBaseClient& unit) {
        if (!ValidHostileUnitInGameplayRange(
                unit, kDisintegrateRange)) return;
        const float predicted = Engine::RuntimeSpells[0]
            ? Engine::RuntimeSpells[0]->GetHealthPrediction(unit)
            : unit.Health();
        const float damage = QDamage(unit);
        if (predicted <= 0.0f || predicted > damage) return;
        float margin = damage - predicted;
        if (IsEpicMonster(unit)) margin -= 500.0f;
        if (margin < bestMargin) {
            best = unit;
            bestMargin = margin;
        }
    };
    if (mode == Mode::Jungle) {
        for (const auto& unit : GameObjects::Jungle()) evaluate(unit);
    } else {
        for (const auto& unit : GameObjects::EnemyMinions()) evaluate(unit);
    }
    return best;
}

inline bool TryQRefundFarm(Mode mode) {
    if (!Ready(0) || QPending || !Bool(FarmMenu, "UseQ", true)) return false;
    const bool preserve = PassivePrimed &&
        ControllerHelpers::HasEnemyChampionNear(1200.0f) &&
        Bool(FarmMenu, "PreserveStun", true);
    if (preserve && mode != Mode::Jungle) return false;
    const AIBaseClient target = BestQFarmTarget(mode);
    if (!target.IsValid()) return false;
    QPlan q = BuildQPlan(target, StunIntent::None, true, false);
    if (!q.Valid) return false;
    return CastDisintegrate(q, mode, false);
}

inline ConePlan BuildFarmCone(Mode mode) {
    ConePlan best{};
    const auto player = GameObjects::Player();
    std::vector<AIBaseClient> units;
    if (mode == Mode::Jungle) {
        for (const auto& unit : GameObjects::Jungle()) {
            if (ValidHostileUnitInGameplayRange(unit, 850.0f)) {
                units.push_back(unit);
            }
        }
    } else {
        for (const auto& unit : GameObjects::EnemyMinions()) {
            if (ValidHostileUnitInGameplayRange(unit, 850.0f)) {
                units.push_back(unit);
            }
        }
    }
    for (const auto& anchor : units) {
        const Vector3 predicted = PredictPosition(anchor, 0.25f);
        const Vector3 direction = SharedGeometry::Direction2D(
            player.Position(), predicted);
        if (direction.IsZero()) continue;
        const Vector3 aim = player.Position() + direction * kIncinerateRange;
        ConePlan plan{};
        plan.Aim = aim;
        plan.Intent = StunIntent::None;
        for (const auto& unit : units) {
            if (IncinerateHits(player.Position(), aim,
                    PredictPosition(unit, 0.25f), unit.BoundingRadius())) {
                ++plan.HitCount;
                if (IsEpicMonster(unit) || unit.MaxHealth() >= 1800.0f) {
                    ++plan.PriorityHits;
                }
            }
        }
        plan.Score = plan.HitCount * 110.0f + plan.PriorityHits * 420.0f;
        plan.Valid = plan.HitCount > 0;
        if (plan.Valid && (!best.Valid || plan.Score > best.Score)) best = plan;
    }
    return best;
}

inline bool TryFarm(Mode mode) {
    if (QPending || WPending || RPending) return false;
    const int minimumMana = mode == Mode::Jungle
        ? Slider(FarmMenu, "JungleMana", 28)
        : Slider(FarmMenu, "LaneMana", 52);
    if (ControllerHelpers::PlayerManaPercent() < minimumMana && mode != Mode::LastHit) return false;
    if (TryQRefundFarm(mode)) return true;
    if (mode == Mode::LastHit) return false;

    if (Ready(1) && Bool(FarmMenu, "UseW", true) &&
        !(PassivePrimed && ControllerHelpers::HasEnemyChampionNear(1250.0f) &&
          Bool(FarmMenu, "PreserveStun", true))) {
        ConePlan w = BuildFarmCone(mode);
        const int minimum = mode == Mode::Jungle
            ? Slider(FarmMenu, "WJungleHits", 2)
            : Slider(FarmMenu, "WLaneHits", 4);
        if (w.Valid && (w.HitCount >= minimum || w.PriorityHits > 0) &&
            CastIncinerate(w, mode)) {
            ActiveSequence = mode == Mode::Jungle
                ? Sequence::JungleCycle : Sequence::FarmRefund;
            return true;
        }
    }

    if (TibbersActive && Bool(PetMenu, "FarmWithPet", true)) {
        AIBaseClient best{};
        float health = -1.0f;
        const auto& candidates = mode == Mode::Jungle
            ? GameObjects::Jungle() : GameObjects::EnemyMinions();
        for (const auto& unit : candidates) {
            if (ValidHostileUnitInGameplayRange(unit, 1100.0f) &&
                unit.Health() > health &&
                PetTargetSafe(unit)) {
                best = unit;
                health = unit.Health();
            }
        }
        if (best.IsValid() && IssuePetAttack(
                best, mode == Mode::Jungle
                    ? PetPurpose::Objective : PetPurpose::FollowTarget)) {
            return true;
        }
    }
    return false;
}

inline bool TryKillSecure() {
    if (!Bool(DisintegrateMenu, "KillSecure", true)) return false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 760.0f) ||
            IsCommonUntargetableOrImmune(enemy)) {
            continue;
        }
        if (Ready(0)) {
            QPlan q = BuildQPlan(enemy, StunIntent::PointCatch);
            if (q.Valid && q.Lethal &&
                CastDisintegrate(q, Mode::Automatic, true)) {
                return true;
            }
        }
        if (Ready(1) && WDamage(enemy) >=
                enemy.Health() + enemy.AllShield()) {
            ConePlan w = BuildConePlan(enemy, StunIntent::None, true);
            if (w.Valid && w.IncludesPrimary &&
                CastIncinerate(w, Mode::Automatic, true)) {
                return true;
            }
        }
        if (!TibbersActive && Ready(3) &&
            Bool(TibbersMenu, "KillSecure", false) &&
            RDamage(enemy) + TibbersContactDamage(enemy, 0.75f, 1) >=
                enemy.Health() + enemy.AllShield()) {
            SummonPlan r = BuildSummonPlan(
                enemy, PassiveStacks >= 3
                    ? StunIntent::AoeSummon : StunIntent::None, true);
            if (r.Valid && r.IncludesPrimary &&
                (r.HitCount >= 2 || enemy.HealthPercent() <= 18.0f) &&
                CastSummonTibbers(r, Mode::Automatic, true)) {
                return true;
            }
        }
    }
    return false;
}

inline bool TryBuildPassiveStacks() {
    if (!Ready(2) || !Bool(PassiveMenu, "BuildStacks", true) ||
        PassiveStacks >= 3 ||
        ControllerHelpers::HasEnemyChampionNear(1450.0f) ||
        ControllerHelpers::PlayerManaPercent() < Slider(PassiveMenu, "BuildMana", 64)) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (player.IsRecalling() || TibbersActive &&
        Tibbers().IsValid() && Tibbers().HealthPercent() <= 25.0f) {
        return false;
    }
    return CastMoltenShield(
        player, Mode::Automatic,
        TibbersActive ? ShieldReason::TibbersSupport
                      : ShieldReason::StackBuilding, false);
}

inline Posture ChoosePosture(Mode mode,
                             const AIHeroClient& selected,
                             const AIHeroClient& ally,
                             const AIHeroClient& threat) {
    if (mode == Mode::Flee) return Posture::Disengage;
    if (Engine::ValidAlly(ally) && Engine::ValidEnemy(threat) &&
        threat.Position().Distance2D(ally.Position()) <= 620.0f) {
        return Posture::Peel;
    }
    if (TibbersActive && mode == Mode::None) return Posture::TibbersControl;
    if (mode == Mode::LaneClear || mode == Mode::LastHit) {
        return HasNearbyJungleTarget(850.0f) ? Posture::Objective
                                    : Posture::LaneControl;
    }
    if (mode == Mode::Harass) return Posture::ShortTrade;
    if (mode == Mode::Combo && Engine::ValidEnemy(selected)) {
        const int nearby = Engine::CountEnemiesAt(
            selected.Position(), 700.0f);
        if (nearby >= 3) return Posture::Teamfight;
        if (TibbersActive && selected.Position().Distance2D(
                GameObjects::Player().Position()) >
                    kDisintegrateRange) {
            return Posture::Siege;
        }
        if (PassivePrimed || PassiveStacks == 3) return Posture::Catch;
        return Posture::Ambush;
    }
    return Posture::Neutral;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshState();
    const AIHeroClient ally = ProtectedAlly();
    const AIHeroClient threat = SelectPeelThreat(ally);
    CurrentPosture = ChoosePosture(mode, selected, ally, threat);

    if (TryReactiveShield(mode)) return true;
    if (TryPrimePendingQ(mode == Mode::None ? Mode::Automatic : mode)) return true;
    if (TryTwoStackLandingRace(
            mode == Mode::None ? Mode::Automatic : mode)) return true;
    if (TryInterrupt()) return true;
    if (TryAntiGapcloser()) return true;
    if (Bool(TacticsMenu, "PeelBeforeDamage", true) &&
        TryPeel(ally, threat, mode)) {
        return true;
    }
    if (TryKillSecure()) return true;

    bool action = false;
    if (mode == Mode::Flee) {
        action = TryFlee(selected);
    } else if (mode == Mode::Combo) {
        action = TryCombo(selected);
    } else if (mode == Mode::Harass) {
        action = TryHarass(selected);
    } else if (mode == Mode::LaneClear) {
        action = TryFarm(HasNearbyJungleTarget(850.0f) ? Mode::Jungle
                                              : Mode::LaneClear);
    } else if (mode == Mode::LastHit) {
        action = TryFarm(Mode::LastHit);
    }
    if (action) return true;
    if (TryPetMicro(selected, mode)) return true;
    if (mode == Mode::None) return TryBuildPassiveStacks();
    return false;
}

inline void ObserveManualQ(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    const std::uint32_t targetNetworkId = args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId;
    const AIBaseClient target = UnitByNetworkId(
        static_cast<int>(targetNetworkId));
    const float distance = target.IsValid()
        ? player.Position().Distance2D(target.Position())
        : kDisintegrateRange;
    QPending = true;
    QMissileObserved = false;
    QCastTick = Now();
    QImpactTick = QCastTick + static_cast<int>(std::ceil(
        DisintegrateImpactSeconds(distance) * 1000.0f));
    QTargetId = static_cast<int>(targetNetworkId);
    QMissileNetworkId = 0;
    QOriginalStacks = PassiveStacks;
    QWasManual = true;
    QExpectedShieldBreak = target.IsValid() && IsSpellShielded(target);
    QExpectedStun = PassivePrimed ||
        (PassiveStacks == 3 && Ready(2) && !QExpectedShieldBreak);
    QOrigin = args.StartPosition.IsValid() &&
            !args.StartPosition.IsZero()
        ? args.StartPosition : player.Position();
    QLastPosition = QOrigin;
    LastQPlan = target.IsValid()
        ? BuildQPlan(target,
              QExpectedShieldBreak ? StunIntent::ShieldBreak
                                   : StunIntent::PointCatch)
        : QPlan{};
    if (QExpectedShieldBreak) {
        ActiveSequence = Sequence::QShieldBreak;
    } else if (PassiveStacks == 3 && Ready(2)) {
        ActiveSequence = Sequence::HiddenQPrime;
        ReservedStunIntent = StunIntent::HiddenPointCatch;
    } else if (PassiveStacks == 2 && Ready(1) && Ready(2)) {
        ActiveSequence = Sequence::TwoStackRace;
    } else if (PassivePrimed) {
        ActiveSequence = Sequence::QStunCatch;
    }
}

inline void ObserveManualW(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    WPending = true;
    WCastTick = Now();
    WResolveTick = WCastTick + 250;
    WPrimaryId = 0;
    WCastStartingStacks = PassiveStacks;
    WWasManual = true;
    WCastOrigin = args.StartPosition.IsValid() &&
            !args.StartPosition.IsZero()
        ? args.StartPosition : player.Position();
    WAim = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
        ? args.EndPosition : args.CastPosition;
    const int earlier = EarliestPendingConsumerTick(PassiveSpell::W);
    WExpectedStun = std::min(4, PassiveStacks + 1) >= 4 &&
        (earlier == 0 || WResolveTick <= earlier + 20);
    GainPassiveStack();
    if (WExpectedStun) {
        ActiveSequence = ManualFlashUntil >= Now()
            ? Sequence::ManualWFlash : Sequence::WStunCone;
        ReservedStunIntent = StunIntent::AoeCone;
    }
}

inline void ObserveManualR(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (TibbersActive) {
        if (ControllerPetOrderUntil < Now()) {
            ManualPetLockUntil = Now() + kPetManualLockMs;
            LastPetPurpose = PetPurpose::Manual;
        }
        return;
    }
    RPending = true;
    RCastTick = Now();
    RResolveTick = RCastTick + 250;
    RPrimaryId = 0;
    RCastStartingStacks = PassiveStacks;
    RWasManual = true;
    Vector3 center = args.EndPosition.IsValid() &&
            !args.EndPosition.IsZero()
        ? args.EndPosition : args.CastPosition;
    if (!center.IsValid() || center.IsZero()) {
        center = GameObjects::Player().Position();
    }
    LastSummonPlan = {};
    LastSummonPlan.Center = center;
    LastSummonPlan.Intent = StunIntent::AoeSummon;
    LastSummonPlan.Valid = true;
    const int earlier = EarliestPendingConsumerTick(PassiveSpell::R);
    RExpectedStun = std::min(4, PassiveStacks + 1) >= 4 &&
        (earlier == 0 || RResolveTick <= earlier + 20);
    LastSummonPlan.WillStun = RExpectedStun;
    GainPassiveStack();
    ActiveSequence = ManualFlashUntil >= Now()
        ? Sequence::ManualRFlash
        : (RExpectedStun ? Sequence::RStunEngage
                         : Sequence::BurstFollowup);
    ReservedStunIntent = StunIntent::AoeSummon;
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Summoner1) ||
        args.Slot == static_cast<int>(SDK::SpellSlot::Summoner2) ||
        SpellEventNameContains(args, "SummonerFlash")) {
        if (SpellEventNameContains(args, "Flash")) {
            ManualFlashTick = Now();
            ManualFlashUntil = Now() + 520;
            if (WPending && WResolveTick >= Now() - 30) {
                ActiveSequence = Sequence::ManualWFlash;
            } else if (RPending && RResolveTick >= Now() - 30) {
                ActiveSequence = Sequence::ManualRFlash;
            }
        }
        return;
    }

    const bool ours = args.Slot >= 0 && args.Slot < 4 &&
                      Engine::WasControllerCast(args.Slot);
    if (args.Slot == 0 || SpellEventNameContains(args, "Disintegrate")) {
        if (!ours) ObserveManualQ(args);
        return;
    }
    if (args.Slot == 1 || SpellEventNameContains(args, "Incinerate")) {
        if (!ours) ObserveManualW(args);
        return;
    }
    if (args.Slot == 2 ||
        ControllerHelpers::SpellEventNameContainsAny(
            args, { "MoltenShield", "AnnieE" })) {
        if (!ours) {
            GainPassiveStack();
            LastShieldReason = ShieldReason::None;
        }
        return;
    }
    if (args.Slot == 3 ||
        ControllerHelpers::SpellEventNameContainsAny(
            args, { "InfernalGuardian", "AnnieR" })) {
        if (!ours) ObserveManualR(args);
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick)) {
        return;
    }
    if (TibbersActive && args.Sender.IsValid() && args.IsAutoAttack &&
        static_cast<int>(args.Sender.NetworkId) == TibbersNetworkId) {
        ++TibbersEnrageAttacks;
        TibbersLastTargetId = static_cast<int>(
            args.TargetNetworkId != 0
                ? args.TargetNetworkId : args.Target.NetworkId);
    }
    RecordIncomingThreats(args);
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args,
                            bool added) {
    if (!args.Sender.IsValid()) return;
    const auto player = GameObjects::Player();
    const int senderId = static_cast<int>(args.Sender.NetworkId);
    const bool local = senderId == player.NetworkId();
    if (local && (Engine::TextContains(args.BuffName, "pyromania") ||
                  Engine::TextContains(args.BuffName, "AnniePassive"))) {
        if (added) {
            if (Engine::TextContains(args.BuffName, "particle") ||
                Engine::TextContains(args.BuffName, "primed")) {
                SetPassiveStacks(4, true);
            } else if (args.Count >= 0 && args.Count <= 4) {
                SetPassiveStacks(args.Count, true);
            }
        } else if (Engine::TextContains(args.BuffName, "particle") ||
                   Engine::TextContains(args.BuffName, "primed")) {
            PassiveBuffConfirmed = false;
            if (Now() - PassiveLastConsumeTick <= kPassiveReconcileMs) {
                SetPassiveStacks(0, false);
            }
        }
    }
    if (senderId == TibbersNetworkId &&
        Engine::TextContains(args.BuffName, "enrage")) {
        if (added) {
            const int duration = args.EndTime > Game::Time()
                ? static_cast<int>((args.EndTime - Game::Time()) * 1000.0f)
                : static_cast<int>(kTibbersEnrageSeconds * 1000.0f);
            TibbersEnrageUntil = Now() + std::max(250, duration);
            TibbersEnrageAttacks = 0;
        } else {
            TibbersEnrageUntil = 0;
            TibbersEnrageAttacks = 5;
        }
    }
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !ObjectEventIsAllied(args)) return;
    if (!IsTibbersName(args.Sender.Name, args.Sender.CharacterName)) return;
    TibbersActive = true;
    TibbersNetworkId = static_cast<int>(args.Sender.NetworkId);
    TibbersSpawnTick = Now();
    TibbersExpireTick = Now() +
        static_cast<int>(kTibbersLifetimeSeconds * 1000.0f);
    TibbersEnrageUntil = Now() +
        static_cast<int>(kTibbersEnrageSeconds * 1000.0f);
    TibbersEnrageAttacks = 0;
    TibbersLastPosition = args.Sender.Position;
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id == TibbersNetworkId ||
        (ObjectEventIsAllied(args) &&
         IsTibbersName(args.Sender.Name, args.Sender.CharacterName))) {
        TibbersActive = false;
        TibbersNetworkId = 0;
        TibbersExpireTick = 0;
        TibbersEnrageUntil = 0;
        TibbersLastTargetId = 0;
        LastPetPurpose = PetPurpose::None;
    }
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args) ||
        !IsQMissileName(args.SpellName, args.MissileName)) {
        return;
    }
    QPending = true;
    QMissileObserved = true;
    QMissileNetworkId = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    if (args.TargetNetworkId != 0) {
        QTargetId = static_cast<int>(args.TargetNetworkId);
    }
    QOrigin = args.StartPosition.IsValid() &&
            !args.StartPosition.IsZero()
        ? args.StartPosition : QOrigin;
    QLastPosition = args.Sender.Position.IsValid()
        ? args.Sender.Position : QOrigin;
    const AIBaseClient target = UnitByNetworkId(QTargetId);
    if (target.IsValid()) {
        QImpactTick = Now() + static_cast<int>(std::ceil(
            QLastPosition.Distance2D(target.Position()) /
                kDisintegrateSpeed * 1000.0f));
    }
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args) ||
        !IsQMissileName(args.SpellName, args.MissileName)) {
        return;
    }
    const int id = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    if (QMissileNetworkId != 0 && id != QMissileNetworkId) return;
    const AIBaseClient target = UnitByNetworkId(QTargetId);
    const Vector3 end = args.Sender.Position.IsValid()
        ? args.Sender.Position : QLastPosition;
    const bool likelyHit = target.IsValid() &&
        end.Distance2D(target.Position()) <= target.BoundingRadius() + 170.0f;
    ResolveQImpact(likelyHit || Now() >= QImpactTick - 90);
}

inline const char* SequenceName(Sequence sequence) {
    switch (sequence) {
    case Sequence::HoldThree: return "hold-three";
    case Sequence::HiddenQPrime: return "Q-E hidden";
    case Sequence::TwoStackRace: return "two-stack race";
    case Sequence::QStunCatch: return "Q catch";
    case Sequence::QShieldBreak: return "Q shield-break";
    case Sequence::WStunCone: return "W stun";
    case Sequence::RStunEngage: return "R stun";
    case Sequence::ManualWFlash: return "manual W-Flash";
    case Sequence::ManualRFlash: return "manual R-Flash";
    case Sequence::BurstFollowup: return "burst follow";
    case Sequence::PeelChain: return "peel";
    case Sequence::ShieldReaction: return "shield react";
    case Sequence::ShieldEngage: return "shield engage";
    case Sequence::FarmRefund: return "Q refund";
    case Sequence::JungleCycle: return "jungle";
    case Sequence::TibbersEnrage: return "bear enrage";
    case Sequence::TibbersZone: return "bear zone";
    case Sequence::TibbersRecall: return "bear return";
    default: return "neutral";
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::LaneControl: return "lane";
    case Posture::ShortTrade: return "short-trade";
    case Posture::Catch: return "catch";
    case Posture::Ambush: return "ambush";
    case Posture::Teamfight: return "teamfight";
    case Posture::Peel: return "peel";
    case Posture::Disengage: return "disengage";
    case Posture::Siege: return "siege";
    case Posture::Objective: return "objective";
    case Posture::TibbersControl: return "Tibbers";
    default: return "neutral";
    }
}

inline const char* ShieldReasonName(ShieldReason reason) {
    switch (reason) {
    case ShieldReason::IncomingBurst: return "burst";
    case ShieldReason::IncomingAttack: return "attack";
    case ShieldReason::IncomingHardCc: return "hard-CC";
    case ShieldReason::AllyEngage: return "engage";
    case ShieldReason::SelfTrade: return "trade";
    case ShieldReason::FleeSpeed: return "flee";
    case ShieldReason::PrimeHiddenStun: return "prime";
    case ShieldReason::StackBuilding: return "stack";
    case ShieldReason::TibbersSupport: return "bear";
    default: return "hold";
    }
}

inline const char* PetPurposeName(PetPurpose purpose) {
    switch (purpose) {
    case PetPurpose::EnrageFocus: return "enrage";
    case PetPurpose::FollowTarget: return "focus";
    case PetPurpose::Peel: return "peel";
    case PetPurpose::Zone: return "zone";
    case PetPurpose::Vision: return "vision";
    case PetPurpose::Objective: return "objective";
    case PetPurpose::ReturnFromTurret: return "leave-turret";
    case PetPurpose::RecoverLeash: return "leash";
    case PetPurpose::Manual: return "manual";
    default: return "hold";
    }
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;

    if (Bool(CoachMenu, "DrawQ", true)) {
        Drawing::DrawCircle(player.Position(), kDisintegrateRange,
                            0x55FFAA55u, 1.2f, 72);
        if (QPending) {
            const AIBaseClient target = UnitByNetworkId(QTargetId);
            if (target.IsValid()) {
                Drawing::DrawLine(
                    QLastPosition.IsValid() ? QLastPosition : player.Position(),
                    target.Position(),
                    QExpectedStun ? 0xFFFFB64Cu : 0xAAFF725Cu, 2.0f);
                Drawing::DrawCircle(target.Position(),
                    target.BoundingRadius() + 25.0f,
                    QExpectedStun ? 0xFFFFB64Cu : 0x88FF725Cu,
                    1.8f, 36);
            }
        }
    }
    if (Bool(CoachMenu, "DrawW", true)) {
        const int flashTick = WPending && ManualFlashTick >= WCastTick &&
                ManualFlashTick <= WResolveTick
            ? ManualFlashTick : 0;
        const Vector3 origin = WPending
            ? IncinerateResolveOrigin(WCastOrigin, player.Position(),
                                      flashTick, WResolveTick)
            : player.Position();
        const Vector3 direction = SharedGeometry::Direction2D(
            origin, WPending ? WAim : Game::CursorPos());
        if (!direction.IsZero()) {
            const float angle = kIncinerateHalfAngleDegrees *
                                SharedGeometry::kPi / 180.0f;
            Drawing::DrawLine(origin,
                origin + SharedGeometry::Rotate2D(direction, angle) *
                    kIncinerateRange,
                WExpectedStun ? 0xFFFFBA4Au : 0x77FF774Fu, 1.4f);
            Drawing::DrawLine(origin,
                origin + SharedGeometry::Rotate2D(direction, -angle) *
                    kIncinerateRange,
                WExpectedStun ? 0xFFFFBA4Au : 0x77FF774Fu, 1.4f);
        }
    }
    if (Bool(CoachMenu, "DrawR", true)) {
        Drawing::DrawCircle(player.Position(), kTibbersCastRange,
                            0x44E75338u, 1.0f, 72);
        if (LastSummonPlan.Valid &&
            (RPending || Now() - RCastTick <= 1100)) {
            Drawing::DrawCircle(LastSummonPlan.Center, kTibbersRadius,
                RExpectedStun ? 0xDFFFBD45u : 0xAAEB4D37u,
                RExpectedStun ? 2.8f : 1.8f, 64);
        }
    }
    if (Bool(CoachMenu, "DrawPet", true) && TibbersActive) {
        const AIMinionClient pet = Tibbers();
        if (pet.IsValid()) {
            Drawing::DrawCircle(pet.Position(), 110.0f,
                TibbersEnrageUntil >= Now() ? 0xFFFFC447u : 0xAAE6523Bu,
                2.0f, 42);
            const AIBaseClient target = UnitByNetworkId(TibbersLastTargetId);
            if (target.IsValid()) {
                Drawing::DrawLine(pet.Position(), target.Position(),
                                  0xAAFF7A45u, 1.8f);
            }
        }
    }
    if (Bool(CoachMenu, "DrawPeel", true)) {
        const AIHeroClient ally = AllyByNetworkId(ProtectedAllyId);
        const AIHeroClient threat = ControllerHelpers::HeroByNetworkId(
            PeelThreatId);
        if (Engine::ValidAlly(ally)) {
            Drawing::DrawCircle(ally.Position(), 95.0f,
                                0xAA6FE9FFu, 1.8f, 40);
        }
        if (Engine::ValidAlly(ally) && Engine::ValidEnemy(threat)) {
            Drawing::DrawLine(ally.Position(), threat.Position(),
                              0xFFFF5B62u, 2.0f);
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char state[460]{};
            const float qMs = QPending
                ? static_cast<float>(std::max(0, QImpactTick - Now())) : 0.0f;
            const float petSeconds = TibbersActive
                ? static_cast<float>(std::max(0, TibbersExpireTick - Now())) /
                    1000.0f : 0.0f;
            _snprintf_s(
                state, sizeof(state), _TRUNCATE,
                "Annie one-trick | %s | %s | P %d%s | Q %.0fms%s | E %s | Tibbers %s %.1fs (%s)",
                PostureName(CurrentPosture), SequenceName(ActiveSequence),
                PassiveStacks, PassivePrimed ? " STUN" : "",
                qMs, QExpectedStun ? " stun" : "",
                ShieldReasonName(LastShieldReason),
                TibbersActive ? "alive" : "none", petSeconds,
                PetPurposeName(LastPetPurpose));
            Drawing::DrawText(screen.x - 250.0f, screen.y - 118.0f,
                              0xFFFFD6A4u, state);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "AnnieOneTrick", "Annie one-trick mechanics"));
    TacticsMenu->Add(new MenuBool(
        "PeelBeforeDamage", "Protect ally before burst", true));
    TacticsMenu->Add(new MenuSeparator(
        "Ownership",
        "Flash, move, attack-move and"));

    PassiveMenu = TacticsMenu->AddSubMenu(new Menu(
        "Pyromania", "Pyromania cast/impact state and hidden stun"));
    PassiveMenu->Add(new MenuBool(
        "HiddenQPrime", "At three stacks, Q then E", true));
    PassiveMenu->Add(new MenuBool(
        "HoldStunHarass", "Hold visible stun when a", true));
    PassiveMenu->Add(new MenuBool(
        "BuildStacks", "Build only to three stacks", true));
    PassiveMenu->Add(new MenuSlider(
        "BuildMana", "Min mana passive (%)", 64, 20, 100));
    PassiveMenu->Add(new MenuSeparator(
        "LandingOrder",
        "Q gains on impact; W/E/R"));

    DisintegrateMenu = TacticsMenu->AddSubMenu(new Menu(
        "Disintegrate", "Q point catch, refund and projectile rules"));
    DisintegrateMenu->Add(new MenuBool(
        "Harass", "Q short trades", true));
    DisintegrateMenu->Add(new MenuSlider(
        "HarassMana", "Minimum mana for Q harass (%)", 42, 0, 100));
    DisintegrateMenu->Add(new MenuBool(
        "KillSecure", "Use conservatively lethal Q/W", true));
    DisintegrateMenu->Add(new MenuBool(
        "BreakSpellShield", "Q break spell shield", true));
    DisintegrateMenu->Add(new MenuBool(
        "SpendStunOnShield", "Allow a primed Q stun to be", false));
    DisintegrateMenu->Add(new MenuBool(
        "Interrupt", "Use primed Q when it lands", true));
    DisintegrateMenu->Add(new MenuBool(
        "AntiGapcloser", "Q stun on committed dash", true));
    DisintegrateMenu->Add(new MenuBool(
        "Peel", "Q peel ally", true));
    DisintegrateMenu->Add(new MenuBool(
        "Flee", "Q on closest pursuer", true));
    DisintegrateMenu->Add(new MenuSeparator(
        "Walls", "Q is rejected through Yasuo,"));

    IncinerateMenu = TacticsMenu->AddSubMenu(new Menu(
        "Incinerate", "W cone, AoE stun and player Flash buffer"));
    IncinerateMenu->Add(new MenuBool(
        "Combo", "W best stun cone", true));
    IncinerateMenu->Add(new MenuSlider(
        "StunMinimumTargets", "Min targets W stun", 2, 1, 5));
    IncinerateMenu->Add(new MenuBool(
        "Harass", "Use W harass when its cone", false));
    IncinerateMenu->Add(new MenuSlider(
        "HarassMana", "Min mana W harass (%)", 68, 35, 100));
    IncinerateMenu->Add(new MenuBool(
        "Interrupt", "W stun in cone channel", true));
    IncinerateMenu->Add(new MenuBool(
        "AntiGapcloser", "Use W's faster AoE resolve", true));
    IncinerateMenu->Add(new MenuBool(
        "Peel", "Use W cone stun to split a", true));
    IncinerateMenu->Add(new MenuBool(
        "Flee", "Use W cone stun while fleeing", true));
    IncinerateMenu->Add(new MenuSeparator(
        "WFlash", "The player presses Flash; W"));

    ShieldMenu = TacticsMenu->AddSubMenu(new Menu(
        "MoltenShield", "E ally protection, speed and stack discipline"));
    ShieldMenu->Add(new MenuBool(
        "Reactive", "Shield targeted or crossing", true));
    ShieldMenu->Add(new MenuSlider(
        "EmergencyHp", "Always override stack hold", 38, 5, 80));
    ShieldMenu->Add(new MenuBool(
        "PreserveThree", "Keep three-stack concealment", true));
    ShieldMenu->Add(new MenuBool(
        "TradeShield", "E in player AA trades", true));
    ShieldMenu->Add(new MenuBool(
        "Peel", "Shield and speed ally vs diver", true));
    ShieldMenu->Add(new MenuSlider(
        "PeelHp", "Protected ally HP for", 58, 10, 95));
    ShieldMenu->Add(new MenuBool(
        "Flee", "E for self shield and", true));

    TibbersMenu = TacticsMenu->AddSubMenu(new Menu(
        "SummonTibbers", "R placement, stun value and summon commitment"));
    TibbersMenu->Add(new MenuBool(
        "Combo", "Use scored R engage when", true));
    TibbersMenu->Add(new MenuSlider(
        "StunMinimumTargets", "Min targets R stun", 2, 1, 5));
    TibbersMenu->Add(new MenuSlider(
        "MinimumScore", "Min R stun placement", 430, 100, 1200));
    TibbersMenu->Add(new MenuSlider(
        "NonStunMinimumScore", "Minimum scored R without stun", 520, 150, 1400));
    TibbersMenu->Add(new MenuBool(
        "KillSecure", "Allow R secure for multi-hit", false));
    TibbersMenu->Add(new MenuBool(
        "Interrupt", "Use R stun for valuable", true));
    TibbersMenu->Add(new MenuBool(
        "AntiGapcloser", "Use R stun for a critical PB", true));
    TibbersMenu->Add(new MenuBool(
        "Peel", "Tibbers peel ally", true));
    TibbersMenu->Add(new MenuSlider(
        "PeelAllyHp", "Ally HP peel R (%)", 45, 10, 85));
    TibbersMenu->Add(new MenuBool(
        "Flee", "R stun last-resort", true));

    PetMenu = TacticsMenu->AddSubMenu(new Menu(
        "TibbersMicro", "Soft pet autopilot with manual ownership"));
    PetMenu->Add(new MenuBool(
        "SoftAutopilot", "Focus clear targets but", true));
    PetMenu->Add(new MenuBool(
        "AllowPetDive", "Allow Tibbers-only turret", false));
    PetMenu->Add(new MenuSlider(
        "PetDiveTargetHp", "Max target HP for optional", 22, 5, 50));
    PetMenu->Add(new MenuSlider(
        "ReturnLeash", "Tibbers return dist", 1050, 700, 1400));
    PetMenu->Add(new MenuSlider(
        "CriticalLeash", "Force return dist", 1450, 1000, 1900));
    PetMenu->Add(new MenuBool(
        "FarmWithPet", "Use Tibbers on the current", true));
    PetMenu->Add(new MenuSeparator(
        "ManualLimit",
        "SDK has no inbound Alt-click"));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Q-refund lane and conservative cone farming"));
    FarmMenu->Add(new MenuBool(
        "UseQ", "Last-hit with Q when", true));
    FarmMenu->Add(new MenuBool(
        "UseW", "W on a large wave or", true));
    FarmMenu->Add(new MenuBool(
        "PreserveStun", "Do not spend visible stun", true));
    FarmMenu->Add(new MenuSlider(
        "WLaneHits", "Minimum lane units for W", 4, 2, 8));
    FarmMenu->Add(new MenuSlider(
        "WJungleHits", "Minimum jungle units for W", 2, 1, 6));
    FarmMenu->Add(new MenuSlider(
        "LaneMana", "Minimum lane farm mana (%)", 52, 0, 100));
    FarmMenu->Add(new MenuSlider(
        "JungleMana", "Minimum jungle farm mana (%)", 28, 0, 100));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "One-trick state visualization"));
    CoachMenu->Add(new MenuBool(
        "DrawQ", "Draw Q range/target", false));
    CoachMenu->Add(new MenuBool(
        "DrawW", "Draw W cone origin", false));
    CoachMenu->Add(new MenuBool(
        "DrawR", "Draw R range/circle", false));
    CoachMenu->Add(new MenuBool(
        "DrawPet", "Draw Tibbers state", false));
    CoachMenu->Add(new MenuBool(
        "DrawPeel", "Draw ally/diver", false));
    CoachMenu->Add(new MenuBool(
        "DrawState", "State: stack, landing race,", false));
}

inline void OnLoad() {
    ActiveSequence = Sequence::None;
    CurrentPosture = Posture::Neutral;
    ReservedStunIntent = StunIntent::None;
    LastShieldReason = ShieldReason::None;
    LastPetPurpose = PetPurpose::None;
    PassiveStacks = 4;
    PassivePrimed = true;
    PassiveBuffConfirmed = false;
    PassiveLastObservedTick = PassiveLastConsumeTick =
        PassiveLastGainTick = Now();
    QPending = QMissileObserved = false;
    QCastTick = QImpactTick = QTargetId = QMissileNetworkId = 0;
    QOriginalStacks = 0;
    QWasManual = QExpectedStun = QExpectedShieldBreak = false;
    QOrigin = QLastPosition = {};
    LastQPlan = {};
    WPending = false;
    WCastTick = WResolveTick = WPrimaryId = WCastStartingStacks = 0;
    WWasManual = WExpectedStun = false;
    WCastOrigin = WAim = {};
    LastConePlan = {};
    RPending = false;
    RCastTick = RResolveTick = RPrimaryId = RCastStartingStacks = 0;
    RWasManual = RExpectedStun = false;
    LastSummonPlan = {};
    TibbersActive = false;
    TibbersNetworkId = TibbersSpawnTick = TibbersExpireTick = 0;
    TibbersEnrageUntil = TibbersEnrageAttacks = 0;
    TibbersLastOrderTick = TibbersLastTargetId = 0;
    ManualPetLockUntil = ControllerPetOrderUntil = 0;
    TibbersLastPosition = {};
    ProtectedAllyId = PeelThreatId = 0;
    TargetedAllyThreatId = TargetedAllyThreatUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEnd = {};
    InterruptTargetId = InterruptExpireTick = 0;
    LastAutoTargetId = LastAutoTick = 0;
    LastAfterAttackTargetId = LastAfterAttackTick = 0;
    ManualFlashUntil = ManualFlashTick = 0;
    IncomingThreats.fill({});
    RefreshPassiveFromBuff();
    RefreshTibbers();
}

inline void OnUnload() {
    TacticsMenu = PassiveMenu = DisintegrateMenu = IncinerateMenu = nullptr;
    ShieldMenu = TibbersMenu = PetMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Use Riot 26.4 Q base damage rather than the obsolete local port values",
    "Use Riot 25.18 W base damage and Tibbers magic penetration revision",
    "Use Riot 25.8 sustained Tibbers damage redistribution",
    "Use CommunityDragon 16.14 cast range, delay, speed and radius payloads",
    "Reject the old OneKeyToWin W pseudo-line geometry",
    "Reject the old OneKeyToWin static R-W-Q priority list",
    "Reject old damage values when a current Riot patch overrides them",
    "Treat Meraki and wiki numeric disagreements as secondary to Riot and CDragon",
    "Start and respawn with four Pyromania stacks",
    "Track Pyromania primed particle as confirmed four-stack state",
    "Track ordinary Pyromania stack-count buff events when count is exposed",
    "Keep predicted and buff-confirmed passive state distinguishable",
    "Gain a Pyromania stack from W on cast rather than on damage",
    "Gain a Pyromania stack from E on cast with no cast time",
    "Gain a Pyromania stack from R on cast before summon damage",
    "Gain a Pyromania stack from Q only when its missile hits",
    "At three stacks let an ordinary Q hit prime four without stunning",
    "At three stacks cast Q then E before impact to make Q stun invisibly",
    "Do not hidden-prime Q into an active spell shield",
    "Do not claim a hidden Q stun when E cannot resolve before impact",
    "At two stacks model Q, E and W as a real landing-order race",
    "At two stacks let Q consume when it lands before the W cone",
    "At two stacks let W consume when its cone resolves before Q",
    "After W consumes, let the later Q rebuild exactly one stack",
    "Choose the first primed damaging spell to land as the stun consumer",
    "Never let E consume Pyromania because E is non-damaging",
    "Do not let auto attacks consume Pyromania",
    "Consume primed Pyromania when a spell shield blocks its stun",
    "Still gain a non-primed Q stack when a spell shield blocks the spell",
    "Do not spend a primed spell into immunity or untargetability",
    "Reconcile a predicted passive consume when the primed particle disappears",
    "Keep three stacks as concealed threat while safely out of combat",
    "Build passive only from zero through three when no enemy is nearby",
    "Do not automatically reveal the fourth stack merely to stack E",
    "Override three-stack concealment for genuinely lethal incoming damage",
    "Override three-stack concealment for incoming hard crowd control on an ally",
    "Reserve visible stun during harmless harass when the target has not committed",
    "Release visible stun when a target walks into point-click catch range",
    "Reserve AoE stun for R when its scored circle materially beats W",
    "Reserve AoE stun for W when its cone hits the important set",
    "Prevent a flying Q from accidentally stealing an intended R stun",
    "Prevent a flying Q from accidentally stealing an intended W stun",
    "Allow R or W to win the race when it resolves before flying Q",
    "Clear stale landing races after their real impact window",
    "Expose passive stack, primed confirmation and reserved consumer to the player",
    "Use current Q range of 625 plus target gameplay radius",
    "Use current Q cast time of 0.25 seconds",
    "Use current Q missile speed of 1400",
    "Calculate Q impact from cast time plus remaining travel",
    "Track the real allied Q missile when its lifecycle event appears",
    "Refresh Q position from the live missile collection",
    "Track a manual Q even when its missile callback arrives late",
    "Resolve Q from missile deletion near its target",
    "Reject a Q destroyed far from its target by a projectile wall",
    "Reject Q paths through Yasuo Wind Wall",
    "Reject Q paths through Samira Blade Whirl",
    "Reject Q paths through Mel projectile reflection wall",
    "Use current Q damage of 80 to 260 plus 80 percent AP",
    "Use Q health prediction for lane last hits",
    "Refund the exact Q mana cost only on a predicted kill",
    "Halve Q cooldown only on a kill",
    "Do not call a nonlethal Q mana-neutral",
    "Prefer the narrowest positive Q last-hit margin",
    "Preserve visible stun instead of Q farming while an enemy champion is near",
    "Allow Q refund farming without a high ordinary mana threshold",
    "Use Q point-click stun for a single committed carry",
    "Use Q to follow a dash or Flash when an AoE spell would miss",
    "Respect enemy Flash readiness before throwing a long-range telegraphed Q",
    "Use Q as anti-gapcloser only when Pyromania will be primed at impact",
    "Use Q to interrupt only when impact arrives before channel expiry",
    "Use Q to peel the protected ally without inventing movement",
    "Use lethal Q automatically only after shield, immunity and wall checks",
    "Preserve player auto windup before ordinary Q",
    "Permit a reactive Q to bypass ordinary humanizer delay",
    "Continue a player-cast Q with the same landing-order state machine",
    "Use current W range of 600",
    "Use current W full cone angle rather than a line width",
    "Include target gameplay radius at the W angular edge",
    "Allow W to clip a target whose circle overlaps the cone apex behind Annie",
    "Reject a target fully detached behind the W apex",
    "Resolve W from Annie position at the end of cast time",
    "Recompute W origin after a player Flash before the resolve tick",
    "Keep original world aim while recomputing post-Flash cone direction",
    "Never cast Flash automatically for W-Flash",
    "Recognize a player Flash during a pending W buffer",
    "Continue burst only after the player-owned W-Flash resolves",
    "Use current W damage of 70 to 230 plus 80 percent AP",
    "Score W directions from predicted enemy positions",
    "Evaluate pair-midpoint W directions for multi-target cones",
    "Reward W directions containing the selected target",
    "Reward W directions containing dashing high-priority targets",
    "Require the selected target in an ordinary proactive W",
    "Use W as AoE stun when its hit count meets the configured threshold",
    "Use W before Q when the fast cone must stop a close dash",
    "Use Q before W when point-click follow is safer",
    "Use W to interrupt a channel already inside the cone",
    "Use W to peel multiple divers from a protected ally",
    "Use W for harass only when explicitly enabled and multi-target",
    "Use W on a large predicted lane wave rather than one minion",
    "Use W on valuable jungle units as an exception to raw hit count",
    "Do not W farm with visible stun while a champion is near",
    "Draw W cone from its actual prospective resolve origin",
    "Use current E ally cast range of 800 plus gameplay radius",
    "Treat E as no-cast-time for hidden Q priming",
    "Use current E shield of 60 to 200 plus 40 percent AP",
    "Use current E reaction damage of 25 to 65 plus 40 percent AP",
    "Remember E reaction damage is once per enemy per shield",
    "Use current E movement speed scaling from 20 to 50 percent by level",
    "Model E movement speed decay over 1.5 seconds",
    "Shield the ally directly targeted by an enemy spell",
    "Shield an ally whose gameplay circle intersects a hostile spell line",
    "Estimate targeted missile impact from cast delay and travel speed",
    "Rank incoming shields by damage, hard CC, lethality and time to impact",
    "Shield before impact rather than after health is lost",
    "Prefer the current protected ally when threat value is otherwise equal",
    "Use E on self around a player-led short auto trade",
    "Use E for level-one E-auto-auto cooperation without issuing attacks",
    "Use E for level-two E-Q-auto cooperation without issuing attacks",
    "Use E to speed the endangered ally away from a diver",
    "Use E self shield and decaying speed during flee mode",
    "Let emergency E break an offensive stack reservation",
    "Reject noncritical E when preserving a planned three-stack AoE sequence",
    "Choose a threatened ally as the hidden-prime E target when in range",
    "Otherwise use self E for a deterministic hidden Q prime",
    "Remember that E also shields Tibbers whenever Tibbers is active",
    "Use E stack building only at safe mana and out of combat",
    "Do not E stack while recalling",
    "Do not waste E stack building merely to support a critically low Tibbers",
    "Expose the last E tactical reason to the player",
    "Use current R cast range of 600",
    "Use current R initial hit radius of 250 plus gameplay radius",
    "Use current R damage of 150 to 400 plus 75 percent AP",
    "Use current R passive magic penetration ranks of 10, 15 and 20 in live damage",
    "Score R at current and predicted target positions",
    "Evaluate pair-midpoint R centers for multi-target catches",
    "Clamp an R candidate to real cast range",
    "Reward R circles containing the selected target",
    "Reward R circles containing the protected ally's diver",
    "Reward dashing and high-priority targets in R scoring",
    "Penalize wasting primed R into a spell shield",
    "Require multiple targets for an ordinary proactive R stun",
    "Allow single-target R when full conservative burst and summon value justify it",
    "Use allied follow-up as part of non-stun R commitment",
    "Do not use R merely because one enemy is low",
    "Keep automatic R kill-secure disabled by default",
    "If enabled, require multi-hit or a near-certain execute for R secure",
    "Use R interrupt only when Q and W cannot deliver better value",
    "Use R anti-gapcloser only for a critical point-blank threat",
    "Use R peel when the ally is critical or multiple enemies occupy the circle",
    "Use R flee only as last-resort primed disengage",
    "Never cast Flash automatically for R-Flash",
    "Recognize a player Flash immediately before a manual R resolve",
    "Continue the burst after a player-owned R-Flash",
    "Track a manual R placement even without a selected target",
    "Distinguish first R summon from R pet-control commands",
    "Do not add a passive stack for a Tibbers control recast",
    "Spawn Tibbers state at R resolve even when object callback is delayed",
    "Reconcile Tibbers state from the allied pet collection",
    "Track Tibbers by allied annietibbers object lifecycle",
    "Use the current 45-second Tibbers lifetime",
    "Enrage Tibbers immediately on summon",
    "Enrage Tibbers whenever Annie stuns an enemy champion",
    "Track the current three-second ordinary enrage window",
    "Track the current ten-second death enrage when exposed by live buff duration",
    "Model the five enrage attacks separately from the time window",
    "Model Tibbers' five live enrage attack-speed stages from 1.736 to 0.739",
    "Track Tibbers auto attacks from pet do-cast events",
    "Use current Tibbers aura damage of 8 to 16 plus 4 percent AP per second",
    "Preserve Tibbers quarter-second aura tick cadence",
    "Use current Tibbers attack damage of 30 to 60 plus 10 percent AP",
    "Include conservative aura contact and one attack in summon commitment",
    "Focus the selected target while Tibbers is enraged",
    "Redirect Tibbers to a diver threatening the protected ally",
    "Do not chase a target under an enemy turret by default",
    "Allow optional Tibbers-only turret pursuit only below configured target HP",
    "Force Tibbers out of turret range when pet dive is disabled",
    "Return Tibbers when its health becomes critical",
    "Return Tibbers at the critical owner-distance leash",
    "Use a softer return threshold when no valid combat target exists",
    "Throttle repeated pet orders to avoid fighting player input",
    "Lock soft autopilot after every observed manual R pet command",
    "Mark controller pet orders so their callbacks do not look manual",
    "Acknowledge that SDK cannot observe every player Alt-click",
    "Never claim perfect manual pet arbitration without an inbound order event",
    "Let the player disable soft Tibbers autopilot entirely",
    "Use Tibbers on the current jungle objective when safe",
    "Use Tibbers on the current wave without moving Annie",
    "Do not invent warding movement for Tibbers without a player zone request",
    "Draw Tibbers enrage, current target and remaining lifetime",
    "Protect a targeted ally before beginning Annie burst",
    "Select the protected ally by offense, range, health and live threat",
    "Select a peel threat by proximity, approach path, dash and offense",
    "Use E before damage when the protected ally needs immediate survival",
    "Use W before Q for multi-diver peel",
    "Use Q before R for a single point-click peel",
    "Use R only for critical or multi-target peel",
    "Handle gapcloser callback facts through the shared neutral adapter",
    "Handle interrupt callback facts through the shared neutral adapter",
    "Expire stale gapcloser, interrupt and incoming-threat state",
    "Use a zero-delay reactive cast path without removing ordinary humanization",
    "Preserve player auto windups during ordinary combo decisions",
    "Never issue movement for Annie",
    "Never issue attack-move for Annie",
    "Never select or cast a summoner spell automatically",
    "Respect the player's selected target through the shared engine",
    "Require cursor agreement only where player direction materially disambiguates intent",
    "Continue a manual Q with E hidden-prime when timing remains valid",
    "Continue a manual W-Flash with Q and burst after resolve",
    "Continue a manual R-Flash with W and Q after resolve",
    "Let manual spell input reset the generic plan while preserving champion state",
    "Use Q-E-auto as a pre-six short-trade family",
    "Use E-auto-auto at level one only through player attack cooperation",
    "Use W-Q-auto after an AoE cone stun",
    "Use R-W-Q-auto after a multi-target summon stun",
    "Use Q-E-W as a two-stack landing race rather than fixed button spam",
    "Use W-E at two stacks to prepare a cone stun when Q is unavailable",
    "Use E-R at two stacks to prepare an AoE summon stun",
    "At three stacks let R's cast stack prime its own damage stun",
    "At three stacks let W's cast stack prime its own damage stun",
    "At three stacks let Q require E before impact to stun",
    "At four stacks choose R for the valuable AoE set",
    "At four stacks choose W for a close multi-target set",
    "At four stacks choose Q for the reliable single carry catch",
    "Do not overwrite a productive flying-Q sequence with redundant crowd control",
    "Wait for a pending stun to resolve before ordinary burst follow-up",
    "Use W follow-up when target control or commitment makes the cone reliable",
    "Use Q follow-up when point-click damage is lethal or target remains committed",
    "Use self E after a player auto when it adds shield and reaction value",
    "Freestyle from live stack, landing and pet state instead of Q-W-E-R order",
    "Use Q refund on lane minions only with positive health prediction",
    "Use Q refund on jungle monsters only when predicted lethal",
    "Use W lane clear only above the configured cone count",
    "Use W jungle clear on a valuable monster even below ordinary lane count",
    "Infer jungle clear from nearby neutral monsters in LaneClear mode",
    "Keep separate lane and jungle mana floors",
    "Never spend R to clear an ordinary wave automatically",
    "Never invent pathing between minions or jungle camps",
    "Expose posture, sequence, passive, Q race, E reason and Tibbers purpose",
    "Never fall back to generic spell ordering because this controller owns the decision loop",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Annie;
    controller.ControllerId = "champion.kuroaio.ai.annie.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIAnnie.md";
    controller.ImplementationSummary =
        "Eleven-posture burst/catch controller with cast-versus-impact "
        "Pyromania simulation, hidden three-stack Q-E and two-stack landing "
        "races, projectile-wall-aware refund Q, cast-end W/Flash cone geometry, "
        "impact-timed ally E protection, scored R stun selection, and manual-"
        "locking turret/leash/enrage-aware soft Tibbers micro.";
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
            &ObserveLocalSpell, &RecordIncomingThreats>;
    controller.OnDoCast = &OnDoCast;
    controller.OnBuffAdd =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnBuffRemove =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;
    controller.OnBuffUpdate =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnAfterAttack =
        &ControllerHelpers::CaptureAfterAttackEvent<
            &LastAfterAttackTargetId, &LastAfterAttackTick>;
    controller.OnGapcloser =
        &ControllerHelpers::CaptureGapcloserEvent<
            &GapcloserTargetId, &GapcloserEnd,
            &GapcloserExpireTick, 520, 760>;
    controller.OnInterruptable =
        &ControllerHelpers::CaptureInterruptableEvent<
            &InterruptTargetId, &InterruptExpireTick, 1100, 250, 5000>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Annie
