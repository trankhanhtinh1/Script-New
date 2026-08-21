#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Ekko = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Ekko;
    p.DisplayName = "Ekko";
    p.InternalId = "champion.kuroaio.ai.ekko";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Dash | Mechanic::ReturnProjectile |
                  Mechanic::MissingHealth | Mechanic::Execute | Mechanic::AutoWeave |
                  Mechanic::ObjectTracking | Mechanic::Terrain;
    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 28.0f;
    p.UltimateTargetHealthPercent = 40.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 54;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "EkkoPassive";
    p.ChannelBuff = "EkkoW";
    p.UltimateBuff = "EkkoR";
    p.TrackedObjectToken = "Ekko";
    p.ThemeFrom = 0xFF6CD6FFu;
    p.ThemeTo = 0xFFB98CFFu;
    p.TacticalSummary =
        "Track Z-Drive Resonance per target, predict Timewinder's outbound and return, "
        "stage Parallel Convergence, empower Phase Dive's next attack, and rewind only to a safe endpoint.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 baseline with explicit three-hit passive state, "
        "three-second W delay, Q collision, E turret checks, and Chronobreak health/damage gates.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Timewinder", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Recast |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::LastHit |
            Mode::Flee | Mode::Automatic,
        1075.0f, 0.25f, 120.0f, 1650.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 91;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].RecastSpellName = "EkkoQReturn";

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Parallel Convergence", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Shield | Intent::Setup |
            Intent::Interrupt | Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1600.0f, 3.0f, 750.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 95;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Phase Dive", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Setup | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        325.0f, 0.0f, 250.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Cursor;
    p.Spells[2].DashDistance = 325.0f;
    p.Spells[2].Priority = 94;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].WeaveAfterAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Chronobreak", CastKind::Self,
        Intent::Damage | Intent::Execute | Intent::Disengage | Intent::Recast | Intent::MissingHealth,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        4000.0f, 0.0f, 750.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 99;
    p.Spells[3].TargetHealthPercent = 40.0f;
    p.Spells[3].PlayerHealthPercent = 28.0f;
    p.Spells[3].RecastSpellName = "EkkoChronobreak";

    p.Trade = Plan("Timewinder and Phase Dive poke",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 0, 800),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 260, 1100));
    p.AllIn = Plan("Parallel Convergence Chronobreak commit",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 3300),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 80, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 260, 1200),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireTargetLow, 650, 1800));
    p.Flee = Plan("Phase Dive and Parallel Convergence retreat",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 600),
        Step(SDK::SpellSlot::W, StepRule::RequireNoCrowdControl | StepRule::RequireSafePosition, 100, 3400));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
