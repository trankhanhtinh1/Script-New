#pragma once

#include "../../Core/Objects.h"
#include "../../Core/Game.h"
#include "../../UI/UI.h"
#include "../../../core/CoreControl.h"

#include <cmath>

namespace SDK {

class TargetSelectorSelected {
public:
    static void Initialize(Menu* root) {
        if (s_initialized || !root) {
            return;
        }
        s_initialized = true;

        auto general = UI::Wrap(root).SubMenu("general");
        general.AddBool("focusSelected", "Focus Selected Target", true);
        general.AddBool("forceSelected", "Only Attack Selected Target", false);
    }

    static void Update() {
        if (s_forced.IsValid() && (!s_forced.IsEnemy() || s_forced.IsDead())) {
            s_forced = AIHeroClient();
        }

        // Click-based target selection (like old NightSharp)
        HandleClickSelect();

        // Expire dead/invisible selected target
        if (s_selected.IsValid()) {
            if (s_selected.IsDead() || !s_selected.IsVisible()) {
                s_selected = AIHeroClient();
            }
        }
    }

    static AIHeroClient Target() { return s_selected; }
    static AIHeroClient ForcedTarget() { return s_forced; }

    static void SetTarget(const AIHeroClient& target) {
        s_selected = target.IsValid() && target.IsHero() ? target : AIHeroClient();
    }

    static void SetForcedTarget(const AIHeroClient& target) {
        s_forced = target.IsValid() && target.IsHero() ? target : AIHeroClient();
    }

    static void ClearTarget() { s_selected = AIHeroClient(); }
    static void ClearForcedTarget() { s_forced = AIHeroClient(); }

    static bool Focus(Menu* root) {
        if (auto general = UI::Wrap(root).SubMenu("general"); general.IsValid()) {
            return general.Bool("focusSelected", true);
        }
        return true;
    }

    static bool Force(Menu* root) {
        if (auto general = UI::Wrap(root).SubMenu("general"); general.IsValid()) {
            return general.Bool("forceSelected", false);
        }
        return false;
    }

private:
    static inline bool s_initialized = false;
    static inline AIHeroClient s_selected = {};
    static inline AIHeroClient s_forced = {};
    static inline bool s_wasClickDown = false;

    static void HandleClickSelect() {
        bool clickDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        // Only process on click DOWN edge (not held)
        if (clickDown && !s_wasClickDown) {
            Vector3 clickPos = Game::CursorPos();

            if (!clickPos.IsZero()) {
                AIHeroClient nearest;
                float nearestDist = 300.0f; // max click distance to select

                for (const auto& hero : ObjectManager::EnemyHeroes()) {
                    if (!hero.IsValid() || hero.IsDead() || !hero.IsVisible()) {
                        continue;
                    }

                    float dist = hero.Distance(clickPos);
                    if (dist < nearestDist) {
                        nearestDist = dist;
                        nearest = hero;
                    }
                }

                // Diagnostic (throttled)
                static DWORD s_lastClickDiag = 0;
                const DWORD clickNow = GetTickCount();
                if ((clickNow - s_lastClickDiag) > 5000) {
                    s_lastClickDiag = clickNow;
                    char buf[256] = {};
                    std::snprintf(buf, sizeof(buf),
                        "[NightSharp][TS] click at %.0f,%.0f,%.0f found=%d nearDist=%.0f\r\n",
                        clickPos.x, clickPos.y, clickPos.z,
                        nearest.IsValid() ? 1 : 0,
                        nearestDist);
                    CoreControl::AppendIssueOrderDebug(buf);
                }

                if (nearest.IsValid()) {
                    s_selected = nearest;
                } else {
                    // Click on empty ground → deselect
                    s_selected = AIHeroClient();
                }
            }
        }

        s_wasClickDown = clickDown;
    }
};

} // namespace SDK
