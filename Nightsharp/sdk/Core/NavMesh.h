#pragma once

#include "../../Core/CoreNavGrid.h"
#include "../Core/Objects.h"
#include "../Enumerations/CollisionFlags.h"

#include <cstdint>

namespace SDK::NavMesh {

struct NavMeshCell {
    NavMeshCell() = default;
    NavMeshCell(int xValue, int yValue) : x(xValue), y(yValue) {}

    int GridX() const { return x; }
    int GridY() const { return y; }

    Vector3 WorldPosition() const {
        return ::CoreNavGrid::GridToWorld(x, y);
    }

    CollisionFlags CollFlags() const {
        return static_cast<CollisionFlags>(
            ::CoreNavGrid::Get().GetCollisionFlags(x, y));
    }

    bool SetCollFlags(CollisionFlags flags) const {
        return ::CoreNavGrid::Get().SetCollisionFlags(
            x,
            y,
            ::SDK::ToMask(flags));
    }

    bool IsWater() const {
        return ::CoreNavGrid::RawHasWater(
            ::CoreNavGrid::Get().GetRawCellFlags(x, y));
    }

    int x = 0;
    int y = 0;
};

inline NavMeshCell GetCell(const Vector2& position) {
    return { static_cast<int>(position.x), static_cast<int>(position.y) };
}

inline NavMeshCell GetCell(int x, int y) {
    return { x, y };
}

inline CollisionFlags GetCollisionFlags(float x, float y) {
    return static_cast<CollisionFlags>(
        ::CoreNavGrid::GetCollisionFlags({ x, 0.0f, y }));
}

inline CollisionFlags GetCollisionFlags(const Vector3& position) {
    return static_cast<CollisionFlags>(
        ::CoreNavGrid::GetCollisionFlags(position));
}

inline std::uint16_t GetRawCellFlags(int x, int y) {
    return ::CoreNavGrid::Get().GetRawCellFlags(x, y);
}

inline std::uint16_t GetRawCellFlags(const Vector3& position) {
    return ::CoreNavGrid::GetRawCellFlags(position);
}

inline bool SetCollisionFlags(CollisionFlags flags, float x, float y) {
    return ::CoreNavGrid::SetCollisionFlags({ x, 0.0f, y }, ::SDK::ToMask(flags));
}

inline bool SetCollisionFlags(CollisionFlags flags, const Vector3& position) {
    return ::CoreNavGrid::SetCollisionFlags(position, ::SDK::ToMask(flags));
}

inline bool IsWallOfType(float x, float y, CollisionFlags flags, float radius) {
    return ::CoreNavGrid::IsWallOfType({ x, 0.0f, y }, ::SDK::ToMask(flags), radius);
}

inline bool IsWallOfType(const Vector3& position, CollisionFlags flags, float radius) {
    return ::CoreNavGrid::IsWallOfType(position, ::SDK::ToMask(flags), radius);
}

inline bool IsWater(float x, float y) {
    return ::CoreNavGrid::IsWater({ x, 0.0f, y });
}

inline bool IsWater(const Vector3& position) {
    return ::CoreNavGrid::IsWater(position);
}

inline bool IsWall(float x, float y) {
    return ::CoreNavGrid::IsWall({ x, 0.0f, y });
}

inline bool IsWall(const Vector3& position) {
    return ::CoreNavGrid::IsWall(position);
}

inline bool IsWalkable(float x, float y) {
    return ::CoreNavGrid::IsWalkable({ x, 0.0f, y });
}

inline bool IsWalkable(const Vector3& position) {
    return ::CoreNavGrid::IsWalkable(position);
}

inline bool IsWallBetween(const Vector3& from,
                          const Vector3& to,
                          float step = 40.0f) {
    return ::CoreNavGrid::IsWallBetween(from, to, step);
}

inline bool FindWallCollision(const Vector3& from,
                              const Vector3& to,
                              Vector3& hitPoint,
                              float step = 10.0f) {
    return ::CoreNavGrid::FindWallCollision(from, to, hitPoint, step);
}

inline Vector2 WorldToGrid(float x, float y) {
    const auto point = ::CoreNavGrid::WorldToGrid({ x, 0.0f, y });
    return { static_cast<float>(point.x), static_cast<float>(point.y) };
}

inline Vector2 WorldToGrid(const Vector3& position) {
    const auto point = ::CoreNavGrid::WorldToGrid(position);
    return { static_cast<float>(point.x), static_cast<float>(point.y) };
}

inline Vector3 GridToWorld(int x, int y) {
    return ::CoreNavGrid::GridToWorld(x, y);
}

inline Vector3 GridToWorld(const Vector2& position) {
    return GridToWorld(static_cast<int>(position.x), static_cast<int>(position.y));
}

inline float GetHeightForPosition(float x, float y) {
    return ::CoreNavGrid::GetHeightForPosition(x, y);
}

inline float GetHeightForPosition(const Vector3& position) {
    return GetHeightForPosition(position.x, position.z);
}

} // namespace SDK::NavMesh

namespace SDK::Core {
namespace NavMesh = ::SDK::NavMesh;
}
