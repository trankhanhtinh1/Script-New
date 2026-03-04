#pragma once
#include "GameObject.h"
#include "GameObjects.h"
#include "TargetSelector.h"
#include "Game.h"
#include "../spoof/spoofcall.h"
#include <Psapi.h>
#include <algorithm>

// ============================================================================
// Orbwalker — Attack + Move logic with spoofcall
// Reference: EnsoulSharp.SDK Orbwalker + Script-New-main/SDK/Orbwalker.h
// ============================================================================

namespace SDK {

    // Memory scanning for trampoline gadget
    namespace mem {
        inline char* ScanBasic(char* pattern, char* mask, char* begin, intptr_t size) {
            intptr_t patternLen = strlen(mask);
            for (intptr_t i = 0; i < size; i++) {
                bool found = true;
                for (intptr_t j = 0; j < patternLen; j++) {
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

    class Orbwalker {
    public:
        // ====================================================================
        // State
        // ====================================================================
        static inline float LastAttackTime  = 0.0f;
        static inline float LastMoveTime    = 0.0f;
        static inline OrbwalkingMode ActiveMode = OrbwalkingMode::None;

        // Config (settable from menu)
        static inline float WindupBuffer    = 0.05f;   // Extra windup delay
        static inline float ClickDelay      = 0.08f;   // Min time between move commands
        static inline TargetSelector::Mode TSMode = TargetSelector::Mode::LowestHP;

        // ====================================================================
        // Timing
        // ====================================================================

        static bool CanAttack() {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return false;
            float time = Game::GetTime();
            float delay = player.GetAttackDelay();
            return time >= LastAttackTime + delay;
        }

        static bool CanMove() {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return false;
            float time = Game::GetTime();
            float windup = player.GetAttackWindup();
            return time >= LastAttackTime + windup + WindupBuffer;
        }

        // ====================================================================
        // IssueOrder via spoof_call
        // ====================================================================

        static void IssueOrder(int order, Vec3 pos, GameObject* target = nullptr) {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return;

            // Find trampoline gadget (FF 23 = jmp [rbx])
            static void* trampoline = nullptr;
            if (!trampoline) {
                trampoline = mem::ScanModInternal(
                    (char*)"\xFF\x23", (char*)"xx",
                    (char*)GetModuleHandleA(nullptr));
            }
            if (!trampoline) return;

            // Function signature: int64_t __cdecl(obj, order, pos*, target, isAttack, isNetworked)
            using fnIssueOrder = int64_t(__cdecl*)(
                uintptr_t, int, Vec3*, uintptr_t, bool, bool);

            fnIssueOrder fn = reinterpret_cast<fnIssueOrder>(
                Globals::base + Offset::Function::IssueOrder);

            // IMPORTANT: local copy of position!
            Vec3 localPos = pos;
            uintptr_t targetAddr = (target && target->IsValid()) ? target->address : 0;
            bool isAttack = (order == 3);

            spoof_call(trampoline, fn,
                player.address, order, &localPos, targetAddr, isAttack, false);
        }

        // ====================================================================
        // Actions
        // ====================================================================

        static void Attack(GameObject& target) {
            if (!CanAttack() || !target.IsValid()) return;
            IssueOrder(3, target.GetPosition(), &target);
            LastAttackTime = Game::GetTime();
        }

        static void MoveTo(Vec3 pos) {
            if (!CanMove()) return;
            float time = Game::GetTime();
            if (time < LastMoveTime + ClickDelay) return;
            IssueOrder(2, pos);
            LastMoveTime = time;
        }

        static void MoveToMouse() {
            MoveTo(Game::GetMouseWorldPos());
        }

        // ====================================================================
        // Mode Detection (hotkeys)
        // ====================================================================

        static OrbwalkingMode GetMode() {
            if (GetAsyncKeyState(VK_SPACE) & 0x8000) return OrbwalkingMode::Combo;
            if (GetAsyncKeyState(0x56)     & 0x8000) return OrbwalkingMode::LaneClear; // V
            if (GetAsyncKeyState(0x58)     & 0x8000) return OrbwalkingMode::LastHit;   // X
            if (GetAsyncKeyState(0x43)     & 0x8000) return OrbwalkingMode::Hybrid;    // C (Harass)
            if (GetAsyncKeyState(0x5A)     & 0x8000) return OrbwalkingMode::Flee;      // Z
            return OrbwalkingMode::None;
        }

        // ====================================================================
        // Mode Behaviors
        // ====================================================================

        static void Combo() {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return;

            float range = player.GetRealAttackRange();
            GameObject target = TargetSelector::GetTarget(range + 50.0f, TSMode);

            if (target.IsValid() && player.IsInAttackRange(target)) {
                Attack(target);
            } else {
                MoveToMouse();
            }
        }

        static void LaneClear() {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return;

            float range = player.GetRealAttackRange();

            // Priority 1: Last-hittable minion
            GameObject lastHitTarget;
            float lowestHP = 999999.0f;

            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsAlive() || !minion.IsVisible()) continue;
                if (!player.IsInAttackRange(minion)) continue;

                float hp = minion.GetHealth();
                float dmg = player.CalcPhysicalDamage(minion);
                if (hp <= dmg && hp < lowestHP) {
                    lastHitTarget = minion;
                    lowestHP = hp;
                }
            }

            if (lastHitTarget.IsValid()) {
                Attack(lastHitTarget);
                return;
            }

            // Priority 2: Any minion (push)
            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsAlive() || !minion.IsVisible()) continue;
                if (player.IsInAttackRange(minion)) {
                    Attack(minion);
                    return;
                }
            }

            // Priority 3: Jungle
            for (auto& mob : GameObjects::JungleMinions) {
                if (!mob.IsAlive() || !mob.IsVisible()) continue;
                if (player.IsInAttackRange(mob)) {
                    Attack(mob);
                    return;
                }
            }

            MoveToMouse();
        }

        static void LastHit() {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return;

            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsAlive() || !minion.IsVisible()) continue;
                if (!player.IsInAttackRange(minion)) continue;

                float hp = minion.GetHealth();
                float dmg = player.CalcPhysicalDamage(minion);
                if (hp <= dmg) {
                    Attack(minion);
                    return;
                }
            }

            MoveToMouse();
        }

        static void Harass() {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return;

            float range = player.GetRealAttackRange();

            // Priority: Champion > Last hit
            GameObject target = TargetSelector::GetTarget(range + 50.0f, TSMode);
            if (target.IsValid() && player.IsInAttackRange(target)) {
                Attack(target);
                return;
            }

            // Fall back to last hit
            LastHit();
        }

        // ====================================================================
        // Main Update — call from render loop
        // ====================================================================

        static void OnUpdate() {
            if (!Game::ShouldProcessInput()) return;

            ActiveMode = GetMode();

            switch (ActiveMode) {
            case OrbwalkingMode::Combo:     Combo();     break;
            case OrbwalkingMode::LaneClear: LaneClear(); break;
            case OrbwalkingMode::LastHit:   LastHit();   break;
            case OrbwalkingMode::Hybrid:    Harass();    break;
            case OrbwalkingMode::Flee:      MoveToMouse(); break;
            default: break;
            }
        }
    };

} // namespace SDK
