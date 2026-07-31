#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Sona = [] {
    ChampionProfile p{};
    p.ChampionName = "Sona";
    p.DisplayName = "Sona";
    p.InternalId = "champion.kuroaio.ai.sona";
    p.PrimaryArchetype = Archetype::Enchanter;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AllyTarget | Mechanic::Stack | Mechanic::AutoWeave |
                  Mechanic::DirectionalSweet | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Hymn of Valor", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::Automatic,
        825.0f, 0.25f, 70.0f, 1300.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 82;
    p.Spells[0].TriggerRange = 825.0f;
    p.Spells[0].HarassManaPercent = 48.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Aria of Perseverance", CastKind::Self,
        Intent::Heal | Intent::Shield | Intent::AllyUtility | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1000.0f, 0.25f, 400.0f, 1500.0f, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 100;
    p.Spells[1].TriggerRange = 1000.0f;
    p.Spells[1].PlayerHealthPercent = 58.0f;
    p.Spells[1].TargetHealthPercent = 62.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Song of Celerity", CastKind::Self,
        Intent::Mobility | Intent::Buff | Intent::AllyUtility | Intent::Disengage |
            Intent::Engage,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1000.0f, 0.25f, 400.0f, 1500.0f, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 88;
    p.Spells[2].TriggerRange = 430.0f;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Crescendo", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Peel |
            Intent::Interrupt | Intent::AntiGapcloser,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        900.0f, 0.25f, 140.0f, 2400.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 98;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("Q poke while building the next aura chord, W only for a real heal window",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireOutsideAaRange),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow));
    p.AllIn = Plan("Crescendo multi-hit setup, Q damage aura, W sustain and E chase speed",
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition));
    p.Flee = Plan("Celerity first, Perseverance under threat, Crescendo only for peel",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget));

    p.PreferredCombatDistance = 575.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 40.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 44;
    p.PreferSelectedTarget = true;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "SonaPassive";
    p.MarkBuff = "SonaPassiveAccelerandoCount";
    p.FormBuff = "SonaE";
    p.UltimateBuff = "SonaR";
    p.ThemeFrom = 0xFF62D5E8u;
    p.ThemeTo = 0xFF8C6BFFu;
    p.ThemeSpeed = 1.04f;
    p.TacticalSummary =
        "Rotate Q/W/E auras deliberately, preserve the third-stack Power Chord for the right target, "
        "keep low-health allies inside Aria range, accelerate grouped allies, and reserve Crescendo "
        "for a confirmed multi-hit engage or imminent peel.";
    p.ResearchSummary =
        "CommunityDragon PC 16.15 Sona JSON and Riot 26.15 notes: Q 825 range/1300 missile, W 1000 heal "
        "and 400 aura, E 400 aura, R 900 range/140 width/2400 missile, plus Accelerando and the three "
        "Power Chord variants; geometry covers aura proximity and piercing Crescendo boundaries.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
