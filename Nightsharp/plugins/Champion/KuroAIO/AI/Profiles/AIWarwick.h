#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Warwick = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Warwick;
    p.DisplayName = "Warwick";
    p.InternalId = "champion.kuroaio.ai.warwick";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Channel | Mechanic::MissingHealth |
                  Mechanic::Mark | Mechanic::ObjectTracking | Mechanic::Execute;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 365.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 45.0f;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "WarwickPassive";
    p.MarkBuff = "WarwickBloodHunt";
    p.ChannelBuff = "WarwickQ";
    p.UltimateBuff = "WarwickR";
    p.TacticalSummary =
        "Blood-scent diver: hold Jaws of the Beast for follow-through and healing, "
        "hunt low-health targets with Blood Hunt, fear with Primal Howl, and only "
        "leap with a collision-safe Infinite Duress.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Warwick Q hold-release, W blood-scent "
        "thresholds, E reduction/fear recast and R suppression projectile are modeled.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Jaws of the Beast", CastKind::EnemyTarget,
        Intent::Damage | Intent::Heal | Intent::Channel | Intent::Execute | Intent::Jungle |
            Intent::LastHit,
        AllModes, 425.0f, 0.5f, 55.0f, 1500.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[0].Priority = 94;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].ComboManaPercent = 0.0f;
    p.Spells[0].HarassManaPercent = 35.0f;
    p.Spells[0].ClearManaPercent = 30.0f;
    p.Spells[0].PlayerHealthPercent = 78.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Blood Hunt", CastKind::Self,
        Intent::Buff | Intent::Damage | Intent::Engage | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Automatic | Mode::Flee,
        4000.0f, 0.5f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 78;
    p.Spells[1].TargetHealthPercent = 50.0f;
    p.Spells[1].PlayerHealthPercent = 45.0f;
    p.Spells[1].HarassManaPercent = 50.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Primal Howl", CastKind::Self,
        Intent::Shield | Intent::CrowdControl | Intent::Disengage | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Automatic | Mode::Flee | Mode::Jungle,
        375.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 88;
    p.Spells[2].PlayerHealthPercent = 72.0f;
    p.Spells[2].PreserveAutoAttack = false;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Infinite Duress", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Heal | Intent::Execute | Intent::Finisher,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        2500.0f, 0.1f, 125.0f, 0.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].TargetHealthPercent = 45.0f;
    p.Spells[3].PlayerHealthPercent = 45.0f;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan("hold Q for bite healing and fear setup",
        Step(SDK::SpellSlot::W, StepRule::RequireTargetLow, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 40, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireInsideAaRange, 100, 1100));
    p.AllIn = Plan("Blood Hunt chase into suppression",
        Step(SDK::SpellSlot::W, StepRule::RequireTargetLow, 0, 700),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 40, 1000),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 120, 1250),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequirePlayerLow, 180, 1450));
    p.Flee = Plan("fear peel then healing bite",
        Step(SDK::SpellSlot::E, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequirePlayerLow, 100, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 160, 1200));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
