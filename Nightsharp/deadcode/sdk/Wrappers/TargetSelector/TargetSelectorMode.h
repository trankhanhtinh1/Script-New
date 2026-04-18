#pragma once

#include "../../Enumerations/TargetSelectorMode.h"
#include "../../UI/UI.h"
#include "ITargetSelectorMode.h"
#include "Modes\Closest.h"
#include "Modes\LeastHealth.h"
#include "Modes\LessAttacksToKill.h"
#include "Modes\LessCastsToKill.h"
#include "Modes\MostAbilityPower.h"
#include "Modes\MostAttackDamage.h"
#include "Modes\NearMouse.h"
#include "Modes\Priority.h"
#include "Modes\Weight.h"

#include <vector>

namespace SDK::TargetSelectorModes {

class TargetSelectorModeManager {
public:
    static void Initialize(Menu* root) {
        if (!root) {
            return;
        }

        auto general = UI::Wrap(root).AddMenu("general", "General");
        general.AddList("mode", "Mode", Names(), static_cast<int>(TargetSelectorMode::Priority));

        Closest().AddToMenu(root);
        LeastHealth().AddToMenu(root);
        LessAttacksToKill().AddToMenu(root);
        LessCastsToKill().AddToMenu(root);
        MostAbilityPower().AddToMenu(root);
        MostAttackDamage().AddToMenu(root);
        NearMouse().AddToMenu(root);
        Priority().AddToMenu(root);
        Weight weight = {};
        weight.AddToMenu(root);
        Weight::BindMenu(root);
    }

    static std::vector<std::string> Names() {
        return {
            "Priority",
            "Closest",
            "Least Health",
            "Most AD",
            "Most AP",
            "Near Mouse",
            "Less Attacks To Kill",
            "Less Casts To Kill",
            "Weight"
        };
    }

    static TargetSelectorMode Current(Menu* root) {
        if (!root) {
            return TargetSelectorMode::Priority;
        }
        const auto general = UI::Wrap(root).SubMenu("general");
        return static_cast<TargetSelectorMode>(general.ListIndex("mode", static_cast<int>(TargetSelectorMode::Priority)));
    }

    static void SetCurrent(Menu* root, TargetSelectorMode mode) {
        if (!root) {
            return;
        }
        const auto general = UI::Wrap(root).SubMenu("general");
        general.Entry("mode").SetIndex(static_cast<int>(mode));
    }

    static std::vector<AIHeroClient> Order(Menu* root,
                                           const std::vector<AIHeroClient>& heroes,
                                           const Vector3& from,
                                           DamageType damageType) {
        switch (Current(root)) {
        case TargetSelectorMode::Closest:
            return Closest().OrderChampions(heroes, from, damageType);
        case TargetSelectorMode::LeastHealth:
            return LeastHealth().OrderChampions(heroes, from, damageType);
        case TargetSelectorMode::MostAttackDamage:
            return MostAttackDamage().OrderChampions(heroes, from, damageType);
        case TargetSelectorMode::MostAbilityPower:
            return MostAbilityPower().OrderChampions(heroes, from, damageType);
        case TargetSelectorMode::NearMouse:
            return NearMouse().OrderChampions(heroes, from, damageType);
        case TargetSelectorMode::LessAttacksToKill:
            return LessAttacksToKill().OrderChampions(heroes, from, damageType);
        case TargetSelectorMode::LessCastsToKill:
            return LessCastsToKill().OrderChampions(heroes, from, damageType);
        case TargetSelectorMode::Weight:
            return Weight().OrderChampions(heroes, from, damageType);
        case TargetSelectorMode::Priority:
        default:
            return Priority().OrderChampions(heroes, from, damageType);
        }
    }

    static int GetPriority(const AIHeroClient& hero) {
        return Priority::GetPriority(hero);
    }
};

} // namespace SDK::TargetSelectorModes
