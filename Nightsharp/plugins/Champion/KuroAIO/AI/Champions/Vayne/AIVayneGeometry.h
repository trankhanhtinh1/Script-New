#pragma once

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Vayne::Geometry {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kCondemnPushDistance = 475.0f;

inline int SilverBoltStacks(int observed) {
    return std::clamp(observed, 0, 2);
}

struct SilverBoltAdvance {
    int Stacks = 0;
    int Procs = 0;
};

// Silver Bolts applies a marker to the same target on each attack and consumes
// the third marker. Modeling multiple attacks keeps event and polling paths
// deterministic when a frame contains more than one attack notification.
inline SilverBoltAdvance AdvanceSilverBolts(int startingStacks,
                                            int attacks) {
    SilverBoltAdvance result{SilverBoltStacks(startingStacks), 0};
    for (int i = 0; i < std::max(0, attacks); ++i) {
        ++result.Stacks;
        if (result.Stacks >= 3) {
            result.Stacks = 0;
            ++result.Procs;
        }
    }
    return result;
}

inline Vec2 Subtract(const Vec2& left, const Vec2& right) {
    return {left.x - right.x, left.y - right.y};
}

inline float Length(const Vec2& value) {
    return std::sqrt(value.x * value.x + value.y * value.y);
}

inline Vec2 Normalize(const Vec2& value) {
    const float length = Length(value);
    return length > 0.001f
        ? Vec2{value.x / length, value.y / length}
        : Vec2{};
}

inline float Dot(const Vec2& left, const Vec2& right) {
    return left.x * right.x + left.y * right.y;
}

inline float Cross(const Vec2& left, const Vec2& right) {
    return left.x * right.y - left.y * right.x;
}

inline float CondemnWallAngleDegrees(const Vec2& source,
                                     const Vec2& target,
                                     const Vec2& wallPoint) {
    const Vec2 push = Normalize(Subtract(target, source));
    const Vec2 wall = Normalize(Subtract(wallPoint, target));
    if (Length(push) <= 0.001f || Length(wall) <= 0.001f) return 180.0f;
    const float cosine = std::clamp(Dot(push, wall), -1.0f, 1.0f);
    return std::acos(cosine) * 180.0f / kPi;
}

// A wall contact is useful only when the knockback ray points into the wall
// and the contact is reachable before Condemn's 475-unit displacement ends.
// wallPoint is the first terrain contact along the intended knockback ray.
inline bool CondemnWallAngleAllows(const Vec2& source,
                                   const Vec2& target,
                                   const Vec2& wallPoint,
                                   float wallTolerance = 65.0f,
                                   float maximumAngleDegrees = 32.0f,
                                   float pushDistance = kCondemnPushDistance) {
    const Vec2 toWall = Subtract(wallPoint, target);
    const float wallDistance = Length(toWall);
    if (wallDistance <= 0.001f || wallDistance > pushDistance +
            std::max(0.0f, wallTolerance)) {
        return false;
    }
    return CondemnWallAngleDegrees(source, target, wallPoint) <=
           std::max(0.0f, maximumAngleDegrees);
}

inline bool TurretDiveAllowed(bool underEnemyTurret,
                              bool playerAlreadyUnderTurret,
                              bool lethalNow,
                              bool explicitDive) {
    if (!underEnemyTurret) return true;
    return playerAlreadyUnderTurret || lethalNow || explicitDive;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Vayne::Geometry
