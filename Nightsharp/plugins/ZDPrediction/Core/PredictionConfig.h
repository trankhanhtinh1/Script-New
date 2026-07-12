#pragma once

namespace ZDPrediction {

struct PredictionConfig {
    bool usePathHistory = true;
    bool useVelocityBlend = true;
    bool useAcceleration = false;
    bool useWallAnalysis = false;
    bool useCollision = true;
    bool useAoe = true;
    int reactionTimeMs = 280;
    int historyWindowMs = 800;
    int maximumPredictionMs = 6000;
    int maximumPathSegments = 24;
    float maximumRangePercent = 90.0f;
    float highThreshold = 0.60f;
    float veryHighThreshold = 0.78f;
};

}
