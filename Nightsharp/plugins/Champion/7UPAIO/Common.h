#pragma once

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::AIO7UP::Common {

inline AIHeroClient Player() {
    return ObjectManager::Player();
}

inline bool Bool(Menu* menu, const char* key, bool fallback = true) {
    if (!menu) {
        return fallback;
    }

    const auto* item = menu->Get<MenuBool>(key);
    return item ? item->Value : fallback;
}

inline int Slider(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) {
        return fallback;
    }

    const auto* item = menu->Get<MenuSlider>(key);
    return item ? item->Value : fallback;
}

inline bool SliderButtonEnabled(Menu* menu, const char* key, bool fallback = false) {
    if (!menu) {
        return fallback;
    }

    const auto* item = menu->Get<MenuSliderButton>(key);
    return item ? item->Enabled : fallback;
}

inline int SliderButtonValue(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) {
        return fallback;
    }

    const auto* item = menu->Get<MenuSliderButton>(key);
    return item ? item->Value : fallback;
}

inline bool Key(Menu* menu, const char* key, bool fallback = false) {
    if (!Game::ShouldProcessInput()) {
        return false;
    }
    if (!menu) {
        return fallback;
    }

    const auto* item = menu->Get<MenuKeyBind>(key);
    return item ? item->Active : fallback;
}

inline int ListIndex(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) {
        return fallback;
    }

    const auto* item = menu->Get<MenuList>(key);
    return item ? item->Index : fallback;
}

inline bool ShouldRunNow(DWORD& lastTick, DWORD intervalMs) {
    const DWORD now = GetTickCount();
    if (lastTick != 0 && now - lastTick < intervalMs) {
        return false;
    }

    lastTick = now;
    return true;
}

inline bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

inline bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f;
}

inline bool ValidTarget(const AIBaseClient& unit, float range = FLT_MAX) {
    return ValidUnit(unit) && Extensions::IsValidTarget(unit, range, true);
}

inline bool ValidHeroTarget(const AIHeroClient& hero, float range = FLT_MAX) {
    return ValidUnit(hero) && Extensions::IsValidTarget(hero, range, true);
}

inline AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

inline AIHeroClient GetTargetNearMouse(float range, DamageType damageType, float mouseRadius) {
    AIHeroClient best;
    float bestDistance = mouseRadius;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, range)) {
            continue;
        }

        const float distance = enemy.Position().Distance2D(Game::CursorPos());
        if (distance <= bestDistance) {
            best = enemy;
            bestDistance = distance;
        }
    }

    if (best.IsValid()) {
        return best;
    }
    return GetTarget(range, damageType);
}

inline AIHeroClient GetTargetNearMouseStrict(float range, float mouseRadius) {
    AIHeroClient best;
    float bestDistance = mouseRadius;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, range)) {
            continue;
        }

        const float distance = enemy.Position().Distance2D(Game::CursorPos());
        if (distance <= bestDistance) {
            best = enemy;
            bestDistance = distance;
        }
    }

    return best;
}

inline bool CastPosition(Spell& spell,
                         const Vector3& position,
                         const AIBaseClient& target = AIBaseClient()) {
    (void)target;
    return spell.Cast(position);
}

inline bool CastUnit(Spell& spell, const AIBaseClient& target) {
    return spell.Cast(target) == CastStates::SuccessfullyCasted;
}

inline std::vector<AIBaseClient> ToBaseList(const std::vector<AIMinionClient>& units) {
    std::vector<AIBaseClient> result;
    result.reserve(units.size());
    for (const auto& unit : units) {
        result.emplace_back(unit.Handle());
    }
    return result;
}

inline std::vector<AIBaseClient> EnemyLaneMinionBases(float range) {
    std::vector<AIBaseClient> result;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, range) && minion.IsMinion() && !minion.IsJungle() && !minion.IsPlant() &&
            !minion.IsPet() && !minion.IsClone()) {
            result.emplace_back(minion.Handle());
        }
    }
    return result;
}

inline std::vector<AIBaseClient> JungleBases(float range) {
    std::vector<AIBaseClient> result;
    for (const auto& mob : GameObjects::Jungle()) {
        if (ValidTarget(mob, range) && mob.GetJungleType() != JungleType::Unknown && !mob.IsPlant() &&
            !mob.IsPet() && !mob.IsClone()) {
            result.emplace_back(mob.Handle());
        }
    }
    return result;
}

inline bool HasAnyProtectionBuff(const AIBaseClient& target) {
    return target.HasBuff("JudicatorIntervention") ||
           target.HasBuff("kindredrnodeathbuff") ||
           target.HasBuff("KindredRNoDeathBuff") ||
           target.HasBuff("Undying Rage") ||
           target.HasBuff("UndyingRage") ||
           target.HasBuff("FioraW") ||
           target.HasBuff("SivirShield") ||
           target.HasBuff("ShroudofDarkness");
}

inline int CountEnemyHeroesInCircle(const Vector3& center, float radius) {
    int count = 0;
    const float radiusSqr = radius * radius;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy) && enemy.Position().DistanceSqr2D(center) <= radiusSqr) {
            ++count;
        }
    }
    return count;
}

} // namespace Plugins::AIO7UP::Common
