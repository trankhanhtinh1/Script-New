#pragma once

#include "../../Core/CoreBuffs.h"
#include "../../Core/CoreAiManager.h"
#include "../../Core/CoreItem.h"
#include "../../Core/CoreSpellBook.h"
#include "../../Core/CoreSpellDataInst.h"
#include "../../Core/CoreObjectManager.h"
#include "../../Core/CoreObjects.h"
#include "../../Core/CoreRuneManager.h"
#include "../../Core/Vector.h"
#include "../Enumerations/JungleType.h"
#include "../Enumerations/MinionTypes.h"
#include "../Enumerations/SpellSlot.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace SDK {

using Vector2 = Vec2;
using Vector3 = Vec3;

class AIBaseClient;
class GameObject;
class InventorySlot;

namespace CoreSpellBook = ::CoreSpellBook;

enum class GameObjectTeam : std::int32_t {
    Unknown = 0,
    Order = 100,
    Chaos = 200,
    Neutral = 300,
};

class SpellDataInstClient {
public:
    SpellDataInstClient() = default;
    SpellDataInstClient(uintptr_t owner, SpellSlot slot)
        : owner_(owner), slot_(slot) {}

    bool IsValid() const {
        return ::CoreSpellDataInst::IsValid(owner_, SlotIndex());
    }

    SpellSlot Slot() const {
        return slot_;
    }

    uintptr_t OwnerAddress() const {
        return owner_;
    }

    uintptr_t SpellbookAddress() const {
        return ::CoreSpellDataInst::Spellbook(owner_);
    }

    uintptr_t Address() const {
        return ::CoreSpellDataInst::Resolve(owner_, SlotIndex()).slot;
    }

    uintptr_t SpellInfoPointer() const {
        return ::CoreSpellDataInst::SpellInfo(Ref());
    }

    uintptr_t SDataPointer() const {
        return ::CoreSpellDataInst::SpellData(Ref());
    }

    uintptr_t DataResourcePointer() const {
        return ::CoreSpellDataInst::SpellDataResource(Ref());
    }

    std::string Name() const {
        return ::CoreSpellDataInst::Name(Ref());
    }

    CoreSpellBook::State State(float gameTime) const {
        return ::CoreSpellDataInst::SimpleState(Ref(), gameTime);
    }

    std::uint32_t RawState() const {
        return ::CoreSpellDataInst::RawState(Ref());
    }

    float RemainingCooldown(float gameTime = 0.0f) const {
        return ::CoreSpellDataInst::RemainingCooldown(Ref(), gameTime);
    }

    float Cooldown() const {
        return ::CoreSpellDataInst::Cooldown(Ref());
    }

    float CooldownExpires() const {
        return ::CoreSpellDataInst::CooldownExpires(Ref());
    }

    float ManaCost() const {
        return ::CoreSpellDataInst::ManaCost(Ref());
    }

    int Ammo() const {
        return ::CoreSpellDataInst::Ammo(Ref());
    }

    int MaxAmmo() const {
        return ::CoreSpellDataInst::MaxAmmo(Ref());
    }

    float AmmoRechargeTime() const {
        return ::CoreSpellDataInst::AmmoRechargeTime(Ref());
    }

    float NextAmmoRechargeTime() const {
        return ::CoreSpellDataInst::NextAmmoRechargeTime(Ref());
    }

    bool Learned() const {
        return ::CoreSpellDataInst::Learned(Ref());
    }

    int Level() const {
        return ::CoreSpellDataInst::Level(Ref());
    }

private:
    std::int32_t SlotIndex() const {
        return static_cast<std::int32_t>(slot_);
    }

    ::CoreSpellDataInst::SpellSlotRef Ref() const {
        return ::CoreSpellDataInst::Resolve(owner_, SlotIndex());
    }

    uintptr_t owner_ = 0;
    SpellSlot slot_ = SpellSlot::Unknown;
};

class SpellBookClient {
public:
    SpellBookClient() = default;
    explicit SpellBookClient(uintptr_t owner) : owner_(owner) {}

    bool IsValid() const {
        return Globals::IsValidPtr(owner_) &&
               Globals::IsValidPtr(::CoreSpellBook::Spellbook(owner_));
    }

    uintptr_t Address() const {
        return ::CoreSpellBook::Spellbook(owner_);
    }

    uintptr_t OwnerAddress() const {
        return owner_;
    }

    AIBaseClient Owner() const;

    std::uint32_t CasterNetworkId() const {
        return ::CoreSpellBook::CasterNetworkId(Address());
    }

    SpellDataInstClient GetSpell(SpellSlot slot) const {
        return SpellDataInstClient(owner_, slot);
    }

    std::vector<SpellDataInstClient> Spells() const {
        std::vector<SpellDataInstClient> result;
        ::CoreSpellDataInst::SpellSlotRef refs[::CoreSpellDataInst::kMaxSpellSlots] = {};
        const int count = ::CoreSpellBook::GetSpells(
            owner_,
            refs,
            ::CoreSpellDataInst::kMaxSpellSlots);
        result.reserve(count > 0 ? static_cast<std::size_t>(count) : 0);
        for (int i = 0; i < count; ++i) {
            result.emplace_back(owner_, static_cast<SpellSlot>(refs[i].slotId));
        }
        return result;
    }

    CoreSpellBook::State CanUseSpell(SpellSlot slot) const {
        return ::CoreSpellBook::CanUseSpell(
            owner_,
            static_cast<std::int32_t>(slot),
            CoreRuntime::GetContext().gameTime);
    }

    uintptr_t ActiveSpell() const {
        return ::CoreSpellBook::ActiveSpell(owner_).address;
    }

    bool IsCastingSpell() const {
        return ::CoreSpellBook::IsCastingSpell(owner_, CoreRuntime::GetContext().gameTime);
    }

    bool IsChanneling() const {
        return ::CoreSpellBook::IsChanneling(owner_, CoreRuntime::GetContext().gameTime);
    }

    bool IsCharging() const {
        return ::CoreSpellBook::IsCharging(owner_);
    }

    bool IsStopped() const {
        return !IsCastingSpell();
    }

    bool IsAutoAttack() const {
        return ::CoreSpellBook::CastSlot(::CoreSpellBook::ActiveSpell(owner_)) == 64;
    }

    bool IsWindingUp() const {
        return IsCastingSpell() && !IsChanneling();
    }

    bool SpellWasCast() const {
        const auto active = ::CoreSpellBook::ActiveSpell(owner_);
        const float endTime = ::CoreSpellBook::CastEndTime(active);
        const float gameTime = CoreRuntime::GetContext().gameTime;
        return active.IsValid() && endTime > 0.0f && gameTime >= endTime;
    }

    float CastEndTime() const {
        return ::CoreSpellBook::CastEndTime(::CoreSpellBook::ActiveSpell(owner_));
    }

    float CastTime() const {
        const auto active = ::CoreSpellBook::ActiveSpell(owner_);
        const float start = ::CoreSpellBook::CastStartTime(active);
        const float end = ::CoreSpellBook::CastEndTime(active);
        return end > start ? (end - start) : 0.0f;
    }

    bool CastSpell(SpellSlot slot, bool triggerEvent = true) const {
        (void)triggerEvent;
        return ::CoreSpellBook::CastSpell(owner_, static_cast<std::int32_t>(slot));
    }

    bool CastSpell(SpellSlot slot,
                   const Vector3& position,
                   bool triggerEvent = true) const {
        (void)triggerEvent;
        return ::CoreSpellBook::CastSpell(
            owner_,
            static_cast<std::int32_t>(slot),
            position);
    }

    bool CastSpell(SpellSlot slot,
                   const Vector3& startPosition,
                   const Vector3& endPosition,
                   bool triggerEvent = true) const {
        (void)triggerEvent;
        return ::CoreSpellBook::CastSpell(
            owner_,
            static_cast<std::int32_t>(slot),
            startPosition,
            endPosition);
    }

    bool CastSpell(SpellSlot slot,
                   uintptr_t target,
                   bool triggerEvent = true) const {
        (void)triggerEvent;
        return ::CoreSpellBook::CastSpellOnTarget(
            owner_,
            static_cast<std::int32_t>(slot),
            target);
    }

    bool CastSpell(SpellSlot slot,
                   const GameObject& target,
                   bool triggerEvent = true) const;

    bool UpdateChargedSpell(SpellSlot slot,
                            const Vector3& position,
                            bool releaseCast,
                            bool triggerEvent = true) const {
        (void)triggerEvent;
        return ::CoreSpellBook::UpdateChargedSpell(
            owner_,
            static_cast<std::int32_t>(slot),
            position,
            releaseCast);
    }

private:
    uintptr_t owner_ = 0;
};

namespace ObjectDetail {
    inline bool EqualsAny(const std::string& text, std::initializer_list<const char*> values) {
        for (const char* value : values) {
            if (value && text == value) {
                return true;
            }
        }
        return false;
    }

    inline bool ContainsAny(const std::string& text, std::initializer_list<const char*> values) {
        for (const char* value : values) {
            if (value && text.find(value) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    inline bool ContainsInsensitive(const std::string& text, const char* token) {
        if (!token || !token[0] || text.empty()) {
            return false;
        }

        std::string lhs = text;
        std::string rhs = token;
        std::transform(lhs.begin(), lhs.end(), lhs.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        std::transform(rhs.begin(), rhs.end(), rhs.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return lhs.find(rhs) != std::string::npos;
    }

    inline GameObjectTeam MapTeam(std::uint32_t team) {
        switch (team) {
        case 100: return GameObjectTeam::Order;
        case 200: return GameObjectTeam::Chaos;
        case 300: return GameObjectTeam::Neutral;
        default: return GameObjectTeam::Unknown;
        }
    }

    inline std::string ToLower(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return text;
    }
} // namespace ObjectDetail

class GameObject {
public:
    GameObject() = default;
    explicit GameObject(uintptr_t address,
                        ::Core::Objects::ObjectType type = ::Core::Objects::ObjectType::GameObject)
        : handle_(::Core::ObjectManager::MakeHandle(address, type)) {}
    explicit GameObject(::Core::Objects::ObjectHandle handle)
        : handle_(handle) {}

    uintptr_t Address() const {
        if (handle_.HasAddress()) {
            ::Core::Objects::ObjectHandle resolved = handle_;
            if (::Core::ObjectManager::Resolve(resolved)) {
                handle_ = resolved;
                return handle_.address;
            }
        }

        if (::Core::ObjectManager::Resolve(handle_)) {
            return handle_.address;
        }
        return 0;
    }

    bool IsValid() const {
        return Address() != 0;
    }

    int NetworkId() const {
        (void)Address();
        return static_cast<int>(handle_.networkId);
    }

    int Index() const {
        (void)Address();
        return static_cast<int>(handle_.index);
    }

    ::Core::Objects::ObjectType Type() const {
        if (handle_.type == ::Core::Objects::ObjectType::Unknown && IsValid()) {
            handle_.type = ::Core::ObjectManager::InferType(handle_.address);
        }
        return handle_.type;
    }

    ::Core::Objects::ObjectHandle Handle() const {
        (void)Address();
        return handle_;
    }

    ::Core::Objects::ObjectSnapshot Snapshot() const {
        return ::Core::ObjectManager::ReadObject(Address(), Type());
    }

    GameObjectTeam Team() const {
        return ObjectDetail::MapTeam(::Core::Objects::ReadTeamValue(Address()));
    }

    bool IsEnemy() const {
        const uintptr_t addr = Address();
        const uintptr_t paddr = ::Core::ObjectManager::PlayerAddress();
        if (!Globals::IsValidPtr(addr) || !Globals::IsValidPtr(paddr)) return false;
        const std::uint32_t st = ::Core::Objects::ReadTeamValue(addr);
        const std::uint32_t pt = ::Core::Objects::ReadTeamValue(paddr);
        return st != 0 && pt != 0 && st != pt;
    }

    bool IsAlly() const {
        const uintptr_t addr = Address();
        const uintptr_t paddr = ::Core::ObjectManager::PlayerAddress();
        if (!Globals::IsValidPtr(addr) || !Globals::IsValidPtr(paddr)) return false;
        const std::uint32_t st = ::Core::Objects::ReadTeamValue(addr);
        const std::uint32_t pt = ::Core::Objects::ReadTeamValue(paddr);
        return st != 0 && st == pt;
    }

    bool IsMe() const {
        const auto player = ::Core::ObjectManager::PlayerHandle();
        return player.HasIdentity() && NetworkId() == static_cast<int>(player.networkId);
    }

    bool IsDead() const {
        return ::Core::Objects::IsDead(Address());
    }

    bool IsVisible() const {
        return ::Core::Objects::ReadBoolByte(Address(), Offset::All::Visible);
    }

    bool IsTargetable() const {
        return ::Core::Objects::ReadBoolByte(Address(), Offset::AttackableUnit::IsTargetable);
    }

    bool IsInvulnerable() const {
        return ::Core::Objects::ReadBoolByte(Address(), Offset::All::IsInvulnerable);
    }

    float BoundingRadius() const {
        return ::Core::Objects::ReadBoundingRadius(Address());
    }

    Vector3 Position() const {
        return ::Core::Objects::ReadPosition(Address());
    }

    Vector3 Direction() const {
        return ::Core::Objects::ReadDirection(Address());
    }

    std::string Name() const {
        char buf[96] = {};
        ::Core::Objects::ReadName(Address(), buf, static_cast<int>(sizeof(buf)));
        return std::string(buf);
    }

    std::string CharacterName() const {
        char buf[96] = {};
        ::Core::Objects::ReadCharacterName(Address(), buf, static_cast<int>(sizeof(buf)));
        return std::string(buf);
    }

    // Gap #2 fix: EnsoulSharp Compare is strict — only matches when both
    // objects are valid AND have the same NetworkId. No fallback to index
    // or address comparison (those can alias after NetworkId was split from
    // Index on build 13337).
    bool Compare(const GameObject& other) const {
        if (!IsValid() || !other.IsValid()) {
            return false;
        }
        const auto lhs = Handle();
        const auto rhs = other.Handle();
        return lhs.networkId != 0 && lhs.networkId != 0xFFFFFFFFu &&
               lhs.networkId == rhs.networkId;
    }

    float Distance(const GameObject& other) const {
        return Position().Distance(other.Position());
    }

    float Distance(const Vector3& position) const {
        return Position().Distance(position);
    }

    float Distance(const Vector2& position) const {
        return Position().To2D().Distance(position);
    }

    bool IsHero() const {
        return Type() == ::Core::Objects::ObjectType::AIHeroClient;
    }

    bool IsMinion() const {
        return Type() == ::Core::Objects::ObjectType::AIMinionClient;
    }

    bool IsTurret() const {
        return Type() == ::Core::Objects::ObjectType::AITurretClient;
    }

    bool IsMissile() const {
        return Type() == ::Core::Objects::ObjectType::MissileClient;
    }

    // Sion remains a targetable AIHeroClient during Glory in Death even
    // though the engine reports the unit as dead. Karthus' Death Defied is
    // intentionally not considered a zombie because that state cannot be
    // attacked as a normal target.
    bool IsZombie() const {
        if (!IsHero() ||
            ObjectDetail::ToLower(CharacterName()) != "sion") {
            return false;
        }
        return CoreBuffs::HasBuff(Address(), "sionpassivezombie");
    }

    // Champion clones are AIMinionClient objects whose CharacterName matches
    // the champion they imitate. Requiring the minion runtime type is what
    // distinguishes the clone from the real AIHeroClient.
    bool IsClone() const {
        if (!IsMinion()) {
            return false;
        }

        const std::string name = ObjectDetail::ToLower(CharacterName());
        return ObjectDetail::EqualsAny(name, {
            "leblanc", "monkeyking", "neeko", "shaco",
        });
    }

    // Pets are summons, not champion clones. Some game builds tag summons
    // through MinionClass::Pet; retain the curated names as a fallback for
    // objects whose replicated class byte has not arrived at creation time.
    bool IsPet() const {
        if (!IsMinion() || IsClone()) {
            return false;
        }

        if (::Core::Objects::ReadMinionClass(Address()) ==
            ::Core::Objects::MinionClass::Pet) {
            return true;
        }

        const std::string name = ObjectDetail::ToLower(CharacterName());
        const std::string nameFallback = name.empty() ? ObjectDetail::ToLower(Name()) : name;
        return ObjectDetail::EqualsAny(nameFallback, {
            "annietibbers", "elisespiderling", "heimertyellow",
            "heimertblue", "ivernminion", "malzaharvoidling",
            "shacobox", "yorickghoulmelee", "yorickbigghoul",
            "zyrathornplant", "zyragraspingplant",
        });
    }

protected:
    mutable ::Core::Objects::ObjectHandle handle_ = {};
};

inline bool SpellBookClient::CastSpell(SpellSlot slot,
                                       const GameObject& target,
                                       bool triggerEvent) const {
    return CastSpell(slot, target.Address(), triggerEvent);
}

class AttackableUnit : public GameObject {
public:
    AttackableUnit() = default;
    explicit AttackableUnit(uintptr_t address,
                            ::Core::Objects::ObjectType type = ::Core::Objects::ObjectType::GameObject)
        : GameObject(address, type) {}
    explicit AttackableUnit(::Core::Objects::ObjectHandle handle)
        : GameObject(handle) {}

    float Health() const { return Globals::Read<float>(Address() + Offset::AttackableUnit::HP); }
    float MaxHealth() const { return Globals::Read<float>(Address() + Offset::AttackableUnit::MaxHP); }
    float HealthPercent() const {
        const float maxHealth = MaxHealth();
        return maxHealth > 0.0f ? (Health() * 100.0f / maxHealth) : 0.0f;
    }
    float AllShield() const { return Globals::Read<float>(Address() + Offset::AttackableUnit::AllShield); }
    float PhysicalShield() const { return Globals::Read<float>(Address() + Offset::AttackableUnit::PhysicalShield); }
    float MagicalShield() const { return Globals::Read<float>(Address() + Offset::AttackableUnit::MagicalShield); }
};

class AIBaseClient : public AttackableUnit {
public:
    AIBaseClient() = default;
    explicit AIBaseClient(uintptr_t address,
                          ::Core::Objects::ObjectType type = ::Core::Objects::ObjectType::GameObject)
        : AttackableUnit(address, type) {}
    explicit AIBaseClient(::Core::Objects::ObjectHandle handle)
        : AttackableUnit(handle) {}

    float Mana() const { return Globals::Read<float>(Address() + Offset::AIHeroClient::MP); }
    float MaxMana() const { return Globals::Read<float>(Address() + Offset::AIHeroClient::MaxMP); }
    float MoveSpeed() const { return Globals::Read<float>(Address() + Offset::AIHeroClient::MoveSpeed); }
    float AttackRange() const { return Globals::Read<float>(Address() + Offset::AIHeroClient::AttackRange); }
    float TotalAttackDamage() const { return Globals::Read<float>(Address() + Offset::AIHeroClient::BaseAttackDamage) + Globals::Read<float>(Address() + Offset::AIHeroClient::FlatPhysicalDmgMod); }
    float BaseAttackDamage() const { return Globals::Read<float>(Address() + Offset::AIHeroClient::BaseAttackDamage); }
    float TotalMagicalDamage() const { return Globals::Read<float>(Address() + Offset::AIHeroClient::BaseAbilityDamage); }
    float Armor() const { return Globals::Read<float>(Address() + Offset::AIHeroClient::Armor); }
    float SpellBlock() const { return Globals::Read<float>(Address() + Offset::AIHeroClient::SpellBlock); }
    float AttackSpeedMod() const { return Globals::Read<float>(Address() + Offset::AIHeroClient::AttackSpeedMod); }
    int Level() const { return Globals::Read<int>(Address() + Offset::AIHeroClient::LevelRef); }

    uintptr_t AiManagerAddress() const {
        return ::CoreAiManager::Address(Address());
    }

    ::CoreAiManager::ManagerRef GetAiManager() const {
        return ::CoreAiManager::Get(Address());
    }

    ::CoreAiManager::Snapshot AiManagerSnapshot() const {
        return ::CoreAiManager::ReadSnapshot(Address());
    }

    Vector3 ServerPosition() const {
        return ::CoreAiManager::GetServerPosition(Address());
    }

    Vector3 PreviousPosition() const {
        return ::CoreAiManager::GetPreviousPosition(Address());
    }

    Vector3 Direction() const {
        return ::CoreAiManager::GetDirection(Address());
    }

    Vector3 Velocity() const {
        return ::CoreAiManager::GetVelocity(Address());
    }

    Vector3 PathStart() const {
        return ::CoreAiManager::GetPathStart(Address());
    }

    Vector3 PathEnd() const {
        return ::CoreAiManager::GetPathEnd(Address());
    }

    Vector3 OrderPosition() const {
        return ::CoreAiManager::GetOrderPosition(Address());
    }

    bool HasPath() const {
        return ::CoreAiManager::HasPath(Address());
    }

    bool IsMoving() const {
        return ::CoreAiManager::IsMoving(Address());
    }

    bool IsDashing() const {
        return ::CoreAiManager::IsDashing(Address());
    }

    bool HasArrived() const {
        return ::CoreAiManager::HasArrived(Address());
    }

    int CurrentPathSegment() const {
        return ::CoreAiManager::GetCurrentSegment(Address());
    }

    int WaypointCount() const {
        return ::CoreAiManager::GetWaypointCount(Address());
    }

    float DashSpeed() const {
        return ::CoreAiManager::GetDashSpeed(Address());
    }

    float DashDistanceRemaining() const {
        return ::CoreAiManager::GetDashDistRemaining(Address());
    }

    int CopyWaypoints(Vector3* out, int maxOut) const {
        return ::CoreAiManager::CopyWaypoints(
            Address(), reinterpret_cast<Vec3*>(out), maxOut);
    }

    std::vector<Vector3> GetWaypoints(int maxPoints = 32) const {
        std::vector<Vector3> path;
        if (!IsValid() || maxPoints <= 0) {
            return path;
        }

        maxPoints = std::clamp(maxPoints, 1, ::CoreAiManager::kMaxWaypoints);
        Vec3 points[::CoreAiManager::kMaxWaypoints] = {};
        const int count = ::CoreAiManager::CopyPath(
            Address(), points, maxPoints);
        path.reserve(static_cast<std::size_t>(std::max(0, count)));
        for (int i = 0; i < count; ++i) {
            path.push_back(points[i]);
        }
        return path;
    }

    std::vector<Vector3> GetPath(int maxPoints = 32) const {
        return GetWaypoints(maxPoints);
    }

    std::vector<Vector3> Path() const {
        return GetWaypoints();
    }

    int GetPathLength() const {
        return static_cast<int>(GetWaypoints().size());
    }

    bool IsMelee() const {
        return AttackRange() < 400.0f;
    }

    bool HasBuff(const char* name) const {
        return CoreBuffs::HasBuff(Address(), name);
    }

    bool HasItem(int id) const {
        return ::CoreItem::HasItemId(Address(), id);
    }

    bool HasItemInSlot(int slotIndex) const {
        return ::CoreItem::HasItem(Address(), slotIndex);
    }

    int GetItemId(int slotIndex) const {
        return ::CoreItem::GetItemId(Address(), slotIndex);
    }

    uintptr_t GetItemInfo(int slotIndex) const {
        return ::CoreItem::GetItemInfo(Address(), slotIndex);
    }

    int GetItemCount() const {
        return ::CoreItem::GetItemCount(Address());
    }

    bool HasTrinket() const {
        return ::CoreItem::HasTrinket(Address());
    }

    std::vector<InventorySlot> InventoryItems() const;

    bool IsRecalling() const {
        return HasBuff("recall");
    }

    SpellBookClient Spellbook() const {
        return SpellBookClient(Address());
    }

    SpellDataInstClient GetSpell(SpellSlot slot) const {
        return Spellbook().GetSpell(slot);
    }

    SpellSlot GetSpellSlot(const char* name) const {
        if (!name || !name[0]) {
            return SpellSlot::Unknown;
        }

        const SpellBookClient book = Spellbook();
        const std::string targetName = ObjectDetail::ToLower(name);
        for (int slot = 0; slot < ::CoreSpellDataInst::kMaxSpellSlots; ++slot) {
            SpellDataInstClient spell = book.GetSpell(static_cast<SpellSlot>(slot));
            if (!spell.IsValid()) {
                continue;
            }

            if (ObjectDetail::ToLower(spell.Name()) == targetName) {
                return static_cast<SpellSlot>(slot);
            }
        }
        return SpellSlot::Unknown;
    }

    float DistanceToPlayer() const {
        const auto player = ::Core::ObjectManager::Player();
        return player.IsValid() ? Position().Distance(player.position) : FLT_MAX;
    }

    int CountAllyHeroesInRange(float range) const;
    int CountEnemyHeroesInRange(float range) const;
    bool IsUnderAllyTurret() const;
    bool IsUnderEnemyTurret() const;
};

inline AIBaseClient SpellBookClient::Owner() const {
    return AIBaseClient(owner_, ::Core::ObjectManager::InferType(owner_));
}

class RuneManagerClient {
public:
    RuneManagerClient() = default;
    explicit RuneManagerClient(uintptr_t address)
        : address_(address) {}

    uintptr_t Address() const { return address_; }
    bool IsValid() const { return Globals::IsValidPtr(address_); }

    ::CoreRuneManager::RuneTreeData PrimaryTree() const {
        return ::CoreRuneManager::ReadPrimaryTree(address_);
    }

    ::CoreRuneManager::RuneTreeData SecondaryTree() const {
        return ::CoreRuneManager::ReadSecondaryTree(address_);
    }

    ::CoreRuneManager::ManagerSnapshot Snapshot() const {
        return ::CoreRuneManager::ReadFromManager(address_);
    }

    std::vector<::CoreRuneManager::RuneEntry> Entries() const {
        ::CoreRuneManager::RuneEntry entries[::CoreRuneManager::kMaxRuneEntries] = {};
        const int count = ::CoreRuneManager::ReadEntries(
            address_,
            entries,
            ::CoreRuneManager::kMaxRuneEntries);
        std::vector<::CoreRuneManager::RuneEntry> result;
        result.reserve(count > 0 ? static_cast<std::size_t>(count) : 0);
        for (int index = 0; index < count; ++index) {
            result.push_back(entries[index]);
        }
        return result;
    }

private:
    uintptr_t address_ = 0;
};

class AIHeroClient : public AIBaseClient {
public:
    AIHeroClient() = default;
    explicit AIHeroClient(uintptr_t address)
        : AIBaseClient(address, ::Core::Objects::ObjectType::AIHeroClient) {}
    explicit AIHeroClient(::Core::Objects::ObjectHandle handle)
        : AIBaseClient(handle) {
        handle_.type = ::Core::Objects::ObjectType::AIHeroClient;
    }

    SpellBookClient Spellbook() const {
        return SpellBookClient(Address());
    }

    SpellSlot GetSpellSlot(const char* name) const {
        return AIBaseClient::GetSpellSlot(name);
    }

    RuneManagerClient RuneManager() const {
        return RuneManagerClient(::CoreRuneManager::Resolve(Address()));
    }
};

class AIMinionClient : public AIBaseClient {
public:
    AIMinionClient() = default;
    explicit AIMinionClient(uintptr_t address)
        : AIBaseClient(address, ::Core::Objects::ObjectType::AIMinionClient) {}
    explicit AIMinionClient(::Core::Objects::ObjectHandle handle)
        : AIBaseClient(handle) {
        handle_.type = ::Core::Objects::ObjectType::AIMinionClient;
    }

    ::Core::Objects::MinionClass GetMinionClass() const {
        return ::Core::Objects::ReadMinionClass(Address());
    }

    MinionTypes GetMinionType() const {
        const std::string name = CharacterName();
        const std::string nameFallback = name.empty() ? Name() : name;
        const ::Core::Objects::MinionClass mc = GetMinionClass();

        if (ObjectDetail::EqualsAny(nameFallback, {
            "SRU_ChaosMinionMelee", "SRU_OrderMinionMelee",
            "HA_ChaosMinionMelee", "HA_OrderMinionMelee",
        })) {
            return MinionTypes::Normal | MinionTypes::Melee;
        }

        if (ObjectDetail::EqualsAny(nameFallback, {
            "SRU_ChaosMinionRanged", "SRU_OrderMinionRanged",
            "HA_ChaosMinionRanged", "HA_OrderMinionRanged",
        })) {
            return MinionTypes::Normal | MinionTypes::Ranged;
        }

        if (ObjectDetail::EqualsAny(nameFallback, {
            "SRU_ChaosMinionSiege", "SRU_OrderMinionSiege",
            "HA_ChaosMinionSiege", "HA_OrderMinionSiege",
        })) {
            return MinionTypes::Siege | MinionTypes::Ranged;
        }

        if (ObjectDetail::EqualsAny(nameFallback, {
            "SRU_ChaosMinionSuper", "SRU_OrderMinionSuper",
            "HA_ChaosMinionSuper", "HA_OrderMinionSuper",
        })) {
            return MinionTypes::Super | MinionTypes::Melee;
        }

        if (ObjectDetail::EqualsAny(nameFallback, {
            "SightWard", "VisionWard", "YellowTrinket", "BlueTrinket",
            "JammerDevice", "JammerDeviceItem",
            "SionUlt_Ward", "SionPassiveCorpse",
        })) {
            return MinionTypes::Ward;
        }

        switch (mc) {
        case ::Core::Objects::MinionClass::MeleeLaneMinion:
            return MinionTypes::Normal | MinionTypes::Melee;
        case ::Core::Objects::MinionClass::RangedLaneMinion:
            return MinionTypes::Normal | MinionTypes::Ranged;
        case ::Core::Objects::MinionClass::SiegeLaneMinion:
            return MinionTypes::Siege | MinionTypes::Ranged;
        case ::Core::Objects::MinionClass::SuperLaneMinion:
            return MinionTypes::Super | MinionTypes::Melee;
        case ::Core::Objects::MinionClass::TeamMinion:
            return MinionTypes::Normal;
        default:
            return MinionTypes::Unknown;
        }
    }

    // Update May 2026 (CDragon map11.bin.json + Riot character folders).
    // Plant scriptnames live under /Characters/SRU_Plant_* and use the
    // "SRU_Plant_*" character name family. Voidgrubs added in 13.20 ship
    // as "SRU_Voidgrub_*" and are an early epic objective. Sentinel
    // (SRU_Sentinel) is the new Atakhan precursor (14.20+) but the Atakhan
    // boss itself is "Atakhan_*". KrugAncient is the Krug elder. RiftHrald
    // and Baron retain their classic names.
    JungleType GetJungleType() const {
        const std::string name = CharacterName();
        const std::string nameFallback = name.empty() ? Name() : name;

        // Plants: blast cone, honeyfruit, scryer's bloom (visible-only +
        // Smolder's Twin Shadows passive variant). Map under the same enum
        // as jungle so target selectors / orbwalker can filter them out.
        if (ObjectDetail::ContainsAny(nameFallback, {
            "SRU_Plant_Satchel",   // Blast Cone (đèn nổ)
            "SRU_Plant_Health",    // Honeyfruit (trái cây hồi máu)
            "SRU_Plant_Vision",    // Scryer's Bloom (đèn soi sáng / hạt thông soi)
            "Plant_Satchel", "Plant_Health", "Plant_Vision",
        })) {
            return JungleType::Plant;
        }

        // Small camp minions (mini wolves/krugs/raptors + TT mini camps).
        if (ObjectDetail::ContainsAny(nameFallback, {
            "SRU_RazorbeakMini", "SRU_MurkwolfMini", "SRU_KrugMini",
            "SRU_KrugMiniMini",   // post 13.18 mini krug split
            "SRU_GrompMini",      // future-proof if Riot adds split
            "TestCubeRender",
            "TT_NGolem2", "TT_NWraith2", "TT_NWolf2",
        })) {
            return JungleType::Small;
        }

        // Large camp monsters + Krug Ancient (the elder krug).
        if (ObjectDetail::ContainsAny(nameFallback, {
            "SRU_Razorbeak", "SRU_Red", "SRU_Krug", "SRU_KrugAncient",
            "SRU_Murkwolf", "SRU_Blue", "SRU_Gromp",
            "Sru_Crab", "SRU_Crab",
            "TT_NGolem", "TT_NWraith", "TT_NWolf",
        })) {
            return JungleType::Large;
        }

        // Voidgrubs are an early-game epic objective that spawns 3 at a
        // time in the Baron pit; treat them as Epic so jungle clear logic
        // can prioritise them differently from large camps.
        if (ObjectDetail::ContainsAny(nameFallback, {
            "SRU_Voidgrub", "SRU_Voidgrubs",
        })) {
            return JungleType::Epic;
        }

        // Legendary epic monsters: dragons, Baron, Rift Herald, Atakhan,
        // Sentinel (Atakhan precursor), TT Vilemaw.
        if (ObjectDetail::ContainsAny(nameFallback, {
            "SRU_Dragon_Air", "SRU_Dragon_Earth", "SRU_Dragon_Fire",
            "SRU_Dragon_Water", "SRU_Dragon_Elder", "SRU_Dragon_Hextech",
            "SRU_Dragon_Chemtech",
            "SRU_RiftHerald", "SRU_Baron",
            "SRU_Sentinel", "Atakhan_",   // Atakhan boss family
            "TT_Spiderboss",
        })) {
            return JungleType::Legendary;
        }

        return JungleType::Unknown;
    }

    bool IsMinion() const {
        const MinionTypes type = GetMinionType();
        return HasFlag(type, MinionTypes::Melee) || HasFlag(type, MinionTypes::Ranged);
    }

    // True for jungle plants only (Blast Cone / Honeyfruit / Scryer's Bloom).
    // Use this to skip them from auto-attack target lists — they are
    // technically AIMinionClient with `Team == Neutral` but are NOT real
    // monsters; auto-attacking them wastes a basic attack and gives no
    // gold/xp. Mirrors EnsoulSharp's IsPlant convention.
    bool IsPlant() const {
        return GetJungleType() == JungleType::Plant;
    }

    bool IsJungleBuff() const {
        const std::string name = CharacterName();
        return name == "SRU_Blue" || name == "SRU_Red";
    }
};


// Update May 2026: turret detection helpers. CDragon Map11 ships several
// stationary tower scriptnames that all live under AITurretClient but
// have very different combat behaviour. Without classification, an
// orbwalker may try to attack a fountain turret (untargetable, infinite
// damage) or an azir-soldier-tower-like dummy (not a real structure).
class AITurretClient : public AIBaseClient {
public:
    AITurretClient() = default;
    explicit AITurretClient(uintptr_t address)
        : AIBaseClient(address, ::Core::Objects::ObjectType::AITurretClient) {}
    explicit AITurretClient(::Core::Objects::ObjectHandle handle)
        : AIBaseClient(handle) {
        handle_.type = ::Core::Objects::ObjectType::AITurretClient;
    }

    // Lane turret families (outer / inner / inhib / nexus) on Summoner's
    // Rift, Howling Abyss, Nexus Blitz. Riot scriptnames keep "Turret"
    // anywhere in CharacterName; the precise tier comes from the Name
    // ("Turret_T1_R_03_A" = bot outer, etc.) but for orbwalker target
    // selection only the Lane/Fountain/Other split matters.
    bool IsLaneTurret() const {
        const std::string name = ObjectDetail::ToLower(CharacterName());
        if (name.find("turret") == std::string::npos) {
            return false;
        }
        // Fountain/shrine turrets appear with the SAME CharacterName but
        // their unit Name starts with "Turret_OrderTurretShrine" /
        // "Turret_ChaosTurretShrine"; carve them out below.
        const std::string unitName = ObjectDetail::ToLower(Name());
        if (unitName.find("shrine") != std::string::npos ||
            unitName.find("nexustower") != std::string::npos) {
            return false;
        }
        return true;
    }

    // Fountain / nexus shrine turrets. Untargetable when a friendly hero
    // is in fountain — orbwalker MUST never target these even when the
    // game reports them as enemy & targetable, because attacking them
    // immediately puts the player in the fountain laser's range.
    bool IsFountainTurret() const {
        const std::string unitName = ObjectDetail::ToLower(Name());
        return unitName.find("shrine") != std::string::npos ||
               unitName.find("nexustower") != std::string::npos;
    }

    // Shuriman / Azir-style summoned tower (sand soldier or special map
    // event). These are NOT real lane structures — they spawn from
    // gameplay scripts and orbwalker should treat them as champion-pet.
    bool IsShurimaTurret() const {
        const std::string name = ObjectDetail::ToLower(CharacterName());
        return name.find("shuriman") != std::string::npos ||
               name.find("azirsoldier") != std::string::npos;
    }
};


class MissileClient : public GameObject {
public:
    MissileClient() = default;
    explicit MissileClient(uintptr_t address)
        : GameObject(address, ::Core::Objects::ObjectType::MissileClient) {}
    explicit MissileClient(::Core::Objects::ObjectHandle handle)
        : GameObject(handle) {
        handle_.type = ::Core::Objects::ObjectType::MissileClient;
    }

    int CasterIndex() const { return static_cast<int>(::Core::Objects::ReadField<std::uint32_t>(Address(), Offset::MissileClient::CasterIndex)); }
    int TargetIndex() const { return static_cast<int>(::Core::Objects::ReadField<std::uint32_t>(Address(), Offset::MissileClient::TargetIndex)); }
    int CasterNetworkId() const { return ResolveNetworkIdFromIndex(::Core::Objects::ReadField<std::uint32_t>(Address(), Offset::MissileClient::CasterIndex)); }
    int TargetNetworkId() const { return ResolveNetworkIdFromIndex(::Core::Objects::ReadField<std::uint32_t>(Address(), Offset::MissileClient::TargetIndex)); }
    std::string SpellName() const {
        char buf[128] = {};
        Globals::ReadRuntimeStringField(Address() + Offset::MissileClient::SpellName, buf, static_cast<int>(sizeof(buf)));
        return std::string(buf);
    }
    std::string MissileName() const {
        char buf[128] = {};
        Globals::ReadRuntimeStringField(Address() + Offset::MissileClient::MissileName, buf, static_cast<int>(sizeof(buf)));
        return std::string(buf);
    }
    Vector3 StartPosition() const { return ::Core::Objects::ReadField<Vec3>(Address(), Offset::MissileClient::StartPos); }
    Vector3 EndPosition() const { return ::Core::Objects::ReadField<Vec3>(Address(), Offset::MissileClient::EndPos); }

private:
    static int ResolveNetworkIdFromIndex(std::uint32_t index) {
        if (index == 0 || index == 0xFFFFFFFFu) {
            return 0;
        }

        const uintptr_t object = ::Core::ObjectManager::FindByIndex(index);
        if (!Globals::IsValidPtr(object)) {
            return 0;
        }

        const std::uint32_t networkId = ::Core::Objects::ReadNetworkId(object);
        return networkId != 0 && networkId != 0xFFFFFFFFu
            ? static_cast<int>(networkId)
            : 0;
    }
};

// Inhibitor / nhà lính. CDragon Map11 spawns 6 inhibitors total
// (Order top/mid/bot + Chaos top/mid/bot) under unit names
// "Barracks_T1_C{1..3}" (Order) and "Barracks_T2_L{1..3}" (Chaos).
// Lane is encoded in the unit Name suffix, NOT in CharacterName, so use
// Name() to decide whether this is the top, mid, or bottom inhibitor.
class BarracksDampenerClient : public AttackableUnit {
public:
    BarracksDampenerClient() = default;
    explicit BarracksDampenerClient(uintptr_t address)
        : AttackableUnit(address, ::Core::Objects::ObjectType::BarracksDampenerClient) {}
    explicit BarracksDampenerClient(::Core::Objects::ObjectHandle handle)
        : AttackableUnit(handle) {
        handle_.type = ::Core::Objects::ObjectType::BarracksDampenerClient;
    }

    // Inhibitor lane index: 0 = top, 1 = mid, 2 = bot.
    // Unit Name pattern: "Barracks_T{team}_{LaneCode}{laneIndex}_..."
    //   LaneCode T = top, C = mid (center), L = bot
    //   The trailing index is the inhib position within the lane (always 1
    //   on Summoner's Rift) — we map LaneCode to {0,1,2} for orbwalker
    //   priority lists.
    int LaneIndex() const {
        const std::string unitName = ObjectDetail::ToLower(Name());
        if (unitName.find("_t1") != std::string::npos ||
            unitName.find("_t2") != std::string::npos) {
            // Both teams use _T1_ / _T2_ for the team prefix; we need the
            // SECOND letter token after the team prefix to identify lane.
            const auto laneTokenPos = unitName.find("_l1");
            if (laneTokenPos != std::string::npos) return 2; // bot
            const auto midTokenPos = unitName.find("_c1");
            if (midTokenPos != std::string::npos) return 1; // mid
            return 0; // top by default
        }
        return -1;
    }

    bool IsTopLane() const { return LaneIndex() == 0; }
    bool IsMidLane() const { return LaneIndex() == 1; }
    bool IsBotLane() const { return LaneIndex() == 2; }
};

// Nexus / nhà chính. CDragon Map11 spawns exactly 2 (one per team) and
// they live in front of the spawn fountain. They are vulnerable only
// AFTER both inhibitors of any single lane are destroyed; before that
// HasShield() returns true and orbwalker should not target them.
class HQClient : public AttackableUnit {
public:
    HQClient() = default;
    explicit HQClient(uintptr_t address)
        : AttackableUnit(address, ::Core::Objects::ObjectType::HQClient) {}
    explicit HQClient(::Core::Objects::ObjectHandle handle)
        : AttackableUnit(handle) {
        handle_.type = ::Core::Objects::ObjectType::HQClient;
    }

    // Nexus is invulnerable until at least one inhibitor of the
    // attacker's team is destroyed AND its respawn timer hasn't ticked
    // back. Use the engine-reported invulnerability flag — Riot keeps
    // this in sync with the inhibitor state, so we don't need to
    // re-derive it from BarracksDampenerClient health.
    bool HasShield() const {
        return ::Core::Objects::ReadBoolByte(Address(), Offset::All::IsInvulnerable);
    }
};

class ShopClient : public GameObject {
public:
    ShopClient() = default;
    explicit ShopClient(uintptr_t address)
        : GameObject(address, ::Core::Objects::ObjectType::ShopClient) {}
    explicit ShopClient(::Core::Objects::ObjectHandle handle)
        : GameObject(handle) {
        handle_.type = ::Core::Objects::ObjectType::ShopClient;
    }
};

class Obj_SpawnPoint : public GameObject {
public:
    Obj_SpawnPoint() = default;
    explicit Obj_SpawnPoint(uintptr_t address)
        : GameObject(address, ::Core::Objects::ObjectType::Obj_SpawnPoint) {}
    explicit Obj_SpawnPoint(::Core::Objects::ObjectHandle handle)
        : GameObject(handle) {
        handle_.type = ::Core::Objects::ObjectType::Obj_SpawnPoint;
    }
};

class EffectEmitter : public GameObject {
public:
    EffectEmitter() = default;
    explicit EffectEmitter(uintptr_t address)
        : GameObject(address, ::Core::Objects::ObjectType::EffectEmitter) {}
    explicit EffectEmitter(::Core::Objects::ObjectHandle handle)
        : GameObject(handle) {
        handle_.type = ::Core::Objects::ObjectType::EffectEmitter;
    }
};

} // namespace SDK
