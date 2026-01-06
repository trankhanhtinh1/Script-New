#pragma once
// ============================================================================
// PREDICTION.H - Skillshot Prediction System
// Full port from LeagueSharp.Common Prediction.cs to C++
// Uses SDK's AiManager, BuffManager, and Collision for calculations
// ============================================================================

#include <vector>
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <map>
#include <mutex>
#include "../Vector.h"
#include "../SDK/Offsets.h"
#include "../SDK/ObjectManager.h"
#include "../SDK/BuffManager.h"
#include "../SDK/Collision.h"
#include "../SDK/AiManager.h"
#include "../SDK/Game.h"

namespace SDK
{
    // ============================================================================
    // HIT CHANCE ENUM
    // Represents the probability of hitting an enemy
    // ============================================================================
    enum class HitChance
    {
        Collision = 0,      // Target is blocked by other units
        OutOfRange = 1,     // Target is out of range
        Impossible = 2,     // Impossible to hit the target
        Low = 3,            // Low probability
        Medium = 4,         // Medium probability
        High = 5,           // High probability
        VeryHigh = 6,       // Very high probability
        Dashing = 7,        // Unit is dashing
        Immobile = 8        // Target is immobile (CC'd)
    };

    // ============================================================================
    // SKILLSHOT TYPE ENUM
    // ============================================================================
    enum class SkillshotType
    {
        Line,       // Linear skillshot (Lux Q, Morgana Q)
        Circle,     // Circular skillshot (Lux E, Ziggs Q)
        Cone        // Conical skillshot (Annie W, Cho'Gath W)
    };

    // ============================================================================
    // STORED PATH - Represents a recorded path for a unit
    // ============================================================================
    struct StoredPath
    {
        std::vector<Vector3> Path;
        uint32_t Tick = 0;
        
        Vector3 StartPoint() const {
            return Path.empty() ? Vector3(0, 0, 0) : Path.front();
        }
        
        Vector3 EndPoint() const {
            return Path.empty() ? Vector3(0, 0, 0) : Path.back();
        }
        
        // Get time since this path was recorded (in seconds)
        double Time() const {
            return (GetTickCount() - Tick) / 1000.0;
        }
        
        int WaypointCount() const {
            return (int)Path.size();
        }
        
        // Calculate total path length
        float PathLength() const {
            float length = 0.0f;
            for (size_t i = 0; i < Path.size() - 1; i++) {
                length += (Path[i + 1] - Path[i]).Length();
            }
            return length;
        }
    };

    // ============================================================================
    // PATH TRACKER - Tracks unit movement history for better prediction
    // ============================================================================
    class PathTracker
    {
    private:
        static inline std::map<uint32_t, std::vector<StoredPath>> s_StoredPaths;
        static inline std::mutex s_Mutex;
        static constexpr double MAX_TIME = 1.5; // Maximum time to store paths
        static constexpr int MAX_PATHS = 50;    // Maximum paths per unit
        
    public:
        // Record a new path for a unit (call when unit changes path)
        static void RecordPath(uint32_t networkId, const std::vector<Vector3>& path)
        {
            std::lock_guard<std::mutex> lock(s_Mutex);
            
            if (s_StoredPaths.find(networkId) == s_StoredPaths.end()) {
                s_StoredPaths[networkId] = std::vector<StoredPath>();
            }
            
            StoredPath newPath;
            newPath.Path = path;
            newPath.Tick = GetTickCount();
            
            s_StoredPaths[networkId].push_back(newPath);
            
            // Clean up old paths
            if (s_StoredPaths[networkId].size() > MAX_PATHS) {
                s_StoredPaths[networkId].erase(
                    s_StoredPaths[networkId].begin(),
                    s_StoredPaths[networkId].begin() + 40
                );
            }
        }
        
        // Get current path for a unit
        static StoredPath GetCurrentPath(uint32_t networkId)
        {
            std::lock_guard<std::mutex> lock(s_Mutex);
            
            if (s_StoredPaths.find(networkId) == s_StoredPaths.end() ||
                s_StoredPaths[networkId].empty()) {
                return StoredPath();
            }
            
            return s_StoredPaths[networkId].back();
        }
        
        // Get all stored paths within time limit
        static std::vector<StoredPath> GetStoredPaths(uint32_t networkId, double maxTime)
        {
            std::lock_guard<std::mutex> lock(s_Mutex);
            
            std::vector<StoredPath> result;
            
            if (s_StoredPaths.find(networkId) == s_StoredPaths.end()) {
                return result;
            }
            
            for (const auto& path : s_StoredPaths[networkId]) {
                if (path.Time() < maxTime) {
                    result.push_back(path);
                }
            }
            
            return result;
        }
        
        // Get mean speed over time (useful for detecting AFK/stopped units)
        static float GetMeanSpeed(uint32_t networkId, float moveSpeed, double maxTime)
        {
            auto paths = GetStoredPaths(networkId, maxTime);
            
            if (paths.empty()) {
                return moveSpeed;
            }
            
            double distance = 0.0;
            
            // Assume unit was moving for first path
            distance += (maxTime - paths[0].Time()) * moveSpeed;
            
            for (size_t i = 0; i < paths.size() - 1; i++) {
                if (paths[i].WaypointCount() > 0) {
                    distance += std::min(
                        (float)((paths[i].Time() - paths[i + 1].Time()) * moveSpeed),
                        paths[i].PathLength()
                    );
                }
            }
            
            // Last path
            if (!paths.empty() && paths.back().WaypointCount() > 0) {
                distance += std::min(
                    (float)(paths.back().Time() * moveSpeed),
                    paths.back().PathLength()
                );
            }
            
            return (float)(distance / maxTime);
        }
        
        // Clear all stored paths
        static void Clear()
        {
            std::lock_guard<std::mutex> lock(s_Mutex);
            s_StoredPaths.clear();
        }
    };

    // ============================================================================
    // PREDICTION INPUT
    // Contains all information needed to calculate prediction
    // ============================================================================
    struct PredictionInput
    {
        // Required fields
        uint64_t UnitAddress = 0;          // Target unit address
        Vector3 From = Vector3(0, 0, 0);   // Cast from position
        
        // Spell parameters
        float Delay = 0.0f;                // Cast delay in seconds
        float Speed = FLT_MAX;             // Missile speed (FLT_MAX = instant)
        float Radius = 1.0f;               // Skillshot width/radius
        float Range = FLT_MAX;             // Skillshot range
        SkillshotType Type = SkillshotType::Line;
        
        // Options
        bool Collision = false;            // Check collision with units
        bool UseBoundingRadius = true;     // Add unit bounding radius
        bool Aoe = false;                  // Area of effect mode
        CollisionableObjects CollisionFlags = CollisionableObjects::Default;
        
        // Range check from position (if different from From)
        Vector3 RangeCheckFrom = Vector3(0, 0, 0);
        
        // Get effective range check position
        Vector3 GetRangeCheckFrom() const {
            if (RangeCheckFrom.Length() > 0.1f) return RangeCheckFrom;
            if (From.Length() > 0.1f) return From;
            return Vector3(0, 0, 0);
        }
        
        // Get real radius including bounding radius
        float GetRealRadius() const {
            if (!UseBoundingRadius || UnitAddress == 0) return Radius;
            float boundingRadius = *(float*)(UnitAddress + Offset::oObjRadius);
            return Radius + boundingRadius;
        }
    };

    // ============================================================================
    // PREDICTION OUTPUT
    // Contains the result of prediction calculation
    // ============================================================================
    struct PredictionOutput
    {
        HitChance Hitchance = HitChance::Impossible;
        Vector3 CastPosition = Vector3(0, 0, 0);    // Where to cast
        Vector3 UnitPosition = Vector3(0, 0, 0);    // Where unit will be
        int AoeTargetsHitCount = 0;                  // Number of targets hit (AOE)
        std::vector<uint64_t> CollisionObjects;      // Units blocking the path
        PredictionInput Input;                       // Original input (for reference)
        
        // Check if prediction is valid for casting
        bool IsValid() const {
            return Hitchance >= HitChance::Medium && CastPosition.Length() > 0.1f;
        }
        
        // Check if we should cast (High+ hitchance)
        bool ShouldCast() const {
            return Hitchance >= HitChance::High;
        }
    };

    // ============================================================================
    // PREDICTION CLASS
    // Main prediction logic - Full port from LeagueSharp.Common
    // ============================================================================
    class Prediction
    {
    public:
        // ========================================================================
        // SIMPLE PREDICTION METHODS (Easy to use)
        // ========================================================================
        
        // Get prediction with just delay
        static PredictionOutput GetPrediction(uint64_t targetAddress, float delay)
        {
            PredictionInput input;
            input.UnitAddress = targetAddress;
            input.Delay = delay;
            
            // Get local player position as From
            uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
            uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
            if (localPlayer) {
                input.From = *(Vector3*)(localPlayer + Offset::oObjPosition);
            }
            
            return GetPrediction(input, true, true);
        }
        
        // Get prediction with delay and radius
        static PredictionOutput GetPrediction(uint64_t targetAddress, float delay, float radius)
        {
            PredictionInput input;
            input.UnitAddress = targetAddress;
            input.Delay = delay;
            input.Radius = radius;
            
            uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
            uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
            if (localPlayer) {
                input.From = *(Vector3*)(localPlayer + Offset::oObjPosition);
            }
            
            return GetPrediction(input, true, true);
        }
        
        // Get prediction with delay, radius, and speed
        static PredictionOutput GetPrediction(uint64_t targetAddress, float delay, float radius, float speed)
        {
            PredictionInput input;
            input.UnitAddress = targetAddress;
            input.Delay = delay;
            input.Radius = radius;
            input.Speed = speed;
            
            uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
            uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
            if (localPlayer) {
                input.From = *(Vector3*)(localPlayer + Offset::oObjPosition);
            }
            
            return GetPrediction(input, true, true);
        }
        
        // ========================================================================
        // FULL PREDICTION METHOD (Main entry point)
        // ft = add latency/tick compensation
        // checkCollision = check for collision objects
        // ========================================================================
        static PredictionOutput GetPrediction(PredictionInput input, bool ft = true, bool checkCollision = true)
        {
            PredictionOutput result;
            result.Input = input;
            result.Hitchance = HitChance::Impossible;
            
            // Validate input
            if (input.UnitAddress == 0) return result;
            
            // Check if target is valid
            bool isDead = *(uint8_t*)(input.UnitAddress + Offset::oDead) != 0;
            if (isDead) return result;
            
            // Add latency compensation
            if (ft) {
                input.Delay += 0.035f + 0.035f; // Avg ping/2 + server tick
            }
            
            // Get target position
            Vector3 targetPos = *(Vector3*)(input.UnitAddress + Offset::oObjPosition);
            
            // Check if target is too far away
            if (input.Range < FLT_MAX) {
                Vector3 checkFrom = input.GetRangeCheckFrom();
                float distance = (targetPos - checkFrom).Length();
                if (distance > input.Range * 1.5f) {
                    result.Hitchance = HitChance::OutOfRange;
                    return result;
                }
            }
            
            // Get AI Manager for movement data
            AiManager aiMgr(input.UnitAddress);
            
            // Check if unit is dashing
            if (aiMgr.IsDashing()) {
                result = GetDashingPrediction(input, aiMgr);
            }
            else {
                // Check if unit is immobile (CC'd)
                float immobileTime = GetImmobileTime(input.UnitAddress);
                if (immobileTime > 0.0f) {
                    result = GetImmobilePrediction(input, immobileTime);
                }
            }
            
            // Normal prediction if not dashing/immobile
            if (result.Hitchance == HitChance::Impossible) {
                result = GetStandardPrediction(input, aiMgr);
            }
            
            // Check range after prediction
            if (input.Range < FLT_MAX && result.Hitchance != HitChance::Impossible) {
                Vector3 checkFrom = input.GetRangeCheckFrom();
                float distToUnit = (result.UnitPosition - checkFrom).Length();
                float distToCast = (result.CastPosition - checkFrom).Length();
                
                // Reduce hitchance if at edge of range
                if (result.Hitchance >= HitChance::High &&
                    distToUnit > input.Range + input.GetRealRadius() * 0.75f) {
                    result.Hitchance = HitChance::Medium;
                }
                
                // Out of range check
                if (distToUnit > input.Range + 
                    (input.Type == SkillshotType::Circle ? input.GetRealRadius() : 0)) {
                    result.Hitchance = HitChance::OutOfRange;
                }
                
                // Clamp cast position to range
                if (distToCast > input.Range && result.Hitchance != HitChance::OutOfRange) {
                    Vector3 direction = (result.UnitPosition - checkFrom);
                    float len = direction.Length();
                    if (len > 0.1f) {
                        direction = direction / len;
                        result.CastPosition = checkFrom + direction * input.Range;
                    }
                }
            }
            
            // Check collision
            if (checkCollision && input.Collision && result.Hitchance >= HitChance::Medium) {
                auto collisions = GetCollision(input.From, result.CastPosition, input);
                if (!collisions.empty()) {
                    result.CollisionObjects = collisions;
                    result.Hitchance = HitChance::Collision;
                }
            }
            
            return result;
        }
        
    private:
        // ========================================================================
        // IMMOBILE PREDICTION (Target is CC'd)
        // ========================================================================
        static PredictionOutput GetImmobilePrediction(PredictionInput& input, float remainingImmobileTime)
        {
            PredictionOutput result;
            result.Input = input;
            
            Vector3 targetPos = *(Vector3*)(input.UnitAddress + Offset::oObjPosition);
            float targetSpeed = *(float*)(input.UnitAddress + Offset::SpeedPlayer);
            
            // Calculate time for skillshot to reach target
            float distance = (targetPos - input.From).Length();
            float travelTime = (input.Speed < FLT_MAX) ? distance / input.Speed : 0.0f;
            float totalTime = input.Delay + travelTime;
            
            // If skillshot arrives before CC ends, it's guaranteed hit
            if (totalTime <= remainingImmobileTime + input.GetRealRadius() / targetSpeed) {
                result.Hitchance = HitChance::Immobile;
            } else {
                result.Hitchance = HitChance::High;
            }
            
            result.CastPosition = targetPos;
            result.UnitPosition = targetPos;
            
            return result;
        }
        
        // ========================================================================
        // DASHING PREDICTION (Target is dashing/blinking)
        // ========================================================================
        static PredictionOutput GetDashingPrediction(PredictionInput& input, AiManager& aiMgr)
        {
            PredictionOutput result;
            result.Input = input;
            
            Vector3 targetPos = *(Vector3*)(input.UnitAddress + Offset::oObjPosition);
            Vector3 dashEnd = aiMgr.GetEndPath();
            float dashSpeed = aiMgr.GetDashSpeed();
            
            if (dashSpeed < 100.0f) dashSpeed = 1000.0f; // Fallback
            
            // Calculate where target will be during/after dash
            float dashDistance = (dashEnd - targetPos).Length();
            float dashTime = dashDistance / dashSpeed;
            
            // Get path from current pos to dash end
            std::vector<Vector3> path = { targetPos, dashEnd };
            
            // Get prediction along dash path
            auto dashPred = GetPositionOnPath(input, path, dashSpeed);
            
            // If we can hit mid-dash
            if (dashPred.Hitchance >= HitChance::High) {
                float distFromEnd = (dashPred.UnitPosition - dashEnd).Length();
                if (distFromEnd < 200.0f) {
                    dashPred.CastPosition = dashPred.UnitPosition;
                    dashPred.Hitchance = HitChance::Dashing;
                    return dashPred;
                }
            }
            
            // Aim at dash end if path is long enough
            if (dashDistance > 200.0f) {
                float timeToPoint = input.Delay / 2.0f + 
                    (dashEnd - input.From).Length() / input.Speed - 0.25f;
                float targetArrivalTime = dashDistance / dashSpeed + 
                    input.GetRealRadius() / (*(float*)(input.UnitAddress + Offset::SpeedPlayer));
                    
                if (timeToPoint <= targetArrivalTime) {
                    result.CastPosition = dashEnd;
                    result.UnitPosition = dashEnd;
                    result.Hitchance = HitChance::Dashing;
                    return result;
                }
            }
            
            result.CastPosition = dashEnd;
            result.UnitPosition = dashEnd;
            result.Hitchance = HitChance::Medium;
            
            return result;
        }
        
        // ========================================================================
        // STANDARD PREDICTION (Normal walking target)
        // ========================================================================
        static PredictionOutput GetStandardPrediction(PredictionInput& input, AiManager& aiMgr)
        {
            float targetSpeed = *(float*)(input.UnitAddress + Offset::SpeedPlayer);
            Vector3 targetPos = *(Vector3*)(input.UnitAddress + Offset::oObjPosition);
            
            // Slow down speed if target is close (reaction time)
            float distance = (targetPos - input.From).Length();
            if (distance < 200.0f) {
                targetSpeed /= 1.5f;
            }
            
            // Get waypoints
            std::vector<Vector3> waypoints;
            waypoints.push_back(targetPos);
            
            if (aiMgr.IsMoving()) {
                waypoints.push_back(aiMgr.GetEndPath());
            }
            
            return GetPositionOnPath(input, waypoints, targetSpeed);
        }
        
        // ========================================================================
        // GET POSITION ON PATH - Core prediction algorithm
        // Calculates where target will be when skillshot arrives
        // ========================================================================
        static PredictionOutput GetPositionOnPath(PredictionInput& input, 
            const std::vector<Vector3>& path, float speed = -1.0f)
        {
            PredictionOutput result;
            result.Input = input;
            
            float targetSpeed = (speed < 0) ? 
                *(float*)(input.UnitAddress + Offset::SpeedPlayer) : speed;
            
            // Not moving - easy prediction
            if (path.size() <= 1) {
                Vector3 pos = path.empty() ? 
                    *(Vector3*)(input.UnitAddress + Offset::oObjPosition) : path[0];
                result.CastPosition = pos;
                result.UnitPosition = pos;
                result.Hitchance = HitChance::VeryHigh;
                return result;
            }
            
            // Calculate path length
            float pathLength = 0.0f;
            for (size_t i = 0; i < path.size() - 1; i++) {
                pathLength += (path[i + 1] - path[i]).Length();
            }
            
            // Skillshots with only delay (instant travel)
            if (input.Speed >= FLT_MAX || pathLength >= input.Delay * targetSpeed - input.GetRealRadius())
            {
                float tDistance = input.Delay * targetSpeed - input.GetRealRadius();
                
                // Walk along path to find position
                for (size_t i = 0; i < path.size() - 1; i++) {
                    float segmentLength = (path[i + 1] - path[i]).Length();
                    
                    if (segmentLength >= tDistance) {
                        Vector3 direction = (path[i + 1] - path[i]);
                        direction = direction / segmentLength;
                        
                        Vector3 cp = path[i] + direction * tDistance;
                        Vector3 p = path[i] + direction * 
                            ((i == path.size() - 2) ? 
                             std::min(tDistance + input.GetRealRadius(), segmentLength) :
                             (tDistance + input.GetRealRadius()));
                        
                        result.CastPosition = cp;
                        result.UnitPosition = p;
                        result.Hitchance = GetHitchanceFromPath(input.UnitAddress);
                        return result;
                    }
                    
                    tDistance -= segmentLength;
                }
            }
            
            // Skillshots with delay AND speed - iterative collision detection
            if (pathLength >= input.Delay * targetSpeed - input.GetRealRadius() && 
                input.Speed < FLT_MAX)
            {
                float d = input.Delay * targetSpeed - input.GetRealRadius();
                
                // For line/cone, if target is close, use full delay distance
                if ((input.Type == SkillshotType::Line || input.Type == SkillshotType::Cone) &&
                    (input.From - path[0]).Length() < 200.0f * 200.0f) {
                    d = input.Delay * targetSpeed;
                }
                
                // Cut path to start from delay position
                std::vector<Vector3> cutPath = CutPath(path, d);
                
                float tT = 0.0f;
                for (size_t i = 0; i < cutPath.size() - 1; i++) {
                    Vector3 a = cutPath[i];
                    Vector3 b = cutPath[i + 1];
                    float segmentTime = (b - a).Length() / targetSpeed;
                    
                    Vector3 direction = (b - a);
                    float len = direction.Length();
                    if (len > 0.1f) direction = direction / len;
                    
                    // Adjusted start position
                    a = a - direction * (targetSpeed * tT);
                    
                    // Solve for intersection time
                    auto solution = VectorMovementCollision(a, b, targetSpeed, input.From, input.Speed, tT);
                    float t = solution.first;
                    Vector3 pos = solution.second;
                    
                    if (pos.Length() > 0.1f && t >= tT && t <= tT + segmentTime) {
                        if ((pos - b).Length() < 20.0f) break;
                        
                        Vector3 p = pos + direction * input.GetRealRadius();
                        
                        result.CastPosition = pos;
                        result.UnitPosition = p;
                        result.Hitchance = GetHitchanceFromPath(input.UnitAddress);
                        return result;
                    }
                    
                    tT += segmentTime;
                }
            }
            
            // Fallback - aim at path end
            Vector3 endPos = path.back();
            result.CastPosition = endPos;
            result.UnitPosition = endPos;
            result.Hitchance = HitChance::Medium;
            
            return result;
        }
        
        // ========================================================================
        // HELPER: Get hitchance based on path timing (new path = VeryHigh)
        // ========================================================================
        static HitChance GetHitchanceFromPath(uint64_t unitAddress)
        {
            uint32_t netId = *(uint32_t*)(unitAddress + Offset::oObjNetId);
            StoredPath currentPath = PathTracker::GetCurrentPath(netId);
            
            // If path was just created (< 0.1s ago), VeryHigh chance
            if (currentPath.Time() < 0.1) {
                return HitChance::VeryHigh;
            }
            
            return HitChance::High;
        }
        
        // ========================================================================
        // HELPER: Cut path by distance (remove walked portion)
        // ========================================================================
        static std::vector<Vector3> CutPath(const std::vector<Vector3>& path, float distance)
        {
            std::vector<Vector3> result;
            float remaining = distance;
            
            for (size_t i = 0; i < path.size() - 1; i++) {
                float segLen = (path[i + 1] - path[i]).Length();
                
                if (remaining > segLen) {
                    remaining -= segLen;
                } else {
                    Vector3 dir = (path[i + 1] - path[i]);
                    if (segLen > 0.1f) dir = dir / segLen;
                    Vector3 startPoint = path[i] + dir * remaining;
                    result.push_back(startPoint);
                    
                    for (size_t j = i + 1; j < path.size(); j++) {
                        result.push_back(path[j]);
                    }
                    break;
                }
            }
            
            if (result.empty() && !path.empty()) {
                result.push_back(path.back());
            }
            
            return result;
        }
        
        // ========================================================================
        // HELPER: Vector movement collision solver
        // Finds where moving unit and projectile will meet
        // ========================================================================
        static std::pair<float, Vector3> VectorMovementCollision(
            Vector3 startPos1, Vector3 endPos1, float speed1,
            Vector3 startPos2, float speed2, float delay = 0.0f)
        {
            float sP1x = startPos1.x, sP1y = startPos1.z;
            float eP1x = endPos1.x, eP1y = endPos1.z;
            float sP2x = startPos2.x, sP2y = startPos2.z;
            
            float d = eP1x - sP1x, e = eP1y - sP1y;
            float dist = sqrtf(d * d + e * e);
            float t1 = dist / speed1;
            
            float S = (std::abs(dist) > 0.0001f) ? speed1 : 0.0f;
            float K = (std::abs(dist) > 0.0001f) ? (d / dist) : 0.0f;
            float L = (std::abs(dist) > 0.0001f) ? (e / dist) : 0.0f;
            
            float a = (S * S) - (speed2 * speed2);
            float b = -2.0f * (sP1x * S * K - sP2x * S * K + sP1y * S * L - sP2y * S * L);
            float c = (sP1x - sP2x) * (sP1x - sP2x) + (sP1y - sP2y) * (sP1y - sP2y);
            
            // Solve quadratic
            if (std::abs(a) < 0.0001f) {
                // Linear case
                if (std::abs(b) > 0.0001f) {
                    float t = -c / b;
                    if (t >= delay && t <= t1) {
                        return { t, Vector3(sP1x + t * S * K, 0, sP1y + t * S * L) };
                    }
                }
            } else {
                float discriminant = b * b - 4.0f * a * c;
                if (discriminant >= 0) {
                    float sqrtD = sqrtf(discriminant);
                    float t_1 = (-b - sqrtD) / (2.0f * a);
                    float t_2 = (-b + sqrtD) / (2.0f * a);
                    
                    float t = (t_1 >= delay && t_1 <= t1) ? t_1 : 
                              ((t_2 >= delay && t_2 <= t1) ? t_2 : -1.0f);
                    
                    if (t >= 0) {
                        return { t, Vector3(sP1x + t * S * K, 0, sP1y + t * S * L) };
                    }
                }
            }
            
            return { -1.0f, Vector3(0, 0, 0) };
        }
        
        // ========================================================================
        // GET IMMOBILE TIME (Check CC buffs)
        // ========================================================================
        static float GetImmobileTime(uint64_t unitAddress)
        {
            BuffManager buffs(unitAddress);
            return buffs.GetImmobileTime(Game::GetTime());
        }
        
        // ========================================================================
        // GET COLLISION - Returns list of units that will block skillshot
        // ========================================================================
        static std::vector<uint64_t> GetCollision(Vector3 from, Vector3 to, PredictionInput& input)
        {
            std::vector<uint64_t> result;
            
            // Check wall collision
            if (HasFlag(input.CollisionFlags, CollisionableObjects::Walls)) {
                if (Collision::LineCollidesWithWall(from, to)) {
                    // Add fake collision object (using local player as placeholder)
                    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
                    uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
                    if (localPlayer) result.push_back(localPlayer);
                }
            }
            
            // Check Yasuo wall
            if (HasFlag(input.CollisionFlags, CollisionableObjects::YasuoWall)) {
                if (YasuoWallTracker::CollidesWithWall(from, to)) {
                    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
                    uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
                    if (localPlayer) result.push_back(localPlayer);
                }
            }
            
            // Check minion collision
            if (HasFlag(input.CollisionFlags, CollisionableObjects::Minions)) {
                // Get minion list
                uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
                uint64_t minionListPtr = *(uint64_t*)(moduleBase + Offset::oMinionList);
                
                if (minionListPtr) {
                    uint64_t minionArray = *(uint64_t*)(minionListPtr + Offset::LaneMinionArray);
                    int minionCount = *(int*)(minionListPtr + Offset::LaneMinionCount);
                    
                    if (minionArray && minionCount > 0 && minionCount < 200) {
                        for (int i = 0; i < minionCount; i++) {
                            uint64_t minionAddr = *(uint64_t*)(minionArray + i * 8);
                            if (!minionAddr || minionAddr == input.UnitAddress) continue;
                            
                            // Check if enemy
                            uint8_t myTeam = *(uint8_t*)(
                                *(uint64_t*)(moduleBase + Offset::oLocalPlayer) + Offset::TeamID);
                            uint8_t minionTeam = *(uint8_t*)(minionAddr + Offset::TeamID);
                            
                            if (myTeam != minionTeam) {
                                // Get minion prediction
                                PredictionInput minionInput = input;
                                minionInput.UnitAddress = minionAddr;
                                auto minionPred = GetPrediction(minionInput, false, false);
                                
                                // Check if minion will be in skillshot path
                                float minionRadius = *(float*)(minionAddr + Offset::oObjRadius);
                                float collisionRadius = input.Radius + 15 + minionRadius;
                                
                                if (PointToLineDistance(minionPred.UnitPosition, from, to) 
                                    <= collisionRadius) {
                                    result.push_back(minionAddr);
                                }
                            }
                        }
                    }
                }
            }
            
            return result;
        }
        
        // ========================================================================
        // HELPER: Point to line distance
        // ========================================================================
        static float PointToLineDistance(Vector3 point, Vector3 lineStart, Vector3 lineEnd)
        {
            Vector3 line = lineEnd - lineStart;
            float lineLen = line.Length();
            if (lineLen < 0.1f) return (point - lineStart).Length();
            
            line = line / lineLen;
            
            Vector3 v = point - lineStart;
            float t = v.x * line.x + v.z * line.z;
            t = std::max(0.0f, std::min(lineLen, t));
            
            Vector3 closest = lineStart + line * t;
            return (point - closest).Length();
        }
    };

    // ============================================================================
    // SPELL CLASS - Helper for casting predicted spells
    // ============================================================================
    class Spell
    {
    public:
        float Delay = 0.25f;
        float Speed = FLT_MAX;
        float Width = 0.0f;
        float Range = 0.0f;
        SkillshotType Type = SkillshotType::Line;
        bool Collision = false;
        CollisionableObjects CollisionFlags = CollisionableObjects::Default;
        
        Spell() = default;
        
        Spell(float range, float delay = 0.25f, float speed = FLT_MAX, float width = 0.0f)
            : Range(range), Delay(delay), Speed(speed), Width(width) {}
        
        // Set skillshot parameters
        void SetSkillshot(float delay, float width, float speed, bool collision, SkillshotType type)
        {
            this->Delay = delay;
            this->Width = width;
            this->Speed = speed;
            this->Collision = collision;
            this->Type = type;
        }
        
        // Get prediction for target
        PredictionOutput GetPrediction(uint64_t targetAddress)
        {
            PredictionInput input;
            input.UnitAddress = targetAddress;
            input.Delay = Delay;
            input.Speed = Speed;
            input.Radius = Width;
            input.Range = Range;
            input.Type = Type;
            input.Collision = Collision;
            input.CollisionFlags = CollisionFlags;
            
            // Get local player position
            uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
            uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
            if (localPlayer) {
                input.From = *(Vector3*)(localPlayer + Offset::oObjPosition);
            }
            
            return Prediction::GetPrediction(input, true, true);
        }
        
        // Get AoE prediction (multi-target)
        PredictionOutput GetAoePrediction(uint64_t primaryTarget)
        {
            PredictionInput input;
            input.UnitAddress = primaryTarget;
            input.Delay = Delay;
            input.Speed = Speed;
            input.Radius = Width;
            input.Range = Range;
            input.Type = Type;
            input.Collision = Collision;
            input.CollisionFlags = CollisionFlags;
            input.Aoe = true;
            
            uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
            uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
            if (localPlayer) {
                input.From = *(Vector3*)(localPlayer + Offset::oObjPosition);
            }
            
            return AoePrediction::GetPrediction(input);
        }
        
        // Check if we're in range of target
        bool IsInRange(uint64_t targetAddress)
        {
            if (targetAddress == 0) return false;
            
            uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
            uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
            if (!localPlayer) return false;
            
            Vector3 myPos = *(Vector3*)(localPlayer + Offset::oObjPosition);
            Vector3 targetPos = *(Vector3*)(targetAddress + Offset::oObjPosition);
            
            return (targetPos - myPos).Length() <= Range;
        }
        
        // Check if spell will collide before reaching target
        bool WillHitTarget(uint64_t targetAddress)
        {
            auto pred = GetPrediction(targetAddress);
            return pred.Hitchance >= HitChance::High && pred.CollisionObjects.empty();
        }
    };

    // ============================================================================
    // GEOMETRY UTILITIES
    // Helper functions for geometric calculations
    // ============================================================================
    class Geometry
    {
    public:
        // Rotate a 2D vector by angle (radians)
        static Vector3 Rotated(Vector3 v, float angle)
        {
            float c = cosf(angle);
            float s = sinf(angle);
            return Vector3(v.x * c - v.z * s, 0, v.z * c + v.x * s);
        }
        
        // Normalize a vector (XZ plane)
        static Vector3 Normalized(Vector3 v)
        {
            float len = sqrtf(v.x * v.x + v.z * v.z);
            if (len < 0.0001f) return Vector3(0, 0, 0);
            return Vector3(v.x / len, 0, v.z / len);
        }
        
        // Cross product (2D, returns scalar)
        static float CrossProduct(Vector3 a, Vector3 b)
        {
            return a.x * b.z - a.z * b.x;
        }
        
        // Dot product (XZ plane)
        static float DotProduct(Vector3 a, Vector3 b)
        {
            return a.x * b.x + a.z * b.z;
        }
        
        // Angle between two vectors
        static float AngleBetween(Vector3 a, Vector3 b)
        {
            float lenA = sqrtf(a.x * a.x + a.z * a.z);
            float lenB = sqrtf(b.x * b.x + b.z * b.z);
            if (lenA < 0.0001f || lenB < 0.0001f) return 0.0f;
            
            float dot = (a.x * b.x + a.z * b.z) / (lenA * lenB);
            dot = std::max(-1.0f, std::min(1.0f, dot));
            return acosf(dot) * 180.0f / 3.14159265f;
        }
        
        // Distance from point to line segment
        static float DistanceToLineSegment(Vector3 point, Vector3 lineStart, Vector3 lineEnd)
        {
            Vector3 line = lineEnd - lineStart;
            float lineLenSq = line.x * line.x + line.z * line.z;
            
            if (lineLenSq < 0.0001f) {
                return (point - lineStart).Length();
            }
            
            float t = std::max(0.0f, std::min(1.0f,
                DotProduct(point - lineStart, line) / lineLenSq));
            
            Vector3 proj = lineStart + line * t;
            return (point - proj).Length();
        }
        
        // Circle-circle intersection points
        static std::vector<Vector3> CircleCircleIntersection(
            Vector3 center1, Vector3 center2, float radius1, float radius2)
        {
            std::vector<Vector3> result;
            
            float d = (center2 - center1).Length();
            
            // No intersection
            if (d > radius1 + radius2 || d < fabsf(radius1 - radius2) || d < 0.0001f) {
                return result;
            }
            
            float a = (radius1 * radius1 - radius2 * radius2 + d * d) / (2 * d);
            float h = sqrtf(std::max(0.0f, radius1 * radius1 - a * a));
            
            Vector3 direction = Normalized(center2 - center1);
            Vector3 p = center1 + direction * a;
            Vector3 perpendicular = Vector3(-direction.z, 0, direction.x);
            
            result.push_back(p + perpendicular * h);
            if (h > 0.0001f) {
                result.push_back(p - perpendicular * h);
            }
            
            return result;
        }
        
        // Project point onto line
        struct ProjectionResult {
            Vector3 LinePoint;
            Vector3 SegmentPoint;
            bool IsOnSegment;
        };
        
        static ProjectionResult ProjectOn(Vector3 point, Vector3 lineStart, Vector3 lineEnd)
        {
            ProjectionResult result;
            
            Vector3 line = lineEnd - lineStart;
            float lineLenSq = line.x * line.x + line.z * line.z;
            
            if (lineLenSq < 0.0001f) {
                result.LinePoint = lineStart;
                result.SegmentPoint = lineStart;
                result.IsOnSegment = true;
                return result;
            }
            
            float t = DotProduct(point - lineStart, line) / lineLenSq;
            result.LinePoint = lineStart + line * t;
            result.SegmentPoint = lineStart + line * std::max(0.0f, std::min(1.0f, t));
            result.IsOnSegment = (t >= 0.0f && t <= 1.0f);
            
            return result;
        }
    };

    // ============================================================================
    // MINIMUM ENCLOSING CIRCLE (MEC)
    // Finds smallest circle that contains all given points
    // ============================================================================
    struct MecCircle
    {
        Vector3 Center;
        float Radius;
        
        MecCircle() : Center(0, 0, 0), Radius(0) {}
        MecCircle(Vector3 c, float r) : Center(c), Radius(r) {}
    };

    class MEC
    {
    public:
        static MecCircle GetMec(const std::vector<Vector3>& points)
        {
            if (points.empty()) return MecCircle();
            if (points.size() == 1) return MecCircle(points[0], 0);
            if (points.size() == 2) {
                Vector3 center = (points[0] + points[1]) * 0.5f;
                float radius = (points[0] - center).Length();
                return MecCircle(center, radius);
            }
            
            // Welzl's algorithm (simplified)
            MecCircle minCircle;
            minCircle.Radius = FLT_MAX;
            
            // Try all pairs for diameter
            for (size_t i = 0; i < points.size(); i++) {
                for (size_t j = i + 1; j < points.size(); j++) {
                    Vector3 center = (points[i] + points[j]) * 0.5f;
                    float radius = (points[i] - center).Length();
                    
                    // Check if all points are inside
                    bool valid = true;
                    for (size_t k = 0; k < points.size(); k++) {
                        if ((points[k] - center).Length() > radius + 0.1f) {
                            valid = false;
                            break;
                        }
                    }
                    
                    if (valid && radius < minCircle.Radius) {
                        minCircle.Center = center;
                        minCircle.Radius = radius;
                    }
                }
            }
            
            // Try all triplets for circumcircle
            for (size_t i = 0; i < points.size(); i++) {
                for (size_t j = i + 1; j < points.size(); j++) {
                    for (size_t k = j + 1; k < points.size(); k++) {
                        auto circle = GetCircumcircle(points[i], points[j], points[k]);
                        
                        if (circle.Radius < minCircle.Radius) {
                            bool valid = true;
                            for (size_t m = 0; m < points.size(); m++) {
                                if ((points[m] - circle.Center).Length() > circle.Radius + 0.1f) {
                                    valid = false;
                                    break;
                                }
                            }
                            
                            if (valid) {
                                minCircle = circle;
                            }
                        }
                    }
                }
            }
            
            return minCircle;
        }
        
    private:
        static MecCircle GetCircumcircle(Vector3 a, Vector3 b, Vector3 c)
        {
            float ax = a.x, az = a.z;
            float bx = b.x, bz = b.z;
            float cx = c.x, cz = c.z;
            
            float d = 2 * (ax * (bz - cz) + bx * (cz - az) + cx * (az - bz));
            if (fabsf(d) < 0.0001f) {
                return MecCircle(Vector3(0, 0, 0), FLT_MAX);
            }
            
            float aSq = ax * ax + az * az;
            float bSq = bx * bx + bz * bz;
            float cSq = cx * cx + cz * cz;
            
            float ux = (aSq * (bz - cz) + bSq * (cz - az) + cSq * (az - bz)) / d;
            float uz = (aSq * (cx - bx) + bSq * (ax - cx) + cSq * (bx - ax)) / d;
            
            Vector3 center(ux, 0, uz);
            float radius = (a - center).Length();
            
            return MecCircle(center, radius);
        }
    };

    // ============================================================================
    // POSSIBLE TARGET - Helper struct for AoE prediction
    // ============================================================================
    struct PossibleTarget
    {
        Vector3 Position;
        uint64_t UnitAddress;
    };

    // ============================================================================
    // AOE PREDICTION - Multi-target prediction for skillshots
    // Port from LeagueSharp.Common AoePrediction
    // ============================================================================
    class AoePrediction
    {
    public:
        // Main AoE prediction entry point
        static PredictionOutput GetPrediction(PredictionInput input)
        {
            switch (input.Type) {
                case SkillshotType::Circle:
                    return CirclePrediction::GetPrediction(input);
                case SkillshotType::Cone:
                    return ConePrediction::GetPrediction(input);
                case SkillshotType::Line:
                default:
                    return LinePrediction::GetPrediction(input);
            }
        }
        
        // Get possible enemy targets for AoE
        static std::vector<PossibleTarget> GetPossibleTargets(PredictionInput& input)
        {
            std::vector<PossibleTarget> result;
            uint64_t originalUnit = input.UnitAddress;
            
            // Get enemy hero list
            uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
            uint64_t heroListPtr = *(uint64_t*)(moduleBase + Offset::oHeroes);
            
            if (!heroListPtr) return result;
            
            uint64_t heroArray = *(uint64_t*)(heroListPtr + Offset::HeroListArray);
            int heroCount = *(int*)(heroListPtr + Offset::HeroListCount);
            
            if (!heroArray || heroCount <= 0 || heroCount > 12) return result;
            
            uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
            if (!localPlayer) return result;
            
            uint8_t myTeam = *(uint8_t*)(localPlayer + Offset::TeamID);
            
            for (int i = 0; i < heroCount; i++) {
                uint64_t heroAddr = *(uint64_t*)(heroArray + i * 8);
                if (!heroAddr || heroAddr == originalUnit) continue;
                
                // Check if enemy
                uint8_t heroTeam = *(uint8_t*)(heroAddr + Offset::TeamID);
                if (heroTeam == myTeam) continue;
                
                // Check if alive and targetable
                bool isDead = *(uint8_t*)(heroAddr + Offset::oDead) != 0;
                if (isDead) continue;
                
                // Check if in range
                Vector3 heroPos = *(Vector3*)(heroAddr + Offset::oObjPosition);
                Vector3 myPos = *(Vector3*)(localPlayer + Offset::oObjPosition);
                if ((heroPos - myPos).Length() > input.Range + 200 + input.GetRealRadius()) continue;
                
                // Get prediction for this enemy
                input.UnitAddress = heroAddr;
                auto prediction = Prediction::GetPrediction(input, false, false);
                
                if (prediction.Hitchance >= HitChance::High) {
                    PossibleTarget target;
                    target.Position = prediction.UnitPosition;
                    target.UnitAddress = heroAddr;
                    result.push_back(target);
                }
            }
            
            input.UnitAddress = originalUnit;
            return result;
        }
        
        // ========================================================================
        // CIRCLE PREDICTION - Find best position for circular AoE
        // ========================================================================
        class CirclePrediction
        {
        public:
            static PredictionOutput GetPrediction(PredictionInput input)
            {
                auto mainPred = Prediction::GetPrediction(input, false, true);
                
                std::vector<PossibleTarget> targets;
                PossibleTarget mainTarget;
                mainTarget.Position = mainPred.UnitPosition;
                mainTarget.UnitAddress = input.UnitAddress;
                targets.push_back(mainTarget);
                
                if (mainPred.Hitchance >= HitChance::Medium) {
                    auto additionalTargets = GetPossibleTargets(input);
                    targets.insert(targets.end(), additionalTargets.begin(), additionalTargets.end());
                }
                
                // Try to find smallest circle containing most targets
                while (targets.size() > 1) {
                    std::vector<Vector3> positions;
                    for (const auto& t : targets) {
                        positions.push_back(t.Position);
                    }
                    
                    auto mecCircle = MEC::GetMec(positions);
                    
                    // Check if circle fits within skill radius and range
                    if (mecCircle.Radius <= input.GetRealRadius() - 10 &&
                        (mecCircle.Center - input.GetRangeCheckFrom()).Length() < input.Range) {
                        
                        PredictionOutput result;
                        result.Input = input;
                        result.CastPosition = mecCircle.Center;
                        result.UnitPosition = mainPred.UnitPosition;
                        result.Hitchance = mainPred.Hitchance;
                        result.AoeTargetsHitCount = (int)targets.size();
                        return result;
                    }
                    
                    // Remove farthest target from main target
                    float maxDist = -1;
                    size_t maxIdx = 1;
                    for (size_t i = 1; i < targets.size(); i++) {
                        float dist = (targets[i].Position - targets[0].Position).Length();
                        if (dist > maxDist) {
                            maxDist = dist;
                            maxIdx = i;
                        }
                    }
                    targets.erase(targets.begin() + maxIdx);
                }
                
                return mainPred;
            }
        };
        
        // ========================================================================
        // CONE PREDICTION - Find best direction for cone AoE
        // ========================================================================
        class ConePrediction
        {
        public:
            static PredictionOutput GetPrediction(PredictionInput input)
            {
                auto mainPred = Prediction::GetPrediction(input, false, true);
                
                std::vector<PossibleTarget> targets;
                PossibleTarget mainTarget;
                mainTarget.Position = mainPred.UnitPosition;
                mainTarget.UnitAddress = input.UnitAddress;
                targets.push_back(mainTarget);
                
                if (mainPred.Hitchance >= HitChance::Medium) {
                    auto additionalTargets = GetPossibleTargets(input);
                    targets.insert(targets.end(), additionalTargets.begin(), additionalTargets.end());
                }
                
                if (targets.size() > 1) {
                    // Convert positions relative to From
                    std::vector<Vector3> relativePositions;
                    for (auto& t : targets) {
                        relativePositions.push_back(t.Position - input.From);
                    }
                    
                    // Try all midpoint directions
                    std::vector<Vector3> candidates;
                    for (size_t i = 0; i < relativePositions.size(); i++) {
                        for (size_t j = i + 1; j < relativePositions.size(); j++) {
                            Vector3 mid = (relativePositions[i] + relativePositions[j]) * 0.5f;
                            mid = Geometry::Normalized(mid);
                            if (mid.Length() > 0.1f) {
                                candidates.push_back(mid);
                            }
                        }
                    }
                    
                    // Find best candidate
                    int bestHits = -1;
                    Vector3 bestCandidate;
                    
                    for (const auto& candidate : candidates) {
                        int hits = GetConeHits(candidate, input.Range, input.Radius, relativePositions);
                        if (hits > bestHits) {
                            bestHits = hits;
                            bestCandidate = candidate;
                        }
                    }
                    
                    if (bestHits > 1 && bestCandidate.Length() > 0.1f) {
                        PredictionOutput result;
                        result.Input = input;
                        result.CastPosition = input.From + bestCandidate * input.Range;
                        result.UnitPosition = mainPred.UnitPosition;
                        result.Hitchance = mainPred.Hitchance;
                        result.AoeTargetsHitCount = bestHits;
                        return result;
                    }
                }
                
                return mainPred;
            }
            
        private:
            static int GetConeHits(Vector3 direction, float range, float angle, 
                                   const std::vector<Vector3>& points)
            {
                int count = 0;
                Vector3 edge1 = Geometry::Rotated(direction, -angle / 2);
                Vector3 edge2 = Geometry::Rotated(direction, angle / 2);
                
                for (const auto& point : points) {
                    float dist = point.Length();
                    if (dist <= range) {
                        // Check if point is within cone angle
                        float cross1 = Geometry::CrossProduct(edge1, point);
                        float cross2 = Geometry::CrossProduct(point, edge2);
                        if (cross1 > 0 && cross2 > 0) {
                            count++;
                        }
                    }
                }
                
                return count;
            }
        };
        
        // ========================================================================
        // LINE PREDICTION - Find best direction for line AoE
        // ========================================================================
        class LinePrediction
        {
        public:
            static PredictionOutput GetPrediction(PredictionInput input)
            {
                auto mainPred = Prediction::GetPrediction(input, false, true);
                
                std::vector<PossibleTarget> targets;
                PossibleTarget mainTarget;
                mainTarget.Position = mainPred.UnitPosition;
                mainTarget.UnitAddress = input.UnitAddress;
                targets.push_back(mainTarget);
                
                if (mainPred.Hitchance >= HitChance::Medium) {
                    auto additionalTargets = GetPossibleTargets(input);
                    targets.insert(targets.end(), additionalTargets.begin(), additionalTargets.end());
                }
                
                if (targets.size() > 1) {
                    // Generate candidate directions
                    std::vector<Vector3> candidates;
                    for (const auto& target : targets) {
                        auto targetCandidates = GetCandidates(
                            input.From, target.Position, input.Radius, input.Range);
                        candidates.insert(candidates.end(), 
                            targetCandidates.begin(), targetCandidates.end());
                    }
                    
                    // Find best candidate
                    int bestHits = -1;
                    Vector3 bestCandidate;
                    std::vector<Vector3> bestHitPoints;
                    
                    std::vector<Vector3> positions;
                    for (const auto& t : targets) positions.push_back(t.Position);
                    
                    for (const auto& candidate : candidates) {
                        // Check if main target is still hit
                        auto mainHits = GetLineHits(input.From, candidate,
                            input.Radius + 30, { targets[0].Position });
                        
                        if (mainHits.size() == 1) {
                            auto hits = GetLineHits(input.From, candidate, input.Radius, positions);
                            
                            if ((int)hits.size() >= bestHits) {
                                bestHits = (int)hits.size();
                                bestCandidate = candidate;
                                bestHitPoints = hits;
                            }
                        }
                    }
                    
                    if (bestHits > 1) {
                        // Center the line between hit points
                        Vector3 p1 = bestHitPoints.front();
                        Vector3 p2 = bestHitPoints.back();
                        Vector3 castPos = (p1 + p2) * 0.5f;
                        
                        PredictionOutput result;
                        result.Input = input;
                        result.CastPosition = castPos;
                        result.UnitPosition = mainPred.UnitPosition;
                        result.Hitchance = mainPred.Hitchance;
                        result.AoeTargetsHitCount = bestHits;
                        return result;
                    }
                }
                
                return mainPred;
            }
            
        private:
            static std::vector<Vector3> GetCandidates(Vector3 from, Vector3 to, 
                                                       float radius, float range)
            {
                std::vector<Vector3> result;
                
                Vector3 midPoint = (from + to) * 0.5f;
                float midDist = (midPoint - from).Length();
                
                auto intersections = Geometry::CircleCircleIntersection(
                    from, midPoint, radius, midDist);
                
                if (intersections.size() >= 2) {
                    Vector3 toDir = Geometry::Normalized(to - from);
                    
                    result.push_back(from + Geometry::Normalized(to - intersections[0]) * range);
                    result.push_back(from + Geometry::Normalized(to - intersections[1]) * range);
                }
                
                return result;
            }
            
            static std::vector<Vector3> GetLineHits(Vector3 start, Vector3 end,
                                                     float radius, const std::vector<Vector3>& points)
            {
                std::vector<Vector3> result;
                
                for (const auto& point : points) {
                    float dist = Geometry::DistanceToLineSegment(point, start, end);
                    if (dist <= radius) {
                        result.push_back(point);
                    }
                }
                
                return result;
            }
        };
    };
}

