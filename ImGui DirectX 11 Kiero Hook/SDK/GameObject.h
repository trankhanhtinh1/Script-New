#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>

#include "Offsets.h"
#include "Spell.h"
#include "AiManager.h" // Add this here
#include "AiManagerScan.h" // For IDA::DecryptAiManager
#include "../Vector.h"

namespace SDK
{
    class GameObject
    {
    public:
        uint64_t Address;

        GameObject(uint64_t address) : Address(address) {}
        GameObject() : Address(0) {}

        bool IsValid() const {
            return Address != 0;
        }

        bool operator==(const GameObject& other) const {
            return Address == other.Address;
        }

        // Basic Properties
        float GetHealth() {
            __try { return *(float*)(Address + Offset::oHealth); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        float GetMaxHealth() {
            __try { return *(float*)(Address + Offset::oMaxHealth); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        bool IsDead() {
            __try {
                int dead = *(int*)(Address + Offset::oDead);
                return dead == 1 || GetHealth() <= 0.0f;
            } __except(EXCEPTION_EXECUTE_HANDLER) { return true; }
        }

        bool IsVisible() {
            __try { return *(bool*)(Address + Offset::oVisibility); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        bool IsTargetable() {
            __try { return *(bool*)(Address + Offset::oTargetable); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        int GetTeam() {
            __try { return (int)(*(unsigned char*)(Address + Offset::TeamID)); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
        }

        bool IsEnemyTo(GameObject* other) {
            if (!other || !other->IsValid()) return false;
            return GetTeam() != other->GetTeam();
        }

        // Network ID for missile target matching
        int GetNetworkId() {
            __try { return *(int*)(Address + Offset::oObjNetId); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
        }

        Vector3 GetPosition() {
            __try { return *(Vector3*)(Address + Offset::oObjPosition); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return Vector3(0, 0, 0); }
        }

        float GetAttackRange() {
            __try { return *(float*)(Address + Offset::RangeAttack); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        float GetBoundingRadius() {
            // Use function call like leagueoflegends-master for accurate bounding radius
            uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
            typedef float(__fastcall* fnGetBoundingRadius)(uint64_t obj);
            fnGetBoundingRadius getBoundingRadius = (fnGetBoundingRadius)(moduleBase + Offset::Function::oGetBoundingRadius);
            
            __try {
                float r = getBoundingRadius(Address);
                // Sanity check
                if (r < 0.0f || r > 500.0f || std::isnan(r)) return 65.0f;
                return r;
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                return 65.0f; // Default on exception
            }
        }
        
        // Following leagueoflegends-master pattern:
        // RealAttackRange = attackRange + myBoundingRadius
        float GetRealAttackRange() {
            return GetAttackRange() + GetBoundingRadius();
        }
        
        // Check if target is in range (following leagueoflegends-master)
        // Formula: myRealAttackRange + targetBoundingRadius >= distance
        bool IsInAttackRange(GameObject* target) {
            if (!target) return false;
            float realRange = GetRealAttackRange();
            float dist = GetPosition().Distance(target->GetPosition());
            return (realRange + target->GetBoundingRadius()) >= dist;
        }

        // Stats
        float GetBaseAttackDamage() {
            __try { return *(float*)(Address + Offset::DamageBase); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        float GetBonusAttackDamage() {
            __try { return *(float*)(Address + Offset::DamageBonus); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }
        
        float GetAttackDamage() {
            return GetBaseAttackDamage() + GetBonusAttackDamage();
        }

        // ============================================================================
        // Defensive Stats
        // ============================================================================
        float GetArmor() {
            __try { return *(float*)(Address + Offset::Armor); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        float GetMagicResist() {
            __try { return *(float*)(Address + Offset::MagicResist); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        float GetAbilityPower() {
            __try { return *(float*)(Address + Offset::AbilityPower); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        // ============================================================================
        // Penetration Stats
        // ============================================================================
        float GetArmorPenFlat() {
            __try { return *(float*)(Address + Offset::ArmorPenFlat); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        float GetArmorPenPercent() {
            __try { return *(float*)(Address + Offset::ArmorPenPercent); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        float GetMagicPenFlat() {
            __try { return *(float*)(Address + Offset::MagicPenFlat); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        float GetMagicPenPercent() {
            __try { return *(float*)(Address + Offset::MagicPenPercent); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        // ============================================================================
        // Critical Strike
        // ============================================================================
        float GetCritChance() {
            __try { return *(float*)(Address + Offset::CritChance); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        float GetCritDamage() {
            __try { return *(float*)(Address + Offset::CritDamage); }
            __except(EXCEPTION_EXECUTE_HANDLER) { return 1.75f; }
        }
        
        // ============================================================================
        // Effective Health (Simple calculation without penetration)
        // ============================================================================
        float GetEffectiveHealthAD() {
            return GetHealth() * (1.0f + GetArmor() / 100.0f);
        }

        float GetEffectiveHealthAP() {
            return GetHealth() * (1.0f + GetMagicResist() / 100.0f);
        }

        bool IsAlive() {
            return !IsDead();
        }

        float GetTotalAD() {
            __try {
                return *(float*)(Address + Offset::DamageBase) + *(float*)(Address + Offset::DamageBonus);
            } __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        // Simple AutoAttackDamage calculation (AD only for now)
        // TODO: Add item passives/crit logic if needed later
        float GetAutoAttackDamage(GameObject* target) {
            float rawDamage = GetTotalAD();
            // Basic armor reduction
            float armor = target->GetArmor();
            if (armor >= 0) {
                return rawDamage * (100.0f / (100.0f + armor));
            } else {
                return rawDamage * (2.0f - (100.0f / (100.0f - armor)));
            }
        }

        // Type Checks (Using Function pointers from Offsets if available, otherwise memory check)
        // Note: Function calls need to be carefully done in internal cheats to avoid crashes if conventions differ.
        // For now, let's use the function offsets provided as addresses to call? Or check flags?
        // References: inline constexpr uint64_t isMinion = 0x301120;
        
        // Type Checks
        bool IsMinion() {
            __try {
                typedef bool(__thiscall* Fn)(void*);
                static uint64_t funcAddr = Offset::Function::isMinion + (uint64_t)GetModuleHandle(NULL);
                Fn func = (Fn)(funcAddr);
                if (IsBadCodePtr((FARPROC)func)) return false;
                return func((void*)Address);
            } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        bool IsTurret() {
            __try {
                typedef bool(__thiscall* Fn)(void*);
                static uint64_t funcAddr = Offset::Function::isTurret + (uint64_t)GetModuleHandle(NULL);
                Fn func = (Fn)(funcAddr);
                if (IsBadCodePtr((FARPROC)func)) return false;
                return func((void*)Address);
            } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        float GetAttackDelay() {
            __try {
                typedef float(__cdecl* Fn)(GameObject*);
                static uint64_t funcAddr = Offset::Function::AttackDelay + (uint64_t)GetModuleHandle(NULL);
                Fn func = (Fn)(funcAddr);
                if (IsBadCodePtr((FARPROC)func)) return 0.0f;
                return func(this);
            } __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }

        float GetAttackWindup() {
            __try {
                typedef float(__cdecl* Fn)(GameObject*, int);
                static uint64_t funcAddr = Offset::Function::oGetAttackWindup + (uint64_t)GetModuleHandle(NULL);
                Fn func = (Fn)(funcAddr);
                if (IsBadCodePtr((FARPROC)func)) return 0.0f;
                return func(this, 0x40);
            } __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        }
        
        bool IsHero() {
            // Simple check: If in HeroList, it is a hero.
            // But we can also check equality with known heroes?
            // Usually we rely on the list we found the object in.
            return GetTeam() == 100 || GetTeam() == 200; // Lazy check
        }

        std::string GetName() {
            __try {
                uint64_t nameStruct = Address + Offset::NamePlayer;

                int length = *(int*)(nameStruct + 0x10);
                if (length <= 0 || length > 100) return "";

                if (length < 16) {
                    return std::string((char*)nameStruct);
                } else {
                    uint64_t ptr = *(uint64_t*)nameStruct;
                    if (!ptr || IsBadReadPtr((void*)ptr, length)) return "";
                    return std::string((char*)ptr);
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                return "";
            }
        }
        
        // SpellBook
        SpellBook GetSpellBook() {
             return SpellBook(Address + Offset::oObjSpellBook);
        }
        
        // ============================================================================
        // AI Manager - Movement Detection
        // ============================================================================
        // Có 2 phương pháp đọc AiManager:
        // 1. Direct (0x3108) - đơn giản, không encryption
        // 2. Obfuscated (0x36F0 + Decrypt) - có encryption, chính xác hơn
        //
        // Sử dụng GetAiManagerObfuscated() cho phương pháp 2 (khuyến nghị)
        // ============================================================================
        


        // Phương pháp 1: Obfuscated (có encryption) - Offset 0x4218 ✅ VERIFIED
        uint64_t GetAiManagerRaw() {
            __try {
                // ✅ VERIFIED: Offset 0x4218 (từ scan trong game)
                // Đây là obfuscated structure, cần decrypt
                uint64_t obfStructAddr = Address + Offset::oObjAiManagerObf; // 0x4218
                return IDA::DecryptAiManager(obfStructAddr);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }
        
        // Phương pháp 2: Obfuscated (alias - giữ để tương thích)
        uint64_t GetAiManagerObfuscated() {
            return GetAiManagerRaw(); // Dùng cùng logic
        }
        
        // Wrapper trả về object AiManager (với owner address để tính velocity từ GameObject position)
        AiManager GetAiManager() {
            return AiManager(GetAiManagerRaw(), Address); // Pass owner address
        }
        
        // Kiểm tra có đang dùng phương pháp obfuscated không
        bool IsUsingObfuscatedAiManager() {
            return GetAiManagerObfuscated() != 0;
        }
        
        // Lấy vị trí server (position từ AiManager - chính xác hơn GetPosition)
        Vector3 GetServerPosition() {
            uint64_t aiManager = GetAiManagerRaw();
            if (!aiManager) return GetPosition();
            
            __try {
                // Use direct offset (oAiManagerStartPath = 0x330)
                Vector3 pos = *(Vector3*)(aiManager + Offset::oAiManagerStartPath);
                if (pos.x > 0 && pos.x < 20000 && pos.z > 0 && pos.z < 20000) {
                    return pos;
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            
            return GetPosition();
        }
        
        // Lấy vị trí đích (target position - điểm click chuột)
        Vector3 GetTargetPosition() {
            uint64_t aiManager = GetAiManagerRaw();
            if (!aiManager) return GetPosition();
            
            __try {
                // Use direct offset (oAiManagerEndPath = 0x33C)
                return *(Vector3*)(aiManager + Offset::oAiManagerEndPath);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            
            return GetPosition();
        }
        
        // Kiểm tra có path hay không
        bool HasPath() {
            uint64_t aiManager = GetAiManagerRaw();
            if (!aiManager) return false;
            
            __try {
                // oAiManagerHasPath = 0x354 (verified: 1=IDLE, 2=MOVING, !=0=has path)
                return *(int*)(aiManager + Offset::oAiManagerHasPath) != 0;
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            
            return false;
        }
        
        // Kiểm tra đang di chuyển (IsMoving)
        bool IsMoving() {
            uint64_t aiManager = GetAiManagerRaw();
            if (!aiManager) return false;
            
            __try {
                // Primary: Use direct offset (oAiManagerIsMoving = 0x31C)
                if (*(bool*)(aiManager + Offset::oAiManagerIsMoving)) return true;
                
                // Fallback: Calculate from distance between StartPath and EndPath
                Vector3 startPath = *(Vector3*)(aiManager + Offset::oAiManagerStartPath);
                Vector3 endPath = *(Vector3*)(aiManager + Offset::oAiManagerEndPath);
                return startPath.Distance(endPath) > 5.0f;
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            
            return false;
        }
        
        // Kiểm tra đang lướt (IsDashing)
        bool IsDashing() {
            uint64_t aiManager = GetAiManagerRaw();
            if (!aiManager) return false;
            
            __try {
                // Primary: Use direct offset (oAiManagerIsDashing = 0x384)
                if (*(bool*)(aiManager + Offset::oAiManagerIsDashing)) return true;
                
                // Fallback: Check DashSpeed > 0
                float dashSpeed = *(float*)(aiManager + Offset::oAiManagerDashSpeed);
                return dashSpeed > 0.0f;
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            
            return false;
        }
        
        // Kiểm tra đứng yên (IsIdle)
        bool IsIdle() {
            return !IsMoving() && !IsDashing();
        }
        
        // Lấy tốc độ lướt (nếu đang dash)
        float GetDashSpeed() {
            uint64_t aiManager = GetAiManagerRaw();
            if (!aiManager) return 0;
            
            __try {
                // Use direct offset (oAiManagerDashSpeed = 0x360)
                return *(float*)(aiManager + Offset::oAiManagerDashSpeed);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            
            return 0;
        }
        
        // Lấy số segment trong path
        int GetPathSegmentsCount() {
            uint64_t aiManager = GetAiManagerRaw();
            if (!aiManager) return 0;
            
            __try {
                // Use direct offset (oAiManagerSegmentsCount = 0x350)
                return *(int*)(aiManager + Offset::oAiManagerSegmentsCount);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            
            return 0;
        }
        
        // Lấy segment hiện tại
        int GetCurrentPathSegment() {
            uint64_t aiManager = GetAiManagerRaw();
            if (!aiManager) return 0;
            
            __try {
                // Use direct offset (oAiManagerCurrentSegment = 0x320)
                return *(int*)(aiManager + Offset::oAiManagerCurrentSegment);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            
            return 0;
        }
        
        // Tính hướng di chuyển (normalized)
        Vector3 GetMoveDirection() {
            if (!IsMoving()) return Vector3(0, 0, 0);
            
            Vector3 start = GetServerPosition();
            Vector3 end = GetTargetPosition();
            Vector3 dir = Vector3(end.x - start.x, 0, end.z - start.z);
            
            float len = sqrtf(dir.x * dir.x + dir.z * dir.z);
            if (len > 0) {
                dir.x /= len;
                dir.z /= len;
            }
            return dir;
        }
        
        // Tính velocity vector (hướng * tốc độ)
        Vector3 GetVelocity() {
            if (!IsMoving()) return Vector3(0, 0, 0);
            
            Vector3 dir = GetMoveDirection();
            float speed = *(float*)(Address + Offset::SpeedPlayer); // MoveSpeed
            
            return Vector3(dir.x * speed, 0, dir.z * speed);
        }
        
        // Dự đoán vị trí sau t giây
        Vector3 PredictPosition(float t) {
            if (!IsMoving()) return GetPosition();
            
            Vector3 pos = GetServerPosition();
            Vector3 vel = GetVelocity();
            
            return Vector3(pos.x + vel.x * t, pos.y, pos.z + vel.z * t);
        }
    };
}

