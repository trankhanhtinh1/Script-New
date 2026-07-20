#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AITaliyahGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Taliyah {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureBeforeAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::CursorDirectionAgrees;
using ControllerHelpers::EnemySpellReady;
using ControllerHelpers::HasEnemyChampionNear;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLargeLaneMinion;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocks;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::SpellSlotOrEventNameContainsAny;
using ControllerHelpers::UnitByNetworkId;
using ControllerHelpers::ValidHostileUnitInGameplayRange;

enum class Posture : std::uint8_t {
    Neutral,
    Poke,
    Catch,
    FrontToBack,
    AntiDash,
    Peel,
    Farm,
    Objective,
    Wall,
};

enum class Sequence : std::uint8_t {
    None,
    CombatBranch,
    ReactiveDash,
    ReactivePeel,
    ReactiveFlee,
    ManualWAssist,
    ManualEAssist,
    ManualBoulderAssist,
    JungleEW,
};

struct QPlan {
    Vector3 CastPosition = {};
    QContact FirstContact = {};
    CastEvaluation Evaluation = {};
    std::vector<int> SplashVictims = {};
    int IntendedId = 0;
    QForm Form = QForm::Volley;
    QPurpose Purpose = QPurpose::Poke;
    SDK::HitChance Hitchance = SDK::HitChance::None;
    float RawDamage = 0.0f;
    float DealtDamage = 0.0f;
    bool AoeBridge = false;
    bool Lethal = false;
    bool Valid = false;
};

struct WPlan {
    Vector3 Center = {};
    Vector3 DirectionEnd = {};
    Vector3 Destination = {};
    MineContactSummary MineContacts = {};
    Minefield PlannedMinefield = {};
    CastEvaluation Evaluation = {};
    int TargetId = 0;
    WPurpose Purpose = WPurpose::MinefieldCatch;
    bool UsesPlannedE = false;
    bool Valid = false;
};

struct EPlan {
    Vector3 CastPosition = {};
    Minefield Field = {};
    CastEvaluation Evaluation = {};
    int TargetId = 0;
    int InitialHits = 0;
    int ExpectedMineContacts = 0;
    EPurpose Purpose = EPurpose::ComboSetup;
    bool Valid = false;
};

struct WallPlan {
    Vector3 Endpoint = {};
    WallContext Context = {};
    CastEvaluation Evaluation = {};
    WallPurpose Purpose = WallPurpose::Rotation;
    bool Valid = false;
};

struct EnemyWindow {
    int NetworkId = 0;
    int CommittedUntil = 0;
    int HardCrowdControlSpentUntil = 0;
    int MobilitySpentUntil = 0;
    int IncomingLineUntil = 0;
};

struct MobilityRule {
    const char* Champion = "";
    SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
};

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* MineMenu = nullptr;
inline Menu* PeelMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* WallMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::vector<WorkedGroundZone> GroundZones = {};
inline int NextGroundZoneId = 1;
inline Minefield ActiveMinefield = {};
inline int ActiveMinefieldExpireTick = 0;
inline std::array<EnemyWindow, 20> EnemyWindows = {};

inline Posture CurrentPosture = Posture::Neutral;
inline Sequence ActiveSequence = Sequence::None;
inline ComboBranch ActiveBranch = ComboBranch::None;
inline int SequenceStep = 0;
inline int SequenceTargetId = 0;
inline int SequenceExpireTick = 0;
inline bool SequenceReactive = false;

inline QPlan LastQPlan = {};
inline WPlan LastWPlan = {};
inline EPlan LastEPlan = {};
inline WallPlan LastWallPlan = {};

inline int ProtectedAllyId = 0;
inline int PeelThreatId = 0;
inline int PeelThreatUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int IncomingCrowdControlUntil = 0;
inline int PredictedDamageLockoutUntil = 0;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline QForm LastQForm = QForm::Volley;
inline int LastQTargetId = 0;
inline int LastWTargetId = 0;
inline Vector3 LastWCenter = {};
inline Vector3 LastWDirectionEnd = {};
inline int LastBeforeAttackTargetId = 0;
inline int LastBeforeAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int LastLocalAutoTargetId = 0;
inline int LastLocalAutoTick = 0;
inline std::array<int, 4> LastRegisteredCastTick = {};
inline int PlayerOverrideUntil = 0;
inline int BoulderSlowTargetId = 0;
inline int BoulderSlowStartTick = 0;
inline int BoulderSlowUntil = 0;
inline int RChannelUntil = 0;

inline constexpr int kSequenceLifetimeMs = 4400;
inline constexpr int kReactiveLifetimeMs = 1800;
inline constexpr int kManualOwnershipMs = 480;

inline constexpr std::array<MobilityRule, 50> MobilityRules = {
    MobilityRule{ "Ahri", SDK::SpellSlot::R },
    MobilityRule{ "Akali", SDK::SpellSlot::E },
    MobilityRule{ "Akshan", SDK::SpellSlot::E },
    MobilityRule{ "Ambessa", SDK::SpellSlot::E },
    MobilityRule{ "Azir", SDK::SpellSlot::E },
    MobilityRule{ "Belveth", SDK::SpellSlot::Q },
    MobilityRule{ "Briar", SDK::SpellSlot::Q },
    MobilityRule{ "Camille", SDK::SpellSlot::E },
    MobilityRule{ "Corki", SDK::SpellSlot::W },
    MobilityRule{ "Diana", SDK::SpellSlot::E },
    MobilityRule{ "Ekko", SDK::SpellSlot::E },
    MobilityRule{ "Elise", SDK::SpellSlot::Q },
    MobilityRule{ "Evelynn", SDK::SpellSlot::R },
    MobilityRule{ "Ezreal", SDK::SpellSlot::E },
    MobilityRule{ "Fizz", SDK::SpellSlot::E },
    MobilityRule{ "Galio", SDK::SpellSlot::E },
    MobilityRule{ "Gnar", SDK::SpellSlot::E },
    MobilityRule{ "Gragas", SDK::SpellSlot::E },
    MobilityRule{ "Graves", SDK::SpellSlot::E },
    MobilityRule{ "Irelia", SDK::SpellSlot::Q },
    MobilityRule{ "JarvanIV", SDK::SpellSlot::Q },
    MobilityRule{ "Jax", SDK::SpellSlot::Q },
    MobilityRule{ "Kaisa", SDK::SpellSlot::R },
    MobilityRule{ "Kassadin", SDK::SpellSlot::R },
    MobilityRule{ "Katarina", SDK::SpellSlot::E },
    MobilityRule{ "Kayn", SDK::SpellSlot::Q },
    MobilityRule{ "Khazix", SDK::SpellSlot::E },
    MobilityRule{ "Kled", SDK::SpellSlot::E },
    MobilityRule{ "Leblanc", SDK::SpellSlot::W },
    MobilityRule{ "LeeSin", SDK::SpellSlot::Q },
    MobilityRule{ "LeeSin", SDK::SpellSlot::W },
    MobilityRule{ "Lucian", SDK::SpellSlot::E },
    MobilityRule{ "Naafiri", SDK::SpellSlot::W },
    MobilityRule{ "Pantheon", SDK::SpellSlot::W },
    MobilityRule{ "Qiyana", SDK::SpellSlot::E },
    MobilityRule{ "Rakan", SDK::SpellSlot::W },
    MobilityRule{ "RekSai", SDK::SpellSlot::E },
    MobilityRule{ "Renekton", SDK::SpellSlot::E },
    MobilityRule{ "Rengar", SDK::SpellSlot::Q },
    MobilityRule{ "Shaco", SDK::SpellSlot::Q },
    MobilityRule{ "Sylas", SDK::SpellSlot::E },
    MobilityRule{ "Talon", SDK::SpellSlot::E },
    MobilityRule{ "Tristana", SDK::SpellSlot::W },
    MobilityRule{ "Tryndamere", SDK::SpellSlot::E },
    MobilityRule{ "Vayne", SDK::SpellSlot::Q },
    MobilityRule{ "Vi", SDK::SpellSlot::Q },
    MobilityRule{ "Yasuo", SDK::SpellSlot::E },
    MobilityRule{ "Yone", SDK::SpellSlot::E },
    MobilityRule{ "Zed", SDK::SpellSlot::W },
    MobilityRule{ "Zeri", SDK::SpellSlot::E },
};

inline bool IsQEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::Q,
        { "taliyahq", "threadedvolley" });
}

inline bool IsWEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::W,
        { "taliyahwvc", "taliyahw", "seismicshove" });
}

inline bool IsEEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::E,
        { "taliyahe", "unraveledearth" });
}

inline bool IsREvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::R,
        { "taliyahr", "weaverswall" });
}

inline int EventSlot(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsQEvent(args)) return 0;
    if (IsWEvent(args)) return 1;
    if (IsEEvent(args)) return 2;
    if (IsREvent(args)) return 3;
    return args.Slot >= 0 && args.Slot < 4 ? args.Slot : -1;
}

inline int EventTargetId(const SDK::Events::ProcessSpellEventArgs& args) {
    return static_cast<int>(args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId);
}

inline EnemyWindow* WindowFor(int networkId, bool create) {
    return ControllerHelpers::FindEnemyCastWindow(
        EnemyWindows, networkId, create);
}

inline const EnemyWindow* WindowFor(int networkId) {
    return ControllerHelpers::EnemyCastWindowById(
        EnemyWindows, networkId);
}

inline bool TargetCommitted(int networkId) {
    return ControllerHelpers::EnemyCastWindowCommitted(
        EnemyWindows, networkId);
}

inline bool MobilitySpent(int networkId) {
    const EnemyWindow* window = WindowFor(networkId);
    return window && window->MobilitySpentUntil >= Now();
}

inline bool ChampionMobilityReady(const AIHeroClient& target) {
    if (!target.IsValid() || MobilitySpent(static_cast<int>(target.NetworkId()))) {
        return false;
    }
    for (const auto& rule : MobilityRules) {
        if (ControllerHelpers::ChampionIs(target, rule.Champion) &&
            EnemySpellReady(target, rule.Slot)) {
            return true;
        }
    }
    return target.IsDashing();
}

inline Vector3 EstimatedVelocity(const AIBaseClient& unit) {
    if (!unit.IsValid() || !unit.IsMoving()) return {};
    const Vector3 end = unit.PathEnd();
    if (!end.IsValid() || end.IsZero()) return {};
    Vector3 direction = SharedGeometry::Direction2D(unit.Position(), end);
    if (direction.IsZero()) return {};
    float speed = std::max(0.0f, unit.MoveSpeed());
    if (unit.IsDashing()) speed = std::max(speed, 950.0f);
    return direction * speed;
}

inline QBody RuntimeQBody(const AIBaseClient& unit,
                          bool champion,
                          bool minion,
                          bool monster) {
    QBody body{};
    if (!unit.IsValid() || unit.IsDead()) return body;
    body.Id = static_cast<int>(unit.NetworkId());
    body.Position = unit.Position();
    body.Velocity = EstimatedVelocity(unit);
    body.Radius = unit.BoundingRadius();
    body.Health = unit.Health();
    body.MaximumHealth = unit.MaxHealth();
    body.Valid = true;
    body.Targetable = unit.IsTargetable();
    body.Hostile = unit.IsEnemy();
    body.Champion = champion;
    body.Minion = minion;
    body.Monster = monster;
    body.Large = champion || unit.BoundingRadius() >= 65.0f ||
                 unit.MaxHealth() >= 1450.0f;
    if (minion) {
        const AIMinionClient lane(unit.Address());
        body.Large = body.Large || IsLargeLaneMinion(lane);
    }
    body.Epic = monster && IsEpicMonster(unit);
    return body;
}

inline std::vector<QBody> BuildQBodies() {
    const auto player = ObjectManager::Player();
    std::vector<QBody> result;
    result.reserve(72);
    if (!player.IsValid()) return result;
    const auto append = [&](const AIBaseClient& unit,
                            bool champion,
                            bool minion,
                            bool monster) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(unit.Position()) > 1325.0f) return;
        QBody body = RuntimeQBody(unit, champion, minion, monster);
        if (ValidQBody(body)) result.push_back(body);
    };
    for (const auto& hero : GameObjects::EnemyHeroes()) {
        append(hero, true, false, false);
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        append(minion, false, true, false);
    }
    for (const auto& monster : GameObjects::Jungle()) {
        append(monster, false, false, true);
    }
    return result;
}

inline QForm CurrentQForm() {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return QForm::Volley;
    const float now = static_cast<float>(Now()) / 1000.0f;
    NormalizeWorkedGround(GroundZones, now);
    return WorkedGroundAt(
        GroundZones, player.Position(), now, player.BoundingRadius()) != 0
        ? QForm::Boulder : QForm::Volley;
}

inline float CurrentQCost(QForm form) {
    return form == QForm::Boulder ? 10.0f : SpellCost(0);
}

inline ManaCosts LiveManaCosts() {
    return { SpellCost(0), SpellCost(1), SpellCost(2), SpellCost(3) };
}

inline bool TargetBoulderSlowedAt(int networkId, int tick) {
    return networkId != 0 && networkId == BoulderSlowTargetId &&
           BoulderSlowStartTick <= tick && BoulderSlowUntil >= tick;
}

inline bool TargetBoulderSlowed(int networkId) {
    return TargetBoulderSlowedAt(networkId, Now());
}

inline void RegisterBoulderSlowWindow(int targetId,
                                      float castElapsedSeconds) {
    if (targetId == 0 || !std::isfinite(castElapsedSeconds)) return;
    const int start = Now() + static_cast<int>(std::round(
        std::max(0.0f, castElapsedSeconds) * 1000.0f));
    BoulderSlowTargetId = targetId;
    BoulderSlowStartTick = start;
    BoulderSlowUntil = start +
        static_cast<int>(kQBigSlowSeconds * 1000.0f);
}

inline float ExpectedQRawDamage(const AIBaseClient& target,
                                QForm form,
                                QPurpose purpose) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const int rank = SpellRank(0);
    const float ap = player.AP();
    const bool monster = target.IsMinion() &&
        Core::Objects::IsJungleMonster(target.Address());
    if (form == QForm::Boulder) {
        return monster
            ? QMonsterBigRawDamage(rank, ap)
            : QBigRawDamage(rank, ap);
    }
    if (monster) {
        if (IsEpicMonster(target) || target.MaxHealth() >= 1200.0f) {
            return QMonsterVolleyRawDamage(rank, ap);
        }
        return QRockRawDamage(rank, ap) + QMonsterFlatDamage(rank);
    }
    if (!target.IsHero()) return QRockRawDamage(rank, ap);
    float multiplier = 1.0f;
    if (Engine::IsHardCrowdControlled(target)) multiplier = 2.60f;
    else if (TargetBoulderSlowed(static_cast<int>(target.NetworkId()))) {
        multiplier = 2.20f;
    } else if (purpose != QPurpose::Kill &&
               (TargetCommitted(static_cast<int>(target.NetworkId())) ||
                !target.IsMoving())) {
        multiplier = 1.80f;
    } else if (purpose != QPurpose::Kill) {
        multiplier = 1.40f;
    }
    return QRockRawDamage(rank, ap) * multiplier;
}

inline float QDamage(const AIBaseClient& target,
                     QForm form,
                     QPurpose purpose) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return player.CalculateMagicDamage(
        target, ExpectedQRawDamage(target, form, purpose));
}

inline float EInitialDamage(const AIBaseClient& target) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    float raw = ERawInitialDamage(SpellRank(2), player.AP());
    if (target.IsMinion() &&
        Core::Objects::IsJungleMonster(target.Address())) {
        raw *= kEMonsterMultiplier;
    }
    return player.CalculateMagicDamage(target, raw);
}

inline float EComboDamage(const AIBaseClient& target, int contacts) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const bool monster = target.IsMinion() &&
        Core::Objects::IsJungleMonster(target.Address());
    return player.CalculateMagicDamage(
        target, ERawTotalDamage(
            SpellRank(2), player.AP(), contacts, monster));
}

inline AIHeroClient PreferredEnemy(const AIHeroClient& selected,
                                   float range = 1120.0f) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const AIHeroClient locked = HeroByNetworkId(Engine::LockedTargetNetworkId);
    if (Engine::ValidEnemy(locked, range)) return locked;
    const auto player = ObjectManager::Player();
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range)) continue;
        float score = (100.0f - enemy.HealthPercent()) * 2.2f -
            player.Position().Distance2D(enemy.Position()) * 0.10f;
        score += std::max(enemy.TotalAttackDamage(), enemy.AP() * 0.78f) *
            0.20f;
        if (Engine::IsHardCrowdControlled(enemy)) score += 320.0f;
        if (enemy.IsDashing()) score += 260.0f;
        if (TargetBoulderSlowed(static_cast<int>(enemy.NetworkId()))) {
            score += 180.0f;
        }
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    return best;
}

inline AIHeroClient ProtectedAlly() {
    return ControllerHelpers::RawAllyHeroByNetworkId(ProtectedAllyId);
}

inline AIHeroClient SelectPeelThreat(const AIHeroClient& ally) {
    if (!Engine::ValidAlly(ally)) return {};
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1250.0f)) continue;
        const float distance = enemy.Position().Distance2D(ally.Position());
        if (distance > 760.0f) continue;
        float score = 800.0f - distance +
            std::max(enemy.TotalAttackDamage(), enemy.AP() * 0.75f);
        if (enemy.IsDashing() && enemy.PathEnd().IsValid() &&
            enemy.PathEnd().Distance2D(ally.Position()) < distance) {
            score += 430.0f;
        }
        if (ChampionMobilityReady(enemy)) score += 125.0f;
        if (static_cast<int>(enemy.NetworkId()) == PeelThreatId &&
            PeelThreatUntil >= Now()) score += 350.0f;
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    return best;
}

inline SDK::HitChance RequiredQHitchance(QPurpose purpose,
                                         const AIBaseClient& target) {
    if (purpose == QPurpose::Peel || purpose == QPurpose::Objective ||
        (target.IsValid() && (target.IsDashing() ||
         Engine::IsHardCrowdControlled(target)))) {
        return SDK::HitChance::High;
    }
    SDK::HitChance baseChance = SDK::HitChance::VeryHigh;
    switch (List(QMenu, "QHitchance", 2)) {
    case 0: baseChance = SDK::HitChance::Medium; break;
    case 1: baseChance = SDK::HitChance::High; break;
    case 3: baseChance = SDK::HitChance::Immobile; break;
    default: baseChance = SDK::HitChance::VeryHigh; break;
    }
    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo && baseChance != SDK::HitChance::Immobile) {
        if (baseChance == SDK::HitChance::VeryHigh) baseChance = SDK::HitChance::High;
        else if (baseChance == SDK::HitChance::High) baseChance = SDK::HitChance::Medium;
    }
    return baseChance;
}

inline float HitchanceConfidence(SDK::HitChance chance) {
    switch (chance) {
    case SDK::HitChance::Immobile: return 1.0f;
    case SDK::HitChance::VeryHigh: return 0.93f;
    case SDK::HitChance::High: return 0.82f;
    case SDK::HitChance::Medium: return 0.65f;
    default: return 0.30f;
    }
}

inline std::vector<Vector3> QCandidates(const AIBaseClient& target,
                                        QForm form,
                                        const std::vector<QBody>& bodies,
                                        SDK::HitChance& observed) {
    std::vector<Vector3> result;
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) return result;
    Vector3 predicted{};
    observed = SDK::HitChance::VeryHigh;
    if (target.IsHero() && Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        predicted = prediction.GetCastPosition();
        observed = prediction.Hitchance;
    }
    if (!predicted.IsValid() || predicted.IsZero()) {
        const float travel = kQCastSeconds + QProjectileTravelSeconds(
            player.Position().Distance2D(target.Position()),
            form == QForm::Boulder);
        predicted = PredictPosition(target, travel);
    }
    const Vector3 direct = SharedGeometry::Direction2D(
        player.Position(), predicted);
    if (direct.IsZero()) return result;
    static constexpr std::array<float, 13> offsets = {
        0.0f, 0.008f, -0.008f, 0.016f, -0.016f,
        0.028f, -0.028f, 0.043f, -0.043f,
        0.061f, -0.061f, 0.082f, -0.082f,
    };
    for (float offset : offsets) {
        const Vector3 direction = SharedGeometry::Rotate2D(direct, offset);
        if (!direction.IsZero()) {
            result.push_back(player.Position() + direction * kQRange);
        }
    }
    const float splash = form == QForm::Boulder
        ? kQBigAoeRadius : kQNormalAoeRadius;
    for (const QBody& body : bodies) {
        if (body.Id == static_cast<int>(target.NetworkId()) ||
            !ValidQBody(body) ||
            player.Position().Distance2D(body.Position) > kQRange +
                body.Radius) continue;
        const float nearby = body.Position.Distance2D(target.Position());
        if (nearby > splash + body.Radius + target.BoundingRadius() + 80.0f) {
            continue;
        }
        const Vector3 bridge = SharedGeometry::Direction2D(
            player.Position(), body.Position);
        if (!bridge.IsZero()) {
            result.push_back(player.Position() + bridge * kQRange);
        }
    }
    return result;
}

inline QPlan BuildQPlan(const AIBaseClient& intended,
                        QPurpose purpose,
                        bool reactive = false,
                        bool forceBoulderSetup = false) {
    QPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Ready(0) || !intended.IsValid() ||
        !ValidHostileUnitInGameplayRange(intended, kQRange + 30.0f)) {
        return best;
    }
    const QForm form = CurrentQForm();
    const bool bigRock = form == QForm::Boulder;
    std::vector<QBody> bodies = BuildQBodies();
    SDK::HitChance observed = SDK::HitChance::None;
    const std::vector<Vector3> candidates = QCandidates(
        intended, form, bodies, observed);
    const SDK::HitChance required = RequiredQHitchance(purpose, intended);
    const bool fixedTarget = !intended.IsHero();
    const bool allowAoeBridge = Bool(QMenu, "AoeBridge", true) ||
        purpose == QPurpose::Wave || purpose == QPurpose::LastHit ||
        purpose == QPurpose::Jungle || purpose == QPurpose::Objective;
    const bool highConfidence = fixedTarget || intended.IsDashing() ||
        Engine::IsHardCrowdControlled(intended) ||
        static_cast<int>(observed) >= static_cast<int>(required);
    const int intendedId = static_cast<int>(intended.NetworkId());
    const QBody* intendedBody = FindBody(bodies, intendedId);
    if (!intendedBody) return best;

    for (const Vector3& aim : candidates) {
        if (!aim.IsValid() || aim.IsZero() ||
            ProjectileWallBlocks(player.Position(), aim, kQMissileRadius)) {
            continue;
        }
        QContact contact{};
        if (!QHitsIntended(player.Position(), aim, bodies, intendedId,
                           bigRock, allowAoeBridge, &contact)) {
            continue;
        }
        const bool bridge = contact.BodyId != intendedId;
        const std::vector<int> victims = QSplashVictimIds(
            contact, bodies, bigRock);
        const float dealt = QDamage(intended, form, purpose);
        const bool denied = intended.IsHero() &&
            (HasSpellShieldOrImmunity(intended) ||
             IsCommonUntargetableOrImmune(intended));
        const bool lethal = !denied &&
            dealt >= intended.Health() + intended.AllShield() + 2.0f;
        const bool monster = intended.IsMinion() &&
            Core::Objects::IsJungleMonster(intended.Address());

        QContext context{};
        context.Ready = true;
        context.HasMana = player.Mana() + 0.5f >= CurrentQCost(form);
        context.TargetValid = context.InRange = true;
        context.CleanContact = !bridge;
        context.AoeBridge = bridge;
        context.ProjectileWallBlocked = false;
        context.TargetSpellShield = intended.IsHero() &&
            HasSpellShieldOrImmunity(intended);
        context.TargetImmune = intended.IsHero() &&
            IsCommonUntargetableOrImmune(intended);
        context.HighConfidence = highConfidence;
        context.TargetImmobile = Engine::IsHardCrowdControlled(intended);
        context.TargetDashing = intended.IsDashing();
        context.Lethal = lethal;
        context.Reactive = reactive;
        context.PlayerAttackWindingUp = Orbwalker::IsWindingUp();
        context.ComboFollowupReady = Ready(1) && Ready(2);
        context.FullVolleyPreferred = monster && bigRock &&
            intended.Health() > dealt + 5.0f &&
            purpose != QPurpose::BoulderSetup;
        context.PreserveGroundForSetup = !bigRock &&
            Bool(QMenu, "ManageGround", true);
        context.CursorAgrees = purpose == QPurpose::Wave ||
            purpose == QPurpose::Jungle || purpose == QPurpose::Objective ||
            CursorDirectionAgrees(aim, -0.18f) ||
            (Orbwalker::ActiveMode() == OrbwalkingMode::Combo);
        context.Form = form;
        context.Purpose = forceBoulderSetup
            ? QPurpose::BoulderSetup : purpose;
        context.AoeVictims = std::max(1, static_cast<int>(victims.size()));
        context.CollisionConfidence = fixedTarget
            ? 1.0f : HitchanceConfidence(observed);
        CastEvaluation evaluation = EvaluateQ(context);
        if (!evaluation.Cast) continue;
        float score = evaluation.Score -
            contact.ProjectileSeconds * 25.0f;
        if (bridge && intended.IsHero()) score += 115.0f;
        if (purpose == QPurpose::LastHit) {
            score += std::max(0.0f, dealt - intended.Health()) * -0.025f;
        }
        if (!best.Valid || score > best.Evaluation.Score) {
            evaluation.Score = score;
            best.CastPosition = aim;
            best.FirstContact = contact;
            best.Evaluation = evaluation;
            best.SplashVictims = victims;
            best.IntendedId = intendedId;
            best.Form = form;
            best.Purpose = context.Purpose;
            best.Hitchance = fixedTarget
                ? SDK::HitChance::VeryHigh : observed;
            best.RawDamage = ExpectedQRawDamage(
                intended, form, purpose);
            best.DealtDamage = dealt;
            best.AoeBridge = bridge;
            best.Lethal = lethal;
            best.Valid = true;
        }
    }
    LastQPlan = best;
    return best;
}

inline void RegisterQCast(QForm form, int targetId) {
    const int now = Now();
    if (now - LastRegisteredCastTick[0] <= 75) return;
    LastRegisteredCastTick[0] = now;
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    const float seconds = static_cast<float>(now) / 1000.0f;
    const QCastTransition transition = ApplyQCastToWorkedGround(
        GroundZones, player.Position(), player.BoundingRadius(),
        seconds, NextGroundZoneId);
    if (transition.CreatedZoneId != 0) ++NextGroundZoneId;
    LastQForm = form;
    // The pre-cast ledger is authoritative.  The explicit form argument is
    // retained for missile reconciliation and manual-cast telemetry.
    if (transition.Form != form) LastQForm = transition.Form;
    LastQCastTick = now;
    LastQTargetId = targetId;
    if (LastQForm == QForm::Boulder && targetId != 0) {
        float elapsed = kQCastSeconds;
        const AIBaseClient target = UnitByNetworkId(targetId);
        if (target.IsValid()) {
            const float distance = player.Position().Distance2D(
                target.Position());
            elapsed += QProjectileTravelSeconds(
                std::max(0.0f, distance - target.BoundingRadius() -
                    kQMissileRadius), true);
        }
        RegisterBoulderSlowWindow(targetId, elapsed);
    }
}

inline bool CastQPlan(const QPlan& plan, bool reactive = false) {
    if (!plan.Valid || !Ready(0)) return false;
    if (!ControllerHelpers::CastThrottleReady(0, 30, reactive ? 0 : -1)) {
        return false;
    }
    if (!Engine::ControllerCastPosition(0, plan.CastPosition)) return false;
    LastQPlan = plan;
    RegisterQCast(plan.Form, plan.IntendedId);
    if (plan.Form == QForm::Boulder && plan.FirstContact.Hit) {
        RegisterBoulderSlowWindow(
            plan.IntendedId, plan.FirstContact.CastElapsedSeconds);
    }
    return true;
}

inline bool FieldActive() {
    return ActiveMinefield.Valid && ActiveMinefieldExpireTick > Now();
}

inline void RegisterECast(const Vector3& castPosition, int targetId) {
    const int now = Now();
    if (now - LastRegisteredCastTick[2] <= 75) return;
    LastRegisteredCastTick[2] = now;
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    ActiveMinefield = BuildMinefield(
        player.Position(), castPosition,
        static_cast<float>(now) / 1000.0f);
    ActiveMinefieldExpireTick = now + static_cast<int>(
        (kECastSeconds + (kERows - 1) * kEDelayBetweenRows +
         kEMineLifetimeSeconds) * 1000.0f);
    LastECastTick = now;
    (void)targetId;
}

inline int MinefieldInitialHits(const Minefield& field) {
    if (!field.Valid) return 0;
    int hits = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1200.0f)) continue;
        const Vector3 predicted = PredictPosition(enemy, 0.45f);
        if (PointInMinefieldEnvelope(
                field, predicted, enemy.BoundingRadius())) ++hits;
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!minion.IsValid() || minion.IsDead()) continue;
        if (PointInMinefieldEnvelope(
                field, minion.Position(), minion.BoundingRadius())) ++hits;
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (!monster.IsValid() || monster.IsDead()) continue;
        if (PointInMinefieldEnvelope(
                field, monster.Position(), monster.BoundingRadius())) ++hits;
    }
    return hits;
}

inline int BestPotentialWContacts(const Minefield& field,
                                  const AIBaseClient& target,
                                  float movementStartSeconds) {
    if (!field.Valid || !target.IsValid()) return 0;
    const auto player = ObjectManager::Player();
    const Vector3 center = PredictPosition(target, kWImpactSeconds);
    std::array<Vector3, 6> directions = {
        SharedGeometry::Direction2D(center, player.Position()),
        SharedGeometry::Direction2D(player.Position(), center),
        field.Direction,
        field.Direction * -1.0f,
        Vector3{ -field.Direction.z, 0.0f, field.Direction.x },
        Vector3{ field.Direction.z, 0.0f, -field.Direction.x },
    };
    int best = 0;
    for (const Vector3& direction : directions) {
        const Vector3 destination = WDestination(center, direction);
        if (destination.IsZero()) continue;
        best = std::max(best, CountMineContacts(
            field, center, destination, target.BoundingRadius(),
            movementStartSeconds, kWDisplacementSeconds).Contacts);
    }
    return best;
}

inline EPlan BuildEPlan(const AIBaseClient& target,
                        EPurpose purpose,
                        bool reactive = false,
                        bool afterW = false) {
    EPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Ready(2) || !target.IsValid() ||
        !ValidHostileUnitInGameplayRange(target, kERange + 80.0f)) {
        return best;
    }
    std::vector<Vector3> candidates;
    const Vector3 predicted = PredictPosition(target,
        reactive && target.IsDashing() ? 0.12f : 0.38f);
    const bool lockPlannedWField = afterW && LastWPlan.Valid &&
        LastWPlan.UsesPlannedE && LastWPlan.PlannedMinefield.Valid &&
        LastWPlan.TargetId == static_cast<int>(target.NetworkId()) &&
        Now() - LastWCastTick <= 650;
    if (lockPlannedWField) {
        candidates.push_back(player.Position() +
            LastWPlan.PlannedMinefield.Direction * kERange);
    } else {
        if (predicted.IsValid() && !predicted.IsZero()) {
            candidates.push_back(predicted);
        }
        candidates.push_back(target.Position());
        if (target.IsDashing() && target.PathEnd().IsValid()) {
            candidates.push_back(target.PathEnd());
        }
        if (GapcloserTargetId == static_cast<int>(target.NetworkId()) &&
            GapcloserExpireTick >= Now() && GapcloserEndpoint.IsValid()) {
            candidates.push_back(GapcloserEndpoint);
        }
    }
    static constexpr std::array<float, 7> offsets = {
        0.0f, 0.06f, -0.06f, 0.12f, -0.12f, 0.20f, -0.20f,
    };
    const Vector3 baseDirection = SharedGeometry::Direction2D(
        player.Position(), predicted.IsValid() ? predicted : target.Position());
    for (float offset : offsets) {
        if (lockPlannedWField) break;
        const Vector3 direction = SharedGeometry::Rotate2D(baseDirection, offset);
        if (!direction.IsZero()) {
            candidates.push_back(player.Position() + direction *
                std::min(kERange, player.Position().Distance2D(target.Position()) + 90.0f));
        }
    }

    const float nowSeconds = static_cast<float>(Now()) / 1000.0f;
    for (Vector3 aim : candidates) {
        if (!aim.IsValid() || aim.IsZero()) continue;
        const float distance = player.Position().Distance2D(aim);
        if (distance > kERange) {
            aim = player.Position() +
                SharedGeometry::Direction2D(player.Position(), aim) * kERange;
        }
        Minefield field = BuildMinefield(player.Position(), aim, nowSeconds);
        if (!field.Valid ||
            !PointInMinefieldEnvelope(
                field, predicted, target.BoundingRadius())) continue;
        const int initialHits = MinefieldInitialHits(field);
        const float wMovementStart = nowSeconds +
            (afterW ? std::max(0.15f, kWImpactSeconds - kECastSeconds)
                    : kECastSeconds + kWImpactSeconds);
        const int contacts = BestPotentialWContacts(
            field, target, wMovementStart);
        const bool holdForChampion =
            (purpose == EPurpose::Wave || purpose == EPurpose::Jungle) &&
            Bool(FarmMenu, "HoldEForChampion", true) &&
            HasEnemyChampionNear(static_cast<float>(
                Slider(FarmMenu, "ChampionHoldRange", 1050)));

        EContext context{};
        context.Ready = true;
        context.HasMana = player.Mana() + 0.5f >= SpellCost(2);
        context.TargetValid = context.InRange = true;
        context.CastPositionValid = true;
        context.TargetSpellShield = target.IsHero() &&
            HasSpellShieldOrImmunity(target);
        context.TargetImmune = target.IsHero() &&
            IsCommonUntargetableOrImmune(target);
        context.TargetDashing = target.IsDashing();
        context.TargetHasReadyDash = target.IsHero() &&
            ChampionMobilityReady(AIHeroClient(target.Address()));
        context.TargetCommitted = target.IsHero() &&
            TargetCommitted(static_cast<int>(target.NetworkId()));
        // Once W has been issued, its cooldown is expected.  `afterW` means
        // that the displacement conversion is already committed, not absent.
        context.WReady = Ready(1) || afterW;
        context.QReady = Ready(0);
        context.WWillCrossMines = contacts > 0;
        context.ChokePoint = ControllerHelpers::NearTerrain(aim, 180.0f) ||
            purpose == EPurpose::Objective;
        context.Reactive = reactive;
        context.HoldForChampion = holdForChampion;
        context.ExpectedInitialHits = std::max(1, initialHits);
        context.ExpectedMineContacts = contacts;
        context.NearbyEnemies = target.IsHero()
            ? Engine::CountEnemiesAt(target.Position(), 650.0f) : 1;
        context.ManaPercent = PlayerManaPercent();
        context.Purpose = purpose;
        CastEvaluation evaluation = EvaluateE(context);
        if (!evaluation.Cast) continue;
        float score = evaluation.Score -
            player.Position().Distance2D(aim) * 0.015f;
        if (purpose == EPurpose::ComboSetup && contacts >= 2) score += 110.0f;
        if (!best.Valid || score > best.Evaluation.Score) {
            evaluation.Score = score;
            best.CastPosition = aim;
            best.Field = field;
            best.Evaluation = evaluation;
            best.TargetId = static_cast<int>(target.NetworkId());
            best.InitialHits = initialHits;
            best.ExpectedMineContacts = contacts;
            best.Purpose = purpose;
            best.Valid = true;
        }
    }
    LastEPlan = best;
    return best;
}

inline bool CastEPlan(const EPlan& plan, bool reactive = false) {
    if (!plan.Valid || !Ready(2)) return false;
    const bool fastFollowup = Now() - LastWCastTick <= 420;
    if (!ControllerHelpers::CastThrottleReady(
            2, 30, (reactive || fastFollowup) ? 0 : -1)) return false;
    if (!Engine::ControllerCastPosition(2, plan.CastPosition)) return false;
    LastEPlan = plan;
    RegisterECast(plan.CastPosition, plan.TargetId);
    return true;
}

inline Vector3 NearestAlliedTurretDirection(const Vector3& center) {
    Vector3 best{};
    float bestDistance = FLT_MAX;
    for (const auto& turret : GameObjects::AllyTurrets()) {
        if (!turret.IsValid() || turret.IsDead()) continue;
        const float distance = center.Distance2D(turret.Position());
        if (distance < bestDistance && distance <= 1800.0f) {
            bestDistance = distance;
            best = SharedGeometry::Direction2D(center, turret.Position());
        }
    }
    return best;
}

inline Vector3 AlliedCentroidDirection(const Vector3& center) {
    Vector3 sum{};
    int count = 0;
    const auto player = ObjectManager::Player();
    if (player.IsValid()) {
        sum = sum + player.Position();
        ++count;
    }
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally) || center.Distance2D(ally.Position()) > 1200.0f) {
            continue;
        }
        sum = sum + ally.Position();
        ++count;
    }
    if (count <= 0) return {};
    return SharedGeometry::Direction2D(
        center, sum / static_cast<float>(count));
}

inline WPlan BuildWPlan(const AIBaseClient& target,
                        WPurpose purpose,
                        bool reactive = false,
                        bool planEAfter = false,
                        const Vector3& forcedAwayFrom = {}) {
    WPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Ready(1) || !target.IsValid() ||
        !ValidHostileUnitInGameplayRange(target, kWRange + 80.0f)) {
        return best;
    }
    SDK::HitChance observed = SDK::HitChance::VeryHigh;
    Vector3 center = PredictPosition(target, kWImpactSeconds);
    if (target.IsHero() && Engine::RuntimeSpells[1]) {
        const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(target);
        if (prediction.GetCastPosition().IsValid() &&
            !prediction.GetCastPosition().IsZero()) {
            center = prediction.GetCastPosition();
            observed = prediction.Hitchance;
        }
    }
    if (!center.IsValid() || center.IsZero() ||
        player.Position().Distance2D(center) > kWRange +
            target.BoundingRadius()) return best;

    std::vector<Vector3> directions;
    directions.reserve(16);
    directions.push_back(SharedGeometry::Direction2D(center, player.Position()));
    directions.push_back(SharedGeometry::Direction2D(player.Position(), center));
    directions.push_back(SharedGeometry::Direction2D(center, Game::CursorPos()));
    directions.push_back(NearestAlliedTurretDirection(center));
    directions.push_back(AlliedCentroidDirection(center));
    if (forcedAwayFrom.IsValid() && !forcedAwayFrom.IsZero()) {
        directions.push_back(SharedGeometry::Direction2D(forcedAwayFrom, center));
    }
    const AIHeroClient protectedAlly = ProtectedAlly();
    if (protectedAlly.IsValid()) {
        directions.push_back(SharedGeometry::Direction2D(
            protectedAlly.Position(), center));
    }

    Minefield field{};
    bool usesPlanned = false;
    const float nowSeconds = static_cast<float>(Now()) / 1000.0f;
    if (FieldActive()) {
        field = ActiveMinefield;
        directions.push_back(field.Direction);
        directions.push_back(field.Direction * -1.0f);
        directions.push_back(Vector3{ -field.Direction.z, 0.0f,
                                      field.Direction.x });
        directions.push_back(Vector3{ field.Direction.z, 0.0f,
                                     -field.Direction.x });
    } else if (planEAfter && Ready(2)) {
        // W starts now; E is queued after W's 0.25 cast.  The field therefore
        // has only its early rows when W erupts at 0.75 seconds.
        field = BuildMinefield(
            player.Position(), center,
            nowSeconds + kWCastSeconds);
        usesPlanned = field.Valid;
        if (field.Valid) {
            directions.push_back(field.Direction);
            directions.push_back(field.Direction * -1.0f);
        }
    }

    for (Vector3 direction : directions) {
        direction.y = 0.0f;
        if (direction.IsZero()) continue;
        const Vector3 destination = WDestination(center, direction);
        if (!destination.IsValid() || destination.IsZero()) continue;
        MineContactSummary contacts{};
        if (field.Valid) {
            contacts = CountMineContacts(
                field, center, destination, target.BoundingRadius(),
                nowSeconds + kWImpactSeconds,
                kWDisplacementSeconds);
        }
        const bool peel = purpose == WPurpose::PeelPlayer ||
            purpose == WPurpose::PeelAlly || purpose == WPurpose::Gapcloser;
        Vector3 peelAnchor = forcedAwayFrom;
        if (!peelAnchor.IsValid() || peelAnchor.IsZero()) {
            peelAnchor = purpose == WPurpose::PeelAlly && protectedAlly.IsValid()
                ? protectedAlly.Position() : player.Position();
        }
        const bool improvesPeel = !peel ||
            destination.Distance2D(peelAnchor) >
                center.Distance2D(peelAnchor) + 100.0f;
        const bool towardEnemySafety =
            Engine::UnderEnemyTurret(destination) &&
            !Engine::UnderEnemyTurret(center);
        const bool towardCarry = protectedAlly.IsValid() && !peel &&
            destination.Distance2D(protectedAlly.Position()) + 80.0f <
                center.Distance2D(protectedAlly.Position()) &&
            destination.Distance2D(protectedAlly.Position()) < 380.0f;
        const bool denied = target.IsHero() &&
            (HasSpellShieldOrImmunity(target) ||
             IsCommonUntargetableOrImmune(target));
        const float comboDamage = EComboDamage(target, contacts.Contacts) +
            (Ready(0) ? QDamage(target, CurrentQForm(), QPurpose::Combo) : 0.0f);
        const bool lethal = !denied && comboDamage >=
            target.Health() + target.AllShield() + 5.0f;

        WContext context{};
        context.Ready = true;
        context.HasMana = player.Mana() + 0.5f >= SpellCost(1);
        context.TargetValid = context.InRange = true;
        context.CenterValid = context.DirectionValid = true;
        context.HighConfidence = !target.IsHero() ||
            static_cast<int>(observed) >=
                static_cast<int>(SDK::HitChance::VeryHigh);
        context.TargetImmobile = Engine::IsHardCrowdControlled(target);
        context.TargetSlowedByBoulder = TargetBoulderSlowedAt(
            static_cast<int>(target.NetworkId()),
            Now() + static_cast<int>(kWImpactSeconds * 1000.0f));
        context.TargetCommitted = !target.IsHero() || TargetCommitted(
            static_cast<int>(target.NetworkId()));
        context.TargetMobilitySpent = !target.IsHero() || MobilitySpent(
            static_cast<int>(target.NetworkId()));
        context.TargetSpellShield = target.IsHero() &&
            HasSpellShieldOrImmunity(target);
        context.TargetImmune = target.IsHero() &&
            IsCommonUntargetableOrImmune(target);
        // Thin-wall displacements are a real Taliyah technique.  Do not map a
        // wall endpoint to invalid automatically; runtime confidence and the
        // target capsule still gate the cast.
        context.DestinationTerrain = false;
        context.PushesTowardEnemySafety = towardEnemySafety;
        context.PushesThreatTowardCarry = towardCarry;
        context.ImprovesPeelDistance = improvesPeel;
        context.CursorAgrees = peel ||
            CursorDirectionAgrees(destination, -0.15f) ||
            (Orbwalker::ActiveMode() == OrbwalkingMode::Combo);
        context.Reactive = reactive;
        context.LethalCombo = lethal;
        context.MineContacts = contacts.Contacts;
        context.AlliedFollowup = CountAlliedFollowup(destination, 650.0f);
        context.EnemiesNearDestination = Engine::CountEnemiesAt(
            destination, 600.0f);
        context.Distance = player.Position().Distance2D(center);
        context.Purpose = purpose;
        CastEvaluation evaluation = EvaluateW(context);
        if (!evaluation.Cast) continue;
        float score = evaluation.Score;
        if (direction.Dot(SharedGeometry::Direction2D(
                center, player.Position())) > 0.70f && !peel) {
            score += 65.0f;
        }
        if (!best.Valid || score > best.Evaluation.Score) {
            evaluation.Score = score;
            best.Center = center;
            best.DirectionEnd = center + direction * kWThrowDistance;
            best.Destination = destination;
            best.MineContacts = contacts;
            best.PlannedMinefield = field;
            best.Evaluation = evaluation;
            best.TargetId = static_cast<int>(target.NetworkId());
            best.Purpose = purpose;
            best.UsesPlannedE = usesPlanned;
            best.Valid = true;
        }
    }
    LastWPlan = best;
    return best;
}

inline void RegisterWCast(const WPlan& plan) {
    const int now = Now();
    if (now - LastRegisteredCastTick[1] <= 75) return;
    LastRegisteredCastTick[1] = now;
    LastWCastTick = now;
    LastWTargetId = plan.TargetId;
    LastWCenter = plan.Center;
    LastWDirectionEnd = plan.DirectionEnd;
}

inline bool CastWPlan(const WPlan& plan, bool reactive = false) {
    if (!plan.Valid || !Ready(1)) return false;
    const bool fastFollowup = Now() - LastECastTick <= 450 ||
                              TargetBoulderSlowed(plan.TargetId);
    if (!ControllerHelpers::CastThrottleReady(
            1, 30, (reactive || fastFollowup) ? 0 : -1)) return false;
    if (!Engine::ControllerCastVector(
            1, plan.Center, plan.DirectionEnd)) return false;
    LastWPlan = plan;
    RegisterWCast(plan);
    return true;
}

inline bool SafeToCommit(const AIHeroClient& target, bool lethal) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, 1100.0f)) return false;
    if (IsCommonUntargetableOrImmune(target) ||
        HasSpellShieldOrImmunity(target)) return false;
    if (Engine::UnderEnemyTurret(player.Position()) && !lethal) return false;
    const int nearby = Engine::CountEnemiesAt(player.Position(), 750.0f);
    if (nearby > (lethal ? 3 : 2) && player.HealthPercent() < 65.0f) {
        return false;
    }
    return true;
}

inline bool PreserveWForProtectedCarry(const AIHeroClient& offensiveTarget) {
    if (!Bool(PeelMenu, "ProtectCarry", true) || !Ready(1)) return false;
    const AIHeroClient ally = ProtectedAlly();
    if (!ally.IsValid()) return false;
    const AIHeroClient threat = SelectPeelThreat(ally);
    if (!threat.IsValid()) return false;
    if (offensiveTarget.IsValid() &&
        threat.NetworkId() == offensiveTarget.NetworkId()) return false;
    return threat.Position().Distance2D(ally.Position()) <=
        static_cast<float>(Slider(PeelMenu, "PeelDistance", 560));
}

inline void ClearSequence() {
    ActiveSequence = Sequence::None;
    ActiveBranch = ComboBranch::None;
    SequenceStep = SequenceTargetId = SequenceExpireTick = 0;
    SequenceReactive = false;
}

inline void StartSequence(Sequence sequence,
                          ComboBranch branch,
                          int targetId,
                          bool reactive = false,
                          int startStep = 0) {
    ActiveSequence = sequence;
    ActiveBranch = branch;
    SequenceTargetId = targetId;
    SequenceStep = startStep;
    SequenceReactive = reactive;
    SequenceExpireTick = Now() +
        (reactive ? kReactiveLifetimeMs : kSequenceLifetimeMs);
}

inline bool TryActiveSequence() {
    if (ActiveSequence == Sequence::None) return false;
    if (Now() > SequenceExpireTick) {
        ClearSequence();
        return false;
    }
    const AIBaseClient target = UnitByNetworkId(SequenceTargetId);
    if (!target.IsValid() || target.IsDead() || !target.IsTargetable()) {
        ClearSequence();
        return false;
    }
    const BranchDefinition definition = DefinitionFor(ActiveBranch);
    if (definition.Count <= 0 || SequenceStep >= definition.Count) {
        ClearSequence();
        return false;
    }
    const int slot = definition.Slots[static_cast<std::size_t>(SequenceStep)];
    bool casted = false;
    if (slot == 0) {
        QPurpose purpose = ActiveBranch == ComboBranch::BoulderWEQ &&
                SequenceStep == 0
            ? QPurpose::BoulderSetup
            : (SequenceReactive ? QPurpose::Peel : QPurpose::Combo);
        const QPlan q = BuildQPlan(
            target, purpose, SequenceReactive,
            purpose == QPurpose::BoulderSetup);
        if (ActiveBranch == ComboBranch::BoulderWEQ &&
            SequenceStep == 0 && q.Valid && q.Form != QForm::Boulder) {
            ClearSequence();
            return false;
        }
        casted = CastQPlan(q, SequenceReactive);
    } else if (slot == 1) {
        WPurpose purpose = WPurpose::MinefieldCatch;
        if (ActiveBranch == ComboBranch::BoulderWEQ) {
            purpose = WPurpose::BigQCatch;
        } else if (ActiveBranch == ComboBranch::FastWEQ) {
            purpose = WPurpose::FastFollowup;
        } else if (ActiveBranch == ComboBranch::DashPunishEWQ ||
                   ActiveSequence == Sequence::ReactiveDash) {
            purpose = WPurpose::Gapcloser;
        } else if (ActiveSequence == Sequence::ReactiveFlee) {
            purpose = WPurpose::PeelPlayer;
        } else if (ActiveSequence == Sequence::ReactivePeel) {
            purpose = WPurpose::PeelAlly;
        }
        const AIHeroClient ally = ProtectedAlly();
        const Vector3 away = purpose == WPurpose::PeelAlly && ally.IsValid()
            ? ally.Position()
            : (purpose == WPurpose::PeelPlayer
                ? ObjectManager::Player().Position() : Vector3{});
        if (ActiveBranch == ComboBranch::BoulderWEQ &&
            SequenceStep == 1 &&
            BoulderSlowTargetId == SequenceTargetId &&
            Now() + static_cast<int>(kWImpactSeconds * 1000.0f) + 50 <
                BoulderSlowStartTick) {
            return false;
        }
        const bool plansEAfterW =
            ActiveBranch == ComboBranch::BoulderWEQ ||
            ActiveBranch == ComboBranch::FastWEQ;
        const WPlan w = BuildWPlan(
            target, purpose, SequenceReactive,
            plansEAfterW, away);
        casted = CastWPlan(w, SequenceReactive);
    } else if (slot == 2) {
        EPurpose purpose = ActiveBranch == ComboBranch::DashPunishEWQ
            ? EPurpose::DashPunish
            : (ActiveSequence == Sequence::ReactivePeel
                ? EPurpose::Peel : EPurpose::ComboSetup);
        const bool followsW =
            ActiveBranch == ComboBranch::BoulderWEQ ||
            ActiveBranch == ComboBranch::FastWEQ;
        const EPlan e = BuildEPlan(
            target, purpose, SequenceReactive,
            followsW);
        casted = CastEPlan(e, SequenceReactive);
    }
    if (casted) {
        ++SequenceStep;
        if (SequenceStep >= definition.Count) ClearSequence();
        return true;
    }
    return false;
}

inline ComboContext RuntimeComboContext(const AIHeroClient& target) {
    ComboContext context{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, 1100.0f)) return context;
    const QForm form = CurrentQForm();
    const bool qInRange = player.Position().Distance2D(target.Position()) <=
        kQRange + target.BoundingRadius();
    const bool wInRange = player.Position().Distance2D(target.Position()) <=
        kWRange + target.BoundingRadius();
    const bool eInRange = player.Position().Distance2D(target.Position()) <=
        kERange + target.BoundingRadius();
    const float qDamage = Ready(0) ? QDamage(target, form, QPurpose::Combo) : 0.0f;
    const float eDamage = Ready(2) ? EComboDamage(target, 2) : 0.0f;
    const bool lethal = qDamage + eDamage >=
        target.Health() + target.AllShield() + 5.0f;
    context.TargetValid = true;
    context.QReady = Ready(0) && qInRange;
    context.WReady = Ready(1) && wInRange;
    context.EReady = Ready(2) && eInRange;
    context.OnWorkedGround = form == QForm::Boulder &&
        Bool(ComboMenu, "BigQCatch", true);
    context.TargetDashing = target.IsDashing();
    context.TargetHasReadyDash = ChampionMobilityReady(target);
    context.TargetCommitted = TargetCommitted(
        static_cast<int>(target.NetworkId()));
    context.TargetImmobile = Engine::IsHardCrowdControlled(target);
    context.BoulderCanHit = qInRange;
    context.WCanHit = wInRange;
    context.ECanHit = eInRange;
    context.Safe = SafeToCommit(target, lethal);
    context.Lethal = lethal;
    context.FastFollowupWindow = context.TargetImmobile ||
        CountAlliedFollowup(target.Position(), 550.0f) >= 1;
    context.PreserveWForPeel = PreserveWForProtectedCarry(target);
    context.CurrentMana = player.Mana();
    context.Costs = LiveManaCosts();
    return context;
}

inline bool TryCombo(const AIHeroClient& selected) {
    const AIHeroClient target = PreferredEnemy(selected);
    if (!Engine::ValidEnemy(target, 1100.0f)) return false;
    const ComboContext context = RuntimeComboContext(target);
    const ComboBranch branch = ChooseComboBranch(context);
    if (branch == ComboBranch::None) return false;
    CurrentPosture = target.IsDashing()
        ? Posture::AntiDash
        : (branch == ComboBranch::QPoke || branch == ComboBranch::EQPoke
            ? Posture::Poke : Posture::Catch);
    StartSequence(Sequence::CombatBranch, branch,
                  static_cast<int>(target.NetworkId()));
    return TryActiveSequence();
}

inline bool TryHarass(const AIHeroClient& selected) {
    if (PlayerManaPercent() <
        static_cast<float>(Slider(ComboMenu, "HarassMana", 48))) return false;
    const AIHeroClient target = PreferredEnemy(selected);
    if (!Engine::ValidEnemy(target, kQRange + 80.0f)) return false;
    CurrentPosture = Posture::Poke;
    if (Ready(0)) {
        const QForm form = CurrentQForm();
        const bool setup = form == QForm::Boulder && Ready(1) && Ready(2) &&
            Bool(ComboMenu, "BigQCatch", true) &&
            !PreserveWForProtectedCarry(target);
        const QPlan q = BuildQPlan(
            target, setup ? QPurpose::BoulderSetup : QPurpose::Poke,
            false, setup);
        if (q.Valid && CastQPlan(q)) {
            if (setup) {
                StartSequence(
                    Sequence::CombatBranch, ComboBranch::BoulderWEQ,
                    static_cast<int>(target.NetworkId()), false, 1);
            }
            return true;
        }
    }
    if (Ready(2) && Ready(1) &&
        target.Position().Distance2D(ObjectManager::Player().Position()) < 720.0f &&
        (target.IsDashing() || TargetCommitted(static_cast<int>(target.NetworkId())))) {
        StartSequence(Sequence::CombatBranch,
                      ComboBranch::ControlledEWQ,
                      static_cast<int>(target.NetworkId()));
        return TryActiveSequence();
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& selected) {
    const AIHeroClient target = PreferredEnemy(selected, 1050.0f);
    if (!Engine::ValidEnemy(target, 1050.0f)) return false;
    CurrentPosture = Posture::Peel;
    if (Ready(2)) {
        const EPlan e = BuildEPlan(target, EPurpose::Peel, true);
        if (CastEPlan(e, true)) {
            StartSequence(Sequence::ReactiveFlee,
                          ComboBranch::ControlledEWQ,
                          static_cast<int>(target.NetworkId()), true, 1);
            return true;
        }
    }
    if (Ready(1)) {
        const WPlan w = BuildWPlan(
            target, WPurpose::PeelPlayer, true, false,
            ObjectManager::Player().Position());
        if (CastWPlan(w, true)) return true;
    }
    if (Ready(0)) {
        return CastQPlan(BuildQPlan(target, QPurpose::Peel, true), true);
    }
    return false;
}

inline bool TryInterrupt() {
    if (!Bool(PeelMenu, "Interrupt", true) ||
        InterruptTargetId == 0 || InterruptExpireTick < Now() ||
        !Ready(1)) return false;
    const AIHeroClient target = HeroByNetworkId(InterruptTargetId);
    if (!Engine::ValidEnemy(target, kWRange + 60.0f)) return false;
    if (InterruptExpireTick - Now() <
        static_cast<int>(kWImpactSeconds * 1000.0f) + 70) return false;
    const WPlan w = BuildWPlan(
        target, WPurpose::Interrupt, true, FieldActive());
    if (CastWPlan(w, true)) {
        CurrentPosture = Posture::AntiDash;
        InterruptTargetId = InterruptExpireTick = 0;
        return true;
    }
    return false;
}

inline bool TryGapcloser() {
    if (!Bool(PeelMenu, "AntiGapcloser", true) ||
        GapcloserTargetId == 0 || GapcloserExpireTick < Now()) return false;
    const AIHeroClient target = HeroByNetworkId(GapcloserTargetId);
    if (!Engine::ValidEnemy(target, 1100.0f)) return false;
    CurrentPosture = Posture::AntiDash;
    if (Ready(2)) {
        const EPlan e = BuildEPlan(target, EPurpose::DashPunish, true);
        if (CastEPlan(e, true)) {
            StartSequence(Sequence::ReactiveDash,
                          ComboBranch::DashPunishEWQ,
                          static_cast<int>(target.NetworkId()), true, 1);
            return true;
        }
    }
    if (Ready(1)) {
        const WPlan w = BuildWPlan(
            target, WPurpose::Gapcloser, true, false,
            ObjectManager::Player().Position());
        return CastWPlan(w, true);
    }
    return false;
}

inline bool TryPeel() {
    if (!Bool(PeelMenu, "ProtectCarry", true)) return false;
    const AIHeroClient ally = ProtectedAlly();
    if (!ally.IsValid()) return false;
    const AIHeroClient threat = SelectPeelThreat(ally);
    if (!Engine::ValidEnemy(threat, 1150.0f)) return false;
    const float distance = threat.Position().Distance2D(ally.Position());
    if (distance > static_cast<float>(
            Slider(PeelMenu, "PeelDistance", 560)) &&
        !threat.IsDashing()) return false;
    CurrentPosture = Posture::Peel;
    PeelThreatId = static_cast<int>(threat.NetworkId());
    PeelThreatUntil = Now() + 1200;
    if (Ready(2) &&
        (threat.IsDashing() || ChampionMobilityReady(threat))) {
        const EPlan e = BuildEPlan(threat, EPurpose::Peel, true);
        if (CastEPlan(e, true)) {
            StartSequence(Sequence::ReactivePeel,
                          ComboBranch::ControlledEWQ,
                          static_cast<int>(threat.NetworkId()), true, 1);
            return true;
        }
    }
    if (Ready(1)) {
        const WPlan w = BuildWPlan(
            threat, WPurpose::PeelAlly, true, false, ally.Position());
        return CastWPlan(w, true);
    }
    return false;
}

inline bool TryKillSecure(const AIHeroClient& selected) {
    if (!Bool(TacticsMenu, "KillSecure", true)) return false;
    std::vector<AIHeroClient> targets;
    if (Engine::ValidEnemy(selected, 1100.0f)) targets.push_back(selected);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1100.0f)) continue;
        if (selected.IsValid() && enemy.NetworkId() == selected.NetworkId()) continue;
        targets.push_back(enemy);
    }
    std::sort(targets.begin(), targets.end(),
        [](const AIHeroClient& left, const AIHeroClient& right) {
            return left.Health() + left.AllShield() <
                   right.Health() + right.AllShield();
        });
    for (const AIHeroClient& target : targets) {
        if (HasSpellShieldOrImmunity(target) ||
            IsCommonUntargetableOrImmune(target)) continue;
        if (Ready(0)) {
            const QPlan q = BuildQPlan(target, QPurpose::Kill, true);
            if (q.Valid && q.Lethal && CastQPlan(q, true)) return true;
        }
        if (Ready(2) && EInitialDamage(target) >=
            target.Health() + target.AllShield() + 2.0f) {
            const EPlan e = BuildEPlan(target, EPurpose::ComboSetup, true);
            if (CastEPlan(e, true)) return true;
        }
    }
    return false;
}

inline bool RIsRecast() {
    const auto player = ObjectManager::Player();
    const auto ultimate = player.IsValid()
        ? player.Spellbook().GetSpell(SDK::SpellSlot::R)
        : SDK::SpellDataInstClient{};
    return ControllerHelpers::RuntimeNameContains(3, "recast") ||
           ControllerHelpers::RuntimeNameContains(3, "ride") ||
           ControllerHelpers::RuntimeNameContains(3, "taliyahr2") ||
           ControllerHelpers::SpellInstanceContains(ultimate, "taliyahr2") ||
           ControllerHelpers::SpellInstanceContains(
               ultimate, "taliyahrrecast") ||
           ControllerHelpers::SpellInstanceContains(ultimate, "taliyahride");
}

inline bool AllyChanneling(const AIHeroClient& ally) {
    if (!ally.IsValid()) return false;
    return ControllerHelpers::HasAnyBuff(
        ally, { "katarinar", "nunur", "missfortunebullettime",
               "jhinr", "xerathlocusofpower2", "meditate",
               "velkozr", "fiddlestickswdrain", "lucianr" });
}

inline std::vector<WallUnit> BuildWallUnits() {
    std::vector<WallUnit> units;
    const auto player = ObjectManager::Player();
    if (player.IsValid()) {
        units.push_back({ player.Position(), true, false, true,
                          false, player.Spellbook().IsChanneling() });
    }
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally)) continue;
        units.push_back({ ally.Position(), true, false,
            static_cast<int>(ally.NetworkId()) == ProtectedAllyId,
            false, AllyChanneling(ally) });
    }
    AIHeroClient priority = PreferredEnemy({}, RRange(SpellRank(3)));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        units.push_back({ enemy.Position(), false, true, false,
            priority.IsValid() && priority.NetworkId() == enemy.NetworkId(),
            false });
    }
    return units;
}

inline AIBaseClient NearbyEpicObjective(float range) {
    const auto player = ObjectManager::Player();
    AIBaseClient best{};
    float bestDistance = FLT_MAX;
    for (const auto& monster : GameObjects::Jungle()) {
        if (!monster.IsValid() || monster.IsDead() ||
            !IsEpicMonster(monster)) continue;
        const float distance = player.Position().Distance2D(monster.Position());
        if (distance <= range && distance < bestDistance) {
            best = monster;
            bestDistance = distance;
        }
    }
    return best;
}

inline WallPlan BuildWallPlan(Mode mode) {
    WallPlan plan{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Ready(3) || RIsRecast()) return plan;
    const int rank = SpellRank(3);
    const float range = RRange(rank);
    if (range <= 0.0f) return plan;
    Vector3 cursor = Game::CursorPos();
    if (!cursor.IsValid() || cursor.IsZero()) return plan;
    Vector3 direction = SharedGeometry::Direction2D(player.Position(), cursor);
    if (direction.IsZero()) return plan;
    const float cursorDistance = player.Position().Distance2D(cursor);
    const float distance = std::clamp(
        cursorDistance, kRMinimumUsefulRange, range);
    const Vector3 endpoint = player.Position() + direction * distance;
    const std::vector<WallUnit> units = BuildWallUnits();
    const WallSplit split = AnalyzeWallSplit(
        player.Position(), endpoint, units);
    const AIBaseClient objective = NearbyEpicObjective(1800.0f);
    WallPurpose purpose = WallPurpose::Rotation;
    if (mode == Mode::Flee || player.HealthPercent() <=
        static_cast<float>(Slider(WallMenu, "EscapeHp", 24))) {
        purpose = WallPurpose::Escape;
    } else if (objective.IsValid() &&
               CountAlliedFollowup(objective.Position(), 900.0f) >= 2) {
        purpose = WallPurpose::Objective;
    } else if (Engine::CountEnemiesAt(endpoint, 900.0f) > 0 ||
               split.PriorityEnemySeparated) {
        purpose = WallPurpose::Cutoff;
    }

    bool objectivePartition = false;
    if (objective.IsValid()) {
        const float objectiveSide = SignedWallSide(
            player.Position(), endpoint, objective.Position());
        int alliesObjectiveSide = 0;
        int enemiesOtherSide = 0;
        for (const WallUnit& unit : units) {
            const float side = SignedWallSide(
                player.Position(), endpoint, unit.Position);
            if (unit.Ally && side * objectiveSide >= 0.0f) ++alliesObjectiveSide;
            if (unit.Enemy && side * objectiveSide < 0.0f) ++enemiesOtherSide;
        }
        objectivePartition = alliesObjectiveSide >= 2 && enemiesOtherSide >= 1;
    }
    bool escapePartition = false;
    if (purpose == WallPurpose::Escape) {
        const float playerSide = SignedWallSide(
            player.Position(), endpoint,
            player.Position() +
                Vector3{ -direction.z, 0.0f, direction.x } * 20.0f);
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!Engine::ValidEnemy(enemy, 1200.0f)) continue;
            const float side = SignedWallSide(
                player.Position(), endpoint, enemy.Position());
            if (side * playerSide < 0.0f) {
                escapePartition = true;
                break;
            }
        }
        // A collinear cursor escape is still useful when the wall physically
        // intersects a pursuer and knocks them aside.
        escapePartition = escapePartition || split.EnemiesKnockedAside > 0;
    }

    WallContext context{};
    context.Ready = true;
    context.HasMana = player.Mana() + 0.5f >= SpellCost(3);
    context.ManualAuthorized = Key(WallMenu, "ManualWall", false);
    context.OriginValid = player.Position().IsValid();
    context.EndpointValid = endpoint.IsValid() && !endpoint.IsZero();
    context.PlayerRecentlyDamaged = PredictedDamageLockoutUntil >= Now();
    context.PlayerImmobilized = PlayerMobilityLocked();
    context.InterruptThreat = IncomingCrowdControlUntil >= Now() ||
        HasReadyPointClickThreatAt(player.Position()) ||
        Engine::CountEnemiesAt(player.Position(), 650.0f) > 0;
    context.CursorAgrees = true;
    context.RouteNavigable = !SDK::NavMesh::IsWall(endpoint);
    context.ObjectiveSecuredSide = objectivePartition;
    context.EscapeSeparatesPursuers = escapePartition;
    context.Distance = distance;
    context.Split = split;
    context.Purpose = purpose;
    CastEvaluation evaluation = EvaluateWall(context);
    if (!evaluation.Cast && Bool(WallMenu, "UnsafeManual", false) &&
        context.ManualAuthorized && context.Ready && context.HasMana &&
        context.OriginValid && context.EndpointValid &&
        !context.PlayerImmobilized &&
        context.Split.ChannelingAlliesNearWall == 0) {
        evaluation = { true, 1.0f, "explicit unsafe override" };
    }
    if (!evaluation.Cast) return plan;
    plan.Endpoint = endpoint;
    plan.Context = context;
    plan.Evaluation = evaluation;
    plan.Purpose = purpose;
    plan.Valid = true;
    LastWallPlan = plan;
    return plan;
}

inline bool TryManualWall(Mode mode) {
    if (!WallMenu || !Key(WallMenu, "ManualWall", false)) return false;
    const WallPlan plan = BuildWallPlan(mode);
    if (!plan.Valid || !ControllerHelpers::CastThrottleReady(3, 80, 0)) {
        return false;
    }
    if (!Engine::ControllerCastPosition(3, plan.Endpoint)) return false;
    LastWallPlan = plan;
    LastRCastTick = Now();
    RChannelUntil = Now() + static_cast<int>(kRChannelSeconds * 1000.0f);
    CurrentPosture = Posture::Wall;
    return true;
}

inline bool TryQLastHit(bool jungle = false) {
    if (!Ready(0)) return false;
    const auto player = ObjectManager::Player();
    AIBaseClient best{};
    QPlan bestPlan{};
    float bestHealth = FLT_MAX;
    if (jungle) {
        for (const auto& monster : GameObjects::Jungle()) {
            if (!monster.IsValid() || monster.IsDead() ||
                !ValidHostileUnitInGameplayRange(monster, kQRange)) continue;
            const QPlan q = BuildQPlan(monster, QPurpose::Jungle);
            if (q.Valid && q.DealtDamage >= monster.Health() + 2.0f &&
                monster.Health() < bestHealth) {
                best = monster; bestPlan = q; bestHealth = monster.Health();
            }
        }
    } else {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!minion.IsValid() || minion.IsDead() ||
                !ValidHostileUnitInGameplayRange(minion, kQRange)) continue;
            const QPlan q = BuildQPlan(minion, QPurpose::LastHit);
            if (q.Valid && q.DealtDamage >= minion.Health() + 2.0f &&
                minion.Health() < bestHealth) {
                best = minion; bestPlan = q; bestHealth = minion.Health();
            }
        }
    }
    (void)best;
    return bestPlan.Valid && CastQPlan(bestPlan);
}

inline bool TryLaneFarm(Mode mode) {
    if (!Bool(FarmMenu, "Lane", true)) return false;
    CurrentPosture = Posture::Farm;
    if (mode == Mode::LastHit) return TryQLastHit(false);
    if (PlayerManaPercent() <
        static_cast<float>(Slider(FarmMenu, "LaneMana", 36))) {
        return TryQLastHit(false);
    }
    if (Ready(2) && Bool(FarmMenu, "UseE", true)) {
        EPlan best{};
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!minion.IsValid() || minion.IsDead()) continue;
            const EPlan e = BuildEPlan(minion, EPurpose::Wave);
            if (e.Valid && e.InitialHits >=
                Slider(FarmMenu, "MinimumEHits", 4) &&
                (!best.Valid || e.Evaluation.Score > best.Evaluation.Score)) {
                best = e;
            }
        }
        if (best.Valid && CastEPlan(best)) return true;
    }
    if (Ready(0)) {
        QPlan best{};
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!minion.IsValid() || minion.IsDead()) continue;
            const QPlan q = BuildQPlan(minion, QPurpose::Wave);
            if (!q.Valid) continue;
            const int hits = static_cast<int>(q.SplashVictims.size());
            if (hits < Slider(FarmMenu, "MinimumQHits", 3)) continue;
            if (!best.Valid || q.Evaluation.Score > best.Evaluation.Score) {
                best = q;
            }
        }
        if (best.Valid && CastQPlan(best)) return true;
    }
    return TryQLastHit(false);
}

inline bool TryJungleFarm() {
    if (!Bool(FarmMenu, "Jungle", true) ||
        PlayerManaPercent() <
            static_cast<float>(Slider(FarmMenu, "JungleMana", 22))) {
        return false;
    }
    const AIMinionClient target = SelectJungleTarget(1100.0f, true);
    if (!target.IsValid()) return false;
    CurrentPosture = IsEpicMonster(target)
        ? Posture::Objective : Posture::Farm;
    if (Ready(2) && Bool(FarmMenu, "JungleE", true) &&
        target.Health() > EInitialDamage(target) * 0.65f) {
        const EPlan e = BuildEPlan(
            target, IsEpicMonster(target)
                ? EPurpose::Objective : EPurpose::Jungle);
        if (e.Valid && CastEPlan(e)) return true;
    }
    if (Ready(0)) {
        QPlan q = BuildQPlan(
            target, IsEpicMonster(target)
                ? QPurpose::Objective : QPurpose::Jungle);
        if (q.Valid && q.Form == QForm::Boulder &&
            target.Health() > q.DealtDamage + 5.0f &&
            Bool(FarmMenu, "PreserveVolley", true)) {
            return false;
        }
        if (q.Valid && CastQPlan(q)) return true;
    }
    return false;
}

inline void RefreshRuntimeState() {
    const float nowSeconds = static_cast<float>(Now()) / 1000.0f;
    NormalizeWorkedGround(GroundZones, nowSeconds);
    if (ActiveMinefieldExpireTick <= Now()) {
        ActiveMinefield = {};
        ActiveMinefieldExpireTick = 0;
    }
    if (BoulderSlowUntil < Now()) {
        BoulderSlowTargetId = BoulderSlowStartTick = BoulderSlowUntil = 0;
    }
    if (GapcloserExpireTick < Now()) {
        GapcloserTargetId = GapcloserExpireTick = 0;
        GapcloserEndpoint = {};
    }
    if (InterruptExpireTick < Now()) {
        InterruptTargetId = InterruptExpireTick = 0;
    }
    if (PeelThreatUntil < Now()) {
        PeelThreatId = PeelThreatUntil = 0;
    }
    const auto player = ObjectManager::Player();
    if (player.IsValid()) {
        const AIHeroClient ally = SelectProtectionAlly(
            1600.0f, 0, true);
        ProtectedAllyId = ally.IsValid()
            ? static_cast<int>(ally.NetworkId()) : 0;
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshRuntimeState();
    CurrentPosture = Posture::Neutral;
    if (TryManualWall(mode)) return true;
    if (TryInterrupt() || TryGapcloser() || TryPeel()) return true;
    if (TryActiveSequence()) return true;
    if (PlayerOverrideUntil >= Now()) return false;
    if (TryKillSecure(selected)) return true;
    if (mode == Mode::Combo) return TryCombo(selected);
    if (mode == Mode::Harass) return TryHarass(selected);
    if (mode == Mode::Flee) return TryFlee(selected);
    if (mode == Mode::LaneClear || mode == Mode::LastHit) {
        if (ControllerHelpers::HasNearbyJungleTarget(1050.0f)) {
            return TryJungleFarm();
        }
        return TryLaneFarm(mode);
    }
    if (mode == Mode::Jungle) return TryJungleFarm();
    return false;
}

inline AIHeroClient NearestEnemyToPosition(const Vector3& position,
                                           float range) {
    AIHeroClient best{};
    float bestDistance = range;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        const float distance = enemy.Position().Distance2D(position);
        if (distance <= bestDistance) {
            best = enemy;
            bestDistance = distance;
        }
    }
    return best;
}

inline void ObserveEnemySpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid) return;
    const int id = static_cast<int>(analysis.Enemy.NetworkId());
    EnemyWindow* window = WindowFor(id, true);
    if (!window) return;
    if (analysis.Committed) {
        window->CommittedUntil = std::max(
            window->CommittedUntil, analysis.CommitmentUntilTick);
    }
    if (analysis.LikelyHardCrowdControl) {
        window->HardCrowdControlSpentUntil = std::max(
            window->HardCrowdControlSpentUntil, Now() + 1200);
        IncomingCrowdControlUntil = std::max(
            IncomingCrowdControlUntil, analysis.LineThreatUntilTick);
    }
    if (analysis.TargetsPlayer || analysis.CrossesPlayer) {
        PredictedDamageLockoutUntil = std::max(
            PredictedDamageLockoutUntil,
            Now() + static_cast<int>(kRDamageLockoutSeconds * 1000.0f));
    }
    const SDK::SpellSlot slot = args.Slot >= 0 && args.Slot < 4
        ? static_cast<SDK::SpellSlot>(args.Slot)
        : SDK::SpellSlot::Unknown;
    for (const auto& rule : MobilityRules) {
        if (ControllerHelpers::ChampionIs(analysis.Enemy, rule.Champion) &&
            slot == rule.Slot) {
            window->MobilitySpentUntil = std::max(
                window->MobilitySpentUntil, Now() + 1800);
        }
    }
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const int slot = EventSlot(args);
    if (slot < 0 || slot >= 4) return;
    const bool ours = Engine::WasControllerCast(slot, 720);
    const int targetId = EventTargetId(args);
    if (!ours) {
        ClearSequence();
        PlayerOverrideUntil = Now() +
            Slider(TacticsMenu, "ManualOwnershipMs", kManualOwnershipMs);
    }
    if (slot == 0) {
        const QForm form = CurrentQForm();
        int resolvedTarget = targetId;
        if (resolvedTarget == 0 && args.EndPosition.IsValid()) {
            const AIHeroClient nearest = NearestEnemyToPosition(
                args.EndPosition, 260.0f);
            if (nearest.IsValid()) {
                resolvedTarget = static_cast<int>(nearest.NetworkId());
            }
        }
        RegisterQCast(form, resolvedTarget);
        if (!ours && form == QForm::Boulder && resolvedTarget != 0 &&
            Bool(ComboMenu, "AssistManualBoulder", true)) {
            StartSequence(Sequence::ManualBoulderAssist,
                          ComboBranch::BoulderWEQ,
                          resolvedTarget, false, 1);
        }
    } else if (slot == 1) {
        WPlan observed{};
        observed.Center = args.StartPosition.IsValid() &&
                !args.StartPosition.IsZero()
            ? args.StartPosition : args.EndPosition;
        observed.DirectionEnd = args.EndPosition;
        const AIHeroClient nearest = NearestEnemyToPosition(
            observed.Center, kWRadius + 100.0f);
        observed.TargetId = nearest.IsValid()
            ? static_cast<int>(nearest.NetworkId()) : targetId;
        observed.Valid = observed.Center.IsValid() &&
            observed.DirectionEnd.IsValid();
        if (!ours && observed.Valid) {
            const auto player = ObjectManager::Player();
            if (player.IsValid()) {
                observed.PlannedMinefield = BuildMinefield(
                    player.Position(), observed.Center,
                    static_cast<float>(Now()) / 1000.0f);
                observed.UsesPlannedE = observed.PlannedMinefield.Valid;
            }
            LastWPlan = observed;
        }
        RegisterWCast(observed);
        if (!ours && observed.TargetId != 0 && Ready(2) &&
            Bool(ComboMenu, "AssistManualW", true)) {
            StartSequence(Sequence::ManualWAssist,
                          ComboBranch::FastWEQ,
                          observed.TargetId, false, 1);
        }
    } else if (slot == 2) {
        Vector3 aim = args.EndPosition;
        if (!aim.IsValid() || aim.IsZero()) aim = args.StartPosition;
        const AIHeroClient nearest = NearestEnemyToPosition(aim, 520.0f);
        const int resolved = nearest.IsValid()
            ? static_cast<int>(nearest.NetworkId()) : targetId;
        RegisterECast(aim, resolved);
        if (!ours && resolved != 0 && Ready(1) &&
            Bool(ComboMenu, "AssistManualE", true)) {
            StartSequence(Sequence::ManualEAssist,
                          ComboBranch::ControlledEWQ,
                          resolved, false, 1);
        }
    } else {
        LastRCastTick = Now();
        RChannelUntil = Now() + static_cast<int>(kRChannelSeconds * 1000.0f);
    }
}

inline void OnProcessSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) ObserveLocalSpell(args);
    else ObserveEnemySpell(args);
    (void)CaptureLocalAutoAttack(
        args, LastLocalAutoTargetId, LastLocalAutoTick);
}

inline void OnDoCast(
    const SDK::Events::ProcessSpellEventArgs& args) {
    (void)CaptureLocalAutoAttack(
        args, LastLocalAutoTargetId, LastLocalAutoTick);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureBeforeAttack(
        args, LastBeforeAttackTargetId, LastBeforeAttackTick);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(
        args, LastAfterAttackTargetId, LastAfterAttackTick);
}

inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (CaptureGapcloser(
            args, GapcloserTargetId, GapcloserEndpoint,
            GapcloserExpireTick, 1150.0f, 1450)) {
        EnemyWindow* window = WindowFor(GapcloserTargetId, true);
        if (window) {
            window->CommittedUntil = Now() + 1450;
            window->MobilitySpentUntil = Now() + 1800;
        }
    }
}

inline void OnInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(
        args, InterruptTargetId, InterruptExpireTick,
        1200, 250, 6500);
}

inline bool BuffContains(const SDK::Events::BuffEventArgs& args,
                         std::initializer_list<const char*> tokens) {
    return ControllerHelpers::TextContainsAny(args.BuffName, tokens);
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args,
                            bool removed) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (!IsLocalPlayer(args.Sender)) {
        if (BuffContains(args, {
                "taliyahqslow", "taliyahqmisbig", "taliyahslow" })) {
            if (removed) {
                if (BoulderSlowTargetId == id) {
                    BoulderSlowTargetId = BoulderSlowStartTick = 0;
                    BoulderSlowUntil = 0;
                }
            } else {
                BoulderSlowTargetId = id;
                BoulderSlowStartTick = Now();
                BoulderSlowUntil = ControllerHelpers::BuffExpireTick(
                    args, static_cast<int>(kQBigSlowSeconds * 1000.0f));
            }
        }
        return;
    }
    if (!removed && BuffContains(args, {
            "taliyahr", "taliyahriding", "taliyahrsuper" })) {
        RChannelUntil = ControllerHelpers::BuffExpireTick(args, 1800);
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    UpdateBuffState(args, false);
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    UpdateBuffState(args, false);
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    UpdateBuffState(args, true);
}

inline bool MissileNameContains(
    const SDK::Events::ObjectEventArgs& args,
    std::initializer_list<const char*> tokens) {
    return ControllerHelpers::AnyTextContains(
        { args.Sender.Name, args.Sender.CharacterName,
          args.SpellName, args.MissileName }, tokens);
}

inline void OnMissileCreate(
    const SDK::Events::ObjectEventArgs& args) {
    if (!ControllerHelpers::MissileEventIsLocal(args)) return;
    if (MissileNameContains(args, { "taliyahqmisbig" })) {
        LastQForm = QForm::Boulder;
    } else if (MissileNameContains(args, { "taliyahqmis" })) {
        LastQForm = QForm::Volley;
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::Poke: return "poke";
    case Posture::Catch: return "catch";
    case Posture::FrontToBack: return "front-to-back";
    case Posture::AntiDash: return "anti-dash";
    case Posture::Peel: return "peel";
    case Posture::Farm: return "farm";
    case Posture::Objective: return "objective";
    case Posture::Wall: return "manual wall";
    default: return "neutral";
    }
}

inline const char* BranchName(ComboBranch branch) {
    switch (branch) {
    case ComboBranch::BoulderWEQ: return "BigQ-W-E-Q";
    case ComboBranch::FastWEQ: return "W-E-Q";
    case ComboBranch::ControlledEWQ: return "E-W-Q";
    case ComboBranch::DashPunishEWQ: return "dash E-W-Q";
    case ComboBranch::EQPoke: return "E-Q";
    case ComboBranch::QPoke: return "Q";
    default: return "none";
    }
}

inline const char* SequenceName(Sequence sequence) {
    switch (sequence) {
    case Sequence::CombatBranch: return "combat";
    case Sequence::ReactiveDash: return "dash reaction";
    case Sequence::ReactivePeel: return "carry peel";
    case Sequence::ManualWAssist: return "manual W assist";
    case Sequence::ManualEAssist: return "manual E assist";
    case Sequence::ManualBoulderAssist: return "manual BigQ assist";
    case Sequence::JungleEW: return "jungle E-W";
    default: return "none";
    }
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    if (Bool(CoachMenu, "DrawRanges", true)) {
        Drawing::DrawCircle(player.Position(), kQRange,
                            0x6659C7E8u, 1.25f, 72);
        Drawing::DrawCircle(player.Position(), kWRange,
                            0x665F8FCBu, 1.15f, 60);
        Drawing::DrawCircle(player.Position(), kERange,
                            0x6659D89Bu, 1.10f, 64);
    }
    if (Bool(CoachMenu, "DrawGround", true)) {
        const float nowSeconds = static_cast<float>(Now()) / 1000.0f;
        for (const WorkedGroundZone& zone : GroundZones) {
            if (!ActiveWorkedGround(zone, nowSeconds)) continue;
            const bool current = zone.Center.Distance2D(player.Position()) <=
                kWorkedGroundRadius + player.BoundingRadius();
            Drawing::DrawCircle(
                zone.Center, kWorkedGroundRadius,
                current ? 0xCC65D6E8u : 0x886E8E9Eu,
                current ? 2.0f : 1.1f, 64);
            Vec2 screen{};
            if (Drawing::WorldToScreen(zone.Center, screen)) {
                char label[48]{};
                _snprintf_s(label, sizeof(label), _TRUNCATE,
                    "ground %.1f",
                    std::max(0.0f, zone.ExpiresAt - nowSeconds));
                Drawing::DrawText(screen.x - 34.0f, screen.y,
                                  0xFFD5EEF4u, label);
            }
        }
    }
    if (Bool(CoachMenu, "DrawMines", true) && FieldActive()) {
        const float nowSeconds = static_cast<float>(Now()) / 1000.0f;
        for (int index = 0; index < ActiveMinefield.Count; ++index) {
            const EMine& mine = ActiveMinefield.Mines[
                static_cast<std::size_t>(index)];
            const bool spawned = mine.SpawnAt <= nowSeconds &&
                                 mine.ExpiresAt > nowSeconds;
            Drawing::DrawCircle(
                mine.Position, 24.0f,
                spawned ? 0xBBCC9A62u : 0x557C6652u,
                spawned ? 1.5f : 0.8f, 16);
        }
    }
    if (Bool(CoachMenu, "DrawQ", true) && LastQPlan.Valid) {
        const std::uint32_t color = LastQPlan.Lethal
            ? 0xFFF4D35Eu
            : (LastQPlan.Form == QForm::Boulder
                ? 0xFFE3A45Fu : 0xFF62C9E8u);
        Drawing::DrawLine(
            player.Position(), LastQPlan.CastPosition, color, 2.0f);
        if (LastQPlan.FirstContact.BodyPosition.IsValid()) {
            Drawing::DrawCircle(
                LastQPlan.FirstContact.BodyPosition,
                LastQPlan.Form == QForm::Boulder
                    ? kQBigAoeRadius : kQNormalAoeRadius,
                color, 1.6f, 40);
        }
    }
    if (Bool(CoachMenu, "DrawW", true) && LastWPlan.Valid) {
        Drawing::DrawCircle(
            LastWPlan.Center, kWRadius, 0xCCB77CFFu, 1.8f, 40);
        Drawing::DrawLine(
            LastWPlan.Center, LastWPlan.Destination,
            0xFFCF9BFFu, 2.4f);
        Drawing::DrawCircle(
            LastWPlan.Destination, 46.0f, 0xFFCF9BFFu, 1.8f, 24);
    }
    if (Bool(CoachMenu, "DrawProtected", true)) {
        const AIHeroClient ally = ProtectedAlly();
        if (ally.IsValid()) {
            Drawing::DrawCircle(
                ally.Position(), ally.BoundingRadius() + 46.0f,
                0xAA71E7A5u, 1.5f, 32);
        }
    }
    if (Bool(CoachMenu, "DrawWall", true) && LastWallPlan.Valid) {
        Drawing::DrawLine(
            player.Position(), LastWallPlan.Endpoint,
            0xBBE0B47Au, 2.0f);
        Drawing::DrawCircle(
            LastWallPlan.Endpoint, 85.0f,
            0xCCE0B47Au, 1.8f, 32);
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char state[512]{};
            _snprintf_s(
                state, sizeof(state), _TRUNCATE,
                "Taliyah OTP | %s | %s %s step %d | Q %s | ground %zu | owner %s",
                PostureName(CurrentPosture), SequenceName(ActiveSequence),
                BranchName(ActiveBranch), SequenceStep,
                CurrentQForm() == QForm::Boulder ? "BIG" : "volley",
                GroundZones.size(),
                PlayerOverrideUntil >= Now() ? "player" : "controller");
            Drawing::DrawText(
                screen.x - 285.0f, screen.y - 112.0f,
                0xFFD9F4F7u, state);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "TaliyahOneTrick", "Taliyah one-trick stoneweaver"));
    TacticsMenu->Add(new MenuBool(
        "KillSecure", "Secure with conservative", true));
    TacticsMenu->Add(new MenuSlider(
        "ManualOwnershipMs", "Yield player spell (ms)",
        kManualOwnershipMs, 150, 1100));
    TacticsMenu->Add(new MenuSeparator(
        "Ownership",
        "Movement, attacks, Flash, R"));

    QMenu = TacticsMenu->AddSubMenu(new Menu(
        "ThreadedVolley", "Accelerating volley, boulder and Worked Ground"));
    QMenu->Add(new MenuList(
        "QHitchance", "Moving Q prediction",
        { "Medium", "High", "Very high", "Immobile only" }, 2));
    QMenu->Add(new MenuBool(
        "AoeBridge", "Use first-body", true));
    QMenu->Add(new MenuBool(
        "ManageGround", "Preserve Worked Ground", true));
    QMenu->Add(new MenuSeparator(
        "ProjectileTruth",
        "Volley Q"));

    ComboMenu = TacticsMenu->AddSubMenu(new Menu(
        "ComboRhythm", "Big Q, W-E surprise and E-W control branches"));
    ComboMenu->Add(new MenuBool(
        "BigQCatch", "Convert Big Q slow into W-E-Q", true));
    ComboMenu->Add(new MenuBool(
        "AssistManualBoulder", "W-E after player Q", true));
    ComboMenu->Add(new MenuBool(
        "AssistManualW", "Place E after a clean player W", true));
    ComboMenu->Add(new MenuBool(
        "AssistManualE", "W vector after player E", true));
    ComboMenu->Add(new MenuSlider(
        "HarassMana", "Min mana harass (%)", 48, 0, 100));
    ComboMenu->Add(new MenuSeparator(
        "OrderTruth",
        "W-E is faster but may miss"));

    MineMenu = TacticsMenu->AddSubMenu(new Menu(
        "UnraveledEarth", "Six timed rows, dash denial and shove contacts"));
    MineMenu->Add(new MenuSeparator(
        "LiveRows",
        "Track 22 mines: 2 in row"));
    MineMenu->Add(new MenuSeparator(
        "Falloff",
        "Detonation scoring uses"));

    PeelMenu = TacticsMenu->AddSubMenu(new Menu(
        "Peel", "Anti-dash, interrupt and protected-carry vectors"));
    PeelMenu->Add(new MenuBool(
        "AntiGapcloser", "E path then W away", true));
    PeelMenu->Add(new MenuBool(
        "Interrupt", "Use reliable W on channels", true));
    PeelMenu->Add(new MenuBool(
        "ProtectCarry", "Reserve W/E for the", true));
    PeelMenu->Add(new MenuSlider(
        "PeelDistance", "Max threat distance from ally", 560, 280, 820));
    PeelMenu->Add(new MenuSeparator(
        "NoDelivery",
        "Never shove a diver closer"));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Worked Ground economy, wave AoE and jungle modifiers"));
    FarmMenu->Add(new MenuBool(
        "Lane", "Champion Q/E wave logic", true));
    FarmMenu->Add(new MenuBool(
        "UseE", "E on valuable wave", true));
    FarmMenu->Add(new MenuSlider(
        "MinimumQHits", "Min Q AoE victims", 3, 1, 7));
    FarmMenu->Add(new MenuSlider(
        "MinimumEHits", "Minimum E initial wave hits", 4, 2, 10));
    FarmMenu->Add(new MenuSlider(
        "LaneMana", "Minimum lane-clear mana (%)", 36, 0, 100));
    FarmMenu->Add(new MenuBool(
        "Jungle", "Taliyah jungle logic", true));
    FarmMenu->Add(new MenuBool(
        "JungleE", "E monster modifier", true));
    FarmMenu->Add(new MenuBool(
        "PreserveVolley", "Do not replace full-volley", true));
    FarmMenu->Add(new MenuSlider(
        "JungleMana", "Minimum jungle mana (%)", 22, 0, 100));
    FarmMenu->Add(new MenuBool(
        "HoldEForChampion", "Reserve E vs contest", true));
    FarmMenu->Add(new MenuSlider(
        "ChampionHoldRange", "Champion contest range", 1050, 500, 1600));

    WallMenu = TacticsMenu->AddSubMenu(new Menu(
        "WeaversWall", "Explicit player wall with team-partition safety"));
    WallMenu->Add(new MenuKeyBind(
        "ManualWall", "Verified wall toward cursor [G]",
        SDK::Keys::G, KeyBindType::Press));
    WallMenu->Add(new MenuSlider(
        "EscapeHp", "Classify manual wall as", 24, 5, 70));
    WallMenu->Add(new MenuBool(
        "UnsafeManual", "Allow explicit endpoint", false));
    WallMenu->Add(new MenuSeparator(
        "NoRide",
        "The controller casts only"));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "Taliyah geometry, row timing and ownership telemetry"));
    CoachMenu->Add(new MenuBool(
        "DrawRanges", "Draw Q/W/E ranges", true));
    CoachMenu->Add(new MenuBool(
        "DrawGround", "Draw Worked Ground", true));
    CoachMenu->Add(new MenuBool(
        "DrawMines", "Draw E rows", true));
    CoachMenu->Add(new MenuBool(
        "DrawQ", "Draw Q first AoE", true));
    CoachMenu->Add(new MenuBool(
        "DrawW", "Draw W center, vector and", true));
    CoachMenu->Add(new MenuBool(
        "DrawProtected", "Mark protected ally", true));
    CoachMenu->Add(new MenuBool(
        "DrawWall", "Draw last verified manual wall", true));
    CoachMenu->Add(new MenuBool(
        "DrawState", "Draw posture/Q form", true));
}

inline void OnLoad() {
    GroundZones.clear();
    GroundZones.reserve(12);
    NextGroundZoneId = 1;
    ActiveMinefield = {};
    ActiveMinefieldExpireTick = 0;
    EnemyWindows.fill({});
    CurrentPosture = Posture::Neutral;
    ActiveSequence = Sequence::None;
    ActiveBranch = ComboBranch::None;
    SequenceStep = SequenceTargetId = SequenceExpireTick = 0;
    SequenceReactive = false;
    LastQPlan = {};
    LastWPlan = {};
    LastEPlan = {};
    LastWallPlan = {};
    ProtectedAllyId = PeelThreatId = PeelThreatUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    InterruptTargetId = InterruptExpireTick = 0;
    IncomingCrowdControlUntil = PredictedDamageLockoutUntil = 0;
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastQForm = QForm::Volley;
    LastQTargetId = LastWTargetId = 0;
    LastWCenter = LastWDirectionEnd = {};
    LastBeforeAttackTargetId = LastBeforeAttackTick = 0;
    LastAfterAttackTargetId = LastAfterAttackTick = 0;
    LastLocalAutoTargetId = LastLocalAutoTick = 0;
    LastRegisteredCastTick.fill(0);
    PlayerOverrideUntil = BoulderSlowTargetId = BoulderSlowStartTick = 0;
    BoulderSlowUntil = 0;
    RChannelUntil = 0;
    RefreshRuntimeState();
}

inline void OnUnload() {
    GroundZones.clear();
    TacticsMenu = QMenu = ComboMenu = MineMenu = nullptr;
    PeelMenu = FarmMenu = WallMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Confirm KuroAIO has no legacy Taliyah champion controller before registration",
    "Confirm 7UPAIO exposes only a commented Taliyah include and route",
    "Confirm SharpShooter, OneKeyToWin and ziblldev do not provide an active Taliyah controller",
    "Pin CommunityDragon PC 16.14 Taliyah bin snapshot dated 15 July 2026",
    "Pin CommunityDragon champion 163 JSON snapshot and SHA-256",
    "Reconcile Riot 12.9 midscope semantics before reading later balance values",
    "Reconcile Riot 12.10 passive speed to 10/15/25/40 percent",
    "Reconcile Riot 13.9 R lockout to damage taken rather than damage dealt",
    "Reconcile Riot 25.12 Big Q damage multiplier to 180 percent",
    "Reconcile Riot 25.18 Q mana and E rank-one-through-five damage",
    "Reconcile Riot 26.2 Q monster bonus ranks and E 225-percent monster modifier",
    "Reconcile Riot 26.5 temporary Q nerf without treating it as current",
    "Reconcile Riot 26.9 restoration to 55/72.5/90/107.5/125 Q damage",
    "Use current Q cooldown 7/6/5/4/3 seconds",
    "Use current ordinary Q mana 55/60/65/70/75",
    "Use current Worked Ground Q mana cost 10",
    "Use current Q 50-percent AP ratio",
    "Use current five-rock total multiplier 2.6",
    "Use 60-percent reduction on each Q rock after the first",
    "Use current Big Q multiplier 1.8 rather than stale double or 1.9 data",
    "Use current Big Q slow 20/25/30/35/40 percent",
    "Use current Big Q slow duration 1.5 seconds rather than stale three-second video narration",
    "Use current normal Q AoE radius 175",
    "Use current Big Q AoE radius 225",
    "Use current Worked Ground radius 400",
    "Use current Worked Ground duration 30 seconds",
    "Use current Worked Ground cooldown reduction 50 percent",
    "Enforce current minimum Worked Ground cooldown 0.75 seconds",
    "Create Worked Ground at Taliyah's Q cast location rather than the impact",
    "Consume exactly the Worked Ground zone containing Taliyah on Big Q",
    "Do not create a new Worked Ground zone from Big Q",
    "Expire inferred Worked Ground exactly after 30 seconds",
    "Resolve deterministic overlap by nearest zone then lower zone id",
    "Draw every inferred active zone and its remaining lifetime",
    "Read current normal Q missile initial speed 3600",
    "Read current normal Q acceleration as negative 5000",
    "Read current normal Q minimum speed 1500",
    "Model normal Q deceleration rather than a stale constant 1450/1700/2000 speed",
    "Read current Big Q fixed missile speed 2000",
    "Separate Q 0.25-second cast from projectile flight",
    "Solve moving first-body contact at sub-frame precision",
    "Include champion, lane-minion and jungle-monster capsules in Q collision",
    "Reject allied, dead, untargetable and invalid Q bodies",
    "Tie-break simultaneous Q contacts by network id",
    "Allow a bounding capsule edge at Q's 1000-unit endpoint",
    "Do not invent lollipop range beyond the endpoint",
    "Reject projectile-wall interception before every Q cast",
    "Aim angular candidates around the predicted moving target",
    "Add minion and monster first-body candidates for AoE bridges",
    "Use normal Q first-impact AoE to hit a champion beside a minion",
    "Use wider Big Q first-impact AoE for champion bridge angles",
    "Preserve the actual first body in coach telemetry",
    "Count all verified first-impact AoE victims",
    "Require moving-target hitchance independently from first-body geometry",
    "Relax Q prediction only for immobile, dashing or reactive targets",
    "Respect the player's cursor direction for nonlethal offensive Q",
    "Allow exact lethal Q to ignore small cursor drift",
    "Preserve an attack windup for nonlethal Q",
    "Allow lethal or reactive Q to override a low-value windup",
    "Use conservative one-rock damage for moving-target Q kill secure",
    "Use full 2.6 multiplier only on verified immobilized targets",
    "Use intermediate volley expectations for committed or Big-Q-slowed targets",
    "Include generic shields in every Q lethal gate",
    "Reject Q lethal claims through spell shield or immunity",
    "Use mitigated magic damage rather than raw tooltip damage",
    "Use rank-scaled Q monster flat damage 20/25/30/35/40",
    "Apply flat monster damage to all five ordinary rocks",
    "Apply the 1.8 multiplier to Big Q monster rock plus flat bonus",
    "Prefer full-volley jungle DPS over Big Q when the camp survives",
    "Permit Big Q jungle use only for execute/setup or player policy",
    "Use Big Q monster stun as setup without claiming champion stun",
    "Use current W script TaliyahWVC and accept the TaliyahW alias",
    "Treat W as zero damage after Riot 12.9",
    "Use W range 900",
    "Use W radius 225",
    "Use W cast time 0.25 seconds",
    "Use W eruption delay 0.5 seconds after cast",
    "Predict W impact 0.75 seconds from the request",
    "Use W throw distance exactly 400",
    "Cast W with a true start/end vector primitive",
    "Normalize every W vector before calculating destination",
    "Generate W directions toward and away from Taliyah",
    "Generate W direction toward player cursor",
    "Generate W direction toward nearby allied turret",
    "Generate W direction toward allied follow-up centroid",
    "Generate W directions along and across the active E field",
    "Generate W away-vector from the protected ally",
    "Score W by the number of distinct E rows crossed",
    "Do not exaggerate overlapping mines in one row into multiple contacts",
    "Reject offensive W that crosses no mines unless exact lethal",
    "Allow interrupt W without requiring mine damage",
    "Wait for target commitment, spent mobility, CC or Big Q slow before ordinary W",
    "Use Big Q slow as a high-reliability W setup",
    "Start the inferred Big Q slow at moving first-body impact rather than cast request",
    "Do not let W erupt before the predicted Big Q slow begins",
    "Prefer E first against close or dash-ready enemies",
    "Prefer W first in fast allied follow-up windows",
    "Allow thin-wall displacement techniques without blanket terrain rejection",
    "Reject nonlethal W that pushes a target into enemy safety",
    "Reject W that delivers a diver closer to the protected carry",
    "Require peel W to increase distance from player or protected ally",
    "Reward W destinations with allied follow-up",
    "Penalize crowded offensive W destinations",
    "Use vector direction rather than a generic behind-target heuristic",
    "Use W cooldown 14/12.5/11/9.5/8 from live spellbook",
    "Use W mana 40/30/20/10/0 from live spellbook",
    "Read current E range 950",
    "Read current E cast time 0.25 seconds",
    "Read current E cooldown 14 seconds at every rank",
    "Read current E mana 90 at every rank",
    "Use E initial damage 60/105/150/195/240",
    "Use E initial AP ratio 60 percent",
    "Use E detonation damage 25/40/55/70/85",
    "Use E detonation AP ratio 30 percent",
    "Use E mine radius 85",
    "Use E slow 20 percent",
    "Use E mine lifetime four seconds per spawned row",
    "Use E stun base 0.75 seconds",
    "Cap E stun at two seconds",
    "Apply current 225-percent E damage modifier to monsters",
    "Reconstruct exactly 22 E mines",
    "Place two mines in E row one",
    "Place four mines in each of E rows two through six",
    "Spawn first E row after the cast",
    "Spawn later E rows 0.17 seconds apart",
    "Track each row's independent spawn and expiry time",
    "Use a conservative widening trapezoid for unavailable server lateral offsets",
    "Reject targets outside the E envelope",
    "Count mine contacts along the full W displacement segment",
    "Require a mine to exist at the exact path-crossing time",
    "Require a mine to remain unexpired at the exact path-crossing time",
    "Apply E detonation falloff 100/75/50/25 percent",
    "Cap E detonation damage at the live 2.5 multiplier",
    "Model E-W after all six rows have time to spawn",
    "Model W-E with only early rows present at W impact",
    "Lock E after W to the minefield direction used to score that W vector",
    "Treat an already-issued W as a live E conversion even though W is on cooldown",
    "Prefer E-W when extra mine rows outweigh surprise timing",
    "Prefer W-E when ally CC or target commitment guarantees the fast center",
    "Cast E first on a committed dash endpoint",
    "Use E as standalone dash denial when W is unavailable",
    "Hold E when no dash, choke, W conversion or committed target exists",
    "Do not spend 90 mana on a one-minion E clear",
    "Require configurable multi-minion E wave value",
    "Hold farm E while a champion contests within configured range",
    "Use E choke value near terrain and objectives",
    "Do not feed an ordinary E into a spell shield",
    "Allow anti-dash E zone despite a shield because denial remains valuable",
    "Track enemy commitment windows from spell casts",
    "Track enemy mobility spent windows from champion-specific slots",
    "Track explicit gapcloser endpoints",
    "Track interruptible channel end time",
    "Reject W interrupt when the 0.75-second impact cannot arrive in time",
    "Allow a timely channel interrupt even when an ordinary shove would favor enemy safety",
    "Refresh the dynamically highest-value protected ally",
    "Reserve W from an unrelated offensive branch while that ally is threatened",
    "E a diver path before W when possible",
    "W the diver away from the protected ally",
    "Never issue movement commands while peeling",
    "Never issue attacks while peeling",
    "Never cast Flash or an item active",
    "Continue a clean player Big Q with W-E when opted in",
    "Continue a clean player W with E when opted in",
    "Continue a clean player E with the best W vector when opted in",
    "Yield for a configurable window after unowned player spell input",
    "Cancel the previous automatic cadence before starting an opted-in manual continuation",
    "Prioritize interrupt, anti-gapclose and carry peel over an offensive active cadence",
    "Keep reactive peel and interrupt available during player ownership",
    "Preserve controller cast ownership through synchronous ProcessSpell",
    "Deduplicate controller prediction and spell-event state registration",
    "Use Q-W-E-Q branch mana with 10 mana for the opening Big Q",
    "Use exact live spellbook mana for all other branch slots",
    "Reject every combo branch whose live mana is insufficient",
    "Select dash E-W-Q ahead of damage branches during an active dash",
    "Select Big Q-W-E-Q on a clean Worked Ground catch",
    "Select W-E-Q for reliable fast follow-up",
    "Select E-W-Q for a dash-ready or committed close target",
    "Fall back to E-Q when W is reserved or unavailable",
    "Fall back to Q-only poke when no control branch is justified",
    "Reject unsafe offensive branches under enemy pressure",
    "Use Q exact first-impact AoE for wave clear scoring",
    "Use cheap Big Q for exact lane last hits when valid",
    "Use E only on configurable wave envelopes",
    "Never use zero-damage W as generic wave clear",
    "Use E's 225-percent modifier on jungle camps and objectives",
    "Allow E on one valuable jungle monster without relaxing the one-lane-minion guard",
    "Use ordinary full Q on large and epic monsters",
    "Do not W displacement-immune epic objectives",
    "Hold farm E for a nearby champion contest",
    "Use Taliyah herself as the W separation anchor while fleeing",
    "Use current R ranges 2500/4500/6500",
    "Use R wall duration four seconds",
    "Use R channel duration one second",
    "Use R missile speed 2000",
    "Use R wall width 120",
    "Respect the three-second champion/structure damage lockout",
    "Track likely incoming damage before the runtime lockout becomes visible",
    "Expose R only through an explicit player key",
    "Reject automatic R from combo, harass, farm and kill secure",
    "Reject the R ride/recast form in the wall planner",
    "Never automate R ride",
    "Never automate R jump-off",
    "Never automate R early wall destruction",
    "Clamp the manual wall endpoint to the learned R rank",
    "Reject low-value manual walls shorter than 900 units",
    "Reject manual wall while immobilized",
    "Reject manual wall with a ready close interrupt threat",
    "Reject manual wall that crosses a protected ally",
    "Reject manual wall that crosses an allied channel",
    "Classify objective walls only when they partition enemy approach",
    "Classify escape walls only when they separate or knock aside a pursuer",
    "Recognize priority-target separation independently of unit iteration order",
    "Allow an explicit unsafe endpoint override without sacrificing allied channels",
    "Keep wall macro intent and ride decision with the player",
    "Draw normal versus Big Q distinctly",
    "Draw Q's actual first-impact AoE rather than a fake endpoint circle",
    "Draw W center, vector and destination",
    "Draw pending versus spawned E rows",
    "Draw the protected ally and last verified wall",
    "Publish current posture, branch, step, Q form, ground count and owner",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Taliyah";
    controller.ControllerId = "kuroaio.ai.taliyah.onetrick";
    controller.KitRevision = "PC 16.14 / Riot 26.9 / CDragon 2026-07-15";
    controller.ResearchArtifact = "AI/Research/AITaliyah.md";
    controller.ImplementationSummary =
        "Accelerating/fixed Q first-body and AoE, Worked Ground ledger, timed 22-mine E, vector W branch planner, player-cooperative assists, carry peel and manual partition-safe R.";
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
    controller.OnInterruptable = &OnInterruptable;
    controller.OnMissileCreate = &OnMissileCreate;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Taliyah