#pragma once

#include "CastSpellTestSupport.h"

namespace Plugins {

class EzrealSemiPlugin final : public CastSpellTestPluginBase {
public:
    const char* GetName() const override { return "Ezreal CastSpell Test"; }
    const char* GetInternalId() const override { return "champion.ezreal_cast_test"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    const char* GetChampionName() const override { return "Ezreal"; }
    bool CanLoad() const override { return CanLoadChampion("Ezreal"); }

protected:
    const char* DebugPrefix() const override { return "EzrealCastTest"; }
    const char* LogPath() const override {
        return "C:\\Users\\Public\\nightsharp_ezreal_cast_test.txt";
    }

    void BuildChampionMenu(Menu* settings) override {
        m_qKey = settings->Add(new MenuKeyBind(
            "castQ",
            "Cast Q at target-selector prediction",
            'A',
            SDK::KeyBindType::Hold));
    }

    void HandleGameUpdate(const SDK::Events::GameUpdateEventArgs&) override {
        const bool down = KeyDown(m_qKey);
        const bool pressed = down && !m_qWasDown;
        m_qWasDown = down;
        if (!pressed) {
            return;
        }

        if (IsChatTyping()) {
            RecordBlocked("EzrealQ", "chat-typing");
            return;
        }

        const uintptr_t target = ResolveComboTarget(1200.0f);
        if (!Globals::IsValidPtr(target)) {
            RecordBlocked("EzrealQ", "no-target-in-range");
            return;
        }

        // Ezreal Q: ~0.25s windup, ~2000 units/s.
        const Vec3 castPosition =
            PredictTargetPosition(target, 0.25f, 2000.0f);
        if (!castPosition.IsValid() || castPosition.IsZero()) {
            RecordBlocked("EzrealQ", "prediction-failed");
            return;
        }

        const bool ok = CoreCastSpell::CastPositionSpell(
            CoreCastSpell::SlotQ,
            castPosition);
        RecordAttempt("EzrealQ", ok, castPosition, target);
    }

    void DrawChampionDebug() override {
        ImGui::Text("A: Ezreal Q at selected/predicted enemy");
        ImGui::Text("Physical mouse position is ignored");
    }

private:
    MenuKeyBind* m_qKey = nullptr;
    bool m_qWasDown = false;
};

} // namespace Plugins
