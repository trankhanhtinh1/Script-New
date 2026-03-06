#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "core/Vector.h"
#include "core/LeagueObfuscation.h"

// ============================================================================
// AiManager — Movement, pathing, dashing state
// Resolution: Read LeagueObfuscation<uint64_t> at obj + 0x4038,
//             Decrypt → +InnerManager(0x10) → AiManager pointer.
// ============================================================================

namespace SDK {

    class AiManager {
    public:
        uintptr_t address; // resolved AiManager pointer

        AiManager() : address(0) {}
        AiManager(uintptr_t objAddr) : address(0) {
            if (!objAddr) return;

            __try {
                // Read the obfuscation structure at verified offset 0x4038
                auto obf = Globals::Read<LeagueObfuscation<uint64_t>>(objAddr + Offset::AiManager::Offset);

                if (obf.isInit) {
                    uint64_t decrypted = Decrypt(obf);
                    if (decrypted && decrypted > 0x10000 && decrypted < 0x7FFFFFFFFFFF) {
                        // Read actual AiManager pointer at decrypted + InnerManager(0x10)
                        uintptr_t manager = Globals::Read<uintptr_t>(decrypted + Offset::AiManager::InnerManager);
                        if (Globals::IsValidPtr(manager)) {
                            address = manager;
                        }
                    }
                }
            } __except(1) { address = 0; }
        }

        bool IsValid() const { return Globals::IsValidPtr(address); }

        // ====================================================================
        // Movement State
        // ====================================================================

        bool IsMoving() const {
            if (!IsValid()) return false;
            return Globals::Read<bool>(address + Offset::AiManager::IsMoving);
        }

        bool IsDashing() const {
            if (!IsValid()) return false;
            return Globals::Read<bool>(address + Offset::AiManager::IsDashing);
        }

        float GetDashSpeed() const {
            if (!IsValid()) return 0.0f;
            return Globals::Read<float>(address + Offset::AiManager::DashSpeed);
        }

        // ====================================================================
        // Positions
        // ====================================================================

        Vec3 GetVelocity() const {
            if (!IsValid()) return Vec3();
            return Globals::Read<Vec3>(address + Offset::AiManager::Velocity);
        }

        Vec3 GetServerPosition() const {
            if (!IsValid()) return Vec3();
            return Globals::Read<Vec3>(address + Offset::AiManager::ServerPos);
        }

        Vec3 GetPathStart() const {
            if (!IsValid()) return Vec3();
            return Globals::Read<Vec3>(address + Offset::AiManager::PathStart);
        }

        Vec3 GetPathEnd() const {
            if (!IsValid()) return Vec3();
            return Globals::Read<Vec3>(address + Offset::AiManager::PathEnd);
        }

        Vec3 GetTargetPosition() const {
            if (!IsValid()) return Vec3();
            return Globals::Read<Vec3>(address + Offset::AiManager::MoveVec3);
        }

        // ====================================================================
        // Path Segments
        // ====================================================================

        int GetCurrentSegment() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::AiManager::CurrentSegment);
        }

        int GetSegmentCount() const {
            if (!IsValid()) return 0;
            return Globals::Read<int>(address + Offset::AiManager::SegmentsCount);
        }

        Vec3 GetSegment(int index) const {
            if (!IsValid()) return Vec3();
            uintptr_t segBase = Globals::Read<uintptr_t>(address + Offset::AiManager::Segments);
            if (!Globals::IsValidPtr(segBase)) return Vec3();
            return Globals::Read<Vec3>(segBase + index * sizeof(Vec3));
        }

        // Get all remaining path segments
        std::vector<Vec3> GetRemainingPath() const {
            std::vector<Vec3> path;
            if (!IsValid()) return path;

            int current = GetCurrentSegment();
            int total = GetSegmentCount();
            if (total <= 0 || total > 50 || current < 0 || current >= total) return path;

            uintptr_t segBase = Globals::Read<uintptr_t>(address + Offset::AiManager::Segments);
            if (!Globals::IsValidPtr(segBase)) return path;

            for (int i = current; i < total; i++) {
                Vec3 seg = Globals::Read<Vec3>(segBase + i * sizeof(Vec3));
                path.push_back(seg);
            }
            return path;
        }

        // ====================================================================
        // Prediction
        // ====================================================================

        // Predict position after t seconds based on current path
        Vec3 PredictPosition(float t, float moveSpeed) const {
            if (!IsValid() || !IsMoving() || t <= 0.0f) {
                return GetServerPosition();
            }

            Vec3 currentPos = GetServerPosition();
            float distToTravel = moveSpeed * t;

            // Simple: linear prediction toward path end
            Vec3 target = GetPathEnd();
            if (target.IsZero()) return currentPos;

            Vec3 dir = target - currentPos;
            float dist = dir.Length2D();
            if (dist < 1.0f) return currentPos;

            Vec3 norm = dir.Normalized();
            float actualDist = (distToTravel < dist) ? distToTravel : dist;
            return Vec3(
                currentPos.x + norm.x * actualDist,
                currentPos.y,
                currentPos.z + norm.z * actualDist
            );
        }

        // Full path-following prediction
        Vec3 PredictPositionPath(float t, float moveSpeed) const {
            if (!IsValid() || !IsMoving() || t <= 0.0f)
                return GetServerPosition();

            auto path = GetRemainingPath();
            if (path.empty()) return GetServerPosition();

            Vec3 current = GetServerPosition();
            float remaining = moveSpeed * t;

            for (auto& waypoint : path) {
                Vec3 dir = waypoint - current;
                float segDist = dir.Length2D();

                if (segDist >= remaining) {
                    Vec3 norm = dir.Normalized();
                    return Vec3(
                        current.x + norm.x * remaining,
                        current.y,
                        current.z + norm.z * remaining
                    );
                }

                remaining -= segDist;
                current = waypoint;
            }

            return current; // Reached end of path
        }
    };

} // namespace SDK
