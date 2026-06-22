#pragma once

#include <cstdint>
#include <type_traits>

namespace SDK {

enum class CollisionableObjects : std::int32_t {
    Minions = 1 << 0,
    Heroes = 1 << 1,
    YasuoWall = 1 << 2,
    BraumShield = 1 << 3,
    Walls = 1 << 4,
};

constexpr CollisionableObjects operator|(CollisionableObjects lhs, CollisionableObjects rhs) {
    using U = std::underlying_type_t<CollisionableObjects>;
    return static_cast<CollisionableObjects>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

constexpr CollisionableObjects operator&(CollisionableObjects lhs, CollisionableObjects rhs) {
    using U = std::underlying_type_t<CollisionableObjects>;
    return static_cast<CollisionableObjects>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

constexpr CollisionableObjects operator~(CollisionableObjects value) {
    using U = std::underlying_type_t<CollisionableObjects>;
    return static_cast<CollisionableObjects>(~static_cast<U>(value));
}

inline CollisionableObjects& operator|=(CollisionableObjects& lhs, CollisionableObjects rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline CollisionableObjects& operator&=(CollisionableObjects& lhs, CollisionableObjects rhs) {
    lhs = lhs & rhs;
    return lhs;
}

constexpr bool HasFlag(CollisionableObjects value, CollisionableObjects flag) {
    using U = std::underlying_type_t<CollisionableObjects>;
    return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
}

} // namespace SDK
