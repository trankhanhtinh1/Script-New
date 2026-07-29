#pragma once
#include "IDeveloperTab.h"
#include "../DeveloperToolsPlugin.h"
#include <mutex>
#include <string>
#include <vector>

namespace Plugins::DevTools {

class PlayerBuffInspectorTab final : public IDeveloperTab {
private:
    struct BuffEntry {
        bool active = false;
        bool initialized = false;
        bool hasBuff = false;
        bool live = false;
        bool liveSeenThisRefresh = false;
        bool previousHasBuff = false;
        bool previousLive = false;
        int sdkCount = 0;
        int liveStacks = 0;
        int previousSdkCount = 0;
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

    struct LiveTargetInfo {
        bool isValid = false;
        std::string characterName;
        std::string name;
        uintptr_t address = 0;
        bool isLocalPlayer = false;
    };

    struct BuffObservation {
        char name[96] = {};
        int stacks = 0;
        int type = -1;
        float startTime = 0.0f;
        float endTime = 0.0f;
        uintptr_t address = 0;
        bool live = false;
    };

    mutable SRWLOCK buffLock_ = SRWLOCK_INIT;
    BuffEntry buffEntries_[256] = {};
    int buffCursor_ = 0;
    float nextBuffRefreshTime_ = 0.0f;
    int buffRefreshMs_ = 150;
    float buffKeepRecentSeconds_ = 6.0f;
    bool buffLogChanges_ = true;
    bool buffShowInactiveRecent_ = true;
    char suppressedBuffRaw_[256][96] = {};
    int suppressedBuffRawCount_ = 0;
    std::uint32_t lastTargetNetId_ = 0;
    int inspectMode_ = 0; // 0 = Live, 1 = Snapshot

    mutable std::mutex targetLock_;
    LiveTargetInfo cachedTargetInfo_;
    int lastUpdateTick_ = 0;

public:
    using IDeveloperTab::IDeveloperTab;

    const char* GetTabName() const override { return "Buffs"; }

    void OnUpdate() override {
        if (!plugin_->enabled_) {
            std::lock_guard<std::mutex> lk(targetLock_);
            cachedTargetInfo_ = {};
            return;
        }

        if (inspectMode_ == 1) {
            std::lock_guard<std::mutex> lk(targetLock_);
            cachedTargetInfo_ = {};
            return; // No live updates when inspecting snapshot
        }

        const int now = SDK::Variables::TickCount();
        if (now - lastUpdateTick_ < 100) {
            return;
        }
        lastUpdateTick_ = now;

        const auto target = plugin_->GetFocusedObject();
        const std::uint32_t currentNetId = target.IsValid() ? static_cast<std::uint32_t>(target.NetworkId()) : 0;
        if (currentNetId != lastTargetNetId_) {
            ResetBuffEntries();
            lastTargetNetId_ = currentNetId;
        }

        LiveTargetInfo targetInfo;
        if (target.IsValid()) {
            targetInfo.isValid = true;
            targetInfo.characterName = target.CharacterName();
            targetInfo.name = target.Name();
            targetInfo.address = target.Address();
            const auto localPlayer = SDK::ObjectManager::Player();
            targetInfo.isLocalPlayer = localPlayer.IsValid() && target.Address() == localPlayer.Address();
        }

        {
            std::lock_guard<std::mutex> lk(targetLock_);
            cachedTargetInfo_ = std::move(targetInfo);
        }

        const float gameTime = SDK::Game::Time();
        if (gameTime <= 0.0f || gameTime < nextBuffRefreshTime_) {
            return;
        }
        nextBuffRefreshTime_ =
            gameTime + static_cast<float>(std::clamp(buffRefreshMs_, 50, 1000)) / 1000.0f;
        RefreshPlayerBuffs(gameTime);
    }

    void OnBuffAddEvent(const SDK::Events::BuffEventArgs& args) override {
        if (inspectMode_ == 0) {
            HandlePlayerBuffEvent("add", args);
        }
    }

    void OnBuffRemoveEvent(const SDK::Events::BuffEventArgs& args) override {
        if (inspectMode_ == 0) {
            HandlePlayerBuffEvent("remove", args);
        }
    }

    void OnBuffUpdateEvent(const SDK::Events::BuffEventArgs& args) override {
        if (inspectMode_ == 0) {
            HandlePlayerBuffEvent("update", args);
        }
    }

    void OnDrawTab() override {
        ImGui::RadioButton("Live Object", &inspectMode_, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Snapshot Object", &inspectMode_, 1);

        if (inspectMode_ == 1) {
            if (plugin_->snapshots_.empty()) {
                ImGui::Text("No snapshots in memory. Hover an object and press key 'M' to take a snapshot!");
                return;
            }
            static int selectedSnapIdx = 0;
            if (selectedSnapIdx >= static_cast<int>(plugin_->snapshots_.size())) {
                selectedSnapIdx = 0;
            }
            std::vector<std::string> comboLabels;
            std::vector<const char*> comboItems;
            for (std::size_t idx = 0; idx < plugin_->snapshots_.size(); ++idx) {
                const auto& s = plugin_->snapshots_[idx];
                std::string label = s.characterName + " (NetID: " + std::to_string(s.networkId) + ")";
                if (!s.note.empty()) {
                    label += " [" + s.note + "]";
                }
                comboLabels.push_back(label);
            }
            for (const auto& l : comboLabels) {
                comboItems.push_back(l.c_str());
            }
            ImGui::Combo("Select SnapshotTarget", &selectedSnapIdx, comboItems.data(), static_cast<int>(comboItems.size()));
            const auto& selectedSnap = plugin_->snapshots_[selectedSnapIdx];
            
            ImGui::Text("Inspecting Snapshot: %s (%s) | Addr: 0x%llX",
                        selectedSnap.characterName.c_str(), selectedSnap.name.c_str(),
                        static_cast<unsigned long long>(selectedSnap.address));
            
            if (selectedSnap.buffs.empty()) {
                ImGui::Text("No buffs captured in this snapshot.");
                return;
            }

            if (!ImGui::BeginTable(
                    "SnapshotBuffTable",
                    6,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                    ImVec2(0.0f, 420.0f))) {
                return;
            }

            ImGui::TableSetupColumn("Buff");
            ImGui::TableSetupColumn("Stacks");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Duration");
            ImGui::TableSetupColumn("Live");
            ImGui::TableSetupColumn("Ptr");
            ImGui::TableHeadersRow();

            for (const auto& b : selectedSnap.buffs) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(b.name);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", b.stacks);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", b.type);
                ImGui::TableSetColumnIndex(3);
                float dur = b.endTime > b.startTime ? b.endTime - b.startTime : 0.0f;
                ImGui::Text("%.2fs", dur);
                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(b.live ? "1" : "0");
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("0x%llX", static_cast<unsigned long long>(b.address));
            }
            ImGui::EndTable();
            return;
        }

        // Live mode drawing
        const float gameTime = SDK::Game::Time();
        const int now = SDK::Game::TickCount();
        
        LiveTargetInfo targetInfo;
        {
            std::lock_guard<std::mutex> lk(targetLock_);
            targetInfo = cachedTargetInfo_;
        }

        if (!targetInfo.isValid) {
            ImGui::Text("No focused live object in range.");
            return;
        }

        ImGui::Text("Inspecting Buffs: %s (%s) | Addr: 0x%llX",
                    targetInfo.characterName.c_str(), targetInfo.name.c_str(),
                    static_cast<unsigned long long>(targetInfo.address));
        if (targetInfo.isLocalPlayer) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[Local Player]");
        }

        ImGui::Text("Game %.2f  Tick %d  EventCache=%d  Refresh=%dms  Recent=%.1fs",
                    gameTime,
                    now,
                    CoreBuffs::IsEventCacheEnabled() ? 1 : 0,
                    buffRefreshMs_,
                    buffKeepRecentSeconds_);
        ImGui::Checkbox("Log state changes", &buffLogChanges_);
        ImGui::SameLine();
        ImGui::Checkbox("Show inactive recent", &buffShowInactiveRecent_);
        ImGui::SameLine();
        if (ImGui::Button("Clear Buffs")) {
            ResetBuffEntries();
        }

        ImGui::SliderInt("Refresh ms", &buffRefreshMs_, 50, 1000);
        ImGui::SliderFloat("Recent seconds", &buffKeepRecentSeconds_, 1.0f, 15.0f);

        BuffEntry entries[256] = {};
        int count = 0;
        CopyBuffEntries(entries, count);

        if (count <= 0) {
            ImGui::TextUnformatted("No buffs tracked on target yet.");
            return;
        }

        if (!ImGui::BeginTable(
                "PlayerBuffDebugTable",
                10,
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
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Remain");
        ImGui::TableSetupColumn("Age");
        ImGui::TableSetupColumn("Ptr");
        ImGui::TableHeadersRow();

        for (int i = 0; i < count; ++i) {
            const BuffEntry& entry = entries[i];
            if (!entry.active || !entry.name[0]) {
                continue;
            }
            if (!buffShowInactiveRecent_ && !entry.live && !entry.hasBuff) {
                continue;
            }

            const int ageMs = entry.lastEventTick > 0 && now >= entry.lastEventTick
                ? now - entry.lastEventTick
                : 0;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.name);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.hasBuff ? "1" : "0");
            ImGui::TableNextColumn();
            ImGui::Text("%d", entry.sdkCount);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.live ? "1" : "0");
            ImGui::TableNextColumn();
            ImGui::Text("%d", entry.liveStacks);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.lastEvent[0] ? entry.lastEvent : "-");
            ImGui::TableNextColumn();
            ImGui::Text("%d", entry.type);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", RemainingBuffTime(entry, gameTime));
            ImGui::TableNextColumn();
            ImGui::Text("%dms", ageMs);
            ImGui::TableNextColumn();
            ImGui::Text("0x%llX", static_cast<unsigned long long>(entry.address));
        }

        ImGui::EndTable();
    }

    void OnCopyHotkey() override {
        if (inspectMode_ == 0) {
            LiveTargetInfo targetInfo;
            {
                std::lock_guard<std::mutex> lk(targetLock_);
                targetInfo = cachedTargetInfo_;
            }
            if (!targetInfo.isValid) return;

            BuffEntry entries[256] = {};
            int count = 0;
            CopyBuffEntries(entries, count);

            std::string dump = "=== DEVELOPER TOOLS BUFFS INSPECTOR (LIVE) ===\n";
            dump += "Target: " + targetInfo.characterName + " | Address: 0x" + ToHexStr(targetInfo.address) + "\n";

            const float gameTime = SDK::Game::Time();
            for (int i = 0; i < count; ++i) {
                const auto& entry = entries[i];
                if (!entry.active || !entry.name[0]) continue;
                if (!buffShowInactiveRecent_ && !entry.live && !entry.hasBuff) continue;

                char line[256];
                std::snprintf(line, sizeof(line),
                              "Buff: %s | HasBuff: %d | Stacks: %d | Live: %d | Type: %d | Remain: %.2f | Address: 0x%llX\n",
                              entry.name, entry.hasBuff ? 1 : 0, entry.liveStacks, entry.live ? 1 : 0,
                              entry.type, RemainingBuffTime(entry, gameTime),
                              static_cast<unsigned long long>(entry.address));
                dump += line;
            }

            ImGui::SetClipboardText(dump.c_str());
            NightSharpDebug::Logf("[Dev] Copied buff details of %s (Live) to Clipboard!", targetInfo.characterName.c_str());
        } else {
            if (plugin_->snapshots_.empty()) return;
            static int selectedSnapIdx = 0;
            if (selectedSnapIdx >= static_cast<int>(plugin_->snapshots_.size())) {
                selectedSnapIdx = 0;
            }
            const auto& selectedSnap = plugin_->snapshots_[selectedSnapIdx];
            
            std::string dump = "=== DEVELOPER TOOLS BUFFS INSPECTOR (SNAPSHOT) ===\n";
            dump += "Target: " + selectedSnap.characterName + " | Address: 0x" + ToHexStr(selectedSnap.address) + "\n";

            for (const auto& b : selectedSnap.buffs) {
                char line[256];
                float dur = b.endTime > b.startTime ? b.endTime - b.startTime : 0.0f;
                std::snprintf(line, sizeof(line),
                              "Buff: %s | Stacks: %d | Live: %d | Type: %d | Duration: %.2f | Address: 0x%llX\n",
                              b.name, b.stacks, b.live ? 1 : 0, b.type, dur,
                              static_cast<unsigned long long>(b.address));
                dump += line;
            }

            ImGui::SetClipboardText(dump.c_str());
            NightSharpDebug::Logf("[Dev] Copied buff details of %s (Snapshot) to Clipboard!", selectedSnap.characterName.c_str());
        }
    }

private:
    static bool SameBuffName(const char* left, const char* right) {
        if (!left || !right || !left[0] || !right[0]) {
            return false;
        }
        return _stricmp(left, right) == 0 ||
               CoreBuffs::NameMatchesQuery(left, right);
    }

    static void CopyBuffText(char* out, int outCount, const char* value) {
        if (!out || outCount <= 0) {
            return;
        }
        out[0] = 0;
        if (value) {
            strncpy_s(out, static_cast<std::size_t>(outCount), value, _TRUNCATE);
        }
    }

    static float RemainingBuffTime(const BuffEntry& entry, float gameTime) {
        if (entry.endTime <= 0.0f) {
            return 0.0f;
        }
        return entry.endTime > gameTime ? entry.endTime - gameTime : 0.0f;
    }

    int FindBuffEntryLocked(const char* name) const {
        if (!name || !name[0]) {
            return -1;
        }
        for (int i = 0; i < 256; ++i) {
            if (buffEntries_[i].active && SameBuffName(buffEntries_[i].name, name)) {
                return i;
            }
        }
        return -1;
    }

    int FindFreeBuffEntryLocked() {
        for (int i = 0; i < 256; ++i) {
            if (!buffEntries_[i].active) {
                return i;
            }
        }
        return buffCursor_++ % 256;
    }

    BuffEntry& EnsureBuffEntryLocked(const char* name) {
        int index = FindBuffEntryLocked(name);
        if (index < 0) {
            index = FindFreeBuffEntryLocked();
            buffEntries_[index] = {};
            buffEntries_[index].active = true;
            CopyBuffText(buffEntries_[index].name,
                         static_cast<int>(sizeof(buffEntries_[index].name)),
                         name);
        }
        return buffEntries_[index];
    }

    void RemoveBuffEntryLocked(const char* name) {
        const int index = FindBuffEntryLocked(name);
        if (index >= 0) {
            buffEntries_[index] = {};
        }
    }

    bool IsBuffRawSuppressedLocked(const char* name) const {
        if (!name || !name[0]) {
            return false;
        }
        for (int i = 0; i < suppressedBuffRawCount_; ++i) {
            if (SameBuffName(suppressedBuffRaw_[i], name)) {
                return true;
            }
        }
        return false;
    }

    void SuppressBuffRawLocked(const char* name) {
        if (!name || !name[0] || IsBuffRawSuppressedLocked(name)) {
            return;
        }
        const int index = suppressedBuffRawCount_ < 256
            ? suppressedBuffRawCount_++
            : 0;
        CopyBuffText(suppressedBuffRaw_[index],
                     static_cast<int>(sizeof(suppressedBuffRaw_[index])),
                     name);
    }

    void UnsuppressBuffRawLocked(const char* name) {
        if (!name || !name[0]) {
            return;
        }
        for (int i = 0; i < suppressedBuffRawCount_;) {
            if (!SameBuffName(suppressedBuffRaw_[i], name)) {
                ++i;
                continue;
            }
            for (int j = i; j + 1 < suppressedBuffRawCount_; ++j) {
                CopyBuffText(suppressedBuffRaw_[j],
                             static_cast<int>(sizeof(suppressedBuffRaw_[j])),
                             suppressedBuffRaw_[j + 1]);
            }
            suppressedBuffRaw_[--suppressedBuffRawCount_][0] = 0;
        }
    }

    void ResetBuffEntries() {
        AcquireSRWLockExclusive(&buffLock_);
        for (auto& entry : buffEntries_) {
            entry = {};
        }
        for (auto& suppressed : suppressedBuffRaw_) {
            suppressed[0] = 0;
        }
        buffCursor_ = 0;
        nextBuffRefreshTime_ = 0.0f;
        suppressedBuffRawCount_ = 0;
        ReleaseSRWLockExclusive(&buffLock_);
    }

    void CopyBuffEntries(BuffEntry* out, int& count) const {
        count = 0;
        if (!out) {
            return;
        }
        AcquireSRWLockShared(&buffLock_);
        for (const auto& entry : buffEntries_) {
            if (!entry.active || count >= 256) {
                continue;
            }
            out[count++] = entry;
        }
        ReleaseSRWLockShared(&buffLock_);
    }

    void RefreshPlayerBuffs(float gameTime) {
        const auto obj = plugin_->GetFocusedObject();
        if (!obj.IsValid()) {
            return;
        }

        const auto type = obj.Type();
        const bool hasBuffs =
            type == ::Core::Objects::ObjectType::AIHeroClient ||
            type == ::Core::Objects::ObjectType::AIMinionClient;
        if (!hasBuffs) {
            return;
        }

        const uintptr_t playerAddress = obj.Address();
        if (!Globals::IsReadablePtr(playerAddress, sizeof(std::uintptr_t))) {
            return;
        }

        uintptr_t buffs[256] = {};
        const int buffCount = CoreBuffs::Enumerate(playerAddress, buffs, 256);

        // Finish every game-memory read before taking buffLock_.  If a stale
        // buff pointer ever faults, the outer tab guard can recover without
        // leaving the SRW lock permanently held.
        BuffObservation observations[256] = {};
        int observationCount = 0;
        char name[96] = {};
        for (int i = 0; i < buffCount && observationCount < 256; ++i) {
            const CoreBuffs::BuffRef buff{ buffs[i] };
            if (!buff.ReadName(name, static_cast<int>(sizeof(name)))) {
                continue;
            }

            auto& observation = observations[observationCount++];
            CopyBuffText(observation.name,
                         static_cast<int>(sizeof(observation.name)),
                         name);
            observation.address = buff.address;
            observation.live = buff.IsActive(gameTime);
            observation.stacks = buff.GetStacks();
            observation.type = buff.GetType();
            observation.startTime = buff.GetStartTime();
            observation.endTime = buff.GetEndTime();
        }

        BuffEntry changedEntries[256] = {};
        int changedCount = 0;

        AcquireSRWLockExclusive(&buffLock_);

        for (auto& entry : buffEntries_) {
            if (entry.active) {
                entry.previousHasBuff = entry.hasBuff;
                entry.previousLive = entry.live;
                entry.previousSdkCount = entry.sdkCount;
                entry.liveSeenThisRefresh = false;
                entry.live = false;
                entry.liveStacks = 0;
                entry.type = -1;
                entry.address = 0;
            }
        }

        for (int i = 0; i < observationCount; ++i) {
            const auto& observation = observations[i];
            if (IsBuffRawSuppressedLocked(observation.name)) {
                continue;
            }

            BuffEntry& entry = EnsureBuffEntryLocked(observation.name);
            entry.address = observation.address;
            entry.live = observation.live;
            entry.liveSeenThisRefresh = true;
            entry.liveStacks = observation.stacks;
            entry.type = observation.type;
            entry.startTime = observation.startTime;
            entry.endTime = observation.endTime;
            if (entry.live) {
                entry.lastSeenTime = gameTime;
            }
        }

        for (auto& entry : buffEntries_) {
            if (!entry.active || !entry.name[0]) {
                continue;
            }

            // The raw enumeration above is already the authoritative snapshot
            // for this refresh.  Re-querying every name rebuilt the buff cache
            // repeatedly and was the last operation visible before the crash.
            entry.hasBuff = entry.live;
            entry.sdkCount = entry.liveStacks;

            const bool liveRecently =
                gameTime - entry.lastSeenTime <= buffKeepRecentSeconds_;
            const bool eventRecently =
                gameTime - entry.lastChangeTime <= buffKeepRecentSeconds_;
            if (!entry.live && !entry.hasBuff && !liveRecently && !eventRecently) {
                entry = {};
                continue;
            }

            const bool changed =
                !entry.initialized ||
                entry.previousHasBuff != entry.hasBuff ||
                entry.previousLive != entry.live ||
                entry.previousSdkCount != entry.sdkCount;
            if (changed) {
                entry.initialized = true;
                entry.lastChangeTime = gameTime;
                if (buffLogChanges_ && changedCount < 256) {
                    changedEntries[changedCount++] = entry;
                }
            }
        }

        ReleaseSRWLockExclusive(&buffLock_);

        for (int i = 0; i < changedCount; ++i) {
            LogBuffEntry("refresh", changedEntries[i]);
        }
    }

    void HandlePlayerBuffEvent(const char* eventName, const SDK::Events::BuffEventArgs& args) {
        const auto target = plugin_->GetFocusedObject();
        if (!target.IsValid() || args.Sender.NetworkId != target.NetworkId() || !args.BuffName[0]) {
            return;
        }

        const float gameTime = SDK::Game::Time();
        const int gameTick = SDK::Game::TickCount();
        AcquireSRWLockExclusive(&buffLock_);

        const bool removeEvent = eventName && _stricmp(eventName, "remove") == 0;
        const bool updateEvent = eventName && _stricmp(eventName, "update") == 0;
        if (removeEvent || (updateEvent && args.Count <= 0)) {
            SuppressBuffRawLocked(args.BuffName);
            RemoveBuffEntryLocked(args.BuffName);
            ReleaseSRWLockExclusive(&buffLock_);
            if (buffLogChanges_) {
                NightSharpDebug::Logf(
                    "[PlayerBuffDebug] remove-display name=%s event=%s count=%d",
                    args.BuffName,
                    eventName ? eventName : "?",
                    args.Count);
            }
            return;
        }

        UnsuppressBuffRawLocked(args.BuffName);

        BuffEntry& entry = EnsureBuffEntryLocked(args.BuffName);
        entry.lastEventTick = gameTick;
        entry.lastChangeTime = gameTime;
        CopyBuffText(entry.lastEvent,
                     static_cast<int>(sizeof(entry.lastEvent)),
                     eventName);
        if (args.Count > 0) {
            entry.lastSeenTime = gameTime;
        }
        const BuffEntry entryForLog = entry;
        ReleaseSRWLockExclusive(&buffLock_);
        if (buffLogChanges_) {
            LogBuffEntry(eventName, entryForLog);
        }
    }

    static void LogBuffEntry(const char* source, const BuffEntry& entry) {
        NightSharpDebug::Logf(
            "[PlayerBuffDebug] %s name=%s has=%d sdkCount=%d live=%d "
            "liveStacks=%d type=%d buff=0x%llX event=%s",
            source ? source : "?",
            entry.name,
            entry.hasBuff ? 1 : 0,
            entry.sdkCount,
            entry.live ? 1 : 0,
            entry.liveStacks,
            entry.type,
            static_cast<unsigned long long>(entry.address),
            entry.lastEvent[0] ? entry.lastEvent : "-");
    }

    static std::string ToHexStr(uintptr_t val) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%llX", static_cast<unsigned long long>(val));
        return buf;
    }
};

} // namespace Plugins::DevTools
