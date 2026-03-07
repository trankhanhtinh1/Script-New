#pragma once
#include "SpellData.h"
#include "SpellDatabase2.h"
#include <vector>

// ============================================================================
// SpellDatabase — All enemy skillshots to be detected and dodged
// Sources:
//   EzEvade/Spells/SpellDatabase.cs (Hellsing)
//   SpellDatabase.lua (Hanbot)
// ============================================================================

namespace EzEvade {

static std::vector<SpellData> BuildSpellDatabase() {
    std::vector<SpellData> db;

    // =========================================================
    // AllChampions
    // =========================================================
    db.push_back({ .charName="AllChampions", .name="Mark/Snowball", .spellName="summonersnowball", .missileName="summonersnowball",
        .extraSpellNames={"summonerporothrow"}, .spellKey=SpellSlotId::Q,
        .spellType=SpellType::Line, .radius=60, .range=1600, .spellDelay=0, .projectileSpeed=1300,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=1 });

    // =========================================================
    // Aatrox
    // =========================================================
    db.push_back({ .charName="Aatrox", .name="Dark Flight", .spellName="AatroxQ", .missileName="AatroxQ",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Circular, .radius=285, .range=650, .spellDelay=650, .dangerlevel=3 });

    db.push_back({ .charName="Aatrox", .name="World Ender", .spellName="AatroxR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=550, .range=550, .spellDelay=250, .dangerlevel=4 });

    db.push_back({ .charName="Aatrox", .name="Infernal Chains", .spellName="AatroxW", .missileName="AatroxW",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Circular, .radius=160, .range=825, .spellDelay=250, .projectileSpeed=1800,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=4 });

    // =========================================================
    // Ahri
    // =========================================================
    db.push_back({ .charName="Ahri", .name="Orb of Deception", .spellName="AhriOrbofDeception", .missileName="AhriOrbMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=100, .range=925, .spellDelay=250, .projectileSpeed=1750, .dangerlevel=2 });

    db.push_back({ .charName="Ahri", .name="Orb of Deception (Return)", .spellName="AhriOrbofDeception2", .missileName="AhriOrbReturn",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=100, .range=925, .spellDelay=250, .projectileSpeed=915, .isSpecial=true, .dangerlevel=3 });

    db.push_back({ .charName="Ahri", .name="Charm", .spellName="AhriSeduce", .missileName="AhriSeduceMissile",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Line, .radius=60, .range=1000, .spellDelay=250, .projectileSpeed=1550,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3,
        .ccType=CCType::Hard });

    // =========================================================
    // Alistar
    // =========================================================
    db.push_back({ .charName="Alistar", .name="Pulverize", .spellName="Pulverize",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Circular, .radius=365, .range=365, .dangerlevel=3,
        .defaultOff=true, .ccType=CCType::Hard });

    // =========================================================
    // Amumu
    // =========================================================
    db.push_back({ .charName="Amumu", .name="Bandage Toss", .spellName="BandageToss", .missileName="SadMummyBandageToss",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=80, .range=1100, .spellDelay=250, .projectileSpeed=2000,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3,
        .ccType=CCType::Hard });

    db.push_back({ .charName="Amumu", .name="Curse of the Sad Mummy", .spellName="CurseoftheSadMummy",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=560, .range=560, .spellDelay=250, .dangerlevel=4,
        .ccType=CCType::Hard });

    // =========================================================
    // Anivia
    // =========================================================
    db.push_back({ .charName="Anivia", .name="Flash Frost", .spellName="FlashFrostSpell", .missileName="FlashFrostSpell",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=110, .range=1250, .spellDelay=250, .projectileSpeed=850, .dangerlevel=3,
        .ccType=CCType::Hard });

    // =========================================================
    // Annie
    // =========================================================
    db.push_back({ .charName="Annie", .name="Incinerate", .spellName="Incinerate",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Cone, .radius=80, .range=625, .angle=25, .spellDelay=250, .dangerlevel=2 });

    db.push_back({ .charName="Annie", .name="Summon: Tibbers", .spellName="InfernalGuardian",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=290, .range=600, .spellDelay=250, .dangerlevel=4 });

    // =========================================================
    // Ashe
    // =========================================================
    db.push_back({ .charName="Ashe", .name="Enchanted Crystal Arrow", .spellName="EnchantedCrystalArrow", .missileName="EnchantedCrystalArrow",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=130, .range=25000, .spellDelay=250, .projectileSpeed=1600, .dangerlevel=3,
        .ccType=CCType::Hard });

    db.push_back({ .charName="Ashe", .name="Volley", .spellName="Volley",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Line, .radius=20, .range=1350, .angle=5, .spellDelay=250, .projectileSpeed=1500,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions},
        .isSpecial=true, .dangerlevel=1 });

    // =========================================================
    // Aurelion Sol
    // =========================================================
    db.push_back({ .charName="AurelionSol", .name="Breath of Light", .spellName="AurelionSolQ", .missileName="AurelionSolQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=180, .range=1500, .fixedRange=true, .spellDelay=250, .projectileSpeed=850, .dangerlevel=2 });

    db.push_back({ .charName="AurelionSol", .name="Voice of Light (R)", .spellName="AurelionSolR", .missileName="AurelionSolRBeamMissile",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=120, .range=1420, .fixedRange=true, .spellDelay=300, .projectileSpeed=4600, .dangerlevel=4,
        .ccType=CCType::Soft });

    // =========================================================
    // Azir
    // =========================================================
    db.push_back({ .charName="Azir", .name="Emperor's Divide", .spellName="AzirR", .missileName="AzirSoldierRMissile",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=450, .range=700, .spellDelay=250, .projectileSpeed=1400, .dangerlevel=3,
        .ccType=CCType::Soft });

    // =========================================================
    // Bard
    // =========================================================
    db.push_back({ .charName="Bard", .name="Cosmic Binding", .spellName="BardQ", .missileName="BardQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=60, .range=950, .spellDelay=250, .projectileSpeed=1600, .dangerlevel=3,
        .ccType=CCType::Hard });

    db.push_back({ .charName="Bard", .name="Tempered Fate", .spellName="BardR", .missileName="BardR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=350, .range=3400, .spellDelay=250, .projectileSpeed=2100, .dangerlevel=2,
        .ccType=CCType::Hard });

    // =========================================================
    // Blitzcrank
    // =========================================================
    db.push_back({ .charName="Blitzcrank", .name="Rocket Grab", .spellName="RocketGrab", .missileName="RocketGrabMissile",
        .extraSpellNames={"RocketGrabMissile"}, .spellKey=SpellSlotId::Q,
        .spellType=SpellType::Line, .radius=70, .range=1050, .spellDelay=250, .projectileSpeed=1800,
        .extraDelay=75, .fixedRange=true,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3,
        .ccType=CCType::Hard });

    db.push_back({ .charName="Blitzcrank", .name="Static Field", .spellName="StaticField",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=600, .range=600, .spellDelay=250, .dangerlevel=2 });

    // =========================================================
    // Brand
    // =========================================================
    db.push_back({ .charName="Brand", .name="Sear", .spellName="BrandQ", .missileName="BrandQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=60, .range=1100, .spellDelay=250, .projectileSpeed=2000,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3,
        .ccType=CCType::Hard });

    db.push_back({ .charName="Brand", .name="Pillar of Flame", .spellName="BrandW",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Circular, .radius=250, .range=1100, .spellDelay=850, .dangerlevel=2 });

    // =========================================================
    // Braum
    // =========================================================
    db.push_back({ .charName="Braum", .name="Winter's Bite", .spellName="BraumQ", .missileName="BraumQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=100, .range=1000, .spellDelay=250, .projectileSpeed=1200,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3,
        .ccType=CCType::Hard });

    db.push_back({ .charName="Braum", .name="Glacial Fissure", .spellName="BraumRWrapper", .missileName="braumrmissile",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=115, .range=1250, .spellDelay=500, .projectileSpeed=1125, .dangerlevel=4,
        .ccType=CCType::Hard });

    // =========================================================
    // Caitlyn
    // =========================================================
    db.push_back({ .charName="Caitlyn", .name="Piltover Peacemaker", .spellName="CaitlynPiltoverPeacemaker",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=90, .range=1300, .spellDelay=625, .projectileSpeed=2200, .dangerlevel=2 });

    db.push_back({ .charName="Caitlyn", .name="90 Caliber Net", .spellName="CaitlynEntrapment", .missileName="CaitlynEntrapmentMissile",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Line, .radius=80, .range=950, .spellDelay=125, .projectileSpeed=2000,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3,
        .ccType=CCType::Soft });

    db.push_back({ .charName="Caitlyn", .name="Yordle Trap", .spellName="CaitlynYordleTrap",
        .trapBaseName="CaitlynTrap", .spellKey=SpellSlotId::W,
        .spellType=SpellType::Circular, .radius=75, .range=800, .hasTrap=true, .dangerlevel=3 });

    // =========================================================
    // Cassiopeia
    // =========================================================
    db.push_back({ .charName="Cassiopeia", .name="Petrifying Gaze", .spellName="CassiopeiaR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Cone, .radius=145, .range=825, .angle=40, .spellDelay=500, .dangerlevel=4,
        .ccType=CCType::Hard });

    db.push_back({ .charName="Cassiopeia", .name="Noxious Blast", .spellName="CassiopeiaQ", .missileName="CassiopeiaQ",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Circular, .radius=200, .range=850, .spellDelay=750, .dangerlevel=1 });

    // =========================================================
    // Cho'Gath
    // =========================================================
    db.push_back({ .charName="Chogath", .name="Rupture", .spellName="Rupture", .missileName="Rupture",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Circular, .radius=250, .range=950, .spellDelay=1200,
        .extraDrawHeight=45, .dangerlevel=3, .ccType=CCType::Hard });

    db.push_back({ .charName="Chogath", .name="Feral Scream", .spellName="FeralScream", .missileName="FeralScream",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Cone, .radius=80, .range=650, .angle=30, .spellDelay=250, .dangerlevel=2,
        .ccType=CCType::Hard });

    // =========================================================
    // Corki
    // =========================================================
    db.push_back({ .charName="Corki", .name="Missile Barrage", .spellName="MissileBarrage", .missileName="MissileBarrageMissile",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=40, .range=1300, .spellDelay=175, .projectileSpeed=2000,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=2 });

    db.push_back({ .charName="Corki", .name="Missile Barrage (Big)", .spellName="MissileBarrage2", .missileName="MissileBarrageMissile2",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=40, .range=1500, .spellDelay=175, .projectileSpeed=2000,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3 });

    db.push_back({ .charName="Corki", .name="Phosphorus Bomb", .spellName="PhosphorusBomb", .missileName="PhosphorusBombMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Circular, .radius=270, .range=825, .spellDelay=500, .projectileSpeed=1125,
        .extraDrawHeight=110, .dangerlevel=2 });

    // =========================================================
    // Darius
    // =========================================================
    db.push_back({ .charName="Darius", .name="Decimate", .spellName="DariusCleave",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Circular, .radius=425, .range=425, .spellDelay=750, .defaultOff=true, .dangerlevel=2 });

    db.push_back({ .charName="Darius", .name="Apprehend (Cone)", .spellName="DariusAxeGrabCone", .missileName="DariusAxeGrabCone",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Cone, .radius=55, .range=570, .angle=25, .spellDelay=320, .dangerlevel=3,
        .ccType=CCType::Hard });

    // =========================================================
    // Diana
    // =========================================================
    db.push_back({ .charName="Diana", .name="Crescent Strike", .spellName="DianaArc",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Arc, .radius=50, .range=850, .fixedRange=true, .spellDelay=250, .projectileSpeed=1400,
        .hasEndExplosion=true, .secondaryRadius=195, .extraEndTime=250, .dangerlevel=3 });

    // =========================================================
    // Dr. Mundo
    // =========================================================
    db.push_back({ .charName="DrMundo", .name="Infected Bonesaw", .spellName="InfectedCleaverMissileCast", .missileName="InfectedCleaverMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=60, .range=1050, .spellDelay=250, .projectileSpeed=2000,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=2 });

    // =========================================================
    // Draven
    // =========================================================
    db.push_back({ .charName="Draven", .name="Whirling Death", .spellName="DravenRCast", .missileName="DravenR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=160, .range=25000, .spellDelay=500, .projectileSpeed=2000, .dangerlevel=2 });

    db.push_back({ .charName="Draven", .name="Stand Aside", .spellName="DravenDoubleShot", .missileName="DravenDoubleShotMissile",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Line, .radius=130, .range=1050, .spellDelay=250, .projectileSpeed=1400,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=2,
        .ccType=CCType::Soft });

    return db;
}

// Global database instance (merged A-Z)
inline std::vector<SpellData>& GetSpellDatabase() {
    static std::vector<SpellData> db = []() {
        auto p1 = BuildSpellDatabase();
        auto p2 = BuildSpellDatabase2();
        p1.insert(p1.end(), p2.begin(), p2.end());
        return p1;
    }();
    return db;
}

// Lookup helpers
inline std::vector<const SpellData*> GetSpellsForChampion(const std::string& champName) {
    std::vector<const SpellData*> result;
    for (auto& s : GetSpellDatabase()) {
        if (s.charName == champName || s.charName == "AllChampions")
            result.push_back(&s);
    }
    return result;
}

inline const SpellData* FindSpellByMissileName(const std::string& missileName) {
    for (auto& s : GetSpellDatabase()) {
        if (s.missileName == missileName) return &s;
        for (auto& e : s.extraMissileNames)
            if (e == missileName) return &s;
    }
    return nullptr;
}

inline const SpellData* FindSpellBySpellName(const std::string& spellName) {
    for (auto& s : GetSpellDatabase()) {
        if (s.spellName == spellName) return &s;
        for (auto& e : s.extraSpellNames)
            if (e == spellName) return &s;
    }
    return nullptr;
}

} // namespace EzEvade
