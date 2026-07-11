#pragma once
#include "WindupSpellData.h"
#include <vector>

namespace ZDEvade {
class SpellWindupDatabase {
public:
    static std::vector<WindupSpellData> Spells;

    static void Initialize() {
        if (!Spells.empty()) return;

        // Aatrox
        { WindupSpellData s; s.charName="Aatrox"; s.name="The Darkin Blade"; s.spellDelay=600; s.spellKey=WindupSpellSlot::Q; s.spellName="AatroxQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Aatrox"; s.name="Infernal Chains"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="AatroxW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Aatrox"; s.name="Umbral Dash"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="AatroxE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Aatrox"; s.name="World Ender"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="AatroxR"; Spells.push_back(s); }

        // Ahri
        { WindupSpellData s; s.charName="Ahri"; s.name="Orb of Deception"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AhriQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ahri"; s.name="Fox-Fire"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="AhriW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ahri"; s.name="Charm"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="AhriE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ahri"; s.name="Spirit Rush"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="AhriR"; Spells.push_back(s); }

        // Akali
        { WindupSpellData s; s.charName="Akali"; s.name="Five Point Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AkaliQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Akali"; s.name="Twilight Shroud"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="AkaliW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Akali"; s.name="Shuriken Flip"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="AkaliE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Akali"; s.name="Perfect Execution"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="AkaliR"; Spells.push_back(s); }

        // Akshan
        { WindupSpellData s; s.charName="Akshan"; s.name="Avengerang"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AkshanQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Akshan"; s.name="Going Rogue"; s.spellDelay=500; s.spellKey=WindupSpellSlot::W; s.spellName="AkshanW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Akshan"; s.name="Heroic Swing"; s.spellDelay=100; s.spellKey=WindupSpellSlot::E; s.spellName="AkshanE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Akshan"; s.name="Comeuppance"; s.spellDelay=125; s.spellKey=WindupSpellSlot::R; s.spellName="AkshanR"; Spells.push_back(s); }

        // Alistar
        { WindupSpellData s; s.charName="Alistar"; s.name="Pulverize"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="Pulverize"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Alistar"; s.name="Headbutt"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="Headbutt"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Alistar"; s.name="Trample"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="AlistarE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Alistar"; s.name="Unbreakable Will"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="FerociousHowl"; Spells.push_back(s); }

        // Ambessa
        { WindupSpellData s; s.charName="Ambessa"; s.name="Cunning Sweep / Sundering Slam"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AmbessaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ambessa"; s.name="Repudiation"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="AmbessaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ambessa"; s.name="Lacerate"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="AmbessaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ambessa"; s.name="Public Execution"; s.spellDelay=699; s.spellKey=WindupSpellSlot::R; s.spellName="AmbessaR"; Spells.push_back(s); }

        // Amumu
        { WindupSpellData s; s.charName="Amumu"; s.name="Bandage Toss"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="BandageToss"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Amumu"; s.name="Despair"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="AuraofDespair"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Amumu"; s.name="Tantrum"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="Tantrum"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Amumu"; s.name="Curse of the Sad Mummy"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="CurseoftheSadMummy"; Spells.push_back(s); }

        // Anivia
        { WindupSpellData s; s.charName="Anivia"; s.name="Flash Frost"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="FlashFrost"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Anivia"; s.name="Crystallize"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="Crystallize"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Anivia"; s.name="Frostbite"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="Frostbite"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Anivia"; s.name="Glacial Storm"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="GlacialStorm"; Spells.push_back(s); }

        // Annie
        { WindupSpellData s; s.charName="Annie"; s.name="Disintegrate"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AnnieQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Annie"; s.name="Incinerate"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="AnnieW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Annie"; s.name="Molten Shield"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="AnnieE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Annie"; s.name="Summon: Tibbers"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="AnnieR"; Spells.push_back(s); }

        // Aphelios
        { WindupSpellData s; s.charName="Aphelios"; s.name="Weapon Abilites"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ApheliosQ_ClientTooltipWrapper"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Aphelios"; s.name="Phase"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ApheliosW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Aphelios"; s.name="Weapon Queue System"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ApheliosE_ClientTooltipWrapper"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Aphelios"; s.name="Moonlight Vigil"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="ApheliosR"; Spells.push_back(s); }

        // Ashe
        { WindupSpellData s; s.charName="Ashe"; s.name="Ranger's Focus"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AsheQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ashe"; s.name="Volley"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="Volley"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ashe"; s.name="Hawkshot"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="AsheSpiritOfTheHawk"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ashe"; s.name="Enchanted Crystal Arrow"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="EnchantedCrystalArrow"; Spells.push_back(s); }

        // AurelionSol
        { WindupSpellData s; s.charName="AurelionSol"; s.name="Breath of Light"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AurelionSolQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="AurelionSol"; s.name="Astral Flight"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="AurelionSolW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="AurelionSol"; s.name="Singularity"; s.spellDelay=200; s.spellKey=WindupSpellSlot::E; s.spellName="AurelionSolE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="AurelionSol"; s.name="Falling Star / The Skies Descend"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="AurelionSolR"; Spells.push_back(s); }

        // Aurora
        { WindupSpellData s; s.charName="Aurora"; s.name="Twofold Hex"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AuroraQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Aurora"; s.name="Across the Veil"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="AuroraW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Aurora"; s.name="The Weirding"; s.spellDelay=349; s.spellKey=WindupSpellSlot::E; s.spellName="AuroraE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Aurora"; s.name="Between Worlds"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="AuroraR"; Spells.push_back(s); }

        // Azir
        { WindupSpellData s; s.charName="Azir"; s.name="Conquering Sands"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="AzirQWrapper"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Azir"; s.name="Arise!"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="AzirW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Azir"; s.name="Shifting Sands"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="AzirEWrapper"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Azir"; s.name="Emperor's Divide"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="AzirR"; Spells.push_back(s); }

        // Bard
        { WindupSpellData s; s.charName="Bard"; s.name="Cosmic Binding"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="BardQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Bard"; s.name="Caretaker's Shrine"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="BardW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Bard"; s.name="Magical Journey"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="BardE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Bard"; s.name="Tempered Fate"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="BardR"; Spells.push_back(s); }

        // Belveth
        { WindupSpellData s; s.charName="Belveth"; s.name="Void Surge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="BelvethQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Belveth"; s.name="Above and Below"; s.spellDelay=500; s.spellKey=WindupSpellSlot::W; s.spellName="BelvethW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Belveth"; s.name="Royal Maelstrom"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="BelvethE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Belveth"; s.name="Endless Banquet"; s.spellDelay=1000; s.spellKey=WindupSpellSlot::R; s.spellName="BelvethR"; Spells.push_back(s); }

        // Blitzcrank
        { WindupSpellData s; s.charName="Blitzcrank"; s.name="Rocket Grab"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RocketGrab"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Blitzcrank"; s.name="Overdrive"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="Overdrive"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Blitzcrank"; s.name="Power Fist"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="PowerFist"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Blitzcrank"; s.name="Static Field"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="StaticField"; Spells.push_back(s); }

        // Brand
        { WindupSpellData s; s.charName="Brand"; s.name="Sear"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="BrandQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Brand"; s.name="Pillar of Flame"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="BrandW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Brand"; s.name="Conflagration"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="BrandE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Brand"; s.name="Pyroclasm"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="BrandR"; Spells.push_back(s); }

        // Braum
        { WindupSpellData s; s.charName="Braum"; s.name="Winter's Bite"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="BraumQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Braum"; s.name="Stand Behind Me"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="BraumW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Braum"; s.name="Unbreakable"; s.spellDelay=9; s.spellKey=WindupSpellSlot::E; s.spellName="BraumE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Braum"; s.name="Glacial Fissure"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="BraumRWrapper"; Spells.push_back(s); }

        // Briar
        { WindupSpellData s; s.charName="Briar"; s.name="Head Rush"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="BriarQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Briar"; s.name="Blood Frenzy / Snack Attack"; s.spellDelay=1000; s.spellKey=WindupSpellSlot::W; s.spellName="BriarW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Briar"; s.name="Chilling Scream"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="BriarE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Briar"; s.name="Certain Death"; s.spellDelay=1000; s.spellKey=WindupSpellSlot::R; s.spellName="BriarR"; Spells.push_back(s); }

        // Caitlyn
        { WindupSpellData s; s.charName="Caitlyn"; s.name="Piltover Peacemaker"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="CaitlynQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Caitlyn"; s.name="Yordle Snap Trap"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="CaitlynW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Caitlyn"; s.name="90 Caliber Net"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="CaitlynE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Caitlyn"; s.name="Ace in the Hole"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="CaitlynR"; Spells.push_back(s); }

        // Camille
        { WindupSpellData s; s.charName="Camille"; s.name="Precision Protocol"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="CamilleQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Camille"; s.name="Tactical Sweep"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="CamilleW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Camille"; s.name="Hookshot"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="CamilleE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Camille"; s.name="The Hextech Ultimatum"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="CamilleR"; Spells.push_back(s); }

        // Cassiopeia
        { WindupSpellData s; s.charName="Cassiopeia"; s.name="Noxious Blast"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="CassiopeiaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Cassiopeia"; s.name="Miasma"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="CassiopeiaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Cassiopeia"; s.name="Twin Fang"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="CassiopeiaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Cassiopeia"; s.name="Petrifying Gaze"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="CassiopeiaR"; Spells.push_back(s); }

        // Chogath
        { WindupSpellData s; s.charName="Chogath"; s.name="Rupture"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="Rupture"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Chogath"; s.name="Feral Scream"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="FeralScream"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Chogath"; s.name="Vorpal Spikes"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="VorpalSpikes"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Chogath"; s.name="Feast"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="Feast"; Spells.push_back(s); }

        // Corki
        { WindupSpellData s; s.charName="Corki"; s.name="Phosphorus Bomb"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="PhosphorusBomb"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Corki"; s.name="Valkyrie"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="CarpetBomb"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Corki"; s.name="Gatling Gun"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="GGun"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Corki"; s.name="Missile Barrage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="MissileBarrage"; Spells.push_back(s); }

        // Darius
        { WindupSpellData s; s.charName="Darius"; s.name="Decimate"; s.spellDelay=234; s.spellKey=WindupSpellSlot::Q; s.spellName="DariusCleave"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Darius"; s.name="Crippling Strike"; s.spellDelay=366; s.spellKey=WindupSpellSlot::W; s.spellName="DariusNoxianTacticsONH"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Darius"; s.name="Apprehend"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="DariusAxeGrabCone"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Darius"; s.name="Noxian Guillotine"; s.spellDelay=366; s.spellKey=WindupSpellSlot::R; s.spellName="DariusExecute"; Spells.push_back(s); }

        // Diana
        { WindupSpellData s; s.charName="Diana"; s.name="Crescent Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="DianaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Diana"; s.name="Pale Cascade"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="DianaOrbs"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Diana"; s.name="Lunar Rush"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="DianaTeleport"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Diana"; s.name="Moonfall"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="DianaR"; Spells.push_back(s); }

        // DrMundo
        { WindupSpellData s; s.charName="DrMundo"; s.name="Infected Bonesaw"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="DrMundoQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="DrMundo"; s.name="Heart Zapper"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="DrMundoW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="DrMundo"; s.name="Blunt Force Trauma"; s.spellDelay=1000; s.spellKey=WindupSpellSlot::E; s.spellName="DrMundoE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="DrMundo"; s.name="Maximum Dosage"; s.spellDelay=750; s.spellKey=WindupSpellSlot::R; s.spellName="DrMundoR"; Spells.push_back(s); }

        // Draven
        { WindupSpellData s; s.charName="Draven"; s.name="Spinning Axe"; s.spellDelay=233; s.spellKey=WindupSpellSlot::Q; s.spellName="DravenSpinning"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Draven"; s.name="Blood Rush"; s.spellDelay=241; s.spellKey=WindupSpellSlot::W; s.spellName="DravenFury"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Draven"; s.name="Stand Aside"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="DravenDoubleShot"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Draven"; s.name="Whirling Death"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="DravenRCast"; Spells.push_back(s); }

        // Ekko
        { WindupSpellData s; s.charName="Ekko"; s.name="Timewinder"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="EkkoQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ekko"; s.name="Parallel Convergence"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="EkkoW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ekko"; s.name="Phase Dive"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="EkkoE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ekko"; s.name="Chronobreak"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="EkkoR"; Spells.push_back(s); }

        // Elise
        { WindupSpellData s; s.charName="Elise"; s.name="Neurotoxin / Venomous Bite"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="EliseHumanQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Elise"; s.name="Volatile Spiderling / Skittering Frenzy"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="EliseHumanW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Elise"; s.name="Cocoon / Rappel"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="EliseHumanE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Elise"; s.name="Spider Form"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="EliseR"; Spells.push_back(s); }

        // Evelynn
        { WindupSpellData s; s.charName="Evelynn"; s.name="Hate Spike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="EvelynnQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Evelynn"; s.name="Allure"; s.spellDelay=150; s.spellKey=WindupSpellSlot::W; s.spellName="EvelynnW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Evelynn"; s.name="Whiplash"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="EvelynnE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Evelynn"; s.name="Last Caress"; s.spellDelay=349; s.spellKey=WindupSpellSlot::R; s.spellName="EvelynnR"; Spells.push_back(s); }

        // Ezreal
        { WindupSpellData s; s.charName="Ezreal"; s.name="Mystic Shot"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="EzrealQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ezreal"; s.name="Essence Flux"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="EzrealW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ezreal"; s.name="Arcane Shift"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="EzrealE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ezreal"; s.name="Trueshot Barrage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="EzrealR"; Spells.push_back(s); }

        // Fiddlesticks
        { WindupSpellData s; s.charName="Fiddlesticks"; s.name="Terrify"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="FiddleSticksQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Fiddlesticks"; s.name="Bountiful Harvest"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="FiddleSticksW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Fiddlesticks"; s.name="Reap"; s.spellDelay=400; s.spellKey=WindupSpellSlot::E; s.spellName="FiddleSticksE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Fiddlesticks"; s.name="Crowstorm"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="FiddleSticksR"; Spells.push_back(s); }

        // Fiora
        { WindupSpellData s; s.charName="Fiora"; s.name="Lunge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="FioraQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Fiora"; s.name="Riposte"; s.spellDelay=9; s.spellKey=WindupSpellSlot::W; s.spellName="FioraW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Fiora"; s.name="Bladework"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="FioraE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Fiora"; s.name="Grand Challenge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="FioraR"; Spells.push_back(s); }

        // Fizz
        { WindupSpellData s; s.charName="Fizz"; s.name="Urchin Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="FizzQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Fizz"; s.name="Seastone Trident"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="FizzW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Fizz"; s.name="Playful / Trickster"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="FizzE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Fizz"; s.name="Chum the Waters"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="FizzR"; Spells.push_back(s); }

        // Galio
        { WindupSpellData s; s.charName="Galio"; s.name="Winds of War"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="GalioQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Galio"; s.name="Shield of Durand"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="GalioW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Galio"; s.name="Justice Punch"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="GalioE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Galio"; s.name="Hero's Entrance"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="GalioR"; Spells.push_back(s); }

        // Gangplank
        { WindupSpellData s; s.charName="Gangplank"; s.name="Parrrley"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="GangplankQWrapper"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gangplank"; s.name="Remove Scurvy"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="GangplankW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gangplank"; s.name="Powder Keg"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="GangplankE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gangplank"; s.name="Cannon Barrage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="GangplankR"; Spells.push_back(s); }

        // Garen
        { WindupSpellData s; s.charName="Garen"; s.name="Decisive Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="GarenQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Garen"; s.name="Courage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="GarenW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Garen"; s.name="Judgment"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="GarenE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Garen"; s.name="Demacian Justice"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="GarenR"; Spells.push_back(s); }

        // Gnar
        { WindupSpellData s; s.charName="Gnar"; s.name="Boomerang Throw / Boulder Toss"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="GnarQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gnar"; s.name="Hyper / Wallop"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="GnarW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gnar"; s.name="Hop / Crunch"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="GnarE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gnar"; s.name="GNAR!"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="GnarR"; Spells.push_back(s); }

        // Gragas
        { WindupSpellData s; s.charName="Gragas"; s.name="Barrel Roll"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="GragasQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gragas"; s.name="Drunken Rage"; s.spellDelay=1; s.spellKey=WindupSpellSlot::W; s.spellName="GragasW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gragas"; s.name="Body Slam"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="GragasE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gragas"; s.name="Explosive Cask"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="GragasR"; Spells.push_back(s); }

        // Graves
        { WindupSpellData s; s.charName="Graves"; s.name="End of the Line"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="GravesQLineSpell"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Graves"; s.name="Smoke Screen"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="GravesSmokeGrenade"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Graves"; s.name="Quickdraw"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="GravesMove"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Graves"; s.name="Collateral Damage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="GravesChargeShot"; Spells.push_back(s); }

        // Gwen
        { WindupSpellData s; s.charName="Gwen"; s.name="Snip Snip!"; s.spellDelay=500; s.spellKey=WindupSpellSlot::Q; s.spellName="GwenQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gwen"; s.name="Hallowed Mist"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="GwenW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gwen"; s.name="Skip 'n Slash"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="GwenE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Gwen"; s.name="Needlework"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="GwenR"; Spells.push_back(s); }

        // Hecarim
        { WindupSpellData s; s.charName="Hecarim"; s.name="Rampage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="HecarimRapidSlash"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Hecarim"; s.name="Spirit of Dread"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="HecarimW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Hecarim"; s.name="Devastating Charge"; s.spellDelay=500; s.spellKey=WindupSpellSlot::E; s.spellName="HecarimRamp"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Hecarim"; s.name="Onslaught of Shadows"; s.spellDelay=9; s.spellKey=WindupSpellSlot::R; s.spellName="HecarimUlt"; Spells.push_back(s); }

        // Heimerdinger
        { WindupSpellData s; s.charName="Heimerdinger"; s.name="H-28 G Evolution Turret"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="HeimerdingerQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Heimerdinger"; s.name="Hextech Micro-Rockets"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="HeimerdingerW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Heimerdinger"; s.name="CH-2 Electron Storm Grenade"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="HeimerdingerE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Heimerdinger"; s.name="UPGRADE!!!"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="HeimerdingerR"; Spells.push_back(s); }

        // Hwei
        { WindupSpellData s; s.charName="Hwei"; s.name="Subject: Disaster"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="HweiQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Hwei"; s.name="Subject: Serenity"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="HweiW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Hwei"; s.name="Subject: Torment"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="HweiE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Hwei"; s.name="Spiraling Despair"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="HweiR"; Spells.push_back(s); }

        // Illaoi
        { WindupSpellData s; s.charName="Illaoi"; s.name="Tentacle Smash"; s.spellDelay=750; s.spellKey=WindupSpellSlot::Q; s.spellName="IllaoiQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Illaoi"; s.name="Harsh Lesson"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="IllaoiW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Illaoi"; s.name="Test of Spirit"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="IllaoiE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Illaoi"; s.name="Leap of Faith"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="IllaoiR"; Spells.push_back(s); }

        // Irelia
        { WindupSpellData s; s.charName="Irelia"; s.name="Bladesurge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="IreliaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Irelia"; s.name="Defiant Dance"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="IreliaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Irelia"; s.name="Flawless Duet"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="IreliaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Irelia"; s.name="Vanguard's Edge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="IreliaR"; Spells.push_back(s); }

        // Ivern
        { WindupSpellData s; s.charName="Ivern"; s.name="Rootcaller"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="IvernQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ivern"; s.name="Brushmaker"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="IvernW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ivern"; s.name="Triggerseed"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="IvernE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ivern"; s.name="Daisy!"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="IvernR"; Spells.push_back(s); }

        // Janna
        { WindupSpellData s; s.charName="Janna"; s.name="Howling Gale"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="HowlingGale"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Janna"; s.name="Zephyr"; s.spellDelay=245; s.spellKey=WindupSpellSlot::W; s.spellName="SowTheWind"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Janna"; s.name="Eye Of The Storm"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="EyeOfTheStorm"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Janna"; s.name="Monsoon"; s.spellDelay=1; s.spellKey=WindupSpellSlot::R; s.spellName="ReapTheWhirlwind"; Spells.push_back(s); }

        // JarvanIV
        { WindupSpellData s; s.charName="JarvanIV"; s.name="Dragon Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="JarvanIVDragonStrike"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="JarvanIV"; s.name="Golden Aegis"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="JarvanIVGoldenAegis"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="JarvanIV"; s.name="Demacian Standard"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="JarvanIVDemacianStandard"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="JarvanIV"; s.name="Cataclysm"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="JarvanIVCataclysm"; Spells.push_back(s); }

        // Jax
        { WindupSpellData s; s.charName="Jax"; s.name="Leap Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="JaxQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jax"; s.name="Empower"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="JaxW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jax"; s.name="Counter Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="JaxE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jax"; s.name="Grandmaster-at-Arms"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="JaxR"; Spells.push_back(s); }

        // Jayce
        { WindupSpellData s; s.charName="Jayce"; s.name="To the Skies! / Shock Blast"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="JayceToTheSkies"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jayce"; s.name="Lightning Field / Hyper Charge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="JayceStaticField"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jayce"; s.name="Thundering Blow / Acceleration Gate"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="JayceThunderingBlow"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jayce"; s.name="Mercury Cannon / Mercury Hammer"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="JayceStanceHtG"; Spells.push_back(s); }

        // Jhin
        { WindupSpellData s; s.charName="Jhin"; s.name="Dancing Grenade"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="JhinQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jhin"; s.name="Deadly Flourish"; s.spellDelay=750; s.spellKey=WindupSpellSlot::W; s.spellName="JhinW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jhin"; s.name="Captive Audience"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="JhinE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jhin"; s.name="Curtain Call"; s.spellDelay=1000; s.spellKey=WindupSpellSlot::R; s.spellName="JhinR"; Spells.push_back(s); }

        // Jinx
        { WindupSpellData s; s.charName="Jinx"; s.name="Switcheroo!"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="JinxQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jinx"; s.name="Zap!"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="JinxW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jinx"; s.name="Flame Chompers!"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="JinxE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Jinx"; s.name="Super Mega Death Rocket!"; s.spellDelay=600; s.spellKey=WindupSpellSlot::R; s.spellName="JinxR"; Spells.push_back(s); }

        // KSante
        { WindupSpellData s; s.charName="KSante"; s.name="Ntofo Strikes"; s.spellDelay=349; s.spellKey=WindupSpellSlot::Q; s.spellName="KSanteQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="KSante"; s.name="Path Maker"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KSanteW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="KSante"; s.name="Footwork"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KSanteE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="KSante"; s.name="All Out"; s.spellDelay=400; s.spellKey=WindupSpellSlot::R; s.spellName="KSanteR"; Spells.push_back(s); }

        // Kaisa
        { WindupSpellData s; s.charName="Kaisa"; s.name="Icathian Rain"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KaisaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kaisa"; s.name="Void Seeker"; s.spellDelay=400; s.spellKey=WindupSpellSlot::W; s.spellName="KaisaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kaisa"; s.name="Supercharge"; s.spellDelay=1500; s.spellKey=WindupSpellSlot::E; s.spellName="KaisaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kaisa"; s.name="Killer Instinct"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KaisaR"; Spells.push_back(s); }

        // Kalista
        { WindupSpellData s; s.charName="Kalista"; s.name="Pierce"; s.spellDelay=349; s.spellKey=WindupSpellSlot::Q; s.spellName="KalistaMysticShot"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kalista"; s.name="Sentinel"; s.spellDelay=500; s.spellKey=WindupSpellSlot::W; s.spellName="KalistaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kalista"; s.name="Rend"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KalistaExpungeWrapper"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kalista"; s.name="Fate's Call"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KalistaRx"; Spells.push_back(s); }

        // Karma
        { WindupSpellData s; s.charName="Karma"; s.name="Inner Flame"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KarmaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Karma"; s.name="Focused Resolve"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KarmaSpiritBind"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Karma"; s.name="Inspire"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KarmaSolKimShield"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Karma"; s.name="Mantra"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KarmaMantra"; Spells.push_back(s); }

        // Karthus
        { WindupSpellData s; s.charName="Karthus"; s.name="Lay Waste"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KarthusLayWasteA1"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Karthus"; s.name="Wall of Pain"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KarthusWallOfPain"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Karthus"; s.name="Defile"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KarthusDefile"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Karthus"; s.name="Requiem"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KarthusFallenOne"; Spells.push_back(s); }

        // Kassadin
        { WindupSpellData s; s.charName="Kassadin"; s.name="Null Sphere"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="NullLance"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kassadin"; s.name="Nether Blade"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="NetherBlade"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kassadin"; s.name="Force Pulse"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ForcePulse"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kassadin"; s.name="Riftwalk"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="RiftWalk"; Spells.push_back(s); }

        // Katarina
        { WindupSpellData s; s.charName="Katarina"; s.name="Bouncing Blade"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KatarinaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Katarina"; s.name="Preparation"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KatarinaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Katarina"; s.name="Shunpo"; s.spellDelay=150; s.spellKey=WindupSpellSlot::E; s.spellName="KatarinaEWrapper"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Katarina"; s.name="Death Lotus"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KatarinaR"; Spells.push_back(s); }

        // Kayle
        { WindupSpellData s; s.charName="Kayle"; s.name="Radiant Blast"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KayleQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kayle"; s.name="Celestial Blessing"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KayleW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kayle"; s.name="Starfire Spellblade"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KayleE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kayle"; s.name="Divine Judgment"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="KayleR"; Spells.push_back(s); }

        // Kayn
        { WindupSpellData s; s.charName="Kayn"; s.name="Reaping Slash"; s.spellDelay=150; s.spellKey=WindupSpellSlot::Q; s.spellName="KaynQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kayn"; s.name="Blade's Reach"; s.spellDelay=550; s.spellKey=WindupSpellSlot::W; s.spellName="KaynW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kayn"; s.name="Shadow Step"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KaynE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kayn"; s.name="Umbral Trespass"; s.spellDelay=100; s.spellKey=WindupSpellSlot::R; s.spellName="KaynR"; Spells.push_back(s); }

        // Kennen
        { WindupSpellData s; s.charName="Kennen"; s.name="Thundering Shuriken"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KennenShurikenHurlMissile1"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kennen"; s.name="Electrical Surge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KennenBringTheLight"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kennen"; s.name="Lightning Rush"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KennenLightningRush"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kennen"; s.name="Slicing Maelstrom"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KennenShurikenStorm"; Spells.push_back(s); }

        // Khazix
        { WindupSpellData s; s.charName="Khazix"; s.name="Taste Their Fear"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KhazixQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Khazix"; s.name="Void Spike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KhazixW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Khazix"; s.name="Leap"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KhazixE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Khazix"; s.name="Void Assault"; s.spellDelay=2000; s.spellKey=WindupSpellSlot::R; s.spellName="KhazixR"; Spells.push_back(s); }

        // Kindred
        { WindupSpellData s; s.charName="Kindred"; s.name="Dance of Arrows"; s.spellDelay=9; s.spellKey=WindupSpellSlot::Q; s.spellName="KindredQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kindred"; s.name="Wolf's Frenzy"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KindredW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kindred"; s.name="Mounting Dread"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KindredEWrapper"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kindred"; s.name="Lamb's Respite"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KindredR"; Spells.push_back(s); }

        // Kled
        { WindupSpellData s; s.charName="Kled"; s.name="Bear Trap on a Rope"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KledQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kled"; s.name="Violent Tendencies"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KledW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kled"; s.name="Jousting"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KledE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Kled"; s.name="Chaaaaaaaarge!!!"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KledR"; Spells.push_back(s); }

        // KogMaw
        { WindupSpellData s; s.charName="KogMaw"; s.name="Caustic Spittle"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="KogMawQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="KogMaw"; s.name="Bio-Arcane Barrage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="KogMawBioArcaneBarrage"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="KogMaw"; s.name="Void Ooze"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="KogMawVoidOoze"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="KogMaw"; s.name="Living Artillery"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="KogMawLivingArtillery"; Spells.push_back(s); }

        // Leblanc
        { WindupSpellData s; s.charName="Leblanc"; s.name="Sigil of Malice"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LeblancQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Leblanc"; s.name="Distortion"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="LeblancW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Leblanc"; s.name="Ethereal Chains"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="LeblancE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Leblanc"; s.name="Mimic"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="LeblancR"; Spells.push_back(s); }

        // LeeSin
        { WindupSpellData s; s.charName="LeeSin"; s.name="Sonic Wave / Resonating Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LeeSinQOne"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="LeeSin"; s.name="Safeguard / Iron Will"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="LeeSinWOne"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="LeeSin"; s.name="Tempest / Cripple"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="LeeSinEOne"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="LeeSin"; s.name="Dragon's Rage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="LeeSinR"; Spells.push_back(s); }

        // Leona
        { WindupSpellData s; s.charName="Leona"; s.name="Shield of Daybreak"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LeonaShieldOfDaybreak"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Leona"; s.name="Eclipse"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="LeonaSolarBarrier"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Leona"; s.name="Zenith Blade"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="LeonaZenithBlade"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Leona"; s.name="Solar Flare"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="LeonaSolarFlare"; Spells.push_back(s); }

        // Lillia
        { WindupSpellData s; s.charName="Lillia"; s.name="Blooming Blows"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LilliaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lillia"; s.name="Watch Out! Eep!"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="LilliaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lillia"; s.name="Swirlseed"; s.spellDelay=349; s.spellKey=WindupSpellSlot::E; s.spellName="LilliaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lillia"; s.name="Lilting Lullaby"; s.spellDelay=400; s.spellKey=WindupSpellSlot::R; s.spellName="LilliaR"; Spells.push_back(s); }

        // Lissandra
        { WindupSpellData s; s.charName="Lissandra"; s.name="Ice Shard"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LissandraQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lissandra"; s.name="Ring of Frost"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="LissandraW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lissandra"; s.name="Glacial Path"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="LissandraE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lissandra"; s.name="Frozen Tomb"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="LissandraR"; Spells.push_back(s); }

        // Lucian
        { WindupSpellData s; s.charName="Lucian"; s.name="Piercing Light"; s.spellDelay=349; s.spellKey=WindupSpellSlot::Q; s.spellName="LucianQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lucian"; s.name="Ardent Blaze"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="LucianW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lucian"; s.name="Relentless Pursuit"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="LucianE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lucian"; s.name="The Culling"; s.spellDelay=9; s.spellKey=WindupSpellSlot::R; s.spellName="LucianR"; Spells.push_back(s); }

        // Lulu
        { WindupSpellData s; s.charName="Lulu"; s.name="Glitterlance"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LuluQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lulu"; s.name="Whimsy"; s.spellDelay=241; s.spellKey=WindupSpellSlot::W; s.spellName="LuluW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lulu"; s.name="Help, Pix!"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="LuluE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lulu"; s.name="Wild Growth"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="LuluR"; Spells.push_back(s); }

        // Lux
        { WindupSpellData s; s.charName="Lux"; s.name="Light Binding"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="LuxLightBinding"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lux"; s.name="Prismatic Barrier"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="LuxPrismaticWave"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lux"; s.name="Lucent Singularity"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="LuxLightStrikeKugel"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Lux"; s.name="Final Spark"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="LuxR"; Spells.push_back(s); }

        // Malphite
        { WindupSpellData s; s.charName="Malphite"; s.name="Seismic Shard"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SeismicShard"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Malphite"; s.name="Thunderclap"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="Obduracy"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Malphite"; s.name="Ground Slam"; s.spellDelay=241; s.spellKey=WindupSpellSlot::E; s.spellName="Landslide"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Malphite"; s.name="Unstoppable Force"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="UFSlash"; Spells.push_back(s); }

        // Malzahar
        { WindupSpellData s; s.charName="Malzahar"; s.name="Call of the Void"; s.spellDelay=600; s.spellKey=WindupSpellSlot::Q; s.spellName="MalzaharQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Malzahar"; s.name="Void Swarm"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="MalzaharW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Malzahar"; s.name="Malefic Visions"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="MalzaharE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Malzahar"; s.name="Nether Grasp"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="MalzaharR"; Spells.push_back(s); }

        // Maokai
        { WindupSpellData s; s.charName="Maokai"; s.name="Bramble Smash"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="MaokaiQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Maokai"; s.name="Twisted Advance"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="MaokaiW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Maokai"; s.name="Sapling Toss"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="MaokaiE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Maokai"; s.name="Nature's Grasp"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="MaokaiR"; Spells.push_back(s); }

        // MasterYi
        { WindupSpellData s; s.charName="MasterYi"; s.name="Alpha Strike"; s.spellDelay=100; s.spellKey=WindupSpellSlot::Q; s.spellName="AlphaStrike"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="MasterYi"; s.name="Meditate"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="Meditate"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="MasterYi"; s.name="Wuju Style"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="WujuStyle"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="MasterYi"; s.name="Highlander"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="Highlander"; Spells.push_back(s); }

        // Mel
        { WindupSpellData s; s.charName="Mel"; s.name="Radiant Volley"; s.spellDelay=349; s.spellKey=WindupSpellSlot::Q; s.spellName="MelQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Mel"; s.name="Rebuttal"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="MelW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Mel"; s.name="Solar Snare"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="MelE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Mel"; s.name="Golden Eclipse"; s.spellDelay=750; s.spellKey=WindupSpellSlot::R; s.spellName="MelR"; Spells.push_back(s); }

        // Milio
        { WindupSpellData s; s.charName="Milio"; s.name="Ultra Mega Fire Kick"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="MilioQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Milio"; s.name="Cozy Campfire"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="MilioW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Milio"; s.name="Warm Hugs"; s.spellDelay=9; s.spellKey=WindupSpellSlot::E; s.spellName="MilioE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Milio"; s.name="Breath of Life"; s.spellDelay=712; s.spellKey=WindupSpellSlot::R; s.spellName="MilioR"; Spells.push_back(s); }

        // MissFortune
        { WindupSpellData s; s.charName="MissFortune"; s.name="Double Up"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="MissFortuneRicochetShot"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="MissFortune"; s.name="Strut"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="MissFortuneViciousStrikes"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="MissFortune"; s.name="Make It Rain"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="MissFortuneScattershot"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="MissFortune"; s.name="Bullet Time"; s.spellDelay=1; s.spellKey=WindupSpellSlot::R; s.spellName="MissFortuneBulletTime"; Spells.push_back(s); }

        // MonkeyKing
        { WindupSpellData s; s.charName="MonkeyKing"; s.name="Crushing Blow"; s.spellDelay=500; s.spellKey=WindupSpellSlot::Q; s.spellName="MonkeyKingDoubleAttack"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="MonkeyKing"; s.name="Warrior Trickster"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="MonkeyKingDecoy"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="MonkeyKing"; s.name="Nimbus Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="MonkeyKingNimbus"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="MonkeyKing"; s.name="Cyclone"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="MonkeyKingSpinToWin"; Spells.push_back(s); }

        // Mordekaiser
        { WindupSpellData s; s.charName="Mordekaiser"; s.name="Obliterate"; s.spellDelay=500; s.spellKey=WindupSpellSlot::Q; s.spellName="MordekaiserQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Mordekaiser"; s.name="Indestructible"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="MordekaiserW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Mordekaiser"; s.name="Death's Grasp"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="MordekaiserE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Mordekaiser"; s.name="Realm of Death"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="MordekaiserR"; Spells.push_back(s); }

        // Morgana
        { WindupSpellData s; s.charName="Morgana"; s.name="Dark Binding"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="MorganaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Morgana"; s.name="Tormented Shadow"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="MorganaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Morgana"; s.name="Black Shield"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="MorganaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Morgana"; s.name="Soul Shackles"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="MorganaR"; Spells.push_back(s); }

        // Naafiri
        { WindupSpellData s; s.charName="Naafiri"; s.name="Darkin Daggers"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="NaafiriQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Naafiri"; s.name="The Call of the Pack"; s.spellDelay=750; s.spellKey=WindupSpellSlot::W; s.spellName="NaafiriR"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Naafiri"; s.name="Eviscerate"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="NaafiriE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Naafiri"; s.name="The Call of the Pack"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="NaafiriR"; Spells.push_back(s); }

        // Nami
        { WindupSpellData s; s.charName="Nami"; s.name="Aqua Prison"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="NamiQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nami"; s.name="Ebb and Flow"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="NamiW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nami"; s.name="Tidecaller's Blessing"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="NamiE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nami"; s.name="Tidal Wave"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="NamiR"; Spells.push_back(s); }

        // Nasus
        { WindupSpellData s; s.charName="Nasus"; s.name="Siphoning Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="NasusQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nasus"; s.name="Wither"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="NasusW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nasus"; s.name="Spirit Fire"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="NasusE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nasus"; s.name="Fury of the Sands"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="NasusR"; Spells.push_back(s); }

        // Nautilus
        { WindupSpellData s; s.charName="Nautilus"; s.name="Dredge Line"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="NautilusAnchorDrag"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nautilus"; s.name="Titan's Wrath"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="NautilusPiercingGaze"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nautilus"; s.name="Riptide"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="NautilusSplashZone"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nautilus"; s.name="Depth Charge"; s.spellDelay=460; s.spellKey=WindupSpellSlot::R; s.spellName="NautilusGrandLine"; Spells.push_back(s); }

        // Neeko
        { WindupSpellData s; s.charName="Neeko"; s.name="Blooming Burst"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="NeekoQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Neeko"; s.name="Shapesplitter"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="NeekoW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Neeko"; s.name="Tangle-Barbs"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="NeekoE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Neeko"; s.name="Pop Blossom"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="NeekoR"; Spells.push_back(s); }

        // Nidalee
        { WindupSpellData s; s.charName="Nidalee"; s.name="Javelin Toss / Takedown"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="JavelinToss"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nidalee"; s.name="Bushwhack / Pounce"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="Bushwhack"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nidalee"; s.name="Primal Surge / Swipe"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="PrimalSurge"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nidalee"; s.name="Aspect Of The Cougar"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="AspectOfTheCougar"; Spells.push_back(s); }

        // Nilah
        { WindupSpellData s; s.charName="Nilah"; s.name="Formless Blade"; s.spellDelay=349; s.spellKey=WindupSpellSlot::Q; s.spellName="NilahQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nilah"; s.name="Jubilant Veil"; s.spellDelay=13; s.spellKey=WindupSpellSlot::W; s.spellName="NilahW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nilah"; s.name="Slipstream"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="NilahE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nilah"; s.name="Apotheosis"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="NilahR"; Spells.push_back(s); }

        // Nocturne
        { WindupSpellData s; s.charName="Nocturne"; s.name="Duskbringer"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="NocturneDuskbringer"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nocturne"; s.name="Shroud of Darkness"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="NocturneShroudofDarkness"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nocturne"; s.name="Unspeakable Horror"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="NocturneUnspeakableHorror"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nocturne"; s.name="Paranoia"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="NocturneParanoia"; Spells.push_back(s); }

        // Nunu
        { WindupSpellData s; s.charName="Nunu"; s.name="Consume"; s.spellDelay=300; s.spellKey=WindupSpellSlot::Q; s.spellName="NunuQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nunu"; s.name="Biggest Snowball Ever!"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="NunuW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nunu"; s.name="Snowball Barrage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="NunuE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Nunu"; s.name="Absolute Zero"; s.spellDelay=9; s.spellKey=WindupSpellSlot::R; s.spellName="NunuR"; Spells.push_back(s); }

        // Olaf
        { WindupSpellData s; s.charName="Olaf"; s.name="Undertow"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="OlafAxeThrowCast"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Olaf"; s.name="Tough It Out"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="OlafFrenziedStrikes"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Olaf"; s.name="Reckless Swing"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="OlafRecklessStrike"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Olaf"; s.name="Ragnarok"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="OlafRagnarok"; Spells.push_back(s); }

        // Orianna
        { WindupSpellData s; s.charName="Orianna"; s.name="Command: Attack"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="OrianaIzunaCommand"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Orianna"; s.name="Command: Dissonance"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="OrianaDissonanceCommand"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Orianna"; s.name="Command: Protect"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="OrianaRedactCommand"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Orianna"; s.name="Command: Shockwave"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="OrianaDetonateCommand"; Spells.push_back(s); }

        // Ornn
        { WindupSpellData s; s.charName="Ornn"; s.name="Volcanic Rupture"; s.spellDelay=300; s.spellKey=WindupSpellSlot::Q; s.spellName="OrnnQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ornn"; s.name="Bellows Breath"; s.spellDelay=500; s.spellKey=WindupSpellSlot::W; s.spellName="OrnnW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ornn"; s.name="Searing Charge"; s.spellDelay=349; s.spellKey=WindupSpellSlot::E; s.spellName="OrnnE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ornn"; s.name="Call of the Forge God"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="OrnnR"; Spells.push_back(s); }

        // Pantheon
        { WindupSpellData s; s.charName="Pantheon"; s.name="Comet Spear"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="PantheonQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Pantheon"; s.name="Shield Vault"; s.spellDelay=375; s.spellKey=WindupSpellSlot::W; s.spellName="PantheonW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Pantheon"; s.name="Aegis Assault"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="PantheonE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Pantheon"; s.name="Grand Starfall"; s.spellDelay=100; s.spellKey=WindupSpellSlot::R; s.spellName="PantheonR"; Spells.push_back(s); }

        // Poppy
        { WindupSpellData s; s.charName="Poppy"; s.name="Hammer Shock"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="PoppyQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Poppy"; s.name="Steadfast Presence"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="PoppyW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Poppy"; s.name="Heroic Charge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="PoppyE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Poppy"; s.name="Keeper's Verdict"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="PoppyR"; Spells.push_back(s); }

        // Pyke
        { WindupSpellData s; s.charName="Pyke"; s.name="Bone Skewer"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="PykeQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Pyke"; s.name="Ghostwater Dive"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="PykeW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Pyke"; s.name="Phantom Undertow"; s.spellDelay=275; s.spellKey=WindupSpellSlot::E; s.spellName="PykeE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Pyke"; s.name="Death From Below"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="PykeR"; Spells.push_back(s); }

        // Qiyana
        { WindupSpellData s; s.charName="Qiyana"; s.name="Elemental Wrath / Edge of Ixtal"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="QiyanaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Qiyana"; s.name="Terrashape"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="QiyanaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Qiyana"; s.name="Audacity"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="QiyanaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Qiyana"; s.name="Supreme Display of Talent"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="QiyanaR"; Spells.push_back(s); }

        // Quinn
        { WindupSpellData s; s.charName="Quinn"; s.name="Blinding Assault"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="QuinnQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Quinn"; s.name="Heightened Senses"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="QuinnW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Quinn"; s.name="Vault"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="QuinnE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Quinn"; s.name="Behind Enemy Lines"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="QuinnR"; Spells.push_back(s); }

        // Rakan
        { WindupSpellData s; s.charName="Rakan"; s.name="Gleaming Quill"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RakanQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rakan"; s.name="Grand Entrance"; s.spellDelay=349; s.spellKey=WindupSpellSlot::W; s.spellName="RakanW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rakan"; s.name="Battle Dance"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RakanE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rakan"; s.name="The Quickness"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="RakanR"; Spells.push_back(s); }

        // Rammus
        { WindupSpellData s; s.charName="Rammus"; s.name="Powerball"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="PowerBall"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rammus"; s.name="Defensive Ball Curl"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="DefensiveBallCurl"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rammus"; s.name="Frenzying Taunt"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="PuncturingTaunt"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rammus"; s.name="Soaring Slam"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="Tremors2"; Spells.push_back(s); }

        // RekSai
        { WindupSpellData s; s.charName="RekSai"; s.name="Queen's Wrath / Prey Seeker"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RekSaiQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="RekSai"; s.name="Burrow / Un-burrow"; s.spellDelay=266; s.spellKey=WindupSpellSlot::W; s.spellName="RekSaiW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="RekSai"; s.name="Furious Bite / Tunnel"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RekSaiE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="RekSai"; s.name="Void Rush"; s.spellDelay=349; s.spellKey=WindupSpellSlot::R; s.spellName="RekSaiR"; Spells.push_back(s); }

        // Rell
        { WindupSpellData s; s.charName="Rell"; s.name="Shattering Strike"; s.spellDelay=400; s.spellKey=WindupSpellSlot::Q; s.spellName="RellQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rell"; s.name="Ferromancy: Crash Down"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="RellW_Dismount"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rell"; s.name="Full Tilt"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RellE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rell"; s.name="Magnet Storm"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="RellR"; Spells.push_back(s); }

        // Renata
        { WindupSpellData s; s.charName="Renata"; s.name="Handshake"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RenataQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Renata"; s.name="Bailout"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="RenataW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Renata"; s.name="Loyalty Program"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RenataE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Renata"; s.name="Hostile Takeover"; s.spellDelay=750; s.spellKey=WindupSpellSlot::R; s.spellName="RenataR"; Spells.push_back(s); }

        // Renekton
        { WindupSpellData s; s.charName="Renekton"; s.name="Cull the Meek"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RenektonCleave"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Renekton"; s.name="Ruthless Predator"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="RenektonPreExecute"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Renekton"; s.name="Slice and Dice"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RenektonSliceAndDice"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Renekton"; s.name="Dominus"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="RenektonReignOfTheTyrant"; Spells.push_back(s); }

        // Rengar
        { WindupSpellData s; s.charName="Rengar"; s.name="Savagery"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RengarQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rengar"; s.name="Battle Roar"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="RengarW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rengar"; s.name="Bola Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RengarE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rengar"; s.name="Thrill of the Hunt"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="RengarR"; Spells.push_back(s); }

        // Riven
        { WindupSpellData s; s.charName="Riven"; s.name="Broken Wings"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RivenTriCleave"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Riven"; s.name="Ki Burst"; s.spellDelay=266; s.spellKey=WindupSpellSlot::W; s.spellName="RivenMartyr"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Riven"; s.name="Valor"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RivenFeint"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Riven"; s.name="Blade of the Exile"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="RivenFengShuiEngine"; Spells.push_back(s); }

        // Rumble
        { WindupSpellData s; s.charName="Rumble"; s.name="Flamespitter"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RumbleFlameThrower"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rumble"; s.name="Scrap Shield"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="RumbleShield"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rumble"; s.name="Electro Harpoon"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RumbleGrenade"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Rumble"; s.name="The Equalizer"; s.spellDelay=583; s.spellKey=WindupSpellSlot::R; s.spellName="RumbleCarpetBomb"; Spells.push_back(s); }

        // Ryze
        { WindupSpellData s; s.charName="Ryze"; s.name="Overload"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="RyzeQWrapper"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ryze"; s.name="Rune Prison"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="RyzeW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ryze"; s.name="Spell Flux"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="RyzeE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ryze"; s.name="Realm Warp"; s.spellDelay=750; s.spellKey=WindupSpellSlot::R; s.spellName="RyzeR"; Spells.push_back(s); }

        // Samira
        { WindupSpellData s; s.charName="Samira"; s.name="Flair"; s.spellDelay=50; s.spellKey=WindupSpellSlot::Q; s.spellName="SamiraQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Samira"; s.name="Blade Whirl"; s.spellDelay=9; s.spellKey=WindupSpellSlot::W; s.spellName="SamiraW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Samira"; s.name="Wild Rush"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SamiraE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Samira"; s.name="Inferno Trigger"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SamiraR"; Spells.push_back(s); }

        // Sejuani
        { WindupSpellData s; s.charName="Sejuani"; s.name="Arctic Assault"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SejuaniQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sejuani"; s.name="Winter's Wrath"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SejuaniW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sejuani"; s.name="Permafrost"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SejuaniE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sejuani"; s.name="Glacial Prison"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SejuaniR"; Spells.push_back(s); }

        // Senna
        { WindupSpellData s; s.charName="Senna"; s.name="Piercing Darkness"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SennaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Senna"; s.name="Last Embrace"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SennaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Senna"; s.name="Curse of the Black Mist"; s.spellDelay=1000; s.spellKey=WindupSpellSlot::E; s.spellName="SennaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Senna"; s.name="Dawning Shadow"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="SennaR"; Spells.push_back(s); }

        // Seraphine
        { WindupSpellData s; s.charName="Seraphine"; s.name="High Note"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SeraphineQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Seraphine"; s.name="Surround Sound"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SeraphineW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Seraphine"; s.name="Beat Drop"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SeraphineE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Seraphine"; s.name="Encore"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="SeraphineR"; Spells.push_back(s); }

        // Sett
        { WindupSpellData s; s.charName="Sett"; s.name="Knuckle Down"; s.spellDelay=330; s.spellKey=WindupSpellSlot::Q; s.spellName="SettQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sett"; s.name="Haymaker"; s.spellDelay=750; s.spellKey=WindupSpellSlot::W; s.spellName="SettW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sett"; s.name="Facebreaker"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SettE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sett"; s.name="The Show Stopper"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SettR"; Spells.push_back(s); }

        // Shaco
        { WindupSpellData s; s.charName="Shaco"; s.name="Deceive"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="Deceive"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Shaco"; s.name="Jack In The Box"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="JackInTheBox"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Shaco"; s.name="Two-Shiv Poison"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="TwoShivPoison"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Shaco"; s.name="Hallucinate"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="HallucinateFull"; Spells.push_back(s); }

        // Shen
        { WindupSpellData s; s.charName="Shen"; s.name="Twilight Assault"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ShenQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Shen"; s.name="Spirit's Refuge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ShenW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Shen"; s.name="Shadow Dash"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ShenE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Shen"; s.name="Stand United"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ShenR"; Spells.push_back(s); }

        // Shyvana
        { WindupSpellData s; s.charName="Shyvana"; s.name="ShyvanaQ"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ShyvanaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Shyvana"; s.name="ShyvanaW"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ShyvanaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Shyvana"; s.name="ShyvanaE"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ShyvanaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Shyvana"; s.name="ShyvanaR"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ShyvanaR"; Spells.push_back(s); }

        // Singed
        { WindupSpellData s; s.charName="Singed"; s.name="Poison Trail"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="PoisonTrail"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Singed"; s.name="Mega Adhesive"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="MegaAdhesive"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Singed"; s.name="Fling"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="Fling"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Singed"; s.name="Insanity Potion"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="InsanityPotion"; Spells.push_back(s); }

        // Sion
        { WindupSpellData s; s.charName="Sion"; s.name="Decimating Smash"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SionQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sion"; s.name="Soul Furnace"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SionW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sion"; s.name="Roar of the Slayer"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SionE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sion"; s.name="Unstoppable Onslaught"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SionR"; Spells.push_back(s); }

        // Sivir
        { WindupSpellData s; s.charName="Sivir"; s.name="Boomerang Blade"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SivirQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sivir"; s.name="Ricochet"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SivirW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sivir"; s.name="Spell Shield"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SivirE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sivir"; s.name="On The Hunt"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SivirR"; Spells.push_back(s); }

        // Skarner
        { WindupSpellData s; s.charName="Skarner"; s.name="Shattered Earth / Upheaval"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SkarnerQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Skarner"; s.name="Seismic Bastion"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SkarnerW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Skarner"; s.name="Ixtal's Impact"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SkarnerE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Skarner"; s.name="Impale"; s.spellDelay=750; s.spellKey=WindupSpellSlot::R; s.spellName="SkarnerR"; Spells.push_back(s); }

        // Smolder
        { WindupSpellData s; s.charName="Smolder"; s.name="Super Scorcher Breath"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SmolderQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Smolder"; s.name="Achooo!"; s.spellDelay=349; s.spellKey=WindupSpellSlot::W; s.spellName="SmolderW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Smolder"; s.name="Flap, Flap, Flap"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SmolderE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Smolder"; s.name="MMOOOMMMM!"; s.spellDelay=750; s.spellKey=WindupSpellSlot::R; s.spellName="SmolderR"; Spells.push_back(s); }

        // Sona
        { WindupSpellData s; s.charName="Sona"; s.name="Hymn of Valor"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SonaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sona"; s.name="Aria of Perseverance"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SonaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sona"; s.name="Song of Celerity"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SonaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sona"; s.name="Crescendo"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SonaR"; Spells.push_back(s); }

        // Soraka
        { WindupSpellData s; s.charName="Soraka"; s.name="Starcall"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SorakaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Soraka"; s.name="Astral Infusion"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SorakaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Soraka"; s.name="Equinox"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SorakaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Soraka"; s.name="Wish"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SorakaR"; Spells.push_back(s); }

        // Swain
        { WindupSpellData s; s.charName="Swain"; s.name="Death's Hand"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SwainQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Swain"; s.name="Vision of Empire"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SwainW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Swain"; s.name="Nevermove"; s.spellDelay=50; s.spellKey=WindupSpellSlot::E; s.spellName="SwainE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Swain"; s.name="Demonic Ascension"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SwainR"; Spells.push_back(s); }

        // Sylas
        { WindupSpellData s; s.charName="Sylas"; s.name="Chain Lash"; s.spellDelay=400; s.spellKey=WindupSpellSlot::Q; s.spellName="SylasQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sylas"; s.name="Kingslayer"; s.spellDelay=150; s.spellKey=WindupSpellSlot::W; s.spellName="SylasW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sylas"; s.name="Abscond / Abduct"; s.spellDelay=100; s.spellKey=WindupSpellSlot::E; s.spellName="SylasE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Sylas"; s.name="Hijack"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SylasR"; Spells.push_back(s); }

        // Syndra
        { WindupSpellData s; s.charName="Syndra"; s.name="Dark Sphere"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="SyndraQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Syndra"; s.name="Force of Will"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="SyndraW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Syndra"; s.name="Scatter the Weak"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="SyndraE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Syndra"; s.name="Unleashed Power"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="SyndraR"; Spells.push_back(s); }

        // TahmKench
        { WindupSpellData s; s.charName="TahmKench"; s.name="Tongue Lash"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="TahmKenchQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="TahmKench"; s.name="Abyssal Dive"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="TahmKenchW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="TahmKench"; s.name="Thick Skin"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="TahmKenchE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="TahmKench"; s.name="Devour"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="TahmKenchRWrapper"; Spells.push_back(s); }

        // Taliyah
        { WindupSpellData s; s.charName="Taliyah"; s.name="Threaded Volley"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="TaliyahQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Taliyah"; s.name="Seismic Shove"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="TaliyahWVC"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Taliyah"; s.name="Unraveled Earth"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="TaliyahE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Taliyah"; s.name="Weaver's Wall"; s.spellDelay=9; s.spellKey=WindupSpellSlot::R; s.spellName="TaliyahR"; Spells.push_back(s); }

        // Talon
        { WindupSpellData s; s.charName="Talon"; s.name="Noxian Diplomacy"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="TalonQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Talon"; s.name="Rake"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="TalonW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Talon"; s.name="Assassin's Path"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="TalonE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Talon"; s.name="Shadow Assault"; s.spellDelay=100; s.spellKey=WindupSpellSlot::R; s.spellName="TalonR"; Spells.push_back(s); }

        // Taric
        { WindupSpellData s; s.charName="Taric"; s.name="Starlight's Touch"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="TaricQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Taric"; s.name="Bastion"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="TaricW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Taric"; s.name="Dazzle"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="TaricE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Taric"; s.name="Cosmic Radiance"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="TaricR"; Spells.push_back(s); }

        // Teemo
        { WindupSpellData s; s.charName="Teemo"; s.name="Blinding Dart"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="TeemoQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Teemo"; s.name="Move Quick"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="TeemoW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Teemo"; s.name="Toxic Shot"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="TeemoE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Teemo"; s.name="Noxious Trap"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="TeemoR"; Spells.push_back(s); }

        // Thresh
        { WindupSpellData s; s.charName="Thresh"; s.name="Death Sentence"; s.spellDelay=500; s.spellKey=WindupSpellSlot::Q; s.spellName="ThreshQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Thresh"; s.name="Dark Passage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ThreshW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Thresh"; s.name="Flay"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ThreshE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Thresh"; s.name="The Box"; s.spellDelay=449; s.spellKey=WindupSpellSlot::R; s.spellName="ThreshRPenta"; Spells.push_back(s); }

        // Tristana
        { WindupSpellData s; s.charName="Tristana"; s.name="Rapid Fire"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="TristanaQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Tristana"; s.name="Rocket Jump"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="TristanaW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Tristana"; s.name="Explosive Charge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="TristanaE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Tristana"; s.name="Buster Shot"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="TristanaR"; Spells.push_back(s); }

        // Trundle
        { WindupSpellData s; s.charName="Trundle"; s.name="Chomp"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="TrundleTrollSmash"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Trundle"; s.name="Frozen Domain"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="trundledesecrate"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Trundle"; s.name="Pillar of Ice"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="TrundleCircle"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Trundle"; s.name="Subjugate"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="TrundlePain"; Spells.push_back(s); }

        // Tryndamere
        { WindupSpellData s; s.charName="Tryndamere"; s.name="Bloodlust"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="TryndamereQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Tryndamere"; s.name="Mocking Shout"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="TryndamereW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Tryndamere"; s.name="Spinning Slash"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="TryndamereE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Tryndamere"; s.name="Undying Rage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="UndyingRage"; Spells.push_back(s); }

        // TwistedFate
        { WindupSpellData s; s.charName="TwistedFate"; s.name="Wild Cards"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="WildCards"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="TwistedFate"; s.name="Pick a Card"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="PickACard"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="TwistedFate"; s.name="Stacked Deck"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="CardmasterStack"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="TwistedFate"; s.name="Destiny"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="Destiny"; Spells.push_back(s); }

        // Twitch
        { WindupSpellData s; s.charName="Twitch"; s.name="Ambush"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="TwitchHideInShadows"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Twitch"; s.name="Venom Cask"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="TwitchVenomCask"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Twitch"; s.name="Contaminate"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="TwitchExpunge"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Twitch"; s.name="Spray and Pray"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="TwitchFullAutomatic"; Spells.push_back(s); }

        // Udyr
        { WindupSpellData s; s.charName="Udyr"; s.name="Wilding Claw"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="UdyrQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Udyr"; s.name="Iron Mantle"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="UdyrW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Udyr"; s.name="Blazing Stampede"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="UdyrE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Udyr"; s.name="Wingborne Storm"; s.spellDelay=100; s.spellKey=WindupSpellSlot::R; s.spellName="UdyrR"; Spells.push_back(s); }

        // Urgot
        { WindupSpellData s; s.charName="Urgot"; s.name="Corrosive Charge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="UrgotQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Urgot"; s.name="Purge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="UrgotW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Urgot"; s.name="Disdain"; s.spellDelay=449; s.spellKey=WindupSpellSlot::E; s.spellName="UrgotE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Urgot"; s.name="Fear Beyond Death"; s.spellDelay=500; s.spellKey=WindupSpellSlot::R; s.spellName="UrgotR"; Spells.push_back(s); }

        // Varus
        { WindupSpellData s; s.charName="Varus"; s.name="Piercing Arrow"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="VarusQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Varus"; s.name="Blighted Quiver"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="VarusW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Varus"; s.name="Hail of Arrows"; s.spellDelay=241; s.spellKey=WindupSpellSlot::E; s.spellName="VarusE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Varus"; s.name="Chain of Corruption"; s.spellDelay=241; s.spellKey=WindupSpellSlot::R; s.spellName="VarusR"; Spells.push_back(s); }

        // Vayne
        { WindupSpellData s; s.charName="Vayne"; s.name="Tumble"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="VayneTumble"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vayne"; s.name="Silver Bolts"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="VayneSilveredBolts"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vayne"; s.name="Condemn"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="VayneCondemn"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vayne"; s.name="Final Hour"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="VayneInquisition"; Spells.push_back(s); }

        // Veigar
        { WindupSpellData s; s.charName="Veigar"; s.name="Baleful Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="VeigarBalefulStrike"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Veigar"; s.name="Dark Matter"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="VeigarDarkMatter"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Veigar"; s.name="Event Horizon"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="VeigarEventHorizon"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Veigar"; s.name="Primordial Burst"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="VeigarR"; Spells.push_back(s); }

        // Velkoz
        { WindupSpellData s; s.charName="Velkoz"; s.name="Plasma Fission"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="VelkozQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Velkoz"; s.name="Void Rift"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="VelkozW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Velkoz"; s.name="Tectonic Disruption"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="VelkozE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Velkoz"; s.name="Life Form Disintegration Ray"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="VelkozR"; Spells.push_back(s); }

        // Vex
        { WindupSpellData s; s.charName="Vex"; s.name="Mistral Bolt"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="VexQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vex"; s.name="Personal Space"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="VexW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vex"; s.name="Looming Darkness"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="VexE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vex"; s.name="Shadow Surge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="VexR"; Spells.push_back(s); }

        // Vi
        { WindupSpellData s; s.charName="Vi"; s.name="Vault Breaker"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ViQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vi"; s.name="Denting Blows"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ViW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vi"; s.name="Relentless Force"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ViE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vi"; s.name="Cease and Desist"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ViR"; Spells.push_back(s); }

        // Viego
        { WindupSpellData s; s.charName="Viego"; s.name="Blade of the Ruined King"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ViegoQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Viego"; s.name="Spectral Maw"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ViegoW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Viego"; s.name="Harrowed Path"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ViegoE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Viego"; s.name="Heartbreaker"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ViegoR"; Spells.push_back(s); }

        // Viktor
        { WindupSpellData s; s.charName="Viktor"; s.name="Siphon Power"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ViktorQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Viktor"; s.name="Gravity Field"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ViktorW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Viktor"; s.name="Hextech Ray"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ViktorE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Viktor"; s.name="Arcane Storm"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ViktorR"; Spells.push_back(s); }

        // Vladimir
        { WindupSpellData s; s.charName="Vladimir"; s.name="Transfusion"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="VladimirQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vladimir"; s.name="Sanguine Pool"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="VladimirSanguinePool"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vladimir"; s.name="Tides of Blood"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="VladimirE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Vladimir"; s.name="Hemoplague"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="VladimirHemoplague"; Spells.push_back(s); }

        // Volibear
        { WindupSpellData s; s.charName="Volibear"; s.name="Thundering Smash"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="VolibearQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Volibear"; s.name="Frenzied Maul"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="VolibearW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Volibear"; s.name="Sky Splitter"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="VolibearE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Volibear"; s.name="Stormbringer"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="VolibearR"; Spells.push_back(s); }

        // Warwick
        { WindupSpellData s; s.charName="Warwick"; s.name="Jaws of the Beast"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="WarwickQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Warwick"; s.name="Blood Hunt"; s.spellDelay=500; s.spellKey=WindupSpellSlot::W; s.spellName="WarwickW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Warwick"; s.name="Primal Howl"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="WarwickE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Warwick"; s.name="Infinite Duress"; s.spellDelay=100; s.spellKey=WindupSpellSlot::R; s.spellName="WarwickR"; Spells.push_back(s); }

        // Xayah
        { WindupSpellData s; s.charName="Xayah"; s.name="Double Daggers"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="XayahQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Xayah"; s.name="Deadly Plumage"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="XayahW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Xayah"; s.name="Bladecaller"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="XayahE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Xayah"; s.name="Featherstorm"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="XayahR"; Spells.push_back(s); }

        // Xerath
        { WindupSpellData s; s.charName="Xerath"; s.name="Arcanopulse"; s.spellDelay=4; s.spellKey=WindupSpellSlot::Q; s.spellName="XerathArcanopulseChargeUp"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Xerath"; s.name="Eye of Destruction"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="XerathArcaneBarrage2"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Xerath"; s.name="Shocking Orb"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="XerathMageSpear"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Xerath"; s.name="Rite of the Arcane"; s.spellDelay=9; s.spellKey=WindupSpellSlot::R; s.spellName="XerathLocusOfPower2"; Spells.push_back(s); }

        // XinZhao
        { WindupSpellData s; s.charName="XinZhao"; s.name="Three Talon Strike"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="XinZhaoQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="XinZhao"; s.name="Wind Becomes Lightning"; s.spellDelay=600; s.spellKey=WindupSpellSlot::W; s.spellName="XinZhaoW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="XinZhao"; s.name="Audacious Charge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="XinZhaoE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="XinZhao"; s.name="Crescent Guard"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="XinZhaoR"; Spells.push_back(s); }

        // Yasuo
        { WindupSpellData s; s.charName="Yasuo"; s.name="Steel Tempest"; s.spellDelay=349; s.spellKey=WindupSpellSlot::Q; s.spellName="YasuoQ1Wrapper"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yasuo"; s.name="Wind Wall"; s.spellDelay=13; s.spellKey=WindupSpellSlot::W; s.spellName="YasuoW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yasuo"; s.name="Sweeping Blade"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="YasuoE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yasuo"; s.name="Last Breath"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="YasuoR"; Spells.push_back(s); }

        // Yone
        { WindupSpellData s; s.charName="Yone"; s.name="Mortal Steel"; s.spellDelay=349; s.spellKey=WindupSpellSlot::Q; s.spellName="YoneQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yone"; s.name="Spirit Cleave"; s.spellDelay=500; s.spellKey=WindupSpellSlot::W; s.spellName="YoneW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yone"; s.name="Soul Unbound"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="YoneE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yone"; s.name="Fate Sealed"; s.spellDelay=750; s.spellKey=WindupSpellSlot::R; s.spellName="YoneR"; Spells.push_back(s); }

        // Yorick
        { WindupSpellData s; s.charName="Yorick"; s.name="Last Rites"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="YorickQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yorick"; s.name="Dark Procession"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="YorickW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yorick"; s.name="Mourning Mist"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="YorickE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yorick"; s.name="Eulogy of the Isles"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="YorickR"; Spells.push_back(s); }

        // Yuumi
        { WindupSpellData s; s.charName="Yuumi"; s.name="Prowling Projectile"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="YuumiQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yuumi"; s.name="You and Me!"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="YuumiW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yuumi"; s.name="Zoomies"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="YuumiE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Yuumi"; s.name="Final Chapter"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="YuumiR"; Spells.push_back(s); }

        // Zac
        { WindupSpellData s; s.charName="Zac"; s.name="Stretching Strikes"; s.spellDelay=330; s.spellKey=WindupSpellSlot::Q; s.spellName="ZacQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zac"; s.name="Unstable Matter"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ZacW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zac"; s.name="Elastic Slingshot"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ZacE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zac"; s.name="Let's Bounce!"; s.spellDelay=300; s.spellKey=WindupSpellSlot::R; s.spellName="ZacR"; Spells.push_back(s); }

        // Zed
        { WindupSpellData s; s.charName="Zed"; s.name="Razor Shuriken"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ZedQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zed"; s.name="Living Shadow"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ZedW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zed"; s.name="Shadow Slash"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ZedE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zed"; s.name="Death Mark"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ZedR"; Spells.push_back(s); }

        // Zeri
        { WindupSpellData s; s.charName="Zeri"; s.name="Burst Fire"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ZeriQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zeri"; s.name="Ultrashock Laser"; s.spellDelay=550; s.spellKey=WindupSpellSlot::W; s.spellName="ZeriW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zeri"; s.name="Spark Surge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ZeriE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zeri"; s.name="Lightning Crash"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ZeriR"; Spells.push_back(s); }

        // Ziggs
        { WindupSpellData s; s.charName="Ziggs"; s.name="Bouncing Bomb"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ZiggsQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ziggs"; s.name="Satchel Charge"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ZiggsW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ziggs"; s.name="Hexplosive Minefield"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ZiggsE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Ziggs"; s.name="Mega Inferno Bomb"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ZiggsR"; Spells.push_back(s); }

        // Zilean
        { WindupSpellData s; s.charName="Zilean"; s.name="Time Bomb"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ZileanQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zilean"; s.name="Rewind"; s.spellDelay=250; s.spellKey=WindupSpellSlot::W; s.spellName="ZileanW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zilean"; s.name="Time Warp"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="TimeWarp"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zilean"; s.name="Chronoshift"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ChronoShift"; Spells.push_back(s); }

        // Zoe
        { WindupSpellData s; s.charName="Zoe"; s.name="Paddle Star!"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ZoeQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zoe"; s.name="Spell Thief"; s.spellDelay=9; s.spellKey=WindupSpellSlot::W; s.spellName="ZoeW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zoe"; s.name="Sleepy Trouble Bubble"; s.spellDelay=300; s.spellKey=WindupSpellSlot::E; s.spellName="ZoeE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zoe"; s.name="Portal Jump"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ZoeR"; Spells.push_back(s); }

        // Zyra
        { WindupSpellData s; s.charName="Zyra"; s.name="Deadly Spines"; s.spellDelay=250; s.spellKey=WindupSpellSlot::Q; s.spellName="ZyraQ"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zyra"; s.name="Rampant Growth"; s.spellDelay=243; s.spellKey=WindupSpellSlot::W; s.spellName="ZyraW"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zyra"; s.name="Grasping Roots"; s.spellDelay=250; s.spellKey=WindupSpellSlot::E; s.spellName="ZyraE"; Spells.push_back(s); }
        { WindupSpellData s; s.charName="Zyra"; s.name="Stranglethorns"; s.spellDelay=250; s.spellKey=WindupSpellSlot::R; s.spellName="ZyraR"; Spells.push_back(s); }

    }
};

inline std::vector<WindupSpellData> SpellWindupDatabase::Spells;
} // namespace ZDEvade
