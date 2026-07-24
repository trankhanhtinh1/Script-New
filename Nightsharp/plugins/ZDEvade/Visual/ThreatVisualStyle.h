#pragma once

#include <cstdint>

namespace ZDEvade {
namespace ThreatVisualStyle {

inline constexpr std::uint32_t kOuterStrokeColor = 0x78700000u;
inline constexpr std::uint32_t kCoreStrokeColor = 0xFFFF3030u;
inline constexpr std::uint32_t kLabelColor = 0xFFFF5555u;
inline constexpr float kOuterStrokeThickness = 6.0f;
inline constexpr float kCoreStrokeThickness = 2.5f;

constexpr std::uint8_t Alpha(std::uint32_t color) {
    return static_cast<std::uint8_t>((color >> 24) & 0xFFu);
}

constexpr std::uint8_t Red(std::uint32_t color) {
    return static_cast<std::uint8_t>((color >> 16) & 0xFFu);
}

constexpr std::uint8_t Green(std::uint32_t color) {
    return static_cast<std::uint8_t>((color >> 8) & 0xFFu);
}

constexpr std::uint8_t Blue(std::uint32_t color) {
    return static_cast<std::uint8_t>(color & 0xFFu);
}

constexpr bool IsRedFamily(std::uint32_t color) {
    return Alpha(color) > 0 &&
        Red(color) > Green(color) &&
        Red(color) > Blue(color);
}

static_assert(IsRedFamily(kOuterStrokeColor));
static_assert(IsRedFamily(kCoreStrokeColor));
static_assert(IsRedFamily(kLabelColor));
static_assert(Alpha(kOuterStrokeColor) < Alpha(kCoreStrokeColor));
static_assert(kOuterStrokeThickness > kCoreStrokeThickness);
static_assert(kCoreStrokeThickness > 0.0f);

} // namespace ThreatVisualStyle
} // namespace ZDEvade
