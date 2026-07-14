#pragma once

#include "../../../../SDK/SDK.h"

#include <algorithm>
#include <chrono>
#include <vector>

namespace Plugins::KuroEvade {

struct EvadeUtils final {
    static int TickCount() {
        return SDK::Variables::TickCount();
    }

    static std::string GetObjectName(const SDK::GameObject& object) {
        if (!object.IsValid()) return {};
        char nameBuf[96] = {};
        if (::Core::Objects::ReadName(object.Address(), nameBuf, sizeof(nameBuf)) && nameBuf[0]) {
            return nameBuf;
        }
        return {};
    }

    static std::string GetObjectCharacterName(const SDK::GameObject& object) {
        if (!object.IsValid()) return {};
        std::string name = object.CharacterName();
        if (!name.empty()) return name;
        char nameBuf[96] = {};
        if (::Core::Objects::ReadCharacterName(object.Address(), nameBuf, sizeof(nameBuf)) && nameBuf[0]) {
            return nameBuf;
        }
        return {};
    }

    static std::vector<Vec2> PathToVector2(const std::vector<Vec3>& path) {
        std::vector<Vec2> result;
        result.reserve(path.size());
        for (const Vec3& point : path) {
            result.push_back(point.To2D());
        }
        return result;
    }

    static Vec2 GetGamePosition(const SDK::AIHeroClient& hero, float delayMs = 0.0f) {
        if (!hero.IsValid()) {
            return {};
        }

        auto path = PathToVector2(hero.Path());
        if (path.size() > 1 && delayMs > 0.0f) {
            auto cut = CutPath(path, hero.MoveSpeed() * delayMs / 1000.0f);
            return cut.empty() ? hero.Position().To2D() : cut.back();
        }
        return hero.Position().To2D();
    }

    static std::vector<Vec2> CutPath(const std::vector<Vec2>& path, float distance) {
        std::vector<Vec2> result;
        if (path.empty()) {
            return result;
        }

        float remaining = std::max(0.0f, distance);
        result.push_back(path.front());
        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            const float segmentLength = path[i].Distance(path[i + 1]);
            if (segmentLength > remaining) {
                result.push_back(path[i] + (path[i + 1] - path[i]).Normalized() * remaining);
                return result;
            }
            result.push_back(path[i + 1]);
            remaining -= segmentLength;
        }
        return result;
    }

    static std::vector<Vec2> CutPathPrev(const std::vector<Vec2>& path, float distance) {
        std::vector<Vec2> result;
        if (path.empty()) {
            return result;
        }

        float remaining = std::max(0.0f, distance);
        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            const float segmentLength = path[i].Distance(path[i + 1]);
            if (segmentLength > remaining) {
                result.push_back(path[i] + (path[i + 1] - path[i]).Normalized() * remaining);
                result.insert(result.end(), path.begin() + static_cast<std::ptrdiff_t>(i + 1), path.end());
                return result;
            }
            remaining -= segmentLength;
        }
        result.push_back(path.back());
        return result;
    }

    static Vec2 ExtendDir(const Vec2& pos, const Vec2& dir, float distance) {
        return pos + dir * distance;
    }

    static Vec3 ExtendDir(const Vec3& pos, const Vec3& dir, float distance) {
        return pos + dir * distance;
    }
};

} // namespace Plugins::KuroEvade
