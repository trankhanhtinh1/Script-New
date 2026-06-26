#pragma once

#include "Prediction/Health.h"

namespace SDK::HealthPrediction {

inline void Initialize() {
    Prediction::Health::Initialize();
}

inline void Reset() {
    Prediction::Health::Reset();
}

inline float GetPrediction(const AIBaseClient& unit,
                           int timeMs,
                           int delayMs = 70,
                           HealthPredictionType type = HealthPredictionType::Default) {
    return Prediction::Health::GetPrediction(unit, timeMs, delayMs, type);
}

inline AIBaseClient GetAggroTurret(const AIMinionClient& minion) {
    return Prediction::Health::GetAggroTurret(minion);
}

inline bool HasMinionAggro(const AIMinionClient& minion) {
    return Prediction::Health::HasMinionAggro(minion);
}

inline bool HasTurretAggro(const AIMinionClient& minion) {
    return Prediction::Health::HasTurretAggro(minion);
}

inline int TurretAggroStartTick(const AIMinionClient& minion) {
    return Prediction::Health::TurretAggroStartTick(minion);
}

} // namespace SDK::HealthPrediction
