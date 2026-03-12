#pragma once
#include "core/Globals.h"
#include "core/Offsets.h"
#include "core/Vector.h"
#include "Enums.h"
#include "Game.h"
#include "BuffManager.h"
#include "AiManager.h"
#include "NavGrid.h"
#include "SpellBook.h"
#include "sdk/Utils/Bypass.h"
#include <string>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#include "spoof/spoofcall.h"

// ============================================================================
// GameObject — Core game object wrapper (EnsoulSharp API compatible)
// Maps: EnsoulSharp GameObject → AttackableUnit → AIBaseClient → AIHeroClient
// All stats read as direct float (confirmed by Debug tab)
// ============================================================================

namespace SDK {

    // Forward declaration for DamageCalc (avoid circular include)
    class DamageCalc;

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
        int NetworkId() const { return GetNetId(); }  // EnsoulSharp alias

        int GetIndex() const {
            return Globals::Read<int>(address + Offset::GameObject::Index);
        }

        GameObjectTeam GetTeam() const {
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

        std::string GetName() const {
            if (!IsValid()) return "";
            char buf[128] = {};
            if (!Globals::ReadGameString(address + Offset::GameObject::Name, buf, sizeof(buf)))
                return "";
            return std::string(buf);
        }

        std::string GetChampionName() const {
            if (!IsValid()) return "";
            char buf[128] = {};
            if (!Globals::ReadGameString(address + Offset::GameObject::CharacterName, buf, sizeof(buf)))
                return "";
            return std::string(buf);
        }

        // EnsoulSharp alias
        std::string CharacterName() const { return GetChampionName(); }

        // ====================================================================
        // Position & Geometry
        // ====================================================================

        Vec3 GetPosition() const {
            return Globals::Read<Vec3>(address + Offset::GameObject::Position);
        }
        Vec3 Position() const { return GetPosition(); }  // EnsoulSharp alias

        Vec3 GetDirection() const {
            return Globals::Read<Vec3>(address + Offset::GameObject::Direction);
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
        float BoundingRadius() const { return GetBoundingRadius(); }

        float DistanceTo(const GameObject& other) const {
            return GetPosition().Distance2D(other.GetPosition());
        }

        float DistanceTo(const Vec3& pos) const {
            return GetPosition().Distance2D(pos);
        }

        float Distance(const GameObject& other) const { return DistanceTo(other); }
        float Distance(const Vec3& pos) const { return DistanceTo(pos); }

        // ====================================================================
        // Health (Direct float read — confirmed working)
        // ====================================================================

        float GetHealth() const {
            return Globals::Read<float>(address + Offset::Health::HP);
        }
        float Health() const { return GetHealth(); }

        float GetMaxHealth() const {
            return Globals::Read<float>(address + Offset::Health::MaxHP);
        }
        float MaxHealth() const { return GetMaxHealth(); }

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

        // === GetRealHealth by damage type (EnsoulSharp: x.GetRealHeath(damageType)) ===
        float GetRealHealth(DamageType type = DamageType::Physical) const {
            float hp = GetHealth();
            float allShield = GetAllShield();
            switch (type) {
            case DamageType::Physical:
                return hp + GetPhysicalShield() + allShield;
            case DamageType::Magical:
                return hp + GetMagicalShield() + allShield;
            case DamageType::True:
                return hp + allShield;
            default:
                return hp + allShield;
            }
        }

        // ====================================================================
        // Mana / Resource
        // ====================================================================

        float GetMana() const {
            return Globals::Read<float>(address + Offset::Mana::MP);
        }
        float Mana() const { return GetMana(); }

        float GetMaxMana() const {
            return Globals::Read<float>(address + Offset::Mana::MaxMP);
        }
        float MaxMana() const { return GetMaxMana(); }

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
        float TotalAttackDamage() const { return GetTotalAD(); }

        float GetAP() const {
            return Globals::Read<float>(address + Offset::HeroStats::BaseAbilityDamage);
        }
        float TotalAbilityPower() const { return GetAP(); }

        float GetArmor() const {
            return Globals::Read<float>(address + Offset::HeroStats::Armor);
        }
        // NOTE: GetBonusArmor() removed — Armor offset (0x2060) already returns TOTAL armor.
        //       Do not add base and bonus separately — it would double count.

        float GetMR() const {
            return Globals::Read<float>(address + Offset::HeroStats::SpellBlock);
        }

        float GetBonusMR() const {
            return Globals::Read<float>(address + Offset::HeroStats::BonusSpellBlock);
        }

        float GetMoveSpeed() const {
            return Globals::Read<float>(address + Offset::HeroStats::MoveSpeed);
        }
        float MoveSpeed() const { return GetMoveSpeed(); }

        float GetAttackRange() const {
            return Globals::Read<float>(address + Offset::HeroStats::AttackRange);
        }
        float AttackRange() const { return GetAttackRange(); }

        float GetCrit() const {
            return Globals::Read<float>(address + Offset::HeroStats::Crit);
        }
        float GetCritChance() const { return GetCrit(); }  // EnsoulSharp alias

        float GetCritMultiplier() const {
            return Globals::Read<float>(address + Offset::HeroStats::CritDamageMultiplier);
        }

        float GetAttackSpeedMod() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentAttackSpeedMod);
        }

        float GetAbilityHaste() const {
            return Globals::Read<float>(address + Offset::HeroStats::AbilityHaste);
        }

        float GetHPRegen() const {
            return Globals::Read<float>(address + Offset::HeroStats::HPRegenRate);
        }

        float GetPercentCCReduction() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentCCReduction);
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

        float GetBonusArmorPenPercent() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentBonusArmorPen);
        }

        float GetMagicPenFlat() const {
            return Globals::Read<float>(address + Offset::HeroStats::FlatMagicPen);
        }

        float GetMagicPenPercent() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentMagicPen);
        }

        float GetBonusMagicPenPercent() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentBonusMagicPen);
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
        float AttackDelay() const { return GetAttackDelay(); }

        float GetAttackWindup() const {
            typedef float(__cdecl* Fn)(uintptr_t, int);
            static uintptr_t fn = Globals::base + Offset::Function::GetAttackWindup;
            __try { return ((Fn)fn)(address, 0x40); }
            __except(1) { return 0.3f; }
        }
        float AttackCastDelay() const { return GetAttackWindup(); }  // EnsoulSharp alias

        // Real attack range = attackRange + myBoundingRadius
        float GetRealAutoAttackRange(const GameObject* target = nullptr) const {
            float range = GetAttackRange() + GetBoundingRadius();
            if (target && target->IsValid())
                range += target->GetBoundingRadius();
            return range;
        }
        float GetRealAttackRange() const { return GetAttackRange() + GetBoundingRadius(); }

        // Is target in real attack range? (EnsoulSharp: InAutoAttackRange)
        bool IsInAttackRange(const GameObject& target, float extraRange = 0.0f) const {
            float range = GetRealAutoAttackRange(&target) + extraRange;
            return DistanceTo(target) <= range;
        }
        bool InAutoAttackRange(const GameObject& target) const { return IsInAttackRange(target); }

        // ====================================================================
        // IsMelee / IsRanged (EnsoulSharp: IsMelee property)
        // ====================================================================

        bool IsMelee() const {
            return GetAttackRange() < 300.0f;
        }

        bool IsRanged() const {
            return !IsMelee();
        }

        // ====================================================================
        // State
        // ====================================================================

        bool IsDead() const {
            if (!IsValid()) return true;
            typedef bool(__fastcall* Fn)(uintptr_t);
            static uintptr_t fn = Globals::base + Offset::Function::IsDead;
            __try { return ((Fn)fn)(address); }
            __except(1) { return true; }
        }

        bool IsAlive() const {
            if (!IsValid()) return false;
            typedef bool(__fastcall* Fn)(uintptr_t);
            static uintptr_t fn = Globals::base + Offset::Function::IsAlive;
            __try { return ((Fn)fn)(address); }
            __except(1) { return false; }
        }

        bool IsVisible() const {
            if (!IsValid()) return false;
            return Globals::Read<unsigned char>(address + Offset::GameObject::Visible) == 1;
        }

        bool IsTargetable() const {
            return Globals::Read<bool>(address + Offset::Targetable::IsTargetable);
        }

        bool IsInvulnerable() const {
            return Globals::Read<bool>(address + Offset::GameObject::IsInvulnerable);
        }

        int GetActionState() const {
            return Globals::Read<int>(address + Offset::ActionState::State1);
        }

        int GetActionState2() const {
            return Globals::Read<int>(address + Offset::ActionState::State2);
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

        bool IsWindingUp() const {
            // Check if in auto-attack wind-up (has active spell cast)
            SpellBook sb(address);
            return sb.IsCasting();
        }

        // ====================================================================
        // Zombie check (EnsoulSharp: IsZombie)
        // Sion passive, Karthus passive, Kog'Maw passive
        // ====================================================================

        bool IsZombie() const {
            if (!IsValid()) return false;
            BuffManager bm(address);
            return bm.HasBuff("SionPassiveZone") ||
                   bm.HasBuff("KarthusDeathDefiedBuff") ||
                   bm.HasBuff("KogMawIcathianSurprise");
        }

        // ====================================================================
        // Movement (from AiManager)
        // ====================================================================

        AiManager GetAiManager() const {
            return AiManager(address);
        }

        bool IsMoving() const {
            AiManager ai(address);
            return ai.IsMoving();
        }

        bool IsDashing() const {
            AiManager ai(address);
            return ai.IsDashing();
        }

        float GetDashSpeed() const {
            AiManager ai(address);
            return ai.GetDashSpeed();
        }

        Vec3 GetServerPosition() const {
            AiManager ai(address);
            if (ai.IsValid()) return ai.GetServerPosition();
            return GetPosition(); // fallback
        }
        Vec3 ServerPosition() const { return GetServerPosition(); }

        Vec3 GetPathEnd() const {
            AiManager ai(address);
            return ai.GetPathEnd();
        }

        std::vector<Vec3> GetWaypoints() const {
            AiManager ai(address);
            auto path = ai.GetRemainingPath();
            if (path.empty()) {
                path.push_back(GetServerPosition());
            }
            return path;
        }
        std::vector<Vec3> Path() const { return GetWaypoints(); }

        int GetPathLength() const {
            return (int)GetWaypoints().size();
        }

        // ====================================================================
        // IsMe check
        // ====================================================================

        bool IsMe() const {
            uintptr_t localAddr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::LocalPlayer);
            return address == localAddr;
        }

        // ====================================================================
        // Buff Access (directly on GameObject — EnsoulSharp API)
        // ====================================================================

        BuffManager GetBuffManager() const {
            return BuffManager(address);
        }

        bool HasBuff(const char* name) const {
            return BuffManager(address).HasBuff(name);
        }

        bool HasBuffOfType(BuffType type) const {
            return BuffManager(address).HasBuffOfType(type);
        }

        Buff GetBuff(const char* name) const {
            Buff result;
            BuffManager(address).ForEach([&](Buff& buff) {
                if (result.IsValid()) return;
                std::string bname = buff.GetName();
                if (_stricmp(bname.c_str(), name) == 0)
                    result = buff;
            });
            return result;
        }

        float GetBuffRemainingTime(const char* name) const {
            return BuffManager(address).GetBuffRemainingTime(name);
        }

        int GetBuffStacks(const char* name) const {
            return BuffManager(address).GetBuffStacks(name);
        }

        // CC state helpers
        bool IsStunned() const { return HasBuffOfType(BuffType::Stun); }
        bool IsSilenced() const { return HasBuffOfType(BuffType::Silence); }
        bool IsCharmed() const { return HasBuffOfType(BuffType::Charm); }
        bool IsFeared() const { return HasBuffOfType(BuffType::Fear); }
        bool IsSuppressed() const { return HasBuffOfType(BuffType::Suppression); }
        bool IsSnared() const { return HasBuffOfType(BuffType::Snare); }
        bool IsSlowed() const { return HasBuffOfType(BuffType::Slow); }
        bool IsAsleep() const { return HasBuffOfType(BuffType::Asleep); }
        bool IsGrounded() const { return HasBuffOfType(BuffType::Grounded); }
        bool IsPolymorphed() const { return HasBuffOfType(BuffType::Polymorph); }
        bool IsTaunted() const { return HasBuffOfType(BuffType::Taunt); }

        bool IsImmobile() const {
            return IsStunned() || IsCharmed() || IsFeared() ||
                   IsSuppressed() || IsSnared() || IsAsleep();
        }

        // ====================================================================
        // SpellBook Access
        // ====================================================================

        SpellBook GetSpellBook() const {
            return SpellBook(address);
        }

        SpellSlot GetSpell(SpellSlotId slot) const {
            return SpellBook(address).GetSpell(slot);
        }

        // Quick spell accessors
        SpellSlot Q() const { return GetSpell(SpellSlotId::Q); }
        SpellSlot W() const { return GetSpell(SpellSlotId::W); }
        SpellSlot E() const { return GetSpell(SpellSlotId::E); }
        SpellSlot R() const { return GetSpell(SpellSlotId::R); }
        SpellSlot Summoner1() const { return GetSpell(SpellSlotId::Summoner1); }
        SpellSlot Summoner2() const { return GetSpell(SpellSlotId::Summoner2); }

        // Check if summoner spell is a specific spell (Flash, Ignite, Smite, etc.)
        bool HasSummonerSpell(const char* name) const {
            std::string s1 = Summoner1().GetName();
            std::string s2 = Summoner2().GetName();
            return (_stricmp(s1.c_str(), name) == 0) || (_stricmp(s2.c_str(), name) == 0);
        }

        // Get summoner spell slot by name
        SpellSlotId GetSummonerSlot(const char* name) const {
            std::string s1 = Summoner1().GetName();
            if (_stricmp(s1.c_str(), name) == 0) return SpellSlotId::Summoner1;
            std::string s2 = Summoner2().GetName();
            if (_stricmp(s2.c_str(), name) == 0) return SpellSlotId::Summoner2;
            return SpellSlotId::Summoner1; // fallback
        }

        // Check if spell at given slot index is ready (learned + off cooldown)
        bool CanUseSpell(int slotIndex) const {
            if (!IsValid() || slotIndex < 0 || slotIndex > 13) return false;
            SpellSlot slot = SpellBook(address).GetSpell(static_cast<SpellSlotId>(slotIndex));
            return slot.IsValid() && slot.IsReady();
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
            if (!IsValid()) return false;
            if (IsPlant()) return false;
            
            // Check native function first
            bool nativeJungle = CallNativeIsJungleMonster();

            if (GetTeam() != GameObjectTeam::Neutral)
                return false;

            const std::string objectName = GetName();
            const std::string characterName = GetChampionName();
            const char* objectNamePtr = objectName.c_str();
            const char* characterNamePtr = characterName.c_str();

            if (nativeJungle) return true;

            return strstr(objectNamePtr, "SRU_") != nullptr ||
                strstr(characterNamePtr, "SRU_") != nullptr ||
                strstr(objectNamePtr, "Sru_Crab") != nullptr ||
                strstr(characterNamePtr, "Sru_Crab") != nullptr ||
                strstr(objectNamePtr, "TT_") != nullptr ||
                strstr(characterNamePtr, "TT_") != nullptr;
        }

        bool CallNativeIsJungleMonster() const {
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
        // Object Classification using game-internal MinionType byte
        // From sub_BBB10: 0=Unset,1=Pet,2=Jungle,3=Team,4=Melee,5=Ranged,6=Cannon,7=Super
        // Offset: Minion::LaneType (0x4CC9) on the object
        // ====================================================================

        // CompareTypeFlags — calls game function sub_29CD30 to check obfuscated type flags
        bool CompareTypeFlags(int flag) const {
            typedef bool(__fastcall* Fn)(uintptr_t, int, int, uintptr_t);
            static uintptr_t fn = Globals::base + Offset::Function::CompareTypeFlags;
            __try { return ((Fn)fn)(address, 0, flag, address); }
            __except(1) { return false; }
        }

        bool IsMinion() const {
            if (!IsValid()) return false;
            float maxHP = GetMaxHealth();
            GameObjectTeam team = GetTeam();
            return (team == GameObjectTeam::Blue || team == GameObjectTeam::Red)
                && maxHP > 0.0f && maxHP < 10000.0f && !IsHero() && !IsTurret();
        }

        bool IsLaneMinion() const {
            if (!IsMinion()) return false;
            uint8_t laneType = Globals::Read<uint8_t>(address + Offset::Minion::LaneType);
            return laneType >= 4 && laneType <= 7;
        }

        bool IsPet() const {
            if (!IsValid()) return false;
            // Primary: game-internal MinionType == Pet(1)
            MinionType mt = GetMinionType();
            if (mt == MinionType::Pet) return true;
            // Fallback: string-based for edge cases
            std::string name = GetName();
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
                return (char)std::tolower(c);
            });
            // Known pet names (traps, summons, clones)
            static const char* const kPets[] = {
                "annietibbers", "elisespiderling", "heimertyellow", "heimertblue",
                "ivernminion", "malzaharvoidling", "shacobox", "teemomushroom",
                "yorickghoulmelee", "yorickbigghoul", "yorickmistwalker",
                "zyrathornplant", "zyragraspingplant", "illaoiminion",
                "azirsoldierghost", "voidspawn", "jihnmine"
            };
            for (auto& p : kPets) {
                if (lower == p) return true;
            }
            return false;
        }

        bool IsWard() const {
            if (!IsValid()) return false;
            std::string name = GetName();
            // Check exact ward object names (fast)
            static const char* const kWards[] = {
                "SightWard", "YellowTrinket", "BlueTrinket", "JammerDevice",
                "YellowTrinketUpgrade", "VisionWard", "ControlWard"
            };
            for (auto& w : kWards) {
                if (name == w) return true;
            }
            // Fallback substring check
            return name.find("Ward") != std::string::npos ||
                   name.find("ward") != std::string::npos;
        }

        bool IsPlant() const {
            if (!IsValid()) return false;
            // Jungle plants are Neutral team only
            if (GetTeam() != GameObjectTeam::Neutral) return false;
            // Primary: use game type flags (TypeFlags::Plant = 0x8000)
            if (CompareTypeFlags(0x8000)) return true;
            
            // Fallback: string-based for reliability
            std::string name = GetName();
            std::string champName = GetChampionName();
            
            std::string lowerName = name;
            std::string lowerChamp = champName;
            
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            std::transform(lowerChamp.begin(), lowerChamp.end(), lowerChamp.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            
            if (lowerName.find("sru_plant") != std::string::npos || lowerChamp.find("sru_plant") != std::string::npos) return true;
            if (lowerName.find("hiddenminionplantdemon") != std::string::npos || lowerChamp.find("hiddenminionplantdemon") != std::string::npos) return true;
            if (lowerName.find("planthealthmirrored") != std::string::npos || lowerChamp.find("planthealthmirrored") != std::string::npos) return true;
            if (lowerName.find("plantmasterminion") != std::string::npos || lowerChamp.find("plantmasterminion") != std::string::npos) return true;
            
            return false;
        }

        bool IsBarrel() const {
            std::string name = GetName();
            return name.find("gangplankbarrel") != std::string::npos;
        }

        // ====================================================================
        // Valid Target Check (EnsoulSharp compatible)
        // ====================================================================

        bool IsValidTarget(float range = 25000.0f, bool checkVisibility = true, const Vec3& from = Vec3()) const {
            if (!IsValid()) return false;
            if (!IsAlive()) return false;
            if (checkVisibility && !IsVisible()) return false;
            if (!IsTargetable()) return false;

            // IsZombie check (Sion passive, Karthus passive, Kog'Maw passive)
            if (IsZombie()) return false;

            // Tryndamere Undying Rage check: skip if very low HP + has UndyingRage
            if (GetHealth() <= 71.0f && HasBuff("UndyingRage")) return false;

            // Range check
            if (range > 0.0f && range < 25000.0f) {
                Vec3 origin = from.IsZero() ?
                    Globals::Read<Vec3>(
                        Globals::Read<uintptr_t>(Globals::base + Offset::Global::LocalPlayer)
                        + Offset::GameObject::Position) : from;
                float dist = GetPosition().Distance2D(origin);
                if (dist > range) return false;
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
        int Level() const { return GetLevel(); }

        float GetVisionScore() const {
            return Globals::Read<float>(address + Offset::Hero::VisionScore);
        }

        // ====================================================================
        // Minion type from LaneMinionType
        // ====================================================================

        MinionType GetMinionType() const {
            return (MinionType)(int)Globals::Read<unsigned char>(address + Offset::Minion::LaneType);
        }

        // ====================================================================
        // Recall State
        // ====================================================================

        int GetRecallState() const {
            return Globals::Read<int>(address + Offset::GameObject::RecallState);
        }

        bool IsRecalling() const {
            return GetRecallState() != 0;
        }

        // ====================================================================
        // IssueOrder (EnsoulSharp API: player.IssueOrder(type, target/pos))
        // ====================================================================

        void IssueOrder(OrderType type, const Vec3& pos) const {
            if (!IsValid()) return;

            static void* trampoline = nullptr;
            if (!trampoline) {
                MODULEINFO mi{};
                GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(nullptr), &mi, sizeof(mi));
                char* base = (char*)mi.lpBaseOfDll;
                for (size_t i = 0; i < mi.SizeOfImage - 2; i++) {
                    if (base[i] == '\xFF' && base[i + 1] == '\x23') {
                        trampoline = base + i;
                        break;
                    }
                }
            }
            if (!trampoline) return;

            using fnIssueOrder = int64_t(__cdecl*)(
                uintptr_t, int, Vec3*, uintptr_t, bool, bool);

            fnIssueOrder fn = reinterpret_cast<fnIssueOrder>(
                Globals::base + Offset::Function::IssueOrderCore);

            Vec3 localPos = pos;
            bool isAttack = ((int)type == 3);

            // Chimera pattern: run mainloop cleanup, then write IssueOrderFlag = order + 17.
            Bypass::PrepareIssueOrder((int)type);

            __try {
                spoof_call(trampoline, fn,
                    address, (int)type, &localPos, (uintptr_t)0, isAttack, false);
            } __except(1) {}
        }

        void IssueOrder(OrderType type, const GameObject& target) const {
            if (!IsValid() || !target.IsValid()) return;

            static void* trampoline = nullptr;
            if (!trampoline) {
                MODULEINFO mi{};
                GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(nullptr), &mi, sizeof(mi));
                char* base = (char*)mi.lpBaseOfDll;
                for (size_t i = 0; i < mi.SizeOfImage - 2; i++) {
                    if (base[i] == '\xFF' && base[i + 1] == '\x23') {
                        trampoline = base + i;
                        break;
                    }
                }
            }
            if (!trampoline) return;

            using fnIssueOrder = int64_t(__cdecl*)(
                uintptr_t, int, Vec3*, uintptr_t, bool, bool);

            fnIssueOrder fn = reinterpret_cast<fnIssueOrder>(
                Globals::base + Offset::Function::IssueOrderCore);

            Vec3 localPos = target.GetPosition();
            bool isAttack = ((int)type == 3);

            // Chimera pattern: run mainloop cleanup, then write IssueOrderFlag = order + 17.
            Bypass::PrepareIssueOrder((int)type);

            __try {
                spoof_call(trampoline, fn,
                    address, (int)type, &localPos, target.address, isAttack, false);
            } __except(1) {}
        }

        // Convenience: Attack target
        void AttackTarget(const GameObject& target) const {
            IssueOrder(OrderType::AttackUnit, target);
        }

        // Convenience: Move to position
        void MoveTo(const Vec3& pos) const {
            IssueOrder(OrderType::MoveTo, pos);
        }

        // ====================================================================
        // Damage Calculation Helpers (inline — no DamageCalc dependency)
        // ====================================================================

        float CalcPhysicalDamage(const GameObject& target) const {
            float damage = GetTotalAD();
            float armor = target.GetArmor();
            float armorPenPercent = GetArmorPenPercent();
            float armorPenFlat = GetArmorPenFlat();
            float lethality = GetLethality();

            int targetLevel = target.GetLevel();
            if (targetLevel <= 0) targetLevel = 1;
            float flatPen = armorPenFlat + lethality * (0.6f + 0.4f * (float)targetLevel / 18.0f);

            if (armor < 0) {
                return damage * (2.0f - 100.0f / (100.0f - armor));
            } else {
                float effectiveArmor = armor * (1.0f - armorPenPercent) - flatPen;
                if (effectiveArmor < 0) effectiveArmor = 0;
                return damage * (100.0f / (100.0f + effectiveArmor));
            }
        }

        float CalcMagicalDamage(const GameObject& target, float rawMagic) const {
            float mr = target.GetMR();
            float magicPenPercent = GetMagicPenPercent();
            float magicPenFlat = GetMagicPenFlat();

            if (mr < 0) {
                return rawMagic * (2.0f - 100.0f / (100.0f - mr));
            } else {
                float effectiveMR = mr * (1.0f - magicPenPercent) - magicPenFlat;
                if (effectiveMR < 0) effectiveMR = 0;
                return rawMagic * (100.0f / (100.0f + effectiveMR));
            }
        }

        // EnsoulSharp: GetAutoAttackDamage(target, includePassive)
        float GetAutoAttackDamage(const GameObject& target, bool includeCrit = false) const {
            float totalAD = GetTotalAD();
            float damage = totalAD;

            if (includeCrit) {
                float critChance = GetCrit();
                float critMulti = GetCritMultiplier();
                if (critMulti <= 0.0f) critMulti = 1.75f;
                damage = totalAD * (1.0f + critChance * (critMulti - 1.0f));
            }

            // Apply armor reduction
            float armor = target.GetArmor();
            float armorPenPercent = GetArmorPenPercent();
            float armorPenFlat = GetArmorPenFlat();
            float lethality = GetLethality();

            int targetLevel = target.GetLevel();
            if (targetLevel <= 0) targetLevel = 1;
            float flatPen = armorPenFlat + lethality * (0.6f + 0.4f * (float)targetLevel / 18.0f);

            if (armor < 0) {
                damage = damage * (2.0f - 100.0f / (100.0f - armor));
            } else {
                float effectiveArmor = armor * (1.0f - armorPenPercent) - flatPen;
                if (effectiveArmor < 0) effectiveArmor = 0;
                damage = damage * (100.0f / (100.0f + effectiveArmor));
            }

            return damage;
        }

        int GetAutoAttacksToKill(const GameObject& target) const {
            float dmg = GetAutoAttackDamage(target, false);
            if (dmg <= 0) return 999;
            return (int)std::ceil(target.GetHealth() / dmg);
        }

        float GetEffectiveHealthAD() const {
            return GetHealth() * (1.0f + GetArmor() / 100.0f);
        }

        float GetEffectiveHealthAP() const {
            return GetHealth() * (1.0f + GetMR() / 100.0f);
        }

        // ====================================================================
        // BasicAttack missile speed (for projectile prediction)
        // ====================================================================

        float GetBasicAttackMissileSpeed() const {
            if (IsMelee()) return FLT_MAX; // Melee = instant

            // Read from SpellData → MissileSpeed of slot[0] (basic attack)
            SpellBook sb(address);
            SpellSlot slot = sb.GetSpell((SpellSlotId)0);
            if (!slot.IsValid()) return 1500.0f; // default ranged

            SpellInfo info = slot.GetSpellInfo();
            if (!info.IsValid()) return 1500.0f;

            SpellData data = info.GetSpellData();
            if (!data.IsValid()) return 1500.0f;

            // SpellDataResource → MissileSpeed
            uintptr_t resource = Globals::Read<uintptr_t>(data.address + Offset::SpellBook::DataResourceBase);
            if (Globals::IsValidPtr(resource)) {
                float speed = Globals::Read<float>(resource + Offset::SpellBook::ResMissileSpeed);
                if (speed > 100.0f && speed < 10000.0f) return speed;
            }

            return 1500.0f; // default fallback
        }

        // ====================================================================
        // NavGrid helpers — wall & bush checks
        // ====================================================================

        // Is this object standing in a bush right now?
        bool IsInBush() const {
            auto ng = SDK::NavGrid::Get();
            if (!ng.IsValid()) return false;
            return ng.IsInBrush(GetPosition());
        }

        // Is this object standing on a wall cell?
        // (Useful for detecting units that clipped into terrain.)
        bool IsOnWall() const {
            auto ng = SDK::NavGrid::Get();
            if (!ng.IsValid()) return false;
            return ng.IsWall(GetPosition());
        }

        // Is a specific world position a wall?
        static bool IsWallAt(const Vec3& pos) {
            auto ng = SDK::NavGrid::Get();
            if (!ng.IsValid()) return false;
            return ng.IsWall(pos);
        }

        // Is a specific world position inside a bush?
        static bool IsInBushAt(const Vec3& pos) {
            auto ng = SDK::NavGrid::Get();
            if (!ng.IsValid()) return false;
            return ng.IsInBrush(pos);
        }

        // Does a clear line-of-sight exist between this object and another?
        // Uses NavGrid wall check (no bush blocking — only terrain blocks LOS).
        bool HasLineOfSightTo(const GameObject& target, float stepSize = 50.0f) const {
            auto ng = SDK::NavGrid::Get();
            if (!ng.IsValid()) return true; // assume LOS if grid unavailable
            return ng.HasLineOfSight(GetPosition(), target.GetPosition(), stepSize);
        }

        bool HasLineOfSightTo(const Vec3& pos, float stepSize = 50.0f) const {
            auto ng = SDK::NavGrid::Get();
            if (!ng.IsValid()) return true;
            return ng.HasLineOfSight(GetPosition(), pos, stepSize);
        }
    };

} // namespace SDK
