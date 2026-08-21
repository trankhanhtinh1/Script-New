#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Zac = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Zac;
    p.DisplayName = "Zac";
    p.InternalId = "champion.kuroaio.ai.zac";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Health;
    p.Mechanics = Mechanic::Recast | Mechanic::Charge | Mechanic::Dash |
                  Mechanic::ObjectTracking | Mechanic::MissingHealth | Mechanic::Terrain |
                  Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 35.0f;
    p.UltimateTargetHealthPercent = 65.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 70;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ZacPassive";
    p.MarkBuff = "ZacQDebuff";
    p.ChannelBuff = "ZacEPrepare";
    p.UltimateBuff = "ZacR";
    p.TacticalSummary =
        "Cell Division tank: collect blobs, pair Stretching Strikes arms, spend health for "
        "Unstable Matter, charge safe Elastic Slingshot landings, and bounce through grouped enemies.";
    p.ResearchSummary =
        "Riot live 26.15 / CommunityDragon 16.15: passive blob cells, Q two-target slam, "
        "W current-health cost, E charge endpoint and R bounce/carry safety are modeled.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Stretching Strikes", CastKind::AnyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Recast | Intent::AutoReset,
        AllModes, 800.0f, 0.25f, 120.0f, 1500.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 95;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].RecastSpellName = "ZacQ";

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Unstable Matter", CastKind::Self,
        Intent::Damage | Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::Automatic,
        350.0f, 0.1f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 82;
    p.Spells[1].PlayerHealthPercent = 20.0f;
    p.Spells[1].ComboManaPercent = 4.0f;
    p.Spells[1].ClearManaPercent = 12.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Elastic Slingshot", CastKind::ChargedCircle,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Automatic | Mode::Flee,
        1800.0f, 0.0f, 275.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 100;
    p.Spells[2].PreserveAutoAttack = false;
    p.Spells[2].MaximumEnemiesAtDestination = 3;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Let's Bounce!", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::Finisher,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        300.0f, 0.01f, 300.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan("Stretching Strikes pair into Unstable Matter",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireFirstCast, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireRecast, 90, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequireMultiTarget | StepRule::AllowDuringWindup, 140, 700));
    p.AllIn = Plan("Elastic Slingshot into a safe bounce carry",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1500),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireFirstCast, 100, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequireMultiTarget | StepRule::AllowDuringWindup, 180, 700),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireSafePosition, 240, 1500));
    p.Flee = Plan("Elastic Slingshot escape and defensive bounce",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 1500),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow | StepRule::RequireSafePosition, 100, 1300));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
