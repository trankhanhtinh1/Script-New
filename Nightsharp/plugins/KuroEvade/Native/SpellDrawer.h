#pragma once

#include "../../../SDK/SDK.h"

#include <memory>
#include <vector>

namespace Plugins::KuroEvade {

class SpellDrawer {
public:
    static void Draw(const std::vector<std::shared_ptr<SDK::Skillshot>>& skillshots, std::uint32_t color) {
        for (const auto& skillshot : skillshots) {
            if (skillshot && !skillshot->HasExpired()) {
                skillshot->Draw(color, color, 2);
            }
        }
    }
};

} // namespace Plugins::KuroEvade
