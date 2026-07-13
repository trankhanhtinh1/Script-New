#pragma once

#include "../../Core/Vector.h"

#include <cstdint>
#include <string>
#include <vector>

namespace NightSharp::Menu {

enum class VisualLayer : std::uint8_t {
    Background = 0,
    World,
    Indicator,
    Text,
    Foreground,
};

enum class VisualLifetime : std::uint8_t {
    Frame = 0,
    Persistent,
};

enum class VisualAnchor : std::uint8_t {
    Center = 0,
    Feet,
    Head,
    AboveHead,
    HealthBar,
};

enum class VisualCommandKind : std::uint8_t {
    LineScreen = 0,
    LineWorld,
    PolylineScreen,
    PolylineWorld,
    CircleScreen,
    CircleWorld,
    RingScreen,
    RingWorld,
    ArcScreen,
    ArcWorld,
    RectangleScreen,
    RectangleWorld,
    PolygonScreen,
    PolygonWorld,
    ArrowScreen,
    ArrowWorld,
    TextScreen,
    LabelUnit,
    HealthBar,
};

struct VisualStyle {
    std::uint32_t color = 0xFFFFFFFFu;
    std::uint32_t fillColor = 0x00000000u;
    float thickness = 1.5f;
    int segments = 0;
    bool filled = false;
    bool closed = false;
    bool foreground = true;
    VisualLayer layer = VisualLayer::World;
    VisualLifetime lifetime = VisualLifetime::Frame;
    int order = 0;
};

struct VisualLabelStyle {
    std::uint32_t color = 0xFFFFFFFFu;
    std::uint32_t shadowColor = 0xCC000000u;
    std::uint32_t outlineColor = 0xCC000000u;
    Vec2 offset{};
    Vec2 shadowOffset{ 1.0f, 1.0f };
    float scale = 1.0f;
    bool centered = true;
    bool shadow = true;
    bool outline = false;
    float maxWidth = 0.0f;
    VisualAnchor anchor = VisualAnchor::AboveHead;
    bool foreground = true;
    VisualLayer layer = VisualLayer::Text;
    VisualLifetime lifetime = VisualLifetime::Frame;
    int order = 0;
};

struct VisualHealthBarStyle {
    std::uint32_t backgroundColor = 0xB0000000u;
    std::uint32_t fillColor = 0xFF39D353u;
    std::uint32_t borderColor = 0xFF101010u;
    std::uint32_t textColor = 0xFFFFFFFFu;
    Vec2 offset{};
    float width = 70.0f;
    float height = 6.0f;
    float borderThickness = 1.0f;
    bool drawText = false;
    bool foreground = true;
    VisualLayer layer = VisualLayer::Indicator;
    VisualLifetime lifetime = VisualLifetime::Frame;
    int order = 0;
};

struct VisualTarget {
    Vec3 position{};
    float hpBarHeight = 0.0f;
    float health = 0.0f;
    float maxHealth = 0.0f;
};

struct VisualCommand {
    VisualCommandKind kind = VisualCommandKind::LineScreen;
    VisualStyle style{};
    VisualLabelStyle labelStyle{};
    VisualHealthBarStyle healthBarStyle{};
    std::string owner;
    std::string text;
    std::uint64_t id = 0;
    std::uint64_t sequence = 0;
    int ownerOrder = 0;
    Vec2 screenStart{};
    Vec2 screenEnd{};
    Vec2 screenCenter{};
    Vec3 worldStart{};
    Vec3 worldEnd{};
    Vec3 worldCenter{};
    VisualTarget target{};
    bool hasTarget = false;
    float radius = 0.0f;
    float secondaryRadius = 0.0f;
    float startAngle = 0.0f;
    float endAngle = 0.0f;
    float unitHeight = 0.0f;
    std::vector<Vec2> screenPoints;
    std::vector<Vec3> worldPoints;
};

}
