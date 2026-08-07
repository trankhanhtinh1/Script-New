#pragma once

#include "../../EvadeTextureAssets.inl"
#include "../../../../../SDK/UI/Icons.h"
#include "../../../../../SDK/SDK.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroEvade {

struct EvadeTextureCache {
    SDK::UI::Icons::LoadedTexture Texture = {};
    bool Loaded = false;
};

inline EvadeTextureCache g_evadeTextureCache[3] = {};

inline EvadeTextureCache* EnsureEvadeTextureLoaded(int index) {
    if (index < 0 || index >= 3) return nullptr;
    EvadeTextureCache& cache = g_evadeTextureCache[index];
    if (cache.Loaded) return &cache;

    const auto& embedded = kEmbeddedEvadeTextures[index];
    if (embedded.bytes && embedded.size > 0) {
        SDK::UI::Icons::ImagePixels pixels{};
        if (SDK::UI::Icons::LoadPixelsFromMemory(embedded.bytes, static_cast<int>(embedded.size), pixels)) {
            cache.Texture = SDK::UI::Icons::CreateTextureFromPixels(pixels);
            cache.Loaded = (cache.Texture.Texture != nullptr);
            if (cache.Loaded) return &cache;
        }
    }
    return nullptr;
}

inline void DrawCircleSkillshotTexture(ImDrawList* draw,
                                       const Vec2& center2D,
                                       float radius,
                                       ImU32 tintColor,
                                       int textureIndex = 0) {
    if (!draw || radius <= 0.0f) return;

    EvadeTextureCache* texCache = EnsureEvadeTextureLoaded(textureIndex);
    if (!texCache || !texCache->Loaded || !texCache->Texture.Texture) {
        return;
    }

    constexpr int kGridSize = 16;
    const float diameter = radius * 2.0f;
    const float step = diameter / static_cast<float>(kGridSize);
    const float startX = center2D.x - radius;
    const float startZ = center2D.y - radius;

    struct GridNode {
        ImVec2 screen;
        ImVec2 uv;
        bool valid;
    };

    GridNode grid[kGridSize + 1][kGridSize + 1];

    for (int row = 0; row <= kGridSize; ++row) {
        const float v = static_cast<float>(row) / static_cast<float>(kGridSize);
        const float z = startZ + step * static_cast<float>(row);
        for (int col = 0; col <= kGridSize; ++col) {
            const float u = static_cast<float>(col) / static_cast<float>(kGridSize);
            const float x = startX + step * static_cast<float>(col);

            const float y = SDK::NavMesh::GetHeightForPosition(center2D.x, center2D.y);
            Vector3 worldPos(x, y, z);
            Vector2 screenPos;
            bool onScreen = SDK::Drawing::WorldToScreen(worldPos, screenPos);

            grid[row][col].screen = ImVec2(screenPos.x, screenPos.y);
            grid[row][col].uv = ImVec2(u, v);
            grid[row][col].valid = onScreen && screenPos.x >= -200.0f && screenPos.y >= -200.0f;
        }
    }

    for (int row = 0; row < kGridSize; ++row) {
        for (int col = 0; col < kGridSize; ++col) {
            const auto& n0 = grid[row][col];         // Top-Left
            const auto& n1 = grid[row][col + 1];     // Top-Right
            const auto& n2 = grid[row + 1][col + 1]; // Bottom-Right
            const auto& n3 = grid[row + 1][col];     // Bottom-Left

            if (n0.valid && n1.valid && n2.valid && n3.valid) {
                draw->AddImageQuad(
                    texCache->Texture.Texture,
                    n0.screen, n1.screen, n2.screen, n3.screen,
                    n0.uv, n1.uv, n2.uv, n3.uv,
                    tintColor);
            }
        }
    }
}

} // namespace Plugins::KuroEvade
