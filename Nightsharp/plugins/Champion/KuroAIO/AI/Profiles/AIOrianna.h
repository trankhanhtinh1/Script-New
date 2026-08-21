#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Orianna = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Orianna;
    p.DisplayName = "Orianna";
    p.InternalId = "champion.kuroaio.ai.orianna";
    p.PrimaryArchetype = Archetype::BurstMage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::ObjectTracking | Mechanic::Pet |
                  Mechanic::AllyTarget | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 720.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 42.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 62;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.TrackedObjectToken = "OriannaBall";
    p.ThemeFrom = 0xFF70C8FFu;
    p.ThemeTo = 0xFFC4A7FFu;
    p.TacticalSummary = "Ball-state control mage: route Q/E from the observed Ball, preserve ally delivery and attack cadence, then use W/R only from a reconciled Ball position.";
    p.ResearchSummary = "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values; event tracking is reconciled conservatively with live attachment buffs.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Command: Attack", CastKind::Position,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle,
        AllModes, 815.0f, 0.0f, 80.0f, 1400.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 90;
    p.Spells[0].Hitchance = SDK::HitChance::High;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Command: Dissonance", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Buff |
            Intent::Waveclear | Intent::Peel,
        AllModes, 225.0f, 0.0f, 225.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 92;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Command: Protect", CastKind::AllyTarget,
        Intent::Shield | Intent::Damage | Intent::Peel | Intent::Setup,
        AllModes, 1095.0f, 0.0f, 85.0f, 1850.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 95;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Command: Shockwave", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Execute |
            Intent::Interrupt | Intent::Peel,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        415.0f, 0.75f, 415.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;

    p.Trade = Plan("Ball poke",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 90, 900));
    p.AllIn = Plan("Ball delivery shockwave",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 80, 950),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget, 200, 1200),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 220, 1300));
    p.Flee = Plan("Protect and dissonance peel",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 800),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 60, 850),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::RequireMultiTarget, 150, 1000));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
