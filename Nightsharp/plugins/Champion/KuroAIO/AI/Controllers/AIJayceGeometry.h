#pragma once
#include "../AIGeometry.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace Plugins::KuroAIO::AI::Controllers::Jayce::Geometry {
inline constexpr float kCannonQRange = 1050.0f, kCannonWRange = 500.0f,
                       kCannonERange = 650.0f, kHammerQRange = 600.0f;
inline constexpr float kGateRadius = 80.0f, kGateDuration = 4000.0f;
inline float CannonQDamage(int rank, float ad) {
  static constexpr std::array<float, 6> b{0, 80, 115, 150, 185, 220};
  return SharedGeometry::RankValue(b, rank) + 1.20f * std::max(0.0f, ad);
}
inline float HammerQDamage(int rank, float ad, float targetHealth) {
  static constexpr std::array<float, 6> b{0, 60, 100, 140, 180, 220};
  return SharedGeometry::RankValue(b, rank) + 1.0f * std::max(0.0f, ad) +
         (targetHealth < 40.0f ? 0.08f * targetHealth : 0.0f);
}
inline float CannonEDamage(int rank, float ad) {
  static constexpr std::array<float, 6> b{0, 8, 10, 12, 14, 16};
  return SharedGeometry::RankValue(b, rank) * std::max(1.0f, ad);
}
inline bool ShockBlastThroughGate(bool gateReady, bool aligned,
                                  bool wallBlocked) {
  return gateReady && aligned && !wallBlocked;
}
inline bool HammerLeapSafe(bool endpointWalkable, bool underTurret, bool lethal,
                           bool escaping) {
  return endpointWalkable && (!underTurret || lethal || escaping);
}
inline bool CanTransform(bool cooldownReady, bool channeling,
                         bool enemyThreat) {
  return cooldownReady && !channeling && !enemyThreat;
}
struct AutomaticContext {
  bool Defensive = false;
  bool Interrupt = false;
  bool KillSecure = false;
  bool Engage = false;
};
inline bool AutomaticAllowed(const AutomaticContext &c) {
  return !c.Engage && (c.Defensive || c.Interrupt || c.KillSecure);
}
} // namespace Plugins::KuroAIO::AI::Controllers::Jayce::Geometry
