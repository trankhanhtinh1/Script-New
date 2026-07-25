#pragma once
#include "EvadeSpellData.h"

namespace ZDEvade {
class EvadeSpellDatabase {
public:
    static std::vector<EvadeSpellData> Spells;

    static void Initialize() {
        if (!Spells.empty()) return;

        // === AllChampions ===
        {
            EvadeSpellData spell;
            spell.charName = "AllChampions";
            spell.dangerlevel = 3;
            spell.name = "Talisman of Ascension";
            spell.spellName = "TalismanOfAscension";
            spell.spellDelay = 50.0f;
            spell.speedArray = { 40.0f, 40.0f, 40.0f, 40.0f, 40.0f };
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.isItem = true;
            spell.itemID = 3060;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "AllChampions";
            spell.dangerlevel = 3;
            spell.name = "Youmuu's Ghostblade";
            spell.spellName = "YoumuusGhostblade";
            spell.spellDelay = 50.0f;
            spell.speedArray = { 20.0f, 20.0f, 20.0f, 20.0f, 20.0f };
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.isItem = true;
            spell.itemID = 3142;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "AllChampions";
            spell.dangerlevel = 4;
            spell.name = "Flash";
            spell.spellName = "SummonerFlash";
            spell.spellDelay = 50.0f;
            spell.range = 400.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Blink;
            spell.isSummonerSpell = true;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "AllChampions";
            spell.dangerlevel = 4;
            spell.name = "Hourglass";
            spell.spellName = "ZhonyasHourglass";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::SpellShield;
            spell.isItem = true;
            spell.itemID = 3157;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "AllChampions";
            spell.dangerlevel = 4;
            spell.name = "Witchcap";
            spell.spellName = "Witchcap";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::SpellShield;
            spell.isItem = true;
            spell.itemID = 3159;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Ahri ===
        {
            EvadeSpellData spell;
            spell.charName = "Ahri";
            spell.dangerlevel = 4;
            spell.name = "AhriTumble";
            spell.spellName = "AhriTumble";
            spell.spellDelay = 50.0f;
            spell.range = 500.0f;
            spell.speed = 1575.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Akali ===
        {
            EvadeSpellData spell;
            spell.charName = "Akali";
            spell.dangerlevel = 4;
            spell.name = "Shadow Dance";
            spell.spellName = "AkaliShadowDance";
            spell.spellDelay = 50.0f;
            spell.range = 700.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Alistar ===
        {
            EvadeSpellData spell;
            spell.charName = "Alistar";
            spell.dangerlevel = 3;
            spell.name = "Headbutt";
            spell.spellName = "Pulverize";
            spell.spellDelay = 250.0f;
            spell.range = 650.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Ambessa ===
        {
            EvadeSpellData spell;
            spell.charName = "Ambessa";
            spell.dangerlevel = 5;
            spell.name = "Public Execution";
            spell.spellName = "AmbessaR";
            spell.spellDelay = 50.0f;
            spell.range = 800.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Aurora ===
        {
            EvadeSpellData spell;
            spell.charName = "Aurora";
            spell.dangerlevel = 3;
            spell.name = "Across the Divide";
            spell.spellName = "AuroraE";
            spell.spellDelay = 50.0f;
            spell.range = 400.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Blitzcrank ===
        {
            EvadeSpellData spell;
            spell.charName = "Blitzcrank";
            spell.dangerlevel = 3;
            spell.name = "Overdrive";
            spell.spellName = "Overdrive";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 70.0f, 75.0f, 80.0f, 85.0f, 90.0f };
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Briar ===
        {
            EvadeSpellData spell;
            spell.charName = "Briar";
            spell.dangerlevel = 5;
            spell.name = "Certain Death";
            spell.spellName = "BriarR";
            spell.spellDelay = 50.0f;
            spell.range = 800.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Caitlyn ===
        {
            EvadeSpellData spell;
            spell.charName = "Caitlyn";
            spell.dangerlevel = 3;
            spell.name = "CaitlynEntrapment";
            spell.spellName = "CaitlynEntrapment";
            spell.spellDelay = 50.0f;
            spell.range = 400.0f;
            spell.speed = 975.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.isReversed = true;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Camille ===
        {
            EvadeSpellData spell;
            spell.charName = "Camille";
            spell.dangerlevel = 4;
            spell.name = "Hookshot";
            spell.spellName = "CamilleE";
            spell.spellDelay = 50.0f;
            spell.range = 800.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Camille";
            spell.dangerlevel = 5;
            spell.name = "The Hextech Ultimatum";
            spell.spellName = "CamilleR";
            spell.spellDelay = 50.0f;
            spell.range = 500.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Corki ===
        {
            EvadeSpellData spell;
            spell.charName = "Corki";
            spell.dangerlevel = 3;
            spell.name = "CarpetBomb";
            spell.spellName = "CarpetBomb";
            spell.spellDelay = 50.0f;
            spell.range = 790.0f;
            spell.speed = 975.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Draven ===
        {
            EvadeSpellData spell;
            spell.charName = "Draven";
            spell.dangerlevel = 3;
            spell.name = "Blood Rush";
            spell.spellName = "DravenFury";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 40.0f, 45.0f, 50.0f, 55.0f, 60.0f };
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Ekko ===
        {
            EvadeSpellData spell;
            spell.charName = "Ekko";
            spell.dangerlevel = 3;
            spell.name = "PhaseDive";
            spell.spellName = "EkkoE";
            spell.spellDelay = 50.0f;
            spell.range = 350.0f;
            spell.speed = 1150.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Ekko";
            spell.dangerlevel = 3;
            spell.name = "PhaseDive2";
            spell.spellName = "EkkoEAttack";
            spell.spellDelay = 250.0f;
            spell.range = 490.0f;
            spell.spellKey = EvadeSpellSlot::Recall;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Ekko";
            spell.dangerlevel = 4;
            spell.name = "Chronobreak";
            spell.spellName = "EkkoR";
            spell.spellDelay = 50.0f;
            spell.range = 20000.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Elise ===
        {
            EvadeSpellData spell;
            spell.charName = "Elise";
            spell.dangerlevel = 4;
            spell.name = "Rappel";
            spell.spellName = "EliseSpiderEInitial";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::SpellShield;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Evelynn ===
        {
            EvadeSpellData spell;
            spell.charName = "Evelynn";
            spell.dangerlevel = 3;
            spell.name = "Darl Frenzy";
            spell.spellName = "EvelynnW";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 30.0f, 45.0f, 50.0f, 60.0f, 70.0f };
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Ezreal ===
        {
            EvadeSpellData spell;
            spell.charName = "Ezreal";
            spell.dangerlevel = 2;
            spell.name = "ArcaneShift";
            spell.spellName = "EzrealArcaneShift";
            spell.spellDelay = 250.0f;
            spell.range = 450.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Fiora ===
        {
            EvadeSpellData spell;
            spell.charName = "Fiora";
            spell.dangerlevel = 3;
            spell.name = "FioraW";
            spell.spellName = "FioraW";
            spell.spellDelay = 100.0f;
            spell.range = 750.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::WindWall;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Fiora";
            spell.dangerlevel = 3;
            spell.name = "FioraQ";
            spell.spellName = "FioraQ";
            spell.spellDelay = 50.0f;
            spell.range = 340.0f;
            spell.speed = 1100.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Fizz ===
        {
            EvadeSpellData spell;
            spell.charName = "Fizz";
            spell.dangerlevel = 3;
            spell.name = "FizzPiercingStrike";
            spell.spellName = "FizzPiercingStrike";
            spell.spellDelay = 50.0f;
            spell.range = 550.0f;
            spell.speed = 1400.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Fizz";
            spell.dangerlevel = 3;
            spell.name = "FizzJump";
            spell.spellName = "FizzJump";
            spell.spellDelay = 50.0f;
            spell.range = 400.0f;
            spell.speed = 1400.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Fizz";
            spell.dangerlevel = 4;
            spell.name = "Playful / Trickster";
            spell.spellName = "FizzE";
            spell.spellDelay = 50.0f;
            spell.range = 400.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Galio ===
        {
            EvadeSpellData spell;
            spell.charName = "Galio";
            spell.dangerlevel = 4;
            spell.name = "Justice Punch";
            spell.spellName = "GalioE";
            spell.spellDelay = 400.0f;
            spell.range = 650.0f;
            spell.speed = 2300.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Garen ===
        {
            EvadeSpellData spell;
            spell.charName = "Garen";
            spell.dangerlevel = 3;
            spell.name = "Decisive Strike";
            spell.spellName = "GarenQ";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 35.0f, 35.0f, 35.0f, 35.0f, 35.0f };
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Garen";
            spell.dangerlevel = 2;
            spell.name = "Decisive Strike";
            spell.spellName = "GarenQ";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 35.0f, 40.0f, 45.0f, 50.0f, 55.0f };
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Gnar ===
        {
            EvadeSpellData spell;
            spell.charName = "Gnar";
            spell.dangerlevel = 3;
            spell.name = "GnarE";
            spell.spellName = "GnarE";
            spell.spellDelay = 50.0f;
            spell.range = 475.0f;
            spell.speed = 900.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Gnar";
            spell.dangerlevel = 4;
            spell.name = "GnarBigE";
            spell.spellName = "gnarbige";
            spell.spellDelay = 50.0f;
            spell.range = 475.0f;
            spell.speed = 800.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Gragas ===
        {
            EvadeSpellData spell;
            spell.charName = "Gragas";
            spell.dangerlevel = 2;
            spell.name = "BodySlam";
            spell.spellName = "GragasBodySlam";
            spell.spellDelay = 50.0f;
            spell.range = 600.0f;
            spell.speed = 900.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Graves ===
        {
            EvadeSpellData spell;
            spell.charName = "Graves";
            spell.dangerlevel = 2;
            spell.name = "QuickDraw";
            spell.spellName = "GravesMove";
            spell.spellDelay = 50.0f;
            spell.range = 425.0f;
            spell.speed = 1250.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Gwen ===
        {
            EvadeSpellData spell;
            spell.charName = "Gwen";
            spell.dangerlevel = 3;
            spell.name = "Hallowed Mist";
            spell.spellName = "GwenW";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::SpellShield;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Gwen";
            spell.dangerlevel = 3;
            spell.name = "Skip 'n Slash";
            spell.spellName = "GwenE";
            spell.spellDelay = 50.0f;
            spell.range = 350.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Irelia ===
        {
            EvadeSpellData spell;
            spell.charName = "Irelia";
            spell.dangerlevel = 3;
            spell.name = "Bladesurge";
            spell.spellName = "IreliaQ";
            spell.spellDelay = 50.0f;
            spell.range = 625.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === JarvanIV ===
        {
            EvadeSpellData spell;
            spell.charName = "JarvanIV";
            spell.dangerlevel = 4;
            spell.name = "Dragon Strike";
            spell.spellName = "JarvanIVQ";
            spell.spellDelay = 250.0f;
            spell.range = 770.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "JarvanIV";
            spell.dangerlevel = 5;
            spell.name = "Cataclysm";
            spell.spellName = "JarvanIVR";
            spell.spellDelay = 50.0f;
            spell.range = 625.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Jax ===
        {
            EvadeSpellData spell;
            spell.charName = "Jax";
            spell.dangerlevel = 3;
            spell.name = "Leap Strike";
            spell.spellName = "JaxQ";
            spell.spellDelay = 50.0f;
            spell.range = 700.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Kaisa ===
        {
            EvadeSpellData spell;
            spell.charName = "Kaisa";
            spell.dangerlevel = 3;
            spell.name = "Supercharge";
            spell.spellName = "KaisaE";
            spell.spellDelay = 50.0f;
            spell.speedArray = { 40.0f, 50.0f, 60.0f, 70.0f, 80.0f };
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Kaisa";
            spell.dangerlevel = 5;
            spell.name = "Killer Instinct";
            spell.spellName = "KaisaR";
            spell.spellDelay = 50.0f;
            spell.range = 1500.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Karma ===
        {
            EvadeSpellData spell;
            spell.charName = "Karma";
            spell.dangerlevel = 3;
            spell.name = "Inspire";
            spell.spellName = "KarmaSolkimShield";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 40.0f, 45.0f, 50.0f, 55.0f, 60.0f };
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Kassadin ===
        {
            EvadeSpellData spell;
            spell.charName = "Kassadin";
            spell.dangerlevel = 1;
            spell.name = "RiftWalk";
            spell.spellDelay = 250.0f;
            spell.range = 450.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Katarina ===
        {
            EvadeSpellData spell;
            spell.charName = "Katarina";
            spell.dangerlevel = 3;
            spell.name = "KatarinaE";
            spell.spellName = "KatarinaE";
            spell.spellDelay = 50.0f;
            spell.range = 700.0f;
            spell.speed = 3.4f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Kayle ===
        {
            EvadeSpellData spell;
            spell.charName = "Kayle";
            spell.dangerlevel = 3;
            spell.name = "Divine Blessing";
            spell.spellName = "JudicatorDivineBlessing";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 18.0f, 21.0f, 24.0f, 27.0f, 30.0f };
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Kayle";
            spell.dangerlevel = 4;
            spell.name = "Intervention";
            spell.spellName = "JudicatorIntervention";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::SpellShield;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Kayn ===
        {
            EvadeSpellData spell;
            spell.charName = "Kayn";
            spell.dangerlevel = 3;
            spell.name = "Reaping Slash";
            spell.spellName = "KaynQ";
            spell.spellDelay = 50.0f;
            spell.range = 350.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Kennen ===
        {
            EvadeSpellData spell;
            spell.charName = "Kennen";
            spell.dangerlevel = 4;
            spell.name = "Lightning Rush";
            spell.spellName = "KennenLightningRush";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 100.0f, 100.0f, 100.0f, 100.0f, 100.0f };
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Kindred ===
        {
            EvadeSpellData spell;
            spell.charName = "Kindred";
            spell.dangerlevel = 1;
            spell.name = "KindredQ";
            spell.spellName = "KindredQ";
            spell.spellDelay = 50.0f;
            spell.range = 300.0f;
            spell.speed = 733.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Kindred";
            spell.dangerlevel = 3;
            spell.name = "Dance of Arrows";
            spell.spellName = "KindredQ";
            spell.spellDelay = 50.0f;
            spell.range = 300.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Kled ===
        {
            EvadeSpellData spell;
            spell.charName = "Kled";
            spell.dangerlevel = 4;
            spell.name = "Jousting";
            spell.spellName = "KledE";
            spell.spellDelay = 50.0f;
            spell.range = 500.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Kled";
            spell.dangerlevel = 5;
            spell.name = "Chaaaaaaaarge!!!";
            spell.spellName = "KledR";
            spell.spellDelay = 50.0f;
            spell.range = 900.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === KSante ===
        {
            EvadeSpellData spell;
            spell.charName = "KSante";
            spell.dangerlevel = 3;
            spell.name = "Footwork";
            spell.spellName = "KSanteE";
            spell.spellDelay = 50.0f;
            spell.range = 400.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Leblanc ===
        {
            EvadeSpellData spell;
            spell.charName = "Leblanc";
            spell.dangerlevel = 2;
            spell.name = "Distortion";
            spell.spellName = "LeblancSlide";
            spell.spellDelay = 50.0f;
            spell.range = 600.0f;
            spell.speed = 1600.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Leblanc";
            spell.dangerlevel = 2;
            spell.name = "DistortionR";
            spell.spellName = "LeblancSlideM";
            spell.spellDelay = 50.0f;
            spell.range = 600.0f;
            spell.speed = 1600.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === LeeSin ===
        {
            EvadeSpellData spell;
            spell.charName = "LeeSin";
            spell.dangerlevel = 3;
            spell.name = "LeeSinW";
            spell.spellName = "BlindMonkWOne";
            spell.spellDelay = 50.0f;
            spell.range = 700.0f;
            spell.speed = 1400.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Shield;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Leona ===
        {
            EvadeSpellData spell;
            spell.charName = "Leona";
            spell.dangerlevel = 4;
            spell.name = "Zenith Blade";
            spell.spellName = "LeonaE";
            spell.spellDelay = 50.0f;
            spell.range = 875.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Lillia ===
        {
            EvadeSpellData spell;
            spell.charName = "Lillia";
            spell.dangerlevel = 3;
            spell.name = "Watch Out! Eep!";
            spell.spellName = "LilliaW";
            spell.spellDelay = 50.0f;
            spell.range = 350.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Locke ===
        {
            EvadeSpellData spell;
            spell.charName = "Locke";
            spell.dangerlevel = 4;
            spell.name = "LockeE";
            spell.spellName = "LockeE";
            spell.spellDelay = 50.0f;
            spell.range = 400.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Locke";
            spell.dangerlevel = 5;
            spell.name = "LockeR";
            spell.spellName = "LockeR";
            spell.spellDelay = 50.0f;
            spell.range = 600.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Lucian ===
        {
            EvadeSpellData spell;
            spell.charName = "Lucian";
            spell.dangerlevel = 1;
            spell.name = "RelentlessPursuit";
            spell.spellName = "LucianE";
            spell.spellDelay = 50.0f;
            spell.range = 425.0f;
            spell.speed = 1350.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Lulu ===
        {
            EvadeSpellData spell;
            spell.charName = "Lulu";
            spell.dangerlevel = 3;
            spell.name = "Whimsy";
            spell.spellName = "LuluW";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 30.0f, 30.0f, 30.0f, 35.0f, 40.0f };
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === MasterYi ===
        {
            EvadeSpellData spell;
            spell.charName = "MasterYi";
            spell.dangerlevel = 3;
            spell.name = "AlphaStrike";
            spell.spellName = "AlphaStrike";
            spell.spellDelay = 100.0f;
            spell.range = 600.0f;
            spell.speed = 3.4f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "MasterYi";
            spell.dangerlevel = 4;
            spell.name = "Alpha Strike";
            spell.spellName = "AlphaStrike";
            spell.spellDelay = 50.0f;
            spell.range = 600.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "MasterYi";
            spell.dangerlevel = 3;
            spell.name = "Highlander";
            spell.spellName = "MasterYiR";
            spell.spellDelay = 50.0f;
            spell.speedArray = { 35.0f, 45.0f, 55.0f, 65.0f, 75.0f };
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Milio ===
        {
            EvadeSpellData spell;
            spell.charName = "Milio";
            spell.dangerlevel = 3;
            spell.name = "Warm Hugs";
            spell.spellName = "MilioE";
            spell.spellDelay = 50.0f;
            spell.range = 800.0f;
            spell.speedArray = { 15.0f, 20.0f, 25.0f, 30.0f, 35.0f };
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === MonkeyKing ===
        {
            EvadeSpellData spell;
            spell.charName = "MonkeyKing";
            spell.dangerlevel = 3;
            spell.name = "Nimbus Strike";
            spell.spellName = "MonkeyKingNimbusKick";
            spell.spellDelay = 50.0f;
            spell.range = 650.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Morgana ===
        {
            EvadeSpellData spell;
            spell.charName = "Morgana";
            spell.dangerlevel = 3;
            spell.name = "BlackShield";
            spell.spellName = "BlackShield";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::SpellShield;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Naafiri ===
        {
            EvadeSpellData spell;
            spell.charName = "Naafiri";
            spell.dangerlevel = 3;
            spell.name = "Hounds' Quest";
            spell.spellName = "NaafiriW";
            spell.spellDelay = 50.0f;
            spell.range = 800.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Naafiri";
            spell.dangerlevel = 5;
            spell.name = "We Are More";
            spell.spellName = "NaafiriR";
            spell.spellDelay = 50.0f;
            spell.speedArray = { 50.0f, 60.0f, 70.0f };
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Neeko ===
        {
            EvadeSpellData spell;
            spell.charName = "Neeko";
            spell.dangerlevel = 3;
            spell.name = "Shapesplitter";
            spell.spellName = "NeekoW";
            spell.spellDelay = 50.0f;
            spell.speedArray = { 20.0f, 25.0f, 30.0f, 35.0f, 40.0f };
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Nidalee ===
        {
            EvadeSpellData spell;
            spell.charName = "Nidalee";
            spell.dangerlevel = 4;
            spell.name = "Pounce";
            spell.spellName = "Pounce";
            spell.spellDelay = 150.0f;
            spell.range = 375.0f;
            spell.speed = 1750.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Nilah ===
        {
            EvadeSpellData spell;
            spell.charName = "Nilah";
            spell.dangerlevel = 4;
            spell.name = "Apotheosis";
            spell.spellName = "NilahR";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::SpellShield;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Nocturne ===
        {
            EvadeSpellData spell;
            spell.charName = "Nocturne";
            spell.dangerlevel = 3;
            spell.name = "ShroudofDarkness";
            spell.spellName = "NocturneShroudofDarkness";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::SpellShield;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Nunu ===
        {
            EvadeSpellData spell;
            spell.charName = "Nunu";
            spell.dangerlevel = 2;
            spell.name = "BloodBoil";
            spell.spellName = "BloodBoil";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 8.0f, 9.0f, 10.0f, 11.0f, 12.0f };
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Poppy ===
        {
            EvadeSpellData spell;
            spell.charName = "Poppy";
            spell.dangerlevel = 3;
            spell.name = "Steadfast Presence";
            spell.spellName = "PoppyW";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 27.0f, 29.0f, 31.0f, 33.0f, 35.0f };
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Poppy";
            spell.dangerlevel = 4;
            spell.name = "Heroic Charge";
            spell.spellName = "PoppyE";
            spell.spellDelay = 50.0f;
            spell.range = 525.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Pyke ===
        {
            EvadeSpellData spell;
            spell.charName = "Pyke";
            spell.dangerlevel = 4;
            spell.name = "Phantom Undertow";
            spell.spellName = "PykeE";
            spell.spellDelay = 50.0f;
            spell.range = 550.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Qiyana ===
        {
            EvadeSpellData spell;
            spell.charName = "Qiyana";
            spell.dangerlevel = 4;
            spell.name = "Edge of Ixtal";
            spell.spellName = "QiyanaE";
            spell.spellDelay = 50.0f;
            spell.range = 650.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Rakan ===
        {
            EvadeSpellData spell;
            spell.charName = "Rakan";
            spell.dangerlevel = 4;
            spell.name = "Grand Entrance";
            spell.spellName = "RakanW";
            spell.spellDelay = 50.0f;
            spell.range = 600.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Rakan";
            spell.dangerlevel = 3;
            spell.name = "Battle Dance";
            spell.spellName = "RakanE";
            spell.spellDelay = 50.0f;
            spell.range = 500.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Rakan";
            spell.dangerlevel = 5;
            spell.name = "The Quickness";
            spell.spellName = "RakanR";
            spell.spellDelay = 50.0f;
            spell.speedArray = { 50.0f, 60.0f, 70.0f };
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Rammus ===
        {
            EvadeSpellData spell;
            spell.charName = "Rammus";
            spell.dangerlevel = 3;
            spell.name = "Powerball";
            spell.spellName = "RammusW";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 25.0f, 30.0f, 35.0f, 40.0f, 45.0f };
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === RekSai ===
        {
            EvadeSpellData spell;
            spell.charName = "RekSai";
            spell.dangerlevel = 4;
            spell.name = "Burrowed Unburrow";
            spell.spellName = "RekSaiE";
            spell.spellDelay = 50.0f;
            spell.range = 250.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "RekSai";
            spell.dangerlevel = 5;
            spell.name = "Void Rush";
            spell.spellName = "RekSaiR";
            spell.spellDelay = 50.0f;
            spell.range = 1500.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Rell ===
        {
            EvadeSpellData spell;
            spell.charName = "Rell";
            spell.dangerlevel = 4;
            spell.name = "Ferromancy: Mount Up";
            spell.spellName = "RellW";
            spell.spellDelay = 50.0f;
            spell.range = 400.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Renata ===
        {
            EvadeSpellData spell;
            spell.charName = "Renata";
            spell.dangerlevel = 3;
            spell.name = "Loyalty Program";
            spell.spellName = "RenataE";
            spell.spellDelay = 250.0f;
            spell.range = 800.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Shield;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Riven ===
        {
            EvadeSpellData spell;
            spell.charName = "Riven";
            spell.dangerlevel = 1;
            spell.name = "BrokenWings";
            spell.spellName = "RivenTriCleave";
            spell.spellDelay = 50.0f;
            spell.range = 260.0f;
            spell.speed = 560.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Riven";
            spell.dangerlevel = 1;
            spell.name = "Valor";
            spell.spellName = "RivenFeint";
            spell.spellDelay = 50.0f;
            spell.range = 325.0f;
            spell.speed = 1200.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Rumble ===
        {
            EvadeSpellData spell;
            spell.charName = "Rumble";
            spell.dangerlevel = 3;
            spell.name = "Scrap Shield";
            spell.spellName = "RumbleShield";
            spell.spellDelay = 50.0f;
            spell.speedArray = { 10.0f, 15.0f, 20.0f, 25.0f, 30.0f };
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Samira ===
        {
            EvadeSpellData spell;
            spell.charName = "Samira";
            spell.dangerlevel = 3;
            spell.name = "Wild Rush";
            spell.spellName = "SamiraE";
            spell.spellDelay = 50.0f;
            spell.range = 600.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Sejuani ===
        {
            EvadeSpellData spell;
            spell.charName = "Sejuani";
            spell.dangerlevel = 4;
            spell.name = "Arctic Assault";
            spell.spellName = "SejuaniQ";
            spell.spellDelay = 50.0f;
            spell.range = 650.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Sett ===
        {
            EvadeSpellData spell;
            spell.charName = "Sett";
            spell.dangerlevel = 5;
            spell.name = "The Show Stopper";
            spell.spellName = "SettR";
            spell.spellDelay = 50.0f;
            spell.range = 500.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Shaco ===
        {
            EvadeSpellData spell;
            spell.charName = "Shaco";
            spell.dangerlevel = 3;
            spell.name = "Deceive";
            spell.spellName = "Deceive";
            spell.spellDelay = 250.0f;
            spell.range = 400.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Shyvana ===
        {
            EvadeSpellData spell;
            spell.charName = "Shyvana";
            spell.dangerlevel = 3;
            spell.name = "Burnout";
            spell.spellName = "ShyvanaImmolationAura";
            spell.spellDelay = 50.0f;
            spell.speedArray = { 30.0f, 35.0f, 40.0f, 45.0f, 50.0f };
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Singed ===
        {
            EvadeSpellData spell;
            spell.charName = "Singed";
            spell.dangerlevel = 2;
            spell.name = "Insanity Potion";
            spell.spellName = "SingedR";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 20.0f, 30.0f, 40.0f, 50.0f, 60.0f };
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Sivir ===
        {
            EvadeSpellData spell;
            spell.charName = "Sivir";
            spell.dangerlevel = 2;
            spell.name = "SivirE";
            spell.spellName = "SivirE";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::SpellShield;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Smolder ===
        {
            EvadeSpellData spell;
            spell.charName = "Smolder";
            spell.dangerlevel = 3;
            spell.name = "Flap, Flap, Flap";
            spell.spellName = "SmolderE";
            spell.spellDelay = 50.0f;
            spell.range = 350.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Sona ===
        {
            EvadeSpellData spell;
            spell.charName = "Sona";
            spell.dangerlevel = 3;
            spell.name = "Song of Celerity";
            spell.spellName = "SonaE";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 13.0f, 14.0f, 15.0f, 16.0f, 25.0f };
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Sona";
            spell.dangerlevel = 2;
            spell.name = "Song of Celerity";
            spell.spellName = "SonaE";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 13.0f, 14.0f, 15.0f, 16.0f, 17.0f };
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Sylas ===
        {
            EvadeSpellData spell;
            spell.charName = "Sylas";
            spell.dangerlevel = 4;
            spell.name = "Abduct";
            spell.spellName = "SylasE";
            spell.spellDelay = 50.0f;
            spell.range = 750.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === TahmKench ===
        {
            EvadeSpellData spell;
            spell.charName = "TahmKench";
            spell.dangerlevel = 4;
            spell.name = "Abyssal Dive";
            spell.spellName = "TahmKenchR";
            spell.spellDelay = 50.0f;
            spell.range = 250.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Talon ===
        {
            EvadeSpellData spell;
            spell.charName = "Talon";
            spell.dangerlevel = 4;
            spell.name = "Shadow Assualt";
            spell.spellName = "TalonShadowAssault";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 40.0f, 40.0f, 40.0f, 40.0f, 40.0f };
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Teemo ===
        {
            EvadeSpellData spell;
            spell.charName = "Teemo";
            spell.dangerlevel = 3;
            spell.name = "Move Quick";
            spell.spellName = "MoveQuick";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 10.0f, 14.0f, 18.0f, 22.0f, 26.0f };
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Tristana ===
        {
            EvadeSpellData spell;
            spell.charName = "Tristana";
            spell.dangerlevel = 3;
            spell.name = "RocketJump";
            spell.spellName = "RocketJump";
            spell.spellDelay = 500.0f;
            spell.range = 900.0f;
            spell.speed = 1100.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Tryndamere ===
        {
            EvadeSpellData spell;
            spell.charName = "Tryndamere";
            spell.dangerlevel = 3;
            spell.name = "SpinningSlash";
            spell.spellName = "Slash";
            spell.spellDelay = 50.0f;
            spell.range = 660.0f;
            spell.speed = 900.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Udyr ===
        {
            EvadeSpellData spell;
            spell.charName = "Udyr";
            spell.dangerlevel = 3;
            spell.name = "Bear Stance";
            spell.spellName = "UdyrBearStance";
            spell.spellDelay = 50.0f;
            spell.speedArray = { 15.0f, 20.0f, 25.0f, 30.0f, 35.0f };
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Udyr";
            spell.dangerlevel = 3;
            spell.name = "Wingborne Storm";
            spell.spellName = "UdyrE";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 20.0f, 25.0f, 30.0f, 35.0f, 40.0f };
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Vayne ===
        {
            EvadeSpellData spell;
            spell.charName = "Vayne";
            spell.dangerlevel = 1;
            spell.name = "Tumble";
            spell.spellName = "VayneTumble";
            spell.spellDelay = 50.0f;
            spell.range = 300.0f;
            spell.speed = 900.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Vi ===
        {
            EvadeSpellData spell;
            spell.charName = "Vi";
            spell.dangerlevel = 4;
            spell.name = "Vault Breaker";
            spell.spellName = "ViQ";
            spell.spellDelay = 50.0f;
            spell.range = 750.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Vi";
            spell.dangerlevel = 5;
            spell.name = "Assault and Battery";
            spell.spellName = "ViR";
            spell.spellDelay = 50.0f;
            spell.range = 800.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Viego ===
        {
            EvadeSpellData spell;
            spell.charName = "Viego";
            spell.dangerlevel = 4;
            spell.name = "Spectral Maw";
            spell.spellName = "ViegoW";
            spell.spellDelay = 50.0f;
            spell.range = 300.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Viego";
            spell.dangerlevel = 5;
            spell.name = "Harrowed Path";
            spell.spellName = "ViegoR";
            spell.spellDelay = 50.0f;
            spell.range = 600.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Volibear ===
        {
            EvadeSpellData spell;
            spell.charName = "Volibear";
            spell.dangerlevel = 3;
            spell.name = "Thundering Smite";
            spell.spellName = "VolibearQ";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 10.0f, 15.0f, 20.0f, 25.0f, 30.0f };
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Volibear";
            spell.dangerlevel = 5;
            spell.name = "Stormbringer";
            spell.spellName = "VolibearR";
            spell.spellDelay = 50.0f;
            spell.range = 700.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Xayah ===
        {
            EvadeSpellData spell;
            spell.charName = "Xayah";
            spell.dangerlevel = 5;
            spell.name = "Featherstorm";
            spell.spellName = "XayahR";
            spell.spellDelay = 50.0f;
            spell.range = 600.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === XinZhao ===
        {
            EvadeSpellData spell;
            spell.charName = "XinZhao";
            spell.dangerlevel = 3;
            spell.name = "Audacious Charge";
            spell.spellName = "XinZhaoE";
            spell.spellDelay = 50.0f;
            spell.range = 600.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Yasuo ===
        {
            EvadeSpellData spell;
            spell.charName = "Yasuo";
            spell.dangerlevel = 2;
            spell.name = "SweepingBlade";
            spell.spellName = "YasuoDashWrapper";
            spell.spellDelay = 50.0f;
            spell.range = 475.0f;
            spell.speed = 1000.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Yasuo";
            spell.dangerlevel = 3;
            spell.name = "WindWall";
            spell.spellName = "YasuoWMovingWall";
            spell.spellDelay = 250.0f;
            spell.range = 400.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::WindWall;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Yone ===
        {
            EvadeSpellData spell;
            spell.charName = "Yone";
            spell.dangerlevel = 4;
            spell.name = "Soul Unbound";
            spell.spellName = "YoneE";
            spell.spellDelay = 50.0f;
            spell.range = 300.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Yone";
            spell.dangerlevel = 5;
            spell.name = "Fate Sealed";
            spell.spellName = "YoneR";
            spell.spellDelay = 50.0f;
            spell.range = 1000.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // === Yuumi ===
        {
            EvadeSpellData spell;
            spell.charName = "Yuumi";
            spell.dangerlevel = 3;
            spell.name = "You and Me!";
            spell.spellName = "YuumiW";
            spell.spellDelay = 50.0f;
            spell.range = 1000.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // === Zilean ===
        {
            EvadeSpellData spell;
            spell.charName = "Zilean";
            spell.dangerlevel = 3;
            spell.name = "Timewarp";
            spell.spellName = "ZileanE";
            spell.spellDelay = 250.0f;
            spell.speedArray = { 40.0f, 55.0f, 70.0f, 85.0f, 99.0f };
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // === Zoe ===
        {
            EvadeSpellData spell;
            spell.charName = "Zoe";
            spell.dangerlevel = 4;
            spell.name = "Portal Jump";
            spell.spellName = "ZoeR";
            spell.spellDelay = 50.0f;
            spell.range = 575.0f;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

    }
};

inline std::vector<EvadeSpellData> EvadeSpellDatabase::Spells;
} // namespace ZDEvade
