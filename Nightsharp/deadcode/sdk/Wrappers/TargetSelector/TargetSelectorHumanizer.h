#pragma once

#include "../../Core/Game.h"
#include "../../Core/Objects.h"
#include "../../UI/UI.h"

#include <vector>

namespace SDK {

class TargetSelectorHumanizer {
public:
    static void Initialize(Menu* root) {
        if (s_initialized || !root) {
            return;
        }
        s_initialized = true;

        auto humanizer = UI::Wrap(root).AddMenu("humanizer", "Humanizer");
        humanizer.AddBool("enabled", "Enabled", true);
        humanizer.AddSlider("switchDelay", "Switch Delay", 125, 0, 500);
        humanizer.AddSlider("stickRange", "Stick Range", 125, 0, 500);
    }

    static AIHeroClient Choose(Menu* root,
                               const std::vector<AIHeroClient>& ordered,
                               float range,
                               const Vector3& from) {
        if (ordered.empty()) {
            s_lastTarget = AIHeroClient();
            return AIHeroClient();
        }

        if (!root) {
            return ordered.front();
        }

        const auto humanizer = UI::Wrap(root).SubMenu("humanizer");
        if (!humanizer.IsValid() || !humanizer.Bool("enabled", true)) {
            s_lastTarget = ordered.front();
            s_lastSwitchTick = Game::TickCount();
            return ordered.front();
        }

        const int switchDelay = humanizer.Slider("switchDelay", 125);
        const float stickRange = static_cast<float>(humanizer.Slider("stickRange", 125));
        const int now = Game::TickCount();

        if (s_lastTarget.IsValidTarget(range + stickRange, from)) {
            for (const auto& hero : ordered) {
                if (hero.NetworkId() == s_lastTarget.NetworkId()) {
                    if ((now - s_lastSwitchTick) < switchDelay) {
                        return s_lastTarget;
                    }
                    break;
                }
            }
        }

        s_lastTarget = ordered.front();
        s_lastSwitchTick = now;
        return s_lastTarget;
    }

private:
    static inline bool s_initialized = false;
    static inline AIHeroClient s_lastTarget = {};
    static inline int s_lastSwitchTick = 0;
};

} // namespace SDK
