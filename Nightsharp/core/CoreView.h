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
        return Globals::IsValidPtr(viewport) ? (viewport + Offset::Hud::ViewportW2S) : 0;
    }

    inline uintptr_t GetHudSpellInfo() {
        const auto hud = CoreRuntime::GetContext().hudInstance;
        if (!Globals::IsValidPtr(hud)) {
            return 0;
        }
        return Globals::Read<uintptr_t>(hud + Offset::Hud::SpellInfo);
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
                proj[i] = Globals::Read<float>(inst + Offset::Extra::ProjMatrixRelative + static_cast<uintptr_t>(i * sizeof(float)));
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

    inline bool WorldToScreen(const Vec3& world, Vec2& screen) {
        float matrix[16] = {};
        Vec2 size = {};
        if (!ReadViewProjection(matrix) || !GetRendererSize(size)) {
            return WorldToScreenCall(world, screen);
        }

        Vec4 clip = {};
        clip.x = world.x * matrix[0] + world.y * matrix[4] + world.z * matrix[8] + matrix[12];
        clip.y = world.x * matrix[1] + world.y * matrix[5] + world.z * matrix[9] + matrix[13];
        clip.z = world.x * matrix[2] + world.y * matrix[6] + world.z * matrix[10] + matrix[14];
        clip.w = world.x * matrix[3] + world.y * matrix[7] + world.z * matrix[11] + matrix[15];

        if (clip.w < 0.01f) {
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
        return WorldToScreenCall(world, screen);
    }

    inline ScreenPoint ToScreen(const Vec3& world) {
        ScreenPoint pt = {};
        pt.onScreen = WorldToScreen(world, pt.position);
        return pt;
    }

    inline Vec3 GetMouseWorldPos() {
        const auto hud = CoreRuntime::GetContext().hudInstance;
        if (!Globals::IsValidPtr(hud)) {
            return {};
        }

        const auto input = Globals::Read<uintptr_t>(hud + Offset::Hud::Input);
        if (!Globals::IsValidPtr(input)) {
            return {};
        }

        return Globals::Read<Vec3>(input + Offset::Hud::MouseWorldPos);
    }

    inline Vec2 GetMouseScreenPos() {
        const auto mouseVec2 = CoreRuntime::GetContext().mouseScreenVec2;
        if (Globals::IsValidPtr(mouseVec2)) {
            return Globals::Read<Vec2>(mouseVec2);
        }

        const auto cursor = CoreRuntime::GetContext().cursorInstance;
        if (Globals::IsValidPtr(cursor)) {
            return Globals::Read<Vec2>(cursor + 0x2C);
        }

        return {};
    }

    inline uint32_t GetSelectedNetId() {
        const auto hud = CoreRuntime::GetContext().hudInstance;
        if (!Globals::IsValidPtr(hud)) {
            return 0;
        }

        const auto input = Globals::Read<uintptr_t>(hud + Offset::Hud::Input);
        if (!Globals::IsValidPtr(input)) {
            return 0;
        }

        return Globals::Read<uint32_t>(input + Offset::Hud::SelectedObjNetId);
    }

} // namespace CoreView
