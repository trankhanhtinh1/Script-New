#pragma once

#include "CoreRuntime.h"
#include "Vector.h"

#include <algorithm>
#include <cstdint>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace CoreNavGrid {

    struct GridRef {
        uintptr_t navGrid = 0;
        uintptr_t manager = 0;
        uintptr_t cellData = 0;
        uintptr_t byteData = 0; // METHOD 1: fast byte flag array (1 byte/cell)
        int width = 0;
        int height = 0;
        float minX = 0.0f;
        float minZ = 0.0f;
        float maxX = 0.0f;
        float maxZ = 0.0f;
        float cellSize = 0.0f;
        float inverseScale = 0.0f;

        bool IsValid() const {
            return Globals::IsValidPtr(manager) &&
                   Globals::IsValidPtr(cellData) &&
                   width > 0 &&
                   height > 0 &&
                   inverseScale > 0.0f;
        }

        bool WorldToCell(const Vec3& pos, int& outX, int& outZ) const {
            if (!IsValid()) {
                outX = 0;
                outZ = 0;
                return false;
            }

            outX = static_cast<int>((pos.x - minX) * inverseScale);
            outZ = static_cast<int>((pos.z - minZ) * inverseScale);
            outX = std::clamp(outX, 0, width - 1);
            outZ = std::clamp(outZ, 0, height - 1);
            return true;
        }

        bool IsInsideWorld(const Vec3& pos) const {
            return IsValid() &&
                   pos.x >= minX && pos.x <= maxX &&
                   pos.z >= minZ && pos.z <= maxZ;
        }

        Vec3 CellToWorld(int x, int z) const {
            if (!IsValid()) {
                return {};
            }

            return {
                minX + (static_cast<float>(x) + 0.5f) * cellSize,
                0.0f,
                minZ + (static_cast<float>(z) + 0.5f) * cellSize
            };
        }

        uint16_t GetCellFlags(int x, int z) const {
            if (!IsValid() || x < 0 || z < 0 || x >= width || z >= height) {
                return 0xFFFF;
            }

            __try {
                const auto cellAddr = cellData +
                    16ULL * (static_cast<uintptr_t>(x) + static_cast<uintptr_t>(z) * static_cast<uintptr_t>(width));
                const auto cellPtr = Globals::Read<uintptr_t>(cellAddr);
                if (Globals::IsValidPtr(cellPtr)) {
                    return Globals::Read<uint16_t>(cellPtr + 6);
                }
                return Globals::Read<uint16_t>(cellAddr + 8);
            }
            __except (1) {
                return 0xFFFF;
            }
        }

        uint16_t GetCellFlags(const Vec3& pos) const {
            int x = 0;
            int z = 0;
            if (!WorldToCell(pos, x, z)) {
                return 0xFFFF;
            }
            return GetCellFlags(x, z);
        }

        bool IsWall(const Vec3& pos) const {
            const uint16_t flags = GetCellFlags(pos);
            return flags != 0xFFFF && (flags & Offset::NavGrid::FLAG_WALL) != 0;
        }

        bool IsBrush(const Vec3& pos) const {
            const uint16_t flags = GetCellFlags(pos);
            return flags != 0xFFFF && (flags & Offset::NavGrid::FLAG_BRUSH) != 0;
        }

        bool IsWalkable(const Vec3& pos) const {
            const uint16_t flags = GetCellFlags(pos);
            if (flags == 0xFFFF) {
                return false;
            }

            if ((flags & Offset::NavGrid::FLAG_BRUSH) != 0) {
                return true;
            }
            return (flags & Offset::NavGrid::FLAG_NOWALK) == 0;
        }

        // METHOD 1: Fast bush check (1 byte read, no overlay)
        bool IsInBrushFast(const Vec3& pos) const {
            if (!Globals::IsValidPtr(byteData)) {
                return IsBrush(pos);
            }
            int x = 0, z = 0;
            if (!WorldToCell(pos, x, z)) {
                return false;
            }
            const int idx = z * width + x;
            __try {
                const uint8_t flag = Globals::Read<uint8_t>(byteData + idx);
                return flag != 0 && (flag & 0x1) == 0; // nonzero + not wall = brush
            } __except(1) { return false; }
        }

        // METHOD 2 extended terrain queries
        bool IsWater(const Vec3& pos) const {
            const uint16_t flags = GetCellFlags(pos);
            return flags != 0xFFFF && (flags & Offset::NavGridCell::CELL_WATER) != 0;
        }

        bool IsBuilding(const Vec3& pos) const {
            const uint16_t flags = GetCellFlags(pos);
            return flags != 0xFFFF && (flags & Offset::NavGridCell::CELL_BUILDING) != 0;
        }

        bool HasVision(const Vec3& pos) const {
            const uint16_t flags = GetCellFlags(pos);
            return flags != 0xFFFF && (flags & Offset::NavGridCell::CELL_VISION) != 0;
        }

        bool IsInPassabilityBrush(const Vec3& pos) const {
            const uint16_t flags = GetCellFlags(pos);
            return flags != 0xFFFF
                && (flags & Offset::NavGridCell::CELL_PASSABILITY) != 0
                && (flags & Offset::NavGridCell::CELL_WALL) == 0;
        }

        // ── Prediction helpers ──
        // Find first wall collision point along a line. Returns true if wall hit.
        bool FindWallCollision(const Vec3& from, const Vec3& to,
                               Vec3& hitPoint, float step = 10.0f) const {
            if (!IsValid() || step <= 0.0f) {
                return false;
            }
            const float dx = to.x - from.x;
            const float dz = to.z - from.z;
            const float dist = from.Distance2D(to);
            if (dist < 1.0f) return false;

            const int steps = (std::max)(1, static_cast<int>(dist / step));
            for (int i = 1; i <= steps; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const Vec3 sample = { from.x + dx * t, from.y, from.z + dz * t };
                if (IsWall(sample)) {
                    const float tPrev = static_cast<float>(i - 1) / static_cast<float>(steps);
                    hitPoint = { from.x + dx * tPrev, from.y, from.z + dz * tPrev };
                    return true;
                }
            }
            return false;
        }

        // Count wall cells in radius (for condemn/knockback predictions)
        int CountWallsInRadius(const Vec3& center, float radius, float step = 25.0f) const {
            if (!IsValid() || radius <= 0.0f) return 0;
            int count = 0;
            for (float dx = -radius; dx <= radius; dx += step) {
                for (float dz = -radius; dz <= radius; dz += step) {
                    if (dx * dx + dz * dz <= radius * radius) {
                        const Vec3 pos = { center.x + dx, center.y, center.z + dz };
                        if (IsWall(pos)) ++count;
                    }
                }
            }
            return count;
        }

        // Check if position is near a wall within given distance
        bool IsNearWall(const Vec3& pos, float distance = 50.0f) const {
            return CountWallsInRadius(pos, distance, 25.0f) > 0;
        }

        bool IsWallBetween(const Vec3& from, const Vec3& to, float step = 40.0f) const {
            if (!IsValid() || step <= 0.0f) {
                return false;
            }

            const Vec3 delta = { to.x - from.x, to.y - from.y, to.z - from.z };
            const float distance = from.Distance2D(to);
            if (distance <= 0.0f) {
                return IsWall(from);
            }

            const int steps = (std::max)(1, static_cast<int>(distance / step));
            for (int i = 0; i <= steps; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const Vec3 sample = {
                    from.x + delta.x * t,
                    from.y + delta.y * t,
                    from.z + delta.z * t
                };
                if (IsWall(sample)) {
                    return true;
                }
            }

            return false;
        }
    };

    inline GridRef Get() {
        GridRef grid = {};
        const auto navGrid = CoreRuntime::GetContext().navGrid;
        if (!Globals::IsValidPtr(navGrid)) {
            return grid;
        }

        const auto manager = Globals::Read<uintptr_t>(navGrid + Offset::NavGrid::NavGridMgr);
        if (!Globals::IsValidPtr(manager)) {
            return grid;
        }

        grid.navGrid = navGrid;
        grid.manager = manager;
        grid.cellData = Globals::Read<uintptr_t>(manager + Offset::NavGrid::Data);
        grid.byteData = Globals::Read<uintptr_t>(manager + Offset::NavGrid::ByteFlagData);
        grid.width = Globals::Read<int>(manager + Offset::NavGrid::Width);
        grid.height = Globals::Read<int>(manager + Offset::NavGrid::Height);
        grid.minX = Globals::Read<float>(manager + Offset::NavGrid::MinX);
        grid.minZ = Globals::Read<float>(manager + Offset::NavGrid::MinZ);
        grid.maxX = Globals::Read<float>(manager + Offset::NavGrid::MaxX);
        grid.maxZ = Globals::Read<float>(manager + Offset::NavGrid::MaxZ);
        grid.cellSize = Globals::Read<float>(manager + Offset::NavGrid::Scale);
        grid.inverseScale = Globals::Read<float>(manager + Offset::NavGrid::InverseScale);
        return grid;
    }

} // namespace CoreNavGrid
