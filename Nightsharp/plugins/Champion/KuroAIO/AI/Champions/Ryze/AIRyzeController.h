#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIRyzeGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Ryze {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::CursorDirectionAgrees;
using ControllerHelpers::EnemyFlashReady;
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
using ControllerHelpers::ValidHostileUnit;
using ControllerHelpers::ValidHostileUnitInGameplayRange;

enum class Posture : std::uint8_t {
    Neutral,
    ShortTrade,
    FullDps,
    RootCatch,
    Kite,
    Peel,
    Farm,
    Warp,
};

enum class Sequence : std::uint8_t {
    None,
    CombatBranch,
    ReactiveRoot,
    FluxBridge,
    WaveEEQ,
    JungleBranch,
    PlayerResetAssist,
    RealmWarpChannel,
};

struct QPlan {
    Vector3 CastPosition = {};
    QContact FirstContact = {};
    CastEvaluation Evaluation = {};
    std::vector<int> FluxVictims = {};
    int IntendedBodyId = 0;
    int PriorityVictimId = 0;
    SDK::HitChance Hitchance = SDK::HitChance::None;
    QPurpose Purpose = QPurpose::Dps;
    float RawDamage = 0.0f;
    float DealtDamage = 0.0f;
    bool Lethal = false;
    bool Valid = false;
};

struct WPlan {
    CastEvaluation Evaluation = {};
    int TargetId = 0;
    WPurpose Purpose = WPurpose::DpsReset;
    bool Valid = false;
};

struct EPlan {
    CastEvaluation Evaluation = {};
    std::vector<int> MarkedIds = {};
    int TargetId = 0;
    int PriorityVictimId = 0;
    EPurpose Purpose = EPurpose::DamageReset;
    bool Valid = false;
};

struct WarpPlan {
    Vector3 Destination = {};
    CastEvaluation Evaluation = {};
    WarpPurpose Purpose = WarpPurpose::ManualCursor;
    int AlliesInPortal = 1;
    bool Valid = false;
};

struct FluxRecord {
    int NetworkId = 0;
    int ExpiresTick = 0;
    bool Confirmed = false;
};

struct EnemyWindow {
    int NetworkId = 0;
    int CommittedUntil = 0;
    int HardCrowdControlSpentUntil = 0;
    int MobilitySpentUntil = 0;
    int IncomingLineUntil = 0;
};

struct MobilityRule {
    SDK::ChampionId Champion = SDK::ChampionId::Unknown;
    SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
};

inline constexpr std::array<MobilityRule, 39> MobilityRules = {
    MobilityRule{ SDK::ChampionId::Ahri, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::Akali, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Akshan, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Azir, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Belveth, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Camille, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Corki, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Diana, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Ekko, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Ezreal, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Fizz, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Gnar, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Gragas, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Graves, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Irelia, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::JarvanIV, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Jax, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Kaisa, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::Kassadin, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::Katarina, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::KhaZix, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Leblanc, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::LeeSin, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Lucian, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Naafiri, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Qiyana, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Rakan, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Renekton, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Rengar, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::Shaco, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Sylas, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Talon, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Tristana, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Tryndamere, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Vayne, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Vi, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Yasuo, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Yone, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Zed, SDK::SpellSlot::W },
};
inline constexpr int kManualOwnershipMs = 520;
inline constexpr int kManualResetWindowMs = 1100;
inline constexpr int kCombatSequenceMs = 4200;
inline constexpr int kWaveSequenceMs = 5600;

inline Menu* TacticsMenu = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* FluxMenu = nullptr;
inline Menu* PrisonMenu = nullptr;
inline Menu* WaveMenu = nullptr;
inline Menu* WarpMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<FluxRecord, 64> FluxRecords = {};
inline std::array<EnemyWindow, 16> EnemyWindows = {};
inline RuneLedger Runes = {};
inline Posture CurrentPosture = Posture::Neutral;
inline Sequence ActiveSequence = Sequence::None;
inline ComboBranch ActiveBranch = ComboBranch::None;
inline QPlan LastQPlan = {};
inline WPlan LastWPlan = {};
inline EPlan LastEPlan = {};
inline WarpPlan LastWarpPlan = {};

inline int SequenceStep = 0;
inline int SequenceTargetId = 0;
inline int SequenceEPrimaryId = 0;
inline int SequenceQBodyId = 0;
inline int SequenceExpireTick = 0;
inline bool SequenceReactive = false;

inline int ProtectedAllyId = 0;
inline int PeelThreatId = 0;
inline int PeelThreatUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int IncomingCrowdControlUntil = 0;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastBeforeAttackTargetId = 0;
inline int LastBeforeAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int LastLocalAutoTargetId = 0;
inline int LastLocalAutoTick = 0;
inline std::array<int, 4> LastRegisteredCastTick = {};
inline std::array<int, 4> LastRegisteredTargetId = {};

inline int PlayerOverrideUntil = 0;
inline int ManualResetTargetId = 0;
inline int ManualResetCastTick = 0;
inline int ManualResetExpireTick = 0;
inline int RootedTargetId = 0;
inline int RootedUntilTick = 0;
inline int WarpChannelUntil = 0;
inline int LastWeaveOpportunityTick = 0;


inline bool IsQEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::Q,
        { "ryzeq", "overload" });
}

inline bool IsWEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::W,
        { "ryzew", "runeprison" });
}

inline bool IsEEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::E,
        { "ryzee", "spellflux" });
}

inline bool IsREvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::R,
        { "ryzer", "realmwarp" });
}

inline int EventSlot(const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsQEvent(args)) return 0;
    if (IsWEvent(args)) return 1;
    if (IsEEvent(args)) return 2;
    if (IsREvent(args)) return 3;
    return -1;
}

inline int EventTargetId(const SDK::Events::ProcessSpellEventArgs& args) {
    return static_cast<int>(args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId);
}

inline AIHeroClient RawEnemyById(int networkId) {
    return ControllerHelpers::RawEnemyHeroByNetworkId(networkId);
}

inline AIHeroClient RawAllyById(int networkId) {
    return ControllerHelpers::RawAllyHeroByNetworkId(networkId);
}

inline FluxRecord* FluxFor(int networkId, bool create = false) {
    if (networkId == 0) return nullptr;
    for (auto& record : FluxRecords) {
        if (record.NetworkId == networkId) return &record;
    }
    if (!create) return nullptr;
    const int now = Now();
    for (auto& record : FluxRecords) {
        if (record.NetworkId == 0 || record.ExpiresTick < now) {
            record = {};
            record.NetworkId = networkId;
            return &record;
        }
    }
    return nullptr;
}

inline bool Fluxed(int networkId) {
    const FluxRecord* record = FluxFor(networkId, false);
    if (record && record->ExpiresTick > Now()) return true;
    const AIBaseClient unit = UnitByNetworkId(networkId);
    return unit.IsValid() && ControllerHelpers::HasAnyBuff(
        unit, { "RyzeE", "ryzee", "RyzeEStack" });
}

inline int FluxExpireTick(int networkId) {
    const FluxRecord* record = FluxFor(networkId, false);
    return record && record->ExpiresTick > Now()
        ? record->ExpiresTick : 0;
}

inline void MarkFlux(int networkId, int expiresTick, bool confirmed = false) {
    FluxRecord* record = FluxFor(networkId, true);
    if (!record) return;
    record->ExpiresTick = std::max(record->ExpiresTick, expiresTick);
    record->Confirmed = record->Confirmed || confirmed;
}

inline void ClearFlux(int networkId) {
    FluxRecord* record = FluxFor(networkId, false);
    if (record) *record = {};
}

inline EnemyWindow* WindowFor(int networkId, bool create = false) {
    return ControllerHelpers::FindEnemyCastWindow(
        EnemyWindows, networkId, create);
}

inline bool MobilitySpent(int networkId) {
    const EnemyWindow* window = ControllerHelpers::EnemyCastWindowById(
        EnemyWindows, networkId);
    return window && window->MobilitySpentUntil >= Now();
}

inline bool TargetCommitted(const AIHeroClient& target) {
    const EnemyWindow* window = ControllerHelpers::EnemyCastWindowById(
        EnemyWindows, static_cast<int>(target.NetworkId()));
    return Engine::IsHardCrowdControlled(target) || target.IsDashing() ||
        (window && window->CommittedUntil >= Now());
}

inline bool ChampionMobilityReady(const AIHeroClient& target) {
    if (!target.IsValid() || MobilitySpent(static_cast<int>(target.NetworkId()))) {
        return false;
    }
    if (EnemyFlashReady(target)) return true;
    for (const auto& rule : MobilityRules) {
        if (ControllerHelpers::ChampionIs(target, rule.Champion) &&
            EnemySpellReady(target, rule.Slot)) return true;
    }
    return false;
}

inline Vector3 EstimatedVelocity(const AIBaseClient& unit) {
    if (!unit.IsValid()) return {};
    const Vector3 future = PredictPosition(unit, 0.45f);
    Vector3 velocity = (future - unit.Position()) / 0.45f;
    velocity.y = 0.0f;
    const float maximum = std::max(325.0f, unit.MoveSpeed() * 1.65f);
    const float speed = velocity.Length2D();
    if (speed > maximum && speed > 0.001f) {
        velocity = velocity * (maximum / speed);
    }
    return velocity;
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
    body.FluxExpiresAt = static_cast<float>(FluxExpireTick(body.Id)) / 1000.0f;
    body.Valid = true;
    body.Targetable = unit.IsTargetable();
    body.Hostile = unit.IsEnemy();
    body.Champion = champion;
    body.Minion = minion;
    body.Monster = monster;
    body.Large = champion || unit.BoundingRadius() >= 65.0f ||
        unit.MaxHealth() >= 1500.0f;
    if (minion) {
        const AIMinionClient lane(unit.Address());
        body.Large = body.Large || IsLargeLaneMinion(lane);
    }
    if (monster) body.Epic = IsEpicMonster(unit);
    return body;
}

inline std::vector<QBody> BuildQBodies() {
    const auto player = GameObjects::Player();
    std::vector<QBody> result;
    result.reserve(64);
    if (!player.IsValid()) return result;
    const auto append = [&](const AIBaseClient& unit,
                            bool champion,
                            bool minion,
                            bool monster) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(unit.Position()) > 1325.0f) return;
        QBody body = RuntimeQBody(unit, champion, minion, monster);
        if (body.Valid && body.Targetable && body.Hostile) result.push_back(body);
    };
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        append(enemy, true, false, false);
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        append(minion, false, true, false);
    }
    for (const auto& monster : GameObjects::Jungle()) {
        append(monster, false, false, true);
    }
    return result;
}

inline float BonusMana() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0.0f;
    const int level = std::clamp(player.Level(), 1, 18);
    const float baseMana = 300.0f + 70.0f * static_cast<float>(level - 1);
    const float passiveMultiplier = 1.0f + 0.001f * std::max(0.0f, player.AP());
    const float unamplifiedMaximum = passiveMultiplier > 0.001f
        ? player.MaxMana() / passiveMultiplier : player.MaxMana();
    return std::max(0.0f, unamplifiedMaximum - baseMana);
}

inline float QDamage(const AIBaseClient& target, bool fluxed) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const float raw = fluxed
        ? FluxedQRawDamage(SpellRank(0), SpellRank(3), player.AP(), BonusMana())
        : QRawDamage(SpellRank(0), player.AP(), BonusMana());
    return player.CalculateMagicDamage(target, raw);
}

inline float WDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return player.CalculateMagicDamage(
        target, WRawDamage(SpellRank(1), player.AP(), BonusMana()));
}

inline float EDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return player.CalculateMagicDamage(
        target, ERawDamage(SpellRank(2), player.AP(), BonusMana()));
}

inline ManaCosts LiveManaCosts() {
    return { SpellCost(0), SpellCost(1), SpellCost(2), SpellCost(3) };
}

inline float DefensiveManaReserve() {
    if (!Bool(ComboMenu, "ReserveEWQ", true)) return 0.0f;
    float reserve = 0.0f;
    if (Ready(2)) reserve += SpellCost(2);
    if (Ready(1)) reserve += SpellCost(1);
    reserve += SpellCost(0);
    return reserve;
}

inline AIHeroClient PreferredEnemy(const AIHeroClient& selected,
                                   float range = 1120.0f) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const AIHeroClient locked = HeroByNetworkId(Engine::LockedTargetNetworkId);
    if (Engine::ValidEnemy(locked, range)) return locked;
    const auto player = GameObjects::Player();
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range)) continue;
        float score = (100.0f - enemy.HealthPercent()) * 2.4f -
            player.Position().Distance2D(enemy.Position()) * 0.10f;
        score += std::max(enemy.TotalAttackDamage(), enemy.AP() * 0.78f) * 0.24f;
        if (Engine::IsHardCrowdControlled(enemy)) score += 260.0f;
        if (Fluxed(static_cast<int>(enemy.NetworkId()))) score += 150.0f;
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    return best;
}

inline AIHeroClient ProtectedAlly() {
    return RawAllyById(ProtectedAllyId);
}

inline AIHeroClient SelectPeelThreat(const AIHeroClient& ally) {
    if (!Engine::ValidAlly(ally)) return {};
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1200.0f)) continue;
        const float distance = enemy.Position().Distance2D(ally.Position());
        if (distance > 720.0f) continue;
        float score = 760.0f - distance +
            std::max(enemy.TotalAttackDamage(), enemy.AP() * 0.74f);
        if (enemy.IsDashing() && enemy.PathEnd().IsValid() &&
            enemy.PathEnd().Distance2D(ally.Position()) < distance) score += 380.0f;
        if (static_cast<int>(enemy.NetworkId()) == PeelThreatId &&
            PeelThreatUntil >= Now()) score += 430.0f;
        if (ChampionMobilityReady(enemy)) score += 85.0f;
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    return best;
}

inline SDK::HitChance RequiredQHitchance(QPurpose purpose,
                                         const AIBaseClient& target) {
    if (purpose == QPurpose::Peel || purpose == QPurpose::Interrupt ||
        purpose == QPurpose::RootFollowup ||
        (target.IsValid() && (target.IsDashing() ||
         Engine::IsHardCrowdControlled(target)))) {
        return SDK::HitChance::Medium;
    }
    SDK::HitChance baseChance = SDK::HitChance::VeryHigh;
    switch (List(FluxMenu, "QHitchance", 2)) {
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
    case SDK::HitChance::VeryHigh: return 0.92f;
    case SDK::HitChance::High: return 0.82f;
    case SDK::HitChance::Medium: return 0.64f;
    default: return 0.30f;
    }
}

inline std::vector<Vector3> QCandidates(const AIBaseClient& target,
                                        SDK::HitChance& observed) {
    std::vector<Vector3> result;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return result;

    Vector3 predicted{};
    observed = SDK::HitChance::VeryHigh;
    if (target.IsHero() && Engine::RuntimeSpells[0]) {
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
        predicted = prediction.GetCastPosition();
        observed = prediction.Hitchance;
    }
    if (!predicted.IsValid() || predicted.IsZero()) {
        const float travel = kQCastSeconds +
            player.Position().Distance2D(target.Position()) / kQMissileSpeed;
        predicted = PredictPosition(target, travel);
    }
    const Vector3 direct = SharedGeometry::Direction2D(
        player.Position(), predicted);
    if (direct.IsZero()) return result;
    static constexpr std::array<float, 11> offsets = {
        0.0f, 0.009f, -0.009f, 0.018f, -0.018f,
        0.032f, -0.032f, 0.050f, -0.050f, 0.072f, -0.072f,
    };
    for (float offset : offsets) {
        const Vector3 direction = SharedGeometry::Rotate2D(direct, offset);
        if (!direction.IsZero()) {
            result.push_back(player.Position() + direction * kQRange);
        }
    }
    return result;
}

inline QPlan BuildQPlan(const AIBaseClient& intended,
                        int priorityVictimId,
                        QPurpose purpose,
                        bool reactive = false) {
    QPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0) || !intended.IsValid() ||
        !ValidHostileUnitInGameplayRange(intended, kQRange + 15.0f)) {
        return best;
    }
    const int intendedId = static_cast<int>(intended.NetworkId());
    if (priorityVictimId == 0) priorityVictimId = intendedId;
    const AIBaseClient priority = UnitByNetworkId(priorityVictimId);
    if (!priority.IsValid()) return best;

    SDK::HitChance observed = SDK::HitChance::None;
    const std::vector<Vector3> candidates = QCandidates(intended, observed);
    const SDK::HitChance required = RequiredQHitchance(purpose, intended);
    const bool fixedTarget = !intended.IsHero();
    const bool highConfidence = fixedTarget || intended.IsDashing() ||
        Engine::IsHardCrowdControlled(intended) ||
        static_cast<int>(observed) >= static_cast<int>(required);
    if (!highConfidence) return best;

    std::vector<QBody> bodies = BuildQBodies();
    const float nowSeconds = static_cast<float>(Now()) / 1000.0f;
    for (const Vector3& aim : candidates) {
        if (!aim.IsValid() || aim.IsZero() ||
            ProjectileWallBlocks(player.Position(), aim, kQMissileRadius)) {
            continue;
        }
        QContact contact{};
        if (!QHitsIntendedFirst(
                player.Position(), aim, bodies, intendedId, &contact)) {
            continue;
        }

        const QBody* intendedBody = FindBody(bodies, intendedId);
        std::vector<int> victims;
        if (intendedBody && FluxActive(*intendedBody, nowSeconds)) {
            victims = FluxedQVictimIds(intendedId, bodies, nowSeconds);
        }
        const bool priorityHit = intendedId == priorityVictimId ||
            ContainsId(victims, priorityVictimId);
        if (priorityVictimId != intendedId && !priorityHit) continue;
        const bool fluxDamage = intendedId == priorityVictimId
            ? Fluxed(intendedId) : ContainsId(victims, priorityVictimId);
        const float dealt = QDamage(priority, fluxDamage);
        const bool shielded = priority.IsHero() &&
            HasSpellShieldOrImmunity(priority);
        const bool immune = priority.IsHero() &&
            IsCommonUntargetableOrImmune(priority);
        const bool lethal = !shielded && !immune &&
            dealt >= priority.Health() + priority.AllShield() + 2.0f;

        QContext context{};
        context.Ready = true;
        context.HasMana = player.Mana() + 0.5f >= SpellCost(0);
        context.TargetValid = true;
        context.IntendedFirstBody = true;
        context.ProjectileWallBlocked = false;
        context.TargetSpellShield = shielded;
        context.TargetImmune = immune;
        context.TargetFluxed = fluxDamage;
        context.TargetImmobile = Engine::IsHardCrowdControlled(intended);
        context.TargetDashing = intended.IsDashing();
        context.HighConfidence = highConfidence;
        context.Lethal = lethal;
        context.PlayerAttackWindingUp = Orbwalker::IsWindingUp();
        context.Reactive = reactive;
        context.SpeedNeeded = player.HealthPercent() <=
            static_cast<float>(Slider(TacticsMenu, "KiteHp", 36)) ||
            purpose == QPurpose::SpeedExit;
        const RuneLedger liveRunes = NormalizeRunes(
            Runes, nowSeconds);
        context.RuneStacks = liveRunes.Stacks;
        context.PreserveTwoRuneSpeed = Bool(
            ComboMenu, "PreserveSpeedQ", true);
        context.PriorityVictimHitByFluxSpread =
            priorityVictimId != intendedId && priorityHit;
        context.CursorAgrees = purpose == QPurpose::Clear ||
            purpose == QPurpose::Objective ||
            CursorDirectionAgrees(priority.Position(), -0.16f) ||
            (Orbwalker::ActiveMode() == OrbwalkingMode::Combo);
        context.FluxVictims = std::max(1, static_cast<int>(victims.size()));
        context.NearbyEnemies = priority.IsHero()
            ? Engine::CountEnemiesAt(priority.Position(), 650.0f) : 1;
        context.CollisionConfidence = fixedTarget
            ? 1.0f : HitchanceConfidence(observed);
        context.Purpose = purpose;
        CastEvaluation evaluation = EvaluateQ(context);
        if (!evaluation.Cast) continue;
        float score = evaluation.Score - contact.ProjectileSeconds * 18.0f;
        if (priorityVictimId != intendedId) score += 180.0f;
        if (!best.Valid || score > best.Evaluation.Score) {
            evaluation.Score = score;
            best.CastPosition = aim;
            best.FirstContact = contact;
            best.Evaluation = evaluation;
            best.FluxVictims = victims;
            best.IntendedBodyId = intendedId;
            best.PriorityVictimId = priorityVictimId;
            best.Hitchance = fixedTarget
                ? SDK::HitChance::VeryHigh : observed;
            best.Purpose = purpose;
            best.RawDamage = fluxDamage
                ? FluxedQRawDamage(
                    SpellRank(0), SpellRank(3), player.AP(), BonusMana())
                : QRawDamage(SpellRank(0), player.AP(), BonusMana());
            best.DealtDamage = dealt;
            best.Lethal = lethal;
            best.Valid = true;
        }
    }
    LastQPlan = best;
    return best;
}

inline void ApplyPredictedFlux(int primaryId,
                               const std::vector<int>* knownMarked = nullptr) {
    const int expires = Now() + static_cast<int>(kFluxSeconds * 1000.0f);
    if (knownMarked && !knownMarked->empty()) {
        for (int id : *knownMarked) MarkFlux(id, expires, false);
        return;
    }
    const std::vector<QBody> bodies = BuildQBodies();
    const std::vector<int> marked = SpellFluxMarkedIds(primaryId, bodies);
    if (marked.empty()) {
        MarkFlux(primaryId, expires, false);
    } else {
        for (int id : marked) MarkFlux(id, expires, false);
    }
}

inline void RegisterLocalSpellState(int slot,
                                    int targetId,
                                    const std::vector<int>* knownIds = nullptr) {
    if (slot < 0 || slot >= 4) return;
    const int now = Now();
    if (now - LastRegisteredCastTick[static_cast<std::size_t>(slot)] <= 70 &&
        LastRegisteredTargetId[static_cast<std::size_t>(slot)] == targetId) {
        return;
    }
    LastRegisteredCastTick[static_cast<std::size_t>(slot)] = now;
    LastRegisteredTargetId[static_cast<std::size_t>(slot)] = targetId;
    const float seconds = static_cast<float>(now) / 1000.0f;
    if (slot == 0) {
        Runes = SpendRunesWithQ(Runes, seconds).After;
        if (knownIds) {
            for (int id : *knownIds) ClearFlux(id);
        } else if (targetId != 0) {
            ClearFlux(targetId);
        }
        LastQCastTick = now;
    } else if (slot == 1) {
        Runes = AddRune(Runes, seconds);
        if (targetId != 0 && Fluxed(targetId)) {
            ClearFlux(targetId);
            RootedTargetId = targetId;
            RootedUntilTick = now + static_cast<int>(
                kWCrowdControlSeconds * 1000.0f);
        }
        LastWCastTick = now;
    } else if (slot == 2) {
        Runes = AddRune(Runes, seconds);
        if (targetId != 0) ApplyPredictedFlux(targetId, knownIds);
        LastECastTick = now;
    } else {
        LastRCastTick = now;
        WarpChannelUntil = now + static_cast<int>(
            (kRealmWarpChargeSeconds + kRealmWarpTeleportSeconds) * 1000.0f);
    }
}

inline bool CastQPlan(const QPlan& plan, bool reactive = false) {
    if (!plan.Valid || !Ready(0)) return false;
    const bool buffered = Now() - std::max(LastWCastTick, LastECastTick) <= 280;
    if (!ControllerHelpers::CastThrottleReady(
            0, 30, (reactive || buffered) ? 0 : -1)) return false;
    if (!Engine::ControllerCastPosition(0, plan.CastPosition)) return false;
    LastQPlan = plan;
    RegisterLocalSpellState(
        0, plan.IntendedBodyId, &LastQPlan.FluxVictims);
    return true;
}

inline bool TargetDenied(const AIBaseClient& target) {
    return target.IsHero() &&
        (HasSpellShieldOrImmunity(target) ||
         IsCommonUntargetableOrImmune(target));
}

inline WPlan BuildWPlan(const AIBaseClient& target,
                        WPurpose purpose,
                        bool reactive = false,
                        bool rootRequired = false) {
    WPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1) || !target.IsValid()) return plan;
    const int targetId = static_cast<int>(target.NetworkId());
    WContext context{};
    context.Ready = true;
    context.HasMana = player.Mana() + 0.5f >= SpellCost(1);
    context.TargetValid = ValidHostileUnit(target, kWRange +
        target.BoundingRadius() + 10.0f);
    context.InRange = context.TargetValid;
    context.TargetFluxed = Fluxed(targetId);
    context.TargetSpellShield = target.IsHero() &&
        HasSpellShieldOrImmunity(target);
    context.TargetImmune = target.IsHero() &&
        IsCommonUntargetableOrImmune(target);
    if (target.IsHero()) {
        const AIHeroClient hero(target.Address());
        context.TargetMobilityReady = ChampionMobilityReady(hero);
        context.TargetCommitted = TargetCommitted(hero);
    }
    context.TargetAlreadyHardCrowdControlled =
        Engine::IsHardCrowdControlled(target);
    context.PlayerAttackWindingUp = Orbwalker::IsWindingUp();
    context.Reactive = reactive;
    // W itself resets Overload, so follow-up Q availability is independent
    // of Q's pre-cast cooldown state.
    context.FollowupQReady = true;
    context.EReady = Ready(2);
    context.RootRequired = rootRequired;
    context.Lethal = WDamage(target) >=
        target.Health() + target.AllShield() + 2.0f;
    context.RuneStacks = NormalizeRunes(
        Runes, static_cast<float>(Now()) / 1000.0f).Stacks;
    context.Purpose = purpose;
    plan.Evaluation = EvaluateW(context);
    plan.TargetId = targetId;
    plan.Purpose = purpose;
    plan.Valid = plan.Evaluation.Cast;
    return plan;
}

inline bool CastWPlan(const WPlan& plan, bool reactive = false) {
    const AIBaseClient target = UnitByNetworkId(plan.TargetId);
    if (!plan.Valid || !target.IsValid() || !Ready(1) ||
        !ControllerHelpers::CastThrottleReady(
            1, 34, reactive ? 0 : -1)) return false;
    if (!Engine::ControllerCastUnit(1, target)) return false;
    LastWPlan = plan;
    RegisterLocalSpellState(1, plan.TargetId);
    return true;
}

inline EPlan BuildEPlan(const AIBaseClient& target,
                        EPurpose purpose,
                        int priorityVictimId = 0,
                        bool reactive = false) {
    EPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2) || !target.IsValid()) return plan;
    const int targetId = static_cast<int>(target.NetworkId());
    if (priorityVictimId == 0) priorityVictimId = targetId;
    const AIBaseClient priority = UnitByNetworkId(priorityVictimId);
    const std::vector<QBody> bodies = BuildQBodies();
    plan.MarkedIds = SpellFluxMarkedIds(targetId, bodies);
    if (plan.MarkedIds.empty()) plan.MarkedIds.push_back(targetId);

    EContext context{};
    context.Ready = true;
    context.HasMana = player.Mana() + 0.5f >= SpellCost(2);
    context.TargetValid = ValidHostileUnit(target, kERange +
        target.BoundingRadius() + 10.0f);
    context.InRange = context.TargetValid;
    context.TargetSpellShield = target.IsHero() &&
        HasSpellShieldOrImmunity(target);
    context.TargetImmune = target.IsHero() &&
        IsCommonUntargetableOrImmune(target);
    context.TargetFluxed = Fluxed(targetId);
    context.WReady = Ready(1);
    context.QReady = true; // E resets Overload regardless of its old cooldown.
    if (priority.IsHero()) {
        const AIHeroClient hero(priority.Address());
        context.TargetMobilityReady = ChampionMobilityReady(hero);
        context.TargetCommitted = TargetCommitted(hero);
    }
    context.TargetWillSurviveE = EDamage(target) + 2.0f <
        target.Health() + target.AllShield();
    context.PriorityVictimWillBeMarked =
        ContainsId(plan.MarkedIds, priorityVictimId);
    context.PlayerAttackWindingUp = Orbwalker::IsWindingUp();
    context.Reactive = reactive;
    context.Lethal = targetId == priorityVictimId &&
        EDamage(target) >= target.Health() + target.AllShield() + 2.0f;
    context.MarkedUnits = static_cast<int>(plan.MarkedIds.size());
    context.RuneStacks = NormalizeRunes(
        Runes, static_cast<float>(Now()) / 1000.0f).Stacks;
    context.Purpose = purpose;
    plan.Evaluation = EvaluateE(context);
    plan.TargetId = targetId;
    plan.PriorityVictimId = priorityVictimId;
    plan.Purpose = purpose;
    plan.Valid = plan.Evaluation.Cast;
    return plan;
}

inline bool CastEPlan(const EPlan& plan, bool reactive = false) {
    const AIBaseClient target = UnitByNetworkId(plan.TargetId);
    if (!plan.Valid || !target.IsValid() || !Ready(2) ||
        !ControllerHelpers::CastThrottleReady(
            2, 32, reactive ? 0 : -1)) return false;
    if (!Engine::ControllerCastUnit(2, target)) return false;
    LastEPlan = plan;
    RegisterLocalSpellState(2, plan.TargetId, &LastEPlan.MarkedIds);
    return true;
}

inline void ClearSequence() {
    ActiveSequence = Sequence::None;
    ActiveBranch = ComboBranch::None;
    SequenceStep = 0;
    SequenceTargetId = 0;
    SequenceEPrimaryId = 0;
    SequenceQBodyId = 0;
    SequenceExpireTick = 0;
    SequenceReactive = false;
}

inline void StartBranch(ComboBranch branch,
                        int targetId,
                        Sequence sequence = Sequence::CombatBranch,
                        bool reactive = false,
                        int ePrimaryId = 0,
                        int qBodyId = 0,
                        int durationMs = kCombatSequenceMs) {
    ActiveSequence = sequence;
    ActiveBranch = branch;
    SequenceStep = 0;
    SequenceTargetId = targetId;
    SequenceEPrimaryId = ePrimaryId;
    SequenceQBodyId = qBodyId;
    SequenceReactive = reactive;
    SequenceExpireTick = Now() + std::max(500, durationMs);
}

inline float EstimatedBranchDamage(const AIBaseClient& target,
                                   ComboBranch branch) {
    if (!target.IsValid()) return 0.0f;
    const BranchDefinition definition = DefinitionFor(branch);
    bool flux = Fluxed(static_cast<int>(target.NetworkId()));
    float total = 0.0f;
    for (int step = 0; step < definition.Count; ++step) {
        const int slot = definition.Slots[static_cast<std::size_t>(step)];
        if (slot == 0) {
            total += QDamage(target, flux);
            flux = false;
        } else if (slot == 1) {
            total += WDamage(target);
            if (flux) flux = false;
        } else if (slot == 2) {
            total += EDamage(target);
            flux = true;
        }
    }
    return total;
}

inline bool SafeToCommit(const AIHeroClient& target, bool lethal) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return false;
    if (Engine::UnderEnemyTurret(player.Position()) && !lethal) return false;
    const int enemies = Engine::CountEnemiesAt(player.Position(), 720.0f);
    const int allies = Engine::CountAlliesAt(player.Position(), 780.0f);
    if (enemies > allies + 1 && !lethal) return false;
    if (HasReadyPointClickThreatAt(player.Position()) &&
        player.HealthPercent() < 52.0f && !Engine::IsHardCrowdControlled(target)) {
        return false;
    }
    return true;
}

inline bool ShouldWaitForAuto(const AIBaseClient& target,
                              int nextSlot,
                              bool reactive) {
    if (reactive || !target.IsHero() || !Bool(ComboMenu, "WeaveAutos", true)) {
        return false;
    }
    const AIHeroClient hero(target.Address());
    AutoWeaveContext context{};
    context.TargetValid = Engine::ValidEnemy(hero);
    context.InAttackRange = InAutoAttackRange(target);
    context.AttackReady = Orbwalker::CanAttack();
    context.Safe = Engine::CountEnemiesAt(
        GameObjects::Player().Position(), 650.0f) <=
        Engine::CountAlliesAt(GameObjects::Player().Position(), 700.0f) + 1;
    context.TargetCanInstantEscape = ChampionMobilityReady(hero);
    context.TargetRooted = Engine::IsHardCrowdControlled(hero) ||
        (RootedTargetId == static_cast<int>(hero.NetworkId()) &&
         RootedUntilTick >= Now());
    context.BufferWindowActive =
        Now() - std::max(LastWCastTick, LastECastTick) <= 230;
    context.NextSpellLethal = nextSlot == 0 &&
        QDamage(target, Fluxed(static_cast<int>(target.NetworkId()))) >=
            target.Health() + target.AllShield();
    context.NextSpellCriticalPeel = ActiveSequence == Sequence::ReactiveRoot;
    context.PlayerIssuedAttack = LastBeforeAttackTargetId ==
        static_cast<int>(target.NetworkId()) &&
        Now() - LastBeforeAttackTick <= 230;
    context.MillisecondsUntilNextReset = Ready(nextSlot) ? 0 : 420;
    const bool wait = ShouldWeaveAuto(context);
    if (wait) LastWeaveOpportunityTick = Now();
    return wait;
}

inline QPurpose SequenceQPurpose() {
    if (ActiveSequence == Sequence::FluxBridge) return QPurpose::FluxBurst;
    if (ActiveSequence == Sequence::WaveEEQ) return QPurpose::Clear;
    if (ActiveSequence == Sequence::JungleBranch) return QPurpose::Objective;
    if (ActiveSequence == Sequence::PlayerResetAssist)
        return QPurpose::ManualResetAssist;
    if (ActiveSequence == Sequence::ReactiveRoot) return QPurpose::SpeedExit;
    if (ActiveBranch == ComboBranch::FastTradeQEQ) return QPurpose::Harass;
    if (ActiveBranch == ComboBranch::ImmediateRootWQEQ ||
        ActiveBranch == ComboBranch::RootedBurstQEWQ)
        return QPurpose::RootFollowup;
    if (ActiveBranch == ComboBranch::KiteEWQ ||
        ActiveBranch == ComboBranch::SlowSpeedWEQ)
        return QPurpose::SpeedExit;
    return QPurpose::Dps;
}

inline WPurpose SequenceWPurpose() {
    if (ActiveSequence == Sequence::ReactiveRoot)
        return WPurpose::Peel;
    if (ActiveBranch == ComboBranch::ImmediateRootWQEQ)
        return WPurpose::ImmediateRoot;
    if (ActiveBranch == ComboBranch::KiteEWQ ||
        ActiveBranch == ComboBranch::FastRootEWQ ||
        ActiveBranch == ComboBranch::RootedBurstQEWQ)
        return WPurpose::RootSetup;
    if (ActiveBranch == ComboBranch::SlowSpeedWEQ)
        return WPurpose::SlowForSpeed;
    if (ActiveSequence == Sequence::JungleBranch)
        return WPurpose::JungleReset;
    return WPurpose::DpsReset;
}

inline EPurpose SequenceEPurpose() {
    if (ActiveSequence == Sequence::FluxBridge) return EPurpose::FluxBridge;
    if (ActiveSequence == Sequence::WaveEEQ) return EPurpose::WaveSpread;
    if (ActiveSequence == Sequence::JungleBranch) return EPurpose::Jungle;
    if (ActiveSequence == Sequence::ReactiveRoot) return EPurpose::PeelSetup;
    if (ActiveBranch == ComboBranch::KiteEWQ ||
        ActiveBranch == ComboBranch::FastRootEWQ ||
        ActiveBranch == ComboBranch::RootedBurstQEWQ)
        return EPurpose::RootSetup;
    return EPurpose::DamageReset;
}

inline bool SequenceWRequiresFlux() {
    return ActiveBranch == ComboBranch::ImmediateRootWQEQ ||
        ActiveBranch == ComboBranch::KiteEWQ ||
        ActiveBranch == ComboBranch::FastRootEWQ ||
        ActiveBranch == ComboBranch::RootedBurstQEWQ;
}

inline AIBaseClient SequenceUnitForSlot(int slot) {
    int networkId = SequenceTargetId;
    if (ActiveSequence == Sequence::FluxBridge ||
        ActiveSequence == Sequence::WaveEEQ) {
        if (slot == 0) networkId = SequenceQBodyId;
        if (slot == 2) {
            networkId = SequenceStep == 0
                ? SequenceEPrimaryId : SequenceQBodyId;
        }
    }
    return UnitByNetworkId(networkId);
}

inline bool TryActiveSequence() {
    if (ActiveSequence == Sequence::None ||
        ActiveSequence == Sequence::RealmWarpChannel) return false;
    if (Now() > SequenceExpireTick) {
        ClearSequence();
        return false;
    }
    const BranchDefinition definition = DefinitionFor(ActiveBranch);
    if (SequenceStep < 0 || SequenceStep >= definition.Count) {
        ClearSequence();
        return false;
    }
    const int slot = definition.Slots[static_cast<std::size_t>(SequenceStep)];
    AIBaseClient unit = SequenceUnitForSlot(slot);
    if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable()) {
        ClearSequence();
        return false;
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return true;
    const float distance = player.Position().Distance2D(unit.Position());
    if (distance > kQRange + unit.BoundingRadius() + 125.0f) {
        ClearSequence();
        return false;
    }
    if (!Ready(slot)) {
        (void)ShouldWaitForAuto(unit, slot, SequenceReactive);
        return true;
    }
    if (ShouldWaitForAuto(unit, slot, SequenceReactive)) return true;

    bool cast = false;
    if (slot == 0) {
        const int priorityId = ActiveSequence == Sequence::FluxBridge
            ? SequenceTargetId : static_cast<int>(unit.NetworkId());
        const QPlan plan = BuildQPlan(
            unit, priorityId, SequenceQPurpose(), SequenceReactive);
        if (!plan.Valid) {
            if (!Orbwalker::IsWindingUp()) ClearSequence();
            return true;
        }
        cast = CastQPlan(plan, SequenceReactive);
    } else if (slot == 1) {
        if (distance > kWRange + unit.BoundingRadius() + 15.0f) return true;
        const WPlan plan = BuildWPlan(
            unit, SequenceWPurpose(), SequenceReactive,
            SequenceWRequiresFlux());
        if (!plan.Valid) {
            if (!Orbwalker::IsWindingUp()) ClearSequence();
            return true;
        }
        cast = CastWPlan(plan, SequenceReactive);
    } else if (slot == 2) {
        if (distance > kERange + unit.BoundingRadius() + 15.0f) return true;
        const int priorityId = ActiveSequence == Sequence::FluxBridge
            ? SequenceTargetId : static_cast<int>(unit.NetworkId());
        const EPlan plan = BuildEPlan(
            unit, SequenceEPurpose(), priorityId, SequenceReactive);
        if (!plan.Valid) {
            if (!Orbwalker::IsWindingUp()) ClearSequence();
            return true;
        }
        cast = CastEPlan(plan, SequenceReactive);
    }
    if (cast) {
        ++SequenceStep;
        if (SequenceStep >= definition.Count) ClearSequence();
    }
    return true;
}

inline bool DestinationHasVisionProxy(const Vector3& destination) {
    if (!destination.IsValid() || destination.IsZero()) return false;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (Engine::ValidAlly(ally) && ally.IsVisible() &&
            ally.Position().Distance2D(destination) <= 900.0f) return true;
    }
    for (const auto& minion : GameObjects::AllyMinions()) {
        if (minion.IsValid() && !minion.IsDead() && minion.IsVisible() &&
            minion.Position().Distance2D(destination) <= 850.0f) return true;
    }
    // REMOVED: Turret/Inhibitor/Nexus class disabled by user request
    // for (const auto& turret : GameObjects::AllyTurrets()) {
    //     if (turret.IsValid() && !turret.IsDead() &&
    //         turret.Position().Distance2D(destination) <= 1100.0f) return true;
    // }
    for (const auto& ward : GameObjects::Wards()) {
        if (ward.IsValid() && !ward.IsDead() && !ward.IsEnemy() &&
            ward.Position().Distance2D(destination) <= 900.0f) return true;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (enemy.IsValid() && enemy.IsVisible() &&
            enemy.Position().Distance2D(destination) <= 900.0f) return true;
    }
    return false;
}

inline bool ProtectedAllyChannelInPortal(const Vector3& origin) {
    static constexpr std::array<const char*, 13> channels = {
        "CaitlynAceintheHole", "Crowstorm", "DrainChannel",
        "JhinR", "KarthusFallenOne", "KatarinaR",
        "LucianR", "Meditate", "MissFortuneBulletTime",
        "NunuR", "ReapTheWhirlwind", "VelkozR", "XerathLocusOfPower2",
    };
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally) ||
            !PortalContains(origin, ally.Position(), ally.BoundingRadius())) {
            continue;
        }
        for (const char* buff : channels) {
            if (ally.HasBuff(buff)) return true;
        }
    }
    return false;
}

inline int AlliesInPortal(const Vector3& origin) {
    const auto player = GameObjects::Player();
    int count = player.IsValid() ? 1 : 0;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally) ||
            (player.IsValid() && ally.NetworkId() == player.NetworkId())) continue;
        if (PortalContains(origin, ally.Position(), ally.BoundingRadius())) ++count;
    }
    return count;
}

inline WarpPlan BuildWarpPlan(WarpPurpose purpose,
                              bool manualAuthorized) {
    WarpPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3)) return plan;
    const Vector3 destination = ClampWarpDestination(
        player.Position(), Game::CursorPos());
    if (!destination.IsValid() || destination.IsZero()) return plan;
    WarpContext context{};
    context.Ready = true;
    context.HasMana = player.Mana() + 0.5f >= SpellCost(3);
    context.ManualAuthorized = manualAuthorized;
    context.AutomaticEmergencyOptIn = Bool(
        WarpMenu, "EmergencyOptIn", false);
    context.OriginValid = player.Position().IsValid();
    context.DestinationValid = destination.IsValid();
    context.DestinationNavigable = !SDK::NavMesh::IsWall(destination);
    context.DestinationUnderEnemyTurret =
        Engine::UnderEnemyTurret(destination);
    context.DestinationHasVision = DestinationHasVisionProxy(destination);
    context.PlayerRootedOrGrounded = PlayerMobilityLocked();
    context.IncomingInterruptLikely = IncomingCrowdControlUntil >= Now() ||
        HasReadyPointClickThreatAt(player.Position());
    context.AllyChannelWouldBeBroken =
        ProtectedAllyChannelInPortal(player.Position());
    context.CursorAgrees = CursorDirectionAgrees(destination, 0.80f);
    context.AllowUnsafeManual = Bool(WarpMenu, "UnsafeManual", false);
    context.PlayerInLethalDanger = player.HealthPercent() <=
        static_cast<float>(Slider(WarpMenu, "EmergencyHp", 17)) &&
        Engine::CountEnemiesAt(player.Position(), 700.0f) > 0;
    context.AlliesAtDestination = Engine::CountAlliesAt(destination, 900.0f);
    context.EnemiesAtDestination = Engine::CountEnemiesAt(destination, 900.0f);
    context.AlliesInPortal = AlliesInPortal(player.Position());
    context.Distance = player.Position().Distance2D(destination);
    context.Purpose = purpose;
    plan.Destination = destination;
    plan.Evaluation = EvaluateWarp(context);
    plan.Purpose = purpose;
    plan.AlliesInPortal = context.AlliesInPortal;
    plan.Valid = plan.Evaluation.Cast;
    return plan;
}

inline bool CastWarpPlan(const WarpPlan& plan) {
    if (!plan.Valid || !Ready(3) ||
        !ControllerHelpers::CastThrottleReady(3, 80, -1)) return false;
    if (!Engine::ControllerCastPosition(3, plan.Destination)) return false;
    LastWarpPlan = plan;
    RegisterLocalSpellState(3, 0);
    ActiveSequence = Sequence::RealmWarpChannel;
    CurrentPosture = Posture::Warp;
    return true;
}

inline bool ManualWarpKeyActive() {
    const auto* key = WarpMenu
        ? WarpMenu->Get<MenuKeyBind>("ManualWarp") : nullptr;
    return key && key->Active;
}

inline bool TryRealmWarp(bool allowEmergency) {
    if (!Ready(3) || WarpChannelUntil >= Now()) return false;
    if (ManualWarpKeyActive()) {
        return CastWarpPlan(BuildWarpPlan(
            WarpPurpose::ManualCursor, true));
    }
    if (allowEmergency && Bool(WarpMenu, "EmergencyOptIn", false)) {
        return CastWarpPlan(BuildWarpPlan(
            WarpPurpose::EmergencyEscape, false));
    }
    return false;
}

inline bool TryFluxBridge(const AIHeroClient& victim,
                          bool harass) {
    if (!Bool(FluxMenu, "IndirectEQ", true) ||
        !Engine::ValidEnemy(victim, 980.0f) || !Ready(2)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const float reserve = harass ? DefensiveManaReserve() : 0.0f;
    const ManaCosts costs = LiveManaCosts();
    if (player.Mana() + 0.5f < costs.E + costs.Q + reserve) return false;
    const std::vector<QBody> bodies = BuildQBodies();
    const float conservativeE = ERawDamage(
        SpellRank(2), player.AP(), BonusMana()) * 0.55f;
    const FluxBridgePlan bridge = BestFluxBridge(
        player.Position(), static_cast<int>(victim.NetworkId()),
        bodies, conservativeE, true);
    if (!bridge.Valid) return false;
    StartBranch(
        ComboBranch::FluxBurstEQ,
        static_cast<int>(victim.NetworkId()),
        Sequence::FluxBridge, false,
        bridge.EPrimaryId, bridge.QDetonationId, 2100);
    return TryActiveSequence();
}

inline bool TryManualResetAssist(Mode mode) {
    if (!Bool(ComboMenu, "AssistManualReset", true) ||
        ManualResetTargetId == 0 || Now() > ManualResetExpireTick ||
        Now() - ManualResetCastTick < 35 ||
        (mode != Mode::Combo && mode != Mode::Harass && mode != Mode::Flee)) {
        return false;
    }
    const AIBaseClient target = UnitByNetworkId(ManualResetTargetId);
    if (!target.IsValid() || !target.IsHero() || !Ready(0)) return false;
    const QPlan plan = BuildQPlan(
        target, ManualResetTargetId,
        QPurpose::ManualResetAssist, false);
    if (!plan.Valid) return false;
    if (!CastQPlan(plan, false)) return false;
    ManualResetTargetId = 0;
    ManualResetExpireTick = 0;
    ActiveSequence = Sequence::PlayerResetAssist;
    return true;
}

inline bool BeginReactiveRoot(const AIHeroClient& target,
                              bool interrupt) {
    if (!Engine::ValidEnemy(target, kERange + 95.0f)) return false;
    const int targetId = static_cast<int>(target.NetworkId());
    if (Fluxed(targetId) && Ready(1)) {
        StartBranch(
            ComboBranch::ImmediateRootWQEQ, targetId,
            Sequence::ReactiveRoot, true, 0, 0, 4200);
        return TryActiveSequence();
    }
    if (Ready(2) && Ready(1)) {
        StartBranch(
            ComboBranch::KiteEWQ, targetId,
            Sequence::ReactiveRoot, true, 0, 0, 2300);
        return TryActiveSequence();
    }
    if (!interrupt && Ready(1)) {
        const AIBaseClient unit(target.Address());
        const WPlan slow = BuildWPlan(
            unit, WPurpose::Peel, true, false);
        return CastWPlan(slow, true);
    }
    return false;
}

inline bool TryInterrupt() {
    if (!Bool(PrisonMenu, "Interrupt", true) ||
        InterruptTargetId == 0 || InterruptExpireTick < Now()) return false;
    const AIHeroClient target = RawEnemyById(InterruptTargetId);
    if (!Engine::ValidEnemy(target, kERange + target.BoundingRadius() + 30.0f)) {
        return false;
    }
    const int remaining = InterruptExpireTick - Now();
    if (Fluxed(InterruptTargetId) && Ready(1)) {
        return BeginReactiveRoot(target, true);
    }
    if (remaining >= 520 && Ready(2) && Ready(1)) {
        return BeginReactiveRoot(target, true);
    }
    return false;
}

inline bool TryGapcloser() {
    if (!Bool(PrisonMenu, "AntiGapcloser", true) ||
        GapcloserTargetId == 0 || GapcloserExpireTick < Now()) return false;
    const AIHeroClient target = RawEnemyById(GapcloserTargetId);
    if (!Engine::ValidEnemy(target, kERange + 100.0f)) return false;
    return BeginReactiveRoot(target, false);
}

inline bool TryPeel() {
    if (!Bool(PrisonMenu, "ProtectCarry", true)) return false;
    const AIHeroClient ally = ProtectedAlly();
    if (!Engine::ValidAlly(ally)) return false;
    const AIHeroClient threat = SelectPeelThreat(ally);
    if (!Engine::ValidEnemy(threat, kERange + 100.0f)) return false;
    const float distance = threat.Position().Distance2D(ally.Position());
    if (distance > static_cast<float>(Slider(
            PrisonMenu, "PeelDistance", 525)) &&
        !(threat.IsDashing() && threat.PathEnd().IsValid() &&
          threat.PathEnd().Distance2D(ally.Position()) < distance)) {
        return false;
    }
    CurrentPosture = Posture::Peel;
    return BeginReactiveRoot(threat, false);
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(TacticsMenu, "KillSecure", true) ||
        ActiveSequence != Sequence::None) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    std::vector<AIHeroClient> targets;
    if (Engine::ValidEnemy(preferred, 1120.0f)) targets.push_back(preferred);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1120.0f)) continue;
        if (preferred.IsValid() && enemy.NetworkId() == preferred.NetworkId()) continue;
        targets.push_back(enemy);
    }
    std::sort(targets.begin(), targets.end(), [](const AIHeroClient& left,
                                                 const AIHeroClient& right) {
        return left.Health() + left.AllShield() <
               right.Health() + right.AllShield();
    });
    for (const auto& target : targets) {
        const int id = static_cast<int>(target.NetworkId());
        if (Ready(0)) {
            const QPlan q = BuildQPlan(target, id, QPurpose::Kill, true);
            if (q.Valid && q.Lethal && CastQPlan(q, true)) return true;
        }
        if (Ready(2) && player.Mana() + 0.5f >= SpellCost(2) &&
            ValidHostileUnit(target, kERange + target.BoundingRadius()) &&
            !TargetDenied(target) && EDamage(target) >=
                target.Health() + target.AllShield() + 2.0f) {
            const EPlan e = BuildEPlan(target, EPurpose::DamageReset, id, true);
            if (CastEPlan(e, true)) return true;
        }
        if (Ready(1) && ValidHostileUnit(
                target, kWRange + target.BoundingRadius()) &&
            !TargetDenied(target) && WDamage(target) >=
                target.Health() + target.AllShield() + 2.0f) {
            const WPlan w = BuildWPlan(target, WPurpose::DpsReset, true, false);
            if (CastWPlan(w, true)) return true;
        }
        if (Ready(2) &&
            EstimatedBranchDamage(target, ComboBranch::FluxBurstEQ) >=
                target.Health() + target.AllShield() + 3.0f &&
            ValidHostileUnit(target, kERange + target.BoundingRadius())) {
            StartBranch(ComboBranch::FluxBurstEQ, id,
                        Sequence::CombatBranch, true, 0, 0, 1800);
            return TryActiveSequence();
        }
    }
    return false;
}

inline ComboContext RuntimeComboContext(const AIHeroClient& target,
                                        bool harass) {
    ComboContext context{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return context;
    const int targetId = static_cast<int>(target.NetworkId());
    context.QReady = Ready(0);
    context.WReady = Ready(1);
    context.EReady = Ready(2);
    context.TargetValid = true;
    context.TargetFluxed = Fluxed(targetId);
    context.TargetCommitted = TargetCommitted(target);
    context.TargetMobilityReady = ChampionMobilityReady(target);
    context.TargetHardCrowdControlled =
        Engine::IsHardCrowdControlled(target);
    if (Ready(0)) {
        context.CleanQ = BuildQPlan(
            target, targetId,
            harass ? QPurpose::Harass : QPurpose::Dps).Valid;
    }
    context.TargetDistance = player.Position().Distance2D(target.Position());
    if (context.TargetDistance > kERange + target.BoundingRadius() + 20.0f) {
        context.WReady = false;
        context.EReady = false;
    }
    const float maximumDamage = EstimatedBranchDamage(
        target, ComboBranch::MaximumDpsQEQWQEQ);
    context.Lethal = maximumDamage >=
        target.Health() + target.AllShield() + 3.0f;
    context.SafeToCommit = SafeToCommit(target, context.Lethal);
    context.SpeedNeeded = player.HealthPercent() <=
            static_cast<float>(Slider(TacticsMenu, "KiteHp", 36)) ||
        Engine::CountEnemiesAt(player.Position(), 600.0f) >
            Engine::CountAlliesAt(player.Position(), 700.0f) + 1;
    context.FullDpsWindow = Bool(ComboMenu, "FullDps", true) &&
        context.TargetDistance <= kERange + target.BoundingRadius() + 20.0f &&
        (context.TargetCommitted || context.TargetHardCrowdControlled ||
         MobilitySpent(targetId));
    context.Harass = harass;
    context.RuneStacks = NormalizeRunes(
        Runes, static_cast<float>(Now()) / 1000.0f).Stacks;
    context.NearbyEnemies = Engine::CountEnemiesAt(player.Position(), 700.0f);
    context.CurrentMana = player.Mana();
    context.ReservedMana = harass ? DefensiveManaReserve() : 0.0f;
    context.Costs = LiveManaCosts();
    return context;
}

inline bool TryCombo(const AIHeroClient& selected) {
    const AIHeroClient target = PreferredEnemy(selected, 1120.0f);
    if (!Engine::ValidEnemy(target)) return false;
    CurrentPosture = Posture::RootCatch;
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    if (distance > kERange + target.BoundingRadius() + 20.0f &&
        TryFluxBridge(target, false)) return true;

    ComboContext context = RuntimeComboContext(target, false);
    const ComboDecision decision = SelectComboBranch(context);
    if (decision.Branch != ComboBranch::None) {
        CurrentPosture = decision.Branch == ComboBranch::MaximumDpsQEQWQEQ
            ? Posture::FullDps
            : (decision.Branch == ComboBranch::KiteEWQ ||
               decision.Branch == ComboBranch::SlowSpeedWEQ
                ? Posture::Kite : Posture::RootCatch);
        StartBranch(
            decision.Branch, static_cast<int>(target.NetworkId()),
            Sequence::CombatBranch, false);
        return TryActiveSequence();
    }
    if (Ready(0)) {
        const QPlan q = BuildQPlan(
            target, static_cast<int>(target.NetworkId()), QPurpose::Dps);
        if (CastQPlan(q)) return true;
    }
    return false;
}

inline bool TryHarass(const AIHeroClient& selected) {
    if (PlayerManaPercent() < static_cast<float>(Slider(
            ComboMenu, "HarassMana", 47))) return false;
    const AIHeroClient target = PreferredEnemy(selected, 1080.0f);
    if (!Engine::ValidEnemy(target)) return false;
    CurrentPosture = Posture::ShortTrade;
    const auto player = GameObjects::Player();
    if (player.Position().Distance2D(target.Position()) >
            kERange + target.BoundingRadius() + 20.0f &&
        TryFluxBridge(target, true)) return true;
    ComboContext context = RuntimeComboContext(target, true);
    const ComboDecision decision = SelectComboBranch(context);
    if (decision.Branch != ComboBranch::None) {
        StartBranch(
            decision.Branch, static_cast<int>(target.NetworkId()),
            Sequence::CombatBranch, false, 0, 0, 3000);
        return TryActiveSequence();
    }
    if (Ready(0)) {
        const QPlan q = BuildQPlan(
            target, static_cast<int>(target.NetworkId()), QPurpose::Harass);
        return CastQPlan(q);
    }
    return false;
}

inline AIHeroClient NearestPursuer(const AIHeroClient& fallback = {}) {
    const auto player = GameObjects::Player();
    AIHeroClient best = Engine::ValidEnemy(fallback, 760.0f)
        ? fallback : AIHeroClient{};
    float bestDistance = best.IsValid()
        ? player.Position().Distance2D(best.Position()) : FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 760.0f)) continue;
        const float distance = player.Position().Distance2D(enemy.Position());
        if (distance < bestDistance) {
            best = enemy;
            bestDistance = distance;
        }
    }
    return best;
}

inline bool TryFlee(const AIHeroClient& selected) {
    CurrentPosture = Posture::Kite;
    const AIHeroClient pursuer = NearestPursuer(selected);
    if (Engine::ValidEnemy(pursuer, kERange + 100.0f) &&
        BeginReactiveRoot(pursuer, false)) return true;
    if (Engine::ValidEnemy(pursuer, kQRange + 80.0f) && Ready(0) &&
        NormalizeRunes(Runes, static_cast<float>(Now()) / 1000.0f).Stacks >= 2) {
        const QPlan q = BuildQPlan(
            pursuer, static_cast<int>(pursuer.NetworkId()),
            QPurpose::SpeedExit, true);
        if (CastQPlan(q, true)) return true;
    }
    return TryRealmWarp(true);
}

inline bool TryBestFluxedWaveQ(int minimumVictims, QPurpose purpose) {
    if (!Ready(0)) return false;
    const auto player = GameObjects::Player();
    const std::vector<QBody> bodies = BuildQBodies();
    const float nowSeconds = static_cast<float>(Now()) / 1000.0f;
    int bestId = 0;
    int bestVictims = 0;
    for (const auto& body : bodies) {
        if ((!body.Minion && !body.Monster) ||
            !FluxActive(body, nowSeconds) ||
            player.Position().Distance2D(body.Position) >
                kQRange + body.Radius) continue;
        const int victims = static_cast<int>(
            FluxedQVictimIds(body.Id, bodies, nowSeconds).size());
        if (victims > bestVictims) {
            bestId = body.Id;
            bestVictims = victims;
        }
    }
    if (bestId == 0 || bestVictims < minimumVictims) return false;
    const AIBaseClient target = UnitByNetworkId(bestId);
    const QPlan q = BuildQPlan(target, bestId, purpose);
    return CastQPlan(q);
}

inline bool TryQLastHit(bool jungle = false) {
    if (!Ready(0)) return false;
    const auto& units = jungle
        ? GameObjects::Jungle() : GameObjects::EnemyMinions();
    AIBaseClient best{};
    float bestHealth = FLT_MAX;
    for (const auto& candidate : units) {
        if (!ValidHostileUnitInGameplayRange(candidate, kQRange) ||
            QDamage(candidate, Fluxed(static_cast<int>(candidate.NetworkId()))) +
                2.0f < candidate.Health()) continue;
        if (candidate.Health() < bestHealth) {
            best = candidate;
            bestHealth = candidate.Health();
        }
    }
    if (!best.IsValid()) return false;
    const QPlan q = BuildQPlan(
        best, static_cast<int>(best.NetworkId()),
        jungle ? QPurpose::Objective : QPurpose::Clear);
    return CastQPlan(q);
}

inline bool TryLaneFarm(Mode mode) {
    if (!Bool(WaveMenu, "Lane", true)) return false;
    const bool lastHit = mode == Mode::LastHit;
    if (PlayerManaPercent() < static_cast<float>(Slider(
            WaveMenu, lastHit ? "LastHitMana" : "LaneMana",
            lastHit ? 28 : 42))) return false;
    if (Bool(WaveMenu, "HoldForChampion", true) &&
        HasEnemyChampionNear(static_cast<float>(Slider(
            WaveMenu, "ChampionHoldRange", 1050)))) return false;
    if (lastHit) return TryQLastHit(false);

    if (TryBestFluxedWaveQ(
            Slider(WaveMenu, "MinimumQHits", 3), QPurpose::Clear)) return true;
    if (Ready(2)) {
        const auto player = GameObjects::Player();
        const std::vector<QBody> bodies = BuildQBodies();
        const float conservativeE = ERawDamage(
            SpellRank(2), player.AP(), BonusMana()) * 0.55f;
        const WaveFluxPlan wave = BestWaveFluxPlan(
            player.Position(), bodies, conservativeE,
            Slider(WaveMenu, "MinimumEEQHits", 4));
        const ManaCosts costs = LiveManaCosts();
        if (wave.Valid && player.Mana() + 0.5f >=
                costs.E * 2.0f + costs.Q) {
            StartBranch(
                ComboBranch::ClearEEQ, wave.EPrimaryId,
                Sequence::WaveEEQ, false,
                wave.EPrimaryId, wave.QDetonationId, kWaveSequenceMs);
            return TryActiveSequence();
        }
    }
    return TryQLastHit(false);
}

inline bool TryJungleFarm() {
    if (!Bool(WaveMenu, "Jungle", true) ||
        PlayerManaPercent() < static_cast<float>(Slider(
            WaveMenu, "JungleMana", 24))) return false;
    const AIMinionClient target = SelectJungleTarget(kQRange + 60.0f);
    if (!target.IsValid()) return false;
    const int id = static_cast<int>(target.NetworkId());
    if (TryBestFluxedWaveQ(1, IsEpicMonster(target)
            ? QPurpose::Objective : QPurpose::Clear)) return true;
    ComboBranch branch = ComboBranch::FluxBurstEQ;
    if (!HasEnemyChampionNear(1100.0f) &&
        Ready(0) && Ready(1) && Ready(2) &&
        GameObjects::Player().Mana() + 0.5f >= BranchMana(
            ComboBranch::MaximumDpsQEQWQEQ, LiveManaCosts())) {
        branch = ComboBranch::MaximumDpsQEQWQEQ;
    } else if (Ready(0) && Ready(2)) {
        branch = ComboBranch::FastTradeQEQ;
    }
    StartBranch(
        branch, id, Sequence::JungleBranch, false, 0, 0,
        IsEpicMonster(target) ? 7200 : 4800);
    return TryActiveSequence();
}

inline void RefreshRuntimeState() {
    const int now = Now();
    const float seconds = static_cast<float>(now) / 1000.0f;
    Runes = NormalizeRunes(Runes, seconds);
    for (auto& record : FluxRecords) {
        if (record.NetworkId != 0 && record.ExpiresTick <= now) record = {};
    }
    for (const QBody& body : BuildQBodies()) {
        const AIBaseClient unit = UnitByNetworkId(body.Id);
        if (unit.IsValid() && ControllerHelpers::HasAnyBuff(
                unit, { "RyzeE", "ryzee", "RyzeEStack" })) {
            MarkFlux(body.Id, now + 260, true);
        }
    }
    const auto player = GameObjects::Player();
    if (player.IsValid()) {
        if (ControllerHelpers::HasAnyBuff(player, {
                "RyzeQIconFullCharge", "RyzeQFullCharge",
                "ryzeqiconfullcharge" })) {
            Runes.Stacks = 2;
            Runes.ExpiresAt = seconds + 0.30f;
        } else if (ControllerHelpers::HasAnyBuff(player, {
                       "RyzeQIconHalfCharge", "RyzeQHalfCharge",
                       "ryzeqiconhalfcharge" })) {
            Runes.Stacks = std::max(1, Runes.Stacks);
            Runes.ExpiresAt = seconds + 0.30f;
        }
        if (ControllerHelpers::HasAnyBuff(player, {
                "RyzeRChannel", "RyzeRChannelManager",
                "RealmWarpChannel" })) {
            WarpChannelUntil = std::max(WarpChannelUntil, now + 180);
        }
    }
    if (RootedUntilTick < now) RootedTargetId = 0;
    if (ManualResetExpireTick < now) ManualResetTargetId = 0;
    if (GapcloserExpireTick < now) GapcloserTargetId = 0;
    if (InterruptExpireTick < now) InterruptTargetId = 0;
    if (PeelThreatUntil < now) PeelThreatId = 0;
    if (ActiveSequence == Sequence::RealmWarpChannel &&
        WarpChannelUntil < now) ClearSequence();
    if (ActiveSequence != Sequence::None &&
        ActiveSequence != Sequence::RealmWarpChannel &&
        SequenceExpireTick < now) ClearSequence();

    const AIHeroClient protectedAlly = SelectProtectionAlly(1250.0f);
    ProtectedAllyId = protectedAlly.IsValid()
        ? static_cast<int>(protectedAlly.NetworkId()) : 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshRuntimeState();
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) return true;

    if (TryRealmWarp(false)) return true;
    if (WarpChannelUntil >= Now() ||
        ActiveSequence == Sequence::RealmWarpChannel) return true;
    if (TryManualResetAssist(mode)) return true;
    if (TryInterrupt()) return true;
    if (TryGapcloser()) return true;
    if (TryPeel()) return true;
    if (PlayerOverrideUntil >= Now()) return true;
    if (ActiveSequence != Sequence::None) {
        (void)TryActiveSequence();
        return true;
    }
    if (TryKillSecure(PreferredEnemy(selected, 1120.0f))) return true;

    CurrentPosture = Posture::Neutral;
    if (mode == Mode::Combo) return TryCombo(selected);
    if (mode == Mode::Harass) return TryHarass(selected);
    if (mode == Mode::Flee) return TryFlee(selected);
    if (mode == Mode::Jungle) {
        CurrentPosture = Posture::Farm;
        return TryJungleFarm();
    }
    if (mode == Mode::LaneClear || mode == Mode::LastHit) {
        CurrentPosture = Posture::Farm;
        return TryLaneFarm(mode);
    }
    return true;
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
            window->HardCrowdControlSpentUntil, Now() + 5000);
        if (analysis.TargetsPlayer || analysis.CrossesPlayer) {
            IncomingCrowdControlUntil = std::max(
                IncomingCrowdControlUntil,
                std::max(analysis.LineThreatUntilTick, Now() + 520));
        }
    }
    for (const auto& rule : MobilityRules) {
        if (ControllerHelpers::ChampionIs(analysis.Enemy, rule.Champion) &&
            args.Slot == static_cast<int>(rule.Slot)) {
            window->MobilitySpentUntil = std::max(
                window->MobilitySpentUntil, Now() + 3500);
        }
    }
    const int targetId = EventTargetId(args);
    const AIHeroClient ally = RawAllyById(targetId);
    if (Engine::ValidAlly(ally) &&
        targetId != static_cast<int>(GameObjects::Player().NetworkId())) {
        PeelThreatId = id;
        PeelThreatUntil = Now() + 900;
    }
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const int slot = EventSlot(args);
    if (slot < 0) return;
    const int targetId = EventTargetId(args);
    const bool controllerOwned = Engine::WasControllerCast(slot, 700);
    RegisterLocalSpellState(slot, targetId);
    if (controllerOwned) return;

    ClearSequence();
    PlayerOverrideUntil = Now() + Slider(
        TacticsMenu, "ManualOwnershipMs", kManualOwnershipMs);
    if ((slot == 1 || slot == 2) && targetId != 0 &&
        Bool(ComboMenu, "AssistManualReset", true)) {
        const AIBaseClient target = UnitByNetworkId(targetId);
        if (target.IsValid() && target.IsHero()) {
            ManualResetTargetId = targetId;
            ManualResetCastTick = Now();
            ManualResetExpireTick = Now() + kManualResetWindowMs;
        }
    }
    if (slot == 3) {
        WarpChannelUntil = Now() + static_cast<int>(
            (kRealmWarpChargeSeconds + kRealmWarpTeleportSeconds) * 1000.0f);
        ActiveSequence = Sequence::RealmWarpChannel;
    }
}

inline void OnProcessSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        (void)CaptureLocalAutoAttack(
            args, LastLocalAutoTargetId, LastLocalAutoTick);
        ObserveLocalSpell(args);
    }
    else ObserveEnemySpell(args);
}




inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (!CaptureGapcloser(
            args, GapcloserTargetId, GapcloserEndpoint,
            GapcloserExpireTick, 650.0f, 780)) return;
    EnemyWindow* window = WindowFor(GapcloserTargetId, true);
    if (window) {
        window->CommittedUntil = std::max(
            window->CommittedUntil, Now() + 900);
        window->MobilitySpentUntil = std::max(
            window->MobilitySpentUntil, Now() + 3500);
    }
}


inline bool BuffContains(const SDK::Events::BuffEventArgs& args,
                         const char* token) {
    return token && token[0] && Engine::TextContains(args.BuffName, token);
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args,
                            bool added) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    const auto player = GameObjects::Player();
    const bool local = player.IsValid() && id == player.NetworkId();
    if (BuffContains(args, "ryzee") ||
        BuffContains(args, "spellflux")) {
        if (added) {
            MarkFlux(id, ControllerHelpers::BuffExpireTick(
                args, static_cast<int>(kFluxSeconds * 1000.0f)), true);
        } else {
            ClearFlux(id);
        }
    }
    if (BuffContains(args, "ryzewroot") ||
        BuffContains(args, "runeprisonroot")) {
        if (added) {
            RootedTargetId = id;
            RootedUntilTick = ControllerHelpers::BuffExpireTick(
                args, static_cast<int>(kWCrowdControlSeconds * 1000.0f));
        } else if (RootedTargetId == id) {
            RootedTargetId = 0;
            RootedUntilTick = 0;
        }
    }
    if (!local) return;
    const float seconds = static_cast<float>(Now()) / 1000.0f;
    if (added && (BuffContains(args, "fullcharge") ||
                  BuffContains(args, "ryzeqiconfull"))) {
        Runes.Stacks = 2;
        Runes.ExpiresAt = std::max(
            Runes.ExpiresAt, static_cast<float>(
                ControllerHelpers::BuffExpireTick(
                    args, static_cast<int>(kRuneSeconds * 1000.0f))) / 1000.0f);
    } else if (added && (BuffContains(args, "halfcharge") ||
                         BuffContains(args, "ryzeqiconhalf"))) {
        Runes.Stacks = std::max(1, Runes.Stacks);
        Runes.ExpiresAt = std::max(Runes.ExpiresAt, seconds + kRuneSeconds);
    }
    if (added && BuffContains(args, "ryzeqms")) Runes = {};
    if (BuffContains(args, "ryzerchannel") ||
        BuffContains(args, "realmwarpchannel")) {
        WarpChannelUntil = added
            ? ControllerHelpers::BuffExpireTick(args, 2800) : 0;
        if (added) ActiveSequence = Sequence::RealmWarpChannel;
        else if (ActiveSequence == Sequence::RealmWarpChannel) ClearSequence();
    }
}




inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::ShortTrade: return "QEQ trade";
    case Posture::FullDps: return "QEQWQEQ";
    case Posture::RootCatch: return "root catch";
    case Posture::Kite: return "two-rune kite";
    case Posture::Peel: return "carry peel";
    case Posture::Farm: return "flux farm";
    case Posture::Warp: return "realm warp";
    default: return "neutral";
    }
}

inline const char* SequenceName(Sequence sequence) {
    switch (sequence) {
    case Sequence::CombatBranch: return "combat";
    case Sequence::ReactiveRoot: return "reactive EWQ";
    case Sequence::FluxBridge: return "indirect EQ";
    case Sequence::WaveEEQ: return "wave EEQ";
    case Sequence::JungleBranch: return "jungle resets";
    case Sequence::PlayerResetAssist: return "player reset";
    case Sequence::RealmWarpChannel: return "R channel";
    default: return "none";
    }
}

inline const char* BranchName(ComboBranch branch) {
    switch (branch) {
    case ComboBranch::ImmediateRootWQEQ: return "W-Q-E-Q";
    case ComboBranch::FastRootEWQ: return "E-W-Q";
    case ComboBranch::RootedBurstQEWQ: return "Q-E-W-Q";
    case ComboBranch::MaximumDpsQEQWQEQ: return "Q-E-Q-W-Q-E-Q";
    case ComboBranch::TripleQNoRootQWQEQ: return "Q-W-Q-E-Q";
    case ComboBranch::SlowSpeedWEQ: return "W-E-Q";
    case ComboBranch::FastTradeQEQ: return "Q-E-Q";
    case ComboBranch::FluxBurstEQ: return "E-Q";
    case ComboBranch::KiteEWQ: return "kite E-W-Q";
    case ComboBranch::ClearEEQ: return "E-E-Q";
    default: return "none";
    }
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (Bool(CoachMenu, "DrawRanges", true)) {
        Drawing::DrawCircle(player.Position(), kQRange,
                            0x665C9DFFu, 1.25f, 72);
        Drawing::DrawCircle(player.Position(), kERange,
                            0x6659E0C7u, 1.15f, 56);
        if (Ready(3)) {
            Drawing::DrawCircle(player.Position(), kRealmWarpMaximumRange,
                                0x334C79FFu, 1.0f, 96);
        }
    }
    if (Bool(CoachMenu, "DrawQ", true) && LastQPlan.Valid) {
        const std::uint32_t color = LastQPlan.Lethal
            ? 0xFFF5C84Cu
            : (LastQPlan.PriorityVictimId != LastQPlan.IntendedBodyId
                ? 0xFFE183FFu : 0xFF62B8FFu);
        Drawing::DrawLine(
            player.Position(), LastQPlan.CastPosition, color, 2.0f);
        if (LastQPlan.FirstContact.TargetPosition.IsValid()) {
            Drawing::DrawCircle(
                LastQPlan.FirstContact.TargetPosition,
                kQMissileRadius + 18.0f, color, 2.0f, 32);
        }
    }
    if (Bool(CoachMenu, "DrawFlux", true)) {
        for (const auto& record : FluxRecords) {
            if (record.NetworkId == 0 || record.ExpiresTick <= Now()) continue;
            const AIBaseClient unit = UnitByNetworkId(record.NetworkId);
            if (!unit.IsValid()) continue;
            const std::uint32_t color = record.Confirmed
                ? 0xDD52D7FFu : 0xAAAB8BFFu;
            Drawing::DrawCircle(
                unit.Position(), unit.BoundingRadius() + 28.0f,
                color, 1.6f, 30);
            Vec2 screen{};
            if (Drawing::WorldToScreen(unit.Position(), screen)) {
                char text[48]{};
                _snprintf_s(text, sizeof(text), _TRUNCATE,
                    "E %.1f", std::max(0, record.ExpiresTick - Now()) / 1000.0f);
                Drawing::DrawText(
                    screen.x - 18.0f, screen.y - 43.0f,
                    0xFFD7F6FFu, text);
            }
        }
    }
    if (Bool(CoachMenu, "DrawProtected", true)) {
        const AIHeroClient ally = ProtectedAlly();
        if (ally.IsValid()) {
            Drawing::DrawCircle(
                ally.Position(), ally.BoundingRadius() + 48.0f,
                0xAA69E8A8u, 1.5f, 32);
        }
    }
    if (Bool(CoachMenu, "DrawWarp", true) && LastWarpPlan.Valid) {
        Drawing::DrawCircle(
            LastWarpPlan.Destination, kRealmWarpRadius,
            0xCC6E7DFFu, 2.0f, 64);
        Drawing::DrawLine(
            player.Position(), LastWarpPlan.Destination,
            0x887A91FFu, 1.3f);
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            const RuneLedger runes = NormalizeRunes(
                Runes, static_cast<float>(Now()) / 1000.0f);
            char state[512]{};
            _snprintf_s(
                state, sizeof(state), _TRUNCATE,
                "Ryze OTP | %s | %s %s | runes %d | owner %s",
                PostureName(CurrentPosture), SequenceName(ActiveSequence),
                BranchName(ActiveBranch), runes.Stacks,
                PlayerOverrideUntil >= Now() ? "player" : "controller");
            Drawing::DrawText(
                screen.x - 245.0f, screen.y - 110.0f,
                0xFFD7F6FFu, state);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "RyzeOneTrick", "Ryze one-trick reset conductor"));
    TacticsMenu->Add(new MenuBool(
        "KillSecure", "Secure mitigated spells", true));
    TacticsMenu->Add(new MenuSlider(
        "KiteHp", "2-rune escape HP (%)", 36, 5, 80));
    TacticsMenu->Add(new MenuSlider(
        "ManualOwnershipMs", "Yield player spell (ms)",
        kManualOwnershipMs, 150, 1000));
    TacticsMenu->Add(new MenuSeparator(
        "Ownership",
        "Movement, target selection,"));

    ComboMenu = TacticsMenu->AddSubMenu(new Menu(
        "ComboRhythm", "Reset cadence, roots and auto-weave windows"));
    ComboMenu->Add(new MenuBool(
        "FullDps", "Use Q-E-Q-W-Q-E-Q on", true));
    ComboMenu->Add(new MenuBool(
        "ReserveEWQ", "Reserve E-W-Q mana", true));
    ComboMenu->Add(new MenuBool(
        "PreserveSpeedQ", "Preserve 2-rune Q", true));
    ComboMenu->Add(new MenuBool(
        "WeaveAutos", "Leave safe cooldown gaps to", true));
    ComboMenu->Add(new MenuBool(
        "AssistManualReset", "Buffer Q after W/E", true));
    ComboMenu->Add(new MenuSlider(
        "HarassMana", "Min mana Q-E-Q (%)", 47, 0, 100));
    ComboMenu->Add(new MenuSeparator(
        "ResetTruth",
        "W and E reset Overload;"));

    FluxMenu = TacticsMenu->AddSubMenu(new Menu(
        "SpellFlux", "First-body Q, indirect E-Q and four-second marks"));
    FluxMenu->Add(new MenuList(
        "QHitchance", "Ordinary moving Q prediction",
        { "Medium", "High", "Very high", "Immobile only" }, 2));
    FluxMenu->Add(new MenuBool(
        "IndirectEQ", "E-Q minion bridge", true));
    FluxMenu->Add(new MenuSeparator(
        "FirstBody",
        "Every Q resolves moving"));

    PrisonMenu = TacticsMenu->AddSubMenu(new Menu(
        "RunePrison", "Root conversion, interrupt and protected-carry peel"));
    PrisonMenu->Add(new MenuBool(
        "Interrupt", "E-W root channels", true));
    PrisonMenu->Add(new MenuBool(
        "AntiGapcloser", "E-W-Q committed dash endpoints", true));
    PrisonMenu->Add(new MenuBool(
        "ProtectCarry", "Root the diver threatening", true));
    PrisonMenu->Add(new MenuSlider(
        "PeelDistance", "Max diver distance from ally", 525, 250, 750));
    PrisonMenu->Add(new MenuSeparator(
        "NoFakeRoot",
        "Unfluxed W is treated as a"));

    WaveMenu = TacticsMenu->AddSubMenu(new Menu(
        "Wave", "E-E-Q compression, exact Q last hits and jungle resets"));
    WaveMenu->Add(new MenuBool(
        "Lane", "Champion Flux wave logic", true));
    WaveMenu->Add(new MenuBool(
        "Jungle", "Reset branches jungle", true));
    WaveMenu->Add(new MenuSlider(
        "MinimumQHits", "Min Flux Q victims", 3, 1, 7));
    WaveMenu->Add(new MenuSlider(
        "MinimumEEQHits", "Min victims for E-E-Q", 4, 2, 8));
    WaveMenu->Add(new MenuSlider(
        "LaneMana", "Minimum E-E-Q lane mana (%)", 42, 0, 100));
    WaveMenu->Add(new MenuSlider(
        "LastHitMana", "Min mana exact Q (%)", 28, 0, 100));
    WaveMenu->Add(new MenuSlider(
        "JungleMana", "Minimum jungle mana (%)", 24, 0, 100));
    WaveMenu->Add(new MenuBool(
        "HoldForChampion", "Hold spells vs contest", true));
    WaveMenu->Add(new MenuSlider(
        "ChampionHoldRange", "Champion contest range", 1050, 500, 1600));
    WaveMenu->Add(new MenuSeparator(
        "WaveSurvival",
        "The chosen second-E and Q"));

    WarpMenu = TacticsMenu->AddSubMenu(new Menu(
        "RealmWarp", "Player-authorized endpoint and portal safety"));
    WarpMenu->Add(new MenuKeyBind(
        "ManualWarp", "Safe Realm Warp toward cursor [G]",
        SDK::Keys::G, KeyBindType::Press));
    WarpMenu->Add(new MenuBool(
        "EmergencyOptIn", "R escape lethal HP", false));
    WarpMenu->Add(new MenuSlider(
        "EmergencyHp", "Emergency R HP (%)", 17, 5, 50));
    WarpMenu->Add(new MenuBool(
        "UnsafeManual", "Allow manual turret/blind", false));
    WarpMenu->Add(new MenuSeparator(
        "NoAbduction",
        "R rejects walls, losing"));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "Ryze one-trick geometry and reset state"));
    CoachMenu->Add(new MenuBool(
        "DrawRanges", "Draw Q, W/E and ready-R ranges", true));
    CoachMenu->Add(new MenuBool(
        "DrawQ", "Draw Q line/first body", true));
    CoachMenu->Add(new MenuBool(
        "DrawFlux", "Draw Flux expiry", true));
    CoachMenu->Add(new MenuBool(
        "DrawProtected", "Mark protected ally", true));
    CoachMenu->Add(new MenuBool(
        "DrawWarp", "Draw last Warp dest", true));
    CoachMenu->Add(new MenuBool(
        "DrawState", "Draw posture/runes", true));
}

inline void OnLoad() {
    FluxRecords.fill({});
    EnemyWindows.fill({});
    Runes = {};
    CurrentPosture = Posture::Neutral;
    ActiveSequence = Sequence::None;
    ActiveBranch = ComboBranch::None;
    LastQPlan = {};
    LastWPlan = {};
    LastEPlan = {};
    LastWarpPlan = {};
    SequenceStep = SequenceTargetId = SequenceEPrimaryId = 0;
    SequenceQBodyId = SequenceExpireTick = 0;
    SequenceReactive = false;
    ProtectedAllyId = PeelThreatId = PeelThreatUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    InterruptTargetId = InterruptExpireTick = 0;
    IncomingCrowdControlUntil = 0;
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastBeforeAttackTargetId = LastBeforeAttackTick = 0;
    LastAfterAttackTargetId = LastAfterAttackTick = 0;
    LastLocalAutoTargetId = LastLocalAutoTick = 0;
    LastRegisteredCastTick.fill(0);
    LastRegisteredTargetId.fill(0);
    PlayerOverrideUntil = ManualResetTargetId = ManualResetCastTick = 0;
    ManualResetExpireTick = RootedTargetId = RootedUntilTick = 0;
    WarpChannelUntil = LastWeaveOpportunityTick = 0;
    RefreshRuntimeState();
}

inline void OnUnload() {
    TacticsMenu = ComboMenu = FluxMenu = PrisonMenu = nullptr;
    WaveMenu = WarpMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Pin Summoner's Rift Ryze behavior to Riot and CommunityDragon PC 16.14",
    "Reconcile Riot 25.11 Flux-Q amplification with the current ultimate ranks",
    "Reconcile Riot 25.13 Rune Prison damage and mana-cost changes",
    "Reconcile Riot 26.3 Rune Prison AP and bonus-mana ratios",
    "Reconcile Riot 26.12 base health 620 and base attack damage 55",
    "Use current base mana 300 and 70 mana growth",
    "Remove Arcane Mastery amplification before estimating bonus mana",
    "Apply ten-percent maximum-mana growth per 100 ability power",
    "Use Q base damage 75 through 155",
    "Use Q 55-percent AP and two-percent bonus-mana ratios",
    "Use W base damage 60 through 180",
    "Use W 60-percent AP and three-percent bonus-mana ratios",
    "Use E base damage 60 through 180",
    "Use E 50-percent AP and two-percent bonus-mana ratios",
    "Apply pre-six Flux-Q bonus of 25 percent",
    "Apply ultimate-rank Flux-Q bonuses of 50, 75 and 100 percent",
    "Use mitigated magic damage for every lethal decision",
    "Include generic shields in every exact lethal decision",
    "Do not call a spell lethal through spell shield or immunity",
    "Read live Q, W, E and R mana costs from the spellbook",
    "Price every combo branch independently",
    "Reserve E-W-Q mana during nonlethal harass",
    "Allow an all-in to spend the defensive reserve only in its selected branch",
    "Use Q cast delay 0.25 seconds",
    "Use Q missile speed 1700",
    "Use Q missile radius 55 plus target bounding radius",
    "Use Q range 1000 without fabricating extra center range",
    "Solve missile and target relative motion analytically",
    "Include Q cast delay before moving-body interception",
    "Predict every enemy champion velocity independently",
    "Predict every enemy lane-minion velocity independently",
    "Predict every jungle-monster velocity independently",
    "Clamp malformed observed velocity to plausible movement speed",
    "Ignore invalid, dead and untargetable Q bodies",
    "Sort first contact by earliest projectile time",
    "Break equal contact times deterministically by network id",
    "Reject a champion hidden behind a moving melee minion",
    "Reject a champion hidden behind a crossing caster minion",
    "Reject a champion hidden behind a moving jungle monster",
    "Allow a blocker that exits the Q corridor before contact",
    "Reject a blocker that enters the Q corridor at contact time",
    "Respect Yasuo, Samira and projectile-intercept walls",
    "Sample small Q angle offsets without changing the intended first body",
    "Require the intended direct champion to be the actual first body",
    "Require an indirect detonation minion to be the actual first body",
    "Never claim a Flux splash when Q first hits a different body",
    "Use high confidence for deterministic minion and monster Q bodies",
    "Use configurable prediction for ordinary moving champion Q",
    "Allow medium Q after a verified root, dash endpoint or peel emergency",
    "Preserve attack windup before ordinary Q",
    "Allow reactive or lethal Q to preempt an attack windup",
    "Respect player cursor direction for proactive champion Q",
    "Ignore cursor disagreement for explicit wave and objective Q",
    "Track each target's Spell Flux expiry independently",
    "Refresh Flux from confirmed buff add and update events",
    "Remove Flux immediately on confirmed buff removal",
    "Predict all E-spread targets immediately after a controller E",
    "Reconcile predicted Flux continuously with visible live buffs",
    "Use four seconds for predicted Flux duration",
    "Use E spread radius 350 on ordinary bodies",
    "Use E spread radius 400 on large bodies",
    "Use Flux-Q splash radius 350 on ordinary primaries",
    "Use Flux-Q splash radius 500 on large primaries",
    "Consume only the Flux victims reached by the chosen Q detonation",
    "Consume target Flux when W converts its slow into a root",
    "Keep unrelated targets' Flux state after a single-target W",
    "Track two Rune charges independently from Flux",
    "Add one Rune after every W",
    "Add one Rune after every E",
    "Cap Rune charges at two",
    "Refresh Rune expiry to four seconds after W or E",
    "Consume both Rune charges on the next Q",
    "Recognize the two-Rune Q movement-speed trigger",
    "Use current Q-rank movement speed 28 through 44 percent",
    "Reconcile Rune state from half-charge and full-charge buffs",
    "Clear Rune state from the Q movement-speed buff",
    "Do not confuse Rune stacks with Spell Flux targets",
    "Treat W and E as real Overload cooldown resets",
    "Allow E-Q while Q was cooling before E",
    "Allow E-W-Q while Q was cooling before E",
    "Allow W-Q-E-Q while Q was cooling before W",
    "Allow W-E-Q while Q was cooling before W",
    "Require initial Q readiness only for branches that actually start with Q",
    "Execute Q-E-Q as the W-preserving short trade",
    "Execute Q-E-W-Q so root lands before the travelling lead Q at max E range",
    "Execute E-W-Q as a fast root followed by two-Rune movement speed",
    "Execute W-Q-E-Q against an already Fluxed mobile target",
    "Execute Q-W-Q-E-Q when verified lethal rewards three Q casts",
    "Execute W-E-Q when immediate slow and two-Rune speed are required",
    "Execute Q-E-Q-W-Q-E-Q only in a full safe DPS window",
    "Do not select maximum-DPS cadence against an uncommitted mobile target",
    "Require target commitment, hard crowd control or spent mobility for full DPS",
    "Reject full DPS under enemy turret unless the branch is lethal",
    "Reject full DPS into losing local numbers unless lethal",
    "Reject full DPS into ready point-click lockdown at low health",
    "Track enemy committed casts as punish windows",
    "Track enemy hard-crowd-control casts as spent windows",
    "Track known mobility spell casts as spent windows",
    "Track gapcloser callbacks as both commitment and spent mobility",
    "Include ready Flash in mobile-target root priority",
    "Use a champion-and-slot mobility registry rather than name substring guesses",
    "Prefer immediate Flux-W against a mobile target",
    "Do not pretend unfluxed W is a root",
    "Use unfluxed W only as an intentional slow/reset branch",
    "Avoid overlapping ordinary W root with existing hard crowd control",
    "Allow lethal W to overlap crowd control",
    "Hold W against spell shield and immunity",
    "Use E-W root for interrupt only when the channel has enough time remaining",
    "Use immediate Flux-W for a channel with a short remaining window",
    "Never report an unfluxed W slow as a successful interrupt",
    "Use E-W-Q on a committed gapcloser endpoint",
    "Use immediate Flux-W-Q-E-Q against an already marked diver",
    "Select the allied champion most expensive to leave unprotected",
    "Track enemy targeted pressure against allied champions",
    "Prefer protected-carry peel before proactive combo",
    "Root a diver moving closer to the protected ally",
    "Do not peel an enemy outside the configured protection radius",
    "Use W slow as last-resort peel when E is unavailable",
    "Keep Orbwalker ownership of every basic attack",
    "Never issue a movement order to create spell range",
    "Never issue an attack order to force an auto weave",
    "Expose safe cooldown gaps so Orbwalker can weave an auto",
    "Do not weave while an unrooted target has instant escape",
    "Allow a rooted target to create a safe auto-weave window",
    "Do not weave inside the W/E-to-Q buffer window",
    "Do not weave before a lethal Q",
    "Do not weave before critical peel",
    "Preserve a player-issued attack even near the next reset",
    "Track before-attack, after-attack and local auto events separately",
    "Yield controller ownership after every player spell cast",
    "Clear an automatic sequence when the player changes the spell cadence",
    "Optionally assist a player W with one clean buffered Q",
    "Optionally assist a player E with one clean buffered Q",
    "Never change the aim of a player-cast Q",
    "Never auto-assist a manual W or E cast onto a nonchampion",
    "Expire manual reset assistance after a bounded window",
    "Keep Flash, movement, target selection and attack-move player-owned",
    "Find an indirect E primary inside 550 range",
    "Require indirect E to mark the priority champion",
    "Require the chosen indirect Q body to survive E damage",
    "Require indirect Q splash to reach the priority champion",
    "Score multi-mark and multi-victim indirect bridges",
    "Prefer a stable large detonation body when equivalent",
    "Use indirect E-Q only when its complete mana cost is affordable",
    "Preserve peel reserve during indirect harass",
    "Fall back to direct Q when no valid wave bridge exists",
    "Use an existing Flux detonation before spending another E in waveclear",
    "Require configurable victims for an existing-Flux wave Q",
    "Select E-E-Q only when configurable wave compression is met",
    "Keep the second E target alive after the first E",
    "Retarget the second E to the surviving Q detonation body",
    "Wait for the real second-E cooldown instead of fabricating readiness",
    "Keep E-E-Q sequence alive long enough for real rank-dependent cooldown",
    "Preserve a large lane minion when E would create a worse detonation",
    "Reward an exact E last hit when it does not destroy the plan",
    "Use exact mitigated Q damage for last hitting",
    "Do not spend W on lane minions",
    "Hold wave spells when an enemy champion contests",
    "Use separate mana thresholds for lane clear and last hit",
    "Prioritize epic jungle monsters in target selection",
    "Use full reset cadence on uncontested epic objectives",
    "Preserve W in jungle when an enemy champion is nearby",
    "Use Q-E-Q or E-Q against ordinary jungle targets",
    "Resolve lane minions as real blockers to jungle Q",
    "Resolve jungle monsters as real blockers to lane Q",
    "Use exact Q, E and W damage independently for kill secure",
    "Try lethal clean Q before shorter-range targeted spells",
    "Use lethal E when Q is blocked and the target is in range",
    "Use lethal W when Q and E cannot secure",
    "Start lethal E-Q when E will reset a cooling Q",
    "Do not interrupt an active champion sequence with opportunistic kill secure",
    "Clamp Realm Warp cursor endpoint to 3000 range",
    "Reject Realm Warp cursor endpoints inside the 1000 minimum",
    "Use portal radius 365 for allied occupancy",
    "Include allied bounding radius when evaluating portal occupants",
    "Require explicit player key authorization for ordinary Realm Warp",
    "Keep automatic emergency Realm Warp disabled by default",
    "Require explicit opt-in and lethal-health danger for emergency Realm Warp",
    "Use the player's cursor even for opted-in emergency escape",
    "Reject Realm Warp endpoint inside NavMesh wall",
    "Reject Realm Warp while rooted or grounded",
    "Reject Realm Warp channel into likely incoming crowd control",
    "Reject Realm Warp origin near ready point-click lockdown",
    "Reject an enemy-turret destination by default",
    "Reject losing enemy-versus-ally numbers at destination",
    "Reject a blind destination containing a known enemy",
    "Use allied heroes as destination vision evidence",
    "Use allied minions as destination vision evidence",
    "Use allied turrets as destination vision evidence",
    "Use allied wards as destination vision evidence",
    "Use visible enemy presence as destination vision evidence",
    "Reject teleporting an allied protected channel inside the portal",
    "Recognize common allied channeled ultimates and Meditate",
    "Permit explicit unsafe manual override only for endpoint danger gates",
    "Do not let unsafe override bypass walls or rooted state",
    "Yield all spell logic throughout Realm Warp charge and teleport phase",
    "Track both player-cast and controller-cast Realm Warp channels",
    "Draw Q's selected corridor and actual moving first body",
    "Distinguish direct Q and indirect Flux bridge colors",
    "Draw confirmed and predicted Flux with remaining duration",
    "Draw current Rune count without reading mana as Rune state",
    "Draw the dynamically protected ally",
    "Draw last Warp dest",
    "Expose posture, exact branch and player/controller ownership",
    "Own Ryze's complete spell loop without generic Q-W-E-R fallback",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Ryze;
    controller.ControllerId = "champion.kuroaio.ai.ryze.controller";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIRyze.md";
    controller.ImplementationSummary =
        "Analytical moving first-body Q, independent four-second Flux and "
        "two-Rune ledgers, indirect wave E-Q, reset-aware QEQWQEQ/QEWQ/EWQ "
        "branching, protected-carry roots, delayed EEQ clear, player-buffered "
        "resets, and explicitly authorized no-abduction Realm Warp safety.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &OnProcessSpell;
    controller.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackEvent<&LastBeforeAttackTargetId, &LastBeforeAttackTick>;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAfterAttackTargetId, &LastAfterAttackTick>;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Ryze
