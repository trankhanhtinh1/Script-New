#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Lulu = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Lulu;
    p.DisplayName = "Lulu";
    p.InternalId = "champion.kuroaio.ai.lulu";
    p.PrimaryArchetype = Archetype::Enchanter;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AllyTarget | Mechanic::Tether | Mechanic::AutoWeave |
                  Mechanic::MissingHealth | Mechanic::Recast;
    p.Ultimate = UltimatePolicy::SaveAlly;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Glitterlance", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        950.0f, 0.25f, 60.0f, 1450.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 87;
    p.Spells[0].TriggerRange = 950.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Whimsy", CastKind::AnyTarget,
        Intent::CrowdControl | Intent::Mobility | Intent::Disengage |
            Intent::Peel | Intent::Buff | Intent::AllyUtility,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        650.0f, 0.25f, 60.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 95;
    p.Spells[1].TriggerRange = 650.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Help, Pix!", CastKind::AnyTarget,
        Intent::Shield | Intent::Damage | Intent::Buff | Intent::AllyUtility |
            Intent::Setup | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        650.0f, 0.25f, 80.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 98;
    p.Spells[2].TriggerRange = 650.0f;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Wild Growth", CastKind::Self,
        Intent::Heal | Intent::CrowdControl | Intent::AllyUtility |
            Intent::Peel | Intent::Engage | Intent::Buff,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        900.0f, 0.25f, 100.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 100;
    p.Spells[3].TriggerRange = 900.0f;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PlayerHealthPercent = 44.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Use dual-bolt Glitterlance, polymorph priority threats and transfer Pix for shield or damage",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup));
    p.AllIn = Plan(
        "Protect the carry with Help Pix, speed the engage or polymorph the threat, then grow the endangered ally",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget));
    p.Flee = Plan(
        "Speed a safe ally, shield during Pix transfer and use Wild Growth to knock up pursuers",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow));

    p.PreferredCombatDistance = 625.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 44.0f;
    p.UltimateTargetHealthPercent = 62.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "LuluPix";
    p.FormBuff = "LuluW";
    p.ChannelBuff = "LuluE";
    p.UltimateBuff = "LuluR";
    p.TrackedObjectToken = "LuluPix";
    p.ThemeFrom = 0xFFFF77D0u;
    p.ThemeTo = 0xFFB86BFFu;
    p.ThemeSpeed = 0.94f;
    p.TacticalSummary =
        "Preserve selected target ownership while managing Pix bolt origin, dual-bolt Glitterlance, defensive polymorph or speed Whimsy posture, and health-safe Wild Growth timing.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Lulu values, spell names and Pix transfer behavior are recorded in AILulu.md.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
