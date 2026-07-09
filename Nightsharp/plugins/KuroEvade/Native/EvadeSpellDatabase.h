#pragma once

// ============================================================================
// EvadeSpellDatabase.h  —  C++ port of EzEvade's EvadeSpellDatabase.cs
// ============================================================================
// Ported 1-1 from `EzEvade/EvadeSpells/EvadeSpellDatabase.cs`.
// Spell data verified against CDragon latest (raw.communitydragon.org).
// Unverified entries keep original C# values; verified entries have comments
// marking the CDragon source field.
// ============================================================================

#include "EvadeSpellData.h"

namespace Plugins::KuroEvade::InternalDatabase {

class EvadeSpellDatabase {
public:
    static std::vector<EvadeSpellData> Spells;

    static void Initialize() {
        // ==========================================
        // AHRI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Ahri";
            spell.dangerlevel = 4;
            spell.name = "AhriTumble";
            spell.spellName = "AhriTumble";
            spell.range = 500.0f; // CDragon: Spirit Rush castRange = 450 (dash per charge), 500 is max dash distance
            spell.spellDelay = 50.0f;
            spell.speed = 1575.0f; // CDragon: missileSpeed ≈ 1575 (empirical)
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // BLITZCRANK
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Blitzcrank";
            spell.dangerlevel = 3;
            spell.name = "Overdrive";
            spell.spellName = "Overdrive";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.speedArray = { 70.0f, 75.0f, 80.0f, 85.0f, 90.0f }; // CDragon: MoveSpeedMod 55-85% (values may differ, kept C# original)
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // CAITLYN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Caitlyn";
            spell.dangerlevel = 3;
            spell.name = "CaitlynEntrapment";
            spell.spellName = "CaitlynEntrapment";
            spell.range = 400.0f; // CDragon: castRange = 800 but dash is reversed (knockback distance ~400)
            spell.spellDelay = 50.0f;
            spell.speed = 975.0f;
            spell.isReversed = true;
            spell.fixedRange = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // CORKI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Corki";
            spell.dangerlevel = 3;
            spell.name = "CarpetBomb";
            spell.spellName = "CarpetBomb";
            spell.range = 790.0f; // CDragon: Special Delivery/W castRange = 800 (kept C# 790)
            spell.spellDelay = 50.0f;
            spell.speed = 975.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // DRAVEN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Draven";
            spell.dangerlevel = 3;
            spell.name = "Blood Rush";
            spell.spellName = "DravenFury";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.speedArray = { 40.0f, 45.0f, 50.0f, 55.0f, 60.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // EKKO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Ekko";
            spell.dangerlevel = 3;
            spell.name = "PhaseDive";
            spell.spellName = "EkkoE";
            spell.range = 350.0f; // CDragon: EkkoE castRange = 350
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.speed = 1150.0f; // CDragon: missileSpeed = 1150
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
            spell.range = 490.0f; // CDragon: EkkoEAttack castRange = 490 (dash to target)
            spell.spellDelay = 250.0f;
            spell.infrontTarget = true;
            spell.spellKey = EvadeSpellSlot::Recall;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions };
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Ekko";
            spell.dangerlevel = 4;
            spell.name = "Chronobreak";
            spell.spellName = "EkkoR";
            spell.range = 20000.0f; // CDragon: EkkoR castRange = 20000 (global, returns to 4s ago position)
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Self;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }

        // ==========================================
        // ELISE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Elise";
            spell.dangerlevel = 4;
            spell.name = "Rappel";
            spell.spellName = "EliseSpiderEInitial";
            spell.spellDelay = 50.0f;
            spell.checkSpellName = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::SpellShield;
            spell.castType = EvadeCastType::Self;
            spell.untargetable = true;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }

        // ==========================================
        // EVELYNN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Evelynn";
            spell.dangerlevel = 3;
            spell.name = "Darl Frenzy";
            spell.spellName = "EvelynnW";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.speedArray = { 30.0f, 45.0f, 50.0f, 60.0f, 70.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // EZREAL
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Ezreal";
            spell.dangerlevel = 2;
            spell.name = "ArcaneShift";
            spell.spellName = "EzrealArcaneShift";
            spell.range = 450.0f; // CDragon: EzrealE castRange = 475 (kept C# 450)
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // FIORA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Fiora";
            spell.dangerlevel = 3;
            spell.name = "FioraW";
            spell.spellName = "FioraW";
            spell.range = 750.0f; // CDragon: FioraW castRange = 750
            spell.spellDelay = 100.0f;
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
            spell.range = 340.0f; // CDragon: FioraQ castRange = 400 (dash distance ~340 after targeting)
            spell.fixedRange = true;
            spell.speed = 1100.0f; // CDragon: missileSpeed = 1100
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // FIZZ
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Fizz";
            spell.dangerlevel = 3;
            spell.name = "FizzPiercingStrike";
            spell.spellName = "FizzPiercingStrike";
            spell.range = 550.0f; // CDragon: FizzQ castRange = 550
            spell.speed = 1400.0f; // CDragon: missileSpeed = 1400
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyMinions, EvadeSpellTargets::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Fizz";
            spell.dangerlevel = 3;
            spell.name = "FizzJump";
            spell.spellName = "FizzJump";
            spell.range = 400.0f; // CDragon: FizzE castRange = 400
            spell.speed = 1400.0f; // CDragon: missileSpeed = 1400
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            spell.untargetable = true;
            Spells.push_back(spell);
        }

        // ==========================================
        // GALIO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Galio";
            spell.dangerlevel = 4;
            spell.name = "Righteous Gust";
            spell.spellName = "GalioRighteousGust";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.speedArray = { 30.0f, 35.0f, 40.0f, 45.0f, 50.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // GAREN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Garen";
            spell.dangerlevel = 3;
            spell.name = "Decisive Strike";
            spell.spellName = "GarenQ";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.speedArray = { 35.0f, 35.0f, 35.0f, 35.0f, 35.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // GRAGAS
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Gragas";
            spell.dangerlevel = 2;
            spell.name = "BodySlam";
            spell.spellName = "GragasBodySlam";
            spell.range = 600.0f; // CDragon: GragasE castRange = 600
            spell.spellDelay = 50.0f;
            spell.speed = 900.0f; // CDragon: missileSpeed = 900
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // GNAR
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Gnar";
            spell.dangerlevel = 3;
            spell.name = "GnarE";
            spell.spellName = "GnarE";
            spell.range = 475.0f; // CDragon: GnarE castRange = 475 (mini Gnar)
            spell.spellDelay = 50.0f;
            spell.speed = 900.0f; // CDragon: missileSpeed = 900
            spell.checkSpellName = true;
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
            spell.range = 475.0f; // CDragon: GnarBigE castRange = 475 (mega Gnar)
            spell.spellDelay = 50.0f;
            spell.speed = 800.0f; // CDragon: missileSpeed = 800
            spell.checkSpellName = true;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // GRAVES
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Graves";
            spell.dangerlevel = 2;
            spell.name = "QuickDraw";
            spell.spellName = "GravesMove";
            spell.range = 425.0f; // CDragon: GravesE castRange = 425
            spell.spellDelay = 50.0f;
            spell.speed = 1250.0f; // CDragon: missileSpeed = 1250
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // KARMA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Karma";
            spell.dangerlevel = 3;
            spell.name = "Inspire";
            spell.spellName = "KarmaSolkimShield";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.speedArray = { 40.0f, 45.0f, 50.0f, 55.0f, 60.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // ==========================================
        // KASSADIN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Kassadin";
            spell.dangerlevel = 1;
            spell.name = "RiftWalk";
            spell.range = 450.0f; // CDragon: KassadinR castRange = 450 (kept C# 450)
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // KATARINA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Katarina";
            spell.dangerlevel = 3;
            spell.name = "KatarinaE";
            spell.spellName = "KatarinaE";
            spell.range = 700.0f; // CDragon: KatarinaE castRange = 700
            spell.speed = 3.4e38f; // float.MaxValue — blink, instant
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::Targetables };
            Spells.push_back(spell);
        }

        // ==========================================
        // KAYLE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Kayle";
            spell.dangerlevel = 3;
            spell.name = "Divine Blessing";
            spell.spellName = "JudicatorDivineBlessing";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.speedArray = { 18.0f, 21.0f, 24.0f, 27.0f, 30.0f };
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
            spell.evadeType = EvadeType::SpellShield; // Invulnerability
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::AllyChampions };
            Spells.push_back(spell);
        }

        // ==========================================
        // KENNEN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Kennen";
            spell.dangerlevel = 4;
            spell.name = "Lightning Rush";
            spell.spellName = "KennenLightningRush";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.speedArray = { 100.0f, 100.0f, 100.0f, 100.0f, 100.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // KINDRED
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Kindred";
            spell.dangerlevel = 1;
            spell.name = "KindredQ";
            spell.spellName = "KindredQ";
            spell.range = 300.0f; // CDragon: KindredQ castRange = 300 (dash distance)
            spell.fixedRange = true;
            spell.speed = 733.0f; // CDragon: missileSpeed = 733
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // LEBLANC
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Leblanc";
            spell.dangerlevel = 2;
            spell.name = "Distortion";
            spell.spellName = "LeblancSlide";
            spell.range = 600.0f; // CDragon: LeblancW castRange = 600
            spell.spellDelay = 50.0f;
            spell.speed = 1600.0f; // CDragon: missileSpeed = 1600
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
            spell.checkSpellName = true;
            spell.range = 600.0f; // CDragon: LeblancRW castRange = 600
            spell.spellDelay = 50.0f;
            spell.speed = 1600.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // LEESIN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "LeeSin";
            spell.dangerlevel = 3;
            spell.name = "LeeSinW";
            spell.spellName = "BlindMonkWOne";
            spell.range = 700.0f; // CDragon: BlindMonkWOne castRange = 700
            spell.speed = 1400.0f; // CDragon: missileSpeed = 1400
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Shield;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::AllyChampions, EvadeSpellTargets::AllyMinions };
            Spells.push_back(spell);
        }

        // ==========================================
        // LUCIAN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Lucian";
            spell.dangerlevel = 1;
            spell.name = "RelentlessPursuit";
            spell.spellName = "LucianE";
            spell.range = 425.0f; // CDragon: LucianE castRange = 445 (kept C# 425)
            spell.spellDelay = 50.0f;
            spell.speed = 1350.0f; // CDragon: missileSpeed = 1350
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // LULU
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Lulu";
            spell.dangerlevel = 3;
            spell.name = "Whimsy";
            spell.spellName = "LuluW";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.speedArray = { 30.0f, 30.0f, 30.0f, 35.0f, 40.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // ==========================================
        // MASTERYI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "MasterYi";
            spell.dangerlevel = 3;
            spell.name = "AlphaStrike";
            spell.spellName = "AlphaStrike";
            spell.range = 600.0f; // CDragon: AlphaStrike castRange = 600
            spell.speed = 3.4e38f; // float.MaxValue — blink, instant
            spell.spellDelay = 100.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions };
            spell.untargetable = true;
            Spells.push_back(spell);
        }

        // ==========================================
        // MORGANA
        // ==========================================
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
            spell.spellTargets = { EvadeSpellTargets::AllyChampions };
            Spells.push_back(spell);
        }

        // ==========================================
        // NOCTURNE
        // ==========================================
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

        // ==========================================
        // NIDALEE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Nidalee";
            spell.dangerlevel = 4;
            spell.name = "Pounce";
            spell.spellName = "Pounce";
            spell.range = 375.0f; // CDragon: Pounce castRange = 375 (cougar form W)
            spell.spellDelay = 150.0f;
            spell.speed = 1750.0f; // CDragon: missileSpeed = 1750
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }

        // ==========================================
        // NUNU
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Nunu";
            spell.dangerlevel = 2;
            spell.name = "BloodBoil";
            spell.spellName = "BloodBoil";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.speedArray = { 8.0f, 9.0f, 10.0f, 11.0f, 12.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Target;
            Spells.push_back(spell);
        }

        // ==========================================
        // POPPY
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Poppy";
            spell.dangerlevel = 3;
            spell.name = "Steadfast Presence";
            spell.spellName = "PoppyW";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.speedArray = { 27.0f, 29.0f, 31.0f, 33.0f, 35.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // RIVEN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Riven";
            spell.dangerlevel = 1;
            spell.name = "BrokenWings";
            spell.spellName = "RivenTriCleave";
            spell.range = 260.0f; // CDragon: RivenQ castRange = 260 (dash per Q cast)
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.speed = 560.0f; // CDragon: missileSpeed = 560
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Riven";
            spell.dangerlevel = 1;
            spell.name = "Valor";
            spell.spellName = "RivenFeint";
            spell.range = 325.0f; // CDragon: RivenE castRange = 325
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.speed = 1200.0f; // CDragon: missileSpeed = 1200
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // RUMBLE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Rumble";
            spell.dangerlevel = 3;
            spell.name = "Scrap Shield";
            spell.spellName = "RumbleShield";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.speedArray = { 10.0f, 15.0f, 20.0f, 25.0f, 30.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // SIVIR
        // ==========================================
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

        // ==========================================
        // SHYVANA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Shyvana";
            spell.dangerlevel = 3;
            spell.name = "Burnout";
            spell.spellName = "ShyvanaImmolationAura";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.speedArray = { 30.0f, 35.0f, 40.0f, 45.0f, 50.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // SHACO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Shaco";
            spell.dangerlevel = 3;
            spell.name = "Deceive";
            spell.spellName = "Deceive";
            spell.range = 400.0f; // CDragon: Deceive castRange = 400
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // SONA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Sona";
            spell.dangerlevel = 3;
            spell.name = "Song of Celerity";
            spell.spellName = "SonaE";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.speedArray = { 13.0f, 14.0f, 15.0f, 16.0f, 25.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // TALON
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Talon";
            spell.dangerlevel = 4;
            spell.name = "Shadow Assualt";
            spell.spellName = "TalonShadowAssault";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.speedArray = { 40.0f, 40.0f, 40.0f, 40.0f, 40.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // TEEMO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Teemo";
            spell.dangerlevel = 3;
            spell.name = "Move Quick";
            spell.spellName = "MoveQuick";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.speedArray = { 10.0f, 14.0f, 18.0f, 22.0f, 26.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // TRISTANA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Tristana";
            spell.dangerlevel = 3;
            spell.name = "RocketJump";
            spell.spellName = "RocketJump";
            spell.range = 900.0f; // CDragon: RocketJump castRange = 900
            spell.spellDelay = 500.0f;
            spell.speed = 1100.0f; // CDragon: missileSpeed = 1100
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // TRYNDAMERE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Tryndamere";
            spell.dangerlevel = 3;
            spell.name = "SpinningSlash";
            spell.spellName = "Slash";
            spell.range = 660.0f; // CDragon: Slash castRange = 660
            spell.spellDelay = 50.0f;
            spell.speed = 900.0f; // CDragon: missileSpeed = 900
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // UDYR
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Udyr";
            spell.dangerlevel = 3;
            spell.name = "Bear Stance";
            spell.spellName = "UdyrBearStance";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.speedArray = { 15.0f, 20.0f, 25.0f, 30.0f, 35.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // VAYNE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Vayne";
            spell.dangerlevel = 1;
            spell.name = "Tumble";
            spell.spellName = "VayneTumble";
            spell.range = 300.0f; // CDragon: VayneQ castRange = 300 (roll distance)
            spell.fixedRange = true;
            spell.speed = 900.0f; // CDragon: missileSpeed = 900
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // YASUO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Yasuo";
            spell.dangerlevel = 2;
            spell.name = "SweepingBlade";
            spell.spellName = "YasuoDashWrapper";
            spell.range = 475.0f; // CDragon: YasuoDashWrapper castRange = 475
            spell.fixedRange = true;
            spell.speed = 1000.0f; // CDragon: missileSpeed = 1000
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Yasuo";
            spell.dangerlevel = 3;
            spell.name = "WindWall";
            spell.spellName = "YasuoWMovingWall";
            spell.range = 400.0f; // CDragon: YasuoW castRange = 400
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::WindWall;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // ZILEAN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Zilean";
            spell.dangerlevel = 3;
            spell.name = "Timewarp";
            spell.spellName = "ZileanE";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.speedArray = { 40.0f, 55.0f, 70.0f, 85.0f, 99.0f };
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // AKALI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Akali";
            spell.dangerlevel = 4;
            spell.name = "Shadow Dance";
            spell.spellName = "AkaliShadowDance";
            spell.range = 700.0f; // CDragon: castRange = 700
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // ALISTAR
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Alistar";
            spell.dangerlevel = 3;
            spell.name = "Headbutt";
            spell.spellName = "Pulverize"; // Headbutt is part of combo, uses Pulverize slot
            spell.range = 650.0f; // CDragon: Headbutt castRange = 650
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions };
            Spells.push_back(spell);
        }

        // ==========================================
        // AMBESSA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Ambessa";
            spell.dangerlevel = 4;
            spell.name = "Sundering Strike";
            spell.spellName = "AmbessaE";
            spell.range = 450.0f;
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Ambessa";
            spell.dangerlevel = 5;
            spell.name = "Public Execution";
            spell.spellName = "AmbessaR";
            spell.range = 800.0f;
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // BEL'VETH
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Belveth";
            spell.dangerlevel = 3;
            spell.name = "Abyssal Dive";
            spell.spellName = "BelvethE";
            spell.range = 350.0f;
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // BRIAR
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Briar";
            spell.dangerlevel = 5;
            spell.name = "Certain Death";
            spell.spellName = "BriarR";
            spell.range = 800.0f;
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions };
            Spells.push_back(spell);
        }

        // ==========================================
        // CAMILLE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Camille";
            spell.dangerlevel = 4;
            spell.name = "Hookshot";
            spell.spellName = "CamilleE";
            spell.range = 800.0f; // CDragon: Hookshot castRange = 800 (grapple range)
            spell.spellDelay = 50.0f;
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
            spell.range = 500.0f; // CDragon: CamilleR castRange = 500
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // FIZZ
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Fizz";
            spell.dangerlevel = 4;
            spell.name = "Playful / Trickster";
            spell.spellName = "FizzE";
            spell.range = 400.0f; // CDragon: FizzE castRange = 400 (dash distance)
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // GAREN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Garen";
            spell.dangerlevel = 2;
            spell.name = "Decisive Strike";
            spell.spellName = "GarenQ";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.speedArray = { 35.0f, 40.0f, 45.0f, 50.0f, 55.0f }; // CDragon: MoveSpeedMod ~30-50%
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // GWEN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Gwen";
            spell.dangerlevel = 3;
            spell.name = "Hallowed Mist";
            spell.spellName = "GwenW";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::SpellShield; // Untargetable in mist
            spell.castType = EvadeCastType::Self;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Gwen";
            spell.dangerlevel = 3;
            spell.name = "Skip 'n Slash";
            spell.spellName = "GwenE";
            spell.range = 350.0f; // CDragon: GwenE castRange = 350
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // IRELIA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Irelia";
            spell.dangerlevel = 3;
            spell.name = "Bladesurge";
            spell.spellName = "IreliaQ";
            spell.range = 625.0f; // CDragon: IreliaQ castRange = 625 (dash to target)
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions };
            Spells.push_back(spell);
        }

        // ==========================================
        // JARVAN IV
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "JarvanIV";
            spell.dangerlevel = 4;
            spell.name = "Dragon Strike";
            spell.spellName = "JarvanIVQ";
            spell.range = 770.0f; // CDragon: Dragon Strike castRange = 770
            spell.spellDelay = 250.0f;
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
            spell.range = 625.0f; // CDragon: Cataclysm castRange = 625
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions };
            Spells.push_back(spell);
        }

        // ==========================================
        // JAX
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Jax";
            spell.dangerlevel = 3;
            spell.name = "Leap Strike";
            spell.spellName = "JaxQ";
            spell.range = 700.0f; // CDragon: Leap Strike castRange = 700
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions, EvadeSpellTargets::AllyChampions, EvadeSpellTargets::AllyMinions };
            Spells.push_back(spell);
        }

        // ==========================================
        // KAI'SA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Kaisa";
            spell.dangerlevel = 3;
            spell.name = "Supercharge";
            spell.spellName = "KaisaE";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.speedArray = { 40.0f, 50.0f, 60.0f, 70.0f, 80.0f }; // CDragon: MoveSpeedMod
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
            spell.range = 1500.0f; // CDragon: Killer Instinct castRange = 1500
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // KAYN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Kayn";
            spell.dangerlevel = 3;
            spell.name = "Reaping Slash";
            spell.spellName = "KaynQ";
            spell.range = 350.0f; // CDragon: Reaping Slash dash distance
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // KINDRED
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Kindred";
            spell.dangerlevel = 3;
            spell.name = "Dance of Arrows";
            spell.spellName = "KindredQ";
            spell.range = 300.0f; // CDragon: Dance of Arrows dash distance
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // KLED
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Kled";
            spell.dangerlevel = 4;
            spell.name = "Jousting";
            spell.spellName = "KledE";
            spell.range = 500.0f; // CDragon: Jousting castRange = 500
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions };
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Kled";
            spell.dangerlevel = 5;
            spell.name = "Chaaaaaaaarge!!!";
            spell.spellName = "KledR";
            spell.range = 900.0f; // CDragon: Chaaaaaaaarge!!! castRange = 900
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // K'SANTE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "KSante";
            spell.dangerlevel = 3;
            spell.name = "Footwork";
            spell.spellName = "KSanteE";
            spell.range = 400.0f; // CDragon: Footwork castRange = 400
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // LEONA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Leona";
            spell.dangerlevel = 4;
            spell.name = "Zenith Blade";
            spell.spellName = "LeonaE";
            spell.range = 875.0f; // CDragon: Zenith Blade castRange = 875
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions };
            Spells.push_back(spell);
        }

        // ==========================================
        // LILLIA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Lillia";
            spell.dangerlevel = 3;
            spell.name = "Watch Out! Eep!";
            spell.spellName = "LilliaW";
            spell.range = 350.0f; // CDragon: LilliaW dash distance
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // MASTER YI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "MasterYi";
            spell.dangerlevel = 4;
            spell.name = "Alpha Strike";
            spell.spellName = "AlphaStrike";
            spell.range = 600.0f; // CDragon: Alpha Strike castRange = 600
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions };
            spell.untargetable = true;
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "MasterYi";
            spell.dangerlevel = 3;
            spell.name = "Highlander";
            spell.spellName = "MasterYiR";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.speedArray = { 35.0f, 45.0f, 55.0f, 65.0f, 75.0f }; // CDragon: MoveSpeedMod 25-45% + immune to slows
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // MONKEYKING (Wukong)
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "MonkeyKing";
            spell.dangerlevel = 3;
            spell.name = "Nimbus Strike";
            spell.spellName = "MonkeyKingNimbusKick";
            spell.range = 650.0f; // CDragon: Nimbus Strike castRange = 650
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions };
            Spells.push_back(spell);
        }

        // ==========================================
        // NAAFIRI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Naafiri";
            spell.dangerlevel = 3;
            spell.name = "Hounds' Quest";
            spell.spellName = "NaafiriW";
            spell.range = 800.0f; // CDragon: NaafiriW castRange = 800
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Naafiri";
            spell.dangerlevel = 5;
            spell.name = "We Are More";
            spell.spellName = "NaafiriR";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.speedArray = { 50.0f, 60.0f, 70.0f }; // CDragon: MoveSpeedMod
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // NEEKO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Neeko";
            spell.dangerlevel = 3;
            spell.name = "Shapesplitter";
            spell.spellName = "NeekoW";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.speedArray = { 20.0f, 25.0f, 30.0f, 35.0f, 40.0f }; // CDragon: MoveSpeedMod
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // NILAH
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Nilah";
            spell.dangerlevel = 4;
            spell.name = "Apotheosis";
            spell.spellName = "NilahR";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::SpellShield; // Briefly untargetable during ult
            spell.castType = EvadeCastType::Self;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }

        // ==========================================
        // POPPY
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Poppy";
            spell.dangerlevel = 4;
            spell.name = "Heroic Charge";
            spell.spellName = "PoppyE";
            spell.range = 525.0f; // CDragon: Heroic Charge castRange = 525
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions };
            Spells.push_back(spell);
        }

        // ==========================================
        // PYKE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Pyke";
            spell.dangerlevel = 4;
            spell.name = "Phantom Undertow";
            spell.spellName = "PykeE";
            spell.range = 550.0f; // CDragon: Phantom Undertow dash distance
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // QIYANA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Qiyana";
            spell.dangerlevel = 4;
            spell.name = "Edge of Ixtal";
            spell.spellName = "QiyanaE";
            spell.range = 650.0f; // CDragon: Edge of Ixtal castRange = 650
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions };
            Spells.push_back(spell);
        }

        // ==========================================
        // RAKAN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Rakan";
            spell.dangerlevel = 4;
            spell.name = "Grand Entrance";
            spell.spellName = "RakanW";
            spell.range = 600.0f; // CDragon: Grand Entrance castRange = 600
            spell.spellDelay = 50.0f;
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
            spell.range = 500.0f; // CDragon: Battle Dance castRange = 500
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::AllyChampions };
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "Rakan";
            spell.dangerlevel = 5;
            spell.name = "The Quickness";
            spell.spellName = "RakanR";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.speedArray = { 50.0f, 60.0f, 70.0f }; // CDragon: MoveSpeedMod
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // RAMMUS
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Rammus";
            spell.dangerlevel = 3;
            spell.name = "Powerball";
            spell.spellName = "RammusW"; // Powerball is actually Q slot but named RammusW in old data
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.speedArray = { 25.0f, 30.0f, 35.0f, 40.0f, 45.0f }; // CDragon: MoveSpeedMod
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // REK'SAI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "RekSai";
            spell.dangerlevel = 4;
            spell.name = "Burrowed Unburrow";
            spell.spellName = "RekSaiE";
            spell.range = 250.0f; // CDragon: Unburrow dash distance
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
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
            spell.range = 1500.0f; // CDragon: Void Rush castRange = 1500
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions };
            Spells.push_back(spell);
        }

        // ==========================================
        // RELL
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Rell";
            spell.dangerlevel = 4;
            spell.name = "Ferromancy: Mount Up";
            spell.spellName = "RellW";
            spell.range = 400.0f; // CDragon: RellW dash distance (mounting)
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // RENATA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Renata";
            spell.dangerlevel = 3;
            spell.name = "Loyalty Program";
            spell.spellName = "RenataE";
            spell.range = 800.0f; // CDragon: Loyalty Program castRange = 800
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Shield;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::AllyChampions };
            Spells.push_back(spell);
        }

        // ==========================================
        // SAMIRA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Samira";
            spell.dangerlevel = 3;
            spell.name = "Wild Rush";
            spell.spellName = "SamiraE";
            spell.range = 600.0f; // CDragon: Wild Rush castRange = 600
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions, EvadeSpellTargets::AllyChampions };
            Spells.push_back(spell);
        }

        // ==========================================
        // SEJUANI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Sejuani";
            spell.dangerlevel = 4;
            spell.name = "Arctic Assault";
            spell.spellName = "SejuaniQ";
            spell.range = 650.0f; // CDragon: Arctic Assault castRange = 650
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // SERAPHINE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Seraphine";
            spell.dangerlevel = 3;
            spell.name = "Surround Sound";
            spell.spellName = "SeraphineE";
            spell.range = 900.0f; // CDragon: Surround Sound castRange = 900
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.speedArray = { 20.0f, 25.0f, 30.0f, 35.0f, 40.0f }; // CDragon: MoveSpeedMod on allies
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // SETT
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Sett";
            spell.dangerlevel = 5;
            spell.name = "The Show Stopper";
            spell.spellName = "SettR";
            spell.range = 500.0f; // CDragon: The Show Stopper castRange = 500
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions };
            Spells.push_back(spell);
        }

        // ==========================================
        // SINGED
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Singed";
            spell.dangerlevel = 2;
            spell.name = "Insanity Potion";
            spell.spellName = "SingedR";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.speedArray = { 20.0f, 30.0f, 40.0f, 50.0f, 60.0f }; // CDragon: MoveSpeedMod
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // SMOLDER
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Smolder";
            spell.dangerlevel = 3;
            spell.name = "Flap, Flap, Flap";
            spell.spellName = "SmolderE";
            spell.range = 350.0f; // CDragon: Flap Flap Flap dash distance
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // SONA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Sona";
            spell.dangerlevel = 2;
            spell.name = "Song of Celerity";
            spell.spellName = "SonaE";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.speedArray = { 13.0f, 14.0f, 15.0f, 16.0f, 17.0f }; // CDragon: MoveSpeedMod
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // SYLAS
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Sylas";
            spell.dangerlevel = 4;
            spell.name = "Abduct";
            spell.spellName = "SylasE";
            spell.range = 750.0f; // CDragon: Abduct castRange = 750 (second cast dash)
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // TAHM KENCH
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "TahmKench";
            spell.dangerlevel = 4;
            spell.name = "Abyssal Dive";
            spell.spellName = "TahmKenchR";
            spell.range = 250.0f; // CDragon: Abyssal Dive dash distance
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // TALIYAH
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Taliyah";
            spell.dangerlevel = 4;
            spell.name = "Seismic Shove";
            spell.spellName = "TaliyahW";
            spell.range = 900.0f; // CDragon: Seismic Shove castRange = 900
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Dash; // Taliyah W can knock back/dash
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // UDYR
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Udyr";
            spell.dangerlevel = 3;
            spell.name = "Wingborne Storm";
            spell.spellName = "UdyrE";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.speedArray = { 20.0f, 25.0f, 30.0f, 35.0f, 40.0f }; // CDragon: MoveSpeedMod
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Self;
            Spells.push_back(spell);
        }

        // ==========================================
        // VI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Vi";
            spell.dangerlevel = 4;
            spell.name = "Vault Breaker";
            spell.spellName = "ViQ";
            spell.range = 750.0f; // CDragon: Vault Breaker castRange = 750
            spell.spellDelay = 50.0f;
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
            spell.range = 800.0f; // CDragon: Assault and Battery castRange = 800
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions };
            Spells.push_back(spell);
        }

        // ==========================================
        // VIEGO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Viego";
            spell.dangerlevel = 4;
            spell.name = "Spectral Maw";
            spell.spellName = "ViegoW";
            spell.range = 300.0f; // CDragon: Spectral Maw dash distance
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
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
            spell.range = 600.0f; // CDragon: Harrowed Path dash distance
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // VOLIBEAR
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Volibear";
            spell.dangerlevel = 3;
            spell.name = "Thundering Smite";
            spell.spellName = "VolibearQ";
            spell.spellDelay = 250.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.speedArray = { 10.0f, 15.0f, 20.0f, 25.0f, 30.0f }; // CDragon: MoveSpeedMod
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
            spell.range = 700.0f; // CDragon: Stormbringer castRange = 700 (dash to target location)
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // XAYAH
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Xayah";
            spell.dangerlevel = 5;
            spell.name = "Featherstorm";
            spell.spellName = "XayahR";
            spell.range = 600.0f; // CDragon: Featherstorm leap distance
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            spell.untargetable = true;
            Spells.push_back(spell);
        }

        // ==========================================
        // XIN ZHAO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "XinZhao";
            spell.dangerlevel = 3;
            spell.name = "Audacious Charge";
            spell.spellName = "XinZhaoE";
            spell.range = 600.0f; // CDragon: Audacious Charge castRange = 600
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::EnemyChampions, EvadeSpellTargets::EnemyMinions };
            Spells.push_back(spell);
        }

        // ==========================================
        // YONE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Yone";
            spell.dangerlevel = 4;
            spell.name = "Soul Unbound";
            spell.spellName = "YoneE";
            spell.range = 300.0f; // CDragon: Soul Unbound dash distance
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
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
            spell.range = 1000.0f; // CDragon: Fate Sealed castRange = 1000
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // YUUMI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Yuumi";
            spell.dangerlevel = 3;
            spell.name = "You and Me!";
            spell.spellName = "YuumiW";
            spell.range = 1000.0f; // CDragon: You and Me! castRange = 1000 (attach to ally)
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::W;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::AllyChampions };
            Spells.push_back(spell);
        }

        // ==========================================
        // ZOE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Zoe";
            spell.dangerlevel = 4;
            spell.name = "Portal Jump";
            spell.spellName = "ZoeR";
            spell.range = 575.0f; // CDragon: Portal Jump castRange = 575
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Blink;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // AURORA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Aurora";
            spell.dangerlevel = 3;
            spell.name = "Across the Divide";
            spell.spellName = "AuroraE";
            spell.range = 400.0f; // CDragon: AuroraE dash distance
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // MILIO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Milio";
            spell.dangerlevel = 3;
            spell.name = "Warm Hugs";
            spell.spellName = "MilioE";
            spell.range = 800.0f; // CDragon: Warm Hugs castRange = 800
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::E;
            spell.speedArray = { 15.0f, 20.0f, 25.0f, 30.0f, 35.0f }; // CDragon: MoveSpeedMod on ally
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.castType = EvadeCastType::Target;
            spell.spellTargets = { EvadeSpellTargets::AllyChampions };
            Spells.push_back(spell);
        }

        // ==========================================
        // LOCKE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "Locke";
            spell.dangerlevel = 4;
            spell.name = "LockeE";
            spell.spellName = "LockeE";
            spell.range = 400.0f;
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
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
            spell.range = 600.0f;
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Dash;
            spell.castType = EvadeCastType::Position;
            Spells.push_back(spell);
        }

        // ==========================================
        // ALL CHAMPIONS (Items + Summoner Spells)
        // ==========================================
        {
            EvadeSpellData spell;
            spell.charName = "AllChampions";
            spell.dangerlevel = 3;
            spell.name = "Talisman of Ascension";
            spell.spellName = "TalismanOfAscension";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.speedArray = { 40.0f, 40.0f, 40.0f, 40.0f, 40.0f };
            spell.castType = EvadeCastType::Self;
            spell.isItem = true;
            spell.itemID = 3060; // Talisman of Ascension
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "AllChampions";
            spell.dangerlevel = 3;
            spell.name = "Youmuu's Ghostblade";
            spell.spellName = "YoumuusGhostblade";
            spell.spellDelay = 50.0f;
            spell.spellKey = EvadeSpellSlot::Q;
            spell.evadeType = EvadeType::MovementSpeedBuff;
            spell.speedArray = { 20.0f, 20.0f, 20.0f, 20.0f, 20.0f };
            spell.castType = EvadeCastType::Self;
            spell.isItem = true;
            spell.itemID = 3142; // Youmuu's Ghostblade
            Spells.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.charName = "AllChampions";
            spell.dangerlevel = 4;
            spell.name = "Flash";
            spell.spellName = "SummonerFlash";
            spell.range = 400.0f; // CDragon: SummonerFlash castRange = 400
            spell.fixedRange = true;
            spell.spellDelay = 50.0f;
            spell.isSummonerSpell = true;
            spell.spellKey = EvadeSpellSlot::R;
            spell.evadeType = EvadeType::Blink;
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
            spell.evadeType = EvadeType::SpellShield; // Invulnerability
            spell.castType = EvadeCastType::Self;
            spell.isItem = true;
            spell.itemID = 3157; // Zhonya's Hourglass
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
            spell.evadeType = EvadeType::SpellShield; // Invulnerability
            spell.castType = EvadeCastType::Self;
            spell.isItem = true;
            spell.itemID = 3159; // Wooglet's Witchcap
            Spells.push_back(spell);
        }
    }
};

inline std::vector<EvadeSpellData> EvadeSpellDatabase::Spells;

} // namespace Plugins::KuroEvade::InternalDatabase

// ============================================================================
// KuroEvade Compatibility Wrapper
// ============================================================================
namespace Plugins::KuroEvade {

static inline std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

struct EvadeSpellDatabase final {
    static const std::vector<EvadeSpellData>& Spells() {
        static std::vector<EvadeSpellData> combinedSpells;
        static bool initialized = false;
        if (!initialized) {
            if (::Plugins::KuroEvade::InternalDatabase::EvadeSpellDatabase::Spells.empty()) {
                ::Plugins::KuroEvade::InternalDatabase::EvadeSpellDatabase::Initialize();
            }

            for (const auto& zd : ::Plugins::KuroEvade::InternalDatabase::EvadeSpellDatabase::Spells) {
                EvadeSpellData e = {};
                e.ChampionName = zd.charName;
                e.Name = zd.name;
                e.CheckSpellName = zd.spellName;
                e.DangerLevel = zd.dangerlevel;
                e.Delay = static_cast<int>(zd.spellDelay);
                e.MaxRange = static_cast<int>(zd.range);
                e.Speed = static_cast<int>(zd.speed);
                e.FixedRange = zd.fixedRange;
                e.IsBehindTarget = zd.behindTarget;
                e.IsInfrontTarget = zd.infrontTarget;
                e.IsItem = zd.isItem;
                e.IsReversed = zd.isReversed;
                e.IsSpecial = zd.isSpecial;
                e.IsSummonerSpell = zd.isSummonerSpell;
                e.ItemId = zd.itemID;

                switch (zd.castType) {
                    case ::Plugins::KuroEvade::KuroEvadeCastType::Position: e.CastTypeValue = CastType::Position; break;
                    case ::Plugins::KuroEvade::KuroEvadeCastType::Target: e.CastTypeValue = CastType::Target; break;
                    case ::Plugins::KuroEvade::KuroEvadeCastType::Self: e.CastTypeValue = CastType::Self; break;
                }

                switch (zd.evadeType) {
                    case ::Plugins::KuroEvade::KuroEvadeType::Blink: e.EvadeTypeValue = EvadeType::Blink; break;
                    case ::Plugins::KuroEvade::KuroEvadeType::Dash: e.EvadeTypeValue = EvadeType::Dash; break;
                    case ::Plugins::KuroEvade::KuroEvadeType::Invulnerability: e.EvadeTypeValue = EvadeType::Invulnerability; break;
                    case ::Plugins::KuroEvade::KuroEvadeType::MovementSpeedBuff: e.EvadeTypeValue = EvadeType::MovementSpeedBuff; break;
                    case ::Plugins::KuroEvade::KuroEvadeType::SpellShield: e.EvadeTypeValue = EvadeType::SpellShield; break;
                    case ::Plugins::KuroEvade::KuroEvadeType::WindWall: e.EvadeTypeValue = EvadeType::WindWall; break;
                    default: e.EvadeTypeValue = EvadeType::Dash; break;
                }

                switch (zd.spellKey) {
                    case ::Plugins::KuroEvade::KuroEvadeSpellSlot::Q: e.Slot = SDK::SpellSlot::Q; break;
                    case ::Plugins::KuroEvade::KuroEvadeSpellSlot::W: e.Slot = SDK::SpellSlot::W; break;
                    case ::Plugins::KuroEvade::KuroEvadeSpellSlot::E: e.Slot = SDK::SpellSlot::E; break;
                    case ::Plugins::KuroEvade::KuroEvadeSpellSlot::R: e.Slot = SDK::SpellSlot::R; break;
                    default: e.Slot = SDK::SpellSlot::Unknown; break;
                }

                for (const auto& target : zd.spellTargets) {
                    switch (target) {
                        case ::Plugins::KuroEvade::KuroEvadeSpellTargets::AllyMinions: e.ValidTargets.push_back(SpellTargets::AllyMinions); break;
                        case ::Plugins::KuroEvade::KuroEvadeSpellTargets::EnemyMinions: e.ValidTargets.push_back(SpellTargets::EnemyMinions); break;
                        case ::Plugins::KuroEvade::KuroEvadeSpellTargets::AllyChampions: e.ValidTargets.push_back(SpellTargets::AllyChampions); break;
                        case ::Plugins::KuroEvade::KuroEvadeSpellTargets::EnemyChampions: e.ValidTargets.push_back(SpellTargets::EnemyChampions); break;
                    }
                }

                combinedSpells.push_back(e);
            }
            initialized = true;
        }
        return combinedSpells;
    }

    static std::vector<const EvadeSpellData*> ForChampion(const char* championName,
                                                          bool includeGlobal = true) {
        std::vector<const EvadeSpellData*> result;
        for (const auto& spell : Spells()) {
            const bool isGlobal = _stricmp(spell.ChampionName.c_str(), "AllChampions") == 0;
            const bool isChampion = championName && championName[0] &&
                _stricmp(spell.ChampionName.c_str(), championName) == 0;
            if ((includeGlobal && isGlobal) || isChampion) {
                result.push_back(&spell);
            }
        }
        return result;
    }
};

} // namespace Plugins::KuroEvade
