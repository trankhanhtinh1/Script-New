#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Karthus = [] {
    ChampionProfile p{};
    p.ChampionName = "Karthus";
    p.DisplayName = "Karthus";
    p.InternalId = "champion.kuroaio.ai.karthus";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Channel | Mechanic::Recast | Mechanic::Global |
                  Mechanic::Execute | Mechanic::Revive | Mechanic::ObjectTracking;
    p.Ultimate = UltimatePolicy::GlobalExecute;
    p.PreferredCombatDistance = 700.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 38.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 60;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "KarthusDeathDefied";
    p.ChannelBuff = "KarthusFallenOne";
    p.UltimateBuff = "KarthusFallenOne";
    p.TacticalSummary =
        "Isolated Lay Waste doubles its damage; Wall of Pain opens a magic-"
        "resistance shred and slow corridor; Defile is a contact-driven toggle "
        "with a strict mana reserve; Requiem is a protected global channel that "
        "remains usable during Death Defied's seven-second cast window.";
    p.ResearchSummary =
        "Pinned to CommunityDragon PC 16.15 and Riot patch 26.15. Q uses the "
        "current 40/59/78/97/116 plus 35 percent AP single-target double hit, "
        "W uses rank-dependent wall width/slow with 25 percent MR reduction, "
        "E uses 30/42/54/66/78 mana per second and 10/30/50/70/90/110 DPS, "
        "and R is a 3-second 200/350/500 plus 70 percent AP global channel; "
        "Death Defied lasts seven seconds on Summoner's Rift.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Lay Waste", CastKind::Circle,
        Intent::Damage | Intent::Finisher | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 875.0f, 0.25f, 160.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 94;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 34.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Wall of Pain", CastKind::Position,
        Intent::CrowdControl | Intent::Setup | Intent::Peel |
            Intent::AntiGapcloser | Intent::Damage,
        AllModes, 1000.0f, 0.25f, 200.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[1].Priority = 88;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 58.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Defile", CastKind::Toggle,
        Intent::Damage | Intent::Recast | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit | Intent::Peel,
        AllModes, 550.0f, 0.25f, 550.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 90;
    p.Spells[2].PreserveAutoAttack = false;
    p.Spells[2].ComboManaPercent = 28.0f;
    p.Spells[2].ClearManaPercent = 30.0f;
    p.Spells[2].RecastSpellName = "KarthusDefile";

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Requiem", CastKind::Self,
        Intent::Damage | Intent::Execute | Intent::Finisher |
            Intent::Channel | Intent::Global | Intent::Interrupt,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        10000.0f, 3.0f, 1000.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].TargetHealthPercent = 38.0f;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].RecastSpellName = "KarthusFallenOne";

    p.Trade = Plan(
        "Wall corridor into isolated Lay Waste while Defile is affordable",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 850),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireNoCrowdControl, 180, 1000),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 260, 1200));
    p.AllIn = Plan(
        "Wall first, Q double hits, contact Defile and a safe Requiem finish",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 150, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 260, 1400),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition |
             StepRule::HoldForExecute, 500, 2800));
    p.Flee = Plan(
        "Wall peel and reserve Defile; Requiem only for an exact interrupt or execute",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition |
             StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 120, 900),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition |
             StepRule::ManualAssistOnly, 220, 3000));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
