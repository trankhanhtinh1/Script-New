#pragma once

namespace SDK {

enum class MinionTypes : int {
    Unknown = 0,
    Normal = 1 << 0,
    Ranged = 1 << 1,
    Melee = 1 << 2,
    Siege = 1 << 3,
    Super = 1 << 4,
    Ward = 1 << 5,
    Jungle = 1 << 6,
    All = 0x7fffffff
};

inline constexpr MinionTypes operator|(MinionTypes lhs, MinionTypes rhs) {
    return static_cast<MinionTypes>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline constexpr bool HasFlag(MinionTypes value, MinionTypes flag) {
    return (static_cast<int>(value) & static_cast<int>(flag)) != 0;
}

} // namespace SDK
