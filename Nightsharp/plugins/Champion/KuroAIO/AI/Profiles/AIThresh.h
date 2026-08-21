#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Thresh = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Thresh;
    p.DisplayName = "Thresh";
    p.InternalId = "champion.kuroaio.ai.thresh";
    p.PrimaryArchetype = Archetype::Catcher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Stack | Mechanic::AllyTarget |
                  Mechanic::WallInteraction | Mechanic::AutoWeave |
                  Mechanic::DirectionalSweet | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::Defensive;
    p.PreferredCombatDistance = 450.0f;
    p.EngageHealthPercent = 50.0f;
    p.DefensiveHealthPercent = 35.0f;
    p.UltimateTargetHealthPercent = 72.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 48;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ThreshPassive";
    p.MarkBuff = "ThreshQ";
    p.FormBuff = "ThreshE";
    p.UltimateBuff = "ThreshRPenta";
    p.TrackedObjectToken = "ThreshLantern";
    p.TacticalSummary =
        "Catcher support that banks souls for armor and shield scaling, hooks reachable targets, "
        "rescues allies with Dark Passage, chooses directional Flay displacement and boxes unsafe chases.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon 16.15 data model soul armor, Q recast, lantern rescue, "
        "directional E and a five-wall R cage with reachable-target and allied-carry safety gates.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Death Sentence", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Interrupt | Intent::AntiGapcloser | Intent::Peel | Intent::Setup |
            Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1075.0f, 0.5f, 1900.0f, 38.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 98;
    p.Spells[0].RecastSpellName = "ThreshQLeap";
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 55.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Dark Passage", CastKind::Position,
        Intent::Shield | Intent::AllyUtility | Intent::Disengage | Intent::Peel |
            Intent::Setup | Intent::Vision,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        950.0f, 0.25f, 1450.0f, 150.0f, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].Priority = 100;
    p.Spells[1].PlayerHealthPercent = 50.0f;
    p.Spells[1].HarassManaPercent = 65.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Flay", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Interrupt | Intent::AntiGapcloser | Intent::Peel | Intent::Setup |
            Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic | Mode::Jungle |
            Mode::LaneClear | Mode::LastHit,
        500.0f, 0.25f, 1200.0f, 80.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Cursor;
    p.Spells[2].Priority = 92;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 58.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "The Box", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Peel | Intent::Setup |
            Intent::Disengage | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        450.0f, 0.75f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 96;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PlayerHealthPercent = 42.0f;

    p.Trade = Plan("hook a reachable target, flay direction and preserve lantern rescue",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1000),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 120, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow, 180, 1300));
    p.AllIn = Plan("hook, recast only into allied followup, box walls and lantern ally exit",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1000),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack, 100, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireSafePosition, 160, 1300),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow, 220, 1500));
    p.Flee = Plan("flay pursuers and place lantern for allied rescue",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 100, 1100),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireRecast, 180, 1250),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition, 220, 1450));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
