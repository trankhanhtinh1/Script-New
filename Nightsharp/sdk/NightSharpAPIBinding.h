// ============================================================================
// NightSharpAPIBinding.h — Binds NightSharpAPI function pointers to SDK internals
//
// Bridges the opaque function table (NightSharpAPI) to NightSharp's internal
// SDK classes (ObjectManager, TargetSelector, Spell, Menu, etc.)
//
// Include this file ONLY in NightSharp runtime (NightPackageLoader.h).
// Plugin code NEVER sees this — they only know about NightSharpAPI.h.
// ============================================================================

#pragma once

#include "External/NightSharpAPI.h"

#include "Core/Objects.h"
#include "Wrappers/TargetSelector/TargetSelector.h"
#include "Wrappers/Orbwalking/Orbwalker.h"
#include "Wrappers/Spells/Spell.h"
#include "../menu/MenuUI.h"
#include "../core/CoreAPI.h"
#include "../core/Globals.h"

#include <cstring>
#include <cfloat>

namespace NightSharpAPIBinding {

    // ════════════════════════════════════════════════════════════════════
    // Internal state for spell configs set by plugins
    // ════════════════════════════════════════════════════════════════════
    struct SpellConfig {
        float  Delay;
        float  Width;
        float  Speed;
        bool   Collision;
        int    Type;
        bool   Configured;
    };
    inline SpellConfig g_spellConfigs[4] = {};

    // ════════════════════════════════════════════════════════════════════
    // Player API
    // ════════════════════════════════════════════════════════════════════

    inline NsHandle API_GetPlayer() {
        uintptr_t addr = CoreAPI::Objects::GetLocalPlayer();
        return (NsHandle)addr;
    }

    inline bool API_IsPlayerValid() {
        return CoreAPI::Objects::GetLocalPlayer() != 0;
    }

    inline bool API_IsPlayerDead() {
        SDK::AIHeroClient p = SDK::ObjectManager::Player();
        return !p.IsValid() || p.IsDead();
    }

    inline bool API_IsPlayerRecalling() {
        return SDK::ObjectManager::Player().IsRecalling();
    }

    inline bool API_IsPlayerWindingUp() {
        return SDK::ObjectManager::Player().IsWindingUp();
    }

    inline NsVec3 API_GetPlayerPosition() {
        auto pos = SDK::ObjectManager::Player().Position();
        return { pos.x, pos.y, pos.z };
    }

    inline float API_GetPlayerHealth() {
        return SDK::ObjectManager::Player().Health();
    }

    inline float API_GetPlayerMaxHealth() {
        return SDK::ObjectManager::Player().MaxHealth();
    }

    inline float API_GetPlayerMana() {
        return SDK::ObjectManager::Player().Mana();
    }

    inline float API_GetPlayerManaPercent() {
        return SDK::ObjectManager::Player().ManaPercent();
    }

    inline float API_GetPlayerAttackRange() {
        return SDK::ObjectManager::Player().AttackRange();
    }

    inline float API_GetPlayerBoundingRadius() {
        return SDK::ObjectManager::Player().BoundingRadius();
    }

    inline float API_GetPlayerBonusAD() {
        return SDK::ObjectManager::Player().BonusAttackDamage();
    }

    inline int API_GetPlayerCharacterName(char* out, int maxLen) {
        if (!out || maxLen <= 0) return -1;
        auto name = SDK::ObjectManager::Player().CharacterName();
        strncpy_s(out, maxLen, name.c_str(), _TRUNCATE);
        return (int)name.length();
    }

    // ════════════════════════════════════════════════════════════════════
    // Object Queries
    // ════════════════════════════════════════════════════════════════════

    inline int API_GetEnemyHeroes(NsHandle* out, int max) {
        if (!out || max <= 0) return 0;
        uintptr_t buf[32] = {};
        int count = CoreAPI::Objects::EnumerateEnemyHeroes(buf, (std::min)(max, 32));
        for (int i = 0; i < count && i < max; i++)
            out[i] = (NsHandle)buf[i];
        return count;
    }

    inline int API_GetAllyHeroes(NsHandle* out, int max) {
        if (!out || max <= 0) return 0;
        uintptr_t buf[32] = {};
        int count = CoreAPI::Objects::EnumerateAllyHeroes(buf, (std::min)(max, 32));
        for (int i = 0; i < count && i < max; i++)
            out[i] = (NsHandle)buf[i];
        return count;
    }

    inline int API_GetEnemyMinions(NsHandle* out, int max) {
        if (!out || max <= 0) return 0;
        uintptr_t buf[128] = {};
        int count = CoreAPI::Objects::EnumerateEnemyMinions(buf, (std::min)(max, 128));
        for (int i = 0; i < count && i < max; i++)
            out[i] = (NsHandle)buf[i];
        return count;
    }

    inline int API_GetJungleMinions(NsHandle* out, int max) {
        if (!out || max <= 0) return 0;
        uintptr_t buf[64] = {};
        int count = CoreAPI::Objects::EnumerateJungleMinions(buf, (std::min)(max, 64));
        for (int i = 0; i < count && i < max; i++)
            out[i] = (NsHandle)buf[i];
        return count;
    }

    // ════════════════════════════════════════════════════════════════════
    // Game Object Properties
    // ════════════════════════════════════════════════════════════════════

    inline bool API_ObjIsValid(NsHandle obj) {
        return SDK::GameObject((uintptr_t)obj).IsValid();
    }
    inline bool API_ObjIsDead(NsHandle obj) {
        return SDK::GameObject((uintptr_t)obj).IsDead();
    }
    inline bool API_ObjIsMelee(NsHandle obj) {
        return SDK::GameObject((uintptr_t)obj).IsMelee();
    }
    inline bool API_ObjIsDashing(NsHandle obj) {
        return SDK::GameObject((uintptr_t)obj).IsDashing();
    }
    inline bool API_ObjIsMoving(NsHandle obj) {
        return SDK::GameObject((uintptr_t)obj).IsMoving();
    }
    inline bool API_ObjIsMe(NsHandle obj) {
        return SDK::GameObject((uintptr_t)obj).IsMe();
    }
    inline bool API_ObjIsValidTarget(NsHandle obj, float range) {
        return SDK::GameObject((uintptr_t)obj).IsValidTarget(range);
    }
    inline NsVec3 API_ObjGetPosition(NsHandle obj) {
        auto p = SDK::GameObject((uintptr_t)obj).Position();
        return { p.x, p.y, p.z };
    }
    inline float API_ObjGetHealth(NsHandle obj) {
        return SDK::GameObject((uintptr_t)obj).Health();
    }
    inline float API_ObjGetMaxHealth(NsHandle obj) {
        return SDK::GameObject((uintptr_t)obj).MaxHealth();
    }
    inline float API_ObjGetMana(NsHandle obj) {
        return SDK::GameObject((uintptr_t)obj).Mana();
    }
    inline float API_ObjGetAttackRange(NsHandle obj) {
        return SDK::GameObject((uintptr_t)obj).AttackRange();
    }
    inline float API_ObjGetBoundingRadius(NsHandle obj) {
        return SDK::GameObject((uintptr_t)obj).BoundingRadius();
    }
    inline float API_ObjGetHPRegenRate(NsHandle obj) {
        return SDK::GameObject((uintptr_t)obj).HPRegenRate();
    }
    inline float API_ObjGetAllShield(NsHandle obj) {
        return SDK::GameObject((uintptr_t)obj).AllShield();
    }
    inline float API_ObjDistanceToPlayer(NsHandle obj) {
        return SDK::AIBaseClient((uintptr_t)obj).DistanceToPlayer();
    }
    inline float API_ObjDistance(NsHandle a, NsHandle b) {
        return SDK::GameObject((uintptr_t)a).Distance(SDK::GameObject((uintptr_t)b));
    }
    inline int API_ObjGetCharacterName(NsHandle obj, char* out, int maxLen) {
        if (!out || maxLen <= 0) return -1;
        auto name = SDK::GameObject((uintptr_t)obj).CharacterName();
        strncpy_s(out, maxLen, name.c_str(), _TRUNCATE);
        return (int)name.length();
    }
    inline bool API_ObjHasBuff(NsHandle obj, const char* buffName) {
        return SDK::GameObject((uintptr_t)obj).HasBuff(buffName);
    }
    inline int API_ObjCountEnemiesInRange(NsHandle obj, float range) {
        return SDK::AIBaseClient((uintptr_t)obj).CountEnemyHeroesInRange(range);
    }
    inline bool API_ObjInAutoAttackRange(NsHandle obj, NsHandle target) {
        return SDK::AIBaseClient((uintptr_t)obj).InAutoAttackRange(SDK::GameObject((uintptr_t)target));
    }

    // ════════════════════════════════════════════════════════════════════
    // Spell API
    // ════════════════════════════════════════════════════════════════════

    inline bool API_SpellIsReady(int slot) {
        auto p = SDK::ObjectManager::Player();
        return p.GetSpellBook().GetSpell((SDK::SpellSlot)slot).IsReady();
    }

    inline void API_SpellGetInfo(int slot, NsSpellInfo* out) {
        if (!out) return;
        auto p = SDK::ObjectManager::Player();
        auto spell = p.GetSpellBook().GetSpell((SDK::SpellSlot)slot);
        out->Level = spell.Level();
        out->Cooldown = spell.Cooldown();
        out->CooldownExpire = spell.TotalCooldown();
        out->ManaCost = spell.ManaCost();
        out->IsReady = spell.IsReady();
    }

    inline void API_SpellCast(int slot, NsVec3 pos) {
        SDK::Vector3 p(pos.x, pos.y, pos.z);
        CoreAPI::Control::CastSpell(slot, p, p, 0);
    }

    inline void API_SpellCastTarget(int slot, NsHandle target) {
        auto obj = SDK::GameObject((uintptr_t)target);
        if (!obj.IsValid()) return;
        auto targetPos = obj.Position();
        CoreAPI::Control::CastSpell(slot, targetPos, targetPos, (uint32_t)obj.NetworkId());
    }

    inline void API_SpellSetSkillshot(int slot, float delay, float width, float speed,
                                       bool collision, int spellType) {
        if (slot < 0 || slot > 3) return;
        g_spellConfigs[slot] = { delay, width, speed, collision, spellType, true };
    }

    inline bool API_SpellGetPrediction(int slot, NsHandle target,
                                        NsPredictionResult* out) {
        if (!out || slot < 0 || slot > 3 || !g_spellConfigs[slot].Configured) return false;

        auto& cfg = g_spellConfigs[slot];
        SDK::Spell spell((SDK::SpellSlot)slot, 2000.0f);
        spell.SetSkillshot(cfg.Delay, cfg.Width, cfg.Speed, cfg.Collision,
            (SDK::SpellType)cfg.Type);

        auto pred = spell.GetPrediction(SDK::AIBaseClient((uintptr_t)target));
        out->CastPosition = { pred.CastPosition.x, pred.CastPosition.y, pred.CastPosition.z };
        out->UnitPosition = { pred.UnitPosition.x, pred.UnitPosition.y, pred.UnitPosition.z };
        out->Hitchance = (int)pred.Hitchance;
        out->CollisionCount = (int)pred.CollisionObjects.size();
        return true;
    }

    inline void API_SpellCastPredicted(int slot, NsHandle target, int hitChance) {
        NsPredictionResult pred = {};
        if (API_SpellGetPrediction(slot, target, &pred)) {
            if (pred.Hitchance >= hitChance) {
                API_SpellCast(slot, pred.CastPosition);
            }
        }
    }

    inline float API_SpellGetDamage(int slot, NsHandle target) {
        auto p = SDK::ObjectManager::Player();
        return p.GetSpellDamage(SDK::GameObject((uintptr_t)target), (SDK::SpellSlot)slot);
    }

    inline float API_SpellGetHealthPrediction(int slot, NsHandle target) {
        (void)slot;
        return SDK::GameObject((uintptr_t)target).Health();
    }

    // ════════════════════════════════════════════════════════════════════
    // Target Selector
    // ════════════════════════════════════════════════════════════════════

    inline NsHandle API_GetTarget(float range, int damageType) {
        auto target = SDK::TargetSelector::GetTarget(range, (SDK::DamageType)damageType);
        return (NsHandle)target.Address();
    }

    // ════════════════════════════════════════════════════════════════════
    // Orbwalker
    // ════════════════════════════════════════════════════════════════════

    inline int API_GetOrbwalkerMode() {
        return (int)SDK::Orbwalker::GetMode();
    }

    inline void API_IssueMove(NsVec3 pos) {
        CoreAPI::Control::IssueMove(SDK::Vector3(pos.x, pos.y, pos.z));
    }

    // ════════════════════════════════════════════════════════════════════
    // Damage Calculator
    // ════════════════════════════════════════════════════════════════════

    inline float API_CalcPhysicalDamage(NsHandle source, NsHandle target, float raw) {
        return SDK::AIBaseClient((uintptr_t)source)
            .CalculatePhysicalDamage(SDK::GameObject((uintptr_t)target), raw);
    }

    inline float API_CalcMagicalDamage(NsHandle source, NsHandle target, float raw) {
        return SDK::AIBaseClient((uintptr_t)source)
            .CalculateMagicDamage(SDK::GameObject((uintptr_t)target), raw);
    }

    inline float API_CalcAutoAttackDamage(NsHandle source, NsHandle target) {
        return SDK::AIBaseClient((uintptr_t)source)
            .GetAutoAttackDamage(SDK::GameObject((uintptr_t)target));
    }

    inline float API_CalcSpellDamage(NsHandle source, NsHandle target, int slot) {
        return SDK::AIBaseClient((uintptr_t)source)
            .GetSpellDamage(SDK::GameObject((uintptr_t)target), (SDK::SpellSlot)slot);
    }

    // ════════════════════════════════════════════════════════════════════
    // Menu API
    // Uses Menu::Create, Menu::AddSubMenu, Menu::Add<T>, Menu::Get<T>
    // ════════════════════════════════════════════════════════════════════

    inline NsHandle API_MenuCreate(const char* id, const char* displayName) {
        auto* menu = SDK::MenuUI::Menu::Create(id, displayName);
        return (NsHandle)menu;
    }

    inline NsHandle API_MenuAddSubMenu(NsHandle menu, const char* id, const char* name) {
        auto* m = (SDK::MenuUI::Menu*)menu;
        if (!m) return nullptr;
        return (NsHandle)m->AddSubMenu(id, name);
    }

    inline void API_MenuAddBool(NsHandle menu, const char* id, const char* name, bool def) {
        auto* m = (SDK::MenuUI::Menu*)menu;
        if (m) m->Add<SDK::MenuUI::MenuBool>(id, name, def);
    }

    inline void API_MenuAddSlider(NsHandle menu, const char* id, const char* name,
                                   int defaultVal, int min, int max) {
        auto* m = (SDK::MenuUI::Menu*)menu;
        if (m) m->Add<SDK::MenuUI::MenuSlider>(id, name, defaultVal, min, max);
    }

    inline void API_MenuAddKeyBind(NsHandle menu, const char* id, const char* name,
                                    int key, bool toggle) {
        auto* m = (SDK::MenuUI::Menu*)menu;
        if (m) m->Add<SDK::MenuUI::MenuKeyBind>(id, name, key,
            toggle ? SDK::MenuUI::KeyBindType::Toggle : SDK::MenuUI::KeyBindType::Press);
    }

    inline bool API_MenuGetBool(NsHandle menu, const char* id) {
        auto* m = (SDK::MenuUI::Menu*)menu;
        if (!m) return false;
        return m->GetBoolValue(id);
    }

    inline int API_MenuGetSlider(NsHandle menu, const char* id) {
        auto* m = (SDK::MenuUI::Menu*)menu;
        if (!m) return 0;
        return m->GetSliderValue(id);
    }

    inline bool API_MenuGetKeyBind(NsHandle menu, const char* id) {
        auto* m = (SDK::MenuUI::Menu*)menu;
        if (!m) return false;
        return m->GetKeyBindValue(id);
    }

    inline void API_MenuRemove(const char* id) {
        SDK::MenuUI::Menu::Remove(id);
    }

    // ════════════════════════════════════════════════════════════════════
    // Game State
    // ════════════════════════════════════════════════════════════════════

    inline NsVec3 API_GetCursorPos() {
        auto pos = CoreAPI::View::GetMouseWorldPos();
        return { pos.x, pos.y, pos.z };
    }

    inline float API_GetGameTime() {
        return CoreAPI::Game::GetTime();
    }

    inline bool API_IsWall(NsVec3 pos) {
        return CoreAPI::NavGrid::IsWall(SDK::Vector3(pos.x, pos.y, pos.z));
    }

    // ════════════════════════════════════════════════════════════════════
    // Utility
    // ════════════════════════════════════════════════════════════════════

    inline float API_Vec3Distance(NsVec3 a, NsVec3 b) {
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return sqrtf(dx*dx + dy*dy + dz*dz);
    }

    inline NsVec3 API_Vec3Normalized(NsVec3 v) {
        float len = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
        if (len < 0.0001f) return { 0, 0, 0 };
        return { v.x/len, v.y/len, v.z/len };
    }

    inline float API_Vec3Length(NsVec3 v) {
        return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    }

    // ════════════════════════════════════════════════════════════════════
    // Logging
    // ════════════════════════════════════════════════════════════════════

    inline void API_LogInfo(const char* msg) {
        OutputDebugStringA("[NightPlugin] ");
        OutputDebugStringA(msg ? msg : "");
        OutputDebugStringA("\n");
    }

    inline void API_LogWarning(const char* msg) {
        OutputDebugStringA("[NightPlugin WARN] ");
        OutputDebugStringA(msg ? msg : "");
        OutputDebugStringA("\n");
    }

    inline void API_LogError(const char* msg) {
        OutputDebugStringA("[NightPlugin ERROR] ");
        OutputDebugStringA(msg ? msg : "");
        OutputDebugStringA("\n");
    }

    // ════════════════════════════════════════════════════════════════════
    // Master Init — fills the entire NightSharpAPI struct
    // ════════════════════════════════════════════════════════════════════

    inline void Initialize(NightSharpAPI* api) {
        if (!api) return;
        memset(api, 0, sizeof(NightSharpAPI));
        api->Version = NIGHTSHARP_API_VERSION;

        // Player
        api->GetPlayer            = API_GetPlayer;
        api->IsPlayerValid        = API_IsPlayerValid;
        api->IsPlayerDead         = API_IsPlayerDead;
        api->IsPlayerRecalling    = API_IsPlayerRecalling;
        api->IsPlayerWindingUp    = API_IsPlayerWindingUp;
        api->GetPlayerPosition    = API_GetPlayerPosition;
        api->GetPlayerHealth      = API_GetPlayerHealth;
        api->GetPlayerMaxHealth   = API_GetPlayerMaxHealth;
        api->GetPlayerMana        = API_GetPlayerMana;
        api->GetPlayerManaPercent = API_GetPlayerManaPercent;
        api->GetPlayerAttackRange = API_GetPlayerAttackRange;
        api->GetPlayerBoundingRadius = API_GetPlayerBoundingRadius;
        api->GetPlayerBonusAD     = API_GetPlayerBonusAD;
        api->GetPlayerCharacterName = API_GetPlayerCharacterName;

        // Object queries
        api->GetEnemyHeroes       = API_GetEnemyHeroes;
        api->GetAllyHeroes        = API_GetAllyHeroes;
        api->GetEnemyMinions      = API_GetEnemyMinions;
        api->GetJungleMinions     = API_GetJungleMinions;

        // Object properties
        api->ObjIsValid           = API_ObjIsValid;
        api->ObjIsDead            = API_ObjIsDead;
        api->ObjIsMelee           = API_ObjIsMelee;
        api->ObjIsDashing         = API_ObjIsDashing;
        api->ObjIsMoving          = API_ObjIsMoving;
        api->ObjIsMe              = API_ObjIsMe;
        api->ObjIsValidTarget     = API_ObjIsValidTarget;
        api->ObjGetPosition       = API_ObjGetPosition;
        api->ObjGetHealth         = API_ObjGetHealth;
        api->ObjGetMaxHealth      = API_ObjGetMaxHealth;
        api->ObjGetMana           = API_ObjGetMana;
        api->ObjGetAttackRange    = API_ObjGetAttackRange;
        api->ObjGetBoundingRadius = API_ObjGetBoundingRadius;
        api->ObjGetHPRegenRate    = API_ObjGetHPRegenRate;
        api->ObjGetAllShield      = API_ObjGetAllShield;
        api->ObjDistanceToPlayer  = API_ObjDistanceToPlayer;
        api->ObjDistance          = API_ObjDistance;
        api->ObjGetCharacterName  = API_ObjGetCharacterName;
        api->ObjHasBuff           = API_ObjHasBuff;
        api->ObjCountEnemiesInRange = API_ObjCountEnemiesInRange;
        api->ObjInAutoAttackRange = API_ObjInAutoAttackRange;

        // Spell
        api->SpellIsReady         = API_SpellIsReady;
        api->SpellGetInfo         = API_SpellGetInfo;
        api->SpellCast            = API_SpellCast;
        api->SpellCastTarget      = API_SpellCastTarget;
        api->SpellSetSkillshot    = API_SpellSetSkillshot;
        api->SpellGetPrediction   = API_SpellGetPrediction;
        api->SpellCastPredicted   = API_SpellCastPredicted;
        api->SpellGetDamage       = API_SpellGetDamage;
        api->SpellGetHealthPrediction = API_SpellGetHealthPrediction;

        api->GetTarget            = API_GetTarget;
        api->GetOrbwalkerMode     = API_GetOrbwalkerMode;
        api->IssueMove            = API_IssueMove;

        // Damage
        api->CalcPhysicalDamage   = API_CalcPhysicalDamage;
        api->CalcMagicalDamage    = API_CalcMagicalDamage;
        api->CalcAutoAttackDamage = API_CalcAutoAttackDamage;
        api->CalcSpellDamage      = API_CalcSpellDamage;

        // Menu
        api->MenuCreate           = API_MenuCreate;
        api->MenuAddSubMenu       = API_MenuAddSubMenu;
        api->MenuAddBool          = API_MenuAddBool;
        api->MenuAddSlider        = API_MenuAddSlider;
        api->MenuAddKeyBind       = API_MenuAddKeyBind;
        api->MenuGetBool          = API_MenuGetBool;
        api->MenuGetSlider        = API_MenuGetSlider;
        api->MenuGetKeyBind       = API_MenuGetKeyBind;
        api->MenuRemove           = API_MenuRemove;

        // Game state
        api->GetCursorPos         = API_GetCursorPos;
        api->GetGameTime          = API_GetGameTime;
        api->IsWall               = API_IsWall;

        // Utility
        api->Vec3Distance         = API_Vec3Distance;
        api->Vec3Normalized       = API_Vec3Normalized;
        api->Vec3Length           = API_Vec3Length;

        // Logging
        api->LogInfo              = API_LogInfo;
        api->LogWarning           = API_LogWarning;
        api->LogError             = API_LogError;
    }

} // namespace NightSharpAPIBinding
