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
    static bool IsUsable(const SDK::AIBaseClient& unit, double stasisTime = -1.0) {
        if (!unit.IsValid() || (unit.IsDead() && !unit.IsZombie())) return false;
        const Vec3 position = ResolvePosition(unit);
        if (!position.IsValid() || position.IsZero()) return false;
        if (unit.IsHero() && !unit.IsVisible() && stasisTime <= 0.0 && !HasRebirthSignal(unit)) return false;
        if (!unit.IsTargetable() && stasisTime <= 0.0 && !HasRebirthSignal(unit)) return false;
        return true;
    }

    static double RemainingImmobileTime(const SDK::AIBaseClient& unit) {
        if (!unit.IsValid()) return -1.0;
        const float now = SDK::Game::Time();
        double maximum = 0.0;
        uintptr_t buffs[256] = {};
        const int count = CoreBuffs::Enumerate(unit.Address(), buffs, 256);
        static constexpr std::array<int, 10> types = {
            static_cast<int>(SDK::BuffType::Charm),
            static_cast<int>(SDK::BuffType::Knockup),
            static_cast<int>(SDK::BuffType::Stun),
            static_cast<int>(SDK::BuffType::Suppression),
            static_cast<int>(SDK::BuffType::Snare),
            static_cast<int>(SDK::BuffType::Fear),
            static_cast<int>(SDK::BuffType::Taunt),
            static_cast<int>(SDK::BuffType::Knockback),
            static_cast<int>(SDK::BuffType::Asleep),
            static_cast<int>(SDK::BuffType::Polymorph)
        };
        for (int index = 0; index < count; ++index) {
            const CoreBuffs::BuffRef buff{buffs[index]};
            if (!buff.IsActive(now)) continue;
            const int type = buff.GetType();
            if (std::find(types.begin(), types.end(), type) == types.end()) continue;
            maximum = std::max(maximum, static_cast<double>(buff.GetEndTime() - now));
        }
        return maximum > 0.0 ? maximum : -1.0;
    }

    static double RemainingStasisTime(const SDK::AIBaseClient& unit) {
        if (!unit.IsValid()) return -1.0;
        const float now = SDK::Game::Time();
        static constexpr std::array<const char*, 7> names = {
            "zhonyasringshield",
            "bardrstasis",
            "lissandrarself",
            "chronorevive",
            "guardianangelrebirth",
            "willrevive",
            "kindredr"
        };
        double maximum = 0.0;
        for (const char* name : names) {
            const CoreBuffs::BuffRef buff = CoreBuffs::FindActiveByName(unit.Address(), name, now);
            if (buff.IsValid()) maximum = std::max(maximum, static_cast<double>(buff.GetEndTime() - now));
        }
        return maximum > 0.0 ? maximum : -1.0;
    }

    static double RemainingRebirthTime(const SDK::AIBaseClient& unit,
                                       const MovementSnapshot& movement) {
        if (movement.reborn && movement.rebornRemainingSeconds > 0.0) {
            return movement.rebornRemainingSeconds;
        }
        const float now = SDK::Game::Time();
        static constexpr std::array<const char*, 4> names = {
            "willrevive",
            "chronorevive",
            "guardianangelrebirth",
            "sionpassivezombie"
        };
        double maximum = 0.0;
        for (const char* name : names) {
            const CoreBuffs::BuffRef buff = CoreBuffs::FindActiveByName(unit.Address(), name, now);
            if (buff.IsValid()) maximum = std::max(maximum, static_cast<double>(buff.GetEndTime() - now));
        }
        return maximum > 0.0 ? maximum : -1.0;
    }

    static double EffectiveMoveSpeed(const SDK::AIBaseClient& unit,
                                     const MovementSnapshot& movement,
                                     const PredictionConfig& config) {
        const double reported = std::max(0.0, static_cast<double>(unit.MoveSpeed()));
        if (!movement.moving) return 0.0;
        if (!config.useVelocityBlend || movement.averageSpeed <= 20.0) return reported;
        const double measured = movement.averageSpeed * 0.65 + movement.velocity.Length() * 0.35;
        const double minimum = reported > 0.0 ? reported * 0.60 : 0.0;
        const double maximum = reported > 0.0 ? reported * 1.30 : 5000.0;
        return std::clamp(reported * 0.45 + measured * 0.55, minimum, maximum);
    }

    static double WallRestriction(const Math::Vector2& position, double escapeRadius) {
        if (!position.IsFinite() || escapeRadius <= 1.0) return 0.0;
        constexpr int samples = 20;
        int blocked = 0;
        for (int index = 0; index < samples; ++index) {
            const double angle = (2.0 * Math::Pi * static_cast<double>(index)) /
                static_cast<double>(samples);
            const Math::Vector2 point = position + Math::Vector2{
                std::cos(angle) * escapeRadius,
                std::sin(angle) * escapeRadius
            };
            if (SDK::NavMesh::IsWall(ToWorld(point, 0.0f))) ++blocked;
        }
        return static_cast<double>(blocked) / static_cast<double>(samples);
    }

    static bool IsWall(const Math::Vector2& position, float height) {
        return position.IsFinite() && SDK::NavMesh::IsWall(ToWorld(position, height));
    }

private:
    static bool HasRebirthSignal(const SDK::AIBaseClient& unit) {
        return unit.HasBuff("willrevive") ||
            unit.HasBuff("chronorevive") ||
            unit.HasBuff("guardianangelrebirth") ||
            unit.HasBuff("sionpassivezombie");
    }

    static Vec3 ResolvePosition(const SDK::AIBaseClient& unit) {
        Vec3 position = unit.ServerPosition();
        if (!position.IsValid() || position.IsZero()) position = unit.Position();
        return position;
    }

    static Vec3 ToWorld(const Math::Vector2& point, float height) {
        return Vec3::From2D(Vec2(static_cast<float>(point.x), static_cast<float>(point.y)), height);
    }
};

}
