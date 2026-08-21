#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Nocturne = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Nocturne;
    p.DisplayName = "Nocturne";
    p.InternalId = "champion.kuroaio.ai.nocturne";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::Recast | Mechanic::Global |
                  Mechanic::SpellShield | Mechanic::Tether |
                  Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::SingleTarget;
    p.PreferredCombatDistance = 175.0f;
    p.EngageHealthPercent = 46.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 48;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "NocturneUmbraBlades";
    p.MarkBuff = "NocturneUnspeakableHorror";
    p.UltimateBuff = "NocturneParanoia";
    p.TrackedObjectToken = "NocturneDuskbringer";
    p.ThemeFrom = 0xFF302060u;
    p.ThemeTo = 0xFF6F4BB8u;
    p.TacticalSummary =
        "Trail-backed melee diver: preserve Umbra Blades cleave/healing, "
        "shield a real incoming spell, hold the fear tether, and only recast "
        "Paranoia into a survivable landing.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values; Q uses "
        "the 1125 displayed range and R uses its rank-scaled global range.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Duskbringer", CastKind::Line,
        Intent::Damage | Intent::Buff | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        1125.0f, 0.25f, 120.0f, 1600.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 86;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].HarassManaPercent = 45.0f;
    p.Spells[0].ClearManaPercent = 42.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Shroud of Darkness", CastKind::Self,
        Intent::Shield | Intent::Buff,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        20.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 100;
    p.Spells[1].PreserveAutoAttack = false;
    p.Spells[1].ComboManaPercent = 8.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Unspeakable Horror", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::AntiGapcloser | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        425.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 92;
    p.Spells[2].TriggerRange = 425.0f;
    p.Spells[2].DesiredDistance = 250.0f;
    p.Spells[2].ComboManaPercent = 12.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Paranoia", CastKind::EnemyTarget,
        Intent::Damage | Intent::Mobility | Intent::Engage |
            Intent::Execute | Intent::Recast,
        Mode::Combo | Mode::Automatic,
        4000.0f, 0.0f, 0.0f, 1800.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 98;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].TargetHealthPercent = 48.0f;
    p.Spells[3].ComboManaPercent = 20.0f;

    p.Variants[0] = { SDK::SpellSlot::R, "NocturneParanoia2", p.Spells[3] };
    p.Variants[0].Spec.Intents = Intent::Damage | Intent::Mobility |
                                  Intent::Engage | Intent::Execute |
                                  Intent::Recast;
    p.VariantCount = 1;

    p.Trade = Plan("Dusk trail tether trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 750),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange,
             80, 950));
    p.AllIn = Plan("Paranoia trail tether all-in",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition,
             0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 120, 900),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange,
             170, 1100));
    p.Flee = Plan("Trail and shield retreat",
        Step(SDK::SpellSlot::W,
             StepRule::RequireNoCrowdControl | StepRule::AllowDuringWindup,
             0, 450),
        Step(SDK::SpellSlot::Q, StepRule::RequireSafePosition, 30, 700));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
