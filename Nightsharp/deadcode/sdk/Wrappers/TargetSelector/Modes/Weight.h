#pragma once

#include "../ITargetSelectorMode.h"
#include "WeightItemWrapper.h"
#include "Weights\AbilityPower.h"
#include "Weights\Aggro.h"
#include "Weights\AttackDamage.h"
#include "Weights\CrowdControl.h"
#include "Weights\FocusMe.h"
#include "Weights\Gold.h"
#include "Weights\Killable.h"
#include "Weights\LowHealth.h"
#include "Weights\LowResists.h"
#include "Weights\ShortDistanceCursor.h"
#include "Weights\ShortDistancePlayer.h"
#include "Weights\TeamFocus.h"

#include <algorithm>
#include <array>

namespace SDK::TargetSelectorModes {

class Weight final : public ITargetSelectorMode {
public:
    const char* Name() const override { return "weight"; }
    const char* DisplayName() const override { return "Weight"; }

    void AddToMenu(Menu* menu) const override {
        if (!menu) {
            return;
        }

        auto weightsMenu = UI::Wrap(menu).AddMenu("weights", "Weights");
        for (const auto& item : Items()) {
            item.AddToMenu(weightsMenu.Raw());
        }
    }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes,
                                             const Vector3&,
                                             DamageType damageType) const override {
        auto ordered = heroes;
        const auto player = ObjectManager::Player();
        std::stable_sort(ordered.begin(), ordered.end(),
            [&](const AIHeroClient& lhs, const AIHeroClient& rhs) {
                return Evaluate(player, lhs, damageType) > Evaluate(player, rhs, damageType);
            });
        return ordered;
    }

    static void BindMenu(Menu* menu) {
        s_menu = menu;
    }

private:
    static float Evaluate(const AIHeroClient& player,
                          const AIHeroClient& hero,
                          DamageType damageType) {
        float total = 0.0f;
        for (const auto& item : Items()) {
            total += item.Evaluate(s_menu, player, hero, damageType);
        }
        return total;
    }

    static const std::array<WeightItemWrapper, 12>& Items() {
        static Weights::AbilityPower abilityPower = {};
        static Weights::Aggro aggro = {};
        static Weights::AttackDamage attackDamage = {};
        static Weights::CrowdControl crowdControl = {};
        static Weights::FocusMe focusMe = {};
        static Weights::Gold gold = {};
        static Weights::Killable killable = {};
        static Weights::LowHealth lowHealth = {};
        static Weights::LowResists lowResists = {};
        static Weights::ShortDistanceCursor shortDistanceCursor = {};
        static Weights::ShortDistancePlayer shortDistancePlayer = {};
        static Weights::TeamFocus teamFocus = {};
        static std::array<WeightItemWrapper, 12> items = {{
            { &abilityPower, true, 60 },
            { &aggro, true, 75 },
            { &attackDamage, true, 70 },
            { &crowdControl, true, 80 },
            { &focusMe, true, 65 },
            { &gold, false, 30 },
            { &killable, true, 100 },
            { &lowHealth, true, 95 },
            { &lowResists, true, 70 },
            { &shortDistanceCursor, true, 60 },
            { &shortDistancePlayer, true, 85 },
            { &teamFocus, true, 55 }
        }};
        return items;
    }

    static inline Menu* s_menu = nullptr;
};

} // namespace SDK::TargetSelectorModes
