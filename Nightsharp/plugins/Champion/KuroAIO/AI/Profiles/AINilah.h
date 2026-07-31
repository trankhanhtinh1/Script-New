#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Nilah = [] {
    ChampionProfile p{};
    p.ChampionName = "Nilah";
    p.DisplayName = "Nilah";
    p.InternalId = "champion.kuroaio.ai.nilah";
    p.PrimaryArchetype = Archetype::Skirmisher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AllyTarget | Mechanic::AutoWeave | Mechanic::Dash |
                  Mechanic::Recast | Mechanic::SpellShield | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 425.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 45.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 35;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "NilahJoyUnending";
    p.ChannelBuff = "NilahR";
    p.UltimateBuff = "NilahR";
    p.TacticalSummary =
        "Close-range skirmisher: preserve the selected target through AA windup, use Formless Blade to empower attacks, hide Nilah and an ally with Jubilant Veil, and reserve Apotheosis for safe multi-target damage or healing.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Nilah review: Joy Unending shares allied healing and shielding and grants experience parity, Formless Blade empowers attacks and whips a line, Jubilant Veil evades attacks, Slipstream dashes through allies or enemies with a recast, and Apotheosis damages nearby enemies while healing Nilah and nearby allies.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Formless Blade", CastKind::Position,
        Intent::Damage | Intent::Buff | Intent::AutoReset | Intent::Setup |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        AllModes, 600.0f, 0.25f, 75.0f, 2200.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 91;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Jubilant Veil", CastKind::Self,
        Intent::Buff | Intent::Disengage | Intent::Peel | Intent::AntiGapcloser |
            Intent::AllyUtility,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 100;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Slipstream", CastKind::AnyTarget,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Recast | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        550.0f, 0.25f, 100.0f, 2200.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[2].Priority = 96;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Apotheosis", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Heal | Intent::Channel |
            Intent::Finisher | Intent::AllyUtility,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        450.0f, 0.25f, 450.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Circular);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 99;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("Q empower into evasive short trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::RequireNoCrowdControl | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup));
    p.AllIn = Plan("Q empowered AA, E chase and safe Apotheosis",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireSafePosition));
    p.Flee = Plan("Veil and ally-safe Slipstream disengage",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
