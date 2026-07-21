#pragma once

// ============================================================================
// KuroAIO shared champion intelligence runtime.
//
// This engine deliberately assists the player's orbwalker intent instead of
// taking ownership of movement.  It runs one champion profile at a time and
// provides the safety/timing machinery every AI-prefixed champion receives:
// plan state, prediction, AA weaving, manual-input protection, safe mobility,
// reactive peel, interrupt, kill secure, farming and channel protection.
// ============================================================================

#include "AIChampionProfile.h"
#include "AIChampionController.h"
#include "../Helper/MenuHelper.h"
#include "../Helper/TargetHelper.h"
#include "../Helper/OrbwalkerModeHelper.h"
#include "../../../../Core/KuroCombatCoordinator.h"
#include "../../../../DebugLog.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::AI::Engine {

inline const ChampionProfile* ActiveProfile = nullptr;
inline const ChampionController* ActiveController = nullptr;
inline std::array<SDK::Spell*, 4> RuntimeSpells = {};
inline std::array<SpellSpec, 4> ResolvedSpecs = {};
inline std::array<std::string, 4> RuntimeSpellNames = {};
inline std::array<int, 4> LastSlotCastTick = {};

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* ClearMenu = nullptr;
inline Menu* AutomaticMenu = nullptr;
inline Menu* HumanMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline std::string MenuTitle;

inline bool Loaded = false;
inline int LastActionTick = 0;
inline int LastDecisionTick = 0;
inline int LastEngineRequestTick = 0;
inline int LastEngineRequestSlot = -1;
inline int LastManualSpellTick = 0;
inline int LastAfterAttackTick = 0;
inline int LastBeforeAttackTick = 0;
inline int LastTargetChangeTick = 0;
inline int LockedTargetNetworkId = 0;
inline int PlanStepIndex = 0;
inline int PlanStartedTick = 0;
inline int PlanLastStepTick = 0;
inline Mode ActivePlanMode = Mode::None;

inline int PendingGapcloserNetworkId = 0;
inline int PendingGapcloserTick = 0;
inline Vector3 PendingGapcloserEnd = {};
inline int PendingInterruptNetworkId = 0;
inline int PendingInterruptTick = 0;

struct TrackedObject {
    std::uint32_t NetworkId = 0;
    Vector3 Position = {};
    int LastSeenTick = 0;
};

inline std::array<TrackedObject, 64> TrackedObjects = {};
inline std::size_t TrackedObjectCount = 0;

inline constexpr float kInfiniteSpeed = FLT_MAX;
inline constexpr int kManualInputLockMs = 80;
inline constexpr int kReactiveWindowMs = 700;
inline constexpr int kPlanTargetGraceMs = 420;

inline int SlotIndex(SDK::SpellSlot slot) {
    switch (slot) {
    case SDK::SpellSlot::Q: return 0;
    case SDK::SpellSlot::W: return 1;
    case SDK::SpellSlot::E: return 2;
    case SDK::SpellSlot::R: return 3;
    default: return -1;
    }
}

inline SDK::SpellSlot SlotFromIndex(int index) {
    switch (index) {
    case 0: return SDK::SpellSlot::Q;
    case 1: return SDK::SpellSlot::W;
    case 2: return SDK::SpellSlot::E;
    case 3: return SDK::SpellSlot::R;
    default: return SDK::SpellSlot::Unknown;
    }
}

inline const char* SlotName(int index) {
    static constexpr const char* names[] = { "Q", "W", "E", "R" };
    return index >= 0 && index < 4 ? names[index] : "?";
}

inline bool TextContains(const char* value, const char* token) {
    if (!value || !token || !value[0] || !token[0]) {
        return false;
    }
    std::string left(value);
    std::string right(token);
    std::transform(left.begin(), left.end(), left.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::transform(right.begin(), right.end(), right.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return left.find(right) != std::string::npos;
}

inline float ClampPercent(float value) {
    return std::clamp(value, 0.0f, 100.0f);
}

inline float ManaPercent(const AIHeroClient& player) {
    if (!player.IsValid() || player.MaxMana() <= 1.0f) {
        return 100.0f;
    }
    return ClampPercent(player.Mana() * 100.0f / player.MaxMana());
}

inline bool IsHardCrowdControlled(const AIBaseClient& unit) {
    if (!unit.IsValid()) {
        return false;
    }
    return SDK::HasBuffOfType(unit, SDK::BuffType::Stun) ||
           SDK::HasBuffOfType(unit, SDK::BuffType::Snare) ||
           SDK::HasBuffOfType(unit, SDK::BuffType::Taunt) ||
           SDK::HasBuffOfType(unit, SDK::BuffType::Fear) ||
           SDK::HasBuffOfType(unit, SDK::BuffType::Charm) ||
           SDK::HasBuffOfType(unit, SDK::BuffType::Suppression) ||
           SDK::HasBuffOfType(unit, SDK::BuffType::Knockup) ||
           SDK::HasBuffOfType(unit, SDK::BuffType::Knockback) ||
           SDK::HasBuffOfType(unit, SDK::BuffType::Asleep);
}

inline bool IsPlayerCrowdControlled(const AIHeroClient& player) {
    return IsHardCrowdControlled(player) ||
           SDK::HasBuffOfType(player, SDK::BuffType::Silence) ||
           SDK::HasBuffOfType(player, SDK::BuffType::Polymorph);
}

inline Vector3 Extend(const Vector3& from, const Vector3& toward, float distance) {
    const float dx = toward.x - from.x;
    const float dz = toward.z - from.z;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length <= 1.0f || !std::isfinite(length)) {
        return from;
    }
    const float scale = distance / length;
    return { from.x + dx * scale, from.y, from.z + dz * scale };
}

inline float Distance2D(const Vector3& left, const Vector3& right) {
    return left.Distance2D(right);
}

inline bool ValidEnemy(const AIHeroClient& hero, float range = FLT_MAX) {
    return hero.IsValid() && !hero.IsDead() && hero.Health() > 0.0f &&
           hero.IsTargetable() && hero.IsVisible() &&
           SDK::Extensions::IsValidTarget(hero, range, true);
}

inline bool ValidAlly(const AIHeroClient& hero, float range = FLT_MAX) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !hero.IsValid() || hero.IsDead() ||
        hero.Health() <= 0.0f || !hero.IsTargetable() || hero.IsEnemy()) {
        return false;
    }
    return range == FLT_MAX || player.Position().DistanceSqr2D(hero.Position()) <= range * range;
}

inline AIHeroClient EnemyByNetworkId(int networkId) {
    if (networkId <= 0) {
        return {};
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (static_cast<int>(enemy.NetworkId()) == networkId && ValidEnemy(enemy)) {
            return enemy;
        }
    }
    return {};
}

inline int CountEnemiesAt(const Vector3& position, float range) {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidEnemy(enemy) && position.DistanceSqr2D(enemy.Position()) <= rangeSqr) {
            ++count;
        }
    }
    return count;
}

inline int CountAlliesAt(const Vector3& position, float range) {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (ValidAlly(ally) && position.DistanceSqr2D(ally.Position()) <= rangeSqr) {
            ++count;
        }
    }
    return count;
}

inline bool UnderEnemyTurret(const Vector3& position) {
    for (const auto& turret : GameObjects::EnemyTurrets()) {
        if (!turret.IsValid() || turret.IsDead()) {
            continue;
        }
        const float range = std::max(775.0f, turret.AttackRange()) + 65.0f;
        if (position.DistanceSqr2D(turret.Position()) <= range * range) {
            return true;
        }
    }
    return false;
}

inline float PositionDangerScore(const Vector3& position,
                                 const AIHeroClient& target,
                                 const SpellSpec& spec) {
    if (!position.IsValid() || position.IsZero() || SDK::NavMesh::IsWall(position)) {
        return -100000.0f;
    }

    const auto player = ObjectManager::Player();
    if (!player.IsValid()) {
        return -100000.0f;
    }

    const int enemies = CountEnemiesAt(position, 650.0f);
    const int allies = CountAlliesAt(position, 700.0f);
    const int maximumEnemies = std::max(
        1,
        std::min(spec.MaximumEnemiesAtDestination,
                 Slider(ComboMenu, "MaxCommitEnemies",
                        ActiveProfile ? ActiveProfile->MaximumCommitEnemies : 2)));
    if (enemies > maximumEnemies && player.HealthPercent() < 72.0f) {
        return -50000.0f;
    }

    const bool turret = UnderEnemyTurret(position);
    const bool allowDive = Bool(ComboMenu, "AllowTurretDive", false) &&
                           ActiveProfile && ActiveProfile->AllowTurretDiveIfKillable;
    if (turret && !allowDive) {
        return -40000.0f;
    }

    float score = static_cast<float>(allies * 165 - enemies * 235);
    if (target.IsValid()) {
        const float desired = spec.DesiredDistance > 1.0f
            ? spec.DesiredDistance
            : (ActiveProfile ? ActiveProfile->PreferredCombatDistance : 450.0f);
        score -= std::abs(position.Distance2D(target.Position()) - desired) * 0.72f;
    }
    score -= position.Distance2D(Game::CursorPos()) * 0.10f;
    score += player.HealthPercent() * 1.4f;
    if (turret) {
        score -= 900.0f;
    }
    return score;
}

inline Vector3 BestSafePosition(const SpellSpec& spec,
                                const AIHeroClient& target,
                                AimPolicy policy) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) {
        return {};
    }

    const float dash = spec.DashDistance > 1.0f
        ? spec.DashDistance
        : std::max(100.0f, spec.Range);
    const Vector3 cursor = Game::CursorPos();
    std::array<Vector3, 20> candidates = {};
    std::size_t count = 0;
    candidates[count++] = Extend(player.Position(), cursor, dash);

    if (target.IsValid()) {
        candidates[count++] = Extend(player.Position(), target.Position(), dash);
        candidates[count++] = Extend(target.Position(), player.Position(), dash);
    }

    constexpr float pi = 3.14159265358979323846f;
    for (int i = 0; i < 16 && count < candidates.size(); ++i) {
        const float angle = 2.0f * pi * static_cast<float>(i) / 16.0f;
        candidates[count++] = {
            player.Position().x + std::cos(angle) * dash,
            player.Position().y,
            player.Position().z + std::sin(angle) * dash
        };
    }

    Vector3 best = {};
    float bestScore = -FLT_MAX;
    for (std::size_t i = 0; i < count; ++i) {
        float score = PositionDangerScore(candidates[i], target, spec);
        if (policy == AimPolicy::AwayFromThreat) {
            score += candidates[i].Distance2D(
                target.IsValid() ? target.Position() : player.Position()) * 0.35f;
        } else if (policy == AimPolicy::Cursor || policy == AimPolicy::SafeCursor) {
            score -= candidates[i].Distance2D(cursor) * 0.35f;
        }
        if (score > bestScore) {
            bestScore = score;
            best = candidates[i];
        }
    }
    return bestScore > -10000.0f ? best : Vector3{};
}

inline Mode CurrentMode() {
    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo: return Mode::Combo;
    case OrbwalkingMode::Harass: return Mode::Harass;
    case OrbwalkingMode::LaneClear: return Mode::LaneClear;
    case OrbwalkingMode::LastHit: return Mode::LastHit;
    case OrbwalkingMode::Flee: return Mode::Flee;
    default: return Mode::None;
    }
}

inline bool ModeEnabled(const SpellSpec& spec, Mode mode) {
    return Has(spec.Modes, mode);
}

inline bool MenuSpellEnabled(Menu* menu, int index, bool fallback = true) {
    char key[16] = {};
    _snprintf_s(key, sizeof(key), _TRUNCATE, "Use%s", SlotName(index));
    return Bool(menu, key, fallback);
}

inline Menu* MenuForMode(Mode mode) {
    if (mode == Mode::Combo) return ComboMenu;
    if (mode == Mode::Harass) return HarassMenu;
    if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        return ClearMenu;
    }
    return AutomaticMenu;
}

inline bool ResourceOkay(const SpellSpec& spec, Mode mode) {
    if (!ActiveProfile || ActiveProfile->Resource == ResourceModel::None ||
        ActiveProfile->Resource == ResourceModel::Health) {
        return true;
    }
    const auto player = ObjectManager::Player();
    const float mana = ManaPercent(player);
    float required = 0.0f;
    if (mode == Mode::Combo) {
        required = spec.ComboManaPercent;
    } else if (mode == Mode::Harass) {
        required = std::max(
            spec.HarassManaPercent,
            static_cast<float>(Slider(HarassMenu, "Mana", 35)));
    } else if (mode == Mode::LaneClear || mode == Mode::Jungle || mode == Mode::LastHit) {
        required = std::max(
            spec.ClearManaPercent,
            static_cast<float>(Slider(ClearMenu, "Mana", 45)));
    }
    return mana >= required;
}

inline bool IsRuntimeRecast(int index) {
    if (!ActiveProfile || index < 0 || index >= 4) {
        return false;
    }
    const auto& spec = ResolvedSpecs[index];
    if (spec.RecastSpellName && spec.RecastSpellName[0] &&
        TextContains(RuntimeSpellNames[index].c_str(), spec.RecastSpellName)) {
        return true;
    }
    if (!Has(ActiveProfile->Mechanics, Mechanic::Recast)) {
        return false;
    }
    const int now = SDK::Variables::TickCount();
    return LastSlotCastTick[index] > 0 && now - LastSlotCastTick[index] < 8000 &&
           RuntimeSpells[index] && RuntimeSpells[index]->IsReady();
}

inline void DeleteRuntimeSpells() {
    for (auto*& spell : RuntimeSpells) {
        delete spell;
        spell = nullptr;
    }
    for (auto& name : RuntimeSpellNames) {
        name.clear();
    }
}

inline SpellSpec ResolveSpellSpec(int index, const std::string& runtimeName) {
    SpellSpec resolved = ActiveProfile->Spells[index];
    for (std::size_t i = 0; i < ActiveProfile->VariantCount &&
                            i < ActiveProfile->Variants.size(); ++i) {
        const auto& variant = ActiveProfile->Variants[i];
        if (SlotIndex(variant.Slot) != index || !variant.RuntimeNameToken ||
            !variant.RuntimeNameToken[0]) {
            continue;
        }
        if (TextContains(runtimeName.c_str(), variant.RuntimeNameToken)) {
            resolved = variant.Spec;
            resolved.Slot = variant.Slot;
            break;
        }
    }
    return resolved;
}

inline void ConfigureRuntimeSpell(int index, bool force = false) {
    if (!ActiveProfile || index < 0 || index >= 4) {
        return;
    }

    const auto player = ObjectManager::Player();
    if (!player.IsValid()) {
        return;
    }
    const auto instance = player.Spellbook().GetSpell(ActiveProfile->Spells[index].Slot);
    const std::string runtimeName = instance.IsValid() ? instance.Name() : std::string();
    if (!force && RuntimeSpells[index] && runtimeName == RuntimeSpellNames[index]) {
        return;
    }
    ResolvedSpecs[index] = ResolveSpellSpec(index, runtimeName);
    const auto& spec = ResolvedSpecs[index];

    delete RuntimeSpells[index];
    RuntimeSpells[index] = nullptr;
    RuntimeSpellNames[index] = runtimeName;

    float range = spec.Range > 1.0f ? spec.Range : FLT_MAX;
    const float nativeRange = instance.IsValid() ? instance.CastRange() : 0.0f;
    if (std::isfinite(nativeRange) && nativeRange > 1.0f && nativeRange < 10000.0f) {
        range = nativeRange;
    }

    auto* spell = new SDK::Spell(spec.Slot, range);
    spell->DamageType = spec.Damage;
    spell->MinHitChance = spec.Hitchance;

    float width = spec.Width;
    const float nativeLine = instance.IsValid() ? instance.LineWidth() : 0.0f;
    const float nativeRadius = instance.IsValid() ? instance.CastRadius() : 0.0f;
    if (std::isfinite(nativeLine) && nativeLine > 1.0f && nativeLine < 3000.0f) {
        width = nativeLine;
    } else if (std::isfinite(nativeRadius) && nativeRadius > 1.0f && nativeRadius < 3000.0f) {
        width = nativeRadius;
    }

    float speed = spec.Speed;
    const float nativeSpeed = instance.IsValid() ? instance.MissileSpeed() : 0.0f;
    if (std::isfinite(nativeSpeed) && nativeSpeed > 1.0f && nativeSpeed < 1000000.0f) {
        speed = nativeSpeed;
    }

    switch (spec.Kind) {
    case CastKind::Line:
    case CastKind::Circle:
    case CastKind::Cone:
    case CastKind::Direction:
    case CastKind::Position:
    case CastKind::Vector:
        spell->SetSkillshot(spec.Delay, width, speed, spec.Collision, spec.Shape);
        break;
    case CastKind::ChargedLine:
    case CastKind::ChargedCircle:
        spell->SetSkillshot(spec.Delay, width, speed, spec.Collision, spec.Shape);
        if (spec.ChargeBuffName && spec.ChargeBuffName[0] && spec.ChargeMaxRange > 0) {
            spell->SetCharged(
                spec.RecastSpellName && spec.RecastSpellName[0]
                    ? spec.RecastSpellName
                    : runtimeName,
                spec.ChargeBuffName,
                spec.ChargeMinRange,
                spec.ChargeMaxRange,
                std::max(0.1f, spec.ChargeDurationSeconds));
        }
        break;
    default:
        spell->SetTargetted(spec.Delay, speed);
        break;
    }
    RuntimeSpells[index] = spell;
}

inline void RefreshRuntimeSpells() {
    for (int index = 0; index < 4; ++index) {
        ConfigureRuntimeSpell(index, false);
    }
}

inline float MaximumTargetRange() {
    if (!ActiveProfile) {
        return 1000.0f;
    }
    float range = 500.0f;
    float mobilityReach = 0.0f;
    for (const auto& spec : ResolvedSpecs) {
        if (Has(spec.Intents, Intent::Mobility)) {
            mobilityReach = std::max(
                mobilityReach,
                spec.DashDistance > 0.0f ? spec.DashDistance : spec.Range);
        } else if (spec.Kind != CastKind::Self &&
            spec.Kind != CastKind::Toggle) {
            const int index = SlotIndex(spec.Slot);
            const float runtimeRange = index >= 0 && RuntimeSpells[index]
                ? RuntimeSpells[index]->CurrentRange()
                : spec.Range;
            range = std::max(range, std::min(runtimeRange, 5000.0f));
        }
    }
    return std::min(5500.0f, range + mobilityReach + 150.0f);
}

inline AIHeroClient SelectTarget(float range = -1.0f) {
    if (!ActiveProfile) {
        return {};
    }
    if (range <= 0.0f) {
        range = MaximumTargetRange();
    }

    if (ActiveProfile->PreferSelectedTarget && Bool(HumanMenu, "PreferSelected", true)) {
        if (auto* selector = SDK::TargetSelector::Instance()) {
            const auto selected = selector->GetSelectedTarget();
            if (ValidEnemy(selected, range)) {
                return selected;
            }
        }
    }

    const auto locked = EnemyByNetworkId(LockedTargetNetworkId);
    if (ValidEnemy(locked, range)) {
        return locked;
    }

    SDK::DamageType damage = SDK::DamageType::True;
    for (const auto& spec : ResolvedSpecs) {
        if (Has(spec.Intents, Intent::Damage)) {
            damage = spec.Damage;
            break;
        }
    }
    return Plugins::KuroAIO::GetTarget(range, damage);
}

inline float EstimatedDamage(const AIHeroClient& target, int excludedSlot = -1) {
    if (!ActiveProfile || !ValidEnemy(target)) {
        return 0.0f;
    }
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) {
        return 0.0f;
    }

    float damage = SDK::Damage::GetAutoAttackDamage(player, target, true) * 1.25f;
    for (int index = 0; index < 4; ++index) {
        if (index == excludedSlot || !RuntimeSpells[index] ||
            !RuntimeSpells[index]->IsReady() ||
            !Has(ResolvedSpecs[index].Intents, Intent::Damage)) {
            continue;
        }
        const float spellDamage = RuntimeSpells[index]->GetDamage(target);
        if (std::isfinite(spellDamage) && spellDamage > 0.0f) {
            damage += spellDamage;
        }
    }
    return damage;
}

inline bool CanAct(bool reactive = false) {
    const auto player = ObjectManager::Player();
    if (!Loaded || !ActiveProfile || !player.IsValid() || player.IsDead() ||
        player.Health() <= 0.0f || player.IsRecalling() || Game::IsChatOpen() ||
        !Game::IsFocused()) {
        return false;
    }
    const int now = SDK::Variables::TickCount();
    if (CoreEvadeState::AreSpellCastsBlocked(now) ||
        Plugins::KuroCombatCoordination::Coordinator::EvadeOwnsActions(now)) {
        return false;
    }
    if (!reactive && Bool(HumanMenu, "RespectManual", true) &&
        LastManualSpellTick > 0) {
        const int now2 = SDK::Variables::TickCount();
        const int lockMs = (Orbwalker::ActiveMode() == OrbwalkingMode::Combo)
            ? std::max(40, kManualInputLockMs / 2)
            : kManualInputLockMs;
        if (now2 - LastManualSpellTick < lockMs) {
            return false;
        }
    }
    if (IsPlayerCrowdControlled(player)) {
        return false;
    }
    return true;
}

inline bool ShouldPreserveAttack(const SpellSpec& spec, StepRule rules) {
    if (!spec.PreserveAutoAttack || Has(rules, StepRule::AllowDuringWindup)) {
        return false;
    }
    if (!Bool(HumanMenu, "PreserveAttacks", true)) {
        return false;
    }
    return Orbwalker::IsWindingUp() && Orbwalker::AttackCastDelayRemaining() > 25;
}

inline bool BuffRequirementsMet(const SpellSpec& spec, const AIHeroClient& target) {
    const auto player = ObjectManager::Player();
    if (spec.RequiredPlayerBuff && spec.RequiredPlayerBuff[0] &&
        !player.HasBuff(spec.RequiredPlayerBuff)) {
        return false;
    }
    if (spec.ForbiddenPlayerBuff && spec.ForbiddenPlayerBuff[0] &&
        player.HasBuff(spec.ForbiddenPlayerBuff)) {
        return false;
    }
    if (spec.RequiredTargetBuff && spec.RequiredTargetBuff[0] && target.IsValid() &&
        !target.HasBuff(spec.RequiredTargetBuff)) {
        return false;
    }
    return true;
}

inline bool UltimateAllowed(const SpellSpec& spec,
                            const AIHeroClient& target,
                            Mode mode,
                            bool manualAssist) {
    if (!ActiveProfile || spec.Slot != SDK::SpellSlot::R) {
        return true;
    }
    if (manualAssist || Key(AutomaticMenu, "ManualR", false)) {
        return true;
    }

    const auto player = ObjectManager::Player();
    const float targetHp = target.IsValid() ? target.HealthPercent() : 100.0f;
    const int enemies = CountEnemiesAt(player.Position(), std::max(500.0f, spec.TriggerRange));
    const float damage = target.IsValid() && RuntimeSpells[3]
        ? RuntimeSpells[3]->GetDamage(target)
        : 0.0f;

    switch (ActiveProfile->Ultimate) {
    case UltimatePolicy::DisabledByDefault:
    case UltimatePolicy::ManualAssist:
        return false;
    case UltimatePolicy::Execute:
        return target.IsValid() && damage > 0.0f && damage >= target.Health();
    case UltimatePolicy::AllIn:
        return mode == Mode::Combo && target.IsValid() &&
            (targetHp <= ActiveProfile->UltimateTargetHealthPercent ||
             enemies >= ActiveProfile->UltimateMinimumTargets ||
             player.HealthPercent() <= ActiveProfile->DefensiveHealthPercent);
    case UltimatePolicy::MultiTarget:
        return mode == Mode::Combo && enemies >= ActiveProfile->UltimateMinimumTargets;
    case UltimatePolicy::Defensive:
        return player.HealthPercent() <= ActiveProfile->DefensiveHealthPercent;
    case UltimatePolicy::SaveAlly:
        return false;
    case UltimatePolicy::GlobalExecute:
        return Bool(AutomaticMenu, "GlobalExecute", false) && target.IsValid() &&
               damage > 0.0f && damage >= target.Health();
    case UltimatePolicy::RecastControl:
        return IsRuntimeRecast(3) ||
               (mode == Mode::Combo && target.IsValid() &&
                targetHp <= ActiveProfile->UltimateTargetHealthPercent);
    default:
        return false;
    }
}

inline bool StepRulesMet(const ComboStep& step,
                         const SpellSpec& spec,
                         const AIHeroClient& target,
                         Mode mode) {
    const auto player = ObjectManager::Player();
    const int index = SlotIndex(step.Slot);
    if (Has(step.Rules, StepRule::RequireTarget) && !ValidEnemy(target)) {
        return false;
    }
    if (Has(step.Rules, StepRule::RequireCrowdControl) &&
        !IsHardCrowdControlled(target)) {
        return false;
    }
    if (Has(step.Rules, StepRule::RequireNoCrowdControl) &&
        IsHardCrowdControlled(target)) {
        return false;
    }
    if (Has(step.Rules, StepRule::RequireAfterAttack)) {
        const int now = SDK::Variables::TickCount();
        if (LastAfterAttackTick <= 0 || now - LastAfterAttackTick > 420) {
            return false;
        }
    }
    if (target.IsValid()) {
        const float aaRange = player.AttackRange() + player.BoundingRadius() +
                              target.BoundingRadius();
        const bool insideAa = player.Position().DistanceSqr2D(target.Position()) <= aaRange * aaRange;
        if (Has(step.Rules, StepRule::RequireOutsideAaRange) && insideAa) {
            return false;
        }
        if (Has(step.Rules, StepRule::RequireInsideAaRange) && !insideAa) {
            return false;
        }
        if (Has(step.Rules, StepRule::RequireTargetLow) &&
            target.HealthPercent() > step.TargetHealthPercent) {
            return false;
        }
        if (Has(step.Rules, StepRule::RequireMark) && ActiveProfile->MarkBuff &&
            ActiveProfile->MarkBuff[0] && !target.HasBuff(ActiveProfile->MarkBuff)) {
            return false;
        }
        if (Has(step.Rules, StepRule::RequireNoMark) && ActiveProfile->MarkBuff &&
            ActiveProfile->MarkBuff[0] && target.HasBuff(ActiveProfile->MarkBuff)) {
            return false;
        }
    }
    if (Has(step.Rules, StepRule::RequirePlayerLow) &&
        player.HealthPercent() > step.PlayerHealthPercent) {
        return false;
    }
    if (Has(step.Rules, StepRule::RequireMultiTarget) &&
        CountEnemiesAt(player.Position(), std::max(500.0f, spec.TriggerRange)) <
            step.MinimumNearbyEnemies) {
        return false;
    }
    if (Has(step.Rules, StepRule::RequireRecast) && !IsRuntimeRecast(index)) {
        return false;
    }
    if (Has(step.Rules, StepRule::RequireFirstCast) && IsRuntimeRecast(index)) {
        return false;
    }
    if (Has(step.Rules, StepRule::SkipIfKillableWithout) && target.IsValid() &&
        EstimatedDamage(target, index) >= target.Health()) {
        return false;
    }
    if (Has(step.Rules, StepRule::ManualAssistOnly) &&
        !Key(AutomaticMenu, "ManualR", false)) {
        return false;
    }
    if (!UltimateAllowed(spec, target, mode, false)) {
        return false;
    }
    return true;
}

inline Vector3 AimPosition(const SpellSpec& spec, const AIHeroClient& target) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) {
        return {};
    }

    switch (spec.Aim) {
    case AimPolicy::Cursor:
        return Extend(player.Position(), Game::CursorPos(),
                      spec.DashDistance > 0.0f ? spec.DashDistance : spec.Range);
    case AimPolicy::SafeCursor:
    case AimPolicy::AwayFromThreat:
        return BestSafePosition(spec, target, spec.Aim);
    case AimPolicy::BehindTarget:
        if (target.IsValid()) {
            const Vector3 predicted = RuntimeSpells[SlotIndex(spec.Slot)]
                ? RuntimeSpells[SlotIndex(spec.Slot)]->GetPrediction(target).GetUnitPosition()
                : target.Position();
            return Extend(player.Position(), predicted,
                          std::min(spec.Range, player.Position().Distance2D(predicted) + 85.0f));
        }
        break;
    case AimPolicy::BetweenPlayerAndTarget:
        if (target.IsValid()) {
            return Extend(player.Position(), target.Position(),
                          std::min(spec.Range, player.Position().Distance2D(target.Position()) * 0.62f));
        }
        break;
    case AimPolicy::SelfPosition:
        return player.Position();
    case AimPolicy::TargetPosition:
        return target.IsValid() ? target.Position() : Vector3{};
    default:
        break;
    }

    if (target.IsValid()) {
        const int index = SlotIndex(spec.Slot);
        if (index >= 0 && RuntimeSpells[index]) {
            return RuntimeSpells[index]->GetPrediction(target, spec.Aim == AimPolicy::BestAoe)
                .GetCastPosition();
        }
        return target.Position();
    }
    return {};
}

inline bool MarkSuccessfulCast(int index) {
    const int now = SDK::Variables::TickCount();
    LastActionTick = now;
    LastEngineRequestTick = now;
    LastEngineRequestSlot = index;
    LastSlotCastTick[index] = now;
    return true;
}

// Champion controllers use these narrowly-scoped cast primitives after their
// own mechanics/state checks.  The request is armed before calling Spell so a
// synchronous ProcessSpell event is still recognized as controller input.
inline void ArmControllerCast(int index) {
    LastEngineRequestTick = SDK::Variables::TickCount();
    LastEngineRequestSlot = index;
}

inline bool WasControllerCast(int index, int ownershipWindowMs = 650) {
    return LastEngineRequestSlot == index && LastEngineRequestTick > 0 &&
           SDK::Variables::TickCount() - LastEngineRequestTick <=
               std::max(0, ownershipWindowMs);
}

inline void CancelControllerCast(int index) {
    if (LastEngineRequestSlot == index) {
        LastEngineRequestSlot = -1;
        LastEngineRequestTick = 0;
    }
}

inline bool ControllerCastPosition(int index, const Vector3& position) {
    if (index < 0 || index >= 4 || !RuntimeSpells[index] ||
        !position.IsValid() || position.IsZero()) {
        return false;
    }
    ArmControllerCast(index);
    if (RuntimeSpells[index]->Cast(position)) {
        return MarkSuccessfulCast(index);
    }
    CancelControllerCast(index);
    return false;
}

inline bool ControllerCastVector(int index,
                                 const Vector3& start,
                                 const Vector3& end) {
    if (index < 0 || index >= 4 || !RuntimeSpells[index] ||
        !start.IsValid() || start.IsZero() ||
        !end.IsValid() || end.IsZero() ||
        start.DistanceSqr2D(end) <= 0.001f) {
        return false;
    }
    ArmControllerCast(index);
    if (RuntimeSpells[index]->Cast(start, end)) {
        return MarkSuccessfulCast(index);
    }
    CancelControllerCast(index);
    return false;
}

inline bool ControllerCastSelf(int index) {
    if (index < 0 || index >= 4 || !RuntimeSpells[index]) {
        return false;
    }
    ArmControllerCast(index);
    if (RuntimeSpells[index]->Cast()) {
        return MarkSuccessfulCast(index);
    }
    CancelControllerCast(index);
    return false;
}

inline bool ControllerCastUnit(int index, const AIBaseClient& target) {
    if (index < 0 || index >= 4 || !RuntimeSpells[index] || !target.IsValid()) {
        return false;
    }
    ArmControllerCast(index);
    if (RuntimeSpells[index]->CastOnUnit(target)) {
        return MarkSuccessfulCast(index);
    }
    CancelControllerCast(index);
    return false;
}

inline bool ControllerCastPredicted(int index,
                                    const AIBaseClient& target,
                                    SDK::HitChance chance) {
    if (index < 0 || index >= 4 || !RuntimeSpells[index] || !target.IsValid()) {
        return false;
    }
    ArmControllerCast(index);
    if (RuntimeSpells[index]->CastIfHitchanceMinimum(target, chance) ==
        SDK::CastStates::SuccessfullyCasted) {
        return MarkSuccessfulCast(index);
    }
    CancelControllerCast(index);
    return false;
}

inline bool TryCast(const SpellSpec& spec,
                    const AIHeroClient& target,
                    Mode mode,
                    StepRule rules = StepRule::None,
                    bool reactive = false,
                    bool manualAssist = false) {
    const int index = SlotIndex(spec.Slot);
    if (index < 0 || !RuntimeSpells[index] || !CanAct(reactive) ||
        !RuntimeSpells[index]->IsReady() || !ModeEnabled(spec, mode) ||
        !MenuSpellEnabled(MenuForMode(mode), index, true) ||
        !ResourceOkay(spec, mode) || !BuffRequirementsMet(spec, target) ||
        ShouldPreserveAttack(spec, rules)) {
        return false;
    }

    const auto player = ObjectManager::Player();
    const auto instance = player.Spellbook().GetSpell(spec.Slot);
    if (instance.IsValid() && instance.MaxAmmo() > 0 && instance.Ammo() < spec.MinimumAmmo) {
        return false;
    }

    if (spec.Slot == SDK::SpellSlot::R &&
        !UltimateAllowed(spec, target, mode, manualAssist)) {
        return false;
    }
    if (target.IsValid() && target.HealthPercent() > spec.TargetHealthPercent &&
        Has(spec.Intents, Intent::Execute)) {
        return false;
    }
    if (player.HealthPercent() > spec.PlayerHealthPercent &&
        (Has(spec.Intents, Intent::Heal) || Has(spec.Intents, Intent::Shield)) &&
        mode == Mode::Automatic) {
        return false;
    }

    const int now = SDK::Variables::TickCount();
    const bool isCombo = Orbwalker::ActiveMode() == OrbwalkingMode::Combo;
    const int humanizer = std::max(
        isCombo ? 15 : 20,
        (isCombo ? 0 : Slider(HumanMenu, "Humanizer",
               ActiveProfile ? ActiveProfile->BaseHumanizerMs : 65)) +
            spec.HumanizerExtraMs);
    if (!reactive && LastActionTick > 0 && now - LastActionTick < humanizer) {
        return false;
    }

    bool casted = false;
    switch (spec.Kind) {
    case CastKind::None:
        return false;
    case CastKind::Self:
    case CastKind::Toggle:
        if (!target.IsValid() || spec.TriggerRange <= 1.0f ||
            player.Position().DistanceSqr2D(target.Position()) <=
                spec.TriggerRange * spec.TriggerRange || reactive) {
            casted = RuntimeSpells[index]->Cast();
        }
        break;
    case CastKind::EnemyTarget:
        if (ValidEnemy(target, RuntimeSpells[index]->CurrentRange())) {
            casted = RuntimeSpells[index]->CastOnUnit(target);
        }
        break;
    case CastKind::AllyTarget:
    case CastKind::AnyTarget:
        if (target.IsValid()) {
            casted = RuntimeSpells[index]->CastOnUnit(target);
        }
        break;
    case CastKind::Line:
    case CastKind::Circle:
    case CastKind::Cone:
    case CastKind::Direction:
        if (ValidEnemy(target, RuntimeSpells[index]->CurrentRange() + 80.0f)) {
            SDK::HitChance chance = IsHardCrowdControlled(target)
                ? SDK::HitChance::Immobile
                : spec.Hitchance;
            if (isCombo && chance != SDK::HitChance::Immobile) {
                if (chance == SDK::HitChance::VeryHigh) {
                    chance = SDK::HitChance::High;
                } else if (chance == SDK::HitChance::High) {
                    chance = SDK::HitChance::Medium;
                }
            }
            const auto state = RuntimeSpells[index]->CastIfHitchanceMinimum(target, chance);
            casted = state == SDK::CastStates::SuccessfullyCasted;
        }
        break;
    case CastKind::Position: {
        const Vector3 position = Has(spec.Intents, Intent::Mobility)
            ? BestSafePosition(spec, target, spec.Aim)
            : AimPosition(spec, target);
        if (position.IsValid() && !position.IsZero() &&
            (!Has(rules, StepRule::RequireSafePosition) ||
             PositionDangerScore(position, target, spec) > -10000.0f)) {
            casted = RuntimeSpells[index]->Cast(position);
        }
        break;
    }
    case CastKind::Vector: {
        const Vector3 end = AimPosition(spec, target);
        Vector3 start = player.Position();
        if (target.IsValid() && spec.DesiredDistance > 1.0f) {
            start = Extend(player.Position(), target.Position(),
                           std::min(spec.Range, spec.DesiredDistance));
        }
        if (start.IsValid() && end.IsValid()) {
            casted = RuntimeSpells[index]->Cast(start, end);
        }
        break;
    }
    case CastKind::ChargedLine:
    case CastKind::ChargedCircle:
        if (ValidEnemy(target, RuntimeSpells[index]->CurrentRange() + 100.0f)) {
            if (RuntimeSpells[index]->IsCharging()) {
                const auto prediction = RuntimeSpells[index]->GetPrediction(target);
                SDK::HitChance requiredChance = spec.Hitchance;
                if (isCombo) {
                    if (requiredChance == SDK::HitChance::VeryHigh) {
                        requiredChance = SDK::HitChance::High;
                    } else if (requiredChance == SDK::HitChance::High) {
                        requiredChance = SDK::HitChance::Medium;
                    }
                }
                if (IsHardCrowdControlled(target) ||
                    static_cast<int>(prediction.Hitchance) >= static_cast<int>(requiredChance)) {
                    casted = RuntimeSpells[index]->ShootChargedSpell(
                        prediction.GetCastPosition());
                }
            } else {
                casted = RuntimeSpells[index]->StartCharging(AimPosition(spec, target));
            }
        }
        break;
    }

    return casted ? MarkSuccessfulCast(index) : false;
}

inline const ComboPlan* PlanForMode(Mode mode) {
    if (!ActiveProfile) return nullptr;
    if (mode == Mode::Combo) return &ActiveProfile->AllIn;
    if (mode == Mode::Harass) return &ActiveProfile->Trade;
    if (mode == Mode::Flee) return &ActiveProfile->Flee;
    return nullptr;
}

inline void ResetPlan(Mode mode = Mode::None, int targetNetworkId = 0) {
    PlanStepIndex = 0;
    PlanStartedTick = SDK::Variables::TickCount();
    PlanLastStepTick = 0;
    ActivePlanMode = mode;
    LockedTargetNetworkId = targetNetworkId;
    LastTargetChangeTick = PlanStartedTick;
}

inline bool TryPlan(Mode mode, const AIHeroClient& target) {
    const ComboPlan* plan = PlanForMode(mode);
    if (!plan || plan->Count == 0) {
        return false;
    }
    const int now = SDK::Variables::TickCount();
    const int targetId = target.IsValid() ? static_cast<int>(target.NetworkId()) : 0;
    if (ActivePlanMode != mode ||
        (targetId != 0 && LockedTargetNetworkId != 0 && targetId != LockedTargetNetworkId) ||
        (PlanStartedTick > 0 && now - PlanStartedTick > plan->ResetAfterMs)) {
        ResetPlan(mode, targetId);
    } else if (LockedTargetNetworkId == 0 && targetId != 0) {
        LockedTargetNetworkId = targetId;
    }

    if (PlanStepIndex < 0 || static_cast<std::size_t>(PlanStepIndex) >= plan->Count) {
        if (PlanLastStepTick > 0 && now - PlanLastStepTick > 260) {
            ResetPlan(mode, targetId);
        }
        return false;
    }

    for (int skips = 0; skips < 4 && static_cast<std::size_t>(PlanStepIndex) < plan->Count; ++skips) {
        const ComboStep& step = plan->Steps[PlanStepIndex];
        const int index = SlotIndex(step.Slot);
        if (index < 0 || !RuntimeSpells[index] || RuntimeSpells[index]->Level() <= 0 ||
            !MenuSpellEnabled(MenuForMode(mode), index, true)) {
            ++PlanStepIndex;
            continue;
        }

        const int elapsed = PlanLastStepTick > 0
            ? now - PlanLastStepTick
            : now - PlanStartedTick;
        if (elapsed < step.EarliestMs) {
            return false;
        }
        if (!StepRulesMet(step, ResolvedSpecs[index], target, mode)) {
            if (elapsed >= step.ExpireMs ||
                Has(step.Rules, StepRule::SkipIfKillableWithout)) {
                ++PlanStepIndex;
                continue;
            }
            return false;
        }
        if (!RuntimeSpells[index]->IsReady()) {
            if (elapsed >= step.ExpireMs) {
                ++PlanStepIndex;
                continue;
            }
            return false;
        }

        if (TryCast(ResolvedSpecs[index], target, mode, step.Rules)) {
            ++PlanStepIndex;
            PlanLastStepTick = now;
            return true;
        }
        return false;
    }
    return false;
}

inline bool TryFallbackCombat(Mode mode, const AIHeroClient& target) {
    if (!ActiveProfile || !ValidEnemy(target)) {
        return false;
    }
    std::array<int, 4> order = { 0, 1, 2, 3 };
    std::sort(order.begin(), order.end(), [](int left, int right) {
        return ResolvedSpecs[left].Priority > ResolvedSpecs[right].Priority;
    });
    for (const int index : order) {
        const auto& spec = ResolvedSpecs[index];
        if (ModeEnabled(spec, mode) && TryCast(spec, target, mode)) {
            return true;
        }
    }
    return false;
}

inline AIHeroClient LowestHealthAlly(float range) {
    AIHeroClient best = {};
    float bestScore = FLT_MAX;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!ValidAlly(ally, range)) {
            continue;
        }
        const float pressure = static_cast<float>(ally.CountEnemyHeroesInRange(750.0f));
        const float score = ally.HealthPercent() - pressure * 12.0f;
        if (score < bestScore) {
            bestScore = score;
            best = ally;
        }
    }
    return best;
}

inline bool TryEmergencyDefense() {
    if (!ActiveProfile || !Bool(AutomaticMenu, "EmergencyDefense", true)) {
        return false;
    }
    const auto player = ObjectManager::Player();
    if (player.HealthPercent() > ActiveProfile->DefensiveHealthPercent ||
        CountEnemiesAt(player.Position(), 850.0f) <= 0) {
        return false;
    }
    for (int index = 0; index < 4; ++index) {
        const auto& spec = ResolvedSpecs[index];
        if (Has(spec.Intents, Intent::Heal) || Has(spec.Intents, Intent::Shield) ||
            Has(spec.Intents, Intent::Cleanse) || Has(spec.Intents, Intent::Disengage)) {
            const auto threat = SelectTarget(1000.0f);
            if (TryCast(spec, threat, Mode::Automatic, StepRule::None, true)) {
                return true;
            }
        }
    }
    return false;
}

inline bool TrySaveAlly() {
    if (!ActiveProfile || !Bool(AutomaticMenu, "SaveAllies", true)) {
        return false;
    }
    for (int index = 0; index < 4; ++index) {
        const auto& spec = ResolvedSpecs[index];
        if (spec.Kind != CastKind::AllyTarget && spec.Kind != CastKind::AnyTarget &&
            !Has(spec.Intents, Intent::AllyUtility)) {
            continue;
        }
        const auto ally = LowestHealthAlly(std::max(500.0f, spec.Range));
        if (!ally.IsValid() || ally.HealthPercent() > 58.0f ||
            ally.CountEnemyHeroesInRange(800.0f) <= 0) {
            continue;
        }
        if (TryCast(spec, AIHeroClient(ally.Handle()), Mode::Automatic,
                    StepRule::None, true)) {
            return true;
        }
    }
    return false;
}

inline bool TryPendingInterrupt() {
    const int now = SDK::Variables::TickCount();
    if (!ActiveProfile || !Bool(AutomaticMenu, "Interrupt", true) ||
        PendingInterruptNetworkId <= 0 || PendingInterruptTick <= 0 ||
        now - PendingInterruptTick > kReactiveWindowMs) {
        return false;
    }
    const auto target = EnemyByNetworkId(PendingInterruptNetworkId);
    if (!ValidEnemy(target)) {
        return false;
    }
    for (int index = 0; index < 4; ++index) {
        const auto& spec = ResolvedSpecs[index];
        if ((Has(spec.Intents, Intent::Interrupt) ||
             Has(spec.Intents, Intent::CrowdControl)) &&
            TryCast(spec, target, Mode::Automatic, StepRule::None, true)) {
            PendingInterruptNetworkId = 0;
            return true;
        }
    }
    return false;
}

inline bool TryPendingGapcloser() {
    const int now = SDK::Variables::TickCount();
    if (!ActiveProfile || !Bool(AutomaticMenu, "AntiGapcloser", true) ||
        PendingGapcloserNetworkId <= 0 || PendingGapcloserTick <= 0 ||
        now - PendingGapcloserTick > kReactiveWindowMs) {
        return false;
    }
    const auto target = EnemyByNetworkId(PendingGapcloserNetworkId);
    if (!ValidEnemy(target, 1100.0f)) {
        return false;
    }
    for (int index = 0; index < 4; ++index) {
        const auto& spec = ResolvedSpecs[index];
        if (!(Has(spec.Intents, Intent::AntiGapcloser) ||
              Has(spec.Intents, Intent::Disengage) ||
              Has(spec.Intents, Intent::Peel))) {
            continue;
        }
        if (TryCast(spec, target, Mode::Automatic,
                    StepRule::RequireSafePosition, true)) {
            PendingGapcloserNetworkId = 0;
            return true;
        }
    }
    return false;
}

inline bool TryKillSecure() {
    if (!ActiveProfile || !Bool(AutomaticMenu, "KillSecure", true)) {
        return false;
    }
    std::array<int, 4> order = { 0, 1, 2, 3 };
    std::sort(order.begin(), order.end(), [](int left, int right) {
        const bool leftExecute = Has(ResolvedSpecs[left].Intents, Intent::Execute);
        const bool rightExecute = Has(ResolvedSpecs[right].Intents, Intent::Execute);
        if (leftExecute != rightExecute) return leftExecute;
        return ResolvedSpecs[left].Priority > ResolvedSpecs[right].Priority;
    });
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidEnemy(enemy, MaximumTargetRange())) {
            continue;
        }
        for (const int index : order) {
            const auto& spec = ResolvedSpecs[index];
            if (!Has(spec.Intents, Intent::Damage) || !RuntimeSpells[index] ||
                !RuntimeSpells[index]->IsReady() || spec.Slot == SDK::SpellSlot::R &&
                    ActiveProfile->Ultimate == UltimatePolicy::ManualAssist) {
                continue;
            }
            const float damage = RuntimeSpells[index]->GetDamage(enemy);
            if (!std::isfinite(damage) || damage <= 0.0f ||
                damage * 0.94f < enemy.Health()) {
                continue;
            }
            if (TryCast(spec, enemy, Mode::Automatic, StepRule::None, false)) {
                return true;
            }
        }
    }
    return false;
}

inline std::vector<AIBaseClient> ClearUnits(bool jungle) {
    std::vector<AIBaseClient> result;
    if (jungle) {
        for (const auto& minion : GameObjects::Jungle()) {
            if (minion.IsValid() && !minion.IsDead() && minion.Health() > 0.0f &&
                minion.IsTargetable()) {
                result.emplace_back(minion.Handle());
            }
        }
    } else {
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (minion.IsValid() && !minion.IsDead() && minion.Health() > 0.0f &&
                minion.IsTargetable()) {
                result.emplace_back(minion.Handle());
            }
        }
    }
    return result;
}

inline bool TryFarmSpell(int index, bool jungle, bool lastHitOnly) {
    if (!ActiveProfile || index < 0 || index >= 4 || !RuntimeSpells[index]) {
        return false;
    }
    const auto& spec = ResolvedSpecs[index];
    const Mode mode = jungle ? Mode::Jungle : (lastHitOnly ? Mode::LastHit : Mode::LaneClear);
    if (!ModeEnabled(spec, mode) || !MenuSpellEnabled(ClearMenu, index, true) ||
        !ResourceOkay(spec, mode) || !RuntimeSpells[index]->IsReady() ||
        !Has(spec.Intents, Intent::Damage) ||
        (!Has(spec.Intents, Intent::Waveclear) && !jungle)) {
        return false;
    }

    auto units = ClearUnits(jungle);
    if (units.empty()) {
        return false;
    }
    const int minimumHits = jungle ? 1 : std::max(
        spec.MinimumAoeTargets,
        Slider(ClearMenu, "MinHits", 3));

    if (spec.Kind == CastKind::Self || spec.Kind == CastKind::Toggle) {
        const auto player = ObjectManager::Player();
        int nearby = 0;
        for (const auto& unit : units) {
            if (player.Position().DistanceSqr2D(unit.Position()) <=
                spec.TriggerRange * spec.TriggerRange) {
                ++nearby;
            }
        }
        if (nearby >= minimumHits) {
            return TryCast(spec, {}, mode);
        }
        return false;
    }

    if (spec.Kind == CastKind::EnemyTarget || spec.Kind == CastKind::AnyTarget) {
        for (const auto& unit : units) {
            if (!SDK::Extensions::IsValidTarget(unit, RuntimeSpells[index]->CurrentRange())) {
                continue;
            }
            const float damage = RuntimeSpells[index]->GetDamage(unit);
            const float health = RuntimeSpells[index]->GetHealthPrediction(unit);
            if (jungle || !lastHitOnly || damage >= health) {
                if (RuntimeSpells[index]->CastOnUnit(unit)) {
                    return MarkSuccessfulCast(index);
                }
            }
        }
        return false;
    }

    if (lastHitOnly) {
        for (const auto& unit : units) {
            if (!SDK::Extensions::IsValidTarget(unit, RuntimeSpells[index]->CurrentRange())) {
                continue;
            }
            const float damage = RuntimeSpells[index]->GetDamage(unit);
            const float health = RuntimeSpells[index]->GetHealthPrediction(unit);
            if (damage > 0.0f && damage >= health) {
                const auto state = RuntimeSpells[index]->CastIfHitchanceMinimum(
                    unit, SDK::HitChance::High);
                if (state == SDK::CastStates::SuccessfullyCasted) {
                    return MarkSuccessfulCast(index);
                }
            }
        }
        return false;
    }

    SDK::Utils::FarmLocation farm = {};
    if (spec.Kind == CastKind::Circle || spec.Kind == CastKind::Position) {
        farm = RuntimeSpells[index]->GetCircularFarmLocation(units);
    } else {
        farm = RuntimeSpells[index]->GetLineFarmLocation(units);
    }
    if (farm.MinionsHit >= minimumHits && farm.Position.IsValid() &&
        RuntimeSpells[index]->Cast(Vector3::From2D(farm.Position))) {
        return MarkSuccessfulCast(index);
    }
    return false;
}

inline bool TryFarm(Mode mode) {
    const bool lastHit = mode == Mode::LastHit;
    const bool jungle = !lastHit && !ClearUnits(true).empty() && ClearUnits(false).empty();
    for (int index = 0; index < 4; ++index) {
        if (TryFarmSpell(index, jungle, lastHit)) {
            return true;
        }
    }
    return false;
}

inline bool TryManualUltimate() {
    if (!ActiveProfile || !Key(AutomaticMenu, "ManualR", false)) {
        return false;
    }
    const auto target = SelectTarget(std::max(1500.0f, ResolvedSpecs[3].Range));
    return TryCast(ResolvedSpecs[3], target, Mode::Automatic,
                   StepRule::None, false, true);
}

inline void PruneTrackedObjects() {
    const int now = SDK::Variables::TickCount();
    std::size_t write = 0;
    for (std::size_t i = 0; i < TrackedObjectCount; ++i) {
        if (TrackedObjects[i].NetworkId != 0 && now - TrackedObjects[i].LastSeenTick <= 15000) {
            TrackedObjects[write++] = TrackedObjects[i];
        }
    }
    for (std::size_t i = write; i < TrackedObjectCount; ++i) {
        TrackedObjects[i] = {};
    }
    TrackedObjectCount = write;
}

inline void Game_OnUpdate(const SDK::Events::GameUpdateEventArgs&) {
    if (!Loaded || !ActiveProfile) {
        return;
    }
    RefreshRuntimeSpells();
    PruneTrackedObjects();
    if (!CanAct(false)) {
        return;
    }

    const int now = SDK::Variables::TickCount();
    const int decisionDelay = (Orbwalker::ActiveMode() == OrbwalkingMode::Combo)
        ? 10
        : std::max(15, Slider(HumanMenu, "DecisionRate", 28));
    if (LastDecisionTick > 0 && now - LastDecisionTick < decisionDelay) {
        return;
    }
    LastDecisionTick = now;

    const Mode mode = CurrentMode();
    const auto controllerTarget = SelectTarget();
    if (ActiveController && ActiveController->OnUpdate) {
        const bool handled = ActiveController->OnUpdate(mode, controllerTarget);
        if (handled || ActiveController->OwnsDecisionLoop) {
            return;
        }
    } else if (ActiveController && ActiveController->OwnsDecisionLoop) {
        return;
    }

    if (TryEmergencyDefense() || TryPendingInterrupt() || TryPendingGapcloser() ||
        TrySaveAlly() || TryManualUltimate() || TryKillSecure()) {
        return;
    }

    if (mode == Mode::Combo || mode == Mode::Harass || mode == Mode::Flee) {
        const auto target = SelectTarget();
        if (TryPlan(mode, target)) {
            return;
        }
        (void)TryFallbackCombat(mode, target);
        return;
    }
    if (mode == Mode::LaneClear || mode == Mode::LastHit) {
        (void)TryFarm(mode);
    }
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!Loaded || !ActiveProfile) {
        return;
    }
    if (ActiveController && ActiveController->OnProcessSpell) {
        ActiveController->OnProcessSpell(args);
    }
    if (!args.Sender.IsValid()) {
        return;
    }
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || args.Sender.NetworkId != player.NetworkId()) {
        return;
    }
    const int index = args.Slot >= 0 && args.Slot < 4 ? args.Slot : -1;
    if (index < 0) {
        return;
    }
    const int now = SDK::Variables::TickCount();
    const bool ours = WasControllerCast(index);
    if (!ours) {
        LastManualSpellTick = now;
        if (Bool(HumanMenu, "ManualResetsPlan", true)) {
            ResetPlan(CurrentMode(), LockedTargetNetworkId);
        }
    }
    LastSlotCastTick[index] = now;
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!Loaded) {
        return;
    }
    if (ActiveController && ActiveController->OnDoCast) {
        ActiveController->OnDoCast(args);
    }
    if (!args.Sender.IsValid() || !args.IsAutoAttack) {
        return;
    }
    const auto player = ObjectManager::Player();
    if (player.IsValid() && args.Sender.NetworkId == player.NetworkId()) {
        LastAfterAttackTick = SDK::Variables::TickCount();
    }
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!Loaded || !ActiveProfile) {
        return;
    }
    LastBeforeAttackTick = SDK::Variables::TickCount();
    if (ActiveProfile->ProtectManualChannels) {
        const auto player = ObjectManager::Player();
        const bool channelBuff = ActiveProfile->ChannelBuff && ActiveProfile->ChannelBuff[0] &&
                                 player.HasBuff(ActiveProfile->ChannelBuff);
        if (player.Spellbook().IsChanneling() || channelBuff) {
            args.Process = false;
        }
    }
    if (ActiveController && ActiveController->OnBeforeAttack) {
        ActiveController->OnBeforeAttack(args);
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs&) {
    if (!Loaded) {
        return;
    }
    LastAfterAttackTick = SDK::Variables::TickCount();
}

inline void OnAfterAttackRelay(SDK::OrbwalkingActionArgs& args) {
    OnAfterAttack(args);
    if (ActiveController && ActiveController->OnAfterAttack) {
        ActiveController->OnAfterAttack(args);
    }
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (!Loaded || !args.IsDirectedToPlayer) {
        return;
    }
    PendingGapcloserNetworkId = static_cast<int>(args.NetworkId);
    PendingGapcloserEnd = args.End;
    PendingGapcloserTick = SDK::Variables::TickCount();
    if (ActiveController && ActiveController->OnGapcloser) {
        ActiveController->OnGapcloser(args);
    }
}

inline void OnInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    if (!Loaded) {
        return;
    }
    PendingInterruptNetworkId = static_cast<int>(args.NetworkId);
    PendingInterruptTick = SDK::Variables::TickCount();
    if (ActiveController && ActiveController->OnInterruptable) {
        ActiveController->OnInterruptable(args);
    }
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (Loaded && ActiveController && ActiveController->OnBuffAdd) {
        ActiveController->OnBuffAdd(args);
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Loaded && ActiveController && ActiveController->OnBuffRemove) {
        ActiveController->OnBuffRemove(args);
    }
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    if (Loaded && ActiveController && ActiveController->OnBuffUpdate) {
        ActiveController->OnBuffUpdate(args);
    }
}

inline bool ObjectMatchesProfile(const SDK::Events::ObjectEventArgs& args) {
    if (!ActiveProfile || !ActiveProfile->TrackedObjectToken ||
        !ActiveProfile->TrackedObjectToken[0]) {
        return false;
    }
    const char* token = ActiveProfile->TrackedObjectToken;
    return TextContains(args.Sender.Name, token) ||
           TextContains(args.Sender.CharacterName, token) ||
           TextContains(args.SpellName, token) ||
           TextContains(args.MissileName, token);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
    if (!Loaded) {
        return;
    }
    if (ActiveController && ActiveController->OnObjectCreate) {
        ActiveController->OnObjectCreate(args);
    }
    if (!ObjectMatchesProfile(args) || TrackedObjectCount >= TrackedObjects.size()) {
        return;
    }
    TrackedObjects[TrackedObjectCount++] = {
        args.Sender.NetworkId,
        args.Sender.Position,
        SDK::Variables::TickCount()
    };
}

inline void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
    if (!Loaded) {
        return;
    }
    if (ActiveController && ActiveController->OnObjectDelete) {
        ActiveController->OnObjectDelete(args);
    }
    if (args.Sender.NetworkId == 0) {
        return;
    }
    for (std::size_t i = 0; i < TrackedObjectCount; ++i) {
        if (TrackedObjects[i].NetworkId == args.Sender.NetworkId) {
            TrackedObjects[i].LastSeenTick = 0;
        }
    }
}

inline void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
    if (Loaded && ActiveController && ActiveController->OnMissileCreate) {
        ActiveController->OnMissileCreate(args);
    }
}

inline void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
    if (Loaded && ActiveController && ActiveController->OnMissileDelete) {
        ActiveController->OnMissileDelete(args);
    }
}

inline void OnDraw() {
    if (!Loaded || !ActiveProfile || !DrawMenu) {
        return;
    }
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    static constexpr std::uint32_t colors[] = {
        0xFF72D6FFu, 0xFFFFC857u, 0xFFFF6B8Au, 0xFFB388FFu
    };
    for (int index = 0; index < 4; ++index) {
        char key[16] = {};
        _snprintf_s(key, sizeof(key), _TRUNCATE, "Draw%s", SlotName(index));
        if (!Bool(DrawMenu, key, false) || !RuntimeSpells[index]) {
            continue;
        }
        const float range = RuntimeSpells[index]->CurrentRange();
        if (std::isfinite(range) && range > 20.0f && range < 5000.0f) {
            Drawing::DrawCircle(player.Position(), range, colors[index], 1.35f, 64);
        }
    }

    if (Bool(DrawMenu, "DrawTarget", false)) {
        const auto target = SelectTarget();
        if (target.IsValid()) {
            Drawing::DrawCircle(target.Position(), 85.0f, 0xFFFFFFFFu, 2.0f, 48);
        }
    }

    if (Bool(DrawMenu, "DrawPlan", false)) {
        Vec2 screen = {};
        if (Drawing::WorldToScreen(player.Position(), screen) && screen.IsValid()) {
            const ComboPlan* plan = PlanForMode(CurrentMode());
            char text[160] = {};
            if (plan && plan->Count > 0) {
                _snprintf_s(text, sizeof(text), _TRUNCATE,
                            "%s | %s step %d/%d",
                            ActiveProfile->DisplayName,
                            plan->Name,
                            std::min(PlanStepIndex + 1, static_cast<int>(plan->Count)),
                            static_cast<int>(plan->Count));
            } else {
                _snprintf_s(text, sizeof(text), _TRUNCATE,
                            "%s | cooperative AI ready",
                            ActiveProfile->DisplayName);
            }
            Drawing::DrawText(screen.x - 90.0f, screen.y - 88.0f,
                              0xFFFFFFFFu, text);
        }
    }
    if (ActiveController && ActiveController->OnDraw) {
        ActiveController->OnDraw();
    }
}

inline void BuildModeSpellToggles(Menu* menu, Mode mode, bool ultimateDefault) {
    if (!menu || !ActiveProfile) {
        return;
    }
    for (int index = 0; index < 4; ++index) {
        char key[16] = {};
        char label[96] = {};
        _snprintf_s(key, sizeof(key), _TRUNCATE, "Use%s", SlotName(index));
        _snprintf_s(label, sizeof(label), _TRUNCATE, "Use %s - %s",
                    SlotName(index), ResolvedSpecs[index].Name);
        const bool defaultValue = ModeEnabled(ResolvedSpecs[index], mode) &&
                                  (index != 3 || ultimateDefault);
        menu->Add(new MenuBool(key, label, defaultValue));
    }
}

inline void BuildMenu() {
    if (!ActiveProfile) {
        return;
    }
    MenuTitle = std::string("Kuro AI - ") + ActiveProfile->DisplayName;
    MenuRoot = new Menu(ActiveProfile->InternalId, MenuTitle.c_str(), true);
    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo"));
    BuildModeSpellToggles(ComboMenu, Mode::Combo, true);
    ComboMenu->Add(new MenuBool("FollowPlan", "Follow plan", true));
    ComboMenu->Add(new MenuSlider(
        "MaxCommitEnemies", "Max enemies for mobility",
        ActiveProfile->MaximumCommitEnemies, 1, 5));
    ComboMenu->Add(new MenuBool(
        "AllowTurretDive", "Allow turret dive",
        ActiveProfile->AllowTurretDiveIfKillable));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass"));
    BuildModeSpellToggles(HarassMenu, Mode::Harass, false);
    HarassMenu->Add(new MenuSlider("Mana", "Min mana (%)", 35, 0, 100));

    ClearMenu = MenuRoot->AddSubMenu(new Menu("Clear", "Clear"));
    BuildModeSpellToggles(ClearMenu, Mode::LaneClear, false);
    ClearMenu->Add(new MenuSlider("Mana", "Min mana (%)", 45, 0, 100));
    ClearMenu->Add(new MenuSlider("MinHits", "Min minions for AoE", 3, 1, 8));

    AutomaticMenu = MenuRoot->AddSubMenu(new Menu("Automatic", "Automatic"));
    BuildModeSpellToggles(AutomaticMenu, Mode::Automatic, false);
    AutomaticMenu->Add(new MenuBool("KillSecure", "Kill secure", true));
    AutomaticMenu->Add(new MenuBool("Interrupt", "Interrupt spells", true));
    AutomaticMenu->Add(new MenuBool("AntiGapcloser", "Anti gapcloser", true));
    AutomaticMenu->Add(new MenuBool("EmergencyDefense", "Emergency defense", true));
    AutomaticMenu->Add(new MenuBool("SaveAllies", "Save allies", true));
    AutomaticMenu->Add(new MenuBool("GlobalExecute", "Global execute", false));
    AutomaticMenu->Add(new MenuKeyBind(
        "ManualR", "Semi-manual ultimate", Keys::T, KeyBindType::Press));

    HumanMenu = MenuRoot->AddSubMenu(new Menu("Cooperation", "Cooperation"));
    HumanMenu->Add(new MenuBool("PreferSelected", "Prefer selected target", true));
    HumanMenu->Add(new MenuBool("RespectManual", "Respect manual cast", true));
    HumanMenu->Add(new MenuBool("ManualResetsPlan", "Re-plan on manual cast", true));
    HumanMenu->Add(new MenuBool("PreserveAttacks", "Preserve windup", true));
    HumanMenu->Add(new MenuSlider(
        "Humanizer", "Humanizer delay (ms)",
        ActiveProfile->BaseHumanizerMs, 20, 220));
    HumanMenu->Add(new MenuSlider(
        "DecisionRate", "Decision rate (ms)", 28, 10, 120));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Draw"));
    for (int index = 0; index < 4; ++index) {
        char key[16] = {};
        char label[48] = {};
        _snprintf_s(key, sizeof(key), _TRUNCATE, "Draw%s", SlotName(index));
        _snprintf_s(label, sizeof(label), _TRUNCATE, "Draw %s range", SlotName(index));
        DrawMenu->Add(new MenuBool(key, label, false));
    }
    DrawMenu->Add(new MenuBool("DrawTarget", "Draw target", false));
    DrawMenu->Add(new MenuBool("DrawPlan", "Draw plan", false));

    if (ActiveController && ActiveController->BuildMenu) {
        ActiveController->BuildMenu(MenuRoot);
    }

    MenuRoot->Attach();
}

inline void RemoveMenu() {
    if (!MenuRoot) {
        return;
    }
    if (auto* item = AutomaticMenu ? AutomaticMenu->Get<MenuKeyBind>("ManualR") : nullptr) {
        item->RemovePermashow();
    }
    MenuManager::Instance().Remove(MenuRoot);
    delete MenuRoot;
    MenuRoot = nullptr;
    ComboMenu = nullptr;
    HarassMenu = nullptr;
    ClearMenu = nullptr;
    AutomaticMenu = nullptr;
    HumanMenu = nullptr;
    DrawMenu = nullptr;
    MenuTitle.clear();
}

inline void OnGameLoad(const ChampionProfile& profile,
                       const ChampionController* controller = nullptr) {
    if (Loaded) {
        return;
    }
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || _stricmp(player.CharacterName().c_str(), profile.ChampionName) != 0) {
        return;
    }

    ActiveProfile = &profile;
    ActiveController = controller;
    ResolvedSpecs = profile.Spells;
    Loaded = true;
    LastActionTick = 0;
    LastDecisionTick = 0;
    LastEngineRequestTick = 0;
    LastEngineRequestSlot = -1;
    LastManualSpellTick = 0;
    LastAfterAttackTick = 0;
    LastBeforeAttackTick = 0;
    LastSlotCastTick.fill(0);
    TrackedObjects.fill({});
    TrackedObjectCount = 0;
    ResetPlan();

    for (int index = 0; index < 4; ++index) {
        ConfigureRuntimeSpell(index, true);
    }
    BuildMenu();

    if (ActiveController && ActiveController->OnLoad) {
        ActiveController->OnLoad();
    }

    SDK::Events::hook.OnGameUpdate += &Game_OnUpdate;
    SDK::Events::hook.OnProcessSpell += &OnProcessSpell;
    SDK::Events::hook.OnDoCast += &OnDoCast;
    SDK::Events::hook.OnGapCloser += &OnGapcloser;
    SDK::Events::hook.OnInterruptableTarget += &OnInterruptable;
    SDK::Events::hook.OnCreateObject += &OnObjectCreate;
    SDK::Events::hook.OnDeleteObject += &OnObjectDelete;
    SDK::Events::hook.OnMissileCreate += &OnMissileCreate;
    SDK::Events::hook.OnMissileDelete += &OnMissileDelete;
    SDK::Events::hook.OnBuffAdd += &OnBuffAdd;
    SDK::Events::hook.OnBuffRemove += &OnBuffRemove;
    SDK::Events::hook.OnBuffUpdate += &OnBuffUpdate;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Orbwalker::OnAfterAttack += &OnAfterAttackRelay;
    Drawing::OnDraw += &OnDraw;

    NightSharpDebug::Logf(
        "[KuroAIO/AI] loaded champion=%s id=%s tactics=%s",
        profile.ChampionName, profile.InternalId, profile.TacticalSummary);
}

inline void OnUnload() {
    if (!Loaded) {
        return;
    }
    SDK::Events::hook.OnGameUpdate -= &Game_OnUpdate;
    SDK::Events::hook.OnProcessSpell -= &OnProcessSpell;
    SDK::Events::hook.OnDoCast -= &OnDoCast;
    SDK::Events::hook.OnGapCloser -= &OnGapcloser;
    SDK::Events::hook.OnInterruptableTarget -= &OnInterruptable;
    SDK::Events::hook.OnCreateObject -= &OnObjectCreate;
    SDK::Events::hook.OnDeleteObject -= &OnObjectDelete;
    SDK::Events::hook.OnMissileCreate -= &OnMissileCreate;
    SDK::Events::hook.OnMissileDelete -= &OnMissileDelete;
    SDK::Events::hook.OnBuffAdd -= &OnBuffAdd;
    SDK::Events::hook.OnBuffRemove -= &OnBuffRemove;
    SDK::Events::hook.OnBuffUpdate -= &OnBuffUpdate;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Orbwalker::OnAfterAttack -= &OnAfterAttackRelay;
    Drawing::OnDraw -= &OnDraw;

    if (ActiveController && ActiveController->OnUnload) {
        ActiveController->OnUnload();
    }

    RemoveMenu();
    DeleteRuntimeSpells();
    ActiveProfile = nullptr;
    ActiveController = nullptr;
    ResolvedSpecs.fill({});
    Loaded = false;
    LockedTargetNetworkId = 0;
    PendingGapcloserNetworkId = 0;
    PendingInterruptNetworkId = 0;
    TrackedObjectCount = 0;
}

} // namespace Plugins::KuroAIO::AI::Engine