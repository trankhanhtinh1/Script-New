#pragma once

#include "Generic.h"
#include "../../Core/Objects.h"
#include "../../GameObjects/ObjectManager.h"
#include "../../UI/Drawing.h"
#include "../../../imgui/imgui.h"

#include <cfloat>
#include <cmath>
#include <vector>

namespace SDK::Extensions {

    namespace detail {
        inline Vector3 ToWorldVector2(const Vector2& vector2, float y = 0.0f) {
            return Vector3(vector2.x, y, vector2.y);
        }
    }

    inline float Polar(const Vector2& vector2) {
        if (std::fabs(vector2.x) <= 1e-9f) {
            return (vector2.y > 0.0f) ? 90.0f : (vector2.y < 0.0f) ? 270.0f : 0.0f;
        }

        float theta = std::atan2(vector2.y, vector2.x) * (180.0f / 3.14159265358979323846f);
        if (theta < 0.0f) {
            theta += 360.0f;
        }
        return theta;
    }

    inline float AngleBetween(const Vector2& vector2, const Vector2& toVector2) {
        float theta = Polar(vector2) - Polar(toVector2);
        if (theta < 0.0f) {
            theta += 360.0f;
        }
        if (theta > 180.0f) {
            theta = 360.0f - theta;
        }
        return theta;
    }

    inline float AngleBetween(const Vector2& vector2, const Vector3& toVector3) {
        return AngleBetween(vector2, toVector3.To2D());
    }

    inline std::vector<Vector2> CircleCircleIntersection(const Vector2& center1,
                                                         const Vector2& center2,
                                                         float radius1,
                                                         float radius2) {
        const float distance = center1.Distance(center2);
        if (distance > radius1 + radius2 || distance <= std::fabs(radius1 - radius2) || distance <= 1e-6f) {
            return {};
        }

        const float a = ((radius1 * radius1) - (radius2 * radius2) + (distance * distance)) / (2.0f * distance);
        const float h = std::sqrt((radius1 * radius1) - (a * a));
        const Vector2 direction = (center2 - center1).Normalized();
        const Vector2 pa = center1 + (direction * a);
        return {
            pa + (direction.Perpendicular() * h),
            pa - (direction.Perpendicular() * h)
        };
    }

    inline Vector2 Closest(const Vector2& vector2, const std::vector<Vector2>& array) {
        Vector2 result = {};
        float distance = FLT_MAX;
        for (const auto& vector : array) {
            const float temporaryDistance = vector2.Distance(vector);
            if (temporaryDistance < distance) {
                distance = temporaryDistance;
                result = vector;
            }
        }
        return result;
    }

    inline Vector3 Closest(const Vector2& vector2, const std::vector<Vector3>& array) {
        Vector3 result = {};
        float distance = FLT_MAX;
        for (const auto& vector : array) {
            const float temporaryDistance = vector2.Distance(vector.To2D());
            if (temporaryDistance < distance) {
                distance = temporaryDistance;
                result = vector;
            }
        }
        return result;
    }

    inline int CountAllyHeroesInRange(const Vector2& vector2, float range, const AIBaseClient& originalUnit = AIBaseClient()) {
        int count = 0;
        for (const auto& hero : ObjectManager::AllyHeroes()) {
            if (!hero.IsValid() || hero.IsDead() || hero.IsInvulnerable()) {
                continue;
            }
            if (originalUnit.IsValid() && hero.Compare(originalUnit)) {
                continue;
            }
            if (hero.Position().To2D().DistanceSqr(vector2) <= (range * range)) {
                ++count;
            }
        }
        return count;
    }

    inline int CountEnemyHeroesInRange(const Vector2& vector2, float range, const AIBaseClient& originalUnit = AIBaseClient()) {
        int count = 0;
        for (const auto& hero : ObjectManager::EnemyHeroes()) {
            if (!hero.IsValid() || hero.IsDead() || hero.IsInvulnerable()) {
                continue;
            }
            if (originalUnit.IsValid() && hero.Compare(originalUnit)) {
                continue;
            }
            if (hero.Position().To2D().DistanceSqr(vector2) <= (range * range)) {
                ++count;
            }
        }
        return count;
    }

    inline float CrossProduct(const Vector2& self, const Vector2& other) {
        return self.Cross(other);
    }

    inline float Distance(const Vector2& vector2, const Vector2& toVector2) {
        return vector2.Distance(toVector2);
    }

    inline float Distance(const Vector2& vector2, const Vector3& toVector3) {
        return vector2.Distance(toVector3.To2D());
    }

    inline float Distance(const Vector2& point, const Vector2& segmentStart, const Vector2& segmentEnd, bool onlyIfOnSegment = false) {
        const auto projection = SDK::Geometry::ProjectOn(point, segmentStart, segmentEnd);
        return (projection.isOnSegment || !onlyIfOnSegment) ? projection.segmentPoint.Distance(point) : FLT_MAX;
    }

    inline float DistanceSquared(const Vector2& vector2, const Vector2& toVector2) {
        return vector2.DistanceSqr(toVector2);
    }

    inline float DistanceSquared(const Vector2& vector2, const Vector3& toVector3) {
        return vector2.DistanceSqr(toVector3.To2D());
    }

    inline float DistanceSquared(const Vector2& point, const Vector2& segmentStart, const Vector2& segmentEnd, bool onlyIfOnSegment = false) {
        const auto projection = SDK::Geometry::ProjectOn(point, segmentStart, segmentEnd);
        return (projection.isOnSegment || !onlyIfOnSegment) ? projection.segmentPoint.DistanceSqr(point) : FLT_MAX;
    }

    inline Vector2 Extend(const Vector2& vector2, const Vector2& toVector2, float distance) {
        return vector2.Extend(toVector2, distance);
    }

    inline Vector2 Extend(const Vector2& vector2, const Vector3& toVector3, float distance) {
        return vector2.Extend(toVector3.To2D(), distance);
    }

    inline float GetPathLength(const std::vector<Vector2>& path) {
        float distance = 0.0f;
        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            distance += path[i].Distance(path[i + 1]);
        }
        return distance;
    }

    inline IntersectionResult Intersection(const Vector2& lineSegment1Start,
                                           const Vector2& lineSegment1End,
                                           const Vector2& lineSegment2Start,
                                           const Vector2& lineSegment2End) {
        return SDK::Geometry::SegmentIntersection(lineSegment1Start, lineSegment1End, lineSegment2Start, lineSegment2End);
    }

    inline bool IsOnScreen(const Vector2& vector2, float radius = 0.0f) {
        Vector2 screen = {};
        if (!Drawing::WorldToScreen(detail::ToWorldVector2(vector2), screen)) {
            return false;
        }

        const auto display = ImGui::GetIO().DisplaySize;
        return !(screen.x + radius < 0.0f) && !(screen.x - radius > display.x) &&
               !(screen.y + radius < 0.0f) && !(screen.y - radius > display.y);
    }

    inline bool IsOrthogonal(const Vector2& vector2, const Vector2& toVector2) {
        return std::fabs(vector2.Dot(toVector2)) < 1e-6f;
    }

    inline bool IsOrthogonal(const Vector2& vector2, const Vector3& toVector3) {
        return IsOrthogonal(vector2, toVector3.To2D());
    }

    inline bool IsUnderAllyTurret(const Vector2& position) {
        for (const auto& turret : ObjectManager::AllyTurrets()) {
            if (!turret.IsDead() && turret.Position().To2D().Distance(position) < 950.0f) {
                return true;
            }
        }
        return false;
    }

    inline bool IsUnderEnemyTurret(const Vector2& position) {
        for (const auto& turret : ObjectManager::EnemyTurrets()) {
            if (!turret.IsDead() && turret.Position().To2D().Distance(position) < 950.0f) {
                return true;
            }
        }
        return false;
    }

    inline bool IsUnderRectangle(const Vector2& point, float x, float y, float width, float height) {
        return point.x > x && point.x < x + width && point.y > y && point.y < y + height;
    }

    inline bool IsValid(const Vector2& vector2) {
        return vector2.IsValid() && !vector2.IsZero();
    }

    inline bool IsWall(const Vector2& vector2) {
        return CoreAPI::NavGrid::IsWall(detail::ToWorldVector2(vector2));
    }

    inline float Magnitude(const Vector2& vector2) {
        return vector2.Length();
    }

    inline Vector2 Normalized(const Vector2& vector2) {
        return vector2.Normalized();
    }

    inline float PathLength(const std::vector<Vector2>& path) {
        return GetPathLength(path);
    }

    inline Vector2 Perpendicular(const Vector2& vector2, int offset = 0) {
        return (offset == 0) ? Vector2(-vector2.y, vector2.x) : Vector2(vector2.y, -vector2.x);
    }

    inline ProjectionInfo ProjectOn(const Vector2& point, const Vector2& segmentStart, const Vector2& segmentEnd) {
        return SDK::Geometry::ProjectOn(point, segmentStart, segmentEnd);
    }

    inline Vector2 Rotated(const Vector2& vector2, float angle) {
        return vector2.Rotated(angle);
    }

    inline Vector3 ToVector3(const Vector2& vector2, float y = 0.0f) {
        return detail::ToWorldVector2(vector2, y);
    }

    inline std::vector<Vector3> ToVector3(const std::vector<Vector2>& path, float y = 0.0f) {
        std::vector<Vector3> result = {};
        result.reserve(path.size());
        for (const auto& point : path) {
            result.push_back(ToVector3(point, y));
        }
        return result;
    }

    inline Vector4 ToVector4(const Vector2& vector2, float z = 0.0f, float w = 1.0f) {
        return Vector4(vector2.x, z, vector2.y, w);
    }

    inline std::vector<Vector4> ToVector4(const std::vector<Vector2>& path, float z = 0.0f, float w = 1.0f) {
        std::vector<Vector4> result = {};
        result.reserve(path.size());
        for (const auto& point : path) {
            result.push_back(ToVector4(point, z, w));
        }
        return result;
    }

} // namespace SDK::Extensions
