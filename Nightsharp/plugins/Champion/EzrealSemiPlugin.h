#pragma once

#include "CastSpellTestSupport.h"

#include <cstdio>

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
        SetupQ();

        m_qKey = settings->Add(new MenuKeyBind(
            "castQ",
            "Cast Q with SDK Prediction/Collision",
            'A',
            SDK::KeyBindType::Hold));
    }

    void HandleGameUpdate(const SDK::Events::GameUpdateEventArgs&) override {
        const bool down = KeyDown(m_qKey);
        const bool pressed = down && !m_qWasDown;
        m_qWasDown = down;
        if (!down) {
            return;
        }

        if (IsChatTyping()) {
            if (pressed) {
                RecordBlocked("EzrealQ", "chat-typing");
            }
            return;
        }

        const uintptr_t target = ResolveComboTarget(m_q.CurrentRange());
        if (!Globals::IsValidPtr(target)) {
            m_lastPredictionValid = false;
            if (pressed) {
                RecordBlocked("EzrealQ", "no-target-in-range");
            }
            return;
        }

        const SDK::AIBaseClient targetUnit(target);
        const SDK::PredictionInput input = BuildQPredictionInput(targetUnit);
        const SDK::PredictionOutput prediction = SDK::Prediction::GetPrediction(input);
        StoreLastPrediction(target, prediction);

        if (!pressed) {
            return;
        }

        LogPrediction("press", target, prediction);

        const Vec3 castPosition = prediction.GetCastPosition();
        if (!castPosition.IsValid() || castPosition.IsZero()) {
            RecordBlocked("EzrealQ", "prediction-failed");
            return;
        }

        if (!m_q.IsReady()) {
            RecordBlocked("EzrealQ", "q-not-ready");
            return;
        }

        if (prediction.Hitchance == SDK::HitChance::Collision ||
            !prediction.CollisionObjects.empty()) {
            RecordBlocked("EzrealQ", "prediction-collision");
            return;
        }

        if (prediction.Hitchance == SDK::HitChance::OutOfRange) {
            RecordBlocked("EzrealQ", "prediction-out-of-range");
            return;
        }

        if (HitChanceValue(prediction.Hitchance) <
            HitChanceValue(SDK::HitChance::Medium)) {
            RecordBlocked("EzrealQ", "prediction-low-hitchance");
            return;
        }

        const bool ok = CoreCastSpell::CastPositionSpell(
            CoreCastSpell::SlotQ,
            castPosition);
        RecordAttempt("EzrealQ", ok, castPosition, target);
    }

    void OnRender() override {
        if (!m_lastPredictionValid ||
            !ImGui::GetCurrentContext() ||
            !SDK::Drawing::IsEnabled()) {
            return;
        }

        const int now = SDK::Game::TickCount();
        if (now - m_lastPredictionTick > 5000) {
            return;
        }

        const Vec3 castPosition = m_lastPrediction.GetCastPosition();
        const Vec3 unitPosition = m_lastPrediction.GetUnitPosition();
        const Vec3 sourcePosition = m_lastPrediction.Input.ResolveFrom();

        Vec2 castScreen;
        if (!SDK::Drawing::WorldToScreen(castPosition, castScreen) ||
            !SDK::Drawing::OnScreen(castScreen)) {
            return;
        }

        const bool hasCollision =
            m_lastPrediction.Hitchance == SDK::HitChance::Collision ||
            !m_lastPrediction.CollisionObjects.empty();
        const std::uint32_t mainColor =
            hasCollision ? 0xFFFF4040u : 0xFF40FF80u;
        const std::uint32_t softColor =
            hasCollision ? 0x99FF4040u : 0x9940FF80u;

        Vec2 sourceScreen;
        if (SDK::Drawing::WorldToScreen(sourcePosition, sourceScreen)) {
            SDK::Drawing::DrawLine(sourceScreen, castScreen, 2.0f, softColor);
        }

        SDK::Drawing::DrawCircle(castScreen, 8.0f, 2.0f, mainColor, 32);
        SDK::Drawing::DrawCircle(castScreen, 16.0f, 1.5f, softColor, 40);

        Vec2 unitScreen;
        if (SDK::Drawing::WorldToScreen(unitPosition, unitScreen) &&
            SDK::Drawing::OnScreen(unitScreen)) {
            SDK::Drawing::DrawCircle(unitScreen, 5.0f, 1.5f, 0xFFFFD24Au, 28);
            SDK::Drawing::DrawLine(unitScreen, castScreen, 1.0f, 0x88FFD24Au);
        }

        for (const auto& object : m_lastPrediction.CollisionObjects) {
            if (!object.IsValid()) {
                continue;
            }

            Vec2 collisionScreen;
            if (!SDK::Drawing::WorldToScreen(object.Position(), collisionScreen) ||
                !SDK::Drawing::OnScreen(collisionScreen)) {
                continue;
            }

            SDK::Drawing::DrawCircle(collisionScreen, 10.0f, 2.0f, 0xFFFF3030u, 32);
            SDK::Drawing::DrawText(
                collisionScreen.x + 12.0f,
                collisionScreen.y - 12.0f,
                0xFFFF6060u,
                "collision");
        }

        char text[192] = {};
        std::snprintf(
            text,
            sizeof(text),
            "Ezreal Q Prediction: %s origin=%s collisions=%zu",
            HitChanceName(m_lastPrediction.Hitchance),
            HitChanceName(m_lastPrediction.GetOriginHitchance()),
            m_lastPrediction.CollisionObjects.size());
        SDK::Drawing::DrawText(
            castScreen.x + 14.0f,
            castScreen.y - 28.0f,
            mainColor,
            text);
    }

    void DrawChampionDebug() override {
        ImGui::Text("A: Ezreal Q with SDK::Prediction + collision");
        ImGui::Text("Q setup: range=%.0f delay=%.2f width=%.0f speed=%.0f collision=%d type=Line",
                    m_q.CurrentRange(),
                    m_q.Delay,
                    m_q.Width,
                    m_q.Speed,
                    m_q.Collision ? 1 : 0);
        ImGui::Text("Last prediction: %s origin=%s collisions=%zu target=0x%llX",
                    m_lastPredictionValid ? HitChanceName(m_lastPrediction.Hitchance) : "none",
                    m_lastPredictionValid ? HitChanceName(m_lastPrediction.GetOriginHitchance()) : "none",
                    m_lastPredictionValid ? m_lastPrediction.CollisionObjects.size() : 0,
                    static_cast<unsigned long long>(m_lastTarget));
        ImGui::Text("Green point = cast position, yellow = predicted unit position, red = collision");
    }

    void OnChampionUnload() override {
        m_lastPredictionValid = false;
        m_lastTarget = 0;
    }

private:
    static int HitChanceValue(SDK::HitChance value) {
        return static_cast<int>(value);
    }

    static const char* HitChanceName(SDK::HitChance value) {
        switch (value) {
        case SDK::HitChance::None:
            return "None";
        case SDK::HitChance::Collision:
            return "Collision";
        case SDK::HitChance::OutOfRange:
            return "OutOfRange";
        case SDK::HitChance::Low:
            return "Low";
        case SDK::HitChance::Medium:
            return "Medium";
        case SDK::HitChance::High:
            return "High";
        case SDK::HitChance::VeryHigh:
            return "VeryHigh";
        case SDK::HitChance::Immobile:
            return "Immobile";
        case SDK::HitChance::Dash:
            return "Dash";
        default:
            return "?";
        }
    }

    void SetupQ() {
        // EnsoulSharp parity:
        // Q = new Spell(SpellSlot.Q, 1200f);
        // Q.SetSkillshot(0.25f, 60f, 2000f, true, SpellType.Line);
        m_q = SDK::Spell(SDK::SpellSlot::Q, 1200.0f);
        m_q.SetSkillshot(0.25f, 60.0f, 2000.0f, true, SDK::SpellType::Line);
        m_q.MinHitChance = SDK::HitChance::Medium;
    }

    SDK::PredictionInput BuildQPredictionInput(const SDK::AIBaseClient& target) const {
        SDK::PredictionInput input;
        input.Unit = target;
        input.Delay = m_q.Delay;
        input.Radius = m_q.Width;
        input.Speed = m_q.Speed;
        input.From = m_q.From;
        input.Range = m_q.CurrentRange();
        input.Collision = m_q.Collision;
        input.Type = SDK::SpellType::Line;
        input.Spell = &m_q;
        input.RangeCheckFrom = m_q.RangeCheckFrom;
        input.AoE = false;
        input.MaxCollisionCount = 0.0f;
        input.CollisionObjects = {
            SDK::CollisionableObjects::Minions,
            SDK::CollisionableObjects::Heroes,
            SDK::CollisionableObjects::YasuoWall,
            SDK::CollisionableObjects::SamiraWall,
            SDK::CollisionableObjects::MelWall,
        };
        return input;
    }

    void StoreLastPrediction(uintptr_t target, const SDK::PredictionOutput& prediction) {
        m_lastTarget = target;
        m_lastPrediction = prediction;
        m_lastPredictionTick = SDK::Game::TickCount();
        m_lastPredictionValid = true;
    }

    void LogPrediction(const char* phase,
                       uintptr_t target,
                       const SDK::PredictionOutput& prediction) {
        const Vec3 castPosition = prediction.GetCastPosition();
        const Vec3 unitPosition = prediction.GetUnitPosition();
        const std::uint32_t targetNetId = Globals::IsValidPtr(target)
            ? Globals::Read<std::uint32_t>(target + Offset::All::NetworkId)
            : 0;

        Appendf(
            "[%s] prediction phase=%s tick=%d target=0x%llX targetNet=%u hitchance=%s origin=%s collisions=%zu cast=%.1f %.1f %.1f unit=%.1f %.1f %.1f range=%.1f width=%.1f speed=%.1f delay=%.2f\r\n",
            DebugPrefix(),
            phase ? phase : "?",
            SDK::Game::TickCount(),
            static_cast<unsigned long long>(target),
            targetNetId,
            HitChanceName(prediction.Hitchance),
            HitChanceName(prediction.GetOriginHitchance()),
            prediction.CollisionObjects.size(),
            castPosition.x,
            castPosition.y,
            castPosition.z,
            unitPosition.x,
            unitPosition.y,
            unitPosition.z,
            m_q.CurrentRange(),
            m_q.Width,
            m_q.Speed,
            m_q.Delay);
    }

    MenuKeyBind* m_qKey = nullptr;
    bool m_qWasDown = false;
    SDK::Spell m_q{ SDK::SpellSlot::Q, 1200.0f };
    SDK::PredictionOutput m_lastPrediction = {};
    uintptr_t m_lastTarget = 0;
    int m_lastPredictionTick = 0;
    bool m_lastPredictionValid = false;
};

} // namespace Plugins
