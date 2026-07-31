#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Neeko = [] {
    ChampionProfile p{};
    p.ChampionName = "Neeko";
    p.DisplayName = "Neeko";
    p.InternalId = "champion.kuroaio.ai.neeko";
    p.PrimaryArchetype = Archetype::BurstMage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Channel | Mechanic::MultiForm |
                  Mechanic::ObjectTracking | Mechanic::SpellShield;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 45.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 70;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "NeekoPassive";
    p.ChannelBuff = "NeekoR";
    p.FormBuff = "NeekoPassive";
    p.UltimateBuff = "NeekoR";
    p.TrackedObjectToken = "NeekoWClone";
    p.TacticalSummary =
        "Disguise-aware burst mage: use W clone and stealth to set up a collision-safe E root and Pop Blossom landing while preserving the player's target and attack rhythm.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 spell ranges, bloom timing, clone/stealth windows and channel interruption policy; all live state is reconciled from events and polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Blooming Burst", CastKind::Position,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle,
        AllModes, 800.0f, 0.25f, 225.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 88;
    p.Spells[0].Hitchance = SDK::HitChance::High;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Shapesplitter", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::Disengage | Intent::Setup,
        AllModes, 650.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Activated);
    p.Spells[1].Aim = AimPolicy::AwayFromThreat;
    p.Spells[1].Priority = 94;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Tangle-Barbs", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Interrupt |
            Intent::Waveclear | Intent::Jungle,
        AllModes, 1000.0f, 0.25f, 70.0f, 1300.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 96;
    p.Spells[2].Hitchance = SDK::HitChance::High;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Pop Blossom", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Interrupt |
        Intent::Channel | Intent::Finisher,
        Mode::Combo | Mode::Flee | Mode::Automatic, 600.0f, 1.25f, 600.0f, FLT_MAX,
        false, SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;

    p.Trade = Plan("Root and bloom poke",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 80, 950),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 150, 700));
    p.AllIn = Plan("Stealth Pop Blossom",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 80, 850),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 160, 950),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget, 240, 1800));
    p.Flee = Plan("Clone retreat",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 80, 900),
        Step(SDK::SpellSlot::R, StepRule::ManualAssistOnly, 180, 1500));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
