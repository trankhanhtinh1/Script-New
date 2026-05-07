#pragma once

#include "NavGrid.h"
#include "Prediction/Types.h"
#include "Prediction/GamePath.h"
#include "Prediction/Movement.h"
#include "Prediction/Cluster.h"
#include "Prediction/Health.h"
#include "../Core/Game.h"

namespace SDK::Prediction {

inline void Initialize() {
    GamePath::Initialize();
    Health::Initialize();
}

inline void Update() {
    static int s_lastUpdateTick = 0;
    const int now = Game::TickCount();
    if (now - s_lastUpdateTick < 50) {
        return;
    }
    s_lastUpdateTick = now;

    GamePath::Update();
    Health::Update();
}

inline void Reset() {
    GamePath::Reset();
    Health::Reset();
}

inline PredictionOutput GetPrediction(const PredictionInput& input) {
    if (!input.Unit.IsValid()) {
        return {};
    }
    return input.AoE ? Cluster::GetAoEPrediction(input, input.Unit) : Movement::GetPrediction(input);
}

inline PredictionOutput GetPrediction(const AIBaseClient& target, const PredictionInput& input) {
    return input.AoE ? Cluster::GetAoEPrediction(input, target) : Movement::GetPrediction(target, input);
}

inline PredictionOutput GetPrediction(const PredictionInput& input, const AIBaseClient& target) {
    return input.AoE ? Cluster::GetAoEPrediction(input, target) : Movement::GetPrediction(input, target);
}

inline bool WillHit(const AIBaseClient& target, const PredictionInput& input, HitChance minimum = HitChance::High) {
    return Movement::WillHit(target, input, minimum);
}

inline PredictionOutput GetAoEPrediction(const AIBaseClient& target, const PredictionInput& input) {
    return Cluster::GetAoEPrediction(input, target);
}

} // namespace SDK::Prediction
