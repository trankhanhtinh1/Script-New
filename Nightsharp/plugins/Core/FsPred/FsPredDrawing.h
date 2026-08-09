#pragma once

#include "FsPredCrestAssets.inl"
#include "FsPredEngine.h"
#include "FsPredUnitTracker.h"
#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/UI/Drawing.h"
#include "../../../sdk/UI/Icons.h"
#include "../../../SectionProfiler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace Plugins::FsPred {
inline constexpr std::size_t kCrestCount =
    sizeof(CrestAssets::kCrests) / sizeof(CrestAssets::kCrests[0]);


inline constexpr int CrestIndexForMotionState(SDK::HitChance state) {
    switch (state) {
    case SDK::HitChance::Low:
        return 0;
    case SDK::HitChance::Medium:
        return 1;
    case SDK::HitChance::High:
        return 2;
    case SDK::HitChance::VeryHigh:
    case SDK::HitChance::Immobile:
    case SDK::HitChance::Dash:
        return 3;
    default:
        return -1;
    }
}

inline constexpr std::uint32_t CrestFallbackColor(int index) {
    constexpr std::array<std::uint32_t, 4> kColors{
        0xFFE85D5Du,
        0xFFF2B84Bu,
        0xFF5BC0EBu,
        0xFF70E0A0u,
    };
    return index >= 0 && index < static_cast<int>(kColors.size())
        ? kColors[static_cast<std::size_t>(index)]
        : 0xFFFFFFFFu;
}

class CrestDrawing {
public:
    void Update(
        const AntiBaitWeights& weights,
        bool antiBaitEnabled) {
        UnitTracker::RefreshCachedMotionStates(
            weights,
            antiBaitEnabled);

        std::array<bool, kCrestCount> needed{};
        for (const auto& hero : SDK::GameObjects::EnemyHeroesFrame()) {
            if (!IsEligible(hero)) {
                continue;
            }

            SDK::HitChance state = SDK::HitChance::None;
            if (!UnitTracker::TryGetCachedMotionState(
                    hero.NetworkId(), state)) {
                continue;
            }
            const int index = CrestIndexForMotionState(state);
            if (index >= 0) {
                needed[static_cast<std::size_t>(index)] = true;
            }
        }

        for (std::size_t index = 0; index < needed.size(); ++index) {
            if (needed[index]) {
                PrepareTexture(index);
            }
        }
    }

    void Render() const {
        NS_PROFILE("FsPred.Draw");
        if (!SDK::Drawing::IsEnabled()) {
            return;
        }
        ImDrawList* draw = SDK::Drawing::GetDrawList(true);
        if (!draw) {
            return;
        }

        for (const auto& hero : SDK::GameObjects::EnemyHeroesFrame()) {
            if (!IsEligible(hero)) {
                continue;
            }

            const SDK::Vector3 center = RenderedPosition(hero);
            if (!center.IsValid() || center.IsZero()) {
                continue;
            }

            SDK::Vector2 centerScreen{};
            if (!SDK::Drawing::WorldToScreen(center, centerScreen) ||
                !SDK::Drawing::OnScreen(centerScreen)) {
                continue;
            }

            SDK::HitChance state = SDK::HitChance::None;
            if (!UnitTracker::TryGetCachedMotionState(
                    hero.NetworkId(), state)) {
                continue;
            }
            const int crestIndex = CrestIndexForMotionState(state);
            if (crestIndex < 0) {
                continue;
            }

            const float radius = hero.BoundingRadius() * 0.5f;
            const TextureSlot& slot =
                slots_[static_cast<std::size_t>(crestIndex)];
            if (slot.State != TextureState::Ready ||
                !slot.Texture.Texture) {
                SDK::Drawing::DrawCircle(
                    center,
                    radius,
                    CrestFallbackColor(crestIndex),
                    1.5f,
                    40,
                    true);
                continue;
            }

            const float visibleRatio =
                CrestAssets::kCrests[static_cast<std::size_t>(crestIndex)]
                    .VisibleRadiusRatio;
            if (!std::isfinite(visibleRatio) || visibleRatio <= 0.0f) {
                continue;
            }
            const float halfExtent = radius / visibleRatio;
            const std::array<SDK::Vector3, 4> worldCorners{
                SDK::Vector3{
                    center.x - halfExtent, center.y, center.z - halfExtent},
                SDK::Vector3{
                    center.x + halfExtent, center.y, center.z - halfExtent},
                SDK::Vector3{
                    center.x + halfExtent, center.y, center.z + halfExtent},
                SDK::Vector3{
                    center.x - halfExtent, center.y, center.z + halfExtent},
            };
            std::array<ImVec2, 4> screenCorners{};
            bool projected = true;
            for (std::size_t index = 0; index < worldCorners.size(); ++index) {
                SDK::Vector2 screen{};
                if (!SDK::Drawing::WorldToScreen(worldCorners[index], screen) ||
                    !screen.IsValid()) {
                    projected = false;
                    break;
                }
                screenCorners[index] = ImVec2(screen.x, screen.y);
            }
            if (!projected) {
                continue;
            }

            draw->AddImageQuad(
                slot.Texture.Texture,
                screenCorners[0],
                screenCorners[1],
                screenCorners[2],
                screenCorners[3],
                ImVec2(0.0f, 0.0f),
                ImVec2(1.0f, 0.0f),
                ImVec2(1.0f, 1.0f),
                ImVec2(0.0f, 1.0f),
                IM_COL32_WHITE);
        }
    }

    void RenderPredictedPositions(
        const FsPredEngine& engine) const {
        if (!SDK::Drawing::IsEnabled()) {
            return;
        }

        const int now = SDK::Variables::TickCount();
        for (const FsPredDebugPrediction& record :
             engine.DebugPredictions()) {
            const int age = now - record.Tick;
            if (record.NetworkId == 0 ||
                age < 0 ||
                age > 250 ||
                !record.CastPosition.IsValid() ||
                record.CastPosition.IsZero() ||
                !record.UnitPosition.IsValid() ||
                record.UnitPosition.IsZero()) {
                continue;
            }

            const int crestIndex =
                CrestIndexForMotionState(record.Hitchance);
            const std::uint32_t color = crestIndex >= 0
                ? CrestFallbackColor(crestIndex)
                : 0xFFE85D5Du;
            SDK::Drawing::DrawLine(
                record.UnitPosition,
                record.CastPosition,
                color,
                1.5f,
                true);
            SDK::Drawing::DrawCircle(
                record.UnitPosition,
                24.0f,
                0xFFF2D95Cu,
                1.5f,
                28,
                true);
            SDK::Drawing::DrawCircle(
                record.CastPosition,
                30.0f,
                color,
                2.0f,
                32,
                true);

            const char* slotLabel = "?";
            switch (record.SlotIndex) {
            case 0: slotLabel = "Q"; break;
            case 1: slotLabel = "W"; break;
            case 2: slotLabel = "E"; break;
            case 3: slotLabel = "R"; break;
            default: break;
            }
            SDK::Drawing::DrawText(
                record.CastPosition,
                slotLabel,
                color,
                true,
                true);
        }
    }

    void Shutdown() {
        for (TextureSlot& slot : slots_) {
            SDK::UI::Icons::ReleaseTexture(slot.Texture);
            slot = {};
        }
    }

private:
    enum class TextureState : std::uint8_t {
        Uninitialized,
        WaitingForDevice,
        Ready,
        Failed,
    };

    struct TextureSlot {
        SDK::UI::Icons::LoadedTexture Texture{};
        SDK::UI::Icons::ImagePixels RetryPixels{};
        TextureState State = TextureState::Uninitialized;
        int RetryDeadlineTick = 0;
    };

    static bool IsEligible(const SDK::AIHeroClient& hero) {
        const float radius = hero.BoundingRadius();
        return hero.IsValid() && hero.IsEnemy() && !hero.IsDead() &&
               !hero.IsZombie() && hero.IsVisible() && hero.IsTargetable() &&
               std::isfinite(radius) && radius > 0.0f;
    }

    static SDK::Vector3 RenderedPosition(const SDK::AIHeroClient& hero) {
        SDK::Vector3 position = hero.Position();
        if (!position.IsValid() || position.IsZero()) {
            position = hero.ServerPosition();
        }
        return position;
    }

    void PrepareTexture(std::size_t index) {
        TextureSlot& slot = slots_[index];
        if (slot.State == TextureState::Ready ||
            slot.State == TextureState::Failed) {
            return;
        }

        const int now = SDK::Variables::TickCount();
        if (slot.State == TextureState::WaitingForDevice) {
            if (now - slot.RetryDeadlineTick > 0) {
                slot.RetryPixels = {};
                slot.State = TextureState::Failed;
                return;
            }
            if (!SDK::UI::Icons::detail::Device()) {
                return;
            }
            UploadTexture(slot, slot.RetryPixels);
            slot.RetryPixels = {};
            return;
        }

        const CrestAssets::CrestAssetDescriptor& asset =
            CrestAssets::kCrests[index];
        if (!asset.Bytes || asset.Size == 0 ||
            asset.Size > static_cast<std::size_t>(
                             (std::numeric_limits<int>::max)())) {
            slot.State = TextureState::Failed;
            return;
        }

        SDK::UI::Icons::ImagePixels pixels{};
        if (!SDK::UI::Icons::LoadPixelsFromMemory(
                asset.Bytes,
                static_cast<int>(asset.Size),
                pixels) ||
            !pixels.IsValid()) {
            slot.State = TextureState::Failed;
            return;
        }

        if (!SDK::UI::Icons::detail::Device()) {
            slot.RetryPixels = std::move(pixels);
            slot.RetryDeadlineTick = now + 1000;
            slot.State = TextureState::WaitingForDevice;
            return;
        }

        UploadTexture(slot, pixels);
    }

    static void UploadTexture(
        TextureSlot& slot,
        const SDK::UI::Icons::ImagePixels& pixels) {
        slot.Texture = SDK::UI::Icons::CreateTextureFromPixels(pixels);
        slot.State = slot.Texture.Texture
            ? TextureState::Ready
            : TextureState::Failed;
    }

    std::array<TextureSlot, kCrestCount> slots_{};
};

} // namespace Plugins::FsPred
