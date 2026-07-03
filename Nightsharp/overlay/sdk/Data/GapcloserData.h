#pragma once
#include "../Enumerations/GapcloserType.h"
#include "../Enumerations/SpellSlot.h"

#include <cctype>

namespace SDK::Generated::GapcloserData {

struct GapcloserEntry {
    const char* ChampionName;
    SDK::GapcloserType SkillType;
    SDK::SpellSlot Slot;
    const char* SpellName;
    bool Invert;
};

inline constexpr int kGapcloserEntryCount = 97;
inline const GapcloserEntry kGapcloserEntries[] = {
    GapcloserEntry{ "Aatrox", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "aatroxe", false },
    GapcloserEntry{ "Ahri", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "ahritumble", false },
    GapcloserEntry{ "Akali", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "akalie", true },
    GapcloserEntry{ "Akali", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "akalir", false },
    GapcloserEntry{ "Akali", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "akalirb", false },
    GapcloserEntry{ "Alistar", SDK::GapcloserType::Targeted, SDK::SpellSlot::W, "headbutt", false },
    GapcloserEntry{ "Ambessa", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "ambessae", false },
    GapcloserEntry{ "Ambessa", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "ambessar", false },
    GapcloserEntry{ "Aurora", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "aurorae", false },
    GapcloserEntry{ "Azir", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "azire", false },
    GapcloserEntry{ "BelVeth", SDK::GapcloserType::Skillshot, SDK::SpellSlot::Q, "belvethq", false },
    GapcloserEntry{ "Briar", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "briare", false },
    GapcloserEntry{ "Briar", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "briarr", false },
    GapcloserEntry{ "Caitlyn", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "caitlynentrapment", true },
    GapcloserEntry{ "Camille", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "camilleedash2", false },
    GapcloserEntry{ "Corki", SDK::GapcloserType::Skillshot, SDK::SpellSlot::W, "carpetbomb", false },
    GapcloserEntry{ "Corki", SDK::GapcloserType::Skillshot, SDK::SpellSlot::W, "carpetbombmega", false },
    GapcloserEntry{ "Diana", SDK::GapcloserType::Targeted, SDK::SpellSlot::R, "dianateleport", false },
    GapcloserEntry{ "Ekko", SDK::GapcloserType::Targeted, SDK::SpellSlot::E, "ekkoeattack", false },
    GapcloserEntry{ "Elise", SDK::GapcloserType::Targeted, SDK::SpellSlot::E, "elisespideredescent", false },
    GapcloserEntry{ "Fiora", SDK::GapcloserType::Skillshot, SDK::SpellSlot::Q, "fioraq", false },
    GapcloserEntry{ "Fizz", SDK::GapcloserType::Targeted, SDK::SpellSlot::Q, "fizzq", false },
    GapcloserEntry{ "Galio", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "galioe", false },
    GapcloserEntry{ "Gnar", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "gnare", false },
    GapcloserEntry{ "Gnar", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "gnarbige", false },
    GapcloserEntry{ "Gragas", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "gragase", false },
    GapcloserEntry{ "Graves", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "gravesmove", false },
    GapcloserEntry{ "Graves", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "graveschargeshot", true },
    GapcloserEntry{ "Gwen", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "gwene", false },
    GapcloserEntry{ "Hecarim", SDK::GapcloserType::Targeted, SDK::SpellSlot::E, "hecarimrampattack", false },
    GapcloserEntry{ "Illaoi", SDK::GapcloserType::Targeted, SDK::SpellSlot::W, "illaoiwattack", false },
    GapcloserEntry{ "Irelia", SDK::GapcloserType::Targeted, SDK::SpellSlot::Q, "ireliaq", false },
    GapcloserEntry{ "JarvanIV", SDK::GapcloserType::Targeted, SDK::SpellSlot::R, "jarvanivcataclysm", false },
    GapcloserEntry{ "Jax", SDK::GapcloserType::Targeted, SDK::SpellSlot::Q, "jaxleapstrike", false },
    GapcloserEntry{ "Jayce", SDK::GapcloserType::Targeted, SDK::SpellSlot::Q, "jaycetotheskies", false },
    GapcloserEntry{ "KSante", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "ksantee", false },
    GapcloserEntry{ "KSante", SDK::GapcloserType::Targeted, SDK::SpellSlot::R, "ksanter", false },
    GapcloserEntry{ "Kaisa", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "kaisar", false },
    GapcloserEntry{ "Kassadin", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "riftwalk", false },
    GapcloserEntry{ "Katarina", SDK::GapcloserType::Targeted, SDK::SpellSlot::E, "katarinae", false },
    GapcloserEntry{ "Katarina", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "katarinaedagger", false },
    GapcloserEntry{ "Kayn", SDK::GapcloserType::Skillshot, SDK::SpellSlot::Q, "kaynq", false },
    GapcloserEntry{ "Khazix", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "khazixe", false },
    GapcloserEntry{ "Khazix", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "khazixelong", false },
    GapcloserEntry{ "Kindred", SDK::GapcloserType::Skillshot, SDK::SpellSlot::Q, "kindredq", false },
    GapcloserEntry{ "Kled", SDK::GapcloserType::Skillshot, SDK::SpellSlot::Q, "kledriderq", true },
    GapcloserEntry{ "Kled", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "klede", false },
    GapcloserEntry{ "Leblanc", SDK::GapcloserType::Skillshot, SDK::SpellSlot::W, "leblancw", false },
    GapcloserEntry{ "Leblanc", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "leblancrw", false },
    GapcloserEntry{ "LeeSin", SDK::GapcloserType::Targeted, SDK::SpellSlot::Q, "blindmonkqtwo", false },
    GapcloserEntry{ "Lucian", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "luciane", false },
    GapcloserEntry{ "Malphite", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "ufslash", false },
    GapcloserEntry{ "Maokai", SDK::GapcloserType::Targeted, SDK::SpellSlot::W, "maokaiw", false },
    GapcloserEntry{ "MasterYi", SDK::GapcloserType::Targeted, SDK::SpellSlot::Q, "alphastrike", false },
    GapcloserEntry{ "MonkeyKing", SDK::GapcloserType::Targeted, SDK::SpellSlot::E, "monkeykingnimbus", false },
    GapcloserEntry{ "Naafiri", SDK::GapcloserType::Skillshot, SDK::SpellSlot::W, "naafiriw", false },
    GapcloserEntry{ "Nidalee", SDK::GapcloserType::Skillshot, SDK::SpellSlot::W, "pounce", false },
    GapcloserEntry{ "Nilah", SDK::GapcloserType::Targeted, SDK::SpellSlot::E, "nilahe", false },
    GapcloserEntry{ "Nocturne", SDK::GapcloserType::Targeted, SDK::SpellSlot::R, "nocturneparanoia2", false },
    GapcloserEntry{ "Ornn", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "ornne", false },
    GapcloserEntry{ "Pantheon", SDK::GapcloserType::Targeted, SDK::SpellSlot::W, "pantheonw", false },
    GapcloserEntry{ "Pantheon", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "pantheonrfall", false },
    GapcloserEntry{ "Poppy", SDK::GapcloserType::Targeted, SDK::SpellSlot::E, "poppye", false },
    GapcloserEntry{ "Pyke", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "pykee", false },
    GapcloserEntry{ "Qiyana", SDK::GapcloserType::Targeted, SDK::SpellSlot::E, "qiyanae", false },
    GapcloserEntry{ "Rakan", SDK::GapcloserType::Skillshot, SDK::SpellSlot::W, "rakanw", false },
    GapcloserEntry{ "RekSai", SDK::GapcloserType::Targeted, SDK::SpellSlot::R, "reksair", false },
    GapcloserEntry{ "Rell", SDK::GapcloserType::Skillshot, SDK::SpellSlot::W, "rellw", false },
    GapcloserEntry{ "Renekton", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "renektonsliceanddice", false },
    GapcloserEntry{ "Rengar", SDK::GapcloserType::Targeted, SDK::SpellSlot::Q, "rengarpassiveleap", false },
    GapcloserEntry{ "Riven", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "rivenfeint", false },
    GapcloserEntry{ "Samira", SDK::GapcloserType::Targeted, SDK::SpellSlot::E, "samirae", false },
    GapcloserEntry{ "Sejuani", SDK::GapcloserType::Skillshot, SDK::SpellSlot::Q, "sejuaniq", false },
    GapcloserEntry{ "Sett", SDK::GapcloserType::Targeted, SDK::SpellSlot::R, "settr", false },
    GapcloserEntry{ "Shen", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "shene", false },
    GapcloserEntry{ "Shyvana", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "shyvanatransformcast", false },
    GapcloserEntry{ "Sylas", SDK::GapcloserType::Targeted, SDK::SpellSlot::W, "sylasw", false },
    GapcloserEntry{ "Sylas", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "sylase", false },
    GapcloserEntry{ "Sylas", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "sylase2", false },
    GapcloserEntry{ "Talon", SDK::GapcloserType::Targeted, SDK::SpellSlot::Q, "talonq", false },
    GapcloserEntry{ "Thresh", SDK::GapcloserType::Skillshot, SDK::SpellSlot::Q, "threshqleap", false },
    GapcloserEntry{ "Tristana", SDK::GapcloserType::Skillshot, SDK::SpellSlot::W, "tristanaw", false },
    GapcloserEntry{ "Tryndamere", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "tryndameree", false },
    GapcloserEntry{ "Urgot", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "urgote", false },
    GapcloserEntry{ "Vayne", SDK::GapcloserType::Skillshot, SDK::SpellSlot::Q, "vaynetumble", false },
    GapcloserEntry{ "Vex", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "vexr", false },
    GapcloserEntry{ "Vi", SDK::GapcloserType::Skillshot, SDK::SpellSlot::Q, "viq", false },
    GapcloserEntry{ "Viego", SDK::GapcloserType::Skillshot, SDK::SpellSlot::W, "viegow", false },
    GapcloserEntry{ "Viego", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "viegor", false },
    GapcloserEntry{ "Warwick", SDK::GapcloserType::Targeted, SDK::SpellSlot::R, "warwickr", false },
    GapcloserEntry{ "XinZhao", SDK::GapcloserType::Targeted, SDK::SpellSlot::E, "xinzhaoe", false },
    GapcloserEntry{ "Yasuo", SDK::GapcloserType::Targeted, SDK::SpellSlot::E, "yasuodashwrapper", false },
    GapcloserEntry{ "Yone", SDK::GapcloserType::Skillshot, SDK::SpellSlot::Q, "yoneq3", false },
    GapcloserEntry{ "Yone", SDK::GapcloserType::Skillshot, SDK::SpellSlot::R, "yoner", false },
    GapcloserEntry{ "Zac", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "zace", false },
    GapcloserEntry{ "Zed", SDK::GapcloserType::Targeted, SDK::SpellSlot::R, "zedr", false },
    GapcloserEntry{ "Zeri", SDK::GapcloserType::Skillshot, SDK::SpellSlot::E, "zerie", false }
};

namespace detail {
    inline char Lower(char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    inline bool EqualsIgnoreCase(const char* a, const char* b) {
        if (!a || !b) {
            return false;
        }

        while (*a && *b) {
            if (Lower(*a++) != Lower(*b++)) {
                return false;
            }
        }

        return *a == 0 && *b == 0;
    }
} // namespace detail

inline const GapcloserEntry* FindBySpellName(const char* spellName) {
    if (!spellName || !spellName[0]) {
        return nullptr;
    }

    for (int i = 0; i < kGapcloserEntryCount; ++i) {
        if (detail::EqualsIgnoreCase(kGapcloserEntries[i].SpellName, spellName)) {
            return &kGapcloserEntries[i];
        }
    }

    return nullptr;
}

} // namespace SDK::Generated::GapcloserData
