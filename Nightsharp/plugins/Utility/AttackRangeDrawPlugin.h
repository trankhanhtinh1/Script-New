#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace Plugins {

class AttackRangeDrawPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Attack Range Draw Test"; }
    const char* GetInternalId() const override { return "utility.attack_range_draw_test"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        NightSharpDebug::Logf("[AttackRangeDraw] loaded");
    }

    void OnUnload() override {
        NightSharpDebug::Logf("[AttackRangeDraw] unloaded");
    }

    void OnRender() override {
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) {
            return;
        }

        const Vec3 position = player.Position();
        float drawRange = SDK::Utils::AutoAttack::GetRealAutoAttackRange(player);

        if (!IsSaneRange(drawRange)) {
            drawRange = player.AttackRange();
        }

        if (!position.IsValid() || position.IsZero() || !IsSaneRange(drawRange)) {
            return;
        }

        DrawWorldCircle(position, drawRange, 0xAA66FF66u, 2.0f);
        DrawLabel(position, drawRange);
    }

private:
    static bool IsSaneRange(float value) {
        return std::isfinite(value) && value > 0.0f && value < 5000.0f;
    }

    static void DrawWorldCircle(const Vec3& center,
                                float radius,
                                std::uint32_t color,
                                float thickness) {
        SDK::Drawing::DrawCircle(center, radius, color, thickness, 64);
    }

    static void DrawLabel(const Vec3& center, float drawRange) {
        Vec2 screen{};
        if (!SDK::Drawing::WorldToScreen(center, screen)) {
            return;
        }

        char text[128] = {};
        std::snprintf(
            text,
            sizeof(text),
            "AA %.0f",
            drawRange);
        SDK::Drawing::DrawText(screen.x + 12.0f, screen.y - 24.0f, 0xFFFFFFFFu, text);
    }
};

} // namespace Plugins
