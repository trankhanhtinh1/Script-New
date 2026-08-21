#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Velkoz = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Velkoz;
    p.DisplayName = "Vel'Koz";
    p.InternalId = "champion.kuroaio.ai.velkoz";
    p.PrimaryArchetype = Archetype::Specialist;
    p.Mechanics = Mechanic::Stack | Mechanic::Mark | Mechanic::Channel |
                  Mechanic::WallInteraction | Mechanic::AutoWeave |
                  Mechanic::Recast;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 900.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 28.0f;
    p.UltimateTargetHealthPercent = 68.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "VelkozPassive";
    p.MarkBuff = "VelkozResearchProc";
    p.ChannelBuff = "VelkozR";
    p.UltimateBuff = "VelkozR";
    p.TrackedObjectToken = "VelkozQMissile";
    p.TacticalSummary =
        "Artillery loop: build Organic Deconstruction through Q/W/E hits, split Plasma Fission, chain both Void Rift stages, and reserve a terrain-safe true-damage ray for researched targets.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 baseline. Q is a 1050-range 50-width beam that splits at impact, W is a two-stage 1050-range line, E is an 800-range 225-radius knockup zone, and R is a 1550-range 2.5-second true-damage channel that researches targets.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Plasma Fission", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Interrupt,
        AllModes, 1050.0f, 0.25f, 50.0f, 1300.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].RecastSpellName = "VelkozQSplit";

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Void Rift", CastKind::Line,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit,
        AllModes, 1050.0f, 0.25f, 88.0f, 1700.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 78;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Tectonic Disruption", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Peel | Intent::AntiGapcloser |
            Intent::Setup | Intent::Interrupt,
        AllModes, 800.0f, 0.75f, 225.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 88;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Life Form Disintegration Ray", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Finisher | Intent::Execute |
            Intent::Channel | Intent::Interrupt,
        Mode::Combo | Mode::Flee | Mode::Automatic, 1550.0f, 0.25f, 90.0f,
        FLT_MAX, false, SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].Hitchance = SDK::HitChance::High;
    p.Spells[3].RequiredTargetBuff = "VelkozResearchProc";
    p.Spells[3].RecastSpellName = "Life Form Disintegration Ray channel";

    p.Trade = Plan("Research poke", Step(SDK::SpellSlot::Q,
        StepRule::RequireTarget, 0, 900), Step(SDK::SpellSlot::W,
        StepRule::RequireTarget, 80, 1000), Step(SDK::SpellSlot::E,
        StepRule::RequireTarget | StepRule::RequireSafePosition, 150, 1000));
    p.AllIn = Plan("Researched true-damage ray", Step(SDK::SpellSlot::E,
        StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 800),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 50, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 100, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition |
            StepRule::HoldForExecute, 200, 1800));
    p.Flee = Plan("Tectonic retreat", Step(SDK::SpellSlot::E,
        StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 60, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
