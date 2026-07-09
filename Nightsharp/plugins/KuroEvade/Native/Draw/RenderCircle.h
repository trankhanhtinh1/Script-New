#pragma once

#include "RenderObject.h"

#include "../../../../SDK/SDK.h"

namespace Plugins::KuroEvade {

struct RenderCircle final : RenderObject {
    Vec2 renderPosition;
    int radius = 65;
    int width = 5;
    std::uint32_t color = 0xFFFFFFFFu;

    RenderCircle() = default;
    RenderCircle(const Vec2& position, float renderTime, int radiusValue = 65, int widthValue = 5)
        : renderPosition(position), radius(radiusValue), width(widthValue) {
        startTime = static_cast<float>(EvadeUtils::TickCount());
        endTime = startTime + renderTime;
    }

    RenderCircle(const Vec2& position, float renderTime, std::uint32_t colorValue,
                 int radiusValue = 65, int widthValue = 5)
        : renderPosition(position), radius(radiusValue), width(widthValue), color(colorValue) {
        startTime = static_cast<float>(EvadeUtils::TickCount());
        endTime = startTime + renderTime;
    }

    void Draw() override {
        const auto player = SDK::ObjectManager::Player();
        const float height = player.IsValid() ? player.Position().y : 0.0f;
        SDK::Drawing::DrawCircle(Vec3::From2D(renderPosition, height),
                                 static_cast<float>(radius),
                                 color,
                                 static_cast<float>(width));
    }
};

} // namespace Plugins::KuroEvade
