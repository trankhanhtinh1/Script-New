#pragma once

#include "IWeightItem.h"
#include "../../../UI/UI.h"

namespace SDK::TargetSelectorModes {

struct WeightItemWrapper {
    const IWeightItem* Item = nullptr;
    bool EnabledByDefault = true;
    int WeightByDefault = 100;

    void AddToMenu(Menu* menu) const {
        if (!menu || !Item) {
            return;
        }

        auto node = UI::Wrap(menu);
        const std::string key = Item->Name();
        node.AddBool(key + "_enabled", Item->DisplayName(), EnabledByDefault);
        node.AddSlider(key + "_weight", std::string(Item->DisplayName()) + " Weight", WeightByDefault, 0, 200);
    }

    float Evaluate(Menu* menu,
                   const AIHeroClient& player,
                   const AIHeroClient& target,
                   DamageType damageType) const {
        if (!Item) {
            return 0.0f;
        }

        if (menu) {
            auto node = UI::Wrap(menu);
            const std::string key = Item->Name();
            if (!node.Bool(key + "_enabled", EnabledByDefault)) {
                return 0.0f;
            }
            const float weight = static_cast<float>(node.Slider(key + "_weight", WeightByDefault)) / 100.0f;
            return Item->Score(player, target, damageType) * weight;
        }

        return Item->Score(player, target, damageType) * (static_cast<float>(WeightByDefault) / 100.0f);
    }
};

} // namespace SDK::TargetSelectorModes
