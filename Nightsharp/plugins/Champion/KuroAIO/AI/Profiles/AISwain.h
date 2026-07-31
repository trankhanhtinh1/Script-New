#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Swain = [] {
    ChampionProfile p{};
    p.ChampionName = "Swain";
    p.DisplayName = "Swain";
    p.InternalId = "champion.kuroaio.ai.swain";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Channel | Mechanic::Global |
                  Mechanic::ObjectTracking | Mechanic::Stack | Mechanic::Tether |
                  Mechanic::MissingHealth | Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 575.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 60.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 58;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "SwainSoulCounter";
    p.MarkBuff = "SwainPPullReady";
    p.ChannelBuff = "SwainR";
    p.UltimateBuff = "SwainR";
    p.TrackedObjectToken = "SwainSoul";
    p.TacticalSummary =
        "Battlemage: collect soul fragments, bind targets with Nevermove and its pull recast, place global Vision of Empire zones, use close-range Death's Hand bolts, and maintain Demon Form until a safe Demonflare detonation.";
    p.ResearchSummary =
        "Riot 26.15 has no Swain changes; spell values and runtime state use the CommunityDragon 16.15 Swain champion JSON dossier.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Death's Hand", CastKind::Cone,
        Intent::Damage | Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        AllModes, 750.0f, 0.25f, 20.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCone);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 78;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 48.0f;
    p.Spells[0].ClearManaPercent = 42.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Vision of Empire", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Vision | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 7500.0f, 0.25f, 325.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::BestAoe;
    p.Spells[1].Priority = 66;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 58.0f;
    p.Spells[1].ClearManaPercent = 55.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Nevermove", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Peel |
            Intent::Recast | Intent::Interrupt,
        AllModes, 850.0f, 0.25f, 85.0f, 935.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 96;
    p.Spells[2].RecastSpellName = "SwainE2";
    p.Spells[2].PreserveAutoAttack = false;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Demonic Ascension", CastKind::Self,
        Intent::Damage | Intent::Heal | Intent::Channel | Intent::Recast | Intent::Engage |
            Intent::Disengage | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::Automatic | Mode::Flee,
        600.0f, 0.25f, 600.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].RecastSpellName = "SwainRSoulFlare";
    p.Spells[3].PreserveAutoAttack = false;

    SpellVariant e2{};
    e2.Slot = SDK::SpellSlot::E;
    e2.RuntimeNameToken = "SwainE2";
    e2.Spec = p.Spells[2];
    e2.Spec.Name = "Nevermove pull";
    e2.Spec.Kind = CastKind::Self;
    e2.Spec.Range = 1150.0f;
    e2.Spec.TriggerRange = 1150.0f;
    e2.Spec.Collision = false;
    e2.Spec.Intents = Intent::CrowdControl | Intent::Damage | Intent::Recast;
    e2.Spec.RequiredPlayerBuff = "SwainPPullReady";
    p.Variants[p.VariantCount++] = e2;

    SpellVariant r2{};
    r2.Slot = SDK::SpellSlot::R;
    r2.RuntimeNameToken = "SwainRSoulFlare";
    r2.Spec = p.Spells[3];
    r2.Spec.Name = "Demonflare";
    r2.Spec.Kind = CastKind::Self;
    r2.Spec.Range = 600.0f;
    r2.Spec.TriggerRange = 600.0f;
    r2.Spec.Intents = Intent::Damage | Intent::CrowdControl | Intent::Recast | Intent::Finisher;
    r2.Spec.RequiredPlayerBuff = "SwainR";
    p.Variants[p.VariantCount++] = r2;

    p.Trade = Plan("Nevermove soul trade",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireRecast | StepRule::RequireTarget, 90, 1050),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 150, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 220, 1250));
    p.AllIn = Plan("Demonic tether collapse",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireRecast | StepRule::RequireTarget, 90, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 130, 1900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 220, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 300, 1400),
        Step(SDK::SpellSlot::R, StepRule::RequireRecast | StepRule::RequireTarget, 650, 5000));
    p.Flee = Plan("Demon form peel",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 80, 1300),
        Step(SDK::SpellSlot::R, StepRule::RequireRecast | StepRule::RequireTarget, 160, 3800));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
