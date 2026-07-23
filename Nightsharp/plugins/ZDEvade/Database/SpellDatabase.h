#pragma once
#include "SpellData.h"

#include <cmath>

namespace ZDEvade { class SpellDatabase {
public:
    static std::vector<ZDEvade::SpellData> Spells;
    static int InvalidConeSpellCount() { return invalidConeSpellCount_; }
    static int InvalidArcSpellCount() { return invalidArcSpellCount_; }
    static int SupportedArcSpellCount() { return supportedArcSpellCount_; }

    static void Initialize() {
        if (!Spells.empty()) return;
        invalidConeSpellCount_ = 0;
        invalidArcSpellCount_ = 0;
        supportedArcSpellCount_ = 0;

        // === AllChampions ===
        {
            SpellData spell;
            spell.charName = "AllChampions";
            spell.dangerlevel = 1;
            spell.missileName = "summonersnowball";
            spell.name = "Mark";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 60.0f;
            spell.range = 1600.0f;
            spell.spellDelay = 0;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "summonersnowball";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraSpellNames = { "summonerporothrow" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Aatrox ===
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.dangerlevel = 3;
            spell.name = "The Darkin Blade (Cast 1)";
            spell.radius = 90.0f;
            spell.range = 625.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AatroxQ1";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.dangerlevel = 3;
            spell.name = "The Darkin Blade (Cast 2)";
            spell.range = 525.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AatroxQ2";
            spell.spellType = ZDSpellType::Cone;
            spell.coneAngleDegrees = 60.0f;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.dangerlevel = 3;
            spell.name = "The Darkin Blade (Cast 3)";
            spell.radius = 300.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AatroxQ3";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aatrox";
            spell.dangerlevel = 2;
            spell.missileName = "AatroxW";
            spell.name = "Infernal Chains";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 80.0f;
            spell.range = 825.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "AatroxW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // === Ahri ===
        {
            SpellData spell;
            spell.charName = "Ahri";
            spell.dangerlevel = 2;
            spell.missileName = "AhriQMissile";
            spell.name = "Orb of Deception";
            spell.projectileSpeed = 2500.0f;
            spell.radius = 100.0f;
            spell.range = 970.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AhriQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraSpellNames = { "AhriOrbofDeception" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ahri";
            spell.dangerlevel = 2;
            spell.missileName = "AhriOrbReturn";
            spell.extraMissileNames = { "AhriQReturnMissile" };
            spell.name = "Orb of Deception (Return)";
            spell.projectileSpeed = 915.0f;
            spell.radius = 100.0f;
            spell.range = 925.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AhriOrbofDeception2";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ahri";
            spell.dangerlevel = 3;
            spell.missileName = "AhriEMissile";
            spell.name = "Charm";
            spell.projectileSpeed = 1550.0f;
            spell.radius = 60.0f;
            spell.range = 975.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "AhriE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Charm;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // === Akali ===
        {
            SpellData spell;
            spell.charName = "Akali";
            spell.dangerlevel = 2;
            spell.missileName = "AkaliQMis";
            spell.name = "Five Point Strike";
            spell.projectileSpeed = 3200.0f;
            spell.radius = 60.0f;
            spell.range = 550.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AkaliQ";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::Slow;
            spell.coneAngleDegrees = 45.0f;
            spell.extraMissileNames = { "AkaliQMis0", "AkaliQMis1", "AkaliQMis2", "AkaliQMis3", "AkaliQMis4", "AkaliQMis5" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Akali";
            spell.dangerlevel = 3;
            spell.missileName = "AkaliEMis";
            spell.name = "Shuriken Flip";
            spell.projectileSpeed = 1900.0f;
            spell.radius = 60.0f;
            spell.range = 825.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "AkaliE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Akshan ===
        {
            SpellData spell;
            spell.charName = "Akshan";
            spell.dangerlevel = 2;
            spell.missileName = "AkshanQMissile";
            spell.name = "Avengerang";
            spell.projectileSpeed = 2400.0f;
            spell.radius = 60.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AkshanQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "AkshanQMissileReturn" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Alistar";
            spell.dangerlevel = 3;
            spell.name = "Pulverize";
            spell.radius = 375.0f;
            spell.range = 375.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "Pulverize";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp;
            spell.useEndPosition = true;
            Spells.push_back(spell);
        }

        // === Ambessa ===
        {
            SpellData spell;
            spell.charName = "Ambessa";
            spell.dangerlevel = 2;
            spell.name = "Cunning Sweep / Sundering Slam";
            spell.radius = 100.0f;
            spell.range = 650.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AmbessaQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ambessa";
            spell.dangerlevel = 2;
            spell.name = "Lacerate";
            spell.radius = 100.0f;
            spell.range = 325.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "AmbessaE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ambessa";
            spell.dangerlevel = 4;
            spell.name = "Public Execution";
            spell.radius = 100.0f;
            spell.range = 1250.0f;
            spell.spellDelay = 699;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "AmbessaR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Suppression;
            Spells.push_back(spell);
        }

        // === Amumu ===
        {
            SpellData spell;
            spell.charName = "Amumu";
            spell.dangerlevel = 3;
            spell.missileName = "SadMummyBandageToss";
            spell.name = "Bandage Toss";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 80.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "BandageToss";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Amumu";
            spell.dangerlevel = 4;
            spell.name = "Curse of the Sad Mummy";
            spell.radius = 560.0f;
            spell.range = 560.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "CurseoftheSadMummy";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }

        // === Anivia ===
        {
            SpellData spell;
            spell.charName = "Anivia";
            spell.dangerlevel = 3;
            spell.missileName = "FlashFrostSpell";
            spell.name = "Flash Frost";
            spell.projectileSpeed = 850.0f;
            spell.radius = 110.0f;
            spell.range = 1250.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "FlashFrostSpell";
            spell.extraSpellNames = { "FlashFrost" };
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }


        // === Annie ===
        {
            SpellData spell;
            spell.charName = "Annie";
            spell.dangerlevel = 2;
            spell.name = "Incinerate";
            spell.range = 625.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "Incinerate";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::Stun;
            spell.coneAngleDegrees = 25.0f;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Annie";
            spell.dangerlevel = 3;
            spell.name = "Summon: Tibbers";
            spell.radius = 250.0f;
            spell.range = 600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "AnnieR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }

        // === Aphelios ===
	//MissQ
        {
            SpellData spell;
            spell.charName = "Aphelios";
            spell.dangerlevel = 2;
            spell.missileName = "ApheliosCalibrumQMis";
            spell.name = "Moonshot";
            spell.projectileSpeed = 1850.0f;
            spell.radius = 60.0f;
            spell.range = 1450.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ApheliosCalibrumQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Aphelios";
            spell.dangerlevel = 3;
            spell.missileName = "ApheliosRMis";
            spell.name = "Moonlight Vigil";
            spell.projectileSpeed = 2050.0f;
            spell.radius = 125.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "ApheliosR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Ashe ===
        {
            SpellData spell;
            spell.charName = "Ashe";
            spell.dangerlevel = 4;
            spell.missileName = "EnchantedCrystalArrow";
            spell.name = "Enchanted Crystal Arrow";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 130.0f;
            spell.range = 25000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "EnchantedCrystalArrow";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyYasuoWall };
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 400.0f;
            spell.endExplosionRequiresUnitCollision = true;
            spell.endExplosionAtUnitCenter = true;
            Spells.push_back(spell);
        }

        // === AurelionSol ===
	//thiếu E
        {
            SpellData spell;
            spell.charName = "AurelionSol";
            spell.dangerlevel = 4;
            spell.name = "Falling Star / The Skies Descend";
            spell.radius = 350.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 1250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "AurelionSolR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }


        // === Aurora ===
        {
            SpellData spell;
            spell.charName = "Aurora";
            spell.dangerlevel = 2;
            spell.missileName = "AuroraQ";
            spell.name = "Twofold Hex";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 90.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AuroraQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "AuroraQReturnMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Aurora";
            spell.dangerlevel = 2;
            spell.name = "The Weirding";
            spell.projectileSpeed = 150.0f;
            spell.radius = 80.0f;
            spell.range = 825.0f;
            spell.spellDelay = 349;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "AuroraE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }


        // === Azir ===
        {
            SpellData spell;
            spell.charName = "Azir";
            spell.dangerlevel = 2;
            spell.name = "Conquering Sands";
            spell.radius = 150.0f;
            spell.range = 740.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "AzirQWrapper";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Azir";
            spell.dangerlevel = 4;
            spell.name = "Emperor's Divide";
            spell.projectileSpeed = 1000.0f;
            spell.radius = 125.0f;
            spell.range = 250.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "AzirR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockBack;
            Spells.push_back(spell);
        }

        // === Bard ===
        {
            SpellData spell;
            spell.charName = "Bard";
            spell.dangerlevel = 3;
            spell.missileName = "BardQMissile";
            spell.name = "Cosmic Binding";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 60.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "BardQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.extraMissileNames = { "BardQMissile2" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Bard";
            spell.dangerlevel = 3;
            spell.missileName = "BardRMissileFixedTravelTime";
            spell.name = "Tempered Fate";
            spell.projectileSpeed = 2100.0f;
            spell.radius = 350.0f;
            spell.range = 3400.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "BardR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun;
            spell.extraMissileNames = { "BardRMissileRange1", "BardRMissileRange2", "BardRMissileRange3", "BardRMissileRange4", "BardRMissileRange5" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Belveth ===

        {
            SpellData spell;
            spell.charName = "Belveth";
            spell.dangerlevel = 2;
            spell.missileName = "BelvethW";
            spell.name = "Above and Below";
            spell.radius = 100.0f;
            spell.range = 715.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "BelvethW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Blitzcrank ===
        {
            SpellData spell;
            spell.charName = "Blitzcrank";
            spell.dangerlevel = 3;
            spell.missileName = "RocketGrabMissile";
            spell.name = "Rocket Grab";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 70.0f;
            spell.range = 1079.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "RocketGrab";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Brand ===
        {
            SpellData spell;
            spell.charName = "Brand";
            spell.dangerlevel = 3;
            spell.missileName = "BrandQMissile";
            spell.name = "Sear";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 60.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "BrandQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Brand";
            spell.dangerlevel = 2;
            spell.name = "Pillar of Flame";
            spell.radius = 240.0f;
            spell.range = 900.0f;
            spell.spellDelay = 877;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "BrandW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        // === Braum ===
        {
            SpellData spell;
            spell.charName = "Braum";
            spell.dangerlevel = 2;
            spell.missileName = "BraumQMissile";
            spell.name = "Winter's Bite";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 60.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "BraumQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Braum";
            spell.dangerlevel = 4;
            spell.missileName = "BraumRMissile";
            spell.name = "Glacial Fissure";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 115.0f;
            spell.range = 1250.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "BraumRWrapper";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Briar ===

        {
            SpellData spell;
            spell.charName = "Briar";
            spell.dangerlevel = 3;
            spell.missileName = "BriarEMis";
            spell.name = "Chilling Scream";
            spell.projectileSpeed = 1900.0f;
            spell.radius = 190.0f;
            spell.range = 600.0f;
            spell.spellDelay = 1000;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "BriarE";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::KnockUp;
            spell.coneAngleDegrees = 40.0f;
            spell.extraMissileNames = { "BriarEMisStrong" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Briar";
            spell.dangerlevel = 3;
            spell.missileName = "BriarR";
            spell.name = "Certain Death";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 160.0f;
            spell.range = 12000.0f;
            spell.spellDelay = 1000;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "BriarR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Fear;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Caitlyn ===
        {
            SpellData spell;
            spell.charName = "Caitlyn";
            spell.dangerlevel = 2;
            spell.missileName = "CaitlynQ";
            spell.name = "Piltover Peacemaker (Cast 1)";
            spell.projectileSpeed = 2200.0f;
            spell.radius = 60.0f;
            spell.range = 1250.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "CaitlynQ1";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Caitlyn";
            spell.dangerlevel = 2;
            spell.missileName = "CaitlynQ";
            spell.name = "Piltover Peacemaker (Cast 2)";
            spell.projectileSpeed = 2200.0f;
            spell.radius = 60.0f;
            spell.range = 1250.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "CaitlynQ2";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Caitlyn";
            spell.dangerlevel = 3;
            spell.name = "Yordle Snap Trap";
            spell.radius = 75.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "CaitlynW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Snare;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Caitlyn";
            spell.dangerlevel = 3;
            spell.missileName = "CaitlynEMissile";
            spell.name = "90 Caliber Net";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 70.0f;
            spell.range = 750.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "CaitlynE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Camille ===

        {
            SpellData spell;
            spell.charName = "Camille";
            spell.dangerlevel = 2;
            spell.name = "Tactical Sweep";
            spell.range = 650.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "CamilleW";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::Slow;
            spell.coneAngleDegrees = 45.0f;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Camille";
            spell.dangerlevel = 3;
            spell.missileName = "CamilleELeftMissile";
            spell.name = "Hookshot";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 30.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "CamilleE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.extraMissileNames = { "CamilleEMissile", "CamilleERightMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Cassiopeia ===
        {
            SpellData spell;
            spell.charName = "Cassiopeia";
            spell.dangerlevel = 2;
            spell.name = "Noxious Blast";
            spell.radius = 160.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "CassiopeiaQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Cassiopeia";
            spell.dangerlevel = 2;
            spell.missileName = "CassiopeiaWMissile";
            spell.name = "Miasma";
            spell.projectileSpeed = 3000.0f;
            spell.radius = 25.0f;
            spell.range = 700.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "CassiopeiaW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
	//Cassiopeia R thiếu
        {
            SpellData spell;
            spell.charName = "Cassiopeia";
            spell.dangerlevel = 4;
            spell.name = "Petrifying Gaze";
            spell.range = 825.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "CassiopeiaR";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::Stun;
            spell.coneAngleDegrees = 80.0f;
            Spells.push_back(spell);
        }

        // === Chogath ===
        {
            SpellData spell;
            spell.charName = "Chogath";
            spell.dangerlevel = 2;
            spell.name = "Rupture";
            spell.radius = 230.0f;
            spell.range = 950.0f;
            spell.spellDelay = 877;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "Rupture";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
	//Chogath thiếu W
        {
            SpellData spell;
            spell.charName = "Chogath";
            spell.dangerlevel = 2;
            spell.name = "Feral Scream";
            spell.range = 650.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "FeralScream";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::Silence;
            spell.coneAngleDegrees = 60.0f;
            Spells.push_back(spell);
        }

        // === Corki ===
        {
            SpellData spell;
            spell.charName = "Corki";
            spell.dangerlevel = 2;
            spell.missileName = "PhosphorusBombMissile";
            spell.name = "Phosphorus Bomb";
            spell.projectileSpeed = 1100.0f;
            spell.radius = 250.0f;
            spell.range = 825.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "PhosphorusBomb";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "PhosphorusBombMissileMin" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Corki";
            spell.dangerlevel = 3;
            spell.missileName = "MissileBarrageMissile";
            spell.name = "Missile Barrage";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 40.0f;
            spell.range = 1225.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "MissileBarrage";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "MissileBarrageMissile2" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Darius ===
	//thiếu Darius Q
        {
            SpellData spell;
            spell.charName = "Darius";
            spell.dangerlevel = 3;
            spell.name = "Apprehend";
            spell.range = 535.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "DariusE";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::KnockUp;
            spell.coneAngleDegrees = 50.0f;
            Spells.push_back(spell);
        }

        // === Diana ===
        {
            SpellData spell;
            spell.charName = "Diana";
            spell.dangerlevel = 2;
            spell.missileName = "DianaQInnerMissile";
            spell.name = "Crescent Strike";
            spell.projectileSpeed = 2100.0f;
            spell.radius = 70.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "DianaQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "DianaQOuterMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
	//thiếu DianaArcArc

        // === DrMundo ===
        {
            SpellData spell;
            spell.charName = "DrMundo";
            spell.dangerlevel = 2;
            spell.missileName = "DrMundoQ";
            spell.name = "Infected Bonesaw";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 60.0f;
            spell.range = 975.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "DrMundoQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Draven ===

        {
            SpellData spell;
            spell.charName = "Draven";
            spell.dangerlevel = 2;
            spell.missileName = "DravenDoubleShotMissile";
            spell.name = "Stand Aside";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 130.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "DravenDoubleShot";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Draven";
            spell.dangerlevel = 3;
            spell.name = "Whirling Death";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 299.29998779296875f;
            spell.range = 20000.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "DravenRCast";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
	    spell.fixedRange = true;
            Spells.push_back(spell);
        }

        // === Ekko ===
        {
            SpellData spell;
            spell.charName = "Ekko";
            spell.dangerlevel = 2;
            spell.missileName = "EkkoQMis";
            spell.name = "Timewinder";
            spell.projectileSpeed = 1650.0f;
            spell.radius = 60.0f;
            spell.range = 1075.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "EkkoQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ekko";
            spell.dangerlevel = 3;
            spell.name = "Parallel Convergence";
            spell.radius = 375.0f;
            spell.range = 1600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "EkkoW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }


        // === Elise ===

        {
            SpellData spell;
            spell.charName = "Elise";
            spell.dangerlevel = 3;
            spell.name = "Cocoon / Rappel";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 55.0f;
            spell.range = 1075.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "EliseHumanE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }

        // === Evelynn ===
        {
            SpellData spell;
            spell.charName = "Evelynn";
            spell.dangerlevel = 2;
            spell.missileName = "EvelynnQ";
            spell.name = "Hate Spike";
            spell.projectileSpeed = 2400.0f;
            spell.radius = 90.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "EvelynnQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "EvelynnQDebuffCircleMissile", "EvelynnQLineMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Evelynn";
            spell.dangerlevel = 3;
            spell.name = "Last Caress";
            spell.radius = 350.0f;
            spell.range = 500.0f;
            spell.spellDelay = 349;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "EvelynnR";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::None;
            spell.coneAngleDegrees = 180.0f;
            Spells.push_back(spell);
        }

        // === Ezreal ===
        {
            SpellData spell;
            spell.charName = "Ezreal";
            spell.dangerlevel = 2;
            spell.missileName = "EzrealQ";
            spell.name = "Mystic Shot";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 60.0f;
            spell.range = 1150.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "EzrealQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ezreal";
            spell.dangerlevel = 2;
            spell.missileName = "EzrealW";
            spell.name = "Essence Flux";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 80.0f;
            spell.range = 1150.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "EzrealW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Ezreal";
            spell.dangerlevel = 3;
            spell.missileName = "EzrealR";
            spell.name = "Trueshot Barrage";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 160.0f;
            spell.range = 25000.0f;
            spell.spellDelay = 1000;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "EzrealTrueshotBarrage";
            spell.extraSpellNames = { "EzrealR" };
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        // === Fiddlesticks ===

        {
            SpellData spell;
            spell.charName = "Fiddlesticks";
            spell.dangerlevel = 2;
            spell.missileName = "FiddleSticksE";
            spell.name = "Reap";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 70.0f;
            spell.range = 850.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "FiddlesticksE";
            spell.extraSpellNames = { "FiddleSticksE" };
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Silence;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Fiddlesticks";
            spell.dangerlevel = 3;
            spell.name = "Crowstorm";
            spell.radius = 600.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "FiddleSticksR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        // === Fiora ===
        {
            SpellData spell;
            spell.charName = "Fiora";
            spell.dangerlevel = 3;
            spell.missileName = "FioraWMissile";
            spell.name = "Riposte";
            spell.projectileSpeed = 3200.0f;
            spell.radius = 70.0f;
            spell.range = 750.0f;
            spell.spellDelay = 9;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "FioraW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.extraMissileNames = { "FioraWMissile2" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Fizz ===

        {
            SpellData spell;
            spell.charName = "Fizz";
            spell.dangerlevel = 4;
            spell.missileName = "FizzRMissile";
            spell.name = "Chum the Waters";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 300.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "FizzR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Galio ===
        {
            SpellData spell;
            spell.charName = "Galio";
            spell.dangerlevel = 2;
            spell.missileName = "GalioQMissile";
            spell.name = "Winds of War";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 60.0f;
            spell.range = 825.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "GalioQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            spell.extraMissileNames = {
                "GalioQMissileR",
                "GalioQMissileIn"
            };
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 225.0f;
            spell.endExplosionDelay = 0;
            spell.endExplosionDuration = 2000;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Galio";
            spell.dangerlevel = 3;
            spell.name = "Justice Punch";
            spell.radius = 200.0f;
            spell.range = 650.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "GalioE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }

        // === Gangplank ===
        {
            SpellData spell;
            spell.charName = "Gangplank";
            spell.dangerlevel = 2;
            spell.missileName = "GangplankEBarrelFuseMissile";
            spell.name = "Powder Keg";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 325.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "GangplankE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Gnar ===
	//sửa Q chia ra làm 3 loại Q
        {
            SpellData spell;
            spell.charName = "Gnar";
            spell.dangerlevel = 2;
            spell.missileName = "GnarQMissile";
            spell.name = "Boomerang Throw / Boulder Toss";
            spell.projectileSpeed = 2500.0f;
            spell.radius = 75.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "GnarQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.extraMissileNames = { "GnarQMissileReturn" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gnar";
            spell.dangerlevel = 2;
            spell.missileName = "GnarBigQMissile";
            spell.name = "Boulder Toss";
            spell.projectileSpeed = 2100.0f;
            spell.radius = 90.0f;
            spell.range = 1150.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "GnarBigQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
	//Gnar thiếu W hoá khổng lồ GnarBigW, GnarBigE
        {
            SpellData spell;
            spell.charName = "Gnar";
            spell.dangerlevel = 3;
            spell.name = "Wallop";
            spell.radius = 100.0f;
            spell.range = 600.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "GnarBigW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gnar";
            spell.dangerlevel = 3;
            spell.name = "GNAR!";
            spell.radius = 475.0f;
            spell.range = 590.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "GnarR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }

        // === Gragas ===
        {
            SpellData spell;
            spell.charName = "Gragas";
            spell.dangerlevel = 2;
            spell.missileName = "GragasQMissile";
            spell.name = "Barrel Roll";
            spell.projectileSpeed = 1000.0f;
            spell.radius = 250.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "GragasQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gragas";
            spell.dangerlevel = 3;
            spell.name = "Body Slam";
            spell.projectileSpeed = 910.0f;
            spell.radius = 180.0f;
            spell.range = 600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "GragasE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Gragas";
            spell.dangerlevel = 3;
            spell.name = "Explosive Cask";
            spell.radius = 350.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "GragasR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        // === Graves ===
	//Graves thiếu Q GravesQLineMis và GravesQReturn, tách làm 2
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.dangerlevel = 2;
            spell.missileName = "GravesQLineMissile";
            spell.name = "End of the Line";
            spell.projectileSpeed = 3000.0f;
            spell.radius = 40.0f;
            spell.range = 925.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "GravesQLineSpell";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.dangerlevel = 2;
            spell.name = "Smoke Screen";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 225.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "GravesSmokeGrenade";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
	//thiếu Graves R GravesChargeShot SpellType Line
        {
            SpellData spell;
            spell.charName = "Graves";
            spell.dangerlevel = 3;
            spell.missileName = "GravesUltimateMissile";
            spell.name = "Collateral Damage";
            spell.projectileSpeed = 2100.0f;
            spell.radius = 100.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "GravesUltimateShot";
            spell.extraSpellNames = { "GravesChargeShot" };
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "GravesChargeShotFxMissile", "GravesChargeShotFxMissile2" };
            Spells.push_back(spell);
        }

        // === Gwen ===

        {
            SpellData spell;
            spell.charName = "Gwen";
            spell.dangerlevel = 3;
            spell.missileName = "GwenRMis";
            spell.name = "Needlework";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 120.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "GwenR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.extraMissileNames = { "GwenRMis_VisualOnly", "GwenRMis3_VisualOnly" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Hecarim ===
        {
            SpellData spell;
            spell.charName = "Hecarim";
            spell.dangerlevel = 3;
            spell.missileName = "HecarimUltMissile";
            spell.name = "Onslaught of Shadows";
            spell.projectileSpeed = 1100.0f;
            spell.radius = 300.0f;
            spell.range = 50000.0f;
            spell.spellDelay = 9;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "HecarimUlt";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Fear;
            spell.extraMissileNames = { "HecarimUltMissileGrab", "HecarimUltMissileGrabEmpty", "HecarimUltMissileSkn4C", "HecarimUltMissileSkn4L1", "HecarimUltMissileSkn4L2", "HecarimUltMissileSkn4R1", "HecarimUltMissileSkn4R2" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Heimerdinger ===

        {
            SpellData spell;
            spell.charName = "Heimerdinger";
            spell.dangerlevel = 2;
            spell.name = "Hextech Micro-Rockets";
            spell.radius = 100.0f;
            spell.range = 1325.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "HeimerdingerW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Heimerdinger";
            spell.dangerlevel = 3;
            spell.name = "CH-2 Electron Storm Grenade";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 100.0f;
            spell.range = 970.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "HeimerdingerE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }

        // === Hwei ===
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 2;
            spell.missileName = "HweiQQMissile";
            spell.name = "Subject: Disaster - Devastating Fire (QQ)";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 50.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "HweiQQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 200.0f;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 2;
            spell.name = "Subject: Disaster - Molten Fissure (QW)";
            spell.radius = 100.0f;
            spell.range = 2000.0f;
            spell.spellDelay = 850;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "HweiQW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 3;
            spell.missileName = "HweiEQMissile";
            spell.name = "Subject: Torment - Grim Visage (EQ)";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 60.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "HweiEQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Charm;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 3;
            spell.missileName = "HweiEWMissile";
            spell.name = "Subject: Torment - Gaze of the Abyss (EW)";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 180.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "HweiEW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Snare;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Hwei";
            spell.dangerlevel = 4;
            spell.missileName = "HweiRMissile";
            spell.extraMissileNames = { "HweiR", "Hwei_R_Mis" };
            spell.name = "Spiraling Despair";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 90.0f;
            spell.range = 1340.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "HweiR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyYasuoWall };
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 500.0f;
            spell.extraDelay = 3000;
            Spells.push_back(spell);
        }

        // === Illaoi ===
        {
            SpellData spell;
            spell.charName = "Illaoi";
            spell.dangerlevel = 2;
            spell.name = "Tentacle Smash";
            spell.radius = 100.0f;
            spell.range = 850.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "IllaoiQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Illaoi";
            spell.dangerlevel = 2;
            spell.missileName = "IllaoiEMis";
            spell.name = "Test of Spirit";
            spell.projectileSpeed = 1900.0f;
            spell.radius = 50.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "IllaoiE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.extraMissileNames = { "IllaoiESpiritMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Irelia ===

        {
            SpellData spell;
            spell.charName = "Irelia";
            spell.dangerlevel = 2;
            spell.name = "Defiant Dance (Cast 2)";
            spell.radius = 100.0f;
            spell.range = 825.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "IreliaW2";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Irelia";
            spell.dangerlevel = 3;
            spell.missileName = "IreliaEMissile";
            spell.name = "Flawless Duet (Cast 2)";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 90.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "IreliaE2";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.extraMissileNames = { "IreliaEParticleMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Irelia";
            spell.dangerlevel = 3;
            spell.missileName = "IreliaR";
            spell.name = "Vanguard's Edge (Cast 1)";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 160.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "IreliaR1";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Ivern ===
        {
            SpellData spell;
            spell.charName = "Ivern";
            spell.dangerlevel = 3;
            spell.missileName = "IvernQ";
            spell.name = "Rootcaller";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 80.0f;
            spell.range = 1125.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "IvernQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Janna ===
        {
            SpellData spell;
            spell.charName = "Janna";
            spell.dangerlevel = 3;
            spell.name = "Howling Gale";
            spell.radius = 120.0f;
            spell.range = 1075.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "HowlingGale";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }

        // === JarvanIV ===
        {
            SpellData spell;
            spell.charName = "JarvanIV";
            spell.dangerlevel = 2;
            spell.name = "Dragon Strike";
            spell.radius = 70.0f;
            spell.range = 770.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "JarvanIVDragonStrike";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "JarvanIV";
            spell.dangerlevel = 2;
            spell.name = "Demacian Standard";
            spell.radius = 175.0f;
            spell.range = 860.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "JarvanIVDemacianStandard";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }



        // === Jayce ===
	//JayceShockBlast và JayceQAccel
        {
            SpellData spell;
            spell.charName = "Jayce";
            spell.dangerlevel = 2;
            spell.missileName = "JayceShockBlastMis";
            spell.name = "Shock Blast";
            spell.projectileSpeed = 1450.0f;
            spell.radius = 70.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "JayceShockBlast";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions, ZDCollisionObjectType::EnemyYasuoWall };
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 175.0f;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jayce";
            spell.dangerlevel = 3;
            spell.missileName = "JayceShockBlastWallMis";
            spell.name = "Shock Blast (Accelerated)";
            spell.projectileSpeed = 2350.0f;
            spell.radius = 70.0f;
            spell.range = 1600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "JayceShockBlastCharged";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions, ZDCollisionObjectType::EnemyYasuoWall };
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 250.0f;
            Spells.push_back(spell);
        }

        // === Jhin ===
        {
            SpellData spell;
            spell.charName = "Jhin";
            spell.dangerlevel = 3;
            spell.name = "Deadly Flourish";
            spell.radius = 40.0f;
            spell.range = 3000.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "JhinW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jhin";
            spell.dangerlevel = 2;
            spell.name = "Captive Audience";
            spell.radius = 135.0f;
            spell.range = 750.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "JhinE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jhin";
            spell.dangerlevel = 3;
            spell.missileName = "JhinRShotMis";
            spell.name = "Curtain Call";
            spell.projectileSpeed = 5000.0f;
            spell.radius = 80.0f;
            spell.range = 3500.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "JhinRShot";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions };
            Spells.push_back(spell);
        }

        // === Jinx ===
        {
            SpellData spell;
            spell.charName = "Jinx";
            spell.dangerlevel = 2;
            spell.missileName = "JinxWMissile";
            spell.name = "Zap!";
            spell.projectileSpeed = 3300.0f;
            spell.radius = 60.0f;
            spell.range = 1450.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "JinxW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.isSpecial = true;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jinx";
            spell.dangerlevel = 3;
            spell.name = "Flame Chompers!";
            spell.radius = 315.0f;
            spell.range = 925.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "JinxE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Snare;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Jinx";
            spell.dangerlevel = 3;
            spell.missileName = "JinxR";
            spell.name = "Super Mega Death Rocket!";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 140.0f;
            spell.range = 400.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "JinxR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === KSante ===
        {
            SpellData spell;
            spell.charName = "KSante";
            spell.dangerlevel = 2;
            spell.name = "Ntofo Strikes (Q3)";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 100.0f;
            spell.range = 475.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KSanteQ3";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KSante";
            spell.dangerlevel = 3;
            spell.missileName = "KSanteQ3Missile";
            spell.name = "Ntofo Strikes (Cast 1)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 70.0f;
            spell.range = 450.0f;
            spell.spellDelay = 349;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KSanteQ1";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KSante";
            spell.dangerlevel = 3;
            spell.missileName = "KSanteQ3Missile";
            spell.name = "Ntofo Strikes (Cast 2)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 70.0f;
            spell.range = 450.0f;
            spell.spellDelay = 349;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KSanteQ2";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KSante";
            spell.dangerlevel = 3;
            spell.name = "Path Maker";
            spell.radius = 55.0f;
            spell.range = 600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "KSanteW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockBack;
            Spells.push_back(spell);
        }

        // === Kaisa ===
        {
            SpellData spell;
            spell.charName = "Kaisa";
            spell.dangerlevel = 2;
            spell.missileName = "KaisaW";
            spell.name = "Void Seeker";
            spell.projectileSpeed = 1750.0f;
            spell.radius = 100.0f;
            spell.range = 3000.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "KaisaW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Kalista ===
        {
            SpellData spell;
            spell.charName = "Kalista";
            spell.dangerlevel = 2;
            spell.missileName = "KalistaMysticShotMissile";
            spell.name = "Pierce";
            spell.projectileSpeed = 2400.0f;
            spell.radius = 40.0f;
            spell.range = 1150.0f;
            spell.spellDelay = 349;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KalistaMysticShot";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "KalistaMysticShotMisTrue" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Kalista";
            spell.dangerlevel = 4;
            spell.missileName = "KalistaRMis";
            spell.name = "Fate's Call";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 1100.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "KalistaRx";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Karma ===
        {
            SpellData spell;
            spell.charName = "Karma";
            spell.dangerlevel = 2;
            spell.missileName = "KarmaQMissile";
            spell.name = "Inner Flame";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 60.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KarmaQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 280.0f;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Karma";
            spell.dangerlevel = 3;
            spell.missileName = "KarmaQMissileMantra";
            spell.name = "Inner Flame (Mantra)";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 80.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KarmaQHeavy";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 280.0f;
            Spells.push_back(spell);
        }
        // === Karthus ===
        {
            SpellData spell;
            spell.charName = "Karthus";
            spell.dangerlevel = 2;
            spell.name = "Lay Waste";
            spell.radius = 160.0f;
            spell.range = 875.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KarthusLayWasteA1";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        // === Kassadin ===
        {
            SpellData spell;
            spell.charName = "Kassadin";
            spell.dangerlevel = 2;
            spell.name = "Force Pulse";
            spell.range = 600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ForcePulse";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::Slow;
            spell.coneAngleDegrees = 80.0f;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kassadin";
            spell.dangerlevel = 3;
            spell.name = "Riftwalk";
            spell.radius = 270.0f;
            spell.range = 500.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "RiftWalk";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }



        // === Kayle ===
	//Kayle Q
        {
            SpellData spell;
            spell.charName = "Kayle";
            spell.dangerlevel = 1;
            spell.missileName = "KayleQMis";
            spell.name = "Radiant Blast";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 75.0f;
            spell.range = 900.0f;
            spell.spellDelay = 264;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KayleQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions, ZDCollisionObjectType::EnemyYasuoWall };
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 100.0f;
            Spells.push_back(spell);
        }

        // === Kayn ===
        {
            SpellData spell;
            spell.charName = "Kayn";
            spell.dangerlevel = 2;
            spell.name = "Blade's Reach";
            spell.radius = 90.0f;
            spell.range = 700.0f;
            spell.spellDelay = 550;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "KaynW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kayn";
            spell.dangerlevel = 3;
            spell.name = "Blade's Reach (Darkin)";
            spell.radius = 90.0f;
            spell.range = 700.0f;
            spell.spellDelay = 550;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "KaynAssW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kayn";
            spell.dangerlevel = 2;
            spell.name = "Reaping Slash";
            spell.range = 350.0f;
            spell.spellDelay = 150;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KaynQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
	//Kayn W

        // === Kennen ===
        {
            SpellData spell;
            spell.charName = "Kennen";
            spell.dangerlevel = 2;
            spell.missileName = "KennenShurikenHurlMissile1";
            spell.name = "Thundering Shuriken";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 50.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KennenShurikenHurlMissile1";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "KennenShurikenHurlMissile1" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Khazix ===
        {
            SpellData spell;
            spell.charName = "Khazix";
            spell.dangerlevel = 2;
            spell.missileName = "KhazixWMissile";
            spell.name = "Void Spike";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 70.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "KhazixW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }




        // === Kled ===
        {
            SpellData spell;
            spell.charName = "Kled";
            spell.dangerlevel = 2;
            spell.missileName = "KledQMissile";
            spell.name = "Bear Trap on a Rope";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 45.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KledQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Kled";
            spell.dangerlevel = 2;
            spell.name = "Jousting";
            spell.range = 550.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "KledE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }


        // === KogMaw ===
        {
            SpellData spell;
            spell.charName = "KogMaw";
            spell.dangerlevel = 2;
            spell.missileName = "KogMawQ";
            spell.name = "Caustic Spittle";
            spell.projectileSpeed = 1650.0f;
            spell.radius = 70.0f;
            spell.range = 1175.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "KogMawQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "KogMawQMis" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KogMaw";
            spell.dangerlevel = 2;
            spell.missileName = "KogMawVoidOozeMissile";
            spell.name = "Void Ooze";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 120.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "KogMawVoidOoze";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "KogMaw";
            spell.dangerlevel = 3;
            spell.name = "Living Artillery";
            spell.radius = 240.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "KogMawLivingArtillery";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        // === Leblanc ===
        {
            SpellData spell;
            spell.charName = "Leblanc";
            spell.dangerlevel = 2;
            spell.name = "Distortion";
            spell.radius = 220.0f;
            spell.range = 600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "LeblancW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Leblanc";
            spell.dangerlevel = 3;
            spell.missileName = "LeblancEMissile";
            spell.name = "Ethereal Chains";
            spell.projectileSpeed = 1750.0f;
            spell.radius = 55.0f;
            spell.range = 925.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "LeblancE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Leblanc";
            spell.dangerlevel = 3;
            spell.name = "Mimic";
            spell.radius = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "LeblancR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Snare;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Leblanc";
            spell.dangerlevel = 3;
            spell.missileName = "LeblancREMissile";
            spell.name = "Ethereal Chains (Mimic)";
            spell.projectileSpeed = 1750.0f;
            spell.radius = 55.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "LeblancRE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === LeeSin ===
        {
            SpellData spell;
            spell.charName = "LeeSin";
            spell.dangerlevel = 2;
            spell.missileName = "LeeSinQOne";
            spell.name = "Sonic Wave / Resonating Strike";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 60.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "LeeSinQOne";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Leona ===

        {
            SpellData spell;
            spell.charName = "Leona";
            spell.dangerlevel = 3;
            spell.missileName = "LeonaZenithBladeMissile";
            spell.name = "Zenith Blade";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 70.0f;
            spell.range = 875.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "LeonaZenithBlade";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Leona";
            spell.dangerlevel = 3;
            spell.name = "Solar Flare";
            spell.radius = 120.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "LeonaSolarFlare";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }

        // === Lillia ===
        {
            SpellData spell;
            spell.charName = "Lillia";
            spell.dangerlevel = 2;
            spell.name = "Watch Out! Eep!";
            spell.radius = 65.0f;
            spell.range = 500.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "LilliaW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lillia";
            spell.dangerlevel = 2;
            spell.missileName = "LilliaE";
            spell.name = "Swirlseed";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 150.0f;
            spell.range = 700.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "LilliaE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            spell.useEndPosition = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lillia";
            spell.dangerlevel = 3;
            spell.missileName = "LilliaERollingMissile";
            spell.name = "Swirlseed (Rolling)";
            spell.projectileSpeed = 1150.0f;
            spell.radius = 60.0f;
            spell.range = 25000.0f;
            spell.spellDelay = 0;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "LilliaERollingMissile";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions, ZDCollisionObjectType::Terrain, ZDCollisionObjectType::EnemyYasuoWall };
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 150.0f;
            spell.endExplosionRequiresCollision = true;
            Spells.push_back(spell);
        }


        // === Lissandra ===
        {
            SpellData spell;
            spell.charName = "Lissandra";
            spell.dangerlevel = 2;
            spell.missileName = "LissandraQMissile";
            spell.name = "Ice Shard";
            spell.projectileSpeed = 2200.0f;
            spell.radius = 75.0f;
            spell.range = 725.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "LissandraQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lissandra";
            spell.dangerlevel = 2;
            spell.missileName = "LissandraEMissile";
            spell.name = "Glacial Path";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 125.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "LissandraE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Locke";
            spell.dangerlevel = 2;
            spell.missileName = "LockeQNailMissile";
            spell.name = "Ritual Nails";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 60.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "LockeQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Locke";
            spell.dangerlevel = 5;
            spell.missileName = "LockeRArtifact";
            spell.name = "Purgatory";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 350.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "LockeR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }

        // === Lucian ===
	//Lucian Q
        {
            SpellData spell;
            spell.charName = "Lucian";
            spell.dangerlevel = 2;
            spell.name = "Piercing Light";
            spell.radius = 65.0f;
            spell.range = 500.0f;
            spell.spellDelay = 350;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "LucianQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lucian";
            spell.dangerlevel = 2;
            spell.missileName = "LucianWMissile";
            spell.name = "Ardent Blaze";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 55.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "LucianW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lucian";
            spell.dangerlevel = 3;
            spell.missileName = "LucianRMissile";
            spell.name = "The Culling";
            spell.projectileSpeed = 2800.0f;
            spell.radius = 110.0f;
            spell.range = 1400.0f;
            spell.spellDelay = 9;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "LucianR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "LucianRMissileOffhand" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Lulu ===
        {
            SpellData spell;
            spell.charName = "Lulu";
            spell.dangerlevel = 2;
            spell.missileName = "LuluQMissile";
            spell.name = "Glitterlance";
            spell.projectileSpeed = 1450.0f;
            spell.radius = 60.0f;
            spell.range = 925.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "LuluQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.extraMissileNames = { "LuluQMissileTwo" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Lux ===
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.dangerlevel = 3;
            spell.missileName = "LuxLightBindingMis";
            spell.name = "Light Binding";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 70.0f;
            spell.range = 1175.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "LuxLightBinding";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.dangerlevel = 2;
            spell.missileName = "LuxLightStrikeKugel";
            spell.name = "Lucent Singularity";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 295.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "LuxLightStrikeKugel";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Lux";
            spell.dangerlevel = 3;
            spell.name = "Final Spark";
            spell.radius = 250.0f;
            spell.range = 3340.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "LuxR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        // === Malphite ===
        {
            SpellData spell;
            spell.charName = "Malphite";
            spell.dangerlevel = 4;
            spell.name = "Unstoppable Force";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 270.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "UFSlash";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }

        // === Malzahar ===
        {
            SpellData spell;
            spell.charName = "Malzahar";
            spell.dangerlevel = 2;
            spell.missileName = "MalzaharQMissile";
            spell.name = "Call of the Void";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 85.0f;
            spell.range = 900.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "MalzaharQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Silence;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Maokai ===
        {
            SpellData spell;
            spell.charName = "Maokai";
            spell.dangerlevel = 3;
            spell.missileName = "MaokaiQMissile";
            spell.name = "Bramble Smash";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 110.0f;
            spell.range = 600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "MaokaiQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockBack;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Maokai";
            spell.dangerlevel = 4;
            spell.name = "Nature's Grasp";
            spell.projectileSpeed = 500.0f;
            spell.radius = 240.0f;
            spell.range = 3000.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "MaokaiR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            Spells.push_back(spell);
        }


        // === Mel ===
        {
            SpellData spell;
            spell.charName = "Mel";
            spell.dangerlevel = 2;
            spell.missileName = "MelQMissileBase";
            spell.name = "Radiant Volley";
            spell.projectileSpeed = 3800.0f;
            spell.radius = 140.0f;
            spell.range = 950.0f;
            spell.spellDelay = 349;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "MelQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "MelQMissile1", "MelQMissile2", "MelQMissile3", "MelQMissile4", "MelQMissile5", "MelQMissile6", "MelQMissile7", "MelQMissile8", "MelQMissile9", "MelQMissile10" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Mel";
            spell.dangerlevel = 3;
            spell.missileName = "MelE";
            spell.name = "Solar Snare";
            spell.projectileSpeed = 1100.0f;
            spell.radius = 70.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "MelE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Milio ===
        {
            SpellData spell;
            spell.charName = "Milio";
            spell.dangerlevel = 3;
            spell.missileName = "MilioQ";
            spell.name = "Ultra Mega Fire Kick";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 60.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "MilioQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === MissFortune ===
        // MissFortune Q, MissFortune E

        // === MonkeyKing ===
        // MonkeyKing Skip

        // === Mordekaiser ===
        {
            SpellData spell;
            spell.charName = "Mordekaiser";
            spell.dangerlevel = 2;
            spell.name = "Obliterate";
            spell.radius = 80.0f;
            spell.range = 675.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "MordekaiserQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Mordekaiser";
            spell.dangerlevel = 2;
            spell.missileName = "MordekaiserEMissile";
            spell.name = "Death's Grasp";
            spell.projectileSpeed = 3000.0f;
            spell.radius = 100.0f;
            spell.range = 700.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "MordekaiserE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Morgana ===
        {
            SpellData spell;
            spell.charName = "Morgana";
            spell.dangerlevel = 3;
            spell.missileName = "MorganaQ";
            spell.name = "Dark Binding";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 70.0f;
            spell.range = 1250.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "MorganaQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Morgana";
            spell.dangerlevel = 2;
            spell.name = "Tormented Shadow";
            spell.radius = 280.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "MorganaW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        // === Naafiri ===
        {
            SpellData spell;
            spell.charName = "Naafiri";
            spell.dangerlevel = 3;
            spell.missileName = "NaafiriQ";
            spell.name = "Darkin Daggers";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 150.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "NaafiriQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Taunt;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Nami ===
        {
            SpellData spell;
            spell.charName = "Nami";
            spell.dangerlevel = 3;
            spell.missileName = "NamiQDummyMissile";
            spell.name = "Aqua Prison";
            spell.projectileSpeed = 100.0f;
            spell.radius = 200.0f;
            spell.range = 875.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "NamiQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun;
            spell.extraMissileNames = { "NamiQMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Nami";
            spell.dangerlevel = 3;
            spell.missileName = "NamiRMissile";
            spell.name = "Tidal Wave";
            spell.projectileSpeed = 850.0f;
            spell.radius = 250.0f;
            spell.range = 2550.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "NamiR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Nasus ===
        {
            SpellData spell;
            spell.charName = "Nasus";
            spell.dangerlevel = 2;
            spell.name = "Spirit Fire";
            spell.radius = 380.0f;
            spell.range = 650.0f;
            spell.spellDelay = 514;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "NasusE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        // === Nautilus ===
        {
            SpellData spell;
            spell.charName = "Nautilus";
            spell.dangerlevel = 3;
            spell.missileName = "NautilusAnchorDragMissile";
            spell.name = "Dredge Line";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 95.0f;
            spell.range = 1150.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "NautilusAnchorDrag";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Neeko ===
        {
            SpellData spell;
            spell.charName = "Neeko";
            spell.dangerlevel = 2;
            spell.missileName = "NeekoQ";
            spell.name = "Blooming Burst";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 250.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "NeekoQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Neeko";
            spell.dangerlevel = 3;
            spell.missileName = "NeekoE";
            spell.name = "Tangle-Barbs";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 70.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "NeekoE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        // thiếu nekko R

        // === Nidalee ===
        {
            SpellData spell;
            spell.charName = "Nidalee";
            spell.dangerlevel = 2;
            spell.missileName = "JavelinToss";
            spell.name = "Javelin Toss / Takedown";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 40.0f;
            spell.range = 1500.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "JavelinToss";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Nilah ===
        // thiếu nilah Q
        {
            SpellData spell;
            spell.charName = "Nilah";
            spell.dangerlevel = 1;
            spell.name = "Formless Blade";
            spell.radius = 75.0f;
            spell.range = 600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "NilahQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        // === Nocturne ===
        {
            SpellData spell;
            spell.charName = "Nocturne";
            spell.dangerlevel = 2;
            spell.missileName = "NocturneDuskbringer";
            spell.name = "Duskbringer";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 60.0f;
            spell.range = 1125.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "NocturneDuskbringer";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Olaf ===
        {
            SpellData spell;
            spell.charName = "Olaf";
            spell.dangerlevel = 2;
            spell.name = "Undertow";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 90.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "OlafAxeThrowCast";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }

        // === Orianna ===
        {
            SpellData spell;
            spell.charName = "Orianna";
            spell.dangerlevel = 2;
            spell.name = "Command: Attack";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 145.0f;
            spell.range = 815.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "OrianaIzunaCommand";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
	//Oriana R

        // === Ornn ===
        {
            SpellData spell;
            spell.charName = "Ornn";
            spell.dangerlevel = 2;
            spell.missileName = "OrnnQ";
            spell.name = "Volcanic Rupture";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 65.0f;
            spell.range = 800.0f;
            spell.spellDelay = 300;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "OrnnQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Ornn";
            spell.dangerlevel = 3;
            spell.name = "Searing Charge";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 100.0f;
            spell.range = 450.0f;
            spell.spellDelay = 349;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "OrnnE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }
	//OrnnRWave và OrnnRWave2

        // === Pantheon ===
	//thiếu PantheonQTap
        {
            SpellData spell;
            spell.charName = "Pantheon";
            spell.dangerlevel = 2;
            spell.missileName = "PantheonQMissile";
            spell.name = "Comet Spear";
            spell.projectileSpeed = 2700.0f;
            spell.radius = 55.0f;
            spell.range = 575.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "PantheonQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Poppy ===
        {
            SpellData spell;
            spell.charName = "Poppy";
            spell.dangerlevel = 2;
            spell.name = "Hammer Shock";
            spell.radius = 80.0f;
            spell.range = 430.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "PoppyQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Poppy";
            spell.dangerlevel = 4;
            spell.missileName = "PoppyRMissile";
            spell.name = "Keeper's Verdict";
            spell.projectileSpeed = 2500.0f;
            spell.radius = 100.0f;
            spell.range = 500.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "PoppyR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockBack;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Pyke ===
	//PykeQ
        {
            SpellData spell;
            spell.charName = "Pyke";
            spell.dangerlevel = 3;
            spell.missileName = "PykeQMissile";
            spell.name = "Bone Skewer";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 70.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 200;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "PykeQCast";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockBack;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Pyke";
            spell.dangerlevel = 3;
            spell.missileName = "PykeEMissile";
            spell.name = "Phantom Undertow";
            spell.projectileSpeed = 3000.0f;
            spell.radius = 110.0f;
            spell.range = 550.0f;
            spell.spellDelay = 275;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "PykeE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Pyke";
            spell.dangerlevel = 3;
            spell.name = "Death From Below";
            spell.radius = 125.0f;
            spell.range = 750.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "PykeR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        // === Qiyana ===
        {
            SpellData spell;
            spell.charName = "Qiyana";
            spell.dangerlevel = 3;
            spell.name = "Elemental Wrath / Edge of Ixtal";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 70.0f;
            spell.range = 525.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "QiyanaQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Qiyana";
            spell.dangerlevel = 3;
            spell.missileName = "QiyanaRMis";
            spell.name = "Supreme Display of Talent";
            spell.projectileSpeed = 3540.0f;
            spell.radius = 100.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "QiyanaR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.extraMissileNames = { "QiyanaRWallHitMis", "QiyanaRWallFollowMis", "QiyanaRWallFollowMisShadow", "QiyanaRWallFollowMisShadowCounterClockwise" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Quinn ===
        {
            SpellData spell;
            spell.charName = "Quinn";
            spell.dangerlevel = 2;
            spell.missileName = "QuinnQ";
            spell.name = "Blinding Assault";
            spell.projectileSpeed = 1550.0f;
            spell.radius = 60.0f;
            spell.range = 1025.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "QuinnQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Rakan ===
        {
            SpellData spell;
            spell.charName = "Rakan";
            spell.dangerlevel = 2;
            spell.missileName = "RakanQMis";
            spell.name = "Gleaming Quill";
            spell.projectileSpeed = 1850.0f;
            spell.radius = 65.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "RakanQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "RakanQReturnMis" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Rakan";
            spell.dangerlevel = 3;
            spell.name = "Grand Entrance";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 275.0f;
            spell.range = 600.0f;
            spell.spellDelay = 349;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "RakanW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Snare;
            Spells.push_back(spell);
        }

        // === Rammus ===

        {
            SpellData spell;
            spell.charName = "Rammus";
            spell.dangerlevel = 4;
            spell.name = "Soaring Slam";
            spell.projectileSpeed = 900.0f;
            spell.radius = 375.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "Tremors2";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }

        // === RekSai ===
	//thiếu Q

        // === Rell ===
        {
            SpellData spell;
            spell.charName = "Rell";
            spell.dangerlevel = 3;
            spell.name = "Shattering Strike";
            spell.radius = 75.0f;
            spell.range = 600.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "RellQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }

        // === Renata ===
        {
            SpellData spell;
            spell.charName = "Renata";
            spell.dangerlevel = 3;
            spell.missileName = "RenataQ";
            spell.name = "Handshake";
            spell.projectileSpeed = 1450.0f;
            spell.radius = 70.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "RenataQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Renata";
            spell.dangerlevel = 2;
            spell.missileName = "RenataE";
            spell.name = "Loyalty Program";
            spell.projectileSpeed = 1450.0f;
            spell.radius = 110.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "RenataE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Renata";
            spell.dangerlevel = 3;
            spell.missileName = "RenataRMissile";
            spell.name = "Hostile Takeover";
            spell.projectileSpeed = 650.0f;
            spell.radius = 250.0f;
            spell.range = 2000.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "RenataR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "RenataRMissileSides" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Rengar ===

        {
            SpellData spell;
            spell.charName = "Rengar";
            spell.dangerlevel = 3;
            spell.missileName = "RengarEMis";
            spell.name = "Bola Strike";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 70.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "RengarE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Riven";
            spell.dangerlevel = 4;
            spell.missileName = "RivenWindSlashMissile";
            spell.name = "Wind Slash";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 125.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "RivenIzunaBlade";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }


        // === Rumble ===
        {
            SpellData spell;
            spell.charName = "Rumble";
            spell.dangerlevel = 2;
            spell.missileName = "RumbleGrenadeMissile";
            spell.name = "Electro Harpoon";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 60.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "RumbleGrenade";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.extraMissileNames = { "RumbleGrenadeMissileDangerZone" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
	//RumbleR

        // === Ryze ===
        {
            SpellData spell;
            spell.charName = "Ryze";
            spell.dangerlevel = 2;
            spell.missileName = "RyzeQMissile";
            spell.name = "Overload";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 55.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "RyzeQWrapper";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Samira ===
        {
            SpellData spell;
            spell.charName = "Samira";
            spell.dangerlevel = 2;
            spell.name = "Flair";
            spell.projectileSpeed = 2600.0f;
            spell.radius = 60.0f;
            spell.range = 950.0f;
            spell.spellDelay = 50;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SamiraQ";
            spell.extraSpellNames = { "SamiraQGun", "SamiraQBuffered" };
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Samira";
            spell.dangerlevel = 2;
            spell.name = "Flair (Sword Cone)";
            spell.radius = 65.0f;
            spell.range = 400.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SamiraQSword";
            spell.extraSpellNames = { "SamiraQBufferedSword" };
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::None;
            spell.coneAngleDegrees = 50.0f;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Samira";
            spell.dangerlevel = 2;
            spell.name = "Wild Rush";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 150.0f;
            spell.range = 650.0f;
            spell.spellDelay = 0;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SamiraE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }

        // === Sejuani ===
        {
            SpellData spell;
            spell.charName = "Sejuani";
            spell.dangerlevel = 2;
            spell.name = "Arctic Assault";
            spell.radius = 150.0f;
            spell.range = 650.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SejuaniQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sejuani";
            spell.dangerlevel = 2;
            spell.name = "Winter's Wrath";
            spell.radius = 130.0f;
            spell.range = 600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "SejuaniW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sejuani";
            spell.dangerlevel = 3;
            spell.missileName = "SejuaniRMissile";
            spell.name = "Glacial Prison";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 120.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "SejuaniR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Senna ===
	//SenaQ
        {
            SpellData spell;
            spell.charName = "Senna";
            spell.dangerlevel = 3;
            spell.missileName = "SennaWMissile";
            spell.name = "Last Embrace";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 60.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "SennaW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            spell.hasEndExplosion = true;
            spell.secondaryRadius = 280.0f;
            spell.extraDelay = 1000;
            spell.endExplosionRequiresUnitCollision = true;
            spell.endExplosionAtUnitCenter = true;
            spell.endExplosionFollowsUnit = true;
            spell.endExplosionDetonatesOnUnitDeath = true;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Senna";
            spell.dangerlevel = 4;
            spell.missileName = "SennaRWarningMis";
            spell.name = "Dawning Shadow";
            spell.projectileSpeed = 20000.0f;
            spell.radius = 160.0f;
            spell.range = 25000.0f;
            spell.spellDelay = 1000;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "SennaR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "SennaR" };
            Spells.push_back(spell);
        }

        // === Seraphine ===
        {
            SpellData spell;
            spell.charName = "Seraphine";
            spell.dangerlevel = 2;
            spell.missileName = "SeraphineQInitialMissile";
            spell.name = "High Note";
            spell.projectileSpeed = 600.0f;
            spell.radius = 50.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SeraphineQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "SeraphineQSecondaryMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Seraphine";
            spell.dangerlevel = 3;
            spell.missileName = "SeraphineEMissile";
            spell.name = "Beat Drop";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 70.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SeraphineE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Seraphine";
            spell.dangerlevel = 3;
            spell.missileName = "SeraphineR";
            spell.name = "Encore";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 160.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "SeraphineR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Charm;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Sett ===
	//SettW
        {
            SpellData spell;
            spell.charName = "Sett";
            spell.dangerlevel = 3;
            spell.name = "Facebreaker";
            spell.radius = 350.0f;
            spell.range = 490.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SettE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }

        // === Shaco ===

        {
            SpellData spell;
            spell.charName = "Shaco";
            spell.dangerlevel = 3;
            spell.name = "Jack In The Box";
            spell.radius = 300.0f;
            spell.range = 500.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "JackInTheBox";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Snare;
            Spells.push_back(spell);
        }

        // === Shen ===

        {
            SpellData spell;
            spell.charName = "Shen";
            spell.dangerlevel = 3;
            spell.name = "Shadow Dash";
            spell.range = 600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ShenE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Taunt;
            Spells.push_back(spell);
        }

        // === Shyvana === //cần kiểm tra lại
        {
            SpellData spell;
            spell.charName = "Shyvana";
            spell.dangerlevel = 2;
            spell.missileName = "ShyvanaE";
            spell.name = "ShyvanaE";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 75.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ShyvanaE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Shyvana";
            spell.dangerlevel = 3;
            spell.name = "ShyvanaR";
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "ShyvanaR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }


        {
            SpellData spell;
            spell.charName = "Singed";
            spell.dangerlevel = 2;
            spell.missileName = "SingedWParticleMissile";
            spell.name = "Mega Adhesive";
            spell.projectileSpeed = 100.0f;
            spell.radius = 265.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "MegaAdhesive";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Sion ===
        {
            SpellData spell;
            spell.charName = "Sion";
            spell.dangerlevel = 3;
            spell.name = "Decimating Smash";
            spell.radius = 200.0f;
            spell.range = 850.0f;
            spell.spellDelay = 1000;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SionQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Sion";
            spell.dangerlevel = 2;
            spell.missileName = "SionEMissile";
            spell.name = "Roar of the Slayer";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 80.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SionE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Sion";
            spell.dangerlevel = 4;
            spell.name = "Unstoppable Onslaught";
            spell.projectileSpeed = 950.0f;
            spell.radius = 120.0f;
            spell.range = 7500.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "SionR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.isSpecial = true;
	    spell.fixedRange = true;
            Spells.push_back(spell);
        }

        // === Sivir ===
        {
            SpellData spell;
            spell.charName = "Sivir";
            spell.dangerlevel = 2;
            spell.missileName = "SivirQMissile";
            spell.name = "Boomerang Blade";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 100.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SivirQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "SivirQMissileReturn", "SivirQMissileReturnDead" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Skarner ===
	//thiếu Q
        {
            SpellData spell;
            spell.charName = "Skarner";
            spell.dangerlevel = 3;
            spell.missileName = "SkarnerEIsotopeMissile";
            spell.name = "Ixtal's Impact";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 70.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SkarnerE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Skarner";
            spell.dangerlevel = 4;
            spell.name = "Impale";
            spell.radius = 175.0f;
            spell.range = 625.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "SkarnerR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Suppression;
            Spells.push_back(spell);
        }

        // === Smolder ===
        {
            SpellData spell;
            spell.charName = "Smolder";
            spell.dangerlevel = 2;
            spell.missileName = "SmolderW";
            spell.name = "Achooo!";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 115.0f;
            spell.range = 1500.0f;
            spell.spellDelay = 349;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "SmolderW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Smolder";
            spell.dangerlevel = 3;
            spell.missileName = "SmolderRMomMissile";
            spell.name = "MMOOOMMMM!";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 125.0f;
            spell.range = 4200.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "SmolderR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.extraMissileNames = { "SmolderRMomMissileSweetspot" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Sona ===
        {
            SpellData spell;
            spell.charName = "Sona";
            spell.dangerlevel = 3;
            spell.missileName = "SonaR";
            spell.name = "Crescendo";
            spell.projectileSpeed = 2400.0f;
            spell.radius = 140.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "SonaR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Soraka ===
        {
            SpellData spell;
            spell.charName = "Soraka";
            spell.dangerlevel = 2;
            spell.missileName = "SorakaQMissile";
            spell.name = "Starcall";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 230.0f;
            spell.range = 810.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SorakaQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            spell.extraMissileNames = { "SorakaQReturnMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Soraka";
            spell.dangerlevel = 3;
            spell.name = "Equinox";
            spell.radius = 260.0f;
            spell.range = 925.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SorakaE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Snare;
            Spells.push_back(spell);
        }

        // === Swain ===
        {
            SpellData spell;
            spell.charName = "Swain";
            spell.dangerlevel = 2;
            spell.name = "Death's Hand";
            spell.radius = 725.0f;
            spell.range = 750.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SwainQ";
            spell.spellType = ZDSpellType::Cone;
            spell.ccType = CCType::None;
            spell.coneAngleDegrees = 45.0f;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Swain";
            spell.dangerlevel = 2;
            spell.name = "Vision of Empire";
            spell.radius = 325.0f;
            spell.range = 5500.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "SwainW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Swain";
            spell.dangerlevel = 3;
            spell.missileName = "SwainE";
            spell.name = "Nevermove";
            spell.projectileSpeed = 600.0f;
            spell.radius = 90.0f;
            spell.range = 850.0f;
            spell.spellDelay = 50;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SwainE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockBack;
            spell.extraMissileNames = { "SwainEReturnMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Sylas ===
        {
            SpellData spell;
            spell.charName = "Sylas";
            spell.dangerlevel = 2;
            spell.missileName = "SylasQ";
            spell.name = "Chain Lash";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 70.0f;
            spell.range = 775.0f;
            spell.spellDelay = 400;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SylasQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Sylas";
            spell.dangerlevel = 3;
            spell.missileName = "SylasE";
            spell.name = "Abscond / Abduct (Cast 2)";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 60.0f;
            spell.range = 240.0f;
            spell.spellDelay = 100;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "SylasE2";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Syndra ===
	//thiếu Q
        {
            SpellData spell;
            spell.charName = "Syndra";
            spell.dangerlevel = 2;
            spell.name = "Dark Sphere";
            spell.radius = 180.0f;
            spell.range = 800.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "SyndraQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Syndra";
            spell.dangerlevel = 2;
            spell.name = "Force of Will";
            spell.range = 925.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "SyndraW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }

        // === TahmKench ===
        {
            SpellData spell;
            spell.charName = "TahmKench";
            spell.dangerlevel = 3;
            spell.missileName = "TahmKenchQ";
            spell.name = "Tongue Lash";
            spell.projectileSpeed = 2800.0f;
            spell.radius = 70.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "TahmKenchQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "TahmKench";
            spell.dangerlevel = 3;
            spell.name = "Abyssal Dive";
            spell.radius = 250.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "TahmKenchW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }

        // === Taliyah ===
        {
            SpellData spell;
            spell.charName = "Taliyah";
            spell.dangerlevel = 3;
            spell.missileName = "TaliyahQMis";
            spell.name = "Threaded Volley";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 100.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "TaliyahQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.extraMissileNames = { "TaliyahQMisBig" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
	//TaliyahW
        {
            SpellData spell;
            spell.charName = "Taliyah";
            spell.dangerlevel = 2;
            spell.missileName = "TaliyahESoundBlowupMis";
            spell.name = "Unraveled Earth";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 800.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "TaliyahE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.extraMissileNames = { "TaliyahESoundMis" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Talon ===
	//TalonW
        {
            SpellData spell;
            spell.charName = "Talon";
            spell.dangerlevel = 2;
            spell.missileName = "TalonWBlades";
            spell.name = "Rake";
            spell.projectileSpeed = 1850.0f;
            spell.radius = 75.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "TalonW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }

        // === Taric ===
        {
            SpellData spell;
            spell.charName = "Taric";
            spell.dangerlevel = 3;
            spell.name = "Dazzle";
            spell.radius = 70.0f;
            spell.range = 610.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "TaricE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }

        // === Teemo ===
        {
            SpellData spell;
            spell.charName = "Teemo";
            spell.dangerlevel = 3;
            spell.name = "Noxious Trap";
            spell.radius = 135.0f;
            spell.range = 600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "TeemoR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }

        // === Thresh ===
        {
            SpellData spell;
            spell.charName = "Thresh";
            spell.dangerlevel = 3;
            spell.missileName = "ThreshQMissile";
            spell.name = "Death Sentence";
            spell.projectileSpeed = 1000.0f;
            spell.radius = 85.0f;
            spell.range = 1075.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ThreshQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            spell.extraMissileNames = { "ThreshQPullMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Thresh";
            spell.dangerlevel = 2;
            spell.missileName = "ThreshEMissile1";
            spell.name = "Flay";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 110.0f;
            spell.range = 500.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ThreshE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Tristana ===

        {
            SpellData spell;
            spell.charName = "Tristana";
            spell.dangerlevel = 2;
            spell.name = "Rocket Jump";
            spell.projectileSpeed = 1100.0f;
            spell.radius = 270.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "TristanaW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }


        // === Tryndamere ===
	//TryndamereW special

        // === TwistedFate ===
        {
            SpellData spell;
            spell.charName = "TwistedFate";
            spell.dangerlevel = 2;
            spell.missileName = "SealFateMissile";
            spell.name = "Wild Cards";
            spell.projectileSpeed = 1000.0f;
            spell.radius = 40.0f;
            spell.range = 1450.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "WildCards";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyYasuoWall };
            spell.fixedRange = true;
            spell.multipleNumber = 3;
            spell.multipleAngle = 28.0f;
            Spells.push_back(spell);
        }

        // === Twitch ===

        {
            SpellData spell;
            spell.charName = "Twitch";
            spell.dangerlevel = 2;
            spell.missileName = "TwitchVenomCaskMissile";
            spell.name = "Venom Cask";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 275.0f;
            spell.range = 950.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "TwitchVenomCask";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Urgot ===
        {
            SpellData spell;
            spell.charName = "Urgot";
            spell.dangerlevel = 2;
            spell.missileName = "UrgotQMissile";
            spell.name = "Corrosive Charge";
            spell.radius = 210.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "UrgotQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            spell.extraMissileNames = { "UrgotQMissileExtraSkin03", "UrgotQMissileSkin03" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Urgot";
            spell.dangerlevel = 3;
            spell.name = "Disdain";
            spell.radius = 300.0f;
            spell.range = 475.0f;
            spell.spellDelay = 449;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "UrgotE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Urgot";
            spell.dangerlevel = 4;
            spell.missileName = "UrgotR";
            spell.name = "Fear Beyond Death";
            spell.projectileSpeed = 3200.0f;
            spell.radius = 80.0f;
            spell.range = 2500.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "UrgotR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Suppression;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Varus ===
        {
            SpellData spell;
            spell.charName = "Varus";
            spell.dangerlevel = 2;
            spell.missileName = "VarusQMissile";
            spell.name = "Piercing Arrow";
            spell.projectileSpeed = 1900.0f;
            spell.radius = 70.0f;
            spell.range = 925.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "VarusQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
	//VarusE
        {
            SpellData spell;
            spell.charName = "Varus";
            spell.dangerlevel = 3;
            spell.missileName = "VarusRMissile";
            spell.name = "Chain of Corruption";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 120.0f;
            spell.range = 1300.0f;
            spell.spellDelay = 241;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "VarusR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Veigar ===
        {
            SpellData spell;
            spell.charName = "Veigar";
            spell.dangerlevel = 2;
            spell.missileName = "VeigarBalefulStrikeMis";
            spell.name = "Baleful Strike";
            spell.projectileSpeed = 2200.0f;
            spell.radius = 70.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "VeigarBalefulStrike";
            spell.extraSpellNames = { "VeigarQ" };
            spell.extraMissileNames = { "VeigarQMis", "VeigarQMissile" };
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Veigar";
            spell.dangerlevel = 3;
            spell.name = "Dark Matter";
            spell.radius = 240.0f;
            spell.range = 900.0f;
            spell.spellDelay = 1250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "VeigarW";
            spell.extraSpellNames = { "VeigarDarkMatter" };
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Veigar";
            spell.dangerlevel = 3;
            spell.name = "Event Horizon";
            spell.radius = 390.0f;
            spell.innerRadius = 290.0f;
            spell.range = 725.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "VeigarEventHorizon";
            spell.spellType = ZDSpellType::Ring;
            spell.ccType = CCType::Stun;
            spell.extraEndTime = 3000;
            Spells.push_back(spell);
        }

        // === Velkoz ===
        {
            SpellData spell;
            spell.charName = "Velkoz";
            spell.dangerlevel = 2;
            spell.missileName = "VelkozQMissile";
            spell.name = "Plasma Fission";
            spell.projectileSpeed = 2100.0f;
            spell.radius = 45.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "VelkozQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
	    //special
        }
        {
            SpellData spell;
            spell.charName = "Velkoz";
            spell.dangerlevel = 3;
            spell.missileName = "VelkozQMissileSplit";
            spell.name = "Plasma Fission (Split)";
            spell.projectileSpeed = 2100.0f;
            spell.radius = 45.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "VelkozQSplit";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions, ZDCollisionObjectType::EnemyYasuoWall };
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Velkoz";
            spell.dangerlevel = 2;
            spell.missileName = "VelkozWMissile";
            spell.name = "Void Rift";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 87.5f;
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "VelkozW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Velkoz";
            spell.dangerlevel = 3;
            spell.missileName = "VelkozEMissile";
            spell.name = "Tectonic Disruption";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 225.0f;
            spell.range = 810.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "VelkozE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockBack;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Vex ===
        {
            SpellData spell;
            spell.charName = "Vex";
            spell.dangerlevel = 2;
            spell.missileName = "VexQ";
            spell.name = "Mistral Bolt";
            spell.projectileSpeed = 600.0f;
            spell.radius = 180.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "VexQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
	//thiếu VexQ Cone
        {
            SpellData spell;
            spell.charName = "Vex";
            spell.dangerlevel = 2;
            spell.missileName = "VexE";
            spell.name = "Looming Darkness";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 80.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "VexE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Vex";
            spell.dangerlevel = 3;
            spell.missileName = "VexR";
            spell.name = "Shadow Surge";
            spell.projectileSpeed = 1600.0f;
            spell.radius = 130.0f;
            spell.range = 2000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "VexR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Vi ===
        {
            SpellData spell;
            spell.charName = "Vi";
            spell.dangerlevel = 2;
            spell.name = "Vault Breaker";
            spell.radius = 55.0f;
            spell.range = 250.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ViQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }


        // === Viego ===
        {
            SpellData spell;
            spell.charName = "Viego";
            spell.dangerlevel = 2;
            spell.name = "Blade of the Ruined King";
            spell.radius = 70.0f;
            spell.range = 600.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ViegoQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Viego";
            spell.dangerlevel = 3;
            spell.missileName = "ViegoWMis";
            spell.name = "Spectral Maw";
            spell.projectileSpeed = 1300.0f;
            spell.radius = 60.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "ViegoW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Viego";
            spell.dangerlevel = 4;
            spell.name = "Heartbreaker";
            spell.radius = 300.0f;
            spell.range = 500.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "ViegoR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::KnockBack;
            Spells.push_back(spell);
        }

        // === Viktor ===
        {
            SpellData spell;
            spell.charName = "Viktor";
            spell.dangerlevel = 3;
            spell.name = "Gravity Field";
            spell.radius = 300.0f;
            spell.range = 800.0f;
            spell.spellDelay = 1000;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "ViktorGravitonField";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun;
            spell.extraSpellNames = { "ViktorW" };
            spell.extraEndTime = 4500;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Viktor";
            spell.dangerlevel = 3;
            spell.missileName = "ViktorEMissile";
            spell.name = "Hextech Ray";
            spell.projectileSpeed = 1050.0f;
            spell.radius = 80.0f;
            spell.range = 700.0f;
            spell.spellDelay = 0;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ViktorE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraSpellNames = { "ViktorDeathRay" };
            spell.extraMissileNames = { "ViktorEAugMissile", "ViktorDeathRayMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyYasuoWall };
            spell.fixedRange = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Viktor";
            spell.dangerlevel = 3;
            spell.name = "Hextech Ray Aftershock";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 80.0f;
            spell.range = 700.0f;
            spell.spellDelay = 1000;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ViktorEAftershock";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraSpellNames = { "ViktorDeathRay3" };
            spell.fixedRange = true;
            spell.noProcess = true;
            Spells.push_back(spell);
        }
        // === Vladimir ===
        {
            SpellData spell;
            spell.charName = "Vladimir";
            spell.dangerlevel = 2;
            spell.name = "Hemoplague";
            spell.radius = 375.0f;
            spell.range = 625.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "VladimirHemoplague";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            Spells.push_back(spell);
        }

        // === Volibear ===
        {
            SpellData spell;
            spell.charName = "Volibear";
            spell.dangerlevel = 2;
            spell.name = "Sky Splitter";
            spell.radius = 325.0f;
            spell.range = 1200.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "VolibearE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }

        // === Warwick ===
        {
            SpellData spell;
            spell.charName = "Warwick";
            spell.dangerlevel = 4;
            spell.name = "Infinite Duress";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 55.0f;
            spell.range = 2500.0f;
            spell.spellDelay = 100;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "WarwickR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Suppression;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }

        // === Xayah ===
        {
            SpellData spell;
            spell.charName = "Xayah";
            spell.dangerlevel = 2;
            spell.missileName = "XayahQMissile1";
            spell.name = "Double Daggers";
            spell.projectileSpeed = 400.0f;
            spell.radius = 50.0f;
            spell.range = 400.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "XayahQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "XayahQMissile2" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Xayah";
            spell.dangerlevel = 3;
            spell.missileName = "XayahRMissile";
            spell.name = "Featherstorm";
            spell.projectileSpeed = 4000.0f;
            spell.radius = 20.0f;
            spell.range = 450.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "XayahR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Xerath ===
        {
            SpellData spell;
            spell.charName = "Xerath";
            spell.dangerlevel = 2;
            spell.name = "Arcanopulse";
            spell.radius = 100.0f;
            spell.range = 1700.0f;
            spell.spellDelay = 500;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "XerathArcanopulse2";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.useEndPosition = true;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Xerath";
            spell.dangerlevel = 2;
            spell.name = "Eye of Destruction";
            spell.radius = 250.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "XerathArcaneBarrage2";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Xerath";
            spell.dangerlevel = 3;
            spell.missileName = "XerathMageSpearMissile";
            spell.name = "Shocking Orb";
            spell.projectileSpeed = 1400.0f;
            spell.radius = 62.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "XerathMageSpear";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Stun;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Xerath";
            spell.dangerlevel = 3;
            spell.name = "Rite of the Arcane";
            spell.radius = 200.0f;
            spell.range = 5600.0f;
            spell.spellDelay = 627;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "XerathRMissileWrapper";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            spell.extraSpellNames = { "XerathLocusPulse" };
            Spells.push_back(spell);
        }

        // === XinZhao ===

        {
            SpellData spell;
            spell.charName = "XinZhao";
            spell.dangerlevel = 2;
            spell.missileName = "XinZhaoWMissile";
            spell.name = "Wind Becomes Lightning";
            spell.projectileSpeed = 6250.0f;
            spell.radius = 40.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 600;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "XinZhaoW";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Yasuo ===
	//Yasuo 2Q, Q1 và Q3
        {
            SpellData spell;
            spell.charName = "Yasuo";
            spell.dangerlevel = 3;
            spell.missileName = "YasuoQ3Mis";
            spell.name = "Steel Tempest (Q3)";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 90.0f;
            spell.range = 1150.0f;
            spell.spellDelay = 333;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "YasuoQ3W";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }


        // === Yone ===
	//Yone 2Q, Q1 và Q3
        {
            SpellData spell;
            spell.charName = "Yone";
            spell.dangerlevel = 3;
            spell.missileName = "YoneQ3Mis";
            spell.name = "Mortal Steel (Q3)";
            spell.projectileSpeed = 1500.0f;
            spell.radius = 80.0f;
            spell.range = 1050.0f;
            spell.spellDelay = 350;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "YoneQ3";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Yone";
            spell.dangerlevel = 5;
            spell.name = "Fate Sealed";
            spell.radius = 112.5f;
            spell.range = 1000.0f;
            spell.spellDelay = 750;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "YoneR";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::KnockUp;
            Spells.push_back(spell);
        }

        // === Yorick ===

        {
            SpellData spell;
            spell.charName = "Yorick";
            spell.dangerlevel = 2;
            spell.missileName = "YorickEMissile";
            spell.name = "Mourning Mist";
            spell.projectileSpeed = 1800.0f;
            spell.radius = 80.0f;
            spell.range = 700.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "YorickE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Yuumi ===
	//YummiQ danger2, special
        {
            SpellData spell;
            spell.charName = "Yuumi";
            spell.dangerlevel = 2;
            spell.missileName = "YuumiQMissile";
            spell.name = "Prowling Projectile";
            spell.projectileSpeed = 1450.0f;
            spell.radius = 60.0f;
            spell.range = 1150.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "YuumiQCast";
            spell.spellType = ZDSpellType::Line;
            spell.missileRouteMode = MissileRouteMode::Steering;
            spell.ccType = CCType::Slow;
            spell.extraSpellNames = { "YuumiQ" };
            spell.isSpecial = true;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Zac ===
        {
            SpellData spell;
            spell.charName = "Zac";
            spell.dangerlevel = 2;
            spell.missileName = "ZacQMissile";
            spell.name = "Stretching Strikes";
            spell.projectileSpeed = 2800.0f;
            spell.radius = 80.0f;
            spell.range = 800.0f;
            spell.spellDelay = 330;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ZacQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Slow;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zac";
            spell.dangerlevel = 3;
            spell.name = "Elastic Slingshot";
            spell.radius = 250.0f;
            spell.range = 300.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ZacE";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun;
            Spells.push_back(spell);
        }

        // === Zed ===
        {
            SpellData spell;
            spell.charName = "Zed";
            spell.dangerlevel = 2;
            spell.missileName = "ZedQMissile";
            spell.name = "Razor Shuriken";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 50.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ZedQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
	//ZedW


        // === Zeri ===
        {
            SpellData spell;
            spell.charName = "Zeri";
            spell.dangerlevel = 2;
            spell.missileName = "ZeriQMis";
            spell.name = "Burst Fire";
            spell.projectileSpeed = 2600.0f;
            spell.radius = 10.0f;
            spell.range = 700.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ZeriQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "ZeriQMisEmpowered", "ZeriQMisEmpoweredParent", "ZeriQMisEmpoweredPierce", "ZeriQMisEmpoweredPierceParent", "ZeriQMisParent", "ZeriQMisPierce", "ZeriQMisPierceParent" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        // === Ziggs ===
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.dangerlevel = 2;
            spell.missileName = "ZiggsQSpell";
            spell.name = "Bouncing Bomb";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 240.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ZiggsQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            spell.isSpecial = true;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyYasuoWall };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.dangerlevel = 2;
            spell.missileName = "ZiggsQSpell2";
            spell.name = "Bouncing Bomb";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 240.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ZiggsQBounce1";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            spell.isSpecial = true;
            spell.noProcess = true;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyYasuoWall };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.dangerlevel = 2;
            spell.missileName = "ZiggsQSpell3";
            spell.name = "Bouncing Bomb";
            spell.projectileSpeed = 1700.0f;
            spell.radius = 240.0f;
            spell.range = 850.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ZiggsQBounce2";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            spell.isSpecial = true;
            spell.noProcess = true;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyYasuoWall };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.dangerlevel = 3;
            spell.missileName = "ZiggsW";
            spell.name = "Satchel Charge";
            spell.projectileSpeed = 1750.0f;
            spell.range = 1000.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::W;
            spell.spellName = "ZiggsW";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.dangerlevel = 2;
            spell.name = "Hexplosive Minefield (Cast 1)";
            spell.projectileSpeed = 1550.0f;
            spell.radius = 235.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ZiggsE1";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.dangerlevel = 2;
            spell.name = "Hexplosive Minefield (Cast 2)";
            spell.projectileSpeed = 1550.0f;
            spell.radius = 235.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ZiggsE2";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.dangerlevel = 2;
            spell.name = "Hexplosive Minefield (Cast 3)";
            spell.projectileSpeed = 1550.0f;
            spell.radius = 235.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ZiggsE3";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Slow;
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Ziggs";
            spell.dangerlevel = 3;
            spell.name = "Mega Inferno Bomb";
            spell.projectileSpeed = 0.0f;
            spell.radius = 250.0f;
            spell.range = 5000.0f;
            spell.spellDelay = 1500;
            spell.spellKey = ZDSpellSlot::R;
            spell.spellName = "ZiggsR";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::None;
            spell.isSpecial = true;
            Spells.push_back(spell);
        }

        // === Zilean ===
        {
            SpellData spell;
            spell.charName = "Zilean";
            spell.dangerlevel = 3;
            spell.missileName = "ZileanQAttachMissile";
            spell.name = "Time Bomb";
            spell.projectileSpeed = 2000.0f;
            spell.radius = 250.0f;
            spell.range = 900.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ZileanQ";
            spell.spellType = ZDSpellType::Circular;
            spell.ccType = CCType::Stun;
            spell.extraMissileNames = { "ZileanQAttachMissileMinion", "ZileanQAttachMissileShort", "ZileanQAttachMissileTall", "ZileanQMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }


        // === Zoe ===
        {
            SpellData spell;
            spell.charName = "Zoe";
            spell.dangerlevel = 2;
            spell.missileName = "ZoeQMis2";
            spell.name = "Paddle Star!";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 50.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ZoeQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "ZoeQMis2SleepCombo", "ZoeQMis2Warning", "ZoeQMis2Warning2", "ZoeQMis2WarningSleepCombo", "ZoeQMissile" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
        {
            SpellData spell;
            spell.charName = "Zoe";
            spell.dangerlevel = 3;
            spell.missileName = "ZoeE";
            spell.name = "Sleepy Trouble Bubble";
            spell.projectileSpeed = 1850.0f;
            spell.radius = 50.0f;
            spell.range = 800.0f;
            spell.spellDelay = 300;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ZoeE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Sleep;
            spell.extraMissileNames = { "ZoeEMis" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
	//thiếu định nghĩa trap, Zoe E trap circular


        // === Zyra ===
        {
            SpellData spell;
            spell.charName = "Zyra";
            spell.dangerlevel = 2;
            spell.missileName = "ZyraQ";
            spell.name = "Deadly Spines";
            spell.projectileSpeed = 1200.0f;
            spell.radius = 140.0f;
            spell.range = 800.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::Q;
            spell.spellName = "ZyraQ";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::None;
            spell.extraMissileNames = { "ZyraQPlantMissile", "ZyraQPlantMissileOnSpawn" };
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }

        {
            SpellData spell;
            spell.charName = "Zyra";
            spell.dangerlevel = 3;
            spell.missileName = "ZyraE";
            spell.name = "Grasping Roots";
            spell.projectileSpeed = 1150.0f;
            spell.radius = 70.0f;
            spell.range = 1100.0f;
            spell.spellDelay = 250;
            spell.spellKey = ZDSpellSlot::E;
            spell.spellName = "ZyraE";
            spell.spellType = ZDSpellType::Line;
            spell.ccType = CCType::Snare;
            spell.collisionObjects = { ZDCollisionObjectType::EnemyChampions, ZDCollisionObjectType::EnemyMinions };
            Spells.push_back(spell);
        }
	//ZyraR

        // Validation runs before Initialize returns, while no caller can hold
        // pointers into the stable database. Invalid cones remain conservative
        // in geometry and are default-off in runtime configuration.
        for (SpellData& spell : Spells) {
            if (!spell.HasValidGeometryFields())
                spell.defaultOff = true;
            if (spell.spellType == ZDSpellType::Cone) {
                if (!std::isfinite(spell.coneAngleDegrees) ||
                    spell.coneAngleDegrees <= 0.0f ||
                    spell.coneAngleDegrees > 360.0f) {
                    spell.defaultOff = true;
                    ++invalidConeSpellCount_;
                }
            }
            if (spell.spellType == ZDSpellType::Arc) {
                spell.defaultOff = true;
                ++invalidArcSpellCount_;
            }
        }
    }

private:
    static int invalidConeSpellCount_;
    static int invalidArcSpellCount_;
    static int supportedArcSpellCount_;
};

inline std::vector<ZDEvade::SpellData> SpellDatabase::Spells;
inline int SpellDatabase::invalidConeSpellCount_ = 0;
inline int SpellDatabase::invalidArcSpellCount_ = 0;
inline int SpellDatabase::supportedArcSpellCount_ = 0;
} // namespace ZDEvade
