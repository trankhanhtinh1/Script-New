#pragma once

// ============================================================================
// GameObject — base wrapper for every in-world entity
// ============================================================================
// Owns a `CoreObjects::ObjectRef` (raw pointer + SEH-guarded readers). Exposes
// identity (NetId / Index / Team / Name / CharacterName), full stat snapshot
// (HP, mana, AD, AP, armor, MR, shields, crit, attack speed, penetrations,
// life steal, …), geometry (position / server position / velocity / path),
// state flags (alive / targetable / immobile / dashing / visible / recalling),
// classification (IsHero / IsMinion / IsTurret / IsMissile / IsJungleMonster
// via RuntimeAPI pattern-match), buff lookups, distance helpers, and the
// `GameObjectType` enum. Subclasses add only the APIs that are specific to
// their layer (combat calc for AIBaseClient, missile endpoints for
// MissileClient, …) and never re-derive primitive accessors.
//
// `GameObject` is deliberately lightweight so it can live in `std::vector`
// returns from `ObjectManager::` enumerators without heap pressure.
// ============================================================================

#include "../../core/CoreAPI.h"
#include "../../core/CoreClassification.h"
#include "../../core/Vector.h"
#include "../Enumerations/SpellSlot.h"

#include <cfloat>
#include <string>
#include <vector>

namespace SDK {

using Vector2 = Vec2;
using Vector3 = Vec3;

enum class GameObjectType : int {
    Unknown = 0,
    AIHeroClient,
    AIMinionClient,
    AITurretClient,
    MissileClient,
    HQClient,
    Barracks,
    BarracksDampenerClient,
    BuildingClient
};

class GameObject {
public:
    GameObject() = default;
    explicit GameObject(uintptr_t address)
        : m_ref{ address } {}
    explicit GameObject(const CoreObjects::ObjectRef& ref)
        : m_ref(ref) {}

    uintptr_t Address() const { return m_ref.address; }
    int NetworkId() const { return m_ref.GetNetId(); }
    int Index() const { return m_ref.GetIndex(); }
    int Team() const { return m_ref.GetTeam(); }
    int Level() const { return m_ref.GetLevel(); }

    Vector3 Position() const { return m_ref.GetPosition(); }
    Vector3 PreviousPosition() const { return m_ref.GetPreviousPosition(); }
    Vector3 Direction() const { return m_ref.GetDirection(); }
    Vector3 ServerPosition() const { return m_ref.GetServerPosition(); }
    Vector3 Velocity() const { return m_ref.GetVelocity(); }
    Vector3 PathEnd() const { return m_ref.GetPathEnd(); }
    Vector3 OrderPosition() const { return m_ref.GetOrderPosition(); }

    // ── Stat block ──────────────────────────────────────────────────────────
    float Health() const { return m_ref.GetHealth(); }
    float MaxHealth() const { return m_ref.GetMaxHealth(); }
    float HealthPercent() const { return m_ref.GetHealthPercent(); }
    float Mana() const { return m_ref.GetMana(); }
    float MaxMana() const { return m_ref.GetMaxMana(); }
    float ManaPercent() const { return m_ref.GetManaPercent(); }
    float MoveSpeed() const { return m_ref.GetMoveSpeed(); }
    float HPRegenRate() const { return m_ref.GetHPRegenRate(); }
    float BaseHPRegenRate() const { return m_ref.GetBaseHPRegenRate(); }
    float BoundingRadius() const { return m_ref.GetBoundingRadius(); }
    float AttackRange() const { return m_ref.GetAttackRange(); }
    float BaseAttackDamage() const { return m_ref.GetBaseAD(); }
    float BonusAttackDamage() const { return m_ref.GetBonusAD(); }
    float TotalAttackDamage() const { return m_ref.GetTotalAD(); }
    float AbilityPower() const { return m_ref.GetAbilityPower(); }
    float TotalMagicalDamage() const { return AbilityPower(); }
    float FlatMagicDamageMod() const { return AbilityPower(); }
    float Armor() const { return m_ref.GetArmor(); }
    float SpellBlock() const { return m_ref.GetSpellBlock(); }
    float BonusArmor() const { return m_ref.GetBonusArmor(); }
    float BonusSpellBlock() const { return m_ref.GetBonusSpellBlock(); }
    float AllShield() const { return m_ref.GetAllShield(); }
    float PhysicalShield() const { return m_ref.GetPhysicalShield(); }
    float MagicalShield() const { return m_ref.GetMagicalShield(); }
    float TotalShield() const { return m_ref.GetTotalShield(); }
    float Crit() const { return m_ref.GetCrit(); }
    float CritMultiplier() const { return m_ref.GetCritMultiplier(); }
    float AttackSpeedMod() const { return m_ref.GetAttackSpeedMod(); }
    float PercentAttackSpeedMod() const { return m_ref.GetPercentAttackSpeedMod(); }
    float FlatBaseAttackSpeedMod() const { return m_ref.GetFlatBaseAttackSpeedMod(); }
    float AbilityHaste() const { return m_ref.GetAbilityHaste(); }
    float FlatArmorPenetration() const { return m_ref.GetArmorPenFlat(); }
    float Lethality() const { return m_ref.GetLethality(); }
    float PercentArmorPenetration() const { return m_ref.GetArmorPenPercent(); }
    float PercentBonusArmorPenetration() const { return m_ref.GetBonusArmorPenPercent(); }
    float FlatMagicPenetration() const { return m_ref.GetMagicPenFlat(); }
    float PercentMagicPenetration() const { return m_ref.GetMagicPenPercent(); }
    float PercentBonusMagicPenetration() const { return m_ref.GetBonusMagicPenPercent(); }
    float LifeSteal() const { return m_ref.GetLifeSteal(); }
    float SpellVamp() const { return m_ref.GetSpellVamp(); }
    float OmniVamp() const { return m_ref.GetOmnivamp(); }

    // ── EnsoulSharp-style short aliases ─────────────────────────────────────
    // Canonical short stat accessors used by EnsoulSharp script code:
    //   `unit.AD()`     → total attack damage  (base + bonus)
    //   `unit.AP()`     → ability power        (= TotalMagicalDamage)
    //   `unit.MR()`     → magic resist         (= SpellBlock, "MagicResist")
    //   `unit.HP()`     → current health       (alias of Health)
    //   `unit.MaxHP()`  → max health           (alias of MaxHealth)
    // Long-form names above are preserved for backwards compat.
    float AD()    const { return TotalAttackDamage(); }
    float AP()    const { return AbilityPower(); }
    float MR()    const { return SpellBlock(); }
    float HP()    const { return Health(); }
    float MaxHP() const { return MaxHealth(); }

    // ── State flags ────────────────────────────────────────────────────────
    bool IsValid() const { return m_ref.IsValid(); }
    bool IsAlive() const { return m_ref.IsAlive(); }
    bool IsDead() const { return m_ref.IsDead(); }
    bool IsVisible() const { return m_ref.IsVisible(); }
    bool IsTargetable() const { return m_ref.IsTargetable(); }
    bool IsInvulnerable() const { return m_ref.IsInvulnerable(); }
    bool IsRecalling() const { return m_ref.IsRecalling(); }
    bool IsWindingUp() const { return m_ref.IsWindingUp(); }
    bool IsMoving() const { return m_ref.IsMovingOnPath(); }
    bool IsDashing() const { return m_ref.IsDashingOnPath(); }
    bool IsImmobile() const { return m_ref.IsImmobile(); }
    bool IsAlly() const { return m_ref.IsAlly(); }
    bool IsEnemy() const { return m_ref.IsEnemy(); }
    // Classification via name-pattern (replaces deprecated RuntimeAPI:: calls).
    // Single Classify() call cached per use-site; no syscall, no obfuscation dep.
    bool IsHero()           const { return CoreClassification::Classify(Address()) == CoreClassification::ObjectType::Hero; }
    bool IsLaneMinion()     const { return CoreClassification::Classify(Address()) == CoreClassification::ObjectType::LaneMinion; }
    bool IsTurret()         const { return CoreClassification::Classify(Address()) == CoreClassification::ObjectType::Turret; }
    bool IsPlant()          const { return CoreClassification::Classify(Address()) == CoreClassification::ObjectType::Plant; }
    bool IsPet()            const { return CoreClassification::Classify(Address()) == CoreClassification::ObjectType::Pet; }
    bool IsJungleMonster()  const {
        const auto t = CoreClassification::Classify(Address());
        return t == CoreClassification::ObjectType::JungleMonster
            || t == CoreClassification::ObjectType::JungleBig
            || t == CoreClassification::ObjectType::JungleEpic
            || t == CoreClassification::ObjectType::Scuttle;
    }
    bool IsMinion() const {
        const auto t = CoreClassification::Classify(Address());
        return t == CoreClassification::ObjectType::LaneMinion
            || t == CoreClassification::ObjectType::JungleMonster
            || t == CoreClassification::ObjectType::JungleBig
            || t == CoreClassification::ObjectType::JungleEpic
            || t == CoreClassification::ObjectType::Scuttle
            || t == CoreClassification::ObjectType::Pet;
    }
    bool IsNeutral() const { return IsJungleMonster() || IsPlant(); }
    bool IsMissile() const { return m_ref.IsMissile(); }  // vtable check, lives on ObjectRef
    bool IsMelee() const { return m_ref.IsMelee(); }
    bool IsRanged() const { return m_ref.IsRanged(); }
    bool IsMe() const {
        // Phase 2 (Apr 26/2026): GetLocalPlayer() returns ObjectRef (struct
        // with `address` field), not raw uintptr_t. Compare via .address.
        const auto local = CoreAPI::Objects::GetLocalPlayer();
        return local.address != 0 && local.address == Address();
    }

    // ── Buffs ──────────────────────────────────────────────────────────────
    bool HasBuff(const char* name) const { return m_ref.HasBuff(name); }
    int GetBuffStacks(const char* name) const { return m_ref.GetBuffStacks(name); }
    int GetBuffCount(const char* name) const { return m_ref.GetBuffStacks(name); }
    float GetBuffRemainingTime(const char* name, float gameTime = CoreAPI::Game::GetTime()) const {
        return m_ref.GetBuffRemainingTime(name, gameTime);
    }

    // ── Targeting / distance ───────────────────────────────────────────────
    bool IsValidTarget(float range = FLT_MAX, const Vector3& from = Vector3()) const {
        return m_ref.IsValidTarget(range, true, true, from);
    }

    float Distance(const GameObject& other) const { return m_ref.DistanceTo(other.m_ref); }
    float Distance(const Vector3& pos) const { return m_ref.DistanceTo(pos); }
    float DistanceSquared(const GameObject& other) const {
        const Vector3 pos = Position();
        const Vector3 otherPos = other.Position();
        return pos.DistanceSqr2D(otherPos);
    }
    float DistanceSquared(const Vector3& pos) const {
        return Position().DistanceSqr2D(pos);
    }
    float DistanceToPlayer() const {
        const CoreObjects::ObjectRef local{ CoreAPI::Objects::GetLocalPlayer() };
        return local.IsValid() ? m_ref.DistanceTo(local.GetPosition()) : FLT_MAX;
    }

    // ── Path ──────────────────────────────────────────────────────────────
    std::vector<Vector3> GetWaypoints() const {
        std::vector<Vector3> path = {};
        if (!IsValid()) {
            return path;
        }

        Vector3 wpBuf[32] = {};
        const int count = CoreAPI::Ai::CopyWaypoints(Address(), reinterpret_cast<Vec3*>(wpBuf), 32);
        if (count > 0) {
            path.reserve(static_cast<size_t>(count));
            for (int i = 0; i < count; ++i) {
                path.push_back(wpBuf[i]);
            }
        }

        if (path.empty()) {
            const Vector3 start = ServerPosition().IsZero() ? Position() : ServerPosition();
            path.push_back(start);
            const Vector3 end = PathEnd();
            if (!end.IsZero() && end.Distance2D(start) > 1.0f) {
                path.push_back(end);
            }
        }

        return path;
    }

    std::vector<Vector3> Path() const {
        return GetWaypoints();
    }

    int GetPathLength() const {
        return static_cast<int>(GetWaypoints().size());
    }

    // ── Names / type ──────────────────────────────────────────────────────
    std::string Name() const {
        char buf[128] = {};
        return m_ref.ReadName(buf, sizeof(buf)) ? std::string(buf) : std::string();
    }

    std::string CharacterName() const {
        char buf[128] = {};
        return m_ref.ReadCharacterName(buf, sizeof(buf)) ? std::string(buf) : std::string();
    }

    GameObjectType Type() const {
        if (IsHero()) return GameObjectType::AIHeroClient;
        if (IsTurret()) return GameObjectType::AITurretClient;
        if (IsMissile()) return GameObjectType::MissileClient;
        if (IsMinion()) return GameObjectType::AIMinionClient;
        return GameObjectType::Unknown;
    }

    bool Compare(const GameObject& other) const {
        return Address() == other.Address();
    }

    const CoreObjects::ObjectRef& Ref() const { return m_ref; }

protected:
    CoreObjects::ObjectRef m_ref = {};
};

} // namespace SDK
