// ============================================================================
// NightSharpAPI.h — SDK Function Table cho External Plugins
//
// Đây là "bridge" giữa NightSharp runtime và external plugin (.night).
// Plugin nhận pointer tới struct này khi load, dùng để gọi mọi SDK function.
//
// Plugin KHÔNG link trực tiếp vào NightSharp — tất cả gọi qua function pointers.
// Điều này cho phép plugin chạy như raw code (shellcode) trong memory space
// của NightSharp mà không tạo module mới.
//
// POD only — no virtual functions, no STL, fixed layout.
// ============================================================================

#pragma once

#include <cstdint>

// Forward declare — plugin không cần biết internal layout
// Plugin dùng opaque handles (void*)
typedef void* NsHandle;

// ============================================================================
// Math Types (must match SDK::Vector3 layout exactly)
// ============================================================================
struct NsVec3 {
    float x, y, z;
};

struct NsVec2 {
    float x, y;
};

// ============================================================================
// Enums (must match SDK internal enums)
// ============================================================================
enum NsSpellSlot : int {
    NS_SLOT_Q = 0,
    NS_SLOT_W = 1,
    NS_SLOT_E = 2,
    NS_SLOT_R = 3,
};

enum NsDamageType : int {
    NS_DMG_PHYSICAL = 0,
    NS_DMG_MAGICAL  = 1,
    NS_DMG_TRUE     = 2,
};

enum NsOrbwalkerMode : int {
    NS_ORB_NONE     = 0,
    NS_ORB_COMBO    = 1,
    NS_ORB_HARASS   = 2,
    NS_ORB_CLEAR    = 3,
    NS_ORB_LASTHIT  = 4,
    NS_ORB_FLEE     = 5,
};

enum NsHitChance : int {
    NS_HC_LOW     = 0,
    NS_HC_MEDIUM  = 1,
    NS_HC_HIGH    = 2,
    NS_HC_VERY_HIGH = 3,
    NS_HC_IMMOBILE = 4,
};

enum NsSpellType : int {
    NS_SPELL_LINE    = 0,
    NS_SPELL_CIRCLE  = 1,
    NS_SPELL_CONE    = 2,
};

// ============================================================================
// Plugin Event Types
// ============================================================================
enum NsPluginEventType : int {
    NS_EVT_ON_UPDATE = 0,
    NS_EVT_ON_RENDER = 1,
    NS_EVT_ON_LOAD   = 2,
    NS_EVT_ON_UNLOAD = 3,
};

// ============================================================================
// Prediction Result
// ============================================================================
struct NsPredictionResult {
    NsVec3  CastPosition;
    NsVec3  UnitPosition;
    int     Hitchance;      // NsHitChance
    int     CollisionCount; // number of objects in collision path
};

// ============================================================================
// Spell Info (read-only, returned by GetSpellInfo)
// ============================================================================
struct NsSpellInfo {
    int     Level;
    float   Cooldown;
    float   CooldownExpire;
    float   ManaCost;
    bool    IsReady;
};

// ============================================================================
// NightSharp SDK API Table — Version 1
//
// All function pointers are filled by NightSharp at runtime.
// Plugin receives a const pointer to this struct.
// ============================================================================
#define NIGHTSHARP_API_VERSION  1

struct NightSharpAPI {
    uint32_t Version;           // Must be NIGHTSHARP_API_VERSION
    uint32_t Reserved;

    // ======================================================================
    // Player Object
    // ======================================================================
    NsHandle (*GetPlayer)();
    bool     (*IsPlayerValid)();
    bool     (*IsPlayerDead)();
    bool     (*IsPlayerRecalling)();
    bool     (*IsPlayerWindingUp)();
    NsVec3   (*GetPlayerPosition)();
    float    (*GetPlayerHealth)();
    float    (*GetPlayerMaxHealth)();
    float    (*GetPlayerMana)();
    float    (*GetPlayerManaPercent)();
    float    (*GetPlayerAttackRange)();
    float    (*GetPlayerBoundingRadius)();
    float    (*GetPlayerBonusAD)();
    int      (*GetPlayerCharacterName)(char* out, int maxLen);

    // ======================================================================
    // Object Queries
    // ======================================================================
    int      (*GetEnemyHeroes)(NsHandle* out, int max);    // Returns count
    int      (*GetAllyHeroes)(NsHandle* out, int max);
    int      (*GetEnemyMinions)(NsHandle* out, int max);
    int      (*GetJungleMinions)(NsHandle* out, int max);

    // ======================================================================
    // Game Object Properties (operate on NsHandle)
    // ======================================================================
    bool     (*ObjIsValid)(NsHandle obj);
    bool     (*ObjIsDead)(NsHandle obj);
    bool     (*ObjIsMelee)(NsHandle obj);
    bool     (*ObjIsDashing)(NsHandle obj);
    bool     (*ObjIsMoving)(NsHandle obj);
    bool     (*ObjIsMe)(NsHandle obj);
    bool     (*ObjIsValidTarget)(NsHandle obj, float range);
    NsVec3   (*ObjGetPosition)(NsHandle obj);
    float    (*ObjGetHealth)(NsHandle obj);
    float    (*ObjGetMaxHealth)(NsHandle obj);
    float    (*ObjGetMana)(NsHandle obj);
    float    (*ObjGetAttackRange)(NsHandle obj);
    float    (*ObjGetBoundingRadius)(NsHandle obj);
    float    (*ObjGetHPRegenRate)(NsHandle obj);
    float    (*ObjGetAllShield)(NsHandle obj);
    float    (*ObjDistanceToPlayer)(NsHandle obj);
    float    (*ObjDistance)(NsHandle a, NsHandle b);
    int      (*ObjGetCharacterName)(NsHandle obj, char* out, int maxLen);
    bool     (*ObjHasBuff)(NsHandle obj, const char* buffName);
    int      (*ObjCountEnemiesInRange)(NsHandle obj, float range);
    bool     (*ObjInAutoAttackRange)(NsHandle obj, NsHandle target);

    // ======================================================================
    // Spell API
    // ======================================================================
    bool     (*SpellIsReady)(int slot);
    void     (*SpellGetInfo)(int slot, NsSpellInfo* out);
    void     (*SpellCast)(int slot, NsVec3 pos);
    void     (*SpellCastTarget)(int slot, NsHandle target);
    void     (*SpellSetSkillshot)(int slot, float delay, float width, float speed,
                                  bool collision, int spellType);
    bool     (*SpellGetPrediction)(int slot, NsHandle target,
                                   NsPredictionResult* out);
    void     (*SpellCastPredicted)(int slot, NsHandle target, int hitChance);
    float    (*SpellGetDamage)(int slot, NsHandle target);
    float    (*SpellGetHealthPrediction)(int slot, NsHandle target);

    // ======================================================================
    // Target Selector
    // ======================================================================
    NsHandle (*GetTarget)(float range, int damageType);

    // ======================================================================
    // Orbwalker
    // ======================================================================
    int      (*GetOrbwalkerMode)();
    void     (*IssueMove)(NsVec3 pos);

    // ======================================================================
    // Damage Calculator
    // ======================================================================
    float    (*CalcPhysicalDamage)(NsHandle source, NsHandle target, float rawDmg);
    float    (*CalcMagicalDamage)(NsHandle source, NsHandle target, float rawDmg);
    float    (*CalcAutoAttackDamage)(NsHandle source, NsHandle target);
    float    (*CalcSpellDamage)(NsHandle source, NsHandle target, int slot);

    // ======================================================================
    // Menu API
    // ======================================================================
    NsHandle (*MenuCreate)(const char* id, const char* displayName);
    NsHandle (*MenuAddSubMenu)(NsHandle menu, const char* id, const char* name);
    void     (*MenuAddBool)(NsHandle menu, const char* id, const char* name, bool def);
    void     (*MenuAddSlider)(NsHandle menu, const char* id, const char* name,
                              int defaultVal, int min, int max);
    void     (*MenuAddKeyBind)(NsHandle menu, const char* id, const char* name,
                               int key, bool toggle);
    bool     (*MenuGetBool)(NsHandle menu, const char* id);
    int      (*MenuGetSlider)(NsHandle menu, const char* id);
    bool     (*MenuGetKeyBind)(NsHandle menu, const char* id);
    void     (*MenuRemove)(const char* id);

    // ======================================================================
    // Game State
    // ======================================================================
    NsVec3   (*GetCursorPos)();
    float    (*GetGameTime)();
    bool     (*IsWall)(NsVec3 pos);

    // ======================================================================
    // Utility
    // ======================================================================
    float    (*Vec3Distance)(NsVec3 a, NsVec3 b);
    NsVec3   (*Vec3Normalized)(NsVec3 v);
    float    (*Vec3Length)(NsVec3 v);

    // ======================================================================
    // Logging (output to NightSharp console/debug)
    // ======================================================================
    void     (*LogInfo)(const char* msg);
    void     (*LogWarning)(const char* msg);
    void     (*LogError)(const char* msg);
};
