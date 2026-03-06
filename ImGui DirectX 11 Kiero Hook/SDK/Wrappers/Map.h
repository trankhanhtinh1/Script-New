#pragma once
// ============================================================================
// Map.h — Map Wrapper (EnsoulSharp.SDK/Core/Wrappers/Map.cs port)
// ============================================================================
// Provides:
//   - GameMapId enum: SummonersRift, HowlingAbyss, TwistedTreeline, etc.
//   - GetMapType() — detect current map
//   - IsOnSummonersRift(), IsOnHowlingAbyss(), IsOnARAM()
//   - MapBounds — min/max world coordinates per map
//   - SpawnPoints — team spawn positions
//   - JungleCampPositions — static jungle camp positions
//   - MapGrid — grid dimensions from Map.json
//
// Usage:
//   if (SDK::Map::GetMapType() == SDK::GameMapId::SummonersRift) {
//       Vec3 spawn = SDK::Map::GetAllySpawnPoint();
//       Vec3 baron = SDK::Map::GetBaronPosition();
//   }
// ============================================================================

#include "core/Globals.h"
#include "core/Offsets.h"
#include "core/Vector.h"
#include "GameObjects/GameObjects.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace SDK {

    // ========================================================================
    // GameMapId — Map identification
    // ========================================================================
    enum class GameMapId : int {
        Unknown         = 0,
        SummonersRift   = 11,
        HowlingAbyss    = 12,   // ARAM
        TwistedTreeline = 10,   // Removed, but kept for reference
        CrystalScar     = 8,    // Dominion (removed)
        TFT             = 22,   // Teamfight Tactics
        Arena           = 30,   // 2v2v2v2 Arena
        NexusBlitz      = 21,   // Nexus Blitz
        Swarm           = 33,   // PvE Swarm mode
    };

    // ========================================================================
    // MapBounds — World coordinate boundaries
    // ========================================================================
    struct MapBounds {
        float MinX, MinZ;   // Bottom-left corner
        float MaxX, MaxZ;   // Top-right corner

        float Width() const { return MaxX - MinX; }
        float Height() const { return MaxZ - MinZ; }
        Vec3 Center() const {
            return Vec3((MinX + MaxX) / 2.0f, 0, (MinZ + MaxZ) / 2.0f);
        }
        bool Contains(const Vec3& pos) const {
            return pos.x >= MinX && pos.x <= MaxX && pos.z >= MinZ && pos.z <= MaxZ;
        }
    };

    // ========================================================================
    // Map — Static map utility class
    // ========================================================================
    class Map {
    public:
        // ====================================================================
        // Map Detection
        // ====================================================================

        /// Initialize map detection (call once at game start)
        static void Init() {
            s_mapType = DetectMapFromBounds();
            if (s_mapType == GameMapId::Unknown)
                s_mapType = GameMapId::SummonersRift;
            s_initialized = true;
        }

        static GameMapId GetMap() {
            if (!s_initialized) Init();
            return s_mapType;
        }

        // ====================================================================
        // Quick checks
        // ====================================================================

        static bool IsOnSummonersRift() { return GetMap() == GameMapId::SummonersRift; }
        static bool IsOnHowlingAbyss() { return GetMap() == GameMapId::HowlingAbyss; }
        static bool IsOnARAM() { return IsOnHowlingAbyss(); }
        static bool IsOnTFT() { return GetMap() == GameMapId::TFT; }
        static bool IsOnArena() { return GetMap() == GameMapId::Arena; }

        // ====================================================================
        // Map Bounds
        // ====================================================================

        /// Get map boundaries for the current map
        static MapBounds GetBounds() {
            return GetBoundsForMap(GetMap());
        }

        /// Get map boundaries for a specific map
        static MapBounds GetBoundsForMap(GameMapId mapId) {
            switch (mapId) {
            case GameMapId::SummonersRift:
                return { -120.0f, -120.0f, 14870.0f, 14980.0f };
            case GameMapId::HowlingAbyss:
                return { -28.0f, -19.0f, 12849.0f, 12858.0f };
            case GameMapId::TwistedTreeline:
                return { 0.0f, 0.0f, 15398.0f, 15398.0f };
            default:
                return { -120.0f, -120.0f, 14870.0f, 14980.0f }; // Default to SR
            }
        }

        /// Check if a position is within the current map boundaries
        static bool IsInBounds(const Vec3& pos) {
            return GetBounds().Contains(pos);
        }

        // ====================================================================
        // Grid (from Map.json data)
        // ====================================================================

        /// Get map grid center
        static Vec2 GetGridCenter() {
            switch (GetMap()) {
            case GameMapId::SummonersRift:  return Vec2(7410.0f, 7318.0f);
            case GameMapId::HowlingAbyss:   return Vec2(6560.0f, 6309.0f);
            case GameMapId::TwistedTreeline: return Vec2(7700.0f, 7237.0f);
            default: return Vec2(7410.0f, 7318.0f);
            }
        }

        /// Get starting level for the current map
        static int GetStartingLevel() {
            switch (GetMap()) {
            case GameMapId::HowlingAbyss: return 3;
            default: return 1;
            }
        }

        // ====================================================================
        // Spawn Points (Summoner's Rift)
        // ====================================================================

        /// Get Blue side (Order) spawn point
        static Vec3 GetBlueSpawnPoint() {
            return Vec3(394.0f, 182.0f, 462.0f);
        }

        /// Get Red side (Chaos) spawn point
        static Vec3 GetRedSpawnPoint() {
            return Vec3(14340.0f, 171.0f, 14390.0f);
        }

        /// Get ally spawn point (based on local player team)
        static Vec3 GetAllySpawnPoint() {
            if (!GameObjects::Player.IsValid()) return GetBlueSpawnPoint();
            return (GameObjects::Player.GetTeam() == GameObjectTeam::Blue)
                ? GetBlueSpawnPoint()
                : GetRedSpawnPoint();
        }

        /// Get enemy spawn point (based on local player team)
        static Vec3 GetEnemySpawnPoint() {
            if (!GameObjects::Player.IsValid()) return GetRedSpawnPoint();
            return (GameObjects::Player.GetTeam() == GameObjectTeam::Blue)
                ? GetRedSpawnPoint()
                : GetBlueSpawnPoint();
        }

        // ====================================================================
        // ARAM Spawn Points
        // ====================================================================

        static Vec3 GetARAMBlueSpawn() {
            return Vec3(1055.0f, 0.0f, 1170.0f);
        }

        static Vec3 GetARAMRedSpawn() {
            return Vec3(12100.0f, 0.0f, 12000.0f);
        }

        // ====================================================================
        // Jungle Camp Positions (Summoner's Rift)
        // ====================================================================

        /// Baron Nashor pit center
        static Vec3 GetBaronPosition() {
            return Vec3(4950.0f, -71.0f, 10400.0f);
        }

        /// Dragon pit center
        static Vec3 GetDragonPosition() {
            return Vec3(9866.0f, -71.0f, 4414.0f);
        }

        /// Rift Herald / Baron pit (same position)
        static Vec3 GetRiftHeraldPosition() {
            return GetBaronPosition();
        }

        /// Blue side Blue Buff
        static Vec3 GetBlueBuffBlue() {
            return Vec3(3821.0f, 51.0f, 7901.0f);
        }

        /// Blue side Red Buff
        static Vec3 GetRedBuffBlue() {
            return Vec3(7862.0f, 56.0f, 4112.0f);
        }

        /// Red side Blue Buff
        static Vec3 GetBlueBuffRed() {
            return Vec3(10984.0f, 51.0f, 6910.0f);
        }

        /// Red side Red Buff
        static Vec3 GetRedBuffRed() {
            return Vec3(7016.0f, 56.0f, 10775.0f);
        }

        /// Blue side Gromp
        static Vec3 GetGrompBlue() {
            return Vec3(2288.0f, 51.0f, 8448.0f);
        }

        /// Red side Gromp
        static Vec3 GetGrompRed() {
            return Vec3(12703.0f, 51.0f, 6444.0f);
        }

        /// Blue side Wolves
        static Vec3 GetWolvesBlue() {
            return Vec3(3780.0f, 52.0f, 6443.0f);
        }

        /// Red side Wolves
        static Vec3 GetWolvesRed() {
            return Vec3(11008.0f, 62.0f, 8380.0f);
        }

        /// Blue side Raptors (Chickens)
        static Vec3 GetRaptorsBlue() {
            return Vec3(6974.0f, 52.0f, 5460.0f);
        }

        /// Red side Raptors
        static Vec3 GetRaptorsRed() {
            return Vec3(7852.0f, 52.0f, 9468.0f);
        }

        /// Blue side Krugs
        static Vec3 GetKrugsBlue() {
            return Vec3(8370.0f, 50.0f, 2716.0f);
        }

        /// Red side Krugs
        static Vec3 GetKrugsRed() {
            return Vec3(6480.0f, 56.0f, 12188.0f);
        }

        /// Rift Scuttler (bot/dragon river)
        static Vec3 GetScuttleDragon() {
            return Vec3(10500.0f, -62.0f, 5170.0f);
        }

        /// Rift Scuttler (top/baron river)
        static Vec3 GetScuttleBaron() {
            return Vec3(4400.0f, -66.0f, 9600.0f);
        }

        /// Get all jungle camp positions (for minimap drawing)
        static std::vector<std::pair<std::string, Vec3>> GetAllJungleCampPositions() {
            return {
                {"Baron",       GetBaronPosition()},
                {"Dragon",      GetDragonPosition()},
                {"Blue (Blue)", GetBlueBuffBlue()},
                {"Red (Blue)",  GetRedBuffBlue()},
                {"Blue (Red)",  GetBlueBuffRed()},
                {"Red (Red)",   GetRedBuffRed()},
                {"Gromp (B)",   GetGrompBlue()},
                {"Gromp (R)",   GetGrompRed()},
                {"Wolves (B)",  GetWolvesBlue()},
                {"Wolves (R)",  GetWolvesRed()},
                {"Raptors (B)", GetRaptorsBlue()},
                {"Raptors (R)", GetRaptorsRed()},
                {"Krugs (B)",   GetKrugsBlue()},
                {"Krugs (R)",   GetKrugsRed()},
                {"Scuttle Bot", GetScuttleDragon()},
                {"Scuttle Top", GetScuttleBaron()},
            };
        }

        // ====================================================================
        // Lane Positions (Summoner's Rift)
        // ====================================================================

        /// Get mid lane center
        static Vec3 GetMidLaneCenter() {
            return Vec3(7500.0f, 50.0f, 7500.0f);
        }

        /// Check if position is in river area
        static bool IsInRiver(const Vec3& pos) {
            // River runs diagonally from bottom-right to top-left
            // Approximate: positions near the diagonal line within ~1500 units
            float diag = pos.x + pos.z;
            float riverCenter = 14800.0f; // x+z ≈ 14800 along river
            return std::abs(diag - riverCenter) < 3000.0f
                && pos.x > 3000.0f && pos.x < 12000.0f;
        }

        /// Check if position is near dragon pit
        static bool IsNearDragonPit(const Vec3& pos, float range = 2000.0f) {
            return pos.Distance2D(GetDragonPosition()) <= range;
        }

        /// Check if position is near baron pit
        static bool IsNearBaronPit(const Vec3& pos, float range = 2000.0f) {
            return pos.Distance2D(GetBaronPosition()) <= range;
        }

        // ====================================================================
        // Tower Positions (Summoner's Rift, static reference positions)
        // ====================================================================

        struct TowerPosition {
            std::string Name;
            Vec3 Position;
            GameObjectTeam Team;
        };

        static std::vector<TowerPosition> GetTowerPositions() {
            return {
                // Blue side towers
                {"Blue_Top_Outer",    Vec3(981.0f, 0, 10441.0f),   GameObjectTeam::Blue},
                {"Blue_Top_Inner",    Vec3(1512.0f, 0, 6699.0f),   GameObjectTeam::Blue},
                {"Blue_Top_Inhib",    Vec3(1169.0f, 0, 4287.0f),   GameObjectTeam::Blue},
                {"Blue_Mid_Outer",    Vec3(5846.0f, 0, 6396.0f),   GameObjectTeam::Blue},
                {"Blue_Mid_Inner",    Vec3(5048.0f, 0, 4812.0f),   GameObjectTeam::Blue},
                {"Blue_Mid_Inhib",    Vec3(3651.0f, 0, 3696.0f),   GameObjectTeam::Blue},
                {"Blue_Bot_Outer",    Vec3(6919.0f, 0, 1483.0f),   GameObjectTeam::Blue},
                {"Blue_Bot_Inner",    Vec3(4281.0f, 0, 1253.0f),   GameObjectTeam::Blue},
                {"Blue_Bot_Inhib",    Vec3(2177.0f, 0, 1807.0f),   GameObjectTeam::Blue},
                {"Blue_Nexus_Top",    Vec3(1748.0f, 0, 2270.0f),   GameObjectTeam::Blue},
                {"Blue_Nexus_Bot",    Vec3(2177.0f, 0, 1807.0f),   GameObjectTeam::Blue},

                // Red side towers
                {"Red_Top_Outer",     Vec3(7943.0f, 0, 13411.0f),  GameObjectTeam::Red},
                {"Red_Top_Inner",     Vec3(10481.0f, 0, 13650.0f), GameObjectTeam::Red},
                {"Red_Top_Inhib",     Vec3(12611.0f, 0, 13084.0f), GameObjectTeam::Red},
                {"Red_Mid_Outer",     Vec3(8955.0f, 0, 8510.0f),   GameObjectTeam::Red},
                {"Red_Mid_Inner",     Vec3(9767.0f, 0, 10113.0f),  GameObjectTeam::Red},
                {"Red_Mid_Inhib",     Vec3(11134.0f, 0, 11207.0f), GameObjectTeam::Red},
                {"Red_Bot_Outer",     Vec3(13866.0f, 0, 4505.0f),  GameObjectTeam::Red},
                {"Red_Bot_Inner",     Vec3(13327.0f, 0, 8226.0f),  GameObjectTeam::Red},
                {"Red_Bot_Inhib",     Vec3(13624.0f, 0, 10572.0f), GameObjectTeam::Red},
                {"Red_Nexus_Top",     Vec3(12920.0f, 0, 12525.0f), GameObjectTeam::Red},
                {"Red_Nexus_Bot",     Vec3(13052.0f, 0, 12612.0f), GameObjectTeam::Red},
            };
        }

    private:
        static inline GameMapId s_mapType = GameMapId::Unknown;
        static inline bool s_initialized = false;

        /// Detect map type based on NavGrid bounds
        static GameMapId DetectMapFromBounds() {
            __try {
                // Read NavGrid to get map dimensions
                uintptr_t navGridPtr = Globals::Read<uintptr_t>(
                    Globals::base + Offset::Global::NavGrid);
                if (!Globals::IsValidPtr(navGridPtr)) return GameMapId::Unknown;

                uintptr_t navGridMgr = Globals::Read<uintptr_t>(
                    navGridPtr + Offset::NavGrid::NavGridMgr);
                if (!Globals::IsValidPtr(navGridMgr)) return GameMapId::Unknown;

                int width = Globals::Read<int>(navGridMgr + Offset::NavGrid::Width);
                int height = Globals::Read<int>(navGridMgr + Offset::NavGrid::Height);

                // Summoner's Rift: ~296x298 cells (14820x14900 / 50)
                // Howling Abyss: ~256x252 cells (12820x12580 / 50)
                // TwistedTreeline: ~308x308 cells

                if (width > 280 && width < 310 && height > 285 && height < 310) {
                    return GameMapId::SummonersRift;
                }
                if (width > 240 && width < 270 && height > 238 && height < 265) {
                    return GameMapId::HowlingAbyss;
                }
                if (width > 300 && height > 300 && width < 320) {
                    return GameMapId::TwistedTreeline;
                }

                return GameMapId::Unknown;
            } __except(1) {
                return GameMapId::Unknown;
            }
        }
    };

} // namespace SDK
