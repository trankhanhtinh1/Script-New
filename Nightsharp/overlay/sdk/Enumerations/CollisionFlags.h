#pragma once

#include <cstdint>
#include <type_traits>

namespace SDK {

enum class CollisionFlags : std::uint16_t {
    None = 0,
    Grass = 1,
    Wall = 2,
    Building = 0x40,
    Prop = 0x80,
    GlobalVision = 0x100
};

constexpr CollisionFlags operator|(CollisionFlags lhs, CollisionFlags rhs) {
    using U = std::underlying_type_t<CollisionFlags>;
    return static_cast<CollisionFlags>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

constexpr CollisionFlags operator&(CollisionFlags lhs, CollisionFlags rhs) {
    using U = std::underlying_type_t<CollisionFlags>;
    return static_cast<CollisionFlags>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

constexpr CollisionFlags operator~(CollisionFlags value) {
    using U = std::underlying_type_t<CollisionFlags>;
    return static_cast<CollisionFlags>(~static_cast<U>(value));
}

inline CollisionFlags& operator|=(CollisionFlags& lhs, CollisionFlags rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline CollisionFlags& operator&=(CollisionFlags& lhs, CollisionFlags rhs) {
    lhs = lhs & rhs;
    return lhs;
}

constexpr std::uint16_t ToMask(CollisionFlags value) {
    return static_cast<std::uint16_t>(value);
}

constexpr bool HasFlag(CollisionFlags value, CollisionFlags flag) {
    return (ToMask(value) & ToMask(flag)) == ToMask(flag);
}

constexpr bool HasAnyFlag(CollisionFlags value, CollisionFlags flags) {
    return (ToMask(value) & ToMask(flags)) != 0;
}

} // namespace SDK
