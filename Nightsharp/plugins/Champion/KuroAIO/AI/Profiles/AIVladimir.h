#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Vladimir = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Vladimir;
    p.DisplayName = "Vladimir";
    p.InternalId = "champion.kuroaio.ai.vladimir";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Health;
    p.Mechanics = Mechanic::Charge | Mechanic::Mark | Mechanic::MissingHealth |
                  Mechanic::Channel | Mechanic::Recast | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 500.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 24.0f;
    p.UltimateTargetHealthPercent = 58.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 58;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "VladimirBloodGorged";
    p.MarkBuff = "VladimirHemoplagueDebuff";
    p.ChannelBuff = "VladimirE";
    p.UltimateBuff = "VladimirHemoplague";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Transfusion", CastKind::EnemyTarget,
        Intent::Damage | Intent::Heal | Intent::Setup | Intent::LastHit,
        AllModes, 600.0f, 0.25f, 0.0f, 1400.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].HarassManaPercent = 0.0f;
    p.Spells[0].ClearManaPercent = 0.0f;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].RequiredTargetBuff = "";

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Sanguine Pool", CastKind::Self,
        Intent::Disengage | Intent::Peel | Intent::Shield | Intent::Heal |
            Intent::AntiGapcloser,
        Mode::Combo | Mode::Flee | Mode::Automatic, 350.0f, 0.0f, 350.0f,
        FLT_MAX, false, SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::AwayFromThreat;
    p.Spells[1].Priority = 100;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].PlayerHealthPercent = 26.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Tides of Blood", CastKind::ChargedCircle,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic, 600.0f, 1.0f, 550.0f,
        FLT_MAX, false, SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 86;
    p.Spells[2].ChargeBuffName = "VladimirE";
    p.Spells[2].ChargeMinRange = 0;
    p.Spells[2].ChargeMaxRange = 600;
    p.Spells[2].ChargeDurationSeconds = 1.0f;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].PlayerHealthPercent = 18.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Hemoplague", CastKind::Circle,
        Intent::Damage | Intent::Mark | Intent::Heal | Intent::Engage,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic, 625.0f,
        0.3889f, 375.0f, 1200.0f, false, SDK::DamageType::Magical,
        SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].TargetHealthPercent = 58.0f;
    p.Spells[3].PlayerHealthPercent = 30.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("empowered Q sustain, short E release, save Pool for threat",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition,
             70, 1050));
    p.AllIn = Plan("Hemoplague mark, charged E and empowered Q finish",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget |
             StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireMark |
             StepRule::RequireSafePosition, 120, 1100),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireMark |
             StepRule::AllowDuringWindup, 220, 1300),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup,
             300, 1400));
    p.Flee = Plan("Pool lethal threat, then low-cost Q sustain and E peel",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup,
             120, 800),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition,
             180, 1000));

    p.TacticalSummary =
        "Vladimir spends health only when the post-cost floor is safe: Transfusion is "
        "cycled for sustain and empowered Q is held for a real target, Tides of Blood "
        "charges before release, Sanguine Pool denies committed threats, and Hemoplague "
        "marks a grouped or killable target before burst.";
    p.ResearchSummary =
        "CommunityDragon 16.15 VladimirQ/W/E/R metadata and Riot 26.15 baseline: health "
        "resource tradeoffs, Q Frenzy empowerment, W untargetability, one-second E charge, "
        "and four-second Hemoplague amplification/heal window.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
