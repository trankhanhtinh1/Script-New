#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Amumu is modeled as a two-charge access/lockdown vanguard.  Q is not a
// generic line spell: the controller resolves the first colliding unit,
// evaluates the forced arrival, reserves the second charge, and chooses
// between a Curse AA weave and an immediate buffered R.  W, E and R each own
// separate mana, timing and team-follow-up policies in AIAmumuController.
inline constexpr ChampionProfile Amumu = [] {
    ChampionProfile p{};
    p.ChampionName = "Amumu";
    p.DisplayName = "Amumu";
    p.InternalId = "champion.kuroaio.ai.amumu";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Ammo | Mechanic::Dash | Mechanic::Mark |
                  Mechanic::AutoWeave | Mechanic::Stack;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Bandage Toss", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Disengage | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Peel | Intent::Setup |
            Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Jungle |
            Mode::Automatic,
        1100.0f, 0.25f, 80.0f, 2000.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 94;
    p.Spells[0].DashDistance = 1100.0f;
    p.Spells[0].MaximumEnemiesAtDestination = 3;
    p.Spells[0].MinimumAmmo = 1;
    p.Spells[0].HarassManaPercent = 48.0f;
    p.Spells[0].ClearManaPercent = 42.0f;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Despair", CastKind::Toggle,
        Intent::Damage | Intent::Buff | Intent::Setup |
            Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Jungle |
            Mode::Automatic,
        350.0f, 0.0f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 72;
    p.Spells[1].TriggerRange = 350.0f;
    p.Spells[1].HarassManaPercent = 55.0f;
    p.Spells[1].ClearManaPercent = 36.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Tantrum", CastKind::Self,
        Intent::Damage | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit | Intent::Peel | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::LastHit |
            Mode::Jungle | Mode::Flee | Mode::Automatic,
        350.0f, 0.25f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 82;
    p.Spells[2].TriggerRange = 350.0f;
    p.Spells[2].HarassManaPercent = 45.0f;
    p.Spells[2].ClearManaPercent = 34.0f;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Curse of the Sad Mummy", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        550.0f, 0.25f, 550.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 100;
    p.Spells[3].TriggerRange = 550.0f;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan(
        "land one safe Q, apply Curse by AA, E, then keep charge two",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireAfterAttack));

    p.AllIn = Plan(
        "Q access, choose immediate R or Curse weave, E, then layered Q2",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireCrowdControl));

    p.Flee = Plan(
        "peel the pursuer, then Q only to a cursor-aligned safe escape unit",
        Step(SDK::SpellSlot::R,
             StepRule::RequireMultiTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireSafePosition));

    p.PreferredCombatDistance = 285.0f;
    p.EngageHealthPercent = 45.0f;
    p.DefensiveHealthPercent = 29.0f;
    p.UltimateTargetHealthPercent = 34.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 38;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "CurseoftheSadMummy";
    p.MarkBuff = "CurseoftheSadMummy";
    p.FormBuff = "AuraofDespair";
    p.ThemeFrom = 0xFF5ED8FFu;
    p.ThemeTo = 0xFF7A5CDBu;
    p.ThemeSpeed = 0.88f;
    p.TacticalSummary =
        "Resolve Bandage Toss's real first collision and arrival before "
        "committing, keep charge two for Flash/dash/peel, weave a Curse AA "
        "only when it does not open a Flash window before R, toggle Despair "
        "around contact and mana reserve, exploit Tantrum attack refunds, and "
        "spend R on predicted high-value lockdown with allied follow-up.";
    p.ResearchSummary =
        "Riot 25.18/10.4/current champion data, CommunityDragon 16.14 bin "
        "and game-data records, current Season 16 Rank-1/Challenger Amumu "
        "material, high-elo clear/gameplay reviews, current combo catalogues, "
        "specialist interaction reports, local plugin audit, and deterministic "
        "collision/timing/damage regressions.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
