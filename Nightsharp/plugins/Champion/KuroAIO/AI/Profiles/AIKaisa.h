#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Kaisa = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Kaisa;
    p.DisplayName = "Kai'Sa";
    p.InternalId = "champion.kuroaio.ai.kaisa";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Evolve | Mechanic::Stack | Mechanic::Mark |
                  Mechanic::Dash | Mechanic::AutoWeave | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::ManualAssist;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Icathian Rain", CastKind::Self,
        Intent::Damage | Intent::Execute | Intent::Waveclear |
            Intent::LastHit | Intent::Jungle | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear |
            Mode::Jungle | Mode::LastHit | Mode::Automatic,
        600.0f, 0.25f, 210.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Priority = 97;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 40.0f;
    p.Spells[0].ClearManaPercent = 45.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Void Seeker", CastKind::Line,
        Intent::Damage | Intent::Setup | Intent::Execute,
        Mode::Combo | Mode::Harass | Mode::Automatic,
        3000.0f, 0.40f, 90.0f, 1750.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Priority = 96;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 48.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Supercharge", CastKind::Self,
        Intent::Buff | Intent::Disengage | Intent::Mobility,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 91;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Killer Instinct", CastKind::Position,
        Intent::Shield | Intent::Mobility | Intent::Engage | Intent::Peel,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        3000.0f, 0.25f, 120.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 100;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].MaximumEnemiesAtDestination = 2;

    p.Trade = Plan(
        "Build Plasma with autos, use isolated Q and safe W poke, then E to weave",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::SkipIfKillableWithout),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireNoMark),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup));
    p.AllIn = Plan(
        "W or autos apply Plasma, Q isolated target, E weave, R only to a safe mark",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::HoldForExecute),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMark));
    p.Flee = Plan(
        "Use E to disengage and R only when a marked endpoint is safe",
        Step(SDK::SpellSlot::E, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireMark | StepRule::RequireSafePosition));

    p.PreferredCombatDistance = 575.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 30.0f;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 24;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "KaisaPassive";
    p.MarkBuff = "KaisaPassiveMarker";
    p.UltimateBuff = "KaisaRShield";
    p.ThemeFrom = 0xFF7B4DFFu;
    p.ThemeTo = 0xFF1AD6C7u;
    p.ThemeSpeed = 0.93f;
    p.TacticalSummary =
        "Keep the selected marked target as the anchor when reachable; poll "
        "evolutions and Plasma rather than guessing from stats, reserve Q while "
        "a lethal auto is available, require collision-aware W prediction, use E "
        "to weave attack speed, and never R into an unsafe dash endpoint.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Kai'Sa data: Second Skin holds four "
        "marks before the fifth-hit explosion; Icathian Rain fires six missiles "
        "(twelve evolved) with reduced repeat hits; Void Seeker reaches 3000 and "
        "applies two marks (three evolved); Supercharge evolves into stealth; "
        "Killer Instinct targets a marked champion, grants a shield and dashes "
        "near that target. Controller adds conservative endpoint and AA safety.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
