#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Viego = [] {
    ChampionProfile p{};
    p.ChampionName = "Viego";
    p.DisplayName = "Viego";
    p.InternalId = "champion.kuroaio.ai.viego";
    p.PrimaryArchetype = Archetype::Skirmisher;
    p.Resource = ResourceModel::None;
    p.Mechanics = Mechanic::Possession | Mechanic::Transform | Mechanic::MultiForm |
                  Mechanic::Charge | Mechanic::Channel | Mechanic::Dash |
                  Mechanic::Blink | Mechanic::Recast | Mechanic::Execute |
                  Mechanic::Mark | Mechanic::AutoWeave | Mechanic::AutoReset |
                  Mechanic::WallInteraction | Mechanic::Terrain |
                  Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 375.0f;
    p.EngageHealthPercent = 45.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 35.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ViegoPassiveTransform";
    p.MarkBuff = "ViegoQMark";
    p.ChannelBuff = "ViegoW";
    p.FormBuff = "ViegoPassiveTransform";
    p.UltimateBuff = "ViegoR";
    p.TrackedObjectToken = "ViegoPassiveSoul";
    p.ThemeFrom = 0xFF45D7A4u;
    p.ThemeTo = 0xFF4A78FFu;
    p.TacticalSummary =
        "Mark-and-auto skirmisher: protect Q passive double strikes, charge W only "
        "through a safe dash, seed wall mist, and use Heartbreaker as an execute "
        "or controlled exit from possession.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift data; possession is "
        "treated as a foreign-kit ownership boundary rather than a fifth spell form.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Blade of the Ruined King", CastKind::Line,
        Intent::Damage | Intent::Heal | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        600.0f, 0.25f, 125.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Spectral Maw", CastKind::ChargedLine,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Peel | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Channel | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        900.0f, 0.20f, 60.0f, 1300.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 88;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].DashDistance = 300.0f;
    p.Spells[1].MaximumEnemiesAtDestination = 2;
    p.Spells[1].ChargeBuffName = "ViegoW";
    p.Spells[1].ChargeMinRange = 500;
    p.Spells[1].ChargeMaxRange = 900;
    p.Spells[1].ChargeDurationSeconds = 1.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Harrowed Path", CastKind::Direction,
        Intent::Buff | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Vision | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee,
        750.0f, 0.0f, 120.0f, 1200.0f, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[2].Priority = 76;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Heartbreaker", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Disengage | Intent::Execute | Intent::Recast |
            Intent::Objective,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        500.0f, 0.50f, 300.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 99;
    p.Spells[3].Hitchance = SDK::HitChance::High;
    p.Spells[3].TargetHealthPercent = 35.0f;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].RecastSpellName = "ViegoR";

    p.Variants[0] = { SDK::SpellSlot::W, "ViegoW", p.Spells[1] };
    p.Variants[1] = { SDK::SpellSlot::R, "ViegoR", p.Spells[3] };
    p.VariantCount = 2;

    p.Trade = Plan("Marked double-strike trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 0, 650),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 180, 1200));
    p.AllIn = Plan("Mist maw execution",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 600),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 80, 1500),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 250, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::HoldForExecute, 450, 1800));
    p.Flee = Plan("Mist and maw retreat",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 650),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 80, 1450),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 250, 1400));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
