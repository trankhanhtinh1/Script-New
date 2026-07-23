#pragma once

#include "CastSpellTestSupport.h"
#include "../../Core/CoreNewCastSpell.h"
#include "../../Core/CoreBuffs.h"
#include "../../Core/CoreSpellBook.h"

namespace Plugins {

class XerathSemiCastNew final : public CastSpellTestPluginBase {
public:
    const char* GetName() const override { return "Xerath Semi Cast New"; }
    const char* GetInternalId() const override { return "champion.xerath_semi_cast_new"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    const char* GetChampionName() const override { return "Xerath"; }
    bool CanLoad() const override { return CanLoadChampion("Xerath"); }

protected:
    const char* DebugPrefix() const override { return "XerathSemiCastNew"; }
    const char* LogPath() const override {
        return "C:\\Users\\Public\\nightsharp_xerath_semi_cast_new.txt";
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

        settings->Add(new MenuSeparator("qStatusHeader", "=== Xerath Q Semi-Cast Controls ==="));
        m_qKey = settings->Add(new MenuKeyBind(
            "autoChargeQNew",
            "Hold A to charge Q, release A to fire Q",
            SDK::Keys::A,
            SDK::KeyBindType::Press));
        m_showQStatus = settings->Add(new MenuBool(
            "showQStatusInfo",
            "Show Real Memory Q Charging Status in Menu",
            true));
    }

    void HandleGameUpdate(const SDK::Events::GameUpdateEventArgs&) override {
        if (!m_qKey) return;

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return;

        const bool isPhysicalKeyDown = (GetAsyncKeyState(m_qKey->Key) & 0x8000) != 0;
        const bool released = !isPhysicalKeyDown && m_qWasDown;
        m_qWasDown = isPhysicalKeyDown;

        const bool realBuffCharging = player.HasBuff("XerathArcanopulseChargeUp") || CoreSpellBook::IsCharging(player.Address());

        if (realBuffCharging && !m_isQCharging) {
            m_isQCharging = true;
            m_qChargeStart = SDK::Game::TickCount();
        }

        if (isPhysicalKeyDown && !m_isQCharging) {
            m_isQCharging = true;
            m_qChargeStart = SDK::Game::TickCount();
            BeginAutoQ();
        }

        if (m_isQCharging || realBuffCharging) {
            const int elapsed = std::max(0, static_cast<int>(SDK::Game::TickCount() - m_qChargeStart));
            const bool maxRangeReached = elapsed >= kRangeGrowMs;

            if (released || maxRangeReached) {
                ReleaseAutoQ();
            } else {
                UpdateAutoQHold();
            }
        }
    }

    void DrawChampionDebug() override {
        if (!m_showQStatus || !m_showQStatus->Value) {
            return;
        }

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return;

        const uintptr_t pAddr = player.Address();
        const bool hasChargeBuff = player.HasBuff("XerathArcanopulseChargeUp");
        const bool isSpellBookCharging = CoreSpellBook::IsCharging(pAddr);
        const bool isMemoryCharging = isSpellBookCharging || hasChargeBuff || m_isQCharging;
        const bool isPhysicalKeyDown = m_qKey && ((GetAsyncKeyState(m_qKey->Key) & 0x8000) != 0);

        const int elapsed = isMemoryCharging ? std::max(0, static_cast<int>(SDK::Game::TickCount() - m_qChargeStart)) : 0;
        const float fraction = std::clamp(static_cast<float>(elapsed) / static_cast<float>(kRangeGrowMs), 0.0f, 1.0f);
        const float currentRange = kMinQRange + (kMaxQRange - kMinQRange) * fraction;

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "=== Realtime Memory Q Charging Monitor ===");

        if (isMemoryCharging) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[REAL MEMORY STATUS]: CHARGING (ACTIVE)");
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[REAL MEMORY STATUS]: IDLE");
        }

        ImGui::Text("Key A State: %s", isPhysicalKeyDown ? "DOWN (Holding)" : "UP (Released)");
        ImGui::Text("HasBuff('XerathArcanopulseChargeUp'): %s", hasChargeBuff ? "YES (True)" : "NO (False)");
        ImGui::Text("CoreSpellBook IsCharging(): %s", isSpellBookCharging ? "YES (True)" : "NO (False)");
        ImGui::Text("Current Range: %.1f / %.1f", currentRange, kMaxQRange);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "=== Last Cast Attempt Telemetry ===");
        ImGui::Text("Last Tier Attempted: %s", m_lastTierAttempted[0] ? m_lastTierAttempted : "None");
        ImGui::Text("Last Result: %s (ok=%d)", m_lastSuccess ? "SUCCESS" : "FAILED", m_lastSuccess ? 1 : 0);
        ImGui::Text("Failure Reason: %s (code=%d)", CoreCastSpell::CastFailureName(m_lastTrace.failure), static_cast<int>(m_lastTrace.failure));
        ImGui::Text("CanCast Accepted: %s", m_lastTrace.canCastAccepted ? "YES" : "NO");
        ImGui::Text("Native Result: %lld", static_cast<long long>(m_lastTrace.nativeResult));

        ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), "Q Charge Level");
        ImGui::Separator();
    }

    void OnChampionUnload() override {
        m_qWasDown = false;
        m_isQCharging = false;
        m_target = 0;
        m_lastPrediction = {};
        m_lastSuccess = false;
        m_lastTierAttempted[0] = '\0';
    }

private:
    static constexpr float kMinQRange = 700.0f;
    static constexpr float kMaxQRange = 1450.0f;
    static constexpr int kRangeGrowMs = 1500;
    static constexpr int kMinimumHoldMs = 200;

    MenuKeyBind* m_qKey = nullptr;
    MenuBool* m_showQStatus = nullptr;
    SDK::Spell m_q{ SDK::SpellSlot::Q, kMinQRange };
    bool m_qWasDown = false;
    bool m_isQCharging = false;
    int m_qChargeStart = 0;
    uintptr_t m_target = 0;
    Vec3 m_lastPrediction = {};
    bool m_lastSuccess = false;
    char m_lastTierAttempted[64] = {};

    void LogTierResult(const char* tierName, bool ok, const Vec3& pos, uintptr_t target) {
        const auto trace = CoreCastSpell::LastTrace();
        m_lastTrace = trace;
        m_lastSuccess = ok;
        strncpy_s(m_lastTierAttempted, sizeof(m_lastTierAttempted), tierName ? tierName : "Unknown", _TRUNCATE);

        const std::uint32_t targetNetId = Globals::IsValidPtr(target)
            ? Globals::Read<std::uint32_t>(target + Offset::All::NetworkId)
            : 0;

        Appendf(
            "[%s] [%s] tick=%lu ok=%d failureReason='%s' (code=%d) canCastAccepted=%d nativeResult=%lld slot=%u target=0x%llX targetNet=%u pos=%.1f,%.1f,%.1f player=0x%llX spellbook=0x%llX spellSlot=0x%llX spellInput=0x%llX canCastFn=0x%llX castFn=0x%llX\r\n",
            DebugPrefix(),
            tierName ? tierName : "UnknownTier",
            SDK::Game::TickCount(),
            ok ? 1 : 0,
            CoreCastSpell::CastFailureName(trace.failure),
            static_cast<int>(trace.failure),
            trace.canCastAccepted ? 1 : 0,
            static_cast<long long>(trace.nativeResult),
            static_cast<unsigned>(trace.slot),
            static_cast<unsigned long long>(target),
            targetNetId,
            pos.x, pos.y, pos.z,
            static_cast<unsigned long long>(trace.localPlayer),
            static_cast<unsigned long long>(trace.spellbook),
            static_cast<unsigned long long>(trace.spellSlot),
            static_cast<unsigned long long>(trace.spellInput),
            static_cast<unsigned long long>(trace.canCastCheck),
            static_cast<unsigned long long>(trace.castSpellSafe));

        NightSharpDebug::Logf(
            "[%s] [%s] ok=%d failureReason='%s' (code=%d) canCast=%d res=%lld",
            DebugPrefix(),
            tierName ? tierName : "UnknownTier",
            ok ? 1 : 0,
            CoreCastSpell::CastFailureName(trace.failure),
            static_cast<int>(trace.failure),
            trace.canCastAccepted ? 1 : 0,
            static_cast<long long>(trace.nativeResult));
    }

    void BeginAutoQ() {
        if (IsChatTyping()) {
            RecordBlocked("XerathQBeginNew", "chat-typing");
            m_isQCharging = false;
            return;
        }

        m_target = ResolveComboTarget(kMaxQRange);
        if (Globals::IsValidPtr(m_target)) {
            m_lastPrediction = PredictTargetPosition(m_target, 0.50f, FLT_MAX);
        } else {
            m_lastPrediction = SDK::Game::CursorPosition();
        }

        if (!m_lastPrediction.IsValid() || m_lastPrediction.IsZero()) {
            RecordBlocked("XerathQBeginNew", "invalid-position");
            m_isQCharging = false;
            return;
        }

        const bool ok = CoreNewCastSpell::UpdateChargedSpellMethod1(
            static_cast<std::int32_t>(SDK::SpellSlot::Q),
            m_lastPrediction,
            false);
        LogTierResult("XerathQBeginNew", ok, m_lastPrediction, m_target);
        RecordAttempt("XerathQBeginNew", ok, m_lastPrediction, m_target);
    }

    void UpdateAutoQHold() {
        if (Globals::IsValidPtr(m_target)) {
            const Vec3 prediction = PredictTargetPosition(m_target, 0.50f, FLT_MAX);
            if (prediction.IsValid() && !prediction.IsZero()) {
                m_lastPrediction = prediction;
            }
        } else {
            m_lastPrediction = SDK::Game::CursorPosition();
        }
    }

    void ReleaseAutoQ() {
        if (!m_isQCharging) {
            return;
        }

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            m_isQCharging = false;
            return;
        }

        if (Globals::IsValidPtr(m_target)) {
            const Vec3 pred = PredictTargetPosition(m_target, 0.50f, FLT_MAX);
            if (pred.IsValid() && !pred.IsZero()) {
                m_lastPrediction = pred;
            }
        } else {
            m_lastPrediction = SDK::Game::CursorPosition();
        }

        if (!m_lastPrediction.IsValid() || m_lastPrediction.IsZero()) {
            m_lastPrediction = SDK::Game::CursorPosition();
        }

        const int elapsed = std::max(0, static_cast<int>(SDK::Game::TickCount() - m_qChargeStart));
        const float fraction = std::clamp(static_cast<float>(elapsed) / static_cast<float>(kRangeGrowMs), 0.0f, 1.0f);
        const float currentRange = kMinQRange + (kMaxQRange - kMinQRange) * fraction;

        const float targetDistance = Globals::IsValidPtr(m_target)
            ? player.Position().Distance2D(m_lastPrediction)
            : 0.0f;

        char detail[256] = {};
        std::snprintf(
            detail,
            sizeof(detail),
            "multi-release tick=%lu elapsed=%dms range=%.1f targetDistance=%.1f target=0x%llX prediction=%.1f %.1f %.1f",
            SDK::Game::TickCount(),
            elapsed,
            currentRange,
            targetDistance,
            static_cast<unsigned long long>(m_target),
            m_lastPrediction.x,
            m_lastPrediction.y,
            m_lastPrediction.z);
        Appendf("[%s] %s starting release attempt sequence...\r\n", DebugPrefix(), detail);

        // 1. Method 1 ReleaseActiveCharge Native (RVA 0xBC3B00)
        bool ok = CoreNewCastSpell::UpdateChargedSpellMethod1(
            static_cast<std::int32_t>(SDK::SpellSlot::Q),
            m_lastPrediction,
            true);
        LogTierResult("Tier1_UpdateChargedSpellMethod1", ok, m_lastPrediction, m_target);

        // 2. Native CastPositionSpell (RVA 0x97E690 - casting Q slot while charging fires Q!)
        if (!ok) {
            ok = CoreNewCastSpell::CastPositionSpellMethod2(0, m_lastPrediction);
            LogTierResult("Tier2_CastPositionSpellMethod2", ok, m_lastPrediction, m_target);
        }

        // 3. CoreCastSpell CastPositionSpell
        if (!ok) {
            ok = CoreCastSpell::CastPositionSpell(0, m_lastPrediction);
            LogTierResult("Tier3_CoreCastSpell_CastPositionSpell", ok, m_lastPrediction, m_target);
        }

        // 4. SDK Player CastSpell
        if (!ok) {
            ok = player.Spellbook().CastSpell(SDK::SpellSlot::Q, m_lastPrediction);
            LogTierResult("Tier4_Player_Spellbook_CastSpell", ok, m_lastPrediction, m_target);
        }

        Appendf("[%s] %s FINAL ok=%d\r\n", DebugPrefix(), detail, ok ? 1 : 0);
        RecordAttempt("XerathQReleaseNewMultiTier", ok, m_lastPrediction, m_target);

        m_isQCharging = false;
        m_target = 0;
        m_lastPrediction = {};
    }
};

} // namespace Plugins

