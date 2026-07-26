#pragma once

#include "CastSpellTestSupport.h"
#include "../../Core/CoreNewCastSpell.h"

#include <algorithm>
#include <cfloat>
#include <cstdint>

namespace Plugins {

// Live test rig for the cursor-free charged cast.
//
// Xerath Q normally aims wherever the HUD cursor points, because the game's own
// charge path runs CanCastCheck -> PrimeCastPosition, which raycasts the mouse.
// CoreNewCastSpell::UpdateChargedSpellMethod1 skips that and hands the native
// charge sender explicit world coordinates instead, so the shot lands on our
// prediction no matter where the cursor sits.
//
// Every log line records the prediction and the live cursor side by side: if the
// two diverge and Q still lands on the prediction, the mouse dependency is gone.
class XerathSemiNewCastSpell final : public CastSpellTestPluginBase {
public:
    const char* GetName() const override { return "Xerath Semi New CastSpell"; }
    const char* GetInternalId() const override {
        return "champion.xerath_semi_new_cast_spell";
    }
    const char* GetAuthor() const override { return "NightSharp"; }
    const char* GetChampionName() const override { return "Xerath"; }
    bool CanLoad() const override { return CanLoadChampion("Xerath"); }

protected:
    const char* DebugPrefix() const override { return "XerathSemiNewCastSpell"; }
    const char* LogPath() const override {
        return "C:\\Users\\Public\\nightsharp_xerath_semi_new_cast_spell.txt";
    }

    void BuildChampionMenu(Menu* settings) override {
        m_autoKey = settings->Add(new MenuKeyBind(
            "autoQMethod1",
            "A: auto Q method1",
            SDK::Keys::A,
            SDK::KeyBindType::Press));
        m_beginKey = settings->Add(new MenuKeyBind(
            "beginQMethod1",
            "S: begin Q method1",
            SDK::Keys::S,
            SDK::KeyBindType::Press));
        m_releaseKey = settings->Add(new MenuKeyBind(
            "releaseQMethod1",
            "D: release Q method1",
            SDK::Keys::D,
            SDK::KeyBindType::Press));
    }

    void HandleGameUpdate(const SDK::Events::GameUpdateEventArgs&) override {
        const bool autoPressed = Pressed(m_autoKey, m_autoWasDown);
        const bool beginPressed = Pressed(m_beginKey, m_beginWasDown);
        const bool releasePressed = Pressed(m_releaseKey, m_releaseWasDown);

        if (autoPressed) {
            ArmQ("XerathQNewMethod1Arm");
        }

        if (beginPressed) {
            BeginQ(false, "XerathQNewMethod1BeginOnly");
        }

        if (releasePressed) {
            ReleaseQ("XerathQNewMethod1ReleaseOnly");
        }

        if (!m_charging) {
            return;
        }

        UpdateChargeState();
        if (m_charging) {
            ReaimQ();
        }
        if (m_charging && m_autoRelease) {
            TryAutoRelease();
        }
    }

    void DrawChampionDebug() override {
        ImGui::Text("A: auto Q method1");
        ImGui::Text("S: begin Q method1  D: release Q method1");
        ImGui::Text("Core: CoreNewCastSpell::UpdateChargedSpellMethod1");
        ImGui::Text(
            "Charge: %s  client=%d  auto=%d  elapsed=%dms  range=%.1f  reaims=%d",
            m_charging ? "active" : "idle",
            CoreNewCastSpell::IsCharging(
                static_cast<std::int32_t>(SDK::SpellSlot::Q)) ? 1 : 0,
            m_autoRelease ? 1 : 0,
            m_charging ? SDK::Game::TickCount() - m_chargeStartTick : 0,
            m_currentRange,
            m_reaimCount);
    }

    void OnChampionUnload() override {
        ResetCharge();
        m_autoWasDown = false;
        m_beginWasDown = false;
        m_releaseWasDown = false;
    }

private:
    static constexpr float kMinQRange = 700.0f;
    static constexpr float kMaxQRange = 1450.0f;
    static constexpr int kRangeGrowMs = 1500;
    // Minimum time the charge stays open. Every millisecond here is a millisecond
    // the game's own cursor stream gets to draw the charge, so keep it just above
    // the round trip: the log shows the server acks the charge ~70 ms after begin.
    static constexpr int kMinimumHoldMs = 80;
    // Re-aim every tick. There is no drift threshold on purpose — the client's
    // charge tick pushes a cursor position packet every single frame, so skipping a
    // tick because our prediction has not moved simply hands that frame's aim to
    // the mouse. Matching its cadence is what keeps the charge pointed at us.
    static constexpr int kReaimIntervalMs = 0;

    MenuKeyBind* m_autoKey = nullptr;
    MenuKeyBind* m_beginKey = nullptr;
    MenuKeyBind* m_releaseKey = nullptr;
    bool m_autoWasDown = false;
    bool m_beginWasDown = false;
    bool m_releaseWasDown = false;
    bool m_charging = false;
    bool m_autoRelease = false;
    int m_chargeStartTick = 0;
    int m_lastReaimTick = 0;
    int m_reaimCount = 0;
    uintptr_t m_target = 0;
    Vec3 m_lastPrediction = {};
    Vec3 m_lastSentAim = {};
    float m_currentRange = kMinQRange;

    static bool Pressed(const MenuKeyBind* key, bool& wasDown) {
        const bool down = KeyDown(key);
        const bool pressed = down && !wasDown;
        wasDown = down;
        return pressed;
    }

    // castContext + 0x38, not the champion buff. XerathArcanopulseChargeUp lingers
    // for a few frames after the release, and gating on it made the next keypress
    // take the "already charging" branch and fail with missing-charge-state.
    static bool IsNativeCharging() {
        return CoreNewCastSpell::IsCharging(
            static_cast<std::int32_t>(SDK::SpellSlot::Q));
    }

    // How long a charge we did not start has already been held, taken from the
    // charge-open timestamp InitChargeState left in the cast context. 0 when that
    // timestamp is unavailable, which makes the adopted charge grow its range from
    // scratch — slower than reality, but never fires short.
    static int AdoptedElapsedMs() {
        const float startTime = CoreNewCastSpell::ChargeStartTime(
            static_cast<std::int32_t>(SDK::SpellSlot::Q));
        const float now = SDK::Game::Time();
        if (startTime <= 0.0f || now <= startTime) {
            return 0;
        }

        const int held = static_cast<int>((now - startTime) * 1000.0f);
        return std::clamp(held, 0, kRangeGrowMs);
    }

    void ResetCharge() {
        m_charging = false;
        m_autoRelease = false;
        m_chargeStartTick = 0;
        m_lastReaimTick = 0;
        m_reaimCount = 0;
        m_target = 0;
        m_lastPrediction = {};
        m_lastSentAim = {};
        m_currentRange = kMinQRange;
    }

    float CurrentChargeRange(int elapsed) const {
        const float progress = std::min(
            1.0f,
            static_cast<float>(std::max(0, elapsed)) /
                static_cast<float>(kRangeGrowMs));
        return kMinQRange + (kMaxQRange - kMinQRange) * progress;
    }

    bool ResolvePrediction(Vec3& prediction, uintptr_t& target) {
        target = Globals::IsValidPtr(m_target)
            ? m_target
            : ResolveComboTarget(kMaxQRange);
        if (!Globals::IsValidPtr(target)) {
            prediction = {};
            return false;
        }

        prediction = PredictTargetPosition(target, 0.50f, FLT_MAX);
        if (!prediction.IsValid() || prediction.IsZero()) {
            prediction = SDK::AIHeroClient(target).Position();
        }

        return prediction.IsValid() && !prediction.IsZero();
    }

    bool RefreshPrediction() {
        uintptr_t target = 0;
        Vec3 prediction = {};
        if (!ResolvePrediction(prediction, target)) {
            return false;
        }

        m_target = target;
        m_lastPrediction = prediction;
        return true;
    }

    // The auto key arms the charge, it does not toggle it. Firing on the second
    // press meant that holding Q by hand and then pressing the key released
    // instantly, long before the charge had grown enough range to reach anything.
    // Now an existing charge is adopted and auto-release decides when it goes off;
    // the dedicated release key is still there for a forced shot.
    void ArmQ(const char* action) {
        if (IsChatTyping()) {
            RecordBlocked(action, "chat-typing");
            return;
        }

        if (m_charging) {
            m_autoRelease = true;
            return;
        }

        if (IsNativeCharging()) {
            AdoptNativeCharge(action);
            return;
        }

        BeginQ(true, "XerathQNewMethod1AutoBegin");
    }

    // Takes over a charge opened by someone else (the player's own Q key), which
    // was aimed by the HUD cursor. Clearing the last-sent aim forces the next tick
    // to re-aim onto the prediction and overwrite that cursor-derived direction.
    void AdoptNativeCharge(const char* action) {
        uintptr_t target = 0;
        Vec3 prediction = {};
        if (!ResolvePrediction(prediction, target)) {
            RecordBlocked(action, "no-target-or-prediction");
            return;
        }

        const int elapsed = AdoptedElapsedMs();
        m_charging = true;
        m_autoRelease = true;
        m_chargeStartTick = SDK::Game::TickCount() - elapsed;
        m_lastReaimTick = 0;
        m_reaimCount = 0;
        m_target = target;
        m_lastPrediction = prediction;
        m_lastSentAim = {};
        m_currentRange = CurrentChargeRange(elapsed);

        Appendf(
            "[XerathSemiNewCastSpell] adopted-native-charge tick=%d action=%s elapsed=%d range=%.1f target=0x%llX prediction=%.1f %.1f %.1f\r\n",
            SDK::Game::TickCount(),
            action ? action : "?",
            elapsed,
            m_currentRange,
            static_cast<unsigned long long>(target),
            prediction.x,
            prediction.y,
            prediction.z);
    }

    void BeginQ(bool autoRelease, const char* action) {
        if (IsChatTyping()) {
            RecordBlocked(action, "chat-typing");
            return;
        }

        if (m_charging || IsNativeCharging()) {
            RecordBlocked(action, "already-charging");
            return;
        }

        uintptr_t target = 0;
        Vec3 prediction = {};
        if (!ResolvePrediction(prediction, target)) {
            RecordBlocked(action, "no-target-or-prediction");
            ResetCharge();
            return;
        }

        const bool ok = CoreNewCastSpell::UpdateChargedSpellMethod1(
            static_cast<std::int32_t>(SDK::SpellSlot::Q),
            prediction,
            false);
        LogMethod1("begin", action, ok, target, prediction);
        RecordAttempt(action, ok, prediction, target);

        if (!ok) {
            ResetCharge();
            return;
        }

        m_charging = true;
        m_autoRelease = autoRelease;
        m_chargeStartTick = SDK::Game::TickCount();
        m_lastReaimTick = m_chargeStartTick;
        m_reaimCount = 0;
        m_target = target;
        m_lastPrediction = prediction;
        m_lastSentAim = prediction;
        m_currentRange = kMinQRange;
    }

    // Keeps the open charge pointed at the current prediction. This is the part
    // that actually removes the mouse from the equation: without it the charge
    // keeps whatever aim it was opened with.
    void ReaimQ() {
        const int now = SDK::Game::TickCount();
        if (now - m_lastReaimTick < kReaimIntervalMs) {
            return;
        }

        if (!RefreshPrediction()) {
            return;
        }

        m_lastReaimTick = now;

        const bool ok = CoreNewCastSpell::UpdateChargedSpellMethod1(
            static_cast<std::int32_t>(SDK::SpellSlot::Q),
            m_lastPrediction,
            false);
        if (ok) {
            ++m_reaimCount;
            m_lastSentAim = m_lastPrediction;
        } else {
            LogMethod1("reaim", "XerathQNewMethod1Reaim", ok, m_target,
                       m_lastPrediction);
        }
    }

    void ReleaseQ(const char* action) {
        if (IsChatTyping()) {
            RecordBlocked(action, "chat-typing");
            return;
        }

        if (!m_charging && !IsNativeCharging()) {
            RecordBlocked(action, "not-charging");
            return;
        }

        if (!RefreshPrediction() &&
            (!m_lastPrediction.IsValid() || m_lastPrediction.IsZero())) {
            RecordBlocked(action, "lost-target-position");
            return;
        }

        const bool ok = CoreNewCastSpell::UpdateChargedSpellMethod1(
            static_cast<std::int32_t>(SDK::SpellSlot::Q),
            m_lastPrediction,
            true);
        LogMethod1("release", action, ok, m_target, m_lastPrediction);
        RecordAttempt(action, ok, m_lastPrediction, m_target);

        if (ok || !IsNativeCharging()) {
            ResetCharge();
        }
    }

    void UpdateChargeState() {
        const int elapsed = SDK::Game::TickCount() - m_chargeStartTick;
        m_currentRange = CurrentChargeRange(elapsed);

        if (Globals::IsValidPtr(m_target)) {
            (void)RefreshPrediction();
        }

        if (!IsNativeCharging() && elapsed > 350) {
            Appendf(
                "[XerathSemiNewCastSpell] charge-ended tick=%d elapsed=%d reaims=%d target=0x%llX last=%.1f %.1f %.1f\r\n",
                SDK::Game::TickCount(),
                elapsed,
                m_reaimCount,
                static_cast<unsigned long long>(m_target),
                m_lastPrediction.x,
                m_lastPrediction.y,
                m_lastPrediction.z);
            ResetCharge();
        }
    }

    void TryAutoRelease() {
        if (!m_lastPrediction.IsValid() || m_lastPrediction.IsZero()) {
            return;
        }

        const int elapsed = SDK::Game::TickCount() - m_chargeStartTick;
        const Vec3 playerPosition = SDK::ObjectManager::Player().Position();
        const float targetDistance = playerPosition.Distance2D(m_lastPrediction);
        const bool enoughRange =
            elapsed >= kMinimumHoldMs &&
            targetDistance <= (m_currentRange - 20.0f);
        const bool maxRangeReached = elapsed >= kRangeGrowMs;

        if (enoughRange || maxRangeReached) {
            Appendf(
                "[XerathSemiNewCastSpell] auto-release-ready tick=%d elapsed=%d range=%.1f distance=%.1f enough=%d max=%d\r\n",
                SDK::Game::TickCount(),
                elapsed,
                m_currentRange,
                targetDistance,
                enoughRange ? 1 : 0,
                maxRangeReached ? 1 : 0);
            ReleaseQ("XerathQNewMethod1AutoRelease");
        }
    }

    void LogMethod1(const char* phase,
                    const char* action,
                    bool ok,
                    uintptr_t target,
                    const Vec3& prediction) {
        const auto& trace = CoreNewCastSpell::LastTrace();
        const Vec3 cursor = SDK::Game::CursorPos();
        const std::uint32_t targetNetId = Globals::IsValidPtr(target)
            ? Globals::Read<std::uint32_t>(target + Offset::All::NetworkId)
            : 0;
        const std::uint32_t targetIndex = Globals::IsValidPtr(target)
            ? Globals::Read<std::uint32_t>(target + Offset::All::Index)
            : 0;
        const uintptr_t chargeInput = Globals::IsValidPtr(trace.castContext)
            ? Globals::Read<uintptr_t>(
                trace.castContext + CoreCastSpell::kHudChargeSpellInput)
            : 0;
        // How far the cursor sits from where we actually aimed. A large value on a
        // successful shot is the proof that the cast ignored the mouse.
        const float cursorDelta =
            (cursor.IsValid() && prediction.IsValid())
                ? cursor.Distance2D(prediction)
                : -1.0f;

        Appendf(
            "[XerathSemiNewCastSpell] method1-charge tick=%d phase=%s action=%s ok=%d target=0x%llX net=%u index=0x%X prediction=%.1f %.1f %.1f cursor=%.1f %.1f %.1f cursorDelta=%.1f reaims=%d chargeInput=0x%llX spellInput=0x%llX provider=0x%llX kind=%s failure=%s virtualCursor=%d nativeResult=%lld chargeFn=0x%llX context=0x%llX spellbook=0x%llX spellSlot=0x%llX\r\n",
            SDK::Game::TickCount(),
            phase ? phase : "?",
            action ? action : "?",
            ok ? 1 : 0,
            static_cast<unsigned long long>(target),
            targetNetId,
            targetIndex,
            prediction.x,
            prediction.y,
            prediction.z,
            cursor.x,
            cursor.y,
            cursor.z,
            cursorDelta,
            m_reaimCount,
            static_cast<unsigned long long>(chargeInput),
            static_cast<unsigned long long>(trace.spellInput),
            static_cast<unsigned long long>(trace.runtimeInput),
            CoreCastSpell::CastKindName(trace.kind),
            CoreCastSpell::CastFailureName(trace.failure),
            trace.virtualCursorApplied ? 1 : 0,
            static_cast<long long>(trace.nativeResult),
            static_cast<unsigned long long>(trace.castSpellSafe),
            static_cast<unsigned long long>(trace.castContext),
            static_cast<unsigned long long>(trace.spellbook),
            static_cast<unsigned long long>(trace.spellSlot));
    }
};

} // namespace Plugins
