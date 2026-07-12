#pragma once

#include "../../../../../SDK/SDK.h"
#include <string>
#include <vector>

namespace Plugins::KuroAIO::Fiora {

struct EvadeSpellData {
    std::string CharacterName;
    std::vector<std::string> SpellNames;
    SpellSlot Slot = SpellSlot::Unknown;
    std::string MissileName;
};

struct OtherEvadeSpellData {
    std::string CharacterName;
    SpellSlot Slot = SpellSlot::Unknown;
};

struct TargetedNoneEvadeSpellData {
    std::string CharacterName;
    bool UseSpellSlot = false;
    SpellSlot Slot = SpellSlot::Unknown;
    std::vector<std::string> SpellNames;
    bool IsDash = false;
    float DistanceDash = 200.0f;
    std::string MissileName;
};

inline std::vector<EvadeSpellData> EvadeTargetSpells = {
    { "Ahri", { "ahrifoxfiremissiletwo" }, SpellSlot::W, "ahrifoxfiremissiletwo" },
    { "Ahri", { "ahritumblemissile" }, SpellSlot::R, "ahritumblemissile" },
    { "Anivia", { "frostbite" }, SpellSlot::E, "frostbite" },
    { "Annie", { "disintegrate" }, SpellSlot::Q, "disintegrate" },
    { "Brand", { "brandconflagrationmissile" }, SpellSlot::E, "brandconflagrationmissile" },
    { "Brand", { "brandwildfire", "brandwildfiremissile" }, SpellSlot::R, "brandwildfire" },
    { "Caitlyn", { "caitlynaceintheholemissile" }, SpellSlot::R, "caitlynaceintheholemissile" },
    { "Cassiopeia", { "cassiopeiatwinfang" }, SpellSlot::E, "cassiopeiatwinfang" },
    { "Elise", { "elisehumanq" }, SpellSlot::Q, "elisehumanq" },
    { "Ezreal", { "ezrealarcaneshiftmissile" }, SpellSlot::E, "ezrealarcaneshiftmissile" },
    { "FiddleSticks", { "fiddlesticksdarkwind", "fiddlesticksdarkwindmissile" }, SpellSlot::E, "fiddlesticksdarkwind" },
    { "Gangplank", { "parley" }, SpellSlot::Q, "parley" },
    { "Janna", { "sowthewind" }, SpellSlot::W, "sowthewind" },
    { "Kassadin", { "nulllance" }, SpellSlot::Q, "nulllance" },
    { "Katarina", { "katarinaq", "katarinaqmis" }, SpellSlot::Q, "katarinaq" },
    { "Kayle", { "judicatorreckoning" }, SpellSlot::Q, "judicatorreckoning" },
    { "Leblanc", { "leblancchaosorb", "leblancchaosorbm" }, SpellSlot::Q, "leblancchaosorb" },
    { "Lulu", { "LuluW" }, SpellSlot::W, "LuluW" },
    { "Malphite", { "seismicshard" }, SpellSlot::Q, "seismicshard" },
    { "MissFortune", { "missfortunericochetshot", "missFortunershotextra" }, SpellSlot::Q, "missfortunericochetshot" },
    { "Nami", { "namiwenemy", "namiwmissileenemy" }, SpellSlot::W, "namiwenemy" },
    { "Nunu", { "iceblast" }, SpellSlot::E, "iceblast" },
    { "Pantheon", { "pantheonw" }, SpellSlot::W, "pantheonw" },
    { "Ryze", { "spellflux", "spellfluxmissile" }, SpellSlot::E, "spellflux" },
    { "Shaco", { "twoshivpoison" }, SpellSlot::E, "twoshivpoison" },
    { "Shen", { "shenvorpalstar" }, SpellSlot::Q, "shenvorpalstar" },
    { "Sona", { "sonaqmissile" }, SpellSlot::Q, "sonaqmissile" },
    { "Swain", { "swaintorment" }, SpellSlot::E, "swaintorment" },
    { "Syndra", { "syndrar" }, SpellSlot::R, "syndrar" },
    { "Taric", { "dazzle" }, SpellSlot::E, "dazzle" },
    { "Teemo", { "blindingdart" }, SpellSlot::Q, "blindingdart" },
    { "Tristana", { "detonatingshot" }, SpellSlot::E, "detonatingshot" },
    { "Tristana", { "tristanar" }, SpellSlot::R, "tristanar" },
    { "TwistedFate", { "bluecardattack" }, SpellSlot::W, "bluecardattack" },
    { "TwistedFate", { "goldcardattack" }, SpellSlot::W, "goldcardattack" },
    { "TwistedFate", { "redcardattack" }, SpellSlot::W, "redcardattack" },
    { "Urgot", { "urgotheatseekinghomemissile" }, SpellSlot::Q, "urgotheatseekinghomemissile" },
    { "Vayne", { "vaynecondemnmissile" }, SpellSlot::E, "vaynecondemnmissile" },
    { "Veigar", { "veigarprimordialburst" }, SpellSlot::R, "veigarprimordialburst" },
    { "Viktor", { "viktorpowertransfer" }, SpellSlot::Q, "viktorpowertransfer" },
    { "Vladimir", { "vladimirtidesofbloodnuke" }, SpellSlot::E, "vladimirtidesofbloodnuke" }
};

inline std::vector<OtherEvadeSpellData> OtherEvadeSpells = {
    { "Azir", SpellSlot::R },
    { "Fizz", SpellSlot::R },
    { "Jax", SpellSlot::W },
    { "Jax", SpellSlot::E },
    { "Riven", SpellSlot::Q },
    { "Riven", SpellSlot::W },
    { "Diana", SpellSlot::E },
    { "Kalista", SpellSlot::E },
    { "Karma", SpellSlot::W },
    { "Karthus", SpellSlot::R },
    { "Kennen", SpellSlot::W },
    { "Leblanc", SpellSlot::E },
    { "Lulu", SpellSlot::W },
    { "Lulu", SpellSlot::R },
    { "Maokai", SpellSlot::R },
    { "Morgana", SpellSlot::R },
    { "Nautilus", SpellSlot::Unknown },
    { "Nautilus", SpellSlot::R },
    { "Neeko", SpellSlot::R },
    { "Nocturne", SpellSlot::E },
    { "Nocturne", SpellSlot::R },
    { "Qiyana", SpellSlot::R },
    { "Rammus", SpellSlot::Q },
    { "Rengar", SpellSlot::Q },
    { "Reksai", SpellSlot::W },
    { "Tryndamere", SpellSlot::E },
    { "Sett", SpellSlot::E },
    { "Sett", SpellSlot::R },
    { "Lissandra", SpellSlot::W },
    { "Camille", SpellSlot::R },
    { "Vladimir", SpellSlot::R },
    { "Zed", SpellSlot::R },
    { "Zoe", SpellSlot::E },
    { "Tristana", SpellSlot::E },
    { "Tristana", SpellSlot::R },
    { "Udyr", SpellSlot::E },
    { "Yorick", SpellSlot::Q },
    { "Yasuo", SpellSlot::Q },
    { "Yone", SpellSlot::Q },
    { "Yone", SpellSlot::R },
    { "Sylas", SpellSlot::E }
};

inline std::vector<TargetedNoneEvadeSpellData> TargetedNoneEvadeSpells = {
    { "Alistar", true, SpellSlot::W, {}, false, 200.0f },
    { "Blitzcrank", false, SpellSlot::E, { "PowerFistAttack" }, false, 200.0f },
    { "Brand", true, SpellSlot::E, {}, false, 200.0f },
    { "Chogath", true, SpellSlot::R, {}, false, 200.0f },
    { "Darius", false, SpellSlot::W, { "DariusNoxianTacticsONHAttack" }, false, 200.0f },
    { "Darius", true, SpellSlot::R, {}, false, 200.0f },
    { "Ekko", false, SpellSlot::E, { "EkkoEAttack" }, false, 200.0f },
    { "Elise", false, SpellSlot::Q, { "EliseSpiderQCast" }, false, 200.0f },
    { "Evelynn", true, SpellSlot::E, {}, false, 200.0f },
    { "Fiddlesticks", true, SpellSlot::Q, {}, false, 200.0f },
    { "Fizz", true, SpellSlot::Q, {}, false, 200.0f },
    { "Garen", false, SpellSlot::Q, { "GarenQAttack" }, false, 200.0f },
    { "Garen", true, SpellSlot::R, {}, false, 200.0f },
    { "Hecarim", false, SpellSlot::E, { "HecarimRampAttack" }, false, 200.0f },
    { "Irelia", true, SpellSlot::Q, {}, true, 200.0f },
    { "Jarvan", true, SpellSlot::R, {}, false, 200.0f },
    { "Sett", true, SpellSlot::R, {}, false, 200.0f },
    { "Jax", true, SpellSlot::Q, {}, true, 200.0f },
    { "Jax", false, SpellSlot::R, { "JaxRelentlessAttack" }, false, 200.0f },
    { "Jayce", false, SpellSlot::Q, { "JayceToTheSkies" }, true, 400.0f },
    { "Jayce", false, SpellSlot::E, { "JayceThunderingBlow" }, false, 200.0f },
    { "Khazix", true, SpellSlot::Q, {}, false, 200.0f },
    { "Leesin", true, SpellSlot::R, {}, false, 200.0f },
    { "Leona", false, SpellSlot::Q, { "LeonaShieldOfDaybreakAttack" }, false, 200.0f },
    { "Lissandra", true, SpellSlot::R, {}, false, 200.0f },
    { "Lucian", true, SpellSlot::Q, {}, false, 200.0f },
    { "Malzahar", true, SpellSlot::E, {}, false, 200.0f },
    { "Malzahar", true, SpellSlot::R, {}, false, 200.0f },
    { "Maokai", true, SpellSlot::W, {}, true, 200.0f },
    { "Mordekaiser", true, SpellSlot::R, {}, false, 200.0f },
    { "Nasus", false, SpellSlot::Q, { "NasusQAttack" }, false, 200.0f },
    { "Nasus", true, SpellSlot::W, {}, false, 200.0f },
    { "MonkeyKing", false, SpellSlot::Q, { "MonkeyKingQAttack" }, false, 200.0f },
    { "Nidalee", false, SpellSlot::Q, { "NidaleeTakedownAttack", "Nidalee_CougarTakedownAttack" }, false, 200.0f },
    { "Olaf", true, SpellSlot::E, {}, false, 200.0f },
    { "Pantheon", true, SpellSlot::W, {}, false, 200.0f },
    { "Poppy", true, SpellSlot::E, {}, false, 200.0f },
    { "Poppy", true, SpellSlot::R, {}, false, 200.0f },
    { "Quinn", true, SpellSlot::E, {}, false, 200.0f },
    { "Rammus", true, SpellSlot::E, {}, false, 200.0f },
    { "RekSai", true, SpellSlot::E, {}, false, 200.0f },
    { "Renekton", false, SpellSlot::W, { "RenektonExecute", "RenektonSuperExecute" }, false, 200.0f },
    { "Ryze", true, SpellSlot::W, {}, false, 200.0f },
    { "Singed", true, SpellSlot::E, {}, false, 200.0f },
    { "Skarner", true, SpellSlot::R, {}, false, 200.0f },
    { "TahmKench", true, SpellSlot::W, {}, false, 200.0f },
    { "Talon", true, SpellSlot::E, {}, false, 200.0f },
    { "Talon", false, SpellSlot::Q, { "TalonNoxianDiplomacyAttack" }, false, 200.0f },
    { "Trundle", true, SpellSlot::R, {}, false, 200.0f },
    { "Udyr", false, SpellSlot::E, { "UdyrBearAttack", "UdyrBearAttackUlt" }, false, 200.0f },
    { "Vi", true, SpellSlot::R, {}, true, 200.0f },
    { "Shen", true, SpellSlot::E, {}, true, 200.0f },
    { "Viktor", false, SpellSlot::Q, { "ViktorQBuff" }, false, 200.0f },
    { "Vladimir", true, SpellSlot::Q, {}, false, 200.0f },
    { "Volibear", true, SpellSlot::W, {}, false, 200.0f },
    { "Volibear", false, SpellSlot::Q, { "VolibearQAttack" }, false, 200.0f },
    { "Warwick", true, SpellSlot::Q, {}, false, 200.0f },
    { "Warwick", true, SpellSlot::R, {}, false, 200.0f },
    { "XinZhao", false, SpellSlot::Q, { "XinZhaoThrust3" }, false, 200.0f },
    { "Yorick", true, SpellSlot::E, {}, false, 200.0f },
    { "Zilean", true, SpellSlot::E, {}, false, 200.0f },
    { "Briar", true, SpellSlot::Q, {}, false, 200.0f },
    { "Ksante", true, SpellSlot::R, {}, false, 200.0f }
};

} // namespace Plugins::KuroAIO::Fiora
