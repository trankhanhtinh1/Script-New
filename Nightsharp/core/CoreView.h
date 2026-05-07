#pragma once

#include "CoreRuntime.h"
#include "Vector.h"
#include "../imgui/imgui.h"

#include <cstdint>

namespace CoreView {

    struct ScreenPoint {
        Vec2 position = {};
        bool onScreen = false;
    };

    inline bool GetRendererSize(Vec2& out);
    inline bool ReadViewProjection(float out[16]);
    inline Vec2 GetMouseScreenPos();

    inline bool IsFiniteFloat(float value) {
        return value == value && value > -3.4e38f && value < 3.4e38f;
    }

    inline bool IsPlausibleScreenVec2(const Vec2& value) {
        if (!IsFiniteFloat(value.x) || !IsFiniteFloat(value.y)) {
            return false;
        }

        Vec2 size = {};
        if (GetRendererSize(size)) {
            const float maxX = (size.x > 0.0f ? size.x * 2.0f : 8192.0f);
            const float maxY = (size.y > 0.0f ? size.y * 2.0f : 8192.0f);
            return value.x >= -maxX && value.x <= maxX && value.y >= -maxY && value.y <= maxY;
        }

        return value.x >= -8192.0f && value.x <= 8192.0f && value.y >= -8192.0f && value.y <= 8192.0f;
    }

    inline bool IsPlausibleWorldVec3(const Vec3& value) {
        if (!IsFiniteFloat(value.x) || !IsFiniteFloat(value.y) || !IsFiniteFloat(value.z)) {
            return false;
        }
        return value.x >= -30000.0f && value.x <= 30000.0f &&
               value.y >= -5000.0f && value.y <= 5000.0f &&
               value.z >= -30000.0f && value.z <= 30000.0f;
    }

    inline float AbsFloat(float value) {
        return value < 0.0f ? -value : value;
    }

    inline bool InvertMatrix4(const float m[16], float out[16]) {
        float inv[16] = {};

        inv[0] = m[5]  * m[10] * m[15] -
                 m[5]  * m[11] * m[14] -
                 m[9]  * m[6]  * m[15] +
                 m[9]  * m[7]  * m[14] +
                 m[13] * m[6]  * m[11] -
                 m[13] * m[7]  * m[10];

        inv[4] = -m[4]  * m[10] * m[15] +
                  m[4]  * m[11] * m[14] +
                  m[8]  * m[6]  * m[15] -
                  m[8]  * m[7]  * m[14] -
                  m[12] * m[6]  * m[11] +
                  m[12] * m[7]  * m[10];

        inv[8] = m[4]  * m[9] * m[15] -
                 m[4]  * m[11] * m[13] -
                 m[8]  * m[5] * m[15] +
                 m[8]  * m[7] * m[13] +
                 m[12] * m[5] * m[11] -
                 m[12] * m[7] * m[9];

        inv[12] = -m[4]  * m[9] * m[14] +
                   m[4]  * m[10] * m[13] +
                   m[8]  * m[5] * m[14] -
                   m[8]  * m[6] * m[13] -
                   m[12] * m[5] * m[10] +
                   m[12] * m[6] * m[9];

        inv[1] = -m[1]  * m[10] * m[15] +
                  m[1]  * m[11] * m[14] +
                  m[9]  * m[2] * m[15] -
                  m[9]  * m[3] * m[14] -
                  m[13] * m[2] * m[11] +
                  m[13] * m[3] * m[10];

        inv[5] = m[0]  * m[10] * m[15] -
                 m[0]  * m[11] * m[14] -
                 m[8]  * m[2] * m[15] +
                 m[8]  * m[3] * m[14] +
                 m[12] * m[2] * m[11] -
                 m[12] * m[3] * m[10];

        inv[9] = -m[0]  * m[9] * m[15] +
                  m[0]  * m[11] * m[13] +
                  m[8]  * m[1] * m[15] -
                  m[8]  * m[3] * m[13] -
                  m[12] * m[1] * m[11] +
                  m[12] * m[3] * m[9];

        inv[13] = m[0]  * m[9] * m[14] -
                  m[0]  * m[10] * m[13] -
                  m[8]  * m[1] * m[14] +
                  m[8]  * m[2] * m[13] +
                  m[12] * m[1] * m[10] -
                  m[12] * m[2] * m[9];

        inv[2] = m[1]  * m[6] * m[15] -
                 m[1]  * m[7] * m[14] -
                 m[5]  * m[2] * m[15] +
                 m[5]  * m[3] * m[14] +
                 m[13] * m[2] * m[7] -
                 m[13] * m[3] * m[6];

        inv[6] = -m[0]  * m[6] * m[15] +
                  m[0]  * m[7] * m[14] +
                  m[4]  * m[2] * m[15] -
                  m[4]  * m[3] * m[14] -
                  m[12] * m[2] * m[7] +
                  m[12] * m[3] * m[6];

        inv[10] = m[0]  * m[5] * m[15] -
                  m[0]  * m[7] * m[13] -
                  m[4]  * m[1] * m[15] +
                  m[4]  * m[3] * m[13] +
                  m[12] * m[1] * m[7] -
                  m[12] * m[3] * m[5];

        inv[14] = -m[0]  * m[5] * m[14] +
                   m[0]  * m[6] * m[13] +
                   m[4]  * m[1] * m[14] -
                   m[4]  * m[2] * m[13] -
                   m[12] * m[1] * m[6] +
                   m[12] * m[2] * m[5];

        inv[3] = -m[1] * m[6] * m[11] +
                  m[1] * m[7] * m[10] +
                  m[5] * m[2] * m[11] -
                  m[5] * m[3] * m[10] -
                  m[9] * m[2] * m[7] +
                  m[9] * m[3] * m[6];

        inv[7] = m[0] * m[6] * m[11] -
                 m[0] * m[7] * m[10] -
                 m[4] * m[2] * m[11] +
                 m[4] * m[3] * m[10] +
                 m[8] * m[2] * m[7] -
                 m[8] * m[3] * m[6];

        inv[11] = -m[0] * m[5] * m[11] +
                   m[0] * m[7] * m[9] +
                   m[4] * m[1] * m[11] -
                   m[4] * m[3] * m[9] -
                   m[8] * m[1] * m[7] +
                   m[8] * m[3] * m[5];

        inv[15] = m[0] * m[5] * m[10] -
                  m[0] * m[6] * m[9] -
                  m[4] * m[1] * m[10] +
                  m[4] * m[2] * m[9] +
                  m[8] * m[1] * m[6] -
                  m[8] * m[2] * m[5];

        float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
        if (AbsFloat(det) < 0.000001f) {
            return false;
        }

        det = 1.0f / det;
        for (int i = 0; i < 16; ++i) {
            out[i] = inv[i] * det;
        }
        return true;
    }

    inline bool UnprojectNdc(const Vec3& ndc, const float inv[16], Vec3& out) {
        const Vec4 clip(ndc.x, ndc.y, ndc.z, 1.0f);
        Vec4 world = {};
        world.x = clip.x * inv[0] + clip.y * inv[4] + clip.z * inv[8]  + clip.w * inv[12];
        world.y = clip.x * inv[1] + clip.y * inv[5] + clip.z * inv[9]  + clip.w * inv[13];
        world.z = clip.x * inv[2] + clip.y * inv[6] + clip.z * inv[10] + clip.w * inv[14];
        world.w = clip.x * inv[3] + clip.y * inv[7] + clip.z * inv[11] + clip.w * inv[15];
        if (AbsFloat(world.w) < 0.000001f) {
            out = {};
            return false;
        }

        const float invW = 1.0f / world.w;
        out = { world.x * invW, world.y * invW, world.z * invW };
        return IsPlausibleWorldVec3(out);
    }

    inline bool ScreenToWorldOnPlane(const Vec2& screen, float planeY, Vec3& out) {
        float matrix[16] = {};
        float inv[16] = {};
        Vec2 size = {};
        if (!IsPlausibleScreenVec2(screen) ||
            !ReadViewProjection(matrix) ||
            !GetRendererSize(size) ||
            size.x <= 0.0f ||
            size.y <= 0.0f ||
            !InvertMatrix4(matrix, inv)) {
            out = {};
            return false;
        }

        const float ndcX = (screen.x / size.x) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (screen.y / size.y) * 2.0f;

        Vec3 nearPoint = {};
        Vec3 farPoint = {};
        if (!UnprojectNdc({ ndcX, ndcY, 0.0f }, inv, nearPoint) ||
            !UnprojectNdc({ ndcX, ndcY, 1.0f }, inv, farPoint)) {
            out = {};
            return false;
        }

        const Vec3 dir = farPoint - nearPoint;
        if (AbsFloat(dir.y) < 0.0001f) {
            out = farPoint;
            return IsPlausibleWorldVec3(out);
        }

        const float t = (planeY - nearPoint.y) / dir.y;
        out = nearPoint + dir * t;
        out.y = planeY;
        return IsPlausibleWorldVec3(out);
    }

    inline bool GetRendererSize(Vec2& out) {
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        if (displaySize.x > 0.0f && displaySize.y > 0.0f) {
            out = { displaySize.x, displaySize.y };
            return true;
        }

        const auto renderer = CoreRuntime::GetContext().renderer;
        if (!Globals::IsValidPtr(renderer)) {
            out = {};
            return false;
        }

        const int width = Globals::Read<int>(renderer + 0xC);
        const int height = Globals::Read<int>(renderer + 0x10);
        if (width <= 0 || height <= 0) {
            out = {};
            return false;
        }

        out = { static_cast<float>(width), static_cast<float>(height) };
        return true;
    }

    inline uintptr_t GetViewport() {
        const auto& ctx = CoreRuntime::GetContext();
        if (Globals::IsValidPtr(ctx.viewPort)) {
            return ctx.viewPort;
        }
        if (Globals::IsValidPtr(ctx.viewPort2)) {
            return ctx.viewPort2;
        }
        return 0;
    }

    inline uintptr_t GetViewportW2S() {
        const auto viewport = GetViewport();
        return Globals::IsValidPtr(viewport) ? (viewport + Offset::HudRuntime::ViewportW2S) : 0;
    }

    inline uintptr_t GetHudSpellInfo() {
        const auto hud = CoreRuntime::GetContext().hudInstance;
        if (!Globals::IsValidPtr(hud)) {
            return 0;
        }
        return Globals::Read<uintptr_t>(hud + Offset::HudRuntime::SpellInfo);
    }

    inline bool ReadViewProjection(float out[16]) {
        const auto inst = CoreRuntime::GetContext().viewProjInstance;
        if (!Globals::IsValidPtr(inst) || !out) {
            return false;
        }

        float view[16] = {};
        float proj[16] = {};
        __try {
            for (int i = 0; i < 16; ++i) {
                view[i] = Globals::Read<float>(inst + static_cast<uintptr_t>(i * sizeof(float)));
                proj[i] = Globals::Read<float>(inst + Offset::DrawingMatrixRuntime::ProjMatrixRelative + static_cast<uintptr_t>(i * sizeof(float)));
            }
        }
        __except (1) {
            return false;
        }

        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += view[row * 4 + k] * proj[k * 4 + col];
                }
                out[row * 4 + col] = sum;
            }
        }

        return true;
    }

    inline bool WorldToScreenCall(const Vec3& world, Vec2& screen) {
        const auto fn = CoreRuntime::GetContext().worldToScreenFn;
        const auto viewportW2S = GetViewportW2S();
        if (!Globals::IsValidPtr(viewportW2S) || !fn) {
            screen = {};
            return false;
        }

        // Current generated RVA for WorldToScreen is only boundary-validated, not
        // ABI-approved for the 3-arg fastcall used below. Calling the wrong
        // target every frame is extremely expensive due to repeated exceptions
        // and can tank FPS, so keep the native path disabled until the callable
        // contract is re-proven on the live build.
        screen = {};
        return false;

        using fnWorldToScreen = bool(__fastcall*)(uintptr_t, const Vec3*, Vec3*);
        Vec3 out = {};
        bool ok = false;
        __try {
            ok = reinterpret_cast<fnWorldToScreen>(fn)(viewportW2S, &world, &out);
        }
        __except (1) {
            ok = false;
        }

        if (!ok) {
            screen = {};
            return false;
        }

        screen = { out.x, out.y };
        return screen.IsValid();
    }

    inline bool ProjectWorldToScreen(const Vec3& world,
                                     const float matrix[16],
                                     const Vec2& size,
                                     Vec2& screen) {
        Vec4 clip = {};
        clip.x = world.x * matrix[0] + world.y * matrix[4] + world.z * matrix[8] + matrix[12];
        clip.y = world.x * matrix[1] + world.y * matrix[5] + world.z * matrix[9] + matrix[13];
        clip.z = world.x * matrix[2] + world.y * matrix[6] + world.z * matrix[10] + matrix[14];
        clip.w = world.x * matrix[3] + world.y * matrix[7] + world.z * matrix[11] + matrix[15];

        if (clip.w < 0.01f || size.x <= 0.0f || size.y <= 0.0f) {
            screen = {};
            return false;
        }

        const Vec3 ndc = { clip.x / clip.w, clip.y / clip.w, clip.z / clip.w };
        screen.x = (size.x * 0.5f) * (1.0f + ndc.x);
        screen.y = (size.y * 0.5f) * (1.0f - ndc.y);
        if (screen.IsValid()) {
            return true;
        }

        screen = {};
        return false;
    }

    inline bool WorldToScreen(const Vec3& world, Vec2& screen) {
        float matrix[16] = {};
        Vec2 size = {};
        if (!ReadViewProjection(matrix) || !GetRendererSize(size)) {
            screen = {};
            return false;
        }

        if (ProjectWorldToScreen(world, matrix, size, screen)) {
            return true;
        }

        return WorldToScreenCall(world, screen);
    }

    inline ScreenPoint ToScreen(const Vec3& world) {
        ScreenPoint pt = {};
        pt.onScreen = WorldToScreen(world, pt.position);
        return pt;
    }

    inline Vec3 GetMouseWorldPos() {
        const auto hud = CoreRuntime::GetContext().hudInstance;
        if (Globals::IsValidPtr(hud)) {
            const auto input = Globals::Read<uintptr_t>(hud + Offset::HudRuntime::Input);
            if (Globals::IsValidPtr(input)) {
                const Vec3 candidate = Globals::Read<Vec3>(input + Offset::HudRuntime::MouseWorldPos);
                if (!candidate.IsZero() && IsPlausibleWorldVec3(candidate)) {
                    return candidate;
                }
            }
        }

        const Vec2 screen = GetMouseScreenPos();
        Vec3 fallback = {};
        if (ScreenToWorldOnPlane(screen, 0.0f, fallback)) {
            return fallback;
        }

        return {};
    }

    inline Vec2 GetMouseScreenPos() {
        const auto mouseVec2 = CoreRuntime::GetContext().mouseScreenVec2;
        if (Globals::IsValidPtr(mouseVec2)) {
            const Vec2 candidate = Globals::Read<Vec2>(mouseVec2);
            if (IsPlausibleScreenVec2(candidate)) {
                return candidate;
            }
        }

        const auto cursor = CoreRuntime::GetContext().cursorInstance;
        if (Globals::IsValidPtr(cursor)) {
            const Vec2 candidate = Globals::Read<Vec2>(cursor + 0x2C);
            if (IsPlausibleScreenVec2(candidate)) {
                return candidate;
            }
        }

        return {};
    }

    inline uint32_t GetSelectedNetId() {
        const auto hud = CoreRuntime::GetContext().hudInstance;
        if (!Globals::IsValidPtr(hud)) {
            return 0;
        }

        const auto input = Globals::Read<uintptr_t>(hud + Offset::HudRuntime::Input);
        if (!Globals::IsValidPtr(input)) {
            return 0;
        }

        return Globals::Read<uint32_t>(input + Offset::HudInputLayout::SelectedObjNetId);
    }

} // namespace CoreView
