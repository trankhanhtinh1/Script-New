#pragma once

#include "../Core/Objects.h"
#include "../Enumerations/JungleType.h"

#include <array>
#include <cstring>
#include <string>

namespace SDK::Utils {

namespace Jungle {

namespace detail {
    inline constexpr std::array<const char*, 10> LargeNames = {
        "SRU_Razorbeak", "SRU_Red", "SRU_Krug", "SRU_Murkwolf", "SRU_Blue", "SRU_Gromp",
        "Sru_Crab", "TT_NGolem", "TT_NWraith", "TT_NWolf"
    };

    inline constexpr std::array<const char*, 8> LegendaryNames = {
        "SRU_Dragon_Air", "SRU_Dragon_Earth", "SRU_Dragon_Fire", "SRU_Dragon_Water",
        "SRU_Dragon_Elder", "SRU_RiftHerald", "SRU_Baron", "TT_Spiderboss"
    };

    inline constexpr std::array<const char*, 7> SmallNames = {
        "SRU_RazorbeakMini", "SRU_MurkwolfMini", "SRU_KrugMini", "TestCubeRender",
        "TT_NGolem2", "TT_NWraith2", "TT_NWolf2"
    };

    template<size_t N>
    inline bool StartsWithAny(const std::string& text, const std::array<const char*, N>& values) {
        return std::any_of(values.begin(), values.end(), [&text](const char* prefix) {
            return _strnicmp(text.c_str(), prefix, std::strlen(prefix)) == 0;
        });
    }
}

inline JungleType GetJungleType(const AIMinionClient& minion) {
    const std::string name = minion.CharacterName();
    if (detail::StartsWithAny(name, detail::SmallNames)) {
        return JungleType::Small;
    }
    if (detail::StartsWithAny(name, detail::LargeNames)) {
        return JungleType::Large;
    }
    if (detail::StartsWithAny(name, detail::LegendaryNames)) {
        return JungleType::Legendary;
    }
    return JungleType::Unknown;
}

inline bool IsJungleBuff(const AIMinionClient& minion) {
    const std::string name = minion.CharacterName();
    return _stricmp(name.c_str(), "SRU_Blue") == 0 || _stricmp(name.c_str(), "SRU_Red") == 0;
}

} // namespace Jungle
} // namespace SDK::Utils
