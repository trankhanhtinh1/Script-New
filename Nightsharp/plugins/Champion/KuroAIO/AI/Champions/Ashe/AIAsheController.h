#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIAsheGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Ashe {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::HasAnyBuff;
using ControllerHelpers::HasCurrentResource;
using ControllerHelpers::HasNearbyJungleTarget;
using ControllerHelpers::HasResourceFor;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocks;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContainsAny;
using ControllerHelpers::SpellRank;
using ControllerHelpers::UnitByNetworkId;
using ControllerHelpers::ValidHostileUnitInGameplayRange;

enum class Sequence : std::uint8_t {
    None,
    VolleyAutoPoke,
    AutoFocusReset,
    VolleyArrowCatch,
    ArrowVolleyFollowup,
    SelfPeel,
    AllyPeel,
    Interrupt,
    HawkFirstClear,
    HawkJunglerTrack,
    HawkObjective,
    HawkNoFacecheck,
    HawkChargeCap,
    FarmFan,
    JungleFlurry,
    StructureFlurry,
    PlayerLed,
};

enum class Posture : std::uint8_t {
    Neutral,
    LanePoke,
    ExtendedTrade,
    Chase,
    Kite,
    Peel,
    Teamfight,
    Pick,
    Objective,
    Scout,
    Siege,
    Flee,
};

enum class QPurpose : std::uint8_t {
    None,
    ResetTrade,
    AllIn,
    Lethal,
    Waveclear,
    Jungle,
    Objective,
    Structure,
};

enum class WPurpose : std::uint8_t {
    None,
    Poke,
    ThreadWave,
    Chase,
    Peel,
    ConfirmArrow,
    FollowArrow,
    KillSecure,
    Waveclear,
    Jungle,
};

enum class ScoutPurpose : std::uint8_t {
    None,
    Manual,
    FirstClear,
    LastSeenJungler,
    Objective,
    NoFacecheck,
    AntiGank,
    ChargeCap,
};

enum class ArrowPurpose : std::uint8_t {
    None,
    Manual,
    SelfPeel,
    AllyPeel,
    Interrupt,
    Pick,
    Teamfight,
    CrossMap,
    KillSecure,
    FollowVolley,
};

struct FrostRecord {
    int NetworkId = 0;
    int ExpireTick = 0;
    int AppliedTick = 0;
    bool Confirmed = false;
};

struct EnemyTrack {
    int NetworkId = 0;
    Vector3 LastPosition = {};
    Vector3 LastPathEnd = {};
    int LastVisibleTick = 0;
    int LastUpdateTick = 0;
    bool Jungler = false;
};

struct ScoutMemory {
    int LandmarkId = 0;
    int LastScoutTick = 0;
};

struct VolleyPlan {
    Vector3 Aim = {};
    Vector3 PrimaryPosition = {};
    VolleyEvaluation Evaluation = {};
    WPurpose Purpose = WPurpose::None;
    int PrimaryId = 0;
    int ImpactTick = 0;
    float Score = -FLT_MAX;
    bool Threaded = false;
    bool ProjectileBlocked = false;
    bool Valid = false;
};

struct ScoutPlan {
    Vector3 Destination = {};
    ScoutEvaluation Evaluation = {};
    ScoutPurpose Purpose = ScoutPurpose::None;
    int PrimaryLandmarkId = 0;
    int JunglerId = 0;
    float Score = -FLT_MAX;
    bool PreservesCharge = false;
    bool Valid = false;
};

struct ArrowPlan {
    Vector3 Aim = {};
    Vector3 FirstHitPosition = {};
    ArrowEvaluation Evaluation = {};
    ArrowPurpose Purpose = ArrowPurpose::None;
    int RequestedTargetId = 0;
    int FirstHitId = 0;
    int ImpactTick = 0;
    float TravelSeconds = 0.0f;
    float Alignment = 0.0f;
    float Score = -FLT_MAX;
    bool CrossMap = false;
    bool ProjectileBlocked = false;
    bool Valid = false;
};

struct LandmarkDefinition {
    Vector3 Position = {};
    float Weight = 0.0f;
    int Id = 0;
    ScoutKind Kind = ScoutKind::Camp;
    bool ChaosSide = false;
    const char* Name = "";
};

inline const std::array<LandmarkDefinition, 20> SummonersRiftLandmarks = {{
    { { 3870.0f, 0.0f, 7900.0f }, 2.4f, 101, ScoutKind::Camp, false, "order blue" },
    { { 2100.0f, 0.0f, 8400.0f }, 2.0f, 102, ScoutKind::Camp, false, "order gromp" },
    { { 3800.0f, 0.0f, 6500.0f }, 2.0f, 103, ScoutKind::Camp, false, "order wolves" },
    { { 6500.0f, 0.0f, 5500.0f }, 2.1f, 104, ScoutKind::Camp, false, "order raptors" },
    { { 7800.0f, 0.0f, 4000.0f }, 2.4f, 105, ScoutKind::Camp, false, "order red" },
    { { 8400.0f, 0.0f, 2700.0f }, 1.8f, 106, ScoutKind::Camp, false, "order krugs" },
    { { 10900.0f, 0.0f, 7000.0f }, 2.4f, 201, ScoutKind::Camp, true, "chaos blue" },
    { { 12700.0f, 0.0f, 6500.0f }, 2.0f, 202, ScoutKind::Camp, true, "chaos gromp" },
    { { 11000.0f, 0.0f, 8400.0f }, 2.0f, 203, ScoutKind::Camp, true, "chaos wolves" },
    { { 7800.0f, 0.0f, 9300.0f }, 2.1f, 204, ScoutKind::Camp, true, "chaos raptors" },
    { { 7000.0f, 0.0f, 11000.0f }, 2.4f, 205, ScoutKind::Camp, true, "chaos red" },
    { { 6200.0f, 0.0f, 12200.0f }, 1.8f, 206, ScoutKind::Camp, true, "chaos krugs" },
    { { 9866.0f, 0.0f, 4414.0f }, 3.4f, 301, ScoutKind::Objective, false, "dragon" },
    { { 5007.0f, 0.0f, 10471.0f }, 3.4f, 302, ScoutKind::Objective, true, "baron/herald" },
    { { 7400.0f, 0.0f, 5200.0f }, 1.7f, 401, ScoutKind::River, false, "lower river" },
    { { 6500.0f, 0.0f, 9700.0f }, 1.7f, 402, ScoutKind::River, true, "upper river" },
    { { 9050.0f, 0.0f, 5650.0f }, 1.6f, 501, ScoutKind::GankRoute, true, "chaos lower entrance" },
    { { 5700.0f, 0.0f, 8950.0f }, 1.6f, 502, ScoutKind::GankRoute, false, "order upper entrance" },
    { { 10500.0f, 0.0f, 3100.0f }, 1.4f, 601, ScoutKind::Brush, true, "lower tri" },
    { { 4300.0f, 0.0f, 11900.0f }, 1.4f, 602, ScoutKind::Brush, false, "upper tri" },
}};

inline Menu* TacticsMenu = nullptr;
inline Menu* FocusMenu = nullptr;
inline Menu* VolleyMenu = nullptr;
inline Menu* HawkshotMenu = nullptr;
inline Menu* ArrowMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Sequence ActiveSequence = Sequence::None;
inline Posture CurrentPosture = Posture::Neutral;
inline QPurpose LastQPurpose = QPurpose::None;
inline WPurpose LastWPurpose = WPurpose::None;
inline ScoutPurpose LastScoutPurpose = ScoutPurpose::None;
inline ArrowPurpose LastArrowPurpose = ArrowPurpose::None;
inline Mode LastKnownMode = Mode::None;

inline int FocusStacks = 0;
inline bool FocusReadyConfirmed = false;
inline bool FocusActive = false;
inline int FocusActiveUntil = 0;
inline int LastFocusObservationTick = 0;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastAutoTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastCombatTick = 0;
inline int EAmmo = 0;
inline int EMaxAmmo = 2;
inline bool EAmmoObserved = false;
inline int LastEAmmoObservationTick = 0;
inline int RMissileNetworkId = 0;
inline Vector3 RMissilePosition = {};
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int IncomingThreatTargetId = 0;
inline int IncomingThreatUntil = 0;
inline bool IncomingHardCrowdControl = false;
inline float RecentIncomingPressure = 0.0f;
inline int ProtectedAllyId = 0;
inline int PeelThreatId = 0;
inline int LastScoutDecisionTick = 0;
inline int LastManualScoutTick = 0;
inline int LastManualArrowTick = 0;

inline std::array<FrostRecord, 24> FrostedTargets = {};
inline std::array<EnemyTrack, 20> EnemyTracks = {};
inline std::array<ScoutMemory, SummonersRiftLandmarks.size()> ScoutHistory = {};
inline VolleyPlan LastVolleyPlan = {};
inline ScoutPlan LastScoutPlan = {};
inline ArrowPlan LastArrowPlan = {};

inline bool IsFocusReadyBuff(const char* name) {
    return name && (Engine::TextContains(name, "asheqcastready") ||
                    Engine::TextContains(name, "asheqready"));
}

inline bool IsFocusActiveBuff(const char* name) {
    return name && (Engine::TextContains(name, "asheqbuff") ||
                    Engine::TextContains(name, "rangersfocus"));
}

inline bool IsFrostBuff(const char* name) {
    return name && (Engine::TextContains(name, "ashepassiveslow") ||
                    Engine::TextContains(name, "ashepassive"));
}

inline bool IsVolleyEvent(
    const SDK::Events::ProcessSpellEventArgs& args) {
    return args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
           SpellEventNameContainsAny(args, { "volley", "ashecone" });
}

inline bool IsHawkshotEvent(
    const SDK::Events::ProcessSpellEventArgs& args) {
    return args.Slot == static_cast<int>(SDK::SpellSlot::E) ||
           SpellEventNameContainsAny(args, { "hawkshot", "asheespell" });
}

inline bool IsArrowEvent(
    const SDK::Events::ProcessSpellEventArgs& args) {
    return args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
           SpellEventNameContainsAny(
               args, { "enchantedcrystalarrow", "asher" });
}

inline bool IsArrowMissileName(const char* spellName,
                               const char* missileName) {
    return ControllerHelpers::AnyTextContains(
        { spellName, missileName },
        { "enchantedcrystalarrow", "asher" });
}

inline FrostRecord* FindFrost(int networkId, bool create = false) {
    if (networkId == 0) return nullptr;
    FrostRecord* empty = nullptr;
    for (auto& record : FrostedTargets) {
        if (record.NetworkId == networkId) return &record;
        if (!empty && record.NetworkId == 0) empty = &record;
    }
    if (!create) return nullptr;
    if (!empty) {
        empty = &*std::min_element(
            FrostedTargets.begin(), FrostedTargets.end(),
            [](const FrostRecord& left, const FrostRecord& right) {
                return left.ExpireTick < right.ExpireTick;
            });
    }
    *empty = {};
    empty->NetworkId = networkId;
    return empty;
}

inline void SetFrost(int networkId,
                     int expireTick,
                     bool confirmed) {
    FrostRecord* record = FindFrost(networkId, true);
    if (!record) return;
    record->AppliedTick = Now();
    record->ExpireTick = std::max(record->ExpireTick, expireTick);
    record->Confirmed = record->Confirmed || confirmed;
}

inline bool IsFrosted(const AIBaseClient& target) {
    if (!target.IsValid()) return false;
    if (HasAnyBuff(target, { "ashepassiveslow", "AshePassiveSlow" })) {
        return true;
    }
    const FrostRecord* record = FindFrost(
        static_cast<int>(target.NetworkId()));
    return record && record->ExpireTick >= Now();
}

inline void RefreshFrost() {
    const int now = Now();
    for (auto& record : FrostedTargets) {
        if (record.NetworkId != 0 && record.ExpireTick < now) record = {};
    }
}

inline EnemyTrack* FindEnemyTrack(int networkId, bool create = false) {
    if (networkId == 0) return nullptr;
    EnemyTrack* empty = nullptr;
    for (auto& track : EnemyTracks) {
        if (track.NetworkId == networkId) return &track;
        if (!empty && track.NetworkId == 0) empty = &track;
    }
    if (!create) return nullptr;
    if (!empty) {
        empty = &*std::min_element(
            EnemyTracks.begin(), EnemyTracks.end(),
            [](const EnemyTrack& left, const EnemyTrack& right) {
                return left.LastUpdateTick < right.LastUpdateTick;
            });
    }
    *empty = {};
    empty->NetworkId = networkId;
    return empty;
}

inline void RefreshEnemyTracks() {
    const int now = Now();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid()) continue;
        EnemyTrack* track = FindEnemyTrack(
            static_cast<int>(enemy.NetworkId()), true);
        if (!track) continue;
        track->LastUpdateTick = now;
        track->Jungler = track->Jungler ||
            ControllerHelpers::HeroHasSmite(enemy);
        if (enemy.IsVisible() && !enemy.IsDead()) {
            track->LastPosition = enemy.Position();
            track->LastPathEnd = enemy.PathEnd();
            track->LastVisibleTick = now;
        }
    }
}

inline const EnemyTrack* RecentJunglerTrack(int minimumMissingMs = 1500,
                                            int maximumMissingMs = 30000) {
    const int now = Now();
    const EnemyTrack* best = nullptr;
    for (const auto& track : EnemyTracks) {
        if (!track.Jungler || track.NetworkId == 0 ||
            track.LastVisibleTick <= 0) {
            continue;
        }
        const int missing = now - track.LastVisibleTick;
        if (missing < minimumMissingMs || missing > maximumMissingMs) continue;
        if (!best || track.LastVisibleTick > best->LastVisibleTick) best = &track;
    }
    return best;
}

inline int ObservedHawkshotAmmo() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return -1;
    const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::E);
    if (!spell.IsValid()) return -1;
    const int maximum = spell.MaxAmmo();
    const int ammo = spell.Ammo();
    if (maximum < 1 || maximum > 2 || ammo < 0 || ammo > maximum) return -1;
    EMaxAmmo = maximum;
    return ammo;
}

inline void RefreshFocusState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const bool liveActive = HasAnyBuff(
        player, { "AsheQBuff", "asheqbuff", "RangersFocus" });
    const bool liveReady = HasAnyBuff(
        player, { "asheqcastready", "AsheQCastReady" });
    const int liveCount = ControllerHelpers::MaximumBuffCount(
        player,
        { "asheqcastready", "AsheQCastReady", "asheqstacks" });
    if (liveActive) {
        FocusActive = true;
        FocusActiveUntil = std::max(FocusActiveUntil, Now() + 120);
        LastFocusObservationTick = Now();
    } else if (FocusActiveUntil < Now()) {
        FocusActive = false;
    }
    if (liveReady) {
        FocusStacks = 4;
        FocusReadyConfirmed = true;
        LastFocusObservationTick = Now();
    } else if (liveCount > 0) {
        FocusStacks = std::clamp(liveCount, 0, 4);
        FocusReadyConfirmed = FocusStacks >= 4;
        LastFocusObservationTick = Now();
    } else if (FocusReadyConfirmed && Now() - LastFocusObservationTick > 250) {
        FocusReadyConfirmed = false;
    }
}

inline void RefreshHawkshotAmmo() {
    const int observed = ObservedHawkshotAmmo();
    if (observed >= 0) {
        EAmmo = observed;
        EAmmoObserved = true;
        LastEAmmoObservationTick = Now();
    } else if (!EAmmoObserved) {
        EAmmo = Ready(2) ? 1 : 0;
    }
}

inline void RefreshState() {
    RefreshFrost();
    RefreshFocusState();
    RefreshHawkshotAmmo();
    RefreshEnemyTracks();
    if (IncomingThreatUntil < Now()) {
        IncomingHardCrowdControl = false;
        RecentIncomingPressure = std::max(0.0f, RecentIncomingPressure - 0.08f);
    }
    if (GapcloserExpireTick < Now()) GapcloserTargetId = 0;
    if (InterruptExpireTick < Now()) InterruptTargetId = 0;
}

inline float VolleyDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return player.CalculatePhysicalDamage(
        target, VolleyRawDamage(SpellRank(1), player.BonusAttackDamage()));
}

inline float ArrowDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return player.CalculateMagicDamage(
        target, ArrowRawDamage(SpellRank(3), player.AP()));
}

inline float AutoDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid()
        ? SDK::Damage::GetAutoAttackDamage(player, target, true)
        : 0.0f;
}

inline bool IsPerHitFlatReductionTarget(const AIBaseClient& target) {
    if (!target.IsValid() || !target.IsHero()) return false;
    const SDK::ChampionId championId =
        SDK::ChampionIdFromName(target.CharacterName().c_str());
    if (championId == SDK::ChampionId::Amumu ||
        championId == SDK::ChampionId::Fizz) {
        return true;
    }
    if (championId == SDK::ChampionId::Leona &&
        HasAnyBuff(target, { "LeonaSolarBarrier", "leonaw" })) {
        return true;
    }
    return false;
}

inline bool TargetLeavingAutoRange(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid() ||
        !target.PathEnd().IsValid() || target.PathEnd().IsZero()) {
        return false;
    }
    return player.Position().Distance2D(target.PathEnd()) >
           player.Position().Distance2D(target.Position()) + 65.0f;
}

inline int CountRemainingWaveAttacks() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    int attacks = 0;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!ValidHostileUnitInGameplayRange(minion, 760.0f)) continue;
        attacks += minion.Health() > AutoDamage(minion) * 1.10f ? 2 : 1;
        if (attacks >= 6) return attacks;
    }
    return attacks;
}

inline int ExpectedFollowupAttacks(const AIBaseClient& target, Mode mode) {
    if (!target.IsValid()) return 0;
    if (target.IsTurret() || (!target.IsHero() && !target.IsMinion())) return 6;
    if (target.Team() == SDK::GameObjectTeam::Neutral) {
        const float damage = std::max(1.0f, AutoDamage(target));
        return std::clamp(static_cast<int>(std::ceil(target.Health() / damage)),
                          0, 8);
    }
    if (target.IsMinion()) return CountRemainingWaveAttacks();
    const float distance = GameObjects::Player().Position().Distance2D(
        target.Position());
    int expected = mode == Mode::Combo ? 3 : 2;
    if (IsFrosted(target)) ++expected;
    if (distance > ControllerHelpers::AutoAttackRange(target, -10.0f)) --expected;
    if (TargetLeavingAutoRange(target) && !IsFrosted(target)) --expected;
    if (Engine::IsHardCrowdControlled(target)) expected += 2;
    return std::clamp(expected, 0, 6);
}

inline QPurpose FocusPurposeFor(const AIBaseClient& target, Mode mode) {
    if (!target.IsValid()) return QPurpose::None;
    if (target.IsHero()) {
        const float flurry = AutoDamage(target) *
            std::max(1.0f, QFlurryAttackRatio(SpellRank(0)));
        if (flurry >= target.Health() + target.AllShield()) {
            return QPurpose::Lethal;
        }
        return mode == Mode::Combo ? QPurpose::AllIn
                                   : QPurpose::ResetTrade;
    }
    if (target.IsTurret() || (!target.IsHero() && !target.IsMinion())) {
        return QPurpose::Structure;
    }
    if (target.Team() == SDK::GameObjectTeam::Neutral) {
        return IsEpicMonster(target) ? QPurpose::Objective
                                     : QPurpose::Jungle;
    }
    return QPurpose::Waveclear;
}

inline bool FocusPurposeEnabled(QPurpose purpose) {
    switch (purpose) {
    case QPurpose::ResetTrade: return Bool(FocusMenu, "Harass", true);
    case QPurpose::AllIn:
    case QPurpose::Lethal: return Bool(FocusMenu, "Combo", true);
    case QPurpose::Waveclear: return Bool(FocusMenu, "Wave", true);
    case QPurpose::Jungle: return Bool(FocusMenu, "Jungle", true);
    case QPurpose::Objective: return Bool(FocusMenu, "Objective", true);
    case QPurpose::Structure: return Bool(FocusMenu, "Structures", true);
    default: return false;
    }
}

inline bool TryFocusReset(Mode requestedMode) {
    if (!Ready(0) || FocusActive ||
        Now() - LastAfterAttackTick > 210 || LastAfterAttackTargetId == 0 ||
        !Engine::CanAct(false) || !CastThrottleReady(0, 0, 0)) {
        return false;
    }
    const AIBaseClient target = UnitByNetworkId(LastAfterAttackTargetId);
    if (!target.IsValid() || target.IsDead() || !target.IsEnemy()) return false;
    const Mode mode = requestedMode == Mode::None
        ? (target.IsHero() ? Mode::Automatic : LastKnownMode)
        : requestedMode;
    if (!SpellEnabled(0, mode)) return false;
    const QPurpose purpose = FocusPurposeFor(target, mode);
    if (!FocusPurposeEnabled(purpose)) return false;
    if (purpose == QPurpose::Waveclear &&
        ControllerHelpers::PlayerManaPercent() < Slider(FarmMenu, "QWaveMana", 42)) {
        return false;
    }
    if ((purpose == QPurpose::Jungle || purpose == QPurpose::Objective) &&
        ControllerHelpers::PlayerManaPercent() < Slider(FarmMenu, "QJungleMana", 24)) {
        return false;
    }
    const int followups = ExpectedFollowupAttacks(target, mode);
    const float manaAfter = GameObjects::Player().Mana() - SpellCost(0);
    const float flurry = AutoDamage(target) *
        std::max(1.0f, QFlurryAttackRatio(SpellRank(0)));
    FocusContext context{};
    context.FocusStacks = FocusReadyConfirmed ? 4 : FocusStacks;
    context.ExpectedFollowupAttacks = followups;
    context.ManaAfterCast = manaAfter;
    context.CastReady = FocusReadyConfirmed || FocusStacks >= 4;
    context.AlreadyActive = FocusActive;
    context.JustAttacked = true;
    context.ChampionTarget = target.IsHero();
    context.EpicMonster = target.Team() == SDK::GameObjectTeam::Neutral &&
                          IsEpicMonster(target);
    context.JungleTarget = target.Team() == SDK::GameObjectTeam::Neutral;
    context.WaveTarget = target.IsMinion() &&
        target.Team() != SDK::GameObjectTeam::Neutral;
    context.StructureTarget = target.IsTurret() ||
        (!target.IsHero() && !target.IsMinion());
    context.TargetFrosted = IsFrosted(target);
    context.TargetLeavingRange = TargetLeavingAutoRange(target);
    context.TargetHasPerHitFlatReduction =
        IsPerHitFlatReductionTarget(target) &&
        Bool(FocusMenu, "RespectFlatReduction", true);
    context.LethalWindow = flurry >= target.Health() + target.AllShield();
    if (!ShouldActivateFocus(context)) return false;
    if (Engine::ControllerCastSelf(0)) {
        LastQCastTick = Now();
        LastQPurpose = purpose;
        FocusStacks = 0;
        FocusReadyConfirmed = false;
        FocusActive = true;
        FocusActiveUntil = Now() + 4000;
        switch (purpose) {
        case QPurpose::Waveclear: ActiveSequence = Sequence::FarmFan; break;
        case QPurpose::Jungle:
        case QPurpose::Objective: ActiveSequence = Sequence::JungleFlurry; break;
        case QPurpose::Structure: ActiveSequence = Sequence::StructureFlurry; break;
        default: ActiveSequence = Sequence::AutoFocusReset; break;
        }
        return true;
    }
    return false;
}

inline Vector3 PredictedVolleyPosition(const AIBaseClient& unit) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !unit.IsValid()) return {};
    float distance = player.Position().Distance2D(unit.Position());
    Vector3 predicted = PredictPosition(unit, VolleyImpactSeconds(distance));
    distance = player.Position().Distance2D(predicted);
    return PredictPosition(unit, VolleyImpactSeconds(distance));
}

inline std::vector<VolleyUnit> CollectVolleyUnits(int primaryId,
                                                  bool includeFarm) {
    std::vector<VolleyUnit> units;
    units.reserve(80);
    auto append = [&](const AIBaseClient& unit,
                      bool champion,
                      bool minion) -> bool {
        if (!ValidHostileUnitInGameplayRange(unit, kVolleyRange + 50.0f)) {
            return false;
        }
        const Vector3 predicted = PredictedVolleyPosition(unit);
        if (!predicted.IsValid() || predicted.IsZero()) return false;
        const int id = static_cast<int>(unit.NetworkId());
        float weight = champion ? 4.2f : 0.8f;
        if (id == primaryId) weight += 5.5f;
        if (champion && (unit.TotalAttackDamage() >= 180.0f ||
                         unit.AP() >= 220.0f)) {
            weight += 1.3f;
        }
        const bool killable = champion &&
            VolleyDamage(unit) >= unit.Health() + unit.AllShield();
        units.push_back({
            predicted,
            std::max(20.0f, unit.BoundingRadius()),
            weight,
            unit.Health(),
            id,
            champion,
            minion,
            id == primaryId,
            Engine::IsHardCrowdControlled(unit),
            killable,
            true,
        });
        return true;
    };
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy, kVolleyRange + 80.0f)) {
            append(enemy, true, false);
        }
    }
    // Every hostile unit is relevant as a first-ray blocker even when this is
    // a champion-only cast. Their score stays low unless farm value is wanted.
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidHostileUnitInGameplayRange(minion, kVolleyRange + 30.0f)) {
            if (append(minion, false, true) && !includeFarm) {
                units.back().Weight = 0.12f;
            }
        }
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (ValidHostileUnitInGameplayRange(monster, kVolleyRange + 30.0f)) {
            const bool appended = append(monster, false, true);
            if (appended && !includeFarm) units.back().Weight = 0.10f;
            else if (appended && IsEpicMonster(monster)) {
                units.back().Weight += 4.0f;
            }
        }
    }
    return units;
}

inline bool VolleyPrimaryPredictionAcceptable(const AIHeroClient& primary,
                                              WPurpose purpose) {
    if (!Engine::ValidEnemy(primary)) return purpose == WPurpose::Waveclear ||
                                           purpose == WPurpose::Jungle;
    if (Engine::IsHardCrowdControlled(primary) || primary.IsDashing() ||
        purpose == WPurpose::Peel || purpose == WPurpose::FollowArrow) {
        return true;
    }
    if (!Engine::RuntimeSpells[1]) return false;
    const auto prediction = Engine::RuntimeSpells[1]->GetPrediction(primary);
    const SDK::HitChance required = purpose == WPurpose::KillSecure
        ? SDK::HitChance::VeryHigh : SDK::HitChance::High;
    return ControllerHelpers::PredictionAtLeast(prediction, required);
}

inline VolleyPlan BuildVolleyPlan(const AIHeroClient& primary,
                                  WPurpose purpose,
                                  bool includeFarm) {
    VolleyPlan best{};
    const auto player = GameObjects::Player();
    const int rank = SpellRank(1);
    const int primaryId = primary.IsValid()
        ? static_cast<int>(primary.NetworkId()) : 0;
    if (!player.IsValid() || rank <= 0 ||
        (primaryId != 0 && !VolleyPrimaryPredictionAcceptable(primary, purpose))) {
        return best;
    }
    const auto units = CollectVolleyUnits(primaryId, includeFarm);
    if (units.empty()) return best;
    std::vector<Vector3> candidates;
    candidates.reserve(units.size() * 11);
    for (const auto& unit : units) {
        const Vector3 toUnit = SharedGeometry::Direction2D(
            player.Position(), unit.Position);
        if (toUnit.IsZero()) continue;
        for (int ray = 0; ray < VolleyArrowCount(rank); ++ray) {
            const float center = static_cast<float>(VolleyArrowCount(rank) - 1) * 0.5f;
            const float angle = (static_cast<float>(ray) - center) *
                                kVolleyRayStepRadians;
            candidates.push_back(SharedGeometry::Rotate2D(toUnit, -angle));
        }
    }
    for (const Vector3& direction : candidates) {
        if (direction.IsZero()) continue;
        VolleyEvaluation evaluation = EvaluateVolley(
            player.Position(), direction, rank, units, primaryId);
        if (!evaluation.Valid || (primaryId != 0 && !evaluation.HitsPrimary)) {
            continue;
        }
        const int centerRay = VolleyArrowCount(rank) / 2;
        const bool threaded = evaluation.HitsPrimary &&
                              evaluation.PrimaryRay != centerRay;
        float score = evaluation.Score * 100.0f;
        if (threaded) score += 85.0f;
        if (purpose == WPurpose::Peel) score += 220.0f;
        if (purpose == WPurpose::KillSecure) score += 310.0f;
        if (purpose == WPurpose::FollowArrow) score += 180.0f;
        if (purpose == WPurpose::Waveclear || purpose == WPurpose::Jungle) {
            score += evaluation.MinionHits * 90.0f;
        }
        if (evaluation.ChampionHits >= 2) score += 150.0f;
        const Vector3 hitDirection = evaluation.PrimaryRay >= 0
            ? VolleyRayDirection(direction, rank, evaluation.PrimaryRay)
            : direction;
        const float wallDistance = evaluation.PrimaryAlong < FLT_MAX
            ? std::min(kVolleyRange, evaluation.PrimaryAlong + 65.0f)
            : kVolleyRange;
        const Vector3 wallEnd = player.Position() + hitDirection * wallDistance;
        const bool blocked = ProjectileWallBlocks(
            player.Position(), wallEnd, kVolleyMissileRadius);
        if (blocked) continue;
        if (!best.Valid || score > best.Score) {
            best.Aim = player.Position() + direction * kVolleyRange;
            best.PrimaryPosition = primaryId != 0
                ? (ControllerHelpers::FindValidRecordById(units, primaryId)
                    ? ControllerHelpers::FindValidRecordById(
                          units, primaryId)->Position : Vector3{})
                : Vector3{};
            best.Evaluation = evaluation;
            best.Purpose = purpose;
            best.PrimaryId = primaryId;
            best.ImpactTick = Now() + static_cast<int>(std::ceil(
                VolleyImpactSeconds(evaluation.PrimaryAlong < FLT_MAX
                    ? evaluation.PrimaryAlong : 700.0f) * 1000.0f));
            best.Score = score;
            best.Threaded = threaded;
            best.ProjectileBlocked = false;
            best.Valid = true;
        }
    }
    return best;
}

inline VolleyPlan BuildFarmVolley(Mode mode) {
    const WPurpose purpose = mode == Mode::Jungle
        ? WPurpose::Jungle : WPurpose::Waveclear;
    return BuildVolleyPlan({}, purpose, true);
}

inline bool CastVolley(const VolleyPlan& plan,
                       Mode mode,
                       bool reactive = false) {
    if (!plan.Valid || !Ready(1) || !SpellEnabled(1, mode) ||
        !Engine::CanAct(reactive) || !CastThrottleReady(1, reactive) ||
        !HasCurrentResource(SpellCost(1))) {
        return false;
    }
    if (!reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true)) {
        return false;
    }
    if (Engine::ControllerCastPosition(1, plan.Aim)) {
        LastWCastTick = Now();
        LastWPurpose = plan.Purpose;
        LastVolleyPlan = plan;
        const int ping = std::clamp(static_cast<int>(SDK::Game::Ping()), 0, 100);
        Orbwalker::SetAttackPauseTime(250 + ping);
        if (plan.PrimaryId != 0) {
            SetFrost(plan.PrimaryId, plan.ImpactTick + 2000, false);
        }
        switch (plan.Purpose) {
        case WPurpose::Poke:
        case WPurpose::ThreadWave: ActiveSequence = Sequence::VolleyAutoPoke; break;
        case WPurpose::ConfirmArrow: ActiveSequence = Sequence::VolleyArrowCatch; break;
        case WPurpose::FollowArrow: ActiveSequence = Sequence::ArrowVolleyFollowup; break;
        case WPurpose::Peel: ActiveSequence = Sequence::SelfPeel; break;
        case WPurpose::Waveclear:
        case WPurpose::Jungle: ActiveSequence = Sequence::FarmFan; break;
        default: break;
        }
        return true;
    }
    return false;
}

inline Vector3 IterativeArrowPrediction(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return {};
    Vector3 predicted = target.IsDashing() && target.PathEnd().IsValid()
        ? target.PathEnd() : target.Position();
    for (int i = 0; i < 3; ++i) {
        const float distance = player.Position().Distance2D(predicted);
        predicted = PredictPosition(target, ArrowTravelSeconds(distance));
    }
    return predicted;
}

inline std::vector<ArrowUnit> CollectArrowUnits(int primaryId) {
    std::vector<ArrowUnit> units;
    units.reserve(GameObjects::EnemyHeroes().size());
    const auto player = GameObjects::Player();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) || IsCommonUntargetableOrImmune(enemy)) {
            continue;
        }
        const Vector3 predicted = IterativeArrowPrediction(enemy);
        if (!predicted.IsValid() || predicted.IsZero()) continue;
        const int id = static_cast<int>(enemy.NetworkId());
        const bool threat = enemy.TotalAttackDamage() >= 185.0f ||
                            enemy.AP() >= 240.0f ||
                            enemy.Position().Distance2D(player.Position()) <= 650.0f;
        const bool killable = ArrowDamage(enemy) >=
                              enemy.Health() + enemy.AllShield();
        float weight = 3.0f + (100.0f - enemy.HealthPercent()) * 0.025f;
        if (id == primaryId) weight += 4.5f;
        if (threat) weight += 1.4f;
        units.push_back({
            predicted,
            std::max(25.0f, enemy.BoundingRadius()),
            weight,
            id,
            CountAlliedFollowup(predicted, 850.0f, false),
            id == primaryId,
            Engine::IsHardCrowdControlled(enemy),
            enemy.IsDashing(),
            killable,
            threat,
            true,
        });
    }
    return units;
}

inline ArrowPlan BuildArrowPlan(const AIHeroClient& requested,
                                ArrowPurpose purpose) {
    ArrowPlan best{};
    const auto player = GameObjects::Player();
    const int requestedId = requested.IsValid()
        ? static_cast<int>(requested.NetworkId()) : 0;
    if (!player.IsValid() || SpellRank(3) <= 0) return best;
    const auto units = CollectArrowUnits(requestedId);
    if (units.empty()) return best;
    for (const auto& anchor : units) {
        const Vector3 direction = SharedGeometry::Direction2D(
            player.Position(), anchor.Position);
        if (direction.IsZero()) continue;
        ArrowEvaluation evaluation = EvaluateArrowLine(
            player.Position(), direction, units, requestedId);
        if (!evaluation.Valid) continue;
        if (requestedId != 0 &&
            purpose != ArrowPurpose::Teamfight &&
            evaluation.FirstHitId != requestedId) {
            continue;
        }
        const ArrowUnit* first = ControllerHelpers::FindValidRecordById(
            units, evaluation.FirstHitId);
        if (!first) continue;
        const AIHeroClient firstHero = HeroByNetworkId(evaluation.FirstHitId);
        if (!Engine::ValidEnemy(firstHero) ||
            (HasSpellShieldOrImmunity(firstHero) &&
             purpose != ArrowPurpose::Teamfight)) {
            continue;
        }
        const float travel = ArrowTravelSeconds(evaluation.FirstHitDistance);
        const Vector3 movement = firstHero.PathEnd().IsValid()
            ? SharedGeometry::Direction2D(firstHero.Position(), firstHero.PathEnd())
            : Vector3{};
        const float alignment = ArrowPathAlignment(direction, movement);
        float score = evaluation.Score * 100.0f + alignment * 115.0f;
        if (IsFrosted(firstHero)) score += 55.0f;
        if (firstHero.IsDashing()) score += 65.0f;
        if (Engine::IsHardCrowdControlled(firstHero)) score += 160.0f;
        if (purpose == ArrowPurpose::SelfPeel ||
            purpose == ArrowPurpose::AllyPeel ||
            purpose == ArrowPurpose::Interrupt) score += 360.0f;
        if (purpose == ArrowPurpose::KillSecure) score += 260.0f;
        if (purpose == ArrowPurpose::FollowVolley) score += 175.0f;
        const Vector3 wallEnd = evaluation.FirstHitPosition;
        const bool blocked = ProjectileWallBlocksFromPlayer(
            wallEnd, kArrowRadius);
        if (blocked) continue;
        if (!best.Valid || score > best.Score) {
            best.Aim = player.Position() + direction *
                std::min(kArrowRange,
                         std::max(1000.0f, evaluation.FirstHitDistance + 250.0f));
            best.FirstHitPosition = evaluation.FirstHitPosition;
            best.Evaluation = evaluation;
            best.Purpose = purpose;
            best.RequestedTargetId = requestedId;
            best.FirstHitId = evaluation.FirstHitId;
            best.TravelSeconds = travel;
            best.ImpactTick = Now() + static_cast<int>(std::ceil(travel * 1000.0f));
            best.Alignment = alignment;
            best.Score = score;
            best.CrossMap = evaluation.FirstHitDistance > 2500.0f;
            best.ProjectileBlocked = false;
            best.Valid = true;
        }
    }
    return best;
}

inline bool ArrowTargetReliable(const AIHeroClient& target,
                                const ArrowPlan& plan,
                                float movingAlignment) {
    if (!Engine::ValidEnemy(target)) return false;
    const bool stationary = !target.PathEnd().IsValid() ||
        target.PathEnd().IsZero() ||
        target.Position().Distance2D(target.PathEnd()) <= 45.0f;
    return stationary || Engine::IsHardCrowdControlled(target) ||
           target.IsDashing() || plan.Alignment >= movingAlignment ||
           (IsFrosted(target) && plan.Alignment >= movingAlignment - 0.18f);
}

inline bool ArrowPlanMeetsPurpose(const ArrowPlan& plan,
                                 const AIHeroClient& target) {
    if (!plan.Valid || !Engine::ValidEnemy(target)) return false;
    const float distance = plan.Evaluation.FirstHitDistance;
    const int follow = plan.Evaluation.AlliedFollowup +
        (distance <= 950.0f ? 1 : 0);
    switch (plan.Purpose) {
    case ArrowPurpose::SelfPeel:
        return distance <= 780.0f &&
            (target.IsDashing() || distance <= 460.0f ||
             GameObjects::Player().HealthPercent() <=
                Slider(ArrowMenu, "SelfPeelHp", 52));
    case ArrowPurpose::AllyPeel:
        return distance <= 1350.0f && plan.Evaluation.FirstHitThreat;
    case ArrowPurpose::Interrupt:
        return plan.ImpactTick <= InterruptExpireTick + 90;
    case ArrowPurpose::KillSecure:
        return plan.Evaluation.FirstHitKillable &&
            ArrowTargetReliable(target, plan, 0.68f) &&
            (distance <= Slider(ArrowMenu, "KillSecureRange", 1800) ||
             follow > 0);
    case ArrowPurpose::Teamfight:
        return plan.Evaluation.ExplosionHits >=
            Slider(ArrowMenu, "TeamfightHits", 3) && follow >= 1;
    case ArrowPurpose::Pick:
        return follow >= Slider(ArrowMenu, "PickAllies", 1) &&
            ArrowTargetReliable(target, plan, 0.62f) &&
            plan.Evaluation.StunSeconds >=
                static_cast<float>(Slider(ArrowMenu, "PickStunTenths", 20)) / 10.0f;
    case ArrowPurpose::CrossMap:
        return Bool(ArrowMenu, "CrossMap", false) && follow >= 1 &&
            ArrowTargetReliable(target, plan, 0.72f) &&
            plan.Evaluation.StunSeconds >= 3.0f;
    case ArrowPurpose::FollowVolley:
        return IsFrosted(target) && follow >= 1 &&
            ArrowTargetReliable(target, plan, 0.48f) &&
            (target.IsDashing() || TargetLeavingAutoRange(target) ||
             distance > 760.0f);
    case ArrowPurpose::Manual:
        return true;
    default:
        return false;
    }
}

inline bool CastArrow(const ArrowPlan& plan,
                      const AIHeroClient& target,
                      Mode mode,
                      bool reactive = false) {
    if (!plan.Valid || !ArrowPlanMeetsPurpose(plan, target) || !Ready(3) ||
        !SpellEnabled(3, mode) || !Engine::CanAct(reactive) ||
        !CastThrottleReady(3, reactive) || !HasCurrentResource(SpellCost(3))) {
        return false;
    }
    if (!reactive && Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true)) {
        return false;
    }
    if (plan.CrossMap && plan.Purpose != ArrowPurpose::Manual &&
        plan.Purpose != ArrowPurpose::CrossMap &&
        !Bool(ArrowMenu, "AllowLongProactive", false)) {
        return false;
    }
    if (Engine::ControllerCastPosition(3, plan.Aim)) {
        LastRCastTick = Now();
        LastArrowPurpose = plan.Purpose;
        LastArrowPlan = plan;
        switch (plan.Purpose) {
        case ArrowPurpose::Interrupt: ActiveSequence = Sequence::Interrupt; break;
        case ArrowPurpose::SelfPeel: ActiveSequence = Sequence::SelfPeel; break;
        case ArrowPurpose::AllyPeel: ActiveSequence = Sequence::AllyPeel; break;
        case ArrowPurpose::FollowVolley: ActiveSequence = Sequence::VolleyArrowCatch; break;
        default: ActiveSequence = Sequence::ArrowVolleyFollowup; break;
        }
        return true;
    }
    return false;
}

inline int ScoutHistoryTick(int landmarkId) {
    for (const auto& memory : ScoutHistory) {
        if (memory.LandmarkId == landmarkId) return memory.LastScoutTick;
    }
    return 0;
}

inline void MarkScoutHistory(int landmarkId, int tick) {
    if (landmarkId == 0) return;
    ScoutMemory* empty = nullptr;
    for (auto& memory : ScoutHistory) {
        if (memory.LandmarkId == landmarkId) {
            memory.LastScoutTick = tick;
            return;
        }
        if (!empty && memory.LandmarkId == 0) empty = &memory;
    }
    if (!empty) {
        empty = &*std::min_element(
            ScoutHistory.begin(), ScoutHistory.end(),
            [](const ScoutMemory& left, const ScoutMemory& right) {
                return left.LastScoutTick < right.LastScoutTick;
            });
    }
    empty->LandmarkId = landmarkId;
    empty->LastScoutTick = tick;
}

inline bool IsEnemyJungleSide(const LandmarkDefinition& landmark) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool enemyIsChaos = player.Team() == SDK::GameObjectTeam::Order;
    return landmark.Kind == ScoutKind::Objective ||
           landmark.Kind == ScoutKind::River ||
           landmark.ChaosSide == enemyIsChaos;
}

inline std::vector<ScoutLandmark> BuildScoutLandmarks(
    ScoutPurpose purpose,
    const EnemyTrack* jungler) {
    std::vector<ScoutLandmark> result;
    result.reserve(SummonersRiftLandmarks.size());
    const int now = Now();
    for (const auto& definition : SummonersRiftLandmarks) {
        float weight = definition.Weight;
        bool priority = IsEnemyJungleSide(definition);
        if (!priority && definition.Kind == ScoutKind::Camp) weight *= 0.30f;
        if (definition.Kind == ScoutKind::Objective &&
            purpose == ScoutPurpose::Objective) {
            weight += 4.0f;
            priority = true;
        }
        if (jungler && jungler->LastPosition.IsValid() &&
            jungler->LastPosition.Distance2D(definition.Position) <= 2100.0f) {
            weight += 4.5f;
            priority = true;
        }
        const int last = ScoutHistoryTick(definition.Id);
        result.push_back({
            definition.Position,
            weight,
            definition.Id,
            jungler && priority && definition.Kind == ScoutKind::Camp
                ? ScoutKind::LastSeenJungler : definition.Kind,
            last > 0 && now - last < Slider(HawkshotMenu, "RepeatSeconds", 24) * 1000,
            priority,
            true,
        });
    }
    return result;
}

inline ScoutPlan BuildScoutPlan(ScoutPurpose purpose,
                                const Vector3& manualDestination = {}) {
    ScoutPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return best;
    const EnemyTrack* jungler = RecentJunglerTrack();
    const auto landmarks = BuildScoutLandmarks(purpose, jungler);
    std::vector<Vector3> candidates;
    candidates.reserve(SummonersRiftLandmarks.size() * 2 + 5);
    if (manualDestination.IsValid() && !manualDestination.IsZero()) {
        candidates.push_back(manualDestination);
    } else {
        for (const auto& landmark : SummonersRiftLandmarks) {
            if (IsEnemyJungleSide(landmark) ||
                landmark.Kind == ScoutKind::Objective ||
                landmark.Kind == ScoutKind::River) {
                candidates.push_back(landmark.Position);
                const Vector3 direction = SharedGeometry::Direction2D(
                    player.Position(), landmark.Position);
                if (!direction.IsZero()) {
                    candidates.push_back(player.Position() + direction *
                        std::min(kHawkshotRange, 14500.0f));
                }
            }
        }
        candidates.push_back({ 13500.0f, 0.0f, 13500.0f });
        candidates.push_back({ 1500.0f, 0.0f, 13500.0f });
        candidates.push_back({ 13500.0f, 0.0f, 1500.0f });
    }
    for (Vector3 destination : candidates) {
        if (destination.IsZero()) continue;
        destination.y = player.Position().y;
        const float distance = player.Position().Distance2D(destination);
        if (distance > kHawkshotRange) {
            destination = player.Position() +
                SharedGeometry::Direction2D(player.Position(), destination) *
                kHawkshotRange;
        }
        ScoutEvaluation evaluation = EvaluateHawkshot(
            player.Position(), destination, landmarks);
        if (!evaluation.Valid && purpose != ScoutPurpose::Manual) continue;
        float score = (evaluation.Valid ? evaluation.Score : 0.0f) * 100.0f;
        if (purpose == ScoutPurpose::Manual) score += 1000.0f;
        if (purpose == ScoutPurpose::LastSeenJungler && jungler) score += 260.0f;
        if (purpose == ScoutPurpose::Objective) score +=
            evaluation.ObjectiveCovered * 300.0f;
        if (purpose == ScoutPurpose::FirstClear &&
            evaluation.Covered >= 2) score += 180.0f;
        if (purpose == ScoutPurpose::ChargeCap) score += 120.0f;
        if (!best.Valid || score > best.Score) {
            best.Destination = destination;
            best.Evaluation = evaluation;
            best.Purpose = purpose;
            best.PrimaryLandmarkId = 0;
            for (const auto& landmark : landmarks) {
                if (landmark.Priority && HawkshotCoversPoint(
                        player.Position(), destination, landmark.Position)) {
                    best.PrimaryLandmarkId = landmark.Id;
                    break;
                }
            }
            best.JunglerId = jungler ? jungler->NetworkId : 0;
            best.Score = score;
            best.PreservesCharge = EAmmo > 1;
            best.Valid = true;
        }
    }
    return best;
}

inline bool CastHawkshot(const ScoutPlan& plan,
                         bool manual = false) {
    if (!plan.Valid || !Ready(2) || !Engine::CanAct(false) ||
        !CastThrottleReady(2) || EAmmo <= 0 ||
        (!manual && EAmmo <= Slider(HawkshotMenu, "ReserveCharges", 1))) {
        return false;
    }
    if (Orbwalker::IsWindingUp() &&
        Bool(Engine::HumanMenu, "PreserveAttacks", true)) {
        return false;
    }
    if (!manual && plan.Score < Slider(HawkshotMenu, "MinimumScore", 620)) {
        return false;
    }
    const int requestTick = Now();
    const int ammoBefore = EAmmo;
    if (Engine::ControllerCastPosition(2, plan.Destination)) {
        const bool synchronousEventObserved = LastECastTick >= requestTick;
        LastECastTick = Now();
        LastScoutDecisionTick = LastECastTick;
        LastScoutPurpose = plan.Purpose;
        LastScoutPlan = plan;
        if (!synchronousEventObserved) {
            EAmmo = std::max(0, ammoBefore - 1);
        }
        const auto landmarks = BuildScoutLandmarks(plan.Purpose,
                                                   RecentJunglerTrack());
        for (const auto& landmark : landmarks) {
            if (HawkshotCoversPoint(GameObjects::Player().Position(),
                                    plan.Destination,
                                    landmark.Position)) {
                MarkScoutHistory(landmark.Id, Now());
            }
        }
        switch (plan.Purpose) {
        case ScoutPurpose::FirstClear: ActiveSequence = Sequence::HawkFirstClear; break;
        case ScoutPurpose::LastSeenJungler: ActiveSequence = Sequence::HawkJunglerTrack; break;
        case ScoutPurpose::Objective: ActiveSequence = Sequence::HawkObjective; break;
        case ScoutPurpose::NoFacecheck: ActiveSequence = Sequence::HawkNoFacecheck; break;
        case ScoutPurpose::ChargeCap: ActiveSequence = Sequence::HawkChargeCap; break;
        default: ActiveSequence = Sequence::PlayerLed; break;
        }
        return true;
    }
    return false;
}

inline AIHeroClient ClosestEnemyToCursor(float maximumCursorDistance) {
    AIHeroClient best{};
    float bestDistance = maximumCursorDistance;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        const float distance = enemy.Position().Distance2D(Game::CursorPos());
        if (distance < bestDistance) {
            best = enemy;
            bestDistance = distance;
        }
    }
    return best;
}

inline bool TryManualScout() {
    if (!Key(HawkshotMenu, "ManualE", false) ||
        Now() - LastManualScoutTick < 260) return false;
    LastManualScoutTick = Now();
    return CastHawkshot(
        BuildScoutPlan(ScoutPurpose::Manual, Game::CursorPos()), true);
}

inline bool TryManualArrow(const AIHeroClient& selected) {
    if (!Key(ArrowMenu, "ManualR", false) ||
        Now() - LastManualArrowTick < 260) return false;
    LastManualArrowTick = Now();
    AIHeroClient target = ClosestEnemyToCursor(
        static_cast<float>(Slider(ArrowMenu, "ManualCursorRadius", 700)));
    if (!Engine::ValidEnemy(target)) target = selected;
    if (!Engine::ValidEnemy(target)) return false;
    ArrowPlan plan = BuildArrowPlan(target, ArrowPurpose::Manual);
    return CastArrow(plan, target, Mode::Automatic, true);
}

inline AIHeroClient ProtectedAlly() {
    AIHeroClient ally = SelectProtectionAlly(1200.0f);
    ProtectedAllyId = ally.IsValid()
        ? static_cast<int>(ally.NetworkId()) : 0;
    return ally;
}

inline AIHeroClient SelectPeelThreat(const AIHeroClient& ally) {
    if (!Engine::ValidAlly(ally)) {
        PeelThreatId = 0;
        return {};
    }
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1400.0f)) continue;
        const float distance = enemy.Position().Distance2D(ally.Position());
        if (distance > 720.0f) continue;
        float score = (720.0f - distance) * 0.9f +
                      enemy.TotalAttackDamage() + enemy.AP() * 0.45f;
        if (enemy.IsDashing()) score += 320.0f;
        if (static_cast<int>(enemy.NetworkId()) == IncomingThreatTargetId &&
            IncomingThreatUntil >= Now()) score += 260.0f;
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    PeelThreatId = best.IsValid() ? static_cast<int>(best.NetworkId()) : 0;
    return best;
}

inline bool TryInterrupt() {
    if (InterruptTargetId == 0 || InterruptExpireTick < Now() || !Ready(3) ||
        !Bool(ArrowMenu, "Interrupt", true)) return false;
    const AIHeroClient target = HeroByNetworkId(InterruptTargetId);
    if (!Engine::ValidEnemy(target)) return false;
    ArrowPlan plan = BuildArrowPlan(target, ArrowPurpose::Interrupt);
    return CastArrow(plan, target, Mode::Automatic, true);
}

inline bool TryAntiGapcloser() {
    if (GapcloserTargetId == 0 || GapcloserExpireTick < Now()) return false;
    const AIHeroClient target = HeroByNetworkId(GapcloserTargetId);
    if (!Engine::ValidEnemy(target, 1000.0f)) return false;
    if (Ready(1) && Bool(VolleyMenu, "AntiGapcloser", true)) {
        VolleyPlan volley = BuildVolleyPlan(target, WPurpose::Peel, false);
        if (volley.Valid && CastVolley(volley, Mode::Automatic, true)) return true;
    }
    if (Ready(3) && Bool(ArrowMenu, "AntiGapcloser", true) &&
        (target.Position().Distance2D(GameObjects::Player().Position()) <= 430.0f ||
         GameObjects::Player().HealthPercent() <=
            Slider(ArrowMenu, "SelfPeelHp", 52))) {
        ArrowPlan arrow = BuildArrowPlan(target, ArrowPurpose::SelfPeel);
        return CastArrow(arrow, target, Mode::Automatic, true);
    }
    return false;
}

inline bool TrySelfPeel(const AIHeroClient& selected, Mode mode) {
    const AIHeroClient threat = NearestEnemyToPlayer(selected, 900.0f);
    if (!Engine::ValidEnemy(threat)) return false;
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(threat.Position());
    const bool committed = threat.IsDashing() || distance <=
        std::max(380.0f, threat.AttackRange() + threat.BoundingRadius() + 45.0f) ||
        (IncomingThreatTargetId == static_cast<int>(threat.NetworkId()) &&
         IncomingThreatUntil >= Now());
    if (!committed) return false;
    if (Ready(1) && Bool(VolleyMenu, "SelfPeel", true)) {
        VolleyPlan volley = BuildVolleyPlan(threat, WPurpose::Peel, false);
        if (volley.Valid && CastVolley(
                volley, mode == Mode::None ? Mode::Automatic : mode, true)) {
            return true;
        }
    }
    if (Ready(3) && Bool(ArrowMenu, "SelfPeel", true) &&
        (distance <= 470.0f || player.HealthPercent() <=
            Slider(ArrowMenu, "SelfPeelHp", 52) || IncomingHardCrowdControl)) {
        ArrowPlan arrow = BuildArrowPlan(threat, ArrowPurpose::SelfPeel);
        return CastArrow(arrow, threat,
            mode == Mode::None ? Mode::Automatic : mode, true);
    }
    return false;
}

inline bool TryAllyPeel(const AIHeroClient& ally,
                        const AIHeroClient& threat,
                        Mode mode) {
    if (!Engine::ValidAlly(ally) || !Engine::ValidEnemy(threat) ||
        !Bool(TacticsMenu, "AllyPeel", true)) return false;
    const float separation = ally.Position().Distance2D(threat.Position());
    if (!threat.IsDashing() && separation >
        std::max(420.0f, threat.AttackRange() + 65.0f)) return false;
    if (Ready(1) && separation <= 650.0f) {
        VolleyPlan volley = BuildVolleyPlan(threat, WPurpose::Peel, false);
        if (volley.Valid && CastVolley(
                volley, mode == Mode::None ? Mode::Automatic : mode, true)) {
            ActiveSequence = Sequence::AllyPeel;
            return true;
        }
    }
    if (Ready(3) && (ally.HealthPercent() <=
            Slider(ArrowMenu, "AllyPeelHp", 48) || threat.IsDashing())) {
        ArrowPlan arrow = BuildArrowPlan(threat, ArrowPurpose::AllyPeel);
        if (CastArrow(arrow, threat,
                mode == Mode::None ? Mode::Automatic : mode, true)) {
            ActiveSequence = Sequence::AllyPeel;
            return true;
        }
    }
    return false;
}

inline bool TryKillSecure() {
    if (!Bool(VolleyMenu, "KillSecure", true)) return false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) || IsCommonUntargetableOrImmune(enemy)) {
            continue;
        }
        if (Ready(1) && VolleyDamage(enemy) >=
                enemy.Health() + enemy.AllShield() &&
            GameObjects::Player().Position().Distance2D(enemy.Position()) <=
                kVolleyRange + enemy.BoundingRadius()) {
            VolleyPlan volley = BuildVolleyPlan(
                enemy, WPurpose::KillSecure, false);
            if (volley.Valid && CastVolley(
                    volley, Mode::Automatic, true)) return true;
        }
        if (Ready(3) && Bool(ArrowMenu, "KillSecure", false) &&
            ArrowDamage(enemy) >= enemy.Health() + enemy.AllShield()) {
            ArrowPlan arrow = BuildArrowPlan(enemy, ArrowPurpose::KillSecure);
            if (CastArrow(arrow, enemy, Mode::Automatic, true)) return true;
        }
    }
    return false;
}

inline bool TryComboArrow(const AIHeroClient& target) {
    if (!Ready(3) || !Engine::ValidEnemy(target) ||
        !Bool(ArrowMenu, "Combo", true)) return false;
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    const int cluster = Engine::CountEnemiesAt(target.Position(),
                                                kArrowExplosionRadius + 70.0f);
    ArrowPurpose purpose = ArrowPurpose::None;
    if (cluster >= Slider(ArrowMenu, "TeamfightHits", 3)) {
        purpose = ArrowPurpose::Teamfight;
    } else if (IsFrosted(target) &&
               Now() - LastWCastTick <= 2400 &&
               Bool(ArrowMenu, "AfterVolley", true)) {
        purpose = ArrowPurpose::FollowVolley;
    } else if (distance > 760.0f) {
        purpose = distance > 2500.0f
            ? ArrowPurpose::CrossMap : ArrowPurpose::Pick;
    }
    if (purpose == ArrowPurpose::None) return false;
    ArrowPlan plan = BuildArrowPlan(target, purpose);
    return CastArrow(plan, target, Mode::Combo, false);
}

inline bool TryComboFocus(const AIHeroClient& selected) {
    if (!Ready(0) || FocusActive) return false;
    if (!Bool(FocusMenu, "Combo", true)) return false;

    // Rule 1: Must NOT be winding up an auto attack
    if (Orbwalker::IsWindingUp()) return false;

    // Rule 2: If attack is ready, DO NOT cast Q (priority to auto attack)
    if (Orbwalker::CanAttack()) return false;

    // Rule 3: Must have a valid target in AA range
    AIHeroClient target = selected;
    if (!Engine::ValidEnemy(target, ControllerHelpers::AutoAttackRange(target))) {
        target = NearestEnemyToPlayer(selected, 650.0f);
    }
    if (!Engine::ValidEnemy(target, ControllerHelpers::AutoAttackRange(target))) {
        return false;
    }

    const Vector3 cursorPos = Game::CursorPos();
    if (Engine::ControllerCastPosition(0, cursorPos) || Engine::ControllerCastSelf(0)) {
        LastQCastTick = Now();
        FocusStacks = 0;
        FocusReadyConfirmed = false;
        FocusActive = true;
        FocusActiveUntil = Now() + 4000;
        ActiveSequence = Sequence::AutoFocusReset;
        return true;
    }

    return false;
}

inline bool TryCombo(const AIHeroClient& selected) {
    if (!Engine::ValidEnemy(selected)) return false;
    const float distance = GameObjects::Player().Position().Distance2D(
        selected.Position());
    const bool committed = distance <= 760.0f || IsFrosted(selected) ||
                           Engine::IsHardCrowdControlled(selected);
    if (!committed && TryComboArrow(selected)) return true;

    // Priority Q cast when not winding up, not attack ready, and target in range
    if (TryComboFocus(selected)) return true;

    if (Ready(1) && Bool(VolleyMenu, "Combo", true)) {
        const WPurpose purpose = LastRCastTick > 0 &&
                Now() - LastRCastTick <= 4200
            ? WPurpose::FollowArrow
            : (distance > ControllerHelpers::AutoAttackRange(selected)
                ? WPurpose::Chase : WPurpose::Poke);
        VolleyPlan volley = BuildVolleyPlan(selected, purpose, false);
        const bool valuable = volley.Valid &&
            (volley.Evaluation.ChampionHits >= 2 || volley.Threaded ||
             purpose == WPurpose::FollowArrow ||
             distance > ControllerHelpers::AutoAttackRange(selected, 20.0f) ||
             Engine::IsHardCrowdControlled(selected));
        if (valuable && CastVolley(volley, Mode::Combo)) return true;
    }
    if (TryComboArrow(selected)) return true;
    return TryFocusReset(Mode::Combo);
}

inline bool TryHarass(const AIHeroClient& selected) {
    if (!Engine::ValidEnemy(selected) ||
        ControllerHelpers::PlayerManaPercent() < Slider(VolleyMenu, "HarassMana", 48)) return false;
    if (Ready(1) && Bool(VolleyMenu, "Harass", true)) {
        VolleyPlan volley = BuildVolleyPlan(
            selected, WPurpose::ThreadWave, false);
        const bool acceptable = volley.Valid &&
            (volley.Threaded || volley.Evaluation.ChampionHits >= 2 ||
             LastAfterAttackTargetId == static_cast<int>(selected.NetworkId()) ||
             GameObjects::Player().Position().Distance2D(selected.Position()) >
                ControllerHelpers::AutoAttackRange(selected));
        if (acceptable && CastVolley(volley, Mode::Harass)) return true;
    }
    return TryFocusReset(Mode::Harass);
}

inline bool TryFlee(const AIHeroClient& selected) {
    if (TrySelfPeel(selected, Mode::Flee)) return true;
    const AIHeroClient pursuer = NearestEnemyToPlayer(selected, 1150.0f);
    if (!Engine::ValidEnemy(pursuer)) return false;
    if (Ready(1)) {
        VolleyPlan volley = BuildVolleyPlan(pursuer, WPurpose::Peel, false);
        if (volley.Valid && CastVolley(volley, Mode::Flee, true)) return true;
    }
    if (Ready(3) && GameObjects::Player().HealthPercent() <=
            Slider(ArrowMenu, "SelfPeelHp", 52)) {
        ArrowPlan arrow = BuildArrowPlan(pursuer, ArrowPurpose::SelfPeel);
        return CastArrow(arrow, pursuer, Mode::Flee, true);
    }
    return false;
}

inline bool TryFarm(Mode mode) {
    if (TryFocusReset(mode)) return true;
    if (!Ready(1) || !Bool(FarmMenu, "UseW", true)) return false;
    const int mana = mode == Mode::Jungle
        ? Slider(FarmMenu, "WJungleMana", 30)
        : Slider(FarmMenu, "WWaveMana", 63);
    if (ControllerHelpers::PlayerManaPercent() < mana) return false;
    if (Bool(FarmMenu, "HoldWForChampion", true)) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (Engine::ValidEnemy(enemy, 1350.0f)) return false;
        }
    }
    VolleyPlan plan = BuildFarmVolley(mode);
    const int minimum = mode == Mode::Jungle
        ? Slider(FarmMenu, "WJungleHits", 2)
        : Slider(FarmMenu, "WWaveHits", 5);
    if (!plan.Valid || (plan.Evaluation.MinionHits < minimum &&
        !(mode == Mode::Jungle && plan.Evaluation.UniqueHits > 0))) {
        return false;
    }
    return CastVolley(plan, mode);
}

inline bool TryAutomaticScout(Mode mode) {
    if (!Bool(HawkshotMenu, "Automatic", true) || Game::MapId() != 11 ||
        !Ready(2) || EAmmo <= Slider(HawkshotMenu, "ReserveCharges", 1) ||
        ControllerHelpers::HasEnemyChampionNear(1250.0f) ||
        GameObjects::Player().IsRecalling() ||
        Now() - LastScoutDecisionTick <
            Slider(HawkshotMenu, "DecisionSeconds", 12) * 1000) {
        return false;
    }
    ScoutPurpose purpose = ScoutPurpose::None;
    const float gameSeconds = Game::Time();
    const EnemyTrack* jungler = RecentJunglerTrack();
    if (jungler && Bool(HawkshotMenu, "TrackJungler", true)) {
        purpose = ScoutPurpose::LastSeenJungler;
    } else if (gameSeconds >= 95.0f && gameSeconds <= 230.0f &&
               Bool(HawkshotMenu, "FirstClear", true)) {
        purpose = ScoutPurpose::FirstClear;
    } else if ((mode == Mode::Jungle || HasNearbyJungleTarget(1150.0f)) &&
               Bool(HawkshotMenu, "Objectives", true)) {
        purpose = ScoutPurpose::Objective;
    } else if (EAmmo >= EMaxAmmo &&
               Bool(HawkshotMenu, "AtChargeCap", true)) {
        purpose = ScoutPurpose::ChargeCap;
    }
    if (purpose == ScoutPurpose::None) return false;
    return CastHawkshot(BuildScoutPlan(purpose), false);
}

inline Posture ChoosePosture(Mode mode,
                             const AIHeroClient& selected,
                             const AIHeroClient& ally,
                             const AIHeroClient& peelThreat) {
    if (mode == Mode::Flee) return Posture::Flee;
    if (Engine::ValidAlly(ally) && Engine::ValidEnemy(peelThreat)) {
        return Posture::Peel;
    }
    if (mode == Mode::LaneClear || mode == Mode::LastHit) {
        if (HasNearbyJungleTarget(1000.0f)) return Posture::Objective;
        return ControllerHelpers::HasEnemyChampionNear(1400.0f)
            ? Posture::LanePoke : Posture::Siege;
    }
    if (mode == Mode::Harass) return Posture::LanePoke;
    if (mode == Mode::Combo && Engine::ValidEnemy(selected)) {
        const int cluster = Engine::CountEnemiesAt(selected.Position(), 650.0f);
        if (cluster >= 3) return Posture::Teamfight;
        const float distance = GameObjects::Player().Position().Distance2D(
            selected.Position());
        if (distance > 1200.0f) return Posture::Pick;
        if (distance > ControllerHelpers::AutoAttackRange(selected)) {
            return Posture::Chase;
        }
        return IsFrosted(selected) ? Posture::ExtendedTrade : Posture::Kite;
    }
    if (EAmmo >= EMaxAmmo &&
        !ControllerHelpers::HasEnemyChampionNear(1500.0f)) {
        return Posture::Scout;
    }
    return Posture::Neutral;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastKnownMode = mode;
    RefreshState();
    const AIHeroClient ally = ProtectedAlly();
    const AIHeroClient peelThreat = SelectPeelThreat(ally);
    CurrentPosture = ChoosePosture(mode, selected, ally, peelThreat);

    if (TryManualScout()) return true;
    if (TryManualArrow(selected)) return true;
    if (mode == Mode::Combo && TryComboFocus(selected)) return true;
    if (TryFocusReset(mode)) return true;
    if (TryInterrupt()) return true;
    if (TryAntiGapcloser()) return true;
    if (TrySelfPeel(selected, mode)) return true;
    if (TryAllyPeel(ally, peelThreat, mode)) return true;
    if (TryKillSecure()) return true;

    bool action = false;
    if (mode == Mode::Flee) {
        action = TryFlee(selected);
    } else if (mode == Mode::Combo) {
        action = TryCombo(selected);
    } else if (mode == Mode::Harass) {
        action = TryHarass(selected);
    } else if (mode == Mode::LaneClear) {
        action = TryFarm(HasNearbyJungleTarget(1200.0f)
            ? Mode::Jungle : Mode::LaneClear);
    } else if (mode == Mode::LastHit) {
        action = TryFocusReset(Mode::LastHit);
    }
    if (action) return true;
    return TryAutomaticScout(mode);
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    int autoTarget = 0;
    int autoTick = 0;
    if (CaptureLocalAutoAttack(args, autoTarget, autoTick)) {
        LastAutoTargetId = autoTarget;
        LastAutoTick = autoTick;
        FocusStacks = std::min(4, FocusStacks + (FocusActive ? 0 : 1));
        if (autoTarget != 0) {
            const AIBaseClient target = UnitByNetworkId(autoTarget);
            if (target.IsValid() && target.IsHero()) {
                LastCombatTick = autoTick;
                SetFrost(autoTarget, autoTick + 2000, false);
            }
        }
        return;
    }
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q) ||
        SpellEventNameContainsAny(args, { "asheq", "rangersfocus" })) {
        LastQCastTick = now;
        FocusStacks = 0;
        FocusReadyConfirmed = false;
        FocusActive = true;
        FocusActiveUntil = now + 4000;
        if (!Engine::WasControllerCast(0)) {
            LastQPurpose = QPurpose::None;
            ActiveSequence = Sequence::PlayerLed;
        }
        return;
    }
    if (IsVolleyEvent(args)) {
        LastWCastTick = now;
        if (!Engine::WasControllerCast(1)) {
            LastWPurpose = WPurpose::None;
            LastVolleyPlan = {};
            LastVolleyPlan.Aim = args.EndPosition.IsValid() &&
                    !args.EndPosition.IsZero()
                ? args.EndPosition : args.CastPosition;
            LastVolleyPlan.Purpose = WPurpose::None;
            LastVolleyPlan.Valid = LastVolleyPlan.Aim.IsValid() &&
                                   !LastVolleyPlan.Aim.IsZero();
            ActiveSequence = Sequence::PlayerLed;
        }
        return;
    }
    if (IsHawkshotEvent(args)) {
        LastECastTick = now;
        EAmmo = std::max(0, EAmmo - 1);
        if (!Engine::WasControllerCast(2)) {
            LastScoutPurpose = ScoutPurpose::Manual;
            LastScoutPlan = {};
            LastScoutPlan.Destination = args.EndPosition.IsValid() &&
                    !args.EndPosition.IsZero()
                ? args.EndPosition : args.CastPosition;
            LastScoutPlan.Purpose = ScoutPurpose::Manual;
            LastScoutPlan.Valid = LastScoutPlan.Destination.IsValid() &&
                                  !LastScoutPlan.Destination.IsZero();
            ActiveSequence = Sequence::PlayerLed;
        }
        return;
    }
    if (IsArrowEvent(args)) {
        LastRCastTick = now;
        if (!Engine::WasControllerCast(3)) {
            LastArrowPurpose = ArrowPurpose::Manual;
            LastArrowPlan = {};
            LastArrowPlan.Aim = args.EndPosition.IsValid() &&
                    !args.EndPosition.IsZero()
                ? args.EndPosition : args.CastPosition;
            LastArrowPlan.Purpose = ArrowPurpose::Manual;
            LastArrowPlan.Valid = LastArrowPlan.Aim.IsValid() &&
                                  !LastArrowPlan.Aim.IsZero();
            ActiveSequence = Sequence::PlayerLed;
        }
    }
}

inline void RecordIncomingThreats(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto analysis = AnalyzeEnemyCast(
        args, 240.0f, 110.0f, 300, 250, 220, 1800, 520);
    if (!analysis.Valid) return;
    if (analysis.TargetsPlayer || analysis.CrossesPlayer) {
        IncomingThreatTargetId = static_cast<int>(analysis.Enemy.NetworkId());
        IncomingThreatUntil = std::max(
            analysis.CommitmentUntilTick, analysis.LineThreatUntilTick);
        IncomingHardCrowdControl = analysis.LikelyHardCrowdControl;
        RecentIncomingPressure = std::min(
            8.0f, RecentIncomingPressure +
                (analysis.LikelyHardCrowdControl ? 2.5f : 1.0f));
    }
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args,
                            bool added) {
    if (IsLocalPlayer(args.Sender)) {
        if (IsFocusReadyBuff(args.BuffName)) {
            FocusReadyConfirmed = added;
            FocusStacks = added ? 4 : std::min(FocusStacks, 3);
            LastFocusObservationTick = Now();
        }
        if (IsFocusActiveBuff(args.BuffName)) {
            FocusActive = added;
            FocusActiveUntil = added
                ? ControllerHelpers::BuffExpireTick(args, 4000) : Now();
            if (added) {
                FocusStacks = 0;
                FocusReadyConfirmed = false;
            }
            LastFocusObservationTick = Now();
        }
        if (Engine::TextContains(args.BuffName, "asheq") &&
            args.Count >= 0 && args.Count <= 4 &&
            !IsFocusActiveBuff(args.BuffName)) {
            FocusStacks = added ? std::clamp(args.Count, 0, 4) : 0;
            if (FocusStacks >= 4) FocusReadyConfirmed = true;
            LastFocusObservationTick = Now();
        }
        return;
    }
    if (!IsFrostBuff(args.BuffName)) return;
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id == 0) return;
    if (added) {
        SetFrost(id, ControllerHelpers::BuffExpireTick(args, 2000), true);
    } else if (FrostRecord* record = FindFrost(id)) {
        *record = {};
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(args,
                            LastAfterAttackTargetId,
                            LastAfterAttackTick)) return;
    LastAutoTargetId = LastAfterAttackTargetId;
    LastAutoTick = LastAfterAttackTick;
    const AIBaseClient target = UnitByNetworkId(LastAfterAttackTargetId);
    if (target.IsValid() && target.IsHero()) LastCombatTick = LastAfterAttackTick;
    if (LastKnownMode == Mode::Combo) {
        AIHeroClient heroTarget = target.IsValid() && target.IsHero()
            ? AIHeroClient(target.Handle())
            : AIHeroClient();
        if (TryComboFocus(heroTarget)) return;
    }
    (void)TryFocusReset(LastKnownMode);
}

inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (ControllerHelpers::CaptureGapcloser(
            args, GapcloserTargetId, GapcloserEnd,
            GapcloserExpireTick, 720.0f, 950)) {
        IncomingThreatTargetId = GapcloserTargetId;
        IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 700);
    }
}


inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args) ||
        !IsArrowMissileName(args.SpellName, args.MissileName)) return;
    RMissileNetworkId = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    RMissilePosition = args.Sender.Position.IsValid()
        ? args.Sender.Position : args.StartPosition;
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = args.MissileNetworkId != 0
        ? static_cast<int>(args.MissileNetworkId)
        : static_cast<int>(args.Sender.NetworkId);
    if (id != RMissileNetworkId &&
        !IsArrowMissileName(args.SpellName, args.MissileName)) return;
    RMissileNetworkId = 0;
    RMissilePosition = {};
}

inline const char* SequenceName(Sequence sequence) {
    switch (sequence) {
    case Sequence::VolleyAutoPoke: return "W-AA poke";
    case Sequence::AutoFocusReset: return "AA-Q-AA reset";
    case Sequence::VolleyArrowCatch: return "W-R catch";
    case Sequence::ArrowVolleyFollowup: return "R-W-AA-Q";
    case Sequence::SelfPeel: return "self peel";
    case Sequence::AllyPeel: return "ally peel";
    case Sequence::Interrupt: return "interrupt";
    case Sequence::HawkFirstClear: return "first-clear scout";
    case Sequence::HawkJunglerTrack: return "jungler track";
    case Sequence::HawkObjective: return "objective scout";
    case Sequence::HawkNoFacecheck: return "no-facecheck";
    case Sequence::HawkChargeCap: return "charge-cap scout";
    case Sequence::FarmFan: return "farm fan";
    case Sequence::JungleFlurry: return "camp flurry";
    case Sequence::StructureFlurry: return "structure flurry";
    case Sequence::PlayerLed: return "player-led";
    default: return "none";
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::LanePoke: return "lane poke";
    case Posture::ExtendedTrade: return "extended trade";
    case Posture::Chase: return "chase";
    case Posture::Kite: return "kite";
    case Posture::Peel: return "peel";
    case Posture::Teamfight: return "teamfight";
    case Posture::Pick: return "pick";
    case Posture::Objective: return "objective";
    case Posture::Scout: return "scout";
    case Posture::Siege: return "siege";
    case Posture::Flee: return "flee";
    default: return "neutral";
    }
}

inline const char* QPurposeName(QPurpose purpose) {
    switch (purpose) {
    case QPurpose::ResetTrade: return "trade";
    case QPurpose::AllIn: return "all-in";
    case QPurpose::Lethal: return "lethal";
    case QPurpose::Waveclear: return "wave";
    case QPurpose::Jungle: return "camp";
    case QPurpose::Objective: return "objective";
    case QPurpose::Structure: return "structure";
    default: return "hold";
    }
}

inline const char* ScoutPurposeName(ScoutPurpose purpose) {
    switch (purpose) {
    case ScoutPurpose::Manual: return "manual";
    case ScoutPurpose::FirstClear: return "first clear";
    case ScoutPurpose::LastSeenJungler: return "jungler";
    case ScoutPurpose::Objective: return "objective";
    case ScoutPurpose::NoFacecheck: return "no-facecheck";
    case ScoutPurpose::AntiGank: return "anti-gank";
    case ScoutPurpose::ChargeCap: return "charge cap";
    default: return "hold";
    }
}

inline const char* ArrowPurposeName(ArrowPurpose purpose) {
    switch (purpose) {
    case ArrowPurpose::Manual: return "manual";
    case ArrowPurpose::SelfPeel: return "self peel";
    case ArrowPurpose::AllyPeel: return "ally peel";
    case ArrowPurpose::Interrupt: return "interrupt";
    case ArrowPurpose::Pick: return "pick";
    case ArrowPurpose::Teamfight: return "teamfight";
    case ArrowPurpose::CrossMap: return "cross-map";
    case ArrowPurpose::KillSecure: return "execute";
    case ArrowPurpose::FollowVolley: return "after W";
    default: return "hold";
    }
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (Bool(CoachMenu, "DrawRanges", true)) {
        Drawing::DrawCircle(player.Position(), kAttackRange,
                            0x337BD9FFu, 1.0f, 72);
        Drawing::DrawCircle(player.Position(), kVolleyRange,
                            0x3359B8FFu, 1.0f, 80);
    }
    if (Bool(CoachMenu, "DrawVolley", true) && LastVolleyPlan.Valid &&
        Now() - LastWCastTick <= 1800) {
        const Vector3 direction = SharedGeometry::Direction2D(
            player.Position(), LastVolleyPlan.Aim);
        for (int ray = 0; ray < VolleyArrowCount(SpellRank(1)); ++ray) {
            const Vector3 rayDirection = VolleyRayDirection(
                direction, SpellRank(1), ray);
            const bool primary = ray == LastVolleyPlan.Evaluation.PrimaryRay;
            Drawing::DrawLine(player.Position(),
                              player.Position() + rayDirection * kVolleyRange,
                              primary ? 0xEEB7F5FFu : 0x557BCBFFu,
                              primary ? 2.2f : 0.8f);
        }
        if (LastVolleyPlan.PrimaryPosition.IsValid() &&
            !LastVolleyPlan.PrimaryPosition.IsZero()) {
            Drawing::DrawCircle(LastVolleyPlan.PrimaryPosition, 75.0f,
                                0xDDB7F5FFu, 1.8f, 40);
        }
    }
    if (Bool(CoachMenu, "DrawHawkshot", true) && LastScoutPlan.Valid &&
        Now() - LastECastTick <= 6000) {
        Drawing::DrawLine(player.Position(), LastScoutPlan.Destination,
                          0xAA7FE8D8u, 1.7f);
        Drawing::DrawCircle(LastScoutPlan.Destination,
                            kHawkshotDestinationVisionRadius,
                            0x557FE8D8u, 1.2f, 72);
    }
    if (Bool(CoachMenu, "DrawArrow", true) && LastArrowPlan.Valid &&
        Now() - LastRCastTick <= 5200) {
        const Vector3 origin = RMissilePosition.IsValid() &&
                !RMissilePosition.IsZero()
            ? RMissilePosition : player.Position();
        Drawing::DrawLine(origin, LastArrowPlan.FirstHitPosition,
                          0xDDDDF6FFu, 2.2f);
        Drawing::DrawCircle(LastArrowPlan.FirstHitPosition,
                            kArrowExplosionRadius,
                            0x66A4C8FFu, 1.5f, 72);
    }
    if (Bool(CoachMenu, "DrawPeel", true)) {
        const AIHeroClient ally = ProtectedAllyId != 0
            ? AIHeroClient(UnitByNetworkId(ProtectedAllyId).Handle())
            : AIHeroClient{};
        const AIHeroClient threat = HeroByNetworkId(PeelThreatId);
        if (Engine::ValidAlly(ally)) {
            Drawing::DrawCircle(ally.Position(), 95.0f,
                                0xAA73F0FFu, 1.7f, 40);
        }
        if (Engine::ValidAlly(ally) && Engine::ValidEnemy(threat)) {
            Drawing::DrawLine(ally.Position(), threat.Position(),
                              0xFFFF6A6Au, 2.0f);
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char state[520]{};
            _snprintf_s(
                state, sizeof(state), _TRUNCATE,
                "Ashe one-trick | %s | %s | Focus %d%s (%s) | E %d/%d %s | R %s %.1fs/%d hits",
                PostureName(CurrentPosture), SequenceName(ActiveSequence),
                FocusStacks, FocusActive ? " ACTIVE" :
                    (FocusReadyConfirmed ? " READY" : ""),
                QPurposeName(LastQPurpose), EAmmo, EMaxAmmo,
                ScoutPurposeName(LastScoutPurpose),
                ArrowPurposeName(LastArrowPurpose),
                LastArrowPlan.Valid ? LastArrowPlan.Evaluation.StunSeconds : 0.0f,
                LastArrowPlan.Valid ? LastArrowPlan.Evaluation.ExplosionHits : 0);
            Drawing::DrawText(screen.x - 285.0f, screen.y - 118.0f,
                              0xFFB7F5FFu, state);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "AsheOneTrick", "Ashe one-trick mechanics"));
    TacticsMenu->Add(new MenuBool(
        "AllyPeel", "Peel a pressured carry", true));
    TacticsMenu->Add(new MenuSeparator(
        "Ownership",
        "Movement, attack-move,"));

    FocusMenu = TacticsMenu->AddSubMenu(new Menu(
        "RangersFocus", "Q four-stack reset and flurry discipline"));
    FocusMenu->Add(new MenuBool(
        "Combo", "AA-Q-AA sustained combo", true));
    FocusMenu->Add(new MenuBool(
        "Harass", "Reset player trade", true));
    FocusMenu->Add(new MenuBool(
        "Wave", "Use after an auto only while", true));
    FocusMenu->Add(new MenuBool(
        "Jungle", "Use after an auto on a", true));
    FocusMenu->Add(new MenuBool(
        "Objective", "After AA on epic", true));
    FocusMenu->Add(new MenuBool(
        "Structures", "Use after an auto while", true));
    FocusMenu->Add(new MenuBool(
        "RespectFlatReduction", "Hold short flurries into", true));
    FocusMenu->Add(new MenuSeparator(
        "Reset",
        "Q is never pre-cast: the"));

    VolleyMenu = TacticsMenu->AddSubMenu(new Menu(
        "Volley", "W live 7-11 ray fan and first-blocker solver"));
    VolleyMenu->Add(new MenuBool(
        "Combo", "clear ray for chase, setup", true));
    VolleyMenu->Add(new MenuBool(
        "Harass", "Thread a side ray through", true));
    VolleyMenu->Add(new MenuSlider(
        "HarassMana", "Minimum mana for W harass (%)", 48, 0, 100));
    VolleyMenu->Add(new MenuBool(
        "SelfPeel", "Slow diver before R", true));
    VolleyMenu->Add(new MenuBool(
        "AntiGapcloser", "Best clear W ray on a", true));
    VolleyMenu->Add(new MenuBool(
        "KillSecure", "Use only a predicted lethal", true));
    VolleyMenu->Add(new MenuSeparator(
        "Geometry",
        "Every rank-dependent"));

    HawkshotMenu = TacticsMenu->AddSubMenu(new Menu(
        "Hawkshot", "E charge economy and information routes"));
    HawkshotMenu->Add(new MenuKeyBind(
        "ManualE", "Scout through cursor", Keys::G, KeyBindType::Press));
    HawkshotMenu->Add(new MenuBool(
        "Automatic", "Allow conservative", true));
    HawkshotMenu->Add(new MenuBool(
        "FirstClear", "Scout enemy camps", true));
    HawkshotMenu->Add(new MenuBool(
        "TrackJungler", "Project last Smite", true));
    HawkshotMenu->Add(new MenuBool(
        "Objectives", "Scout objectives", true));
    HawkshotMenu->Add(new MenuBool(
        "AtChargeCap", "Spend a strong route before", true));
    HawkshotMenu->Add(new MenuSlider(
        "ReserveCharges", "Automatic charges to preserve", 1, 0, 1));
    HawkshotMenu->Add(new MenuSlider(
        "MinimumScore", "Minimum automatic route score", 620, 200, 1800));
    HawkshotMenu->Add(new MenuSlider(
        "RepeatSeconds", "No repeat landmark (s)", 24, 8, 60));
    HawkshotMenu->Add(new MenuSlider(
        "DecisionSeconds", "Min scout interval", 12, 5, 40));
    HawkshotMenu->Add(new MenuSeparator(
        "MapScope",
        "Automatic landmarks run on"));

    ArrowMenu = TacticsMenu->AddSubMenu(new Menu(
        "CrystalArrow", "R first-champion collision and follow-up commitment"));
    ArrowMenu->Add(new MenuKeyBind(
        "ManualR", "Fire scored R at the enemy nearest cursor", Keys::T,
        KeyBindType::Press));
    ArrowMenu->Add(new MenuSlider(
        "ManualCursorRadius", "Manual cursor radius", 700, 150, 1600));
    ArrowMenu->Add(new MenuBool(
        "Combo", "R for verified pick or", true));
    ArrowMenu->Add(new MenuBool(
        "AfterVolley", "W slow for R confirm", true));
    ArrowMenu->Add(new MenuSlider(
        "PickAllies", "Min allied follow-up", 1, 0, 3));
    ArrowMenu->Add(new MenuSlider(
        "PickStunTenths", "Min pick stun (tenths)", 20, 10, 35));
    ArrowMenu->Add(new MenuSlider(
        "TeamfightHits", "Minimum R explosion targets", 3, 2, 5));
    ArrowMenu->Add(new MenuBool(
        "CrossMap", "Allow cross-map picks", false));
    ArrowMenu->Add(new MenuBool(
        "AllowLongProactive", "Allow long-range plans", false));
    ArrowMenu->Add(new MenuBool(
        "Interrupt", "Interrupt when R arrives", true));
    ArrowMenu->Add(new MenuBool(
        "AntiGapcloser", "R point-blank peel", true));
    ArrowMenu->Add(new MenuBool(
        "SelfPeel", "R after W fails escape", true));
    ArrowMenu->Add(new MenuSlider(
        "SelfPeelHp", "Player HP for defensive R (%)", 52, 10, 90));
    ArrowMenu->Add(new MenuSlider(
        "AllyPeelHp", "Ally HP for R peel (%)", 48, 10, 85));
    ArrowMenu->Add(new MenuBool(
        "KillSecure", "Allow exact R execute under", false));
    ArrowMenu->Add(new MenuSlider(
        "KillSecureRange", "Maximum solo R execute range", 1800, 700, 5000));
    ArrowMenu->Add(new MenuSeparator(
        "Collision",
        "R ignores minions, stops on"));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Player-led flurry and exact Volley farming"));
    FarmMenu->Add(new MenuBool(
        "UseW", "fan on a valuable wave or camp", true));
    FarmMenu->Add(new MenuBool(
        "HoldWForChampion", "Preserve W while an enemy", true));
    FarmMenu->Add(new MenuSlider(
        "WWaveHits", "Min unique W targets", 5, 3, 10));
    FarmMenu->Add(new MenuSlider(
        "WJungleHits", "Minimum jungle targets for W", 2, 1, 6));
    FarmMenu->Add(new MenuSlider(
        "WWaveMana", "Minimum mana for wave W (%)", 63, 0, 100));
    FarmMenu->Add(new MenuSlider(
        "WJungleMana", "Minimum mana for jungle W (%)", 30, 0, 100));
    FarmMenu->Add(new MenuSlider(
        "QWaveMana", "Minimum mana for wave Q (%)", 42, 0, 100));
    FarmMenu->Add(new MenuSlider(
        "QJungleMana", "Minimum mana for jungle Q (%)", 24, 0, 100));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "One-trick geometry and state visualization"));
    CoachMenu->Add(new MenuBool(
        "DrawRanges", "Draw attack and Volley ranges", false));
    CoachMenu->Add(new MenuBool(
        "DrawVolley", "Draw Volley rays", false));
    CoachMenu->Add(new MenuBool(
        "DrawHawkshot", "Draw latest scout route and", false));
    CoachMenu->Add(new MenuBool(
        "DrawArrow", "Draw R path/explosion", false));
    CoachMenu->Add(new MenuBool(
        "DrawPeel", "Draw protected ally and diver", false));
    CoachMenu->Add(new MenuBool(
        "DrawState", "Draw posture/Focus/R", false));
}

inline void OnLoad() {
    ActiveSequence = Sequence::None;
    CurrentPosture = Posture::Neutral;
    LastQPurpose = QPurpose::None;
    LastWPurpose = WPurpose::None;
    LastScoutPurpose = ScoutPurpose::None;
    LastArrowPurpose = ArrowPurpose::None;
    LastKnownMode = Mode::None;
    FocusStacks = 0;
    FocusReadyConfirmed = false;
    FocusActive = false;
    FocusActiveUntil = 0;
    LastFocusObservationTick = 0;
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastAutoTick = LastAutoTargetId = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = 0;
    LastCombatTick = 0;
    EAmmo = 0;
    EMaxAmmo = 2;
    EAmmoObserved = false;
    LastEAmmoObservationTick = 0;
    RMissileNetworkId = 0;
    RMissilePosition = {};
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEnd = {};
    InterruptTargetId = InterruptExpireTick = 0;
    IncomingThreatTargetId = IncomingThreatUntil = 0;
    IncomingHardCrowdControl = false;
    RecentIncomingPressure = 0.0f;
    ProtectedAllyId = PeelThreatId = 0;
    LastScoutDecisionTick = LastManualScoutTick = LastManualArrowTick = 0;
    FrostedTargets.fill({});
    EnemyTracks.fill({});
    ScoutHistory.fill({});
    LastVolleyPlan = {};
    LastScoutPlan = {};
    LastArrowPlan = {};
    RefreshState();
}

inline void OnUnload() {
    TacticsMenu = FocusMenu = VolleyMenu = HawkshotMenu = nullptr;
    ArrowMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Use League 26.14 and CommunityDragon 16.14 as the pinned live kit",
    "Use Riot 26.10 Q total damage of 110/115/120/125/130 percent AD",
    "Use Riot 26.1 Volley base damage and full bonus-AD ratio",
    "Reject every old local Ashe controller's fixed 2500 E and R range",
    "Reject a generic single-line interpretation of Volley",
    "Create seven Volley rays at rank one",
    "Create eight Volley rays at rank two",
    "Create nine Volley rays at rank three",
    "Create ten Volley rays at rank four",
    "Create eleven Volley rays at rank five",
    "Keep exactly five degrees between neighboring Volley rays",
    "Center odd-rank fans on a real middle ray",
    "Center even-rank fans between the two middle rays",
    "Resolve the first hostile blocker independently on every Volley ray",
    "Let a side ray pass around a minion that blocks the center ray",
    "Reject W when no ray reaches the selected champion",
    "Count a unit hit by several rays only once for damage and AoE value",
    "Keep minions in the geometry even during champion-only casts",
    "Keep jungle monsters in the geometry even during champion-only casts",
    "Predict every blocker at its own ray impact time",
    "Generate candidate aim headings that align every ray with every unit",
    "Prefer a side-threaded primary ray during lane poke",
    "Prefer multi-champion fan value when it does not lose the primary",
    "Reject W through an active projectile wall",
    "Require high prediction for ordinary W poke",
    "Require very-high prediction for lethal W",
    "Relax W prediction only for dashes, hard CC and urgent peel",
    "Do not cancel a valuable player attack windup to cast W",
    "Use W after R stun as R-W-AA-Q follow-up",
    "Use W before R only when the slow represents real commitment",
    "Use W to slow a committed melee diver before spending R",
    "Use W on a directed gapcloser using the exact clear ray",
    "Use W kill secure only when damage and first-blocker geometry agree",
    "Hold W farming while a champion can contest the wave",
    "Require five unique lane targets by default for farm W",
    "Score epic monsters above ordinary camp bodies",
    "Never cast Q before the attack that gives reset value",
    "Activate Q only from the after-attack window",
    "Require four observed or confirmed Focus stacks",
    "Treat asheqcastready as authoritative four-stack telemetry",
    "Treat AsheQBuff as authoritative active-flurry telemetry",
    "Use event-counted Focus only as a conservative fallback",
    "Clear ready state when the active Q buff appears",
    "Model AA-Q-AA as a reset rather than a generic self buff",
    "Require at least two expected follow-up attacks on a champion",
    "Increase expected attacks when Frost already controls the target",
    "Increase expected attacks while the target is hard controlled",
    "Reduce expected attacks when an unfrosted target leaves range",
    "Hold a short Q flurry into Amumu flat per-hit reduction",
    "Hold a short Q flurry into Fizz flat per-hit reduction",
    "Hold a short Q flurry into active Leona Eclipse reduction",
    "Override flat-reduction hold for a predicted lethal flurry",
    "Use Q on an epic monster after a real player attack",
    "Use Q on an ordinary camp only while three attacks remain",
    "Use Q on a lane wave only while four attacks remain",
    "Use Q on a structure only after the player attacks it",
    "Respect separate Q mana reserves for wave and jungle",
    "Never issue an attack merely to make Q available",
    "Track Frost from confirmed passive buff events",
    "Track short predicted Frost after a local auto",
    "Track short predicted Frost after the selected Volley ray lands",
    "Expire predicted Frost after the live two-second duration",
    "Use live passive state rather than assuming every target stays slowed",
    "Read Hawkshot Ammo and MaxAmmo when the spell instance exposes them",
    "Accept only a one-or-two-charge Hawkshot signature",
    "Preserve one automatic Hawkshot charge by default",
    "Allow the manual Hawkshot key to spend the reserved charge",
    "Keep automatic map landmarks disabled outside Summoner's Rift",
    "Let manual cursor Hawkshot work on nonstandard maps",
    "Identify the enemy jungler from either Smite summoner slot",
    "Remember the last visible position of every enemy champion",
    "Remember the last path end of the Smite holder",
    "Reject stale jungler tracks older than thirty seconds",
    "Wait until the jungler has actually left vision before tracking",
    "Score routes through several enemy camps above a single camp",
    "Score the last-seen jungler's nearby camps as priority landmarks",
    "Score objective destination vision above an ordinary river reveal",
    "Use Hawkshot path vision as well as destination vision",
    "Use the 325 path-vision radius from live data",
    "Use the 1000 destination-vision radius from live data",
    "Penalize repeatedly scouting the same recent landmarks",
    "Throttle automatic scouting decisions independently from casts",
    "Use the early first-clear timing window only once routes score well",
    "Spend at charge cap only on a strong information line",
    "Do not automatic-scout while an enemy champion is in combat range",
    "Do not cancel a valuable player attack windup for Hawkshot",
    "Expose the latest Hawkshot line and destination reveal to the player",
    "Model Crystal Arrow as global rather than a 2500-range line",
    "Model Crystal Arrow initial speed of 1500",
    "Model Crystal Arrow acceleration of 200 per second squared",
    "Cap Crystal Arrow speed at 2100",
    "Include the 0.25-second Arrow cast time",
    "Scale Arrow stun from one to three-and-a-half seconds by distance",
    "Cap maximum stun scaling at the live 2800 distance",
    "Use a 130-radius Arrow capsule",
    "Use a 400-radius impact explosion",
    "Ignore minions for Arrow collision",
    "Stop Arrow evaluation on the first enemy champion capsule",
    "Reject a desired target hidden behind another enemy champion",
    "Allow the first champion to create multi-target explosion value",
    "Iterate target prediction against accelerating travel time",
    "Prefer movement parallel to the Arrow line over perpendicular jukes",
    "Prefer a dashing endpoint only when the live endpoint is valid",
    "Reject R through an active projectile wall",
    "Reject proactive R into spell shield or spell immunity",
    "Require allied follow-up for an ordinary pick",
    "Count Ashe herself as follow-up only at practical local range",
    "Require a configurable minimum distance-scaled stun for picks",
    "Require clustered explosion value for teamfight R",
    "Allow point-blank R without long-stun requirements for self peel",
    "Use point-blank R only after W cannot safely solve the dive",
    "Use R to peel a threatened allied carry",
    "Use R interrupt only when calculated impact precedes channel end",
    "Use R kill secure only when exact mitigated damage is lethal",
    "Keep automatic R execute disabled by default",
    "Keep automatic cross-map R disabled by default",
    "Require path alignment, full stun and allied follow-up for cross-map R",
    "Let the manual R key select the enemy nearest the player's cursor",
    "Let manual R override automatic long-range restrictions",
    "Still reject manual R when no enemy champion is first on the line",
    "Use W-R when a Frosted target is leaving the local fight",
    "Use R-W-AA-Q after a successful long-range catch",
    "Do not cast Q during the R-W part before the player's attack",
    "Prioritize interrupt and anti-gapcloser reactions before damage",
    "Prioritize self peel before kill secure",
    "Prioritize protected-ally peel before ordinary combo damage",
    "Choose front-to-back W against the player-selected target",
    "Retain player movement ownership while kiting",
    "Retain player attack-move ownership while chasing",
    "Retain player target-selection ownership",
    "Retain player summoner-spell ownership",
    "Yield briefly after observed manual spell casts through engine arbitration",
    "Never use Flash to extend a Volley or Arrow plan",
    "Never move Ashe toward a speculative Hawkshot reveal",
    "Never facecheck merely because Hawkshot is on cooldown",
    "Expose Focus stack confidence instead of claiming perfect telemetry",
    "Expose observed Hawkshot charges instead of inventing infinite scouting",
    "Expose the chosen Volley primary ray",
    "Expose Arrow first-hit point, stun duration and explosion count",
    "Expose the protected ally and selected diver",
    "Own the full decision loop and never fall back to generic Q-W-E-R priority",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Ashe;
    controller.ControllerId = "champion.kuroaio.ai.ashe.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIAshe.md";
    controller.ImplementationSummary =
        "Four-stack after-attack AA-Q-AA policy; rank-dependent 7-11 ray "
        "Volley solver with independent first blockers and side-ray threading; "
        "two-charge Hawkshot information planner using Smite-holder history, "
        "multi-camp/objective coverage and repeat suppression; accelerating "
        "global Arrow first-champion/AoE geometry with distance stun, path "
        "alignment, interrupt timing, allied follow-up and player-owned kiting.";
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
    controller.OnBuffAdd =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnBuffRemove =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;
    controller.OnBuffUpdate =
        &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1900, 250, 6000>;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Ashe
