#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Darius = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Darius;
    p.DisplayName = "Darius";
    p.InternalId = "champion.kuroaio.ai.darius";
    p.PrimaryArchetype = Archetype::Juggernaut;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Execute | Mechanic::Recast |
                  Mechanic::ObjectTracking | Mechanic::AutoReset;
    p.Ultimate = UltimatePolicy::Execute;
    p.PreferredCombatDistance = 250.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 42.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 60;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "DariusHemoMarker";
    p.MarkBuff = "DariusHemo";
    p.UltimateBuff = "DariusExecute";
    p.TrackedObjectToken = "DariusHemoMax";
    p.TacticalSummary =
        "Hemorrhage juggernaut: keep outer-edge Decimate healing, weave Crippling Strike, "
        "pull with Apprehend and reserve Noxian Guillotine for stack-scaled executes.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 values: five Hemorrhage stacks, Noxian Might, "
        "Q outer-edge heal, W reset/slow, E pull and penetration, and R stack-scaled true execute.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Decimate", CastKind::Self,
        Intent::Damage | Intent::Heal | Intent::CrowdControl | Intent::Setup |
            Intent::Jungle | Intent::Waveclear,
        AllModes, 425.0f, 0.75f, 425.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::SelfPosition;
    p.Spells[0].Priority = 94;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].ComboManaPercent = 0.0f;
    p.Spells[0].HarassManaPercent = 40.0f;
    p.Spells[0].ClearManaPercent = 35.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Crippling Strike", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Buff | Intent::AutoReset |
            Intent::Jungle | Intent::LastHit,
        AllModes, 175.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 96;
    p.Spells[1].WeaveAfterAttack = true;
    p.Spells[1].PreserveAutoAttack = false;
    p.Spells[1].ComboManaPercent = 0.0f;
    p.Spells[1].HarassManaPercent = 45.0f;
    p.Spells[1].ClearManaPercent = 35.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Apprehend", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Peel |
            Intent::Interrupt | Intent::AntiGapcloser,
        AllModes, 535.0f, 0.25f, 90.0f, 1500.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCone);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 91;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].ComboManaPercent = 0.0f;
    p.Spells[2].HarassManaPercent = 48.0f;
    p.Spells[2].ClearManaPercent = 100.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Noxian Guillotine", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::Finisher | Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic, 460.0f, 0.0f,
        0.0f, FLT_MAX, false, SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].RecastSpellName = "DariusExecute";
    p.Spells[3].TargetHealthPercent = 48.0f;
    p.Spells[3].PreserveAutoAttack = false;
    p.Spells[3].ComboManaPercent = 0.0f;

    p.Trade = Plan("Hemorrhage stack weave with outer Decimate",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireInsideAaRange | StepRule::AllowDuringWindup, 90, 850),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 160, 1100));
    p.AllIn = Plan("five-stack Noxian Guillotine execute",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireInsideAaRange | StepRule::AllowDuringWindup, 80, 850),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 150, 1050),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireTargetLow | StepRule::HoldForExecute, 260, 1400));
    p.Flee = Plan("Apprehend peel and emergency Guillotine",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 100, 900),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::HoldForExecute, 180, 1000));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
