#pragma once

#include "../../../SDK/SDK.h"
#include "AioMenu.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::ziblldev9898::Irelia {

using SDK::Core::Utils::AutoAttack;

// ============================================================================
// Menu pointers
// ============================================================================
inline constexpr const char* ComboMenu = "irelia.combo";
inline constexpr const char* HarassMenu = "irelia.harass";
inline constexpr const char* LaneClearMenu = "irelia.lane";
inline constexpr const char* JungleClearMenu = "irelia.jungle";
inline constexpr const char* KillStealMenu = "irelia.killSteal";

// ============================================================================
// Spell instances (CDragon irelia.bin.json verified 2026-07-09)
// ============================================================================
// Q: CastRange=600, CD=10/10/9/8/7/6/5, Mana=15, Dash (target unit/position)
// W: CastRange=775 (display 825), CD=20/20/18/16/14/12/10, Mana=70-90, Channel+Recast
// E: CastRange=850 (display), CD=16/16/14.5/13/11.5/10/10, Mana=50, Location 2-cast
// R: CastRange=950 (display), CD=125/125/105/85, Mana=100, Line width=160, speed=2000
inline Spell Q{ SpellSlot::Q, 600.0f };
inline Spell W{ SpellSlot::W, 825.0f };
inline Spell E{ SpellSlot::E, 850.0f };
inline Spell R{ SpellSlot::R, 950.0f };

// ============================================================================
// State / tick tracking
// ============================================================================
inline bool Loaded = false;
inline DWORD LastComboEvalTick = 0;
inline DWORD LastHarassEvalTick = 0;
inline DWORD LastLaneClearEvalTick = 0;
inline DWORD LastJungleClearEvalTick = 0;
inline uintptr_t QFarmStyleLastTargetAddress = 0;
inline Vec3 QFarmStylePreviousPosition = {};
inline Vec3 QFarmStyleLastPosition = {};
inline DWORD QFarmStyleLastCastTick = 0;
inline int QFarmStyleDirection = 1;

// --- Passive tracking ---
inline int PassiveStacks = 0;

// ============================================================================
// Helpers
// ============================================================================
static AIHeroClient Player() {
    return ObjectManager::Player();
}

static bool Bool(const char* section, const char* key, bool fallback = true) {
    return AioMenu::Bool(section, key, fallback);
}

static int Slider(const char* section, const char* key, int fallback = 0) {
    return AioMenu::Slider(section, key, fallback);
}

static bool ShouldRunNow(DWORD& lastTick, DWORD intervalMs) {
    const DWORD now = GetTickCount();
    if (lastTick != 0 && now - lastTick < intervalMs) return false;
    lastTick = now;
    return true;
}

static bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f;
}

static bool ValidTarget(const AIBaseClient& unit, float range = FLT_MAX) {
    return ValidUnit(unit) && Extensions::IsValidTarget(unit, range, true);
}

static bool ValidHeroTarget(const AIHeroClient& hero, float range = FLT_MAX) {
    return ValidUnit(hero) && Extensions::IsValidTarget(hero, range, true);
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

// ============================================================================
// Passive: Ionian Fervor
// Buff: IreliaPassiveStacks (max 4, duration 6s)
// Each stack is a separate buff entry, so we count entries instead of GetStacks.
// ============================================================================

static int DetectPassiveStacks() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return 0;
    }

    uintptr_t buffs[256] = {};
    const int count = CoreBuffs::Enumerate(player.Address(), buffs, 256);
    const float gameTime = CoreBuffs::ResolveGameTime();
    int matchingEntries = 0;
    int fallbackStacks = 0;
    int counterStacks = 0;
    char name[96] = {};

    for (int i = 0; i < count; ++i) {
        CoreBuffs::BuffRef buff{ buffs[i] };
        if (!buff.IsActive(gameTime) ||
            !buff.ReadName(name, static_cast<int>(sizeof(name))) ||
            !CoreBuffs::NameMatchesQuery(name, "IreliaPassiveStacks")) {
            continue;
        }

        ++matchingEntries;

        const int raw38 = Globals::Read<int>(
            buff.address + Offset::BuffDataLayout::BuffStacks);
        const int raw3C = Globals::Read<int>(
            buff.address + Offset::BuffDataLayout::BuffStacksAlt);
        fallbackStacks = std::max(fallbackStacks, std::max(raw38, raw3C));

        const int current = buff.GetCounterCurrent();
        const int maximum = buff.GetCounterMax();
        if (current >= 0 && current <= 4 &&
            (maximum == 0 || maximum == 4 ||
             (maximum > 0 && maximum <= 4 && current <= maximum))) {
            counterStacks = std::max(counterStacks, current);
        }
    }

    const int detectedStacks = std::max(
        matchingEntries,
        std::max(fallbackStacks, counterStacks));
    return std::clamp(detectedStacks, 0, 4);
}

static void UpdatePassiveState(int hintedStacks = -1) {
    const auto player = Player();
    if (!player.IsValid()) {
        PassiveStacks = 0;
        return;
    }

    PassiveStacks = DetectPassiveStacks();
    if (hintedStacks >= 0 && hintedStacks <= 4) {
        PassiveStacks = std::max(PassiveStacks, hintedStacks);
    }
}

// ============================================================================
// Passive buff change handling
// ============================================================================
static void OnBuffChanged(const Events::BuffEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) return;
    if (args.Sender.NetworkId != player.NetworkId()) return;

    bool isPassiveBuff = _stricmp(args.BuffName, "IreliaPassiveStacks") == 0;
    if (!isPassiveBuff && args.BuffAddress != 0) {
        CoreBuffs::BuffRef buff{ args.BuffAddress };
        char name[96] = {};
        isPassiveBuff = buff.ReadName(name, static_cast<int>(sizeof(name))) &&
            CoreBuffs::NameMatchesQuery(name, "IreliaPassiveStacks");
    }

    if (isPassiveBuff) {
        UpdatePassiveState(std::clamp(args.Count, 0, 4));
    }
}

// ============================================================================
// Damage calculations
// ============================================================================
static double QDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0;
    const float sdkDamage = Q.GetDamage(target);
    if (sdkDamage > 0.0f) return sdkDamage;
    return player.GetSpellDamage(target, SpellSlot::Q);
}

static float QFarmOnHitDamage(const AIHeroClient& player, const AIBaseClient& target) {
    float onHitDamage = Damage::GetAutoAttackDamage(player, target, true) -
        Damage::GetAutoAttackDamage(player, target, false);

    if (PassiveStacks >= 4) {
        const int level = std::clamp(player.Level(), 1, 18);
        const float cdragonPassiveRawDamage =
            10.0f + 3.0f * static_cast<float>(level - 1) +
            0.20f * player.BonusAttackDamage();
        const float cdragonPassiveDamage =
            player.CalculateMagicDamage(target, cdragonPassiveRawDamage);

        if (player.GetBuffCount("ireliapassivestacks") >= 4) {
            const float legacyPassiveRawDamage =
                10.0f + 3.0f * static_cast<float>(level - 1) +
                0.25f * player.BonusAttackDamage();
            onHitDamage -= player.CalculateMagicDamage(
                target, legacyPassiveRawDamage);
        }
        onHitDamage += cdragonPassiveDamage;
    }

    return std::max(onHitDamage, 0.0f);
}

static double QFarmDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0;

    static constexpr float qBaseDamage[6] = {
        0.0f, 5.0f, 25.0f, 45.0f, 65.0f, 85.0f
    };
    static constexpr float qMinionLevelBonus[18] = {
        50.0f, 61.0f, 72.0f, 83.0f, 94.0f, 105.0f,
        116.0f, 127.0f, 138.0f, 149.0f, 160.0f, 171.0f,
        182.0f, 193.0f, 204.0f, 215.0f, 226.0f, 237.0f
    };

    const int qRank = std::clamp(Q.Level(), 1, 5);
    const int championLevel = std::clamp(player.Level(), 1, 18);
    const float rawDamage = qBaseDamage[qRank] +
        qMinionLevelBonus[championLevel - 1] +
        0.70f * player.TotalAttackDamage();
    return player.CalculatePhysicalDamage(target, rawDamage) +
        QFarmOnHitDamage(player, target);
}

static bool QFarmCanKill(const AIBaseClient& target) {
    if (!target.IsValid() || target.IsDead()) return false;
    const float effectiveHealth = target.Health() + target.AllShield();
    return QFarmDamage(target) > effectiveHealth + 1.0f;
}

static bool IsLargeJungleMonster(const AIMinionClient& mob) {
    const auto type = mob.GetJungleType();
    return type == JungleType::Large ||
           type == JungleType::Epic ||
           type == JungleType::Legendary;
}

static int LaneFarmPriority(const AIMinionClient& minion) {
    const MinionTypes type = minion.GetMinionType();
    if (HasFlag(type, MinionTypes::Super)) return 4000;
    if (HasFlag(type, MinionTypes::Siege)) return 3000;
    if (HasFlag(type, MinionTypes::Melee)) return 2000;
    if (HasFlag(type, MinionTypes::Ranged)) return 1000;
    return 0;
}

static bool IsLaneFarmMinion(const AIMinionClient& minion) {
    return ValidTarget(minion, Q.Range) &&
           !minion.IsJungle() &&
           !minion.IsPlant() &&
           !minion.IsPet() &&
           !minion.IsClone();
}

static bool IsJungleFarmMonster(const AIMinionClient& mob) {
    return ValidTarget(mob, Q.Range) &&
           mob.IsJungle() &&
           !mob.IsPlant() &&
           !mob.IsPet() &&
           !mob.IsClone();
}

static bool HasMark(const AIBaseClient& target) {
    if (!target.IsValid()) return false;
    return target.HasBuff("IreliaMark");
}

static bool CastQFarmTarget(const AIMinionClient& target) {
    if (!Q.IsReady() || !ValidTarget(target, Q.Range)) return false;
    return Q.Cast(target) == CastStates::SuccessfullyCasted;
}

static void ResetQFarmStyle() {
    QFarmStyleLastTargetAddress = 0;
    QFarmStylePreviousPosition = {};
    QFarmStyleLastPosition = {};
    QFarmStyleLastCastTick = 0;
    QFarmStyleDirection = 1;
}

static float QFarmStyleAngleDelta(float from, float to) {
    constexpr float pi = 3.14159265358979323846f;
    constexpr float twoPi = pi * 2.0f;
    float delta = to - from;
    while (delta > pi) delta -= twoPi;
    while (delta < -pi) delta += twoPi;
    return delta;
}

static bool QFarmPreviousTargetResolved() {
    if (QFarmStyleLastTargetAddress == 0) return true;

    const AIMinionClient previousTarget(QFarmStyleLastTargetAddress);
    if (previousTarget.IsValid() &&
        !previousTarget.IsDead() &&
        previousTarget.Health() > 0.0f) {
        return false;
    }

    QFarmStyleLastTargetAddress = 0;
    return true;
}

static bool SelectStyledLaneTarget(
    const std::vector<AIMinionClient>& candidates,
    AIMinionClient& selected) {
    if (candidates.empty()) return false;

    const auto player = Player();
    if (!player.IsValid()) return false;

    const Vec3 playerPosition = player.Position();
    float centerX = 0.0f;
    float centerZ = 0.0f;
    for (const auto& minion : candidates) {
        centerX += minion.Position().x;
        centerZ += minion.Position().z;
    }
    centerX /= static_cast<float>(candidates.size());
    centerZ /= static_cast<float>(candidates.size());

    float forwardX = centerX - playerPosition.x;
    float forwardZ = centerZ - playerPosition.z;
    float forwardLength = std::hypot(forwardX, forwardZ);

    const float previousX = QFarmStyleLastPosition.x - QFarmStylePreviousPosition.x;
    const float previousZ = QFarmStyleLastPosition.z - QFarmStylePreviousPosition.z;
    const float previousLength = std::hypot(previousX, previousZ);
    const bool hasPreviousDash = previousLength > 1.0f;

    if (forwardLength <= 1.0f && hasPreviousDash) {
        forwardX = previousX;
        forwardZ = previousZ;
        forwardLength = previousLength;
    }
    if (forwardLength <= 1.0f) {
        forwardX = 1.0f;
        forwardZ = 0.0f;
        forwardLength = 1.0f;
    }

    forwardX /= forwardLength;
    forwardZ /= forwardLength;
    const float rightX = -forwardZ;
    const float rightZ = forwardX;
    const float previousAngle = std::atan2(previousZ, previousX);
    const bool hasAlternative = candidates.size() > 1;

    bool hasMeaningfulDash = false;
    for (const auto& minion : candidates) {
        if (hasAlternative && minion.Address() == QFarmStyleLastTargetAddress) {
            continue;
        }
        if (playerPosition.Distance2D(minion.Position()) >= 90.0f) {
            hasMeaningfulDash = true;
            break;
        }
    }

    float bestScore = -FLT_MAX;
    bool found = false;
    for (const auto& minion : candidates) {
        if (hasAlternative && minion.Address() == QFarmStyleLastTargetAddress) {
            continue;
        }

        const Vec3 minionPosition = minion.Position();
        const float offsetX = minionPosition.x - playerPosition.x;
        const float offsetZ = minionPosition.z - playerPosition.z;
        const float distance = std::hypot(offsetX, offsetZ);
        if (distance <= 1.0f) continue;
        if (hasMeaningfulDash && distance < 90.0f) continue;

        const float lateral = offsetX * rightX + offsetZ * rightZ;
        const float desiredLateral = QFarmStyleDirection > 0 ? lateral : -lateral;
        float score = desiredLateral * 4.0f;
        if (desiredLateral >= 0.0f) score += 700.0f;
        score += std::min(distance, Q.Range) * 0.75f;
        score += static_cast<float>(LaneFarmPriority(minion)) * 0.001f;

        const float angle = std::atan2(offsetZ, offsetX);
        if (hasPreviousDash) {
            const float alignment =
                (offsetX * previousX + offsetZ * previousZ) / (distance * previousLength);
            const float turn = QFarmStyleDirection > 0
                ? QFarmStyleAngleDelta(previousAngle, angle)
                : QFarmStyleAngleDelta(angle, previousAngle);
            score -= alignment * 350.0f;
            score += turn * 120.0f;
        } else {
            score += (QFarmStyleDirection > 0 ? angle : -angle) * 20.0f;
        }

        if (!found || score > bestScore) {
            bestScore = score;
            selected = minion;
            found = true;
        }
    }

    return found;
}

static bool LaneClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        ResetQFarmStyle();
        return false;
    }

    if (!Bool(LaneClearMenu, "useQ") ||
        player.ManaPercent() < static_cast<float>(Slider(LaneClearMenu, "ManaLC", 30))) {
        ResetQFarmStyle();
        return false;
    }

    if (QFarmStyleLastCastTick != 0 &&
        GetTickCount() - QFarmStyleLastCastTick > 1500) {
        ResetQFarmStyle();
    }

    if (!Q.IsReady()) {
        return false;
    }

    if (QFarmStyleLastCastTick != 0 &&
        GetTickCount() - QFarmStyleLastCastTick < 100) {
        return false;
    }

    if (!QFarmPreviousTargetResolved()) {
        return false;
    }

    if (!ShouldRunNow(LastLaneClearEvalTick, 120)) {
        return false;
    }

    auto minions = GameObjects::EnemyLaneMinions();
    if (minions.empty()) minions = GameObjects::EnemyMinions();

    minions.erase(
        std::remove_if(
            minions.begin(),
            minions.end(),
            [](const AIMinionClient& minion) {
                return !IsLaneFarmMinion(minion);
            }),
        minions.end());

    std::vector<AIMinionClient> candidates;
    candidates.reserve(minions.size());
    for (const auto& minion : minions) {
        if (QFarmCanKill(minion)) candidates.push_back(minion);
    }

    if (candidates.empty()) {
        ResetQFarmStyle();
        return false;
    }

    AIMinionClient target;
    if (!SelectStyledLaneTarget(candidates, target) || !QFarmCanKill(target)) {
        return false;
    }

    const Vec3 sourcePosition = player.Position();
    if (!CastQFarmTarget(target)) return false;

    QFarmStylePreviousPosition = sourcePosition;
    QFarmStyleLastPosition = target.Position();
    QFarmStyleLastTargetAddress = target.Address();
    QFarmStyleLastCastTick = GetTickCount();
    QFarmStyleDirection *= -1;
    LastLaneClearEvalTick = 0;
    return true;
}

static int JungleFarmPriority(const AIMinionClient& mob) {
    const auto type = mob.GetJungleType();
    if (type == JungleType::Legendary) return 5000;
    if (type == JungleType::Epic) return 4000;
    if (type == JungleType::Large) return 3000;
    return 1000;
}

static bool JungleClear() {
    const auto player = Player();
    if (!player.IsValid() || !Bool(JungleClearMenu, "useQ") ||
        player.ManaPercent() < static_cast<float>(Slider(JungleClearMenu, "ManaJC", 30)) ||
        !Q.IsReady() || !ShouldRunNow(LastJungleClearEvalTick, 120)) {
        return false;
    }

    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& mob) {
                return !IsJungleFarmMonster(mob);
            }),
        mobs.end());

    std::sort(
        mobs.begin(),
        mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) {
            const bool aMarkedLarge = IsLargeJungleMonster(a) && HasMark(a);
            const bool bMarkedLarge = IsLargeJungleMonster(b) && HasMark(b);
            if (aMarkedLarge != bMarkedLarge) return aMarkedLarge;

            const bool aKillable = QFarmCanKill(a);
            const bool bKillable = QFarmCanKill(b);
            if (aKillable != bKillable) return aKillable;

            const int aPriority = JungleFarmPriority(a);
            const int bPriority = JungleFarmPriority(b);
            if (aPriority != bPriority) return aPriority > bPriority;
            return a.DistanceToPlayer() < b.DistanceToPlayer();
        });

    for (const auto& mob : mobs) {
        const bool markedLarge = IsLargeJungleMonster(mob) && HasMark(mob);
        if ((markedLarge || QFarmCanKill(mob)) && CastQFarmTarget(mob)) {
            return true;
        }
    }
    return false;
}

static double EDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const float sdkDamage = E.GetDamage(target);
    if (sdkDamage > 0.0f) return sdkDamage;
    return player.GetSpellDamage(target, SpellSlot::E);
}

static double RDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const float sdkDamage = R.GetDamage(target);
    if (sdkDamage > 0.0f) return sdkDamage;
    return player.GetSpellDamage(target, SpellSlot::R);
}

// ============================================================================
// Combo damage
// ============================================================================
static double GetComboDamage(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0;

    double damage = Damage::GetAutoAttackDamage(player, target);
    if (Q.IsReady()) damage += QDamage(target);
    if (E.IsReady()) {
        damage += EDamage(target);
        damage += Damage::GetAutoAttackDamage(player, target); // Q reset after E mark
    }
    if (R.IsReady()) damage += RDamage(target);
    return damage;
}

static bool HasMaxPassive() {
    return PassiveStacks >= 4;
}

// ============================================================================
// Forward declarations
// ============================================================================
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnUnload();

// ============================================================================
// OnGameLoad
// ============================================================================
static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) return;

    // CDragon: Q=600 dash, W=825 channel+recast, E=850 location 2-cast, R=950 line
    Q = Spell(SpellSlot::Q, 600.0f);
    W = Spell(SpellSlot::W, 825.0f);
    E = Spell(SpellSlot::E, 850.0f);
    R = Spell(SpellSlot::R, 950.0f);
    R.SetSkillshot(0.25f, 160.0f, 2000.0f, false, SpellType::Line);

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnBuffAdd += &OnBuffChanged;
    Events::hook.OnBuffUpdate += &OnBuffChanged;
    Events::hook.OnBuffRemove += &OnBuffChanged;

    Loaded = true;
    Game::Print("<font color='#8ec5ff' size='20'>ziblldev9898 - Irelia loaded</font>");
}

// ============================================================================
// Game_OnUpdate
// ============================================================================
static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid()) {
        ResetQFarmStyle();
        return;
    }
    if (player.IsDead() || player.IsRecalling()) {
        ResetQFarmStyle();
        return;
    }

    UpdatePassiveState();

    if (Game::IsChatOpen()) return;
    if (player.Spellbook().IsWindingUp()) return;

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::LaneClear:
        if (!LaneClear()) JungleClear();
        break;
    default:
        ResetQFarmStyle();
        break;
    }
}

// ============================================================================
// OnUnload
// ============================================================================
static void OnUnload() {
    if (!Loaded) return;

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnBuffAdd -= &OnBuffChanged;
    Events::hook.OnBuffUpdate -= &OnBuffChanged;
    Events::hook.OnBuffRemove -= &OnBuffChanged;

    Loaded = false;
    PassiveStacks = 0;
    ResetQFarmStyle();
}

} // namespace Plugins::ziblldev9898::Irelia
