#pragma once

#include "Types.h"
#include "../../Core/Game.h"
#include "../../Enumerations/HealthPredictionType.h"
#include "../../Events/ObjectTracker.h"
#include "../../Events/SpellCastTracker.h"
#include "../../../core/RuntimeAPI.h"

#include <algorithm>
#include <cfloat>
#include <climits>
#include <new>
#include <unordered_map>

namespace SDK::Prediction::Health {

struct PredictedDamage {
    uint64_t AttackId = 0;
    int SourceNetId = 0;
    int TargetNetId = 0;
    int StartTick = 0;
    int Delay = 0;
    float AnimationTime = 0.0f;
    float ProjectileSpeed = 0.0f;
    float Damage = 0.0f;
    bool IsMelee = false;
    bool Processed = false;
};

namespace detail {

    inline std::unordered_map<uint64_t, PredictedDamage>* g_activeAttacks = nullptr;
    inline std::unordered_map<int, uint64_t>* g_lastAttackBySource = nullptr;
    inline std::unordered_map<int, int>* g_turretAggroStartTick = nullptr;
    inline uint64_t g_nextAttackId = 1;
    inline bool g_hooksRegistered = false;

    inline bool EnsureStorage() {
        if (!g_activeAttacks) {
            g_activeAttacks = new(std::nothrow) std::unordered_map<uint64_t, PredictedDamage>();
        }
        if (!g_lastAttackBySource) {
            g_lastAttackBySource = new(std::nothrow) std::unordered_map<int, uint64_t>();
        }
        if (!g_turretAggroStartTick) {
            g_turretAggroStartTick = new(std::nothrow) std::unordered_map<int, int>();
        }
        return g_activeAttacks && g_lastAttackBySource && g_turretAggroStartTick;
    }

    /// Resolve projectile speed: prefer SData.MissileSpeed from args,
    /// melee → FLT_MAX, fallback to heuristic only if SData unavailable.
    /// Matches EnsoulSharp: sender.IsMelee ? int.MaxValue : (int)args.SData.MissileSpeed
    inline float ResolveProjectileSpeed(const AIBaseClient& source, float sdataMissileSpeed) {
        if (!source.IsValid()) {
            return 1400.0f;
        }
        if (source.IsMelee()) {
            return FLT_MAX;
        }
        // Use real SData.MissileSpeed when available
        if (sdataMissileSpeed > 0.0f) {
            return sdataMissileSpeed;
        }
        // Fallback only when SData is unavailable
        if (source.IsTurret()) {
            return 1200.0f;
        }
        if (source.IsMinion()) {
            uint8_t mc = RuntimeAPI::GetMinionClass(source.Address());
            if (mc == ::MC_SiegeLaneMinion) return 1200.0f; // Cannon/Siege
            return 650.0f; // Ranged caster
        }
        return 1400.0f;
    }

    inline GameObject ResolveTarget(int targetNetId) {
        return targetNetId != 0 ? ObjectManager::GetByNetId(targetNetId) : GameObject();
    }

    inline AIBaseClient ResolveSource(int sourceNetId) {
        return sourceNetId != 0 ? AIBaseClient(ObjectManager::GetByNetId(sourceNetId).Address()) : AIBaseClient();
    }

    inline void OnProcessSpellCast(const AIBaseClient& sender, const Events::SpellCast::ProcessSpellCastEventArgs& args) {
        if (!EnsureStorage() || !sender.IsValid() || !args.IsAutoAttack || args.TargetNetworkId == 0) {
            return;
        }

        if (!sender.IsAlly() || (!sender.IsMinion() && !sender.IsTurret())) {
            return;
        }

        const auto target = ResolveTarget(args.TargetNetworkId);
        if (!target.IsValid() || !target.IsMinion()) {
            return;
        }

        PredictedDamage attack = {};
        attack.AttackId = g_nextAttackId++;
        attack.SourceNetId = sender.NetworkId();
        attack.TargetNetId = args.TargetNetworkId;
        attack.StartTick = Game::TickCount() - (Game::Ping() / 2);
        attack.Delay = static_cast<int>(std::max(sender.AttackCastDelay(), 0.0f) * 1000.0f);
        attack.AnimationTime = std::max(sender.AttackDelay(), 0.0f) * 1000.0f;
        if (sender.IsTurret() && attack.AnimationTime >= 70.0f) {
            attack.AnimationTime -= 70.0f;
        }
        attack.ProjectileSpeed = ResolveProjectileSpeed(sender, args.MissileSpeed);
        attack.Damage = sender.GetAutoAttackDamage(target);
        attack.IsMelee = sender.IsMelee();
        attack.Processed = false;

        (*g_activeAttacks)[attack.AttackId] = attack;
        (*g_lastAttackBySource)[sender.NetworkId()] = attack.AttackId;

        if (sender.IsTurret()) {
            (*g_turretAggroStartTick)[args.TargetNetworkId] = attack.StartTick;
        }
    }

    inline void OnDoCast(const AIBaseClient& sender, const Events::SpellCast::ProcessSpellCastEventArgs& args) {
        if (!EnsureStorage() || !sender.IsValid() || !args.IsAutoAttack) {
            return;
        }

        if (!sender.IsAlly() || (!sender.IsMinion() && !sender.IsTurret())) {
            return;
        }

        const auto target = ResolveTarget(args.TargetNetworkId);
        if (!target.IsValid() || !target.IsMinion()) {
            return;
        }

        const auto it = g_lastAttackBySource->find(sender.NetworkId());
        if (it == g_lastAttackBySource->end()) {
            return;
        }

        auto attackIt = g_activeAttacks->find(it->second);
        if (attackIt == g_activeAttacks->end()) {
            return;
        }

        attackIt->second.StartTick = Game::TickCount() - (Game::Ping() / 2);
        attackIt->second.Delay = static_cast<int>(std::max(sender.AttackCastDelay(), 0.0f) * 1000.0f);
        attackIt->second.AnimationTime = std::max(sender.AttackDelay(), 0.0f) * 1000.0f;
        if (sender.IsTurret() && attackIt->second.AnimationTime >= 70.0f) {
            attackIt->second.AnimationTime -= 70.0f;
        }
        attackIt->second.Processed = true;
    }

    inline void OnStopCast(const AIBaseClient& sender, const Events::SpellCast::StopCastEventArgs& args) {
        (void)args;
        if (!EnsureStorage() || !sender.IsValid()) {
            return;
        }

        const auto it = g_lastAttackBySource->find(sender.NetworkId());
        if (it == g_lastAttackBySource->end()) {
            return;
        }

        g_activeAttacks->erase(it->second);
        g_lastAttackBySource->erase(it);
    }

    inline void OnDelete(const GameObject& sender) {
        if (!EnsureStorage() || !sender.IsValid()) {
            return;
        }

        if (sender.IsMissile()) {
            const MissileClient missile(sender.Address());
            const int casterNetId = missile.CasterNetworkId();
            if (casterNetId != 0) {
                for (auto& [id, attack] : *g_activeAttacks) {
                    (void)id;
                    if (attack.SourceNetId == casterNetId) {
                        attack.Processed = true;
                    }
                }
            }
            return;
        }

        const int netId = sender.NetworkId();
        if (netId == 0) {
            return;
        }

        for (auto it = g_activeAttacks->begin(); it != g_activeAttacks->end();) {
            if (it->second.SourceNetId == netId || it->second.TargetNetId == netId) {
                it = g_activeAttacks->erase(it);
            } else {
                ++it;
            }
        }

        g_lastAttackBySource->erase(netId);
        g_turretAggroStartTick->erase(netId);
    }

    inline int EstimateTimeToHit(const PredictedDamage& attack, const AIBaseClient& unit) {
        if (!unit.IsValid()) {
            return INT_MAX;
        }

        const auto source = ResolveSource(attack.SourceNetId);
        if (!source.IsValid()) {
            return INT_MAX;
        }

        const float distance = source.Distance(unit);
        if (attack.IsMelee || attack.ProjectileSpeed >= FLT_MAX) {
            return attack.Delay;
        }

        return attack.Delay + static_cast<int>((distance / std::max(attack.ProjectileSpeed, 1.0f)) * 1000.0f);
    }

    inline float GetPredictionDefault(const AIBaseClient& unit, int timeMs, int delayMs) {
        float damage = 0.0f;
        if (!EnsureStorage() || !unit.IsValid()) {
            return damage;
        }

        for (const auto& [id, attack] : *g_activeAttacks) {
            (void)id;
            if (attack.TargetNetId != unit.NetworkId() || attack.Processed) {
                continue;
            }

            const auto source = ResolveSource(attack.SourceNetId);
            if (!source.IsValid()) {
                continue;
            }

            const int landTime = attack.StartTick + EstimateTimeToHit(attack, unit) + std::max(delayMs, 0);
            if (landTime < Game::TickCount() + timeMs) {
                damage += attack.Damage;
            }
        }

        return std::max(0.0f, unit.Health() - damage);
    }

    inline float GetPredictionSimulated(const AIBaseClient& unit, int timeMs) {
        float damage = 0.0f;
        if (!EnsureStorage() || !unit.IsValid()) {
            return damage;
        }

        const int now = Game::TickCount();
        const int toT = now + std::max(timeMs, 0);

        for (const auto& [id, attack] : *g_activeAttacks) {
            (void)id;
            if (attack.TargetNetId != unit.NetworkId()) {
                continue;
            }

            const auto source = ResolveSource(attack.SourceNetId);
            if (!source.IsValid() || attack.AnimationTime <= 0.0f) {
                continue;
            }

            if (now - 100 > attack.StartTick + static_cast<int>(attack.AnimationTime)) {
                continue;
            }

            int fromT = attack.StartTick;
            const int cycleMs = std::max(static_cast<int>(attack.AnimationTime), 1);
            while (fromT < toT) {
                const int landTime = fromT + detail::EstimateTimeToHit(attack, unit);
                if (fromT >= now && landTime < toT) {
                    damage += attack.Damage;
                }
                fromT += cycleMs;
            }
        }

        return std::max(0.0f, unit.Health() - damage);
    }
}

inline void Initialize() {
    if (!detail::EnsureStorage()) {
        return;
    }

    if (!detail::g_hooksRegistered) {
        Events::SpellCast::AddOnProcessSpellCast(detail::OnProcessSpellCast);
        Events::SpellCast::AddOnDoCast(detail::OnDoCast);
        Events::SpellCast::AddOnStopCast(detail::OnStopCast);
        Events::ObjectTracker::AddOnDelete(detail::OnDelete);
        detail::g_hooksRegistered = true;
    }
}

inline void Reset() {
    if (detail::g_activeAttacks) {
        detail::g_activeAttacks->clear();
    }
    if (detail::g_lastAttackBySource) {
        detail::g_lastAttackBySource->clear();
    }
    if (detail::g_turretAggroStartTick) {
        detail::g_turretAggroStartTick->clear();
    }
    detail::g_nextAttackId = 1;
    detail::g_hooksRegistered = false;
}

// Missile scan implementation — called from SEH wrapper
inline void ScanMissilesImpl(int now) {
    if (!detail::EnsureStorage()) return;
    const int myTeam = ObjectManager::Player().Team();
    if (detail::g_activeAttacks->size() >= 64) return;

    for (const auto& missile : ObjectManager::Missiles()) {
        if (!missile.IsValid()) continue;

        int classify = RuntimeAPI::ClassifyMissile(missile.Address());
        if (classify != 1 && classify != 2) continue;

        int casterNetId = missile.CasterNetworkId();
        int targetNetId = missile.TargetNetworkId();
        if (casterNetId == 0 || targetNetId == 0) continue;

        auto existIt = detail::g_lastAttackBySource->find(casterNetId);
        if (existIt != detail::g_lastAttackBySource->end()) {
            auto attackIt = detail::g_activeAttacks->find(existIt->second);
            if (attackIt != detail::g_activeAttacks->end() &&
                attackIt->second.TargetNetId == targetNetId) {
                continue;
            }
        }

        auto casterObj = ObjectManager::GetByNetId(casterNetId);
        if (!casterObj.IsValid() || casterObj.Team() != myTeam) continue;

        auto caster = AIBaseClient(casterObj.Address());
        auto target = AIBaseClient(ObjectManager::GetByNetId(targetNetId).Address());
        if (!caster.IsValid() || !target.IsValid()) continue;

        PredictedDamage attack = {};
        attack.AttackId = detail::g_nextAttackId++;
        attack.SourceNetId = casterNetId;
        attack.TargetNetId = targetNetId;
        attack.StartTick = now;
        attack.Damage = caster.GetAutoAttackDamage(target);
        attack.Delay = (int)(caster.AttackCastDelay() * 1000.0f);
        if (caster.IsMelee()) {
            attack.ProjectileSpeed = FLT_MAX;
        } else if (classify == 2) {
            attack.ProjectileSpeed = 1200.0f; // Turret
        } else {
            uint8_t mc = RuntimeAPI::GetMinionClass(caster.Address());
            attack.ProjectileSpeed = (mc == ::MC_SiegeLaneMinion) ? 1200.0f : 650.0f;
        }
        attack.IsMelee = caster.IsMelee();
        attack.Processed = false;

        (*detail::g_activeAttacks)[attack.AttackId] = attack;
        (*detail::g_lastAttackBySource)[casterNetId] = attack.AttackId;

        if (classify == 2) {
            (*detail::g_turretAggroStartTick)[targetNetId] = now;
        }
    }
}

// SEH wrapper — no local objects with destructors (C2712 safe)
inline void ScanMissilesSafe(int now) {
    __try {
        ScanMissilesImpl(now);
    }
    __except (1) {
        if (detail::g_activeAttacks) detail::g_activeAttacks->clear();
        if (detail::g_lastAttackBySource) detail::g_lastAttackBySource->clear();
    }
}

inline void Update() {
    Initialize();
    if (!detail::EnsureStorage()) {
        return;
    }

    const int now = Game::TickCount();

    // ── Missile-based AA tracking (RuntimeAPI) ──
    // Throttle: only scan missiles every other frame to reduce CPU
    static int s_frameCounter = 0;
    s_frameCounter++;
    if ((s_frameCounter & 1) == 0) {
        ScanMissilesSafe(now);
    }

    // ── Cleanup expired attacks ──
    // Age-based: remove any entry older than 3 seconds (prevents unbounded growth)
    std::vector<uint64_t> toErase = {};
    toErase.reserve(16);

    for (const auto& [id, attack] : *detail::g_activeAttacks) {
        // Hard age limit: 3 seconds max
        if (now - attack.StartTick > 3000) {
            toErase.push_back(id);
            continue;
        }

        const auto target = AIBaseClient(detail::ResolveTarget(attack.TargetNetId).Address());
        if (!target.IsValid() || target.IsDead()) {
            toErase.push_back(id);
            continue;
        }

        const int landTime = attack.StartTick + detail::EstimateTimeToHit(attack, target) + 250;
        if (now > landTime) {
            toErase.push_back(id);
        }
    }

    for (const auto id : toErase) {
        detail::g_activeAttacks->erase(id);
    }

    // Cleanup stale source entries
    if (detail::g_lastAttackBySource->size() > 128) {
        detail::g_lastAttackBySource->clear();
    }
}

inline float EstimateIncomingAutoAttackDamage(const AIBaseClient& unit, int timeMs) {
    float damage = 0.0f;
    if (!detail::EnsureStorage() || !unit.IsValid()) {
        return damage;
    }

    for (const auto& [id, attack] : *detail::g_activeAttacks) {
        (void)id;
        if (attack.TargetNetId != unit.NetworkId()) {
            continue;
        }

        const auto source = detail::ResolveSource(attack.SourceNetId);
        if (!source.IsValid()) {
            continue;
        }

        const int etaMs = detail::EstimateTimeToHit(attack, unit);
        if (etaMs <= timeMs) {
            damage += attack.Damage;
        }
    }
    return damage;
}

inline float GetPrediction(const AIBaseClient& unit,
                           int timeMs,
                           int delayMs = 70,
                           HealthPredictionType type = HealthPredictionType::Default) {
    if (!unit.IsValid() || unit.IsDead()) {
        return 0.0f;
    }

    switch (type) {
    case HealthPredictionType::Simulated:
        return detail::GetPredictionSimulated(unit, timeMs);
    case HealthPredictionType::Default:
    default:
        return detail::GetPredictionDefault(unit, timeMs, delayMs);
    }
}

inline AIBaseClient GetAggroTurret(const AIMinionClient& minion) {
    if (!detail::EnsureStorage() || !minion.IsValid()) {
        return {};
    }

    for (const auto& [id, attack] : *detail::g_activeAttacks) {
        (void)id;
        if (attack.TargetNetId != minion.NetworkId()) {
            continue;
        }
        const auto source = detail::ResolveSource(attack.SourceNetId);
        if (source.IsValid() && source.IsTurret()) {
            return source;
        }
    }
    return {};
}

inline bool HasMinionAggro(const AIMinionClient& minion) {
    if (!detail::EnsureStorage() || !minion.IsValid()) {
        return false;
    }

    for (const auto& [id, attack] : *detail::g_activeAttacks) {
        (void)id;
        if (attack.TargetNetId != minion.NetworkId()) {
            continue;
        }
        const auto source = detail::ResolveSource(attack.SourceNetId);
        if (source.IsValid() && source.IsMinion()) {
            return true;
        }
    }
    return false;
}

inline bool HasTurretAggro(const AIMinionClient& minion) {
    return GetAggroTurret(minion).IsValid();
}

inline int TurretAggroStartTick(const AIMinionClient& minion) {
    if (!detail::EnsureStorage() || !minion.IsValid()) {
        return 0;
    }

    const auto it = detail::g_turretAggroStartTick->find(minion.NetworkId());
    return it != detail::g_turretAggroStartTick->end() ? it->second : 0;
}

} // namespace SDK::Prediction::Health

namespace SDK::Health {

inline void Initialize() {
    Prediction::Health::Initialize();
}

inline void Update() {
    Prediction::Health::Update();
}

inline void Reset() {
    Prediction::Health::Reset();
}

inline float EstimateIncomingAutoAttackDamage(const AIBaseClient& unit, int timeMs) {
    return Prediction::Health::EstimateIncomingAutoAttackDamage(unit, timeMs);
}

inline float GetPrediction(const AIBaseClient& unit,
                           int timeMs,
                           int delayMs = 70,
                           HealthPredictionType type = HealthPredictionType::Default) {
    return Prediction::Health::GetPrediction(unit, timeMs, delayMs, type);
}

inline AIBaseClient GetAggroTurret(const AIMinionClient& minion) {
    return Prediction::Health::GetAggroTurret(minion);
}

inline bool HasMinionAggro(const AIMinionClient& minion) {
    return Prediction::Health::HasMinionAggro(minion);
}

inline bool HasTurretAggro(const AIMinionClient& minion) {
    return Prediction::Health::HasTurretAggro(minion);
}

inline int TurretAggroStartTick(const AIMinionClient& minion) {
    return Prediction::Health::TurretAggroStartTick(minion);
}

} // namespace SDK::Health
