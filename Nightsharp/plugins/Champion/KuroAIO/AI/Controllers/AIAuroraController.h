#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIAuroraGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Aurora {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::CursorDirectionAgrees;
using ControllerHelpers::HasAnyBuff;
using ControllerHelpers::HasCurrentResource;
using ControllerHelpers::HasEnemyChampionNear;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLargeLaneMinion;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MaximumBuffCount;
using ControllerHelpers::NearTerrain;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Ready;
using ControllerHelpers::RuntimeNameContains;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContainsAny;
using ControllerHelpers::SpellRank;
using ControllerHelpers::UnitByNetworkId;
using ControllerHelpers::ValidHostileUnitInGameplayRange;

enum class Sequence : std::uint8_t {
    None,
    LevelOneWeave,
    ShortTrade,
    ExtendedDoubleSpirit,
    MarkedWavePullback,
    StealthAngle,
    ResetReengage,
    ResetEscape,
    RecoilPeel,
    ArenaAllIn,
    PortalBuffer,
    FarmPullback,
    PlayerLed,
};

enum class Posture : std::uint8_t {
    Neutral,
    LaneTrade,
    ExtendedFight,
    Flank,
    FrontToBack,
    RecoilPeel,
    Arena,
    Farm,
    Flee,
};

enum class QPurpose : std::uint8_t {
    None,
    Manual,
    Trade,
    AllIn,
    PullbackLethal,
    PullbackPassive,
    MarkedWave,
    Jungle,
    FleeSlowSetup,
};

enum class WPurpose : std::uint8_t {
    None,
    Manual,
    FlankAngle,
    Dodge,
    TakedownReset,
    ResetEscape,
    WallHop,
    HiddenPortal,
    Flee,
};

enum class EPurpose : std::uint8_t {
    None,
    Manual,
    TradeProc,
    AllInFinish,
    StandalonePoke,
    RecoilPeel,
    DisplacementBuffer,
    PortalBuffer,
    MarkedWaveAfterQ2,
    Wave,
    Jungle,
};

enum class RPurpose : std::uint8_t {
    None,
    Manual,
    FollowupEngage,
    ArenaAllIn,
    CrowdControlBuffer,
    SelfPeel,
    MultiTarget,
    KillSecure,
};

struct QPlan {
    Vector3 Aim = {};
    int TargetId = 0;
    QPurpose Purpose = QPurpose::None;
    std::vector<QMark> ExpectedMarks = {};
    int ChampionHits = 0;
    int MarkedWaveHits = 0;
    float Score = -FLT_MAX;
    bool ProjectileBlocked = false;
    bool Valid = false;
};

struct WPlan {
    Vector3 CastPosition = {};
    Vector3 Endpoint = {};
    int TargetId = 0;
    WPurpose Purpose = WPurpose::None;
    WRouteContext Context = {};
    float Score = -FLT_MAX;
    bool CrossesWall = false;
    bool Valid = false;
};

struct EPlan {
    Vector3 Aim = {};
    Vector3 RecoilEndpoint = {};
    int TargetId = 0;
    EPurpose Purpose = EPurpose::None;
    std::vector<int> HitIds = {};
    int ChampionHits = 0;
    int FarmHits = 0;
    int MarkedFarmKills = 0;
    float Score = -FLT_MAX;
    bool GroundedOrRooted = false;
    bool Valid = false;
};

struct RPlan {
    Vector3 CastPosition = {};
    RPlacement Placement = {};
    REvaluation Evaluation = {};
    std::vector<int> HitIds = {};
    int TargetId = 0;
    RPurpose Purpose = RPurpose::None;
    float Score = -FLT_MAX;
    bool TerrainFeasible = false;
    bool LeapEndpointSafe = false;
    bool Defensive = false;
    bool Valid = false;
};

struct PassiveRecord {
    int NetworkId = 0;
    PassiveState State = {};
    int ExpireTick = 0;
    bool DoubledTelemetry = false;
    bool TelemetryReliable = false;
};

struct TrackedMark {
    int NetworkId = 0;
    Vector3 LastPosition = {};
    float Radius = 0.0f;
    int ExpireTick = 0;
    bool Champion = false;
    bool Minion = false;
    bool Confirmed = false;
};

struct EnemyWindow {
    int NetworkId = 0;
    int CommittedUntil = 0;
    int HardCrowdControlSpentUntil = 0;
};

inline Menu* TacticsMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* HexMenu = nullptr;
inline Menu* VeilMenu = nullptr;
inline Menu* WeirdingMenu = nullptr;
inline Menu* WorldsMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Sequence ActiveSequence = Sequence::None;
inline Posture CurrentPosture = Posture::Neutral;
inline QPurpose LastQPurpose = QPurpose::None;
inline WPurpose LastWPurpose = WPurpose::None;
inline EPurpose LastEPurpose = EPurpose::None;
inline RPurpose LastRPurpose = RPurpose::None;
inline Mode LastKnownMode = Mode::None;

inline std::array<PassiveRecord, 10> PassiveRecords = {};
inline std::array<TrackedMark, 48> QMarks = {};
inline std::array<EnemyWindow, 10> EnemyWindows = {};
inline std::array<int, kMaximumSpirits> SpiritExpireTicks = {};
inline int PredictedSpiritCount = 0;

inline bool QActive = false;
inline bool QControllerOwned = false;
inline int QCastTick = 0;
inline int QExpireTick = 0;
inline int QPrimaryTargetId = 0;
inline int QLastObservedTick = 0;
inline int Q2LastTick = 0;

inline bool WInvisible = false;
inline bool WRealmHopper = false;
inline bool WControllerOwned = false;
inline int WCastTick = 0;
inline int WInvisibleUntil = 0;
inline int WRealmHopperUntil = 0;
inline int LastChampionDamageTick = 0;
inline float LastWCooldownSeconds = 0.0f;
inline int LastWCooldownSampleTick = 0;
inline int LastWResetTick = 0;
inline bool WCooldownObserved = false;

inline int ECastTick = 0;
inline int ERecoilUntil = 0;
inline Vector3 LastERecoilEndpoint = {};

inline bool RArenaActive = false;
inline bool RControllerOwned = false;
inline int RCastTick = 0;
inline int RArenaExpireTick = 0;
inline int RPortalReadyTick = 0;
inline int RLastPortalTick = 0;
inline Vector3 RArenaCenter = {};
inline Vector3 LastPortalDestination = {};
inline Vector3 LastPlayerPosition = {};
inline int LastPlayerPositionTick = 0;

inline int LastAutoTargetId = 0;
inline int LastAutoCastTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int IncomingThreatTargetId = 0;
inline int IncomingThreatUntil = 0;
inline bool IncomingOneInstanceCrowdControl = false;
inline bool IncomingPersistentCrowdControl = false;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};

inline QPlan LastQPlan = {};
inline WPlan LastWPlan = {};
inline EPlan LastEPlan = {};
inline RPlan LastRPlan = {};

inline float TargetPriority(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const float offense = target.TotalAttackDamage() * 0.0042f +
                          target.AP() * 0.0042f;
    const float range = std::clamp(
        target.AttackRange() / 700.0f, 0.0f, 1.45f);
    const float wounded = (100.0f - target.HealthPercent()) * 0.012f;
    return 0.85f + offense + range + wounded;
}

inline bool GroundedOrRooted() {
    const auto player = ObjectManager::Player();
    return player.IsValid() &&
        (SDK::HasBuffOfType(player, SDK::BuffType::Grounded) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Snare));
}

inline bool IsQEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::Q, {
            "auroraq", "twofoldhex",
        });
}

inline bool IsQ2Event(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellEventNameContainsAny(args, {
        "auroraqrecast", "auroraq2", "twofoldhexrecast",
        "auroraqreturn",
    });
}

inline bool IsWEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::W, {
            "auroraw", "acrosstheveil",
        });
}

inline bool IsEEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::E, {
            "aurorae", "theweirding",
        });
}

inline bool IsREvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::R, {
            "aurorar", "betweenworlds",
        });
}

inline bool IsR2Event(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellEventNameContainsAny(args, {
        "aurorarr", "aurorarecast", "aurorarrecast",
        "betweenworldsrecast",
    });
}

inline bool IsQMarkBuff(const char* name) {
    return ControllerHelpers::TextContainsAny(name, {
        "auroraqdebuff", "auroraqmark", "auroraqtarget",
        "twofoldhex",
    });
}

inline bool IsPassiveMarkBuff(const char* name) {
    return ControllerHelpers::TextContainsAny(name, {
        "aurorapassive", "aurorapassivemanager", "spirittaker",
        "spiritabjuration",
    });
}

inline bool IsWInvisibleBuff(const char* name) {
    return ControllerHelpers::TextContainsAny(name, {
        "aurorawstealth", "aurorawinvis", "acrosstheveil",
    });
}

inline bool IsRealmHopperBuff(const char* name) {
    return ControllerHelpers::TextContainsAny(name, {
        "aurorawbuff", "aurorarealmhopper", "realmbunnyhop",
    });
}

inline bool IsRArenaBuff(const char* name) {
    return ControllerHelpers::TextContainsAny(name, {
        "aurorar", "auroraractive", "betweenworlds",
    });
}

inline bool RuntimeQRecast() {
    return RuntimeNameContains(0, "auroraqrecast") ||
           RuntimeNameContains(0, "auroraq2") ||
           RuntimeNameContains(0, "twofoldhexrecast");
}

inline bool RuntimeRRecast() {
    return RuntimeNameContains(3, "aurorarecast") ||
           RuntimeNameContains(3, "aurorarrecast") ||
           RuntimeNameContains(3, "betweenworldsrecast");
}

inline PassiveRecord* FindPassiveRecord(int networkId,
                                        bool create = false) {
    if (networkId == 0) return nullptr;
    for (auto& record : PassiveRecords) {
        if (record.NetworkId == networkId) return &record;
    }
    if (!create) return nullptr;
    for (auto& record : PassiveRecords) {
        if (record.NetworkId == 0 || record.ExpireTick < Now()) {
            record = {};
            record.NetworkId = networkId;
            return &record;
        }
    }
    return nullptr;
}

inline EnemyWindow* FindEnemyWindow(int networkId,
                                    bool create = false) {
    return ControllerHelpers::FindEnemyCastWindow(
        EnemyWindows, networkId, create);
}

inline void AddSpirit() {
    const int now = Now();
    int slot = -1;
    int oldest = 0;
    for (int index = 0; index < kMaximumSpirits; ++index) {
        if (SpiritExpireTicks[static_cast<std::size_t>(index)] <= now) {
            slot = index;
            break;
        }
        if (index == 0 ||
            SpiritExpireTicks[static_cast<std::size_t>(index)] <
                SpiritExpireTicks[static_cast<std::size_t>(oldest)]) {
            oldest = index;
        }
    }
    if (slot < 0) slot = oldest;
    SpiritExpireTicks[static_cast<std::size_t>(slot)] = now +
        static_cast<int>(kSpiritSeconds * 1000.0f);
}

inline void RefreshSpirits() {
    PredictedSpiritCount = 0;
    for (const int expiry : SpiritExpireTicks) {
        if (expiry > Now()) ++PredictedSpiritCount;
    }
}

inline void ObservePassiveApplication(int networkId, int applications = 1) {
    PassiveRecord* record = FindPassiveRecord(networkId, true);
    if (!record || applications <= 0) return;
    const int oldProcs = record->State.Procs;
    record->State = AdvancePassive(record->State, applications, 0.0f);
    record->ExpireTick = Now() + static_cast<int>(
        kPassiveMarkSeconds * 1000.0f);
    if (record->State.Procs > oldProcs) AddSpirit();
}

inline int PassiveStacksOn(int networkId) {
    const PassiveRecord* record = FindPassiveRecord(networkId);
    return record && record->ExpireTick >= Now()
        ? record->State.Stacks : 0;
}

inline int PassiveProcsOn(int networkId) {
    const PassiveRecord* record = FindPassiveRecord(networkId);
    return record ? record->State.Procs : 0;
}

inline TrackedMark* FindMark(int networkId, bool create = false) {
    if (networkId == 0) return nullptr;
    for (auto& mark : QMarks) {
        if (mark.NetworkId == networkId) return &mark;
    }
    if (!create) return nullptr;
    for (auto& mark : QMarks) {
        if (mark.NetworkId == 0 || mark.ExpireTick < Now()) {
            mark = {};
            mark.NetworkId = networkId;
            return &mark;
        }
    }
    return nullptr;
}

inline void TrackMark(int networkId,
                      const Vector3& position,
                      float radius,
                      bool champion,
                      bool minion,
                      int expireTick,
                      bool confirmed = false) {
    TrackedMark* mark = FindMark(networkId, true);
    if (!mark) return;
    mark->LastPosition = position;
    mark->Radius = std::max(0.0f, radius);
    mark->ExpireTick = expireTick;
    mark->Champion = champion;
    mark->Minion = minion;
    mark->Confirmed = mark->Confirmed || confirmed;
}

inline void RemoveMark(int networkId) {
    if (TrackedMark* mark = FindMark(networkId)) *mark = {};
}

inline void ClearMarks() {
    QMarks.fill({});
}

inline void RefreshMarks() {
    const int now = Now();
    for (auto& mark : QMarks) {
        if (mark.NetworkId == 0) continue;
        const AIBaseClient unit = UnitByNetworkId(mark.NetworkId);
        if (mark.ExpireTick < now || !unit.IsValid() || unit.IsDead()) {
            mark = {};
            continue;
        }
        mark.LastPosition = unit.Position();
        mark.Radius = unit.BoundingRadius();
    }
}

inline int ActiveMarkCount(bool championsOnly = false) {
    int count = 0;
    for (const auto& mark : QMarks) {
        if (mark.NetworkId != 0 && mark.ExpireTick >= Now() &&
            (!championsOnly || mark.Champion)) ++count;
    }
    return count;
}

inline std::vector<QMark> CurrentMarks() {
    std::vector<QMark> result;
    result.reserve(QMarks.size());
    for (const auto& mark : QMarks) {
        if (mark.NetworkId == 0 || mark.ExpireTick < Now()) continue;
        result.push_back({
            mark.LastPosition,
            mark.Radius,
            mark.NetworkId,
            static_cast<float>(mark.ExpireTick - Now()) / 1000.0f,
            mark.Champion,
            mark.Minion,
            true,
        });
    }
    return result;
}

inline void ClearQState(bool clearMarks = true) {
    QActive = false;
    QControllerOwned = false;
    QCastTick = QExpireTick = QPrimaryTargetId = 0;
    if (clearMarks) ClearMarks();
}

inline float Q1Damage(const AIBaseClient& target) {
    const auto player = ObjectManager::Player();
    return target.IsValid() && player.IsValid()
        ? player.CalculateMagicDamage(
              target, QBaseDamage(SpellRank(0), player.AP()))
        : 0.0f;
}

inline float Q2Damage(const AIBaseClient& target,
                      float damageUnits = 1.0f) {
    const auto player = ObjectManager::Player();
    if (!target.IsValid() || !player.IsValid()) return 0.0f;
    const float missing = target.MaxHealth() > 0.0f
        ? 1.0f - target.Health() / target.MaxHealth() : 0.0f;
    const float raw = Q2BoltRawDamage(
        SpellRank(0), player.AP(), missing) * std::max(0.0f, damageUnits);
    return player.CalculateMagicDamage(target, raw);
}

inline float EDamage(const AIBaseClient& target) {
    const auto player = ObjectManager::Player();
    return target.IsValid() && player.IsValid()
        ? player.CalculateMagicDamage(
              target, ERawDamage(SpellRank(2), player.AP()))
        : 0.0f;
}

inline float RDamage(const AIBaseClient& target) {
    const auto player = ObjectManager::Player();
    return target.IsValid() && player.IsValid()
        ? player.CalculateMagicDamage(
              target, RRawDamage(SpellRank(3), player.AP()))
        : 0.0f;
}

inline float PassiveDamage(const AIBaseClient& target) {
    const auto player = ObjectManager::Player();
    return target.IsValid() && player.IsValid()
        ? player.CalculateMagicDamage(
              target, PassiveRawDamage(
                          target.MaxHealth(), player.AP(), false,
                          player.Level()))
        : 0.0f;
}

inline float AutoDamage(const AIBaseClient& target) {
    const auto player = ObjectManager::Player();
    return target.IsValid() && player.IsValid()
        ? SDK::Damage::GetAutoAttackDamage(player, target, true)
        : 0.0f;
}

inline float ConservativeComboDamage(const AIHeroClient& target,
                                     bool includeR = true) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    float damage = AutoDamage(target);
    if (Ready(0) || QActive) damage += Q1Damage(target) + Q2Damage(target);
    if (Ready(2)) damage += EDamage(target);
    if (includeR && Ready(3) && !RArenaActive) damage += RDamage(target);
    damage += PassiveDamage(target) *
        static_cast<float>(PassiveStacksOn(
            static_cast<int>(target.NetworkId())) >= 1 ? 1 : 0);
    return damage;
}

inline std::vector<LineUnit> BuildLineUnits(float delaySeconds,
                                            bool includeFarm) {
    std::vector<LineUnit> result;
    result.reserve(52);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kQRange + 220.0f)) continue;
        result.push_back({
            PredictPosition(enemy, delaySeconds),
            enemy.BoundingRadius(),
            static_cast<int>(enemy.NetworkId()),
            true,
            false,
            !IsCommonUntargetableOrImmune(enemy),
        });
    }
    if (!includeFarm) return result;
    auto append = [&](const AIMinionClient& unit) {
        if (!ValidHostileUnitInGameplayRange(unit, kQRange + 120.0f) ||
            HasFlag(unit.GetMinionType(), MinionTypes::Ward) ||
            unit.IsPlant()) return;
        result.push_back({
            PredictPosition(unit, delaySeconds),
            unit.BoundingRadius(),
            static_cast<int>(unit.NetworkId()),
            false,
            true,
            true,
        });
    };
    for (const auto& minion : GameObjects::EnemyMinions()) append(minion);
    for (const auto& monster : GameObjects::Jungle()) append(monster);
    return result;
}

inline Vector3 ClampDirectionCast(const Vector3& requested, float range) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !requested.IsValid() || requested.IsZero()) {
        return {};
    }
    const Vector3 direction = SharedGeometry::Direction2D(
        player.Position(), requested);
    return direction.IsZero() ? Vector3{} : player.Position() +
        direction * std::max(0.0f, range);
}

inline QPlan BuildQPlan(const AIBaseClient& primary,
                        QPurpose purpose,
                        bool includeFarm = false,
                        bool reactive = false) {
    QPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !primary.IsValid() ||
        SpellRank(0) <= 0 || RuntimeQRecast()) return best;
    const int primaryId = static_cast<int>(primary.NetworkId());
    const float travel = player.Position().Distance2D(primary.Position()) /
        kQOutboundSpeed;
    const float delay = kQCastSeconds + travel;
    const auto units = BuildLineUnits(delay, includeFarm);
    Vector3 primaryPosition = PredictPosition(primary, delay);
    Vector3 baseDirection = SharedGeometry::Direction2D(
        player.Position(), primaryPosition);
    if (baseDirection.IsZero()) return best;

    static constexpr std::array<float, 9> angles = {
        0.0f, -3.0f, 3.0f, -6.0f, 6.0f,
        -9.0f, 9.0f, -12.0f, 12.0f,
    };
    for (const float degrees : angles) {
        const Vector3 direction = SharedGeometry::Rotate2D(
            baseDirection, degrees * kPi / 180.0f);
        const Vector3 aim = player.Position() + direction * kQRange;
        if (ProjectileWallBlocksFromPlayer(aim, kQMissileHalfWidth)) {
            continue;
        }
        std::vector<QMark> expected;
        int championHits = 0;
        int farmHits = 0;
        bool primaryHit = false;
        float score = 0.0f;
        for (const auto& unit : units) {
            if (!QLineHits(player.Position(), aim, unit)) continue;
            const AIBaseClient live = UnitByNetworkId(unit.Id);
            const float arrival = kQCastSeconds +
                player.Position().Distance2D(unit.Position) /
                    kQOutboundSpeed;
            expected.push_back({
                unit.Position,
                unit.Radius,
                unit.Id,
                kQMarkSeconds + arrival,
                unit.Champion,
                unit.Minion,
                true,
            });
            primaryHit = primaryHit || unit.Id == primaryId;
            if (unit.Champion) {
                ++championHits;
                const AIHeroClient enemy = HeroByNetworkId(unit.Id);
                score += 245.0f * std::max(
                    0.6f, TargetPriority(enemy));
                if (PassiveStacksOn(unit.Id) == 2) score += 125.0f;
            } else {
                ++farmHits;
                score += 26.0f;
                const AIMinionClient minion = live.IsValid()
                    ? AIMinionClient(live.Address()) : AIMinionClient{};
                if (minion.IsValid() && IsLargeLaneMinion(minion)) {
                    score += 72.0f;
                }
            }
        }
        if (!primaryHit) continue;

        LineUnit returnTarget{
            primaryPosition,
            primary.BoundingRadius(),
            primaryId,
            primary.IsHero(),
            !primary.IsHero(),
            true,
        };
        const auto returnValue = EvaluateQReturn(
            returnTarget, expected, player.Position());
        if (returnValue.Score > -FLT_MAX * 0.5f) {
            score += returnValue.Score * 0.45f;
        }
        score += championHits * championHits * 30.0f;
        if (purpose == QPurpose::MarkedWave) score += farmHits * 20.0f;
        if (!reactive && !CursorDirectionAgrees(aim, -0.18f)) score -= 390.0f;
        if (Orbwalker::IsWindingUp()) score -= 280.0f;
        if (score > best.Score) {
            best.Aim = aim;
            best.TargetId = primaryId;
            best.Purpose = purpose;
            best.ExpectedMarks = std::move(expected);
            best.ChampionHits = championHits;
            best.MarkedWaveHits = farmHits;
            best.Score = score;
            best.ProjectileBlocked = false;
            best.Valid = true;
        }
    }
    return best;
}

inline void CommitExpectedMarks(const QPlan& plan) {
    const int now = Now();
    for (const auto& expected : plan.ExpectedMarks) {
        const int expiry = now + static_cast<int>(
            std::max(0.0f, expected.RemainingSeconds) * 1000.0f);
        TrackMark(expected.Id, expected.Position, expected.Radius,
                  expected.Champion, expected.Minion, expiry, false);
    }
}

inline bool CastQPlan(const QPlan& plan,
                      Mode mode,
                      bool reactive = false) {
    if (!plan.Valid || QActive || RuntimeQRecast() || !Ready(0) ||
        !SpellEnabled(0, mode) || !CastThrottleReady(0, 34,
            reactive ? 0 : -1) || !HasCurrentResource(SpellCost(0))) {
        return false;
    }
    if (Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true) && !reactive) {
        return false;
    }
    if (plan.ProjectileBlocked ||
        ProjectileWallBlocksFromPlayer(plan.Aim, kQMissileHalfWidth)) {
        return false;
    }
    if (!Engine::ControllerCastPosition(0, plan.Aim)) return false;
    QActive = true;
    QControllerOwned = true;
    QCastTick = QLastObservedTick = Now();
    QExpireTick = QCastTick + static_cast<int>(
        (kQCastSeconds + kQMarkSeconds) * 1000.0f);
    QPrimaryTargetId = plan.TargetId;
    CommitExpectedMarks(plan);
    for (const auto& mark : plan.ExpectedMarks) {
        ObservePassiveApplication(mark.Id);
    }
    if (plan.ChampionHits > 0) LastChampionDamageTick = Now();
    LastQPlan = plan;
    LastQPurpose = plan.Purpose;
    ActiveSequence = plan.Purpose == QPurpose::MarkedWave
        ? Sequence::MarkedWavePullback
        : (plan.Purpose == QPurpose::Trade
            ? Sequence::ShortTrade : Sequence::ExtendedDoubleSpirit);
    return true;
}

inline LineUnit ReturnTargetUnit(const AIBaseClient& target) {
    return {
        target.Position(),
        target.BoundingRadius(),
        static_cast<int>(target.NetworkId()),
        target.IsHero(),
        !target.IsHero(),
        target.IsValid() && !target.IsDead(),
    };
}

inline bool MarkedMinionWouldDieToE() {
    if (!Ready(2)) return false;
    for (const auto& mark : QMarks) {
        if (mark.NetworkId == 0 || !mark.Minion ||
            mark.ExpireTick < Now()) continue;
        const AIBaseClient unit = UnitByNetworkId(mark.NetworkId);
        if (unit.IsValid() && EDamage(unit) >= unit.Health()) return true;
    }
    return false;
}

inline AIBaseClient BestQReturnTarget() {
    AIBaseClient primary = UnitByNetworkId(QPrimaryTargetId);
    if (primary.IsValid() && !primary.IsDead()) return primary;
    float bestScore = -FLT_MAX;
    AIBaseClient best{};
    for (const auto& mark : QMarks) {
        if (mark.NetworkId == 0 || mark.ExpireTick < Now()) continue;
        const AIBaseClient unit = UnitByNetworkId(mark.NetworkId);
        if (!unit.IsValid() || unit.IsDead()) continue;
        float score = mark.Champion ? 1000.0f : 0.0f;
        score -= unit.Health() * 0.001f;
        if (mark.Champion) {
            score += TargetPriority(AIHeroClient(unit.Handle())) * 100.0f;
        }
        if (score > bestScore) {
            bestScore = score;
            best = unit;
        }
    }
    return best;
}

inline bool CastQ2(QPurpose purpose, bool reactive = false) {
    if (!QActive || !QControllerOwned || !RuntimeQRecast() || !Ready(0) ||
        !CastThrottleReady(0, 34, reactive ? 0 : -1)) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    for (const auto& mark : QMarks) {
        if (mark.NetworkId != 0 && mark.ExpireTick >= Now()) {
            ObservePassiveApplication(mark.NetworkId);
            if (mark.Champion) LastChampionDamageTick = Now();
        }
    }
    Q2LastTick = Now();
    LastQPurpose = purpose;
    if (purpose == QPurpose::PullbackPassive) {
        ActiveSequence = Sequence::ExtendedDoubleSpirit;
    } else if (purpose == QPurpose::MarkedWave) {
        ActiveSequence = Sequence::MarkedWavePullback;
    }
    ClearQState();
    return true;
}

inline bool ManageQRecast(Mode mode) {
    if (!QActive || !RuntimeQRecast()) return false;
    RefreshMarks();
    const auto player = ObjectManager::Player();
    const AIBaseClient target = BestQReturnTarget();
    if (!player.IsValid() || !target.IsValid()) {
        if (QExpireTick - Now() <= 160) {
            return CastQ2(QPurpose::MarkedWave, true);
        }
        return false;
    }

    const auto marks = CurrentMarks();
    const LineUnit returnTarget = ReturnTargetUnit(target);
    const auto current = EvaluateQReturn(
        returnTarget, marks, player.Position());
    QReturnEvaluation better = current;
    if (player.PathEnd().IsValid() && !player.PathEnd().IsZero() &&
        player.Position().Distance2D(player.PathEnd()) <= 360.0f) {
        better = EvaluateQReturn(returnTarget, marks, player.PathEnd());
    }
    const float remaining = static_cast<float>(
        std::max(0, QExpireTick - Now())) / 1000.0f;
    const bool targetChampion = target.IsHero();
    const float q2Damage = Q2Damage(target,
        std::max(1.0f, current.DamageUnits));
    const bool autoSafe = targetChampion && InAutoAttackRange(target) &&
        Engine::CountEnemiesAt(player.Position(), 650.0f) <=
            Engine::CountAlliesAt(player.Position(), 800.0f) + 1;
    const bool eWouldHit = Ready(2) && ELineHits(
        player.Position(), target.Position(), returnTarget);
    const bool markedWave = ActiveSequence == Sequence::MarkedWavePullback ||
        LastQPurpose == QPurpose::MarkedWave;
    QRecastContext context{};
    context.MarkActive = true;
    context.ControllerOwned = QControllerOwned;
    context.AutoAttackWindup = Orbwalker::IsWindingUp();
    context.TargetValid = target.IsValid() && !target.IsDead();
    context.TargetEscaping = targetChampion &&
        player.Position().Distance2D(target.Position()) > kQRange + 90.0f;
    context.LethalNow = q2Damage >= target.Health() + target.AllShield();
    context.PassiveProcNow = PassiveStacksOn(
        static_cast<int>(target.NetworkId())) == 2;
    context.EReady = Ready(2);
    context.EWouldHit = eWouldHit;
    context.EWouldKillMarkedWave = MarkedMinionWouldDieToE();
    context.WaveSequence = markedWave;
    context.CurrentBolts = current.Bolts;
    context.BetterPositionBolts = better.Bolts;
    context.RemainingSeconds = remaining;
    context.ExpectedPreRecastDamage = autoSafe ? AutoDamage(target) : 0.0f;
    if (!markedWave && eWouldHit) context.ExpectedPreRecastDamage += EDamage(target);

    if (!ShouldRecastQ(context)) return false;
    QPurpose purpose = markedWave
        ? QPurpose::MarkedWave
        : (context.LethalNow ? QPurpose::PullbackLethal
                             : QPurpose::PullbackPassive);
    return CastQ2(purpose, context.LethalNow || remaining <= 0.16f);
}

inline Vector3 ResolveWEndpoint(const Vector3& direction,
                                bool& crossesWall,
                                bool& reachable) {
    const auto player = ObjectManager::Player();
    crossesWall = false;
    reachable = false;
    if (!player.IsValid() || direction.IsZero()) return {};
    const Vector3 origin = player.Position();
    bool sawWall = false;
    Vector3 terrainExit{};
    for (float distance = 75.0f;
         distance <= kWWallExtension;
         distance += 25.0f) {
        const Vector3 sample = origin + direction * distance;
        if (SDK::NavMesh::IsWall(sample)) {
            sawWall = true;
            continue;
        }
        if (sawWall) {
            terrainExit = sample;
            break;
        }
    }
    crossesWall = sawWall && terrainExit.IsValid() && !terrainExit.IsZero();
    Vector3 endpoint = WResolvedEndpoint(
        origin, origin + direction * kWBaseDash,
        terrainExit, crossesWall);
    reachable = endpoint.IsValid() && !endpoint.IsZero() &&
                !SDK::NavMesh::IsWall(endpoint);
    return reachable ? endpoint : Vector3{};
}

inline WPlan BuildWPlan(const AIHeroClient& target,
                        WPurpose purpose,
                        bool defensive = false) {
    WPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || SpellRank(1) <= 0 || WInvisible ||
        PlayerMobilityLocked()) return best;
    const Vector3 cursorDirection = SharedGeometry::Direction2D(
        player.Position(), Game::CursorPos());
    Vector3 targetDirection = Engine::ValidEnemy(target)
        ? SharedGeometry::Direction2D(player.Position(), target.Position())
        : Vector3{};
    if (targetDirection.IsZero()) targetDirection = cursorDirection;
    if (targetDirection.IsZero()) return best;

    std::vector<Vector3> directions;
    directions.reserve(10);
    if (!cursorDirection.IsZero()) directions.push_back(cursorDirection);
    if (defensive && Engine::ValidEnemy(target)) {
        const Vector3 away = targetDirection * -1.0f;
        directions.push_back(away);
        directions.push_back(SharedGeometry::Rotate2D(away, 28.0f * kPi / 180.0f));
        directions.push_back(SharedGeometry::Rotate2D(away, -28.0f * kPi / 180.0f));
    } else {
        directions.push_back(SharedGeometry::Rotate2D(
            targetDirection, 52.0f * kPi / 180.0f));
        directions.push_back(SharedGeometry::Rotate2D(
            targetDirection, -52.0f * kPi / 180.0f));
        directions.push_back(SharedGeometry::Rotate2D(
            targetDirection, 76.0f * kPi / 180.0f));
        directions.push_back(SharedGeometry::Rotate2D(
            targetDirection, -76.0f * kPi / 180.0f));
    }

    const bool recentDamage = Now() - LastChampionDamageTick <=
        static_cast<int>(kWResetDamageWindowSeconds * 1000.0f);
    const bool takedownLikely = Engine::ValidEnemy(target) &&
        ConservativeComboDamage(target, true) >=
            target.Health() + target.AllShield();
    for (const Vector3& direction : directions) {
        if (direction.IsZero()) continue;
        bool crossesWall = false;
        bool reachable = false;
        const Vector3 endpoint = ResolveWEndpoint(
            direction, crossesWall, reachable);
        if (!reachable || endpoint.IsZero()) continue;
        const Vector3 endpointToTarget = Engine::ValidEnemy(target)
            ? SharedGeometry::Direction2D(endpoint, target.Position())
            : Vector3{};
        WRouteContext context{};
        context.EndpointValid = true;
        context.CursorAgrees = cursorDirection.IsZero() ||
            direction.Dot(cursorDirection) >= (defensive ? -0.18f : 0.05f);
        context.TerrainReachable = reachable;
        context.EnemyTurret = Engine::UnderEnemyTurret(endpoint);
        context.DashHazard = HasReadyDashHazardAt(endpoint);
        context.PointClickLockdown = HasReadyPointClickThreatAt(endpoint);
        context.EscapesThreat = defensive && Engine::ValidEnemy(target) &&
            endpoint.Distance2D(target.Position()) >
                player.Position().Distance2D(target.Position()) + 120.0f;
        context.CreatesSpellAngle = Engine::ValidEnemy(target) &&
            endpoint.Distance2D(target.Position()) <= kQRange - 60.0f &&
            !endpointToTarget.IsZero();
        context.ConcealsTurn = crossesWall ||
            std::fabs(direction.Dot(targetDirection)) < 0.45f;
        context.TakedownLikely = takedownLikely;
        context.DamagedChampionRecently = recentDamage;
        context.Defensive = defensive;
        context.PlayerWindingUp = Orbwalker::IsWindingUp();
        context.EnemiesAtEndpoint = Engine::CountEnemiesAt(endpoint, 620.0f);
        context.AlliesAtEndpoint = Engine::CountAlliesAt(endpoint, 760.0f);
        context.DistanceFromThreat = Engine::ValidEnemy(target)
            ? endpoint.Distance2D(target.Position()) : 0.0f;
        float score = WRouteScore(context);
        if (crossesWall) score += purpose == WPurpose::WallHop || defensive
            ? 125.0f : 45.0f;
        if (purpose == WPurpose::TakedownReset &&
            takedownLikely && recentDamage) score += 260.0f;
        if (purpose == WPurpose::HiddenPortal && RArenaActive) score += 190.0f;
        if (Engine::ValidEnemy(target) &&
            endpoint.Distance2D(target.Position()) < 260.0f && !defensive) {
            score -= 260.0f;
        }
        if (score > best.Score) {
            best.CastPosition = player.Position() + direction * kWBaseDash;
            best.Endpoint = endpoint;
            best.TargetId = Engine::ValidEnemy(target)
                ? static_cast<int>(target.NetworkId()) : 0;
            best.Purpose = purpose;
            best.Context = context;
            best.Score = score;
            best.CrossesWall = crossesWall;
            best.Valid = true;
        }
    }
    return best;
}

inline bool CastWPlan(const WPlan& plan,
                      Mode mode,
                      bool reactive = false) {
    if (!plan.Valid || WInvisible || !Ready(1) ||
        !SpellEnabled(1, mode) || !CastThrottleReady(1, 34,
            reactive ? 0 : -1) || !HasCurrentResource(SpellCost(1)) ||
        PlayerMobilityLocked()) return false;
    const float minimum = static_cast<float>(
        Slider(VeilMenu, "MinimumRouteScore", 160));
    if (!ShouldSpendW(plan.Context, minimum) &&
        plan.Purpose != WPurpose::HiddenPortal) return false;
    if (Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true) && !reactive) {
        return false;
    }
    if (!Engine::ControllerCastPosition(1, plan.CastPosition)) return false;
    WControllerOwned = true;
    WCastTick = Now();
    const int rank = SpellRank(1);
    WInvisibleUntil = WCastTick + static_cast<int>(
        (kQCastSeconds + WInvisibilitySeconds(rank)) * 1000.0f);
    WRealmHopperUntil = WCastTick + static_cast<int>(
        kWRealmHopperSeconds * 1000.0f);
    LastWPlan = plan;
    LastWPurpose = plan.Purpose;
    ActiveSequence = plan.Purpose == WPurpose::ResetEscape ||
                         plan.Purpose == WPurpose::Flee
        ? Sequence::ResetEscape
        : (plan.Purpose == WPurpose::TakedownReset
            ? Sequence::ResetReengage : Sequence::StealthAngle);
    return true;
}

inline void RefreshWResetTelemetry() {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::W);
    if (!spell.IsValid() || spell.Level() <= 0) return;
    const int now = Now();
    const float remaining = std::max(
        0.0f, spell.RemainingCooldown(Game::Time()));
    if (WCooldownObserved && now - LastWCooldownSampleTick <= 500 &&
        remaining + 1.25f < LastWCooldownSeconds &&
        now - LastChampionDamageTick <=
            static_cast<int>(kWResetDamageWindowSeconds * 1000.0f)) {
        LastWResetTick = now;
    }
    LastWCooldownSeconds = remaining;
    LastWCooldownSampleTick = now;
    WCooldownObserved = true;
}

inline std::vector<LineUnit> BuildEUnits(float delaySeconds,
                                        bool includeFarm) {
    return BuildLineUnits(delaySeconds, includeFarm);
}

inline EPlan BuildEPlan(const AIBaseClient& primary,
                        EPurpose purpose,
                        bool includeFarm = false,
                        bool reactive = false) {
    EPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !primary.IsValid() || SpellRank(2) <= 0) {
        return best;
    }
    const int primaryId = static_cast<int>(primary.NetworkId());
    const auto units = BuildEUnits(kECastSeconds, includeFarm);
    Vector3 predicted = PredictPosition(primary, kECastSeconds);
    Vector3 baseDirection = SharedGeometry::Direction2D(
        player.Position(), predicted);
    if (baseDirection.IsZero()) return best;
    static constexpr std::array<float, 7> angles = {
        0.0f, -4.0f, 4.0f, -8.0f, 8.0f, -12.0f, 12.0f,
    };
    const bool rooted = GroundedOrRooted();
    for (const float degrees : angles) {
        const Vector3 direction = SharedGeometry::Rotate2D(
            baseDirection, degrees * kPi / 180.0f);
        const Vector3 aim = player.Position() + direction * kERange;
        const Vector3 recoil = ERecoilEndpoint(
            player.Position(), aim, rooted);
        if (recoil.IsZero() && !rooted) continue;
        std::vector<int> hitIds;
        int championHits = 0;
        int farmHits = 0;
        int markedFarmKills = 0;
        bool primaryHit = false;
        float score = 0.0f;
        for (const auto& unit : units) {
            if (!ELineHits(player.Position(), aim, unit)) continue;
            hitIds.push_back(unit.Id);
            primaryHit = primaryHit || unit.Id == primaryId;
            const AIBaseClient live = UnitByNetworkId(unit.Id);
            if (unit.Champion) {
                ++championHits;
                const AIHeroClient enemy = HeroByNetworkId(unit.Id);
                score += TargetPriority(enemy) * 240.0f;
                if (PassiveStacksOn(unit.Id) == 2) score += 145.0f;
                if (live.IsValid() && EDamage(live) >=
                    live.Health() + live.AllShield()) score += 170.0f;
            } else {
                ++farmHits;
                score += 28.0f;
                const TrackedMark* mark = FindMark(unit.Id);
                if (mark && mark->ExpireTick >= Now() && live.IsValid() &&
                    EDamage(live) >= live.Health()) {
                    ++markedFarmKills;
                    score += 38.0f;
                }
            }
        }
        if (!primaryHit) continue;
        const bool endpointValid = rooted ||
            (recoil.IsValid() && !recoil.IsZero() &&
             !SDK::NavMesh::IsWall(recoil));
        const bool endpointTurret = !rooted &&
            Engine::UnderEnemyTurret(recoil) &&
            !Engine::UnderEnemyTurret(player.Position());
        const bool endpointLockdown = !rooted &&
            (HasReadyPointClickThreatAt(recoil) ||
             HasReadyDashHazardAt(recoil));
        const AIHeroClient hero = primary.IsHero()
            ? AIHeroClient(primary.Handle()) : AIHeroClient{};
        ECastContext context{};
        context.TargetHit = true;
        context.RecoilEndpointValid = endpointValid;
        context.GroundedOrRooted = rooted;
        context.EndpointTurret = endpointTurret;
        context.EndpointTerrainBlocked = !rooted &&
            SDK::NavMesh::IsWall(recoil);
        context.EndpointLockdown = endpointLockdown;
        context.IncomingDisplacement = IncomingOneInstanceCrowdControl &&
            IncomingThreatUntil >= Now();
        context.CanBufferDisplacement = purpose == EPurpose::DisplacementBuffer ||
            purpose == EPurpose::PortalBuffer;
        context.AllIn = purpose == EPurpose::AllInFinish ||
            purpose == EPurpose::TradeProc ||
            purpose == EPurpose::RecoilPeel;
        context.FinalMobilityResource = !Ready(1) && !RArenaActive;
        context.TargetCanBeChasedAfter = !primary.IsHero() ||
            recoil.Distance2D(primary.Position()) <= kQRange ||
            (Engine::ValidEnemy(hero) && ConservativeComboDamage(hero, false) >=
                hero.Health() + hero.AllShield());
        context.StandalonePoke = purpose == EPurpose::StandalonePoke ||
            purpose == EPurpose::Wave || purpose == EPurpose::Jungle ||
            purpose == EPurpose::MarkedWaveAfterQ2;
        context.PlayerWindingUp = Orbwalker::IsWindingUp() && !reactive;
        if (!ShouldCastE(context)) continue;
        score += championHits * championHits * 42.0f + farmHits * 12.0f;
        if (purpose == EPurpose::RecoilPeel && Engine::ValidEnemy(hero) &&
            recoil.Distance2D(hero.Position()) >
                player.Position().Distance2D(hero.Position())) score += 260.0f;
        if (purpose == EPurpose::DisplacementBuffer) score += 300.0f;
        if (purpose == EPurpose::PortalBuffer) score += 360.0f;
        if (!reactive && !CursorDirectionAgrees(aim, -0.28f) &&
            purpose != EPurpose::RecoilPeel) score -= 190.0f;
        if (score > best.Score) {
            best.Aim = aim;
            best.RecoilEndpoint = recoil;
            best.TargetId = primaryId;
            best.Purpose = purpose;
            best.HitIds = std::move(hitIds);
            best.ChampionHits = championHits;
            best.FarmHits = farmHits;
            best.MarkedFarmKills = markedFarmKills;
            best.Score = score;
            best.GroundedOrRooted = rooted;
            best.Valid = true;
        }
    }
    return best;
}

inline bool CastEPlan(const EPlan& plan,
                      Mode mode,
                      bool reactive = false) {
    if (!plan.Valid || !Ready(2) || !SpellEnabled(2, mode) ||
        !CastThrottleReady(2, 34, reactive ? 0 : -1) ||
        !HasCurrentResource(SpellCost(2))) return false;
    if (QActive && LastQPurpose == QPurpose::MarkedWave &&
        plan.MarkedFarmKills > 0) return false;
    if (Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true) && !reactive) {
        return false;
    }
    if (!plan.GroundedOrRooted &&
        (plan.RecoilEndpoint.IsZero() ||
         SDK::NavMesh::IsWall(plan.RecoilEndpoint) ||
         (Engine::UnderEnemyTurret(plan.RecoilEndpoint) &&
          !Engine::UnderEnemyTurret(ObjectManager::Player().Position())) ||
         HasReadyPointClickThreatAt(plan.RecoilEndpoint))) return false;
    if (!Engine::ControllerCastPosition(2, plan.Aim)) return false;
    ECastTick = Now();
    ERecoilUntil = ECastTick + 750;
    LastERecoilEndpoint = plan.RecoilEndpoint;
    for (const int id : plan.HitIds) ObservePassiveApplication(id);
    if (plan.ChampionHits > 0) LastChampionDamageTick = Now();
    LastEPlan = plan;
    LastEPurpose = plan.Purpose;
    ActiveSequence = plan.Purpose == EPurpose::RecoilPeel
        ? Sequence::RecoilPeel
        : (plan.Purpose == EPurpose::PortalBuffer
            ? Sequence::PortalBuffer
            : (plan.Purpose == EPurpose::Wave ||
               plan.Purpose == EPurpose::MarkedWaveAfterQ2
                ? Sequence::FarmPullback : Sequence::ExtendedDoubleSpirit));
    return true;
}

inline bool ResolveRLeapTerrain(RPlacement& placement) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !placement.Valid ||
        placement.Direction.IsZero()) return false;
    if (!SDK::NavMesh::IsWall(placement.LeapEndpoint)) return true;

    bool sawWall = false;
    for (float distance = 25.0f;
         distance <= kRWallForgiveness;
         distance += 25.0f) {
        const Vector3 sample = player.Position() +
            placement.Direction * distance;
        if (SDK::NavMesh::IsWall(sample)) {
            sawWall = true;
            continue;
        }
        if (sawWall && distance > kRLeapMaximum) {
            placement.LeapEndpoint = sample;
            return true;
        }
    }
    return false;
}

inline std::vector<RUnit> BuildRUnits(float delaySeconds,
                                      int primaryId) {
    std::vector<RUnit> result;
    result.reserve(12);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kRCastRange +
                kRArenaRadius + 260.0f)) continue;
        result.push_back({
            PredictPosition(enemy, delaySeconds),
            enemy.BoundingRadius(),
            TargetPriority(enemy),
            static_cast<int>(enemy.NetworkId()) == primaryId,
            Engine::IsHardCrowdControlled(enemy),
            ControllerHelpers::EnemyFlashReady(enemy),
            HasSpellShieldOrImmunity(enemy),
            !IsCommonUntargetableOrImmune(enemy),
        });
    }
    return result;
}

inline RPlan BuildRPlan(const AIHeroClient& primary,
                        RPurpose purpose,
                        bool defensive = false,
                        bool manual = false) {
    RPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(primary) ||
        SpellRank(3) <= 0 || RArenaActive || RuntimeRRecast()) return best;
    const int primaryId = static_cast<int>(primary.NetworkId());
    const Vector3 predicted = PredictPosition(primary, 0.42f);
    Vector3 baseDirection = SharedGeometry::Direction2D(
        player.Position(), predicted);
    if (baseDirection.IsZero()) return best;
    std::vector<Vector3> directions;
    directions.reserve(14);
    directions.push_back(baseDirection);
    static constexpr std::array<float, 8> angles = {
        -8.0f, 8.0f, -16.0f, 16.0f,
        -25.0f, 25.0f, -36.0f, 36.0f,
    };
    for (const float degrees : angles) {
        directions.push_back(SharedGeometry::Rotate2D(
            baseDirection, degrees * kPi / 180.0f));
    }
    if (manual) {
        const Vector3 cursorDirection = SharedGeometry::Direction2D(
            player.Position(), Game::CursorPos());
        if (!cursorDirection.IsZero()) directions.push_back(cursorDirection);
    }

    const int followup = CountAlliedFollowup(
        predicted, 900.0f, false);
    int expectedProcs = 0;
    if (Ready(0) && Ready(2)) expectedProcs = 1;
    if (Ready(0) && Ready(2) && InAutoAttackRange(primary)) expectedProcs = 2;
    const auto units = BuildRUnits(0.42f, primaryId);
    for (const Vector3& direction : directions) {
        if (direction.IsZero()) continue;
        const Vector3 castPosition = player.Position() +
            direction * kRCastRange;
        RPlacement placement = ResolveRPlacement(
            player.Position(), castPosition);
        const bool terrainFeasible = ResolveRLeapTerrain(placement);
        if (!terrainFeasible) continue;
        const bool leapSafe = !SDK::NavMesh::IsWall(placement.LeapEndpoint) &&
            (!Engine::UnderEnemyTurret(placement.LeapEndpoint) ||
             Engine::UnderEnemyTurret(player.Position())) &&
            !HasReadyDashHazardAt(placement.LeapEndpoint) &&
            !HasReadyPointClickThreatAt(placement.LeapEndpoint) &&
            Engine::CountEnemiesAt(placement.LeapEndpoint, 520.0f) <=
                Engine::CountAlliesAt(placement.LeapEndpoint, 800.0f) +
                    (defensive ? 1 : 0);
        const auto evaluation = EvaluateR(
            placement.ArenaCenter, units, followup,
            expectedProcs, terrainFeasible);
        if (evaluation.Score <= -FLT_MAX * 0.5f) continue;
        float score = evaluation.Score;
        if (purpose == RPurpose::CrowdControlBuffer) score += 520.0f;
        if (purpose == RPurpose::SelfPeel) score += 430.0f;
        if (purpose == RPurpose::KillSecure) score += 330.0f;
        if (purpose == RPurpose::ArenaAllIn && expectedProcs >= 2) {
            score += 310.0f;
        }
        if (ControllerHelpers::EnemyCastWindowCommitted(
                EnemyWindows, primaryId)) score += 110.0f;
        if (ControllerHelpers::EnemyCastWindowHardCrowdControlSpent(
                EnemyWindows, primaryId)) score += 95.0f;
        if (!manual && !defensive &&
            !CursorDirectionAgrees(castPosition, -0.22f)) score -= 330.0f;
        if (!leapSafe) score -= 1000.0f;
        if (score > best.Score) {
            best.CastPosition = castPosition;
            best.Placement = placement;
            best.Evaluation = evaluation;
            best.HitIds.clear();
            for (const auto& unit : units) {
                if (unit.Valid && unit.Position.Distance2D(
                        placement.ArenaCenter) <=
                        kRArenaRadius + unit.Radius) {
                    // BuildRUnits follows enemy iteration order; recover IDs
                    // below from the live set to avoid storing runtime handles
                    // inside the pure geometry structure.
                    for (const auto& enemy : GameObjects::EnemyHeroes()) {
                        if (Engine::ValidEnemy(enemy) &&
                            PredictPosition(enemy, 0.42f).Distance2D(
                                unit.Position) <= 1.0f) {
                            best.HitIds.push_back(
                                static_cast<int>(enemy.NetworkId()));
                            break;
                        }
                    }
                }
            }
            best.TargetId = primaryId;
            best.Purpose = purpose;
            best.Score = score;
            best.TerrainFeasible = terrainFeasible;
            best.LeapEndpointSafe = leapSafe;
            best.Defensive = defensive;
            best.Valid = true;
        }
    }
    return best;
}

inline bool CastRPlan(const RPlan& plan,
                      Mode mode,
                      bool reactive = false) {
    if (!plan.Valid || RArenaActive || RuntimeRRecast() || !Ready(3) ||
        !SpellEnabled(3, mode) || !CastThrottleReady(3, 34,
            reactive ? 0 : -1) || !HasCurrentResource(SpellCost(3))) {
        return false;
    }
    const bool defensiveBuffer = plan.Purpose ==
        RPurpose::CrowdControlBuffer;
    RCastContext context{};
    context.TerrainFeasible = plan.TerrainFeasible;
    context.LeapEndpointSafe = plan.LeapEndpointSafe;
    context.PlayerWindingUp = Orbwalker::IsWindingUp() && !reactive;
    context.FollowupReady = Ready(0) || Ready(2) ||
        CountAlliedFollowup(plan.Placement.ArenaCenter, 900.0f) > 0;
    context.DefensiveBuffer = defensiveBuffer;
    context.IncomingOneInstanceCrowdControl =
        IncomingOneInstanceCrowdControl && IncomingThreatUntil >= Now();
    context.IncomingSuppressionOrLongCrowdControl =
        IncomingPersistentCrowdControl && IncomingThreatUntil >= Now();
    context.MinimumHits = Slider(WorldsMenu, "MinimumHits", 2);
    context.Evaluation = plan.Evaluation;
    if (plan.Purpose == RPurpose::KillSecure ||
        plan.Purpose == RPurpose::SelfPeel ||
        plan.Purpose == RPurpose::Manual) {
        context.MinimumHits = 1;
        context.FollowupReady = true;
    }
    if (!ShouldCastR(context)) return false;
    if (!Engine::ControllerCastPosition(3, plan.CastPosition)) return false;
    RControllerOwned = true;
    RArenaActive = true;
    RCastTick = Now();
    RArenaExpireTick = RCastTick + static_cast<int>(
        RArenaDuration(SpellRank(3)) * 1000.0f);
    RPortalReadyTick = RCastTick + 350;
    RArenaCenter = plan.Placement.ArenaCenter;
    for (const int id : plan.HitIds) ObservePassiveApplication(id);
    if (plan.Evaluation.Hits > 0) LastChampionDamageTick = Now();
    LastRPlan = plan;
    LastRPurpose = plan.Purpose;
    ActiveSequence = defensiveBuffer
        ? Sequence::PortalBuffer : Sequence::ArenaAllIn;
    return true;
}

inline bool CastRRecast() {
    if (!RArenaActive || !RControllerOwned || !RuntimeRRecast() ||
        !Ready(3) || !CastThrottleReady(3, 34, 0)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RArenaActive = false;
    RControllerOwned = false;
    RArenaExpireTick = RPortalReadyTick = 0;
    RArenaCenter = {};
    LastPortalDestination = {};
    return true;
}

inline bool PortalDestinationSafe(const Vector3& destination) {
    if (!destination.IsValid() || destination.IsZero() ||
        SDK::NavMesh::IsWall(destination) ||
        Engine::UnderEnemyTurret(destination) ||
        HasReadyDashHazardAt(destination) ||
        HasReadyPointClickThreatAt(destination)) return false;
    return Engine::CountEnemiesAt(destination, 620.0f) <=
        Engine::CountAlliesAt(destination, 850.0f) + 1;
}

inline bool TryPortalBufferE(const AIHeroClient& threat,
                             const Vector3& destination) {
    if (!Bool(WorldsMenu, "EPortalBuffer", true) || !Ready(2) ||
        !Engine::ValidEnemy(threat) || !PortalDestinationSafe(destination)) {
        return false;
    }
    const auto player = ObjectManager::Player();
    const Vector3 inward = SharedGeometry::Direction2D(
        player.Position(), RArenaCenter);
    if (inward.IsZero()) return false;
    EPlan plan{};
    plan.Aim = player.Position() + inward * kERange;
    plan.RecoilEndpoint = ERecoilEndpoint(
        player.Position(), plan.Aim, GroundedOrRooted());
    const LineUnit unit = ReturnTargetUnit(threat);
    if (!ELineHits(player.Position(), plan.Aim, unit)) return false;
    plan.TargetId = static_cast<int>(threat.NetworkId());
    plan.Purpose = EPurpose::PortalBuffer;
    plan.HitIds.push_back(plan.TargetId);
    plan.ChampionHits = 1;
    plan.Score = 900.0f;
    plan.GroundedOrRooted = GroundedOrRooted();
    plan.Valid = true;
    return CastEPlan(plan, Mode::Automatic, true);
}

inline bool TryHiddenPortalW(const AIHeroClient& threat,
                             const Vector3& destination) {
    if (!Bool(WorldsMenu, "WHiddenPortal", false) || !Ready(1) ||
        !Engine::ValidEnemy(threat) || !PortalDestinationSafe(destination)) {
        return false;
    }
    const auto player = ObjectManager::Player();
    const Vector3 outward = SharedGeometry::Direction2D(
        RArenaCenter, player.Position());
    if (outward.IsZero()) return false;
    bool wall = false;
    bool reachable = false;
    const Vector3 endpoint = ResolveWEndpoint(outward, wall, reachable);
    if (!reachable || endpoint.Distance2D(RArenaCenter) <=
        player.Position().Distance2D(RArenaCenter)) return false;
    WPlan plan{};
    plan.CastPosition = player.Position() + outward * kWBaseDash;
    plan.Endpoint = endpoint;
    plan.TargetId = static_cast<int>(threat.NetworkId());
    plan.Purpose = WPurpose::HiddenPortal;
    plan.Context.EndpointValid = true;
    plan.Context.CursorAgrees = CursorDirectionAgrees(plan.CastPosition, -0.3f);
    plan.Context.TerrainReachable = true;
    plan.Context.EnemyTurret = Engine::UnderEnemyTurret(endpoint);
    plan.Context.DashHazard = HasReadyDashHazardAt(endpoint);
    plan.Context.PointClickLockdown = HasReadyPointClickThreatAt(endpoint);
    plan.Context.EscapesThreat = true;
    plan.Context.CreatesSpellAngle = true;
    plan.Context.ConcealsTurn = true;
    plan.Context.Defensive = true;
    plan.Context.EnemiesAtEndpoint = Engine::CountEnemiesAt(endpoint, 620.0f);
    plan.Context.AlliesAtEndpoint = Engine::CountAlliesAt(endpoint, 800.0f);
    plan.Context.DistanceFromThreat = endpoint.Distance2D(threat.Position());
    plan.Score = WRouteScore(plan.Context) + 280.0f;
    plan.CrossesWall = wall;
    plan.Valid = true;
    return CastWPlan(plan, Mode::Automatic, true);
}

inline void DetectPortalTransit() {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    if (RArenaActive && LastPlayerPosition.IsValid() &&
        !LastPlayerPosition.IsZero() && now - LastPlayerPositionTick <= 180 &&
        player.Position().Distance2D(LastPlayerPosition) >= 900.0f) {
        RLastPortalTick = now;
        RPortalReadyTick = now + 350;
    }
    LastPlayerPosition = player.Position();
    LastPlayerPositionTick = now;
}

inline bool ManageArena() {
    if (!RArenaActive) return false;
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return false;
    if (RArenaExpireTick <= Now()) {
        RArenaActive = false;
        RControllerOwned = false;
        RArenaCenter = {};
        return false;
    }
    const Vector3 radial = SharedGeometry::Direction2D(
        RArenaCenter, player.Position());
    if (radial.IsZero()) return false;
    const Vector3 boundary = RArenaCenter + radial * kRArenaRadius;
    const Vector3 destination = PortalDestination(RArenaCenter, boundary);
    LastPortalDestination = destination;
    const bool nearBoundary = NearPortalBoundary(
        player.Position(), RArenaCenter);
    const bool destinationSafe = PortalDestinationSafe(destination);
    const AIHeroClient threat = HeroByNetworkId(IncomingThreatTargetId);
    PortalContext portal{};
    portal.ArenaActive = true;
    portal.PortalReady = Now() >= RPortalReadyTick;
    portal.NearBoundary = nearBoundary;
    portal.DestinationSafe = destinationSafe;
    portal.IncomingTargetedDamage = IncomingThreatUntil >= Now() &&
        Engine::ValidEnemy(threat) && !IncomingOneInstanceCrowdControl;
    portal.IncomingOneInstanceCrowdControl =
        IncomingThreatUntil >= Now() && IncomingOneInstanceCrowdControl;
    portal.IncomingSuppressionOrLongCrowdControl =
        IncomingThreatUntil >= Now() && IncomingPersistentCrowdControl;
    portal.TurretShotPending = Engine::UnderEnemyTurret(player.Position()) &&
        IncomingThreatUntil >= Now();
    portal.ChasingPriorityTarget = false;
    portal.WReady = Ready(1);
    portal.EReady = Ready(2);
    portal.CombiningMobilityAddsValue =
        portal.IncomingOneInstanceCrowdControl || portal.TurretShotPending;
    portal.RemainingSeconds = static_cast<float>(
        RArenaExpireTick - Now()) / 1000.0f;
    if (ShouldUsePortal(portal) && Engine::ValidEnemy(threat)) {
        if (TryPortalBufferE(threat, destination)) return true;
        if (TryHiddenPortalW(threat, destination)) return true;
    }

    const Vector3 cursorDirection = SharedGeometry::Direction2D(
        player.Position(), Game::CursorPos());
    const bool wantsBoundary = nearBoundary && !cursorDirection.IsZero() &&
        cursorDirection.Dot(radial) > 0.48f;
    REarlyEndContext end{};
    end.ArenaActive = true;
    end.RecastReady = RuntimeRRecast() && Ready(3);
    end.ForcedUnsafePortal = wantsBoundary && !destinationSafe;
    end.DestinationUnsafe = !destinationSafe;
    end.PlayerWantsExit = wantsBoundary;
    end.TargetEscaped = Engine::CountEnemiesAt(
        RArenaCenter, kRArenaRadius + 180.0f) == 0;
    end.IncomingThreatCanBePortaled =
        portal.IncomingTargetedDamage ||
        portal.IncomingOneInstanceCrowdControl;
    end.RemainingSeconds = portal.RemainingSeconds;
    return ShouldEndR(end) && CastRRecast();
}

inline void RefreshRuntimeState() {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    RefreshMarks();
    RefreshSpirits();
    RefreshWResetTelemetry();
    DetectPortalTransit();

    for (auto& record : PassiveRecords) {
        if (record.NetworkId != 0 && record.ExpireTick < now) {
            record.State.Stacks = 0;
            record.State.Procs = 0;
        }
    }
    if (RuntimeQRecast()) {
        if (!QActive) {
            QActive = true;
            QControllerOwned = false;
            QCastTick = now;
            QExpireTick = now + static_cast<int>(kQMarkSeconds * 1000.0f);
            ActiveSequence = Sequence::PlayerLed;
        }
        QLastObservedTick = now;
    } else if (QActive && now - QLastObservedTick > 260 &&
               now > QExpireTick + 180) {
        ClearQState();
    }

    WInvisible = HasAnyBuff(player, {
        "AuroraWStealth", "AuroraWInvis", "AuroraW",
        "AcrossTheVeil",
    }) || now < WInvisibleUntil;
    WRealmHopper = HasAnyBuff(player, {
        "AuroraWBuff", "AuroraRealmHopper", "RealmHopper",
    }) || now < WRealmHopperUntil;

    if (RuntimeRRecast() || HasAnyBuff(player, {
            "AuroraR", "AuroraRActive", "BetweenWorlds",
        })) {
        if (!RArenaActive) {
            RArenaActive = true;
            RControllerOwned = false;
            RCastTick = now;
            RArenaExpireTick = now + static_cast<int>(
                RArenaDuration(std::max(1, SpellRank(3))) * 1000.0f);
            if (RArenaCenter.IsZero()) {
                const Vector3 direction = SharedGeometry::Direction2D(
                    player.Position(), Game::CursorPos());
                if (!direction.IsZero()) {
                    RArenaCenter = player.Position() +
                        direction * kRArenaCenterOffset;
                }
            }
        }
    } else if (RArenaActive && now > RArenaExpireTick + 150) {
        RArenaActive = false;
        RControllerOwned = false;
        RArenaCenter = {};
    }

    if (IncomingThreatUntil < now) {
        IncomingThreatTargetId = 0;
        IncomingOneInstanceCrowdControl = false;
        IncomingPersistentCrowdControl = false;
    }
    if (GapcloserExpireTick < now) {
        GapcloserTargetId = 0;
        GapcloserEnd = {};
    }
}

inline Posture DeterminePosture(Mode mode,
                                const AIHeroClient& target) {
    if (mode == Mode::Flee) return Posture::Flee;
    if (RArenaActive) return Posture::Arena;
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return Posture::Farm;
    if (IncomingThreatUntil >= Now() || GapcloserExpireTick >= Now()) {
        return Posture::RecoilPeel;
    }
    if (mode == Mode::Harass) return Posture::LaneTrade;
    if (mode == Mode::Combo && Engine::ValidEnemy(target)) {
        if (WInvisible || WRealmHopper) return Posture::Flank;
        if (QActive || PassiveProcsOn(
                static_cast<int>(target.NetworkId())) > 0) {
            return Posture::ExtendedFight;
        }
        return Posture::FrontToBack;
    }
    return Posture::Neutral;
}

inline AIHeroClient CursorEnemy(float range = 1450.0f) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return {};
    AIHeroClient best{};
    float bestCursorDistance = FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range)) continue;
        const float cursorDistance = enemy.Position().Distance2D(
            Game::CursorPos());
        if (cursorDistance < bestCursorDistance) {
            bestCursorDistance = cursorDistance;
            best = enemy;
        }
    }
    return bestCursorDistance <= 620.0f ? best : AIHeroClient{};
}

inline bool TryManualR() {
    if (!Key(WorldsMenu, "ManualR", false) || !Ready(3) ||
        RArenaActive) return false;
    const AIHeroClient target = CursorEnemy();
    if (!Engine::ValidEnemy(target)) return false;
    const RPlan plan = BuildRPlan(
        target, RPurpose::Manual, false, true);
    return CastRPlan(plan, Mode::Automatic, true);
}

inline bool TryReactiveDefense(Mode mode) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return false;
    AIHeroClient threat = HeroByNetworkId(IncomingThreatTargetId);
    if (!Engine::ValidEnemy(threat) && GapcloserExpireTick >= Now()) {
        threat = HeroByNetworkId(GapcloserTargetId);
    }
    if (!Engine::ValidEnemy(threat)) return false;

    if (IncomingThreatUntil >= Now() &&
        IncomingOneInstanceCrowdControl &&
        !IncomingPersistentCrowdControl && Ready(3) &&
        Bool(WorldsMenu, "BufferOneInstanceCC", true)) {
        const RPlan r = BuildRPlan(
            threat, RPurpose::CrowdControlBuffer, true);
        if (CastRPlan(r, Mode::Automatic, true)) return true;
    }

    if (GapcloserExpireTick >= Now() && Ready(2) &&
        Bool(WeirdingMenu, "AntiGapcloser", true)) {
        const EPlan e = BuildEPlan(
            threat, EPurpose::RecoilPeel, false, true);
        if (CastEPlan(e, Mode::Automatic, true)) return true;
    }

    if (IncomingThreatUntil >= Now() && Ready(2) &&
        IncomingOneInstanceCrowdControl &&
        Bool(WeirdingMenu, "BufferDisplacements", true)) {
        const EPlan e = BuildEPlan(
            threat, EPurpose::DisplacementBuffer, false, true);
        if (CastEPlan(e, Mode::Automatic, true)) return true;
    }

    const bool low = player.HealthPercent() <=
        Slider(TacticsMenu, "DefensiveHealth", 42);
    if ((low || mode == Mode::Flee) && Ready(1) &&
        Bool(VeilMenu, "Defensive", true)) {
        const WPurpose purpose = LastWResetTick + 1100 >= Now()
            ? WPurpose::ResetEscape : WPurpose::Dodge;
        const WPlan w = BuildWPlan(threat, purpose, true);
        if (CastWPlan(w, Mode::Automatic, true)) return true;
    }
    if (low && Ready(3) && Bool(WorldsMenu, "SelfPeel", true)) {
        const RPlan r = BuildRPlan(threat, RPurpose::SelfPeel, true);
        if (CastRPlan(r, Mode::Automatic, true)) return true;
    }
    return false;
}

inline bool TryKillSecure(const AIHeroClient& target) {
    if (!Bool(TacticsMenu, "KillSecure", true) ||
        !Engine::ValidEnemy(target, 1500.0f) ||
        HasSpellShieldOrImmunity(target)) return false;
    const float health = target.Health() + target.AllShield();
    if (QActive && RuntimeQRecast()) {
        const auto evaluation = EvaluateQReturn(
            ReturnTargetUnit(target), CurrentMarks(),
            ObjectManager::Player().Position());
        if (Q2Damage(target, std::max(1.0f, evaluation.DamageUnits)) >=
            health) {
            return CastQ2(QPurpose::PullbackLethal, true);
        }
    }
    if (!QActive && Ready(2) && EDamage(target) >= health) {
        const EPlan e = BuildEPlan(
            target, EPurpose::AllInFinish, false, true);
        if (CastEPlan(e, Mode::Automatic, true)) return true;
    }
    if (!RArenaActive && Ready(3) && RDamage(target) >= health &&
        Bool(WorldsMenu, "KillSecure", true)) {
        const RPlan r = BuildRPlan(
            target, RPurpose::KillSecure, false);
        if (CastRPlan(r, Mode::Automatic, true)) return true;
    }
    if (!QActive && Ready(0) &&
        Q1Damage(target) + Q2Damage(target) >= health) {
        return CastQPlan(
            BuildQPlan(target, QPurpose::AllIn, false, true),
            Mode::Automatic, true);
    }
    return false;
}

inline bool ShouldYieldForPlayerAuto(const AIHeroClient& target) {
    if (!Bool(PassiveMenu, "WeaveAutos", true) ||
        !Engine::ValidEnemy(target) || !InAutoAttackRange(target) ||
        !QActive || QExpireTick - Now() <= 430) return false;
    if (Orbwalker::IsWindingUp()) return true;
    const bool attackedRecently = LastAfterAttackTargetId ==
        static_cast<int>(target.NetworkId()) &&
        Now() - LastAfterAttackTick <= 520;
    if (!attackedRecently && PassiveStacksOn(
            static_cast<int>(target.NetworkId())) < 2) {
        ActiveSequence = PassiveProcsOn(
            static_cast<int>(target.NetworkId())) > 0
            ? Sequence::ExtendedDoubleSpirit : Sequence::LevelOneWeave;
        return true;
    }
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const int id = static_cast<int>(target.NetworkId());
    if (QActive) {
        if (ShouldYieldForPlayerAuto(target)) return false;
        if (Ready(2) && QExpireTick - Now() > 480 &&
            Bool(WeirdingMenu, "Combo", true)) {
            const EPlan e = BuildEPlan(
                target, PassiveStacksOn(id) == 2
                    ? EPurpose::TradeProc : EPurpose::AllInFinish,
                false, false);
            if (CastEPlan(e, Mode::Combo)) return true;
        }
        if (Ready(3) && !RArenaActive &&
            Bool(WorldsMenu, "Combo", true) &&
            (Engine::CountEnemiesAt(target.Position(), 650.0f) >=
                 Slider(WorldsMenu, "MinimumHits", 2) ||
             ControllerHelpers::EnemyCastWindowHardCrowdControlSpent(
                 EnemyWindows, id))) {
            const RPlan r = BuildRPlan(
                target, RPurpose::ArenaAllIn, false);
            if (CastRPlan(r, Mode::Combo)) return true;
        }
        return false;
    }

    if (Ready(3) && Bool(WorldsMenu, "OpenVerifiedAllIn", true) &&
        (Engine::CountEnemiesAt(target.Position(), 650.0f) >=
             Slider(WorldsMenu, "MinimumHits", 2) ||
         (Engine::IsHardCrowdControlled(target) &&
          CountAlliedFollowup(target.Position(), 900.0f) > 0))) {
        const RPlan r = BuildRPlan(
            target, RPurpose::FollowupEngage, false);
        if (CastRPlan(r, Mode::Combo)) return true;
    }

    const float targetDistance = ObjectManager::Player().Position().Distance2D(
        target.Position());
    const bool resetEntry = Now() - LastChampionDamageTick <=
            static_cast<int>(kWResetDamageWindowSeconds * 1000.0f) &&
        ConservativeComboDamage(target, true) >=
            target.Health() + target.AllShield() &&
        targetDistance > 300.0f;
    if (Ready(1) && Bool(VeilMenu, "Combo", true) &&
        (targetDistance > kQRange - 90.0f || resetEntry)) {
        const bool ccWindow =
            ControllerHelpers::EnemyCastWindowHardCrowdControlSpent(
                EnemyWindows, id) ||
            Engine::IsHardCrowdControlled(target);
        if (!Bool(VeilMenu, "OnlyAfterCCSpent", true) || ccWindow) {
            const WPurpose purpose =
                ConservativeComboDamage(target, true) >=
                    target.Health() + target.AllShield()
                ? WPurpose::TakedownReset : WPurpose::FlankAngle;
            const WPlan w = BuildWPlan(target, purpose, false);
            if (CastWPlan(w, Mode::Combo)) return true;
        }
    }

    if (Ready(0) && Bool(HexMenu, "Combo", true)) {
        const QPlan q = BuildQPlan(target, QPurpose::AllIn, false);
        if (CastQPlan(q, Mode::Combo)) return true;
    }

    if (Ready(2) && Bool(WeirdingMenu, "Combo", true) &&
        (!Ready(0) || target.IsDashing())) {
        const EPlan e = BuildEPlan(
            target, EPurpose::StandalonePoke, false);
        if (CastEPlan(e, Mode::Combo)) return true;
    }
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) ||
        PlayerManaPercent() < Slider(HexMenu, "HarassMana", 48)) {
        return false;
    }
    const int id = static_cast<int>(target.NetworkId());
    if (QActive) {
        if (ShouldYieldForPlayerAuto(target)) return false;
        if (Ready(2) && Bool(WeirdingMenu, "Harass", true) &&
            QExpireTick - Now() > 500 &&
            (PassiveStacksOn(id) >= 1 ||
             target.IsDashing())) {
            const EPlan e = BuildEPlan(
                target, EPurpose::TradeProc, false);
            if (CastEPlan(e, Mode::Harass)) return true;
        }
        return false;
    }
    if (Ready(0) && Bool(HexMenu, "Harass", true)) {
        const QPlan q = BuildQPlan(target, QPurpose::Trade, false);
        if (CastQPlan(q, Mode::Harass)) return true;
    }
    if (Ready(2) && Bool(WeirdingMenu, "Harass", true) &&
        PlayerManaPercent() >= Slider(WeirdingMenu, "HarassMana", 64) &&
        (!InAutoAttackRange(target) || target.IsDashing())) {
        const EPlan e = BuildEPlan(
            target, EPurpose::StandalonePoke, false);
        if (CastEPlan(e, Mode::Harass)) return true;
    }
    return false;
}

inline AIBaseClient SelectLaneTarget(float range) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return {};
    AIBaseClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!ValidHostileUnitInGameplayRange(minion, range) ||
            HasFlag(minion.GetMinionType(), MinionTypes::Ward) ||
            minion.IsPlant()) continue;
        int neighbors = 0;
        for (const auto& other : GameObjects::EnemyMinions()) {
            if (other.IsValid() && !other.IsDead() &&
                other.NetworkId() != minion.NetworkId() &&
                other.Position().Distance2D(minion.Position()) <= 340.0f) {
                ++neighbors;
            }
        }
        float score = neighbors * 80.0f - minion.Health() * 0.02f;
        if (IsLargeLaneMinion(minion)) score += 500.0f;
        if (score > bestScore) {
            bestScore = score;
            best = AIBaseClient(minion.Handle());
        }
    }
    return best;
}

inline bool TryFarm(Mode mode) {
    if (Bool(FarmMenu, "HoldForChampion", true) &&
        HasEnemyChampionNear(
            static_cast<float>(Slider(FarmMenu, "ChampionHoldRange", 1100)))) {
        return false;
    }
    const float manaFloor = mode == Mode::Jungle
        ? static_cast<float>(Slider(FarmMenu, "JungleMana", 35))
        : static_cast<float>(Slider(FarmMenu, "LaneMana", 58));
    if (PlayerManaPercent() < manaFloor) return false;

    AIBaseClient primary{};
    if (mode == Mode::Jungle) {
        const AIMinionClient monster = SelectJungleTarget(kQRange);
        if (monster.IsValid()) primary = AIBaseClient(monster.Handle());
    } else {
        primary = SelectLaneTarget(kQRange);
    }
    if (!primary.IsValid()) return false;

    if (!QActive && Ready(0) && Bool(FarmMenu, "UseQ", true)) {
        const QPurpose purpose = mode == Mode::Jungle
            ? QPurpose::Jungle : QPurpose::MarkedWave;
        const QPlan q = BuildQPlan(primary, purpose, true);
        if (q.MarkedWaveHits >= Slider(FarmMenu, "QMinimumUnits", 3) ||
            mode == Mode::Jungle) {
            if (CastQPlan(q, mode)) return true;
        }
    }

    const bool qPullbackResolved = !QActive &&
        Q2LastTick > 0 && Now() - Q2LastTick <= 720;
    if (Ready(2) && Bool(FarmMenu, "UseE", true) &&
        (!Ready(0) || qPullbackResolved || mode == Mode::Jungle)) {
        const EPlan e = BuildEPlan(
            primary,
            qPullbackResolved ? EPurpose::MarkedWaveAfterQ2
                              : (mode == Mode::Jungle
                                  ? EPurpose::Jungle : EPurpose::Wave),
            true);
        if (e.FarmHits >= Slider(FarmMenu, "EMinimumUnits", 4) ||
            mode == Mode::Jungle) {
            if (CastEPlan(e, mode)) return true;
        }
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& fallback) {
    AIHeroClient threat = NearestEnemyToPlayer(fallback, 1050.0f);
    if (!Engine::ValidEnemy(threat)) return false;
    if (Ready(2) && Bool(WeirdingMenu, "Flee", true)) {
        const EPlan e = BuildEPlan(
            threat, EPurpose::RecoilPeel, false, true);
        if (CastEPlan(e, Mode::Flee, true)) return true;
    }
    if (Ready(1) && Bool(VeilMenu, "Flee", true)) {
        const WPurpose purpose = LastWResetTick + 1200 >= Now()
            ? WPurpose::ResetEscape : WPurpose::Flee;
        const WPlan w = BuildWPlan(threat, purpose, true);
        if (CastWPlan(w, Mode::Flee, true)) return true;
    }
    if (Ready(3) && Bool(WorldsMenu, "Flee", true) &&
        ObjectManager::Player().HealthPercent() <=
            Slider(WorldsMenu, "FleeHealth", 35)) {
        const RPlan r = BuildRPlan(
            threat, RPurpose::SelfPeel, true);
        if (CastRPlan(r, Mode::Flee, true)) return true;
    }
    return false;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& target) {
    LastKnownMode = mode;
    RefreshRuntimeState();
    CurrentPosture = DeterminePosture(mode, target);
    if (TryManualR()) return true;
    if (RArenaActive && ManageArena()) return true;
    if (QActive && ManageQRecast(mode)) return true;
    if (TryReactiveDefense(mode)) return true;
    if (TryKillSecure(target)) return true;
    if (mode == Mode::Flee) return TryFlee(target);
    if (mode == Mode::Combo) return TryCombo(target);
    if (mode == Mode::Harass) return TryHarass(target);
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return TryFarm(mode);
    return false;
}

inline void ObserveManualQLine(const Vector3& endpoint) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || endpoint.IsZero()) return;
    const Vector3 aim = ClampDirectionCast(endpoint, kQRange);
    const auto units = BuildLineUnits(kQCastSeconds + 0.25f, true);
    for (const auto& unit : units) {
        if (!QLineHits(player.Position(), aim, unit)) continue;
        const float travel = player.Position().Distance2D(unit.Position) /
            kQOutboundSpeed;
        TrackMark(unit.Id, unit.Position, unit.Radius,
                  unit.Champion, unit.Minion,
                  Now() + static_cast<int>((kQCastSeconds + travel +
                      kQMarkSeconds) * 1000.0f), false);
        ObservePassiveApplication(unit.Id);
    }
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (args.IsAutoAttack) {
        if (ControllerHelpers::CaptureLocalAutoAttack(
                args, LastAutoTargetId, LastAutoCastTick)) {
            const AIBaseClient attacked = UnitByNetworkId(LastAutoTargetId);
            if (attacked.IsValid() && attacked.IsHero()) {
                LastChampionDamageTick = now;
            }
        }
        return;
    }
    if (IsQ2Event(args)) {
        const bool ours = Engine::WasControllerCast(0);
        if (!ours) {
            for (const auto& mark : QMarks) {
                if (mark.NetworkId != 0 && mark.ExpireTick >= now) {
                    ObservePassiveApplication(mark.NetworkId);
                }
            }
            LastQPurpose = QPurpose::Manual;
            ActiveSequence = Sequence::PlayerLed;
        }
        Q2LastTick = now;
        ClearQState();
        return;
    }
    if (IsQEvent(args)) {
        const bool ours = Engine::WasControllerCast(0);
        QActive = true;
        QControllerOwned = ours;
        QCastTick = QLastObservedTick = now;
        QExpireTick = now + static_cast<int>(
            (kQCastSeconds + kQMarkSeconds) * 1000.0f);
        if (!ours) {
            LastQPurpose = QPurpose::Manual;
            ActiveSequence = Sequence::PlayerLed;
            const Vector3 endpoint = args.EndPosition.IsValid() &&
                    !args.EndPosition.IsZero()
                ? args.EndPosition : (args.CastPosition.IsValid() &&
                    !args.CastPosition.IsZero()
                    ? args.CastPosition : Game::CursorPos());
            ObserveManualQLine(endpoint);
        }
        return;
    }
    if (IsWEvent(args)) {
        const bool ours = Engine::WasControllerCast(1);
        WControllerOwned = ours;
        WCastTick = now;
        WInvisibleUntil = now + static_cast<int>(
            (0.25f + WInvisibilitySeconds(SpellRank(1))) * 1000.0f);
        WRealmHopperUntil = now + static_cast<int>(
            kWRealmHopperSeconds * 1000.0f);
        if (!ours) {
            LastWPurpose = WPurpose::Manual;
            ActiveSequence = Sequence::PlayerLed;
        }
        return;
    }
    if (IsEEvent(args)) {
        const bool ours = Engine::WasControllerCast(2);
        const auto player = ObjectManager::Player();
        const Vector3 endpoint = args.EndPosition.IsValid() &&
                !args.EndPosition.IsZero()
            ? args.EndPosition : args.CastPosition;
        ECastTick = now;
        ERecoilUntil = now + 750;
        LastERecoilEndpoint = player.IsValid()
            ? ERecoilEndpoint(player.Position(), endpoint,
                              GroundedOrRooted())
            : Vector3{};
        if (!ours && player.IsValid()) {
            const auto units = BuildEUnits(kECastSeconds, true);
            for (const auto& unit : units) {
                if (ELineHits(player.Position(), endpoint, unit)) {
                    ObservePassiveApplication(unit.Id);
                }
            }
            LastEPurpose = EPurpose::Manual;
            ActiveSequence = Sequence::PlayerLed;
        }
        return;
    }
    if (IsR2Event(args)) {
        const bool ours = Engine::WasControllerCast(3);
        RArenaActive = false;
        RControllerOwned = false;
        RArenaExpireTick = RPortalReadyTick = 0;
        RArenaCenter = {};
        LastPortalDestination = {};
        if (!ours) {
            LastRPurpose = RPurpose::Manual;
            ActiveSequence = Sequence::PlayerLed;
        }
        return;
    }
    if (IsREvent(args)) {
        const bool ours = Engine::WasControllerCast(3);
        const auto player = ObjectManager::Player();
        const Vector3 endpoint = args.EndPosition.IsValid() &&
                !args.EndPosition.IsZero()
            ? args.EndPosition : (args.CastPosition.IsValid() &&
                !args.CastPosition.IsZero()
                ? args.CastPosition : Game::CursorPos());
        if (player.IsValid()) {
            RPlacement placement = ResolveRPlacement(
                player.Position(), endpoint);
            RArenaCenter = placement.ArenaCenter;
        }
        RArenaActive = true;
        RControllerOwned = ours;
        RCastTick = now;
        RArenaExpireTick = now + static_cast<int>(
            RArenaDuration(std::max(1, SpellRank(3))) * 1000.0f);
        RPortalReadyTick = now + 350;
        if (!ours) {
            LastRPurpose = RPurpose::Manual;
            ActiveSequence = Sequence::PlayerLed;
        }
    }
}

inline bool PersistentCrowdControlSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellEventNameContainsAny(args, {
        "alzaharr", "nethergrasp", "warwickr", "infiniteduress",
        "skarnerimpale", "skarnerult", "urgotr", "fearbeyonddeath",
        "mordekaiserr", "realmofdeath", "settult", "showstopper",
        "suppression",
    });
}

inline void RecordEnemySpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto analysis = AnalyzeEnemyCast(
        args, 210.0f, 112.0f, 300, 250, 210, 1700, 540);
    if (!analysis.Valid) return;
    const int id = static_cast<int>(analysis.Enemy.NetworkId());
    if (EnemyWindow* record = FindEnemyWindow(id, true)) {
        if (analysis.Committed) {
            record->CommittedUntil = std::max(
                record->CommittedUntil,
                std::max(analysis.CommitmentUntilTick, Now() + 760));
        }
        if (analysis.LikelyHardCrowdControl) {
            record->HardCrowdControlSpentUntil = std::max(
                record->HardCrowdControlSpentUntil, Now() + 3400);
        }
    }
    if (analysis.TargetsPlayer || analysis.CrossesPlayer) {
        IncomingThreatTargetId = id;
        IncomingThreatUntil = std::max(
            analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
        if (IncomingThreatUntil <= Now()) IncomingThreatUntil = Now() + 420;
        IncomingPersistentCrowdControl = PersistentCrowdControlSpell(args);
        IncomingOneInstanceCrowdControl =
            analysis.LikelyHardCrowdControl &&
            !IncomingPersistentCrowdControl;
    }
}

inline void OnDoCast(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || args.IsAutoAttack) return;
    if (IsQEvent(args)) QLastObservedTick = Now();
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args,
                            bool added) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender)) {
        if (IsWInvisibleBuff(args.BuffName)) {
            WInvisible = added;
            if (!added) WInvisibleUntil = 0;
        }
        if (IsRealmHopperBuff(args.BuffName)) {
            WRealmHopper = added;
            if (!added) WRealmHopperUntil = 0;
        }
        if (IsRArenaBuff(args.BuffName)) {
            RArenaActive = added;
            if (added) {
                RArenaExpireTick = std::max(
                    RArenaExpireTick,
                    Now() + static_cast<int>(
                        RArenaDuration(std::max(1, SpellRank(3))) *
                        1000.0f));
            } else {
                RControllerOwned = false;
                RArenaCenter = {};
                LastPortalDestination = {};
            }
        }
        return;
    }

    const AIBaseClient unit = UnitByNetworkId(id);
    if (IsQMarkBuff(args.BuffName)) {
        if (added && unit.IsValid()) {
            TrackMark(id, unit.Position(), unit.BoundingRadius(),
                      unit.IsHero(), unit.IsMinion(),
                      Now() + static_cast<int>(kQMarkSeconds * 1000.0f),
                      true);
        } else if (!added) {
            RemoveMark(id);
        }
    }
    if (IsPassiveMarkBuff(args.BuffName)) {
        PassiveRecord* record = FindPassiveRecord(id, true);
        if (!record) return;
        if (!added) {
            record->State.Stacks = 0;
            record->ExpireTick = 0;
            return;
        }
        if (args.Count > 0) {
            if (args.Count > 3) record->DoubledTelemetry = true;
            const int observed = NormalizePassiveStacks(
                args.Count, record->DoubledTelemetry);
            if (record->State.Stacks == 2 && observed == 0) {
                ++record->State.Procs;
                AddSpirit();
            }
            record->State.Stacks = observed;
            record->State.RemainingSeconds = kPassiveMarkSeconds;
            record->ExpireTick = Now() + static_cast<int>(
                kPassiveMarkSeconds * 1000.0f);
            record->TelemetryReliable = true;
        }
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

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(
            args, LastAfterAttackTargetId, LastAfterAttackTick)) return;
    const AIBaseClient attacked = UnitByNetworkId(LastAfterAttackTargetId);
    if (!attacked.IsValid() || !attacked.IsEnemy() || attacked.IsDead()) {
        return;
    }
    ObservePassiveApplication(LastAfterAttackTargetId);
    if (attacked.IsValid() && attacked.IsHero()) {
        LastChampionDamageTick = Now();
    }
}

inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (ControllerHelpers::CaptureGapcloser(
            args, GapcloserTargetId, GapcloserEnd,
            GapcloserExpireTick, 760.0f, 1050)) {
        IncomingThreatTargetId = GapcloserTargetId;
        IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 720);
    }
}

inline const char* SequenceName(Sequence sequence) {
    switch (sequence) {
    case Sequence::LevelOneWeave: return "Q-AA-Q2";
    case Sequence::ShortTrade: return "Q-E-Q2";
    case Sequence::ExtendedDoubleSpirit: return "two-passive extended trade";
    case Sequence::MarkedWavePullback: return "Q-Q2-E marked wave";
    case Sequence::StealthAngle: return "W stealth angle";
    case Sequence::ResetReengage: return "W reset re-entry";
    case Sequence::ResetEscape: return "W reset exit";
    case Sequence::RecoilPeel: return "E recoil peel";
    case Sequence::ArenaAllIn: return "R arena all-in";
    case Sequence::PortalBuffer: return "R portal buffer";
    case Sequence::FarmPullback: return "pullback farm";
    case Sequence::PlayerLed: return "player-led";
    default: return "idle";
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::LaneTrade: return "lane trade";
    case Posture::ExtendedFight: return "extended fight";
    case Posture::Flank: return "flank";
    case Posture::FrontToBack: return "front-to-back";
    case Posture::RecoilPeel: return "recoil peel";
    case Posture::Arena: return "arena";
    case Posture::Farm: return "farm";
    case Posture::Flee: return "flee";
    default: return "neutral";
    }
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    if (Bool(CoachMenu, "DrawRanges", true)) {
        Drawing::DrawCircle(player.Position(), kQRange,
                            0x3359E7FFu, 1.0f, 72);
        Drawing::DrawCircle(player.Position(), kERange,
                            0x33B56CFFu, 1.0f, 72);
    }
    if (Bool(CoachMenu, "DrawQ", true) && LastQPlan.Valid) {
        Drawing::DrawLine(player.Position(), LastQPlan.Aim,
                          0xDD59E7FFu, 2.0f);
    }
    if (Bool(CoachMenu, "DrawPullbacks", true)) {
        for (const auto& mark : QMarks) {
            if (mark.NetworkId == 0 || mark.ExpireTick < Now()) continue;
            Drawing::DrawCircle(mark.LastPosition,
                                mark.Radius + kQMissileHalfWidth,
                                mark.Confirmed ? 0xCCF0A6FFu : 0x889B7BFFu,
                                1.4f, 36);
            Drawing::DrawLine(mark.LastPosition, player.Position(),
                              mark.Champion ? 0xCCF0A6FFu : 0x7789DFFFu,
                              mark.Champion ? 1.8f : 1.0f);
        }
    }
    if (Bool(CoachMenu, "DrawW", true) && LastWPlan.Valid) {
        Drawing::DrawLine(player.Position(), LastWPlan.Endpoint,
                          LastWPlan.CrossesWall ? 0xDDAA77FFu : 0xCC6CF0D8u,
                          2.0f);
        Drawing::DrawCircle(LastWPlan.Endpoint, 85.0f,
                            0xAA6CF0D8u, 1.5f, 36);
    }
    if (Bool(CoachMenu, "DrawE", true) && LastEPlan.Valid) {
        Drawing::DrawLine(player.Position(), LastEPlan.Aim,
                          0xDDB56CFFu, 2.0f);
        if (!LastEPlan.GroundedOrRooted &&
            !LastEPlan.RecoilEndpoint.IsZero()) {
            Drawing::DrawLine(player.Position(), LastEPlan.RecoilEndpoint,
                              0xDDFF7C8Eu, 2.1f);
            Drawing::DrawCircle(LastEPlan.RecoilEndpoint, 72.0f,
                                0xAAFF7C8Eu, 1.4f, 32);
        }
    }
    if (Bool(CoachMenu, "DrawR", true) &&
        (RArenaActive || LastRPlan.Valid)) {
        const Vector3 center = RArenaActive
            ? RArenaCenter : LastRPlan.Placement.ArenaCenter;
        if (!center.IsZero()) {
            Drawing::DrawCircle(center, kRArenaRadius,
                                0xCCB56CFFu, 2.0f, 96);
        }
        if (RArenaActive && !LastPortalDestination.IsZero()) {
            Drawing::DrawLine(player.Position(), LastPortalDestination,
                              PortalDestinationSafe(LastPortalDestination)
                                  ? 0xDD67F6D0u : 0xDDFF5964u,
                              2.2f);
            Drawing::DrawCircle(LastPortalDestination, 90.0f,
                                PortalDestinationSafe(LastPortalDestination)
                                    ? 0xAA67F6D0u : 0xAAFF5964u,
                                1.6f, 40);
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            const PassiveRecord* targetPassive =
                FindPassiveRecord(QPrimaryTargetId);
            const int stacks = targetPassive
                ? targetPassive->State.Stacks : 0;
            const int procs = targetPassive
                ? targetPassive->State.Procs : 0;
            char text[620]{};
            _snprintf_s(
                text, sizeof(text), _TRUNCATE,
                "Aurora OTP | %s | %s | Q marks %d %.2fs | passive %d (%d procs) | spirits %d | W reset %s | R %.2fs",
                PostureName(CurrentPosture), SequenceName(ActiveSequence),
                ActiveMarkCount(),
                QActive ? static_cast<float>(
                    std::max(0, QExpireTick - Now())) / 1000.0f : 0.0f,
                stacks, procs, PredictedSpiritCount,
                LastWResetTick + 1100 >= Now() ? "seen" : "no",
                RArenaActive ? static_cast<float>(
                    std::max(0, RArenaExpireTick - Now())) / 1000.0f : 0.0f);
            Drawing::DrawText(screen.x - 310.0f, screen.y - 118.0f,
                              0xFFCBA9FFu, text);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "AuroraOneTrick", "Aurora one-trick mechanics"));
    TacticsMenu->Add(new MenuBool(
        "KillSecure", "Use exact Q2/E/R packages to secure kills", true));
    TacticsMenu->Add(new MenuSlider(
        "DefensiveHealth", "Defensive W/R health threshold (%)", 42, 10, 90));
    TacticsMenu->Add(new MenuSeparator(
        "Ownership",
        "Movement, attacks, cursor and summoners stay player-owned; the controller times spells around them."));

    PassiveMenu = TacticsMenu->AddSubMenu(new Menu(
        "SpiritAbjuration", "Passive three-hit and auto-weave discipline"));
    PassiveMenu->Add(new MenuBool(
        "WeaveAutos", "Yield Q mark time for safe player auto attacks", true));
    PassiveMenu->Add(new MenuBool(
        "SeekSecondProc", "Extend arena fights only when a second passive proc is plausible", true));
    PassiveMenu->Add(new MenuSeparator(
        "PassiveRule",
        "Q, autos, E, Q2 and R are tracked as applications; observed buffs override estimates."));

    HexMenu = TacticsMenu->AddSubMenu(new Menu(
        "TwofoldHex", "Q pierce, pullback alignment and recast timing"));
    HexMenu->Add(new MenuBool(
        "Combo", "Use Q as the ordinary first spell in all-ins", true));
    HexMenu->Add(new MenuBool(
        "Harass", "Use Q before the player auto in short trades", true));
    HexMenu->Add(new MenuSlider(
        "HarassMana", "Minimum mana for Q trades (%)", 48, 0, 100));
    HexMenu->Add(new MenuBool(
        "PreserveWindup", "Delay Q2 unless lethal/expiry while an auto winds up", true));
    HexMenu->Add(new MenuBool(
        "AlignMarkedWave", "Hold Q2 for the player's natural bolt-alignment movement", true));
    HexMenu->Add(new MenuSeparator(
        "PullbackRule",
        "Q2 is delayed for missing health, autos or E; marked waves pull before E can delete bolt sources."));

    VeilMenu = TacticsMenu->AddSubMenu(new Menu(
        "AcrossTheVeil", "W stealth route, wall hop and reset value"));
    VeilMenu->Add(new MenuBool(
        "Combo", "Use W only to create a verified side angle", true));
    VeilMenu->Add(new MenuBool(
        "OnlyAfterCCSpent", "Hold offensive W until key CC is spent or target is locked", true));
    VeilMenu->Add(new MenuBool(
        "Defensive", "Use W to dodge or leave after a reset", true));
    VeilMenu->Add(new MenuBool(
        "Flee", "Use cursor-agreeing W wall routes while fleeing", true));
    VeilMenu->Add(new MenuSlider(
        "MinimumRouteScore", "Minimum automatic W route score", 160, -300, 1200));
    VeilMenu->Add(new MenuSeparator(
        "ResetRule",
        "A takedown is valued only inside Aurora's recent champion-damage window; reset never excuses turret/CC endpoints."));

    WeirdingMenu = TacticsMenu->AddSubMenu(new Menu(
        "TheWeirding", "E damage line and opposite recoil endpoint"));
    WeirdingMenu->Add(new MenuBool(
        "Combo", "Use E late enough that recoil does not end the chase", true));
    WeirdingMenu->Add(new MenuBool(
        "Harass", "Use E for passive proc or long-range poke", true));
    WeirdingMenu->Add(new MenuSlider(
        "HarassMana", "Minimum mana for standalone E poke (%)", 64, 0, 100));
    WeirdingMenu->Add(new MenuBool(
        "AntiGapcloser", "E a committed diver only with a safe recoil", true));
    WeirdingMenu->Add(new MenuBool(
        "BufferDisplacements", "Use E recoil to buffer a one-instance displacement", true));
    WeirdingMenu->Add(new MenuBool(
        "Flee", "Slow a pursuer and recoil toward safety", true));
    WeirdingMenu->Add(new MenuSeparator(
        "NonProjectile",
        "E is not rejected by projectile walls; rooted/grounded Aurora casts without recoil."));

    WorldsMenu = TacticsMenu->AddSubMenu(new Menu(
        "BetweenWorlds", "R leap, arena, opposite portal and early end"));
    WorldsMenu->Add(new MenuBool(
        "Combo", "Use R for a verified multi-target/extended fight", true));
    WorldsMenu->Add(new MenuBool(
        "OpenVerifiedAllIn", "Open R only on allied CC or multi-target value", true));
    WorldsMenu->Add(new MenuSlider(
        "MinimumHits", "Minimum offensive R hits", 2, 1, 5));
    WorldsMenu->Add(new MenuBool(
        "BufferOneInstanceCC", "Use unstoppable R leap against one-instance CC", true));
    WorldsMenu->Add(new MenuBool(
        "SelfPeel", "Use R defensively when W/E cannot safely peel", true));
    WorldsMenu->Add(new MenuBool(
        "EPortalBuffer", "E inward so recoil crosses a safe portal during CC", true));
    WorldsMenu->Add(new MenuBool(
        "WHiddenPortal", "Allow rare W-hidden portal bounce", false));
    WorldsMenu->Add(new MenuBool(
        "KillSecure", "Use exact mitigated R damage for kill secure", true));
    WorldsMenu->Add(new MenuBool(
        "Flee", "Use R portal as the last fleeing resource", true));
    WorldsMenu->Add(new MenuSlider(
        "FleeHealth", "Maximum health for fleeing R (%)", 35, 10, 80));
    WorldsMenu->Add(new MenuKeyBind(
        "ManualR", "Manual R toward enemy nearest cursor [T]",
        SDK::Keys::T, KeyBindType::Press));
    WorldsMenu->Add(new MenuSeparator(
        "CurrentR",
        "Enemies are not trapped: R plans initial damage/slow, Aurora portal value and enemy exits separately."));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Q-marked wave pullback and recoil-safe E"));
    FarmMenu->Add(new MenuBool(
        "HoldForChampion", "Do not spend farm spells while a champion contests", true));
    FarmMenu->Add(new MenuSlider(
        "ChampionHoldRange", "Champion contest range", 1100, 500, 1800));
    FarmMenu->Add(new MenuBool(
        "UseQ", "Mark a valuable line with Q", true));
    FarmMenu->Add(new MenuBool(
        "UseE", "Use E only after marked-wave pullback or without Q", true));
    FarmMenu->Add(new MenuSlider(
        "QMinimumUnits", "Minimum Q lane bodies", 3, 1, 8));
    FarmMenu->Add(new MenuSlider(
        "EMinimumUnits", "Minimum E lane bodies", 4, 1, 8));
    FarmMenu->Add(new MenuSlider(
        "LaneMana", "Minimum lane-clear mana (%)", 58, 0, 100));
    FarmMenu->Add(new MenuSlider(
        "JungleMana", "Minimum jungle-clear mana (%)", 35, 0, 100));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "One-trick state and geometry visualization"));
    CoachMenu->Add(new MenuBool(
        "DrawRanges", "Draw live Q and E ranges", true));
    CoachMenu->Add(new MenuBool(
        "DrawQ", "Draw selected Q1 line", true));
    CoachMenu->Add(new MenuBool(
        "DrawPullbacks", "Draw every marked body's Q2 path", true));
    CoachMenu->Add(new MenuBool(
        "DrawW", "Draw chosen W endpoint and wall route", true));
    CoachMenu->Add(new MenuBool(
        "DrawE", "Draw E aim and opposite recoil endpoint", true));
    CoachMenu->Add(new MenuBool(
        "DrawR", "Draw arena and current opposite portal destination", true));
    CoachMenu->Add(new MenuBool(
        "DrawState", "Draw passive, Q, W reset and arena state", true));
}

inline void OnLoad() {
    ActiveSequence = Sequence::None;
    CurrentPosture = Posture::Neutral;
    LastQPurpose = QPurpose::None;
    LastWPurpose = WPurpose::None;
    LastEPurpose = EPurpose::None;
    LastRPurpose = RPurpose::None;
    LastKnownMode = Mode::None;
    PassiveRecords.fill({});
    QMarks.fill({});
    EnemyWindows.fill({});
    SpiritExpireTicks.fill(0);
    PredictedSpiritCount = 0;
    ClearQState();
    QLastObservedTick = Q2LastTick = 0;
    WInvisible = WRealmHopper = WControllerOwned = false;
    WCastTick = WInvisibleUntil = WRealmHopperUntil = 0;
    LastChampionDamageTick = 0;
    LastWCooldownSeconds = 0.0f;
    LastWCooldownSampleTick = LastWResetTick = 0;
    WCooldownObserved = false;
    ECastTick = ERecoilUntil = 0;
    LastERecoilEndpoint = {};
    RArenaActive = RControllerOwned = false;
    RCastTick = RArenaExpireTick = RPortalReadyTick = 0;
    RLastPortalTick = 0;
    RArenaCenter = LastPortalDestination = {};
    LastPlayerPosition = {};
    LastPlayerPositionTick = 0;
    LastAutoTargetId = LastAutoCastTick = 0;
    LastAfterAttackTargetId = LastAfterAttackTick = 0;
    IncomingThreatTargetId = IncomingThreatUntil = 0;
    IncomingOneInstanceCrowdControl = false;
    IncomingPersistentCrowdControl = false;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEnd = {};
    LastQPlan = {};
    LastWPlan = {};
    LastEPlan = {};
    LastRPlan = {};
    RefreshRuntimeState();
}

inline void OnUnload() {
    TacticsMenu = PassiveMenu = HexMenu = VeilMenu = nullptr;
    WeirdingMenu = WorldsMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Pin Summoner's Rift mechanics to Riot 26.14 and CommunityDragon PC 16.14",
    "Reject release-era Aurora R trapping behavior after the 14.23 update",
    "Use current 32 base magic resistance rather than Aurora's release value",
    "Use current passive one-percent target maximum health base damage",
    "Add exactly 2.7 percent target maximum health per 100 ability power",
    "Apply the level-scaled passive monster cap without capping champion damage",
    "Proc Spirit Abjuration on every third attack or damaging spell application",
    "Normalize both 1/2/3 and legacy 2/4/6 passive telemetry encodings",
    "Expire incomplete passive stacks after four seconds",
    "Create one four-second spirit after a champion passive proc",
    "Keep at most four simultaneous spirits",
    "Heal 3 through 20 plus two percent AP per spirit each second",
    "Never invent the passive movement speed removed in patch 14.23",
    "Track predicted passive stacks per target rather than globally",
    "Let observed passive buff counts override predicted applications",
    "Count player auto attacks without issuing an attack command",
    "Yield spell timing so the player can supply the second passive application",
    "Seek a second passive proc only in an extended safe fight",
    "Use Q-AA-Q2 as the canonical level-one Electrocute trade",
    "Use Q-E-Q2 when no safe auto window exists at level two",
    "Support Q-AA-AA-Q2-AA-E for two passive procs in a long trade",
    "Open ordinary trades with Q before the player's auto attack",
    "Use current Q 45/70/95/120/145 plus forty percent AP damage",
    "Use current 900 Q cast range after the 14.23 update",
    "Use the 0.25-second Q cast and 1600 outbound missile speed",
    "Treat outbound Q as a piercing 90-wide projectile",
    "Reject Q1 through Yasuo, Samira or Mel projectile barriers",
    "Mark every champion, minion and monster actually intersected by Q1",
    "Generate narrow angular Q candidates instead of assuming center aim is best",
    "Prefer a Q line that marks both the champion and useful wave bodies",
    "Track each Q mark independently for 3.5 seconds",
    "Recognize the guaranteed Q2 bolt on every original marked target",
    "Model every Q2 return path from its marked body to Aurora's current position",
    "Value crossing bolts that pass through a priority champion",
    "Apply full Q2 damage to the first bolt reaching a target",
    "Apply twenty percent Q2 damage to every additional bolt on that target",
    "Apply the current Q2 minion modifier independently from extra-bolt damage",
    "Scale Q2 from one to 1.5 times damage with target missing health",
    "Delay single-target Q2 so E or autos increase missing-health damage",
    "Recast Q2 immediately when its current damage is lethal",
    "Recast Q2 immediately when it completes a passive proc",
    "Recast Q2 before the marked target escapes the useful fight window",
    "Recast Q2 just before expiry instead of losing controller-owned marks",
    "Never recast a manually cast Q merely because the controller dislikes it",
    "Never cancel a valuable auto windup with Q2 unless lethal or expiring",
    "Compare current Q2 alignment with the player's natural path endpoint",
    "Never issue movement to manufacture a better Q2 return angle",
    "Never steer the player's cursor to repair Q2 alignment",
    "Use Q1-Q2-E on a marked minion wave",
    "Pull Q2 before E can kill marked backline minions and erase their bolts",
    "Use Q1-E-Q2 against a lone champion when the mark has time",
    "Let Q2 auto-fire remain a fallback when manual Q ownership is detected",
    "Track Q2 auto-attack cancellation risk in the protected-windup policy",
    "Use Q2 return lines to reveal and continue tracking marked fog targets",
    "Use current W 22/21/20/19/18 cooldown telemetry",
    "Reserve the full 80 W mana cost for offensive routes",
    "Use a 300-unit ordinary W dash endpoint",
    "Extend W only to a validated terrain exit within 450 units",
    "Reject a wall hop whose terrain exit remains inside a wall",
    "Use W invisibility for 1/1.15/1.3/1.45/1.6 seconds by rank",
    "Use Realm Hopper movement speed 20/25/30/35/40 percent by rank",
    "Track the four-second Realm Hopper speed window independently from stealth",
    "Recognize that attacks or spell casts break W invisibility",
    "Do not cast another spell blindly during the W dash lockout",
    "Prefer side-angle W over a direct dash into melee",
    "Prefer a cursor-agreeing wall hop when the player is already leaving",
    "Use deceptive W direction changes only through player-owned movement",
    "Score W endpoint turret exposure before casting",
    "Score ready point-click lockdown at the W endpoint",
    "Score Poppy, Taliyah and Cassiopeia dash hazards at the W endpoint",
    "Reject offensive W during a valuable player auto windup",
    "Allow defensive W to override an unsafe attack windup",
    "Require key crowd control spent or target control before aggressive W",
    "Track the three-second recent champion-damage takedown window",
    "Detect abrupt W cooldown reduction as a takedown reset",
    "Spend W before a likely takedown only when the reset has real value",
    "Use a W reset to exit or re-angle rather than diving by default",
    "Never let reset optimism excuse a turret or lockdown endpoint",
    "Never automate Flash with W",
    "Use current E 70/110/150/190/230 plus seventy percent AP damage",
    "Treat E as an 825-range 175-wide non-projectile line",
    "Never reject E because of a projectile wall",
    "Predict E from Aurora's position at the end of its 0.35-second cast",
    "Apply the strong one-second decaying E slow without treating it as hard CC",
    "Recoil exactly 250 units opposite E's cast direction",
    "Use current 150 plus twice movement-speed recoil timing in threat policy",
    "Cast grounded or rooted E without pretending the recoil occurs",
    "Reject an E recoil endpoint inside terrain",
    "Reject an E recoil endpoint newly entering an enemy turret",
    "Reject an E recoil endpoint entering ready point-click lockdown",
    "Use E late in an all-in when early recoil would end the chase",
    "Use standalone E poke against long-range opponents when Q cannot trade safely",
    "Use E to create space from Camille or Ekko after their commitment",
    "Use E recoil to buffer one-instance displacements when landing is safe",
    "Use E on a directed gapcloser endpoint only with a safe backstep",
    "Use E turret-dive recoil only when it leaves rather than deepens turret exposure",
    "Allow E through very thin terrain only when the sampled endpoint is navigable",
    "Never spend E before Q2 on marked minions it would kill",
    "Use current R 175/275/375 plus seventy percent AP damage",
    "Use 100 R mana and current 140/120/100 cooldown telemetry",
    "Distinguish R's maximum 250 leap from its roughly 425-ahead arena center",
    "Keep R arena radius at 700 rather than centering a generic circle on Aurora",
    "Use current R arena durations 2.5/3.25/4 seconds",
    "Use current initial 30-percent slow and two-second duration",
    "Use current 50-percent boundary slow rather than stale 75 percent",
    "Use 1.5/1.75/2-second boundary slow duration by R rank",
    "Never assume enemies are trapped inside current Between Worlds",
    "Expect ordinary movement, blink and Flash exits from the current arena",
    "Penalize enemies with ready blinks unless allied control is present",
    "Require allied follow-up or Aurora follow-up spells for offensive R",
    "Prefer an R that enables two passive procs over an empty cinematic cast",
    "Use R as follow-up engage rather than blind primary engage by default",
    "Prefer two-versus-two and three-versus-three arena fights with allied follow-up",
    "Reject R leap endpoints entering turrets, lockdown or anti-dash zones",
    "Validate a wall exit up to 450 before accepting an R leap through terrain",
    "Reject unjumpable terrain that would corrupt R's intended direction",
    "Account for the slight delayed R damage wave in target prediction",
    "Pre-position R damage for a predictable reappearance only when geometry agrees",
    "Use R leap unstoppable against one-instance crowd control",
    "Do not pretend R leap cleanses suppression or persistent crowd control",
    "Do not spend R into Malzahar, Warwick or Skarner suppression as a fake cleanse",
    "Track arena center and expiry even for a player-cast manual R",
    "Never early-end a player-owned manual arena automatically",
    "Map every boundary contact to the diametrically opposite portal destination",
    "Require real boundary proximity before planning a portal interaction",
    "Track a short portal transit cooldown conservatively after a long position jump",
    "Reject a portal destination inside terrain",
    "Reject a portal destination under an enemy turret",
    "Reject a portal destination inside ready point-click lockdown",
    "Use portal untargetability against one-instance targeted damage",
    "Use portal timing to drop a pending turret shot when the destination is safe",
    "Use portal timing against Karthus or Fizz delayed damage only with live threat evidence",
    "Preserve W, E and portal as separate mobility resources by default",
    "Allow E inward so recoil carries Aurora across the portal during incoming CC",
    "Allow W-hidden portal only behind an explicit opt-in and safe destination",
    "End controller-owned R before a player-requested unsafe forced portal",
    "End controller-owned R after targets escape and no defensive portal value remains",
    "Never issue movement commands to touch an R boundary",
    "Never issue attack-move commands inside R",
    "Never cast summoner spells for Q-Flash, E-Flash or Q-E-Flash combos",
    "Expose Flash extensions as player execution rather than automation",
    "Use exact mitigated Q2, E and R damage for kill secure",
    "Do not dump R into an intact spell shield for ordinary kill secure",
    "Use Q before E in ordinary lane waveclear",
    "Prioritize cannon and super minions as marked-wave anchors",
    "Require multiple lane bodies before spending Q or E",
    "Use separate lane and jungle mana thresholds",
    "Never spend W or R merely to farm",
    "Hold farm spells while an enemy champion contests the wave",
    "Keep current high-elo identity: burst flank or follow-up, not front-line tank",
    "Cooperate with the jungler by rewarding allied follow-up in R scoring",
    "Cooperate with the player's cursor on Q, W and offensive R direction",
    "Cooperate with the player's autos by yielding spell windows",
    "Respect manual Q, W, E and R ownership in every state transition",
    "Draw every tracked Q2 return path for one-trick alignment coaching",
    "Draw W wall endpoints before they are committed",
    "Draw E aim and recoil as two opposite paths",
    "Draw the live R arena and opposite portal destination",
    "Expose predicted passive stacks, procs and spirit count",
    "Expose W reset detection and arena remaining time",
    "Own the complete Aurora decision loop and never fall back to generic Q-W-E-R priority",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Aurora";
    controller.ControllerId = "champion.kuroaio.ai.aurora.controller";
    controller.KitRevision = "Riot 26.14 / CommunityDragon PC 16.14";
    controller.ResearchArtifact = "AI/Research/AIAurora.md";
    controller.ImplementationSummary =
        "Three-hit/player-auto weaving, multi-mark Q2 return alignment, safe "
        "W reset routes, recoil-validated non-projectile E, and separately "
        "modeled R leap/arena/opposite portal with CC buffering.";
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
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Aurora
