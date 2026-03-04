#pragma once
#include "../core/Globals.h"
#include "../core/Offsets.h"
#include "../core/Vector.h"
#include "Enums.h"
#include <string>
#include <cmath>
#include <Windows.h>

// ============================================================================
// GameObject — Core game object wrapper
// Maps: EnsoulSharp GameObject → AttackableUnit → AIBaseClient → AIHeroClient
// All stats read as direct float (confirmed by Debug tab)
// ============================================================================

namespace SDK {

    class GameObject {
    public:
        uintptr_t address;

        GameObject() : address(0) {}
        GameObject(uintptr_t addr) : address(addr) {}

        bool IsValid() const { return address != 0 && Globals::IsValidPtr(address); }
        bool operator==(const GameObject& o) const { return address == o.address; }
        bool operator!=(const GameObject& o) const { return address != o.address; }
        operator bool() const { return IsValid(); }

        // ====================================================================
        // Identity
        // ====================================================================

        int GetNetId() const {
            return Globals::Read<int>(address + Offset::GameObject::NetId);
        }

        int GetIndex() const {
            return Globals::Read<int>(address + Offset::GameObject::Index);
        }

        GameObjectTeam GetTeam() const {
            // Read as byte at TeamAlt (0x259) — confirmed working
            return (GameObjectTeam)(int)Globals::Read<unsigned char>(address + Offset::GameObject::TeamAlt);
        }

        bool IsAlly(const GameObject& other) const {
            return GetTeam() == other.GetTeam();
        }

        bool IsEnemy(const GameObject& other) const {
            return GetTeam() != other.GetTeam() && GetTeam() != GameObjectTeam::Neutral;
        }

        // ====================================================================
        // Name
        // ====================================================================

        // Short name at 0x68 (e.g. "Annie", "SRU_Baron")
        std::string GetName() const {
            if (!IsValid()) return "";
            char buf[128] = {};
            if (!Globals::ReadGameString(address + Offset::GameObject::Name, buf, sizeof(buf)))
                return "";
            return std::string(buf);
        }

        // Champion display name at 0x4330 (e.g. "Annie", "Jinx")
        std::string GetChampionName() const {
            if (!IsValid()) return "";
            char buf[128] = {};
            if (!Globals::ReadGameString(address + Offset::GameObject::CharacterName, buf, sizeof(buf)))
                return "";
            return std::string(buf);
        }

        // ====================================================================
        // Position & Geometry
        // ====================================================================

        Vec3 GetPosition() const {
            return Globals::Read<Vec3>(address + Offset::GameObject::Position);
        }

        float GetBoundingRadius() const {
            typedef float(__fastcall* Fn)(uintptr_t);
            static uintptr_t fn = Globals::base + Offset::Function::GetBoundingRadius;
            __try {
                float r = ((Fn)fn)(address);
                if (r < 0.0f || r > 500.0f || std::isnan(r)) return 65.0f;
                return r;
            } __except(1) { return 65.0f; }
        }

        float DistanceTo(const GameObject& other) const {
            return GetPosition().Distance2D(other.GetPosition());
        }

        float DistanceTo(const Vec3& pos) const {
            return GetPosition().Distance2D(pos);
        }

        // ====================================================================
        // Health (Direct float read — confirmed working)
        // ====================================================================

        float GetHealth() const {
            return Globals::Read<float>(address + Offset::Health::HP);
        }

        float GetMaxHealth() const {
            return Globals::Read<float>(address + Offset::Health::MaxHP);
        }

        float GetHealthPercent() const {
            float max = GetMaxHealth();
            return max > 0.0f ? (GetHealth() / max * 100.0f) : 0.0f;
        }

        float GetAllShield() const {
            return Globals::Read<float>(address + Offset::Health::AllShield);
        }

        float GetPhysicalShield() const {
            return Globals::Read<float>(address + Offset::Health::PhysicalShield);
        }

        float GetMagicalShield() const {
            return Globals::Read<float>(address + Offset::Health::MagicalShield);
        }

        // ====================================================================
        // Mana / Resource
        // ====================================================================

        float GetMana() const {
            return Globals::Read<float>(address + Offset::Mana::MP);
        }

        float GetMaxMana() const {
            return Globals::Read<float>(address + Offset::Mana::MaxMP);
        }

        float GetManaPercent() const {
            float max = GetMaxMana();
            return max > 0.0f ? (GetMana() / max * 100.0f) : 0.0f;
        }

        // ====================================================================
        // Combat Stats (Direct float — confirmed working)
        // ====================================================================

        float GetBaseAD() const {
            return Globals::Read<float>(address + Offset::HeroStats::BaseAttackDamage);
        }

        float GetBonusAD() const {
            return Globals::Read<float>(address + Offset::HeroStats::FlatPhysicalDmgMod);
        }

        float GetTotalAD() const {
            return GetBaseAD() + GetBonusAD();
        }

        float GetAP() const {
            return Globals::Read<float>(address + Offset::HeroStats::BaseAbilityDamage);
        }

        float GetArmor() const {
            return Globals::Read<float>(address + Offset::HeroStats::Armor);
        }

        float GetBonusArmor() const {
            return Globals::Read<float>(address + Offset::HeroStats::BonusArmor);
        }

        float GetMR() const {
            return Globals::Read<float>(address + Offset::HeroStats::SpellBlock);
        }

        float GetBonusMR() const {
            return Globals::Read<float>(address + Offset::HeroStats::BonusSpellBlock);
        }

        float GetMoveSpeed() const {
            return Globals::Read<float>(address + Offset::HeroStats::MoveSpeed);
        }

        float GetAttackRange() const {
            return Globals::Read<float>(address + Offset::HeroStats::AttackRange);
        }

        float GetCrit() const {
            return Globals::Read<float>(address + Offset::HeroStats::Crit);
        }

        float GetCritMultiplier() const {
            return Globals::Read<float>(address + Offset::HeroStats::CritDamageMultiplier);
        }

        // ====================================================================
        // Penetration Stats
        // ====================================================================

        float GetArmorPenFlat() const {
            return Globals::Read<float>(address + Offset::HeroStats::FlatArmorPen);
        }

        float GetLethality() const {
            return Globals::Read<float>(address + Offset::HeroStats::PhysicalLethality);
        }

        float GetArmorPenPercent() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentArmorPen);
        }

        float GetMagicPenFlat() const {
            return Globals::Read<float>(address + Offset::HeroStats::FlatMagicPen);
        }

        float GetMagicPenPercent() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentMagicPen);
        }

        // ====================================================================
        // Lifesteal / Vamp
        // ====================================================================

        float GetLifeSteal() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentLifeSteal);
        }

        float GetSpellVamp() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentSpellVamp);
        }

        float GetOmnivamp() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentOmnivamp);
        }

        // ====================================================================
        // Attack Timing (via game functions)
        // ====================================================================

        float GetAttackDelay() const {
            typedef float(__cdecl* Fn)(uintptr_t);
            static uintptr_t fn = Globals::base + Offset::Function::GetAttackDelay;
            __try { return ((Fn)fn)(address); }
            __except(1) { return 0.625f; }
        }

        float GetAttackWindup() const {
            typedef float(__cdecl* Fn)(uintptr_t, int);
            static uintptr_t fn = Globals::base + Offset::Function::GetAttackWindup;
            __try { return ((Fn)fn)(address, 0x40); }
            __except(1) { return 0.3f; }
        }

        // Real attack range = attackRange + myBoundingRadius
        float GetRealAttackRange() const {
            return GetAttackRange() + GetBoundingRadius();
        }

        // Is target in real attack range?
        bool IsInAttackRange(const GameObject& target) const {
            float range = GetRealAttackRange() + target.GetBoundingRadius();
            return DistanceTo(target) <= range;
        }

        // ====================================================================
        // State
        // ====================================================================

        bool IsDead() const {
            return Globals::Read<int>(address + Offset::GameObject::Dead) != 0;
        }

        bool IsAlive() const {
            return !IsDead() && GetHealth() > 0.0f;
        }

        bool IsVisible() const {
            return Globals::Read<bool>(address + Offset::GameObject::Visible);
        }

        bool IsTargetable() const {
            return Globals::Read<bool>(address + Offset::Targetable::IsTargetable);
        }

        int GetActionState() const {
            return Globals::Read<int>(address + Offset::ActionState::State1);
        }

        bool CanAttack() const {
            return (GetActionState() & SDK::ActionState::CanAttack) != 0;
        }

        bool CanMove() const {
            return (GetActionState() & SDK::ActionState::CanMove) != 0;
        }

        bool CanCast() const {
            return (GetActionState() & SDK::ActionState::CanCast) != 0;
        }

        // ====================================================================
        // Type Checks (via function calls)
        // ====================================================================

        bool IsHero() const {
            typedef bool(__fastcall* Fn)(uintptr_t);
            static uintptr_t fn = Globals::base + Offset::Function::IsHero;
            __try { return ((Fn)fn)(address); }
            __except(1) { return false; }
        }

        bool IsTurret() const {
            typedef bool(__fastcall* Fn)(uintptr_t);
            static uintptr_t fn = Globals::base + Offset::Function::IsTurret;
            __try { return ((Fn)fn)(address); }
            __except(1) { return false; }
        }

        bool IsJungleMonster() const {
            typedef bool(__fastcall* Fn)(uintptr_t);
            static uintptr_t fn = Globals::base + Offset::Function::IsJungleMonster;
            __try { return ((Fn)fn)(address); }
            __except(1) { return false; }
        }

        bool IsDragon() const {
            typedef bool(__fastcall* Fn)(uintptr_t);
            static uintptr_t fn = Globals::base + Offset::Function::IsDragon;
            __try { return ((Fn)fn)(address); }
            __except(1) { return false; }
        }

        bool IsBaron() const {
            typedef bool(__fastcall* Fn)(uintptr_t);
            static uintptr_t fn = Globals::base + Offset::Function::IsBaron;
            __try { return ((Fn)fn)(address); }
            __except(1) { return false; }
        }

        // ====================================================================
        // Valid Target Check (for TargetSelector)
        // ====================================================================

        bool IsValidTarget(float range = 25000.0f, const Vec3& from = Vec3()) const {
            if (!IsValid()) return false;
            if (!IsAlive()) return false;
            if (!IsVisible()) return false;
            if (!IsTargetable()) return false;
            if (range > 0.0f) {
                Vec3 fromPos = from.IsZero() ? Vec3() : from;
                // TODO: use player pos if fromPos is zero
            }
            return true;
        }

        // ====================================================================
        // Hero-specific
        // ====================================================================

        float GetGold() const {
            return Globals::Read<float>(address + Offset::Hero::Gold);
        }

        float GetGoldTotal() const {
            return Globals::Read<float>(address + Offset::Hero::GoldTotal);
        }

        float GetExp() const {
            return Globals::Read<float>(address + Offset::Hero::Exp);
        }

        int GetLevel() const {
            return Globals::Read<int>(address + Offset::Hero::LevelRef);
        }

        // ====================================================================
        // Minion type from LaneMinionType
        // ====================================================================

        MinionType GetMinionType() const {
            return (MinionType)(int)Globals::Read<unsigned char>(address + 0x4C79);
        }

        // ====================================================================
        // Damage Calculation Helpers
        // ====================================================================

        float CalcPhysicalDamage(const GameObject& target) const {
            float damage = GetTotalAD();
            float armor = target.GetArmor();
            // Simple armor reduction
            if (armor >= 0)
                return damage * (100.0f / (100.0f + armor));
            else
                return damage * (2.0f - 100.0f / (100.0f - armor));
        }

        float CalcMagicalDamage(const GameObject& target, float rawMagic) const {
            float mr = target.GetMR();
            if (mr >= 0)
                return rawMagic * (100.0f / (100.0f + mr));
            else
                return rawMagic * (2.0f - 100.0f / (100.0f - mr));
        }

        float GetEffectiveHealthAD() const {
            return GetHealth() * (1.0f + GetArmor() / 100.0f);
        }

        float GetEffectiveHealthAP() const {
            return GetHealth() * (1.0f + GetMR() / 100.0f);
        }
    };

} // namespace SDK
