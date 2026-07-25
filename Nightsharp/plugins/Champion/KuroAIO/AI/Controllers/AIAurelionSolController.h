#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIAurelionSolGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::AurelionSol {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::CursorDirectionAgrees;
using ControllerHelpers::HasAnyBuff;
using ControllerHelpers::HasCurrentResource;
using ControllerHelpers::HasEnemyChampionNear;
using ControllerHelpers::HasNearbyJungleTarget;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLargeLaneMinion;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MaximumBuffCount;
using ControllerHelpers::NearTerrain;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PredictionAtLeast;
using ControllerHelpers::ProjectileWallFirstContactFromPlayer;
using ControllerHelpers::Ready;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContainsAny;
using ControllerHelpers::SpellRank;
using ControllerHelpers::UnitByNetworkId;

enum class Sequence : std::uint8_t {
    None,
    LaneBurst,
    SplashTap,
    SingularityBreath,
    SingularityStarBreath,
    FlightBreath,
    FlightSingularityBreath,
    FlightSingularityStarBreath,
    FlightResetExit,
    SelfPeel,
    AllyPeel,
    Interrupt,
    ObjectiveCalamity,
    CannonHarvest,
    JungleHarvest,
    PlayerLed,
};

enum class Posture : std::uint8_t {
    Neutral,
    LanePressure,
    FrontToBack,
    FlightAngle,
    Zone,
    Peel,
    Objective,
    Siege,
    Farm,
    Flee,
};

enum class QPurpose : std::uint8_t {
    None,
    Manual,
    LaneBurst,
    SplashTap,
    FlightContinue,
    EContact,
    RContact,
    Lethal,
    Peel,
    Wave,
    Jungle,
    Objective,
};

enum class WPurpose : std::uint8_t {
    None,
    Manual,
    PunishCooldown,
    ContinueBreath,
    OffsetAllIn,
    Chase,
    ResetReposition,
    ResetEscape,
    Flee,
};

enum class EPurpose : std::uint8_t {
    None,
    Manual,
    LaneSetup,
    FlightDrop,
    RSetup,
    Peel,
    Choke,
    Wave,
    CannonWave,
    Jungle,
    Objective,
};

enum class RPurpose : std::uint8_t {
    None,
    Manual,
    ELock,
    Teamfight,
    Calamity,
    SelfPeel,
    AllyPeel,
    Interrupt,
    AntiGapcloser,
    KillSecure,
    Objective,
};

struct QPlan {
    Vector3 Aim = {};
    int RequestedTargetId = 0;
    int FirstBodyId = 0;
    QPurpose Purpose = QPurpose::None;
    float ExpectedContactSeconds = 0.0f;
    float Alignment = 0.0f;
    float Score = -FLT_MAX;
    bool Splash = false;
    bool Valid = false;
};

struct WPlan {
    Vector3 Destination = {};
    Vector3 Direction = {};
    WPurpose Purpose = WPurpose::None;
    int TargetId = 0;
    float Score = -FLT_MAX;
    bool EscapeRoute = false;
    bool TakedownReset = false;
    bool Valid = false;
};

struct EPlan {
    Vector3 Center = {};
    SingularityEvaluation Evaluation = {};
    EPurpose Purpose = EPurpose::None;
    int TargetId = 0;
    float Score = -FLT_MAX;
    bool Valid = false;
};

struct RPlan {
    Vector3 RequestedCenter = {};
    Vector3 ImpactCenter = {};
    UltimateEvaluation Evaluation = {};
    RPurpose Purpose = RPurpose::None;
    int TargetId = 0;
    int ImpactTick = 0;
    float ImpactSeconds = 0.0f;
    float Score = -FLT_MAX;
    bool Empowered = false;
    bool ProjectileIntercepted = false;
    bool Manual = false;
    bool Valid = false;
};

struct EnemyWindow {
    int NetworkId = 0;
    int CommittedUntil = 0;
    int HardCrowdControlSpentUntil = 0;
    int LastSpellSlot = -1;
};

inline Menu* TacticsMenu = nullptr;
inline Menu* BreathMenu = nullptr;
inline Menu* FlightMenu = nullptr;
inline Menu* SingularityMenu = nullptr;
inline Menu* StarMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Sequence ActiveSequence = Sequence::None;
inline Posture CurrentPosture = Posture::Neutral;
inline QPurpose LastQPurpose = QPurpose::None;
inline WPurpose LastWPurpose = WPurpose::None;
inline EPurpose LastEPurpose = EPurpose::None;
inline RPurpose LastRPurpose = RPurpose::None;
inline Mode LastKnownMode = Mode::None;

inline int ObservedStardust = 0;
inline int PredictedStardust = 0;
inline bool StardustTelemetryReliable = false;
inline int LastStardustObservationTick = 0;
inline CalamityState Calamity = {};
inline bool CalamityTelemetryReliable = false;

inline bool QActive = false;
inline bool QControllerOwned = false;
inline int QCastTick = 0;
inline int QLastObservedTick = 0;
inline int QTargetId = 0;
inline int QFirstBodyId = 0;
inline int QNoContactSince = 0;
inline int QLastUpdateTick = 0;
inline QContactState QContact = {};

inline bool WActive = false;
inline bool WControllerOwned = false;
inline int WCastTick = 0;
inline int WLastObservedTick = 0;
inline int WTargetId = 0;
inline Vector3 WDestination = {};
inline int LastWReadyTick = 0;
inline int LastWTakedownResetTick = 0;
inline float LastWCooldownSeconds = 0.0f;
inline int LastWCooldownSampleTick = 0;
inline bool WCooldownObserved = false;
inline int LastChampionContactTick = 0;

inline bool EActive = false;
inline bool EControllerOwned = false;
inline int ECastTick = 0;
inline int EExpireTick = 0;
inline int ETargetId = 0;
inline Vector3 ECenter = {};

inline int RCastTick = 0;
inline int RImpactTick = 0;
inline Vector3 RImpactCenter = {};
inline bool LastRWasEmpowered = false;
inline bool RStardustResolutionPending = false;

inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int IncomingThreatTargetId = 0;
inline int IncomingThreatUntil = 0;
inline bool IncomingHardCrowdControl = false;
inline int ProtectedAllyId = 0;
inline int PeelThreatId = 0;

inline std::array<EnemyWindow, 10> EnemyWindows = {};
inline QPlan LastQPlan = {};
inline WPlan LastWPlan = {};
inline EPlan LastEPlan = {};
inline RPlan LastRPlan = {};

inline int Stardust() {
    return std::max(ObservedStardust, PredictedStardust);
}

inline float TargetPriority(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const float offense = target.TotalAttackDamage() * 0.0044f +
                         target.AP() * 0.0040f;
    const float range = std::clamp(
        target.AttackRange() / 700.0f, 0.0f, 1.4f);
    const float wounded = (100.0f - target.HealthPercent()) * 0.014f;
    return 0.72f + offense + range + wounded;
}

inline Vector3 ClampCastPosition(const Vector3& requested, float range) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !requested.IsValid() || requested.IsZero()) {
        return {};
    }
    const float distance = player.Position().Distance2D(requested);
    return distance <= range
        ? requested
        : Engine::Extend(player.Position(), requested, range);
}

inline bool IsQEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::Q, {
            "aurelionsolq", "breathoflight",
        });
}

inline bool IsWEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::W, {
            "aurelionsolw", "astralflight",
        });
}

inline bool IsEEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::E, {
            "aurelionsole", "singularity",
        });
}

inline bool IsREvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::R, {
            "aurelionsolr", "fallingstar", "skiesdescend",
        });
}

inline bool IsQBuff(const char* name) {
    return ControllerHelpers::TextContainsAny(name, {
        "aurelionsolq", "breathoflight",
    });
}

inline bool IsWBuff(const char* name) {
    return ControllerHelpers::TextContainsAny(name, {
        "aurelionsolw", "astralflight",
    });
}

inline bool IsStardustBuff(const char* name) {
    return ControllerHelpers::TextContainsAny(name, {
        "aurelionsolpassive", "aurelionsolstardust",
        "stardustcounter", "c9372c6b",
    });
}

inline bool IsCalamityBuff(const char* name) {
    return ControllerHelpers::TextContainsAny(name, {
        "aurelionsolr2", "aurelionsolrready",
        "skiesdescend", "calamity",
    });
}

inline bool RuntimeEmpoweredR() {
    const auto player = GameObjects::Player();
    return RuntimeNameContains(3, "aurelionsolr2") ||
           RuntimeNameContains(3, "skiesdescend") ||
           (player.IsValid() && HasAnyBuff(player, {
               "AurelionSolR2", "AurelionSolRReady",
               "AurelionSolR2Ready", "AurelionSolCalamity",
           }));
}

inline EnemyWindow* FindEnemyWindow(int networkId, bool create = false) {
    return ControllerHelpers::FindEnemyCastWindow(
        EnemyWindows, networkId, create);
}

inline void AddPredictedStardust(int amount) {
    if (amount <= 0) return;
    PredictedStardust = std::max(0, PredictedStardust + amount);
    Calamity = AdvanceCalamity(Calamity, amount, false);
}

inline void ObserveStardustCount(int count) {
    if (count < 0 || count > 10000) return;
    const int old = Stardust();
    ObservedStardust = count;
    PredictedStardust = std::max(PredictedStardust, count);
    StardustTelemetryReliable = true;
    LastStardustObservationTick = Now();
    if (count > old) Calamity = AdvanceCalamity(
        Calamity, count - old, false);
}

inline void RefreshStardustTelemetry() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int buffCount = MaximumBuffCount(player, {
        "AurelionSolPassive", "AurelionSolStardust",
        "AurelionSolPassiveStack", "AurelionSolStardustCounter",
        "AurelionSolPassiveManager",
    });
    if (buffCount > 0) ObserveStardustCount(buffCount);

    // A few builds replicate the passive counter as an auxiliary spell ammo
    // entry. Never treat Q/W/E/R charges as Stardust: require passive/icon
    // vocabulary and a sane unbounded-looking maximum.
    for (const auto& spell : player.Spellbook().Spells()) {
        if (!spell.IsValid() || !ControllerHelpers::AnyTextContains(
                { spell.Name().c_str(), spell.ScriptName().c_str(),
                  spell.IconName().c_str() },
                { "aurelionsolp", "stardust", "cosmiccreator" })) {
            continue;
        }
        const int ammo = spell.Ammo();
        if (ammo >= 0 && ammo <= 10000) ObserveStardustCount(ammo);
    }

    const auto ultimate = player.Spellbook().GetSpell(SDK::SpellSlot::R);
    if (ultimate.IsValid() && ultimate.MaxAmmo() == kRUpgradeRequirement &&
        ultimate.Ammo() >= 0 && ultimate.Ammo() <= kRUpgradeRequirement) {
        Calamity.Progress = ultimate.Ammo();
        Calamity.Ready = Calamity.Progress >= kRUpgradeRequirement;
        CalamityTelemetryReliable = true;
    }
    if (RuntimeEmpoweredR()) {
        Calamity.Progress = kRUpgradeRequirement;
        Calamity.Ready = true;
        CalamityTelemetryReliable = true;
    }
}

inline bool RuntimeQActive() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    return HasAnyBuff(player, {
               "AurelionSolQ", "AurelionSolQChannel",
               "AurelionSolQBuff", "AurelionSolQManager",
           }) ||
           ((Now() - QCastTick <= 4200 || WActive) &&
            (player.Spellbook().IsChanneling() ||
             player.Spellbook().IsCharging()));
}

inline bool RuntimeWActive() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
        (HasAnyBuff(player, {
             "AurelionSolW", "AurelionSolWFlight",
             "AurelionSolWBuff", "AurelionSolWActive",
         }) || RuntimeNameContains(1, "toggle") ||
         RuntimeNameContains(1, "recast"));
}

inline void ClearQState() {
    QActive = false;
    QControllerOwned = false;
    QCastTick = QLastObservedTick = 0;
    QTargetId = QFirstBodyId = 0;
    QNoContactSince = QLastUpdateTick = 0;
    QContact = {};
}

inline void ClearWState() {
    WActive = false;
    WControllerOwned = false;
    WCastTick = WLastObservedTick = 0;
    WTargetId = 0;
    WDestination = {};
}

inline EUnitKind ClassifyEUnit(const AIBaseClient& unit) {
    if (unit.IsHero()) return EUnitKind::Champion;
    const AIMinionClient minion(unit.Handle());
    if (!minion.IsValid()) return EUnitKind::SmallMinion;
    if (minion.IsJungle()) {
        const JungleType type = minion.GetJungleType();
        if (type == JungleType::Epic || type == JungleType::Legendary) {
            return EUnitKind::EpicMonster;
        }
        return type == JungleType::Large
            ? EUnitKind::LargeMonster
            : EUnitKind::SmallMonster;
    }
    return IsLargeLaneMinion(minion)
        ? EUnitKind::LargeMinion
        : EUnitKind::SmallMinion;
}

inline float QOneBurstDamage(const AIBaseClient& target,
                             bool flying = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const bool monster = target.IsMinion() &&
        AIMinionClient(target.Handle()).IsJungle();
    float raw = QDamagePerSecond(SpellRank(0), player.AP()) +
        QBurstRawDamage(SpellRank(0), player.AP(), Stardust(),
                        target.MaxHealth(), monster);
    if (flying) raw *= WQDamageMultiplier(SpellRank(1));
    return player.CalculateMagicDamage(target, raw);
}

inline float ETotalDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return player.CalculateMagicDamage(
        target, SingularityDamagePerSecond(
            SpellRank(2), player.AP()) * kEDurationSeconds);
}

inline float RDirectDamage(const AIBaseClient& target, bool empowered) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const float raw = empowered
        ? SkiesDescendDirectRawDamage(SpellRank(3), player.AP())
        : FallingStarRawDamage(SpellRank(3), player.AP());
    return player.CalculateMagicDamage(target, raw);
}

inline std::vector<BeamUnit> BuildBeamUnits(float delaySeconds) {
    std::vector<BeamUnit> result;
    result.reserve(48);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        result.push_back({ PredictPosition(enemy, delaySeconds),
                           enemy.BoundingRadius(),
                           static_cast<int>(enemy.NetworkId()),
                           true, true });
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!minion.IsValid() || minion.IsDead() ||
            !minion.IsTargetable() ||
            HasFlag(minion.GetMinionType(), MinionTypes::Ward) ||
            minion.IsPlant()) {
            continue;
        }
        result.push_back({ PredictPosition(minion, delaySeconds),
                           minion.BoundingRadius(),
                           static_cast<int>(minion.NetworkId()),
                           false, true });
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (!monster.IsValid() || monster.IsDead() ||
            !monster.IsTargetable() || monster.IsPlant()) {
            continue;
        }
        result.push_back({ PredictPosition(monster, delaySeconds),
                           monster.BoundingRadius(),
                           static_cast<int>(monster.NetworkId()),
                           false, true });
    }
    return result;
}

inline int FirstBeamBodyAt(const Vector3& origin,
                           const Vector3& toward,
                           float range,
                           float delaySeconds,
                           BeamUnit* first = nullptr) {
    const Vector3 direction = SharedGeometry::Direction2D(origin, toward);
    if (direction.IsZero()) return 0;
    const auto units = BuildBeamUnits(delaySeconds);
    const int index = FirstBeamCollisionIndex(
        origin, direction, units, range);
    if (index < 0) return 0;
    if (first) *first = units[static_cast<std::size_t>(index)];
    return units[static_cast<std::size_t>(index)].Id;
}

inline float CursorAlignment(const Vector3& destination) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return -1.0f;
    const Vector3 desired = SharedGeometry::Direction2D(
        player.Position(), destination);
    const Vector3 cursor = SharedGeometry::Direction2D(
        player.Position(), Game::CursorPos());
    return desired.IsZero() || cursor.IsZero()
        ? 1.0f : desired.Dot(cursor);
}

inline bool UnsafeStationaryChannel(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return true;
    const int enemies = Engine::CountEnemiesAt(player.Position(), 390.0f);
    const int allies = Engine::CountAlliesAt(player.Position(), 650.0f);
    return enemies > allies + 1 ||
        (target.IsValid() && !Engine::IsHardCrowdControlled(target) &&
         player.Position().Distance2D(target.Position()) <= 285.0f);
}

inline QPlan BuildQPlan(const AIBaseClient& target,
                        QPurpose purpose,
                        bool allowSplash) {
    QPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid() || target.IsDead() ||
        !target.IsTargetable() || SpellRank(0) <= 0) {
        return best;
    }
    const float range = QRange(player.Level());
    const Vector3 predicted = PredictPosition(target, 0.08f);
    if (player.Position().Distance2D(predicted) >
        range + target.BoundingRadius()) {
        return best;
    }
    const Vector3 baseDirection = SharedGeometry::Direction2D(
        player.Position(), predicted);
    if (baseDirection.IsZero()) return best;
    static constexpr std::array<float, 9> angles = {
        0.0f, -2.5f, 2.5f, -5.0f, 5.0f,
        -7.5f, 7.5f, -10.0f, 10.0f,
    };
    const auto units = BuildBeamUnits(0.08f);
    const int requestedId = static_cast<int>(target.NetworkId());
    for (float degrees : angles) {
        const Vector3 direction = SharedGeometry::Rotate2D(
            baseDirection, degrees * kPi / 180.0f);
        const int index = FirstBeamCollisionIndex(
            player.Position(), direction, units, range);
        if (index < 0) continue;
        const BeamUnit& first = units[static_cast<std::size_t>(index)];
        const bool direct = first.Id == requestedId;
        bool splash = false;
        if (!direct && allowSplash) {
            BeamUnit requested{ predicted, target.BoundingRadius(),
                                requestedId, target.IsHero(), true };
            splash = QSplashHits(first, requested);
        }
        if (!direct && !splash) continue;
        const Vector3 aim = player.Position() + direction * range;
        const float alignment = CursorAlignment(aim);
        float score = direct ? 640.0f : 245.0f;
        score += alignment * 180.0f;
        score -= std::abs(degrees) * 8.0f;
        score += target.IsHero()
            ? TargetPriority(AIHeroClient(target.Handle())) * 125.0f
            : std::clamp(target.MaxHealth() / 1000.0f, 0.0f, 3.0f) * 55.0f;
        if (Engine::IsHardCrowdControlled(target)) score += 210.0f;
        if (target.IsDashing()) score += 80.0f;
        if (QOneBurstDamage(target, WActive) >=
            target.Health() + target.AllShield()) score += 420.0f;
        if (score > best.Score) {
            best.Aim = aim;
            best.RequestedTargetId = requestedId;
            best.FirstBodyId = first.Id;
            best.Purpose = splash ? QPurpose::SplashTap : purpose;
            best.ExpectedContactSeconds = direct
                ? (Engine::IsHardCrowdControlled(target) ? 1.45f :
                   (target.IsDashing() ? 1.10f : 0.92f))
                : 0.14f;
            best.Alignment = alignment;
            best.Score = score;
            best.Splash = splash;
            best.Valid = true;
        }
    }
    return best;
}

inline bool CastQPlan(const QPlan& plan, Mode mode, bool reactive = false) {
    if (!plan.Valid || QActive || !Ready(0) ||
        !SpellEnabled(0, mode) || !CastThrottleReady(0, reactive)) {
        return false;
    }
    const auto player = GameObjects::Player();
    const AIBaseClient target = UnitByNetworkId(plan.RequestedTargetId);
    if (!player.IsValid() || !target.IsValid()) return false;
    const float intendedSeconds = plan.Splash
        ? 0.14f : std::max(0.85f, plan.ExpectedContactSeconds);
    QStartContext context{};
    context.TargetValid = true;
    context.RequestedTargetFirst = plan.FirstBodyId ==
        plan.RequestedTargetId || plan.Splash;
    context.CursorAgrees = plan.Alignment >=
        (plan.Splash ? -0.20f : -0.08f);
    context.PlayerAttackWindup = Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true);
    context.IncomingHardCrowdControl = IncomingHardCrowdControl &&
        IncomingThreatUntil >= Now();
    context.UnsafeMelee = UnsafeStationaryChannel(target) && !WActive;
    context.PlayerRecentlyCast = Engine::LastManualSpellTick > 0 &&
        Now() - Engine::LastManualSpellTick < 180;
    context.Flying = WActive;
    context.TargetControlled = Engine::IsHardCrowdControlled(target);
    context.ExpectedContactSeconds = plan.Splash
        ? 1.0f : plan.ExpectedContactSeconds;
    context.AvailableMana = ControllerHelpers::CurrentResource();
    context.RequiredMana = QExpectedManaCost(
        SpellRank(0), intendedSeconds) +
        static_cast<float>(Slider(BreathMenu, "FlatReserve", 45));
    if (!ShouldStartQ(context)) return false;
    if (!Engine::ControllerCastPosition(0, plan.Aim)) return false;
    QActive = true;
    QControllerOwned = true;
    QCastTick = QLastObservedTick = QLastUpdateTick = Now();
    QTargetId = plan.RequestedTargetId;
    QFirstBodyId = plan.FirstBodyId;
    QNoContactSince = 0;
    QContact = {};
    LastQPlan = plan;
    LastQPurpose = plan.Purpose;
    ActiveSequence = plan.Splash
        ? Sequence::SplashTap
        : (WActive ? Sequence::FlightBreath : Sequence::LaneBurst);
    return true;
}

inline void UpdateQContactTelemetry() {
    if (!QActive) return;
    const int now = Now();
    const float elapsed = QLastUpdateTick > 0
        ? static_cast<float>(std::clamp(now - QLastUpdateTick, 0, 120)) /
              1000.0f
        : 0.0f;
    QLastUpdateTick = now;
    const auto player = GameObjects::Player();
    const AIBaseClient target = UnitByNetworkId(QTargetId);
    if (!player.IsValid() || !target.IsValid()) {
        QContact = AdvanceQContact(QContact, 0, elapsed, false);
        return;
    }
    BeamUnit first{};
    const int firstId = FirstBeamBodyAt(
        player.Position(), Game::CursorPos(), QRange(player.Level()),
        0.04f, &first);
    QFirstBodyId = firstId;
    const bool touching = firstId == QTargetId;
    if (touching && target.IsHero()) LastChampionContactTick = now;
    if (!touching) {
        if (QNoContactSince == 0) QNoContactSince = now;
    } else {
        QNoContactSince = 0;
    }
    const int oldBursts = QContact.Bursts;
    QContact = AdvanceQContact(
        QContact, touching ? QTargetId : 0, elapsed, touching);
    if (QContact.Bursts > oldBursts && target.IsHero()) {
        AddPredictedStardust(
            QStardustFromBursts(QContact.Bursts - oldBursts));
    }
}

inline bool StopQ(bool emergency = false) {
    if (!QActive || !QControllerOwned ||
        !CastThrottleReady(0, true)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    ClearQState();
    if (emergency) ActiveSequence = Sequence::SelfPeel;
    return true;
}

inline bool ManageQ() {
    if (!QActive) return false;
    UpdateQContactTelemetry();
    const auto player = GameObjects::Player();
    const AIBaseClient target = UnitByNetworkId(QTargetId);
    const float channelSeconds = QCastTick > 0
        ? static_cast<float>(Now() - QCastTick) / 1000.0f : 0.0f;
    const float noContactSeconds = QNoContactSince > 0
        ? static_cast<float>(Now() - QNoContactSince) / 1000.0f : 0.0f;
    const float burstDue = std::max(
        0.0f, 1.0f - QContact.ContinuousSeconds);
    bool steeringAway = false;
    if (player.IsValid() && target.IsValid()) {
        steeringAway = CursorAlignment(target.Position()) < -0.32f;
    }
    QStopContext context{};
    context.ControllerOwned = QControllerOwned;
    context.TapPurpose = LastQPurpose == QPurpose::SplashTap;
    context.IncomingHardCrowdControl = IncomingHardCrowdControl &&
        IncomingThreatUntil >= Now();
    context.UnsafeMelee = target.IsValid() &&
        UnsafeStationaryChannel(target) && !WActive;
    context.PrimaryContact = QFirstBodyId == QTargetId;
    context.TargetAlive = target.IsValid() && !target.IsDead();
    context.ManaReserveBroken = ControllerHelpers::CurrentResource() <=
        static_cast<float>(Slider(BreathMenu, "FlatReserve", 45));
    context.PlayerSteeringAway = steeringAway;
    context.ChannelSeconds = channelSeconds;
    context.NoContactSeconds = noContactSeconds;
    context.BurstDueSeconds = burstDue;
    if (ShouldStopQ(context)) {
        return StopQ(context.IncomingHardCrowdControl ||
                     context.UnsafeMelee);
    }
    const float maximum = QMaximumChannelSeconds(
        SpellRank(0), WActive);
    if (QControllerOwned && channelSeconds >= maximum - 0.04f) {
        return StopQ(false);
    }
    return false;
}

inline bool ObjectiveActive(float range = 2100.0f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    for (const auto& monster : GameObjects::Jungle()) {
        if (monster.IsValid() && !monster.IsDead() &&
            monster.IsTargetable() && IsEpicMonster(monster) &&
            player.Position().Distance2D(monster.Position()) <= range) {
            return true;
        }
    }
    return false;
}

inline WPlan BuildWPlan(const AIHeroClient& target,
                        WPurpose purpose,
                        bool escape) {
    WPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) ||
        PlayerMobilityLocked() || SpellRank(1) <= 0) return best;
    const float maximumRange = WRange(Stardust());
    Vector3 baseDirection = escape
        ? SharedGeometry::Direction2D(target.Position(), player.Position())
        : SharedGeometry::Direction2D(player.Position(), target.Position());
    const Vector3 cursorDirection = SharedGeometry::Direction2D(
        player.Position(), Game::CursorPos());
    if (baseDirection.IsZero()) baseDirection = cursorDirection;
    if (baseDirection.IsZero()) return best;
    const float targetDistance = player.Position().Distance2D(target.Position());
    const float routeLength = escape
        ? std::min(maximumRange, 1050.0f)
        : std::min(maximumRange, std::clamp(
              targetDistance + 260.0f, 650.0f,
              static_cast<float>(Slider(FlightMenu, "MaximumRoute", 1350))));
    static constexpr std::array<float, 9> angles = {
        0.0f, -18.0f, 18.0f, -32.0f, 32.0f,
        -46.0f, 46.0f, -62.0f, 62.0f,
    };
    for (float degrees : angles) {
        Vector3 direction = SharedGeometry::Rotate2D(
            baseDirection, degrees * kPi / 180.0f);
        if (!cursorDirection.IsZero() && escape &&
            direction.Dot(cursorDirection) < -0.15f) continue;
        const Vector3 destination = player.Position() + direction * routeLength;
        FlightContext context{};
        context.Origin = player.Position();
        context.Destination = destination;
        context.Target = target.Position();
        context.QRange = QRange(player.Level());
        context.CursorDot = cursorDirection.IsZero()
            ? 1.0f : direction.Dot(cursorDirection);
        context.PlayerHealthPercent = player.HealthPercent();
        context.GroundedOrImmobilized = PlayerMobilityLocked();
        context.PlayerRecentlyCast = Engine::LastManualSpellTick > 0 &&
            Now() - Engine::LastManualSpellTick < 180;
        context.TargetValid = true;
        context.TakedownReset = purpose == WPurpose::ResetEscape ||
                                purpose == WPurpose::ResetReposition;
        context.EscapeRoute = escape;
        context.DirectDive = !escape &&
            destination.Distance2D(target.Position()) <= 300.0f;
        constexpr int sampleCount = 5;
        for (int sampleIndex = 1; sampleIndex <= sampleCount; ++sampleIndex) {
            const float fraction = static_cast<float>(sampleIndex) /
                                   static_cast<float>(sampleCount);
            const Vector3 position = player.Position() +
                direction * (routeLength * fraction);
            const float speed = WFlightSpeed(
                std::max(0.0f, player.MoveSpeed() - kWFlightBaseMoveSpeed),
                QActive || Bool(FlightMenu, "PlanWithBreathSlow", true));
            const float time = routeLength * fraction /
                std::max(1.0f, speed);
            const Vector3 predictedTarget = PredictPosition(target, time);
            BeamUnit first{};
            const int firstId = FirstBeamBodyAt(
                position, predictedTarget, QRange(player.Level()),
                time, &first);
            FlightSample sample{};
            sample.Position = position;
            sample.TargetDistance = position.Distance2D(predictedTarget);
            sample.NearbyEnemies = Engine::CountEnemiesAt(position, 650.0f);
            sample.NearbyAllies = Engine::CountAlliesAt(position, 760.0f);
            sample.QHasPrimaryContact = firstId ==
                static_cast<int>(target.NetworkId());
            sample.EnemyTurret = Engine::UnderEnemyTurret(position);
            sample.PointClickLockdown =
                HasReadyPointClickThreatAt(position);
            sample.DashHazard = HasReadyDashHazardAt(position);
            sample.ReadyHardCrowdControl =
                sample.PointClickLockdown ||
                (IncomingHardCrowdControl && IncomingThreatUntil >= Now());
            sample.TerrainSeparatesThreat =
                NearTerrain(position, 165.0f, 8) &&
                position.Distance2D(target.Position()) >= 390.0f;
            context.Samples.push_back(sample);
        }
        float score = FlightRouteScore(context);
        if (ControllerHelpers::EnemyCastWindowHardCrowdControlSpent(
                EnemyWindows, static_cast<int>(target.NetworkId()))) {
            score += 420.0f;
        }
        if (Engine::IsHardCrowdControlled(target)) score += 360.0f;
        if (target.HealthPercent() <= 30.0f) score += 210.0f;
        if (purpose == WPurpose::ContinueBreath && QActive) score += 330.0f;
        if (score > best.Score) {
            best.Destination = destination;
            best.Direction = direction;
            best.Purpose = purpose;
            best.TargetId = static_cast<int>(target.NetworkId());
            best.Score = score;
            best.EscapeRoute = escape;
            best.TakedownReset = context.TakedownReset;
            best.Valid = score >= static_cast<float>(
                Slider(FlightMenu, "MinimumRouteScore", 180));
        }
    }
    return best;
}

inline bool CastWPlan(const WPlan& plan, Mode mode, bool reactive = false) {
    if (!plan.Valid || WActive || !Ready(1) ||
        !SpellEnabled(1, mode) || !CastThrottleReady(1, reactive) ||
        !HasCurrentResource(SpellCost(1))) return false;
    const AIHeroClient target = HeroByNetworkId(plan.TargetId);
    if (!Engine::ValidEnemy(target) || PlayerMobilityLocked()) return false;
    if (!plan.EscapeRoute && Bool(FlightMenu, "OnlyAfterCCSpent", true) &&
        !ControllerHelpers::EnemyCastWindowHardCrowdControlSpent(
            EnemyWindows, plan.TargetId) &&
        !Engine::IsHardCrowdControlled(target) &&
        target.HealthPercent() > 30.0f &&
        plan.Purpose != WPurpose::ContinueBreath) {
        return false;
    }
    if (Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true) && !reactive) {
        return false;
    }
    if (!Engine::ControllerCastPosition(1, plan.Destination)) return false;
    WActive = true;
    WControllerOwned = true;
    WCastTick = WLastObservedTick = Now();
    WTargetId = plan.TargetId;
    WDestination = plan.Destination;
    LastWPlan = plan;
    LastWPurpose = plan.Purpose;
    ActiveSequence = plan.EscapeRoute
        ? (plan.TakedownReset ? Sequence::FlightResetExit : Sequence::SelfPeel)
        : (QActive ? Sequence::FlightBreath : Sequence::FlightSingularityBreath);
    return true;
}

inline bool StopW() {
    if (!WActive || !WControllerOwned ||
        Now() - WCastTick < static_cast<int>(kWRecastLockSeconds * 1000.0f) ||
        !CastThrottleReady(1, true)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    ClearWState();
    return true;
}

inline bool ManageW() {
    if (!WActive) return false;
    const auto player = GameObjects::Player();
    const AIHeroClient target = HeroByNetworkId(WTargetId);
    if (!player.IsValid()) return false;
    const Vector3 routeDirection = SharedGeometry::Direction2D(
        player.Position(), WDestination);
    const Vector3 cursorDirection = SharedGeometry::Direction2D(
        player.Position(), Game::CursorPos());
    FlightStopContext context{};
    context.Flying = true;
    context.RecastUnlocked = Now() - WCastTick >=
        static_cast<int>(kWRecastLockSeconds * 1000.0f);
    context.TargetOutsideQ = !Engine::ValidEnemy(target) ||
        player.Position().Distance2D(target.Position()) >
            QRange(player.Level()) + 170.0f;
    context.EndpointTurret = Engine::UnderEnemyTurret(WDestination);
    context.LockdownAhead = HasReadyPointClickThreatAt(WDestination) ||
                            HasReadyDashHazardAt(WDestination);
    context.PlayerCursorReversed = !routeDirection.IsZero() &&
        !cursorDirection.IsZero() && routeDirection.Dot(cursorDirection) < -0.42f;
    context.SafeResetExitReached =
        LastWPurpose == WPurpose::ResetEscape &&
        Engine::CountEnemiesAt(player.Position(), 720.0f) == 0;
    context.CurrentRouteStillScores =
        !Engine::UnderEnemyTurret(player.Position()) &&
        Engine::CountEnemiesAt(player.Position(), 520.0f) <=
            Engine::CountAlliesAt(player.Position(), 760.0f) + 1;
    return ShouldStopFlight(context) && StopW();
}

inline std::vector<SingularityUnit> BuildSingularityUnits(
    float delaySeconds,
    bool includeFarm,
    int primaryId) {
    std::vector<SingularityUnit> result;
    result.reserve(48);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        SingularityUnit unit{};
        unit.Position = PredictPosition(enemy, delaySeconds);
        unit.Radius = enemy.BoundingRadius();
        unit.Health = enemy.Health() + enemy.AllShield();
        unit.MaximumHealth = enemy.MaxHealth();
        unit.Priority = TargetPriority(enemy);
        unit.ExpectedSecondsInside = Engine::IsHardCrowdControlled(enemy)
            ? 3.0f : (enemy.IsDashing() ? 1.15f : 1.75f);
        unit.Kind = EUnitKind::Champion;
        unit.ExpectedToDie = ETotalDamage(enemy) +
            QOneBurstDamage(enemy, WActive) >= unit.Health;
        unit.Primary = static_cast<int>(enemy.NetworkId()) == primaryId;
        unit.HardCrowdControlled = Engine::IsHardCrowdControlled(enemy);
        unit.Valid = true;
        result.push_back(unit);
    }
    if (!includeFarm) return result;
    auto appendMinion = [&](const AIMinionClient& minion) {
        if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable() ||
            HasFlag(minion.GetMinionType(), MinionTypes::Ward) ||
            minion.IsPlant()) return;
        const EUnitKind kind = ClassifyEUnit(minion);
        SingularityUnit unit{};
        unit.Position = PredictPosition(minion, delaySeconds);
        unit.Radius = minion.BoundingRadius();
        unit.Health = minion.Health();
        unit.MaximumHealth = minion.MaxHealth();
        unit.Priority = kind == EUnitKind::LargeMinion ? 2.3f :
                        (kind == EUnitKind::EpicMonster ? 4.0f :
                         (kind == EUnitKind::LargeMonster ? 2.2f : 0.9f));
        unit.ExpectedSecondsInside = kEDurationSeconds;
        unit.Kind = kind;
        const float package = ETotalDamage(minion) +
            QOneBurstDamage(minion, WActive) * 0.55f;
        unit.ExpectedToDie = package >= unit.Health;
        unit.Primary = static_cast<int>(minion.NetworkId()) == primaryId;
        unit.Valid = true;
        result.push_back(unit);
    };
    for (const auto& minion : GameObjects::EnemyMinions()) appendMinion(minion);
    for (const auto& monster : GameObjects::Jungle()) appendMinion(monster);
    return result;
}

inline EPlan BuildEPlan(const AIBaseClient& primary,
                        EPurpose purpose,
                        bool includeFarm) {
    EPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !primary.IsValid() || SpellRank(2) <= 0) {
        return best;
    }
    const float range = ERange(player.Level(), WActive);
    const int primaryId = static_cast<int>(primary.NetworkId());
    std::vector<Vector3> candidates;
    candidates.reserve(28);
    const Vector3 predicted = PredictPosition(primary, 0.70f);
    candidates.push_back(predicted);
    if (primary.PathEnd().IsValid() && !primary.PathEnd().IsZero()) {
        const Vector3 pathDirection = SharedGeometry::Direction2D(
            primary.Position(), primary.PathEnd());
        if (!pathDirection.IsZero()) {
            candidates.push_back(predicted + pathDirection * 95.0f);
            candidates.push_back(predicted - pathDirection * 70.0f);
        }
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            static_cast<int>(enemy.NetworkId()) == primaryId) continue;
        const Vector3 other = PredictPosition(enemy, 0.70f);
        if (other.Distance2D(predicted) <=
            SingularityRadius(Stardust()) * 2.0f + 180.0f) {
            candidates.push_back((predicted + other) * 0.5f);
        }
    }
    if (EActive && !ECenter.IsZero()) candidates.push_back(ECenter);
    if (GapcloserExpireTick >= Now() && !GapcloserEnd.IsZero()) {
        candidates.push_back(GapcloserEnd);
    }
    if (includeFarm) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (minion.IsValid() && !minion.IsDead() &&
                minion.IsTargetable() &&
                !HasFlag(minion.GetMinionType(), MinionTypes::Ward)) {
                candidates.push_back(minion.Position());
            }
        }
        for (const auto& monster : GameObjects::Jungle()) {
            if (monster.IsValid() && !monster.IsDead() &&
                monster.IsTargetable()) candidates.push_back(monster.Position());
        }
    }
    const auto units = BuildSingularityUnits(
        kEAppearanceDelaySeconds, includeFarm, primaryId);
    for (const Vector3& raw : candidates) {
        const Vector3 center = ClampCastPosition(raw, range);
        if (center.IsZero()) continue;
        SingularityEvaluation evaluation = EvaluateSingularity(
            center, Stardust(), units, includeFarm);
        if (evaluation.Score <= -FLT_MAX * 0.5f) continue;
        float score = evaluation.Score * 105.0f;
        if (purpose == EPurpose::RSetup && Ready(3)) score += 260.0f;
        if (purpose == EPurpose::Peel &&
            center.Distance2D(player.Position()) <= 460.0f) score += 310.0f;
        if (purpose == EPurpose::CannonWave &&
            evaluation.ExpectedStardust >= 8) score += 420.0f;
        if (purpose == EPurpose::Objective && ObjectiveActive()) score += 280.0f;
        if (Engine::UnderEnemyTurret(center) && purpose != EPurpose::Peel) {
            score -= 180.0f;
        }
        if (score > best.Score) {
            best.Center = center;
            best.Evaluation = evaluation;
            best.Purpose = purpose;
            best.TargetId = primaryId;
            best.Score = score;
            best.Valid = true;
        }
    }
    return best;
}

inline bool CastEPlan(const EPlan& plan, Mode mode, bool reactive = false) {
    if (!plan.Valid || EActive || !Ready(2) ||
        !SpellEnabled(2, mode) || !CastThrottleReady(2, reactive) ||
        !HasCurrentResource(SpellCost(2))) return false;
    if (Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true) && !reactive) {
        return false;
    }
    if (!Engine::ControllerCastPosition(2, plan.Center)) return false;
    EActive = true;
    EControllerOwned = true;
    ECastTick = Now();
    EExpireTick = ECastTick + static_cast<int>(
        (kEAppearanceDelaySeconds + kEDurationSeconds) * 1000.0f);
    ETargetId = plan.TargetId;
    ECenter = plan.Center;
    LastEPlan = plan;
    LastEPurpose = plan.Purpose;
    ActiveSequence = WActive
        ? Sequence::FlightSingularityBreath
        : (plan.Purpose == EPurpose::CannonWave
            ? Sequence::CannonHarvest : Sequence::SingularityBreath);
    return true;
}

inline std::vector<UltimateUnit> BuildUltimateUnits(
    float impactSeconds,
    int primaryId,
    bool empowered) {
    std::vector<UltimateUnit> units;
    units.reserve(16);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        UltimateUnit unit{};
        unit.Position = PredictPosition(enemy, impactSeconds);
        unit.Radius = enemy.BoundingRadius();
        unit.Priority = TargetPriority(enemy);
        unit.Champion = true;
        unit.Primary = static_cast<int>(enemy.NetworkId()) == primaryId;
        unit.SpellShield = HasSpellShieldOrImmunity(enemy);
        unit.HardCrowdControlled = Engine::IsHardCrowdControlled(enemy);
        unit.Dashing = enemy.IsDashing();
        unit.Lethal = RDirectDamage(enemy, empowered) >=
            enemy.Health() + enemy.AllShield();
        unit.Valid = !IsCommonUntargetableOrImmune(enemy);
        units.push_back(unit);
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (!monster.IsValid() || monster.IsDead() ||
            !monster.IsTargetable() || !IsEpicMonster(monster)) continue;
        UltimateUnit unit{};
        unit.Position = PredictPosition(monster, impactSeconds);
        unit.Radius = monster.BoundingRadius();
        unit.Priority = 2.2f;
        unit.Champion = false;
        unit.EpicMonster = true;
        unit.Primary = static_cast<int>(monster.NetworkId()) == primaryId;
        unit.Lethal = RDirectDamage(monster, empowered) >= monster.Health();
        unit.Valid = true;
        units.push_back(unit);
    }
    return units;
}

inline RPlan BuildRPlan(const AIBaseClient& primary,
                        RPurpose purpose,
                        bool manual = false) {
    RPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !primary.IsValid() || SpellRank(3) <= 0) {
        return best;
    }
    const bool empowered = RuntimeEmpoweredR() || Calamity.Ready;
    const float baseDelay = UltimateImpactDelay(empowered);
    const int primaryId = static_cast<int>(primary.NetworkId());
    const Vector3 predicted = PredictPosition(primary, baseDelay);
    std::vector<Vector3> candidates;
    candidates.reserve(24);
    candidates.push_back(predicted);
    if (EActive && !ECenter.IsZero()) candidates.push_back(ECenter);
    if (purpose == RPurpose::AntiGapcloser && !GapcloserEnd.IsZero()) {
        candidates.push_back(GapcloserEnd);
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            static_cast<int>(enemy.NetworkId()) == primaryId) continue;
        const Vector3 other = PredictPosition(enemy, baseDelay);
        const float radius = empowered
            ? SkiesDescendRadius(Stardust())
            : FallingStarRadius(Stardust());
        if (other.Distance2D(predicted) <= radius * 2.0f + 160.0f) {
            candidates.push_back((predicted + other) * 0.5f);
        }
    }
    for (const Vector3& raw : candidates) {
        const Vector3 requested = ClampCastPosition(raw, kRRange);
        if (requested.IsZero()) continue;
        Vector3 wallContact{};
        const bool intercepted = ProjectileWallFirstContactFromPlayer(
            requested, 60.0f, wallContact);
        const Vector3 impactCenter = ResolveUltimateImpactCenter(
            requested, wallContact, intercepted);
        float impactSeconds = baseDelay;
        if (intercepted) {
            impactSeconds = std::min(
                baseDelay,
                0.20f + player.Position().Distance2D(impactCenter) / 1200.0f);
        }
        const auto units = BuildUltimateUnits(
            impactSeconds, primaryId, empowered);
        UltimateEvaluation evaluation = EvaluateUltimate(
            impactCenter, Stardust(), empowered, units);
        if (evaluation.Score <= -FLT_MAX * 0.5f) continue;
        float score = evaluation.Score * 115.0f;
        if (EActive && impactCenter.Distance2D(ECenter) <=
            SingularityRadius(Stardust()) * 0.75f) score += 340.0f;
        if (purpose == RPurpose::SelfPeel ||
            purpose == RPurpose::AllyPeel) score += 520.0f;
        if (purpose == RPurpose::Interrupt) score += 480.0f;
        if (purpose == RPurpose::KillSecure) score += 440.0f;
        if (purpose == RPurpose::Objective && empowered) score += 390.0f;
        if (intercepted && !evaluation.PrimaryDirect) score -= 720.0f;
        if (!manual && !CursorDirectionAgrees(requested, -0.24f) &&
            purpose != RPurpose::SelfPeel &&
            purpose != RPurpose::AllyPeel &&
            purpose != RPurpose::Interrupt &&
            purpose != RPurpose::AntiGapcloser &&
            Orbwalker::ActiveMode() != OrbwalkingMode::Combo) score -= 360.0f;
        if (score > best.Score) {
            best.RequestedCenter = requested;
            best.ImpactCenter = impactCenter;
            best.Evaluation = evaluation;
            best.Purpose = purpose;
            best.TargetId = primaryId;
            best.ImpactSeconds = impactSeconds;
            best.ImpactTick = Now() + static_cast<int>(
                std::ceil(impactSeconds * 1000.0f));
            best.Score = score;
            best.Empowered = empowered;
            best.ProjectileIntercepted = intercepted;
            best.Manual = manual;
            best.Valid = true;
        }
    }
    return best;
}

inline bool RPlanAllowed(const RPlan& plan) {
    if (!plan.Valid) return false;
    const AIHeroClient primary = HeroByNetworkId(plan.TargetId);
    if (Engine::ValidEnemy(primary) &&
        HasSpellShieldOrImmunity(primary) &&
        plan.Evaluation.DirectHits <= 1 &&
        plan.Purpose != RPurpose::SelfPeel &&
        plan.Purpose != RPurpose::AllyPeel) return false;
    if (plan.Purpose == RPurpose::Interrupt) {
        const float remaining = static_cast<float>(
            std::max(0, InterruptExpireTick - Now())) / 1000.0f;
        if (plan.ImpactSeconds > remaining + 0.10f) return false;
    }
    const int minimumHits = plan.Empowered
        ? Slider(StarMenu, "EmpoweredMinimumHits", 2)
        : Slider(StarMenu, "RegularMinimumHits", 2);
    const bool reactive = plan.Purpose == RPurpose::SelfPeel ||
        plan.Purpose == RPurpose::AllyPeel ||
        plan.Purpose == RPurpose::Interrupt ||
        plan.Purpose == RPurpose::AntiGapcloser ||
        plan.Purpose == RPurpose::KillSecure || plan.Manual;
    if (!reactive && plan.Evaluation.DirectHits < minimumHits &&
        !(plan.Empowered && ShouldSpendEmpoweredUltimate(
            plan.Evaluation, ObjectiveActive(), false, false,
            minimumHits))) return false;
    if (plan.Empowered && Bool(StarMenu, "ReserveForObjective", true) &&
        !ObjectiveActive() && !reactive &&
        plan.Evaluation.DirectHits < 3 &&
        !plan.Evaluation.PrimaryDirect) return false;
    return true;
}

inline bool CastRPlan(const RPlan& plan, Mode mode, bool reactive = false) {
    if (!RPlanAllowed(plan) || !Ready(3) || !SpellEnabled(3, mode) ||
        !CastThrottleReady(3, reactive) ||
        !HasCurrentResource(SpellCost(3))) return false;
    if (Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true) && !reactive) {
        return false;
    }
    if (!Engine::ControllerCastPosition(3, plan.RequestedCenter)) return false;
    RCastTick = Now();
    RImpactTick = plan.ImpactTick;
    RImpactCenter = plan.ImpactCenter;
    LastRWasEmpowered = plan.Empowered;
    RStardustResolutionPending = true;
    LastRPlan = plan;
    LastRPurpose = plan.Purpose;
    if (plan.Empowered) {
        Calamity = AdvanceCalamity(Calamity, 0, true);
    }
    ActiveSequence = plan.Purpose == RPurpose::Interrupt
        ? Sequence::Interrupt
        : (plan.Purpose == RPurpose::Objective
            ? Sequence::ObjectiveCalamity
            : (WActive ? Sequence::FlightSingularityStarBreath
                       : Sequence::SingularityStarBreath));
    return true;
}

inline AIHeroClient CursorEnemy(float range = kRRange + 200.0f) {
    const auto player = GameObjects::Player();
    AIHeroClient best{};
    float bestCursorDistance = FLT_MAX;
    if (!player.IsValid()) return best;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range)) continue;
        const float distance = enemy.Position().Distance2D(Game::CursorPos());
        if (distance < bestCursorDistance) {
            best = enemy;
            bestCursorDistance = distance;
        }
    }
    return bestCursorDistance <= 650.0f ? best : AIHeroClient{};
}

inline void RefreshProtection() {
    const AIHeroClient ally = SelectProtectionAlly(1250.0f);
    ProtectedAllyId = Engine::ValidAlly(ally)
        ? static_cast<int>(ally.NetworkId()) : 0;
    PeelThreatId = 0;
    if (!Engine::ValidAlly(ally)) return;
    float bestDistance = FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        const float distance = ally.Position().Distance2D(enemy.Position());
        if (distance <= 700.0f && distance < bestDistance) {
            bestDistance = distance;
            PeelThreatId = static_cast<int>(enemy.NetworkId());
        }
    }
}

inline void RefreshWResetTelemetry() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::W);
    if (!spell.IsValid() || spell.Level() <= 0) return;
    const int now = Now();
    const float remaining = std::max(
        0.0f, spell.RemainingCooldown(Game::Time()));
    if (WCooldownObserved && LastWCooldownSampleTick > 0) {
        const int elapsed = now - LastWCooldownSampleTick;
        const float abruptDrop = LastWCooldownSeconds - remaining;
        if (elapsed >= 0 && elapsed <= 280 && abruptDrop >= 1.35f &&
            remaining <= 2.6f && now - LastChampionContactTick <= 3200) {
            LastWTakedownResetTick = now;
        }
    }
    if (remaining <= 0.08f) LastWReadyTick = now;
    LastWCooldownSeconds = remaining;
    LastWCooldownSampleTick = now;
    WCooldownObserved = true;
}

inline void RefreshEChampionContact() {
    if (!EActive || ECenter.IsZero()) return;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && SingularityDamageContains(
                ECenter, enemy.Position(), enemy.BoundingRadius(),
                Stardust())) {
            LastChampionContactTick = Now();
            return;
        }
    }
}

inline void ResolvePendingRStardust() {
    if (!RStardustResolutionPending || RImpactTick <= 0 ||
        Now() < RImpactTick) return;
    RStardustResolutionPending = false;
    LastChampionContactTick = Now();
    if (StardustTelemetryReliable || RImpactCenter.IsZero()) return;
    const float radius = LastRWasEmpowered
        ? SkiesDescendRadius(Stardust())
        : FallingStarRadius(Stardust());
    int championHits = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            HasSpellShieldOrImmunity(enemy) ||
            IsCommonUntargetableOrImmune(enemy)) continue;
        if (RImpactCenter.Distance2D(enemy.Position()) <=
            radius + enemy.BoundingRadius()) ++championHits;
    }
    AddPredictedStardust(
        championHits * static_cast<int>(kRStardustPerChampion));
}

inline void RefreshRuntimeState() {
    RefreshStardustTelemetry();
    RefreshWResetTelemetry();
    const int now = Now();
    const bool runtimeW = RuntimeWActive();
    if (runtimeW) {
        WActive = true;
        WLastObservedTick = now;
    } else if (WActive && now - WLastObservedTick > 180) {
        ClearWState();
    }
    const bool runtimeQ = RuntimeQActive();
    if (runtimeQ) {
        QActive = true;
        QLastObservedTick = now;
        if (QLastUpdateTick == 0) QLastUpdateTick = now;
    } else if (QActive && now - QLastObservedTick > 180) {
        ClearQState();
    }
    if (EActive && now > EExpireTick + 120) {
        EActive = false;
        EControllerOwned = false;
        ECastTick = EExpireTick = ETargetId = 0;
        ECenter = {};
    }
    RefreshEChampionContact();
    ResolvePendingRStardust();
    if (GapcloserExpireTick < now) {
        GapcloserTargetId = GapcloserExpireTick = 0;
        GapcloserEnd = {};
    }
    if (InterruptExpireTick < now) {
        InterruptTargetId = InterruptExpireTick = 0;
    }
    if (IncomingThreatUntil < now) {
        IncomingThreatTargetId = IncomingThreatUntil = 0;
        IncomingHardCrowdControl = false;
    }
    RefreshProtection();
}

inline Posture DeterminePosture(Mode mode, const AIHeroClient& target) {
    if (mode == Mode::Flee) return Posture::Flee;
    if (WActive) return Posture::FlightAngle;
    if (ObjectiveActive()) return Posture::Objective;
    if (PeelThreatId != 0 || GapcloserExpireTick >= Now()) return Posture::Peel;
    if (mode == Mode::LaneClear || mode == Mode::LastHit ||
        mode == Mode::Jungle) return Posture::Farm;
    if (EActive) return Posture::Zone;
    if (Engine::ValidEnemy(target) &&
        Engine::CountEnemiesAt(target.Position(), 700.0f) >= 2) {
        return Posture::FrontToBack;
    }
    return mode == Mode::Harass ? Posture::LanePressure : Posture::Neutral;
}

inline bool TryManualR() {
    if (!Key(StarMenu, "ManualR", false) || !Ready(3)) return false;
    const AIHeroClient target = CursorEnemy();
    if (!Engine::ValidEnemy(target)) return false;
    return CastRPlan(BuildRPlan(target, RPurpose::Manual, true),
                     Mode::Automatic, true);
}

inline bool TryReactiveRAndE(Mode mode) {
    if (InterruptTargetId != 0 && InterruptExpireTick >= Now()) {
        const AIHeroClient target = HeroByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(target)) {
            if (Ready(3) && Bool(StarMenu, "Interrupt", true)) {
                const RPlan plan = BuildRPlan(target, RPurpose::Interrupt);
                if (RPlanAllowed(plan) &&
                    CastRPlan(plan, Mode::Automatic, true)) return true;
            }
            if (Ready(2) && Bool(SingularityMenu, "InterruptZone", true)) {
                const EPlan plan = BuildEPlan(
                    target, EPurpose::RSetup, false);
                if (CastEPlan(plan, Mode::Automatic, true)) return true;
            }
        }
    }
    if (GapcloserTargetId != 0 && GapcloserExpireTick >= Now()) {
        const AIHeroClient target = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(target)) {
            if (Ready(2) && Bool(SingularityMenu, "AntiGapcloser", true)) {
                EPlan plan = BuildEPlan(target, EPurpose::Peel, false);
                if (!GapcloserEnd.IsZero()) plan.Center = GapcloserEnd;
                if (CastEPlan(plan, Mode::Automatic, true)) return true;
            }
            const auto player = GameObjects::Player();
            if (Ready(3) && Bool(StarMenu, "AntiGapcloser", true) &&
                player.IsValid() && player.HealthPercent() <=
                    Slider(StarMenu, "PeelHealth", 48)) {
                if (CastRPlan(BuildRPlan(
                        target, RPurpose::AntiGapcloser),
                        Mode::Automatic, true)) return true;
            }
        }
    }
    if (PeelThreatId != 0 && Bool(TacticsMenu, "ProtectWinCondition", true)) {
        const AIHeroClient threat = HeroByNetworkId(PeelThreatId);
        const AIBaseClient allyUnit = UnitByNetworkId(ProtectedAllyId);
        const AIHeroClient ally = allyUnit.IsValid()
            ? AIHeroClient(allyUnit.Handle()) : AIHeroClient{};
        if (Engine::ValidEnemy(threat) && Engine::ValidAlly(ally) &&
            ally.HealthPercent() <= Slider(TacticsMenu, "AllyPeelHealth", 52)) {
            if (Ready(2) && Bool(SingularityMenu, "AllyPeel", true) &&
                CastEPlan(BuildEPlan(threat, EPurpose::Peel, false),
                          Mode::Automatic, true)) return true;
            if (Ready(3) && Bool(StarMenu, "AllyPeel", true) &&
                CastRPlan(BuildRPlan(threat, RPurpose::AllyPeel),
                          Mode::Automatic, true)) return true;
        }
    }
    (void)mode;
    return false;
}

inline bool TryKillSecure(const AIHeroClient& selected) {
    if (!Bool(TacticsMenu, "KillSecure", true)) return false;
    AIHeroClient best = selected;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kRRange + 100.0f)) continue;
        if (!best.IsValid() || enemy.Health() + enemy.AllShield() <
            best.Health() + best.AllShield()) best = enemy;
    }
    if (!Engine::ValidEnemy(best)) return false;
    if (Ready(3) && Bool(StarMenu, "KillSecure", true) &&
        RDirectDamage(best, RuntimeEmpoweredR() || Calamity.Ready) >=
            best.Health() + best.AllShield()) {
        const RPlan plan = BuildRPlan(best, RPurpose::KillSecure);
        if (plan.Evaluation.PrimaryDirect &&
            CastRPlan(plan, Mode::Automatic, true)) return true;
    }
    if (!QActive && Ready(0) &&
        QOneBurstDamage(best, WActive) >= best.Health() + best.AllShield()) {
        return CastQPlan(BuildQPlan(best, QPurpose::Lethal, false),
                         Mode::Automatic, true);
    }
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const float distance = player.Position().Distance2D(target.Position());

    if (WActive) {
        if (!EActive && Ready(2) &&
            Bool(SingularityMenu, "FlightDrop", true) &&
            CastEPlan(BuildEPlan(target, EPurpose::FlightDrop, true),
                      Mode::Combo)) return true;
        if (EActive && Ready(3) && Bool(StarMenu, "Combo", true)) {
            const RPurpose purpose = RuntimeEmpoweredR() || Calamity.Ready
                ? RPurpose::Calamity : RPurpose::ELock;
            if (CastRPlan(BuildRPlan(target, purpose), Mode::Combo)) return true;
        }
        if (!QActive && Ready(0) && Bool(BreathMenu, "Combo", true) &&
            CastQPlan(BuildQPlan(target,
                                EActive ? QPurpose::EContact
                                        : QPurpose::FlightContinue,
                                false), Mode::Combo)) return true;
        return ManageW();
    }

    // Holding Q then taking off preserves the breath in flight. Only use the
    // route if the target leaves stationary contact and a safe offset exists.
    if (QActive && Ready(1) && Bool(FlightMenu, "ContinueActiveBreath", true) &&
        distance > QRange(player.Level()) - 120.0f) {
        const WPlan plan = BuildWPlan(
            target, WPurpose::ContinueBreath, false);
        if (CastWPlan(plan, Mode::Combo)) return true;
    }
    if (QActive) return ManageQ();

    if (Ready(1) && LastWTakedownResetTick + 1450 >= Now()) {
        const bool escapeReset = player.HealthPercent() <= 42.0f ||
            Engine::CountEnemiesAt(player.Position(), 650.0f) >
                Engine::CountAlliesAt(player.Position(), 760.0f) + 1;
        const WPurpose resetPurpose = escapeReset
            ? WPurpose::ResetEscape : WPurpose::ResetReposition;
        if (CastWPlan(BuildWPlan(target, resetPurpose, escapeReset),
                      Mode::Combo, escapeReset)) return true;
    }

    if (!EActive && Ready(2) && Bool(SingularityMenu, "Combo", true)) {
        const EPurpose purpose = Ready(3)
            ? EPurpose::RSetup : EPurpose::LaneSetup;
        const EPlan plan = BuildEPlan(target, purpose, true);
        if (plan.Evaluation.Champions >= 1 &&
            CastEPlan(plan, Mode::Combo)) return true;
    }
    if (EActive && Ready(3) && Bool(StarMenu, "Combo", true)) {
        const RPurpose purpose = RuntimeEmpoweredR() || Calamity.Ready
            ? (ObjectiveActive() ? RPurpose::Objective : RPurpose::Calamity)
            : RPurpose::ELock;
        if (CastRPlan(BuildRPlan(target, purpose), Mode::Combo)) return true;
    }
    if (Ready(1) && Bool(FlightMenu, "Combo", true) &&
        (distance > QRange(player.Level()) - 80.0f ||
         ControllerHelpers::EnemyCastWindowHardCrowdControlSpent(
             EnemyWindows, static_cast<int>(target.NetworkId())))) {
        const WPurpose purpose =
            ControllerHelpers::EnemyCastWindowHardCrowdControlSpent(
                EnemyWindows, static_cast<int>(target.NetworkId()))
            ? WPurpose::PunishCooldown : WPurpose::OffsetAllIn;
        if (CastWPlan(BuildWPlan(target, purpose, false), Mode::Combo)) {
            return true;
        }
    }
    if (Ready(0) && Bool(BreathMenu, "Combo", true)) {
        const QPurpose purpose = EActive ? QPurpose::EContact
            : (RImpactTick >= Now() ? QPurpose::RContact
                                    : QPurpose::LaneBurst);
        if (CastQPlan(BuildQPlan(target, purpose, false), Mode::Combo)) {
            return true;
        }
    }
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) ||
        ControllerHelpers::PlayerManaPercent() < Slider(BreathMenu, "HarassMana", 52)) return false;
    if (QActive) return ManageQ();
    if (Ready(2) && Bool(SingularityMenu, "Harass", true) &&
        ControllerHelpers::PlayerManaPercent() >= Slider(SingularityMenu, "HarassMana", 72)) {
        const EPlan plan = BuildEPlan(target, EPurpose::LaneSetup, true);
        if ((plan.Evaluation.ExpectedStardust >=
                 Slider(SingularityMenu, "HarassMinimumStacks", 3) ||
             plan.Evaluation.Champions >= 2) &&
            CastEPlan(plan, Mode::Harass)) return true;
    }
    if (Ready(0) && Bool(BreathMenu, "Harass", true)) {
        const bool allowSplash = Bool(BreathMenu, "SplashTap", true) &&
            ControllerHelpers::PlayerManaPercent() >= Slider(BreathMenu, "SplashTapMana", 72);
        const QPlan plan = BuildQPlan(
            target, QPurpose::LaneBurst, allowSplash);
        if ((!plan.Splash || Bool(BreathMenu, "SplashTap", true)) &&
            CastQPlan(plan, Mode::Harass)) return true;
    }
    return false;
}

inline AIBaseClient BestFarmTarget(bool jungle) {
    const auto player = GameObjects::Player();
    AIBaseClient best{};
    float bestScore = -FLT_MAX;
    if (!player.IsValid()) return best;
    auto consider = [&](const AIMinionClient& unit) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            HasFlag(unit.GetMinionType(), MinionTypes::Ward) ||
            unit.IsPlant()) return;
        const float range = ERange(player.Level(), false) + 120.0f;
        if (player.Position().Distance2D(unit.Position()) > range) return;
        const EUnitKind kind = ClassifyEUnit(unit);
        float score = unit.MaxHealth() * 0.002f - unit.Health() * 0.0002f;
        if (kind == EUnitKind::LargeMinion) score += 8.0f;
        if (kind == EUnitKind::LargeMonster) score += 6.0f;
        if (kind == EUnitKind::EpicMonster) score += 20.0f;
        if (score > bestScore) {
            best = unit;
            bestScore = score;
        }
    };
    if (jungle) {
        for (const auto& monster : GameObjects::Jungle()) consider(monster);
    } else {
        for (const auto& minion : GameObjects::EnemyMinions()) consider(minion);
    }
    return best;
}

inline bool TryFarm(Mode mode) {
    const bool jungle = mode == Mode::Jungle ||
        (mode == Mode::LaneClear && HasNearbyJungleTarget(
             ERange(GameObjects::Player().Level(), false)) &&
         GameObjects::EnemyMinions().empty());
    if (HasEnemyChampionNear(
            static_cast<float>(Slider(FarmMenu, "ChampionHoldRange", 1150))) &&
        Bool(FarmMenu, "HoldForChampion", true)) return false;
    const float minimumMana = static_cast<float>(Slider(
        FarmMenu, jungle ? "JungleMana" : "LaneMana",
        jungle ? 38 : 62));
    if (ControllerHelpers::PlayerManaPercent() < minimumMana) return false;
    const AIBaseClient target = BestFarmTarget(jungle);
    if (!target.IsValid()) return false;
    if (QActive) return ManageQ();

    if (!EActive && Ready(2) && Bool(FarmMenu,
            jungle ? "UseEJungle" : "UseELane", true)) {
        const EUnitKind kind = ClassifyEUnit(target);
        const EPurpose purpose = jungle
            ? (kind == EUnitKind::EpicMonster
                ? EPurpose::Objective : EPurpose::Jungle)
            : (kind == EUnitKind::LargeMinion
                ? EPurpose::CannonWave : EPurpose::Wave);
        const EPlan plan = BuildEPlan(target, purpose, true);
        const int minimumStacks = Slider(
            FarmMenu, jungle ? "EJungleStacks" : "ELaneStacks",
            jungle ? 3 : 6);
        const int minimumUnits = Slider(
            FarmMenu, jungle ? "EJungleUnits" : "ELaneUnits",
            jungle ? 2 : 5);
        if ((IsEpicMonster(target) ||
             (plan.Evaluation.ExpectedStardust >= minimumStacks &&
              plan.Evaluation.Units >= minimumUnits)) &&
            CastEPlan(plan, mode)) return true;
    }
    if (Ready(0) && Bool(FarmMenu,
            jungle ? "UseQJungle" : "UseQLane", true)) {
        const QPurpose purpose = jungle
            ? (IsEpicMonster(target) ? QPurpose::Objective
                                     : QPurpose::Jungle)
            : QPurpose::Wave;
        return CastQPlan(BuildQPlan(target, purpose, false), mode);
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& selected) {
    const AIHeroClient threat = NearestEnemyToPlayer(selected, 1300.0f);
    if (!Engine::ValidEnemy(threat)) return false;
    if (QActive && QControllerOwned) {
        if (StopQ(true)) return true;
    }
    if (WActive) return ManageW();
    if (Ready(2) && Bool(SingularityMenu, "Flee", true) &&
        CastEPlan(BuildEPlan(threat, EPurpose::Peel, false),
                  Mode::Flee, true)) return true;
    const auto player = GameObjects::Player();
    if (Ready(3) && Bool(StarMenu, "Flee", true) && player.IsValid() &&
        (player.HealthPercent() <= Slider(StarMenu, "PeelHealth", 48) ||
         threat.Position().Distance2D(player.Position()) <= 360.0f) &&
        CastRPlan(BuildRPlan(threat, RPurpose::SelfPeel),
                  Mode::Flee, true)) return true;
    if (Ready(1) && Bool(FlightMenu, "Flee", true)) {
        const WPurpose purpose = LastWTakedownResetTick + 1200 >= Now()
            ? WPurpose::ResetEscape : WPurpose::Flee;
        return CastWPlan(BuildWPlan(threat, purpose, true),
                         Mode::Flee, true);
    }
    return false;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& target) {
    LastKnownMode = mode;
    RefreshRuntimeState();
    CurrentPosture = DeterminePosture(mode, target);

    if (TryManualR()) return true;
    if (ManageW()) return true;
    if (QActive && ManageQ()) return true;
    if (TryReactiveRAndE(mode)) return true;
    if (TryKillSecure(target)) return true;
    if (mode == Mode::Flee) return TryFlee(target);
    if (mode == Mode::Combo) return TryCombo(target);
    if (mode == Mode::Harass) return TryHarass(target);
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return TryFarm(mode);
    return false;
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (IsQEvent(args)) {
        const bool ours = Engine::WasControllerCast(0);
        QActive = true;
        QControllerOwned = ours;
        QCastTick = QLastObservedTick = QLastUpdateTick = now;
        if (!ours) {
            LastQPurpose = QPurpose::Manual;
            QTargetId = 0;
            float nearest = FLT_MAX;
            const Vector3 end = args.EndPosition.IsValid() &&
                    !args.EndPosition.IsZero()
                ? args.EndPosition : Game::CursorPos();
            const auto player = GameObjects::Player();
            if (player.IsValid()) {
                const Vector3 direction = SharedGeometry::Direction2D(
                    player.Position(), end);
                const auto units = BuildBeamUnits(0.05f);
                const int index = FirstBeamCollisionIndex(
                    player.Position(), direction, units,
                    QRange(player.Level()));
                if (index >= 0) {
                    QTargetId = units[static_cast<std::size_t>(index)].Id;
                    QFirstBodyId = QTargetId;
                    nearest = BeamEntryDistance(
                        player.Position(), direction,
                        units[static_cast<std::size_t>(index)],
                        QRange(player.Level()));
                }
            }
            (void)nearest;
            ActiveSequence = Sequence::PlayerLed;
        }
        return;
    }
    if (IsWEvent(args)) {
        const bool ours = Engine::WasControllerCast(1);
        const bool recast = WActive && now - WCastTick >= 250;
        if (recast && !ours) {
            ClearWState();
        } else {
            WActive = true;
            WControllerOwned = ours;
            WCastTick = WLastObservedTick = now;
            WDestination = args.EndPosition.IsValid() &&
                    !args.EndPosition.IsZero()
                ? args.EndPosition : args.CastPosition;
            if (!ours) {
                LastWPurpose = WPurpose::Manual;
                ActiveSequence = Sequence::PlayerLed;
            }
        }
        return;
    }
    if (IsEEvent(args)) {
        const bool ours = Engine::WasControllerCast(2);
        EActive = true;
        EControllerOwned = ours;
        ECastTick = now;
        EExpireTick = now + static_cast<int>(
            (kEAppearanceDelaySeconds + kEDurationSeconds) * 1000.0f);
        ECenter = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
            ? args.EndPosition : args.CastPosition;
        if (!ours) {
            LastEPurpose = EPurpose::Manual;
            ActiveSequence = Sequence::PlayerLed;
        }
        return;
    }
    if (IsREvent(args)) {
        const bool ours = Engine::WasControllerCast(3);
        const bool empowered = RuntimeEmpoweredR() ||
            SpellEventNameContainsAny(args, {
                "aurelionsolr2", "skiesdescend",
            });
        RCastTick = now;
        RImpactCenter = args.EndPosition.IsValid() &&
                !args.EndPosition.IsZero()
            ? args.EndPosition : args.CastPosition;
        RImpactTick = now + static_cast<int>(
            UltimateImpactDelay(empowered) * 1000.0f);
        LastRWasEmpowered = empowered;
        Vector3 wallContact{};
        if (ProjectileWallFirstContactFromPlayer(
                RImpactCenter, 60.0f, wallContact)) {
            RImpactCenter = wallContact;
            RImpactTick = now + static_cast<int>(std::min(
                UltimateImpactDelay(empowered),
                0.20f + GameObjects::Player().Position().Distance2D(
                    wallContact) / 1200.0f) * 1000.0f);
        }
        RStardustResolutionPending = true;
        if (empowered) Calamity = AdvanceCalamity(Calamity, 0, true);
        if (!ours) {
            LastRPurpose = RPurpose::Manual;
            ActiveSequence = Sequence::PlayerLed;
        }
    }
}

inline void RecordEnemySpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto analysis = AnalyzeEnemyCast(
        args, 220.0f, 115.0f, 320, 250, 220, 1800, 560);
    if (!analysis.Valid) return;
    const int id = static_cast<int>(analysis.Enemy.NetworkId());
    if (EnemyWindow* record = FindEnemyWindow(id, true)) {
        record->LastSpellSlot = args.Slot;
        if (analysis.Committed) {
            record->CommittedUntil = std::max(
                record->CommittedUntil,
                std::max(analysis.CommitmentUntilTick, Now() + 900));
        }
        if (analysis.LikelyHardCrowdControl) {
            record->HardCrowdControlSpentUntil = std::max(
                record->HardCrowdControlSpentUntil, Now() + 3600);
        }
    }
    if (analysis.TargetsPlayer || analysis.CrossesPlayer) {
        IncomingThreatTargetId = id;
        IncomingThreatUntil = std::max(
            analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
        IncomingHardCrowdControl = analysis.LikelyHardCrowdControl;
    }
}

inline void OnDoCast(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || args.IsAutoAttack) return;
    // ProcessSpell is authoritative; DoCast only refreshes liveness for builds
    // where the channel event arrives on the later bridge.
    if (IsQEvent(args)) QLastObservedTick = Now();
    if (IsWEvent(args)) WLastObservedTick = Now();
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args,
                            bool added) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (IsStardustBuff(args.BuffName) && added && args.Count >= 0) {
        ObserveStardustCount(args.Count);
    }
    if (IsCalamityBuff(args.BuffName)) {
        Calamity.Ready = added;
        Calamity.Progress = added ? kRUpgradeRequirement : 0;
        CalamityTelemetryReliable = true;
    }
    if (IsQBuff(args.BuffName)) {
        QActive = added;
        QLastObservedTick = Now();
        if (!added) ClearQState();
    }
    if (IsWBuff(args.BuffName)) {
        WActive = added;
        WLastObservedTick = Now();
        if (!added) ClearWState();
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    UpdateBuffState(args, true);
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    UpdateBuffState(args, false);
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    UpdateBuffState(args, true);
}

inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (ControllerHelpers::CaptureGapcloser(
            args, GapcloserTargetId, GapcloserEnd,
            GapcloserExpireTick, 760.0f, 1050)) {
        IncomingThreatTargetId = GapcloserTargetId;
        IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 850);
    }
}

inline void OnInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    ControllerHelpers::CaptureInterruptable(
        args, InterruptTargetId, InterruptExpireTick,
        1200, 250, 6000);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (QActive && Bool(BreathMenu, "ProtectChannel", true)) {
        args.Process = false;
    }
}

inline const char* SequenceName(Sequence sequence) {
    switch (sequence) {
    case Sequence::LaneBurst: return "Q one-second burst";
    case Sequence::SplashTap: return "Q splash tap";
    case Sequence::SingularityBreath: return "E-Q";
    case Sequence::SingularityStarBreath: return "E-R-Q";
    case Sequence::FlightBreath: return "W-Q";
    case Sequence::FlightSingularityBreath: return "W-E-Q";
    case Sequence::FlightSingularityStarBreath: return "W-E-R-Q";
    case Sequence::FlightResetExit: return "W reset exit";
    case Sequence::SelfPeel: return "self peel";
    case Sequence::AllyPeel: return "wincon peel";
    case Sequence::Interrupt: return "interrupt";
    case Sequence::ObjectiveCalamity: return "objective Skies Descend";
    case Sequence::CannonHarvest: return "eight-stack cannon wave";
    case Sequence::JungleHarvest: return "jungle harvest";
    case Sequence::PlayerLed: return "player-led";
    default: return "idle";
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::LanePressure: return "lane pressure";
    case Posture::FrontToBack: return "front-to-back";
    case Posture::FlightAngle: return "flight angle";
    case Posture::Zone: return "zone";
    case Posture::Peel: return "peel";
    case Posture::Objective: return "objective";
    case Posture::Siege: return "siege";
    case Posture::Farm: return "farm";
    case Posture::Flee: return "flee";
    default: return "neutral";
    }
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (Bool(CoachMenu, "DrawRanges", true)) {
        Drawing::DrawCircle(player.Position(), QRange(player.Level()),
                            0x337CE5FFu, 1.0f, 72);
        Drawing::DrawCircle(player.Position(), kRRange,
                            0x338A52FFu, 1.0f, 80);
    }
    if (Bool(CoachMenu, "DrawBeam", true) && LastQPlan.Valid) {
        Drawing::DrawLine(player.Position(), LastQPlan.Aim,
                          LastQPlan.Splash ? 0xDDF6C66Au : 0xDDB6ECFFu,
                          2.0f);
        const AIBaseClient first = UnitByNetworkId(QFirstBodyId);
        if (first.IsValid()) Drawing::DrawCircle(
            first.Position(), first.BoundingRadius() + kQBeamHalfWidth,
            0xAAE7D8FFu, 1.5f, 40);
    }
    if (Bool(CoachMenu, "DrawFlight", true) && LastWPlan.Valid) {
        Drawing::DrawLine(player.Position(), LastWPlan.Destination,
                          0xCC7CE5FFu, 2.2f);
        Drawing::DrawCircle(LastWPlan.Destination, 95.0f,
                            0x887CE5FFu, 1.5f, 40);
    }
    if (Bool(CoachMenu, "DrawSingularity", true) &&
        (EActive || LastEPlan.Valid)) {
        const Vector3 center = EActive ? ECenter : LastEPlan.Center;
        Drawing::DrawCircle(center, SingularityRadius(Stardust()),
                            0x778A52FFu, 1.8f, 72);
        Drawing::DrawCircle(center, SingularityInnerRadius(Stardust()),
                            0xBBE05BFFu, 1.7f, 56);
    }
    if (Bool(CoachMenu, "DrawStar", true) && LastRPlan.Valid) {
        Drawing::DrawCircle(
            LastRPlan.ImpactCenter,
            LastRPlan.Empowered ? SkiesDescendRadius(Stardust())
                                : FallingStarRadius(Stardust()),
            LastRPlan.Empowered ? 0xCCFF7FE8u : 0xCC8AA8FFu,
            2.0f, 80);
        if (LastRPlan.ProjectileIntercepted) {
            Drawing::DrawLine(player.Position(), LastRPlan.ImpactCenter,
                              0xFFFF675Cu, 2.2f);
        }
    }
    if (Bool(CoachMenu, "DrawPeel", true)) {
        const AIBaseClient ally = UnitByNetworkId(ProtectedAllyId);
        const AIHeroClient threat = HeroByNetworkId(PeelThreatId);
        if (ally.IsValid()) Drawing::DrawCircle(
            ally.Position(), 100.0f, 0xAA73F0FFu, 1.7f, 40);
        if (ally.IsValid() && Engine::ValidEnemy(threat)) {
            Drawing::DrawLine(ally.Position(), threat.Position(),
                              0xFFFF6A6Au, 2.0f);
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char text[600]{};
            _snprintf_s(
                text, sizeof(text), _TRUNCATE,
                "Aurelion Sol OTP | %s | %s | Stardust %d%s | R %d/75%s | Q body %d burst %.2f | W %.0f",
                PostureName(CurrentPosture), SequenceName(ActiveSequence),
                Stardust(), StardustTelemetryReliable ? " observed" : " estimated",
                Calamity.Progress, (RuntimeEmpoweredR() || Calamity.Ready)
                    ? " READY" : "",
                QFirstBodyId, QContact.ContinuousSeconds,
                LastWPlan.Valid ? LastWPlan.Score : 0.0f);
            Drawing::DrawText(screen.x - 310.0f, screen.y - 120.0f,
                              0xFFB6ECFFu, text);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "AurelionSolOneTrick", "Aurelion Sol one-trick mechanics"));
    TacticsMenu->Add(new MenuBool(
        "ProtectWinCondition", "Front-to-back: peel the", true));
    TacticsMenu->Add(new MenuSlider(
        "AllyPeelHealth", "Peel ally below health (%)", 52, 10, 90));
    TacticsMenu->Add(new MenuBool(
        "KillSecure", "Exact Q/R kill", true));
    TacticsMenu->Add(new MenuSeparator(
        "Ownership",
        "Movement, cursor steering,"));

    BreathMenu = TacticsMenu->AddSubMenu(new Menu(
        "BreathOfLight", "Q first-body and continuous burst discipline"));
    BreathMenu->Add(new MenuBool(
        "Combo", "Channel with a verified", true));
    BreathMenu->Add(new MenuBool(
        "Harass", "Pressure lane after blocker", true));
    BreathMenu->Add(new MenuBool(
        "SplashTap", "Q splash tap wave", true));
    BreathMenu->Add(new MenuSlider(
        "SplashTapMana", "Min mana splash tap (%)", 72, 0, 100));
    BreathMenu->Add(new MenuSlider(
        "HarassMana", "Minimum mana for Q harass (%)", 52, 0, 100));
    BreathMenu->Add(new MenuSlider(
        "FlatReserve", "Mana after channel", 45, 0, 250));
    BreathMenu->Add(new MenuBool(
        "ProtectChannel", "Block AA during Q", true));
    BreathMenu->Add(new MenuSeparator(
        "Steering",
        "The controller chooses"));

    FlightMenu = TacticsMenu->AddSubMenu(new Menu(
        "AstralFlight", "W offset routes, CC windows and reset exits"));
    FlightMenu->Add(new MenuBool(
        "Combo", "Use an offset flight during", true));
    FlightMenu->Add(new MenuBool(
        "OnlyAfterCCSpent", "Engage only after key CC is", true));
    FlightMenu->Add(new MenuBool(
        "ContinueActiveBreath", "Convert active Q into Q-W", true));
    FlightMenu->Add(new MenuBool(
        "PlanWithBreathSlow", "Half-speed routes W-Q", true));
    FlightMenu->Add(new MenuBool(
        "Flee", "Fly on a cursor-aligned", true));
    FlightMenu->Add(new MenuSlider(
        "MaximumRoute", "Max combat flight", 1350, 650, 2500));
    FlightMenu->Add(new MenuSlider(
        "MinimumRouteScore", "Minimum sampled W route score", 180, -500, 2500));

    SingularityMenu = TacticsMenu->AddSubMenu(new Menu(
        "Singularity", "E pull, execute, line opening and Stardust"));
    SingularityMenu->Add(new MenuBool(
        "Combo", "E on target exit", true));
    SingularityMenu->Add(new MenuBool(
        "FlightDrop", "Drop E from the safe side of W", true));
    SingularityMenu->Add(new MenuBool(
        "Harass", "E when champion and stack", true));
    SingularityMenu->Add(new MenuSlider(
        "HarassMana", "Minimum mana for harass E (%)", 72, 0, 100));
    SingularityMenu->Add(new MenuSlider(
        "HarassMinimumStacks", "Expected E Stardust for harass", 3, 1, 10));
    SingularityMenu->Add(new MenuBool(
        "AntiGapcloser", "E on gapcloser endpoint", true));
    SingularityMenu->Add(new MenuBool(
        "InterruptZone", "E to disrupt a long channel", true));
    SingularityMenu->Add(new MenuBool(
        "AllyPeel", "W pull diver from carry", true));
    SingularityMenu->Add(new MenuBool(
        "Flee", "Drop E between Aurelion Sol", true));

    StarMenu = TacticsMenu->AddSubMenu(new Menu(
        "FallingStar", "R normal/Skies Descend impact planner"));
    StarMenu->Add(new MenuBool(
        "Combo", "R inside E/multi window", true));
    StarMenu->Add(new MenuSlider(
        "RegularMinimumHits", "Minimum regular R direct hits", 2, 1, 5));
    StarMenu->Add(new MenuSlider(
        "EmpoweredMinimumHits", "Min empow R hits", 2, 1, 5));
    StarMenu->Add(new MenuBool(
        "ReserveForObjective", "Preserve Skies Descend for", true));
    StarMenu->Add(new MenuBool(
        "Interrupt", "R when its real impact beats", true));
    StarMenu->Add(new MenuBool(
        "AntiGapcloser", "Spend R for critical self peel", true));
    StarMenu->Add(new MenuBool(
        "AllyPeel", "R to protect carry", true));
    StarMenu->Add(new MenuBool(
        "KillSecure", "Exact mitigated dmg", true));
    StarMenu->Add(new MenuBool(
        "Flee", "R before W: peel only", true));
    StarMenu->Add(new MenuSlider(
        "PeelHealth", "Self-peel R below health (%)", 48, 10, 90));
    StarMenu->Add(new MenuKeyBind(
        "ManualR", "Manual R toward enemy nearest cursor [T]",
        SDK::Keys::T, KeyBindType::Press));
    StarMenu->Add(new MenuSeparator(
        "WallImpact",
        "Wind Wall, Blade Whirl and"));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Cannon-wave and jungle Stardust discipline"));
    FarmMenu->Add(new MenuBool(
        "HoldForChampion", "Do not spend farm spells", true));
    FarmMenu->Add(new MenuSlider(
        "ChampionHoldRange", "Champion contest range", 1150, 500, 1800));
    FarmMenu->Add(new MenuBool(
        "UseELane", "E on valuable dying wave", true));
    FarmMenu->Add(new MenuBool(
        "UseQLane", "Focus cannon/large body so", true));
    FarmMenu->Add(new MenuSlider(
        "ELaneStacks", "Expected lane E Stardust", 6, 1, 10));
    FarmMenu->Add(new MenuSlider(
        "ELaneUnits", "Minimum lane units in E", 5, 1, 10));
    FarmMenu->Add(new MenuSlider(
        "LaneMana", "Min mana lane (%)", 62, 0, 100));
    FarmMenu->Add(new MenuBool(
        "UseEJungle", "E on durable camps", true));
    FarmMenu->Add(new MenuBool(
        "UseQJungle", "Hold Q jungle body", true));
    FarmMenu->Add(new MenuSlider(
        "EJungleStacks", "Expected jungle E Stardust", 3, 1, 10));
    FarmMenu->Add(new MenuSlider(
        "EJungleUnits", "Minimum jungle units in E", 2, 1, 8));
    FarmMenu->Add(new MenuSlider(
        "JungleMana", "Min mana jungle (%)", 38, 0, 100));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "One-trick geometry and state visualization"));
    CoachMenu->Add(new MenuBool(
        "DrawRanges", "Draw Q/E/R ranges", true));
    CoachMenu->Add(new MenuBool(
        "DrawBeam", "Draw beam/first body", true));
    CoachMenu->Add(new MenuBool(
        "DrawFlight", "Draw W route", true));
    CoachMenu->Add(new MenuBool(
        "DrawSingularity", "Draw E dmg/execute", true));
    CoachMenu->Add(new MenuBool(
        "DrawStar", "Draw actual R impact after", true));
    CoachMenu->Add(new MenuBool(
        "DrawPeel", "Draw ally/diver", true));
    CoachMenu->Add(new MenuBool(
        "DrawState", "Draw Stardust, R cycle and", true));
}

inline void OnLoad() {
    ActiveSequence = Sequence::None;
    CurrentPosture = Posture::Neutral;
    LastQPurpose = QPurpose::None;
    LastWPurpose = WPurpose::None;
    LastEPurpose = EPurpose::None;
    LastRPurpose = RPurpose::None;
    LastKnownMode = Mode::None;
    ObservedStardust = PredictedStardust = 0;
    StardustTelemetryReliable = false;
    LastStardustObservationTick = 0;
    Calamity = {};
    CalamityTelemetryReliable = false;
    ClearQState();
    ClearWState();
    EActive = EControllerOwned = false;
    ECastTick = EExpireTick = ETargetId = 0;
    ECenter = {};
    RCastTick = RImpactTick = 0;
    RImpactCenter = {};
    LastRWasEmpowered = false;
    RStardustResolutionPending = false;
    LastWReadyTick = LastWTakedownResetTick = 0;
    LastWCooldownSeconds = 0.0f;
    LastWCooldownSampleTick = 0;
    WCooldownObserved = false;
    LastChampionContactTick = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEnd = {};
    InterruptTargetId = InterruptExpireTick = 0;
    IncomingThreatTargetId = IncomingThreatUntil = 0;
    IncomingHardCrowdControl = false;
    ProtectedAllyId = PeelThreatId = 0;
    EnemyWindows.fill({});
    LastQPlan = {};
    LastWPlan = {};
    LastEPlan = {};
    LastRPlan = {};
    RefreshRuntimeState();
}

inline void OnUnload() {
    TacticsMenu = BreathMenu = FlightMenu = SingularityMenu = nullptr;
    StarMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Pin Summoner's Rift mechanics to Riot 26.14 and CommunityDragon PC 16.14",
    "Keep Riot 25.22 Q burst at 60/70/80/90/100 plus thirty percent AP",
    "Keep current 340 base movement speed in Astral Flight route timing",
    "Reject Arena and ARAM Mayhem mode overrides from Summoner's Rift arithmetic",
    "Reject every stale pre-CGU local Aurelion Sol Q/R database entry",
    "Scale Q and grounded E from 750 at level one to 920 at level eighteen",
    "Use the dedicated 1100 E cast range while Astral Flight is active",
    "Treat Q as a 140-wide first-body beam rather than a projectile skillshot",
    "Never reject Q because of Yasuo, Samira or Mel projectile walls",
    "Order Q collisions by capsule entry instead of unit-center distance",
    "Let a large offset jungle monster intercept before a closer-center minion",
    "Reject a champion hidden behind a living first-body minion",
    "Generate small angular Q candidates that clip the champion around a blocker",
    "Allow a deliberate short splash tap when the champion is beside the first minion",
    "Keep splash taps separate from full one-second burst commitments",
    "Use fifty percent splash damage only for secondary-body valuation",
    "Reset Q burst progress immediately when the first body changes",
    "Reset Q burst progress immediately when beam contact is lost",
    "Carry fractional continuous contact only on the same first body",
    "Grant exactly two predicted Stardust for each current champion Q burst",
    "Read passive buff telemetry before trusting event-based Stardust estimates",
    "Expose estimated versus observed Stardust confidence to the player",
    "Use 45/60/75/90/105 plus fifty-five percent AP Q damage per second",
    "Use 0.031 percent target max health per Stardust on Q bursts",
    "Cap only the Q percent-health component at 300 against monsters",
    "Use Q initial mana and continuous mana as distinct costs",
    "Reserve 30/35/40/45/50 initial Q mana by rank",
    "Reserve 35/40/45/50/55 Q mana per full second by rank",
    "Use the one-second lockout when a deliberate Q tap ends before 0.25 seconds",
    "Use the normal three-second cooldown after a substantive stationary channel",
    "Remove Q maximum duration at rank five",
    "Remove Q maximum duration during Astral Flight at every rank",
    "Never start stationary Q during a valuable player attack windup",
    "Never start Q against the player's cursor direction",
    "Never move the cursor to steer an active Q",
    "Never issue a movement command to keep Q connected",
    "Never issue an attack merely to begin a spell sequence",
    "Release only controller-owned Q after an unrecoverable blocker change",
    "Never release a player-owned manual Q because the controller dislikes its target",
    "Release controller-owned Q before a known incoming hard-CC line arrives",
    "Release controller-owned Q when an uncontrolled melee threat reaches dragon range",
    "Hold Q through a brief recoverable contact wobble when the next burst is near",
    "Stop spending Q mana once the configured flat reserve would be broken",
    "Block attacks that would accidentally break an active protected Q channel",
    "Let the player disable protected-channel attack blocking",
    "Use Q level-one lane pressure after the opponent spends their key spell",
    "Recognize that Q start is stronger than passive level-one E stacking in many lanes",
    "Walk no path and exploit no chest-over-minion collision bug automatically",
    "Use E pull and minion deaths to open a legitimate Q line instead",
    "Focus the cannon so Q splash helps finish an E wave",
    "Treat being alive and dodging CC as more valuable than one extra Q stack",
    "Scale W range by exactly 7.5 units for every Stardust",
    "Use 1500 base W range at zero Stardust",
    "Use 340 plus bonus move speed for ordinary flight sampling",
    "Halve sampled flight speed while planning W-Q",
    "Increase flat Q damage by eight through twelve percent during W",
    "Keep W reset eligibility tied to the three-second recent-damage window",
    "Model a qualifying takedown as ninety percent remaining W cooldown refund",
    "Wait through W's first 0.5 seconds before requesting a stop recast",
    "Reject W while grounded, rooted, stunned, knocked up or suppressed",
    "Prefer an offset W line that preserves Q range over flying directly into melee",
    "Sample every proposed W route instead of checking only its endpoint",
    "Penalize enemy turret exposure at every W route sample",
    "Penalize ready point-click lockdown at every W route sample",
    "Penalize Poppy, Taliyah and Cassiopeia anti-dash zones",
    "Reward terrain separation when the route keeps artillery distance",
    "Reward allied follow-up along the route rather than only at takeoff",
    "Require the selected champion to remain the first Q body along W-Q samples",
    "Hold automatic aggressive W until key crowd control was spent by default",
    "Open W immediately on a hard-controlled target when the sampled line is safe",
    "Allow W-Q punishment after an observed hard-CC spell is spent",
    "Convert an already held Q into Q-W when the target leaves stationary range",
    "Do not cancel Q before W when continuous breath is the chosen combo",
    "Use W-E-R-Q only from a safe sampled side angle",
    "Use W-E-Q when R value is insufficient",
    "Use W-Q without E when the short punish window would close",
    "Stop W before the planned endpoint enters an enemy turret",
    "Stop W before entering ready point-click lockdown",
    "Stop W when the player's cursor reverses strongly against the route",
    "Stop W after a reset escape reaches a safe no-enemy pocket",
    "Use a takedown reset to reposition or exit rather than blindly diving deeper",
    "Never Flash to repair a bad W angle",
    "Keep player movement ownership before, during and after W",
    "Use current E 90 mana and 12-second cooldown runtime telemetry",
    "Account for E's 0.2-second cast and 0.5-second appearance delay",
    "Keep E active for five seconds",
    "Use 10/15/20/25/30 plus twelve percent AP damage per second",
    "Use sixty percent total AP over a full five-second E",
    "Scale E outer radius by preserving area with 900 area units per Stardust",
    "Use live 275 outer starting radius",
    "Use live 120 inner starting radius and 180 inner area per Stardust",
    "Count target radius for E damage fringe",
    "Use target center only for E pull and execute-center checks",
    "Execute below five percent plus 0.026 percentage points per Stardust",
    "Never execute epic monsters with E",
    "Let a spell shield block one E damage tick rather than the whole field plan",
    "Count one Stardust for each full champion second inside E",
    "Count one Stardust when a small minion or small monster dies inside E",
    "Count two Stardust when a cannon or super minion dies inside E",
    "Count two Stardust when a large monster, champion or epic monster dies inside E",
    "Model a complete cannon wave as eight E Stardust",
    "Require expected deaths instead of casting E on a healthy passive wave",
    "Raise E value when champion control and wave deaths overlap",
    "Place E slightly ahead of a fleeing target's path",
    "Place E behind a diver to pull it away from the protected carry",
    "E on gapcloser endpoint",
    "Use E to disrupt follow-up even when R cannot land before a short channel ends",
    "Use E on objective chokes without pretending epic monsters are executable",
    "Never cast farm E while an enemy champion contests the wave by default",
    "Use separate mana thresholds for lane and jungle harvesting",
    "Require six expected lane Stardust by default",
    "Require five lane bodies by default",
    "Prioritize cannon and super minions as Q/E farm primaries",
    "Prioritize epic monsters over ordinary jungle bodies",
    "Never use W or R merely to collect farm Stardust",
    "Track 75 post-R-learning Stardust toward The Skies Descend",
    "Prefer runtime R2 name or ready buff over predicted Calamity progress",
    "Clamp predicted Calamity progress at exactly 75",
    "Reset Calamity progress only after an empowered R is consumed",
    "Do not reset Calamity progress after a regular Falling Star",
    "Use 1250 target range for both R forms",
    "Use 1.25 seconds for regular Falling Star impact prediction",
    "Use two seconds for The Skies Descend impact prediction",
    "Scale regular R radius with the same 275 and 900-area formula as E outer radius",
    "Double empowered R base area rather than simply doubling radius",
    "Add 1500 empowered R area units per Stardust",
    "Use 150/250/350 plus seventy-five percent AP regular direct damage",
    "Increase empowered direct R damage by exactly twenty-five percent",
    "Use ninety percent of regular R damage for empowered shockwave damage",
    "Stun regular direct victims for the current one-second control window",
    "Knock up empowered direct victims rather than treating it as a stun variant",
    "Keep the empowered shockwave radius fixed at 5000",
    "Expand the empowered shockwave over three seconds",
    "Damage only champions and epic monsters with the shockwave",
    "Slow all shockwave enemies even when they are not damage-eligible",
    "Never double-count direct impact victims as shockwave damage victims",
    "Grant five Stardust for every unshielded champion directly hit by R",
    "Reject an ordinary one-target R into a spell shield",
    "Allow multi-target R value even when one secondary shield is present",
    "Layer R inside active E so the pull reduces exits",
    "Generate pair-midpoint R centers for clustered teamfights",
    "Use front-to-back R to peel the primary threat from the allied win condition",
    "Use regular R as an interrupt only when 1.25-second impact beats channel expiry",
    "Use empowered R as an interrupt only when its two-second impact beats channel expiry",
    "Use exact mitigated R damage for kill secure",
    "Preserve empowered R for an objective fight by default",
    "Spend empowered R on a committed threat when protecting the win condition",
    "Spend empowered R for objective shockwave value even with fewer direct hits",
    "Do not spend empowered R on an empty cinematic shockwave",
    "Treat Yasuo Wind Wall as an R impact relocation rather than deletion",
    "Treat Samira Blade Whirl as an R impact relocation rather than deletion",
    "Treat Mel Rebuttal as an R impact relocation rather than deletion",
    "Find the first projectile-barrier contact with the shared helper",
    "Evaluate direct R coverage at the relocated barrier contact",
    "Keep empowered shockwave evaluation after projectile interception",
    "Reject a wall-relocated R that no longer directly reaches its required primary",
    "Draw requested versus actual intercepted impact information for coaching",
    "Let manual R choose the enemy nearest the player's cursor",
    "Still enforce actual projectile-wall impact geometry for manual R",
    "Yield after every observed manual spell through shared engine arbitration",
    "Observe manual Q without taking ownership of its release",
    "Observe manual W and respect the player's own stop recast",
    "Observe manual E center for follow-up Q/R decisions",
    "Observe manual empowered R and reset the 75-stack cycle",
    "Prioritize manual R, flight safety, reactions and peel before ordinary damage",
    "Prioritize self-preservation over one extra continuous Q burst",
    "Prioritize the fed allied carry over a speculative backline W dive",
    "Never issue summoner spells",
    "Never issue attack-move commands",
    "Never select a different orbwalker target for the player",
    "Never move toward a speculative E or R hit",
    "Expose live Q first body and partial burst time",
    "Expose sampled W route score and destination",
    "Expose E outer and execute-center radii separately",
    "Expose observed or estimated Stardust and Calamity confidence",
    "Own the full decision loop and never fall back to generic Q-W-E-R priority",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "AurelionSol";
    controller.ControllerId = "champion.kuroaio.ai.aurelionsol.controller";
    controller.KitRevision = "Riot 26.14 / CommunityDragon PC 16.14";
    controller.ResearchArtifact = "AI/Research/AIAurelionSol.md";
    controller.ImplementationSummary =
        "First-body continuous Q, sampled offset/reset W, Stardust/execute E, "
        "75-stack dual-form R with projectile-barrier impact relocation and "
        "player-owned movement/cursor/attacks.";
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
            &ObserveLocalSpell, &RecordEnemySpell>;
    controller.OnDoCast = &OnDoCast;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &OnBuffUpdate;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &OnInterruptable;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::AurelionSol