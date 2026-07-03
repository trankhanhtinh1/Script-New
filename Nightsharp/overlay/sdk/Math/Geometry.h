#pragma once

// ============================================================================
// Geometry.h - Geometry static methods and small source/DLL compatibility APIs
// Ported from EnsoulSharp.SDK DLL: Geometry class (polygon operations)
//
// Source Geometry.cs has rendering helpers (GetCenter, GetCenteredText with
// SharpDX.Direct3D9 Sprite/Font). NightSharp ports the math-only centering core
// and leaves text measuring/rendering to the UI layer. DLL adds these polygon
// operations:
//   - CenterOfPolygone, ClipPolygons, JoinPolygons, MovePolygone
//   - RotatePolygon, ToPolygon, ToPolygons, Close
//   - DegreeToRadian, RadianToDegree
//   - GetCenter/GetCenteredText math-only compatibility helpers
//
// Path helpers (PathLength, CutPath, VectorMovementCollision) live in
// SDK::Utils::MathUtils and Vec2Ext - do not duplicate here.
//
// Dependencies:
//   - Core/Vector.h (Vec2, Vec3)
//   - Polygons/Polygon.h (SDK::Polygon, SDK::Clipper)
//   - Polygons/SectorPoly.h (SDK::Geometry::Sector facade)
// ============================================================================

#include "../../Core/Vector.h"
#include "../Enumerations/CenteredFlags.h"
#include "Polygons/Polygon.h"
#include "Polygons/SectorPoly.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace SDK::Geometry {

using Sector = ::SDK::SectorPoly;

struct ScreenRectangle {
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;

    ScreenRectangle() = default;
    ScreenRectangle(float x, float y, float width, float height)
        : X(x), Y(y), Width(width), Height(height) {}
};

// ============================================================================
// Source Geometry.cs rendering helper core.
// C# overloads measure text through SharpDX Font/Sprite first; C++ callers pass
// an already measured text size so Math does not depend on the rendering layer.
// ============================================================================
inline Vec2 GetCenter(
    float x, float y, float width, float height,
    float textWidth, float textHeight,
    CenteredFlags flags)
{
    Vec2 result;

    if (HasFlag(flags, CenteredFlags::HorizontalLeft)) {
        result.x = x;
    } else if (HasFlag(flags, CenteredFlags::HorizontalCenter)) {
        result.x = x + width * 0.5f - textWidth * 0.5f;
    } else if (HasFlag(flags, CenteredFlags::HorizontalRight)) {
        result.x = x + width - textWidth;
    }

    if (HasFlag(flags, CenteredFlags::VerticalUp)) {
        result.y = y;
    } else if (HasFlag(flags, CenteredFlags::VerticalCenter)) {
        result.y = y + height * 0.5f - textHeight * 0.5f;
    } else if (HasFlag(flags, CenteredFlags::VerticalDown)) {
        result.y = y + height - textHeight;
    }

    return result;
}

inline Vec2 GetCenter(
    float x, float y, float width, float height,
    const Vec2& textDimension,
    CenteredFlags flags)
{
    return GetCenter(x, y, width, height, textDimension.x, textDimension.y, flags);
}

inline Vec2 GetCenter(
    const ScreenRectangle& rectangle,
    const Vec2& textDimension,
    CenteredFlags flags)
{
    return GetCenter(rectangle.X, rectangle.Y, rectangle.Width, rectangle.Height, textDimension, flags);
}

template <typename TRectangle>
inline Vec2 GetCenter(
    const TRectangle& rectangle,
    const Vec2& textDimension,
    CenteredFlags flags)
{
    return GetCenter(
        static_cast<float>(rectangle.X),
        static_cast<float>(rectangle.Y),
        static_cast<float>(rectangle.Width),
        static_cast<float>(rectangle.Height),
        textDimension,
        flags);
}

template <typename TRectangle, typename TSprite, typename TDimensions>
inline Vec2 GetCenter(
    const TRectangle& rectangle,
    const TSprite&,
    const TDimensions& dimensions,
    CenteredFlags flags)
{
    return GetCenter(
        static_cast<float>(rectangle.X),
        static_cast<float>(rectangle.Y),
        static_cast<float>(rectangle.Width),
        static_cast<float>(rectangle.Height),
        static_cast<float>(dimensions.Width),
        static_cast<float>(dimensions.Height),
        flags);
}

inline Vec2 GetCenteredText(
    float x, float y, float width, float height,
    const Vec2& textDimension,
    CenteredFlags flags)
{
    return GetCenter(x, y, width, height, textDimension, flags);
}

inline Vec2 GetCenteredText(
    float x, float y, float width, float height,
    float textWidth, float textHeight,
    CenteredFlags flags)
{
    return GetCenter(x, y, width, height, textWidth, textHeight, flags);
}

inline Vec2 GetCenteredText(
    const ScreenRectangle& rectangle,
    const Vec2& textDimension,
    CenteredFlags flags)
{
    return GetCenter(rectangle, textDimension, flags);
}

inline Vec2 GetCenteredText(
    const ScreenRectangle& rectangle,
    float textWidth, float textHeight,
    CenteredFlags flags)
{
    return GetCenter(rectangle.X, rectangle.Y, rectangle.Width, rectangle.Height, textWidth, textHeight, flags);
}

template <typename TRectangle>
inline Vec2 GetCenteredText(
    const TRectangle& rectangle,
    const Vec2& textDimension,
    CenteredFlags flags)
{
    return GetCenter(rectangle, textDimension, flags);
}

template <typename TRectangle>
inline Vec2 GetCenteredText(
    const TRectangle& rectangle,
    float textWidth, float textHeight,
    CenteredFlags flags)
{
    return GetCenter(
        static_cast<float>(rectangle.X),
        static_cast<float>(rectangle.Y),
        static_cast<float>(rectangle.Width),
        static_cast<float>(rectangle.Height),
        textWidth,
        textHeight,
        flags);
}

// ============================================================================
// Angle conversion helpers
// ============================================================================
inline float DegreeToRadian(float degree) {
    return degree * (3.14159265358979323846f / 180.0f);
}

inline float RadianToDegree(float radian) {
    return radian * (180.0f / 3.14159265358979323846f);
}

// ============================================================================
// ToPolygon — convert Clipper IntPoint path to SDK::Polygon
// DLL: Geometry.ToPolygon(List<IntPoint> path)
// ============================================================================
inline Polygon ToPolygon(const std::vector<Clipper::IntPoint>& path) {
    Polygon result;
    for (const auto& pt : path) {
        result.Add(Vec2(static_cast<float>(pt.X), static_cast<float>(pt.Y)));
    }
    return result;
}

// ============================================================================
// ToPolygons — convert list of Clipper IntPoint paths to list of SDK::Polygon
// DLL: Geometry.ToPolygons(List<List<IntPoint>> paths)
// ============================================================================
inline std::vector<Polygon> ToPolygons(const std::vector<std::vector<Clipper::IntPoint>>& paths) {
    std::vector<Polygon> result;
    result.reserve(paths.size());
    for (const auto& path : paths) {
        result.push_back(ToPolygon(path));
    }
    return result;
}

// ============================================================================
// CenterOfPolygone — returns centroid of polygon points
// DLL: Geometry.CenterOfPolygone(Polygon poly)
// ============================================================================
inline Vec2 CenterOfPolygone(const Polygon& poly) {
    if (poly.Points.empty()) {
        return {};
    }

    float sumX = 0.0f;
    float sumY = 0.0f;
    for (const auto& pt : poly.Points) {
        sumX += pt.x;
        sumY += pt.y;
    }
    return Vec2(sumX / static_cast<float>(poly.Points.size()),
                sumY / static_cast<float>(poly.Points.size()));
}

// ============================================================================
// Close — ensure polygon is closed (first point == last point)
// DLL: Geometry.Close(Polygon poly)
// ============================================================================
inline Polygon Close(const Polygon& poly) {
    Polygon result = poly;
    if (result.Points.size() >= 2
        && result.Points.front() != result.Points.back())
    {
        result.Add(result.Points.front());
    }
    return result;
}

// ============================================================================
// MovePolygone — translate polygon by offset vector
// DLL: Geometry.MovePolygone(Polygon poly, Vector2 offset)
// ============================================================================
inline Polygon MovePolygone(const Polygon& poly, const Vec2& offset) {
    Polygon result;
    result.Points.reserve(poly.Points.size());
    for (const auto& pt : poly.Points) {
        result.Add(pt + offset);
    }
    return result;
}

// ============================================================================
// RotatePolygon — rotate polygon around a center point by angle (radians)
// DLL: Geometry.RotatePolygon(Polygon poly, Vector2 center, float angle)
// ============================================================================
inline Polygon RotatePolygon(const Polygon& poly, const Vec2& center, float angle) {
    const float cos = std::cos(angle);
    const float sin = std::sin(angle);

    Polygon result;
    result.Points.reserve(poly.Points.size());
    for (const auto& pt : poly.Points) {
        const float dx = pt.x - center.x;
        const float dy = pt.y - center.y;
        result.Add(Vec2(
            center.x + dx * cos - dy * sin,
            center.y + dx * sin + dy * cos
        ));
    }
    return result;
}

// ============================================================================
// RotatePolygon — rotate polygon around its centroid by angle (radians)
// DLL: Geometry.RotatePolygon(Polygon poly, float angle)
// ============================================================================
inline Polygon RotatePolygon(const Polygon& poly, float angle) {
    return RotatePolygon(poly, CenterOfPolygone(poly), angle);
}

// ============================================================================
// ClipPolygons — clip subject polygon against clip polygon
// DLL: Geometry.ClipPolygons(Polygon subject, Polygon clip)
// Uses Sutherland-Hodgman algorithm for convex clip polygon
// ============================================================================
inline Polygon ClipPolygons(const Polygon& subject, const Polygon& clip) {
    if (clip.Points.size() < 3) return subject;

    std::vector<Vec2> output = subject.Points;

    // Sutherland-Hodgman: clip against each edge of the clip polygon
    for (size_t i = 0; i < clip.Points.size(); ++i) {
        if (output.empty()) break;

        const Vec2& clipA = clip.Points[i];
        const Vec2& clipB = clip.Points[(i + 1) % clip.Points.size()];

        std::vector<Vec2> input = std::move(output);
        output.clear();

        for (size_t j = 0; j < input.size(); ++j) {
            const Vec2& current = input[j];
            const Vec2& next = input[(j + 1) % input.size()];

            // Cross product to determine which side of clip edge
            auto isInside = [&](const Vec2& p) {
                return (clipB.x - clipA.x) * (p.y - clipA.y)
                     - (clipB.y - clipA.y) * (p.x - clipA.x) >= 0.0f;
            };

            // Line segment intersection
            auto intersect = [&](const Vec2& a, const Vec2& b) {
                float dx1 = clipB.x - clipA.x;
                float dy1 = clipB.y - clipA.y;
                float dx2 = b.x - a.x;
                float dy2 = b.y - a.y;
                float denom = dx1 * dy2 - dy1 * dx2;
                if (std::abs(denom) < 1e-6f) return a;
                float t = ((clipA.x - a.x) * dy1 - (clipA.y - a.y) * dx1) / denom;
                return Vec2(a.x + t * dx2, a.y + t * dy2);
            };

            bool currentInside = isInside(current);
            bool nextInside = isInside(next);

            if (currentInside) {
                output.push_back(current);
                if (!nextInside) {
                    output.push_back(intersect(current, next));
                }
            } else if (nextInside) {
                output.push_back(intersect(current, next));
            }
        }
    }

    Polygon result;
    for (const auto& pt : output) {
        result.Add(pt);
    }
    return result;
}

// ============================================================================
// JoinPolygons — merge two polygons into one (concatenate points)
// DLL: Geometry.JoinPolygons(Polygon poly1, Polygon poly2)
// ============================================================================
inline Polygon JoinPolygons(const Polygon& poly1, const Polygon& poly2) {
    Polygon result = poly1;
    result.Add(poly2);
    return result;
}

// ============================================================================
// JoinPolygons — merge list of polygons into one
// DLL: Geometry.JoinPolygons(List<Polygon> polygons)
// ============================================================================
inline Polygon JoinPolygons(const std::vector<Polygon>& polygons) {
    Polygon result;
    for (const auto& poly : polygons) {
        result.Add(poly);
    }
    return result;
}

} // namespace SDK::Geometry
