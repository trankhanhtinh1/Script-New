#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Sejuani = [] {
    ChampionProfile p{};
    p.ChampionName = "Sejuani";
    p.DisplayName = "Sejuani";
    p.InternalId = "champion.kuroaio.ai.sejuani";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::Stack | Mechanic::ObjectTracking |
                  Mechanic::AllyTarget | Mechanic::MissingHealth | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 525.0f;
    p.EngageHealthPercent = 54.0f;
    p.DefensiveHealthPercent = 35.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "SejuaniPassive";
    p.TrackedObjectToken = "SejuaniRMissile";
    p.TacticalSummary =
        "Frost-armored vanguard: use Arctic Assault only through safe terrain, sequence both flail swings into Permafrost stun stacks, and reserve Glacial Prison for predicted projectile or explosion control.";
    p.ResearchSummary =
        "Riot live 26.15 and CommunityDragon 16.15 values, including passive Frost Armor timing, Q dash, two-phase W, four-stack E stun and R projectile/explosion safety.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Arctic Assault", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Mobility | Intent::Jungle,
        AllModes, 650.0f, 0.25f, 75.0f, 1000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::SafeCursor;
    p.Spells[0].Priority = 94;
    p.Spells[0].DashDistance = 625.0f;
    p.Spells[0].MaximumEnemiesAtDestination = 2;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Winter's Wrath", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::LastHit,
        600.0f, 0.25f, 130.0f, 0.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCone);
    p.Spells[1].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[1].Priority = 88;
    p.Spells[1].WeaveAfterAttack = true;
    p.Spells[1].ComboManaPercent = 30.0f;
    p.Spells[1].HarassManaPercent = 50.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Permafrost", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Recast |
            Intent::Peel | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        600.0f, 0.25f, 1100.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 96;
    p.Spells[2].RequiredTargetBuff = "SejuaniEPassive";
    p.Spells[2].RecastSpellName = "SejuaniE2";
    p.Spells[2].ComboManaPercent = 28.0f;
    p.Spells[2].HarassManaPercent = 45.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Glacial Prison", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Interrupt | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1300.0f, 0.25f, 120.0f, 1600.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].TargetHealthPercent = 48.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("Flail into Permafrost",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireMark, 120, 900));
    p.AllIn = Plan("Safe Arctic Assault and Glacial Prison",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireMark, 180, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget, 260, 1600));
    p.Flee = Plan("Frost peel",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireMark, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 90, 700),
        Step(SDK::SpellSlot::R, StepRule::ManualAssistOnly | StepRule::RequireTarget, 180, 1500));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
