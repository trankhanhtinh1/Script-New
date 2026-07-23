#pragma once

using namespace ::SDK;

namespace OrbwalkerKuro {

inline void OrbwalkerBase::DrawAutoAttackRangeFade(const AIHeroClient& player) {
    if (!player.IsValid() || !Drawing::IsEnabled()) {
        return;
    }

    const float outerRadius = GetRealAutoAttackRange(player) + player.BoundingRadius();
    if (outerRadius <= 0.0f) {
        return;
    }

    const bool ready = CanAttack();
    const bool windingUp = IsWindingUp();

    ImU32 color = 0;
    if (windingUp) {
        // Windup state: Orange / Amber
        color = 0xFF0050FFu;
    } else if (ready) {
        // Ready state: Vibrant Green
        color = 0xFF64FF00u;
    } else {
        // Cooldown state: Light Blue / Cyan
        color = 0xFFFFC896u;
    }

    Drawing::DrawCircle(player.Position(), outerRadius, color, 2.0f, 64);
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

        const float ringRadius = soldier.BoundingRadius() > 0.0f ? soldier.BoundingRadius() : 50.0f;

        if (isCommandable) {
            hasCommandableSoldier = true;

            // 1. Base ring for commandable soldier (Cyan/Gold)
            Drawing::DrawCircle(
                pos,
                ringRadius,
                0xFF00E5FFu,
                2.5f,
                32);

            // 2. Auto-Attack Range (350 range) around active soldier
            Drawing::DrawCircle(
                pos,
                AzirSoldierSupport::kPrimaryAttackRange,
                0xFF00E5FFu,
                2.0f,
                64);

            // 3. Active tether line from Azir to soldier
            Drawing::DrawLine(
                player.Position(), pos, 0xAA00E5FFu, 1.5f);
        } else {
            // Non-commandable soldier: Mark clearly with a distinct color (Red/Orange)
            // 1. Base ring for non-commandable soldier
            Drawing::DrawCircle(
                pos,
                ringRadius,
                0xFFFF4444u,
                2.0f,
                32);

            // 2. Auto-Attack Range (350 range) around non-commandable soldier
            Drawing::DrawCircle(
                pos,
                AzirSoldierSupport::kPrimaryAttackRange,
                0x88FF4444u,
                1.5f,
                48);

            // 3. Out-of-range tether line from Azir to non-commandable soldier
            Drawing::DrawLine(
                player.Position(), pos, 0x55FF4444u, 1.0f);
        }
    }

    // Draw Azir W command radius (660 + player bounding radius) around Azir
    Drawing::DrawCircle(
        player.Position(),
        AzirSoldierSupport::kCommandRadius + player.BoundingRadius(),
        hasCommandableSoldier ? 0x4400E5FFu : 0x2200E5FFu,
        1.25f,
        72);
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
