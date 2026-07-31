#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Milio = [] {
    ChampionProfile p{};
    p.ChampionName = "Milio";
    p.DisplayName = "Milio";
    p.InternalId = "champion.kuroaio.ai.milio";
    p.PrimaryArchetype = Archetype::Enchanter;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AllyTarget | Mechanic::Ammo | Mechanic::Cleanse |
                  Mechanic::Mark | Mechanic::AutoWeave | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::SaveAlly;

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Ultra Mega Fire Kick", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Peel | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Automatic,
        1200.0f, 0.25f, 60.0f, 1200.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 90;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Cozy Campfire", CastKind::AllyTarget,
        Intent::Heal | Intent::Buff | Intent::AllyUtility | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        650.0f, 0.25f, 350.0f, 0.0f, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 100;
    p.Spells[1].TargetHealthPercent = 72.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Warm Hugs", CastKind::AllyTarget,
        Intent::Shield | Intent::Buff | Intent::AllyUtility | Intent::Mobility | Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        650.0f, 0.01f, 172.0f, 0.0f, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 96;
    p.Spells[2].MinimumAmmo = 1;
    p.Spells[2].TargetHealthPercent = 82.0f;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Breath of Life", CastKind::Self,
        Intent::Heal | Intent::Cleanse | Intent::AllyUtility | Intent::Peel |
            Intent::AntiGapcloser,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        800.0f, 0.713f, 800.0f, 0.0f, false,
        SDK::DamageType::True, SDK::SpellType::Circular);
    p.Spells[3].Priority = 110;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("Kick a predicted threat, then sustain the selected ally with Campfire",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireOutsideAaRange),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow));
    p.AllIn = Plan("Q peel/setup, Cozy Campfire sustain, Warm Hugs shields and cleanse heal",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireCrowdControl | StepRule::RequireSafePosition));
    p.Flee = Plan("Warm Hugs movement shield, Campfire reposition and emergency cleanse",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireCrowdControl));

    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 65.0f;
    p.DefensiveHealthPercent = 42.0f;
    p.UltimateTargetHealthPercent = 62.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 44;
    p.PreferSelectedTarget = true;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "MilioPassive";
    p.MarkBuff = "MilioPBuff";
    p.FormBuff = "MilioW";
    p.UltimateBuff = "MilioR";
    p.ThemeFrom = 0xFFFFA347u;
    p.ThemeTo = 0xFF67D5FFu;
    p.ThemeSpeed = 1.02f;
    p.TacticalSummary =
        "Prime Fired Up on allied spell hits, kick the first predicted enemy through a collision-safe line, "
        "place Cozy Campfire where allies can stay, spend two-charge Warm Hugs deliberately, and reserve "
        "Breath of Life for a threatened ally who needs cleanse or healing.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon PC 16.15: Q 1200 range/60 missile width/1200 speed with 140 kick "
        "and 250 landing radius, W 650 cast/350 camp radius, E 650 range with two 17--12 second charges, "
        "and R 800 radius with 65% tenacity for 3 seconds and 150/250/350 plus 0.5 AP healing.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
