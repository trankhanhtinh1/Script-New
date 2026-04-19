#pragma once

#include "../../core/CoreAPI.h"
#include "../../core/CoreBypass.h"
#include "../../core/CoreItem.h"
#include "../../core/Globals.h"
#include "../../core/Offsets.h"
#include "../../core/RuntimeAPI.h"
#include "../../core/Vector.h"
#include "../Enumerations/DamageType.h"
#include "../Enumerations/GameObjectOrder.h"
#include "../Enumerations/SpellSlot.h"
#include "ItemCatalog.h"
#include "Items.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
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

class AIHeroClient;
namespace ObjectManager {
    AIHeroClient Player();
}
class AIBaseClient;
namespace Damage {
    float GetSpellDamage(const AIBaseClient& source, const AIBaseClient& target, SpellSlot slot);
}

class SpellDataInstClient {
public:
    SpellDataInstClient() = default;
    SpellDataInstClient(uintptr_t owner, SpellSlot slot)
        : m_owner(owner), m_slot(slot) {}

    bool IsValid() const {
        return GetSlot().IsValid();
    }

    SpellSlot Slot() const {
        return m_slot;
    }

    int Level() const {
        return GetSlot().GetLevel();
    }

    int Ammo() const {
        return GetSlot().GetAmmo();
    }

    int Stacks() const {
        return GetSlot().GetStacks();
    }

    float Cooldown() const {
        return GetSlot().GetCooldown();
    }

    float TotalCooldown() const {
        return GetSlot().GetTotalCooldown();
    }

    float RemainingCooldown(float gameTime = CoreAPI::Game::GetTime()) const {
        return GetSlot().GetRemainingCooldown(gameTime);
    }

    float ManaCost() const {
        return GetSlot().GetManaCost();
    }

    float CastRange() const {
        return GetSlot().GetCastRange(Level());
    }

    float Width() const {
        return GetSlot().GetLineWidth();
    }

    float MissileSpeed() const {
        return GetSlot().GetMissileSpeed();
    }

    int CastType() const {
        return GetSlot().GetCastType();
    }

    std::string Name() const {
        char buf[128] = {};
        return GetSlot().ReadSpellName(buf, sizeof(buf)) ? std::string(buf) : std::string();
    }

    std::string ScriptName() const {
        char buf[128] = {};
        return GetSlot().ReadScriptName(buf, sizeof(buf)) ? std::string(buf) : std::string();
    }

    CoreAPI::SpellBook::SpellState State(float gameTime = CoreAPI::Game::GetTime()) const {
        return CoreAPI::SpellBook::GetSpellState(m_owner, static_cast<int>(m_slot), gameTime);
    }

    bool IsReady(float gameTime = CoreAPI::Game::GetTime()) const {
        return State(gameTime) == CoreSpellBook::State_Ready;
    }

private:
    CoreSpellBook::SlotRef GetSlot() const {
        return CoreAPI::SpellBook::GetSlot(m_owner, static_cast<int>(m_slot));
    }

    uintptr_t m_owner = 0;
    SpellSlot m_slot = SpellSlot::Unknown;
};

class SpellbookClient {
public:
    SpellbookClient() = default;
    explicit SpellbookClient(uintptr_t owner)
        : m_owner(owner) {}

    bool IsValid() const {
        return CoreAPI::SpellBook::Get(m_owner) != 0;
    }

    SpellDataInstClient GetSpell(SpellSlot slot) const {
        return SpellDataInstClient(m_owner, slot);
    }

private:
    uintptr_t m_owner = 0;
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
    bool IsHero() const { return RuntimeAPI::IsHero(Address()); }
    bool IsMinion() const { return RuntimeAPI::IsMinion(Address()); }
    bool IsTurret() const { return RuntimeAPI::IsTurret(Address()); }
    bool IsMissile() const { return RuntimeAPI::IsMissile(Address()); }
    bool IsPlant() const { return RuntimeAPI::IsPlant(Address()); }
    bool IsPet() const { return RuntimeAPI::IsPet(Address()); }
    bool IsNeutral() const { return RuntimeAPI::IsNeutral(Address()); }
    bool IsJungleMonster() const { return RuntimeAPI::IsJungleMonster(Address()); }
    bool IsLaneMinion() const { return RuntimeAPI::IsLaneMinion(Address()); }
    bool IsMelee() const { return m_ref.IsMelee(); }
    bool IsRanged() const { return m_ref.IsRanged(); }
    bool IsMe() const {
        const auto local = CoreAPI::Objects::GetLocalPlayer();
        return local != 0 && local == Address();
    }

    bool HasBuff(const char* name) const { return m_ref.HasBuff(name); }
    bool HasBuffOfType(int type) const { return m_ref.HasBuffType(type); }
    int GetBuffStacks(const char* name) const { return m_ref.GetBuffStacks(name); }
    int GetBuffCount(const char* name) const { return m_ref.GetBuffStacks(name); }
    float GetBuffRemainingTime(const char* name, float gameTime = CoreAPI::Game::GetTime()) const {
        return m_ref.GetBuffRemainingTime(name, gameTime);
    }

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

class AIBaseClient : public GameObject {
public:
    AIBaseClient() = default;
    explicit AIBaseClient(uintptr_t address)
        : GameObject(address) {}
    explicit AIBaseClient(const CoreObjects::ObjectRef& ref)
        : GameObject(ref) {}

    SpellbookClient GetSpellBook() const {
        return SpellbookClient(Address());
    }

    SpellbookClient Spellbook() const {
        return GetSpellBook();
    }

    bool CanAttack() const { return m_ref.CanAttack(); }
    bool CanMove() const { return m_ref.CanMove(); }
    bool CanCast() const { return m_ref.CanCast(); }

    bool InAutoAttackRange(const GameObject& target, float extraRange = 0.0f) const {
        return m_ref.IsInAutoAttackRange(target.Ref(), extraRange);
    }

    float RealAutoAttackRange(const GameObject& target) const {
        return m_ref.GetRealAttackRange(target.Ref());
    }

    float GetRealAutoAttackRange(const GameObject& target = GameObject()) const {
        return m_ref.GetRealAttackRange(target.Ref());
    }

    float GetAutoAttackDamage(const GameObject& target, bool includePassives = true) const {
        (void)includePassives;
        if (!target.IsValid() || !Globals::IsValidPtr(target.Address())) {
            return 0.0f;
        }

        __try {
            const float rawDamage = TotalAttackDamage();
            const float armor = target.Armor();
            if (armor >= 0.0f) {
                return rawDamage * (100.0f / (100.0f + armor));
            }
            return rawDamage * (2.0f - (100.0f / (100.0f - armor)));
        }
        __except (1) {
            return 0.0f;
        }
    }

    float CalculatePhysicalDamage(const GameObject& target, float rawDamage) const {
        if (!target.IsValid() || rawDamage <= 0.0f || !Globals::IsValidPtr(target.Address())) {
            return 0.0f;
        }

        __try {
            const float mitigatedArmor = target.Armor() - FlatArmorPenetration();
            if (mitigatedArmor >= 0.0f) {
                return rawDamage * (100.0f / (100.0f + mitigatedArmor));
            }
            return rawDamage * (2.0f - (100.0f / (100.0f - mitigatedArmor)));
        }
        __except (1) {
            return 0.0f;
        }
    }

    float CalculateMagicDamage(const GameObject& target, float rawDamage) const {
        if (!target.IsValid() || rawDamage <= 0.0f || !Globals::IsValidPtr(target.Address())) {
            return 0.0f;
        }

        __try {
            const float mitigatedMR = target.SpellBlock() - FlatMagicPenetration();
            if (mitigatedMR >= 0.0f) {
                return rawDamage * (100.0f / (100.0f + mitigatedMR));
            }
            return rawDamage * (2.0f - (100.0f / (100.0f - mitigatedMR)));
        }
        __except (1) {
            return 0.0f;
        }
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

    float AttackDelay() const {
        return CoreAPI::Control::GetAttackDelay(Address());
    }

    float AttackCastDelay() const {
        return CoreAPI::Control::GetAttackWindup(Address());
    }

    // ── HasItem(riotItemId) ──
    //
    // Resolution order:
    //   1. Learned catalog: if ItemCatalog has a RiotId -> internalId mapping
    //      for this patch, compare against info+0xB4 across all visible slots.
    //   2. Unique-buff fallback: for items whose presence can be proven by a
    //      buff that no other item or spell produces (e.g. "InfinityEdge",
    //      "lichbane", "itemfrozenfist"), check HasBuff. This also records a
    //      learned mapping by snapshotting the slot whose internal id changes
    //      coincides with the buff being present, so subsequent calls short
    //      circuit through the catalog.
    //   3. Otherwise return false (caller falls back to HasBuff-based logic).
    bool HasItem(int riotItemId) const {
        if (!IsValid() || riotItemId <= 0) {
            return false;
        }

        // 1) Catalog lookup
        if (const uint32_t internalId = ItemCatalog::GetInternal(riotItemId)) {
            return m_ref.HasItemWithInternalId(internalId);
        }

        // 2) Unique-buff fallback
        if (const char* buffName = ItemCatalog::GetUniqueBuffFor(riotItemId)) {
            if (m_ref.HasBuff(buffName)) {
                // Try to learn the internal id for this riot id so future
                // checks short-circuit through the catalog. Accept a mapping
                // only when exactly one visible slot holds an as-yet-unknown
                // internal id; otherwise we'd risk binding the wrong slot.
                uint32_t candidate = 0;
                int unknownCount = 0;
                for (int i = 0; i < CoreItem::SLOT_VISIBLE_COUNT; ++i) {
                    const uint32_t raw = m_ref.GetItemInternalId(i);
                    if (raw == 0) continue;
                    if (ItemCatalog::GetRiot(raw) != 0) continue; // already claimed
                    if (++unknownCount > 1) { candidate = 0; break; }
                    candidate = raw;
                }
                if (candidate != 0) {
                    ItemCatalog::Learn(riotItemId, candidate);
                }
                return true;
            }
        }

        // 3) No reliable way to tell — conservative false.
        return false;
    }

    // ── HasItem(scriptName) ──
    //
    // Looks up `scriptName` in the SDK::Items name table (case-insensitive,
    // ignores spaces/underscores/apostrophes/dashes) and delegates to the
    // integer overload. Empty or unknown names return false.
    //
    //   hero.HasItem("Tiamat")
    //   hero.HasItem("Blade Of The Ruined King")
    //   hero.HasItem("BotRK")
    //   hero.HasItem("Rabadon's Deathcap")
    bool HasItem(const char* scriptName) const {
        const int riotId = SDK::Items::FromScript(scriptName);
        return (riotId > 0) && HasItem(riotId);
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

    float GetSpellDamage(const GameObject& target, SpellSlot slot) const {
        return Damage::GetSpellDamage(*this, AIBaseClient(target.Address()), slot);
    }

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

    SpellSlot GetSpellSlot(const char* scriptOrSpellName) const {
        if (!scriptOrSpellName || !scriptOrSpellName[0]) {
            return SpellSlot::Unknown;
        }

        const auto spellbook = GetSpellBook();
        // Iterate real slots [Q..Recall] only — SpellSlot::Unknown is a sentinel
        // (value 14) and GetSpell(Unknown) has no meaningful result.
        for (int i = 0; i < static_cast<int>(SpellSlot::Unknown); ++i) {
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

    bool IsCastingImportantSpell() const {
        return IsCastingInterruptableSpell(false);
    }

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

class AIHeroClient : public AIBaseClient {
public:
    using AIBaseClient::AIBaseClient;
};

class AIMinionClient : public AIBaseClient {
public:
    using AIBaseClient::AIBaseClient;

    bool IsJungle() const { return IsJungleMonster(); }
};

class AITurretClient : public AIBaseClient {
public:
    using AIBaseClient::AIBaseClient;
};

class MissileClient : public GameObject {
public:
    using GameObject::GameObject;

    int CasterNetworkId() const { return m_ref.GetMissileCasterNetId(); }
    int TargetNetworkId() const { return m_ref.GetMissileTargetNetId(); }
    Vector3 StartPosition() const { return m_ref.GetMissileStartPos(); }
    Vector3 EndPosition() const { return m_ref.GetMissileEndPos(); }
};

namespace ObjectManager {

    namespace detail {

        inline int ReadManagerListLegacy(uintptr_t globalRva, uintptr_t* out, int maxOut, int hardCap) {
            if (!out || maxOut <= 0 || !Globals::base || !globalRva) {
                return 0;
            }

            const uintptr_t mgr = Globals::Read<uintptr_t>(Globals::base + globalRva);
            if (!Globals::IsValidPtr(mgr)) {
                return 0;
            }

            const uintptr_t list = Globals::Read<uintptr_t>(mgr + 0x8);
            const int count = Globals::Read<int>(mgr + 0x10);
            if (!Globals::IsValidPtr(list) || count <= 0) {
                return 0;
            }

            const int safeCount = std::min(count, std::min(maxOut, hardCap));
            if (safeCount <= 0) {
                return 0;
            }

            return Globals::ReadPtrArray(list, safeCount, out, maxOut);
        }

        inline int ReadAllObjectsLegacy(uintptr_t* out, int maxOut) {
            if (!out || maxOut <= 0 || !Globals::base) {
                return 0;
            }

            const uintptr_t mgr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::ObjectManager);
            if (!Globals::IsValidPtr(mgr)) {
                return 0;
            }

            using fnGetFirst = uintptr_t(__cdecl*)(uintptr_t);
            using fnGetNext = uintptr_t(__cdecl*)(uintptr_t, uintptr_t);

            const auto getFirst = reinterpret_cast<fnGetFirst>(Globals::base + Offset::Function::GetFirstObject);
            const auto getFirstAlt = reinterpret_cast<fnGetFirst>(Globals::base + Offset::Function::GetFirstObjectAlt);
            const auto getNext = reinterpret_cast<fnGetNext>(Globals::base + Offset::Function::GetNextObject);

            auto iterateFrom = [&](fnGetFirst starter) -> int {
                if (!starter || !getNext) {
                    return 0;
                }

                int count = 0;
                __try {
                    CoreBypass::MainloopCheck();
                    uintptr_t obj = starter(mgr);
                    while (Globals::IsValidPtr(obj) && count < maxOut) {
                        out[count++] = obj;
                        obj = getNext(mgr, obj);
                    }
                }
                __except (1) {
                    return count;
                }
                return count;
            };

            int count = iterateFrom(getFirst);
            if (count == 0 && getFirstAlt && getFirstAlt != getFirst) {
                count = iterateFrom(getFirstAlt);
            }
            if (count > 0) {
                return count;
            }

            const uintptr_t list = Globals::Read<uintptr_t>(mgr + 0x8);
            const int listCount = Globals::Read<int>(mgr + 0x10);
            if (!Globals::IsValidPtr(list) || listCount <= 0) {
                return 0;
            }

            const int safeCount = std::min(listCount, maxOut);
            return Globals::ReadPtrArray(list, safeCount, out, maxOut);
        }

        inline std::string ToLower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        inline std::string BestName(const GameObject& obj) {
            std::string name = obj.CharacterName();
            if (name.empty()) {
                name = obj.Name();
            }
            return ToLower(std::move(name));
        }

        inline bool IsJunglePlantName(const std::string& lowerName) {
            return lowerName.find("sru_plant") != std::string::npos ||
                   lowerName.find("hiddenminionplantdemon") != std::string::npos ||
                   lowerName.find("planthealthmirrored") != std::string::npos ||
                   lowerName.find("plantmasterminion") != std::string::npos ||
                   lowerName.find("minimapicon") != std::string::npos;
        }

        inline bool IsKnownJungleMonsterName(const std::string& lowerName) {
            static const char* known[] = {
                "sru_baron", "sru_dragon", "sru_riftherald", "voidgrub", "sru_atakhan",
                "sru_blue", "sru_red", "sru_gromp", "sru_krug", "sru_murkwolf",
                "sru_razorbeak", "sru_crab", "sru_riftscuttler"
            };

            if (IsJunglePlantName(lowerName)) {
                return false;
            }

            for (const auto* token : known) {
                if (lowerName.find(token) != std::string::npos) {
                    return true;
                }
            }
            return false;
        }

        inline bool IsLaneMinionName(const std::string& lowerName) {
            return lowerName.find("sru_chaosminion") != std::string::npos ||
                   lowerName.find("sru_orderminion") != std::string::npos ||
                   lowerName.find("ha_chaosminion") != std::string::npos ||
                   lowerName.find("ha_orderminion") != std::string::npos;
        }

        inline bool IsLaneMinionObject(const AIMinionClient& obj) {
            if (!obj.IsValid() || !obj.IsAlive()) {
                return false;
            }

            if (obj.Team() == 300) {
                return false;
            }

            if (obj.IsHero() || obj.IsTurret()) {
                return false;
            }

            const float maxHP = obj.MaxHealth();
            if (maxHP <= 0.0f || maxHP >= 10000.0f) {
                return false;
            }

            if (obj.IsPlant() || obj.IsPet() || obj.IsJungleMonster()) {
                return false;
            }

            if (obj.IsLaneMinion()) {
                return true;
            }

            const std::string lowerName = BestName(obj);
            if (IsJunglePlantName(lowerName) || IsKnownJungleMonsterName(lowerName)) {
                return false;
            }

            return obj.IsMinion() || IsLaneMinionName(lowerName);
        }

        inline bool IsPlantObject(const AIMinionClient& obj) {
            if (!obj.IsValid() || !obj.IsAlive()) {
                return false;
            }

            // NOTE: Do NOT use a "maxHP <= 6" heuristic here — wards have 3-6 HP
            // and would be misclassified as plants. Rely on the native plant flag
            // (RuntimeAPI::IsPlant checks 0x8000 + "SRU_Plant" name fallback) and
            // the explicit jungle-plant name list.
            const std::string lowerName = BestName(obj);
            // Exclude wards inline (IsWardObject defined later in the same namespace).
            if (lowerName.find("ward") != std::string::npos ||
                lowerName.find("jammerdevice") != std::string::npos) {
                return false;
            }
            return obj.IsPlant() || IsJunglePlantName(lowerName);
        }

        inline bool IsJungleObject(const AIMinionClient& obj) {
            if (!obj.IsValid() || !obj.IsAlive()) {
                return false;
            }

            if (obj.Team() != 300) {
                return false;
            }

            if (IsPlantObject(obj)) {
                return false;
            }

            const float maxHP = obj.MaxHealth();
            if (maxHP <= 6.0f) {
                return false;
            }

            const std::string lowerName = BestName(obj);
            return obj.IsJungleMonster() || IsKnownJungleMonsterName(lowerName);
        }

        inline bool IsPetObject(const AIMinionClient& obj) {
            if (!obj.IsValid() || !obj.IsAlive()) {
                return false;
            }

            if (obj.IsPlant()) {
                return false;
            }

            return obj.IsPet();
        }

        inline bool IsWardObject(const AIMinionClient& obj) {
            if (!obj.IsValid() || !obj.IsAlive()) {
                return false;
            }
            const std::string lowerName = BestName(obj);
            return lowerName.find("ward") != std::string::npos ||
                   lowerName.find("jammerdevice") != std::string::npos;
        }

        inline bool IsBarrelObject(const AIMinionClient& obj) {
            if (!obj.IsValid() || !obj.IsAlive()) {
                return false;
            }
            const std::string lowerName = BestName(obj);
            return lowerName.find("gangplankbarrel") != std::string::npos;
        }

        inline bool IsSpecialMinionObject(const AIMinionClient& obj) {
            if (!obj.IsValid() || !obj.IsAlive()) {
                return false;
            }
            const std::string lowerName = BestName(obj);
            static const char* specials[] = {
                "annietibbers", "elisespiderling", "heimertyellow", "heimertblue",
                "ivernminion", "malzaharvoidling", "shacobox", "teemomushroom",
                "yorickghoulmelee", "yorickbigghoul", "zyrathornplant", "zyragraspingplant"
            };
            for (const auto& s : specials) {
                if (lowerName == s) return true;
            }
            return false;
        }

        inline bool IsCloneObject(const AIMinionClient& obj) {
            if (!obj.IsValid() || !obj.IsAlive()) {
                return false;
            }
            const std::string lowerName = BestName(obj);
            static const char* clones[] = { "leblanc", "monkeyking", "neeko", "shaco" };
            for (const auto& c : clones) {
                if (lowerName == c) return true;
            }
            return false;
        }
    } // namespace detail

    inline AIHeroClient Player() {
        return AIHeroClient(CoreAPI::Objects::GetLocalPlayer());
    }

    inline GameObject UnderMouse() {
        return GameObject(CoreAPI::Objects::GetUnderMouseObject());
    }

    inline std::vector<AIHeroClient> Heroes() {
        uintptr_t buffer[64] = {};
        const int count = detail::ReadManagerListLegacy(Offset::Global::HeroManager, buffer, 64, 64);
        std::vector<AIHeroClient> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            AIHeroClient hero(buffer[i]);
            if (!hero.IsValid()) {
                continue;
            }
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    inline GameObject GetByNetId(int netId) {
        if (netId <= 0) {
            return GameObject();
        }

        uintptr_t buffer[4096] = {};
        const int count = detail::ReadAllObjectsLegacy(buffer, 4096);
        for (int i = 0; i < count; ++i) {
            GameObject obj(buffer[i]);
            if (obj.IsValid() && obj.NetworkId() == netId) {
                return obj;
            }
        }
        return GameObject();
    }

    inline GameObject GetByIndex(int index) {
        if (index <= 0) {
            return GameObject();
        }

        uintptr_t buffer[4096] = {};
        const int count = detail::ReadAllObjectsLegacy(buffer, 4096);
        for (int i = 0; i < count; ++i) {
            GameObject obj(buffer[i]);
            if (obj.IsValid() && obj.Index() == index) {
                return obj;
            }
        }
        return GameObject();
    }

    inline std::vector<AIHeroClient> AllyHeroes() {
        const int myTeam = Player().Team();
        uintptr_t buffer[64] = {};
        const int count = detail::ReadManagerListLegacy(Offset::Global::HeroManager, buffer, 64, 64);
        std::vector<AIHeroClient> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            AIHeroClient hero(buffer[i]);
            if (!hero.IsValid() || hero.Team() != myTeam) {
                continue;
            }
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    inline std::vector<AIHeroClient> EnemyHeroes() {
        const int myTeam = Player().Team();
        uintptr_t buffer[64] = {};
        const int count = detail::ReadManagerListLegacy(Offset::Global::HeroManager, buffer, 64, 64);
        std::vector<AIHeroClient> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            AIHeroClient hero(buffer[i]);
            if (!hero.IsValid() || hero.Team() == myTeam || hero.Team() == 300) {
                continue;
            }
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    // ── AllyMinions — use RuntimeAPI directly (matches old NightSharp) ──
    // Old NightSharp: FillMinions() returns ALL from MinionManager, then
    // caller checks obj.IsLaneMinion() which delegates to RuntimeAPI::IsLaneMinion()
    inline std::vector<AIMinionClient> AllyMinions() {
        const int myTeam = Player().Team();
        uintptr_t buffer[512] = {};
        const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
        std::vector<AIMinionClient> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            if (!Globals::IsValidPtr(buffer[i])) continue;
            // Use RuntimeAPI directly — same as old NightSharp obj.IsLaneMinion()
            if (!RuntimeAPI::IsLaneMinion(buffer[i])) continue;
            AIMinionClient minion(buffer[i]);
            if (!minion.IsAlive()) continue;
            if (minion.Team() != myTeam) continue;
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    // ── EnemyMinions — use RuntimeAPI directly (matches old NightSharp) ──
    inline std::vector<AIMinionClient> EnemyMinions() {
        const int myTeam = Player().Team();
        uintptr_t buffer[512] = {};
        const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
        std::vector<AIMinionClient> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            if (!Globals::IsValidPtr(buffer[i])) continue;
            // Use RuntimeAPI directly — same as old NightSharp obj.IsLaneMinion()
            if (!RuntimeAPI::IsLaneMinion(buffer[i])) continue;
            AIMinionClient minion(buffer[i]);
            if (!minion.IsAlive()) continue;
            const int team = minion.Team();
            if (team == myTeam || team == 300) continue;
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    // ── JungleMinions — use RuntimeAPI directly (matches old NightSharp) ──
    // Old NightSharp: obj.IsJungleMonster() → RuntimeAPI::IsJungleMonster()
    inline std::vector<AIMinionClient> JungleMinions() {
        uintptr_t buffer[512] = {};
        const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
        std::vector<AIMinionClient> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            if (!Globals::IsValidPtr(buffer[i])) continue;
            // Use RuntimeAPI directly — same as old NightSharp obj.IsJungleMonster()
            if (!RuntimeAPI::IsJungleMonster(buffer[i])) continue;
            AIMinionClient minion(buffer[i]);
            if (!minion.IsAlive()) continue;
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    inline std::vector<AIMinionClient> Plants() {
        uintptr_t buffer[512] = {};
        const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
        std::vector<AIMinionClient> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            AIMinionClient minion(buffer[i]);
            if (!detail::IsPlantObject(minion)) {
                continue;
            }
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    inline std::vector<AIMinionClient> Pets() {
        uintptr_t buffer[512] = {};
        const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
        std::vector<AIMinionClient> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            AIMinionClient minion(buffer[i]);
            if (!detail::IsPetObject(minion)) {
                continue;
            }
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    inline std::vector<AIMinionClient> Wards() {
        uintptr_t buffer[512] = {};
        const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
        std::vector<AIMinionClient> out;
        out.reserve(16);
        for (int i = 0; i < count; ++i) {
            AIMinionClient minion(buffer[i]);
            if (!detail::IsWardObject(minion)) {
                continue;
            }
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    inline std::vector<AIMinionClient> Barrels() {
        uintptr_t buffer[512] = {};
        const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
        std::vector<AIMinionClient> out;
        out.reserve(8);
        for (int i = 0; i < count; ++i) {
            AIMinionClient minion(buffer[i]);
            if (!detail::IsBarrelObject(minion)) {
                continue;
            }
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    inline std::vector<AITurretClient> AllyTurrets() {
        uintptr_t buffer[64] = {};
        const int count = CoreAPI::Objects::EnumerateAllyTurrets(buffer, 64);
        std::vector<AITurretClient> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    inline std::vector<AIMinionClient> SpecialMinions() {
        uintptr_t buffer[512] = {};
        const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
        std::vector<AIMinionClient> out;
        out.reserve(16);
        const int myTeam = Player().Team();
        for (int i = 0; i < count; ++i) {
            AIMinionClient minion(buffer[i]);
            if (!detail::IsSpecialMinionObject(minion)) continue;
            if (minion.Team() == myTeam) continue;  // enemy specials only
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    inline std::vector<AIMinionClient> Clones() {
        uintptr_t buffer[512] = {};
        const int count = detail::ReadManagerListLegacy(Offset::Global::MinionManager, buffer, 512, 2000);
        std::vector<AIMinionClient> out;
        out.reserve(8);
        const int myTeam = Player().Team();
        for (int i = 0; i < count; ++i) {
            AIMinionClient minion(buffer[i]);
            if (!detail::IsCloneObject(minion)) continue;
            if (minion.Team() == myTeam) continue;  // enemy clones only
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    inline std::vector<AITurretClient> EnemyTurrets() {
        uintptr_t buffer[64] = {};
        const int count = CoreAPI::Objects::EnumerateEnemyTurrets(buffer, 64);
        std::vector<AITurretClient> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    inline std::vector<MissileClient> Missiles() {
        uintptr_t buffer[1024] = {};
        const int count = CoreAPI::Objects::EnumerateMissiles(buffer, 1024);
        std::vector<MissileClient> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    inline std::vector<GameObject> AllObjects() {
        uintptr_t buffer[4096] = {};
        const int count = detail::ReadAllObjectsLegacy(buffer, 4096);
        std::vector<GameObject> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            out.emplace_back(buffer[i]);
        }
        return out;
    }

    // Inhibitors (BarracksDampener) — iterate AllObjects, filter by "Barracks" name prefix
    inline std::vector<AIBaseClient> EnemyInhibitors() {
        const int myTeam = Player().Team();
        std::vector<AIBaseClient> out;
        for (const auto& obj : AllObjects()) {
            if (!obj.IsValid() || obj.Team() == myTeam) continue;
            std::string name = obj.CharacterName();
            if (name.size() >= 8 && _strnicmp(name.c_str(), "Barracks", 8) == 0) {
                AIBaseClient inhib(obj.Address());
                if (inhib.IsAlive()) out.push_back(inhib);
            }
        }
        return out;
    }

    // Nexus (HQ) — iterate AllObjects, filter by "HQ" name prefix
    inline AIBaseClient EnemyNexus() {
        const int myTeam = Player().Team();
        for (const auto& obj : AllObjects()) {
            if (!obj.IsValid() || obj.Team() == myTeam) continue;
            std::string name = obj.CharacterName();
            if (name.size() >= 2 && _strnicmp(name.c_str(), "HQ", 2) == 0) {
                AIBaseClient nexus(obj.Address());
                if (nexus.IsAlive()) return nexus;
            }
        }
        return AIBaseClient();
    }
}

namespace GameObjects {
    inline AIHeroClient Player() { return ObjectManager::Player(); }
    inline std::vector<AIHeroClient> Heroes() { return ObjectManager::Heroes(); }
    inline std::vector<AIHeroClient> AllyHeroes() { return ObjectManager::AllyHeroes(); }
    inline std::vector<AIHeroClient> EnemyHeroes() { return ObjectManager::EnemyHeroes(); }
    inline std::vector<AIMinionClient> AllyMinions() { return ObjectManager::AllyMinions(); }
    inline std::vector<AIMinionClient> EnemyMinions() { return ObjectManager::EnemyMinions(); }
    inline std::vector<AIMinionClient> Jungle() { return ObjectManager::JungleMinions(); }
    inline std::vector<AIMinionClient> JungleMinions() { return ObjectManager::JungleMinions(); }
    inline std::vector<AIMinionClient> Plants() { return ObjectManager::Plants(); }
    inline std::vector<AIMinionClient> JunglePlants() { return ObjectManager::Plants(); }
    inline std::vector<AIMinionClient> Pets() { return ObjectManager::Pets(); }
    inline std::vector<AIMinionClient> Wards() { return ObjectManager::Wards(); }
    inline std::vector<AIMinionClient> Barrels() { return ObjectManager::Barrels(); }
    inline std::vector<AIMinionClient> SpecialMinions() { return ObjectManager::SpecialMinions(); }
    inline std::vector<AIMinionClient> Clones() { return ObjectManager::Clones(); }
    inline std::vector<AITurretClient> AllyTurrets() { return ObjectManager::AllyTurrets(); }
    inline std::vector<AITurretClient> EnemyTurrets() { return ObjectManager::EnemyTurrets(); }
    inline std::vector<MissileClient> Missiles() { return ObjectManager::Missiles(); }
    inline std::vector<GameObject> AllObjects() { return ObjectManager::AllObjects(); }
    inline std::vector<AIBaseClient> EnemyInhibitors() { return ObjectManager::EnemyInhibitors(); }
    inline AIBaseClient EnemyNexus() { return ObjectManager::EnemyNexus(); }
}

} // namespace SDK
