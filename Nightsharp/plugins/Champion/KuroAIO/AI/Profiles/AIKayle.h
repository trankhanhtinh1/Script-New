#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Kayle's profile records the level-gated ascent and attack/passive semantics;
// AIKayleController owns stack reconciliation, execute timing and ally-save policy.
inline constexpr ChampionProfile Kayle = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Kayle;
    p.DisplayName = "Kayle";
    p.InternalId = "champion.kuroaio.ai.kayle";
    p.PrimaryArchetype = Archetype::Specialist;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Transform | Mechanic::MultiForm | Mechanic::Stack |
                  Mechanic::Execute | Mechanic::AllyTarget | Mechanic::AutoWeave |
                  Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::SaveAlly;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Radiant Blast", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        900.0f, 0.25f, 80.0f, 1600.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 92;
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].Collision = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 45.0f;
    p.Spells[0].ClearManaPercent = 55.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Celestial Blessing", CastKind::AllyTarget,
        Intent::Heal | Intent::Buff | Intent::Disengage | Intent::AllyUtility,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee |
            Mode::Automatic,
        900.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[1].Priority = 98;
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].PlayerHealthPercent = 66.0f;
    p.Spells[1].HarassManaPercent = 48.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Starfire Spellblade", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::AutoReset | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        525.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 97;
    p.Spells[2].TriggerRange = 525.0f;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = false;
    p.Spells[2].AllowOnMinions = true;
    p.Spells[2].TargetHealthPercent = 48.0f;
    p.Spells[2].HarassManaPercent = 0.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Divine Judgment", CastKind::AnyTarget,
        Intent::Shield | Intent::Damage | Intent::AllyUtility | Intent::Disengage,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        900.0f, 0.0f, 675.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 100;
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].PlayerHealthPercent = 42.0f;

    p.Trade = Plan(
        "Q resistance shred into safe ranged attacks, E only for a reset or execute",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireOutsideAaRange),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack |
             StepRule::HoldForExecute));
    p.AllIn = Plan(
        "stack Zealous, Q shred, E reset, reserve Divine Judgment for the threatened ally",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::AllowDuringWindup));
    p.Flee = Plan(
        "W speed first, then R only for a low-health threatened ally or self",
        Step(SDK::SpellSlot::W, StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 525.0f;
    p.EngageHealthPercent = 72.0f;
    p.DefensiveHealthPercent = 35.0f;
    p.UltimateTargetHealthPercent = 38.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "KaylePassiveStack";
    p.MarkBuff = "KayleQDebuff";
    p.FormBuff = "KayleEPassive";
    p.UltimateBuff = "KayleR";
    p.ThemeFrom = 0xFFFFD66Bu;
    p.ThemeTo = 0xFF8EA7FFu;
    p.ThemeSpeed = 0.68f;
    p.TacticalSummary =
        "Track Zealous stacks and level-gated melee/ranged/wave forms; shred with a collision-checked Q, preserve attack windups for E's missing-health execute reset, use W on the most threatened low-health ally, and reserve R for a real invulnerability save rather than damage greed.";
    p.ResearchSummary =
        "Pinned to Riot 26.15 and CommunityDragon 16.15: Kayle ascends at levels 6/11/16, Q is a 900-range resistance-shredding line, W heals and speeds an ally plus Kayle, E empowers the next attack with missing-health damage, and R grants ally invulnerability before an area sword strike.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
