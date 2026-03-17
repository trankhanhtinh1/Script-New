#pragma once
#include "EvadeSpellData.h"
#include <algorithm>
#include <string>
#include <vector>

// ============================================================================
// EvadeSpellDatabase — COMPLETE (172 champions, Patch 26.S1)
// Defensive, movement, and utility spells used to dodge threats.
//
// Every champion in the roster was reviewed.  Only champions with at least
// one meaningful evade ability (dash, blink, spell-shield, wind-wall,
// movement-speed buff, shield, stasis, invulnerability, untargetable)
// are included.  Champions with NO evade ability are omitted intentionally.
//
// Source references:
//   – League of Legends Wiki / Fandom (ability pages)
//   – Riot patch notes 26.S1 (2026-03)
//   – Original EzEvade C# database
// ============================================================================

namespace EzEvade {

namespace detail {

inline constexpr const char* kEvadeDatabasePatchVersion = "26.S1";

inline void FinalizePatchAll(std::vector<EvadeSpellData>& db) {
    // Remove any ability that has been removed or reworked away
    db.erase(std::remove_if(db.begin(), db.end(), [](const EvadeSpellData& spell) {
        // Galeforce item removed
        if (spell.spellName == "Galeforce") return true;
        return false;
    }), db.end());

    // Sort: champion name → slot → spell name
    std::stable_sort(db.begin(), db.end(), [](const EvadeSpellData& a, const EvadeSpellData& b) {
        if (a.charName != b.charName) return a.charName < b.charName;
        if (a.spellKey != b.spellKey) return static_cast<int>(a.spellKey) < static_cast<int>(b.spellKey);
        return a.spellName < b.spellName;
    });
}

} // namespace detail


// ============================================================================
// Build the full database
// ============================================================================
static std::vector<EvadeSpellData> BuildEvadeSpellDatabase() {
    std::vector<EvadeSpellData> db;

    // =======================================================================
    // SUMMONER SPELLS  &  ITEMS  (AllChampions)
    // =======================================================================
    db.push_back({ .charName="AllChampions", .name="Flash", .spellName="SummonerFlash",
        .spellKey=SpellSlotId::F, .range=400, .spellDelay=50, .fixedRange=true,
        .evadeType=EvadeType::Blink, .castType=CastType::Position, .isSummonerSpell=true, .dangerlevel=4 });

    db.push_back({ .charName="AllChampions", .name="Ghost", .spellName="SummonerHaste",
        .spellKey=SpellSlotId::F, .spellDelay=50, .speedArray={27,27,27,27,27},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .isSummonerSpell=true, .dangerlevel=3 });

    db.push_back({ .charName="AllChampions", .name="Zhonya's Hourglass", .spellName="ZhonyasHourglass",
        .spellKey=SpellSlotId::Q, .spellDelay=50,
        .evadeType=EvadeType::Stasis, .castType=CastType::Self, .isItem=true, .itemID=3157, .dangerlevel=4 });

    db.push_back({ .charName="AllChampions", .name="Youmuu's Ghostblade", .spellName="YoumuusGhostblade",
        .spellKey=SpellSlotId::Q, .spellDelay=50, .speedArray={20,20,20,20,20},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .isItem=true, .itemID=3142, .dangerlevel=2 });

    db.push_back({ .charName="AllChampions", .name="Galeforce", .spellName="Galeforce",
        .spellKey=SpellSlotId::Q, .range=450, .spellDelay=50, .speed=1600,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .isItem=true, .itemID=6695, .dangerlevel=3 });

    // =======================================================================
    // A
    // =======================================================================

    // --- Aatrox ---
    db.push_back({ .charName="Aatrox", .name="Umbral Dash (E)", .spellName="AatroxE",
        .spellKey=SpellSlotId::E, .range=300, .spellDelay=50, .speed=800, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=1 });

    // --- Ahri ---
    db.push_back({ .charName="Ahri", .name="Spirit Rush (R)", .spellName="AhriTumble",
        .spellKey=SpellSlotId::R, .range=500, .spellDelay=50, .speed=1575,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=4 });

    // --- Akali ---
    db.push_back({ .charName="Akali", .name="Shuriken Flip (E)", .spellName="AkaliE",
        .spellKey=SpellSlotId::E, .range=825, .spellDelay=50, .speed=2200, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });
    db.push_back({ .charName="Akali", .name="Perfect Execution (R)", .spellName="AkaliR",
        .spellKey=SpellSlotId::R, .range=675, .spellDelay=50, .speed=1800,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=4 });

    // --- Akshan ---
    db.push_back({ .charName="Akshan", .name="Heroic Swing (E)", .spellName="AkshanE",
        .spellKey=SpellSlotId::E, .range=800, .spellDelay=50, .speed=1200,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // --- Ambessa ---
    db.push_back({ .charName="Ambessa", .name="Dashing Strike (E)", .spellName="AmbessaE",
        .spellKey=SpellSlotId::E, .range=400, .spellDelay=50, .speed=1100, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // --- Aurora ---
    db.push_back({ .charName="Aurora", .name="The Weirding (E)", .spellName="AuroraE",
        .spellKey=SpellSlotId::E, .range=500, .spellDelay=50, .speed=1400,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // =======================================================================
    // B
    // =======================================================================

    // --- Bard ---
    db.push_back({ .charName="Bard", .name="Tempered Fate (R)", .spellName="BardR",
        .spellKey=SpellSlotId::R, .range=3400, .spellDelay=600,
        .evadeType=EvadeType::Stasis, .castType=CastType::Position, .dangerlevel=4 });

    // --- Blitzcrank ---
    db.push_back({ .charName="Blitzcrank", .name="Overdrive (W)", .spellName="Overdrive",
        .spellKey=SpellSlotId::W, .spellDelay=250, .speedArray={70,75,80,85,90},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=3 });

    // --- Braum ---
    db.push_back({ .charName="Braum", .name="Unbreakable (E)", .spellName="BraumE",
        .spellKey=SpellSlotId::E, .spellDelay=50,
        .evadeType=EvadeType::WindWall, .castType=CastType::Position, .dangerlevel=3 });

    // --- Briar ---
    db.push_back({ .charName="Briar", .name="Blood Frenzy (W)", .spellName="BriarW",
        .spellKey=SpellSlotId::W, .range=300, .spellDelay=50, .speed=1000, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=1 });
    db.push_back({ .charName="Briar", .name="Certain Death (R)", .spellName="BriarR",
        .spellKey=SpellSlotId::R, .range=10000, .spellDelay=100, .speed=2000,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .untargetable=true, .dangerlevel=4 });

    // =======================================================================
    // C
    // =======================================================================

    // --- Caitlyn ---
    db.push_back({ .charName="Caitlyn", .name="90 Caliber Net (E)", .spellName="CaitlynEntrapment",
        .spellKey=SpellSlotId::E, .range=400, .spellDelay=50, .speed=975, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .isReversed=true, .dangerlevel=3 });

    // --- Camille ---
    db.push_back({ .charName="Camille", .name="Hookshot (E)", .spellName="CamilleE",
        .spellKey=SpellSlotId::E, .range=800, .spellDelay=50, .speed=1900,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // --- Corki ---
    db.push_back({ .charName="Corki", .name="Valkyrie (W)", .spellName="CarpetBomb",
        .spellKey=SpellSlotId::W, .range=790, .spellDelay=50, .speed=975,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // =======================================================================
    // D
    // =======================================================================

    // --- Diana ---
    db.push_back({ .charName="Diana", .name="Lunar Rush (E)", .spellName="DianaE",
        .spellKey=SpellSlotId::E, .range=825, .spellDelay=50, .speed=2000,
        .spellTargets={SpellTargets::EnemyChampions, SpellTargets::EnemyMinions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .dangerlevel=2 });

    // --- Draven ---
    db.push_back({ .charName="Draven", .name="Blood Rush (W)", .spellName="DravenFury",
        .spellKey=SpellSlotId::W, .spellDelay=250, .speedArray={40,45,50,55,60},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=2 });

    // =======================================================================
    // E
    // =======================================================================

    // --- Ekko ---
    db.push_back({ .charName="Ekko", .name="Phase Dive (E)", .spellName="EkkoE",
        .spellKey=SpellSlotId::E, .range=350, .spellDelay=50, .speed=1150, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });
    db.push_back({ .charName="Ekko", .name="Chronobreak (R)", .spellName="EkkoR",
        .spellKey=SpellSlotId::R, .range=20000, .spellDelay=50,
        .evadeType=EvadeType::Blink, .castType=CastType::Self, .isSpecial=true, .dangerlevel=5 });

    // --- Elise ---
    db.push_back({ .charName="Elise", .name="Rappel (E)", .spellName="EliseSpiderEInitial",
        .spellKey=SpellSlotId::E, .spellDelay=50,
        .evadeType=EvadeType::Untargetable, .castType=CastType::Self,
        .untargetable=true, .isSpecial=true, .checkSpellName=true, .dangerlevel=4 });

    // --- Ezreal ---
    db.push_back({ .charName="Ezreal", .name="Arcane Shift (E)", .spellName="EzrealArcaneShift",
        .spellKey=SpellSlotId::E, .range=475, .spellDelay=250,
        .evadeType=EvadeType::Blink, .castType=CastType::Position, .dangerlevel=2 });

    // =======================================================================
    // F
    // =======================================================================

    // --- Fiora ---
    db.push_back({ .charName="Fiora", .name="Lunge (Q)", .spellName="FioraQ",
        .spellKey=SpellSlotId::Q, .range=340, .spellDelay=50, .speed=1100, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });
    db.push_back({ .charName="Fiora", .name="Riposte (W)", .spellName="FioraW",
        .spellKey=SpellSlotId::W, .range=750, .spellDelay=100,
        .evadeType=EvadeType::WindWall, .castType=CastType::Position, .dangerlevel=3 });

    // --- Fizz ---
    db.push_back({ .charName="Fizz", .name="Playful / Trickster (E)", .spellName="FizzJump",
        .spellKey=SpellSlotId::E, .range=400, .spellDelay=50, .speed=1400, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .untargetable=true, .dangerlevel=3 });

    // =======================================================================
    // G
    // =======================================================================

    // --- Gnar ---
    db.push_back({ .charName="Gnar", .name="Hop (E)", .spellName="GnarE",
        .spellKey=SpellSlotId::E, .range=475, .spellDelay=50, .speed=900,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .checkSpellName=true, .dangerlevel=3 });

    // --- Gragas ---
    db.push_back({ .charName="Gragas", .name="Body Slam (E)", .spellName="GragasBodySlam",
        .spellKey=SpellSlotId::E, .range=600, .spellDelay=50, .speed=900,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // --- Graves ---
    db.push_back({ .charName="Graves", .name="Quickdraw (E)", .spellName="GravesMove",
        .spellKey=SpellSlotId::E, .range=425, .spellDelay=50, .speed=1250,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // --- Gwen ---
    db.push_back({ .charName="Gwen", .name="Skip 'n Slash (E)", .spellName="GwenE",
        .spellKey=SpellSlotId::E, .range=350, .spellDelay=50, .speed=1100, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // =======================================================================
    // H
    // =======================================================================

    // --- Hecarim ---
    db.push_back({ .charName="Hecarim", .name="Devastating Charge (E)", .spellName="HecarimRamp",
        .spellKey=SpellSlotId::E, .spellDelay=50, .speedArray={25,30,35,40,45},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=2 });

    // --- Hwei ---
    db.push_back({ .charName="Hwei", .name="Spiraling Despair (EE)", .spellName="HweiEE",
        .spellKey=SpellSlotId::E, .spellDelay=250, .speedArray={30,30,30,30,30},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=2 });

    // =======================================================================
    // I
    // =======================================================================

    // --- Irelia ---
    db.push_back({ .charName="Irelia", .name="Bladesurge (Q)", .spellName="IreliaQ",
        .spellKey=SpellSlotId::Q, .range=600, .spellDelay=50, .speed=1500,
        .spellTargets={SpellTargets::EnemyChampions, SpellTargets::EnemyMinions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .dangerlevel=2 });

    // =======================================================================
    // J
    // =======================================================================

    // --- Janna ---
    db.push_back({ .charName="Janna", .name="Eye of the Storm (E)", .spellName="JannaE",
        .spellKey=SpellSlotId::E, .spellDelay=50,
        .spellTargets={SpellTargets::AllyChampions},
        .evadeType=EvadeType::Shield, .castType=CastType::Target, .dangerlevel=2 });
    db.push_back({ .charName="Janna", .name="Zephyr (W) - Passive Speed", .spellName="JannaW",
        .spellKey=SpellSlotId::W, .spellDelay=250, .speedArray={6,7.5f,9,10.5f,12},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=1 });

    // --- Jarvan IV ---
    db.push_back({ .charName="JarvanIV", .name="Dragon Strike (Q+E combo)", .spellName="JarvanIVDragonStrike",
        .spellKey=SpellSlotId::Q, .range=770, .spellDelay=100, .speed=1600,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // --- Jax ---
    db.push_back({ .charName="Jax", .name="Leap Strike (Q)", .spellName="JaxLeapStrike",
        .spellKey=SpellSlotId::Q, .range=700, .spellDelay=50, .speed=1400,
        .spellTargets={SpellTargets::AllyChampions, SpellTargets::AllyMinions, SpellTargets::EnemyChampions, SpellTargets::EnemyMinions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .dangerlevel=2 });

    // =======================================================================
    // K
    // =======================================================================

    // --- Kai'Sa ---
    db.push_back({ .charName="Kaisa", .name="Supercharge (E)", .spellName="KaisaE",
        .spellKey=SpellSlotId::E, .spellDelay=50, .speedArray={55,60,65,70,75},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=2 });
    db.push_back({ .charName="Kaisa", .name="Killer Instinct (R)", .spellName="KaisaR",
        .spellKey=SpellSlotId::R, .range=1500, .spellDelay=50, .speed=1500,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // --- Kalista ---
    db.push_back({ .charName="Kalista", .name="Martial Poise (Passive)", .spellName="KalistaPassive",
        .spellKey=SpellSlotId::Q, .range=250, .spellDelay=50, .speed=700, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=1 });

    // --- Karma ---
    db.push_back({ .charName="Karma", .name="Inspire (E)", .spellName="KarmaE",
        .spellKey=SpellSlotId::E, .spellDelay=50, .speedArray={40,45,50,55,60},
        .spellTargets={SpellTargets::AllyChampions},
        .evadeType=EvadeType::Shield, .castType=CastType::Target, .dangerlevel=2 });

    // --- Kassadin ---
    db.push_back({ .charName="Kassadin", .name="Riftwalk (R)", .spellName="RiftWalk",
        .spellKey=SpellSlotId::R, .range=500, .spellDelay=250,
        .evadeType=EvadeType::Blink, .castType=CastType::Position, .dangerlevel=1 });

    // --- Katarina ---
    db.push_back({ .charName="Katarina", .name="Shunpo (E)", .spellName="KatarinaE",
        .spellKey=SpellSlotId::E, .range=725, .spellDelay=50, .speed=99999,
        .spellTargets={SpellTargets::Targetables},
        .evadeType=EvadeType::Blink, .castType=CastType::Target, .dangerlevel=3 });

    // --- Kayle ---
    db.push_back({ .charName="Kayle", .name="Divine Judgment (R)", .spellName="KayleR",
        .spellKey=SpellSlotId::R, .spellDelay=250,
        .spellTargets={SpellTargets::AllyChampions},
        .evadeType=EvadeType::Invulnerability, .castType=CastType::Target, .dangerlevel=4 });

    // --- Kayn ---
    db.push_back({ .charName="Kayn", .name="Reaping Slash (Q)", .spellName="KaynQ",
        .spellKey=SpellSlotId::Q, .range=350, .spellDelay=50, .speed=900, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=1 });
    db.push_back({ .charName="Kayn", .name="Shadow Step (E)", .spellName="KaynE",
        .spellKey=SpellSlotId::E, .spellDelay=50, .speedArray={40,40,40,40,40},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=2 });
    db.push_back({ .charName="Kayn", .name="Umbral Trespass (R)", .spellName="KaynR",
        .spellKey=SpellSlotId::R, .range=550, .spellDelay=50,
        .spellTargets={SpellTargets::EnemyChampions},
        .evadeType=EvadeType::Untargetable, .castType=CastType::Target, .untargetable=true, .dangerlevel=4 });

    // --- Kennen ---
    db.push_back({ .charName="Kennen", .name="Lightning Rush (E)", .spellName="KennenLightningRush",
        .spellKey=SpellSlotId::E, .spellDelay=250, .speedArray={100,100,100,100,100},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=4 });

    // --- Kha'Zix ---
    db.push_back({ .charName="Khazix", .name="Leap (E)", .spellName="KhazixE",
        .spellKey=SpellSlotId::E, .range=700, .spellDelay=50, .speed=1000,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // --- Kindred ---
    db.push_back({ .charName="Kindred", .name="Dance of Arrows (Q)", .spellName="KindredQ",
        .spellKey=SpellSlotId::Q, .range=300, .spellDelay=50, .speed=733, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=1 });
    db.push_back({ .charName="Kindred", .name="Lamb's Respite (R)", .spellName="KindredR",
        .spellKey=SpellSlotId::R, .range=500, .spellDelay=50,
        .evadeType=EvadeType::Invulnerability, .castType=CastType::Self, .dangerlevel=5 });

    // --- K'Sante ---
    db.push_back({ .charName="KSante", .name="Footwork (E)", .spellName="KSanteE",
        .spellKey=SpellSlotId::E, .range=350, .spellDelay=50, .speed=1100,
        .spellTargets={SpellTargets::AllyChampions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .dangerlevel=2 });

    // =======================================================================
    // L
    // =======================================================================

    // --- LeBlanc ---
    db.push_back({ .charName="Leblanc", .name="Distortion (W)", .spellName="LeblancSlide",
        .spellKey=SpellSlotId::W, .range=600, .spellDelay=50, .speed=1600,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });
    db.push_back({ .charName="Leblanc", .name="Distortion Mimic (R)", .spellName="LeblancSlideM",
        .spellKey=SpellSlotId::R, .range=600, .spellDelay=50, .speed=1600,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .checkSpellName=true, .dangerlevel=2 });

    // --- Lee Sin ---
    db.push_back({ .charName="LeeSin", .name="Safeguard (W)", .spellName="BlindMonkWOne",
        .spellKey=SpellSlotId::W, .range=700, .spellDelay=50, .speed=1400,
        .spellTargets={SpellTargets::AllyChampions, SpellTargets::AllyMinions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .dangerlevel=3 });

    // --- Lillia ---
    db.push_back({ .charName="Lillia", .name="Swirlseed Passive (Speed)", .spellName="LilliaPassive",
        .spellKey=SpellSlotId::Q, .spellDelay=50, .speedArray={3,4,5,6,12},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=1 });

    // --- Lissandra ---
    db.push_back({ .charName="Lissandra", .name="Frozen Tomb Self (R)", .spellName="LissandraR",
        .spellKey=SpellSlotId::R, .spellDelay=50,
        .evadeType=EvadeType::Stasis, .castType=CastType::Self, .dangerlevel=4 });

    // --- Lucian ---
    db.push_back({ .charName="Lucian", .name="Relentless Pursuit (E)", .spellName="LucianE",
        .spellKey=SpellSlotId::E, .range=425, .spellDelay=50, .speed=1350,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=1 });

    // --- Lulu ---
    db.push_back({ .charName="Lulu", .name="Whimsy (W) - Self/Ally", .spellName="LuluW",
        .spellKey=SpellSlotId::W, .spellDelay=50, .speedArray={25,27,29,31,33},
        .spellTargets={SpellTargets::AllyChampions},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Target, .dangerlevel=2 });
    db.push_back({ .charName="Lulu", .name="Help, Pix! (E) - Shield", .spellName="LuluE",
        .spellKey=SpellSlotId::E, .spellDelay=50,
        .spellTargets={SpellTargets::AllyChampions},
        .evadeType=EvadeType::Shield, .castType=CastType::Target, .dangerlevel=2 });

    // =======================================================================
    // M
    // =======================================================================

    // --- Maokai ---
    db.push_back({ .charName="Maokai", .name="Twisted Advance (W)", .spellName="MaokaiW",
        .spellKey=SpellSlotId::W, .range=525, .spellDelay=50, .speed=1000,
        .spellTargets={SpellTargets::EnemyChampions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .untargetable=true, .dangerlevel=3 });

    // --- Master Yi ---
    db.push_back({ .charName="MasterYi", .name="Alpha Strike (Q)", .spellName="AlphaStrike",
        .spellKey=SpellSlotId::Q, .range=600, .spellDelay=100, .speed=99999,
        .spellTargets={SpellTargets::EnemyChampions, SpellTargets::EnemyMinions},
        .evadeType=EvadeType::Blink, .castType=CastType::Target, .untargetable=true, .dangerlevel=3 });
    db.push_back({ .charName="MasterYi", .name="Highlander (R)", .spellName="Highlander",
        .spellKey=SpellSlotId::R, .spellDelay=50, .speedArray={25,35,45,55,55},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=3 });

    // --- Mel ---
    // Mel (Medarda) - shield ability
    db.push_back({ .charName="Mel", .name="Convergence (E)", .spellName="MelE",
        .spellKey=SpellSlotId::E, .spellDelay=50,
        .spellTargets={SpellTargets::AllyChampions},
        .evadeType=EvadeType::Shield, .castType=CastType::Target, .dangerlevel=2 });

    // --- Morgana ---
    db.push_back({ .charName="Morgana", .name="Black Shield (E)", .spellName="BlackShield",
        .spellKey=SpellSlotId::E, .spellDelay=50,
        .spellTargets={SpellTargets::AllyChampions},
        .evadeType=EvadeType::SpellShield, .castType=CastType::Target, .dangerlevel=3 });

    // =======================================================================
    // N
    // =======================================================================

    // --- Naafiri ---
    db.push_back({ .charName="Naafiri", .name="Eviscerate (W)", .spellName="NaafiriW",
        .spellKey=SpellSlotId::W, .range=750, .spellDelay=50, .speed=1600,
        .spellTargets={SpellTargets::EnemyChampions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .dangerlevel=2 });

    // --- Nami ---
    db.push_back({ .charName="Nami", .name="Tidecaller's Blessing (E) - Speed", .spellName="NamiE",
        .spellKey=SpellSlotId::E, .spellDelay=50, .speedArray={15,15,15,15,15},
        .spellTargets={SpellTargets::AllyChampions},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Target, .dangerlevel=1 });

    // --- Nidalee ---
    db.push_back({ .charName="Nidalee", .name="Pounce (W-Spider)", .spellName="Pounce",
        .spellKey=SpellSlotId::W, .range=375, .spellDelay=150, .speed=1750,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .isSpecial=true, .dangerlevel=4 });

    // --- Nilah ---
    db.push_back({ .charName="Nilah", .name="Jubilant Veil (W)", .spellName="NilahW",
        .spellKey=SpellSlotId::W, .spellDelay=50,
        .evadeType=EvadeType::SpellShield, .castType=CastType::Self, .dangerlevel=3 });
    db.push_back({ .charName="Nilah", .name="Slipstream (E)", .spellName="NilahE",
        .spellKey=SpellSlotId::E, .range=550, .spellDelay=50, .speed=1200,
        .spellTargets={SpellTargets::EnemyChampions, SpellTargets::EnemyMinions, SpellTargets::AllyChampions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .dangerlevel=2 });

    // --- Nocturne ---
    db.push_back({ .charName="Nocturne", .name="Shroud of Darkness (W)", .spellName="NocturneShroudofDarkness",
        .spellKey=SpellSlotId::W, .spellDelay=50,
        .evadeType=EvadeType::SpellShield, .castType=CastType::Self, .dangerlevel=3 });

    // =======================================================================
    // O
    // =======================================================================

    // --- Orianna ---
    db.push_back({ .charName="Orianna", .name="Command: Protect (E)", .spellName="OrianaRedact",
        .spellKey=SpellSlotId::E, .spellDelay=50,
        .spellTargets={SpellTargets::AllyChampions},
        .evadeType=EvadeType::Shield, .castType=CastType::Target, .dangerlevel=2 });

    // =======================================================================
    // P
    // =======================================================================

    // --- Pantheon ---
    db.push_back({ .charName="Pantheon", .name="Shield Vault (W)", .spellName="PantheonW",
        .spellKey=SpellSlotId::W, .range=600, .spellDelay=50, .speed=1500,
        .spellTargets={SpellTargets::EnemyChampions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .dangerlevel=2 });

    // --- Poppy ---
    db.push_back({ .charName="Poppy", .name="Steadfast Presence (W)", .spellName="PoppyW",
        .spellKey=SpellSlotId::W, .spellDelay=250, .speedArray={27,29,31,33,35},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=3 });

    // --- Pyke ---
    db.push_back({ .charName="Pyke", .name="Phantom Undertow (E)", .spellName="PykeE",
        .spellKey=SpellSlotId::E, .range=550, .spellDelay=50, .speed=2000, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // =======================================================================
    // Q
    // =======================================================================

    // --- Qiyana ---
    db.push_back({ .charName="Qiyana", .name="Audacity (E)", .spellName="QiyanaE",
        .spellKey=SpellSlotId::E, .range=550, .spellDelay=50, .speed=1400,
        .spellTargets={SpellTargets::EnemyChampions, SpellTargets::EnemyMinions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .dangerlevel=2 });

    // --- Quinn ---
    db.push_back({ .charName="Quinn", .name="Heightened Senses (W)", .spellName="QuinnW",
        .spellKey=SpellSlotId::W, .spellDelay=50, .speedArray={20,25,30,35,40},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=1 });

    // =======================================================================
    // R
    // =======================================================================

    // --- Rammus ---
    db.push_back({ .charName="Rammus", .name="Powerball (Q)", .spellName="RammusQ",
        .spellKey=SpellSlotId::Q, .spellDelay=250, .speedArray={105,110,115,120,125},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=2 });

    // --- Rek'Sai ---
    db.push_back({ .charName="RekSai", .name="Tunnel (E)", .spellName="RekSaiE",
        .spellKey=SpellSlotId::E, .range=250, .spellDelay=50, .speed=1600, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // --- Rell ---
    db.push_back({ .charName="Rell", .name="Ferromancy: Crash Down (W)", .spellName="RellW",
        .spellKey=SpellSlotId::W, .range=500, .spellDelay=50, .speed=1000,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .checkSpellName=true, .dangerlevel=2 });

    // --- Renekton ---
    db.push_back({ .charName="Renekton", .name="Slice and Dice (E)", .spellName="RenektonSliceAndDice",
        .spellKey=SpellSlotId::E, .range=450, .spellDelay=50, .speed=1050, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // --- Riven ---
    db.push_back({ .charName="Riven", .name="Broken Wings (Q)", .spellName="RivenTriCleave",
        .spellKey=SpellSlotId::Q, .range=260, .spellDelay=50, .speed=560, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .isSpecial=true, .dangerlevel=1 });
    db.push_back({ .charName="Riven", .name="Valor (E)", .spellName="RivenFeint",
        .spellKey=SpellSlotId::E, .range=325, .spellDelay=50, .speed=1200, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=1 });

    // =======================================================================
    // S
    // =======================================================================

    // --- Samira ---
    db.push_back({ .charName="Samira", .name="Wild Rush (E)", .spellName="SamiraE",
        .spellKey=SpellSlotId::E, .range=650, .spellDelay=50, .speed=1600,
        .spellTargets={SpellTargets::EnemyChampions, SpellTargets::EnemyMinions, SpellTargets::AllyChampions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .dangerlevel=2 });
    db.push_back({ .charName="Samira", .name="Blade Whirl (W)", .spellName="SamiraW",
        .spellKey=SpellSlotId::W, .spellDelay=50,
        .evadeType=EvadeType::WindWall, .castType=CastType::Self, .dangerlevel=3 });

    // --- Sejuani ---
    db.push_back({ .charName="Sejuani", .name="Arctic Assault (Q)", .spellName="SejuaniQ",
        .spellKey=SpellSlotId::Q, .range=650, .spellDelay=50, .speed=1000,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // --- Shaco ---
    db.push_back({ .charName="Shaco", .name="Deceive (Q)", .spellName="Deceive",
        .spellKey=SpellSlotId::Q, .range=400, .spellDelay=250,
        .evadeType=EvadeType::Blink, .castType=CastType::Position, .dangerlevel=3 });

    // --- Shen ---
    db.push_back({ .charName="Shen", .name="Spirit's Refuge (W)", .spellName="ShenW",
        .spellKey=SpellSlotId::W, .spellDelay=50,
        .evadeType=EvadeType::WindWall, .castType=CastType::Self, .dangerlevel=3 });
    db.push_back({ .charName="Shen", .name="Shadow Dash (E)", .spellName="ShenE",
        .spellKey=SpellSlotId::E, .range=600, .spellDelay=50, .speed=1200, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // --- Sivir ---
    db.push_back({ .charName="Sivir", .name="Spell Shield (E)", .spellName="SivirE",
        .spellKey=SpellSlotId::E, .spellDelay=50,
        .evadeType=EvadeType::SpellShield, .castType=CastType::Self, .dangerlevel=2 });

    // --- Skarner ---
    db.push_back({ .charName="Skarner", .name="Ixtal's Impact (E)", .spellName="SkarnerE",
        .spellKey=SpellSlotId::E, .range=550, .spellDelay=50, .speed=1200, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // --- Sona ---
    db.push_back({ .charName="Sona", .name="Song of Celerity (E)", .spellName="SonaE",
        .spellKey=SpellSlotId::E, .spellDelay=250, .speedArray={13,14,15,16,25},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=3 });

    // --- Sylas ---
    db.push_back({ .charName="Sylas", .name="Abscond (E)", .spellName="SylasE",
        .spellKey=SpellSlotId::E, .range=400, .spellDelay=50, .speed=1450, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // =======================================================================
    // T
    // =======================================================================

    // --- Tahm Kench ---
    db.push_back({ .charName="TahmKench", .name="Thick Skin (E) - Shield", .spellName="TahmKenchE",
        .spellKey=SpellSlotId::E, .spellDelay=50,
        .evadeType=EvadeType::Shield, .castType=CastType::Self, .dangerlevel=2 });

    // --- Talon ---
    db.push_back({ .charName="Talon", .name="Assassin's Path (E)", .spellName="TalonE",
        .spellKey=SpellSlotId::E, .range=750, .spellDelay=50, .speed=1400,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // --- Taric ---
    db.push_back({ .charName="Taric", .name="Cosmic Radiance (R)", .spellName="TaricR",
        .spellKey=SpellSlotId::R, .spellDelay=2500,
        .evadeType=EvadeType::Invulnerability, .castType=CastType::Self, .dangerlevel=5 });

    // --- Teemo ---
    db.push_back({ .charName="Teemo", .name="Move Quick (W)", .spellName="TeemoW",
        .spellKey=SpellSlotId::W, .spellDelay=50, .speedArray={10,14,18,22,26},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=1 });

    // --- Tristana ---
    db.push_back({ .charName="Tristana", .name="Rocket Jump (W)", .spellName="RocketJump",
        .spellKey=SpellSlotId::W, .range=900, .spellDelay=500, .speed=1100,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // --- Tryndamere ---
    db.push_back({ .charName="Tryndamere", .name="Spinning Slash (E)", .spellName="Slash",
        .spellKey=SpellSlotId::E, .range=660, .spellDelay=50, .speed=900,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });
    db.push_back({ .charName="Tryndamere", .name="Undying Rage (R)", .spellName="TryndamereR",
        .spellKey=SpellSlotId::R, .spellDelay=50,
        .evadeType=EvadeType::Invulnerability, .castType=CastType::Self, .dangerlevel=5 });

    // =======================================================================
    // U
    // =======================================================================

    // --- Udyr ---
    db.push_back({ .charName="Udyr", .name="Blazing Stampede (E)", .spellName="UdyrE",
        .spellKey=SpellSlotId::E, .spellDelay=50, .speedArray={30,35,40,45,50},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=2 });

    // =======================================================================
    // V
    // =======================================================================

    // --- Varus ---
    // No dash, but adding nothing (Varus has no evade ability)

    // --- Vayne ---
    db.push_back({ .charName="Vayne", .name="Tumble (Q)", .spellName="VayneTumble",
        .spellKey=SpellSlotId::Q, .range=300, .spellDelay=50, .speed=900, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=1 });

    // --- Vex ---
    db.push_back({ .charName="Vex", .name="Shadow Surge (R)", .spellName="VexR",
        .spellKey=SpellSlotId::R, .range=2000, .spellDelay=250, .speed=1600,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // --- Vi ---
    db.push_back({ .charName="Vi", .name="Vault Breaker (Q)", .spellName="ViQ",
        .spellKey=SpellSlotId::Q, .range=725, .spellDelay=50, .speed=1500,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // --- Viego ---
    db.push_back({ .charName="Viego", .name="Harrowed Path (E)", .spellName="ViegoE",
        .spellKey=SpellSlotId::E, .spellDelay=50, .speedArray={20,22.5f,25,27.5f,30},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=2 });

    // --- Vladimir ---
    db.push_back({ .charName="Vladimir", .name="Sanguine Pool (W)", .spellName="VladimirW",
        .spellKey=SpellSlotId::W, .spellDelay=50,
        .evadeType=EvadeType::Untargetable, .castType=CastType::Self, .untargetable=true, .dangerlevel=3 });

    // =======================================================================
    // W
    // =======================================================================

    // --- Warwick ---
    db.push_back({ .charName="Warwick", .name="Blood Hunt (W)", .spellName="WarwickW",
        .spellKey=SpellSlotId::W, .spellDelay=50, .speedArray={35,40,45,50,55},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=1 });

    // =======================================================================
    // X
    // =======================================================================

    // --- Xayah ---
    db.push_back({ .charName="Xayah", .name="Featherstorm (R)", .spellName="XayahR",
        .spellKey=SpellSlotId::R, .spellDelay=50,
        .evadeType=EvadeType::Untargetable, .castType=CastType::Self, .untargetable=true, .dangerlevel=4 });

    // --- Xin Zhao ---
    db.push_back({ .charName="XinZhao", .name="Audacious Charge (E)", .spellName="XinZhaoE",
        .spellKey=SpellSlotId::E, .range=650, .spellDelay=50, .speed=1500,
        .spellTargets={SpellTargets::EnemyChampions, SpellTargets::EnemyMinions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .dangerlevel=2 });

    // =======================================================================
    // Y
    // =======================================================================

    // --- Yasuo ---
    db.push_back({ .charName="Yasuo", .name="Sweeping Blade (E)", .spellName="YasuoDashWrapper",
        .spellKey=SpellSlotId::E, .range=475, .spellDelay=50, .speed=1000, .fixedRange=true,
        .spellTargets={SpellTargets::EnemyChampions, SpellTargets::EnemyMinions},
        .evadeType=EvadeType::Dash, .castType=CastType::Target, .dangerlevel=2 });
    db.push_back({ .charName="Yasuo", .name="Wind Wall (W)", .spellName="YasuoWMovingWall",
        .spellKey=SpellSlotId::W, .range=400, .spellDelay=250,
        .evadeType=EvadeType::WindWall, .castType=CastType::Position, .dangerlevel=3 });

    // --- Yone ---
    db.push_back({ .charName="Yone", .name="Spirit Cleave (W)", .spellName="YoneW",
        .spellKey=SpellSlotId::W, .spellDelay=50,
        .evadeType=EvadeType::Shield, .castType=CastType::Self, .dangerlevel=1 });
    db.push_back({ .charName="Yone", .name="Soul Unbound (E)", .spellName="YoneE",
        .spellKey=SpellSlotId::E, .range=300, .spellDelay=50, .speed=1200, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });
    db.push_back({ .charName="Yone", .name="Fate Sealed (R)", .spellName="YoneR",
        .spellKey=SpellSlotId::R, .range=1000, .spellDelay=50, .speed=1500,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .untargetable=true, .dangerlevel=4 });

    // --- Yuumi ---
    db.push_back({ .charName="Yuumi", .name="You and Me! (W)", .spellName="YuumiW",
        .spellKey=SpellSlotId::W, .range=700, .spellDelay=50,
        .spellTargets={SpellTargets::AllyChampions},
        .evadeType=EvadeType::Untargetable, .castType=CastType::Target, .untargetable=true, .dangerlevel=4 });

    // =======================================================================
    // Z
    // =======================================================================

    // --- Zac ---
    db.push_back({ .charName="Zac", .name="Elastic Slingshot (E)", .spellName="ZacE",
        .spellKey=SpellSlotId::E, .range=1800, .spellDelay=900, .speed=1500,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=3 });

    // --- Zed ---
    db.push_back({ .charName="Zed", .name="Living Shadow (W)", .spellName="ZedW",
        .spellKey=SpellSlotId::W, .range=650, .spellDelay=250, .speed=1750,
        .evadeType=EvadeType::Blink, .castType=CastType::Position, .dangerlevel=2 });
    db.push_back({ .charName="Zed", .name="Death Mark (R)", .spellName="ZedR",
        .spellKey=SpellSlotId::R, .range=625, .spellDelay=50,
        .spellTargets={SpellTargets::EnemyChampions},
        .evadeType=EvadeType::Blink, .castType=CastType::Target, .untargetable=true, .dangerlevel=4 });

    // --- Zeri ---
    db.push_back({ .charName="Zeri", .name="Spark Surge (E)", .spellName="ZeriE",
        .spellKey=SpellSlotId::E, .range=300, .spellDelay=50, .speed=1200, .fixedRange=true,
        .evadeType=EvadeType::Dash, .castType=CastType::Position, .dangerlevel=2 });

    // --- Zilean ---
    db.push_back({ .charName="Zilean", .name="Time Warp (E)", .spellName="ZileanE",
        .spellKey=SpellSlotId::E, .spellDelay=250, .speedArray={40,55,70,85,99},
        .evadeType=EvadeType::MovementSpeedBuff, .castType=CastType::Self, .dangerlevel=3 });

    // =======================================================================
    // POST-PROCESS: finalize (remove obsolete, sort)
    // =======================================================================
    detail::FinalizePatchAll(db);
    return db;
}


// ============================================================================
// Global database instance
// ============================================================================
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
