#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Nunu = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Nunu;
    p.DisplayName = "Nunu & Willump";
    p.InternalId = "champion.kuroaio.ai.nunu";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::Channel | Mechanic::ObjectTracking |
                  Mechanic::Stack | Mechanic::Terrain | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 500.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 62.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 55;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "NunuP";
    p.ChannelBuff = "NunuR";
    p.UltimateBuff = "NunuR";
    p.TrackedObjectToken = "NunuSnowball";
    p.TacticalSummary =
        "Consume-first objective controller with charge-distance Snowball ganks, three-stack Snowball Barrage roots, and interrupt-aware Absolute Zero channels.";
    p.ResearchSummary =
        "CommunityDragon 16.15 NunuQ/W/E/R data and Riot 26.15 (no Nunu changes): objective Consume sequencing, W charge tiers, E stack/root state and R slow/channel gates are event-reconciled.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Consume", CastKind::EnemyTarget,
        Intent::Damage | Intent::Heal | Intent::Execute | Intent::LastHit |
            Intent::Jungle | Intent::Objective,
        AllModes, 125.0f, 0.30f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 100;
    p.Spells[0].PreserveAutoAttack = false;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].HarassManaPercent = 30.0f;
    p.Spells[0].ClearManaPercent = 20.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Biggest Snowball Ever!", CastKind::ChargedLine,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Mobility | Intent::Setup | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        2500.0f, 0.25f, 200.0f, 350.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 95;
    p.Spells[1].PreserveAutoAttack = false;
    p.Spells[1].ChargeMinRange = 750;
    p.Spells[1].ChargeMaxRange = 1750;
    p.Spells[1].ChargeDurationSeconds = 5.0f;
    p.Spells[1].ComboManaPercent = 35.0f;
    p.Spells[1].HarassManaPercent = 50.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Snowball Barrage", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Recast |
            Intent::Jungle | Intent::Peel | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        625.0f, 0.25f, 450.0f, 1850.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 88;
    p.Spells[2].PreserveAutoAttack = false;
    p.Spells[2].RecastSpellName = "NunuESnowballBurstFire";
    p.Spells[2].ChargeBuffName = "NunuEStackMarker";
    p.Spells[2].ComboManaPercent = 30.0f;
    p.Spells[2].HarassManaPercent = 45.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Absolute Zero", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Shield | Intent::Channel |
            Intent::Engage | Intent::Disengage | Intent::Interrupt,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        650.0f, 0.01f, 650.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 98;
    p.Spells[3].PreserveAutoAttack = false;
    p.Spells[3].TargetHealthPercent = 62.0f;
    p.Spells[3].MinimumAoeTargets = 1;

    p.Trade = Plan("Snowball poke into Barrage slow",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireOutsideAaRange, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 120, 1100));
    p.AllIn = Plan("Snowball engage, three stacks, Absolute Zero",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireOutsideAaRange, 0, 1300),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 120, 1600),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireCrowdControl | StepRule::AllowDuringWindup, 250, 3000));
    p.Flee = Plan("Barrage peel and safe cursor snowball",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 100, 1200),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::AllowDuringWindup, 250, 1800));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
