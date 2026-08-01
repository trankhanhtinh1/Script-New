#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Trundle = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Trundle;
    p.DisplayName = "Trundle";
    p.InternalId = "champion.kuroaio.ai.trundle";
    p.PrimaryArchetype = Archetype::Juggernaut;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AutoReset | Mechanic::Terrain | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::Defensive;
    p.PreferredCombatDistance = 250.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 65.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 45;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "trundlepdef";
    p.TrackedObjectToken = "TrundlePillar";
    p.TacticalSummary =
        "Juggernaut controller that resets an attack with Q, contests the target inside W, "
        "uses E only for verified displacement or terrain denial, and reserves R's stat drain "
        "for a reachable primary target or a defensive low-health turn.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 spell values, Trundle buff identities, "
        "pillar collision and conservative target policy reconciled through events and polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Chomp", CastKind::EnemyTarget,
        Intent::Damage | Intent::Buff | Intent::AutoReset | Intent::Setup |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        300.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 96;
    p.Spells[0].TriggerRange = 300.0f;
    p.Spells[0].DesiredDistance = 125.0f;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = false;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Frozen Domain", CastKind::Position,
        Intent::Buff | Intent::Damage | Intent::Engage | Intent::Disengage |
            Intent::Waveclear | Intent::Jungle | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        750.0f, 0.25f, 720.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[1].Priority = 90;
    p.Spells[1].TriggerRange = 750.0f;
    p.Spells[1].HarassManaPercent = 48.0f;
    p.Spells[1].ClearManaPercent = 42.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Pillar of Ice", CastKind::Position,
        Intent::CrowdControl | Intent::Disengage | Intent::Engage | Intent::Peel |
            Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1000.0f, 0.25f, 450.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::BehindTarget;
    p.Spells[2].Priority = 92;
    p.Spells[2].TriggerRange = 1000.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Subjugate", CastKind::EnemyTarget,
        Intent::Damage | Intent::Buff | Intent::Finisher |
            Intent::Heal | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        650.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].TargetHealthPercent = 65.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("Q bite inside Frozen Domain",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition));
    p.AllIn = Plan("Domain, bite, pillar displacement and stat drain",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition));
    p.Flee = Plan("Pillar peel and defensive domain",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
