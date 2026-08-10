#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIAmumuGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Amumu {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CastThrottleReady;
using ControllerHelpers::ChampionIs;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::CurrentResource;
using ControllerHelpers::EnemyFlashReady;
using ControllerHelpers::EnemySpellReady;
using ControllerHelpers::HasReadyDashHazardAt;
using ControllerHelpers::HasCurrentResource;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NameEquals;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerMobilityLocked;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellEventNameContains;
using ControllerHelpers::SpellRank;
using ControllerHelpers::UnitByNetworkId;

enum class Sequence : int {
    None,
    FirstBandage,
    ArrivalCurseWeave,
    ArrivalUltimate,
    LayerSecondBandage,
    MinionBridge,
    FollowFlashOrDash,
    PeelChain,
    EscapeAnchor,
    JungleCycle,
};

enum class Posture : int {
    Neutral,
    Gank,
    Followup,
    TeamfightEngage,
    Peel,
    Disengage,
    Jungle,
    SupportLane,
};

enum class BandagePurpose : int {
    None,
    Engage,
    FollowDash,
    Bridge,
    Peel,
    Interrupt,
    Escape,
    Jungle,
};

enum class UltimateReason : int {
    None,
    QArrival,
    MultiTarget,
    HighValuePick,
    Peel,
    AntiDash,
    Interrupt,
    Lethal,
    Survival,
    ManualFlash,
};

struct BandagePlan {
    Vector3 Aim = {};
    Vector3 CollisionPosition = {};
    Vector3 ArrivalPosition = {};
    int IntendedId = 0;
    int CollisionId = 0;
    float EntryDistance = FLT_MAX;
    float ImpactSeconds = 0.0f;
    float ArrivalSeconds = 0.0f;
    SDK::HitChance Hitchance = SDK::HitChance::None;
    bool IntendedFirst = false;
    bool ArrivalSafe = false;
    bool SpellShieldMobility = false;
    bool Valid = false;
};

struct UltimatePlan {
    Vector3 Center = {};
    std::array<int, 10> HitIds = {};
    int RawHitCount = 0;
    int EffectiveHitCount = 0;
    int PrimaryId = 0;
    float Score = 0.0f;
    float PrimaryScore = 0.0f;
    bool IncludesProtectedThreat = false;
    bool IncludesSelected = false;
    bool Valid = false;
};

struct CurseMark {
    int NetworkId = 0;
    int ExpireTick = 0;
};

struct IncomingAttack {
    int SourceId = 0;
    int ImpactTick = 0;
    int ExpireTick = 0;
};

inline Menu* TacticsMenu = nullptr;
inline Menu* RoleMenu = nullptr;
inline Menu* BandageMenu = nullptr;
inline Menu* DespairMenu = nullptr;
inline Menu* TantrumMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline Sequence ActiveSequence = Sequence::None;
inline Posture CurrentPosture = Posture::Neutral;
inline BandagePurpose LastBandagePurpose = BandagePurpose::None;
inline UltimateReason LastUltimateReason = UltimateReason::None;

inline int QObservedAmmo = -1;
inline int QObservedMaxAmmo = -1;
inline int QCastTick = 0;
inline int QExpectedImpactTick = 0;
inline int QExpectedArrivalTick = 0;
inline int QTargetId = 0;
inline int QFollowChampionId = 0;
inline int QStunUntil = 0;
inline int QForcedAutoSuppressUntil = 0;
inline int QArrivalGraceUntil = 0;
inline bool QPendingArrival = false;
inline bool QDashObserved = false;
inline Vector3 QCastOrigin = {};
inline BandagePlan LastBandagePlan = {};

// Bandage planning is queried by several posture branches in one update.
// Keep a small movement-aware cache so the nine angular probes share one
// object snapshot instead of rescanning minions/monsters/heroes per offset.
struct BandagePlanCacheEntry {
    int Tick = 0;
    int IntendedId = 0;
    BandagePurpose Purpose = BandagePurpose::None;
    bool AllowSpellShieldMobility = false;
    Vector3 Origin = {};
    Vector3 IntendedPosition = {};
    BandagePlan Plan = {};
};

inline std::array<BandagePlanCacheEntry, 8> BandagePlanCaches = {};
inline std::size_t BandagePlanCacheCursor = 0;

inline bool WActive = false;
inline int WToggleTick = 0;
inline int WLastContactTick = 0;
inline int WExpectedContactUntil = 0;

inline int ECastTick = 0;
inline int ETargetId = 0;

inline int RCastTick = 0;
inline int RResolveTick = 0;
inline int RStunUntil = 0;
inline int RPendingMarkTick = 0;
inline int RCastTargetId = 0;
inline bool RPendingResolve = false;
inline bool RWasManual = false;
inline UltimatePlan LastUltimatePlan = {};

inline std::array<CurseMark, 16> CurseMarks = {};
inline std::array<IncomingAttack, 24> IncomingAttacks = {};

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
inline int ManualFlashUntil = 0;
inline int ManualRFlashWindowUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline float RecentIncomingPressure = 0.0f;

inline constexpr int kCurseDurationMs = 3000;
inline constexpr int kBandageStunMs = 1000;
inline constexpr int kUltimateStunMs = 1500;
inline constexpr int kQSequenceGraceMs = 500;
inline constexpr float kWDrainPerSecond = 8.0f;

inline int RuntimeQCharges() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::Q);
    if (spell.IsValid()) {
        const int maximum = spell.MaxAmmo();
        const int ammo = spell.Ammo();
        if (maximum > 0) {
            QObservedMaxAmmo = maximum;
            QObservedAmmo = std::clamp(ammo, 0, maximum);
            return QObservedAmmo;
        }
    }
    if (QObservedAmmo >= 0) return QObservedAmmo;
    return Ready(0) ? 1 : 0;
}

inline int ReservedQCharges() {
    return Bool(BandageMenu, "ReserveSecond", true) ? 1 : 0;
}

inline bool JungleRole() {
    const int role = List(RoleMenu, "Role", 0);
    return role == 1 ||
        (role == 0 && ControllerHelpers::HeroHasSmite(
            GameObjects::Player()));
}

inline bool SupportRole() {
    const int role = List(RoleMenu, "Role", 0);
    return role == 2 ||
        (role == 0 && !ControllerHelpers::HeroHasSmite(
            GameObjects::Player()));
}

inline void MarkCurse(int networkId, int durationMs = kCurseDurationMs) {
    if (networkId == 0) return;
    CurseMark* empty = nullptr;
    CurseMark* oldest = &CurseMarks[0];
    for (auto& mark : CurseMarks) {
        if (mark.NetworkId == networkId) {
            mark.ExpireTick = Now() + std::max(0, durationMs);
            return;
        }
        if (mark.NetworkId == 0 || mark.ExpireTick < Now()) {
            if (!empty) empty = &mark;
        }
        if (mark.ExpireTick < oldest->ExpireTick) oldest = &mark;
    }
    CurseMark* slot = empty ? empty : oldest;
    slot->NetworkId = networkId;
    slot->ExpireTick = Now() + std::max(0, durationMs);
}

inline bool HasTrackedCurse(int networkId) {
    for (const auto& mark : CurseMarks) {
        if (mark.NetworkId == networkId && mark.ExpireTick >= Now()) {
            return true;
        }
    }
    return false;
}

inline bool HasCurse(const AIBaseClient& target) {
    if (!target.IsValid()) return false;
    return HasTrackedCurse(static_cast<int>(target.NetworkId())) ||
           target.HasBuff("CurseoftheSadMummy") ||
           target.HasBuff("CurseOfTheSadMummy") ||
           target.HasBuff("AmumuPassiveDebuff");
}

inline float QRawDamage() {
    static constexpr float base[] = {
        0.0f, 70.0f, 95.0f, 120.0f, 145.0f, 170.0f,
    };
    const auto player = GameObjects::Player();
    return base[std::clamp(SpellRank(0), 0, 5)] +
           player.AP() * 0.85f;
}

inline float QDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid()
        ? player.CalculateMagicDamage(target, QRawDamage())
        : 0.0f;
}

inline float EDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid()
        ? player.CalculateMagicDamage(
              target, TantrumRawDamage(SpellRank(2), player.AP()))
        : 0.0f;
}

inline float RDamage(const AIBaseClient& target, bool cursedBeforeCast) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const float raw = UltimateRawDamage(SpellRank(3), player.AP());
    return player.CalculateMagicDamage(target, raw) +
           CurseBonusTrueDamage(raw, cursedBeforeCast);
}

inline float ConservativeComboDamage(const AIHeroClient& target,
                                     bool includeUltimate = true) {
    if (!Engine::ValidEnemy(target)) return 0.0f;
    const auto player = GameObjects::Player();
    float damage = SDK::Damage::GetAutoAttackDamage(player, target, true);
    const int charges = RuntimeQCharges();
    if (Ready(0) && charges > 0) {
        damage += QDamage(target) * static_cast<float>(charges);
    }
    if (Ready(2)) damage += EDamage(target);
    if (WActive || Ready(1)) {
        const float rawTick = DespairRawDamagePerTick(
            SpellRank(1), player.AP(), target.MaxHealth());
        damage += player.CalculateMagicDamage(target, rawTick * 2.0f);
    }
    if (includeUltimate && Ready(3)) {
        damage += RDamage(target, HasCurse(target));
    }
    return damage * 0.88f;
}

inline bool ConservativeKillable(const AIHeroClient& target,
                                 bool includeUltimate = true) {
    return Engine::ValidEnemy(target) &&
           ConservativeComboDamage(target, includeUltimate) >=
               target.Health();
}

inline bool EnemyCanCleanse(const AIHeroClient& target) {
    if (!target.IsValid()) return false;
    if (SDK::CanUseItem(target, SDK::ItemId::Quicksilver_Sash) ||
        SDK::CanUseItem(target, SDK::ItemId::Mercurial_Scimitar) ||
        SDK::CanUseItem(target, SDK::ItemId::Silvermere_Dawn)) {
        return true;
    }
    return (ChampionIs(target, SDK::ChampionId::Olaf) &&
            EnemySpellReady(target, SDK::SpellSlot::R)) ||
           (ChampionIs(target, SDK::ChampionId::Gangplank) &&
            EnemySpellReady(target, SDK::SpellSlot::W)) ||
           (ChampionIs(target, SDK::ChampionId::Alistar) &&
            EnemySpellReady(target, SDK::SpellSlot::R)) ||
           target.HasBuff("DrMundoPImmunity") ||
           target.HasBuff("MundoPassiveCCImmune");
}

inline bool TargetUntargetableState(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
           target.HasBuff("FioraW") ||
           target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzE") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("EliseSpiderE") ||
           target.HasBuff("BardRStasis");
}

inline bool CursorAgrees(const Vector3& destination,
                         float minimumDot = -0.05f) {
    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo) return true;
    if (!Bool(RoleMenu, "RespectCursor", true)) return true;
    return ControllerHelpers::CursorDirectionAgrees(
        destination, minimumDot);
}

inline void AppendLineUnit(std::vector<LineUnit>& result,
                           const AIBaseClient& unit,
                           float predictionDelay) {
    if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable()) return;
    Vector3 position = predictionDelay > 0.0f
        ? PredictPosition(unit, predictionDelay)
        : unit.Position();
    if (!position.IsValid() || position.IsZero()) position = unit.Position();
    result.push_back(LineUnit{
        position, unit.BoundingRadius(),
        static_cast<int>(unit.NetworkId()), true,
    });
}

inline std::vector<LineUnit> BandageCollisionUnits(float delaySeconds) {
    std::vector<LineUnit> result;
    result.reserve(80);
    for (const auto& minion : GameObjects::EnemyMinions()) {
        AppendLineUnit(result, minion, delaySeconds);
    }
    for (const auto& monster : GameObjects::Jungle()) {
        AppendLineUnit(result, monster, delaySeconds);
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        AppendLineUnit(result, enemy, delaySeconds);
    }
    return result;
}

inline bool ArrivalSafe(const Vector3& position,
                        BandagePurpose purpose,
                        const AIHeroClient& target = {}) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !position.IsValid() || position.IsZero() ||
        SDK::NavMesh::IsWall(position)) {
        return false;
    }
    const bool turret = Engine::UnderEnemyTurret(position);
    const bool allowDive = Bool(Engine::ComboMenu, "AllowTurretDive", false) &&
        Engine::ActiveProfile && Engine::ActiveProfile->AllowTurretDiveIfKillable &&
        Engine::ValidEnemy(target) && ConservativeKillable(target);
    const bool hazard = Bool(BandageMenu, "RespectDashHazards", true) &&
                        HasReadyDashHazardAt(position);
    const int enemies = Engine::CountEnemiesAt(position, 700.0f);
    const int allies = CountAlliedFollowup(position, 850.0f, true);
    int maximum = Slider(BandageMenu, "MaxArrivalEnemies", 3);
    if (purpose == BandagePurpose::Peel ||
        purpose == BandagePurpose::Interrupt ||
        purpose == BandagePurpose::Escape) {
        maximum = std::min(5, maximum + 1);
    }
    if (enemies > maximum && player.HealthPercent() < 82.0f) return false;
    const float score = ArrivalSafetyScore(
        enemies, allies, turret && !allowDive, hazard,
        player.HealthPercent());
    return score >= static_cast<float>(
        Slider(BandageMenu, "MinimumSafety", -180));
}

inline SDK::HitChance RequiredBandageHitchance(
    const AIHeroClient& target,
    BandagePurpose purpose) {
    if (Engine::IsHardCrowdControlled(target)) {
        return SDK::HitChance::Immobile;
    }
    if (target.IsDashing() || purpose == BandagePurpose::FollowDash ||
        purpose == BandagePurpose::Interrupt ||
        purpose == BandagePurpose::Peel) {
        return SDK::HitChance::High;
    }
    SDK::HitChance baseChance = SDK::HitChance::VeryHigh;
    switch (List(BandageMenu, "Hitchance", 2)) {
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

inline BandagePlan BuildBandagePlan(
    const AIBaseClient& intended,
    BandagePurpose purpose,
    bool allowSpellShieldMobility = false) {
    BandagePlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !intended.IsValid() || intended.IsDead() ||
        !intended.IsTargetable() || !Ready(0)) {
        return best;
    }
    const Vector3 origin = player.Position();
    const Vector3 intendedPosition = intended.Position();
    const int intendedId = static_cast<int>(intended.NetworkId());
    const int now = Now();
    for (const BandagePlanCacheEntry& entry : BandagePlanCaches) {
        if (entry.Tick <= 0 || now < entry.Tick || now - entry.Tick > 72 ||
            entry.IntendedId != intendedId || entry.Purpose != purpose ||
            entry.AllowSpellShieldMobility != allowSpellShieldMobility ||
            entry.Origin.Distance2D(origin) > 24.0f ||
            entry.IntendedPosition.Distance2D(intendedPosition) > 24.0f) {
            continue;
        }
        return entry.Plan;
    }
    const float centerDistance = origin.Distance2D(intendedPosition);
    if (centerDistance > kBandageRange + intended.BoundingRadius() +
                             kBandageHalfWidth + 25.0f) {
        return best;
    }

    AIHeroClient hero{};
    SDK::HitChance observedChance = SDK::HitChance::High;
    Vector3 directPoint = intended.Position();
    if (intended.IsHero()) {
        hero = AIHeroClient(intended.Address());
        if (TargetUntargetableState(hero)) return best;
        const auto prediction = Engine::RuntimeSpells[0]->GetPrediction(hero);
        observedChance = prediction.Hitchance;
        directPoint = prediction.GetCastPosition();
        if (!directPoint.IsValid() || directPoint.IsZero()) {
            directPoint = PredictPosition(hero, kBandageCastSeconds);
        }
        const SDK::HitChance required = RequiredBandageHitchance(hero, purpose);
        if (static_cast<int>(observedChance) < static_cast<int>(required) &&
            !Engine::IsHardCrowdControlled(hero) && !hero.IsDashing()) {
            return best;
        }
        const bool shielded = HasSpellShieldOrImmunity(hero);
        if (shielded && !allowSpellShieldMobility) return best;
        best.SpellShieldMobility = shielded;
    }

    Vector3 direct = SharedGeometry::Direction2D(
        player.Position(), directPoint);
    if (direct.IsZero()) return best;
    static constexpr std::array<float, 9> offsets = {
        0.0f, 0.018f, -0.018f, 0.035f, -0.035f,
        0.055f, -0.055f, 0.080f, -0.080f,
    };
    float bestScore = -FLT_MAX;
    const float provisionalEntry = std::max(
        0.0f, centerDistance - intended.BoundingRadius() -
                  kBandageHalfWidth);
    const float impactDelay = BandageMissileSeconds(provisionalEntry);
    // This snapshot is independent of the angular offset.  Build it lazily
    // (only after the first clear projectile ray) and then reuse it for all
    // remaining offsets, removing up to eight full object-list scans.
    std::vector<LineUnit> units;
    bool unitsBuilt = false;
    for (const float offset : offsets) {
        const Vector3 direction = SharedGeometry::Rotate2D(direct, offset);
        if (direction.IsZero()) continue;
        const Vector3 castAim = origin + direction * kBandageRange;
        if (ControllerHelpers::ProjectileWallBlocksFromPlayer(
                castAim, kBandageHalfWidth)) {
            continue;
        }
        if (!unitsBuilt) {
            units = BandageCollisionUnits(impactDelay);
            unitsBuilt = true;
        }
        const int firstIndex = FirstBandageCollisionIndex(
            origin, direction, units);
        if (firstIndex < 0) continue;
        const LineUnit& collision = units[static_cast<std::size_t>(firstIndex)];
        const bool intendedFirst = collision.Id ==
            static_cast<int>(intended.NetworkId());
        if (!intendedFirst) continue;
        const float entry = BandageEntryDistance(
            origin, direction, collision);
        const float impact = BandageMissileSeconds(entry);
        const float arrival = BandageArrivalSeconds(
            origin.Distance2D(collision.Position), entry,
            player.BoundingRadius(), intended.BoundingRadius());
        const Vector3 arrivalPosition = collision.Position;
        const bool safe = ArrivalSafe(arrivalPosition, purpose, hero);
        float score = (safe ? 1000.0f : 0.0f) -
                      std::fabs(offset) * 480.0f -
                      arrival * 18.0f;
        if (purpose == BandagePurpose::Peel ||
            purpose == BandagePurpose::Interrupt) {
            score += 150.0f;
        }
        if (score > bestScore) {
            bestScore = score;
            best.Aim = player.Position() + direction * kBandageRange;
            best.CollisionPosition = collision.Position;
            best.ArrivalPosition = arrivalPosition;
            best.IntendedId = static_cast<int>(intended.NetworkId());
            best.CollisionId = collision.Id;
            best.EntryDistance = entry;
            best.ImpactSeconds = impact;
            best.ArrivalSeconds = arrival;
            best.Hitchance = observedChance;
            best.IntendedFirst = true;
            best.ArrivalSafe = safe;
            best.Valid = true;
        }
    }
    BandagePlanCacheEntry& cache =
        BandagePlanCaches[BandagePlanCacheCursor % BandagePlanCaches.size()];
    BandagePlanCacheCursor =
        (BandagePlanCacheCursor + 1) % BandagePlanCaches.size();
    cache.Tick = now;
    cache.IntendedId = intendedId;
    cache.Purpose = purpose;
    cache.AllowSpellShieldMobility = allowSpellShieldMobility;
    cache.Origin = origin;
    cache.IntendedPosition = intendedPosition;
    cache.Plan = best;
    return best;
}

inline float ExistingCrowdControlSeconds(const AIHeroClient& enemy) {
    if (!Engine::ValidEnemy(enemy)) return 0.0f;
    const int id = static_cast<int>(enemy.NetworkId());
    if (id == QTargetId && QStunUntil > Now()) {
        return static_cast<float>(QStunUntil - Now()) / 1000.0f;
    }
    if (RStunUntil > Now() && LastUltimatePlan.Valid) {
        for (const int hitId : LastUltimatePlan.HitIds) {
            if (hitId == id) {
                return static_cast<float>(RStunUntil - Now()) / 1000.0f;
            }
        }
    }
    return Engine::IsHardCrowdControlled(enemy) ? 0.90f : 0.0f;
}

inline float EnemyUltimatePriority(const AIHeroClient& enemy) {
    if (!Engine::ValidEnemy(enemy)) return 0.0f;
    const float offense = std::max(
        enemy.TotalAttackDamage() * 0.0062f,
        enemy.AP() * 0.0048f);
    const float range = std::max(0.0f, enemy.AttackRange() - 175.0f) *
                        0.0012f;
    const float wounded = (100.0f - enemy.HealthPercent()) * 0.003f;
    float priority = 0.85f + offense + range + wounded;
    if (static_cast<int>(enemy.NetworkId()) == PeelThreatId) priority += 0.70f;
    return std::clamp(priority, 0.65f, 3.25f);
}

inline UltimatePlan BuildUltimatePlan(const Vector3& center,
                                      float delaySeconds,
                                      int selectedId = 0) {
    UltimatePlan plan{};
    plan.Center = center;
    if (!center.IsValid() || center.IsZero()) return plan;
    std::vector<UltimateUnit> pureUnits;
    std::vector<int> ids;
    pureUnits.reserve(10);
    ids.reserve(10);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        Vector3 position = PredictPosition(enemy, delaySeconds);
        if (!position.IsValid() || position.IsZero()) position = enemy.Position();
        const bool shield = HasSpellShieldOrImmunity(enemy);
        const bool cleanse = Bool(UltimateMenu, "RespectCleanse", true) &&
                             EnemyCanCleanse(enemy);
        const float cc = Bool(UltimateMenu, "AvoidCCOverlap", true)
            ? ExistingCrowdControlSeconds(enemy)
            : 0.0f;
        const bool dashing = enemy.IsDashing() && enemy.PathEnd().IsValid() &&
            CurseHits(center, enemy.PathEnd(), enemy.BoundingRadius());
        pureUnits.push_back(UltimateUnit{
            position, enemy.BoundingRadius(), EnemyUltimatePriority(enemy),
            cc, shield, cleanse, dashing, true,
        });
        ids.push_back(static_cast<int>(enemy.NetworkId()));
    }
    plan.RawHitCount = UltimateHitCount(center, pureUnits, true);
    plan.EffectiveHitCount = UltimateHitCount(center, pureUnits, false);
    plan.Score = UltimateScore(center, pureUnits);
    for (std::size_t i = 0; i < pureUnits.size(); ++i) {
        const auto& unit = pureUnits[i];
        if (!unit.Valid || !CurseHits(center, unit.Position, unit.Radius)) {
            continue;
        }
        const int id = ids[i];
        for (auto& slot : plan.HitIds) {
            if (slot == 0) {
                slot = id;
                break;
            }
        }
        const float score = UltimateUnitScore(center, unit);
        if (score > plan.PrimaryScore) {
            plan.PrimaryScore = score;
            plan.PrimaryId = id;
        }
        if (id == PeelThreatId) plan.IncludesProtectedThreat = true;
        if (id == selectedId) plan.IncludesSelected = true;
    }
    plan.Valid = plan.RawHitCount > 0;
    return plan;
}

inline bool ImmediateArrivalUltimateWanted(const BandagePlan& qPlan,
                                           const AIHeroClient& target) {
    if (!Bool(UltimateMenu, "BufferAfterQ", true) || !Ready(3) ||
        SpellRank(3) <= 0 || !Engine::ValidEnemy(target)) {
        return false;
    }
    const UltimatePlan rPlan = BuildUltimatePlan(
        qPlan.ArrivalPosition, qPlan.ArrivalSeconds + 0.25f,
        static_cast<int>(target.NetworkId()));
    const int minimum = Slider(UltimateMenu, "MinimumTargets", 2);
    const float minimumScore = static_cast<float>(
        Slider(UltimateMenu, "MinimumScore", 210)) / 100.0f;
    if (rPlan.EffectiveHitCount >= minimum && rPlan.Score >= minimumScore) {
        return true;
    }
    const bool highValue = rPlan.PrimaryId ==
            static_cast<int>(target.NetworkId()) &&
        rPlan.PrimaryScore >= 1.55f &&
        CountAlliedFollowup(qPlan.ArrivalPosition, 900.0f) >= 1;
    return Bool(UltimateMenu, "SingleCarryArrival", true) && highValue &&
           (EnemyFlashReady(target) || target.IsDashing() ||
            target.HealthPercent() <=
                static_cast<float>(Slider(UltimateMenu, "SingleCarryHp", 42)));
}

inline bool CastBandage(const AIBaseClient& intended,
                        BandagePurpose purpose,
                        Mode mode,
                        bool spendReservedCharge = false,
                        bool allowSpellShieldMobility = false,
                        int followChampionId = 0,
                        bool reactive = false) {
    if (!Ready(0) || !SpellEnabled(0, mode) ||
        !CastThrottleReady(0, reactive) || PlayerMobilityLocked() ||
        !HasCurrentResource(SpellCost(0))) {
        return false;
    }
    const int charges = RuntimeQCharges();
    if (charges <= 0 ||
        (!spendReservedCharge && charges <= ReservedQCharges())) {
        return false;
    }
    if (!reactive && Orbwalker::IsWindingUp() &&
        Orbwalker::AttackCastDelayRemaining() > 25) {
        return false;
    }
    if ((purpose == BandagePurpose::Engage ||
         purpose == BandagePurpose::Bridge) &&
        !CursorAgrees(intended.Position())) {
        return false;
    }
    AIHeroClient hero = intended.IsHero()
        ? AIHeroClient(intended.Address())
        : AIHeroClient{};
    const bool allowShield = allowSpellShieldMobility &&
        Bool(BandageMenu, "ShieldMobility", false);
    const BandagePlan plan = BuildBandagePlan(intended, purpose, allowShield);
    if (!plan.Valid || !plan.IntendedFirst || !plan.ArrivalSafe) return false;

    if (hero.IsValid() && purpose == BandagePurpose::Engage &&
        !Engine::IsHardCrowdControlled(hero) && !hero.IsDashing()) {
        const float distance = GameObjects::Player().Position().Distance2D(
            hero.Position());
        const float maximum = static_cast<float>(
            Slider(BandageMenu, "MaximumNakedRange", 1000));
        if (distance > maximum && !ConservativeKillable(hero) &&
            !ImmediateArrivalUltimateWanted(plan, hero)) {
            return false;
        }
    }
    if (plan.SpellShieldMobility &&
        (purpose != BandagePurpose::Engage || !Ready(3) ||
         CountAlliedFollowup(plan.ArrivalPosition, 900.0f) <= 0)) {
        return false;
    }
    if (!Engine::ControllerCastPosition(0, plan.Aim)) return false;

    QCastTick = Now();
    QExpectedImpactTick = QCastTick + static_cast<int>(
        std::ceil(plan.ImpactSeconds * 1000.0f));
    QExpectedArrivalTick = QCastTick + static_cast<int>(
        std::ceil(plan.ArrivalSeconds * 1000.0f));
    QArrivalGraceUntil = QExpectedArrivalTick + kQSequenceGraceMs;
    QTargetId = plan.CollisionId;
    QFollowChampionId = followChampionId;
    QStunUntil = QExpectedImpactTick + kBandageStunMs;
    QPendingArrival = true;
    QDashObserved = false;
    QCastOrigin = GameObjects::Player().Position();
    LastBandagePlan = plan;
    LastBandagePurpose = purpose;
    if (QObservedAmmo > 0) --QObservedAmmo;

    switch (purpose) {
    case BandagePurpose::Bridge:
        ActiveSequence = Sequence::MinionBridge;
        break;
    case BandagePurpose::FollowDash:
        ActiveSequence = Sequence::FollowFlashOrDash;
        break;
    case BandagePurpose::Peel:
    case BandagePurpose::Interrupt:
        ActiveSequence = Sequence::PeelChain;
        break;
    case BandagePurpose::Escape:
        ActiveSequence = Sequence::EscapeAnchor;
        break;
    case BandagePurpose::Jungle:
        ActiveSequence = Sequence::JungleCycle;
        break;
    default:
        ActiveSequence = Sequence::FirstBandage;
        break;
    }

    if (hero.IsValid() && ImmediateArrivalUltimateWanted(plan, hero)) {
        ActiveSequence = Sequence::ArrivalUltimate;
        QForcedAutoSuppressUntil = QArrivalGraceUntil;
    } else if (hero.IsValid()) {
        ActiveSequence = Sequence::ArrivalCurseWeave;
    }
    WExpectedContactUntil = QArrivalGraceUntil + 350;
    return true;
}

inline float DespairReserve(Mode mode) {
    float reserve = static_cast<float>(
        Slider(DespairMenu, "FlatReserve", 70));
    if (Ready(0)) reserve = std::max(reserve, SpellCost(0));
    if (Ready(2)) reserve += SpellCost(2);
    if (Ready(3) && SpellRank(3) > 0 &&
        (mode == Mode::Combo || mode == Mode::Automatic ||
         QPendingArrival)) {
        reserve += SpellCost(3);
    }
    return reserve;
}

inline int HostileAuraCount(bool includeLaneMinions,
                            bool includeJungle) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && DespairHits(
                player.Position(), enemy.Position(),
                enemy.BoundingRadius())) {
            ++count;
        }
    }
    if (includeLaneMinions) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (minion.IsValid() && !minion.IsDead() &&
                minion.IsTargetable() && DespairHits(
                    player.Position(), minion.Position(),
                    minion.BoundingRadius())) {
                ++count;
            }
        }
    }
    if (includeJungle) {
        for (const auto& monster : GameObjects::Jungle()) {
            if (monster.IsValid() && !monster.IsDead() &&
                monster.IsTargetable() && DespairHits(
                    player.Position(), monster.Position(),
                    monster.BoundingRadius())) {
                ++count;
            }
        }
    }
    return count;
}

inline bool EpicMonsterInAura() {
    const auto player = GameObjects::Player();
    for (const auto& monster : GameObjects::Jungle()) {
        if (monster.IsValid() && !monster.IsDead() &&
            monster.IsTargetable() && IsEpicMonster(monster) &&
            DespairHits(player.Position(), monster.Position(),
                        monster.BoundingRadius())) {
            return true;
        }
    }
    return false;
}

inline bool CastDespair(bool enable,
                        Mode mode,
                        bool fastFollowup = false) {
    if (enable == WActive || !Ready(1) || !SpellEnabled(1, mode) ||
        !CastThrottleReady(1, fastFollowup)) {
        return false;
    }
    if (enable && !HasCurrentResource(
            DespairReserve(mode) + kWDrainPerSecond * 1.5f)) {
        return false;
    }
    if (!Engine::ControllerCastSelf(1)) return false;
    WActive = enable;
    WToggleTick = Now();
    return true;
}

inline bool TryManageDespair(Mode mode,
                             const AIHeroClient& target = {}) {
    if (!Bool(DespairMenu, "AutoToggle", true) || !Ready(1)) {
        return false;
    }
    const bool lane = mode == Mode::LaneClear &&
                      Bool(FarmMenu, "LaneW", false);
    const bool jungle = mode == Mode::LaneClear && JungleRole();
    const int contacts = HostileAuraCount(lane, jungle);
    if (contacts > 0) {
        WLastContactTick = Now();
        if (!WActive) {
            return CastDespair(true, mode, QPendingArrival);
        }
        return false;
    }

    if (!WActive && QPendingArrival &&
        Bool(DespairMenu, "PreToggleQ", true) &&
        QExpectedArrivalTick - Now() <=
            Slider(DespairMenu, "PreToggleLead", 650) &&
        QExpectedArrivalTick > Now()) {
        return CastDespair(true, mode, true);
    }

    if (!WActive) return false;
    const bool keepEpic = Bool(DespairMenu, "KeepEpic", true) &&
                          EpicMonsterInAura();
    const bool reserveBroken = CurrentResource() <=
        DespairReserve(mode) + kWDrainPerSecond * 0.75f;
    const bool contactExpired = Now() - WLastContactTick >=
        Slider(DespairMenu, "OffDelay", 420);
    const bool noArrival = !QPendingArrival &&
                           Now() > WExpectedContactUntil;
    if (!keepEpic && (reserveBroken || (contactExpired && noArrival))) {
        return CastDespair(false, mode, true);
    }
    (void)target;
    return false;
}

inline void RecordIncomingAttack(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !args.Sender.IsValid() ||
        IsLocalPlayer(args.Sender) || !args.IsAutoAttack) {
        return;
    }
    const std::uint32_t playerId =
        static_cast<std::uint32_t>(player.NetworkId());
    const std::uint32_t targetId = args.TargetNetworkId != 0
        ? args.TargetNetworkId
        : args.Target.NetworkId;
    if (targetId != playerId) return;

    const int sourceId = static_cast<int>(args.Sender.NetworkId);
    int castMs = ControllerHelpers::NormalizedCastDelayMs(
        args.CastDelay, 180);
    float travelMs = 0.0f;
    const float speed = args.MissileSpeed > 1.0f
        ? args.MissileSpeed
        : 0.0f;
    if (speed > 1.0f && args.StartPosition.IsValid()) {
        travelMs = args.StartPosition.Distance2D(player.Position()) /
                   speed * 1000.0f;
    }
    const int impact = Now() + std::clamp(
        castMs + static_cast<int>(travelMs), 40, 1800);
    IncomingAttack* slot = nullptr;
    for (auto& attack : IncomingAttacks) {
        if (attack.SourceId == sourceId &&
            std::abs(attack.ImpactTick - impact) <= 120) {
            attack.ImpactTick = impact;
            attack.ExpireTick = impact + 350;
            return;
        }
        if (!slot && (attack.SourceId == 0 || attack.ExpireTick < Now())) {
            slot = &attack;
        }
    }
    if (!slot) slot = &IncomingAttacks[0];
    *slot = IncomingAttack{ sourceId, impact, impact + 350 };
}

inline int IncomingAttackCount(int withinMs) {
    int count = 0;
    const int limit = Now() + std::max(0, withinMs);
    for (const auto& attack : IncomingAttacks) {
        if (attack.SourceId != 0 && attack.ExpireTick >= Now() &&
            attack.ImpactTick <= limit) {
            ++count;
        }
    }
    return count;
}

inline float TantrumCooldownRemaining() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return FLT_MAX;
    const auto spell = player.Spellbook().GetSpell(SDK::SpellSlot::E);
    if (!spell.IsValid()) return FLT_MAX;
    return std::max(0.0f, spell.RemainingCooldown(Game::Time()));
}

inline bool TantrumWillRefreshSoon(int horizonMs = 520) {
    if (Ready(2)) return true;
    const int incoming = IncomingAttackCount(horizonMs);
    return incoming > 0 && TantrumRemainingCooldown(
        TantrumCooldownRemaining(), incoming) <=
            static_cast<float>(horizonMs) / 1000.0f + 0.08f;
}

inline bool CastTantrum(const AIBaseClient& target,
                        Mode mode,
                        bool reactive = false,
                        bool allowWithoutCurse = false) {
    if (!Ready(2) || !SpellEnabled(2, mode) ||
        !CastThrottleReady(2, reactive) ||
        !HasCurrentResource(SpellCost(2))) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (target.IsValid()) {
        const Vector3 predicted = PredictPosition(target, 0.25f);
        if (!TantrumHits(player.Position(), predicted,
                         target.BoundingRadius())) {
            return false;
        }
        if (!allowWithoutCurse && target.IsHero() &&
            Bool(TantrumMenu, "WeaveCurse", true) &&
            !HasCurse(target) && InAutoAttackRange(target) &&
            EDamage(target) < target.Health() &&
            Now() - QExpectedArrivalTick > 180) {
            return false;
        }
    }
    if (!reactive && Orbwalker::IsWindingUp() &&
        Orbwalker::AttackCastDelayRemaining() > 25) {
        return false;
    }
    if (!Engine::ControllerCastSelf(2)) return false;
    ECastTick = Now();
    ETargetId = target.IsValid()
        ? static_cast<int>(target.NetworkId())
        : 0;
    return true;
}

inline bool UltimateReady() {
    return SpellRank(3) > 0 && Ready(3) &&
           HasCurrentResource(SpellCost(3));
}

inline bool UltimatePlanMeetsReason(const UltimatePlan& plan,
                                   UltimateReason reason,
                                   const AIHeroClient& selected) {
    if (!plan.Valid) return false;
    const int minimum = Slider(UltimateMenu, "MinimumTargets", 2);
    const float scoreMinimum = static_cast<float>(
        Slider(UltimateMenu, "MinimumScore", 210)) / 100.0f;
    switch (reason) {
    case UltimateReason::QArrival:
        return plan.EffectiveHitCount >= minimum ||
               (plan.IncludesSelected && plan.PrimaryScore >= 1.55f);
    case UltimateReason::MultiTarget:
        return plan.EffectiveHitCount >= minimum &&
               plan.Score >= scoreMinimum;
    case UltimateReason::HighValuePick:
        return plan.IncludesSelected && plan.PrimaryId ==
                   static_cast<int>(selected.NetworkId()) &&
               plan.PrimaryScore >= 1.65f &&
               CountAlliedFollowup(plan.Center, 900.0f) >= 1;
    case UltimateReason::Peel:
        return plan.IncludesProtectedThreat;
    case UltimateReason::AntiDash:
    case UltimateReason::Interrupt:
        return plan.IncludesSelected;
    case UltimateReason::Lethal:
        return plan.IncludesSelected && Engine::ValidEnemy(selected) &&
               RDamage(selected, HasCurse(selected)) >= selected.Health();
    case UltimateReason::Survival:
        return plan.EffectiveHitCount >= 1;
    case UltimateReason::ManualFlash:
        return plan.EffectiveHitCount >= minimum ||
               (plan.IncludesSelected && plan.PrimaryScore >= 1.45f);
    default:
        return false;
    }
}

inline bool CastUltimate(UltimateReason reason,
                         Mode mode,
                         const AIHeroClient& selected,
                         const Vector3& expectedCenter = {},
                         bool fastFollowup = false) {
    if (!UltimateReady() || !SpellEnabled(3, mode) ||
        !CastThrottleReady(3, fastFollowup)) {
        return false;
    }
    const auto player = GameObjects::Player();
    const Vector3 center = expectedCenter.IsValid() &&
            !expectedCenter.IsZero()
        ? expectedCenter
        : player.Position();
    const float delay = fastFollowup && QPendingArrival
        ? std::max(0.25f,
              static_cast<float>(QExpectedArrivalTick - Now()) / 1000.0f +
                  0.25f)
        : 0.25f;
    const UltimatePlan plan = BuildUltimatePlan(
        center, delay,
        selected.IsValid() ? static_cast<int>(selected.NetworkId()) : 0);
    if (!UltimatePlanMeetsReason(plan, reason, selected)) return false;
    if (!fastFollowup && Orbwalker::IsWindingUp() &&
        Orbwalker::AttackCastDelayRemaining() > 25 &&
        reason != UltimateReason::Peel &&
        reason != UltimateReason::Interrupt &&
        reason != UltimateReason::AntiDash) {
        return false;
    }
    if (!Engine::ControllerCastSelf(3)) return false;
    RCastTick = Now();
    RResolveTick = RCastTick + 250;
    RStunUntil = RResolveTick + kUltimateStunMs;
    RPendingMarkTick = RResolveTick;
    RPendingResolve = true;
    RWasManual = false;
    RCastTargetId = selected.IsValid()
        ? static_cast<int>(selected.NetworkId())
        : plan.PrimaryId;
    LastUltimatePlan = plan;
    LastUltimateReason = reason;
    QForcedAutoSuppressUntil = 0;
    return true;
}

inline AIHeroClient ProtectedAlly() {
    AIHeroClient current{};
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (static_cast<int>(ally.NetworkId()) == ProtectedAllyId &&
            Engine::ValidAlly(ally, 1800.0f)) {
            current = ally;
            break;
        }
    }
    if (!current.IsValid()) {
        current = SelectProtectionAlly(
            1650.0f, TargetedAllyThreatId, TargetedAllyThreatUntil,
            255.0f, 580.0f);
        ProtectedAllyId = current.IsValid()
            ? static_cast<int>(current.NetworkId())
            : 0;
    }
    return current;
}

inline float PeelThreatScore(const AIHeroClient& enemy,
                             const AIHeroClient& ally) {
    if (!Engine::ValidEnemy(enemy) || !Engine::ValidAlly(ally)) {
        return -FLT_MAX;
    }
    const float distance = enemy.Position().Distance2D(ally.Position());
    float score = 900.0f - distance;
    score += std::max(enemy.TotalAttackDamage(), enemy.AP() * 0.75f);
    if (enemy.IsDashing() && enemy.PathEnd().IsValid() &&
        enemy.PathEnd().Distance2D(ally.Position()) < 475.0f) {
        score += 650.0f;
    }
    if (static_cast<int>(enemy.NetworkId()) == TargetedAllyThreatId &&
        Now() <= TargetedAllyThreatUntil) {
        score += 700.0f;
    }
    if (ally.HealthPercent() <= 35.0f) score += 300.0f;
    return score;
}

inline AIHeroClient SelectPeelThreat(const AIHeroClient& ally) {
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1400.0f) ||
            enemy.Position().Distance2D(ally.Position()) > 825.0f) {
            continue;
        }
        const float score = PeelThreatScore(enemy, ally);
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    PeelThreatId = best.IsValid()
        ? static_cast<int>(best.NetworkId())
        : 0;
    return best;
}

inline bool LayeredBandageWindow(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target) || !Ready(0)) return false;
    if (target.IsDashing()) return true;
    const BandagePlan plan = BuildBandagePlan(
        target, BandagePurpose::FollowDash, false);
    if (!plan.Valid) return false;
    const float remaining = ExistingCrowdControlSeconds(target);
    if (!Bool(BandageMenu, "LayerCrowdControl", true)) return true;
    return ShouldLayerBandage(
        remaining, plan.ImpactSeconds,
        static_cast<float>(Slider(
            BandageMenu, "OverlapAllowance", 100)) / 1000.0f);
}

inline bool TryLayerSecondBandage(const AIHeroClient& target,
                                  Mode mode,
                                  BandagePurpose purpose =
                                      BandagePurpose::FollowDash) {
    if (!Engine::ValidEnemy(target) || RuntimeQCharges() <= 0 ||
        !LayeredBandageWindow(target)) {
        return false;
    }
    const bool urgent = target.IsDashing() ||
        (QFollowChampionId != 0 &&
         static_cast<int>(target.NetworkId()) == QFollowChampionId) ||
        (RStunUntil > Now() && RStunUntil - Now() <= 650) ||
        ConservativeKillable(target, false);
    if (!urgent && RuntimeQCharges() <= ReservedQCharges()) return false;
    if (CastBandage(target, purpose, mode, true, false, 0, true)) {
        ActiveSequence = Sequence::LayerSecondBandage;
        return true;
    }
    return false;
}

inline bool HandleBandageSequence(Mode mode,
                                  const AIHeroClient& selected) {
    const auto player = GameObjects::Player();
    AIHeroClient contact = HeroByNetworkId(QTargetId);
    AIHeroClient follow = HeroByNetworkId(QFollowChampionId);
    if (!Engine::ValidEnemy(follow)) follow = selected;

    if (QPendingArrival) {
        if (ActiveSequence == Sequence::ArrivalUltimate &&
            UltimateReady() &&
            (QDashObserved || player.IsDashing() ||
             Now() >= QExpectedImpactTick - 20) &&
            QExpectedArrivalTick - Now() <=
                Slider(UltimateMenu, "BufferLead", 520)) {
            AIHeroClient rTarget = Engine::ValidEnemy(contact)
                ? contact
                : follow;
            Mode castMode = mode == Mode::Combo
                ? Mode::Combo
                : Mode::Automatic;
            if (CastUltimate(
                    UltimateReason::QArrival, castMode, rTarget,
                    LastBandagePlan.ArrivalPosition, true)) {
                return true;
            }
        }
        if (!WActive && Bool(DespairMenu, "PreToggleQ", true) &&
            QExpectedArrivalTick > Now() &&
            QExpectedArrivalTick - Now() <=
                Slider(DespairMenu, "PreToggleLead", 650) &&
            CastDespair(true, mode == Mode::None
                                  ? Mode::Automatic
                                  : mode,
                        true)) {
            return true;
        }
        return false;
    }

    if (Now() > QArrivalGraceUntil + 1100 &&
        ActiveSequence != Sequence::LayerSecondBandage) {
        return false;
    }

    if (ActiveSequence == Sequence::MinionBridge &&
        Engine::ValidEnemy(follow)) {
        if (TryLayerSecondBandage(
                follow, mode == Mode::None ? Mode::Combo : mode,
                BandagePurpose::FollowDash)) {
            QFollowChampionId = 0;
            return true;
        }
        return false;
    }

    if (ActiveSequence == Sequence::ArrivalUltimate &&
        UltimateReady() && Engine::ValidEnemy(contact) &&
        CastUltimate(
            UltimateReason::QArrival,
            mode == Mode::None ? Mode::Automatic : mode,
            contact, player.Position(), true)) {
        return true;
    }

    if (ActiveSequence == Sequence::ArrivalCurseWeave &&
        Engine::ValidEnemy(contact)) {
        if (!HasCurse(contact) && InAutoAttackRange(contact)) {
            // Q issues a game-side AA order.  Preserve it unless R urgency
            // explicitly armed the suppression branch.
            return false;
        }
        if ((HasCurse(contact) ||
             (LastAutoTargetId == static_cast<int>(contact.NetworkId()) &&
              Now() - LastAutoTick <= 550)) &&
            CastTantrum(contact, mode, false, true)) {
            return true;
        }
        if (TryLayerSecondBandage(contact, mode)) return true;
    }
    return false;
}

inline bool TryInterrupt() {
    if (InterruptTargetId == 0 || Now() > InterruptExpireTick) {
        return false;
    }
    AIHeroClient target = HeroByNetworkId(InterruptTargetId);
    if (!Engine::ValidEnemy(target)) return false;
    const BandagePlan qPlan = BuildBandagePlan(
        target, BandagePurpose::Interrupt, false);
    const int remaining = InterruptExpireTick - Now();
    if (Bool(BandageMenu, "InterruptQ", true) && qPlan.Valid &&
        qPlan.ImpactSeconds * 1000.0f <=
            static_cast<float>(remaining + 80) &&
        CastBandage(target, BandagePurpose::Interrupt,
                    Mode::Automatic, true, false, 0, true)) {
        InterruptTargetId = 0;
        return true;
    }
    const auto player = GameObjects::Player();
    if (Bool(UltimateMenu, "InterruptR", true) &&
        CurseHits(player.Position(), PredictPosition(target, 0.25f),
                  target.BoundingRadius()) &&
        CastUltimate(UltimateReason::Interrupt, Mode::Automatic,
                     target, player.Position(), true)) {
        InterruptTargetId = 0;
        return true;
    }
    return false;
}

inline bool TryPeel(const AIHeroClient& ally,
                    const AIHeroClient& threat,
                    Mode currentMode) {
    if (!Engine::ValidAlly(ally) || !Engine::ValidEnemy(threat)) {
        return false;
    }
    CurrentPosture = Posture::Peel;
    const auto player = GameObjects::Player();
    const float allyDistance = threat.Position().Distance2D(ally.Position());
    const bool critical = ally.HealthPercent() <=
            static_cast<float>(Slider(UltimateMenu, "PeelAllyHp", 42)) ||
        (static_cast<int>(threat.NetworkId()) == TargetedAllyThreatId &&
         Now() <= TargetedAllyThreatUntil) || threat.IsDashing();
    const Mode mode = currentMode == Mode::Combo
        ? Mode::Combo
        : Mode::Automatic;

    if (critical && Bool(UltimateMenu, "PeelR", true) &&
        CurseHits(player.Position(), PredictPosition(threat, 0.25f),
                  threat.BoundingRadius()) &&
        CastUltimate(UltimateReason::Peel, mode, threat,
                     player.Position(), true)) {
        return true;
    }
    if (Bool(BandageMenu, "PeelQ", true) && allyDistance <= 700.0f &&
        CastBandage(threat, BandagePurpose::Peel, mode,
                    true, false, 0, true)) {
        ActiveSequence = Sequence::PeelChain;
        return true;
    }
    if (TantrumHits(player.Position(), PredictPosition(threat, 0.25f),
                    threat.BoundingRadius()) &&
        CastTantrum(threat, mode, true, true)) {
        return true;
    }
    return false;
}

inline bool TryReactiveUltimate(const AIHeroClient& selected,
                                Mode mode) {
    if (!UltimateReady()) return false;
    const auto player = GameObjects::Player();
    const Mode castMode = mode == Mode::Combo
        ? Mode::Combo
        : Mode::Automatic;

    if (ManualFlashUntil >= Now() &&
        Bool(UltimateMenu, "AfterManualFlash", true) &&
        CastUltimate(UltimateReason::ManualFlash, castMode,
                     selected, player.Position(), true)) {
        ManualFlashUntil = 0;
        return true;
    }

    if (GapcloserTargetId != 0 && Now() <= GapcloserExpireTick &&
        Bool(UltimateMenu, "AntiDashR", true)) {
        AIHeroClient dasher = HeroByNetworkId(GapcloserTargetId);
        if (Engine::ValidEnemy(dasher) &&
            CurseHits(player.Position(), GapcloserEnd,
                      dasher.BoundingRadius()) &&
            CastUltimate(UltimateReason::AntiDash, castMode,
                         dasher, player.Position(), true)) {
            GapcloserTargetId = 0;
            return true;
        }
    }

    const UltimatePlan plan = BuildUltimatePlan(
        player.Position(), 0.25f,
        selected.IsValid() ? static_cast<int>(selected.NetworkId()) : 0);
    if (Bool(UltimateMenu, "MultiTarget", true) &&
        UltimatePlanMeetsReason(
            plan, UltimateReason::MultiTarget, selected) &&
        CastUltimate(UltimateReason::MultiTarget, castMode,
                     selected, player.Position())) {
        return true;
    }
    if (Engine::ValidEnemy(selected) &&
        Bool(UltimateMenu, "LethalSingle", true) &&
        RDamage(selected, HasCurse(selected)) >= selected.Health() &&
        ConservativeComboDamage(selected, false) < selected.Health() &&
        CastUltimate(UltimateReason::Lethal, castMode,
                     selected, player.Position())) {
        return true;
    }
    if (Engine::ValidEnemy(selected) &&
        Bool(UltimateMenu, "HighValuePick", true) &&
        selected.HealthPercent() <=
            static_cast<float>(Slider(UltimateMenu, "PickHp", 58)) &&
        CastUltimate(UltimateReason::HighValuePick, castMode,
                     selected, player.Position())) {
        return true;
    }
    if (Bool(UltimateMenu, "SurvivalR", true) &&
        player.HealthPercent() <=
            static_cast<float>(Slider(UltimateMenu, "SurvivalHp", 27)) &&
        IncomingThreatUntil >= Now() &&
        CastUltimate(UltimateReason::Survival, castMode,
                     selected, player.Position(), true)) {
        return true;
    }
    return false;
}

inline AIBaseClient BestBridgeAnchor(const AIHeroClient& champion) {
    const auto player = GameObjects::Player();
    if (!Engine::ValidEnemy(champion) || RuntimeQCharges() < 2 ||
        player.Position().Distance2D(champion.Position()) <=
            kBandageRange - 40.0f ||
        player.Position().Distance2D(champion.Position()) > 2250.0f) {
        return {};
    }
    AIBaseClient best{};
    float bestScore = -FLT_MAX;
    const auto consider = [&](const AIBaseClient& unit) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(unit.Position()) >
                kBandageRange + unit.BoundingRadius()) {
            return;
        }
        const Vector3 landing = PredictPosition(unit, 0.75f);
        if (!BridgeImprovesReach(
                player.Position(), landing, champion.Position(),
                kBandageRange,
                static_cast<float>(Slider(
                    BandageMenu, "BridgeMinimumGain", 300))) ||
            !ArrivalSafe(landing, BandagePurpose::Bridge, champion) ||
            !CursorAgrees(landing, 0.10f)) {
            return;
        }
        const BandagePlan plan = BuildBandagePlan(
            unit, BandagePurpose::Bridge, false);
        if (!plan.Valid || !plan.IntendedFirst || !plan.ArrivalSafe) return;
        float score = 1500.0f - landing.Distance2D(champion.Position()) -
                      plan.ArrivalSeconds * 80.0f;
        score += static_cast<float>(
            CountAlliedFollowup(landing, 900.0f)) * 160.0f;
        if (score > bestScore) {
            best = unit;
            bestScore = score;
        }
    };
    for (const auto& minion : GameObjects::EnemyMinions()) {
        consider(AIBaseClient(minion.Address()));
    }
    for (const auto& monster : GameObjects::Jungle()) {
        consider(AIBaseClient(monster.Address()));
    }
    return best;
}

inline AIBaseClient BestEscapeAnchor(const AIHeroClient& pursuer) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    const Vector3 cursorDirection = SharedGeometry::Direction2D(
        player.Position(), Game::CursorPos());
    AIBaseClient best{};
    float bestScore = -FLT_MAX;
    const auto consider = [&](const AIBaseClient& unit) {
        if (!unit.IsValid() || unit.IsDead() || !unit.IsTargetable() ||
            player.Position().Distance2D(unit.Position()) >
                kBandageRange + unit.BoundingRadius()) {
            return;
        }
        const Vector3 direction = SharedGeometry::Direction2D(
            player.Position(), unit.Position());
        if (direction.IsZero() || (!cursorDirection.IsZero() &&
            direction.Dot(cursorDirection) < 0.18f)) {
            return;
        }
        const Vector3 landing = PredictPosition(unit, 0.65f);
        if (!ArrivalSafe(landing, BandagePurpose::Escape) ||
            (Engine::ValidEnemy(pursuer) &&
             landing.Distance2D(pursuer.Position()) <=
                 player.Position().Distance2D(pursuer.Position()) + 170.0f)) {
            return;
        }
        const BandagePlan plan = BuildBandagePlan(
            unit, BandagePurpose::Escape, false);
        if (!plan.Valid || !plan.ArrivalSafe) return;
        float score = landing.Distance2D(
            Engine::ValidEnemy(pursuer)
                ? pursuer.Position()
                : player.Position());
        score -= landing.Distance2D(Game::CursorPos()) * 0.15f;
        if (score > bestScore) {
            best = unit;
            bestScore = score;
        }
    };
    for (const auto& minion : GameObjects::EnemyMinions()) {
        consider(AIBaseClient(minion.Address()));
    }
    for (const auto& monster : GameObjects::Jungle()) {
        consider(AIBaseClient(monster.Address()));
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        consider(AIBaseClient(enemy.Address()));
    }
    return best;
}

inline bool TryFlee(const AIHeroClient& selected) {
    CurrentPosture = Posture::Disengage;
    const AIHeroClient pursuer = NearestEnemyToPlayer(selected, 1300.0f);
    const auto player = GameObjects::Player();
    if (Bool(UltimateMenu, "FleeR", true) &&
        Engine::CountEnemiesAt(player.Position(), kCurseRadius + 65.0f) >=
            Slider(UltimateMenu, "FleeTargets", 2) &&
        CastUltimate(UltimateReason::Survival, Mode::Flee,
                     pursuer, player.Position(), true)) {
        return true;
    }
    if (Engine::ValidEnemy(pursuer) &&
        TantrumHits(player.Position(), PredictPosition(pursuer, 0.25f),
                    pursuer.BoundingRadius()) &&
        CastTantrum(pursuer, Mode::Flee, true, true)) {
        return true;
    }
    if (Bool(BandageMenu, "EscapeAnchor", true)) {
        const AIBaseClient anchor = BestEscapeAnchor(pursuer);
        if (anchor.IsValid() && CastBandage(
                anchor, BandagePurpose::Escape, Mode::Flee,
                true, false, 0, true)) {
            return true;
        }
    }
    return false;
}

inline bool TryCombo(const AIHeroClient& selected) {
    if (!Engine::ValidEnemy(selected)) return false;
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(selected.Position());
    CurrentPosture = distance > 650.0f
        ? Posture::Gank
        : Posture::Followup;

    if (TryReactiveUltimate(selected, Mode::Combo)) return true;

    if (TantrumWillRefreshSoon(
            Slider(TantrumMenu, "RefundWaitMs", 480)) &&
        !Ready(2) && Bool(TantrumMenu, "WaitAttackRefund", true) &&
        TantrumHits(player.Position(), selected.Position(),
                    selected.BoundingRadius())) {
        return false;
    }

    if (Ready(2) && TantrumHits(
            player.Position(), PredictPosition(selected, 0.25f),
            selected.BoundingRadius())) {
        const bool urgent = EDamage(selected) >= selected.Health() ||
            RCastTick > 0 && Now() - RCastTick <= 650 ||
            selected.IsDashing();
        if (CastTantrum(selected, Mode::Combo, urgent, urgent)) {
            return true;
        }
    }

    if (RuntimeQCharges() > 0 &&
        (selected.IsDashing() ||
         (ActiveSequence != Sequence::None &&
          LayeredBandageWindow(selected))) &&
        TryLayerSecondBandage(selected, Mode::Combo)) {
        return true;
    }

    if (Bool(BandageMenu, "Bridge", true) && RuntimeQCharges() >= 2 &&
        HasCurrentResource(SpellCost(0) * 2.0f +
            (UltimateReady() ? SpellCost(3) : 0.0f))) {
        const AIBaseClient bridge = BestBridgeAnchor(selected);
        if (bridge.IsValid() && CastBandage(
                bridge, BandagePurpose::Bridge, Mode::Combo,
                false, false, static_cast<int>(selected.NetworkId()))) {
            return true;
        }
    }

    if (Bool(BandageMenu, "DirectEngage", true) && Ready(0) &&
        distance <= kBandageRange + selected.BoundingRadius() + 45.0f) {
        const bool shieldMobility =
            HasSpellShieldOrImmunity(selected) && Ready(3);
        if (CastBandage(
                selected, BandagePurpose::Engage, Mode::Combo,
                RuntimeQCharges() == 1 && ConservativeKillable(selected),
                shieldMobility)) {
            return true;
        }
    }

    return false;
}

inline bool TryHarass(const AIHeroClient& selected) {
    if (!Engine::ValidEnemy(selected)) return false;
    CurrentPosture = SupportRole()
        ? Posture::SupportLane
        : Posture::Followup;
    const auto player = GameObjects::Player();
    const float manaPercent = Engine::ManaPercent(player);
    if (manaPercent < Slider(BandageMenu, "HarassMana", 55)) {
        return false;
    }
    if (Ready(2) && TantrumHits(
            player.Position(), PredictPosition(selected, 0.25f),
            selected.BoundingRadius()) &&
        (HasCurse(selected) ||
         EDamage(selected) >= selected.Health()) &&
        CastTantrum(selected, Mode::Harass, false,
                    EDamage(selected) >= selected.Health())) {
        return true;
    }
    if (RuntimeQCharges() >= 2 &&
        Bool(BandageMenu, "HarassQ", true) &&
        CountAlliedFollowup(selected.Position(), 900.0f) >=
            Slider(RoleMenu, "HarassFollowup", 1) &&
        CastBandage(selected, BandagePurpose::Engage,
                    Mode::Harass, false)) {
        return true;
    }
    return false;
}

inline int JungleUnitsInTantrum() {
    const auto player = GameObjects::Player();
    int count = 0;
    for (const auto& monster : GameObjects::Jungle()) {
        if (monster.IsValid() && !monster.IsDead() &&
            monster.IsTargetable() && TantrumHits(
                player.Position(), monster.Position(),
                monster.BoundingRadius())) {
            ++count;
        }
    }
    return count;
}

inline bool TryJungle() {
    if (!Bool(FarmMenu, "JungleAbilities", true)) return false;
    const AIMinionClient monster =
        ControllerHelpers::SelectJungleTarget(1250.0f, 0.18f);
    if (!monster.IsValid()) return false;
    CurrentPosture = Posture::Jungle;
    const AIBaseClient unit(monster.Address());
    const auto player = GameObjects::Player();
    const float distance = player.Position().Distance2D(monster.Position());

    if (Ready(2) && Bool(FarmMenu, "JungleE", true) &&
        JungleUnitsInTantrum() >=
            Slider(FarmMenu, "JungleEMinimum", 1) &&
        CastTantrum(unit, Mode::LaneClear, false, true)) {
        ActiveSequence = Sequence::JungleCycle;
        return true;
    }
    if (!Ready(2) && Bool(TantrumMenu, "WaitAttackRefund", true) &&
        TantrumWillRefreshSoon(
            Slider(TantrumMenu, "RefundWaitMs", 480)) &&
        distance <= kTantrumRadius + monster.BoundingRadius()) {
        return false;
    }
    if (Bool(FarmMenu, "JungleQ", true) && Ready(0) &&
        distance > ControllerHelpers::AutoAttackRange(unit) + 80.0f &&
        RuntimeQCharges() > (Engine::CountEnemiesAt(
            player.Position(), 1500.0f) > 0 ? 1 : 0) &&
        CastBandage(unit, BandagePurpose::Jungle,
                    Mode::LaneClear, true)) {
        return true;
    }
    return false;
}

struct LaneTantrumPlan {
    int Hits = 0;
    int LastHits = 0;
    AIBaseClient Representative = {};
};

inline LaneTantrumPlan BuildLaneTantrumPlan() {
    LaneTantrumPlan plan{};
    const auto player = GameObjects::Player();
    const float raw = TantrumRawDamage(SpellRank(2), player.AP());
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!minion.IsValid() || minion.IsDead() ||
            !minion.IsTargetable() || !TantrumHits(
                player.Position(), PredictPosition(minion, 0.25f),
                minion.BoundingRadius())) {
            continue;
        }
        ++plan.Hits;
        if (!plan.Representative.IsValid()) {
            plan.Representative = AIBaseClient(minion.Address());
        }
        if (player.CalculateMagicDamage(minion, raw) >= minion.Health()) {
            ++plan.LastHits;
        }
    }
    return plan;
}

inline bool TryLaneFarm(Mode mode) {
    if (!Bool(FarmMenu, "LaneE", true) || !Ready(2)) return false;
    const auto player = GameObjects::Player();
    if (Engine::ManaPercent(player) < Slider(FarmMenu, "LaneMana", 55)) {
        return false;
    }
    const LaneTantrumPlan plan = BuildLaneTantrumPlan();
    const bool lastHit = mode == Mode::LastHit;
    const int required = lastHit
        ? Slider(FarmMenu, "MinimumLastHits", 1)
        : Slider(FarmMenu, "MinimumLaneHits", 3);
    const int value = lastHit ? plan.LastHits : plan.Hits;
    return value >= required && plan.Representative.IsValid() &&
           CastTantrum(plan.Representative, mode, false, true);
}

inline Posture ChoosePosture(Mode mode,
                             const AIHeroClient& selected,
                             const AIHeroClient& ally,
                             const AIHeroClient& threat) {
    if (mode == Mode::Flee) return Posture::Disengage;
    if (mode == Mode::LaneClear && JungleRole()) return Posture::Jungle;
    if (Engine::ValidAlly(ally) && Engine::ValidEnemy(threat) &&
        threat.Position().Distance2D(ally.Position()) <= 625.0f) {
        return Posture::Peel;
    }
    if (mode == Mode::Harass && SupportRole()) return Posture::SupportLane;
    if (mode == Mode::Combo && Engine::ValidEnemy(selected)) {
        const float distance = GameObjects::Player().Position().Distance2D(
            selected.Position());
        if (Engine::CountEnemiesAt(selected.Position(), 700.0f) >= 2) {
            return Posture::TeamfightEngage;
        }
        return distance > 650.0f ? Posture::Gank : Posture::Followup;
    }
    return Posture::Neutral;
}

inline void ResolvePendingUltimate() {
    if (!RPendingResolve || Now() < RPendingMarkTick) return;
    const auto player = GameObjects::Player();
    LastUltimatePlan = BuildUltimatePlan(
        player.Position(), 0.0f, RCastTargetId);
    for (const int id : LastUltimatePlan.HitIds) {
        if (id == 0) continue;
        const AIHeroClient enemy = HeroByNetworkId(id);
        if (Engine::ValidEnemy(enemy) &&
            !HasSpellShieldOrImmunity(enemy)) {
            MarkCurse(id);
        }
    }
    RPendingResolve = false;
}

inline void RefreshCurseFromDespair() {
    if (!WActive) return;
    const auto player = GameObjects::Player();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && DespairHits(
                player.Position(), enemy.Position(),
                enemy.BoundingRadius())) {
            MarkCurse(static_cast<int>(enemy.NetworkId()));
            WLastContactTick = Now();
        }
    }
}

inline void RefreshState() {
    const int now = Now();
    (void)RuntimeQCharges();
    const auto player = GameObjects::Player();

    const bool buffW = player.HasBuff("AuraofDespair") ||
                       player.HasBuff("AuraOfDespair") ||
                       player.HasBuff("AmumuW");
    if (buffW) WActive = true;
    if (!buffW && WActive && now - WToggleTick > 1300) {
        WActive = false;
    }

    if (QPendingArrival) {
        if (player.IsDashing()) QDashObserved = true;
        const bool arrivedByClock = now >= QExpectedArrivalTick;
        const bool dashFinished = QDashObserved && !player.IsDashing() &&
                                  now >= QExpectedImpactTick;
        if (arrivedByClock || dashFinished || now > QArrivalGraceUntil) {
            QPendingArrival = false;
            QArrivalGraceUntil = std::max(
                QArrivalGraceUntil, now + kQSequenceGraceMs);
            WLastContactTick = now;
        }
    }
    if (QForcedAutoSuppressUntil < now) QForcedAutoSuppressUntil = 0;
    if (GapcloserExpireTick < now) GapcloserTargetId = 0;
    if (InterruptExpireTick < now) InterruptTargetId = 0;
    if (TargetedAllyThreatUntil < now) TargetedAllyThreatId = 0;
    if (IncomingThreatUntil < now) RecentIncomingPressure *= 0.85f;
    for (auto& mark : CurseMarks) {
        if (mark.ExpireTick < now) mark = {};
    }
    for (auto& attack : IncomingAttacks) {
        if (attack.ExpireTick < now) attack = {};
    }
    ResolvePendingUltimate();
    RefreshCurseFromDespair();

    if (!QPendingArrival && now > QArrivalGraceUntil + 1200 &&
        now > RStunUntil + 600 &&
        ActiveSequence != Sequence::JungleCycle) {
        ActiveSequence = Sequence::None;
        LastBandagePurpose = BandagePurpose::None;
        QFollowChampionId = 0;
    }
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshState();
    const AIHeroClient ally = ProtectedAlly();
    const AIHeroClient threat = Engine::ValidAlly(ally)
        ? SelectPeelThreat(ally)
        : AIHeroClient{};
    CurrentPosture = ChoosePosture(mode, selected, ally, threat);

    if (HandleBandageSequence(mode, selected)) return true;
    if (QPendingArrival) return false;
    if (TryInterrupt()) return true;
    if (Bool(RoleMenu, "PeelBeforeEngage", true) &&
        Engine::ValidAlly(ally) && Engine::ValidEnemy(threat) &&
        TryPeel(ally, threat, mode)) {
        return true;
    }
    if (TryReactiveUltimate(selected, mode)) return true;
    if (TryManageDespair(
            mode == Mode::None ? Mode::Automatic : mode, selected)) {
        return true;
    }

    if (mode == Mode::Flee) return TryFlee(selected);
    if (mode == Mode::Combo) return TryCombo(selected);
    if (mode == Mode::Harass) return TryHarass(selected);
    if (mode == Mode::LaneClear) {
        if (JungleRole() && TryJungle()) return true;
        return TryLaneFarm(mode);
    }
    if (mode == Mode::LastHit) return TryLaneFarm(mode);
    return false;
}

inline void ObserveManualBandage(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto player = GameObjects::Player();
    Vector3 aim = args.EndPosition.IsValid() && !args.EndPosition.IsZero()
        ? args.EndPosition
        : args.CastPosition;
    const Vector3 origin = args.StartPosition.IsValid() &&
            !args.StartPosition.IsZero()
        ? args.StartPosition
        : player.Position();
    const Vector3 direction = SharedGeometry::Direction2D(origin, aim);
    if (direction.IsZero()) return;
    const auto units = BandageCollisionUnits(0.65f);
    const int first = FirstBandageCollisionIndex(origin, direction, units);
    if (first < 0) {
        LastBandagePlan = {};
        QCastTick = Now();
        QExpectedImpactTick = QCastTick + 900;
        QExpectedArrivalTick = QExpectedImpactTick;
        QArrivalGraceUntil = QExpectedArrivalTick + 300;
        QPendingArrival = false;
        return;
    }
    const LineUnit& collision = units[static_cast<std::size_t>(first)];
    const AIBaseClient unit = UnitByNetworkId(collision.Id);
    const float entry = BandageEntryDistance(origin, direction, collision);
    const float impact = BandageMissileSeconds(entry);
    const float arrival = BandageArrivalSeconds(
        origin.Distance2D(collision.Position), entry,
        player.BoundingRadius(), unit.IsValid()
            ? unit.BoundingRadius()
            : collision.Radius);
    LastBandagePlan = BandagePlan{
        origin + direction * kBandageRange,
        collision.Position,
        collision.Position,
        collision.Id,
        collision.Id,
        entry,
        impact,
        arrival,
        SDK::HitChance::High,
        true,
        ArrivalSafe(collision.Position, BandagePurpose::Engage),
        false,
        true,
    };
    QCastTick = Now();
    QExpectedImpactTick = QCastTick + static_cast<int>(
        std::ceil(impact * 1000.0f));
    QExpectedArrivalTick = QCastTick + static_cast<int>(
        std::ceil(arrival * 1000.0f));
    QArrivalGraceUntil = QExpectedArrivalTick + kQSequenceGraceMs;
    QTargetId = collision.Id;
    QStunUntil = QExpectedImpactTick + kBandageStunMs;
    QPendingArrival = true;
    QDashObserved = false;
    QCastOrigin = origin;
    LastBandagePurpose = BandagePurpose::Engage;
    if (QObservedAmmo > 0) --QObservedAmmo;

    const AIHeroClient hero = HeroByNetworkId(collision.Id);
    if (Engine::ValidEnemy(hero) &&
        ImmediateArrivalUltimateWanted(LastBandagePlan, hero)) {
        ActiveSequence = Sequence::ArrivalUltimate;
        QForcedAutoSuppressUntil = QArrivalGraceUntil;
    } else if (Engine::ValidEnemy(hero)) {
        ActiveSequence = Sequence::ArrivalCurseWeave;
    } else {
        ActiveSequence = Sequence::FirstBandage;
    }
    WExpectedContactUntil = QArrivalGraceUntil + 350;
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const bool ours = args.Slot >= 0 && args.Slot < 4 &&
                      Engine::WasControllerCast(args.Slot);
    if (args.Slot == 0 || SpellEventNameContains(args, "BandageToss")) {
        if (!ours) ObserveManualBandage(args);
        return;
    }
    if (args.Slot == 1 || SpellEventNameContains(args, "AuraofDespair")) {
        if (!ours) WActive = !WActive;
        WToggleTick = Now();
        return;
    }
    if (args.Slot == 2 || SpellEventNameContains(args, "Tantrum")) {
        if (!ours) {
            ECastTick = Now();
            ETargetId = 0;
        }
        return;
    }
    if (args.Slot == 3 ||
        SpellEventNameContains(args, "CurseoftheSadMummy")) {
        if (!ours) {
            RCastTick = Now();
            RResolveTick = RCastTick + 250;
            RStunUntil = RResolveTick + kUltimateStunMs;
            RPendingMarkTick = RResolveTick;
            RPendingResolve = true;
            RWasManual = true;
            LastUltimateReason = UltimateReason::None;
            ManualRFlashWindowUntil = RResolveTick + 80;
            LastUltimatePlan = BuildUltimatePlan(
                GameObjects::Player().Position(), 0.25f, 0);
        }
        return;
    }
    if (args.Slot == static_cast<int>(SDK::SpellSlot::Summoner1) ||
        args.Slot == static_cast<int>(SDK::SpellSlot::Summoner2) ||
        SpellEventNameContains(args, "SummonerFlash")) {
        if (SpellEventNameContains(args, "Flash")) {
            ManualFlashUntil = Now() + 480;
        }
    }
}

inline void ObserveEnemyCast(
    const SDK::Events::ProcessSpellEventArgs& args) {
    RecordIncomingAttack(args);
    if (!args.Sender.IsValid() || IsLocalPlayer(args.Sender)) return;
    const auto player = GameObjects::Player();
    const std::uint32_t targetId = args.TargetNetworkId != 0
        ? args.TargetNetworkId
        : args.Target.NetworkId;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally) ||
            targetId != static_cast<std::uint32_t>(ally.NetworkId()) ||
            ally.NetworkId() == player.NetworkId()) {
            continue;
        }
        TargetedAllyThreatId = static_cast<int>(args.Sender.NetworkId);
        TargetedAllyThreatUntil = Now() + std::clamp(
            ControllerHelpers::NormalizedCastDelayMs(args.CastDelay, 250) +
                650,
            500, 1800);
        ProtectedAllyId = static_cast<int>(ally.NetworkId());
        break;
    }

    const auto analysis = AnalyzeEnemyCast(
        args, 220.0f, 105.0f, 300, 250, 220, 1600, 480);
    if (!analysis.Valid) return;
    if (analysis.TargetsPlayer || analysis.CrossesPlayer) {
        IncomingThreatUntil = std::max(
            IncomingThreatUntil,
            std::max(analysis.CommitmentUntilTick,
                     analysis.LineThreatUntilTick));
        RecentIncomingPressure = std::min(
            player.MaxHealth(),
            RecentIncomingPressure +
                (args.IsAutoAttack
                    ? SDK::Damage::GetAutoAttackDamage(
                          analysis.Enemy, player, true)
                    : player.MaxHealth() * 0.13f));
    }
    if (analysis.CrossesPlayer && analysis.LikelyHardCrowdControl) {
        IncomingHardCcUntil = std::max(
            IncomingHardCcUntil, analysis.LineThreatUntilTick);
    }
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (CaptureLocalAutoAttack(
            args, LastAutoTargetId, LastAutoTick)) {
        MarkCurse(LastAutoTargetId);
        if (ActiveSequence == Sequence::ArrivalCurseWeave) {
            WLastContactTick = Now();
        }
        return;
    }
    RecordIncomingAttack(args);
}

inline void UpdateBuffState(const SDK::Events::BuffEventArgs& args,
                            bool added) {
    if (!args.Sender.IsValid() || !args.BuffName[0]) return;
    if (IsLocalPlayer(args.Sender) &&
        (Engine::TextContains(args.BuffName, "AuraofDespair") ||
         Engine::TextContains(args.BuffName, "AmumuW"))) {
        WActive = added;
        WToggleTick = Now();
        return;
    }
    const AIHeroClient enemy = HeroByNetworkId(
        static_cast<int>(args.Sender.NetworkId));
    if (Engine::ValidEnemy(enemy) &&
        (Engine::TextContains(args.BuffName, "CurseoftheSadMummy") ||
         Engine::TextContains(args.BuffName, "AmumuPassive"))) {
        if (added) {
            const int duration = args.EndTime > Game::Time()
                ? static_cast<int>((args.EndTime - Game::Time()) * 1000.0f)
                : kCurseDurationMs;
            MarkCurse(static_cast<int>(enemy.NetworkId()),
                      std::clamp(duration, 250, 4000));
        } else {
            for (auto& mark : CurseMarks) {
                if (mark.NetworkId == static_cast<int>(enemy.NetworkId())) {
                    mark = {};
                }
            }
        }
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid() || QForcedAutoSuppressUntil < Now() ||
        ActiveSequence != Sequence::ArrivalUltimate || !UltimateReady() ||
        !Bool(BandageMenu, "SuppressQAutoForR", true)) {
        return;
    }
    const int targetId = static_cast<int>(args.Target.NetworkId());
    if (targetId == QTargetId &&
        (QDashObserved || Now() >= QExpectedImpactTick)) {
        args.Process = false;
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick)) {
        MarkCurse(LastAutoTargetId);
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::Gank: return "gank";
    case Posture::Followup: return "follow-up";
    case Posture::TeamfightEngage: return "teamfight";
    case Posture::Peel: return "peel";
    case Posture::Disengage: return "disengage";
    case Posture::Jungle: return "jungle";
    case Posture::SupportLane: return "support-lane";
    default: return "neutral";
    }
}

inline const char* SequenceName(Sequence sequence) {
    switch (sequence) {
    case Sequence::FirstBandage: return "Q access";
    case Sequence::ArrivalCurseWeave: return "Q-AA Curse";
    case Sequence::ArrivalUltimate: return "Q->R NOW";
    case Sequence::LayerSecondBandage: return "layer Q2";
    case Sequence::MinionBridge: return "Q bridge";
    case Sequence::FollowFlashOrDash: return "follow dash";
    case Sequence::PeelChain: return "peel chain";
    case Sequence::EscapeAnchor: return "escape Q";
    case Sequence::JungleCycle: return "jungle cycle";
    default: return "hold";
    }
}

inline const char* UltimateReasonName(UltimateReason reason) {
    switch (reason) {
    case UltimateReason::QArrival: return "Q-arrival";
    case UltimateReason::MultiTarget: return "multi";
    case UltimateReason::HighValuePick: return "carry-pick";
    case UltimateReason::Peel: return "peel";
    case UltimateReason::AntiDash: return "anti-dash";
    case UltimateReason::Interrupt: return "interrupt";
    case UltimateReason::Lethal: return "lethal";
    case UltimateReason::Survival: return "survival";
    case UltimateReason::ManualFlash: return "manual-Flash";
    default: return "hold";
    }
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;

    if (Bool(CoachMenu, "DrawQ", true)) {
        Drawing::DrawCircle(player.Position(), kBandageRange,
                            0x665BD6FFu, 1.2f, 80);
        if (LastBandagePlan.Valid &&
            Now() <= QArrivalGraceUntil + 650) {
            Drawing::DrawLine(
                QCastOrigin.IsValid() && !QCastOrigin.IsZero()
                    ? QCastOrigin
                    : player.Position(),
                LastBandagePlan.Aim,
                LastBandagePlan.IntendedFirst
                    ? (LastBandagePlan.ArrivalSafe
                        ? 0xFF58E69Au
                        : 0xFFFF5B69u)
                    : 0xFFFFC857u,
                2.2f);
            Drawing::DrawCircle(
                LastBandagePlan.CollisionPosition,
                48.0f,
                LastBandagePlan.ArrivalSafe
                    ? 0xFF58E69Au
                    : 0xFFFF5B69u,
                2.0f, 40);
        }
    }
    if (Bool(CoachMenu, "DrawAura", false)) {
        Drawing::DrawCircle(player.Position(), kDespairRadius,
                            WActive ? 0xAA62C9FFu : 0x5562C9FFu,
                            1.4f, 56);
        Drawing::DrawCircle(player.Position(), kTantrumRadius,
                            0x557E67FFu, 1.0f, 56);
    }
    if (Bool(CoachMenu, "DrawR", true)) {
        Drawing::DrawCircle(player.Position(), kCurseRadius,
                            UltimateReady()
                                ? 0xAA9B7CFFu
                                : 0x449B7CFFu,
                            1.8f, 72);
        if (LastUltimatePlan.Valid && Now() <= RStunUntil + 500) {
            for (const int id : LastUltimatePlan.HitIds) {
                if (id == 0) continue;
                const AIHeroClient enemy = HeroByNetworkId(id);
                if (enemy.IsValid()) {
                    Drawing::DrawCircle(
                        enemy.Position(), enemy.BoundingRadius() + 38.0f,
                        id == LastUltimatePlan.PrimaryId
                            ? 0xFFFFD166u
                            : 0xAA9B7CFFu,
                        id == LastUltimatePlan.PrimaryId ? 2.8f : 1.5f,
                        42);
                }
            }
        }
    }
    if (Bool(CoachMenu, "DrawPeel", true)) {
        const AIHeroClient ally = ProtectedAlly();
        const AIHeroClient threat = HeroByNetworkId(PeelThreatId);
        if (ally.IsValid()) {
            Drawing::DrawCircle(ally.Position(), 105.0f,
                                0xAA56D8FFu, 2.0f, 44);
        }
        if (ally.IsValid() && threat.IsValid()) {
            Drawing::DrawLine(ally.Position(), threat.Position(),
                              0xFFFF6375u, 2.4f);
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            char state[320]{};
            _snprintf_s(
                state, sizeof(state), _TRUNCATE,
                "Amumu one-trick | %s | %s | Q %d/%d%s | W %s | E %.1fs(%d hits) | R %s %.2f/%d",
                PostureName(CurrentPosture), SequenceName(ActiveSequence),
                RuntimeQCharges(), std::max(2, QObservedMaxAmmo),
                QForcedAutoSuppressUntil >= Now() ? " AUTO-OFF" : "",
                WActive ? "ON" : "off",
                TantrumCooldownRemaining(), IncomingAttackCount(600),
                UltimateReasonName(LastUltimateReason),
                LastUltimatePlan.Score,
                LastUltimatePlan.EffectiveHitCount);
            Drawing::DrawText(screen.x - 195.0f, screen.y - 118.0f,
                              0xFFD9F4FFu, state);
        }
    }
    if (ManualRFlashWindowUntil >= Now() &&
        Bool(CoachMenu, "DrawRFlashWindow", true)) {
        Drawing::DrawCircle(Game::CursorPos(), 85.0f,
                            0xFFFFD166u, 3.0f, 48);
        Drawing::DrawLine(player.Position(), Game::CursorPos(),
                          0xFFFFD166u, 2.0f);
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "AmumuOneTrick", "Amumu one-trick mechanics"));

    RoleMenu = TacticsMenu->AddSubMenu(new Menu(
        "Role", "Role, ally coordination and intent"));
    RoleMenu->Add(new MenuList(
        "Role", "Role model", { "Auto", "Jungle", "Support" }, 0));
    RoleMenu->Add(new MenuBool(
        "RespectCursor", "Require cursor agreement", true));
    RoleMenu->Add(new MenuBool(
        "PeelBeforeEngage", "Protect carry first", true));
    RoleMenu->Add(new MenuSlider(
        "HarassFollowup", "Min allies follow Q", 1, 0, 3));
    RoleMenu->Add(new MenuSeparator(
        "Ownership",
        "Movement, attack-move, Hold,"));

    BandageMenu = TacticsMenu->AddSubMenu(new Menu(
        "Bandage", "Two-charge first-collision and arrival planner"));
    BandageMenu->Add(new MenuBool(
        "DirectEngage", "Use a safe direct champion Q", true));
    BandageMenu->Add(new MenuBool(
        "ReserveSecond", "Reserve Q for peel", true));
    BandageMenu->Add(new MenuList(
        "Hitchance", "Naked champion Q hitchance",
        { "Medium", "High", "Very high", "Immobile only" }, 2));
    BandageMenu->Add(new MenuSlider(
        "MaximumNakedRange", "Max naked-Q center range",
        1000, 700, 1160));
    BandageMenu->Add(new MenuSlider(
        "MaxArrivalEnemies", "Max enemies at Q", 3, 1, 5));
    BandageMenu->Add(new MenuSlider(
        "MinimumSafety", "Minimum Q-arrival safety score", -180, -1200, 600));
    BandageMenu->Add(new MenuBool(
        "RespectDashHazards", "Reject ready", true));
    BandageMenu->Add(new MenuBool(
        "ShieldMobility", "Allow shielded Q only as", false));
    BandageMenu->Add(new MenuBool(
        "Bridge", "Use minion/monster Q bridge", true));
    BandageMenu->Add(new MenuSlider(
        "BridgeMinimumGain", "Min Q bridge reach", 300, 150, 650));
    BandageMenu->Add(new MenuBool(
        "LayerCrowdControl", "Delay Q2 near CC end", true));
    BandageMenu->Add(new MenuSlider(
        "OverlapAllowance", "Allowed Q2 CC overlap (ms)", 100, 0, 300));
    BandageMenu->Add(new MenuBool(
        "SuppressQAutoForR", "Cancel Q-AA for R", true));
    BandageMenu->Add(new MenuBool(
        "PeelQ", "Q diver vs carry", true));
    BandageMenu->Add(new MenuBool(
        "InterruptQ", "Q interrupt channel", true));
    BandageMenu->Add(new MenuBool(
        "EscapeAnchor", "Q safe unit on flee", true));
    BandageMenu->Add(new MenuBool(
        "HarassQ", "Use Q harass with two", true));
    BandageMenu->Add(new MenuSlider(
        "HarassMana", "Minimum mana for Q harass (%)", 55, 0, 100));

    DespairMenu = TacticsMenu->AddSubMenu(new Menu(
        "Despair", "Contact-aware W toggle and mana economy"));
    DespairMenu->Add(new MenuBool(
        "AutoToggle", "Auto W on contact", true));
    DespairMenu->Add(new MenuBool(
        "PreToggleQ", "Enable W shortly before a", true));
    DespairMenu->Add(new MenuSlider(
        "PreToggleLead", "Max W pre-arrival (ms)", 650, 150, 950));
    DespairMenu->Add(new MenuSlider(
        "FlatReserve", "Min mana before W drain", 70, 0, 300));
    DespairMenu->Add(new MenuSlider(
        "OffDelay", "W off delay (ms)", 420, 100, 1200));
    DespairMenu->Add(new MenuBool(
        "KeepEpic", "Keep W on while an epic", true));

    TantrumMenu = TacticsMenu->AddSubMenu(new Menu(
        "Tantrum", "Curse weave and incoming-attack cooldown refunds"));
    TantrumMenu->Add(new MenuBool(
        "WeaveCurse", "Wait for an in-range AA", true));
    TantrumMenu->Add(new MenuBool(
        "WaitAttackRefund", "Wait when autos refresh E", true));
    TantrumMenu->Add(new MenuSlider(
        "RefundWaitMs", "Maximum E-refund wait (ms)", 480, 100, 900));
    TantrumMenu->Add(new MenuSeparator(
        "RefundRule", "Each basic attack that hits"));

    UltimateMenu = TacticsMenu->AddSubMenu(new Menu(
        "Ultimate", "Predicted R quality, layering and player Flash"));
    UltimateMenu->Add(new MenuBool(
        "MultiTarget", "R multi-target cluster", true));
    UltimateMenu->Add(new MenuSlider(
        "MinimumTargets", "Minimum effective R targets", 2, 1, 5));
    UltimateMenu->Add(new MenuSlider(
        "MinimumScore", "Minimum R quality score x100", 210, 60, 800));
    UltimateMenu->Add(new MenuBool(
        "AvoidCCOverlap", "R penalty long CC", true));
    UltimateMenu->Add(new MenuBool(
        "RespectCleanse", "Discount QSS/cleanses", true));
    UltimateMenu->Add(new MenuBool(
        "BufferAfterQ", "Queue R during Q dash when", true));
    UltimateMenu->Add(new MenuSlider(
        "BufferLead", "Max Q-R buffer (ms)", 520, 120, 900));
    UltimateMenu->Add(new MenuBool(
        "SingleCarryArrival", "Immediate R one Flash-ready", true));
    UltimateMenu->Add(new MenuSlider(
        "SingleCarryHp", "Solo carry R HP ceil (%)", 42, 10, 90));
    UltimateMenu->Add(new MenuBool(
        "HighValuePick", "Use supported R for a", true));
    UltimateMenu->Add(new MenuSlider(
        "PickHp", "Carry pick HP ceil (%)", 58, 15, 95));
    UltimateMenu->Add(new MenuBool(
        "LethalSingle", "R one target when R is", true));
    UltimateMenu->Add(new MenuBool(
        "PeelR", "R to peel ally threat", true));
    UltimateMenu->Add(new MenuSlider(
        "PeelAllyHp", "Ally HP crit peel (%)", 42, 10, 85));
    UltimateMenu->Add(new MenuBool(
        "AntiDashR", "R knock down dash", true));
    UltimateMenu->Add(new MenuBool(
        "InterruptR", "R when Q too slow", true));
    UltimateMenu->Add(new MenuBool(
        "SurvivalR", "R nearby enemies under", true));
    UltimateMenu->Add(new MenuSlider(
        "SurvivalHp", "Player HP for survival R (%)", 27, 5, 65));
    UltimateMenu->Add(new MenuBool(
        "AfterManualFlash", "Cast smart R after the", true));
    UltimateMenu->Add(new MenuBool(
        "FleeR", "Use R fleeing from multiple", true));
    UltimateMenu->Add(new MenuSlider(
        "FleeTargets", "Minimum flee-R targets", 2, 1, 5));
    UltimateMenu->Add(new MenuSeparator(
        "RFlash",
        "Manual R->Flash is coached"));

    FarmMenu = TacticsMenu->AddSubMenu(new Menu(
        "Farm", "Jungle E-refund and conservative lane clear"));
    FarmMenu->Add(new MenuBool(
        "JungleAbilities", "Abilities in jungle", true));
    FarmMenu->Add(new MenuBool(
        "JungleQ", "Q a distant jungle target", true));
    FarmMenu->Add(new MenuBool(
        "JungleE", "Use E on jungle contact", true));
    FarmMenu->Add(new MenuSlider(
        "JungleEMinimum", "Minimum jungle units for E", 1, 1, 6));
    FarmMenu->Add(new MenuBool(
        "LaneE", "E for lane clear", true));
    FarmMenu->Add(new MenuBool(
        "LaneW", "Allow W during lane clear", false));
    FarmMenu->Add(new MenuSlider(
        "LaneMana", "Minimum lane-clear mana (%)", 55, 0, 100));
    FarmMenu->Add(new MenuSlider(
        "MinimumLaneHits", "Minimum lane minions hit by E", 3, 1, 8));
    FarmMenu->Add(new MenuSlider(
        "MinimumLastHits", "Minimum E last hits", 1, 1, 6));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "One-trick visual coaching"));
    CoachMenu->Add(new MenuBool(
        "DrawQ", "Draw Q range, first", false));
    CoachMenu->Add(new MenuBool(
        "DrawAura", "Draw W and E effect radii", false));
    CoachMenu->Add(new MenuBool(
        "DrawR", "Draw R radius/target", false));
    CoachMenu->Add(new MenuBool(
        "DrawPeel", "Draw ally/diver", false));
    CoachMenu->Add(new MenuBool(
        "DrawState", "Draw posture/ammo state", false));
    CoachMenu->Add(new MenuBool(
        "DrawRFlashWindow", "Draw R->Flash window", false));
}

inline void OnLoad() {
    ActiveSequence = Sequence::None;
    CurrentPosture = Posture::Neutral;
    LastBandagePurpose = BandagePurpose::None;
    LastUltimateReason = UltimateReason::None;
    QObservedAmmo = QObservedMaxAmmo = -1;
    QCastTick = QExpectedImpactTick = QExpectedArrivalTick = 0;
    QTargetId = QFollowChampionId = QStunUntil = 0;
    QForcedAutoSuppressUntil = QArrivalGraceUntil = 0;
    QPendingArrival = QDashObserved = false;
    QCastOrigin = {};
    LastBandagePlan = {};
    BandagePlanCaches.fill({});
    BandagePlanCacheCursor = 0;
    WActive = GameObjects::Player().HasBuff("AuraofDespair");
    WToggleTick = WLastContactTick = WExpectedContactUntil = 0;
    ECastTick = ETargetId = 0;
    RCastTick = RResolveTick = RStunUntil = RPendingMarkTick = 0;
    RCastTargetId = 0;
    RPendingResolve = RWasManual = false;
    LastUltimatePlan = {};
    CurseMarks.fill({});
    IncomingAttacks.fill({});
    ProtectedAllyId = PeelThreatId = 0;
    TargetedAllyThreatId = TargetedAllyThreatUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEnd = {};
    InterruptTargetId = InterruptExpireTick = 0;
    LastAutoTargetId = LastAutoTick = 0;
    ManualFlashUntil = ManualRFlashWindowUntil = 0;
    IncomingThreatUntil = IncomingHardCcUntil = 0;
    RecentIncomingPressure = 0.0f;
    RefreshState();
}

inline void OnUnload() {
    TacticsMenu = RoleMenu = BandageMenu = DespairMenu = nullptr;
    TantrumMenu = UltimateMenu = FarmMenu = CoachMenu = nullptr;
    BandagePlanCaches.fill({});
    BandagePlanCacheCursor = 0;
}

inline constexpr const char* Scenarios[] = {
    "Route each decision through gank, follow-up, teamfight, peel, disengage, jungle or support-lane posture",
    "Preserve player ownership of movement, attack-move, Hold, Stop, Flash and Smite",
    "Prefer the player's selected target while requiring cursor agreement for proactive access",
    "Protect a pressured carry before starting a new engage",
    "Infer jungle versus support role from an explicit setting or live Smite",
    "Model Bandage Toss as two stored charges with a three-second cast lockout",
    "Read live Q Ammo and MaxAmmo instead of inventing a two-charge counter",
    "Maintain a cast-event Q ammo fallback when the runtime field is temporarily unavailable",
    "Reserve charge two for Flash, dash, channel interruption or peel",
    "Spend the reserved charge when a conservative full sequence is lethal",
    "Spend the reserved charge during flee when a safe anchor is the only disengage",
    "Use the current 1100 Q range and 80 half-width",
    "Use current 0.25 cast time and 2000 missile speed in Q prediction",
    "Model the follow dash at current 1800 speed",
    "Measure Q range to collision-circle entry rather than target center",
    "Include every target gameplay radius in Q collision",
    "Order Q collisions by capsule entry distance instead of center distance",
    "Allow a large offset jungle monster to intercept before a closer-center minion",
    "Enumerate enemy champions, lane minions and jungle monsters as Q blockers",
    "Predict each moving blocker at Q impact time",
    "Try narrow angular offsets while still requiring the intended unit to collide first",
    "Reject a champion Q when a lane minion is the real first collision",
    "Reject a Q whose predicted first collision disappears or becomes untargetable",
    "Reject Fiora parry, pool, rappel, playful and stasis states",
    "Treat spell shield as failed Q CC and damage",
    "Permit shielded Q only as explicitly enabled, supported R mobility",
    "Remember that a spell-shielded Q still pulls Amumu to the target",
    "Reject shield mobility without R and allied follow-up",
    "Require very-high hitchance for an ordinary naked Q by default",
    "Lower Q hitchance requirements for dashes, channels and peel emergencies",
    "Prefer walking closer over throwing an ordinary maximum-range naked Q",
    "Allow a long Q when multi-target arrival R or conservative lethal damage justifies it",
    "Reject a proactive Q opposite the player's cursor",
    "Evaluate Q arrival at the followed unit rather than the original cast endpoint",
    "Reject a Q arrival embedded in NavMesh terrain",
    "Reject an unapproved Q arrival under an enemy turret",
    "Reject a Q arrival with more enemies than configured",
    "Reward allied follow-up in the Q arrival safety score",
    "Reject ready Poppy, Taliyah and Cassiopeia dash hazards at the arrival",
    "Keep peel and interrupt arrivals on their own emergency safety branch",
    "Track Q missile impact and Amumu arrival as separate clocks",
    "Track the one-second Q stun from missile impact rather than arrival",
    "Observe the live player dash before declaring Q arrival when possible",
    "Fallback to the radius-aware arrival clock when dash state is missing",
    "Continue a player-cast Q by reconstructing its actual first collision",
    "Continue a manual Q without treating it as controller-owned input",
    "Pre-toggle W only after a Q collision and arrival have been confirmed",
    "Queue R during the Q dash when the predicted arrival demands immediate lockdown",
    "Suppress Q's forced automatic AA only inside the immediate-arrival R branch",
    "Never suppress the Q AA in the ordinary Curse-weave branch",
    "Choose Q to AA to E when applying Curse is worth more than instant R",
    "Choose Q to R when an AA would give a Flash-ready target an escape window",
    "Choose Q to R when multiple valuable enemies will be caught on arrival",
    "Choose Q to R on one high-value carry only with follow-up and urgency",
    "Recompute R coverage at the expected Q arrival instead of current Amumu position",
    "Fall back from a stale Q-arrival R plan if targets leave the future radius",
    "Layer charge two near the end of Q stun instead of overlapping it immediately",
    "Layer charge two near the end of R stun instead of wasting lockdown duration",
    "Fire charge two early when the target begins a new dash",
    "Fire charge two early when the second cast is conservatively lethal",
    "Hold charge two when ordinary CC still has substantial duration",
    "Use Q on a minion or monster to bridge into second-Q champion range",
    "Require two Q charges before starting a normal bridge",
    "Require meaningful reach gain before spending Q on a bridge unit",
    "Require the bridge unit itself to be the real first Q collision",
    "Require the bridge arrival to pass turret, density and dash-hazard checks",
    "Require bridge direction to agree with the player's cursor",
    "Carry the original champion target across the bridge state",
    "Cast the second Q only after landing and recomputing blockers",
    "Use a cursor-aligned minion, monster or champion as a flee anchor",
    "Require a flee Q endpoint to increase separation from the pursuer",
    "Reject a flee anchor that ends under turret or inside dash denial",
    "Q a channel only when impact arrives before the interrupt window closes",
    "Use R to interrupt when Q cannot arrive in time",
    "Q a diver threatening the protected ally before treating it as an engage target",
    "Use E on a diver already inside Amumu's contact radius",
    "Use R for critical carry peel when Q is too slow or blocked",
    "Use the current 350 W effect radius rather than the misleading 300 cast range",
    "Track W toggle state from both local casts and AuraofDespair buff events",
    "Avoid double-inverting W when the controller's cast event is synchronous",
    "Enable W shortly before confirmed Q contact, not on speculative travel",
    "Keep W on while enemy champions remain in contact",
    "Keep W on around an epic monster when configured",
    "Turn W off after a short no-contact grace period",
    "Turn W off immediately when its drain would consume the Q-E-R reserve",
    "Reserve current eight mana per second while estimating W sustain",
    "Refresh tracked Curse continuously from real W contact",
    "Use current W half-second tick cadence",
    "Use current W base of ten per second and halve it per tick",
    "Use current W max-health scaling of 1 to 2 percent per second by rank",
    "Use current W scaling of 0.5 percent max health per 100 AP per second",
    "Track Curse for three seconds from local AA, R or W refresh",
    "Use live Curse buffs when available and event timing as fallback",
    "Apply Curse before subsequent magic damage only after a real AA or prior mark",
    "Remember that R applies Curse after its own magic damage",
    "Add R's ten-percent true damage only when the target was cursed before R",
    "Do not award R's own damage a Curse bonus from the mark it applies afterward",
    "Use current Q damage of 70 to 170 plus 85 percent AP",
    "Use current E damage of 65 to 185 plus 50 percent AP",
    "Use current R damage of 200 to 400 plus 80 percent AP",
    "Predict E targets through its 0.25-second cast time",
    "Include gameplay radius at the 350 E boundary",
    "Wait for an in-range AA to Curse before ordinary champion E",
    "Skip the Curse AA wait when E is lethal or immediate control matters",
    "Track incoming basic attacks directed at Amumu",
    "Estimate ranged-auto impact from cast delay, travel distance and missile speed",
    "Deduplicate ProcessSpell and DoCast reports for the same incoming attack",
    "Reduce predicted E cooldown by exactly 0.75 seconds per incoming basic attack",
    "Wait briefly when raptor, krug, minion or champion autos will refresh E",
    "Do not wait for an E refund beyond the configured tactical horizon",
    "Use E immediately once the runtime spell reports ready",
    "Prefer the highest-health or epic jungle monster",
    "Enable W on actual jungle contact while preserving mana",
    "Use E repeatedly in multi-unit camps through observed cooldown readiness",
    "Use Q on a distant jungle unit without consuming the last combat charge near enemies",
    "Avoid inventing movement between jungle camps",
    "Use lane E only above an explicit mana threshold",
    "Require multiple E hits during ordinary lane clear",
    "Require predicted lethal minions during LastHit mode",
    "Keep lane W opt-in because indefinite drain disrupts wave control",
    "Predict every R target at the end of the 0.25-second cast",
    "Include target gameplay radius in the 550 R boundary",
    "Score R by target value rather than raw body count alone",
    "Prefer ranged high-damage carries inside the same R cluster",
    "Penalize a target protected by a spell shield",
    "Discount ready Quicksilver Sash, Mercurial Scimitar and Silvermere Dawn",
    "Discount ready Olaf, Gangplank and Alistar self-cleanses",
    "Penalize R overlap on long existing hard CC",
    "Reward R for knocking down a dash whose endpoint enters the radius",
    "Require both target count and quality score for ordinary multi-target R",
    "Allow single-target R only for unique lethal, high-value pick, peel, interrupt or survival",
    "Avoid R when non-ultimate damage is already conservatively lethal",
    "Use survival R only with nearby targets and observed incoming pressure",
    "React to a player-owned Flash by re-evaluating R at the new position",
    "Coach manual R to Flash without ever casting Flash for the player",
    "Resolve manual R hits at Amumu's position after its cast time",
    "Apply R Curse marks after resolution so manual R-Flash uses the final center",
    "Observe and continue player-cast W, E and R",
    "Respect the shared manual-input arbitration window",
    "Preserve ordinary valuable attack windups",
    "Expose first Q collision, arrival safety and selected R hit set visually",
    "Expose Q ammo, auto-suppression, W state, E refund forecast and R quality visually",
    "Never fall back to generic Q-W-E-R ordering because this controller owns the full decision loop",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Amumu;
    controller.ControllerId = "champion.kuroaio.ai.amumu.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIAmumu.md";
    controller.ImplementationSummary =
        "Seven-posture two-charge vanguard with radius-aware first-collision "
        "Q and arrival safety, minion bridges, Flash/dash charge reserve, "
        "Q-AA versus buffered Q-R arbitration, contact/mana W toggling, "
        "incoming-AA E refunds, Curse order, and value/cleanse/CC-aware R.";
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
            &ObserveLocalSpell, &ObserveEnemyCast>;
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
            &GapcloserExpireTick, 620, 900>;
    controller.OnInterruptable =
        &ControllerHelpers::CaptureInterruptableEvent<
            &InterruptTargetId, &InterruptExpireTick, 950, 160, 2600>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Amumu
