#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../sdk/GameObjects/YasuoWallTracker.h"
#include "../../imgui/imgui.h"

#include <cstdio>
#include <cstdint>

namespace Plugins {

class YasuoWallDebugPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Yasuo Wall Debug"; }
    const char* GetInternalId() const override { return "utility.yasuo_wall_debug"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        SDK::YasuoWallTracker::EnsureInitialized();
        NightSharpDebug::Logf("[YasuoWallDebug] loaded");
    }

    void OnUnload() override {
        NightSharpDebug::Logf("[YasuoWallDebug] unloaded");
    }

    void OnUpdate() override {}

    void OnRender() override {
        if (!drawEnabled_ ||
            !ImGui::GetCurrentContext() ||
            !SDK::Drawing::IsEnabled()) {
            return;
        }

        for (const auto& wall : SDK::YasuoWallTracker::ActiveWalls()) {
            DrawWall(wall);
        }
    }

    void OnMenu() override {
        ImGui::Checkbox("Draw Yasuo walls", &drawEnabled_);
        const auto walls = SDK::YasuoWallTracker::ActiveWalls();
        ImGui::Text("Active walls: %d", static_cast<int>(walls.size()));
    }

private:
    bool drawEnabled_ = true;

    static void DrawTextWorld(
        const Vec3& world,
        const char* text,
        std::uint32_t color,
        float offsetY) {
        Vec2 screen = {};
        if (SDK::Drawing::WorldToScreen(world, screen)) {
            SDK::Drawing::DrawText(
                screen.x,
                screen.y + offsetY,
                color,
                text);
        }
    }

    static void DrawWall(
        const SDK::YasuoWallTracker::WallSnapshot& wall) {
        SDK::Drawing::DrawLine(
            wall.start,
            wall.end,
            kWallColor,
            5.0f);
        SDK::Drawing::DrawCircleAlways(
            wall.start,
            30.0f,
            kEndpointColor,
            2.0f,
            24);
        SDK::Drawing::DrawCircleAlways(
            wall.end,
            30.0f,
            kEndpointColor,
            2.0f,
            24);
        SDK::Drawing::DrawCircleAlways(
            wall.center,
            40.0f,
            kCenterColor,
            2.0f,
            24);

        char label[160] = {};
        std::snprintf(
            label,
            sizeof(label),
            "YasuoWall L%d span=%.1f main=%u A=%u B=%u",
            wall.level,
            wall.Span(),
            wall.main.networkId,
            wall.endpointA.networkId,
            wall.endpointB.networkId);
        DrawTextWorld(wall.center, label, kTextColor, -18.0f);

        // Ownership readout. `team` is what the tracker resolved for the caster
        // and is what collision filters on; `raw` is the emitter's own team byte,
        // kept visible because it reads 0 on every windwall object and is the
        // reason ownership has to come from the co-located visual instead.
        DrawTextWorld(wall.center, BuildTeamLabel(wall), kTeamTextColor, -34.0f);
    }

    static const char* BuildTeamLabel(
        const SDK::YasuoWallTracker::WallSnapshot& wall) {
        static char label[240] = {};

        const auto player = SDK::GameObjects::Player();
        const int playerTeam = player.IsValid()
            ? static_cast<int>(player.Team())
            : -1;

        // Nearest Yasuo, i.e. the presumed caster.
        int yasuoTeam = -1;
        int yasuoEnemy = -1;
        float yasuoDistance = -1.0f;
        for (const auto& hero : SDK::GameObjects::Heroes()) {
            if (!hero.IsValid() ||
                _stricmp(hero.CharacterName().c_str(), "Yasuo") != 0) {
                continue;
            }
            const float distance = hero.Position().Distance(wall.center);
            if (yasuoDistance < 0.0f || distance < yasuoDistance) {
                yasuoDistance = distance;
                yasuoTeam = static_cast<int>(hero.Team());
                yasuoEnemy = hero.IsEnemy() ? 1 : 0;
            }
        }

        const std::uint32_t localTeam = SDK::YasuoWallTracker::LocalTeam();
        const bool blocksOurCasts = SDK::YasuoWallTracker::WallMatchesOwner(
            wall,
            SDK::YasuoWallTracker::WallOwner::Enemy);

        std::snprintf(
            label,
            sizeof(label),
            "team=%u local=%u %s | raw m=%u a=%u b=%u | player=%d | yasuo team=%d enemy=%d d=%.0f",
            wall.team,
            localTeam,
            wall.team == 0 ? "UNKNOWN"
                           : (blocksOurCasts ? "ENEMY-blocks-us" : "ALLY-passes"),
            ::Core::Objects::ReadTeam(wall.main.address),
            ::Core::Objects::ReadTeam(wall.endpointA.address),
            ::Core::Objects::ReadTeam(wall.endpointB.address),
            playerTeam,
            yasuoTeam,
            yasuoEnemy,
            yasuoDistance);
        return label;
    }

    static constexpr std::uint32_t kWallColor = 0xFFFF5050u;
    static constexpr std::uint32_t kEndpointColor = 0xFF50FF50u;
    static constexpr std::uint32_t kCenterColor = 0xFFFFD24Au;
    static constexpr std::uint32_t kTextColor = 0xFFFFFFFFu;
    static constexpr std::uint32_t kTeamTextColor = 0xFF4AD2FFu;
};

} // namespace Plugins
