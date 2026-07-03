#pragma once

#include "CoreRuntime.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"
#include "../imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
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

namespace detail {
    inline constexpr int kRawFlagCacheSize = 8192;
    inline constexpr int kRawFlagCacheMask = kRawFlagCacheSize - 1;

    struct RawFlagCacheEntry {
        uintptr_t cellData = 0;
        int frame = -1;
        int x = -1;
        int y = -1;
        std::uint16_t flags = kInvalidRawFlags;
    };

    inline RawFlagCacheEntry* RawFlagCache() {
        static RawFlagCacheEntry cache[kRawFlagCacheSize] = {};
        return cache;
    }

    inline int FrameKey() {
        if (ImGui::GetCurrentContext()) {
            return ImGui::GetFrameCount();
        }

        const auto generation = CoreRuntime::GetContext().refreshGeneration;
        return -static_cast<int>((generation & 0x3FFFFFFFu) + 1u);
    }

    inline unsigned HashCell(uintptr_t cellData, int x, int y) {
        unsigned hash = static_cast<unsigned>((cellData >> 4) ^ (cellData >> 16));
        hash ^= static_cast<unsigned>(x) * 73856093u;
        hash ^= static_cast<unsigned>(y) * 19349663u;
        return hash & kRawFlagCacheMask;
    }

    inline bool LookupRawFlag(uintptr_t cellData,
                              int x,
                              int y,
                              std::uint16_t& outFlags) {
        const int frame = FrameKey();
        const RawFlagCacheEntry& entry = RawFlagCache()[HashCell(cellData, x, y)];
        if (entry.frame == frame &&
            entry.cellData == cellData &&
            entry.x == x &&
            entry.y == y) {
            outFlags = entry.flags;
            return true;
        }
        return false;
    }

    inline void StoreRawFlag(uintptr_t cellData,
                             int x,
                             int y,
                             std::uint16_t flags) {
        RawFlagCacheEntry& entry = RawFlagCache()[HashCell(cellData, x, y)];
        entry.cellData = cellData;
        entry.frame = FrameKey();
        entry.x = x;
        entry.y = y;
        entry.flags = flags;
    }
} // namespace detail

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

        std::uint16_t cached = kInvalidRawFlags;
        if (detail::LookupRawFlag(cellData, x, y, cached)) {
            return cached;
        }

        std::uint16_t result = kInvalidRawFlags;
        __try {
            const uintptr_t overlay = Globals::Read<uintptr_t>(
                cell + Offset::NavGridCellLayout::CellOverlay);
            if (Globals::IsValidPtr(overlay)) {
                result = Globals::Read<std::uint16_t>(
                    overlay + Offset::NavGridCellLayout::OverlayFlagsOff);
            } else {
                result = Globals::Read<std::uint16_t>(
                    cell + Offset::NavGridCellLayout::CellFlags);
            }
        }
        __except (1) {
            result = kInvalidRawFlags;
        }

        detail::StoreRawFlag(cellData, x, y, result);
        return result;
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
            const bool written = Globals::Write<std::uint16_t>(target, rawFlags);
            if (written) {
                detail::StoreRawFlag(cellData, x, y, rawFlags);
            }
            return written;
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
        if (!IsValid() || step <= 0.0f || !from.IsValid() || !to.IsValid()) {
            return false;
        }

        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        if (!WorldToCell(from, x0, y0, true) ||
            !WorldToCell(to, x1, y1, true)) {
            return false;
        }

        const int dx = std::abs(x1 - x0);
        const int dy = -std::abs(y1 - y0);
        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;

        int x = x0;
        int y = y0;
        for (;;) {
            if (RawHasWall(GetRawCellFlags(x, y))) {
                return true;
            }
            if (x == x1 && y == y1) {
                break;
            }

            const int twiceError = error * 2;
            if (twiceError >= dy) {
                error += dy;
                x += sx;
            }
            if (twiceError <= dx) {
                error += dx;
                y += sy;
            }
        }
        return false;
    }

    bool FindWallCollision(const Vec3& from, const Vec3& to, Vec3& hitPoint, float step = 10.0f) const {
        hitPoint = {};
        if (!IsValid() || step <= 0.0f || !from.IsValid() || !to.IsValid()) {
            return false;
        }

        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        if (!WorldToCell(from, x0, y0, true) ||
            !WorldToCell(to, x1, y1, true) ||
            (x0 == x1 && y0 == y1)) {
            return false;
        }

        const int dx = std::abs(x1 - x0);
        const int dy = -std::abs(y1 - y0);
        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;

        int x = x0;
        int y = y0;
        int previousX = x0;
        int previousY = y0;
        for (;;) {
            if (x == x1 && y == y1) {
                break;
            }

            previousX = x;
            previousY = y;

            const int twiceError = error * 2;
            if (twiceError >= dy) {
                error += dy;
                x += sx;
            }
            if (twiceError <= dx) {
                error += dx;
                y += sy;
            }

            if (RawHasWall(GetRawCellFlags(x, y))) {
                hitPoint = CellToWorld(previousX, previousY, from.y);
                return true;
            }
        }
        return false;
    }

    int CountWallsInRadius(const Vec3& center, float radius, float step = 25.0f) const {
        if (!IsValid() ||
            radius <= 0.0f ||
            step <= 0.0f ||
            !center.IsValid()) {
            return 0;
        }

        int minCellX = 0;
        int minCellY = 0;
        int maxCellX = 0;
        int maxCellY = 0;
        (void)WorldToCell({ center.x - radius, center.y, center.z - radius }, minCellX, minCellY, true);
        (void)WorldToCell({ center.x + radius, center.y, center.z + radius }, maxCellX, maxCellY, true);

        if (minCellX > maxCellX) {
            std::swap(minCellX, maxCellX);
        }
        if (minCellY > maxCellY) {
            std::swap(minCellY, maxCellY);
        }

        int count = 0;
        const float radiusSqr = radius * radius;
        for (int y = minCellY; y <= maxCellY; ++y) {
            const float cellZ = minZ + (static_cast<float>(y) + 0.5f) * cellSize;
            const float dz = cellZ - center.z;
            const float dzSqr = dz * dz;
            if (dzSqr > radiusSqr) {
                continue;
            }

            for (int x = minCellX; x <= maxCellX; ++x) {
                const float cellX = minX + (static_cast<float>(x) + 0.5f) * cellSize;
                const float dx = cellX - center.x;
                if (dx * dx + dzSqr > radiusSqr) {
                    continue;
                }
                if (RawHasWall(GetRawCellFlags(x, y))) {
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

    static int cachedFrame = -1;
    static GridRef cachedGrid = {};
    const int frame = detail::FrameKey();
    if (cachedFrame == frame) {
        return cachedGrid;
    }

    GridRef grid = {};
    const auto& ctx = CoreRuntime::GetContext();
    uintptr_t navGrid = ctx.navGrid;
    if (!Globals::IsValidPtr(navGrid) && Globals::IsValidPtr(ctx.navGridGlobal)) {
        navGrid = Globals::Read<uintptr_t>(ctx.navGridGlobal);
    }
    if (!Globals::IsValidPtr(navGrid)) {
        cachedFrame = frame;
        cachedGrid = grid;
        return grid;
    }

    const uintptr_t manager = Globals::Read<uintptr_t>(
        navGrid + Offset::NavGridLayout::NavGridMgr);
    if (!Globals::IsValidPtr(manager)) {
        cachedFrame = frame;
        cachedGrid = grid;
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
    cachedFrame = frame;
    cachedGrid = grid;
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
