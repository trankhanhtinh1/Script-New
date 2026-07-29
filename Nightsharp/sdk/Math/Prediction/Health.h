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
        // REMOVED: Turret/Inhibitor/Nexus disabled by user request
        // AITurretClient turret(source.Address());
        // TurretType tt = SDK::Extensions::GetTurretType(turret);
        // switch (tt) {
        // case TurretType::TierOne:
        //     result = 0.14f * minion.MaxHealth();
        //     break;
        // case TurretType::TierTwo:
        //     result = 0.11f * minion.MaxHealth();
        //     break;
        // case TurretType::TierThree:
        // case TurretType::TierFour:
        //     result = 0.08f * minion.MaxHealth();
        //     break;
        // default:
        //     break;
        // }
        // REMOVED: Turret/Inhibitor/Nexus disabled
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

    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    // Turret → minion: use override mod (% max HP formula)
    // if (source.Type() == ::Core::Objects::ObjectType::AITurretClient
    //     && target.Type() == ::Core::Objects::ObjectType::AIMinionClient) {
    //     AITurretClient turret(source.Address());
    //     AIMinionClient minion(target.Address());
    //     return AutoAttackDamageOverrideMod(turret, minion, source.TotalAttackDamage());
    // }
    // REMOVED: Turret/Inhibitor/Nexus disabled

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

// ── Diagnostics (bounded) ────────────────────────────────────────────────────
// ShouldWait / last-hit timing depends ENTIRELY on ActiveAttacks being fed by
// the events below. These counters pinpoint which filter kills the feed when
// farm pacing "has no pause": read the periodic [HPTrack] line in the debug log.
struct TrackDiag {
    long seen = 0;             // OnDoCast events that reached us
    long rejSender = 0;        // sender invalid / not resolvable
    long rejAllyOrType = 0;    // not ally, or not minion/turret
    long rejDeadVisible = 0;   // sender dead/invisible/too far
    long rejNotAuto = 0;       // not an auto attack
    long rejNoTargetNet = 0;   // TargetNetworkId == 0
    long rejTargetInvalid = 0; // target resolve failed / dead
    long accepted = 0;         // inserted into ActiveAttacks
    int  lastLogTick = 0;
    int  detailLogs = 0;       // first few accepted/rejected logged in detail
};
inline TrackDiag Diag;

// ── OnDoCast: register auto-attack damage ────────────────────────────────────
inline void OnDoCast(const Events::ProcessSpellEventArgs& args) {
    ++Diag.seen;

    // Resolve sender from args.Sender.
    //
    // IMPORTANT: do NOT trust wrapper Type() here. AIBaseClient(address) stamps
    // ObjectType::GameObject (the ctor default) into the handle, and Type()
    // only infers when the stamp is Unknown — so Type() returned GameObject(11)
    // for every minion/turret and this filter rejected 100% of ally attacks,
    // leaving ActiveAttacks empty (=> no last-hit pause, broken ShouldWait).
    // Classify by asking the game's typed managers directly instead.
    if (!args.Sender.IsValid()) { ++Diag.rejSender; return; }
    const uintptr_t senderPtr = args.Sender.Ptr;
    const bool isMinion = ::Core::ObjectManager::ManagerContains(
        ::Core::ObjectManager::ManagerKind::Minions, senderPtr);
    const bool isTurret = !isMinion && ::Core::ObjectManager::ManagerContains(
        ::Core::ObjectManager::ManagerKind::Turrets, senderPtr);

    // Stamp the REAL type on the wrapper so downstream Type() checks work
    // (turret damage override, GetAggroTurret/HasTurretAggro/HasMinionAggro).
    AIBaseClient sender(
        senderPtr,
        // REMOVED: Turret/Inhibitor/Nexus disabled by user request
        // isTurret ? ::Core::Objects::ObjectType::AITurretClient
        //          : ::Core::Objects::ObjectType::AIMinionClient
        ::Core::Objects::ObjectType::AIMinionClient);

    if (!sender.IsValid()) { ++Diag.rejSender; return; }
    if (!sender.IsAlly() || (!isMinion && !isTurret)) {
        ++Diag.rejAllyOrType;
        return;
    }

    // DLL: sender.IsValidTarget(3000f, checkTeam: false). We keep dead+distance
    // but deliberately DROP the IsVisible() check: the Visible byte (All::0x308)
    // reads false for a large share of nearby ALLY minions on 26.x and was
    // silently discarding their attacks (HPTrack rejDeadVis=364 in one lane
    // phase) -> ActiveAttacks stayed empty -> no last-hit pause. Visibility is
    // irrelevant for tracking our own minions' attacks anyway.
    if (sender.IsDead()) { ++Diag.rejDeadVisible; return; }
    const auto player = SDK::ObjectManager::Player();
    if (player.IsValid() && sender.Distance(player) > 3000.0f) { ++Diag.rejDeadVisible; return; }

    // Auto-attack gate: minions and turrets only ever "cast" their basic attack,
    // so a DoCast from them IS an auto attack by definition. The castInfo IsAuto
    // flag and resource ScriptName are unreliable for non-hero casters here
    // (ScriptName decodes as 0xFF garbage) and rejected every ally minion attack
    // (HPTrack rejAuto=238). EnsoulSharp's name check works only because its
    // runtime resolves SData.Name; ours does not for these casters — so no
    // flag/name gate for minion/turret senders.

    // Resolve target. Same disease as the sender gate: routing through
    // GetUnitByNetworkId<AIMinionClient> silently failed for every ally attack
    // (HPTrack rejTgt=58, acc=0) because of the wrapper type-inference layer.
    // The decoder already resolved the target object at event time — use its
    // pointer directly and validate minion-ness via the game's minion manager.
    uintptr_t targetPtr = args.Target.IsValid() ? args.Target.Ptr : 0;
    if (!targetPtr && args.TargetNetworkId != 0) {
        targetPtr = ::Core::ObjectManager::FindByNetworkId(args.TargetNetworkId);
    }
    // Salvage by SLOT: TargetIndex is the local object id from CastInfo's target
    // array. If strict index resolution failed, the low word is still the
    // object-manager slot; the minion-manager membership check below keeps it safe.
    const std::uint32_t targetLocalId = static_cast<std::uint32_t>(args.TargetIndex);
    if (!targetPtr && targetLocalId != 0 && targetLocalId != 0xFFFFFFFFu) {
        const auto view = ::Core::ObjectManager::ReadObjectArrayView(
            CoreRuntime::GetContext().objectManager);
        const std::uint32_t slot = targetLocalId & 0xFFFFu;
        if (view.IsValid() && slot < view.count) {
            const uintptr_t candidate = Globals::Read<uintptr_t>(
                view.items + static_cast<uintptr_t>(slot) * sizeof(uintptr_t));
            if (::Core::ObjectManager::IsLiveEntry(candidate)) {
                targetPtr = candidate;
            }
        }
    }
    if (!targetPtr) {
        ++Diag.rejNoTargetNet;
        if (Diag.detailLogs < 12) {
            ++Diag.detailLogs;
            NightSharpDebug::Logf(
                "[HPTrack] reject target-resolve tIdx=%d tNet=0x%X tPtr=0x%p sender=%s",
                args.TargetIndex,
                args.TargetNetworkId,
                reinterpret_cast<void*>(args.Target.Ptr),
                sender.CharacterName().c_str());
        }
        return;
    }
    if (!::Core::ObjectManager::ManagerContains(
            ::Core::ObjectManager::ManagerKind::Minions, targetPtr)) {
        ++Diag.rejTargetInvalid;
        return;
    }
    AIMinionClient target(targetPtr); // ctor stamps ObjectType::AIMinionClient
    if (!target.IsValid() || target.IsDead()) { ++Diag.rejTargetInvalid; return; }
    ++Diag.accepted;
    if (Diag.detailLogs < 6) {
        ++Diag.detailLogs;
        NightSharpDebug::Logf(
            "[HPTrack] accept sender=%s -> target=%s net=%u",
            sender.CharacterName().c_str(),
            target.CharacterName().c_str(),
            args.TargetNetworkId);
    }

    PredictedDamage pd;
    pd.StartTick = SDK::Variables::TickCount() - SDK::Game::Ping() / 2;
    pd.ProjectileSpeed = sender.IsMelee()
        ? std::numeric_limits<int>::max()
        : static_cast<int>(args.MissileSpeed);
    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    pd.AnimationTime = ::CoreControl::GetAttackDelay(sender.Address()) * 1000.0f;
    // - (sender.Type() == ::Core::Objects::ObjectType::AITurretClient ? 70.0f : 0.0f);
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

    AIBaseClient owner(args.Sender.Ptr);
    if (!owner.IsValid() || !owner.IsAlly()) return;

    // DLL: args.KeepAnimationPlaying && args.DestroyMissile
    if (!args.KeepAnimationPlaying || !args.DestroyMissile) return;

    if (owner.IsMelee()) {
        ActiveAttacks.erase(owner.NetworkId());
    }
    ActiveAttacks.erase(owner.NetworkId());
}

// ── OnDelete: mark attack as processed when missile is destroyed ─────────────
inline void OnMissileDelete(const Events::ObjectEventArgs& args) {
    // DLL reads GameObject.OnDelete and then filters MissileClient. NightSharp
    // has a narrower missile-delete event, so HealthPrediction does not need
    // the hot general object-delete hook.
    if (args.SourceNetworkId == 0) return;

    auto it = detail::ActiveAttacks.find(static_cast<int>(args.SourceNetworkId));
    if (it != detail::ActiveAttacks.end()) {
        it->second.Processed = true;
    }
}

// ── OnProcessSpellCast: mark melee ally attacks as processed ─────────────────
inline void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;

    AIBaseClient sender(args.Sender.Ptr);
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

    // Periodic tracking-health summary (every 5s). If `acc` stays 0 while lane
    // minions are fighting, the feed is dead and the rej* counter that grows
    // names the exact filter responsible — that is why farm has no wait-pause.
    if (now - Diag.lastLogTick >= 5000) {
        Diag.lastLogTick = now;
        NightSharpDebug::Logf(
            "[HPTrack] seen=%ld acc=%ld rejSender=%ld rejAllyType=%ld rejDeadVis=%ld rejAuto=%ld rejNet=%ld rejTgt=%ld active=%zu",
            Diag.seen, Diag.accepted, Diag.rejSender, Diag.rejAllyOrType,
            Diag.rejDeadVisible, Diag.rejNotAuto, Diag.rejNoTargetNet,
            Diag.rejTargetInvalid, ActiveAttacks.size());
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
    Events::AddOnMissileDelete(&detail::OnMissileDelete);
    Events::AddOnProcessSpell(&detail::OnProcessSpellCast);
    Events::AddOnGameUpdate(&detail::OnUpdate);
}

inline void Reset() {
    if (detail::Initialized) {
        Events::RemoveOnGameUpdate(&detail::OnUpdate);
        Events::RemoveOnProcessSpell(&detail::OnProcessSpellCast);
        Events::RemoveOnMissileDelete(&detail::OnMissileDelete);
        Events::RemoveOnStopCast(&detail::OnStopCast);
        Events::RemoveOnDoCast(&detail::OnDoCast);
    }
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
                  / static_cast<float>(std::max(1, pd.ProjectileSpeed)))
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
                // StartTick/AnimationTime/endTime are milliseconds. The old
                // port converted windup + flight to seconds here and then added
                // that value directly to a millisecond tick, making simulated
                // minion attacks land hundreds of milliseconds too early.
                const float travelTimeMs = pd.Delay
                    + 1000.0f * (pd.Source.IsMelee() ? 0.0f
                        : std::max(0.0f, unit.Distance(pd.Source) - pd.TargetBoundingRadius)
                          / static_cast<float>(std::max(1, pd.ProjectileSpeed)));

                if (tick >= now
                    && static_cast<float>(tick) + travelTimeMs < static_cast<float>(endTime)) {
                    hitCount++;
                }

                int step = static_cast<int>(pd.AnimationTime);
                if (step < 30) step = 30;
                tick += step;
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
    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    // if (!minion.IsValid()) return AIBaseClient();
    //
    // for (auto& [key, pd] : detail::ActiveAttacks) {
    //     if (!pd.Source.IsValid()) continue;
    //     if (pd.Source.Type() != ::Core::Objects::ObjectType::AITurretClient) continue;
    //     if (pd.Target.Compare(minion)) {
    //         return pd.Source;
    //     }
    // }
    return AIBaseClient();
    // REMOVED: Turret/Inhibitor/Nexus disabled
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
    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    // if (!minion.IsValid()) return false;
    //
    // for (auto& [key, pd] : detail::ActiveAttacks) {
    //     if (!pd.Source.IsValid()) continue;
    //     if (pd.Source.Type() != ::Core::Objects::ObjectType::AITurretClient) continue;
    //     if (pd.Target.Compare(minion)) return true;
    // }
    return false;
    // REMOVED: Turret/Inhibitor/Nexus disabled
}

// ── TurretAggroStartTick ─────────────────────────────────────────────────────
inline int TurretAggroStartTick(const AIMinionClient& minion) {
    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    // if (!minion.IsValid()) return 0;
    //
    // for (auto& [key, pd] : detail::ActiveAttacks) {
    //     if (!pd.Source.IsValid()) continue;
    //     if (pd.Source.Type() != ::Core::Objects::ObjectType::AITurretClient) continue;
    //     if (pd.Target.Compare(minion)) return pd.StartTick;
    // }
    return 0;
    // REMOVED: Turret/Inhibitor/Nexus disabled
}

} // namespace SDK::Prediction::Health
