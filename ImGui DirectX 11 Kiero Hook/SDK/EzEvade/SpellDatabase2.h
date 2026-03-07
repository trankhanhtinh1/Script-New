#pragma once
#include "SpellData.h"
#include <vector>

// ============================================================================
// SpellDatabase Part 2 — E to Z champion skillshots
// Merged into one BuildSpellDatabase2() — called from SpellDatabase.h
// ============================================================================

namespace EzEvade {

static std::vector<SpellData> BuildSpellDatabase2() {
    std::vector<SpellData> db;

    // Ekko
    db.push_back({ .charName="Ekko", .name="Timewinder", .spellName="EkkoQ", .missileName="EkkoQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=60, .range=950, .spellDelay=250, .projectileSpeed=1650, .dangerlevel=2 });
    db.push_back({ .charName="Ekko", .name="Parallel Convergence", .spellName="EkkoW",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Circular, .radius=400, .range=1575, .spellDelay=3500, .dangerlevel=3, .ccType=CCType::Hard });
    db.push_back({ .charName="Ekko", .name="Phase Dive", .spellName="EkkoE",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Line, .radius=70, .range=350, .spellDelay=50, .projectileSpeed=1150, .dangerlevel=2 });

    // Elise
    db.push_back({ .charName="Elise", .name="Cocoon", .spellName="EliseHumanE", .missileName="EliseHumanEProjectile",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Line, .radius=55, .range=1075, .spellDelay=250, .projectileSpeed=1600,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3, .ccType=CCType::Hard });

    // Ezreal
    db.push_back({ .charName="Ezreal", .name="Mystic Shot", .spellName="EzrealQ", .missileName="EzrealQ",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=60, .range=1150, .spellDelay=250, .projectileSpeed=2000,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=2 });
    db.push_back({ .charName="Ezreal", .name="Essence Flux", .spellName="EzrealW", .missileName="EzrealW",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Line, .radius=80, .range=1150, .spellDelay=250, .projectileSpeed=1600, .dangerlevel=1 });
    db.push_back({ .charName="Ezreal", .name="Trueshot Barrage", .spellName="EzrealR", .missileName="EzrealR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=160, .range=25000, .spellDelay=1000, .projectileSpeed=2000, .dangerlevel=3 });

    // Galio
    db.push_back({ .charName="Galio", .name="Winds of War", .spellName="GalioQ", .missileName="GalioQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Circular, .radius=160, .range=825, .spellDelay=250, .projectileSpeed=1500, .dangerlevel=2 });
    db.push_back({ .charName="Galio", .name="Justice Punch", .spellName="GalioE",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Line, .radius=160, .range=650, .spellDelay=300, .projectileSpeed=2200, .dangerlevel=3, .ccType=CCType::Hard });

    // Gnar
    db.push_back({ .charName="Gnar", .name="Boomerang", .spellName="GnarQ", .missileName="GnarQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=55, .range=1100, .spellDelay=250, .projectileSpeed=2500,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=2 });
    db.push_back({ .charName="Gnar", .name="MEGA Gnar Slam (R)", .spellName="GnarR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=500, .range=500, .spellDelay=250, .dangerlevel=4, .ccType=CCType::Hard });

    // Gragas
    db.push_back({ .charName="Gragas", .name="Barrel Roll", .spellName="GragasQ", .missileName="GragasQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Circular, .radius=275, .range=850, .spellDelay=500, .projectileSpeed=1000, .dangerlevel=2 });
    db.push_back({ .charName="Gragas", .name="Explosive Cask", .spellName="GragasR", .missileName="GragasR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=400, .range=1050, .spellDelay=250, .projectileSpeed=1750, .dangerlevel=4, .ccType=CCType::Soft });

    // Graves
    db.push_back({ .charName="Graves", .name="End of the Line", .spellName="GravesQLineMis", .missileName="GravesQLineMis",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=40, .range=950, .spellDelay=250, .projectileSpeed=2000,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=2 });

    // Hecarim
    db.push_back({ .charName="Hecarim", .name="Onslaught of Shadows", .spellName="HecarimR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=200, .range=1000, .spellDelay=250, .projectileSpeed=1500, .dangerlevel=4, .ccType=CCType::Hard });

    // Heimerdinger
    db.push_back({ .charName="Heimerdinger", .name="CH-2 Electron Storm Grenade", .spellName="HeimerdingerE", .missileName="HeimerdingerE",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Circular, .radius=250, .range=975, .spellDelay=250, .projectileSpeed=1200, .dangerlevel=3, .ccType=CCType::Hard });

    // Irelia
    db.push_back({ .charName="Irelia", .name="Flawless Duet (E)", .spellName="IreliaE", .missileName="IreliaE",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Line, .radius=70, .range=1000, .spellDelay=250, .projectileSpeed=2000,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3, .ccType=CCType::Hard });
    db.push_back({ .charName="Irelia", .name="Vanguard's Edge (R)", .spellName="IreliaR", .missileName="IreliaRMissile",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=160, .range=1000, .spellDelay=250, .projectileSpeed=2200,
        .hasEndExplosion=true, .secondaryRadius=425, .dangerlevel=4, .ccType=CCType::Hard });

    // Janna
    db.push_back({ .charName="Janna", .name="Howling Gale", .spellName="JannaQ", .missileName="JannaQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=80, .range=1700, .spellDelay=0, .projectileSpeed=1700, .dangerlevel=2, .ccType=CCType::Hard });

    // Jarvan IV
    db.push_back({ .charName="JarvanIV", .name="Dragon Strike + Cataclysm combo", .spellName="JarvanIVEQ",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=70, .range=780, .spellDelay=250, .projectileSpeed=1800,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3, .ccType=CCType::Soft });

    // Jhin
    db.push_back({ .charName="Jhin", .name="Deadly Flourish", .spellName="JhinW", .missileName="JhinWMissile",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Line, .radius=70, .range=2500, .spellDelay=750, .projectileSpeed=5000, .dangerlevel=3, .ccType=CCType::Hard });
    db.push_back({ .charName="Jhin", .name="Curtain Call", .spellName="JhinR", .missileName="JhinRShotMis",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=80, .range=25000, .spellDelay=250, .projectileSpeed=5000, .dangerlevel=3 });

    // Jinx
    db.push_back({ .charName="Jinx", .name="Zap!", .spellName="JinxW", .missileName="JinxWMissile",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Line, .radius=60, .range=1500, .spellDelay=600, .projectileSpeed=3300,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3, .ccType=CCType::Soft });
    db.push_back({ .charName="Jinx", .name="Super Mega Death Rocket!", .spellName="JinxR", .missileName="JinxR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=140, .range=25000, .spellDelay=600, .projectileSpeed=1700, .dangerlevel=4 });
    db.push_back({ .charName="Jinx", .name="Flame Chompers", .spellName="JinxE", .missileName="JinxE",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Circular, .radius=100, .range=920, .spellDelay=500, .hasTrap=true, .dangerlevel=3, .ccType=CCType::Hard });

    // Kai'Sa
    db.push_back({ .charName="Kaisa", .name="Void Seeker (W)", .spellName="KaisaW", .missileName="KaisaWMis",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Line, .radius=100, .range=3000, .spellDelay=500, .projectileSpeed=1750, .fixedRange=true,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3 });

    // Karma
    db.push_back({ .charName="Karma", .name="Inner Flame", .spellName="KarmaQ", .missileName="KarmaQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=60, .range=950, .spellDelay=250, .projectileSpeed=1700,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=2 });
    db.push_back({ .charName="Karma", .name="Soulflare (Mantra Q)", .spellName="KarmaQMantra", .missileName="KarmaQMissileMantra",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Circular, .radius=250, .range=950, .spellDelay=250, .projectileSpeed=1700,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3 });

    // Kassadin
    db.push_back({ .charName="Kassadin", .name="Null Sphere", .spellName="KassadinQ", .missileName="KassadinQ",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=70, .range=650, .spellDelay=250, .projectileSpeed=1400,
        .collisionObjects={CollisionObjectType::EnemyChampions}, .dangerlevel=3, .ccType=CCType::Hard });

    // Katarina
    db.push_back({ .charName="Katarina", .name="Bouncing Blade", .spellName="KatarinaQ", .missileName="KatarinaQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=75, .range=675, .spellDelay=250, .projectileSpeed=1800, .dangerlevel=2 });

    // Kennen
    db.push_back({ .charName="Kennen", .name="Thundering Shuriken", .spellName="KennenShurikenHurlMissile1", .missileName="KennenShurikenHurlMissile1",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=50, .range=900, .spellDelay=175, .projectileSpeed=1700,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3 });

    // Kha'Zix
    db.push_back({ .charName="Khazix", .name="Void Spike", .spellName="KhazixW", .missileName="KhazixWMissile",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Line, .radius=70, .range=1000, .spellDelay=250, .projectileSpeed=1700,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=2 });

    // Kindred
    db.push_back({ .charName="Kindred", .name="Dance of Arrows (Q)", .spellName="KindredQ",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=50, .range=300, .spellDelay=50, .projectileSpeed=733, .fixedRange=true, .dangerlevel=1 });

    // LeBlanc
    db.push_back({ .charName="Leblanc", .name="Ethereal Chains", .spellName="LeblancE", .missileName="LeblancEMissile",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Line, .radius=55, .range=950, .spellDelay=250, .projectileSpeed=1750,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3, .ccType=CCType::Hard });

    // Lee Sin
    db.push_back({ .charName="Lesin", .name="Resonating Strike (Q2)", .spellName="BlindMonkQTwo",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=60, .range=1200, .spellDelay=250, .projectileSpeed=1800,
        .collisionObjects={CollisionObjectType::EnemyChampions}, .dangerlevel=3 });

    // Lissandra
    db.push_back({ .charName="Lissandra", .name="Ice Shard", .spellName="LissandraQ", .missileName="LissandraQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=75, .range=725, .spellDelay=250, .projectileSpeed=2200,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=2 });
    db.push_back({ .charName="Lissandra", .name="Frozen Tomb (R)", .spellName="LissandraR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=550, .range=550, .spellDelay=250, .dangerlevel=4, .ccType=CCType::Hard });
    db.push_back({ .charName="Lissandra", .name="Ring of Frost (W)", .spellName="LissandraW",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Circular, .radius=450, .range=450, .spellDelay=250, .dangerlevel=3, .ccType=CCType::Hard });
    db.push_back({ .charName="Lissandra", .name="Glacial Path (E)", .spellName="LissandraE", .missileName="LissandraE",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Line, .radius=100, .range=1050, .spellDelay=250, .projectileSpeed=850,
        .fixedRange=true, .dangerlevel=2 });

    // Lucian
    db.push_back({ .charName="Lucian", .name="Ardent Blaze (W)", .spellName="LucianW", .missileName="LucianWMissile",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Circular, .radius=320, .range=900, .spellDelay=250, .projectileSpeed=1600, .dangerlevel=1 });
    db.push_back({ .charName="Lucian", .name="The Culling (R)", .spellName="LucianR", .missileName="LucianRMissile",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=75, .range=1200, .spellDelay=250, .projectileSpeed=2800, .dangerlevel=3 });

    // Lux
    db.push_back({ .charName="Lux", .name="Light Binding (Q)", .spellName="LuxLightBindingMis", .missileName="LuxLightBindingMis",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=70, .range=1300, .spellDelay=250, .projectileSpeed=1200,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3, .ccType=CCType::Hard });
    db.push_back({ .charName="Lux", .name="Lucent Singularity (E)", .spellName="LuxLightStrikeKugel", .missileName="LuxMissileUnlock",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Circular, .radius=325, .range=1100, .spellDelay=250, .projectileSpeed=1300, .dangerlevel=2 });
    db.push_back({ .charName="Lux", .name="Final Spark (R)", .spellName="LuxR", .missileName="LuxR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=200, .range=3340, .spellDelay=500, .projectileSpeed=3000, .fixedRange=true, .dangerlevel=4 });

    // Malphite
    db.push_back({ .charName="Malphite", .name="Unstoppable Force (R)", .spellName="UFSlash",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=300, .range=1500, .spellDelay=250, .projectileSpeed=2000, .dangerlevel=5, .ccType=CCType::Hard });
    db.push_back({ .charName="Malphite", .name="Seismic Shard (Q)", .spellName="SeismicShard", .missileName="SeismicShard",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=80, .range=675, .spellDelay=250, .projectileSpeed=1200,
        .collisionObjects={CollisionObjectType::EnemyChampions}, .dangerlevel=2, .ccType=CCType::Soft });

    // Morgana
    db.push_back({ .charName="Morgana", .name="Dark Binding (Q)", .spellName="MorganaQ", .missileName="MorganaQ",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=70, .range=1300, .spellDelay=250, .projectileSpeed=1200,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=4, .ccType=CCType::Hard });
    db.push_back({ .charName="Morgana", .name="Soul Shackles (R)", .spellName="MorganaRWrapper",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=600, .range=600, .spellDelay=250, .dangerlevel=4, .ccType=CCType::Hard });

    // Nautilus
    db.push_back({ .charName="Nautilus", .name="Dredge Line (Q)", .spellName="NautilusAnchorDrag", .missileName="NautilusAnchorDragMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=90, .range=1080, .spellDelay=250, .projectileSpeed=2000,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .fixedRange=true, .dangerlevel=4, .ccType=CCType::Hard });

    // Nidalee
    db.push_back({ .charName="Nidalee", .name="Javelin Toss (Q)", .spellName="JavelinToss", .missileName="JavelinToss",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=40, .range=1500, .spellDelay=250, .projectileSpeed=1300,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3 });

    // Orianna
    db.push_back({ .charName="Orianna", .name="Command: Attack (Q)", .spellName="OrianaIzazvati", .missileName="OrianaQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=80, .range=825, .spellDelay=0, .projectileSpeed=1100, .dangerlevel=2 });
    db.push_back({ .charName="Orianna", .name="Command: Shockwave (R)", .spellName="OrianaDetonateCommand",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=410, .range=0, .spellDelay=250, .dangerlevel=5, .ccType=CCType::Hard });

    // Pantheon
    db.push_back({ .charName="Pantheon", .name="Comet Spear (Q)", .spellName="PantheonQ", .missileName="PantheonQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=65, .range=1150, .spellDelay=250, .projectileSpeed=2300,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=2 });

    // Pyke
    db.push_back({ .charName="Pyke", .name="Bone Skewer (Q)", .spellName="PykeQ", .missileName="PykeQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=70, .range=1100, .spellDelay=250, .projectileSpeed=2000,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3, .ccType=CCType::Hard });
    db.push_back({ .charName="Pyke", .name="Phantom Undertow (E)", .spellName="PykeE",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Line, .radius=90, .range=550, .spellDelay=0, .projectileSpeed=2200, .dangerlevel=3, .ccType=CCType::Hard });

    // Rengar
    db.push_back({ .charName="Rengar", .name="Savagery (Q)", .spellName="RengarQ",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=55, .range=300, .spellDelay=150, .dangerlevel=2 });

    // Riven
    db.push_back({ .charName="Riven", .name="Blade of the Exile (R - Wind Slash)", .spellName="RivenIzunaSlashMissile", .missileName="RivenLightsaberMissile",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=100, .range=1100, .spellDelay=250, .projectileSpeed=2200, .dangerlevel=4 });

    // Sejuani
    db.push_back({ .charName="Sejuani", .name="Arctic Assault (Q)", .spellName="SejuaniQ",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=90, .range=650, .spellDelay=250, .projectileSpeed=1500, .dangerlevel=3, .ccType=CCType::Hard });
    db.push_back({ .charName="Sejuani", .name="Glacial Prison (R)", .spellName="SejuaniR", .missileName="SejuaniRThrow",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=350, .range=1300, .spellDelay=250, .projectileSpeed=1600, .dangerlevel=5, .ccType=CCType::Hard });

    // Sivir
    db.push_back({ .charName="Sivir", .name="Boomerang Blade (Q)", .spellName="SivirQ", .missileName="SivirQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=90, .range=1250, .spellDelay=250, .projectileSpeed=1350, .dangerlevel=2 });

    // Sona
    db.push_back({ .charName="Sona", .name="Crescendo (R)", .spellName="SonaR", .missileName="SonaR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=140, .range=1000, .spellDelay=250, .projectileSpeed=2400, .dangerlevel=5, .ccType=CCType::Hard });

    // Syndra
    db.push_back({ .charName="Syndra", .name="Dark Sphere (Q)", .spellName="SyndraQ",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Circular, .radius=250, .range=750, .spellDelay=625, .dangerlevel=2 });
    db.push_back({ .charName="Syndra", .name="Scatter the Weak (E)", .spellName="SyndraE",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Cone, .radius=50, .range=700, .angle=34, .spellDelay=250, .dangerlevel=3, .ccType=CCType::Hard });
    db.push_back({ .charName="Syndra", .name="Force of Will (W)", .spellName="SyndraW", .missileName="SyndraW",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Circular, .radius=250, .range=925, .spellDelay=250, .projectileSpeed=1750, .dangerlevel=2, .ccType=CCType::Hard });

    // Taliyah
    db.push_back({ .charName="Taliyah", .name="Threaded Volley (Q)", .spellName="TaliyahQ", .missileName="TaliyahQMis",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=60, .range=1000, .spellDelay=250, .projectileSpeed=1700,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=2 });

    // Thresh
    db.push_back({ .charName="Thresh", .name="Death Sentence (Q)", .spellName="ThreshQ", .missileName="ThreshQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=70, .range=1100, .spellDelay=500, .projectileSpeed=1900,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .fixedRange=true, .dangerlevel=4, .ccType=CCType::Hard });
    db.push_back({ .charName="Thresh", .name="Flay (E)", .spellName="ThreshEQ",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Line, .radius=110, .range=500, .spellDelay=250, .dangerlevel=3, .ccType=CCType::Soft });

    // Twisted Fate
    db.push_back({ .charName="TwistedFate", .name="Wild Cards (Q)", .spellName="WildCards", .missileName="WildCard1",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=40, .range=1450, .spellDelay=250, .projectileSpeed=1000, .dangerlevel=2 });
    db.push_back({ .charName="TwistedFate", .name="Gold Card (CC)", .spellName="TwistedFatePick",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Line, .radius=40, .range=1450, .spellDelay=250, .projectileSpeed=2000, .dangerlevel=4, .ccType=CCType::Hard });

    // Urgot
    db.push_back({ .charName="Urgot", .name="Corrosive Charge (E)", .spellName="UrgotE",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Circular, .radius=200, .range=800, .spellDelay=450, .dangerlevel=3, .ccType=CCType::Soft });

    // Varus
    db.push_back({ .charName="Varus", .name="Piercing Arrow (Q)", .spellName="VarusQ", .missileName="VarusQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=70, .range=1825, .spellDelay=0, .projectileSpeed=1900,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3 });
    db.push_back({ .charName="Varus", .name="Chain of Corruption (R)", .spellName="VarusR", .missileName="VarusRMissile",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Line, .radius=120, .range=1300, .spellDelay=250, .projectileSpeed=1950,
        .collisionObjects={CollisionObjectType::EnemyChampions}, .dangerlevel=4, .ccType=CCType::Hard });

    // Vel'Koz
    db.push_back({ .charName="Velkoz", .name="Plasma Fission (Q)", .spellName="VelkozQ", .missileName="VelkozQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=55, .range=1250, .spellDelay=250, .projectileSpeed=1300, .fixedRange=true,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=2 });
    db.push_back({ .charName="Velkoz", .name="Void Rift (W)", .spellName="VelkozW", .missileName="VelkozWMissile",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Line, .radius=90, .range=1150, .spellDelay=250, .projectileSpeed=1700, .fixedRange=true, .extraEndTime=1000, .dangerlevel=2 });
    db.push_back({ .charName="Velkoz", .name="Tectonic Disruption (E)", .spellName="VelkozE",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Circular, .radius=225, .range=800, .spellDelay=250, .projectileSpeed=1500, .dangerlevel=3, .ccType=CCType::Hard });

    // Viktor
    db.push_back({ .charName="Viktor", .name="Death Ray (E)", .spellName="ViktorDeathRay", .missileName="ViktorDeathRayMissile",
        .extraMissileNames={"ViktorEAugMissile"}, .spellKey=SpellSlotId::E,
        .spellType=SpellType::Line, .radius=75, .range=815, .projectileSpeed=1050, .fixedRange=true, .dangerlevel=3 });
    db.push_back({ .charName="Viktor", .name="Gravity Field (W)", .spellName="ViktorGravitonField",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Circular, .radius=300, .range=625, .spellDelay=1500, .defaultOff=true, .dangerlevel=3, .ccType=CCType::Hard });

    // Vladimir
    db.push_back({ .charName="Vladimir", .name="Hemoplague (R)", .spellName="VladimirR", .missileName="VladimirR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=375, .range=700, .spellDelay=250, .dangerlevel=3 });

    // Xerath
    db.push_back({ .charName="Xerath", .name="Arcanopulse (Q)", .spellName="XerathArcanopulse2", .missileName="XerathArcanopulse2",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=70, .range=1525, .spellDelay=500, .dangerlevel=2 });
    db.push_back({ .charName="Xerath", .name="Eye of Destruction (W)", .spellName="XerathArcaneBarrage2", .missileName="XerathArcaneBarrage2",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Circular, .radius=280, .range=1000, .spellDelay=750, .extraDrawHeight=45, .dangerlevel=2 });
    db.push_back({ .charName="Xerath", .name="Shocking Orb (E)", .spellName="XerathMageSpear", .missileName="XerathMageSpearMissile",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Line, .radius=60, .range=1125, .spellDelay=200, .projectileSpeed=1600, .fixedRange=true,
        .collisionObjects={CollisionObjectType::EnemyChampions, CollisionObjectType::EnemyMinions}, .dangerlevel=3, .ccType=CCType::Hard });
    db.push_back({ .charName="Xerath", .name="Rite of the Arcane (R)", .spellName="xerathrmissilewrapper", .missileName="XerathLocusPulse",
        .extraSpellNames={"XerathLocusPulse"}, .spellKey=SpellSlotId::R,
        .spellType=SpellType::Circular, .radius=200, .range=5600, .spellDelay=600, .dangerlevel=3 });

    // Yasuo
    db.push_back({ .charName="Yasuo", .name="Steel Tempest", .spellName="YasuoQ", .missileName="yasuoq",
        .extraMissileNames={"yasuoq2"}, .extraSpellNames={"YasuoQ2"}, .spellKey=SpellSlotId::Q,
        .spellType=SpellType::Line, .radius=40, .range=550, .spellDelay=400, .fixedRange=true, .invert=true, .dangerlevel=2 });
    db.push_back({ .charName="Yasuo", .name="Steel Tempest (Tornado)", .spellName="YasuoQ3W", .missileName="YasuoQ3Mis",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=90, .range=1150, .spellDelay=300, .projectileSpeed=1250, .fixedRange=true, .dangerlevel=3 });

    // Zac
    db.push_back({ .charName="Zac", .name="Stretching Strike (Q)", .spellName="ZacQ", .missileName="ZacQ",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=120, .range=550, .spellDelay=400, .fixedRange=true, .dangerlevel=3, .ccType=CCType::Hard });

    // Zed
    db.push_back({ .charName="Zed", .name="Razor Shuriken (Q)", .spellName="ZedQ", .missileName="ZedQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Line, .radius=50, .range=925, .spellDelay=250, .projectileSpeed=1700, .dangerlevel=3 });

    // Ziggs
    db.push_back({ .charName="Ziggs", .name="Bouncing Bomb (Q)", .spellName="ZiggsQ",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Circular, .radius=150, .range=850, .spellDelay=125, .projectileSpeed=1700, .isSpecial=true, .noProcess=true, .dangerlevel=2 });
    db.push_back({ .charName="Ziggs", .name="Satchel Charge (W)", .spellName="ZiggsW", .missileName="ZiggsW",
        .spellKey=SpellSlotId::W, .spellType=SpellType::Circular, .radius=275, .range=1000, .spellDelay=250, .projectileSpeed=2000, .extraEndTime=1000, .dangerlevel=2 });
    db.push_back({ .charName="Ziggs", .name="Hexplosive Minefield (E)", .spellName="ZiggsE", .missileName="ZiggsE",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Circular, .radius=235, .range=2000, .spellDelay=250, .projectileSpeed=3000, .dangerlevel=1 });
    db.push_back({ .charName="Ziggs", .name="Mega Inferno Bomb (R)", .spellName="ZiggsR", .missileName="ZiggsR",
        .spellKey=SpellSlotId::R, .spellType=SpellType::Circular, .radius=500, .range=5300, .spellDelay=400, .projectileSpeed=1550, .defaultOff=true, .isSpecial=true, .dangerlevel=4 });

    // Zilean
    db.push_back({ .charName="Zilean", .name="Time Bomb (Q)", .spellName="ZileanQ", .missileName="ZileanQMissile",
        .spellKey=SpellSlotId::Q, .spellType=SpellType::Circular, .radius=150, .range=900, .spellDelay=650, .extraEndTime=1000, .isSpecial=true, .dangerlevel=3 });

    // Zyra
    db.push_back({ .charName="Zyra", .name="Grasping Roots (E)", .spellName="ZyraE", .missileName="ZyraEMissile",
        .spellKey=SpellSlotId::E, .spellType=SpellType::Line, .radius=70, .range=1150, .spellDelay=250, .projectileSpeed=1400, .fixedRange=true, .dangerlevel=3, .ccType=CCType::Hard });
    db.push_back({ .charName="Zyra", .name="Stranglethorns (R)", .spellName="ZyraR",
        .extraSpellNames={"ZyraBrambleZone"}, .spellKey=SpellSlotId::R,
        .spellType=SpellType::Circular, .radius=525, .range=700, .spellDelay=500, .extraEndTime=2000, .defaultOff=true, .dangerlevel=4, .ccType=CCType::Hard });

    return db;
}

} // namespace EzEvade
