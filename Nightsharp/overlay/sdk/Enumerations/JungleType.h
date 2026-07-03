#pragma once

#include <cstdint>

namespace SDK {

enum class JungleType : std::int32_t {
    Unknown,
    Small,
    Large,
    Legendary,
    Plant,    // Blast Cone, Honeyfruit, Scryer's Bloom (jungle plants)
    Epic,     // Voidgrubs (early-game epic objective)
};

} // namespace SDK
