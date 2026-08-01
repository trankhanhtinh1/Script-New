#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Annie is modeled around Pyromania's landing-order race, not around a static
// "has stun" flag.  Q grants its stack on missile impact while W/E/R grant on
// cast, so the controller can conceal a stun at three stacks, select whether
// Q/W/R consumes it, and avoid a flying Q stealing an intended AoE stun.
inline constexpr ChampionProfile Annie = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Annie;
    p.DisplayName = "Annie";
    p.InternalId = "champion.kuroaio.ai.annie";
    p.PrimaryArchetype = Archetype::BurstMage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Pet |
                  Mechanic::ObjectTracking | Mechanic::AllyTarget |
                  Mechanic::AutoWeave | Mechanic::Recast;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Disintegrate", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Finisher |
            Intent::LastHit | Intent::Interrupt | Intent::AntiGapcloser |
            Intent::Peel | Intent::Setup | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::LastHit |
            Mode::Jungle | Mode::Flee | Mode::Automatic,
        625.0f, 0.25f, 0.0f, 1400.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[0].Priority = 94;
    p.Spells[0].HarassManaPercent = 32.0f;
    p.Spells[0].ClearManaPercent = 18.0f;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].WeaveAfterAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Incinerate", CastKind::Cone,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Peel | Intent::Setup |
            Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        600.0f, 0.25f, 49.52f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCone);
    p.Spells[1].Priority = 92;
    p.Spells[1].MinimumAoeTargets = 2;
    p.Spells[1].HarassManaPercent = 52.0f;
    p.Spells[1].ClearManaPercent = 62.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Molten Shield", CastKind::AllyTarget,
        Intent::Shield | Intent::Buff | Intent::AllyUtility |
            Intent::Damage | Intent::Engage | Intent::Disengage |
            Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        800.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 90;
    p.Spells[2].HarassManaPercent = 38.0f;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Summon: Tibbers", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::Setup |
            Intent::Vision | Intent::Objective | Intent::Recast,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        600.0f, 0.25f, 250.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 98;
    p.Spells[3].TriggerRange = 250.0f;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "conceal third stack with E, land Q stun, W only when its cone adds value",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireCrowdControl));

    p.AllIn = Plan(
        "choose AoE R/W or point-click Q as stun consumer, then burst and enrage Tibbers",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireCrowdControl),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireCrowdControl),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.Flee = Plan(
        "speed the endangered ally, stop the committed pursuer, preserve AoE peel",
        Step(SDK::SpellSlot::E,
             StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 590.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 72.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 34;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "pyromania_particle";
    p.MarkBuff = "pyromania";
    p.UltimateBuff = "AnnieRController";
    p.TrackedObjectToken = "annietibbers";
    p.ThemeFrom = 0xFFFF9A42u;
    p.ThemeTo = 0xFFE94831u;
    p.ThemeSpeed = 0.86f;
    p.TacticalSummary =
        "Track Pyromania by cast and Q-impact events, hide the fourth stack "
        "behind E, reserve the stun for the best Q/W/R landing, use W from "
        "its cast-end origin so player Flash buffers remain valid, shield "
        "the correct ally without stealing an intended stack sequence, and "
        "micro Tibbers through enrage, aura contact, turret and leash states.";
    p.ResearchSummary =
        "Pinned to Riot 26.4/25.18/25.8 and CommunityDragon 16.14. Cross-"
        "checked current Challenger Annie trading, Faker and EUW Challenger "
        "replays, combo catalogs, AnnieMains landing-order mechanics and both "
        "local OneKeyToWin implementations; rejected their obsolete ranges, "
        "damage and static has-stun priority lists.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
