#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIAzirGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Azir {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::CursorDirectionAgrees;
using ControllerHelpers::EnemyFlashReady;
using ControllerHelpers::EnemySpellReady;
using ControllerHelpers::HasAnyBuff;
using ControllerHelpers::HasCurrentResource;
using ControllerHelpers::HasEnemyChampionNear;
using ControllerHelpers::HasNearbyJungleTarget;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HasResourceFor;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsLargeLaneMinion;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearTerrain;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::ObjectEventIsAllied;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocks;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContainsAny;
using ControllerHelpers::SpellRank;
using ControllerHelpers::UnitByNetworkId;
using ControllerHelpers::ValidHostileUnitInGameplayRange;

enum class Posture : std::uint8_t {
    Neutral,
    LaneControl,
    FrontToBack,
    ExtendedDps,
    Pick,
    Shuffle,
    Peel,
    Farm,
    Flee,
};

enum class QPurpose : std::uint8_t {
    None,
    Manual,
    LateTrade,
    ExtendDps,
    ReacquireTarget,
    KillSecure,
    Drift,
    Shuffle,
    Revenant,
    WaveFormation,
    JungleFormation,
    FleeRedirect,
};

enum class WPurpose : std::uint8_t {
    None,
    Manual,
    FirstTradeSoldier,
    ExtendDps,
    DefensiveAnchor,
    ShuffleAnchor,
    CollisionRefund,
    FollowupLanding,
    WaveZone,
    JungleZone,
    FleeAnchor,
};

enum class EPurpose : std::uint8_t {
    None,
    Manual,
    CollisionKill,
    CollisionRefund,
    DefensiveShield,
    AntiGapcloser,
    DriftEngage,
    Shuffle,
    Revenant,
    Flee,
};

struct SoldierRecord {
    int NetworkId = 0;
    Vector3 Position = {};
    int SpawnTick = 0;
    int ExpireTick = 0;
    int LastSeenTick = 0;
    bool ConfirmedOwned = false;
    bool NearEnemyTurret = false;
};

struct EnemyWindow {
    int NetworkId = 0;
    int CommittedUntil = 0;
    int HardCrowdControlSpentUntil = 0;
    int MobilitySpentUntil = 0;
};

struct RuinRecord {
    int NetworkId = 0;
    Vector3 Position = {};
    int LastSeenTick = 0;
    bool Valid = false;
};

struct QPlan {
    QEvaluation Evaluation = {};
    int TargetId = 0;
    QPurpose Purpose = QPurpose::None;
    float RawDamage = 0.0f;
    float Score = -FLT_MAX;
    bool EscapeAnchorLost = false;
    bool Valid = false;
};

struct WPlan {
    Vector3 CastPosition = {};
    int TargetId = 0;
    WPurpose Purpose = WPurpose::None;
    int FarmHits = 0;
    float Score = -FLT_MAX;
    bool CreatesCoverage = false;
    bool ExistingEscapeAnchor = false;
    bool NearTurret = false;
    bool Valid = false;
};

struct EPlan {
    int AnchorId = 0;
    Vector3 AnchorPosition = {};
    Vector3 RedirectPosition = {};
    Vector3 FinalEndpoint = {};
    DriftResult Drift = {};
    DashResult Direct = {};
    int TargetId = 0;
    EPurpose Purpose = EPurpose::None;
    float Score = -FLT_MAX;
    bool NeedsQ = false;
    bool Defensive = false;
    bool Valid = false;
};

struct RPlan {
    REvaluation Evaluation = {};
    int TargetId = 0;
    RPurpose Purpose = RPurpose::None;
    float RawDamage = 0.0f;
    float Score = -FLT_MAX;
    bool Defensive = false;
    bool Valid = false;
};

struct MobilityRule {
    const char* Champion = "";
    SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
};

inline constexpr std::array<MobilityRule, 28> EscapeRules = {
    MobilityRule{ "Ahri", SDK::SpellSlot::R },
    MobilityRule{ "Akali", SDK::SpellSlot::E },
    MobilityRule{ "Akshan", SDK::SpellSlot::E },
    MobilityRule{ "Caitlyn", SDK::SpellSlot::E },
    MobilityRule{ "Corki", SDK::SpellSlot::W },
    MobilityRule{ "Ekko", SDK::SpellSlot::E },
    MobilityRule{ "Ezreal", SDK::SpellSlot::E },
    MobilityRule{ "Fizz", SDK::SpellSlot::E },
    MobilityRule{ "Graves", SDK::SpellSlot::E },
    MobilityRule{ "Kaisa", SDK::SpellSlot::R },
    MobilityRule{ "Kassadin", SDK::SpellSlot::R },
    MobilityRule{ "Katarina", SDK::SpellSlot::E },
    MobilityRule{ "Khazix", SDK::SpellSlot::E },
    MobilityRule{ "Leblanc", SDK::SpellSlot::W },
    MobilityRule{ "Lucian", SDK::SpellSlot::E },
    MobilityRule{ "Naafiri", SDK::SpellSlot::W },
    MobilityRule{ "Nidalee", SDK::SpellSlot::W },
    MobilityRule{ "Qiyana", SDK::SpellSlot::E },
    MobilityRule{ "Rakan", SDK::SpellSlot::E },
    MobilityRule{ "Shaco", SDK::SpellSlot::Q },
    MobilityRule{ "Tristana", SDK::SpellSlot::W },
    MobilityRule{ "Vayne", SDK::SpellSlot::Q },
    MobilityRule{ "Yasuo", SDK::SpellSlot::E },
    MobilityRule{ "Yone", SDK::SpellSlot::E },
    MobilityRule{ "Zed", SDK::SpellSlot::W },
    MobilityRule{ "Zeri", SDK::SpellSlot::E },
    MobilityRule{ "Zoe", SDK::SpellSlot::R },
    MobilityRule{ "Smolder", SDK::SpellSlot::E },
};

inline Menu* TacticsMenu = nullptr;
inline Menu* SoldierMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* PassiveMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<SoldierRecord, 12> SoldierRecords = {};
inline std::array<EnemyWindow, 10> EnemyWindows = {};
inline std::array<RuinRecord, 12> TurretRuins = {};
inline SequenceState ActiveSequence = {};
inline Posture CurrentPosture = Posture::Neutral;
inline Mode LastKnownMode = Mode::None;

inline QPurpose LastQPurpose = QPurpose::None;
inline WPurpose LastWPurpose = WPurpose::None;
inline EPurpose LastEPurpose = EPurpose::None;
inline RPurpose LastRPurpose = RPurpose::None;
inline QPlan LastQPlan = {};
inline WPlan LastWPlan = {};
inline EPlan LastEPlan = {};
inline RPlan LastRPlan = {};

inline int LastBeforeAttackTargetId = 0;
inline int LastBeforeAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int LastSoldierAttackTargetId = 0;
inline int LastSoldierAttackTick = 0;
inline int LastLocalSpellTick = 0;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int EDashUntil = 0;
inline int PlayerOverrideUntil = 0;
inline int PendingAnchorId = 0;
inline int PendingAnchorUntil = 0;
inline Vector3 PendingWPosition = {};
inline Vector3 PendingQPosition = {};
inline Vector3 SequenceOrigin = {};
inline bool SequenceManualIntent = false;
inline Vector3 LastKnownPlayerPosition = {};
inline int PassiveReadyTick = 0;

inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int IncomingThreatTargetId = 0;
inline int IncomingThreatUntil = 0;
inline bool IncomingHardCrowdControl = false;

inline bool IsSandSoldierName(const std::string& character,
                              const std::string& name) {
    return SoldierRules::IsSandSoldierName(character) ||
           SoldierRules::IsSandSoldierName(name);
}

inline bool IsSandSoldierObject(
    const SDK::Events::ObjectEventArgs& args) {
    return SoldierRules::IsSandSoldierName(args.Sender.CharacterName) ||
           SoldierRules::IsSandSoldierName(args.Sender.Name) ||
           SoldierRules::IsSandSoldierName(args.SpellName);
}

inline bool IsSoldierAttackEvent(
    const SDK::Events::ProcessSpellEventArgs& args) {
    return SoldierRules::IsSoldierAttackSpellName(args.SpellName) ||
           SoldierRules::IsSoldierAttackSpellName(args.ScriptName) ||
           SoldierRules::IsSoldierAttackSpellName(args.MissileName) ||
           SoldierRules::IsSoldierAttackSpellName(args.SpellSlotName) ||
           SoldierRules::IsSoldierAttackSpellName(args.PayloadSpellName) ||
           SoldierRules::IsSoldierAttackSpellName(args.PayloadMissileName);
}

inline bool IsOwnedSoldierSender(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !args.Sender.IsValid() ||
        args.Sender.Team != static_cast<std::uint32_t>(player.Team()) ||
        (!SoldierRules::IsSandSoldierName(args.Sender.CharacterName) &&
         !SoldierRules::IsSandSoldierName(args.Sender.Name))) {
        return false;
    }
    const int id = static_cast<int>(args.Sender.NetworkId);
    for (const auto& record : SoldierRecords) {
        if (record.NetworkId == id && id != 0) return true;
    }
    return SoldierRules::IsCommandable(
        Point2(player.Position()), Point2(args.Sender.Position));
}

inline bool IsQEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
           SpellEventNameContainsAny(args, {
               "azirqwrapper", "azirq", "conqueringsands",
           });
}

inline bool IsWEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
           SpellEventNameContainsAny(args, {
               "azirw", "azirwspawnsoldier", "arise",
           });
}

inline bool IsEEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
           SpellEventNameContainsAny(args, {
               "azirewrapper", "azire", "shiftingsands",
           });
}

inline bool IsREvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
           SpellEventNameContainsAny(args, {
               "azirr", "emperorsdivide",
           });
}

inline bool IsRuinName(const char* character, const char* name) {
    return ControllerHelpers::AnyTextContains(
        { character, name },
        { "turretruin", "turretrubble", "towerrubble",
          "turretremnant", "azirpassivetarget" });
}

inline SoldierRecord* FindSoldierRecord(int networkId,
                                        bool create = false) {
    if (networkId == 0) return nullptr;
    SoldierRecord* empty = nullptr;
    for (auto& record : SoldierRecords) {
        if (record.NetworkId == networkId) return &record;
        if (!empty && record.NetworkId == 0) empty = &record;
    }
    if (!create) return nullptr;
    if (!empty) {
        empty = &*std::min_element(
            SoldierRecords.begin(), SoldierRecords.end(),
            [](const SoldierRecord& left, const SoldierRecord& right) {
                return left.ExpireTick < right.ExpireTick;
            });
    }
    *empty = {};
    empty->NetworkId = networkId;
    return empty;
}

inline EnemyWindow* FindEnemyWindow(int networkId,
                                    bool create = false) {
    return ControllerHelpers::FindEnemyCastWindow(
        EnemyWindows, networkId, create);
}

inline RuinRecord* FindRuin(int networkId, bool create = false) {
    if (networkId == 0) return nullptr;
    RuinRecord* empty = nullptr;
    for (auto& ruin : TurretRuins) {
        if (ruin.NetworkId == networkId) return &ruin;
        if (!empty && ruin.NetworkId == 0) empty = &ruin;
    }
    if (!create) return nullptr;
    if (!empty) empty = &TurretRuins.front();
    *empty = {};
    empty->NetworkId = networkId;
    return empty;
}

inline bool NearEnemyTurret(const Vector3& position,
                            float padding = 0.0f) {
    if (!position.IsValid() || position.IsZero()) return false;
    for (const auto& turret : GameObjects::EnemyTurrets()) {
        if (!turret.IsValid() || turret.IsDead()) continue;
        const float range = std::max(775.0f, turret.AttackRange()) +
                            std::max(0.0f, padding);
        if (turret.Position().DistanceSqr2D(position) <= range * range) {
            return true;
        }
    }
    return false;
}

inline void RefreshSoldiers() {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    const auto& soldiers = SoldierRules::GetAzirSandSoldiers(player);
    for (const auto& minion : soldiers) {
        if (!minion.IsValid() || minion.IsDead() || minion.Team() != player.Team()) {
            continue;
        }
        SoldierRecord* record = FindSoldierRecord(
            static_cast<int>(minion.NetworkId()), true);
        if (!record) continue;
        if (record->SpawnTick <= 0) {
            record->SpawnTick = now;
            record->ExpireTick = now + static_cast<int>(
                kWSoldierLifetimeSeconds * 1000.0f);
        }
        record->Position = minion.Position();
        record->LastSeenTick = now;
        record->NearEnemyTurret = NearEnemyTurret(record->Position, -110.0f);
        if (record->NearEnemyTurret) {
            record->ExpireTick = std::min(
                record->ExpireTick,
                record->SpawnTick + static_cast<int>(
                    kWSoldierLifetimeSeconds *
                    kWTurretLifetimeMultiplier * 1000.0f));
        }
    }
    for (auto& record : SoldierRecords) {
        if (record.NetworkId == 0) continue;
        if (record.ExpireTick <= now ||
            (record.LastSeenTick > 0 && now - record.LastSeenTick > 650)) {
            record = {};
        }
    }
}

inline std::vector<Soldier> BuildSoldiers(bool commandableOnly = true) {
    const auto player = ObjectManager::Player();
    std::vector<Soldier> result;
    if (!player.IsValid()) return result;
    const int now = Now();
    for (const auto& record : SoldierRecords) {
        if (record.NetworkId == 0 || record.ExpireTick <= now ||
            !record.Position.IsValid() || record.Position.IsZero()) continue;
        Soldier soldier{};
        soldier.Position = record.Position;
        soldier.Id = record.NetworkId;
        soldier.RemainingSeconds = static_cast<float>(
            record.ExpireTick - now) / 1000.0f;
        soldier.ConfirmedOwned = record.ConfirmedOwned;
        soldier.Valid = true;
        if (!commandableOnly || Commandable(player.Position(), soldier)) {
            result.push_back(soldier);
        }
    }
    return result;
}

inline GameObject LiveSoldier(int networkId) {
    if (networkId == 0) return {};
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return {};
    const auto& soldiers = SoldierRules::GetAzirSandSoldiers(player);
    for (const auto& obj : soldiers) {
        if (obj.IsValid() && !obj.IsDead() &&
            static_cast<int>(obj.NetworkId()) == networkId) {
            return obj;
        }
    }
    return {};
}

inline int WCharges() {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return 0;
    const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::W);
    if (!spell.IsValid() || spell.Level() <= 0) return 0;
    const int maximum = spell.MaxAmmo();
    const int ammo = spell.Ammo();
    if (maximum > 0 && ammo >= 0) return std::clamp(ammo, 0, maximum);
    return Ready(1) ? 1 : 0;
}

inline bool WAvailable() {
    return Ready(1) && WCharges() > 0;
}

inline float TargetPriority(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const float offense = std::max(
        target.TotalAttackDamage() * 0.75f,
        target.AP() * 0.55f);
    const float range = std::max(0.0f, target.AttackRange() - 175.0f);
    const float vulnerability = 100.0f - target.HealthPercent();
    return 1.0f + std::min(1.2f,
        offense / 700.0f + range / 1800.0f + vulnerability / 300.0f);
}

inline Unit HeroUnit(const AIHeroClient& hero, float delaySeconds) {
    Unit unit{};
    if (!Engine::ValidEnemy(hero)) return unit;
    unit.Position = hero.Position();
    unit.PredictedPosition = PredictPosition(hero, delaySeconds);
    unit.Radius = hero.BoundingRadius();
    unit.Id = static_cast<int>(hero.NetworkId());
    unit.Health = hero.Health();
    unit.Priority = TargetPriority(hero);
    unit.Champion = true;
    unit.HardCrowdControlled = Engine::IsHardCrowdControlled(hero);
    unit.Valid = true;
    return unit;
}

inline Unit MinionUnit(const AIMinionClient& minion,
                       float delaySeconds,
                       bool jungle) {
    Unit unit{};
    if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable()) {
        return unit;
    }
    unit.Position = minion.Position();
    unit.PredictedPosition = PredictPosition(minion, delaySeconds);
    unit.Radius = minion.BoundingRadius();
    unit.Id = static_cast<int>(minion.NetworkId());
    unit.Health = minion.Health();
    unit.Priority = jungle
        ? (ControllerHelpers::IsEpicMonster(minion) ? 2.4f : 1.0f)
        : 0.45f;
    unit.Minion = !jungle;
    unit.Jungle = jungle;
    unit.Valid = true;
    return unit;
}

inline std::vector<Unit> BuildUnits(float delaySeconds,
                                    bool includeFarm) {
    std::vector<Unit> result;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        Unit unit = HeroUnit(enemy, delaySeconds);
        if (unit.Valid) result.push_back(unit);
    }
    if (includeFarm) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            Unit unit = MinionUnit(minion, delaySeconds, false);
            if (unit.Valid) result.push_back(unit);
        }
        for (const auto& monster : GameObjects::Jungle()) {
            Unit unit = MinionUnit(monster, delaySeconds, true);
            if (unit.Valid) result.push_back(unit);
        }
    }
    return result;
}

inline std::vector<CollisionBody> BuildCollisionBodies(
    float delaySeconds) {
    std::vector<CollisionBody> result;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        result.push_back({
            PredictPosition(enemy, delaySeconds), enemy.BoundingRadius(),
            static_cast<int>(enemy.NetworkId()), true,
            TargetPriority(enemy) >= 1.55f, true,
        });
    }
    return result;
}

inline Vector3 RetreatDirection(const AIHeroClient& target) {
    if (!target.IsValid()) return {};
    if (target.PathEnd().IsValid() && !target.PathEnd().IsZero() &&
        target.Position().Distance2D(target.PathEnd()) > 35.0f) {
        return SharedGeometry::Direction2D(
            target.Position(), target.PathEnd());
    }
    const auto player = ObjectManager::Player();
    return player.IsValid()
        ? SharedGeometry::Direction2D(player.Position(), target.Position())
        : Vector3{};
}

inline Vector3 AlliedCentroid(const Vector3& around,
                              float range = 1200.0f) {
    Vector3 sum{};
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally) ||
            ally.Position().DistanceSqr2D(around) > rangeSqr) continue;
        sum = sum + ally.Position();
        ++count;
    }
    return count > 0 ? sum / static_cast<float>(count) : Vector3{};
}

inline bool HasReadyEscape(const AIHeroClient& target) {
    if (!target.IsValid()) return false;
    if (target.IsDashing()) return true;
    for (const auto& rule : EscapeRules) {
        if (ControllerHelpers::ChampionIs(target, rule.Champion) &&
            EnemySpellReady(target, rule.Slot)) return true;
    }
    return false;
}

inline bool EnemyCommitted(int networkId) {
    const EnemyWindow* window =
        ControllerHelpers::EnemyCastWindowById(EnemyWindows, networkId);
    return window && window->CommittedUntil >= Now();
}

inline bool EnemyCrowdControlSpent(int networkId) {
    const EnemyWindow* window =
        ControllerHelpers::EnemyCastWindowById(EnemyWindows, networkId);
    return window && window->HardCrowdControlSpentUntil >= Now();
}

inline bool EndpointSafe(const Vector3& position,
                         bool allowCurrentTurret = false,
                         int maximumEnemies = 2) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !position.IsValid() || position.IsZero() ||
        SDK::NavMesh::IsWall(position)) return false;
    if (Engine::UnderEnemyTurret(position) &&
        !(allowCurrentTurret &&
          Engine::UnderEnemyTurret(player.Position()))) return false;
    if (HasReadyDashHazardAt(position) ||
        HasReadyPointClickThreatAt(position)) return false;
    const int enemies = Engine::CountEnemiesAt(position, 650.0f);
    const int allies = Engine::CountAlliesAt(position, 750.0f);
    return enemies <= std::max(maximumEnemies, allies + 1);
}

inline int CurrentSoldierAttackers(const AIHeroClient& target) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target)) return 0;
    return SoldierAttackCount(
        player.Position(), BuildSoldiers(), HeroUnit(target, 0.0f));
}

inline bool HasEscapeAnchor(const AIHeroClient& threat = {}) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return false;
    const Vector3 away = threat.IsValid()
        ? SharedGeometry::Direction2D(threat.Position(), player.Position())
        : SharedGeometry::Direction2D(player.Position(), Game::CursorPos());
    for (const auto& soldier : BuildSoldiers()) {
        const Vector3 route = SharedGeometry::Direction2D(
            player.Position(), soldier.Position);
        if (route.IsZero() || away.IsZero() || route.Dot(away) >= 0.15f) {
            return true;
        }
    }
    return false;
}

inline Posture DeterminePosture(Mode mode,
                                const AIHeroClient& target) {
    if (mode == Mode::Flee) return Posture::Flee;
    if (mode == Mode::LaneClear || mode == Mode::Jungle ||
        mode == Mode::LastHit) return Posture::Farm;
    if (ActiveSequence.Kind == Sequence::ShurimaShuffle ||
        ActiveSequence.Kind == Sequence::RevenantShuffle) {
        return Posture::Shuffle;
    }
    if (GapcloserExpireTick >= Now() || IncomingThreatUntil >= Now()) {
        return Posture::Peel;
    }
    if (mode == Mode::Harass) return Posture::LaneControl;
    if (mode == Mode::Combo && Engine::ValidEnemy(target)) {
        const int attackers = CurrentSoldierAttackers(target);
        if (attackers >= 2) return Posture::ExtendedDps;
        if (attackers >= 1) return Posture::FrontToBack;
        if (target.HealthPercent() <= 32.0f) return Posture::Pick;
    }
    return Posture::Neutral;
}

inline WPlan BuildWPlan(const AIHeroClient& target,
                        WPurpose purpose,
                        bool farm = false,
                        bool jungle = false) {
    WPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !WAvailable()) return best;
    std::vector<Vector3> candidates;
    const bool defensive = purpose == WPurpose::DefensiveAnchor ||
        purpose == WPurpose::FleeAnchor;
    if (defensive) {
        candidates.push_back(ClampCast(
            player.Position(), Game::CursorPos(), kWSpawnRange));
        if (target.IsValid()) {
            const Vector3 away = SharedGeometry::Direction2D(
                target.Position(), player.Position());
            if (!away.IsZero()) {
                candidates.push_back(
                    player.Position() + away * (kWSpawnRange - 15.0f));
            }
        }
    } else if (Engine::ValidEnemy(target)) {
        const Vector3 currentPos = target.Position();
        const Vector3 predicted = PredictPosition(target, kWCastSeconds);
        const Vector3 midPos = (currentPos + predicted) * 0.5f;
        const Vector3 retreat = RetreatDirection(target);

        // 1. Direct on current target position
        candidates.push_back(ClampCast(
            player.Position(), currentPos, kWSpawnRange));
        // 2. Midpoint between current and predicted target position (optimal overlap)
        candidates.push_back(ClampCast(
            player.Position(), midPos, kWSpawnRange));
        // 3. Predicted target position
        candidates.push_back(ClampCast(
            player.Position(), predicted, kWSpawnRange));
        // 4. Short lead on retreat direction (50u instead of 110u)
        if (!retreat.IsZero()) {
            candidates.push_back(ClampCast(
                player.Position(), currentPos + retreat * 50.0f, kWSpawnRange));
        }
    }
    if (farm) {
        const auto& units = jungle
            ? GameObjects::Jungle() : GameObjects::EnemyMinions();
        for (const auto& unit : units) {
            if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable()) {
                continue;
            }
            candidates.push_back(ClampCast(
                player.Position(), unit.Position(), kWSpawnRange));
        }
    }
    if (candidates.empty()) return best;

    const bool existingAnchor = HasEscapeAnchor(target);
    for (const Vector3& candidate : candidates) {
        if (!candidate.IsValid() || candidate.IsZero() ||
            SDK::NavMesh::IsWall(candidate) ||
            player.Position().Distance2D(candidate) > kWSpawnRange + 5.0f) {
            continue;
        }
        int farmHits = 0;
        if (farm) {
            const auto& units = jungle
                ? GameObjects::Jungle() : GameObjects::EnemyMinions();
            for (const auto& unit : units) {
                if (unit.IsValid() && !unit.IsDead() && unit.IsTargetable() &&
                    candidate.Distance2D(unit.Position()) <=
                        SoldierRules::kPrimaryAttackRange +
                            unit.BoundingRadius()) {
                    ++farmHits;
                }
            }
        }
        bool coverage = false;
        bool doubleCoverage = false;
        if (Engine::ValidEnemy(target)) {
            const float currentDist = candidate.Distance2D(target.Position());
            const float predDist = candidate.Distance2D(PredictPosition(target, kWCastSeconds));
            const float maxAttackDist = SoldierRules::kPrimaryAttackRange + target.BoundingRadius() - 15.0f; // 15u safety buffer
            const bool coversCurrent = currentDist <= maxAttackDist;
            const bool coversPredicted = predDist <= maxAttackDist;
            coverage = coversPredicted;
            doubleCoverage = coversCurrent && coversPredicted;
        }
        const bool nearTurret = NearEnemyTurret(candidate, -100.0f);
        WPlacementContext context{};
        context.CastPositionValid = true;
        context.Offensive = !defensive && !farm;
        context.Defensive = purpose == WPurpose::DefensiveAnchor;
        context.Flee = purpose == WPurpose::FleeAnchor;
        context.Farm = farm;
        context.CursorAgrees = CursorDirectionAgrees(candidate, -0.30f);
        context.Terrain = false;
        context.PlayerAttackWindingUp = Orbwalker::IsWindingUp();
        context.EReady = Ready(2);
        context.ExistingEscapeAnchor = existingAnchor;
        context.CreatesTargetCoverage = coverage;
        context.CreatesZone = coverage || purpose == WPurpose::ShuffleAnchor;
        context.TargetCommitted = target.IsValid() &&
            (EnemyCommitted(static_cast<int>(target.NetworkId())) ||
             Engine::IsHardCrowdControlled(target));
        context.TargetCanImmediatelyEscape =
            target.IsValid() && HasReadyEscape(target);
        context.Charges = WCharges();
        context.MinimumReserve = Ready(2) &&
            Bool(SoldierMenu, "ReserveForE", true) ? 1 : 0;
        context.FarmHits = farmHits;
        if (!ShouldPlaceW(context) &&
            purpose != WPurpose::ShuffleAnchor) continue;
        float score = static_cast<float>(farmHits) * 95.0f +
            (coverage ? 510.0f : 0.0f) +
            (doubleCoverage ? 250.0f : 0.0f) +
            (context.CursorAgrees ? 90.0f : -150.0f) +
            (existingAnchor ? 75.0f : 0.0f) -
            (nearTurret ? 100.0f : 0.0f) -
            candidate.Distance2D(Game::CursorPos()) *
                (defensive ? 0.12f : 0.025f);
        if (purpose == WPurpose::ShuffleAnchor) {
            score += coverage ? 260.0f : 80.0f;
            if (WCharges() <= 0 || Orbwalker::IsWindingUp()) continue;
        }
        if (score > best.Score) {
            best.CastPosition = candidate;
            best.TargetId = target.IsValid()
                ? static_cast<int>(target.NetworkId()) : 0;
            best.Purpose = purpose;
            best.FarmHits = farmHits;
            best.Score = score;
            best.CreatesCoverage = coverage;
            best.ExistingEscapeAnchor = existingAnchor;
            best.NearTurret = nearTurret;
            best.Valid = true;
        }
    }
    return best;
}

inline QPlan BuildQPlan(const AIHeroClient& target,
                        QPurpose purpose,
                        bool includeFarm = false,
                        bool jungle = false) {
    QPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Ready(0)) return best;
    const auto soldiers = BuildSoldiers();
    if (soldiers.empty()) return best;
    const auto units = BuildUnits(kQCastSeconds + 0.12f, includeFarm);
    std::vector<Vector3> candidates;
    Vector3 retreat{};
    int primaryId = 0;
    if (Engine::ValidEnemy(target)) {
        primaryId = static_cast<int>(target.NetworkId());
        const Vector3 predicted = PredictPosition(
            target, kQCastSeconds + target.Position().Distance2D(
                player.Position()) / kQMoveSpeed);
        retreat = RetreatDirection(target);
        candidates.push_back(predicted + retreat * 95.0f);
        candidates.push_back(predicted + retreat * 155.0f);
        candidates.push_back(predicted);
        if (target.PathEnd().IsValid() && !target.PathEnd().IsZero()) {
            candidates.push_back(target.PathEnd() + retreat * 55.0f);
        }
    }
    if (purpose == QPurpose::Drift ||
        purpose == QPurpose::Shuffle ||
        purpose == QPurpose::FleeRedirect ||
        purpose == QPurpose::Revenant) {
        Vector3 buffered = PendingQPosition;
        if (!buffered.IsValid() || buffered.IsZero()) {
            buffered = purpose == QPurpose::Revenant &&
                    SequenceOrigin.IsValid() && !SequenceOrigin.IsZero()
                ? SequenceOrigin : Game::CursorPos();
        }
        candidates.insert(candidates.begin(), buffered);
    }
    if (includeFarm) {
        const auto& farmUnits = jungle
            ? GameObjects::Jungle() : GameObjects::EnemyMinions();
        for (const auto& unit : farmUnits) {
            if (unit.IsValid() && !unit.IsDead() && unit.IsTargetable()) {
                candidates.push_back(unit.Position());
            }
        }
    }
    for (const Vector3& candidate : candidates) {
        QEvaluation evaluation = EvaluateQ(
            player.Position(), candidate, soldiers, units,
            primaryId, retreat);
        if (!evaluation.Valid) continue;
        bool escapeLost = false;
        if (Ready(2) && soldiers.size() == 1 &&
            Bool(SoldierMenu, "PreserveEscapeAnchor", true)) {
            escapeLost = evaluation.Endpoints.empty() ||
                !SoldierRules::IsCommandable(
                    Point2(player.Position()),
                    Point2(evaluation.Endpoints.front()));
        }
        float score = evaluation.Score +
            (CursorDirectionAgrees(evaluation.CastPosition, -0.28f)
                ? 100.0f : -260.0f) -
            (escapeLost ? 850.0f : 0.0f);
        if (includeFarm) {
            score += static_cast<float>(evaluation.FarmHits) * 80.0f;
            if (evaluation.ChampionHits > 0) score += 260.0f;
        }
        if (purpose == QPurpose::Drift ||
            purpose == QPurpose::Shuffle ||
            purpose == QPurpose::Revenant ||
            purpose == QPurpose::FleeRedirect) {
            score += 1000.0f;
            escapeLost = false;
        }
        if (score > best.Score) {
            best.Evaluation = std::move(evaluation);
            best.TargetId = primaryId;
            best.Purpose = purpose;
            best.RawDamage = QRawDamage(
                SpellRank(0), player.AP());
            best.Score = score;
            best.EscapeAnchorLost = escapeLost;
            best.Valid = true;
        }
    }
    return best;
}

inline EPlan BuildEPlan(const AIHeroClient& target,
                        EPurpose purpose,
                        const Vector3& redirect = {}) {
    EPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Ready(2) || PlayerMobilityLocked()) return best;
    const auto soldiers = BuildSoldiers();
    const auto collisions = BuildCollisionBodies(0.08f);
    const bool defensive = purpose == EPurpose::DefensiveShield ||
        purpose == EPurpose::AntiGapcloser || purpose == EPurpose::Flee;
    const bool shuffle = purpose == EPurpose::Shuffle ||
        purpose == EPurpose::Revenant;
    const int targetId = target.IsValid()
        ? static_cast<int>(target.NetworkId()) : 0;
    for (const auto& soldier : soldiers) {
        if (player.Position().Distance2D(soldier.Position) >
            kESelectionRange + 5.0f) continue;
        const DashResult direct = ResolveDashSegment(
            player.Position(), soldier.Position, collisions);
        Vector3 qAim = redirect;
        if ((shuffle || purpose == EPurpose::DriftEngage) &&
            (!qAim.IsValid() || qAim.IsZero()) && target.IsValid()) {
            const Vector3 forward = SharedGeometry::Direction2D(
                player.Position(), PredictPosition(target, 0.30f));
            qAim = PredictPosition(target, 0.30f) + forward * 320.0f;
        }
        if (purpose == EPurpose::Flee &&
            (!qAim.IsValid() || qAim.IsZero())) qAim = Game::CursorPos();
        const DriftResult drift = Ready(0) && qAim.IsValid() && !qAim.IsZero()
            ? ResolveDrift(player.Position(), soldier, qAim, collisions)
            : DriftResult{};
        const Vector3 endpoint = drift.Valid && drift.QBuffered
            ? drift.FinalEndpoint : direct.ActualEndpoint;
        if (!endpoint.IsValid() || endpoint.IsZero()) continue;
        const bool blockedByProjectileWall = ProjectileWallBlocks(
            player.Position(), endpoint, kEDashHalfWidth * 0.5f);
        const int allies = Engine::CountAlliesAt(endpoint, 750.0f);
        const int enemies = Engine::CountEnemiesAt(endpoint, 650.0f);
        const bool desiredCollision = purpose == EPurpose::CollisionKill ||
            purpose == EPurpose::CollisionRefund;
        const bool collisionConfirmed = direct.CollisionId == targetId ||
            (drift.Valid && drift.CollisionId == targetId);
        const bool lethal = target.IsValid() &&
            target.Health() <= player.CalculateMagicDamage(
                target, ERawDamage(SpellRank(2), player.AP())) + 5.0f;
        ECommitContext context{};
        context.EReady = true;
        context.AnchorValid = true;
        context.EndpointNavigable = EndpointSafe(
            endpoint, defensive, defensive ? 3 : 2);
        context.PathCrossesForbiddenTerrain = blockedByProjectileWall;
        context.EndpointEnemyTurret = Engine::UnderEnemyTurret(endpoint) &&
            !Engine::UnderEnemyTurret(player.Position());
        context.EndpointDashHazard = HasReadyDashHazardAt(endpoint);
        context.EndpointPointClickThreat = HasReadyPointClickThreatAt(endpoint);
        context.PlayerMobilityLocked = false;
        context.PlayerAttackWindingUp = Orbwalker::IsWindingUp();
        context.Defensive = defensive;
        context.Flee = purpose == EPurpose::Flee;
        context.Shuffle = shuffle;
        context.Killable = lethal;
        context.TargetCollisionDesired = desiredCollision;
        context.TargetCollisionConfirmed = collisionConfirmed;
        context.QReadyForRedirect = Ready(0) && drift.Valid && drift.QBuffered;
        context.CursorAgrees = CursorDirectionAgrees(endpoint, -0.24f);
        context.HasRExit = Ready(3);
        context.HasAlliedFollowup = CountAlliedFollowup(
            endpoint, 950.0f) > 0;
        context.EnemiesAtEndpoint = enemies;
        context.AlliesAtEndpoint = allies;
        if (!ShouldCommitE(context)) continue;
        float score = endpoint.Distance2D(Game::CursorPos()) * -0.08f +
            static_cast<float>(allies) * 170.0f -
            static_cast<float>(enemies) * 230.0f +
            (collisionConfirmed ? 560.0f : 0.0f) +
            (drift.Valid && drift.QBuffered ? 280.0f : 0.0f) +
            (defensive ? endpoint.Distance2D(
                target.IsValid() ? target.Position() : player.Position()) *
                    0.30f : 0.0f);
        if (score > best.Score) {
            best.AnchorId = soldier.Id;
            best.AnchorPosition = soldier.Position;
            best.RedirectPosition = qAim;
            best.FinalEndpoint = endpoint;
            best.Drift = drift;
            best.Direct = direct;
            best.TargetId = targetId;
            best.Purpose = purpose;
            best.Score = score;
            best.NeedsQ = drift.Valid && drift.QBuffered;
            best.Defensive = defensive;
            best.Valid = true;
        }
    }
    return best;
}

inline bool LandingSafeForR(const AIHeroClient& target,
                            const Vector3& direction) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) return false;
    Unit unit = HeroUnit(target, kRCastSeconds);
    const Vector3 landing = RLandingPosition(
        player.Position(), direction, unit);
    return landing.IsValid() && !landing.IsZero() &&
           !SDK::NavMesh::IsWall(landing) &&
           (!Engine::UnderEnemyTurret(landing) ||
            Engine::UnderEnemyTurret(player.Position()));
}

inline RPlan BuildRPlan(const AIHeroClient& target,
                        RPurpose purpose,
                        bool playerAuthorized = false) {
    RPlan best{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Ready(3) || !Engine::ValidEnemy(target)) {
        return best;
    }
    const int rank = SpellRank(3);
    if (rank <= 0) return best;
    const auto units = BuildUnits(kRCastSeconds, false);
    std::vector<Vector3> candidates;
    const bool defensive = purpose == RPurpose::Peel ||
        purpose == RPurpose::Disengage;
    if (defensive) {
        candidates.push_back(target.Position());
        candidates.push_back(PredictPosition(target, kRCastSeconds));
    } else if (purpose == RPurpose::Shuffle ||
               purpose == RPurpose::Revenant ||
               purpose == RPurpose::Pick) {
        const Vector3 allies = AlliedCentroid(target.Position(), 1500.0f);
        const Vector3 towardAllies = allies.IsValid() && !allies.IsZero()
            ? SharedGeometry::Direction2D(target.Position(), allies)
            : SharedGeometry::Direction2D(target.Position(), player.Position());
        if (!towardAllies.IsZero()) {
            candidates.push_back(
                player.Position() + towardAllies * kRCastDistance);
        }
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 950.0f)) continue;
        const Vector3 base = SharedGeometry::Direction2D(
            player.Position(), PredictPosition(enemy, kRCastSeconds));
        if (base.IsZero()) continue;
        candidates.push_back(player.Position() + base * kRCastDistance);
        candidates.push_back(player.Position() +
            SharedGeometry::Rotate2D(base, 0.18f) * kRCastDistance);
        candidates.push_back(player.Position() +
            SharedGeometry::Rotate2D(base, -0.18f) * kRCastDistance);
    }
    const float raw = RRawDamage(rank, player.AP());
    const bool lethal = target.Health() <=
        player.CalculateMagicDamage(target, raw) + 5.0f;
    const bool frontToBack = CurrentSoldierAttackers(target) > 0 &&
        Engine::CountEnemiesAt(player.Position(), 500.0f) <= 1;
    const bool targetSpellShield = HasSpellShieldOrImmunity(target);
    const bool targetFlashReady = EnemyFlashReady(target);
    const bool targetDashReady = HasReadyEscape(target);
    const bool keyCCSpent = EnemyCrowdControlSpent(
        static_cast<int>(target.NetworkId())) ||
        Engine::IsHardCrowdControlled(target);
    const bool playerLow = player.HealthPercent() <=
        Slider(TacticsMenu, "DefensiveHealth", 42);
    const bool threatCommitted = EnemyCommitted(
        static_cast<int>(target.NetworkId())) ||
        GapcloserTargetId == static_cast<int>(target.NetworkId());
    const Vector3 alliedCentroid = AlliedCentroid(player.Position(), 1100.0f);

    for (const Vector3& candidate : candidates) {
        const Vector3 direction = SharedGeometry::Direction2D(
            player.Position(), candidate);
        if (direction.IsZero()) continue;
        Unit primary = HeroUnit(target, kRCastSeconds);
        const Vector3 landing = RLandingPosition(
            player.Position(), direction, primary);
        const int followup = CountAlliedFollowup(landing, 950.0f);
        const bool landingSafe = LandingSafeForR(target, direction);
        REvaluation evaluation = EvaluateR(
            player.Position(), candidate, rank, units,
            static_cast<int>(target.NetworkId()), alliedCentroid,
            followup, landingSafe);
        if (!evaluation.Valid) continue;
        const bool hasExit = WCharges() > 0 ||
            evaluation.AlliedFollowup >= 2 ||
            (player.HealthPercent() >= 72.0f &&
             Engine::CountEnemiesAt(player.Position(), 700.0f) <=
                 evaluation.AlliedFollowup + 1);
        RCastContext context{};
        context.Evaluation = evaluation;
        context.Purpose = purpose;
        context.RReady = true;
        context.PlayerAttackWindingUp = Orbwalker::IsWindingUp();
        context.TargetSpellShield = targetSpellShield;
        context.TargetFlashReady = targetFlashReady;
        context.TargetDashReady = targetDashReady;
        context.KeyCrowdControlSpent = keyCCSpent;
        context.PlayerLow = playerLow;
        context.ThreatCommitted = threatCommitted;
        context.EndpointEnemyTurret = Engine::UnderEnemyTurret(
            player.Position()) && !defensive;
        context.EndpointTerrain = SDK::NavMesh::IsWall(landing);
        context.EndpointPointClickThreat =
            HasReadyPointClickThreatAt(player.Position()) && !defensive;
        context.CursorAgrees = defensive ||
            CursorDirectionAgrees(candidate, -0.36f);
        context.HasStasisOrExit = hasExit;
        context.FrontToBackDpsAvailable =
            frontToBack && !playerAuthorized;
        context.Lethal = lethal;
        context.MinimumHits = purpose == RPurpose::KillSecure ? 1 :
            Slider(RMenu, "MinimumHits", 2);
        if (!ShouldCastR(context)) continue;
        float score = evaluation.Score +
            (defensive ? 420.0f : 0.0f) +
            (lethal ? 500.0f : 0.0f) -
            (frontToBack && !playerAuthorized && evaluation.Hits < 3
                ? 460.0f : 0.0f);
        if (score > best.Score) {
            best.Evaluation = std::move(evaluation);
            best.TargetId = static_cast<int>(target.NetworkId());
            best.Purpose = purpose;
            best.RawDamage = raw;
            best.Score = score;
            best.Defensive = defensive;
            best.Valid = true;
        }
    }
    return best;
}

inline bool CastWPlan(const WPlan& plan,
                      bool reactive = false) {
    if (!plan.Valid || !WAvailable() ||
        !CastThrottleReady(1, reactive)) return false;
    if (Engine::ControllerCastPosition(1, plan.CastPosition)) {
        LastWPlan = plan;
        LastWPurpose = plan.Purpose;
        LastWCastTick = Now();
        PendingWPosition = plan.CastPosition;
        PendingAnchorUntil = Now() + 900;
        return true;
    }
    return false;
}

inline bool CastQPlan(const QPlan& plan,
                      bool reactive = false) {
    if (!plan.Valid || !Ready(0) || plan.EscapeAnchorLost ||
        !CastThrottleReady(0, reactive)) return false;
    if (Engine::ControllerCastPosition(
            0, plan.Evaluation.CastPosition)) {
        LastQPlan = plan;
        LastQPurpose = plan.Purpose;
        LastQCastTick = Now();
        return true;
    }
    return false;
}

inline bool CastEPlan(const EPlan& plan,
                      bool reactive = false) {
    if (!plan.Valid || !Ready(2) ||
        !CastThrottleReady(2, reactive)) return false;
    const GameObject anchor = LiveSoldier(plan.AnchorId);
    if (!anchor.IsValid() || anchor.IsDead()) return false;
    if (Engine::ControllerCastUnit(2, AIBaseClient(anchor.Handle()))) {
        LastEPlan = plan;
        LastEPurpose = plan.Purpose;
        LastECastTick = Now();
        const float distance = ObjectManager::Player().Position().Distance2D(
            plan.FinalEndpoint);
        EDashUntil = Now() + std::clamp(
            static_cast<int>(distance / kEDashSpeed * 1000.0f) + 320,
            280, 1500);
        if (plan.NeedsQ) PendingQPosition = plan.RedirectPosition;
        return true;
    }
    return false;
}

inline bool CastRPlan(const RPlan& plan,
                      bool reactive = false) {
    if (!plan.Valid || !Ready(3) ||
        !CastThrottleReady(3, reactive)) return false;
    if (Engine::ControllerCastPosition(
            3, plan.Evaluation.CastPosition)) {
        LastRPlan = plan;
        LastRPurpose = plan.Purpose;
        LastRCastTick = Now();
        return true;
    }
    return false;
}

inline void StartSequence(Sequence kind,
                          SequencePhase phase,
                          int targetId,
                          int anchorId,
                          int lifetimeMs,
                          bool controllerOwned = true) {
    const int now = Now();
    ActiveSequence = {};
    ActiveSequence.Kind = kind;
    ActiveSequence.Phase = phase;
    ActiveSequence.TargetId = targetId;
    ActiveSequence.AnchorId = anchorId;
    ActiveSequence.StartedTick = now;
    ActiveSequence.LastTransitionTick = now;
    ActiveSequence.ExpireTick = now + std::max(200, lifetimeMs);
    ActiveSequence.ControllerOwned = controllerOwned;
}

inline void TransitionSequence(SequencePhase phase,
                               int lifetimeMs = -1) {
    ActiveSequence.Phase = phase;
    ActiveSequence.LastTransitionTick = Now();
    if (lifetimeMs > 0) ActiveSequence.ExpireTick = Now() + lifetimeMs;
}

inline void ClearSequence() {
    ActiveSequence = {};
    PendingAnchorId = 0;
    PendingAnchorUntil = 0;
    PendingWPosition = {};
    PendingQPosition = {};
    SequenceOrigin = {};
    SequenceManualIntent = false;
}

inline const Soldier* SoldierById(const std::vector<Soldier>& soldiers,
                                  int networkId) {
    for (const auto& soldier : soldiers) {
        if (soldier.Valid && soldier.Id == networkId) return &soldier;
    }
    return nullptr;
}

inline int PendingSpawnedSoldier() {
    const int now = Now();
    int bestId = 0;
    float bestDistance = FLT_MAX;
    for (const auto& record : SoldierRecords) {
        if (record.NetworkId == 0 || record.ExpireTick <= now ||
            record.SpawnTick + 140 < LastWCastTick ||
            !record.Position.IsValid() || record.Position.IsZero()) {
            continue;
        }
        const float distance = PendingWPosition.IsValid() &&
                !PendingWPosition.IsZero()
            ? record.Position.Distance2D(PendingWPosition)
            : 0.0f;
        if (distance <= 190.0f && distance < bestDistance) {
            bestDistance = distance;
            bestId = record.NetworkId;
        }
    }
    return bestId;
}

inline bool RecentAttackOn(int targetId, int windowMs = 520) {
    if (targetId == 0) return false;
    const int now = Now();
    return (LastAfterAttackTargetId == targetId &&
            now - LastAfterAttackTick <= windowMs) ||
           (LastSoldierAttackTargetId == targetId &&
            now - LastSoldierAttackTick <= windowMs);
}

inline bool TargetLeavingSoldierCoverage(
    const AIHeroClient& target,
    int currentAttackers) {
    if (!Engine::ValidEnemy(target) || currentAttackers <= 0) return false;
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return false;
    Unit future = HeroUnit(target, 0.42f);
    if (!future.Valid) return false;
    const int futureAttackers = SoldierAttackCount(
        player.Position(), BuildSoldiers(), future);
    if (futureAttackers < currentAttackers) return true;
    if (!target.PathEnd().IsValid() || target.PathEnd().IsZero() ||
        target.Position().Distance2D(target.PathEnd()) <= 120.0f) {
        return false;
    }
    const Vector3 retreat = RetreatDirection(target);
    return !retreat.IsZero() &&
        SharedGeometry::Direction2D(
            player.Position(), target.Position()).Dot(retreat) > 0.45f;
}

inline LateQContext BuildLateQContext(const AIHeroClient& target,
                                      const QPlan& plan) {
    LateQContext context{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) || !plan.Valid) {
        return context;
    }
    const int id = static_cast<int>(target.NetworkId());
    const int current = CurrentSoldierAttackers(target);
    const float reserve = Ready(2) &&
            Bool(SoldierMenu, "PreserveEscapeAnchor", true)
        ? SpellCost(2) : 0.0f;
    context.QReady = Ready(0);
    context.TargetValid = true;
    context.TargetLeavingCoverage =
        TargetLeavingSoldierCoverage(target, current);
    context.CurrentCoverage = current > 0;
    context.FutureCoverage = plan.Evaluation.FuturePrimaryAttackers > 0;
    context.QHitsTarget = plan.Evaluation.PrimaryHit;
    context.QLethal = !HasSpellShieldOrImmunity(target) &&
        player.CalculateMagicDamage(target, plan.RawDamage) >=
            target.Health() + target.AllShield();
    context.TargetHardCrowdControlled =
        Engine::IsHardCrowdControlled(target);
    context.PlayerAttackWindingUp = Orbwalker::IsWindingUp();
    context.AttackJustCompleted = RecentAttackOn(id, 430);
    context.EnoughMana = HasCurrentResource(SpellCost(0) + reserve);
    context.DirectionAgrees = CursorDirectionAgrees(
        plan.Evaluation.CastPosition, -0.32f);
    context.EscapeAnchorWouldBeLost = plan.EscapeAnchorLost;
    context.CurrentSoldierAttackers = current;
    context.FutureSoldierAttackers =
        plan.Evaluation.FuturePrimaryAttackers;
    return context;
}

inline bool TryLateQ(const AIHeroClient& target,
                     QPurpose purpose,
                     bool fastFollowup = false) {
    if (!Engine::ValidEnemy(target) || !Ready(0) ||
        !SpellEnabled(0, LastKnownMode)) return false;
    const QPlan plan = BuildQPlan(target, purpose);
    if (!plan.Valid || !ShouldCastLateQ(
            BuildLateQContext(target, plan))) return false;
    return CastQPlan(plan, fastFollowup);
}

inline Vector3 ShuffleRedirect(const AIHeroClient& target,
                               bool revenant) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return {};
    if (revenant && SequenceOrigin.IsValid() &&
        !SequenceOrigin.IsZero()) return SequenceOrigin;
    if (!Engine::ValidEnemy(target)) return Game::CursorPos();
    const Vector3 predicted = PredictPosition(target, 0.34f);
    Vector3 direction = SharedGeometry::Direction2D(
        player.Position(), predicted);
    if (direction.IsZero()) direction = SharedGeometry::Direction2D(
        player.Position(), Game::CursorPos());
    return direction.IsZero()
        ? predicted
        : predicted + direction * 330.0f;
}

inline bool TryAdvanceSequence(Mode mode) {
    if (ActiveSequence.Kind == Sequence::None) return false;
    const int now = Now();
    if (SequenceExpired(ActiveSequence, now)) {
        ClearSequence();
        return false;
    }
    if (ActiveSequence.Kind == Sequence::PlayerLed) return false;

    AIHeroClient target = HeroByNetworkId(ActiveSequence.TargetId);
    const bool targetRequired = ActiveSequence.Kind != Sequence::DriftEscape;
    if (targetRequired && !Engine::ValidEnemy(target) &&
        ActiveSequence.Phase != SequencePhase::RecoverDps) {
        ClearSequence();
        return false;
    }

    if (ActiveSequence.Phase == SequencePhase::AwaitSoldier) {
        if (!CanAdvanceSequence(ActiveSequence, now, 35)) return false;
        int anchorId = ActiveSequence.AnchorId;
        if (anchorId == 0 || !LiveSoldier(anchorId).IsValid()) {
            anchorId = PendingSpawnedSoldier();
        }
        if (anchorId == 0) return false;
        ActiveSequence.AnchorId = anchorId;
        PendingAnchorId = anchorId;
        if (ActiveSequence.Kind == Sequence::WAutoQAuto ||
            ActiveSequence.Kind == Sequence::ExtendedSoldierDps) {
            TransitionSequence(SequencePhase::AwaitFirstAttack, 1700);
            return false;
        }

        const bool revenant =
            ActiveSequence.Kind == Sequence::RevenantShuffle;
        EPurpose purpose = EPurpose::DriftEngage;
        if (ActiveSequence.Kind == Sequence::DriftEscape) {
            purpose = EPurpose::Flee;
        } else if (revenant) {
            purpose = EPurpose::Revenant;
        } else if (ActiveSequence.Kind == Sequence::ShurimaShuffle) {
            purpose = EPurpose::Shuffle;
        }
        const Vector3 redirect = ActiveSequence.Kind == Sequence::DriftEscape
            ? Game::CursorPos() : ShuffleRedirect(target, revenant);
        const EPlan e = BuildEPlan(target, purpose, redirect);
        if (CastEPlan(e, true)) {
            ActiveSequence.AnchorId = e.AnchorId;
            PendingQPosition = e.RedirectPosition;
            TransitionSequence(SequencePhase::AwaitQBuffer, 1050);
            return true;
        }
        return false;
    }

    if (ActiveSequence.Phase == SequencePhase::AwaitFirstAttack) {
        if (RecentAttackOn(ActiveSequence.TargetId, 600)) {
            TransitionSequence(SequencePhase::AwaitTargetExit, 1250);
        }
        return false;
    }

    if (ActiveSequence.Phase == SequencePhase::AwaitTargetExit) {
        if (TryLateQ(target, QPurpose::LateTrade)) {
            TransitionSequence(SequencePhase::RecoverDps, 950);
            return true;
        }
        if (!Ready(0) || now - ActiveSequence.LastTransitionTick > 1050) {
            TransitionSequence(SequencePhase::RecoverDps, 650);
        }
        return false;
    }

    if (ActiveSequence.Phase == SequencePhase::Dashing) {
        TransitionSequence(SequencePhase::AwaitQBuffer, 850);
    }

    if (ActiveSequence.Phase == SequencePhase::AwaitQBuffer) {
        if (!CanAdvanceSequence(ActiveSequence, now, 18)) return false;
        const bool revenant =
            ActiveSequence.Kind == Sequence::RevenantShuffle;
        QPurpose purpose = QPurpose::Drift;
        if (ActiveSequence.Kind == Sequence::DriftEscape) {
            purpose = QPurpose::FleeRedirect;
        } else if (revenant) {
            purpose = QPurpose::Revenant;
        } else if (ActiveSequence.Kind == Sequence::ShurimaShuffle) {
            purpose = QPurpose::Shuffle;
        }
        const QPlan q = BuildQPlan(target, purpose);
        if (CastQPlan(q, true)) {
            if (ActiveSequence.Kind == Sequence::ShurimaShuffle ||
                revenant) {
                TransitionSequence(SequencePhase::AwaitRWindow, 920);
            } else {
                TransitionSequence(SequencePhase::RecoverDps, 650);
            }
            return true;
        }
        if (now > EDashUntil + 120 || !Ready(0)) {
            ClearSequence();
        }
        return false;
    }

    if (ActiveSequence.Phase == SequencePhase::AwaitRWindow) {
        // Q has a 0.25 second cast lock.  Casting R earlier is the common
        // revenant/shuffle failure that leaves the wall behind Azir.
        if (now - LastQCastTick < 245 ||
            !CanAdvanceSequence(ActiveSequence, now, 235)) return false;
        const RPurpose purpose =
            ActiveSequence.Kind == Sequence::RevenantShuffle
                ? RPurpose::Revenant : RPurpose::Shuffle;
        const RPlan r = BuildRPlan(
            target, purpose, SequenceManualIntent);
        if (CastRPlan(r, true)) {
            TransitionSequence(SequencePhase::AwaitFollowupSoldier, 900);
            return true;
        }
        if (!Ready(3) || now - ActiveSequence.LastTransitionTick > 700) {
            ClearSequence();
        }
        return false;
    }

    if (ActiveSequence.Phase == SequencePhase::AwaitFollowupSoldier) {
        if (WAvailable() &&
            Bool(WMenu, "FollowupAfterR", true) &&
            CanAdvanceSequence(ActiveSequence, now, 85)) {
            const WPlan w = BuildWPlan(
                target, WPurpose::FollowupLanding);
            if (CastWPlan(w, true)) {
                TransitionSequence(SequencePhase::RecoverDps, 850);
                return true;
            }
        }
        if (now - ActiveSequence.LastTransitionTick > 520) {
            TransitionSequence(SequencePhase::RecoverDps, 650);
        }
        return false;
    }

    if (ActiveSequence.Phase == SequencePhase::RecoverDps) {
        if (now - ActiveSequence.LastTransitionTick >= 520 ||
            !Engine::ValidEnemy(target)) ClearSequence();
    }
    return false;
}

inline AIHeroClient CursorEnemy(float range = 1550.0f) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return {};
    AIHeroClient best{};
    float bestCursorDistance = FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range)) continue;
        const float distance = enemy.Position().Distance2D(Game::CursorPos());
        if (distance < bestCursorDistance) {
            bestCursorDistance = distance;
            best = enemy;
        }
    }
    return bestCursorDistance <= 650.0f ? best : AIHeroClient{};
}

inline bool TryStartShuffle(const AIHeroClient& target,
                            bool revenant,
                            bool manualIntent) {
    if (ActiveSequence.Kind != Sequence::None ||
        !Engine::ValidEnemy(target, 1500.0f)) return false;
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return false;
    const auto soldiers = BuildSoldiers();
    const int nearbyEnemies = Engine::CountEnemiesAt(
        target.Position(), 470.0f);
    const int alliedFollowup = CountAlliedFollowup(
        target.Position(), 1050.0f);
    const bool automatic = !revenant &&
        Bool(RMenu, "AutomaticShuffle", false);
    const bool frontToBack = CurrentSoldierAttackers(target) > 0 &&
        player.Position().Distance2D(target.Position()) >= 350.0f;
    const bool hasAnchor = !soldiers.empty();
    const int chargesNeeded = hasAnchor ? 1 : 2;
    const bool exitAvailable = WCharges() >= chargesNeeded ||
        alliedFollowup >= 2 ||
        (player.HealthPercent() >= 78.0f &&
         Engine::CountEnemiesAt(target.Position(), 720.0f) <=
             alliedFollowup + 1);
    ShuffleGate gate{};
    gate.ManualKey = manualIntent;
    gate.AutomaticEnabled = automatic;
    gate.TargetValid = true;
    gate.WReadyOrAnchor = hasAnchor || WAvailable();
    gate.EReady = Ready(2) && !PlayerMobilityLocked();
    gate.QReady = Ready(0);
    gate.RReady = Ready(3);
    gate.CursorAgrees = CursorDirectionAgrees(target.Position(), -0.28f);
    gate.AlliedFollowup = alliedFollowup > 0;
    gate.ExitAvailable = exitAvailable;
    gate.TargetFlashReady = EnemyFlashReady(target);
    gate.TargetDashReady = HasReadyEscape(target);
    gate.KeyCrowdControlSpent = EnemyCrowdControlSpent(
        static_cast<int>(target.NetworkId())) ||
        Engine::IsHardCrowdControlled(target);
    gate.FrontToBackDpsAvailable = frontToBack;
    gate.MultiTargetOpportunity = nearbyEnemies >=
        Slider(RMenu, "ShuffleMinimumEnemies", 3);
    gate.TurretRisk = Engine::UnderEnemyTurret(target.Position()) &&
        !Engine::UnderEnemyTurret(player.Position());
    gate.TerrainRisk = NearTerrain(target.Position(), 95.0f, 10);
    if (!MayStartShuffle(gate)) return false;

    SequenceOrigin = player.Position();
    const Sequence sequence = revenant
        ? Sequence::RevenantShuffle : Sequence::ShurimaShuffle;
    if (!hasAnchor) {
        const WPlan w = BuildWPlan(target, WPurpose::ShuffleAnchor);
        if (!CastWPlan(w)) return false;
        StartSequence(sequence, SequencePhase::AwaitSoldier,
                      static_cast<int>(target.NetworkId()), 0, 2850);
        SequenceOrigin = player.Position();
        SequenceManualIntent = manualIntent;
        return true;
    }

    const EPurpose ePurpose = revenant
        ? EPurpose::Revenant : EPurpose::Shuffle;
    const Vector3 redirect = ShuffleRedirect(target, revenant);
    const EPlan e = BuildEPlan(target, ePurpose, redirect);
    if (!CastEPlan(e)) return false;
    StartSequence(sequence, SequencePhase::AwaitQBuffer,
                  static_cast<int>(target.NetworkId()), e.AnchorId, 2350);
    SequenceOrigin = player.Position();
    PendingQPosition = e.RedirectPosition;
    SequenceManualIntent = manualIntent;
    return true;
}

inline bool TryManualShuffle() {
    const bool normal = Key(RMenu, "ManualShuffle", false);
    const bool revenant = Key(RMenu, "ManualRevenant", false);
    if (!normal && !revenant) return false;
    const AIHeroClient target = CursorEnemy();
    return Engine::ValidEnemy(target) &&
        TryStartShuffle(target, revenant, true);
}

inline bool TryAutomaticShuffle(const AIHeroClient& target) {
    if (!Bool(RMenu, "AutomaticShuffle", false) ||
        !Engine::ValidEnemy(target)) return false;
    return TryStartShuffle(target, false, false);
}

inline bool TryReactiveDefense(Mode mode) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return false;
    AIHeroClient threat{};
    if (InterruptExpireTick >= Now()) {
        threat = HeroByNetworkId(InterruptTargetId);
    }
    if (!Engine::ValidEnemy(threat) && GapcloserExpireTick >= Now()) {
        threat = HeroByNetworkId(GapcloserTargetId);
    }
    if (!Engine::ValidEnemy(threat) && IncomingThreatUntil >= Now()) {
        threat = HeroByNetworkId(IncomingThreatTargetId);
    }
    if (!Engine::ValidEnemy(threat)) return false;
    const int threatId = static_cast<int>(threat.NetworkId());
    const float distance = player.Position().Distance2D(threat.Position());
    const bool urgent = GapcloserExpireTick >= Now() ||
        IncomingHardCrowdControl || InterruptExpireTick >= Now();

    if (Ready(3) && CastThrottleReady(3, true) && Bool(RMenu, "ReactivePeel", true) &&
        distance <= kRForwardReach + threat.BoundingRadius() + 80.0f &&
        (urgent || player.HealthPercent() <=
            Slider(TacticsMenu, "DefensiveHealth", 42))) {
        const RPurpose purpose = InterruptExpireTick >= Now()
            ? RPurpose::Disengage : RPurpose::Peel;
        const RPlan r = BuildRPlan(threat, purpose, true);
        if (CastRPlan(r, true)) {
            StartSequence(Sequence::DefensivePeel,
                          SequencePhase::RecoverDps,
                          threatId, 0, 700);
            return true;
        }
    }

    if (Ready(2) && CastThrottleReady(2, true) && Bool(EMenu, "ReactiveEscape", true) &&
        !BuildSoldiers().empty()) {
        const EPlan e = BuildEPlan(
            threat,
            GapcloserExpireTick >= Now()
                ? EPurpose::AntiGapcloser
                : EPurpose::DefensiveShield,
            Game::CursorPos());
        if (CastEPlan(e, true)) {
            StartSequence(Sequence::DriftEscape,
                          e.NeedsQ ? SequencePhase::AwaitQBuffer
                                   : SequencePhase::RecoverDps,
                          threatId, e.AnchorId,
                          e.NeedsQ ? 1100 : 650);
            PendingQPosition = e.RedirectPosition;
            return true;
        }
    }

    if (WAvailable() && CastThrottleReady(1, true) && Bool(WMenu, "DefensiveAnchor", true) &&
        (urgent || mode == Mode::Flee)) {
        const WPlan w = BuildWPlan(threat, WPurpose::DefensiveAnchor);
        if (CastWPlan(w, true)) {
            StartSequence(Sequence::DriftEscape,
                          SequencePhase::AwaitSoldier,
                          threatId, 0, 1650);
            return true;
        }
    }
    return false;
}

inline bool TryKillSecure(const AIHeroClient& preferred) {
    if (!Bool(TacticsMenu, "KillSecure", true)) return false;
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return false;
    std::vector<AIHeroClient> candidates;
    if (Engine::ValidEnemy(preferred, 1500.0f)) {
        candidates.push_back(preferred);
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1500.0f)) continue;
        if (!candidates.empty() &&
            enemy.NetworkId() == candidates.front().NetworkId()) continue;
        candidates.push_back(enemy);
    }
    std::sort(candidates.begin(), candidates.end(),
        [](const AIHeroClient& left, const AIHeroClient& right) {
            return left.Health() + left.AllShield() <
                   right.Health() + right.AllShield();
        });
    for (const auto& target : candidates) {
        if (IsCommonUntargetableOrImmune(target) ||
            HasSpellShieldOrImmunity(target)) continue;
        const float shieldedHealth = target.Health() + target.AllShield();
        if (Ready(0) && SpellEnabled(0, Mode::Automatic)) {
            const float qDamage = player.CalculateMagicDamage(
                target, QRawDamage(SpellRank(0), player.AP()));
            if (qDamage >= shieldedHealth) {
                const QPlan q = BuildQPlan(target, QPurpose::KillSecure);
                if (q.Valid && q.Evaluation.PrimaryHit && CastQPlan(q, true)) return true;
            }
        }
        if (Ready(2) && SpellEnabled(2, Mode::Automatic)) {
            const float eDamage = player.CalculateMagicDamage(
                target, ERawDamage(SpellRank(2), player.AP()));
            if (eDamage >= shieldedHealth) {
                const EPlan e = BuildEPlan(
                    target, EPurpose::CollisionKill);
                if (CastEPlan(e, true)) return true;
            }
        }
        if (Ready(3) && Bool(RMenu, "KillSecure", true) &&
            player.CalculateMagicDamage(
                target, RRawDamage(SpellRank(3), player.AP())) >=
                shieldedHealth) {
            const RPlan r = BuildRPlan(
                target, RPurpose::KillSecure, true);
            if (CastRPlan(r, true)) return true;
        }
    }
    return false;
}

inline bool TryOpenSoldierTrade(const AIHeroClient& target,
                                Mode mode) {
    if (!Engine::ValidEnemy(target) || !WAvailable() ||
        !SpellEnabled(1, mode)) return false;
    const WPurpose purpose = mode == Mode::Harass
        ? WPurpose::FirstTradeSoldier : WPurpose::ExtendDps;
    const WPlan w = BuildWPlan(target, purpose);
    if (!w.Valid || !w.CreatesCoverage || !CastWPlan(w)) return false;
    StartSequence(Sequence::WAutoQAuto,
                  SequencePhase::AwaitSoldier,
                  static_cast<int>(target.NetworkId()), 0,
                  mode == Mode::Harass ? 2500 : 2900);
    return true;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    if (TryAutomaticShuffle(target)) return true;

    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return false;

    const int id = static_cast<int>(target.NetworkId());
    const float dist = player.Position().Distance2D(target.Position());
    const int attackers = CurrentSoldierAttackers(target);
    const auto soldiers = BuildSoldiers();

    // 1. Multi-Target R
    if (Ready(3) && Bool(RMenu, "MultiTarget", true) &&
        Engine::CountEnemiesAt(target.Position(), 460.0f) >=
            Slider(RMenu, "MinimumHits", 2)) {
        const RPlan r = BuildRPlan(target, RPurpose::MultiTarget);
        if (CastRPlan(r)) return true;
    }

    // 2. W Placement (Soldier Spawning & Long-Range W-Q Setup)
    if (WAvailable() && SpellEnabled(1, Mode::Combo) && !Orbwalker::IsWindingUp()) {
        const Vector3 targetPos = PredictPosition(target, kWCastSeconds);
        const float targetDistFromPlayer = player.Position().Distance2D(targetPos);
        const Vector3 castPos = ClampCast(player.Position(), targetPos, kWSpawnRange);
        const float distFromSoldierToTarget = castPos.Distance2D(targetPos);
        const float maxReach = SoldierRules::kPrimaryAttackRange + target.BoundingRadius() - 15.0f;

        // Case A: Target in direct W range + attack reach (500 + 350 = ~850 max reach)
        if (distFromSoldierToTarget <= maxReach) {
            // Place W if target not currently covered by soldiers, or if we have 2 W charges for max DPS
            if (attackers == 0 || (WCharges() >= 2 && attackers < 2)) {
                if (Engine::ControllerCastPosition(1, castPos)) {
                    LastWCastTick = Now();
                    PendingWPosition = castPos;
                    PendingAnchorUntil = Now() + 900;
                    return true;
                }
            }
        }
        // Case B: Long-range W-Q Poke/Engage (500 < dist <= 1100 and Q is ready!)
        else if (targetDistFromPlayer <= 1100.0f && Ready(0) && SpellEnabled(0, Mode::Combo)) {
            if (Engine::ControllerCastPosition(1, castPos)) {
                LastWCastTick = Now();
                PendingWPosition = castPos;
                PendingAnchorUntil = Now() + 900;
                return true;
            }
        }
    }

    // 3. Q Repositioning & AA Reset (Conquering Sands)
    if (Ready(0) && !soldiers.empty() && SpellEnabled(0, Mode::Combo)) {
        const Vector3 targetPos = PredictPosition(target, 0.25f);
        const float targetDistFromPlayer = player.Position().Distance2D(targetPos);

        // Q can reach target if target is within Q range (720) + soldier attack range (350)
        if (targetDistFromPlayer <= kQCastRange + SoldierRules::kPrimaryAttackRange) {
            bool shouldQ = false;

            // Condition 1: Target is outside current soldier attack range
            if (attackers == 0) {
                shouldQ = true;
            }
            // Condition 2: AA-Q Reset (Just finished auto-attack, reset animation!)
            else if (!Orbwalker::CanAttack() && !Orbwalker::IsWindingUp()) {
                shouldQ = true;
            }
            // Condition 3: Target is running away / escaping
            else if (dist > 550.0f || !RetreatDirection(target).IsZero()) {
                shouldQ = true;
            }

            if (shouldQ) {
                const Vector3 retreat = RetreatDirection(target);
                const Vector3 qCastPos = ClampCast(player.Position(), targetPos + retreat * 50.0f, kQCastRange);
                if (Engine::ControllerCastPosition(0, qCastPos)) {
                    LastQCastTick = Now();
                    return true;
                }
            }
        }
    }

    // 4. E Collision / Gapclose Finisher
    if (Ready(2) && Bool(EMenu, "CollisionRefund", true) && !soldiers.empty()) {
        if (target.HealthPercent() <= 35.0f || player.HealthPercent() >= 55.0f) {
            const EPlan e = BuildEPlan(target, EPurpose::CollisionRefund);
            if (e.Valid && CastEPlan(e)) {
                StartSequence(Sequence::CollisionRefund,
                              e.NeedsQ ? SequencePhase::AwaitQBuffer : SequencePhase::RecoverDps,
                              id, e.AnchorId, 1100);
                PendingQPosition = e.RedirectPosition;
                return true;
            }
        }
    }

    // 5. R Finisher / Pick
    if (Ready(3) && Bool(RMenu, "Pick", true) && target.HealthPercent() <= 35.0f &&
        CountAlliedFollowup(target.Position(), 950.0f) > 0) {
        const RPlan r = BuildRPlan(target, RPurpose::Pick);
        if (CastRPlan(r)) return true;
    }
    return false;
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) ||
        PlayerManaPercent() < Slider(QMenu, "HarassMana", 40)) {
        return false;
    }
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return false;

    const float dist = player.Position().Distance2D(target.Position());
    const int attackers = CurrentSoldierAttackers(target);
    const auto soldiers = BuildSoldiers();

    if (WAvailable() && SpellEnabled(1, Mode::Harass) && !Orbwalker::IsWindingUp()) {
        const Vector3 targetPos = PredictPosition(target, kWCastSeconds);
        const Vector3 castPos = ClampCast(player.Position(), targetPos, kWSpawnRange);
        const float distFromSoldierToTarget = castPos.Distance2D(targetPos);
        const float maxReach = SoldierRules::kPrimaryAttackRange + target.BoundingRadius() - 15.0f;

        if (distFromSoldierToTarget <= maxReach && attackers == 0) {
            if (Engine::ControllerCastPosition(1, castPos)) {
                LastWCastTick = Now();
                return true;
            }
        }
    }

    if (Ready(0) && !soldiers.empty() && SpellEnabled(0, Mode::Harass)) {
        const Vector3 targetPos = PredictPosition(target, 0.25f);
        if (attackers == 0 || (!Orbwalker::CanAttack() && !Orbwalker::IsWindingUp())) {
            const Vector3 qCastPos = ClampCast(player.Position(), targetPos, kQCastRange);
            if (Engine::ControllerCastPosition(0, qCastPos)) {
                LastQCastTick = Now();
                return true;
            }
        }
    }
    return false;
}

inline bool TryFarm(Mode mode) {
    // OrbwalkerKuro's soldier-aware last-hit path is more precise and does
    // not spend mana.  The champion controller intentionally delegates it.
    if (mode == Mode::LastHit) return false;
    if (Bool(FarmMenu, "HoldForChampion", true) &&
        HasEnemyChampionNear(static_cast<float>(
            Slider(FarmMenu, "ChampionHoldRange", 1150)))) {
        return false;
    }
    const bool jungle = HasNearbyJungleTarget(900.0f) &&
        GameObjects::EnemyMinions().empty();
    const float manaFloor = static_cast<float>(Slider(
        FarmMenu, jungle ? "JungleMana" : "LaneMana",
        jungle ? 32 : 56));
    if (PlayerManaPercent() < manaFloor) return false;

    if (Ready(0) && !BuildSoldiers().empty() &&
        Bool(FarmMenu, "UseQ", true) &&
        !Orbwalker::IsWindingUp()) {
        const QPlan q = BuildQPlan(
            {}, jungle ? QPurpose::JungleFormation
                       : QPurpose::WaveFormation,
            true, jungle);
        const int minimum = Slider(
            FarmMenu, jungle ? "JungleQMinimum" : "LaneQMinimum",
            jungle ? 1 : 4);
        if (q.Valid && q.Evaluation.FarmHits >= minimum &&
            !q.EscapeAnchorLost && CastQPlan(q)) return true;
    }

    if (WAvailable() && Bool(FarmMenu, "UseW", true)) {
        const WPlan w = BuildWPlan(
            {}, jungle ? WPurpose::JungleZone : WPurpose::WaveZone,
            true, jungle);
        const int minimum = Slider(
            FarmMenu, jungle ? "JungleWMinimum" : "LaneWMinimum",
            jungle ? 1 : 3);
        if (w.Valid && w.FarmHits >= minimum && CastWPlan(w)) {
            StartSequence(Sequence::FarmFormation,
                          SequencePhase::RecoverDps,
                          0, 0, 850);
            return true;
        }
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& fallback) {
    const AIHeroClient threat = NearestEnemyToPlayer(fallback, 1200.0f);
    if (Ready(2) && !BuildSoldiers().empty() &&
        Bool(EMenu, "Flee", true)) {
        const EPlan e = BuildEPlan(
            threat, EPurpose::Flee, Game::CursorPos());
        if (CastEPlan(e, true)) {
            StartSequence(Sequence::DriftEscape,
                          e.NeedsQ ? SequencePhase::AwaitQBuffer
                                   : SequencePhase::RecoverDps,
                          threat.IsValid()
                              ? static_cast<int>(threat.NetworkId()) : 0,
                          e.AnchorId, e.NeedsQ ? 1100 : 650);
            PendingQPosition = e.RedirectPosition;
            return true;
        }
    }
    if (WAvailable() && Bool(WMenu, "FleeAnchor", true)) {
        const WPlan w = BuildWPlan(threat, WPurpose::FleeAnchor);
        if (CastWPlan(w, true)) {
            StartSequence(Sequence::DriftEscape,
                          SequencePhase::AwaitSoldier,
                          threat.IsValid()
                              ? static_cast<int>(threat.NetworkId()) : 0,
                          0, 1700);
            return true;
        }
    }
    if (Engine::ValidEnemy(threat) && Ready(3) &&
        Bool(RMenu, "FleePeel", true)) {
        const RPlan r = BuildRPlan(threat, RPurpose::Disengage, true);
        if (CastRPlan(r, true)) return true;
    }
    return false;
}

inline void RefreshRuntimeState() {
    RefreshSoldiers();
    const int now = Now();
    if (GapcloserExpireTick < now) {
        GapcloserTargetId = 0;
        GapcloserEndpoint = {};
    }
    if (InterruptExpireTick < now) InterruptTargetId = 0;
    if (IncomingThreatUntil < now) {
        IncomingThreatTargetId = 0;
        IncomingHardCrowdControl = false;
    }
    for (auto& ruin : TurretRuins) {
        if (ruin.NetworkId != 0 && now - ruin.LastSeenTick > 600000) {
            ruin = {};
        }
    }
    const auto player = ObjectManager::Player();
    if (player.IsValid()) LastKnownPlayerPosition = player.Position();
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastKnownMode = mode;
    RefreshRuntimeState();
    AIHeroClient target = Engine::ValidEnemy(selected)
        ? selected : Engine::SelectTarget(1250.0f);
    if (!Engine::ValidEnemy(target)) {
        target = NearestEnemyToPlayer({}, 1550.0f);
    }
    CurrentPosture = DeterminePosture(mode, target);

    if (TryManualShuffle()) return true;
    // A manual spell means the player owns the immediate continuation.  Do
    // not auto-complete their E-Q or E-Q-R input behind their back.
    if (PlayerOverrideUntil >= Now()) return false;
    if (TryReactiveDefense(mode)) return true;
    if (ActiveSequence.Kind != Sequence::None) {
        (void)TryAdvanceSequence(mode);
        return true;
    }
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
    if (args.IsAutoAttack) {
        if (ControllerHelpers::CaptureLocalAutoAttack(
                args, LastAfterAttackTargetId, LastAfterAttackTick) &&
            ActiveSequence.Phase == SequencePhase::AwaitFirstAttack &&
            LastAfterAttackTargetId == ActiveSequence.TargetId) {
            TransitionSequence(SequencePhase::AwaitTargetExit, 1250);
        }
        return;
    }

    int slot = -1;
    if (IsQEvent(args)) slot = 0;
    else if (IsWEvent(args)) slot = 1;
    else if (IsEEvent(args)) slot = 2;
    else if (IsREvent(args)) slot = 3;
    const bool passive = SpellEventNameContainsAny(args, {
        "azirpassive", "shurimaslegacy", "sundisc",
    });
    if (passive) PassiveReadyTick = now +
        static_cast<int>(kPassiveCooldownSeconds * 1000.0f);
    if (slot < 0) return;

    const bool controllerOwned = Engine::WasControllerCast(slot);
    LastLocalSpellTick = now;
    if (slot == 0) {
        LastQCastTick = now;
        if (!controllerOwned) LastQPurpose = QPurpose::Manual;
    } else if (slot == 1) {
        LastWCastTick = now;
        if (args.EndPosition.IsValid() && !args.EndPosition.IsZero()) {
            PendingWPosition = args.EndPosition;
            PendingAnchorUntil = now + 900;
        }
        if (!controllerOwned) LastWPurpose = WPurpose::Manual;
    } else if (slot == 2) {
        LastECastTick = now;
        EDashUntil = std::max(EDashUntil, now + 950);
        if (!controllerOwned) LastEPurpose = EPurpose::Manual;
    } else if (slot == 3) {
        LastRCastTick = now;
        if (!controllerOwned) LastRPurpose = RPurpose::None;
    }

    if (!controllerOwned) {
        ClearSequence();
        PlayerOverrideUntil = now +
            Slider(TacticsMenu, "ManualOwnershipMs", 520);
        StartSequence(Sequence::PlayerLed,
                      SequencePhase::RecoverDps,
                      static_cast<int>(args.TargetNetworkId), 0,
                      std::max(560, PlayerOverrideUntil - now + 80), false);
    }
}

inline void RecordSoldierAttack(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsSoldierAttackEvent(args) || !IsOwnedSoldierSender(args)) return;
    const std::uint32_t observed = args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId;
    LastSoldierAttackTargetId = static_cast<int>(observed);
    LastSoldierAttackTick = Now();
    if (ActiveSequence.Phase == SequencePhase::AwaitFirstAttack &&
        LastSoldierAttackTargetId == ActiveSequence.TargetId) {
        TransitionSequence(SequencePhase::AwaitTargetExit, 1250);
    }
}

inline void RecordEnemySpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid() || args.Sender.Type != ::Core::Objects::ObjectType::AIHeroClient) {
        return;
    }
    const auto analysis = AnalyzeEnemyCast(
        args, 220.0f, 115.0f, 280, 250, 220, 1650, 480);
    if (!analysis.Valid) return;
    const int id = static_cast<int>(analysis.Enemy.NetworkId());
    EnemyWindow* window = FindEnemyWindow(id, true);
    if (window) {
        if (analysis.Committed) {
            window->CommittedUntil = std::max(
                window->CommittedUntil,
                analysis.CommitmentUntilTick);
        }
        if (analysis.LikelyHardCrowdControl) {
            window->HardCrowdControlSpentUntil = std::max(
                window->HardCrowdControlSpentUntil, Now() + 3600);
        }
        for (const auto& rule : EscapeRules) {
            if (ControllerHelpers::ChampionIs(
                    analysis.Enemy, rule.Champion) &&
                args.Slot == static_cast<int>(rule.Slot)) {
                window->MobilitySpentUntil = std::max(
                    window->MobilitySpentUntil, Now() + 3200);
                break;
            }
        }
    }
    if (analysis.TargetsPlayer || analysis.CrossesPlayer) {
        IncomingThreatTargetId = id;
        IncomingThreatUntil = std::max(
            analysis.CommitmentUntilTick,
            analysis.LineThreatUntilTick);
        if (IncomingThreatUntil <= Now()) IncomingThreatUntil = Now() + 420;
        IncomingHardCrowdControl = analysis.LikelyHardCrowdControl;
    }
}

inline void OnProcessSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsSoldierAttackEvent(args) && IsOwnedSoldierSender(args)) {
        RecordSoldierAttack(args);
        return;
    }
    if (IsLocalPlayer(args.Sender)) ObserveLocalSpell(args);
    else RecordEnemySpell(args);
}

inline void OnDoCast(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsSoldierAttackEvent(args) && IsOwnedSoldierSender(args)) {
        RecordSoldierAttack(args);
        return;
    }
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return;
    (void)ControllerHelpers::CaptureLocalAutoAttack(
        args, LastAfterAttackTargetId, LastAfterAttackTick);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    (void)ControllerHelpers::CaptureBeforeAttack(
        args, LastBeforeAttackTargetId, LastBeforeAttackTick);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(
            args, LastAfterAttackTargetId, LastAfterAttackTick)) return;
    if (ActiveSequence.Phase == SequencePhase::AwaitFirstAttack &&
        LastAfterAttackTargetId == ActiveSequence.TargetId) {
        TransitionSequence(SequencePhase::AwaitTargetExit, 1250);
    }
}

inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (CaptureGapcloser(
            args, GapcloserTargetId, GapcloserEndpoint,
            GapcloserExpireTick, 820.0f, 1150)) {
        IncomingThreatTargetId = GapcloserTargetId;
        IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 760);
    }
}

inline void OnInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(
        args, InterruptTargetId, InterruptExpireTick,
        1100, 250, 5500);
    EnemyWindow* window = FindEnemyWindow(InterruptTargetId, true);
    if (window) window->CommittedUntil = std::max(
        window->CommittedUntil, InterruptExpireTick);
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args,
                            bool added) {
    if (!IsLocalPlayer(args.Sender)) return;
    if (ControllerHelpers::TextContainsAny(args.BuffName, {
            "azirpassivecooldown", "shurimaslegacycooldown" })) {
        PassiveReadyTick = added
            ? std::max(PassiveReadyTick,
                ControllerHelpers::BuffExpireTick(args, 90000))
            : Now();
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

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    if (IsSandSoldierObject(args) && ObjectEventIsAllied(args)) {
        const int id = static_cast<int>(args.Sender.NetworkId);
        SoldierRecord* record = FindSoldierRecord(id, true);
        if (!record) return;
        const int now = Now();
        record->NetworkId = id;
        record->Position = args.Sender.Position;
        record->SpawnTick = now;
        record->ExpireTick = now + static_cast<int>(
            kWSoldierLifetimeSeconds * 1000.0f);
        record->LastSeenTick = now;
        const std::uint32_t playerId =
            static_cast<std::uint32_t>(player.NetworkId());
        record->ConfirmedOwned = args.SourceNetworkId == playerId ||
            args.Source.NetworkId == playerId ||
            (LastWCastTick > 0 && now - LastWCastTick <= 950 &&
             (!PendingWPosition.IsValid() || PendingWPosition.IsZero() ||
              record->Position.Distance2D(PendingWPosition) <= 220.0f));
        record->NearEnemyTurret = NearEnemyTurret(
            record->Position, -110.0f);
        if (record->NearEnemyTurret) {
            record->ExpireTick = now + static_cast<int>(
                kWSoldierLifetimeSeconds *
                kWTurretLifetimeMultiplier * 1000.0f);
        }
        if (ActiveSequence.Phase == SequencePhase::AwaitSoldier &&
            now <= PendingAnchorUntil + 180 &&
            (!PendingWPosition.IsValid() || PendingWPosition.IsZero() ||
             record->Position.Distance2D(PendingWPosition) <= 220.0f)) {
            ActiveSequence.AnchorId = id;
            PendingAnchorId = id;
        }
        return;
    }
    if (IsRuinName(args.Sender.CharacterName, args.Sender.Name)) {
        RuinRecord* ruin = FindRuin(
            static_cast<int>(args.Sender.NetworkId), true);
        if (ruin) {
            ruin->Position = args.Sender.Position;
            ruin->LastSeenTick = Now();
            ruin->Valid = true;
        }
    }
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (SoldierRecord* soldier = FindSoldierRecord(id)) *soldier = {};
    if (RuinRecord* ruin = FindRuin(id)) *ruin = {};
    if (ActiveSequence.AnchorId == id &&
        ActiveSequence.Phase != SequencePhase::RecoverDps) {
        ClearSequence();
    }
}

inline const char* SequenceName(Sequence sequence) {
    switch (sequence) {
    case Sequence::WAutoQAuto: return "W-AA-Q-AA";
    case Sequence::ExtendedSoldierDps: return "extended soldier DPS";
    case Sequence::DriftEscape: return "W-E-Q escape";
    case Sequence::DriftEngage: return "W-E-Q drift";
    case Sequence::ShurimaShuffle: return "Shurima shuffle";
    case Sequence::RevenantShuffle: return "revenant shuffle";
    case Sequence::CollisionRefund: return "E collision refund";
    case Sequence::DefensivePeel: return "R peel";
    case Sequence::FarmFormation: return "soldier farm formation";
    case Sequence::PlayerLed: return "player-led";
    default: return "idle";
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::LaneControl: return "lane control";
    case Posture::FrontToBack: return "front-to-back";
    case Posture::ExtendedDps: return "extended DPS";
    case Posture::Pick: return "pick";
    case Posture::Shuffle: return "shuffle";
    case Posture::Peel: return "peel";
    case Posture::Farm: return "farm";
    case Posture::Flee: return "flee";
    default: return "neutral";
    }
}

inline bool SunDiscSuggestion(Vector3& position) {
    position = {};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || PassiveReadyTick > Now()) return false;
    for (const auto& ruin : TurretRuins) {
        if (!ruin.Valid || ruin.NetworkId == 0 ||
            ruin.Position.Distance2D(player.Position()) >
                kPassiveCastRange + 25.0f) continue;
        SunDiscContext context{};
        context.PassiveReady = true;
        context.RuinInRange = true;
        context.PlayerChannelSafe =
            Engine::CountEnemiesAt(player.Position(), 850.0f) == 0 &&
            !Engine::UnderEnemyTurret(player.Position());
        context.ObjectiveSoon = HasNearbyJungleTarget(1900.0f);
        context.SideLanePressure =
            GameObjects::EnemyMinions().size() >= 4;
        context.DefendingBase = Engine::CountEnemiesAt(
            ruin.Position, 1350.0f) > 0;
        context.TeamCanUseZone = CountAlliedFollowup(
            ruin.Position, 1500.0f) > 0;
        context.EnemyCanImmediatelyDestroy = Engine::CountEnemiesAt(
            ruin.Position, 600.0f) >=
            Engine::CountAlliesAt(ruin.Position, 850.0f) + 2;
        context.PlayerLeavingArea =
            !CursorDirectionAgrees(ruin.Position, -0.15f);
        context.AlliedFollowup = CountAlliedFollowup(
            ruin.Position, 1500.0f);
        context.NearbyEnemies = Engine::CountEnemiesAt(
            ruin.Position, 1200.0f);
        if (ShouldSuggestSunDisc(context)) {
            position = ruin.Position;
            return true;
        }
    }
    return false;
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    if (Bool(CoachMenu, "DrawRanges", true)) {
        Drawing::DrawCircle(player.Position(), kWSpawnRange,
                            0x3374D9FFu, 1.0f, 72);
        Drawing::DrawCircle(player.Position(),
                            SoldierRules::kCommandRadius,
                            0x3369F0AEu, 1.0f, 72);
        Drawing::DrawCircle(player.Position(), kQCastRange,
                            0x335E8BFFu, 1.0f, 72);
    }
    if (Bool(CoachMenu, "DrawSoldiers", true)) {
        for (const auto& soldier : BuildSoldiers(false)) {
            const bool commandable = Commandable(
                player.Position(), soldier);
            Drawing::DrawCircle(
                soldier.Position,
                SoldierRules::kPrimaryAttackRange,
                commandable ? 0xAAE8C44Fu : 0x667A7A7Au,
                commandable ? 1.6f : 1.0f, 48);
            Drawing::DrawLine(
                player.Position(), soldier.Position,
                commandable ? 0x8877E2C4u : 0x55777777u, 1.0f);
        }
    }
    if (Bool(CoachMenu, "DrawQFormation", true) && LastQPlan.Valid) {
        for (const auto& endpoint : LastQPlan.Evaluation.Endpoints) {
            Drawing::DrawLine(player.Position(), endpoint,
                              0xCC65A9FFu, 1.7f);
            Drawing::DrawCircle(endpoint, 48.0f,
                                0xAA65A9FFu, 1.3f, 28);
        }
    }
    if (Bool(CoachMenu, "DrawDrift", true) && LastEPlan.Valid) {
        Drawing::DrawLine(player.Position(), LastEPlan.AnchorPosition,
                          0xCCE8C44Fu, 2.0f);
        if (LastEPlan.NeedsQ) {
            Drawing::DrawLine(LastEPlan.AnchorPosition,
                              LastEPlan.FinalEndpoint,
                              0xCCFF9E62u, 2.0f);
            Drawing::DrawCircle(LastEPlan.FinalEndpoint, 72.0f,
                                0xAAFF9E62u, 1.5f, 32);
        }
    }
    if (Bool(CoachMenu, "DrawR", true) && LastRPlan.Valid) {
        const auto& evaluation = LastRPlan.Evaluation;
        Drawing::DrawLine(player.Position(), evaluation.PrimaryLanding,
                          LastRPlan.Defensive
                              ? 0xCC66F3B6u : 0xCCF39A66u,
                          2.2f);
        if (!evaluation.PrimaryLanding.IsZero()) {
            const Vector3 side = SharedGeometry::Rotate2D(
                evaluation.Direction, SharedGeometry::kPi * 0.5f);
            const float half = RWallHalfLength(
                std::max(1, SpellRank(3)));
            Drawing::DrawLine(
                evaluation.PrimaryLanding - side * half,
                evaluation.PrimaryLanding + side * half,
                0xCCF3D166u, 2.3f);
        }
    }
    Vector3 disc{};
    if (Bool(PassiveMenu, "SuggestSunDisc", true) &&
        SunDiscSuggestion(disc)) {
        Drawing::DrawCircle(disc, 125.0f,
                            0xDDE6C65Au, 2.2f, 48);
        Vec2 discScreen{};
        if (Drawing::WorldToScreen(disc, discScreen)) {
            Drawing::DrawText(discScreen.x - 85.0f,
                              discScreen.y - 48.0f,
                              0xFFE8D26Au,
                              "Sun Disc window (player click)");
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char text[620]{};
            _snprintf_s(
                text, sizeof(text), _TRUNCATE,
                "Azir OTP | %s | %s | soldiers %zu | W ammo %d | target soldiers %d | owner %s",
                PostureName(CurrentPosture),
                SequenceName(ActiveSequence.Kind),
                BuildSoldiers(false).size(), WCharges(),
                ActiveSequence.TargetId != 0
                    ? CurrentSoldierAttackers(
                        HeroByNetworkId(ActiveSequence.TargetId)) : 0,
                PlayerOverrideUntil >= Now() ? "player" : "controller");
            Drawing::DrawText(screen.x - 300.0f, screen.y - 116.0f,
                              0xFFE6C65Au, text);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "AzirOneTrick", "Azir one-trick mechanics"));
    TacticsMenu->Add(new MenuBool(
        "KillSecure", "Exact Q/E/R kills", true));
    TacticsMenu->Add(new MenuSlider(
        "DefensiveHealth", "Peel R health threshold (%)", 42, 10, 90));
    TacticsMenu->Add(new MenuSlider(
        "ManualOwnershipMs", "Yield after player spell (ms)",
        520, 180, 1100));
    TacticsMenu->Add(new MenuSeparator(
        "Ownership",
        "Movement, attack commands,"));

    SoldierMenu = TacticsMenu->AddSubMenu(new Menu(
        "SandSoldiers", "Sand Soldier command and reserve policy"));
    SoldierMenu->Add(new MenuBool(
        "ReserveForE", "Keep W charge for E", true));
    SoldierMenu->Add(new MenuBool(
        "PreserveEscapeAnchor", "Preserve E anchor", true));
    SoldierMenu->Add(new MenuSeparator(
        "Orbwalker",
        "OrbwalkerKuro now attacks,"));

    QMenu = TacticsMenu->AddSubMenu(new Menu(
        "ConqueringSands", "Late Q, formation and retreat tracking"));
    QMenu->Add(new MenuSlider(
        "HarassMana", "Minimum mana for W-AA-Q-AA (%)", 52, 0, 100));
    QMenu->Add(new MenuSeparator(
        "LateQ",
        "Q follows the first soldier"));

    WMenu = TacticsMenu->AddSubMenu(new Menu(
        "Arise", "Soldier placement and charge economy"));
    WMenu->Add(new MenuBool(
        "DefensiveAnchor", "Place a safe anchor vs", true));
    WMenu->Add(new MenuBool(
        "FollowupAfterR", "Soldier on R path", true));
    WMenu->Add(new MenuBool(
        "FleeAnchor", "W toward flee cursor", true));

    EMenu = TacticsMenu->AddSubMenu(new Menu(
        "ShiftingSands", "First-collision dash and E-Q drift"));
    EMenu->Add(new MenuBool(
        "CollisionRefund", "Use verified champion", true));
    EMenu->Add(new MenuBool(
        "ReactiveEscape", "E to safe soldier", true));
    EMenu->Add(new MenuBool(
        "Flee", "W-E-Q flee cursor", true));
    EMenu->Add(new MenuSeparator(
        "Projectile",
        "E is rejected across live"));

    RMenu = TacticsMenu->AddSubMenu(new Menu(
        "EmperorsDivide", "Peel-first R and gated shuffle policy"));
    RMenu->Add(new MenuBool(
        "ReactivePeel", "R committed divers and", true));
    RMenu->Add(new MenuBool(
        "MultiTarget", "R multi-target value", true));
    RMenu->Add(new MenuBool(
        "Pick", "Push target to allies", true));
    RMenu->Add(new MenuBool(
        "KillSecure", "R mitigated lethal only", true));
    RMenu->Add(new MenuBool(
        "FleePeel", "Use R as the final flee peel", true));
    RMenu->Add(new MenuSlider(
        "MinimumHits", "Min off R hits", 2, 1, 5));
    RMenu->Add(new MenuBool(
        "AutomaticShuffle", "Automatically shuffle only a", false));
    RMenu->Add(new MenuSlider(
        "ShuffleMinimumEnemies", "Min enemies for shuffle",
        3, 2, 5));
    RMenu->Add(new MenuKeyBind(
        "ManualShuffle", "Manual Shurima shuffle toward cursor [T]",
        SDK::Keys::T, KeyBindType::Press));
    RMenu->Add(new MenuKeyBind(
        "ManualRevenant", "Manual revenant shuffle toward cursor [G]",
        SDK::Keys::G, KeyBindType::Press));
    RMenu->Add(new MenuSeparator(
        "NoFlash",
        "Flash extensions are coached"));

    PassiveMenu = TacticsMenu->AddSubMenu(new Menu(
        "ShurimasLegacy", "Sun Disc macro advisory"));
    PassiveMenu->Add(new MenuBool(
        "SuggestSunDisc", "Draw a safe", true));
    PassiveMenu->Add(new MenuSeparator(
        "AdvisoryOnly",
        "The passive requires a"));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Soldier formation without wasting mobility"));
    FarmMenu->Add(new MenuBool(
        "HoldForChampion", "Hold spells vs contest", true));
    FarmMenu->Add(new MenuSlider(
        "ChampionHoldRange", "Champion contest range", 1150, 500, 1800));
    FarmMenu->Add(new MenuBool(
        "UseW", "Place a soldier on valuable", true));
    FarmMenu->Add(new MenuBool(
        "UseQ", "Re-form existing soldiers", true));
    FarmMenu->Add(new MenuSlider(
        "LaneWMinimum", "Minimum lane units for W", 3, 1, 8));
    FarmMenu->Add(new MenuSlider(
        "LaneQMinimum", "Minimum lane units for Q", 4, 1, 10));
    FarmMenu->Add(new MenuSlider(
        "JungleWMinimum", "Minimum jungle units for W", 1, 1, 6));
    FarmMenu->Add(new MenuSlider(
        "JungleQMinimum", "Minimum jungle units for Q", 1, 1, 6));
    FarmMenu->Add(new MenuSlider(
        "LaneMana", "Minimum lane-clear mana (%)", 56, 0, 100));
    FarmMenu->Add(new MenuSlider(
        "JungleMana", "Minimum jungle-clear mana (%)", 32, 0, 100));
    FarmMenu->Add(new MenuSeparator(
        "LastHit",
        "Last-hit is delegated to"));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "One-trick state and geometry visualization"));
    CoachMenu->Add(new MenuBool(
        "DrawRanges", "Draw W/soldier/Q ranges", false));
    CoachMenu->Add(new MenuBool(
        "DrawSoldiers", "Draw soldier zones", false));
    CoachMenu->Add(new MenuBool(
        "DrawQFormation", "Draw every selected Q endpoint", false));
    CoachMenu->Add(new MenuBool(
        "DrawDrift", "Draw E anchor and E-Q endpoint", false));
    CoachMenu->Add(new MenuBool(
        "DrawR", "Draw R push/wall", false));
    CoachMenu->Add(new MenuBool(
        "DrawState", "Draw posture/seq, ammo", false));
}

inline void OnLoad() {
    SoldierRecords.fill({});
    EnemyWindows.fill({});
    TurretRuins.fill({});
    ActiveSequence = {};
    CurrentPosture = Posture::Neutral;
    LastKnownMode = Mode::None;
    LastQPurpose = QPurpose::None;
    LastWPurpose = WPurpose::None;
    LastEPurpose = EPurpose::None;
    LastRPurpose = RPurpose::None;
    LastQPlan = {};
    LastWPlan = {};
    LastEPlan = {};
    LastRPlan = {};
    LastBeforeAttackTargetId = LastBeforeAttackTick = 0;
    LastAfterAttackTargetId = LastAfterAttackTick = 0;
    LastSoldierAttackTargetId = LastSoldierAttackTick = 0;
    LastLocalSpellTick = LastQCastTick = LastWCastTick = 0;
    LastECastTick = LastRCastTick = EDashUntil = 0;
    PlayerOverrideUntil = 0;
    PendingAnchorId = PendingAnchorUntil = 0;
    PendingWPosition = PendingQPosition = SequenceOrigin = {};
    SequenceManualIntent = false;
    LastKnownPlayerPosition = {};
    PassiveReadyTick = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    InterruptTargetId = InterruptExpireTick = 0;
    IncomingThreatTargetId = IncomingThreatUntil = 0;
    IncomingHardCrowdControl = false;
    RefreshRuntimeState();
}

inline void OnUnload() {
    TacticsMenu = SoldierMenu = QMenu = WMenu = nullptr;
    EMenu = RMenu = PassiveMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Pin Summoner's Rift behavior to Riot 26.14 and CommunityDragon PC 16.14",
    "Use current Q 60/80/100/120/140 base damage",
    "Use current Q 35/40/45/50/55 percent AP ratios after patch 26.6",
    "Use current Q 70 through 110 mana cost",
    "Use current Q 14/12/10/8/6 second cooldown",
    "Use the 0.25-second Q cast time and 1600 soldier move speed",
    "Clamp Q commands to 720 rather than the 740 display range",
    "Move every commandable soldier on one Q cast",
    "Give Q only one damage instance even when several soldier paths cross",
    "Slow a Q-hit target by 25 percent for one second",
    "Place moved soldiers roughly 50 units beyond the requested Q point",
    "Spread multiple soldiers laterally instead of stacking one fake endpoint",
    "Sort soldiers deterministically before assigning formation endpoints",
    "Test each original soldier-to-endpoint Q capsule against every body",
    "Credit a champion hit once even if multiple Q capsules cross it",
    "Predict the target at cast plus soldier travel time",
    "Aim Q along the target's actual retreat vector",
    "Use a path endpoint candidate when the target is visibly pathing",
    "Prefer future soldier attackers over cosmetic Q damage",
    "Penalize Q that loses existing primary-target coverage",
    "Never move the only E escape anchor outside command range",
    "Allow drift Q to override ordinary escape-anchor preservation",
    "Insert the precomputed E-Q buffer point before generic Q candidates",
    "Use the player's cursor as the flee redirect when no stored point exists",
    "Use the sequence origin as the revenant Q return point",
    "Delay ordinary Q until a soldier auto has completed",
    "Delay Q while the player or orbwalker is winding up",
    "Cast late Q when the target is leaving current stab coverage",
    "Cast late Q when it creates coverage that does not currently exist",
    "Cast late Q immediately when its mitigated damage is lethal",
    "Cast late Q on a hard-controlled target when the formation remains useful",
    "Reject late Q when it misses both damage and future attack coverage",
    "Reject late Q when it spends the only E anchor",
    "Hold Q when two current attackers would become fewer without a retreat need",
    "Reserve E mana while late Q protects the escape resource",
    "Use W-AA-Q-AA as the canonical lane trade",
    "Place W slightly along the target's retreat rather than directly on Azir",
    "Place a second W candidate on current predicted target position",
    "Place W at 470 forward when a closer zoning soldier is better",
    "Require offensive W to create target coverage or a real zone",
    "Keep one W charge for E while E is available",
    "Permit the last W charge only for defense, flee or a committed target",
    "Reject W terrain positions",
    "Respect the player's cursor direction for offensive soldier placement",
    "Do not interrupt a valuable auto windup with offensive W",
    "Prefer W after the target has committed or key control is spent",
    "Track the current W ammo count rather than assuming ready means two charges",
    "Track every soldier for its ten-second ordinary lifetime",
    "Shorten predicted soldier lifetime to five seconds near an enemy turret",
    "Confirm soldier ownership through source id or the recent W spawn window",
    "Reject enemy and stale soldiers from Azir planning",
    "Keep non-commandable soldiers visible for coaching but out of cast plans",
    "Use a 660-unit Azir-to-soldier command tether",
    "Use a 350-unit soldier primary target range plus target bounding radius",
    "Never use the spear's extra 50 units to legalize a primary target command",
    "Never attack structures, wards or traps through a Sand Soldier",
    "Allow the ordinary Azir basic attack when no soldier can command the target",
    "Recognize AzirBasicAttackSoldier and bridge aliases as soldier attacks",
    "Deduplicate simultaneous soldier stabs in OrbwalkerKuro timing",
    "Apply full damage for the first soldier and 25 percent for each additional soldier",
    "Apply 50 percent effectiveness to direct on-hit effects",
    "Use current W 50/65/80/95/110 rank damage",
    "Add eight W base damage per level from levels 10 through 18",
    "Use current W 35/42.5/50/57.5/65 percent AP ratios",
    "Use current W secondary line scaling from 20 percent to 100 percent",
    "Use the soldier magic damage model for last-hit and wait decisions",
    "Let OrbwalkerKuro issue attack and kite commands instead of the champion controller",
    "Never issue movement from AIAzir",
    "Never issue attack-move from AIAzir",
    "Yield spell windows while a target remains inside soldier coverage",
    "Use one W charge for lane farming and preserve the other for defense",
    "Use Q on a wave only when existing soldiers hit the configured body count",
    "Use separate W and Q thresholds for lane and jungle formations",
    "Hold all farm spells while an enemy champion contests the wave",
    "Delegate LastHit mode entirely to the soldier-aware orbwalker",
    "Never use E merely to farm",
    "Never use R merely to farm",
    "Prioritize formation value rather than generic circular farm prediction",
    "Use current E 70/110/150/190/230 damage plus 60 percent AP",
    "Use current E shield 70/110/150/190/230 plus 60 percent AP",
    "Use the current roughly 1700 E dash speed",
    "Select only a live soldier inside E's 1100 selection range",
    "Resolve E against the first enemy champion capsule on its path",
    "Stop E at the first collision rather than pretending Azir reaches the soldier",
    "Grant collision-kill intent only when the desired target is first",
    "Use champion collision deliberately to regain a W charge",
    "Reject collision-refund E without allied follow-up",
    "Reject offensive E during a valuable auto windup unless lethal",
    "Reject offensive E endpoints under a new enemy turret",
    "Reject E endpoints inside terrain",
    "Reject E endpoints inside ready point-click lockdown",
    "Reject E endpoints inside Poppy, Taliyah or Cassiopeia dash hazards",
    "Reject E paths through a live projectile-intercept wall",
    "Treat Azir as a projectile during E for Yasuo, Braum and Samira interactions",
    "Allow defensive E to a cursor-agreeing soldier away from a committed diver",
    "Use E's 1.5-second shield window as defense rather than fake invulnerability",
    "Model direct E and E-Q drift as separate endpoints",
    "Require Q ready before planning a drift redirect",
    "Buffer Q only after E begins and before the dash window closes",
    "Use the stored redirect rather than recomputing cursor geometry mid-dash",
    "Clear a drift sequence if Q is too late to extend the dash",
    "Build W-E-Q toward the player's cursor while fleeing",
    "Use direct E without spending Q when the safe anchor already suffices",
    "Use R peel as the final flee resource when no safe drift remains",
    "Use current R 200/400/600 plus 75 percent AP damage",
    "Use current R 120/105/90 second cooldown and 100 mana",
    "Use the 0.5-second R cast time",
    "Use six, seven and eight wall soldiers by rank",
    "Use current 700/810/930 wall lengths by rank",
    "Model R's 190-unit wall depth separately from wall length",
    "Test R bodies from 300 behind Azir through 600 forward",
    "Preserve each target's lateral offset in its R landing point",
    "Push a target roughly 650 units along R direction",
    "Score R by total hits, priority hits and allied follow-up",
    "Prefer R that pushes the primary target toward allied follow-up",
    "Prefer R that separates enemy front line from back line",
    "Use R as self-peel against a committed gapcloser by default",
    "Use R to interrupt a nearby committed channel",
    "Use R kill secure only from exact mitigated damage",
    "Do not consume an intact spell shield with ordinary offensive R",
    "Require at least the configured enemy count for ordinary offensive R",
    "Prefer front-to-back soldier DPS over a one-target automatic shuffle",
    "Keep automatic shuffle disabled by default",
    "Require W or a live anchor, E, Q and R before starting a shuffle",
    "Require the player's cursor to agree with shuffle direction",
    "Require allied follow-up before starting a shuffle",
    "Require an exit resource or overwhelming allied presence",
    "Reject shuffle endpoints under a new enemy turret",
    "Reject shuffle opportunities beside corrupting terrain geometry",
    "Reject single-target shuffle into ready Flash or dash without control spent",
    "Allow a multi-target window to justify expended enemy escape tools",
    "Let a manual shuffle override front-to-back preference but not safety",
    "Keep normal shuffle and revenant shuffle on separate manual keys",
    "Use the original Azir position as the revenant return anchor",
    "Wait at least 245 milliseconds after Q before issuing R",
    "Abort a shuffle when E collision or Q timing invalidates the R window",
    "Place a follow-up soldier on the target's R landing route",
    "Never automate Flash for E-Q-Flash-R or WEQFR",
    "Expose Flash extensions only as player-executed coaching",
    "Track enemy Flash and champion-specific mobility readiness",
    "Track when enemy mobility was just spent",
    "Track when key hard crowd control was just spent",
    "Track targeted and crossing line threats separately",
    "Use reactive R before a risky E when a diver is already in wall range",
    "Create a defensive W anchor when a threat commits and no soldier exists",
    "Respect manual Q ownership and never auto-complete it",
    "Respect manual W ownership and never dash to its soldier automatically",
    "Respect manual E ownership and never append Q or R automatically",
    "Respect manual R ownership and never append a follow-up sequence automatically",
    "Yield for a configurable window after every manual spell",
    "Track player and soldier auto completions as the same trade milestone",
    "Use player-selected target preference through the shared engine",
    "Use current passive 90-second cooldown and 45-second tower lifetime",
    "Use current Sun Disc 230 plus 15 per level from level seven plus 40 percent AP damage",
    "Use current Sun Disc 30 plus five per level from level seven bonus resists",
    "Track valid turret ruins without treating arbitrary rubble as a soldier",
    "Suggest Sun Disc only when the channel location is safe",
    "Suggest Sun Disc for objective, side-lane or base-defense strategic windows",
    "Reject Sun Disc advice when enemies can immediately erase the zone",
    "Keep passive ruin selection and channel click entirely player-owned",
    "Draw live soldier commandability and primary attack circles",
    "Draw every selected Q formation endpoint",
    "Draw direct E and buffered E-Q paths separately",
    "Draw R push direction and rank-scaled final wall",
    "Expose W ammo, active soldier count, posture, sequence and spell ownership",
    "Own Azir's complete spell decision loop without generic Q-W-E-R fallback",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Azir";
    controller.ControllerId = "champion.kuroaio.ai.azir.controller";
    controller.KitRevision = "Riot 26.14 / CommunityDragon PC 16.14";
    controller.ResearchArtifact = "AI/Research/AIAzir.md";
    controller.ImplementationSummary =
        "Soldier-aware W-AA-Q-AA and OrbwalkerKuro kiting, multi-soldier "
        "formation geometry, first-collision E-Q drift, peel-first R, "
        "safety-gated shuffle/revenant sequences and advisory-only Sun Disc.";
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

} // namespace Plugins::KuroAIO::AI::Controllers::Azir
