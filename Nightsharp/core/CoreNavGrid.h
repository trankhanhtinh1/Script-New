#pragma once

#include "CoreRuntime.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace CoreNavGrid {

constexpr std::uint16_t kInvalidRawFlags = 0xFFFFu;

enum PublicCollisionFlags : std::uint16_t {
    Collision_None         = 0,
    Collision_Grass        = 1u,
    Collision_Wall         = 2u,
    Collision_Building     = 0x40u,
    Collision_Prop         = 0x80u,
    Collision_GlobalVision = 0x100u
};

struct GridPoint {
    int x = 0;
    int y = 0;
};

inline bool HasPublicFlag(std::uint16_t flags, std::uint16_t flag) {
    return (flags & flag) == flag;
}

inline bool RawHasWall(std::uint16_t rawFlags) {
    return rawFlags != kInvalidRawFlags &&
           (rawFlags & Offset::NavGridCellLayout::CELL_WALL) != 0;
}

inline bool RawHasBrush(std::uint16_t rawFlags) {
    return rawFlags != kInvalidRawFlags &&
           (rawFlags & Offset::NavGridFlags::FlagBrush) != 0;
}

inline bool RawHasWater(std::uint16_t rawFlags) {
    return rawFlags != kInvalidRawFlags &&
           (rawFlags & Offset::NavGridCellLayout::CELL_WATER) != 0;
}

inline bool RawHasBuilding(std::uint16_t rawFlags) {
    return rawFlags != kInvalidRawFlags &&
           (rawFlags & Collision_Building) != 0;
}

inline bool RawHasProp(std::uint16_t rawFlags) {
    return rawFlags != kInvalidRawFlags &&
           (rawFlags & Collision_Prop) != 0;
}

inline bool RawHasGlobalVision(std::uint16_t rawFlags) {
    return rawFlags != kInvalidRawFlags &&
           (rawFlags & Collision_GlobalVision) != 0;
}

inline std::uint16_t RawToPublicFlags(std::uint16_t rawFlags) {
    if (rawFlags == kInvalidRawFlags) {
        return Collision_None;
    }

    std::uint16_t result = Collision_None;
    if (RawHasBrush(rawFlags)) {
        result |= Collision_Grass;
    }
    if (RawHasWall(rawFlags)) {
        result |= Collision_Wall;
    }
    if (RawHasBuilding(rawFlags)) {
        result |= Collision_Building;
    }
    if (RawHasProp(rawFlags)) {
        result |= Collision_Prop;
    }
    if (RawHasGlobalVision(rawFlags)) {
        result |= Collision_GlobalVision;
    }
    return result;
}

inline std::uint16_t PublicToRawFlags(std::uint16_t flags) {
    std::uint16_t raw = 0;
    if ((flags & Collision_Grass) != 0) {
        raw |= Offset::NavGridFlags::FlagBrush;
    }
    if ((flags & Collision_Wall) != 0) {
        raw |= Offset::NavGridCellLayout::CELL_WALL;
    }
    if ((flags & Collision_Building) != 0) {
        raw |= Collision_Building;
    }
    if ((flags & Collision_Prop) != 0) {
        raw |= Collision_Prop;
    }
    if ((flags & Collision_GlobalVision) != 0) {
        raw |= Collision_GlobalVision;
    }
    return raw;
}

struct GridRef {
    uintptr_t navGrid = 0;
    uintptr_t manager = 0;
    uintptr_t cellData = 0;
    uintptr_t byteData = 0;
    int width = 0;
    int height = 0;
    float minX = 0.0f;
    float minZ = 0.0f;
    float maxX = 0.0f;
    float maxZ = 0.0f;
    float cellSize = 0.0f;
    float inverseScale = 0.0f;

    bool IsValid() const {
        return Globals::IsValidPtr(navGrid) &&
               Globals::IsValidPtr(manager) &&
               Globals::IsValidPtr(cellData) &&
               width > 0 &&
               height > 0 &&
               width < 10000 &&
               height < 10000 &&
               std::isfinite(cellSize) &&
               std::isfinite(inverseScale) &&
               cellSize > 0.0f &&
               inverseScale > 0.0f;
    }

    bool IsInsideWorld(const Vec3& pos) const {
        return IsValid() &&
               pos.IsValid() &&
               pos.x >= minX &&
               pos.x <= maxX &&
               pos.z >= minZ &&
               pos.z <= maxZ;
    }

    bool WorldToCell(const Vec3& pos, int& outX, int& outY, bool clampToGrid = true) const {
        outX = 0;
        outY = 0;
        if (!IsValid() || !pos.IsValid()) {
            return false;
        }

        outX = static_cast<int>((pos.x - minX) * inverseScale);
        outY = static_cast<int>((pos.z - minZ) * inverseScale);
        if (clampToGrid) {
            outX = std::clamp(outX, 0, width - 1);
            outY = std::clamp(outY, 0, height - 1);
            return true;
        }
        return outX >= 0 && outY >= 0 && outX < width && outY < height;
    }

    GridPoint WorldToCell(const Vec3& pos) const {
        GridPoint point = {};
        (void)WorldToCell(pos, point.x, point.y, true);
        return point;
    }

    Vec3 CellToWorld(int x, int y, float heightValue = 0.0f) const {
        if (!IsValid()) {
            return {};
        }

        const int clampedX = std::clamp(x, 0, width - 1);
        const int clampedY = std::clamp(y, 0, height - 1);
        return {
            minX + (static_cast<float>(clampedX) + 0.5f) * cellSize,
            heightValue,
            minZ + (static_cast<float>(clampedY) + 0.5f) * cellSize
        };
    }

    uintptr_t CellAddress(int x, int y) const {
        if (!IsValid() || x < 0 || y < 0 || x >= width || y >= height) {
            return 0;
        }

        return cellData +
            static_cast<uintptr_t>(Offset::NavGridCellLayout::CellStride) *
            (static_cast<uintptr_t>(x) +
             static_cast<uintptr_t>(y) * static_cast<uintptr_t>(width));
    }

    std::uint16_t GetRawCellFlags(int x, int y) const {
        const uintptr_t cell = CellAddress(x, y);
        if (!cell) {
            return kInvalidRawFlags;
        }

        __try {
            const uintptr_t overlay = Globals::Read<uintptr_t>(
                cell + Offset::NavGridCellLayout::CellOverlay);
            if (Globals::IsValidPtr(overlay)) {
                return Globals::Read<std::uint16_t>(
                    overlay + Offset::NavGridCellLayout::OverlayFlagsOff);
            }
            return Globals::Read<std::uint16_t>(
                cell + Offset::NavGridCellLayout::CellFlags);
        }
        __except (1) {
            return kInvalidRawFlags;
        }
    }

    std::uint16_t GetRawCellFlags(const Vec3& pos) const {
        int x = 0;
        int y = 0;
        if (!WorldToCell(pos, x, y, true)) {
            return kInvalidRawFlags;
        }
        return GetRawCellFlags(x, y);
    }

    bool SetRawCellFlags(int x, int y, std::uint16_t rawFlags) const {
        const uintptr_t cell = CellAddress(x, y);
        if (!cell) {
            return false;
        }

        __try {
            const uintptr_t overlay = Globals::Read<uintptr_t>(
                cell + Offset::NavGridCellLayout::CellOverlay);
            const uintptr_t target = Globals::IsValidPtr(overlay)
                ? overlay + Offset::NavGridCellLayout::OverlayFlagsOff
                : cell + Offset::NavGridCellLayout::CellFlags;
            return Globals::Write<std::uint16_t>(target, rawFlags);
        }
        __except (1) {
            return false;
        }
    }

    bool SetRawCellFlags(const Vec3& pos, std::uint16_t rawFlags) const {
        int x = 0;
        int y = 0;
        return WorldToCell(pos, x, y, true) && SetRawCellFlags(x, y, rawFlags);
    }

    std::uint16_t GetCollisionFlags(int x, int y) const {
        return RawToPublicFlags(GetRawCellFlags(x, y));
    }

    std::uint16_t GetCollisionFlags(const Vec3& pos) const {
        return RawToPublicFlags(GetRawCellFlags(pos));
    }

    bool SetCollisionFlags(int x, int y, std::uint16_t flags) const {
        return SetRawCellFlags(x, y, PublicToRawFlags(flags));
    }

    bool SetCollisionFlags(const Vec3& pos, std::uint16_t flags) const {
        return SetRawCellFlags(pos, PublicToRawFlags(flags));
    }

    bool IsWall(const Vec3& pos) const {
        return RawHasWall(GetRawCellFlags(pos));
    }

    bool IsBrush(const Vec3& pos) const {
        return RawHasBrush(GetRawCellFlags(pos));
    }

    bool IsWater(const Vec3& pos) const {
        return RawHasWater(GetRawCellFlags(pos));
    }

    bool IsBuilding(const Vec3& pos) const {
        return RawHasBuilding(GetRawCellFlags(pos));
    }

    bool IsProp(const Vec3& pos) const {
        return RawHasProp(GetRawCellFlags(pos));
    }

    bool HasGlobalVision(const Vec3& pos) const {
        return RawHasGlobalVision(GetRawCellFlags(pos));
    }

    bool IsWalkable(const Vec3& pos) const {
        const std::uint16_t rawFlags = GetRawCellFlags(pos);
        if (rawFlags == kInvalidRawFlags) {
            return false;
        }
        if (RawHasBrush(rawFlags)) {
            return true;
        }
        return !RawHasWall(rawFlags);
    }

    bool IsInBrushFast(const Vec3& pos) const {
        if (!Globals::IsValidPtr(byteData)) {
            return IsBrush(pos);
        }

        int x = 0;
        int y = 0;
        if (!WorldToCell(pos, x, y, true)) {
            return false;
        }

        __try {
            const auto index =
                static_cast<uintptr_t>(y) * static_cast<uintptr_t>(width) +
                static_cast<uintptr_t>(x);
            const std::uint8_t value = Globals::Read<std::uint8_t>(byteData + index);
            return value != 0 && (value & 0x1u) == 0;
        }
        __except (1) {
            return false;
        }
    }

    float GetHeightForPosition(float, float) const {
        return 0.0f;
    }

    bool IsWallOfType(const Vec3& pos, std::uint16_t flags, float radius) const {
        if (!IsValid() || flags == Collision_None || !pos.IsValid()) {
            return false;
        }

        const auto matches = [flags](std::uint16_t sampleFlags) {
            return (sampleFlags & flags) != 0;
        };

        if (radius <= 0.0f || cellSize <= 0.0f) {
            return matches(GetCollisionFlags(pos));
        }

        int minCellX = 0;
        int minCellY = 0;
        int maxCellX = 0;
        int maxCellY = 0;
        (void)WorldToCell({ pos.x - radius, pos.y, pos.z - radius }, minCellX, minCellY, true);
        (void)WorldToCell({ pos.x + radius, pos.y, pos.z + radius }, maxCellX, maxCellY, true);

        if (minCellX > maxCellX) {
            std::swap(minCellX, maxCellX);
        }
        if (minCellY > maxCellY) {
            std::swap(minCellY, maxCellY);
        }

        const float radiusSqr = radius * radius;
        for (int y = minCellY; y <= maxCellY; ++y) {
            for (int x = minCellX; x <= maxCellX; ++x) {
                const Vec3 center = CellToWorld(x, y, pos.y);
                if (center.DistanceSqr2D(pos) <= radiusSqr &&
                    matches(GetCollisionFlags(x, y))) {
                    return true;
                }
            }
        }
        return false;
    }

    bool IsWallBetween(const Vec3& from, const Vec3& to, float step = 40.0f) const {
        if (!IsValid() || step <= 0.0f) {
            return false;
        }

        const float distance = from.Distance2D(to);
        if (distance <= 0.0f) {
            return IsWall(from);
        }

        const Vec3 delta = to - from;
        const int steps = std::max(1, static_cast<int>(distance / step));
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const Vec3 sample = from + delta * t;
            if (IsWall(sample)) {
                return true;
            }
        }
        return false;
    }

    bool FindWallCollision(const Vec3& from, const Vec3& to, Vec3& hitPoint, float step = 10.0f) const {
        hitPoint = {};
        if (!IsValid() || step <= 0.0f) {
            return false;
        }

        const float distance = from.Distance2D(to);
        if (distance <= 0.0f) {
            return false;
        }

        const Vec3 delta = to - from;
        const int steps = std::max(1, static_cast<int>(distance / step));
        for (int i = 1; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const Vec3 sample = from + delta * t;
            if (IsWall(sample)) {
                const float previousT = static_cast<float>(i - 1) /
                    static_cast<float>(steps);
                hitPoint = from + delta * previousT;
                return true;
            }
        }
        return false;
    }

    int CountWallsInRadius(const Vec3& center, float radius, float step = 25.0f) const {
        if (!IsValid() || radius <= 0.0f || step <= 0.0f) {
            return 0;
        }

        int count = 0;
        const float radiusSqr = radius * radius;
        for (float dx = -radius; dx <= radius; dx += step) {
            for (float dz = -radius; dz <= radius; dz += step) {
                if (dx * dx + dz * dz > radiusSqr) {
                    continue;
                }
                if (IsWall({ center.x + dx, center.y, center.z + dz })) {
                    ++count;
                }
            }
        }
        return count;
    }

    bool IsNearWall(const Vec3& pos, float distance = 50.0f) const {
        return CountWallsInRadius(pos, distance, 25.0f) > 0;
    }
};

inline GridRef Get() {
    (void)CoreRuntime::EnsureInitialized();

    GridRef grid = {};
    const auto& ctx = CoreRuntime::GetContext();
    uintptr_t navGrid = ctx.navGrid;
    if (!Globals::IsValidPtr(navGrid) && Globals::IsValidPtr(ctx.navGridGlobal)) {
        navGrid = Globals::Read<uintptr_t>(ctx.navGridGlobal);
    }
    if (!Globals::IsValidPtr(navGrid)) {
        return grid;
    }

    const uintptr_t manager = Globals::Read<uintptr_t>(
        navGrid + Offset::NavGridLayout::NavGridMgr);
    if (!Globals::IsValidPtr(manager)) {
        return grid;
    }

    grid.navGrid = navGrid;
    grid.manager = manager;
    grid.cellData = Globals::Read<uintptr_t>(manager + Offset::NavGridLayout::Data);
    grid.byteData = Globals::Read<uintptr_t>(manager + Offset::NavGridLayout::ByteFlagData);
    grid.width = Globals::Read<int>(manager + Offset::NavGridLayout::Width);
    grid.height = Globals::Read<int>(manager + Offset::NavGridLayout::Height);
    grid.minX = Globals::Read<float>(manager + Offset::NavGridLayout::MinX);
    grid.minZ = Globals::Read<float>(manager + Offset::NavGridLayout::MinZ);
    grid.maxX = Globals::Read<float>(manager + Offset::NavGridLayout::MaxX);
    grid.maxZ = Globals::Read<float>(manager + Offset::NavGridLayout::MaxZ);
    grid.cellSize = Globals::Read<float>(manager + Offset::NavGridLayout::Scale);
    grid.inverseScale = Globals::Read<float>(manager + Offset::NavGridLayout::InverseScale);
    return grid;
}

inline bool IsWall(const Vec3& pos) { return Get().IsWall(pos); }
inline bool IsBrush(const Vec3& pos) { return Get().IsBrush(pos); }
inline bool IsWalkable(const Vec3& pos) { return Get().IsWalkable(pos); }
inline bool IsWater(const Vec3& pos) { return Get().IsWater(pos); }
inline bool IsBuilding(const Vec3& pos) { return Get().IsBuilding(pos); }
inline bool IsProp(const Vec3& pos) { return Get().IsProp(pos); }
inline bool HasGlobalVision(const Vec3& pos) { return Get().HasGlobalVision(pos); }
inline bool IsInsideWorld(const Vec3& pos) { return Get().IsInsideWorld(pos); }
inline bool IsInBrushFast(const Vec3& pos) { return Get().IsInBrushFast(pos); }
inline std::uint16_t GetRawCellFlags(const Vec3& pos) { return Get().GetRawCellFlags(pos); }
inline std::uint16_t GetCollisionFlags(const Vec3& pos) { return Get().GetCollisionFlags(pos); }
inline bool SetCollisionFlags(const Vec3& pos, std::uint16_t flags) {
    return Get().SetCollisionFlags(pos, flags);
}
inline GridPoint WorldToGrid(const Vec3& pos) { return Get().WorldToCell(pos); }
inline Vec3 GridToWorld(int x, int y, float heightValue = 0.0f) {
    return Get().CellToWorld(x, y, heightValue);
}
inline float GetHeightForPosition(float x, float y) {
    return Get().GetHeightForPosition(x, y);
}
inline bool IsWallOfType(const Vec3& pos, std::uint16_t flags, float radius) {
    return Get().IsWallOfType(pos, flags, radius);
}
inline bool IsWallBetween(const Vec3& from, const Vec3& to, float step = 40.0f) {
    return Get().IsWallBetween(from, to, step);
}
inline bool FindWallCollision(const Vec3& from, const Vec3& to, Vec3& hitPoint, float step = 10.0f) {
    return Get().FindWallCollision(from, to, hitPoint, step);
}
inline int CountWallsInRadius(const Vec3& center, float radius, float step = 25.0f) {
    return Get().CountWallsInRadius(center, radius, step);
}
inline bool IsNearWall(const Vec3& pos, float distance = 50.0f) {
    return Get().IsNearWall(pos, distance);
}

} // namespace CoreNavGrid
