#pragma once

#include "Polygon.h"

#include <cmath>

namespace SDK::Geometry {

class RingPoly : public Polygon {
public:
    Vec2 Center = {};
    float InnerRadius = 0.0f;
    float OuterRadius = 0.0f;
    int Quality = 32;

    RingPoly() = default;

    RingPoly(const Vec3& center, float innerRadius, float outerRadius, int quality = 32) {
        Build(center.To2D(), innerRadius, outerRadius, quality);
    }

    RingPoly(const Vec2& center, float innerRadius, float outerRadius, int quality = 32) {
        Build(center, innerRadius, outerRadius, quality);
    }

    void Build(const Vec2& center, float innerRadius, float outerRadius, int quality = 32) {
        Center = center;
        InnerRadius = innerRadius;
        OuterRadius = outerRadius;
        Quality = quality;
        Points.clear();
        if (outerRadius <= innerRadius || quality < 4) {
            return;
        }

        const float step = (2.0f * static_cast<float>(M_PI)) / static_cast<float>(quality);
        for (int i = 0; i < quality; ++i) {
            const float angle = step * static_cast<float>(i);
            Points.emplace_back(center.x + std::cos(angle) * outerRadius, center.y + std::sin(angle) * outerRadius);
        }
        for (int i = quality - 1; i >= 0; --i) {
            const float angle = step * static_cast<float>(i);
            Points.emplace_back(center.x + std::cos(angle) * innerRadius, center.y + std::sin(angle) * innerRadius);
        }
    }

    void UpdatePolygon(int offset = 0) {
        Build(Center,
              std::max(0.0f, InnerRadius + static_cast<float>(offset)),
              std::max(0.0f, OuterRadius + static_cast<float>(offset)),
              Quality);
    }
};

} // namespace SDK::Geometry
