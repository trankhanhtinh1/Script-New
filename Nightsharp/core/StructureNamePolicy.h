#pragma once

#include <cctype>

namespace Core::StructureNamePolicy {

inline bool EqualsInsensitive(const char* lhs, const char* rhs) {
    if (!lhs || !rhs) {
        return lhs == rhs;
    }
    while (*lhs && *rhs) {
        if (std::tolower(static_cast<unsigned char>(*lhs)) !=
            std::tolower(static_cast<unsigned char>(*rhs))) {
            return false;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == 0 && *rhs == 0;
}

inline bool StartsWithInsensitive(const char* value, const char* prefix) {
    if (!value || !prefix) {
        return false;
    }
    while (*prefix) {
        if (!*value ||
            std::tolower(static_cast<unsigned char>(*value)) !=
                std::tolower(static_cast<unsigned char>(*prefix))) {
            return false;
        }
        ++value;
        ++prefix;
    }
    return true;
}

// Accept a live map-instance suffix only when every field is numeric. This
// keeps names such as "..._DeathParticle" out of the attackable structure
// caches while allowing Riot's per-match numeric instance identifiers.
inline bool HasOptionalNumericFields(const char* value) {
    if (!value || !*value) {
        return true;
    }
    while (*value) {
        if (*value++ != '_') {
            return false;
        }
        if (!std::isdigit(static_cast<unsigned char>(*value))) {
            return false;
        }
        while (std::isdigit(static_cast<unsigned char>(*value))) {
            ++value;
        }
    }
    return true;
}

inline bool IsLiveInhibitorInstanceName(const char* value) {
    constexpr const char* orderPrefix = "Inhib_TOrder_L";
    constexpr const char* chaosPrefix = "Inhib_TChaos_L";
    const char* lane = nullptr;
    if (StartsWithInsensitive(value, orderPrefix)) {
        lane = value + 14;
    } else if (StartsWithInsensitive(value, chaosPrefix)) {
        lane = value + 14;
    }
    if (!lane || lane[0] < '1' || lane[0] > '3' || lane[1] != '_' ||
        std::tolower(static_cast<unsigned char>(lane[2])) != 'p') {
        return false;
    }

    const char* placement = lane + 3;
    if (!std::isdigit(static_cast<unsigned char>(*placement))) {
        return false;
    }
    while (std::isdigit(static_cast<unsigned char>(*placement))) {
        ++placement;
    }
    return HasOptionalNumericFields(placement);
}

inline bool IsLiveNexusInstanceName(const char* value) {
    constexpr const char* orderPrefix = "Nexus_TOrder";
    constexpr const char* chaosPrefix = "Nexus_TChaos";
    if (StartsWithInsensitive(value, orderPrefix)) {
        return HasOptionalNumericFields(value + 12);
    }
    if (StartsWithInsensitive(value, chaosPrefix)) {
        return HasOptionalNumericFields(value + 12);
    }
    return false;
}

inline bool IsBarracksDampenerName(const char* value) {
    if (!value || !*value) {
        return false;
    }
    if (EqualsInsensitive(value, "BarracksDampener") ||
        EqualsInsensitive(value, "BarracksDampenerClient") ||
        IsLiveInhibitorInstanceName(value)) {
        return true;
    }

    const char* lane = nullptr;
    if (StartsWithInsensitive(value, "Barracks_T1_")) {
        lane = value + 12;
    } else if (StartsWithInsensitive(value, "Barracks_T2_")) {
        lane = value + 12;
    }
    if (!lane) {
        return false;
    }

    const char laneCode = static_cast<char>(
        std::tolower(static_cast<unsigned char>(lane[0])));
    const bool validLane = laneCode == 't' || laneCode == 'c' ||
                           laneCode == 'l' || laneCode == 'r';
    const bool validIndex = lane[1] >= '1' && lane[1] <= '3';
    return validLane && validIndex && (lane[2] == 0 || lane[2] == '_');
}

inline bool IsNexusName(const char* value) {
    return value && (
        EqualsInsensitive(value, "HQ_T1") ||
        EqualsInsensitive(value, "HQ_T2") ||
        EqualsInsensitive(value, "Nexus") ||
        EqualsInsensitive(value, "Nexus_T1") ||
        EqualsInsensitive(value, "Nexus_T2") ||
        EqualsInsensitive(value, "OrderNexus") ||
        EqualsInsensitive(value, "ChaosNexus") ||
        IsLiveNexusInstanceName(value));
}

} // namespace Core::StructureNamePolicy
