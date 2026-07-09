#pragma once

#include "RenderObject.h"

#include "../../../../SDK/SDK.h"

namespace Plugins::KuroEvade {

struct RenderLine final : RenderObject {
    Vec2 start;
    Vec2 end;
    int width = 3;
    std::uint32_t color = 0xFFFFFFFFu;

    RenderLine() = default;
    RenderLine(const Vec2& startPosition, const Vec2& endPosition,
               float renderTime, int /*radius*/ = 65, int widthValue = 3)
        : start(startPosition), end(endPosition), width(widthValue) {
        startTime = static_cast<float>(EvadeUtils::TickCount());
        endTime = startTime + renderTime;
    }

    RenderLine(const Vec2& startPosition, const Vec2& endPosition,
               float renderTime, std::uint32_t colorValue,
               int /*radius*/ = 65, int widthValue = 3)
        : start(startPosition), end(endPosition), width(widthValue), color(colorValue) {
        startTime = static_cast<float>(EvadeUtils::TickCount());
        endTime = startTime + renderTime;
    }

    void Draw() override {
        const auto player = SDK::ObjectManager::Player();
        const float height = player.IsValid() ? player.Position().y : 0.0f;
        Vec2 startScreen;
        Vec2 endScreen;
        const bool hasStart = SDK::Drawing::WorldToScreen(Vec3::From2D(start, height), startScreen);
        const bool hasEnd = SDK::Drawing::WorldToScreen(Vec3::From2D(end, height), endScreen);
        if (hasStart || hasEnd) {
            SDK::Drawing::DrawLine(startScreen.x, startScreen.y,
                                   endScreen.x, endScreen.y,
                                   static_cast<float>(width), color, true);
        }
    }
};

} // namespace Plugins::KuroEvade
