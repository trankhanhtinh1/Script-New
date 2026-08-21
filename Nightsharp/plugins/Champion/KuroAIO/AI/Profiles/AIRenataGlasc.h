#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile RenataGlasc = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::RenataGlasc;
    p.DisplayName = "Renata Glasc";
    p.InternalId = "champion.kuroaio.ai.renataglasc";
    p.PrimaryArchetype = Archetype::Enchanter;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Mark | Mechanic::Recast | Mechanic::AllyTarget |
                  Mechanic::Revive | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 36.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 70;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "RenataPassiveDebuff";
    p.UltimateBuff = "RenataR";
    p.TrackedObjectToken = "RenataQMissile";
    p.TacticalSummary =
        "Enchanter-controller loop: mark champions with Leverage autos, handshake and reposition a committed target, "
        "bail out a carry through its revive window, shield allies while damaging enemies with Loyalty Program, and "
        "reserve Hostile Takeover for a safe hostile line or defensive peel.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 baseline. Q is a 900-range collision line with a target throw recast, "
        "W is a five-second ally/self steroid with a three-second takedown revive window, E is an 800-range shield "
        "and missile, and R is a wide 2000-range hostile-only berserk projectile.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Handshake", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Disengage |
            Intent::Recast | Intent::Peel | Intent::Interrupt,
        AllModes, 900.0f, 0.25f, 70.0f, 1450.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 94;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].RecastSpellName = "Handshake throw";

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Bailout", CastKind::AllyTarget,
        Intent::Buff | Intent::Heal | Intent::AllyUtility |
            Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        800.0f, 0.10f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 100;
    p.Spells[1].RecastSpellName = "Bailout revive window";

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Loyalty Program", CastKind::Direction,
        Intent::Damage | Intent::Shield | Intent::AllyUtility | Intent::Peel |
            Intent::AntiGapcloser | Intent::Setup,
        AllModes, 800.0f, 0.25f, 100.0f, 1500.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[2].Priority = 88;
    p.Spells[2].MinimumAoeTargets = 1;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Hostile Takeover", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Peel | Intent::Interrupt | Intent::Finisher,
        Mode::Combo | Mode::Flee | Mode::Automatic, 2000.0f, 0.75f, 250.0f, 650.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;

    p.Trade = Plan("Mark and handshake",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireNoCrowdControl, 80, 1050),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 150, 1100));
    p.AllIn = Plan("Hostile takeover escort",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 60, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 130, 1050),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireTarget, 200, 1600));
    p.Flee = Plan("Bailout peel",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 60, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 120, 950),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget, 180, 1400));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
