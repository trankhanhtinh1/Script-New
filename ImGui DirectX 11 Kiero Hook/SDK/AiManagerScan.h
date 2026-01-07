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
// League Obfuscation Decryption - EXACT REPRODUCTION OF sub_289E40
// Updated for 15.24
// ============================================================================

namespace IDA
{
    inline uint64_t DecryptAiManager(uintptr_t obfStructAddr)
    {
        __try {
            uint8_t* v11 = (uint8_t*)obfStructAddr;
            
            // IDA: v11[43] is valueIndex
            uint8_t valueIndex = v11[43];
            if (valueIndex > 3) return 0;

            // IDA: v14 = *(_QWORD *)&v11[8 * v11[43] + 8]
            uint64_t encryptedPtr = *(uint64_t*)(v11 + (8 * valueIndex) + 8);
            
            // IDA: retaddr = v14
            uint64_t retaddr = encryptedPtr;
            
            // IDA: if ( v11[41] ) { ... LABEL_8: (&retaddr)[v12] = ~v11[8*v12] ^ retaddr[v12] }
            uint8_t xorCount64 = v11[41];
            if (xorCount64 > 0) {
                // Decrypt the pointer itself (v12 = 0)
                retaddr = (~*(uint64_t*)v11) ^ retaddr;
            }

            // IDA: if ( v11[42] ) { v21 = 8 - v11[42] ... pRet[v21] ^= ~v11[v21] }
            uint8_t xorCount8 = v11[42];
            if (xorCount8 > 0) {
                uint64_t v21 = 8 - xorCount8;
                uint8_t* pRet = (uint8_t*)&retaddr;
                for (; v21 < 8; v21++) {
                    pRet[v21] ^= ~v11[v21];
                }
            }

            // IDA: return retaddr[2] (which is retaddr + 0x10)
            if (retaddr > 0x100000 && retaddr < 0x7FFFFFFFFFFF) {
                // Validate retaddr is readable
                return *(uint64_t*)(retaddr + 0x10);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
        
        return 0;
    }
}

// ============================================================================
// AiManager Offset Scanner
// Based on leagueoflegends-master structure:
//   oObjAiManager = 0x36F0 (Object -> LeagueObfuscation<ptr>)
//   After decrypt + 0x10 = AiManager*
//   
// AiManager structure offsets:
//   oObjAiManagerManagerTargetPosition = 0x14
//   oObjAiManagerManagerIsMoving = 0x2BC
//   oObjAiManagerManagerCurrentSegment = 0x2C0
//   oObjAiManagerManagerPathStart = 0x2D0
//   oObjAiManagerManagerPathEnd = 0x2DC
//   oObjAiManagerManagerSegments = 0x2E8
//   oObjAiManagerManagerSegmentsCount = 0x2F0
//   oObjAiManagerManagerDashSpeed = 0x300
//   oObjAiManagerManagerIsDashing = 0x324
//   oObjAiManagerManagerPosition = 0x414
// ============================================================================

namespace AiManagerScan
{
    struct OffsetCandidate {
        uint64_t offset;
        const char* name;
    };

    inline constexpr OffsetCandidate g_vec3Candidates[] = {
        {0x14, "TargetPosition_0x14"},
        {0x2D0, "PathStart_0x2D0"},
        {0x2DC, "PathEnd_0x2DC"},
        {0x414, "ServerPos_0x414"},
        {0x1E0, "StartPath_0x1E0"},
        {0x23C, "EndPath_0x23C"},
        {0x20, "Vec3_0x20"},
        {0x2C, "Vec3_0x2C"},
        {0x38, "Vec3_0x38"},
        {0x44, "Vec3_0x44"},
        {0x50, "Vec3_0x50"},
        {0x1EC, "Vec3_0x1EC"},
        {0x1F8, "Vec3_0x1F8"},
        {0x204, "Vec3_0x204"},
        {0x220, "Vec3_0x220"},
        {0x248, "Vec3_0x248"},
        {0x254, "Vec3_0x254"},
        {0x260, "Vec3_0x260"},
        {0x400, "Vec3_0x400"},
        {0x420, "Vec3_0x420"},
        {0x42C, "Vec3_0x42C"},
        {0x438, "Vec3_0x438"},
        {0x444, "Vec3_0x444"},
        {0x450, "Vec3_0x450"},
        {0x45C, "Vec3_0x45C"},
        {0x468, "Vec3_0x468"},
        {0x474, "Vec3_0x474"},
        {0x480, "Vec3_0x480"},
        {0x48C, "Vec3_0x48C"},
    };
    inline constexpr size_t g_vec3CandidatesCount = sizeof(g_vec3Candidates) / sizeof(g_vec3Candidates[0]);

    inline constexpr OffsetCandidate g_boolCandidates[] = {
        {0x2BC, "IsMoving_0x2BC"},
        {0x324, "IsDashing_0x324"},
        {0x1B8, "State_0x1B8"},
        {0x214, "Bool_0x214"},
        {0x21C, "Bool_0x21C"},
        {0x2B0, "Bool_0x2B0"},
        {0x2B8, "Bool_0x2B8"},
        {0x2BD, "Bool_0x2BD"},
        {0x2BE, "Bool_0x2BE"},
        {0x2BF, "Bool_0x2BF"},
        {0x320, "Bool_0x320"},
        {0x321, "Bool_0x321"},
        {0x322, "Bool_0x322"},
        {0x323, "Bool_0x323"},
        {0x325, "Bool_0x325"},
    };
    inline constexpr size_t g_boolCandidatesCount = sizeof(g_boolCandidates) / sizeof(g_boolCandidates[0]);

    inline constexpr OffsetCandidate g_intCandidates[] = {
        {0x2C0, "CurrentSegment_0x2C0"},
        {0x2F0, "SegmentsCount_0x2F0"},
        {0x210, "Segments_0x210"},
        {0x268, "HasPath_0x268"},
    };
    inline constexpr size_t g_intCandidatesCount = sizeof(g_intCandidates) / sizeof(g_intCandidates[0]);

    inline constexpr OffsetCandidate g_floatCandidates[] = {
        {0x300, "DashSpeed_0x300"},
        {0x304, "Float_0x304"},
        {0x308, "Float_0x308"},
    };
    inline constexpr size_t g_floatCandidatesCount = sizeof(g_floatCandidates) / sizeof(g_floatCandidates[0]);

    inline constexpr OffsetCandidate g_ptrCandidates[] = {
        {0x2E8, "SegmentsPtr_0x2E8"},
        {0x348, "NavArray_0x348"},
    };
    inline constexpr size_t g_ptrCandidatesCount = sizeof(g_ptrCandidates) / sizeof(g_ptrCandidates[0]);
    
    // ============================================================================
    // SAFE MEMORY READ HELPERS (SEH-compatible, no C++ objects)
    // ============================================================================
    
    inline bool SafeReadByte(uint64_t addr, uint8_t* out) {
        __try {
            *out = *(uint8_t*)addr;
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
    
    inline bool SafeReadInt(uint64_t addr, int* out) {
        __try {
            *out = *(int*)addr;
            return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    
    // Read safe Vector3 from address (uses raw pointers, no C++ objects)
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
    
    inline bool TryReadBool(uint64_t addr, bool& out) {
        uint8_t val;
        if (SafeReadByte(addr, &val)) {
            out = (val != 0);
            return true;
        }
        return false;
    }
    
    inline bool TryReadInt(uint64_t addr, int& out) {
        return SafeReadInt(addr, &out);
    }
    
    inline bool TryReadFloat(uint64_t addr, float& out) {
        return SafeReadFloat(addr, &out);
    }
    
    inline bool TryReadPtr(uint64_t addr, uint64_t& out) {
        uint64_t val;
        if (SafeReadQWORD(addr, &val)) {
            if (val > 0x10000 && val < 0x7FFFFFFFFFFF) {
                out = val;
                return true;
            }
        }
        return false;
    }
    
    // ============================================================================
    // SCAN HELPER (No __try, uses safe read functions)
    // ============================================================================
    
    inline void DumpObfuscationBytes(std::ofstream& file, uint64_t obfAddr) {
        file << "\n=== OBFUSCATION STRUCTURE RAW BYTES ===\n";
        for (int i = 0; i < 64; i += 8) {
            file << std::hex << std::setw(4) << std::setfill('0') << i << ": ";
            for (int j = 0; j < 8; j++) {
                uint8_t b = 0;
                SafeReadByte(obfAddr + i + j, &b);
                file << std::setw(2) << std::setfill('0') << (int)b << " ";
            }
            uint64_t qw = 0;
            SafeReadQWORD(obfAddr + i, &qw);
            file << "  |  QWORD: " << std::setw(16) << std::setfill('0') << qw;
            file << std::dec << "\n";
        }
    }
    
    inline void DumpObfuscationFields(std::ofstream& file, uint64_t obfAddr) {
        uint8_t v41 = 0, v42 = 0, v43 = 0;
        uint64_t xorKey = 0, vt0 = 0, vt1 = 0, vt2 = 0, vt3 = 0;
        
        SafeReadByte(obfAddr + 41, &v41);
        SafeReadByte(obfAddr + 42, &v42);
        SafeReadByte(obfAddr + 43, &v43);
        SafeReadQWORD(obfAddr, &xorKey);
        SafeReadQWORD(obfAddr + 8, &vt0);
        SafeReadQWORD(obfAddr + 16, &vt1);
        SafeReadQWORD(obfAddr + 24, &vt2);
        SafeReadQWORD(obfAddr + 32, &vt3);
        
        file << "\n=== OBFUSCATION STRUCTURE FIELDS ===\n";
        file << "v11[41] (xorCount64): " << (int)v41 << "\n";
        file << "v11[42] (xorCount8): " << (int)v42 << "\n";
        file << "v11[43] (valueIndex): " << (int)v43 << "\n";
        file << "XorKey (first 8 bytes): " << std::hex << xorKey << std::dec << "\n";
        file << "ValueTable[0] (offset 8): " << std::hex << vt0 << std::dec << "\n";
        file << "ValueTable[1] (offset 16): " << std::hex << vt1 << std::dec << "\n";
        file << "ValueTable[2] (offset 24): " << std::hex << vt2 << std::dec << "\n";
        file << "ValueTable[3] (offset 32): " << std::hex << vt3 << std::dec << "\n";
    }
    
    // Main scan function - call this with LocalPlayer address
    inline void ScanAiManagerOffsets(uint64_t localPlayerAddr, const char* playerPosX, const char* playerPosY, const char* playerPosZ) {
        std::ofstream file("aimanager_scan.txt");
        if (!file.is_open()) return;
        
        file << "====================================================================\n";
        file << "AI MANAGER OFFSET SCAN - 15.24\n";
        file << "====================================================================\n";
        file << "LocalPlayer: " << std::hex << localPlayerAddr << std::dec << "\n";
        file << "Current Position (from GameObject): " << playerPosX << ", " << playerPosY << ", " << playerPosZ<< "\n";
        file << "Time: " << GetTickCount() << "\n";
        file << "====================================================================\n\n";
        
        // =========================================================================
        // DIRECT POINTER METHOD (0x3108) - THIS WORKS!
        // =========================================================================
        file << "=== USING DIRECT POINTER METHOD (0x3108) ===\n";
        file << "(Obfuscation at 0x36F0 is empty/unused for this version)\n\n";
        
        uint64_t aiManager = 0;
        if (!SafeReadQWORD(localPlayerAddr + 0x3108, &aiManager)) {
            file << "ERROR: Cannot read AiManager at 0x3108\n";
            file.close();
            return;
        }
        
        file << "AiManager Address: " << std::hex << aiManager << std::dec << "\n\n";
        
        if (aiManager < 0x10000 || aiManager > 0x7FFFFFFFFFFF) {
            file << "ERROR: Invalid AiManager pointer!\n";
            file.close();
            return;
        }
        
        // =========================================================================
        // VERIFIED OFFSETS (from current SDK)
        // =========================================================================
        file << "=== VERIFIED OFFSETS (Current SDK) ===\n";
        
        Vector3 v;
        if (TryReadVec3(aiManager + 0x1E0, v)) {
            file << "[VERIFIED] oAiManagerStartPath (0x1E0): " << v.x << ", " << v.y << ", " << v.z << "\n";
        }
        if (TryReadVec3(aiManager + 0x23C, v)) {
            file << "[VERIFIED] oAiManagerEndPath (0x23C): " << v.x << ", " << v.y << ", " << v.z << "\n";
        }
        
        // =========================================================================
        // LEAGUEOFLEGENDS-MASTER OFFSETS (need verification)
        // These are relative to AiManager base (after +0x10 dereference in their code)
        // But our direct pointer doesn't need the +0x10, so offsets may differ
        // =========================================================================
        file << "\n=== LEAGUEOFLEGENDS-MASTER OFFSETS (Checking...) ===\n";
        file << "Note: Their offsets assume AiManager from obfuscation+0x10\n";
        file << "Our offsets are from Direct Pointer (0x3108)\n\n";
        
        // Their TargetPosition = 0x14
        if (TryReadVec3(aiManager + 0x14, v)) {
            file << "[lol-master] TargetPosition (0x14): " << v.x << ", " << v.y << ", " << v.z;
            if (v.x > 0 && v.x < 20000) file << " <- VALID POS";
            file << "\n";
        }
        
        // Their IsMoving = 0x2BC
        bool bVal;
        if (TryReadBool(aiManager + 0x2BC, bVal)) {
            file << "[lol-master] IsMoving (0x2BC): " << (bVal ? "TRUE" : "FALSE") << "\n";
        }
        
        // Their CurrentSegment = 0x2C0
        int iVal;
        if (TryReadInt(aiManager + 0x2C0, iVal)) {
            file << "[lol-master] CurrentSegment (0x2C0): " << iVal << "\n";
        }
        
        // Their PathStart = 0x2D0
        if (TryReadVec3(aiManager + 0x2D0, v)) {
            file << "[lol-master] PathStart (0x2D0): " << v.x << ", " << v.y << ", " << v.z;
            if (v.x > 0 && v.x < 20000) file << " <- VALID POS";
            file << "\n";
        }
        
        // Their PathEnd = 0x2DC
        if (TryReadVec3(aiManager + 0x2DC, v)) {
            file << "[lol-master] PathEnd (0x2DC): " << v.x << ", " << v.y << ", " << v.z;
            if (v.x > 0 && v.x < 20000) file << " <- VALID POS";
            file << "\n";
        }
        
        // Their Segments pointer = 0x2E8
        uint64_t ptr;
        if (TryReadPtr(aiManager + 0x2E8, ptr)) {
            file << "[lol-master] Segments PTR (0x2E8): " << std::hex << ptr << std::dec << "\n";
        }
        
        // Their SegmentsCount = 0x2F0
        if (TryReadInt(aiManager + 0x2F0, iVal)) {
            file << "[lol-master] SegmentsCount (0x2F0): " << iVal << "\n";
        }
        
        // Their DashSpeed = 0x300
        float fVal;
        if (TryReadFloat(aiManager + 0x300, fVal)) {
            file << "[lol-master] DashSpeed (0x300): " << fVal << "\n";
        }
        
        // Their IsDashing = 0x324
        if (TryReadBool(aiManager + 0x324, bVal)) {
            file << "[lol-master] IsDashing (0x324): " << (bVal ? "TRUE" : "FALSE") << "\n";
        }
        
        // Their ServerPosition = 0x414
        if (TryReadVec3(aiManager + 0x414, v)) {
            file << "[lol-master] ServerPos (0x414): " << v.x << ", " << v.y << ", " << v.z;
            if (v.x > 0 && v.x < 20000) file << " <- VALID POS";
            file << "\n";
        }
        
        // =========================================================================
        // COMPREHENSIVE VEC3 SCAN (Find all valid positions)
        // =========================================================================
        file << "\n=== ALL VALID VEC3 POSITIONS (0x00 - 0x500) ===\n";
        file << "Showing only coordinates that look like map positions\n\n";
        
        for (uint64_t off = 0; off <= 0x500; off += 0x04) {
            if (TryReadVec3(aiManager + off, v)) {
                // Check if this looks like a valid LoL position
                if (v.x > 100 && v.x < 16000 && v.z > 100 && v.z < 16000 && v.y > -500 && v.y < 500) {
                    file << "0x" << std::hex << off << std::dec << ": (" << v.x << ", " << v.y << ", " << v.z << ")";
                    
                    // Annotate known offsets
                    if (off == 0x1E0) file << " <- oAiManagerStartPath [VERIFIED]";
                    else if (off == 0x23C) file << " <- oAiManagerEndPath [VERIFIED]";
                    else if (off == 0x2D0) file << " <- PathStart? (lol-master)";
                    else if (off == 0x2DC) file << " <- PathEnd? (lol-master)";
                    else if (off == 0x414) file << " <- ServerPos? (lol-master)";
                    
                    file << "\n";
                }
            }
        }
        
        // =========================================================================
        // BOOL FLAGS SCAN (Find IsMoving/IsDashing candidates)
        // =========================================================================
        file << "\n=== BOOL FLAGS (0x200 - 0x400) ===\n";
        file << "Values that are 0 or 1 - candidates for IsMoving/IsDashing\n\n";
        
        for (uint64_t off = 0x200; off <= 0x400; off += 0x01) {
            uint8_t byteVal = 0;
            if (SafeReadByte(aiManager + off, &byteVal)) {
                if (byteVal == 0 || byteVal == 1) {
                    file << "0x" << std::hex << off << std::dec << ": " << (int)byteVal;
                    if (off == 0x2BC) file << " <- IsMoving? (lol-master)";
                    if (off == 0x324) file << " <- IsDashing? (lol-master)";
                    file << "\n";
                }
            }
        }
        
        // =========================================================================
        // SEGMENT COUNT SCAN (Small ints 1-20)
        // =========================================================================
        file << "\n=== SEGMENT COUNTS (0x200 - 0x400) ===\n";
        file << "Values 1-20 - candidates for SegmentsCount/CurrentSegment\n\n";
        
        for (uint64_t off = 0x200; off <= 0x400; off += 0x04) {
            if (TryReadInt(aiManager + off, iVal)) {
                if (iVal >= 1 && iVal <= 20) {
                    file << "0x" << std::hex << off << std::dec << ": " << iVal;
                    if (off == 0x2C0) file << " <- CurrentSegment? (lol-master)";
                    if (off == 0x2F0) file << " <- SegmentsCount? (lol-master)";
                    file << "\n";
                }
            }
        }
        
        // =========================================================================
        // FLOAT SCAN (Speed values 100-1000)
        // =========================================================================
        file << "\n=== SPEED VALUES (0x200 - 0x500) ===\n";
        file << "Float values 100-1000 - candidates for DashSpeed/MoveSpeed\n\n";
        
        for (uint64_t off = 0x200; off <= 0x500; off += 0x04) {
            if (TryReadFloat(aiManager + off, fVal)) {
                if (fVal > 100.0f && fVal < 1000.0f) {
                    file << "0x" << std::hex << off << std::dec << ": " << fVal;
                    if (off == 0x300) file << " <- DashSpeed? (lol-master)";
                    file << "\n";
                }
            }
        }
        
        file << "\n====================================================================\n";
        file << "SCAN COMPLETE\n";
        file << "====================================================================\n";
        file << "\nNEXT STEPS:\n";
        file << "1. MOVE your character and scan again to see which offsets change\n";
        file << "2. DASH (Flash, E, etc) and scan while dashing to find IsDashing\n";
        file << "3. Compare StartPath vs EndPath when moving - they should differ\n";
        file.close();
    }
    // ============================================================================
    // GUIDED SCAN - 3 Phase capture: IDLE → MOVING → DASH
    // ============================================================================
    
    enum class ScanPhase {
        IDLE,           // Waiting for button click
        CAPTURING_IDLE, // 2 seconds to stand still
        CAPTURING_MOVE, // 5 seconds to move around (zigzag)
        CAPTURING_PAUSE,// 3 seconds pause before dash (NEW!)
        CAPTURING_DASH, // 5 seconds to dash (Zeri E through wall, etc.)
        COMPLETE        // Done, writing results
    };
    
    struct ScanState {
        ScanPhase phase = ScanPhase::IDLE;
        DWORD phaseStartTime = 0;
        
        // Memory snapshots (0x600 bytes from AiManager)
        uint8_t idleData[0x600] = {0};
        uint8_t moveData[0x600] = {0};
        uint8_t pauseData[0x600] = {0};  // NEW: Before dash (standing still)
        uint8_t dashData[0x600] = {0};
        
        // Positions at each phase
        Vector3 idlePos = {0, 0, 0};
        Vector3 movePos = {0, 0, 0};
        Vector3 pausePos = {0, 0, 0};  // NEW
        Vector3 dashPos = {0, 0, 0};
        
        // Additional move data (multiple captures)
        Vector3 moveEndPath[10] = {{0,0,0}};
        int moveCaptures = 0;
        
        // Multiple dash captures (to catch fast dashes like Lucian E)
        uint8_t dashSnapshots[5][0x600] = {{0}};
        int dashCaptureCount = 0;
        bool dashDetected = false;
        float maxDashSpeed = 0.0f;
        
        bool isActive = false;
    };
    
    inline ScanState g_scanState;
    
    // Forward declaration
    inline void WriteGuidedScanResults();
    
    // Get status message for ImGui display
    inline const char* GetScanStatusMessage() {
        switch (g_scanState.phase) {
            case ScanPhase::IDLE: 
                return "Click 'Start Guided Scan' to begin";
            case ScanPhase::CAPTURING_IDLE: {
                DWORD elapsed = GetTickCount() - g_scanState.phaseStartTime;
                DWORD remaining = (elapsed < 2000) ? (2000 - elapsed) / 1000 : 0;
                static char buf[64];
                sprintf_s(buf, sizeof(buf), "[1/4] STAND STILL! Capturing IDLE... %lus", remaining + 1);
                return buf;
            }
            case ScanPhase::CAPTURING_MOVE: {
                DWORD elapsed = GetTickCount() - g_scanState.phaseStartTime;
                DWORD remaining = (elapsed < 5000) ? (5000 - elapsed) / 1000 : 0;
                static char buf[64];
                sprintf_s(buf, sizeof(buf), "[2/4] MOVE AROUND (zigzag)! %lus remaining", remaining + 1);
                return buf;
            }
            case ScanPhase::CAPTURING_PAUSE: {
                DWORD elapsed = GetTickCount() - g_scanState.phaseStartTime;
                DWORD remaining = (elapsed < 3000) ? (3000 - elapsed) / 1000 : 0;
                static char buf[64];
                sprintf_s(buf, sizeof(buf), "[3/4] STOP! Prepare for dash... %lus", remaining + 1);
                return buf;
            }
            case ScanPhase::CAPTURING_DASH: {
                DWORD elapsed = GetTickCount() - g_scanState.phaseStartTime;
                DWORD remaining = (elapsed < 5000) ? (5000 - elapsed) / 1000 : 0;
                static char buf[128];
                sprintf_s(buf, sizeof(buf), "[4/4] DASH NOW! (Zeri E through wall!) %lus (captures: %d/5)", 
                    remaining + 1, g_scanState.dashCaptureCount);
                return buf;
            }
            case ScanPhase::COMPLETE:
                return "SCAN COMPLETE! Check aimanager_guided_scan.txt";
            default:
                return "";
        }
    }
    
    // Get color for ImGui status (green/yellow/red based on phase)
    inline void GetScanStatusColor(float* r, float* g, float* b) {
        switch (g_scanState.phase) {
            case ScanPhase::IDLE: *r = 0.5f; *g = 0.5f; *b = 0.5f; break;
            case ScanPhase::CAPTURING_IDLE: *r = 0.0f; *g = 1.0f; *b = 0.5f; break;
            case ScanPhase::CAPTURING_MOVE: *r = 1.0f; *g = 1.0f; *b = 0.0f; break;
            case ScanPhase::CAPTURING_PAUSE: *r = 0.5f; *g = 0.5f; *b = 1.0f; break; // Blue for pause
            case ScanPhase::CAPTURING_DASH: *r = 1.0f; *g = 0.3f; *b = 0.3f; break;
            case ScanPhase::COMPLETE: *r = 0.3f; *g = 1.0f; *b = 0.3f; break;
            default: *r = 1.0f; *g = 1.0f; *b = 1.0f; break;
        }
    }
    
    // Start the guided scan
    inline void StartGuidedScan() {
        g_scanState.phase = ScanPhase::CAPTURING_IDLE;
        g_scanState.phaseStartTime = GetTickCount();
        g_scanState.isActive = true;
        g_scanState.moveCaptures = 0;
        g_scanState.dashCaptureCount = 0;
        g_scanState.dashDetected = false;
        g_scanState.maxDashSpeed = 0.0f;
        memset(g_scanState.idleData, 0, sizeof(g_scanState.idleData));
        memset(g_scanState.moveData, 0, sizeof(g_scanState.moveData));
        memset(g_scanState.pauseData, 0, sizeof(g_scanState.pauseData));
        memset(g_scanState.dashData, 0, sizeof(g_scanState.dashData));
        memset(g_scanState.dashSnapshots, 0, sizeof(g_scanState.dashSnapshots));
    }
    
    // Capture current AiManager state to buffer
    inline bool CaptureAiManagerSnapshot(uint64_t localPlayerAddr, uint8_t* outBuffer, Vector3* outPos) {
        uint64_t aiManager = 0;
        if (!SafeReadQWORD(localPlayerAddr + 0x3108, &aiManager)) return false;
        if (aiManager < 0x10000 || aiManager > 0x7FFFFFFFFFFF) return false;
        
        // Read 0x600 bytes
        for (int i = 0; i < 0x600; i++) {
            SafeReadByte(aiManager + i, &outBuffer[i]);
        }
        
        // Read position
        float x, y, z;
        SafeReadFloat(localPlayerAddr + 0x254, &x);
        SafeReadFloat(localPlayerAddr + 0x258, &y);
        SafeReadFloat(localPlayerAddr + 0x25C, &z);
        outPos->x = x; outPos->y = y; outPos->z = z;
        
        return true;
    }
    
    // Update guided scan - call this in render loop
    inline void UpdateGuidedScan(uint64_t localPlayerAddr) {
        if (!g_scanState.isActive) return;
        
        DWORD elapsed = GetTickCount() - g_scanState.phaseStartTime;
        
        switch (g_scanState.phase) {
            case ScanPhase::CAPTURING_IDLE:
                // After 2 seconds, capture IDLE state
                if (elapsed >= 2000) {
                    CaptureAiManagerSnapshot(localPlayerAddr, g_scanState.idleData, &g_scanState.idlePos);
                    g_scanState.phase = ScanPhase::CAPTURING_MOVE;
                    g_scanState.phaseStartTime = GetTickCount();
                }
                break;
                
            case ScanPhase::CAPTURING_MOVE:
                // During 5 seconds, capture multiple times
                if (elapsed < 5000) {
                    // Capture every 500ms
                    static DWORD lastCapture = 0;
                    if (GetTickCount() - lastCapture > 500 && g_scanState.moveCaptures < 10) {
                        CaptureAiManagerSnapshot(localPlayerAddr, g_scanState.moveData, &g_scanState.movePos);
                        
                        // Also capture EndPath for path tracking
                        uint64_t aiManager = 0;
                        if (SafeReadQWORD(localPlayerAddr + 0x3108, &aiManager)) {
                            TryReadVec3(aiManager + 0x23C, g_scanState.moveEndPath[g_scanState.moveCaptures]);
                        }
                        g_scanState.moveCaptures++;
                        lastCapture = GetTickCount();
                    }
                } else {
                    // Final capture of MOVING state, then go to PAUSE
                    CaptureAiManagerSnapshot(localPlayerAddr, g_scanState.moveData, &g_scanState.movePos);
                    g_scanState.phase = ScanPhase::CAPTURING_PAUSE;
                    g_scanState.phaseStartTime = GetTickCount();
                }
                break;
                
            case ScanPhase::CAPTURING_PAUSE:
                // 3 seconds pause - user should STOP and prepare for dash
                if (elapsed >= 3000) {
                    // Capture PAUSE state (standing still before dash)
                    CaptureAiManagerSnapshot(localPlayerAddr, g_scanState.pauseData, &g_scanState.pausePos);
                    g_scanState.phase = ScanPhase::CAPTURING_DASH;
                    g_scanState.phaseStartTime = GetTickCount();
                }
                break;
                
            case ScanPhase::CAPTURING_DASH:
                // Dash phase runs for 5 SECONDS - user can dash multiple times
                {
                    uint64_t aiManager = 0;
                    if (SafeReadQWORD(localPlayerAddr + 0x3108, &aiManager)) {
                        // Check if position changed rapidly (dash indicator)
                        Vector3 currentPos;
                        float x, y, z;
                        SafeReadFloat(localPlayerAddr + 0x254, &x);
                        SafeReadFloat(localPlayerAddr + 0x258, &y);
                        SafeReadFloat(localPlayerAddr + 0x25C, &z);
                        currentPos.x = x; currentPos.y = y; currentPos.z = z;
                        
                        // Detect dash: rapid position change
                        static Vector3 lastDashCheckPos = {0, 0, 0};
                        static DWORD lastDashCheck = 0;
                        
                        // Poll every 50ms for dash detection
                        DWORD now = GetTickCount();
                        if (now - lastDashCheck > 50) {
                            DWORD deltaMs = now - lastDashCheck;
                            if (deltaMs > 0 && lastDashCheck > 0) {
                                float dx = currentPos.x - lastDashCheckPos.x;
                                float dz = currentPos.z - lastDashCheckPos.z;
                                float distance = sqrtf(dx*dx + dz*dz);
                                float speed = distance * (1000.0f / deltaMs); // units per second
                                
                                // Track max dash speed seen
                                if (speed > g_scanState.maxDashSpeed) {
                                    g_scanState.maxDashSpeed = speed;
                                }
                                
                                // Detect dash (speed > 500 units/sec)
                                if (speed > 500) {
                                    g_scanState.dashDetected = true;
                                    
                                    // Capture snapshot (max 5)
                                    if (g_scanState.dashCaptureCount < 5) {
                                        CaptureAiManagerSnapshot(localPlayerAddr, 
                                            g_scanState.dashSnapshots[g_scanState.dashCaptureCount], 
                                            &g_scanState.dashPos);
                                        
                                        // Also copy to main dashData
                                        memcpy(g_scanState.dashData, 
                                            g_scanState.dashSnapshots[g_scanState.dashCaptureCount], 
                                            0x600);
                                        
                                        g_scanState.dashCaptureCount++;
                                    }
                                }
                            }
                            
                            lastDashCheckPos = currentPos;
                            lastDashCheck = now;
                        }
                    }
                    
                    // Dash phase lasts exactly 5 seconds
                    if (elapsed >= 5000) {
                        g_scanState.phase = ScanPhase::COMPLETE;
                        WriteGuidedScanResults();
                        g_scanState.isActive = false; // Reset here too
                    }
                }
                break;
                
            case ScanPhase::COMPLETE:
                g_scanState.isActive = false;
                break;
                
            default:
                break;
        }
    }
    
    // Write comparison results to file
    inline void WriteGuidedScanResults() {
        std::ofstream file("aimanager_guided_scan.txt");
        if (!file.is_open()) return;
        
        file << "====================================================================\n";
        file << "GUIDED AI MANAGER SCAN - IDLE vs MOVE vs PAUSE vs DASH\n";
        file << "====================================================================\n\n";
        
        file << "=== SCAN INFO ===\n";
        file << "Dash Detected: " << (g_scanState.dashDetected ? "YES" : "NO") << "\n";
        file << "Dash Captures: " << g_scanState.dashCaptureCount << " snapshots\n";
        file << "Max Dash Speed Detected: " << g_scanState.maxDashSpeed << " units/sec\n\n";
        
        file << "=== POSITIONS ===\n";
        file << "IDLE:  (" << g_scanState.idlePos.x << ", " << g_scanState.idlePos.y << ", " << g_scanState.idlePos.z << ")\n";
        file << "MOVE:  (" << g_scanState.movePos.x << ", " << g_scanState.movePos.y << ", " << g_scanState.movePos.z << ")\n";
        file << "PAUSE: (" << g_scanState.pausePos.x << ", " << g_scanState.pausePos.y << ", " << g_scanState.pausePos.z << ")\n";
        file << "DASH:  (" << g_scanState.dashPos.x << ", " << g_scanState.dashPos.y << ", " << g_scanState.dashPos.z << ")\n\n";
        
        file << "=== PATH TRACKING (EndPath during movement) ===\n";
        for (int i = 0; i < g_scanState.moveCaptures; i++) {
            auto& p = g_scanState.moveEndPath[i];
            file << "  [" << i << "] (" << p.x << ", " << p.y << ", " << p.z << ")\n";
        }
        file << "\n";
        
        // =========================================================================
        // COMPARE IDLE vs MOVE - Find IsMoving candidates (FULL RANGE 0x00-0x600)
        // =========================================================================
        file << "====================================================================\n";
        file << "BOOL CHANGES: IDLE -> MOVING (IsMoving candidates) [0x00-0x600]\n";
        file << "====================================================================\n";
        
        int boolChangeCount = 0;
        for (int i = 0; i < 0x600; i++) {
            uint8_t idle = g_scanState.idleData[i];
            uint8_t move = g_scanState.moveData[i];
            
            // Look for 0->1 transition (IsMoving)
            if ((idle == 0 && move == 1) || (idle == 1 && move == 0)) {
                file << "0x" << std::hex << i << std::dec << ": IDLE=" << (int)idle << " -> MOVE=" << (int)move;
                if (idle == 0 && move == 1) file << " <-- LIKELY IsMoving!";
                file << "\n";
                boolChangeCount++;
            }
        }
        if (boolChangeCount == 0) file << "(No bool changes found in 0x00-0x600)\n";
        file << "\n";
        
        // =========================================================================
        // COMPARE PAUSE vs DASH - Find IsDashing candidates (PAUSE = still, DASH = dashing)
        // =========================================================================
        file << "====================================================================\n";
        file << "BOOL CHANGES: PAUSE -> DASH (IsDashing candidates) [0x00-0x600]\n";
        file << "====================================================================\n";
        
        int dashBoolCount = 0;
        for (int i = 0; i < 0x600; i++) {
            uint8_t pause = g_scanState.pauseData[i];
            uint8_t dash = g_scanState.dashData[i];
            
            if ((pause == 0 && dash == 1) || (pause == 1 && dash == 0)) {
                file << "0x" << std::hex << i << std::dec << ": PAUSE=" << (int)pause << " -> DASH=" << (int)dash;
                if (pause == 0 && dash == 1) file << " <-- LIKELY IsDashing!";
                file << "\n";
                dashBoolCount++;
            }
        }
        if (dashBoolCount == 0) file << "(No bool changes found - maybe dash not detected)\n";
        file << "\n";
        
        // =========================================================================
        // FLOAT CHANGES - Find DashSpeed (PAUSE = 0 or low, DASH = high)
        // =========================================================================
        file << "====================================================================\n";
        file << "FLOAT CHANGES: PAUSE -> DASH (DashSpeed candidates) [0x00-0x600]\n";
        file << "====================================================================\n";
        
        for (uint64_t off = 0; off < 0x600; off += 4) {
            float pauseF = *(float*)&g_scanState.pauseData[off];
            float dashF = *(float*)&g_scanState.dashData[off];
            // DashSpeed should be 0/low when paused, > 500 when dashing
            if (dashF > 500.0f && dashF < 3000.0f && pauseF < 100.0f) {
                file << "0x" << std::hex << off << std::dec << ": PAUSE=" << pauseF << " -> DASH=" << dashF;
                file << " <-- LIKELY DashSpeed!\n";
            }
        }
        file << "\n";
        
        // =========================================================================
        // VEC3 CHANGES - Compare StartPath/EndPath
        // =========================================================================
        file << "====================================================================\n";
        file << "VEC3 CHANGES: IDLE -> MOVING (Path offsets)\n";
        file << "====================================================================\n";
        
        for (uint64_t off = 0; off < 0x500; off += 4) {
            float idleX = *(float*)&g_scanState.idleData[off];
            float idleY = *(float*)&g_scanState.idleData[off + 4];
            float idleZ = *(float*)&g_scanState.idleData[off + 8];
            
            float moveX = *(float*)&g_scanState.moveData[off];
            float moveY = *(float*)&g_scanState.moveData[off + 4];
            float moveZ = *(float*)&g_scanState.moveData[off + 8];
            
            // Check if both are valid positions but different
            bool idleValid = (idleX > 100 && idleX < 16000 && idleZ > 100 && idleZ < 16000);
            bool moveValid = (moveX > 100 && moveX < 16000 && moveZ > 100 && moveZ < 16000);
            
            if (idleValid && moveValid) {
                float dx = idleX - moveX;
                float dz = idleZ - moveZ;
                float dist = sqrtf(dx*dx + dz*dz);
                
                if (dist > 50.0f) { // Significant change
                    file << "0x" << std::hex << off << std::dec << ":\n";
                    file << "  IDLE: (" << idleX << ", " << idleY << ", " << idleZ << ")\n";
                    file << "  MOVE: (" << moveX << ", " << moveY << ", " << moveZ << ")\n";
                    file << "  DIFF: " << dist << " units";
                    
                    if (off == 0x1E0) file << " <- StartPath [KNOWN]";
                    else if (off == 0x23C) file << " <- EndPath [KNOWN]";
                    file << "\n\n";
                }
            }
        }
        
        // =========================================================================
        // SEGMENT COUNT / INT CHANGES
        // =========================================================================
        file << "====================================================================\n";
        file << "INT CHANGES: IDLE -> MOVING (SegmentCount candidates)\n";
        file << "====================================================================\n";
        
        for (uint64_t off = 0x200; off < 0x400; off += 4) {
            int idleI = *(int*)&g_scanState.idleData[off];
            int moveI = *(int*)&g_scanState.moveData[off];
            
            // Segment count should go from 0/1 (idle) to 2+ (moving with waypoints)
            if (idleI != moveI && idleI >= 0 && idleI <= 20 && moveI >= 0 && moveI <= 20) {
                file << "0x" << std::hex << off << std::dec << ": IDLE=" << idleI << " -> MOVE=" << moveI;
                if (idleI <= 1 && moveI > 1) file << " <-- LIKELY SegmentCount!";
                file << "\n";
            }
        }
        file << "\n";
        
        // =========================================================================
        // SPEED VALUES
        // =========================================================================
        file << "====================================================================\n";
        file << "SPEED VALUES (0x200 - 0x500)\n";
        file << "====================================================================\n";
        
        for (uint64_t off = 0x200; off < 0x500; off += 4) {
            float idleF = *(float*)&g_scanState.idleData[off];
            float moveF = *(float*)&g_scanState.moveData[off];
            float pauseF = *(float*)&g_scanState.pauseData[off];
            float dashF = *(float*)&g_scanState.dashData[off];
            
            if ((idleF > 100 && idleF < 700) || (dashF > 100 && dashF < 3000)) {
                file << "0x" << std::hex << off << std::dec << ": IDLE=" << idleF << ", MVP=" << moveF << ", PAUSE=" << pauseF << ", DASH=" << dashF;
                
                // Annotate MoveSpeed (consistent ~320-500)
                if (fabsf(idleF - moveF) < 10 && idleF > 200 && idleF < 600) {
                    file << " <- MoveSpeed?";
                }
                // DashSpeed (high only in dash)
                if (dashF > moveF + 200) {
                    file << " <- DashSpeed?";
                }
                file << "\n";
            }
        }
        
        file << "\n====================================================================\n";
        file << "GUIDED SCAN COMPLETE\n";
        file << "====================================================================\n";
        file.close();
        
        // Reset state
        g_scanState.phase = ScanPhase::IDLE;
    }
    
    // Helper function để đọc IsMoving (tách ra để tránh __try trong hàm có C++ objects)
    inline bool SafeReadIsMoving(uint64_t addr, uint8_t* out) {
        __try {
            *out = *(uint8_t*)addr;
            return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }
    
    // ============================================================================
    // SCAN OBFUSCATED OFFSET - Tìm offset obfuscated structure trực tiếp trong game
    // ============================================================================
    // Cách hoạt động:
    // 1. Thử các offset candidates (0x3108, 0x41A8, 0x4218, 0x36F0, ...)
    // 2. Với mỗi offset, đọc obfuscated structure
    // 3. Dùng hàm DecryptAiManager để giải mã
    // 4. Verify bằng cách đọc StartPath (0x330) và so sánh với GameObject position (0x254)
    // ============================================================================
    
    inline void ScanObfuscatedOffset(uint64_t localPlayerAddr) {
        std::ofstream file("aimanager_offset_scan.txt");
        if (!file.is_open()) return;
        
        file << "====================================================================\n";
        file << "AI MANAGER OFFSET SCAN - Tìm offset obfuscated structure\n";
        file << "====================================================================\n";
        file << "LocalPlayer: " << std::hex << localPlayerAddr << std::dec << "\n";
        file << "Time: " << GetTickCount() << "\n";
        file << "====================================================================\n\n";
        
        // Đọc GameObject position để verify
        Vector3 objPos(0, 0, 0);
        if (!TryReadVec3(localPlayerAddr + 0x254, objPos)) {
            file << "ERROR: Cannot read GameObject position at 0x254\n";
            file.close();
            return;
        }
        file << "GameObject Position: (" << objPos.x << ", " << objPos.y << ", " << objPos.z << ")\n\n";
        
        // Offset candidates để scan
        uint64_t candidates[] = {
            0x3108,  // Offset cũ
            0x41A8,  // Từ partern 15.11 (có thể outdated)
            0x4218,  // Từ hàm decrypt sub_289E40 (a4 + 16920 = 0x4218)
            0x36F0,  // Obfuscated pointer cũ
            0x4000,  // Thử các offset gần 0x4218
            0x4100,
            0x4200,
            0x4300,
        };
        const char* names[] = {
            "0x3108 (old)",
            "0x41A8 (partern 15.11)",
            "0x4218 (from sub_289E40)",
            "0x36F0 (old obfuscated)",
            "0x4000 (near 0x4218)",
            "0x4100 (near 0x4218)",
            "0x4200 (near 0x4218)",
            "0x4300 (near 0x4218)",
        };
        
        file << "=== SCANNING OFFSET CANDIDATES ===\n\n";
        
        for (int i = 0; i < 8; i++) {
            uint64_t offset = candidates[i];
            file << "------------------------------------------------------------\n";
            file << "Testing: " << names[i] << " (offset 0x" << std::hex << offset << std::dec << ")\n";
            file << "------------------------------------------------------------\n";
            
            // Đọc obfuscated structure
            uint64_t obfStructAddr = localPlayerAddr + offset;
            
            // Kiểm tra có thể đọc được không
            uint64_t testQword = 0;
            if (!SafeReadQWORD(obfStructAddr, &testQword)) {
                file << "❌ Cannot read obfuscated structure\n\n";
                continue;
            }
            
            // Thử decrypt
            uint64_t aiManagerPtr = IDA::DecryptAiManager(obfStructAddr);
            
            if (aiManagerPtr == 0 || aiManagerPtr < 0x100000 || aiManagerPtr > 0x7FFFFFFFFFFF) {
                file << "❌ Decrypt failed or invalid pointer: 0x" << std::hex << aiManagerPtr << std::dec << "\n\n";
                continue;
            }
            
            file << "✅ Decrypted AiManager pointer: 0x" << std::hex << aiManagerPtr << std::dec << "\n";
            
            // Verify bằng cách đọc StartPath (0x330)
            Vector3 startPath(0, 0, 0);
            if (!TryReadVec3(aiManagerPtr + 0x330, startPath)) {
                file << "❌ Cannot read StartPath (0x330)\n\n";
                continue;
            }
            
            file << "✅ StartPath (0x330): (" << startPath.x << ", " << startPath.y << ", " << startPath.z << ")\n";
            
            // So sánh với GameObject position
            float dist = objPos.Distance(startPath);
            file << "   Distance to GameObject: " << dist << " units\n";
            
            if (dist < 100.0f) {
                file << "   ✅✅✅ MATCHES GameObject position! OFFSET IS CORRECT!\n";
                file << "\n";
                file << "====================================================================\n";
                file << "FOUND CORRECT OFFSET: 0x" << std::hex << offset << std::dec << "\n";
                file << "====================================================================\n";
            } else {
                file << "   ⚠️  Does NOT match GameObject position (might be wrong offset)\n";
            }
            
            // Thử đọc EndPath (0x33C) để verify thêm
            Vector3 endPath(0, 0, 0);
            if (TryReadVec3(aiManagerPtr + 0x33C, endPath)) {
                file << "✅ EndPath (0x33C): (" << endPath.x << ", " << endPath.y << ", " << endPath.z << ")\n";
                float pathDist = startPath.Distance(endPath);
                file << "   Path distance: " << pathDist << " units\n";
                if (pathDist < 10.0f) {
                    file << "   → Unit is IDLE\n";
                } else {
                    file << "   → Unit is MOVING\n";
                }
            }
            
            // Thử đọc IsMoving (0x31C) - dùng helper function
            uint8_t isMovingByte = 0;
            if (SafeReadIsMoving(aiManagerPtr + 0x31C, &isMovingByte)) {
                file << "✅ IsMoving (0x31C): " << (isMovingByte ? "true" : "false") << "\n";
            } else {
                file << "❌ Cannot read IsMoving (0x31C)\n";
            }
            
            // Thử đọc IsDashing (0x384) - từ partern 15.11
            uint8_t isDashingByte = 0;
            if (SafeReadIsMoving(aiManagerPtr + 0x384, &isDashingByte)) {
                file << "✅ IsDashing (0x384): " << (isDashingByte ? "true" : "false") << "\n";
            } else {
                file << "❌ Cannot read IsDashing (0x384)\n";
            }
            
            // Thử đọc DashSpeed (0x360) - từ partern 15.11
            float dashSpeed = 0.0f;
            if (SafeReadFloat(aiManagerPtr + 0x360, &dashSpeed)) {
                file << "✅ DashSpeed (0x360): " << dashSpeed << "\n";
                if (dashSpeed > 0.0f) {
                    file << "   → Unit is DASHING!\n";
                }
            } else {
                file << "❌ Cannot read DashSpeed (0x360)\n";
            }
            
            // Thử đọc ServerPos (0x474) - từ partern 15.11
            Vector3 serverPos(0, 0, 0);
            if (TryReadVec3(aiManagerPtr + 0x474, serverPos)) {
                file << "✅ ServerPos (0x474): (" << serverPos.x << ", " << serverPos.y << ", " << serverPos.z << ")\n";
                float serverDist = objPos.Distance(serverPos);
                file << "   Distance to GameObject: " << serverDist << " units\n";
            } else {
                file << "❌ Cannot read ServerPos (0x474)\n";
            }
            
            // Thử đọc CurrentSegment (0x320) - từ partern 15.11
            int currentSegment = 0;
            if (SafeReadInt(aiManagerPtr + 0x320, &currentSegment)) {
                file << "✅ CurrentSegment (0x320): " << currentSegment << "\n";
            } else {
                file << "❌ Cannot read CurrentSegment (0x320)\n";
            }
            
            // Thử đọc SegmentsCount (0x350) - từ partern 15.11
            int segmentsCount = 0;
            if (SafeReadInt(aiManagerPtr + 0x350, &segmentsCount)) {
                file << "✅ SegmentsCount (0x350): " << segmentsCount << "\n";
            } else {
                file << "❌ Cannot read SegmentsCount (0x350)\n";
            }
            
            // Thử đọc Velocity (0x318) - từ partern 15.11
            // Velocity có thể là Vec3 hoặc chỉ là float speed
            Vector3 velocity(0, 0, 0);
            bool hasVelocity = false;
            
            // Thử đọc như Vec3
            if (TryReadVec3(aiManagerPtr + 0x318, velocity)) {
                file << "✅ Velocity (0x318): (" << velocity.x << ", " << velocity.y << ", " << velocity.z << ")\n";
                float speed = sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);
                file << "   Speed: " << speed << " units/sec\n";
                hasVelocity = true;
            } else {
                // Thử đọc như float (có thể chỉ là speed, không phải Vec3)
                float speedValue = 0.0f;
                if (SafeReadFloat(aiManagerPtr + 0x318, &speedValue)) {
                    if (speedValue > 0.0f && speedValue < 10000.0f) {
                        file << "✅ Velocity (0x318): " << speedValue << " (float speed, not Vec3)\n";
                        hasVelocity = true;
                    }
                }
                
                if (!hasVelocity) {
                    file << "❌ Cannot read Velocity (0x318) - trying to scan nearby offsets...\n";
                    // Scan nearby offsets để tìm Velocity
                    for (uint64_t offset = 0x310; offset <= 0x330; offset += 4) {
                        Vector3 testVec(0, 0, 0);
                        if (TryReadVec3(aiManagerPtr + offset, testVec)) {
                            float testSpeed = sqrtf(testVec.x * testVec.x + testVec.z * testVec.z);
                            if (testSpeed > 0.1f && testSpeed < 2000.0f) {
                                file << "   → Found potential Velocity at 0x" << std::hex << offset << std::dec 
                                     << ": (" << testVec.x << ", " << testVec.y << ", " << testVec.z << ") speed=" << testSpeed << "\n";
                            }
                        }
                    }
                }
            }
            
            // Thử đọc NavArray (0x348) - từ partern 15.11
            uint64_t navArray = 0;
            if (SafeReadQWORD(aiManagerPtr + 0x348, &navArray)) {
                if (navArray > 0x100000 && navArray < 0x7FFFFFFFFFFF) {
                    file << "✅ NavArray (0x348): 0x" << std::hex << navArray << std::dec << " (valid pointer)\n";
                } else {
                    file << "⚠️  NavArray (0x348): 0x" << std::hex << navArray << std::dec << " (invalid/null pointer)\n";
                }
            } else {
                file << "❌ Cannot read NavArray (0x348)\n";
            }
            
            // Thử đọc TargetPosition (0x34) - từ partern 15.11
            Vector3 targetPos(0, 0, 0);
            if (TryReadVec3(aiManagerPtr + 0x34, targetPos)) {
                file << "✅ TargetPosition (0x34): (" << targetPos.x << ", " << targetPos.y << ", " << targetPos.z << ")\n";
                float targetDist = objPos.Distance(targetPos);
                file << "   Distance to GameObject: " << targetDist << " units\n";
            } else {
                file << "❌ Cannot read TargetPosition (0x34)\n";
            }
            
            // Thử đọc MoveVec3 (0x480) - từ partern 15.11
            Vector3 moveVec3(0, 0, 0);
            bool foundMoveVec3 = false;
            if (TryReadVec3(aiManagerPtr + 0x480, moveVec3)) {
                float length = sqrtf(moveVec3.x * moveVec3.x + moveVec3.y * moveVec3.y + moveVec3.z * moveVec3.z);
                if (length > 0.1f && length < 2.0f) { // Normalized vector
                    file << "✅ MoveVec3 (0x480): (" << moveVec3.x << ", " << moveVec3.y << ", " << moveVec3.z << ") length=" << length << "\n";
                    foundMoveVec3 = true;
                }
            }
            
            // Nếu không tìm thấy ở 0x480, scan toàn bộ range để tìm MoveVec3 mới
            if (!foundMoveVec3) {
                file << "❌ Cannot read MoveVec3 (0x480) - SCANNING for new offset...\n";
                file << "   Scanning range: 0x300 - 0x600 (looking for normalized direction vector)\n";
                
                std::vector<std::pair<uint64_t, Vector3>> candidates;
                
                // Scan range rộng hơn: 0x300 - 0x600
                for (uint64_t offset = 0x300; offset <= 0x600; offset += 4) {
                    // Skip các offset đã biết
                    if (offset == 0x318 || offset == 0x330 || offset == 0x33C || offset == 0x348 || 
                        offset == 0x350 || offset == 0x360 || offset == 0x384 || offset == 0x474) {
                        continue;
                    }
                    
                    Vector3 testVec(0, 0, 0);
                    if (TryReadVec3(aiManagerPtr + offset, testVec)) {
                        // MoveVec3 thường là normalized direction vector (length ~1.0)
                        float length = sqrtf(testVec.x * testVec.x + testVec.y * testVec.y + testVec.z * testVec.z);
                        
                        // Kiểm tra: normalized vector (length 0.5-1.5) và không phải position
                        // Position thường có x, z > 1000, direction vector có x, z < 10
                        if (length > 0.5f && length < 1.5f && 
                            abs(testVec.x) < 10.0f && abs(testVec.z) < 10.0f &&
                            abs(testVec.y) < 10.0f) {
                            candidates.push_back({offset, testVec});
                        }
                    }
                }
                
                if (candidates.empty()) {
                    file << "   ❌ No normalized direction vector found in range 0x300-0x600\n";
                    file << "   💡 MoveVec3 might not exist or calculated differently in this version\n";
                    file << "   💡 Alternative: Calculate direction from (EndPath - StartPath).Normalized()\n";
                } else {
                    file << "   ✅ Found " << candidates.size() << " potential MoveVec3 candidates:\n";
                    for (const auto& cand : candidates) {
                        float length = sqrtf(cand.second.x * cand.second.x + cand.second.y * cand.second.y + cand.second.z * cand.second.z);
                        file << "      → Offset 0x" << std::hex << cand.first << std::dec 
                             << ": (" << cand.second.x << ", " << cand.second.y << ", " << cand.second.z 
                             << ") length=" << length << "\n";
                    }
                    file << "   💡 RECOMMENDATION: Test these offsets when unit is MOVING to verify!\n";
                    file << "   💡 Compare with calculated direction: (EndPath - StartPath).Normalized()\n";
                }
            }
            
            // ====================================================================
            // SCAN FacingAngle (0x7C) - Float: ~PI when idle, changes when moving
            // ====================================================================
            file << "\n--- Scanning FacingAngle (0x7C) ---\n";
            
            // Detect current state (IDLE or MOVING) for better verification
            bool isCurrentlyMoving = false;
            uint8_t isMovingByteState = 0;
            if (SafeReadIsMoving(aiManagerPtr + 0x31C, &isMovingByteState)) {
                isCurrentlyMoving = (isMovingByteState != 0);
                file << "Current State: " << (isCurrentlyMoving ? "MOVING" : "IDLE") << " (from IsMoving 0x31C)\n";
            } else {
                // Fallback: Check path distance
                Vector3 startPathTest(0, 0, 0), endPathTest(0, 0, 0);
                if (TryReadVec3(aiManagerPtr + 0x330, startPathTest) && TryReadVec3(aiManagerPtr + 0x33C, endPathTest)) {
                    float pathDist = startPathTest.Distance(endPathTest);
                    isCurrentlyMoving = (pathDist > 10.0f);
                    file << "Current State: " << (isCurrentlyMoving ? "MOVING" : "IDLE") << " (from path distance)\n";
                }
            }
            
            float facingAngle = 0.0f;
            bool foundFacingAngle = false;
            if (SafeReadFloat(aiManagerPtr + 0x7C, &facingAngle)) {
                file << "✅ FacingAngle (0x7C): " << facingAngle << " radians (" << (facingAngle * 180.0f / 3.14159f) << " degrees)\n";
                
                // Check if it's ~PI (idle state)
                bool isPI = (fabsf(facingAngle - 3.14159f) < 0.1f);
                bool isAngleRange = (facingAngle >= -3.2f && facingAngle <= 3.2f);
                
                if (isPI) {
                    file << "   → Value is ~PI (3.14159)\n";
                    if (!isCurrentlyMoving) {
                        file << "   ✅ CORRECT! IDLE state should have FacingAngle ~PI\n";
                        file << "   ✅ Offset 0x7C is VERIFIED for FacingAngle!\n";
                    } else {
                        file << "   ⚠️  WARNING: Value is ~PI but unit is MOVING (should change when moving)\n";
                        file << "   💡 RECOMMENDATION: Scan again when IDLE to verify\n";
                    }
                    foundFacingAngle = true;
                } else if (isAngleRange) {
                    file << "   → Value is in angle range (-PI to PI)\n";
                    if (isCurrentlyMoving) {
                        file << "   ✅ CORRECT! MOVING state should have FacingAngle in range\n";
                        file << "   ✅ Offset 0x7C is VERIFIED for FacingAngle!\n";
                        file << "   💡 RECOMMENDATION: Scan again when IDLE (should be ~PI) to fully verify\n";
                    } else {
                        file << "   ⚠️  WARNING: Value is in angle range but unit is IDLE (should be ~PI)\n";
                        file << "   💡 RECOMMENDATION: Scan again when MOVING to verify it changes\n";
                    }
                    foundFacingAngle = true;
                } else {
                    file << "   ⚠️  Value is NOT in expected angle range (-PI to PI) - might be wrong offset\n";
                }
            } else {
                file << "❌ Cannot read FacingAngle (0x7C) - SCANNING for new offset...\n";
            }
            
            // If not found at 0x7C, scan range 0x0-0x200 for float ~PI
            if (!foundFacingAngle) {
                file << "   Scanning range: 0x0 - 0x200 (looking for float value ~PI when idle)\n";
                std::vector<std::pair<uint64_t, float>> candidates;
                
                for (uint64_t offset = 0x0; offset <= 0x200; offset += 4) {
                    // Skip known offsets
                    if (offset == 0x34 || offset == 0x7C || offset == 0x318 || offset == 0x330 || 
                        offset == 0x33C || offset == 0x348 || offset == 0x350 || offset == 0x360 || 
                        offset == 0x384 || offset == 0x474) {
                        continue;
                    }
                    
                    float testFloat = 0.0f;
                    if (SafeReadFloat(aiManagerPtr + offset, &testFloat)) {
                        // Check if it's ~PI (idle) or angle-like value (-PI to PI range)
                        if ((fabsf(testFloat - 3.14159f) < 0.1f) || 
                            (testFloat >= -3.2f && testFloat <= 3.2f && fabsf(testFloat) > 0.1f)) {
                            candidates.push_back({offset, testFloat});
                        }
                    }
                }
                
                if (candidates.empty()) {
                    file << "   ❌ No angle-like float value found in range 0x0-0x200\n";
                } else {
                    file << "   ✅ Found " << candidates.size() << " potential FacingAngle candidates:\n";
                    for (const auto& cand : candidates) {
                        bool isPI = (fabsf(cand.second - 3.14159f) < 0.1f);
                        file << "      → Offset 0x" << std::hex << cand.first << std::dec 
                             << ": " << cand.second << " radians";
                        if (isPI) {
                            file << " (~PI - IDLE state)";
                        }
                        file << "\n";
                    }
                    file << "   💡 RECOMMENDATION: Test when IDLE (should be ~PI) and MOVING (should change)\n";
                }
            }
            
            // ====================================================================
            // SCAN HasPath (0x268) - MỞ RỘNG SCAN để tìm offset chính xác
            // ====================================================================
            file << "\n--- Scanning HasPath (0x268) - MỞ RỘNG SCAN ---\n";
            
            // Detect current state for verification
            file << "Current State: " << (isCurrentlyMoving ? "MOVING (should have path)" : "IDLE (may or may not have path)") << "\n";
            
            int hasPath = 0;
            bool foundHasPath = false;
            if (SafeReadInt(aiManagerPtr + 0x268, &hasPath)) {
                file << "✅ HasPath (0x268): " << hasPath << " (int value)\n";
                
                // Check if it looks like a flag (0 = no path, non-zero = has path)
                if (hasPath == 0 || hasPath == 1) {
                    file << "   → Value is 0/1 flag → " << (hasPath ? "HAS PATH" : "NO PATH") << "\n";
                    if (isCurrentlyMoving && hasPath == 1) {
                        file << "   ✅ CORRECT! MOVING state should have HasPath = 1\n";
                        file << "   ✅ Offset 0x268 is VERIFIED for HasPath!\n";
                        file << "   💡 RECOMMENDATION: Scan again when IDLE to verify (might be 0 or 1)\n";
                    } else if (!isCurrentlyMoving) {
                        file << "   ✅ Offset 0x268 looks CORRECT for HasPath (0/1 flag pattern)\n";
                        file << "   💡 RECOMMENDATION: Scan again when MOVING to verify it changes to 1\n";
                    } else {
                        file << "   ⚠️  WARNING: MOVING but HasPath = 0 (should be 1 when moving)\n";
                        file << "   💡 RECOMMENDATION: Verify offset or check if flag works differently\n";
                    }
                    foundHasPath = true;
                } else if (hasPath >= 0 && hasPath <= 10) {
                    file << "   → Value is small int (0-10) → Might be path flag\n";
                    if (isCurrentlyMoving && hasPath > 0) {
                        file << "   ✅ CORRECT! MOVING state has non-zero value\n";
                        file << "   ✅ Offset 0x268 might be CORRECT for HasPath!\n";
                        file << "   💡 RECOMMENDATION: Scan again when IDLE to verify pattern\n";
                    } else {
                        file << "   ✅ Offset 0x268 might be CORRECT for HasPath (small int pattern)\n";
                        file << "   💡 RECOMMENDATION: Scan again when MOVING/IDLE to verify pattern\n";
                    }
                    foundHasPath = true;
                } else {
                    file << "   ⚠️  Value is NOT in expected flag range (0-10) - might be wrong offset\n";
                }
            } else {
                file << "❌ Cannot read HasPath (0x268) - SCANNING for new offset...\n";
            }
            
            // ====================================================================
            // MỞ RỘNG SCAN: Tìm tất cả các giá trị có thể là HasPath
            // ====================================================================
            file << "\n=== MỞ RỘNG SCAN HasPath (0x0 - 0x600) ===\n";
            file << "Scanning cả BYTE (0/1) và INT (0-100) để tìm flag pattern\n\n";
            
            // Phase 1: Scan BYTE values (0/1 flags) - Range 0x0-0x600
            file << "--- PHASE 1: BYTE FLAGS (0/1) - Range 0x0-0x600 ---\n";
            std::vector<std::pair<uint64_t, uint8_t>> byteCandidates;
            
            for (uint64_t offset = 0x0; offset <= 0x600; offset += 1) {
                // Skip known offsets
                if (offset == 0x31C || offset == 0x384) { // IsMoving, IsDashing
                    continue;
                }
                
                uint8_t testByte = 0;
                if (SafeReadByte(aiManagerPtr + offset, &testByte)) {
                    // Look for 0/1 flag pattern
                    if (testByte == 0 || testByte == 1) {
                        byteCandidates.push_back({offset, testByte});
                    }
                }
            }
            
            if (byteCandidates.empty()) {
                file << "   ❌ No 0/1 byte flags found\n";
            } else {
                file << "   ✅ Found " << byteCandidates.size() << " byte flag candidates (0/1):\n";
                // Limit output to first 50 to avoid spam
                int count = 0;
                for (const auto& cand : byteCandidates) {
                    if (count++ >= 50) {
                        file << "   ... (showing first 50, total " << byteCandidates.size() << ")\n";
                        break;
                    }
                    file << "      → Offset 0x" << std::hex << cand.first << std::dec 
                         << ": " << (int)cand.second << " (byte)\n";
                }
            }
            file << "\n";
            
            // Phase 2: Scan INT values (0-100) - Range 0x0-0x600, step 4
            file << "--- PHASE 2: INT FLAGS (0-100) - Range 0x0-0x600 ---\n";
            std::vector<std::pair<uint64_t, int>> intCandidates;
            
            for (uint64_t offset = 0x0; offset <= 0x600; offset += 4) {
                // Skip known offsets
                if (offset == 0x268 || offset == 0x320 || offset == 0x350) { // HasPath, CurrentSegment, SegmentsCount
                    continue;
                }
                
                int testInt = 0;
                if (SafeReadInt(aiManagerPtr + offset, &testInt)) {
                    // Look for flag-like values: 0/1 or small int 0-100
                    // HasPath có thể là:
                    // - 0/1 flag (boolean)
                    // - Small int 0-10 (segment count related)
                    // - Hoặc giá trị lớn hơn nhưng vẫn là flag (0 = no path, non-zero = has path)
                    if ((testInt == 0 || testInt == 1) || 
                        (testInt >= 0 && testInt <= 100)) {
                        intCandidates.push_back({offset, testInt});
                    }
                }
            }
            
            if (intCandidates.empty()) {
                file << "   ❌ No flag-like int values found\n";
            } else {
                file << "   ✅ Found " << intCandidates.size() << " int flag candidates (0-100):\n";
                // Limit output to first 50
                int count = 0;
                for (const auto& cand : intCandidates) {
                    if (count++ >= 50) {
                        file << "   ... (showing first 50, total " << intCandidates.size() << ")\n";
                        break;
                    }
                    file << "      → Offset 0x" << std::hex << cand.first << std::dec 
                         << ": " << cand.second;
                    if (cand.second == 0 || cand.second == 1) {
                        file << " (0/1 flag)";
                    } else if (cand.second <= 10) {
                        file << " (small int)";
                    }
                    file << "\n";
                }
            }
            file << "\n";
            
            // Phase 3: Scan các candidates đã biết từ scan trước
            file << "--- PHASE 3: TESTING KNOWN CANDIDATES ---\n";
            file << "Testing các candidates từ scan trước: 0x218, 0x220, 0x224, 0x228, 0x258, 0x260, 0x298, 0x2d8, 0x2f4\n\n";
            
            uint64_t knownCandidates[] = {0x218, 0x220, 0x224, 0x228, 0x258, 0x260, 0x298, 0x2d8, 0x2f4};
            const char* knownNames[] = {"0x218", "0x220", "0x224", "0x228", "0x258", "0x260", "0x298", "0x2d8", "0x2f4"};
            
            for (int i = 0; i < 9; i++) {
                uint64_t offset = knownCandidates[i];
                
                // Test as BYTE
                uint8_t byteVal = 0;
                bool hasByte = SafeReadByte(aiManagerPtr + offset, &byteVal);
                
                // Test as INT
                int intVal = 0;
                bool hasInt = SafeReadInt(aiManagerPtr + offset, &intVal);
                
                file << "   [" << knownNames[i] << "]: ";
                if (hasByte) {
                    file << "BYTE=" << (int)byteVal;
                }
                if (hasInt) {
                    if (hasByte) file << ", ";
                    file << "INT=" << intVal;
                }
                if (!hasByte && !hasInt) {
                    file << "❌ Cannot read";
                } else {
                    // Analyze value
                    if (hasByte && (byteVal == 0 || byteVal == 1)) {
                        file << " → BYTE FLAG (0/1)";
                    }
                    if (hasInt && (intVal == 0 || intVal == 1)) {
                        file << " → INT FLAG (0/1)";
                    }
                    if (hasInt && intVal >= 0 && intVal <= 10) {
                        file << " → Small int (0-10)";
                    }
                }
                file << "\n";
            }
            
            file << "\n";
            file << "💡 RECOMMENDATION:\n";
            file << "1. Scan TWICE: một lần khi IDLE, một lần khi MOVING\n";
            file << "2. So sánh giá trị giữa IDLE và MOVING\n";
            file << "3. HasPath nên thay đổi: 0 (IDLE) → 1 hoặc non-zero (MOVING)\n";
            file << "4. Các candidates tốt nhất là những giá trị thay đổi giữa IDLE và MOVING\n";
            file << "5. HasPath có thể là BYTE (0/1) hoặc INT (0-100)\n";
            file << "6. Nếu tất cả candidates = 0 khi MOVING, có thể cần test khi IDLE\n";
            file << "   (có thể logic ngược: 1 = no path, 0 = has path)\n";
            
            file << "\n";
        }
        
        file << "====================================================================\n";
        file << "Scan Complete!\n";
        file << "====================================================================\n";
        file << "\n";
        file << "RECOMMENDATION:\n";
        file << "1. Look for offset that shows 'MATCHES GameObject position'\n";
        file << "2. Update oObjAiManagerObf in Offsets.h with the correct offset\n";
        file << "3. Verify IsMoving/IsDashing change correctly when unit moves/dashes\n";
        file << "\n";
        file << "=== VERIFY FacingAngle (0x7C) & HasPath (0x268) ===\n";
        file << "To fully verify these offsets, scan TWICE:\n";
        file << "\n";
        file << "STEP 1: Scan when IDLE (đứng yên):\n";
        file << "  - Stand still in game\n";
        file << "  - Click 'Scan Obfuscated Offset'\n";
        file << "  - Check FacingAngle: Should be ~PI (3.14159)\n";
        file << "  - Check HasPath: May be 0 or 1 (depends on implementation)\n";
        file << "\n";
        file << "STEP 2: Scan when MOVING (di chuyển):\n";
        file << "  - Click to move your character\n";
        file << "  - While moving, click 'Scan Obfuscated Offset'\n";
        file << "  - Check FacingAngle: Should change (not ~PI, should be angle in -PI to PI range)\n";
        file << "  - Check HasPath: Should be 1 (or non-zero) when moving\n";
        file << "\n";
        file << "If values change correctly between IDLE and MOVING → Offsets are VERIFIED!\n";
        
        file.close();
    }
    
    // ============================================================================
    // SCAN HasPath với so sánh IDLE vs MOVING - Tìm offset chính xác
    // ============================================================================
    inline void ScanHasPathIdleVsMoving(uint64_t localPlayerAddr, bool isCurrentlyIdle) {
        std::ofstream file(isCurrentlyIdle ? "haspath_scan_idle.txt" : "haspath_scan_moving.txt", std::ios::app);
        if (!file.is_open()) return;
        
        file << "====================================================================\n";
        file << "HAS PATH SCAN - " << (isCurrentlyIdle ? "IDLE STATE" : "MOVING STATE") << "\n";
        file << "====================================================================\n";
        file << "LocalPlayer: " << std::hex << localPlayerAddr << std::dec << "\n";
        file << "Time: " << GetTickCount() << "\n";
        file << "State: " << (isCurrentlyIdle ? "IDLE" : "MOVING") << "\n";
        file << "====================================================================\n\n";
        
        // Đọc obfuscated structure và decrypt
        uint64_t obfStructAddr = localPlayerAddr + 0x4218; // Verified offset
        uint64_t aiManagerPtr = IDA::DecryptAiManager(obfStructAddr);
        
        if (aiManagerPtr == 0 || aiManagerPtr < 0x100000 || aiManagerPtr > 0x7FFFFFFFFFFF) {
            file << "ERROR: Cannot decrypt AiManager\n";
            file.close();
            return;
        }
        
        file << "AiManager: 0x" << std::hex << aiManagerPtr << std::dec << "\n\n";
        
        // Verify IsMoving để confirm state
        uint8_t isMovingByte = 0;
        if (SafeReadIsMoving(aiManagerPtr + 0x31C, &isMovingByte)) {
            file << "IsMoving (0x31C): " << (isMovingByte ? "true" : "false") << "\n";
        }
        
        Vector3 startPath(0, 0, 0), endPath(0, 0, 0);
        if (TryReadVec3(aiManagerPtr + 0x330, startPath) && TryReadVec3(aiManagerPtr + 0x33C, endPath)) {
            float pathDist = startPath.Distance(endPath);
            file << "Path Distance: " << pathDist << " units\n";
        }
        file << "\n";
        
        // Scan BYTE values (0/1) - Full range
        file << "=== BYTE VALUES (0/1) - Range 0x0-0x600 ===\n";
        std::vector<std::pair<uint64_t, uint8_t>> byteValues;
        
        for (uint64_t offset = 0x0; offset <= 0x600; offset += 1) {
            // Skip known offsets
            if (offset == 0x31C || offset == 0x384) { // IsMoving, IsDashing
                continue;
            }
            
            uint8_t val = 0;
            if (SafeReadByte(aiManagerPtr + offset, &val)) {
                if (val == 0 || val == 1) {
                    byteValues.push_back({offset, val});
                }
            }
        }
        
        file << "Found " << byteValues.size() << " byte flags (0/1):\n";
        for (const auto& v : byteValues) {
            file << "  0x" << std::hex << v.first << std::dec << ": " << (int)v.second << "\n";
        }
        file << "\n";
        
        // Scan INT values (0-100) - Full range
        file << "=== INT VALUES (0-100) - Range 0x0-0x600 ===\n";
        std::vector<std::pair<uint64_t, int>> intValues;
        
        for (uint64_t offset = 0x0; offset <= 0x600; offset += 4) {
            // Skip known offsets
            if (offset == 0x320 || offset == 0x350) { // CurrentSegment, SegmentsCount
                continue;
            }
            
            int val = 0;
            if (SafeReadInt(aiManagerPtr + offset, &val)) {
                if ((val == 0 || val == 1) || (val >= 0 && val <= 100)) {
                    intValues.push_back({offset, val});
                }
            }
        }
        
        file << "Found " << intValues.size() << " int flags (0-100):\n";
        for (const auto& v : intValues) {
            file << "  0x" << std::hex << v.first << std::dec << ": " << v.second;
            if (v.second == 0 || v.second == 1) {
                file << " (0/1 flag)";
            }
            file << "\n";
        }
        file << "\n";
        
        // Test known candidates
        file << "=== KNOWN CANDIDATES ===\n";
        uint64_t knownCandidates[] = {0x218, 0x220, 0x224, 0x228, 0x258, 0x260, 0x268, 0x298, 0x2d8, 0x2f4};
        
        for (uint64_t offset : knownCandidates) {
            uint8_t byteVal = 0;
            int intVal = 0;
            bool hasByte = SafeReadByte(aiManagerPtr + offset, &byteVal);
            bool hasInt = SafeReadInt(aiManagerPtr + offset, &intVal);
            
            file << "  0x" << std::hex << offset << std::dec << ": ";
            if (hasByte) file << "BYTE=" << (int)byteVal;
            if (hasInt) {
                if (hasByte) file << ", ";
                file << "INT=" << intVal;
            }
            file << "\n";
        }
        
        file << "\n====================================================================\n";
        file << "INSTRUCTIONS:\n";
        file << "1. Scan khi IDLE (đứng yên) → haspath_scan_idle.txt\n";
        file << "2. Scan khi MOVING (di chuyển) → haspath_scan_moving.txt\n";
        file << "3. So sánh 2 files để tìm giá trị thay đổi\n";
        file << "4. HasPath offset là giá trị thay đổi: 0 (IDLE) → 1 hoặc non-zero (MOVING)\n";
        file << "====================================================================\n";
        
        file.close();
    }
}

