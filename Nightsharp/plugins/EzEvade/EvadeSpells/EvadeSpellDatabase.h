#pragma once

#include <cstdint>
#include <string>
#include <vector>

// ============================================================================
// EzEvade evade spell database
//
// Sources:
//   - Original EzEvade EvadeSpellDatabase.cs
//   - CommunityDragon latest champion and item data
//   - Existing Nightsharp spell and gapcloser data for current internal names
//
// Notes:
//   - Internal spellName values are kept from EzEvade/Nightsharp where possible.
//   - Removed item entries are intentionally omitted.
//   - Only spells that can reasonably help dodge are listed.
// ============================================================================

namespace EzEvade {

//inline constexpr const char* kCommunityDragonLatestBase =
//    "https://raw.communitydragon.org/latest/";

enum class SpellSlotId : int {
    Q = 0,
    W = 1,
    E = 2,
    R = 3,
    Summoner1 = 4,
    Summoner2 = 5,
    Item1 = 6,
    Item2 = 7,
    Item3 = 8,
    Item4 = 9,
    Item5 = 10,
    Item6 = 11,
    Trinket = 12,
    Recall = 13,
    Unknown = 14
};

enum class CastType : int {
    Position,
    Target,
    Self
};

enum class SpellTargets : int {
    AllyMinions,
    EnemyMinions,
    AllyChampions,
    EnemyChampions,
    Targetables
};

enum class EvadeType : int {
    Blink,
    Dash,
    Invulnerability,
    MovementSpeedBuff,
    Shield,
    SpellShield,
    WindWall,
    Stasis,
    Untargetable
};

struct EvadeSpellData {
    std::string charName = {};
    std::string name = {};
    std::string spellName = {};
    SpellSlotId spellKey = SpellSlotId::Q;
    int dangerlevel = 1;
    bool checkSpellName = false;
    float spellDelay = 250.0f;
    float range = 0.0f;
    float speed = 0.0f;
    std::vector<float> speedArray = {};
    bool fixedRange = false;
    EvadeType evadeType = EvadeType::Dash;
    CastType castType = CastType::Position;
    std::vector<SpellTargets> spellTargets = {};
    bool isReversed = false;
    bool behindTarget = false;
    bool infrontTarget = false;
    bool isSummonerSpell = false;
    bool isItem = false;
    int itemID = 0;
    bool isSpecial = false;
    bool untargetable = false;
};

inline std::vector<EvadeSpellData> BuildEvadeSpellDatabase() {
    std::vector<EvadeSpellData> db;

    // AllChampions
    db.push_back(EvadeSpellData{
        .charName = "AllChampions",
        .name = "Flash",
        .spellName = "SummonerFlash",
        .spellKey = SpellSlotId::Summoner1,
        .dangerlevel = 4,
        .spellDelay = 50.0f,
        .range = 400.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Blink,
        .castType = CastType::Position,
        .isSummonerSpell = true
    });
    db.push_back(EvadeSpellData{
        .charName = "AllChampions",
        .name = "Ghost",
        .spellName = "SummonerHaste",
        .spellKey = SpellSlotId::Summoner2,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .speedArray = {27.0f, 27.0f, 27.0f, 27.0f, 27.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self,
        .isSummonerSpell = true
    });
    db.push_back(EvadeSpellData{
        .charName = "AllChampions",
        .name = "Youmuu's Ghostblade",
        .spellName = "YoumuusGhostblade",
        .spellKey = SpellSlotId::Item1,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .speedArray = {20.0f, 20.0f, 20.0f, 20.0f, 20.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self,
        .isItem = true,
        .itemID = 3142
    });
    db.push_back(EvadeSpellData{
        .charName = "AllChampions",
        .name = "Zhonya's Hourglass",
        .spellName = "ZhonyasHourglass",
        .spellKey = SpellSlotId::Item1,
        .dangerlevel = 4,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::Stasis,
        .castType = CastType::Self,
        .isItem = true,
        .itemID = 3157
    });

    // Aatrox
    db.push_back(EvadeSpellData{
        .charName = "Aatrox",
        .name = "Umbral Dash (E)",
        .spellName = "AatroxE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .range = 300.0f,
        .speed = 800.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Ahri
    db.push_back(EvadeSpellData{
        .charName = "Ahri",
        .name = "Spirit Rush (R)",
        .spellName = "AhriTumble",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 4,
        .spellDelay = 50.0f,
        .range = 500.0f,
        .speed = 1575.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Akali
    db.push_back(EvadeSpellData{
        .charName = "Akali",
        .name = "Shuriken Flip (E)",
        .spellName = "AkaliE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 825.0f,
        .speed = 2200.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });
    db.push_back(EvadeSpellData{
        .charName = "Akali",
        .name = "Perfect Execution (R)",
        .spellName = "AkaliR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 4,
        .spellDelay = 50.0f,
        .range = 675.0f,
        .speed = 1800.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Akshan
    db.push_back(EvadeSpellData{
        .charName = "Akshan",
        .name = "Heroic Swing (E)",
        .spellName = "AkshanE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 800.0f,
        .speed = 1200.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Ambessa
    db.push_back(EvadeSpellData{
        .charName = "Ambessa",
        .name = "Dashing Strike (E)",
        .spellName = "AmbessaE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 400.0f,
        .speed = 1100.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Aurora
    db.push_back(EvadeSpellData{
        .charName = "Aurora",
        .name = "The Weirding (E)",
        .spellName = "AuroraE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 500.0f,
        .speed = 1400.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Bard
    db.push_back(EvadeSpellData{
        .charName = "Bard",
        .name = "Tempered Fate (R)",
        .spellName = "BardR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 4,
        .spellDelay = 600.0f,
        .range = 3400.0f,
        .evadeType = EvadeType::Stasis,
        .castType = CastType::Position
    });

    // Blitzcrank
    db.push_back(EvadeSpellData{
        .charName = "Blitzcrank",
        .name = "Overdrive (W)",
        .spellName = "Overdrive",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 250.0f,
        .speedArray = {70.0f, 75.0f, 80.0f, 85.0f, 90.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Braum
    db.push_back(EvadeSpellData{
        .charName = "Braum",
        .name = "Unbreakable (E)",
        .spellName = "BraumE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::WindWall,
        .castType = CastType::Position
    });

    // Briar
    db.push_back(EvadeSpellData{
        .charName = "Briar",
        .name = "Blood Frenzy (W)",
        .spellName = "BriarW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .range = 300.0f,
        .speed = 1000.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });
    db.push_back(EvadeSpellData{
        .charName = "Briar",
        .name = "Certain Death (R)",
        .spellName = "BriarR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 4,
        .spellDelay = 100.0f,
        .range = 10000.0f,
        .speed = 2000.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position,
        .untargetable = true
    });

    // Caitlyn
    db.push_back(EvadeSpellData{
        .charName = "Caitlyn",
        .name = "90 Caliber Net (E)",
        .spellName = "CaitlynEntrapment",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 400.0f,
        .speed = 975.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position,
        .isReversed = true
    });

    // Camille
    db.push_back(EvadeSpellData{
        .charName = "Camille",
        .name = "Hookshot (E)",
        .spellName = "CamilleE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 800.0f,
        .speed = 1900.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Corki
    db.push_back(EvadeSpellData{
        .charName = "Corki",
        .name = "Valkyrie (W)",
        .spellName = "CarpetBomb",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 790.0f,
        .speed = 975.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Diana
    db.push_back(EvadeSpellData{
        .charName = "Diana",
        .name = "Lunar Rush (E)",
        .spellName = "DianaE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 825.0f,
        .speed = 2000.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions, SpellTargets::EnemyMinions}
    });

    // Draven
    db.push_back(EvadeSpellData{
        .charName = "Draven",
        .name = "Blood Rush (W)",
        .spellName = "DravenFury",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 2,
        .spellDelay = 250.0f,
        .speedArray = {40.0f, 45.0f, 50.0f, 55.0f, 60.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Ekko
    db.push_back(EvadeSpellData{
        .charName = "Ekko",
        .name = "Phase Dive (E)",
        .spellName = "EkkoE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 350.0f,
        .speed = 1150.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });
    db.push_back(EvadeSpellData{
        .charName = "Ekko",
        .name = "Chronobreak (R)",
        .spellName = "EkkoR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 5,
        .spellDelay = 50.0f,
        .range = 20000.0f,
        .evadeType = EvadeType::Blink,
        .castType = CastType::Self,
        .isSpecial = true
    });
    db.push_back(EvadeSpellData{
        .charName = "Ekko",
        .name = "PhaseDive2",
        .spellName = "EkkoEAttack",
        .spellKey = SpellSlotId::Recall,
        .dangerlevel = 3,
        .spellDelay = 250.0f,
        .range = 490.0f,
        .evadeType = EvadeType::Blink,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions, SpellTargets::EnemyMinions},
        .infrontTarget = true,
        .isSpecial = true
    });

    // Elise
    db.push_back(EvadeSpellData{
        .charName = "Elise",
        .name = "Rappel (E)",
        .spellName = "EliseSpiderEInitial",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 4,
        .checkSpellName = true,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::Untargetable,
        .castType = CastType::Self,
        .isSpecial = true,
        .untargetable = true
    });

    // Ezreal
    db.push_back(EvadeSpellData{
        .charName = "Ezreal",
        .name = "Arcane Shift (E)",
        .spellName = "EzrealArcaneShift",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 250.0f,
        .range = 475.0f,
        .evadeType = EvadeType::Blink,
        .castType = CastType::Position
    });

    // Fiora
    db.push_back(EvadeSpellData{
        .charName = "Fiora",
        .name = "Lunge (Q)",
        .spellName = "FioraQ",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 340.0f,
        .speed = 1100.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });
    db.push_back(EvadeSpellData{
        .charName = "Fiora",
        .name = "Riposte (W)",
        .spellName = "FioraW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 100.0f,
        .range = 750.0f,
        .evadeType = EvadeType::WindWall,
        .castType = CastType::Position
    });

    // Fizz
    db.push_back(EvadeSpellData{
        .charName = "Fizz",
        .name = "FizzPiercingStrike",
        .spellName = "FizzPiercingStrike",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 550.0f,
        .speed = 1400.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyMinions, SpellTargets::EnemyChampions}
    });
    db.push_back(EvadeSpellData{
        .charName = "Fizz",
        .name = "Playful / Trickster (E)",
        .spellName = "FizzJump",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 400.0f,
        .speed = 1400.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position,
        .untargetable = true
    });

    // Galio
    db.push_back(EvadeSpellData{
        .charName = "Galio",
        .name = "Justice Punch (E)",
        .spellName = "GalioE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 400.0f,
        .range = 650.0f,
        .speed = 2300.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Garen
    db.push_back(EvadeSpellData{
        .charName = "Garen",
        .name = "Decisive Strike",
        .spellName = "GarenQ",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 3,
        .spellDelay = 250.0f,
        .speedArray = {35.0f, 35.0f, 35.0f, 35.0f, 35.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Gnar
    db.push_back(EvadeSpellData{
        .charName = "Gnar",
        .name = "Hop (E)",
        .spellName = "GnarE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .checkSpellName = true,
        .spellDelay = 50.0f,
        .range = 475.0f,
        .speed = 900.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Gragas
    db.push_back(EvadeSpellData{
        .charName = "Gragas",
        .name = "Body Slam (E)",
        .spellName = "GragasBodySlam",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 600.0f,
        .speed = 900.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Graves
    db.push_back(EvadeSpellData{
        .charName = "Graves",
        .name = "Quickdraw (E)",
        .spellName = "GravesMove",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 425.0f,
        .speed = 1250.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Gwen
    db.push_back(EvadeSpellData{
        .charName = "Gwen",
        .name = "Skip 'n Slash (E)",
        .spellName = "GwenE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 350.0f,
        .speed = 1100.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Hecarim
    db.push_back(EvadeSpellData{
        .charName = "Hecarim",
        .name = "Devastating Charge (E)",
        .spellName = "HecarimRamp",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .speedArray = {25.0f, 30.0f, 35.0f, 40.0f, 45.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Hwei
    db.push_back(EvadeSpellData{
        .charName = "Hwei",
        .name = "Spiraling Despair (EE)",
        .spellName = "HweiEE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 250.0f,
        .speedArray = {30.0f, 30.0f, 30.0f, 30.0f, 30.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Irelia
    db.push_back(EvadeSpellData{
        .charName = "Irelia",
        .name = "Bladesurge (Q)",
        .spellName = "IreliaQ",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 600.0f,
        .speed = 1500.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions, SpellTargets::EnemyMinions}
    });

    // Janna
    db.push_back(EvadeSpellData{
        .charName = "Janna",
        .name = "Zephyr (W) - Passive Speed",
        .spellName = "JannaW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 1,
        .spellDelay = 250.0f,
        .speedArray = {6.0f, 7.5f, 9.0f, 10.5f, 12.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });
    db.push_back(EvadeSpellData{
        .charName = "Janna",
        .name = "Eye of the Storm (E)",
        .spellName = "JannaE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::Shield,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions}
    });

    // JarvanIV
    db.push_back(EvadeSpellData{
        .charName = "JarvanIV",
        .name = "Dragon Strike (Q+E combo)",
        .spellName = "JarvanIVDragonStrike",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 2,
        .spellDelay = 100.0f,
        .range = 770.0f,
        .speed = 1600.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Jax
    db.push_back(EvadeSpellData{
        .charName = "Jax",
        .name = "Leap Strike (Q)",
        .spellName = "JaxLeapStrike",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 700.0f,
        .speed = 1400.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions, SpellTargets::AllyMinions, SpellTargets::EnemyChampions, SpellTargets::EnemyMinions}
    });

    // Kaisa
    db.push_back(EvadeSpellData{
        .charName = "Kaisa",
        .name = "Supercharge (E)",
        .spellName = "KaisaE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .speedArray = {55.0f, 60.0f, 65.0f, 70.0f, 75.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });
    db.push_back(EvadeSpellData{
        .charName = "Kaisa",
        .name = "Killer Instinct (R)",
        .spellName = "KaisaR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 1500.0f,
        .speed = 1500.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Kalista
    db.push_back(EvadeSpellData{
        .charName = "Kalista",
        .name = "Martial Poise (Passive)",
        .spellName = "KalistaPassive",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .range = 250.0f,
        .speed = 700.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Karma
    db.push_back(EvadeSpellData{
        .charName = "Karma",
        .name = "Inspire (E)",
        .spellName = "KarmaE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .speedArray = {40.0f, 45.0f, 50.0f, 55.0f, 60.0f},
        .evadeType = EvadeType::Shield,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions}
    });

    // Kassadin
    db.push_back(EvadeSpellData{
        .charName = "Kassadin",
        .name = "Riftwalk (R)",
        .spellName = "RiftWalk",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 1,
        .spellDelay = 250.0f,
        .range = 500.0f,
        .evadeType = EvadeType::Blink,
        .castType = CastType::Position
    });

    // Katarina
    db.push_back(EvadeSpellData{
        .charName = "Katarina",
        .name = "Shunpo (E)",
        .spellName = "KatarinaE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 725.0f,
        .speed = 99999.0f,
        .evadeType = EvadeType::Blink,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::Targetables}
    });

    // Kayle
    db.push_back(EvadeSpellData{
        .charName = "Kayle",
        .name = "Celestial Blessing (W)",
        .spellName = "KayleW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 250.0f,
        .speedArray = {18.0f, 21.0f, 24.0f, 27.0f, 30.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions}
    });
    db.push_back(EvadeSpellData{
        .charName = "Kayle",
        .name = "Divine Judgment (R)",
        .spellName = "KayleR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 4,
        .spellDelay = 250.0f,
        .evadeType = EvadeType::Invulnerability,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions}
    });

    // Kayn
    db.push_back(EvadeSpellData{
        .charName = "Kayn",
        .name = "Reaping Slash (Q)",
        .spellName = "KaynQ",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .range = 350.0f,
        .speed = 900.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });
    db.push_back(EvadeSpellData{
        .charName = "Kayn",
        .name = "Shadow Step (E)",
        .spellName = "KaynE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .speedArray = {40.0f, 40.0f, 40.0f, 40.0f, 40.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });
    db.push_back(EvadeSpellData{
        .charName = "Kayn",
        .name = "Umbral Trespass (R)",
        .spellName = "KaynR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 4,
        .spellDelay = 50.0f,
        .range = 550.0f,
        .evadeType = EvadeType::Untargetable,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions},
        .untargetable = true
    });

    // Kennen
    db.push_back(EvadeSpellData{
        .charName = "Kennen",
        .name = "Lightning Rush (E)",
        .spellName = "KennenLightningRush",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 4,
        .spellDelay = 250.0f,
        .speedArray = {100.0f, 100.0f, 100.0f, 100.0f, 100.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Khazix
    db.push_back(EvadeSpellData{
        .charName = "Khazix",
        .name = "Leap (E)",
        .spellName = "KhazixE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 700.0f,
        .speed = 1000.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Kindred
    db.push_back(EvadeSpellData{
        .charName = "Kindred",
        .name = "Dance of Arrows (Q)",
        .spellName = "KindredQ",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .range = 300.0f,
        .speed = 733.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });
    db.push_back(EvadeSpellData{
        .charName = "Kindred",
        .name = "Lamb's Respite (R)",
        .spellName = "KindredR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 5,
        .spellDelay = 50.0f,
        .range = 500.0f,
        .evadeType = EvadeType::Invulnerability,
        .castType = CastType::Self
    });

    // KSante
    db.push_back(EvadeSpellData{
        .charName = "KSante",
        .name = "Footwork (E)",
        .spellName = "KSanteE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 350.0f,
        .speed = 1100.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions}
    });

    // Leblanc
    db.push_back(EvadeSpellData{
        .charName = "Leblanc",
        .name = "Distortion (W)",
        .spellName = "LeblancSlide",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 600.0f,
        .speed = 1600.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });
    db.push_back(EvadeSpellData{
        .charName = "Leblanc",
        .name = "Distortion Mimic (R)",
        .spellName = "LeblancSlideM",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 2,
        .checkSpellName = true,
        .spellDelay = 50.0f,
        .range = 600.0f,
        .speed = 1600.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // LeeSin
    db.push_back(EvadeSpellData{
        .charName = "LeeSin",
        .name = "Safeguard (W)",
        .spellName = "BlindMonkWOne",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 700.0f,
        .speed = 1400.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions, SpellTargets::AllyMinions}
    });

    // Lillia
    db.push_back(EvadeSpellData{
        .charName = "Lillia",
        .name = "Swirlseed Passive (Speed)",
        .spellName = "LilliaPassive",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .speedArray = {3.0f, 4.0f, 5.0f, 6.0f, 12.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Lissandra
    db.push_back(EvadeSpellData{
        .charName = "Lissandra",
        .name = "Frozen Tomb Self (R)",
        .spellName = "LissandraR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 4,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::Stasis,
        .castType = CastType::Self
    });

    // Lucian
    db.push_back(EvadeSpellData{
        .charName = "Lucian",
        .name = "Relentless Pursuit (E)",
        .spellName = "LucianE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .range = 425.0f,
        .speed = 1350.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Lulu
    db.push_back(EvadeSpellData{
        .charName = "Lulu",
        .name = "Whimsy (W) - Self/Ally",
        .spellName = "LuluW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .speedArray = {25.0f, 27.0f, 29.0f, 31.0f, 33.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions}
    });
    db.push_back(EvadeSpellData{
        .charName = "Lulu",
        .name = "Help, Pix! (E) - Shield",
        .spellName = "LuluE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::Shield,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions}
    });

    // Maokai
    db.push_back(EvadeSpellData{
        .charName = "Maokai",
        .name = "Twisted Advance (W)",
        .spellName = "MaokaiW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 525.0f,
        .speed = 1000.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions},
        .untargetable = true
    });

    // MasterYi
    db.push_back(EvadeSpellData{
        .charName = "MasterYi",
        .name = "Alpha Strike (Q)",
        .spellName = "AlphaStrike",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 3,
        .spellDelay = 100.0f,
        .range = 600.0f,
        .speed = 99999.0f,
        .evadeType = EvadeType::Blink,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions, SpellTargets::EnemyMinions},
        .untargetable = true
    });
    db.push_back(EvadeSpellData{
        .charName = "MasterYi",
        .name = "Highlander (R)",
        .spellName = "Highlander",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .speedArray = {25.0f, 35.0f, 45.0f, 55.0f, 55.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Mel
    db.push_back(EvadeSpellData{
        .charName = "Mel",
        .name = "Convergence (E)",
        .spellName = "MelE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::Shield,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions}
    });

    // Morgana
    db.push_back(EvadeSpellData{
        .charName = "Morgana",
        .name = "Black Shield (E)",
        .spellName = "BlackShield",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::SpellShield,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions}
    });

    // Naafiri
    db.push_back(EvadeSpellData{
        .charName = "Naafiri",
        .name = "Eviscerate (W)",
        .spellName = "NaafiriW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 750.0f,
        .speed = 1600.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions}
    });

    // Nami
    db.push_back(EvadeSpellData{
        .charName = "Nami",
        .name = "Tidecaller's Blessing (E) - Speed",
        .spellName = "NamiE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .speedArray = {15.0f, 15.0f, 15.0f, 15.0f, 15.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions}
    });

    // Nidalee
    db.push_back(EvadeSpellData{
        .charName = "Nidalee",
        .name = "Pounce (W-Spider)",
        .spellName = "Pounce",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 4,
        .spellDelay = 150.0f,
        .range = 375.0f,
        .speed = 1750.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position,
        .isSpecial = true
    });

    // Nilah
    db.push_back(EvadeSpellData{
        .charName = "Nilah",
        .name = "Jubilant Veil (W)",
        .spellName = "NilahW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::SpellShield,
        .castType = CastType::Self
    });
    db.push_back(EvadeSpellData{
        .charName = "Nilah",
        .name = "Slipstream (E)",
        .spellName = "NilahE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 550.0f,
        .speed = 1200.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions, SpellTargets::EnemyMinions, SpellTargets::AllyChampions}
    });

    // Nocturne
    db.push_back(EvadeSpellData{
        .charName = "Nocturne",
        .name = "Shroud of Darkness (W)",
        .spellName = "NocturneShroudofDarkness",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::SpellShield,
        .castType = CastType::Self
    });

    // Nunu
    db.push_back(EvadeSpellData{
        .charName = "Nunu",
        .name = "Biggest Snowball Ever! (W)",
        .spellName = "NunuW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 250.0f,
        .range = 7500.0f,
        .speed = 600.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position,
        .isSpecial = true
    });

    // Orianna
    db.push_back(EvadeSpellData{
        .charName = "Orianna",
        .name = "Command: Protect (E)",
        .spellName = "OrianaRedact",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::Shield,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions}
    });

    // Pantheon
    db.push_back(EvadeSpellData{
        .charName = "Pantheon",
        .name = "Shield Vault (W)",
        .spellName = "PantheonW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 600.0f,
        .speed = 1500.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions}
    });

    // Poppy
    db.push_back(EvadeSpellData{
        .charName = "Poppy",
        .name = "Steadfast Presence (W)",
        .spellName = "PoppyW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 250.0f,
        .speedArray = {27.0f, 29.0f, 31.0f, 33.0f, 35.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Pyke
    db.push_back(EvadeSpellData{
        .charName = "Pyke",
        .name = "Phantom Undertow (E)",
        .spellName = "PykeE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 550.0f,
        .speed = 2000.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Qiyana
    db.push_back(EvadeSpellData{
        .charName = "Qiyana",
        .name = "Audacity (E)",
        .spellName = "QiyanaE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 550.0f,
        .speed = 1400.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions, SpellTargets::EnemyMinions}
    });

    // Quinn
    db.push_back(EvadeSpellData{
        .charName = "Quinn",
        .name = "Heightened Senses (W)",
        .spellName = "QuinnW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .speedArray = {20.0f, 25.0f, 30.0f, 35.0f, 40.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Rammus
    db.push_back(EvadeSpellData{
        .charName = "Rammus",
        .name = "Powerball (Q)",
        .spellName = "RammusQ",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 2,
        .spellDelay = 250.0f,
        .speedArray = {105.0f, 110.0f, 115.0f, 120.0f, 125.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // RekSai
    db.push_back(EvadeSpellData{
        .charName = "RekSai",
        .name = "Tunnel (E)",
        .spellName = "RekSaiE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 250.0f,
        .speed = 1600.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Rell
    db.push_back(EvadeSpellData{
        .charName = "Rell",
        .name = "Ferromancy: Crash Down (W)",
        .spellName = "RellW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 2,
        .checkSpellName = true,
        .spellDelay = 50.0f,
        .range = 500.0f,
        .speed = 1000.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Renekton
    db.push_back(EvadeSpellData{
        .charName = "Renekton",
        .name = "Slice and Dice (E)",
        .spellName = "RenektonSliceAndDice",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 450.0f,
        .speed = 1050.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Riven
    db.push_back(EvadeSpellData{
        .charName = "Riven",
        .name = "Broken Wings (Q)",
        .spellName = "RivenTriCleave",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .range = 260.0f,
        .speed = 560.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position,
        .isSpecial = true
    });
    db.push_back(EvadeSpellData{
        .charName = "Riven",
        .name = "Valor (E)",
        .spellName = "RivenFeint",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .range = 325.0f,
        .speed = 1200.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Rumble
    db.push_back(EvadeSpellData{
        .charName = "Rumble",
        .name = "Scrap Shield",
        .spellName = "RumbleShield",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .speedArray = {10.0f, 15.0f, 20.0f, 25.0f, 30.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Samira
    db.push_back(EvadeSpellData{
        .charName = "Samira",
        .name = "Blade Whirl (W)",
        .spellName = "SamiraW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::WindWall,
        .castType = CastType::Self
    });
    db.push_back(EvadeSpellData{
        .charName = "Samira",
        .name = "Wild Rush (E)",
        .spellName = "SamiraE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 650.0f,
        .speed = 1600.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions, SpellTargets::EnemyMinions, SpellTargets::AllyChampions}
    });

    // Sejuani
    db.push_back(EvadeSpellData{
        .charName = "Sejuani",
        .name = "Arctic Assault (Q)",
        .spellName = "SejuaniQ",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 650.0f,
        .speed = 1000.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Shaco
    db.push_back(EvadeSpellData{
        .charName = "Shaco",
        .name = "Deceive (Q)",
        .spellName = "Deceive",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 3,
        .spellDelay = 250.0f,
        .range = 400.0f,
        .evadeType = EvadeType::Blink,
        .castType = CastType::Position
    });

    // Shen
    db.push_back(EvadeSpellData{
        .charName = "Shen",
        .name = "Spirit's Refuge (W)",
        .spellName = "ShenW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::WindWall,
        .castType = CastType::Self
    });
    db.push_back(EvadeSpellData{
        .charName = "Shen",
        .name = "Shadow Dash (E)",
        .spellName = "ShenE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 600.0f,
        .speed = 1200.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Shyvana
    db.push_back(EvadeSpellData{
        .charName = "Shyvana",
        .name = "Burnout",
        .spellName = "ShyvanaImmolationAura",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .speedArray = {30.0f, 35.0f, 40.0f, 45.0f, 50.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Sivir
    db.push_back(EvadeSpellData{
        .charName = "Sivir",
        .name = "Spell Shield (E)",
        .spellName = "SivirE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::SpellShield,
        .castType = CastType::Self
    });

    // Skarner
    db.push_back(EvadeSpellData{
        .charName = "Skarner",
        .name = "Ixtal's Impact (E)",
        .spellName = "SkarnerE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 550.0f,
        .speed = 1200.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Sona
    db.push_back(EvadeSpellData{
        .charName = "Sona",
        .name = "Song of Celerity (E)",
        .spellName = "SonaE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 250.0f,
        .speedArray = {13.0f, 14.0f, 15.0f, 16.0f, 25.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Sylas
    db.push_back(EvadeSpellData{
        .charName = "Sylas",
        .name = "Abscond (E)",
        .spellName = "SylasE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 400.0f,
        .speed = 1450.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // TahmKench
    db.push_back(EvadeSpellData{
        .charName = "TahmKench",
        .name = "Thick Skin (E) - Shield",
        .spellName = "TahmKenchE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::Shield,
        .castType = CastType::Self
    });

    // Talon
    db.push_back(EvadeSpellData{
        .charName = "Talon",
        .name = "Assassin's Path (E)",
        .spellName = "TalonE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 750.0f,
        .speed = 1400.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });
    db.push_back(EvadeSpellData{
        .charName = "Talon",
        .name = "Shadow Assualt",
        .spellName = "TalonShadowAssault",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 4,
        .spellDelay = 250.0f,
        .speedArray = {40.0f, 40.0f, 40.0f, 40.0f, 40.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Taric
    db.push_back(EvadeSpellData{
        .charName = "Taric",
        .name = "Cosmic Radiance (R)",
        .spellName = "TaricR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 5,
        .spellDelay = 2500.0f,
        .evadeType = EvadeType::Invulnerability,
        .castType = CastType::Self
    });

    // Teemo
    db.push_back(EvadeSpellData{
        .charName = "Teemo",
        .name = "Move Quick (W)",
        .spellName = "TeemoW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .speedArray = {10.0f, 14.0f, 18.0f, 22.0f, 26.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Tristana
    db.push_back(EvadeSpellData{
        .charName = "Tristana",
        .name = "Rocket Jump (W)",
        .spellName = "RocketJump",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 500.0f,
        .range = 900.0f,
        .speed = 1100.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Tryndamere
    db.push_back(EvadeSpellData{
        .charName = "Tryndamere",
        .name = "Spinning Slash (E)",
        .spellName = "Slash",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .range = 660.0f,
        .speed = 900.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });
    db.push_back(EvadeSpellData{
        .charName = "Tryndamere",
        .name = "Undying Rage (R)",
        .spellName = "TryndamereR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 5,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::Invulnerability,
        .castType = CastType::Self
    });

    // Udyr
    db.push_back(EvadeSpellData{
        .charName = "Udyr",
        .name = "Blazing Stampede (E)",
        .spellName = "UdyrE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .speedArray = {30.0f, 35.0f, 40.0f, 45.0f, 50.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Vayne
    db.push_back(EvadeSpellData{
        .charName = "Vayne",
        .name = "Tumble (Q)",
        .spellName = "VayneTumble",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .range = 300.0f,
        .speed = 900.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Vex
    db.push_back(EvadeSpellData{
        .charName = "Vex",
        .name = "Shadow Surge (R)",
        .spellName = "VexR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 3,
        .spellDelay = 250.0f,
        .range = 2000.0f,
        .speed = 1600.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Vi
    db.push_back(EvadeSpellData{
        .charName = "Vi",
        .name = "Vault Breaker (Q)",
        .spellName = "ViQ",
        .spellKey = SpellSlotId::Q,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 725.0f,
        .speed = 1500.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Viego
    db.push_back(EvadeSpellData{
        .charName = "Viego",
        .name = "Harrowed Path (E)",
        .spellName = "ViegoE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .speedArray = {20.0f, 22.5f, 25.0f, 27.5f, 30.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Vladimir
    db.push_back(EvadeSpellData{
        .charName = "Vladimir",
        .name = "Sanguine Pool (W)",
        .spellName = "VladimirW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::Untargetable,
        .castType = CastType::Self,
        .untargetable = true
    });

    // Warwick
    db.push_back(EvadeSpellData{
        .charName = "Warwick",
        .name = "Blood Hunt (W)",
        .spellName = "WarwickW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .speedArray = {35.0f, 40.0f, 45.0f, 50.0f, 55.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    // Xayah
    db.push_back(EvadeSpellData{
        .charName = "Xayah",
        .name = "Featherstorm (R)",
        .spellName = "XayahR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 4,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::Untargetable,
        .castType = CastType::Self,
        .untargetable = true
    });

    // XinZhao
    db.push_back(EvadeSpellData{
        .charName = "XinZhao",
        .name = "Audacious Charge (E)",
        .spellName = "XinZhaoE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 650.0f,
        .speed = 1500.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions, SpellTargets::EnemyMinions}
    });

    // Yasuo
    db.push_back(EvadeSpellData{
        .charName = "Yasuo",
        .name = "Wind Wall (W)",
        .spellName = "YasuoWMovingWall",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 3,
        .spellDelay = 250.0f,
        .range = 400.0f,
        .evadeType = EvadeType::WindWall,
        .castType = CastType::Position
    });
    db.push_back(EvadeSpellData{
        .charName = "Yasuo",
        .name = "Sweeping Blade (E)",
        .spellName = "YasuoDashWrapper",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 475.0f,
        .speed = 1000.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions, SpellTargets::EnemyMinions}
    });

    // Yone
    db.push_back(EvadeSpellData{
        .charName = "Yone",
        .name = "Spirit Cleave (W)",
        .spellName = "YoneW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 1,
        .spellDelay = 50.0f,
        .evadeType = EvadeType::Shield,
        .castType = CastType::Self
    });
    db.push_back(EvadeSpellData{
        .charName = "Yone",
        .name = "Soul Unbound (E)",
        .spellName = "YoneE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 300.0f,
        .speed = 1200.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });
    db.push_back(EvadeSpellData{
        .charName = "Yone",
        .name = "Fate Sealed (R)",
        .spellName = "YoneR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 4,
        .spellDelay = 50.0f,
        .range = 1000.0f,
        .speed = 1500.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position,
        .untargetable = true
    });

    // Yunara
    db.push_back(EvadeSpellData{
        .charName = "Yunara",
        .name = "Kanmei's Steps / Untouchable Shadow (E)",
        .spellName = "YunaraE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .speedArray = {30.0f, 35.0f, 40.0f, 45.0f, 50.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self,
        .isSpecial = true
    });

    // Yuumi
    db.push_back(EvadeSpellData{
        .charName = "Yuumi",
        .name = "You and Me! (W)",
        .spellName = "YuumiW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 4,
        .spellDelay = 50.0f,
        .range = 700.0f,
        .evadeType = EvadeType::Untargetable,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::AllyChampions},
        .untargetable = true
    });

    // Zac
    db.push_back(EvadeSpellData{
        .charName = "Zac",
        .name = "Elastic Slingshot (E)",
        .spellName = "ZacE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 900.0f,
        .range = 1800.0f,
        .speed = 1500.0f,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Zed
    db.push_back(EvadeSpellData{
        .charName = "Zed",
        .name = "Living Shadow (W)",
        .spellName = "ZedW",
        .spellKey = SpellSlotId::W,
        .dangerlevel = 2,
        .spellDelay = 250.0f,
        .range = 650.0f,
        .speed = 1750.0f,
        .evadeType = EvadeType::Blink,
        .castType = CastType::Position
    });
    db.push_back(EvadeSpellData{
        .charName = "Zed",
        .name = "Death Mark (R)",
        .spellName = "ZedR",
        .spellKey = SpellSlotId::R,
        .dangerlevel = 4,
        .spellDelay = 50.0f,
        .range = 625.0f,
        .evadeType = EvadeType::Blink,
        .castType = CastType::Target,
        .spellTargets = {SpellTargets::EnemyChampions},
        .untargetable = true
    });

    // Zeri
    db.push_back(EvadeSpellData{
        .charName = "Zeri",
        .name = "Spark Surge (E)",
        .spellName = "ZeriE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 2,
        .spellDelay = 50.0f,
        .range = 300.0f,
        .speed = 1200.0f,
        .fixedRange = true,
        .evadeType = EvadeType::Dash,
        .castType = CastType::Position
    });

    // Zilean
    db.push_back(EvadeSpellData{
        .charName = "Zilean",
        .name = "Time Warp (E)",
        .spellName = "ZileanE",
        .spellKey = SpellSlotId::E,
        .dangerlevel = 3,
        .spellDelay = 250.0f,
        .speedArray = {40.0f, 55.0f, 70.0f, 85.0f, 99.0f},
        .evadeType = EvadeType::MovementSpeedBuff,
        .castType = CastType::Self
    });

    return db;
}

inline std::vector<EvadeSpellData>& GetEvadeSpellDatabase() {
    static std::vector<EvadeSpellData> db = BuildEvadeSpellDatabase();
    return db;
}

inline std::vector<const EvadeSpellData*> GetEvadeSpellsForChampion(
    const std::string& champName) {
    std::vector<const EvadeSpellData*> result;

    for (auto& spell : GetEvadeSpellDatabase()) {
        if (spell.charName == champName || spell.charName == "AllChampions") {
            result.push_back(&spell);
        }
    }

    return result;
}

} // namespace EzEvade
