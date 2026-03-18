#pragma once
#include <cstdint>
#include "GameObjects/MissileClassification.h"

// ============================================================================
// PredictedDamage — Tracks incoming damage from active missiles
//
// Matching EnsoulSharp.SDK Health.cs PredictedDamage struct:
//   - Source/Target objects
//   - Timing: start tick, delay, animation time
//   - Damage amount and type classification
//   - Used by HealthPrediction to compute future HP
// ============================================================================

namespace SDK {

    struct PredictedDamage {
        uintptr_t source;           // Caster object address
        uintptr_t target;           // Target object address
        int sourceNetId;            // Caster network ID
        int targetNetId;            // Target network ID
        float startTick;            // Game time when missile was created/detected
        float delay;                // Time until impact (seconds)
        float animationTime;        // Attack animation time (seconds)
        float damage;               // Estimated damage amount
        float missileSpeed;         // Missile travel speed
        MissileType missileType;    // Classification (minion/turret/hero/spell)
        bool isAutoAttack;          // Is an auto attack (any type)
        bool isTurretShot;          // Is a turret shot specifically
        bool processed;             // Already counted in health prediction

        PredictedDamage()
            : source(0), target(0)
            , sourceNetId(0), targetNetId(0)
            , startTick(0.0f), delay(0.0f), animationTime(0.0f)
            , damage(0.0f), missileSpeed(0.0f)
            , missileType(MissileType::Unknown)
            , isAutoAttack(false), isTurretShot(false)
            , processed(false) {}

        // Predicted arrival time = startTick + delay
        float GetArrivalTime() const {
            return startTick + delay;
        }

        // Is this damage still relevant? (hasn't arrived yet)
        bool IsActive(float currentGameTime) const {
            return currentGameTime < GetArrivalTime();
        }
    };

} // namespace SDK
