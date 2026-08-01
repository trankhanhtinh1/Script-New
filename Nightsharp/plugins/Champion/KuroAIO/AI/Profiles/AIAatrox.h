#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Research synthesis (26.14): W is the reliable all-in setup; Q1/Q2 sweetspots
// are extended by E during their windups, while Q3 is held for the pull/escape
// endpoint.  R is a commit/reset tool, not an opener for every short trade.
inline constexpr ChampionProfile Aatrox = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Aatrox;
    p.DisplayName = "Aatrox";
    p.InternalId = "champion.kuroaio.ai.aatrox";
    p.PrimaryArchetype = Archetype::Juggernaut;
    p.Resource = ResourceModel::None;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash |
                  Mechanic::DirectionalSweet | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::AllIn;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "The Darkin Blade", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        650.0f, 0.60f, 180.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::BehindTarget;
    p.Spells[0].Priority = 82;
    p.Spells[0].MinimumAoeTargets = 2;
    p.Spells[0].RecastSpellName = "AatroxQ2";
    p.Spells[0].ClearManaPercent = 0.0f;

    // The runtime exposes each Darkin Blade cast under its own spell name.
    // Preserve those forms in the catalog so prediction is rebuilt with the
    // widening Q2 blade and the circular Q3 body instead of retaining Q1's
    // narrow line metadata for the whole three-cast sequence.
    SpellSpec q2 = p.Spells[0];
    q2.Name = "The Darkin Blade - Second Cast";
    q2.Kind = CastKind::Cone;
    q2.Range = q2.TriggerRange = 525.0f;
    q2.Width = 500.0f;
    q2.Shape = SDK::SpellType::SkillshotCone;
    q2.RecastSpellName = "AatroxQ3";
    p.Variants[p.VariantCount++] = {
        SDK::SpellSlot::Q, "AatroxQ2", q2,
    };

    SpellSpec q3 = p.Spells[0];
    q3.Name = "The Darkin Blade - Third Cast";
    q3.Kind = CastKind::Circle;
    q3.Range = q3.TriggerRange = 400.0f;
    q3.Width = 300.0f;
    q3.Shape = SDK::SpellType::SkillshotCircle;
    q3.RecastSpellName = "";
    p.Variants[p.VariantCount++] = {
        SDK::SpellSlot::Q, "AatroxQ3", q3,
    };

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Infernal Chains", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Interrupt | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Automatic,
        825.0f, 0.25f, 80.0f, 1800.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotMissileLine);
    p.Spells[1].Priority = 95;
    p.Spells[1].Hitchance = SDK::HitChance::High;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Umbral Dash", CastKind::Position,
        Intent::Mobility | Intent::Engage | Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        300.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].DashDistance = 300.0f;
    p.Spells[2].DesiredDistance = 475.0f;
    p.Spells[2].Priority = 74;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "World Ender", CastKind::Self,
        Intent::Buff | Intent::Engage | Intent::Heal,
        Mode::Combo,
        650.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].TriggerRange = 700.0f;
    p.Spells[3].Priority = 60;
    p.Spells[3].TargetHealthPercent = 75.0f;

    p.Trade = Plan(
        "Sweetspot trade",
        Step(SDK::SpellSlot::Q),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition |
                 StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireRecast));

    p.AllIn = Plan(
        "Chain into triple sweetspot",
        Step(SDK::SpellSlot::W),
        Step(SDK::SpellSlot::Q),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition |
                 StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireRecast),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireRecast),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::SkipIfKillableWithout));

    p.Flee = Plan(
        "Dash and peel",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget));

    p.PreferredCombatDistance = 475.0f;
    p.EngageHealthPercent = 30.0f;
    p.DefensiveHealthPercent = 24.0f;
    p.UltimateTargetHealthPercent = 75.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.PassiveBuff = "AatroxPassiveReady";
    p.MarkBuff = "AatroxW";
    p.UltimateBuff = "AatroxR";
    p.ThemeFrom = 0xFFFF4A4Au;
    p.ThemeTo = 0xFF5A0B0Bu;
    p.ThemeSpeed = 0.96f;
    p.TacticalSummary =
        "W setup; E-correct Q1/Q2 sweetspots; preserve Q3 for pull or exit; "
        "commit R only when the extended fight or reset is justified.";
    p.ResearchSummary =
        "CommunityDragon 26.14 kit data, League Wiki geometry/timings, local "
        "EnsoulSharp TestOrbwalker stage/event audit, AIO state-machine audit, "
        "and challenger/pro combo-guide review.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
