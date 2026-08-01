#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIApheliosGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Aphelios {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::CursorDirectionAgrees;
using ControllerHelpers::EnemyFlashReady;
using ControllerHelpers::HasNearbyJungleTarget;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::MissileEventIsLocal;
using ControllerHelpers::Now;
using ControllerHelpers::PredictionAtLeast;
using ControllerHelpers::ProjectileWallBlocksFromPlayer;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectJungleTarget;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEventNameContains;
using ControllerHelpers::SpellRank;

enum class Sequence : std::uint8_t {
    None,
    CalibrumMarkChain,
    CalibrumPreMarkAuto,
    SeverumCommit,
    SeverumGravitumRoot,
    GravitumGlobalRoot,
    InfernumWave,
    InfernumMoonlight,
    SentryArmMoonlight,
    CrescendumCloseDps,
    LowAmmoSwap,
    IncomingWeaponCancel,
    HiddenWeaponUltimate,
    DefensiveMoonlight,
    RotationRepair,
    ObjectivePreparation,
    PlayerLedWeave,
};

enum class Posture : std::uint8_t {
    Neutral,
    Poke,
    ShortTrade,
    Catch,
    FrontToBack,
    CloseCommit,
    Peel,
    Disengage,
    Objective,
    Rotation,
    IncomingWeapon,
};

enum class QPurpose : std::uint8_t {
    None,
    Trade,
    AllIn,
    MarkChain,
    RootSetup,
    Peel,
    Interrupt,
    AntiGapcloser,
    Sustain,
    ChakramBuild,
    Teamfight,
    SentryZone,
    Execute,
    Rotation,
    Waveclear,
    Jungle,
    Objective,
};

enum class UltimatePurpose : std::uint8_t {
    None,
    CalibrumExecute,
    SeverumSurvive,
    GravitumCatch,
    GravitumPeel,
    InfernumTeamfight,
    CrescendumCommit,
    Objective,
    Manual,
};

enum class StateConfidence : std::uint8_t {
    Predicted,
    PairObserved,
    AmmoObserved,
    FullyObserved,
};

struct WeaponMark {
    int NetworkId = 0;
    int CalibrumExpireTick = 0;
    int GravitumExpireTick = 0;
    bool Calibrum = false;
    bool Gravitum = false;
};

struct SentryRecord {
    int NetworkId = 0;
    Vector3 Position = {};
    Weapon Offhand = Weapon::Unknown;
    int SpawnTick = 0;
    int ActiveUntil = 0;
    int IdleUntil = 0;
    bool Active = false;
};

struct QCastPlan {
    Weapon WeaponUsed = Weapon::Unknown;
    QPurpose Purpose = QPurpose::None;
    Vector3 Aim = {};
    int TargetId = 0;
    int HitCount = 0;
    float Score = -FLT_MAX;
    bool WillDeplete = false;
    bool ProjectileBlocked = false;
    bool Valid = false;
};

struct UltimatePlan {
    Vector3 Aim = {};
    Vector3 Explosion = {};
    Weapon WeaponUsed = Weapon::Unknown;
    UltimatePurpose Purpose = UltimatePurpose::None;
    int PrimaryId = 0;
    int HitCount = 0;
    int PriorityHits = 0;
    float Score = -FLT_MAX;
    bool RequiresSwap = false;
    bool ProjectileBlocked = false;
    bool Valid = false;
};

inline Menu* TacticsMenu = nullptr;
inline Menu* WeaponsMenu = nullptr;
inline Menu* AmmoMenu = nullptr;
inline Menu* CalibrumMenu = nullptr;
inline Menu* SeverumMenu = nullptr;
inline Menu* GravitumMenu = nullptr;
inline Menu* InfernumMenu = nullptr;
inline Menu* CrescendumMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline WeaponState State = {};
inline StateConfidence Confidence = StateConfidence::Predicted;
inline Sequence ActiveSequence = Sequence::None;
inline Posture CurrentPosture = Posture::Neutral;
inline QPurpose LastQPurpose = QPurpose::None;
inline UltimatePurpose LastUltimatePurpose = UltimatePurpose::None;
inline RotationPlan CurrentRotation = RotationPlan::BuildStandard;

inline std::array<int, 5> QReadyAt = {};
inline std::array<WeaponMark, 24> Marks = {};
inline std::array<SentryRecord, 8> Sentries = {};
inline int Chakrams = 0;
inline int ChakramExpireTick = 0;
inline int ReloadUntil = 0;
inline int IncomingWeaponUntil = 0;
inline int LastStateReconcileTick = 0;
inline int LastAmmoObservationTick = 0;
inline int LastWeaponChangeTick = 0;
inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastRCastTick = 0;
inline int LastAutoTick = 0;
inline int LastAutoTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastCombatTick = 0;
inline int QOffhandEffectUntil = 0;
inline int PendingRootTargetId = 0;
inline int PendingRootUntil = 0;
inline int PendingHandSwitchUntil = 0;
inline Weapon PendingHandWeapon = Weapon::Unknown;
inline int RequestedHandSwitchUntil = 0;
inline Weapon RequestedHandWeapon = Weapon::Unknown;
inline int PendingRUntil = 0;
inline Weapon PendingRWeapon = Weapon::Unknown;
inline UltimatePlan PendingRPlan = {};
inline Weapon PendingSentryOffhand = Weapon::Unknown;
inline int PendingSentryUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEnd = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;
inline int IncomingThreatUntil = 0;
inline bool IncomingHardCrowdControl = false;
inline float RecentIncomingPressure = 0.0f;
inline int QMissileNetworkId = 0;
inline int RMissileNetworkId = 0;
inline Vector3 RMissilePosition = {};
inline int CrescendumReturnUntil = 0;
inline float LastCrescendumAttackDistance = 0.0f;
inline QCastPlan LastQPlan = {};
inline UltimatePlan LastRPlan = {};

inline bool PlayerReloading() {
    const auto player = GameObjects::Player();
    return ReloadUntil > Now() ||
           (player.IsValid() && player.HasBuff("ApheliosPReload"));
}

inline bool IsCalibrumMarkName(const char* name) {
    return IsCalibrumMarkBuffName(name);
}

inline bool IsGravitumDebuffName(const char* name) {
    return Engine::TextContains(name, "apheliosgravitumdebuff");
}

inline bool IsReloadName(const char* name) {
    return Engine::TextContains(name, "apheliospreload");
}

inline Weapon OffhandFromBuffs(const AIHeroClient& player) {
    if (!player.IsValid()) return Weapon::Unknown;
    if (player.HasBuff("ApheliosOffHandBuffCalibrum")) return Weapon::Calibrum;
    if (player.HasBuff("ApheliosOffHandBuffSeverum")) return Weapon::Severum;
    if (player.HasBuff("ApheliosOffHandBuffGravitum")) return Weapon::Gravitum;
    if (player.HasBuff("ApheliosOffHandBuffInfernum")) return Weapon::Infernum;
    if (player.HasBuff("ApheliosOffHandBuffCrescendum")) return Weapon::Crescendum;
    return Weapon::Unknown;
}

inline Weapon MainFromRuntime(const AIHeroClient& player) {
    if (!player.IsValid()) return Weapon::Unknown;
    const auto q = player.Spellbook().GetSpell(SDK::SpellSlot::Q);
    if (!q.IsValid()) return Weapon::Unknown;
    Weapon result = WeaponFromRuntimeName(q.ScriptName().c_str());
    if (!IsWeapon(result)) result = WeaponFromRuntimeName(q.Name().c_str());
    if (!IsWeapon(result)) result = WeaponFromRuntimeName(q.IconName().c_str());
    return result;
}

inline std::array<Weapon, 3> RemainingWeapons(
    Weapon main,
    Weapon offhand,
    const std::array<Weapon, 5>& preferred) {
    std::array<Weapon, 3> result = {
        Weapon::Unknown, Weapon::Unknown, Weapon::Unknown,
    };
    int count = 0;
    auto insert = [&](Weapon weapon) {
        if (!IsWeapon(weapon) || weapon == main || weapon == offhand) return;
        for (int i = 0; i < count; ++i) {
            if (result[i] == weapon) return;
        }
        if (count < 3) result[count++] = weapon;
    };
    for (Weapon weapon : preferred) insert(weapon);
    for (Weapon weapon : AllWeapons) insert(weapon);
    return result;
}

inline void ReconcileObservedPair(Weapon main, Weapon offhand) {
    if (!IsWeapon(main) || !IsWeapon(offhand) || main == offhand) return;
    if (State.Main == main && State.Offhand == offhand) {
        Confidence = Confidence >= StateConfidence::AmmoObserved
            ? StateConfidence::FullyObserved
            : StateConfidence::PairObserved;
        return;
    }

    const auto previous = CycleOrder(State);
    State.Main = main;
    State.Offhand = offhand;
    State.Queue = RemainingWeapons(main, offhand, previous);
    State.QueueKnown = HasUniqueWeapons(State);
    LastWeaponChangeTick = Now();
    LastStateReconcileTick = Now();
    Confidence = State.QueueKnown
        ? StateConfidence::PairObserved
        : StateConfidence::Predicted;
}

inline int LiveQAmmo(const AIHeroClient& player) {
    if (!player.IsValid()) return -1;
    const auto q = player.Spellbook().GetSpell(SDK::SpellSlot::Q);
    if (!q.IsValid()) return -1;
    const int ammo = q.Ammo();
    const int maximum = q.MaxAmmo();
    // Ordinary charge spells often report MaxAmmo 1-3.  Accept only the
    // characteristic Moonlight-sized reservoir so those counters cannot
    // corrupt the five-gun model.
    if (maximum < 40 || maximum > 60 || ammo < 0 || ammo > maximum) return -1;
    return std::clamp(ammo, 0, kWeaponAmmo);
}

inline const char* ManagerBuffName(Weapon weapon) {
    switch (weapon) {
    case Weapon::Calibrum: return "ApheliosCalibrumManager";
    case Weapon::Severum: return "ApheliosSeverumManager";
    case Weapon::Gravitum: return "ApheliosGravitumManager";
    case Weapon::Infernum: return "ApheliosInfernumManager";
    case Weapon::Crescendum: return "ApheliosCrescendumManager";
    default: return "";
    }
}

inline const char* OffhandBuffName(Weapon weapon) {
    switch (weapon) {
    case Weapon::Calibrum: return "ApheliosOffHandBuffCalibrum";
    case Weapon::Severum: return "ApheliosOffHandBuffSeverum";
    case Weapon::Gravitum: return "ApheliosOffHandBuffGravitum";
    case Weapon::Infernum: return "ApheliosOffHandBuffInfernum";
    case Weapon::Crescendum: return "ApheliosOffHandBuffCrescendum";
    default: return "";
    }
}

inline int LiveBuffAmmo(const AIHeroClient& player, Weapon weapon) {
    if (!player.IsValid() || !IsWeapon(weapon)) return -1;
    const char* manager = ManagerBuffName(weapon);
    const char* offhand = OffhandBuffName(weapon);
    const bool managerPresent = manager[0] && player.HasBuff(manager);
    const bool offhandPresent = offhand[0] && player.HasBuff(offhand);
    return ObservedWeaponAmmo(
        managerPresent, managerPresent ? player.GetBuffCount(manager) : -1,
        offhandPresent, offhandPresent ? player.GetBuffCount(offhand) : -1);
}

inline void RefreshMarks() {
    const int now = Now();
    for (auto& mark : Marks) {
        if (mark.NetworkId == 0) continue;
        if (mark.Calibrum && mark.CalibrumExpireTick < now) {
            mark.Calibrum = false;
        }
        if (mark.Gravitum && mark.GravitumExpireTick < now) {
            mark.Gravitum = false;
        }
        if (!mark.Calibrum && !mark.Gravitum) mark = {};
    }
}

inline WeaponMark* FindMark(int networkId, bool create = false) {
    if (networkId == 0) return nullptr;
    WeaponMark* empty = nullptr;
    for (auto& mark : Marks) {
        if (mark.NetworkId == networkId) return &mark;
        if (!empty && mark.NetworkId == 0) empty = &mark;
    }
    if (!create) return nullptr;
    if (!empty) {
        empty = &*std::min_element(Marks.begin(), Marks.end(),
            [](const WeaponMark& left, const WeaponMark& right) {
                return std::max(left.CalibrumExpireTick,
                                left.GravitumExpireTick) <
                       std::max(right.CalibrumExpireTick,
                                right.GravitumExpireTick);
            });
    }
    *empty = {};
    empty->NetworkId = networkId;
    return empty;
}

inline bool HasCalibrumMark(const AIBaseClient& target) {
    if (!target.IsValid()) return false;
    if (target.HasBuff("ApheliosCalibrumBonusRangeBuff") ||
        target.HasBuff("ApheliosCalibrumBonusRangeDebuff")) return true;
    const auto* mark = FindMark(static_cast<int>(target.NetworkId()));
    return mark && mark->Calibrum &&
           mark->CalibrumExpireTick >= Now();
}

inline bool HasGravitumMark(const AIBaseClient& target) {
    if (!target.IsValid()) return false;
    if (target.HasBuff("ApheliosGravitumDebuff")) return true;
    const auto* mark = FindMark(static_cast<int>(target.NetworkId()));
    return mark && mark->Gravitum &&
           mark->GravitumExpireTick >= Now();
}

inline int CountGravitumTargets(float range = FLT_MAX,
                                int* priorityCount = nullptr) {
    const auto player = GameObjects::Player();
    int count = 0;
    int priority = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range) || !HasGravitumMark(enemy)) continue;
        ++count;
        if (enemy.HealthPercent() <= 45.0f || enemy.TotalAttackDamage() >= 190.0f ||
            enemy.AP() >= 220.0f) {
            ++priority;
        }
    }
    if (priorityCount) *priorityCount = priority;
    (void)player;
    return count;
}

inline void RefreshSentries() {
    const int now = Now();
    for (auto& sentry : Sentries) {
        if (sentry.NetworkId == 0) continue;
        if (sentry.IdleUntil < now && sentry.ActiveUntil < now) sentry = {};
        else if (sentry.ActiveUntil < now) sentry.Active = false;
    }
}

inline void RefreshChakrams(const AIHeroClient& player) {
    if (!player.IsValid()) return;
    const int live = ControllerHelpers::MaximumBuffCount(
        player,
        { "ApheliosCrescendumManager",
          "ApheliosCrescendumOrbitManager",
          "ApheliosChakram" });
    if (live > 0) {
        Chakrams = std::clamp(live, 0, 20);
        ChakramExpireTick = Now() + 5000;
    } else if (ChakramExpireTick < Now()) {
        Chakrams = 0;
    }
}

inline void RefreshWeaponState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const Weapon main = MainFromRuntime(player);
    const Weapon offhand = OffhandFromBuffs(player);
    const bool transitionObserved = PlayerReloading() &&
        Now() - LastWeaponChangeTick <= 1400 && main != State.Main;
    if (!transitionObserved && IsWeapon(main) && IsWeapon(offhand) &&
        main != offhand) {
        ReconcileObservedPair(main, offhand);
    } else if (!transitionObserved && IsWeapon(main) && main != State.Main) {
        if (main == State.Offhand && Now() - LastWCastTick <= 850) {
            SwapHands(State);
            LastWeaponChangeTick = Now();
        } else {
            State.Main = main;
            State.QueueKnown = false;
            Confidence = StateConfidence::Predicted;
        }
    }

    const bool liveReconcile = Bool(AmmoMenu, "LiveReconcile", true);
    int liveMainAmmo = liveReconcile ? LiveQAmmo(player) : -1;
    if (liveMainAmmo < 0 && liveReconcile) {
        liveMainAmmo = LiveBuffAmmo(player, State.Main);
    }
    const int liveOffhandAmmo = liveReconcile
        ? LiveBuffAmmo(player, State.Offhand) : -1;
    bool observedAmmo = false;
    if (liveMainAmmo >= 0 && IsWeapon(State.Main)) {
        SetAmmo(State, State.Main, liveMainAmmo, true);
        observedAmmo = true;
    }
    if (liveOffhandAmmo >= 0 && IsWeapon(State.Offhand)) {
        SetAmmo(State, State.Offhand, liveOffhandAmmo, true);
        observedAmmo = true;
    }
    if (observedAmmo) {
        LastAmmoObservationTick = Now();
        Confidence = Confidence == StateConfidence::PairObserved ||
                     Confidence == StateConfidence::FullyObserved
            ? StateConfidence::FullyObserved
            : StateConfidence::AmmoObserved;
    }
    RefreshChakrams(player);
}

inline float WeaponQCooldownSeconds(Weapon weapon, int championLevel) {
    const float breakpoint = static_cast<float>(LevelBreakpointIndex(championLevel));
    switch (weapon) {
    case Weapon::Calibrum:
    case Weapon::Severum:
        return 10.0f - breakpoint / 3.0f;
    case Weapon::Gravitum:
        return 12.0f - breakpoint / 3.0f;
    case Weapon::Infernum:
    case Weapon::Crescendum:
        return 9.0f - breakpoint * 0.5f;
    default:
        return 10.0f;
    }
}

inline bool QReadyFor(Weapon weapon) {
    const int index = WeaponIndex(weapon);
    if (index < 0) return false;
    if (weapon == State.Main) return Ready(0);
    return QReadyAt[index] <= Now();
}

inline Weapon IncomingWeapon() {
    return State.QueueKnown ? State.Queue[0] : Weapon::Unknown;
}

inline bool IsIncomingWeaponWindow() {
    return IncomingWeaponUntil > Now() || PlayerReloading();
}

inline void RecordAmmoSpend(int amount, bool fromAbility) {
    if (!IsWeapon(State.Main) || amount <= 0) return;
    const Weapon spent = State.Main;
    const auto transition = ConsumeMainAmmo(State, amount);
    if (!transition.Depleted) return;
    IncomingWeaponUntil = Now() + (fromAbility ? 1150 : 1050);
    ReloadUntil = IncomingWeaponUntil;
    LastWeaponChangeTick = Now();
    ActiveSequence = fromAbility && Ready(3)
        ? Sequence::IncomingWeaponCancel
        : Sequence::LowAmmoSwap;
    if (spent == Weapon::Crescendum && Chakrams > 0) {
        ChakramExpireTick = std::max(ChakramExpireTick, Now() + 3500);
    }
}

inline bool IsOrdinaryAmmoAttack(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.IsAutoAttack || args.IsSpecialAttack || !IsLocalPlayer(args.Sender)) {
        return false;
    }
    if (ControllerHelpers::SpellEventNameContainsAny(
            args, { "severumq", "crescendumturret", "infernumq" })) {
        return false;
    }
    const std::uint32_t id = args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId;
    const AIBaseClient target = ControllerHelpers::UnitByNetworkId(
        static_cast<int>(id));
    // The 1800-range Calibrum consume is an off-hand special attack and does
    // not spend ordinary main-hand moonlight.
    if (target.IsValid() && HasCalibrumMark(target)) return false;
    return true;
}

inline bool EnemyCanDashToSentry(const Vector3& position) {
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            enemy.Position().Distance2D(position) > 850.0f) continue;
        const SDK::ChampionId championId =
            ControllerHelpers::ChampionIdOf(enemy);
        if ((championId == SDK::ChampionId::Yasuo &&
             ControllerHelpers::EnemySpellReady(enemy, SDK::SpellSlot::E)) ||
            (championId == SDK::ChampionId::Samira &&
             ControllerHelpers::EnemySpellReady(enemy, SDK::SpellSlot::E)) ||
            (championId == SDK::ChampionId::Nilah &&
             ControllerHelpers::EnemySpellReady(enemy, SDK::SpellSlot::E))) {
            return true;
        }
    }
    return false;
}

inline bool TargetApproaching(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!Engine::ValidEnemy(target) || !player.IsValid()) return false;
    const Vector3 end = target.PathEnd();
    if (!end.IsValid() || end.IsZero()) return target.IsDashing();
    const Vector3 movement = Direction2D(target.Position(), end);
    const Vector3 toward = Direction2D(target.Position(), player.Position());
    return target.IsDashing() ||
           (!movement.IsZero() && !toward.IsZero() && movement.Dot(toward) >= 0.55f);
}

inline CombatContext BuildCombatContext(const AIHeroClient& target,
                                        Mode mode) {
    CombatContext context{};
    const auto player = GameObjects::Player();
    context.PlayerHealthPercent = player.IsValid()
        ? player.HealthPercent() : 100.0f;
    context.TargetDistance = Engine::ValidEnemy(target) && player.IsValid()
        ? player.Position().Distance2D(target.Position()) : 900.0f;
    context.NearbyEnemies = player.IsValid()
        ? Engine::CountEnemiesAt(player.Position(), 750.0f) : 0;
    context.GroupedEnemies = Engine::ValidEnemy(target)
        ? Engine::CountEnemiesAt(target.Position(), 430.0f) : 0;
    context.Chakrams = Chakrams;
    context.EnemyDiving = GapcloserExpireTick > Now() ||
        TargetApproaching(target) || IncomingThreatUntil > Now();
    context.NeedCatch = Engine::ValidEnemy(target) &&
        (target.IsDashing() || EnemyFlashReady(target) ||
         context.TargetDistance > 700.0f);
    context.NeedPeel = context.EnemyDiving &&
        (context.PlayerHealthPercent < 62.0f || context.NearbyEnemies >= 2);
    context.ObjectiveSoon = ControllerHelpers::HasNearbyEpicMonster(1800.0f);
    context.ObjectiveActive = ControllerHelpers::HasNearbyEpicMonster(900.0f);
    context.OpenMap = mode == Mode::Combo &&
        context.TargetDistance > 650.0f && context.GroupedEnemies <= 1;
    context.CanCommitClose = context.TargetDistance <= 520.0f &&
        context.NearbyEnemies <= std::max(1,
            player.IsValid() ? player.CountAllyHeroesInRange(750.0f) + 1 : 1) &&
        context.PlayerHealthPercent >= 38.0f;
    context.TargetEscaping = Engine::ValidEnemy(target) &&
        !TargetApproaching(target) && context.TargetDistance > 550.0f;
    context.ProjectileWall = Engine::ValidEnemy(target) &&
        ProjectileWallBlocksFromPlayer(target.Position(), 55.0f);
    context.PlayerChasingReturn = Engine::ValidEnemy(target) &&
        CursorDirectionAgrees(target.Position(), -0.05f);
    context.EarlyGame = player.IsValid() && player.Level() <= 9;
    return context;
}

inline float TargetPriority(const AIHeroClient& enemy) {
    if (!Engine::ValidEnemy(enemy)) return 0.0f;
    float score = 1.0f + enemy.TotalAttackDamage() * 0.003f +
                  enemy.AP() * 0.0025f;
    if (enemy.HealthPercent() <= 35.0f) score += 0.7f;
    if (enemy.IsDashing()) score += 0.45f;
    return score;
}

inline float PhysicalDamage(const AIBaseClient& target, float raw) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid()
        ? player.CalculatePhysicalDamage(target, std::max(0.0f, raw))
        : 0.0f;
}

inline float CurrentQDamage(const AIBaseClient& target, Weapon weapon) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const int level = player.Level();
    float raw = 0.0f;
    switch (weapon) {
    case Weapon::Calibrum:
        raw = CalibrumQRawDamage(level, player.BonusAttackDamage(), player.AP()) +
              CalibrumMarkRawDamage(player.BonusAttackDamage());
        break;
    case Weapon::Severum:
        raw = SeverumQPerHitRawDamage(level, player.TotalAttackDamage()) *
              static_cast<float>(SeverumQAttackCount(
                  std::max(0.0f, player.AttackSpeedMod() - 1.0f) * 100.0f));
        break;
    case Weapon::Gravitum:
        raw = GravitumQRawDamage(level, player.BonusAttackDamage(), player.AP());
        break;
    case Weapon::Infernum:
        raw = InfernumQRawDamage(level, player.BonusAttackDamage(), player.AP()) +
              player.TotalAttackDamage();
        break;
    case Weapon::Crescendum:
        raw = CrescendumSentryRawDamage(
            level, player.BonusAttackDamage(), player.AP()) * 2.0f;
        break;
    default:
        break;
    }
    return PhysicalDamage(target, raw);
}

inline float UltimateDamage(const AIBaseClient& target,
                            Weapon weapon,
                            int hitCount) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const int rank = std::clamp(SpellRank(3), 0, 3);
    float raw = MoonlightVigilRawDamage(
        rank, player.BonusAttackDamage(), player.AP()) +
        player.TotalAttackDamage();
    if (weapon == Weapon::Infernum) {
        raw += UltimateWeaponBonus(
            weapon, std::max(1, rank), player.BonusAttackDamage());
        raw += std::max(0, hitCount - 1) * player.TotalAttackDamage() * 0.35f;
    } else if (weapon == Weapon::Calibrum) {
        raw += CalibrumMarkRawDamage(
            player.BonusAttackDamage(), 1,
            UltimateWeaponBonus(weapon, std::max(1, rank),
                                player.BonusAttackDamage()));
    }
    return PhysicalDamage(target, raw);
}

inline bool CanSpendQ(QPurpose purpose,
                      const CombatContext& context,
                      bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !IsWeapon(State.Main) || !Ready(0) ||
        !QReadyFor(State.Main) || PlayerReloading() ||
        player.Mana() + 0.01f < SpellCost(0)) {
        return false;
    }
    if (!reactive && Engine::ShouldPreserveAttack(
            Engine::ResolvedSpecs[0], StepRule::None)) {
        return false;
    }
    if (!CastThrottleReady(0, reactive)) return false;
    const bool maintenance = purpose == QPurpose::Rotation ||
                             purpose == QPurpose::Waveclear ||
                             purpose == QPurpose::Jungle;
    if (maintenance && ShouldHoldWeapon(
            State.Main, context, AmmoOf(State, State.Main))) {
        return false;
    }
    return AmmoOf(State, State.Main) > 0;
}

inline bool CommitQPlan(const QCastPlan& plan,
                        const CombatContext& context,
                        bool reactive = false) {
    if (!plan.Valid || plan.WeaponUsed != State.Main ||
        !CanSpendQ(plan.Purpose, context, reactive)) {
        return false;
    }
    bool cast = false;
    switch (plan.WeaponUsed) {
    case Weapon::Calibrum:
    case Weapon::Infernum:
    case Weapon::Crescendum:
        cast = Engine::ControllerCastPosition(0, plan.Aim);
        break;
    case Weapon::Severum:
    case Weapon::Gravitum:
        cast = Engine::ControllerCastSelf(0);
        break;
    default:
        break;
    }
    if (!cast) return false;
    LastQPlan = plan;
    LastQPurpose = plan.Purpose;
    if (plan.WeaponUsed == Weapon::Crescendum) {
        PendingSentryOffhand = State.Offhand;
        PendingSentryUntil = Now() + 750;
    }
    if (plan.WillDeplete) {
        ActiveSequence = Sequence::LowAmmoSwap;
    }
    return true;
}

inline QCastPlan BuildCalibrumPlan(const AIHeroClient& target,
                                   QPurpose purpose,
                                   bool reactive = false) {
    QCastPlan plan{};
    plan.WeaponUsed = Weapon::Calibrum;
    plan.Purpose = purpose;
    const auto player = GameObjects::Player();
    if (State.Main != Weapon::Calibrum ||
        !Engine::ValidEnemy(target, kCalibrumQRange + 100.0f) ||
        IsCommonUntargetableOrImmune(target) ||
        !Engine::RuntimeSpells[0]) {
        return plan;
    }
    const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(target);
    Vector3 aim = prediction.GetCastPosition();
    if (!aim.IsValid() || aim.IsZero()) aim = prediction.GetUnitPosition();
    if (!aim.IsValid() || aim.IsZero()) aim = target.Position();
    if (player.Position().Distance2D(aim) >
        kCalibrumQRange + target.BoundingRadius()) {
        return plan;
    }
    const bool committed = TargetApproaching(target) || target.IsDashing() ||
                           Engine::IsHardCrowdControlled(target);
    SDK::HitChance required = reactive || committed
        ? SDK::HitChance::High : SDK::HitChance::VeryHigh;
    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo && !reactive && !committed) {
        required = SDK::HitChance::High;
    }
    if (!PredictionAtLeast(prediction, required) && !committed) return plan;
    if (!reactive && !CursorDirectionAgrees(aim, -0.12f) &&
        purpose != QPurpose::Execute && purpose != QPurpose::MarkChain &&
        Orbwalker::ActiveMode() != OrbwalkingMode::Combo) {
        return plan;
    }
    plan.ProjectileBlocked = ProjectileWallBlocksFromPlayer(aim, 34.0f);
    if (plan.ProjectileBlocked) return plan;
    const bool shield = ControllerHelpers::HasSpellShieldOrImmunity(target);
    if (shield && !Bool(CalibrumMenu, "BreakShield", false) &&
        purpose != QPurpose::Peel && purpose != QPurpose::Interrupt) {
        return plan;
    }
    if (HasCalibrumMark(target) &&
        purpose != QPurpose::MarkChain && purpose != QPurpose::Execute &&
        Bool(CalibrumMenu, "ConsumeBeforeRefresh", true)) {
        return plan;
    }

    const float distance = player.Position().Distance2D(target.Position());
    plan.Aim = aim;
    plan.TargetId = static_cast<int>(target.NetworkId());
    plan.HitCount = 1;
    plan.Score = TargetPriority(target) * 2.2f +
                 (distance > 650.0f ? 2.3f : 0.4f) +
                 (purpose == QPurpose::Execute ? 3.0f : 0.0f) +
                 (purpose == QPurpose::MarkChain ? 1.8f : 0.0f) -
                 (shield ? 1.3f : 0.0f);
    plan.WillDeplete = LowAmmoAbilitySwaps(AmmoOf(State, State.Main));
    plan.Valid = true;
    return plan;
}

inline QCastPlan BuildSeverumPlan(const AIHeroClient& target,
                                  QPurpose purpose,
                                  const CombatContext& context,
                                  bool reactive = false) {
    QCastPlan plan{};
    plan.WeaponUsed = Weapon::Severum;
    plan.Purpose = purpose;
    const auto player = GameObjects::Player();
    if (State.Main != Weapon::Severum || !player.IsValid() ||
        player.HasBuff("ApheliosSeverumQ")) {
        return plan;
    }
    const bool validTarget = Engine::ValidEnemy(target, 610.0f);
    int nearbyUnits = 0;
    for (const auto& unit : GameObjects::EnemyMinions()) {
        if (unit.IsValid() && !unit.IsDead() && unit.IsTargetable() &&
            player.Position().Distance2D(unit.Position()) <= 560.0f) {
            ++nearbyUnits;
        }
    }
    const bool survival = player.HealthPercent() <=
            static_cast<float>(Slider(SeverumMenu, "SurvivalHp", 48)) ||
        context.EnemyDiving || context.ProjectileWall ||
        purpose == QPurpose::Sustain || purpose == QPurpose::Peel ||
        purpose == QPurpose::AntiGapcloser;
    const bool chakramBuild = State.Offhand == Weapon::Crescendum &&
        (validTarget || nearbyUnits > 0) &&
        (context.CanCommitClose || purpose == QPurpose::ChakramBuild);
    const bool rootSetup = State.Offhand == Weapon::Gravitum && validTarget &&
        (context.NeedCatch || context.NeedPeel);
    const bool rotationBurn = purpose == QPurpose::Rotation &&
        nearbyUnits > 0 && Engine::CountEnemiesAt(player.Position(), 900.0f) == 0;
    if (!survival && !chakramBuild && !rootSetup && !rotationBurn &&
        !(validTarget && purpose == QPurpose::AllIn)) {
        return plan;
    }
    if (!validTarget && nearbyUnits == 0) return plan;
    if (!reactive && validTarget && player.IsUnderEnemyTurret() &&
        !target.IsUnderEnemyTurret()) {
        return plan;
    }
    // Late-game Onslaught can lose damage to ordinary autos. Require a real
    // sustain, off-hand, movement or rotation payoff when attack speed is high.
    if (Bool(SeverumMenu, "RespectLateAutoDps", true) &&
        !survival && !rootSetup && !chakramBuild &&
        player.AttackSpeedMod() >= 1.75f && InAutoAttackRange(target)) {
        return plan;
    }

    plan.TargetId = validTarget ? static_cast<int>(target.NetworkId()) : 0;
    plan.HitCount = validTarget ? 1 : nearbyUnits;
    plan.Score = (survival ? 4.4f : 0.0f) +
                 (chakramBuild ? 4.0f : 0.0f) +
                 (rootSetup ? 3.8f : 0.0f) +
                 (rotationBurn ? 1.2f : 0.0f);
    plan.WillDeplete = LowAmmoAbilitySwaps(AmmoOf(State, State.Main));
    plan.Valid = true;
    return plan;
}

inline QCastPlan BuildGravitumPlan(QPurpose purpose,
                                   const AIHeroClient& preferred,
                                   bool reactive = false) {
    QCastPlan plan{};
    plan.WeaponUsed = Weapon::Gravitum;
    plan.Purpose = purpose;
    if (State.Main != Weapon::Gravitum) return plan;
    int priority = 0;
    const int marked = CountGravitumTargets(kCalibrumMarkRange, &priority);
    if (marked <= 0) return plan;
    const bool preferredMarked = Engine::ValidEnemy(preferred) &&
                                 HasGravitumMark(preferred);
    const bool urgent = reactive || purpose == QPurpose::Interrupt ||
                        purpose == QPurpose::AntiGapcloser ||
                        purpose == QPurpose::Peel;
    const int proactiveMinimum = Slider(
        GravitumMenu, "ProactiveTargets", 2);
    if (!urgent && !preferredMarked && marked < proactiveMinimum) return plan;
    if (!urgent && marked < proactiveMinimum &&
        Bool(GravitumMenu, "HoldSingle", true) &&
        (!Engine::ValidEnemy(preferred) || !TargetApproaching(preferred))) {
        return plan;
    }
    if (preferredMarked &&
        ControllerHelpers::HasSpellShieldOrImmunity(preferred) &&
        marked == 1 && !Bool(GravitumMenu, "RootShield", false)) {
        return plan;
    }
    plan.TargetId = preferredMarked
        ? static_cast<int>(preferred.NetworkId()) : 0;
    plan.HitCount = marked;
    plan.Score = GravitumRootValue(
        marked, priority, purpose == QPurpose::Interrupt,
        purpose == QPurpose::Peel || purpose == QPurpose::AntiGapcloser);
    plan.WillDeplete = LowAmmoAbilitySwaps(AmmoOf(State, State.Main));
    plan.Valid = true;
    return plan;
}

inline std::vector<AreaUnit> BuildAreaUnits(float delaySeconds,
                                            int primaryId) {
    std::vector<AreaUnit> units;
    units.reserve(GameObjects::EnemyHeroes().size());
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1600.0f)) continue;
        units.push_back({
            ControllerHelpers::PredictPosition(enemy, delaySeconds),
            enemy.BoundingRadius(),
            TargetPriority(enemy),
            static_cast<int>(enemy.NetworkId()) == primaryId,
            enemy.IsDashing(),
            Engine::IsHardCrowdControlled(enemy),
            ControllerHelpers::HasSpellShieldOrImmunity(enemy),
            true,
        });
    }
    return units;
}

inline QCastPlan BuildInfernumPlan(const AIHeroClient& target,
                                   QPurpose purpose,
                                   bool reactive = false) {
    QCastPlan plan{};
    plan.WeaponUsed = Weapon::Infernum;
    plan.Purpose = purpose;
    const auto player = GameObjects::Player();
    if (State.Main != Weapon::Infernum || !player.IsValid() ||
        !Engine::ValidEnemy(target, kInfernumQRange + 120.0f)) {
        return plan;
    }
    const int targetId = static_cast<int>(target.NetworkId());
    const auto units = BuildAreaUnits(0.40f, targetId);
    std::vector<Vector3> candidates;
    candidates.reserve(units.size() * 2 + 2);
    for (const auto& unit : units) {
        if (unit.Valid) candidates.push_back(unit.Position);
    }
    for (std::size_t i = 0; i < units.size(); ++i) {
        for (std::size_t j = i + 1; j < units.size(); ++j) {
            if (units[i].Position.Distance2D(units[j].Position) <= 650.0f) {
                candidates.push_back(
                    (units[i].Position + units[j].Position) * 0.5f);
            }
        }
    }
    candidates.push_back(target.Position());

    for (const Vector3& candidate : candidates) {
        const Vector3 direction = Direction2D(player.Position(), candidate);
        if (direction.IsZero()) continue;
        const Vector3 aim = player.Position() + direction * kInfernumQRange;
        float score = InfernumQScore(player.Position(), aim, units);
        int hits = 0;
        bool primary = false;
        for (const auto& unit : units) {
            if (!unit.Valid || !InfernumQHits(
                    player.Position(), aim, unit.Position, unit.Radius)) continue;
            ++hits;
            primary = primary || unit.Primary;
        }
        if (primary) score += 1.6f;
        if (purpose == QPurpose::Teamfight) score += hits * 0.45f;
        if (purpose == QPurpose::Execute && primary) score += 2.3f;
        if (score > plan.Score) {
            plan.Aim = aim;
            plan.TargetId = targetId;
            plan.HitCount = hits;
            plan.Score = score;
            plan.Valid = primary || (hits >= 2 && purpose == QPurpose::Teamfight);
        }
    }
    const int minimum = reactive ? 1 :
        (purpose == QPurpose::Teamfight
            ? Slider(InfernumMenu, "ComboTargets", 2) : 1);
    if (plan.HitCount < minimum) plan.Valid = false;
    if (!reactive && !CursorDirectionAgrees(plan.Aim, -0.18f) &&
        purpose != QPurpose::Teamfight && purpose != QPurpose::Execute &&
        Orbwalker::ActiveMode() != OrbwalkingMode::Combo) {
        plan.Valid = false;
    }
    plan.WillDeplete = LowAmmoAbilitySwaps(AmmoOf(State, State.Main));
    return plan;
}

inline int CountEnemiesAtSentry(const Vector3& position) {
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) &&
            position.Distance2D(ControllerHelpers::PredictPosition(enemy, 0.5f)) <=
                kCrescendumSentryAttackRange + enemy.BoundingRadius()) {
            ++count;
        }
    }
    return count;
}

inline Vector3 ClampFromPlayer(const Vector3& position, float range) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !position.IsValid() || position.IsZero()) return {};
    const float distance = player.Position().Distance2D(position);
    if (distance <= range) return position;
    const Vector3 direction = Direction2D(player.Position(), position);
    return direction.IsZero() ? Vector3{} : player.Position() + direction * range;
}

inline QCastPlan BuildCrescendumPlan(const AIHeroClient& target,
                                     QPurpose purpose,
                                     const CombatContext& context,
                                     bool reactive = false) {
    QCastPlan plan{};
    plan.WeaponUsed = Weapon::Crescendum;
    plan.Purpose = purpose;
    const auto player = GameObjects::Player();
    if (State.Main != Weapon::Crescendum || !player.IsValid()) return plan;
    const bool validTarget = Engine::ValidEnemy(target, 1100.0f);
    if (!validTarget && purpose != QPurpose::Objective &&
        purpose != QPurpose::Rotation && purpose != QPurpose::Jungle) {
        return plan;
    }

    std::vector<Vector3> candidates;
    if (validTarget) {
        const Vector3 predicted = ControllerHelpers::PredictPosition(target, 0.55f);
        candidates.push_back(ClampFromPlayer(predicted, kCrescendumSentryRange));
        const Vector3 towardPlayer = Direction2D(predicted, player.Position());
        if (!towardPlayer.IsZero()) {
            candidates.push_back(ClampFromPlayer(
                predicted + towardPlayer * 180.0f, kCrescendumSentryRange));
        }
        const Vector3 pathEnd = target.PathEnd();
        const Vector3 path = Direction2D(target.Position(), pathEnd);
        if (!path.IsZero()) {
            candidates.push_back(ClampFromPlayer(
                predicted + path * 170.0f, kCrescendumSentryRange));
        }
    }
    if (context.ObjectiveActive) {
        const auto epic = SelectJungleTarget(1200.0f);
        if (epic.IsValid()) {
            candidates.push_back(ClampFromPlayer(
                epic.Position(), kCrescendumSentryRange));
        }
    }
    candidates.push_back(ClampFromPlayer(Game::CursorPos(), kCrescendumSentryRange));

    for (const Vector3& candidate : candidates) {
        if (!candidate.IsValid() || candidate.IsZero()) continue;
        SentryContext sentry{};
        sentry.Player = player.Position();
        sentry.Position = candidate;
        sentry.PredictedTarget = validTarget
            ? ControllerHelpers::PredictPosition(target, 0.55f) : Vector3{};
        sentry.TargetRadius = validTarget ? target.BoundingRadius() : 0.0f;
        sentry.ExpectedTargets = CountEnemiesAtSentry(candidate);
        sentry.ObjectiveChoke = context.ObjectiveActive;
        sentry.BushOrFogEdge = validTarget && !target.IsVisible();
        sentry.UnderEnemyTurret = Engine::UnderEnemyTurret(candidate);
        sentry.GivesEnemyDashTarget = EnemyCanDashToSentry(candidate);
        sentry.PlayerFleeing = purpose == QPurpose::Peel ||
                              purpose == QPurpose::AntiGapcloser;
        sentry.RetreatDirection = Direction2D(
            validTarget ? target.Position() : Game::CursorPos(),
            player.Position());
        sentry.CalibrumOffhand = State.Offhand == Weapon::Calibrum;
        sentry.GravitumOffhand = State.Offhand == Weapon::Gravitum;
        float score = SentryPlacementScore(sentry);
        if (purpose == QPurpose::Objective) score += 2.0f;
        if (purpose == QPurpose::SentryZone) score += 1.2f;
        if (purpose == QPurpose::Peel) score += 1.0f;
        if (score > plan.Score) {
            plan.Aim = candidate;
            plan.TargetId = validTarget
                ? static_cast<int>(target.NetworkId()) : 0;
            plan.HitCount = sentry.ExpectedTargets;
            plan.Score = score;
            plan.Valid = score >= (reactive ? 1.6f : 3.0f);
        }
    }
    plan.WillDeplete = LowAmmoAbilitySwaps(AmmoOf(State, State.Main));
    return plan;
}

inline QCastPlan BuildCurrentQPlan(const AIHeroClient& target,
                                   QPurpose purpose,
                                   const CombatContext& context,
                                   bool reactive = false) {
    switch (State.Main) {
    case Weapon::Calibrum:
        return BuildCalibrumPlan(target, purpose, reactive);
    case Weapon::Severum:
        return BuildSeverumPlan(target, purpose, context, reactive);
    case Weapon::Gravitum:
        return BuildGravitumPlan(purpose, target, reactive);
    case Weapon::Infernum:
        return BuildInfernumPlan(target, purpose, reactive);
    case Weapon::Crescendum:
        return BuildCrescendumPlan(target, purpose, context, reactive);
    default:
        return {};
    }
}

inline UltimatePurpose PurposeForUltimate(Weapon weapon,
                                          const UltimateContext& context) {
    switch (weapon) {
    case Weapon::Calibrum: return UltimatePurpose::CalibrumExecute;
    case Weapon::Severum: return UltimatePurpose::SeverumSurvive;
    case Weapon::Gravitum:
        return context.NeedPeel
            ? UltimatePurpose::GravitumPeel
            : UltimatePurpose::GravitumCatch;
    case Weapon::Infernum:
        return context.ObjectiveFight
            ? UltimatePurpose::Objective
            : UltimatePurpose::InfernumTeamfight;
    case Weapon::Crescendum: return UltimatePurpose::CrescendumCommit;
    default: return UltimatePurpose::None;
    }
}

struct UltimateCandidateUnit {
    AIHeroClient Hero = {};
    Vector3 Position = {};
    float Radius = 0.0f;
    float Priority = 0.0f;
    bool Primary = false;
    bool Valid = false;
};

inline std::vector<UltimateCandidateUnit> BuildUltimateUnits(int primaryId) {
    std::vector<UltimateCandidateUnit> result;
    result.reserve(GameObjects::EnemyHeroes().size());
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kMoonlightVigilRange + 350.0f) ||
            IsCommonUntargetableOrImmune(enemy)) {
            continue;
        }
        result.push_back({
            enemy,
            ControllerHelpers::PredictPosition(enemy, 0.82f),
            enemy.BoundingRadius(),
            TargetPriority(enemy),
            static_cast<int>(enemy.NetworkId()) == primaryId,
            true,
        });
    }
    return result;
}

inline UltimatePlan ScoreUltimateAim(
    const Vector3& aimCandidate,
    Weapon weapon,
    int primaryId,
    const CombatContext& combat,
    const std::vector<UltimateCandidateUnit>& units) {
    UltimatePlan plan{};
    plan.WeaponUsed = weapon;
    plan.PrimaryId = primaryId;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !aimCandidate.IsValid() || aimCandidate.IsZero()) {
        return plan;
    }
    const Vector3 direction = Direction2D(player.Position(), aimCandidate);
    if (direction.IsZero()) return plan;
    plan.Aim = player.Position() + direction * kMoonlightVigilRange;
    plan.ProjectileBlocked = ProjectileWallBlocksFromPlayer(plan.Aim, 58.0f);
    if (plan.ProjectileBlocked) return plan;

    float firstT = FLT_MAX;
    const UltimateCandidateUnit* first = nullptr;
    for (const auto& unit : units) {
        if (!unit.Valid) continue;
        const auto projection = SharedGeometry::ProjectPointToSegment2D(
            unit.Position, player.Position(), plan.Aim);
        if (projection.Distance <= kMoonlightVigilWidth * 0.5f + unit.Radius &&
            projection.T < firstT) {
            firstT = projection.T;
            first = &unit;
        }
    }
    if (!first) return plan;
    plan.Explosion = first->Position;

    bool containsPrimary = false;
    bool shieldedPrimary = false;
    for (const auto& unit : units) {
        if (!unit.Valid || !MoonlightVigilExplosionHits(
                plan.Explosion, unit.Position, unit.Radius)) {
            continue;
        }
        ++plan.HitCount;
        if (unit.Priority >= 1.7f || unit.Primary) ++plan.PriorityHits;
        containsPrimary = containsPrimary || unit.Primary;
        if (unit.Primary) {
            shieldedPrimary = ControllerHelpers::HasSpellShieldOrImmunity(
                unit.Hero);
        }
    }

    const AIHeroClient primary = HeroByNetworkId(primaryId);
    UltimateContext context{};
    context.PlayerHealthPercent = player.HealthPercent();
    context.TargetHealthPercent = Engine::ValidEnemy(primary)
        ? primary.HealthPercent() : 100.0f;
    context.TargetDistance = Engine::ValidEnemy(primary)
        ? player.Position().Distance2D(primary.Position())
        : player.Position().Distance2D(plan.Explosion);
    context.HitCount = plan.HitCount;
    context.PriorityHits = plan.PriorityHits;
    context.Chakrams = Chakrams;
    context.GravitumMarked = CountGravitumTargets();
    context.EnemyDiving = combat.EnemyDiving;
    context.NeedPeel = combat.NeedPeel;
    context.NeedCatch = combat.NeedCatch;
    context.CanFollowClose = combat.CanCommitClose;
    context.TargetEscaping = combat.TargetEscaping;
    context.ObjectiveFight = combat.ObjectiveActive;
    context.ProjectileWall = plan.ProjectileBlocked;
    context.SpellShieldedPrimary = shieldedPrimary;
    plan.Purpose = PurposeForUltimate(weapon, context);
    plan.Score = UltimateVariantScore(weapon, context) +
                 (containsPrimary ? 1.8f : -2.0f);
    if (weapon == Weapon::Infernum && plan.HitCount >= 3) plan.Score += 2.5f;
    if (weapon == Weapon::Gravitum &&
        (combat.NeedCatch || combat.NeedPeel)) plan.Score += 1.8f;
    if (weapon == Weapon::Severum &&
        player.HealthPercent() <= 28.0f) plan.Score += 2.4f;
    if (weapon == Weapon::Crescendum && Chakrams >= 8) plan.Score -= 1.5f;
    plan.Valid = plan.HitCount > 0 &&
        (containsPrimary || plan.HitCount >= 2 || combat.NeedPeel);
    return plan;
}

inline UltimatePlan BuildUltimatePlan(const AIHeroClient& target,
                                      const CombatContext& combat,
                                      bool defensive = false) {
    UltimatePlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3) || SpellRank(3) <= 0 ||
        player.Mana() + 0.01f < SpellCost(3)) {
        return best;
    }
    int primaryId = Engine::ValidEnemy(target)
        ? static_cast<int>(target.NetworkId()) : 0;
    if (primaryId == 0 && GapcloserExpireTick > Now()) {
        primaryId = GapcloserTargetId;
    }
    const auto units = BuildUltimateUnits(primaryId);
    if (units.empty()) return best;
    std::vector<Vector3> candidates;
    candidates.reserve(units.size() * 2);
    for (const auto& unit : units) candidates.push_back(unit.Position);
    for (std::size_t i = 0; i < units.size(); ++i) {
        for (std::size_t j = i + 1; j < units.size(); ++j) {
            if (units[i].Position.Distance2D(units[j].Position) <=
                kMoonlightVigilRadius * 2.0f +
                    units[i].Radius + units[j].Radius) {
                candidates.push_back(
                    (units[i].Position + units[j].Position) * 0.5f);
            }
        }
    }

    for (Weapon weapon : { State.Main, State.Offhand }) {
        if (!IsWeapon(weapon)) continue;
        if (defensive && weapon != Weapon::Severum &&
            weapon != Weapon::Gravitum) {
            continue;
        }
        for (const Vector3& candidate : candidates) {
            UltimatePlan plan = ScoreUltimateAim(
                candidate, weapon, primaryId, combat, units);
            if (!plan.Valid) continue;
            plan.RequiresSwap = weapon != State.Main;
            if (plan.RequiresSwap) plan.Score -= 0.65f;
            if (plan.Score > best.Score) best = plan;
        }
    }
    if (!best.Valid) return best;
    const int minimumHits = best.Purpose == UltimatePurpose::InfernumTeamfight ||
            best.Purpose == UltimatePurpose::Objective
        ? Slider(UltimateMenu, "InfernumTargets", 2) : 1;
    const float threshold = static_cast<float>(Slider(
        UltimateMenu,
        defensive ? "DefensiveScore" : "MinimumScore",
        defensive ? 520 : 680)) / 100.0f;
    if (best.HitCount < minimumHits || best.Score < threshold) {
        best.Valid = false;
    }
    return best;
}

inline bool CastPhaseTo(Weapon desired,
                        Sequence sequence,
                        bool reactive = false) {
    if (!IsWeapon(desired) || desired != State.Offhand || !Ready(1) ||
        PlayerReloading() || !CastThrottleReady(1, reactive)) {
        return false;
    }
    if (!reactive && Engine::ShouldPreserveAttack(
            Engine::ResolvedSpecs[1], StepRule::None)) {
        return false;
    }
    PendingHandWeapon = desired;
    PendingHandSwitchUntil = Now() + 800;
    ActiveSequence = sequence;
    if (!Engine::ControllerCastSelf(1)) {
        PendingHandWeapon = Weapon::Unknown;
        PendingHandSwitchUntil = 0;
        return false;
    }
    return true;
}

inline bool CastUltimateNow(const UltimatePlan& plan,
                            bool reactive = false) {
    const bool incomingCancel = PlayerReloading() &&
        Bool(AmmoMenu, "CancelIncomingWithR", true) &&
        (ActiveSequence == Sequence::IncomingWeaponCancel ||
         ActiveSequence == Sequence::LowAmmoSwap);
    if (!plan.Valid || plan.WeaponUsed != State.Main || !Ready(3) ||
        (PlayerReloading() && !incomingCancel) ||
        !CastThrottleReady(3, reactive || incomingCancel)) {
        return false;
    }
    if (!reactive && !incomingCancel && Engine::ShouldPreserveAttack(
            Engine::ResolvedSpecs[3], StepRule::None)) {
        return false;
    }
    if (!Engine::ControllerCastPosition(3, plan.Aim)) return false;
    LastRPlan = plan;
    LastUltimatePurpose = plan.Purpose;
    PendingRUntil = 0;
    PendingRWeapon = Weapon::Unknown;
    PendingRPlan = {};
    switch (plan.Purpose) {
    case UltimatePurpose::SeverumSurvive:
    case UltimatePurpose::GravitumPeel:
        ActiveSequence = Sequence::DefensiveMoonlight;
        break;
    case UltimatePurpose::InfernumTeamfight:
    case UltimatePurpose::Objective:
        ActiveSequence = Sequence::InfernumMoonlight;
        break;
    case UltimatePurpose::CrescendumCommit:
        ActiveSequence = Sequence::CrescendumCloseDps;
        break;
    case UltimatePurpose::CalibrumExecute:
        ActiveSequence = Sequence::CalibrumMarkChain;
        break;
    default:
        ActiveSequence = Sequence::HiddenWeaponUltimate;
        break;
    }
    return true;
}

inline bool CommitUltimatePlan(const UltimatePlan& plan,
                               bool reactive = false) {
    if (!plan.Valid) return false;
    if (plan.RequiresSwap) {
        PendingRPlan = plan;
        PendingRPlan.RequiresSwap = false;
        PendingRWeapon = plan.WeaponUsed;
        PendingRUntil = Now() + 1200;
        if (!CastPhaseTo(plan.WeaponUsed,
                         Sequence::HiddenWeaponUltimate, reactive)) {
            PendingRPlan = {};
            PendingRWeapon = Weapon::Unknown;
            PendingRUntil = 0;
            return false;
        }
        return true;
    }
    return CastUltimateNow(plan, reactive);
}

inline bool TryPendingUltimate() {
    if (PendingRUntil <= Now() || !PendingRPlan.Valid ||
        !IsWeapon(PendingRWeapon)) {
        PendingRUntil = 0;
        PendingRWeapon = Weapon::Unknown;
        PendingRPlan = {};
        return false;
    }
    if (State.Main != PendingRWeapon || Now() - LastWCastTick < 35) {
        return false;
    }
    PendingRPlan.WeaponUsed = State.Main;
    return CastUltimateNow(PendingRPlan, true);
}

inline Weapon DesiredMarkOffhand(const AIHeroClient& target,
                                 const CombatContext& context) {
    if (!HasCalibrumMark(target)) return Weapon::Unknown;
    if (context.NeedCatch && PairContains(State, Weapon::Gravitum)) {
        return Weapon::Gravitum;
    }
    if (context.CanCommitClose && PairContains(State, Weapon::Crescendum)) {
        return Weapon::Crescendum;
    }
    if (context.GroupedEnemies >= 2 && PairContains(State, Weapon::Infernum)) {
        return Weapon::Infernum;
    }
    if (context.PlayerHealthPercent < 45.0f &&
        PairContains(State, Weapon::Severum)) {
        return Weapon::Severum;
    }
    return State.Offhand;
}

inline bool TrySmartHandSwap(const AIHeroClient& target,
                             const CombatContext& context,
                             bool reactive = false) {
    if (!IsWeapon(State.Main) || !IsWeapon(State.Offhand) ||
        !Ready(1) || PlayerReloading()) return false;
    if (PendingHandSwitchUntil > Now() || PendingRUntil > Now()) return false;

    Weapon desired = State.Offhand;
    const bool requested = RequestedHandSwitchUntil > Now() &&
                           RequestedHandWeapon == State.Offhand;
    float mainScore = WeaponTacticalScore(State.Main, context);
    float offScore = WeaponTacticalScore(State.Offhand, context);
    if (QReadyFor(State.Offhand)) offScore += 0.7f;
    if (!QReadyFor(State.Main)) offScore += 0.45f;
    if (Engine::ValidEnemy(target) && HasCalibrumMark(target)) {
        const Weapon markOffhand = DesiredMarkOffhand(target, context);
        // Mark attacks fire the current off-hand.  If the desired effect is
        // already off-hand, preserve it; if it is main-hand, Phase first.
        if (markOffhand == State.Main) offScore += 2.0f;
        else if (markOffhand == State.Offhand) mainScore += 1.3f;
    }
    if (ShouldHoldWeapon(State.Main, context, AmmoOf(State, State.Main))) {
        mainScore += 2.4f;
    }
    const LowAmmoCombo lowAmmo = ChooseLowAmmoCombo(
        State.Main, State.Offhand, IncomingWeapon(),
        AmmoOf(State, State.Main), context);
    if (lowAmmo != LowAmmoCombo::None) mainScore += 2.2f;
    if (State.Main == Weapon::Crescendum && Engine::ValidEnemy(target)) {
        const float distance = GameObjects::Player().Position().Distance2D(
            target.Position());
        if (!ShouldUseCrescendumAuto(
                distance, Chakrams, context.PlayerChasingReturn, 1.55f)) {
            offScore += 1.4f;
        }
    }
    const float threshold = static_cast<float>(Slider(
        WeaponsMenu, "SwapAdvantage", 150)) / 100.0f;
    if (!reactive && !requested && offScore < mainScore + threshold) return false;
    if (reactive && offScore <= mainScore) return false;
    const bool cast = CastPhaseTo(desired,
        reactive ? Sequence::DefensiveMoonlight : Sequence::PlayerLedWeave,
        reactive);
    if (cast) {
        RequestedHandSwitchUntil = 0;
        RequestedHandWeapon = Weapon::Unknown;
    }
    return cast;
}

inline bool TryPrepareGravitumRoot(const AIHeroClient& target,
                                   QPurpose purpose,
                                   const CombatContext& context,
                                   bool reactive = false) {
    if (!Engine::ValidEnemy(target) || !HasGravitumMark(target)) return false;
    if (State.Main == Weapon::Gravitum) {
        return CommitQPlan(
            BuildGravitumPlan(purpose, target, reactive), context, reactive);
    }
    if (State.Offhand == Weapon::Gravitum && QReadyFor(Weapon::Gravitum)) {
        PendingRootTargetId = static_cast<int>(target.NetworkId());
        PendingRootUntil = Now() + 1100;
        return CastPhaseTo(
            Weapon::Gravitum, Sequence::SeverumGravitumRoot, reactive);
    }
    return false;
}

inline AIHeroClient ReactiveTarget() {
    if (InterruptExpireTick > Now()) {
        const auto target = HeroByNetworkId(InterruptTargetId);
        if (Engine::ValidEnemy(target)) return target;
    }
    if (GapcloserExpireTick > Now()) {
        const auto target = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(target)) return target;
    }
    return ControllerHelpers::NearestEnemyToPlayer({}, 900.0f);
}

inline bool TryPendingRoot() {
    if (PendingRootUntil <= Now()) {
        PendingRootTargetId = 0;
        PendingRootUntil = 0;
        return false;
    }
    const AIHeroClient target = HeroByNetworkId(PendingRootTargetId);
    if (!Engine::ValidEnemy(target) || !HasGravitumMark(target)) return false;
    const CombatContext context = BuildCombatContext(target, Mode::Automatic);
    return TryPrepareGravitumRoot(
        target,
        InterruptTargetId == PendingRootTargetId && InterruptExpireTick > Now()
            ? QPurpose::Interrupt : QPurpose::RootSetup,
        context, true);
}

inline bool TryReactiveControl() {
    const AIHeroClient target = ReactiveTarget();
    if (!Engine::ValidEnemy(target)) return false;
    const CombatContext context = BuildCombatContext(target, Mode::Automatic);
    if (InterruptExpireTick > Now() &&
        static_cast<int>(target.NetworkId()) == InterruptTargetId &&
        Bool(GravitumMenu, "Interrupt", true) &&
        TryPrepareGravitumRoot(
            target, QPurpose::Interrupt, context, true)) {
        ActiveSequence = Sequence::GravitumGlobalRoot;
        return true;
    }
    if (GapcloserExpireTick > Now() &&
        static_cast<int>(target.NetworkId()) == GapcloserTargetId) {
        if (Bool(GravitumMenu, "AntiGapcloser", true) &&
            TryPrepareGravitumRoot(
                target, QPurpose::AntiGapcloser, context, true)) {
            ActiveSequence = Sequence::GravitumGlobalRoot;
            return true;
        }
        if (State.Main == Weapon::Severum &&
            Bool(SeverumMenu, "AntiGapcloser", true) &&
            CommitQPlan(BuildSeverumPlan(
                target, QPurpose::AntiGapcloser, context, true),
                context, true)) {
            ActiveSequence = Sequence::SeverumCommit;
            return true;
        }
        if (State.Offhand == Weapon::Severum &&
            Bool(SeverumMenu, "AntiGapcloser", true) &&
            CastPhaseTo(Weapon::Severum,
                        Sequence::SeverumCommit, true)) {
            return true;
        }
        if (State.Main == Weapon::Crescendum &&
            Bool(CrescendumMenu, "PeelSentry", true) &&
            CommitQPlan(BuildCrescendumPlan(
                target, QPurpose::Peel, context, true), context, true)) {
            ActiveSequence = Sequence::SentryArmMoonlight;
            return true;
        }
    }
    return false;
}

inline bool TryDefensiveUltimate(const AIHeroClient& target,
                                 const CombatContext& context) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3) ||
        !Bool(UltimateMenu, "Defensive", true)) return false;
    const float hp = player.HealthPercent();
    const bool lethalPressure = IncomingThreatUntil > Now() &&
        (IncomingHardCrowdControl || RecentIncomingPressure >= 2.0f);
    if (hp > static_cast<float>(Slider(
            UltimateMenu, "DefensiveHp", 32)) &&
        !lethalPressure && !(context.NeedPeel && context.NearbyEnemies >= 2)) {
        return false;
    }
    UltimatePlan plan = BuildUltimatePlan(target, context, true);
    if (!plan.Valid) return false;
    if (plan.WeaponUsed == Weapon::Severum && hp > 48.0f &&
        !lethalPressure) return false;
    return CommitUltimatePlan(plan, true);
}

inline bool TryKillSecure() {
    if (!Bool(TacticsMenu, "KillSecure", true)) return false;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, kMoonlightVigilRange + 200.0f) ||
            IsCommonUntargetableOrImmune(enemy)) continue;
        const float effectiveHealth = enemy.Health() + enemy.AllShield();
        const CombatContext context = BuildCombatContext(enemy, Mode::Automatic);
        if (Ready(0) && QReadyFor(State.Main) &&
            State.Main != Weapon::Crescendum &&
            CurrentQDamage(enemy, State.Main) >= effectiveHealth) {
            QCastPlan plan = BuildCurrentQPlan(
                enemy, QPurpose::Execute, context, true);
            if (CommitQPlan(plan, context, true)) return true;
        }
        if (Ready(3) && Bool(UltimateMenu, "KillSecure", false)) {
            UltimatePlan plan = BuildUltimatePlan(enemy, context, false);
            if (plan.Valid &&
                UltimateDamage(enemy, plan.WeaponUsed, plan.HitCount) >=
                    effectiveHealth * 1.06f &&
                (plan.HitCount >= 2 || enemy.HealthPercent() <= 16.0f)) {
                if (CommitUltimatePlan(plan, true)) return true;
            }
        }
    }
    return false;
}

inline bool TryLowAmmoSequence(const AIHeroClient& target,
                               const CombatContext& context) {
    const LowAmmoCombo combo = ChooseLowAmmoCombo(
        State.Main, State.Offhand, IncomingWeapon(),
        AmmoOf(State, State.Main), context);
    if (combo == LowAmmoCombo::None ||
        !Bool(AmmoMenu, "LowAmmoCombos", true)) return false;
    const auto player = GameObjects::Player();
    const bool utilityCombo = combo ==
            LowAmmoCombo::SeverumGravitumRootIntoIncoming ||
        combo == LowAmmoCombo::GravitumRootIntoIncoming ||
        context.NeedPeel || context.ObjectiveActive;
    if (Bool(AmmoMenu, "NoLateShowboating", true) &&
        player.IsValid() && player.Level() >= 11 &&
        player.AttackSpeedMod() >= 1.75f && InAutoAttackRange(target) &&
        !utilityCombo) {
        return false;
    }
    QPurpose purpose = QPurpose::AllIn;
    switch (combo) {
    case LowAmmoCombo::CalibrumIntoIncoming:
        purpose = QPurpose::MarkChain;
        break;
    case LowAmmoCombo::SeverumGravitumRootIntoIncoming:
    case LowAmmoCombo::GravitumRootIntoIncoming:
        purpose = QPurpose::RootSetup;
        break;
    case LowAmmoCombo::InfernumIntoIncoming:
        purpose = context.GroupedEnemies >= 2
            ? QPurpose::Teamfight : QPurpose::AllIn;
        break;
    case LowAmmoCombo::SentryIntoIncoming:
        purpose = context.ObjectiveActive
            ? QPurpose::Objective : QPurpose::SentryZone;
        break;
    default:
        break;
    }
    const QCastPlan plan = BuildCurrentQPlan(target, purpose, context, false);
    if (!CommitQPlan(plan, context, false)) return false;
    ActiveSequence = Sequence::LowAmmoSwap;
    if ((combo == LowAmmoCombo::SeverumGravitumRootIntoIncoming ||
         combo == LowAmmoCombo::GravitumRootIntoIncoming) &&
        Engine::ValidEnemy(target)) {
        PendingRootTargetId = static_cast<int>(target.NetworkId());
        PendingRootUntil = Now() + 1700;
    }
    return true;
}

inline RotationPlan ControllerRotationPlan(const CombatContext& context) {
    RotationPlan plan = ChooseRotationPlan(State, context);
    if (plan == RotationPlan::BuildGreenBlue &&
        !Bool(WeaponsMenu, "GreenBlueCycle", true)) {
        plan = Bool(WeaponsMenu, "StandardCycle", true)
            ? RotationPlan::BuildStandard
            : RotationPlan::PreserveCurrent;
    }
    if (plan == RotationPlan::BuildStandard &&
        !Bool(WeaponsMenu, "StandardCycle", true)) {
        plan = RotationPlan::PreserveCurrent;
    }
    if ((plan == RotationPlan::HoldSurvivalGun ||
         plan == RotationPlan::HoldObjectiveGun) &&
        !Bool(WeaponsMenu, "ContextHold", true)) {
        plan = Bool(WeaponsMenu, "StandardCycle", true)
            ? RotationPlan::BuildStandard
            : RotationPlan::PreserveCurrent;
    }
    return plan;
}

inline bool TryCombo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return false;
    const CombatContext context = BuildCombatContext(target, Mode::Combo);
    CurrentRotation = ControllerRotationPlan(context);

    if (TryPrepareGravitumRoot(
            target, context.NeedPeel ? QPurpose::Peel : QPurpose::RootSetup,
            context, context.NeedPeel)) {
        return true;
    }
    if (TryDefensiveUltimate(target, context)) return true;

    // Sentry must arm while R is being cast; Calibrum Q should land before R
    // when building two sequential marks. Low-ammo Q is also cast before R so
    // Moonlight Vigil can cancel the Incoming Weapon animation.
    if (State.Main == Weapon::Crescendum && Ready(0) && Ready(3) &&
        Bool(CrescendumMenu, "SentryBeforeR", true)) {
        const QCastPlan sentry = BuildCrescendumPlan(
            target, QPurpose::SentryZone, context, false);
        if (CommitQPlan(sentry, context, false)) {
            ActiveSequence = Sequence::SentryArmMoonlight;
            return true;
        }
    }
    if (State.Main == Weapon::Calibrum && Ready(0) && Ready(3) &&
        !HasCalibrumMark(target) &&
        Bool(CalibrumMenu, "QBeforeR", true)) {
        const QCastPlan mark = BuildCalibrumPlan(
            target, QPurpose::MarkChain, false);
        if (CommitQPlan(mark, context, false)) {
            ActiveSequence = Sequence::CalibrumMarkChain;
            return true;
        }
    }
    if (TryLowAmmoSequence(target, context)) return true;

    if (Ready(3) && Bool(UltimateMenu, "Combo", true)) {
        UltimatePlan plan = BuildUltimatePlan(target, context, false);
        if (plan.Valid && CommitUltimatePlan(plan, false)) return true;
    }

    QPurpose purpose = QPurpose::AllIn;
    if (State.Main == Weapon::Infernum && context.GroupedEnemies >= 2) {
        purpose = QPurpose::Teamfight;
    } else if (State.Main == Weapon::Severum &&
               State.Offhand == Weapon::Crescendum) {
        purpose = QPurpose::ChakramBuild;
    } else if (State.Main == Weapon::Crescendum) {
        purpose = QPurpose::SentryZone;
    } else if (State.Offhand == Weapon::Gravitum) {
        purpose = QPurpose::RootSetup;
    }
    if (CommitQPlan(BuildCurrentQPlan(
            target, purpose, context, false), context, false)) {
        if (State.Main == Weapon::Severum) ActiveSequence = Sequence::SeverumCommit;
        else if (State.Main == Weapon::Infernum) ActiveSequence = Sequence::InfernumWave;
        else if (State.Main == Weapon::Crescendum) ActiveSequence = Sequence::SentryArmMoonlight;
        return true;
    }
    return TrySmartHandSwap(target, context, false);
}

inline bool TryHarass(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) ||
        ControllerHelpers::PlayerManaPercent() < Slider(TacticsMenu, "HarassMana", 48)) {
        return false;
    }
    const CombatContext context = BuildCombatContext(target, Mode::Harass);
    CurrentRotation = ControllerRotationPlan(context);
    QPurpose purpose = QPurpose::Trade;
    if (State.Main == Weapon::Calibrum) purpose = QPurpose::MarkChain;
    if (State.Main == Weapon::Severum && State.Offhand == Weapon::Crescendum) {
        purpose = QPurpose::ChakramBuild;
    }
    if (State.Main == Weapon::Gravitum) purpose = QPurpose::RootSetup;
    if (State.Main == Weapon::Crescendum &&
        !Bool(CrescendumMenu, "HarassSentry", false)) {
        return TrySmartHandSwap(target, context, false);
    }
    if (CommitQPlan(BuildCurrentQPlan(
            target, purpose, context, false), context, false)) {
        ActiveSequence = Sequence::PlayerLedWeave;
        return true;
    }
    return TrySmartHandSwap(target, context, false);
}

inline int CountFarmUnitsInInfernum(const Vector3& aim,
                                    bool jungle) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    int count = 0;
    const auto units = jungle ? GameObjects::Jungle()
                              : GameObjects::EnemyMinions();
    for (const auto& unit : units) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable()) continue;
        if (InfernumQHits(player.Position(), aim, unit.Position(),
                          unit.BoundingRadius())) {
            ++count;
        }
    }
    return count;
}

inline bool TryInfernumFarm(bool jungle,
                            const CombatContext& context) {
    if (State.Main != Weapon::Infernum || !Ready(0)) return false;
    const auto player = GameObjects::Player();
    const auto units = jungle ? GameObjects::Jungle()
                              : GameObjects::EnemyMinions();
    Vector3 bestAim{};
    int bestHits = 0;
    for (const auto& unit : units) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(unit.Position()) >
                kInfernumQRange + unit.BoundingRadius()) continue;
        const Vector3 direction = Direction2D(player.Position(), unit.Position());
        if (direction.IsZero()) continue;
        const Vector3 aim = player.Position() + direction * kInfernumQRange;
        const int hits = CountFarmUnitsInInfernum(aim, jungle);
        if (hits > bestHits) {
            bestHits = hits;
            bestAim = aim;
        }
    }
    const int minimum = Slider(FarmMenu,
        jungle ? "InfernumJungle" : "InfernumLane",
        jungle ? 2 : 4);
    if (bestHits < minimum) return false;
    QCastPlan plan{};
    plan.WeaponUsed = Weapon::Infernum;
    plan.Purpose = jungle ? QPurpose::Jungle : QPurpose::Waveclear;
    plan.Aim = bestAim;
    plan.HitCount = bestHits;
    plan.Score = static_cast<float>(bestHits);
    plan.WillDeplete = LowAmmoAbilitySwaps(AmmoOf(State, State.Main));
    plan.Valid = true;
    return CommitQPlan(plan, context, false);
}

inline bool TryCalibrumLastHit(const CombatContext& context) {
    if (State.Main != Weapon::Calibrum || !Ready(0) ||
        !Bool(FarmMenu, "CalibrumLastHit", false) ||
        !Engine::RuntimeSpells[0]) return false;
    const auto player = GameObjects::Player();
    AIMinionClient best{};
    float bestMargin = FLT_MAX;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!minion.IsValid() || minion.IsDead() || !minion.IsTargetable() ||
            player.Position().Distance2D(minion.Position()) >
                kCalibrumQRange + minion.BoundingRadius()) continue;
        const float predicted = Engine::RuntimeSpells[0]->GetHealthPrediction(minion);
        const float damage = CurrentQDamage(minion, Weapon::Calibrum);
        if (predicted > 0.0f && damage >= predicted &&
            damage - predicted < bestMargin) {
            best = minion;
            bestMargin = damage - predicted;
        }
    }
    if (!best.IsValid()) return false;
    QCastPlan plan{};
    plan.WeaponUsed = Weapon::Calibrum;
    plan.Purpose = QPurpose::Waveclear;
    plan.Aim = best.Position();
    plan.TargetId = static_cast<int>(best.NetworkId());
    plan.HitCount = 1;
    plan.Score = 1.0f;
    plan.WillDeplete = LowAmmoAbilitySwaps(AmmoOf(State, State.Main));
    plan.Valid = !ProjectileWallBlocksFromPlayer(best.Position(), 34.0f);
    return CommitQPlan(plan, context, false);
}

inline bool TryGravitumFarm(bool jungle,
                            const CombatContext& context) {
    if (State.Main != Weapon::Gravitum || !Ready(0) ||
        !Bool(FarmMenu, "GravitumDamage", false)) return false;
    int marked = 0;
    const auto units = jungle ? GameObjects::Jungle()
                              : GameObjects::EnemyMinions();
    for (const auto& unit : units) {
        if (unit.IsValid() && !unit.IsDead() && HasGravitumMark(unit)) ++marked;
    }
    const int minimum = Slider(FarmMenu, "GravitumTargets", 4);
    if (marked < minimum) return false;
    QCastPlan plan{};
    plan.WeaponUsed = Weapon::Gravitum;
    plan.Purpose = jungle ? QPurpose::Jungle : QPurpose::Waveclear;
    plan.HitCount = marked;
    plan.Score = static_cast<float>(marked);
    plan.WillDeplete = LowAmmoAbilitySwaps(AmmoOf(State, State.Main));
    plan.Valid = true;
    return CommitQPlan(plan, context, false);
}

inline bool TryFarm(Mode mode) {
    const bool jungle = mode == Mode::Jungle || HasNearbyJungleTarget(850.0f);
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const int manaFloor = Slider(FarmMenu,
        jungle ? "JungleMana" : "LaneMana", jungle ? 28 : 58);
    if (ControllerHelpers::PlayerManaPercent() < manaFloor) return false;
    if (Engine::CountEnemiesAt(player.Position(), 1000.0f) > 0 &&
        Bool(FarmMenu, "StopNearEnemy", true)) return false;
    CombatContext context{};
    context.PlayerHealthPercent = player.HealthPercent();
    context.ObjectiveSoon = ControllerHelpers::HasNearbyEpicMonster(1800.0f);
    context.ObjectiveActive = ControllerHelpers::HasNearbyEpicMonster(900.0f);
    context.EarlyGame = player.Level() <= 9;
    CurrentRotation = ControllerRotationPlan(context);
    if (TryInfernumFarm(jungle, context)) return true;
    if (!jungle && mode == Mode::LastHit && TryCalibrumLastHit(context)) {
        return true;
    }
    if (TryGravitumFarm(jungle, context)) return true;
    if (jungle && State.Main == Weapon::Crescendum &&
        Bool(FarmMenu, "CrescendumObjective", true)) {
        const auto target = SelectJungleTarget(950.0f);
        if (target.IsValid() && IsEpicMonster(target)) {
            Vector3 aim = ClampFromPlayer(
                target.Position(), kCrescendumSentryRange);
            QCastPlan plan{};
            plan.WeaponUsed = Weapon::Crescendum;
            plan.Purpose = QPurpose::Objective;
            plan.Aim = aim;
            plan.TargetId = static_cast<int>(target.NetworkId());
            plan.HitCount = 1;
            plan.Score = 5.0f;
            plan.WillDeplete = LowAmmoAbilitySwaps(
                AmmoOf(State, State.Main));
            plan.Valid = aim.IsValid() && !aim.IsZero() &&
                         !Engine::UnderEnemyTurret(aim);
            if (CommitQPlan(plan, context, false)) return true;
        }
    }
    return false;
}

inline bool TryFlee(const AIHeroClient& selected) {
    AIHeroClient target = Engine::ValidEnemy(selected)
        ? selected : ControllerHelpers::NearestEnemyToPlayer({}, 1000.0f);
    if (!Engine::ValidEnemy(target)) return false;
    const CombatContext context = BuildCombatContext(target, Mode::Flee);
    if (TryPrepareGravitumRoot(
            target, QPurpose::Peel, context, true)) return true;
    if (State.Main == Weapon::Severum &&
        CommitQPlan(BuildSeverumPlan(
            target, QPurpose::Sustain, context, true), context, true)) {
        return true;
    }
    if (State.Offhand == Weapon::Severum &&
        CastPhaseTo(Weapon::Severum, Sequence::SeverumCommit, true)) {
        return true;
    }
    if (State.Main == Weapon::Crescendum &&
        CommitQPlan(BuildCrescendumPlan(
            target, QPurpose::Peel, context, true), context, true)) {
        return true;
    }
    return TryDefensiveUltimate(target, context);
}

inline Posture ChoosePosture(Mode mode,
                             const AIHeroClient& target,
                             const CombatContext& context) {
    if (IsIncomingWeaponWindow()) return Posture::IncomingWeapon;
    if (mode == Mode::Flee) return Posture::Disengage;
    if (context.NeedPeel) return Posture::Peel;
    if (context.ObjectiveActive || mode == Mode::Jungle) return Posture::Objective;
    if (mode == Mode::LaneClear || mode == Mode::LastHit) return Posture::Rotation;
    if (mode == Mode::Harass) {
        return State.Main == Weapon::Calibrum ? Posture::Poke
                                               : Posture::ShortTrade;
    }
    if (mode == Mode::Combo && Engine::ValidEnemy(target)) {
        if (context.NeedCatch && PairContains(State, Weapon::Gravitum)) {
            return Posture::Catch;
        }
        if (context.CanCommitClose && PairContains(State, Weapon::Crescendum)) {
            return Posture::CloseCommit;
        }
        return Posture::FrontToBack;
    }
    return Posture::Neutral;
}

inline void ExpireTransientState() {
    const int now = Now();
    if (GapcloserExpireTick <= now) GapcloserTargetId = 0;
    if (InterruptExpireTick <= now) InterruptTargetId = 0;
    if (IncomingThreatUntil <= now) {
        IncomingHardCrowdControl = false;
        RecentIncomingPressure *= 0.92f;
    }
    if (PendingHandSwitchUntil <= now) PendingHandWeapon = Weapon::Unknown;
    if (RequestedHandSwitchUntil <= now) RequestedHandWeapon = Weapon::Unknown;
    if (PendingSentryUntil <= now) PendingSentryOffhand = Weapon::Unknown;
    if (CrescendumReturnUntil <= now) LastCrescendumAttackDistance = 0.0f;
    RefreshMarks();
    RefreshSentries();
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshWeaponState();
    ExpireTransientState();
    const CombatContext context = BuildCombatContext(selected, mode);
    CurrentPosture = ChoosePosture(mode, selected, context);
    CurrentRotation = ControllerRotationPlan(context);

    if (TryPendingUltimate()) return true;
    if (TryPendingRoot()) return true;
    if (TryReactiveControl()) return true;
    if (TryKillSecure()) return true;

    if (mode == Mode::Flee) return TryFlee(selected);
    if (mode == Mode::Combo) return TryCombo(selected);
    if (mode == Mode::Harass) return TryHarass(selected);
    if (mode == Mode::LaneClear) {
        return TryFarm(HasNearbyJungleTarget(850.0f)
            ? Mode::Jungle : Mode::LaneClear);
    }
    if (mode == Mode::LastHit) return TryFarm(Mode::LastHit);

    if (RequestedHandSwitchUntil > Now() && Engine::ValidEnemy(selected)) {
        return TrySmartHandSwap(selected, context, false);
    }
    return false;
}

inline Weapon EventWeapon(
    const SDK::Events::ProcessSpellEventArgs& args) {
    for (const char* name : {
             args.ScriptName, args.SpellName, args.PayloadSpellName,
             args.SpellSlotName }) {
        const Weapon weapon = WeaponFromRuntimeName(name);
        if (IsWeapon(weapon)) return weapon;
    }
    return Weapon::Unknown;
}

inline bool IsPhaseEvent(
    const SDK::Events::ProcessSpellEventArgs& args) {
    return args.Slot == static_cast<int>(SDK::SpellSlot::W) ||
           ControllerHelpers::SpellEventNameContainsAny(
               args, { "ApheliosW", "ApheliosPhase" });
}

inline bool IsMoonlightVigilEvent(
    const SDK::Events::ProcessSpellEventArgs& args) {
    return args.Slot == static_cast<int>(SDK::SpellSlot::R) ||
           ControllerHelpers::SpellEventNameContainsAny(
               args, { "ApheliosR", "MoonlightVigil" });
}

inline bool IsWeaponQEvent(
    const SDK::Events::ProcessSpellEventArgs& args,
    Weapon weapon) {
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Q)) return true;
    if (!IsWeapon(weapon)) return false;
    return ControllerHelpers::SpellEventNameContainsAny(args, {
        "ApheliosCalibrumQ", "ApheliosSeverumQ", "ApheliosGravitumQ",
        "ApheliosInfernumQ", "ApheliosCrescendumQ",
    });
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (IsOrdinaryAmmoAttack(args)) {
        const std::uint32_t targetId = args.TargetNetworkId != 0
            ? args.TargetNetworkId : args.Target.NetworkId;
        LastAutoTargetId = static_cast<int>(targetId);
        LastAutoTick = now;
        LastCombatTick = now;
        if (State.Main == Weapon::Crescendum) {
            const AIBaseClient target = ControllerHelpers::UnitByNetworkId(
                LastAutoTargetId);
            if (target.IsValid()) {
                LastCrescendumAttackDistance =
                    GameObjects::Player().Position().Distance2D(
                        target.Position());
                CrescendumReturnUntil = now + static_cast<int>(std::ceil(
                    CrescendumRoundTripSeconds(
                        LastCrescendumAttackDistance) * 1000.0f));
            }
        }
        RecordAmmoSpend(1, false);
        return;
    }

    const Weapon eventWeapon = EventWeapon(args);
    if (IsWeaponQEvent(args, eventWeapon)) {
        const Weapon used = IsWeapon(eventWeapon) ? eventWeapon : State.Main;
        if (IsWeapon(used) && used != State.Main) {
            // Runtime spell identity is a stronger observation than a stale
            // predicted state. Preserve off-hand when possible and let the
            // next buff refresh rebuild the remaining queue.
            if (used == State.Offhand) SwapHands(State);
            else {
                State.Main = used;
                State.QueueKnown = false;
            }
        }
        const int index = WeaponIndex(used);
        if (index >= 0) {
            const auto q = GameObjects::Player().Spellbook().GetSpell(
                SDK::SpellSlot::Q);
            float remaining = q.IsValid()
                ? q.RemainingCooldown(Game::Time()) : 0.0f;
            if (remaining <= 0.05f) {
                remaining = WeaponQCooldownSeconds(
                    used, GameObjects::Player().Level());
            }
            QReadyAt[index] = now + static_cast<int>(
                std::ceil(remaining * 1000.0f));
        }
        LastQCastTick = now;
        QOffhandEffectUntil = now +
            (used == Weapon::Severum ? 2100 : 850);
        if (Chakrams > 0) {
            ChakramExpireTick = std::max(
                ChakramExpireTick, now + 1500);
        }
        RecordAmmoSpend(kAbilityAmmoCost, true);
        if (!Engine::WasControllerCast(0)) {
            LastQPurpose = QPurpose::None;
            LastQPlan = {};
            LastQPlan.WeaponUsed = used;
            LastQPlan.Aim = args.EndPosition.IsValid() &&
                    !args.EndPosition.IsZero()
                ? args.EndPosition : args.CastPosition;
            LastQPlan.TargetId = static_cast<int>(
                args.TargetNetworkId != 0
                    ? args.TargetNetworkId : args.Target.NetworkId);
            LastQPlan.Valid = true;
            ActiveSequence = Sequence::PlayerLedWeave;
        }
        return;
    }

    if (IsPhaseEvent(args)) {
        LastWCastTick = now;
        if (IsWeapon(State.Main) && IsWeapon(State.Offhand)) {
            if (!IsWeapon(PendingHandWeapon) ||
                PendingHandWeapon == State.Offhand) {
                SwapHands(State);
            }
        }
        LastWeaponChangeTick = now;
        PendingHandSwitchUntil = 0;
        PendingHandWeapon = Weapon::Unknown;
        if (!Engine::WasControllerCast(1)) {
            ActiveSequence = Sequence::PlayerLedWeave;
        }
        return;
    }

    if (IsMoonlightVigilEvent(args)) {
        LastRCastTick = now;
        ReloadUntil = 0;
        IncomingWeaponUntil = 0;
        if (State.Main == Weapon::Crescendum) {
            Chakrams = std::max(Chakrams, 5);
            ChakramExpireTick = now + 5000;
        }
        if (!Engine::WasControllerCast(3)) {
            LastUltimatePurpose = UltimatePurpose::Manual;
            LastRPlan = {};
            LastRPlan.WeaponUsed = State.Main;
            LastRPlan.Aim = args.EndPosition.IsValid() &&
                    !args.EndPosition.IsZero()
                ? args.EndPosition : args.CastPosition;
            LastRPlan.Purpose = UltimatePurpose::Manual;
            LastRPlan.Valid = true;
            ActiveSequence = Sequence::PlayerLedWeave;
        }
    }
}

inline void RecordIncomingThreats(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto analysis = AnalyzeEnemyCast(args, 240.0f, 110.0f,
                                           280, 260, 220, 1700, 500);
    if (!analysis.Valid) return;
    if (analysis.TargetsPlayer || analysis.CrossesPlayer) {
        IncomingThreatUntil = std::max(
            IncomingThreatUntil,
            std::max(analysis.CommitmentUntilTick,
                     analysis.LineThreatUntilTick));
        IncomingHardCrowdControl = IncomingHardCrowdControl ||
                                   analysis.LikelyHardCrowdControl;
        RecentIncomingPressure = std::min(
            8.0f, RecentIncomingPressure +
                (analysis.LikelyHardCrowdControl ? 2.2f : 1.0f));
    }
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args,
                            bool added) {
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (IsReloadName(args.BuffName)) {
            ReloadUntil = added
                ? ControllerHelpers::BuffExpireTick(args, 1100) : 0;
            if (added) IncomingWeaponUntil = std::max(
                IncomingWeaponUntil, ReloadUntil);
        }
        if (Engine::TextContains(args.BuffName, "aphelioscrescendum") ||
            Engine::TextContains(args.BuffName, "aphelioschakram")) {
            if (added && args.Count > 0) {
                Chakrams = std::clamp(args.Count, 0, 20);
                ChakramExpireTick = ControllerHelpers::BuffExpireTick(
                    args, 5000);
            } else if (!added &&
                       (Engine::TextContains(args.BuffName, "manager") ||
                        Engine::TextContains(args.BuffName, "orbit"))) {
                Chakrams = 0;
                ChakramExpireTick = 0;
            }
        }
        if (Engine::TextContains(args.BuffName, "apheliosoffhandbuff")) {
            RefreshWeaponState();
        }
        return;
    }

    const int id = static_cast<int>(args.Sender.NetworkId);
    if (id == 0) return;
    if (IsCalibrumMarkName(args.BuffName)) {
        WeaponMark* mark = FindMark(id, added);
        if (mark) {
            mark->Calibrum = added;
            mark->CalibrumExpireTick = added
                ? ControllerHelpers::BuffExpireTick(args, 4500) : now;
            if (!mark->Calibrum && !mark->Gravitum) *mark = {};
        }
    }
    if (IsGravitumDebuffName(args.BuffName)) {
        WeaponMark* mark = FindMark(id, added);
        if (mark) {
            mark->Gravitum = added;
            mark->GravitumExpireTick = added
                ? ControllerHelpers::BuffExpireTick(args, 2500) : now;
            if (!mark->Calibrum && !mark->Gravitum) *mark = {};
        }
        if (added &&
            (id == GapcloserTargetId || id == InterruptTargetId ||
             id == LastQPlan.TargetId)) {
            PendingRootTargetId = id;
            PendingRootUntil = now + 1600;
        }
    }
}

inline bool SentryObjectName(const SDK::Events::ObjectEventArgs& args) {
    return Engine::TextContains(args.Sender.Name, "aphelioscrescendumturret") ||
           Engine::TextContains(args.Sender.CharacterName, "aphelioscrescendumturret") ||
           Engine::TextContains(args.SpellName, "aphelioscrescendumturret") ||
           Engine::TextContains(args.MissileName, "aphelioscrescendumturret");
}

inline SentryRecord* FindSentry(int networkId, bool create = false) {
    SentryRecord* empty = nullptr;
    for (auto& sentry : Sentries) {
        if (sentry.NetworkId == networkId && networkId != 0) return &sentry;
        if (!empty && sentry.NetworkId == 0) empty = &sentry;
    }
    if (!create) return nullptr;
    if (!empty) {
        empty = &*std::min_element(Sentries.begin(), Sentries.end(),
            [](const SentryRecord& left, const SentryRecord& right) {
                return left.IdleUntil < right.IdleUntil;
            });
    }
    *empty = {};
    empty->NetworkId = networkId;
    return empty;
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!SentryObjectName(args) ||
        !ControllerHelpers::ObjectEventIsAllied(args)) return;
    const std::uint32_t sourceId = args.SourceNetworkId != 0
        ? args.SourceNetworkId : args.Source.NetworkId;
    if (sourceId != 0 && player.IsValid() &&
        sourceId != static_cast<std::uint32_t>(player.NetworkId())) {
        return;
    }
    const int id = static_cast<int>(args.Sender.NetworkId != 0
        ? args.Sender.NetworkId : args.Sender.Index);
    SentryRecord* sentry = FindSentry(id, true);
    if (!sentry) return;
    sentry->Position = args.Sender.Position.IsValid()
        ? args.Sender.Position : args.EndPosition;
    sentry->Offhand = PendingSentryUntil > Now()
        ? PendingSentryOffhand : State.Offhand;
    sentry->SpawnTick = Now();
    sentry->IdleUntil = Now() + static_cast<int>(
        kCrescendumSentryIdleSeconds * 1000.0f);
    sentry->ActiveUntil = 0;
    sentry->Active = false;
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = static_cast<int>(args.Sender.NetworkId != 0
        ? args.Sender.NetworkId : args.Sender.Index);
    if (auto* sentry = FindSentry(id)) *sentry = {};
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!MissileEventIsLocal(args)) return;
    const int id = static_cast<int>(args.MissileNetworkId != 0
        ? args.MissileNetworkId : args.Sender.NetworkId);
    if (Engine::TextContains(args.MissileName, "apheliosrmis") ||
        Engine::TextContains(args.SpellName, "apheliosr")) {
        RMissileNetworkId = id;
        RMissilePosition = args.Sender.Position.IsValid()
            ? args.Sender.Position : args.StartPosition;
    } else if (Engine::TextContains(args.MissileName, "aphelioscalibrum") ||
               Engine::TextContains(args.SpellName, "aphelioscalibrumq")) {
        QMissileNetworkId = id;
    }
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    const int id = static_cast<int>(args.MissileNetworkId != 0
        ? args.MissileNetworkId : args.Sender.NetworkId);
    if (id == RMissileNetworkId) {
        RMissileNetworkId = 0;
        RMissilePosition = {};
    }
    if (id == QMissileNetworkId) QMissileNetworkId = 0;
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!SpellEventNameContains(args, "aphelioscrescendumturret")) return;
    const Vector3 position = args.Sender.Position.IsValid()
        ? args.Sender.Position : args.StartPosition;
    SentryRecord* closest = nullptr;
    float best = FLT_MAX;
    for (auto& sentry : Sentries) {
        if (sentry.NetworkId == 0 || !sentry.Position.IsValid()) continue;
        const float distance = sentry.Position.Distance2D(position);
        if (distance < best) {
            best = distance;
            closest = &sentry;
        }
    }
    if (closest && best <= 220.0f) {
        closest->Active = true;
        closest->ActiveUntil = Now() + static_cast<int>(
            kCrescendumSentryActiveSeconds * 1000.0f);
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    if (PlayerReloading()) {
        args.Process = false;
        return;
    }
    const AIBaseClient base(args.Target.Handle());
    if (!base.IsValid()) return;
    if (HasCalibrumMark(base)) return;
    const AIHeroClient target = HeroByNetworkId(
        static_cast<int>(args.Target.NetworkId()));
    if (!Engine::ValidEnemy(target) || !Ready(1) ||
        Engine::CurrentMode() == Mode::None) return;
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(target.Position());
    bool request = false;
    if (State.Main == Weapon::Crescendum && Chakrams == 0 &&
        distance > static_cast<float>(Slider(
            CrescendumMenu, "AvoidZeroStackRange", 590)) &&
        Bool(CrescendumMenu, "ProtectBadAuto", true)) {
        const CombatContext context = BuildCombatContext(
            target, Engine::CurrentMode());
        request = !ShouldUseCrescendumAuto(
            distance, Chakrams, context.PlayerChasingReturn, 1.45f);
    }
    if (!request && State.Main != Weapon::Severum &&
        State.Offhand == Weapon::Severum &&
        ProjectileWallBlocksFromPlayer(target.Position(), 45.0f) &&
        Bool(SeverumMenu, "SwapThroughWalls", true)) {
        request = true;
    }
    if (!request) return;
    args.Process = false;
    RequestedHandWeapon = State.Offhand;
    RequestedHandSwitchUntil = Now() + 420;
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(
            args, LastAfterAttackTargetId, LastAfterAttackTick)) return;
    const AIHeroClient target = HeroByNetworkId(LastAfterAttackTargetId);
    if (Engine::ValidEnemy(target)) {
        LastCombatTick = LastAfterAttackTick;
        ActiveSequence = Sequence::PlayerLedWeave;
    }
}

inline const char* SequenceName(Sequence sequence) {
    switch (sequence) {
    case Sequence::CalibrumMarkChain: return "green mark chain";
    case Sequence::CalibrumPreMarkAuto: return "pre-mark auto";
    case Sequence::SeverumCommit: return "red commit";
    case Sequence::SeverumGravitumRoot: return "red-purple root";
    case Sequence::GravitumGlobalRoot: return "purple global root";
    case Sequence::InfernumWave: return "blue wave";
    case Sequence::InfernumMoonlight: return "blue moonlight";
    case Sequence::SentryArmMoonlight: return "turret then moonlight";
    case Sequence::CrescendumCloseDps: return "white close DPS";
    case Sequence::LowAmmoSwap: return "low-ammo swap";
    case Sequence::IncomingWeaponCancel: return "incoming cancel";
    case Sequence::HiddenWeaponUltimate: return "hidden R weapon";
    case Sequence::DefensiveMoonlight: return "defensive moonlight";
    case Sequence::RotationRepair: return "rotation repair";
    case Sequence::ObjectivePreparation: return "objective prep";
    case Sequence::PlayerLedWeave: return "player weave";
    default: return "hold";
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::Poke: return "poke";
    case Posture::ShortTrade: return "short trade";
    case Posture::Catch: return "catch";
    case Posture::FrontToBack: return "front-to-back";
    case Posture::CloseCommit: return "close commit";
    case Posture::Peel: return "peel";
    case Posture::Disengage: return "disengage";
    case Posture::Objective: return "objective";
    case Posture::Rotation: return "rotation";
    case Posture::IncomingWeapon: return "incoming weapon";
    default: return "neutral";
    }
}

inline const char* RotationName(RotationPlan plan) {
    switch (plan) {
    case RotationPlan::PreserveCurrent: return "maintain cycle";
    case RotationPlan::BuildStandard: return "build standard";
    case RotationPlan::BuildGreenBlue: return "build green-blue";
    case RotationPlan::HoldSurvivalGun: return "hold utility";
    case RotationPlan::HoldObjectiveGun: return "hold objective gun";
    case RotationPlan::EmergencyFreestyle: return "freestyle/observe";
    default: return "maintain";
    }
}

inline const char* ConfidenceName(StateConfidence confidence) {
    switch (confidence) {
    case StateConfidence::PairObserved: return "pair-live";
    case StateConfidence::AmmoObserved: return "ammo-live";
    case StateConfidence::FullyObserved: return "pair+ammo-live";
    default: return "event-predicted";
    }
}

inline float MainWeaponRange() {
    switch (State.Main) {
    case Weapon::Calibrum: return kCalibrumQRange;
    case Weapon::Severum: return 550.0f;
    case Weapon::Gravitum: return kCalibrumMarkRange;
    case Weapon::Infernum: return kInfernumQRange;
    case Weapon::Crescendum: return kCrescendumSentryRange;
    default: return 550.0f;
    }
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (Bool(CoachMenu, "DrawRanges", true)) {
        Drawing::DrawCircle(player.Position(), MainWeaponRange(),
                            WeaponColor(State.Main) & 0x66FFFFFFu,
                            1.2f, 84);
        Drawing::DrawCircle(player.Position(),
                            State.Main == Weapon::Calibrum
                                ? kCalibrumAttackRange : player.AttackRange(),
                            WeaponColor(State.Main), 1.6f, 72);
    }
    if (Bool(CoachMenu, "DrawMarks", true)) {
        for (const auto& mark : Marks) {
            if (mark.NetworkId == 0 ||
                (!mark.Calibrum && !mark.Gravitum)) continue;
            const AIBaseClient unit = ControllerHelpers::UnitByNetworkId(
                mark.NetworkId);
            if (!unit.IsValid()) continue;
            if (mark.Calibrum) {
                Drawing::DrawCircle(unit.Position(),
                    unit.BoundingRadius() + 28.0f,
                    WeaponColor(Weapon::Calibrum), 2.0f, 38);
                Drawing::DrawLine(player.Position(), unit.Position(),
                                  0x8876E5D8u, 1.0f);
            }
            if (mark.Gravitum) {
                Drawing::DrawCircle(unit.Position(),
                    unit.BoundingRadius() + 40.0f,
                    WeaponColor(Weapon::Gravitum), 2.0f, 38);
            }
        }
    }
    if (Bool(CoachMenu, "DrawSentries", true)) {
        for (const auto& sentry : Sentries) {
            if (sentry.NetworkId == 0 || !sentry.Position.IsValid()) continue;
            Drawing::DrawCircle(sentry.Position,
                kCrescendumSentryAttackRange,
                sentry.Active ? 0x99F4E6A2u : 0x55F4E6A2u,
                sentry.Active ? 2.0f : 1.0f, 64);
            Drawing::DrawCircle(sentry.Position, 55.0f,
                WeaponColor(sentry.Offhand), 2.2f, 30);
        }
    }
    if (Bool(CoachMenu, "DrawUltimate", true) &&
        LastRPlan.Valid && Now() - LastRCastTick <= 1400) {
        Drawing::DrawLine(player.Position(), LastRPlan.Aim,
                          WeaponColor(LastRPlan.WeaponUsed), 2.0f);
        if (LastRPlan.Explosion.IsValid()) {
            Drawing::DrawCircle(LastRPlan.Explosion,
                kMoonlightVigilRadius,
                WeaponColor(LastRPlan.WeaponUsed), 2.4f, 64);
        }
    }
    if (Bool(CoachMenu, "DrawReturn", true) &&
        CrescendumReturnUntil > Now() && LastAutoTargetId != 0) {
        const AIBaseClient target = ControllerHelpers::UnitByNetworkId(
            LastAutoTargetId);
        if (target.IsValid()) {
            Drawing::DrawLine(player.Position(), target.Position(),
                              0xAAF4E6A2u, 1.6f);
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char text[620]{};
            _snprintf_s(
                text, sizeof(text), _TRUNCATE,
                "Aphelios OTP | %s | %s | %s(%d) / %s(%d) | next %s > %s > %s | chakram %d | %s | %s",
                PostureName(CurrentPosture), SequenceName(ActiveSequence),
                WeaponName(State.Main), AmmoOf(State, State.Main),
                WeaponName(State.Offhand), AmmoOf(State, State.Offhand),
                WeaponName(State.Queue[0]), WeaponName(State.Queue[1]),
                WeaponName(State.Queue[2]), Chakrams,
                RotationName(CurrentRotation), ConfidenceName(Confidence));
            Drawing::DrawText(screen.x - 300.0f, screen.y - 118.0f,
                              0xFFF4E6D0u, text);
            char cooldowns[360]{};
            _snprintf_s(
                cooldowns, sizeof(cooldowns), _TRUNCATE,
                "Q CDs G %.1f | R %.1f | P %.1f | B %.1f | W %.1f%s",
                std::max(0, QReadyAt[0] - Now()) / 1000.0f,
                std::max(0, QReadyAt[1] - Now()) / 1000.0f,
                std::max(0, QReadyAt[2] - Now()) / 1000.0f,
                std::max(0, QReadyAt[3] - Now()) / 1000.0f,
                std::max(0, QReadyAt[4] - Now()) / 1000.0f,
                PlayerReloading() ? " | RELOAD" : "");
            Drawing::DrawText(screen.x - 235.0f, screen.y - 96.0f,
                              0xFFBFDCE8u, cooldowns);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "ApheliosOneTrick", "Aphelios five-weapon one-trick mechanics"));
    TacticsMenu->Add(new MenuBool(
        "KillSecure", "conservatively lethal", true));
    TacticsMenu->Add(new MenuSlider(
        "HarassMana", "Min mana weapon harass (%)", 48, 0, 100));
    TacticsMenu->Add(new MenuSeparator(
        "Ownership",
        "Movement, ordinary attacks,"));

    WeaponsMenu = TacticsMenu->AddSubMenu(new Menu(
        "Weapons", "Main/off-hand choice and pair value"));
    WeaponsMenu->Add(new MenuSlider(
        "SwapAdvantage", "Off-hand adv (x0.01)", 150, 20, 500));
    WeaponsMenu->Add(new MenuBool(
        "StandardCycle", "Maintain", true));
    WeaponsMenu->Add(new MenuBool(
        "GreenBlueCycle", "Allow early", true));
    WeaponsMenu->Add(new MenuBool(
        "ContextHold", "Hold red/purple/blue/white", true));
    WeaponsMenu->Add(new MenuSeparator(
        "NoBlindRotation",
        "Cycle maintenance never"));

    AmmoMenu = TacticsMenu->AddSubMenu(new Menu(
        "Ammo", "Per-gun moonlight and incoming-weapon sequences"));
    AmmoMenu->Add(new MenuBool(
        "LiveReconcile", "Trust live Q ammo when", true));
    AmmoMenu->Add(new MenuBool(
        "LowAmmoCombos", "Use <=10 ammo Q for a", true));
    AmmoMenu->Add(new MenuBool(
        "CancelIncomingWithR", "Allow R to cancel Incoming", true));
    AmmoMenu->Add(new MenuBool(
        "NoLateShowboating", "Reject long late-game swap", true));
    AmmoMenu->Add(new MenuSeparator(
        "HybridState",
        "Runtime Q/off-hand buffs are"));

    CalibrumMenu = TacticsMenu->AddSubMenu(new Menu(
        "Calibrum", "Moonshot, mark reset and off-hand delivery"));
    CalibrumMenu->Add(new MenuBool(
        "QBeforeR", "Q before R marks", true));
    CalibrumMenu->Add(new MenuBool(
        "ConsumeBeforeRefresh", "Let the player consume a", true));
    CalibrumMenu->Add(new MenuBool(
        "BreakShield", "Moonshot vs spell shield", false));
    CalibrumMenu->Add(new MenuBool(
        "PreMarkAutoCoach", "Coach AA-before-mark-reset", true));
    CalibrumMenu->Add(new MenuSeparator(
        "Walls", "Moonshot and mark plans"));

    SeverumMenu = TacticsMenu->AddSubMenu(new Menu(
        "Severum", "Onslaught sustain, movement and off-hand setup"));
    SeverumMenu->Add(new MenuSlider(
        "SurvivalHp", "Onslaught survival HP (%)", 48, 10, 85));
    SeverumMenu->Add(new MenuBool(
        "AntiGapcloser", "Use/swap to Severum vs", true));
    SeverumMenu->Add(new MenuBool(
        "SwapThroughWalls", "Use non-projectile Severum", true));
    SeverumMenu->Add(new MenuBool(
        "RespectLateAutoDps", "Do not channel when", true));
    SeverumMenu->Add(new MenuSeparator(
        "OffHand", "Red-white builds chakrams;"));

    GravitumMenu = TacticsMenu->AddSubMenu(new Menu(
        "Gravitum", "Binding Eclipse root discipline"));
    GravitumMenu->Add(new MenuSlider(
        "ProactiveTargets", "Min marked champions for", 2, 1, 5));
    GravitumMenu->Add(new MenuBool(
        "HoldSingle", "Hold a harmless single mark", true));
    GravitumMenu->Add(new MenuBool(
        "RootShield", "Root spell-shielded", false));
    GravitumMenu->Add(new MenuBool(
        "Interrupt", "Root channel early", true));
    GravitumMenu->Add(new MenuBool(
        "AntiGapcloser", "Root a marked committed dash", true));
    GravitumMenu->Add(new MenuSeparator(
        "Truth", "Binding Eclipse is never"));

    InfernumMenu = TacticsMenu->AddSubMenu(new Menu(
        "Infernum", "Duskwave cone and grouped-fight damage"));
    InfernumMenu->Add(new MenuSlider(
        "ComboTargets", "Min Duskwave targets in a", 2, 1, 5));
    InfernumMenu->Add(new MenuBool(
        "PreserveObjective", "Hold useful blue ammo for an", true));
    InfernumMenu->Add(new MenuBool(
        "AutoResetCoach", "Coach AA-R-Q/AA timing", true));
    InfernumMenu->Add(new MenuSeparator(
        "Geometry", "Every cone candidate must"));

    CrescendumMenu = TacticsMenu->AddSubMenu(new Menu(
        "Crescendum", "Sentry snapshot and close-range chakram DPS"));
    CrescendumMenu->Add(new MenuBool(
        "SentryBeforeR", "Place Sentry before", true));
    CrescendumMenu->Add(new MenuBool(
        "PeelSentry", "Place a safe retreat-zone", true));
    CrescendumMenu->Add(new MenuBool(
        "HarassSentry", "Sentry for lane harass", false));
    CrescendumMenu->Add(new MenuBool(
        "ProtectBadAuto", "Replace only zero-stack", true));
    CrescendumMenu->Add(new MenuSlider(
        "AvoidZeroStackRange", "Zero-stack off-hand range", 590, 450, 700));
    CrescendumMenu->Add(new MenuSeparator(
        "DashTargets", "Never flee-place a turret"));

    UltimateMenu = TacticsMenu->AddSubMenu(new Menu(
        "MoonlightVigil", "First-hit trajectory and five weapon variants"));
    UltimateMenu->Add(new MenuBool(
        "Combo", "Best current/off-hand R only", true));
    UltimateMenu->Add(new MenuSlider(
        "MinimumScore", "Min pro R score", 680, 200, 1600));
    UltimateMenu->Add(new MenuSlider(
        "InfernumTargets", "Min Infernum R targets", 2, 1, 5));
    UltimateMenu->Add(new MenuBool(
        "Defensive", "Use Severum/Gravitum R vs", true));
    UltimateMenu->Add(new MenuSlider(
        "DefensiveHp", "Defensive R HP threshold (%)", 32, 5, 70));
    UltimateMenu->Add(new MenuSlider(
        "DefensiveScore", "Min def R score", 520, 100, 1200));
    UltimateMenu->Add(new MenuBool(
        "KillSecure", "Spend R for multi-hit or", false));
    UltimateMenu->Add(new MenuSeparator(
        "NoFlash", "The controller never casts"));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Ammo-aware lane and objective preparation"));
    FarmMenu->Add(new MenuSlider(
        "LaneMana", "Minimum lane-clear mana (%)", 58, 0, 100));
    FarmMenu->Add(new MenuSlider(
        "JungleMana", "Minimum jungle mana (%)", 28, 0, 100));
    FarmMenu->Add(new MenuBool(
        "StopNearEnemy", "Stop auto ammo preparation", true));
    FarmMenu->Add(new MenuSlider(
        "InfernumLane", "Min lane Duskwave", 4, 2, 9));
    FarmMenu->Add(new MenuSlider(
        "InfernumJungle", "Min jungle Duskwave", 2, 1, 6));
    FarmMenu->Add(new MenuBool(
        "CalibrumLastHit", "Spend 10 Calibrum ammo for a", false));
    FarmMenu->Add(new MenuBool(
        "GravitumDamage", "Use Binding Eclipse damage", false));
    FarmMenu->Add(new MenuSlider(
        "GravitumTargets", "Min Gravitum farm units", 4, 2, 10));
    FarmMenu->Add(new MenuBool(
        "CrescendumObjective", "Sentry on epic obj", true));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "Five-gun state and one-trick timing visualization"));
    CoachMenu->Add(new MenuBool(
        "DrawRanges", "Draw Q/attack ranges", false));
    CoachMenu->Add(new MenuBool(
        "DrawMarks", "Draw Calibrum/Gravitum", false));
    CoachMenu->Add(new MenuBool(
        "DrawSentries", "Draw sentry/off-hand", false));
    CoachMenu->Add(new MenuBool(
        "DrawUltimate", "Draw last R path/first-hit", false));
    CoachMenu->Add(new MenuBool(
        "DrawReturn", "Draw the active Crescendum", false));
    CoachMenu->Add(new MenuBool(
        "DrawState", "Draw pair, queue, ammo,", false));
}

inline void OnLoad() {
    State = {};
    Confidence = StateConfidence::Predicted;
    ActiveSequence = Sequence::None;
    CurrentPosture = Posture::Neutral;
    LastQPurpose = QPurpose::None;
    LastUltimatePurpose = UltimatePurpose::None;
    CurrentRotation = RotationPlan::BuildStandard;
    QReadyAt.fill(0);
    Marks.fill({});
    Sentries.fill({});
    Chakrams = ChakramExpireTick = ReloadUntil = IncomingWeaponUntil = 0;
    LastStateReconcileTick = LastAmmoObservationTick = 0;
    LastWeaponChangeTick = LastQCastTick = LastWCastTick = LastRCastTick = 0;
    LastAutoTick = LastAutoTargetId = 0;
    LastAfterAttackTick = LastAfterAttackTargetId = LastCombatTick = 0;
    QOffhandEffectUntil = PendingRootTargetId = PendingRootUntil = 0;
    PendingHandSwitchUntil = RequestedHandSwitchUntil = 0;
    PendingHandWeapon = RequestedHandWeapon = Weapon::Unknown;
    PendingRUntil = 0;
    PendingRWeapon = Weapon::Unknown;
    PendingRPlan = {};
    PendingSentryOffhand = Weapon::Unknown;
    PendingSentryUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEnd = {};
    InterruptTargetId = InterruptExpireTick = 0;
    IncomingThreatUntil = 0;
    IncomingHardCrowdControl = false;
    RecentIncomingPressure = 0.0f;
    QMissileNetworkId = RMissileNetworkId = 0;
    RMissilePosition = {};
    CrescendumReturnUntil = 0;
    LastCrescendumAttackDistance = 0.0f;
    LastQPlan = {};
    LastRPlan = {};
    RefreshWeaponState();
}

inline void OnUnload() {
    TacticsMenu = WeaponsMenu = AmmoMenu = CalibrumMenu = nullptr;
    SeverumMenu = GravitumMenu = InfernumMenu = CrescendumMenu = nullptr;
    UltimateMenu = FarmMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Use Riot 26.13 Calibrum mark damage of 15 plus 15 percent bonus AD",
    "Use Riot 26.13 Severum per-hit total-AD ratios rather than the old bonus-AD port",
    "Use Riot 26.13 Infernum Q base and bonus-AD progression",
    "Use Riot 26.13 Crescendum Sentry base and bonus-AD progression",
    "Use Riot 26.4 passive AD ranks of four through twenty-four",
    "Use Riot 26.1 full-crit follow-up attacks rather than historical reduced crit",
    "Use Riot 26.1 Moonlight Vigil thirty-percent bonus crit-damage follow-up",
    "Use Riot 26.1 post-midpatch lethality ranks of 4.5 each",
    "Use CommunityDragon 16.14 Calibrum Q 1450 range and 0.35 cast time",
    "Use CommunityDragon 16.14 Calibrum Q 1800 missile speed and 60 width",
    "Use CommunityDragon 16.14 4.5-second Calibrum mark lifetime",
    "Use CommunityDragon 16.14 Severum Q 1.75-second channel",
    "Use CommunityDragon 16.14 Gravitum initial 30-percent slow and one-second root",
    "Use CommunityDragon 16.14 Infernum Q 850 range and 0.40 cast time",
    "Use CommunityDragon 16.14 Crescendum Sentry 475 placement range",
    "Use CommunityDragon 16.14 Sentry 500 attack range and active lifetime",
    "Use CommunityDragon 16.14 Moonlight Vigil 1300 range and 210 explosion radius",
    "Use CommunityDragon 16.14 Moonlight Vigil 1000 speed and 0.50 cast time",
    "Reject the obsolete local DamagePassives Severum bonus-AD approximation",
    "Do not invent a local Aphelios plugin where repository audit found none",

    "Start with Calibrum main, Severum off-hand and the canonical three-gun queue",
    "Identify the main weapon from Q script name before consulting predictions",
    "Fall back from Q script name to spell name and icon name",
    "Identify the off-hand from exactly one ApheliosOffHandBuff weapon buff",
    "Treat an observed main-and-off-hand pair as stronger than predicted state",
    "Preserve the prior relative queue order while reconciling an observed pair",
    "Rebuild missing queue members exactly once without duplicate weapons",
    "Mark queue confidence false when runtime identity contradicts the predicted queue",
    "Expose predicted, pair-live, ammo-live and fully-live confidence separately",
    "Read live ammo only when MaxAmmo resembles the fifty-shot Moonlight reservoir",
    "Fall back to main/off-hand manager buff stacks when Q ammo is unavailable",
    "Reconcile off-hand ammo before Phase exposes that weapon as main-hand Q",
    "Reject ordinary one-charge and three-charge spell ammo as weapon ammo",
    "Clamp every observed weapon ammo value to zero through fifty",
    "Decrement ordinary main-hand attacks by one ammo",
    "Decrement a weapon Q by ten ammo",
    "Allow a below-ten low-ammo Q to deplete the remaining gun",
    "Do not decrement ammo for a special off-hand follow-up attack",
    "Do not decrement ammo for an Onslaught pseudo-attack",
    "Do not decrement ammo for a Sentry attack",
    "Do not decrement ammo for a Calibrum mark-consume attack",
    "Move off-hand to main when the current main weapon depletes",
    "Move queue head to off-hand when the current main weapon depletes",
    "Return the empty gun to queue tail with fifty ammo",
    "Track Incoming Weapon separately from the longer-term queue",
    "Treat the live ApheliosPReload buff as authoritative disarm state",
    "Block an impossible orbwalker attack during live reload",
    "Allow Moonlight Vigil to clear a predicted Incoming Weapon lock",
    "Keep an independent Q cooldown clock for each of the five guns",
    "Use the live main-hand Q readiness when that gun is equipped",
    "Use stored per-gun cooldown when evaluating the off-hand Q",
    "Scale each weapon Q cooldown across Aphelios level breakpoints",
    "Reconcile an event-predicted cooldown with live Q cooldown when exposed",
    "Never cast an unavailable off-hand Q merely because Phase is ready",
    "Expire stale marks, sentries, threats and requested swaps independently",
    "Recognize both BonusRangeBuff and BonusRangeDebuff Calibrum mark aliases",
    "Reset all five-gun state without leaking a prior game's queue",

    "Build the standard Calibrum-Gravitum-Infernum-Severum-Crescendum cycle",
    "Build the alternate Calibrum-Infernum-Gravitum-Severum-Crescendum cycle early",
    "Recognize either accepted cycle modulo its current rotation point",
    "Deplete Severum first during initial standard-cycle setup",
    "Prefer oldest-gun depletion once the intended cycle is established",
    "Allow Infernum-before-Gravitum only when building the green-blue cycle",
    "Do not blindly burn Severum while a dive threat is present",
    "Do not blindly burn Gravitum while catch or peel is needed",
    "Do not blindly burn Infernum immediately before a grouped objective fight",
    "Do not blindly burn Crescendum immediately before an epic objective",
    "Leave the accepted cycle to survive a genuinely lethal dive",
    "Leave the accepted cycle to secure a genuinely valuable catch",
    "Leave the accepted cycle for an active objective fight",
    "Freestyle from live pair value when queue confidence is lost",
    "Repair rotation only while no nearby enemy champion can punish the cast",
    "Never cast a rotation Q into empty space",
    "Require a minion, monster or champion payoff for Onslaught rotation burn",
    "Do not spend held utility ammo on routine wave preparation",
    "Score all ten weapon pairs instead of assigning one universal best pair",
    "Reward Severum-Crescendum as the reliable chakram-building duel pair",
    "Reward Calibrum-Crescendum as long-range Sentry mark into white DPS",
    "Reward Calibrum-Infernum for early green-blue burst",
    "Reward Calibrum-Gravitum for long-range root delivery",
    "Reward Gravitum-Infernum for grouped control and follow-up",
    "Reward Severum-Infernum for damage plus survival",
    "Reward Severum-Gravitum only when dive utility justifies low damage",
    "Penalize Gravitum-Crescendum's weak off-hand synergy",
    "Penalize Calibrum-Severum outside its narrow early trade pattern",
    "Evaluate Infernum-Crescendum differently during an active objective",
    "Let player health, range, grouping, peel and objective state change pair scores",
    "Require a material off-hand advantage before automatic Phase",
    "Preserve a low-ammo combo instead of swapping merely for a small pair advantage",
    "Preserve a held survival gun instead of swapping for theoretical damage",
    "Never Phase during a valuable ordinary attack windup",
    "Throttle Phase requests so the controller cannot oscillate hands",
    "Observe and continue a player-cast Phase without treating it as controller input",

    "Predict Moonshot with the live current Q spell wrapper",
    "Require high or very-high Moonshot hitchance according to commitment",
    "Relax Moonshot hitchance only for a real dash, control or reactive case",
    "Reject Moonshot beyond 1450 plus target gameplay radius",
    "Reject Moonshot through a live projectile wall",
    "Respect the player's cursor direction for ordinary Moonshot harass",
    "Bypass cursor agreement only for execute, mark chain, interrupt or peel",
    "Avoid spending Moonshot into a spell shield by default",
    "Allow the player to opt into a deliberate Moonshot shield break",
    "Do not refresh a live Calibrum mark before the player consumes it by default",
    "Allow Q-before-R to create two sequential Calibrum marks",
    "Track Calibrum marks by target and real buff expiry",
    "Use target live buff as stronger evidence than cached mark state",
    "Expire a cached Calibrum mark after 4.5 seconds",
    "Recognize the 1800-range Calibrum mark attack in the orbwalker",
    "Treat the mark consume as an off-hand special attack with no ordinary ammo loss",
    "Choose Gravitum as mark off-hand when a long-range catch is needed",
    "Choose Crescendum as mark off-hand when close follow-up is safe",
    "Choose Infernum as mark off-hand when enemies are grouped",
    "Choose Severum as mark off-hand when survival is critical",
    "Preserve the current off-hand when it already owns the desired mark effect",
    "Model Q travel as cast time plus distance divided by 1800",
    "Coach the AA-before-Moonshot-mark reset window without issuing the attack",
    "Do not claim a pre-mark auto fits when its windup ends after the mark arrives",
    "Use current Calibrum Q level-breakpoint base and bonus-AD ratios",
    "Include current mark bonus in conservative Calibrum execute damage",
    "Reject a Moonshot execute if immunity or untargetability is active",
    "Prefer long-range Calibrum value over close-range use when other guns are better",
    "Keep movement and the actual mark click entirely player-owned",

    "Cast Onslaught only with a nearby viable unit",
    "Use Onslaught as sustain below the configured health threshold",
    "Use Onslaught against a committed dive even above the ordinary health threshold",
    "Use non-projectile Severum through Yasuo, Samira and Braum-style projectile denial",
    "Swap to Severum before a blocked projectile auto when the off-hand is available",
    "Build at least the predicted minimum chakrams with red-white Onslaught",
    "Use red-purple Onslaught to tag Gravitum before Binding Eclipse",
    "Use red-blue Onslaught for safety without pretending its off-hand damage is exceptional",
    "Treat red-green repeated marks as a weak late-game Onslaught synergy",
    "Do not channel Onslaught when fast ordinary autos clearly win the damage race",
    "Override the late-auto rule for sustain, movement, root setup or chakram setup",
    "Do not channel Onslaught to chase under an enemy turret from safety",
    "Allow minions to support a deliberate out-of-combat red rotation burn",
    "Do not use minions to justify Onslaught when an enemy champion is nearby",
    "Calculate Onslaught attacks from bonus attack speed",
    "Use current total-AD per-hit ratios across seven level breakpoints",
    "Keep the one-quarter on-hit modifier separate from ordinary auto damage",
    "Track the live Onslaught buff to prevent duplicate Q casts",
    "Extend existing mini-chakram lifetime when an ability cast supports it",
    "Prefer Severum R over damage variants during genuinely lethal pressure",
    "Do not waste Severum R at healthy HP merely because it is equipped",
    "Keep player movement during Onslaught entirely player-owned",

    "Never cast Binding Eclipse with zero live Gravitum debuffs",
    "Count every marked enemy champion for the global Binding Eclipse root",
    "Count high-damage and low-health marked champions as priority roots",
    "Root a marked interrupt target before its channel expires",
    "Root a marked gapcloser before ordinary combo damage",
    "Swap purple to main before rooting an off-hand-applied Gravitum mark",
    "Carry a pending root target across the Phase event",
    "Expire a pending root if the Gravitum mark disappears",
    "Hold a harmless lone Gravitum mark as catch pressure by default",
    "Release the held single root when the target commits toward Aphelios",
    "Release the held single root for peel, interrupt or anti-gapcloser",
    "Require the configured marked-target count for a proactive multi-root",
    "Avoid rooting a lone spell-shielded target by default",
    "Allow an explicit lone-shield root override",
    "Track Gravitum debuff add, update and remove events",
    "Use live target buff as stronger evidence than cached Gravitum state",
    "Use current 50-to-140 base and 32-to-50 percent bonus-AD Q damage",
    "Remember Binding Eclipse applies one second of root after the slow mark",
    "Value Gravitum R's 99-percent slow and 1.35-second follow-up root for catch",
    "Prefer a guaranteed purple R pick over a flashy blue R when peel decides the fight",
    "Do not issue an attack merely to apply Gravitum; cooperate with the player's autos",

    "Treat Duskwave as a cone rather than a generic line skillshot",
    "Expand the Infernum cone edge by target gameplay radius",
    "Evaluate every predicted enemy bearing as a candidate cone direction",
    "Evaluate pair midpoints for multi-target Duskwave directions",
    "Require the selected target or a materially better multi-hit set",
    "Reward the selected target inside the best Duskwave cone",
    "Reward grouped targets more than scattered maximum-range poke",
    "Use the configured minimum target count for grouped combo Duskwave",
    "Allow a single-target lethal Duskwave without inventing an AoE requirement",
    "Respect cursor direction for an ordinary single-target Duskwave",
    "Ignore cursor direction for a genuinely superior teamfight cone",
    "Model Infernum Q's off-hand follow-up attack separately from wave damage",
    "Use current 20-to-110 base and 15-to-21 percent bonus-AD Q ratios",
    "Remember Infernum ordinary attacks carry a 110-percent damage multiplier",
    "Preserve useful Infernum ammo for a nearby epic objective",
    "Use Duskwave lane clear only at the configured hit count",
    "Use a lower configurable Duskwave threshold for jungle packs",
    "Never spend Infernum R on an ordinary wave",
    "Prefer Infernum R when the real first-hit explosion contains a grouped set",
    "Add objective value to Infernum R without forcing it over lifesaving utility",
    "Coach AA-R-Q or AA-R-AA timing without issuing the auto attacks",

    "Place Sentry only within 475 units of Aphelios",
    "Score Sentry contact using its 500 attack range plus target radius",
    "Clamp a predicted target placement back to real cast range",
    "Evaluate target position, retreat edge, path lead, objective and cursor placements",
    "Reject Sentry placement under an enemy turret",
    "Reject a flee Sentry that gives Yasuo a ready dash target",
    "Reject a flee Sentry that gives Samira a ready dash target",
    "Reject a flee Sentry that gives Nilah a ready dash target",
    "Reward a Sentry placed on an objective choke",
    "Reward a Sentry whose Calibrum off-hand can create long-range marks",
    "Reward a Sentry whose Gravitum off-hand can create peel marks",
    "Snapshot the off-hand at Sentry creation instead of reading a later hand",
    "Track multiple simultaneous Sentries by network ID",
    "Track Sentry idle and activated lifetimes separately",
    "Activate the nearest tracked Sentry from its turret attack event",
    "Delete exactly the lifecycle-matched Sentry record",
    "Do not cast ordinary lane-harass Sentry unless explicitly enabled",
    "Place Sentry before Crescendum R so it arms during the R cast",
    "Use Sentry before white DPS instead of after the target has escaped",
    "Use Sentry on an epic objective without moving Aphelios",
    "Treat close Crescendum blade return as higher DPS than max-range return",
    "Compute Crescendum round-trip time from target distance",
    "Reward movement toward the returning blade when the player is already chasing",
    "Do not block a max-range white auto when chasing makes its return efficient",
    "Replace only a zero-stack max-range white auto when off-hand is clearly better",
    "Never block a Calibrum mark consume merely because Crescendum is main",
    "Track the current Crescendum return window for player coaching",
    "Track mini-chakram stacks from manager and orbit-manager buffs",
    "Expire mini-chakrams when their live manager disappears",
    "Model diminishing mini-chakram damage from 15 to 5 percent",
    "Avoid Crescendum R when many existing chakrams make its minimum-five grant wasteful",
    "Prefer Crescendum R when close follow-up is safe and stacks are still needed",
    "Never issue movement to force close-range Crescendum DPS",

    "Treat Moonlight Vigil as a line missile that explodes on the first champion hit",
    "Find the earliest predicted champion intersection along every R candidate line",
    "Center the R explosion on that first champion rather than on the cursor",
    "Count secondary champions inside 210 plus gameplay radius",
    "Reject an R path through a live projectile wall",
    "Evaluate each predicted champion bearing as an R trajectory",
    "Evaluate pair midpoints only as trajectories, not imaginary explosion centers",
    "Score the same R trajectory for both currently accessible weapons",
    "Apply a Phase cost to the off-hand R score",
    "Use W-R to hide the chosen R weapon when off-hand value wins materially",
    "Carry the scored R plan across the synchronous Phase event",
    "Cancel a pending hidden R if its weapon or timing becomes stale",
    "Never cast Flash for Moonlight Vigil",
    "Allow the player's manual Flash during R without issuing movement",
    "Use Calibrum R for long-range single-target execute and mark chaining",
    "Use Severum R for emergency flat healing under lethal pressure",
    "Use Gravitum R for catch, peel and disengage",
    "Use Infernum R for a real grouped explosion rather than raw hit count alone",
    "Use Crescendum R only when close follow-up can exploit the granted chakrams",
    "Require the configured Infernum target count for ordinary proactive blue R",
    "Allow a lower-hit Gravitum R when its control decides a catch",
    "Allow a lower-hit Severum R when its healing prevents death",
    "Penalize a single spell-shielded primary R target",
    "Reward priority carries inside the R explosion",
    "Reward a selected target inside the actual R explosion",
    "Penalize a trajectory whose first collision prevents hitting the selected target",
    "Use current R base 125, 175 and 225 plus 20-percent bonus AD and 100-percent AP",
    "Add the current Infernum R 50, 100 and 150 plus 25-percent bonus-AD burst",
    "Add the current Calibrum R 50, 80 and 110 mark bonus",
    "Track the current Severum R 250, 350 and 450 flat healing value",
    "Track Gravitum R's 1.35-second follow-up root value",
    "Track Crescendum R as at least five mini-chakrams",
    "Let a valuable low-ammo Q precede R so R cancels Incoming Weapon",
    "Reject late-game swap-cancel showboating when ordinary autos have higher value",
    "Use defensive R before kill secure when incoming lethal pressure exists",
    "Disable automatic R kill secure by default",
    "When enabled require multi-hit value or a near-certain very-low-health execute",
    "Observe and continue a player-cast R with the weapon actually equipped",
    "Track the allied R missile separately from its eventual explosion",

    "Use Calibrum Q then incoming Infernum R then mark follow-up when low ammo permits",
    "Use Infernum Q then incoming Crescendum R then close white DPS",
    "Use Gravitum Q then incoming Crescendum R after a real marked root",
    "Use red-purple Onslaught tags then Binding Eclipse then incoming weapon",
    "Use Sentry then incoming weapon R only when objective or zone value exists",
    "Do not attempt a low-ammo combo when the next weapon is unknown",
    "Do not attempt a low-ammo combo above ten ammo merely for style",
    "Do not overwrite a low-ammo root sequence with a routine hand swap",
    "Preserve R for the swap-cancel only when its scored variant remains valuable",
    "Let live runtime pair reconciliation correct a predicted swap-combo queue",

    "React to an interrupt callback through the shared neutral event adapter",
    "React to a gapcloser callback through the shared neutral event adapter",
    "Prioritize a real marked Gravitum root over speculative damage",
    "Use Severum against an unmarked committed diver",
    "Use a safe Sentry retreat zone when neither purple nor red can answer",
    "Use defensive Severum or Gravitum R only under configured pressure",
    "Analyze targeted and crossing enemy spell events for incoming pressure",
    "Increase defensive urgency for likely hard crowd control",
    "Expire incoming spell pressure instead of treating it as permanent",
    "Respect spell shields, immunity, stasis and untargetability independently",
    "Preserve ordinary attack windup for every non-reactive Q",
    "Preserve ordinary attack windup for every non-reactive W",
    "Preserve ordinary attack windup for every non-reactive R",
    "Use zero-delay casting only for a real interrupt, gapcloser or lethal pressure",
    "Respect the player's selected target in every proactive plan",
    "Use cursor agreement only where direction disambiguates player intent",
    "Never issue movement for Aphelios",
    "Never issue attack-move for Aphelios",
    "Never issue an ordinary attack for Aphelios",
    "Never cast Flash or another summoner spell automatically",
    "Never level Q, W or E automatically through the combat controller",
    "Continue a player-cast Q using the same ammo and cooldown state machine",
    "Continue a player-cast W using the same pair and queue state machine",
    "Continue a player-cast R using the same weapon-variant state machine",
    "Yield the actual Calibrum mark click to the player",
    "Yield Crescendum chase direction and blade-return positioning to the player",
    "Stop automatic ammo preparation while an enemy champion is nearby",
    "Use health prediction before optional Calibrum Q last hit",
    "Keep high-cost Calibrum Q last hitting disabled by default",
    "Use Infernum Q only on a large enough lane or jungle set",
    "Keep Gravitum farm damage disabled by default",
    "Use Crescendum Sentry automatically only on a valuable epic objective",
    "Never spend Moonlight Vigil on an ordinary wave or camp",
    "Infer jungle mode from nearby neutral monsters without inventing pathing",
    "Expose current pair, queue, ammo, chakrams and all five Q cooldowns",
    "Expose mark, Sentry, Crescendum return and R first-hit geometry",
    "Expose rotation plan separately from the current combat posture",
    "Expose when state is predicted instead of falsely claiming perfect telemetry",
    "Fall back to conservative live-pair play when ammo telemetry is unavailable",
    "Never fall back to a generic Q-W-E-R priority because this controller owns the loop",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Aphelios;
    controller.ControllerId = "champion.kuroaio.ai.aphelios.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIAphelios.md";
    controller.ImplementationSummary =
        "Five-weapon state machine with hybrid live/event ammo, independent "
        "per-gun Q cooldowns, standard and green-blue cycle policy, all twenty "
        "main/off-hand Q interactions, low-ammo Incoming Weapon chains, mark/"
        "root/chakram/Sentry lifecycle tracking, first-champion R collision "
        "geometry, five scored R variants and player-owned attack/positioning.";
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
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser =
        &ControllerHelpers::CaptureGapcloserEvent<
            &GapcloserTargetId, &GapcloserEnd,
            &GapcloserExpireTick, 650, 900>;
    controller.OnInterruptable =
        &ControllerHelpers::CaptureInterruptableEvent<
            &InterruptTargetId, &InterruptExpireTick, 1850, 250, 5200>;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Aphelios
