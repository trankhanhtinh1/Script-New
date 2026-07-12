#pragma once

#include "../../../SDK/SDK.h"
#include "Damage.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>

namespace Plugins::ziblldev9898::LeeSin {

using SDK::Core::Utils::AutoAttack;

inline constexpr float Q1Range = 1200.0f;
inline constexpr float Q2Range = 1250.0f;
inline constexpr float W1Range = 700.0f;
inline constexpr float E1Range = 450.0f;
inline constexpr float E2Range = 575.0f;
inline constexpr float RRange = 375.0f;
inline constexpr float Q2SafetyRadius = 600.0f;
inline constexpr float Q2HealthThreshold = 0.30f;
inline constexpr float WardPlacementRange = 600.0f;
inline constexpr DWORD WardHopTimeoutMs = 1500;
inline constexpr DWORD WardHopRetryIntervalMs = 150;
inline constexpr DWORD WardHopRetryWindowMs = 450;
inline constexpr float InsecWardSafetyMargin = 25.0f;
inline constexpr float InsecKickSafetyMargin = 25.0f;
inline constexpr float InsecDirectRange = 300.0f;
inline constexpr float InsecWardBehindDistance = 120.0f;
inline constexpr DWORD InsecMoveIntervalMs = 75;
inline constexpr DWORD InsecActionIntervalMs = 120;
inline constexpr DWORD FancyComboQ2FlightWindowMs = 900;
inline constexpr DWORD FancyComboW1ToQ2DelayMs = 50;
inline constexpr DWORD FancyComboW1ConfirmTimeoutMs = 300;
inline constexpr float FancyComboWardDistance = 250.0f;
inline constexpr float FancyComboWardMinDistance = 65.0f;
inline constexpr float MultiRKickDistance = 1200.0f;
inline constexpr float MultiRKickCollisionRadius = 120.0f;
inline constexpr float MultiRKickStartOffset = 90.0f;
inline constexpr float MultiRWardDistance = 200.0f;
inline constexpr float MultiRWardSafetyMargin = 35.0f;
inline constexpr DWORD MultiRActionIntervalMs = 100;
inline constexpr DWORD MultiRQ2DashWindowMs = 750;
inline constexpr DWORD MultiRW1ToRDelayMs = 50;
inline constexpr DWORD MultiRW1ConfirmTimeoutMs = 300;
inline constexpr DWORD ComboIntervalMs = 50;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline MenuBool* DrawPassiveMenu = nullptr;
inline MenuBool* DrawComboMenu = nullptr;
inline MenuBool* WardHopEnabledMenu = nullptr;
inline MenuKeyBind* WardHopKeyMenu = nullptr;
inline MenuKeyBind* InsecKeyMenu = nullptr;
inline MenuBool* FancyComboMenu = nullptr;
inline MenuSlider* FancyQ2HealthMenu = nullptr;
inline MenuSlider* FancyQ2EnemiesMenu = nullptr;
inline MenuBool* MultiRComboMenu = nullptr;
inline MenuSlider* MultiRMinCollisionsMenu = nullptr;
inline Menu* KillStealMenu = nullptr;
inline MenuBool* RAutoKillStealMenu = nullptr;

inline Spell Q{ SpellSlot::Q, Q1Range };
inline Spell W{ SpellSlot::W, W1Range };
inline Spell E{ SpellSlot::E, E1Range };
inline Spell R{ SpellSlot::R, RRange };

inline bool Loaded = false;
inline bool DrawHooked = false;
inline bool UpdateHooked = false;
inline volatile LONG PassiveStacks = 0;
inline uint32_t QMarkedTargetNetworkId = 0;
inline bool Q1AwaitingAutoAttack = false;
inline bool Q2AwaitingAutoAttack = false;
inline DWORD QRecastExpireTick = 0;
inline bool WardHopPending = false;
inline bool WardHopKeyWasActive = false;
inline bool WardHopCompleted = false;
inline Vec3 WardHopPosition = {};
inline DWORD WardHopRequestTick = 0;
inline DWORD WardHopLastAttemptTick = 0;
inline DWORD WardHopRetryUntilTick = 0;
inline uint32_t WardHopKnownWardNetworkIds[64] = {};
inline int WardHopKnownWardCount = 0;
inline DWORD LastComboTick = 0;

enum class RecastPhase {
    Unknown,
    First,
    Second
};

enum class InsecStage {
    Idle,
    NeedQ1,
    NeedQ2,
    NeedWard,
    NeedW,
    NeedR,
    Completed
};

enum class FancyComboStage {
    Idle,
    NeedWard,
    NeedW,
    NeedQ2,
    Q2InFlight
};

enum class MultiRStage {
    Idle,
    NeedQ1,
    NeedQ2,
    NeedWard,
    NeedW,
    NeedR
};

struct MultiRKickCandidate {
    AIHeroClient Target = {};
    Vec3 Direction = {};
    Vec3 WardPosition = {};
    int CollisionCount = 0;
    float TargetDistance = FLT_MAX;
    bool Direct = false;
};

inline FancyComboStage CurrentFancyComboStage = FancyComboStage::Idle;
inline uint32_t FancyComboTargetNetworkId = 0;
inline Vec3 FancyComboWardPosition = {};
inline DWORD FancyComboStageTick = 0;
inline MultiRStage CurrentMultiRStage = MultiRStage::Idle;
inline uint32_t MultiRTargetNetworkId = 0;
inline Vec3 MultiRDirection = {};
inline Vec3 MultiRWardPosition = {};
inline int MultiRCollisionCount = 0;
inline bool MultiRNeedsWConfirm = false;
inline DWORD MultiRQ2CastTick = 0;
inline DWORD MultiRStageTick = 0;
inline DWORD MultiRLastActionTick = 0;

inline InsecStage CurrentInsecStage = InsecStage::Idle;
inline bool InsecKeyWasActive = false;
inline uint32_t InsecTargetNetworkId = 0;
inline Vec3 InsecWardPosition = {};
inline Vec3 InsecDestinationPosition = {};
inline DWORD InsecStageTick = 0;
inline DWORD InsecLastActionTick = 0;
inline DWORD InsecLastMoveTick = 0;
inline Vec3 InsecLastMovePosition = {};

inline RecastPhase LastQCastPhase = RecastPhase::Unknown;
inline RecastPhase LastWCastPhase = RecastPhase::Unknown;
inline RecastPhase LastECastPhase = RecastPhase::Unknown;
inline DWORD LastQCastTick = 0;
inline DWORD LastWCastTick = 0;
inline DWORD LastECastTick = 0;

static float SimulatedRDamage(const AIHeroClient& target);

static AIHeroClient Player() {
    return ObjectManager::Player();
}

static bool EqualsIgnoreCase(const char* left, const char* right) {
    return left && right && left[0] && right[0] && _stricmp(left, right) == 0;
}

static std::string CurrentSpellName(SpellSlot slot) {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }

    const auto spell = player.GetSpell(slot);
    return spell.IsValid() ? spell.Name() : std::string();
}

static RecastPhase ResolvePhase(SpellSlot slot,
                                const char* firstName,
                                const char* legacyFirstName,
                                const char* secondName,
                                const char* legacySecondName) {
    const std::string name = CurrentSpellName(slot);
    if (EqualsIgnoreCase(name.c_str(), firstName) ||
        EqualsIgnoreCase(name.c_str(), legacyFirstName)) {
        return RecastPhase::First;
    }
    if (EqualsIgnoreCase(name.c_str(), secondName) ||
        EqualsIgnoreCase(name.c_str(), legacySecondName)) {
        return RecastPhase::Second;
    }
    return RecastPhase::Unknown;
}

static RecastPhase QPhase() {
    const std::string name = CurrentSpellName(SpellSlot::Q);
    if (EqualsIgnoreCase(name.c_str(), "LeeSinQOne") ||
        EqualsIgnoreCase(name.c_str(), "BlindMonkQOne")) {
        return RecastPhase::First;
    }
    if (EqualsIgnoreCase(name.c_str(), "LeeSinQTwo") ||
        EqualsIgnoreCase(name.c_str(), "BlindMonkQTwo") ||
        EqualsIgnoreCase(name.c_str(), "LeeSinQTwoDash") ||
        EqualsIgnoreCase(name.c_str(), "BlindMonkQTwoDash")) {
        return RecastPhase::Second;
    }
    return RecastPhase::Unknown;
}

static bool QRecastPending() {
    const RecastPhase phase = QPhase();
    if (phase == RecastPhase::Second) {
        return true;
    }
    if (phase != RecastPhase::Unknown || QMarkedTargetNetworkId == 0 ||
        LastQCastPhase != RecastPhase::First) {
        return false;
    }

    const DWORD now = GetTickCount();
    return now - LastQCastTick >= 200 && now <= QRecastExpireTick;
}

static RecastPhase WPhase() {
    const std::string name = CurrentSpellName(SpellSlot::W);
    if (EqualsIgnoreCase(name.c_str(), "LeeSinWOne") ||
        EqualsIgnoreCase(name.c_str(), "BlindMonkWOne")) {
        return RecastPhase::First;
    }
    if (EqualsIgnoreCase(name.c_str(), "LeeSinWTwo") ||
        EqualsIgnoreCase(name.c_str(), "BlindMonkWTwo")) {
        return RecastPhase::Second;
    }
    return RecastPhase::Unknown;
}

static RecastPhase EPhase() {
    const std::string name = CurrentSpellName(SpellSlot::E);
    if (EqualsIgnoreCase(name.c_str(), "LeeSinEOne") ||
        EqualsIgnoreCase(name.c_str(), "BlindMonkEOne")) {
        return RecastPhase::First;
    }
    if (EqualsIgnoreCase(name.c_str(), "LeeSinETwo") ||
        EqualsIgnoreCase(name.c_str(), "BlindMonkETwo") ||
        EqualsIgnoreCase(name.c_str(), "LeeSinETwoDebuff") ||
        EqualsIgnoreCase(name.c_str(), "BlindMonkETwoDebuff")) {
        return RecastPhase::Second;
    }
    return RecastPhase::Unknown;
}

static bool IsPassiveBuffName(const char* name) {
    return EqualsIgnoreCase(name, "LeeSinPassiveBuff") ||
           EqualsIgnoreCase(name, "blindmonkpassive_cosmetic");
}

static int DetectPassiveStacks() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return 0;
    }

    uintptr_t buffs[256] = {};
    const int count = CoreBuffs::Enumerate(player.Address(), buffs, 256);
    const float gameTime = CoreBuffs::ResolveGameTime();
    int fallbackStacks = 0;
    int counterStacks = 0;
    bool hasGameplayCounter = false;
    char name[96] = {};

    for (int i = 0; i < count; ++i) {
        CoreBuffs::BuffRef buff{ buffs[i] };
        if (!buff.IsActive(gameTime) ||
            !buff.ReadName(name, static_cast<int>(sizeof(name))) ||
            !IsPassiveBuffName(name)) {
            continue;
        }

        const int raw38 = Globals::Read<int>(
            buff.address + Offset::BuffDataLayout::BuffStacks);
        const int raw3C = Globals::Read<int>(
            buff.address + Offset::BuffDataLayout::BuffStacksAlt);
        fallbackStacks = std::max(fallbackStacks, raw3C > 0 ? raw3C : raw38);

        const int current = buff.GetCounterCurrent();
        const int maximum = buff.GetCounterMax();
        if (maximum == 2 && current >= 0 && current <= maximum) {
            hasGameplayCounter = true;
            counterStacks = std::max(counterStacks, current);
        }
    }

    return std::clamp(hasGameplayCounter ? counterStacks : fallbackStacks, 0, 2);
}

static int CurrentPassiveStacks() {
    return static_cast<int>(InterlockedCompareExchange(&PassiveStacks, 0, 0));
}

static bool ComboEnabled(const char* key) {
    if (!ComboMenu || !key) {
        return true;
    }
    const auto* item = ComboMenu->Get<MenuBool>(key);
    return item ? item->Value : true;
}

static bool IsValidEnemy(const AIHeroClient& target, float range = FLT_MAX) {
    return target.IsValid() && !target.IsDead() && target.Health() > 0.0f &&
           Extensions::IsValidTarget(target, range, true);
}

static AIHeroClient GetComboTarget(float range) {
    auto* selector = SDK::TargetSelector::Instance();
    if (!selector) {
        return AIHeroClient();
    }
    const auto target = selector->GetTarget(range, DamageType::Physical);
    return IsValidEnemy(target, range) ? target : AIHeroClient();
}

static AIHeroClient GetNearestInsecTarget() {
    const auto player = Player();
    if (!player.IsValid()) {
        return AIHeroClient();
    }

    AIHeroClient closest = {};
    float closestDistance = FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!IsValidEnemy(enemy)) {
            continue;
        }
        const float distance = player.Position().Distance2D(enemy.Position());
        if (distance < closestDistance) {
            closest = enemy;
            closestDistance = distance;
        }
    }
    return closest;
}

static bool GetInsecDestination(const AIHeroClient& target, Vec3& position) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return false;
    }

    float closestDistance = FLT_MAX;
    bool found = false;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!ally.IsValid() || ally.IsDead() || ally.Compare(player)) {
            continue;
        }

        const float distance = target.Position().Distance2D(ally.Position());
        if (distance < closestDistance) {
            position = ally.Position();
            closestDistance = distance;
            found = true;
        }
    }
    if (found) {
        return true;
    }

    closestDistance = FLT_MAX;
    for (const auto& turret : GameObjects::AllyTurrets()) {
        if (!turret.IsValid() || turret.IsDead()) {
            continue;
        }

        const float distance = target.Position().Distance2D(turret.Position());
        if (distance < closestDistance) {
            position = turret.Position();
            closestDistance = distance;
            found = true;
        }
    }
    return found;
}

static float InsecWardCastRange() {
    const float wRange = W.Range > 0.0f ? W.Range : W1Range;
    return std::min(WardPlacementRange,
                    std::max(0.0f, wRange - InsecWardSafetyMargin));
}

static float InsecWardOffset() {
    return std::min(InsecWardBehindDistance,
                    std::min(InsecWardCastRange(),
                             std::max(0.0f, RRange - InsecKickSafetyMargin)));
}

static bool GetInsecWardPosition(const AIHeroClient& target,
                                 const Vec3& destination,
                                 Vec3& position) {
    const Vec3 direction{
        destination.x - target.Position().x,
        0.0f,
        destination.z - target.Position().z
    };
    if (direction.IsZero() || !direction.IsValid()) {
        return false;
    }

    const Vec3 normalized = direction.Normalized();
    const float offset = InsecWardOffset();
    position = {
        target.Position().x - normalized.x * offset,
        target.Position().y,
        target.Position().z - normalized.z * offset
    };
    return position.IsValid() && !position.IsZero();
}

static bool CanDirectInsecWard(const AIHeroClient& player,
                               const AIHeroClient& target,
                               const Vec3& wardPosition) {
    if (!player.IsValid() || !target.IsValid() ||
        !wardPosition.IsValid() || wardPosition.IsZero()) {
        return false;
    }

    const float targetDistance = player.Position().Distance2D(target.Position());
    return targetDistance <= InsecDirectRange;
}

static bool IssueInsecMove(const AIHeroClient& player, const Vec3& position) {
    if (!player.IsValid() || !position.IsValid() || position.IsZero() ||
        player.Position().Distance2D(position) <= 65.0f) {
        return false;
    }

    const DWORD now = GetTickCount();
    if (now - InsecLastMoveTick < InsecMoveIntervalMs ||
        (InsecLastMovePosition.IsValid() &&
         InsecLastMovePosition.Distance2D(position) < 35.0f)) {
        return false;
    }

    const int orbwalkerMoveTick = Orbwalker::LastMovementTick();
    Orbwalker::Move(position);
    if (Orbwalker::LastMovementTick() == orbwalkerMoveTick &&
        orbwalkerMoveTick != static_cast<int>(now)) {
        SDK::IssueOrder(player, SDK::GameObjectOrder::MoveTo, position);
    }

    InsecLastMoveTick = now;
    InsecLastMovePosition = position;
    return true;
}

static bool HasInsecActionIntervalElapsed(DWORD now) {
    return now - InsecLastActionTick >= InsecActionIntervalMs;
}

static void SetInsecStage(InsecStage stage) {
    CurrentInsecStage = stage;
    InsecStageTick = GetTickCount();
    InsecLastActionTick = 0;
}

static bool HasActiveBuffAlias(const AIBaseClient& unit,
                               std::initializer_list<const char*> names) {
    if (!unit.IsValid()) {
        return false;
    }
    const float gameTime = CoreBuffs::ResolveGameTime();
    for (const char* name : names) {
        if (name && CoreBuffs::HasActiveBuff(unit.Address(), name, gameTime)) {
            return true;
        }
    }
    return false;
}

static bool HasQMark(const AIHeroClient& target) {
    if (HasActiveBuffAlias(
            target,
            { "LeeSinQOne", "BlindMonkQOne", "LeeSinQMark", "BlindMonkQMark" })) {
        return true;
    }
    const float gameTime = CoreBuffs::ResolveGameTime();
    return CoreBuffs::HasActiveBuffContaining(
               target.Address(), "leesinq", -1, gameTime) ||
           CoreBuffs::HasActiveBuffContaining(
               target.Address(), "blindmonkq", -1, gameTime);
}

static AIHeroClient MarkedQTarget() {
    if (QMarkedTargetNetworkId == 0) {
        return AIHeroClient();
    }

    const auto marked = ObjectManager::GetUnitByNetworkId<AIHeroClient>(
        static_cast<int>(QMarkedTargetNetworkId));
    if (!IsValidEnemy(marked, Q2Range)) {
        QMarkedTargetNetworkId = 0;
        return AIHeroClient();
    }

    const bool markConfirmed = HasQMark(marked) ||
                                QPhase() == RecastPhase::Second ||
                                QRecastPending();
    if (!markConfirmed) {
        const DWORD now = GetTickCount();
        const bool q1StillUpdating =
            LastQCastPhase == RecastPhase::First &&
            now - LastQCastTick <= 1000;
        if (!q1StillUpdating) {
            QMarkedTargetNetworkId = 0;
        }
        return AIHeroClient();
    }
    return marked;
}

static int CountOtherEnemiesNear(const AIHeroClient& center, float radius) {
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!IsValidEnemy(enemy) || enemy.NetworkId() == center.NetworkId()) {
            continue;
        }
        if (enemy.Position().Distance2D(center.Position()) <= radius) {
            ++count;
        }
    }
    return count;
}

static bool IsCloseOneVsOne(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !IsValidEnemy(target)) {
        return false;
    }
    return AutoAttack::InAutoAttackRange(target) &&
           CountOtherEnemiesNear(target, Q2SafetyRadius) == 0;
}

static bool CanStartNewSkill(const AIHeroClient& target) {
    return !IsCloseOneVsOne(target) || CurrentPassiveStacks() <= 1;
}

static float ComboQ2HealthThreshold() {
    const int value = FancyQ2HealthMenu
        ? FancyQ2HealthMenu->Value
        : static_cast<int>(Q2HealthThreshold * 100.0f);
    return static_cast<float>(std::clamp(value, 0, 100)) / 100.0f;
}

static int ComboQ2MaxNearbyEnemies() {
    return std::clamp(FancyQ2EnemiesMenu ? FancyQ2EnemiesMenu->Value : 1, 0, 5);
}

static bool Q2IsSafeAtHealth(const AIHeroClient& target, float health) {
    const float maxHealth = target.MaxHealth();
    if (maxHealth <= 0.0f || health / maxHealth > ComboQ2HealthThreshold()) {
        return false;
    }
    return CountOtherEnemiesNear(target, Q2SafetyRadius) <= ComboQ2MaxNearbyEnemies();
}

static bool Q2IsSafe(const AIHeroClient& target) {
    return Q2IsSafeAtHealth(target, target.Health());
}

static bool CastQ1(const AIHeroClient& target) {
    const auto phase = QPhase();
    const DWORD now = GetTickCount();
    if (phase != RecastPhase::First || !Q.IsReady() ||
        (phase == LastQCastPhase && now - LastQCastTick < 300) ||
        !IsValidEnemy(target, Q1Range)) {
        return false;
    }
    const auto result = Q.Cast(target, false, false, -1, HitChance::High);
    if (result != CastStates::SuccessfullyCasted) {
        return false;
    }
    LastQCastPhase = phase;
    LastQCastTick = now;
    QMarkedTargetNetworkId = target.NetworkId();
    QRecastExpireTick = now + 3000;
    Q1AwaitingAutoAttack = AutoAttack::InAutoAttackRange(target);
    Q2AwaitingAutoAttack = false;
    return true;
}

static bool CastQ2(const AIHeroClient& target) {
    const auto phase = QPhase();
    const DWORD now = GetTickCount();
    const bool markConfirmed = HasQMark(target) ||
                                phase == RecastPhase::Second ||
                                QRecastPending();
    const bool recastAvailable = phase == RecastPhase::Second ||
                                  (QRecastPending() && markConfirmed);
    if (!recastAvailable || !Q.IsReady() ||
        (phase == LastQCastPhase && now - LastQCastTick < 300) ||
        !IsValidEnemy(target, Q2Range) || !markConfirmed || !Q2IsSafe(target)) {
        return false;
    }
    const bool waitForAutoAttack = !AutoAttack::InAutoAttackRange(target);
    if (!Q.CastOnUnit(target)) {
        return false;
    }
    LastQCastPhase = phase == RecastPhase::Unknown ? RecastPhase::Second : phase;
    LastQCastTick = now;
    QMarkedTargetNetworkId = 0;
    QRecastExpireTick = 0;
    Q1AwaitingAutoAttack = false;
    Q2AwaitingAutoAttack = waitForAutoAttack;
    return true;
}

static bool CastInsecQ2(const AIHeroClient& target) {
    const auto phase = QPhase();
    const DWORD now = GetTickCount();
    const bool markConfirmed = HasQMark(target) ||
                                phase == RecastPhase::Second ||
                                QRecastPending();
    const bool recastAvailable = phase == RecastPhase::Second ||
                                  (QRecastPending() && markConfirmed);
    if (!recastAvailable || !Q.IsReady() ||
        (phase == LastQCastPhase && now - LastQCastTick < 300) ||
        !IsValidEnemy(target, Q2Range) || !markConfirmed) {
        return false;
    }

    const bool waitForAutoAttack = !AutoAttack::InAutoAttackRange(target);
    if (!Q.CastOnUnit(target)) {
        return false;
    }
    LastQCastPhase = phase == RecastPhase::Unknown ? RecastPhase::Second : phase;
    LastQCastTick = now;
    QMarkedTargetNetworkId = 0;
    QRecastExpireTick = 0;
    Q1AwaitingAutoAttack = false;
    Q2AwaitingAutoAttack = waitForAutoAttack;
    return true;
}

static bool CastInsecR(const AIHeroClient& target) {
    if (!R.IsReady() || !IsValidEnemy(target, RRange)) {
        return false;
    }
    return R.CastOnUnit(target);
}

static bool CastW1(const AIHeroClient& target) {
    const auto player = Player();
    const auto phase = WPhase();
    const DWORD now = GetTickCount();
    if (phase != RecastPhase::First || !W.IsReady() || !player.IsValid() ||
        (phase == LastWCastPhase && now - LastWCastTick < 300) ||
        !IsValidEnemy(target) || !AutoAttack::InAutoAttackRange(target)) {
        return false;
    }
    if (!W.CastOnUnit(player)) {
        return false;
    }
    LastWCastPhase = phase;
    LastWCastTick = now;
    return true;
}

static bool CastW2(const AIHeroClient& target) {
    const auto player = Player();
    const auto phase = WPhase();
    const DWORD now = GetTickCount();
    if (phase != RecastPhase::Second || !W.IsReady() || !player.IsValid() ||
        (phase == LastWCastPhase && now - LastWCastTick < 300) ||
        !IsValidEnemy(target) || !AutoAttack::InAutoAttackRange(target)) {
        return false;
    }
    if (!W.CastOnUnit(player)) {
        return false;
    }
    LastWCastPhase = phase;
    LastWCastTick = now;
    return true;
}

static void ClearKnownWardNetworkIds() {
    WardHopKnownWardCount = 0;
}

static void RememberKnownWardNetworkId(uint32_t networkId) {
    if (networkId == 0 || WardHopKnownWardCount >= 64) {
        return;
    }
    for (int i = 0; i < WardHopKnownWardCount; ++i) {
        if (WardHopKnownWardNetworkIds[i] == networkId) {
            return;
        }
    }
    WardHopKnownWardNetworkIds[WardHopKnownWardCount++] = networkId;
}

static bool IsKnownWardNetworkId(uint32_t networkId) {
    if (networkId == 0) {
        return false;
    }
    for (int i = 0; i < WardHopKnownWardCount; ++i) {
        if (WardHopKnownWardNetworkIds[i] == networkId) {
            return true;
        }
    }
    return false;
}

static void SnapshotKnownWardNetworkIds() {
    ClearKnownWardNetworkIds();
    for (const auto& ward : GameObjects::AllyWards()) {
        if (ward.IsValid() && !ward.IsDead()) {
            RememberKnownWardNetworkId(ward.NetworkId());
        }
    }
    for (const auto& ward : GameObjects::Wards()) {
        if (ward.IsValid() && !ward.IsDead() && ward.IsAlly()) {
            RememberKnownWardNetworkId(ward.NetworkId());
        }
    }
}

static AIMinionClient FindAllyWardNear(const Vec3& position,
                                       float radius = 300.0f,
                                       bool onlyNew = false) {
    if (!position.IsValid() || position.IsZero()) {
        return AIMinionClient();
    }

    AIMinionClient closest = {};
    float closestDistance = radius;
    for (const auto& ward : GameObjects::AllyWards()) {
        if (!ward.IsValid() || ward.IsDead() ||
            (onlyNew && IsKnownWardNetworkId(ward.NetworkId()))) {
            continue;
        }

        const float distance = ward.Position().Distance2D(position);
        if (distance <= closestDistance) {
            closest = ward;
            closestDistance = distance;
        }
    }

    if (closest.IsValid()) {
        return closest;
    }

    for (const auto& ward : GameObjects::Wards()) {
        if (!ward.IsValid() || ward.IsDead() || !ward.IsAlly() ||
            (onlyNew && IsKnownWardNetworkId(ward.NetworkId()))) {
            continue;
        }

        const float distance = ward.Position().Distance2D(position);
        if (distance <= closestDistance) {
            closest = ward;
            closestDistance = distance;
        }
    }
    return closest;
}

static Vec3 GetWardHopPosition(const AIHeroClient& player) {
    if (!player.IsValid()) {
        return {};
    }

    const Vec3 playerPosition = player.Position();
    const Vec3 cursorPosition = Game::CursorPos();
    if (!cursorPosition.IsValid() || cursorPosition.IsZero()) {
        return {};
    }

    if (playerPosition.Distance2D(cursorPosition) > WardPlacementRange) {
        return playerPosition.Extend(cursorPosition, WardPlacementRange);
    }

    return { cursorPosition.x, playerPosition.y, cursorPosition.z };
}

static bool IsWardHopSpellName(const std::string& name) {
    return EqualsIgnoreCase(name.c_str(), "TrinketTotemLvl1") ||
           EqualsIgnoreCase(name.c_str(), "TrinketTotemLvl2") ||
           EqualsIgnoreCase(name.c_str(), "TrinketTotemLvl3") ||
           EqualsIgnoreCase(name.c_str(), "TrinketTotemLvl3B") ||
           EqualsIgnoreCase(name.c_str(), "SightWard") ||
           EqualsIgnoreCase(name.c_str(), "VisionWard") ||
           EqualsIgnoreCase(name.c_str(), "ItemGhostWard");
}

static SpellSlot GetWardHopSpellSlot(const AIHeroClient& player) {
    const auto spellbook = player.Spellbook();
    for (int slotIndex = static_cast<int>(SpellSlot::Item1);
         slotIndex <= static_cast<int>(SpellSlot::Trinket);
         ++slotIndex) {
        const SpellSlot slot = static_cast<SpellSlot>(slotIndex);
        const auto spell = spellbook.GetSpell(slot);
        if (!spell.IsValid() || !IsWardHopSpellName(spell.Name())) {
            continue;
        }
        if (spell.State(0.0f) == CoreSpellBook::State_Ready) {
            return slot;
        }
    }
    return SpellSlot::Unknown;
}

static bool IsW1ReadyForWardHop() {
    return WPhase() != RecastPhase::Second && W.IsReady();
}

static bool StartWardHopAt(const AIHeroClient& player, const Vec3& position) {
    if (!IsW1ReadyForWardHop() || !position.IsValid() || position.IsZero()) {
        return false;
    }

    const SpellSlot wardSpellSlot = GetWardHopSpellSlot(player);
    if (wardSpellSlot == SpellSlot::Unknown) {
        return false;
    }

    SnapshotKnownWardNetworkIds();
    if (!player.Spellbook().CastSpell(wardSpellSlot, position)) {
        ClearKnownWardNetworkIds();
        return false;
    }

    WardHopPosition = position;
    WardHopRequestTick = GetTickCount();
    WardHopPending = true;
    return true;
}

static bool StartWardHop(const AIHeroClient& player) {
    return StartWardHopAt(player, GetWardHopPosition(player));
}

static bool FinishWardHop() {
    if (!WardHopPending) {
        return false;
    }

    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        WardHopPending = false;
        WardHopPosition = {};
        return false;
    }

    const auto ward = FindAllyWardNear(WardHopPosition, 300.0f, true);
    if (!ward.IsValid() || !IsW1ReadyForWardHop()) {
        return false;
    }

    if (!W.CastOnUnit(ward)) {
        return false;
    }

    LastWCastPhase = RecastPhase::First;
    LastWCastTick = GetTickCount();
    WardHopPending = false;
    WardHopCompleted = true;
    WardHopPosition = {};
    WardHopRequestTick = 0;
    ClearKnownWardNetworkIds();
    return true;
}

static bool WardHopKeyActive() {
    if (!WardHopKeyMenu || WardHopKeyMenu->Key <= 0) {
        return false;
    }

    return WardHopKeyMenu->Active ||
           (GetAsyncKeyState(WardHopKeyMenu->Key) & 0x8000) != 0;
}

static bool HandleWardHop() {
    if (!WardHopEnabledMenu || !WardHopKeyMenu || !WardHopEnabledMenu->Value) {
        WardHopPending = false;
        WardHopKeyWasActive = false;
        WardHopCompleted = false;
        return false;
    }

    const DWORD now = GetTickCount();
    const bool keyActive = WardHopKeyActive();
    const bool retryWindow = WardHopRetryUntilTick != 0 &&
                             now < WardHopRetryUntilTick;
    if (!keyActive && !WardHopPending && !retryWindow) {
        WardHopKeyWasActive = false;
        WardHopCompleted = false;
        WardHopLastAttemptTick = 0;
        WardHopRetryUntilTick = 0;
        return false;
    }

    if (Game::IsChatOpen()) {
        return false;
    }

    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        WardHopPending = false;
        WardHopKeyWasActive = keyActive;
        WardHopCompleted = false;
        return keyActive || retryWindow;
    }

    if ((keyActive || retryWindow) && !WardHopPending && !WardHopCompleted &&
        (!WardHopKeyWasActive || now - WardHopLastAttemptTick >= WardHopRetryIntervalMs)) {
        WardHopKeyWasActive = true;
        WardHopLastAttemptTick = now;
        if (StartWardHop(player)) {
            WardHopRetryUntilTick = 0;
        } else {
            WardHopRetryUntilTick = now + WardHopRetryWindowMs;
        }
    }

    if (WardHopPending) {
        if (now - WardHopRequestTick > WardHopTimeoutMs) {
            WardHopPending = false;
            WardHopCompleted = true;
            WardHopPosition = {};
            WardHopRequestTick = 0;
            WardHopLastAttemptTick = 0;
            WardHopRetryUntilTick = 0;
            ClearKnownWardNetworkIds();
        } else {
            FinishWardHop();
        }
    }

    return keyActive || WardHopPending || retryWindow;
}

static void ResetInsecState() {
    CurrentInsecStage = InsecStage::Idle;
    InsecKeyWasActive = false;
    InsecTargetNetworkId = 0;
    InsecWardPosition = {};
    InsecDestinationPosition = {};
    InsecStageTick = 0;
    InsecLastActionTick = 0;
    InsecLastMoveTick = 0;
    InsecLastMovePosition = {};
    WardHopPending = false;
    WardHopCompleted = false;
    WardHopPosition = {};
    WardHopRequestTick = 0;
    WardHopLastAttemptTick = 0;
    WardHopRetryUntilTick = 0;
    ClearKnownWardNetworkIds();
}

static AIHeroClient GetActiveInsecTarget() {
    if (InsecTargetNetworkId == 0) {
        return AIHeroClient();
    }

    const auto target = ObjectManager::GetUnitByNetworkId<AIHeroClient>(
        static_cast<int>(InsecTargetNetworkId));
    return IsValidEnemy(target) ? target : AIHeroClient();
}

static bool BeginInsec() {
    const auto target = GetNearestInsecTarget();
    if (!target.IsValid()) {
        return false;
    }

    Vec3 destination = {};
    if (!GetInsecDestination(target, destination)) {
        return false;
    }

    InsecTargetNetworkId = target.NetworkId();
    InsecDestinationPosition = destination;
    WardHopPending = false;
    WardHopCompleted = false;
    SetInsecStage(InsecStage::NeedQ1);
    return true;
}

static bool RefreshInsecGeometry(const AIHeroClient& target) {
    Vec3 destination = {};
    if (!GetInsecDestination(target, destination)) {
        return false;
    }

    Vec3 wardPosition = {};
    if (!GetInsecWardPosition(target, destination, wardPosition)) {
        return false;
    }

    InsecDestinationPosition = destination;
    InsecWardPosition = wardPosition;
    return true;
}

static bool InsecActionReady(DWORD now) {
    if (!HasInsecActionIntervalElapsed(now)) {
        return false;
    }
    InsecLastActionTick = now;
    return true;
}

static bool HandleInsec() {
    const bool keyActive = InsecKeyMenu && InsecKeyMenu->Key > 0 &&
        (InsecKeyMenu->Active ||
         (::GetAsyncKeyState(InsecKeyMenu->Key) & 0x8000) != 0);

    if (!keyActive) {
        if (CurrentInsecStage != InsecStage::Idle) {
            ResetInsecState();
        }
        return false;
    }

    if (!InsecKeyWasActive) {
        InsecKeyWasActive = true;
        if (!BeginInsec()) {
            CurrentInsecStage = InsecStage::Completed;
        }
    }

    if (CurrentInsecStage == InsecStage::Completed) {
        return true;
    }

    if (Game::IsChatOpen()) {
        return true;
    }

    const auto player = Player();
    const auto target = GetActiveInsecTarget();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() ||
        !target.IsValid() || !RefreshInsecGeometry(target)) {
        CurrentInsecStage = InsecStage::Completed;
        return true;
    }

    const DWORD now = GetTickCount();
    switch (CurrentInsecStage) {
    case InsecStage::NeedQ1: {
        if (CanDirectInsecWard(player, target, InsecWardPosition)) {
            SetInsecStage(InsecStage::NeedWard);
            break;
        }

        const float targetDistance = player.Position().Distance2D(target.Position());
        const RecastPhase qPhase = QPhase();
        if (qPhase == RecastPhase::Second || HasQMark(target) || QRecastPending()) {
            SetInsecStage(InsecStage::NeedQ2);
            break;
        }

        if (targetDistance <= Q1Range && Q.IsReady()) {
            if (InsecActionReady(now) && CastQ1(target)) {
                SetInsecStage(InsecStage::NeedQ2);
            }
        } else {
            IssueInsecMove(player, target.Position());
        }
        break;
    }
    case InsecStage::NeedQ2: {
        if (CanDirectInsecWard(player, target, InsecWardPosition)) {
            SetInsecStage(InsecStage::NeedWard);
            break;
        }
        if (CastInsecQ2(target)) {
            SetInsecStage(InsecStage::NeedWard);
        } else if (!HasQMark(target) && QPhase() != RecastPhase::Second &&
                   !QRecastPending()) {
            SetInsecStage(InsecStage::NeedQ1);
        } else {
            IssueInsecMove(player, target.Position());
        }
        break;
    }
    case InsecStage::NeedWard:
        if (player.Position().Distance2D(InsecWardPosition) > InsecWardCastRange()) {
            IssueInsecMove(player, InsecWardPosition);
            break;
        }

        if (IsW1ReadyForWardHop() && !WardHopPending && !WardHopCompleted &&
            InsecActionReady(now) && StartWardHopAt(player, InsecWardPosition)) {
            SetInsecStage(InsecStage::NeedW);
        }
        break;
    case InsecStage::NeedW:
        if (WardHopCompleted) {
            SetInsecStage(InsecStage::NeedR);
        } else if (WardHopPending) {
            if (now - WardHopRequestTick > WardHopTimeoutMs) {
                ResetInsecState();
                InsecKeyWasActive = true;
                CurrentInsecStage = InsecStage::Completed;
            } else if (FinishWardHop()) {
                SetInsecStage(InsecStage::NeedR);
            }
        } else {
            CurrentInsecStage = InsecStage::Completed;
        }
        break;
    case InsecStage::NeedR:
        if (now - InsecStageTick < InsecActionIntervalMs) {
            break;
        }
        if (!IsValidEnemy(target, RRange)) {
            IssueInsecMove(player, target.Position());
            break;
        }
        if (R.IsReady() && InsecActionReady(now) && CastInsecR(target)) {
            SetInsecStage(InsecStage::Completed);
        }
        break;
    case InsecStage::Idle:
    case InsecStage::Completed:
        break;
    }

    return true;
}

static bool CastE1(const AIHeroClient& target) {
    const auto phase = EPhase();
    const DWORD now = GetTickCount();
    if (phase != RecastPhase::First || !E.IsReady() ||
        (phase == LastECastPhase && now - LastECastTick < 300) ||
        !IsValidEnemy(target, E1Range)) {
        return false;
    }
    if (!E.Cast()) {
        return false;
    }
    LastECastPhase = phase;
    LastECastTick = now;
    return true;
}

static bool CastFancyE1() {
    const auto phase = EPhase();
    const DWORD now = GetTickCount();
    if (phase != RecastPhase::First || !E.IsReady() ||
        (phase == LastECastPhase && now - LastECastTick < 300)) {
        return false;
    }
    if (!E.Cast()) {
        return false;
    }
    LastECastPhase = phase;
    LastECastTick = now;
    return true;
}

static bool CastE2(const AIHeroClient& target) {
    const auto phase = EPhase();
    const DWORD now = GetTickCount();
    if (phase != RecastPhase::Second || !E.IsReady() ||
        (phase == LastECastPhase && now - LastECastTick < 300) ||
        !IsValidEnemy(target, E2Range)) {
        return false;
    }
    if (!E.Cast()) {
        return false;
    }
    LastECastPhase = phase;
    LastECastTick = now;
    return true;
}

static bool FancyComboEnabled() {
    return FancyComboMenu && FancyComboMenu->Value;
}

static void ResetFancyComboState() {
    CurrentFancyComboStage = FancyComboStage::Idle;
    FancyComboTargetNetworkId = 0;
    FancyComboWardPosition = {};
    FancyComboStageTick = 0;
    WardHopPending = false;
    WardHopCompleted = false;
    WardHopPosition = {};
    WardHopRequestTick = 0;
    WardHopLastAttemptTick = 0;
    WardHopRetryUntilTick = 0;
    ClearKnownWardNetworkIds();
}

static void SetFancyComboStage(FancyComboStage stage) {
    CurrentFancyComboStage = stage;
    FancyComboStageTick = GetTickCount();
}

static AIHeroClient GetActiveFancyComboTarget() {
    if (FancyComboTargetNetworkId == 0) {
        return AIHeroClient();
    }

    const auto target = ObjectManager::GetUnitByNetworkId<AIHeroClient>(
        static_cast<int>(FancyComboTargetNetworkId));
    return IsValidEnemy(target, Q2Range) ? target : AIHeroClient();
}

static bool BeginFancyCombo(const AIHeroClient& target) {
    if (!FancyComboEnabled() || !ComboEnabled("useQ") ||
        !ComboEnabled("useW") || !target.IsValid() || !Q2IsSafe(target) ||
        !IsW1ReadyForWardHop()) {
        return false;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }

    const Vec3 playerPosition = player.Position();
    const Vec3 targetPosition = target.Position();
    const Vec3 direction{
        targetPosition.x - playerPosition.x,
        0.0f,
        targetPosition.z - playerPosition.z
    };
    const float targetDistance = playerPosition.Distance2D(targetPosition);
    if (!direction.IsValid() || direction.IsZero() ||
        targetDistance <= FancyComboWardMinDistance) {
        return false;
    }

    const Vec3 normalized = direction.Normalized();
    const float wardDistance = std::min(
        FancyComboWardDistance,
        targetDistance - FancyComboWardMinDistance);
    const Vec3 wardPosition{
        playerPosition.x + normalized.x * wardDistance,
        playerPosition.y,
        playerPosition.z + normalized.z * wardDistance
    };
    if (!wardPosition.IsValid() || wardPosition.IsZero() ||
        GetWardHopSpellSlot(player) == SpellSlot::Unknown) {
        return false;
    }

    FancyComboTargetNetworkId = target.NetworkId();
    FancyComboWardPosition = wardPosition;
    WardHopPending = false;
    WardHopCompleted = false;
    SetFancyComboStage(FancyComboStage::NeedWard);
    return true;
}

static bool StartFancyQ2(const AIHeroClient& target) {
    if (!CastQ2(target)) {
        return false;
    }

    SetFancyComboStage(FancyComboStage::Q2InFlight);
    if (!ComboEnabled("useE")) {
        ResetFancyComboState();
        return true;
    }
    if (CastFancyE1()) {
        ResetFancyComboState();
    }
    return true;
}

static bool HandleFancyCombo() {
    if (Orbwalker::ActiveMode() != OrbwalkingMode::Combo ||
        !FancyComboEnabled()) {
        if (CurrentFancyComboStage != FancyComboStage::Idle) {
            ResetFancyComboState();
        }
        return false;
    }

    if (CurrentFancyComboStage == FancyComboStage::Idle) {
        const auto markedTarget = MarkedQTarget();
        if (!markedTarget.IsValid() || !BeginFancyCombo(markedTarget)) {
            return false;
        }
    }

    if (Game::IsChatOpen()) {
        return true;
    }

    const auto player = Player();
    const auto target = GetActiveFancyComboTarget();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() ||
        !target.IsValid()) {
        ResetFancyComboState();
        return false;
    }

    const DWORD now = GetTickCount();
    switch (CurrentFancyComboStage) {
    case FancyComboStage::NeedWard:
        if (!IsW1ReadyForWardHop() ||
            GetWardHopSpellSlot(player) == SpellSlot::Unknown) {
            ResetFancyComboState();
            return false;
        }
        if (!WardHopPending && !WardHopCompleted) {
            if (!StartWardHopAt(player, FancyComboWardPosition)) {
                ResetFancyComboState();
                return false;
            }
            SetFancyComboStage(FancyComboStage::NeedW);
        }
        return true;
    case FancyComboStage::NeedW:
        if (WardHopPending && now - WardHopRequestTick > WardHopTimeoutMs) {
            ResetFancyComboState();
            return false;
        }
        if (WardHopPending && !FinishWardHop()) {
            return true;
        }
        if (!WardHopCompleted) {
            ResetFancyComboState();
            return false;
        }
        SetFancyComboStage(FancyComboStage::NeedQ2);
        return true;
    case FancyComboStage::NeedQ2: {
        const DWORD w1Elapsed = now - LastWCastTick;
        if (w1Elapsed < FancyComboW1ToQ2DelayMs ||
            (WPhase() != RecastPhase::Second &&
             w1Elapsed < FancyComboW1ConfirmTimeoutMs)) {
            return true;
        }
        if (StartFancyQ2(target)) {
            return true;
        }
        if (!HasQMark(target) && QPhase() != RecastPhase::Second &&
            !QRecastPending()) {
            ResetFancyComboState();
            return false;
        }
        return true;
    }
    case FancyComboStage::Q2InFlight:
        if (now - FancyComboStageTick > FancyComboQ2FlightWindowMs) {
            ResetFancyComboState();
            return false;
        }
        if (ComboEnabled("useE") && CastFancyE1()) {
            ResetFancyComboState();
            return false;
        }
        return true;
    case FancyComboStage::Idle:
        break;
    }

    return false;
}

static bool MultiRComboEnabled() {
    return MultiRComboMenu && MultiRComboMenu->Value;
}

static int MultiRMinCollisions() {
    return std::clamp(MultiRMinCollisionsMenu ? MultiRMinCollisionsMenu->Value : 2, 2, 5);
}

static AIHeroClient GetActiveMultiRTarget() {
    if (MultiRTargetNetworkId == 0) {
        return AIHeroClient();
    }
    const auto target = ObjectManager::GetUnitByNetworkId<AIHeroClient>(
        static_cast<int>(MultiRTargetNetworkId));
    return IsValidEnemy(target, Q1Range) ? target : AIHeroClient();
}

static Vec3 MultiRDirectionBetween(const Vec3& from, const Vec3& to) {
    const float x = to.x - from.x;
    const float z = to.z - from.z;
    const float length = std::sqrt(x * x + z * z);
    if (!std::isfinite(length) || length <= 1.0f) {
        return {};
    }
    return { x / length, 0.0f, z / length };
}

static bool MultiRPathContains(const AIHeroClient& target,
                               const AIHeroClient& other,
                               const Vec3& direction) {
    if (!target.IsValid() || !IsValidEnemy(other) ||
        target.NetworkId() == other.NetworkId()) {
        return false;
    }

    const Vec3 relative{
        other.Position().x - target.Position().x,
        0.0f,
        other.Position().z - target.Position().z
    };
    const float projection = relative.x * direction.x + relative.z * direction.z;
    if (projection < MultiRKickStartOffset ||
        projection > MultiRKickDistance) {
        return false;
    }

    const Vec3 closestPoint{
        target.Position().x + direction.x * projection,
        target.Position().y,
        target.Position().z + direction.z * projection
    };
    const float collisionRadius = MultiRKickCollisionRadius +
        std::max(0.0f, other.BoundingRadius());
    return other.Position().Distance2D(closestPoint) <= collisionRadius;
}

static int CountMultiRCollisions(const AIHeroClient& target,
                                 const Vec3& direction) {
    int count = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (MultiRPathContains(target, enemy, direction)) {
            ++count;
        }
    }
    return count;
}

static Vec3 MultiRWardPositionFor(const AIHeroClient& target,
                                  const Vec3& direction) {
    return {
        target.Position().x - direction.x * MultiRWardDistance,
        target.Position().y,
        target.Position().z - direction.z * MultiRWardDistance
    };
}

static MultiRKickCandidate BuildMultiRDirectCandidate(
    const AIHeroClient& target,
    const Vec3& playerPosition) {
    MultiRKickCandidate candidate = {};
    if (!IsValidEnemy(target, Q1Range)) {
        return candidate;
    }

    const Vec3 direction = MultiRDirectionBetween(playerPosition, target.Position());
    if (!direction.IsValid() || direction.IsZero()) {
        return candidate;
    }

    candidate.Target = target;
    candidate.Direction = direction;
    candidate.WardPosition = MultiRWardPositionFor(target, direction);
    candidate.CollisionCount = CountMultiRCollisions(target, direction);
    candidate.TargetDistance = playerPosition.Distance2D(target.Position());
    candidate.Direct = candidate.TargetDistance <= RRange;
    return candidate;
}

static MultiRKickCandidate BuildMultiRDirectionalCandidate(
    const AIHeroClient& target,
    const AIHeroClient& seed,
    const Vec3& playerPosition) {
    MultiRKickCandidate candidate = {};
    if (!IsValidEnemy(target, Q1Range) || !IsValidEnemy(seed) ||
        target.NetworkId() == seed.NetworkId()) {
        return candidate;
    }

    const Vec3 direction = MultiRDirectionBetween(target.Position(), seed.Position());
    if (!direction.IsValid() || direction.IsZero()) {
        return candidate;
    }

    candidate.Target = target;
    candidate.Direction = direction;
    candidate.WardPosition = MultiRWardPositionFor(target, direction);
    candidate.CollisionCount = CountMultiRCollisions(target, direction);
    candidate.TargetDistance = playerPosition.Distance2D(target.Position());
    candidate.Direct = false;
    return candidate;
}

static bool IsBetterMultiRCandidate(const MultiRKickCandidate& candidate,
                                    const MultiRKickCandidate& current) {
    if (!candidate.Target.IsValid()) {
        return false;
    }
    if (!current.Target.IsValid()) {
        return true;
    }
    if (candidate.CollisionCount != current.CollisionCount) {
        return candidate.CollisionCount > current.CollisionCount;
    }
    if (candidate.Direct != current.Direct) {
        return candidate.Direct;
    }
    return candidate.TargetDistance < current.TargetDistance;
}

static MultiRKickCandidate BestMultiRCandidateForTarget(
    const AIHeroClient& target,
    const Vec3& playerPosition) {
    MultiRKickCandidate best = BuildMultiRDirectCandidate(target, playerPosition);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const auto candidate = BuildMultiRDirectionalCandidate(
            target, enemy, playerPosition);
        if (IsBetterMultiRCandidate(candidate, best)) {
            best = candidate;
        }
    }
    return best;
}

static MultiRKickCandidate FindBestMultiRCandidate(
    const Vec3& playerPosition) {
    MultiRKickCandidate best = {};
    const int minimum = MultiRMinCollisions();
    for (const auto& target : GameObjects::EnemyHeroes()) {
        const auto candidate = BestMultiRCandidateForTarget(target, playerPosition);
        if (candidate.CollisionCount < minimum ||
            !IsBetterMultiRCandidate(candidate, best)) {
            continue;
        }
        best = candidate;
    }
    return best;
}

static void SetMultiRPlan(const MultiRKickCandidate& candidate) {
    MultiRTargetNetworkId = candidate.Target.NetworkId();
    MultiRDirection = candidate.Direction;
    MultiRWardPosition = candidate.WardPosition;
    MultiRCollisionCount = candidate.CollisionCount;
}

static bool MultiRCanUseWard(const AIHeroClient& player) {
    if (!player.IsValid() || !MultiRWardPosition.IsValid() ||
        MultiRWardPosition.IsZero() || !IsW1ReadyForWardHop() ||
        GetWardHopSpellSlot(player) == SpellSlot::Unknown) {
        return false;
    }
    const float maxDistance = std::min(
        WardPlacementRange, W1Range - MultiRWardSafetyMargin);
    return player.Position().Distance2D(MultiRWardPosition) <= maxDistance;
}

static bool MultiRActionReady(DWORD now) {
    if (MultiRLastActionTick != 0 &&
        now - MultiRLastActionTick < MultiRActionIntervalMs) {
        return false;
    }
    MultiRLastActionTick = now;
    return true;
}

static void SetMultiRStage(MultiRStage stage) {
    CurrentMultiRStage = stage;
    MultiRStageTick = GetTickCount();
    MultiRLastActionTick = 0;
}

static void ResetMultiRState() {
    const bool ownedWardState = CurrentMultiRStage == MultiRStage::NeedWard ||
        CurrentMultiRStage == MultiRStage::NeedW ||
        MultiRNeedsWConfirm;
    CurrentMultiRStage = MultiRStage::Idle;
    MultiRTargetNetworkId = 0;
    MultiRDirection = {};
    MultiRWardPosition = {};
    MultiRCollisionCount = 0;
    MultiRNeedsWConfirm = false;
    MultiRQ2CastTick = 0;
    MultiRStageTick = 0;
    MultiRLastActionTick = 0;
    if (ownedWardState) {
        WardHopPending = false;
        WardHopCompleted = false;
        WardHopPosition = {};
        WardHopRequestTick = 0;
        ClearKnownWardNetworkIds();
    }
}

static bool BeginMultiRCombo() {
    if (!MultiRComboEnabled() || !R.IsReady()) {
        return false;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }
    const auto candidate = FindBestMultiRCandidate(player.Position());
    if (!candidate.Target.IsValid() ||
        candidate.CollisionCount < MultiRMinCollisions()) {
        return false;
    }

    SetMultiRPlan(candidate);
    const auto target = candidate.Target;
    const auto direct = BuildMultiRDirectCandidate(target, player.Position());
    if (direct.Direct && direct.CollisionCount >= MultiRMinCollisions()) {
        SetMultiRPlan(direct);
        SetMultiRStage(MultiRStage::NeedR);
        return true;
    }
    if (HasQMark(target) || QPhase() == RecastPhase::Second ||
        QRecastPending()) {
        SetMultiRStage(MultiRStage::NeedQ2);
        return true;
    }
    if (Q.IsReady() && IsValidEnemy(target, Q1Range)) {
        SetMultiRStage(MultiRStage::NeedQ1);
        return true;
    }
    if (MultiRCanUseWard(player)) {
        SetMultiRStage(MultiRStage::NeedWard);
        return true;
    }

    if (direct.Direct && direct.CollisionCount >= MultiRMinCollisions()) {
        SetMultiRPlan(direct);
        SetMultiRStage(MultiRStage::NeedR);
        return true;
    }
    return false;
}

static bool IsRProtectedTarget(const AIHeroClient& target) {
    return HasActiveBuffAlias(
        target,
        { "JudicatorIntervention", "kindredrnodeathbuff", "Undying Rage",
          "FioraW", "BlitzcrankManaBarrierCO" });
}

static bool HandleRAutoKillSteal() {
    if (!RAutoKillStealMenu || !RAutoKillStealMenu->Value ||
        !R.IsReady() || CurrentMultiRStage != MultiRStage::Idle) {
        return false;
    }

    AIHeroClient bestTarget = {};
    float bestHealthPercent = FLT_MAX;
    float bestDistance = FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!IsValidEnemy(enemy, RRange) || IsRProtectedTarget(enemy)) {
            continue;
        }
        const float totalHealth = enemy.Health() + enemy.AllShield();
        if (totalHealth <= 0.0f || SimulatedRDamage(enemy) < totalHealth) {
            continue;
        }
        const float maxHealth = enemy.MaxHealth();
        const float healthPercent = maxHealth > 0.0f
            ? totalHealth / maxHealth
            : totalHealth;
        const float distance = Player().Position().Distance2D(enemy.Position());
        if (healthPercent < bestHealthPercent ||
            (healthPercent == bestHealthPercent && distance < bestDistance)) {
            bestTarget = enemy;
            bestHealthPercent = healthPercent;
            bestDistance = distance;
        }
    }

    if (!bestTarget.IsValid() || !R.CastOnUnit(bestTarget)) {
        return false;
    }
    return true;
}

static bool HandleMultiRCombo() {
    if (Orbwalker::ActiveMode() != OrbwalkingMode::Combo ||
        !MultiRComboEnabled()) {
        if (CurrentMultiRStage != MultiRStage::Idle) {
            ResetMultiRState();
        }
        return false;
    }

    if (CurrentMultiRStage == MultiRStage::Idle && !BeginMultiRCombo()) {
        return false;
    }
    if (Game::IsChatOpen()) {
        return true;
    }

    const auto player = Player();
    const auto target = GetActiveMultiRTarget();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() ||
        !target.IsValid()) {
        ResetMultiRState();
        return false;
    }

    const DWORD now = GetTickCount();
    switch (CurrentMultiRStage) {
    case MultiRStage::NeedQ1:
        if (HasQMark(target) || QPhase() == RecastPhase::Second ||
            QRecastPending()) {
            SetMultiRStage(MultiRStage::NeedQ2);
            return true;
        }
        if (Q.IsReady() && IsValidEnemy(target, Q1Range) &&
            MultiRActionReady(now) && CastQ1(target)) {
            SetMultiRStage(MultiRStage::NeedQ2);
            return true;
        }
        if (!Q.IsReady() && MultiRCanUseWard(player)) {
            SetMultiRStage(MultiRStage::NeedWard);
            return true;
        }
        if (!Q.IsReady()) {
            ResetMultiRState();
            return false;
        }
        return true;
    case MultiRStage::NeedQ2:
        if (!HasQMark(target) && QPhase() != RecastPhase::Second &&
            !QRecastPending()) {
            if (Q.IsReady()) {
                SetMultiRStage(MultiRStage::NeedQ1);
                return true;
            }
            ResetMultiRState();
            return false;
        }
        if (Q.IsReady() && MultiRActionReady(now) && CastInsecQ2(target)) {
            MultiRQ2CastTick = now;
            SetMultiRStage(MultiRStage::NeedWard);
            return true;
        }
        return true;
    case MultiRStage::NeedWard: {
        const auto currentPlan = BestMultiRCandidateForTarget(
            target, player.Position());
        if (currentPlan.CollisionCount < MultiRMinCollisions()) {
            if (MultiRQ2CastTick != 0 &&
                now - MultiRQ2CastTick < MultiRQ2DashWindowMs) {
                return true;
            }
            ResetMultiRState();
            return false;
        }
        SetMultiRPlan(currentPlan);
        const float wardRange = std::min(
            WardPlacementRange, W1Range - MultiRWardSafetyMargin);
        if (player.Position().Distance2D(MultiRWardPosition) > wardRange) {
            if (MultiRQ2CastTick != 0 &&
                now - MultiRQ2CastTick < MultiRQ2DashWindowMs) {
                return true;
            }
            ResetMultiRState();
            return false;
        }
        if (MultiRCanUseWard(player) && !WardHopPending &&
            !WardHopCompleted && MultiRActionReady(now) &&
            StartWardHopAt(player, MultiRWardPosition)) {
            SetMultiRStage(MultiRStage::NeedW);
        }
        return true;
    }
    case MultiRStage::NeedW:
        if (WardHopPending && now - WardHopRequestTick > WardHopTimeoutMs) {
            ResetMultiRState();
            return false;
        }
        if (WardHopPending && !FinishWardHop()) {
            return true;
        }
        if (!WardHopCompleted) {
            ResetMultiRState();
            return false;
        }
        MultiRNeedsWConfirm = true;
        SetMultiRStage(MultiRStage::NeedR);
        return true;
    case MultiRStage::NeedR: {
        if (!R.IsReady()) {
            ResetMultiRState();
            return false;
        }
        if (MultiRNeedsWConfirm) {
            const DWORD elapsed = now - LastWCastTick;
            if (elapsed < MultiRW1ToRDelayMs ||
                (WPhase() != RecastPhase::Second &&
                 elapsed < MultiRW1ConfirmTimeoutMs)) {
                return true;
            }
            MultiRNeedsWConfirm = false;
        }
        const auto direct = BuildMultiRDirectCandidate(target, player.Position());
        if (!direct.Direct || direct.CollisionCount < MultiRMinCollisions()) {
            if (now - MultiRStageTick < MultiRW1ConfirmTimeoutMs + 300) {
                return true;
            }
            ResetMultiRState();
            return false;
        }
        if (MultiRActionReady(now) && R.CastOnUnit(target)) {
            ResetMultiRState();
            return true;
        }
        return true;
    }
    case MultiRStage::Idle:
        break;
    }
    return false;
}

static bool Combo() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() ||
        Game::IsChatOpen() || player.Spellbook().IsWindingUp()) {
        return false;
    }

    const DWORD now = GetTickCount();
    if (LastComboTick != 0 && now - LastComboTick < ComboIntervalMs) {
        return false;
    }

    const auto selectedTarget = GetComboTarget(Q2Range);
    const auto markedTarget = MarkedQTarget();
    const auto target = markedTarget.IsValid() ? markedTarget : selectedTarget;
    if (!target.IsValid()) {
        return false;
    }

    if (Q1AwaitingAutoAttack) {
        if (!AutoAttack::InAutoAttackRange(target)) {
            Q1AwaitingAutoAttack = false;
        } else if (now - LastQCastTick < 150 || CurrentPassiveStacks() >= 2) {
            return false;
        } else {
            Q1AwaitingAutoAttack = false;
        }
    }

    if (Q2AwaitingAutoAttack) {
        if (now - LastQCastTick < 150 || CurrentPassiveStacks() >= 2) {
            return false;
        }
        Q2AwaitingAutoAttack = false;
    }

    if (ComboEnabled("useQ") && markedTarget.IsValid() && CastQ2(markedTarget)) {
        LastComboTick = now;
        return true;
    }

    if (ComboEnabled("useW") && CastW2(target)) {
        LastComboTick = now;
        return true;
    }
    if (ComboEnabled("useE") && CastE2(target)) {
        LastComboTick = now;
        return true;
    }

    if (!CanStartNewSkill(target)) {
        return false;
    }

    if (ComboEnabled("useQ") && CastQ1(target)) {
        LastComboTick = now;
        return true;
    }
    if (ComboEnabled("useW") && CastW1(target)) {
        LastComboTick = now;
        return true;
    }
    if (ComboEnabled("useE") && CastE1(target)) {
        LastComboTick = now;
        return true;
    }
    return false;
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    if (!Loaded) {
        return;
    }
    InterlockedExchange(&PassiveStacks, static_cast<LONG>(DetectPassiveStacks()));
    if (HandleInsec()) {
        if (CurrentMultiRStage != MultiRStage::Idle) {
            ResetMultiRState();
        }
        return;
    }
    if (HandleFancyCombo()) {
        if (CurrentMultiRStage != MultiRStage::Idle) {
            ResetMultiRState();
        }
        return;
    }
    if (HandleRAutoKillSteal()) {
        return;
    }
    if (HandleMultiRCombo()) {
        return;
    }
    if (HandleWardHop()) {
        return;
    }
    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo) {
        Combo();
    }
}

struct ComboStep {
    const char* Name = "";
    float Damage = 0.0f;
    float HealthAfter = 0.0f;
};

struct KillComboPlan {
    ComboStep Steps[10] = {};
    int Count = 0;
    float StartHealth = 0.0f;
    float FinalHealth = 0.0f;
    float TotalDamage = 0.0f;
};

static float QBaseDamage() {
    static constexpr float values[5] = { 60.0f, 90.0f, 120.0f, 150.0f, 180.0f };
    const int rank = std::clamp(Q.Level(), 1, 5);
    return values[rank - 1];
}

static float EBaseDamage() {
    static constexpr float values[5] = { 35.0f, 60.0f, 85.0f, 110.0f, 135.0f };
    const int rank = std::clamp(E.Level(), 1, 5);
    return values[rank - 1];
}

static float RBaseDamage() {
    static constexpr float values[3] = { 175.0f, 400.0f, 625.0f };
    const int rank = std::clamp(R.Level(), 1, 3);
    return values[rank - 1];
}

static float SimulatedQDamage(const AIHeroClient& target,
                              bool secondCast,
                              float simulatedHealth) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    Damage::AbilityInput input = {};
    input.BaseDamage = QBaseDamage();
    input.BonusADRatio = 0.90f;
    input.Type = SDK::DamageType::Physical;
    auto raw = Damage::ResolveAbilityRaw(player, target, input);
    if (secondCast) {
        const float maxHealth = target.MaxHealth();
        const float missingHealth = maxHealth > 0.0f
            ? std::clamp((maxHealth - simulatedHealth) / maxHealth, 0.0f, 1.0f)
            : 0.0f;
        raw.Physical *= 1.0f + missingHealth;
    }
    return Damage::Calculate(player, target, raw).TotalDamage;
}

static float SimulatedE1Damage(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    Damage::AbilityInput input = {};
    input.BaseDamage = EBaseDamage();
    input.TotalADRatio = 0.90f;
    input.Type = SDK::DamageType::Magical;
    return Damage::CalculateAbility(player, target, input).TotalDamage;
}

static float SimulatedRDamage(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    Damage::AbilityInput input = {};
    input.BaseDamage = RBaseDamage();
    input.BonusADRatio = 2.0f;
    input.Type = SDK::DamageType::Physical;
    return Damage::CalculateAbility(player, target, input).TotalDamage;
}

static void AddComboStep(KillComboPlan& plan,
                         const char* name,
                         float damage,
                         float& simulatedHealth) {
    if (plan.Count >= static_cast<int>(sizeof(plan.Steps) / sizeof(plan.Steps[0])) ||
        simulatedHealth <= 0.0f) {
        return;
    }

    const float safeDamage = std::max(damage, 0.0f);
    simulatedHealth = std::max(simulatedHealth - safeDamage, 0.0f);
    plan.Steps[plan.Count++] = { name, safeDamage, simulatedHealth };
    plan.TotalDamage += safeDamage;
}

static KillComboPlan BuildKillComboPlan(const AIHeroClient& target) {
    KillComboPlan plan = {};
    if (!IsValidEnemy(target, Q2Range)) {
        return plan;
    }

    const auto player = Player();
    float simulatedHealth = target.Health();
    plan.StartHealth = simulatedHealth;
    const bool targetInAttackRange = AutoAttack::InAutoAttackRange(target);
    bool q2WasCast = false;
    bool q2FollowUpNeedsAttack = false;

    if (ComboEnabled("useQ")) {
        const auto phase = QPhase();
        if (phase == RecastPhase::Second && Q.IsReady() && HasQMark(target) &&
            Q2IsSafeAtHealth(target, simulatedHealth)) {
            const bool q2WasFar = !targetInAttackRange;
            AddComboStep(plan, "Q2", SimulatedQDamage(target, true, simulatedHealth), simulatedHealth);
            q2WasCast = true;
            q2FollowUpNeedsAttack = q2WasFar;
        } else if (phase == RecastPhase::First && Q.IsReady() &&
                   IsValidEnemy(target, Q1Range)) {
            AddComboStep(plan, "Q1", SimulatedQDamage(target, false, simulatedHealth), simulatedHealth);
            if (simulatedHealth > 0.0f && targetInAttackRange) {
                AddComboStep(plan, "AA", Damage::GetAutoAttackDamage(player, target), simulatedHealth);
            }
            if (simulatedHealth > 0.0f &&
                Q2IsSafeAtHealth(target, simulatedHealth)) {
                AddComboStep(plan, "Q2", SimulatedQDamage(target, true, simulatedHealth), simulatedHealth);
                q2WasCast = true;
                q2FollowUpNeedsAttack = !targetInAttackRange;
            }
        }
    }

    if (q2WasCast && q2FollowUpNeedsAttack && simulatedHealth > 0.0f) {
        AddComboStep(plan, "AA", Damage::GetAutoAttackDamage(player, target), simulatedHealth);
    }

    if (simulatedHealth > 0.0f && ComboEnabled("useE") &&
        EPhase() == RecastPhase::First && E.IsReady() &&
        (targetInAttackRange || q2WasCast) && IsValidEnemy(target, E1Range)) {
        AddComboStep(plan, "E1", SimulatedE1Damage(target), simulatedHealth);
        if (simulatedHealth > 0.0f && targetInAttackRange) {
            AddComboStep(plan, "AA", Damage::GetAutoAttackDamage(player, target), simulatedHealth);
        }
    }

    if (simulatedHealth > 0.0f && R.IsReady() &&
        (targetInAttackRange || q2WasCast) && IsValidEnemy(target, RRange)) {
        AddComboStep(plan, "R", SimulatedRDamage(target), simulatedHealth);
    }

    plan.FinalHealth = simulatedHealth;
    return plan;
}

static void DrawKillCombo(const AIHeroClient& target) {
    if (!DrawComboMenu || !DrawComboMenu->Value) {
        return;
    }

    const auto plan = BuildKillComboPlan(target);
    if (plan.Count == 0 || plan.FinalHealth > 0.0f) {
        return;
    }

    Vec2 worldToScreen = {};
    if (!Drawing::WorldToScreen(target.Position(), worldToScreen)) {
        return;
    }

    std::string sequence;
    std::string damageBreakdown;
    for (int i = 0; i < plan.Count; ++i) {
        if (!sequence.empty()) {
            sequence += " -> ";
            damageBreakdown += " + ";
        }
        sequence += plan.Steps[i].Name;
        char damageText[32] = {};
        _snprintf_s(damageText,
                    sizeof(damageText),
                    _TRUNCATE,
                    "%s %.0f",
                    plan.Steps[i].Name,
                    plan.Steps[i].Damage);
        damageBreakdown += damageText;
    }

    char summary[96] = {};
    _snprintf_s(summary,
                sizeof(summary),
                _TRUNCATE,
                "Kill combo: %.0f damage",
                plan.TotalDamage);
    const Vec2 summaryPosition{ worldToScreen.x, worldToScreen.y - 88.0f };
    const Vec2 sequencePosition{ worldToScreen.x, worldToScreen.y - 70.0f };
    const Vec2 detailsPosition{ worldToScreen.x, worldToScreen.y - 52.0f };
    Drawing::DrawText(summaryPosition, summary, 0xFF00FF00, true);
    Drawing::DrawText(sequencePosition, sequence.c_str(), 0xFFFFFFFF, true);
    Drawing::DrawText(detailsPosition, damageBreakdown.c_str(), 0xFFFFFF00, true);
}

static void OnDraw() {
    const auto markedTarget = MarkedQTarget();
    const auto selectedTarget = GetComboTarget(Q2Range);
    const auto target = markedTarget.IsValid() ? markedTarget : selectedTarget;
    if (target.IsValid()) {
        DrawKillCombo(target);
    }

    if (!DrawPassiveMenu || !DrawPassiveMenu->Value) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    Vec2 worldToScreen = {};
    if (!Drawing::WorldToScreen(player.Position(), worldToScreen)) {
        return;
    }
    const Vec2 screenPosition(worldToScreen.x, worldToScreen.y - 40.0f);

    const int passiveStacks = CurrentPassiveStacks();
    char text[48] = {};
    _snprintf_s(text, sizeof(text), _TRUNCATE, "Flurry: %d/2", passiveStacks);
    const uint32_t color = passiveStacks >= 2
        ? 0xFF00FF00
        : (passiveStacks == 1 ? 0xFFFFFF00 : 0xFFFFFFFF);
    const Vec2 shadowPosition{ screenPosition.x + 1.0f, screenPosition.y + 1.0f };
    Drawing::DrawText(shadowPosition, text, 0xE0000000, true);
    Drawing::DrawText(screenPosition, text, color, true);
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.ziblldev9898", "Lee Sin", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q", true));
    ComboMenu->Add(new MenuBool("useW", "Use W", true));
    ComboMenu->Add(new MenuBool("useE", "Use E", true));
    ComboMenu->Add(new MenuBool("drawPassive", "Draw Passive Stacks", true));
    ComboMenu->Add(new MenuBool("drawCombo", "Draw Kill Combo", true));
    WardHopEnabledMenu = ComboMenu->Add(new MenuBool("wardHop", "Use Ward Hop", true));
    WardHopKeyMenu = ComboMenu->Add(new MenuKeyBind(
        "wardHopKey", "Ward Hop Key", SDK::Keys::Z, SDK::KeyBindType::Press));
    InsecKeyMenu = ComboMenu->Add(new MenuKeyBind(
        "insecKey", "Insec Key", SDK::Keys::A, SDK::KeyBindType::Press));
    FancyComboMenu = ComboMenu->Add(new MenuBool(
        "fancyCombo", "Fancy Combo", false));
    FancyQ2HealthMenu = ComboMenu->Add(new MenuSlider(
        "fancyQ2Health", "Q2 Health Threshold (%)", 30, 0, 100));
    FancyQ2EnemiesMenu = ComboMenu->Add(new MenuSlider(
        "fancyQ2Enemies", "Q2 Max Nearby Enemies", 1, 0, 5));
    MultiRComboMenu = ComboMenu->Add(new MenuBool(
        "multiRCombo", "Multi-Enemy R Kick", false));
    MultiRMinCollisionsMenu = ComboMenu->Add(new MenuSlider(
        "multiRMinCollisions", "Minimum R Collisions", 2, 2, 5));

    KillStealMenu = MenuRoot->AddSubMenu(new Menu("KillSteal Settings", "KillSteal"));
    RAutoKillStealMenu = KillStealMenu->Add(new MenuBool(
        "rAutoKillSteal", "R Auto KS", false));

    DrawPassiveMenu = ComboMenu->Get<MenuBool>("drawPassive");
    DrawComboMenu = ComboMenu->Get<MenuBool>("drawCombo");
    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (Loaded || !player.IsValid()) {
        return;
    }

    Q = Spell(SpellSlot::Q, Q1Range);
    Q.SetSkillshot(0.25f, 60.0f, 1800.0f, true, SpellType::Line);
    Q.Range = Q2Range;

    W = Spell(SpellSlot::W, W1Range);

    E = Spell(SpellSlot::E, E1Range);
    E.Delay = 0.25f;

    R = Spell(SpellSlot::R, RRange);
    R.Delay = 0.25f;

    BuildMenu();

    DrawHooked = (Drawing::OnDraw += &OnDraw);
    UpdateHooked = (Events::hook.OnGameUpdate += &Game_OnUpdate);

    InterlockedExchange(&PassiveStacks, static_cast<LONG>(DetectPassiveStacks()));
    QMarkedTargetNetworkId = 0;
    Q1AwaitingAutoAttack = false;
    Q2AwaitingAutoAttack = false;
    QRecastExpireTick = 0;
    WardHopPending = false;
    WardHopKeyWasActive = false;
    WardHopCompleted = false;
    WardHopPosition = {};
    WardHopRequestTick = 0;
    WardHopLastAttemptTick = 0;
    WardHopRetryUntilTick = 0;
    ResetInsecState();
    ResetFancyComboState();
    ResetMultiRState();
    LastQCastPhase = RecastPhase::Unknown;
    LastWCastPhase = RecastPhase::Unknown;
    LastECastPhase = RecastPhase::Unknown;
    LastQCastTick = 0;
    LastWCastTick = 0;
    LastECastTick = 0;
    LastComboTick = 0;
    Loaded = true;
    Game::Print("<font color='#8ec5ff' size='20'>ziblldev9898 - Lee Sin loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    if (DrawHooked) {
        Drawing::OnDraw -= &OnDraw;
    }
    if (UpdateHooked) {
        Events::hook.OnGameUpdate -= &Game_OnUpdate;
    }
    if (MenuRoot) {
        MenuManager::Instance().Remove(MenuRoot);
        delete MenuRoot;
    }

    MenuRoot = nullptr;
    ComboMenu = nullptr;
    DrawPassiveMenu = nullptr;
    DrawComboMenu = nullptr;
    WardHopEnabledMenu = nullptr;
    WardHopKeyMenu = nullptr;
    InsecKeyMenu = nullptr;
    FancyComboMenu = nullptr;
    FancyQ2HealthMenu = nullptr;
    FancyQ2EnemiesMenu = nullptr;
    MultiRComboMenu = nullptr;
    MultiRMinCollisionsMenu = nullptr;
    KillStealMenu = nullptr;
    RAutoKillStealMenu = nullptr;
    ResetInsecState();
    ResetFancyComboState();
    ResetMultiRState();
    DrawHooked = false;
    UpdateHooked = false;
    InterlockedExchange(&PassiveStacks, 0);
    QMarkedTargetNetworkId = 0;
    Q1AwaitingAutoAttack = false;
    Q2AwaitingAutoAttack = false;
    QRecastExpireTick = 0;
    WardHopPending = false;
    WardHopKeyWasActive = false;
    WardHopCompleted = false;
    WardHopPosition = {};
    WardHopRequestTick = 0;
    WardHopLastAttemptTick = 0;
    WardHopRetryUntilTick = 0;
    LastQCastPhase = RecastPhase::Unknown;
    LastWCastPhase = RecastPhase::Unknown;
    LastECastPhase = RecastPhase::Unknown;
    LastQCastTick = 0;
    LastWCastTick = 0;
    LastECastTick = 0;
    LastComboTick = 0;
    Loaded = false;
}

}
