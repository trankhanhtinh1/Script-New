#pragma once

// ============================================================================
// Hash.h — Champion/Buff/Spell hash functions and champion_id enum
//
// Ported from BGXGG SDK (plugin_sdk.hpp)
// Hash type: SDBM (ignorecase) for champion identification
//            FNV1a (ignorecase) for buff/spell name hashing
//
// Usage:
//   if (unit.GetChampionHash() == champion_id::Ezreal) { ... }
//   if (buff.GetHash() == buff_hash("ZeriR")) { ... }
//   if (spell.GetNameHash() == spell_hash("EzrealQ")) { ... }
// ============================================================================

#include "../../core/Globals.h"
#include "../../core/Offsets.h"

#include <cstdint>

namespace SDK {

// ---------------------------------------------------------------------------
// Hash functions (constexpr, compile-time)
// ---------------------------------------------------------------------------

constexpr uint8_t char_to_lower(uint8_t input) {
    if (static_cast<uint8_t>(input - 0x41) > 0x19u)
        return input;
    return input + 0x20;
}

constexpr uint32_t hash_sdbm_ignorecase(const char* str) {
    uint32_t hash = 0;
    for (auto i = 0u; str[i]; ++i) {
        hash = hash * 65599 + char_to_lower(str[i]);
    }
    return hash;
}

constexpr uint32_t hash_fnv1a_ignorecase(const char* str) {
    uint32_t hash = 0x811C9DC5;
    for (auto i = 0u; str[i]; ++i) {
        hash = 16777619 * (hash ^ char_to_lower(str[i]));
    }
    return hash;
}

constexpr uint32_t hash_elf_ignorecase(const char* str) {
    uint32_t hash = 0;
    for (auto i = 0u; str[i]; ++i) {
        hash = char_to_lower(str[i]) + 0x10 * hash;
        if (hash & 0xF0000000)
            hash ^= (hash & 0xF0000000) ^ ((hash & 0xF0000000) >> 24);
    }
    return hash;
}

// ---------------------------------------------------------------------------
// Macros for compile-time hash
// ---------------------------------------------------------------------------
#define champion_hash(str) (std::integral_constant<uint32_t, SDK::hash_sdbm_ignorecase(str)>::value)
#define buff_hash(str)     (std::integral_constant<uint32_t, SDK::hash_fnv1a_ignorecase(str)>::value)
#define spell_hash(str)    (std::integral_constant<uint32_t, SDK::hash_fnv1a_ignorecase(str)>::value)

// ---------------------------------------------------------------------------
// champion_id enum — Riot official champion IDs (from BGXGG SDK)
// ---------------------------------------------------------------------------
enum class champion_id : int32_t {
    Annie = 1,
    Olaf = 2,
    Galio = 3,
    TwistedFate = 4,
    XinZhao = 5,
    Urgot = 6,
    Leblanc = 7,
    Vladimir = 8,
    FiddleSticks = 9,
    Kayle = 10,
    MasterYi = 11,
    Alistar = 12,
    Ryze = 13,
    Sion = 14,
    Sivir = 15,
    Soraka = 16,
    Teemo = 17,
    Tristana = 18,
    Warwick = 19,
    Nunu = 20,
    MissFortune = 21,
    Ashe = 22,
    Tryndamere = 23,
    Jax = 24,
    Morgana = 25,
    Zilean = 26,
    Singed = 27,
    Evelynn = 28,
    Twitch = 29,
    Karthus = 30,
    Chogath = 31,
    Amumu = 32,
    Rammus = 33,
    Anivia = 34,
    Shaco = 35,
    DrMundo = 36,
    Sona = 37,
    Kassadin = 38,
    Irelia = 39,
    Janna = 40,
    Gangplank = 41,
    Corki = 42,
    Karma = 43,
    Taric = 44,
    Veigar = 45,
    Trundle = 48,
    Swain = 50,
    Caitlyn = 51,
    Blitzcrank = 53,
    Malphite = 54,
    Katarina = 55,
    Nocturne = 56,
    Maokai = 57,
    Renekton = 58,
    JarvanIV = 59,
    Elise = 60,
    Orianna = 61,
    MonkeyKing = 62,
    Brand = 63,
    LeeSin = 64,
    Vayne = 67,
    Rumble = 68,
    Cassiopeia = 69,
    Skarner = 72,
    Heimerdinger = 74,
    Nasus = 75,
    Nidalee = 76,
    Udyr = 77,
    Poppy = 78,
    Gragas = 79,
    Pantheon = 80,
    Ezreal = 81,
    Mordekaiser = 82,
    Yorick = 83,
    Akali = 84,
    Kennen = 85,
    Garen = 86,
    Leona = 89,
    Malzahar = 90,
    Talon = 91,
    Riven = 92,
    KogMaw = 96,
    Shen = 98,
    Lux = 99,
    Xerath = 101,
    Shyvana = 102,
    Ahri = 103,
    Graves = 104,
    Fizz = 105,
    Volibear = 106,
    Rengar = 107,
    Varus = 110,
    Nautilus = 111,
    Viktor = 112,
    Sejuani = 113,
    Fiora = 114,
    Ziggs = 115,
    Lulu = 117,
    Draven = 119,
    Hecarim = 120,
    Khazix = 121,
    Darius = 122,
    Jayce = 126,
    Lissandra = 127,
    Diana = 131,
    Quinn = 133,
    Syndra = 134,
    AurelionSol = 136,
    Kayn = 141,
    Zoe = 142,
    Zyra = 143,
    Kaisa = 145,
    Seraphine = 147,
    Gnar = 150,
    Zac = 154,
    Yasuo = 157,
    Velkoz = 161,
    Taliyah = 163,
    Camille = 164,
    Akshan = 166,
    Belveth = 200,
    Braum = 201,
    Jhin = 202,
    Kindred = 203,
    Jinx = 222,
    TahmKench = 223,
    Zeri = 221,
    Briar = 233,
    Viego = 234,
    Senna = 235,
    Lucian = 236,
    Zed = 238,
    Kled = 240,
    Ekko = 245,
    Qiyana = 246,
    Vi = 254,
    Aatrox = 266,
    Nami = 267,
    Azir = 268,
    Yuumi = 350,
    Samira = 360,
    Thresh = 412,
    Illaoi = 420,
    RekSai = 421,
    Ivern = 427,
    Kalista = 429,
    Bard = 432,
    Rakan = 497,
    Xayah = 498,
    Ornn = 516,
    Sylas = 517,
    Neeko = 518,
    Aphelios = 523,
    Rell = 526,
    Pyke = 555,
    Vex = 711,
    Yone = 777,
    Ambessa = 799,
    Mel = 800,
    Yunara = 804,
    Lillia = 876,
    Sett = 875,
    Gwen = 887,
    Renata = 888,
    Aurora = 893,
    Nilah = 895,
    KSante = 897,
    Smolder = 901,
    Milio = 902,
    Zaahen = 904,
    Hwei = 910,
    Naafiri = 950,
    Unknown = 5000,
};

// ---------------------------------------------------------------------------
// Runtime API — read champion hash from game object
// ---------------------------------------------------------------------------

inline uint32_t GetChampionHash(uintptr_t obj) {
    if (!Globals::IsValidPtr(obj)) return 0;
    const auto charData = Globals::Read<uintptr_t>(obj + CharacterDataLayout::CharacterDataPtr);
    if (!Globals::IsValidPtr(charData)) return 0;
    return Globals::Read<uint32_t>(charData + CharacterDataLayout::CharacterHash);
}

inline bool IsChampion(uintptr_t obj, champion_id id) {
    return GetChampionHash(obj) == static_cast<uint32_t>(id);
}

} // namespace SDK
