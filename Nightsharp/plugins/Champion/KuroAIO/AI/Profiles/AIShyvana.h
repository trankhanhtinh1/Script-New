#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Shyvana = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Shyvana;
    p.DisplayName = "Shyvana";
    p.InternalId = "champion.kuroaio.ai.shyvana";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Fury;
    p.Mechanics = Mechanic::Transform | Mechanic::MultiForm | Mechanic::Dash |
                  Mechanic::Mark | Mechanic::AutoWeave | Mechanic::AutoReset |
                  Mechanic::ObjectTracking | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 125.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 65.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ShyvanaPassive";
    p.FormBuff = "ShyvanaDragonForm";
    p.TrackedObjectToken = "ShyvanaE";
    p.TacticalSummary =
        "Fury diver: Twin Bite resets the next attack, Burnout supplies movement and burn, "
        "Flame Breath marks targets, and Dragon's Descent commits the dragon form only through safe endpoints.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 data models Fury of the Dragonborn, Q auto reset, "
        "W burn and movement, E projectile/mark, and R flight, impact, transformation and fury drain.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Twin Bite",
        CastKind::Self, Intent::Damage | Intent::Buff | Intent::AutoReset |
            Intent::Recast | Intent::Jungle | Intent::LastHit,
        AllModes, 125.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Priority = 94;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].RecastSpellName = "ShyvanaDoubleAttack";
    p.Spells[0].ComboManaPercent = 0.0f;
    p.Spells[0].HarassManaPercent = 0.0f;
    p.Spells[0].ClearManaPercent = 0.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Burnout",
        CastKind::Self, Intent::Damage | Intent::Mobility | Intent::Buff |
            Intent::Waveclear | Intent::Jungle,
        AllModes, 350.0f, 0.0f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 86;
    p.Spells[1].PreserveAutoAttack = false;
    p.Spells[1].ComboManaPercent = 0.0f;
    p.Spells[1].HarassManaPercent = 0.0f;
    p.Spells[1].ClearManaPercent = 0.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Flame Breath",
        CastKind::Line, Intent::Damage | Intent::CrowdControl | Intent::Mark |
            Intent::Setup | Intent::Jungle | Intent::LastHit,
        AllModes, 925.0f, 0.25f, 60.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 90;
    p.Spells[2].ComboManaPercent = 0.0f;
    p.Spells[2].HarassManaPercent = 0.0f;
    p.Spells[2].ClearManaPercent = 0.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Dragon's Descent",
        CastKind::Position, Intent::Damage | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::CrowdControl | Intent::Objective | Intent::Setup,
        AllModes, 1000.0f, 0.7f, 345.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 98;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].ComboManaPercent = 0.0f;
    p.Spells[3].HarassManaPercent = 0.0f;
    p.Spells[3].ClearManaPercent = 0.0f;

    p.Trade = Plan("mark then Twin Bite weave with Burnout pressure",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireNoMark, 0, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 90, 1100),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireInsideAaRange |
            StepRule::RequireAfterAttack, 140, 1400));
    p.AllIn = Plan("safe dragon flight, mark and empowered attack sequence",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget |
            StepRule::RequireSafePosition, 0, 1200),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireNoMark, 100, 1300),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 180, 1500),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireInsideAaRange |
            StepRule::RequireAfterAttack, 260, 1700));
    p.Flee = Plan("dragon flight away from danger and Burnout retreat",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition |
            StepRule::AllowDuringWindup, 0, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 100, 1000));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
