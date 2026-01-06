#pragma once
#include <cstdint>
#include <windows.h>
#include <vector>
#include <string>
#include <cmath>
#include "Offsets.h"
#include "../Vector.h"

// Helper function to check if memory is readable (without SEH)
inline bool IsReadableMemory(uint64_t addr, size_t size) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) == 0) {
        return false;
    }
    if (mbi.State != MEM_COMMIT) {
        return false;
    }
    if (mbi.Protect == PAGE_NOACCESS || mbi.Protect == PAGE_EXECUTE) {
        return false;
    }
    // Check if address range is within committed memory
    return ((uint64_t)mbi.BaseAddress + mbi.RegionSize) >= (addr + size);
}

// ============================================================================
// MISSILE OFFSET SCANNER - Dynamic Offset Discovery for Multi-Version Support
// ============================================================================
// Purpose: Scan missile structure to find missing offsets that may change per version
// Strategy:
// 1. Get SpellInfo from missile (via verified patterns)
// 2. Read known values from SpellInfo (StartPos, EndPos, SrcIdx, etc.)
// 3. Scan missile structure to find matching values
// 4. Calculate derived values (Speed from SpellData, etc.)
// ============================================================================

namespace MissileScanner
{
    // ============================================================================
    // Offset Candidates - Store found offset candidates with confidence scores
    // ============================================================================
    struct OffsetCandidate {
        uint64_t offset;
        float confidence;      // 0.0 - 1.0
        std::string source;    // Which missile/spell found this
    };
    
    struct ScanResults {
        // Position Offsets
        std::vector<OffsetCandidate> startPosCandidates;
        std::vector<OffsetCandidate> endPosCandidates;
        
        // Movement Offsets
        std::vector<OffsetCandidate> speedCandidates;
        std::vector<OffsetCandidate> directionCandidates;
        
        // Collision Offsets  
        std::vector<OffsetCandidate> radiusCandidates;
        std::vector<OffsetCandidate> widthCandidates;
        
        // Timing Offsets
        std::vector<OffsetCandidate> startTimeCandidates;
        
        // Targeting Offsets
        std::vector<OffsetCandidate> destIdxCandidates;
        
        // Best guesses (highest confidence)
        uint64_t bestStartPosOffset = 0;
        uint64_t bestEndPosOffset = 0;
        uint64_t bestSpeedOffset = 0;
        uint64_t bestRadiusOffset = 0;
        uint64_t bestWidthOffset = 0;
        uint64_t bestStartTimeOffset = 0;
        uint64_t bestDestIdxOffset = 0;
    };
    
    // ============================================================================
    // Safe Read Functions (without SEH to avoid C2712)
    // ============================================================================
    inline bool SafeReadFloat(uint64_t addr, float* out) {
        if (!out || !IsReadableMemory(addr, sizeof(float))) return false;
        *out = *(float*)addr;
        return true;
    }
    
    inline bool SafeReadInt32(uint64_t addr, int* out) {
        if (!out || !IsReadableMemory(addr, sizeof(int))) return false;
        *out = *(int*)addr;
        return true;
    }
    
    inline bool SafeReadUInt64(uint64_t addr, uint64_t* out) {
        if (!out || !IsReadableMemory(addr, sizeof(uint64_t))) return false;
        *out = *(uint64_t*)addr;
        return true;
    }
    
    inline bool SafeReadVec3(uint64_t addr, float* x, float* y, float* z) {
        return SafeReadFloat(addr, x) && 
               SafeReadFloat(addr + 4, y) && 
               SafeReadFloat(addr + 8, z);
    }
    
    // ============================================================================
    // Vector3 Distance (for matching positions)
    // ============================================================================
    inline float Vec3Distance(float x1, float y1, float z1, float x2, float y2, float z2) {
        float dx = x2 - x1;
        float dy = y2 - y1;
        float dz = z2 - z1;
        return sqrtf(dx*dx + dy*dy + dz*dz);
    }
    
    inline bool Vec3Match(float x1, float y1, float z1, float x2, float y2, float z2, float tolerance = 10.0f) {
        return Vec3Distance(x1, y1, z1, x2, y2, z2) < tolerance;
    }
    
    // ============================================================================
    // Get SpellInfo from Missile (try all patterns)
    // ============================================================================
    inline uint64_t GetSpellInfoFromMissile(uint64_t missile, std::string* outPattern = nullptr) {
        if (!missile) return 0;
        
        // Pattern 1: Missile[0x1F0] -> SpellCast -> SpellCast[0xD8] -> SpellInfo
        if (IsReadableMemory(missile + Offset::oMissileSpellCast, sizeof(uint64_t))) {
            uint64_t spellCast = *(uint64_t*)(missile + Offset::oMissileSpellCast);
            if (spellCast > 0x100000 && spellCast < 0x7FFFFFFFFFFF) {
                if (IsReadableMemory(spellCast + Offset::oSpellCastSpellInfo, sizeof(uint64_t))) {
                    uint64_t spellInfo = *(uint64_t*)(spellCast + Offset::oSpellCastSpellInfo);
                    if (spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                        if (outPattern) *outPattern = "Pattern1_0x1F0->0xD8";
                        return spellInfo;
                    }
                }
            }
        }
        
        // Pattern 2: Missile[0x578] -> PTR[0x98] -> SpellInfo
        if (IsReadableMemory(missile + Offset::oMissileSpellInfo_Pattern2_Offset, sizeof(uint64_t))) {
            uint64_t ptr1 = *(uint64_t*)(missile + Offset::oMissileSpellInfo_Pattern2_Offset);
            if (ptr1 > 0x100000 && ptr1 < 0x7FFFFFFFFFFF) {
                if (IsReadableMemory(ptr1 + Offset::oMissileSpellInfo_Pattern2_Inner, sizeof(uint64_t))) {
                    uint64_t spellInfo = *(uint64_t*)(ptr1 + Offset::oMissileSpellInfo_Pattern2_Inner);
                    if (spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                        if (outPattern) *outPattern = "Pattern2_0x578->0x98";
                        return spellInfo;
                    }
                }
            }
        }
        
        // Pattern 3: Missile[0x728] -> PTR[0x98] -> SpellInfo
        if (IsReadableMemory(missile + Offset::oMissileSpellInfo_Pattern3_Offset, sizeof(uint64_t))) {
            uint64_t ptr2 = *(uint64_t*)(missile + Offset::oMissileSpellInfo_Pattern3_Offset);
            if (ptr2 > 0x100000 && ptr2 < 0x7FFFFFFFFFFF) {
                if (IsReadableMemory(ptr2 + Offset::oMissileSpellInfo_Pattern3_Inner, sizeof(uint64_t))) {
                    uint64_t spellInfo = *(uint64_t*)(ptr2 + Offset::oMissileSpellInfo_Pattern3_Inner);
                    if (spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                        if (outPattern) *outPattern = "Pattern3_0x728->0x98";
                        return spellInfo;
                    }
                }
            }
        }
        
        // Pattern 4: Missile[0xC0] -> PTR[0xE8] -> SpellInfo
        if (IsReadableMemory(missile + Offset::oMissileSpellInfo_Pattern4_Offset, sizeof(uint64_t))) {
            uint64_t ptr3 = *(uint64_t*)(missile + Offset::oMissileSpellInfo_Pattern4_Offset);
            if (ptr3 > 0x100000 && ptr3 < 0x7FFFFFFFFFFF) {
                if (IsReadableMemory(ptr3 + Offset::oMissileSpellInfo_Pattern4_Inner, sizeof(uint64_t))) {
                    uint64_t spellInfo = *(uint64_t*)(ptr3 + Offset::oMissileSpellInfo_Pattern4_Inner);
                    if (spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                        if (outPattern) *outPattern = "Pattern4_0xC0->0xE8";
                        return spellInfo;
                    }
                }
            }
        }
        
        return 0;
    }
    
    // ============================================================================
    // Get SpellInfo from Missile - C-style version (for use with __try/__except)
    // ============================================================================
    inline uint64_t GetSpellInfoFromMissileC(uint64_t missile, char* outPatternBuf, size_t bufSize) {
        if (!missile) return 0;
        
        // Pattern 1: Missile[0x1F0] -> SpellCast -> SpellCast[0xD8] -> SpellInfo
        if (IsReadableMemory(missile + Offset::oMissileSpellCast, sizeof(uint64_t))) {
            uint64_t spellCast = *(uint64_t*)(missile + Offset::oMissileSpellCast);
            if (spellCast > 0x100000 && spellCast < 0x7FFFFFFFFFFF) {
                if (IsReadableMemory(spellCast + Offset::oSpellCastSpellInfo, sizeof(uint64_t))) {
                    uint64_t spellInfo = *(uint64_t*)(spellCast + Offset::oSpellCastSpellInfo);
                    if (spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                        if (outPatternBuf && bufSize > 0) strncpy_s(outPatternBuf, bufSize, "Pattern1_0x1F0->0xD8", _TRUNCATE);
                        return spellInfo;
                    }
                }
            }
        }
        
        // Pattern 2: Missile[0x578] -> PTR[0x98] -> SpellInfo
        if (IsReadableMemory(missile + Offset::oMissileSpellInfo_Pattern2_Offset, sizeof(uint64_t))) {
            uint64_t ptr1 = *(uint64_t*)(missile + Offset::oMissileSpellInfo_Pattern2_Offset);
            if (ptr1 > 0x100000 && ptr1 < 0x7FFFFFFFFFFF) {
                if (IsReadableMemory(ptr1 + Offset::oMissileSpellInfo_Pattern2_Inner, sizeof(uint64_t))) {
                    uint64_t spellInfo = *(uint64_t*)(ptr1 + Offset::oMissileSpellInfo_Pattern2_Inner);
                    if (spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                        if (outPatternBuf && bufSize > 0) strncpy_s(outPatternBuf, bufSize, "Pattern2_0x578->0x98", _TRUNCATE);
                        return spellInfo;
                    }
                }
            }
        }
        
        // Pattern 3: Missile[0x728] -> PTR[0x98] -> SpellInfo
        if (IsReadableMemory(missile + Offset::oMissileSpellInfo_Pattern3_Offset, sizeof(uint64_t))) {
            uint64_t ptr2 = *(uint64_t*)(missile + Offset::oMissileSpellInfo_Pattern3_Offset);
            if (ptr2 > 0x100000 && ptr2 < 0x7FFFFFFFFFFF) {
                if (IsReadableMemory(ptr2 + Offset::oMissileSpellInfo_Pattern3_Inner, sizeof(uint64_t))) {
                    uint64_t spellInfo = *(uint64_t*)(ptr2 + Offset::oMissileSpellInfo_Pattern3_Inner);
                    if (spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                        if (outPatternBuf && bufSize > 0) strncpy_s(outPatternBuf, bufSize, "Pattern3_0x728->0x98", _TRUNCATE);
                        return spellInfo;
                    }
                }
            }
        }
        
        // Pattern 4: Missile[0xC0] -> PTR[0xE8] -> SpellInfo
        if (IsReadableMemory(missile + Offset::oMissileSpellInfo_Pattern4_Offset, sizeof(uint64_t))) {
            uint64_t ptr3 = *(uint64_t*)(missile + Offset::oMissileSpellInfo_Pattern4_Offset);
            if (ptr3 > 0x100000 && ptr3 < 0x7FFFFFFFFFFF) {
                if (IsReadableMemory(ptr3 + Offset::oMissileSpellInfo_Pattern4_Inner, sizeof(uint64_t))) {
                    uint64_t spellInfo = *(uint64_t*)(ptr3 + Offset::oMissileSpellInfo_Pattern4_Inner);
                    if (spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                        if (outPatternBuf && bufSize > 0) strncpy_s(outPatternBuf, bufSize, "Pattern4_0xC0->0xE8", _TRUNCATE);
                        return spellInfo;
                    }
                }
            }
        }
        
        return 0;
    }
    
    // ============================================================================
    // Get Spell Name from SpellInfo - C-style version (for use with __try/__except)
    // ============================================================================
    inline void GetSpellNameC(uint64_t spellInfo, char* outNameBuf, size_t bufSize) {
        if (!outNameBuf || bufSize == 0) return;
        outNameBuf[0] = '\0';
        
        if (!spellInfo) return;
        
        // Read SpellData pointer
        if (!IsReadableMemory(spellInfo + Offset::oSpellInfoSpellData, sizeof(uint64_t))) return;
        uint64_t spellData = *(uint64_t*)(spellInfo + Offset::oSpellInfoSpellData);
        if (!spellData || spellData < 0x100000) return;
        
        // Read name pointer
        if (!IsReadableMemory(spellData + Offset::oSpellDataName, sizeof(uint64_t))) return;
        uint64_t namePtr = *(uint64_t*)(spellData + Offset::oSpellDataName);
        if (!namePtr || namePtr < 0x10000) return;
        
        // Read name string (max bufSize-1 chars)
        size_t maxLen = bufSize - 1;
        for (size_t i = 0; i < maxLen; i++) {
            if (!IsReadableMemory(namePtr + i, sizeof(char))) break;
            char ch = *(char*)(namePtr + i);
            if (ch == 0) break;
            if (ch < 32 || ch > 126) { outNameBuf[0] = '\0'; return; }
            outNameBuf[i] = ch;
            outNameBuf[i + 1] = '\0';
        }
    }
    
    // ============================================================================
    // Get Spell Name from SpellInfo
    // ============================================================================
    inline std::string GetSpellName(uint64_t spellInfo) {
        if (!spellInfo) return "";
        
        // Read SpellData pointer
        if (!IsReadableMemory(spellInfo + Offset::oSpellInfoSpellData, sizeof(uint64_t))) return "";
        uint64_t spellData = *(uint64_t*)(spellInfo + Offset::oSpellInfoSpellData);
        if (!spellData || spellData < 0x100000) return "";
        
        // Read name pointer
        if (!IsReadableMemory(spellData + Offset::oSpellDataName, sizeof(uint64_t))) return "";
        uint64_t namePtr = *(uint64_t*)(spellData + Offset::oSpellDataName);
        if (!namePtr || namePtr < 0x10000) return "";
        
        // Read name string (max 63 chars)
        char name[64] = {0};
        for (int i = 0; i < 63; i++) {
            if (!IsReadableMemory(namePtr + i, sizeof(char))) break;
            char ch = *(char*)(namePtr + i);
            if (ch == 0) break;
            if (ch < 32 || ch > 126) return "";
            name[i] = ch;
        }
        return std::string(name);
    }
    
    // ============================================================================
    // MAIN SCAN FUNCTION - Scan single missile for missing offsets
    // ============================================================================
    inline bool ScanMissileForOffsets(uint64_t missile, float gameTime, int playerNetId, 
                                       ScanResults& results, FILE* logFile = nullptr) {
        if (!missile || missile < 0x10000) return false;
        
        // Get SpellInfo (reference for comparison)
        std::string pattern;
        uint64_t spellInfo = GetSpellInfoFromMissile(missile, &pattern);
        if (!spellInfo) {
            if (logFile) fprintf(logFile, "  ❌ Could not get SpellInfo from missile 0x%llX\n", missile);
            return false;
        }
        
        // Get spell name
        std::string spellName = GetSpellName(spellInfo);
        if (spellName.empty()) {
            if (logFile) fprintf(logFile, "  ⚠️ SpellInfo found but name invalid\n");
            return false;
        }
        
        if (logFile) {
            fprintf(logFile, "  ✅ Missile: %s (via %s)\n", spellName.c_str(), pattern.c_str());
        }
        
        // ====================================================================
        // Read reference values from SpellInfo
        // ====================================================================
        float refStartX, refStartY, refStartZ;
        float refEndX, refEndY, refEndZ;
        float refCastX, refCastY, refCastZ;
        int refSrcIdx = 0, refTargetIdx = 0;
        
        bool hasStartPos = SafeReadVec3(spellInfo + Offset::oSpellInfoStartPos, &refStartX, &refStartY, &refStartZ);
        bool hasEndPos = SafeReadVec3(spellInfo + Offset::oSpellInfoEndPos, &refEndX, &refEndY, &refEndZ);
        bool hasCastPos = SafeReadVec3(spellInfo + Offset::oSpellInfoCastPos, &refCastX, &refCastY, &refCastZ);
        SafeReadInt32(spellInfo + Offset::oSpellInfoSrcIndex, &refSrcIdx);
        SafeReadInt32(spellInfo + Offset::oSpellInfoTargetIndex, &refTargetIdx);
        
        if (logFile) {
            fprintf(logFile, "  Reference from SpellInfo:\n");
            if (hasStartPos) fprintf(logFile, "    StartPos: (%.1f, %.1f, %.1f)\n", refStartX, refStartY, refStartZ);
            if (hasEndPos) fprintf(logFile, "    EndPos: (%.1f, %.1f, %.1f)\n", refEndX, refEndY, refEndZ);
            if (hasCastPos) fprintf(logFile, "    CastPos: (%.1f, %.1f, %.1f)\n", refCastX, refCastY, refCastZ);
            fprintf(logFile, "    SrcIdx: 0x%X, TargetIdx: 0x%X\n", refSrcIdx, refTargetIdx);
        }
        
        // ====================================================================
        // SCAN PHASE 1: Find StartPos/EndPos by matching with SpellInfo
        // ====================================================================
        if (logFile) fprintf(logFile, "\n  --- Scanning for StartPos/EndPos ---\n");
        
        for (uint64_t off = 0x100; off <= 0x700; off += 0x4) {
            float x, y, z;
            if (!SafeReadVec3(missile + off, &x, &y, &z)) continue;
            
            // Validate coordinates are within map bounds
            if (x < 0 || x > 20000 || z < 0 || z > 20000) continue;
            if (y < -2000 || y > 5000) continue;
            
            // Check if matches StartPos
            if (hasStartPos && Vec3Match(x, y, z, refStartX, refStartY, refStartZ, 5.0f)) {
                OffsetCandidate candidate;
                candidate.offset = off;
                candidate.confidence = 1.0f;
                candidate.source = spellName;
                results.startPosCandidates.push_back(candidate);
                
                if (logFile) fprintf(logFile, "    ✅ StartPos MATCH at 0x%llX: (%.1f, %.1f, %.1f)\n", off, x, y, z);
            }
            
            // Check if matches EndPos
            if (hasEndPos && Vec3Match(x, y, z, refEndX, refEndY, refEndZ, 5.0f)) {
                OffsetCandidate candidate;
                candidate.offset = off;
                candidate.confidence = 1.0f;
                candidate.source = spellName;
                results.endPosCandidates.push_back(candidate);
                
                if (logFile) fprintf(logFile, "    ✅ EndPos MATCH at 0x%llX: (%.1f, %.1f, %.1f)\n", off, x, y, z);
            }
            
            // Check if matches CastPos
            if (hasCastPos && Vec3Match(x, y, z, refCastX, refCastY, refCastZ, 5.0f)) {
                // CastPos could be StartPos alternative
                OffsetCandidate candidate;
                candidate.offset = off;
                candidate.confidence = 0.8f;
                candidate.source = spellName + "_CastPos";
                results.startPosCandidates.push_back(candidate);
                
                if (logFile) fprintf(logFile, "    ⚠️ CastPos MATCH at 0x%llX: (%.1f, %.1f, %.1f)\n", off, x, y, z);
            }
        }
        
        // ====================================================================
        // SCAN PHASE 2: Find Speed/Radius/Width (float values)
        // ====================================================================
        if (logFile) fprintf(logFile, "\n  --- Scanning for Speed/Radius/Width ---\n");
        
        // Get SpellData for reference values if possible
        uint64_t spellData = 0;
        SafeReadUInt64(spellInfo + Offset::oSpellInfoSpellData, &spellData);
        
        for (uint64_t off = 0x100; off <= 0x600; off += 0x4) {
            float val;
            if (!SafeReadFloat(missile + off, &val)) continue;
            
            // Speed candidates: 400-5000 (most spells are 1000-2000)
            if (val >= 400 && val <= 5000) {
                OffsetCandidate candidate;
                candidate.offset = off;
                candidate.confidence = 0.5f; // Base confidence
                candidate.source = spellName;
                
                // Higher confidence for common missile speeds
                if (val >= 1000 && val <= 2500) candidate.confidence = 0.8f;
                
                results.speedCandidates.push_back(candidate);
                if (logFile && val >= 800 && val <= 3000) {
                    fprintf(logFile, "    Speed candidate at 0x%llX: %.1f\n", off, val);
                }
            }
            
            // Radius/Width candidates: 10-500
            if (val >= 10 && val <= 500) {
                OffsetCandidate candidate;
                candidate.offset = off;
                candidate.confidence = 0.5f;
                candidate.source = spellName;
                
                // Common widths: 60-120 (most skillshots)
                if (val >= 50 && val <= 150) candidate.confidence = 0.7f;
                
                results.radiusCandidates.push_back(candidate);
                results.widthCandidates.push_back(candidate);
            }
        }
        
        // ====================================================================
        // SCAN PHASE 3: Find StartTime (float close to gameTime)
        // ====================================================================
        if (logFile) fprintf(logFile, "\n  --- Scanning for StartTime ---\n");
        
        for (uint64_t off = 0x100; off <= 0x600; off += 0x4) {
            float val;
            if (!SafeReadFloat(missile + off, &val)) continue;
            
            // StartTime should be within 2 seconds of current gameTime
            float diff = fabsf(val - gameTime);
            if (diff < 2.0f && val > 10.0f) { // val > 10 to avoid random small floats
                OffsetCandidate candidate;
                candidate.offset = off;
                candidate.confidence = 1.0f - (diff / 2.0f); // Higher confidence for closer times
                candidate.source = spellName;
                results.startTimeCandidates.push_back(candidate);
                
                if (logFile) {
                    fprintf(logFile, "    StartTime candidate at 0x%llX: %.2f (gameTime=%.2f, diff=%.2f)\n", 
                            off, val, gameTime, diff);
                }
            }
        }
        
        // ====================================================================
        // SCAN PHASE 4: Find DestIdx (NetID matching target)
        // ====================================================================
        if (logFile) fprintf(logFile, "\n  --- Scanning for DestIdx ---\n");
        
        for (uint64_t off = 0x100; off <= 0x600; off += 0x4) {
            int val;
            if (!SafeReadInt32(missile + off, &val)) continue;
            
            // Valid NetID range
            if (val >= 0x40000000 && val <= 0x7FFFFFFF) {
                // Check if matches SpellInfo target
                if (refTargetIdx > 0 && val == refTargetIdx) {
                    OffsetCandidate candidate;
                    candidate.offset = off;
                    candidate.confidence = 1.0f;
                    candidate.source = spellName;
                    results.destIdxCandidates.push_back(candidate);
                    
                    if (logFile) fprintf(logFile, "    ✅ DestIdx MATCH at 0x%llX: 0x%X\n", off, val);
                }
                // Skip if it's the SrcIdx offset we already know
                else if (off != Offset::oMissileSrcIdx && off != Offset::oMissileNetId) {
                    OffsetCandidate candidate;
                    candidate.offset = off;
                    candidate.confidence = 0.3f;
                    candidate.source = spellName;
                    results.destIdxCandidates.push_back(candidate);
                    
                    if (logFile) fprintf(logFile, "    NetID at 0x%llX: 0x%X\n", off, val);
                }
            }
        }
        
        return true;
    }
    
    // ============================================================================
    // ANALYZE RESULTS - Find best offsets from all candidates
    // ============================================================================
    inline void AnalyzeResults(ScanResults& results, FILE* logFile = nullptr) {
        if (logFile) fprintf(logFile, "\n========================================\n");
        if (logFile) fprintf(logFile, "ANALYSIS RESULTS - Best Offset Candidates\n");
        if (logFile) fprintf(logFile, "========================================\n\n");
        
        // Helper lambda to find best candidate
        auto findBest = [](std::vector<OffsetCandidate>& candidates) -> uint64_t {
            if (candidates.empty()) return 0;
            
            // Count occurrences and sum confidence for each offset
            std::vector<std::pair<uint64_t, float>> scores;
            for (auto& c : candidates) {
                bool found = false;
                for (auto& s : scores) {
                    if (s.first == c.offset) {
                        s.second += c.confidence;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    scores.push_back({c.offset, c.confidence});
                }
            }
            
            // Find highest score
            uint64_t bestOffset = 0;
            float bestScore = 0;
            for (auto& s : scores) {
                if (s.second > bestScore) {
                    bestScore = s.second;
                    bestOffset = s.first;
                }
            }
            return bestOffset;
        };
        
        results.bestStartPosOffset = findBest(results.startPosCandidates);
        results.bestEndPosOffset = findBest(results.endPosCandidates);
        results.bestSpeedOffset = findBest(results.speedCandidates);
        results.bestRadiusOffset = findBest(results.radiusCandidates);
        results.bestWidthOffset = findBest(results.widthCandidates);
        results.bestStartTimeOffset = findBest(results.startTimeCandidates);
        results.bestDestIdxOffset = findBest(results.destIdxCandidates);
        
        if (logFile) {
            fprintf(logFile, "oMissileStartPos = 0x%llX;  // %s\n", 
                    results.bestStartPosOffset, 
                    results.bestStartPosOffset ? "✅ FOUND" : "❌ NOT FOUND");
            fprintf(logFile, "oMissileEndPos = 0x%llX;    // %s\n", 
                    results.bestEndPosOffset,
                    results.bestEndPosOffset ? "✅ FOUND" : "❌ NOT FOUND");
            fprintf(logFile, "oMissileSpeed = 0x%llX;     // %s (needs verification)\n", 
                    results.bestSpeedOffset,
                    results.bestSpeedOffset ? "⚠️ CANDIDATE" : "❌ NOT FOUND");
            fprintf(logFile, "oMissileRadius = 0x%llX;    // %s (needs verification)\n", 
                    results.bestRadiusOffset,
                    results.bestRadiusOffset ? "⚠️ CANDIDATE" : "❌ NOT FOUND");
            fprintf(logFile, "oMissileWidth = 0x%llX;     // %s (needs verification)\n", 
                    results.bestWidthOffset,
                    results.bestRadiusOffset ? "⚠️ CANDIDATE" : "❌ NOT FOUND");
            fprintf(logFile, "oMissileStartTime = 0x%llX; // %s\n", 
                    results.bestStartTimeOffset,
                    results.bestStartTimeOffset ? "✅ FOUND" : "❌ NOT FOUND");
            fprintf(logFile, "oMissileDestIdx = 0x%llX;   // %s (only for targeted spells)\n", 
                    results.bestDestIdxOffset,
                    results.bestDestIdxOffset ? "✅ FOUND" : "❌ NOT FOUND (normal for skillshots)");
        }
    }
    
    // ============================================================================
    // FULL SCAN - Scan all missiles and analyze
    // ============================================================================
    inline ScanResults FullScan(FILE* logFile = nullptr) {
        ScanResults results;
        
        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
        if (!moduleBase) return results;
        
        // Get game time
        float gameTime = 0;
        if (IsReadableMemory(moduleBase + Offset::oGametime, sizeof(float))) {
            gameTime = *(float*)(moduleBase + Offset::oGametime);
        } else {
            return results;
        }
        
        // Get player NetID
        int playerNetId = 0;
        if (IsReadableMemory(moduleBase + Offset::oLocalPlayer, sizeof(uint64_t))) {
            uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
            if (localPlayer && IsReadableMemory(localPlayer + Offset::oObjNetId, sizeof(int))) {
                playerNetId = *(int*)(localPlayer + Offset::oObjNetId);
            }
        }
        
        // Get missile manager
        uint64_t missileMgr = 0;
        if (IsReadableMemory(moduleBase + Offset::oMissileList, sizeof(uint64_t))) {
            missileMgr = *(uint64_t*)(moduleBase + Offset::oMissileList);
        } else {
            return results;
        }
        if (!missileMgr) return results;
        
        // Get missile array
        uint64_t arrayPtr = 0;
        int size = 0;
        if (IsReadableMemory(missileMgr + 0x08, sizeof(uint64_t)) && 
            IsReadableMemory(missileMgr + 0x10, sizeof(int))) {
            arrayPtr = *(uint64_t*)(missileMgr + 0x08);
            size = *(int*)(missileMgr + 0x10);
        } else {
            return results;
        }
        
        if (!arrayPtr || size <= 0 || size > 100) return results;
        
        if (logFile) {
            fprintf(logFile, "========================================\n");
            fprintf(logFile, "MISSILE OFFSET SCANNER - Dynamic Discovery\n");
            fprintf(logFile, "========================================\n");
            fprintf(logFile, "GameTime: %.2f\n", gameTime);
            fprintf(logFile, "PlayerNetID: 0x%X\n", playerNetId);
            fprintf(logFile, "Missiles found: %d\n\n", size);
        }
        
        // Scan each missile
        int scannedCount = 0;
        for (int i = 0; i < size && scannedCount < 10; i++) {
            uint64_t missile = 0;
            if (IsReadableMemory(arrayPtr + i * 0x8, sizeof(uint64_t))) {
                missile = *(uint64_t*)(arrayPtr + i * 0x8);
            } else {
                continue;
            }
            
            if (!missile || missile < 0x10000) continue;
            
            if (logFile) fprintf(logFile, "\n=== Missile[%d] @ 0x%llX ===\n", i, missile);
            
            if (ScanMissileForOffsets(missile, gameTime, playerNetId, results, logFile)) {
                scannedCount++;
            }
        }
        
        // Analyze and find best offsets
        AnalyzeResults(results, logFile);
        
        return results;
    }
}
