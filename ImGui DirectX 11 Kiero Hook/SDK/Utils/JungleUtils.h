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

    // ---- Get jungle monster type from character name ----
    static JungleType GetJungleType(const std::string& charName) {
        for (auto& n : s_smallMonsters) {
            if (charName.find(n) != std::string::npos) return JungleType::Small;
        }
        for (auto& n : s_largeMonsters) {
            if (charName.find(n) != std::string::npos) return JungleType::Large;
        }
        for (auto& n : s_legendaryMonsters) {
            if (charName.find(n) != std::string::npos) return JungleType::Legendary;
        }
        return JungleType::Unknown;
    }

    static JungleType GetJungleType(const GameObject& minion) {
        return GetJungleType(minion.GetChampionName());
    }

    // ---- Quick checks ----
    static bool IsJungleBuff(const std::string& charName) {
        return charName == "SRU_Blue" || charName == "SRU_Red";
    }

    static bool IsJungleBuff(const GameObject& minion) {
        return IsJungleBuff(minion.GetChampionName());
    }

    static bool IsDragon(const std::string& charName) {
        return charName.find("SRU_Dragon") != std::string::npos;
    }

    static bool IsBaron(const std::string& charName) {
        return charName.find("SRU_Baron") != std::string::npos;
    }

    static bool IsRiftHerald(const std::string& charName) {
        return charName.find("SRU_RiftHerald") != std::string::npos;
    }

    static bool IsVoidGrub(const std::string& charName) {
        return charName.find("SRU_Horde") != std::string::npos ||
               charName.find("VoidGrub") != std::string::npos;
    }

    static bool IsEpicMonster(const std::string& charName) {
        return IsDragon(charName) || IsBaron(charName) || IsRiftHerald(charName);
    }

    static bool IsEpicMonster(const GameObject& minion) {
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
        "SRU_RazorbeakMini",
        "SRU_MurkwolfMini",
        "SRU_KrugMini", "SRU_KrugMiniMini",
        "TestCubeRender",
        "TT_NGolem2", "TT_NWraith2", "TT_NWolf2"
    };

    static inline const std::vector<std::string> s_largeMonsters = {
        "SRU_Razorbeak",    // Raptors (main)
        "SRU_Red",          // Red buff
        "SRU_Krug",         // Krugs (main)
        "SRU_Murkwolf",     // Wolves (main)
        "SRU_Blue",         // Blue buff
        "SRU_Gromp",        // Gromp
        "Sru_Crab",         // Scuttle crab
        "TT_NGolem", "TT_NWraith", "TT_NWolf"
    };

    static inline const std::vector<std::string> s_legendaryMonsters = {
        "SRU_Dragon_Air",
        "SRU_Dragon_Earth",
        "SRU_Dragon_Fire",
        "SRU_Dragon_Water",
        "SRU_Dragon_Hextech",
        "SRU_Dragon_Chemtech",
        "SRU_Dragon_Elder",
        "SRU_RiftHerald",
        "SRU_Baron",
        "SRU_Horde",          // Void Grubs
        "TT_Spiderboss"
    };
};

} // namespace SDK
