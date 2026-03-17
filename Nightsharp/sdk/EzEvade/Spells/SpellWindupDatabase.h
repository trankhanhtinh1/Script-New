#pragma once
#include "SpellData.h"
#include <vector>

namespace EzEvade {
    inline std::vector<SpellData>& GetSpellWindupDatabase() {
        static std::vector<SpellData> Spells;
        if (!Spells.empty()) return Spells;
        Spells.reserve(1000);

        // ==== Aatrox ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Aatrox";
            d.name = "Blade of Torment";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::E;
            d.spellName = "AatroxE";
            return d;
        }());

        // ==== Ahri ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Ahri";
            d.name = "Orb of Deception";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "AhriOrbofDeception";
            d.radius = 100.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Ahri";
            d.name = "Charm";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "AhriSeduce";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        // ==== Akali ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Akali";
            d.name = "AkaliMota";
            d.spellKey = SpellSlot::Q;
            d.spellName = "AkaliMota";
            d.radius = 175.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Akali";
            d.name = "AkaliSmokeBomb";
            d.spellKey = SpellSlot::W;
            d.spellName = "AkaliSmokeBomb";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Akali";
            d.name = "AkaliShadowSwipe";
            d.spellKey = SpellSlot::E;
            d.spellName = "AkaliShadowSwipe";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        // ==== Alistar ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Alistar";
            d.name = "Pulverize";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::Q;
            d.spellName = "Pulverize";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Alistar";
            d.name = "TriumphantRoar";
            d.spellKey = SpellSlot::E;
            d.spellName = "TriumphantRoar";
            return d;
        }());

        // ==== Azir ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Azir";
            d.name = "AzirQ";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "AzirQ";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Azir";
            d.name = "AzirW";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "AzirW";
            return d;
        }());

        // ==== Amumu ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Amumu";
            d.name = "Tantrum";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "Tantrum";
            return d;
        }());

        // ==== Anivia ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Anivia";
            d.name = "Flash Frost";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "FlashFrostSpell";
            d.radius = 110.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Anivia";
            d.name = "Frostbite";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "Frostbite";
            return d;
        }());

        // ==== Annie ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Annie";
            d.name = "InfernalGuardian";
            d.spellKey = SpellSlot::R;
            d.spellName = "InfernalGuardian";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Annie";
            d.name = "Disintegrate";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "Disintegrate";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Annie";
            d.name = "Incinerate";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "Incinerate";
            return d;
        }());

        // ==== Ashe ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Ashe";
            d.name = "Volley";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "Volley";
            d.radius = 10.0f; // Patched wiki
            return d;
        }());

        // ==== Bard ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Bard";
            d.name = "BardQ";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "BardQ";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Bard";
            d.name = "BardW";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "BardW";
            return d;
        }());

        // ==== Blitzcrank ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Blitzcrank";
            d.name = "Rocket Grab";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "RocketGrabMissile";
            return d;
        }());

        // ==== Brand ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Brand";
            d.name = "BrandConflagration";
            d.spellKey = SpellSlot::E;
            d.spellName = "BrandConflagration";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Brand";
            d.name = "BrandBlaze";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "BrandBlaze";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Brand";
            d.name = "Pillar of Flame";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "BrandFissure";
            return d;
        }());

        // ==== Braum ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Braum";
            d.name = "BraumQ";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "BraumQ";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        // ==== Caitlyn ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Caitlyn";
            d.name = "Piltover Peacemaker";
            d.spellDelay = 625.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "CaitlynPiltoverPeacemaker";
            return d;
        }());

        // ==== Cassiopeia ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Cassiopeia";
            d.name = "Twin Fang";
            d.spellDelay = 125.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "CassiopeiaTwinFang";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Cassiopeia";
            d.name = "CassiopeiaMiasma";
            d.spellKey = SpellSlot::W;
            d.spellName = "CassiopeiaMiasma";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Cassiopeia";
            d.name = "Noxious Blast";
            d.spellKey = SpellSlot::Q;
            d.spellName = "CassiopeiaNoxiousBlast";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Chogath ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Chogath";
            d.name = "FeralScream";
            d.spellDelay = 500.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "FeralScream";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Chogath";
            d.name = "Rupture";
            d.spellDelay = 500.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "Rupture";
            return d;
        }());

        // ==== Corki ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Corki";
            d.name = "Missile Barrage";
            d.spellDelay = 175.0f; // Patched wiki
            d.spellKey = SpellSlot::R;
            d.spellName = "MissileBarrage";
            d.radius = 40.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Corki";
            d.name = "Phosphorus Bomb";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "PhosphorusBomb";
            return d;
        }());

        // ==== Darius ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Darius";
            d.name = "Decimate";
            d.spellDelay = 230;
            d.spellKey = SpellSlot::Q;
            d.spellName = "DariusCleave";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Darius";
            d.name = "Apprehend";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "DariusAxeGrabCone";
            return d;
        }());

        // ==== Diana ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Diana";
            d.name = "DianaVortex";
            d.spellKey = SpellSlot::E;
            d.spellName = "DianaVortex";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Diana";
            d.name = "DianaArc";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "DianaArc";
            return d;
        }());

        // ==== DrMundo ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "DrMundo";
            d.name = "Infected Cleaver";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "InfectedCleaverMissile";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        // ==== Draven ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Draven";
            d.name = "Stand Aside";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "DravenDoubleShot";
            d.radius = 130.0f; // Patched wiki
            return d;
        }());

        // ==== Ekko ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Ekko";
            d.name = "EkkoQ";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "EkkoQ";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Ekko";
            d.name = "EkkoW";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "EkkoW";
            return d;
        }());

        // ==== Elise ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Elise";
            d.name = "Volatile Spiderling";
            d.spellDelay = 125.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "EliseHumanW";
            d.radius = 150.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Elise";
            d.name = "Venomous Bite";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "EliseSpiderQCast";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Elise";
            d.name = "Cocoon";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "EliseHumanE";
            d.radius = 55.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Elise";
            d.name = "Neurotoxin";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "EliseHumanQ";
            return d;
        }());

        // ==== Evelynn ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Evelynn";
            d.name = "Ravage";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "EvelynnQ";
            return d;
        }());

        // ==== Ezreal ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Ezreal";
            d.name = "Essence Flux";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "EzrealEssenceFlux";
            d.radius = 80.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Ezreal";
            d.name = "Mystic Shot";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "EzrealMysticShot";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Ezreal";
            d.name = "Trueshot Barrage";
            d.spellDelay = 1000.0f; // Patched wiki
            d.spellKey = SpellSlot::R;
            d.spellName = "EzrealTrueshotBarrage";
            d.radius = 160.0f; // Patched wiki
            return d;
        }());

        // ==== FiddleSticks ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "FiddleSticks";
            d.name = "Crowstorm";
            d.spellKey = SpellSlot::R;
            d.spellName = "Crowstorm";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "FiddleSticks";
            d.name = "Terrify";
            d.spellKey = SpellSlot::Q;
            d.spellName = "Terrify";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "FiddleSticks";
            d.name = "Dark Wind";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::E;
            d.spellName = "FiddlesticksDarkWind";
            return d;
        }());

        // ==== Galio ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Galio";
            d.name = "GalioResoluteSmite";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "GalioResoluteSmite";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Galio";
            d.name = "GalioRighteousGust";
            d.spellKey = SpellSlot::E;
            d.spellName = "GalioRighteousGust";
            d.spellDelay = 400.0f; // Patched wiki
            d.radius = 200.0f; // Patched wiki
            return d;
        }());

        // ==== Gangplank ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Gangplank";
            d.name = "CannonBarrage";
            d.spellKey = SpellSlot::R;
            d.spellName = "CannonBarrage";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Gangplank";
            d.name = "Parley";
            d.spellKey = SpellSlot::Q;
            d.spellName = "Parley";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Gangplank";
            d.name = "RaiseMorale";
            d.spellKey = SpellSlot::E;
            d.spellName = "RaiseMorale";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Gnar ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Gnar";
            d.name = "GnarQ";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "GnarQ";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Gnar";
            d.name = "GnarW";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::W;
            d.spellName = "gnarbigw";
            return d;
        }());

        // ==== Gragas ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Gragas";
            d.name = "Barrel Roll";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "GragasBarrelRoll";
            return d;
        }());

        // ==== Graves ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Graves";
            d.name = "Smoke Screen";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "GravesSmokeGrenade";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Graves";
            d.name = "Buckshot";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "GravesClusterShot";
            return d;
        }());

        // ==== Hecarim ====
        // ==== Heimerdinger ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Heimerdinger";
            d.name = "HeimerdingerE";
            d.spellKey = SpellSlot::E;
            d.spellName = "HeimerdingerE";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Heimerdinger";
            d.name = "HeimerdingerW";
            d.spellKey = SpellSlot::W;
            d.spellName = "HeimerdingerW";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Janna ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Janna";
            d.name = "SowTheWind";
            d.spellKey = SpellSlot::W;
            d.spellName = "SowTheWind";
            d.spellDelay = 245.0f; // Patched wiki
            return d;
        }());

        // ==== Jayce ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Jayce";
            d.name = "JayceShockBlast";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::Q;
            d.spellName = "jayceshockblast";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Jayce";
            d.name = "JayceQ";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::Q;
            d.spellName = "JayceToTheSkies";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Jayce";
            d.name = "Thundering Blow";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "JayceThunderingBlow";
            return d;
        }());

        // ==== Jinx ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Jinx";
            d.name = "Zap";
            d.spellDelay = 600;
            d.spellKey = SpellSlot::W;
            d.spellName = "JinxWMissile";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        // ==== Kalista ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Kalista";
            d.name = "KalistaMysticShot";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "KalistaMysticShot";
            d.radius = 40.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Kalista";
            d.name = "KalistaW";
            d.spellDelay = 500.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "KalistaW";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Kalista";
            d.name = "KalistaE";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "KalistaE";
            return d;
        }());

        // ==== Karma ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Karma";
            d.name = "KarmaQ";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "KarmaQ";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        // ==== Karthus ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Karthus";
            d.name = "Lay Waste";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "karthuslaywastea3";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Karthus";
            d.name = "WallOfPain";
            d.spellKey = SpellSlot::W;
            d.spellName = "WallOfPain";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Kassadin ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Kassadin";
            d.name = "Force Pulse";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "ForcePulse";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Kassadin";
            d.name = "Null Sphere";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "NullLance";
            return d;
        }());

        // ==== Katarina ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Katarina";
            d.name = "KatarinaQ";
            d.spellKey = SpellSlot::Q;
            d.spellName = "KatarinaQ";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Kayle ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Kayle";
            d.name = "Reckoning";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::Q;
            d.spellName = "JudicatorReckoning";
            d.radius = 75.0f; // Patched wiki
            return d;
        }());

        // ==== Kennen ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Kennen";
            d.name = "Thundering Shuriken";
            d.spellDelay = 175.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "KennenShurikenHurlMissile1";
            d.radius = 50.0f; // Patched wiki
            return d;
        }());

        // ==== Khazix ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Khazix";
            d.name = "KhazixQ";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "KhazixQ";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Khazix";
            d.name = "KhazixW";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "KhazixW";
            d.radius = 70.0f; // Patched wiki
            return d;
        }());

        // ==== KogMaw ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "KogMaw";
            d.name = "Living Artillery";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::R;
            d.spellName = "KogMawLivingArtillery";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "KogMaw";
            d.name = "Caustic Spittle";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::Q;
            d.spellName = "KogMawCausticSpittle";
            d.radius = 70.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "KogMaw";
            d.name = "Void Ooze";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "KogMawVoidOozeMissile";
            d.radius = 120.0f; // Patched wiki
            return d;
        }());

        // ==== Leblanc ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Leblanc";
            d.name = "Sigil of Silence";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "LeblancChaosOrb";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Leblanc";
            d.name = "Ethereal Chains";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "LeblancSoulShackle";
            d.radius = 55.0f; // Patched wiki
            return d;
        }());

        // ==== LeeSin ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "LeeSin";
            d.name = "Sonic Wave";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "BlindMonkQOne";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "LeeSin";
            d.name = "BlindMonkEOne";
            d.spellKey = SpellSlot::E;
            d.spellName = "BlindMonkEOne";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Lissandra ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Lissandra";
            d.name = "LissandraQ";
            d.spellKey = SpellSlot::Q;
            d.spellName = "LissandraQ";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Lissandra";
            d.name = "LissandraW";
            d.spellKey = SpellSlot::W;
            d.spellName = "LissandraW";
            return d;
        }());

        // ==== Lucian ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Lucian";
            d.name = "LucianQ";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::Q;
            d.spellName = "LucianQ";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Lucian";
            d.name = "LucianW";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "LucianW";
            d.radius = 55.0f; // Patched wiki
            return d;
        }());

        // ==== Lulu ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Lulu";
            d.name = "LuluQ";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "LuluQ";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Lulu";
            d.name = "LuluE";
            d.spellKey = SpellSlot::E;
            d.spellName = "LuluE";
            return d;
        }());

        // ==== Lux ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Lux";
            d.name = "Light Binding";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "LuxLightBinding";
            d.radius = 70.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Lux";
            d.name = "LuxLightStrikeKugel";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "LuxLightStrikeKugel";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Lux";
            d.name = "LuxPrismaticWave";
            d.spellKey = SpellSlot::W;
            d.spellName = "LuxPrismaticWave";
            d.spellDelay = 250.0f; // Patched wiki
            d.radius = 110.0f; // Patched wiki
            return d;
        }());

        // ==== Malphite ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Malphite";
            d.name = "Seismic Shard";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "SeismicShard";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Malphite";
            d.name = "Ground Slam";
            d.spellDelay = 241.9f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "Landslide";
            return d;
        }());

        // ==== Malzahar ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Malzahar";
            d.name = "AlZaharCalloftheVoid";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "AlZaharCalloftheVoid";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Malzahar";
            d.name = "Null Zone";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::W;
            d.spellName = "AlZaharNullZone";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Malzahar";
            d.name = "Malefic Visions";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "AlZaharMaleficVisions";
            return d;
        }());

        // ==== Maokai ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Maokai";
            d.name = "MaokaiTrunkLine";
            d.spellKey = SpellSlot::Q;
            d.spellName = "MaokaiTrunkLine";
            d.spellDelay = 300.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Maokai";
            d.name = "MaokaiSapling2";
            d.spellKey = SpellSlot::E;
            d.spellName = "MaokaiSapling2";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== MissFortune ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "MissFortune";
            d.name = "MissFortuneRicochetShot";
            d.spellKey = SpellSlot::Q;
            d.spellName = "MissFortuneRicochetShot";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "MissFortune";
            d.name = "MissFortuneScattershot";
            d.spellKey = SpellSlot::E;
            d.spellName = "MissFortuneScattershot";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Mordekaiser ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Mordekaiser";
            d.name = "MordekaiserSyphonOfDestruction";
            d.spellKey = SpellSlot::E;
            d.spellName = "MordekaiserSyphonOfDestruction";
            d.spellDelay = 250.0f; // Patched wiki
            d.radius = 100.0f; // Patched wiki
            return d;
        }());

        // ==== Morgana ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Morgana";
            d.name = "TormentedSoil";
            d.spellKey = SpellSlot::W;
            d.spellName = "TormentedSoil";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Morgana";
            d.name = "Dark Binding";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "DarkBindingMissile";
            d.radius = 70.0f; // Patched wiki
            return d;
        }());

        // ==== Nami ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Nami";
            d.name = "NamiQ";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "NamiQ";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Nami";
            d.name = "TidecallersBlessing";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::E;
            d.spellName = "NamiE";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Nami";
            d.name = "Ebb and Flow";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "NamiW";
            return d;
        }());

        // ==== Nasus ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Nasus";
            d.name = "Wither";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "NasusW";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Nasus";
            d.name = "NasusE";
            d.spellKey = SpellSlot::E;
            d.spellName = "NasusE";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Nautilus ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Nautilus";
            d.name = "NautilusSplashZone";
            d.spellKey = SpellSlot::E;
            d.spellName = "NautilusSplashZone";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Nidalee ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Nidalee";
            d.name = "PrimalSurge";
            d.spellKey = SpellSlot::E;
            d.spellName = "PrimalSurge";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Nidalee";
            d.name = "Javelin Toss";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "JavelinToss";
            d.radius = 40.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Nidalee";
            d.name = "Swipe";
            d.spellKey = SpellSlot::E;
            d.spellName = "Swipe";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Nocturne ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Nocturne";
            d.name = "Unspeakable Horror";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::E;
            d.spellName = "NocturneUnspeakableHorror";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Nocturne";
            d.name = "NocturneDuskbringer";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "NocturneDuskbringer";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        // ==== Nunu ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Nunu";
            d.name = "IceBlast";
            d.spellDelay = 400;
            d.spellKey = SpellSlot::E;
            d.spellName = "IceBlast";
            d.radius = 25.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Nunu";
            d.name = "Consume";
            d.spellDelay = 300.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "Consume";
            return d;
        }());

        // ==== Olaf ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Olaf";
            d.name = "Reckless Swing";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::E;
            d.spellName = "OlafRecklessStrike";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Olaf";
            d.name = "Undertow";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "OlafAxeThrow";
            d.radius = 90.0f; // Patched wiki
            return d;
        }());

        // ==== Pantheon ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Pantheon";
            d.name = "Pantheon_Throw";
            d.spellKey = SpellSlot::Q;
            d.spellName = "Pantheon_Throw";
            d.spellDelay = 200.0f; // Patched wiki
            return d;
        }());

        // ==== Quinn ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Quinn";
            d.name = "QuinnQ";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "QuinnQ";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        // ==== Renekton ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Renekton";
            d.name = "RenektonCleave";
            d.spellKey = SpellSlot::Q;
            d.spellName = "RenektonCleave";
            return d;
        }());

        // ==== Rengar ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Rengar";
            d.name = "Bola Strike";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::E;
            d.spellName = "RengarE";
            d.radius = 70.0f; // Patched wiki
            return d;
        }());

        // ==== Riven ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Riven";
            d.name = "Ki Burst";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "RivenMartyr";
            return d;
        }());

        // ==== Rumble ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Rumble";
            d.name = "RumbleGrenade";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "RumbleGrenade";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        // ==== Ryze ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Ryze";
            d.name = "Overload";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "Overload";
            d.radius = 55.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Ryze";
            d.name = "Rune Prison";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "RunePrison";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Ryze";
            d.name = "Spell Flux";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "SpellFlux";
            return d;
        }());

        // ==== Shaco ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Shaco";
            d.name = "JackInTheBox";
            d.spellKey = SpellSlot::W;
            d.spellName = "JackInTheBox";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Shaco";
            d.name = "TwoShivPoison";
            d.spellKey = SpellSlot::E;
            d.spellName = "TwoShivPoison";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Shen ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Shen";
            d.name = "ShenVorpalStar";
            d.spellKey = SpellSlot::Q;
            d.spellName = "ShenVorpalStar";
            d.radius = 80.0f; // Patched wiki
            return d;
        }());

        // ==== Shyvana ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Shyvana";
            d.name = "ShyvanaFireball";
            d.spellKey = SpellSlot::E;
            d.spellName = "ShyvanaFireball";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        // ==== Sion ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Sion";
            d.name = "CrypticGaze";
            d.spellKey = SpellSlot::Q;
            d.spellName = "CrypticGaze";
            return d;
        }());

        // ==== Sivir ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Sivir";
            d.name = "Boomerang Blade";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::Q;
            d.spellName = "SivirQ";
            return d;
        }());

        // ==== Skarner ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Skarner";
            d.name = "Fracture";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::E;
            d.spellName = "SkarnerFracture";
            d.radius = 160.0f; // Patched wiki
            return d;
        }());

        // ==== Soraka ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Soraka";
            d.name = "Infuse";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "Infuse";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Soraka";
            d.name = "Starcall";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "Starcall";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Soraka";
            d.name = "AstralBlessing";
            d.spellKey = SpellSlot::W;
            d.spellName = "AstralBlessing";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Swain ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Swain";
            d.name = "Nevermove";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "SwainShadowGrasp";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Swain";
            d.name = "Torment";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "SwainTorment";
            d.radius = 90.0f; // Patched wiki
            return d;
        }());

        // ==== Syndra ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Syndra";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "SyndraE";
            d.radius = 70.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Syndra";
            d.name = "SyndraQ";
            d.spellDelay = 200;
            d.spellKey = SpellSlot::Q;
            d.spellName = "SyndraQ";
            return d;
        }());

        // ==== Talon ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Talon";
            d.name = "Rake";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "TalonRake";
            d.radius = 75.0f; // Patched wiki
            return d;
        }());

        // ==== Taric ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Taric";
            d.name = "Shatter";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "Shatter";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Taric";
            d.name = "Radiance";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::R;
            d.spellName = "TaricHammerSmash";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Taric";
            d.name = "Imbue";
            d.spellKey = SpellSlot::Q;
            d.spellName = "Imbue";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Teemo ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Teemo";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::R;
            d.spellName = "BantamTrap";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Teemo";
            d.name = "BlindingDart";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "BlindingDart";
            return d;
        }());

        // ==== Thresh ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Thresh";
            d.name = "ThreshQ";
            d.spellDelay = 500.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "ThreshQ";
            d.radius = 70.0f; // Patched wiki
            return d;
        }());

        // ==== Tristana ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Tristana";
            d.name = "Explosive Shot";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::E;
            d.spellName = "DetonatingShot";
            return d;
        }());

        // ==== Trundle ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Trundle";
            d.name = "TrundleCircle";
            d.spellKey = SpellSlot::E;
            d.spellName = "TrundleCircle";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Tryndamere ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Tryndamere";
            d.name = "MockingShout";
            d.spellKey = SpellSlot::W;
            d.spellName = "MockingShout";
            d.spellDelay = 300.0f; // Patched wiki
            return d;
        }());

        // ==== TwistedFate ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "TwistedFate";
            d.name = "Loaded Dice";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "WildCards";
            return d;
        }());

        // ==== Twitch ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Twitch";
            d.name = "Expunge";
            d.spellKey = SpellSlot::E;
            d.spellName = "Expunge";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Twitch";
            d.name = "TwitchVenomCask";
            d.spellKey = SpellSlot::W;
            d.spellName = "TwitchVenomCask";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Urgot ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Urgot";
            d.name = "UrgotPlasmaGrenade";
            d.spellKey = SpellSlot::E;
            d.spellName = "UrgotPlasmaGrenade";
            d.spellDelay = 450.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Urgot";
            d.name = "UrgotHeatseekingMissile";
            d.spellKey = SpellSlot::Q;
            d.spellName = "UrgotHeatseekingMissile";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Varus ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Varus";
            d.name = "Varus E";
            d.spellDelay = 241.9f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "VarusE";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Varus";
            d.name = "Varus Q Missile";
            d.spellDelay = 0;
            d.spellKey = SpellSlot::Q;
            d.spellName = "VarusQ";
            d.radius = 70.0f; // Patched wiki
            return d;
        }());

        // ==== Vayne ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Vayne";
            d.name = "VayneCondemn";
            d.spellKey = SpellSlot::E;
            d.spellName = "VayneCondemn";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Veigar ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Veigar";
            d.name = "VeigarDarkMatter";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "VeigarDarkMatter";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Veigar";
            d.name = "Baleful Strike";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "VeigarBalefulStrike";
            d.radius = 70.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Veigar";
            d.name = "VeigarEventHorizon";
            d.spellKey = SpellSlot::E;
            d.spellName = "VeigarEventHorizon";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Velkoz ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Velkoz";
            d.name = "VelkozW";
            d.spellKey = SpellSlot::W;
            d.spellName = "VelkozW";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Velkoz";
            d.name = "VelkozQ";
            d.spellKey = SpellSlot::Q;
            d.spellName = "VelkozQ";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Velkoz";
            d.name = "VelkozE";
            d.spellKey = SpellSlot::E;
            d.spellName = "VelkozE";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Vi ====
        // ==== Viktor ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Viktor";
            d.name = "ViktorPowerTransfer";
            d.spellKey = SpellSlot::Q;
            d.spellName = "ViktorPowerTransfer";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Viktor";
            d.name = "ViktorGravitonField";
            d.spellKey = SpellSlot::W;
            d.spellName = "ViktorGravitonField";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Vladimir ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Vladimir";
            d.name = "VladimirTidesofBlood";
            d.spellKey = SpellSlot::E;
            d.spellName = "VladimirTidesofBlood";
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Vladimir";
            d.name = "VladimirHemoplague";
            d.spellDelay = 389;
            d.spellKey = SpellSlot::R;
            d.spellName = "VladimirHemoplague";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Vladimir";
            d.name = "VladimirTransfusion";
            d.spellKey = SpellSlot::Q;
            d.spellName = "VladimirTransfusion";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Warwick ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Warwick";
            d.name = "Hungering Strike";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::Q;
            d.spellName = "HungeringStrike";
            return d;
        }());

        // ==== Xerath ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Xerath";
            d.name = "XerathMageSpear";
            d.spellKey = SpellSlot::E;
            d.spellName = "XerathMageSpear";
            d.spellDelay = 250.0f; // Patched wiki
            d.radius = 60.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Xerath";
            d.name = "XerathArcaneBarrage2";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "XerathArcaneBarrage2";
            return d;
        }());

        // ==== Yasuo ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Yasuo";
            d.name = "Steel Tempest3";
            d.spellDelay = 175;
            d.spellKey = SpellSlot::Q;
            d.spellName = "YasuoQW";
            return d;
        }());

        // ==== Yorick ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Yorick";
            d.name = "YorickDecayed";
            d.spellKey = SpellSlot::W;
            d.spellName = "YorickDecayed";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Yorick";
            d.name = "YorickRavenous";
            d.spellKey = SpellSlot::E;
            d.spellName = "YorickRavenous";
            d.spellDelay = 250.0f; // Patched wiki
            return d;
        }());

        // ==== Zac ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Zac";
            d.name = "ZacQ";
            d.spellKey = SpellSlot::Q;
            d.spellName = "ZacQ";
            d.spellDelay = 330.0f; // Patched wiki
            d.radius = 80.0f; // Patched wiki
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Zac";
            d.name = "ZacW";
            d.spellKey = SpellSlot::W;
            d.spellName = "ZacW";
            return d;
        }());

        // ==== Zed ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Zed";
            d.name = "ZedShuriken";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "ZedShuriken";
            d.radius = 50.0f; // Patched wiki
            return d;
        }());

        // ==== Ziggs ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Ziggs";
            d.name = "ZiggsW";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::W;
            d.spellName = "ZiggsW";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Ziggs";
            d.name = "ZiggsE";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "ZiggsE";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Ziggs";
            d.name = "ZiggsQ";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "ZiggsQ";
            d.radius = 180.0f; // Patched wiki
            return d;
        }());

        // ==== Zilean ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Zilean";
            d.name = "Time Bomb";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "TimeBomb";
            d.radius = 140.0f; // Patched wiki
            return d;
        }());

        // ==== Zyra ====
        Spells.push_back([](){
            SpellData d;
            d.charName = "Zyra";
            d.name = "Grasping Roots";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::E;
            d.spellName = "ZyraGraspingRoots";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Zyra";
            d.name = "Rampant Growth";
            d.spellDelay = 250;
            d.spellKey = SpellSlot::W;
            d.spellName = "ZyraSeed";
            return d;
        }());

        Spells.push_back([](){
            SpellData d;
            d.charName = "Zyra";
            d.name = "Deadly Bloom";
            d.spellDelay = 250.0f; // Patched wiki
            d.spellKey = SpellSlot::Q;
            d.spellName = "ZyraQFissure";
            return d;
        }());

        return Spells;
    }
}