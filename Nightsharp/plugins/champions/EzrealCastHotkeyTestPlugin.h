#pragma once

#include "../IPlugin.h"
#include "core/CoreBypass.h"
#include "core/CoreControl.h"
#include "core/CoreObjects.h"
#include "core/CoreRuntime.h"
#include "core/CoreSpellBook.h"
#include "core/CoreView.h"
#include "sdk/Core/Game.h"
#include "sdk/SDK.h"
#include "sdk/Wrappers/Spells/Spell.h"
#include "sdk/Wrappers/TargetSelector/TargetSelector.h"

#include <cstdio>

namespace Plugins {

class EzrealCastHotkeyTestPlugin : public IPlugin {
public:
    const char* GetName() const override { return "EzrealCastHotkeys"; }
    const char* GetInternalId() const override { return "ezreal_cast_hotkeys_test"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Misc; }
    bool AutoLoadByDefault() const override { return true; }

    bool CanLoad() const override {
        const auto player = SDK::ObjectManager::Player();
        return player.IsValid() && player.CharacterName() == "Ezreal";
    }

    void OnLoad() override {
        m_q = SDK::Spell(SDK::SpellSlot::Q, 1200.0f);
        m_q.SetSkillshot(0.25f, 53.0f, 2000.0f, true, SDK::SpellType::Line);

        m_r = SDK::Spell(SDK::SpellSlot::R, 5000.0f);
        m_r.SetSkillshot(1.0f, 160.0f, 2200.0f, false, SDK::SpellType::Line);

        Log("[EzrealCastHotkeys] loaded\r\n");
    }

    void OnUnload() override {
        Log("[EzrealCastHotkeys] unloaded\r\n");
    }

    void OnUpdate() override {
        using namespace SDK;

        const auto player = ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) {
            m_prevA = false;
            m_prevT = false;
            return;
        }

        if (!Game::IsFocused() || !Game::ShouldProcessInput() || Game::IsChatOpen() || Game::IsShopOpen()) {
            m_prevA = false;
            m_prevT = false;
            return;
        }

        const bool aDown = (GetAsyncKeyState('A') & 0x8000) != 0;
        const bool tDown = (GetAsyncKeyState('T') & 0x8000) != 0;

        if (aDown && !m_prevA) {
            TryCastQCursor(player);
        }
        if (tDown && !m_prevT) {
            TryCastRTarget(player);
        }

        m_prevA = aDown;
        m_prevT = tDown;
    }

private:
    static constexpr int CastModeNormal = 1;
    static constexpr int CastModeSmart = 2;
    static constexpr int CastPhasePress = 1;

    SDK::Spell m_q = {};
    SDK::Spell m_r = {};
    bool m_prevA = false;
    bool m_prevT = false;

    static void Log(const char* text) {
        if (!text || !*text) {
            return;
        }

        HANDLE hFile = CreateFileA(
            "C:\\Users\\Public\\EzrealCastHotkeys.txt",
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            return;
        }

        DWORD written = 0;
        WriteFile(hFile, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
        CloseHandle(hFile);
    }

    static bool CastRaw(int slotId, const Vec3& start, const Vec3& end, uint32_t targetNetId, int castMode) {
        auto& ctx = CoreRuntime::g_ctx;
        if (!CoreRuntime::IsWritePhase() || !CoreControl::CanCastSpell()) {
            return false;
        }

        if (!Globals::IsValidPtr(ctx.localPlayer) || !Globals::IsValidPtr(ctx.spoofTrampoline)) {
            return false;
        }

        const auto slot = CoreSpellBook::GetSlot(ctx.localPlayer, slotId);
        if (!slot.IsValid()) {
            return false;
        }

        const auto spellInput = slot.GetSpellInput();
        if (!Globals::IsValidPtr(spellInput)) {
            return false;
        }

        // Save SpellInput originals
        const auto origTargetNetId = Globals::Read<uint32_t>(spellInput + Offset::SpellBook::InputTargetNetId);
        const auto origStartPos = Globals::Read<Vec3>(spellInput + Offset::SpellBook::InputStartPos);
        const auto origEndPos = Globals::Read<Vec3>(spellInput + Offset::SpellBook::InputEndPos);
        const auto origEndPos2 = Globals::Read<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3));
        const auto origEndPos3 = Globals::Read<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3) * 2);

        // Write SpellInput
        if (!slot.SetInputData(targetNetId, start, end)) {
            return false;
        }

        // CastSpellPacket now tries the BB8A20 target-flow first, then falls back to CastSpellSafe.
        const bool ok = CoreControl::CastSpellPacket(slotId, start, end, targetNetId);

        // Restore SpellInput
        Globals::Write<uint32_t>(spellInput + Offset::SpellBook::InputTargetNetId, origTargetNetId);
        Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputStartPos, origStartPos);
        Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos, origEndPos);
        Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3), origEndPos2);
        Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3) * 2, origEndPos3);
        return ok;
    }

    void TryCastQCursor(const SDK::AIHeroClient& player) {
        if (!m_q.IsReady()) {
            Log("[EzrealCastHotkeys] A blocked: Q not ready\r\n");
            return;
        }

        Vec3 start = player.Position();
        Vec3 end = SDK::Game::CursorPos();
        if (!end.IsValid() || end.IsZero()) {
            Log("[EzrealCastHotkeys] A blocked: invalid cursor world pos\r\n");
            return;
        }

        const float range = m_q.GetRange();
        if (start.Distance2D(end) > range) {
            end = start.Extend(end, range - 5.0f);
        }

        const bool ok = CastRaw(0, start, end, 0, CastModeSmart);

        char buf[256] = {};
        std::snprintf(
            buf,
            sizeof(buf),
            "[EzrealCastHotkeys] A Q cursor ok=%d start=(%.1f %.1f %.1f) end=(%.1f %.1f %.1f)\r\n",
            ok ? 1 : 0,
            start.x,
            start.y,
            start.z,
            end.x,
            end.y,
            end.z);
        Log(buf);
    }

    void TryCastRTarget(const SDK::AIHeroClient& player) {
        if (!m_r.IsReady()) {
            Log("[EzrealCastHotkeys] T blocked: R not ready\r\n");
            return;
        }

        const auto target = SDK::TargetSelector::GetTarget(m_r.GetRange(), SDK::DamageType::Magical, player.Position());
        if (!target.IsValid()) {
            Log("[EzrealCastHotkeys] T blocked: no valid target\r\n");
            return;
        }

        const auto prediction = m_r.GetPrediction(target);
        const Vec3 start = player.Position();
        Vec3 end = prediction.CastPosition;
        if (!end.IsValid() || end.IsZero()) {
            end = target.Position();
        }

        const auto targetNetId = static_cast<uint32_t>(target.NetworkId());
        const bool ok = CastRaw(3, start, end, targetNetId, CastModeSmart);

        char buf[320] = {};
        std::snprintf(
            buf,
            sizeof(buf),
            "[EzrealCastHotkeys] T R target ok=%d netId=%u hitchance=%d mode=%d start=(%.1f %.1f %.1f) end=(%.1f %.1f %.1f)\r\n",
            ok ? 1 : 0,
            static_cast<unsigned int>(targetNetId),
            static_cast<int>(prediction.Hitchance),
            CastModeSmart,
            start.x,
            start.y,
            start.z,
            end.x,
            end.y,
            end.z);
        Log(buf);
    }
};

} // namespace Plugins
