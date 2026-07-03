#pragma once

#include "CastSpellTestSupport.h"

namespace Plugins {

class JaxSemiPlugin final : public CastSpellTestPluginBase {
public:
    const char* GetName() const override { return "Jax CastSpell Test"; }
    const char* GetInternalId() const override { return "champion.jax_cast_test"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    const char* GetChampionName() const override { return "Jax"; }
    bool CanLoad() const override { return CanLoadChampion("Jax"); }

protected:
    const char* DebugPrefix() const override { return "JaxCastTest"; }
    const char* LogPath() const override {
        return "C:\\Users\\Public\\nightsharp_jax_cast_test.txt";
    }

    void BuildChampionMenu(Menu* settings) override {
        m_qKey = settings->Add(new MenuKeyBind(
            "castQ",
            "Cast Q at target-selector enemy",
            'A',
            SDK::KeyBindType::Hold));
        m_wKey = settings->Add(new MenuKeyBind(
            "castW",
            "Cast self W",
            'S',
            SDK::KeyBindType::Hold));
    }

    void HandleGameUpdate(const SDK::Events::GameUpdateEventArgs&) override {
        const bool qDown = KeyDown(m_qKey);
        const bool wDown = KeyDown(m_wKey);
        const bool qPressed = qDown && !m_qWasDown;
        const bool wPressed = wDown && !m_wWasDown;
        m_qWasDown = qDown;
        m_wWasDown = wDown;

        if (!qPressed && !wPressed) {
            return;
        }

        if (IsChatTyping()) {
            RecordBlocked(qPressed ? "JaxQ" : "JaxW", "chat-typing");
            return;
        }

        if (qPressed) {
            CastQSelectedTarget();
        }
        if (wPressed) {
            const Vec3 selfPosition = SDK::ObjectManager::Player().Position();
            const bool ok = m_w.Cast();
            RecordAttempt("JaxW", ok, selfPosition);
        }
    }

    void DrawChampionDebug() override {
        ImGui::Text("A: Jax Q at target-selector enemy");
        ImGui::Text("Physical mouse position is ignored");
        ImGui::Text("S: Jax W self/toggle");
    }

private:
    MenuKeyBind* m_qKey = nullptr;
    MenuKeyBind* m_wKey = nullptr;
    SDK::Spell m_q{ SDK::SpellSlot::Q, 700.0f };
    SDK::Spell m_w{ SDK::SpellSlot::W };
    bool m_qWasDown = false;
    bool m_wWasDown = false;

    int DebugEnumerateHeroCount() const {
        uintptr_t heroes[64] = {};
        return ::Core::ObjectManager::EnumerateHeroes(heroes, 64);
    }

    void CastQSelectedTarget() {
        (void)CoreRuntime::RefreshReadState();

        const uintptr_t targetAddress = ResolveComboTarget(700.0f);
        if (!Globals::IsValidPtr(targetAddress)) {
            const int heroCount = DebugEnumerateHeroCount();
            Appendf(
                "[JaxCastTest] blocked tick=%d action=JaxQ reason=no-target-in-range heroes-enumerated=%d cursor=%.1f %.1f %.1f\r\n",
                SDK::Game::TickCount(),
                heroCount,
                SDK::Game::CursorPos().x,
                SDK::Game::CursorPos().y,
                SDK::Game::CursorPos().z);
            return;
        }

        const SDK::AIHeroClient target(targetAddress);
        const Vec3 targetPosition = target.Position();
        const bool ok = m_q.CastOnUnit(target);
        RecordAttempt("JaxQ", ok, targetPosition, targetAddress);
    }
};

} // namespace Plugins
