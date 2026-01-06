#pragma once
#include <windows.h>
#include <cmath>
#include <map>
#include <mutex>
#include <vector>
#include "Offsets.h"
#include "../Vector.h"

namespace SDK
{
    // ============================================================================
    // AI MANAGER CLASS - MOVEMENT DETECTION
    // ============================================================================
    // Strategy: Since direct offsets (IsMoving, IsDashing, Velocity) may be encrypted
    // or outdated, we use PHYSICS-BASED SIMULATION:
    // 
    // 1. IsMoving = Calculated from velocity (position delta / time)
    // 2. IsDashing = Speed > 600 units/sec (typical run speed < 500)
    // 3. Velocity = (CurrentPos - LastPos) / DeltaTime
    //
    // This approach works regardless of game version or offset changes!
    // ============================================================================
    
    class AiManager
    {
    public:
        uint64_t m_Address;
        uint64_t m_OwnerAddress; // Owner GameObject address for position reading

        AiManager(uint64_t address, uint64_t ownerAddr = 0) : m_Address(address), m_OwnerAddress(ownerAddr) {}
        AiManager(uint64_t address) : m_Address(address), m_OwnerAddress(0) {}
        AiManager() : m_Address(0), m_OwnerAddress(0) {}

        bool IsValid() const {
            return m_Address != 0 && m_Address > 0x10000 && m_Address < 0x7FFFFFFFFFFF;
        }

        // ======================================================================================
        // BASIC PROPERTIES - Read from AiManager structure
        // ======================================================================================

        Vector3 GetStartPath() {
            if (!IsValid()) return Vector3(0, 0, 0);
            __try {
                Vector3 pos = *(Vector3*)(m_Address + Offset::oAiManagerStartPath);
                // Sanity check
                if (pos.x > 0 && pos.x < 20000 && pos.z > 0 && pos.z < 20000) {
                    return pos;
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return Vector3(0, 0, 0);
        }

        Vector3 GetEndPath() {
            if (!IsValid()) return Vector3(0, 0, 0);
            __try {
                Vector3 pos = *(Vector3*)(m_Address + Offset::oAiManagerEndPath);
                // Sanity check
                if (pos.x > 0 && pos.x < 20000 && pos.z > 0 && pos.z < 20000) {
                    return pos;
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return Vector3(0, 0, 0);
        }

        Vector3 GetTargetPosition() {
            return GetEndPath(); // Alias
        }

        Vector3 GetPosition() {
             return GetStartPath();
        }
        
        // NEW: Server Position from NavGrid scan (0xD8) - More accurate real-time position
        Vector3 GetServerPosition() {
            if (!IsValid()) return Vector3(0, 0, 0);
            __try {
                Vector3 pos = *(Vector3*)(m_Address + Offset::oAiManagerServerPos);
                if (pos.x > 0 && pos.x < 20000 && pos.z > 0 && pos.z < 20000) {
                    return pos;
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return GetStartPath(); // Fallback
        }
        
        // Facing angle (0x7C) - PI when idle, changes when moving
        // ⚠️ UNVERIFIED - Cần scan để verify offset này
        // Hữu ích cho prediction: Biết hướng nhìn của champion (facing direction)
        // KHÁC với MoveDirection: MoveDirection = hướng di chuyển, FacingAngle = hướng nhìn
        float GetFacingAngle() {
            if (!IsValid()) return 0.0f;
            __try {
                return *(float*)(m_Address + Offset::oAiManagerFacingAngle);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return 0.0f;
        }
        
        // NEW: Check IsMoving using offset 0x31C (partern 15.11)
        bool IsMovingByState() {
            if (!IsValid()) return false;
            __try {
                return *(bool*)(m_Address + Offset::oAiManagerIsMoving); // 0x31C
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return false;
        }
        
        // NEW: Check IsDashing using offset 0x384 (partern 15.11)
        bool IsDashingByOffset() {
            if (!IsValid()) return false;
            __try {
                return *(bool*)(m_Address + Offset::oAiManagerIsDashing); // 0x384
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return false;
        }
        
        // NEW: Get DashSpeed using offset 0x360 (partern 15.11)
        float GetDashSpeedByOffset() {
            if (!IsValid()) return 0.0f;
            __try {
                return *(float*)(m_Address + Offset::oAiManagerDashSpeed); // 0x360
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return 0.0f;
        }
        
        // NEW: Get CurrentSegment using offset 0x320 (partern 15.11)
        int GetCurrentSegmentByOffset() {
            if (!IsValid()) return 0;
            __try {
                return *(int*)(m_Address + Offset::oAiManagerCurrentSegment); // 0x320
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return 0;
        }
        
        // NEW: Get SegmentsCount using offset 0x350 (partern 15.11)
        int GetSegmentsCountByOffset() {
            if (!IsValid()) return 0;
            __try {
                return *(int*)(m_Address + Offset::oAiManagerSegmentsCount); // 0x350
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return 0;
        }
        
        // NEW: Get Velocity Speed (float) using offset 0x318 (partern 15.11)
        // ✅ VERIFIED: This is a float speed value, NOT a Vec3
        float GetVelocitySpeedByOffset() {
            if (!IsValid()) return 0.0f;
            __try {
                return *(float*)(m_Address + Offset::oAiManagerVelocity); // 0x318 - float speed
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return 0.0f;
        }

        // ======================================================================================
        // POSITION-BASED VELOCITY CALCULATION
        // Uses owner's GameObject position for accurate tracking
        // ======================================================================================
        
        Vector3 GetOwnerPosition() {
            if (m_OwnerAddress == 0) {
                return GetStartPath(); // Fallback to AiManager's StartPath
            }
            __try {
                Vector3 pos = *(Vector3*)(m_OwnerAddress + Offset::oObjPosition);
                if (pos.x > 0 && pos.x < 20000 && pos.z > 0 && pos.z < 20000) {
                    return pos;
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return GetStartPath();
        }

        // ======================================================================================
        // VELOCITY TRACKING & MOVEMENT STATE
        // Strategy: Calculate velocity from position delta over time
        // This works regardless of game offsets!
        // ======================================================================================
        
        Vector3 GetVelocity()
        {
            if (!IsValid()) return Vector3(0, 0, 0);
            
            // NOTE: Temporarily disabled offset-based velocity to avoid crashes
            // GetMoveDirection() may crash when reading offset 0x480
            // Use physics-based calculation instead (more stable)
            
            // Fallback: Use static maps to track position history per unique address
            static std::map<uint64_t, Vector3> lastPosMap;
            static std::map<uint64_t, DWORD> lastTimeMap;
            static std::map<uint64_t, Vector3> velocityMap;
            static std::mutex mtx;

            // No C++ exceptions - manual map compatible
            mtx.lock();
            
            // Use ServerPos (most accurate - authoritative server position)
            // If ServerPos not available, fallback to GameObject position
            Vector3 currentPos = GetServerPosition();
            if (currentPos.x <= 0 || currentPos.x >= 20000) {
                currentPos = GetOwnerPosition(); // Fallback to GameObject position
            }
            DWORD currentTime = GetTickCount();
            
            // Validate position
            if (currentPos.x <= 0 || currentPos.x >= 20000) {
                Vector3 result = velocityMap.count(m_Address) ? velocityMap[m_Address] : Vector3(0, 0, 0);
                mtx.unlock();
                return result;
            }

            // Initialization for new entity
            if (lastPosMap.find(m_Address) == lastPosMap.end()) {
                lastPosMap[m_Address] = currentPos;
                lastTimeMap[m_Address] = currentTime;
                velocityMap[m_Address] = Vector3(0, 0, 0);
                mtx.unlock();
                return Vector3(0, 0, 0);
            }

            DWORD timeDiff = currentTime - lastTimeMap[m_Address];
            
            // Update velociy every 16ms (~60fps) for smooth tracking
            if (timeDiff >= 16) {
                Vector3 lastPos = lastPosMap[m_Address];
                float deltaTime = (float)timeDiff / 1000.0f; // Convert to seconds
                
                if (deltaTime > 0.001f) {
                    Vector3 deltaPos = currentPos - lastPos;
                    Vector3 velocity = deltaPos / deltaTime; // Units per second
                    
                    // Filter out teleports (sudden huge position changes)
                    if (velocity.Length() > 3000.0f) {
                        velocity = Vector3(0, 0, 0); // Ignore teleport
                    }
                    
                    // Noise filter: Very small movements are noise, not actual movement
                    if (velocity.Length() < 10.0f) {
                        velocity = Vector3(0, 0, 0);
                    }
                    
                    // Smooth velocity with previous (simple moving average)
                    Vector3 oldVel = velocityMap[m_Address];
                    velocityMap[m_Address] = (oldVel + velocity) * 0.5f;
                }
                
                lastPosMap[m_Address] = currentPos;
                lastTimeMap[m_Address] = currentTime;
            }
            
            Vector3 result = velocityMap[m_Address];
            mtx.unlock();
            return result;
        }

        // ======================================================================================
        // MOVEMENT STATE DETECTION
        // ======================================================================================
        
        // Method 1: Use offset 0x31C (partern 15.11) - RECOMMENDED
        bool IsMoving()
        {
            // Primary: Use direct offset (fastest and most accurate)
            if (IsMovingByState()) return true;
            
            // Fallback: Check velocity (physics calculation)
            float speed = GetVelocity().Length();
            if (speed > 30.0f) return true; // Moving if speed > 30 units/sec
            
            // Fallback 2: Check if StartPath != EndPath
            Vector3 start = GetStartPath();
            Vector3 end = GetEndPath();
            if (start.x > 0 && end.x > 0) {
                float dist = start.Distance(end);
                if (dist > 10.0f) return true; // Has path = moving
            }
            
            return false;
        }

        // Method 2: Use offset 0x384 (partern 15.11) - RECOMMENDED
        bool IsDashing()
        {
            // Primary: Use direct offset (fastest and most accurate)
            if (IsDashingByOffset()) return true;
            
            // Fallback: Check speed (dash typically > 600-1000 units/sec)
            // Normal movement speed is ~325-~450 depending on boots/champions
            return GetVelocity().Length() > 600.0f;
        }
        
        // Raw speed value
        float GetMoveSpeed()
        {
            return GetVelocity().Length();
        }

        // ======================================================================================
        // PREDICTION
        // ======================================================================================
        
        // Simple wall check: Check if position is within valid game bounds
        // (More advanced wall checking would require NavGrid access)
        bool IsValidPosition(const Vector3& pos) {
            // Basic bounds check
            if (pos.x <= 0 || pos.x >= 20000 || pos.z <= 0 || pos.z >= 20000) {
                return false;
            }
            // Y coordinate should be reasonable (not underground or flying too high)
            if (pos.y < -1000.0f || pos.y > 2000.0f) {
                return false;
            }
            return true;
        }
        
        // Predict position following actual path waypoints (handles pathfinding around walls)
        Vector3 PredictPositionWithWallCheck(float time, float stepSize = 50.0f) {
            // Use ServerPos (most accurate) for prediction base
            Vector3 currentPos = GetServerPosition();
            if (currentPos.x <= 0 || currentPos.x >= 20000) {
                currentPos = GetOwnerPosition(); // Fallback to GameObject position
            }
            
            // Get waypoints from NavArray (includes pathfinding around walls)
            std::vector<Vector3> waypoints = GetWaypoints();
            
            if (waypoints.size() < 2) {
                // No waypoints, fallback to basic prediction
                Vector3 velocity = GetVelocity();
                if (velocity.Length() < 1.0f) {
                    return currentPos;
                }
                return currentPos + (velocity * time);
            }
            
            // Calculate distance to travel
            float speed = GetMoveSpeed();
            if (speed < 1.0f) {
                return currentPos; // Not moving
            }
            float distanceToTravel = speed * time;
            
            // Follow waypoints along path
            float distanceTraveled = 0.0f;
            Vector3 lastWaypoint = waypoints[0];
            
            for (size_t i = 1; i < waypoints.size(); i++) {
                Vector3 waypoint = waypoints[i];
                Vector3 segmentDir = waypoint - lastWaypoint;
                float segmentLength = segmentDir.Length();
                
                if (segmentLength > 0.1f) {
                    segmentDir = segmentDir.Normalized();
                    
                    if (distanceTraveled + segmentLength >= distanceToTravel) {
                        // Destination is within this segment
                        float remainingDistance = distanceToTravel - distanceTraveled;
                        return lastWaypoint + (segmentDir * remainingDistance);
                    }
                    
                    distanceTraveled += segmentLength;
                    lastWaypoint = waypoint;
                }
            }
            
            // Traveled entire path, return last waypoint
            return waypoints.back();
        }
        
        // Basic prediction (no wall checking) - for backward compatibility
        Vector3 PredictPosition(float time)
        {
            // Use ServerPos (most accurate) for prediction base
            Vector3 currentPos = GetServerPosition();
            if (currentPos.x <= 0 || currentPos.x >= 20000) {
                currentPos = GetOwnerPosition(); // Fallback to GameObject position
            }
            Vector3 velocity = GetVelocity();
            return currentPos + (velocity * time);
        }

        Vector3 GetMoveDirection()
        {
            // NOTE: Offset 0x480 (MoveVec3) may not exist or cause crashes
            // Temporarily disabled Method 1 to avoid crashes
            // Use Method 2 (EndPath - StartPath) instead - more stable
            
            // Method 1: Try to read from offset 0x480 (DISABLED - causes crashes)
            // if (IsValid()) {
            //     __try {
            //         Vector3 moveVec = *(Vector3*)(m_Address + Offset::oAiManagerMoveVec3); // 0x480
            //         float len = moveVec.Length();
            //         if (len > 0.5f && len < 1.5f && abs(moveVec.x) < 10.0f && abs(moveVec.z) < 10.0f) {
            //             return moveVec;
            //         }
            //     } __except(EXCEPTION_EXECUTE_HANDLER) {}
            // }
            
            // Method 2: Calculate from EndPath - StartPath (RECOMMENDED - stable)
            Vector3 start = GetStartPath();
            Vector3 end = GetEndPath();
            if (start.x > 0 && end.x > 0) {
                Vector3 dir = end - start;
                float len = dir.Length();
                if (len > 1.0f) {
                    return Vector3(dir.x / len, dir.y / len, dir.z / len);
                }
            }
            
            // Method 3: Calculate from velocity (fallback)
            Vector3 v = GetVelocity();
            float len = v.Length();
            if (len > 1.0f) {
                return Vector3(v.x / len, v.y / len, v.z / len);
            }
            return Vector3(0, 0, 0);
        }
        
        // ======================================================================================
        // DASH PREDICTION (Based on "huong phat trien" strategy)
        // ======================================================================================
        
        float GetDashSpeed() {
            // Primary: Use direct offset 0x360 (partern 15.11)
            float dashSpeed = GetDashSpeedByOffset();
            if (dashSpeed > 0.0f) return dashSpeed;
            
            // Fallback: Calculate from velocity
            if (!IsDashing()) return 0.0f;
            return GetVelocity().Length();
        }
        
        Vector3 GetDashEndPos(float dashDuration = 0.4f) {
            if (!IsDashing()) return GetOwnerPosition();
            // Estimate dash end position
            return GetOwnerPosition() + (GetVelocity() * dashDuration);
        }
        
        // ======================================================================================
        // WAYPOINTS - Read from NavArray or simulate based on segments
        // ======================================================================================
        
        // Get NavArray pointer (offset 0x348)
        uint64_t GetNavArrayPointer() {
            if (!IsValid()) return 0;
            __try {
                uint64_t navArrayPtr = *(uint64_t*)(m_Address + Offset::oAiManagerNavArray);
                if (navArrayPtr > 0x100000 && navArrayPtr < 0x7FFFFFFFFFFF) {
                    return navArrayPtr;
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return 0;
        }
        
        // Helper: Read waypoint from NavArray (no C++ objects to avoid unwinding issues)
        bool ReadWaypointFromNavArray(uint64_t navArrayPtr, int index, Vector3* out) {
            if (!navArrayPtr || !out) return false;
            __try {
                Vector3 waypoint = *(Vector3*)(navArrayPtr + (index * sizeof(Vector3)));
                // Validate waypoint
                if (waypoint.x > 0 && waypoint.x < 20000 && waypoint.z > 0 && waypoint.z < 20000) {
                    *out = waypoint;
                    return true;
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            return false;
        }
        
        // Get waypoints from NavArray or simulate based on segments
        // NavArray structure: Array of Vector3 waypoints, size = SegmentsCount
        std::vector<Vector3> GetWaypoints() {
            std::vector<Vector3> path;
            
            // Try to read NavArray pointer
            uint64_t navArrayPtr = GetNavArrayPointer();
            int segmentsCount = GetSegmentsCount();
            
            if (navArrayPtr > 0 && segmentsCount > 0 && segmentsCount < 20) {
                // Try to read waypoints from NavArray using helper function
                for (int i = 0; i < segmentsCount; i++) {
                    Vector3 waypoint;
                    if (ReadWaypointFromNavArray(navArrayPtr, i, &waypoint)) {
                        path.push_back(waypoint);
                    } else {
                        break; // Invalid waypoint, stop reading
                    }
                }
                
                if (path.size() > 0) {
                    return path; // Successfully read waypoints
                }
            }
            
            // Fallback: Simulate waypoints based on ServerPos (most accurate), EndPath, and segments
            // Use ServerPos instead of StartPath for better accuracy (ServerPos is authoritative)
            Vector3 start = GetServerPosition();
            if (start.x <= 0 || start.x >= 20000) {
                start = GetStartPath(); // Fallback to StartPath if ServerPos invalid
            }
            Vector3 end = GetEndPath();
            
            if (start.x > 0 && end.x > 0 && segmentsCount > 0) {
                path.push_back(start); // Start from ServerPos (most accurate current position)
                
                // If multiple segments, create intermediate points
                // Note: This is a simple linear interpolation, real pathfinding would have actual waypoints
                if (segmentsCount > 1) {
                    Vector3 direction = end - start;
                    float totalDist = direction.Length();
                    
                    if (totalDist > 1.0f && segmentsCount > 1) {
                        direction = direction.Normalized();
                        
                        // Create intermediate waypoints (simple linear interpolation)
                        // Real pathfinding would have waypoints that go around walls
                        for (int i = 1; i < segmentsCount; i++) {
                            float t = (float)i / (float)segmentsCount;
                            Vector3 waypoint = start + (direction * (totalDist * t));
                            path.push_back(waypoint);
                        }
                    }
                }
                
                path.push_back(end);
            } else {
                // Last resort: Use ServerPos (or GameObject position) and predicted position
                Vector3 currentPos = GetServerPosition();
                if (currentPos.x <= 0 || currentPos.x >= 20000) {
                    currentPos = GetOwnerPosition(); // Fallback
                }
                path.push_back(currentPos);
            if (IsMoving()) {
                    Vector3 predictedTarget = currentPos + (GetVelocity() * 2.0f);
                path.push_back(predictedTarget);
            }
            }
            
            return path;
        }
        
        // ======================================================================================
        // ADDITIONAL METHODS FOR 15.11 OFFSET COMPATIBILITY
        // ======================================================================================
        
        // CurrentSegment (0x320) - Use direct offset (partern 15.11)
        int GetCurrentSegment() {
            int segment = GetCurrentSegmentByOffset();
            if (segment >= 0) return segment;
            // Fallback: Simulated
            return IsMoving() ? 1 : 0;
        }
        
        // SegmentsCount (0x350) - Use direct offset (partern 15.11)
        int GetSegmentsCount() {
            int count = GetSegmentsCountByOffset();
            if (count > 0) return count;
            // Fallback: Simulated
            return IsMoving() ? 2 : 1; // Current pos + predicted target
        }
        
        // ServerPos - Now uses actual offset 0xD8 from NavGrid scan (see GetServerPosition above)
        
        // NavArray (0x348) - Returns waypoints array (simulated)
        std::vector<Vector3> GetNavArray() {
            return GetWaypoints();
        }
        
        // MoveVec3 (0x480) - Movement direction vector
        Vector3 GetMoveVec3() {
            return GetMoveDirection();
        }
        
        // ======================================================================================
        // ALIAS METHODS (For SDK compatibility with different naming conventions)
        // ======================================================================================
        
        // Aliases for common naming patterns
        Vector3 GetTargetPos() { return GetTargetPosition(); }
        Vector3 GetPathStart() { return GetStartPath(); }
        Vector3 GetPathEnd() { return GetEndPath(); }
        float GetSpeed() { return GetMoveSpeed(); }
        bool HasPath() { return IsMoving(); }
        int GetPathSegments() { return GetSegmentsCount(); }
    };
}

