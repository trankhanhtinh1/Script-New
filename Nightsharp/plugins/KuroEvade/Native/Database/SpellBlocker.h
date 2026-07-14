#pragma once

#include "../../../../SDK/SDK.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace Plugins::KuroEvade::Database {

class SpellBlocker final {
public:
    static bool ShouldBlock(std::string_view champion, SDK::SpellSlot slot) {
        if (slot == SDK::SpellSlot::Summoner1 || slot == SDK::SpellSlot::Summoner2) {
            return false;
        }
        const std::uint8_t bit = SlotBit(slot);
        if (bit == 0) {
            return false;
        }
        for (const ChampionPolicy& policy : Policies) {
            if (_stricmp(policy.Champion.data(), champion.data()) == 0) {
                return (policy.AllowedMask & bit) == 0;
            }
        }
        return false;
    }

    static bool ShouldBlockForPlayer(int rawSlot) {
        const SDK::AIHeroClient player = SDK::ObjectManager::Player();
        return player.IsValid() && ShouldBlock(
            player.CharacterName(), static_cast<SDK::SpellSlot>(rawSlot));
    }

private:
    struct ChampionPolicy {
        std::string_view Champion;
        std::uint8_t AllowedMask;
    };

    static constexpr std::uint8_t SlotBit(SDK::SpellSlot slot) {
        switch (slot) {
        case SDK::SpellSlot::Q: return 1;
        case SDK::SpellSlot::W: return 2;
        case SDK::SpellSlot::E: return 4;
        case SDK::SpellSlot::R: return 8;
        default: return 0;
        }
    }

    static inline constexpr std::array<ChampionPolicy, 151> Policies = {{
        ChampionPolicy{ "aatrox", 10 },
        ChampionPolicy{ "ahri", 10 },
        ChampionPolicy{ "akali", 10 },
        ChampionPolicy{ "alistar", 10 },
        ChampionPolicy{ "amumu", 10 },
        ChampionPolicy{ "anivia", 3 },
        ChampionPolicy{ "annie", 12 },
        ChampionPolicy{ "aphelios", 0 },
        ChampionPolicy{ "ashe", 9 },
        ChampionPolicy{ "aurelionsol", 6 },
        ChampionPolicy{ "azir", 12 },
        ChampionPolicy{ "bard", 14 },
        ChampionPolicy{ "blitzcrank", 6 },
        ChampionPolicy{ "brand", 12 },
        ChampionPolicy{ "braum", 14 },
        ChampionPolicy{ "caitlyn", 4 },
        ChampionPolicy{ "camille", 1 },
        ChampionPolicy{ "cassiopeia", 0 },
        ChampionPolicy{ "chogath", 10 },
        ChampionPolicy{ "corki", 6 },
        ChampionPolicy{ "darius", 11 },
        ChampionPolicy{ "diana", 10 },
        ChampionPolicy{ "draven", 10 },
        ChampionPolicy{ "drmundo", 10 },
        ChampionPolicy{ "ekko", 12 },
        ChampionPolicy{ "elise", 15 },
        ChampionPolicy{ "evelynn", 11 },
        ChampionPolicy{ "ezreal", 4 },
        ChampionPolicy{ "fiddlesticks", 1 },
        ChampionPolicy{ "fiora", 15 },
        ChampionPolicy{ "fizz", 7 },
        ChampionPolicy{ "galio", 6 },
        ChampionPolicy{ "gangplank", 6 },
        ChampionPolicy{ "garen", 11 },
        ChampionPolicy{ "gnar", 14 },
        ChampionPolicy{ "gragas", 15 },
        ChampionPolicy{ "graves", 12 },
        ChampionPolicy{ "hecarim", 15 },
        ChampionPolicy{ "heimerdinger", 8 },
        ChampionPolicy{ "illaoi", 2 },
        ChampionPolicy{ "irelia", 15 },
        ChampionPolicy{ "ivern", 4 },
        ChampionPolicy{ "janna", 5 },
        ChampionPolicy{ "jarvaniv", 15 },
        ChampionPolicy{ "jax", 15 },
        ChampionPolicy{ "jayce", 15 },
        ChampionPolicy{ "jhin", 0 },
        ChampionPolicy{ "jinx", 5 },
        ChampionPolicy{ "kaisa", 13 },
        ChampionPolicy{ "kalista", 12 },
        ChampionPolicy{ "karma", 14 },
        ChampionPolicy{ "karthus", 4 },
        ChampionPolicy{ "kassadin", 10 },
        ChampionPolicy{ "katarina", 6 },
        ChampionPolicy{ "kayle", 14 },
        ChampionPolicy{ "kayn", 13 },
        ChampionPolicy{ "kennen", 14 },
        ChampionPolicy{ "khazix", 12 },
        ChampionPolicy{ "kindred", 11 },
        ChampionPolicy{ "kled", 4 },
        ChampionPolicy{ "kogmaw", 2 },
        ChampionPolicy{ "leblanc", 10 },
        ChampionPolicy{ "leesin", 14 },
        ChampionPolicy{ "leona", 11 },
        ChampionPolicy{ "lillia", 0 },
        ChampionPolicy{ "lissandra", 12 },
        ChampionPolicy{ "lucian", 4 },
        ChampionPolicy{ "lulu", 14 },
        ChampionPolicy{ "lux", 6 },
        ChampionPolicy{ "malphite", 14 },
        ChampionPolicy{ "malzahar", 4 },
        ChampionPolicy{ "maokai", 14 },
        ChampionPolicy{ "masteryi", 13 },
        ChampionPolicy{ "missfortune", 2 },
        ChampionPolicy{ "monkeyking", 15 },
        ChampionPolicy{ "mordekaiser", 11 },
        ChampionPolicy{ "morgana", 12 },
        ChampionPolicy{ "nami", 6 },
        ChampionPolicy{ "nasus", 9 },
        ChampionPolicy{ "nautilus", 15 },
        ChampionPolicy{ "neeko", 2 },
        ChampionPolicy{ "nidalee", 15 },
        ChampionPolicy{ "nocturne", 10 },
        ChampionPolicy{ "nunu", 2 },
        ChampionPolicy{ "olaf", 14 },
        ChampionPolicy{ "orianna", 7 },
        ChampionPolicy{ "ornn", 0 },
        ChampionPolicy{ "pantheon", 6 },
        ChampionPolicy{ "poppy", 15 },
        ChampionPolicy{ "pyke", 6 },
        ChampionPolicy{ "qiyana", 6 },
        ChampionPolicy{ "quinn", 15 },
        ChampionPolicy{ "rakan", 14 },
        ChampionPolicy{ "rammus", 11 },
        ChampionPolicy{ "reksai", 15 },
        ChampionPolicy{ "renekton", 14 },
        ChampionPolicy{ "rengar", 3 },
        ChampionPolicy{ "riven", 13 },
        ChampionPolicy{ "rumble", 2 },
        ChampionPolicy{ "ryze", 8 },
        ChampionPolicy{ "samira", 14 },
        ChampionPolicy{ "sejuani", 7 },
        ChampionPolicy{ "senna", 4 },
        ChampionPolicy{ "sett", 9 },
        ChampionPolicy{ "shaco", 9 },
        ChampionPolicy{ "shen", 14 },
        ChampionPolicy{ "shyvana", 15 },
        ChampionPolicy{ "singed", 9 },
        ChampionPolicy{ "sion", 14 },
        ChampionPolicy{ "sivir", 12 },
        ChampionPolicy{ "skarner", 15 },
        ChampionPolicy{ "sona", 14 },
        ChampionPolicy{ "soraka", 15 },
        ChampionPolicy{ "swain", 15 },
        ChampionPolicy{ "sylas", 0 },
        ChampionPolicy{ "syndra", 11 },
        ChampionPolicy{ "tahmkench", 6 },
        ChampionPolicy{ "taliyah", 0 },
        ChampionPolicy{ "talon", 13 },
        ChampionPolicy{ "taric", 8 },
        ChampionPolicy{ "teemo", 6 },
        ChampionPolicy{ "thresh", 1 },
        ChampionPolicy{ "tristana", 2 },
        ChampionPolicy{ "trundle", 15 },
        ChampionPolicy{ "tryndamere", 13 },
        ChampionPolicy{ "twistedfate", 6 },
        ChampionPolicy{ "twitch", 9 },
        ChampionPolicy{ "udyr", 15 },
        ChampionPolicy{ "urgot", 14 },
        ChampionPolicy{ "varus", 0 },
        ChampionPolicy{ "vayne", 9 },
        ChampionPolicy{ "veigar", 0 },
        ChampionPolicy{ "velkoz", 5 },
        ChampionPolicy{ "vi", 15 },
        ChampionPolicy{ "viktor", 14 },
        ChampionPolicy{ "vladimir", 10 },
        ChampionPolicy{ "volibear", 15 },
        ChampionPolicy{ "warwick", 15 },
        ChampionPolicy{ "xayah", 10 },
        ChampionPolicy{ "xerath", 0 },
        ChampionPolicy{ "xinzhao", 15 },
        ChampionPolicy{ "yasuo", 14 },
        ChampionPolicy{ "yone", 4 },
        ChampionPolicy{ "yorick", 0 },
        ChampionPolicy{ "yuumi", 15 },
        ChampionPolicy{ "zac", 14 },
        ChampionPolicy{ "zed", 14 },
        ChampionPolicy{ "ziggs", 2 },
        ChampionPolicy{ "zilean", 14 },
        ChampionPolicy{ "zoe", 8 },
        ChampionPolicy{ "zyra", 2 },
    }};
};

} // namespace Plugins::KuroEvade::Database
