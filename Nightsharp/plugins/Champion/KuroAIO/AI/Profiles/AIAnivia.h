#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Anivia is modeled as a persistent control mage, not a Q-E burst template.
// The controller tracks the real Flash Frost missile and both damage stages,
// Crystallize's occupied terrain, Frostbite's impact-time Chill race, the
// 1.5-second Glacial Storm growth clock, mana drain and Rebirth state.
inline constexpr ChampionProfile Anivia = [] {
    ChampionProfile p{};
    p.ChampionName = "Anivia";
    p.DisplayName = "Anivia";
    p.InternalId = "champion.kuroaio.ai.anivia";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::ObjectTracking |
                  Mechanic::Mark | Mechanic::WallInteraction |
                  Mechanic::Terrain | Mechanic::Revive |
                  Mechanic::AutoWeave | Mechanic::Channel;
    p.Ultimate = UltimatePolicy::RecastControl;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Flash Frost", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Peel | Intent::Setup |
            Intent::Recast | Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::LaneClear |
            Mode::Jungle | Mode::Automatic,
        1075.0f, 0.25f, 220.0f, 950.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 96;
    p.Spells[0].HarassManaPercent = 48.0f;
    p.Spells[0].ClearManaPercent = 58.0f;
    p.Spells[0].RecastSpellName = "FlashFrostSpell2";
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Crystallize", CastKind::Position,
        Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Interrupt | Intent::AntiGapcloser | Intent::Peel |
            Intent::Setup | Intent::AllyUtility,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1000.0f, 0.25f, 150.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[1].Priority = 93;
    p.Spells[1].MinimumAoeTargets = 1;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Frostbite", CastKind::EnemyTarget,
        Intent::Damage | Intent::Finisher | Intent::LastHit |
            Intent::Jungle | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::LastHit |
            Mode::Jungle | Mode::Automatic,
        650.0f, 0.25f, 0.0f, 1600.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 88;
    p.Spells[2].HarassManaPercent = 38.0f;
    p.Spells[2].ClearManaPercent = 42.0f;
    p.Spells[2].RequiredTargetBuff = "chilled";
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].AllowOnMinions = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Glacial Storm", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::Setup |
            Intent::Waveclear | Intent::Jungle | Intent::Objective |
            Intent::Recast | Intent::Channel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::LaneClear |
            Mode::Jungle | Mode::Automatic,
        750.0f, 0.25f, 400.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 91;
    p.Spells[3].TriggerRange = 400.0f;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].HarassManaPercent = 62.0f;
    p.Spells[3].ClearManaPercent = 52.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "hold Q until committed, secure both Q hits, then impact-timed E",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireNoCrowdControl),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireRecast),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireCrowdControl));

    p.AllIn = Plan(
        "lead R, wall the exit, take first empowered E, hold Q, then second E",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireCrowdControl),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireNoCrowdControl),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireCrowdControl));

    p.Flee = Plan(
        "stun the committed pursuer, split with wall, leave a storm behind",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 610.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 31.0f;
    p.UltimateTargetHealthPercent = 72.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 36;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "RebirthReady";
    p.MarkBuff = "chilled";
    p.ChannelBuff = "GlacialStorm";
    p.UltimateBuff = "GlacialStorm";
    p.TrackedObjectToken = "cryo_storm";
    p.ThemeFrom = 0xFF7DEBFFu;
    p.ThemeTo = 0xFF466BC7u;
    p.ThemeSpeed = 0.72f;
    p.TacticalSummary =
        "Hold Flash Frost as pressure until a committed or wall-forced line, "
        "let it pass before detonation when both hits are available, cast "
        "Frostbite by impact-time Chill rather than cast-time appearance, "
        "shape exits with ally-safe Crystallize, and keep or relocate a "
        "growing Glacial Storm around full-Chill, mana and double-E windows.";
    p.ResearchSummary =
        "Riot champion and 10.25/14.22/25.20/26.10 patch records, "
        "CommunityDragon 16.14 champion/bin data, Meraki cross-check, current "
        "Master and Challenger Anivia OTP combo guides, current specialist "
        "AMA/mechanics discussions, video/GIF demonstrations, local OKTW "
        "ports, and deterministic Q/W/E/R geometry/timing regressions.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
