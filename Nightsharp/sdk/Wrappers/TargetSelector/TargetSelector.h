#pragma once

#include "../../Enumerations/DamageType.h"
#include "../../Enumerations/TargetSelectorMode.h"
#include "../../Core/Objects.h"
#include "../Spells/Spell.h"
#include "../../UI/UI.h"
#include "HeroVisibleEntry.h"
#include "TargetSelectorDrawing.h"
#include "TargetSelectorHumanizer.h"
#include "TargetSelectorMode.h"
#include "TargetSelectorSelected.h"

#include <algorithm>
#include <string>
#include <vector>

namespace SDK {

class TargetSelector {
public:
    static void Initialize() {
        if (s_initialized) {
            return;
        }
        s_initialized = true;

        auto root = UI::CreateMenu("targetselector", "Target Selector");
        s_menu = root.Raw();
        TargetSelectorModes::TargetSelectorModeManager::Initialize(s_menu);
        TargetSelectorSelected::Initialize(s_menu);
        TargetSelectorDrawing::Initialize(s_menu);
        TargetSelectorHumanizer::Initialize(s_menu);
    }

    static Menu* GetMenu() {
        return s_menu;
    }

    static void Update() {
        TargetSelectorSelected::Update();
    }

    static AIHeroClient GetSelectedTarget() {
        return TargetSelectorSelected::Target();
    }

    static void SetSelectedTarget(const AIHeroClient& target) {
        TargetSelectorSelected::SetTarget(target);
    }

    static void ClearSelectedTarget() {
        TargetSelectorSelected::ClearTarget();
    }

    static AIHeroClient GetForcedTarget() {
        return TargetSelectorSelected::ForcedTarget();
    }

    static void SetForcedTarget(const AIHeroClient& target) {
        TargetSelectorSelected::SetForcedTarget(target);
    }

    static void ClearForcedTarget() {
        TargetSelectorSelected::ClearForcedTarget();
    }

    static int GetPriority(const AIHeroClient& hero) {
        return TargetSelectorModes::TargetSelectorModeManager::GetPriority(hero);
    }

    static TargetSelectorMode Mode() {
        return TargetSelectorModes::TargetSelectorModeManager::Current(s_menu);
    }

    static void SetMode(TargetSelectorMode mode) {
        TargetSelectorModes::TargetSelectorModeManager::SetCurrent(s_menu, mode);
    }

    static AIHeroClient GetTarget(const Spell& spell, bool ignoreShields = true) {
        (void)ignoreShields;
        return GetTarget(spell.GetRange(), spell.DamageType, spell.GetSource());
    }

    static AIHeroClient GetTarget(float range, DamageType damageType = DamageType::True, const Vector3& from = Vector3()) {
        const auto targets = GetTargets(range, damageType, from);
        return targets.empty() ? AIHeroClient() : targets.front();
    }

    static bool IsValidTarget(const AIHeroClient& hero,
                              float range,
                              DamageType damageType = DamageType::True,
                              const Vector3& from = Vector3()) {
        if (!hero.IsValid()) {
            return false;
        }
        const auto origin = from.IsZero() ? ObjectManager::Player().Position() : from;
        if (!hero.IsValidTarget(range, origin)) {
            return false;
        }
        return TargetSelectorModes::ITargetSelectorMode::EffectiveHealth(hero, damageType) > 0.0f;
    }

    static std::vector<AIHeroClient> GetTargets(float range, DamageType damageType = DamageType::True, const Vector3& from = Vector3()) {
        const auto player = ObjectManager::Player();
        const Vector3 origin = from.IsZero() ? player.Position() : from;

        std::vector<AIHeroClient> out;
        out.reserve(16);

        auto pushUnique = [&](const AIHeroClient& hero) {
            if (!hero.IsValid() || !hero.IsValidTarget(range, origin)) {
                return;
            }
            for (const auto& existing : out) {
                if (existing.NetworkId() == hero.NetworkId()) {
                    return;
                }
            }
            out.push_back(hero);
        };

        const auto forced = TargetSelectorSelected::ForcedTarget();
        if (forced.IsValidTarget(range, origin)) {
            pushUnique(forced);
            return out;
        }

        const auto selected = TargetSelectorSelected::Target();
        if (TargetSelectorSelected::Force(s_menu) && selected.IsValidTarget(range, origin)) {
            pushUnique(selected);
            return out;
        }

        const auto enemies = ObjectManager::EnemyHeroes();
        for (const auto& enemy : enemies) {
            if (!enemy.IsValidTarget(range, origin)) {
                continue;
            }
            pushUnique(enemy);
        }

        out = TargetSelectorModes::TargetSelectorModeManager::Order(s_menu, out, origin, damageType);
        const auto stabilized = TargetSelectorHumanizer::Choose(s_menu, out, range, origin);
        if (stabilized.IsValid()) {
            for (auto it = out.begin(); it != out.end(); ++it) {
                if (it->NetworkId() == stabilized.NetworkId()) {
                    std::rotate(out.begin(), it, it + 1);
                    break;
                }
            }
        }

        const auto selectedFocus = TargetSelectorSelected::Target();
        if (TargetSelectorSelected::Focus(s_menu) && selectedFocus.IsValid()) {
            for (auto it = out.begin(); it != out.end(); ++it) {
                if (it->NetworkId() == selectedFocus.NetworkId()) {
                    std::rotate(out.begin(), it, it + 1);
                    break;
                }
            }
        }

        return out;
    }

    static void Render() {
        const auto current = GetTarget(1500.0f, DamageType::Physical, ObjectManager::Player().Position());
        TargetSelectorDrawing::Render(s_menu, TargetSelectorSelected::Target(), TargetSelectorSelected::ForcedTarget(), current);
    }

private:
    static inline bool s_initialized = false;
    static inline Menu* s_menu = nullptr;
};

} // namespace SDK
