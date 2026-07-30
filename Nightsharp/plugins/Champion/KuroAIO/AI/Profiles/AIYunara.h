#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Yunara = [] {
    ChampionProfile p{};
    p.ChampionName = "Yunara";
    p.DisplayName = "Yunara";
    p.InternalId = "champion.kuroaio.ai.yunara";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AutoWeave | Mechanic::Stack |
                  Mechanic::Transform | Mechanic::MultiForm |
                  Mechanic::Dash | Mechanic::ReturnProjectile;
    p.Ultimate = UltimatePolicy::AllIn;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Cultivation of Spirit", CastKind::Self,
        Intent::Buff | Intent::Damage | Intent::AutoReset |
            Intent::Waveclear | Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle,
        0.0f, 0.0f, 300.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Priority = 98;
    p.Spells[0].PreserveAutoAttack = false;
    p.Spells[0].HarassManaPercent = 38.0f;
    p.Spells[0].ClearManaPercent = 46.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Arc of Judgment", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Waveclear | Intent::Jungle | Intent::Execute |
            Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic | Mode::Flee,
        1150.0f, 0.45f, 60.0f, 2150.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Priority = 94;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 42.0f;
    p.Spells[1].ClearManaPercent = 52.0f;
    p.Spells[1].AllowOnMinions = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Kanmei's Steps", CastKind::Self,
        Intent::Buff | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        0.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 91;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 45.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Transcend One's Self", CastKind::Self,
        Intent::Buff | Intent::Damage | Intent::Engage |
            Intent::Execute | Intent::Disengage | Intent::Peel,
        Mode::Combo | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 100;
    p.Spells[3].PreserveAutoAttack = true;

    p.Variants[0] = { SDK::SpellSlot::W, "YunaraW2", p.Spells[1] };
    p.Variants[0].Spec.Name = "Arc of Ruin";
    p.Variants[0].Spec.Delay = 0.60f;
    p.Variants[0].Spec.Width = 90.0f;
    p.Variants[0].Spec.Speed = FLT_MAX;
    p.Variants[0].Spec.Collision = false;
    p.Variants[0].Spec.PreserveAutoAttack = true;
    p.Variants[1] = { SDK::SpellSlot::E, "YunaraE2", p.Spells[2] };
    p.Variants[1].Spec.Name = "Untouchable Shadow";
    p.Variants[1].Spec.Kind = CastKind::Position;
    p.Variants[1].Spec.Range = 450.0f;
    p.Variants[1].Spec.TriggerRange = 450.0f;
    p.Variants[1].Spec.DashDistance = 450.0f;
    p.Variants[1].Spec.MaximumEnemiesAtDestination = 1;
    p.Variants[1].Spec.PreserveAutoAttack = true;
    p.VariantCount = 2;

    p.Trade = Plan(
        "Activate Q on a real attack, then weave a clear Arc of Judgment",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange |
                 StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireAfterAttack));
    p.AllIn = Plan(
        "Transcend only for a committed fight, keep attacking, then use empowered W and safe E spacing",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition));
    p.Flee = Plan(
        "Use empowered E only toward a verified safe cursor endpoint; otherwise use E movement speed and W slow",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 575.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 80.0f;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 24;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.PassiveBuff = "YunaraQ";
    p.FormBuff = "YunaraR";
    p.UltimateBuff = "YunaraR";
    p.ThemeFrom = 0xFFF1C56Bu;
    p.ThemeTo = 0xFF78D6FFu;
    p.ThemeSpeed = 0.9f;
    p.TacticalSummary =
        "Cooperate with the orbwalker, spend Q only on a real attack, respect "
        "Arc of Judgment bodies and projectile walls, and use empowered E only "
        "for a walkable low-threat landing.";
    p.ResearchSummary =
        "Pinned to Riot 26.15 / CommunityDragon 16.15: eight-point Q resource, "
        "five-second Q, 1150 W/W2, 450 E2 and fifteen-second R state.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
