#pragma once
#include <windows.h>
#include <cmath>
#include <vector>
#include "Offsets.h"
#include "../Vector.h"

namespace SDK
{
    // ============================================================================
    // COLLISIONABLE OBJECTS FLAGS
    // Matching EnsoulSharp.SDK/Core/Enumerations/CollisionableObjects.cs
    // ============================================================================
    enum class CollisionableObjects : int
    {
        None = 0,
        Minions = 1 << 0,     // Check collision with minions
        Heroes = 1 << 1,      // Check collision with enemy heroes
        YasuoWall = 1 << 2,   // Check collision with Yasuo's Wind Wall
        BraumShield = 1 << 3, // Check collision with Braum's Unbreakable
        Walls = 1 << 4,       // Check collision with terrain walls
        
        // Common combinations
        AllUnits = Minions | Heroes,
        AllBlockers = YasuoWall | BraumShield,
        Default = Minions | YasuoWall | Walls
    };
    
    // Bitwise operators for flags
    inline CollisionableObjects operator|(CollisionableObjects a, CollisionableObjects b) {
        return static_cast<CollisionableObjects>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline CollisionableObjects operator&(CollisionableObjects a, CollisionableObjects b) {
        return static_cast<CollisionableObjects>(static_cast<int>(a) & static_cast<int>(b));
    }
    inline bool HasFlag(CollisionableObjects value, CollisionableObjects flag) {
        return (static_cast<int>(value) & static_cast<int>(flag)) != 0;
    }

    // ============================================================================
    // NAVGRID CLASS - Direct navigation grid data reading
    // Alternative method for bush/wall detection without calling game functions
    // Source: unknowncheats.me forum (Dunkrius method)
    // ============================================================================
    class NavGrid
    {
    public:
        // Get NavGrid Manager pointer
        static uint64_t GetManager()
        {
            uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
            uint64_t navGrid = *(uint64_t*)(moduleBase + Offset::NavigationGrid::GlobalPtr);
            if (!navGrid) return 0;
            return *(uint64_t*)(navGrid + Offset::NavigationGrid::Manager::Manager);
        }

        // Get grid dimensions and scale
        static int GetWidth()
        {
            uint64_t manager = GetManager();
            if (!manager) return 0;
            return *(int*)(manager + Offset::NavigationGrid::Manager::Width);
        }

        static int GetHeight()
        {
            uint64_t manager = GetManager();
            if (!manager) return 0;
            return *(int*)(manager + Offset::NavigationGrid::Manager::Height);
        }

        static float GetScale()
        {
            uint64_t manager = GetManager();
            if (!manager) return 0;
            return *(float*)(manager + Offset::NavigationGrid::Manager::Scale);
        }

        static float GetMinX()
        {
            uint64_t manager = GetManager();
            if (!manager) return 0;
            return *(float*)(manager + Offset::NavigationGrid::Manager::MinimumX);
        }

        static float GetMinZ()
        {
            uint64_t manager = GetManager();
            if (!manager) return 0;
            return *(float*)(manager + Offset::NavigationGrid::Manager::MinimumZ);
        }

        static uint64_t GetGridData()
        {
            uint64_t manager = GetManager();
            if (!manager) return 0;
            return *(uint64_t*)(manager + Offset::NavigationGrid::Manager::Data);
        }

        // ========================================================================
        // CELL-BASED CHECKS (Direct memory read, no function call)
        // ========================================================================
        
        // Get cell coordinates from world position
        static bool GetCellCoords(Vector3 pos, int& cellX, int& cellZ)
        {
            uint64_t manager = GetManager();
            if (!manager) return false;

            float scale = *(float*)(manager + Offset::NavigationGrid::Manager::Scale);
            float minX = *(float*)(manager + Offset::NavigationGrid::Manager::MinimumX);
            float minZ = *(float*)(manager + Offset::NavigationGrid::Manager::MinimumZ);
            int width = *(int*)(manager + Offset::NavigationGrid::Manager::Width);
            int height = *(int*)(manager + Offset::NavigationGrid::Manager::Height);

            cellX = static_cast<int>((pos.x - minX) * scale);
            cellZ = static_cast<int>((pos.z - minZ) * scale);

            return (cellX >= 0 && cellX < width && cellZ >= 0 && cellZ < height);
        }

        // Get flags at cell position
        static uint8_t GetCellFlags(int cellX, int cellZ)
        {
            uint64_t manager = GetManager();
            if (!manager) return 0;

            int width = *(int*)(manager + Offset::NavigationGrid::Manager::Width);
            uint64_t gridData = *(uint64_t*)(manager + Offset::NavigationGrid::Manager::Data);
            if (!gridData) return 0;

            int index = cellZ * width + cellX;
            return *(uint8_t*)(gridData + index);
        }

        // Get flags at world position
        static uint8_t GetFlagsAtPosition(Vector3 pos)
        {
            int cellX, cellZ;
            if (!GetCellCoords(pos, cellX, cellZ)) return 0;
            return GetCellFlags(cellX, cellZ);
        }

        // ========================================================================
        // BUSH/WALL DETECTION (NavGrid method - no function call!)
        // ========================================================================
        
        // Check if position is in bush using NavGrid (flags != 0)
        static bool IsInBush(Vector3 pos)
        {
            return GetFlagsAtPosition(pos) != 0;
        }

        // Check if position is NOT walkable (wall)
        static bool IsNotWall(Vector3 pos)
        {
            // If we can get valid cell coords, it's not a wall
            int cellX, cellZ;
            if (!GetCellCoords(pos, cellX, cellZ)) return false;
            
            // Check the flags - specific implementation may vary
            // Usually 0 means walkable, other values have different meanings
            uint8_t flags = GetCellFlags(cellX, cellZ);
            
            // This is a simplified check - you may need to test specific flag values
            return true; // If we got here, position is in valid grid = not wall
        }

        // Check if object at position is in bush
        static bool IsObjectInBush(uint64_t objAddress)
        {
            // Read object position
            Vector3 pos;
            pos.x = *(float*)(objAddress + Offset::oObjPosition);
            pos.y = *(float*)(objAddress + Offset::oObjPosition + 0x4);
            pos.z = *(float*)(objAddress + Offset::oObjPosition + 0x8);
            
            return IsInBush(pos);
        }
    };

    // ============================================================================
    // COLLISION FLAGS - Terrain collision types
    // Based on leagueoflegends-master/global/structs.h
    // ============================================================================
    enum CollisionFlags : unsigned int
    {
        Flag_None = 0,
        Flag_Grass = 1,        // Bush/Grass (hides vision)
        Flag_Wall = 2,         // Wall (blocks movement and most skillshots)
        Flag_Building = 64,    // Building structure
        Flag_Tower = 70,       // Tower/Turret
        Flag_Prop = 128,       // Prop/Decoration
        Flag_GlobalVision = 256 // Global vision area
    };

    // ============================================================================
    // COLLISION CLASS - Terrain checking and skillshot collision
    // Based on leagueoflegends-master/global/functions.cpp
    // ============================================================================
    class Collision
    {
    public:
        // ========================================================================
        // GET COLLISION FLAGS AT POSITION
        // Returns collision flags at a specific world position
        // ========================================================================
        static unsigned int GetCollisionFlags(Vector3 pos)
        {
            uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
            
            // Function signature: unsigned int __fastcall GetCollisionFlags(Vector3 pos)
            typedef unsigned int(__fastcall* fnGetCollisionFlags)(Vector3 pos);
            fnGetCollisionFlags _fnGetCollisionFlags = (fnGetCollisionFlags)(moduleBase + Offset::Function::oGetCollisionFlags);
            
            __try {
                return _fnGetCollisionFlags(pos);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

        // ========================================================================
        // TERRAIN CHECKS
        // ========================================================================
        
        // Check if position is inside a bush/grass
        static bool IsBrush(Vector3 pos)
        {
            return (GetCollisionFlags(pos) & Flag_Grass) != 0;
        }
        
        // Alias for IsBrush
        static bool IsGrass(Vector3 pos)
        {
            return IsBrush(pos);
        }

        // Check if position is a wall
        static bool IsWall(Vector3 pos)
        {
            return (GetCollisionFlags(pos) & Flag_Wall) != 0;
        }

        // Check if position is walkable (not a wall)
        static bool IsWalkable(Vector3 pos)
        {
            unsigned int flags = GetCollisionFlags(pos);
            return (flags & Flag_Wall) == 0 && (flags & Flag_Building) == 0;
        }

        // Check if position is inside a building
        static bool IsBuilding(Vector3 pos)
        {
            return (GetCollisionFlags(pos) & Flag_Building) != 0;
        }

        // Check if position is inside a tower range
        static bool IsTower(Vector3 pos)
        {
            return (GetCollisionFlags(pos) & Flag_Tower) != 0;
        }

        // ========================================================================
        // LINE COLLISION CHECKS
        // Check if a line between two points intersects with terrain
        // ========================================================================
        
        // Check if line from->to intersects with any wall
        static bool LineCollidesWithWall(Vector3 from, Vector3 to, float step = 25.0f)
        {
            Vector3 direction = to - from;
            float distance = direction.Length();
            
            if (distance < step) {
                return IsWall(to);
            }
            
            // Normalize direction
            direction = direction / distance;
            
            // Check points along the line
            for (float d = 0; d < distance; d += step) {
                Vector3 checkPos = from + (direction * d);
                if (IsWall(checkPos)) {
                    return true;
                }
            }
            
            // Check final point
            return IsWall(to);
        }

        // Get the first wall collision point on a line
        static Vector3 GetFirstWallCollision(Vector3 from, Vector3 to, float step = 25.0f)
        {
            Vector3 direction = to - from;
            float distance = direction.Length();
            
            if (distance < step) {
                if (IsWall(to)) return to;
                return Vector3(0, 0, 0); // No collision
            }
            
            direction = direction / distance;
            
            for (float d = 0; d < distance; d += step) {
                Vector3 checkPos = from + (direction * d);
                if (IsWall(checkPos)) {
                    return checkPos;
                }
            }
            
            if (IsWall(to)) return to;
            return Vector3(0, 0, 0); // No collision
        }

        // ========================================================================
        // SKILLSHOT COLLISION HELPERS  
        // ========================================================================
        
        // Check if a linear skillshot path collides with walls
        static bool SkillshotCollidesWithWall(Vector3 from, Vector3 to, float width = 70.0f)
        {
            // Check center line
            if (LineCollidesWithWall(from, to)) return true;
            
            // Check edges of skillshot
            Vector3 direction = (to - from);
            float distance = direction.Length();
            if (distance < 1.0f) return false;
            
            direction = direction / distance;
            
            // Perpendicular vector (for width)
            Vector3 perpendicular = Vector3(-direction.z, 0, direction.x);
            
            // Check left edge
            Vector3 leftFrom = from + (perpendicular * (width / 2.0f));
            Vector3 leftTo = to + (perpendicular * (width / 2.0f));
            if (LineCollidesWithWall(leftFrom, leftTo)) return true;
            
            // Check right edge
            Vector3 rightFrom = from - (perpendicular * (width / 2.0f));
            Vector3 rightTo = to - (perpendicular * (width / 2.0f));
            if (LineCollidesWithWall(rightFrom, rightTo)) return true;
            
            return false;
        }

        // Calculate path length that doesn't hit wall
        static float GetPathLengthBeforeWall(Vector3 from, Vector3 to, float step = 25.0f)
        {
            Vector3 direction = to - from;
            float distance = direction.Length();
            
            if (distance < step) {
                if (IsWall(to)) return 0;
                return distance;
            }
            
            direction = direction / distance;
            
            for (float d = 0; d < distance; d += step) {
                Vector3 checkPos = from + (direction * d);
                if (IsWall(checkPos)) {
                    return d;
                }
            }
            
            return distance;
        }

        // ========================================================================
        // VISION HELPERS
        // ========================================================================
        
        // Check if target is in a bush from source perspective
        static bool IsTargetInBush(Vector3 source, Vector3 target)
        {
            // Target is in bush if the target position is in grass
            // but source is not in same grass area
            return IsBrush(target) && !IsBrush(source);
        }

        // Check if we have vision (not blocked by wall)
        static bool HasLineOfSight(Vector3 from, Vector3 to)
        {
            return !LineCollidesWithWall(from, to, 50.0f);
        }

        // ========================================================================
        // UNIT COLLISION DETECTION
        // For skillshot prediction - check collision with minions/champions
        // ========================================================================
        
        // Check if a point is within collision range of a unit
        static bool PointCollidesWithUnit(Vector3 point, Vector3 unitPos, float unitRadius, float skillshotWidth)
        {
            float dx = point.x - unitPos.x;
            float dz = point.z - unitPos.z;
            float distSq = dx * dx + dz * dz;
            float collisionRadius = skillshotWidth + unitRadius;
            return distSq <= (collisionRadius * collisionRadius);
        }

        // Check if a line segment collides with a unit (circular hitbox)
        // Returns true if skillshot from->to would hit the unit
        static bool LineCollidesWithUnit(Vector3 from, Vector3 to, Vector3 unitPos, float unitRadius, float skillshotWidth = 0.0f)
        {
            // Vector from->to
            Vector3 lineVec = to - from;
            float lineLength = lineVec.Length();
            if (lineLength < 1.0f) return false;
            
            // Vector from->unit
            Vector3 toUnit = unitPos - from;
            
            // Project unit position onto line
            float t = (toUnit.x * lineVec.x + toUnit.z * lineVec.z) / (lineLength * lineLength);
            
            // Clamp to line segment
            t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
            
            // Closest point on line to unit
            Vector3 closestPoint = from + (lineVec * t);
            
            // Check distance
            float dx = closestPoint.x - unitPos.x;
            float dz = closestPoint.z - unitPos.z;
            float distSq = dx * dx + dz * dz;
            
            float collisionRadius = unitRadius + skillshotWidth;
            return distSq <= (collisionRadius * collisionRadius);
        }
    };

    // ============================================================================
    // YASUO WIND WALL TRACKER
    // Tracks enemy Yasuo's Wind Wall (W) spell for collision detection
    // 
    // DETECTION METHODS:
    // 1. Manual: Call OnYasuoWCast() when you hook spell cast
    // 2. Auto: Call ScanForWindWall() with ObjectManager to find wall GameObject
    // 
    // Wall GameObject name pattern: "*windwall*" or "_w_windwall_enemy_0.troy"
    // ============================================================================
    class YasuoWallTracker
    {
    private:
        static inline uint32_t s_WallCastTick = 0;
        static inline Vector3 s_WallCastPos = Vector3(0, 0, 0);
        static inline Vector3 s_YasuoPos = Vector3(0, 0, 0);
        static inline Vector3 s_WallPosition = Vector3(0, 0, 0);
        static inline int s_WallLevel = 1;
        static inline bool s_WallActive = false;

    public:
        // ========================================================================
        // METHOD 1: Manual trigger (from spell cast hook)
        // ========================================================================
        static void OnYasuoWCast(Vector3 yasuoPos, Vector3 castPos, int level)
        {
            s_WallCastTick = GetTickCount();
            s_YasuoPos = yasuoPos;
            s_WallCastPos = castPos;
            s_WallLevel = level;
            s_WallActive = true;
        }

        // ========================================================================
        // METHOD 2: Auto-detect from ObjectManager (call every frame)
        // Pass in list of all game objects and check for windwall name pattern
        // ========================================================================
        struct WallInfo {
            bool found;
            Vector3 position;
            int level;  // Extracted from name if possible
        };

        // Check if object name contains windwall pattern
        static bool IsWindWallObject(const std::string& name)
        {
            // Convert to lowercase for comparison
            std::string lower = name;
            for (auto& c : lower) c = (char)tolower(c);
            
            // LeagueSharp pattern: "_w_windwall_enemy_0.troy" where . is level digit
            // Also check for simpler patterns
            return (lower.find("windwall") != std::string::npos) ||
                   (lower.find("wind_wall") != std::string::npos);
        }

        // Extract level from wall name (e.g., "..._01.troy" -> level 1)
        static int ExtractLevelFromName(const std::string& name)
        {
            // Pattern: "_w_windwall_enemy_0X.troy" where X is level (1-5)
            // Or look for last digit before ".troy"
            size_t troyPos = name.rfind(".troy");
            if (troyPos != std::string::npos && troyPos > 0) {
                char c = name[troyPos - 1];
                if (c >= '1' && c <= '5') {
                    return c - '0';
                }
            }
            return 1; // Default level
        }

        // Update from wall GameObject (call when you find wall in ObjectManager)
        static void UpdateFromWallObject(Vector3 wallPos, const std::string& wallName, Vector3 yasuoCasterPos)
        {
            s_WallCastTick = GetTickCount();
            s_WallPosition = wallPos;
            s_YasuoPos = yasuoCasterPos;
            s_WallCastPos = wallPos; // Wall position is cast direction
            s_WallLevel = ExtractLevelFromName(wallName);
            s_WallActive = true;
        }

        // Wall exists for 4 seconds (3.75s + travel time)
        static bool IsWallActive()
        {
            if (!s_WallActive) return false;
            if (GetTickCount() - s_WallCastTick > 4000) {
                s_WallActive = false;
                return false;
            }
            return true;
        }

        // Get wall width based on level (300 + 50 * level)
        static float GetWallWidth()
        {
            return 300.0f + 50.0f * s_WallLevel;
        }

        // Get wall center position
        static Vector3 GetWallCenter()
        {
            // If updated from wall object, use direct position
            if (s_WallPosition.x != 0 || s_WallPosition.z != 0) {
                return s_WallPosition;
            }
            // Otherwise calculate from Yasuo cast direction
            Vector3 direction = s_WallCastPos - s_YasuoPos;
            float len = direction.Length();
            if (len < 1.0f) return s_WallCastPos;
            direction = direction / len;
            return s_YasuoPos + (direction * 400.0f); // Wall spawns 400 units away
        }

        // Get wall direction (perpendicular to cast direction)
        static Vector3 GetWallDirection()
        {
            Vector3 direction = s_WallCastPos - s_YasuoPos;
            float len = direction.Length();
            if (len < 1.0f) return Vector3(1, 0, 0);
            direction = direction / len;
            // Perpendicular in XZ plane
            return Vector3(-direction.z, 0, direction.x);
        }

        // Check if a line from->to intersects with wind wall
        static bool CollidesWithWall(Vector3 from, Vector3 to)
        {
            if (!IsWallActive()) return false;

            Vector3 wallCenter = GetWallCenter();
            Vector3 wallDir = GetWallDirection();
            float halfWidth = GetWallWidth() / 2.0f;

            // Wall endpoints
            Vector3 wallStart = wallCenter + (wallDir * halfWidth);
            Vector3 wallEnd = wallCenter - (wallDir * halfWidth);

            // Line-line intersection (2D, XZ plane)
            float x1 = from.x, z1 = from.z;
            float x2 = to.x, z2 = to.z;
            float x3 = wallStart.x, z3 = wallStart.z;
            float x4 = wallEnd.x, z4 = wallEnd.z;

            float denom = (x1 - x2) * (z3 - z4) - (z1 - z2) * (x3 - x4);
            if (denom == 0) return false; // Parallel lines

            float t = ((x1 - x3) * (z3 - z4) - (z1 - z3) * (x3 - x4)) / denom;
            float u = -((x1 - x2) * (z1 - z3) - (z1 - z2) * (x1 - x3)) / denom;

            // Check if intersection is within both line segments
            return (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f);
        }

        // Reset tracker (when game starts or Yasuo dies)
        static void Reset()
        {
            s_WallActive = false;
            s_WallCastTick = 0;
            s_WallPosition = Vector3(0, 0, 0);
        }

        // Getters for debug
        static Vector3 GetYasuoPos() { return s_YasuoPos; }
        static uint32_t GetLastCastTick() { return s_WallCastTick; }
        static int GetWallLevel() { return s_WallLevel; }
    };

    // ============================================================================
    // BRAUM SHIELD TRACKER
    // Tracks Braum's Unbreakable (E) shield
    //
    // DETECTION: Check buff "BraumShieldRaise" on Braum
    // BLOCKING: Shield blocks projectiles from the direction Braum is facing
    // FACING: Use AiManager.GetMoveDirection() or calculate from target pos
    // ============================================================================
    class BraumShieldTracker
    {
    public:
        // Buff name for Braum's E shield
        static constexpr const char* SHIELD_BUFF_NAME = "BraumShieldRaise";

        // Check if Braum has shield active using BuffManager
        // Usage: BraumShieldTracker::IsShieldActive(braumObj.Address, gameTime);
        // Note: Requires including BuffManager.h
        /*
        static bool IsShieldActive(uint64_t braumAddress, float gameTime)
        {
            BuffManager buffs(braumAddress);
            return buffs.HasBuff(SHIELD_BUFF_NAME, gameTime);
        }
        */

        // Alternative: Check if any buff name starts with "BraumShield"
        static bool IsShieldBuffName(const std::string& buffName)
        {
            return buffName.find("BraumShield") != std::string::npos ||
                   buffName.find("braumshield") != std::string::npos;
        }

        // Check if skillshot would be blocked by Braum's shield
        // Shield blocks projectiles from the direction Braum is facing
        //
        // @param skillshotFrom: Where the skillshot was cast from
        // @param braumPos: Braum's current position
        // @param braumFacing: Direction Braum is facing (use AiManager.GetMoveDirection())
        static bool WouldBlockSkillshot(Vector3 skillshotFrom, Vector3 braumPos, Vector3 braumFacing)
        {
            // Direction from Braum to skillshot source
            Vector3 toSource = skillshotFrom - braumPos;
            float len = toSource.Length();
            if (len < 1.0f) return false;
            toSource = toSource / len;

            // Normalize facing if needed
            float facingLen = braumFacing.Length();
            if (facingLen > 0.01f) {
                braumFacing = braumFacing / facingLen;
            } else {
                return false; // No facing direction
            }

            // Dot product: positive = skillshot coming from front (blocked)
            // Shield blocks in ~180 degree cone in front
            float dot = toSource.x * braumFacing.x + toSource.z * braumFacing.z;
            return dot > 0.0f; // Coming from front = blocked
        }

        // Calculate facing direction from Braum's target position
        // Use when AiManager direction is not available
        static Vector3 CalculateFacing(Vector3 braumPos, Vector3 braumTargetPos)
        {
            Vector3 facing = braumTargetPos - braumPos;
            float len = facing.Length();
            if (len < 1.0f) return Vector3(1, 0, 0);
            return facing / len;
        }
    };

    // ============================================================================
    // UNIT COLLISION MANAGER
    // Full collision checking for skillshots against units
    // ============================================================================
    class UnitCollision
    {
    public:
        // Get all minions that a skillshot would collide with
        // NOTE: Requires ObjectManager to be included
        // Returns vector of colliding unit positions (for simple use without including ObjectManager)
        static std::vector<Vector3> GetMinionCollisionPoints(
            Vector3 from, 
            Vector3 to, 
            float skillshotWidth,
            const std::vector<std::pair<Vector3, float>>& minions) // pos, radius pairs
        {
            std::vector<Vector3> collisions;
            
            for (const auto& minion : minions) {
                if (Collision::LineCollidesWithUnit(from, to, minion.first, minion.second, skillshotWidth)) {
                    collisions.push_back(minion.first);
                }
            }
            
            return collisions;
        }

        // Check if skillshot will collide with any unit before reaching target
        static bool WillCollideBeforeTarget(
            Vector3 from,
            Vector3 targetPos,
            float skillshotWidth,
            const std::vector<std::pair<Vector3, float>>& units)
        {
            float targetDistSq = (targetPos.x - from.x) * (targetPos.x - from.x) + 
                                 (targetPos.z - from.z) * (targetPos.z - from.z);

            for (const auto& unit : units) {
                if (Collision::LineCollidesWithUnit(from, targetPos, unit.first, unit.second, skillshotWidth)) {
                    // Check if this unit is closer than target
                    float unitDistSq = (unit.first.x - from.x) * (unit.first.x - from.x) + 
                                       (unit.first.z - from.z) * (unit.first.z - from.z);
                    if (unitDistSq < targetDistSq) {
                        return true;
                    }
                }
            }
            return false;
        }

        // Count how many units a skillshot will hit
        static int CountCollisions(
            Vector3 from,
            Vector3 to,
            float skillshotWidth,
            const std::vector<std::pair<Vector3, float>>& units)
        {
            int count = 0;
            for (const auto& unit : units) {
                if (Collision::LineCollidesWithUnit(from, to, unit.first, unit.second, skillshotWidth)) {
                    count++;
                }
            }
            return count;
        }

        // Get first collision point on skillshot path
        static Vector3 GetFirstCollisionPoint(
            Vector3 from,
            Vector3 to,
            float skillshotWidth,
            const std::vector<std::pair<Vector3, float>>& units)
        {
            float minDistSq = FLT_MAX;
            Vector3 firstCollision(0, 0, 0);

            for (const auto& unit : units) {
                if (Collision::LineCollidesWithUnit(from, to, unit.first, unit.second, skillshotWidth)) {
                    float distSq = (unit.first.x - from.x) * (unit.first.x - from.x) + 
                                   (unit.first.z - from.z) * (unit.first.z - from.z);
                    if (distSq < minDistSq) {
                        minDistSq = distSq;
                        firstCollision = unit.first;
                    }
                }
            }
            return firstCollision;
        }
    };
}

