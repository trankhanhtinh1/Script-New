#pragma once

#include "../../../../../SDK/SDK.h"
#include <string>
#include <vector>

namespace Plugins::KuroAIO::Fiora {

struct EvadeSpellData {
    SDK::ChampionId Champion = SDK::ChampionId::Unknown;
    std::vector<std::string> SpellNames;
    SpellSlot Slot = SpellSlot::Unknown;
    std::string MissileName;
};

struct OtherEvadeSpellData {
    SDK::ChampionId Champion = SDK::ChampionId::Unknown;
    SpellSlot Slot = SpellSlot::Unknown;
};

struct TargetedNoneEvadeSpellData {
    SDK::ChampionId Champion = SDK::ChampionId::Unknown;
    bool UseSpellSlot = false;
    SpellSlot Slot = SpellSlot::Unknown;
    std::vector<std::string> SpellNames;
    bool IsDash = false;
    float DistanceDash = 200.0f;
    std::string MissileName;
};

inline std::vector<EvadeSpellData> EvadeTargetSpells = {
    { SDK::ChampionId::Ahri, { "ahrifoxfiremissiletwo" }, SpellSlot::W, "ahrifoxfiremissiletwo" },
    { SDK::ChampionId::Ahri, { "ahritumblemissile" }, SpellSlot::R, "ahritumblemissile" },
    { SDK::ChampionId::Anivia, { "frostbite" }, SpellSlot::E, "frostbite" },
    { SDK::ChampionId::Annie, { "disintegrate" }, SpellSlot::Q, "disintegrate" },
    { SDK::ChampionId::Brand, { "brandconflagrationmissile" }, SpellSlot::E, "brandconflagrationmissile" },
    { SDK::ChampionId::Brand, { "brandwildfire", "brandwildfiremissile" }, SpellSlot::R, "brandwildfire" },
    { SDK::ChampionId::Caitlyn, { "caitlynaceintheholemissile" }, SpellSlot::R, "caitlynaceintheholemissile" },
    { SDK::ChampionId::Cassiopeia, { "cassiopeiatwinfang" }, SpellSlot::E, "cassiopeiatwinfang" },
    { SDK::ChampionId::Elise, { "elisehumanq" }, SpellSlot::Q, "elisehumanq" },
    { SDK::ChampionId::Ezreal, { "ezrealarcaneshiftmissile" }, SpellSlot::E, "ezrealarcaneshiftmissile" },
    { SDK::ChampionId::Fiddlesticks, { "fiddlesticksdarkwind", "fiddlesticksdarkwindmissile" }, SpellSlot::E, "fiddlesticksdarkwind" },
    { SDK::ChampionId::Gangplank, { "parley" }, SpellSlot::Q, "parley" },
    { SDK::ChampionId::Janna, { "sowthewind" }, SpellSlot::W, "sowthewind" },
    { SDK::ChampionId::Kassadin, { "nulllance" }, SpellSlot::Q, "nulllance" },
    { SDK::ChampionId::Katarina, { "katarinaq", "katarinaqmis" }, SpellSlot::Q, "katarinaq" },
    { SDK::ChampionId::Kayle, { "judicatorreckoning" }, SpellSlot::Q, "judicatorreckoning" },
    { SDK::ChampionId::Leblanc, { "leblancchaosorb", "leblancchaosorbm" }, SpellSlot::Q, "leblancchaosorb" },
    { SDK::ChampionId::Lulu, { "LuluW" }, SpellSlot::W, "LuluW" },
    { SDK::ChampionId::Malphite, { "seismicshard" }, SpellSlot::Q, "seismicshard" },
    { SDK::ChampionId::MissFortune, { "missfortunericochetshot", "missFortunershotextra" }, SpellSlot::Q, "missfortunericochetshot" },
    { SDK::ChampionId::Nami, { "namiwenemy", "namiwmissileenemy" }, SpellSlot::W, "namiwenemy" },
    { SDK::ChampionId::Nunu, { "iceblast" }, SpellSlot::E, "iceblast" },
    { SDK::ChampionId::Pantheon, { "pantheonw" }, SpellSlot::W, "pantheonw" },
    { SDK::ChampionId::Ryze, { "spellflux", "spellfluxmissile" }, SpellSlot::E, "spellflux" },
    { SDK::ChampionId::Shaco, { "twoshivpoison" }, SpellSlot::E, "twoshivpoison" },
    { SDK::ChampionId::Shen, { "shenvorpalstar" }, SpellSlot::Q, "shenvorpalstar" },
    { SDK::ChampionId::Sona, { "sonaqmissile" }, SpellSlot::Q, "sonaqmissile" },
    { SDK::ChampionId::Swain, { "swaintorment" }, SpellSlot::E, "swaintorment" },
    { SDK::ChampionId::Syndra, { "syndrar" }, SpellSlot::R, "syndrar" },
    { SDK::ChampionId::Taric, { "dazzle" }, SpellSlot::E, "dazzle" },
    { SDK::ChampionId::Teemo, { "blindingdart" }, SpellSlot::Q, "blindingdart" },
    { SDK::ChampionId::Tristana, { "detonatingshot" }, SpellSlot::E, "detonatingshot" },
    { SDK::ChampionId::Tristana, { "tristanar" }, SpellSlot::R, "tristanar" },
    { SDK::ChampionId::TwistedFate, { "bluecardattack" }, SpellSlot::W, "bluecardattack" },
    { SDK::ChampionId::TwistedFate, { "goldcardattack" }, SpellSlot::W, "goldcardattack" },
    { SDK::ChampionId::TwistedFate, { "redcardattack" }, SpellSlot::W, "redcardattack" },
    { SDK::ChampionId::Urgot, { "urgotheatseekinghomemissile" }, SpellSlot::Q, "urgotheatseekinghomemissile" },
    { SDK::ChampionId::Vayne, { "vaynecondemnmissile" }, SpellSlot::E, "vaynecondemnmissile" },
    { SDK::ChampionId::Veigar, { "veigarprimordialburst" }, SpellSlot::R, "veigarprimordialburst" },
    { SDK::ChampionId::Viktor, { "viktorpowertransfer" }, SpellSlot::Q, "viktorpowertransfer" },
    { SDK::ChampionId::Vladimir, { "vladimirtidesofbloodnuke" }, SpellSlot::E, "vladimirtidesofbloodnuke" }
};

inline std::vector<OtherEvadeSpellData> OtherEvadeSpells = {
    { SDK::ChampionId::Azir, SpellSlot::R },
    { SDK::ChampionId::Fizz, SpellSlot::R },
    { SDK::ChampionId::Jax, SpellSlot::W },
    { SDK::ChampionId::Jax, SpellSlot::E },
    { SDK::ChampionId::Riven, SpellSlot::Q },
    { SDK::ChampionId::Riven, SpellSlot::W },
    { SDK::ChampionId::Diana, SpellSlot::E },
    { SDK::ChampionId::Kalista, SpellSlot::E },
    { SDK::ChampionId::Karma, SpellSlot::W },
    { SDK::ChampionId::Karthus, SpellSlot::R },
    { SDK::ChampionId::Kennen, SpellSlot::W },
    { SDK::ChampionId::Leblanc, SpellSlot::E },
    { SDK::ChampionId::Lulu, SpellSlot::W },
    { SDK::ChampionId::Lulu, SpellSlot::R },
    { SDK::ChampionId::Maokai, SpellSlot::R },
    { SDK::ChampionId::Morgana, SpellSlot::R },
    { SDK::ChampionId::Nautilus, SpellSlot::Unknown },
    { SDK::ChampionId::Nautilus, SpellSlot::R },
    { SDK::ChampionId::Neeko, SpellSlot::R },
    { SDK::ChampionId::Nocturne, SpellSlot::E },
    { SDK::ChampionId::Nocturne, SpellSlot::R },
    { SDK::ChampionId::Qiyana, SpellSlot::R },
    { SDK::ChampionId::Rammus, SpellSlot::Q },
    { SDK::ChampionId::Rengar, SpellSlot::Q },
    { SDK::ChampionId::RekSai, SpellSlot::W },
    { SDK::ChampionId::Tryndamere, SpellSlot::E },
    { SDK::ChampionId::Sett, SpellSlot::E },
    { SDK::ChampionId::Sett, SpellSlot::R },
    { SDK::ChampionId::Lissandra, SpellSlot::W },
    { SDK::ChampionId::Camille, SpellSlot::R },
    { SDK::ChampionId::Vladimir, SpellSlot::R },
    { SDK::ChampionId::Zed, SpellSlot::R },
    { SDK::ChampionId::Zoe, SpellSlot::E },
    { SDK::ChampionId::Tristana, SpellSlot::E },
    { SDK::ChampionId::Tristana, SpellSlot::R },
    { SDK::ChampionId::Udyr, SpellSlot::E },
    { SDK::ChampionId::Yorick, SpellSlot::Q },
    { SDK::ChampionId::Yasuo, SpellSlot::Q },
    { SDK::ChampionId::Yone, SpellSlot::Q },
    { SDK::ChampionId::Yone, SpellSlot::R },
    { SDK::ChampionId::Sylas, SpellSlot::E }
};

inline std::vector<TargetedNoneEvadeSpellData> TargetedNoneEvadeSpells = {
    { SDK::ChampionId::Alistar, true, SpellSlot::W, {}, false, 200.0f },
    { SDK::ChampionId::Blitzcrank, false, SpellSlot::E, { "PowerFistAttack" }, false, 200.0f },
    { SDK::ChampionId::Brand, true, SpellSlot::E, {}, false, 200.0f },
    { SDK::ChampionId::Chogath, true, SpellSlot::R, {}, false, 200.0f },
    { SDK::ChampionId::Darius, false, SpellSlot::W, { "DariusNoxianTacticsONHAttack" }, false, 200.0f },
    { SDK::ChampionId::Darius, true, SpellSlot::R, {}, false, 200.0f },
    { SDK::ChampionId::Ekko, false, SpellSlot::E, { "EkkoEAttack" }, false, 200.0f },
    { SDK::ChampionId::Elise, false, SpellSlot::Q, { "EliseSpiderQCast" }, false, 200.0f },
    { SDK::ChampionId::Evelynn, true, SpellSlot::E, {}, false, 200.0f },
    { SDK::ChampionId::Fiddlesticks, true, SpellSlot::Q, {}, false, 200.0f },
    { SDK::ChampionId::Fizz, true, SpellSlot::Q, {}, false, 200.0f },
    { SDK::ChampionId::Garen, false, SpellSlot::Q, { "GarenQAttack" }, false, 200.0f },
    { SDK::ChampionId::Garen, true, SpellSlot::R, {}, false, 200.0f },
    { SDK::ChampionId::Hecarim, false, SpellSlot::E, { "HecarimRampAttack" }, false, 200.0f },
    { SDK::ChampionId::Irelia, true, SpellSlot::Q, {}, true, 200.0f },
    { SDK::ChampionId::JarvanIV, true, SpellSlot::R, {}, false, 200.0f },
    { SDK::ChampionId::Sett, true, SpellSlot::R, {}, false, 200.0f },
    { SDK::ChampionId::Jax, true, SpellSlot::Q, {}, true, 200.0f },
    { SDK::ChampionId::Jax, false, SpellSlot::R, { "JaxRelentlessAttack" }, false, 200.0f },
    { SDK::ChampionId::Jayce, false, SpellSlot::Q, { "JayceToTheSkies" }, true, 400.0f },
    { SDK::ChampionId::Jayce, false, SpellSlot::E, { "JayceThunderingBlow" }, false, 200.0f },
    { SDK::ChampionId::KhaZix, true, SpellSlot::Q, {}, false, 200.0f },
    { SDK::ChampionId::LeeSin, true, SpellSlot::R, {}, false, 200.0f },
    { SDK::ChampionId::Leona, false, SpellSlot::Q, { "LeonaShieldOfDaybreakAttack" }, false, 200.0f },
    { SDK::ChampionId::Lissandra, true, SpellSlot::R, {}, false, 200.0f },
    { SDK::ChampionId::Lucian, true, SpellSlot::Q, {}, false, 200.0f },
    { SDK::ChampionId::Malzahar, true, SpellSlot::E, {}, false, 200.0f },
    { SDK::ChampionId::Malzahar, true, SpellSlot::R, {}, false, 200.0f },
    { SDK::ChampionId::Maokai, true, SpellSlot::W, {}, true, 200.0f },
    { SDK::ChampionId::Mordekaiser, true, SpellSlot::R, {}, false, 200.0f },
    { SDK::ChampionId::Nasus, false, SpellSlot::Q, { "NasusQAttack" }, false, 200.0f },
    { SDK::ChampionId::Nasus, true, SpellSlot::W, {}, false, 200.0f },
    { SDK::ChampionId::MonkeyKing, false, SpellSlot::Q, { "MonkeyKingQAttack" }, false, 200.0f },
    { SDK::ChampionId::Nidalee, false, SpellSlot::Q, { "NidaleeTakedownAttack", "Nidalee_CougarTakedownAttack" }, false, 200.0f },
    { SDK::ChampionId::Olaf, true, SpellSlot::E, {}, false, 200.0f },
    { SDK::ChampionId::Pantheon, true, SpellSlot::W, {}, false, 200.0f },
    { SDK::ChampionId::Poppy, true, SpellSlot::E, {}, false, 200.0f },
    { SDK::ChampionId::Poppy, true, SpellSlot::R, {}, false, 200.0f },
    { SDK::ChampionId::Quinn, true, SpellSlot::E, {}, false, 200.0f },
    { SDK::ChampionId::Rammus, true, SpellSlot::E, {}, false, 200.0f },
    { SDK::ChampionId::RekSai, true, SpellSlot::E, {}, false, 200.0f },
    { SDK::ChampionId::Renekton, false, SpellSlot::W, { "RenektonExecute", "RenektonSuperExecute" }, false, 200.0f },
    { SDK::ChampionId::Ryze, true, SpellSlot::W, {}, false, 200.0f },
    { SDK::ChampionId::Singed, true, SpellSlot::E, {}, false, 200.0f },
    { SDK::ChampionId::Skarner, true, SpellSlot::R, {}, false, 200.0f },
    { SDK::ChampionId::TahmKench, true, SpellSlot::W, {}, false, 200.0f },
    { SDK::ChampionId::Talon, true, SpellSlot::E, {}, false, 200.0f },
    { SDK::ChampionId::Talon, false, SpellSlot::Q, { "TalonNoxianDiplomacyAttack" }, false, 200.0f },
    { SDK::ChampionId::Trundle, true, SpellSlot::R, {}, false, 200.0f },
    { SDK::ChampionId::Udyr, false, SpellSlot::E, { "UdyrBearAttack", "UdyrBearAttackUlt" }, false, 200.0f },
    { SDK::ChampionId::Vi, true, SpellSlot::R, {}, true, 200.0f },
    { SDK::ChampionId::Shen, true, SpellSlot::E, {}, true, 200.0f },
    { SDK::ChampionId::Viktor, false, SpellSlot::Q, { "ViktorQBuff" }, false, 200.0f },
    { SDK::ChampionId::Vladimir, true, SpellSlot::Q, {}, false, 200.0f },
    { SDK::ChampionId::Volibear, true, SpellSlot::W, {}, false, 200.0f },
    { SDK::ChampionId::Volibear, false, SpellSlot::Q, { "VolibearQAttack" }, false, 200.0f },
    { SDK::ChampionId::Warwick, true, SpellSlot::Q, {}, false, 200.0f },
    { SDK::ChampionId::Warwick, true, SpellSlot::R, {}, false, 200.0f },
    { SDK::ChampionId::XinZhao, false, SpellSlot::Q, { "XinZhaoThrust3" }, false, 200.0f },
    { SDK::ChampionId::Yorick, true, SpellSlot::E, {}, false, 200.0f },
    { SDK::ChampionId::Zilean, true, SpellSlot::E, {}, false, 200.0f },
    { SDK::ChampionId::Briar, true, SpellSlot::Q, {}, false, 200.0f },
    { SDK::ChampionId::KSante, true, SpellSlot::R, {}, false, 200.0f }
};

} // namespace Plugins::KuroAIO::Fiora
