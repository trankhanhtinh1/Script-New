#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Aurora is a weave-and-reposition burst mage. Q2 is not a generic recast:
// its missing-health multiplier competes with extra autos/E, and every marked
// body returns a bolt through Aurora's current position. W is both a scarce
// stealth dash and a takedown-reset resource, E damage is coupled to a recoil
// endpoint, and R's leap/arena/portal are three distinct geometries.
inline constexpr ChampionProfile Aurora = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Aurora;
    p.DisplayName = "Aurora";
    p.InternalId = "champion.kuroaio.ai.aurora";
    p.PrimaryArchetype = Archetype::BurstMage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Mark |
                  Mechanic::Stack | Mechanic::AutoWeave |
                  Mechanic::ReturnProjectile | Mechanic::MissingHealth |
                  Mechanic::Dash | Mechanic::Terrain |
                  Mechanic::Execute | Mechanic::ObjectTracking |
                  Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Twofold Hex", CastKind::Line,
        Intent::Damage | Intent::Recast | Intent::Finisher |
            Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic,
        900.0f, 0.25f, 90.0f, 1600.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 96;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 45.0f;
    p.Spells[0].ClearManaPercent = 55.0f;
    p.Spells[0].RecastSpellName = "AuroraQRecast";

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Across the Veil", CastKind::Position,
        Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Setup | Intent::AllyUtility,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        450.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 92;
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].MaximumEnemiesAtDestination = 1;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 64.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "The Weirding", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Disengage |
            Intent::Peel | Intent::Finisher | Intent::Waveclear |
            Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        825.0f, 0.35f, 175.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 94;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 58.0f;
    p.Spells[2].ClearManaPercent = 62.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Between Worlds", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Setup | Intent::Peel |
            Intent::Finisher | Intent::Recast,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        700.0f, 0.25f, 700.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 98;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].RecastSpellName = "AuroraRRecast";

    p.Trade = Plan(
        "Q first, preserve an auto or E application, then pull Q2 through aligned marks",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.AllIn = Plan(
        "enter only from a safe W/R angle, seek two passive procs, and leave E recoil as the final spacing tool",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition));

    p.Flee = Plan(
        "slow the pursuer with E, use W on a cursor-guided wall route, and reserve R portal for a committed threat",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition |
                 StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition |
                 StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 575.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 40.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 34;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "AuroraPassive";
    p.FormBuff = "AuroraW";
    p.UltimateBuff = "AuroraR";
    p.TrackedObjectToken = "Aurora";
    p.ThemeFrom = 0xFFB56CFFu;
    p.ThemeTo = 0xFF59E7FFu;
    p.ThemeSpeed = 0.72f;
    p.TacticalSummary =
        "Automate attacks and movement with cursor-guided routes; open Q before the "
        "auto, delay Q2 for missing health unless marked-wave bolts would be "
        "lost, spend W only on safe angle/reset routes, validate E's opposite "
        "recoil endpoint, and treat R leap, arena center, opposite portal and "
        "early recast as separate decisions.";
    p.ResearchSummary =
        "Pinned to Riot 26.14 and CommunityDragon PC 16.14; reconciled Riot "
        "14.15/14.18/14.23/25.12/25.18 changes with current Rank-1 Aurora "
        "Rebrrt VOD/transcript, Shok's Challenger guide, current high-elo "
        "guides and local spell/passive/evade data.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
