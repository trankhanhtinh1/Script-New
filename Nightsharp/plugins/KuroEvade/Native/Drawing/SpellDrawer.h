#pragma once

#include "../../../../SDK/SDK.h"
#include "../Engine/Skillshot.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Plugins::KuroEvade {

enum class SpellVisualState : std::uint8_t {
    Irrelevant,
    PathThreat,
    DirectThreat,
    Intervention,
};

struct SpellVisualInfo {
    SpellVisualState State = SpellVisualState::Irrelevant;
    float HitTimeMs = 0.0f;
    int Danger = 1;
};

struct SpellDrawStyle {
    bool Fill = true;
    bool DrawIrrelevant = true;
    bool DrawLabels = false;
    int IrrelevantOpacity = 15;
    int ThreatOpacity = 70;
    int BorderWidth = 2;
    ImU32 IrrelevantColor = IM_COL32(145, 154, 164, 255);
    ImU32 PathColor = IM_COL32(255, 190, 65, 255);
    ImU32 DirectColor = IM_COL32(255, 72, 72, 255);
    ImU32 InterventionColor = IM_COL32(82, 220, 255, 255);
    ImU32 RouteColor = IM_COL32(100, 255, 150, 255);
    ImU32 MissileColor = IM_COL32(50, 205, 50, 255);
};

class SpellDrawer {
public:
    using VisualMap = std::unordered_map<const SourceSkillshot*, SpellVisualInfo>;

    static void Draw(const SourceSkillshotList& skillshots,
                     const VisualMap& visuals,
                     const SpellDrawStyle& style) {
        if (!ImGui::GetCurrentContext()) {
            return;
        }

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        if (!draw) {
            return;
        }

        for (const auto& skillshot : skillshots) {
            if (!skillshot || !skillshot->Native) {
                continue;
            }
            const auto it = visuals.find(skillshot.get());
            if (it == visuals.end()) {
                continue;
            }
            const SpellVisualInfo info = it->second;
            if (info.State == SpellVisualState::Irrelevant &&
                !style.DrawIrrelevant) {
                continue;
            }
            DrawSkillshot(draw, *skillshot->Native, info, style);
        }
    }

    static void DrawRoute(const Vec2& from,
                          const Vec2& to,
                          ImU32 color,
                          bool waiting) {
        if (!ImGui::GetCurrentContext() || from.IsZero() || to.IsZero()) {
            return;
        }

        const auto player = SDK::ObjectManager::Player();
        const float height = player.IsValid() ? player.ServerPosition().y : 0.0f;
        Vec2 fromScreen;
        Vec2 toScreen;
        if (!SDK::Drawing::WorldToScreen(Vec3::From2D(from, height), fromScreen) ||
            !SDK::Drawing::WorldToScreen(Vec3::From2D(to, height), toScreen)) {
            return;
        }

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        const ImVec2 a(fromScreen.x, fromScreen.y);
        const ImVec2 b(toScreen.x, toScreen.y);
        draw->AddLine(a, b, WithAlpha(color, waiting ? 52 : 64), 7.0f);
        draw->AddLine(a, b, WithAlpha(color, waiting ? 210 : 245), waiting ? 2.0f : 2.8f);
        draw->AddCircleFilled(b, 4.0f, WithAlpha(color, 235), 16);
        draw->AddCircle(b, waiting ? 10.0f : 8.0f,
                        WithAlpha(color, waiting ? 180 : 230), 24, 2.0f);
    }

private:
    static constexpr int kMaxPoints = 64;

    static ImU32 WithAlpha(ImU32 color, int alpha) {
        const int clamped = std::clamp(alpha, 0, 255);
        return (color & ~IM_COL32_A_MASK) |
            (static_cast<ImU32>(clamped) << IM_COL32_A_SHIFT);
    }

    static ImU32 BaseColor(SpellVisualState state,
                           const SpellDrawStyle& style) {
        switch (state) {
        case SpellVisualState::PathThreat: return style.PathColor;
        case SpellVisualState::DirectThreat: return style.DirectColor;
        case SpellVisualState::Intervention: return style.InterventionColor;
        default: return style.IrrelevantColor;
        }
    }

    static int ProjectPath(const SDK::Skillshot& skillshot,
                           std::array<ImVec2, kMaxPoints>& screens,
                           std::array<bool, kMaxPoints>& visible,
                           float height) {
        const int count = std::min(
            static_cast<int>(skillshot.Path.size()), kMaxPoints);
        for (int i = 0; i < count; ++i) {
            const auto& point = skillshot.Path[static_cast<std::size_t>(i)];
            Vec2 screen;
            visible[static_cast<std::size_t>(i)] = SDK::Drawing::WorldToScreen(
                Vec3(static_cast<float>(point.X), height,
                     static_cast<float>(point.Y)),
                screen);
            if (visible[static_cast<std::size_t>(i)]) {
                screens[static_cast<std::size_t>(i)] = ImVec2(screen.x, screen.y);
            }
        }
        return count;
    }

    static void DrawPartialOutline(ImDrawList* draw,
                                   const std::array<ImVec2, kMaxPoints>& screens,
                                   const std::array<bool, kMaxPoints>& visible,
                                   int count,
                                   ImU32 color,
                                   float thickness) {
        for (int i = 0; i < count; ++i) {
            const int next = (i + 1) % count;
            if (visible[static_cast<std::size_t>(i)] &&
                visible[static_cast<std::size_t>(next)]) {
                draw->AddLine(screens[static_cast<std::size_t>(i)],
                              screens[static_cast<std::size_t>(next)],
                              color, thickness);
            }
        }
    }

    static void DrawLabel(ImDrawList* draw,
                          const SDK::Skillshot& skillshot,
                          const SpellVisualInfo& info,
                          const std::array<ImVec2, kMaxPoints>& screens,
                          int count,
                          ImU32 color) {
        if (count <= 0) {
            return;
        }
        ImVec2 center;
        for (int i = 0; i < count; ++i) {
            center.x += screens[static_cast<std::size_t>(i)].x;
            center.y += screens[static_cast<std::size_t>(i)].y;
        }
        center.x /= static_cast<float>(count);
        center.y /= static_cast<float>(count);

        char text[128] = {};
        std::snprintf(text, sizeof(text), "%s  %.0fms",
                      skillshot.SData.SpellName.c_str(),
                      std::max(0.0f, info.HitTimeMs));
        const ImVec2 size = ImGui::CalcTextSize(text);
        const ImVec2 pos(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
        draw->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f),
                      IM_COL32(0, 0, 0, 210), text);
        draw->AddText(pos, WithAlpha(color, 245), text);
    }

    static void DrawSkillshot(ImDrawList* draw,
                              const SDK::Skillshot& skillshot,
                              const SpellVisualInfo& info,
                              const SpellDrawStyle& style) {
        if (skillshot.Path.size() < 2) {
            return;
        }

        const auto player = SDK::ObjectManager::Player();
        const float height = player.IsValid() ? player.ServerPosition().y : 0.0f;
        std::array<ImVec2, kMaxPoints> screens = {};
        std::array<bool, kMaxPoints> visible = {};
        const int count = ProjectPath(skillshot, screens, visible, height);
        if (count < 2) {
            return;
        }

        const bool allVisible = std::all_of(
            visible.begin(), visible.begin() + count,
            [](bool value) { return value; });
        const ImU32 base = BaseColor(info.State, style);
        const bool irrelevant = info.State == SpellVisualState::Irrelevant;
        const int fillOpacity = irrelevant
            ? style.IrrelevantOpacity
            : style.ThreatOpacity;
        const int fillAlpha = std::clamp(
            static_cast<int>(255.0f * static_cast<float>(fillOpacity) / 100.0f),
            0, 255);
        const int outlineAlpha = irrelevant
            ? std::max(24, fillAlpha * 2)
            : (info.State == SpellVisualState::PathThreat ? 215 : 250);
        const float thickness = irrelevant
            ? 1.0f
            : (info.State == SpellVisualState::PathThreat ? 2.0f : 2.8f);
        const bool canFill = style.Fill && allVisible && count >= 3 &&
            skillshot.SData.SpellType != SDK::SpellType::SkillshotRing;

        if (canFill && fillAlpha > 0) {
            draw->AddConvexPolyFilled(screens.data(), count,
                                      WithAlpha(base, fillAlpha));
        }

        if (info.State == SpellVisualState::Intervention && allVisible) {
            const float pulse = 0.82f + 0.18f *
                std::sin(SDK::Game::Time() * 7.0f);
            draw->AddPolyline(
                screens.data(), count,
                WithAlpha(base, static_cast<int>(80.0f * pulse)),
                ImDrawFlags_Closed, 7.0f);
        }

        if (allVisible) {
            draw->AddPolyline(screens.data(), count,
                              WithAlpha(base, outlineAlpha),
                              ImDrawFlags_Closed,
                              std::max(thickness, static_cast<float>(style.BorderWidth)));
        } else {
            DrawPartialOutline(draw, screens, visible, count,
                               WithAlpha(base, outlineAlpha),
                               std::max(thickness, static_cast<float>(style.BorderWidth)));
        }

        if (style.DrawLabels && !irrelevant && allVisible) {
            DrawLabel(draw, skillshot, info, screens, count, base);
        }
    }
};

} // namespace Plugins::KuroEvade
