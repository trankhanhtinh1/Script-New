#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Riven = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Riven;
    p.DisplayName = "Riven";
    p.InternalId = "champion.kuroaio.ai.riven";
    p.PrimaryArchetype = Archetype::Skirmisher;
    p.Resource = ResourceModel::None;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash |
                  Mechanic::Execute | Mechanic::AutoWeave | Mechanic::WallInteraction;
    p.PreferredCombatDistance = 260.0f;
    p.EngageHealthPercent = 44.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 55.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 52;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "RivenPassiveAABuff";
    p.ChannelBuff = "RivenFeint";
    p.UltimateBuff = "RivenFengShuiEngine";
    p.TrackedObjectToken = "Riven";
    p.ThemeFrom = 0xFFDD4D65u;
    p.ThemeTo = 0xFFFFB45Bu;
    p.TacticalSummary =
        "AA-weaved Broken Wings chain: reserve Q3 for knock-up or escape, use W at melee range, "
        "E as shielded cursor-respecting gap close, and reserve Wind Slash for a verified execute.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift baseline; runtime names, buff events, "
        "animation timing, and conservative wall/turret safety are kept explicit.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Broken Wings", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::CrowdControl | Intent::Setup |
            Intent::Recast | Intent::Waveclear | Intent::Jungle | Intent::LastHit | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::LastHit |
            Mode::Flee | Mode::Automatic,
        260.0f, 0.25f, 85.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[0].Priority = 90;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].RecastSpellName = "RivenTriCleave";

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Ki Burst", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Interrupt | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        260.0f, 0.0f, 260.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 86;
    p.Spells[1].WeaveAfterAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Valor", CastKind::Position,
        Intent::Shield | Intent::Mobility | Intent::Engage | Intent::Disengage | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        250.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Cursor;
    p.Spells[2].DashDistance = 250.0f;
    p.Spells[2].Priority = 94;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Blade of the Exile", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::Execute | Intent::Engage | Intent::Recast,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1100.0f, 0.25f, 100.0f, 1600.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 99;
    p.Spells[3].TargetHealthPercent = 55.0f;
    p.Spells[3].RecastSpellName = "RivenIzunaBlade";

    SpellSpec r2 = p.Spells[3];
    r2.Name = "Wind Slash";
    r2.Kind = CastKind::Line;
    r2.Aim = AimPolicy::Prediction;
    p.Variants[0] = { SDK::SpellSlot::R, "RivenIzunaBlade", r2 };
    p.VariantCount = 1;

    p.Trade = Plan("Q-W-E weave",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 0, 650),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireAfterAttack, 80, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 150, 1000),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack | StepRule::RequireRecast, 250, 1200));
    p.AllIn = Plan("Blade of the Exile execute",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 650),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 60, 800),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireAfterAttack, 180, 950),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack | StepRule::RequireRecast, 250, 1150),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireTargetLow, 320, 1400),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireRecast | StepRule::RequireTargetLow, 600, 1700));
    p.Flee = Plan("Cursor-safe Q/E retreat",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 600),
        Step(SDK::SpellSlot::Q, StepRule::RequireNoCrowdControl | StepRule::RequireSafePosition, 80, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
