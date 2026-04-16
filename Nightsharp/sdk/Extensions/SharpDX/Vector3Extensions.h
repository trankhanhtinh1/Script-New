#pragma once

#include "Generic.h"
#include "Vector2Extensions.h"
#include "../../Core/Objects.h"
#include "../../UI/Drawing.h"
#include "../../../imgui/imgui.h"

#include <vector>

namespace SDK::Extensions {

    inline float AngleBetween(const Vector3& vector3, const Vector3& toVector3) {
        return AngleBetween(vector3.To2D(), toVector3.To2D());
    }

    inline float AngleBetween(const Vector3& vector3, const Vector2& toVector2) {
        return AngleBetween(vector3.To2D(), toVector2);
    }

    inline Vector3 Closest(const Vector3& vector3, const std::vector<Vector3>& array) {
        return Closest(vector3.To2D(), array);
    }

    inline Vector2 Closest(const Vector3& vector3, const std::vector<Vector2>& array) {
        return Closest(vector3.To2D(), array);
    }

    inline int CountAllyHeroesInRange(const Vector3& vector3, float range, const AIBaseClient& originalUnit = AIBaseClient()) {
        return CountAllyHeroesInRange(vector3.To2D(), range, originalUnit);
    }

    inline int CountEnemyHeroesInRange(const Vector3& vector3, float range, const AIBaseClient& originalUnit = AIBaseClient()) {
        return CountEnemyHeroesInRange(vector3.To2D(), range, originalUnit);
    }

    inline float Distance(const Vector3& vector3, const Vector3& toVector3) {
        return vector3.Distance2D(toVector3);
    }

    inline float Distance(const Vector3& vector3, const Vector2& toVector2) {
        return vector3.To2D().Distance(toVector2);
    }

    inline float DistanceSquared(const Vector3& vector3, const Vector3& toVector3) {
        return vector3.DistanceSqr2D(toVector3);
    }

    inline float DistanceSquared(const Vector3& vector3, const Vector2& toVector2) {
        return vector3.To2D().DistanceSqr(toVector2);
    }

    inline Vector3 Extend(const Vector3& vector3, const Vector3& toVector3, float distance) {
        return vector3.Extend(toVector3, distance);
    }

    inline Vector3 Extend(const Vector3& vector3, const Vector2& toVector2, float distance) {
        return vector3.Extend(Vector3(toVector2.x, vector3.y, toVector2.y), distance);
    }

    inline float GetPathLength(const std::vector<Vector3>& path) {
        return SDK::Geometry::PathLength(path);
    }

    inline bool IsOnScreen(const Vector3& vector3, float radius = 0.0f) {
        Vector2 screen = {};
        if (!Drawing::WorldToScreen(vector3, screen)) {
            return false;
        }

        const auto display = ImGui::GetIO().DisplaySize;
        return !(screen.x + radius < 0.0f) && !(screen.x - radius > display.x) &&
               !(screen.y + radius < 0.0f) && !(screen.y - radius > display.y);
    }

    inline bool IsOrthogonal(const Vector3& vector3, const Vector3& toVector3) {
        return IsOrthogonal(vector3.To2D(), toVector3.To2D());
    }

    inline bool IsOrthogonal(const Vector3& vector3, const Vector2& toVector2) {
        return IsOrthogonal(vector3.To2D(), toVector2);
    }

    inline bool IsUnderAllyTurret(const Vector3& position) {
        return IsUnderAllyTurret(position.To2D());
    }

    inline bool IsUnderEnemyTurret(const Vector3& position) {
        return IsUnderEnemyTurret(position.To2D());
    }

    inline bool IsValid(const Vector3& vector3) {
        return vector3.IsValid() && !vector3.IsZero();
    }

    inline bool IsWall(const Vector3& vector3) {
        return CoreAPI::NavGrid::IsWall(vector3);
    }

    inline float Magnitude(const Vector3& vector3) {
        return vector3.Length();
    }

    inline Vector3 Normalized(const Vector3& vector3) {
        return vector3.Normalized();
    }

    inline float PathLength(const std::vector<Vector3>& path) {
        return GetPathLength(path);
    }

    inline Vector3 Perpendicular(const Vector3& vector3, int offset = 0) {
        return (offset == 0)
            ? Vector3(-vector3.z, vector3.y, vector3.x)
            : Vector3(vector3.z, vector3.y, -vector3.x);
    }

    inline float Polar(const Vector3& vector3) {
        return Polar(vector3.To2D());
    }

    inline ProjectionInfo ProjectOn(const Vector3& point, const Vector3& segmentStart, const Vector3& segmentEnd) {
        return SDK::Geometry::ProjectOn(point.To2D(), segmentStart.To2D(), segmentEnd.To2D());
    }

    inline Vector3 Rotated(const Vector3& vector3, float angle) {
        return vector3.Rotated(angle);
    }

    inline Vector2 ToVector2(const Vector3& vector3) {
        return vector3.To2D();
    }

    inline std::vector<Vector2> ToVector2(const std::vector<Vector3>& path) {
        std::vector<Vector2> result = {};
        result.reserve(path.size());
        for (const auto& point : path) {
            result.push_back(ToVector2(point));
        }
        return result;
    }

    inline Vector4 ToVector4(const Vector3& vector3, float w = 1.0f) {
        return Vector4(vector3.x, vector3.y, vector3.z, w);
    }

    inline std::vector<Vector4> ToVector4(const std::vector<Vector3>& path, float w = 1.0f) {
        std::vector<Vector4> result = {};
        result.reserve(path.size());
        for (const auto& point : path) {
            result.push_back(ToVector4(point, w));
        }
        return result;
    }

} // namespace SDK::Extensions
