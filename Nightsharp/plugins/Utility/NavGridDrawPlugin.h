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
    bool AutoLoadByDefault() const override { return true; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        NightSharpDebug::Logf("[NavGridDraw] loaded");
    }

    void OnUnload() override {
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

        const float drawRadius = 1300.0f;
        const int radiusCells = std::clamp(
            static_cast<int>(drawRadius / std::max(1.0f, grid.cellSize)) + 2,
            1,
            42);
        const float ringRadius = std::clamp(grid.cellSize * 0.42f, 14.0f, 42.0f);
        const float radiusSqr = drawRadius * drawRadius;

        for (int y = centerY - radiusCells; y <= centerY + radiusCells; ++y) {
            if (y < 0 || y >= grid.height) {
                continue;
            }

            for (int x = centerX - radiusCells; x <= centerX + radiusCells; ++x) {
                if (x < 0 || x >= grid.width) {
                    continue;
                }

                const Vec3 world = grid.CellToWorld(x, y, playerPos.y);
                if (world.DistanceSqr2D(playerPos) > radiusSqr) {
                    continue;
                }

                const std::uint16_t rawFlags = grid.GetRawCellFlags(x, y);
                const bool isWall = CoreNavGrid::RawHasWall(rawFlags) ||
                                    CoreNavGrid::RawHasBuilding(rawFlags);
                const bool isBrush = CoreNavGrid::RawHasBrush(rawFlags);
                if (!isWall && !isBrush) {
                    continue;
                }

                if (isWall) {
                    DrawWorldRing(world, ringRadius, 0xD00000FFu, 3.0f);
                }
                if (isBrush) {
                    DrawWorldRing(world, ringRadius * 0.78f, 0xCC20FF70u, 2.0f);
                }
            }
        }
    }

private:
    static void DrawWorldRing(const Vec3& center,
                              float radius,
                              std::uint32_t color,
                              float thickness) {
        constexpr int kSegments = 28;
        constexpr float kPi = 3.14159265358979323846f;

        Vec2 first = {};
        Vec2 previous = {};
        bool haveFirst = false;
        bool havePrevious = false;

        for (int i = 0; i <= kSegments; ++i) {
            const float angle = (2.0f * kPi * static_cast<float>(i)) /
                static_cast<float>(kSegments);
            const Vec3 point = {
                center.x + std::cos(angle) * radius,
                center.y,
                center.z + std::sin(angle) * radius
            };

            Vec2 screen = {};
            if (!SDK::Drawing::WorldToScreen(point, screen)) {
                havePrevious = false;
                continue;
            }

            if (!haveFirst) {
                first = screen;
                haveFirst = true;
            }
            if (havePrevious) {
                SDK::Drawing::DrawLine(previous, screen, thickness, color);
            }
            previous = screen;
            havePrevious = true;
        }

        if (haveFirst && havePrevious && previous.Distance(first) < 500.0f) {
            SDK::Drawing::DrawLine(previous, first, thickness, color);
        }
    }
};

} // namespace Plugins
