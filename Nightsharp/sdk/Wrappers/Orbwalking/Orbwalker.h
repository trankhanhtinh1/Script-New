#pragma once
#include "GameObject.h"
#include "GameObjects.h"
#include "TargetSelector.h"
#include "DamageCalc.h"
#include "BuffManager.h"
#include "EventSystem.h"
#include "Game.h"
#include "HealthPrediction.h"
#include "sdk/UI/MenuUI.h"
#include "sdk/UI/Drawing.h"
#include "sdk/Utils/AutoAttackUtil.h"
#include "sdk/Utils/Bypass.h"
#include "spoof/spoofcall.h"
#include <Psapi.h>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cfloat>
#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "sdk/Utils/DebugConsole.h"
#include "sdk/Utils/JungleUtils.h"

// ============================================================================
// Orbwalker — Attack + Move logic with spoofcall
// Reference: EnsoulSharp.SDK OrbwalkerBase + Orbwalker + OrbwalkerSelector
// ============================================================================

namespace SDK {

    // ========================================================================
    // OrbwalkingActionArgs — Event data for orbwalker actions
    // Source: EnsoulSharp.SDK OrbwalkerBase.cs / OrbwalkingActionArgs
    // ========================================================================
    struct OrbwalkingActionArgs {
        OrbwalkingType Type = OrbwalkingType::None;
        GameObject     Target;         // Attack target (for attack events)
        GameObject     Sender;         // Caster (usually local player)
        Vec3           Position;       // Move destination (for movement events)
        bool           Process = true; // Set to false in BeforeAttack/Movement to cancel
    };

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

    using fnIssueOrder = int64_t(__cdecl*)(uintptr_t, int, Vec3*, uintptr_t, bool, bool);

    static void IssueOrderExecute(void* trampoline, fnIssueOrder fn,
        uintptr_t playerAddr, int order, Vec3* pos, uintptr_t targetAddr, bool isAttack) {
        __try {
            spoof_call(trampoline, fn,
                playerAddr, order, pos, targetAddr, isAttack, false);
        } __except(1) {
            // IssueOrder crashed â€” ignore
        }
    }

    class Orbwalker {
    public:
        // ====================================================================
        // OnAction Event System — EnsoulSharp OrbwalkerBase.OnAction port
        // Scripts subscribe to get notified of orbwalker actions.
        // For BeforeAttack/Movement: set args.Process = false to cancel.
        // ====================================================================
        using OnActionCallback = std::function<void(OrbwalkingActionArgs&)>;
        static inline std::vector<OnActionCallback> s_onActionCallbacks;
        using OnBeforeAttackCallback = std::function<void(OrbwalkingActionArgs&)>;
        using OnAfterAttackCallback = std::function<void(OrbwalkingActionArgs&)>;
        static inline std::vector<OnBeforeAttackCallback> s_beforeAttackCallbacks;
        static inline std::vector<OnAfterAttackCallback> s_afterAttackCallbacks;

        /// Subscribe to orbwalker action events
        static void OnAction(OnActionCallback cb) {
            s_onActionCallbacks.push_back(std::move(cb));
        }

        /// Subscribe to BeforeAttack event (EnsoulSharp-style API)
        static void OnBeforeAttack(OnBeforeAttackCallback cb) {
            s_beforeAttackCallbacks.push_back(std::move(cb));
        }

        /// Subscribe to AfterAttack event (EnsoulSharp-style API)
        static void OnAfterAttack(OnAfterAttackCallback cb) {
            s_afterAttackCallbacks.push_back(std::move(cb));
        }

        /// Fire BeforeAttack event — returns false if cancelled.
        static bool InvokeBeforeAttack(OrbwalkingActionArgs& args) {
            for (auto& cb : s_beforeAttackCallbacks) {
                try { cb(args); } catch (...) {}
            }
            return args.Process;
        }

        /// Fire AfterAttack event.
        static void InvokeAfterAttack(OrbwalkingActionArgs& args) {
            for (auto& cb : s_afterAttackCallbacks) {
                try { cb(args); } catch (...) {}
            }
        }

        /// Fire an action event — returns false if any subscriber set Process=false
        static bool InvokeAction(OrbwalkingActionArgs& args) {
            for (auto& cb : s_onActionCallbacks) {
                try { cb(args); } catch (...) {}
            }
            return args.Process;
        }

        // Default SDK orbwalker is active unless OrbwalkerPlugin is loaded.
        static inline bool PluginOverrideActive = false;
        static void SetPluginOverrideActive(bool active) {
            PluginOverrideActive = active;
        }
        static bool IsPluginOverrideActive() {
            return PluginOverrideActive;
        }

        // ====================================================================
        // State (public — readable by plugins)
        // ====================================================================
        static inline float LastAttackTime  = 0.0f;   // Game time of last AA start
        static inline float LastMoveTime    = 0.0f;   // Game time of last move cmd
        static inline OrbwalkingMode ActiveMode = OrbwalkingMode::None;

        // Last target that was attacked
        static inline GameObject LastTarget;
        // Locked target to prevent switching mid-windup
        static inline GameObject LockedTarget;
        // Forced target (set by plugin/script)
        static inline GameObject ForceTarget;
        // Lane-clear cached minion (EnsoulSharp selector behavior)
        static inline GameObject LaneClearMinion;

        // Missile launched flag — when true, movement can happen earlier (ranged AA)
        static inline bool MissileLaunched = false;

        // Auto attack counter (for Sett / high AS limit logic)
        static inline int AutoAttackCounter = 0;
        // Temporary order lock after attack command (EnsoulSharp BlockOrdersUntilTick style)
        static inline int BlockOrdersUntilTick = 0;
        static inline int LastAttackCommandTick = 0;
        static inline int LastMovementOrderTick = 0;

        // Attack/Move state — set by scripts to block orbwalker actions
        static inline bool AttackState = true;   // false = don't attack
        static inline bool MovementState = true;  // false = don't move

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

        // EnsoulSharp-style default orbwalker menu hosted in SDK.
        static inline std::shared_ptr<MenuUI::Menu> MenuRoot;
        static inline bool Enabled = true;
        static inline float MovementMaximumDistance = 1500.0f;
        static inline bool MovementRandomize = true;
        static inline bool DrawAARange = true;
        static inline bool DrawAARangeEnemy = false;
        static inline bool DrawExtraHoldPosition = false;
        static inline bool DrawKillableMinion = false;
        static inline bool DrawKillableMinionFade = false;
        static inline bool DebugLaneClear = true;  // LaneClear target debug overlay

        static MenuUI::Menu* GetMenuRoot() {
            return MenuRoot.get();
        }

        static void EnsureMenu() {
            if (MenuRoot) return;

            MenuRoot = MenuUI::Menu::Create("OrbwalkerCore", "Orbwalker");
            if (!MenuRoot) return;

            auto drawings = MenuRoot->AddSubMenu("drawings", "Drawings");
            drawings->Add<MenuUI::MenuBool>("drawAARange", "Auto-Attack Range", true);
            drawings->Add<MenuUI::MenuBool>("drawAARangeEnemy", "Auto-Attack Range Enemy", false);
            drawings->Add<MenuUI::MenuBool>("drawExtraHoldPosition", "Extra Hold Position", false);
            drawings->Add<MenuUI::MenuBool>("drawKillableMinion", "Killable Minions", false);
            drawings->Add<MenuUI::MenuBool>("drawKillableMinionFade", "Killable Minions Fade Effect", false);

            auto advanced = MenuRoot->AddSubMenu("advanced", "Advanced");
            advanced->Add<MenuUI::MenuBool>("movementRandomize", "Randomize Location", true);
            advanced->Add<MenuUI::MenuSlider>("movementExtraHold", "Extra Hold Position", 0, 0, 250);
            advanced->Add<MenuUI::MenuSlider>("movementMaximumDistance", "Maximum Distance", 1500, 500, 1500);
            advanced->Add<MenuUI::MenuSlider>("delayMovement", "Movement Delay", 0, 0, 500);
            advanced->Add<MenuUI::MenuSlider>("delayWindup", "Windup Delay", 80, 0, 200);
            advanced->Add<MenuUI::MenuSlider>("delayFarm", "Farm Delay", 30, 0, 200);
            advanced->Add<MenuUI::MenuBool>("prioritizeFarm", "Farm Over Harass", true);
            advanced->Add<MenuUI::MenuBool>("prioritizeMinions", "Minions Over Objectives", false);
            advanced->Add<MenuUI::MenuBool>("prioritizeSmallJungle", "Small Jungle", false);
            advanced->Add<MenuUI::MenuBool>("prioritizeWards", "Wards", false);
            advanced->Add<MenuUI::MenuBool>("prioritizeSpecialMinions", "Special Minions", false);
            advanced->Add<MenuUI::MenuBool>("attackWards", "Attack Wards", false);
            advanced->Add<MenuUI::MenuBool>("attackBarrels", "Attack Barrels", false);
            advanced->Add<MenuUI::MenuBool>("attackClones", "Attack Clones", false);
            advanced->Add<MenuUI::MenuBool>("attackSpecialMinions", "Attack Special Minions", true);
            advanced->Add<MenuUI::MenuBool>("miscMissile", "Use Missile Checks", true);
            advanced->Add<MenuUI::MenuBool>("miscAttackSpeed", "Don't Kite if Attack Speed > 2.5", true);

            MenuRoot->Add<MenuUI::MenuKeyBind>("lasthitKey", "Last Hit", 'X', MenuUI::KeyBindType::Press);
            MenuRoot->Add<MenuUI::MenuKeyBind>("laneclearKey", "Lane Clear", 'V', MenuUI::KeyBindType::Press);
            MenuRoot->Add<MenuUI::MenuKeyBind>("hybridKey", "Hybrid", 'C', MenuUI::KeyBindType::Press);
            MenuRoot->Add<MenuUI::MenuKeyBind>("comboKey", "Combo", VK_SPACE, MenuUI::KeyBindType::Press);
            MenuRoot->Add<MenuUI::MenuKeyBind>("fleeKey", "Flee", 'Z', MenuUI::KeyBindType::Press);
            MenuRoot->Add<MenuUI::MenuBool>("enabledOption", "Enabled", true);
        }

        static bool MenuBool(const char* name, bool fallback = false, const char* sub = nullptr) {
            if (!MenuRoot) return fallback;
            MenuUI::Menu* owner = MenuRoot.get();
            if (sub && *sub) {
                MenuUI::Menu* sm = owner->GetSubMenu(sub);
                if (!sm) return fallback;
                owner = sm;
            }
            auto* item = owner->Get<MenuUI::MenuBool>(name);
            return item ? item->Enabled : fallback;
        }

        static int MenuSlider(const char* name, int fallback = 0, const char* sub = nullptr) {
            if (!MenuRoot) return fallback;
            MenuUI::Menu* owner = MenuRoot.get();
            if (sub && *sub) {
                MenuUI::Menu* sm = owner->GetSubMenu(sub);
                if (!sm) return fallback;
                owner = sm;
            }
            auto* item = owner->Get<MenuUI::MenuSlider>(name);
            return item ? item->Value : fallback;
        }

        static bool MenuKeyActive(const char* name, bool fallback = false) {
            if (!MenuRoot) return fallback;
            const auto* item = MenuRoot->Get<MenuUI::MenuKeyBind>(name);
            return item ? item->Active : fallback;
        }

        static void SyncConfigFromMenu() {
            EnsureMenu();
            Enabled = MenuBool("enabledOption", true);
            WindupBuffer = (float)MenuSlider("delayWindup", 80, "advanced") / 1000.0f;
            ClickDelay = (float)MenuSlider("delayMovement", 0, "advanced") / 1000.0f;
            HoldRadius = (float)MenuSlider("movementExtraHold", 0, "advanced");
            FarmDelay = (float)MenuSlider("delayFarm", 30, "advanced") / 1000.0f;
            MissileCheckEnabled = MenuBool("miscMissile", true, "advanced");
            MovementRandomize = MenuBool("movementRandomize", true, "advanced");
            MovementMaximumDistance = (float)MenuSlider("movementMaximumDistance", 1500, "advanced");
            DrawAARange = MenuBool("drawAARange", true, "drawings");
            DrawAARangeEnemy = MenuBool("drawAARangeEnemy", false, "drawings");
            DrawExtraHoldPosition = MenuBool("drawExtraHoldPosition", false, "drawings");
            DrawKillableMinion = MenuBool("drawKillableMinion", false, "drawings");
            DrawKillableMinionFade = MenuBool("drawKillableMinionFade", false, "drawings");
        }

        // ====================================================================
        // Init — Register event callbacks (call once at startup)
        // ====================================================================

        static void Init() {
            static bool initialized = false;
            if (initialized) return;
            initialized = true;

            EnsureMenu();
            SyncConfigFromMenu();
            srand((unsigned int)GetTickCount64());

            // Auto-detect MissileLaunched when our auto-attack missile is created
            // This fires the AfterAttack event (ranged: projectile visible)
            EventSystem::OnMissileCreated([](const MissileArgs& args) {
                auto& player = GameObjects::Player;
                if (!player.IsValid()) return;

                // Check if this missile was fired by us
                if (args.CasterNetId != player.GetNetId()) return;

                // Check if it's an auto-attack missile
                if (args.SpellName.find("BasicAttack") != std::string::npos ||
                    args.SpellName.find("CritAttack") != std::string::npos) {
                    OnMissileLaunched();

                    // Fire AfterAttack event (ranged champions)
                    OrbwalkingActionArgs actionArgs;
                    actionArgs.Type = OrbwalkingType::AfterAttack;
                    actionArgs.Target = LastTarget;
                    actionArgs.Sender = player;
                    InvokeAfterAttack(actionArgs);
                    InvokeAction(actionArgs);
                }
            });

            // Auto-detect spell casts for attack timing (OnDoCast equivalent)
            EventSystem::OnProcessSpellCast([](const SpellCastArgs& args) {
                auto& player = GameObjects::Player;
                if (!player.IsValid()) return;

                // Only care about our own casts
                if (args.Sender.GetNetId() != player.GetNetId()) return;

                if (args.IsAutoAttack) {
                    LastAttackTime = Game::GetTime();
                    MissileLaunched = false;
                    AutoAttackCounter++;

                    // Fire OnAttack event (attack command confirmed by server)
                    OrbwalkingActionArgs actionArgs;
                    actionArgs.Type = OrbwalkingType::OnAttack;
                    actionArgs.Target = LastTarget;
                    actionArgs.Sender = player;
                    InvokeAction(actionArgs);

                    // For melee champions, also fire AfterAttack immediately
                    // (no projectile missile to wait for)
                    if (player.IsMelee()) {
                        OrbwalkingActionArgs afterArgs;
                        afterArgs.Type = OrbwalkingType::AfterAttack;
                        afterArgs.Target = LastTarget;
                        afterArgs.Sender = player;
                        InvokeAfterAttack(afterArgs);
                        InvokeAction(afterArgs);
                    }
                }

                // Auto-attack reset detection
                if (AutoAttackUtil::IsAutoAttackReset(args.SpellName)) {
                    ResetAutoAttackTimer();
                }
            });

            // StopCast event — reset timer when our AA is interrupted
            EventSystem::OnStopCast([](const StopCastArgs& args) {
                auto& player = GameObjects::Player;
                if (!player.IsValid()) return;
                if (!args.Sender.IsValid() || args.Sender.GetNetId() != player.GetNetId()) return;
                // Reset if our auto-attack was force-stopped (CC interrupt)
                if (args.WasAutoAttack && args.ForceStop) {
                    ResetAutoAttackTimer();
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
            BlockOrdersUntilTick = 0;
            LastAttackCommandTick = 0;
            LastMovementOrderTick = 0;
            AllPauseTick = 0;
            AttackPauseTick = 0;
            MovePauseTick = 0;
        }

        // ====================================================================
        // Timing — Can we attack / move?
        // ====================================================================

        // Cached champion name — refreshed once in OnUpdate instead of every call
        static inline std::string s_cachedChampName;
        static inline float s_cachedChampNameTime = 0.0f;

        static const std::string& GetCachedChampName() {
            float now = Game::GetTime();
            if (now - s_cachedChampNameTime > 1.0f) {
                s_cachedChampNameTime = now;
                if (GameObjects::Player.IsValid()) {
                    s_cachedChampName = GameObjects::Player.GetChampionName();
                }
            }
            return s_cachedChampName;
        }

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

            // EnsoulSharp-specific champion checks (using cached name)
            const std::string& champName = GetCachedChampName();
            if (_stricmp(champName.c_str(), "Jhin") == 0 && player.HasBuff("JhinPassiveReload")) {
                return false;
            }
            if (_stricmp(champName.c_str(), "Graves") == 0 && !player.HasBuff("gravesbasicattackammo1")) {
                return false;
            }

            float time = Game::GetTime();
            float delay = player.GetAttackDelay();
            if (_stricmp(champName.c_str(), "Graves") == 0) {
                delay = (delay * 1.0740296828f) - 0.7162381256f;
            }
            float ping = Game::GetPing() / 2000.0f;

            return time + ping >= LastAttackTime + delay;
        }

        static bool CanMove(float extraWindup = 0.0f, bool disableMissileCheck = false) {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return false;
            if (!MoveEnabled) return false;

            ULONGLONG now = GetTickCount64();

            // All pause active?
            if (AllPauseTick > 0 && now < AllPauseTick) return false;
            // Move pause active?
            if (MovePauseTick > 0 && now < MovePauseTick) return false;

            // If missile already launched, allow movement immediately
            if (MissileLaunched && MissileCheckEnabled && !disableMissileCheck) return true;

            float time = Game::GetTime();
            float windup = player.GetAttackWindup();
            const std::string& champName = GetCachedChampName();
            if (_stricmp(champName.c_str(), "Rengar") == 0 &&
                (player.HasBuff("RengarQ") || player.HasBuff("RengarQEmp"))) {
                extraWindup += 0.2f; // +200ms like EnsoulSharp
            }
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

            const float predictionMs = player.GetAttackDelay() * 2000.0f + FarmDelay * 1000.0f;

            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsAlive() || !minion.IsVisible()) continue;
                if (IsIgnoredMinionName(ToLower(minion.GetName()))) continue;
                if (!player.IsInAttackRange(minion)) continue;

                const float dmg = DamageCalc::GetAutoAttackDamage(player, minion, false, true);
                const float predictedHP = HealthPrediction::GetPrediction(minion, predictionMs);
                if (predictedHP > 0.0f && predictedHP < dmg) {
                    return true;
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
                Globals::base + Offset::Function::IssueOrderCore);

            // IMPORTANT: local copy of position!
            Vec3 localPos = pos;
            uintptr_t targetAddr = (target && target->IsValid()) ? target->address : 0;
            bool isAttack = (order == 3);

            // Global anti-spam guard for raw order path (extra safety against disconnects).
            static int lastIssueTick = 0;
            static int lastIssueOrder = -1;
            static uintptr_t lastIssueTarget = 0;
            static Vec3 lastIssuePos;
            const int nowTick = Game::GetTickCount();
            const int ping = std::max(0, (int)Game::GetPing());
            const int minGap = (order == 3 ? 150 : 100) + std::min(80, ping / 2);  // Anti-crash: raised from 70/40
            if (nowTick - lastIssueTick < minGap) {
                const bool sameOrder = (order == lastIssueOrder);
                const bool sameTarget = (targetAddr != 0 && targetAddr == lastIssueTarget);
                const bool samePos = (localPos.IsValid() && lastIssuePos.IsValid() &&
                                      localPos.Distance2D(lastIssuePos) <= 12.0f);
                if (sameOrder && (sameTarget || samePos || order == (int)OrderType::Stop)) {
                    return;
                }
            }

            // Chimera pattern: run mainloop cleanup, then write IssueOrderFlag = order + 17.
            Bypass::PrepareIssueOrder(order);

            IssueOrderExecute(trampoline, fn, player.address, order, &localPos, targetAddr, isAttack);
            lastIssueTick = nowTick;
            lastIssueOrder = order;
            lastIssueTarget = targetAddr;
            lastIssuePos = localPos;
        }

        // ====================================================================
        // Actions
        // ====================================================================

        static void Attack(GameObject& target) {
            const int nowTick = Game::GetTickCount();
            const int minOrderGap = 150 + std::min(80, (int)Game::GetPing());  // Anti-crash: raised from 70

            if (nowTick < BlockOrdersUntilTick) return;
            if (nowTick - LastAttackCommandTick < minOrderGap) return;
            if (!CanAttack() || !target.IsValid()) return;
            if (!AttackState) return; // BlockOrders: attack disabled by script

            // Fire BeforeAttack event — scripts can cancel by setting Process=false
            OrbwalkingActionArgs beforeArgs;
            beforeArgs.Type = OrbwalkingType::BeforeAttack;
            beforeArgs.Target = target;
            beforeArgs.Sender = GameObjects::Player;
            if (!InvokeBeforeAttack(beforeArgs)) return; // Cancelled
            if (!InvokeAction(beforeArgs)) return; // Cancelled

            // TargetSwitch event
            if (LastTarget.IsValid() && LastTarget.GetNetId() != target.GetNetId()) {
                OrbwalkingActionArgs switchArgs;
                switchArgs.Type = OrbwalkingType::TargetSwitch;
                switchArgs.Target = target;
                switchArgs.Sender = GameObjects::Player;
                InvokeAction(switchArgs);
            }

            IssueOrder(3, target.GetPosition(), &target);
            LastAttackTime = Game::GetTime();
            LastTarget = target;
            LockedTarget = target;
            MissileLaunched = false;
            LastAttackCommandTick = nowTick;
            BlockOrdersUntilTick = nowTick + minOrderGap;
        }

        static void MoveTo(Vec3 pos) {
            const int nowTick = Game::GetTickCount();
            const int minOrderGap = 100 + std::min(80, (int)Game::GetPing());  // Anti-crash: raised from 70
            const int menuMoveDelayMs = MenuSlider("delayMovement", 0, "advanced");

            if (nowTick < BlockOrdersUntilTick) return;
            if (nowTick - LastMovementOrderTick < menuMoveDelayMs) return;
            if (nowTick - LastMovementOrderTick < minOrderGap) return;
            if (!CanMove()) return;
            if (!MovementState) return; // BlockOrders: movement disabled by script

            auto& player = GameObjects::Player;

            // EnsoulSharp: high-AS movement limiter
            if (MenuBool("miscAttackSpeed", true, "advanced") &&
                player.GetAttackDelay() < (1.0f / 2.6f) &&
                (AutoAttackCounter % 3) != 0 &&
                !CanMove(0.5f, true)) {
                return;
            }

            Vec3 playerPos = player.GetPosition();
            Vec3 finalPos = pos;

            // Hold position check: don't move if cursor very close
            float distToCursor = playerPos.Distance2D(finalPos);
            if (distToCursor <= HoldRadius + player.GetBoundingRadius()) {
                if (player.GetPathLength() > 0) {
                    OrbwalkingActionArgs stopArgs;
                    stopArgs.Type = OrbwalkingType::StopMovement;
                    stopArgs.Sender = player;
                    stopArgs.Position = playerPos;
                    if (InvokeAction(stopArgs)) {
                        IssueOrder((int)OrderType::Stop, stopArgs.Position);
                        LastMoveTime = Game::GetTime() - 0.07f;
                        LastMovementOrderTick = nowTick - 70;
                    }
                }
                return;
            }

            if (MovementMaximumDistance > 0.0f && distToCursor > MovementMaximumDistance) {
                finalPos = playerPos.Extend(finalPos, MovementMaximumDistance);
                distToCursor = playerPos.Distance2D(finalPos);
            }

            if (MovementRandomize && distToCursor > 350.0f) {
                const float angle = ((float)rand() / (float)RAND_MAX) * 6.28318530718f;
                const float radius = player.GetBoundingRadius() * 0.5f;
                finalPos.x += radius * cosf(angle);
                finalPos.z += radius * sinf(angle);
            }

            float time = Game::GetTime();
            if (time < LastMoveTime + ClickDelay) return;

            // Fire Movement event — scripts can cancel
            OrbwalkingActionArgs moveArgs;
            moveArgs.Type = OrbwalkingType::Movement;
            moveArgs.Position = finalPos;
            moveArgs.Sender = player;
            if (!InvokeAction(moveArgs)) {
                // Cancelled — fire StopMovement event
                OrbwalkingActionArgs stopArgs;
                stopArgs.Type = OrbwalkingType::StopMovement;
                stopArgs.Sender = player;
                InvokeAction(stopArgs);
                return;
            }

            IssueOrder(2, moveArgs.Position);
            LastMoveTime = time;
            LastMovementOrderTick = nowTick;
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
            SyncConfigFromMenu();
            if (!Enabled) return OrbwalkingMode::None;

            if (MenuKeyActive("comboKey")) return OrbwalkingMode::Combo;
            if (MenuKeyActive("hybridKey")) return OrbwalkingMode::Harass;
            if (MenuKeyActive("laneclearKey")) return OrbwalkingMode::LaneClear;
            if (MenuKeyActive("lasthitKey")) return OrbwalkingMode::LastHit;
            if (MenuKeyActive("fleeKey")) return OrbwalkingMode::Flee;
            return OrbwalkingMode::None;
        }

        // ====================================================================
        // Mode Behaviors
        // ====================================================================

        static std::string ToLower(std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                return (char)std::tolower(c);
            });
            return s;
        }

        static bool IsNameInList(const std::string& lowerName, const char* const* names, size_t count) {
            for (size_t i = 0; i < count; i++) {
                if (lowerName == names[i]) return true;
            }
            return false;
        }

        static bool IsIgnoredMinionName(const std::string& lowerName) {
            static const char* const kIgnored[] = {
                "jarvanivstandard"
            };
            return IsNameInList(lowerName, kIgnored, sizeof(kIgnored) / sizeof(kIgnored[0]));
        }

        static bool IsCloneName(const std::string& lowerName) {
            static const char* const kClones[] = {
                "leblanc",
                "monkeyking",
                "neeko",
                "shaco"
            };
            return IsNameInList(lowerName, kClones, sizeof(kClones) / sizeof(kClones[0]));
        }

        static bool IsSpecialMinionName(const std::string& lowerName) {
            static const char* const kSpecial[] = {
                "annietibbers", "elisespiderling", "heimertyellow", "heimertblue",
                "ivernminion", "malzaharvoidling", "shacobox", "teemomushroom",
                "yorickghoulmelee", "yorickbigghoul", "zyrathornplant", "zyragraspingplant"
            };
            return IsNameInList(lowerName, kSpecial, sizeof(kSpecial) / sizeof(kSpecial[0]));
        }

        static bool IsValidUnit(const GameObject& unit, float range = 0.0f) {
            if (!unit.IsValid()) return false;
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return false;
            const float checkRange = range > 0.0f ? range : player.GetRealAttackRange() + 65.0f;
            // Distance check FIRST — cheapest, eliminates most units
            if (player.DistanceTo(unit) > checkRange) return false;
            if (!unit.IsAlive()) return false;
            if (!unit.IsVisible()) return false;

            // Neutral units can be targetable even if IsTargetable offset is unreliable.
            const bool isNeutral = unit.GetTeam() == GameObjectTeam::Neutral;
            if (!isNeutral && !unit.IsTargetable()) return false;
            // NOTE: Removed string allocation here — ignore check happens at list build time
            return true;
        }

        static std::vector<GameObject> OrderEnemyMinions(const std::vector<GameObject>& minions) {
            std::vector<GameObject> out = minions;
            auto rank = [](const GameObject& m) {
                MinionType type = m.GetMinionType();
                if (type == MinionType::Super) return 0;
                if (type == MinionType::Cannon) return 1;
                return 2;
            };

            std::sort(out.begin(), out.end(), [&](const GameObject& a, const GameObject& b) {
                const int ra = rank(a);
                const int rb = rank(b);
                if (ra != rb) return ra < rb;
                if (fabsf(a.GetHealth() - b.GetHealth()) > 0.01f) return a.GetHealth() < b.GetHealth();
                return a.GetMaxHealth() > b.GetMaxHealth();
            });
            return out;
        }

        static std::vector<GameObject> OrderJungleMinions(const std::vector<GameObject>& minions) {
            std::vector<GameObject> out = minions;
            const bool prioritizeSmall = MenuBool("prioritizeSmallJungle", false, "advanced");
            std::sort(out.begin(), out.end(), [&](const GameObject& a, const GameObject& b) {
                return prioritizeSmall ? (a.GetMaxHealth() < b.GetMaxHealth())
                                       : (a.GetMaxHealth() > b.GetMaxHealth());
            });
            return out;
        }

        static std::vector<GameObject> GetMinions(OrbwalkingMode mode) {
            const bool includeLaneMinions = mode != OrbwalkingMode::Combo;
            const bool attackWards = MenuBool("attackWards", false, "advanced");
            const bool attackClones = MenuBool("attackClones", false, "advanced");
            const bool attackSpecialMinions = MenuBool("attackSpecialMinions", true, "advanced");
            const bool prioritizeWards = MenuBool("prioritizeWards", false, "advanced");
            const bool prioritizeSpecialMinions = MenuBool("prioritizeSpecialMinions", false, "advanced");

            std::vector<GameObject> minionList;
            std::vector<GameObject> specialList;
            std::vector<GameObject> cloneList;
            std::vector<GameObject> wardList;

            const float checkRange = GameObjects::Player.GetRealAttackRange() + 65.0f;
            for (auto& minion : GameObjects::EnemyMinions) {
                if (!IsValidUnit(minion, checkRange)) continue;
                // EnemyMinions is already pre-filtered by GameObjects::Update()
                // — no need to call IsMinion() again (was causing drops when
                // RuntimeAPI::CompareTypeFlags failed under SEH).
                if (includeLaneMinions) {
                    minionList.push_back(minion);
                }
            }

            for (auto& pet : GameObjects::Pets) {
                if (!pet.IsValid()) continue;
                if (pet.GetTeam() == GameObjects::Player.GetTeam()) continue;
                if (!IsValidUnit(pet, checkRange)) continue;

                const std::string lower = ToLower(pet.GetName());
                if (attackSpecialMinions && IsSpecialMinionName(lower)) {
                    specialList.push_back(pet);
                } else if (attackClones && IsCloneName(lower)) {
                    cloneList.push_back(pet);
                }
            }

            if (includeLaneMinions) {
                minionList = OrderEnemyMinions(minionList);
                // JungleMinions is already filtered at source (GameObjects::Update)
                // — only real jungle monsters, no plants/decorations.
                // Just check range + alive, no redundant RuntimeAPI calls.
                std::vector<GameObject> jungle;
                for (auto& j : GameObjects::JungleMinions) {
                    if (!IsValidUnit(j, checkRange)) continue;
                    jungle.push_back(j);
                }
                auto orderedJungle = OrderJungleMinions(jungle);
                minionList.insert(minionList.end(), orderedJungle.begin(), orderedJungle.end());
            }

            if (attackWards) {
                for (auto& ward : GameObjects::EnemyWards) {
                    if (!IsValidUnit(ward, checkRange)) continue;
                    wardList.push_back(ward);
                }
            }

            std::vector<GameObject> finalMinionList;
            if (attackWards && prioritizeWards && attackSpecialMinions && prioritizeSpecialMinions) {
                finalMinionList.insert(finalMinionList.end(), wardList.begin(), wardList.end());
                finalMinionList.insert(finalMinionList.end(), specialList.begin(), specialList.end());
                finalMinionList.insert(finalMinionList.end(), minionList.begin(), minionList.end());
            } else if (attackSpecialMinions && prioritizeSpecialMinions) {
                finalMinionList.insert(finalMinionList.end(), specialList.begin(), specialList.end());
                finalMinionList.insert(finalMinionList.end(), minionList.begin(), minionList.end());
                finalMinionList.insert(finalMinionList.end(), wardList.begin(), wardList.end());
            } else if (attackWards && prioritizeWards) {
                finalMinionList.insert(finalMinionList.end(), wardList.begin(), wardList.end());
                finalMinionList.insert(finalMinionList.end(), minionList.begin(), minionList.end());
                finalMinionList.insert(finalMinionList.end(), specialList.begin(), specialList.end());
            } else {
                finalMinionList.insert(finalMinionList.end(), minionList.begin(), minionList.end());
                finalMinionList.insert(finalMinionList.end(), specialList.begin(), specialList.end());
                finalMinionList.insert(finalMinionList.end(), wardList.begin(), wardList.end());
            }

            if (MenuBool("attackBarrels", false, "advanced")) {
                for (auto& j : GameObjects::JungleMinions) {
                    if (!IsValidUnit(j, checkRange)) continue;
                    if (ToLower(j.GetName()) == "gangplankbarrel" && j.GetHealth() <= 1.0f) {
                        finalMinionList.push_back(j);
                    }
                }
            }

            if (attackClones) {
                finalMinionList.insert(finalMinionList.end(), cloneList.begin(), cloneList.end());
            }

            // Removed redundant string-heavy erase loop.
            // Ignored minions are already filtered out in IsValidUnit().
            // JarvanIV standard filtered at list build time.

            return finalMinionList;
        }

        static bool ShouldWaitUnderTurret(const GameObject* nonKillableMinion = nullptr) {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return false;

            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsValid() || !minion.IsAlive()) continue;
                if (!player.IsInAttackRange(minion)) continue;
                if (nonKillableMinion && nonKillableMinion->IsValid() &&
                    nonKillableMinion->GetNetId() == minion.GetNetId()) {
                    continue;
                }

                const float predictMs = (player.GetAttackDelay() * 1000.0f) +
                    AutoAttackUtil::GetTimeToHit(player, minion) +
                    (FarmDelay * 1000.0f);
                const float pred = HealthPrediction::GetPrediction(minion, predictMs);
                if (pred < player.GetAutoAttackDamage(minion)) {
                    return true;
                }
            }

            return false;
        }

        static GameObject GetTarget(OrbwalkingMode mode) {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return GameObject();

            if (LockedTarget.IsValid()) {
                if (LockedTarget.IsAlive() &&
                    player.IsInAttackRange(LockedTarget) &&
                    !CanAttack()) {
                    return LockedTarget;
                }
            }

            const float range = player.GetRealAttackRange() + 65.0f;
            const bool prioritizeFarm = MenuBool("prioritizeFarm", true, "advanced");

            if ((mode == OrbwalkingMode::Harass || mode == OrbwalkingMode::LaneClear) && !prioritizeFarm) {
                GameObject hero = TargetSelector::GetTarget(range, TSMode);
                if (hero.IsValid() && player.IsInAttackRange(hero)) {
                    return hero;
                }
            }

            std::vector<GameObject> minions;
            if (mode != OrbwalkingMode::None) {
                minions = GetMinions(mode);
            }

            // ===============================
            // FIX: sort minions by predicted hp
            // ===============================
            std::sort(minions.begin(), minions.end(),
            [&](const GameObject& a, const GameObject& b)
            {
                if (!a.IsValid() || !b.IsValid())
                    return false;

                const float timeA =
                    AutoAttackUtil::GetTimeToHit(GameObjects::Player, a) +
                    (FarmDelay * 1000.0f);

                const float timeB =
                    AutoAttackUtil::GetTimeToHit(GameObjects::Player, b) +
                    (FarmDelay * 1000.0f);

                float hpA = HealthPrediction::GetPrediction(a, timeA);
                float hpB = HealthPrediction::GetPrediction(b, timeB);

                return hpA < hpB;
            });

            // Killable minion pass: LaneClear/Harass(=Hybrid)/LastHit
            if (mode == OrbwalkingMode::LaneClear || mode == OrbwalkingMode::Harass || mode == OrbwalkingMode::LastHit) {
                for (auto& minion : minions) {
                    if (!minion.IsValid() || !minion.IsAlive()) continue;
                    if (!player.IsInAttackRange(minion)) continue;

                    const float predictMs =
                        AutoAttackUtil::GetTimeToHit(player, minion) +
                        (FarmDelay * 1000.0f);

                    const float predHp =
                        HealthPrediction::GetPrediction(minion, predictMs);

                    const float dmg =
                        player.GetAutoAttackDamage(minion);

                    if (predHp > 0.0f && predHp <= dmg) {
                        return minion;
                    }

                    if (minion.GetMaxHealth() <= 10.0f) {
                        if (minion.GetHealth() <= 1.0f) {
                            return minion;
                        }
                    } else {
                        if (predHp <= 0.0f) {
                            OrbwalkingActionArgs nonKillable;
                            nonKillable.Type = OrbwalkingType::NonKillableMinion;
                            nonKillable.Target = minion;
                            nonKillable.Position = minion.GetPosition();
                            nonKillable.Process = true;
                            InvokeAction(nonKillable);
                        }
                        if (predHp > 0.0f && predHp < dmg) {
                            return minion;
                        }
                    }
                }
            }

            // Forced target
            if (ForceTarget.IsValidTarget(range, true, player.GetPosition()) && player.IsInAttackRange(ForceTarget)) {
                return ForceTarget;
            }

            // Objectives
            if (mode == OrbwalkingMode::LaneClear &&
                (!MenuBool("prioritizeMinions", false, "advanced") || minions.empty())) {
                for (auto& turret : GameObjects::EnemyTurrets) {
                    if (turret.IsValid() && turret.IsAlive() && player.IsInAttackRange(turret)) return turret;
                }
                for (auto& inhib : GameObjects::EnemyInhibitors) {
                    if (inhib.IsValid() && inhib.IsAlive() && player.IsInAttackRange(inhib)) return inhib;
                }
                for (auto& nexus : GameObjects::EnemyNexus) {
                    if (nexus.IsValid() && nexus.IsAlive() && player.IsInAttackRange(nexus)) return nexus;
                }
            }

            // Champions
            if (mode != OrbwalkingMode::LastHit) {
                GameObject hero = TargetSelector::GetTarget(range, TSMode);
                if (hero.IsValid() && player.IsInAttackRange(hero)) return hero;
            }

            // Jungle in farm modes (priority by camp size, fast bitmask checks)
            if (mode == OrbwalkingMode::LaneClear || mode == OrbwalkingMode::Harass) {
                GameObject bestJungleTarget;
                float bestPriority = -1.0f;

                auto evalJungle = [&](const GameObject& minion) {
                    if (!minion.IsValid() || !minion.IsAlive()) return;
                    if (!player.IsInAttackRange(minion)) return;
                    if (!minion.IsJungleMonster()) return;

                    float priority = minion.GetMaxHealth();
                    if (minion.IsBaron()) priority += 100000.0f;
                    else if (minion.IsDragon()) priority += 50000.0f;
                    else if (priority > 3000.0f) priority += 30000.0f;

                    if (priority > bestPriority) {
                        bestPriority = priority;
                        bestJungleTarget = minion;
                    }
                };

                for (auto& minion : minions) {
                    evalJungle(minion);
                }

                if (!bestJungleTarget.IsValid()) {
                    for (auto& minion : GameObjects::JungleMinions) {
                        evalJungle(minion);
                    }
                }

                if (bestJungleTarget.IsValid()) {
                    return bestJungleTarget;
                }
            }

            // Under turret farming
            if (mode == OrbwalkingMode::LaneClear || mode == OrbwalkingMode::Harass || mode == OrbwalkingMode::LastHit) {
                std::vector<GameObject> turretMinions;
                for (auto& minion : minions) {
                    if (!minion.IsValid() || !minion.IsAlive()) continue;
                    if (minion.GetTeam() == GameObjectTeam::Neutral) continue;
                    if (!minion.IsMinion()) continue;
                    if (!player.IsInAttackRange(minion)) continue;
                    if (!GameObjects::IsUnderAllyTurret(minion.GetPosition())) continue;
                    turretMinions.push_back(minion);
                }

                if (!turretMinions.empty()) {
                    GameObject turretAggroMinion;
                    for (auto& minion : turretMinions) {
                        if (HealthPrediction::HasTurretAggro(minion)) {
                            turretAggroMinion = minion;
                            break;
                        }
                    }

                    if (turretAggroMinion.IsValid()) {
                        const float myDamage = player.GetAutoAttackDamage(turretAggroMinion);
                        const float predictMs = AutoAttackUtil::GetTimeToHit(player, turretAggroMinion) + (FarmDelay * 1000.0f);
                        const float pred = HealthPrediction::GetPrediction(turretAggroMinion, predictMs);
                        if (pred > 0.0f && pred <= myDamage) {
                            return turretAggroMinion;
                        }

                        if (ShouldWaitUnderTurret(&turretAggroMinion)) {
                            return GameObject();
                        }

                        GameObject aggroTurret;
                        for (auto& turret : GameObjects::AllyTurrets) {
                            if (!turret.IsValid() || !turret.IsAlive()) continue;
                            if (turret.IsValidTarget(950.0f, false, turretAggroMinion.GetPosition())) {
                                aggroTurret = turret;
                                break;
                            }
                        }

                        if (aggroTurret.IsValid()) {
                            for (auto& minion : turretMinions) {
                                if (minion.GetNetId() == turretAggroMinion.GetNetId()) continue;
                                if (HealthPrediction::HasMinionAggro(minion)) continue;
                                const int turretDamage = std::max(1, (int)aggroTurret.GetAutoAttackDamage(minion));
                                const int myDamageInt = std::max(1, (int)player.GetAutoAttackDamage(minion));
                                if (((int)minion.GetHealth() % turretDamage) > myDamageInt) {
                                    return minion;
                                }
                            }
                        }
                    } else {
                        if (ShouldWaitUnderTurret()) {
                            return GameObject();
                        }

                        for (auto& minion : turretMinions) {
                            if (HealthPrediction::HasMinionAggro(minion)) continue;
                            GameObject turret;
                            for (auto& allyTurret : GameObjects::AllyTurrets) {
                                if (!allyTurret.IsValid() || !allyTurret.IsAlive()) continue;
                                if (allyTurret.IsValidTarget(950.0f, false, minion.GetPosition())) {
                                    turret = allyTurret;
                                    break;
                                }
                            }
                            if (!turret.IsValid()) continue;
                            const int turretDamage = std::max(1, (int)turret.GetAutoAttackDamage(minion));
                            const int myDamageInt = std::max(1, (int)player.GetAutoAttackDamage(minion));
                            if (((int)minion.GetHealth() % turretDamage) > myDamageInt) {
                                return minion;
                            }
                        }
                    }

                    return GameObject();
                }
            }

            // Lane clear non-last-hit push target
            if (mode == OrbwalkingMode::LaneClear) {
                if (ShouldWait()) {
                    if (HealthPrediction::LaneClearWait()) {
                        return GameObject();
                    }
                }

                if (LaneClearMinion.IsValid() && player.IsInAttackRange(LaneClearMinion)) {
                    if (LaneClearMinion.GetMaxHealth() <= 10.0f) {
                        return LaneClearMinion;
                    }

                    const float pred = HealthPrediction::GetPrediction(
                        LaneClearMinion,
                        (player.GetAttackDelay() * 2000.0f) + (FarmDelay * 1000.0f));
                    if (pred >= 2.0f * player.GetAutoAttackDamage(LaneClearMinion) ||
                        fabsf(pred - LaneClearMinion.GetHealth()) < 0.001f) {
                        return LaneClearMinion;
                    }
                }

                for (auto& minion : minions)
                {
                    if (minion.GetTeam() == GameObjectTeam::Neutral)
                        continue;

                    if (!GameObjects::Player.IsInAttackRange(minion))
                        continue;

                    float pred =
                        HealthPrediction::GetPrediction(
                            minion,
                            (GameObjects::Player.GetAttackDelay() * 2000.0f) +
                            (FarmDelay * 1000.0f));

                    float dmg =
                        GameObjects::Player.GetAutoAttackDamage(minion);

                    if (pred >= 2.0f * dmg)
                    {
                        LaneClearMinion = minion;
                        return minion;
                    }
                }
            }

            // Combo fallback: special minion when no enemy heroes nearby
            if (mode == OrbwalkingMode::Combo) {
                if (!minions.empty()) {
                    bool enemyNearby = false;
                    for (auto& enemy : GameObjects::EnemyHeroes) {
                        if (enemy.IsValidTarget(enemy.GetRealAttackRange() * 2.0f, true, player.GetPosition())) {
                            enemyNearby = true;
                            break;
                        }
                    }
                    if (!enemyNearby) {
                        return minions.front();
                    }
                }
            }

            return GameObject();
        }

        static void OrbwalkMode(OrbwalkingMode mode) {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return;
            if (!player.IsAlive()) return;

            GameObject target = GetTarget(mode);
            LastTarget = target;  // Cache for debug overlay
            if (target.IsValid() && target.IsAlive() && CanAttack() && AttackState && player.IsInAttackRange(target)) {
                Attack(target);
            }

            if (CanMove() && MovementState) {
                MoveToMouse();
            }
        }

        static void Combo() { OrbwalkMode(OrbwalkingMode::Combo); }
        static void LaneClear() { OrbwalkMode(OrbwalkingMode::LaneClear); }
        static void LastHit() { OrbwalkMode(OrbwalkingMode::LastHit); }
        static void Harass() { OrbwalkMode(OrbwalkingMode::Harass); }

        static void OnRender() {
            if (PluginOverrideActive) return;
            auto& player = GameObjects::Player;
            if (!player.IsValid() || !player.IsAlive()) return;

            if (DrawAARange) {
                Drawing::DrawCircle(player.GetPosition(), player.GetRealAttackRange(), IM_COL32(30, 150, 230, 170), 1.4f);
            }

            if (DrawExtraHoldPosition) {
                Drawing::DrawCircle(player.GetPosition(), player.GetBoundingRadius() + HoldRadius, IM_COL32(190, 100, 255, 120), 1.0f);
            }

            if (DrawAARangeEnemy) {
                for (auto& enemy : GameObjects::EnemyHeroes) {
                    if (!enemy.IsAlive() || !enemy.IsVisible()) continue;
                    Drawing::DrawCircle(enemy.GetPosition(), enemy.GetRealAttackRange(), IM_COL32(100, 200, 255, 90), 1.0f);
                }
            }

            if (DrawKillableMinion) {
                for (auto& minion : GameObjects::EnemyMinions) {
                    if (!minion.IsAlive() || !minion.IsVisible()) continue;
                    // ONLY draw for real lane minions — not plants, wards, special objects
                    if (!minion.IsMinion()) continue;
                    if (minion.GetMaxHealth() <= 6.0f) continue; // Skip plants/objects
                    if (player.DistanceTo(minion) > player.GetRealAttackRange() * 1.5f) continue;
                    float dmg = DamageCalc::GetAutoAttackDamage(player, minion, false, true);
                    if (dmg <= 0.01f) continue;
                    float alpha = 220.0f;
                    if (DrawKillableMinionFade && dmg > 1.0f) {
                        const float t = std::clamp(minion.GetHealth() / dmg, 0.0f, 1.0f);
                        alpha = 255.0f * (1.0f - t);
                    }
                    if (minion.GetHealth() <= dmg * (DrawKillableMinionFade ? 2.0f : 1.0f)) {
                        Drawing::DrawCircle(minion.GetPosition(), minion.GetBoundingRadius() * 2.0f, IM_COL32(0, 255, 0, (int)alpha), 1.4f);
                    }
                }
            }

            // ---- LaneClear Debug Overlay (throttled to avoid FPS drop) ----
            if (DebugLaneClear && (ActiveMode == OrbwalkingMode::LaneClear || ActiveMode == OrbwalkingMode::LastHit)) {
                static int s_debugFrameCounter = 0;
                if (++s_debugFrameCounter >= 3) { // Only render every 3 frames
                    s_debugFrameCounter = 0;
                    DrawLaneClearDebugOverlay();
                }
            }
        }

        // ====================================================================
        // LaneClear Debug: Shows what orbwalker is targeting and why
        // Uses cached LastTarget to avoid calling GetTarget() again (expensive!)
        // ====================================================================
        static void DrawLaneClearDebugOverlay() {
            const auto& player = GameObjects::Player;
            if (!player.IsValid() || !player.IsAlive()) return;

            const float aaRange = player.GetRealAttackRange() + 65.0f;
            const float now = Game::GetTime();
            static float lastDebugLogTime = 0.0f;
            const bool shouldLog = (now - lastDebugLogTime > 2.0f);

            float px = 10.0f;
            float py = 350.0f;
            float lh = 14.0f;
            int line = 0;
            char buf[300] = {};

            auto drawDbgLine = [&](const char* text, ImU32 color = IM_COL32(230, 230, 230, 255)) {
                Drawing::DrawScreenText(Vec2(px, py + lh * line++), text, color);
            };

            const char* modeName = (ActiveMode == OrbwalkingMode::LaneClear) ? "LaneClear" : "LastHit";
            snprintf(buf, sizeof(buf), "=== Orbwalker [%s] Debug ===", modeName);
            drawDbgLine(buf, IM_COL32(255, 220, 80, 255));

            // Use cached LastTarget instead of calling GetTarget() again (FPS fix!)
            GameObject currentTarget = LastTarget;
            if (currentTarget.IsValid() && currentTarget.IsAlive() && player.IsInAttackRange(currentTarget)) {
                std::string targetName = currentTarget.GetName();
                if (targetName.empty()) {
                    char tmp[64];
                    snprintf(tmp, sizeof(tmp), "(noname_netId=%d)", currentTarget.GetNetId());
                    targetName = tmp;
                }
                std::string targetLower = ToLower(targetName);
                const char* targetType = "UNKNOWN";
                bool isPlant = currentTarget.IsPlant() || JungleUtils::IsJunglePlantName(targetLower);
                if (isPlant) {
                    targetType = "PLANT (!)";
                } else if (currentTarget.IsHero()) {
                    targetType = "HERO";
                } else if (currentTarget.IsTurret()) {
                    targetType = "TURRET";
                } else if (currentTarget.GetTeam() == GameObjectTeam::Neutral) {
                    JungleType jType = JungleUtils::GetJungleType(currentTarget);
                    switch (jType) {
                    case JungleType::Legendary: targetType = "JUNGLE(Epic)"; break;
                    case JungleType::Large: targetType = "JUNGLE(Large)"; break;
                    case JungleType::Small: targetType = "JUNGLE(Small)"; break;
                    default: targetType = "JUNGLE(?)"; break;
                    }
                } else if (currentTarget.IsMinion()) {
                    targetType = "MINION";
                }

                float aaDmg = player.GetAutoAttackDamage(currentTarget);
                float predMs = (player.GetAttackDelay() * 2000.0f) + (Orbwalker::FarmDelay * 1000.0f);
                float predHP = HealthPrediction::GetPrediction(currentTarget, predMs);

                snprintf(buf, sizeof(buf), "TARGET: [%s] %s", targetType, targetName.c_str());
                ImU32 targetColor = isPlant ? IM_COL32(255, 60, 60, 255) : IM_COL32(100, 255, 120, 255);
                drawDbgLine(buf, targetColor);

                snprintf(buf, sizeof(buf), "  hp=%.0f/%.0f  aa_dmg=%.0f  pred_hp=%.0f  kill=%s",
                    currentTarget.GetHealth(), currentTarget.GetMaxHealth(),
                    aaDmg, predHP, (predHP > 0 && predHP < aaDmg) ? "YES" : "no");
                drawDbgLine(buf, IM_COL32(180, 200, 255, 240));

                snprintf(buf, sizeof(buf), "  team=%d  netId=%d  dist=%.0f  tgt=%d",
                    (int)currentTarget.GetTeam(), currentTarget.GetNetId(),
                    player.DistanceTo(currentTarget),
                    currentTarget.IsTargetable() ? 1 : 0);
                drawDbgLine(buf, IM_COL32(160, 180, 220, 220));

                // Draw circle around target
                Drawing::DrawCircle(currentTarget.GetPosition(),
                    currentTarget.GetBoundingRadius() * 2.0f + 20.0f,
                    isPlant ? IM_COL32(255, 50, 50, 230) : IM_COL32(50, 255, 100, 220), 2.5f);

                if (shouldLog) {
                    DebugConsole::LogTagged("LC-Target", "[%s] %s -> [%s] hp=%.0f/%.0f dmg=%.0f pred=%.0f tgtable=%d",
                        modeName, targetType, targetName.c_str(),
                        currentTarget.GetHealth(), currentTarget.GetMaxHealth(), aaDmg, predHP,
                        currentTarget.IsTargetable() ? 1 : 0);
                }
            } else {
                drawDbgLine("TARGET: (none)", IM_COL32(200, 200, 200, 180));
            }

            // Show nearby jungle units in range (where plant confusion happens)
            drawDbgLine("--- Nearby Jungle ---", IM_COL32(180, 180, 200, 200));
            int shown = 0;
            for (auto& j : GameObjects::JungleMinions) {
                if (!j.IsValid() || !j.IsAlive()) continue;
                if (player.DistanceTo(j) > aaRange * 1.3f) continue;
                if (shown >= 6) break;

                std::string jungleName = j.GetName();
                std::string jLower = ToLower(jungleName);
                bool isPlant = j.IsPlant() || JungleUtils::IsJunglePlantName(jLower) || (j.GetMaxHealth() <= 6.0f);
                bool inRange = player.IsInAttackRange(j);
                const char* typeStr = isPlant ? "PLANT" : "MON";

                snprintf(buf, sizeof(buf), "  [%s] %s hp=%.0f/%.0f %s",
                    typeStr, jungleName.c_str(), j.GetHealth(), j.GetMaxHealth(),
                    inRange ? "IN" : "out");
                drawDbgLine(buf, isPlant ? IM_COL32(255, 100, 100, 220) : IM_COL32(100, 200, 255, 210));

                if (isPlant && inRange) {
                    Drawing::DrawCircle(j.GetPosition(), 30.0f, IM_COL32(255, 50, 50, 200), 2.0f);
                }
                ++shown;
            }

            if (shouldLog) {
                lastDebugLogTime = now;
            }
        }

        // ====================================================================
        // Main Update — call from render loop
        // ====================================================================

        static void OnUpdate() {
            EnsureMenu();
            SyncConfigFromMenu();

            if (PluginOverrideActive) return;

            auto& player = GameObjects::Player;
            if (!player.IsValid() || !player.IsAlive()) {
                ActiveMode = OrbwalkingMode::None;
                return;
            }

            if (!Enabled || !Game::ShouldProcessInput()) {
                ActiveMode = OrbwalkingMode::None;
                return;
            }

            ActiveMode = GetMode();

            switch (ActiveMode) {
            case OrbwalkingMode::Combo:     Combo();       break;
            case OrbwalkingMode::LaneClear: LaneClear();   break;
            case OrbwalkingMode::LastHit:   LastHit();     break;
            case OrbwalkingMode::Harass:    Harass();      break;
            case OrbwalkingMode::Flee:      MoveToMouse(); break;
            default: break;
            }
        }
    };

} // namespace SDK

