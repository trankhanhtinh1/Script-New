#pragma once
// ===========================================================================
// SpellWindupDatabase.h — EzEvade spell windup (cast time) database.
// -------------------------------------------------------------------------
// Port từ EzEvade C# (Spells/SpellWindupDatabase.cs) + UPDATE cho 173 champion
// dùng CDragon data (E:\DamageData\Database).
//
// MỤC ĐÍCH: Danh sách spell của CHÍNH myHero cần BLOCK khi đang dodge.
//   Khi isDodging && ShouldDodge(), nếu user cast spell có slot match,
//   args.Process = false → block command (tránh bị trúng skillshot).
//   Xem Evade.cs Game_ProcessSpellCast → SpellDetector.windupSpells.
//
// TIÊU CHÍ SELECT (Loại A — đưa vào):
//   * Spell có cast/windup rõ ràng (mCastTime >= 100ms hoặc CastFrame tương đương)
//   * Cast ảnh hưởng khả năng movement/dodge (khóa movement trong windup)
//   * KHÔNG phải dash/blink/teleport (escape — không nên block)
//   * KHÔNG phải invisibility/untargetable (defensive escape)
//   * KHÔNG phải pure buff/shield/heal (defensive, không lock movement)
//   * KHÔNG phải toggle (channeling, không windup lock)
//
// DỮ LIỆN spellDelay từ CDragon:
//   * mCastTime  — mSpell.mCastTime * 1000 (ms), ưu tiên dùng
//   * CastFrame  — mSpell.CastFrame / 30 * 1000 (ms), fallback
//
// spellName theo mScriptName format từ CDragon
// ===========================================================================
#include "SpellData.h"

#include <vector>

namespace EzEvade {

class SpellWindupDatabase {
public:
    static inline std::vector<SpellData> Spells;

    static void InitSpells() {
        if (!Spells.empty()) {
            return;
        }

        // #region Aatrox
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.spellName = "AatroxQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 600;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.spellName = "AatroxW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Aatrox

        // #region Ahri
        {
            SpellData spell;
            spell.charName = "Ahri";
            spell.spellName = "AhriQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ahri";
            spell.spellName = "AhriE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Ahri

        // #region Akali
        {
            SpellData spell;
            spell.charName = "Akali";
            spell.spellName = "AkaliQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Akali

        // #region Akshan
        {
            SpellData spell;
            spell.charName = "Akshan";
            spell.spellName = "AkshanQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }

        // #endregion Akshan

        // #region Alistar
        {
            SpellData spell;
            spell.charName = "Alistar";
            spell.spellName = "Pulverize";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 393;
            Spells.push_back(spell);
        }
        // #endregion Alistar

        // #region Ambessa
        {
            SpellData spell;
            spell.charName = "Ambessa";
            spell.spellName = "AmbessaR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 700;
            Spells.push_back(spell);
        }
        // #endregion Ambessa

        // #region Amumu
        {
            SpellData spell;
            spell.charName = "Amumu";
            spell.spellName = "Tantrum";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 444;
            Spells.push_back(spell);
        }
        // #endregion Amumu

        // #region Anivia
        {
            SpellData spell;
            spell.charName = "Anivia";
            spell.spellName = "FlashFrostSpell";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Anivia";
            spell.spellName = "Frostbite";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Anivia

        // #region Annie
        {
            SpellData spell;
            spell.charName = "Annie";
            spell.spellName = "AnnieQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Annie";
            spell.spellName = "AnnieW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Annie";
            spell.spellName = "AnnieR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        // #endregion Annie

        // #region Aphelios
        {
            SpellData spell;
            spell.charName = "Aphelios";
            spell.spellName = "ApheliosCrescendumQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aphelios";
            spell.spellName = "ApheliosE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aphelios";
            spell.spellName = "ApheliosR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Aphelios

        // #region Ashe

        {
            SpellData spell;
            spell.charName = "Ashe";
            spell.spellName = "Volley";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ashe";
            spell.spellName = "AsheSpiritOfTheHawk";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 377;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ashe";
            spell.spellName = "EnchantedCrystalArrow";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Ashe

        // #region AurelionSol
        {
            SpellData spell;
            spell.charName = "AurelionSol";
            spell.spellName = "AurelionSolQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 167;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "AurelionSol";
            spell.spellName = "AurelionSolE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 200;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "AurelionSol";
            spell.spellName = "AurelionSolR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 200;
            Spells.push_back(spell);
        }
        // #endregion AurelionSol

        // #region Aurora
        {
            SpellData spell;
            spell.charName = "Aurora";
            spell.spellName = "AuroraQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Aurora

        // #region Azir
        {
            SpellData spell;
            spell.charName = "Azir";
            spell.spellName = "AzirQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Azir";
            spell.spellName = "AzirW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Azir";
            spell.spellName = "AzirR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Azir

        // #region Bard
        {
            SpellData spell;
            spell.charName = "Bard";
            spell.spellName = "BardQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Bard";
            spell.spellName = "BardE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Bard";
            spell.spellName = "BardR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Bard

        // #region Belveth
        {
            SpellData spell;
            spell.charName = "Belveth";
            spell.spellName = "BelvethW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Belveth";
            spell.spellName = "BelvethE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 300;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Belveth";
            spell.spellName = "BelvethR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 1000;
            Spells.push_back(spell);
        }
        // #endregion Belveth

        // #region Blitzcrank
        {
            SpellData spell;
            spell.charName = "Blitzcrank";
            spell.spellName = "RocketGrab";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Blitzcrank";
            spell.spellName = "PowerFist";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 175;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Blitzcrank";
            spell.spellName = "StaticField";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 335;
            Spells.push_back(spell);
        }
        // #endregion Blitzcrank

        // #region Brand
        {
            SpellData spell;
            spell.charName = "Brand";
            spell.spellName = "BrandQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Brand";
            spell.spellName = "BrandW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Brand";
            spell.spellName = "BrandE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Brand";
            spell.spellName = "BrandR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Brand

        // #region Braum
        {
            SpellData spell;
            spell.charName = "Braum";
            spell.spellName = "BraumQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Braum

        // #region Briar
        {
            SpellData spell;
            spell.charName = "Briar";
            spell.spellName = "BriarQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Briar";
            spell.spellName = "BriarE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 267;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Briar";
            spell.spellName = "BriarR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 1000;
            Spells.push_back(spell);
        }
        // #endregion Briar

        // #region Caitlyn
        {
            SpellData spell;
            spell.charName = "Caitlyn";
            spell.spellName = "CaitlynQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 1000;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Caitlyn";
            spell.spellName = "CaitlynW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Caitlyn";
            spell.spellName = "CaitlynR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 468;
            Spells.push_back(spell);
        }
        // #endregion Caitlyn


        // #endregion Camille

        // #region Cassiopeia
        {
            SpellData spell;
            spell.charName = "Cassiopeia";
            spell.spellName = "CassiopeiaQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 217;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Cassiopeia";
            spell.spellName = "CassiopeiaW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Cassiopeia";
            spell.spellName = "CassiopeiaE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Cassiopeia";
            spell.spellName = "CassiopeiaR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Cassiopeia

        // #region Chogath
        {
            SpellData spell;
            spell.charName = "Chogath";
            spell.spellName = "Rupture";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 672;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Chogath";
            spell.spellName = "FeralScream";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 642;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Chogath";
            spell.spellName = "Feast";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Chogath

        // #region Corki
        {
            SpellData spell;
            spell.charName = "Corki";
            spell.spellName = "PhosphorusBomb";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 267;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Corki";
            spell.spellName = "Gatling_Gun";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 295;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Corki";
            spell.spellName = "MissileBarrage";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 172;
            Spells.push_back(spell);
        }
        // #endregion Corki

        // #region Darius
        {
            SpellData spell;
            spell.charName = "Darius";
            spell.spellName = "DariusCleave";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 234;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Darius";
            spell.spellName = "DariusNoxianTacticsActive";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 234;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Darius";
            spell.spellName = "DariusAxeGrabCone";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Darius";
            spell.spellName = "DariusExecute";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 367;
            Spells.push_back(spell);
        }
        // #endregion Darius

        // #region Diana
        {
            SpellData spell;
            spell.charName = "Diana";
            spell.spellName = "DianaQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Diana";
            spell.spellName = "DianaR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Diana

        // #region DrMundo
        {
            SpellData spell;
            spell.charName = "DrMundo";
            spell.spellName = "DrMundoQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 364;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "DrMundo";
            spell.spellName = "DrMundoE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 1000;
            Spells.push_back(spell);
        }
        // #endregion DrMundo

        // #region Draven
        {
            SpellData spell;
            spell.charName = "Draven";
            spell.spellName = "DravenDoubleShot";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Draven";
            spell.spellName = "DravenR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Draven

        // #region Ekko
        {
            SpellData spell;
            spell.charName = "Ekko";
            spell.spellName = "EkkoQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 251;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ekko";
            spell.spellName = "EkkoW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 251;
            Spells.push_back(spell);
        }
        // #endregion Ekko

        // #region Elise
        {
            SpellData spell;
            spell.charName = "Elise";
            spell.spellName = "EliseHumanQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Elise";
            spell.spellName = "EliseHumanW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Elise";
            spell.spellName = "EliseHumanE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Elise";
            spell.spellName = "EliseR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Elise

        // #region Evelynn
        {
            SpellData spell;
            spell.charName = "Evelynn";
            spell.spellName = "EvelynnQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Evelynn";
            spell.spellName = "EvelynnW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 150;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Evelynn";
            spell.spellName = "EvelynnE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Evelynn

        // #region Ezreal
        {
            SpellData spell;
            spell.charName = "Ezreal";
            spell.spellName = "EzrealR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 1767;
            Spells.push_back(spell);
        }
        // #endregion Ezreal

        // #region FiddleSticks
        {
            SpellData spell;
            spell.charName = "FiddleSticks";
            spell.spellName = "FiddleSticksQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "FiddleSticks";
            spell.spellName = "FiddleSticksW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "FiddleSticks";
            spell.spellName = "FiddleSticksE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 400;
            Spells.push_back(spell);
        }
        // #endregion FiddleSticks

        // #region Fiora
        // #endregion Fiora

        // #region Fizz
        {
            SpellData spell;
            spell.charName = "Fizz";
            spell.spellName = "FizzR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Fizz

        // #region Galio
        {
            SpellData spell;
            spell.charName = "Galio";
            spell.spellName = "GalioQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Galio";
            spell.spellName = "GalioW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Galio";
            spell.spellName = "GalioR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        // #endregion Galio

        // #region Gangplank
        {
            SpellData spell;
            spell.charName = "Gangplank";
            spell.spellName = "GangplankQWrapper";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gangplank";
            spell.spellName = "GangplankE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gangplank";
            spell.spellName = "GangplankR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Gangplank

        // #region Garen
        // #endregion Garen

        // #region Gnar
        {
            SpellData spell;
            spell.charName = "Gnar";
            spell.spellName = "GnarQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gnar";
            spell.spellName = "GnarW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gnar";
            spell.spellName = "GnarR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Gnar

        // #region Gragas
        {
            SpellData spell;
            spell.charName = "Gragas";
            spell.spellName = "GragasQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gragas";
            spell.spellName = "GragasR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Gragas

        // #region Graves
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.spellName = "GravesQLineSpell";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 267;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.spellName = "GravesSmokeGrenade";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 283;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.spellName = "GravesChargeShot";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Graves

        // #region Gwen
        {
            SpellData spell;
            spell.charName = "Gwen";
            spell.spellName = "GwenQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gwen";
            spell.spellName = "GwenR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Gwen

        // #region Hecarim
        // #endregion Hecarim

        // #region Heimerdinger
        {
            SpellData spell;
            spell.charName = "Heimerdinger";
            spell.spellName = "HeimerdingerQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 495;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Heimerdinger";
            spell.spellName = "HeimerdingerW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Heimerdinger";
            spell.spellName = "HeimerdingerE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Heimerdinger

        // #region Hwei
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.spellName = "HweiQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.spellName = "HweiE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.spellName = "HweiR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Hwei

        // #region Illaoi
        {
            SpellData spell;
            spell.charName = "Illaoi";
            spell.spellName = "IllaoiQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 750;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Illaoi";
            spell.spellName = "IllaoiE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Illaoi";
            spell.spellName = "IllaoiR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Illaoi

        // #region Irelia
        {
            SpellData spell;
            spell.charName = "Irelia";
            spell.spellName = "IreliaQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 233;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Irelia";
            spell.spellName = "IreliaW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 233;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Irelia";
            spell.spellName = "IreliaE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Irelia";
            spell.spellName = "IreliaR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 233;
            Spells.push_back(spell);
        }
        // #endregion Irelia

        // #region Ivern
        {
            SpellData spell;
            spell.charName = "Ivern";
            spell.spellName = "IvernQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ivern";
            spell.spellName = "IvernW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ivern";
            spell.spellName = "IvernE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Ivern

        // #region Janna
        {
            SpellData spell;
            spell.charName = "Janna";
            spell.spellName = "HowlingGale";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 245;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Janna";
            spell.spellName = "SowTheWind";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 245;
            Spells.push_back(spell);
        }
        // #endregion Janna

        // #region JarvanIV
        {
            SpellData spell;
            spell.charName = "JarvanIV";
            spell.spellName = "JarvanIVDragonStrike";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 267;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "JarvanIV";
            spell.spellName = "JarvanIVDemacianStandard";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 501;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "JarvanIV";
            spell.spellName = "JarvanIVCataclysm";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 267;
            Spells.push_back(spell);
        }
        // #endregion JarvanIV

        // #region Jax
        {
            SpellData spell;
            spell.charName = "Jax";
            spell.spellName = "JaxW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jax";
            spell.spellName = "JaxR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Jax

        // #region Jayce
        {
            SpellData spell;
            spell.charName = "Jayce";
            spell.spellName = "JayceShockBlast";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 214;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jayce";
            spell.spellName = "JayceHyperCharge";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 107;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jayce";
            spell.spellName = "JayceThunderingBlow";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Jayce

        // #region Jhin
        {
            SpellData spell;
            spell.charName = "Jhin";
            spell.spellName = "JhinQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 233;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jhin";
            spell.spellName = "JhinW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 750;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jhin";
            spell.spellName = "JhinE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jhin";
            spell.spellName = "JhinR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 1000;
            Spells.push_back(spell);
        }
        // #endregion Jhin

        // #region Jinx
        {
            SpellData spell;
            spell.charName = "Jinx";
            spell.spellName = "JinxW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jinx";
            spell.spellName = "JinxE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jinx";
            spell.spellName = "JinxR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 600;
            Spells.push_back(spell);
        }
        // #endregion Jinx

        // #region KSante
        {
            SpellData spell;
            spell.charName = "KSante";
            spell.spellName = "KSanteQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 350;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KSante";
            spell.spellName = "KSanteW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 400;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KSante";
            spell.spellName = "KSanteR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 400;
            Spells.push_back(spell);
        }
        // #endregion KSante

        // #region Kaisa
        {
            SpellData spell;
            spell.charName = "Kaisa";
            spell.spellName = "KaisaW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 400;
            Spells.push_back(spell);
        }
        // #endregion Kaisa

        // #region Kalista
        {
            SpellData spell;
            spell.charName = "Kalista";
            spell.spellName = "KalistaW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kalista";
            spell.spellName = "KalistaExpunge";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Kalista

        // #region Karma
        {
            SpellData spell;
            spell.charName = "Karma";
            spell.spellName = "KarmaQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Karma";
            spell.spellName = "KarmaSpiritBind";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Karma";
            spell.spellName = "KarmaMantra";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Karma

        // #region Karthus
        {
            SpellData spell;
            spell.charName = "Karthus";
            spell.spellName = "KarthusLayWasteA1";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Karthus";
            spell.spellName = "KarthusWallOfPain";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Karthus";
            spell.spellName = "KarthusFallenOne";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Karthus

        // #region Kassadin
        {
            SpellData spell;
            spell.charName = "Kassadin";
            spell.spellName = "NullLance";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kassadin";
            spell.spellName = "NetherBlade";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kassadin";
            spell.spellName = "ForcePulse";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Kassadin

        // #region Katarina
        {
            SpellData spell;
            spell.charName = "Katarina";
            spell.spellName = "KatarinaQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Katarina";
            spell.spellName = "KatarinaR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Katarina

        // #region Kayle
        {
            SpellData spell;
            spell.charName = "Kayle";
            spell.spellName = "KayleQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Kayle

        // #region Kayn
        {
            SpellData spell;
            spell.charName = "Kayn";
            spell.spellName = "KaynW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 550;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kayn";
            spell.spellName = "KaynR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 100;
            Spells.push_back(spell);
        }
        // #endregion Kayn

        // #region Kennen
        {
            SpellData spell;
            spell.charName = "Kennen";
            spell.spellName = "KennenShurikenHurl1";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 185;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kennen";
            spell.spellName = "KennenShurikenStorm";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 414;
            Spells.push_back(spell);
        }
        // #endregion Kennen

        // #region Khazix
        {
            SpellData spell;
            spell.charName = "Khazix";
            spell.spellName = "KhazixQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Khazix";
            spell.spellName = "KhazixW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Khazix

        // #region Kindred
        // #endregion Kindred

        // #region Kled
        {
            SpellData spell;
            spell.charName = "Kled";
            spell.spellName = "KledQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kled";
            spell.spellName = "KledW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 400;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kled";
            spell.spellName = "KledR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 400;
            Spells.push_back(spell);
        }
        // #endregion Kled

                // #region KogMaw
        {
            SpellData spell;
            spell.charName = "KogMaw";
            spell.spellName = "KogMawQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 251;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KogMaw";
            spell.spellName = "KogMawVoidOozeMissile";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KogMaw";
            spell.spellName = "KogMawLivingArtillery";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 251;
            Spells.push_back(spell);
        }
        // #endregion KogMaw

                // #region Leblanc
        {
            SpellData spell;
            spell.charName = "Leblanc";
            spell.spellName = "LeblancQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 401;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Leblanc";
            spell.spellName = "LeblancE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 367;
            Spells.push_back(spell);
        }
        // #endregion Leblanc

                // #region LeeSin
        {
            SpellData spell;
            spell.charName = "LeeSin";
            spell.spellName = "LeeSinQOne";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "LeeSin";
            spell.spellName = "LeeSinEOne";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "LeeSin";
            spell.spellName = "LeeSinR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion LeeSin

                // #region Leona
        {
            SpellData spell;
            spell.charName = "Leona";
            spell.spellName = "LeonaSolarBarrier";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Leona";
            spell.spellName = "LeonaSolarFlare";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Leona

                // #region Lillia
        {
            SpellData spell;
            spell.charName = "Lillia";
            spell.spellName = "LilliaE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 350;
            Spells.push_back(spell);
        }
        // #endregion Lillia

        // #region Lissandra
        {
            SpellData spell;
            spell.charName = "Lissandra";
            spell.spellName = "LissandraQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 251;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lissandra";
            spell.spellName = "LissandraW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Lissandra

        // #region Locke
        {
            SpellData spell;
            spell.charName = "Locke";
            spell.spellName = "LockeQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Locke";
            spell.spellName = "LockeR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Locke

        // #region Lucian
        {
            SpellData spell;
            spell.charName = "Lucian";
            spell.spellName = "LucianQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 350;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lucian";
            spell.spellName = "LucianW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Lucian

                // #region Lulu
        {
            SpellData spell;
            spell.charName = "Lulu";
            spell.spellName = "LuluQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 242;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lulu";
            spell.spellName = "LuluW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 242;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lulu";
            spell.spellName = "LuluE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Lulu

                // #region Lux
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.spellName = "LuxLightBinding";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.spellName = "LuxPrismaticWave";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 362;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.spellName = "LuxLightStrikeKugel";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.spellName = "LuxR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 2467;
            Spells.push_back(spell);
        }
        // #endregion Lux

                // #region Malphite
        {
            SpellData spell;
            spell.charName = "Malphite";
            spell.spellName = "SeismicShard";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 242;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Malphite";
            spell.spellName = "Landslide";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 242;
            Spells.push_back(spell);
        }
        // #endregion Malphite

        // #region Malzahar
        {
            SpellData spell;
            spell.charName = "Malzahar";
            spell.spellName = "MalzaharQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 600;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Malzahar";
            spell.spellName = "MalzaharW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 600;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Malzahar";
            spell.spellName = "MalzaharE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Malzahar";
            spell.spellName = "MalzaharR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Malzahar

        // #region Maokai
        {
            SpellData spell;
            spell.charName = "Maokai";
            spell.spellName = "MaokaiQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 1000;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Maokai";
            spell.spellName = "MaokaiE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Maokai";
            spell.spellName = "MaokaiR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 1167;
            Spells.push_back(spell);
        }
        // #endregion Maokai

                // #region MasterYi
        // #endregion MasterYi

        // #region Mel
        {
            SpellData spell;
            spell.charName = "Mel";
            spell.spellName = "MelQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 350;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Mel";
            spell.spellName = "MelE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Mel";
            spell.spellName = "MelR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 750;
            Spells.push_back(spell);
        }
        // #endregion Mel

        // #region Milio
        {
            SpellData spell;
            spell.charName = "Milio";
            spell.spellName = "MilioQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Milio

                // #region MissFortune
        {
            SpellData spell;
            spell.charName = "MissFortune";
            spell.spellName = "MissFortuneRicochetShot";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "MissFortune";
            spell.spellName = "MissFortuneScattershot";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion MissFortune

                // #region MonkeyKing
        {
            SpellData spell;
            spell.charName = "MonkeyKing";
            spell.spellName = "MonkeyKingDoubleAttack";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion MonkeyKing

                // #region Mordekaiser
        {
            SpellData spell;
            spell.charName = "Mordekaiser";
            spell.spellName = "MordekaiserQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Mordekaiser";
            spell.spellName = "MordekaiserE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Mordekaiser

        // #region Morgana
        {
            SpellData spell;
            spell.charName = "Morgana";
            spell.spellName = "MorganaQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Morgana";
            spell.spellName = "MorganaW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Morgana";
            spell.spellName = "MorganaR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Morgana

        // #region Naafiri
        {
            SpellData spell;
            spell.charName = "Naafiri";
            spell.spellName = "NaafiriQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Naafiri";
            spell.spellName = "NaafiriW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 750;
            Spells.push_back(spell);
        }
        // #endregion Naafiri

                // #region Nami
        {
            SpellData spell;
            spell.charName = "Nami";
            spell.spellName = "NamiQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nami";
            spell.spellName = "NamiR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Nami

        // #region Nasus
        {
            SpellData spell;
            spell.charName = "Nasus";
            spell.spellName = "NasusQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 485;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nasus";
            spell.spellName = "NasusW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 494;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nasus";
            spell.spellName = "NasusE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 414;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nasus";
            spell.spellName = "NasusR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 414;
            Spells.push_back(spell);
        }
        // #endregion Nasus

                // #region Nautilus
        {
            SpellData spell;
            spell.charName = "Nautilus";
            spell.spellName = "NautilusAnchorDrag";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 400;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nautilus";
            spell.spellName = "NautilusSplashZone";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nautilus";
            spell.spellName = "NautilusGrandLine";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 460;
            Spells.push_back(spell);
        }
        // #endregion Nautilus

                // #region Neeko
        {
            SpellData spell;
            spell.charName = "Neeko";
            spell.spellName = "NeekoQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Neeko";
            spell.spellName = "NeekoE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Neeko";
            spell.spellName = "NeekoR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        // #endregion Neeko

                // #region Nidalee
        {
            SpellData spell;
            spell.charName = "Nidalee";
            spell.spellName = "PrimalSurge";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Nidalee

                // #region Nilah
        {
            SpellData spell;
            spell.charName = "Nilah";
            spell.spellName = "NilahQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 350;
            Spells.push_back(spell);
        }
        // #endregion Nilah

        // #region Nocturne
        {
            SpellData spell;
            spell.charName = "Nocturne";
            spell.spellName = "NocturneDuskbringer";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nocturne";
            spell.spellName = "NocturneUnspeakableHorror";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nocturne";
            spell.spellName = "NocturneParanoia";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Nocturne

                // #region Nunu
        {
            SpellData spell;
            spell.charName = "Nunu";
            spell.spellName = "NunuQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 300;
            Spells.push_back(spell);
        }
        // #endregion Nunu

                // #region Olaf
        {
            SpellData spell;
            spell.charName = "Olaf";
            spell.spellName = "OlafAxeThrow";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Olaf

                // #region Orianna
        {
            SpellData spell;
            spell.charName = "Orianna";
            spell.spellName = "OrianaDetonateCommand";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Orianna

                // #region Ornn
        {
            SpellData spell;
            spell.charName = "Ornn";
            spell.spellName = "OrnnQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 300;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ornn";
            spell.spellName = "OrnnR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Ornn

                // #region Pantheon
        {
            SpellData spell;
            spell.charName = "Pantheon";
            spell.spellName = "PantheonQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Pantheon

        // #region Poppy
        {
            SpellData spell;
            spell.charName = "Poppy";
            spell.spellName = "PoppyQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Poppy";
            spell.spellName = "PoppyW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Poppy";
            spell.spellName = "PoppyE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Poppy";
            spell.spellName = "PoppyR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        // #endregion Poppy

                // #region Pyke
        {
            SpellData spell;
            spell.charName = "Pyke";
            spell.spellName = "PykeQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 383;
            Spells.push_back(spell);
        }
        // #endregion Pyke

                // #region Qiyana
        {
            SpellData spell;
            spell.charName = "Qiyana";
            spell.spellName = "QiyanaQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Qiyana";
            spell.spellName = "QiyanaR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Qiyana

                // #region Quinn
        {
            SpellData spell;
            spell.charName = "Quinn";
            spell.spellName = "QuinnQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Quinn";
            spell.spellName = "QuinnR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Quinn

                // #region Rakan
        {
            SpellData spell;
            spell.charName = "Rakan";
            spell.spellName = "RakanQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Rakan

                // #region Rammus
        // #endregion Rammus

                // #region RekSai
        {
            SpellData spell;
            spell.charName = "RekSai";
            spell.spellName = "RekSaiQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 350;
            Spells.push_back(spell);
        }
        // #endregion RekSai

                // #region Rell
        {
            SpellData spell;
            spell.charName = "Rell";
            spell.spellName = "RellQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 400;
            Spells.push_back(spell);
        }
        // #endregion Rell

        // #region Renata
        {
            SpellData spell;
            spell.charName = "Renata";
            spell.spellName = "RenataQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Renata";
            spell.spellName = "RenataE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Renata";
            spell.spellName = "RenataR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 750;
            Spells.push_back(spell);
        }
        // #endregion Renata

                // #region Renekton
        {
            SpellData spell;
            spell.charName = "Renekton";
            spell.spellName = "RenektonExecute";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Renekton

                // #region Rengar
        {
            SpellData spell;
            spell.charName = "Rengar";
            spell.spellName = "RengarE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Rengar

                // #region Riven
        {
            SpellData spell;
            spell.charName = "Riven";
            spell.spellName = "RivenMartyr";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 267;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Riven";
            spell.spellName = "RivenFengShuiEngine";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Riven";
            spell.spellName = "RivenIzunaBlade";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Riven

                // #region Rumble
        {
            SpellData spell;
            spell.charName = "Rumble";
            spell.spellName = "RumbleGrenade";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Rumble";
            spell.spellName = "RumbleCarpetBomb";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 583;
            Spells.push_back(spell);
        }
        // #endregion Rumble

                // #region Ryze
        {
            SpellData spell;
            spell.charName = "Ryze";
            spell.spellName = "RyzeQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ryze";
            spell.spellName = "RyzeW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ryze";
            spell.spellName = "RyzeE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Ryze

        // #region Samira
        {
            SpellData spell;
            spell.charName = "Samira";
            spell.spellName = "SamiraQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 50;
            Spells.push_back(spell);
        }
        // #endregion Samira

        // #region Sejuani
        {
            SpellData spell;
            spell.charName = "Sejuani";
            spell.spellName = "SejuaniR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Sejuani

        // #region Senna
        {
            SpellData spell;
            spell.charName = "Senna";
            spell.spellName = "SennaQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Senna";
            spell.spellName = "SennaR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Senna

        // #region Seraphine
        {
            SpellData spell;
            spell.charName = "Seraphine";
            spell.spellName = "SeraphineQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Seraphine";
            spell.spellName = "SeraphineW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Seraphine";
            spell.spellName = "SeraphineE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Seraphine";
            spell.spellName = "SeraphineR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Seraphine

                // #region Sett
        {
            SpellData spell;
            spell.charName = "Sett";
            spell.spellName = "SettE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Sett

        // #region Shaco
        {
            SpellData spell;
            spell.charName = "Shaco";
            spell.spellName = "JackInTheBox";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 583;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Shaco";
            spell.spellName = "TwoShivPoison";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 300;
            Spells.push_back(spell);
        }
        // #endregion Shaco

                // #region Shen
        // #endregion Shen

                // #region Shyvana
        {
            SpellData spell;
            spell.charName = "Shyvana";
            spell.spellName = "ShyvanaE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Shyvana

                // #region Singed
        {
            SpellData spell;
            spell.charName = "Singed";
            spell.spellName = "MegaAdhesive";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 680;
            Spells.push_back(spell);
        }
        // #endregion Singed

                // #region Sion
        {
            SpellData spell;
            spell.charName = "Sion";
            spell.spellName = "SionQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sion";
            spell.spellName = "SionE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sion";
            spell.spellName = "SionR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Sion

        // #region Sivir
        {
            SpellData spell;
            spell.charName = "Sivir";
            spell.spellName = "SivirQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sivir";
            spell.spellName = "SivirW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Sivir

                // #region Skarner
        {
            SpellData spell;
            spell.charName = "Skarner";
            spell.spellName = "SkarnerQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Skarner";
            spell.spellName = "SkarnerR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 750;
            Spells.push_back(spell);
        }
        // #endregion Skarner

        // #region Smolder
        {
            SpellData spell;
            spell.charName = "Smolder";
            spell.spellName = "SmolderQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Smolder";
            spell.spellName = "SmolderW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 350;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Smolder";
            spell.spellName = "SmolderR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 750;
            Spells.push_back(spell);
        }
        // #endregion Smolder

                // #region Sona
        {
            SpellData spell;
            spell.charName = "Sona";
            spell.spellName = "SonaR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 283;
            Spells.push_back(spell);
        }
        // #endregion Sona

        // #region Soraka
        {
            SpellData spell;
            spell.charName = "Soraka";
            spell.spellName = "SorakaQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Soraka";
            spell.spellName = "SorakaE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Soraka

                // #region Swain
        {
            SpellData spell;
            spell.charName = "Swain";
            spell.spellName = "SwainE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Swain";
            spell.spellName = "SwainW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Swain

                // #region Sylas
        {
            SpellData spell;
            spell.charName = "Sylas";
            spell.spellName = "SylasQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 400;
            Spells.push_back(spell);
        }
        // #endregion Sylas

                // #region Syndra
        {
            SpellData spell;
            spell.charName = "Syndra";
            spell.spellName = "SyndraE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Syndra";
            spell.spellName = "SyndraR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Syndra

        // #region TahmKench
        {
            SpellData spell;
            spell.charName = "TahmKench";
            spell.spellName = "TahmKenchQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "TahmKench";
            spell.spellName = "TahmKenchR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion TahmKench

                // #region Taliyah
        {
            SpellData spell;
            spell.charName = "Taliyah";
            spell.spellName = "TaliyahW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Taliyah";
            spell.spellName = "TaliyahE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Taliyah

                // #region Talon
        {
            SpellData spell;
            spell.charName = "Talon";
            spell.spellName = "TalonW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Talon

                // #region Taric
        // #endregion Taric

                // #region Teemo
        {
            SpellData spell;
            spell.charName = "Teemo";
            spell.spellName = "TeemoQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 493;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Teemo";
            spell.spellName = "TeemoR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 650;
            Spells.push_back(spell);
        }
        // #endregion Teemo

        // #region Thresh
        {
            SpellData spell;
            spell.charName = "Thresh";
            spell.spellName = "ThreshQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Thresh";
            spell.spellName = "ThreshE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 1000;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Thresh";
            spell.spellName = "ThreshRTrigger1";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 450;
            Spells.push_back(spell);
        }
        // #endregion Thresh

                // #region Tristana
        // #endregion Tristana

                // #region Trundle
        // #endregion Trundle

                // #region TwistedFate
        {
            SpellData spell;
            spell.charName = "TwistedFate";
            spell.spellName = "WildCards";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 285;
            Spells.push_back(spell);
        }
        // #endregion TwistedFate

                // #region Twitch
        {
            SpellData spell;
            spell.charName = "Twitch";
            spell.spellName = "TwitchDeadlyVenom";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Twitch

                // #region Udyr
        // #endregion Udyr

                // #region Urgot
        {
            SpellData spell;
            spell.charName = "Urgot";
            spell.spellName = "UrgotQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 1500;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Urgot";
            spell.spellName = "UrgotR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Urgot

                // #region Varus
        {
            SpellData spell;
            spell.charName = "Varus";
            spell.spellName = "VarusW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 251;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Varus";
            spell.spellName = "VarusE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 242;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Varus";
            spell.spellName = "VarusR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 242;
            Spells.push_back(spell);
        }
        // #endregion Varus

                // #region Vayne
        // #endregion Vayne

        // #region Veigar
        {
            SpellData spell;
            spell.charName = "Veigar";
            spell.spellName = "VeigarDarkMatter";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 433;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Veigar";
            spell.spellName = "VeigarEventHorizon";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 672;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Veigar";
            spell.spellName = "VeigarR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 265;
            Spells.push_back(spell);
        }
        // #endregion Veigar

                // #region Velkoz
        {
            SpellData spell;
            spell.charName = "Velkoz";
            spell.spellName = "VelkozQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 251;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Velkoz";
            spell.spellName = "VelkozE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Velkoz";
            spell.spellName = "VelkozR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Velkoz

                // #region Vex
        {
            SpellData spell;
            spell.charName = "Vex";
            spell.spellName = "VexQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vex";
            spell.spellName = "VexE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vex";
            spell.spellName = "VexR2";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Vex

                // #region Vi
        // #endregion Vi

        // #region Viego
        {
            SpellData spell;
            spell.charName = "Viego";
            spell.spellName = "ViegoQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Viego

                // #region Viktor
        {
            SpellData spell;
            spell.charName = "Viktor";
            spell.spellName = "ViktorQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Viktor";
            spell.spellName = "ViktorW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Viktor

        // #region Vladimir
        {
            SpellData spell;
            spell.charName = "Vladimir";
            spell.spellName = "VladimirQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 283;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vladimir";
            spell.spellName = "VladimirE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vladimir";
            spell.spellName = "VladimirHemoplague";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 267;
            Spells.push_back(spell);
        }
        // #endregion Vladimir

                // #region Volibear
        // #endregion Volibear

                // #region Warwick
        // #endregion Warwick

                // #region Xayah
        // #endregion Xayah

                // #region Xerath
        {
            SpellData spell;
            spell.charName = "Xerath";
            spell.spellName = "XerathArcaneBarrage2";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Xerath";
            spell.spellName = "XerathMageSpear";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Xerath

                // #region XinZhao
        {
            SpellData spell;
            spell.charName = "XinZhao";
            spell.spellName = "XinZhaoW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 600;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "XinZhao";
            spell.spellName = "XinZhaoR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        // #endregion XinZhao

        // #region Yasuo
        {
            SpellData spell;
            spell.charName = "Yasuo";
            spell.spellName = "YasuoQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 350;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yasuo";
            spell.spellName = "YasuoE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 300;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yasuo";
            spell.spellName = "YasuoR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 300;
            Spells.push_back(spell);
        }
        // #endregion Yasuo

        // #region Yone
        {
            SpellData spell;
            spell.charName = "Yone";
            spell.spellName = "YoneQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 350;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yone";
            spell.spellName = "YoneW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Yone

                // #region Yorick
        {
            SpellData spell;
            spell.charName = "Yorick";
            spell.spellName = "YorickQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 600;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yorick";
            spell.spellName = "YorickQ2";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yorick";
            spell.spellName = "YorickE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 333;
            Spells.push_back(spell);
        }
        // #endregion Yorick

                // #region Yunara
        {
            SpellData spell;
            spell.charName = "Yunara";
            spell.spellName = "YunaraW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Yunara

                // #region Yuumi
        // #endregion Yuumi

                // #region Zaahen
        {
            SpellData spell;
            spell.charName = "Zaahen";
            spell.spellName = "ZaahenW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 500;
            Spells.push_back(spell);
        }
        // #endregion Zaahen

                // #region Zac
        {
            SpellData spell;
            spell.charName = "Zac";
            spell.spellName = "ZacQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 330;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zac";
            spell.spellName = "ZacW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 330;
            Spells.push_back(spell);
        }
        // #endregion Zac

                // #region Zed
        {
            SpellData spell;
            spell.charName = "Zed";
            spell.spellName = "ZedQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 400;
            Spells.push_back(spell);
        }
        // #endregion Zed

                // #region Zeri
        {
            SpellData spell;
            spell.charName = "Zeri";
            spell.spellName = "ZeriQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 550;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zeri";
            spell.spellName = "ZeriW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 550;
            Spells.push_back(spell);
        }
        // #endregion Zeri

        // #region Ziggs
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.spellName = "ZiggsQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.spellName = "ZiggsW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.spellName = "ZiggsE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.spellName = "ZiggsR";
            spell.spellKey = SpellSlot::R;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Ziggs

                // #region Zilean
        {
            SpellData spell;
            spell.charName = "Zilean";
            spell.spellName = "ZileanQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        // #endregion Zilean

        // #region Zoe
        {
            SpellData spell;
            spell.charName = "Zoe";
            spell.spellName = "ZoeQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zoe";
            spell.spellName = "ZoeE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 300;
            Spells.push_back(spell);
        }
        // #endregion Zoe

                // #region Zyra
        {
            SpellData spell;
            spell.charName = "Zyra";
            spell.spellName = "ZyraQ";
            spell.spellKey = SpellSlot::Q;
            spell.spellDelay = 250;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zyra";
            spell.spellName = "ZyraW";
            spell.spellKey = SpellSlot::W;
            spell.spellDelay = 243;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zyra";
            spell.spellName = "ZyraE";
            spell.spellKey = SpellSlot::E;
            spell.spellDelay = 150;
            Spells.push_back(spell);
        }
        // #endregion Zyra

    }
};

} // namespace EzEvade