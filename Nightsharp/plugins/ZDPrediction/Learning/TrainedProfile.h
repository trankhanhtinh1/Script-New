#pragma once

namespace ZDPrediction::TrainedProfile {

inline constexpr double VelocityAlpha = 0.48;
inline constexpr double AccelerationAlpha = 0.14;
inline constexpr double AccelerationScale = 1.00;
inline constexpr double LossDecay = 0.84;
inline constexpr double LossTemperature = 3.40;
inline constexpr double MinimumModelWeight = 0.04;

}
