#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Locke = [] {
    ChampionProfile p{};
    p.ChampionName = "Locke";
    p.DisplayName = "Locke";
    p.InternalId = "champion.kuroaio.ai.locke";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::Blink |
                  Mechanic::Execute | Mechanic::Mark | Mechanic::MissingHealth |
                  Mechanic::AutoWeave | Mechanic::ObjectTracking;
    p.Ultimate = UltimatePolicy::Execute;
    p.PreferredCombatDistance = 375.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 38.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 60;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "LockePassive";
    p.MarkBuff = "LockeQMark";
    p.ChannelBuff = "LockeW";
    p.UltimateBuff = "LockeRMark";
    p.TrackedObjectToken = "LockeArtifact";
    p.TacticalSummary =
        "Soul-Nail burst assassin: mark with Q, weave an empowered attack, "
        "use W only when its self-drain is affordable, blink with E before a "
        "targeted dash, and reserve Purgatory for a verified execution window.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 champion id 805. Numbers exposed "
        "by the live payload are conservative where tooltip placeholders hide "
        "rank values; event state is reconciled with polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Ritual Nails", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        AllModes, 950.0f, 0.25f, 64.0f, 1800.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].Hitchance = SDK::HitChance::High;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Soul Ignition", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::Heal | Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        250.0f, 0.0f, 0.0f, 0.0f, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 72;
    p.Spells[1].RequiredPlayerBuff = "";
    p.Spells[1].RecastSpellName = "LockeW2";

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Ashen Pursuit", CastKind::Position,
        Intent::Mobility | Intent::Damage | Intent::Engage | Intent::Disengage |
            Intent::Peel,
        AllModes, 425.0f, 0.0f, 250.0f, 0.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 96;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Purgatory", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Execute | Intent::Finisher |
            Intent::Objective | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic, 1000.0f, 0.50f, 300.0f,
        1200.0f, false, SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 1;

    p.Trade = Plan("Nails and ignition trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireNoMark, 0, 800),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 120, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 180, 1000));
    p.AllIn = Plan("Marked pursuit and execution",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 100, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 180, 1000),
        Step(SDK::SpellSlot::R, StepRule::RequireTargetLow | StepRule::HoldForExecute, 280, 1300));
    p.Flee = Plan("Ashen retreat",
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow | StepRule::RequireSafePosition, 0, 600),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 60, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 120, 850));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
