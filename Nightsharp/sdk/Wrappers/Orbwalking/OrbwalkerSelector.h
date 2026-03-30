#pragma once

#include "../../Enumerations/OrbwalkerMode.h"
#include "../../Enumerations/DamageType.h"
#include "../../Core/Objects.h"
#include "../../Core/Game.h"
#include "../TargetSelector/TargetSelector.h"
#include "../Damages/Damage.h"
#include "../../Math/Prediction/Health.h"
#include "../../../core/CrashTelemetry.h"

#include <algorithm>
#include <cfloat>
#include <cstdlib>
#include <string>
#include <vector>

namespace SDK::OrbwalkerSelector {

// ── Constants ──
constexpr float LaneClearWaitTime = 2.0f;

// ── Special names (matching C#) ──
inline bool IsSpecialMinion(const std::string& name) {
    static const char* specials[] = {
        "annietibbers", "elisespiderling", "heimertyellow", "heimertblue",
        "ivernminion", "malzaharvoidling", "shacobox", "teemomushroom",
        "yorickghoulmelee", "yorickbigghoul", "zyrathornplant", "zyragraspingplant"
    };
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const auto& s : specials) {
        if (lower == s) return true;
    }
    return false;
}

inline bool IsClone(const std::string& name) {
    static const char* clones[] = { "leblanc", "monkeyking", "neeko", "shaco" };
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const auto& c : clones) {
        if (lower == c) return true;
    }
    return false;
}

inline bool IsIgnored(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "jarvanivstandard";
}

// ── Helpers ──
inline float GetAADamage(const AIHeroClient& player, const AIBaseClient& target) {
    float damage = 0.0f;
    __try {
        damage = Damage::GetAutoAttackDamage(player, target, true);
    }
    __except (1) {
        damage = 0.0f;
    }
    // Fallback: if damage pipeline fails, use simple AD vs Armor calc
    if (damage <= 0.0f && player.TotalAttackDamage() > 0.0f) {
        damage = player.GetAutoAttackDamage(target, false);
    }
    return damage;
}

inline float GetHealthPrediction(const AIBaseClient& target, int time, int farmDelay = 0) {
    return Prediction::Health::GetPrediction(target, time, farmDelay);
}

inline AIBaseClient GetNearestEnemyTarget(const AIHeroClient& player, float range) {
    AIBaseClient best;
    float bestDist = FLT_MAX;
    for (const auto& enemy : ObjectManager::EnemyHeroes()) {
        if (!enemy.IsValidTarget(range, player.Position())) continue;
        const float dist = enemy.Distance(player.Position());
        if (dist < bestDist) {
            bestDist = dist;
            best = enemy;
        }
    }
    return best;
}

inline AIBaseClient GetTargetSelectorSafe(const AIHeroClient& player, float range) {
    __try {
        return TargetSelector::GetTarget(range, DamageType::Physical, player.Position());
    }
    __except (CrashTelemetry::ReportAndHandle("OrbwalkerSelector::TargetSelector", GetExceptionInformation())) {
        return AIBaseClient();
    }
}

// ── Combo (TargetSelector) ──
inline AIBaseClient GetComboTarget(const AIHeroClient& player, float range) {
    // Attack special minions if no enemies near
    for (const auto& minion : ObjectManager::EnemyMinions()) {
        if (!minion.IsValidTarget(range, player.Position())) continue;
        if (IsSpecialMinion(minion.CharacterName())) {
            bool hasEnemyNear = false;
            for (const auto& enemy : ObjectManager::EnemyHeroes()) {
                if (enemy.IsValidTarget(enemy.AttackRange() * 2.0f + 200.0f, player.Position())) {
                    hasEnemyNear = true;
                    break;
                }
            }
            if (!hasEnemyNear) return minion;
        }
    }

    auto ts = GetTargetSelectorSafe(player, range);
    if (ts.IsValid()) {
        return ts;
    }
    return GetNearestEnemyTarget(player, range);
}

// ── LastHit ──
inline AIBaseClient GetLastHitTarget(const AIHeroClient& player, float range, int farmDelay = 0) {
    AIBaseClient best;
    float bestHealth = FLT_MAX;

    const auto minions = ObjectManager::EnemyMinions();

    // Diagnostic (throttled)
    static DWORD s_lastMinDiag = 0;
    const DWORD minDiagNow = GetTickCount();
    if ((minDiagNow - s_lastMinDiag) > 5000 && !minions.empty()) {
        s_lastMinDiag = minDiagNow;
        char buf[512] = {};
        std::snprintf(buf, sizeof(buf),
            "[NightSharp][FarmDiag] GetLastHit: minionCount=%d range=%.0f playerAD=%.0f\r\n",
            (int)minions.size(), range, player.TotalAttackDamage());
        CoreControl::AppendIssueOrderDebug(buf);
        int logged = 0;
        for (const auto& m : minions) {
            if (logged >= 5) break;
            if (!m.IsValidTarget(range, player.Position())) { ++logged; continue; }
            const float hp = m.Health();
            const float aaDmg = GetAADamage(player, m);
            const bool killable = (hp > 0.0f && hp <= aaDmg);
            std::snprintf(buf, sizeof(buf),
                "[NightSharp][FarmDiag]   minion=0x%llX hp=%.0f maxHP=%.0f aaDmg=%.0f killable=%d inRange=%d\r\n",
                (unsigned long long)m.Address(),
                hp, m.MaxHealth(), aaDmg,
                killable ? 1 : 0,
                player.InAutoAttackRange(m) ? 1 : 0);
            CoreControl::AppendIssueOrderDebug(buf);
            ++logged;
        }
    }

    for (const auto& minion : minions) {
        if (!minion.IsValidTarget(range, player.Position())) continue;
        if (IsIgnored(minion.CharacterName())) continue;

        const float health = minion.Health();
        const float aaDmg = GetAADamage(player, minion);

        // Direct kill
        if (health > 0.0f && health <= aaDmg && health < bestHealth) {
            bestHealth = health;
            best = minion;
            continue;
        }

        // Health prediction kill
        if (minion.MaxHealth() > 10.0f) {
            const float timeToHit = static_cast<float>(minion.GetTimeToHit());
            const float predHP = GetHealthPrediction(minion, static_cast<int>(timeToHit), farmDelay);
            if (predHP > 0.0f && predHP < aaDmg && health < bestHealth) {
                bestHealth = health;
                best = minion;
            }
        } else if (health <= 1.0f && health < bestHealth) {
            bestHealth = health;
            best = minion;
        }
    }

    return best;
}

// ── Harass = LastHit + TargetSelector ──
inline AIBaseClient GetHarassTarget(const AIHeroClient& player, float range, int farmDelay = 0,
                                     bool prioritizeFarm = true) {
    // Killable minion first
    AIBaseClient killable = GetLastHitTarget(player, range, farmDelay);
    if (killable.IsValid()) return killable;

    // Then champion target
    if (!prioritizeFarm) {
        auto tsTarget = GetTargetSelectorSafe(player, range);
        if (tsTarget.IsValid() && player.InAutoAttackRange(tsTarget))
            return tsTarget;
    }

    // Champion target anyway
    auto tsTarget = GetTargetSelectorSafe(player, range);
    if (tsTarget.IsValid() && player.InAutoAttackRange(tsTarget))
        return tsTarget;

    auto nearTarget = GetNearestEnemyTarget(player, range);
    if (nearTarget.IsValid() && player.InAutoAttackRange(nearTarget))
        return nearTarget;

    // Jungle minions
    for (const auto& jungle : ObjectManager::JungleMinions()) {
        if (jungle.IsValidTarget(range, player.Position())) return jungle;
    }

    return AIBaseClient();
}

// ── ShouldWait – wait if any minion will be killable soon ──
// Since HealthPrediction requires SpellCastTracker (broken), use heuristic:
// Count allied minions attacking the target, estimate DPS, predict HP.
inline bool ShouldWait(const AIHeroClient& player, float range, int farmDelay = 0) {
    const float aaDmgPlayer = GetAADamage(player, AIBaseClient()); // will be recalc per minion
    const float atkDelayMs = CoreAPI::Control::GetAttackDelay() * 1000.0f;
    const float waitWindowMs = atkDelayMs * LaneClearWaitTime;
    const float allyMinionDps = 12.0f; // ~12 damage per second per allied minion (rough average)

    const auto allyMinions = ObjectManager::AllyMinions();

    for (const auto& minion : ObjectManager::EnemyMinions()) {
        if (!minion.IsValidTarget(range, player.Position())) continue;
        if (IsIgnored(minion.CharacterName())) continue;

        const float aaDmg = GetAADamage(player, minion);
        if (aaDmg <= 0.0f) continue;

        const float hp = minion.Health();

        // Already killable → no need to wait (should last hit NOW)
        if (hp <= aaDmg) continue;

        // Count allied minions near this enemy minion (attacking it)
        int allyAttacking = 0;
        for (const auto& ally : allyMinions) {
            if (ally.Distance(minion.Position()) < 600.0f) {
                allyAttacking++;
            }
        }

        // Predict HP after wait window
        const float estimatedDamage = (float)allyAttacking * allyMinionDps * (waitWindowMs / 1000.0f);
        const float predictedHP = hp - estimatedDamage;

        // If minion will be killable within the wait window, should wait
        if (predictedHP > 0.0f && predictedHP <= aaDmg) {
            return true;
        }
    }
    return false;
}

// ── LaneClear ──
inline AIBaseClient GetLaneClearTarget(const AIHeroClient& player, float range, int farmDelay = 0,
                                        bool prioritizeFarm = true, bool prioritizeMinions = false,
                                        bool prioritizeSmallJungle = false) {
    // 1. Killable minion (same as LastHit)
    AIBaseClient killable = GetLastHitTarget(player, range, farmDelay);
    if (killable.IsValid()) return killable;

    // Forced target (handled by caller)

    // 2. Turrets / Inhib / Nexus (if not prioritizeMinions or no minions)
    if (!prioritizeMinions) {
        for (const auto& turret : ObjectManager::EnemyTurrets()) {
            if (turret.IsValidTarget(range, player.Position()) && player.InAutoAttackRange(turret))
                return turret;
        }
    }

    // 3. Champion via TS
    if (!prioritizeFarm) {
        auto tsTarget = GetTargetSelectorSafe(player, range);
        if (tsTarget.IsValid() && player.InAutoAttackRange(tsTarget))
            return tsTarget;
    } else {
        // Still attack heroes when no farm target
        auto tsTarget = GetTargetSelectorSafe(player, range);
        if (tsTarget.IsValid() && player.InAutoAttackRange(tsTarget))
            return tsTarget;
    }

    // 4. Jungle monsters
    AIBaseClient bestJungle;
    float bestJungleHP = prioritizeSmallJungle ? FLT_MAX : -FLT_MAX;
    for (const auto& jungle : ObjectManager::JungleMinions()) {
        if (!jungle.IsValidTarget(range, player.Position())) continue;
        const float hp = jungle.Health();
        if (prioritizeSmallJungle ? (hp < bestJungleHP) : (hp > bestJungleHP)) {
            bestJungleHP = hp;
            bestJungle = jungle;
        }
    }
    if (bestJungle.IsValid()) return bestJungle;

    // 5. Lane clear minion (safe push)
    if (!ShouldWait(player, range, farmDelay)) {
        AIBaseClient bestMinion;
        float bestMinionHP = -FLT_MAX;
        for (const auto& minion : ObjectManager::EnemyMinions()) {
            if (!minion.IsValidTarget(range, player.Position())) continue;
            if (IsIgnored(minion.CharacterName())) continue;

            const float hp = minion.Health();
            const float predHP = GetHealthPrediction(
                minion,
                static_cast<int>(CoreAPI::Control::GetAttackDelay() * 1000.0f * LaneClearWaitTime),
                farmDelay);
            const float aaDmg = GetAADamage(player, minion);

            // Only clear if won't deny last-hit later
            if (predHP >= 2.0f * aaDmg || std::abs(predHP - hp) < 1.0f) {
                if (hp > bestMinionHP) {
                    bestMinionHP = hp;
                    bestMinion = minion;
                }
            }
        }
        if (bestMinion.IsValid()) return bestMinion;
    }

    return AIBaseClient();
}

// ── GetTarget – main dispatch ──
inline AIBaseClient GetTarget(const AIHeroClient& player, OrbwalkerMode mode, float range) {
    // Get menu settings (safely)
    int farmDelay = 30;
    bool prioritizeFarm = true;
    bool prioritizeMinions = false;
    bool prioritizeSmallJungle = false;

    // Try to read from orbwalker menu (you'd adapt if menu is available)

    switch (mode) {
    case OrbwalkerMode::Combo:
        return GetComboTarget(player, range);
    case OrbwalkerMode::Harass:
        return GetHarassTarget(player, range, farmDelay, prioritizeFarm);
    case OrbwalkerMode::LastHit:
        return GetLastHitTarget(player, range, farmDelay);
    case OrbwalkerMode::Clear:
        return GetLaneClearTarget(player, range, farmDelay, prioritizeFarm, prioritizeMinions, prioritizeSmallJungle);
    default:
        return AIBaseClient();
    }
}

} // namespace SDK::OrbwalkerSelector
