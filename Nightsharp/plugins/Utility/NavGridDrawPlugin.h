#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreNavGrid.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Plugins {

class NavGridDrawPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "NavGrid Wall Brush Draw"; }
    const char* GetInternalId() const override { return "utility.navgrid_wall_brush_draw"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        NightSharpDebug::Logf("[NavGridDraw] loaded");
    }

    void OnUnload() override {
        ResetCache();
        NightSharpDebug::Logf("[NavGridDraw] unloaded");
    }

    void OnRender() override {
        if (!ImGui::GetCurrentContext()) {
            return;
        }

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) {
            return;
        }

        const auto grid = CoreNavGrid::Get();
        if (!grid.IsValid()) {
            return;
        }

        const Vec3 playerPos = player.Position();
        int centerX = 0;
        int centerY = 0;
        if (!grid.WorldToCell(playerPos, centerX, centerY, false)) {
            return;
        }

        constexpr float drawRadius = 1300.0f;
        const int radiusCells = std::clamp(
            static_cast<int>(drawRadius / std::max(1.0f, grid.cellSize)) + 2,
            1,
            kMaxRadiusCells);
        const float radiusSqr = drawRadius * drawRadius;

        CellCache& cache = Cache();
        EnsureCellCache(cache, grid, playerPos, centerX, centerY, radiusCells, radiusSqr);
        DrawCachedCells(cache, grid, playerPos, centerX, centerY);
    }

private:
    static constexpr int kMaxRadiusCells = 42;
    static constexpr int kMaxCachedCells = 8192;
    static constexpr int kCacheRefreshFrames = 12;
    static constexpr std::uint32_t kWallColor = 0xD00000FFu;
    static constexpr std::uint32_t kBrushColor = 0xCC20FF70u;

    struct CachedCell {
        Vec3 world = {};
        std::uint16_t rawFlags = CoreNavGrid::kInvalidRawFlags;
    };

    struct CellCache {
        uintptr_t cellData = 0;
        int width = 0;
        int height = 0;
        int centerX = -1;
        int centerY = -1;
        int radiusCells = 0;
        int lastBuildFrame = -100000;
        float cellSize = 0.0f;
        float heightValue = 0.0f;
        int count = 0;
        CachedCell cells[kMaxCachedCells] = {};
    };

    static CellCache& Cache() {
        static CellCache cache;
        return cache;
    }

    static ImVec2* WallScreens() {
        static ImVec2 points[kMaxCachedCells] = {};
        return points;
    }

    static ImVec2* BrushScreens() {
        static ImVec2 points[kMaxCachedCells] = {};
        return points;
    }

    static void ResetCache() {
        CellCache& cache = Cache();
        cache.cellData = 0;
        cache.width = 0;
        cache.height = 0;
        cache.centerX = -1;
        cache.centerY = -1;
        cache.radiusCells = 0;
        cache.lastBuildFrame = -100000;
        cache.cellSize = 0.0f;
        cache.heightValue = 0.0f;
        cache.count = 0;
    }

    static bool ShouldRebuildCache(const CellCache& cache,
                                   const CoreNavGrid::GridRef& grid,
                                   const Vec3& playerPos,
                                   int centerX,
                                   int centerY,
                                   int radiusCells) {
        const int frame = ImGui::GetFrameCount();
        return cache.cellData != grid.cellData ||
               cache.width != grid.width ||
               cache.height != grid.height ||
               cache.centerX != centerX ||
               cache.centerY != centerY ||
               cache.radiusCells != radiusCells ||
               std::fabs(cache.cellSize - grid.cellSize) > 0.01f ||
               std::fabs(cache.heightValue - playerPos.y) > 25.0f ||
               frame - cache.lastBuildFrame >= kCacheRefreshFrames;
    }

    static void EnsureCellCache(CellCache& cache,
                                const CoreNavGrid::GridRef& grid,
                                const Vec3& playerPos,
                                int centerX,
                                int centerY,
                                int radiusCells,
                                float radiusSqr) {
        if (!ShouldRebuildCache(cache, grid, playerPos, centerX, centerY, radiusCells)) {
            return;
        }

        cache.cellData = grid.cellData;
        cache.width = grid.width;
        cache.height = grid.height;
        cache.centerX = centerX;
        cache.centerY = centerY;
        cache.radiusCells = radiusCells;
        cache.lastBuildFrame = ImGui::GetFrameCount();
        cache.cellSize = grid.cellSize;
        cache.heightValue = playerPos.y;
        cache.count = 0;

        const int minY = std::max(0, centerY - radiusCells);
        const int maxY = std::min(grid.height - 1, centerY + radiusCells);
        const int hardMinX = std::max(0, centerX - radiusCells);
        const int hardMaxX = std::min(grid.width - 1, centerX + radiusCells);
        const float baseX = grid.minX + grid.cellSize * 0.5f;
        const float baseZ = grid.minZ + grid.cellSize * 0.5f;

        for (int y = centerY - radiusCells; y <= centerY + radiusCells; ++y) {
            if (y < minY || y > maxY || cache.count >= kMaxCachedCells) {
                continue;
            }

            const float worldZ = baseZ + static_cast<float>(y) * grid.cellSize;
            const float dz = worldZ - playerPos.z;
            const float dzSqr = dz * dz;
            if (dzSqr > radiusSqr) {
                continue;
            }

            const float rowRadius = std::sqrt(radiusSqr - dzSqr);
            int rowMinX = static_cast<int>((playerPos.x - rowRadius - grid.minX) * grid.inverseScale);
            int rowMaxX = static_cast<int>((playerPos.x + rowRadius - grid.minX) * grid.inverseScale);
            rowMinX = std::clamp(rowMinX, hardMinX, hardMaxX);
            rowMaxX = std::clamp(rowMaxX, hardMinX, hardMaxX);

            for (int x = rowMinX; x <= rowMaxX && cache.count < kMaxCachedCells; ++x) {
                const float worldX = baseX + static_cast<float>(x) * grid.cellSize;

                const std::uint16_t rawFlags = grid.GetRawCellFlags(x, y);
                const bool isWall = CoreNavGrid::RawHasWall(rawFlags) ||
                                    CoreNavGrid::RawHasBuilding(rawFlags);
                const bool isBrush = CoreNavGrid::RawHasBrush(rawFlags);
                if (!isWall && !isBrush) {
                    continue;
                }

                CachedCell& cell = cache.cells[cache.count++];
                cell.world = { worldX, playerPos.y, worldZ };
                cell.rawFlags = rawFlags;
            }
        }
    }

    static ImU32 ToImColor(std::uint32_t argb) {
        return IM_COL32(
            static_cast<int>((argb >> 16) & 0xFFu),
            static_cast<int>((argb >> 8) & 0xFFu),
            static_cast<int>(argb & 0xFFu),
            static_cast<int>((argb >> 24) & 0xFFu));
    }

    static float EstimateMarkerHalf(const CoreNavGrid::GridRef& grid,
                                    const Vec3& playerPos,
                                    int centerX,
                                    int centerY) {
        Vec2 a = {};
        Vec2 b = {};
        const int neighborX = centerX + 1 < grid.width ? centerX + 1 : centerX - 1;
        if (neighborX >= 0 &&
            SDK::Drawing::WorldToScreen(grid.CellToWorld(centerX, centerY, playerPos.y), a) &&
            SDK::Drawing::WorldToScreen(grid.CellToWorld(neighborX, centerY, playerPos.y), b)) {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            const float cellPixels = std::sqrt(dx * dx + dy * dy);
            return std::clamp(cellPixels * 0.22f, 2.0f, 6.0f);
        }
        return 3.0f;
    }

    static void AddScreenSquares(ImDrawList* draw,
                                 const ImVec2* centers,
                                 int count,
                                 float halfSize,
                                 ImU32 color) {
        if (!draw || !centers || count <= 0 || halfSize <= 0.0f || color == 0) {
            return;
        }

        draw->PrimReserve(count * 6, count * 4);
        for (int i = 0; i < count; ++i) {
            const ImVec2& p = centers[i];
            draw->PrimRect(
                ImVec2(p.x - halfSize, p.y - halfSize),
                ImVec2(p.x + halfSize, p.y + halfSize),
                color);
        }
    }

    static void DrawCachedCells(const CellCache& cache,
                                const CoreNavGrid::GridRef& grid,
                                const Vec3& playerPos,
                                int centerX,
                                int centerY) {
        if (cache.count <= 0 || !SDK::Drawing::IsEnabled()) {
            return;
        }

        ImDrawList* draw = SDK::Drawing::GetDrawList(true);
        if (!draw) {
            return;
        }

        const Vec2 rendererSize = SDK::Drawing::GetRendererSize();
        if (!rendererSize.IsValid() || rendererSize.x <= 0.0f || rendererSize.y <= 0.0f) {
            return;
        }

        ImVec2* wallScreens = WallScreens();
        ImVec2* brushScreens = BrushScreens();
        int wallCount = 0;
        int brushCount = 0;
        constexpr float screenMargin = 12.0f;

        for (int i = 0; i < cache.count; ++i) {
            Vec2 screen = {};
            if (!SDK::Drawing::WorldToScreen(cache.cells[i].world, screen) ||
                screen.x < -screenMargin ||
                screen.y < -screenMargin ||
                screen.x > rendererSize.x + screenMargin ||
                screen.y > rendererSize.y + screenMargin) {
                continue;
            }

            const std::uint16_t rawFlags = cache.cells[i].rawFlags;
            const ImVec2 point(screen.x, screen.y);
            if ((CoreNavGrid::RawHasWall(rawFlags) || CoreNavGrid::RawHasBuilding(rawFlags)) &&
                wallCount < kMaxCachedCells) {
                wallScreens[wallCount++] = point;
            }
            if (CoreNavGrid::RawHasBrush(rawFlags) && brushCount < kMaxCachedCells) {
                brushScreens[brushCount++] = point;
            }
        }

        const float wallHalf = EstimateMarkerHalf(grid, playerPos, centerX, centerY);
        AddScreenSquares(draw, wallScreens, wallCount, wallHalf, ToImColor(kWallColor));
        AddScreenSquares(draw, brushScreens, brushCount, std::max(1.5f, wallHalf * 0.72f), ToImColor(kBrushColor));
    }
};

} // namespace Plugins
