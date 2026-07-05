#pragma once

#include "CoreNavGrid.h"
#include "CoreRuntime.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace CoreMap {

enum class MapId : int {
    Unknown = 0,
    SummonersRiftOriginalSummer = 1,
    SummonersRiftOriginalAutumn = 2,
    ProvingGrounds = 3,
    TwistedTreelineOriginal = 4,
    CrystalScar = 8,
    TwistedTreeline = 10,
    SummonersRift = 11,
    HowlingAbyss = 12,
    ButchersBridge = 14,
    CosmicRuins = 16,
    ValoranCityPark = 18,
    Substructure43 = 19,
    CrashSite = 20,
    NexusBlitz = 21,
    TeamfightTactics = 22,
    Arena = 30,
    Swarm = 33,
    Bandlewood = 35
};

struct MapBounds {
    float minX = 0.0f;
    float minZ = 0.0f;
    float maxX = 0.0f;
    float maxZ = 0.0f;

    bool IsValid() const {
        return std::isfinite(minX) && std::isfinite(minZ) &&
               std::isfinite(maxX) && std::isfinite(maxZ) &&
               maxX > minX && maxZ > minZ;
    }

    float Width() const { return maxX - minX; }
    float Height() const { return maxZ - minZ; }

    Vec3 CenterWorldPos(float y = 0.0f) const {
        return { (minX + maxX) * 0.5f, y, (minZ + maxZ) * 0.5f };
    }
};

struct TacticalMapState {
    uintptr_t address = 0;
    Vec2 offset = {};
    Vec2 size = {};
    Vec2 cachedSize = {};
    Vec2 negWorld = {};
    Vec2 scale = {};
    Vec3 centerWorldPos = {};
    bool fallback = false;

    bool IsValid() const {
        return offset.IsValid() &&
               size.IsValid() &&
               scale.IsValid() &&
               size.x > 32.0f &&
               size.y > 32.0f &&
               scale.x > 0.000001f &&
               scale.y > 0.000001f;
    }

    bool WorldToMinimap(const Vec3& world, Vec2& out) const {
        out = {};
        if (!IsValid() || !world.IsValid()) {
            return false;
        }

        out = {
            (world.x + negWorld.x) * scale.x + offset.x,
            (world.z + negWorld.y) * scale.y + offset.y
        };
        return out.IsValid();
    }

    bool MinimapToWorld(const Vec2& minimap, Vec3& out, float y = 0.0f) const {
        out = {};
        if (!IsValid() || !minimap.IsValid()) {
            return false;
        }

        out = {
            (minimap.x - offset.x) / scale.x - negWorld.x,
            y,
            (minimap.y - offset.y) / scale.y - negWorld.y
        };
        return out.IsValid();
    }
};

namespace detail {
    inline uintptr_t CachedTacticalMap = 0;

    inline bool IsFinite(float value) {
        return std::isfinite(value);
    }

    inline bool ReadRendererSize(Vec2& out) {
        out = {};
        const auto& ctx = CoreRuntime::GetContext();
        if (Globals::IsValidPtr(ctx.renderer)) {
            __try {
                const int width = Globals::Read<int>(ctx.renderer + 0xC);
                const int height = Globals::Read<int>(ctx.renderer + 0x10);
                if (width > 0 && height > 0 && width < 20000 && height < 20000) {
                    out = { static_cast<float>(width), static_cast<float>(height) };
                    return true;
                }
            } __except (1) {
                out = {};
            }
        }

        const int width = GetSystemMetrics(SM_CXSCREEN);
        const int height = GetSystemMetrics(SM_CYSCREEN);
        if (width > 0 && height > 0 && width < 20000 && height < 20000) {
            out = { static_cast<float>(width), static_cast<float>(height) };
            return true;
        }
        return false;
    }

    inline bool ReadMapBounds(MapBounds& out) {
        out = {};
        const auto grid = CoreNavGrid::Get();
        if (!grid.IsValid()) {
            return false;
        }

        out = { grid.minX, grid.minZ, grid.maxX, grid.maxZ };
        return out.IsValid();
    }

    inline bool IsPlausibleTacticalMap(const TacticalMapState& state) {
        Vec2 renderer = {};
        (void)ReadRendererSize(renderer);
        const float maxX = renderer.x > 0.0f ? renderer.x + 512.0f : 8192.0f;
        const float maxY = renderer.y > 0.0f ? renderer.y + 512.0f : 8192.0f;

        return state.IsValid() &&
               state.offset.x >= -512.0f &&
               state.offset.y >= -512.0f &&
               state.offset.x <= maxX &&
               state.offset.y <= maxY &&
               state.size.x < 1024.0f &&
               state.size.y < 1024.0f &&
               state.scale.x < 1.0f &&
               state.scale.y < 1.0f &&
               std::fabs(state.negWorld.x) < 50000.0f &&
               std::fabs(state.negWorld.y) < 50000.0f;
    }

    inline bool TryReadTacticalMap(uintptr_t address, TacticalMapState& out) {
        out = {};
        if (!Globals::IsReadablePtr(address, Offset::TacticalMapLayout::ScaleY + sizeof(float))) {
            return false;
        }

        TacticalMapState state = {};
        state.address = address;
        __try {
            state.negWorld = {
                Globals::Read<float>(address + Offset::TacticalMapLayout::NegMinimapX),
                Globals::Read<float>(address + Offset::TacticalMapLayout::NegMinimapY)
            };
            state.offset = {
                Globals::Read<float>(address + Offset::TacticalMapLayout::MinimapX),
                Globals::Read<float>(address + Offset::TacticalMapLayout::MinimapY)
            };
            state.size = {
                Globals::Read<float>(address + Offset::TacticalMapLayout::MinimapWidth),
                Globals::Read<float>(address + Offset::TacticalMapLayout::MinimapHeight)
            };
            state.cachedSize = {
                Globals::Read<float>(address + Offset::TacticalMapLayout::CachedWidth),
                Globals::Read<float>(address + Offset::TacticalMapLayout::CachedHeight)
            };
            state.scale = {
                Globals::Read<float>(address + Offset::TacticalMapLayout::ScaleX),
                Globals::Read<float>(address + Offset::TacticalMapLayout::ScaleY)
            };
        } __except (1) {
            out = {};
            return false;
        }

        if (!IsPlausibleTacticalMap(state)) {
            return false;
        }

        Vec3 center = {};
        if (!state.MinimapToWorld(state.offset + state.size * 0.5f, center)) {
            MapBounds bounds = {};
            if (ReadMapBounds(bounds)) {
                center = bounds.CenterWorldPos();
            }
        }
        state.centerWorldPos = center;
        out = state;
        return true;
    }

    inline bool BuildFallbackState(TacticalMapState& out) {
        out = {};
        MapBounds bounds = {};
        Vec2 renderer = {};
        if (!ReadMapBounds(bounds) || !ReadRendererSize(renderer)) {
            return false;
        }

        const float shortest = std::min(renderer.x, renderer.y);
        const float side = std::clamp(shortest * 0.24f, 180.0f, 340.0f);
        TacticalMapState state = {};
        state.offset = { std::max(0.0f, renderer.x - side - 12.0f),
                         std::max(0.0f, renderer.y - side - 12.0f) };
        state.size = { side, side };
        state.cachedSize = state.size;
        state.negWorld = { -bounds.minX, -bounds.minZ };
        state.scale = { side / bounds.Width(), side / bounds.Height() };
        state.centerWorldPos = bounds.CenterWorldPos();
        state.fallback = true;
        if (!state.IsValid()) {
            return false;
        }

        out = state;
        return true;
    }

    inline bool ResolveTacticalMap(TacticalMapState& out) {
        out = {};
        (void)CoreRuntime::RefreshReadState();
        const auto& ctx = CoreRuntime::GetContext();

        if (TryReadTacticalMap(CachedTacticalMap, out)) {
            return true;
        }
        CachedTacticalMap = 0;

        const uintptr_t hud = ctx.hudInstance;
        if (!Globals::IsReadablePtr(hud, 0x600)) {
            return BuildFallbackState(out);
        }

        constexpr uintptr_t candidateOffsets[] = {
            0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60,
            0x68, 0x70, 0x78, 0x80, 0x88, 0x90, 0x98, 0xA0, 0xA8, 0xB0,
            0xB8, 0xC0, 0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xF0, 0xF8, 0x100,
            0x110, 0x120, 0x130, 0x140, 0x150, 0x160, 0x170, 0x180, 0x190,
            0x1A0, 0x1B0, 0x1C0, 0x1D0, 0x1E0, 0x200, 0x220, 0x240, 0x260,
            0x280, 0x2A0, 0x2B0, 0x2C0, 0x2D0, 0x300, 0x330, 0x360, 0x390,
            0x3C0, 0x400, 0x440, 0x480, 0x4C0, 0x500, 0x530, 0x560
        };

        for (uintptr_t offset : candidateOffsets) {
            const uintptr_t candidate = Globals::Read<uintptr_t>(hud + offset);
            if (TryReadTacticalMap(candidate, out)) {
                CachedTacticalMap = candidate;
                return true;
            }
        }

        return BuildFallbackState(out);
    }

    inline std::uint32_t ReadMissionInfoMapId() {
        if (!CoreRuntime::EnsureInitialized()) {
            return 0;
        }

        auto& ctx = CoreRuntime::g_ctx;
        if (!Globals::IsValidPtr(ctx.missionInfo)) {
            (void)CoreRuntime::RefreshReadState();
        }

        uintptr_t missionInfo = ctx.missionInfo;
        if (!Globals::IsValidPtr(missionInfo)) {
            const uintptr_t base = ctx.moduleBase ? ctx.moduleBase : Globals::base;
            if (!base) {
                return 0;
            }
            missionInfo = Globals::Read<uintptr_t>(
                base + Offset::GameRuntime::MissionInfoInstance);
        }

        return Globals::IsValidPtr(missionInfo)
            ? Globals::Read<std::uint32_t>(missionInfo + Offset::MissionInfo::MapId)
            : 0;
    }
} // namespace detail

inline bool GetMapBounds(MapBounds& out) {
    return detail::ReadMapBounds(out);
}

inline TacticalMapState GetTacticalMap() {
    TacticalMapState state = {};
    (void)detail::ResolveTacticalMap(state);
    return state;
}

inline Vec2 Size() {
    return GetTacticalMap().size;
}

inline Vec2 Offset() {
    return GetTacticalMap().offset;
}

inline Vec3 CenterWorldPos() {
    return GetTacticalMap().centerWorldPos;
}

inline bool WorldToMinimap(const Vec3& world, Vec2& out) {
    TacticalMapState state = {};
    return detail::ResolveTacticalMap(state) && state.WorldToMinimap(world, out);
}

inline Vec2 WorldToMinimap(const Vec3& world) {
    Vec2 out = {};
    (void)WorldToMinimap(world, out);
    return out;
}

inline bool MinimapToWorld(const Vec2& minimap, Vec3& out, float y = 0.0f) {
    TacticalMapState state = {};
    return detail::ResolveTacticalMap(state) && state.MinimapToWorld(minimap, out, y);
}

inline Vec3 MinimapToWorld(const Vec2& minimap, float y = 0.0f) {
    Vec3 out = {};
    (void)MinimapToWorld(minimap, out, y);
    return out;
}

inline MapId GetMapId() {
    switch (detail::ReadMissionInfoMapId()) {
    case static_cast<std::uint32_t>(MapId::SummonersRiftOriginalSummer):
        return MapId::SummonersRiftOriginalSummer;
    case static_cast<std::uint32_t>(MapId::SummonersRiftOriginalAutumn):
        return MapId::SummonersRiftOriginalAutumn;
    case static_cast<std::uint32_t>(MapId::ProvingGrounds):
        return MapId::ProvingGrounds;
    case static_cast<std::uint32_t>(MapId::TwistedTreelineOriginal):
        return MapId::TwistedTreelineOriginal;
    case static_cast<std::uint32_t>(MapId::CrystalScar):
        return MapId::CrystalScar;
    case static_cast<std::uint32_t>(MapId::SummonersRift):
        return MapId::SummonersRift;
    case static_cast<std::uint32_t>(MapId::HowlingAbyss):
        return MapId::HowlingAbyss;
    case static_cast<std::uint32_t>(MapId::TwistedTreeline):
        return MapId::TwistedTreeline;
    case static_cast<std::uint32_t>(MapId::ButchersBridge):
        return MapId::ButchersBridge;
    case static_cast<std::uint32_t>(MapId::CosmicRuins):
        return MapId::CosmicRuins;
    case static_cast<std::uint32_t>(MapId::ValoranCityPark):
        return MapId::ValoranCityPark;
    case static_cast<std::uint32_t>(MapId::Substructure43):
        return MapId::Substructure43;
    case static_cast<std::uint32_t>(MapId::CrashSite):
        return MapId::CrashSite;
    case static_cast<std::uint32_t>(MapId::NexusBlitz):
        return MapId::NexusBlitz;
    case static_cast<std::uint32_t>(MapId::TeamfightTactics):
        return MapId::TeamfightTactics;
    case static_cast<std::uint32_t>(MapId::Arena):
        return MapId::Arena;
    case static_cast<std::uint32_t>(MapId::Swarm):
        return MapId::Swarm;
    case static_cast<std::uint32_t>(MapId::Bandlewood):
        return MapId::Bandlewood;
    default:
        break;
    }

    const auto grid = CoreNavGrid::Get();
    if (!grid.IsValid()) {
        return MapId::Unknown;
    }

    const float width = grid.maxX - grid.minX;
    const float height = grid.maxZ - grid.minZ;
    const float longest = std::max(width, height);
    const float shortest = std::min(width, height);

    if (grid.width >= 280 && grid.height >= 280 && longest > 14000.0f) {
        return MapId::SummonersRift;
    }
    if (longest < 7000.0f && shortest < 7000.0f) {
        return MapId::HowlingAbyss;
    }
    if (longest >= 7000.0f && longest <= 12000.0f) {
        return MapId::TwistedTreeline;
    }
    return MapId::Unknown;
}

} // namespace CoreMap
