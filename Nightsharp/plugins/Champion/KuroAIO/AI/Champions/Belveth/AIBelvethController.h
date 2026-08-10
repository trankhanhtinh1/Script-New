#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIBelvethGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Belveth {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::AutoAttackRange;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CursorDirectionAgrees;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NameEquals;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::ObjectEventIsAllied;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Ready;
using ControllerHelpers::RemainingMilliseconds;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContainsAny;
using ControllerHelpers::SpellRank;
using ControllerHelpers::UnitByNetworkId;

enum class Sequence : int {
    None,
    AutoQAuto,
    QWQChase,
    WLockedE,
    DefensiveE,
    ECancelDodge,
    CoralExecute,
    CoralSave,
    EnhancedMacro,
    TrueFormWallFlank,
    JungleWallCancel,
    SecondEntry,
    PlayerLed,
};

enum class Posture : int {
    Neutral,
    Duel,
    ShortTrade,
    SecondEntry,
    Chase,
    Execute,
    Kite,
    Flee,
    Objective,
    Clear,
};

enum class RReason : int {
    None,
    FirstForm,
    Execute,
    HealSave,
    EnhancedMacro,
    Expiring,
    Manual,
};

struct EnemyWindow {
    int NetworkId = 0;
    int CommittedUntil = 0;
    int HardCrowdControlSpentUntil = 0;
    int DashSpentUntil = 0;
};

struct QPath {
    Vector3 Aim = {};
    Vector3 Endpoint = {};
    float TravelDistance = 0.0f;
    bool Valid = false;
    bool WallTouched = false;
    bool TerrainCrossed = false;
    bool HasWallExit = false;
};

struct QPlan {
    Vector3 Aim = {};
    Vector3 Endpoint = {};
    Quadrant Direction = Quadrant::Invalid;
    QPurpose Purpose = QPurpose::None;
    QEvaluation Evaluation = {};
    int TargetId = 0;
    int PathHits = 0;
    bool TargetFirst = false;
    bool Valid = false;
};

struct WPlan {
    Vector3 Aim = {};
    std::uint8_t ResetMask = 0u;
    int TargetId = 0;
    int HitCount = 0;
    WContext Context = {};
    bool Valid = false;
};

struct CoralRecord {
    int NetworkId = 0;
    Vector3 Position = {};
    int SpawnTick = 0;
    int ExpireTick = 0;
    int LastSeenTick = 0;
    bool Enhanced = false;
    bool ConfirmedAllied = false;
};

struct RPlan {
    int CoralId = 0;
    Vector3 Position = {};
    RContext Context = {};
    REvaluation Evaluation = {};
    RReason Reason = RReason::None;
    bool Valid = false;
};

struct EPlan {
    EStartContext Context = {};
    int DesiredTargetId = 0;
    int ForcedTargetId = 0;
    float ExpectedDamage = 0.0f;
    bool Defensive = false;
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
inline RReason LastRReason = RReason::None;
inline int LastDecisionTargetId = 0;

inline DirectionState QDirections = {};
inline HudCalibration QHudCalibration = {};
inline int LastHudMask = -1;
inline int PendingHudPreviousMask = -1;
inline Quadrant PendingHudSpent = Quadrant::Invalid;
inline int PendingHudSpentUntil = 0;
inline std::uint8_t PendingHudRefreshMask = 0u;
inline int PendingHudRefreshUntil = 0;
inline int LastQCastTick = 0;
inline int LastQTargetId = 0;
inline QPurpose LastQPurpose = QPurpose::None;
inline Quadrant LastQDirection = Quadrant::Invalid;
inline QPlan LastQPlan = {};

inline int LastWCastTick = 0;
inline int LastWHitTick = 0;
inline int LastWTargetId = 0;
inline Vector3 LastWOrigin = {};
inline Vector3 LastWAim = {};
inline WPlan LastWPlan = {};

inline bool EActive = false;
inline int EStartTick = 0;
inline int EEndTick = 0;
inline int EDesiredTargetId = 0;
inline int EForcedTargetId = 0;

inline bool RChannelActive = false;
inline int RChannelUntil = 0;
inline bool TrueFormActive = false;
inline bool EnhancedFormActive = false;
inline int TrueFormExpireTick = 0;
inline int LastRCastTick = 0;
inline RPlan LastRPlan = {};
inline std::array<CoralRecord, 20> Corals = {};
inline Vector3 RecentEnhancedSourcePosition = {};
inline int RecentEnhancedSourceUntil = 0;

inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastAutoProcessTick = 0;
inline bool PassiveSheenActive = false;
inline int PassiveSheenExpireTick = 0;
inline int LavenderStacks = 0;
inline RPassiveTracker RAttackTracker = {};

inline EPlan LastEPlan = {};
inline int LastECastTick = 0;
inline int LastLocalSpellTick = 0;
inline int PlayerOverrideUntil = 0;
inline int SequenceExpireTick = 0;

inline std::array<EnemyWindow, 16> EnemyWindows = {};
inline int IncomingThreatUntil = 0;
inline int IncomingImpactTick = 0;
inline float IncomingDamage = 0.0f;
inline bool IncomingTrueDamageOnly = false;
inline bool IncomingHardCrowdControl = false;
inline Vector3 IncomingLineStart = {};
inline Vector3 IncomingLineEnd = {};
inline float IncomingLineWidth = 0.0f;
inline int GapcloserTargetId = 0;
inline Vector3 GapcloserEnd = {};
inline int GapcloserExpireTick = 0;
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;

struct QPathCache {
    Vector3 Origin = {};
    Vector3 Direction = {};
    int Tick = 0;
    bool TrueForm = false;
    QPath Path = {};
};

inline QPathCache LastQPathCache = {};

inline constexpr int kQHudLearnMs = 650;
inline constexpr int kWRefreshObserveMs = 800;
inline constexpr int kWeaveWindowMs = 360;
inline constexpr int kWLockFollowupMs = 1250;
inline constexpr int kIncomingWindowMs = 1300;

inline float BonusAttackSpeedPercent() {
    const auto player = GameObjects::Player();
    return player.IsValid()
        ? std::max(0.0f, player.AttackSpeedMod() - 1.0f) * 100.0f
        : 0.0f;
}

inline bool HasTrueForm() {
    const auto player = GameObjects::Player();
    return TrueFormActive ||
        (player.IsValid() && player.HasBuff("BelvethRSteroid"));
}

inline float TrueFormSecondsRemaining() {
    return TrueFormExpireTick > Now()
        ? static_cast<float>(TrueFormExpireTick - Now()) / 1000.0f
        : 0.0f;
}

inline void SetFormExpiryFromStacks(int now) {
    const float duration = RFormDurationSeconds(LavenderStacks);
    TrueFormExpireTick = std::isinf(duration)
        ? std::numeric_limits<int>::max()
        : now + static_cast<int>(duration * 1000.0f);
}

inline bool HasEnhancedForm() {
    return HasTrueForm() && EnhancedFormActive;
}

inline bool EnemyCommitted(int networkId) {
    const EnemyWindow* record =
        ControllerHelpers::EnemyCastWindowById(EnemyWindows, networkId);
    return record && record->CommittedUntil >= Now();
}

inline bool EnemyDashSpent(int networkId) {
    const EnemyWindow* record =
        ControllerHelpers::EnemyCastWindowById(EnemyWindows, networkId);
    return record && record->DashSpentUntil >= Now();
}

inline bool EnemyHardCrowdControlSpent(int networkId) {
    const EnemyWindow* record =
        ControllerHelpers::EnemyCastWindowById(EnemyWindows, networkId);
    return record && record->HardCrowdControlSpentUntil >= Now();
}

inline EnemyWindow* FindEnemyWindow(int networkId,
                                    bool create = false) {
    return ControllerHelpers::FindEnemyCastWindow(
        EnemyWindows, networkId, create);
}

inline int ReadQHudMask() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return -1;
    char name[40]{};
    for (int mask = 0; mask <= 15; ++mask) {
        _snprintf_s(
            name, sizeof(name), _TRUNCATE,
            "BelvethQHudIcon%d", mask);
        if (player.HasBuff(name)) return mask;
    }
    return -1;
}

inline bool InternalDirectionReady(Quadrant quadrant, int now = -1) {
    if (now < 0) now = Now();
    const bool fallback = QDirections.Ready(quadrant, now);
    return QHudCalibration.HudReady(LastHudMask, quadrant, fallback);
}

inline std::array<bool, 4> ReadyDirections(int now = -1) {
    if (now < 0) now = Now();
    std::array<bool, 4> result = {};
    for (int i = 0; i < 4; ++i) {
        result[static_cast<std::size_t>(i)] =
            InternalDirectionReady(static_cast<Quadrant>(i), now);
    }
    return result;
}

inline int ReadyDirectionCount(int now = -1) {
    const auto ready = ReadyDirections(now);
    return static_cast<int>(std::count(ready.begin(), ready.end(), true));
}

inline void SyncQHud() {
    const int now = Now();
    const int observed = ReadQHudMask();
    if (observed < 0) return;
    if (LastHudMask < 0) {
        LastHudMask = observed;
        return;
    }
    if (observed != LastHudMask) {
        if (PendingHudSpentUntil >= now &&
            QuadrantIndex(PendingHudSpent) >= 0) {
            (void)QHudCalibration.LearnSpend(
                PendingHudPreviousMask >= 0
                    ? PendingHudPreviousMask : LastHudMask,
                observed, PendingHudSpent);
        }
        if (PendingHudRefreshUntil >= now &&
            PendingHudRefreshMask != 0u) {
            for (int i = 0; i < 4; ++i) {
                if ((PendingHudRefreshMask &
                     static_cast<std::uint8_t>(1u << i)) != 0u) {
                    (void)QHudCalibration.LearnRefresh(
                        LastHudMask, observed,
                        static_cast<Quadrant>(i));
                }
            }
            PendingHudRefreshMask = 0u;
            PendingHudRefreshUntil = 0;
        }
        PendingHudSpent = Quadrant::Invalid;
        PendingHudSpentUntil = 0;
        LastHudMask = observed;
    }

    // A learned HUD bit is authoritative and repairs estimates after Navori,
    // W multi-refresh or a manual cast that arrived before our event bridge.
    for (int i = 0; i < 4; ++i) {
        const Quadrant quadrant = static_cast<Quadrant>(i);
        if (!QHudCalibration.Knows(quadrant)) continue;
        if (QHudCalibration.HudReady(observed, quadrant, false)) {
            QDirections.ReadyTick[static_cast<std::size_t>(i)] = now;
        }
    }
}

inline float QPhysicalDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const bool monster = AIMinionClient(target.Address()).IsJungle();
    const bool minion = target.IsMinion() && !monster;
    return player.CalculatePhysicalDamage(
        target,
        QRawDamage(
            SpellRank(0), player.TotalAttackDamage(), monster, minion));
}

inline float WMagicalDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return player.CalculateMagicDamage(
        target,
        WRawDamage(
            SpellRank(1), player.BonusAttackDamage(), player.AP()));
}

inline float RExplosionDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return RExplosionRawDamage(
        SpellRank(3), player.AP(), target.Health(), target.MaxHealth(),
        AIMinionClient(target.Address()).IsJungle());
}

inline bool TargetEscaping(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return false;
    const Vector3 end = target.PathEnd();
    if (!end.IsValid() || end.IsZero()) return false;
    return end.Distance2D(player.Position()) >
        target.Position().Distance2D(player.Position()) + 45.0f;
}

inline bool CursorRetreatsFrom(const AIBaseClient& threat) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !threat.IsValid()) return false;
    const Vector3 away = SharedGeometry::Direction2D(
        threat.Position(), player.Position());
    const Vector3 cursor = SharedGeometry::Direction2D(
        player.Position(), Game::CursorPos());
    return !away.IsZero() && !cursor.IsZero() && away.Dot(cursor) >= 0.55f;
}

inline bool PointInsideIncomingLine(const Vector3& point) {
    if (IncomingThreatUntil < Now() || !IncomingLineStart.IsValid() ||
        !IncomingLineEnd.IsValid()) return false;
    const auto player = GameObjects::Player();
    const auto projection = SharedGeometry::ProjectPointToSegment2D(
        point, IncomingLineStart, IncomingLineEnd);
    return projection.Distance <=
        std::max(35.0f, IncomingLineWidth) +
        (player.IsValid() ? player.BoundingRadius() : 65.0f);
}

inline bool QEndpointSafe(const Vector3& endpoint,
                          const AIHeroClient& target,
                          bool defensive) {
    if (!endpoint.IsValid() || endpoint.IsZero() ||
        SDK::NavMesh::IsWall(endpoint)) return false;
    if (HasReadyDashHazardAt(endpoint)) return false;
    if (!defensive && HasReadyPointClickThreatAt(endpoint)) return false;
    if (!defensive && Engine::UnderEnemyTurret(endpoint) &&
        !Bool(QMenu, "AllowLethalDive", false)) return false;
    const int maximum = Slider(QMenu, "MaxEndpointEnemies", 2);
    if (!defensive && Engine::CountEnemiesAt(endpoint, 575.0f) > maximum) {
        return false;
    }
    if (target.IsValid() && Engine::PositionDangerScore(
            endpoint, target, Engine::ResolvedSpecs[0]) < -1200.0f) {
        return false;
    }
    return true;
}

inline QPath TraceQPathUncached(const Vector3& origin,
                                const Vector3& direction,
                                bool trueForm) {
    QPath result{};
    if (!origin.IsValid() || direction.IsZero()) return result;
    const float maximum = trueForm
        ? kQTrueFormWallDistance : kQOpenDashDistance;
    Vector3 lastOpen = origin;
    bool insideWall = false;
    bool sawWall = false;
    float firstWallDistance = 0.0f;
    for (float travel = 14.0f; travel <= maximum; travel += 14.0f) {
        Vector3 sample = origin + direction * travel;
        sample.y = origin.y;
        const bool wall = SDK::NavMesh::IsWall(sample);
        if (wall) {
            if (!sawWall) firstWallDistance = travel;
            sawWall = true;
            insideWall = true;
            if (!trueForm) {
                result.Aim = origin + direction * kQOpenDashDistance;
                result.Endpoint = lastOpen;
                result.TravelDistance = origin.Distance2D(lastOpen);
                result.WallTouched = true;
                result.Valid = result.TravelDistance >= 10.0f;
                return result;
            }
            continue;
        }
        if (insideWall && trueForm) {
            // Exit just beyond terrain. The live dash extends only as far as
            // required to clear terrain, capped by MaxDistanceOverWalls=625.
            result.Aim = sample;
            result.Endpoint = sample;
            result.TravelDistance = travel;
            result.WallTouched = true;
            result.TerrainCrossed = true;
            result.HasWallExit = travel <= kQTrueFormWallDistance &&
                firstWallDistance <= kQOpenDashDistance + 1.0f;
            result.Valid = result.HasWallExit;
            return result;
        }
        lastOpen = sample;
        if (!sawWall && travel >= kQOpenDashDistance - 14.0f) break;
    }
    if (sawWall) return result;
    result.Aim = origin + direction * kQOpenDashDistance;
    result.Endpoint = result.Aim;
    result.TravelDistance = kQOpenDashDistance;
    result.Valid = !SDK::NavMesh::IsWall(result.Endpoint);
    return result;
}

inline QPath TraceQPath(const Vector3& origin,
                        const Vector3& direction,
                        bool trueForm) {
    const int now = Now();
    const bool reusable = LastQPathCache.Tick > 0 && now >= LastQPathCache.Tick &&
        now - LastQPathCache.Tick <= 56 &&
        LastQPathCache.TrueForm == trueForm &&
        LastQPathCache.Origin.Distance2D(origin) <= 20.0f &&
        !LastQPathCache.Direction.IsZero() &&
        LastQPathCache.Direction.Dot(direction) >= 0.998f;
    if (reusable) return LastQPathCache.Path;

    const QPath path = TraceQPathUncached(origin, direction, trueForm);
    LastQPathCache.Origin = origin;
    LastQPathCache.Direction = direction;
    LastQPathCache.TrueForm = trueForm;
    LastQPathCache.Tick = now;
    LastQPathCache.Path = path;
    return path;
}

inline void AppendBody(std::vector<Body>& bodies,
                       const AIBaseClient& unit) {
    if (!unit.IsValid() || unit.IsDead() || !unit.IsEnemy() ||
        !unit.IsTargetable()) return;
    const AIMinionClient minion(unit.Address());
    Body body{};
    body.Position = unit.Position();
    body.Radius = unit.BoundingRadius();
    body.Id = static_cast<int>(unit.NetworkId());
    body.Champion = unit.IsHero();
    body.Monster = minion.IsJungle();
    body.Epic = body.Monster && IsEpicMonster(unit);
    body.Health = unit.Health();
    body.MaximumHealth = unit.MaxHealth();
    bodies.push_back(body);
}

inline std::vector<Body> QCollisionBodies() {
    std::vector<Body> result;
    result.reserve(72);
    for (const auto& minion : GameObjects::EnemyMinions()) {
        AppendBody(result, minion);
    }
    for (const auto& monster : GameObjects::Jungle()) {
        AppendBody(result, monster);
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        AppendBody(result, enemy);
    }
    return result;
}

inline QPurpose PurposeForQ(Mode mode,
                            const AIBaseClient& target,
                            bool killable,
                            bool attackJustCompleted) {
    if (IncomingThreatUntil >= Now() &&
        Bool(QMenu, "EvadeSkillshots", true)) return QPurpose::Evade;
    if (mode == Mode::Flee) return QPurpose::Flee;
    if (killable) return QPurpose::Execute;
    const AIMinionClient minion(target.Address());
    if (minion.IsJungle()) {
        return ControllerHelpers::NearTerrain(
            GameObjects::Player().Position(), 125.0f, 18)
            ? QPurpose::WallCancel : QPurpose::Farm;
    }
    if (target.IsMinion()) return QPurpose::Farm;
    if (attackJustCompleted) return QPurpose::Weave;
    return QPurpose::Chase;
}

inline QPlan BuildQPlan(const AIBaseClient& target,
                        Mode mode,
                        QPurpose forcedPurpose = QPurpose::None) {
    QPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || PlayerMobilityLocked() || !Ready(0)) return best;
    if (!target.IsValid() && mode != Mode::Flee &&
        forcedPurpose != QPurpose::Evade) return best;

    const int now = Now();
    const bool targetHero = target.IsValid() && target.IsHero();
    AIHeroClient heroTarget = targetHero
        ? AIHeroClient(target.Address()) : AIHeroClient{};
    Vector3 desiredPosition = Game::CursorPos();
    if (target.IsValid() && mode != Mode::Flee &&
        forcedPurpose != QPurpose::Evade) {
        desiredPosition = PredictPosition(target, 0.18f);
    }
    Vector3 base = SharedGeometry::Direction2D(
        player.Position(), desiredPosition);
    if (forcedPurpose == QPurpose::Evade &&
        PointInsideIncomingLine(player.Position())) {
        const Vector3 threatDirection = SharedGeometry::Direction2D(
            IncomingLineStart, IncomingLineEnd);
        const Vector3 cursorDirection = SharedGeometry::Direction2D(
            player.Position(), Game::CursorPos());
        if (!threatDirection.IsZero()) {
            const Vector3 left = SharedGeometry::Rotate2D(
                threatDirection, SharedGeometry::kPi * 0.5f);
            const Vector3 right = SharedGeometry::Rotate2D(
                threatDirection, -SharedGeometry::kPi * 0.5f);
            base = !cursorDirection.IsZero() &&
                cursorDirection.Dot(left) >= cursorDirection.Dot(right)
                ? left : right;
        }
    }
    if (base.IsZero()) return best;

    const auto ready = ReadyDirections(now);
    const int readyCount = static_cast<int>(
        std::count(ready.begin(), ready.end(), true));
    const int forwardCount = ForwardDirectionCount(base, ready);
    const bool attackJustCompleted = LastAutoTick > 0 &&
        now - LastAutoTick <= kWeaveWindowMs &&
        (!target.IsValid() || LastAutoTargetId ==
            static_cast<int>(target.NetworkId()));
    const bool killable = target.IsValid() &&
        QPhysicalDamage(target) * 0.92f >= target.Health();
    const QPurpose purpose = forcedPurpose != QPurpose::None
        ? forcedPurpose
        : PurposeForQ(mode, target, killable, attackJustCompleted);
    const bool trueForm = HasTrueForm();
    const auto bodies = QCollisionBodies();
    float bestScore = -FLT_MAX;

    static constexpr std::array<float, 7> offsets = {
        0.0f, 0.045f, -0.045f, 0.11f, -0.11f, 0.20f, -0.20f,
    };
    for (int i = 0; i < 4; ++i) {
        if (!ready[static_cast<std::size_t>(i)]) continue;
        const Quadrant quadrant = static_cast<Quadrant>(i);
        for (float offset : offsets) {
            const Vector3 rotated = SharedGeometry::Rotate2D(base, offset);
            const Vector3 direction = BoundaryBiasedDirection(
                rotated, quadrant);
            if (direction.IsZero() ||
                !DirectionInsideQuadrant(direction, quadrant)) continue;
            const QPath path = TraceQPath(
                player.Position(), direction, trueForm);
            if (!path.Valid) continue;

            const bool defensive = purpose == QPurpose::Evade ||
                purpose == QPurpose::Flee;
            QContext context{};
            context.DirectionReady = true;
            context.GlobalReady = Ready(0);
            context.EndpointValid = path.Valid;
            context.TerrainCrossed = path.TerrainCrossed;
            context.TrueForm = trueForm;
            context.HasWallExit = path.HasWallExit;
            context.PlayerGrounded = PlayerMobilityLocked();
            context.PlayerWindingUp = Orbwalker::IsWindingUp();
            context.AttackJustCompleted = attackJustCompleted;
            context.TargetHit = target.IsValid() && QHits(
                player.Position(), path.Endpoint,
                Body{ desiredPosition, target.BoundingRadius(),
                      static_cast<int>(target.NetworkId()), true,
                      targetHero,
                      AIMinionClient(target.Address()).IsJungle(),
                      IsEpicMonster(target), target.Health(), target.MaxHealth() });
            context.TargetInAttackRangeBefore = target.IsValid() &&
                InAutoAttackRange(target, 20.0f);
            const float attackRange = target.IsValid()
                ? player.AttackRange() + target.BoundingRadius() : 0.0f;
            context.TargetInAttackRangeAfter = target.IsValid() &&
                path.Endpoint.Distance2D(desiredPosition) <= attackRange + 25.0f;
            context.TargetKillable = killable;
            context.IncomingSkillshot = IncomingThreatUntil >= now;
            context.DodgesIncomingSkillshot =
                context.IncomingSkillshot &&
                PointInsideIncomingLine(player.Position()) &&
                !PointInsideIncomingLine(path.Endpoint);
            context.CursorAgrees = CursorDirectionAgrees(path.Endpoint, -0.12f);
            context.CursorDot = SharedGeometry::Direction2D(
                player.Position(), Game::CursorPos()).Dot(direction);
            context.DestinationSafe = QEndpointSafe(
                path.Endpoint, heroTarget, defensive);
            context.WReady = Ready(1);
            const bool reliableW = targetHero &&
                (Engine::IsHardCrowdControlled(heroTarget) ||
                 SDK::HasBuffOfType(heroTarget, SDK::BuffType::Slow) ||
                 EnemyDashSpent(static_cast<int>(target.NetworkId())) ||
                 EnemyCommitted(static_cast<int>(target.NetworkId())));
            context.WCanRefreshSpentDirection = context.WReady &&
                targetHero && reliableW &&
                path.Endpoint.Distance2D(target.Position()) <=
                    kWGameplayRange + target.BoundingRadius() &&
                QuadrantForPoints(path.Endpoint, target.Position()) == quadrant;
            context.TargetCommitted = targetHero &&
                EnemyCommitted(static_cast<int>(target.NetworkId()));
            context.TargetEscaping = target.IsValid() && TargetEscaping(target);
            context.FarmTarget = target.IsValid() && target.IsMinion();
            context.MonsterTarget = target.IsValid() &&
                AIMinionClient(target.Address()).IsJungle();
            context.NearWall = path.WallTouched ||
                ControllerHelpers::NearTerrain(player.Position(), 125.0f, 18);
            context.ReadyDirectionCount = readyCount;
            context.ForwardDirectionCount = forwardCount;
            context.EnemiesAtDestination = Engine::CountEnemiesAt(
                path.Endpoint, 575.0f);
            context.AlliesAtDestination = Engine::CountAlliesAt(
                path.Endpoint, 650.0f);
            context.TargetDistanceBefore = target.IsValid()
                ? player.Position().Distance2D(target.Position()) : FLT_MAX;
            context.TargetDistanceAfter = target.IsValid()
                ? path.Endpoint.Distance2D(desiredPosition) : FLT_MAX;
            context.Purpose = purpose;

            QEvaluation evaluation = EvaluateQ(context);
            if (!evaluation.Cast) continue;
            int pathHits = 0;
            for (const auto& body : bodies) {
                if (QHits(player.Position(), path.Endpoint, body)) ++pathHits;
            }
            const int firstIndex = FirstQBodyIndex(
                player.Position(), path.Endpoint, bodies);
            const bool targetFirst = target.IsValid() && firstIndex >= 0 &&
                bodies[static_cast<std::size_t>(firstIndex)].Id ==
                    static_cast<int>(target.NetworkId());
            float score = evaluation.Score +
                static_cast<float>(pathHits) *
                    (purpose == QPurpose::Farm ||
                     purpose == QPurpose::WallCancel ? 45.0f : 8.0f) +
                (targetFirst ? 70.0f : 0.0f);
            if ((purpose == QPurpose::Farm ||
                 purpose == QPurpose::WallCancel) && pathHits < 1) continue;
            if (purpose == QPurpose::WallFlank && !path.TerrainCrossed) {
                score -= 170.0f;
            }
            if (score > bestScore) {
                bestScore = score;
                best.Aim = path.Aim;
                best.Endpoint = path.Endpoint;
                best.Direction = quadrant;
                best.Purpose = purpose;
                best.Evaluation = evaluation;
                best.Evaluation.Score = score;
                best.TargetId = target.IsValid()
                    ? static_cast<int>(target.NetworkId()) : 0;
                best.PathHits = pathHits;
                best.TargetFirst = targetFirst;
                best.Valid = true;
            }
        }
    }
    return best;
}

inline bool CastQPlan(const QPlan& plan, bool reactive = false) {
    if (!plan.Valid || !CastThrottleReady(0, 30, reactive ? 0 : -1)) {
        return false;
    }
    if (!Engine::ControllerCastPosition(0, plan.Aim)) return false;
    const int now = Now();
    LastQPlan = plan;
    LastQPurpose = plan.Purpose;
    LastQDirection = plan.Direction;
    LastQTargetId = plan.TargetId;
    LastQCastTick = now;
    QDirections.Spend(
        plan.Direction, now,
        QPerDirectionCooldownSeconds(
            SpellRank(0), BonusAttackSpeedPercent()),
        QGlobalLockSeconds(SpellRank(0)));
    PendingHudPreviousMask = LastHudMask;
    PendingHudSpent = plan.Direction;
    PendingHudSpentUntil = now + kQHudLearnMs;
    switch (plan.Purpose) {
    case QPurpose::Weave: ActiveSequence = Sequence::AutoQAuto; break;
    case QPurpose::WallFlank: ActiveSequence = Sequence::TrueFormWallFlank; break;
    case QPurpose::WallCancel: ActiveSequence = Sequence::JungleWallCancel; break;
    case QPurpose::Evade: ActiveSequence = Sequence::ECancelDodge; break;
    default: break;
    }
    return true;
}

inline int BitCount4(std::uint8_t mask) {
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        if ((mask & static_cast<std::uint8_t>(1u << i)) != 0u) ++count;
    }
    return count;
}

inline std::uint8_t ReadyDirectionMask(int now = -1) {
    if (now < 0) now = Now();
    std::uint8_t mask = 0u;
    const auto ready = ReadyDirections(now);
    for (int i = 0; i < 4; ++i) {
        if (ready[static_cast<std::size_t>(i)]) {
            mask = static_cast<std::uint8_t>(mask | (1u << i));
        }
    }
    return mask;
}

inline SDK::HitChance RequiredWHitchance(bool reactive) {
    if (reactive) return SDK::HitChance::High;
    SDK::HitChance baseChance = SDK::HitChance::High;
    switch (List(WMenu, "Hitchance", 1)) {
    case 0: baseChance = SDK::HitChance::Medium; break;
    case 2: baseChance = SDK::HitChance::VeryHigh; break;
    case 3: baseChance = SDK::HitChance::Immobile; break;
    default: baseChance = SDK::HitChance::High; break;
    }
    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo && baseChance != SDK::HitChance::Immobile) {
        if (baseChance == SDK::HitChance::VeryHigh) baseChance = SDK::HitChance::High;
        else if (baseChance == SDK::HitChance::High) baseChance = SDK::HitChance::Medium;
    }
    return baseChance;
}

inline std::vector<Vector3> WCandidates(const AIHeroClient& target) {
    std::vector<Vector3> candidates;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(
            target, kWGameplayRange + 180.0f)) return candidates;
    Vector3 primary = PredictPosition(target, kWCastSeconds + 0.12f);
    if (Engine::RuntimeSpells[1]) {
        const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(target);
        const Vector3 cast = prediction.GetCastPosition();
        if (cast.IsValid() && !cast.IsZero()) primary = cast;
    }
    candidates.push_back(primary);
    if (target.IsDashing() && target.PathEnd().IsValid()) {
        candidates.push_back(target.PathEnd());
    }
    if (static_cast<int>(target.NetworkId()) == GapcloserTargetId &&
        GapcloserExpireTick >= Now() && GapcloserEnd.IsValid()) {
        candidates.push_back(GapcloserEnd);
    }

    // Multi-target W can refund more than one independent Q sector. Aim down
    // the bisector only when both predicted bodies still fit the real line.
    for (const auto& other : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(other, kWGameplayRange + 180.0f) ||
            other.NetworkId() == target.NetworkId()) continue;
        const Vector3 secondary = PredictPosition(
            other, kWCastSeconds + 0.12f);
        const Vector3 a = SharedGeometry::Direction2D(
            player.Position(), primary);
        const Vector3 b = SharedGeometry::Direction2D(
            player.Position(), secondary);
        Vector3 bisector = a + b;
        const float length = std::sqrt(
            bisector.x * bisector.x + bisector.z * bisector.z);
        if (length <= 0.001f) continue;
        bisector.x /= length;
        bisector.z /= length;
        candidates.push_back(
            player.Position() + bisector * kWGameplayRange);
    }
    return candidates;
}

inline WPlan BuildWPlan(const AIHeroClient& target,
                        bool interrupt = false,
                        bool gapcloser = false,
                        bool peel = false) {
    WPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1) || EActive ||
        !Engine::ValidEnemy(target, kWGameplayRange + 180.0f)) return best;

    SDK::HitChance observed = SDK::HitChance::Medium;
    if (Engine::RuntimeSpells[1]) {
        observed = Engine::RuntimeSpells[1]->GetPrediction(target).Hitchance;
    }
    const bool reliable = Engine::IsHardCrowdControlled(target) ||
        SDK::HasBuffOfType(target, SDK::BuffType::Slow) ||
        EnemyDashSpent(static_cast<int>(target.NetworkId())) ||
        EnemyCommitted(static_cast<int>(target.NetworkId())) ||
        target.IsDashing();
    const std::uint8_t readyMask = ReadyDirectionMask();
    const std::uint8_t spentMask = static_cast<std::uint8_t>(~readyMask) & 0xFu;
    const auto candidates = WCandidates(target);
    float bestScore = -FLT_MAX;
    for (const Vector3& aim : candidates) {
        if (!aim.IsValid() || aim.IsZero()) continue;
        const Vector3 direction = SharedGeometry::Direction2D(
            player.Position(), aim);
        if (direction.IsZero()) continue;
        const Vector3 end = player.Position() +
            direction * kWGameplayRange;
        std::vector<Body> hits;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!Engine::ValidEnemy(enemy, kWGameplayRange + 220.0f)) continue;
            Body body{};
            body.Position = PredictPosition(enemy, kWCastSeconds + 0.12f);
            body.Radius = enemy.BoundingRadius();
            body.Id = static_cast<int>(enemy.NetworkId());
            body.Champion = true;
            if (WLineHits(player.Position(), end, body)) hits.push_back(body);
        }
        const bool primaryHit = std::any_of(
            hits.begin(), hits.end(), [&](const Body& body) {
                return body.Id == static_cast<int>(target.NetworkId());
            });
        if (!primaryHit) continue;
        const std::uint8_t resetMask = WResetMask(player.Position(), hits);
        const std::uint8_t refunded = static_cast<std::uint8_t>(
            resetMask & spentMask);
        WContext context{};
        context.Ready = true;
        context.TargetValid = true;
        context.PredictionHits = true;
        context.HighHitchance =
            static_cast<int>(observed) >=
            static_cast<int>(RequiredWHitchance(
                interrupt || gapcloser || peel));
        context.TargetHardCrowdControlled =
            Engine::IsHardCrowdControlled(target);
        context.TargetSlowed = SDK::HasBuffOfType(
            target, SDK::BuffType::Slow);
        context.TargetDashSpent = EnemyDashSpent(
            static_cast<int>(target.NetworkId())) || target.IsDashing();
        context.TargetCommitted = EnemyCommitted(
            static_cast<int>(target.NetworkId()));
        context.Interrupt = interrupt;
        context.Gapcloser = gapcloser;
        context.Peel = peel;
        context.Lethal = WMagicalDamage(target) + 2.0f >= target.Health();
        context.PlayerWindingUp = Orbwalker::IsWindingUp();
        context.PlayerInE = EActive;
        context.ResetsSpentQ = refunded != 0u;
        context.ResetsMultipleQ = BitCount4(refunded) >= 2;
        context.FollowupAvailable = Ready(0) || refunded != 0u;
        context.TargetInAttackRange = InAutoAttackRange(target, 30.0f);
        context.HitCount = static_cast<int>(hits.size());
        if (!ShouldCastW(context)) continue;
        float score = static_cast<float>(hits.size()) * 145.0f +
            static_cast<float>(BitCount4(refunded)) * 260.0f +
            (reliable ? 160.0f : 0.0f) +
            (context.Lethal ? 1000.0f : 0.0f) +
            (interrupt ? 1200.0f : 0.0f) +
            (gapcloser || peel ? 850.0f : 0.0f);
        if (score > bestScore) {
            bestScore = score;
            best.Aim = end;
            best.ResetMask = resetMask;
            best.TargetId = static_cast<int>(target.NetworkId());
            best.HitCount = static_cast<int>(hits.size());
            best.Context = context;
            best.Valid = true;
        }
    }
    return best;
}

inline WPlan BuildFarmWPlan(bool jungle) {
    WPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1) || EActive ||
        Engine::CountEnemiesAt(player.Position(), 1100.0f) > 0) return best;
    std::vector<Body> units;
    const auto append = [&](const AIMinionClient& unit, bool monster) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(unit.Position()) >
                kWGameplayRange + unit.BoundingRadius() + 80.0f) return;
        Body body{};
        body.Position = PredictPosition(unit, kWCastSeconds);
        body.Radius = unit.BoundingRadius();
        body.Id = static_cast<int>(unit.NetworkId());
        body.Monster = monster;
        body.Epic = monster && IsEpicMonster(unit);
        body.Health = unit.Health();
        body.MaximumHealth = unit.MaxHealth();
        units.push_back(body);
    };
    if (jungle) {
        for (const auto& unit : GameObjects::Jungle()) append(unit, true);
    } else {
        for (const auto& unit : GameObjects::EnemyMinions()) append(unit, false);
    }
    int bestHits = 0;
    for (const Body& candidate : units) {
        const Vector3 direction = SharedGeometry::Direction2D(
            player.Position(), candidate.Position);
        if (direction.IsZero()) continue;
        const Vector3 end = player.Position() +
            direction * kWGameplayRange;
        int hits = 0;
        for (const Body& unit : units) {
            if (WLineHits(player.Position(), end, unit)) ++hits;
        }
        WContext context{};
        context.Ready = context.TargetValid =
            context.PredictionHits = context.HighHitchance = true;
        context.PlayerWindingUp = Orbwalker::IsWindingUp();
        context.PlayerInE = EActive;
        context.Farm = true;
        context.Jungle = jungle;
        context.FarmHits = hits;
        context.EnemyChampionNearby = false;
        if (!ShouldCastW(context) || hits <= bestHits) continue;
        bestHits = hits;
        best.Aim = end;
        best.TargetId = candidate.Id;
        best.HitCount = hits;
        best.Context = context;
        best.Valid = true;
    }
    return best;
}

inline bool CastWPlan(const WPlan& plan, bool reactive = false) {
    if (!plan.Valid || !CastThrottleReady(1, 34, reactive ? 0 : -1)) {
        return false;
    }
    if (!Engine::ControllerCastPosition(1, plan.Aim)) return false;
    LastWPlan = plan;
    LastWCastTick = Now();
    LastWTargetId = plan.TargetId;
    LastWOrigin = GameObjects::Player().Position();
    LastWAim = plan.Aim;
    if (plan.Context.Interrupt || plan.Context.Gapcloser ||
        plan.Context.Peel) {
        ActiveSequence = Sequence::WLockedE;
    } else if (plan.Context.ResetsSpentQ) {
        ActiveSequence = Sequence::QWQChase;
    }
    SequenceExpireTick = Now() + kWLockFollowupMs;
    return true;
}

inline std::vector<Body> ETargetBodies() {
    std::vector<Body> result;
    result.reserve(72);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        AppendBody(result, enemy);
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        AppendBody(result, minion);
    }
    for (const auto& monster : GameObjects::Jungle()) {
        AppendBody(result, monster);
    }
    return result;
}

inline int ForcedETargetId() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    const auto bodies = ETargetBodies();
    const int index = SelectETargetIndex(player.Position(), bodies);
    return index >= 0 ? bodies[static_cast<std::size_t>(index)].Id : 0;
}

inline bool QDirectionReturnsDuringE() {
    const int future = Now() + static_cast<int>(kEDurationSeconds * 1000.0f);
    for (int tick : QDirections.ReadyTick) {
        if (tick <= future) return true;
    }
    return false;
}

inline float EPhysicalDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const bool monster = AIMinionClient(target.Address()).IsJungle();
    const float raw = ESimulatedRawDamage(
        SpellRank(2), player.TotalAttackDamage(), target.Health(),
        target.MaxHealth(), BonusAttackSpeedPercent(), 0.0f, monster);
    return player.CalculatePhysicalDamage(target, raw);
}

inline EPlan BuildEPlan(const AIBaseClient& desired,
                        bool defensive = false,
                        bool jungleSustain = false,
                        bool objectiveSecure = false) {
    EPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2) || EActive) return plan;
    const int forcedId = ForcedETargetId();
    const AIBaseClient forced = UnitByNetworkId(forcedId);
    const bool forcedValid = forced.IsValid() && !forced.IsDead() &&
        forced.IsTargetable() && player.Position().Distance2D(
            forced.Position()) <= kERadius + forced.BoundingRadius();
    const bool desiredValid = desired.IsValid() && !desired.IsDead();
    const int desiredId = desiredValid
        ? static_cast<int>(desired.NetworkId()) : 0;
    const AIHeroClient desiredHero = desiredValid && desired.IsHero()
        ? AIHeroClient(desired.Address()) : AIHeroClient{};
    const bool forcedMonster = forcedValid &&
        AIMinionClient(forced.Address()).IsJungle();
    const bool forcedChampion = forcedValid && forced.IsHero();
    const float expected = forcedValid ? EPhysicalDamage(forced) : 0.0f;
    const int enemies = Engine::CountEnemiesAt(player.Position(), 650.0f);
    const int allies = Engine::CountAlliesAt(player.Position(), 750.0f);
    EStartContext context{};
    context.Ready = true;
    context.CanDeclareAttacks =
        !SDK::HasBuffOfType(player, SDK::BuffType::Blind) &&
        !SDK::HasBuffOfType(player, SDK::BuffType::Disarm);
    context.PlayerWindingUp = Orbwalker::IsWindingUp();
    context.ForcedTargetValid = forcedValid;
    context.ForcedTargetIsDesired = forcedValid && forcedId == desiredId;
    context.ForcedTargetChampion = forcedChampion;
    context.ForcedTargetMonster = forcedMonster;
    context.ForcedTargetEpic = forcedMonster && IsEpicMonster(forced);
    context.TargetHardCrowdControlled = desiredHero.IsValid() &&
        Engine::IsHardCrowdControlled(desiredHero);
    context.TargetSlowed = desiredHero.IsValid() &&
        SDK::HasBuffOfType(desiredHero, SDK::BuffType::Slow);
    context.TargetCommitted = desiredHero.IsValid() && EnemyCommitted(
        static_cast<int>(desiredHero.NetworkId()));
    context.TargetEscaping = desiredValid && TargetEscaping(desired);
    context.PositionSafe = !Engine::UnderEnemyTurret(player.Position()) &&
        enemies <= allies + 1 &&
        !HasReadyPointClickThreatAt(player.Position());
    context.Execute = forcedValid && forcedId == desiredId &&
        (expected + 1.0f >= forced.Health() ||
         forced.HealthPercent() <= Slider(EMenu, "ExecuteHp", 34));
    context.Defensive = defensive;
    context.IncomingReducibleBurst = IncomingThreatUntil >= Now() &&
        IncomingDamage >= std::max(75.0f, player.Health() * 0.20f);
    context.IncomingTrueDamageOnly = IncomingTrueDamageOnly;
    context.JungleSustain = jungleSustain;
    context.ObjectiveSecure = objectiveSecure;
    context.RecentAbilityCast =
        Now() - std::max(LastQCastTick, LastWCastTick) <= 1550 ||
        LastWHitTick + kWLockFollowupMs >= Now();
    context.QEscapeAvailableAfter = QDirectionReturnsDuringE();
    context.EnemiesNearby = enemies;
    context.AlliesNearby = allies;
    context.PlayerHealthPercent = player.HealthPercent();
    context.ForcedTargetHealthPercent = forcedValid
        ? forced.HealthPercent() : 100.0f;
    context.DesiredTargetHealthPercent = desiredValid
        ? desired.HealthPercent() : 100.0f;
    context.ExpectedDamage = expected;
    context.ForcedTargetHealth = forcedValid ? forced.Health() : FLT_MAX;
    if (!ShouldStartE(context)) return plan;
    plan.Context = context;
    plan.DesiredTargetId = desiredId;
    plan.ForcedTargetId = forcedId;
    plan.ExpectedDamage = expected;
    plan.Defensive = defensive;
    plan.Valid = true;
    return plan;
}

inline bool CastEPlan(const EPlan& plan, bool reactive = false) {
    if (!plan.Valid || !CastThrottleReady(2, 30, reactive ? 0 : -1)) {
        return false;
    }
    if (!Engine::ControllerCastSelf(2)) return false;
    LastEPlan = plan;
    LastECastTick = EStartTick = Now();
    EEndTick = EStartTick + static_cast<int>(kEDurationSeconds * 1000.0f);
    EDesiredTargetId = plan.DesiredTargetId;
    EForcedTargetId = plan.ForcedTargetId;
    EActive = true;
    ActiveSequence = plan.Defensive
        ? Sequence::DefensiveE : Sequence::WLockedE;
    SequenceExpireTick = EEndTick + 250;
    return true;
}

inline bool HandleEChannel(const AIHeroClient& desired) {
    if (!EActive) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int now = Now();
    if (now >= EEndTick + 120 || !player.HasBuff("BelvethE") &&
        now - EStartTick > 250 && now > EEndTick) {
        EActive = false;
        return false;
    }
    EForcedTargetId = ForcedETargetId();
    const AIBaseClient forced = UnitByNetworkId(EForcedTargetId);
    AIHeroClient wanted = desired;
    if (!wanted.IsValid()) wanted = HeroByNetworkId(EDesiredTargetId);
    const bool wantedValid = Engine::ValidEnemy(wanted);
    const bool forcedValid = forced.IsValid() && !forced.IsDead() &&
        forced.IsTargetable() && player.Position().Distance2D(
            forced.Position()) <= kERadius + forced.BoundingRadius();
    const bool forcedDesired = wantedValid && forcedValid &&
        EForcedTargetId == static_cast<int>(wanted.NetworkId());
    const bool escaped = wantedValid && player.Position().Distance2D(
        wanted.Position()) > kERadius + wanted.BoundingRadius() + 45.0f;
    const QPlan evade = IncomingThreatUntil >= now
        ? BuildQPlan(AIBaseClient{}, Mode::Flee, QPurpose::Evade)
        : QPlan{};
    const QPlan chase = escaped && wantedValid
        ? BuildQPlan(wanted, Mode::Combo, QPurpose::Chase)
        : QPlan{};
    ECancelContext context{};
    context.Active = true;
    context.ElapsedSeconds = static_cast<float>(now - EStartTick) / 1000.0f;
    context.IncomingLethalSkillshot = IncomingThreatUntil >= now &&
        PointInsideIncomingLine(player.Position()) &&
        IncomingDamage >= player.Health() * 0.78f;
    context.SafeQEvadeAvailable = evade.Valid;
    context.ForcedTargetValid = forcedValid;
    context.ForcedTargetIsDesired = forcedDesired;
    context.DesiredTargetEscaped = escaped;
    context.SafeQChaseAvailable = chase.Valid;
    context.PlayerExplicitlyRetreating = wantedValid &&
        CursorRetreatsFrom(wanted);
    context.DefensiveWindowFinished = IncomingThreatUntil < now;
    if (!ShouldCancelE(context)) return false;
    if (context.IncomingLethalSkillshot && CastQPlan(evade, true)) {
        EActive = false;
        ActiveSequence = Sequence::ECancelDodge;
        return true;
    }
    if (context.DesiredTargetEscaped && CastQPlan(chase, true)) {
        EActive = false;
        ActiveSequence = Sequence::QWQChase;
        return true;
    }
    // A raw E recast throws away damage and DR. Only honor a clear cursor-side
    // retreat after the defensive window has actually ended.
    if (context.PlayerExplicitlyRetreating &&
        context.DefensiveWindowFinished &&
        CastThrottleReady(2, 30, 0) && Engine::ControllerCastSelf(2)) {
        EActive = false;
        return true;
    }
    return false;
}

inline bool IsCoralObject(const SDK::Events::ObjectEventArgs& args) {
    return ControllerHelpers::AnyTextContains(
        { args.Sender.Name, args.Sender.CharacterName,
          args.SpellName, args.MissileName },
        { "belvethspore", "belvethcoral", "lavendercoral" });
}

inline bool IsCoralUnit(const AIMinionClient& unit) {
    return unit.IsValid() && ControllerHelpers::AnyTextContains(
        { unit.Name().c_str(), unit.CharacterName().c_str() },
        { "belvethspore", "belvethcoral", "lavendercoral" });
}

inline bool IsEnhancedSourceObject(
    const SDK::Events::ObjectEventArgs& args) {
    return ControllerHelpers::AnyTextContains(
        { args.Sender.Name, args.Sender.CharacterName,
          args.SpellName },
        { "sru_baron", "baron", "sru_riftherald", "riftherald",
          "sru_voidgrub", "voidgrub" });
}

inline CoralRecord* FindCoral(int networkId, bool create = false) {
    if (networkId == 0) return nullptr;
    for (auto& coral : Corals) {
        if (coral.NetworkId == networkId) return &coral;
    }
    if (!create) return nullptr;
    for (auto& coral : Corals) {
        if (coral.NetworkId == 0 || coral.ExpireTick < Now()) {
            coral = {};
            coral.NetworkId = networkId;
            return &coral;
        }
    }
    auto* oldest = &*std::min_element(
        Corals.begin(), Corals.end(),
        [](const CoralRecord& left, const CoralRecord& right) {
            return left.ExpireTick < right.ExpireTick;
        });
    *oldest = {};
    oldest->NetworkId = networkId;
    return oldest;
}

inline void ObserveCoral(int networkId,
                         const Vector3& position,
                         bool confirmedAllied) {
    if (networkId == 0 || !position.IsValid() || position.IsZero()) return;
    CoralRecord* coral = FindCoral(networkId, true);
    if (!coral) return;
    const int now = Now();
    const bool newRecord = coral->SpawnTick <= 0;
    coral->NetworkId = networkId;
    coral->Position = position;
    coral->LastSeenTick = now;
    coral->ConfirmedAllied = coral->ConfirmedAllied || confirmedAllied;
    if (newRecord) {
        coral->SpawnTick = now;
        coral->ExpireTick = now + static_cast<int>(
            kRCoralSeconds * 1000.0f);
        coral->Enhanced = RecentEnhancedSourceUntil >= now &&
            RecentEnhancedSourcePosition.IsValid() &&
            position.Distance2D(RecentEnhancedSourcePosition) <= 1150.0f;
    }
}

inline void ScanCorals() {
    const int now = Now();
    for (auto& coral : Corals) {
        if (coral.NetworkId != 0 && coral.ExpireTick + 250 < now) coral = {};
    }
    for (const auto& unit : GameObjects::AllyMinions()) {
        if (!IsCoralUnit(unit)) continue;
        ObserveCoral(
            static_cast<int>(unit.NetworkId()), unit.Position(), true);
    }
}

inline int ActiveCoralCount() {
    int count = 0;
    for (const auto& coral : Corals) {
        if (coral.NetworkId != 0 && coral.ExpireTick >= Now() &&
            coral.ConfirmedAllied) ++count;
    }
    return count;
}

inline float RCastRange() {
    static constexpr std::array<float, 4> range = {
        0.0f, 275.0f, 375.0f, 450.0f,
    };
    return SharedGeometry::RankValue(range, SpellRank(3));
}

inline bool ObjectiveNear(const Vector3& position, float radius) {
    for (const auto& monster : GameObjects::Jungle()) {
        if (!monster.IsValid() || monster.IsDead() ||
            !IsEpicMonster(monster)) continue;
        if (position.Distance2D(monster.Position()) <=
            radius + monster.BoundingRadius()) return true;
    }
    return false;
}

inline int EnemyLaneMinionsAt(const Vector3& position, float radius) {
    int count = 0;
    for (const auto& minion : GameObjects::EnemyLaneMinions()) {
        if (minion.IsValid() && !minion.IsDead() &&
            position.Distance2D(minion.Position()) <=
                radius + minion.BoundingRadius()) ++count;
    }
    return count;
}

inline RPlan BuildRPlan(bool manual = false) {
    RPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3) || RChannelActive) return best;
    ScanCorals();
    const int now = Now();
    int consumed = 0;
    bool anyEnhanced = false;
    float soonestExpiry = kRCoralSeconds;
    for (const auto& coral : Corals) {
        if (coral.NetworkId == 0 || coral.ExpireTick < now ||
            !coral.ConfirmedAllied) continue;
        ++consumed;
        anyEnhanced = anyEnhanced || coral.Enhanced;
        soonestExpiry = std::min(
            soonestExpiry,
            static_cast<float>(coral.ExpireTick - now) / 1000.0f);
    }
    if (consumed == 0) return best;

    const float range = RCastRange();
    const float heal = RHeal(
        SpellRank(3), player.BonusAttackDamage(), player.AP());
    float bestScore = -FLT_MAX;
    for (const auto& coral : Corals) {
        if (coral.NetworkId == 0 || coral.ExpireTick < now ||
            !coral.ConfirmedAllied || !coral.Position.IsValid() ||
            player.Position().Distance2D(coral.Position) >
                range + 85.0f) continue;
        const AIBaseClient coralUnit = UnitByNetworkId(coral.NetworkId);
        if (!coralUnit.IsValid()) continue;

        RContext context{};
        context.Ready = true;
        context.PlayerGrounded = PlayerMobilityLocked();
        context.UnderEnemyTurret = Engine::UnderEnemyTurret(coral.Position);
        context.CursorAgrees = CursorDirectionAgrees(coral.Position, -0.20f);
        context.AnyEnhancedCoral = anyEnhanced;
        context.CurrentlyTrueForm = HasTrueForm();
        context.CurrentlyEnhanced = HasEnhancedForm();
        context.ObjectiveWindow = ObjectiveNear(coral.Position, 1250.0f);
        context.WaveMacroWindow = anyEnhanced &&
            EnemyLaneMinionsAt(coral.Position, 1000.0f) >= 4 &&
            Engine::CountEnemiesAt(coral.Position, 1100.0f) == 0;
        context.PlayerLow = player.HealthPercent() <=
            Slider(RMenu, "HealHp", 36);
        context.IncomingLethalDamage = IncomingThreatUntil >= now &&
            IncomingDamage >= player.Health() * 0.72f;
        context.PlayerWindingUp = Orbwalker::IsWindingUp();
        context.PlayerExplicitlyRequested = manual;
        context.CurrentFormSeconds = TrueFormSecondsRemaining();
        context.PlayerMissingHealth = std::max(
            0.0f, player.MaxHealth() - player.Health());
        context.Heal = heal;
        context.EnemiesAtCoral = Engine::CountEnemiesAt(
            coral.Position, kRRadius + 180.0f);
        context.AlliesAtCoral = Engine::CountAlliesAt(
            coral.Position, kRRadius + 260.0f);
        context.CoralsConsumed = consumed;
        context.SoonestCoralExpiry = soonestExpiry;

        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!Engine::ValidEnemy(enemy, 2400.0f)) continue;
            const Vector3 predicted = PredictPosition(
                enemy, kRChannelSeconds + 0.12f);
            if (predicted.Distance2D(coral.Position) >
                kRRadius + enemy.BoundingRadius()) continue;
            const float damage = RExplosionDamage(enemy);
            ++context.HitCount;
            context.ExplosionDamage += std::min(damage, enemy.Health());
            if (damage + 2.0f >= enemy.Health()) ++context.KillCount;
        }

        const bool numericalSafety = context.EnemiesAtCoral <=
            context.AlliesAtCoral + (context.KillCount > 0 ? 1 : 0);
        context.DestinationSafe = numericalSafety &&
            !HasReadyDashHazardAt(coral.Position) &&
            (!HasReadyPointClickThreatAt(coral.Position) ||
             context.KillCount > 0 || context.IncomingLethalDamage);
        REvaluation evaluation = EvaluateR(context);
        if (!evaluation.Cast) continue;
        float score = evaluation.Score;
        if (coral.Enhanced) score += 250.0f;
        if (manual) {
            score -= 0.20f * coral.Position.Distance2D(Game::CursorPos());
        }
        if (score <= bestScore) continue;
        bestScore = score;
        best.CoralId = coral.NetworkId;
        best.Position = coral.Position;
        best.Context = context;
        best.Evaluation = evaluation;
        best.Evaluation.Score = score;
        if (manual) best.Reason = RReason::Manual;
        else if (anyEnhanced) best.Reason = RReason::EnhancedMacro;
        else if (context.KillCount > 0) best.Reason = RReason::Execute;
        else if (context.IncomingLethalDamage ||
                 context.PlayerLow) best.Reason = RReason::HealSave;
        else if (soonestExpiry <= 2.25f) best.Reason = RReason::Expiring;
        else best.Reason = RReason::FirstForm;
        best.Valid = true;
    }
    return best;
}

inline bool CastRPlan(const RPlan& plan, bool reactive = false) {
    if (!plan.Valid || !CastThrottleReady(3, 34, reactive ? 0 : -1)) {
        return false;
    }
    const AIBaseClient coral = UnitByNetworkId(plan.CoralId);
    if (!coral.IsValid() || !Engine::ControllerCastUnit(3, coral)) {
        return false;
    }
    LastRPlan = plan;
    LastRReason = plan.Reason;
    LastRCastTick = Now();
    RChannelActive = true;
    RChannelUntil = LastRCastTick + static_cast<int>(
        (kRChannelSeconds + kRPostLockSeconds) * 1000.0f);
    const float formDuration = RFormDurationSeconds(LavenderStacks);
    TrueFormActive = true;
    EnhancedFormActive = plan.Context.AnyEnhancedCoral ||
        plan.Context.CurrentlyEnhanced;
    TrueFormExpireTick = std::isinf(formDuration)
        ? std::numeric_limits<int>::max()
        : LastRCastTick + static_cast<int>(formDuration * 1000.0f);
    switch (plan.Reason) {
    case RReason::Execute:
        ActiveSequence = Sequence::CoralExecute;
        break;
    case RReason::HealSave:
        ActiveSequence = Sequence::CoralSave;
        break;
    case RReason::EnhancedMacro:
        ActiveSequence = Sequence::EnhancedMacro;
        break;
    default:
        ActiveSequence = Sequence::PlayerLed;
        break;
    }
    SequenceExpireTick = RChannelUntil + 500;
    // R consumes every existing coral, not only the selected unit.
    for (auto& coralRecord : Corals) coralRecord = {};
    return true;
}

inline AIHeroClient PreferredEnemy(const AIHeroClient& selected,
                                   float range = 1800.0f) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    return NearestEnemyToPlayer({}, range);
}

inline bool WallBetweenPlayerAnd(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return false;
    const Vector3 direction = SharedGeometry::Direction2D(
        player.Position(), target.Position());
    if (direction.IsZero()) return false;
    const float distance = std::min(
        kQTrueFormWallDistance,
        player.Position().Distance2D(target.Position()));
    bool wall = false;
    for (float travel = 24.0f; travel <= distance; travel += 18.0f) {
        const bool sampleWall = SDK::NavMesh::IsWall(
            player.Position() + direction * travel);
        if (sampleWall) wall = true;
        else if (wall) return true;
    }
    return false;
}

inline bool TeamfightReadyToEnter(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (EnemyCommitted(static_cast<int>(target.NetworkId())) ||
        Engine::IsHardCrowdControlled(target) || target.IsDashing()) {
        return true;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1250.0f)) continue;
        if (EnemyCommitted(static_cast<int>(enemy.NetworkId())) ||
            Engine::IsHardCrowdControlled(enemy)) return true;
    }
    return Engine::CountAlliesAt(target.Position(), 625.0f) >= 1 &&
        target.HealthPercent() <= 62.0f;
}

inline Posture DeterminePosture(Mode mode, const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return Posture::Neutral;
    if (mode == Mode::Flee) return Posture::Flee;
    if (mode == Mode::Jungle &&
        ControllerHelpers::HasNearbyEpicMonster(1400.0f)) {
        return Posture::Objective;
    }
    if (mode == Mode::LaneClear || mode == Mode::LastHit ||
        mode == Mode::Jungle) return Posture::Clear;
    if (!Engine::ValidEnemy(target)) return Posture::Neutral;
    const int nearbyEnemies = Engine::CountEnemiesAt(
        player.Position(), 1050.0f);
    if (player.HealthPercent() <= Slider(TacticsMenu, "KiteHp", 36) &&
        nearbyEnemies > 0) return Posture::Kite;
    if (target.HealthPercent() <= 32.0f) return Posture::Execute;
    if (nearbyEnemies >= 2 &&
        !TeamfightReadyToEnter(target)) return Posture::SecondEntry;
    if (TargetEscaping(target)) return Posture::Chase;
    if (mode == Mode::Harass) return Posture::ShortTrade;
    return Posture::Duel;
}

inline AIBaseClient BestFarmUnit(bool jungle) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    AIBaseClient best{};
    float bestScore = -FLT_MAX;
    const auto consider = [&](const AIMinionClient& unit, bool monster) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(unit.Position()) > 875.0f) return;
        float score = -unit.HealthPercent() * 220.0f -
            player.Position().Distance2D(unit.Position()) * 0.05f;
        if (monster && IsEpicMonster(unit)) score += 1200.0f;
        if (LastAutoTargetId == static_cast<int>(unit.NetworkId()) &&
            Now() - LastAutoTick <= kWeaveWindowMs) score += 620.0f;
        if (QPhysicalDamage(unit) + 1.0f >= unit.Health()) score += 480.0f;
        if (score > bestScore) {
            bestScore = score;
            best = unit;
        }
    };
    if (jungle) {
        for (const auto& unit : GameObjects::Jungle()) consider(unit, true);
    } else {
        for (const auto& unit : GameObjects::EnemyMinions()) {
            consider(unit, false);
        }
    }
    return best;
}

inline bool TryManualCoral() {
    if (!Key(RMenu, "ManualCoral", false)) return false;
    return CastRPlan(BuildRPlan(true), true);
}

inline bool TryReactive(const AIHeroClient& selected) {
    AIHeroClient target = PreferredEnemy(selected, 1250.0f);
    if (EActive) {
        const bool cast = HandleEChannel(target);
        return cast || EActive;
    }
    if (InterruptExpireTick >= Now()) {
        const AIHeroClient interrupt = HeroByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(interrupt, kWGameplayRange + 120.0f) &&
            CastWPlan(BuildWPlan(interrupt, true, false, false), true)) {
            return true;
        }
    }
    if (GapcloserExpireTick >= Now()) {
        const AIHeroClient gap = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(gap, kWGameplayRange + 180.0f) &&
            CastWPlan(BuildWPlan(gap, false, true, true), true)) {
            return true;
        }
    }
    if (IncomingThreatUntil >= Now() &&
        PointInsideIncomingLine(GameObjects::Player().Position()) &&
        Bool(QMenu, "EvadeSkillshots", true)) {
        const QPlan evade = BuildQPlan(
            AIBaseClient{}, Mode::Flee, QPurpose::Evade);
        if (CastQPlan(evade, true)) return true;
    }
    if (IncomingThreatUntil >= Now() &&
        Bool(EMenu, "Defensive", true)) {
        if (CastEPlan(BuildEPlan(target, true), true)) return true;
    }
    if (IncomingThreatUntil >= Now() &&
        Bool(RMenu, "Survival", true)) {
        if (CastRPlan(BuildRPlan(false), true)) return true;
    }
    return false;
}

inline bool TryKillSecure(const AIHeroClient& target) {
    if (!Bool(TacticsMenu, "KillSecure", true) ||
        !Engine::ValidEnemy(target, 1200.0f)) return false;
    if (Ready(1) && WMagicalDamage(target) + 2.0f >= target.Health()) {
        const WPlan w = BuildWPlan(target);
        if (CastWPlan(w, true)) return true;
    }
    if (Ready(0) && QPhysicalDamage(target) + 2.0f >= target.Health()) {
        const QPlan q = BuildQPlan(target, Mode::Combo, QPurpose::Execute);
        if (CastQPlan(q, true)) return true;
    }
    if (Ready(2) && GameObjects::Player().Position().Distance2D(
            target.Position()) <= kERadius + target.BoundingRadius()) {
        const EPlan e = BuildEPlan(target);
        if (e.Valid && e.Context.Execute && CastEPlan(e, true)) return true;
    }
    if (Ready(3) && ActiveCoralCount() > 0) {
        const RPlan r = BuildRPlan(false);
        if (r.Valid && r.Context.KillCount > 0 && CastRPlan(r, true)) {
            return true;
        }
    }
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, 1800.0f)) {
        return Bool(RMenu, "Automatic", true) &&
            CastRPlan(BuildRPlan(false));
    }
    if (CurrentPosture == Posture::SecondEntry &&
        !TeamfightReadyToEnter(target)) {
        // Bel'Veth follows the first commitment; she does not spend both
        // forward arrows to become the team's initial engage.
        return false;
    }
    if (Bool(RMenu, "Automatic", true)) {
        const RPlan r = BuildRPlan(false);
        if (r.Valid && (r.Context.KillCount > 0 ||
            r.Context.IncomingLethalDamage ||
            r.Context.AnyEnhancedCoral ||
            !r.Context.CurrentlyTrueForm) && CastRPlan(r)) return true;
    }

    const bool attackJustCompleted = LastAutoTick > 0 &&
        Now() - LastAutoTick <= kWeaveWindowMs &&
        LastAutoTargetId == static_cast<int>(target.NetworkId());
    if (attackJustCompleted && Bool(QMenu, "AutoReset", true)) {
        const QPlan q = BuildQPlan(target, Mode::Combo, QPurpose::Weave);
        if (CastQPlan(q)) return true;
    }

    const bool recentlyLocked = LastWHitTick > 0 &&
        Now() - LastWHitTick <= kWLockFollowupMs;
    if (Ready(2) && Bool(EMenu, "Combo", true) &&
        (recentlyLocked || target.HealthPercent() <= 44.0f)) {
        const EPlan e = BuildEPlan(target);
        if (CastEPlan(e)) return true;
    }

    // Preserve the A in AA-Q-AA. A generic dash-first loop loses both the Q
    // reset and an available every-application R passive ramp.
    if (InAutoAttackRange(target, 25.0f) && Orbwalker::CanAttack() &&
        !Engine::IsHardCrowdControlled(target)) return false;

    if (Ready(1) && Bool(WMenu, "Combo", true)) {
        const WPlan w = BuildWPlan(target);
        if (CastWPlan(w)) return true;
    }

    if (Ready(0) && Bool(QMenu, "Combo", true)) {
        if (HasTrueForm() && Bool(QMenu, "WallFlanks", true) &&
            WallBetweenPlayerAnd(target)) {
            const QPlan flank = BuildQPlan(
                target, Mode::Combo, QPurpose::WallFlank);
            if (CastQPlan(flank)) return true;
        }
        const QPlan q = BuildQPlan(target, Mode::Combo);
        if (CastQPlan(q)) return true;
    }

    if (Ready(2) && Bool(EMenu, "Combo", true)) {
        const EPlan e = BuildEPlan(target);
        if (CastEPlan(e)) return true;
    }
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target, 1000.0f)) return false;
    const bool afterAttack = LastAutoTick > 0 &&
        Now() - LastAutoTick <= kWeaveWindowMs &&
        LastAutoTargetId == static_cast<int>(target.NetworkId());
    if (afterAttack && Bool(QMenu, "Harass", true)) {
        if (CastQPlan(BuildQPlan(
                target, Mode::Harass, QPurpose::Weave))) return true;
    }
    if (InAutoAttackRange(target, 25.0f) && Orbwalker::CanAttack()) {
        return false;
    }
    if (Ready(1) && Bool(WMenu, "Harass", true)) {
        const WPlan w = BuildWPlan(target);
        if (w.Valid && w.Context.ResetsSpentQ && CastWPlan(w)) return true;
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& selected) {
    const AIHeroClient threat = PreferredEnemy(selected, 1050.0f);
    if (EActive) {
        const bool cast = HandleEChannel(threat);
        return cast || EActive;
    }
    if (IncomingThreatUntil >= Now() && Ready(0)) {
        if (CastQPlan(BuildQPlan(
                AIBaseClient{}, Mode::Flee, QPurpose::Evade), true)) {
            return true;
        }
    }
    if (Engine::ValidEnemy(threat, kWGameplayRange + 150.0f) &&
        Bool(WMenu, "Flee", true)) {
        if (CastWPlan(BuildWPlan(threat, false, false, true), true)) {
            return true;
        }
    }
    if (Ready(0) && Bool(QMenu, "Flee", true)) {
        if (CastQPlan(BuildQPlan(
                threat, Mode::Flee, QPurpose::Flee), true)) return true;
    }
    if (Ready(2) && Bool(EMenu, "Defensive", true) &&
        Engine::ValidEnemy(threat)) {
        if (CastEPlan(BuildEPlan(threat, true), true)) return true;
    }
    return false;
}

inline bool TryFarm(Mode mode) {
    const bool jungle = mode == Mode::Jungle;
    const AIBaseClient target = BestFarmUnit(jungle);
    if (!target.IsValid()) return false;
    if (EActive) {
        const bool cast = HandleEChannel({});
        return cast || EActive;
    }
    const bool qEnabled = Bool(
        FarmMenu, jungle ? "JungleQ" : "LaneQ", true);
    const bool afterAttack = LastAutoTick > 0 &&
        Now() - LastAutoTick <= kWeaveWindowMs &&
        LastAutoTargetId == static_cast<int>(target.NetworkId());
    const bool lastHit = QPhysicalDamage(target) + 1.0f >= target.Health();
    const bool qEconomy = ReadyDirectionCount() >
        Slider(FarmMenu, "ReserveQ", 1) || lastHit;
    if (qEnabled && afterAttack && qEconomy) {
        if (CastQPlan(BuildQPlan(target, mode))) return true;
    }
    if (jungle && Bool(EMenu, "Jungle", true)) {
        const EPlan e = BuildEPlan(
            target, false, true, IsEpicMonster(target));
        if (CastEPlan(e)) return true;
    }
    if (Bool(FarmMenu, jungle ? "JungleW" : "LaneW", jungle)) {
        const WPlan w = BuildFarmWPlan(jungle);
        if (CastWPlan(w)) return true;
    }
    if (!qEnabled || !qEconomy || !lastHit) return false;
    return CastQPlan(BuildQPlan(target, mode));
}

inline void RefreshRuntimeState() {
    const int now = Now();
    SyncQHud();
    const auto player = GameObjects::Player();
    if (player.IsValid()) {
        LavenderStacks = std::max(
            LavenderStacks,
            ControllerHelpers::MaximumBuffCount(
                player, { "BelvethPassiveStacks", "belvethpassivestacks" }));
        if (player.HasBuff("BelvethPassiveSheen") &&
            !PassiveSheenActive) {
            PassiveSheenActive = true;
            PassiveSheenExpireTick = now + static_cast<int>(
                kPassiveSheenDurationSeconds * 1000.0f);
        }
        if (player.HasBuff("BelvethRSteroid")) {
            TrueFormActive = true;
            if (TrueFormExpireTick <= now) SetFormExpiryFromStacks(now);
        }
    }
    ScanCorals();
    if (GapcloserExpireTick < now) {
        GapcloserTargetId = 0;
        GapcloserEnd = {};
    }
    if (InterruptExpireTick < now) InterruptTargetId = 0;
    if (IncomingThreatUntil < now) {
        IncomingDamage = 0.0f;
        IncomingTrueDamageOnly = false;
        IncomingHardCrowdControl = false;
        IncomingLineStart = IncomingLineEnd = {};
        IncomingLineWidth = 0.0f;
    }
    if (PassiveSheenExpireTick < now) PassiveSheenActive = false;
    if (RChannelUntil < now) RChannelActive = false;
    if (TrueFormExpireTick < now) {
        TrueFormActive = false;
        EnhancedFormActive = false;
    }
    if (SequenceExpireTick < now) {
        ActiveSequence = Sequence::None;
    }
    if (EActive && now > EEndTick + 180) EActive = false;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshRuntimeState();
    const AIHeroClient target = PreferredEnemy(selected, 1800.0f);
    LastDecisionTargetId = target.IsValid()
        ? static_cast<int>(target.NetworkId()) : 0;
    CurrentPosture = DeterminePosture(mode, target);
    if (TryManualCoral()) return true;
    if (PlayerOverrideUntil >= Now()) return false;
    if (TryReactive(target)) return true;
    if (TryKillSecure(target)) return true;
    if (mode == Mode::Flee) return TryFlee(target);
    if (mode == Mode::Combo) return TryCombo(target);
    if (mode == Mode::Harass) return TryHarass(target);
    if (mode == Mode::LaneClear || mode == Mode::LastHit ||
        mode == Mode::Jungle) return TryFarm(mode);
    if ((mode == Mode::None || mode == Mode::Automatic) &&
        Bool(RMenu, "Automatic", true)) {
        return CastRPlan(BuildRPlan(false));
    }
    return false;
}

inline bool IsQEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::Q, {
            "belvethq", "voidsurge",
        });
}

inline bool IsWEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::W, {
            "belvethw", "aboveandbelow",
        });
}

inline bool IsEEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::E, {
            "belvethe", "royalmaelstrom",
        });
}

inline bool IsREvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return ControllerHelpers::SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::R, {
            "belvethr", "endlessbanquet", "belvethconsumecoral",
        });
}

inline int EventTargetId(
    const SDK::Events::ProcessSpellEventArgs& args) {
    return static_cast<int>(args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId);
}

inline int LocalSpellSlot(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsQEvent(args)) return 0;
    if (IsWEvent(args)) return 1;
    if (IsEEvent(args)) return 2;
    if (IsREvent(args)) return 3;
    return args.Slot >= 0 && args.Slot < 4 ? args.Slot : -1;
}

inline Vector3 EventCastPosition(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (args.EndPosition.IsValid() && !args.EndPosition.IsZero()) {
        return args.EndPosition;
    }
    if (args.CastPosition.IsValid() && !args.CastPosition.IsZero()) {
        return args.CastPosition;
    }
    return args.StartPosition;
}

inline void ActivatePassiveSheen() {
    PassiveSheenActive = true;
    PassiveSheenExpireTick = Now() + static_cast<int>(
        kPassiveSheenDurationSeconds * 1000.0f);
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const int now = Now();
    const int slot = LocalSpellSlot(args);
    if (slot < 0 || slot >= 4) return;
    const bool controllerOwned = Engine::WasControllerCast(slot);
    const bool eRecast = slot == 2 && EStartTick > 0 &&
        now - EStartTick >=
            static_cast<int>(kEEarlyCancelSeconds * 1000.0f) - 80 &&
        now <= EEndTick + 220;
    LastLocalSpellTick = now;
    if (!eRecast) ActivatePassiveSheen();

    if (slot == 0) {
        const auto player = GameObjects::Player();
        Vector3 origin = args.StartPosition;
        if (!origin.IsValid() || origin.IsZero()) origin = player.Position();
        const Quadrant direction = QuadrantForPoints(
            origin, EventCastPosition(args));
        if (QuadrantIndex(direction) >= 0 &&
            (!controllerOwned || now - LastQCastTick > 140)) {
            QDirections.Spend(
                direction, now,
                QPerDirectionCooldownSeconds(
                    SpellRank(0), BonusAttackSpeedPercent()),
                QGlobalLockSeconds(SpellRank(0)));
            PendingHudPreviousMask = LastHudMask;
            PendingHudSpent = direction;
            PendingHudSpentUntil = now + kQHudLearnMs;
            LastQDirection = direction;
        }
        LastQCastTick = now;
        LastQTargetId = EventTargetId(args);
    } else if (slot == 1) {
        LastWCastTick = now;
        LastWOrigin = args.StartPosition.IsValid() &&
            !args.StartPosition.IsZero()
            ? args.StartPosition : GameObjects::Player().Position();
        LastWAim = EventCastPosition(args);
        LastWTargetId = EventTargetId(args);
    } else if (slot == 2) {
        LastECastTick = now;
        if (eRecast) {
            EActive = false;
        } else if (!EActive) {
            EActive = true;
            EStartTick = now;
            EEndTick = now + static_cast<int>(
                kEDurationSeconds * 1000.0f);
            EForcedTargetId = ForcedETargetId();
        }
    } else if (slot == 3) {
        LastRCastTick = now;
        RChannelActive = true;
        RChannelUntil = now + static_cast<int>(
            (kRChannelSeconds + kRPostLockSeconds) * 1000.0f);
    }

    if (!controllerOwned) {
        PlayerOverrideUntil = now + Slider(
            TacticsMenu, "ManualOwnershipMs", 520);
        ActiveSequence = Sequence::PlayerLed;
        SequenceExpireTick = PlayerOverrideUntil + 180;
    }
}

inline float EstimatedIncomingDamage(
    const AIHeroClient& enemy,
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!enemy.IsValid()) return 0.0f;
    if (args.IsAutoAttack) {
        return std::max(35.0f,
            enemy.GetAutoAttackDamage(GameObjects::Player(), true));
    }
    return 80.0f + enemy.TotalAttackDamage() * 0.46f +
        enemy.AP() * 0.52f;
}

inline bool IsTrueDamageOnlySpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellEventNameContainsAny(args, {
        "summonerdot", "ignite", "fiorapassive", "fiorap",
        "vaynew", "silverbolts", "velkozresearchproc",
        "garenr", "dariusrexecute", "chogathr", "feast",
        "pykeexecute", "urgotrrecast",
    });
}

inline void RecordEnemySpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto analysis = AnalyzeEnemyCast(
        args, 220.0f, 110.0f, 300, 250, 220, 1700, 650);
    if (!analysis.Valid) return;
    const int now = Now();
    const int id = static_cast<int>(analysis.Enemy.NetworkId());
    EnemyWindow* record = FindEnemyWindow(id, true);
    if (record) {
        if (analysis.Committed) {
            record->CommittedUntil = std::max(
                record->CommittedUntil,
                std::max(now + 620, analysis.CommitmentUntilTick));
        }
        if (analysis.LikelyHardCrowdControl) {
            record->HardCrowdControlSpentUntil = std::max(
                record->HardCrowdControlSpentUntil, now + 3400);
        }
        if (analysis.Enemy.IsDashing() ||
            SpellEventNameContainsAny(args, {
                "dash", "leap", "jump", "blink", "lunge",
            })) {
            record->DashSpentUntil = std::max(
                record->DashSpentUntil, now + 3200);
        }
    }
    if (!analysis.TargetsPlayer && !analysis.CrossesPlayer) return;

    const int castMs = ControllerHelpers::NormalizedCastDelayMs(
        args.CastDelay, args.IsAutoAttack ? 180 : 250);
    float travel = 0.0f;
    if (std::isfinite(args.MissileSpeed) && args.MissileSpeed > 80.0f) {
        Vector3 start = args.StartPosition;
        if (!start.IsValid() || start.IsZero()) {
            start = analysis.Enemy.Position();
        }
        travel = start.Distance2D(GameObjects::Player().Position()) /
            args.MissileSpeed;
    }
    IncomingImpactTick = now + std::clamp(
        castMs + static_cast<int>(travel * 1000.0f), 80, 4200);
    IncomingThreatUntil = std::max(
        std::max(analysis.CommitmentUntilTick,
                 analysis.LineThreatUntilTick),
        IncomingImpactTick + 240);
    const float observedDamage = EstimatedIncomingDamage(
        analysis.Enemy, args);
    const bool trueOnly = IsTrueDamageOnlySpell(args);
    if (observedDamage >= IncomingDamage) {
        IncomingDamage = observedDamage;
        IncomingTrueDamageOnly = trueOnly;
    } else if (!trueOnly) {
        IncomingTrueDamageOnly = false;
    }
    IncomingHardCrowdControl = analysis.LikelyHardCrowdControl;
    if (analysis.CrossesPlayer) {
        IncomingLineStart = args.StartPosition;
        IncomingLineEnd = args.EndPosition;
        IncomingLineWidth = 110.0f;
    }
}

inline void ObserveAttack(int targetId) {
    if (targetId == 0) return;
    const int now = Now();
    if (LastAutoProcessTick > 0 && now - LastAutoProcessTick <= 180 &&
        LastAutoTargetId == targetId) return;
    LastAutoProcessTick = now;
    LastAutoTargetId = targetId;
    LastAutoTick = now;
    const AIBaseClient target = UnitByNetworkId(targetId);
    const bool epic = target.IsValid() && IsEpicMonster(target);
    (void)RAttackTracker.ObserveApplication(
        targetId, now, SpellRank(3),
        GameObjects::Player().BonusAttackDamage(), epic);
}

inline void OnProcessSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) ObserveLocalSpell(args);
    else RecordEnemySpell(args);
}

inline void OnDoCast(
    const SDK::Events::ProcessSpellEventArgs& args) {
    int targetId = 0;
    int tick = 0;
    if (CaptureLocalAutoAttack(args, targetId, tick)) {
        ObserveAttack(targetId);
    }
}


inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    int targetId = 0;
    int tick = 0;
    if (CaptureAfterAttack(args, targetId, tick)) ObserveAttack(targetId);
}

inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (CaptureGapcloser(
            args, GapcloserTargetId, GapcloserEnd,
            GapcloserExpireTick, 900.0f, 1250)) {
        EnemyWindow* record = FindEnemyWindow(GapcloserTargetId, true);
        if (record) {
            record->CommittedUntil = std::max(
                record->CommittedUntil, GapcloserExpireTick);
            record->DashSpentUntil = std::max(
                record->DashSpentUntil, Now() + 3600);
        }
    }
}

inline void OnInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(
        args, InterruptTargetId, InterruptExpireTick,
        1100, 260, 6000);
    EnemyWindow* record = FindEnemyWindow(InterruptTargetId, true);
    if (record) record->CommittedUntil = std::max(
        record->CommittedUntil, InterruptExpireTick);
}

inline bool BuffNameIs(const SDK::Events::BuffEventArgs& args,
                       std::initializer_list<const char*> names) {
    return ControllerHelpers::TextContainsAny(args.BuffName, names);
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args,
                            bool added) {
    const int now = Now();
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender)) {
        if (BuffNameIs(args, { "belvethe", "royalmaelstrom" })) {
            EActive = added;
            if (added) {
                EStartTick = now;
                EEndTick = ControllerHelpers::BuffExpireTick(
                    args, static_cast<int>(kEDurationSeconds * 1000.0f));
                EForcedTargetId = ForcedETargetId();
            }
        }
        if (BuffNameIs(args, { "belvethrinside" })) {
            RChannelActive = added;
            RChannelUntil = added
                ? ControllerHelpers::BuffExpireTick(args, 1500) : now;
        }
        if (BuffNameIs(args, { "belvethrsteroid" })) {
            TrueFormActive = added;
            if (added) {
                const float duration = RFormDurationSeconds(LavenderStacks);
                TrueFormExpireTick = std::isinf(duration)
                    ? std::numeric_limits<int>::max()
                    : ControllerHelpers::BuffExpireTick(
                        args, static_cast<int>(duration * 1000.0f));
            } else {
                TrueFormExpireTick = now;
                EnhancedFormActive = false;
            }
        }
        if (BuffNameIs(args, { "belvethpassivestacks" })) {
            LavenderStacks = added ? std::max(0, args.Count) : 0;
        }
        if (BuffNameIs(args, { "belvethpassivesheen" })) {
            PassiveSheenActive = added;
            PassiveSheenExpireTick = added
                ? ControllerHelpers::BuffExpireTick(
                    args, static_cast<int>(
                        kPassiveSheenDurationSeconds * 1000.0f))
                : 0;
        }
        return;
    }

    if (added && LastWCastTick > 0 &&
        now - LastWCastTick <= kWRefreshObserveMs &&
        BuffNameIs(args, { "belvethw", "aboveandbelow" })) {
        const AIHeroClient enemy = HeroByNetworkId(id);
        if (!Engine::ValidEnemy(enemy)) return;
        const Quadrant reset = QuadrantForPoints(
            LastWOrigin, enemy.Position());
        const std::uint8_t mask = QuadrantMask(reset);
        if (mask != 0u) {
            QDirections.Refresh(mask, now);
            PendingHudRefreshMask = static_cast<std::uint8_t>(
                PendingHudRefreshMask | mask);
            PendingHudRefreshUntil = now + kWRefreshObserveMs;
            LastWHitTick = now;
            LastWTargetId = id;
        }
    }
}



inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid() || !IsCoralObject(args) ||
        !ObjectEventIsAllied(args)) return;
    ObserveCoral(
        static_cast<int>(args.Sender.NetworkId),
        args.Sender.Position, true);
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsEnhancedSourceObject(args)) {
        RecentEnhancedSourcePosition = args.Sender.Position;
        RecentEnhancedSourceUntil = Now() + 2600;
    }
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (CoralRecord* coral = FindCoral(id)) *coral = {};
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::Duel: return "duel";
    case Posture::ShortTrade: return "short trade";
    case Posture::SecondEntry: return "second entry";
    case Posture::Chase: return "chase";
    case Posture::Execute: return "execute";
    case Posture::Kite: return "kite";
    case Posture::Flee: return "flee";
    case Posture::Objective: return "objective";
    case Posture::Clear: return "clear";
    default: return "neutral";
    }
}

inline const char* SequenceName(Sequence sequence) {
    switch (sequence) {
    case Sequence::AutoQAuto: return "AA-Q-AA";
    case Sequence::QWQChase: return "Q-W-Q";
    case Sequence::WLockedE: return "W lock -> E";
    case Sequence::DefensiveE: return "defensive E";
    case Sequence::ECancelDodge: return "E cancel -> Q dodge";
    case Sequence::CoralExecute: return "coral execute";
    case Sequence::CoralSave: return "coral heal";
    case Sequence::EnhancedMacro: return "enhanced coral";
    case Sequence::TrueFormWallFlank: return "true-form wall flank";
    case Sequence::JungleWallCancel: return "Q wall cancel";
    case Sequence::SecondEntry: return "second entry";
    case Sequence::PlayerLed: return "player-led";
    default: return "idle";
    }
}

inline const char* RReasonName(RReason reason) {
    switch (reason) {
    case RReason::FirstForm: return "first form";
    case RReason::Execute: return "execute";
    case RReason::HealSave: return "heal save";
    case RReason::EnhancedMacro: return "enhanced";
    case RReason::Expiring: return "expiring";
    case RReason::Manual: return "manual";
    default: return "hold";
    }
}

inline void OnDraw() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    if (Bool(CoachMenu, "DrawQ", true)) {
        for (int i = 0; i < 4; ++i) {
            const Quadrant quadrant = static_cast<Quadrant>(i);
            const Vector3 end = player.Position() +
                QuadrantCenter(quadrant) * kQOpenDashDistance;
            const bool ready = InternalDirectionReady(quadrant, now);
            Drawing::DrawLine(
                player.Position(), end,
                ready ? 0xDDAF7DFFu : 0x66707070u,
                ready ? 2.3f : 1.1f);
            Drawing::DrawCircle(
                end, 24.0f,
                ready ? 0xDDAF7DFFu : 0x66707070u,
                1.4f, 18);
        }
        if (LastQPlan.Valid) {
            Drawing::DrawLine(
                player.Position(), LastQPlan.Endpoint,
                0xDDF2A4FFu, 2.0f);
        }
    }
    if (Bool(CoachMenu, "DrawW", true) && LastWPlan.Valid) {
        Drawing::DrawLine(
            player.Position(), LastWPlan.Aim,
            0xCC72D9F4u, 2.0f);
    }
    if (Bool(CoachMenu, "DrawE", true)) {
        Drawing::DrawCircle(
            player.Position(), kERadius,
            EActive ? 0xDDF35B83u : 0x668A4BAAu,
            EActive ? 2.4f : 1.0f, 64);
        const AIBaseClient forced = UnitByNetworkId(EForcedTargetId);
        if (forced.IsValid()) {
            Drawing::DrawCircle(
                forced.Position(), forced.BoundingRadius() + 38.0f,
                0xDDF35B83u, 2.0f, 32);
        }
    }
    if (Bool(CoachMenu, "DrawCorals", true)) {
        for (const auto& coral : Corals) {
            if (coral.NetworkId == 0 || coral.ExpireTick < now ||
                !coral.Position.IsValid()) continue;
            Drawing::DrawCircle(
                coral.Position, kRRadius,
                coral.Enhanced ? 0xDDF0A4FFu : 0xCC8B66D9u,
                coral.Enhanced ? 2.4f : 1.6f, 64);
            Drawing::DrawLine(
                player.Position(), coral.Position,
                coral.Enhanced ? 0x99F0A4FFu : 0x668B66D9u,
                1.1f);
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char state[640]{};
            _snprintf_s(
                state, sizeof(state), _TRUNCATE,
                "Bel'Veth OTP | %s | %s | Q %d/4 | lavender %d | sheen %s | R %s | corals %d | form %.0fs%s",
                PostureName(CurrentPosture), SequenceName(ActiveSequence),
                ReadyDirectionCount(), LavenderStacks,
                PassiveSheenActive ? "active" : "idle",
                RReasonName(LastRReason), ActiveCoralCount(),
                TrueFormSecondsRemaining(),
                TrueFormExpireTick == std::numeric_limits<int>::max()
                    ? " permanent" : "");
            Drawing::DrawText(
                screen.x - 300.0f, screen.y - 112.0f,
                0xFFE8C5FFu, state);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "BelvethOneTrick", "Bel'Veth one-trick conductor"));
    TacticsMenu->Add(new MenuBool(
        "KillSecure", "Exact lethal windows", true));
    TacticsMenu->Add(new MenuSlider(
        "KiteHp", "Kite posture HP (%)", 36, 10, 80));
    TacticsMenu->Add(new MenuSlider(
        "ManualOwnershipMs", "Yield player spell (ms)",
        520, 180, 1200));
    TacticsMenu->Add(new MenuSeparator(
        "SecondEntry",
        "Bel'Veth waits for enemy"));

    PassiveMenu = TacticsMenu->AddSubMenu(new Menu(
        "DeathInLavender", "Three-second spell haste and persistent R ramp"));
    PassiveMenu->Add(new MenuSeparator(
        "Weave",
        "Each ability starts the passive attack-speed window"));

    QMenu = TacticsMenu->AddSubMenu(new Menu(
        "VoidSurge", "Four map-fixed Q sectors and AA reset"));
    QMenu->Add(new MenuBool(
        "Combo", "Q farm sectors in combo", true));
    QMenu->Add(new MenuBool(
        "AutoReset", "Q after confirmed AA", true));
    QMenu->Add(new MenuBool(
        "Harass", "Use only AA-Q harass windows", true));
    QMenu->Add(new MenuBool(
        "Flee", "Cursor-Q flee sectors", true));
    QMenu->Add(new MenuBool(
        "EvadeSkillshots", "Spend a safe sector on a", true));
    QMenu->Add(new MenuBool(
        "WallFlanks", "Use true-form Q only through", true));
    QMenu->Add(new MenuBool(
        "AllowLethalDive", "Allow a lethal Q endpoint", false));
    QMenu->Add(new MenuSlider(
        "MaxEndpointEnemies", "Max enemies Q endpoint", 2, 1, 5));
    QMenu->Add(new MenuSeparator(
        "Calibration",
        "BelvethQHudIcon0..15 is"));

    WMenu = TacticsMenu->AddSubMenu(new Menu(
        "AboveAndBelow", "Reliable knock-up and multi-sector Q refund"));
    WMenu->Add(new MenuBool(
        "Combo", "W for a reliable lock or", true));
    WMenu->Add(new MenuBool(
        "Harass", "W harass: Q refund only", true));
    WMenu->Add(new MenuBool(
        "Flee", "W peel pursuer", true));
    WMenu->Add(new MenuList(
        "Hitchance", "Ordinary W prediction",
        { "Medium", "High", "Very high", "Immobile only" }, 1));
    WMenu->Add(new MenuSeparator(
        "NoRawW",
        "Mobile targets must first"));

    EMenu = TacticsMenu->AddSubMenu(new Menu(
        "RoyalMaelstrom", "Forced-target execute and damage reduction"));
    EMenu->Add(new MenuBool(
        "Combo", "E after W/Q window", true));
    EMenu->Add(new MenuBool(
        "Defensive", "E vs meaningful reducible", true));
    EMenu->Add(new MenuBool(
        "Jungle", "E jungle/epic", true));
    EMenu->Add(new MenuSlider(
        "ExecuteHp", "E execute thresh (%)", 34, 10, 60));
    EMenu->Add(new MenuSeparator(
        "ForcedTarget",
        "E is authorized only after"));
    EMenu->Add(new MenuSeparator(
        "Cancel",
        "After 0.75s, E cancels for"));

    RMenu = TacticsMenu->AddSubMenu(new Menu(
        "EndlessBanquet", "Global coral consumption and form economy"));
    RMenu->Add(new MenuBool(
        "Automatic", "Consume a safe valuable coral", true));
    RMenu->Add(new MenuBool(
        "Survival", "Use coral heal vs tracked", true));
    RMenu->Add(new MenuSlider(
        "HealHp", "Coral heal HP (%)", 36, 5, 75));
    RMenu->Add(new MenuKeyBind(
        "ManualCoral", "Consume best safe cursor-side coral [T]",
        SDK::Keys::T, KeyBindType::Press));
    RMenu->Add(new MenuSeparator(
        "GlobalConsume",
        "Casting R consumes every"));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "AA-reset clear and jungle wall cancel"));
    FarmMenu->Add(new MenuBool(
        "LaneQ", "Use lane Q only after AA or", true));
    FarmMenu->Add(new MenuBool(
        "LaneW", "W lane min 3 hits", false));
    FarmMenu->Add(new MenuBool(
        "JungleQ", "Jungle AA-Q/short-Q", true));
    FarmMenu->Add(new MenuBool(
        "JungleW", "W jungle min 2", true));
    FarmMenu->Add(new MenuSlider(
        "ReserveQ", "Q sectors for farm", 1, 0, 3));
    FarmMenu->Add(new MenuSeparator(
        "PlayerOwned",
        "Orbwalker owns attacks and"));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "Bel'Veth one-trick geometry and state"));
    CoachMenu->Add(new MenuBool(
        "DrawQ", "Draw Q sectors", true));
    CoachMenu->Add(new MenuBool(
        "DrawW", "Draw the last selected W line", true));
    CoachMenu->Add(new MenuBool(
        "DrawE", "Draw E radius/target", true));
    CoachMenu->Add(new MenuBool(
        "DrawCorals", "Draw ordinary and enhanced", true));
    CoachMenu->Add(new MenuBool(
        "DrawState", "Draw state/seq,", true));
}

inline void OnLoad() {
    const int now = Now();
    LastQPathCache = {};
    ActiveSequence = Sequence::None;
    CurrentPosture = Posture::Neutral;
    LastRReason = RReason::None;
    LastDecisionTargetId = 0;
    QDirections = {};
    QDirections.ReadyTick.fill(now);
    QHudCalibration = {};
    LastHudMask = -1;
    PendingHudPreviousMask = -1;
    PendingHudSpent = Quadrant::Invalid;
    PendingHudSpentUntil = 0;
    PendingHudRefreshMask = 0u;
    PendingHudRefreshUntil = 0;
    LastQCastTick = LastQTargetId = 0;
    LastQPurpose = QPurpose::None;
    LastQDirection = Quadrant::Invalid;
    LastQPlan = {};
    LastWCastTick = LastWHitTick = LastWTargetId = 0;
    LastWOrigin = LastWAim = {};
    LastWPlan = {};
    EActive = false;
    EStartTick = EEndTick = EDesiredTargetId = EForcedTargetId = 0;
    LastECastTick = 0;
    LastEPlan = {};
    RChannelActive = TrueFormActive = EnhancedFormActive = false;
    RChannelUntil = TrueFormExpireTick = LastRCastTick = 0;
    LastRPlan = {};
    Corals.fill({});
    RecentEnhancedSourcePosition = {};
    RecentEnhancedSourceUntil = 0;
    LastAutoTargetId = LastAutoTick = LastAutoProcessTick = 0;
    PassiveSheenActive = false;
    PassiveSheenExpireTick = LavenderStacks = 0;
    RAttackTracker.Reset();
    EnemyWindows.fill({});
    IncomingThreatUntil = IncomingImpactTick = 0;
    IncomingDamage = 0.0f;
    IncomingTrueDamageOnly = IncomingHardCrowdControl = false;
    IncomingLineStart = IncomingLineEnd = {};
    IncomingLineWidth = 0.0f;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEnd = {};
    InterruptTargetId = InterruptExpireTick = 0;
    LastLocalSpellTick = PlayerOverrideUntil = SequenceExpireTick = 0;
}

inline void OnUnload() {
    TacticsMenu = PassiveMenu = QMenu = WMenu = nullptr;
    EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    LastQPathCache = {};
}

inline constexpr const char* Scenarios[] = {
    "Pin live behavior to Riot 26.15 and CommunityDragon PC 16.15",
    "Migrate the complete live Bel'Veth scaling update without PBE exclusions",
    "Use the pinned CommunityDragon 16.15 champion bin as machine data",
    "Preserve Riot 25.12 rank-scaled coral cast range",
    "Treat attacks and movement as player-orbwalker owned",
    "Never automate Flash or Smite",
    "Yield a configurable ownership window after every manual spell",
    "Resume only after the player's ownership window expires",
    "Keep selected-target intent without overriding orbwalker attacks",
    "Start the passive 20-percent attack-speed window after an observed ability",
    "Keep the passive attack-speed window for three seconds rather than charges",
    "Repair passive attack-speed state from BelvethPassiveSheen",
    "Read permanent Lavender stacks from BelvethPassiveStacks",
    "Reconcile Lavender stacks by event and polling telemetry",
    "Apply R passive true damage on every observed attack",
    "Retain R passive ramp when the attacked target changes",
    "Reset stale R passive ramp only after its live stack duration",
    "Cap epic-monster R passive ramp at eight applications",
    "Permit Q and E to apply R passive without prior target priming",
    "Do not invent unobserved Q or E passive applications in telemetry",
    "Never reject an orbwalker target merely to preserve R passive ramp",
    "Allow intentional target changes in every orbwalker mode",
    "Represent Q as four independent map-fixed diagonal sectors",
    "Aim freely inside each Q sector rather than at its center only",
    "Bias Q near a sector boundary to preserve near-parallel double-Q tech",
    "Track the rank-scaled Q global lock separately from sector cooldowns",
    "Apply the live 0.2 Q haste conversion from bonus attack speed",
    "Read every BelvethQHudIcon0 through BelvethQHudIcon15 state",
    "Learn HUD-bit-to-world-sector mapping from an actual spent Q",
    "Learn HUD-bit refresh mapping from an actual W hit",
    "Never hard-code CommunityDragon HUD bit ordering",
    "Repair internal Q estimates from learned HUD bits",
    "Use fixed 850 open-ground Q speed across ranks over its 400-unit dash",
    "Treat Q as a 625-unit maximum true-form terrain crossing",
    "Trace NavMesh samples for every proposed Q endpoint",
    "Stop normal-form Q at the last open sample before terrain",
    "Require a real wall exit for true-form terrain Q",
    "Reject a true-form Q that remains inside terrain",
    "Reject grounded Q casts",
    "Reject Q during an attack windup except lethal or defensive use",
    "Use Q immediately after a confirmed attack for AA-Q-AA",
    "Preserve an available auto attack before ordinary Q",
    "Calculate current Q damage with live rank scaling and total AD",
    "Apply the live Q monster bonus",
    "Apply no separate Q minion damage modifier",
    "Model Q as a capsule with its real gameplay half-width",
    "Order every Q collision body along the dash segment",
    "Prefer the intended target as the first Q collision body",
    "Score extra farm bodies without treating structures as Q targets",
    "Reject an unsafe nonlethal Q endpoint under enemy turret",
    "Permit a configured lethal Q dive only after endpoint evaluation",
    "Reject a Q endpoint inside a ready anti-dash hazard",
    "Reject an ordinary offensive Q endpoint inside point-click lockdown",
    "Reject a Q endpoint that becomes badly outnumbered",
    "Respect cursor direction when choosing between legal Q sectors",
    "Spend Q to leave a tracked line skillshot only when endpoint exits it",
    "Choose the safer perpendicular side of an incoming line from cursor intent",
    "Use cursor-aligned Q while fleeing",
    "Conserve the final forward Q when it neither hits nor gets refunded",
    "Conserve the final total Q outside lethal, evade and flee windows",
    "Do not spend both forward Qs merely to enter attack range",
    "Allow W to justify spending a Q only when W can refund that same sector",
    "Use true-form wall flank only when actual terrain separates target and player",
    "Shorten jungle Q against a nearby wall after an attack",
    "Use lane Q only after an attack or for an exact last hit",
    "Reserve the configured number of Q sectors while farming",
    "Use W gameplay range 660 rather than resource-only values",
    "Use W cast time 0.5 seconds for every prediction",
    "Model W as a 200-wide line",
    "Calculate current W magic damage from rank base and 150-percent AP",
    "Reject W while E is active",
    "Reject ordinary W during an attack windup",
    "Allow reactive W to interrupt despite an attack windup",
    "Require configured hitchance for ordinary W",
    "Treat hard crowd control as a reliable W window",
    "Treat an existing slow as a reliable W window",
    "Treat a spent dash as a reliable W window",
    "Treat an enemy committed cast as a reliable W window",
    "Prefer W after the target spends mobility instead of throwing it raw",
    "Use W immediately against an interruptible channel",
    "Use W to peel a directed gapcloser",
    "Predict every champion separately before evaluating multi-hit W",
    "Aim W down a bisector only if both predicted hitboxes remain in the line",
    "Determine every W-refreshed Q sector relative to W cast origin",
    "Refresh multiple Q sectors when W hits champions in multiple quadrants",
    "Prefer W that refunds a currently spent Q sector",
    "Require usable follow-up before spending W only for a refund",
    "Allow exact lethal W even when no Q sector is refunded",
    "Use lane W only for at least three real bodies",
    "Use jungle W only for at least two real monsters",
    "Hold farming W whenever an enemy champion is nearby",
    "Select E's forced victim by lowest current health percentage",
    "Break equal E health percentages by nearest distance",
    "Include champions, lane minions and monsters in forced E selection",
    "Show the actual forced E victim in coach drawing",
    "Reject offensive E when its forced victim is not the desired champion",
    "Permit jungle E when the forced victim is a monster",
    "Reject E while Blind prevents declaring attacks",
    "Reject E while Disarm prevents declaring attacks",
    "Use six E strikes plus one per 40-percent bonus attack speed",
    "Recompute missing-health multiplier after every simulated E strike",
    "Use live one-to-two E missing-health damage ramp",
    "Use live 12-percent on-hit effectiveness in the deterministic model",
    "Apply the live 200-percent E monster modifier",
    "Calculate mitigated physical E damage against the actual forced victim",
    "Use E late when the actual forced champion is executable",
    "Use E after a recent Q or W only on a secured low target",
    "Hold E when the desired target is running unsecured",
    "Allow E against a slowed target",
    "Allow E against a hard crowd-controlled target",
    "Allow E against a target committed into Bel'Veth",
    "Reject unsafe offensive E without a returning Q escape",
    "Use E defensively only against meaningful incoming reducible burst",
    "Never claim E damage reduction against true-damage-only pressure",
    "Use rank-scaled 20-60 percent E reduction and 20-40 percent lifesteal",
    "Use jungle E for sustain only below the modeled health thresholds",
    "Use E to secure a low epic monster only when it is the forced victim",
    "Keep E channel for its first mandatory 0.75 seconds",
    "Keep E channel while it still attacks the desired victim",
    "After 0.75 seconds cancel E with Q for a tracked lethal skillshot",
    "After 0.75 seconds cancel E with safe Q when the desired target escaped",
    "Do not cancel E merely because another low-health minion entered range",
    "Recast E early only for explicit cursor retreat after danger ends",
    "Never cancel E for ordinary damage optimization before 0.75 seconds",
    "Track E start and end from both local spell and buff events",
    "Use live R passive base damage 2, 4 and 6",
    "Use live R passive 3-percent bonus-AD ratio",
    "Track R coral lifetime as fifteen seconds",
    "Track true form as 45, 90 or permanent at 0, 40 or 80 stacks",
    "Apply the same stack duration rule to ordinary and enhanced form",
    "Never infer enhanced form from duration after the 26.15 migration",
    "Identify Baron, Rift Herald and Voidgrub as enhanced-coral sources",
    "Do not treat ordinary champion coral as enhanced",
    "Treat Voidgrubs as one enhanced-source group through the live coral event",
    "Use rank-scaled R cast ranges 275, 375 and 450",
    "Include coral gameplay radius tolerance in castability",
    "Cast R on the coral unit rather than at an arbitrary position",
    "Model the one-second R channel before explosion",
    "Predict enemy positions to the R explosion time",
    "Model R explosion as a 500-radius circle",
    "Use live R true damage base 150, 200 and 250",
    "Use live R 150-percent AP ratio",
    "Use live R 20-percent target missing-health scaling",
    "Cap R damage to monsters at 1500",
    "Count exact R hits and kills at each candidate coral",
    "Use live R heal bases 100, 250, 400 with 150-percent bonus AD and AP",
    "Value meaningful survival heal against tracked incoming pressure",
    "Reject R channel at an unsafe nonlethal coral",
    "Reject ordinary nonlethal R under enemy turret",
    "Reject R during an attack windup unless lethal or survival-critical",
    "Reject a coral endpoint inside a ready anti-dash hazard",
    "Reject a coral endpoint inside point-click lockdown without payoff",
    "Respect cursor direction for automatic coral selection",
    "Select the safest cursor-side coral for manual T input",
    "Treat manual coral as assisted selection rather than unconditional danger override",
    "Remember that one R cast consumes every existing coral globally",
    "Count every consumed coral in R opportunity cost",
    "Use the soonest global coral expiry when evaluating a cast",
    "Prefer an enhanced coral over an ordinary coral at equal safety",
    "Hold an ordinary form refresh while substantial normal form remains",
    "Hold an ordinary extension while substantial enhanced form remains",
    "Allow an expiring coral to override ordinary form waste",
    "Allow an R AoE execute to override ordinary form waste",
    "Allow meaningful survival heal to override ordinary form waste",
    "Use enhanced coral before a safe wave-macro window",
    "Use enhanced coral around a live epic-objective window",
    "Reset ordinary and enhanced form to the stack-derived duration",
    "Treat 80 Lavender stacks as permanent form until death",
    "Enter multi-enemy fights second rather than acting as primary engage",
    "Recognize enemy casts, dashes and crowd control as a second-entry signal",
    "Recognize allied proximity to a low target as a second-entry signal",
    "Allow duel posture against one nearby enemy",
    "Switch to kite posture at configured low health",
    "Switch to execute posture against a low target",
    "Switch to chase posture only when target path is moving away",
    "Use Q-W-Q when W can refund the just-spent chase sector",
    "Use safe Q-W-E as a short secured sequence",
    "Preserve repeated AA-AA-Q ramp after the opening sequence",
    "Prefer E at the end of a secured combo rather than at its start",
    "Do not use E on a freely escaping target merely because E is ready",
    "Capture directed gapclosers and respond with reliable W peel",
    "Capture interruptible channels and prioritize W knock-up",
    "Estimate line impact time from cast delay and missile travel",
    "Track whether incoming pressure is reducible or true-damage-only",
    "Expire stale incoming threats instead of holding defensive state forever",
    "Expire stale gapcloser and interrupt records",
    "Track ordinary and enhanced coral objects by network id",
    "Repair missed coral-create events by scanning allied minions",
    "Remove consumed, deleted and expired coral records",
    "Draw independently ready and spent Q sectors",
    "Draw the selected W line rather than a generic range",
    "Draw E radius and the current forced target",
    "Draw ordinary and enhanced coral payoff circles separately",
    "Expose posture, sequence, Lavender stacks, passive sheen, Q count and form duration",
    "Own Bel'Veth's complete spell loop without generic Q-W-E-R fallback",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Belveth;
    controller.ControllerId = "champion.kuroaio.ai.belveth.controller";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIBelveth.md";
    controller.ImplementationSummary =
        "Self-calibrating four-sector AA-Q-AA logic with W multi-sector "
        "refunds, exact lowest-health-percent E targeting and post-0.75s "
        "cancels, second-entry combat posture, and global ordinary/enhanced "
        "coral form/heal/execute economy.";
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
    controller.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Belveth
