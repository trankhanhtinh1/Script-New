// ============================================================================
// Health.h - Health prediction ported from EnsoulSharp.SDK
// ----------------------------------------------------------------------------
// Source: EnsoulSharp.SDK/Core/Math/Prediction/HealthPredictionSDK.cs
// ============================================================================
#pragma once

#include "../../Core/Objects.h"
#include "../../Core/Variables.h"
#include "../../Core/Game.h"
#include "../../Core/CoreControl.h"
#include "../../Events/Events.h"
#include "../../Enumerations/HealthPredictionType.h"
#include "../../Utils/AutoAttack.h"
#include "../../Wrappers/Damages/DamageLibrary.h"
#include "../../Extensions/Unit.h"
#include "../../GameObjects/ObjectManager.h"

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

namespace SDK::Prediction::Health {

// ============================================================================
// AutoAttackDamageOverrideMod (matching DLL's Damage.AutoAttackDamageOverrideMod)
// Turret → minion damage uses % max HP formula instead of raw AD.
// ============================================================================
inline float AutoAttackDamageOverrideMod(const AIBaseClient& source,
                                         const AIMinionClient& minion,
                                         float amount) {
    float result = amount;
    MinionTypes minionType = minion.GetMinionType();

    if (HasFlag(minionType, MinionTypes::Melee) && !HasFlag(minionType, MinionTypes::Super)) {
        result = 0.45f * minion.MaxHealth();
    } else if (HasFlag(minionType, MinionTypes::Ranged) && !HasFlag(minionType, MinionTypes::Siege)) {
        result = 0.7f * minion.MaxHealth();
    } else if (HasFlag(minionType, MinionTypes::Siege)) {
        AITurretClient turret(source.Address());
        TurretType tt = SDK::Extensions::GetTurretType(turret);
        switch (tt) {
        case TurretType::TierOne:
            result = 0.14f * minion.MaxHealth();
            break;
        case TurretType::TierTwo:
            result = 0.11f * minion.MaxHealth();
            break;
        case TurretType::TierThree:
        case TurretType::TierFour:
            result = 0.08f * minion.MaxHealth();
            break;
        default:
            break;
        }
    } else if (HasFlag(minionType, MinionTypes::Super)) {
        result = 0.05f * minion.MaxHealth();
    }
    return result;
}

// ============================================================================
// GetAutoAttackDamage (matching DLL's Damage.GetAutoAttackDamage on AIBaseClient)
// Key difference from DamageLibrary: turret→minion uses AutoAttackDamageOverrideMod
// ============================================================================
inline float GetAutoAttackDamage(const AIBaseClient& source,
                                 const AIBaseClient& target) {
    if (!source.IsValid() || !target.IsValid()) return 0.0f;

    // Turret → minion: use override mod (% max HP formula)
    if (source.Type() == ::Core::Objects::ObjectType::AITurretClient
        && target.Type() == ::Core::Objects::ObjectType::AIMinionClient) {
        AITurretClient turret(source.Address());
        AIMinionClient minion(target.Address());
        return AutoAttackDamageOverrideMod(turret, minion, source.TotalAttackDamage());
    }

    // All other cases: delegate to DamageLibrary (handles minion→minion, hero, etc.)
    return DamageLibrary::GetAutoAttackDamage(source, target);
}

// ============================================================================
// PredictedDamage (matching DLL's PredictedDamage class)
// ============================================================================
struct PredictedDamage {
    int SourceNetworkId = 0;
    int TargetNetworkId = 0;
    int StartTick = 0;
    int ProjectileSpeed = 0;
    float AnimationTime = 0.0f;
    float TargetBoundingRadius = 0.0f;
    float Damage = 0.0f;
    float Delay = 0.0f;
    AIBaseClient Source;
    AIBaseClient Target;
    bool IsSkillshot = false;
    bool Processed = false;
};

// ============================================================================
// HealthPredictionSDK (matching DLL's HealthPredictionSDK class)
// ============================================================================
namespace detail {

inline std::unordered_map<int, PredictedDamage> ActiveAttacks;
inline bool Initialized = false;

// ── OnDoCast: register auto-attack damage ────────────────────────────────────
inline void OnDoCast(const Events::ProcessSpellEventArgs& args) {
    // Resolve sender from args.Sender
    AIBaseClient sender;
    if (args.Sender.IsValid()) {
        sender = AIBaseClient(args.Sender.Address());
    }

    if (!sender.IsValid() || !sender.IsAlly()) return;
    if (sender.Type() != ::Core::Objects::ObjectType::AIMinionClient
        && sender.Type() != ::Core::Objects::ObjectType::AITurretClient) return;

    // DLL: sender.IsValidTarget(3000f, checkTeam: false)
    // IsValidTarget checks: IsValid, !IsDead, !IsZombie, IsVisible, distance <= range
    if (sender.IsDead() || !sender.IsVisible()) return;
    const auto player = SDK::ObjectManager::Player();
    if (player.IsValid() && sender.Distance(player) > 3000.0f) return;

    // DLL: Orbwalker.IsAutoAttack(args.SData.Name)
    // NightSharp: args.IsAutoAttack is set by DecodeDoCast
    if (!args.IsAutoAttack) {
        // Also check by spell name as fallback
        std::string spellName(args.ScriptName);
        if (!SDK::Utils::AutoAttack::IsAutoAttack(spellName)) return;
    }

    // Resolve target
    if (args.TargetNetworkId == 0) return;
    AIMinionClient target = SDK::ObjectManager::GetUnitByNetworkId<AIMinionClient>(
        static_cast<int>(args.TargetNetworkId));
    if (!target.IsValid() || target.IsDead()) return;

    PredictedDamage pd;
    pd.StartTick = SDK::Variables::TickCount() - SDK::Game::Ping() / 2;
    pd.ProjectileSpeed = sender.IsMelee()
        ? std::numeric_limits<int>::max()
        : static_cast<int>(args.MissileSpeed);
    pd.AnimationTime = ::CoreControl::GetAttackDelay(sender.Address()) * 1000.0f
        - (sender.Type() == ::Core::Objects::ObjectType::AITurretClient ? 70.0f : 0.0f);
    pd.TargetBoundingRadius = sender.BoundingRadius();
    pd.Damage = GetAutoAttackDamage(sender, target);
    pd.Delay = ::CoreControl::GetAttackWindup(sender.Address()) * 1000.0f;
    pd.Processed = false;
    pd.Source = sender;
    pd.SourceNetworkId = sender.NetworkId();
    pd.Target = target;
    pd.TargetNetworkId = target.NetworkId();

    // Remove existing entry for this source, then add
    ActiveAttacks.erase(pd.SourceNetworkId);
    ActiveAttacks[pd.SourceNetworkId] = pd;
}

// ── OnStopCast: remove attack if missile destroyed ───────────────────────────
inline void OnStopCast(const Events::StopCastEventArgs& args) {
    if (!args.Sender.IsValid()) return;

    AIBaseClient owner(args.Sender.Address());
    if (!owner.IsValid() || !owner.IsAlly()) return;

    // DLL: args.KeepAnimationPlaying && args.DestroyMissile
    if (!args.KeepAnimationPlaying || !args.DestroyMissile) return;

    if (owner.IsMelee()) {
        ActiveAttacks.erase(owner.NetworkId());
    }
    ActiveAttacks.erase(owner.NetworkId());
}

// ── OnDelete: mark attack as processed when missile is destroyed ─────────────
inline void OnDelete(const Events::ObjectEventArgs& args) {
    // DLL: only process MissileClient deletions
    if (args.Sender.Type != ::Core::Objects::ObjectType::MissileClient) return;

    // DLL: missileClient.SpellCaster.NetworkId -> mark attack as processed
    // NightSharp: ObjectEventArgs has SourceNetworkId for missile caster
    if (args.SourceNetworkId == 0) return;

    auto it = detail::ActiveAttacks.find(static_cast<int>(args.SourceNetworkId));
    if (it != detail::ActiveAttacks.end()) {
        it->second.Processed = true;
    }
}

// ── OnProcessSpellCast: mark melee ally attacks as processed ─────────────────
inline void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;

    AIBaseClient sender(args.Sender.Address());
    if (!sender.IsValid() || !sender.IsAlly() || !sender.IsMelee()) return;

    auto it = ActiveAttacks.find(sender.NetworkId());
    if (it != ActiveAttacks.end()) {
        it->second.Processed = true;
    }
}

// ── OnUpdate: clean up old attacks ───────────────────────────────────────────
inline void OnUpdate(const Events::GameUpdateEventArgs&) {
    int now = SDK::Variables::TickCount();
    for (auto it = ActiveAttacks.begin(); it != ActiveAttacks.end(); ) {
        if (it->second.StartTick < now - 3000) {
            it = ActiveAttacks.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace detail

// ============================================================================
// Public API (matching DLL's HealthPredictionSDK public methods)
// ============================================================================

inline void Initialize() {
    if (detail::Initialized) return;
    detail::Initialized = true;

    Events::AddOnDoCast(&detail::OnDoCast);
    Events::AddOnStopCast(&detail::OnStopCast);
    Events::AddOnDeleteObject(&detail::OnDelete);
    Events::AddOnProcessSpell(&detail::OnProcessSpellCast);
    Events::AddOnGameUpdate(&detail::OnUpdate);
}

inline void Reset() {
    detail::ActiveAttacks.clear();
    detail::Initialized = false;
}

// ── GetPredictionDefault ─────────────────────────────────────────────────────
inline float GetPredictionDefault(const AIBaseClient& unit, int time, int delay = 70) {
    float totalDamage = 0.0f;
    int now = SDK::Variables::TickCount();

    for (auto& [key, pd] : detail::ActiveAttacks) {
        if (!pd.Target.Compare(unit)) continue;
        if (pd.Processed) continue;
        if (!pd.Source.IsValid() || pd.Source.IsDead()) continue;

        float landTime = static_cast<float>(pd.StartTick) + pd.Delay
            + 1000.0f * (pd.Source.IsMelee() ? 0.0f
                : std::max(0.0f, unit.Distance(pd.Source) - pd.TargetBoundingRadius)
                  / static_cast<float>(pd.ProjectileSpeed))
            + static_cast<float>(delay);

        if (landTime < static_cast<float>(now + time)) {
            totalDamage += pd.Damage;
        }
    }

    return unit.Health() - totalDamage;
}

// ── GetPredictionSimulated ───────────────────────────────────────────────────
inline float GetPredictionSimulated(const AIBaseClient& unit, int time) {
    float totalDamage = 0.0f;
    int now = SDK::Variables::TickCount();

    for (auto& [key, pd] : detail::ActiveAttacks) {
        if (!pd.Target.Compare(unit)) continue;
        if (!pd.Source.IsValid() || pd.Source.IsDead()) continue;

        int hitCount = 0;

        // DLL: (float)(GameTimeTickCount - 100) <= (float)StartTick + AnimationTime
        if (static_cast<float>(now - 100) <= static_cast<float>(pd.StartTick) + pd.AnimationTime) {
            int tick = pd.StartTick;
            int endTime = now + time;

            // DLL uses do-while (executes at least once)
            do {
                float travelTime = pd.Delay / 1000.0f
                    + (pd.Source.IsMelee() ? 0.0f
                        : std::max(0.0f, unit.Distance(pd.Source) - pd.TargetBoundingRadius)
                          / static_cast<float>(pd.ProjectileSpeed));

                if (tick >= now
                    && static_cast<float>(tick) + travelTime < static_cast<float>(endTime)) {
                    hitCount++;
                }

                tick += static_cast<int>(pd.AnimationTime);
            } while (tick < endTime);
        }

        totalDamage += static_cast<float>(hitCount) * pd.Damage;
    }

    return unit.Health() - totalDamage;
}

// ── GetPrediction (main entry) ───────────────────────────────────────────────
inline float GetPrediction(const AIBaseClient& unit,
                           int time,
                           int delay = 70,
                           HealthPredictionType type = HealthPredictionType::Default) {
    if (type != HealthPredictionType::Simulated) {
        return GetPredictionDefault(unit, time, delay);
    }
    return GetPredictionSimulated(unit, time);
}

// ── GetAggroTurret ───────────────────────────────────────────────────────────
inline AIBaseClient GetAggroTurret(const AIMinionClient& minion) {
    if (!minion.IsValid()) return AIBaseClient();

    for (auto& [key, pd] : detail::ActiveAttacks) {
        if (!pd.Source.IsValid()) continue;
        if (pd.Source.Type() != ::Core::Objects::ObjectType::AITurretClient) continue;
        if (pd.Target.Compare(minion)) {
            return pd.Source;
        }
    }
    return AIBaseClient();
}

// ── HasMinionAggro ───────────────────────────────────────────────────────────
inline bool HasMinionAggro(const AIMinionClient& minion) {
    if (!minion.IsValid()) return false;

    for (auto& [key, pd] : detail::ActiveAttacks) {
        if (!pd.Source.IsValid()) continue;
        if (pd.Source.Type() != ::Core::Objects::ObjectType::AIMinionClient) continue;
        if (pd.Target.Compare(minion)) return true;
    }
    return false;
}

// ── HasTurretAggro ───────────────────────────────────────────────────────────
inline bool HasTurretAggro(const AIMinionClient& minion) {
    if (!minion.IsValid()) return false;

    for (auto& [key, pd] : detail::ActiveAttacks) {
        if (!pd.Source.IsValid()) continue;
        if (pd.Source.Type() != ::Core::Objects::ObjectType::AITurretClient) continue;
        if (pd.Target.Compare(minion)) return true;
    }
    return false;
}

// ── TurretAggroStartTick ─────────────────────────────────────────────────────
inline int TurretAggroStartTick(const AIMinionClient& minion) {
    if (!minion.IsValid()) return 0;

    for (auto& [key, pd] : detail::ActiveAttacks) {
        if (!pd.Source.IsValid()) continue;
        if (pd.Source.Type() != ::Core::Objects::ObjectType::AITurretClient) continue;
        if (pd.Target.Compare(minion)) return pd.StartTick;
    }
    return 0;
}

} // namespace SDK::Prediction::Health
