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

  // It is wrong
  static float TimeUntilNextAttack() {
      const float lastTime = Instance().LastAutoAttackTime;
      if (lastTime <= 0.0f) return 0.0f;
      const float now = Game::Time();
      const float ping = static_cast<float>(CoreAPI::Control::GetPing());
      const float attackDelay = CoreAPI::Control::GetAttackDelay();
      const float nextAttackTime = lastTime + attackDelay;
      const float adjusted = now + (ping / 2.0f + 25.0f) / 1000.0f;
      if (adjusted >= nextAttackTime) return 0.0f;
      return nextAttackTime - adjusted;
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
