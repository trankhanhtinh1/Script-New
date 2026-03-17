#pragma once
#include <vector>
#include <ctime>
#include <chrono>
#include "../../GameObjects/GameObjects.h"
#include "../../Math/MathUtils.h"

namespace EzEvade {
    namespace EvadeUtils {
        
        // Custom tick count implementation mapping to original assemblyTime calculation
        float TickCount();

        Vec2 GetGamePosition(SDK::GameObject* hero, float delay = 0.0f);

        // Path cut algorithms
        std::vector<Vec2> CutPath(const std::vector<Vec2>& path, SDK::GameObject* unit, float delay, float speed = 0.0f);
        std::vector<Vec2> CutPathPrev(const std::vector<Vec2>& path, SDK::GameObject* unit, float delay);
        
        std::vector<Vec2> CutPath(const std::vector<Vec2>& path, float distance);
        std::vector<Vec2> CutPathPrev(const std::vector<Vec2>& path, float distance);

        inline Vec2 ExtendDir(const Vec2& pos, const Vec2& dir, float distance) {
            return pos + dir * distance;
        }

        inline Vec3 ExtendDir(const Vec3& pos, const Vec3& dir, float distance) {
            return pos + dir * distance;
        }

    } // namespace EvadeUtils
} // namespace EzEvade
