#pragma once

// ============================================================================
// AIBaseClient — units driven by the AI pipeline (heroes, minions, turrets)
// ============================================================================
// Extends `GameObject` with everything that only makes sense for an *actor*:
//   * SpellBook access (slot lookup by name or enum, one-step cooldown reads)
//   * Combat calc (Physical / Magical / Mixed / True damage mitigation)
//   * Auto-attack helpers (range, windup, time-to-hit including ping +
//     projectile travel)
//   * Interrupt / cast classification (IsCastingInterruptableSpell)
//   * IssueOrder wrappers dispatching to `CoreAPI::Control`
//   * Counting helpers for heroes / minions in radius
//
// `AIBaseClient` exists purely as a classification layer — it does not own
// extra raw state. The `AIHeroClient / AIMinionClient / AITurretClient` leaves
// inherit with minimal overrides so downstream code can accept the right
// strong type via `AIHeroClient` etc.
//
// `Damage::GetSpellDamage` is forward-declared here so `GetSpellDamage()`
// below can delegate without a hard include of `DamageLibrary.h` (which in
// turn includes `DamageData.h` — keeping the cycle resolution at the wrapper
// layer).
// ============================================================================

#include "../../core/CoreAPI.h"
#include "../../core/CoreBypass.h"
#include "../../core/Globals.h"
#include "../Enumerations/DamageType.h"
#include "../Enumerations/GameObjectOrder.h"
#include "../Enumerations/SpellSlot.h"
#include "GameObject.h"
#include "SpellbookClient.h"
#include "SpellDataInstClient.h"

#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <string>

namespace SDK {

class AIBaseClient;
namespace Damage {
    float GetSpellDamage(const AIBaseClient& source, const AIBaseClient& target, SpellSlot slot);
}

class AIBaseClient : public GameObject {
public:
    AIBaseClient() = default;
    explicit AIBaseClient(uintptr_t address)
        : GameObject(address) {}
    explicit AIBaseClient(const CoreObjects::ObjectRef& ref)
        : GameObject(ref) {}

    // ── Spellbook ──────────────────────────────────────────────────────────
    SpellbookClient GetSpellBook() const {
        return SpellbookClient(Address());
    }

    SpellbookClient Spellbook() const {
        return GetSpellBook();
    }

    // ── Movement / casting permission ─────────────────────────────────────
    bool CanAttack() const { return m_ref.CanAttack(); }
    bool CanMove() const { return m_ref.CanMove(); }
    bool CanCast() const { return m_ref.CanCast(); }

    // ── Auto-attack geometry ──────────────────────────────────────────────
    bool InAutoAttackRange(const GameObject& target, float extraRange = 0.0f) const {
        return m_ref.IsInAutoAttackRange(target.Ref(), extraRange);
    }

    float RealAutoAttackRange(const GameObject& target) const {
        return m_ref.GetRealAttackRange(target.Ref());
    }

    float GetRealAutoAttackRange(const GameObject& target = GameObject()) const {
        return m_ref.GetRealAttackRange(target.Ref());
    }

    // ── Damage calc ───────────────────────────────────────────────────────
    float GetAutoAttackDamage(const GameObject& target, bool includePassives = true) const {
        (void)includePassives;
        if (!target.IsValid()) {
            return 0.0f;
        }

        const float rawDamage = TotalAttackDamage();
        const float armor = target.Armor();
        if (armor >= 0.0f) {
            return rawDamage * (100.0f / (100.0f + armor));
        }

        return rawDamage * (2.0f - (100.0f / (100.0f - armor)));
    }

    float CalculatePhysicalDamage(const GameObject& target, float rawDamage) const {
        if (!target.IsValid() || rawDamage <= 0.0f) {
            return 0.0f;
        }

        const float mitigatedArmor = target.Armor() - FlatArmorPenetration();
        if (mitigatedArmor >= 0.0f) {
            return rawDamage * (100.0f / (100.0f + mitigatedArmor));
        }
        return rawDamage * (2.0f - (100.0f / (100.0f - mitigatedArmor)));
    }

    float CalculateMagicDamage(const GameObject& target, float rawDamage) const {
        if (!target.IsValid() || rawDamage <= 0.0f) {
            return 0.0f;
        }

        const float mitigatedMR = target.SpellBlock() - FlatMagicPenetration();
        if (mitigatedMR >= 0.0f) {
            return rawDamage * (100.0f / (100.0f + mitigatedMR));
        }
        return rawDamage * (2.0f - (100.0f / (100.0f - mitigatedMR)));
    }

    float CalculateDamage(const GameObject& target, DamageType damageType, float rawDamage) const {
        switch (damageType) {
        case DamageType::Physical:
            return CalculatePhysicalDamage(target, rawDamage);
        case DamageType::Magical:
            return CalculateMagicDamage(target, rawDamage);
        case DamageType::Mixed:
            return CalculatePhysicalDamage(target, rawDamage * 0.5f) + CalculateMagicDamage(target, rawDamage * 0.5f);
        case DamageType::True:
        default:
            return rawDamage;
        }
    }

    float CalculateMixedDamage(const GameObject& target, float physicalRawDamage, float magicalRawDamage) const {
        return CalculatePhysicalDamage(target, physicalRawDamage) + CalculateMagicDamage(target, magicalRawDamage);
    }

    // ── Attack timing ─────────────────────────────────────────────────────
    float AttackDelay() const {
        return CoreAPI::Control::GetAttackDelay(Address());
    }

    float AttackCastDelay() const {
        return CoreAPI::Control::GetAttackWindup(Address());
    }

    bool HasItem(int itemId) const {
        (void)itemId;
        return false;
    }

    float GetTimeToHit() const {
        const CoreObjects::ObjectRef player(CoreAPI::Objects::GetLocalPlayer());
        if (!player.IsValid() || !IsValid()) {
            return FLT_MAX;
        }

        float time = (CoreAPI::Control::GetAttackWindup(player.address) * 1000.0f) - 100.0f +
            (CoreAPI::Control::GetPing() * 0.5f);
        if (!player.IsMelee()) {
            float projectileSpeed = player.GetActiveSpellCast().GetMissileSpeed();
            if (projectileSpeed <= 0.0f || projectileSpeed >= 50000.0f) {
                projectileSpeed = 2000.0f;
            }

            time += 1000.0f *
                std::max(0.0f, player.DistanceTo(Position()) - player.GetBoundingRadius()) /
                projectileSpeed;
        }

        return time;
    }

    // ── Spell resolve (defers to Damage::GetSpellDamage) ──────────────────
    float GetSpellDamage(const GameObject& target, SpellSlot slot) const {
        return Damage::GetSpellDamage(*this, AIBaseClient(target.Address()), slot);
    }

    // ── Range counting ────────────────────────────────────────────────────
    int CountEnemyHeroesInRange(float range) const {
        return CoreAPI::Objects::CountEnemyHeroesInRange(range, Position());
    }

    int CountAllyHeroesInRange(float range) const {
        return CoreAPI::Objects::CountAllyHeroesInRange(range, Position());
    }

    int CountEnemyMinionsInRange(float range) const {
        return CoreAPI::Objects::CountEnemyMinionsInRange(range, Position());
    }

    int CountAllyMinionsInRange(float range) const {
        return CoreAPI::Objects::CountAllyMinionsInRange(range, Position());
    }

    // ── Spell lookup by name (case-insensitive on both display + script) ─
    SpellSlot GetSpellSlot(const char* scriptOrSpellName) const {
        if (!scriptOrSpellName || !scriptOrSpellName[0]) {
            return SpellSlot::Unknown;
        }

        const auto spellbook = GetSpellBook();
        for (int i = 0; i <= static_cast<int>(SpellSlot::Unknown); ++i) {
            const auto slot = spellbook.GetSpell(static_cast<SpellSlot>(i));
            if (!slot.IsValid()) {
                continue;
            }

            const std::string spellName = slot.Name();
            const std::string scriptName = slot.ScriptName();
            if ((!spellName.empty() && lstrcmpiA(spellName.c_str(), scriptOrSpellName) == 0) ||
                (!scriptName.empty() && lstrcmpiA(scriptName.c_str(), scriptOrSpellName) == 0)) {
                return static_cast<SpellSlot>(i);
            }
        }

        return SpellSlot::Unknown;
    }

    SpellDataInstClient GetSpell(SpellSlot slot) const {
        return GetSpellBook().GetSpell(slot);
    }

    // ── Turret awareness ──────────────────────────────────────────────────
    bool IsUnderEnemyTurret(float extraRange = 0.0f) const {
        uintptr_t buffer[64] = {};
        const int count = CoreAPI::Objects::EnumerateEnemyTurrets(buffer, 64);
        for (int i = 0; i < count; ++i) {
            const GameObject turret(buffer[i]);
            if (!turret.IsValid() || turret.IsDead()) {
                continue;
            }
            if (Distance(turret) <= turret.AttackRange() + turret.BoundingRadius() + BoundingRadius() + extraRange) {
                return true;
            }
        }
        return false;
    }

    bool IsUnderAllyTurret(float extraRange = 0.0f) const {
        uintptr_t buffer[64] = {};
        const int count = CoreAPI::Objects::EnumerateAllyTurrets(buffer, 64);
        for (int i = 0; i < count; ++i) {
            const GameObject turret(buffer[i]);
            if (!turret.IsValid() || turret.IsDead()) {
                continue;
            }
            if (Distance(turret) <= turret.AttackRange() + turret.BoundingRadius() + BoundingRadius() + extraRange) {
                return true;
            }
        }
        return false;
    }

    // ── Interrupt heuristics ──────────────────────────────────────────────
    bool IsCastingInterruptableSpell(bool checkMovementInterruption = false) const {
        const auto cast = m_ref.GetActiveSpellCast();
        if (!cast.IsValid()) {
            return false;
        }
        if (!checkMovementInterruption) {
            return true;
        }
        return cast.GetCastDelay() > 0.25f;
    }

    bool IsCastingImporantSpell() const {
        return IsCastingInterruptableSpell(false);
    }

    // ── Orders ────────────────────────────────────────────────────────────
    bool IssueOrder(GameObjectOrder order, const Vector3& position) const {
        switch (order) {
        case GameObjectOrder::MoveTo:
            return CoreAPI::Control::IssueMove(position);
        case GameObjectOrder::AttackMove:
            return CoreAPI::Control::IssueAttackMove(position);
        case GameObjectOrder::HoldPosition:
        case GameObjectOrder::Stop:
            return CoreAPI::Control::IssueMove(Position());
        default:
            return false;
        }
    }

    bool IssueOrder(GameObjectOrder order, const GameObject& target) const {
        if (order != GameObjectOrder::AttackUnit || !target.IsValid()) {
            return false;
        }
        return CoreAPI::Control::IssueAttack(target.Address(), target.Position());
    }
};

} // namespace SDK
