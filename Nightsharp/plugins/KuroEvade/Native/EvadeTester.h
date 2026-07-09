#pragma once

#include "Draw/RenderCircle.h"
#include "Draw/RenderLine.h"
#include "Draw/RenderObjects.h"
#include "EvadeHelper.h"

namespace Plugins::KuroEvade {

struct EvadeTester final {
    static void DrawCandidate(const Vec2& position,
                              float renderTime = 500.0f,
                              std::uint32_t color = 0xFF00FF00u) {
        RenderObjects::Emplace<RenderCircle>(position, renderTime, color, 45, 2);
    }

    static void DrawPath(const Vec2& start,
                         const Vec2& end,
                         float renderTime = 500.0f,
                         std::uint32_t color = 0xFF00FFFFu) {
        RenderObjects::Emplace<RenderLine>(start, end, renderTime, color, 65, 2);
    }

    static bool IsDangerous(const EvadeSettings& settings,
                            const Vec2& position,
                            float radius,
                            const EvadeHelper::SkillshotList& skillshots) {
        EvadeHelper helper(settings);
        for (const auto& skillshot : skillshots) {
            if (skillshot && helper.ShouldConsiderSpell(*skillshot) &&
                EvadeHelper::InSkillShot(*skillshot, position, radius + settings.ExtraSpellRadius)) {
                return true;
            }
        }
        return false;
    }
};

} // namespace Plugins::KuroEvade
