#pragma once
#include "GameObject.h"
#include "GameObjects.h"
#include "TargetSelector.h"
#include "DamageCalc.h"
#include "BuffManager.h"
#include "EventSystem.h"
#include "Game.h"
#include "../spoof/spoofcall.h"
#include <Psapi.h>
#include <algorithm>
#include <cmath>

// ============================================================================
// Orbwalker — Attack + Move logic with spoofcall
// Reference: EnsoulSharp.SDK Orbwalker + NewOrbwalker.cs
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
        // State (public — readable by plugins)
        // ====================================================================
        static inline float LastAttackTime  = 0.0f;   // Game time of last AA start
        static inline float LastMoveTime    = 0.0f;   // Game time of last move cmd
        static inline OrbwalkingMode ActiveMode = OrbwalkingMode::None;

        // Last target that was attacked
        static inline GameObject LastTarget;
        // Forced target (set by plugin/script)
        static inline GameObject ForceTarget;

        // Missile launched flag — when true, movement can happen earlier (ranged AA)
        static inline bool MissileLaunched = false;

        // Auto attack counter (for Sett / high AS limit logic)
        static inline int AutoAttackCounter = 0;

        // ====================================================================
        // Pause Timers (millisecond-based, using GetTickCount64)
        // Scripts can pause attack/move/both for a duration
        // ====================================================================
        static inline ULONGLONG AttackPauseTick = 0;
        static inline ULONGLONG MovePauseTick   = 0;
        static inline ULONGLONG AllPauseTick    = 0;

        // ====================================================================
        // Config (settable from menu)
        // ====================================================================
        static inline float WindupBuffer        = 0.05f;   // Extra windup delay (seconds)
        static inline float ClickDelay          = 0.08f;   // Min time between move commands
        static inline float HoldRadius          = 50.0f;   // Don't move if cursor within this range
        static inline float FarmDelay           = 0.03f;   // Farm delay offset (seconds)
        static inline bool  AttackEnabled       = true;
        static inline bool  MoveEnabled         = true;
        static inline bool  MissileCheckEnabled = true;    // Allow early move if missile launched
        static inline TargetSelector::Mode TSMode = TargetSelector::Mode::AutoPriority;

        // ====================================================================
        // Init — Register event callbacks (call once at startup)
        // ====================================================================

        static void Init() {
            static bool initialized = false;
            if (initialized) return;
            initialized = true;

            // Auto-detect MissileLaunched when our auto-attack missile is created
            EventSystem::OnMissileCreated([](const MissileArgs& args) {
                auto& player = GameObjects::Player;
                if (!player.IsValid()) return;

                // Check if this missile was fired by us
                if (args.CasterNetId != player.GetNetId()) return;

                // Check if it's an auto-attack missile
                if (args.SpellName.find("BasicAttack") != std::string::npos ||
                    args.SpellName.find("CritAttack") != std::string::npos) {
                    OnMissileLaunched();
                }
            });

            // Auto-detect spell casts for attack timing
            EventSystem::OnProcessSpellCast([](const SpellCastArgs& args) {
                auto& player = GameObjects::Player;
                if (!player.IsValid()) return;

                // Only care about our own casts
                if (args.Sender.GetNetId() != player.GetNetId()) return;

                if (args.IsAutoAttack) {
                    LastAttackTime = Game::GetTime();
                    MissileLaunched = false;
                    AutoAttackCounter++;
                }
            });
        }

        // ====================================================================
        // Pause API (for scripts/plugins)
        // ====================================================================

        static void SetPauseTime(int ms) {
            AllPauseTick = GetTickCount64() + ms;
        }

        static void SetAttackPauseTime(int ms) {
            AttackPauseTick = GetTickCount64() + ms;
        }

        static void SetMovePauseTime(int ms) {
            MovePauseTick = GetTickCount64() + ms;
        }

        static void ResetAutoAttackTimer() {
            LastAttackTime = 0.0f;
            MissileLaunched = false;
            AllPauseTick = 0;
            AttackPauseTick = 0;
            MovePauseTick = 0;
        }

        // ====================================================================
        // Timing — Can we attack / move?
        // ====================================================================

        static bool CanAttack() {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return false;
            if (!AttackEnabled) return false;

            ULONGLONG now = GetTickCount64();

            // All pause active?
            if (AllPauseTick > 0 && now < AllPauseTick) return false;
            // Attack pause active?
            if (AttackPauseTick > 0 && now < AttackPauseTick) return false;

            // CC checks
            BuffManager buffs(player.address);
            if (buffs.HasBuffOfType(BuffType::Fear)) return false;
            if (buffs.HasBuffOfType(BuffType::Polymorph)) return false;

            float time = Game::GetTime();
            float delay = player.GetAttackDelay();
            float ping = Game::GetPing() / 2000.0f; // half ping in seconds

            return time + ping >= LastAttackTime + delay;
        }

        static bool CanMove(float extraWindup = 0.0f) {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return false;
            if (!MoveEnabled) return false;

            ULONGLONG now = GetTickCount64();

            // All pause active?
            if (AllPauseTick > 0 && now < AllPauseTick) return false;
            // Move pause active?
            if (MovePauseTick > 0 && now < MovePauseTick) return false;

            // If missile already launched, allow movement immediately
            if (MissileLaunched && MissileCheckEnabled) return true;

            float time = Game::GetTime();
            float windup = player.GetAttackWindup();
            float ping = Game::GetPing() / 2000.0f;

            return time + ping >= LastAttackTime + windup + WindupBuffer + extraWindup;
        }

        // ====================================================================
        // ShouldWait — In LaneClear, wait if a minion will be last-hittable soon
        // Reference: NewOrbwalker.cs ShouldWait()
        // ====================================================================

        static bool ShouldWait() {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return false;

            float aaDmg = DamageCalc::GetAutoAttackDamage(player, player, false); // rough estimate
            float delay = player.GetAttackDelay() * 2.0f; // Look 2 attack delays ahead

            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsAlive() || !minion.IsVisible()) continue;
                if (!player.IsInAttackRange(minion)) continue;

                // Simple health prediction: will this minion die within 2 AA cycles?
                // We approximate: if HP < 2 * our AA damage, it might be last-hittable soon
                float hp = minion.GetHealth();
                float dmg = DamageCalc::CalcPhysicalDamage(player, minion, player.GetTotalAD());

                if (hp <= dmg * 2.0f) {
                    return true; // A minion is about to be last-hittable, wait!
                }
            }
            return false;
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

            // Set IssueOrder bypass flag BEFORE calling (same pattern as CastSpellSafe)
            // dword_1CDDF88 must be non-zero or game ignores the call
            Globals::Write<int>(Globals::base + Offset::Flag::IssueOrder, 1);

            __try {
                spoof_call(trampoline, fn,
                    player.address, order, &localPos, targetAddr, isAttack, false);
            } __except(1) {
                // IssueOrder crashed — silently ignore
            }

            // Reset flag
            Globals::Write<int>(Globals::base + Offset::Flag::IssueOrder, 0);
        }

        // ====================================================================
        // Actions
        // ====================================================================

        static void Attack(GameObject& target) {
            if (!CanAttack() || !target.IsValid()) return;
            IssueOrder(3, target.GetPosition(), &target);
            LastAttackTime = Game::GetTime();
            LastTarget = target;
            MissileLaunched = false;
            AutoAttackCounter++;
        }

        static void MoveTo(Vec3 pos) {
            if (!CanMove()) return;

            auto& player = GameObjects::Player;

            // Hold position check: don't move if cursor very close
            float distToCursor = player.GetPosition().Distance2D(pos);
            if (distToCursor <= HoldRadius + player.GetBoundingRadius()) return;

            float time = Game::GetTime();
            if (time < LastMoveTime + ClickDelay) return;

            IssueOrder(2, pos);
            LastMoveTime = time;
        }

        static void MoveToMouse() {
            MoveTo(Game::GetMouseWorldPos());
        }

        // ====================================================================
        // Notify: Missile launched (call from OnProcessSpell hook or update loop)
        // For ranged champions, this allows earlier movement
        // ====================================================================

        static void OnMissileLaunched() {
            MissileLaunched = true;
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

            // Check forced target first
            if (ForceTarget.IsValid() && TargetSelector::IsValidTarget(ForceTarget, range + 50.0f)) {
                if (player.IsInAttackRange(ForceTarget)) {
                    Attack(ForceTarget);
                    return;
                }
            }

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

            // Priority 1: Last-hittable minion (sorted: Siege > Super > health ascending)
            GameObject lastHitTarget;
            float lowestHP = 999999.0f;

            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsAlive() || !minion.IsVisible()) continue;
                if (!player.IsInAttackRange(minion)) continue;

                float hp = minion.GetHealth();
                float dmg = DamageCalc::CalcPhysicalDamage(player, minion, player.GetTotalAD());
                if (hp <= dmg && hp < lowestHP) {
                    lastHitTarget = minion;
                    lowestHP = hp;
                }
            }

            if (lastHitTarget.IsValid()) {
                Attack(lastHitTarget);
                return;
            }

            // Priority 2: Enemy hero if no minion about to die (don't waste AA if ShouldWait)
            if (!ShouldWait()) {
                GameObject heroTarget = TargetSelector::GetTarget(range + 50.0f, TSMode);
                if (heroTarget.IsValid() && player.IsInAttackRange(heroTarget)) {
                    Attack(heroTarget);
                    return;
                }
            }

            // Priority 3: Push wave — attack minion with highest HP (so we don't steal from last-hit)
            if (!ShouldWait()) {
                GameObject pushTarget;
                float highestHP = 0.0f;

                for (auto& minion : GameObjects::EnemyMinions) {
                    if (!minion.IsAlive() || !minion.IsVisible()) continue;
                    if (!player.IsInAttackRange(minion)) continue;

                    float hp = minion.GetHealth();
                    if (hp > highestHP) {
                        pushTarget = minion;
                        highestHP = hp;
                    }
                }

                if (pushTarget.IsValid()) {
                    Attack(pushTarget);
                    return;
                }
            }

            // Priority 4: Jungle monsters
            for (auto& mob : GameObjects::JungleMinions) {
                if (!mob.IsAlive() || !mob.IsVisible()) continue;
                if (player.IsInAttackRange(mob)) {
                    Attack(mob);
                    return;
                }
            }

            // Priority 5: Enemy turrets
            for (auto& turret : GameObjects::EnemyTurrets) {
                if (!turret.IsAlive()) continue;
                if (player.IsInAttackRange(turret)) {
                    Attack(turret);
                    return;
                }
            }

            MoveToMouse();
        }

        static void LastHit() {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return;

            // Find minion that can be last-hit (lowest HP first)
            GameObject bestTarget;
            float lowestHP = 999999.0f;

            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsAlive() || !minion.IsVisible()) continue;
                if (!player.IsInAttackRange(minion)) continue;

                float hp = minion.GetHealth();
                float dmg = DamageCalc::CalcPhysicalDamage(player, minion, player.GetTotalAD());
                if (hp <= dmg && hp < lowestHP) {
                    bestTarget = minion;
                    lowestHP = hp;
                }
            }

            if (bestTarget.IsValid()) {
                Attack(bestTarget);
                return;
            }

            MoveToMouse();
        }

        static void Harass() {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return;

            float range = player.GetRealAttackRange();

            // Priority 1: Champion
            GameObject target = TargetSelector::GetTarget(range + 50.0f, TSMode);
            if (target.IsValid() && player.IsInAttackRange(target)) {
                Attack(target);
                return;
            }

            // Priority 2: Last hit
            LastHit();
        }

        // ====================================================================
        // Main Update — call from render loop
        // ====================================================================

        static void OnUpdate() {
            if (!Game::ShouldProcessInput()) return;

            ActiveMode = GetMode();

            switch (ActiveMode) {
            case OrbwalkingMode::Combo:     Combo();       break;
            case OrbwalkingMode::LaneClear: LaneClear();   break;
            case OrbwalkingMode::LastHit:   LastHit();     break;
            case OrbwalkingMode::Hybrid:    Harass();      break;
            case OrbwalkingMode::Flee:      MoveToMouse(); break;
            default: break;
            }
        }
    };

} // namespace SDK
