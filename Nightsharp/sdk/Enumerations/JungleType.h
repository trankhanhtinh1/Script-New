#pragma once

#include <cstdint>

namespace SDK {

enum class JungleType : std::int32_t {
    Unknown = 0,
    All = 1,
    Small = 2,
    Large = 4,
    Legendary = 8,
    Epic = 24,             // Voidgrubs (early-game epic objective), includes Legendary flag
    Plant = 32,            // Blast Cone, Honeyfruit, Scryer's Bloom (jungle plants)
};

constexpr JungleType operator&(JungleType lhs, JungleType rhs) {
    return static_cast<JungleType>(
        static_cast<std::int32_t>(lhs) & static_cast<std::int32_t>(rhs));
}

constexpr JungleType operator|(JungleType lhs, JungleType rhs) {
    return static_cast<JungleType>(
        static_cast<std::int32_t>(lhs) | static_cast<std::int32_t>(rhs));
}

} // namespace SDK
