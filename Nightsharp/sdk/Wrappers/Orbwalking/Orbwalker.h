#pragma once

#include "../../../menu/MenuUI.h"
#include "../../Enumerations/OrbwalkerMode.h"
#include "../../Core/Game.h"
#include "../../Core/Objects.h"
#include "OrbwalkerBase.h"
#include "../../UI/UI.h"

#include <Windows.h>

namespace SDK {

class Orbwalker : public OrbwalkerBase {
public:

  // ── Singleton access ──
  static Orbwalker& Instance() {
    static Orbwalker instance = {};
    return instance;
  }
  static void SetMenu(Menu* m) { Instance().s_menu = m; }
  static Menu* GetMenu() { return Instance().s_menu; }
  static OrbwalkerMode GetMode() { return Instance().ActiveMode; }

  static float TimeUntilNextAttack() {
      const int now = Game::TickCount();
      const int ping = CoreAPI::Control::GetPing();
      const float attackDelayMs = CoreAPI::Control::GetAttackDelay() * 1000.0f;
      const int nextAttackTick = Instance().LastAutoAttackTick + static_cast<int>(attackDelayMs);
      const int adjusted = now + (ping / 2) + 25;
      if (adjusted >= nextAttackTick) return 0.0f;
      return static_cast<float>(nextAttackTick - adjusted) / 1000.0f;
  }

  static void ForceTarget(const AIBaseClient& t) { Instance().s_forcedTarget = t; }
  static void ClearForcedTarget() { Instance().s_forcedTarget = AIBaseClient(); }
  static AIBaseClient GetForcedTarget() { return Instance().s_forcedTarget; }

  static void Update() {}

  static void Render() {}

private:
  Menu* s_menu = nullptr;
  AIBaseClient s_forcedTarget = {};
};

} // namespace SDK
