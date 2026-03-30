#pragma once

namespace SDK {

enum class CollisionObjects : int {
    None = 0,
    Minions = 1,
    Heroes = 2,
    YasuoWall = 4,
    BraumShield = 8,
    Walls = 16
};

inline constexpr CollisionObjects operator|(CollisionObjects lhs, CollisionObjects rhs) {
    return static_cast<CollisionObjects>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline constexpr bool HasFlag(CollisionObjects value, CollisionObjects flag) {
    return (static_cast<int>(value) & static_cast<int>(flag)) != 0;
}

} // namespace SDK
