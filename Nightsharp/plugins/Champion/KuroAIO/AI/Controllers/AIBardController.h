#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIBardGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Bard {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::CursorDirectionAgrees;
using ControllerHelpers::HasCurrentResource;
using ControllerHelpers::HasEnemyChampionNear;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HasResourceFor;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::ObjectEventIsAllied;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocks;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContainsAny;
using ControllerHelpers::SpellRank;
using ControllerHelpers::UnitByNetworkId;

enum class Posture : std::uint8_t {
    Neutral,
    LaneGuard,
    Catch,
    FrontToBack,
    Peel,
    Roam,
    Objective,
    Dive,
    Disengage,
    Farm,
};

enum class Sequence : std::uint8_t {
    None,
    MeepQWeave,
    StasisExitQ,
    PortalExitQ,
    PortalBait,
    DiveReset,
    SaveStasis,
    ObjectiveDeny,
    PlayerLed,
};

enum class QPurpose : std::uint8_t {
    None,
    WallStun,
    DoubleStun,
    MeepFollowup,
    StasisExit,
    PortalExit,
    Peel,
    Interrupt,
    AntiGapcloser,
    KillSecure,
    Flee,
    LaneDoubleHit,
    Jungle,
};

enum class WPurpose : std::uint8_t {
    None,
    EmergencyHeal,
    CombatSpeed,
    RetreatSpeed,
    LaneSustain,
    PreObjectiveShrine,
    RetreatShrine,
    VisionProbe,
};

enum class EPurpose : std::uint8_t {
    None,
    DefensiveEscape,
    CarryEscape,
    PlayerCommit,
    RoamAccess,
    PortalBait,
    ObjectiveAccess,
};

enum class RPurpose : std::uint8_t {
    None,
    Catch,
    BacklineIsolation,
    Peel,
    AllySave,
    DiveTurret,
    ObjectiveDeny,
    EnemyJunglerDeny,
    Interrupt,
    Flee,
    PlantDeny,
};

struct QPlan {
    QEvaluation Evaluation = {};
    int TargetId = 0;
    QPurpose Purpose = QPurpose::None;
    SDK::HitChance Hitchance = SDK::HitChance::None;
    float RawDamage = 0.0f;
    bool Lethal = false;
    bool Guaranteed = false;
    bool Valid = false;
};

struct WPlan {
    Vector3 CastPosition = {};
    int TargetId = 0;
    WPurpose Purpose = WPurpose::None;
    float Score = -FLT_MAX;
    bool DirectTarget = false;
    bool ReserveOverride = false;
    bool Valid = false;
};

struct EPlan {
    PortalTrace Portal = {};
    EPurpose Purpose = EPurpose::None;
    float Score = -FLT_MAX;
    bool Defensive = false;
    bool PlayerRequested = false;
    bool Valid = false;
};

struct RPlan {
    REvaluation Evaluation = {};
    int PrimaryId = 0;
    RPurpose Purpose = RPurpose::None;
    int ExpectedImpactTick = 0;
    bool Manual = false;
    bool Valid = false;
};

struct ShrineRecord {
    int NetworkId = 0;
    Vector3 Position = {};
    int SpawnTick = 0;
    int LastSeenTick = 0;
    bool ConfirmedOwned = false;
};

struct ChimeRecord {
    int NetworkId = 0;
    Vector3 Position = {};
    int SpawnTick = 0;
    int ExpireTick = 0;
    bool Visible = false;
};

struct PortalRecord {
    int NetworkId = 0;
    PortalTrace Geometry = {};
    int SpawnTick = 0;
    int ExpireTick = 0;
    bool ConfirmedOwned = false;
};

struct StasisRecord {
    int NetworkId = 0;
    int StartTick = 0;
    int EndTick = 0;
    bool Allied = false;
    bool FromRecentBardR = false;
};

struct FocusRecord {
    int NetworkId = 0;
    int AllyNetworkId = 0;
    int UntilTick = 0;
    float Pressure = 0.0f;
};

struct AllyThreatRecord {
    int NetworkId = 0;
    int SourceNetworkId = 0;
    int UntilTick = 0;
    int ImpactTick = 0;
    float Pressure = 0.0f;
    bool HardCrowdControl = false;
    bool Targeted = false;
};

inline Menu* TacticsMenu = nullptr;
inline Menu* RoleMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<ShrineRecord, 8> Shrines = {};
inline std::array<ChimeRecord, 40> Chimes = {};
inline std::array<PortalRecord, 6> Portals = {};
inline std::array<StasisRecord, 20> StasisRecords = {};
inline std::array<FocusRecord, 20> FocusRecords = {};
inline std::array<AllyThreatRecord, 12> AllyThreats = {};

inline Posture CurrentPosture = Posture::Neutral;
inline Sequence ActiveSequence = Sequence::None;
inline QPurpose LastQPurpose = QPurpose::None;
inline WPurpose LastWPurpose = WPurpose::None;
inline EPurpose LastEPurpose = EPurpose::None;
inline RPurpose LastRPurpose = RPurpose::None;
inline QPlan LastQPlan = {};
inline WPlan LastWPlan = {};
inline EPlan LastEPlan = {};
inline RPlan LastRPlan = {};

inline int ChimeCount = 0;
inline int MeepAmmo = 0;
inline int MeepMaximum = 1;
inline int ProtectedAllyId = 0;
inline int LastBeforeAttackTargetId = 0;
inline int LastBeforeAttackTick = 0;
inline bool LastBeforeAttackHadMeep = false;
inline int LastAfterAttackTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int LastMeepAttackTargetId = 0;
inline int LastMeepAttackTick = 0;
inline int LastLocalAutoTargetId = 0;
inline int LastLocalAutoTick = 0;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int PlayerOverrideUntil = 0;
inline int SequenceTargetId = 0;
inline int SequenceExpireTick = 0;
inline int PortalTravellerId = 0;
inline int PortalTravellerUntil = 0;
inline Vector3 PortalTravellerExit = {};
inline Vector3 PendingWPosition = {};
inline Vector3 PendingPortalCast = {};
inline PortalTrace PendingPortalGeometry = {};
inline int PendingPortalUntil = 0;
inline int PendingRPrimaryId = 0;
inline int PendingRImpactTick = 0;
inline bool PendingRManual = false;

inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;

inline constexpr int kManualOwnershipMs = 320;
inline constexpr int kMeepQWindowMs = 950;
inline constexpr int kPortalTravellerGraceMs = 1300;
inline constexpr int kStasisEventGraceMs = 700;

inline bool IsQEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
           SpellEventNameContainsAny(args, {
               "bardq", "cosmicbinding",
           });
}

inline bool IsWEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
           SpellEventNameContainsAny(args, {
               "bardw", "caretakersshrine", "bardwdirectheal",
           });
}

inline bool IsEEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
           SpellEventNameContainsAny(args, {
               "barde", "magicaljourney", "bardecreatedoor",
           });
}

inline bool IsREvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
           SpellEventNameContainsAny(args, {
               "bardr", "temperedfate",
           });
}

inline bool IsShrineObject(const SDK::Events::ObjectEventArgs& args) {
    return ControllerHelpers::AnyTextContains(
        { args.Sender.Name, args.Sender.CharacterName,
          args.SpellName, args.MissileName },
        { "bardwhealthpack", "bardwshrine", "caretakersshrine" });
}

inline bool IsChimeObject(const SDK::Events::ObjectEventArgs& args) {
    return ControllerHelpers::AnyTextContains(
        { args.Sender.Name, args.Sender.CharacterName,
          args.SpellName },
        { "bardpchime", "bardchime", "bard_chime" });
}

inline bool IsPortalObject(const SDK::Events::ObjectEventArgs& args) {
    return ControllerHelpers::AnyTextContains(
        { args.Sender.Name, args.Sender.CharacterName,
          args.SpellName },
        { "bardecreatedoor", "bardportal", "magicaljourney" });
}

template <typename Record, std::size_t N>
inline Record* RecordById(std::array<Record, N>& records,
                          int networkId) {
    if (networkId == 0) return nullptr;
    for (auto& record : records) {
        if (record.NetworkId == networkId) return &record;
    }
    return nullptr;
}

template <typename Record, std::size_t N>
inline Record* EmptyOrOldest(std::array<Record, N>& records,
                             int now) {
    for (auto& record : records) {
        if (record.NetworkId == 0) return &record;
    }
    return &*std::min_element(
        records.begin(), records.end(),
        [now](const Record& left, const Record& right) {
            const int leftAge = left.LastSeenTick > 0
                ? left.LastSeenTick : now;
            const int rightAge = right.LastSeenTick > 0
                ? right.LastSeenTick : now;
            return leftAge < rightAge;
        });
}

inline ShrineRecord* FindShrine(int id, bool create = false) {
    if (auto* record = RecordById(Shrines, id)) return record;
    if (!create || id == 0) return nullptr;
    ShrineRecord* record = EmptyOrOldest(Shrines, Now());
    *record = {};
    record->NetworkId = id;
    return record;
}

inline ChimeRecord* FindChime(int id, bool create = false) {
    if (auto* record = RecordById(Chimes, id)) return record;
    if (!create || id == 0) return nullptr;
    for (auto& record : Chimes) {
        if (record.NetworkId == 0 || record.ExpireTick < Now()) {
            record = {};
            record.NetworkId = id;
            return &record;
        }
    }
    auto* record = &*std::min_element(
        Chimes.begin(), Chimes.end(),
        [](const ChimeRecord& left, const ChimeRecord& right) {
            return left.ExpireTick < right.ExpireTick;
        });
    *record = {};
    record->NetworkId = id;
    return record;
}

inline PortalRecord* FindPortal(int id, bool create = false) {
    if (auto* record = RecordById(Portals, id)) return record;
    if (!create || id == 0) return nullptr;
    for (auto& record : Portals) {
        if (record.NetworkId == 0 || record.ExpireTick < Now()) {
            record = {};
            record.NetworkId = id;
            return &record;
        }
    }
    return nullptr;
}

inline StasisRecord* FindStasis(int id, bool create = false) {
    if (auto* record = RecordById(StasisRecords, id)) return record;
    if (!create || id == 0) return nullptr;
    for (auto& record : StasisRecords) {
        if (record.NetworkId == 0 || record.EndTick < Now()) {
            record = {};
            record.NetworkId = id;
            return &record;
        }
    }
    return nullptr;
}

inline FocusRecord* FindFocus(int id, bool create = false) {
    if (auto* record = RecordById(FocusRecords, id)) return record;
    if (!create || id == 0) return nullptr;
    for (auto& record : FocusRecords) {
        if (record.NetworkId == 0 || record.UntilTick < Now()) {
            record = {};
            record.NetworkId = id;
            return &record;
        }
    }
    return nullptr;
}

inline AllyThreatRecord* FindAllyThreat(int id, bool create = false) {
    if (auto* record = RecordById(AllyThreats, id)) return record;
    if (!create || id == 0) return nullptr;
    for (auto& record : AllyThreats) {
        if (record.NetworkId == 0 || record.UntilTick < Now()) {
            record = {};
            record.NetworkId = id;
            return &record;
        }
    }
    return nullptr;
}

inline bool FocusedByAlly(int enemyId) {
    const FocusRecord* record = RecordById(FocusRecords, enemyId);
    return record && record->UntilTick >= Now();
}

inline AllyThreatRecord CurrentAllyThreat(int allyId) {
    const AllyThreatRecord* record = RecordById(AllyThreats, allyId);
    return record && record->UntilTick >= Now()
        ? *record : AllyThreatRecord{};
}

inline int RuntimeWAmmo() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::W);
    if (spell.IsValid() && spell.MaxAmmo() > 0) {
        return std::clamp(spell.Ammo(), 0, spell.MaxAmmo());
    }
    return Ready(1) ? 1 : 0;
}

inline int ActiveShrineCount() {
    int count = 0;
    for (const auto& shrine : Shrines) {
        if (shrine.NetworkId != 0 && shrine.ConfirmedOwned) ++count;
    }
    return count;
}

inline std::vector<Shrine> GeometryShrines() {
    std::vector<Shrine> result;
    result.reserve(Shrines.size());
    const int now = Now();
    for (const auto& record : Shrines) {
        if (record.NetworkId == 0 || !record.Position.IsValid() ||
            record.Position.IsZero()) {
            continue;
        }
        Shrine shrine{};
        shrine.Position = record.Position;
        shrine.Id = record.NetworkId;
        shrine.AgeSeconds = std::max(
            0.0f, static_cast<float>(now - record.SpawnTick) / 1000.0f);
        shrine.ConfirmedOwned = record.ConfirmedOwned;
        shrine.Valid = true;
        result.push_back(shrine);
    }
    return result;
}

inline bool MeepAvailable() {
    const auto player = GameObjects::Player();
    return MeepAmmo > 0 ||
           (player.IsValid() &&
            (player.HasBuff("BardPSpirits") ||
             player.HasBuff("BardPSpiritAmmoCount")));
}

inline float EnemyPriority(const AIHeroClient& enemy) {
    if (!Engine::ValidEnemy(enemy)) return 0.0f;
    return 1.0f + std::min(2.5f,
        enemy.TotalAttackDamage() / 180.0f + enemy.AP() / 280.0f);
}

inline bool TargetRejectsQ(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) ||
           IsCommonUntargetableOrImmune(target) ||
           target.HasBuff("FioraW") ||
           target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzE") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("EliseSpiderE");
}

inline std::vector<QUnit> BuildQUnits(float predictionSeconds) {
    std::vector<QUnit> units;
    units.reserve(40);
    const auto appendHero = [&](const AIHeroClient& hero) {
        if (!Engine::ValidEnemy(hero, kQInitialTargetRange +
                kQContinuationDistance + 260.0f)) return;
        QUnit unit{};
        unit.Position = hero.Position();
        unit.PredictedPosition = PredictPosition(hero, predictionSeconds);
        unit.Radius = hero.BoundingRadius();
        unit.Priority = EnemyPriority(hero);
        unit.Id = static_cast<int>(hero.NetworkId());
        unit.Hostile = unit.Champion = unit.Targetable = unit.Valid = true;
        unit.SpellShield = HasSpellShieldOrImmunity(hero);
        unit.HardCrowdControlled = Engine::IsHardCrowdControlled(hero);
        units.push_back(unit);
    };
    const auto appendMinion = [&](const AIMinionClient& minion,
                                  bool jungle) {
        const auto player = GameObjects::Player();
        if (!player.IsValid() || !minion.IsValid() || minion.IsDead() ||
            !minion.IsTargetable() ||
            player.Position().Distance2D(minion.Position()) >
                kQInitialTargetRange + kQContinuationDistance + 220.0f) {
            return;
        }
        QUnit unit{};
        unit.Position = minion.Position();
        unit.PredictedPosition = PredictPosition(minion, predictionSeconds);
        unit.Radius = minion.BoundingRadius();
        unit.Priority = IsEpicMonster(minion) ? 2.2f : 0.45f;
        unit.Id = static_cast<int>(minion.NetworkId());
        unit.Hostile = true;
        unit.Minion = !jungle;
        unit.Monster = jungle;
        unit.Targetable = unit.Valid = true;
        units.push_back(unit);
    };
    for (const auto& hero : GameObjects::EnemyHeroes()) appendHero(hero);
    for (const auto& minion : GameObjects::EnemyMinions()) {
        appendMinion(minion, false);
    }
    for (const auto& monster : GameObjects::Jungle()) {
        appendMinion(monster, true);
    }
    return units;
}

inline std::vector<Vector3> TerrainSamplesForQ(const Vector3& origin,
                                               const Vector3& aim) {
    std::vector<Vector3> samples;
    const Vector3 direction = SharedGeometry::Direction2D(origin, aim);
    if (direction.IsZero()) return samples;
    for (float distance = 24.0f;
         distance <= kQInitialTargetRange + kQContinuationDistance;
         distance += 12.0f) {
        Vector3 sample = origin + direction * distance;
        if (SDK::NavMesh::IsWall(sample)) samples.push_back(sample);
    }
    return samples;
}

inline SDK::HitChance RequiredQHitchance(QPurpose purpose,
                                         const AIHeroClient& target) {
    if (Engine::IsHardCrowdControlled(target)) return SDK::HitChance::Immobile;
    if (target.IsDashing() || purpose == QPurpose::Interrupt ||
        purpose == QPurpose::AntiGapcloser ||
        purpose == QPurpose::Peel ||
        purpose == QPurpose::StasisExit ||
        purpose == QPurpose::PortalExit) {
        return SDK::HitChance::High;
    }
    SDK::HitChance baseChance = SDK::HitChance::High;
    switch (List(QMenu, "Hitchance", 1)) {
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

inline std::vector<Vector3> QCandidates(const AIHeroClient& target) {
    std::vector<Vector3> candidates;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return candidates;
    const auto prediction = Engine::RuntimeSpells[0]
        ? Engine::RuntimeSpells[0]->GetPrediction(target)
        : SDK::PredictionOutput{};
    Vector3 predicted = prediction.GetCastPosition();
    if (!predicted.IsValid() || predicted.IsZero()) {
        predicted = PredictPosition(target, kQCastSeconds + 0.28f);
    }
    Vector3 direct = SharedGeometry::Direction2D(
        player.Position(), predicted);
    if (direct.IsZero()) return candidates;
    static constexpr std::array<float, 11> offsets = {
        0.0f, 0.018f, -0.018f, 0.035f, -0.035f,
        0.060f, -0.060f, 0.095f, -0.095f, 0.135f, -0.135f,
    };
    for (float offset : offsets) {
        const Vector3 direction = SharedGeometry::Rotate2D(direct, offset);
        if (!direction.IsZero()) {
            candidates.push_back(
                player.Position() + direction * kQInitialTargetRange);
        }
    }

    const auto units = BuildQUnits(kQCastSeconds + 0.35f);
    for (const auto& unit : units) {
        if (!unit.Valid || unit.Id == static_cast<int>(target.NetworkId())) {
            continue;
        }
        const Vector3 aim = PairAlignmentAim(
            player.Position(), predicted, unit.PredictedPosition);
        if (aim.IsValid() && !aim.IsZero()) candidates.push_back(aim);
    }

    // Tangential Q contact is real Bard tech: touching the side of the first
    // body delays Q2's origin and can gain the final few continuation units.
    const Vector3 perpendicular{ -direct.z, 0.0f, direct.x };
    const float side = target.BoundingRadius() * 0.72f;
    candidates.push_back(predicted + perpendicular * side);
    candidates.push_back(predicted - perpendicular * side);

    // Wall candidates are aimed through the first body toward actual terrain,
    // never at a generic point merely because the target is near a wall.
    for (int sample = 0; sample < 24; ++sample) {
        const float angle = 2.0f * kPi *
            static_cast<float>(sample) / 24.0f;
        const Vector3 wall{
            predicted.x + std::cos(angle) * 280.0f,
            predicted.y,
            predicted.z + std::sin(angle) * 280.0f,
        };
        if (!SDK::NavMesh::IsWall(wall)) continue;
        const Vector3 aim = PairAlignmentAim(
            player.Position(), predicted, wall);
        if (aim.IsValid() && !aim.IsZero()) candidates.push_back(aim);
    }
    return candidates;
}

inline QPlan BuildQPlan(const AIHeroClient& target,
                        QPurpose purpose,
                        bool requireStun = true) {
    QPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0) || TargetRejectsQ(target)) return best;
    if (player.Position().Distance2D(target.Position()) >
            kQInitialTargetRange + target.BoundingRadius() +
                kQHalfWidth + 35.0f) {
        return best;
    }
    SDK::HitChance observed = SDK::HitChance::High;
    if (Engine::RuntimeSpells[0]) {
        observed = Engine::RuntimeSpells[0]->GetPrediction(target).Hitchance;
    }
    const SDK::HitChance required = RequiredQHitchance(purpose, target);
    if (static_cast<int>(observed) < static_cast<int>(required) &&
        !Engine::IsHardCrowdControlled(target) && !target.IsDashing()) {
        return best;
    }
    const float raw = QRawDamage(SpellRank(0), player.AP());
    const float dealt = player.CalculateMagicDamage(target, raw);
    const bool lethal = dealt >= target.Health() + 4.0f;
    const auto candidates = QCandidates(target);
    for (const Vector3& aim : candidates) {
        if (!aim.IsValid() || aim.IsZero() ||
            ProjectileWallBlocks(player.Position(), aim, kQHalfWidth)) {
            continue;
        }
        const float impact = kQCastSeconds +
            std::max(0.0f,
                player.Position().Distance2D(target.Position()) -
                target.BoundingRadius() - kQHalfWidth) / kQMissileSpeed;
        const auto units = BuildQUnits(impact);
        QEvaluation evaluation = EvaluateCosmicBinding(
            player.Position(), aim, units,
            TerrainSamplesForQ(player.Position(), aim),
            static_cast<int>(target.NetworkId()));
        if (!evaluation.Valid) continue;
        const bool stun = evaluation.FirstStunned;
        if (requireStun && !stun && !lethal &&
            purpose != QPurpose::MeepFollowup &&
            purpose != QPurpose::Jungle) {
            continue;
        }
        float score = evaluation.Score;
        if (stun) score += 260.0f;
        if (evaluation.SecondStunned) score += 190.0f;
        if (lethal) score += 520.0f;
        if (purpose == QPurpose::StasisExit ||
            purpose == QPurpose::PortalExit) score += 310.0f;
        if (purpose == QPurpose::Peel ||
            purpose == QPurpose::Interrupt) score += 220.0f;
        if (!best.Valid || score > best.Evaluation.Score) {
            evaluation.Score = score;
            best.Evaluation = evaluation;
            best.TargetId = static_cast<int>(target.NetworkId());
            best.Purpose = purpose;
            best.Hitchance = observed;
            best.RawDamage = raw;
            best.Lethal = lethal;
            best.Guaranteed = Engine::IsHardCrowdControlled(target) ||
                              target.IsDashing();
            best.Valid = true;
        }
    }
    return best;
}

inline bool CastQPlan(const QPlan& plan, bool reactive = false) {
    if (!plan.Valid || !Ready(0) ||
        !ControllerHelpers::CastThrottleReady(
            0, 34, reactive ? 0 : -1)) return false;
    if (Engine::ControllerCastPosition(
            0, plan.Evaluation.CastPosition)) {
        LastQPlan = plan;
        LastQPurpose = plan.Purpose;
        LastQCastTick = Now();
        return true;
    }
    return false;
}

inline AIHeroClient RawEnemyById(int networkId) {
    if (networkId == 0) return {};
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (static_cast<int>(enemy.NetworkId()) == networkId &&
            enemy.IsValid() && !enemy.IsDead()) {
            return enemy;
        }
    }
    return {};
}

inline AIHeroClient RawAllyById(int networkId) {
    if (networkId == 0) return {};
    const auto player = GameObjects::Player();
    if (player.IsValid() && player.NetworkId() == networkId) return player;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (static_cast<int>(ally.NetworkId()) == networkId &&
            ally.IsValid() && !ally.IsDead()) {
            return ally;
        }
    }
    return {};
}

inline QPlan BuildAnchoredQPlan(int targetId,
                               const Vector3& targetPosition,
                               QPurpose purpose,
                               bool perfectStasisExit) {
    QPlan best{};
    const auto player = GameObjects::Player();
    const AIHeroClient target = RawEnemyById(targetId);
    if (!player.IsValid() || !target.IsValid() || target.IsDead() ||
        !Ready(0) || !targetPosition.IsValid() || targetPosition.IsZero()) {
        return best;
    }
    if (player.Position().Distance2D(targetPosition) >
            kQInitialTargetRange + target.BoundingRadius() + kQHalfWidth) {
        return best;
    }
    std::vector<Vector3> candidates;
    const Vector3 direct = SharedGeometry::Direction2D(
        player.Position(), targetPosition);
    if (direct.IsZero()) return best;
    static constexpr std::array<float, 9> offsets = {
        0.0f, 0.018f, -0.018f, 0.038f, -0.038f,
        0.070f, -0.070f, 0.115f, -0.115f,
    };
    for (float offset : offsets) {
        const Vector3 direction = SharedGeometry::Rotate2D(direct, offset);
        if (!direction.IsZero()) {
            candidates.push_back(
                player.Position() + direction * kQInitialTargetRange);
        }
    }
    auto units = BuildQUnits(0.0f);
    units.erase(std::remove_if(
        units.begin(), units.end(),
        [&](const QUnit& unit) { return unit.Id == targetId; }),
        units.end());
    QUnit frozen{};
    frozen.Position = frozen.PredictedPosition = targetPosition;
    frozen.Radius = target.BoundingRadius();
    frozen.Priority = EnemyPriority(target);
    frozen.Id = targetId;
    frozen.Hostile = frozen.Champion = frozen.Targetable = frozen.Valid = true;
    units.push_back(frozen);
    for (const auto& unit : units) {
        if (unit.Id == targetId || !unit.Valid) continue;
        const Vector3 aim = PairAlignmentAim(
            player.Position(), targetPosition, unit.PredictedPosition);
        if (aim.IsValid() && !aim.IsZero()) candidates.push_back(aim);
    }
    for (int sample = 0; sample < 24; ++sample) {
        const float angle = 2.0f * kPi *
            static_cast<float>(sample) / 24.0f;
        const Vector3 wall{
            targetPosition.x + std::cos(angle) * 285.0f,
            targetPosition.y,
            targetPosition.z + std::sin(angle) * 285.0f,
        };
        if (!SDK::NavMesh::IsWall(wall)) continue;
        const Vector3 aim = PairAlignmentAim(
            player.Position(), targetPosition, wall);
        if (aim.IsValid() && !aim.IsZero()) candidates.push_back(aim);
    }
    const float raw = QRawDamage(SpellRank(0), player.AP());
    for (const Vector3& aim : candidates) {
        if (ProjectileWallBlocks(player.Position(), aim, kQHalfWidth)) continue;
        QEvaluation evaluation = EvaluateCosmicBinding(
            player.Position(), aim, units,
            TerrainSamplesForQ(player.Position(), aim), targetId);
        if (!evaluation.Valid) continue;
        evaluation.PerfectStasisExit = perfectStasisExit;
        evaluation.Score += evaluation.FirstStunned ? 520.0f : 260.0f;
        if (!best.Valid || evaluation.Score > best.Evaluation.Score) {
            best.Evaluation = evaluation;
            best.TargetId = targetId;
            best.Purpose = purpose;
            best.Hitchance = SDK::HitChance::Immobile;
            best.RawDamage = raw;
            best.Lethal = player.CalculateMagicDamage(target, raw) >=
                          target.Health() + 4.0f;
            best.Guaranteed = true;
            best.Valid = true;
        }
    }
    return best;
}

inline QPlan BuildStasisExitQPlan(const StasisRecord& stasis) {
    if (stasis.EndTick <= Now()) return {};
    const AIHeroClient target = RawEnemyById(stasis.NetworkId);
    if (!target.IsValid()) return {};
    return BuildAnchoredQPlan(
        stasis.NetworkId, target.Position(), QPurpose::StasisExit, true);
}

inline QPlan BuildPortalExitQPlan(int targetId,
                                 const Vector3& exitPosition) {
    return BuildAnchoredQPlan(
        targetId, exitPosition, QPurpose::PortalExit, false);
}

inline WPlan BuildEmergencyWPlan() {
    WPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || RuntimeWAmmo() <= 0 ||
        !HasCurrentResource(SpellCost(1))) return best;
    const float heal = WMinimumRawHeal(SpellRank(1), player.AP());
    const auto evaluate = [&](const AIHeroClient& ally) {
        if (!ally.IsValid() || ally.IsDead() || !ally.IsTargetable() ||
            ally.IsEnemy()) return;
        const float distance = player.Position().Distance2D(ally.Position());
        if (distance > kWCastRange + ally.BoundingRadius() + 60.0f) return;
        const AllyThreatRecord threat = CurrentAllyThreat(
            static_cast<int>(ally.NetworkId()));
        EmergencyHealContext context{};
        context.HealthPercent = ally.HealthPercent();
        context.IncomingDamage = threat.Pressure;
        context.CurrentHealth = ally.Health();
        context.Distance = distance;
        context.Priority = ally.NetworkId() == player.NetworkId()
            ? 1.2f : std::max(
                0.8f, ControllerHelpers::AllyProtectionPriority(ally) / 250.0f);
        context.Targeted = threat.Targeted && threat.UntilTick >= Now();
        context.HardCrowdControlled = threat.HardCrowdControl ||
            SDK::HasBuffOfType(ally, SDK::BuffType::Stun) ||
            SDK::HasBuffOfType(ally, SDK::BuffType::Snare);
        context.HasGrievousWounds = ControllerHelpers::HasAnyBuff(ally, {
            "GrievousWounds", "grievouswounds", "summonerdot",
            "MortalReminderDebuff", "MorellonomiconDebuff",
        });
        float score = EmergencyHealScore(context, heal);
        const float threshold = static_cast<float>(
            Slider(WMenu, "EmergencyScore", 520));
        const bool critical = ally.HealthPercent() <=
                static_cast<float>(Slider(WMenu, "EmergencyHp", 34)) ||
            (context.Targeted && threat.Pressure >= ally.Health() * 0.70f);
        if ((!critical && score < threshold) || score <= best.Score) return;
        best.TargetId = static_cast<int>(ally.NetworkId());
        best.CastPosition = ally.Position();
        best.Purpose = WPurpose::EmergencyHeal;
        best.Score = score;
        best.DirectTarget = true;
        best.ReserveOverride = true;
        best.Valid = true;
    };
    evaluate(player);
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (ally.NetworkId() != player.NetworkId()) evaluate(ally);
    }
    return best;
}

inline WPlan BuildCombatWPlan(const AIHeroClient& target,
                              bool retreat) {
    WPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || RuntimeWAmmo() <= 0 ||
        !HasCurrentResource(SpellCost(1))) return plan;
    AIHeroClient receiver = player;
    const AIHeroClient protectedAlly = RawAllyById(ProtectedAllyId);
    if (protectedAlly.IsValid() &&
        player.Position().Distance2D(protectedAlly.Position()) <= kWCastRange &&
        (retreat || protectedAlly.HealthPercent() < player.HealthPercent())) {
        receiver = protectedAlly;
    }
    if (!receiver.IsValid() || receiver.HasBuff("BardWSpeedBoost")) return plan;
    const int ammo = RuntimeWAmmo();
    const bool urgent = retreat || receiver.HealthPercent() <= 42.0f;
    if (ammo <= 1 && !urgent && Bool(WMenu, "ReserveLast", true)) return plan;
    if (!retreat && !Engine::ValidEnemy(target) &&
        receiver.HealthPercent() > 72.0f) return plan;
    plan.TargetId = static_cast<int>(receiver.NetworkId());
    plan.CastPosition = receiver.Position();
    plan.Purpose = retreat
        ? WPurpose::RetreatSpeed : WPurpose::CombatSpeed;
    plan.Score = (100.0f - receiver.HealthPercent()) * 3.0f +
        (retreat ? 280.0f : 120.0f);
    plan.DirectTarget = true;
    plan.ReserveOverride = urgent;
    plan.Valid = true;
    return plan;
}

inline WPlan BuildGroundShrinePlan(bool objectiveSetup) {
    WPlan best{};
    const auto player = GameObjects::Player();
    const int ammo = RuntimeWAmmo();
    if (!player.IsValid() || ammo <= 0 || !HasCurrentResource(SpellCost(1)) ||
        HasEnemyChampionNear(objectiveSetup ? 1200.0f : 950.0f)) {
        return best;
    }
    const auto shrines = GeometryShrines();
    if (static_cast<int>(shrines.size()) >= kWMaximumGroundShrines) return best;
    std::vector<Vector3> candidates;
    const AIHeroClient protectedAlly = RawAllyById(ProtectedAllyId);
    const Vector3 anchor = protectedAlly.IsValid()
        ? protectedAlly.Position() : player.Position();
    Vector3 retreat = SharedGeometry::Direction2D(
        Game::CursorPos(), anchor);
    if (retreat.IsZero()) retreat = SharedGeometry::Direction2D(
        player.Position(), Game::CursorPos()) * -1.0f;
    if (!retreat.IsZero()) {
        candidates.push_back(anchor + retreat * 230.0f);
        candidates.push_back(anchor + retreat * 420.0f);
    }
    const Vector3 cursor = ClampToRange(
        player.Position(), Game::CursorPos(), 650.0f);
    if (cursor.IsValid() && !cursor.IsZero()) candidates.push_back(cursor);
    for (int i = 0; i < 8; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / 8.0f;
        candidates.push_back(Vector3{
            anchor.x + std::cos(angle) * (objectiveSetup ? 460.0f : 320.0f),
            anchor.y,
            anchor.z + std::sin(angle) * (objectiveSetup ? 460.0f : 320.0f),
        });
    }
    for (Vector3 candidate : candidates) {
        candidate.y = SDK::NavMesh::GetHeightForPosition(candidate);
        const bool terrain = SDK::NavMesh::IsWall(candidate);
        if (!CanPlaceGroundShrine(
                player.Position(), candidate, shrines, ammo, terrain,
                Bool(WMenu, "ReserveLast", true))) {
            continue;
        }
        float score = 300.0f -
            candidate.Distance2D(Game::CursorPos()) * 0.10f;
        score += static_cast<float>(Engine::CountAlliesAt(candidate, 600.0f)) *
                 90.0f;
        score -= static_cast<float>(Engine::CountEnemiesAt(candidate, 800.0f)) *
                 260.0f;
        if (Engine::UnderEnemyTurret(candidate)) score -= 800.0f;
        if (objectiveSetup) score += 120.0f;
        if (score > best.Score) {
            best.CastPosition = candidate;
            best.Purpose = objectiveSetup
                ? WPurpose::PreObjectiveShrine : WPurpose::RetreatShrine;
            best.Score = score;
            best.DirectTarget = false;
            best.Valid = true;
        }
    }
    return best;
}

inline bool CastWPlan(const WPlan& plan, bool reactive = false) {
    if (!plan.Valid || RuntimeWAmmo() <= 0 ||
        !ControllerHelpers::CastThrottleReady(
            1, 34, reactive ? 0 : -1)) return false;
    bool cast = false;
    if (plan.DirectTarget) {
        const AIHeroClient target = RawAllyById(plan.TargetId);
        if (!target.IsValid()) return false;
        cast = Engine::ControllerCastUnit(1, target);
    } else {
        cast = Engine::ControllerCastPosition(1, plan.CastPosition);
    }
    if (!cast) return false;
    LastWPlan = plan;
    LastWPurpose = plan.Purpose;
    LastWCastTick = Now();
    PendingWPosition = plan.DirectTarget ? Vector3{} : plan.CastPosition;
    return true;
}

inline EPlan BuildPortalPlan(const Vector3& desired,
                             EPurpose purpose,
                             bool defensive,
                             bool playerRequested) {
    EPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2) || PlayerMobilityLocked() ||
        !HasCurrentResource(SpellCost(2))) return best;
    Vector3 base = SharedGeometry::Direction2D(player.Position(), desired);
    if (base.IsZero()) return best;
    const AIHeroClient threat = ControllerHelpers::NearestEnemyToPlayer(
        {}, 1500.0f);
    static constexpr std::array<float, 9> angles = {
        0.0f, 0.08f, -0.08f, 0.16f, -0.16f,
        0.26f, -0.26f, 0.40f, -0.40f,
    };
    for (float angle : angles) {
        const Vector3 direction = SharedGeometry::Rotate2D(base, angle);
        if (direction.IsZero()) continue;
        for (float distance = 120.0f; distance <= kECastRange;
             distance += 35.0f) {
            const Vector3 terrainPoint = player.Position() + direction * distance;
            if (!SDK::NavMesh::IsWall(terrainPoint)) continue;
            const PortalTrace portal = TracePortal(
                player.Position(), terrainPoint,
                [](const Vector3& point) { return SDK::NavMesh::IsWall(point); },
                14.0f);
            if (!portal.Valid || SDK::NavMesh::IsWall(portal.Exit) ||
                HasReadyPointClickThreatAt(portal.Exit) ||
                (Bool(EMenu, "RespectDashHazards", true) &&
                 HasReadyDashHazardAt(portal.Exit))) {
                continue;
            }
            PortalSafetyContext context{};
            context.AlliesAtExit = Engine::CountAlliesAt(portal.Exit, 700.0f);
            context.EnemiesAtExit = Engine::CountEnemiesAt(portal.Exit, 700.0f);
            context.EnemiesAtEntrance = Engine::CountEnemiesAt(
                portal.Entrance, 550.0f);
            context.CursorDistance = portal.Exit.Distance2D(Game::CursorPos());
            context.ExitTerrain = SDK::NavMesh::IsWall(portal.Exit);
            context.ExitUnderEnemyTurret = Engine::UnderEnemyTurret(portal.Exit);
            context.DashHazardAtExit = HasReadyDashHazardAt(portal.Exit);
            context.AllyRequestedDirection = playerRequested ||
                CursorDirectionAgrees(portal.Exit, defensive ? -0.15f : 0.20f);
            if (threat.IsValid()) {
                context.ThreatSeparationGain =
                    portal.Exit.Distance2D(threat.Position()) -
                    player.Position().Distance2D(threat.Position());
            }
            float score = PortalSafetyScore(portal, context, defensive);
            if (!playerRequested && !defensive) score -= 220.0f;
            if (portal.TerrainLength > 1800.0f && !defensive) score -= 90.0f;
            if (score > best.Score) {
                best.Portal = portal;
                best.Purpose = purpose;
                best.Score = score;
                best.Defensive = defensive;
                best.PlayerRequested = playerRequested;
                best.Valid = score >= static_cast<float>(
                    Slider(EMenu, "MinimumSafety", defensive ? 180 : 260));
            }
            break;
        }
    }
    return best;
}

inline bool CastEPlan(const EPlan& plan, bool reactive = false) {
    if (!plan.Valid || !Ready(2) ||
        !ControllerHelpers::CastThrottleReady(
            2, 34, reactive ? 0 : -1)) return false;
    if (!Engine::ControllerCastPosition(2, plan.Portal.CastPosition)) {
        return false;
    }
    LastEPlan = plan;
    LastEPurpose = plan.Purpose;
    LastECastTick = Now();
    PendingPortalCast = plan.Portal.CastPosition;
    PendingPortalGeometry = plan.Portal;
    PendingPortalUntil = Now() + 1000;
    return true;
}

inline std::vector<StasisUnit> BuildStasisUnits(float predictionSeconds) {
    std::vector<StasisUnit> units;
    units.reserve(48);
    const auto player = GameObjects::Player();
    const auto appendAlly = [&](const AIHeroClient& ally) {
        if (!ally.IsValid() || ally.IsDead()) return;
        StasisUnit unit{};
        unit.Position = ally.Position();
        unit.PredictedPosition = PredictPosition(ally, predictionSeconds);
        unit.Radius = ally.BoundingRadius();
        unit.Priority = ally.NetworkId() == player.NetworkId()
            ? 1.2f : std::max(
                0.8f, ControllerHelpers::AllyProtectionPriority(ally) / 250.0f);
        unit.HealthPercent = ally.HealthPercent();
        unit.Id = static_cast<int>(ally.NetworkId());
        unit.Team = TeamRelation::Ally;
        unit.Champion = unit.Valid = true;
        const AllyThreatRecord threat = CurrentAllyThreat(unit.Id);
        unit.IncomingLethal = threat.UntilTick >= Now() &&
            (threat.Pressure >= ally.Health() * 0.80f ||
             (threat.Targeted && ally.HealthPercent() <=
                 static_cast<float>(Slider(RMenu, "SaveAllyHp", 28))));
        unit.ProtectedAlly = unit.Id == ProtectedAllyId;
        unit.Channeling = ally.Spellbook().IsChanneling();
        unit.HardCrowdControlled = Engine::IsHardCrowdControlled(ally);
        units.push_back(unit);
    };
    if (player.IsValid()) appendAlly(player);
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!player.IsValid() || ally.NetworkId() != player.NetworkId()) {
            appendAlly(ally);
        }
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid() || enemy.IsDead()) continue;
        StasisUnit unit{};
        unit.Position = enemy.Position();
        unit.PredictedPosition = PredictPosition(enemy, predictionSeconds);
        unit.Radius = enemy.BoundingRadius();
        unit.Priority = EnemyPriority(enemy);
        unit.HealthPercent = enemy.HealthPercent();
        unit.Id = static_cast<int>(enemy.NetworkId());
        unit.Team = TeamRelation::Enemy;
        unit.Champion = unit.Valid = true;
        unit.CurrentAllyFocus = FocusedByAlly(unit.Id);
        unit.Channeling = enemy.Spellbook().IsChanneling();
        unit.HardCrowdControlled = Engine::IsHardCrowdControlled(enemy);
        units.push_back(unit);
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (!monster.IsValid() || monster.IsDead() ||
            !monster.IsTargetable() || !IsEpicMonster(monster)) continue;
        StasisUnit unit{};
        unit.Position = unit.PredictedPosition = monster.Position();
        unit.Radius = monster.BoundingRadius();
        unit.HealthPercent = monster.HealthPercent();
        unit.Id = static_cast<int>(monster.NetworkId());
        unit.Team = TeamRelation::Neutral;
        unit.Monster = unit.EpicMonster = unit.Valid = true;
        units.push_back(unit);
    }
    // REMOVED: Turret/Inhibitor/Nexus class disabled by user request
    // for (const auto& turret : GameObjects::EnemyTurrets()) {
    //     if (!turret.IsValid() || turret.IsDead()) continue;
    //     StasisUnit unit{};
    //     unit.Position = unit.PredictedPosition = turret.Position();
    //     unit.Radius = turret.BoundingRadius();
    //     unit.Id = static_cast<int>(turret.NetworkId());
    //     unit.Team = TeamRelation::Enemy;
    //     unit.Turret = unit.Valid = true;
    //     units.push_back(unit);
    // }
    for (const auto& plant : GameObjects::Plants()) {
        if (!plant.IsValid() || plant.IsDead()) continue;
        StasisUnit unit{};
        unit.Position = unit.PredictedPosition = plant.Position();
        unit.Radius = plant.BoundingRadius();
        unit.Id = static_cast<int>(plant.NetworkId());
        unit.Team = TeamRelation::Neutral;
        unit.Plant = unit.Valid = true;
        units.push_back(unit);
    }
    return units;
}

inline RPlan EvaluateRPlanAt(const Vector3& center,
                             int primaryId,
                             RPurpose purpose,
                             const StasisContext& context,
                             bool manual = false) {
    RPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3) || !center.IsValid() ||
        center.IsZero()) return plan;
    const float distance = player.Position().Distance2D(center);
    const float impactSeconds = RImpactSeconds(distance);
    const auto units = BuildStasisUnits(impactSeconds);
    plan.Evaluation = EvaluateTemperedFate(
        player.Position(), center, units, context);
    plan.PrimaryId = primaryId;
    plan.Purpose = purpose;
    plan.ExpectedImpactTick = Now() +
        static_cast<int>(std::ceil(impactSeconds * 1000.0f));
    plan.Manual = manual;
    plan.Valid = plan.Evaluation.Valid;
    return plan;
}

inline RPlan BuildCatchRPlan(const AIHeroClient& preferred,
                             bool flee = false) {
    RPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3)) return best;
    const float maximumRange = static_cast<float>(
        Slider(RMenu, "MaximumCatchRange", 1650));
    std::vector<AIHeroClient> candidates;
    if (Engine::ValidEnemy(preferred, maximumRange)) candidates.push_back(preferred);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, maximumRange)) continue;
        if (!candidates.empty() &&
            enemy.NetworkId() == candidates.front().NetworkId()) continue;
        candidates.push_back(enemy);
    }
    for (const auto& enemy : candidates) {
        if (TargetRejectsQ(enemy) ||
            (!flee && FocusedByAlly(static_cast<int>(enemy.NetworkId())))) {
            continue;
        }
        const float distance = player.Position().Distance2D(enemy.Position());
        const float impact = RImpactSeconds(distance);
        const Vector3 center = PredictPosition(enemy, impact);
        if (!center.IsValid() || center.IsZero()) continue;
        if (!flee && Bool(RoleMenu, "RespectCursor", true) &&
            !CursorDirectionAgrees(center, -0.28f)) continue;
        const int followup = CountAlliedFollowup(center, 950.0f);
        if (!flee && followup < Slider(RMenu, "CatchFollowup", 1)) continue;
        StasisContext context{};
        context.Catch = true;
        context.MinimumEnemyChampions = flee
            ? Slider(RMenu, "FleeMinimum", 2) : 1;
        context.MaximumFriendlyGrief = 0;
        RPlan plan = EvaluateRPlanAt(
            center, static_cast<int>(enemy.NetworkId()),
            flee ? RPurpose::Flee : RPurpose::Catch, context);
        if (!plan.Valid) continue;
        plan.Evaluation.Score += static_cast<float>(followup) * 130.0f;
        if (enemy.NetworkId() == preferred.NetworkId()) {
            plan.Evaluation.Score += 120.0f;
        }
        if (BetterRPlan(plan.Evaluation, best.Evaluation)) best = plan;
    }
    return best;
}

inline RPlan BuildSaveRPlan() {
    RPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3) || !Bool(RMenu, "SaveAlly", true)) {
        return best;
    }
    const auto evaluate = [&](const AIHeroClient& ally) {
        if (!ally.IsValid() || ally.IsDead() ||
            player.Position().Distance2D(ally.Position()) > kRCastRange) return;
        const AllyThreatRecord threat = CurrentAllyThreat(
            static_cast<int>(ally.NetworkId()));
        if (threat.UntilTick < Now()) return;
        const bool lethal = threat.Pressure >= ally.Health() * 0.80f ||
            (threat.Targeted && ally.HealthPercent() <=
                static_cast<float>(Slider(RMenu, "SaveAllyHp", 28)));
        if (!lethal) return;
        const float distance = player.Position().Distance2D(ally.Position());
        const float impactSeconds = RImpactSeconds(distance);
        if (threat.ImpactTick > Now() &&
            threat.Pressure >= ally.Health() * 0.80f &&
            Now() + static_cast<int>(impactSeconds * 1000.0f) >
                threat.ImpactTick + 70) {
            return;
        }
        const Vector3 center = PredictPosition(ally, impactSeconds);
        StasisContext context{};
        context.SaveAlly = true;
        context.MaximumFriendlyGrief = 0;
        RPlan plan = EvaluateRPlanAt(
            center, static_cast<int>(ally.NetworkId()),
            RPurpose::AllySave, context);
        if (BetterRPlan(plan.Evaluation, best.Evaluation)) best = plan;
    };
    evaluate(player);
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (ally.NetworkId() != player.NetworkId()) evaluate(ally);
    }
    return best;
}

inline RPlan BuildInterruptRPlan() {
    RPlan plan{};
    if (InterruptTargetId == 0 || InterruptExpireTick < Now() || !Ready(3) ||
        !Bool(RMenu, "Interrupt", true)) return plan;
    const AIHeroClient target = RawEnemyById(InterruptTargetId);
    if (!target.IsValid()) return plan;
    const float distance = GameObjects::Player().Position().Distance2D(
        target.Position());
    const float impact = RImpactSeconds(distance);
    if (Now() + static_cast<int>(impact * 1000.0f) > InterruptExpireTick) {
        return plan;
    }
    StasisContext context{};
    context.Catch = true;
    context.MaximumFriendlyGrief = 0;
    plan = EvaluateRPlanAt(
        PredictPosition(target, impact), InterruptTargetId,
        RPurpose::Interrupt, context);
    return plan;
}

inline RPlan BuildDiveTurretRPlan() {
    RPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3) ||
        !Key(RMenu, "DiveTurret", false)) return best;
    // REMOVED: Turret/Inhibitor/Nexus class disabled by user request
    // for (const auto& turret : GameObjects::EnemyTurrets()) {
    //     if (!turret.IsValid() || turret.IsDead() ||
    //         player.Position().Distance2D(turret.Position()) > kRCastRange) {
    //         continue;
    //     }
    //     if (Engine::CountAlliesAt(turret.Position(), 900.0f) < 1) continue;
    //     StasisContext context{};
    //     context.DiveTower = true;
    //     context.MaximumFriendlyGrief = 0;
    //     RPlan plan = EvaluateRPlanAt(
    //         turret.Position(), static_cast<int>(turret.NetworkId()),
    //         RPurpose::DiveTurret, context, true);
    //     if (BetterRPlan(plan.Evaluation, best.Evaluation)) best = plan;
    // }
    return best;
}

inline RPlan BuildObjectiveRPlan() {
    RPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3) ||
        !Key(RMenu, "ObjectiveDeny", false)) return best;
    for (const auto& monster : GameObjects::Jungle()) {
        if (!monster.IsValid() || monster.IsDead() ||
            !monster.IsTargetable() || !IsEpicMonster(monster) ||
            player.Position().Distance2D(monster.Position()) > kRCastRange) {
            continue;
        }
        const int enemies = Engine::CountEnemiesAt(monster.Position(), 950.0f);
        const int allies = Engine::CountAlliesAt(monster.Position(), 950.0f);
        if (enemies <= 0 || allies > enemies + 1) continue;
        StasisContext context{};
        context.ObjectiveDeny = true;
        context.AlliesSecuringObjective = allies > 0 && enemies == 0;
        context.MaximumFriendlyGrief = 0;
        RPlan plan = EvaluateRPlanAt(
            monster.Position(), static_cast<int>(monster.NetworkId()),
            RPurpose::ObjectiveDeny, context, true);
        if (BetterRPlan(plan.Evaluation, best.Evaluation)) best = plan;
    }
    return best;
}

inline bool CastRPlan(const RPlan& plan, bool reactive = false) {
    if (!plan.Valid || !Ready(3) ||
        !ControllerHelpers::CastThrottleReady(
            3, 34, reactive ? 0 : -1)) return false;
    if (!Engine::ControllerCastPosition(3, plan.Evaluation.Center)) return false;
    LastRPlan = plan;
    LastRPurpose = plan.Purpose;
    LastRCastTick = Now();
    PendingRPrimaryId = plan.PrimaryId;
    PendingRImpactTick = plan.ExpectedImpactTick;
    PendingRManual = plan.Manual;
    if (plan.Purpose == RPurpose::AllySave) {
        ActiveSequence = Sequence::SaveStasis;
    } else if (plan.Purpose == RPurpose::DiveTurret) {
        ActiveSequence = Sequence::DiveReset;
    } else if (plan.Purpose == RPurpose::ObjectiveDeny) {
        ActiveSequence = Sequence::ObjectiveDeny;
    } else {
        ActiveSequence = Sequence::StasisExitQ;
    }
    SequenceTargetId = plan.PrimaryId;
    SequenceExpireTick = plan.ExpectedImpactTick + 4200;
    return true;
}

inline bool ContinueStasisExitQ() {
    if (ActiveSequence != Sequence::StasisExitQ ||
        SequenceExpireTick < Now() || SequenceTargetId == 0 || !Ready(0)) {
        return false;
    }
    StasisRecord* stasis = FindStasis(SequenceTargetId);
    if (!stasis || stasis->EndTick <= Now()) return false;
    QPlan plan = BuildStasisExitQPlan(*stasis);
    if (!plan.Valid) return false;
    const AIHeroClient target = RawEnemyById(SequenceTargetId);
    if (!target.IsValid()) return false;
    const float remaining = static_cast<float>(
        stasis->EndTick - Now()) / 1000.0f;
    const float travel = std::max(
        0.0f, GameObjects::Player().Position().Distance2D(
            target.Position()) - target.BoundingRadius() - kQHalfWidth);
    const float waitSeconds = QCastDelayForStasisExit(
        remaining, travel,
        static_cast<float>(Slider(QMenu, "StasisEarlyMs", 45)) / 1000.0f);
    if (waitSeconds > 0.055f) return false;
    if (CastQPlan(plan, true)) {
        ActiveSequence = Sequence::MeepQWeave;
        SequenceExpireTick = Now() + 900;
        return true;
    }
    return false;
}

inline void ClearSequence() {
    ActiveSequence = Sequence::None;
    SequenceTargetId = 0;
    SequenceExpireTick = 0;
    PortalTravellerId = 0;
    PortalTravellerUntil = 0;
    PortalTravellerExit = {};
}

inline const PortalRecord* NearestOwnedPortal(const Vector3& position,
                                              float range) {
    const PortalRecord* best = nullptr;
    float bestDistance = std::max(0.0f, range);
    for (const auto& portal : Portals) {
        if (portal.NetworkId == 0 || !portal.ConfirmedOwned ||
            portal.ExpireTick < Now() || !portal.Geometry.Valid) {
            continue;
        }
        const float distance = std::min(
            position.Distance2D(portal.Geometry.Entrance),
            position.Distance2D(portal.Geometry.Exit));
        if (distance <= bestDistance) {
            best = &portal;
            bestDistance = distance;
        }
    }
    return best;
}

inline bool ContinuePortalExitQ() {
    if (ActiveSequence != Sequence::PortalExitQ ||
        SequenceExpireTick < Now() || PortalTravellerId == 0 ||
        !PortalTravellerExit.IsValid() || PortalTravellerExit.IsZero() ||
        !Ready(0)) {
        return false;
    }
    const AIHeroClient target = RawEnemyById(PortalTravellerId);
    if (!target.IsValid()) return false;
    QPlan plan = BuildPortalExitQPlan(
        PortalTravellerId, PortalTravellerExit);
    if (!plan.Valid) return false;
    const float remaining = std::max(
        0.0f, static_cast<float>(PortalTravellerUntil - Now()) / 1000.0f);
    const float travelDistance = std::max(
        0.0f, GameObjects::Player().Position().Distance2D(
            PortalTravellerExit) - target.BoundingRadius() - kQHalfWidth);
    const float delay = QCastDelayForStasisExit(
        remaining, travelDistance,
        static_cast<float>(Slider(QMenu, "PortalEarlyMs", 55)) / 1000.0f);
    if (delay > 0.055f) return false;
    if (!CastQPlan(plan, true)) return false;
    ClearSequence();
    ActiveSequence = Sequence::MeepQWeave;
    SequenceTargetId = plan.TargetId;
    SequenceExpireTick = Now() + 850;
    return true;
}

inline AIHeroClient CursorEnemy(float range = kRCastRange) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    AIHeroClient best{};
    float bestCursorDistance = 560.0f;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range)) continue;
        const float cursorDistance = enemy.Position().Distance2D(
            Game::CursorPos());
        if (cursorDistance < bestCursorDistance) {
            best = enemy;
            bestCursorDistance = cursorDistance;
        }
    }
    return best;
}

inline AIHeroClient PreferredEnemy(const AIHeroClient& selected,
                                   float range) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    return ControllerHelpers::NearestEnemyToPlayer({}, range);
}

inline Posture DeterminePosture(Mode mode,
                                const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return Posture::Neutral;
    if (mode == Mode::Flee) return Posture::Disengage;
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return Posture::Farm;
    if (HasEnemyChampionNear(625.0f) &&
        (player.HealthPercent() <= Slider(TacticsMenu, "DefensiveHp", 42) ||
         GapcloserExpireTick >= Now())) {
        return Posture::Peel;
    }
    if (Engine::ValidEnemy(target)) {
        const int allies = CountAlliedFollowup(target.Position(), 1000.0f);
        const int enemies = Engine::CountEnemiesAt(target.Position(), 850.0f);
        if (mode == Mode::Combo && allies >= enemies) return Posture::Catch;
        return Posture::FrontToBack;
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (monster.IsValid() && !monster.IsDead() && IsEpicMonster(monster) &&
            player.Position().Distance2D(monster.Position()) < 2200.0f) {
            return Posture::Objective;
        }
    }
    const AIHeroClient protectedAlly = RawAllyById(ProtectedAllyId);
    if (protectedAlly.IsValid() &&
        player.Position().Distance2D(protectedAlly.Position()) <= 1200.0f) {
        return Posture::LaneGuard;
    }
    return Posture::Roam;
}

inline RPlan BuildBacklineIsolationRPlan() {
    RPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3) ||
        !Bool(RMenu, "BacklineIsolation", true)) return best;
    const float maximumRange = static_cast<float>(
        Slider(RMenu, "IsolationRange", 1850));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, maximumRange) ||
            FocusedByAlly(static_cast<int>(enemy.NetworkId()))) continue;
        const float distance = player.Position().Distance2D(enemy.Position());
        const float impact = RImpactSeconds(distance);
        const Vector3 center = PredictPosition(enemy, impact);
        const int nearbyTeam = Engine::CountEnemiesAt(center, 950.0f);
        if (nearbyTeam < 2 || CountAlliedFollowup(center, 1150.0f) < 2) {
            continue;
        }
        StasisContext context{};
        context.BacklineIsolation = true;
        context.MinimumEnemyChampions = 1;
        context.MaximumFriendlyGrief = 0;
        RPlan plan = EvaluateRPlanAt(
            center, static_cast<int>(enemy.NetworkId()),
            RPurpose::BacklineIsolation, context);
        if (!plan.Valid || plan.Evaluation.FocusedEnemies > 0 ||
            plan.Evaluation.EnemyChampions >= nearbyTeam) continue;
        plan.Evaluation.Score += EnemyPriority(enemy) * 115.0f;
        if (BetterRPlan(plan.Evaluation, best.Evaluation)) best = plan;
    }
    return best;
}

inline AIMinionClient MinionById(int networkId) {
    if (networkId == 0) return {};
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (static_cast<int>(minion.NetworkId()) == networkId) return minion;
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (static_cast<int>(monster.NetworkId()) == networkId) return monster;
    }
    return {};
}

inline QPlan BuildFarmQPlan(Mode mode) {
    QPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0) ||
        !HasCurrentResource(SpellCost(0))) return best;
    const bool jungle = mode == Mode::Jungle;
    const bool lastHit = mode == Mode::LastHit;
    const auto units = BuildQUnits(kQCastSeconds + 0.30f);
    std::vector<Vector3> candidates;
    candidates.reserve(units.size() * 3);
    for (const auto& first : units) {
        if (!first.Valid || first.Champion || first.Monster != jungle) continue;
        const Vector3 direction = SharedGeometry::Direction2D(
            player.Position(), first.PredictedPosition);
        if (!direction.IsZero()) {
            candidates.push_back(player.Position() +
                direction * kQInitialTargetRange);
        }
        for (const auto& second : units) {
            if (!second.Valid || second.Id == first.Id || second.Champion ||
                second.Monster != jungle) continue;
            const Vector3 aim = PairAlignmentAim(
                player.Position(), first.PredictedPosition,
                second.PredictedPosition);
            if (aim.IsValid() && !aim.IsZero()) candidates.push_back(aim);
        }
    }
    const float raw = QRawDamage(SpellRank(0), player.AP());
    for (const Vector3& aim : candidates) {
        QEvaluation evaluation = EvaluateCosmicBinding(
            player.Position(), aim, units,
            TerrainSamplesForQ(player.Position(), aim));
        if (!evaluation.Valid) continue;
        const QUnit* first = QUnitById(units, evaluation.FirstId);
        const QUnit* second = QUnitById(units, evaluation.SecondId);
        if (!first || first->Champion || first->Monster != jungle ||
            (second && (second->Champion || second->Monster != jungle))) {
            continue;
        }
        const int hitCount = 1 + (second ? 1 : 0);
        bool lethal = false;
        for (int id : { evaluation.FirstId, evaluation.SecondId }) {
            const AIMinionClient minion = MinionById(id);
            if (minion.IsValid() &&
                player.CalculateMagicDamage(minion, raw) >=
                    minion.Health() + 2.0f) lethal = true;
        }
        if ((lastHit && !lethal) || (!lastHit && !jungle && hitCount < 2)) {
            continue;
        }
        float score = static_cast<float>(hitCount) * 260.0f +
            (lethal ? 420.0f : 0.0f);
        if (first->Monster && IsEpicMonster(MinionById(first->Id))) {
            score += 190.0f;
        }
        if (!best.Valid || score > best.Evaluation.Score) {
            evaluation.Score = score;
            best.Evaluation = evaluation;
            best.TargetId = evaluation.FirstId;
            best.Purpose = jungle ? QPurpose::Jungle
                                  : QPurpose::LaneDoubleHit;
            best.RawDamage = raw;
            best.Lethal = lethal;
            best.Valid = true;
        }
    }
    return best;
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(TacticsMenu, "KillSecure", true) || !Ready(0)) return false;
    std::vector<AIHeroClient> candidates;
    if (Engine::ValidEnemy(preferred, kQInitialTargetRange + 100.0f)) {
        candidates.push_back(preferred);
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kQInitialTargetRange + 100.0f)) continue;
        if (!candidates.empty() &&
            candidates.front().NetworkId() == enemy.NetworkId()) continue;
        candidates.push_back(enemy);
    }
    std::sort(candidates.begin(), candidates.end(),
        [](const AIHeroClient& left, const AIHeroClient& right) {
            return left.Health() + left.AllShield() <
                   right.Health() + right.AllShield();
        });
    for (const auto& enemy : candidates) {
        const QPlan q = BuildQPlan(enemy, QPurpose::KillSecure, false);
        if (q.Valid && q.Lethal && CastQPlan(q, true)) return true;
    }
    return false;
}

inline bool TryReactive(Mode mode) {
    if (ContinueStasisExitQ() || ContinuePortalExitQ()) return true;

    if (Bool(WMenu, "EmergencyDirect", true)) {
        const WPlan emergency = BuildEmergencyWPlan();
        if (CastWPlan(emergency, true)) return true;
    }

    AIHeroClient threat{};
    QPurpose qPurpose = QPurpose::Peel;
    if (InterruptExpireTick >= Now()) {
        threat = RawEnemyById(InterruptTargetId);
        qPurpose = QPurpose::Interrupt;
    }
    if (!Engine::ValidEnemy(threat) && GapcloserExpireTick >= Now()) {
        threat = RawEnemyById(GapcloserTargetId);
        qPurpose = QPurpose::AntiGapcloser;
    }
    if (Engine::ValidEnemy(threat) && Ready(0)) {
        const QPlan q = BuildQPlan(threat, qPurpose, true);
        if (CastQPlan(q, true)) return true;
    }

    const RPlan save = BuildSaveRPlan();
    if (CastRPlan(save, true)) return true;
    if (InterruptExpireTick >= Now()) {
        const RPlan interrupt = BuildInterruptRPlan();
        if (CastRPlan(interrupt, true)) return true;
    }

    const auto player = GameObjects::Player();
    const bool urgent = player.IsValid() &&
        (mode == Mode::Flee || GapcloserExpireTick >= Now() ||
         player.HealthPercent() <= Slider(TacticsMenu, "DefensiveHp", 42));
    if (urgent && Bool(EMenu, "DefensiveEscape", true) && Ready(2)) {
        const EPlan portal = BuildPortalPlan(
            Game::CursorPos(), EPurpose::DefensiveEscape, true, true);
        if (CastEPlan(portal, true)) return true;
    }
    return false;
}

inline bool TryManualInputs(const AIHeroClient& selected) {
    if (Key(EMenu, "PlayerPortal", false)) {
        const EPlan portal = BuildPortalPlan(
            Game::CursorPos(), EPurpose::PlayerCommit, false, true);
        if (CastEPlan(portal, true)) return true;
    }
    if (Key(RMenu, "DiveTurret", false)) {
        const RPlan dive = BuildDiveTurretRPlan();
        if (CastRPlan(dive, true)) return true;
    }
    if (Key(RMenu, "ObjectiveDeny", false)) {
        const RPlan objective = BuildObjectiveRPlan();
        if (CastRPlan(objective, true)) return true;
    }
    if (Key(RMenu, "ManualCatch", false)) {
        const AIHeroClient target = Engine::ValidEnemy(selected)
            ? selected : CursorEnemy();
        RPlan catchPlan = BuildCatchRPlan(target);
        catchPlan.Manual = true;
        if (CastRPlan(catchPlan, true)) return true;
    }
    return false;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;

    const float distance = player.Position().Distance2D(target.Position());
    if (Ready(3) && Bool(RMenu, "AutomaticCatch", true) &&
        (distance > kQInitialTargetRange - 80.0f || !Ready(0))) {
        const RPlan catchPlan = BuildCatchRPlan(target);
        if (CastRPlan(catchPlan)) return true;
    }
    if (Ready(3) && Bool(RMenu, "BacklineIsolation", true)) {
        const RPlan isolation = BuildBacklineIsolationRPlan();
        if (CastRPlan(isolation)) return true;
    }

    if (Ready(0) && SpellEnabled(0, Mode::Combo)) {
        QPurpose purpose = QPurpose::WallStun;
        if (LastMeepAttackTargetId == static_cast<int>(target.NetworkId()) &&
            Now() - LastMeepAttackTick <= kMeepQWindowMs) {
            purpose = QPurpose::MeepFollowup;
        }
        const QPlan q = BuildQPlan(
            target, purpose, purpose != QPurpose::MeepFollowup);
        if (q.Valid) {
            const bool immediateStun = q.Evaluation.FirstStunned ||
                                       q.Evaluation.SecondStunned;
            if (ShouldWaitForMeepBeforeQ(
                    MeepAvailable(), InAutoAttackRange(target),
                    immediateStun, false, q.Lethal) &&
                !(LastMeepAttackTargetId ==
                      static_cast<int>(target.NetworkId()) &&
                  Now() - LastMeepAttackTick <= kMeepQWindowMs)) {
                return false;
            }
            if (CastQPlan(q)) {
                ActiveSequence = Sequence::MeepQWeave;
                SequenceTargetId = static_cast<int>(target.NetworkId());
                SequenceExpireTick = Now() + 900;
                return true;
            }
        }
    }
    if (Bool(WMenu, "CombatSpeed", true)) {
        const WPlan speed = BuildCombatWPlan(target, false);
        if (CastWPlan(speed)) return true;
    }
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) ||
        PlayerManaPercent() < Slider(QMenu, "HarassMana", 58)) return false;
    if (!Ready(0) || !SpellEnabled(0, Mode::Harass)) return false;
    const bool afterMeep = LastMeepAttackTargetId ==
            static_cast<int>(target.NetworkId()) &&
        Now() - LastMeepAttackTick <= kMeepQWindowMs;
    const QPlan q = BuildQPlan(
        target, afterMeep ? QPurpose::MeepFollowup : QPurpose::WallStun,
        !afterMeep);
    if (!q.Valid) return false;
    if (!afterMeep && ShouldWaitForMeepBeforeQ(
            MeepAvailable(), InAutoAttackRange(target),
            q.Evaluation.FirstStunned || q.Evaluation.SecondStunned,
            false, q.Lethal)) return false;
    return CastQPlan(q);
}

inline bool TryFlee(const AIHeroClient& fallback) {
    const AIHeroClient threat = PreferredEnemy(fallback, 1250.0f);
    if (Bool(WMenu, "RetreatSpeed", true)) {
        const WPlan speed = BuildCombatWPlan(threat, true);
        if (CastWPlan(speed, true)) return true;
    }
    if (Engine::ValidEnemy(threat) && Ready(0) &&
        Bool(QMenu, "FleeQ", true)) {
        const QPlan q = BuildQPlan(threat, QPurpose::Flee, false);
        if (CastQPlan(q, true)) return true;
    }
    if (Ready(2) && Bool(EMenu, "FleePortal", true)) {
        const EPlan portal = BuildPortalPlan(
            Game::CursorPos(), EPurpose::DefensiveEscape, true, true);
        if (CastEPlan(portal, true)) return true;
    }
    if (Ready(3) && Bool(RMenu, "FleeR", true) &&
        Engine::ValidEnemy(threat)) {
        const RPlan flee = BuildCatchRPlan(threat, true);
        if (CastRPlan(flee, true)) return true;
    }
    return false;
}

inline bool TryFarm(Mode mode) {
    if (!Bool(FarmMenu, "UseQ", true) ||
        (Bool(FarmMenu, "HoldForChampion", true) &&
         HasEnemyChampionNear(static_cast<float>(
             Slider(FarmMenu, "ChampionHoldRange", 1200))))) {
        return false;
    }
    const float manaFloor = static_cast<float>(Slider(
        FarmMenu, mode == Mode::Jungle ? "JungleMana" : "LaneMana",
        mode == Mode::Jungle ? 30 : 62));
    if (PlayerManaPercent() < manaFloor || Orbwalker::IsWindingUp()) {
        return false;
    }
    const QPlan q = BuildFarmQPlan(mode);
    return CastQPlan(q);
}

inline bool TryAutomaticSetup() {
    if (!Bool(WMenu, "GroundShrines", true) ||
        ActiveShrineCount() >= kWMaximumGroundShrines) return false;
    const bool objective = ControllerHelpers::HasNearbyEpicMonster(2300.0f);
    if (!objective && CurrentPosture != Posture::LaneGuard) return false;
    const WPlan shrine = BuildGroundShrinePlan(objective);
    return CastWPlan(shrine);
}

inline void RefreshRuntimeState() {
    const int now = Now();
    if (GapcloserExpireTick < now) {
        GapcloserTargetId = 0;
        GapcloserEndpoint = {};
    }
    if (InterruptExpireTick < now) InterruptTargetId = 0;
    if (ActiveSequence != Sequence::None && SequenceExpireTick < now) {
        ClearSequence();
    }
    for (auto& shrine : Shrines) {
        if (shrine.NetworkId != 0 && now - shrine.LastSeenTick > 600000) {
            shrine = {};
        }
    }
    for (auto& chime : Chimes) {
        if (chime.NetworkId != 0 && chime.ExpireTick < now) chime = {};
    }
    for (auto& portal : Portals) {
        if (portal.NetworkId != 0 && portal.ExpireTick < now) portal = {};
    }
    for (auto& record : StasisRecords) {
        if (record.NetworkId != 0 && record.EndTick + 750 < now) record = {};
    }
    for (auto& record : FocusRecords) {
        if (record.NetworkId != 0 && record.UntilTick < now) record = {};
    }
    for (auto& record : AllyThreats) {
        if (record.NetworkId != 0 && record.UntilTick < now) record = {};
    }
    MeepMaximum = MeepMaximumCount(ChimeCount);
    MeepAmmo = std::clamp(MeepAmmo, 0, MeepMaximum);

    int recentId = 0;
    int recentUntil = 0;
    for (const auto& threat : AllyThreats) {
        if (threat.NetworkId != 0 && threat.UntilTick > recentUntil) {
            recentId = threat.NetworkId;
            recentUntil = threat.UntilTick;
        }
    }
    const AIHeroClient protectedAlly = SelectProtectionAlly(
        1800.0f, recentId, recentUntil);
    ProtectedAllyId = protectedAlly.IsValid()
        ? static_cast<int>(protectedAlly.NetworkId()) : 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshRuntimeState();
    const AIHeroClient target = PreferredEnemy(selected, 2000.0f);
    CurrentPosture = DeterminePosture(mode, target);
    if (TryManualInputs(selected)) return true;
    if (PlayerOverrideUntil >= Now()) return false;
    if (TryReactive(mode)) return true;
    if (TryKillSecure(target)) return true;
    if (mode == Mode::Flee) return TryFlee(target);
    if (mode == Mode::Combo) return TryCombo(target);
    if (mode == Mode::Harass) return TryHarass(target);
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return TryFarm(mode);
    if (mode == Mode::None || mode == Mode::Automatic) {
        return TryAutomaticSetup();
    }
    return false;
}

inline int EventTargetId(
    const SDK::Events::ProcessSpellEventArgs& args) {
    return static_cast<int>(args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId);
}

inline float EstimatedIncomingPressure(
    const AIHeroClient& enemy,
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!enemy.IsValid()) return 0.0f;
    if (args.IsAutoAttack) return std::max(45.0f, enemy.TotalAttackDamage());
    return 90.0f + enemy.AP() * 0.48f +
           enemy.TotalAttackDamage() * 0.42f;
}

inline AIHeroClient InferAlliedFocusTarget(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const AIHeroClient explicitTarget = RawEnemyById(EventTargetId(args));
    if (explicitTarget.IsValid()) return explicitTarget;
    Vector3 endpoint = args.EndPosition;
    if (!endpoint.IsValid() || endpoint.IsZero()) endpoint = args.CastPosition;
    AIHeroClient best{};
    float bestMetric = FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 4000.0f)) continue;
        float metric = endpoint.IsValid() && !endpoint.IsZero()
            ? endpoint.Distance2D(enemy.Position()) : FLT_MAX;
        if (!args.IsAutoAttack && args.StartPosition.IsValid() &&
            args.EndPosition.IsValid() && !args.StartPosition.IsZero() &&
            !args.EndPosition.IsZero()) {
            const auto projection = SharedGeometry::ProjectPointToSegment2D(
                enemy.Position(), args.StartPosition, args.EndPosition);
            if (projection.T > 0.01f && projection.T < 0.999f) {
                metric = std::min(metric, projection.Distance);
            }
        }
        if (metric <= enemy.BoundingRadius() + 165.0f &&
            metric < bestMetric) {
            best = enemy;
            bestMetric = metric;
        }
    }
    return best;
}

inline void RecordThreatOnAlly(
    const AIHeroClient& ally,
    const AIHeroClient& enemy,
    const SDK::Events::ProcessSpellEventArgs& args,
    bool targeted) {
    if (!ally.IsValid() || !enemy.IsValid()) return;
    AllyThreatRecord* threat = FindAllyThreat(
        static_cast<int>(ally.NetworkId()), true);
    if (!threat) return;
    const int now = Now();
    const int castMs = ControllerHelpers::NormalizedCastDelayMs(
        args.CastDelay, args.IsAutoAttack ? 180 : 250);
    float travelSeconds = 0.0f;
    if (std::isfinite(args.MissileSpeed) && args.MissileSpeed > 80.0f) {
        const Vector3 start = args.StartPosition.IsValid() &&
                              !args.StartPosition.IsZero()
            ? args.StartPosition : enemy.Position();
        travelSeconds = start.Distance2D(ally.Position()) /
                        args.MissileSpeed;
    }
    const int impactTick = now + std::clamp(
        castMs + static_cast<int>(travelSeconds * 1000.0f),
        80, 4200);
    threat->NetworkId = static_cast<int>(ally.NetworkId());
    threat->SourceNetworkId = static_cast<int>(enemy.NetworkId());
    threat->UntilTick = std::max(threat->UntilTick, impactTick + 520);
    if (threat->ImpactTick <= now || impactTick < threat->ImpactTick) {
        threat->ImpactTick = impactTick;
    }
    threat->Pressure = std::max(
        threat->Pressure, EstimatedIncomingPressure(enemy, args));
    threat->HardCrowdControl = threat->HardCrowdControl ||
        ControllerHelpers::LikelyHardCrowdControlSpell(args);
    threat->Targeted = threat->Targeted || targeted;
}

inline void RecordCombatIntent(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !args.Sender.IsValid()) return;
    const std::uint32_t playerTeam =
        static_cast<std::uint32_t>(player.Team());
    const bool alliedCaster = args.Sender.Team == playerTeam ||
                              IsLocalPlayer(args.Sender);
    if (alliedCaster) {
        const AIHeroClient enemyTarget = InferAlliedFocusTarget(args);
        if (!enemyTarget.IsValid()) return;
        const int targetId = static_cast<int>(enemyTarget.NetworkId());
        FocusRecord* focus = FindFocus(targetId, true);
        if (focus) {
            focus->NetworkId = targetId;
            focus->AllyNetworkId = static_cast<int>(
                args.Sender.NetworkId);
            focus->UntilTick = Now() + (args.IsAutoAttack ? 850 : 1150);
            focus->Pressure = std::max(
                focus->Pressure, args.IsAutoAttack ? 1.0f : 1.5f);
        }
        return;
    }

    const AIHeroClient enemyCaster = RawEnemyById(
        static_cast<int>(args.Sender.NetworkId));
    if (!enemyCaster.IsValid()) return;
    const int targetId = EventTargetId(args);
    const AIHeroClient explicitAlly = RawAllyById(targetId);
    if (explicitAlly.IsValid()) {
        RecordThreatOnAlly(explicitAlly, enemyCaster, args, true);
    }
    if (args.IsAutoAttack || !args.StartPosition.IsValid() ||
        !args.EndPosition.IsValid() || args.StartPosition.IsZero() ||
        args.EndPosition.IsZero()) return;
    const auto crosses = [&](const AIHeroClient& ally) {
        if (!ally.IsValid() || ally.IsDead()) return;
        const auto projection = SharedGeometry::ProjectPointToSegment2D(
            ally.Position(), args.StartPosition, args.EndPosition);
        if (projection.Distance <= ally.BoundingRadius() + 115.0f) {
            RecordThreatOnAlly(
                ally, enemyCaster, args,
                static_cast<int>(ally.NetworkId()) == targetId);
        }
    };
    crosses(player);
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (ally.NetworkId() != player.NetworkId()) crosses(ally);
    }
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (args.IsAutoAttack) {
        (void)CaptureLocalAutoAttack(
            args, LastLocalAutoTargetId, LastLocalAutoTick);
        return;
    }
    int slot = -1;
    if (IsQEvent(args)) slot = 0;
    else if (IsWEvent(args)) slot = 1;
    else if (IsEEvent(args)) slot = 2;
    else if (IsREvent(args)) slot = 3;
    if (slot < 0) return;
    const bool controllerOwned = Engine::WasControllerCast(slot);
    if (slot == 0) LastQCastTick = now;
    else if (slot == 1) {
        LastWCastTick = now;
        if (args.EndPosition.IsValid() && !args.EndPosition.IsZero()) {
            PendingWPosition = args.EndPosition;
        }
    } else if (slot == 2) {
        LastECastTick = now;
        if (args.EndPosition.IsValid() && !args.EndPosition.IsZero()) {
            PendingPortalCast = args.EndPosition;
        }
    } else if (slot == 3) {
        LastRCastTick = now;
    }
    if (controllerOwned) return;

    ClearSequence();
    PlayerOverrideUntil = now +
        Slider(TacticsMenu, "ManualOwnershipMs", 520);
    ActiveSequence = Sequence::PlayerLed;
    SequenceTargetId = EventTargetId(args);
    SequenceExpireTick = PlayerOverrideUntil + 120;

    // A manual R remains player-owned.  Optional assistance starts only
    // after the ownership window and only times Q against the actual Bard R
    // stasis buff; it never changes the cast center or issues movement.
    if (slot == 3 && Bool(RMenu, "AssistManualR", true)) {
        Vector3 center = args.EndPosition;
        if (!center.IsValid() || center.IsZero()) center = args.CastPosition;
        AIHeroClient primary{};
        float closest = kRRadius + 150.0f;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!Engine::ValidEnemy(enemy, kRCastRange + 200.0f)) continue;
            const float distance = enemy.Position().Distance2D(center);
            if (distance < closest) {
                primary = enemy;
                closest = distance;
            }
        }
        if (primary.IsValid()) {
            PendingRPrimaryId = static_cast<int>(primary.NetworkId());
            PendingRImpactTick = now + static_cast<int>(
                RImpactSeconds(GameObjects::Player().Position().Distance2D(
                    center)) * 1000.0f);
            PendingRManual = true;
            SequenceTargetId = PendingRPrimaryId;
            SequenceExpireTick = PendingRImpactTick + 3800;
        }
    }
}

inline void OnProcessSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    RecordCombatIntent(args);
    if (IsLocalPlayer(args.Sender)) ObserveLocalSpell(args);
}

inline void OnDoCast(
    const SDK::Events::ProcessSpellEventArgs& args) {
    RecordCombatIntent(args);
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return;
    (void)CaptureLocalAutoAttack(
        args, LastLocalAutoTargetId, LastLocalAutoTick);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    LastBeforeAttackTargetId = static_cast<int>(args.Target.NetworkId());
    LastBeforeAttackTick = Now();
    LastBeforeAttackHadMeep = MeepAvailable();
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(
            args, LastAfterAttackTargetId, LastAfterAttackTick)) return;
    const AIHeroClient target = RawEnemyById(LastAfterAttackTargetId);
    if (target.IsValid() && LastBeforeAttackHadMeep &&
        LastBeforeAttackTargetId == LastAfterAttackTargetId &&
        LastAfterAttackTick - LastBeforeAttackTick <= 1250) {
        LastMeepAttackTargetId = LastAfterAttackTargetId;
        LastMeepAttackTick = LastAfterAttackTick;
        ActiveSequence = Sequence::MeepQWeave;
        SequenceTargetId = LastAfterAttackTargetId;
        SequenceExpireTick = Now() + kMeepQWindowMs;
    }
    LastBeforeAttackHadMeep = false;
}

inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(
        args, GapcloserTargetId, GapcloserEndpoint,
        GapcloserExpireTick, 900.0f, 1250);
}

inline void OnInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(
        args, InterruptTargetId, InterruptExpireTick,
        1050, 300, 6000);
}

inline bool BuffNameIs(const SDK::Events::BuffEventArgs& args,
                       std::initializer_list<const char*> names) {
    return ControllerHelpers::TextContainsAny(args.BuffName, names);
}

inline void UpdateBardBuffState(
    const SDK::Events::BuffEventArgs& args,
    bool added) {
    const int now = Now();
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender)) {
        if (BuffNameIs(args, {
                "bardpchimes", "bardp_chimes", "bardchimecount" })) {
            if (added && args.Count >= 0) {
                ChimeCount = std::max(ChimeCount, args.Count);
            }
            MeepMaximum = MeepMaximumCount(ChimeCount);
        }
        if (BuffNameIs(args, {
                "bardpspiritammocount", "bardpspirits",
                "bardp_meepcount" })) {
            MeepAmmo = added ? std::max(1, args.Count) : 0;
            MeepAmmo = std::clamp(MeepAmmo, 0, MeepMaximumCount(ChimeCount));
        }
    }

    if (id != 0 && BuffNameIs(args, {
            "bardrstasis", "bardr_stasis", "temperedfatestasis" })) {
        StasisRecord* record = FindStasis(id, true);
        if (record) {
            record->NetworkId = id;
            record->StartTick = now;
            record->EndTick = added
                ? ControllerHelpers::BuffExpireTick(args, 2500) : now;
            const auto player = GameObjects::Player();
            record->Allied = player.IsValid() &&
                args.Sender.Team ==
                    static_cast<std::uint32_t>(player.Team());
            record->FromRecentBardR =
                std::abs(PendingRImpactTick - now) <= kStasisEventGraceMs ||
                (LastRCastTick > 0 && now - LastRCastTick <= 2900);
            if (added && !record->Allied && record->FromRecentBardR &&
                id == PendingRPrimaryId && Bool(QMenu, "StasisExitQ", true)) {
                ActiveSequence = Sequence::StasisExitQ;
                SequenceTargetId = id;
                SequenceExpireTick = record->EndTick + 850;
            }
        }
    }

    if (id != 0 && BuffNameIs(args, {
            "bardedoor", "bardedoormovement", "bardetravel",
            "magicaljourney" })) {
        const AIHeroClient enemy = RawEnemyById(id);
        if (!enemy.IsValid()) return;
        const PortalRecord* portal = NearestOwnedPortal(
            enemy.Position(), 360.0f);
        if (!portal || !portal->Geometry.Valid) return;
        PortalTravellerId = id;
        PortalTravellerExit = portal->Geometry.Exit;
        const int fallback = std::max(
            120, static_cast<int>(portal->Geometry.EnemyTravelSeconds *
                                  1000.0f));
        PortalTravellerUntil = added
            ? ControllerHelpers::BuffExpireTick(args, fallback) : now;
        if (added && Bool(QMenu, "PortalExitQ", true)) {
            ActiveSequence = Sequence::PortalExitQ;
            SequenceTargetId = id;
            SequenceExpireTick = PortalTravellerUntil + 900;
        }
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    UpdateBardBuffState(args, true);
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    UpdateBardBuffState(args, false);
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    UpdateBardBuffState(args, true);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    const int now = Now();
    const std::uint32_t playerId =
        static_cast<std::uint32_t>(player.NetworkId());

    if (IsShrineObject(args) && ObjectEventIsAllied(args)) {
        ShrineRecord* shrine = FindShrine(id, true);
        if (!shrine) return;
        shrine->NetworkId = id;
        shrine->Position = args.Sender.Position;
        shrine->SpawnTick = now;
        shrine->LastSeenTick = now;
        shrine->ConfirmedOwned = args.SourceNetworkId == playerId ||
            args.Source.NetworkId == playerId ||
            (LastWCastTick > 0 && now - LastWCastTick <= 1100 &&
             (!PendingWPosition.IsValid() || PendingWPosition.IsZero() ||
              shrine->Position.Distance2D(PendingWPosition) <= 220.0f));
        return;
    }
    if (IsChimeObject(args)) {
        ChimeRecord* chime = FindChime(id, true);
        if (!chime) return;
        chime->NetworkId = id;
        chime->Position = args.Sender.Position;
        chime->SpawnTick = now;
        chime->ExpireTick = now + 600000;
        chime->Visible = args.Sender.IsVisible;
        return;
    }
    if (IsPortalObject(args) && ObjectEventIsAllied(args)) {
        PortalRecord* portal = FindPortal(id, true);
        if (!portal) return;
        portal->NetworkId = id;
        portal->SpawnTick = now;
        portal->ExpireTick = now + static_cast<int>(
            kEPortalSeconds * 1000.0f);
        portal->Geometry = PendingPortalUntil >= now
            ? PendingPortalGeometry : LastEPlan.Portal;
        portal->ConfirmedOwned = args.SourceNetworkId == playerId ||
            args.Source.NetworkId == playerId ||
            (LastECastTick > 0 && now - LastECastTick <= 1200);
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (ShrineRecord* shrine = FindShrine(id)) *shrine = {};
    if (ChimeRecord* chime = FindChime(id)) *chime = {};
    if (PortalRecord* portal = FindPortal(id)) *portal = {};
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::LaneGuard: return "lane guard";
    case Posture::Catch: return "catch";
    case Posture::FrontToBack: return "front-to-back";
    case Posture::Peel: return "peel";
    case Posture::Roam: return "roam";
    case Posture::Objective: return "objective";
    case Posture::Dive: return "dive";
    case Posture::Disengage: return "disengage";
    case Posture::Farm: return "farm";
    default: return "neutral";
    }
}

inline const char* SequenceName(Sequence sequence) {
    switch (sequence) {
    case Sequence::MeepQWeave: return "meep-AA -> Q -> AA";
    case Sequence::StasisExitQ: return "R exit Q";
    case Sequence::PortalExitQ: return "portal exit Q";
    case Sequence::PortalBait: return "portal bait";
    case Sequence::DiveReset: return "turret stasis reset";
    case Sequence::SaveStasis: return "ally save";
    case Sequence::ObjectiveDeny: return "objective deny";
    case Sequence::PlayerLed: return "player-led";
    default: return "idle";
    }
}

inline const ChimeRecord* BestChimeSuggestion(float& score) {
    score = -FLT_MAX;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return nullptr;
    const AIHeroClient carry = RawAllyById(ProtectedAllyId);
    const Vector3 destination = Game::CursorPos();
    const float direct = player.Position().Distance2D(destination);
    const bool objectiveUrgent =
        ControllerHelpers::HasNearbyEpicMonster(1700.0f) &&
        (Engine::CountEnemiesAt(player.Position(), 1800.0f) > 0 ||
         Engine::CountAlliesAt(player.Position(), 1800.0f) > 1);
    const ChimeRecord* best = nullptr;
    for (const auto& chime : Chimes) {
        if (chime.NetworkId == 0 || chime.ExpireTick < Now() ||
            !chime.Position.IsValid() || chime.Position.IsZero()) continue;
        const auto projection = SharedGeometry::ProjectPointToSegment2D(
            chime.Position, player.Position(), destination);
        ChimeRouteContext context{};
        context.DirectPathDistance = direct;
        context.ChimePathDistance =
            player.Position().Distance2D(chime.Position) +
            chime.Position.Distance2D(destination);
        context.ExpireSeconds = static_cast<float>(
            chime.ExpireTick - Now()) / 1000.0f;
        context.CarryCanFarmSafely = !carry.IsValid() ||
            Engine::CountEnemiesAt(carry.Position(), 900.0f) == 0;
        context.AllyLaneSafety = context.CarryCanFarmSafely ? 1.0f : 0.0f;
        context.ObjectiveUrgency = objectiveUrgent ? 1.0f : 0.0f;
        context.OnPrimaryRoute = projection.Distance <= 145.0f;
        const float candidate = ChimeRouteScore(context);
        if (candidate > score) {
            score = candidate;
            best = &chime;
        }
    }
    return best;
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (Bool(CoachMenu, "DrawRanges", true)) {
        Drawing::DrawCircle(player.Position(), kQInitialTargetRange,
                            0x3356B9E9u, 1.0f, 72);
        Drawing::DrawCircle(player.Position(), kWCastRange,
                            0x3358D68Du, 1.0f, 72);
        Drawing::DrawCircle(player.Position(), kECastRange,
                            0x33E4B95Cu, 1.0f, 72);
    }
    if (Bool(CoachMenu, "DrawQ", true) && LastQPlan.Valid) {
        Drawing::DrawLine(
            player.Position(), LastQPlan.Evaluation.CastPosition,
            0xDDA4E6FFu, 2.0f);
        if (!LastQPlan.Evaluation.FirstContact.IsZero()) {
            Drawing::DrawCircle(
                LastQPlan.Evaluation.FirstContact, 48.0f,
                LastQPlan.Evaluation.FirstStunned
                    ? 0xDD64E6A8u : 0xDDE7C967u,
                1.8f, 30);
        }
        if (!LastQPlan.Evaluation.SecondContact.IsZero()) {
            Drawing::DrawCircle(
                LastQPlan.Evaluation.SecondContact, 54.0f,
                0xDDB56DFFu, 1.8f, 30);
            Drawing::DrawLine(
                LastQPlan.Evaluation.FirstContact,
                LastQPlan.Evaluation.SecondContact,
                0xCCB56DFFu, 2.0f);
        }
    }
    if (Bool(CoachMenu, "DrawShrines", true)) {
        for (const auto& shrine : GeometryShrines()) {
            const float charge = WShrineChargeFraction(shrine.AgeSeconds);
            Drawing::DrawCircle(
                shrine.Position, kWShrineRadius,
                charge >= 0.99f ? 0xDD65E69Eu : 0xAAE9C75Fu,
                charge >= 0.99f ? 2.0f : 1.2f, 36);
        }
    }
    if (Bool(CoachMenu, "DrawPortal", true) && LastEPlan.Valid) {
        Drawing::DrawLine(
            LastEPlan.Portal.Entrance, LastEPlan.Portal.Exit,
            LastEPlan.Defensive ? 0xDD62E7B0u : 0xDDE8BD60u,
            2.4f);
        Drawing::DrawCircle(
            LastEPlan.Portal.Exit, 72.0f,
            LastEPlan.Valid ? 0xCC62E7B0u : 0xCCEF6363u,
            1.8f, 32);
    }
    if (Bool(CoachMenu, "DrawR", true) && LastRPlan.Valid) {
        Drawing::DrawCircle(
            LastRPlan.Evaluation.Center, kRRadius,
            LastRPlan.Evaluation.FriendlyGrief == 0
                ? 0xDDE9C55Eu : 0xDDEF6363u,
            2.2f, 64);
        Drawing::DrawLine(
            player.Position(), LastRPlan.Evaluation.Center,
            0x88E9C55Eu, 1.3f);
    }
    if (Bool(PassiveMenu, "DrawChimeRoute", true)) {
        float routeScore = -FLT_MAX;
        const ChimeRecord* chime = BestChimeSuggestion(routeScore);
        if (chime && routeScore >= Slider(PassiveMenu, "RouteScore", 180)) {
            Drawing::DrawLine(
                player.Position(), chime->Position,
                0xCC77DDF1u, 1.8f);
            Drawing::DrawCircle(
                chime->Position, 75.0f,
                0xDD77DDF1u, 2.0f, 36);
            Vec2 chimeScreen{};
            if (Drawing::WorldToScreen(chime->Position, chimeScreen)) {
                Drawing::DrawText(
                    chimeScreen.x - 72.0f, chimeScreen.y - 40.0f,
                    0xFF8DE8F5u, "safe route chime");
            }
        }
    }
    if (Bool(CoachMenu, "DrawProtectedAlly", true)) {
        const AIHeroClient ally = RawAllyById(ProtectedAllyId);
        if (ally.IsValid()) {
            Drawing::DrawCircle(
                ally.Position(), ally.BoundingRadius() + 55.0f,
                0xAA72E7A7u, 1.6f, 36);
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char state[640]{};
            _snprintf_s(
                state, sizeof(state), _TRUNCATE,
                "Bard OTP | %s | %s | chimes %d | meeps %d/%d | shrines %d | owner %s",
                PostureName(CurrentPosture), SequenceName(ActiveSequence),
                ChimeCount, MeepAmmo, MeepMaximum, ActiveShrineCount(),
                PlayerOverrideUntil >= Now() ? "player" : "controller");
            Drawing::DrawText(
                screen.x - 285.0f, screen.y - 112.0f,
                0xFFE5CB68u, state);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "BardOneTrick", "Bard one-trick conductor"));
    TacticsMenu->Add(new MenuBool(
        "KillSecure", "Exact mitigated Q dmg", true));
    TacticsMenu->Add(new MenuSlider(
        "DefensiveHp", "Defensive posture HP (%)",
        42, 10, 90));
    TacticsMenu->Add(new MenuSlider(
        "ManualOwnershipMs", "Yield player spell (ms)",
        520, 180, 1200));
    TacticsMenu->Add(new MenuSeparator(
        "Ownership",
        "Movement, attacks, Flash,"));

    RoleMenu = TacticsMenu->AddSubMenu(new Menu(
        "Conductor", "Carry protection and catch posture"));
    RoleMenu->Add(new MenuBool(
        "RespectCursor", "Require cursor agreement for", true));
    RoleMenu->Add(new MenuSeparator(
        "Carry",
        "The ally is re-ranked from"));

    PassiveMenu = TacticsMenu->AddSubMenu(new Menu(
        "TravelersCall", "Chime and meep route policy"));
    PassiveMenu->Add(new MenuBool(
        "DrawChimeRoute", "Suggest chimes that fit the", true));
    PassiveMenu->Add(new MenuSlider(
        "RouteScore", "Minimum safe chime route score", 180, -300, 500));
    PassiveMenu->Add(new MenuSeparator(
        "NoMovement",
        "Chimes are advisory only;"));

    QMenu = TacticsMenu->AddSubMenu(new Menu(
        "CosmicBinding", "Ordered Q1-Q2 collision and weave"));
    QMenu->Add(new MenuList(
        "Hitchance", "Ordinary Q prediction",
        { "Medium", "High", "Very high", "Immobile only" }, 1));
    QMenu->Add(new MenuSlider(
        "HarassMana", "Minimum harass mana (%)", 58, 0, 100));
    QMenu->Add(new MenuBool(
        "StasisExitQ", "Q at Bard R stasis end", true));
    QMenu->Add(new MenuSlider(
        "StasisEarlyMs", "R-exit Q early bias (ms)", 45, 0, 140));
    QMenu->Add(new MenuBool(
        "PortalExitQ", "Q at Bard portal exit", true));
    QMenu->Add(new MenuSlider(
        "PortalEarlyMs", "Portal-exit Q early bias (ms)", 55, 0, 160));
    QMenu->Add(new MenuBool(
        "FleeQ", "Slow or stun the closest", true));
    QMenu->Add(new MenuSeparator(
        "MeepFirst",
        "In attack range Bard waits"));

    WMenu = TacticsMenu->AddSubMenu(new Menu(
        "CaretakersShrine", "Direct heal, speed and shrine economy"));
    WMenu->Add(new MenuBool(
        "EmergencyDirect", "W direct on ally", true));
    WMenu->Add(new MenuSlider(
        "EmergencyHp", "Emergency ally HP (%)", 34, 5, 80));
    WMenu->Add(new MenuSlider(
        "EmergencyScore", "Min heal score", 520, 100, 1400));
    WMenu->Add(new MenuBool(
        "CombatSpeed", "Use direct W for a valuable", true));
    WMenu->Add(new MenuBool(
        "RetreatSpeed", "W on retreating carry", true));
    WMenu->Add(new MenuBool(
        "GroundShrines", "Prepare spaced shrines", true));
    WMenu->Add(new MenuBool(
        "ReserveLast", "Keep the final W charge", true));

    EMenu = TacticsMenu->AddSubMenu(new Menu(
        "MagicalJourney", "Terrain trace and player-led portal"));
    EMenu->Add(new MenuBool(
        "DefensiveEscape", "Create a safe portal vs", true));
    EMenu->Add(new MenuBool(
        "FleePortal", "Create a cursor-aligned", true));
    EMenu->Add(new MenuBool(
        "RespectDashHazards", "Reject anti-dash exits", true));
    EMenu->Add(new MenuSlider(
        "MinimumSafety", "Minimum portal safety score", 220, -200, 900));
    EMenu->Add(new MenuKeyBind(
        "PlayerPortal", "Trace best portal through cursor [G]",
        SDK::Keys::G, KeyBindType::Press));
    EMenu->Add(new MenuSeparator(
        "PlayerEntry",
        "The controller creates a"));

    RMenu = TacticsMenu->AddSubMenu(new Menu(
        "TemperedFate", "Mixed-team stasis and no-grief policy"));
    RMenu->Add(new MenuBool(
        "AutomaticCatch", "Catch an overextended target", true));
    RMenu->Add(new MenuSlider(
        "MaximumCatchRange", "Maximum automatic catch range", 1650, 700, 2800));
    RMenu->Add(new MenuSlider(
        "CatchFollowup", "Allies to follow an auto catch", 1, 0, 4));
    RMenu->Add(new MenuBool(
        "BacklineIsolation", "Stasis backline pocket", true));
    RMenu->Add(new MenuSlider(
        "IsolationRange", "Maximum isolation R range", 1850, 800, 2800));
    RMenu->Add(new MenuBool(
        "SaveAlly", "Stasis an ally only vs", true));
    RMenu->Add(new MenuSlider(
        "SaveAllyHp", "Ally save thresh (%)", 28, 5, 65));
    RMenu->Add(new MenuBool(
        "Interrupt", "R when Q cannot interrupt in", true));
    RMenu->Add(new MenuSlider(
        "FleeMinimum", "Pursuers required for flee R", 2, 1, 5));
    RMenu->Add(new MenuBool(
        "FleeR", "R multi-pursuer escape", true));
    RMenu->Add(new MenuBool(
        "AssistManualR", "Q exit after player R", true));
    RMenu->Add(new MenuKeyBind(
        "ManualCatch", "Manual no-grief catch near cursor [T]",
        SDK::Keys::T, KeyBindType::Press));
    RMenu->Add(new MenuKeyBind(
        "DiveTurret", "Stasis enemy turret for a player-led dive [H]",
        SDK::Keys::H, KeyBindType::Press));
    RMenu->Add(new MenuKeyBind(
        "ObjectiveDeny", "Deny contested epic objective [J]",
        SDK::Keys::J, KeyBindType::Press));
    RMenu->Add(new MenuSeparator(
        "NoGrief",
        "Automatic R rejects allied"));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Exact two-body Q farming only"));
    FarmMenu->Add(new MenuBool(
        "UseQ", "Use Q for ordered double", true));
    FarmMenu->Add(new MenuBool(
        "HoldForChampion", "Hold Q vs contest", true));
    FarmMenu->Add(new MenuSlider(
        "ChampionHoldRange", "Champion contest range", 1200, 500, 1800));
    FarmMenu->Add(new MenuSlider(
        "LaneMana", "Minimum lane/last-hit mana (%)", 62, 0, 100));
    FarmMenu->Add(new MenuSlider(
        "JungleMana", "Minimum jungle mana (%)", 30, 0, 100));
    FarmMenu->Add(new MenuSeparator(
        "NoWaste",
        "W, E and R are never farming"));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "Bard one-trick geometry and state"));
    CoachMenu->Add(new MenuBool(
        "DrawRanges", "Draw Q/W/E cast ranges", false));
    CoachMenu->Add(new MenuBool(
        "DrawQ", "Draw Q shackle", false));
    CoachMenu->Add(new MenuBool(
        "DrawShrines", "Draw shrine charge state", false));
    CoachMenu->Add(new MenuBool(
        "DrawPortal", "Draw portal entry/exit", false));
    CoachMenu->Add(new MenuBool(
        "DrawR", "Draw the last evaluated R", false));
    CoachMenu->Add(new MenuBool(
        "DrawProtectedAlly", "Mark protected carry", false));
    CoachMenu->Add(new MenuBool(
        "DrawState", "Draw posture/seq, chimes", false));
}

inline void OnLoad() {
    Shrines.fill({});
    Chimes.fill({});
    Portals.fill({});
    StasisRecords.fill({});
    FocusRecords.fill({});
    AllyThreats.fill({});
    CurrentPosture = Posture::Neutral;
    ActiveSequence = Sequence::None;
    LastQPurpose = QPurpose::None;
    LastWPurpose = WPurpose::None;
    LastEPurpose = EPurpose::None;
    LastRPurpose = RPurpose::None;
    LastQPlan = {};
    LastWPlan = {};
    LastEPlan = {};
    LastRPlan = {};
    ChimeCount = 0;
    MeepAmmo = 0;
    MeepMaximum = 1;
    ProtectedAllyId = 0;
    LastBeforeAttackTargetId = LastBeforeAttackTick = 0;
    LastBeforeAttackHadMeep = false;
    LastAfterAttackTargetId = LastAfterAttackTick = 0;
    LastMeepAttackTargetId = LastMeepAttackTick = 0;
    LastLocalAutoTargetId = LastLocalAutoTick = 0;
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    PlayerOverrideUntil = 0;
    SequenceTargetId = SequenceExpireTick = 0;
    PortalTravellerId = PortalTravellerUntil = 0;
    PortalTravellerExit = {};
    PendingWPosition = PendingPortalCast = {};
    PendingPortalGeometry = {};
    PendingPortalUntil = 0;
    PendingRPrimaryId = PendingRImpactTick = 0;
    PendingRManual = false;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    InterruptTargetId = InterruptExpireTick = 0;
    RefreshRuntimeState();
}

inline void OnUnload() {
    TacticsMenu = RoleMenu = PassiveMenu = QMenu = WMenu = nullptr;
    EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Pin Summoner's Rift behavior to Riot 26.14 and CommunityDragon PC 16.14",
    "Use patch 26.13 passive damage 30 plus six per five chimes plus 40 percent AP",
    "Track every meep maximum breakpoint from one through nine",
    "Track meep recharge breakpoints at 20, 40, 55 and 70 chimes",
    "Track meep slow breakpoints from 5 through 85 chimes",
    "Track normal and expanded meep splash breakpoints",
    "Treat chimes as advisory route opportunities rather than movement orders",
    "Reject a chime detour when the protected carry cannot farm safely",
    "Reject a chime detour during an urgent nearby epic objective",
    "Prefer chimes already lying on the player's route",
    "Track ten-minute chime lifetime from object creation",
    "Use current chime 12 percent maximum-mana restoration as research context",
    "Use current chime out-of-combat movement stacking as research context",
    "Use current chime experience growth after minute five as research context",
    "Use Q gameplay first-target range 850 rather than blindly using resource range 950",
    "Continue Q exactly 300 units after first contact",
    "Use Q half-width 60, cast time 0.25 and speed 1500",
    "Use current Q 80 through 240 base damage plus 80 percent AP",
    "Use current Q 60 percent slow",
    "Use current rank-scaled one through 1.8 second disable",
    "Order every champion, minion and monster collision by ray entry distance",
    "Ignore structures, wards and traps as Q collision bodies",
    "Require the intended target to be the real first body",
    "Allow only one second unit after first contact",
    "Allow terrain to shackle only after a first unit is struck",
    "Reject terrain encountered before any first unit",
    "Scan actual NavMesh samples instead of using target-near-wall heuristics",
    "Aim through the side of a hitbox to exploit real continuation geometry",
    "Generate pair-alignment candidates for champion-to-champion Q",
    "Generate pair-alignment candidates for champion-to-minion Q",
    "Generate pair-alignment candidates for champion-to-monster Q",
    "Resolve a wall versus second unit by whichever collision happens first",
    "Model first-target spell shield allowing the projectile to continue",
    "Model a shielded first target not stunning from a second unit",
    "Model a shielded first target still stunning against terrain",
    "Model a shielded second target blocking only its own damage and stun",
    "Reject ordinary Q into parry, pool, rappel and untargetable states",
    "Respect projectile-intercept walls before issuing Q",
    "Require configurable prediction for ordinary Q",
    "Lower no prediction standards only for immobile or dashing targets",
    "Prefer a real wall stun over a raw slow",
    "Prefer a verified double stun over a single stun",
    "Use exact mitigated Q damage for kill secure",
    "Never call a raw Q slow a stun",
    "Wait for meep-AA before ordinary Q when already in attack range",
    "Bypass meep wait for an immediate stun",
    "Bypass meep wait for an interrupt",
    "Bypass meep wait for lethal Q",
    "Continue meep-AA with Q and leave the second AA to Orbwalker",
    "Never issue attacks or movement from the champion controller",
    "Time Q arrival to the actual Bard R stasis end buff",
    "Use a configurable early bias for latency on R-exit Q",
    "Build R-exit Q against a frozen target position even while untargetable",
    "Search second bodies and real walls for the R-exit Q shackle",
    "Time Q against an enemy's expected portal exit",
    "Assist only portals confirmed as Bard's own",
    "Do not append portal Q to a manual E without a confirmed traveller",
    "Use exact ordered double-body geometry for lane Q",
    "Use exact mitigated health for last-hit Q",
    "Require two lane bodies unless an exact last hit is secured",
    "Keep W, E and R out of farming",
    "Hold farming Q while an enemy champion contests the wave",
    "Respect Orbwalker windup before farming Q",
    "Use current W two-ammo and 18-second recharge economy",
    "Track no more than three owned ground shrines",
    "Use current W 800 range and 100 shrine radius",
    "Use current five-second shrine charge time",
    "Interpolate shrine healing rather than treating every shrine as fully charged",
    "Use current minimum W heal 25 through 125 plus 40 percent AP",
    "Use current maximum W heal 50 through 200 plus 70 percent AP",
    "Use current W move speed and AP scaling",
    "Direct-cast W on an ally without deleting an oldest ground shrine",
    "Score emergency W from health, incoming pressure, targeting and ally value",
    "Account for tracked grievous-wounds buff names in emergency heal value",
    "Override the final-charge reserve only for urgent direct healing",
    "Reserve the final W charge during ordinary combat",
    "Use direct W for a retreating carry before using a portal",
    "Space ground shrines so their trigger circles do not overlap",
    "Reject ground shrines in terrain or under immediate enemy pressure",
    "Prepare ground shrines before nearby epic objectives",
    "Prepare retreat shrines only while guarding a carry",
    "Track shrine ownership from source id or recent cast position",
    "Use current E cast range 900 and maximum tunnel length 2600",
    "Trace actual continuous terrain for portal entrance and exit",
    "Reject terrain segments shorter than a real portal",
    "Reject exits still inside terrain",
    "Use current enemy portal speed 900 and allied speed 1197",
    "Score exit ally count, enemy count and threat separation",
    "Reject defensive exits under enemy turret",
    "Reject exits beside ready point-click lockdown",
    "Reject anti-dash exits",
    "Require cursor agreement for player-led portal direction",
    "Keep lazy travel portals disabled",
    "Never make Bard walk into a created portal",
    "Expose proactive portal creation only on a player key",
    "Allow automatic E only for committed defensive pressure or flee mode",
    "Use current R range 3400 and radius 350",
    "Use current R 0.5-second cast and distance-scaled travel",
    "Use current R 2.5-second stasis duration",
    "Predict every champion to R impact rather than current position",
    "Include epic monsters, turrets and plants in R hit evaluation",
    "Treat allied and enemy champions in the same R circle",
    "Reject automatic R that catches a healthy ally",
    "Reject automatic R that cancels an allied channel",
    "Reject automatic R on an enemy already focused by an ally",
    "Reject automatic R on a focused low-health enemy",
    "Require allied follow-up before an automatic catch",
    "Cap automatic catch range to avoid low-quality max-range ultimates",
    "Respect cursor direction for automatic catches",
    "Isolate an unfocused backline pocket without freezing its frontline",
    "Require two allied follow-up bodies for backline isolation",
    "Use R ally save only against tracked lethal pressure",
    "Reject ally-save R when its predicted impact follows the lethal incoming hit",
    "Let a saved protected carry outweigh ordinary stasis value",
    "Try Q before spending R on an interruptible channel",
    "Use flee R only against the configured number of pursuers",
    "Stasis a dive turret only behind a player-held key",
    "Reject dive-turret R if an ally is already inside the stasis circle",
    "Deny a contested epic objective only behind a player-held key",
    "Reject objective R while allies are freely securing it",
    "Track allied attacks and casts to prevent hostile R grief",
    "Infer untargeted allied skillshot focus from its cast segment and endpoint",
    "Track enemy targeted pressure on every allied champion",
    "Track untargeted line-crossing threats against every allied champion",
    "Dynamically select the ally most expensive to leave unprotected",
    "Yield a configurable ownership window after every manual spell",
    "Never auto-complete a manual Q, W or E input",
    "Optionally assist manual R only after actual stasis is observed",
    "Never automate Flash for Q-Flash or any Bard extension",
    "Draw selected Q contacts rather than a generic line",
    "Draw each shrine's real charge state",
    "Draw portal entrance, exit and safety classification",
    "Draw R no-grief status from the evaluated mixed-team set",
    "Draw the dynamically protected ally",
    "Expose chimes, meeps, shrines, posture, sequence and spell ownership",
    "Own Bard's complete spell decision loop without generic Q-W-E-R fallback",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Bard";
    controller.ControllerId = "champion.kuroaio.ai.bard.controller";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIBard.md";
    controller.ImplementationSummary =
        "Ordered two-stage Q with meep-first weaving and exact stasis/portal "
        "exit timing, direct/ground W economy, terrain-traced player-led E, "
        "and mixed-team no-grief R catch/save/dive/objective policy.";
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
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    return controller;
}();

// AIBARD_CONTROLLER_CONTINUE

} // namespace Plugins::KuroAIO::AI::Controllers::Bard