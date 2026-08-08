#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreBuffs.h"
#include "../../Core/CoreObjectManager.h"
#include "../../Core/CoreObjects.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../imgui/imgui.h"

#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Plugins {

class BuffInspectorPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Buff Inspector"; }
    const char* GetInternalId() const override { return "core.buff_inspector"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }

    bool CanLoad() const override {
        return CoreRuntime::EnsureInitialized() &&
               CoreRuntime::RefreshReadState();
    }

    void OnLoad() override {
        Reset();
        NightSharpDebug::Logf("[BuffInspector] loaded");
    }

    void OnUnload() override {
        Reset();
        NightSharpDebug::Logf("[BuffInspector] unloaded");
    }

    void OnUpdate() override {
        RefreshIfDue();
    }

    void OnMenu() override {
        if (!ImGui::GetCurrentContext()) {
            return;
        }

        RefreshIfDue(true);

        // Row[2048] ~ 670 KB: KHONG duoc dat tren stack cua render thread
        // (mac dinh 1 MB) vi se gay stack overflow -> OnMenu bi __except bat,
        // plugin bi disable am tham va panel hien ra trong.
        if (m_viewRows.size() != static_cast<std::size_t>(kMaxRows)) {
            m_viewRows.resize(static_cast<std::size_t>(kMaxRows));
        }

        Row* rows = m_viewRows.data();
        int count = 0;
        int scannedOwners = 0;
        int scannedBuffs = 0;
        CopyRows(rows, count, scannedOwners, scannedBuffs);

        ImGui::Text("Live snapshot only - no event cache, no recent retention.");
        ImGui::Text("Game %.2f  Rows %d  Owners %d  Buffs %d  Refresh=%dms  EventCache=%d",
                    SDK::Game::Time(),
                    count,
                    scannedOwners,
                    scannedBuffs,
                    m_refreshMs,
                    CoreBuffs::IsEventCacheEnabled() ? 1 : 0);

        ImGui::Checkbox("Player", &m_scanPlayer);
        ImGui::SameLine();
        ImGui::Checkbox("Heroes", &m_scanHeroes);
        ImGui::SameLine();
        ImGui::Checkbox("Minions", &m_scanMinions);
        ImGui::SameLine();
        ImGui::Checkbox("Turrets", &m_scanTurrets);
        ImGui::SameLine();
        ImGui::Checkbox("All Objects", &m_scanAllObjects);

        ImGui::Checkbox("Show inactive/raw stale entries", &m_showInactiveRaw);
        ImGui::SameLine();
        ImGui::Checkbox("Only problems", &m_onlyProblems);
        ImGui::SameLine();
        if (ImGui::Button("Refresh now")) {
            Refresh(SDK::Game::Time(), true);
            CopyRows(rows, count, scannedOwners, scannedBuffs);
        }

        ImGui::SliderInt("Refresh ms", &m_refreshMs, 100, 2000);
        ImGui::SliderInt("Row limit", &m_rowLimit, 128, kMaxRows);
        ImGui::InputText("Filter", m_filter, sizeof(m_filter));

        DrawSummary(rows, count);
        DrawTable(rows, count);
    }

private:
    static constexpr int kMaxOwners = 4096;
    static constexpr int kMaxBuffsPerOwner = 256;
    static constexpr int kMaxRows = 2048;

    struct Row {
        bool active = false;
        bool rawActive = false;
        bool sdkHasBuff = false;
        bool recallSuppressed = false;
        bool problem = false;
        int sdkCount = 0;
        int rawStacks = 0;
        int rawStacksAlt = 0;
        int counterCurrent = 0;
        int counterMax = 0;
        int buffType = -1;
        int ownerType = 0;
        std::uint32_t ownerIndex = 0;
        std::uint32_t ownerNetId = 0;
        std::uint32_t ownerTeam = 0;
        float startTime = 0.0f;
        float endTime = 0.0f;
        float remaining = 0.0f;
        uintptr_t owner = 0;
        uintptr_t buff = 0;
        uintptr_t stackArray = 0;
        char ownerName[64] = {};
        char ownerChar[64] = {};
        char buffName[96] = {};
        char status[24] = {};
    };

    mutable SRWLOCK m_lock = SRWLOCK_INIT;
    std::vector<Row> m_rows{ static_cast<std::size_t>(kMaxRows) };
    std::vector<Row> m_viewRows;
    std::vector<Row> m_scratchRows;
    std::vector<uintptr_t> m_scratchOwners;
    int m_rowCount = 0;
    int m_scannedOwners = 0;
    int m_scannedBuffs = 0;
    float m_nextRefreshTime = 0.0f;
    int m_refreshMs = 300;
    int m_rowLimit = 1024;
    bool m_scanPlayer = true;
    bool m_scanHeroes = true;
    bool m_scanMinions = true;
    bool m_scanTurrets = false;
    bool m_scanAllObjects = false;
    bool m_showInactiveRaw = false;
    bool m_onlyProblems = false;
    char m_filter[96] = {};

    static void CopyText(char* out, int outCount, const char* value) {
        if (!out || outCount <= 0) {
            return;
        }
        out[0] = 0;
        if (value) {
            strncpy_s(out, static_cast<std::size_t>(outCount), value, _TRUNCATE);
        }
    }

    static bool ContainsInsensitive(const char* text, const char* needle) {
        if (!needle || !needle[0]) {
            return true;
        }
        if (!text || !text[0]) {
            return false;
        }

        for (const char* p = text; *p; ++p) {
            const char* a = p;
            const char* b = needle;
            while (*a && *b) {
                char ca = *a;
                char cb = *b;
                if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
                if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
                if (ca != cb) {
                    break;
                }
                ++a;
                ++b;
            }
            if (!*b) {
                return true;
            }
        }
        return false;
    }

    static const char* ObjectTypeName(::Core::Objects::ObjectType type) {
        using Type = ::Core::Objects::ObjectType;
        switch (type) {
        case Type::AIHeroClient: return "Hero";
        case Type::AIMinionClient: return "Minion";
        // case Type::AITurretClient: return "Turret";
        case Type::MissileClient: return "Missile";
        case Type::EffectEmitter: return "Effect";
        // case Type::ShopClient: return "Shop";
        case Type::BarracksDampenerClient: return "Inhib";
        case Type::HQClient: return "HQ";
        case Type::GameObject: return "Object";
        default: return "Unknown";
        }
    }

    static float RemainingTime(float endTime, float gameTime) {
        if (endTime <= 0.0f) {
            return 0.0f;
        }
        return endTime > gameTime ? endTime - gameTime : 0.0f;
    }

    static bool IsDuplicateOwner(const uintptr_t* owners, int count, uintptr_t owner) {
        if (!Globals::IsValidPtr(owner)) {
            return true;
        }
        for (int i = 0; i < count; ++i) {
            if (owners[i] == owner) {
                return true;
            }
        }
        return false;
    }

    static void AddOwner(uintptr_t* owners, int& count, uintptr_t owner) {
        if (count >= kMaxOwners || IsDuplicateOwner(owners, count, owner)) {
            return;
        }
        owners[count++] = owner;
    }

    static void AddManagerOwners(
        uintptr_t* owners,
        int& count,
        ::Core::ObjectManager::ManagerKind kind) {
        uintptr_t buffer[1024] = {};
        int read = 0;
        switch (kind) {
        case ::Core::ObjectManager::ManagerKind::Objects:
            read = ::Core::ObjectManager::EnumerateAll(buffer, 1024);
            break;
        case ::Core::ObjectManager::ManagerKind::Heroes:
            read = ::Core::ObjectManager::EnumerateHeroes(buffer, 1024);
            break;
        case ::Core::ObjectManager::ManagerKind::Minions:
            read = ::Core::ObjectManager::EnumerateMinions(buffer, 1024);
            break;
        case ::Core::ObjectManager::ManagerKind::Turrets:
            read = ::Core::ObjectManager::EnumerateTurrets(buffer, 1024);
            break;
        default:
            break;
        }

        for (int i = 0; i < read; ++i) {
            AddOwner(owners, count, buffer[i]);
        }
    }

    bool MatchesFilter(const Row& row) const {
        if (!m_filter[0]) {
            return true;
        }
        return ContainsInsensitive(row.buffName, m_filter) ||
               ContainsInsensitive(row.ownerName, m_filter) ||
               ContainsInsensitive(row.ownerChar, m_filter) ||
               ContainsInsensitive(row.status, m_filter);
    }

    static bool ShouldTreatAsLiveBuff(
        uintptr_t owner,
        const char* buffName,
        const CoreBuffs::BuffRef& buff,
        float gameTime,
        bool& recallSuppressed) {
        recallSuppressed = false;
        if (!buff.IsActive(gameTime)) {
            return false;
        }

        if (CoreBuffs::IsSuppressedByLiveState(owner, buffName)) {
            recallSuppressed = true;
            return false;
        }

        return true;
    }

    static void BuildOwnerNames(uintptr_t owner, Row& row) {
        char name[64] = {};
        char characterName[64] = {};
        ::Core::Objects::ReadName(owner, name, static_cast<int>(sizeof(name)));
        ::Core::Objects::ReadCharacterName(
            owner,
            characterName,
            static_cast<int>(sizeof(characterName)));

        CopyText(row.ownerName, static_cast<int>(sizeof(row.ownerName)), name);
        CopyText(row.ownerChar, static_cast<int>(sizeof(row.ownerChar)), characterName);
    }

    static void BuildStatus(Row& row) {
        if (row.recallSuppressed) {
            CopyText(row.status, static_cast<int>(sizeof(row.status)), "STALE_RECALL");
            row.problem = true;
            return;
        }

        if (!row.active) {
            CopyText(row.status, static_cast<int>(sizeof(row.status)), "RAW_INACTIVE");
            row.problem = true;
            return;
        }

        if (!row.sdkHasBuff) {
            CopyText(row.status, static_cast<int>(sizeof(row.status)), "SDK_HAS_FALSE");
            row.problem = true;
            return;
        }

        if (row.sdkCount != row.rawStacks) {
            CopyText(row.status, static_cast<int>(sizeof(row.status)), "COUNT_DIFF");
            row.problem = true;
            return;
        }

        CopyText(row.status, static_cast<int>(sizeof(row.status)), "OK");
        row.problem = false;
    }

    Row BuildRow(uintptr_t owner, const CoreBuffs::BuffRef& buff, float gameTime) const {
        Row row{};
        row.owner = owner;
        row.buff = buff.address;
        row.stackArray = Globals::Read<uintptr_t>(
            buff.address + Offset::BuffDataLayout::BuffStackArrayBegin);
        row.ownerIndex = ::Core::Objects::ReadIndex(owner);
        row.ownerNetId = ::Core::Objects::ReadNetworkId(owner);
        row.ownerTeam = ::Core::Objects::ReadTeamValue(owner);
        row.ownerType = static_cast<int>(::Core::ObjectManager::InferType(owner));
        row.buffType = buff.GetType();
        row.rawStacks = buff.GetLiveStackCount();
        row.rawStacksAlt = Globals::Read<int>(
            buff.address + Offset::BuffDataLayout::BuffStacksAlt);
        row.counterCurrent = buff.GetCounterCurrent();
        row.counterMax = buff.GetCounterMax();
        row.startTime = buff.GetStartTime();
        row.endTime = buff.GetEndTime();
        row.remaining = RemainingTime(row.endTime, gameTime);
        row.rawActive = buff.IsActive(gameTime);
        buff.ReadName(row.buffName, static_cast<int>(sizeof(row.buffName)));
        BuildOwnerNames(owner, row);
        row.active = ShouldTreatAsLiveBuff(
            owner,
            row.buffName,
            buff,
            gameTime,
            row.recallSuppressed);
        row.sdkHasBuff = CoreBuffs::HasActiveBuff(owner, row.buffName, gameTime);
        row.sdkCount = CoreBuffs::GetActiveBuffStacks(owner, row.buffName, gameTime);
        BuildStatus(row);
        return row;
    }

    void Reset() {
        AcquireSRWLockExclusive(&m_lock);
        for (auto& row : m_rows) {
            row = {};
        }
        m_rowCount = 0;
        m_scannedOwners = 0;
        m_scannedBuffs = 0;
        m_nextRefreshTime = 0.0f;
        ReleaseSRWLockExclusive(&m_lock);
    }

    void CopyRows(Row* out, int& count, int& scannedOwners, int& scannedBuffs) const {
        count = 0;
        scannedOwners = 0;
        scannedBuffs = 0;
        if (!out) {
            return;
        }

        AcquireSRWLockShared(&m_lock);
        count = std::clamp(m_rowCount, 0, kMaxRows);
        for (int i = 0; i < count; ++i) {
            out[i] = m_rows[i];
        }
        scannedOwners = m_scannedOwners;
        scannedBuffs = m_scannedBuffs;
        ReleaseSRWLockShared(&m_lock);
    }

    void RefreshIfDue(bool forceWhenEmpty = false) {
        const float gameTime = SDK::Game::Time();
        if (gameTime <= 0.0f) {
            return;
        }
        if (!forceWhenEmpty && gameTime < m_nextRefreshTime) {
            return;
        }

        Refresh(gameTime, forceWhenEmpty && m_rowCount == 0);
    }

    void Refresh(float gameTime, bool force = false) {
        if (!force && gameTime < m_nextRefreshTime) {
            return;
        }

        m_nextRefreshTime =
            gameTime + static_cast<float>(std::clamp(m_refreshMs, 100, 2000)) / 1000.0f;

        if (!CoreRuntime::RefreshReadState()) {
            Reset();
            return;
        }

        // Toan bo scratch buffer nam tren heap. Truoc day owners[4096] (32 KB)
        // + nextRows[2048] (~670 KB) nam tren stack, cong voi Row[2048] trong
        // OnMenu -> vuot 1 MB stack cua render thread -> stack overflow.
        if (m_scratchOwners.size() != static_cast<std::size_t>(kMaxOwners)) {
            m_scratchOwners.resize(static_cast<std::size_t>(kMaxOwners));
        }
        if (m_scratchRows.size() != static_cast<std::size_t>(kMaxRows)) {
            m_scratchRows.resize(static_cast<std::size_t>(kMaxRows));
        }

        uintptr_t* owners = m_scratchOwners.data();
        std::memset(owners, 0, m_scratchOwners.size() * sizeof(uintptr_t));
        int ownerCount = 0;
        if (m_scanPlayer) {
            AddOwner(owners, ownerCount, ::Core::ObjectManager::PlayerAddress());
        }
        if (m_scanHeroes) {
            AddManagerOwners(owners, ownerCount, ::Core::ObjectManager::ManagerKind::Heroes);
        }
        if (m_scanMinions) {
            AddManagerOwners(owners, ownerCount, ::Core::ObjectManager::ManagerKind::Minions);
        }
        if (m_scanTurrets) {
            AddManagerOwners(owners, ownerCount, ::Core::ObjectManager::ManagerKind::Turrets);
        }
        if (m_scanAllObjects) {
            AddManagerOwners(owners, ownerCount, ::Core::ObjectManager::ManagerKind::Objects);
        }

        Row* nextRows = m_scratchRows.data();
        int nextCount = 0;
        int scannedBuffs = 0;
        const int rowLimit = std::clamp(m_rowLimit, 128, kMaxRows);

        for (int ownerIndex = 0; ownerIndex < ownerCount; ++ownerIndex) {
            uintptr_t buffs[kMaxBuffsPerOwner] = {};
            const int buffCount =
                CoreBuffs::Enumerate(owners[ownerIndex], buffs, kMaxBuffsPerOwner);
            scannedBuffs += std::max(0, buffCount);

            for (int buffIndex = 0; buffIndex < buffCount && nextCount < rowLimit; ++buffIndex) {
                CoreBuffs::BuffRef buff{ buffs[buffIndex] };
                if (!buff.IsValid()) {
                    continue;
                }

                Row row = BuildRow(owners[ownerIndex], buff, gameTime);
                if (!row.buffName[0]) {
                    continue;
                }
                if (!row.active && !m_showInactiveRaw) {
                    continue;
                }
                if (m_onlyProblems && !row.problem) {
                    continue;
                }
                if (!MatchesFilter(row)) {
                    continue;
                }

                nextRows[nextCount++] = row;
            }
        }

        std::sort(nextRows, nextRows + nextCount, [](const Row& left, const Row& right) {
            if (left.ownerType != right.ownerType) {
                return left.ownerType < right.ownerType;
            }
            const int ownerCmp = _stricmp(left.ownerChar, right.ownerChar);
            if (ownerCmp != 0) {
                return ownerCmp < 0;
            }
            return _stricmp(left.buffName, right.buffName) < 0;
        });

        AcquireSRWLockExclusive(&m_lock);
        for (int i = 0; i < kMaxRows; ++i) {
            m_rows[i] = i < nextCount ? nextRows[i] : Row{};
        }
        m_rowCount = nextCount;
        m_scannedOwners = ownerCount;
        m_scannedBuffs = scannedBuffs;
        ReleaseSRWLockExclusive(&m_lock);
    }

    static void DrawSummary(const Row* rows, int count) {
        int problems = 0;
        int staleRecall = 0;
        int countDiff = 0;
        int sdkFalse = 0;
        for (int i = 0; i < count; ++i) {
            if (!rows[i].problem) {
                continue;
            }
            ++problems;
            if (_stricmp(rows[i].status, "STALE_RECALL") == 0) {
                ++staleRecall;
            } else if (_stricmp(rows[i].status, "COUNT_DIFF") == 0) {
                ++countDiff;
            } else if (_stricmp(rows[i].status, "SDK_HAS_FALSE") == 0) {
                ++sdkFalse;
            }
        }

        ImGui::Text("Problems: %d  StaleRecall=%d  CountDiff=%d  SdkHasFalse=%d",
                    problems,
                    staleRecall,
                    countDiff,
                    sdkFalse);
    }

    void DrawTable(const Row* rows, int count) const {
        if (!rows || count <= 0) {
            ImGui::TextUnformatted("No live buffs in current scope.");
            return;
        }

        if (!ImGui::BeginTable(
                "BuffInspectorTable",
                17,
                ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_SizingFixedFit,
                ImVec2(0.0f, 520.0f))) {
            return;
        }

        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Owner");
        ImGui::TableSetupColumn("ObjType");
        ImGui::TableSetupColumn("Net");
        ImGui::TableSetupColumn("Idx");
        ImGui::TableSetupColumn("Team");
        ImGui::TableSetupColumn("Buff");
        ImGui::TableSetupColumn("Active");
        ImGui::TableSetupColumn("Has");
        ImGui::TableSetupColumn("SDK");
        ImGui::TableSetupColumn("+38");
        ImGui::TableSetupColumn("+3C");
        ImGui::TableSetupColumn("Ctr");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Remain");
        ImGui::TableSetupColumn("BuffPtr");
        ImGui::TableSetupColumn("Array");
        ImGui::TableHeadersRow();

        for (int i = 0; i < count; ++i) {
            const Row& row = rows[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.status);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row.ownerChar[0] ? row.ownerChar : row.ownerName);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(ObjectTypeName(
                static_cast<::Core::Objects::ObjectType>(row.ownerType)));
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("0x%X", row.ownerNetId);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("0x%X", row.ownerIndex);
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%u", row.ownerTeam);
            ImGui::TableSetColumnIndex(6);
            ImGui::TextUnformatted(row.buffName);
            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%d", row.active ? 1 : 0);
            ImGui::TableSetColumnIndex(8);
            ImGui::Text("%d", row.sdkHasBuff ? 1 : 0);
            ImGui::TableSetColumnIndex(9);
            ImGui::Text("%d", row.sdkCount);
            ImGui::TableSetColumnIndex(10);
            ImGui::Text("%d", row.rawStacks);
            ImGui::TableSetColumnIndex(11);
            ImGui::Text("%d", row.rawStacksAlt);
            ImGui::TableSetColumnIndex(12);
            ImGui::Text("%d/%d", row.counterCurrent, row.counterMax);
            ImGui::TableSetColumnIndex(13);
            ImGui::Text("%d", row.buffType);
            ImGui::TableSetColumnIndex(14);
            ImGui::Text("%.2f", row.remaining);
            ImGui::TableSetColumnIndex(15);
            ImGui::Text("0x%llX", static_cast<unsigned long long>(row.buff));
            ImGui::TableSetColumnIndex(16);
            ImGui::Text("0x%llX", static_cast<unsigned long long>(row.stackArray));
        }

        ImGui::EndTable();
    }
};

} // namespace Plugins
