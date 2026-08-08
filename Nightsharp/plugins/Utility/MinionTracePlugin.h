#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreObjects.h"
#include "../../Core/CoreObjectManager.h"
#include "../../Core/CoreAttackableUnit.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../SDK/Extensions/Unit.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace Plugins {

class MinionTracePlugin final : public IPlugin {
public:
    struct ObjectRecord {
        uintptr_t address = 0;
        uint32_t networkId = 0;
        ::Core::Objects::ObjectType type = ::Core::Objects::ObjectType::Unknown;
        bool dropped = false;
        char name[64] = {};
        char charName[64] = {};
        int team = 0;
        float health = 0.0f;
        float maxHealth = 0.0f;
        bool isVisible = false;
        bool isZombie = false;
        bool isTargetable = false;
        bool isInvulnerable = false;
        bool isDead = false;
        bool predMinion = false;
        bool inMinMgr = false;
    };

    MinionTracePlugin() {
        s_instance_ = this;
        SDK::GameObjects::detail::OnObjectProcessedCallback = &OnObjectProcessedStatic;
        SDK::GameObjects::detail::OnObjectRemovedCallback = &OnObjectRemovedStatic;
    }

    const char* GetName() const override { return "Minion Tracing & Inspector"; }
    const char* GetInternalId() const override { return "utility.minion_trace_inspector"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void BootstrapInitialObjects() {
        if (!records_.empty() || !SDK::GameObjects::detail::Loaded) return;
        const auto objects = SDK::ObjectManager::Get<GameObject>();
        for (const auto& obj : objects) {
            const uintptr_t addr = obj.Address();
            if (!addr) continue;
            const auto type = SDK::ObjectManager::detail::InferExtendedType(addr);
            const bool dropped = (type != ::Core::Objects::ObjectType::AIHeroClient &&
                                  type != ::Core::Objects::ObjectType::AIMinionClient &&
                                  // REMOVED: Turret/Inhibitor/Nexus disabled by user request
                                  // type != ::Core::Objects::ObjectType::AITurretClient &&
                                  // type != ::Core::Objects::ObjectType::ShopClient &&
                                  type != ::Core::Objects::ObjectType::Obj_SpawnPoint
                                  // REMOVED: Turret/Inhibitor/Nexus disabled by user request
                                  // && type != ::Core::Objects::ObjectType::BarracksDampenerClient
                                  // && type != ::Core::Objects::ObjectType::HQClient
                                  );
            RecordObject(addr, type, dropped);
        }
    }

    void OnLoad() override {
        s_instance_ = this;
        SDK::GameObjects::detail::OnObjectProcessedCallback = &OnObjectProcessedStatic;
        SDK::GameObjects::detail::OnObjectRemovedCallback = &OnObjectRemovedStatic;
        BootstrapInitialObjects();
        NightSharpDebug::Logf("[MinionTracePlugin] loaded and connected to AddObject funnel");
    }

    void OnUnload() override {
        // Keep listening even if unloaded/toggled off, as requested by user ("as long as plugin exists")
        NightSharpDebug::Logf("[MinionTracePlugin] unloaded (listening continues)");
    }

    void OnUpdate() override {
        if (records_.empty()) {
            BootstrapInitialObjects();
        }
    }

    void OnRender() override {
        if (!showWindow_) {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(960.0f, 480.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("NightSharp Minion Trace Inspector Table", &showWindow_)) {
            ImGui::End();
            return;
        }

        ImGui::Checkbox("Show Dropped Only", &filterDroppedOnly_);
        ImGui::SameLine();
        ImGui::Checkbox("Show Minions Only", &filterMinionsOnly_);
        ImGui::SameLine();
        if (ImGui::Button("Clear Table")) {
            std::lock_guard<std::mutex> lock(recordsMutex_);
            records_.clear();
        }

        std::vector<ObjectRecord> snapshot;
        {
            std::lock_guard<std::mutex> lock(recordsMutex_);
            snapshot = records_;
        }

        ImGui::Separator();
        ImGui::Text("Total Captured Objects: %zu", snapshot.size());

        if (ImGui::BeginTable(
                "MinionTraceTable",
                10,
                ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Status");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("CharName");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Team");
            ImGui::TableSetupColumn("HP");
            ImGui::TableSetupColumn("Flags (Vis/Zomb/Inv/Targ/Dead)");
            ImGui::TableSetupColumn("Pred / Mgr");
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("NetID");
            ImGui::TableHeadersRow();

            for (const auto& rec : snapshot) {
                if (filterDroppedOnly_ && !rec.dropped) {
                    continue;
                }
                if (filterMinionsOnly_ && rec.type != ::Core::Objects::ObjectType::AIMinionClient) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (rec.dropped) {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[DROPPED]");
                } else if (rec.type == ::Core::Objects::ObjectType::AIMinionClient) {
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Minion");
                } else {
                    ImGui::Text("Other");
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(rec.name[0] ? rec.name : "--");

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(rec.charName[0] ? rec.charName : "--");

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%d", static_cast<int>(rec.type));

                ImGui::TableSetColumnIndex(4);
                if (rec.team == 100) ImGui::Text("Order");
                else if (rec.team == 200) ImGui::Text("Chaos");
                else if (rec.team == 300) ImGui::Text("Neutral");
                else ImGui::Text("%d", rec.team);

                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%.0f / %.0f", rec.health, rec.maxHealth);

                ImGui::TableSetColumnIndex(6);
                char flags[32];
                std::snprintf(flags, sizeof(flags), "V:%d Z:%d I:%d T:%d D:%d",
                              rec.isVisible ? 1 : 0,
                              rec.isZombie ? 1 : 0,
                              rec.isInvulnerable ? 1 : 0,
                              rec.isTargetable ? 1 : 0,
                              rec.isDead ? 1 : 0);
                ImGui::TextUnformatted(flags);

                ImGui::TableSetColumnIndex(7);
                ImGui::Text("Pred:%s Mgr:%s", rec.predMinion ? "Y" : "N", rec.inMinMgr ? "Y" : "N");

                ImGui::TableSetColumnIndex(8);
                ImGui::Text("0x%llX", static_cast<unsigned long long>(rec.address));

                ImGui::TableSetColumnIndex(9);
                ImGui::Text("0x%X", rec.networkId);
            }

            ImGui::EndTable();
        }

        ImGui::End();
    }

    void OnMenu() override {
        ImGui::Checkbox("Open Minion Inspector Window", &showWindow_);
        ImGui::SameLine();
        if (ImGui::Button("Dump Table To File")) {
            DumpToFile();
        }
    }

private:
    static void OnObjectProcessedStatic(uintptr_t address, ::Core::Objects::ObjectType type, bool dropped) {
        if (s_instance_) {
            s_instance_->RecordObject(address, type, dropped);
        }
    }

    static void OnObjectRemovedStatic(uintptr_t address) {
        if (s_instance_) {
            s_instance_->RemoveRecord(address);
        }
    }

    void PopulateRecordSafe(ObjectRecord& rec, uintptr_t address, SDK::AttackableUnit& unit) {
        __try {
            char nameBuf[64] = {};
            ::Core::Objects::ReadName(address, nameBuf, sizeof(nameBuf));
            strncpy_s(rec.name, nameBuf, _TRUNCATE);

            char charBuf[64] = {};
            ::Core::Objects::ReadCharacterName(address, charBuf, sizeof(charBuf));
            strncpy_s(rec.charName, charBuf, _TRUNCATE);

            rec.team = static_cast<int>(unit.Team());
            rec.health = unit.Health();
            rec.maxHealth = unit.MaxHealth();
            rec.isVisible = unit.IsVisible();
            rec.isZombie = unit.IsZombie();
            rec.isTargetable = unit.IsTargetable();
            rec.isInvulnerable = unit.IsInvulnerable();
            rec.isDead = unit.IsDead();
            rec.predMinion = ::Core::Objects::IsMinion(address);
            rec.inMinMgr = ::Core::ObjectManager::ManagerContains(::Core::ObjectManager::ManagerKind::Minions, address);
        }
        __except (1) {
            // Object memory not yet fully initialized; store with defaults populated so far.
        }
    }

    void RecordObject(uintptr_t address, ::Core::Objects::ObjectType type, bool dropped) {
        if (!Globals::IsValidPtr(address)) {
            return;
        }

        ObjectRecord rec{};
        rec.address = address;
        rec.networkId = ::Core::Objects::ReadNetworkId(address);
        rec.type = type;
        rec.dropped = dropped;

        SDK::AttackableUnit unit(address);
        PopulateRecordSafe(rec, address, unit);

        std::lock_guard<std::mutex> lock(recordsMutex_);
        auto it = std::find_if(records_.begin(), records_.end(), [&](const ObjectRecord& r) {
            return r.address == address;
        });
        if (it != records_.end()) {
            *it = rec;
        } else {
            if (records_.size() < 2048) {
                records_.push_back(rec);
            }
        }
    }

    void RemoveRecord(uintptr_t address) {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        records_.erase(std::remove_if(records_.begin(), records_.end(), [&](const ObjectRecord& r) {
            return r.address == address;
        }), records_.end());
    }

    void DumpToFile() {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        std::ofstream out("c:\\Users\\Public\\nightsharp_minions_trace.txt", std::ios::trunc);
        if (!out.is_open()) return;

        out << "Status\tName\tCharName\tType\tTeam\tHP\tMaxHP\tVis\tZomb\tInvul\tTarg\tDead\tPred\tMgr\tAddress\tNetID\n";
        for (const auto& rec : records_) {
            out << (rec.dropped ? "DROPPED" : "CLASSIFIED") << "\t"
                << rec.name << "\t"
                << rec.charName << "\t"
                << static_cast<int>(rec.type) << "\t"
                << rec.team << "\t"
                << rec.health << "\t"
                << rec.maxHealth << "\t"
                << rec.isVisible << "\t"
                << rec.isZombie << "\t"
                << rec.isInvulnerable << "\t"
                << rec.isTargetable << "\t"
                << rec.isDead << "\t"
                << rec.predMinion << "\t"
                << rec.inMinMgr << "\t"
                << "0x" << std::hex << rec.address << "\t"
                << "0x" << rec.networkId << std::dec << "\n";
        }
        out.close();
    }

    static inline MinionTracePlugin* s_instance_ = nullptr;
    std::vector<ObjectRecord> records_;
    std::mutex recordsMutex_;
    bool showWindow_ = false;
    bool filterDroppedOnly_ = false;
    bool filterMinionsOnly_ = false;
};

} // namespace Plugins
