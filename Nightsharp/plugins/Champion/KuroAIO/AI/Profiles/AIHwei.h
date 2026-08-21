#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Hwei = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Hwei;
    p.DisplayName = "Hwei";
    p.InternalId = "champion.kuroaio.ai.hwei";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::MultiForm | Mechanic::Stance | Mechanic::Stack |
                  Mechanic::Channel | Mechanic::ObjectTracking | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 35.0f;
    p.UltimateTargetHealthPercent = 52.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 65;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "HweiPassive";
    p.MarkBuff = "HweiPassiveMark";
    p.ChannelBuff = "HweiR";
    p.TrackedObjectToken = "HweiE";
    p.TacticalSummary =
        "Paintbook battlemage: choose Disaster, Serenity or Turmoil stance, commit a useful Q/W/E paint combination, and preserve mana and safe zones.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 spellbook metadata, passive-mark and zone state reconciled from events and polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Subject of Disaster", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 1200.0f, 0.25f, 110.0f, 1400.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 38.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Subject of Serenity", CastKind::Position,
        Intent::Damage | Intent::Shield | Intent::Heal | Intent::Buff |
            Intent::Setup | Intent::Peel | Intent::Waveclear,
        AllModes, 950.0f, 0.25f, 240.0f, 1800.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[1].Priority = 84;
    p.Spells[1].HarassManaPercent = 50.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Subject of Turmoil", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::AntiGapcloser | Intent::Peel | Intent::Waveclear,
        AllModes, 1200.0f, 0.25f, 275.0f, 1800.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 90;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].HarassManaPercent = 48.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Spiraling Despair", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Execute | Intent::Finisher |
            Intent::Channel | Intent::Setup,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        1300.0f, 0.25f, 650.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = true;

    // CommunityDragon exposes the active spell names differently while the
    // first key selects a subject. Keep all nine paint combinations auditable.
    p.Variants[0] = {SDK::SpellSlot::Q, "HweiQQ", p.Spells[0]};
    p.Variants[1] = {SDK::SpellSlot::W, "HweiQW", p.Spells[0]};
    p.Variants[2] = {SDK::SpellSlot::E, "HweiQE", p.Spells[0]};
    p.Variants[3] = {SDK::SpellSlot::Q, "HweiWQ", p.Spells[1]};
    p.Variants[4] = {SDK::SpellSlot::W, "HweiWW", p.Spells[1]};
    p.Variants[5] = {SDK::SpellSlot::E, "HweiWE", p.Spells[1]};
    p.Variants[6] = {SDK::SpellSlot::Q, "HweiEQ", p.Spells[2]};
    p.Variants[7] = {SDK::SpellSlot::W, "HweiEW", p.Spells[2]};
    p.Variants[8] = {SDK::SpellSlot::E, "HweiEE", p.Spells[2]};
    p.VariantCount = 9;

    p.Trade = Plan("Painted poke and peel",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 140, 900));
    p.AllIn = Plan("Disaster mark into Turmoil and Despair",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 100, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireCrowdControl | StepRule::AllowDuringWindup, 180, 950),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireSafePosition, 280, 1800));
    p.Flee = Plan("Serenity movement and Turmoil peel",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 800),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 90, 850),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 170, 1000));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
