#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Zyra = [] {
    ChampionProfile p{};
    p.ChampionName = "Zyra";
    p.DisplayName = "Zyra";
    p.InternalId = "champion.kuroaio.ai.zyra";
    p.PrimaryArchetype = Archetype::Catcher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::ObjectTracking | Mechanic::Trap |
                  Mechanic::Terrain | Mechanic::WallInteraction | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 800.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 72.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ZyraPassive";
    p.MarkBuff = "ZyraE";
    p.UltimateBuff = "ZyraR";
    p.TrackedObjectToken = "ZyraSeed";
    p.TacticalSummary =
        "Seed economy and plant network: Q/E awaken nearby seeds into Thorn Spitters or Vine Lashers, "
        "W places seeds deliberately, and R grows plants while knocking enemies up.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon PC 16.15 baseline. Q is an 800-range ground burst, W stores "
        "seeds, E is a 1100-range root line with unit collision, and R is a 700-range 500-radius "
        "plant zone with a delayed knock-up.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Deadly Spines", CastKind::Position,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit, AllModes, 800.0f, 0.85f, 140.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 88;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 34.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Rampant Growth", CastKind::Position,
        Intent::Setup | Intent::Vision | Intent::Waveclear | Intent::Jungle,
        AllModes, 850.0f, 0.25f, 80.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::TargetPosition;
    p.Spells[1].Priority = 74;
    p.Spells[1].MinimumAmmo = 1;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 48.0f;
    p.Spells[1].ClearManaPercent = 38.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Grasping Roots", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Peel |
            Intent::AntiGapcloser | Intent::Setup | Intent::Interrupt,
        AllModes, 1100.0f, 0.25f, 70.0f, 1150.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 96;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 52.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Stranglethorns", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Peel |
            Intent::Finisher | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic, 700.0f, 2.0f, 500.0f, FLT_MAX,
        false, SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].Hitchance = SDK::HitChance::High;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("Seed into root and plant burst",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireNoCrowdControl, 100, 1300),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 180, 1100));
    p.AllIn = Plan("Root, awaken plants, then Stranglethorns",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1300),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 140, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition |
             StepRule::RequireMultiTarget, 260, 2200));
    p.Flee = Plan("Root the pursuer and zone the escape",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1300),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 120, 2200));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
