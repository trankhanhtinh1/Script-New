#pragma once

#include "../ITargetSelectorMode.h"
#include "../../Core/Game.h"

namespace SDK::Modes {

class NearMouse : public ITargetSelectorMode {
public:
    const char* DisplayName() const override { return "Near Mouse"; }
    const char* Name() const override { return "near-mouse"; }

    void AddToMenu(Menu* menu) override {
    }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes) override {
        const auto cursorPos = Game::CursorPosRaw();
        auto result = heroes;
        std::sort(result.begin(), result.end(), [&cursorPos](const AIHeroClient& a, const AIHeroClient& b) {
            return a.Distance(cursorPos) < b.Distance(cursorPos);
        });
        return result;
    }
};

} // namespace SDK::Modes
