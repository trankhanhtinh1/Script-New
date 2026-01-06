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
            return *(float*)(Address + Offset::oHealth);
        }

        float GetMaxHealth() {
            return *(float*)(Address + Offset::oMaxHealth);
        }

        bool IsDead() {
            // Check if health <= 0 or flag ? (usually flag is safer)
            // But if we trust offset:
            int dead = *(int*)(Address + Offset::oDead); // Check type, sometimes float or byte
            return dead == 1 || GetHealth() <= 0.0f;
        }

        bool IsVisible() {
            return *(bool*)(Address + Offset::oVisibility);
        }

        bool IsTargetable() {
            return *(bool*)(Address + Offset::oTargetable);
        }

        int GetTeam() {
            // Offset::TeamID (0x251). Instruction 0F B6 indicates byte.
            return (int)(*(unsigned char*)(Address + Offset::TeamID)); 
        }

        bool IsEnemyTo(GameObject* other) {
            if (!other || !other->IsValid()) return false;
            return GetTeam() != other->GetTeam();
        }

        // Network ID for missile target matching
        int GetNetworkId() {
            return *(int*)(Address + Offset::oObjNetId);
        }

        Vector3 GetPosition() {
            // Using oObjPosition which seemed reliable in original
            return *(Vector3*)(Address + Offset::oObjPosition);
        }
        
        float GetAttackRange() {
            return *(float*)(Address + Offset::RangeAttack);
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
            return *(float*)(Address + Offset::DamageBase);
        }

        float GetBonusAttackDamage() {
            return *(float*)(Address + Offset::DamageBonus);
        }
        
        float GetAttackDamage() {
            return GetBaseAttackDamage() + GetBonusAttackDamage();
        }

        // ============================================================================
        // Defensive Stats
        // ============================================================================
        float GetArmor() {
            return *(float*)(Address + Offset::Armor);
        }

        float GetMagicResist() {
            return *(float*)(Address + Offset::MagicResist);
        }

        float GetAbilityPower() {
            return *(float*)(Address + Offset::AbilityPower);
        }

        // ============================================================================
        // Penetration Stats
        // ============================================================================
        float GetArmorPenFlat() {  // Sát lực (Lethality)
            return *(float*)(Address + Offset::ArmorPenFlat);
        }

        float GetArmorPenPercent() {  // Xuyên giáp %
            return *(float*)(Address + Offset::ArmorPenPercent);
        }

        float GetMagicPenFlat() {  // Xuyên kháng phép (chỉ số)
            return *(float*)(Address + Offset::MagicPenFlat);
        }

        float GetMagicPenPercent() {  // Xuyên kháng phép %
            return *(float*)(Address + Offset::MagicPenPercent);
        }

        // ============================================================================
        // Critical Strike
        // ============================================================================
        float GetCritChance() {  // Tỉ lệ chí mạng (0.0 - 1.0)
            return *(float*)(Address + Offset::CritChance);
        }
        
        float GetCritDamage() {  // Sát thương chí mạng (1.75 base, 2.15 với IE)
            return *(float*)(Address + Offset::CritDamage);
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
            return *(float*)(Address + Offset::DamageBase) + *(float*)(Address + Offset::DamageBonus);
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
        // Type Checks
        bool IsMinion() {
             typedef bool(__thiscall* Fn)(void*);
             static uint64_t funcAddr = Offset::Function::isMinion + (uint64_t)GetModuleHandle(NULL);
             Fn func = (Fn)(funcAddr);
             if (IsBadCodePtr((FARPROC)func)) return false;
             return func((void*)Address);
        }

        bool IsTurret() {
             typedef bool(__thiscall* Fn)(void*);
             static uint64_t funcAddr = Offset::Function::isTurret + (uint64_t)GetModuleHandle(NULL);
             Fn func = (Fn)(funcAddr);
             if (IsBadCodePtr((FARPROC)func)) return false;
             return func((void*)Address);
        }
        
        // Attack Speed / Delay
        float GetAttackDelay() {
             typedef float(__cdecl* Fn)(GameObject*); // Reference says __cdecl, takes obj*
             static uint64_t funcAddr = Offset::Function::AttackDelay + (uint64_t)GetModuleHandle(NULL);
             Fn func = (Fn)(funcAddr);
             if (IsBadCodePtr((FARPROC)func)) return 0.0f;
             return func(this);
        }

        float GetAttackWindup() {
             typedef float(__cdecl* Fn)(GameObject*, int); // Reference says __cdecl, takes flags 0x40
             static uint64_t funcAddr = Offset::Function::oGetAttackWindup + (uint64_t)GetModuleHandle(NULL);
             Fn func = (Fn)(funcAddr);
             if (IsBadCodePtr((FARPROC)func)) return 0.0f;
             return func(this, 0x40); // 64 = legacy flag ? Reference uses 0x40
        }
        
        bool IsHero() {
            // Simple check: If in HeroList, it is a hero.
            // But we can also check equality with known heroes?
            // Usually we rely on the list we found the object in.
            return GetTeam() == 100 || GetTeam() == 200; // Lazy check
        }

        std::string GetName() {
            // NamePlayer = 0x4358;
            // The game uses a custom string class similar to std::string with SSO (Short String Optimization).
            // Structure:
            // [0x00]: Content (if len < 16) OR Pointer (if len >= 16)
            // [0x10]: Length
            // [0x14]: Capacity
            
            uint64_t nameStruct = Address + Offset::NamePlayer;
            
            int length = *(int*)(nameStruct + 0x10);
            if (length <= 0 || length > 100) return ""; // Invalid length sanity check
            
            if (length < 16) {
                // String is inline
                return std::string((char*)nameStruct);
            } else {
                // String is pointed to
                uint64_t ptr = *(uint64_t*)nameStruct;
                if (!ptr || IsBadReadPtr((void*)ptr, length)) return "BadPtr";
                
                // Read the string from the pointer
                // Since we can't directly construct std::string from remote memory pointer easily without reading it first...
                // But we are Internal here! So we can just access it.
                return std::string((char*)ptr);
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

