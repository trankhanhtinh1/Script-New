#pragma once

#include <cstdint>
#include <type_traits>

namespace SDK {

enum class MinionTypes : std::int32_t {
    Unknown = 0,
    Normal = 1 << 0,
    Ranged = 1 << 1,
    Melee = 1 << 2,
    Siege = 1 << 3,
    Super = 1 << 4,
    Ward = 1 << 5,
    JunglePlant = 1 << 6,
    Plant = JunglePlant,
};

constexpr MinionTypes operator|(MinionTypes lhs, MinionTypes rhs) {
    using U = std::underlying_type_t<MinionTypes>;
    return static_cast<MinionTypes>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

constexpr MinionTypes operator&(MinionTypes lhs, MinionTypes rhs) {
    using U = std::underlying_type_t<MinionTypes>;
    return static_cast<MinionTypes>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

constexpr MinionTypes operator~(MinionTypes value) {
    using U = std::underlying_type_t<MinionTypes>;
    return static_cast<MinionTypes>(~static_cast<U>(value));
}

inline MinionTypes& operator|=(MinionTypes& lhs, MinionTypes rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline MinionTypes& operator&=(MinionTypes& lhs, MinionTypes rhs) {
    lhs = lhs & rhs;
    return lhs;
}

constexpr bool HasFlag(MinionTypes value, MinionTypes flag) {
    using U = std::underlying_type_t<MinionTypes>;
    return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
}

} // namespace SDK
