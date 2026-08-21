#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile MasterYi = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::MasterYi;
    p.DisplayName = "Master Yi";
    p.InternalId = "champion.kuroaio.ai.masteryi";
    p.PrimaryArchetype = Archetype::Skirmisher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Blink | Mechanic::Channel | Mechanic::Execute |
                  Mechanic::Mark | Mechanic::AutoWeave | Mechanic::AutoReset |
                  Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 125.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 72.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 42;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "MasterYiDoubleStrike";
    p.ChannelBuff = "Meditate";
    p.UltimateBuff = "Highlander";
    p.TacticalSummary =
        "Reset-aware Wuju skirmisher: preserve Alpha Strike to close or dodge, prime Wuju Style for true-damage autos, and channel Meditation only behind a real safety gate.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon 16.15 Master Yi Q/W/E/R data; Alpha Strike reach, Meditation reduction, Wuju true-damage timing, and Highlander takedown extensions are event-reconciled.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Alpha Strike", CastKind::EnemyTarget,
        Intent::Damage | Intent::Mobility | Intent::Setup | Intent::Finisher,
        AllModes, 600.0f, 0.15f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 90;
    p.Spells[0].HarassManaPercent = 40.0f;
    p.Spells[0].ClearManaPercent = 30.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Meditate", CastKind::Self,
        Intent::Heal | Intent::Buff | Intent::Shield | Intent::Channel,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 84;
    p.Spells[1].PlayerHealthPercent = 38.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Wuju Style", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::Finisher | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        20.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 96;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].TargetHealthPercent = 65.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Highlander", CastKind::Self,
        Intent::Buff | Intent::Engage | Intent::Disengage | Intent::Cleanse,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 100;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan("Alpha Strike and Wuju Style poke",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireOutsideAaRange, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack, 80, 900));
    p.AllIn = Plan("Highlander reset chain",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack, 60, 1100),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 120, 1300));
    p.Flee = Plan("Alpha Strike retreat and Meditation",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 800),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow | StepRule::RequireSafePosition, 90, 1000));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
