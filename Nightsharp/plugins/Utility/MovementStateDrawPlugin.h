#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <utility>
#include <vector>

namespace Plugins {

class MovementStateDrawPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Movement State Draw"; }
    const char* GetInternalId() const override { return "utility.movement_state_draw"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        NightSharpDebug::Logf("[MovementStateDraw] loaded");
    }

    void OnUnload() override {
        NightSharpDebug::Logf("[MovementStateDraw] unloaded");
    }

    void OnRender() override {
        if (!ImGui::GetCurrentContext() || !SDK::Drawing::IsEnabled()) {
            return;
        }

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) {
            return;
        }

        const auto ai = player.AiManagerSnapshot();
        const bool isMoving = player.IsMoving();
        const bool isDashing = player.IsDashing();
        const Vec3 serverPos = ai.serverPosition.IsZero()
            ? player.Position()
            : ai.serverPosition;
        if (!serverPos.IsValid() || serverPos.IsZero()) {
            return;
        }

        std::vector<Vec3> path = player.GetWaypoints(32);
        NormalizePath(path, serverPos, ai.targetPosition);

        if (m_drawPath) {
            DrawPath(path, isDashing);
            DrawPointRings(serverPos, 0xFFFFD200u, 0xFFFF00A8u);
            for (std::size_t i = 1; i < path.size(); ++i) {
                DrawWaypoint(path[i], i == path.size() - 1);
            }
        }

        if (m_drawDirection) {
            DrawDirectionArrow(serverPos, ai.velocity, path, isDashing);
        }

        if (m_drawPanel) {
            DrawPanel(player, ai, path, isMoving, isDashing);
        }
    }

    void OnMenu() override {
        ImGui::Checkbox("Draw path", &m_drawPath);
        ImGui::Checkbox("Draw direction arrow", &m_drawDirection);
        ImGui::Checkbox("Draw state panel", &m_drawPanel);
    }

private:
    static constexpr std::uint32_t kPathColor = 0xFFFFFF00u;
    static constexpr std::uint32_t kDashPathColor = 0xFFFF4444u;
    static constexpr std::uint32_t kDirectionColor = 0xFF42FFE8u;
    static constexpr std::uint32_t kDashDirectionColor = 0xFFFF40B8u;
    static constexpr std::uint32_t kWaypointColor = 0xFFFF5A2Au;
    static constexpr std::uint32_t kEndPointColor = 0xFF50FF70u;
    static constexpr std::uint32_t kTextColor = 0xFFFFFFFFu;
    static constexpr std::uint32_t kHeaderColor = 0xFF66E6FFu;
    static constexpr std::uint32_t kMovingColor = 0xFF5CFF7Au;
    static constexpr std::uint32_t kDashColor = 0xFFFF66DDu;
    static constexpr std::uint32_t kIdleColor = 0xFFFFD066u;

    bool m_drawPath = true;
    bool m_drawDirection = true;
    bool m_drawPanel = true;

    static ImU32 ToImColor(std::uint32_t argb) {
        const auto a = static_cast<int>((argb >> 24) & 0xFF);
        const auto r = static_cast<int>((argb >> 16) & 0xFF);
        const auto g = static_cast<int>((argb >> 8) & 0xFF);
        const auto b = static_cast<int>(argb & 0xFF);
        return IM_COL32(r, g, b, a);
    }

    static bool IsUsablePoint(const Vec3& point) {
        return point.IsValid() && !point.IsZero();
    }

    static Vec3 Normalize2D(Vec3 value) {
        value.y = 0.0f;
        const float length = value.Length2D();
        if (length < 0.0001f || !std::isfinite(length)) {
            return {};
        }
        return { value.x / length, 0.0f, value.z / length };
    }

    static void NormalizePath(std::vector<Vec3>& path,
                              const Vec3& serverPos,
                              const Vec3& pathEnd) {
        std::vector<Vec3> normalized;
        normalized.reserve(path.size() + 2);

        const auto push = [&](const Vec3& point) {
            if (!IsUsablePoint(point)) {
                return;
            }
            if (!normalized.empty() &&
                point.Distance2D(normalized.back()) <= 1.0f) {
                return;
            }
            normalized.push_back(point);
        };

        push(serverPos);
        for (const auto& point : path) {
            push(point);
        }
        push(pathEnd);
        path = std::move(normalized);
    }

    static void DrawPath(const std::vector<Vec3>& path, bool isDashing) {
        if (path.size() < 2) {
            return;
        }

        const std::uint32_t color = isDashing ? kDashPathColor : kPathColor;
        const float thickness = isDashing ? 3.0f : 2.0f;
        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            Vec2 from = {};
            Vec2 to = {};
            if (!SDK::Drawing::WorldToScreen(path[i], from) ||
                !SDK::Drawing::WorldToScreen(path[i + 1], to)) {
                continue;
            }
            SDK::Drawing::DrawLine(from, to, thickness, color);
        }
    }

    static void DrawPointRings(const Vec3& point,
                               std::uint32_t innerColor,
                               std::uint32_t outerColor) {
        Vec2 screen = {};
        if (!SDK::Drawing::WorldToScreen(point, screen)) {
            return;
        }

        SDK::Drawing::DrawCircle(screen, 8.0f, 2.0f, innerColor, 40);
        SDK::Drawing::DrawCircle(screen, 14.0f, 2.0f, outerColor, 48);
        SDK::Drawing::DrawCircle(screen, 21.0f, 1.5f, 0x88FF00FFu, 56);
    }

    static void DrawWaypoint(const Vec3& point, bool isEndPoint) {
        Vec2 screen = {};
        if (!SDK::Drawing::WorldToScreen(point, screen)) {
            return;
        }

        SDK::Drawing::DrawCircle(
            screen,
            isEndPoint ? 9.0f : 7.0f,
            2.0f,
            isEndPoint ? kEndPointColor : kWaypointColor,
            36);
        SDK::Drawing::DrawCircle(screen, 3.0f, 1.5f, kPathColor, 24);
    }

    static void DrawArrowScreen(const Vec2& from,
                                const Vec2& to,
                                std::uint32_t color,
                                float thickness) {
        const Vec2 delta = to - from;
        const float length = delta.Length();
        if (!std::isfinite(length) || length < 2.0f) {
            return;
        }

        const Vec2 direction = delta / length;
        const Vec2 normal{ -direction.y, direction.x };
        const float headLength = std::clamp(length * 0.18f, 12.0f, 24.0f);
        const float headWidth = headLength * 0.55f;
        const Vec2 left = to - direction * headLength + normal * headWidth;
        const Vec2 right = to - direction * headLength - normal * headWidth;

        SDK::Drawing::DrawLine(from, to, thickness, color);
        SDK::Drawing::DrawLine(to, left, thickness, color);
        SDK::Drawing::DrawLine(to, right, thickness, color);
    }

    static void DrawDirectionArrow(const Vec3& origin,
                                   const Vec3& velocity,
                                   const std::vector<Vec3>& path,
                                   bool isDashing) {
        Vec3 direction = Normalize2D(velocity);
        if (direction.IsZero() && path.size() >= 2) {
            direction = Normalize2D(path[1] - path[0]);
        }
        if (direction.IsZero()) {
            return;
        }

        const float speed = velocity.Length2D();
        const float arrowLength = std::clamp(
            speed > 1.0f ? speed * 0.85f : 425.0f,
            300.0f,
            isDashing ? 900.0f : 650.0f);
        const Vec3 end = origin + direction * arrowLength;

        Vec2 from = {};
        Vec2 to = {};
        if (!SDK::Drawing::WorldToScreen(origin, from) ||
            !SDK::Drawing::WorldToScreen(end, to)) {
            return;
        }

        DrawArrowScreen(
            from,
            to,
            isDashing ? kDashDirectionColor : kDirectionColor,
            isDashing ? 3.5f : 2.5f);
    }

    static void DrawTextShadow(float x,
                               float y,
                               std::uint32_t color,
                               const char* text) {
        if (!text || !ImGui::GetCurrentContext()) {
            return;
        }

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        draw->AddText(ImVec2(x + 1.0f, y + 1.0f), ToImColor(0xCC000000u), text);
        draw->AddText(ImVec2(x, y), ToImColor(color), text);
    }

    static void DrawPanelLine(float x,
                              float& y,
                              std::uint32_t color,
                              const char* fmt,
                              ...) {
        char text[256] = {};
        va_list args;
        va_start(args, fmt);
        _vsnprintf_s(text, sizeof(text), _TRUNCATE, fmt, args);
        va_end(args);

        DrawTextShadow(x, y, color, text);
        y += 14.0f;
    }

    static const char* StateText(bool isMoving, bool isDashing) {
        if (isDashing) {
            return "DASHING";
        }
        if (isMoving) {
            return "MOVING";
        }
        return "IDLE";
    }

    static std::uint32_t StateColor(bool isMoving, bool isDashing) {
        if (isDashing) {
            return kDashColor;
        }
        if (isMoving) {
            return kMovingColor;
        }
        return kIdleColor;
    }

    static void DrawPanel(const SDK::AIHeroClient& player,
                          const CoreAiManager::Snapshot& ai,
                          const std::vector<Vec3>& path,
                          bool isMoving,
                          bool isDashing) {
        float x = 18.0f;
        float y = 300.0f;

        DrawPanelLine(x, y, kHeaderColor, "=== Movement Debug ===");
        DrawPanelLine(
            x,
            y,
            StateColor(isMoving, isDashing),
            "State: %s  IsMoving=%d  IsDashing=%d",
            StateText(isMoving, isDashing),
            isMoving ? 1 : 0,
            isDashing ? 1 : 0);
        DrawPanelLine(
            x,
            y,
            kTextColor,
            "MoveSpeed %.1f  DashSpeed %.1f  |vel| %.1f",
            ai.moveSpeed,
            ai.dashSpeed,
            ai.velocity.Length2D());
        DrawPanelLine(
            x,
            y,
            kTextColor,
            "CurSeg/Total: %d / %d  Waypoints: %d",
            ai.currentSegment,
            ai.segmentsCount,
            static_cast<int>(path.size()));
        DrawPanelLine(
            x,
            y,
            kTextColor,
            "ServerPos: %.0f, %.0f, %.0f",
            ai.serverPosition.x,
            ai.serverPosition.y,
            ai.serverPosition.z);
        DrawPanelLine(
            x,
            y,
            kTextColor,
            "PathEnd  : %.0f, %.0f, %.0f",
            ai.targetPosition.x,
            ai.targetPosition.y,
            ai.targetPosition.z);
        DrawPanelLine(
            x,
            y,
            kTextColor,
            "Velocity : %.1f, %.1f, %.1f",
            ai.velocity.x,
            ai.velocity.y,
            ai.velocity.z);
        DrawPanelLine(
            x,
            y,
            0xFFD8D8D8u,
            "Manager: 0x%llX  Player: %s",
            static_cast<unsigned long long>(ai.manager),
            player.CharacterName().c_str());
    }
};

} // namespace Plugins
