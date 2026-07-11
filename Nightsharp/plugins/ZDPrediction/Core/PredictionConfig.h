#pragma once

namespace ZDPrediction {

struct PredictionConfig {
    bool usePathHistory = true;
    bool useVelocityBlend = true;
    bool useAcceleration = true;
    bool useWallAnalysis = true;
    bool useCollision = true;
    bool useAoe = true;
    int reactionTimeMs = 220;
    int historyWindowMs = 700;
    int maximumPredictionMs = 6000;
    int maximumPathSegments = 24;
    float maximumRangePercent = 100.0f;
    float highThreshold = 0.60f;
    float veryHighThreshold = 0.78f;
};

}
