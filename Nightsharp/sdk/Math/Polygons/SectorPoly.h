#pragma once

#include "Polygon.h"

#include <cmath>

namespace SDK::Geometry {

class SectorPoly : public Polygon {
public:
    Vec2 Center = {};
    Vec2 Direction = {};
    float Angle = 0.0f;
    float Radius = 0.0f;
    int Quality = 20;

    SectorPoly() = default;

    SectorPoly(const Vec3& center, const Vec3& direction, float angle, float radius, int quality = 20) {
        Build(center.To2D(), direction.To2D(), angle, radius, quality);
    }

    SectorPoly(const Vec2& center, const Vec2& direction, float angle, float radius, int quality = 18) {
        Build(center, direction, angle, radius, quality);
    }

    void Build(const Vec2& center, const Vec2& direction, float angle, float radius, int quality = 18) {
        Center = center;
        Direction = direction.Normalized();
        Angle = angle;
        Radius = radius;
        Quality = quality;
        Points.clear();
        if (radius <= 0.0f || quality < 2) {
            return;
        }

        const float outRadius = radius / std::cos((2.0f * static_cast<float>(M_PI)) / static_cast<float>(quality));
        const float startAngle = Direction.Angle() - (Angle * 0.5f);
        const float step = Angle / static_cast<float>(quality);

        Points.push_back(Center);
        for (int i = 0; i <= quality; ++i) {
            const float current = startAngle + (step * static_cast<float>(i));
            Points.emplace_back(Center.x + std::cos(current) * outRadius, Center.y + std::sin(current) * outRadius);
        }
    }

    void UpdatePolygon(int offset = 0) {
        Build(Center, Direction, Angle, Radius + static_cast<float>(offset), Quality);
    }
};

} // namespace SDK::Geometry
