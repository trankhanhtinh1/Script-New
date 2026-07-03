#pragma once

// ============================================================================
// UnitInfo.h - Static unit data loaded from UnitData.json
// ----------------------------------------------------------------------------
// Port of LView/UnitInfo.{h,cpp}.
//
//   * Same `UnitTag` enum and same field set as the original (preserved 1:1
//     so JSON files written for LView remain compatible).
//   * `TagMapping` is exposed as a Meyers-singleton getter to avoid static
//     init order fiascos (the original LView code stored it as a regular
//     static which broke when SetTag was called before TagMapping was
//     constructed in some translation units).
//   * Methods inline in the header to match the rest of NightSharp SDK
//     (whole project is header-only outside of overlay/imgui).
// ============================================================================

#include <bitset>
#include <string>
#include <unordered_map>

namespace SDK::Data {

enum UnitTag : int {
    Unit_                                                              = 1,
    Unit_Champion                                                      = 2,
    Unit_Champion_Clone                                                = 3,
    Unit_IsolationNonImpacting                                         = 4,
    Unit_KingPoro                                                      = 5,
    Unit_Minion                                                        = 6,
    Unit_Minion_Lane                                                   = 7,
    Unit_Minion_Lane_Melee                                             = 8,
    Unit_Minion_Lane_Ranged                                            = 9,
    Unit_Minion_Lane_Siege                                             = 10,
    Unit_Minion_Lane_Super                                             = 11,
    Unit_Minion_Summon                                                 = 12,
    Unit_Minion_SummonName_game_character_displayname_ZyraSeed         = 13,
    Unit_Minion_Summon_Large                                           = 14,
    Unit_Monster                                                       = 15,
    Unit_Monster_Blue                                                  = 16,
    Unit_Monster_Buff                                                  = 17,
    Unit_Monster_Camp                                                  = 18,
    Unit_Monster_Crab                                                  = 19,
    Unit_Monster_Dragon                                                = 20,
    Unit_Monster_Epic                                                  = 21,
    Unit_Monster_Gromp                                                 = 22,
    Unit_Monster_Krug                                                  = 23,
    Unit_Monster_Large                                                 = 24,
    Unit_Monster_Medium                                                = 25,
    Unit_Monster_Raptor                                                = 26,
    Unit_Monster_Red                                                   = 27,
    Unit_Monster_Wolf                                                  = 28,
    Unit_Plant                                                         = 29,
    Unit_Special                                                       = 30,
    Unit_Special_AzirR                                                 = 31,
    Unit_Special_AzirW                                                 = 32,
    Unit_Special_CorkiBomb                                             = 33,
    Unit_Special_EpicMonsterIgnores                                    = 34,
    Unit_Special_KPMinion                                              = 35,
    Unit_Special_MonsterIgnores                                        = 36,
    Unit_Special_Peaceful                                              = 37,
    Unit_Special_SyndraSphere                                          = 38,
    Unit_Special_TeleportTarget                                        = 39,
    Unit_Special_Trap                                                  = 40,
    Unit_Special_Tunnel                                                = 41,
    Unit_Special_TurretIgnores                                         = 42,
    Unit_Special_UntargetableBySpells                                  = 43,
    Unit_Special_Void                                                  = 44,
    Unit_Special_YorickW                                               = 45,
    Unit_Structure                                                     = 46,
    Unit_Structure_Inhibitor                                           = 47,
    Unit_Structure_Nexus                                               = 48,
    Unit_Structure_Turret                                              = 49,
    Unit_Structure_Turret_Inhib                                        = 50,
    Unit_Structure_Turret_Inner                                        = 51,
    Unit_Structure_Turret_Nexus                                        = 52,
    Unit_Structure_Turret_Outer                                        = 53,
    Unit_Structure_Turret_Shrine                                       = 54,
    Unit_Ward                                                          = 55,
};

namespace detail {

    // Meyers-singleton accessor — initialised on first call, thread-safe under
    // C++11+. Keeps the table in one translation unit no matter how many
    // headers include UnitInfo.h.
    inline const std::unordered_map<std::string, UnitTag>& UnitTagMapping() {
        static const std::unordered_map<std::string, UnitTag> mapping = {
            { "Unit_",                                                      Unit_ },
            { "Unit_Champion",                                              Unit_Champion },
            { "Unit_Champion_Clone",                                        Unit_Champion_Clone },
            { "Unit_IsolationNonImpacting",                                 Unit_IsolationNonImpacting },
            { "Unit_KingPoro",                                              Unit_KingPoro },
            { "Unit_Minion",                                                Unit_Minion },
            { "Unit_Minion_Lane",                                           Unit_Minion_Lane },
            { "Unit_Minion_Lane_Melee",                                     Unit_Minion_Lane_Melee },
            { "Unit_Minion_Lane_Ranged",                                    Unit_Minion_Lane_Ranged },
            { "Unit_Minion_Lane_Siege",                                     Unit_Minion_Lane_Siege },
            { "Unit_Minion_Lane_Super",                                     Unit_Minion_Lane_Super },
            { "Unit_Minion_Summon",                                         Unit_Minion_Summon },
            { "Unit_Minion_SummonName_game_character_displayname_ZyraSeed", Unit_Minion_SummonName_game_character_displayname_ZyraSeed },
            { "Unit_Minion_Summon_Large",                                   Unit_Minion_Summon_Large },
            { "Unit_Monster",                                               Unit_Monster },
            { "Unit_Monster_Blue",                                          Unit_Monster_Blue },
            { "Unit_Monster_Buff",                                          Unit_Monster_Buff },
            { "Unit_Monster_Camp",                                          Unit_Monster_Camp },
            { "Unit_Monster_Crab",                                          Unit_Monster_Crab },
            { "Unit_Monster_Dragon",                                        Unit_Monster_Dragon },
            { "Unit_Monster_Epic",                                          Unit_Monster_Epic },
            { "Unit_Monster_Gromp",                                         Unit_Monster_Gromp },
            { "Unit_Monster_Krug",                                          Unit_Monster_Krug },
            { "Unit_Monster_Large",                                         Unit_Monster_Large },
            { "Unit_Monster_Medium",                                        Unit_Monster_Medium },
            { "Unit_Monster_Raptor",                                        Unit_Monster_Raptor },
            { "Unit_Monster_Red",                                           Unit_Monster_Red },
            { "Unit_Monster_Wolf",                                          Unit_Monster_Wolf },
            { "Unit_Plant",                                                 Unit_Plant },
            { "Unit_Special",                                               Unit_Special },
            { "Unit_Special_AzirR",                                         Unit_Special_AzirR },
            { "Unit_Special_AzirW",                                         Unit_Special_AzirW },
            { "Unit_Special_CorkiBomb",                                     Unit_Special_CorkiBomb },
            { "Unit_Special_EpicMonsterIgnores",                            Unit_Special_EpicMonsterIgnores },
            { "Unit_Special_KPMinion",                                      Unit_Special_KPMinion },
            { "Unit_Special_MonsterIgnores",                                Unit_Special_MonsterIgnores },
            { "Unit_Special_Peaceful",                                      Unit_Special_Peaceful },
            { "Unit_Special_SyndraSphere",                                  Unit_Special_SyndraSphere },
            { "Unit_Special_TeleportTarget",                                Unit_Special_TeleportTarget },
            { "Unit_Special_Trap",                                          Unit_Special_Trap },
            { "Unit_Special_Tunnel",                                        Unit_Special_Tunnel },
            { "Unit_Special_TurretIgnores",                                 Unit_Special_TurretIgnores },
            { "Unit_Special_UntargetableBySpells",                          Unit_Special_UntargetableBySpells },
            { "Unit_Special_Void",                                          Unit_Special_Void },
            { "Unit_Special_YorickW",                                       Unit_Special_YorickW },
            { "Unit_Structure",                                              Unit_Structure },
            { "Unit_Structure_Inhibitor",                                   Unit_Structure_Inhibitor },
            { "Unit_Structure_Nexus",                                       Unit_Structure_Nexus },
            { "Unit_Structure_Turret",                                      Unit_Structure_Turret },
            { "Unit_Structure_Turret_Inhib",                                Unit_Structure_Turret_Inhib },
            { "Unit_Structure_Turret_Inner",                                Unit_Structure_Turret_Inner },
            { "Unit_Structure_Turret_Nexus",                                Unit_Structure_Turret_Nexus },
            { "Unit_Structure_Turret_Outer",                                Unit_Structure_Turret_Outer },
            { "Unit_Structure_Turret_Shrine",                               Unit_Structure_Turret_Shrine },
            { "Unit_Ward",                                                  Unit_Ward },
        };
        return mapping;
    }

} // namespace detail

// ----------------------------------------------------------------------------
// UnitInfo - static description of one unit (champion / minion / structure).
// Loaded once from UnitData.json by GameData::LoadUnitData and looked up by
// lower-cased CharacterName at runtime via GameData::GetUnitInfoByName.
// ----------------------------------------------------------------------------
struct UnitInfo {
    std::string name;

    float healthBarHeight        = 0.0f;
    float baseMovementSpeed      = 0.0f;
    float baseAttackRange        = 0.0f;
    float baseAttackSpeed        = 0.0f;
    float attackSpeedRatio       = 0.0f;

    float acquisitionRange       = 0.0f;
    float selectionRadius        = 0.0f;
    float pathRadius             = 0.0f;
    float gameplayRadius         = 0.0f;

    float basicAttackMissileSpeed = 0.0f;
    float basicAttackWindup       = 0.0f;

    std::bitset<128> tags;

    // Set tag from JSON string ("Unit_Champion", "Unit_Minion_Lane_Siege" …).
    // Unknown tags are silently ignored — preserves LView behaviour but avoids
    // the original `tags[mapping[unknown]] = 0` insert that polluted the map.
    void SetTag(const char* tagStr) {
        if (!tagStr || !*tagStr) return;
        const auto& mapping = detail::UnitTagMapping();
        const auto it = mapping.find(tagStr);
        if (it != mapping.end()) {
            tags.set(static_cast<std::size_t>(it->second));
        }
    }

    bool HasTag(UnitTag tag) const {
        return tags.test(static_cast<std::size_t>(tag));
    }
};

} // namespace SDK::Data
