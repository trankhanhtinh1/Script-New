#pragma once
#include <cstdint>
#include <windows.h>
#include <vector>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "../Vector.h"
#include "Offsets.h"

// ============================================================================
// AI MANAGER NAVGRID-BASED OFFSET SCANNER
// ============================================================================
// Approach: Use NavGrid pointer (0x1D32A80) as reference to find AiManager
// path-related offsets by analyzing memory patterns around path data.
//
// Target offsets to find:
//   - oObjAiMgrCurrentSegment: Current path segment index
//   - oObjAiMgrNavArray: Pointer to waypoints array
//   - oObjAiMgrSegmentsCount: Number of path segments
//   - oObjAiMgrMoveVec3: Movement direction vector
//   - oObjAiMgrServerPos: Server-side position
//
// NavGrid Signature: 48 8B 05 ? ? ? ? 0F 28 DA -> 0x1D32A80
// ============================================================================

namespace AiManagerNavGridScan
{
    // ============================================================================
    // SAFE MEMORY READ HELPERS
    // ============================================================================
    
    inline bool SafeReadByte(uint64_t addr, uint8_t* out) {
        __try {
            *out = *(uint8_t*)addr;
            return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    
    inline bool SafeReadDWORD(uint64_t addr, uint32_t* out) {
        __try {
            *out = *(uint32_t*)addr;
            return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    
    inline bool SafeReadQWORD(uint64_t addr, uint64_t* out) {
        __try {
            *out = *(uint64_t*)addr;
            return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    
    inline bool SafeReadFloat(uint64_t addr, float* out) {
        __try {
            *out = *(float*)addr;
            return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    
    inline bool TryReadVec3(uint64_t addr, Vector3& out) {
        float x, y, z;
        if (SafeReadFloat(addr, &x) && SafeReadFloat(addr + 4, &y) && SafeReadFloat(addr + 8, &z)) {
            if (x > 0 && x < 20000 && z > 0 && z < 20000 && y >= -1000 && y < 2000) {
                out.x = x; out.y = y; out.z = z;
                return true;
            }
        }
        return false;
    }
    
    inline bool IsValidPointer(uint64_t ptr) {
        return ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF;
    }
    
    // ============================================================================
    // NAVGRID REFERENCE SCANNER
    // ============================================================================
    
    struct NavGridInfo {
        uint64_t navGridPtr;
        uint64_t managerPtr;
        int width;
        int height;
        float scale;
        float minX;
        float minZ;
        bool valid;
    };
    
    inline NavGridInfo GetNavGridInfo() {
        NavGridInfo info = {0};
        
        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
        uint64_t navGridGlobal = moduleBase + Offset::NavigationGrid::GlobalPtr;
        
        uint64_t navGrid = 0;
        if (!SafeReadQWORD(navGridGlobal, &navGrid) || !IsValidPointer(navGrid)) {
            return info;
        }
        
        info.navGridPtr = navGrid;
        
        // Get manager
        uint64_t manager = 0;
        if (!SafeReadQWORD(navGrid + Offset::NavigationGrid::Manager::Manager, &manager) || !IsValidPointer(manager)) {
            return info;
        }
        
        info.managerPtr = manager;
        
        // Read grid properties
        uint32_t w = 0, h = 0;
        float s = 0, mx = 0, mz = 0;
        
        SafeReadDWORD(manager + Offset::NavigationGrid::Manager::Width, &w);
        SafeReadDWORD(manager + Offset::NavigationGrid::Manager::Height, &h);
        SafeReadFloat(manager + Offset::NavigationGrid::Manager::Scale, &s);
        SafeReadFloat(manager + Offset::NavigationGrid::Manager::MinimumX, &mx);
        SafeReadFloat(manager + Offset::NavigationGrid::Manager::MinimumZ, &mz);
        
        info.width = w;
        info.height = h;
        info.scale = s;
        info.minX = mx;
        info.minZ = mz;
        info.valid = (w > 0 && h > 0 && s > 0);
        
        return info;
    }
    
    // ============================================================================
    // AIMANAGER PATH STRUCTURE SCANNER
    // ============================================================================
    // Strategy:
    // 1. Get AiManager from LocalPlayer + 0x3108
    // 2. Scan for pointer patterns that could be NavArray
    // 3. Validate pointers by checking if they point to valid Vec3 arrays
    // 4. Find segment count near NavArray pointer
    // ============================================================================
    
    struct PathArrayCandidate {
        uint64_t offset;        // Offset from AiManager base
        uint64_t arrayPtr;      // Pointer value
        int validVec3Count;     // Number of valid Vec3s found
        Vector3 firstVec3;      // First waypoint
        Vector3 lastVec3;       // Last waypoint
    };
    
    inline std::vector<PathArrayCandidate> ScanForNavArrayCandidates(uint64_t aiManager) {
        std::vector<PathArrayCandidate> candidates;
        
        // Scan 0x200 - 0x500 for pointer-like values
        for (uint64_t off = 0x200; off < 0x500; off += 8) {
            uint64_t ptr = 0;
            if (!SafeReadQWORD(aiManager + off, &ptr)) continue;
            if (!IsValidPointer(ptr)) continue;
            
            // Check if this pointer leads to valid Vec3 array
            PathArrayCandidate candidate = {0};
            candidate.offset = off;
            candidate.arrayPtr = ptr;
            candidate.validVec3Count = 0;
            
            // Try to read up to 20 Vec3s from this pointer
            for (int i = 0; i < 20; i++) {
                Vector3 v;
                if (TryReadVec3(ptr + (i * 12), v)) {
                    if (candidate.validVec3Count == 0) {
                        candidate.firstVec3 = v;
                    }
                    candidate.lastVec3 = v;
                    candidate.validVec3Count++;
                } else {
                    break; // Stop at first invalid
                }
            }
            
            // Only add if we found at least 2 valid Vec3s (path needs 2+ points)
            if (candidate.validVec3Count >= 2) {
                candidates.push_back(candidate);
            }
        }
        
        return candidates;
    }
    
    // ============================================================================
    // SEGMENT COUNT SCANNER
    // ============================================================================
    // Look for small integers (1-20) near NavArray pointer
    
    struct SegmentCountCandidate {
        uint64_t offset;
        int value;
        int distanceFromNavArray;
    };
    
    inline std::vector<SegmentCountCandidate> ScanForSegmentCount(uint64_t aiManager, uint64_t navArrayOffset) {
        std::vector<SegmentCountCandidate> candidates;
        
        // Scan around NavArray offset (+/- 0x40 bytes)
        int64_t startOff = (int64_t)navArrayOffset - 0x40;
        if (startOff < 0) startOff = 0;
        uint64_t endOff = navArrayOffset + 0x40;
        
        for (uint64_t off = startOff; off <= endOff; off += 4) {
            if (off == navArrayOffset) continue; // Skip the pointer itself
            
            uint32_t val = 0;
            if (!SafeReadDWORD(aiManager + off, &val)) continue;
            
            // Segment count should be 1-20
            if (val >= 1 && val <= 20) {
                SegmentCountCandidate c;
                c.offset = off;
                c.value = val;
                c.distanceFromNavArray = (int)(off - navArrayOffset);
                candidates.push_back(c);
            }
        }
        
        return candidates;
    }
    
    // ============================================================================
    // CURRENT SEGMENT SCANNER
    // ============================================================================
    // Look for integers 0 to SegmentCount-1
    
    inline std::vector<SegmentCountCandidate> ScanForCurrentSegment(uint64_t aiManager, uint64_t navArrayOffset, int maxSegments) {
        std::vector<SegmentCountCandidate> candidates;
        
        int64_t startOff = (int64_t)navArrayOffset - 0x40;
        if (startOff < 0) startOff = 0;
        uint64_t endOff = navArrayOffset + 0x40;
        
        for (uint64_t off = startOff; off <= endOff; off += 4) {
            uint32_t val = 0;
            if (!SafeReadDWORD(aiManager + off, &val)) continue;
            
            // Current segment should be 0 to maxSegments-1
            if (val >= 0 && val < (uint32_t)maxSegments) {
                SegmentCountCandidate c;
                c.offset = off;
                c.value = val;
                c.distanceFromNavArray = (int)(off - navArrayOffset);
                candidates.push_back(c);
            }
        }
        
        return candidates;
    }
    
    // ============================================================================
    // MOVE VECTOR SCANNER
    // ============================================================================
    // Look for normalized Vec3 (length ~1.0) that represents direction
    
    struct MoveVecCandidate {
        uint64_t offset;
        Vector3 vec;
        float length;
    };
    
    inline std::vector<MoveVecCandidate> ScanForMoveVector(uint64_t aiManager) {
        std::vector<MoveVecCandidate> candidates;
        
        // Scan 0x400 - 0x600 for direction vectors
        for (uint64_t off = 0x400; off < 0x600; off += 4) {
            float x, y, z;
            if (!SafeReadFloat(aiManager + off, &x)) continue;
            if (!SafeReadFloat(aiManager + off + 4, &y)) continue;
            if (!SafeReadFloat(aiManager + off + 8, &z)) continue;
            
            // Direction vector should have length close to 1.0
            float len = sqrtf(x*x + y*y + z*z);
            if (len > 0.9f && len < 1.1f) {
                MoveVecCandidate c;
                c.offset = off;
                c.vec = Vector3(x, y, z);
                c.length = len;
                candidates.push_back(c);
            }
        }
        
        return candidates;
    }
    
    // ============================================================================
    // SERVER POSITION SCANNER
    // ============================================================================
    // Look for Vec3 that matches or is very close to StartPath
    
    struct ServerPosCandidate {
        uint64_t offset;
        Vector3 pos;
        float distanceFromStartPath;
    };
    
    inline std::vector<ServerPosCandidate> ScanForServerPos(uint64_t aiManager, Vector3 startPath) {
        std::vector<ServerPosCandidate> candidates;
        
        // Scan 0x00 - 0x600 for positions close to StartPath
        for (uint64_t off = 0x00; off < 0x600; off += 4) {
            if (off == 0x1E0) continue; // Skip known StartPath
            
            Vector3 v;
            if (!TryReadVec3(aiManager + off, v)) continue;
            
            // Check distance from StartPath
            float dx = v.x - startPath.x;
            float dy = v.y - startPath.y;
            float dz = v.z - startPath.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            
            // ServerPos should be very close to StartPath (within 50 units)
            if (dist < 50.0f) {
                ServerPosCandidate c;
                c.offset = off;
                c.pos = v;
                c.distanceFromStartPath = dist;
                candidates.push_back(c);
            }
        }
        
        return candidates;
    }
    
    // ============================================================================
    // MAIN SCAN FUNCTION
    // ============================================================================
    
    inline void ScanAiManagerWithNavGrid(uint64_t localPlayerAddr) {
        std::ofstream file("aimanager_navgrid_scan.txt");
        if (!file.is_open()) return;
        
        file << "====================================================================\n";
        file << "AI MANAGER NAVGRID-BASED OFFSET SCAN - 15.24\n";
        file << "====================================================================\n";
        file << "Time: " << GetTickCount() << "\n\n";
        
        // =====================================================================
        // STEP 1: Verify NavGrid is accessible
        // =====================================================================
        file << "=== STEP 1: NAVGRID VERIFICATION ===\n";
        NavGridInfo navInfo = GetNavGridInfo();
        
        if (!navInfo.valid) {
            file << "ERROR: NavGrid not accessible!\n";
            file << "NavGrid Ptr: " << std::hex << navInfo.navGridPtr << std::dec << "\n";
            file << "Manager Ptr: " << std::hex << navInfo.managerPtr << std::dec << "\n";
        } else {
            file << "NavGrid Ptr: " << std::hex << navInfo.navGridPtr << std::dec << "\n";
            file << "Manager Ptr: " << std::hex << navInfo.managerPtr << std::dec << "\n";
            file << "Grid Size: " << navInfo.width << " x " << navInfo.height << "\n";
            file << "Scale: " << navInfo.scale << "\n";
            file << "Min X/Z: " << navInfo.minX << ", " << navInfo.minZ << "\n";
        }
        file << "\n";
        
        // =====================================================================
        // STEP 2: Get AiManager
        // =====================================================================
        file << "=== STEP 2: AIMANAGER ACCESS ===\n";
        
        uint64_t aiManager = 0;
        if (!SafeReadQWORD(localPlayerAddr + 0x3108, &aiManager) || !IsValidPointer(aiManager)) {
            file << "ERROR: Cannot read AiManager at LocalPlayer + 0x3108\n";
            file.close();
            return;
        }
        
        file << "LocalPlayer: " << std::hex << localPlayerAddr << std::dec << "\n";
        file << "AiManager: " << std::hex << aiManager << std::dec << "\n\n";
        
        // Read known offsets for reference
        Vector3 startPath, endPath;
        TryReadVec3(aiManager + 0x1E0, startPath);
        TryReadVec3(aiManager + 0x23C, endPath);
        
        file << "Known StartPath (0x1E0): (" << startPath.x << ", " << startPath.y << ", " << startPath.z << ")\n";
        file << "Known EndPath (0x23C): (" << endPath.x << ", " << endPath.y << ", " << endPath.z << ")\n\n";
        
        // =====================================================================
        // STEP 3: Scan for NavArray (pointer to Vec3 array)
        // =====================================================================
        file << "=== STEP 3: NAVARRAY CANDIDATES ===\n";
        file << "Looking for pointers that lead to valid Vec3 arrays...\n\n";
        
        auto navArrayCandidates = ScanForNavArrayCandidates(aiManager);
        
        if (navArrayCandidates.empty()) {
            file << "No NavArray candidates found!\n";
            file << "This may mean:\n";
            file << "  1. Path data is stored inline (not as pointer)\n";
            file << "  2. Path structure has changed in 15.24\n";
            file << "  3. Need to scan different offset range\n\n";
        } else {
            for (const auto& c : navArrayCandidates) {
                file << "Offset 0x" << std::hex << c.offset << std::dec << ":\n";
                file << "  Pointer: " << std::hex << c.arrayPtr << std::dec << "\n";
                file << "  Valid Vec3 Count: " << c.validVec3Count << "\n";
                file << "  First: (" << c.firstVec3.x << ", " << c.firstVec3.y << ", " << c.firstVec3.z << ")\n";
                file << "  Last: (" << c.lastVec3.x << ", " << c.lastVec3.y << ", " << c.lastVec3.z << ")\n";
                
                // Check if first matches StartPath
                float dx = c.firstVec3.x - startPath.x;
                float dz = c.firstVec3.z - startPath.z;
                float dist = sqrtf(dx*dx + dz*dz);
                if (dist < 100) {
                    file << "  *** LIKELY NAVARRAY! First waypoint matches StartPath ***\n";
                }
                file << "\n";
            }
        }
        
        // =====================================================================
        // STEP 4: Scan for SegmentsCount
        // =====================================================================
        file << "=== STEP 4: SEGMENTS COUNT CANDIDATES ===\n";
        file << "Looking for small integers (1-20) near potential NavArray...\n\n";
        
        // Use best NavArray candidate or default offset
        uint64_t bestNavArrayOffset = navArrayCandidates.empty() ? 0x348 : navArrayCandidates[0].offset;
        
        auto segCountCandidates = ScanForSegmentCount(aiManager, bestNavArrayOffset);
        
        for (const auto& c : segCountCandidates) {
            file << "Offset 0x" << std::hex << c.offset << std::dec << ": " << c.value;
            file << " (distance from NavArray: " << c.distanceFromNavArray << " bytes)\n";
        }
        file << "\n";
        
        // =====================================================================
        // STEP 5: Scan for MoveVec3 (direction vector)
        // =====================================================================
        file << "=== STEP 5: MOVE VECTOR CANDIDATES ===\n";
        file << "Looking for normalized direction vectors (length ~1.0)...\n\n";
        
        auto moveVecCandidates = ScanForMoveVector(aiManager);
        
        if (moveVecCandidates.empty()) {
            file << "No normalized direction vectors found in 0x400-0x600\n";
        } else {
            for (const auto& c : moveVecCandidates) {
                file << "Offset 0x" << std::hex << c.offset << std::dec << ": ";
                file << "(" << c.vec.x << ", " << c.vec.y << ", " << c.vec.z << ")";
                file << " len=" << c.length << "\n";
            }
        }
        file << "\n";
        
        // =====================================================================
        // STEP 6: Scan for ServerPos
        // =====================================================================
        file << "=== STEP 6: SERVER POSITION CANDIDATES ===\n";
        file << "Looking for Vec3 close to StartPath (within 50 units)...\n\n";
        
        auto serverPosCandidates = ScanForServerPos(aiManager, startPath);
        
        for (const auto& c : serverPosCandidates) {
            file << "Offset 0x" << std::hex << c.offset << std::dec << ": ";
            file << "(" << c.pos.x << ", " << c.pos.y << ", " << c.pos.z << ")";
            file << " dist=" << c.distanceFromStartPath << "\n";
            
            if (c.distanceFromStartPath < 5.0f && c.offset != 0x1E0) {
                file << "  *** LIKELY SERVER POS! Very close to StartPath ***\n";
            }
        }
        file << "\n";
        
        // =====================================================================
        // STEP 7: Raw memory dump of interesting regions
        // =====================================================================
        file << "=== STEP 7: RAW MEMORY DUMP ===\n";
        file << "Dumping 0x200-0x400 for manual analysis...\n\n";
        
        for (uint64_t off = 0x200; off < 0x400; off += 0x10) {
            file << std::hex << std::setw(3) << std::setfill('0') << off << ": ";
            
            for (int i = 0; i < 16; i++) {
                uint8_t b = 0;
                SafeReadByte(aiManager + off + i, &b);
                file << std::setw(2) << std::setfill('0') << (int)b << " ";
            }
            
            file << " | ";
            
            // Also show as floats
            for (int i = 0; i < 4; i++) {
                float f = 0;
                SafeReadFloat(aiManager + off + (i * 4), &f);
                file << std::dec << std::setw(10) << std::fixed << std::setprecision(2) << f << " ";
            }
            
            file << std::dec << "\n";
        }
        
        // =====================================================================
        // SUMMARY
        // =====================================================================
        file << "\n====================================================================\n";
        file << "SCAN SUMMARY\n";
        file << "====================================================================\n";
        file << "NavArray Candidates: " << navArrayCandidates.size() << "\n";
        file << "SegmentCount Candidates: " << segCountCandidates.size() << "\n";
        file << "MoveVec Candidates: " << moveVecCandidates.size() << "\n";
        file << "ServerPos Candidates: " << serverPosCandidates.size() << "\n\n";
        
        file << "NEXT STEPS:\n";
        file << "1. Run this scan while MOVING to see which values change\n";
        file << "2. Compare with IDLE scan to find dynamic offsets\n";
        file << "3. Check if NavArray pointer changes when path updates\n";
        file << "4. Verify SegmentCount matches number of waypoints\n";
        
        file << "\n====================================================================\n";
        file.close();
    }
    
    // ============================================================================
    // COMPARATIVE SCAN (IDLE vs MOVING)
    // ============================================================================
    
    struct CompareScanState {
        bool idleCaptured = false;
        bool moveCaptured = false;
        uint8_t idleData[0x600] = {0};
        uint8_t moveData[0x600] = {0};
        Vector3 idlePos, movePos;
    };
    
    inline CompareScanState g_compareState;
    
    inline void CaptureIdleState(uint64_t localPlayerAddr) {
        uint64_t aiManager = 0;
        if (!SafeReadQWORD(localPlayerAddr + 0x3108, &aiManager)) return;
        
        for (int i = 0; i < 0x600; i++) {
            SafeReadByte(aiManager + i, &g_compareState.idleData[i]);
        }
        TryReadVec3(aiManager + 0x1E0, g_compareState.idlePos);
        g_compareState.idleCaptured = true;
    }
    
    inline void CaptureMoveState(uint64_t localPlayerAddr) {
        uint64_t aiManager = 0;
        if (!SafeReadQWORD(localPlayerAddr + 0x3108, &aiManager)) return;
        
        for (int i = 0; i < 0x600; i++) {
            SafeReadByte(aiManager + i, &g_compareState.moveData[i]);
        }
        TryReadVec3(aiManager + 0x1E0, g_compareState.movePos);
        g_compareState.moveCaptured = true;
    }
    
    inline void WriteCompareResults() {
        if (!g_compareState.idleCaptured || !g_compareState.moveCaptured) return;
        
        std::ofstream file("aimanager_compare_scan.txt");
        if (!file.is_open()) return;
        
        file << "====================================================================\n";
        file << "AI MANAGER IDLE vs MOVING COMPARISON\n";
        file << "====================================================================\n\n";
        
        file << "IDLE Pos: (" << g_compareState.idlePos.x << ", " << g_compareState.idlePos.y << ", " << g_compareState.idlePos.z << ")\n";
        file << "MOVE Pos: (" << g_compareState.movePos.x << ", " << g_compareState.movePos.y << ", " << g_compareState.movePos.z << ")\n\n";
        
        file << "=== CHANGED BYTES ===\n";
        for (int i = 0; i < 0x600; i++) {
            if (g_compareState.idleData[i] != g_compareState.moveData[i]) {
                file << "0x" << std::hex << i << std::dec << ": ";
                file << (int)g_compareState.idleData[i] << " -> " << (int)g_compareState.moveData[i] << "\n";
            }
        }
        
        file << "\n=== CHANGED FLOATS (significant) ===\n";
        for (int i = 0; i < 0x600 - 4; i += 4) {
            float idleF = *(float*)&g_compareState.idleData[i];
            float moveF = *(float*)&g_compareState.moveData[i];
            
            if (fabsf(idleF - moveF) > 1.0f) {
                file << "0x" << std::hex << i << std::dec << ": ";
                file << idleF << " -> " << moveF << "\n";
            }
        }
        
        file << "\n=== CHANGED POINTERS ===\n";
        for (int i = 0; i < 0x600 - 8; i += 8) {
            uint64_t idleP = *(uint64_t*)&g_compareState.idleData[i];
            uint64_t moveP = *(uint64_t*)&g_compareState.moveData[i];
            
            if (idleP != moveP && IsValidPointer(idleP) && IsValidPointer(moveP)) {
                file << "0x" << std::hex << i << ": " << idleP << " -> " << moveP << std::dec << "\n";
            }
        }
        
        file.close();
        
        // Reset state
        g_compareState.idleCaptured = false;
        g_compareState.moveCaptured = false;
    }
    
    // ============================================================================
    // GUIDED NAVGRID SCAN - Automated IDLE -> MOVE -> DASH comparison
    // Similar to AiManagerScan::StartGuidedScan() but focused on path offsets
    // ============================================================================
    
    enum class NavGridScanPhase {
        IDLE,               // Waiting for button click
        CAPTURING_IDLE,     // 2 seconds to stand still
        CAPTURING_MOVE,     // 5 seconds to move around
        CAPTURING_DASH,     // 5 seconds to dash
        ANALYZING,          // Analyzing results
        COMPLETE            // Done, results written
    };
    
    struct NavGridGuidedScanState {
        NavGridScanPhase phase = NavGridScanPhase::IDLE;
        DWORD phaseStartTime = 0;
        bool isActive = false;
        
        // Memory snapshots
        uint8_t idleData[0x600] = {0};
        uint8_t moveData[0x600] = {0};
        uint8_t dashData[0x600] = {0};
        
        // Positions
        Vector3 idlePos = {0, 0, 0};
        Vector3 movePos = {0, 0, 0};
        Vector3 dashPos = {0, 0, 0};
        
        // Detected offsets (results)
        struct DetectedOffsets {
            uint64_t serverPos = 0;         // Vec3 that changes with movement
            uint64_t facingAngle = 0;       // Float that's PI when idle
            uint64_t movementState = 0;     // Int that changes idle->move
            uint64_t segmentCount = 0;      // Small int (1-20)
            uint64_t moveDirection = 0;     // Normalized Vec3
            uint64_t navArrayBegin = 0;     // NavArray begin pointer offset
            uint64_t navArrayEnd = 0;       // NavArray end pointer offset
            int navArrayNodes = 0;          // Number of nodes found
            bool serverPosConfirmed = false;
            bool facingAngleConfirmed = false;
            bool movementStateConfirmed = false;
            bool segmentCountConfirmed = false;
            bool moveDirectionConfirmed = false;
            bool navArrayConfirmed = false;
        } detected;
        
        // Store AiManager address for PATH HUNTER
        uint64_t aiManagerAddr = 0;
        
        // For dash detection
        float maxDashSpeed = 0.0f;
        bool dashDetected = false;
    };
    
    inline NavGridGuidedScanState g_navGridScanState;
    
    // Get status message for ImGui
    inline const char* GetNavGridScanStatusMessage() {
        switch (g_navGridScanState.phase) {
            case NavGridScanPhase::IDLE:
                return "Click 'Start NavGrid Guided Scan' to begin";
            case NavGridScanPhase::CAPTURING_IDLE: {
                DWORD elapsed = GetTickCount() - g_navGridScanState.phaseStartTime;
                DWORD remaining = (elapsed < 2000) ? (2000 - elapsed) / 1000 : 0;
                static char buf[64];
                sprintf_s(buf, sizeof(buf), "[1/3] STAND STILL! Capturing IDLE... %lus", remaining + 1);
                return buf;
            }
            case NavGridScanPhase::CAPTURING_MOVE: {
                DWORD elapsed = GetTickCount() - g_navGridScanState.phaseStartTime;
                DWORD remaining = (elapsed < 5000) ? (5000 - elapsed) / 1000 : 0;
                static char buf[64];
                sprintf_s(buf, sizeof(buf), "[2/3] MOVE AROUND! %lus remaining", remaining + 1);
                return buf;
            }
            case NavGridScanPhase::CAPTURING_DASH: {
                DWORD elapsed = GetTickCount() - g_navGridScanState.phaseStartTime;
                DWORD remaining = (elapsed < 5000) ? (5000 - elapsed) / 1000 : 0;
                static char buf[64];
                sprintf_s(buf, sizeof(buf), "[3/3] DASH NOW! %lus remaining", remaining + 1);
                return buf;
            }
            case NavGridScanPhase::ANALYZING:
                return "Analyzing results...";
            case NavGridScanPhase::COMPLETE:
                return "SCAN COMPLETE! Check todo file for results";
            default:
                return "";
        }
    }
    
    // Get status color for ImGui
    inline void GetNavGridScanStatusColor(float* r, float* g, float* b) {
        switch (g_navGridScanState.phase) {
            case NavGridScanPhase::IDLE: *r = 0.5f; *g = 0.5f; *b = 0.5f; break;
            case NavGridScanPhase::CAPTURING_IDLE: *r = 0.0f; *g = 1.0f; *b = 0.5f; break;
            case NavGridScanPhase::CAPTURING_MOVE: *r = 1.0f; *g = 1.0f; *b = 0.0f; break;
            case NavGridScanPhase::CAPTURING_DASH: *r = 1.0f; *g = 0.3f; *b = 0.3f; break;
            case NavGridScanPhase::ANALYZING: *r = 0.5f; *g = 0.5f; *b = 1.0f; break;
            case NavGridScanPhase::COMPLETE: *r = 0.3f; *g = 1.0f; *b = 0.3f; break;
            default: *r = 1.0f; *g = 1.0f; *b = 1.0f; break;
        }
    }
    
    // Start the guided scan
    inline void StartNavGridGuidedScan() {
        g_navGridScanState.phase = NavGridScanPhase::CAPTURING_IDLE;
        g_navGridScanState.phaseStartTime = GetTickCount();
        g_navGridScanState.isActive = true;
        g_navGridScanState.dashDetected = false;
        g_navGridScanState.maxDashSpeed = 0.0f;
        memset(g_navGridScanState.idleData, 0, sizeof(g_navGridScanState.idleData));
        memset(g_navGridScanState.moveData, 0, sizeof(g_navGridScanState.moveData));
        memset(g_navGridScanState.dashData, 0, sizeof(g_navGridScanState.dashData));
        memset(&g_navGridScanState.detected, 0, sizeof(g_navGridScanState.detected));
    }
    
    // Capture snapshot
    inline bool CaptureNavGridSnapshot(uint64_t localPlayerAddr, uint8_t* outBuffer, Vector3* outPos) {
        uint64_t aiManager = 0;
        if (!SafeReadQWORD(localPlayerAddr + 0x3108, &aiManager)) return false;
        if (!IsValidPointer(aiManager)) return false;
        
        // Store AiManager address for PATH HUNTER
        g_navGridScanState.aiManagerAddr = aiManager;
        
        for (int i = 0; i < 0x600; i++) {
            SafeReadByte(aiManager + i, &outBuffer[i]);
        }
        
        float x, y, z;
        SafeReadFloat(localPlayerAddr + 0x254, &x);
        SafeReadFloat(localPlayerAddr + 0x258, &y);
        SafeReadFloat(localPlayerAddr + 0x25C, &z);
        outPos->x = x; outPos->y = y; outPos->z = z;
        
        return true;
    }
    
    // Analyze results and detect offsets
    inline void AnalyzeNavGridResults() {
        auto& state = g_navGridScanState;
        auto& detected = state.detected;
        
        // 1. Find ServerPos: Vec3 that changes significantly between IDLE and MOVE
        for (uint64_t off = 0; off < 0x500; off += 4) {
            if (off == 0x1E0 || off == 0x23C) continue; // Skip known offsets
            
            float idleX = *(float*)&state.idleData[off];
            float idleY = *(float*)&state.idleData[off + 4];
            float idleZ = *(float*)&state.idleData[off + 8];
            
            float moveX = *(float*)&state.moveData[off];
            float moveY = *(float*)&state.moveData[off + 4];
            float moveZ = *(float*)&state.moveData[off + 8];
            
            // Check if both are valid positions
            bool idleValid = (idleX > 100 && idleX < 16000 && idleZ > 100 && idleZ < 16000);
            bool moveValid = (moveX > 100 && moveX < 16000 && moveZ > 100 && moveZ < 16000);
            
            if (idleValid && moveValid) {
                float dx = idleX - moveX;
                float dz = idleZ - moveZ;
                float dist = sqrtf(dx*dx + dz*dz);
                
                if (dist > 100.0f && !detected.serverPosConfirmed) {
                    detected.serverPos = off;
                    detected.serverPosConfirmed = true;
                }
            }
        }
        
        // 2. Find FacingAngle: Float that's ~PI (3.14159) when idle OR any angle-like value
        for (uint64_t off = 0; off < 0x200; off += 4) {
            float idleF = *(float*)&state.idleData[off];
            float moveF = *(float*)&state.moveData[off];
            
            // PI when idle, changes when moving
            if (fabsf(idleF - 3.14159f) < 0.1f && fabsf(moveF - 3.14159f) > 0.1f) {
                detected.facingAngle = off;
                detected.facingAngleConfirmed = true;
                break;
            }
            // Also check for angle-like values (-PI to PI range) that change
            if (!detected.facingAngleConfirmed && 
                idleF >= -3.2f && idleF <= 3.2f && 
                moveF >= -3.2f && moveF <= 3.2f &&
                fabsf(idleF - moveF) > 0.5f) {
                detected.facingAngle = off;
                detected.facingAngleConfirmed = true;
            }
        }
        
        // 3. Find MovementState: Byte/Int that changes between idle and move
        // Expanded search - look for any consistent change pattern
        for (uint64_t off = 0x100; off < 0x300; off += 1) {
            uint8_t idleB = state.idleData[off];
            uint8_t moveB = state.moveData[off];
            
            // Look for 0<->1, 0<->2, 1<->0, 2<->0 transitions
            if (idleB != moveB && idleB <= 10 && moveB <= 10) {
                // Prefer offsets that look like state flags
                if ((idleB == 0 && moveB > 0) || (idleB > 0 && moveB == 0) ||
                    (idleB == 2 && moveB == 0) || (idleB == 0 && moveB == 2)) {
                    detected.movementState = off;
                    detected.movementStateConfirmed = true;
                    break;
                }
            }
        }
        
        // 4. Find SegmentCount: Small int (1-20) in range 0x300-0x400
        for (uint64_t off = 0x300; off < 0x400; off += 4) {
            int idleI = *(int*)&state.idleData[off];
            int moveI = *(int*)&state.moveData[off];
            
            if (idleI >= 1 && idleI <= 20 && moveI >= 1 && moveI <= 20) {
                detected.segmentCount = off;
                detected.segmentCountConfirmed = true;
                break;
            }
        }
        
        // 5. Find MoveDirection: Normalized Vec3 (length ~1.0) in 0x400-0x600
        for (uint64_t off = 0x400; off < 0x600; off += 4) {
            float x = *(float*)&state.moveData[off];
            float y = *(float*)&state.moveData[off + 4];
            float z = *(float*)&state.moveData[off + 8];
            
            float len = sqrtf(x*x + y*y + z*z);
            if (len > 0.9f && len < 1.1f) {
                detected.moveDirection = off;
                detected.moveDirectionConfirmed = true;
                break;
            }
        }
        
        // 6. PATH HUNTER (from backup method) - Find NavArray by pointer pairs
        // NavArray is stored as (begin, end) pointers where (end-begin) % 12 == 0
        // Each node is a Vec3 (12 bytes), last node should match target position
        uint64_t aiManager = state.aiManagerAddr;
        if (aiManager > 0x10000) {
            // Get target position (EndPath at 0x23C)
            Vector3 targetPos;
            SafeReadFloat(aiManager + 0x23C, &targetPos.x);
            SafeReadFloat(aiManager + 0x240, &targetPos.y);
            SafeReadFloat(aiManager + 0x244, &targetPos.z);
            
            for (uint64_t off = 0; off < 0x500; off += 8) {
                uint64_t beginPtr = 0, endPtr = 0;
                if (SafeReadQWORD(aiManager + off, &beginPtr) && 
                    SafeReadQWORD(aiManager + off + 8, &endPtr)) {
                    
                    // Check valid pointer pair
                    if (beginPtr > 0x10000 && endPtr > beginPtr && (endPtr - beginPtr) % 12 == 0) {
                        int nodes = (int)((endPtr - beginPtr) / 12);
                        if (nodes > 0 && nodes < 100) {
                            // Read last node and compare with target
                            Vector3 lastNode;
                            if (SafeReadFloat(beginPtr + (nodes - 1) * 12, &lastNode.x) &&
                                SafeReadFloat(beginPtr + (nodes - 1) * 12 + 4, &lastNode.y) &&
                                SafeReadFloat(beginPtr + (nodes - 1) * 12 + 8, &lastNode.z)) {
                                
                                float dx = lastNode.x - targetPos.x;
                                float dz = lastNode.z - targetPos.z;
                                float dist = sqrtf(dx*dx + dz*dz);
                                
                                if (dist < 10.0f) {
                                    // Found NavArray!
                                    detected.navArrayBegin = off;
                                    detected.navArrayEnd = off + 8;
                                    detected.navArrayConfirmed = true;
                                    detected.navArrayNodes = nodes;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Write results to todo file
    inline void WriteNavGridResultsToTodo() {
        auto& detected = g_navGridScanState.detected;
        
        // Read current todo file
        std::ifstream inFile("d:\\source\\LOL_Dumper_[unknowncheats.me]_\\New Project\\todo");
        std::string content;
        if (inFile.is_open()) {
            std::stringstream buffer;
            buffer << inFile.rdbuf();
            content = buffer.str();
            inFile.close();
        }
        
        // Generate results section
        std::stringstream results;
        results << "\n## 🔬 NAVGRID GUIDED SCAN RESULTS (" << GetTickCount() << ")\n";
        results << "## ================================================\n";
        results << "# Auto-generated by NavGrid Guided Scan\n\n";
        results << "| Offset | Name | Status | Value |\n";
        results << "|--------|------|--------|-------|\n";
        
        if (detected.serverPosConfirmed) {
            results << "| **0x" << std::hex << detected.serverPos << std::dec << "** | oAiManagerServerPos | ✅ CONFIRMED | Vec3 position |\n";
        }
        if (detected.facingAngleConfirmed) {
            results << "| **0x" << std::hex << detected.facingAngle << std::dec << "** | oAiManagerFacingAngle | ✅ CONFIRMED | Float = PI when idle |\n";
        }
        if (detected.movementStateConfirmed) {
            results << "| **0x" << std::hex << detected.movementState << std::dec << "** | oAiManagerState | ✅ CONFIRMED | Int: changes idle<->move |\n";
        }
        if (detected.segmentCountConfirmed) {
            results << "| **0x" << std::hex << detected.segmentCount << std::dec << "** | oAiManagerSegmentCount | 🔶 CANDIDATE | Int 1-20 |\n";
        }
        if (detected.moveDirectionConfirmed) {
            results << "| **0x" << std::hex << detected.moveDirection << std::dec << "** | oAiManagerMoveDirection | 🔶 CANDIDATE | Normalized Vec3 |\n";
        }
        if (detected.navArrayConfirmed) {
            results << "| **0x" << std::hex << detected.navArrayBegin << std::dec << "** | oAiManagerNavBegin | ✅ CONFIRMED | Ptr to Vec3 array |\n";
            results << "| **0x" << std::hex << detected.navArrayEnd << std::dec << "** | oAiManagerNavEnd | ✅ CONFIRMED | Ptr to array end |\n";
        }
        
        results << "\n### Summary:\n";
        results << "- ServerPos: " << (detected.serverPosConfirmed ? "FOUND" : "NOT FOUND") << "\n";
        results << "- FacingAngle: " << (detected.facingAngleConfirmed ? "FOUND" : "NOT FOUND") << "\n";
        results << "- MovementState: " << (detected.movementStateConfirmed ? "FOUND" : "NOT FOUND") << "\n";
        results << "- SegmentCount: " << (detected.segmentCountConfirmed ? "FOUND" : "NOT FOUND") << "\n";
        results << "- MoveDirection: " << (detected.moveDirectionConfirmed ? "FOUND" : "NOT FOUND") << "\n";
        results << "- NavArray: " << (detected.navArrayConfirmed ? "FOUND" : "NOT FOUND");
        if (detected.navArrayConfirmed) {
            results << " (" << detected.navArrayNodes << " nodes)";
        }
        results << "\n\n";
        
        // Find position to insert (after existing NAVGRID section or at end of offset table)
        size_t insertPos = content.find("## 🔬 NAVGRID GUIDED SCAN RESULTS");
        if (insertPos != std::string::npos) {
            // Replace existing results section
            size_t endPos = content.find("\n## ", insertPos + 10);
            if (endPos == std::string::npos) endPos = content.length();
            content.replace(insertPos, endPos - insertPos, results.str());
        } else {
            // Insert before "## ❌ CHƯA CÓ" section
            insertPos = content.find("## ❌ CHƯA CÓ");
            if (insertPos != std::string::npos) {
                content.insert(insertPos, results.str());
            } else {
                // Append at end
                content += results.str();
            }
        }
        
        // Write back to todo file
        std::ofstream outFile("d:\\source\\LOL_Dumper_[unknowncheats.me]_\\New Project\\todo");
        if (outFile.is_open()) {
            outFile << content;
            outFile.close();
        }
        
        // Also write detailed log
        std::ofstream logFile("aimanager_navgrid_guided_scan.txt");
        if (logFile.is_open()) {
            logFile << "====================================================================\n";
            logFile << "NAVGRID GUIDED SCAN DETAILED RESULTS\n";
            logFile << "====================================================================\n\n";
            logFile << results.str();
            logFile << "\n=== RAW COMPARISON DATA ===\n";
            logFile << "IDLE Pos: (" << g_navGridScanState.idlePos.x << ", " << g_navGridScanState.idlePos.y << ", " << g_navGridScanState.idlePos.z << ")\n";
            logFile << "MOVE Pos: (" << g_navGridScanState.movePos.x << ", " << g_navGridScanState.movePos.y << ", " << g_navGridScanState.movePos.z << ")\n";
            logFile << "DASH Pos: (" << g_navGridScanState.dashPos.x << ", " << g_navGridScanState.dashPos.y << ", " << g_navGridScanState.dashPos.z << ")\n";
            logFile << "Max Dash Speed: " << g_navGridScanState.maxDashSpeed << " units/sec\n";
            logFile.close();
        }
    }
    
    // Update guided scan - call this in render loop
    inline void UpdateNavGridGuidedScan(uint64_t localPlayerAddr) {
        if (!g_navGridScanState.isActive) return;
        
        DWORD elapsed = GetTickCount() - g_navGridScanState.phaseStartTime;
        
        switch (g_navGridScanState.phase) {
            case NavGridScanPhase::CAPTURING_IDLE:
                if (elapsed >= 2000) {
                    CaptureNavGridSnapshot(localPlayerAddr, g_navGridScanState.idleData, &g_navGridScanState.idlePos);
                    g_navGridScanState.phase = NavGridScanPhase::CAPTURING_MOVE;
                    g_navGridScanState.phaseStartTime = GetTickCount();
                }
                break;
                
            case NavGridScanPhase::CAPTURING_MOVE:
                if (elapsed >= 5000) {
                    CaptureNavGridSnapshot(localPlayerAddr, g_navGridScanState.moveData, &g_navGridScanState.movePos);
                    g_navGridScanState.phase = NavGridScanPhase::CAPTURING_DASH;
                    g_navGridScanState.phaseStartTime = GetTickCount();
                }
                break;
                
            case NavGridScanPhase::CAPTURING_DASH:
                {
                    // Detect dash by speed
                    static Vector3 lastPos = {0, 0, 0};
                    static DWORD lastTime = 0;
                    
                    Vector3 currentPos;
                    float x, y, z;
                    SafeReadFloat(localPlayerAddr + 0x254, &x);
                    SafeReadFloat(localPlayerAddr + 0x258, &y);
                    SafeReadFloat(localPlayerAddr + 0x25C, &z);
                    currentPos.x = x; currentPos.y = y; currentPos.z = z;
                    
                    DWORD now = GetTickCount();
                    if (now - lastTime > 50 && lastTime > 0) {
                        float dx = currentPos.x - lastPos.x;
                        float dz = currentPos.z - lastPos.z;
                        float dist = sqrtf(dx*dx + dz*dz);
                        float speed = dist * (1000.0f / (now - lastTime));
                        
                        if (speed > g_navGridScanState.maxDashSpeed) {
                            g_navGridScanState.maxDashSpeed = speed;
                        }
                        
                        if (speed > 500 && !g_navGridScanState.dashDetected) {
                            CaptureNavGridSnapshot(localPlayerAddr, g_navGridScanState.dashData, &g_navGridScanState.dashPos);
                            g_navGridScanState.dashDetected = true;
                        }
                    }
                    
                    lastPos = currentPos;
                    lastTime = now;
                    
                    if (elapsed >= 5000) {
                        if (!g_navGridScanState.dashDetected) {
                            // No dash detected, use move data as fallback
                            memcpy(g_navGridScanState.dashData, g_navGridScanState.moveData, 0x600);
                            g_navGridScanState.dashPos = g_navGridScanState.movePos;
                        }
                        g_navGridScanState.phase = NavGridScanPhase::ANALYZING;
                        g_navGridScanState.phaseStartTime = GetTickCount();
                    }
                }
                break;
                
            case NavGridScanPhase::ANALYZING:
                AnalyzeNavGridResults();
                WriteNavGridResultsToTodo();
                g_navGridScanState.phase = NavGridScanPhase::COMPLETE;
                g_navGridScanState.isActive = false;
                break;
                
            case NavGridScanPhase::COMPLETE:
                g_navGridScanState.isActive = false;
                break;
                
            default:
                break;
        }
    }
}
