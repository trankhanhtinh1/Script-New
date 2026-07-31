#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Morgana = [] {
    ChampionProfile p{};
    p.ChampionName = "Morgana";
    p.DisplayName = "Morgana";
    p.InternalId = "champion.kuroaio.ai.morgana";
    p.PrimaryArchetype = Archetype::Catcher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AllyTarget | Mechanic::SpellShield |
                  Mechanic::MissingHealth | Mechanic::Tether |
                  Mechanic::Recast | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 45.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "MorganaPassive";
    p.UltimateBuff = "MorganaR";
    p.TrackedObjectToken = "MorganaTormentedShadow";
    p.TacticalSummary =
        "Catcher-support loop: bind through collision, place missing-health Tormented Shadow, match Black Shield to an ally's incoming disabling threat, and hold Soul Shackles until tethered enemies can remain for the stun.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 baseline. Q is a 1250-range collision line, W is a 900-range missing-health zone, E is an 800-range anti-CC magic shield, and R chains nearby enemies for a delayed stun.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Dark Binding", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Finisher,
        AllModes, 1250.0f, 0.25f, 70.0f, 1200.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 95;
    p.Spells[0].Hitchance = SDK::HitChance::High;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Tormented Shadow", CastKind::Circle,
        Intent::Damage | Intent::Waveclear | Intent::Jungle | Intent::Setup,
        AllModes, 900.0f, 0.25f, 325.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::BestAoe;
    p.Spells[1].Priority = 68;
    p.Spells[1].MinimumAoeTargets = 1;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Black Shield", CastKind::AllyTarget,
        Intent::Shield | Intent::Buff | Intent::Cleanse | Intent::Peel |
            Intent::AllyUtility | Intent::AntiGapcloser,
        AllModes, 800.0f, 0.10f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 100;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Soul Shackles", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Peel | Intent::Recast | Intent::Finisher,
        Mode::Combo | Mode::Flee | Mode::Automatic, 625.0f, 0.25f,
        625.0f, FLT_MAX, false, SDK::DamageType::Magical,
        SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].RecastSpellName = "Soul Shackles stun tether";

    p.Trade = Plan("Binding into shadow",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 800),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireNoCrowdControl, 90, 950),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 160, 1000));
    p.AllIn = Plan("Shackles tether",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 60, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 130, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireTarget, 190, 1500));
    p.Flee = Plan("Black Shield retreat",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::RequireNoCrowdControl, 0, 600),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 60, 800),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget, 120, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
