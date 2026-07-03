#pragma once

#include "CoreAIHeroClient.h"
#include "CoreAttackableUnit.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include "../SectionProfiler.h"

#ifndef NIGHTSHARP_ENABLE_OBJECT_VFUNC_READS
#define NIGHTSHARP_ENABLE_OBJECT_VFUNC_READS 0
#endif

namespace Core::Objects {

enum class ObjectType : std::int32_t {
    Unknown = 0,
    AIHeroClient = 1,
    AIMarker = 2,
    AIMinionClient = 3,
    AITurretClient = 4,
    AITurretCommon = 5,
    AnimatedBuildingClient = 6,
    Barracks = 7,
    BarracksDampenerClient = 8,
    BuildingClient = 9,
    DrawFX = 10,
    GameObject = 11,
    GrassObject = 12,
    HQClient = 13,
    LevelPropAIClient = 14,
    LevelPropGameObjectClient = 15,
    LevelPropSpawnerPoint = 16,
    MapPropClient = 17,
    MissileClient = 18,
    NeutralMinionCampClient = 19,
    EffectEmitter = 20,
    Obj_InfoPoint = 21,
    Obj_Levelsizer = 22,
    Obj_NavPoint = 23,
    Obj_SpawnPoint = 24,
    ObjectAttacher = 25,
    Pawn = 26,
    ShopClient = 27,
    Turret = 28,
    UnrevealedTarget = 29,
};

enum class Team : std::uint32_t {
    Unknown = 0,
    Order = 100,
    Chaos = 200,
    Neutral = 300,
};

enum class MinionClass : std::uint8_t {
    Unset = Offset::MinionClassRuntime::Unset,
    Pet = Offset::MinionClassRuntime::Pet,
    JungleMonster = Offset::MinionClassRuntime::JungleMonster,
    TeamMinion = Offset::MinionClassRuntime::TeamMinion,
    MeleeLaneMinion = Offset::MinionClassRuntime::MeleeLaneMinion,
    RangedLaneMinion = Offset::MinionClassRuntime::RangedLaneMinion,
    SiegeLaneMinion = Offset::MinionClassRuntime::SiegeLaneMinion,
    SuperLaneMinion = Offset::MinionClassRuntime::SuperLaneMinion,
};

enum class JungleRuntimeType : std::uint8_t {
    Normal = Offset::JungleTypeRuntime::Normal,
    Baron = Offset::JungleTypeRuntime::Baron,
    Dragon = Offset::JungleTypeRuntime::Dragon,
};

struct ObjectHandle {
    uintptr_t address = 0;
    std::uint32_t networkId = 0;
    std::uint32_t index = 0;
    ObjectType type = ObjectType::Unknown;

    bool HasAddress() const {
        return Globals::IsValidPtr(address);
    }

    bool HasIdentity() const {
        return networkId != 0 && networkId != 0xFFFFFFFFu;
    }

    bool IsValid() const {
        return HasAddress() && HasIdentity();
    }
};

struct MissileInfo {
    uintptr_t spellData = 0;
    uintptr_t castInfo = 0;
    std::uint32_t casterIndex = 0;
    std::uint32_t targetIndex = 0;
    std::uint32_t casterNetworkId = 0;
    std::uint32_t targetNetworkId = 0;
    std::uint32_t missileNetworkId = 0;
    Vec3 startPosition = {};
    Vec3 endPosition = {};
    Vec3 castEndPosition = {};
    char spellName[96] = {};
    char missileName[96] = {};
};

struct ObjectSnapshot {
    ObjectHandle handle = {};
    std::uint8_t teamSlot = 0;
    std::uint32_t team = 0;
    bool isDead = false;
    bool isVisible = false;
    bool isTargetable = false;
    bool isInvulnerable = false;
    Vec3 position = {};
    Vec3 direction = {};
    float boundingRadius = 0.0f;
    float health = 0.0f;
    float maxHealth = 0.0f;
    float allShield = 0.0f;
    float physicalShield = 0.0f;
    float magicalShield = 0.0f;
    float mana = 0.0f;
    float maxMana = 0.0f;
    float moveSpeed = 0.0f;
    float attackRange = 0.0f;
    float totalAttackDamage = 0.0f;
    float baseAttackDamage = 0.0f;
    float abilityPower = 0.0f;
    float armor = 0.0f;
    float spellBlock = 0.0f;
    float attackSpeedMod = 0.0f;
    int level = 0;
    int minionClass = 0;
    int jungleType = 0;
    char name[96] = {};
    char characterName[96] = {};
    MissileInfo missile = {};

    bool IsValid() const {
        return handle.IsValid();
    }
};

inline bool IsAttackable(ObjectType type) {
    switch (type) {
    case ObjectType::AIHeroClient:
    case ObjectType::AIMinionClient:
    case ObjectType::AITurretClient:
    case ObjectType::AITurretCommon:
    case ObjectType::AnimatedBuildingClient:
    case ObjectType::Barracks:
    case ObjectType::BarracksDampenerClient:
    case ObjectType::BuildingClient:
    case ObjectType::HQClient:
    case ObjectType::LevelPropAIClient:
    case ObjectType::NeutralMinionCampClient:
    case ObjectType::Pawn:
    case ObjectType::Turret:
        return true;
    default:
        return false;
    }
}

inline bool IsAIBase(ObjectType type) {
    switch (type) {
    case ObjectType::AIHeroClient:
    case ObjectType::AIMinionClient:
    case ObjectType::AITurretClient:
    case ObjectType::AITurretCommon:
    case ObjectType::LevelPropAIClient:
    case ObjectType::NeutralMinionCampClient:
    case ObjectType::Pawn:
        return true;
    default:
        return false;
    }
}

inline bool IsMissile(ObjectType type) {
    return type == ObjectType::MissileClient;
}

inline bool IsLifecycleType(ObjectType type) {
    switch (type) {
    case ObjectType::AIHeroClient:
    case ObjectType::AIMinionClient:
    case ObjectType::AITurretClient:
    case ObjectType::MissileClient:
    case ObjectType::BarracksDampenerClient:
    case ObjectType::HQClient:
    case ObjectType::ShopClient:
    case ObjectType::Obj_SpawnPoint:
    case ObjectType::EffectEmitter:
        return true;
    default:
        return false;
    }
}

inline bool EqualsInsensitive(const char* lhs, const char* rhs) {
    if (!lhs || !rhs) {
        return false;
    }
    while (*lhs && *rhs) {
        if (std::tolower(static_cast<unsigned char>(*lhs)) !=
            std::tolower(static_cast<unsigned char>(*rhs))) {
            return false;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == 0 && *rhs == 0;
}

inline bool ContainsInsensitive(const char* value, const char* needle) {
    if (!value || !needle || !*needle) {
        return false;
    }

    for (const char* start = value; *start; ++start) {
        const char* a = start;
        const char* b = needle;
        while (*a && *b &&
               std::tolower(static_cast<unsigned char>(*a)) ==
                   std::tolower(static_cast<unsigned char>(*b))) {
            ++a;
            ++b;
        }
        if (!*b) {
            return true;
        }
    }
    return false;
}

inline bool IsClone(const ObjectSnapshot& snapshot) {
    if (snapshot.handle.type != ObjectType::AIMinionClient) {
        return false;
    }

    return EqualsInsensitive(snapshot.characterName, "Leblanc") ||
           EqualsInsensitive(snapshot.characterName, "MonkeyKing") ||
           EqualsInsensitive(snapshot.characterName, "Neeko") ||
           EqualsInsensitive(snapshot.characterName, "Shaco");
}

inline bool IsPet(const ObjectSnapshot& snapshot) {
    if (snapshot.handle.type != ObjectType::AIMinionClient ||
        IsClone(snapshot)) {
        return false;
    }

    if (static_cast<MinionClass>(snapshot.minionClass) == MinionClass::Pet) {
        return true;
    }

    const char* name = snapshot.characterName[0]
        ? snapshot.characterName
        : snapshot.name;
    static constexpr const char* kPetNames[] = {
        "AnnieTibbers", "EliseSpiderling", "HeimerTYellow",
        "HeimerTBlue", "IvernMinion", "MalzaharVoidling",
        "ShacoBox", "YorickGhoulMelee", "YorickBigGhoul",
        "ZyraThornPlant", "ZyraGraspingPlant",
    };
    for (const char* petName : kPetNames) {
        if (EqualsInsensitive(name, petName)) {
            return true;
        }
    }
    return false;
}

inline bool ShouldIgnoreLifecycle(const ObjectSnapshot& snapshot) {
    if (!snapshot.IsValid() || !IsLifecycleType(snapshot.handle.type)) {
        return true;
    }

    // These are bookkeeping/visual minions registered in ObjectManager,
    // not usable SDK units. They were the only concrete typed noise found
    // in the lifecycle log; generic GameObject allocations are rejected by
    // IsLifecycleType before reaching this point.
    if (snapshot.handle.type == ObjectType::AIMinionClient) {
        const char* name = snapshot.characterName[0]
            ? snapshot.characterName
            : snapshot.name;
        return ContainsInsensitive(name, "CampRespawnMarker") ||
               ContainsInsensitive(name, "RespawnMarker") ||
               ContainsInsensitive(name, "PlantMasterMinion") ||
               ContainsInsensitive(name, "HiddenMinion") ||
               ContainsInsensitive(name, "GenericUnit") ||
               ContainsInsensitive(name, "WardCorpse");
    }
    return false;
}

inline const char* TypeName(ObjectType type) {
    switch (type) {
    case ObjectType::AIHeroClient: return "AIHeroClient";
    case ObjectType::AIMinionClient: return "AIMinionClient";
    case ObjectType::AITurretClient: return "AITurretClient";
    case ObjectType::BarracksDampenerClient: return "BarracksDampenerClient";
    case ObjectType::HQClient: return "HQClient";
    case ObjectType::MissileClient: return "MissileClient";
    case ObjectType::EffectEmitter: return "EffectEmitter";
    case ObjectType::Obj_SpawnPoint: return "Obj_SpawnPoint";
    case ObjectType::ShopClient: return "ShopClient";
    case ObjectType::GameObject: return "GameObject";
    default: return "Unknown";
    }
}

template <typename T>
inline T ReadField(uintptr_t object, uintptr_t offset) {
    return Globals::Read<T>(object + offset);
}

inline uintptr_t RuntimeAddress(uintptr_t rva) {
    if (!Globals::base) {
        (void)Globals::Init();
    }
    return Globals::base ? Globals::base + rva : 0;
}

// -------------------------------------------------------------------------
// RVA function pointer cache
// VirtualQuery (kernel syscall) is expensive. Static game function addresses
// never change after module load, so we validate once and cache the result.
// kFnCacheCapacity of 64 comfortably covers all ClassificationRuntime RVAs
// plus spell/control/drawing functions used every frame.
// -------------------------------------------------------------------------
namespace FnCache {
    constexpr int kCapacity = 64;
    struct Entry { uintptr_t rva = 0; uintptr_t address = 0; };
    inline Entry g_entries[kCapacity] = {};
    inline int   g_count = 0;

    inline uintptr_t Lookup(uintptr_t rva) {
        for (int i = 0; i < g_count; ++i) {
            if (g_entries[i].rva == rva) return g_entries[i].address;
        }
        return ~static_cast<uintptr_t>(0); // sentinel = not found
    }

    inline void Store(uintptr_t rva, uintptr_t address) {
        if (g_count < kCapacity) {
            g_entries[g_count++] = {rva, address};
        }
    }

    // Call on module reinitialization so stale base+RVA mappings are cleared.
    inline void Invalidate() {
        for (int i = 0; i < g_count; ++i) g_entries[i] = {};
        g_count = 0;
    }
} // namespace FnCache

template <typename Fn>
inline Fn RuntimeFunction(uintptr_t rva) {
    constexpr uintptr_t kNotFound = ~static_cast<uintptr_t>(0);
    const uintptr_t cached = FnCache::Lookup(rva);
    if (cached != kNotFound) {
        return reinterpret_cast<Fn>(cached);
    }
    const uintptr_t address = RuntimeAddress(rva);
    const uintptr_t result  = Globals::IsExecutablePtr(address) ? address : 0;
    FnCache::Store(rva, result);
    return reinterpret_cast<Fn>(result);
}

inline bool TryCallObjectPredicate(uintptr_t object, uintptr_t rva, bool& out) {
    if (!Globals::IsValidPtr(object)) {
        out = false;
        return false;
    }

    __try {
        using ObjectPredicateFn = bool(__fastcall*)(uintptr_t);
        const auto fn = RuntimeFunction<ObjectPredicateFn>(rva);
        if (!fn) {
            out = false;
            return false;
        }

        out = fn(object);
        return true;
    }
    __except (1) {
        out = false;
        return false;
    }
}

inline bool ReadBoolByte(uintptr_t object, uintptr_t offset) {
    return ReadField<std::uint8_t>(object, offset) != 0;
}

inline bool IsSaneFloat(float value, float minValue, float maxValue) {
    return value == value && value >= minValue && value <= maxValue;
}

inline bool TryCallObjectFloatVFunc(uintptr_t object, uintptr_t vtableOffset, float& out) {
    if (!Globals::IsValidPtr(object)) {
        out = 0.0f;
        return false;
    }

    __try {
        const uintptr_t vtable = Globals::Read<uintptr_t>(object);
        if (!Globals::IsValidPtr(vtable)) {
            out = 0.0f;
            return false;
        }

        using ObjectFloatVFunc = float(__fastcall*)(uintptr_t);
        const uintptr_t fnAddress = Globals::Read<uintptr_t>(vtable + vtableOffset);
        if (!Globals::IsExecutablePtr(fnAddress)) {
            out = 0.0f;
            return false;
        }

        const auto fn = reinterpret_cast<ObjectFloatVFunc>(fnAddress);
        out = fn(object);
        return IsSaneFloat(out, 0.0f, 10000.0f);
    }
    __except (1) {
        out = 0.0f;
        return false;
    }
}

inline bool TryCallObjectFloatFunction(uintptr_t object, uintptr_t rva, float& out) {
    if (!Globals::IsValidPtr(object)) {
        out = 0.0f;
        return false;
    }

    __try {
        using ObjectFloatFn = float(__fastcall*)(uintptr_t);
        const auto fn = RuntimeFunction<ObjectFloatFn>(rva);
        if (!fn) {
            out = 0.0f;
            return false;
        }

        out = fn(object);
        return IsSaneFloat(out, 0.0f, 10000.0f);
    }
    __except (1) {
        out = 0.0f;
        return false;
    }
}

inline std::uint32_t ReadIndex(uintptr_t object) {
    return ReadField<std::uint32_t>(object, Offset::All::Index);
}

inline std::uint32_t ReadNetworkId(uintptr_t object) {
    return ReadField<std::uint32_t>(object, Offset::All::NetId);
}

inline std::uint8_t ReadTeam(uintptr_t object) {
    return ReadField<std::uint8_t>(object, Offset::All::Team);
}

inline std::uint32_t ReadTeamValue(uintptr_t object) {
    const std::uint8_t slot = ReadTeam(object);
    if (!Globals::base) {
        (void)Globals::Init();
    }

    const uintptr_t teamTable = Globals::Read<uintptr_t>(Globals::base + Offset::GameObjectsRuntime::TeamTable);
    if (!Globals::IsValidPtr(teamTable)) {
        return static_cast<std::uint32_t>(slot);
    }

    const std::uint32_t value = Globals::Read<std::uint32_t>(
        teamTable + 0x20 + static_cast<uintptr_t>(slot) * sizeof(std::uint32_t));
    return value != 0 ? value : static_cast<std::uint32_t>(slot);
}

inline Vec3 ReadPosition(uintptr_t object) {
    return ReadField<Vec3>(object, Offset::All::Position);
}

inline float ReadBoundingRadius(uintptr_t object) {
    const float fieldRadius = ReadField<float>(object, Offset::All::Radius);
    if (IsSaneFloat(fieldRadius, 0.0f, 10000.0f)) {
        return fieldRadius;
    }

    float radius = 0.0f;
#if NIGHTSHARP_ENABLE_OBJECT_VFUNC_READS
    if (TryCallObjectFloatVFunc(object, Offset::VTable::GameObjectBoundingRadius, radius)) {
        return radius;
    }
#endif

    if (TryCallObjectFloatFunction(object, Offset::ControlRuntime::GetBoundingRadius, radius)) {
        return radius;
    }

    return 0.0f;
}

inline Vec3 ReadDirection(uintptr_t object) {
    if (!Globals::IsValidPtr(object)) {
        return {};
    }

#if !NIGHTSHARP_ENABLE_OBJECT_VFUNC_READS
    return {};
#else
    __try {
        const uintptr_t component = object + Offset::All::DirectionComponent;
        const uintptr_t vtable = Globals::Read<uintptr_t>(component);
        if (!Globals::IsValidPtr(vtable)) {
            return {};
        }

        using GetDirectionHolderFn = uintptr_t(__fastcall*)(uintptr_t);
        const uintptr_t getHolderAddress =
            Globals::Read<uintptr_t>(vtable + Offset::All::DirectionVFunc);
        if (!Globals::IsExecutablePtr(getHolderAddress)) {
            return {};
        }

        const auto getHolder = reinterpret_cast<GetDirectionHolderFn>(getHolderAddress);
        const uintptr_t holder = getHolder(component);
        if (!Globals::IsValidPtr(holder)) {
            return {};
        }

        const Vec3 direction = Globals::Read<Vec3>(holder + Offset::All::DirectionVector);
        return direction.IsValid() ? direction : Vec3{};
    }
    __except (1) {
        return {};
    }
#endif
}

inline bool ReadNameField(uintptr_t object, uintptr_t offset, char* out, int outCount) {
    if (!out || outCount <= 0) {
        return false;
    }

    out[0] = 0;
    if (!Globals::IsValidPtr(object)) {
        return false;
    }
    return Globals::ReadRuntimeStringField(object + offset, out, outCount);
}

inline bool ReadName(uintptr_t object, char* out, int outCount) {
    return ReadNameField(object, Offset::All::Name, out, outCount);
}

inline bool ReadCharacterName(uintptr_t object, char* out, int outCount) {
    return ReadNameField(object, Offset::All::CharacterName, out, outCount);
}

inline MinionClass ReadMinionClass(uintptr_t object) {
    return static_cast<MinionClass>(
        ReadField<std::uint8_t>(object, Offset::MinionClassRuntime::TypeOffset));
}

inline JungleRuntimeType ReadJungleRuntimeType(uintptr_t object) {
    return static_cast<JungleRuntimeType>(
        ReadField<std::uint8_t>(object, Offset::JungleTypeRuntime::TypeOffset));
}

inline bool IsHero(uintptr_t object) {
    bool result = false;
    return TryCallObjectPredicate(object, Offset::ClassificationRuntime::IsHero, result) && result;
}

inline bool IsMinion(uintptr_t object) {
    bool result = false;
    return TryCallObjectPredicate(object, Offset::ClassificationRuntime::IsMinion, result) && result;
}

inline bool IsTurret(uintptr_t object) {
    bool result = false;
    return TryCallObjectPredicate(object, Offset::ClassificationRuntime::IsTurret, result) && result;
}

inline bool IsBarracksDampener(uintptr_t object) {
    bool result = false;
    return TryCallObjectPredicate(object, Offset::ClassificationRuntime::IsBarracksDampener, result) && result;
}

inline bool IsHQ(uintptr_t object) {
    bool result = false;
    return TryCallObjectPredicate(object, Offset::ClassificationRuntime::IsHQ, result) && result;
}

inline bool IsShop(uintptr_t object) {
    bool result = false;
    return TryCallObjectPredicate(object, Offset::ClassificationRuntime::IsShop, result) && result;
}

inline bool IsBuilding(uintptr_t object) {
    bool result = false;
    return TryCallObjectPredicate(object, Offset::ClassificationRuntime::IsBuilding, result) && result;
}

inline bool IsDead(uintptr_t object) {
    bool result = false;
    if (TryCallObjectPredicate(object, Offset::ClassificationRuntime::IsDead, result)) {
        return result;
    }
    return ReadBoolByte(object, Offset::All::Dead);
}

inline bool IsJungleMonster(uintptr_t object) {
    bool result = false;
    if (TryCallObjectPredicate(object, Offset::ClassificationRuntime::IsJungleMonster, result)) {
        return result;
    }

    return ReadMinionClass(object) == MinionClass::JungleMonster;
}

inline bool IsLaneMinion(uintptr_t object) {
    bool result = false;
    if (TryCallObjectPredicate(object, Offset::ClassificationRuntime::IsLaneMinion, result)) {
        return result;
    }

    const auto minionClass = ReadMinionClass(object);
    return minionClass == MinionClass::TeamMinion ||
           minionClass == MinionClass::MeleeLaneMinion ||
           minionClass == MinionClass::RangedLaneMinion ||
           minionClass == MinionClass::SiegeLaneMinion ||
           minionClass == MinionClass::SuperLaneMinion;
}

inline int ReadNativeJungleType(uintptr_t object) {
    if (!Globals::IsValidPtr(object)) {
        return 0;
    }

    // ClassificationRuntime::GetJungleType is tracked for signature parity,
    // but the current RVA is a handler offset, not a verified int(Object*)
    // callable getter. Use the replicated jungle-type byte until that calling
    // convention is proven.
    return static_cast<int>(ReadJungleRuntimeType(object));
}

inline ObjectHandle MakeHandle(uintptr_t object, ObjectType type = ObjectType::Unknown) {
    ObjectHandle handle{};
    handle.address = object;
    handle.type = type;
    if (!Globals::IsValidPtr(object)) {
        return handle;
    }

    handle.index = ReadIndex(object);
    handle.networkId = ReadNetworkId(object);
    return handle;
}

inline bool SameIdentity(const ObjectHandle& lhs, const ObjectHandle& rhs) {
    if (lhs.HasIdentity() && rhs.HasIdentity()) {
        return lhs.networkId == rhs.networkId;
    }
    return lhs.index != 0 && lhs.index == rhs.index;
}

inline bool SameObject(uintptr_t lhs, uintptr_t rhs) {
    if (lhs == rhs) {
        return Globals::IsValidPtr(lhs);
    }
    return SameIdentity(MakeHandle(lhs), MakeHandle(rhs));
}

inline void ReadBase(ObjectSnapshot& out, uintptr_t object) {
    out.teamSlot = ReadTeam(object);
    out.team = ReadTeamValue(object);
    out.isDead = IsDead(object);
    out.isVisible = ReadBoolByte(object, Offset::All::Visible);
    out.isInvulnerable = ReadBoolByte(object, Offset::All::IsInvulnerable);
    out.position = ReadPosition(object);
    out.direction = ReadDirection(object);
    out.boundingRadius = ReadBoundingRadius(object);
    ReadName(object, out.name, static_cast<int>(sizeof(out.name)));
    ReadCharacterName(object, out.characterName, static_cast<int>(sizeof(out.characterName)));
}

inline void ReadAttackable(ObjectSnapshot& out, uintptr_t object) {
    const auto attackable = CoreAttackableUnit::Read(object);
    out.health = attackable.health;
    out.maxHealth = attackable.maxHealth;
    out.allShield = attackable.allShield;
    out.physicalShield = attackable.physicalShield;
    out.magicalShield = attackable.magicalShield;
    out.isTargetable = attackable.isTargetable;
}

inline void ReadAIBase(ObjectSnapshot& out, uintptr_t object) {
    // CoreAIHeroClient::Read() re-reads the 15 attackable fields via
    // CoreAttackableUnit::Read(). ReadAttackable() already populated those in
    // `out`, so use ReadFromAttackable() which skips that duplicate read.
    const auto hero = CoreAIHeroClient::ReadFromAttackable(
        CoreAIHeroClient::Snapshot{}, object);
    out.mana = hero.mana;
    out.maxMana = hero.maxMana;
    out.moveSpeed = hero.moveSpeed;
    out.attackRange = hero.attackRange;
    out.baseAttackDamage = hero.baseAttackDamage;
    out.totalAttackDamage = hero.TotalAttackDamage();
    out.abilityPower = hero.baseAbilityDamage;
    out.armor = hero.armor;
    out.spellBlock = hero.spellBlock;
    out.attackSpeedMod = hero.attackSpeedMod;
    out.level = hero.level;
}

inline void ReadMinion(ObjectSnapshot& out, uintptr_t object) {
    out.minionClass = static_cast<int>(ReadMinionClass(object));
    out.jungleType = ReadNativeJungleType(object);
}

inline void ReadMissile(ObjectSnapshot& out, uintptr_t object) {
    out.missile.spellData = ReadField<uintptr_t>(object, Offset::MissileClient::SpellDataPtr);
    out.missile.castInfo = object + Offset::MissileClient::CastInfoBase;
    out.missile.casterIndex = ReadField<std::uint32_t>(object, Offset::MissileClient::CasterIndex);
    out.missile.targetIndex = ReadField<std::uint32_t>(object, Offset::MissileClient::TargetIndex);
    out.missile.casterNetworkId = out.missile.casterIndex;
    out.missile.targetNetworkId = out.missile.targetIndex;
    out.missile.missileNetworkId = ReadField<std::uint32_t>(object, Offset::MissileClient::MissileNetId);
    const std::uint32_t objectNetworkId = ReadField<std::uint32_t>(object, Offset::MissileClient::ObjectNetId);
    if (objectNetworkId != 0 && objectNetworkId != 0xFFFFFFFFu) {
        out.missile.missileNetworkId = objectNetworkId;
    }
    out.missile.startPosition = ReadField<Vec3>(object, Offset::MissileClient::StartPos);
    out.missile.endPosition = ReadField<Vec3>(object, Offset::MissileClient::EndPos);
    out.missile.castEndPosition = ReadField<Vec3>(object, Offset::MissileClient::CastEndPos);
    Globals::ReadRuntimeStringField(
        object + Offset::MissileClient::SpellName,
        out.missile.spellName,
        static_cast<int>(sizeof(out.missile.spellName)));
    Globals::ReadRuntimeStringField(
        object + Offset::MissileClient::MissileName,
        out.missile.missileName,
        static_cast<int>(sizeof(out.missile.missileName)));
}

namespace detail {
    // Separate function to avoid C2712 (MSVC cannot mix __try with C++ objects
    // requiring unwinding — the NS_PROFILE SectionProbe in ReadSnapshot conflicts).
    inline void ReadSnapshotBulk(ObjectSnapshot& snapshot, uintptr_t object, ObjectType type) {
        __try {
            snapshot.handle.index = *reinterpret_cast<const std::uint32_t*>(object + Offset::All::Index);
            snapshot.handle.networkId = *reinterpret_cast<const std::uint32_t*>(object + Offset::All::NetId);

            const std::uint8_t teamSlot = *reinterpret_cast<const std::uint8_t*>(object + Offset::All::Team);
            snapshot.teamSlot = teamSlot;

            if (Globals::base) {
                const uintptr_t teamTable = *reinterpret_cast<const uintptr_t*>(Globals::base + Offset::GameObjectsRuntime::TeamTable);
                if (Globals::IsValidPtr(teamTable)) {
                    snapshot.team = *reinterpret_cast<const std::uint32_t*>(
                        teamTable + 0x20 + static_cast<uintptr_t>(teamSlot) * sizeof(std::uint32_t));
                } else {
                    snapshot.team = teamSlot;
                }
            } else {
                snapshot.team = teamSlot;
            }
            if (snapshot.team == 0) snapshot.team = teamSlot;

            snapshot.isDead = IsDead(object);

            snapshot.isVisible = (*reinterpret_cast<const std::uint8_t*>(object + Offset::All::Visible)) != 0;
            snapshot.isInvulnerable = (*reinterpret_cast<const std::uint8_t*>(object + Offset::All::IsInvulnerable)) != 0;
            snapshot.position = *reinterpret_cast<const Vec3*>(object + Offset::All::Position);
            snapshot.direction = Vec3{};

            const float rawRadius = *reinterpret_cast<const float*>(object + Offset::All::Radius);
            if (rawRadius == rawRadius && rawRadius >= 0.0f && rawRadius <= 10000.0f) {
                snapshot.boundingRadius = rawRadius;
            } else {
                float r = 0.0f;
                if (TryCallObjectFloatFunction(object, Offset::ControlRuntime::GetBoundingRadius, r))
                    snapshot.boundingRadius = r;
            }

            if (IsAttackable(type)) {
                snapshot.health = *reinterpret_cast<const float*>(object + Offset::AttackableUnit::HP);
                snapshot.maxHealth = *reinterpret_cast<const float*>(object + Offset::AttackableUnit::MaxHP);
                snapshot.allShield = *reinterpret_cast<const float*>(object + Offset::AttackableUnit::AllShield);
                snapshot.physicalShield = *reinterpret_cast<const float*>(object + Offset::AttackableUnit::PhysicalShield);
                snapshot.magicalShield = *reinterpret_cast<const float*>(object + Offset::AttackableUnit::MagicalShield);
                snapshot.isTargetable = (*reinterpret_cast<const std::uint8_t*>(object + Offset::AttackableUnit::IsTargetable)) != 0;
            }

            if (IsAIBase(type)) {
                snapshot.mana = *reinterpret_cast<const float*>(object + Offset::AIHeroClient::MP);
                snapshot.maxMana = *reinterpret_cast<const float*>(object + Offset::AIHeroClient::MaxMP);
                snapshot.moveSpeed = *reinterpret_cast<const float*>(object + Offset::AIHeroClient::MoveSpeed);
                snapshot.attackRange = *reinterpret_cast<const float*>(object + Offset::AIHeroClient::AttackRange);
                snapshot.baseAttackDamage = *reinterpret_cast<const float*>(object + Offset::AIHeroClient::BaseAttackDamage);
                snapshot.totalAttackDamage = snapshot.baseAttackDamage + *reinterpret_cast<const float*>(object + Offset::AIHeroClient::FlatPhysicalDmgMod);
                snapshot.abilityPower = *reinterpret_cast<const float*>(object + Offset::AIHeroClient::BaseAbilityDamage);
                snapshot.armor = *reinterpret_cast<const float*>(object + Offset::AIHeroClient::Armor);
                snapshot.spellBlock = *reinterpret_cast<const float*>(object + Offset::AIHeroClient::SpellBlock);
                snapshot.attackSpeedMod = *reinterpret_cast<const float*>(object + Offset::AIHeroClient::AttackSpeedMod);
                snapshot.level = *reinterpret_cast<const int*>(object + Offset::AIHeroClient::LevelRef);
            }

            if (type == ObjectType::AIMinionClient || type == ObjectType::NeutralMinionCampClient) {
                snapshot.minionClass = static_cast<int>(*reinterpret_cast<const std::uint8_t*>(
                    object + Offset::MinionClassRuntime::TypeOffset));
                snapshot.jungleType = static_cast<int>(*reinterpret_cast<const std::uint8_t*>(
                    object + Offset::JungleTypeRuntime::TypeOffset));
            }

            if (IsMissile(type)) {
                ReadMissile(snapshot, object);
            }
        }
        __except (1) {
        }
    }
}

inline ObjectSnapshot ReadSnapshot(uintptr_t object, ObjectType type = ObjectType::Unknown) {
    NS_PROFILE("core.ReadSnapshot");
    ObjectSnapshot snapshot{};
    if (!Globals::IsValidPtr(object)) return snapshot;

    snapshot.handle.address = object;
    snapshot.handle.type = type;

    detail::ReadSnapshotBulk(snapshot, object, type);

    if (Globals::IsValidPtr(object)) {
        ReadName(object, snapshot.name, static_cast<int>(sizeof(snapshot.name)));
        ReadCharacterName(object, snapshot.characterName, static_cast<int>(sizeof(snapshot.characterName)));
    }

    return snapshot;
}

} // namespace Core::Objects
