#pragma once

#include "../../../../SDK/SDK.h"
#include "../../../../imgui/imgui.h"
#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::Render {

inline void DrawRingImGui(const Vector3& center, float outerRadius, float fadeWidth, ImU32 color, int opacityPercent = 60) {
    auto* draw = Drawing::GetDrawList(true);
    if (!draw) {
        return;
    }

    constexpr int kSegments = 64;
    constexpr int kRings = 12;
    constexpr int kVerticesPerRing = kSegments + 1;
    constexpr int kVertexCount = (kRings + 1) * kVerticesPerRing;
    constexpr int kIndexCount = kRings * kSegments * 6;
    
    ImVec2 vertices[kVertexCount] = {};
    ImU32 colors[kVertexCount] = {};
    
    unsigned char a = (color >> 24) & 0xFF;
    unsigned char r = (color >> 16) & 0xFF;
    unsigned char g = (color >> 8) & 0xFF;
    unsigned char b = color & 0xFF;
    
    const int maxAlpha = std::clamp(static_cast<int>(a) * opacityPercent / 100, 0, 255);
    if (outerRadius <= 0.0f || fadeWidth <= 0.0f || maxAlpha <= 0) {
        return;
    }

    const float innerRadius = std::max(0.0f, outerRadius - fadeWidth);
    for (int ring = 0; ring <= kRings; ++ring) {
        const float progress = static_cast<float>(ring) / static_cast<float>(kRings);
        const float radius = innerRadius + fadeWidth * progress;
        const int alpha = static_cast<int>(std::lround(static_cast<float>(maxAlpha) * progress));
        const ImU32 vertexColor = IM_COL32(r, g, b, alpha);

        for (int segment = 0; segment <= kSegments; ++segment) {
            const float angle =
                2.0f * 3.1415926535f * static_cast<float>(segment) /
                static_cast<float>(kSegments);
            const Vector3 world{
                center.x + std::cos(angle) * radius,
                center.y,
                center.z + std::sin(angle) * radius
            };
            Vec2 screen = {};
            if (!Drawing::WorldToScreenAlways(world, screen) || !screen.IsValid()) {
                return;
            }

            const int index = ring * kVerticesPerRing + segment;
            vertices[index] = ImVec2(screen.x, screen.y);
            colors[index] = vertexColor;
        }
    }

    Drawing::MarkCaptureVisibleContent();
    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    draw->PrimReserve(kIndexCount, kVertexCount);
    const unsigned int baseVertex = draw->_VtxCurrentIdx;
    for (int ring = 0; ring < kRings; ++ring) {
        const int inner = ring * kVerticesPerRing;
        const int outer = (ring + 1) * kVerticesPerRing;
        for (int segment = 0; segment < kSegments; ++segment) {
            const unsigned int a = baseVertex + inner + segment;
            const unsigned int b = a + 1;
            const unsigned int d = baseVertex + outer + segment;
            const unsigned int c = d + 1;
            draw->PrimWriteIdx(static_cast<ImDrawIdx>(a));
            draw->PrimWriteIdx(static_cast<ImDrawIdx>(b));
            draw->PrimWriteIdx(static_cast<ImDrawIdx>(c));
            draw->PrimWriteIdx(static_cast<ImDrawIdx>(a));
            draw->PrimWriteIdx(static_cast<ImDrawIdx>(c));
            draw->PrimWriteIdx(static_cast<ImDrawIdx>(d));
        }
    }
    for (int i = 0; i < kVertexCount; ++i) {
        draw->PrimWriteVtx(vertices[i], uv, colors[i]);
    }
}

} // namespace Plugins::KuroAIO::Render
