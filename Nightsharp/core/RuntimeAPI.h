#pragma once
// ============================================================================
// RuntimeAPI.h — Native Game Engine API Wrappers
//
// Uses offsets from Offsets.h to call native LoL functions directly.
// This replaces string-based classification with bitmask flags which is
// faster, more accurate, and resolves plant/monster confusion.
//
// Usage: #include "RuntimeAPI.h" after "Offsets.h"
//        No Init() needed — uses Globals::base automatically.
//
// ALL FLAGS VERIFIED BY IDA DISASM 2026-03-17:
//   IsObjectAI = 0x400, Minion = 0x800, Hero = 0x1000,
//   Turret = 0x2000, Plant = 0x8000
//
// CALLING CONVENTION (IDA VERIFIED):
//   CompareTypeFlags = sub_29CD30(obj, 0, flag, obj) — 4 ARGS!
//
// NOTE ON __try / C2712:
//   MSVC forbids __try in functions that contain C++11 static locals
//   or C++ objects with destructors. ALL function pointers are cached
//   at namespace scope (NOT as static locals inside __try blocks).
// ============================================================================

#include "Offsets.h"
#include "Globals.h"
#include <cstdint>
#include <cstring>

// ================================================================
// MinionClass byte offset (IDA VERIFIED 2026-03-17)
// ================================================================
constexpr uintptr_t MinionClassOffset = 0x4C79;

// MinionClass enum values
enum MinionClassValue : uint8_t {
    MC_Unset            = 0,
    MC_Pet              = 1,
    MC_JungleMonster    = 2,
    MC_TeamMinion       = 3,
    MC_MeleeLaneMinion  = 4,
    MC_RangedLaneMinion = 5,
    MC_SiegeLaneMinion  = 6,
    MC_SuperLaneMinion  = 7
};

namespace RuntimeAPI {

    // ================================================================
    // Function pointer typedefs
    // ================================================================
    typedef bool(__fastcall* fnCompareTypeFlags4)(uintptr_t, int, int, uintptr_t);
    typedef bool(__fastcall* fnIsAlive)(uintptr_t);
    typedef bool(__fastcall* fnIsBuilding)(uintptr_t);
    typedef int (__fastcall* fnGetJungleType)(uintptr_t);
    typedef bool(__fastcall* fnIsClone)(uintptr_t);
    typedef bool(__fastcall* fnIsJungleMonsterNative)(uintptr_t);
    typedef bool(__fastcall* fnIsDragon)(uintptr_t);
    typedef bool(__fastcall* fnIsBaron)(uintptr_t);

    // ================================================================
    // Module base — syncs automatically from Globals::base
    // ================================================================
    inline uintptr_t GetBase() {
        return Globals::base;
    }

    // ================================================================
    // Cached function pointer resolver
    // Returns the absolute address for a given RVA offset.
    // NOT a static local — avoids C2712 entirely.
    // ================================================================
    inline uintptr_t ResolveFunc(uintptr_t rva) {
        uintptr_t b = GetBase();
        if (!b) return 0;
        return b + rva;
    }

    // ================================================================
    // Core: CompareTypeFlags
    // IDA VERIFIED calling convention:
    //   sub_29CD30(uintptr_t obj, int zero, int flag, uintptr_t obj)
    //   4 parameters! NOT 2!
    // ================================================================

    // SEH wrapper — NO static locals, NO C++ objects
    __declspec(noinline) inline bool CompareTypeFlags(uintptr_t objAddr, int flag) {
        if (!objAddr || !GetBase()) return false;
        uintptr_t fn = ResolveFunc(Offset::Function::CompareTypeFlags);
        if (!fn) return false;
        __try {
            return ((fnCompareTypeFlags4)fn)(objAddr, 0, flag, objAddr);
        }
        __except (1) { return false; }
    }

    // ================================================================
    // PRIMARY CLASSIFICATION — with fallbacks
    // ================================================================

    // Is this a Hero (Champion)? [VERIFIED: 0x1000]
    inline bool IsHero(uintptr_t obj) {
        return CompareTypeFlags(obj, 0x1000);
    }

    // Is this a Turret? [VERIFIED: 0x2000]
    inline bool IsTurret(uintptr_t obj) {
        return CompareTypeFlags(obj, 0x2000);
    }

    // Is this any AI unit? [VERIFIED: 0x400]
    inline bool IsObjectAI(uintptr_t obj) {
        return CompareTypeFlags(obj, 0x400);
    }

    // Is this a Plant? [VERIFIED: 0x8000 + name fallback]
    __declspec(noinline) inline bool IsPlant(uintptr_t obj) {
        if (!obj) return false;
        // Primary: native bitmask
        if (CompareTypeFlags(obj, 0x8000)) return true;
        // Fallback: team Neutral + name check
        __try {
            int team = *(int*)(obj + Offset::GameObject::Team);
            if (team != 300) return false;
            const char* name = (const char*)(obj + Offset::GameObject::Name);
            if (!name || name[0] == '\0') return false;
            if (strncmp(name, "SRU_Plant", 9) == 0) return true;
            float maxHP = *(float*)(obj + Offset::Health::MaxHP);
            if (maxHP > 0.0f && maxHP <= 6.0f && strstr(name, "Plant") != nullptr) return true;
        }
        __except (1) {}
        return false;
    }

    // Is this any kind of Minion? [VERIFIED: 0x800 + HP/Team fallback]
    __declspec(noinline) inline bool IsMinion(uintptr_t obj) {
        if (!obj) return false;
        // Primary: native bitmask
        if (CompareTypeFlags(obj, 0x800)) return true;
        // Fallback: HP/Team heuristic
        __try {
            int team = *(int*)(obj + Offset::GameObject::Team);
            if (team != 100 && team != 200) return false;
            float maxHP = *(float*)(obj + Offset::Health::MaxHP);
            if (maxHP <= 0.0f || maxHP >= 10000.0f) return false;
            if (IsHero(obj)) return false;
            if (IsTurret(obj)) return false;
            return true;
        }
        __except (1) {}
        return false;
    }

    // ================================================================
    // SECONDARY CLASSIFICATION (minion subtypes via MinionClass byte)
    // ================================================================

    // Get raw MinionClass value
    __declspec(noinline) inline uint8_t GetMinionClass(uintptr_t obj) {
        if (!obj) return 0;
        __try { return *(uint8_t*)(obj + MinionClassOffset); }
        __except (1) { return 0; }
    }

    // Is this a Lane Minion? (Melee=4, Ranged=5, Siege=6, Super=7)
    __declspec(noinline) inline bool IsLaneMinion(uintptr_t obj) {
        if (!obj) return false;
        uint8_t mc = GetMinionClass(obj);
        if (mc >= MC_MeleeLaneMinion && mc <= MC_SuperLaneMinion) return true;
        // Fallback: if IsMinion and team is Blue/Red
        if (!IsMinion(obj)) return false;
        __try {
            int team = *(int*)(obj + Offset::GameObject::Team);
            return team == 100 || team == 200;
        }
        __except (1) {}
        return false;
    }

    // Is this a Pet? (Tibbers, Daisy, Voidlings, etc.) — byte == 1
    inline bool IsPet(uintptr_t obj) {
        if (!obj) return false;
        return GetMinionClass(obj) == MC_Pet;
    }

    // Helper: call native IsJungleMonster (NO static locals!)
    __declspec(noinline) inline bool CallNativeIsJungleMonster(uintptr_t obj) {
        uintptr_t fn = ResolveFunc(Offset::Function::IsJungleMonster);
        if (!fn) return false;
        __try {
            return ((fnIsJungleMonsterNative)fn)(obj);
        }
        __except (1) { return false; }
    }

    // Is this a Jungle Monster? (byte == 2, exclude plants, + native fallback)
    inline bool IsJungleMonster(uintptr_t obj) {
        if (!obj) return false;
        if (IsPlant(obj)) return false;
        uint8_t mc = GetMinionClass(obj);
        if (mc == MC_JungleMonster) return true;
        return CallNativeIsJungleMonster(obj);
    }

    // ================================================================
    // NATIVE FUNCTIONS (direct game engine calls)
    // ALL use ResolveFunc() instead of static locals to avoid C2712!
    // ================================================================

    // Is alive?
    __declspec(noinline) inline bool IsAlive(uintptr_t obj) {
        if (!obj) return false;
        uintptr_t fn = ResolveFunc(Offset::Function::IsAlive);
        if (!fn) return false;
        __try {
            return ((fnIsAlive)fn)(obj);
        }
        __except (1) { return false; }
    }

    // Is this a Structure? (Turret, Inhibitor, Nexus)
    __declspec(noinline) inline bool IsStructure(uintptr_t obj) {
        if (!obj) return false;
        uintptr_t fn = ResolveFunc(Offset::Function::IsBuilding);
        if (!fn) return false;
        __try {
            return ((fnIsBuilding)fn)(obj);
        }
        __except (1) { return false; }
    }

    // Is this a Clone? (Shaco, LeBlanc, Neeko, etc.)
    __declspec(noinline) inline bool IsClone(uintptr_t obj) {
        if (!obj) return false;
        uintptr_t fn = ResolveFunc(Offset::Extra::IsClone);
        if (!fn) return false;
        __try {
            return ((fnIsClone)fn)(obj);
        }
        __except (1) { return false; }
    }

    // Is Dragon?
    __declspec(noinline) inline bool IsDragon(uintptr_t obj) {
        if (!obj) return false;
        uintptr_t fn = ResolveFunc(Offset::Function::IsDragon);
        if (!fn) return false;
        __try {
            return ((fnIsDragon)fn)(obj);
        }
        __except (1) { return false; }
    }

    // Is Baron?
    __declspec(noinline) inline bool IsBaron(uintptr_t obj) {
        if (!obj) return false;
        uintptr_t fn = ResolveFunc(Offset::Function::IsBaron);
        if (!fn) return false;
        __try {
            return ((fnIsBaron)fn)(obj);
        }
        __except (1) { return false; }
    }

    // Get jungle type: 0=Normal, 1=Baron, 2=Dragon
    __declspec(noinline) inline int GetJungleType(uintptr_t obj) {
        if (!obj) return -1;
        uintptr_t fn = ResolveFunc(Offset::Function::GetJungleType);
        if (!fn) return -1;
        __try {
            return ((fnGetJungleType)fn)(obj);
        }
        __except (1) { return -1; }
    }

    // ================================================================
    // FIELD ACCESSORS (direct memory read)
    // ================================================================

    // Get team ID (100 = blue, 200 = red, 300 = neutral)
    __declspec(noinline) inline int GetTeamID(uintptr_t obj) {
        if (!obj) return 0;
        __try { return *(int*)(obj + Offset::GameObject::Team); }
        __except (1) { return 0; }
    }

    // Get position
    struct Vec3f { float x, y, z; };
    __declspec(noinline) inline Vec3f GetPosition(uintptr_t obj) {
        if (!obj) return { 0, 0, 0 };
        __try { return *(Vec3f*)(obj + Offset::GameObject::Position); }
        __except (1) { return { 0, 0, 0 }; }
    }

    // Get network ID
    __declspec(noinline) inline int GetNetId(uintptr_t obj) {
        if (!obj) return 0;
        __try { return *(int*)(obj + Offset::GameObject::NetId); }
        __except (1) { return 0; }
    }

    // Get max health
    __declspec(noinline) inline float GetMaxHealth(uintptr_t obj) {
        if (!obj) return 0.0f;
        __try { return *(float*)(obj + Offset::Health::MaxHP); }
        __except (1) { return 0.0f; }
    }

    // Get health
    __declspec(noinline) inline float GetHealth(uintptr_t obj) {
        if (!obj) return 0.0f;
        __try { return *(float*)(obj + Offset::Health::HP); }
        __except (1) { return 0.0f; }
    }

    // ================================================================
    // DRAGON SUBTYPES (name-based)
    // ================================================================

    __declspec(noinline) inline bool IsElderDragon(uintptr_t obj) {
        if (!obj) return false;
        __try {
            const char* name = (const char*)(obj + Offset::GameObject::CharacterName);
            if (!name) return false;
            return (strcmp(name, "SRU_Dragon_Elder") == 0);
        }
        __except (1) { return false; }
    }

    __declspec(noinline) inline bool IsRiftHerald(uintptr_t obj) {
        if (!obj) return false;
        __try {
            const char* name = (const char*)(obj + Offset::GameObject::CharacterName);
            if (!name) return false;
            return (strncmp(name, "SRU_RiftHerald", 14) == 0);
        }
        __except (1) { return false; }
    }

    // ================================================================
    // ALLY / ENEMY helpers (no __try needed, delegate to GetTeamID)
    // ================================================================

    inline bool IsAlly(uintptr_t obj, uintptr_t localPlayer) {
        return GetTeamID(obj) == GetTeamID(localPlayer);
    }

    inline bool IsEnemy(uintptr_t obj, uintptr_t localPlayer) {
        int t = GetTeamID(obj);
        return t != GetTeamID(localPlayer) && t != 300;
    }

    inline bool IsNeutral(uintptr_t obj) {
        return GetTeamID(obj) == 300;
    }

    // ================================================================
    // MISSILE (for Evade) — FULLY IDA VERIFIED
    // ================================================================

    __declspec(noinline) inline bool IsMissile(uintptr_t obj) {
        if (!obj || !GetBase()) return false;
        __try {
            uintptr_t* vtable = *(uintptr_t**)obj;
            typedef uintptr_t(__fastcall* fnGetTypeID)(uintptr_t);
            fnGetTypeID getTypeID = (fnGetTypeID)(vtable[1]);
            typedef uintptr_t(*fnGetMissileType)();
            fnGetMissileType getMissileType = (fnGetMissileType)(GetBase() + 0x4264F0);
            return getTypeID(obj) == (uintptr_t)getMissileType();
        }
        __except (1) { return false; }
    }

    __declspec(noinline) inline Vec3f GetMissileStartPos(uintptr_t missile) {
        if (!missile) return { 0, 0, 0 };
        __try { return *(Vec3f*)(missile + Offset::Missile::StartPos); }
        __except (1) { return { 0, 0, 0 }; }
    }

    __declspec(noinline) inline Vec3f GetMissileEndPos(uintptr_t missile) {
        if (!missile) return { 0, 0, 0 };
        __try { return *(Vec3f*)(missile + Offset::Missile::EndPos); }
        __except (1) { return { 0, 0, 0 }; }
    }

    __declspec(noinline) inline Vec3f GetMissilePosition(uintptr_t missile) {
        if (!missile) return { 0, 0, 0 };
        __try { return *(Vec3f*)(missile + Offset::Missile::Position); }
        __except (1) { return { 0, 0, 0 }; }
    }

    __declspec(noinline) inline int GetMissileCasterNetId(uintptr_t missile) {
        if (!missile) return 0;
        __try { return *(int*)(missile + Offset::Missile::CasterNetId); }
        __except (1) { return 0; }
    }

    __declspec(noinline) inline int GetMissileTargetNetId(uintptr_t missile) {
        if (!missile) return 0;
        __try { return *(int*)(missile + Offset::Missile::TargetNetId); }
        __except (1) { return 0; }
    }

    __declspec(noinline) inline const char* GetMissileSpellName(uintptr_t missile) {
        if (!missile) return nullptr;
        __try {
            uintptr_t strAddr = missile + Offset::Missile::SpellName;
            size_t capacity = *(size_t*)(strAddr + 0x18);
            if (capacity >= 0x10) return *(const char**)strAddr;
            return (const char*)strAddr;
        }
        __except (1) { return nullptr; }
    }

    __declspec(noinline) inline float GetMissileSpeed(uintptr_t missile) {
        if (!missile) return 0.0f;
        __try {
            uintptr_t spellData = *(uintptr_t*)(missile + Offset::Missile::SpellDataPtr);
            if (!spellData) return 0.0f;
            uintptr_t spellDataRes = *(uintptr_t*)(spellData + Offset::SpellBook::DataResourceBase);
            if (!spellDataRes) return 0.0f;
            return *(float*)(spellDataRes + Offset::SpellBook::ResMissileSpeed);
        }
        __except (1) { return 0.0f; }
    }

    __declspec(noinline) inline float GetMissileWidth(uintptr_t missile) {
        if (!missile) return 0.0f;
        __try {
            uintptr_t spellData = *(uintptr_t*)(missile + Offset::Missile::SpellDataPtr);
            if (!spellData) return 0.0f;
            uintptr_t spellDataRes = *(uintptr_t*)(spellData + Offset::SpellBook::DataResourceBase);
            if (!spellDataRes) return 0.0f;
            return *(float*)(spellDataRes + Offset::SpellBook::ResLineWidth);
        }
        __except (1) { return 0.0f; }
    }

} // namespace RuntimeAPI
