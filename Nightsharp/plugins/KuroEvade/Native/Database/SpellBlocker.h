#pragma once

#include "../../../../SDK/SDK.h"

#include <array>
#include <cstdint>
#include <string>
namespace Plugins::KuroEvade::Database {

class SpellBlocker final {
public:
    static bool ShouldBlock(SDK::ChampionId championId, SDK::SpellSlot slot) {
        if (slot == SDK::SpellSlot::Summoner1 || slot == SDK::SpellSlot::Summoner2) {
            return false;
        }
        const std::uint8_t bit = SlotBit(slot);
        if (bit == 0 || championId == SDK::ChampionId::Unknown) {
            return false;
        }
        for (const ChampionPolicy& policy : Policies) {
            if (policy.Champion == championId) {
                return (policy.AllowedMask & bit) == 0;
            }
        }
        return false;
    }

    static bool ShouldBlockForPlayer(int rawSlot) {
        const SDK::AIHeroClient player = GameObjects::Player();
        if (!player.IsValid()) {
            return false;
        }
        const std::string championName = player.CharacterName();
        const SDK::ChampionId championId =
            SDK::ChampionIdFromName(championName.c_str());
        return ShouldBlock(championId, static_cast<SDK::SpellSlot>(rawSlot));
    }

private:
    struct ChampionPolicy {
        SDK::ChampionId Champion;
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
        ChampionPolicy{ SDK::ChampionId::Aatrox, 10 },
        ChampionPolicy{ SDK::ChampionId::Ahri, 10 },
        ChampionPolicy{ SDK::ChampionId::Akali, 10 },
        ChampionPolicy{ SDK::ChampionId::Alistar, 10 },
        ChampionPolicy{ SDK::ChampionId::Amumu, 10 },
        ChampionPolicy{ SDK::ChampionId::Anivia, 3 },
        ChampionPolicy{ SDK::ChampionId::Annie, 12 },
        ChampionPolicy{ SDK::ChampionId::Aphelios, 0 },
        ChampionPolicy{ SDK::ChampionId::Ashe, 9 },
        ChampionPolicy{ SDK::ChampionId::AurelionSol, 6 },
        ChampionPolicy{ SDK::ChampionId::Azir, 12 },
        ChampionPolicy{ SDK::ChampionId::Bard, 14 },
        ChampionPolicy{ SDK::ChampionId::Blitzcrank, 6 },
        ChampionPolicy{ SDK::ChampionId::Brand, 12 },
        ChampionPolicy{ SDK::ChampionId::Braum, 14 },
        ChampionPolicy{ SDK::ChampionId::Caitlyn, 4 },
        ChampionPolicy{ SDK::ChampionId::Camille, 1 },
        ChampionPolicy{ SDK::ChampionId::Cassiopeia, 0 },
        ChampionPolicy{ SDK::ChampionId::Chogath, 10 },
        ChampionPolicy{ SDK::ChampionId::Corki, 6 },
        ChampionPolicy{ SDK::ChampionId::Darius, 11 },
        ChampionPolicy{ SDK::ChampionId::Diana, 10 },
        ChampionPolicy{ SDK::ChampionId::Draven, 10 },
        ChampionPolicy{ SDK::ChampionId::DrMundo, 10 },
        ChampionPolicy{ SDK::ChampionId::Ekko, 12 },
        ChampionPolicy{ SDK::ChampionId::Elise, 15 },
        ChampionPolicy{ SDK::ChampionId::Evelynn, 11 },
        ChampionPolicy{ SDK::ChampionId::Ezreal, 4 },
        ChampionPolicy{ SDK::ChampionId::Fiddlesticks, 1 },
        ChampionPolicy{ SDK::ChampionId::Fiora, 15 },
        ChampionPolicy{ SDK::ChampionId::Fizz, 7 },
        ChampionPolicy{ SDK::ChampionId::Galio, 6 },
        ChampionPolicy{ SDK::ChampionId::Gangplank, 6 },
        ChampionPolicy{ SDK::ChampionId::Garen, 11 },
        ChampionPolicy{ SDK::ChampionId::Gnar, 14 },
        ChampionPolicy{ SDK::ChampionId::Gragas, 15 },
        ChampionPolicy{ SDK::ChampionId::Graves, 12 },
        ChampionPolicy{ SDK::ChampionId::Hecarim, 15 },
        ChampionPolicy{ SDK::ChampionId::Heimerdinger, 8 },
        ChampionPolicy{ SDK::ChampionId::Illaoi, 2 },
        ChampionPolicy{ SDK::ChampionId::Irelia, 15 },
        ChampionPolicy{ SDK::ChampionId::Ivern, 4 },
        ChampionPolicy{ SDK::ChampionId::Janna, 5 },
        ChampionPolicy{ SDK::ChampionId::JarvanIV, 15 },
        ChampionPolicy{ SDK::ChampionId::Jax, 15 },
        ChampionPolicy{ SDK::ChampionId::Jayce, 15 },
        ChampionPolicy{ SDK::ChampionId::Jhin, 0 },
        ChampionPolicy{ SDK::ChampionId::Jinx, 5 },
        ChampionPolicy{ SDK::ChampionId::Kaisa, 13 },
        ChampionPolicy{ SDK::ChampionId::Kalista, 12 },
        ChampionPolicy{ SDK::ChampionId::Karma, 14 },
        ChampionPolicy{ SDK::ChampionId::Karthus, 4 },
        ChampionPolicy{ SDK::ChampionId::Kassadin, 10 },
        ChampionPolicy{ SDK::ChampionId::Katarina, 6 },
        ChampionPolicy{ SDK::ChampionId::Kayle, 14 },
        ChampionPolicy{ SDK::ChampionId::Kayn, 13 },
        ChampionPolicy{ SDK::ChampionId::Kennen, 14 },
        ChampionPolicy{ SDK::ChampionId::KhaZix, 12 },
        ChampionPolicy{ SDK::ChampionId::Kindred, 11 },
        ChampionPolicy{ SDK::ChampionId::Kled, 4 },
        ChampionPolicy{ SDK::ChampionId::KogMaw, 2 },
        ChampionPolicy{ SDK::ChampionId::Leblanc, 10 },
        ChampionPolicy{ SDK::ChampionId::LeeSin, 14 },
        ChampionPolicy{ SDK::ChampionId::Leona, 11 },
        ChampionPolicy{ SDK::ChampionId::Lillia, 0 },
        ChampionPolicy{ SDK::ChampionId::Lissandra, 12 },
        ChampionPolicy{ SDK::ChampionId::Lucian, 4 },
        ChampionPolicy{ SDK::ChampionId::Lulu, 14 },
        ChampionPolicy{ SDK::ChampionId::Lux, 6 },
        ChampionPolicy{ SDK::ChampionId::Malphite, 14 },
        ChampionPolicy{ SDK::ChampionId::Malzahar, 4 },
        ChampionPolicy{ SDK::ChampionId::Maokai, 14 },
        ChampionPolicy{ SDK::ChampionId::MasterYi, 13 },
        ChampionPolicy{ SDK::ChampionId::MissFortune, 2 },
        ChampionPolicy{ SDK::ChampionId::MonkeyKing, 15 },
        ChampionPolicy{ SDK::ChampionId::Mordekaiser, 11 },
        ChampionPolicy{ SDK::ChampionId::Morgana, 12 },
        ChampionPolicy{ SDK::ChampionId::Nami, 6 },
        ChampionPolicy{ SDK::ChampionId::Nasus, 9 },
        ChampionPolicy{ SDK::ChampionId::Nautilus, 15 },
        ChampionPolicy{ SDK::ChampionId::Neeko, 2 },
        ChampionPolicy{ SDK::ChampionId::Nidalee, 15 },
        ChampionPolicy{ SDK::ChampionId::Nocturne, 10 },
        ChampionPolicy{ SDK::ChampionId::Nunu, 2 },
        ChampionPolicy{ SDK::ChampionId::Olaf, 14 },
        ChampionPolicy{ SDK::ChampionId::Orianna, 7 },
        ChampionPolicy{ SDK::ChampionId::Ornn, 0 },
        ChampionPolicy{ SDK::ChampionId::Pantheon, 6 },
        ChampionPolicy{ SDK::ChampionId::Poppy, 15 },
        ChampionPolicy{ SDK::ChampionId::Pyke, 6 },
        ChampionPolicy{ SDK::ChampionId::Qiyana, 6 },
        ChampionPolicy{ SDK::ChampionId::Quinn, 15 },
        ChampionPolicy{ SDK::ChampionId::Rakan, 14 },
        ChampionPolicy{ SDK::ChampionId::Rammus, 11 },
        ChampionPolicy{ SDK::ChampionId::RekSai, 15 },
        ChampionPolicy{ SDK::ChampionId::Renekton, 14 },
        ChampionPolicy{ SDK::ChampionId::Rengar, 3 },
        ChampionPolicy{ SDK::ChampionId::Riven, 13 },
        ChampionPolicy{ SDK::ChampionId::Rumble, 2 },
        ChampionPolicy{ SDK::ChampionId::Ryze, 8 },
        ChampionPolicy{ SDK::ChampionId::Samira, 14 },
        ChampionPolicy{ SDK::ChampionId::Sejuani, 7 },
        ChampionPolicy{ SDK::ChampionId::Senna, 4 },
        ChampionPolicy{ SDK::ChampionId::Sett, 9 },
        ChampionPolicy{ SDK::ChampionId::Shaco, 9 },
        ChampionPolicy{ SDK::ChampionId::Shen, 14 },
        ChampionPolicy{ SDK::ChampionId::Shyvana, 15 },
        ChampionPolicy{ SDK::ChampionId::Singed, 9 },
        ChampionPolicy{ SDK::ChampionId::Sion, 14 },
        ChampionPolicy{ SDK::ChampionId::Sivir, 12 },
        ChampionPolicy{ SDK::ChampionId::Skarner, 15 },
        ChampionPolicy{ SDK::ChampionId::Sona, 14 },
        ChampionPolicy{ SDK::ChampionId::Soraka, 15 },
        ChampionPolicy{ SDK::ChampionId::Swain, 15 },
        ChampionPolicy{ SDK::ChampionId::Sylas, 0 },
        ChampionPolicy{ SDK::ChampionId::Syndra, 11 },
        ChampionPolicy{ SDK::ChampionId::TahmKench, 6 },
        ChampionPolicy{ SDK::ChampionId::Taliyah, 0 },
        ChampionPolicy{ SDK::ChampionId::Talon, 13 },
        ChampionPolicy{ SDK::ChampionId::Taric, 8 },
        ChampionPolicy{ SDK::ChampionId::Teemo, 6 },
        ChampionPolicy{ SDK::ChampionId::Thresh, 1 },
        ChampionPolicy{ SDK::ChampionId::Tristana, 2 },
        ChampionPolicy{ SDK::ChampionId::Trundle, 15 },
        ChampionPolicy{ SDK::ChampionId::Tryndamere, 13 },
        ChampionPolicy{ SDK::ChampionId::TwistedFate, 6 },
        ChampionPolicy{ SDK::ChampionId::Twitch, 9 },
        ChampionPolicy{ SDK::ChampionId::Udyr, 15 },
        ChampionPolicy{ SDK::ChampionId::Urgot, 14 },
        ChampionPolicy{ SDK::ChampionId::Varus, 0 },
        ChampionPolicy{ SDK::ChampionId::Vayne, 9 },
        ChampionPolicy{ SDK::ChampionId::Veigar, 0 },
        ChampionPolicy{ SDK::ChampionId::Velkoz, 5 },
        ChampionPolicy{ SDK::ChampionId::Vi, 15 },
        ChampionPolicy{ SDK::ChampionId::Viktor, 14 },
        ChampionPolicy{ SDK::ChampionId::Vladimir, 10 },
        ChampionPolicy{ SDK::ChampionId::Volibear, 15 },
        ChampionPolicy{ SDK::ChampionId::Warwick, 15 },
        ChampionPolicy{ SDK::ChampionId::Xayah, 10 },
        ChampionPolicy{ SDK::ChampionId::Xerath, 0 },
        ChampionPolicy{ SDK::ChampionId::XinZhao, 15 },
        ChampionPolicy{ SDK::ChampionId::Yasuo, 14 },
        ChampionPolicy{ SDK::ChampionId::Yone, 4 },
        ChampionPolicy{ SDK::ChampionId::Yorick, 0 },
        ChampionPolicy{ SDK::ChampionId::Yuumi, 15 },
        ChampionPolicy{ SDK::ChampionId::Zac, 14 },
        ChampionPolicy{ SDK::ChampionId::Zed, 14 },
        ChampionPolicy{ SDK::ChampionId::Ziggs, 2 },
        ChampionPolicy{ SDK::ChampionId::Zilean, 14 },
        ChampionPolicy{ SDK::ChampionId::Zoe, 8 },
        ChampionPolicy{ SDK::ChampionId::Zyra, 2 },
    }};
};

} // namespace Plugins::KuroEvade::Database
