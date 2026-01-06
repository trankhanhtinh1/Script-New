#pragma once
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <map>
#include <cmath>
#include "Offsets.h"
#include "../Vector.h"

// ============================================================================
// HUD & SPELL INPUT OFFSET SCANNER (LIVE MONITORING VERSION)
// ============================================================================
// Chế độ monitoring liên tục:
// - Check ON: Bắt đầu theo dõi, thu thập data
// - Check OFF: Xuất kết quả ra file
// ============================================================================

namespace HudOffsetScanner {

    // ============================================================================
    // GLOBAL STATE
    // ============================================================================
    
    // Monitoring flags
    inline bool g_MonitoringSpellInput = false;
    inline bool g_MonitoringCamera = false;
    
    // Data storage during monitoring
    struct SpellInputMonitorData {
        // Collected candidates
        std::map<uint64_t, std::vector<uint32_t>> netIdHistory;      // offset -> values
        std::map<uint64_t, std::vector<Vector3>> positionHistory;    // offset -> Vec3 values
        int sampleCount = 0;
        Vector3 lastPlayerPos;
    };
    
    struct CameraMonitorData {
        std::map<uint64_t, std::vector<float>> zoomHistory;   // offset -> zoom values
        float minZoomSeen = 99999.0f;
        float maxZoomSeen = 0.0f;
        int sampleCount = 0;
    };
    
    inline SpellInputMonitorData g_SpellInputData;
    inline CameraMonitorData g_CameraData;

    // Safe memory read helper
    template<typename T>
    bool SafeRead(uint64_t address, T* out) {
        if (address < 0x10000 || address > 0x7FFFFFFFFFFF) return false;
        __try {
            *out = *(T*)address;
            return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    bool IsValidPointer(uint64_t ptr) {
        return ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF;
    }

    bool IsValidGamePosition(float x, float y, float z) {
        return x > 0 && x < 20000 && z > 0 && z < 20000 && y > -1000 && y < 3000;
    }

    // ============================================================================
    // SPELL INPUT MONITORING
    // ============================================================================
    
    void MonitorSpellInput() {
        if (!g_MonitoringSpellInput) return;
        
        uint64_t gameBase = (uint64_t)GetModuleHandleA(NULL);
        uint64_t localPlayer = 0;
        
        if (!SafeRead<uint64_t>(gameBase + Offset::oLocalPlayer, &localPlayer) || !IsValidPointer(localPlayer)) {
            return;
        }
        
        // Get player position for reference
        Vector3 playerPos;
        SafeRead<Vector3>(localPlayer + Offset::oObjPosition, &playerPos);
        g_SpellInputData.lastPlayerPos = playerPos;
        
        // Get SpellSlot for Q (slot 0)
        uint64_t spellBook = localPlayer + Offset::oObjSpellBook;
        uint64_t spellSlot = 0;
        if (!SafeRead<uint64_t>(spellBook + Offset::oObjSpellBookSpellSlot + (0 * 8), &spellSlot) || !IsValidPointer(spellSlot)) {
            return;
        }
        
        // Get SpellInput
        uint64_t spellInput = 0;
        if (!SafeRead<uint64_t>(spellSlot + Offset::oSpellSlotSpellInput, &spellInput) || !IsValidPointer(spellInput)) {
            return;
        }
        
        g_SpellInputData.sampleCount++;
        
        // Scan for NetIds
        for (uint64_t offset = 0; offset < 0x100; offset += 4) {
            uint32_t netId = 0;
            if (SafeRead<uint32_t>(spellInput + offset, &netId)) {
                if (netId >= 0x40000000 && netId < 0x50000000) {
                    g_SpellInputData.netIdHistory[offset].push_back(netId);
                }
            }
        }
        
        // Scan for Vec3 positions
        for (uint64_t offset = 0; offset < 0x100; offset += 4) {
            Vector3 pos;
            if (SafeRead<Vector3>(spellInput + offset, &pos)) {
                if (IsValidGamePosition(pos.x, pos.y, pos.z)) {
                    g_SpellInputData.positionHistory[offset].push_back(pos);
                }
            }
        }
    }
    
    void ExportSpellInputResults() {
        std::ofstream logFile("spellinput_scan_results.txt", std::ios::trunc);
        if (!logFile.is_open()) return;
        
        logFile << "============================================================" << std::endl;
        logFile << "SPELL INPUT OFFSET SCAN RESULTS" << std::endl;
        logFile << "============================================================" << std::endl;
        logFile << "Total samples: " << g_SpellInputData.sampleCount << std::endl;
        logFile << "Last player pos: (" << g_SpellInputData.lastPlayerPos.x << ", " 
                << g_SpellInputData.lastPlayerPos.y << ", " << g_SpellInputData.lastPlayerPos.z << ")" << std::endl;
        logFile << std::endl;
        
        // NetId candidates
        logFile << "=== TARGET NETID CANDIDATES ===" << std::endl;
        logFile << "Format: Offset -> appearances (unique values)" << std::endl;
        for (auto& pair : g_SpellInputData.netIdHistory) {
            if (pair.second.size() > 0) {
                std::map<uint32_t, int> valueCounts;
                for (auto v : pair.second) valueCounts[v]++;
                
                logFile << "  +0x" << std::hex << pair.first << std::dec << ": ";
                logFile << pair.second.size() << " hits, values: ";
                for (auto& vc : valueCounts) {
                    logFile << "0x" << std::hex << vc.first << std::dec << "(" << vc.second << ") ";
                }
                logFile << std::endl;
            }
        }
        
        // Position candidates
        logFile << "\n=== POSITION CANDIDATES (Vec3) ===" << std::endl;
        logFile << "Format: Offset -> appearances (sample positions)" << std::endl;
        for (auto& pair : g_SpellInputData.positionHistory) {
            if (pair.second.size() > 0) {
                logFile << "  +0x" << std::hex << pair.first << std::dec << ": ";
                logFile << pair.second.size() << " hits" << std::endl;
                
                // Show first 3 samples
                int shown = 0;
                for (auto& pos : pair.second) {
                    if (shown++ >= 3) break;
                    float dist = sqrt(pow(pos.x - g_SpellInputData.lastPlayerPos.x, 2) + 
                                     pow(pos.z - g_SpellInputData.lastPlayerPos.z, 2));
                    logFile << "      (" << pos.x << ", " << pos.y << ", " << pos.z << ") dist=" << dist << std::endl;
                }
            }
        }
        
        // Suggested offsets
        logFile << "\n=== SUGGESTED OFFSETS ===" << std::endl;
        logFile << "// Copy-paste to Offsets.h:" << std::endl;
        
        // Find best NetId candidate (most consistent)
        uint64_t bestNetIdOffset = 0;
        size_t bestNetIdCount = 0;
        for (auto& pair : g_SpellInputData.netIdHistory) {
            if (pair.second.size() > bestNetIdCount) {
                bestNetIdCount = pair.second.size();
                bestNetIdOffset = pair.first;
            }
        }
        if (bestNetIdCount > 0) {
            logFile << "inline constexpr uint64_t oSpellInputTargetNetId = 0x" << std::hex << bestNetIdOffset << ";" << std::dec << std::endl;
        }
        
        // Find position candidates closest to player (StartPos) and furthest (EndPos)
        uint64_t startPosOffset = 0, endPosOffset = 0;
        float minAvgDist = 99999.0f, maxAvgDist = 0.0f;
        for (auto& pair : g_SpellInputData.positionHistory) {
            if (pair.second.size() >= 1) {
                float avgDist = 0.0f;
                for (auto& pos : pair.second) {
                    avgDist += sqrt(pow(pos.x - g_SpellInputData.lastPlayerPos.x, 2) + 
                                   pow(pos.z - g_SpellInputData.lastPlayerPos.z, 2));
                }
                avgDist /= pair.second.size();
                
                if (avgDist < minAvgDist && avgDist < 100.0f) {
                    minAvgDist = avgDist;
                    startPosOffset = pair.first;
                }
                if (avgDist > maxAvgDist && avgDist > 50.0f) {
                    maxAvgDist = avgDist;
                    endPosOffset = pair.first;
                }
            }
        }
        if (startPosOffset > 0) {
            logFile << "inline constexpr uint64_t oSpellInputStartPos = 0x" << std::hex << startPosOffset << ";  // avg dist=" << std::dec << minAvgDist << std::endl;
        }
        if (endPosOffset > 0) {
            logFile << "inline constexpr uint64_t oSpellInputEndPos = 0x" << std::hex << endPosOffset << ";    // avg dist=" << std::dec << maxAvgDist << std::endl;
        }
        
        logFile.close();
        
        // Reset data
        g_SpellInputData = SpellInputMonitorData();
    }

    // ============================================================================
    // CAMERA ZOOM MONITORING
    // ============================================================================
    
    void MonitorCameraZoom() {
        if (!g_MonitoringCamera) return;
        
        uint64_t gameBase = (uint64_t)GetModuleHandleA(NULL);
        uint64_t hudInstance = 0;
        
        if (!SafeRead<uint64_t>(gameBase + Offset::oHudInstance, &hudInstance) || !IsValidPointer(hudInstance)) {
            return;
        }
        
        g_CameraData.sampleCount++;
        
        // Try getting camera first
        uint64_t camera = 0;
        if (SafeRead<uint64_t>(hudInstance + Offset::oHudInstanceCamera, &camera) && IsValidPointer(camera)) {
            // Scan camera for zoom values
            for (uint64_t offset = 0x100; offset < 0x500; offset += 4) {
                float zoom = 0;
                if (SafeRead<float>(camera + offset, &zoom)) {
                    if (zoom > 500.0f && zoom < 5000.0f) {
                        g_CameraData.zoomHistory[offset].push_back(zoom);
                        if (zoom < g_CameraData.minZoomSeen) g_CameraData.minZoomSeen = zoom;
                        if (zoom > g_CameraData.maxZoomSeen) g_CameraData.maxZoomSeen = zoom;
                    }
                }
            }
        }
        
        // Also scan HudInstance directly (in case camera offset is wrong)
        for (uint64_t offset = 0x100; offset < 0x500; offset += 4) {
            float zoom = 0;
            if (SafeRead<float>(hudInstance + offset, &zoom)) {
                if (zoom > 500.0f && zoom < 5000.0f) {
                    // Use offset + 0x10000 to distinguish from camera offsets
                    g_CameraData.zoomHistory[offset + 0x10000].push_back(zoom);
                }
            }
        }
    }
    
    void ExportCameraResults() {
        std::ofstream logFile("camera_zoom_scan_results.txt", std::ios::trunc);
        if (!logFile.is_open()) return;
        
        logFile << "============================================================" << std::endl;
        logFile << "CAMERA ZOOM OFFSET SCAN RESULTS" << std::endl;
        logFile << "============================================================" << std::endl;
        logFile << "Total samples: " << g_CameraData.sampleCount << std::endl;
        logFile << "Zoom range seen: " << g_CameraData.minZoomSeen << " - " << g_CameraData.maxZoomSeen << std::endl;
        logFile << "(Scroll mouse wheel during scan to see range change)" << std::endl;
        logFile << std::endl;
        
        logFile << "=== CAMERA ZOOM CANDIDATES ===" << std::endl;
        logFile << "Looking for: Offset that shows variation when zooming" << std::endl;
        
        // Sort by variation (offsets that changed = likely zoom)
        std::vector<std::pair<uint64_t, float>> variations;
        for (auto& pair : g_CameraData.zoomHistory) {
            if (pair.second.size() >= 2) {
                float minV = 99999.0f, maxV = 0.0f;
                for (auto v : pair.second) {
                    if (v < minV) minV = v;
                    if (v > maxV) maxV = v;
                }
                float variation = maxV - minV;
                variations.push_back({pair.first, variation});
            }
        }
        
        // Sort by variation (descending)
        std::sort(variations.begin(), variations.end(), 
            [](auto& a, auto& b) { return a.second > b.second; });
        
        for (auto& var : variations) {
            uint64_t offset = var.first;
            bool isHudDirect = (offset >= 0x10000);
            if (isHudDirect) offset -= 0x10000;
            
            auto& values = g_CameraData.zoomHistory[var.first];
            float minV = 99999.0f, maxV = 0.0f, sum = 0.0f;
            for (auto v : values) {
                if (v < minV) minV = v;
                if (v > maxV) maxV = v;
                sum += v;
            }
            float avgV = sum / values.size();
            
            logFile << "  " << (isHudDirect ? "HudInstance" : "Camera") << "+0x" << std::hex << offset << std::dec;
            logFile << ": samples=" << values.size();
            logFile << ", min=" << minV << ", max=" << maxV << ", avg=" << avgV;
            logFile << ", variation=" << var.second;
            if (var.second > 100.0f) {
                logFile << " **LIKELY ZOOM**";
            }
            logFile << std::endl;
        }
        
        // Suggested offset
        logFile << "\n=== SUGGESTED OFFSETS ===" << std::endl;
        if (!variations.empty() && variations[0].second > 50.0f) {
            uint64_t bestOffset = variations[0].first;
            bool isHudDirect = (bestOffset >= 0x10000);
            if (isHudDirect) bestOffset -= 0x10000;
            
            logFile << "// Best candidate (most variation):" << std::endl;
            logFile << "inline constexpr uint64_t oHudInstanceCameraZoom = 0x" << std::hex << bestOffset << ";" << std::dec << std::endl;
            if (isHudDirect) {
                logFile << "// Note: This offset is on HudInstance, not Camera" << std::endl;
            }
        } else {
            logFile << "// No clear zoom offset found. Try zooming in/out more during scan." << std::endl;
        }
        
        logFile.close();
        
        // Reset data
        g_CameraData = CameraMonitorData();
    }
    
    // ============================================================================
    // Toggle functions (called from menu)
    // ============================================================================
    
    void ToggleSpellInputMonitor(bool enable) {
        if (enable) {
            // Start monitoring
            g_MonitoringSpellInput = true;
            g_SpellInputData = SpellInputMonitorData(); // Reset data
        } else {
            // Stop monitoring and export
            g_MonitoringSpellInput = false;
            ExportSpellInputResults();
        }
    }
    
    void ToggleCameraMonitor(bool enable) {
        if (enable) {
            // Start monitoring
            g_MonitoringCamera = true;
            g_CameraData = CameraMonitorData(); // Reset data
        } else {
            // Stop monitoring and export
            g_MonitoringCamera = false;
            ExportCameraResults();
        }
    }
    
    // Call these every frame
    void Update() {
        if (g_MonitoringSpellInput) MonitorSpellInput();
        if (g_MonitoringCamera) MonitorCameraZoom();
    }
}
