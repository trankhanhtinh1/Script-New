#pragma once

#include "CastSpellTestSupport.h"

namespace Plugins {

class XerathSemiPlugin final : public CastSpellTestPluginBase {
public:
    const char* GetName() const override { return "Xerath CastSpell Test"; }
    const char* GetInternalId() const override { return "champion.xerath_cast_test"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    const char* GetChampionName() const override { return "Xerath"; }
    bool CanLoad() const override { return CanLoadChampion("Xerath"); }

protected:
    const char* DebugPrefix() const override { return "XerathCastTest"; }
    const char* LogPath() const override {
        return "C:\\Users\\Public\\nightsharp_xerath_cast_test.txt";
    }

    void BuildChampionMenu(Menu* settings) override {
        m_q = SDK::Spell(SDK::SpellSlot::Q, kMinQRange);
        m_q.SetSkillshot(0.55f, 65.0f, FLT_MAX, false, SDK::SpellType::Line);
        m_q.SetCharged(
            "XerathArcanopulseChargeUp",
            "XerathArcanopulseChargeUp",
            static_cast<int>(kMinQRange),
            static_cast<int>(kMaxQRange),
            static_cast<float>(kRangeGrowMs) / 1000.0f);

        m_qKey = settings->Add(new MenuKeyBind(
            "autoChargeQ",
            "Press: auto charge/release Q at selected enemy",
            SDK::Keys::A,
            SDK::KeyBindType::Press));
    }

    void HandleGameUpdate(const SDK::Events::GameUpdateEventArgs&) override {
        const bool down = KeyDown(m_qKey);
        const bool pressed = down && !m_qWasDown;
        m_qWasDown = down;

        if (pressed && !m_charging) {
            BeginAutoQ();
        }
        if (m_charging) {
            UpdateAutoQ();
        }
    }

    void DrawChampionDebug() override {
        ImGui::Text("A press: auto charge/release Xerath Q");
        ImGui::Text("Physical mouse position is ignored");
        ImGui::Text(
            "Charge: %s  elapsed=%dms  range=%.1f",
            m_charging ? "active" : "idle",
            m_charging
                ? SDK::Game::TickCount() - m_chargeStartTick
                : 0,
            m_currentRange);
    }

    void OnChampionUnload() override {
        ResetCharge();
        m_qWasDown = false;
    }

private:
    static constexpr float kMinQRange = 700.0f;
    static constexpr float kMaxQRange = 1450.0f;
    static constexpr int kRangeGrowMs = 1500;
    static constexpr int kMinimumHoldMs = 100;

    MenuKeyBind* m_qKey = nullptr;
    SDK::Spell m_q{ SDK::SpellSlot::Q, kMinQRange };
    bool m_qWasDown = false;
    bool m_charging = false;
    int m_chargeStartTick = 0;
    uintptr_t m_target = 0;
    Vec3 m_lastPrediction = {};
    float m_currentRange = kMinQRange;

    void ResetCharge() {
        m_charging = false;
        m_chargeStartTick = 0;
        m_target = 0;
        m_lastPrediction = {};
        m_currentRange = kMinQRange;
    }

    void BeginAutoQ() {
        if (IsChatTyping()) {
            RecordBlocked("XerathQBegin", "chat-typing");
            return;
        }

        m_target = ResolveComboTarget(kMaxQRange);
        if (!Globals::IsValidPtr(m_target)) {
            RecordBlocked("XerathQBegin", "no-target-in-max-range");
            ResetCharge();
            return;
        }

        // Xerath Q has no projectile travel after release; predict its cast
        // delay while charging begins.
        m_lastPrediction =
            PredictTargetPosition(m_target, 0.50f, FLT_MAX);
        if (!m_lastPrediction.IsValid() ||
            m_lastPrediction.IsZero()) {
            RecordBlocked("XerathQBegin", "prediction-failed");
            ResetCharge();
            return;
        }

        const bool ok = SDK::ObjectManager::Player().Spellbook().UpdateChargedSpell(
            SDK::SpellSlot::Q,
            m_lastPrediction,
            false);
        RecordAttempt(
            "XerathQBeginAuto",
            ok,
            m_lastPrediction,
            m_target);
        if (!ok) {
            ResetCharge();
            return;
        }

        m_charging = true;
        m_chargeStartTick = SDK::Game::TickCount();
        m_currentRange = kMinQRange;
    }

    void UpdateAutoQ() {
        const int now = SDK::Game::TickCount();
        const int elapsed = std::max(0, now - m_chargeStartTick);
        const float progress = std::min(
            1.0f,
            static_cast<float>(elapsed) /
                static_cast<float>(kRangeGrowMs));
        m_currentRange =
            kMinQRange + (kMaxQRange - kMinQRange) * progress;

        if (Globals::IsValidPtr(m_target)) {
            const Vec3 prediction =
                PredictTargetPosition(m_target, 0.50f, FLT_MAX);
            if (prediction.IsValid() && !prediction.IsZero()) {
                m_lastPrediction = prediction;
            }
        }

        if (!m_lastPrediction.IsValid() ||
            m_lastPrediction.IsZero()) {
            if (elapsed >= kRangeGrowMs) {
                RecordBlocked("XerathQRelease", "lost-target-position");
                ResetCharge();
            }
            return;
        }

        const Vec3 playerPosition =
            SDK::ObjectManager::Player().Position();
        const float targetDistance =
            playerPosition.Distance2D(m_lastPrediction);
        const bool enoughRange =
            elapsed >= kMinimumHoldMs &&
            targetDistance <= (m_currentRange - 20.0f);
        const bool maxRangeReached = elapsed >= kRangeGrowMs;

        if (enoughRange || maxRangeReached) {
            ReleaseAutoQ(targetDistance);
        }
    }

    void ReleaseAutoQ(float targetDistance) {
        const bool ok = SDK::ObjectManager::Player().Spellbook().UpdateChargedSpell(
            SDK::SpellSlot::Q,
            m_lastPrediction,
            true);
        Appendf(
            "[XerathCastTest] auto-release tick=%d elapsed=%d range=%.1f targetDistance=%.1f target=0x%llX prediction=%.1f %.1f %.1f ok=%d\r\n",
            SDK::Game::TickCount(),
            SDK::Game::TickCount() - m_chargeStartTick,
            m_currentRange,
            targetDistance,
            static_cast<unsigned long long>(m_target),
            m_lastPrediction.x,
            m_lastPrediction.y,
            m_lastPrediction.z,
            ok ? 1 : 0);
        RecordAttempt(
            "XerathQReleaseAuto",
            ok,
            m_lastPrediction,
            m_target);
        ResetCharge();
    }
};

} // namespace Plugins
