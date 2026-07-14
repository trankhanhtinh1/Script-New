#pragma once

#include "RenderObject.h"

#include "../../../../../SDK/SDK.h"

#include <string>
#include <utility>

namespace Plugins::KuroEvade {

struct RenderText final : RenderObject {
    Vec2 renderPosition;
    std::string text;
    std::uint32_t color = 0xFFFFFFFFu;

    RenderText() = default;
    RenderText(std::string value, const Vec2& position, float renderTime)
        : renderPosition(position), text(std::move(value)) {
        startTime = static_cast<float>(EvadeUtils::TickCount());
        endTime = startTime + renderTime;
    }

    RenderText(std::string value, const Vec2& position, float renderTime, std::uint32_t colorValue)
        : renderPosition(position), text(std::move(value)), color(colorValue) {
        startTime = static_cast<float>(EvadeUtils::TickCount());
        endTime = startTime + renderTime;
    }

    void Draw() override {
        const auto player = SDK::ObjectManager::Player();
        const float height = player.IsValid() ? player.Position().y : 0.0f;
        Vec2 screen;
        if (SDK::Drawing::WorldToScreen(Vec3::From2D(renderPosition, height), screen)) {
            SDK::Drawing::DrawText(screen.x, screen.y, color, text.c_str());
        }
    }
};

} // namespace Plugins::KuroEvade
