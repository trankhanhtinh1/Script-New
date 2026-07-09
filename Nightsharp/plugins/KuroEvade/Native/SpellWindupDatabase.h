#pragma once
#include "WindupSpellData.h"
#include <vector>

namespace Plugins::KuroEvade::ZD {

class SpellWindupDatabase {
public:
    static std::vector<WindupSpellData> Spells;
    static void Initialize() {
        // Aatrox
        { WindupSpellData s; s.charName="Aatrox"; s.name="Blade of Torment"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="AatroxE"; Spells.push_back(s); }
        // Ahri
        { WindupSpellData s; s.charName="Ahri"; s.name="Orb of Deception"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AhriOrbofDeception"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ahri"; s.name="Charm"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="AhriSeduce"; Spells.push_back(s); }
        // Akali
        { WindupSpellData s; s.charName="Akali"; s.name="AkaliMota"; s.spellKey=WindupSpellSlot::Q; s.spellName="AkaliMota"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Akali"; s.name="AkaliSmokeBomb"; s.spellKey=WindupSpellSlot::W; s.spellName="AkaliSmokeBomb"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Akali"; s.name="AkaliShadowSwipe"; s.spellKey=WindupSpellSlot::E; s.spellName="AkaliShadowSwipe"; Spells.push_back(s); }
        // Alistar
        { WindupSpellData s; s.charName="Alistar"; s.name="Pulverize"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="Pulverize"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Alistar"; s.name="TriumphantRoar"; s.spellKey=WindupSpellSlot::E; s.spellName="TriumphantRoar"; Spells.push_back(s); }
        // Azir
        { WindupSpellData s; s.charName="Azir"; s.name="AzirQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AzirQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Azir"; s.name="AzirW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="AzirW"; Spells.push_back(s); }
        // Amumu
        { WindupSpellData s; s.charName="Amumu"; s.name="Tantrum"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="Tantrum"; Spells.push_back(s); }
        // Anivia
        { WindupSpellData s; s.charName="Anivia"; s.name="Flash Frost"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="FlashFrostSpell"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Anivia"; s.name="Frostbite"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="Frostbite"; Spells.push_back(s); }
        // Annie
        { WindupSpellData s; s.charName="Annie"; s.name="InfernalGuardian"; s.spellKey=WindupSpellSlot::R; s.spellName="InfernalGuardian"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Annie"; s.name="Disintegrate"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="Disintegrate"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Annie"; s.name="Incinerate"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="Incinerate"; Spells.push_back(s); }
        // Ashe
        { WindupSpellData s; s.charName="Ashe"; s.name="Volley"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="Volley"; Spells.push_back(s); }
        // Bard
        { WindupSpellData s; s.charName="Bard"; s.name="BardQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="BardQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Bard"; s.name="BardW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="BardW"; Spells.push_back(s); }
        // Blitzcrank
        { WindupSpellData s; s.charName="Blitzcrank"; s.name="Rocket Grab"; s.spellDelay=685; s.spellKey=WindupSpellSlot::Q; s.spellName="RocketGrabMissile"; Spells.push_back(s); }
        // Brand
        { WindupSpellData s; s.charName="Brand"; s.name="BrandConflagration"; s.spellKey=WindupSpellSlot::E; s.spellName="BrandConflagration"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Brand"; s.name="BrandBlaze"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="BrandBlaze"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Brand"; s.name="Pillar of Flame"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="BrandFissure"; Spells.push_back(s); }
        // Braum
        { WindupSpellData s; s.charName="Braum"; s.name="BraumQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="BraumQ"; Spells.push_back(s); }
        // Caitlyn
        { WindupSpellData s; s.charName="Caitlyn"; s.name="Piltover Peacemaker"; s.spellDelay=625; s.spellKey=WindupSpellSlot::Q; s.spellName="CaitlynPiltoverPeacemaker"; Spells.push_back(s); }
        // Cassiopeia
        { WindupSpellData s; s.charName="Cassiopeia"; s.name="Twin Fang"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="CassiopeiaTwinFang"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Cassiopeia"; s.name="CassiopeiaMiasma"; s.spellKey=WindupSpellSlot::W; s.spellName="CassiopeiaMiasma"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Cassiopeia"; s.name="Noxious Blast"; s.spellKey=WindupSpellSlot::Q; s.spellName="CassiopeiaNoxiousBlast"; Spells.push_back(s); }
        // Chogath
        { WindupSpellData s; s.charName="Chogath"; s.name="FeralScream"; s.spellDelay=500; s.spellKey=WindupSpellSlot::W; s.spellName="FeralScream"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Chogath"; s.name="Rupture"; s.spellDelay=500; s.spellKey=WindupSpellSlot::Q; s.spellName="Rupture"; Spells.push_back(s); }
        // Corki
        { WindupSpellData s; s.charName="Corki"; s.name="Missile Barrage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="MissileBarrage"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Corki"; s.name="Phosphorus Bomb"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="PhosphorusBomb"; Spells.push_back(s); }
        // Darius
        { WindupSpellData s; s.charName="Darius"; s.name="Decimate"; s.spellDelay=230; s.spellKey=WindupSpellSlot::Q; s.spellName="DariusCleave"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Darius"; s.name="Apprehend"; s.spellDelay=320; s.spellKey=WindupSpellSlot::E; s.spellName="DariusAxeGrabCone"; Spells.push_back(s); }
        // Diana
        { WindupSpellData s; s.charName="Diana"; s.name="DianaVortex"; s.spellKey=WindupSpellSlot::E; s.spellName="DianaVortex"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Diana"; s.name="DianaArc"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="DianaArc"; Spells.push_back(s); }
        // DrMundo
        { WindupSpellData s; s.charName="DrMundo"; s.name="Infected Cleaver"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="InfectedCleaverMissile"; Spells.push_back(s); }
        // Draven
        { WindupSpellData s; s.charName="Draven"; s.name="Stand Aside"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="DravenDoubleShot"; Spells.push_back(s); }
        // Ekko
        { WindupSpellData s; s.charName="Ekko"; s.name="EkkoQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="EkkoQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ekko"; s.name="EkkoW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="EkkoW"; Spells.push_back(s); }
        // Elise
        { WindupSpellData s; s.charName="Elise"; s.name="Volatile Spiderling"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="EliseHumanW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Elise"; s.name="Venomous Bite"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="EliseSpiderQCast"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Elise"; s.name="Cocoon"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="EliseHumanE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Elise"; s.name="Neurotoxin"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="EliseHumanQ"; Spells.push_back(s); }
        // Evelynn
        { WindupSpellData s; s.charName="Evelynn"; s.name="Ravage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="EvelynnQ"; Spells.push_back(s); }
        // Ezreal
        { WindupSpellData s; s.charName="Ezreal"; s.name="Essence Flux"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="EzrealEssenceFlux"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ezreal"; s.name="Mystic Shot"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="EzrealMysticShot"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ezreal"; s.name="Trueshot Barrage"; s.spellDelay=1000; s.spellKey=WindupSpellSlot::R; s.spellName="EzrealTrueshotBarrage"; Spells.push_back(s); }
        // FiddleSticks
        { WindupSpellData s; s.charName="FiddleSticks"; s.name="Crowstorm"; s.spellKey=WindupSpellSlot::R; s.spellName="Crowstorm"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="FiddleSticks"; s.name="Terrify"; s.spellKey=WindupSpellSlot::Q; s.spellName="Terrify"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="FiddleSticks"; s.name="Dark Wind"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="FiddlesticksDarkWind"; Spells.push_back(s); }
        // Galio
        { WindupSpellData s; s.charName="Galio"; s.name="GalioResoluteSmite"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="GalioResoluteSmite"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Galio"; s.name="GalioRighteousGust"; s.spellKey=WindupSpellSlot::E; s.spellName="GalioRighteousGust"; Spells.push_back(s); }
        // Gangplank
        { WindupSpellData s; s.charName="Gangplank"; s.name="CannonBarrage"; s.spellKey=WindupSpellSlot::R; s.spellName="CannonBarrage"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gangplank"; s.name="Parley"; s.spellKey=WindupSpellSlot::Q; s.spellName="Parley"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gangplank"; s.name="RaiseMorale"; s.spellKey=WindupSpellSlot::E; s.spellName="RaiseMorale"; Spells.push_back(s); }
        // Gnar
        { WindupSpellData s; s.charName="Gnar"; s.name="GnarQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="GnarQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gnar"; s.name="GnarW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="gnarbigw"; Spells.push_back(s); }
        // Gragas
        { WindupSpellData s; s.charName="Gragas"; s.name="Barrel Roll"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="GragasBarrelRoll"; Spells.push_back(s); }
        // Graves
        { WindupSpellData s; s.charName="Graves"; s.name="Smoke Screen"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="GravesSmokeGrenade"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Graves"; s.name="Buckshot"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="GravesClusterShot"; Spells.push_back(s); }
        // Heimerdinger
        { WindupSpellData s; s.charName="Heimerdinger"; s.name="HeimerdingerE"; s.spellKey=WindupSpellSlot::E; s.spellName="HeimerdingerE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Heimerdinger"; s.name="HeimerdingerW"; s.spellKey=WindupSpellSlot::W; s.spellName="HeimerdingerW"; Spells.push_back(s); }
        // Janna
        { WindupSpellData s; s.charName="Janna"; s.name="SowTheWind"; s.spellKey=WindupSpellSlot::W; s.spellName="SowTheWind"; Spells.push_back(s); }
        // Jayce
        { WindupSpellData s; s.charName="Jayce"; s.name="JayceShockBlast"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="jayceshockblast"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jayce"; s.name="JayceQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="JayceToTheSkies"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jayce"; s.name="Thundering Blow"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="JayceThunderingBlow"; Spells.push_back(s); }
        // Jinx
        { WindupSpellData s; s.charName="Jinx"; s.name="Zap"; s.spellDelay=600; s.spellKey=WindupSpellSlot::W; s.spellName="JinxWMissile"; Spells.push_back(s); }
        // Kalista
        { WindupSpellData s; s.charName="Kalista"; s.name="KalistaMysticShot"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KalistaMysticShot"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kalista"; s.name="KalistaW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KalistaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kalista"; s.name="KalistaE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KalistaE"; Spells.push_back(s); }
        // Karma
        { WindupSpellData s; s.charName="Karma"; s.name="KarmaQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KarmaQ"; Spells.push_back(s); }
        // Karthus
        { WindupSpellData s; s.charName="Karthus"; s.name="Lay Waste"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="karthuslaywastea3"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Karthus"; s.name="WallOfPain"; s.spellKey=WindupSpellSlot::W; s.spellName="WallOfPain"; Spells.push_back(s); }
        // Kassadin
        { WindupSpellData s; s.charName="Kassadin"; s.name="Force Pulse"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ForcePulse"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kassadin"; s.name="Null Sphere"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="NullLance"; Spells.push_back(s); }
        // Katarina
        { WindupSpellData s; s.charName="Katarina"; s.name="KatarinaQ"; s.spellKey=WindupSpellSlot::Q; s.spellName="KatarinaQ"; Spells.push_back(s); }
        // Kayle
        { WindupSpellData s; s.charName="Kayle"; s.name="Reckoning"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="JudicatorReckoning"; Spells.push_back(s); }
        // Kennen
        { WindupSpellData s; s.charName="Kennen"; s.name="Thundering Shuriken"; s.spellDelay=180; s.spellKey=WindupSpellSlot::Q; s.spellName="KennenShurikenHurlMissile1"; Spells.push_back(s); }
        // Khazix
        { WindupSpellData s; s.charName="Khazix"; s.name="KhazixQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KhazixQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Khazix"; s.name="KhazixW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KhazixW"; Spells.push_back(s); }
        // KogMaw
        { WindupSpellData s; s.charName="KogMaw"; s.name="Living Artillery"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KogMawLivingArtillery"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="KogMaw"; s.name="Caustic Spittle"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KogMawCausticSpittle"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="KogMaw"; s.name="Void Ooze"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KogMawVoidOozeMissile"; Spells.push_back(s); }
        // Leblanc
        { WindupSpellData s; s.charName="Leblanc"; s.name="Sigil of Silence"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LeblancChaosOrb"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Leblanc"; s.name="Ethereal Chains"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="LeblancSoulShackle"; Spells.push_back(s); }
        // LeeSin
        { WindupSpellData s; s.charName="LeeSin"; s.name="Sonic Wave"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="BlindMonkQOne"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="LeeSin"; s.name="BlindMonkEOne"; s.spellKey=WindupSpellSlot::E; s.spellName="BlindMonkEOne"; Spells.push_back(s); }
        // Lissandra
        { WindupSpellData s; s.charName="Lissandra"; s.name="LissandraQ"; s.spellKey=WindupSpellSlot::Q; s.spellName="LissandraQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lissandra"; s.name="LissandraW"; s.spellKey=WindupSpellSlot::W; s.spellName="LissandraW"; Spells.push_back(s); }
        // Lucian
        { WindupSpellData s; s.charName="Lucian"; s.name="LucianQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LucianQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lucian"; s.name="LucianW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="LucianW"; Spells.push_back(s); }
        // Lulu
        { WindupSpellData s; s.charName="Lulu"; s.name="LuluQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LuluQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lulu"; s.name="LuluE"; s.spellKey=WindupSpellSlot::E; s.spellName="LuluE"; Spells.push_back(s); }
        // Lux
        { WindupSpellData s; s.charName="Lux"; s.name="Light Binding"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LuxLightBinding"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lux"; s.name="LuxLightStrikeKugel"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="LuxLightStrikeKugel"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lux"; s.name="LuxPrismaticWave"; s.spellKey=WindupSpellSlot::W; s.spellName="LuxPrismaticWave"; Spells.push_back(s); }
        // Malphite
        { WindupSpellData s; s.charName="Malphite"; s.name="Seismic Shard"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SeismicShard"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Malphite"; s.name="Ground Slam"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="Landslide"; Spells.push_back(s); }
        // Malzahar
        { WindupSpellData s; s.charName="Malzahar"; s.name="AlZaharCalloftheVoid"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AlZaharCalloftheVoid"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Malzahar"; s.name="Null Zone"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="AlZaharNullZone"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Malzahar"; s.name="Malefic Visions"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="AlZaharMaleficVisions"; Spells.push_back(s); }
        // Maokai
        { WindupSpellData s; s.charName="Maokai"; s.name="MaokaiTrunkLine"; s.spellKey=WindupSpellSlot::Q; s.spellName="MaokaiTrunkLine"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Maokai"; s.name="MaokaiSapling2"; s.spellKey=WindupSpellSlot::E; s.spellName="MaokaiSapling2"; Spells.push_back(s); }
        // MissFortune
        { WindupSpellData s; s.charName="MissFortune"; s.name="MissFortuneRicochetShot"; s.spellKey=WindupSpellSlot::Q; s.spellName="MissFortuneRicochetShot"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="MissFortune"; s.name="MissFortuneScattershot"; s.spellKey=WindupSpellSlot::E; s.spellName="MissFortuneScattershot"; Spells.push_back(s); }
        // Mordekaiser
        { WindupSpellData s; s.charName="Mordekaiser"; s.name="MordekaiserSyphonOfDestruction"; s.spellKey=WindupSpellSlot::E; s.spellName="MordekaiserSyphonOfDestruction"; Spells.push_back(s); }
        // Morgana
        { WindupSpellData s; s.charName="Morgana"; s.name="TormentedSoil"; s.spellKey=WindupSpellSlot::W; s.spellName="TormentedSoil"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Morgana"; s.name="Dark Binding"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="DarkBindingMissile"; Spells.push_back(s); }
        // Nami
        { WindupSpellData s; s.charName="Nami"; s.name="NamiQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="NamiQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nami"; s.name="TidecallersBlessing"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="NamiE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nami"; s.name="Ebb and Flow"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="NamiW"; Spells.push_back(s); }
        // Nasus
        { WindupSpellData s; s.charName="Nasus"; s.name="Wither"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="NasusW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nasus"; s.name="NasusE"; s.spellKey=WindupSpellSlot::E; s.spellName="NasusE"; Spells.push_back(s); }
        // Nautilus
        { WindupSpellData s; s.charName="Nautilus"; s.name="NautilusSplashZone"; s.spellKey=WindupSpellSlot::E; s.spellName="NautilusSplashZone"; Spells.push_back(s); }
        // Nidalee
        { WindupSpellData s; s.charName="Nidalee"; s.name="PrimalSurge"; s.spellKey=WindupSpellSlot::E; s.spellName="PrimalSurge"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nidalee"; s.name="Javelin Toss"; s.spellDelay=125; s.spellKey=WindupSpellSlot::Q; s.spellName="JavelinToss"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nidalee"; s.name="Swipe"; s.spellKey=WindupSpellSlot::E; s.spellName="Swipe"; Spells.push_back(s); }
        // Nocturne
        { WindupSpellData s; s.charName="Nocturne"; s.name="Unspeakable Horror"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="NocturneUnspeakableHorror"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nocturne"; s.name="NocturneDuskbringer"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="NocturneDuskbringer"; Spells.push_back(s); }
        // Nunu
        { WindupSpellData s; s.charName="Nunu"; s.name="IceBlast"; s.spellDelay=400; s.spellKey=WindupSpellSlot::E; s.spellName="IceBlast"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nunu"; s.name="Consume"; s.spellDelay=400; s.spellKey=WindupSpellSlot::Q; s.spellName="Consume"; Spells.push_back(s); }
        // Olaf
        { WindupSpellData s; s.charName="Olaf"; s.name="Reckless Swing"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="OlafRecklessStrike"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Olaf"; s.name="Undertow"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="OlafAxeThrow"; Spells.push_back(s); }
        // Pantheon
        { WindupSpellData s; s.charName="Pantheon"; s.name="Pantheon_Throw"; s.spellKey=WindupSpellSlot::Q; s.spellName="Pantheon_Throw"; Spells.push_back(s); }
        // Quinn
        { WindupSpellData s; s.charName="Quinn"; s.name="QuinnQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="QuinnQ"; Spells.push_back(s); }
        // Renekton
        { WindupSpellData s; s.charName="Renekton"; s.name="RenektonCleave"; s.spellKey=WindupSpellSlot::Q; s.spellName="RenektonCleave"; Spells.push_back(s); }
        // Rengar
        { WindupSpellData s; s.charName="Rengar"; s.name="Bola Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RengarE"; Spells.push_back(s); }
        // Riven
        { WindupSpellData s; s.charName="Riven"; s.name="Ki Burst"; s.spellDelay=267; s.spellKey=WindupSpellSlot::W; s.spellName="RivenMartyr"; Spells.push_back(s); }
        // Rumble
        { WindupSpellData s; s.charName="Rumble"; s.name="RumbleGrenade"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RumbleGrenade"; Spells.push_back(s); }
        // Ryze
        { WindupSpellData s; s.charName="Ryze"; s.name="Overload"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="Overload"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ryze"; s.name="Rune Prison"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="RunePrison"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ryze"; s.name="Spell Flux"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SpellFlux"; Spells.push_back(s); }
        // Shaco
        { WindupSpellData s; s.charName="Shaco"; s.name="JackInTheBox"; s.spellKey=WindupSpellSlot::W; s.spellName="JackInTheBox"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Shaco"; s.name="TwoShivPoison"; s.spellKey=WindupSpellSlot::E; s.spellName="TwoShivPoison"; Spells.push_back(s); }
        // Shen
        { WindupSpellData s; s.charName="Shen"; s.name="ShenVorpalStar"; s.spellKey=WindupSpellSlot::Q; s.spellName="ShenVorpalStar"; Spells.push_back(s); }
        // Shyvana
        { WindupSpellData s; s.charName="Shyvana"; s.name="ShyvanaFireball"; s.spellKey=WindupSpellSlot::E; s.spellName="ShyvanaFireball"; Spells.push_back(s); }
        // Sion
        { WindupSpellData s; s.charName="Sion"; s.name="CrypticGaze"; s.spellKey=WindupSpellSlot::Q; s.spellName="CrypticGaze"; Spells.push_back(s); }
        // Sivir
        { WindupSpellData s; s.charName="Sivir"; s.name="Boomerang Blade"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SivirQ"; Spells.push_back(s); }
        // Skarner
        { WindupSpellData s; s.charName="Skarner"; s.name="Fracture"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SkarnerFracture"; Spells.push_back(s); }
        // Soraka
        { WindupSpellData s; s.charName="Soraka"; s.name="Infuse"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="Infuse"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Soraka"; s.name="Starcall"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="Starcall"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Soraka"; s.name="AstralBlessing"; s.spellKey=WindupSpellSlot::W; s.spellName="AstralBlessing"; Spells.push_back(s); }
        // Swain
        { WindupSpellData s; s.charName="Swain"; s.name="Nevermove"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SwainShadowGrasp"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Swain"; s.name="Torment"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SwainTorment"; Spells.push_back(s); }
        // Syndra
        { WindupSpellData s; s.charName="Syndra"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SyndraE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Syndra"; s.name="SyndraQ"; s.spellDelay=200; s.spellKey=WindupSpellSlot::Q; s.spellName="SyndraQ"; Spells.push_back(s); }
        // Talon
        { WindupSpellData s; s.charName="Talon"; s.name="Rake"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="TalonRake"; Spells.push_back(s); }
        // Taric
        { WindupSpellData s; s.charName="Taric"; s.name="Shatter"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="Shatter"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Taric"; s.name="Radiance"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="TaricHammerSmash"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Taric"; s.name="Imbue"; s.spellKey=WindupSpellSlot::Q; s.spellName="Imbue"; Spells.push_back(s); }
        // Teemo
        { WindupSpellData s; s.charName="Teemo"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="BantamTrap"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Teemo"; s.name="BlindingDart"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="BlindingDart"; Spells.push_back(s); }
        // Thresh
        { WindupSpellData s; s.charName="Thresh"; s.name="ThreshQ"; s.spellDelay=500; s.spellKey=WindupSpellSlot::Q; s.spellName="ThreshQ"; Spells.push_back(s); }
        // Tristana
        { WindupSpellData s; s.charName="Tristana"; s.name="Explosive Shot"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="DetonatingShot"; Spells.push_back(s); }
        // Trundle
        { WindupSpellData s; s.charName="Trundle"; s.name="TrundleCircle"; s.spellKey=WindupSpellSlot::E; s.spellName="TrundleCircle"; Spells.push_back(s); }
        // Tryndamere
        { WindupSpellData s; s.charName="Tryndamere"; s.name="MockingShout"; s.spellKey=WindupSpellSlot::W; s.spellName="MockingShout"; Spells.push_back(s); }
        // TwistedFate
        { WindupSpellData s; s.charName="TwistedFate"; s.name="Loaded Dice"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="WildCards"; Spells.push_back(s); }
        // Twitch
        { WindupSpellData s; s.charName="Twitch"; s.name="Expunge"; s.spellKey=WindupSpellSlot::E; s.spellName="Expunge"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Twitch"; s.name="TwitchVenomCask"; s.spellKey=WindupSpellSlot::W; s.spellName="TwitchVenomCask"; Spells.push_back(s); }
        // Urgot
        { WindupSpellData s; s.charName="Urgot"; s.name="UrgotPlasmaGrenade"; s.spellKey=WindupSpellSlot::E; s.spellName="UrgotPlasmaGrenade"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Urgot"; s.name="UrgotHeatseekingMissile"; s.spellKey=WindupSpellSlot::Q; s.spellName="UrgotHeatseekingMissile"; Spells.push_back(s); }
        // Varus
        { WindupSpellData s; s.charName="Varus"; s.name="Varus E"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="VarusE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Varus"; s.name="Varus Q Missile"; s.spellDelay=0; s.spellKey=WindupSpellSlot::Q; s.spellName="VarusQ"; Spells.push_back(s); }
        // Vayne
        { WindupSpellData s; s.charName="Vayne"; s.name="VayneCondemn"; s.spellKey=WindupSpellSlot::E; s.spellName="VayneCondemn"; Spells.push_back(s); }
        // Veigar
        { WindupSpellData s; s.charName="Veigar"; s.name="VeigarDarkMatter"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="VeigarDarkMatter"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Veigar"; s.name="Baleful Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="VeigarBalefulStrike"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Veigar"; s.name="VeigarEventHorizon"; s.spellKey=WindupSpellSlot::E; s.spellName="VeigarEventHorizon"; Spells.push_back(s); }
        // Velkoz
        { WindupSpellData s; s.charName="Velkoz"; s.name="VelkozW"; s.spellKey=WindupSpellSlot::W; s.spellName="VelkozW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Velkoz"; s.name="VelkozQ"; s.spellKey=WindupSpellSlot::Q; s.spellName="VelkozQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Velkoz"; s.name="VelkozE"; s.spellKey=WindupSpellSlot::E; s.spellName="VelkozE"; Spells.push_back(s); }
        // Viktor
        { WindupSpellData s; s.charName="Viktor"; s.name="ViktorPowerTransfer"; s.spellKey=WindupSpellSlot::Q; s.spellName="ViktorPowerTransfer"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Viktor"; s.name="ViktorGravitonField"; s.spellKey=WindupSpellSlot::W; s.spellName="ViktorGravitonField"; Spells.push_back(s); }
        // Vladimir
        { WindupSpellData s; s.charName="Vladimir"; s.name="VladimirTidesofBlood"; s.spellKey=WindupSpellSlot::E; s.spellName="VladimirTidesofBlood"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vladimir"; s.name="VladimirHemoplague"; s.spellDelay=389; s.spellKey=WindupSpellSlot::R; s.spellName="VladimirHemoplague"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vladimir"; s.name="VladimirTransfusion"; s.spellKey=WindupSpellSlot::Q; s.spellName="VladimirTransfusion"; Spells.push_back(s); }
        // Warwick
        { WindupSpellData s; s.charName="Warwick"; s.name="Hungering Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="HungeringStrike"; Spells.push_back(s); }
        // Xerath
        { WindupSpellData s; s.charName="Xerath"; s.name="XerathMageSpear"; s.spellKey=WindupSpellSlot::E; s.spellName="XerathMageSpear"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Xerath"; s.name="XerathArcaneBarrage2"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="XerathArcaneBarrage2"; Spells.push_back(s); }
        // Yasuo
        { WindupSpellData s; s.charName="Yasuo"; s.name="Steel Tempest3"; s.spellDelay=175; s.spellKey=WindupSpellSlot::Q; s.spellName="YasuoQW"; Spells.push_back(s); }
        // Yorick
        { WindupSpellData s; s.charName="Yorick"; s.name="YorickDecayed"; s.spellKey=WindupSpellSlot::W; s.spellName="YorickDecayed"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yorick"; s.name="YorickRavenous"; s.spellKey=WindupSpellSlot::E; s.spellName="YorickRavenous"; Spells.push_back(s); }
        // Zac
        { WindupSpellData s; s.charName="Zac"; s.name="ZacQ"; s.spellKey=WindupSpellSlot::Q; s.spellName="ZacQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zac"; s.name="ZacW"; s.spellKey=WindupSpellSlot::W; s.spellName="ZacW"; Spells.push_back(s); }
        // Zed
        { WindupSpellData s; s.charName="Zed"; s.name="ZedShuriken"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ZedShuriken"; Spells.push_back(s); }
        // Ziggs
        { WindupSpellData s; s.charName="Ziggs"; s.name="ZiggsW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ZiggsW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ziggs"; s.name="ZiggsE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ZiggsE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ziggs"; s.name="ZiggsQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ZiggsQ"; Spells.push_back(s); }
        // Zilean
        { WindupSpellData s; s.charName="Zilean"; s.name="Time Bomb"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="TimeBomb"; Spells.push_back(s); }
        // Zyra
        { WindupSpellData s; s.charName="Zyra"; s.name="Grasping Roots"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ZyraGraspingRoots"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zyra"; s.name="Rampant Growth"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ZyraSeed"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zyra"; s.name="Deadly Bloom"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ZyraQFissure"; Spells.push_back(s); }

        // ── MISSING CHAMPIONS (added for latest patch coverage) ──

        // Ambessa
        { WindupSpellData s; s.charName="Ambessa"; s.name="Sundering Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AmbessaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ambessa"; s.name="Sundering Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="AmbessaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ambessa"; s.name="Public Execution"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="AmbessaR"; Spells.push_back(s); }

        // AurelionSol
        { WindupSpellData s; s.charName="AurelionSol"; s.name="AurelionSolQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AurelionSolQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="AurelionSol"; s.name="AurelionSolR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="AurelionSolR"; Spells.push_back(s); }

        // Belveth
        { WindupSpellData s; s.charName="Belveth"; s.name="BelvethQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="BelvethQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Belveth"; s.name="BelvethW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="BelvethW"; Spells.push_back(s); }

        // Briar
        { WindupSpellData s; s.charName="Briar"; s.name="BriarQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="BriarQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Briar"; s.name="BriarE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="BriarE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Briar"; s.name="BriarR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="BriarR"; Spells.push_back(s); }

        // Camille
        { WindupSpellData s; s.charName="Camille"; s.name="CamilleQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="CamilleQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Camille"; s.name="CamilleW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="CamilleW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Camille"; s.name="CamilleE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="CamilleE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Camille"; s.name="CamilleR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="CamilleR"; Spells.push_back(s); }

        // Fizz
        { WindupSpellData s; s.charName="Fizz"; s.name="FizzR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="FizzR"; Spells.push_back(s); }

        // Garen
        { WindupSpellData s; s.charName="Garen"; s.name="GarenQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="GarenQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Garen"; s.name="GarenE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="GarenE"; Spells.push_back(s); }

        // Gwen
        { WindupSpellData s; s.charName="Gwen"; s.name="GwenQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="GwenQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gwen"; s.name="GwenE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="GwenE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gwen"; s.name="GwenR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="GwenR"; Spells.push_back(s); }

        // Hecarim
        { WindupSpellData s; s.charName="Hecarim"; s.name="HecarimQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="HecarimQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Hecarim"; s.name="HecarimE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="HecarimE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Hecarim"; s.name="HecarimR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="HecarimR"; Spells.push_back(s); }

        // Hwei
        { WindupSpellData s; s.charName="Hwei"; s.name="HweiQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="HweiQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Hwei"; s.name="HweiW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="HweiW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Hwei"; s.name="HweiE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="HweiE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Hwei"; s.name="HweiR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="HweiR"; Spells.push_back(s); }

        // Illaoi
        { WindupSpellData s; s.charName="Illaoi"; s.name="IllaoiQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="IllaoiQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Illaoi"; s.name="IllaoiE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="IllaoiE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Illaoi"; s.name="IllaoiR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="IllaoiR"; Spells.push_back(s); }

        // Irelia
        { WindupSpellData s; s.charName="Irelia"; s.name="IreliaQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="IreliaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Irelia"; s.name="IreliaE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="IreliaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Irelia"; s.name="IreliaR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="IreliaR"; Spells.push_back(s); }

        // Ivern
        { WindupSpellData s; s.charName="Ivern"; s.name="IvernQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="IvernQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ivern"; s.name="IvernE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="IvernE"; Spells.push_back(s); }

        // JarvanIV
        { WindupSpellData s; s.charName="JarvanIV"; s.name="JarvanIVQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="JarvanIVQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="JarvanIV"; s.name="JarvanIVE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="JarvanIVE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="JarvanIV"; s.name="JarvanIVR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="JarvanIVR"; Spells.push_back(s); }

        // Jhin
        { WindupSpellData s; s.charName="Jhin"; s.name="JhinQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="JhinQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jhin"; s.name="JhinW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="JhinW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jhin"; s.name="JhinE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="JhinE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jhin"; s.name="JhinR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="JhinR"; Spells.push_back(s); }

        // Kaisa
        { WindupSpellData s; s.charName="Kaisa"; s.name="KaisaQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KaisaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kaisa"; s.name="KaisaW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KaisaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kaisa"; s.name="KaisaR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KaisaR"; Spells.push_back(s); }

        // Kayn
        { WindupSpellData s; s.charName="Kayn"; s.name="KaynQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KaynQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kayn"; s.name="KaynW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KaynW"; Spells.push_back(s); }

        // Kindred
        { WindupSpellData s; s.charName="Kindred"; s.name="KindredQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KindredQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kindred"; s.name="KindredW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KindredW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kindred"; s.name="KindredE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KindredE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kindred"; s.name="KindredR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KindredR"; Spells.push_back(s); }

        // Kled
        { WindupSpellData s; s.charName="Kled"; s.name="KledQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KledQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kled"; s.name="KledE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KledE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kled"; s.name="KledR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KledR"; Spells.push_back(s); }

        // KSante
        { WindupSpellData s; s.charName="KSante"; s.name="KSanteQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KSanteQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="KSante"; s.name="KSanteW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KSanteW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="KSante"; s.name="KSanteE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KSanteE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="KSante"; s.name="KSanteR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KSanteR"; Spells.push_back(s); }

        // Leona
        { WindupSpellData s; s.charName="Leona"; s.name="LeonaQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LeonaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Leona"; s.name="LeonaE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="LeonaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Leona"; s.name="LeonaR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="LeonaR"; Spells.push_back(s); }

        // Lillia
        { WindupSpellData s; s.charName="Lillia"; s.name="LilliaQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LilliaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lillia"; s.name="LilliaW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="LilliaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lillia"; s.name="LilliaE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="LilliaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lillia"; s.name="LilliaR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="LilliaR"; Spells.push_back(s); }

        // Locke
        { WindupSpellData s; s.charName="Locke"; s.name="LockeQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LockeQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Locke"; s.name="LockeW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="LockeW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Locke"; s.name="LockeE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="LockeE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Locke"; s.name="LockeR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="LockeR"; Spells.push_back(s); }

        // MasterYi
        { WindupSpellData s; s.charName="MasterYi"; s.name="AlphaStrike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AlphaStrike"; Spells.push_back(s); }

        // Mel
        { WindupSpellData s; s.charName="Mel"; s.name="MelQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="MelQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Mel"; s.name="MelW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="MelW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Mel"; s.name="MelE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="MelE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Mel"; s.name="MelR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="MelR"; Spells.push_back(s); }

        // Milio
        { WindupSpellData s; s.charName="Milio"; s.name="MilioQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="MilioQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Milio"; s.name="MilioE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="MilioE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Milio"; s.name="MilioR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="MilioR"; Spells.push_back(s); }

        // MonkeyKing (Wukong)
        { WindupSpellData s; s.charName="MonkeyKing"; s.name="MonkeyKingQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="MonkeyKingQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="MonkeyKing"; s.name="MonkeyKingE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="MonkeyKingE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="MonkeyKing"; s.name="MonkeyKingR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="MonkeyKingR"; Spells.push_back(s); }

        // Naafiri
        { WindupSpellData s; s.charName="Naafiri"; s.name="NaafiriQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="NaafiriQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Naafiri"; s.name="NaafiriW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="NaafiriW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Naafiri"; s.name="NaafiriE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="NaafiriE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Naafiri"; s.name="NaafiriR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="NaafiriR"; Spells.push_back(s); }

        // Neeko
        { WindupSpellData s; s.charName="Neeko"; s.name="NeekoQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="NeekoQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Neeko"; s.name="NeekoE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="NeekoE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Neeko"; s.name="NeekoR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="NeekoR"; Spells.push_back(s); }

        // Nilah
        { WindupSpellData s; s.charName="Nilah"; s.name="NilahQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="NilahQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nilah"; s.name="NilahE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="NilahE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nilah"; s.name="NilahR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="NilahR"; Spells.push_back(s); }

        // Ornn
        { WindupSpellData s; s.charName="Ornn"; s.name="OrnnQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="OrnnQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ornn"; s.name="OrnnW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="OrnnW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ornn"; s.name="OrnnE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="OrnnE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ornn"; s.name="OrnnR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="OrnnR"; Spells.push_back(s); }

        // Orianna
        { WindupSpellData s; s.charName="Orianna"; s.name="OriannaQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="OriannaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Orianna"; s.name="OriannaW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="OriannaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Orianna"; s.name="OriannaE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="OriannaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Orianna"; s.name="OriannaR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="OriannaR"; Spells.push_back(s); }

        // Poppy
        { WindupSpellData s; s.charName="Poppy"; s.name="PoppyQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="PoppyQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Poppy"; s.name="PoppyE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="PoppyE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Poppy"; s.name="PoppyR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="PoppyR"; Spells.push_back(s); }

        // Pyke
        { WindupSpellData s; s.charName="Pyke"; s.name="PykeQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="PykeQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Pyke"; s.name="PykeE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="PykeE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Pyke"; s.name="PykeR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="PykeR"; Spells.push_back(s); }

        // Qiyana
        { WindupSpellData s; s.charName="Qiyana"; s.name="QiyanaQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="QiyanaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Qiyana"; s.name="QiyanaW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="QiyanaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Qiyana"; s.name="QiyanaE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="QiyanaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Qiyana"; s.name="QiyanaR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="QiyanaR"; Spells.push_back(s); }

        // Rakan
        { WindupSpellData s; s.charName="Rakan"; s.name="RakanQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RakanQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rakan"; s.name="RakanW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="RakanW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rakan"; s.name="RakanE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RakanE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rakan"; s.name="RakanR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="RakanR"; Spells.push_back(s); }

        // Rammus
        { WindupSpellData s; s.charName="Rammus"; s.name="RammusQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RammusQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rammus"; s.name="RammusE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RammusE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rammus"; s.name="RammusR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="RammusR"; Spells.push_back(s); }

        // RekSai
        { WindupSpellData s; s.charName="RekSai"; s.name="RekSaiQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RekSaiQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="RekSai"; s.name="RekSaiE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RekSaiE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="RekSai"; s.name="RekSaiR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="RekSaiR"; Spells.push_back(s); }

        // Rell
        { WindupSpellData s; s.charName="Rell"; s.name="RellQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RellQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rell"; s.name="RellW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="RellW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rell"; s.name="RellE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RellE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rell"; s.name="RellR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="RellR"; Spells.push_back(s); }

        // Renata
        { WindupSpellData s; s.charName="Renata"; s.name="RenataQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RenataQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Renata"; s.name="RenataE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RenataE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Renata"; s.name="RenataR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="RenataR"; Spells.push_back(s); }

        // Samira
        { WindupSpellData s; s.charName="Samira"; s.name="SamiraQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SamiraQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Samira"; s.name="SamiraW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SamiraW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Samira"; s.name="SamiraE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SamiraE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Samira"; s.name="SamiraR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SamiraR"; Spells.push_back(s); }

        // Sejuani
        { WindupSpellData s; s.charName="Sejuani"; s.name="SejuaniQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SejuaniQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sejuani"; s.name="SejuaniW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SejuaniW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sejuani"; s.name="SejuaniE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SejuaniE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sejuani"; s.name="SejuaniR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SejuaniR"; Spells.push_back(s); }

        // Seraphine
        { WindupSpellData s; s.charName="Seraphine"; s.name="SeraphineQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SeraphineQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Seraphine"; s.name="SeraphineW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SeraphineW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Seraphine"; s.name="SeraphineE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SeraphineE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Seraphine"; s.name="SeraphineR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SeraphineR"; Spells.push_back(s); }

        // Sett
        { WindupSpellData s; s.charName="Sett"; s.name="SettQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SettQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sett"; s.name="SettW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SettW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sett"; s.name="SettE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SettE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sett"; s.name="SettR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SettR"; Spells.push_back(s); }

        // Singed
        { WindupSpellData s; s.charName="Singed"; s.name="SingedQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SingedQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Singed"; s.name="SingedE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SingedE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Singed"; s.name="SingedR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SingedR"; Spells.push_back(s); }

        // Smolder
        { WindupSpellData s; s.charName="Smolder"; s.name="SmolderQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SmolderQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Smolder"; s.name="SmolderW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SmolderW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Smolder"; s.name="SmolderE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SmolderE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Smolder"; s.name="SmolderR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SmolderR"; Spells.push_back(s); }

        // Sona
        { WindupSpellData s; s.charName="Sona"; s.name="SonaQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SonaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sona"; s.name="SonaW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SonaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sona"; s.name="SonaE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SonaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sona"; s.name="SonaR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SonaR"; Spells.push_back(s); }

        // Sylas
        { WindupSpellData s; s.charName="Sylas"; s.name="SylasQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SylasQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sylas"; s.name="SylasW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SylasW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sylas"; s.name="SylasE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SylasE"; Spells.push_back(s); }

        // TahmKench
        { WindupSpellData s; s.charName="TahmKench"; s.name="TahmKenchQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="TahmKenchQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="TahmKench"; s.name="TahmKenchW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="TahmKenchW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="TahmKench"; s.name="TahmKenchE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="TahmKenchE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="TahmKench"; s.name="TahmKenchR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="TahmKenchR"; Spells.push_back(s); }

        // Taliyah
        { WindupSpellData s; s.charName="Taliyah"; s.name="TaliyahQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="TaliyahQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Taliyah"; s.name="TaliyahW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="TaliyahW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Taliyah"; s.name="TaliyahE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="TaliyahE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Taliyah"; s.name="TaliyahR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="TaliyahR"; Spells.push_back(s); }

        // Udyr
        { WindupSpellData s; s.charName="Udyr"; s.name="UdyrQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="UdyrQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Udyr"; s.name="UdyrW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="UdyrW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Udyr"; s.name="UdyrE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="UdyrE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Udyr"; s.name="UdyrR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="UdyrR"; Spells.push_back(s); }

        // Vi
        { WindupSpellData s; s.charName="Vi"; s.name="ViQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ViQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vi"; s.name="ViE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ViE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vi"; s.name="ViR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ViR"; Spells.push_back(s); }

        // Viego
        { WindupSpellData s; s.charName="Viego"; s.name="ViegoQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ViegoQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Viego"; s.name="ViegoW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ViegoW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Viego"; s.name="ViegoE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ViegoE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Viego"; s.name="ViegoR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ViegoR"; Spells.push_back(s); }

        // Volibear
        { WindupSpellData s; s.charName="Volibear"; s.name="VolibearQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="VolibearQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Volibear"; s.name="VolibearW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="VolibearW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Volibear"; s.name="VolibearE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="VolibearE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Volibear"; s.name="VolibearR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="VolibearR"; Spells.push_back(s); }

        // Xayah
        { WindupSpellData s; s.charName="Xayah"; s.name="XayahQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="XayahQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Xayah"; s.name="XayahW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="XayahW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Xayah"; s.name="XayahE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="XayahE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Xayah"; s.name="XayahR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="XayahR"; Spells.push_back(s); }

        // XinZhao
        { WindupSpellData s; s.charName="XinZhao"; s.name="XinZhaoQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="XinZhaoQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="XinZhao"; s.name="XinZhaoW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="XinZhaoW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="XinZhao"; s.name="XinZhaoE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="XinZhaoE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="XinZhao"; s.name="XinZhaoR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="XinZhaoR"; Spells.push_back(s); }

        // Yone
        { WindupSpellData s; s.charName="Yone"; s.name="YoneQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="YoneQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yone"; s.name="YoneW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="YoneW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yone"; s.name="YoneE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="YoneE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yone"; s.name="YoneR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="YoneR"; Spells.push_back(s); }

        // Yuumi
        { WindupSpellData s; s.charName="Yuumi"; s.name="YuumiQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="YuumiQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yuumi"; s.name="YuumiR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="YuumiR"; Spells.push_back(s); }

        // Yunara
        { WindupSpellData s; s.charName="Yunara"; s.name="YunaraQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="YunaraQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yunara"; s.name="YunaraW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="YunaraW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yunara"; s.name="YunaraE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="YunaraE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yunara"; s.name="YunaraR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="YunaraR"; Spells.push_back(s); }

        // Zaahen
        { WindupSpellData s; s.charName="Zaahen"; s.name="ZaahenQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ZaahenQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zaahen"; s.name="ZaahenW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ZaahenW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zaahen"; s.name="ZaahenE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ZaahenE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zaahen"; s.name="ZaahenR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ZaahenR"; Spells.push_back(s); }

        // Zeri
        { WindupSpellData s; s.charName="Zeri"; s.name="ZeriQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ZeriQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zeri"; s.name="ZeriW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ZeriW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zeri"; s.name="ZeriE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ZeriE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zeri"; s.name="ZeriR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ZeriR"; Spells.push_back(s); }

        // Zoe
        { WindupSpellData s; s.charName="Zoe"; s.name="ZoeQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ZoeQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zoe"; s.name="ZoeE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ZoeE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zoe"; s.name="ZoeR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ZoeR"; Spells.push_back(s); }

        // Aurora
        { WindupSpellData s; s.charName="Aurora"; s.name="AuroraQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AuroraQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Aurora"; s.name="AuroraW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="AuroraW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Aurora"; s.name="AuroraE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="AuroraE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Aurora"; s.name="AuroraR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="AuroraR"; Spells.push_back(s); }

        // Jax
        { WindupSpellData s; s.charName="Jax"; s.name="JaxQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="JaxQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jax"; s.name="JaxE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="JaxE"; Spells.push_back(s); }
    }
};

inline std::vector<WindupSpellData> SpellWindupDatabase::Spells;

} // namespace Plugins::KuroEvade::ZD

// ============================================================================
// KuroEvade Compatibility Wrapper
// ============================================================================
namespace Plugins::KuroEvade {

struct SpellWindupEntry {
    std::string ChampionName;
    std::string SpellName;
    SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
};

struct SpellWindupDatabase final {
    using Entry = SpellWindupEntry;

    static const std::vector<Entry>& Spells() {
        static std::vector<Entry> combinedWindups;
        static bool initialized = false;
        if (!initialized) {
            if (::Plugins::KuroEvade::ZD::SpellWindupDatabase::Spells.empty()) {
                ::Plugins::KuroEvade::ZD::SpellWindupDatabase::Initialize();
            }

            for (const auto& zd : ::Plugins::KuroEvade::ZD::SpellWindupDatabase::Spells) {
                Entry e = {};
                e.ChampionName = zd.charName;
                e.SpellName = zd.spellName;

                switch (zd.spellKey) {
                    case ::Plugins::KuroEvade::ZD::WindupSpellSlot::Q: e.Slot = SDK::SpellSlot::Q; break;
                    case ::Plugins::KuroEvade::ZD::WindupSpellSlot::W: e.Slot = SDK::SpellSlot::W; break;
                    case ::Plugins::KuroEvade::ZD::WindupSpellSlot::E: e.Slot = SDK::SpellSlot::E; break;
                    case ::Plugins::KuroEvade::ZD::WindupSpellSlot::R: e.Slot = SDK::SpellSlot::R; break;
                }

                combinedWindups.push_back(e);
            }
            initialized = true;
        }
        return combinedWindups;
    }

    static const Entry* Find(const char* championName,
                             const char* spellName,
                             SDK::SpellSlot slot) {
        for (const auto& entry : Spells()) {
            if (championName && championName[0] &&
                _stricmp(entry.ChampionName.c_str(), championName) != 0) {
                continue;
            }
            if (slot != SDK::SpellSlot::Unknown && entry.Slot != slot) {
                continue;
            }
            if (spellName && spellName[0] && !entry.SpellName.empty() &&
                _stricmp(entry.SpellName.c_str(), spellName) != 0) {
                continue;
            }
            return &entry;
        }
        return nullptr;
    }
};

} // namespace Plugins::KuroEvade
