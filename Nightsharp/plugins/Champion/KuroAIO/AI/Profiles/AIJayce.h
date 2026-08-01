#pragma once
#include "../AIChampionProfile.h"
namespace Plugins::KuroAIO::AI::Profiles {
inline constexpr ChampionProfile Jayce = [] {
  ChampionProfile p{};
  p.ChampionId = SDK::ChampionId::Jayce;
  p.DisplayName = "Jayce";
  p.InternalId = "champion.kuroaio.ai.jayce";
  p.PrimaryArchetype = Archetype::Specialist;
  p.Resource = ResourceModel::Mana;
  p.Mechanics = Mechanic::MultiForm | Mechanic::ObjectTracking |
                Mechanic::Terrain | Mechanic::DirectionalSweet;
  p.Ultimate = UltimatePolicy::RecastControl;
  p.PreferredCombatDistance = 700.0f;
  p.EngageHealthPercent = 55.0f;
  p.DefensiveHealthPercent = 30.0f;
  p.UltimateTargetHealthPercent = 45.0f;
  p.UltimateMinimumTargets = 1;
  p.MaximumCommitEnemies = 2;
  p.BaseHumanizerMs = 60;
  p.PreferSelectedTarget = true;
  p.AllowTurretDiveIfKillable = false;
  p.ProtectManualChannels = true;
  p.TrackedObjectToken = "JayceAccelerationGate";
  p.TacticalSummary =
      "Dual-form artillery and hammer specialist: use gate-accelerated shock "
      "blasts at range, switch to hammer for safe knockback or lethal dives, "
      "and preserve form state.";
  p.ResearchSummary =
      "Riot 26.15 / CommunityDragon 16.15; Summoner's Rift baseline.";
  p.Spells[0] =
      Spell(SDK::SpellSlot::Q, "To the Skies!/Shock Blast", CastKind::Position,
            Intent::Damage | Intent::Engage | Intent::Waveclear, AllModes,
            1050.0f, 0.25f, 70.0f, FLT_MAX, false, SDK::DamageType::Physical,
            SDK::SpellType::SkillshotLine);
  p.Spells[0].Aim = AimPolicy::Prediction;
  p.Spells[0].Priority = 92;
  p.Spells[1] =
      Spell(SDK::SpellSlot::W, "Lightning Field/Hyper Charge", CastKind::Self,
            Intent::Damage | Intent::AutoReset,
            Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle, 500.0f,
            0.0f, 0.0f, FLT_MAX, false, SDK::DamageType::Physical,
            SDK::SpellType::Targeted);
  p.Spells[1].Priority = 80;
  p.Spells[2] =
      Spell(SDK::SpellSlot::E, "Thundering Blow/Acceleration Gate",
            CastKind::Position,
            Intent::CrowdControl | Intent::Disengage | Intent::Setup, AllModes,
            650.0f, 0.0f, 80.0f, FLT_MAX, false, SDK::DamageType::Physical,
            SDK::SpellType::SkillshotCircle);
  p.Spells[2].Aim = AimPolicy::SafeCursor;
  p.Spells[2].Priority = 88;
  p.Spells[3] = Spell(SDK::SpellSlot::R, "Mercury Cannon/Mercury Hammer",
                      CastKind::Toggle,
                      Intent::Recast | Intent::Engage | Intent::Disengage,
                      AllModes, 0.0f, 0.0f, 0.0f, FLT_MAX, false,
                      SDK::DamageType::Physical, SDK::SpellType::Targeted);
  p.Spells[3].Priority = 75;
  p.Trade = Plan("Gate shock trade",
                 Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 700),
                 Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 100, 1100));
  p.AllIn =
      Plan("Hammer conversion",
           Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 650),
           Step(SDK::SpellSlot::E, StepRule::RequireTarget, 120, 800),
           Step(SDK::SpellSlot::R, StepRule::RequireSafePosition, 200, 1000));
  p.Flee = Plan("Hammer disengage",
                Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 700),
                Step(SDK::SpellSlot::R, StepRule::ManualAssistOnly, 100, 900));
  return p;
}();
} // namespace Plugins::KuroAIO::AI::Profiles
