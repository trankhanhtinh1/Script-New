#pragma once

// ============================================================================
// SDK::NavGrid — thin SDK-facing wrapper over `CoreNavGrid`
// ============================================================================
// Consumer-friendly free functions for the most common navmesh queries.
// Internally rebuilds the `CoreNavGrid::GridRef` snapshot per call (handful
// of pointer reads, ~free) so there's no state to invalidate when the map
// changes / client reconnects.
//
// Mirrors the API surface scripts already used via `Vector3.IsWall(...)`
// extension methods, plus the prediction-layer helpers (wall collision
// along a line, wall density in a radius, near-wall check).
//
// EnsoulSharp parity:
//   - `NavMesh.IsWallOfGrass(pos)`      → `IsBrush(pos)`
//   - `NavMesh.GetCollisionFlags(pos)`  → `GetCellFlags(pos)`
//   - `NavMesh.IsWall(pos)`             → `IsWall(pos)`
// ============================================================================

#include "../../core/CoreNavGrid.h"
#include "../../core/Vector.h"

namespace SDK::NavGrid {

// ── Pointwise terrain queries ──────────────────────────────────────────────
inline bool IsWall(const Vector3& pos)        { return CoreNavGrid::IsWall(pos); }
inline bool IsBrush(const Vector3& pos)       { return CoreNavGrid::IsBrush(pos); }
inline bool IsWalkable(const Vector3& pos)    { return CoreNavGrid::IsWalkable(pos); }
inline bool IsWater(const Vector3& pos)       { return CoreNavGrid::IsWater(pos); }
inline bool IsBuilding(const Vector3& pos)    { return CoreNavGrid::IsBuilding(pos); }
inline bool HasVision(const Vector3& pos)     { return CoreNavGrid::HasVision(pos); }
inline bool IsInsideWorld(const Vector3& pos) { return CoreNavGrid::IsInsideWorld(pos); }
inline bool IsInBrushFast(const Vector3& pos) { return CoreNavGrid::IsInBrushFast(pos); }

inline uint16_t GetCellFlags(const Vector3& pos) {
    return CoreNavGrid::GetCellFlags(pos);
}

// ── 2D conveniences (read y from the grid origin plane) ────────────────────
inline bool IsWall(const Vector2& pos)     { return IsWall(Vector3(pos.x, 0.0f, pos.y)); }
inline bool IsBrush(const Vector2& pos)    { return IsBrush(Vector3(pos.x, 0.0f, pos.y)); }
inline bool IsWalkable(const Vector2& pos) { return IsWalkable(Vector3(pos.x, 0.0f, pos.y)); }
inline bool IsBuilding(const Vector2& pos) { return IsBuilding(Vector3(pos.x, 0.0f, pos.y)); }

// ── Line / radius queries (used by skillshot collision filters) ────────────
inline bool IsWallBetween(const Vector3& from, const Vector3& to, float step = 40.0f) {
    return CoreNavGrid::IsWallBetween(from, to, step);
}
inline bool FindWallCollision(const Vector3& from, const Vector3& to,
                              Vector3& hitPoint, float step = 10.0f) {
    return CoreNavGrid::FindWallCollision(from, to, hitPoint, step);
}
inline int CountWallsInRadius(const Vector3& center, float radius, float step = 25.0f) {
    return CoreNavGrid::CountWallsInRadius(center, radius, step);
}
inline bool IsNearWall(const Vector3& pos, float distance = 50.0f) {
    return CoreNavGrid::IsNearWall(pos, distance);
}

} // namespace SDK::NavGrid
