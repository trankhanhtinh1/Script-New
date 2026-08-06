#pragma once

// Generated from CommunityDragon champion-summary.json (playable ids 1..999).
// Canonical names preserve the runtime CharacterName/profile spelling used by KuroAIO.

#include "../Utils/HashUtils.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace SDK {

enum class ChampionId : std::int32_t {
    Unknown = 0,
    Annie = 1,
    Olaf = 2,
    Galio = 3,
    TwistedFate = 4,
    XinZhao = 5,
    Urgot = 6,
    Leblanc = 7,
    Vladimir = 8,
    Fiddlesticks = 9,
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
    KhaZix = 121,
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
    Zeri = 221,
    Jinx = 222,
    TahmKench = 223,
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
    Locke = 805,
    Sett = 875,
    Lillia = 876,
    Gwen = 887,
    RenataGlasc = 888,
    Aurora = 893,
    Nilah = 895,
    KSante = 897,
    Smolder = 901,
    Milio = 902,
    Zaahen = 904,
    Hwei = 910,
    Naafiri = 950,
};

struct ChampionInfo {
    ChampionId Id;
    const char* Name;
    const char* Alias;
    const char* DisplayName;
};

inline constexpr std::array<ChampionInfo, 173> kChampions = {{
    ChampionInfo{ ChampionId::Annie, "Annie", "Annie", "" },
    ChampionInfo{ ChampionId::Olaf, "Olaf", "Olaf", "" },
    ChampionInfo{ ChampionId::Galio, "Galio", "Galio", "" },
    ChampionInfo{ ChampionId::TwistedFate, "TwistedFate", "TwistedFate", "" },
    ChampionInfo{ ChampionId::XinZhao, "XinZhao", "XinZhao", "" },
    ChampionInfo{ ChampionId::Urgot, "Urgot", "Urgot", "" },
    ChampionInfo{ ChampionId::Leblanc, "Leblanc", "Leblanc", "" },
    ChampionInfo{ ChampionId::Vladimir, "Vladimir", "Vladimir", "" },
    ChampionInfo{ ChampionId::Fiddlesticks, "Fiddlesticks", "FiddleSticks", "" },
    ChampionInfo{ ChampionId::Kayle, "Kayle", "Kayle", "" },
    ChampionInfo{ ChampionId::MasterYi, "MasterYi", "MasterYi", "" },
    ChampionInfo{ ChampionId::Alistar, "Alistar", "Alistar", "" },
    ChampionInfo{ ChampionId::Ryze, "Ryze", "Ryze", "" },
    ChampionInfo{ ChampionId::Sion, "Sion", "Sion", "" },
    ChampionInfo{ ChampionId::Sivir, "Sivir", "Sivir", "" },
    ChampionInfo{ ChampionId::Soraka, "Soraka", "Soraka", "" },
    ChampionInfo{ ChampionId::Teemo, "Teemo", "Teemo", "" },
    ChampionInfo{ ChampionId::Tristana, "Tristana", "Tristana", "" },
    ChampionInfo{ ChampionId::Warwick, "Warwick", "Warwick", "" },
    ChampionInfo{ ChampionId::Nunu, "Nunu", "Nunu", "" },
    ChampionInfo{ ChampionId::MissFortune, "MissFortune", "MissFortune", "" },
    ChampionInfo{ ChampionId::Ashe, "Ashe", "Ashe", "" },
    ChampionInfo{ ChampionId::Tryndamere, "Tryndamere", "Tryndamere", "" },
    ChampionInfo{ ChampionId::Jax, "Jax", "Jax", "" },
    ChampionInfo{ ChampionId::Morgana, "Morgana", "Morgana", "" },
    ChampionInfo{ ChampionId::Zilean, "Zilean", "Zilean", "" },
    ChampionInfo{ ChampionId::Singed, "Singed", "Singed", "" },
    ChampionInfo{ ChampionId::Evelynn, "Evelynn", "Evelynn", "" },
    ChampionInfo{ ChampionId::Twitch, "Twitch", "Twitch", "" },
    ChampionInfo{ ChampionId::Karthus, "Karthus", "Karthus", "" },
    ChampionInfo{ ChampionId::Chogath, "Chogath", "Chogath", "" },
    ChampionInfo{ ChampionId::Amumu, "Amumu", "Amumu", "" },
    ChampionInfo{ ChampionId::Rammus, "Rammus", "Rammus", "" },
    ChampionInfo{ ChampionId::Anivia, "Anivia", "Anivia", "" },
    ChampionInfo{ ChampionId::Shaco, "Shaco", "Shaco", "" },
    ChampionInfo{ ChampionId::DrMundo, "DrMundo", "DrMundo", "" },
    ChampionInfo{ ChampionId::Sona, "Sona", "Sona", "" },
    ChampionInfo{ ChampionId::Kassadin, "Kassadin", "Kassadin", "" },
    ChampionInfo{ ChampionId::Irelia, "Irelia", "Irelia", "" },
    ChampionInfo{ ChampionId::Janna, "Janna", "Janna", "" },
    ChampionInfo{ ChampionId::Gangplank, "Gangplank", "Gangplank", "" },
    ChampionInfo{ ChampionId::Corki, "Corki", "Corki", "" },
    ChampionInfo{ ChampionId::Karma, "Karma", "Karma", "" },
    ChampionInfo{ ChampionId::Taric, "Taric", "Taric", "" },
    ChampionInfo{ ChampionId::Veigar, "Veigar", "Veigar", "" },
    ChampionInfo{ ChampionId::Trundle, "Trundle", "Trundle", "" },
    ChampionInfo{ ChampionId::Swain, "Swain", "Swain", "" },
    ChampionInfo{ ChampionId::Caitlyn, "Caitlyn", "Caitlyn", "" },
    ChampionInfo{ ChampionId::Blitzcrank, "Blitzcrank", "Blitzcrank", "" },
    ChampionInfo{ ChampionId::Malphite, "Malphite", "Malphite", "" },
    ChampionInfo{ ChampionId::Katarina, "Katarina", "Katarina", "" },
    ChampionInfo{ ChampionId::Nocturne, "Nocturne", "Nocturne", "" },
    ChampionInfo{ ChampionId::Maokai, "Maokai", "Maokai", "" },
    ChampionInfo{ ChampionId::Renekton, "Renekton", "Renekton", "" },
    ChampionInfo{ ChampionId::JarvanIV, "JarvanIV", "JarvanIV", "" },
    ChampionInfo{ ChampionId::Elise, "Elise", "Elise", "" },
    ChampionInfo{ ChampionId::Orianna, "Orianna", "Orianna", "" },
    ChampionInfo{ ChampionId::MonkeyKing, "MonkeyKing", "MonkeyKing", "" },
    ChampionInfo{ ChampionId::Brand, "Brand", "Brand", "" },
    ChampionInfo{ ChampionId::LeeSin, "LeeSin", "LeeSin", "" },
    ChampionInfo{ ChampionId::Vayne, "Vayne", "Vayne", "" },
    ChampionInfo{ ChampionId::Rumble, "Rumble", "Rumble", "" },
    ChampionInfo{ ChampionId::Cassiopeia, "Cassiopeia", "Cassiopeia", "" },
    ChampionInfo{ ChampionId::Skarner, "Skarner", "Skarner", "" },
    ChampionInfo{ ChampionId::Heimerdinger, "Heimerdinger", "Heimerdinger", "" },
    ChampionInfo{ ChampionId::Nasus, "Nasus", "Nasus", "" },
    ChampionInfo{ ChampionId::Nidalee, "Nidalee", "Nidalee", "" },
    ChampionInfo{ ChampionId::Udyr, "Udyr", "Udyr", "" },
    ChampionInfo{ ChampionId::Poppy, "Poppy", "Poppy", "" },
    ChampionInfo{ ChampionId::Gragas, "Gragas", "Gragas", "" },
    ChampionInfo{ ChampionId::Pantheon, "Pantheon", "Pantheon", "" },
    ChampionInfo{ ChampionId::Ezreal, "Ezreal", "Ezreal", "" },
    ChampionInfo{ ChampionId::Mordekaiser, "Mordekaiser", "Mordekaiser", "" },
    ChampionInfo{ ChampionId::Yorick, "Yorick", "Yorick", "" },
    ChampionInfo{ ChampionId::Akali, "Akali", "Akali", "" },
    ChampionInfo{ ChampionId::Kennen, "Kennen", "Kennen", "" },
    ChampionInfo{ ChampionId::Garen, "Garen", "Garen", "" },
    ChampionInfo{ ChampionId::Leona, "Leona", "Leona", "" },
    ChampionInfo{ ChampionId::Malzahar, "Malzahar", "Malzahar", "" },
    ChampionInfo{ ChampionId::Talon, "Talon", "Talon", "" },
    ChampionInfo{ ChampionId::Riven, "Riven", "Riven", "" },
    ChampionInfo{ ChampionId::KogMaw, "KogMaw", "KogMaw", "" },
    ChampionInfo{ ChampionId::Shen, "Shen", "Shen", "" },
    ChampionInfo{ ChampionId::Lux, "Lux", "Lux", "" },
    ChampionInfo{ ChampionId::Xerath, "Xerath", "Xerath", "" },
    ChampionInfo{ ChampionId::Shyvana, "Shyvana", "Shyvana", "" },
    ChampionInfo{ ChampionId::Ahri, "Ahri", "Ahri", "" },
    ChampionInfo{ ChampionId::Graves, "Graves", "Graves", "" },
    ChampionInfo{ ChampionId::Fizz, "Fizz", "Fizz", "" },
    ChampionInfo{ ChampionId::Volibear, "Volibear", "Volibear", "" },
    ChampionInfo{ ChampionId::Rengar, "Rengar", "Rengar", "" },
    ChampionInfo{ ChampionId::Varus, "Varus", "Varus", "" },
    ChampionInfo{ ChampionId::Nautilus, "Nautilus", "Nautilus", "" },
    ChampionInfo{ ChampionId::Viktor, "Viktor", "Viktor", "" },
    ChampionInfo{ ChampionId::Sejuani, "Sejuani", "Sejuani", "" },
    ChampionInfo{ ChampionId::Fiora, "Fiora", "Fiora", "" },
    ChampionInfo{ ChampionId::Ziggs, "Ziggs", "Ziggs", "" },
    ChampionInfo{ ChampionId::Lulu, "Lulu", "Lulu", "" },
    ChampionInfo{ ChampionId::Draven, "Draven", "Draven", "" },
    ChampionInfo{ ChampionId::Hecarim, "Hecarim", "Hecarim", "" },
    ChampionInfo{ ChampionId::KhaZix, "Khazix", "Khazix", "Kha'Zix" },
    ChampionInfo{ ChampionId::Darius, "Darius", "Darius", "" },
    ChampionInfo{ ChampionId::Jayce, "Jayce", "Jayce", "" },
    ChampionInfo{ ChampionId::Lissandra, "Lissandra", "Lissandra", "" },
    ChampionInfo{ ChampionId::Diana, "Diana", "Diana", "" },
    ChampionInfo{ ChampionId::Quinn, "Quinn", "Quinn", "" },
    ChampionInfo{ ChampionId::Syndra, "Syndra", "Syndra", "" },
    ChampionInfo{ ChampionId::AurelionSol, "AurelionSol", "AurelionSol", "" },
    ChampionInfo{ ChampionId::Kayn, "Kayn", "Kayn", "" },
    ChampionInfo{ ChampionId::Zoe, "Zoe", "Zoe", "" },
    ChampionInfo{ ChampionId::Zyra, "Zyra", "Zyra", "" },
    ChampionInfo{ ChampionId::Kaisa, "Kaisa", "Kaisa", "" },
    ChampionInfo{ ChampionId::Seraphine, "Seraphine", "Seraphine", "" },
    ChampionInfo{ ChampionId::Gnar, "Gnar", "Gnar", "" },
    ChampionInfo{ ChampionId::Zac, "Zac", "Zac", "" },
    ChampionInfo{ ChampionId::Yasuo, "Yasuo", "Yasuo", "" },
    ChampionInfo{ ChampionId::Velkoz, "Velkoz", "Velkoz", "" },
    ChampionInfo{ ChampionId::Taliyah, "Taliyah", "Taliyah", "" },
    ChampionInfo{ ChampionId::Camille, "Camille", "Camille", "" },
    ChampionInfo{ ChampionId::Akshan, "Akshan", "Akshan", "" },
    ChampionInfo{ ChampionId::Belveth, "Belveth", "Belveth", "" },
    ChampionInfo{ ChampionId::Braum, "Braum", "Braum", "" },
    ChampionInfo{ ChampionId::Jhin, "Jhin", "Jhin", "" },
    ChampionInfo{ ChampionId::Kindred, "Kindred", "Kindred", "" },
    ChampionInfo{ ChampionId::Zeri, "Zeri", "Zeri", "" },
    ChampionInfo{ ChampionId::Jinx, "Jinx", "Jinx", "" },
    ChampionInfo{ ChampionId::TahmKench, "TahmKench", "TahmKench", "" },
    ChampionInfo{ ChampionId::Briar, "Briar", "Briar", "" },
    ChampionInfo{ ChampionId::Viego, "Viego", "Viego", "" },
    ChampionInfo{ ChampionId::Senna, "Senna", "Senna", "" },
    ChampionInfo{ ChampionId::Lucian, "Lucian", "Lucian", "" },
    ChampionInfo{ ChampionId::Zed, "Zed", "Zed", "" },
    ChampionInfo{ ChampionId::Kled, "Kled", "Kled", "" },
    ChampionInfo{ ChampionId::Ekko, "Ekko", "Ekko", "" },
    ChampionInfo{ ChampionId::Qiyana, "Qiyana", "Qiyana", "" },
    ChampionInfo{ ChampionId::Vi, "Vi", "Vi", "" },
    ChampionInfo{ ChampionId::Aatrox, "Aatrox", "Aatrox", "" },
    ChampionInfo{ ChampionId::Nami, "Nami", "Nami", "" },
    ChampionInfo{ ChampionId::Azir, "Azir", "Azir", "" },
    ChampionInfo{ ChampionId::Yuumi, "Yuumi", "Yuumi", "" },
    ChampionInfo{ ChampionId::Samira, "Samira", "Samira", "" },
    ChampionInfo{ ChampionId::Thresh, "Thresh", "Thresh", "" },
    ChampionInfo{ ChampionId::Illaoi, "Illaoi", "Illaoi", "" },
    ChampionInfo{ ChampionId::RekSai, "RekSai", "RekSai", "" },
    ChampionInfo{ ChampionId::Ivern, "Ivern", "Ivern", "" },
    ChampionInfo{ ChampionId::Kalista, "Kalista", "Kalista", "" },
    ChampionInfo{ ChampionId::Bard, "Bard", "Bard", "" },
    ChampionInfo{ ChampionId::Rakan, "Rakan", "Rakan", "" },
    ChampionInfo{ ChampionId::Xayah, "Xayah", "Xayah", "" },
    ChampionInfo{ ChampionId::Ornn, "Ornn", "Ornn", "" },
    ChampionInfo{ ChampionId::Sylas, "Sylas", "Sylas", "" },
    ChampionInfo{ ChampionId::Neeko, "Neeko", "Neeko", "" },
    ChampionInfo{ ChampionId::Aphelios, "Aphelios", "Aphelios", "" },
    ChampionInfo{ ChampionId::Rell, "Rell", "Rell", "" },
    ChampionInfo{ ChampionId::Pyke, "Pyke", "Pyke", "" },
    ChampionInfo{ ChampionId::Vex, "Vex", "Vex", "" },
    ChampionInfo{ ChampionId::Yone, "Yone", "Yone", "" },
    ChampionInfo{ ChampionId::Ambessa, "Ambessa", "Ambessa", "" },
    ChampionInfo{ ChampionId::Mel, "Mel", "Mel", "" },
    ChampionInfo{ ChampionId::Yunara, "Yunara", "Yunara", "" },
    ChampionInfo{ ChampionId::Locke, "Locke", "Locke", "" },
    ChampionInfo{ ChampionId::Sett, "Sett", "Sett", "" },
    ChampionInfo{ ChampionId::Lillia, "Lillia", "Lillia", "" },
    ChampionInfo{ ChampionId::Gwen, "Gwen", "Gwen", "" },
    ChampionInfo{ ChampionId::RenataGlasc, "RenataGlasc", "Renata", "" },
    ChampionInfo{ ChampionId::Aurora, "Aurora", "Aurora", "" },
    ChampionInfo{ ChampionId::Nilah, "Nilah", "Nilah", "" },
    ChampionInfo{ ChampionId::KSante, "KSante", "KSante", "" },
    ChampionInfo{ ChampionId::Smolder, "Smolder", "Smolder", "" },
    ChampionInfo{ ChampionId::Milio, "Milio", "Milio", "" },
    ChampionInfo{ ChampionId::Zaahen, "Zaahen", "Zaahen", "" },
    ChampionInfo{ ChampionId::Hwei, "Hwei", "Hwei", "" },
    ChampionInfo{ ChampionId::Naafiri, "Naafiri", "Naafiri", "" },
}};

constexpr int ChampionIdValue(ChampionId id) {
    return static_cast<int>(id);
}

constexpr const char* ChampionName(ChampionId id) {
    switch (id) {
    case ChampionId::Annie: return "Annie";
    case ChampionId::Olaf: return "Olaf";
    case ChampionId::Galio: return "Galio";
    case ChampionId::TwistedFate: return "TwistedFate";
    case ChampionId::XinZhao: return "XinZhao";
    case ChampionId::Urgot: return "Urgot";
    case ChampionId::Leblanc: return "Leblanc";
    case ChampionId::Vladimir: return "Vladimir";
    case ChampionId::Fiddlesticks: return "Fiddlesticks";
    case ChampionId::Kayle: return "Kayle";
    case ChampionId::MasterYi: return "MasterYi";
    case ChampionId::Alistar: return "Alistar";
    case ChampionId::Ryze: return "Ryze";
    case ChampionId::Sion: return "Sion";
    case ChampionId::Sivir: return "Sivir";
    case ChampionId::Soraka: return "Soraka";
    case ChampionId::Teemo: return "Teemo";
    case ChampionId::Tristana: return "Tristana";
    case ChampionId::Warwick: return "Warwick";
    case ChampionId::Nunu: return "Nunu";
    case ChampionId::MissFortune: return "MissFortune";
    case ChampionId::Ashe: return "Ashe";
    case ChampionId::Tryndamere: return "Tryndamere";
    case ChampionId::Jax: return "Jax";
    case ChampionId::Morgana: return "Morgana";
    case ChampionId::Zilean: return "Zilean";
    case ChampionId::Singed: return "Singed";
    case ChampionId::Evelynn: return "Evelynn";
    case ChampionId::Twitch: return "Twitch";
    case ChampionId::Karthus: return "Karthus";
    case ChampionId::Chogath: return "Chogath";
    case ChampionId::Amumu: return "Amumu";
    case ChampionId::Rammus: return "Rammus";
    case ChampionId::Anivia: return "Anivia";
    case ChampionId::Shaco: return "Shaco";
    case ChampionId::DrMundo: return "DrMundo";
    case ChampionId::Sona: return "Sona";
    case ChampionId::Kassadin: return "Kassadin";
    case ChampionId::Irelia: return "Irelia";
    case ChampionId::Janna: return "Janna";
    case ChampionId::Gangplank: return "Gangplank";
    case ChampionId::Corki: return "Corki";
    case ChampionId::Karma: return "Karma";
    case ChampionId::Taric: return "Taric";
    case ChampionId::Veigar: return "Veigar";
    case ChampionId::Trundle: return "Trundle";
    case ChampionId::Swain: return "Swain";
    case ChampionId::Caitlyn: return "Caitlyn";
    case ChampionId::Blitzcrank: return "Blitzcrank";
    case ChampionId::Malphite: return "Malphite";
    case ChampionId::Katarina: return "Katarina";
    case ChampionId::Nocturne: return "Nocturne";
    case ChampionId::Maokai: return "Maokai";
    case ChampionId::Renekton: return "Renekton";
    case ChampionId::JarvanIV: return "JarvanIV";
    case ChampionId::Elise: return "Elise";
    case ChampionId::Orianna: return "Orianna";
    case ChampionId::MonkeyKing: return "MonkeyKing";
    case ChampionId::Brand: return "Brand";
    case ChampionId::LeeSin: return "LeeSin";
    case ChampionId::Vayne: return "Vayne";
    case ChampionId::Rumble: return "Rumble";
    case ChampionId::Cassiopeia: return "Cassiopeia";
    case ChampionId::Skarner: return "Skarner";
    case ChampionId::Heimerdinger: return "Heimerdinger";
    case ChampionId::Nasus: return "Nasus";
    case ChampionId::Nidalee: return "Nidalee";
    case ChampionId::Udyr: return "Udyr";
    case ChampionId::Poppy: return "Poppy";
    case ChampionId::Gragas: return "Gragas";
    case ChampionId::Pantheon: return "Pantheon";
    case ChampionId::Ezreal: return "Ezreal";
    case ChampionId::Mordekaiser: return "Mordekaiser";
    case ChampionId::Yorick: return "Yorick";
    case ChampionId::Akali: return "Akali";
    case ChampionId::Kennen: return "Kennen";
    case ChampionId::Garen: return "Garen";
    case ChampionId::Leona: return "Leona";
    case ChampionId::Malzahar: return "Malzahar";
    case ChampionId::Talon: return "Talon";
    case ChampionId::Riven: return "Riven";
    case ChampionId::KogMaw: return "KogMaw";
    case ChampionId::Shen: return "Shen";
    case ChampionId::Lux: return "Lux";
    case ChampionId::Xerath: return "Xerath";
    case ChampionId::Shyvana: return "Shyvana";
    case ChampionId::Ahri: return "Ahri";
    case ChampionId::Graves: return "Graves";
    case ChampionId::Fizz: return "Fizz";
    case ChampionId::Volibear: return "Volibear";
    case ChampionId::Rengar: return "Rengar";
    case ChampionId::Varus: return "Varus";
    case ChampionId::Nautilus: return "Nautilus";
    case ChampionId::Viktor: return "Viktor";
    case ChampionId::Sejuani: return "Sejuani";
    case ChampionId::Fiora: return "Fiora";
    case ChampionId::Ziggs: return "Ziggs";
    case ChampionId::Lulu: return "Lulu";
    case ChampionId::Draven: return "Draven";
    case ChampionId::Hecarim: return "Hecarim";
    case ChampionId::KhaZix: return "Khazix";
    case ChampionId::Darius: return "Darius";
    case ChampionId::Jayce: return "Jayce";
    case ChampionId::Lissandra: return "Lissandra";
    case ChampionId::Diana: return "Diana";
    case ChampionId::Quinn: return "Quinn";
    case ChampionId::Syndra: return "Syndra";
    case ChampionId::AurelionSol: return "AurelionSol";
    case ChampionId::Kayn: return "Kayn";
    case ChampionId::Zoe: return "Zoe";
    case ChampionId::Zyra: return "Zyra";
    case ChampionId::Kaisa: return "Kaisa";
    case ChampionId::Seraphine: return "Seraphine";
    case ChampionId::Gnar: return "Gnar";
    case ChampionId::Zac: return "Zac";
    case ChampionId::Yasuo: return "Yasuo";
    case ChampionId::Velkoz: return "Velkoz";
    case ChampionId::Taliyah: return "Taliyah";
    case ChampionId::Camille: return "Camille";
    case ChampionId::Akshan: return "Akshan";
    case ChampionId::Belveth: return "Belveth";
    case ChampionId::Braum: return "Braum";
    case ChampionId::Jhin: return "Jhin";
    case ChampionId::Kindred: return "Kindred";
    case ChampionId::Zeri: return "Zeri";
    case ChampionId::Jinx: return "Jinx";
    case ChampionId::TahmKench: return "TahmKench";
    case ChampionId::Briar: return "Briar";
    case ChampionId::Viego: return "Viego";
    case ChampionId::Senna: return "Senna";
    case ChampionId::Lucian: return "Lucian";
    case ChampionId::Zed: return "Zed";
    case ChampionId::Kled: return "Kled";
    case ChampionId::Ekko: return "Ekko";
    case ChampionId::Qiyana: return "Qiyana";
    case ChampionId::Vi: return "Vi";
    case ChampionId::Aatrox: return "Aatrox";
    case ChampionId::Nami: return "Nami";
    case ChampionId::Azir: return "Azir";
    case ChampionId::Yuumi: return "Yuumi";
    case ChampionId::Samira: return "Samira";
    case ChampionId::Thresh: return "Thresh";
    case ChampionId::Illaoi: return "Illaoi";
    case ChampionId::RekSai: return "RekSai";
    case ChampionId::Ivern: return "Ivern";
    case ChampionId::Kalista: return "Kalista";
    case ChampionId::Bard: return "Bard";
    case ChampionId::Rakan: return "Rakan";
    case ChampionId::Xayah: return "Xayah";
    case ChampionId::Ornn: return "Ornn";
    case ChampionId::Sylas: return "Sylas";
    case ChampionId::Neeko: return "Neeko";
    case ChampionId::Aphelios: return "Aphelios";
    case ChampionId::Rell: return "Rell";
    case ChampionId::Pyke: return "Pyke";
    case ChampionId::Vex: return "Vex";
    case ChampionId::Yone: return "Yone";
    case ChampionId::Ambessa: return "Ambessa";
    case ChampionId::Mel: return "Mel";
    case ChampionId::Yunara: return "Yunara";
    case ChampionId::Locke: return "Locke";
    case ChampionId::Sett: return "Sett";
    case ChampionId::Lillia: return "Lillia";
    case ChampionId::Gwen: return "Gwen";
    case ChampionId::RenataGlasc: return "RenataGlasc";
    case ChampionId::Aurora: return "Aurora";
    case ChampionId::Nilah: return "Nilah";
    case ChampionId::KSante: return "KSante";
    case ChampionId::Smolder: return "Smolder";
    case ChampionId::Milio: return "Milio";
    case ChampionId::Zaahen: return "Zaahen";
    case ChampionId::Hwei: return "Hwei";
    case ChampionId::Naafiri: return "Naafiri";
    default: return "";
    }
}

inline bool ChampionNameEquals(const char* left, const char* right) {
    if (!left || !right) {
        return false;
    }
    return ::_stricmp(left, right) == 0;
}

struct ChampionAlias {
    const char* Name;
    ChampionId Id;
};

inline constexpr std::array<ChampionAlias, 8> kChampionAliases = {{
    ChampionAlias{ "Renata", ChampionId::RenataGlasc },
    ChampionAlias{ "Jarvan", ChampionId::JarvanIV },
    ChampionAlias{ "NunuWillump", ChampionId::Nunu },
    ChampionAlias{ "BlueKayn", ChampionId::Kayn },
    ChampionAlias{ "JayceHammer", ChampionId::Jayce },
    ChampionAlias{ "JayceStanceHtG", ChampionId::Jayce },
    ChampionAlias{ "NidaleeCougar", ChampionId::Nidalee },
    ChampionAlias{ "Cho'Gath", ChampionId::Chogath },
}};

inline ChampionId ChampionIdFromHash(std::uint32_t hash) {
    struct HashEntry {
        std::uint32_t hash;
        ChampionId id;
    };
    static const std::vector<HashEntry> table = [] {
        std::vector<HashEntry> entries;
        entries.reserve(768);
        auto add = [&entries](const char* name, ChampionId id) {
            if (name && name[0]) {
                entries.push_back({ Utils::HashName(name), id });
            }
        };
        for (const auto& champion : kChampions) {
            add(champion.Name, champion.Id);
            add(champion.Alias, champion.Id);
            add(champion.DisplayName, champion.Id);
        }
        for (const auto& alias : kChampionAliases) {
            add(alias.Name, alias.Id);
        }
        std::stable_sort(
            entries.begin(), entries.end(),
            [](const HashEntry& left, const HashEntry& right) {
                return left.hash < right.hash;
            });
        entries.erase(
            std::unique(
                entries.begin(), entries.end(),
                [](const HashEntry& left, const HashEntry& right) {
                    return left.hash == right.hash;
                }),
            entries.end());
        return entries;
    }();

    const auto it = std::lower_bound(
        table.begin(), table.end(), hash,
        [](const HashEntry& entry, std::uint32_t value) {
            return entry.hash < value;
        });
    return it != table.end() && it->hash == hash
        ? it->id
        : ChampionId::Unknown;
}

// FNV-1a hash based lookup: the previous implementation did up to three
// _stricmp against every champion name (≈500 string compares) per call across
// the per-frame orbwalker/evade/activator hot paths. Hashing the input once
// and binary-searching a compacted hash table turns that into a uint32 compare
// against a few entries.
inline ChampionId ChampionIdFromName(const char* name) {
    if (!name || !name[0]) {
        return ChampionId::Unknown;
    }
    return ChampionIdFromHash(Utils::HashName(name));
}

inline const ChampionInfo* FindChampion(ChampionId id) {
    for (const auto& champion : kChampions) {
        if (champion.Id == id) return &champion;
    }
    return nullptr;
}

} // namespace SDK
