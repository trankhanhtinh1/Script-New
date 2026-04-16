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
// champion_id enum (SDBM hash of champion name, case-insensitive)
// ---------------------------------------------------------------------------
enum class champion_id : uint32_t {
    Unknown = 0,
    Aatrox = champion_hash("Aatrox"),
    Ahri = champion_hash("Ahri"),
    Akali = champion_hash("Akali"),
    Akshan = champion_hash("Akshan"),
    Alistar = champion_hash("Alistar"),
    Ambessa = champion_hash("Ambessa"),
    Amumu = champion_hash("Amumu"),
    Anivia = champion_hash("Anivia"),
    Annie = champion_hash("Annie"),
    Aphelios = champion_hash("Aphelios"),
    Ashe = champion_hash("Ashe"),
    AurelionSol = champion_hash("AurelionSol"),
    Aurora = champion_hash("Aurora"),
    Azir = champion_hash("Azir"),
    Bard = champion_hash("Bard"),
    Belveth = champion_hash("Belveth"),
    Blitzcrank = champion_hash("Blitzcrank"),
    Brand = champion_hash("Brand"),
    Braum = champion_hash("Braum"),
    Briar = champion_hash("Briar"),
    Caitlyn = champion_hash("Caitlyn"),
    Camille = champion_hash("Camille"),
    Cassiopeia = champion_hash("Cassiopeia"),
    Chogath = champion_hash("Chogath"),
    Corki = champion_hash("Corki"),
    Darius = champion_hash("Darius"),
    Diana = champion_hash("Diana"),
    Draven = champion_hash("Draven"),
    DrMundo = champion_hash("DrMundo"),
    Ekko = champion_hash("Ekko"),
    Elise = champion_hash("Elise"),
    Evelynn = champion_hash("Evelynn"),
    Ezreal = champion_hash("Ezreal"),
    Fiddlesticks = champion_hash("Fiddlesticks"),
    Fiora = champion_hash("Fiora"),
    Fizz = champion_hash("Fizz"),
    Galio = champion_hash("Galio"),
    Gangplank = champion_hash("Gangplank"),
    Garen = champion_hash("Garen"),
    Gnar = champion_hash("Gnar"),
    Gragas = champion_hash("Gragas"),
    Graves = champion_hash("Graves"),
    Gwen = champion_hash("Gwen"),
    Hecarim = champion_hash("Hecarim"),
    Heimerdinger = champion_hash("Heimerdinger"),
    Hwei = champion_hash("Hwei"),
    Illaoi = champion_hash("Illaoi"),
    Irelia = champion_hash("Irelia"),
    Ivern = champion_hash("Ivern"),
    Janna = champion_hash("Janna"),
    JarvanIV = champion_hash("JarvanIV"),
    Jax = champion_hash("Jax"),
    Jayce = champion_hash("Jayce"),
    Jhin = champion_hash("Jhin"),
    Jinx = champion_hash("Jinx"),
    Kaisa = champion_hash("Kaisa"),
    Kalista = champion_hash("Kalista"),
    Karma = champion_hash("Karma"),
    Karthus = champion_hash("Karthus"),
    Kassadin = champion_hash("Kassadin"),
    Katarina = champion_hash("Katarina"),
    Kayle = champion_hash("Kayle"),
    Kayn = champion_hash("Kayn"),
    Kennen = champion_hash("Kennen"),
    Khazix = champion_hash("Khazix"),
    Kindred = champion_hash("Kindred"),
    Kled = champion_hash("Kled"),
    KogMaw = champion_hash("KogMaw"),
    KSante = champion_hash("KSante"),
    Leblanc = champion_hash("Leblanc"),
    LeeSin = champion_hash("LeeSin"),
    Leona = champion_hash("Leona"),
    Lillia = champion_hash("Lillia"),
    Lissandra = champion_hash("Lissandra"),
    Lucian = champion_hash("Lucian"),
    Lulu = champion_hash("Lulu"),
    Lux = champion_hash("Lux"),
    Malphite = champion_hash("Malphite"),
    Malzahar = champion_hash("Malzahar"),
    Maokai = champion_hash("Maokai"),
    MasterYi = champion_hash("MasterYi"),
    Mel = champion_hash("Mel"),
    Milio = champion_hash("Milio"),
    MissFortune = champion_hash("MissFortune"),
    Mordekaiser = champion_hash("Mordekaiser"),
    Morgana = champion_hash("Morgana"),
    Naafiri = champion_hash("Naafiri"),
    Nami = champion_hash("Nami"),
    Nasus = champion_hash("Nasus"),
    Nautilus = champion_hash("Nautilus"),
    Neeko = champion_hash("Neeko"),
    Nidalee = champion_hash("Nidalee"),
    Nilah = champion_hash("Nilah"),
    Nocturne = champion_hash("Nocturne"),
    Nunu = champion_hash("Nunu"),
    Olaf = champion_hash("Olaf"),
    Orianna = champion_hash("Orianna"),
    Ornn = champion_hash("Ornn"),
    Pantheon = champion_hash("Pantheon"),
    Poppy = champion_hash("Poppy"),
    Pyke = champion_hash("Pyke"),
    Qiyana = champion_hash("Qiyana"),
    Quinn = champion_hash("Quinn"),
    Rakan = champion_hash("Rakan"),
    Rammus = champion_hash("Rammus"),
    RekSai = champion_hash("RekSai"),
    Rell = champion_hash("Rell"),
    Renata = champion_hash("Renata"),
    Renekton = champion_hash("Renekton"),
    Rengar = champion_hash("Rengar"),
    Riven = champion_hash("Riven"),
    Rumble = champion_hash("Rumble"),
    Ryze = champion_hash("Ryze"),
    Samira = champion_hash("Samira"),
    Sejuani = champion_hash("Sejuani"),
    Senna = champion_hash("Senna"),
    Seraphine = champion_hash("Seraphine"),
    Sett = champion_hash("Sett"),
    Shaco = champion_hash("Shaco"),
    Shen = champion_hash("Shen"),
    Shyvana = champion_hash("Shyvana"),
    Singed = champion_hash("Singed"),
    Sion = champion_hash("Sion"),
    Sivir = champion_hash("Sivir"),
    Skarner = champion_hash("Skarner"),
    Smolder = champion_hash("Smolder"),
    Sona = champion_hash("Sona"),
    Soraka = champion_hash("Soraka"),
    Swain = champion_hash("Swain"),
    Sylas = champion_hash("Sylas"),
    Syndra = champion_hash("Syndra"),
    TahmKench = champion_hash("TahmKench"),
    Taliyah = champion_hash("Taliyah"),
    Talon = champion_hash("Talon"),
    Taric = champion_hash("Taric"),
    Teemo = champion_hash("Teemo"),
    Thresh = champion_hash("Thresh"),
    Tristana = champion_hash("Tristana"),
    Trundle = champion_hash("Trundle"),
    Tryndamere = champion_hash("Tryndamere"),
    TwistedFate = champion_hash("TwistedFate"),
    Twitch = champion_hash("Twitch"),
    Udyr = champion_hash("Udyr"),
    Urgot = champion_hash("Urgot"),
    Varus = champion_hash("Varus"),
    Vayne = champion_hash("Vayne"),
    Veigar = champion_hash("Veigar"),
    Velkoz = champion_hash("Velkoz"),
    Vex = champion_hash("Vex"),
    Vi = champion_hash("Vi"),
    Viego = champion_hash("Viego"),
    Viktor = champion_hash("Viktor"),
    Vladimir = champion_hash("Vladimir"),
    Volibear = champion_hash("Volibear"),
    Warwick = champion_hash("Warwick"),
    Wukong = champion_hash("MonkeyKing"),
    Xayah = champion_hash("Xayah"),
    Xerath = champion_hash("Xerath"),
    XinZhao = champion_hash("XinZhao"),
    Yasuo = champion_hash("Yasuo"),
    Yone = champion_hash("Yone"),
    Yorick = champion_hash("Yorick"),
    Yuumi = champion_hash("Yuumi"),
    Zac = champion_hash("Zac"),
    Zed = champion_hash("Zed"),
    Zeri = champion_hash("Zeri"),
    Ziggs = champion_hash("Ziggs"),
    Zilean = champion_hash("Zilean"),
    Zoe = champion_hash("Zoe"),
    Zyra = champion_hash("Zyra"),
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
