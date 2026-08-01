#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Lissandra = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Lissandra;
    p.DisplayName = "Lissandra";
    p.InternalId = "champion.kuroaio.ai.lissandra";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::ReturnProjectile |
                  Mechanic::ObjectTracking | Mechanic::Terrain |
                  Mechanic::AutoWeave | Mechanic::AllyTarget;
    p.Ultimate = UltimatePolicy::Defensive;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Ice Shard", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Engage | Intent::Disengage | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::LastHit |
            Mode::Jungle | Mode::Flee | Mode::Automatic,
        725.0f, 0.25f, 75.0f, 2200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 90;
    p.Spells[0].HarassManaPercent = 38.0f;
    p.Spells[0].ClearManaPercent = 35.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Ring of Frost", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Peel |
            Intent::AntiGapcloser | Intent::Interrupt | Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        450.0f, 0.25f, 225.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 96;
    p.Spells[1].MinimumAoeTargets = 1;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Glacial Path", CastKind::Position,
        Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1050.0f, 0.25f, 120.0f, 850.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 84;
    p.Spells[2].RecastSpellName = "LissandraEMissile";
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Frozen Tomb", CastKind::AnyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Execute |
            Intent::Peel | Intent::Disengage |
            Intent::Interrupt,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        550.0f, 0.25f, 450.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 99;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].TargetHealthPercent = 35.0f;
    p.Spells[3].PlayerHealthPercent = 30.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Q shard spread into W root, preserving E as a return or escape",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireNoCrowdControl),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup));
    p.AllIn = Plan(
        "Q spread, W root, claw through the fight, then target or self Frozen Tomb",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition));
    p.Flee = Plan(
        "send Glacial Path toward cursor, return only to a safe origin, root pursuers",
        Step(SDK::SpellSlot::E, StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 525.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 35.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 36;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "LissandraPassiveReady";
    p.MarkBuff = "LissandraQSlow";
    p.ChannelBuff = "LissandraRSelf";
    p.UltimateBuff = "LissandraRSelf";
    p.TrackedObjectToken = "IcyGrave";
    p.ThemeFrom = 0xFFBCEBFFu;
    p.ThemeTo = 0xFF4268B7u;
    p.ThemeSpeed = 0.72f;
    p.TacticalSummary =
        "Use Ice Shard's central and spread lanes to secure poke and wave control, "
        "root targets only when they are inside Ring of Frost, track Glacial Path "
        "travel and its recast return, and choose Frozen Tomb on an enemy for a "
        "lethal or multi-target lock while self-casting for defensive stasis. "
        "Icy Grave object reconciliation informs safe automatic peel without "
        "inventing a controllable passive cast.";
    p.ResearchSummary =
        "Riot 26.15 spell semantics, CommunityDragon 16.15 spell and buff names, "
        "Riot champion ability text, current Lissandra matchup and teamfight "
        "guides, and deterministic Q spread, E travel/return and R safety "
        "regressions recorded in AI/Research/AILissandra.md.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
