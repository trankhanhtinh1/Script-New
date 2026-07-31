#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Gragas = [] {
    ChampionProfile p{};
    p.ChampionName = "Gragas";
    p.DisplayName = "Gragas";
    p.InternalId = "champion.kuroaio.ai.gragas";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::Recast | Mechanic::Dash |
                  Mechanic::WallInteraction | Mechanic::MissingHealth |
                  Mechanic::AutoWeave | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 460.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 58;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "GragasPassive";
    p.ChannelBuff = "GragasQ";
    p.MarkBuff = "GragasWAttack";
    p.UltimateBuff = "GragasR";
    p.TacticalSummary =
        "Barrel-control vanguard: charge Q for area denial, weave W's empowered attack, collide E only through a safe endpoint, and use R to displace or kill-secure without throwing a protected target into danger.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift spell data, passive cooldown and missing-health sustain, with cast and barrel state reconciled from events and polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Barrel Roll", CastKind::ChargedCircle,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Waveclear | Intent::Jungle,
        AllModes, 850.0f, 0.25f, 250.0f, 1000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 90;
    p.Spells[0].ChargeBuffName = "GragasQ";
    p.Spells[0].ChargeMinRange = 150;
    p.Spells[0].ChargeMaxRange = 850;
    p.Spells[0].ChargeDurationSeconds = 4.0f;
    p.Spells[0].RecastSpellName = "Barrel Roll";

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Drunken Rage", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::AutoReset | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        50.0f, 0.10f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 84;
    p.Spells[1].WeaveAfterAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Body Slam", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Disengage | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Peel,
        AllModes, 600.0f, 0.0f, 180.0f, 900.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 96;
    p.Spells[2].DashDistance = 600.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Explosive Cask", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Disengage |
            Intent::Engage | Intent::Execute | Intent::Interrupt |
            Intent::Setup | Intent::Finisher,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1000.0f, 0.25f, 400.0f, 1000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;

    p.Trade = Plan("Barrel and empowered trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 150, 700));
    p.AllIn = Plan("Body Slam cask displacement",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 650),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 120, 700),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireSafePosition, 250, 1600));
    p.Flee = Plan("Cask peel and safe Body Slam",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1200),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 90, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 160, 700));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
