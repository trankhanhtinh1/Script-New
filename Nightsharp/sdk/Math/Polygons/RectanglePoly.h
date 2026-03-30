#pragma once

#include "Polygon.h"

namespace SDK::Geometry {

class RectanglePoly : public Polygon {
public:
    Vec2 Start = {};
    Vec2 End = {};
    float Width = 0.0f;

    RectanglePoly() = default;

    RectanglePoly(const Vec3& start, const Vec3& end, float width) {
        Build(start.To2D(), end.To2D(), width);
    }

    RectanglePoly(const Vec2& start, const Vec2& end, float width) {
        Build(start, end, width);
    }

    Vec2 Direction() const {
        return (End - Start).Normalized();
    }

    Vec2 Perpendicular() const {
        return Direction().Perpendicular();
    }

    void Build(const Vec2& start, const Vec2& end, float width) {
        Start = start;
        End = end;
        Width = width;
        Points.clear();
        const Vec2 dir = Direction();
        const Vec2 normal = Perpendicular();
        const float effectiveWidth = std::max(width, 0.0f);

        Points.push_back(Start + (normal * effectiveWidth));
        Points.push_back(Start - (normal * effectiveWidth));
        Points.push_back(End - (normal * effectiveWidth));
        Points.push_back(End + (normal * effectiveWidth));
    }

    void UpdatePolygon(int offset = 0, float overrideWidth = -1.0f) {
        Points.clear();
        const Vec2 dir = Direction();
        const Vec2 normal = Perpendicular();
        const float effectiveWidth = overrideWidth > 0.0f ? overrideWidth : (Width + static_cast<float>(offset));
        const float push = static_cast<float>(offset);

        Points.push_back(Start + (normal * effectiveWidth) - (dir * push));
        Points.push_back(Start - (normal * effectiveWidth) - (dir * push));
        Points.push_back(End - (normal * effectiveWidth) + (dir * push));
        Points.push_back(End + (normal * effectiveWidth) + (dir * push));
    }
};

} // namespace SDK::Geometry
