#pragma once

namespace OrbwalkerKuro {

using namespace ::SDK;

inline void OrbwalkerBase::DrawAutoAttackRangeFade(const AIHeroClient& player) {
    if (!player.IsValid() || !Drawing::IsEnabled()) {
        return;
    }

    const float outerRadius = GetRealAutoAttackRange(player) + player.BoundingRadius();
    const float fadeWidth = std::min(
        outerRadius,
        static_cast<float>(std::max(1, menu_.AARangeFadeWidth())));
    const int maxAlpha = std::clamp(
        menu_.AARangeFadeOpacityPercent() * 255 / 100,
        0,
        255);
    if (outerRadius <= 0.0f || fadeWidth <= 0.0f || maxAlpha <= 0) {
        return;
    }

    auto* draw = Drawing::GetDrawList(true);
    if (!draw) {
        return;
    }

    // Build a world-space annulus, then project every vertex.  This keeps the
    // fill on the ground plane instead of turning it into a flat screen circle.
    constexpr int kSegments = 64;
    constexpr int kRings = 12;
    constexpr int kVerticesPerRing = kSegments + 1;
    constexpr int kVertexCount = (kRings + 1) * kVerticesPerRing;
    constexpr int kIndexCount = kRings * kSegments * 6;
    ImVec2 vertices[kVertexCount] = {};
    ImU32 colors[kVertexCount] = {};

    unsigned char r = 255, g = 255, b = 255;
    if (IsWindingUp()) {
        // Attack winding up (orange-red)
        r = 255;
        g = 80;
        b = 0;
    } else if (CanAttack()) {
        // Can attack (vibrant green)
        r = 0;
        g = 255;
        b = 100;
    } else {
        // Cooldown (sleek light-blue/cyan)
        r = 150;
        g = 200;
        b = 255;
    }

    const Vector3 center = player.Position();
    const float innerRadius = std::max(0.0f, outerRadius - fadeWidth);
    for (int ring = 0; ring <= kRings; ++ring) {
        const float progress = static_cast<float>(ring) / static_cast<float>(kRings);
        const float radius = innerRadius + fadeWidth * progress;
        const int alpha = static_cast<int>(std::lround(static_cast<float>(maxAlpha) * progress));
        const ImU32 color = IM_COL32(r, g, b, alpha);

        for (int segment = 0; segment <= kSegments; ++segment) {
            const float angle =
                2.0f * 3.14159265358979323846f * static_cast<float>(segment) /
                static_cast<float>(kSegments);
            const Vector3 world{
                center.x + std::cos(angle) * radius,
                center.y,
                center.z + std::sin(angle) * radius
            };
            Vec2 screen = {};
            if (!Drawing::WorldToScreenAlways(world, screen) || !screen.IsValid()) {
                // Avoid joining projected points across the camera plane.
                return;
            }

            const int index = ring * kVerticesPerRing + segment;
            vertices[index] = ImVec2(screen.x, screen.y);
            colors[index] = color;
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

inline void OrbwalkerBase::DrawAzirSoldierRanges(const AIHeroClient& player) {
    if (!OrbwalkingDetail::IsAzirPlayer(player) || !Drawing::IsEnabled()) {
        return;
    }

    bool hasCommandableSoldier = false;
    for (const auto& soldier : OrbwalkingDetail::GetAzirSandSoldiers(player)) {
        if (!soldier.IsValid() || soldier.IsDead()) {
            continue;
        }

        const bool isCommandable = OrbwalkingDetail::IsCommandableAzirSandSoldier(player, soldier);
        const Vector3 pos = soldier.Position();

        // 1. Draw Ring right at soldier base position
        const float ringRadius = soldier.BoundingRadius() > 0.0f ? soldier.BoundingRadius() : 50.0f;
        const ImU32 ringColor = isCommandable ? 0xFF00FFFFu : 0x88888888u; // Gold-Cyan ring when active, gray when out of range
        Drawing::DrawCircle(
            pos,
            ringRadius,
            ringColor,
            2.5f,
            32);

        if (isCommandable) {
            hasCommandableSoldier = true;

            // 2. Draw Auto-Attack Range around soldier (375 range)
            Drawing::DrawCircle(
                pos,
                AzirSoldierSupport::kPrimaryAttackRange,
                0xFF00E5FFu, // Gold/Cyan AA range
                2.0f,
                64);

            // 3. Draw tether line from Azir to soldier
            Drawing::DrawLine(
                player.Position(), pos, 0xAA00E5FFu, 1.5f);
        } else {
            // Out of command range (dimmed circle)
            Drawing::DrawCircle(
                pos,
                AzirSoldierSupport::kPrimaryAttackRange,
                0x55888888u,
                1.0f,
                32);
        }
    }

    // 4. Draw Azir W tether command radius (660 range) around Azir
    if (hasCommandableSoldier) {
        Drawing::DrawCircle(
            player.Position(),
            AzirSoldierSupport::kCommandRadius,
            0x4400E5FFu,
            1.25f,
            72);
    }
}

inline void OrbwalkerBase::TryShowFakeClick(Hud::ClickType type,
                                            const Vector3& position,
                                            int now,
                                            int& lastTick) {
    const bool fakeClickEnabled = menu_.ShowFakeClick();
    const bool fakeCursorEnabled = fakeClickEnabled && menu_.ShowFakeCursor();
    if (!fakeClickEnabled) {
        context_.fakeCursorScreenValid = false;
        return;
    }

    if (!position.IsValid() || position.IsZero()) {
        return;
    }

    const int minDelay = std::max(0, 250 - Game::Ping() * 10);
    if (now - lastTick <= minDelay) {
        return;
    }

    const bool nativeOk = Hud::ShowClick(type, position);
    if (!nativeOk) {
        context_.fakeClickPosition = position;
        context_.fakeClickExpireTick = now + 350;
    }

    if (fakeCursorEnabled) {
        TrackFakeCursorClick(position, now);
    }

    lastTick = now;
}

inline void OrbwalkerBase::TrackFakeCursorClick(const Vector3& position, int now) {
    const bool enabled = menu_.ShowFakeClick() && menu_.ShowFakeCursor();
    if (!enabled) {
        context_.fakeCursorScreenValid = false;
        return;
    }

    if (!position.IsValid() || position.IsZero()) {
        return;
    }

    context_.fakeCursorTargetPosition = position;
    context_.fakeCursorVisibleUntilTick = now + 350;
    context_.fakeCursorScreenValid = false;
}

inline bool OrbwalkerBase::EnsureFakeCursorTexture() {
    if (context_.fakeCursorTexture.Texture) {
        return true;
    }

    const int now = Tick();
    if (context_.fakeCursorTextureLoadTried &&
        now - context_.fakeCursorTextureLastTryTick < 1000) {
        return false;
    }

    context_.fakeCursorTextureLoadTried = true;
    context_.fakeCursorTextureLastTryTick = now;
    context_.fakeCursorTexturePath = Utils::AssetInstaller::CursorHandPath();
    return !context_.fakeCursorTexturePath.empty() &&
           UI::Icons::LoadTextureFromFile(
               context_.fakeCursorTexturePath.c_str(),
               context_.fakeCursorTexture);
}

inline void OrbwalkerBase::DrawFakeCursorFallback(ImDrawList* draw,
                                                  const Vec2& position,
                                                  float size) const {
    if (!draw || !position.IsValid()) {
        return;
    }

    const float s = std::clamp(size, 12.0f, 42.0f);
    const ImVec2 tip(position.x, position.y);
    const ImVec2 left(position.x + s * 0.10f, position.y + s * 1.20f);
    const ImVec2 right(position.x + s * 0.62f, position.y + s * 0.82f);
    const ImVec2 innerTip(position.x + s * 0.08f, position.y + s * 0.10f);
    const ImVec2 innerLeft(position.x + s * 0.16f, position.y + s * 0.98f);
    const ImVec2 innerRight(position.x + s * 0.50f, position.y + s * 0.74f);
    const ImVec2 stemStart(position.x + s * 0.30f, position.y + s * 0.82f);
    const ImVec2 stemEnd(position.x + s * 0.56f, position.y + s * 1.36f);

    draw->AddTriangleFilled(tip, left, right, IM_COL32(0, 0, 0, 220));
    draw->AddTriangleFilled(innerTip, innerLeft, innerRight, IM_COL32(255, 255, 255, 245));
    draw->AddTriangle(tip, left, right, IM_COL32(0, 0, 0, 240), 1.4f);
    draw->AddLine(stemStart, stemEnd, IM_COL32(0, 0, 0, 230), 4.0f);
    draw->AddLine(stemStart, stemEnd, IM_COL32(255, 255, 255, 245), 2.0f);
}

inline void OrbwalkerBase::DrawFakeCursor() {
    const bool enabled = menu_.ShowFakeClick() && menu_.ShowFakeCursor();
    if (!enabled ||
        context_.fakeCursorVisibleUntilTick <= Tick() ||
        !context_.fakeCursorTargetPosition.IsValid() ||
        context_.fakeCursorTargetPosition.IsZero()) {
        context_.fakeCursorScreenValid = false;
        return;
    }

    Vec2 targetScreen = {};
    if (!Drawing::WorldToScreenAlways(context_.fakeCursorTargetPosition, targetScreen) ||
        !Drawing::OnScreenAlways(targetScreen)) {
        context_.fakeCursorScreenValid = false;
        return;
    }

    context_.fakeCursorScreenPosition = targetScreen;
    context_.fakeCursorScreenValid = true;
    Drawing::MarkCaptureVisibleContent(180);

    auto* draw = Drawing::GetDrawList(true);
    if (!draw) {
        return;
    }

    const float size = std::clamp(static_cast<float>(menu_.FakeCursorSize()), 12.0f, 42.0f);
    const bool textureReady = EnsureFakeCursorTexture();
    if (!textureReady) {
        DrawFakeCursorFallback(draw, context_.fakeCursorScreenPosition, size);
        return;
    }

    const float scale = size / 22.0f;
    const Vec2 p = context_.fakeCursorScreenPosition;
    const float width = static_cast<float>(context_.fakeCursorTexture.Width) * scale;
    const float height = static_cast<float>(context_.fakeCursorTexture.Height) * scale;
    draw->AddImage(
        context_.fakeCursorTexture.Texture,
        ImVec2(p.x, p.y),
        ImVec2(p.x + width, p.y + height),
        ImVec2(0.0f, 0.0f),
        ImVec2(1.0f, 1.0f),
        IM_COL32(255, 255, 255, 245));
}

inline void OrbwalkerBase::DrawFakeVisuals() {
    if (menu_.ShowFakeClick() &&
        context_.fakeClickExpireTick > Tick() &&
        context_.fakeClickPosition.IsValid() &&
        !context_.fakeClickPosition.IsZero()) {
        Drawing::DrawCircleAlways(context_.fakeClickPosition, 65.0f, 0xAA66CCFFu, 1.5f, 48);
        Drawing::DrawCircleAlways(context_.fakeClickPosition, 14.0f, 0xCCFFFFFFu, 1.25f, 32);
    }

    DrawFakeCursor();
}

} // namespace OrbwalkerKuro
