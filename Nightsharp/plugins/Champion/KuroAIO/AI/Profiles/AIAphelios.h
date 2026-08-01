#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Aphelios cannot be represented by a fixed spell priority.  The profile is
// deliberately descriptive; AIApheliosController owns the live main/off-hand
// state, independent Q cooldowns, ammo/queue reconciliation, marks, chakrams,
// sentries and every weapon-dependent decision.
inline constexpr ChampionProfile Aphelios = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Aphelios;
    p.DisplayName = "Aphelios";
    p.InternalId = "champion.kuroaio.ai.aphelios";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Ammo | Mechanic::Transform |
                  Mechanic::MultiForm | Mechanic::ObjectTracking |
                  Mechanic::Mark | Mechanic::Stack | Mechanic::Trap |
                  Mechanic::AutoWeave | Mechanic::ReturnProjectile |
                  Mechanic::Recast;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Weapon Ability", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Heal |
            Intent::Buff | Intent::Engage | Intent::Disengage |
            Intent::Interrupt | Intent::AntiGapcloser | Intent::Peel |
            Intent::Setup | Intent::Finisher | Intent::Waveclear |
            Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        1450.0f, 0.35f, 60.0f, 1800.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 92;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 58.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Phase", CastKind::Self,
        Intent::Buff | Intent::Setup | Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 96;
    p.Spells[1].PreserveAutoAttack = true;

    // Slot E is the passive weapon-queue/stat-level interface, not a castable
    // combat spell.  It remains explicitly disabled to prevent a generic
    // engine from ever issuing an E request.
    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Weapon Queue / Lethality", CastKind::None,
        Intent::Buff, Mode::None, 0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 0;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Moonlight Vigil", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Heal |
            Intent::Engage | Intent::Disengage | Intent::Execute |
            Intent::AntiGapcloser | Intent::Peel | Intent::Setup |
            Intent::Objective,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1300.0f, 0.50f, 110.0f, 1000.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 98;
    p.Spells[3].TriggerRange = 210.0f;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = true;

    SpellSpec calibrum = p.Spells[0];
    calibrum.Name = "Moonshot";
    calibrum.Kind = CastKind::Line;
    calibrum.Range = calibrum.TriggerRange = 1450.0f;
    calibrum.Delay = 0.35f;
    calibrum.Width = 60.0f;
    calibrum.Speed = 1800.0f;
    calibrum.Collision = true;
    calibrum.Shape = SDK::SpellType::SkillshotLine;
    p.Variants[p.VariantCount++] = {
        SDK::SpellSlot::Q, "ApheliosCalibrumQ", calibrum,
    };

    SpellSpec severum = p.Spells[0];
    severum.Name = "Onslaught";
    severum.Kind = CastKind::Self;
    severum.Range = severum.TriggerRange = 550.0f;
    severum.Delay = 0.0f;
    severum.Width = 0.0f;
    severum.Speed = FLT_MAX;
    severum.Collision = false;
    severum.Shape = SDK::SpellType::Targeted;
    p.Variants[p.VariantCount++] = {
        SDK::SpellSlot::Q, "ApheliosSeverumQ", severum,
    };

    SpellSpec gravitum = p.Spells[0];
    gravitum.Name = "Binding Eclipse";
    gravitum.Kind = CastKind::Self;
    gravitum.Range = gravitum.TriggerRange = 1800.0f;
    gravitum.Delay = 0.25f;
    gravitum.Width = 0.0f;
    gravitum.Speed = FLT_MAX;
    gravitum.Collision = false;
    gravitum.Shape = SDK::SpellType::Targeted;
    p.Variants[p.VariantCount++] = {
        SDK::SpellSlot::Q, "ApheliosGravitumQ", gravitum,
    };

    SpellSpec infernum = p.Spells[0];
    infernum.Name = "Duskwave";
    infernum.Kind = CastKind::Cone;
    infernum.Range = infernum.TriggerRange = 850.0f;
    infernum.Delay = 0.40f;
    infernum.Width = 375.0f;
    infernum.Speed = FLT_MAX;
    infernum.Collision = false;
    infernum.Shape = SDK::SpellType::SkillshotCone;
    p.Variants[p.VariantCount++] = {
        SDK::SpellSlot::Q, "ApheliosInfernumQ", infernum,
    };

    SpellSpec crescendum = p.Spells[0];
    crescendum.Name = "Sentry";
    crescendum.Kind = CastKind::Position;
    crescendum.Range = crescendum.TriggerRange = 475.0f;
    crescendum.Delay = 0.25f;
    crescendum.Width = 500.0f;
    crescendum.Speed = FLT_MAX;
    crescendum.Collision = false;
    crescendum.Shape = SDK::SpellType::SkillshotCircle;
    p.Variants[p.VariantCount++] = {
        SDK::SpellSlot::Q, "ApheliosCrescendumQ", crescendum,
    };

    p.Trade = Plan(
        "weapon-aware short trade: exploit off-hand proc, preserve cycle and let the player weave autos",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W,
             StepRule::AllowDuringWindup | StepRule::ManualAssistOnly));

    p.AllIn = Plan(
        "score the current pair, low-ammo incoming gun and five R variants rather than fixed Q-W-R",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W,
             StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack));

    p.Flee = Plan(
        "prefer Gravitum root or Severum sustain, use Sentry as a safe zone and reserve defensive R",
        Step(SDK::SpellSlot::W,
             StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequirePlayerLow |
                 StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 590.0f;
    p.EngageHealthPercent = 44.0f;
    p.DefensiveHealthPercent = 33.0f;
    p.UltimateTargetHealthPercent = 58.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 30;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ApheliosPReload";
    p.MarkBuff = "ApheliosCalibrumBonusRangeBuff";
    p.FormBuff = "ApheliosCalibrumManager";
    p.TrackedObjectToken = "ApheliosCrescendumTurret";
    p.ThemeFrom = 0xFF72D8D3u;
    p.ThemeTo = 0xFFB27AE8u;
    p.ThemeSpeed = 0.72f;
    p.TacticalSummary =
        "Reconstruct all five guns and ammo, keep independent Q cooldowns, "
        "choose standard or green-blue rotation contextually, exploit low-ammo "
        "incoming-weapon chains, preserve Calibrum marks and Crescendum returns, "
        "root only real Gravitum marks, snapshot Sentry off-hand effects and "
        "select among five Moonlight Vigil variants while yielding movement, "
        "ordinary attacks, Flash and summoners to the player.";
    p.ResearchSummary =
        "Pinned to Riot 26.14 with Aphelios changes through 26.13/26.4/26.1 "
        "and CommunityDragon 16.14. Cross-checked The Book of Aphelios, "
        "current Challenger/OTP rotation material, Season 16 Aleksis007, "
        "Peyz/Viper/Gumayusi reviews, the local EnsoulSharp five-gun/ammo "
        "implementation, combo catalogs and every local champion plugin.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
