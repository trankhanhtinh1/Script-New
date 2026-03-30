#pragma once

#include "Polygon.h"

#include <cmath>

namespace SDK::Geometry {

class ArcPoly : public Polygon {
public:
    Vec2 Center = {};
    float Radius = 0.0f;
    float StartAngle = 0.0f;
    float EndAngle = 0.0f;
    int Quality = 18;

    ArcPoly() = default;

    ArcPoly(const Vec3& center, float radius, float startAngle, float endAngle, int quality = 18) {
        Build(center.To2D(), radius, startAngle, endAngle, quality);
    }

    ArcPoly(const Vec2& center, float radius, float startAngle, float endAngle, int quality = 18) {
        Build(center, radius, startAngle, endAngle, quality);
    }

    void Build(const Vec2& center, float radius, float startAngle, float endAngle, int quality = 18) {
        Center = center;
        Radius = radius;
        StartAngle = startAngle;
        EndAngle = endAngle;
        Quality = quality;
        Points.clear();
        if (radius <= 0.0f || quality < 2) {
            return;
        }

        const float step = (endAngle - startAngle) / static_cast<float>(quality);
        for (int i = 0; i <= quality; ++i) {
            const float angle = startAngle + (step * static_cast<float>(i));
            Points.emplace_back(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
        }
    }

    void UpdatePolygon(int offset = 0, float overrideWidth = -1.0f) {
        const float effectiveRadius = overrideWidth > 0.0f ? overrideWidth : (Radius + static_cast<float>(offset));
        Build(Center, effectiveRadius, StartAngle, EndAngle, Quality);
    }
};

} // namespace SDK::Geometry
