#pragma once

#include "Polygon.h"

#include <cmath>

namespace SDK::Geometry {

class CirclePoly : public Polygon {
public:
    Vec2 Center = {};
    float Radius = 0.0f;
    int Quality = 20;

    CirclePoly() = default;

    CirclePoly(const Vec3& center, float radius, int quality = 20) {
        Build(center.To2D(), radius, quality);
    }

    CirclePoly(const Vec2& center, float radius, int quality = 24) {
        Build(center, radius, quality);
    }

    void Build(const Vec2& center, float radius, int quality = 24) {
        Center = center;
        Radius = radius;
        Quality = quality;
        Points.clear();
        if (radius <= 0.0f || quality < 3) {
            return;
        }

        const float outRadius = radius / std::cos((2.0f * static_cast<float>(M_PI)) / static_cast<float>(quality));
        const float step = (2.0f * static_cast<float>(M_PI)) / static_cast<float>(quality);
        for (int i = 1; i <= quality; ++i) {
            const float angle = step * static_cast<float>(i);
            Points.emplace_back(center.x + std::cos(angle) * outRadius, center.y + std::sin(angle) * outRadius);
        }
    }

    void UpdatePolygon(int offset = 0, float overrideWidth = -1.0f) {
        const float effectiveRadius = overrideWidth > 0.0f ? overrideWidth : (Radius + static_cast<float>(offset));
        Build(Center, effectiveRadius, Quality);
    }
};

} // namespace SDK::Geometry
