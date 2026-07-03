#pragma once

// ============================================================================
// LinePoly.h - 1:1 port of EnsoulSharp.SDK / Core/Math/Polygons/LinePoly.cs
// ----------------------------------------------------------------------------
// Represents a Line Polygon (2 points). The Length property is computed from
// LineStart/LineEnd; setting Length re-extends LineEnd along the start->end
// direction so the segment matches the requested length.
// ============================================================================

#include "Polygon.h"

namespace SDK {

    class LinePoly : public Polygon {
    public:
        Vec2 LineStart = {};
        Vec2 LineEnd   = {};

        // length=-1 keeps the original distance between start/end.
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

        // Distance between LineStart and LineEnd (does not need an update).
        float GetLength() const {
            return LineStart.Distance(LineEnd);
        }

        // Re-extend LineEnd so that the segment has the requested length.
        void SetLength(float value) {
            LineEnd = ((LineEnd - LineStart).Normalized() * value) + LineStart;
        }

        // Use this after changing something.
        void UpdatePolygon() {
            Points.clear();
            Points.push_back(LineStart);
            Points.push_back(LineEnd);
        }
    };

} // namespace SDK
