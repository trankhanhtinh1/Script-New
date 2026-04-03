#pragma once

#include "../../../core/Vector.h"

#include <cmath>
#include <limits>

namespace SDK {
    using Vector4 = Vec4;
}

namespace SDK::Extensions {

    using ProjectionInfo = SDK::Geometry::ProjectionInfo;
    using IntersectionResult = SDK::Geometry::IntersectionResult;
    using Vector4 = Vec4;

    /// <summary>
    ///     Holds info for the VectorMovementCollision method.
    ///     Port of EnsoulSharp.SDK MovementCollisionInfo struct.
    /// </summary>
    struct MovementCollisionInfo {
        float CollisionTime = std::numeric_limits<float>::quiet_NaN();
        Vec2  CollisionPosition = {};

        MovementCollisionInfo() = default;
        MovementCollisionInfo(float time, const Vec2& pos)
            : CollisionTime(time), CollisionPosition(pos) {}
    };

} // namespace SDK::Extensions
