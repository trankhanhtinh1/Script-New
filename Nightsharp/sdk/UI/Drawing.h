#pragma once

#include "../../Core/CoreRuntime.h"
#include "../../Core/CoreView.h"
#include "../../Core/Globals.h"
#include "../../Core/Vector.h"
#include "../../Core/offset.h"
#include "../../imgui/imgui.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace SDK::Drawing {

namespace detail {
    template <typename Handler, int MaxHandlers = 64>
    struct HandlerList {
        Handler handlers[MaxHandlers] = {};
        int count = 0;

        bool Add(Handler handler) {
            if (!handler) {
                return false;
            }
            for (int i = 0; i < count; ++i) {
                if (handlers[i] == handler) {
                    return true;
                }
            }
            if (count >= MaxHandlers) {
                return false;
            }
            handlers[count++] = handler;
            return true;
        }

        bool Remove(Handler handler) {
            if (!handler) {
                return false;
            }
            for (int i = 0; i < count; ++i) {
                if (handlers[i] != handler) {
                    continue;
                }
                for (int j = i; j + 1 < count; ++j) {
                    handlers[j] = handlers[j + 1];
                }
                handlers[--count] = nullptr;
                return true;
            }
            return false;
        }

        void Fire() const {
            for (int i = 0; i < count; ++i) {
                if (auto handler = handlers[i]) {
                    __try {
                        handler();
                    } __except (1) {}
                }
            }
        }

        void Clear() {
            for (int i = 0; i < count; ++i) {
                handlers[i] = nullptr;
            }
            count = 0;
        }
    };

    inline ImU32 FromArgb(std::uint32_t argb) {
        const auto a = static_cast<int>((argb >> 24) & 0xFF);
        const auto r = static_cast<int>((argb >> 16) & 0xFF);
        const auto g = static_cast<int>((argb >> 8) & 0xFF);
        const auto b = static_cast<int>(argb & 0xFF);
        return IM_COL32(r, g, b, a);
    }

    inline bool DrawingEnabled = true;
    inline bool F7WasDown = false;

    inline void UpdateHotkey() {
        const bool down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        if (down && !F7WasDown) {
            DrawingEnabled = !DrawingEnabled;
        }
        F7WasDown = down;
    }

    inline HWND FindProcessWindow() {
        struct EnumData {
            DWORD processId = 0;
            HWND window = nullptr;
        } data{ GetCurrentProcessId(), nullptr };

        EnumWindows([](HWND window, LPARAM parameter) -> BOOL {
            auto* state = reinterpret_cast<EnumData*>(parameter);
            DWORD processId = 0;
            GetWindowThreadProcessId(window, &processId);
            if (processId != state->processId ||
                !IsWindowVisible(window) ||
                (GetWindowLongPtrW(window, GWL_STYLE) & WS_CHILD) != 0) {
                return TRUE;
            }

            RECT rect = {};
            if (!GetClientRect(window, &rect) ||
                rect.right <= rect.left ||
                rect.bottom <= rect.top) {
                return TRUE;
            }

            state->window = window;
            return FALSE;
        }, reinterpret_cast<LPARAM>(&data));
        return data.window;
    }

    inline bool GetRendererSize(Vec2& out) {
        out = {};
        const auto renderer = CoreRuntime::GetContext().renderer;
        if (Globals::IsValidPtr(renderer)) {
            const int width = Globals::Read<int>(renderer + 0xC);
            const int height = Globals::Read<int>(renderer + 0x10);
            if (width > 0 && height > 0 && width < 20000 && height < 20000) {
                out = { static_cast<float>(width), static_cast<float>(height) };
                return true;
            }
        }

        const HWND window = FindProcessWindow();
        RECT rect = {};
        if (window && GetClientRect(window, &rect) &&
            rect.right > rect.left && rect.bottom > rect.top) {
            out = {
                static_cast<float>(rect.right - rect.left),
                static_cast<float>(rect.bottom - rect.top)
            };
            return true;
        }
        return false;
    }

    inline bool IsPlausibleWorld(const Vec3& value) {
        return value.IsValid() &&
               value.x >= -30000.0f && value.x <= 30000.0f &&
               value.y >= -5000.0f && value.y <= 5000.0f &&
               value.z >= -30000.0f && value.z <= 30000.0f;
    }

    inline bool ReadViewProjection(float out[16]) {
        if (!out) {
            return false;
        }

        const auto instance = CoreRuntime::GetContext().viewProjInstance;
        if (!Globals::IsValidPtr(instance)) {
            return false;
        }

        float view[16] = {};
        float projection[16] = {};
        for (int i = 0; i < 16; ++i) {
            view[i] = Globals::Read<float>(
                instance + static_cast<uintptr_t>(i) * sizeof(float));
            projection[i] = Globals::Read<float>(
                instance + Offset::DrawingMatrixRuntime::ProjMatrixRelative +
                static_cast<uintptr_t>(i) * sizeof(float));
            if (!std::isfinite(view[i]) || !std::isfinite(projection[i])) {
                return false;
            }
        }

        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                float value = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    value += view[row * 4 + k] * projection[k * 4 + column];
                }
                out[row * 4 + column] = value;
            }
        }
        return true;
    }

    inline bool ProjectWorldToScreen(const Vec3& world,
                                     const float matrix[16],
                                     const Vec2& size,
                                     Vec2& screen) {
        screen = {};
        if (!matrix || size.x <= 0.0f || size.y <= 0.0f) {
            return false;
        }

        Vec4 clip = {};
        clip.x = world.x * matrix[0] + world.y * matrix[4] +
                 world.z * matrix[8] + matrix[12];
        clip.y = world.x * matrix[1] + world.y * matrix[5] +
                 world.z * matrix[9] + matrix[13];
        clip.w = world.x * matrix[3] + world.y * matrix[7] +
                 world.z * matrix[11] + matrix[15];
        if (!std::isfinite(clip.w) || clip.w < 0.01f) {
            return false;
        }

        const float ndcX = clip.x / clip.w;
        const float ndcY = clip.y / clip.w;
        screen = {
            size.x * 0.5f * (1.0f + ndcX),
            size.y * 0.5f * (1.0f - ndcY)
        };
        return screen.IsValid();
    }

    inline bool WorldToScreenNative(const Vec3& world, Vec2& screen) {
        screen = {};
        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(ctx.worldToScreenFn)) {
            return false;
        }

        const uintptr_t rootGlobal =
            CoreRuntime::ResolveRva(Offset::DrawingRuntime::ViewProjectionRoot);
        const uintptr_t root = Globals::Read<uintptr_t>(rootGlobal);
        if (!Globals::IsValidPtr(root)) {
            return false;
        }

        using WorldToScreenFn =
            bool(__fastcall*)(uintptr_t, const Vec3*, Vec3*);
        Vec3 output = {};
        bool result = false;
        __try {
            result = reinterpret_cast<WorldToScreenFn>(ctx.worldToScreenFn)(
                root + Offset::DrawingRuntime::WorldToScreenContextOffset,
                &world,
                &output);
        }
        __except (1) {
            return false;
        }

        if (!std::isfinite(output.x) || !std::isfinite(output.y)) {
            return false;
        }
        screen = { output.x, output.y };
        return result || (!screen.IsZero() &&
                          screen.x > -100000.0f &&
                          screen.y > -100000.0f);
    }

    inline bool WorldToScreen(const Vec3& world, Vec2& screen) {
        if (!IsPlausibleWorld(world)) {
            screen = {};
            return false;
        }

        if (WorldToScreenNative(world, screen)) {
            return true;
        }

        float matrix[16] = {};
        Vec2 size = {};
        return ReadViewProjection(matrix) &&
               GetRendererSize(size) &&
               ProjectWorldToScreen(world, matrix, size, screen);
    }

    inline HandlerList<void(*)()> DrawHandlers;
    inline HandlerList<void(*)()> EndSceneHandlers;
    inline HandlerList<void(*)()> PreResetHandlers;
    inline HandlerList<void(*)()> PostResetHandlers;
} // namespace detail

using DrawHandler = void(*)();
using Matrix4x4 = ::CoreView::Matrix4x4;

inline bool IsEnabled() {
    detail::UpdateHotkey();
    return detail::DrawingEnabled;
}

inline void SetEnabled(bool enabled) {
    detail::DrawingEnabled = enabled;
}

inline bool ToggleEnabled() {
    detail::DrawingEnabled = !detail::DrawingEnabled;
    return detail::DrawingEnabled;
}

inline int Width() {
    if (ImGui::GetCurrentContext()) {
        const int width = static_cast<int>(ImGui::GetIO().DisplaySize.x);
        if (width > 0) {
            return width;
        }
    }
    Vec2 size = {};
    return ::CoreView::GetRendererSize(size)
        ? static_cast<int>(size.x)
        : 0;
}

inline int Height() {
    if (ImGui::GetCurrentContext()) {
        const int height = static_cast<int>(ImGui::GetIO().DisplaySize.y);
        if (height > 0) {
            return height;
        }
    }
    Vec2 size = {};
    return ::CoreView::GetRendererSize(size)
        ? static_cast<int>(size.y)
        : 0;
}

inline bool ReadView(Matrix4x4& out) {
    return ::CoreView::ReadView(out);
}

inline Matrix4x4 View() {
    Matrix4x4 out = {};
    (void)::SDK::Drawing::ReadView(out);
    return out;
}

inline bool ReadProjection(Matrix4x4& out) {
    return ::CoreView::ReadProjection(out);
}

inline Matrix4x4 Projection() {
    Matrix4x4 out = {};
    (void)::SDK::Drawing::ReadProjection(out);
    return out;
}

inline bool ScreenToWorld(const Vec2& screen, Vec3& world) {
    (void)CoreRuntime::EnsureInitialized();
    return ::CoreView::ScreenToWorld(screen, world);
}

inline Vec3 ScreenToWorld(const Vec2& screen) {
    Vec3 world = {};
    (void)ScreenToWorld(screen, world);
    return world;
}

inline bool WorldToScreen(const Vec3& world, Vec2& screen) {
    (void)CoreRuntime::EnsureInitialized();
    if (!IsEnabled()) {
        screen = {};
        return false;
    }
    return ::CoreView::WorldToScreen(world, screen);
}

inline Vec2 WorldToScreen(const Vec3& world) {
    Vec2 screen{};
    (void)WorldToScreen(world, screen);
    return screen;
}

inline bool WorldToMinimap(const Vec3& world, Vec2& minimap) {
    (void)CoreRuntime::EnsureInitialized();
    return ::CoreView::WorldToMinimap(world, minimap);
}

inline Vec2 WorldToMinimap(const Vec3& world) {
    Vec2 minimap = {};
    (void)WorldToMinimap(world, minimap);
    return minimap;
}

inline bool MinimapToWorld(const Vec2& minimap, Vec3& world) {
    (void)CoreRuntime::EnsureInitialized();
    return ::CoreView::MinimapToWorld(minimap, world);
}

inline Vec3 MinimapToWorld(const Vec2& minimap) {
    Vec3 world = {};
    (void)MinimapToWorld(minimap, world);
    return world;
}

inline bool OnScreen(const Vec2& point) {
    if (!IsEnabled()) {
        return false;
    }
    return point.x > 0.0f && point.y > 0.0f &&
           point.x < static_cast<float>(Width()) &&
           point.y < static_cast<float>(Height());
}

inline void DrawLine(float x1, float y1, float x2, float y2, float width, std::uint32_t color) {
    if (!IsEnabled() || !ImGui::GetCurrentContext()) {
        return;
    }
    ImGui::GetForegroundDrawList()->AddLine(
        ImVec2(x1, y1),
        ImVec2(x2, y2),
        detail::FromArgb(color),
        width);
}

inline void DrawLine(const Vec2& start, const Vec2& end, float width, std::uint32_t color) {
    DrawLine(start.x, start.y, end.x, end.y, width, color);
}

inline void DrawCircle(const Vec2& position, float radius, float thickness, std::uint32_t color, int segments = 64) {
    if (!IsEnabled() || !ImGui::GetCurrentContext()) {
        return;
    }
    ImGui::GetForegroundDrawList()->AddCircle(
        ImVec2(position.x, position.y),
        radius,
        detail::FromArgb(color),
        segments,
        thickness);
}

inline void DrawText(float x, float y, std::uint32_t color, const char* text) {
    if (!IsEnabled() || !ImGui::GetCurrentContext() || !text) {
        return;
    }
    ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y), detail::FromArgb(color), text);
}

inline bool AddOnDraw(DrawHandler handler) { return detail::DrawHandlers.Add(handler); }
inline bool RemoveOnDraw(DrawHandler handler) { return detail::DrawHandlers.Remove(handler); }
inline bool AddOnEndScene(DrawHandler handler) { return detail::EndSceneHandlers.Add(handler); }
inline bool RemoveOnEndScene(DrawHandler handler) { return detail::EndSceneHandlers.Remove(handler); }
inline bool AddOnPreReset(DrawHandler handler) { return detail::PreResetHandlers.Add(handler); }
inline bool RemoveOnPreReset(DrawHandler handler) { return detail::PreResetHandlers.Remove(handler); }
inline bool AddOnPostReset(DrawHandler handler) { return detail::PostResetHandlers.Add(handler); }
inline bool RemoveOnPostReset(DrawHandler handler) { return detail::PostResetHandlers.Remove(handler); }

inline void DispatchDraw() {
    if (IsEnabled()) {
        detail::DrawHandlers.Fire();
    }
}
inline void DispatchEndScene() {
    if (IsEnabled()) {
        detail::EndSceneHandlers.Fire();
    }
}
inline void DispatchPreReset() { detail::PreResetHandlers.Fire(); }
inline void DispatchPostReset() { detail::PostResetHandlers.Fire(); }

inline void Reset() {
    detail::DrawHandlers.Clear();
    detail::EndSceneHandlers.Clear();
    detail::PreResetHandlers.Clear();
    detail::PostResetHandlers.Clear();
}

struct DrawEventSlot {
    bool (*Add)(DrawHandler) = nullptr;
    bool (*Remove)(DrawHandler) = nullptr;
    bool operator+=(DrawHandler handler) const { return Add ? Add(handler) : false; }
    bool operator-=(DrawHandler handler) const { return Remove ? Remove(handler) : false; }
    bool operator()(DrawHandler handler) const { return (*this += handler); }
};

inline DrawEventSlot OnDraw{ &AddOnDraw, &RemoveOnDraw };
inline DrawEventSlot OnEndScene{ &AddOnEndScene, &RemoveOnEndScene };
inline DrawEventSlot OnPreReset{ &AddOnPreReset, &RemoveOnPreReset };
inline DrawEventSlot OnPostReset{ &AddOnPostReset, &RemoveOnPostReset };

} // namespace SDK::Drawing
