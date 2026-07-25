#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Azir is an AP marksman controlled through persistent soldier geometry.
// The profile is descriptive: AIAzirController owns soldier discovery,
// late-Q timing, E collision/drift and every R displacement decision.
inline constexpr ChampionProfile Azir = [] {
    ChampionProfile p{};
    p.ChampionName = "Azir";
    p.DisplayName = "Azir";
    p.InternalId = "champion.kuroaio.ai.azir";
    p.PrimaryArchetype = Archetype::Specialist;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Pet | Mechanic::ObjectTracking |
                  Mechanic::AutoWeave | Mechanic::Dash |
                  Mechanic::Terrain | Mechanic::WallInteraction |
                  Mechanic::Ammo | Mechanic::Tether |
                  Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::Defensive;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Conquering Sands", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee,
        720.0f, 0.25f, 70.0f, 1600.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 91;
    p.Spells[0].Aim = AimPolicy::BehindTarget;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].HarassManaPercent = 48.0f;
    p.Spells[0].ClearManaPercent = 60.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Arise!", CastKind::Position,
        Intent::Damage | Intent::Setup | Intent::Disengage |
            Intent::Waveclear | Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        500.0f, 0.25f, 1.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 96;
    p.Spells[1].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[1].MinimumAmmo = 1;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 38.0f;
    p.Spells[1].ClearManaPercent = 52.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Shifting Sands", CastKind::AnyTarget,
        Intent::Damage | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Shield | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1100.0f, 0.0f, 140.0f, 1700.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 98;
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].DashDistance = 1100.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 100.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Emperor's Divide", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::Setup |
            Intent::Finisher,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        250.0f, 0.50f, 930.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCone);
    p.Spells[3].Priority = 99;
    p.Spells[3].Aim = AimPolicy::AwayFromThreat;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "place W, let the player stab, move soldiers with late Q, then let the player stab again",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack));

    p.AllIn = Plan(
        "prefer soldier DPS; spend E-Q-R only after displacement and exit geometry are verified",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition));

    p.Flee = Plan(
        "place a cursor-side escape soldier, dash, and redirect it with Q without issuing movement",
        Step(SDK::SpellSlot::W,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 68.0f;
    p.DefensiveHealthPercent = 42.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 30;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "AzirPassive";
    p.FormBuff = "AzirEShield";
    p.TrackedObjectToken = "AzirSoldier";
    p.ThemeFrom = 0xFFFFD35Au;
    p.ThemeTo = 0xFF9B6A21u;
    p.ThemeSpeed = 0.58f;
    p.TacticalSummary =
        "Keep movement, attacks, target choice and summoners player-owned. "
        "Maintain a defensive soldier, delay Q until the target leaves stab "
        "range, collide with E only deliberately, front-to-back by default, "
        "and use R as peel unless a safe allied displacement is proven.";
    p.ResearchSummary =
        "Pinned to Riot 26.14 and CommunityDragon PC 16.14, including the "
        "26.6 Q/W scaling and 26.14 on-hit fixes; reconciled current Shok "
        "Rank-1 guidance, the in-depth Azir mechanics guide, Mobalytics "
        "combos, current AzirMains OTP discussions, EzEvade soldier tracking "
        "and the dedicated OrbwalkerKuro Sand Soldier implementation.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
