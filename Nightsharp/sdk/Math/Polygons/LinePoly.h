#pragma once

#include "RectanglePoly.h"

namespace SDK::Geometry {

class LinePoly : public RectanglePoly {
public:
    LinePoly() = default;

    LinePoly(const Vec3& start, const Vec3& end, float width)
        : RectanglePoly(start, end, width) {}

    LinePoly(const Vec2& start, const Vec2& end, float width)
        : RectanglePoly(start, end, width) {}
};

} // namespace SDK::Geometry
