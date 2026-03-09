#pragma once
#include "../Spells/SpellData.h"
#include <algorithm>
#include <string>
#include <vector>

// ============================================================================
// EvadeSpellDatabase
// Defensive, movement, and utility spells used to dodge threats.
// Source:
//   EzEvade/EvadeSpells/EvadeSpellDatabase.cs
// Patch target:
//   26.5 (2026-03-03)
// References:
//   - Riot patch notes 26.5
//   - League Wiki/Fandom champion pages (Ezreal, Kassadin)
// ============================================================================

namespace EzEvade {

namespace detail {

inline constexpr const char* kEvadeDatabasePatchVersion = "26.5";

inline void ApplyPatch265WikiOverrides(EvadeSpellData& spell) {
    // Ezreal E target range is 475 (wiki).
    if (spell.charName == "Ezreal" && spell.spellName == "EzrealArcaneShift") {
        spell.range = 475.0f;
    }

    // Kassadin R target range is 500 (wiki).
    if (spell.charName == "Kassadin" && spell.spellName == "RiftWalk") {
        spell.range = 500.0f;
    }
}

inline bool IsRemovedInPatch265(const EvadeSpellData& spell) {
    // Galeforce is not available on current SR patch set.
    return spell.spellName == "Galeforce";
}

inline void FinalizePatch265(std::vector<EvadeSpellData>& db) {
    for (auto& spell : db) {
        ApplyPatch265WikiOverrides(spell);
    }

    db.erase(std::remove_if(db.begin(), db.end(), [](const EvadeSpellData& spell) {
        return IsRemovedInPatch265(spell);
    }), db.end());

    std::stable_sort(db.begin(), db.end(), [](const EvadeSpellData& left, const EvadeSpellData& right) {
        if (left.charName != right.charName) {
            return left.charName < right.charName;
        }
        if (left.spellKey != right.spellKey) {
            return static_cast<int>(left.spellKey) < static_cast<int>(right.spellKey);
        }
        return left.spellName < right.spellName;
    });
}

} // namespace detail

static std::vector<EvadeSpellData> BuildEvadeSpellDatabase() {
    std::vector<EvadeSpellData> db;

    // =========================================================
    // AllChampions - Summoner Spells & Items
    // =========================================================
    db.push_back({ .charName="AllChampions", .name="Flash", .spellName="SummonerFlash",
        .spellKey=SpellSlotId::F, .range=400, .spellDelay=50, .fixedRange=true,
        .evadeType=EvadeType::Blink, .castType=CastType::Position, .isSummonerSpell=true, .dangerlevel=4 });

    db.push_back({ .charName="AllChampions", .name="Ghost", .spellName="SummonerHaste",
        .spellKey=SpellSlotId::F, .spellDelay=50, .speedArray={27, 27, 27, 27, 27},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .isSummonerSpell=true, .dangerlevel=3 });

    db.push_back({ .charName="AllChampions", .name="Zhonya's Hourglass", .spellName="ZhonyasHourglass",
        .spellKey=SpellSlotId::Q, .spellDelay=50,
        .evadeType=EvadeType::Stasis, .castType=CastType::Self, .isItem=true, .itemID=3157, .dangerlevel=4 });

    db.push_back({ .charName="AllChampions", .name="Youmuu's Ghostblade", .spellName="YoumuusGhostblade",
        .spellKey=SpellSlotId::Q, .spellDelay=50, .speedArray={20, 20, 20, 20, 20},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .isItem=true, .itemID=3142, .dangerlevel=2 });

    db.push_back({ .charName="AllChampions", .name="Galeforce", .spellName="Galeforce",
        .spellKey=SpellSlotId::Q, .range=450, .spellDelay=50, .speed=1600,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .isItem=true, .itemID=6695, .dangerlevel=3 });

    // =========================================================
    // Ahri
    // =========================================================
    db.push_back({ .charName="Ahri", .name="Spirit Rush (R)", .spellName="AhriTumble",
        .spellKey=SpellSlotId::R, .range=500, .spellDelay=50, .speed=1575,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=4 });

    // =========================================================
    // Akali
    // =========================================================
    db.push_back({ .charName="Akali", .name="Shuriken Flip (E)", .spellName="AkaliE",
        .spellKey=SpellSlotId::E, .range=825, .spellDelay=50, .speed=2200, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // =========================================================
    // Blitzcrank
    // =========================================================
    db.push_back({ .charName="Blitzcrank", .name="Overdrive (W)", .spellName="Overdrive",
        .spellKey=SpellSlotId::W, .spellDelay=250, .speedArray={70, 75, 80, 85, 90},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=3 });

    // =========================================================
    // Caitlyn
    // =========================================================
    db.push_back({ .charName="Caitlyn", .name="90 Caliber Net (E) - Knocked Back", .spellName="CaitlynEntrapment",
        .spellKey=SpellSlotId::E, .range=400, .spellDelay=50, .speed=975, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .isReversed=true, .dangerlevel=3 });

    // =========================================================
    // Corki
    // =========================================================
    db.push_back({ .charName="Corki", .name="Valkyrie (W)", .spellName="CarpetBomb",
        .spellKey=SpellSlotId::W, .range=790, .spellDelay=50, .speed=975,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // =========================================================
    // Draven
    // =========================================================
    db.push_back({ .charName="Draven", .name="Blood Rush (W)", .spellName="DravenFury",
        .spellKey=SpellSlotId::W, .spellDelay=250, .speedArray={40, 45, 50, 55, 60},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=2 });

    // =========================================================
    // Ekko
    // =========================================================
    db.push_back({ .charName="Ekko", .name="Phase Dive (E)", .spellName="EkkoE",
        .spellKey=SpellSlotId::E, .range=350, .spellDelay=50, .speed=1150, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });
    db.push_back({ .charName="Ekko", .name="Chronobreak (R)", .spellName="EkkoR",
        .spellKey=SpellSlotId::R, .range=20000, .spellDelay=50,
        .evadeType=EvadeType::Blink, .castType=CastType::Self, .isSpecial=true, .dangerlevel=5 });

    // =========================================================
    // Elise
    // =========================================================
    db.push_back({ .charName="Elise", .name="Rappel (E) - Untargetable", .spellName="EliseSpiderEInitial",
        .spellKey=SpellSlotId::E, .spellDelay=50,
        .evadeType=EvadeType::Untargetable, .castType=CastType::Self,
        .untargetable=true, .isSpecial=true, .checkSpellName=true, .dangerlevel=4 });

    // =========================================================
    // Ezreal
    // =========================================================
    db.push_back({ .charName="Ezreal", .name="Arcane Shift (E)", .spellName="EzrealArcaneShift",
        .spellKey=SpellSlotId::E, .range=475, .spellDelay=250,
        .evadeType=EvadeType::Blink, .castType=CastType::Position, .dangerlevel=2 });

    // =========================================================
    // Fiora
    // =========================================================
    db.push_back({ .charName="Fiora", .name="Riposte (W) - Wind Wall", .spellName="FioraW",
        .spellKey=SpellSlotId::W, .range=750, .spellDelay=100,
        .evadeType=EvadeType::WindWall, .castType=CastType::Position, .dangerlevel=3 });
    db.push_back({ .charName="Fiora", .name="Lunge (Q)", .spellName="FioraQ",
        .spellKey=SpellSlotId::Q, .range=340, .spellDelay=50, .speed=1100, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // =========================================================
    // Fizz
    // =========================================================
    db.push_back({ .charName="Fizz", .name="Playful / Trickster (E)", .spellName="FizzJump",
        .spellKey=SpellSlotId::E, .range=400, .spellDelay=50, .speed=1400, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .untargetable=true, .dangerlevel=3 });

    // =========================================================
    // Gnar
    // =========================================================
    db.push_back({ .charName="Gnar", .name="Hop (E)", .spellName="GnarE",
        .spellKey=SpellSlotId::E, .range=475, .spellDelay=50, .speed=900,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .checkSpellName=true, .dangerlevel=3 });

    // =========================================================
    // Gragas
    // =========================================================
    db.push_back({ .charName="Gragas", .name="Body Slam (E)", .spellName="GragasBodySlam",
        .spellKey=SpellSlotId::E, .range=600, .spellDelay=50, .speed=900,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // =========================================================
    // Graves
    // =========================================================
    db.push_back({ .charName="Graves", .name="Quickdraw (E)", .spellName="GravesMove",
        .spellKey=SpellSlotId::E, .range=425, .spellDelay=50, .speed=1250,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // =========================================================
    // Kassadin
    // =========================================================
    db.push_back({ .charName="Kassadin", .name="Riftwalk (R)", .spellName="RiftWalk",
        .spellKey=SpellSlotId::R, .range=500, .spellDelay=250,
        .evadeType=EvadeType::Blink, .castType=CastType::Position, .dangerlevel=1 });

    // =========================================================
    // Katarina
    // =========================================================
    db.push_back({ .charName="Katarina", .name="Shunpo (E)", .spellName="KatarinaE",
        .spellKey=SpellSlotId::E, .range=700, .spellDelay=50, .speed=99999,
        .spellTargets={SpellTargets::Targetables},
        .evadeType=EvadeType::Blink, .castType=CastType::Target, .dangerlevel=3 });

    // =========================================================
    // Kayle
    // =========================================================
    db.push_back({ .charName="Kayle", .name="Intervention (R) - Invuln", .spellName="JudicatorIntervention",
        .spellKey=SpellSlotId::R, .spellDelay=250,
        .spellTargets={SpellTargets::AllyChampions},
        .evadeType=EvadeType::Stasis, .castType=CastType::Target, .dangerlevel=4 });

    // =========================================================
    // Kennen
    // =========================================================
    db.push_back({ .charName="Kennen", .name="Lightning Rush (E)", .spellName="KennenLightningRush",
        .spellKey=SpellSlotId::E, .spellDelay=250, .speedArray={100, 100, 100, 100, 100},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=4 });

    // =========================================================
    // Kindred
    // =========================================================
    db.push_back({ .charName="Kindred", .name="Dance of Arrows (Q)", .spellName="KindredQ",
        .spellKey=SpellSlotId::Q, .range=300, .spellDelay=50, .speed=733, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=1 });

    // =========================================================
    // LeBlanc
    // =========================================================
    db.push_back({ .charName="Leblanc", .name="Distortion (W)", .spellName="LeblancSlide",
        .spellKey=SpellSlotId::W, .range=600, .spellDelay=50, .speed=1600,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });
    db.push_back({ .charName="Leblanc", .name="Distortion Mimic (R)", .spellName="LeblancSlideM",
        .spellKey=SpellSlotId::R, .range=600, .spellDelay=50, .speed=1600,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .checkSpellName=true, .dangerlevel=2 });

    // =========================================================
    // Lee Sin
    // =========================================================
    db.push_back({ .charName="LeeSin", .name="Safeguard (W) - Shield ally", .spellName="BlindMonkWOne",
        .spellKey=SpellSlotId::W, .range=700, .spellDelay=50, .speed=1400,
        .spellTargets={SpellTargets::AllyChampions, SpellTargets::AllyMinions},
        .evadeType=EvadeType::Shield, .castType=CastType::Target, .dangerlevel=3 });

    // =========================================================
    // Lucian
    // =========================================================
    db.push_back({ .charName="Lucian", .name="Relentless Pursuit (E)", .spellName="LucianE",
        .spellKey=SpellSlotId::E, .range=425, .spellDelay=50, .speed=1350,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=1 });

    // =========================================================
    // Master Yi
    // =========================================================
    db.push_back({ .charName="MasterYi", .name="Alpha Strike (Q)", .spellName="AlphaStrike",
        .spellKey=SpellSlotId::Q, .range=600, .spellDelay=100, .speed=99999,
        .spellTargets={SpellTargets::EnemyChampions, SpellTargets::EnemyMinions},
        .evadeType=EvadeType::Blink, .castType=CastType::Target, .untargetable=true, .dangerlevel=3 });

    // =========================================================
    // Morgana
    // =========================================================
    db.push_back({ .charName="Morgana", .name="Black Shield (E)", .spellName="BlackShield",
        .spellKey=SpellSlotId::E, .spellDelay=50,
        .spellTargets={SpellTargets::AllyChampions},
        .evadeType=EvadeType::SpellShield, .castType=CastType::Target, .dangerlevel=3 });

    // =========================================================
    // Nidalee
    // =========================================================
    db.push_back({ .charName="Nidalee", .name="Pounce (W-Spider)", .spellName="Pounce",
        .spellKey=SpellSlotId::W, .range=375, .spellDelay=150, .speed=1750,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .isSpecial=true, .dangerlevel=4 });

    // =========================================================
    // Nocturne
    // =========================================================
    db.push_back({ .charName="Nocturne", .name="Shroud of Darkness (W) - Spell Shield", .spellName="NocturneShroudofDarkness",
        .spellKey=SpellSlotId::W, .spellDelay=50,
        .evadeType=EvadeType::SpellShield, .castType=CastType::Self, .dangerlevel=3 });

    // =========================================================
    // Poppy
    // =========================================================
    db.push_back({ .charName="Poppy", .name="Hextech Faraday Zone (W) - Speed", .spellName="PoppyW",
        .spellKey=SpellSlotId::W, .spellDelay=250, .speedArray={27, 29, 31, 33, 35},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=3 });

    // =========================================================
    // Riven
    // =========================================================
    db.push_back({ .charName="Riven", .name="Broken Wings (Q)", .spellName="RivenTriCleave",
        .spellKey=SpellSlotId::Q, .range=260, .spellDelay=50, .speed=560, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .isSpecial=true, .dangerlevel=1 });
    db.push_back({ .charName="Riven", .name="Valor (E)", .spellName="RivenFeint",
        .spellKey=SpellSlotId::E, .range=325, .spellDelay=50, .speed=1200, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=1 });

    // =========================================================
    // Shaco
    // =========================================================
    db.push_back({ .charName="Shaco", .name="Deceive (Q)", .spellName="Deceive",
        .spellKey=SpellSlotId::Q, .range=400, .spellDelay=250,
        .evadeType=EvadeType::Blink, .castType=CastType::Position, .dangerlevel=3 });

    // =========================================================
    // Sivir
    // =========================================================
    db.push_back({ .charName="Sivir", .name="Spell Shield (E)", .spellName="SivirE",
        .spellKey=SpellSlotId::E, .spellDelay=50,
        .evadeType=EvadeType::SpellShield, .castType=CastType::Self, .dangerlevel=2 });

    // =========================================================
    // Sona
    // =========================================================
    db.push_back({ .charName="Sona", .name="Song of Celerity (E)", .spellName="SonaE",
        .spellKey=SpellSlotId::E, .spellDelay=250, .speedArray={13, 14, 15, 16, 25},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=3 });

    // =========================================================
    // Talon
    // =========================================================
    db.push_back({ .charName="Talon", .name="Assassin's Path (E)", .spellName="TalonE",
        .spellKey=SpellSlotId::E, .range=750, .spellDelay=50, .speed=1400,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // =========================================================
    // Tristana
    // =========================================================
    db.push_back({ .charName="Tristana", .name="Rocket Jump (W)", .spellName="RocketJump",
        .spellKey=SpellSlotId::W, .range=900, .spellDelay=500, .speed=1100,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // =========================================================
    // Tryndamere
    // =========================================================
    db.push_back({ .charName="Tryndamere", .name="Spinning Slash (E)", .spellName="Slash",
        .spellKey=SpellSlotId::E, .range=660, .spellDelay=50, .speed=900,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // =========================================================
    // Vayne
    // =========================================================
    db.push_back({ .charName="Vayne", .name="Tumble (Q)", .spellName="VayneTumble",
        .spellKey=SpellSlotId::Q, .range=300, .spellDelay=50, .speed=900, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=1 });

    // =========================================================
    // Yasuo
    // =========================================================
    db.push_back({ .charName="Yasuo", .name="Sweeping Blade (E)", .spellName="YasuoDashWrapper",
        .spellKey=SpellSlotId::E, .range=475, .spellDelay=50, .speed=1000, .fixedRange=true,
        .spellTargets={SpellTargets::EnemyChampions, SpellTargets::EnemyMinions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .dangerlevel=2 });
    db.push_back({ .charName="Yasuo", .name="Wind Wall (W)", .spellName="YasuoWMovingWall",
        .spellKey=SpellSlotId::W, .range=400, .spellDelay=250,
        .evadeType=EvadeType::WindWall, .castType=CastType::Position, .dangerlevel=3 });

    // =========================================================
    // Zilean
    // =========================================================
    db.push_back({ .charName="Zilean", .name="Time Warp (E)", .spellName="ZileanE",
        .spellKey=SpellSlotId::E, .spellDelay=250, .speedArray={40, 55, 70, 85, 99},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=3 });

    detail::FinalizePatch265(db);
    return db;
}

// Global database instance
inline std::vector<EvadeSpellData>& GetEvadeSpellDatabase() {
    static std::vector<EvadeSpellData> db = BuildEvadeSpellDatabase();
    return db;
}

// Get evade spells for one champion and shared AllChampions entries.
inline std::vector<const EvadeSpellData*> GetEvadeSpellsForChampion(const std::string& champName) {
    std::vector<const EvadeSpellData*> result;
    for (auto& s : GetEvadeSpellDatabase()) {
        if (s.charName == champName || s.charName == "AllChampions")
            result.push_back(&s);
    }
    return result;
}

} // namespace EzEvade


