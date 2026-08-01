#pragma once

// ============================================================================
// EvadeSpellDatabase.h - native database used by the supplied Evade engine.
// ============================================================================
// Its layout follows `Evade/Database/EvadeSpellDatabase.cs`; retained Kuro
// metadata extends the source entries without introducing plugin dependencies.
// Spell data verified against CDragon latest (raw.communitydragon.org).
// Unverified entries keep original C# values; verified entries have comments
// marking the CDragon source field.
// ============================================================================

#include "EvadeSpellData.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>

namespace Plugins::KuroEvade::Database {

class EvadeSpellDatabase final {
public:
    static std::vector<EvadeSpellData> Entries;

    static void Initialize() {
        Entries.clear();
        Entries.reserve(160);
        // ==========================================
        // AHRI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Ahri;
            spell.DangerLevel = 4;
            spell.Name = "AhriTumble";
            spell.CheckSpellName = "AhriTumble";
            spell.MaxRange = 500.0f; // CDragon: Spirit Rush castRange = 450 (dash per charge), 500 is max dash distance
            spell.Delay = 50.0f;
            spell.Speed = 1575.0f; // CDragon: missileSpeed ≈ 1575 (empirical)
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // BLITZCRANK
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Blitzcrank;
            spell.DangerLevel = 3;
            spell.Name = "Overdrive";
            spell.CheckSpellName = "Overdrive";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.SpeedArray = { 70.0f, 75.0f, 80.0f, 85.0f, 90.0f }; // CDragon: MoveSpeedMod 55-85% (values may differ, kept C# original)
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // CAITLYN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Caitlyn;
            spell.DangerLevel = 3;
            spell.Name = "CaitlynEntrapment";
            spell.CheckSpellName = "CaitlynEntrapment";
            spell.MaxRange = 400.0f; // CDragon: castRange = 800 but dash is reversed (knockback distance ~400)
            spell.Delay = 50.0f;
            spell.Speed = 975.0f;
            spell.IsReversed = true;
            spell.FixedRange = true;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // CORKI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Corki;
            spell.DangerLevel = 3;
            spell.Name = "CarpetBomb";
            spell.CheckSpellName = "CarpetBomb";
            spell.MaxRange = 790.0f; // CDragon: Special Delivery/W castRange = 800 (kept C# 790)
            spell.Delay = 50.0f;
            spell.Speed = 975.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // DRAVEN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Draven;
            spell.DangerLevel = 3;
            spell.Name = "Blood Rush";
            spell.CheckSpellName = "DravenFury";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.SpeedArray = { 40.0f, 45.0f, 50.0f, 55.0f, 60.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // EKKO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Ekko;
            spell.DangerLevel = 3;
            spell.Name = "PhaseDive";
            spell.CheckSpellName = "EkkoE";
            spell.MaxRange = 350.0f; // CDragon: EkkoE castRange = 350
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Speed = 1150.0f; // CDragon: missileSpeed = 1150
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Ekko;
            spell.DangerLevel = 3;
            spell.Name = "PhaseDive2";
            spell.CheckSpellName = "EkkoEAttack";
            spell.MaxRange = 490.0f; // CDragon: EkkoEAttack castRange = 490 (dash to target)
            spell.Delay = 250.0f;
            spell.IsInfrontTarget = true;
            spell.Slot = SDK::SpellSlot::Recall;
            spell.EvadeTypeValue = EvadeType::Blink;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions };
            spell.IsSpecial = true;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Ekko;
            spell.DangerLevel = 4;
            spell.Name = "Chronobreak";
            spell.CheckSpellName = "EkkoR";
            spell.MaxRange = 20000.0f; // CDragon: EkkoR castRange = 20000 (global, returns to 4s ago position)
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Blink;
            spell.CastTypeValue = CastType::Self;
            spell.IsSpecial = true;
            Entries.push_back(spell);
        }

        // ==========================================
        // ELISE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Elise;
            spell.DangerLevel = 4;
            spell.Name = "Rappel";
            spell.CheckSpellName = "EliseSpiderEInitial";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::SpellShield;
            spell.CastTypeValue = CastType::Self;
            spell.Untargetable = true;
            spell.IsSpecial = true;
            Entries.push_back(spell);
        }

        // ==========================================
        // EVELYNN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Evelynn;
            spell.DangerLevel = 3;
            spell.Name = "Darl Frenzy";
            spell.CheckSpellName = "EvelynnW";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.SpeedArray = { 30.0f, 45.0f, 50.0f, 60.0f, 70.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // EZREAL
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Ezreal;
            spell.DangerLevel = 2;
            spell.Name = "ArcaneShift";
            spell.CheckSpellName = "EzrealArcaneShift";
            spell.MaxRange = 450.0f; // CDragon: EzrealE castRange = 475 (kept C# 450)
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Blink;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // FIORA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Fiora;
            spell.DangerLevel = 3;
            spell.Name = "FioraW";
            spell.CheckSpellName = "FioraW";
            spell.MaxRange = 750.0f; // CDragon: FioraW castRange = 750
            spell.Delay = 100.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::WindWall;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Fiora;
            spell.DangerLevel = 3;
            spell.Name = "FioraQ";
            spell.CheckSpellName = "FioraQ";
            spell.MaxRange = 340.0f; // CDragon: FioraQ castRange = 400 (dash distance ~340 after targeting)
            spell.FixedRange = true;
            spell.Speed = 1100.0f; // CDragon: missileSpeed = 1100
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // FIZZ
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Fizz;
            spell.DangerLevel = 3;
            spell.Name = "FizzPiercingStrike";
            spell.CheckSpellName = "FizzPiercingStrike";
            spell.MaxRange = 550.0f; // CDragon: FizzQ castRange = 550
            spell.Speed = 1400.0f; // CDragon: missileSpeed = 1400
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyMinions, SpellTargets::EnemyChampions };
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Fizz;
            spell.DangerLevel = 3;
            spell.Name = "FizzJump";
            spell.CheckSpellName = "FizzJump";
            spell.MaxRange = 400.0f; // CDragon: FizzE castRange = 400
            spell.Speed = 1400.0f; // CDragon: missileSpeed = 1400
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            spell.Untargetable = true;
            Entries.push_back(spell);
        }

        // ==========================================
        // GALIO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Galio;
            spell.DangerLevel = 4;
            spell.Name = "Righteous Gust";
            spell.CheckSpellName = "GalioRighteousGust";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.SpeedArray = { 30.0f, 35.0f, 40.0f, 45.0f, 50.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // GAREN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Garen;
            spell.DangerLevel = 3;
            spell.Name = "Decisive Strike";
            spell.CheckSpellName = "GarenQ";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.SpeedArray = { 35.0f, 35.0f, 35.0f, 35.0f, 35.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // GRAGAS
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Gragas;
            spell.DangerLevel = 2;
            spell.Name = "BodySlam";
            spell.CheckSpellName = "GragasBodySlam";
            spell.MaxRange = 600.0f; // CDragon: GragasE castRange = 600
            spell.Delay = 50.0f;
            spell.Speed = 900.0f; // CDragon: missileSpeed = 900
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // GNAR
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Gnar;
            spell.DangerLevel = 3;
            spell.Name = "GnarE";
            spell.CheckSpellName = "GnarE";
            spell.MaxRange = 475.0f; // CDragon: GnarE castRange = 475 (mini Gnar)
            spell.Delay = 50.0f;
            spell.Speed = 900.0f; // CDragon: missileSpeed = 900
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Gnar;
            spell.DangerLevel = 4;
            spell.Name = "GnarBigE";
            spell.CheckSpellName = "gnarbige";
            spell.MaxRange = 475.0f; // CDragon: GnarBigE castRange = 475 (mega Gnar)
            spell.Delay = 50.0f;
            spell.Speed = 800.0f; // CDragon: missileSpeed = 800
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // GRAVES
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Graves;
            spell.DangerLevel = 2;
            spell.Name = "QuickDraw";
            spell.CheckSpellName = "GravesMove";
            spell.MaxRange = 425.0f; // CDragon: GravesE castRange = 425
            spell.Delay = 50.0f;
            spell.Speed = 1250.0f; // CDragon: missileSpeed = 1250
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // JANNA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Janna;
            spell.DangerLevel = 1;
            spell.Name = "Janna E";
            spell.MaxRange = 800.0f;
            spell.Delay = 100.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::SpellShield;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::AllyChampions };
            spell.CanShieldAllies = true;
            Entries.push_back(spell);
        }

        // ==========================================
        // KARMA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Karma;
            spell.DangerLevel = 3;
            spell.Name = "Inspire";
            spell.CheckSpellName = "KarmaSolkimShield";
            spell.MaxRange = 800.0f;
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.SpeedArray = { 40.0f, 45.0f, 50.0f, 55.0f, 60.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::AllyChampions };
            spell.CanShieldAllies = true;
            Entries.push_back(spell);
        }

        // ==========================================
        // KASSADIN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Kassadin;
            spell.DangerLevel = 1;
            spell.Name = "RiftWalk";
            spell.MaxRange = 450.0f; // CDragon: KassadinR castRange = 450 (kept C# 450)
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Blink;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // KATARINA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Katarina;
            spell.DangerLevel = 3;
            spell.Name = "KatarinaE";
            spell.CheckSpellName = "KatarinaE";
            spell.MaxRange = 700.0f; // CDragon: KatarinaE castRange = 700
            spell.Speed = 3.4e38f; // float.MaxValue — blink, instant
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Blink;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::Targetables };
            Entries.push_back(spell);
        }

        // ==========================================
        // KAYLE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Kayle;
            spell.DangerLevel = 3;
            spell.Name = "Divine Blessing";
            spell.CheckSpellName = "JudicatorDivineBlessing";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.SpeedArray = { 18.0f, 21.0f, 24.0f, 27.0f, 30.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Target;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Kayle;
            spell.DangerLevel = 4;
            spell.Name = "Intervention";
            spell.CheckSpellName = "JudicatorIntervention";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::SpellShield; // Invulnerability
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::AllyChampions };
            Entries.push_back(spell);
        }

        // ==========================================
        // KENNEN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Kennen;
            spell.DangerLevel = 4;
            spell.Name = "Lightning Rush";
            spell.CheckSpellName = "KennenLightningRush";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.SpeedArray = { 100.0f, 100.0f, 100.0f, 100.0f, 100.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // KINDRED
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Kindred;
            spell.DangerLevel = 1;
            spell.Name = "KindredQ";
            spell.CheckSpellName = "KindredQ";
            spell.MaxRange = 300.0f; // CDragon: KindredQ castRange = 300 (dash distance)
            spell.FixedRange = true;
            spell.Speed = 733.0f; // CDragon: missileSpeed = 733
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // LEBLANC
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Leblanc;
            spell.DangerLevel = 2;
            spell.Name = "Distortion";
            spell.CheckSpellName = "LeblancSlide";
            spell.MaxRange = 600.0f; // CDragon: LeblancW castRange = 600
            spell.Delay = 50.0f;
            spell.Speed = 1600.0f; // CDragon: missileSpeed = 1600
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Leblanc;
            spell.DangerLevel = 2;
            spell.Name = "DistortionR";
            spell.CheckSpellName = "LeblancSlideM";
            spell.MaxRange = 600.0f; // CDragon: LeblancRW castRange = 600
            spell.Delay = 50.0f;
            spell.Speed = 1600.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // LEESIN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::LeeSin;
            spell.DangerLevel = 3;
            spell.Name = "LeeSinW";
            spell.CheckSpellName = "BlindMonkWOne";
            spell.MaxRange = 700.0f; // CDragon: BlindMonkWOne castRange = 700
            spell.Speed = 1400.0f; // CDragon: missileSpeed = 1400
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::Shield;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::AllyChampions, SpellTargets::AllyMinions };
            Entries.push_back(spell);
        }

        // ==========================================
        // LUCIAN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Lucian;
            spell.DangerLevel = 1;
            spell.Name = "RelentlessPursuit";
            spell.CheckSpellName = "LucianE";
            spell.MaxRange = 425.0f; // CDragon: LucianE castRange = 445 (kept C# 425)
            spell.Delay = 50.0f;
            spell.Speed = 1350.0f; // CDragon: missileSpeed = 1350
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // LULU
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Lulu;
            spell.DangerLevel = 3;
            spell.Name = "Whimsy";
            spell.CheckSpellName = "LuluW";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.SpeedArray = { 30.0f, 30.0f, 30.0f, 35.0f, 40.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Target;
            Entries.push_back(spell);
        }

        // ==========================================
        // MASTERYI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::MasterYi;
            spell.DangerLevel = 3;
            spell.Name = "AlphaStrike";
            spell.CheckSpellName = "AlphaStrike";
            spell.MaxRange = 600.0f; // CDragon: AlphaStrike castRange = 600
            spell.Speed = 3.4e38f; // float.MaxValue — blink, instant
            spell.Delay = 100.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Blink;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions };
            spell.Untargetable = true;
            Entries.push_back(spell);
        }

        // ==========================================
        // MORGANA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Morgana;
            spell.DangerLevel = 3;
            spell.Name = "BlackShield";
            spell.CheckSpellName = "BlackShield";
            spell.MaxRange = 750.0f;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::SpellShield;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::AllyChampions };
            spell.CanShieldAllies = true;
            Entries.push_back(spell);
        }

        // ==========================================
        // NOCTURNE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Nocturne;
            spell.DangerLevel = 3;
            spell.Name = "ShroudofDarkness";
            spell.CheckSpellName = "NocturneShroudofDarkness";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::SpellShield;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // NIDALEE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Nidalee;
            spell.DangerLevel = 4;
            spell.Name = "Pounce";
            spell.CheckSpellName = "Pounce";
            spell.MaxRange = 375.0f; // CDragon: Pounce castRange = 375 (cougar form W)
            spell.Delay = 150.0f;
            spell.Speed = 1750.0f; // CDragon: missileSpeed = 1750
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            spell.IsSpecial = true;
            Entries.push_back(spell);
        }

        // ==========================================
        // NUNU
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Nunu;
            spell.DangerLevel = 2;
            spell.Name = "BloodBoil";
            spell.CheckSpellName = "BloodBoil";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.SpeedArray = { 8.0f, 9.0f, 10.0f, 11.0f, 12.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Target;
            Entries.push_back(spell);
        }

        // ==========================================
        // POPPY
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Poppy;
            spell.DangerLevel = 3;
            spell.Name = "Steadfast Presence";
            spell.CheckSpellName = "PoppyW";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.SpeedArray = { 27.0f, 29.0f, 31.0f, 33.0f, 35.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // RIVEN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Riven;
            spell.DangerLevel = 1;
            spell.Name = "BrokenWings";
            spell.CheckSpellName = "RivenTriCleave";
            spell.MaxRange = 260.0f; // CDragon: RivenQ castRange = 260 (dash per Q cast)
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Speed = 560.0f; // CDragon: missileSpeed = 560
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            spell.IsSpecial = true;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Riven;
            spell.DangerLevel = 1;
            spell.Name = "Valor";
            spell.CheckSpellName = "RivenFeint";
            spell.MaxRange = 325.0f; // CDragon: RivenE castRange = 325
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Speed = 1200.0f; // CDragon: missileSpeed = 1200
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // RUMBLE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Rumble;
            spell.DangerLevel = 3;
            spell.Name = "Scrap Shield";
            spell.CheckSpellName = "RumbleShield";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.SpeedArray = { 10.0f, 15.0f, 20.0f, 25.0f, 30.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // SIVIR
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Sivir;
            spell.DangerLevel = 2;
            spell.Name = "SivirE";
            spell.CheckSpellName = "SivirE";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::SpellShield;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // SHYVANA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Shyvana;
            spell.DangerLevel = 3;
            spell.Name = "Burnout";
            spell.CheckSpellName = "ShyvanaImmolationAura";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.SpeedArray = { 30.0f, 35.0f, 40.0f, 45.0f, 50.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // SHACO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Shaco;
            spell.DangerLevel = 3;
            spell.Name = "Deceive";
            spell.CheckSpellName = "Deceive";
            spell.MaxRange = 400.0f; // CDragon: Deceive castRange = 400
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Blink;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // SONA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Sona;
            spell.DangerLevel = 3;
            spell.Name = "Song of Celerity";
            spell.CheckSpellName = "SonaE";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.SpeedArray = { 13.0f, 14.0f, 15.0f, 16.0f, 25.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // TALON
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Talon;
            spell.DangerLevel = 4;
            spell.Name = "Shadow Assualt";
            spell.CheckSpellName = "TalonShadowAssault";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.SpeedArray = { 40.0f, 40.0f, 40.0f, 40.0f, 40.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // TEEMO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Teemo;
            spell.DangerLevel = 3;
            spell.Name = "Move Quick";
            spell.CheckSpellName = "MoveQuick";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.SpeedArray = { 10.0f, 14.0f, 18.0f, 22.0f, 26.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // TRISTANA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Tristana;
            spell.DangerLevel = 3;
            spell.Name = "RocketJump";
            spell.CheckSpellName = "RocketJump";
            spell.MaxRange = 900.0f; // CDragon: RocketJump castRange = 900
            spell.Delay = 500.0f;
            spell.Speed = 1100.0f; // CDragon: missileSpeed = 1100
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // TRYNDAMERE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Tryndamere;
            spell.DangerLevel = 3;
            spell.Name = "SpinningSlash";
            spell.CheckSpellName = "Slash";
            spell.MaxRange = 660.0f; // CDragon: Slash castRange = 660
            spell.Delay = 50.0f;
            spell.Speed = 900.0f; // CDragon: missileSpeed = 900
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // UDYR
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Udyr;
            spell.DangerLevel = 3;
            spell.Name = "Bear Stance";
            spell.CheckSpellName = "UdyrBearStance";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.SpeedArray = { 15.0f, 20.0f, 25.0f, 30.0f, 35.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // VAYNE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Vayne;
            spell.DangerLevel = 1;
            spell.Name = "Tumble";
            spell.CheckSpellName = "VayneTumble";
            spell.MaxRange = 300.0f; // CDragon: VayneQ castRange = 300 (roll distance)
            spell.FixedRange = true;
            spell.Speed = 900.0f; // CDragon: missileSpeed = 900
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // YASUO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Yasuo;
            spell.DangerLevel = 2;
            spell.Name = "SweepingBlade";
            spell.CheckSpellName = "YasuoDashWrapper";
            spell.MaxRange = 475.0f; // CDragon: YasuoDashWrapper castRange = 475
            spell.FixedRange = true;
            spell.Speed = 1000.0f; // CDragon: missileSpeed = 1000
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Yasuo;
            spell.DangerLevel = 3;
            spell.Name = "WindWall";
            spell.CheckSpellName = "YasuoWMovingWall";
            spell.MaxRange = 400.0f; // CDragon: YasuoW castRange = 400
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::WindWall;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // ZILEAN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Zilean;
            spell.DangerLevel = 3;
            spell.Name = "Timewarp";
            spell.CheckSpellName = "ZileanE";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.SpeedArray = { 40.0f, 55.0f, 70.0f, 85.0f, 99.0f };
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // AKALI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Akali;
            spell.DangerLevel = 4;
            spell.Name = "Shadow Dance";
            spell.CheckSpellName = "AkaliShadowDance";
            spell.MaxRange = 700.0f; // CDragon: castRange = 700
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // ALISTAR
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Alistar;
            spell.DangerLevel = 3;
            spell.Name = "Headbutt";
            spell.CheckSpellName = "Pulverize"; // Headbutt is part of combo, uses Pulverize slot
            spell.MaxRange = 650.0f; // CDragon: Headbutt castRange = 650
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions };
            Entries.push_back(spell);
        }

        // ==========================================
        // AMBESSA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Ambessa;
            spell.DangerLevel = 4;
            spell.Name = "Sundering Strike";
            spell.CheckSpellName = "AmbessaE";
            spell.MaxRange = 450.0f;
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Ambessa;
            spell.DangerLevel = 5;
            spell.Name = "Public Execution";
            spell.CheckSpellName = "AmbessaR";
            spell.MaxRange = 800.0f;
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // BEL'VETH
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Belveth;
            spell.DangerLevel = 3;
            spell.Name = "Abyssal Dive";
            spell.CheckSpellName = "BelvethE";
            spell.MaxRange = 350.0f;
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // BRIAR
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Briar;
            spell.DangerLevel = 5;
            spell.Name = "Certain Death";
            spell.CheckSpellName = "BriarR";
            spell.MaxRange = 800.0f;
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions };
            Entries.push_back(spell);
        }

        // ==========================================
        // CAMILLE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Camille;
            spell.DangerLevel = 4;
            spell.Name = "Hookshot";
            spell.CheckSpellName = "CamilleE";
            spell.MaxRange = 800.0f; // CDragon: Hookshot castRange = 800 (grapple range)
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Camille;
            spell.DangerLevel = 5;
            spell.Name = "The Hextech Ultimatum";
            spell.CheckSpellName = "CamilleR";
            spell.MaxRange = 500.0f; // CDragon: CamilleR castRange = 500
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // FIZZ
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Fizz;
            spell.DangerLevel = 4;
            spell.Name = "Playful / Trickster";
            spell.CheckSpellName = "FizzE";
            spell.MaxRange = 400.0f; // CDragon: FizzE castRange = 400 (dash distance)
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // GAREN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Garen;
            spell.DangerLevel = 2;
            spell.Name = "Decisive Strike";
            spell.CheckSpellName = "GarenQ";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.SpeedArray = { 35.0f, 40.0f, 45.0f, 50.0f, 55.0f }; // CDragon: MoveSpeedMod ~30-50%
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // GWEN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Gwen;
            spell.DangerLevel = 3;
            spell.Name = "Hallowed Mist";
            spell.CheckSpellName = "GwenW";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::SpellShield; // Untargetable in mist
            spell.CastTypeValue = CastType::Self;
            spell.IsSpecial = true;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Gwen;
            spell.DangerLevel = 3;
            spell.Name = "Skip 'n Slash";
            spell.CheckSpellName = "GwenE";
            spell.MaxRange = 350.0f; // CDragon: GwenE castRange = 350
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // IRELIA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Irelia;
            spell.DangerLevel = 3;
            spell.Name = "Bladesurge";
            spell.CheckSpellName = "IreliaQ";
            spell.MaxRange = 625.0f; // CDragon: IreliaQ castRange = 625 (dash to target)
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions };
            Entries.push_back(spell);
        }

        // ==========================================
        // JARVAN IV
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::JarvanIV;
            spell.DangerLevel = 4;
            spell.Name = "Dragon Strike";
            spell.CheckSpellName = "JarvanIVQ";
            spell.MaxRange = 770.0f; // CDragon: Dragon Strike castRange = 770
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::JarvanIV;
            spell.DangerLevel = 5;
            spell.Name = "Cataclysm";
            spell.CheckSpellName = "JarvanIVR";
            spell.MaxRange = 625.0f; // CDragon: Cataclysm castRange = 625
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions };
            Entries.push_back(spell);
        }

        // ==========================================
        // JAX
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Jax;
            spell.DangerLevel = 3;
            spell.Name = "Leap Strike";
            spell.CheckSpellName = "JaxQ";
            spell.MaxRange = 700.0f; // CDragon: Leap Strike castRange = 700
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions, SpellTargets::AllyChampions, SpellTargets::AllyMinions };
            Entries.push_back(spell);
        }

        // ==========================================
        // KAI'SA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Kaisa;
            spell.DangerLevel = 3;
            spell.Name = "Supercharge";
            spell.CheckSpellName = "KaisaE";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.SpeedArray = { 40.0f, 50.0f, 60.0f, 70.0f, 80.0f }; // CDragon: MoveSpeedMod
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Kaisa;
            spell.DangerLevel = 5;
            spell.Name = "Killer Instinct";
            spell.CheckSpellName = "KaisaR";
            spell.MaxRange = 1500.0f; // CDragon: Killer Instinct castRange = 1500
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // KAYN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Kayn;
            spell.DangerLevel = 3;
            spell.Name = "Reaping Slash";
            spell.CheckSpellName = "KaynQ";
            spell.MaxRange = 350.0f; // CDragon: Reaping Slash dash distance
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // KINDRED
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Kindred;
            spell.DangerLevel = 3;
            spell.Name = "Dance of Arrows";
            spell.CheckSpellName = "KindredQ";
            spell.MaxRange = 300.0f; // CDragon: Dance of Arrows dash distance
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // KLED
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Kled;
            spell.DangerLevel = 4;
            spell.Name = "Jousting";
            spell.CheckSpellName = "KledE";
            spell.MaxRange = 500.0f; // CDragon: Jousting castRange = 500
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions };
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Kled;
            spell.DangerLevel = 5;
            spell.Name = "Chaaaaaaaarge!!!";
            spell.CheckSpellName = "KledR";
            spell.MaxRange = 900.0f; // CDragon: Chaaaaaaaarge!!! castRange = 900
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // K'SANTE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::KSante;
            spell.DangerLevel = 3;
            spell.Name = "Footwork";
            spell.CheckSpellName = "KSanteE";
            spell.MaxRange = 400.0f; // CDragon: Footwork castRange = 400
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // LEONA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Leona;
            spell.DangerLevel = 4;
            spell.Name = "Zenith Blade";
            spell.CheckSpellName = "LeonaE";
            spell.MaxRange = 875.0f; // CDragon: Zenith Blade castRange = 875
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions };
            Entries.push_back(spell);
        }

        // ==========================================
        // LILLIA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Lillia;
            spell.DangerLevel = 3;
            spell.Name = "Watch Out! Eep!";
            spell.CheckSpellName = "LilliaW";
            spell.MaxRange = 350.0f; // CDragon: LilliaW dash distance
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // MASTER YI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::MasterYi;
            spell.DangerLevel = 4;
            spell.Name = "Alpha Strike";
            spell.CheckSpellName = "AlphaStrike";
            spell.MaxRange = 600.0f; // CDragon: Alpha Strike castRange = 600
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Blink;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions };
            spell.Untargetable = true;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::MasterYi;
            spell.DangerLevel = 3;
            spell.Name = "Highlander";
            spell.CheckSpellName = "MasterYiR";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.SpeedArray = { 35.0f, 45.0f, 55.0f, 65.0f, 75.0f }; // CDragon: MoveSpeedMod 25-45% + immune to slows
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // MONKEYKING (Wukong)
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::MonkeyKing;
            spell.DangerLevel = 3;
            spell.Name = "Nimbus Strike";
            spell.CheckSpellName = "MonkeyKingNimbusKick";
            spell.MaxRange = 650.0f; // CDragon: Nimbus Strike castRange = 650
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions };
            Entries.push_back(spell);
        }

        // ==========================================
        // NAAFIRI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Naafiri;
            spell.DangerLevel = 3;
            spell.Name = "Hounds' Quest";
            spell.CheckSpellName = "NaafiriW";
            spell.MaxRange = 800.0f; // CDragon: NaafiriW castRange = 800
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions };
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Naafiri;
            spell.DangerLevel = 5;
            spell.Name = "We Are More";
            spell.CheckSpellName = "NaafiriR";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.SpeedArray = { 50.0f, 60.0f, 70.0f }; // CDragon: MoveSpeedMod
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // NEEKO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Neeko;
            spell.DangerLevel = 3;
            spell.Name = "Shapesplitter";
            spell.CheckSpellName = "NeekoW";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.SpeedArray = { 20.0f, 25.0f, 30.0f, 35.0f, 40.0f }; // CDragon: MoveSpeedMod
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // NILAH
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Nilah;
            spell.DangerLevel = 4;
            spell.Name = "Apotheosis";
            spell.CheckSpellName = "NilahR";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::SpellShield; // Briefly untargetable during ult
            spell.CastTypeValue = CastType::Self;
            spell.IsSpecial = true;
            Entries.push_back(spell);
        }

        // ==========================================
        // POPPY
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Poppy;
            spell.DangerLevel = 4;
            spell.Name = "Heroic Charge";
            spell.CheckSpellName = "PoppyE";
            spell.MaxRange = 525.0f; // CDragon: Heroic Charge castRange = 525
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions };
            Entries.push_back(spell);
        }

        // ==========================================
        // PYKE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Pyke;
            spell.DangerLevel = 4;
            spell.Name = "Phantom Undertow";
            spell.CheckSpellName = "PykeE";
            spell.MaxRange = 550.0f; // CDragon: Phantom Undertow dash distance
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // QIYANA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Qiyana;
            spell.DangerLevel = 4;
            spell.Name = "Edge of Ixtal";
            spell.CheckSpellName = "QiyanaE";
            spell.MaxRange = 650.0f; // CDragon: Edge of Ixtal castRange = 650
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions };
            Entries.push_back(spell);
        }

        // ==========================================
        // RAKAN
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Rakan;
            spell.DangerLevel = 4;
            spell.Name = "Grand Entrance";
            spell.CheckSpellName = "RakanW";
            spell.MaxRange = 600.0f; // CDragon: Grand Entrance castRange = 600
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Rakan;
            spell.DangerLevel = 3;
            spell.Name = "Battle Dance";
            spell.CheckSpellName = "RakanE";
            spell.MaxRange = 500.0f; // CDragon: Battle Dance castRange = 500
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::AllyChampions };
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Rakan;
            spell.DangerLevel = 5;
            spell.Name = "The Quickness";
            spell.CheckSpellName = "RakanR";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.SpeedArray = { 50.0f, 60.0f, 70.0f }; // CDragon: MoveSpeedMod
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // RAMMUS
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Rammus;
            spell.DangerLevel = 3;
            spell.Name = "Powerball";
            spell.CheckSpellName = "RammusW"; // Powerball is actually Q slot but named RammusW in old data
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.SpeedArray = { 25.0f, 30.0f, 35.0f, 40.0f, 45.0f }; // CDragon: MoveSpeedMod
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // REK'SAI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::RekSai;
            spell.DangerLevel = 4;
            spell.Name = "Burrowed Unburrow";
            spell.CheckSpellName = "RekSaiE";
            spell.MaxRange = 250.0f; // CDragon: Unburrow dash distance
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::RekSai;
            spell.DangerLevel = 5;
            spell.Name = "Void Rush";
            spell.CheckSpellName = "RekSaiR";
            spell.MaxRange = 1500.0f; // CDragon: Void Rush castRange = 1500
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions };
            Entries.push_back(spell);
        }

        // ==========================================
        // RELL
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Rell;
            spell.DangerLevel = 4;
            spell.Name = "Ferromancy: Mount Up";
            spell.CheckSpellName = "RellW";
            spell.MaxRange = 400.0f; // CDragon: RellW dash distance (mounting)
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // RENATA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::RenataGlasc;
            spell.DangerLevel = 3;
            spell.Name = "Loyalty Program";
            spell.CheckSpellName = "RenataE";
            spell.MaxRange = 800.0f; // CDragon: Loyalty Program castRange = 800
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Shield;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::AllyChampions };
            Entries.push_back(spell);
        }

        // ==========================================
        // SAMIRA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Samira;
            spell.DangerLevel = 3;
            spell.Name = "Wild Rush";
            spell.CheckSpellName = "SamiraE";
            spell.MaxRange = 600.0f; // CDragon: Wild Rush castRange = 600
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions, SpellTargets::AllyChampions };
            Entries.push_back(spell);
        }

        // ==========================================
        // SEJUANI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Sejuani;
            spell.DangerLevel = 4;
            spell.Name = "Arctic Assault";
            spell.CheckSpellName = "SejuaniQ";
            spell.MaxRange = 650.0f; // CDragon: Arctic Assault castRange = 650
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // SERAPHINE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Seraphine;
            spell.DangerLevel = 3;
            spell.Name = "Surround Sound";
            spell.CheckSpellName = "SeraphineE";
            spell.MaxRange = 900.0f; // CDragon: Surround Sound castRange = 900
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.SpeedArray = { 20.0f, 25.0f, 30.0f, 35.0f, 40.0f }; // CDragon: MoveSpeedMod on allies
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // SETT
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Sett;
            spell.DangerLevel = 5;
            spell.Name = "The Show Stopper";
            spell.CheckSpellName = "SettR";
            spell.MaxRange = 500.0f; // CDragon: The Show Stopper castRange = 500
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions };
            Entries.push_back(spell);
        }

        // ==========================================
        // SINGED
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Singed;
            spell.DangerLevel = 2;
            spell.Name = "Insanity Potion";
            spell.CheckSpellName = "SingedR";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.SpeedArray = { 20.0f, 30.0f, 40.0f, 50.0f, 60.0f }; // CDragon: MoveSpeedMod
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // SMOLDER
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Smolder;
            spell.DangerLevel = 3;
            spell.Name = "Flap, Flap, Flap";
            spell.CheckSpellName = "SmolderE";
            spell.MaxRange = 350.0f; // CDragon: Flap Flap Flap dash distance
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // SONA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Sona;
            spell.DangerLevel = 2;
            spell.Name = "Song of Celerity";
            spell.CheckSpellName = "SonaE";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.SpeedArray = { 13.0f, 14.0f, 15.0f, 16.0f, 17.0f }; // CDragon: MoveSpeedMod
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // SYLAS
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Sylas;
            spell.DangerLevel = 4;
            spell.Name = "Abduct";
            spell.CheckSpellName = "SylasE";
            spell.MaxRange = 750.0f; // CDragon: Abduct castRange = 750 (second cast dash)
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // TAHM KENCH
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::TahmKench;
            spell.DangerLevel = 4;
            spell.Name = "Abyssal Dive";
            spell.CheckSpellName = "TahmKenchR";
            spell.MaxRange = 250.0f; // CDragon: Abyssal Dive dash distance
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // TALIYAH
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Taliyah;
            spell.DangerLevel = 4;
            spell.Name = "Seismic Shove";
            spell.CheckSpellName = "TaliyahW";
            spell.MaxRange = 900.0f; // CDragon: Seismic Shove castRange = 900
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::Dash; // Taliyah W can knock back/dash
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // UDYR
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Udyr;
            spell.DangerLevel = 3;
            spell.Name = "Wingborne Storm";
            spell.CheckSpellName = "UdyrE";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.SpeedArray = { 20.0f, 25.0f, 30.0f, 35.0f, 40.0f }; // CDragon: MoveSpeedMod
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }

        // ==========================================
        // VI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Vi;
            spell.DangerLevel = 4;
            spell.Name = "Vault Breaker";
            spell.CheckSpellName = "ViQ";
            spell.MaxRange = 750.0f; // CDragon: Vault Breaker castRange = 750
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Vi;
            spell.DangerLevel = 5;
            spell.Name = "Assault and Battery";
            spell.CheckSpellName = "ViR";
            spell.MaxRange = 800.0f; // CDragon: Assault and Battery castRange = 800
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions };
            Entries.push_back(spell);
        }

        // ==========================================
        // VIEGO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Viego;
            spell.DangerLevel = 4;
            spell.Name = "Spectral Maw";
            spell.CheckSpellName = "ViegoW";
            spell.MaxRange = 300.0f; // CDragon: Spectral Maw dash distance
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Viego;
            spell.DangerLevel = 5;
            spell.Name = "Harrowed Path";
            spell.CheckSpellName = "ViegoR";
            spell.MaxRange = 600.0f; // CDragon: Harrowed Path dash distance
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // VOLIBEAR
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Volibear;
            spell.DangerLevel = 3;
            spell.Name = "Thundering Smite";
            spell.CheckSpellName = "VolibearQ";
            spell.Delay = 250.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.SpeedArray = { 10.0f, 15.0f, 20.0f, 25.0f, 30.0f }; // CDragon: MoveSpeedMod
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Self;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Volibear;
            spell.DangerLevel = 5;
            spell.Name = "Stormbringer";
            spell.CheckSpellName = "VolibearR";
            spell.MaxRange = 700.0f; // CDragon: Stormbringer castRange = 700 (dash to target location)
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // XAYAH
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Xayah;
            spell.DangerLevel = 5;
            spell.Name = "Featherstorm";
            spell.CheckSpellName = "XayahR";
            spell.MaxRange = 600.0f; // CDragon: Featherstorm leap distance
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            spell.Untargetable = true;
            Entries.push_back(spell);
        }

        // ==========================================
        // XIN ZHAO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::XinZhao;
            spell.DangerLevel = 3;
            spell.Name = "Audacious Charge";
            spell.CheckSpellName = "XinZhaoE";
            spell.MaxRange = 600.0f; // CDragon: Audacious Charge castRange = 600
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::EnemyChampions, SpellTargets::EnemyMinions };
            Entries.push_back(spell);
        }

        // ==========================================
        // YONE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Yone;
            spell.DangerLevel = 4;
            spell.Name = "Soul Unbound";
            spell.CheckSpellName = "YoneE";
            spell.MaxRange = 300.0f; // CDragon: Soul Unbound dash distance
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Yone;
            spell.DangerLevel = 5;
            spell.Name = "Fate Sealed";
            spell.CheckSpellName = "YoneR";
            spell.MaxRange = 1000.0f; // CDragon: Fate Sealed castRange = 1000
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // YUUMI
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Yuumi;
            spell.DangerLevel = 3;
            spell.Name = "You and Me!";
            spell.CheckSpellName = "YuumiW";
            spell.MaxRange = 1000.0f; // CDragon: You and Me! castRange = 1000 (attach to ally)
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::W;
            spell.EvadeTypeValue = EvadeType::Blink;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::AllyChampions };
            Entries.push_back(spell);
        }

        // ==========================================
        // ZOE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Zoe;
            spell.DangerLevel = 4;
            spell.Name = "Portal Jump";
            spell.CheckSpellName = "ZoeR";
            spell.MaxRange = 575.0f; // CDragon: Portal Jump castRange = 575
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Blink;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // AURORA
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Aurora;
            spell.DangerLevel = 3;
            spell.Name = "Across the Divide";
            spell.CheckSpellName = "AuroraE";
            spell.MaxRange = 400.0f; // CDragon: AuroraE dash distance
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // MILIO
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Milio;
            spell.DangerLevel = 3;
            spell.Name = "Warm Hugs";
            spell.CheckSpellName = "MilioE";
            spell.MaxRange = 800.0f; // CDragon: Warm Hugs castRange = 800
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.SpeedArray = { 15.0f, 20.0f, 25.0f, 30.0f, 35.0f }; // CDragon: MoveSpeedMod on ally
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.CastTypeValue = CastType::Target;
            spell.ValidTargets = { SpellTargets::AllyChampions };
            Entries.push_back(spell);
        }

        // ==========================================
        // LOCKE
        // ==========================================
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Locke;
            spell.DangerLevel = 4;
            spell.Name = "LockeE";
            spell.CheckSpellName = "LockeE";
            spell.MaxRange = 400.0f;
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::E;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.ChampionId = SDK::ChampionId::Locke;
            spell.DangerLevel = 5;
            spell.Name = "LockeR";
            spell.CheckSpellName = "LockeR";
            spell.MaxRange = 600.0f;
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Dash;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }

        // ==========================================
        // ALL CHAMPIONS (Items + Summoner Spells)
        // ==========================================
        {
            EvadeSpellData spell;
            spell.IsGlobal = true;
            spell.DangerLevel = 3;
            spell.Name = "Talisman of Ascension";
            spell.CheckSpellName = "TalismanOfAscension";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.SpeedArray = { 40.0f, 40.0f, 40.0f, 40.0f, 40.0f };
            spell.CastTypeValue = CastType::Self;
            spell.IsItem = true;
            spell.ItemId = 3060; // Talisman of Ascension
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.IsGlobal = true;
            spell.DangerLevel = 3;
            spell.Name = "Youmuu's Ghostblade";
            spell.CheckSpellName = "YoumuusGhostblade";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::MovementSpeedBuff;
            spell.SpeedArray = { 20.0f, 20.0f, 20.0f, 20.0f, 20.0f };
            spell.CastTypeValue = CastType::Self;
            spell.IsItem = true;
            spell.ItemId = 3142; // Youmuu's Ghostblade
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.IsGlobal = true;
            spell.DangerLevel = 4;
            spell.Name = "Flash";
            spell.CheckSpellName = "SummonerFlash";
            spell.MaxRange = 400.0f; // CDragon: SummonerFlash castRange = 400
            spell.FixedRange = true;
            spell.Delay = 50.0f;
            spell.IsSummonerSpell = true;
            spell.Slot = SDK::SpellSlot::R;
            spell.EvadeTypeValue = EvadeType::Blink;
            spell.CastTypeValue = CastType::Position;
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.IsGlobal = true;
            spell.DangerLevel = 4;
            spell.Name = "Hourglass";
            spell.CheckSpellName = "ZhonyasHourglass";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::SpellShield; // Invulnerability
            spell.CastTypeValue = CastType::Self;
            spell.IsItem = true;
            spell.ItemId = 3157; // Zhonya's Hourglass
            Entries.push_back(spell);
        }
        {
            EvadeSpellData spell;
            spell.IsGlobal = true;
            spell.DangerLevel = 4;
            spell.Name = "Witchcap";
            spell.CheckSpellName = "Witchcap";
            spell.Delay = 50.0f;
            spell.Slot = SDK::SpellSlot::Q;
            spell.EvadeTypeValue = EvadeType::SpellShield; // Invulnerability
            spell.CastTypeValue = CastType::Self;
            spell.IsItem = true;
            spell.ItemId = 3159; // Wooglet's Witchcap
            Entries.push_back(spell);
        }
    }

    static const std::vector<EvadeSpellData>& Spells() {
        static bool initialized = false;
        if (!initialized) {
            Initialize();
            initialized = true;
        }
        return Entries;
    }

    static std::vector<const EvadeSpellData*> ForChampion(
            SDK::ChampionId championId,
            bool includeGlobal = true) {
        std::vector<const EvadeSpellData*> result;
        for (const EvadeSpellData& spell : Spells()) {
            if ((includeGlobal && spell.IsGlobal) ||
                (!spell.IsGlobal && spell.ChampionId == championId &&
                 championId != SDK::ChampionId::Unknown)) {
                result.push_back(&spell);
            }
        }
        return result;
    }
};

inline std::vector<EvadeSpellData> EvadeSpellDatabase::Entries;

} // namespace Plugins::KuroEvade::Database
