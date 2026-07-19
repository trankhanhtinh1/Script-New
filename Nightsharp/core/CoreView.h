#pragma once

#include "CoreMap.h"
#include "CoreRuntime.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"
#include "../imgui/imgui.h"

#include <Windows.h>
#include <DirectXMath.h>
#include <cmath>
#include <cstdint>

namespace CoreView {

struct Matrix4x4 {
    float m[16] = {};

    float* Data() { return m; }
    const float* Data() const { return m; }
    float operator[](int index) const { return m[index]; }
    float& operator[](int index) { return m[index]; }

    bool IsValid() const {
        for (float value : m) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
        return true;
    }
};

namespace detail {
    struct FrameViewCache {
        int frame = -1;
        Matrix4x4 viewProjection = {};
        Vec2 rendererSize = {};
        bool hasViewProjection = false;
        bool hasRendererSize = false;
    };

    inline FrameViewCache& GetFrameCache() {
        static FrameViewCache cache = {};
        return cache;
    }

    inline void SyncFrameCache() {
        FrameViewCache& cache = GetFrameCache();
        const int frame = ImGui::GetFrameCount();
        if (cache.frame != frame) {
            cache = {};
            cache.frame = frame;
        }
    }

    inline bool IsPlausibleWorld(const Vec3& value) {
        return value.IsValid() &&
               value.x >= -30000.0f && value.x <= 30000.0f &&
               value.y >= -5000.0f && value.y <= 5000.0f &&
               value.z >= -30000.0f && value.z <= 30000.0f;
    }

    inline bool IsPlausibleScreen(const Vec2& value, const Vec2& size = {}) {
        if (!value.IsValid()) {
            return false;
        }

        const float maxX = size.x > 0.0f ? size.x * 2.0f : 8192.0f;
        const float maxY = size.y > 0.0f ? size.y * 2.0f : 8192.0f;
        return value.x >= -maxX && value.x <= maxX &&
               value.y >= -maxY && value.y <= maxY;
    }

    inline bool ReadMatrix(uintptr_t address, Matrix4x4& out) {
        out = {};
        if (!Globals::IsReadablePtr(address, sizeof(out.m))) {
            return false;
        }

        __try {
            for (int i = 0; i < 16; ++i) {
                out.m[i] = Globals::Read<float>(
                    address + static_cast<uintptr_t>(i) * sizeof(float));
            }
        } __except (1) {
            out = {};
            return false;
        }
        return out.IsValid();
    }

    inline Matrix4x4 Multiply(const Matrix4x4& left, const Matrix4x4& right) {
        Matrix4x4 out = {};
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                float value = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    value += left.m[row * 4 + k] * right.m[k * 4 + column];
                }
                out.m[row * 4 + column] = value;
            }
        }
        return out;
    }

    inline float PlayerPlaneY() {
        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(ctx.localPlayer)) {
            return 0.0f;
        }

        __try {
            const Vec3 position = Globals::Read<Vec3>(ctx.localPlayer + Offset::All::Position);
            if (position.IsValid() && std::isfinite(position.y)) {
                return position.y;
            }
        } __except (1) {}
        return 0.0f;
    }
} // namespace detail

inline bool GetRendererSize(Vec2& out) {
    detail::SyncFrameCache();
    detail::FrameViewCache& cache = detail::GetFrameCache();
    if (cache.hasRendererSize) {
        out = cache.rendererSize;
        return true;
    }

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    if (displaySize.x > 0.0f && displaySize.y > 0.0f) {
        out = { displaySize.x, displaySize.y };
        cache.rendererSize = out;
        cache.hasRendererSize = true;
        return true;
    }

    const auto& ctx = CoreRuntime::GetContext();
    if (Globals::IsValidPtr(ctx.renderer)) {
        __try {
            const int width = Globals::Read<int>(
                ctx.renderer + Offset::D3D::ScreenWidth);
            const int height = Globals::Read<int>(
                ctx.renderer + Offset::D3D::ScreenHeight);
            if (width > 0 && height > 0 && width < 20000 && height < 20000) {
                out = { static_cast<float>(width), static_cast<float>(height) };
                cache.rendererSize = out;
                cache.hasRendererSize = true;
                return true;
            }
        } __except (1) {
            out = {};
        }
    }
    out = {};
    return false;
}

inline int Width() {
    Vec2 size = {};
    return GetRendererSize(size) ? static_cast<int>(size.x) : 0;
}

inline int Height() {
    Vec2 size = {};
    return GetRendererSize(size) ? static_cast<int>(size.y) : 0;
}

inline bool ReadView(Matrix4x4& out) {
    (void)CoreRuntime::EnsureInitialized();
    const uintptr_t instance = CoreRuntime::GetContext().viewProjInstance;
    return detail::ReadMatrix(instance, out);
}

inline bool ReadProjection(Matrix4x4& out) {
    (void)CoreRuntime::EnsureInitialized();
    const uintptr_t instance = CoreRuntime::GetContext().viewProjInstance;
    return detail::ReadMatrix(
        instance + Offset::DrawingMatrixRuntime::ProjMatrixRelative,
        out);
}

inline bool ReadViewProjection(Matrix4x4& out) {
    detail::SyncFrameCache();
    detail::FrameViewCache& cache = detail::GetFrameCache();
    if (cache.hasViewProjection) {
        out = cache.viewProjection;
        return true;
    }

    Matrix4x4 view = {};
    Matrix4x4 projection = {};
    if (!ReadView(view) || !ReadProjection(projection)) {
        out = {};
        return false;
    }

    out = detail::Multiply(view, projection);
    if (!out.IsValid()) {
        out = {};
        return false;
    }

    cache.viewProjection = out;
    cache.hasViewProjection = true;
    return true;
}

inline bool ReadViewProjection(float out[16]) {
    if (!out) {
        return false;
    }

    Matrix4x4 matrix = {};
    if (!ReadViewProjection(matrix)) {
        return false;
    }

    for (int i = 0; i < 16; ++i) {
        out[i] = matrix.m[i];
    }
    return true;
}

inline bool ProjectWorldToScreen(const Vec3& world,
                                 const Matrix4x4& matrix,
                                 const Vec2& size,
                                 Vec2& screen) {
    screen = {};
    if (!detail::IsPlausibleWorld(world) ||
        !matrix.IsValid() ||
        size.x <= 0.0f ||
        size.y <= 0.0f) {
        return false;
    }

    Vec4 clip = {};
    clip.x = world.x * matrix.m[0] + world.y * matrix.m[4] +
             world.z * matrix.m[8] + matrix.m[12];
    clip.y = world.x * matrix.m[1] + world.y * matrix.m[5] +
             world.z * matrix.m[9] + matrix.m[13];
    clip.w = world.x * matrix.m[3] + world.y * matrix.m[7] +
             world.z * matrix.m[11] + matrix.m[15];
    if (!std::isfinite(clip.w) || clip.w < 0.01f) {
        return false;
    }

    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;
    screen = {
        size.x * 0.5f * (1.0f + ndcX),
        size.y * 0.5f * (1.0f - ndcY)
    };
    return detail::IsPlausibleScreen(screen, size);
}

inline bool WorldToScreenNative(const Vec3& world, Vec2& screen) {
    (void)world;
    screen = {};
    // Old source disables the native W2S call: a bad ABI target throws every
    // frame and makes drawing stutter. Matrix projection is the fast path.
    return false;
}

inline bool WorldToScreen(const Vec3& world, Vec2& screen) {
    screen = {};
    Matrix4x4 matrix = {};
    Vec2 size = {};
    if (!ReadViewProjection(matrix) || !GetRendererSize(size)) {
        return false;
    }

    if (ProjectWorldToScreen(world, matrix, size, screen)) {
        return true;
    }

    return WorldToScreenNative(world, screen);
}

inline Vec2 WorldToScreen(const Vec3& world) {
    Vec2 screen = {};
    (void)WorldToScreen(world, screen);
    return screen;
}

inline bool ScreenToWorldOnPlane(const Vec2& screen, float planeY, Vec3& out) {
    out = {};
    Vec2 size = {};
    Matrix4x4 matrix = {};
    if (!GetRendererSize(size) ||
        !detail::IsPlausibleScreen(screen, size) ||
        !ReadViewProjection(matrix)) {
        return false;
    }

    using namespace DirectX;
    XMMATRIX viewProjection(
        matrix.m[0], matrix.m[1], matrix.m[2], matrix.m[3],
        matrix.m[4], matrix.m[5], matrix.m[6], matrix.m[7],
        matrix.m[8], matrix.m[9], matrix.m[10], matrix.m[11],
        matrix.m[12], matrix.m[13], matrix.m[14], matrix.m[15]);

    XMVECTOR determinant = XMVectorZero();
    XMMATRIX inverse = XMMatrixInverse(&determinant, viewProjection);
    if (XMVectorGetX(determinant) == 0.0f) {
        return false;
    }

    const float ndcX = (screen.x / size.x) * 2.0f - 1.0f;
    const float ndcY = 1.0f - (screen.y / size.y) * 2.0f;

    XMVECTOR nearClip = XMVectorSet(ndcX, ndcY, 0.0f, 1.0f);
    XMVECTOR farClip = XMVectorSet(ndcX, ndcY, 1.0f, 1.0f);
    XMVECTOR nearWorld = XMVector3TransformCoord(nearClip, inverse);
    XMVECTOR farWorld = XMVector3TransformCoord(farClip, inverse);

    const Vec3 nearPoint{
        XMVectorGetX(nearWorld),
        XMVectorGetY(nearWorld),
        XMVectorGetZ(nearWorld)
    };
    const Vec3 farPoint{
        XMVectorGetX(farWorld),
        XMVectorGetY(farWorld),
        XMVectorGetZ(farWorld)
    };

    if (!detail::IsPlausibleWorld(nearPoint) && !detail::IsPlausibleWorld(farPoint)) {
        return false;
    }

    const Vec3 dir = farPoint - nearPoint;
    if (std::fabs(dir.y) < 0.0001f) {
        out = farPoint;
        return detail::IsPlausibleWorld(out);
    }

    const float t = (planeY - nearPoint.y) / dir.y;
    out = nearPoint + dir * t;
    out.y = planeY;
    return detail::IsPlausibleWorld(out);
}

inline bool ScreenToWorld(const Vec2& screen, Vec3& out) {
    (void)CoreRuntime::RefreshReadState();
    return ScreenToWorldOnPlane(screen, detail::PlayerPlaneY(), out);
}

inline Vec3 ScreenToWorld(const Vec2& screen) {
    Vec3 out = {};
    (void)ScreenToWorld(screen, out);
    return out;
}

inline bool WorldToMinimap(const Vec3& world, Vec2& out) {
    return CoreMap::WorldToMinimap(world, out);
}

inline Vec2 WorldToMinimap(const Vec3& world) {
    return CoreMap::WorldToMinimap(world);
}

inline bool MinimapToWorld(const Vec2& minimap, Vec3& out) {
    return CoreMap::MinimapToWorld(minimap, out, detail::PlayerPlaneY());
}

inline Vec3 MinimapToWorld(const Vec2& minimap) {
    return CoreMap::MinimapToWorld(minimap, detail::PlayerPlaneY());
}

inline bool OnScreen(const Vec2& point) {
    return point.x > 0.0f && point.y > 0.0f &&
           point.x < static_cast<float>(Width()) &&
           point.y < static_cast<float>(Height());
}

} // namespace CoreView
