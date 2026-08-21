#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Mordekaiser = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Mordekaiser;
    p.DisplayName = "Mordekaiser";
    p.InternalId = "champion.kuroaio.ai.mordekaiser";
    p.PrimaryArchetype = Archetype::Juggernaut;
    p.Resource = ResourceModel::Special;
    p.Mechanics = Mechanic::Recast | Mechanic::ObjectTracking | Mechanic::Tether |
                  Mechanic::AutoWeave | Mechanic::Mark;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 350.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 60.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 72;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "MordekaiserPassive";
    p.MarkBuff = "MordekaiserPassive";
    p.ChannelBuff = "MordekaiserR";
    p.UltimateBuff = "MordekaiserR";
    p.TrackedObjectToken = "MordekaiserPassive";
    p.TacticalSummary =
        "Build Darkness Rise through committed attacks and spells, isolate Obliterate, convert Indestructible into a timely shield/heal, pull with Death's Grasp, and reserve Realm of Death for a safe single-target commitment.";
    p.ResearchSummary =
        "Riot live 26.15 / CommunityDragon 16.15 Mordekaiser passive aura, isolated Q, W stored-damage conversion, E pull geometry and seven-second Realm of Death tracking.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Obliterate", CastKind::Position,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit,
        AllModes, 675.0f, 0.50f, 172.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].WeaveAfterAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Indestructible", CastKind::Self,
        Intent::Shield | Intent::Heal | Intent::Buff | Intent::Damage,
        AllModes, 125.0f, 0.05f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 88;
    p.Spells[1].RecastSpellName = "MordekaiserWRecast";
    p.Spells[1].PreserveAutoAttack = false;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Death's Grasp", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Peel |
            Intent::Interrupt | Intent::Waveclear | Intent::Jungle,
        AllModes, 700.0f, 0.25f, 200.0f, 3000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCone);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 96;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Realm of Death", CastKind::EnemyTarget,
        Intent::CrowdControl | Intent::Engage | Intent::Disengage | Intent::Recast |
            Intent::Channel,
        Mode::Combo | Mode::Flee | Mode::Automatic, 650.0f, 0.50f, 0.0f,
        FLT_MAX, false, SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].TargetHealthPercent = 60.0f;
    p.Spells[3].RecastSpellName = "MordekaiserR";

    p.Trade = Plan("Death's Grasp into isolated Obliterate",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireNoCrowdControl, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 100, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow, 220, 1400));
    p.AllIn = Plan("Realm isolation and Darkness Rise",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 110, 1200),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow, 260, 1500),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireTargetLow |
             StepRule::RequireSafePosition | StepRule::HoldForExecute, 420, 1800));
    p.Flee = Plan("Indestructible peel and Death's Grasp",
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 80, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
