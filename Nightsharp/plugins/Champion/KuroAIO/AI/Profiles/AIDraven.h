#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Draven = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Draven;
    p.DisplayName = "Draven";
    p.InternalId = "champion.kuroaio.ai.draven";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::ObjectTracking | Mechanic::AutoWeave |
                  Mechanic::ReturnProjectile | Mechanic::AutoReset |
                  Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::GlobalExecute;

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Spinning Axe", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::AutoReset | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 99;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Blood Rush", CastKind::Self,
        Intent::Buff | Intent::Disengage | Intent::Mobility,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[1].Priority = 94;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 38.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Stand Aside", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Peel |
            Intent::Interrupt | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1050.0f, 0.25f, 130.0f, 1400.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 97;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Whirling Death", CastKind::Line,
        Intent::Damage | Intent::Execute | Intent::Finisher |
            Intent::Interrupt,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        25000.0f, 0.4f, 160.0f, 2000.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 100;
    p.Spells[3].Hitchance = SDK::HitChance::High;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Keep one spinning axe active, weave Blood Rush only when it advances a catch or trade, and throw Stand Aside on a reachable target",
        Step(SDK::SpellSlot::Q, StepRule::RequireFirstCast | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup));
    p.AllIn = Plan(
        "Maintain two axes, secure the selected target with E, then use a return-capable Whirling Death only for execute or multi-target value",
        Step(SDK::SpellSlot::Q, StepRule::RequireFirstCast),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::HoldForExecute));
    p.Flee = Plan(
        "Use Blood Rush for catch movement and Stand Aside for immediate peel; never spend an axe or global return while escaping",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 625.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 42.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 24;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "DravenPassiveStacks";
    p.ChannelBuff = "DravenSpinningAttack";
    p.ThemeFrom = 0xFFEF9B2Du;
    p.ThemeTo = 0xFFE3312Au;
    p.ThemeSpeed = 0.90f;
    p.TacticalSummary =
        "Treat every axe as an owned landing object: preserve a safe catch route, keep the selected/orbwalker target stable through the AA windup, and spend W only when it buys a catch, spacing or escape. E is a high-confidence peel/interrupt tool; R is a return-aware global execute and must not be fired merely because it is ready.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Draven kit review: Q spinning-axe buff and dropped-object lifecycle, W Blood Rush speed reset, E Stand Aside displacement and R two-pass Whirling Death. Controller policy follows conservative catch safety and manual-return precedence when telemetry is incomplete.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
