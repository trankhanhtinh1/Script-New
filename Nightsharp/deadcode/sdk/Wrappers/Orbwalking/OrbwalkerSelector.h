#pragma once

#include "../../Enumerations/OrbwalkerMode.h"
#include "../../Enumerations/DamageType.h"
#include "../../Core/Objects.h"
#include "../../Core/Game.h"
#include "../TargetSelector/TargetSelector.h"
#include "../Damages/Damage.h"
#include "../../Math/Prediction/Health.h"
#include "../../../core/CrashTelemetry.h"
#include "../../../menu/MenuUI.h"

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

template <typename Container>
inline AIBaseClient FindNearestInAttackRange(const Container& objects, const AIHeroClient& player, float range) {
    AIBaseClient best;
    float bestDist = FLT_MAX;
    for (const auto& obj : objects) {
        if (!obj.IsValidTarget(range, player.Position())) continue;
        if (!player.InAutoAttackRange(obj)) continue;
        const float dist = obj.Distance(player.Position());
        if (dist < bestDist) {
            bestDist = dist;
            best = obj;
        }
    }
    return best;
}

template <typename Container, typename Pred>
inline AIBaseClient FindNearestInAttackRangeIf(const Container& objects, const AIHeroClient& player, float range, Pred extra) {
    AIBaseClient best;
    float bestDist = FLT_MAX;
    for (const auto& obj : objects) {
        if (!extra(obj)) continue;
        if (!obj.IsValidTarget(range, player.Position())) continue;
        if (!player.InAutoAttackRange(obj)) continue;
        const float dist = obj.Distance(player.Position());
        if (dist < bestDist) {
            bestDist = dist;
            best = obj;
        }
    }
    return best;
}

// ── Combo (TargetSelector) ──
inline AIBaseClient GetComboTarget(const AIHeroClient& player, float range, Menu* atkMenu = nullptr) {
    auto ts = GetTargetSelectorSafe(player, range);
    if (ts.IsValid()) return ts;

    auto nearest = GetNearestEnemyTarget(player, range);
    if (nearest.IsValid()) return nearest;

    bool hasEnemyNear = false;
    for (const auto& enemy : ObjectManager::EnemyHeroes()) {
        if (enemy.IsValidTarget(enemy.AttackRange() * 2.0f + 200.0f, player.Position())) {
            hasEnemyNear = true;
            break;
        }
    }

    if (!hasEnemyNear) {
        const bool doSpecials = atkMenu ? atkMenu->GetBoolValue("specialMinions", true) : true;
        if (doSpecials) {
            for (const auto& minion : ObjectManager::SpecialMinions()) {
                if (minion.IsValidTarget(range, player.Position()) && player.InAutoAttackRange(minion))
                    return minion;
            }
        }
    }

    return AIBaseClient();
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
// C# logic (OrbwalkerSelector.cs line 148-156):
//   if (!prioritizeFarm) → check hero FIRST, before any minion
//   if (prioritizeFarm) → check killable minion FIRST, then hero
inline AIBaseClient GetHarassTarget(const AIHeroClient& player, float range, int farmDelay = 0,
                                     bool prioritizeFarm = true) {
    // When NOT prioritizing farm → hero target FIRST
    if (!prioritizeFarm) {
        auto tsTarget = GetTargetSelectorSafe(player, range);
        if (tsTarget.IsValid() && player.InAutoAttackRange(tsTarget))
            return tsTarget;
    }

    // Killable minion (last-hit)
    AIBaseClient killable = GetLastHitTarget(player, range, farmDelay);
    if (killable.IsValid()) return killable;

    // When prioritizing farm → hero target AFTER minion check
    if (prioritizeFarm) {
        auto tsTarget = GetTargetSelectorSafe(player, range);
        if (tsTarget.IsValid() && player.InAutoAttackRange(tsTarget))
            return tsTarget;
    }

    auto nearTarget = GetNearestEnemyTarget(player, range);
    if (nearTarget.IsValid() && player.InAutoAttackRange(nearTarget))
        return nearTarget;

    // Jungle minions
    for (const auto& jungle : ObjectManager::JungleMinions()) {
        if (jungle.IsValidTarget(range, player.Position())) return jungle;
    }

    return AIBaseClient();
}

// ── ShouldWait – C# OrbwalkerSelector.cs line 448-459 ──
// Uses FarmDelay in health prediction to determine if any minion
// will become killable soon (within LaneClearWaitTime attack cycles).
inline bool ShouldWait(const AIHeroClient& player, float range, int farmDelay = 0) {
    for (const auto& minion : ObjectManager::EnemyMinions()) {
        if (!minion.IsValidTarget(range, player.Position())) continue;
        if (IsIgnored(minion.CharacterName())) continue;

        const float predHP = GetHealthPrediction(
            minion,
            static_cast<int>(CoreAPI::Control::GetAttackDelay() * 1000.0f * LaneClearWaitTime),
            farmDelay);

        if (predHP < GetAADamage(player, minion)) {
            return true;
        }
    }
    return false;
}

// ── LaneClear ──
// C# OrbwalkerSelector.cs logic:
//   1. If !prioritizeFarm → hero first (line 148-156)
//   2. Killable minions (line 164-199)
//   3. Forced target (line 201-205)
//   4. Turrets/Inhib/Nexus if !prioritizeMinions || no minions (line 207-227)
//   5. Heroes (if mode != LastHit) (line 229-237)
//   6. Jungle minions (line 239-247)
//   7. Lane clear minions if !ShouldWait (line 383-428)
inline AIBaseClient GetLaneClearTarget(const AIHeroClient& player, float range, int farmDelay = 0,
                                        bool prioritizeFarm = true, bool prioritizeMinions = false,
                                        bool prioritizeSmallJungle = false) {
    // C# line 148-156: !prioritizeFarm → hero BEFORE everything
    if (!prioritizeFarm) {
        auto tsTarget = GetTargetSelectorSafe(player, range);
        if (tsTarget.IsValid() && player.InAutoAttackRange(tsTarget))
            return tsTarget;
    }

    // 1. Killable minion (same as LastHit)
    AIBaseClient killable = GetLastHitTarget(player, range, farmDelay);
    if (killable.IsValid()) return killable;

    // 2. Turrets / Inhib / Nexus
    // C# line 209: !prioritizeMinions || !minions.Any()
    const auto enemyMinions = ObjectManager::EnemyMinions();
    if (!prioritizeMinions || enemyMinions.empty()) {
        for (const auto& turret : ObjectManager::EnemyTurrets()) {
            if (turret.IsValidTarget(range, player.Position()) && player.InAutoAttackRange(turret))
                return turret;
        }
    }

    // 3. Champion target (only when prioritizeFarm, since !prioritizeFarm already checked above)
    // C# line 230-237: mode != LastHit → check hero
    if (prioritizeFarm) {
        auto tsTarget = GetTargetSelectorSafe(player, range);
        if (tsTarget.IsValid() && player.InAutoAttackRange(tsTarget))
            return tsTarget;
    }

    // 4. Jungle monsters
    // C# line 240-247: Jungle minions in LaneClear/Hybrid
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
    // C# line 383-428: Only if !ShouldWait()
    if (!ShouldWait(player, range, farmDelay)) {
        AIBaseClient bestMinion;
        float bestMinionHP = -FLT_MAX;
        for (const auto& minion : enemyMinions) {
            if (!minion.IsValidTarget(range, player.Position())) continue;
            if (IsIgnored(minion.CharacterName())) continue;

            const float hp = minion.Health();
            const float predHP = GetHealthPrediction(
                minion,
                static_cast<int>(CoreAPI::Control::GetAttackDelay() * 1000.0f * LaneClearWaitTime),
                farmDelay);
            const float aaDmg = GetAADamage(player, minion);

            // C# line 400-401/420-421: predHP >= 2*aaDmg || abs(predHP - hp) < epsilon
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

// ── Extra targets (wards, barrels, plants, pets) based on menu settings ──
inline AIBaseClient GetExtraTarget(const AIHeroClient& player, float range, Menu* atkMenu) {
    if (!atkMenu) return AIBaseClient();

    if (atkMenu->GetBoolValue("wards", false)) {
        auto best = FindNearestInAttackRangeIf(ObjectManager::Wards(), player, range,
            [](const auto& w) { return !w.IsAlly(); });
        if (best.IsValid()) return best;
    }

    if (atkMenu->GetBoolValue("barrels", false)) {
        auto best = FindNearestInAttackRangeIf(ObjectManager::Barrels(), player, range,
            [](const auto& b) { return b.Health() <= 1.0f; });
        if (best.IsValid()) return best;
    }

    if (atkMenu->GetBoolValue("junglePlant", false)) {
        auto best = FindNearestInAttackRange(ObjectManager::Plants(), player, range);
        if (best.IsValid()) return best;
    }

    if (atkMenu->GetBoolValue("specialMinions", true)) {
        const int myTeam = player.Team();
        auto best = FindNearestInAttackRangeIf(ObjectManager::Pets(), player, range,
            [myTeam](const auto& p) { return p.Team() != myTeam; });
        if (best.IsValid()) return best;
        auto bestSpecial = FindNearestInAttackRange(ObjectManager::SpecialMinions(), player, range);
        if (bestSpecial.IsValid()) return bestSpecial;
    }

    return AIBaseClient();
}

// ── GetTarget – main dispatch ──
inline AIBaseClient GetTarget(const AIHeroClient& player, OrbwalkerMode mode, float range, Menu* orbMenu = nullptr) {
    Menu* farmMenu = orbMenu ? orbMenu->GetSubMenu("farm") : nullptr;
    Menu* priMenu  = orbMenu ? orbMenu->GetSubMenu("prioritize") : nullptr;
    Menu* atkMenu  = orbMenu ? orbMenu->GetSubMenu("attackable") : nullptr;

    const int farmDelay            = farmMenu ? farmMenu->GetSliderValue("farmDelay", 30) : 30;
    const bool prioritizeFarm      = priMenu  ? priMenu->GetBoolValue("farmOverHarass", true) : true;
    const bool prioritizeSmallJung = priMenu  ? priMenu->GetBoolValue("smallJungle", false) : false;

    AIBaseClient target;
    switch (mode) {
    case OrbwalkerMode::Combo:
        target = GetComboTarget(player, range, atkMenu);
        break;
    case OrbwalkerMode::Harass:
        target = GetHarassTarget(player, range, farmDelay, prioritizeFarm);
        break;
    case OrbwalkerMode::LastHit:
        target = GetLastHitTarget(player, range, farmDelay);
        break;
    case OrbwalkerMode::Clear:
        target = GetLaneClearTarget(player, range, farmDelay, prioritizeFarm, false, prioritizeSmallJung);
        break;
    default:
        return AIBaseClient();
    }

    if (target.IsValid()) return target;
    return GetExtraTarget(player, range, atkMenu);
}

} // namespace SDK::OrbwalkerSelector
