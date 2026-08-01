#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Udyr = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Udyr;
    p.DisplayName = "Udyr";
    p.InternalId = "champion.kuroaio.ai.udyr";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Stance | Mechanic::MultiForm |
                  Mechanic::AutoWeave | Mechanic::ObjectTracking | Mechanic::MissingHealth |
                  Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 260.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 65.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "UdyrPassive";
    p.FormBuff = "UdyrQ/UdyrW/UdyrE/UdyrR";
    p.TrackedObjectToken = "UdyrR";
    p.TacticalSummary =
        "Four-stance diver: Wilding Claw pressures isolated targets, Iron Mantle guards low health, "
        "Blazing Stampede closes and stuns, and Wingborne Storm controls zones and camps.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 spell data models 40-mana six-second stances, "
        "four-second Awakened recasts, Q lightning, W shield/heal, E stun/immunity and R storm slow/damage.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Wilding Claw", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::Recast | Intent::AutoReset | Intent::Jungle |
            Intent::LastHit,
        AllModes, 125.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Priority = 92;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].RecastSpellName = "UdyrQ2Activation";
    p.Spells[0].ComboManaPercent = 0.0f;
    p.Spells[0].HarassManaPercent = 45.0f;
    p.Spells[0].ClearManaPercent = 30.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Iron Mantle", CastKind::Self,
        Intent::Shield | Intent::Heal | Intent::Buff | Intent::Recast | Intent::Jungle,
        AllModes, 0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 96;
    p.Spells[1].PreserveAutoAttack = false;
    p.Spells[1].PlayerHealthPercent = 72.0f;
    p.Spells[1].RecastSpellName = "UdyrW2Activation";
    p.Spells[1].ComboManaPercent = 0.0f;
    p.Spells[1].HarassManaPercent = 50.0f;
    p.Spells[1].ClearManaPercent = 30.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Blazing Stampede", CastKind::Self,
        Intent::Mobility | Intent::Engage | Intent::CrowdControl | Intent::Disengage |
            Intent::Recast | Intent::AntiGapcloser,
        AllModes, 370.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 90;
    p.Spells[2].PreserveAutoAttack = false;
    p.Spells[2].RecastSpellName = "UdyrE2Activation";
    p.Spells[2].PlayerHealthPercent = 75.0f;
    p.Spells[2].ComboManaPercent = 0.0f;
    p.Spells[2].HarassManaPercent = 45.0f;
    p.Spells[2].ClearManaPercent = 25.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Wingborne Storm", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Recast | Intent::Waveclear |
            Intent::Jungle | Intent::Objective | Intent::Setup,
        AllModes, 370.0f, 0.1f, 370.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 88;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].RecastSpellName = "UdyrR2Activation";
    p.Spells[3].ComboManaPercent = 0.0f;
    p.Spells[3].HarassManaPercent = 40.0f;
    p.Spells[3].ClearManaPercent = 25.0f;

    p.Trade = Plan("stance weave into controlled recast",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireOutsideAaRange, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 100, 1000),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget, 180, 1200),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 220, 1400));
    p.AllIn = Plan("storm or claw opener, stampede stun, mantle reset",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget, 80, 1100),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 140, 1300),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow | StepRule::RequireRecast, 200, 1500));
    p.Flee = Plan("stampede immunity and storm slow retreat",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 900),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 120, 1150),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 180, 1300));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
