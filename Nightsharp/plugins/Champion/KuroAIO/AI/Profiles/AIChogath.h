#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Chogath = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Chogath;
    p.DisplayName = "Cho'Gath";
    p.InternalId = "champion.kuroaio.ai.chogath";
    p.PrimaryArchetype = Archetype::Juggernaut;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Execute | Mechanic::Stack | Mechanic::Recast |
                  Mechanic::AutoReset | Mechanic::AutoWeave | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::Execute;
    p.PreferredCombatDistance = 250.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 42.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "Feast";
    p.MarkBuff = "VorpalSpikes";
    p.FormBuff = "Feast";
    p.TacticalSummary =
        "Cho'Gath owns a delayed predicted Rupture, a directional silence cone,"
        " an attack-reset Vorpal Spikes toggle and Feast execute.  Feast chooses"
        " epic objectives before ordinary farm and reconciles champion/monster"
        " stacks from the live Feast buff and spell events.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values: Rupture"
        " range 950 and radius 230, Feral Scream 675/28 degree cone, Vorpal"
        " max-health scaling with Feast stacks, and Feast 175 plus 2.5 range per"
        " stack (25 bonus cap), 1200 epic-monster base damage.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Rupture", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit | Intent::AntiGapcloser,
        AllModes, 950.0f, 0.65f, 460.0f, 20.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 94;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].HarassManaPercent = 48.0f;
    p.Spells[0].ClearManaPercent = 35.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Feral Scream", CastKind::Cone,
        Intent::Damage | Intent::CrowdControl | Intent::Interrupt | Intent::Peel |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        675.0f, 0.25f, 28.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCone);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 91;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].HarassManaPercent = 54.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Vorpal Spikes", CastKind::Toggle,
        Intent::Damage | Intent::AutoReset | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        40.0f, 0.0f, 170.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 82;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = false;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Feast", CastKind::AnyTarget,
        Intent::Damage | Intent::Execute | Intent::Finisher | Intent::Objective |
            Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::LaneClear | Mode::Jungle | Mode::LastHit |
            Mode::Automatic,
        175.0f, 0.25f, 160.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].TargetHealthPercent = 42.0f;
    p.Spells[3].AllowOnMinions = true;
    p.Spells[3].MaximumEnemiesAtDestination = 2;

    p.Trade = Plan("Rupture into silence and Vorpal weave",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 160, 1000),
        Step(SDK::SpellSlot::E, StepRule::RequireInsideAaRange |
            StepRule::AllowDuringWindup, 260, 1200));
    p.AllIn = Plan("Feast execute after crowd control",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 170, 1050),
        Step(SDK::SpellSlot::E, StepRule::RequireInsideAaRange |
            StepRule::AllowDuringWindup, 260, 1300),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireTargetLow |
            StepRule::HoldForExecute, 360, 1500));
    p.Flee = Plan("Silence and rupture peel",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 800),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 100, 950));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
