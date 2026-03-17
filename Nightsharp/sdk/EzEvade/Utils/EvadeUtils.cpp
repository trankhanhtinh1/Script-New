#include "EvadeUtils.h"

namespace EzEvade {
    namespace EvadeUtils {

        // Emulate assemblyLoadTime from C# global init
        static auto assemblyLoadTime = std::chrono::steady_clock::now();

        float TickCount()
        {
            auto now = std::chrono::steady_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - assemblyLoadTime);
            return (float)diff.count();
        }

        Vec2 GetGamePosition(SDK::GameObject* hero, float delay)
        {
            if (hero != nullptr)
            {
                if (hero->IsMoving())
                {
                    std::vector<Vec2> path;
                    path.push_back(hero->GetPosition().To2D());
                    
                    // SDK provides GetPathEnd() instead of full path array
                    Vec3 pathEnd = hero->GetPathEnd();
                    path.push_back(pathEnd.To2D());

                    auto finalPath = CutPath(path, hero, delay);

                    if (!finalPath.empty()) {
                        return finalPath.back();
                    }
                }

                return hero->GetPosition().To2D();
            }

            return Vec2(0, 0);
        }

        std::vector<Vec2> CutPath(const std::vector<Vec2>& path, SDK::GameObject* unit, float delay, float speed)
        {
            if (speed == 0.0f && unit)
            {
                speed = unit->GetMoveSpeed();
            }

            float dist = speed * delay / 1000.0f;
            return CutPath(path, dist);
        }

        std::vector<Vec2> CutPathPrev(const std::vector<Vec2>& path, SDK::GameObject* unit, float delay)
        {
            float dist = (unit ? unit->GetMoveSpeed() : 0.0f) * delay / 1000.0f;
            return CutPathPrev(path, dist);
        }

        std::vector<Vec2> CutPath(const std::vector<Vec2>& path, float distance)
        {
            std::vector<Vec2> result;
            float currentDistance = distance;

            if (path.size() > 0)
            {
                result.push_back(path.front());
            }

            for (size_t i = 0; i < (path.size() > 1 ? path.size() - 1 : 0); i++)
            {
                float dist = path[i].Distance(path[i + 1]);
                if (dist > currentDistance)
                {
                    Vec2 dir = (path[i + 1] - path[i]).Normalized();
                    result.push_back(path[i] + dir * currentDistance);
                    break;
                }
                else
                {
                    result.push_back(path[i + 1]);
                }
                currentDistance -= dist;
            }

            return result.size() > 0 ? result : (!path.empty() ? std::vector<Vec2>{ path.back() } : std::vector<Vec2>());
        }

        std::vector<Vec2> CutPathPrev(const std::vector<Vec2>& path, float distance)
        {
            std::vector<Vec2> result;
            float currentDistance = distance;

            for (size_t i = 0; i < (path.size() > 1 ? path.size() - 1 : 0); i++)
            {
                float dist = path[i].Distance(path[i + 1]);
                if (dist > currentDistance)
                {
                    Vec2 dir = (path[i + 1] - path[i]).Normalized();
                    result.push_back(path[i] + dir * currentDistance);

                    for (size_t j = i + 1; j < path.size(); j++)
                    {
                        result.push_back(path[j]);
                    }

                    break;
                }
                currentDistance -= dist;
            }
            
            return result.size() > 0 ? result : (!path.empty() ? std::vector<Vec2>{ path.back() } : std::vector<Vec2>());
        }

    } // namespace EvadeUtils
} // namespace EzEvade
