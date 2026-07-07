#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreBuffs.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../imgui/imgui.h"

#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace Plugins {

class PlayerBuffDebugPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Player Buff Debug"; }
    const char* GetInternalId() const override { return "core.player_buff_debug"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override {
        return CoreRuntime::EnsureInitialized() &&
               CoreRuntime::RefreshReadState();
    }

    void OnLoad() override {
        s_instance = this;
        ResetEntries();
        SDK::Events::hook.OnBuffAdd += &PlayerBuffDebugPlugin::OnBuffAdd;
        SDK::Events::hook.OnBuffRemove += &PlayerBuffDebugPlugin::OnBuffRemove;
        SDK::Events::hook.OnBuffUpdate += &PlayerBuffDebugPlugin::OnBuffUpdate;
        NightSharpDebug::Logf("[PlayerBuffDebug] loaded");
    }

    void OnUnload() override {
        SDK::Events::hook.OnBuffUpdate -= &PlayerBuffDebugPlugin::OnBuffUpdate;
        SDK::Events::hook.OnBuffRemove -= &PlayerBuffDebugPlugin::OnBuffRemove;
        SDK::Events::hook.OnBuffAdd -= &PlayerBuffDebugPlugin::OnBuffAdd;
        ResetEntries();
        if (s_instance == this) {
            s_instance = nullptr;
        }
        NightSharpDebug::Logf("[PlayerBuffDebug] unloaded");
    }

    void OnUpdate() override {
        RefreshIfDue();
    }

    void OnRender() override {
        if (!ImGui::GetCurrentContext()) {
            return;
        }

        RefreshIfDue();

        Entry entries[kMaxEntries] = {};
        int count = 0;
        CopyEntries(entries, count);

        const auto player = SDK::ObjectManager::Player();
        const float gameTime = SDK::Game::Time();
        const int now = SDK::Game::TickCount();

        ImGui::SetNextWindowBgAlpha(0.86f);
        ImGui::SetNextWindowSize(ImVec2(900.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("NightSharp Player Buff Debug")) {
            ImGui::End();
            return;
        }

        ImGui::Text("Player: %s  ptr=0x%llX  net=%d",
                    player.IsValid() ? player.CharacterName().c_str() : "--",
                    static_cast<unsigned long long>(player.Address()),
                    player.IsValid() ? player.NetworkId() : 0);
        ImGui::Text("Game %.2f  Tick %d  EventCache=%d  Refresh=%dms  Recent=%.1fs",
                    gameTime,
                    now,
                    CoreBuffs::IsEventCacheEnabled() ? 1 : 0,
                    m_refreshMs,
                    m_keepRecentSeconds);
        ImGui::Checkbox("Log state changes", &m_logChanges);
        ImGui::SameLine();
        ImGui::Checkbox("Show inactive recent", &m_showInactiveRecent);
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            ResetEntries();
        }

        DrawTable(entries, count, gameTime, now);
        ImGui::End();
    }

    void OnMenu() override {
        ImGui::Checkbox("Log buff state changes", &m_logChanges);
        ImGui::Checkbox("Show inactive recent buffs", &m_showInactiveRecent);
        ImGui::SliderInt("Refresh ms", &m_refreshMs, 50, 1000);
        ImGui::SliderFloat("Recent seconds", &m_keepRecentSeconds, 1.0f, 15.0f);
        if (ImGui::Button("Clear player buff debug")) {
            ResetEntries();
        }
    }

private:
    static inline PlayerBuffDebugPlugin* s_instance = nullptr;
    static constexpr int kMaxEntries = 256;
    static constexpr int kMaxBuffs = 256;

    struct Entry {
        bool active = false;
        bool initialized = false;
        bool hasBuff = false;
        bool live = false;
        bool liveSeenThisRefresh = false;
        bool previousHasBuff = false;
        bool previousLive = false;
        int sdkCount = 0;
        int liveStacks = 0;
        int eventCount = -1;
        int previousSdkCount = 0;
        int previousEventCount = -1;
        int type = -1;
        int lastEventTick = 0;
        float startTime = 0.0f;
        float endTime = 0.0f;
        float lastSeenTime = 0.0f;
        float lastChangeTime = 0.0f;
        uintptr_t address = 0;
        char name[96] = {};
        char lastEvent[12] = {};
    };

    mutable SRWLOCK m_lock = SRWLOCK_INIT;
    Entry m_entries[kMaxEntries] = {};
    int m_cursor = 0;
    float m_nextRefreshTime = 0.0f;
    int m_refreshMs = 150;
    float m_keepRecentSeconds = 6.0f;
    bool m_logChanges = true;
    bool m_showInactiveRecent = true;
    char m_suppressedRaw[kMaxEntries][96] = {};
    int m_suppressedRawCount = 0;

    static bool SameName(const char* left, const char* right) {
        if (!left || !right || !left[0] || !right[0]) {
            return false;
        }
        return _stricmp(left, right) == 0 ||
               CoreBuffs::NameMatchesQuery(left, right);
    }

    static void CopyText(char* out, int outCount, const char* value) {
        if (!out || outCount <= 0) {
            return;
        }

        out[0] = 0;
        if (value) {
            strncpy_s(out, static_cast<std::size_t>(outCount), value, _TRUNCATE);
        }
    }

    static const char* BoolText(bool value) {
        return value ? "1" : "0";
    }

    static float RemainingTime(const Entry& entry, float gameTime) {
        if (entry.endTime <= 0.0f) {
            return 0.0f;
        }
        return entry.endTime > gameTime ? entry.endTime - gameTime : 0.0f;
    }

    int FindEntryLocked(const char* name) const {
        if (!name || !name[0]) {
            return -1;
        }

        for (int i = 0; i < kMaxEntries; ++i) {
            if (m_entries[i].active && SameName(m_entries[i].name, name)) {
                return i;
            }
        }
        return -1;
    }

    int FindFreeEntryLocked() {
        for (int i = 0; i < kMaxEntries; ++i) {
            if (!m_entries[i].active) {
                return i;
            }
        }
        return m_cursor++ % kMaxEntries;
    }

    Entry& EnsureEntryLocked(const char* name) {
        int index = FindEntryLocked(name);
        if (index < 0) {
            index = FindFreeEntryLocked();
            m_entries[index] = {};
            m_entries[index].active = true;
            CopyText(m_entries[index].name,
                     static_cast<int>(sizeof(m_entries[index].name)),
                     name);
        }
        return m_entries[index];
    }

    void RemoveEntryLocked(const char* name) {
        const int index = FindEntryLocked(name);
        if (index >= 0) {
            m_entries[index] = {};
        }
    }

    bool IsRawSuppressedLocked(const char* name) const {
        if (!name || !name[0]) {
            return false;
        }

        for (int i = 0; i < m_suppressedRawCount; ++i) {
            if (SameName(m_suppressedRaw[i], name)) {
                return true;
            }
        }
        return false;
    }

    void SuppressRawLocked(const char* name) {
        if (!name || !name[0] || IsRawSuppressedLocked(name)) {
            return;
        }

        const int index = m_suppressedRawCount < kMaxEntries
            ? m_suppressedRawCount++
            : 0;
        CopyText(m_suppressedRaw[index],
                 static_cast<int>(sizeof(m_suppressedRaw[index])),
                 name);
    }

    void UnsuppressRawLocked(const char* name) {
        if (!name || !name[0]) {
            return;
        }

        for (int i = 0; i < m_suppressedRawCount;) {
            if (!SameName(m_suppressedRaw[i], name)) {
                ++i;
                continue;
            }

            for (int j = i; j + 1 < m_suppressedRawCount; ++j) {
                CopyText(m_suppressedRaw[j],
                         static_cast<int>(sizeof(m_suppressedRaw[j])),
                         m_suppressedRaw[j + 1]);
            }
            m_suppressedRaw[--m_suppressedRawCount][0] = 0;
        }
    }

    void ResetEntries() {
        AcquireSRWLockExclusive(&m_lock);
        for (auto& entry : m_entries) {
            entry = {};
        }
        for (auto& suppressed : m_suppressedRaw) {
            suppressed[0] = 0;
        }
        m_cursor = 0;
        m_nextRefreshTime = 0.0f;
        m_suppressedRawCount = 0;
        ReleaseSRWLockExclusive(&m_lock);
    }

    void CopyEntries(Entry* out, int& count) const {
        count = 0;
        if (!out) {
            return;
        }

        AcquireSRWLockShared(&m_lock);
        for (const auto& entry : m_entries) {
            if (!entry.active || count >= kMaxEntries) {
                continue;
            }
            out[count++] = entry;
        }
        ReleaseSRWLockShared(&m_lock);
    }

    void RefreshIfDue() {
        const float gameTime = SDK::Game::Time();
        if (gameTime <= 0.0f || gameTime < m_nextRefreshTime) {
            return;
        }

        m_nextRefreshTime =
            gameTime + static_cast<float>(std::clamp(m_refreshMs, 50, 1000)) / 1000.0f;
        Refresh(gameTime);
    }

    void Refresh(float gameTime) {
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            return;
        }

        uintptr_t buffs[kMaxBuffs] = {};
        const int buffCount = CoreBuffs::Enumerate(player.Address(), buffs, kMaxBuffs);

        AcquireSRWLockExclusive(&m_lock);

        for (auto& entry : m_entries) {
            if (entry.active) {
                entry.previousHasBuff = entry.hasBuff;
                entry.previousLive = entry.live;
                entry.previousSdkCount = entry.sdkCount;
                entry.previousEventCount = entry.eventCount;
                entry.liveSeenThisRefresh = false;
                entry.live = false;
                entry.liveStacks = 0;
                entry.type = -1;
                entry.address = 0;
            }
        }

        char name[96] = {};
        for (int i = 0; i < buffCount; ++i) {
            CoreBuffs::BuffRef buff{ buffs[i] };
            if (!buff.ReadName(name, static_cast<int>(sizeof(name)))) {
                continue;
            }
            if (IsRawSuppressedLocked(name)) {
                continue;
            }

            Entry& entry = EnsureEntryLocked(name);
            entry.address = buff.address;
            entry.live = buff.IsActive(gameTime);
            entry.liveSeenThisRefresh = true;
            entry.liveStacks = buff.GetStacks();
            entry.type = buff.GetType();
            entry.startTime = buff.GetStartTime();
            entry.endTime = buff.GetEndTime();
            if (entry.live) {
                entry.lastSeenTime = gameTime;
            }
        }

        for (auto& entry : m_entries) {
            if (!entry.active || !entry.name[0]) {
                continue;
            }

            entry.hasBuff = player.HasBuff(entry.name);
            entry.sdkCount = player.GetBuffCount(entry.name);

            const bool liveRecently =
                gameTime - entry.lastSeenTime <= m_keepRecentSeconds;
            const bool eventRecently =
                gameTime - entry.lastChangeTime <= m_keepRecentSeconds;
            if (!entry.live && !entry.hasBuff && !liveRecently && !eventRecently) {
                entry = {};
                continue;
            }

            const bool changed =
                !entry.initialized ||
                entry.previousHasBuff != entry.hasBuff ||
                entry.previousLive != entry.live ||
                entry.previousSdkCount != entry.sdkCount ||
                entry.previousEventCount != entry.eventCount;
            if (changed) {
                entry.initialized = true;
                entry.lastChangeTime = gameTime;
                if (m_logChanges) {
                    LogEntry("refresh", entry);
                }
            }
        }

        ReleaseSRWLockExclusive(&m_lock);
    }

    void HandleBuffEvent(const char* eventName, const SDK::Events::BuffEventArgs& args) {
        if (!SDK::Events::IsLocalPlayer(args.Sender) || !args.BuffName[0]) {
            return;
        }

        const float gameTime = SDK::Game::Time();
        AcquireSRWLockExclusive(&m_lock);

        const bool removeEvent = eventName && _stricmp(eventName, "remove") == 0;
        const bool updateEvent = eventName && _stricmp(eventName, "update") == 0;
        if (removeEvent || (updateEvent && args.Count <= 0)) {
            SuppressRawLocked(args.BuffName);
            RemoveEntryLocked(args.BuffName);
            if (m_logChanges) {
                NightSharpDebug::Logf(
                    "[PlayerBuffDebug] remove-display name=%s event=%s count=%d",
                    args.BuffName,
                    eventName ? eventName : "?",
                    args.Count);
            }
            ReleaseSRWLockExclusive(&m_lock);
            return;
        }

        UnsuppressRawLocked(args.BuffName);

        Entry& entry = EnsureEntryLocked(args.BuffName);
        entry.eventCount = args.Count;
        entry.lastEventTick = SDK::Game::TickCount();
        entry.lastChangeTime = gameTime;
        CopyText(entry.lastEvent,
                 static_cast<int>(sizeof(entry.lastEvent)),
                 eventName);
        if (args.Count > 0) {
            entry.lastSeenTime = gameTime;
        }
        if (m_logChanges) {
            LogEntry(eventName, entry);
        }
        ReleaseSRWLockExclusive(&m_lock);
    }

    static void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
        if (s_instance) {
            s_instance->HandleBuffEvent("add", args);
        }
    }

    static void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
        if (s_instance) {
            s_instance->HandleBuffEvent("remove", args);
        }
    }

    static void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
        if (s_instance) {
            s_instance->HandleBuffEvent("update", args);
        }
    }

    static void LogEntry(const char* source, const Entry& entry) {
        NightSharpDebug::Logf(
            "[PlayerBuffDebug] %s name=%s has=%d sdkCount=%d live=%d "
            "liveStacks=%d eventCount=%d type=%d buff=0x%llX event=%s",
            source ? source : "?",
            entry.name,
            entry.hasBuff ? 1 : 0,
            entry.sdkCount,
            entry.live ? 1 : 0,
            entry.liveStacks,
            entry.eventCount,
            entry.type,
            static_cast<unsigned long long>(entry.address),
            entry.lastEvent[0] ? entry.lastEvent : "-");
    }

    void DrawTable(const Entry* entries, int count, float gameTime, int now) const {
        if (!entries || count <= 0) {
            ImGui::TextUnformatted("No player buffs tracked yet.");
            return;
        }

        if (!ImGui::BeginTable(
                "PlayerBuffDebugTable",
                11,
                ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_SizingFixedFit,
                ImVec2(0.0f, 420.0f))) {
            return;
        }

        ImGui::TableSetupColumn("Buff");
        ImGui::TableSetupColumn("HasBuff");
        ImGui::TableSetupColumn("Count");
        ImGui::TableSetupColumn("RawLive");
        ImGui::TableSetupColumn("RawStacks");
        ImGui::TableSetupColumn("Event");
        ImGui::TableSetupColumn("EvtCnt");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Remain");
        ImGui::TableSetupColumn("Age");
        ImGui::TableSetupColumn("Ptr");
        ImGui::TableHeadersRow();

        for (int i = 0; i < count; ++i) {
            const Entry& entry = entries[i];
            if (!entry.active || !entry.name[0]) {
                continue;
            }
            if (!m_showInactiveRecent && !entry.live && !entry.hasBuff) {
                continue;
            }

            const int ageMs = entry.lastEventTick > 0 && now >= entry.lastEventTick
                ? now - entry.lastEventTick
                : 0;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(entry.name);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(BoolText(entry.hasBuff));
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d", entry.sdkCount);
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(BoolText(entry.live));
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", entry.liveStacks);
            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(entry.lastEvent[0] ? entry.lastEvent : "-");
            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%d", entry.eventCount);
            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%d", entry.type);
            ImGui::TableSetColumnIndex(8);
            ImGui::Text("%.2f", RemainingTime(entry, gameTime));
            ImGui::TableSetColumnIndex(9);
            ImGui::Text("%dms", ageMs);
            ImGui::TableSetColumnIndex(10);
            ImGui::Text("0x%llX", static_cast<unsigned long long>(entry.address));
        }

        ImGui::EndTable();
    }
};

} // namespace Plugins
