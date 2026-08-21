#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile DrMundo = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::DrMundo;
    p.DisplayName = "Dr. Mundo";
    p.InternalId = "champion.kuroaio.ai.drmundo";
    p.PrimaryArchetype = Archetype::Juggernaut;
    p.Resource = ResourceModel::Health;
    p.Mechanics = Mechanic::Recast | Mechanic::ObjectTracking |
                  Mechanic::MissingHealth | Mechanic::AutoReset;
    p.Ultimate = UltimatePolicy::Defensive;
    p.PreferredCombatDistance = 180.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 70;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "DrMundoP";
    p.MarkBuff = "DrMundoPCooldown";
    p.ChannelBuff = "DrMundoW";
    p.UltimateBuff = "DrMundoR";
    p.TacticalSummary =
        "Health-cost juggernaut: land percent-current-health cleavers, keep W's"
        " burn/recast state reconciled, empower close attacks with missing-health E"
        " and reserve regeneration R for safe anti-grievous survival windows.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 values: Q 1050 range, 60 width and"
        " 17.5-27.5% current-health damage with health refund; W spends 8% current"
        " health for a 3-second burn and recasts at 325; E spends 10-70 health for"
        " missing-health-amplified damage; R restores missing health over 10 seconds.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Infected Bonesaw", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 1050.0f, 0.25f, 60.0f, 1500.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 95;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].HarassManaPercent = 0.0f;
    p.Spells[0].ClearManaPercent = 0.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Heart Zapper", CastKind::Toggle,
        Intent::Damage | Intent::Heal | Intent::Recast | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        325.0f, 0.0f, 325.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 88;
    p.Spells[1].RecastSpellName = "DrMundoWRecast";
    p.Spells[1].PreserveAutoAttack = false;
    p.Spells[1].ComboManaPercent = 12.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Blunt Force Trauma", CastKind::Self,
        Intent::Damage | Intent::AutoReset | Intent::Buff,
        AllModes, 180.0f, 1.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 92;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = false;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Maximum Dosage", CastKind::Self,
        Intent::Heal | Intent::Buff | Intent::Disengage | Intent::Engage | Intent::Peel,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        0.0f, 0.25f, 600.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].PlayerHealthPercent = 56.0f;
    p.Spells[3].MinimumAoeTargets = 1;

    p.Trade = Plan("Cleaver into empowered attack",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireInsideAaRange | StepRule::AllowDuringWindup, 120, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 180, 900));
    p.AllIn = Plan("Maximum dosage juggernaut",
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow | StepRule::RequireSafePosition, 0, 1200),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 80, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireInsideAaRange | StepRule::AllowDuringWindup, 180, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 260, 1000));
    p.Flee = Plan("Regeneration and cleaver peel",
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow | StepRule::RequireSafePosition, 0, 1000),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 90, 900),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow, 150, 700));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
