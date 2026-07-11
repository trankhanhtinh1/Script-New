#pragma once

#include "../Core/PredictionConfig.h"
#include "../Tracking/MovementTracker.h"
#include "../../../sdk/SDK.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ZDPrediction {

class StateAnalyzer {
public:
    static double RemainingImmobileTime(const SDK::AIBaseClient& unit) {
        if (!unit.IsValid()) return -1.0;
        const float now = SDK::Game::Time();
        double maximum = 0.0;
        uintptr_t buffs[256] = {};
        const int count = CoreBuffs::Enumerate(unit.Address(), buffs, 256);
        for (int index = 0; index < count; ++index) {
            const CoreBuffs::BuffRef buff{buffs[index]};
            if (!buff.IsActive(now)) continue;
            const int type = buff.GetType();
            if (type != 5 && type != 8 && type != 12 && type != 22 &&
                type != 23 && type != 25 && type != 30 && type != 31 && type != 35) continue;
            maximum = std::max(maximum, static_cast<double>(buff.GetEndTime() - now));
        }
        return maximum > 0.0 ? maximum : -1.0;
    }

    static double RemainingStasisTime(const SDK::AIBaseClient& unit) {
        if (!unit.IsValid()) return -1.0;
        const float now = SDK::Game::Time();
        static constexpr std::array<const char*, 5> names = {
            "zhonyasringshield",
            "bardrstasis",
            "lissandrarself",
            "chronorevive",
            "guardianangelrebirth"
        };
        double maximum = 0.0;
        for (const char* name : names) {
            const CoreBuffs::BuffRef buff = CoreBuffs::FindActiveByName(unit.Address(), name, now);
            if (buff.IsValid()) {
                maximum = std::max(maximum, static_cast<double>(buff.GetEndTime() - now));
            }
        }
        return maximum > 0.0 ? maximum : -1.0;
    }

    static double EffectiveMoveSpeed(const SDK::AIBaseClient& unit,
                                     const MovementSnapshot& movement,
                                     const PredictionConfig& config) {
        const double reported = std::max(0.0, static_cast<double>(unit.MoveSpeed()));
        if (!movement.moving) return 0.0;
        if (!config.useVelocityBlend || movement.averageSpeed <= 20.0) return reported;
        const double velocitySpeed = movement.velocity.Length();
        double measured = movement.averageSpeed;
        if (velocitySpeed > 20.0) measured = measured * 0.65 + velocitySpeed * 0.35;
        const double minimum = reported > 0.0 ? reported * 0.65 : 0.0;
        const double maximum = reported > 0.0 ? reported * 1.35 : 5000.0;
        return std::clamp(reported * 0.45 + measured * 0.55, minimum, maximum);
    }

    static double WallRestriction(const Math::Vector2& position, double escapeRadius) {
        if (!position.IsFinite() || escapeRadius <= 1.0) return 0.0;
        constexpr int samples = 16;
        int blocked = 0;
        for (int index = 0; index < samples; ++index) {
            const double angle = (2.0 * Math::Pi * static_cast<double>(index)) /
                static_cast<double>(samples);
            const Math::Vector2 point = position + Math::Vector2{
                std::cos(angle) * escapeRadius,
                std::sin(angle) * escapeRadius
            };
            if (SDK::NavMesh::IsWall(Vec3::From2D(
                    Vec2(static_cast<float>(point.x), static_cast<float>(point.y))))) {
                ++blocked;
            }
        }
        return static_cast<double>(blocked) / static_cast<double>(samples);
    }
};

}
