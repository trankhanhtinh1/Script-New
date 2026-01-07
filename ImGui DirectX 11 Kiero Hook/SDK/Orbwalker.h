#pragma once
#include "ObjectManager.h"
#include "TargetSelector.h"
#include "MinionSelector.h"
#include "DamageCalculator.h"
#include "Game.h"
#include "Offsets.h"
#include "../Spoof_call/spoofcall.h"
#include "../Menu.h"
#include "HealthPrediction.h"
#include "DebugLog.h"
#include <algorithm>
#include <Psapi.h>
#include <fstream>

// ============================================================================
// INPUT BLOCKING HELPER (From LeagueAddon-main logic)
// ============================================================================
namespace InputHelper {
    // Check if chat is open using ChatInstance pointer + offset
    // Logic following LeagueAddon: *(bool*)(*(uintptr_t*)(base + ChatInstance) + IsChatOpenOffset)
    inline bool IsChatOpen() {
        uintptr_t base = (uintptr_t)GetModuleHandle(NULL);
        if (!base) return false;
        
        // Method 1: Direct global flag (faster, might be less reliable)
        bool directState = *(bool*)(base + Offset::oChatState);
        if (directState) return true;
        
        // Method 2: Pointer chain (more reliable, LeagueAddon style)
        uintptr_t chatPtr = *(uintptr_t*)(base + Offset::oChatInstance);
        if (chatPtr) {
            return *(bool*)(chatPtr + Offset::oIsChatOpen);
        }
        
        return false;
    }
    
    // Check if League window is in foreground (user is playing)
    inline bool IsLeagueInForeground() {
        HWND foreground = GetForegroundWindow();
        if (!foreground) return false;
        
        DWORD foregroundPID = 0;
        GetWindowThreadProcessId(foreground, &foregroundPID);
        
        return foregroundPID == GetCurrentProcessId();
    }
    
    // Combined check: should Orbwalker be active?
    inline bool ShouldProcessInput() {
        return IsLeagueInForeground() && !IsChatOpen();
    }
}

// Memory scanning helper for spoof_call trampoline
namespace mem {
    inline char* ScanBasic(char* pattern, char* mask, char* begin, intptr_t size) {
        intptr_t patternLen = strlen(mask);
        for (int i = 0; i < size; i++) {
            bool found = true;
            for (int j = 0; j < patternLen; j++) {
                if (mask[j] != '?' && pattern[j] != *(char*)((intptr_t)begin + i + j)) {
                    found = false;
                    break;
                }
            }
            if (found) return (begin + i);
        }
        return nullptr;
    }

    inline char* ScanInternal(char* pattern, char* mask, char* begin, intptr_t size) {
        char* match = nullptr;
        MEMORY_BASIC_INFORMATION mbi{};
        for (char* curr = begin; curr < begin + size; curr += mbi.RegionSize) {
            if (!VirtualQuery(curr, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS) 
                continue;
            match = ScanBasic(pattern, mask, curr, mbi.RegionSize);
            if (match != nullptr) break;
        }
        return match;
    }

    inline char* ScanModInternal(char* pattern, char* mask, char* moduleBase) {
        MODULEINFO moduleInfo;
        GetModuleInformation(GetCurrentProcess(), (HMODULE)GetModuleHandleA(nullptr), &moduleInfo, sizeof(MODULEINFO));
        return ScanInternal(pattern, mask, moduleBase, moduleInfo.SizeOfImage);
    }
}

namespace SDK
{
    enum class OrbwalkerMode {
        None,
        Combo,
        LaneClear,
        LastHit,
        Harass,
        Flee
    };

    class Orbwalker
    {
    public:
        static float LastAttackTime;
        static float LastMoveTime;
        static OrbwalkerMode ActiveMode; 

        static float GetGameTime() {
            return Game::GetTime();
        }

        static OrbwalkerMode GetMode() {
            if (GetAsyncKeyState(VK_SPACE) & 0x8000) return OrbwalkerMode::Combo;
            if (GetAsyncKeyState(0x56) & 0x8000) return OrbwalkerMode::LaneClear; // V
            if (GetAsyncKeyState(0x58) & 0x8000) return OrbwalkerMode::LastHit;  // X
            if (GetAsyncKeyState(0x43) & 0x8000) return OrbwalkerMode::Harass;   // C
            if (GetAsyncKeyState(0x5A) & 0x8000) return OrbwalkerMode::Flee;     // Z
            return OrbwalkerMode::None;
        }

        static bool CanAttack() {
            GameObject* local = ObjectManager::GetLocalPlayer();
            if (!local) return false;
            float time = GetGameTime();
            float delay = local->GetAttackDelay();
            delete local;
            return time >= LastAttackTime + delay;
        }

        static bool CanMove() {
            GameObject* local = ObjectManager::GetLocalPlayer();
            if (!local) return false;
            float time = GetGameTime();
            float windup = local->GetAttackWindup();
            delete local;
            return time >= LastAttackTime + windup + Menu::windupBuffer; 
        }

        static void IssueOrder(GameObject* unit, int Order, Vector3 Pos, GameObject* Target) {
             Debug::Log("=== IssueOrder Called ===");
             Debug::LogInt("Order", Order);

             if (!unit || !unit->Address) {
                 Debug::Log("!!! IssueOrder ERROR: unit is null !!!");
                 return;
             }
             Debug::LogHex("Unit", unit->Address);

             uint64_t moduleBase = ObjectManager::GetModuleBase();
             if (!moduleBase) {
                 Debug::Log("!!! IssueOrder ERROR: moduleBase is null !!!");
                 return;
             }

             static void* spoof_trampoline = nullptr;
             if (!spoof_trampoline) {
                 spoof_trampoline = mem::ScanModInternal((char*)"\xFF\x23", (char*)"xx", (char*)GetModuleHandleA(nullptr));
                 Debug::LogPtr("spoof_trampoline", spoof_trampoline);
             }

             if (!spoof_trampoline) {
                 Debug::Log("!!! IssueOrder ERROR: spoof_trampoline is null !!!");
                 return;
             }

             using fnIssueOrderCdecl = int64_t(__cdecl*)(
                 uintptr_t pAIBase,
                 int orderType,
                 Vector3* pos,
                 uintptr_t target,
                 bool isAttack,
                 bool isNetworked
             );

             fnIssueOrderCdecl spoofedIssueOrder = reinterpret_cast<fnIssueOrderCdecl>(moduleBase + Offset::Function::oIssueOrder);
             Debug::LogOffset("IssueOrder.func", moduleBase, Offset::Function::oIssueOrder, (uint64_t)spoofedIssueOrder);

             Vector3 localPos = Pos;
             uint64_t targetAddr = Target ? Target->Address : 0;
             bool isAttack = (Order == 3);

             Debug::LogHex("Target", targetAddr);
             Debug::Log("Calling spoof_call...");

             __try {
                 spoof_call(
                     spoof_trampoline,
                     spoofedIssueOrder,
                     unit->Address,
                     Order,
                     &localPos,
                     targetAddr,
                     isAttack,
                     false
                 );
                 Debug::Log("spoof_call OK");
             } __except(EXCEPTION_EXECUTE_HANDLER) {
                 Debug::Log("!!! IssueOrder spoof_call EXCEPTION !!!");
             }
        }

        static void Attack(GameObject* target) {
            if (CanAttack() && target) {
                 GameObject* local = ObjectManager::GetLocalPlayer();
                 if (local) {
                     IssueOrder(local, 3, target->GetPosition(), target);
                     LastAttackTime = GetGameTime();
                     delete local;
                 }
            }
        }

        static void Move() {
             if (CanMove()) {
                  float time = GetGameTime();
                  if (time >= LastMoveTime + Menu::clickDelay) { 
                      Vector3 mousePos = Game::GetMousePos(); 
                      if (mousePos.x != 0 || mousePos.z != 0) {
                           GameObject* local = ObjectManager::GetLocalPlayer();
                           if (local) {
                                IssueOrder(local, 2, mousePos, nullptr);
                                LastMoveTime = time;
                                delete local;
                           }
                      }
                  }
             }
        }

        // ============================================================================
        // Get Target Selector Mode from Menu settings
        // ============================================================================
        static TargetSelectorMode GetTSMode() {
            // Menu::tsMode: 0=LowestHealth, 1=MostPriority, 2=NearMouse, 3=LeastAttacks, 4=MostAD, 5=MostAP
            switch (Menu::tsMode) {
                case 0: return TargetSelectorMode::LowestHealth;
                case 1: return TargetSelectorMode::MostPriority;
                case 2: return TargetSelectorMode::NearMouse;
                case 3: return TargetSelectorMode::LeastAttacks;
                case 4: return TargetSelectorMode::MostAD;
                case 5: return TargetSelectorMode::MostAP;
                default: return TargetSelectorMode::LowestHealth;
            }
        }
        
        // ============================================================================
        // Combo - Attack enemy champions using TargetSelector
        // ============================================================================
        static void Combo() {
             GameObject* local = ObjectManager::GetLocalPlayer();
             if (!local) return;
             
             float range = local->GetRealAttackRange();
             delete local;
             
             // Use TargetSelector with mode from Menu settings
             GameObject* target = TargetSelector::GetTarget(range, GetTSMode());
             
             if (target) {
                 Attack(target);
                 delete target;
             } else {
                 Move();
             }
        }

        // ============================================================================
        // LaneClear - Push lane with last hit priority
        // Uses MinionSelector for improved targeting with DamageCalculator
        // ============================================================================
        static void LaneClear() {
             GameObject* local = ObjectManager::GetLocalPlayer();
             if (!local) return;
             
             float range = local->GetRealAttackRange();
             float attackDelayMs = (local->GetAttackWindup() + local->GetAttackDelay()) * 1000.0f;
             delete local;
             
             // 1. Check if we should wait for a minion about to be killable
             if (MinionSelector::ShouldWait(range, attackDelayMs)) {
                 Move();  // Wait for minion to get low enough
                 return;
             }
             
             // 2. Priority: Last hit > Push
             // GetLaneClearMinion handles both - returns last-hittable first, then push target
             GameObject* minion = MinionSelector::GetLaneClearMinion(range);
             
             if (minion) {
                 Attack(minion);
                 delete minion;
             } else {
                 // 3. No lane minions - check for jungle camps
                 GameObject* jungleMinion = MinionSelector::GetJungleMinion(range);
                 if (jungleMinion) {
                     Attack(jungleMinion);
                     delete jungleMinion;
                 } else {
                     Move();
                 }
             }
        }

        // ============================================================================
        // LastHit - Only attack minions that can be killed with one attack
        // Uses MinionSelector with DamageCalculator for accurate prediction
        // ============================================================================
        static void LastHit() {
             GameObject* local = ObjectManager::GetLocalPlayer();
             if (!local) return;
             
             float range = local->GetRealAttackRange();
             float attackDelayMs = (local->GetAttackWindup() + local->GetAttackDelay()) * 1000.0f;
             delete local;
             
             // Check if we should wait for any minion to become killable
             if (MinionSelector::ShouldWait(range, attackDelayMs)) {
                 Move();  // Don't waste attacks, wait for CS
                 return;
             }
             
             // Get best last-hittable minion
             GameObject* minion = MinionSelector::GetLastHitMinion(range);
             
             if (minion) {
                 Attack(minion);
                 delete minion;
             } else {
                 Move();
             }
        }
        
        // ============================================================================
        // Harass - Attack champions, fall back to last hit
        // ============================================================================
        static void Harass() {
             GameObject* local = ObjectManager::GetLocalPlayer();
             if (!local) return;
             
             float range = local->GetRealAttackRange();
             delete local;
             
             // Priority: Champion > Last Hit Minion
             GameObject* target = TargetSelector::GetTarget(range, GetTSMode());
             if (target) {
                 Attack(target);
                 delete target;
                 return;
             }
             
             // No champion, try last hit
             GameObject* minion = MinionSelector::GetLastHitMinion(range);
             if (minion) {
                 Attack(minion);
                 delete minion;
             } else {
                 Move();
             }
        }

        // ============================================================================
        // JungleClear - Clear jungle camps
        // ============================================================================
        static void JungleClear() {
             GameObject* local = ObjectManager::GetLocalPlayer();
             if (!local) return;
             
             float range = local->GetRealAttackRange();
             delete local;
             
             GameObject* jungleMinion = MinionSelector::GetJungleMinion(range);
             if (jungleMinion) {
                 Attack(jungleMinion);
                 delete jungleMinion;
             } else {
                 Move();
             }
        }

        static void OnUpdate() {
             if (!Menu::orbwalkerEnabled) return;
             
             // Block input when chat is open or game is not focused
             if (!InputHelper::ShouldProcessInput()) return;

             ActiveMode = GetMode();
             
             switch (ActiveMode) {
                 case OrbwalkerMode::Combo:
                     Combo();
                     break;
                 case OrbwalkerMode::LaneClear:
                     LaneClear();
                     break;
                 case OrbwalkerMode::LastHit:
                     LastHit();
                     break;
                 case OrbwalkerMode::Harass:
                     Harass();
                     break;
                 case OrbwalkerMode::Flee:
                     Move();
                     break;
                 default:
                     break;
             }
        }
    };
    
    // Init statics
    inline float Orbwalker::LastAttackTime = 0.0f;
    inline float Orbwalker::LastMoveTime = 0.0f;
    inline OrbwalkerMode Orbwalker::ActiveMode = OrbwalkerMode::None;
}
