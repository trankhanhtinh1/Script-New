#pragma once

namespace OrbwalkerKuro {

using namespace ::SDK;

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
