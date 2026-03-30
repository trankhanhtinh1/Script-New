#pragma once

#include "Cursor.h"
#include "Drawing.h"

#include <array>
#include <cmath>

namespace SDK::UI {

    class Utils {
    public:
        enum class CircleType {
            Full,
            Half,
            Quarter
        };

        static void DrawLine(float xa, float ya, float xb, float yb, float width, ImU32 color) {
            Drawing::DrawLine(Vector2(xa, ya), Vector2(xb, yb), color, width);
        }

        static void DrawBoxFilled(float x, float y, float w, float h, ImU32 color) {
            if (auto* draw = Drawing::GetDrawList(true)) {
                draw->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), color);
            }
        }

        static void RoundedRectangle(int x, int y, int w, int h, int smooth, ImU32 color) {
            if (auto* draw = Drawing::GetDrawList(true)) {
                draw->AddRectFilled(ImVec2((float)x, (float)y), ImVec2((float)(x + w), (float)(y + h)), color, (float)smooth);
            }
        }

        static void DrawCircle(float x,
                               float y,
                               float radius,
                               int rotate,
                               CircleType type,
                               bool /*smoothing*/,
                               int resolution,
                               ImU32 color) {
            if (radius <= 0.0f || resolution < 8) {
                return;
            }

            const float startAngle = rotate * 3.1415926535f / 180.0f;
            float sweep = 6.283185307f;
            if (type == CircleType::Half) {
                sweep = 3.1415926535f;
            } else if (type == CircleType::Quarter) {
                sweep = 1.5707963267f;
            }

            std::array<ImVec2, 258> points{};
            const int capped = (resolution > 256) ? 256 : resolution;
            for (int i = 0; i <= capped; ++i) {
                const float t = startAngle + (sweep * static_cast<float>(i) / static_cast<float>(capped));
                points[i] = ImVec2(
                    x + std::cos(t) * radius,
                    y + std::sin(t) * radius);
            }

            if (auto* draw = Drawing::GetDrawList(true)) {
                draw->AddPolyline(points.data(), capped + 1, color, false, 1.5f);
            }
        }

        static void DrawCircleFilled(float x,
                                     float y,
                                     float radius,
                                     float rotate,
                                     CircleType type,
                                     bool /*smoothing*/,
                                     int resolution,
                                     ImU32 color) {
            if (radius <= 0.0f || resolution < 8) {
                return;
            }

            const float startAngle = rotate * 3.1415926535f / 180.0f;
            float sweep = 6.283185307f;
            if (type == CircleType::Half) {
                sweep = 3.1415926535f;
            } else if (type == CircleType::Quarter) {
                sweep = 1.5707963267f;
            }

            std::array<ImVec2, 258> points{};
            const int capped = (resolution > 256) ? 256 : resolution;
            for (int i = 0; i <= capped; ++i) {
                const float t = startAngle + (sweep * static_cast<float>(i) / static_cast<float>(capped));
                points[i] = ImVec2(
                    x + std::cos(t) * radius,
                    y + std::sin(t) * radius);
            }

            if (auto* draw = Drawing::GetDrawList(true)) {
                draw->AddConvexPolyFilled(points.data(), capped + 1, color);
            }
        }
    };

} // namespace SDK::UI
