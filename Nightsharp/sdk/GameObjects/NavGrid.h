#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "core/Vector.h"

// ============================================================================
// NavGrid — Wall & Bush collision checks via NavigationGrid
//
// Memory layout (patch 15.24):
//   sig: 48 8B 05 ? ? ? ? 0F 28 DA  →  rva 0x1D32A80
//
//   base + Offset::Global::NavGrid              → navGridPtr  (uint64)
//   navGridPtr + Offset::NavGrid::NavGridMgr    → managerPtr  (uint64, +0x8)
//
//   managerPtr layout:
//     +0x0EC  → float  minX
//     +0x0F4  → float  minZ
//     +0x150  → uint64 gridData  (byte-flag array)
//     +0x708  → int    width
//     +0x70C  → int    height
//     +0x714  → float  scale     (NavGridScale)
//
//   gridData[cellZ * width + cellX]:
//     0x00 = open / passable
//     0x01 = wall / terrain (bit 0)
//     0x02 = bush           (bit 1)
//     typically any non-zero bit means "not open terrain"
//     wall flag bit mask = 0x01
//     bush flag bit mask = 0x02
//
// Usage (standalone):
//   auto ng = SDK::NavGrid::Get();
//   if (ng.IsInBush(unit.GetPosition()))  { ... }
//   if (ng.IsWall(pos))                   { ... }
//
// Usage (on GameObject — via GameObject::IsInBush() etc.):
//   see GameObject.h integration section
// ============================================================================

namespace SDK {

    // -------------------------------------------------------------------------
    // NavGrid flag bitmasks (empirically verified for SR)
    // -------------------------------------------------------------------------
    enum class NavCellFlags : uint8_t {
        Open    = 0x00,
        Wall    = 0x01,     // terrain / wall — not passable
        Bush    = 0x02,     // brush / bush — grants vision-block
        //  Additional bits may encode other attributes (river, building, etc.)
        //  but Wall and Bush are the two we test.
    };

    // -------------------------------------------------------------------------
    // NavGrid
    // -------------------------------------------------------------------------
    class NavGrid {
    public:
        // Resolved pointers cached on construction:
        uintptr_t managerPtr;   // manager  (0 if invalid)
        uintptr_t gridData;     // byte-flag array base
        int       width;
        int       height;
        float     scale;
        float     minX;
        float     minZ;
        bool      valid;

        // ------------------------------------------------------------------
        // Default constructor — not initialised
        // ------------------------------------------------------------------
        NavGrid()
            : managerPtr(0), gridData(0),
              width(0), height(0),
              scale(0.0f), minX(0.0f), minZ(0.0f),
              valid(false)
        {}

        // ------------------------------------------------------------------
        // Get() — resolve the complete NavGrid from game memory.
        //         Call once per frame and cache the result.
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

                ng.width  = Globals::Read<int>  (mgr + Offset::NavGrid::Width);
                ng.height = Globals::Read<int>  (mgr + Offset::NavGrid::Height);
                ng.scale  = Globals::Read<float>(mgr + Offset::NavGrid::Scale);
                ng.minX   = Globals::Read<float>(mgr + Offset::NavGrid::MinX);
                ng.minZ   = Globals::Read<float>(mgr + Offset::NavGrid::MinZ);

                uintptr_t data =
                    Globals::Read<uintptr_t>(mgr + Offset::NavGrid::Data);
                if (!Globals::IsValidPtr(data)) return ng;

                ng.gridData = data;

                // Sanity-check the resolved values before trusting them.
                if (ng.width  <= 0 || ng.width  > 10000) return ng;
                if (ng.height <= 0 || ng.height > 10000) return ng;
                if (ng.scale  <= 0.0f || ng.scale > 10.0f)  return ng;

                ng.valid = true;
            } __except(1) { ng.valid = false; }
            return ng;
        }

        // ------------------------------------------------------------------
        // IsValid
        // ------------------------------------------------------------------
        bool IsValid() const { return valid; }

        // ------------------------------------------------------------------
        // WorldToCell — convert a world Vec3 to (cellX, cellZ)
        //   Returns false if the resulting cell is out of bounds.
        // ------------------------------------------------------------------
        bool WorldToCell(const Vec3& pos, int& cellX, int& cellZ) const {
            if (!valid) return false;
            cellX = static_cast<int>((pos.x - minX) * scale);
            cellZ = static_cast<int>((pos.z - minZ) * scale);
            if (cellX < 0 || cellX >= width)  return false;
            if (cellZ < 0 || cellZ >= height) return false;
            return true;
        }

        // ------------------------------------------------------------------
        // GetCellFlags — raw byte flags for the cell at world position.
        //   Returns 0xFF on failure.
        // ------------------------------------------------------------------
        uint8_t GetCellFlags(const Vec3& pos) const {
            int cellX, cellZ;
            if (!WorldToCell(pos, cellX, cellZ)) return 0xFF;
            int index = cellZ * width + cellX;
            __try {
                return Globals::Read<uint8_t>(gridData + static_cast<uintptr_t>(index));
            } __except(1) { return 0xFF; }
        }

        // ------------------------------------------------------------------
        // IsWall — returns true if the cell is terrain / wall (not passable).
        //          Tests bit 0 of the NavGrid cell flag byte.
        // ------------------------------------------------------------------
        bool IsWall(const Vec3& pos) const {
            uint8_t flags = GetCellFlags(pos);
            if (flags == 0xFF) return false; // read error → assume passable
            return (flags & static_cast<uint8_t>(NavCellFlags::Wall)) != 0;
        }

        // ------------------------------------------------------------------
        // IsInBush — returns true if the position is inside a bush/brush.
        //             Tests bit 1 of the NavGrid cell flag byte.
        //
        //   NOTE: a cell can be both wall AND bush (edge-of-terrain brush),
        //         so both bits may be set simultaneously.
        // ------------------------------------------------------------------
        bool IsInBush(const Vec3& pos) const {
            uint8_t flags = GetCellFlags(pos);
            if (flags == 0xFF) return false;
            return (flags & static_cast<uint8_t>(NavCellFlags::Bush)) != 0;
        }

        // ------------------------------------------------------------------
        // IsPassable — open terrain (neither wall nor bush).
        // ------------------------------------------------------------------
        bool IsPassable(const Vec3& pos) const {
            uint8_t flags = GetCellFlags(pos);
            if (flags == 0xFF) return false;
            return flags == 0x00;
        }

        // ------------------------------------------------------------------
        // IsWallBetween — simple ray-step check between two world positions.
        //   Steps along the segment in small increments and tests each cell.
        //   stepSize: world-unit step size (default 50 ≈ ~1 cell at typical scale).
        // ------------------------------------------------------------------
        bool IsWallBetween(const Vec3& from, const Vec3& to, float stepSize = 50.0f) const {
            if (!valid) return false;

            float dx = to.x - from.x;
            float dz = to.z - from.z;
            float dist = sqrtf(dx * dx + dz * dz);
            if (dist < 0.0001f) return IsWall(from);

            float nx = dx / dist;
            float nz = dz / dist;
            float traveled = 0.0f;

            while (traveled <= dist) {
                Vec3 sample = {
                    from.x + nx * traveled,
                    from.y,
                    from.z + nz * traveled
                };
                if (IsWall(sample)) return true;
                traveled += stepSize;
            }
            // Always test the exact endpoint
            return IsWall(to);
        }

        // ------------------------------------------------------------------
        // HasLineOfSight — true if there is NO wall between from and to.
        // ------------------------------------------------------------------
        bool HasLineOfSight(const Vec3& from, const Vec3& to, float stepSize = 50.0f) const {
            return !IsWallBetween(from, to, stepSize);
        }
    }; // class NavGrid

} // namespace SDK
