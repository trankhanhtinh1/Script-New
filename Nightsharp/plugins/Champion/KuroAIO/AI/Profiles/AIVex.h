#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Vex = [] {
    ChampionProfile p{};
    p.ChampionName = "Vex";
    p.DisplayName = "Vex";
    p.InternalId = "champion.kuroaio.ai.vex";
    p.PrimaryArchetype = Archetype::BurstMage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Mark | Mechanic::Recast | Mechanic::Dash |
                  Mechanic::ObjectTracking | Mechanic::AutoReset |
                  Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 35.0f;
    p.UltimateTargetHealthPercent = 68.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 52;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.MarkBuff = "VexGloom";
    p.UltimateBuff = "VexR";
    p.TrackedObjectToken = "VexShadow";
    p.ThemeFrom = 0xFF7C5CFFu;
    p.ThemeTo = 0xFFB99BFFu;
    p.ThemeSpeed = 0.92f;
    p.TacticalSummary =
        "Gloom-aware control mage: layer Q/E projectiles, consume a confirmed mark for fear, "
        "and use Shadow Surge recasts only when the landing is safe or lethal.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon 16.15 spell, buff and missile behavior; "
        "dash-reset state is reconciled from events and polling when telemetry is incomplete.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Mistral Bolt", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        1200.0f, 0.25f, 100.0f, 1900.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 86;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 50.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Personal Space", CastKind::Self,
        Intent::Damage | Intent::Shield | Intent::CrowdControl | Intent::Peel |
            Intent::AntiGapcloser | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        475.0f, 0.0f, 475.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 94;
    p.Spells[1].TriggerRange = 475.0f;
    p.Spells[1].HarassManaPercent = 48.0f;
    p.Spells[1].ClearManaPercent = 62.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Looming Darkness", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::Peel | Intent::Interrupt,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic | Mode::Flee,
        800.0f, 0.25f, 250.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 92;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].HarassManaPercent = 55.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Shadow Surge", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Recast | Intent::Execute | Intent::Interrupt,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        2000.0f, 0.25f, 140.0f, 1600.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::SafeCursor;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAmmo = 1;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].RecastSpellName = "VexRRecast";

    p.Trade = Plan("Gloom poke and fear",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::RequireCrowdControl | StepRule::AllowDuringWindup));
    p.AllIn = Plan("Marked Shadow dive",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireCrowdControl),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireCrowdControl));
    p.Flee = Plan("Fear and safe Shadow exit",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::ManualAssistOnly));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
