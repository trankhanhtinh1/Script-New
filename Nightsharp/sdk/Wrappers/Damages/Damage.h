#pragma once

#include "DamageJson.h"
#include "DamageMastery.h"
#include "DamagePassives.h"
#include "DamageLibrary.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace SDK::Damage {

    template <typename TVersion>
    inline void Initialize(const TVersion&) {
        // Damage tables are compiled into the native SDK; this keeps the
        // EnsoulSharp bootstrap call shape while preserving native startup.
    }

    inline float CalculateMixedDamage(const AIBaseClient& source,
                                      const AIBaseClient& target,
                                      float physicalAmount,
                                      float magicalAmount) {
        if (!source.IsValid() || !target.IsValid()) {
            return 0.0f;
        }

        return source.CalculatePhysicalDamage(target, std::max(physicalAmount, 0.0f)) +
               source.CalculateMagicDamage(target, std::max(magicalAmount, 0.0f));
    }

    inline float CalculateDamage(const AIBaseClient& source,
                                 const AIBaseClient& target,
                                 DamageType damageType,
                                 float amount) {
        if (!source.IsValid() || !target.IsValid()) {
            return 0.0f;
        }

        const float safeAmount = std::max(amount, 0.0f);
        float damage = 0.0f;
        switch (damageType) {
        case DamageType::Magical:
            damage = source.CalculateMagicDamage(target, safeAmount);
            break;
        case DamageType::Physical:
            damage = source.CalculatePhysicalDamage(target, safeAmount);
            break;
        case DamageType::Mixed:
            damage = CalculateMixedDamage(source, target, safeAmount * 0.5f, safeAmount * 0.5f);
            break;
        case DamageType::True:
        default:
            damage = std::floor(safeAmount);
            break;
        }

        if (!source.IsHero()) {
            return DamageMastery::ApplyIncoming(target, damageType, damage);
        }
        return DamageMastery::Apply(AIHeroClient(source.Address()), target, damageType, damage);
    }

    inline float GetSpellDamage(const AIBaseClient& source,
                                const AIBaseClient& target,
                                SpellSlot slot,
                                DamageStage stage = DamageStage::Default) {
        const float raw = DamageLibrary::GetSpellDamage(source, target, slot, stage);
        if (!source.IsHero()) {
            return DamageMastery::ApplyIncoming(target, DamageLibrary::GetSpellDamageType(source, slot, stage), raw);
        }
        return DamageMastery::Apply(
            AIHeroClient(source.Address()),
            target,
            DamageLibrary::GetSpellDamageType(source, slot, stage),
            raw);
    }

    inline float GetSpellDamage(const AIBaseClient& source,
                                const AIBaseClient& target,
                                SpellSlot slot) {
        return GetSpellDamage(source, target, slot, DamageStage::Default);
    }

    inline float GetPassiveDamage(const AIHeroClient& source,
                                  const AIBaseClient& target) {
        return DamagePassives::GetPassiveDamage(source, target);
    }

    inline float ComputeAutoAttackDamage(const AIHeroClient& source,
                                         const AIBaseClient& target,
                                         bool includePassives) {
        float damage = 0.0f;

        if (includePassives) {
            const auto passiveInfo = DamagePassives::GetPassiveDamageDetails(source, target);
            if (passiveInfo.Override) {
                // Override replaces base AA damage (e.g. TF cards)
                damage = source.CalculatePhysicalDamage(target, passiveInfo.Physical)
                       + source.CalculateMagicDamage(target, passiveInfo.Magical)
                       + passiveInfo.True_;
            } else {
                // Base AA + passive bonus
                damage = source.GetAutoAttackDamage(target, false);
                damage += source.CalculatePhysicalDamage(target, passiveInfo.Physical)
                        + source.CalculateMagicDamage(target, passiveInfo.Magical)
                        + passiveInfo.True_;
            }
        } else {
            damage = source.GetAutoAttackDamage(target, false);
        }

        return DamageMastery::Apply(source, target, DamageType::Physical, damage);
    }

    // Matching EnsoulSharp: GetAutoAttackDamage considers passives + override.
    //
    // Per-frame memo. `includePassives` runs GetPassiveDamageDetails = ~10
    // source-side HasBuff name-scans (each enumerates the whole buff array on a
    // miss, e.g. sheen/lichbane/InfinityEdge the player doesn't own) + ~20
    // HasItem lookups. The TargetSelector damage-based modes (LessAttacksToKill /
    // LessCastsToKill / Weight Killable+LessAttack) call this INSIDE the
    // std::sort comparator, so a single OrderChampions pass evaluates it
    // O(n log n) times over the SAME (player, hero) pairs — the dominant cost of
    // one GetTargets call (the orbwalker's per-tick target scan) and of champion
    // combo damage. EnsoulSharp read these as cached properties; the port did
    // not. Cache per (source, target, includePassives) for the current
    // GetTickCount window (~15 ms) — attack damage barely changes within that,
    // so no meaningful freshness loss. thread_local so game/render threads never
    // share the cache.
    inline float GetAutoAttackDamage(const AIHeroClient& source,
                                     const AIBaseClient& target,
                                     bool includePassives = true) {
        if (!source.IsValid() || !target.IsValid()) return 0.0f;

        struct Memo {
            uintptr_t src = 0;
            uintptr_t tgt = 0;
            bool passives = false;
            DWORD tick = 0;
            float damage = 0.0f;
        };
        static thread_local std::vector<Memo> memo;
        const DWORD now = ::GetTickCount();
        const uintptr_t src = source.Address();
        const uintptr_t tgt = target.Address();
        for (auto& m : memo) {
            if (m.src == src && m.tgt == tgt && m.passives == includePassives) {
                if (m.tick != now) {
                    m.tick = now;
                    m.damage = ComputeAutoAttackDamage(source, target, includePassives);
                }
                return m.damage;
            }
        }
        const float damage = ComputeAutoAttackDamage(source, target, includePassives);
        memo.push_back({ src, tgt, includePassives, now, damage });
        return damage;
    }

} // namespace SDK::Damage
