#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Kayn = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Kayn;
    p.DisplayName = "Kayn";
    p.InternalId = "champion.kuroaio.ai.kayn";
    p.PrimaryArchetype = Archetype::Skirmisher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Transform | Mechanic::MultiForm |
                  Mechanic::Dash | Mechanic::Mark | Mechanic::WallInteraction |
                  Mechanic::Terrain | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 350.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 42.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 45;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "KaynPassive";
    p.MarkBuff = "KaynREnemyMark";
    p.ChannelBuff = "KaynRHost";
    p.FormBuff = "KaynAssReady|KaynSlayReady";
    p.UltimateBuff = "KaynRHost";
    p.TacticalSummary =
        "Orb-aware Kayn: preserve attack windups, select Shadow Assassin or Darkin Slayer form, "
        "dash through Q, line knockup with W, wall-traverse E and enter/recast R only on a marked host.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon 16.15 Kayn Q/W/E/R values, KaynAssReady/KaynSlayReady "
        "form buffs, KaynTransforming transition and KaynREnemyMark/KaynRHost lifecycle are reconciled "
        "from spell, buff and polling observations.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Reaping Slash", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit | Intent::AutoReset,
        AllModes, 350.0f, 0.15f, 100.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[0].Priority = 90;
    p.Spells[0].DashDistance = 350.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Blade's Reach", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit | Intent::Interrupt | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        700.0f, 0.55f, 160.0f, 900.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 94;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Shadow Step", CastKind::Position,
        Intent::Mobility | Intent::Heal | Intent::Disengage | Intent::Engage |
            Intent::AntiGapcloser,
        AllModes, 400.0f, 0.10f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 76;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Umbral Trespass", CastKind::AnyTarget,
        Intent::Damage | Intent::Recast | Intent::Execute | Intent::Heal |
            Intent::Mobility | Intent::Disengage | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        550.0f, 0.10f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].MaximumEnemiesAtDestination = 2;

    p.Trade = Plan("W knockup and Q slash",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 850),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 130, 800),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 250, 1100));
    p.AllIn = Plan("Marked host form-aware all in",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireCrowdControl, 0, 850),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 100, 800),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMark | StepRule::RequireTargetLow, 230, 1500),
        Step(SDK::SpellSlot::R, StepRule::RequireRecast | StepRule::RequireSafePosition, 700, 2300));
    p.Flee = Plan("Wall traversal and host exit",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 900),
        Step(SDK::SpellSlot::R, StepRule::RequireRecast | StepRule::RequireSafePosition, 100, 1600));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
