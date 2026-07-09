#pragma once

#include "CastSpellTestSupport.h"
#include "../../Core/CoreNewCastSpell.h"

namespace Plugins {

class JaxSemiCastNew final : public CastSpellTestPluginBase {
public:
    const char* GetName() const override { return "Jax Semi Cast New"; }
    const char* GetInternalId() const override { return "champion.jax_semi_cast_new"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    const char* GetChampionName() const override { return "Jax"; }
    bool CanLoad() const override { return CanLoadChampion("Jax"); }

protected:
    const char* DebugPrefix() const override { return "JaxSemiCastNew"; }
    const char* LogPath() const override {
        return "C:\\Users\\Public\\nightsharp_jax_semi_cast_new.txt";
    }

    void BuildChampionMenu(Menu* settings) override {
        m_qKey = settings->Add(new MenuKeyBind(
            "castQNew",
            "Cast Q with CoreNewCastSpell method 2",
            SDK::Keys::A,
            SDK::KeyBindType::Press));
    }

    void HandleGameUpdate(const SDK::Events::GameUpdateEventArgs&) override {
        const bool qDown = KeyDown(m_qKey);
        const bool qPressed = qDown && !m_qWasDown;
        m_qWasDown = qDown;

        if (!qPressed) {
            return;
        }

        if (IsChatTyping()) {
            RecordBlocked("JaxQNewMethod2", "chat-typing");
            return;
        }

        CastQSelectedTarget();
    }

    void DrawChampionDebug() override {
        ImGui::Text("A: Jax Q with CoreNewCastSpell target method 2");
        ImGui::Text("Method: prime provider -> CastSpellSafe");
        ImGui::Text("Does not use HUD cursor / physical mouse position");
    }

    void OnChampionUnload() override {
        m_qWasDown = false;
    }

private:
    static constexpr float kQRange = 700.0f;

    MenuKeyBind* m_qKey = nullptr;
    bool m_qWasDown = false;

    int DebugEnumerateHeroCount() const {
        uintptr_t heroes[64] = {};
        return ::Core::ObjectManager::EnumerateHeroes(heroes, 64);
    }

    void CastQSelectedTarget() {
        (void)CoreRuntime::RefreshReadState();

        const uintptr_t targetAddress = ResolveComboTarget(kQRange);
        if (!Globals::IsValidPtr(targetAddress)) {
            const int heroCount = DebugEnumerateHeroCount();
            Appendf(
                "[JaxSemiCastNew] blocked tick=%d action=JaxQNewMethod2 reason=no-target-in-range heroes-enumerated=%d cursor=%.1f %.1f %.1f\r\n",
                SDK::Game::TickCount(),
                heroCount,
                SDK::Game::CursorPos().x,
                SDK::Game::CursorPos().y,
                SDK::Game::CursorPos().z);
            return;
        }

        const SDK::AIHeroClient target(targetAddress);
        const Vec3 targetPosition = target.Position();
        const bool ok = CoreNewCastSpell::CastTargetSpellMethod2(
            static_cast<std::int32_t>(SDK::SpellSlot::Q),
            targetAddress);

        const auto& trace = CoreNewCastSpell::LastTrace();
        Appendf(
            "[JaxSemiCastNew] method2-target tick=%d target=0x%llX net=%u index=0x%X pos=%.1f %.1f %.1f ok=%d canCast=%d virtualCursor=%d failure=%s nativeResult=%lld provider=0x%llX\r\n",
            SDK::Game::TickCount(),
            static_cast<unsigned long long>(targetAddress),
            trace.targetNetworkId,
            trace.targetObjectIndex,
            targetPosition.x,
            targetPosition.y,
            targetPosition.z,
            ok ? 1 : 0,
            trace.canCastAccepted ? 1 : 0,
            trace.virtualCursorApplied ? 1 : 0,
            CoreCastSpell::CastFailureName(trace.failure),
            static_cast<long long>(trace.nativeResult),
            static_cast<unsigned long long>(trace.runtimeInput));

        RecordAttempt("JaxQNewMethod2", ok, targetPosition, targetAddress);
    }
};

} // namespace Plugins
