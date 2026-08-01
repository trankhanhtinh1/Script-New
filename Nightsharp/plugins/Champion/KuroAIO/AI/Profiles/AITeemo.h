#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Teemo = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Teemo;
    p.DisplayName = "Teemo";
    p.InternalId = "champion.kuroaio.ai.teemo";
    p.PrimaryArchetype = Archetype::Specialist;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::ObjectTracking | Mechanic::Trap | Mechanic::Ammo |
                  Mechanic::Mark | Mechanic::AutoWeave | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 500.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 38;
    p.AllowTurretDiveIfKillable = false;
    p.PassiveBuff = "TeemoPStealthBuff";
    p.MarkBuff = "teemopoison";
    p.TrackedObjectToken = "TeemoMushroom";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Blinding Dart", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Finisher | Intent::LastHit |
            Intent::Jungle,
        AllModes, 680.0f, 0.25f, 100.0f, 2500.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 28.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Move Quick", CastKind::Self,
        Intent::Buff | Intent::Mobility | Intent::Engage | Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        20.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 76;
    p.Spells[1].PreserveAutoAttack = false;
    p.Spells[1].PlayerHealthPercent = 70.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Toxic Shot", CastKind::Self,
        Intent::Damage | Intent::AutoWeave | Intent::Finisher | Intent::LastHit |
            Intent::Jungle | Intent::Waveclear,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::LastHit |
            Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 85;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = false;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Noxious Trap", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Vision | Intent::Setup |
            Intent::Disengage | Intent::Jungle | Intent::Objective,
        AllModes, 900.0f, 0.25f, 135.0f, 1000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 98;
    p.Spells[3].MinimumAmmo = 1;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].HarassManaPercent = 45.0f;
    p.Spells[3].ClearManaPercent = 30.0f;

    p.Trade = Plan("E-on-hit pressure, Q blind and reserve a mushroom",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack, 0, 650),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::SkipIfKillableWithout, 70, 850),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition, 150, 1100));
    p.AllIn = Plan("W chase into poison autos, Q blind and armed mushroom",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack, 45, 800),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 110, 950),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition, 180, 1200));
    p.Flee = Plan("Move Quick retreat with a defensive mushroom choke",
        Step(SDK::SpellSlot::W, StepRule::AllowDuringWindup, 0, 600),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 100, 900));

    p.PreferSelectedTarget = true;
    p.ProtectManualChannels = true;
    p.ThemeFrom = 0xFF6EBD55u;
    p.ThemeTo = 0xFFE3D14Cu;
    p.ThemeSpeed = 0.82f;
    p.TacticalSummary =
        "Use Toxic Shot as an on-hit poison state, blind the attack-dependent threat, "
        "use Move Quick for pursuit or escape, and spend mushroom charges only on "
        "armed vision chokepoints, objective approaches or lethal area damage.";
    p.ResearchSummary =
        "CommunityDragon 16.15 TeemoQ/W/E/R: Q is a 680-range 2500-speed targeted dart "
        "with blind; W is a 3-second self speed burst and passive speed; E is passive "
        "four-second poison on hit; R is a 900-range 160-radius ammo trap with 1-second arm, "
        "450-radius explosion and 210-radius vision.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
