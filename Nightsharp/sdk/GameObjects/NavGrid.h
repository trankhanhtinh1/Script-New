#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "core/Vector.h"
#include <cmath>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>

// ============================================================================
// NavGrid — Wall & Bush collision checks via NavigationGrid
//
// Memory layout (IDA MCP verified 2026-03-11):
//   Global::NavGrid (0x1D7DD08)  →  navGridPtr  (uint64)
//   navGridPtr + 0x8             →  NavGridManager  (uint64)
//
//   NavGridManager layout:
//     +0x0EC  → float  MinX        (mgr[59])
//     +0x0F4  → float  MinZ        (mgr[61])
//     +0x0F8  → float  MaxX        (mgr[62])
//     +0x100  → float  MaxZ        (mgr[64])
//     +0x110  → uint64 CellDataPtr (16 bytes per cell)
//     +0x158  → uint64 GrassRegionBitfield ptr
//     +0x708  → int    Width       (mgr+1800)
//     +0x70C  → int    Height      (mgr+1804)
//     +0x710  → float  CellSize    (mgr[452])
//     +0x714  → float  InvScale    (mgr+1812) = 1/cellSize
//
//   Cell structure (16 bytes):
//     [uint64_t ptrData][uint16_t flags_inline][6 bytes padding]
//     If ptrData != 0: real flags = *(uint16_t*)(ptrData + 6)
//     If ptrData == 0: real flags = flags_inline (at cell+8)
//
//   Flag bitmask (uint16_t):
//     0x0001 = Wall (terrain, not passable)
//     0x0002 = Not walkable (general)
//     0x0C00 = Brush/Grass (bits 10-11)
//     0x1000 = Special terrain
//
// Combined approach: uses BOTH GetCollisionFlags game function (accurate)
// AND manual navgrid reads (fast, for batch operations like wall proximity).
// ============================================================================

namespace SDK {

    // -------------------------------------------------------------------------
    // NavCellFlags — flag bitmask (uint16_t from IDA decompile)
    // -------------------------------------------------------------------------
    namespace NavCellFlag {
        constexpr uint16_t Wall     = 0x0001;   // bit 0: terrain/wall
        constexpr uint16_t NoWalk   = 0x0002;   // bit 1: not walkable
        constexpr uint16_t Brush    = 0x0C00;   // bits 10-11: brush/grass
        constexpr uint16_t Special  = 0x1000;   // bit 12: special terrain
    };

    // -------------------------------------------------------------------------
    // NavGrid
    // -------------------------------------------------------------------------
    class NavGrid {
    public:
        uintptr_t managerPtr = 0;
        uintptr_t cellDataPtr = 0;
        int       width = 0;
        int       height = 0;
        float     invScale = 0.0f;    // 1/cellSize — MULTIPLY to get cell index
        float     cellSize = 0.0f;    // actual cell size (for bounds check)
        float     minX = 0.0f;
        float     minZ = 0.0f;
        float     maxX = 0.0f;
        float     maxZ = 0.0f;
        bool      valid = false;

        NavGrid() = default;

        // ------------------------------------------------------------------
        // Get() — resolve from game memory (call once per frame, cache)
        // ------------------------------------------------------------------
        static NavGrid Get() {
            NavGrid ng;
            __try {
                uintptr_t navGridPtr =
                    Globals::Read<uintptr_t>(Globals::base + Offset::Global::NavGrid);
                if (!Globals::IsValidPtr(navGridPtr)) return ng;

                uintptr_t mgr =
                    Globals::Read<uintptr_t>(navGridPtr + Offset::NavGrid::NavGridMgr);
                if (!Globals::IsValidPtr(mgr)) return ng;

                ng.managerPtr = mgr;

                ng.width    = Globals::Read<int>  (mgr + Offset::NavGrid::Width);
                ng.height   = Globals::Read<int>  (mgr + Offset::NavGrid::Height);
                ng.invScale = Globals::Read<float>(mgr + Offset::NavGrid::InverseScale);
                ng.cellSize = Globals::Read<float>(mgr + Offset::NavGrid::Scale);
                ng.minX     = Globals::Read<float>(mgr + Offset::NavGrid::MinX);
                ng.minZ     = Globals::Read<float>(mgr + Offset::NavGrid::MinZ);
                ng.maxX     = Globals::Read<float>(mgr + Offset::NavGrid::MaxX);
                ng.maxZ     = Globals::Read<float>(mgr + Offset::NavGrid::MaxZ);

                uintptr_t data =
                    Globals::Read<uintptr_t>(mgr + Offset::NavGrid::Data);
                if (!Globals::IsValidPtr(data)) return ng;

                ng.cellDataPtr = data;

                // Sanity checks
                if (ng.width  <= 0 || ng.width  > 20000) return ng;
                if (ng.height <= 0 || ng.height > 20000) return ng;
                if (ng.invScale <= 0.0f) return ng;

                ng.valid = true;
            } __except(1) { ng.valid = false; }
            return ng;
        }

        bool IsValid() const { return valid; }

        // ------------------------------------------------------------------
        // WorldToCell — world Vec3 → cell (col, row)
        // Uses InverseScale (MULTIPLY) as per IDA decompile
        // ------------------------------------------------------------------
        bool WorldToCell(const Vec3& pos, int& cellX, int& cellZ) const {
            if (!valid) return false;
            cellX = static_cast<int>((pos.x - minX) * invScale);
            cellZ = static_cast<int>((pos.z - minZ) * invScale);
            // Clamp like the game does
            cellX = std::clamp(cellX, 0, width - 1);
            cellZ = std::clamp(cellZ, 0, height - 1);
            return true;
        }

        bool WorldToCell(float worldX, float worldZ, int& cellX, int& cellZ) const {
            if (!valid) return false;
            cellX = static_cast<int>((worldX - minX) * invScale);
            cellZ = static_cast<int>((worldZ - minZ) * invScale);
            cellX = std::clamp(cellX, 0, width - 1);
            cellZ = std::clamp(cellZ, 0, height - 1);
            return true;
        }

        // ------------------------------------------------------------------
        // GetCellFlags — read uint16_t flags from 16-byte cell struct
        // IDA: cell = cellDataPtr + 16 * (col + row * width)
        //   if ptrData != 0: flags = *(uint16_t*)(ptrData + 6)
        //   if ptrData == 0: flags = *(uint16_t*)(cell + 8)
        // ------------------------------------------------------------------
        uint16_t GetCellFlags(int cellX, int cellZ) const {
            if (!valid) return 0xFFFF;
            __try {
                uintptr_t cellAddr = cellDataPtr +
                    16ULL * (static_cast<uintptr_t>(cellX) +
                             static_cast<uintptr_t>(cellZ) * width);

                uintptr_t ptrData = Globals::Read<uintptr_t>(cellAddr);
                if (ptrData != 0 && Globals::IsValidPtr(ptrData)) {
                    return Globals::Read<uint16_t>(ptrData + 6);
                }
                return Globals::Read<uint16_t>(cellAddr + 8);
            } __except(1) { return 0xFFFF; }
        }

        uint16_t GetCellFlags(const Vec3& pos) const {
            int cx, cz;
            if (!WorldToCell(pos, cx, cz)) return 0xFFFF;
            return GetCellFlags(cx, cz);
        }

        // ------------------------------------------------------------------
        // IsWall — bit 0 of flags (terrain/wall)
        // ------------------------------------------------------------------
        bool IsWall(const Vec3& pos) const {
            uint16_t flags = GetCellFlags(pos);
            if (flags == 0xFFFF) return false;
            return (flags & NavCellFlag::Wall) != 0;
        }

        bool IsWall(float worldX, float worldZ) const {
            int cx, cz;
            if (!WorldToCell(worldX, worldZ, cx, cz)) return false;
            uint16_t flags = GetCellFlags(cx, cz);
            if (flags == 0xFFFF) return false;
            return (flags & NavCellFlag::Wall) != 0;
        }

        // ------------------------------------------------------------------
        // IsWalkable — (flags & 2) == 0  (from IDA sub_119C210)
        // Also checks brush pathability mask
        // ------------------------------------------------------------------
        bool IsWalkable(const Vec3& pos) const {
            uint16_t flags = GetCellFlags(pos);
            if (flags == 0xFFFF) return false;
            uint16_t brushBits = flags & NavCellFlag::Brush;
            if (brushBits != 0) {
                // Brush cells are walkable (pathable through brush)
                return true;
            }
            return (flags & NavCellFlag::NoWalk) == 0;
        }

        // ------------------------------------------------------------------
        // IsInBrush — bits 10-11 set (0x0C00)
        // ------------------------------------------------------------------
        bool IsInBrush(const Vec3& pos) const {
            uint16_t flags = GetCellFlags(pos);
            if (flags == 0xFFFF) return false;
            return (flags & NavCellFlag::Brush) != 0;
        }

        // ------------------------------------------------------------------
        // IsWallBetween — ray-step check (improved with correct cell stride)
        // ------------------------------------------------------------------
        bool IsWallBetween(const Vec3& from, const Vec3& to, float stepSize = 40.0f) const {
            if (!valid) return false;
            float dx = to.x - from.x;
            float dz = to.z - from.z;
            float dist = sqrtf(dx * dx + dz * dz);
            if (dist < 0.0001f) return IsWall(from);

            float nx = dx / dist;
            float nz = dz / dist;
            float traveled = 0.0f;

            while (traveled <= dist) {
                Vec3 sample = { from.x + nx * traveled, from.y, from.z + nz * traveled };
                if (IsWall(sample)) return true;
                traveled += stepSize;
            }
            return IsWall(to);
        }

        bool HasLineOfSight(const Vec3& from, const Vec3& to, float stepSize = 40.0f) const {
            return !IsWallBetween(from, to, stepSize);
        }

        // ------------------------------------------------------------------
        // IsNearWall — check if position is within 'radius' units of any wall
        // Useful for Evade: don't dodge INTO tight spaces near walls
        // Useful for Prediction: enemy near wall → limited escape routes
        // ------------------------------------------------------------------
        bool IsNearWall(const Vec3& pos, float radius = 80.0f) const {
            if (!valid) return false;
            int cx, cz;
            if (!WorldToCell(pos, cx, cz)) return false;

            // Check cells in a square around the position
            int cellRadius = static_cast<int>(radius * invScale) + 1;
            int minCx = std::max(0, cx - cellRadius);
            int maxCx = std::min(width - 1, cx + cellRadius);
            int minCz = std::max(0, cz - cellRadius);
            int maxCz = std::min(height - 1, cz + cellRadius);

            float radiusSq = radius * radius;

            for (int z = minCz; z <= maxCz; ++z) {
                for (int x = minCx; x <= maxCx; ++x) {
                    uint16_t flags = GetCellFlags(x, z);
                    if (flags == 0xFFFF) continue;
                    if ((flags & NavCellFlag::Wall) == 0) continue;

                    // This cell is wall — check distance
                    float worldCellX = minX + (float)x / invScale;
                    float worldCellZ = minZ + (float)z / invScale;
                    float ddx = pos.x - worldCellX;
                    float ddz = pos.z - worldCellZ;
                    if (ddx * ddx + ddz * ddz <= radiusSq) {
                        return true;
                    }
                }
            }
            return false;
        }

        // ------------------------------------------------------------------
        // GetWallProximity — how close is position to nearest wall (units)
        // Returns FLT_MAX if no wall nearby
        // ------------------------------------------------------------------
        float GetWallDistance(const Vec3& pos, float searchRadius = 300.0f) const {
            if (!valid) return 999999.0f;
            int cx, cz;
            if (!WorldToCell(pos, cx, cz)) return 999999.0f;

            int cellRadius = static_cast<int>(searchRadius * invScale) + 1;
            int minCx = std::max(0, cx - cellRadius);
            int maxCx = std::min(width - 1, cx + cellRadius);
            int minCz = std::max(0, cz - cellRadius);
            int maxCz = std::min(height - 1, cz + cellRadius);

            float bestDistSq = searchRadius * searchRadius + 1.0f;

            for (int z = minCz; z <= maxCz; ++z) {
                for (int x = minCx; x <= maxCx; ++x) {
                    uint16_t flags = GetCellFlags(x, z);
                    if (flags == 0xFFFF) continue;
                    if ((flags & NavCellFlag::Wall) == 0) continue;

                    float worldCellX = minX + (float)x / invScale;
                    float worldCellZ = minZ + (float)z / invScale;
                    float ddx = pos.x - worldCellX;
                    float ddz = pos.z - worldCellZ;
                    float distSq = ddx * ddx + ddz * ddz;
                    if (distSq < bestDistSq) {
                        bestDistSq = distSq;
                    }
                }
            }
            return sqrtf(bestDistSq);
        }

        // ------------------------------------------------------------------
        // CountWalkableDirections — how many escape directions from position
        // Returns 0-8 (8 directions checked at 'distance' units away)
        // Useful for Prediction: if enemy has few escape routes → high hit chance
        // ------------------------------------------------------------------
        int CountWalkableDirections(const Vec3& pos, float distance = 100.0f) const {
            if (!valid) return 8;
            int count = 0;
            static const float angles[] = { 0, 45, 90, 135, 180, 225, 270, 315 };
            static constexpr float DEG2RAD = 3.14159265f / 180.0f;

            for (float angle : angles) {
                float rad = angle * DEG2RAD;
                Vec3 check = {
                    pos.x + cosf(rad) * distance,
                    pos.y,
                    pos.z + sinf(rad) * distance
                };
                if (!IsWall(check)) {
                    count++;
                }
            }
            return count;
        }

        // ------------------------------------------------------------------
        // IsInBounds — is world position inside the map boundaries
        // ------------------------------------------------------------------
        bool IsInBounds(const Vec3& pos) const {
            if (!valid) return false;
            return pos.x >= minX && pos.x <= maxX &&
                   pos.z >= minZ && pos.z <= maxZ;
        }

        // ------------------------------------------------------------------
        // GetSafeNearWallPosition — push position away from wall
        // For Evade: ensure dodge point maintains buffer from wall
        // ------------------------------------------------------------------
        Vec3 PushAwayFromWall(const Vec3& pos, float buffer = 65.0f) const {
            if (!valid || !IsNearWall(pos, buffer)) return pos;

            // Find the direction AWAY from the nearest wall cells
            int cx, cz;
            if (!WorldToCell(pos, cx, cz)) return pos;

            int cellRadius = static_cast<int>(buffer * invScale) + 1;
            int minCx = std::max(0, cx - cellRadius);
            int maxCx = std::min(width - 1, cx + cellRadius);
            int minCz = std::max(0, cz - cellRadius);
            int maxCz = std::min(height - 1, cz + cellRadius);

            float pushX = 0.0f, pushZ = 0.0f;
            int wallCount = 0;

            for (int z = minCz; z <= maxCz; ++z) {
                for (int x = minCx; x <= maxCx; ++x) {
                    uint16_t flags = GetCellFlags(x, z);
                    if (flags == 0xFFFF) continue;
                    if ((flags & NavCellFlag::Wall) == 0) continue;

                    float worldCellX = minX + (float)x / invScale;
                    float worldCellZ = minZ + (float)z / invScale;
                    float ddx = pos.x - worldCellX;
                    float ddz = pos.z - worldCellZ;
                    float distSq = ddx * ddx + ddz * ddz;
                    if (distSq < buffer * buffer && distSq > 0.01f) {
                        float dist = sqrtf(distSq);
                        pushX += ddx / dist;
                        pushZ += ddz / dist;
                        wallCount++;
                    }
                }
            }

            if (wallCount == 0) return pos;

            float len = sqrtf(pushX * pushX + pushZ * pushZ);
            if (len < 0.01f) return pos;

            Vec3 result = pos;
            result.x += (pushX / len) * buffer * 0.5f;
            result.z += (pushZ / len) * buffer * 0.5f;

            // Make sure pushed position isn't wall itself
            if (IsWall(result)) return pos;
            return result;
        }

    }; // class NavGrid

} // namespace SDK
