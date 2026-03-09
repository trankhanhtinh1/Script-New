#pragma once
// ============================================================================
// JungleUtils.h — Jungle Classification Utilities (EnsoulSharp SDK Port)
// ============================================================================
// Full port of EnsoulSharp.SDK/Core/Utils/Jungle.cs
// Provides:
//   - GetJungleType(), IsJungleBuff(), IsEpicMonster(), IsDragon(), IsBaron()
//   - SmiteValue for objective stealing
//   - Jungle monster name databases
// ============================================================================

#include "GameObject.h"

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

namespace SDK {

// ============================================================================
// JungleType enum
// ============================================================================
enum class JungleType {
    Unknown,
    Small,
    Large,
    Legendary,  // Dragon, Baron, Rift Herald
    Epic        // Same as Legendary — alias for clarity
};

// ============================================================================
// JungleUtils — Static utility class
// ============================================================================
class JungleUtils {
public:
    static std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return (char)std::tolower(c);
        });
        return s;
    }

    static bool IsJunglePlantName(const std::string& objectName) {
        if (objectName.empty()) return false;
        const std::string name = ToLower(objectName);
        return name.find("sru_plant") != std::string::npos ||
               name.find("sru_plant_health") != std::string::npos ||
               name.find("sru_plant_satchel") != std::string::npos ||
               name.find("sru_plant_vision") != std::string::npos ||
               name.find("hiddenminionplantdemon") != std::string::npos ||
               name.find("planthealthmirrored") != std::string::npos ||
               name.find("plantmasterminion") != std::string::npos ||
               name.find("minimapicon") != std::string::npos;
    }

    static bool IsKnownJungleMonsterName(const std::string& objectName) {
        if (objectName.empty()) return false;
        const std::string name = ToLower(objectName);
        if (IsJunglePlantName(name)) return false;

        for (const auto& n : s_legendaryMonsters) {
            if (name.find(n) != std::string::npos) return true;
        }
        for (const auto& n : s_largeMonsters) {
            if (name.find(n) != std::string::npos) return true;
        }
        for (const auto& n : s_smallMonsters) {
            if (name.find(n) != std::string::npos) return true;
        }
        return false;
    }

    // ---- Get jungle monster type from character name ----
    static JungleType GetJungleType(const std::string& charName) {
        if (charName.empty()) return JungleType::Unknown;
        std::string name = ToLower(charName);

        // Explicit non-jungle entries
        if (IsJunglePlantName(name)) {
            return JungleType::Unknown;
        }

        if (name.find("sru_train_") != std::string::npos ||
            name.find("camprespawn") != std::string::npos ||
            name.find("respawnmarker") != std::string::npos) {
            return JungleType::Unknown;
        }

        // Prefix-based fast checks for current/future variants
        if (name.find("sru_baron") != std::string::npos ||
            name.find("sru_dragon") != std::string::npos ||
            name.find("sru_riftherald") != std::string::npos ||
            name.find("sru_horde") != std::string::npos ||
            name.find("voidgrub") != std::string::npos ||
            name.find("sru_atakhan") != std::string::npos) {
            return JungleType::Legendary;
        }

        if (name.find("sru_blue") != std::string::npos ||
            name.find("sru_red") != std::string::npos ||
            name.find("sru_gromp") != std::string::npos ||
            name.find("sru_krug") != std::string::npos ||
            name.find("sru_murkwolf") != std::string::npos ||
            name.find("sru_razorbeak") != std::string::npos ||
            name.find("sru_crab") != std::string::npos ||
            name.find("sru_riftscuttler") != std::string::npos) {
            if (name.find("mini") != std::string::npos) {
                return JungleType::Small;
            }
            return JungleType::Large;
        }

        for (auto& n : s_smallMonsters) {
            if (name.find(n) != std::string::npos) return JungleType::Small;
        }
        for (auto& n : s_largeMonsters) {
            if (name.find(n) != std::string::npos) return JungleType::Large;
        }
        for (auto& n : s_legendaryMonsters) {
            if (name.find(n) != std::string::npos) return JungleType::Legendary;
        }
        return JungleType::Unknown;
    }

    static JungleType GetJungleType(const GameObject& minion) {
        const std::string objectName = minion.GetName();
        const JungleType byObjectName = GetJungleType(objectName);
        if (byObjectName != JungleType::Unknown || IsJunglePlantName(objectName)) {
            return byObjectName;
        }

        const std::string characterName = minion.GetChampionName();
        return GetJungleType(characterName);
    }

    // ---- Quick checks ----
    static bool IsJungleBuff(const std::string& charName) {
        std::string name = ToLower(charName);
        return name.find("sru_blue") != std::string::npos || name.find("sru_red") != std::string::npos;
    }

    static bool IsJungleBuff(const GameObject& minion) {
        if (IsJungleBuff(minion.GetName())) {
            return true;
        }
        return IsJungleBuff(minion.GetChampionName());
    }

    static bool IsDragon(const std::string& charName) {
        return ToLower(charName).find("sru_dragon") != std::string::npos;
    }

    static bool IsBaron(const std::string& charName) {
        return ToLower(charName).find("sru_baron") != std::string::npos;
    }

    static bool IsRiftHerald(const std::string& charName) {
        return ToLower(charName).find("sru_riftherald") != std::string::npos;
    }

    static bool IsVoidGrub(const std::string& charName) {
        std::string name = ToLower(charName);
        return name.find("sru_horde") != std::string::npos ||
               name.find("voidgrub") != std::string::npos;
    }

    static bool IsEpicMonster(const std::string& charName) {
        return IsDragon(charName) || IsBaron(charName) || IsRiftHerald(charName);
    }

    static bool IsEpicMonster(const GameObject& minion) {
        if (IsEpicMonster(minion.GetName())) {
            return true;
        }
        return IsEpicMonster(minion.GetChampionName());
    }

    static bool IsLargeMonster(const std::string& charName) {
        return GetJungleType(charName) == JungleType::Large;
    }

    static bool IsSmallMonster(const std::string& charName) {
        return GetJungleType(charName) == JungleType::Small;
    }

    // ---- Get Smite damage value (Season 2026) ----
    // Smite always deals fixed damage to monsters
    static float GetSmiteDamage(int championLevel) {
        // Season 2026: Smite damage = 600-900 based on level
        // Level 1-5: 600, Level 6-10: 750, Level 11+: 900
        // Updated: Smite always does 900 damage after latest patches
        (void)championLevel;
        return 900.0f;
    }

    // ---- Get dragon variant from name ----
    enum class DragonType {
        Unknown,
        Infernal,
        Mountain,
        Ocean,
        Cloud,
        Hextech,
        Chemtech,
        Elder
    };

    static DragonType GetDragonType(const std::string& charName) {
        if (charName.find("Fire") != std::string::npos || charName.find("Infernal") != std::string::npos)
            return DragonType::Infernal;
        if (charName.find("Earth") != std::string::npos || charName.find("Mountain") != std::string::npos)
            return DragonType::Mountain;
        if (charName.find("Water") != std::string::npos || charName.find("Ocean") != std::string::npos)
            return DragonType::Ocean;
        if (charName.find("Air") != std::string::npos || charName.find("Cloud") != std::string::npos)
            return DragonType::Cloud;
        if (charName.find("Hextech") != std::string::npos)
            return DragonType::Hextech;
        if (charName.find("Chemtech") != std::string::npos)
            return DragonType::Chemtech;
        if (charName.find("Elder") != std::string::npos)
            return DragonType::Elder;
        return DragonType::Unknown;
    }

    // ---- Respawn timers (seconds) ----
    static float GetRespawnTime(const std::string& charName) {
        if (IsBaron(charName))      return 360.0f;  // 6 minutes
        if (IsDragon(charName))     return 300.0f;  // 5 minutes
        if (IsRiftHerald(charName)) return 360.0f;  // 6 minutes (only spawns twice)
        if (IsJungleBuff(charName)) return 300.0f;  // 5 minutes
        if (IsVoidGrub(charName))   return 240.0f;  // 4 minutes

        // Standard camps
        auto type = GetJungleType(charName);
        if (type == JungleType::Large)  return 135.0f;  // 2:15
        if (type == JungleType::Small)  return 135.0f;  // Same as parent
        return 0.0f;
    }

    // ---- Camp priority for jungling (higher = more important) ----
    static int GetCampPriority(const std::string& charName) {
        if (IsBaron(charName))       return 100;
        if (IsDragon(charName))      return 90;
        if (IsRiftHerald(charName))  return 85;
        if (IsVoidGrub(charName))    return 80;
        if (IsJungleBuff(charName))  return 70;

        // Large camps
        std::string name = charName;
        if (name.find("SRU_Gromp") != std::string::npos)    return 60;
        if (name.find("SRU_Krug") != std::string::npos)     return 55;
        if (name.find("SRU_Murkwolf") != std::string::npos) return 50;
        if (name.find("SRU_Razorbeak") != std::string::npos) return 50;
        if (name.find("Sru_Crab") != std::string::npos)     return 65;  // Scuttle crab

        return 10; // Small
    }

private:
    // ---- Name databases ----
    static inline const std::vector<std::string> s_smallMonsters = {
        "sru_krugmini",
        "sru_murkwolfmini",
        "sru_razorbeakmini",
        "sru_krugminimini",
        "testcuberender",
        "tt_ngolem2", "tt_nwraith2", "tt_nwolf2"
    };

    static inline const std::vector<std::string> s_largeMonsters = {
        "sru_red",          // Red buff
        "sru_blue",         // Blue buff
        "sru_gromp",        // Gromp
        "sru_krug",         // Krugs (main)
        "sru_murkwolf",     // Wolves (main)
        "sru_razorbeak",    // Raptors (main)
        "sru_crab",         // Scuttle crab
        "sru_riftscuttler",
        "tt_ngolem", "tt_nwraith", "tt_nwolf"
    };

    static inline const std::vector<std::string> s_legendaryMonsters = {
        "sru_dragon_air",
        "sru_dragon_earth",
        "sru_dragon_fire",
        "sru_dragon_water",
        "sru_dragon_hextech",
        "sru_dragon_chemtech",
        "sru_dragon_elder",
        "sru_riftherald",
        "sru_baron",
        "sru_horde",          // Void Grubs
        "sru_atakhan",
        "tt_spiderboss"
    };
};

} // namespace SDK
