#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Kassadin = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Kassadin;
    p.DisplayName = "Kassadin";
    p.InternalId = "champion.kuroaio.ai.kassadin";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Blink | Mechanic::Stack | Mechanic::SpellShield |
                  Mechanic::AutoWeave | Mechanic::AutoReset | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 180.0f;
    p.EngageHealthPercent = 45.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 52;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "VoidStone";
    p.MarkBuff = "NetherBladeBuff";
    p.ChannelBuff = "ForcePulseAvailable";
    p.UltimateBuff = "RiftWalk";
    p.TacticalSummary =
        "Mana-scaling assassin: use Null Sphere's magic shield against incoming magic, weave a Nether Blade reset, charge Force Pulse before spending it, and reserve a safe Riftwalk endpoint and enough mana for escape.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Kassadin spell data: six-cast Force Pulse charge gate, 15-second four-stack Riftwalk escalation, max-mana cost/damage, Nether Blade reset, and Null Sphere 1.5-second magic shield.";

p.Spells[0] = Spell(SDK::SpellSlot::Q, "Null Sphere", CastKind::EnemyTarget,
        Intent::Damage | Intent::Shield | Intent::Setup | Intent::Interrupt,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        650.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 84;
    p.Spells[0].Collision = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Nether Blade", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::AutoReset | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        125.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 96;
    p.Spells[1].WeaveAfterAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Force Pulse", CastKind::Cone,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        600.0f, 0.25f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCone);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 92;
    p.Spells[2].MinimumAoeTargets = 1;
    p.Spells[2].ChargeBuffName = "ForcePulseAvailable";
    p.Spells[2].ChargeMinRange = 6;
    p.Spells[2].ChargeMaxRange = 6;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Riftwalk", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Execute | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        500.0f, 0.25f, 270.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SafeCursor;
    p.Spells[3].Priority = 100;
    p.Spells[3].DashDistance = 500.0f;
    p.Spells[3].MaximumEnemiesAtDestination = 2;

    p.Trade = Plan("Shielded Nether Blade trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireInsideAaRange | StepRule::AllowDuringWindup, 70, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireCrowdControl, 100, 800));
    p.AllIn = Plan("Charged Force Pulse Riftwalk all-in",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireInsideAaRange | StepRule::AllowDuringWindup, 70, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireCrowdControl, 100, 850),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 180, 1200));
    p.Flee = Plan("Safe Riftwalk retreat",
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequirePlayerLow, 80, 700));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
