#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile KSante = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::KSante;
    p.DisplayName = "K'Sante";
    p.InternalId = "champion.kuroaio.ai.ksante";
    p.PrimaryArchetype = Archetype::Tank;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stance | Mechanic::MultiForm | Mechanic::Charge |
        Mechanic::Stack | Mechanic::Dash | Mechanic::AllyTarget |
        Mechanic::Terrain | Mechanic::Recast | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "KSantePMark";
    p.ChannelBuff = "KSanteW";
    p.FormBuff = "KsanteRTransform";
    p.UltimateBuff = "KsanteRTransform";
    p.TacticalSummary =
        "Front-line displacement tank that builds Q3, owns W charge and release, "
        "uses ally/self E only through safe endpoints, and commits All Out only "
        "when R genuinely isolates a target.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15; Summoner's Rift baseline.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Ntofo Strikes", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 450.0f, 0.35f, 120.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 86;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Path Maker", CastKind::ChargedLine,
        Intent::Damage | Intent::CrowdControl | Intent::Channel |
            Intent::Disengage | Intent::Interrupt | Intent::Peel,
        Mode::Combo | Mode::Flee | Mode::Automatic, 600.0f, 0.0f, 110.0f,
        1500.0f, false, SDK::DamageType::Physical,
        SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 94;
    p.Spells[1].ChargeBuffName = "KSanteW";
    p.Spells[1].ChargeMinRange = 100;
    p.Spells[1].ChargeMaxRange = 600;
    p.Spells[1].ChargeDurationSeconds = 1.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Footwork", CastKind::AnyTarget,
        Intent::Mobility | Intent::Shield | Intent::AllyUtility |
            Intent::Disengage | Intent::Peel,
        AllModes, 550.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 82;
    p.Spells[2].DashDistance = 250.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "All Out", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Recast,
        Mode::Combo | Mode::Automatic, 350.0f, 0.4f, 160.0f, FLT_MAX,
        false, SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::BehindTarget;
    p.Spells[3].Priority = 90;
    p.Spells[3].TargetHealthPercent = 60.0f;
    p.Spells[3].MaximumEnemiesAtDestination = 2;

    p.Variants[0] = { SDK::SpellSlot::Q, "KSanteQ3", p.Spells[0] };
    p.Variants[0].Spec.Name = "Ntofo Strikes (Q3)";
    p.Variants[0].Spec.Range = 825.0f;
    p.Variants[0].Spec.TriggerRange = 825.0f;
    p.Variants[0].Spec.Delay = 0.45f;
    p.Variants[0].Spec.Speed = 1800.0f;
    p.Variants[0].Spec.Collision = true;
    p.Variants[1] = { SDK::SpellSlot::W, "KSanteW_AllOut", p.Spells[1] };
    p.Variants[1].Spec.Name = "Path Maker (All Out)";
    p.Variants[1].Spec.ChargeMinRange = 100;
    p.Variants[1].Spec.ChargeMaxRange = 600;
    p.Variants[2] = { SDK::SpellSlot::E, "KSanteESelfDash", p.Spells[2] };
    p.Variants[2].Spec.Kind = CastKind::Position;
    p.Variants[2].Spec.Range = 250.0f;
    p.Variants[2].Spec.TriggerRange = 250.0f;
    p.Variants[3] = { SDK::SpellSlot::E, "KSanteEAllyDash", p.Spells[2] };
    p.Variants[3].Spec.Kind = CastKind::AllyTarget;
    p.Variants[3].Spec.Range = 550.0f;
    p.Variants[3].Spec.TriggerRange = 550.0f;
    p.Variants[4] = { SDK::SpellSlot::R, "KSanteREndEarly", p.Spells[3] };
    p.Variants[4].Spec.Name = "All Out (End Early)";
    p.Variants[4].Spec.Kind = CastKind::Self;
    p.Variants[4].Spec.Intents = Intent::Recast | Intent::Disengage;
    p.VariantCount = 5;

    p.Trade = Plan("Q pressure and retreat",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 120, 950));
    p.AllIn = Plan("Isolate and All Out",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 650),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition, 80, 1200),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition, 160, 1400),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 350, 1700));
    p.Flee = Plan("Protected retreat",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 650),
        Step(SDK::SpellSlot::W,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 80, 1200));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
