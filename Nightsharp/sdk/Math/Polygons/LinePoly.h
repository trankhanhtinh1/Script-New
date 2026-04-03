#pragma once

#include "Polygon.h"

namespace SDK::Geometry {

/// <summary>
///     Represents a Line Polygon (just 2 points: start and end).
///     Port of EnsoulSharp.SDK LinePoly.cs — exact behavioral parity.
///     NOTE: This is NOT a rectangle.  It inherits directly from Polygon.
/// </summary>
class LinePoly : public Polygon {
public:
    Vec2 LineStart = {};
    Vec2 LineEnd   = {};

    LinePoly() = default;

    LinePoly(const Vec3& start, const Vec3& end, float length = -1.0f)
        : LinePoly(start.To2D(), end.To2D(), length) {}

    LinePoly(const Vec2& start, const Vec2& end, float length = -1.0f) {
        LineStart = start;
        LineEnd   = end;

        if (length > 0.0f) {
            SetLength(length);
        }

        UpdatePolygon();
    }

    /// <summary>
    ///     Get the distance between LineStart and LineEnd.
    /// </summary>
    float GetLength() const {
        return LineStart.Distance(LineEnd);
    }

    /// <summary>
    ///     Set the length by adjusting LineEnd along the (LineEnd - LineStart) direction.
    ///     Equivalent to C# Length setter: LineEnd = (LineEnd - LineStart).Normalized() * value + LineStart
    /// </summary>
    void SetLength(float value) {
        LineEnd = (LineEnd - LineStart).Normalized() * value + LineStart;
    }

    /// <summary>
    ///     Updates the polygon.  Simply clears and adds the two endpoints.
    /// </summary>
    void UpdatePolygon() {
        Points.clear();
        Points.push_back(LineStart);
        Points.push_back(LineEnd);
    }
};

} // namespace SDK::Geometry
