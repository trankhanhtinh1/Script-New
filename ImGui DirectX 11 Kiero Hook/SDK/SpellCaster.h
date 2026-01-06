#pragma once
// ============================================================================
// SPELLCASTER.H - Cast spells using memory function call with spoof_call
// Based on: leagueoflegends-master CastSpell implementation
// ============================================================================

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include "Offsets.h"
#include "ObjectManager.h"
#include "GameObject.h"
#include "Spell.h"
#include "../Spoof_call/spoofcall.h"
#include "../ScanInternal.h"  // For ScanModInternal
#include "../Vector.h"

namespace SDK
{
    class SpellCaster
    {
    public:
        // Static log file
        static std::ofstream& GetLog() {
            static std::ofstream log("castspell_debug.txt", std::ios::app);
            return log;
        }
        
        // Log with timestamp
        static void Log(const std::string& msg) {
            auto& log = GetLog();
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            log << "[" << std::put_time(std::localtime(&time), "%H:%M:%S") << "] " << msg << std::endl;
            log.flush();
        }
        
        static void LogHex(const std::string& name, uint64_t value) {
            std::stringstream ss;
            ss << name << ": 0x" << std::hex << value << std::dec;
            Log(ss.str());
        }
        
        // ============================================================================
        // Get spoof trampoline (pattern scan for FF 23 = jmp qword ptr [rbx])
        // ============================================================================
        static void* GetSpoofTrampoline() {
            static void* spoof_trampoline = nullptr;
            if (!spoof_trampoline) {
                spoof_trampoline = mem::ScanModInternal(
                    (char*)"\xFF\x23", 
                    (char*)"xx", 
                    (char*)GetModuleHandleA(nullptr)
                );
            }
            return spoof_trampoline;
        }
        
        // ============================================================================
        // Read/Write Vector3 helpers (from leagueoflegends-master)
        // ============================================================================
        static Vector3 ReadVector3(uint64_t offset) {
            Vector3 result;
            result.x = *(float*)(offset);
            result.y = *(float*)(offset + 0x4);
            result.z = *(float*)(offset + 0x8);
            return result;
        }
        
        static void WriteVector3(uint64_t offset, Vector3 vector) {
            *(float*)(offset) = vector.x;
            *(float*)(offset + 0x4) = vector.y;
            *(float*)(offset + 0x8) = vector.z;
        }
        
        // ============================================================================
        // CAST SPELL - Main function
        // Uses same logic as leagueoflegends-master/global/functions.cpp
        // ============================================================================
        static bool CastSpell(int spellSlot, Vector3 targetPos) {
            Log("========================================");
            Log("=== CastSpell Called ===");
            Log("SpellSlot: " + std::to_string(spellSlot));
            Log("Target Pos: (" + std::to_string(targetPos.x) + ", " + 
                std::to_string(targetPos.y) + ", " + std::to_string(targetPos.z) + ")");
            
            // Validate spell slot (0=Q, 1=W, 2=E, 3=R, 4-5=Summoners)
            if (spellSlot < 0 || spellSlot >= 14) {
                Log("ERROR: Invalid spell slot " + std::to_string(spellSlot));
                return false;
            }
            
            // Get module base
            uint64_t moduleBase = ObjectManager::GetModuleBase();
            if (!moduleBase) {
                Log("ERROR: moduleBase is null");
                return false;
            }
            LogHex("ModuleBase", moduleBase);
            
            // Get spoof trampoline
            void* spoof_trampoline = GetSpoofTrampoline();
            if (!spoof_trampoline) {
                Log("ERROR: spoof_trampoline is null (pattern FF 23 not found)");
                return false;
            }
            LogHex("SpoofTrampoline", (uint64_t)spoof_trampoline);
            
            // Get local player
            GameObject* localPlayer = ObjectManager::GetLocalPlayer();
            if (!localPlayer || !localPlayer->IsValid()) {
                Log("ERROR: LocalPlayer is null or invalid");
                if (localPlayer) delete localPlayer;
                return false;
            }
            LogHex("LocalPlayer", localPlayer->Address);
            
            // Get SpellBook
            SpellBook spellBook(localPlayer->Address);
            if (!spellBook.IsValid()) {
                Log("ERROR: SpellBook is invalid");
                delete localPlayer;
                return false;
            }
            
            // Get Spell Slot
            SpellSlot spell = spellBook.GetSpell(spellSlot);
            if (!spell.IsValid()) {
                Log("ERROR: SpellSlot is invalid for slot " + std::to_string(spellSlot));
                delete localPlayer;
                return false;
            }
            LogHex("SpellSlot Address", spell.Address);
            
            // Check if spell is ready
            float gameTime = ObjectManager::GetGameTime();
            if (!spell.IsReady(gameTime)) {
                Log("ERROR: Spell is not ready (on cooldown)");
                Log("Cooldown remaining: " + std::to_string(spell.GetRemainingCooldown(gameTime)) + "s");
                delete localPlayer;
                return false;
            }
            Log("Spell is ready!");
            
            // Get SpellInfo pointer (from SpellSlot + oSpellSlotSpellInfo)
            uint64_t spellInfoPtr = *(uint64_t*)(spell.Address + Offset::oSpellSlotSpellInfo);
            if (!spellInfoPtr || spellInfoPtr < 0x10000) {
                Log("ERROR: SpellInfo pointer is invalid");
                LogHex("SpellInfo ptr", spellInfoPtr);
                delete localPlayer;
                return false;
            }
            LogHex("SpellInfo", spellInfoPtr);
            
            // Get SpellInput pointer (from SpellSlot + oSpellSlotSpellInput)
            uint64_t spellInput = *(uint64_t*)(spell.Address + Offset::oSpellSlotSpellInput);
            if (!spellInput || spellInput < 0x10000) {
                Log("ERROR: SpellInput pointer is invalid");
                LogHex("SpellInput ptr", spellInput);
                delete localPlayer;
                return false;
            }
            LogHex("SpellInput", spellInput);
            
            // Get HudInstance
            uint64_t hudInstance = *(uint64_t*)(moduleBase + Offset::oHudInstance);
            if (!hudInstance) {
                Log("ERROR: HudInstance is null");
                delete localPlayer;
                return false;
            }
            LogHex("HudInstance", hudInstance);
            
            // Get HudSpellInfo (HudInstance + oHudInstanceSpellInfo)
            uint64_t hudSpellInfo = *(uint64_t*)(hudInstance + Offset::oHudInstanceSpellInfo);
            if (!hudSpellInfo) {
                Log("ERROR: HudSpellInfo is null");
                delete localPlayer;
                return false;
            }
            LogHex("HudSpellInfo", hudSpellInfo);
            
            // Save original SpellInput values (restore after cast)
            Vector3 originalStartPos = ReadVector3(spellInput + Offset::oSpellInputStartPos);
            Vector3 originalEndPos = ReadVector3(spellInput + Offset::oSpellInputEndPos);
            Vector3 originalEndPos2 = ReadVector3(spellInput + Offset::oSpellInputEndPos + sizeof(Vector3));
            Vector3 originalEndPos3 = ReadVector3(spellInput + Offset::oSpellInputEndPos + sizeof(Vector3) * 2);
            
            Log("Original StartPos: (" + std::to_string(originalStartPos.x) + ", " + 
                std::to_string(originalStartPos.y) + ", " + std::to_string(originalStartPos.z) + ")");
            Log("Original EndPos: (" + std::to_string(originalEndPos.x) + ", " + 
                std::to_string(originalEndPos.y) + ", " + std::to_string(originalEndPos.z) + ")");
            
            // Write target position to SpellInput
            Vector3 playerPos = localPlayer->GetPosition();
            if (targetPos.x != 0 || targetPos.y != 0 || targetPos.z != 0) {
                WriteVector3(spellInput + Offset::oSpellInputStartPos, playerPos);
                WriteVector3(spellInput + Offset::oSpellInputEndPos, targetPos);
                WriteVector3(spellInput + Offset::oSpellInputEndPos + sizeof(Vector3), targetPos);
                WriteVector3(spellInput + Offset::oSpellInputEndPos + sizeof(Vector3) * 2, targetPos);
                Log("Wrote target position to SpellInput");
            }
            
            // Get CastSpellWrapper function address
            uint64_t castSpellAddr = moduleBase + Offset::Function::oCastSpellWrapper;
            LogHex("CastSpellWrapper addr", castSpellAddr);
            
            // Define function type
            // Signature: bool __fastcall CastSpellWrapper(uint64_t* hudSpellInfo, uint64_t* spellInfo)
            using fnCastSpellWrapper = bool(__fastcall*)(uint64_t* hudSpellInfo, uint64_t* spellInfo);
            fnCastSpellWrapper _fnCastSpellWrapper = (fnCastSpellWrapper)castSpellAddr;
            
            Log("Calling spoof_call to CastSpellWrapper...");
            
            // Call with spoof_call
            bool result = false;
            __try {
                result = spoof_call(
                    spoof_trampoline,
                    _fnCastSpellWrapper,
                    (uint64_t*)hudSpellInfo,
                    (uint64_t*)spellInfoPtr
                );
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                Log("EXCEPTION: CastSpellWrapper crashed!");
            }
            
            Log("CastSpellWrapper returned: " + std::to_string(result));
            
            // Restore original SpellInput values
            WriteVector3(spellInput + Offset::oSpellInputStartPos, originalStartPos);
            WriteVector3(spellInput + Offset::oSpellInputEndPos, originalEndPos);
            WriteVector3(spellInput + Offset::oSpellInputEndPos + sizeof(Vector3), originalEndPos2);
            WriteVector3(spellInput + Offset::oSpellInputEndPos + sizeof(Vector3) * 2, originalEndPos3);
            Log("Restored original SpellInput values");
            
            delete localPlayer;
            
            Log("=== CastSpell Complete ===");
            Log("Result: " + std::string(result ? "SUCCESS" : "FAILED"));
            Log("========================================");
            
            return result;
        }
        
        // ============================================================================
        // Convenience functions
        // ============================================================================
        static bool CastQ(Vector3 targetPos) { return CastSpell(0, targetPos); }
        static bool CastW(Vector3 targetPos) { return CastSpell(1, targetPos); }
        static bool CastE(Vector3 targetPos) { return CastSpell(2, targetPos); }
        static bool CastR(Vector3 targetPos) { return CastSpell(3, targetPos); }
        
        // Cast at mouse position
        static bool CastQAtMouse() {
            Vector3 mousePos = GetMouseWorldPos();
            return CastQ(mousePos);
        }
        
        static bool CastWAtMouse() {
            Vector3 mousePos = GetMouseWorldPos();
            return CastW(mousePos);
        }
        
        static bool CastEAtMouse() {
            Vector3 mousePos = GetMouseWorldPos();
            return CastE(mousePos);
        }
        
        static bool CastRAtMouse() {
            Vector3 mousePos = GetMouseWorldPos();
            return CastR(mousePos);
        }
        
        // Get mouse world position
        static Vector3 GetMouseWorldPos() {
            uint64_t moduleBase = ObjectManager::GetModuleBase();
            uint64_t hudInstance = *(uint64_t*)(moduleBase + Offset::oHudInstance);
            uint64_t hudInput = *(uint64_t*)(hudInstance + Offset::oHudInstanceInput);
            return ReadVector3(hudInput + Offset::oHudMouseVec3);
        }
    };
}
