#pragma once

#include "../../../core/CrashTelemetry.h"
#include "../../../imgui/imgui.h"
#include "../../../menu/MenuUI.h"
#include "../../UI/Drawing.h"
#include "../../Enumerations/OrbwalkerMode.h"
#include "../../Core/Game.h"
#include "../../Core/Objects.h"
#include "OrbwalkerBase.h"
#include "OrbwalkerSelector.h"
#include "../TargetSelector/TargetSelector.h"
#include "../../../core/CoreSpellCastInfo.h"
#include "../../UI/UI.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <Windows.h>

namespace SDK {

class Orbwalker : public OrbwalkerBase {
public:

  static void TraceStage(const char* stage) {
    CrashTelemetry::SetStage(stage);
  }

  static void AppendDebugLine(const char* text) {
    if (!text || !*text) {
      return;
    }

    HANDLE hFile = CreateFileA(
      "C:\\Users\\Public\\ns_orbwalker_debug.txt",
      FILE_APPEND_DATA,
      FILE_SHARE_READ,
      nullptr,
      OPEN_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
      return;
    }

    DWORD written = 0;
    WriteFile(hFile, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
    CloseHandle(hFile);
  }

  static void TraceRuntime(const AIHeroClient& player,
                           const AIBaseClient& target,
                           bool canAttackNow,
                           bool canMoveNow,
                           bool timingReady,
                           bool nativeCanAttack,
                           bool targetInRange) {
    static DWORD lastLogTick = 0;
    const DWORD now = GetTickCount();
    if ((now - lastLogTick) < 2000) {
      return;
    }
    lastLogTick = now;

    const auto enemyHeroCount = ObjectManager::EnemyHeroes().size();
    const auto enemyMinionCount = ObjectManager::EnemyMinions().size();
    const auto jungleCount = ObjectManager::JungleMinions().size();
    const auto allObjectCount = ObjectManager::AllObjects().size();

    char buf[512] = {};
    std::snprintf(
      buf, sizeof(buf),
      "[NightSharp][Orbwalker] mode=%d enabled=%d atkState=%d moveState=%d timing=%d nativeAtk=%d canAtk=%d canMove=%d target=%d inRange=%d targetNetId=%d atkIssued=%d moveIssued=%d counts=%llu/%llu/%llu/%llu playerPos=%.1f %.1f %.1f\r\n",
      static_cast<int>(Instance().ActiveMode),
      IsEnabled() ? 1 : 0,
      Instance().AttackState ? 1 : 0,
      Instance().MovementState ? 1 : 0,
      timingReady ? 1 : 0,
      nativeCanAttack ? 1 : 0,
      canAttackNow ? 1 : 0,
      canMoveNow ? 1 : 0,
      target.IsValid() ? 1 : 0,
      targetInRange ? 1 : 0,
      target.IsValid() ? target.NetworkId() : 0,
      Instance().s_debug.attackIssued ? 1 : 0,
      Instance().s_debug.moveIssued ? 1 : 0,
      static_cast<unsigned long long>(enemyHeroCount),
      static_cast<unsigned long long>(enemyMinionCount),
      static_cast<unsigned long long>(jungleCount),
      static_cast<unsigned long long>(allObjectCount),
      player.Position().x, player.Position().y, player.Position().z);
    AppendDebugLine(buf);
  }

  // ── Initialize menu (matching Orbwalker.cs ctor) ──
  static void Initialize() {
    if (Instance().s_initialized) return;
    Instance().s_initialized = true;

    auto root = UI::CreateMenu("orbwalker", "Orbwalker");
    Instance().s_menu = root.Raw();

    // Drawings
    auto drawings = root.AddMenu("drawings", "Drawings");
    drawings.AddBool("drawAARange", "Auto-Attack Range", true);
    drawings.AddBool("drawAARangeEnemy", "Auto-Attack Range Enemy", false);
    drawings.AddBool("drawExtraHoldPosition", "Extra Hold Position", false);
    drawings.AddBool("drawKillableMinion", "Killable Minions", false);
    drawings.AddBool("drawKillableMinionFade", "Killable Minions Fade Effect", false);
    drawings.AddBool("drawActiveMode", "Active Mode", true);

    // Advanced
    auto advanced = root.AddMenu("advanced", "Advanced");

    advanced.AddSeparator("separatorMovement", "Movement");
    advanced.AddBool("movementRandomize", "Randomize Location", true);
    advanced.AddSlider("movementExtraHold", "Extra Hold Position", 0, 0, 250);
    advanced.AddSlider("movementMaximumDistance", "Maximum Distance", 1500, 500, 1500);

    advanced.AddSeparator("separatorDelay", "Delay");
    advanced.AddSlider("delayMovement", "Movement", 35, 0, 500);
    advanced.AddSlider("delayWindup", "Windup %", 80, 0, 200);
    advanced.AddSlider("delayFarm", "Farm", 30, 0, 200);

    advanced.AddSeparator("separatorPrioritization", "Prioritization");
    advanced.AddBool("prioritizeFarm", "Farm Over Harass", true);
    advanced.AddBool("prioritizeMinions", "Minions Over Objectives", false);
    advanced.AddBool("prioritizeSmallJungle", "Small Jungle", false);
    advanced.AddBool("prioritizeWards", "Wards", false);
    advanced.AddBool("prioritizeSpecialMinions", "Special Minions", false);

    advanced.AddSeparator("separatorAttack", "Attack");
    advanced.AddBool("attackWards", "Wards", false);
    advanced.AddBool("attackBarrels", "Barrels", false);
    advanced.AddBool("attackClones", "Clones", false);
    advanced.AddBool("attackSpecialMinions", "Special Minions", true);
    advanced.AddBool("attackPlants", "Plants", false);
    advanced.AddBool("attackPets", "Pets", false);

    advanced.AddSeparator("separatorMisc", "Miscellaneous");
    advanced.AddBool("miscMissile", "Use Missile Checks", true);
    advanced.AddBool("miscAttackSpeed", "Don't Kite if Attack Speed > 2.5", true);

    auto misc = root.AddMenu("misc", "Miscellaneous");
    misc.AddBool("drawChaseRange", "Draw Chase Range", false);
    misc.AddSlider("forceChaseRange", "Force Chase Extra Range", 0, 0, 500);
    misc.AddKeyBind("forceChaseKey", "Force Chase Key", 'F', KeyBindType::Press);

    // Key Bindings
    root.AddSeparator("separatorKeys", "Key Bindings");
    root.AddKeyBind("lasthitKey", "Last Hit", 'X', KeyBindType::Press);
    root.AddKeyBind("laneclearKey", "Lane Clear", 'V', KeyBindType::Press);
    root.AddKeyBind("fastLaneClearKey", "Fast LaneClear", 'A', KeyBindType::Press);
    root.AddKeyBind("hybridKey", "Hybrid", 'C', KeyBindType::Press);
    root.AddKeyBind("comboKey", "Combo", VK_SPACE, KeyBindType::Press);
    root.AddKeyBind("comboNoMoveKey", "Combo (No Move)", 0, KeyBindType::Press);
    root.AddKeyBind("fleeKey", "Flee", 'Z', KeyBindType::Press);
    root.AddBool("enabledOption", "Enabled", true);
  }

  // ── Singleton access ──
  static Orbwalker& Instance() {
    static Orbwalker instance = {};
    return instance;
  }
  static Menu* GetMenu() { return Instance().s_menu; }
  static OrbwalkerMode GetMode() { return Instance().ActiveMode; }
  static bool IsEnabled() {
    auto* menu = Instance().s_menu;
    return menu && menu->GetBoolValue("enabledOption", true);
  }

  static bool IsComboNoMoveActive() {
    auto* menu = Instance().s_menu;
    return menu && menu->GetKeyBindValue("comboNoMoveKey", false);
  }

  static bool IsForceChaseActive() {
    auto* menu = Instance().s_menu;
    if (!menu || Instance().ActiveMode != OrbwalkerMode::Combo) {
      return false;
    }

    auto* misc = menu->GetSubMenu("misc");
    if (!misc) {
      return false;
    }

    return misc->GetKeyBindValue("forceChaseKey", false) &&
           misc->GetSliderValue("forceChaseRange", 0) > 0;
  }

  static float GetForceChaseExtraRange() {
    auto* menu = Instance().s_menu;
    if (!menu) {
      return 0.0f;
    }

    auto* misc = menu->GetSubMenu("misc");
    return misc ? static_cast<float>(misc->GetSliderValue("forceChaseRange", 0)) : 0.0f;
  }

  // ── Public API (static wrappers) ──
  static int  LastAttackTick()        { return Instance().LastAutoAttackTick; }
  static int  LastAutoAttackCommandT(){ return Instance().LastAutoAttackCommandTick; }
  static int  LastMoveTick()          { return Instance().LastMovementOrderTick; }
  static AIBaseClient GetLastTarget() { return Instance().LastTarget; }
  static int  GetTotalAutoAttacks()   { return Instance().TotalAutoAttacks; }

  static bool CanAttackNow(float extra = 0.0f) { return Instance().CanAttackFull(extra); }
  static bool CanMoveNow(float extra = 0.0f) { return Instance().CanMoveFull(extra); }
  static void ForceTarget(const AIBaseClient& t) { Instance().s_forcedTarget = t; }
  static void ClearForcedTarget() { Instance().s_forcedTarget = AIBaseClient(); }
  static AIBaseClient GetForcedTarget() { return Instance().s_forcedTarget; }
  static void ResetAutoAttackTimer() { Instance().ResetSwingTimer(); }

  // ── Block orders (matching C# BlockOrdersUntilTick) ──
  static int BlockOrdersUntilTick() { return Instance().s_blockOrdersUntil; }

  // ── Update – main orbwalker loop (called from Game::OnUpdate) ──
  static void Update() {
    __try {
      TraceStage("Orbwalker::Update::RefreshMode");
      Instance().RefreshMode();

      TraceStage("Orbwalker::Update::PreChecks");
      Instance().s_debug.attackIssued = false;
      Instance().s_debug.moveIssued = false;
      Instance().s_debug.hasTarget = false;
      Instance().s_debug.targetInRange = false;
      Instance().s_debug.targetNetId = 0;
      if (!IsEnabled()) return;
      if (Instance().ActiveMode == OrbwalkerMode::None) return;

      auto player = ObjectManager::Player();
      if (!player.IsValid() || player.IsDead()) return;

      // ══════════════════════════════════════════════════════════════
      // CRITICAL: Poll-based AA detection
      // In C# this is done via OnDoCast / OnProcessSpellCast events.
      // In C++ we must poll every frame to detect AA state changes.
      // ══════════════════════════════════════════════════════════════
      Instance().PollAutoAttackState(player);

      const bool timingReady = Instance().CanAttackTimingOnly();
      const bool nativeCanAttack = player.CanAttack();
      const bool canAttackNow = Instance().CanAttackFull();
      const bool canMoveNow = Instance().CanMoveFull();
      Instance().s_debug.canAttack = canAttackNow;
      Instance().s_debug.canMove = canMoveNow;
      Instance().s_debug.timingReady = timingReady;
      Instance().s_debug.nativeCanAttack = nativeCanAttack;

    // Flee mode – chỉ di chuyển
      if (Instance().ActiveMode == OrbwalkerMode::Flee) {
        TraceStage("Orbwalker::Update::FleeMove");
        Instance().MoveInternal(Game::CursorPos());
        return;
      }

      AIBaseClient target = {};
      bool targetInRange = false;
      if (Instance().AttackState) {
        TraceStage("Orbwalker::Update::ResolveTarget");
        target = Instance().GetTargetInternal();
        Instance().s_debug.hasTarget = target.IsValid();
        Instance().s_debug.targetNetId = target.IsValid() ? target.NetworkId() : 0;
        targetInRange = target.IsValid() && player.InAutoAttackRange(target);
        Instance().s_debug.targetInRange = targetInRange;
      }

      TraceRuntime(player, target, canAttackNow, canMoveNow, timingReady, nativeCanAttack, targetInRange);

    // Orbwalk logic (matching OrbwalkerBase.cs::Orbwalk)
    // CRITICAL: Attack and Move are MUTUALLY EXCLUSIVE per frame.
    // If we successfully issued attack, do NOT move this frame.
    // Only move when attack cooldown allows or no valid target.
      if (canAttackNow && Instance().AttackState) {
        TraceStage("Orbwalker::Update::AttackCheck");
        if (target.IsValid() && targetInRange) {
          TraceStage("Orbwalker::Update::AttackInternal");
          Instance().AttackInternal(target);
          // AttackInternal may return early (blockOrders, etc.)
          // Only skip move if attack was ACTUALLY issued
        }
      }

      // Move only if attack was NOT issued this frame AND we can move
      const bool attackWasIssued = Instance().s_debug.attackIssued;
      if (!attackWasIssued && canMoveNow && Instance().MovementState) {
        TraceStage("Orbwalker::Update::MoveInternal");
        Instance().MoveInternal(Game::CursorPos());
      }
      TraceStage("Orbwalker::Update::Done");
    }
    __except (CrashTelemetry::ReportAndHandle("Orbwalker::Update", GetExceptionInformation())) {
      return;
    }
  }

  // ── Render – draw circles ──
  static void Render() {
    auto* menu = Instance().s_menu;
    if (!menu || !menu->GetBoolValue("enabledOption", true)) return;

    auto player = ObjectManager::Player();
    if (!player.IsValid() || player.IsDead()) return;

    auto* drawings = menu->GetSubMenu("drawings");
    if (!drawings) return;

    // Player AA range
    if (drawings->GetBoolValue("drawAARange", true)) {
      Drawing::DrawCircle(player.Position(),
                          player.AttackRange() + player.BoundingRadius(),
                          IM_COL32(80, 200, 235, 255));
    }

    // Extra hold position
    auto* advanced = menu->GetSubMenu("advanced");
    if (drawings->GetBoolValue("drawExtraHoldPosition", false) && advanced) {
      int holdRadius = advanced->GetSliderValue("movementExtraHold", 0);
      if (holdRadius > 0) {
        Drawing::DrawCircle(player.Position(),
                            player.BoundingRadius() + static_cast<float>(holdRadius),
                            IM_COL32(160, 80, 200, 180));
      }
    }

    // Enemy AA range
    if (drawings->GetBoolValue("drawAARangeEnemy", false)) {
      for (const auto& enemy : ObjectManager::EnemyHeroes()) {
        if (!enemy.IsValidTarget(2000.0f, player.Position())) continue;
        Drawing::DrawCircle(enemy.Position(),
                            enemy.AttackRange() + enemy.BoundingRadius(),
                            IM_COL32(80, 200, 235, 180));
      }
    }

    // Killable minions
    if (drawings->GetBoolValue("drawKillableMinion", false)) {
      const float aaRange = player.AttackRange() + player.BoundingRadius();
      const bool fade = drawings->GetBoolValue("drawKillableMinionFade", false);

      for (const auto& minion : ObjectManager::EnemyMinions()) {
        if (!minion.IsValidTarget(aaRange * 2.0f, player.Position())) continue;
        const float aaDmg = player.GetAutoAttackDamage(minion);
        const float health = minion.Health();

        if (fade && health < aaDmg * 2.0f) {
          float value = 255.0f - health * 2.0f;
          value = std::clamp(value, 0.0f, 255.0f);
          Drawing::DrawCircle(minion.Position(), minion.BoundingRadius() * 2.0f,
                              IM_COL32(0, 255, static_cast<int>(255.0f - value), 255));
        } else if (!fade && health < aaDmg) {
          Drawing::DrawCircle(minion.Position(), minion.BoundingRadius() * 2.0f,
                              IM_COL32(0, 255, 0, 255));
        }
      }
    }

    auto* misc = menu->GetSubMenu("misc");
    if (misc && misc->GetBoolValue("drawChaseRange", false)) {
      const float extraRange = GetForceChaseExtraRange();
      if (extraRange > 0.0f) {
        const float totalRange = player.AttackRange() + player.BoundingRadius() + extraRange;
        ImU32 color = IM_COL32(100, 200, 255, 120);
        if (IsForceChaseActive()) {
          const float t = std::fmod(Game::Time() * 2.0f, 1.0f);
          const int r = static_cast<int>(std::sinf(t * 6.2831853f) * 127.0f + 128.0f);
          const int g = static_cast<int>(std::sinf(t * 6.2831853f + 2.0943951f) * 127.0f + 128.0f);
          const int b = static_cast<int>(std::sinf(t * 6.2831853f + 4.1887902f) * 127.0f + 128.0f);
          color = IM_COL32(r, g, b, 200);
        }
        Drawing::DrawCircle(player.Position(), totalRange, color, IsForceChaseActive() ? 2.5f : 1.0f);
      }
    }

    if (drawings->GetBoolValue("drawActiveMode", true) && Instance().ActiveMode != OrbwalkerMode::None) {
      const char* modeText = nullptr;
      switch (Instance().ActiveMode) {
      case OrbwalkerMode::Combo:   modeText = "Combo"; break;
      case OrbwalkerMode::Harass:  modeText = "Harass"; break;
      case OrbwalkerMode::Clear:   modeText = "LaneClear"; break;
      case OrbwalkerMode::LastHit: modeText = "LastHit"; break;
      case OrbwalkerMode::Flee:    modeText = "Flee"; break;
      default: break;
      }

      if (modeText) {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (dl) {
          ImVec2 display = ImGui::GetIO().DisplaySize;
          ImVec2 textSize = ImGui::CalcTextSize(modeText);
          const float x = display.x * 0.5f - textSize.x * 0.5f;
          const float y = display.y - 80.0f;
          dl->AddText(ImVec2(x + 1.0f, y + 1.0f), IM_COL32(0, 0, 0, 180), modeText);
          dl->AddText(ImVec2(x, y), IM_COL32(255, 255, 255, 220), modeText);
        }
      }
    }
  }

  // ── Debug state (kept for diagnostics) ──
  struct DebugState {
    int activeMode = 0;
    int reason = 0;
    bool enabled = false;
    bool canAttack = false;
    bool canMove = false;
    bool timingReady = false;
    bool nativeCanAttack = false;
    bool hasTarget = false;
    bool targetInRange = false;
    int targetNetId = 0;
    bool attackIssued = false;
    bool moveIssued = false;
  };

  static const DebugState& GetDebugState() { return Instance().s_debug; }

private:

  // ── Mode detection (matching C# MenuValueChanged) ──
  void RefreshMode() {
    if (!s_menu) {
      ActiveMode = OrbwalkerMode::None;
      return;
    }

    if (!s_menu->GetBoolValue("enabledOption", true)) {
      ActiveMode = OrbwalkerMode::None;
      return;
    }

    // Priority: Flee > Combo > ComboNoMove > Hybrid > FastLaneClear > LaneClear > LastHit
    if (s_menu->GetKeyBindValue("fleeKey", false)) {
      ActiveMode = OrbwalkerMode::Flee;
    } else if (s_menu->GetKeyBindValue("comboKey", false)) {
      ActiveMode = OrbwalkerMode::Combo;
    } else if (s_menu->GetKeyBindValue("comboNoMoveKey", false)) {
      ActiveMode = OrbwalkerMode::Combo;
    } else if (s_menu->GetKeyBindValue("hybridKey", false)) {
      ActiveMode = OrbwalkerMode::Harass;
    } else if (s_menu->GetKeyBindValue("fastLaneClearKey", false)) {
      ActiveMode = OrbwalkerMode::Clear;
    } else if (s_menu->GetKeyBindValue("laneclearKey", false)) {
      ActiveMode = OrbwalkerMode::Clear;
    } else if (s_menu->GetKeyBindValue("lasthitKey", false)) {
      ActiveMode = OrbwalkerMode::LastHit;
    } else {
      ActiveMode = OrbwalkerMode::None;
    }

    s_debug.activeMode = static_cast<int>(ActiveMode);
    s_debug.enabled = true;
  }

  // ── CanAttack with champion-specific checks (matching Orbwalker.cs::CanAttack) ──
  bool CanAttackFull(float extra = 0.0f) const {
    const auto player = ObjectManager::Player();
    float extraDelay = extra;

    // Graves – cần ammo
    const std::string name = player.CharacterName();
    if (name == "Graves") {
      if (!player.HasBuff("gravesbasicattackammo1")) return false;
      const float attackDelayMs = CoreAPI::Control::GetAttackDelay() * 1000.0f;
      extraDelay += (attackDelayMs * 1.0740296828f) - 716.2381256175f - attackDelayMs;
    }
    // Jhin – reloading
    else if (name == "Jhin" && player.HasBuff("JhinPassiveReload")) {
      return false;
    }

    return OrbwalkerBase::CanAttack(extraDelay);
  }

  bool CanAttackTimingOnly(float extra = 0.0f) const {
    const auto player = ObjectManager::Player();
    float extraDelay = extra;

    const std::string name = player.CharacterName();
    if (name == "Graves") {
      if (!player.HasBuff("gravesbasicattackammo1")) return false;
      const float attackDelayMs = CoreAPI::Control::GetAttackDelay() * 1000.0f;
      extraDelay += (attackDelayMs * 1.0740296828f) - 716.2381256175f - attackDelayMs;
    } else if (name == "Jhin" && player.HasBuff("JhinPassiveReload")) {
      return false;
    }

    return OrbwalkerBase::CanAttack(extraDelay);
  }

  // ── CanMoveFull – matching C# Orbwalker.CanMove ──
  // C# calls: base.CanMove(extraWindup + localExtraWindup + delayWindup_slider, 
  //                         disableMissileCheck || !miscMissile)
  // Slider "delayWindup" (0-200) is treated as FLAT ms added to windup (matching C#).
  bool CanMoveFull(float extra = 0.0f) const {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return false;

    // CanCancelAutoAttack check (Kalista etc.)
    if (!AutoAttack::CanCancelAutoAttack(player)) {
      return true;
    }

    const int now = Game::TickCount();
    const int ping = CoreAPI::Control::GetPing();
    const float windupMs = CoreAPI::Control::GetAttackWindup() * 1000.0f;
    const int sinceAA = now - LastAutoAttackTick;

    // Hard minimum: never move before 85% of windup has elapsed
    // This prevents false-positive MissileLaunched from polling jitter
    if (LastAutoAttackTick > 0 && sinceAA < static_cast<int>(windupMs * 0.85f)) {
      return false;
    }

    float localExtra = extra;

    // Rengar Q extra windup (matching C# Orbwalker.CanMove line 278-282)
    const std::string name = player.CharacterName();
    if (name == "Rengar" && (player.HasBuff("RengarQ") || player.HasBuff("RengarQEmp"))) {
      localExtra += 200.0f;
    }

    // C#: extraWindup + localExtraWindup + delayWindup slider value (FLAT ms)
    auto* advanced = s_menu ? s_menu->GetSubMenu("advanced") : nullptr;
    const int sliderVal = advanced ? advanced->GetSliderValue("delayWindup", 80) : 80;
    localExtra += static_cast<float>(sliderVal);

    // MissileLaunched: trust ONLY if timing also confirms windup is done
    const bool useMissile = advanced ? advanced->GetBoolValue("miscMissile", true) : true;
    if (MissileLaunched && useMissile) {
      // Additional timing confirmation: at least 90% of windup must have passed
      if (sinceAA >= static_cast<int>(windupMs * 0.9f)) {
        return true;
      }
      // If timing doesn't confirm, fall through to normal timing gate
    }

    // Normal timing gate: full windup + slider buffer
    return (now + (ping / 2)) >= (LastAutoAttackTick + static_cast<int>(windupMs + localExtra));
  }

  int GetMovementOrderDelay() const {
    auto* advanced = s_menu ? s_menu->GetSubMenu("advanced") : nullptr;
    return advanced ? advanced->GetSliderValue("delayMovement", 35) : 35;
  }

  // ── Attack (matching Orbwalker.cs::Attack) ──
  void AttackInternal(const AIBaseClient& target) {
    TraceStage("Orbwalker::AttackInternal::Enter");
    const int now = Game::TickCount();

    // Block orders check
    if ((s_blockOrdersUntil - now) > 0) return;

    auto player = ObjectManager::Player();
    if (!target.IsValid() || !player.InAutoAttackRange(target)) return;

    // BeforeAttack event
    OrbwalkingActionArgs args{};
    args.Target = target;
    args.Position = target.Position();
    args.Process = true;
    args.Type = OrbwalkingType::BeforeAttack;
    InvokeAction(args);

    if (!args.Process) return;

    // Issue attack order
    MissileLaunched = false;

    TraceStage("Orbwalker::AttackInternal::IssueOrder");
    if (player.IssueOrder(GameObjectOrder::AttackUnit, args.Target)) {
      LastAutoAttackCommandTick = now;
      LastTarget = args.Target;
      s_debug.attackIssued = true;
      s_lastConfirmedAutoAttackTick = 0;

      // Match C# OnDoCast line 523: LastAutoAttackTick = TickCount - (Ping / 2);
      // Backdate is safe now because CanMove checks MissileLaunched first.
      const int ping = CoreAPI::Control::GetPing();
      LastAutoAttackTick = now - (ping / 2);
      MissileLaunched = false;
      LastMovementOrderTick = 0;

      const int movementDelay = GetMovementOrderDelay();
      const int baseBlock = 70 + movementDelay + std::min(60, ping);
      s_blockOrdersUntil = now + baseBlock;
    }
  }

  // ── Move (matching Orbwalker.cs::Move) ──
  void MoveInternal(const Vector3& position) {
    TraceStage("Orbwalker::MoveInternal::Enter");
    const int now = Game::TickCount();
    if ((s_blockOrdersUntil - now) > 0) return;
    if (!position.IsValid()) return;

    TraceStage("Orbwalker::MoveInternal::Player");
    auto player = ObjectManager::Player();
    if (!player.IsValid()) return;
    if (ActiveMode == OrbwalkerMode::Combo && IsComboNoMoveActive()) {
      return;
    }

    TraceStage("Orbwalker::MoveInternal::Throttle");
    const int movementDelay = GetMovementOrderDelay();
    const int minInterval = 70 + movementDelay + std::min(60, CoreAPI::Control::GetPing());
    if ((now - LastMovementOrderTick) < minInterval) {
      return;
    }

    TraceStage("Orbwalker::MoveInternal::InvokeAction");
    OrbwalkingActionArgs args{};
    args.Position = position;
    args.Process = true;
    args.Type = OrbwalkingType::Movement;
    InvokeAction(args);
    if (!args.Process) return;

    TraceStage("Orbwalker::MoveInternal::IssueOrder");
    s_debug.moveIssued = SafeIssueMove(player, args.Position);
    if (s_debug.moveIssued) {
      LastMovementOrderTick = now;
    }
  }

  // ── GetTarget (delegates to OrbwalkerSelector) ──
  AIBaseClient GetTargetInternal() {
    TraceStage("Orbwalker::GetTargetInternal::Enter");
    const auto player = ObjectManager::Player();
    float range = player.AttackRange() + player.BoundingRadius();
    if (IsForceChaseActive()) {
      range += GetForceChaseExtraRange();
    }

    if (s_forcedTarget.IsValid() && s_forcedTarget.IsValidTarget(range, player.Position())) {
      return s_forcedTarget;
    }

    return OrbwalkerSelector::GetTarget(player, ActiveMode, range, s_menu);
  }

  // ══════════════════════════════════════════════════════════════
  // Poll-based AA detection (replaces C# event-driven approach)
  //
  // In C#, OrbwalkerBase hooks OnDoCast/OnProcessSpellCast events.
  // In C++ (manual-map), we poll the active spell cast each frame
  // to detect transitions:
  //   NOT-attacking → attacking  = AA started  → set LastAutoAttackTick
  //   attacking     → NOT-winding up = missile launched → set MissileLaunched
  // ══════════════════════════════════════════════════════════════
  void PollAutoAttackState(const AIHeroClient& player) {
    const auto castRef = CoreSpellCastInfo::GetActive(player.Address());
    const bool isCurrentlyWindingUp = player.IsWindingUp();
    const bool isCurrentlyAAing = castRef.IsValid() && castRef.IsAutoAttack();

    // Diagnostic: log poll state (throttled)
    static DWORD s_lastPollLog = 0;
    const DWORD pollNow = GetTickCount();
    if ((pollNow - s_lastPollLog) > 5000) {
      s_lastPollLog = pollNow;
      const int diagNow = Game::TickCount();
      const int diagPing = CoreAPI::Control::GetPing();
      const float diagAtkDelay = CoreAPI::Control::GetAttackDelay() * 1000.0f;
      const float diagWindup = CoreAPI::Control::GetAttackWindup() * 1000.0f;
      const int diagTimeSince = diagNow - LastAutoAttackTick;
      auto* advanced = s_menu ? s_menu->GetSubMenu("advanced") : nullptr;
      const int diagSlider = advanced ? advanced->GetSliderValue("delayWindup", 80) : 80;
      const float diagBuffer = diagWindup * (static_cast<float>(diagSlider) / 200.0f);
      char buf[512] = {};
      std::snprintf(buf, sizeof(buf),
        "[NightSharp][PollAA] missile=%d lastAATick=%d total=%d now=%d ping=%d atkDelay=%.0f windup=%.0f buffer=%.0f(slider=%d) sinceAA=%d\r\n",
        MissileLaunched ? 1 : 0,
        LastAutoAttackTick,
        TotalAutoAttacks,
        diagNow,
        diagPing,
        diagAtkDelay,
        diagWindup,
        diagBuffer,
        diagSlider,
        diagTimeSince);
      CoreControl::AppendIssueOrderDebug(buf);
    }

    // ── Phase 1: Detect AA START (windup began) ──
    // Transition: was NOT attacking → IS attacking
    if (isCurrentlyAAing && !s_wasAttacking) {
      // Match C# OnDoCast line 523: LastAutoAttackTick = TickCount - (Ping / 2);
      // Backdate by ping/2 to compensate for network delay in detection.
      // Safe now because CanMove checks MissileLaunched before timing gate.
      const int now = Game::TickCount();
      const int ping = CoreAPI::Control::GetPing();
      s_lastConfirmedAutoAttackTick = now - (ping / 2);
      LastAutoAttackTick = s_lastConfirmedAutoAttackTick;
      MissileLaunched = false;
      LastMovementOrderTick = 0; // allow immediate move after windup
      TotalAutoAttacks++;
      // Fire OnAttack event (target is already set by AttackInternal → LastTarget)
      if (LastTarget.IsValid()) {
        OrbwalkingActionArgs onAttack{};
        onAttack.Target = LastTarget;
        onAttack.Sender = player;
        onAttack.Type = OrbwalkingType::OnAttack;
        InvokeAction(onAttack);
      }
    }

    // ── Phase 2: Detect MISSILE LAUNCH (windup ended) ──
    // Safety: only accept missile launch if enough windup time has actually passed.
    // This prevents false-positive detection from polling jitter/frame drops.
    if (s_wasAttacking && !isCurrentlyWindingUp && !MissileLaunched) {
      const int now2 = Game::TickCount();
      const float windupCheck = CoreAPI::Control::GetAttackWindup() * 1000.0f;
      const bool enoughTimePassed = (LastAutoAttackTick <= 0) ||
          ((now2 - LastAutoAttackTick) >= static_cast<int>(windupCheck * 0.6f));
      if (enoughTimePassed) {
        MissileLaunched = true;
        if (LastTarget.IsValid()) {
          OrbwalkingActionArgs afterArgs{};
          afterArgs.Target = LastTarget;
          afterArgs.Sender = player;
          afterArgs.Type = OrbwalkingType::AfterAttack;
          InvokeAction(afterArgs);
        }
      }
    }

    // ── Fallback: fire AfterAttack by timing if poll didn't detect it ──
    if (!MissileLaunched && LastAutoAttackTick > 0) {
      const int now = Game::TickCount();
      const float windupMs = CoreAPI::Control::GetAttackWindup() * 1000.0f;
      if ((now - LastAutoAttackTick) >= static_cast<int>(windupMs + 80.0f)) {
        MissileLaunched = true;
        if (LastTarget.IsValid()) {
          OrbwalkingActionArgs afterArgs{};
          afterArgs.Target = LastTarget;
          afterArgs.Sender = player;
          afterArgs.Type = OrbwalkingType::AfterAttack;
          InvokeAction(afterArgs);
        }
      }
    }

    s_wasAttacking = isCurrentlyAAing;
  }

  // ── Internal state ──
  bool s_initialized = false;
  Menu* s_menu = nullptr;
  AIBaseClient s_forcedTarget = {};
  int s_blockOrdersUntil = 0;
  bool s_wasAttacking = false;  // tracks previous frame's AA state
  int s_lastConfirmedAutoAttackTick = 0;
  DebugState s_debug = {};

  static bool SafeIssueMove(const AIHeroClient& player, const Vector3& position) {
    __try {
      return player.IssueOrder(GameObjectOrder::MoveTo, position);
    }
    __except (CrashTelemetry::ReportAndHandle("Orbwalker::MoveInternal", GetExceptionInformation())) {
      return false;
    }
  }

};

} // namespace SDK
