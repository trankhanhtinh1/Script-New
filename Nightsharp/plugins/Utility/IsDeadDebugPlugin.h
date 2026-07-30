#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreCastSpell.h"
#include "../../Core/CoreObjects.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"

#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace Plugins {

class IsDeadDebugPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "IsDead Debug"; }
    const char* GetInternalId() const override { return "utility.is_dead_debug"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        rows_.clear();
        lastRefreshTick_ = 0;
        NightSharpDebug::Logf("[IsDeadDebug] loaded");
    }

    void OnUnload() override {
        if (logFile_.is_open()) {
            logFile_.close();
        }
        rows_.clear();
        NightSharpDebug::Logf("[IsDeadDebug] unloaded");
    }

    void OnUpdate() override {
        const int now = SDK::Game::TickCount();
        if (autoRefresh_ && now - lastRefreshTick_ >= refreshMs_) {
            RefreshSnapshot();
        }

        if (logToFile_ && now - lastLogTick_ >= logThrottleMs_) {
            WriteLog(now);
        }
    }

    void OnRender() override {
        if (!drawOnChampion_ || !ImGui::GetCurrentContext() || !SDK::Drawing::IsEnabled()) {
            return;
        }

        for (const auto& row : rows_) {
            if (!Globals::IsValidPtr(row.address)) {
                continue;
            }
            if (drawOnlySuspicious_ && !row.suspicious) {
                continue;
            }

            Vec2 screen = {};
            if (!SDK::Drawing::WorldToScreen(row.position, screen)) {
                continue;
            }

            const std::uint32_t color = RowColor(row);
            char line1[160] = {};
            char line2[160] = {};
            std::snprintf(line1,
                          sizeof(line1),
                          "%s %s HP %.0f/%.0f",
                          row.relation,
                          row.name,
                          row.health,
                          row.maxHealth);
            std::snprintf(line2,
                          sizeof(line2),
                          "SDKDead=%d Alive=%d DB=0x%02X %s",
                          row.sdkDead ? 1 : 0,
                          row.isAliveNative ? 1 : 0,
                          static_cast<unsigned>(row.deadByteRaw),
                          row.context);

            SDK::Drawing::DrawText(screen.x - 46.0f, screen.y - 76.0f, color, line1);
            SDK::Drawing::DrawText(screen.x - 46.0f, screen.y - 60.0f, color, line2);
        }
    }

    void OnMenu() override {
        ImGui::Text("Offsets: HP=0x%X Targetable=0x%X Flags=0x%X",
                    static_cast<unsigned>(Offset::AttackableUnit::HP),
                    static_cast<unsigned>(Offset::AttackableUnit::IsTargetable),
                    static_cast<unsigned>(Offset::AttackableUnit::TargetableFlags));
        ImGui::Text("Dead field offset: 0x%X", static_cast<unsigned>(Offset::All::Dead));
        ImGui::Text("Log: %s", kLogPath);

        ImGui::Checkbox("Auto refresh", &autoRefresh_);
        ImGui::SameLine(0, 12);
        if (ImGui::Button("Refresh now")) {
            RefreshSnapshot();
        }
        ImGui::SameLine(0, 12);
        if (ImGui::Button("Clear log")) {
            ClearLog();
        }

        ImGui::SliderInt("Refresh ms", &refreshMs_, 50, 1000);
        ImGui::Checkbox("Draw on champion", &drawOnChampion_);
        ImGui::Checkbox("Draw only suspicious rows", &drawOnlySuspicious_);
        ImGui::Checkbox("Log to file", &logToFile_);
        ImGui::Checkbox("Log suspicious only", &logSuspiciousOnly_);
        ImGui::SliderInt("Log throttle ms", &logThrottleMs_, 100, 3000);

        ImGui::Separator();
        DrawContextSummary();
        ImGui::Separator();
        DrawRows();
    }

private:
    static constexpr const char* kLogPath =
        "C:\\Users\\Public\\nightsharp_isdead_debug.txt";
    struct Row {
        char relation[16] = {};
        char name[64] = {};
        char context[48] = {};
        uintptr_t address = 0;
        std::uint32_t networkId = 0;
        std::uint32_t index = 0;
        int team = 0;
        float health = 0.0f;
        float maxHealth = 0.0f;
        float distanceToPlayer = 0.0f;
        Vec3 position = {};
        bool sdkDead = false;
        bool isAliveNative = false;
        std::uint8_t deadByteRaw = 0;
        bool zombie = false;
        bool visible = false;
        bool targetable = false;
        std::uint8_t targetableByte = 0;
        std::uint32_t targetableFlags = 0;
        bool invulnerable = false;
        bool validTarget = false;
        bool targetSelectorTarget = false;
        bool orbwalkerTarget = false;
        bool lastCastTarget = false;
        bool suspicious = false;
    };

    bool autoRefresh_ = true;
    bool drawOnChampion_ = true;
    bool drawOnlySuspicious_ = false;
    bool logToFile_ = false;
    bool logSuspiciousOnly_ = true;
    int refreshMs_ = 250;
    int logThrottleMs_ = 500;
    int lastRefreshTick_ = 0;
    int lastLogTick_ = 0;
    int targetSelectorNetId_ = 0;
    int orbwalkerNetId_ = 0;
    int lastCastNetId_ = 0;
    CoreCastSpell::CastTrace lastTrace_ = {};
    std::string targetSelectorName_;
    std::string orbwalkerName_;
    std::string lastCastName_;
    std::vector<Row> rows_;
    std::ofstream logFile_;

    static const char* BoolText(bool value) {
        return value ? "true" : "false";
    }

    static ImVec4 BoolColor(bool value) {
        return value
            ? ImVec4(0.30f, 0.95f, 0.42f, 1.0f)
            : ImVec4(1.0f, 0.34f, 0.34f, 1.0f);
    }

    static std::uint32_t RowColor(const Row& row) {
        if (row.suspicious) {
            return 0xFFFF5050u;
        }
        if (row.targetSelectorTarget || row.orbwalkerTarget || row.lastCastTarget) {
            return 0xFFFFD24Au;
        }
        return row.sdkDead ? 0xFFB0B0B0u : 0xFF50FF70u;
    }

    static std::uint8_t ReadU8(uintptr_t address) {
        if (!Globals::IsValidPtr(address)) {
            return 0;
        }

        __try {
            return *reinterpret_cast<const std::uint8_t*>(address);
        } __except (1) {
            return 0;
        }
    }

    static std::uint32_t ReadU32(uintptr_t address) {
        if (!Globals::IsValidPtr(address)) {
            return 0;
        }

        __try {
            return *reinterpret_cast<const std::uint32_t*>(address);
        } __except (1) {
            return 0;
        }
    }

    static void CopyText(char* out, std::size_t outSize, const char* text) {
        if (!out || outSize == 0) {
            return;
        }
        out[0] = '\0';
        if (!text) {
            return;
        }
        std::snprintf(out, outSize, "%s", text);
    }

    static void CopyText(char* out, std::size_t outSize, const std::string& text) {
        CopyText(out, outSize, text.c_str());
    }

    static std::string BestName(const SDK::GameObject& object) {
        if (!object.IsValid()) {
            return "invalid";
        }

        std::string name = object.CharacterName();
        if (name.empty()) {
            char fallback[48] = {};
            std::snprintf(fallback, sizeof(fallback), "net:%d", object.NetworkId());
            name = fallback;
        }
        return name;
    }

    static void AppendContext(char* context, std::size_t contextSize, const char* text) {
        if (!context || contextSize == 0 || !text || !*text) {
            return;
        }

        const std::size_t used = std::strlen(context);
        if (used >= contextSize - 1) {
            return;
        }

        if (used > 0) {
            std::snprintf(context + used, contextSize - used, " %s", text);
        } else {
            std::snprintf(context, contextSize, "%s", text);
        }
    }

    static bool SameNet(std::uint32_t lhs, int rhs) {
        return lhs != 0 &&
               lhs != 0xFFFFFFFFu &&
               rhs != 0 &&
               rhs != -1 &&
               lhs == static_cast<std::uint32_t>(rhs);
    }

    void AddHero(const char* relation, const SDK::AIHeroClient& hero) {
        if (!hero.IsValid()) {
            return;
        }

        const uintptr_t address = hero.Address();
        if (!Globals::IsValidPtr(address)) {
            return;
        }

        Row row = {};
        CopyText(row.relation, sizeof(row.relation), relation);
        CopyText(row.name, sizeof(row.name), BestName(hero));
        row.address = address;
        row.networkId = static_cast<std::uint32_t>(hero.NetworkId());
        row.index = static_cast<std::uint32_t>(hero.Index());
        row.team = static_cast<int>(hero.Team());
        row.health = hero.Health();
        row.maxHealth = hero.MaxHealth();
        row.position = hero.Position();

        const auto player = SDK::ObjectManager::Player();
        if (player.IsValid()) {
            row.distanceToPlayer = player.Distance(hero);
        }

        row.sdkDead = hero.IsDead();
        row.isAliveNative = !::Core::Objects::IsDead(address);
        row.deadByteRaw = Offset::All::Dead
            ? ReadU8(address + Offset::All::Dead)
            : 0;
        row.zombie = hero.IsZombie();
        row.visible = hero.IsVisible();
        row.targetable = hero.IsTargetable();
        row.targetableByte = ReadU8(address + Offset::AttackableUnit::IsTargetable);
        row.targetableFlags = ReadU32(address + Offset::AttackableUnit::TargetableFlags);
        row.invulnerable = hero.IsInvulnerable();
        row.validTarget = SDK::Extensions::IsValidTarget(
            hero,
            FLT_MAX,
            _stricmp(relation, "Player") != 0);

        row.targetSelectorTarget = SameNet(row.networkId, targetSelectorNetId_);
        row.orbwalkerTarget = SameNet(row.networkId, orbwalkerNetId_);
        row.lastCastTarget = SameNet(row.networkId, lastCastNetId_);
        if (row.targetSelectorTarget) {
            AppendContext(row.context, sizeof(row.context), "TS");
        }
        if (row.orbwalkerTarget) {
            AppendContext(row.context, sizeof(row.context), "Orb");
        }
        if (row.lastCastTarget) {
            AppendContext(row.context, sizeof(row.context), "Cast");
        }

        const bool selectedDead =
            (row.targetSelectorTarget || row.orbwalkerTarget || row.lastCastTarget) &&
            row.sdkDead &&
            !row.zombie;
        row.suspicious = selectedDead;

        rows_.push_back(row);
    }

    void RefreshContextTargets() {
        targetSelectorNetId_ = 0;
        orbwalkerNetId_ = 0;
        lastCastNetId_ = 0;
        targetSelectorName_.clear();
        orbwalkerName_.clear();
        lastCastName_.clear();

        if (auto* ts = SDK::TargetSelector::Instance()) {
            const auto target = ts->GetTarget(2000.0f, SDK::DamageType::True);
            if (target.IsValid()) {
                targetSelectorNetId_ = target.NetworkId();
                targetSelectorName_ = BestName(target);
            }
        }

        const auto orbTarget = SDK::Orbwalker::GetTarget();
        if (orbTarget.IsValid()) {
            orbwalkerNetId_ = orbTarget.NetworkId();
            orbwalkerName_ = BestName(orbTarget);
        }

        lastTrace_ = CoreCastSpell::LastTrace();
        lastCastNetId_ = static_cast<int>(lastTrace_.targetNetworkId);
        if (lastCastNetId_ != 0 && lastCastNetId_ != -1) {
            const auto target = SDK::ObjectManager::GetUnitByNetworkId<SDK::GameObject>(lastCastNetId_);
            lastCastName_ = BestName(target);
        }
    }

    void RefreshSnapshot() {
        RefreshContextTargets();

        rows_.clear();
        rows_.reserve(8);

        AddHero("Player", SDK::ObjectManager::Player());
        for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
            AddHero("Enemy", hero);
        }

        lastRefreshTick_ = SDK::Game::TickCount();
    }

    void DrawContextSummary() const {
        ImGui::Text("Rows: %d  Last refresh tick: %d",
                    static_cast<int>(rows_.size()),
                    lastRefreshTick_);
        ImGui::Text("TargetSelector: %s net=%d",
                    targetSelectorName_.empty() ? "none" : targetSelectorName_.c_str(),
                    targetSelectorNetId_);
        ImGui::Text("Orbwalker: %s net=%d",
                    orbwalkerName_.empty() ? "none" : orbwalkerName_.c_str(),
                    orbwalkerNetId_);
        ImGui::Text("LastCast: kind=%s failure=%s success=%d slot=%u target=%s net=%d",
                    CoreCastSpell::CastKindName(lastTrace_.kind),
                    CoreCastSpell::CastFailureName(lastTrace_.failure),
                    lastTrace_.success ? 1 : 0,
                    static_cast<unsigned>(lastTrace_.slot),
                    lastCastName_.empty() ? "none" : lastCastName_.c_str(),
                    lastCastNetId_);
    }

    void DrawRows() const {
        if (rows_.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.7f, 1.0f), "No player/enemy heroes available.");
            return;
        }

        for (const auto& row : rows_) {
            ImGui::TextColored(row.suspicious
                                   ? ImVec4(1.0f, 0.25f, 0.25f, 1.0f)
                                   : ImVec4(0.82f, 0.86f, 0.90f, 1.0f),
                               "%s %-16s addr=0x%llX net=%u idx=%u team=%d %s",
                               row.relation,
                               row.name,
                               static_cast<unsigned long long>(row.address),
                               row.networkId,
                               row.index,
                               row.team,
                               row.context);
            ImGui::Text("  HP %.1f / %.1f  Dist %.0f",
                        row.health,
                        row.maxHealth,
                        row.distanceToPlayer);
            ImGui::Text("  SDKDead=");
            ImGui::SameLine();
            ImGui::TextColored(BoolColor(row.sdkDead), "%s", BoolText(row.sdkDead));
            ImGui::Text("  IsAliveNative=%s  DeadByteRaw=0x%02X",
                        BoolText(row.isAliveNative),
                        static_cast<unsigned>(row.deadByteRaw));
            ImGui::Text("  Zombie=%s  ValidTarget=%s  Visible=%s  Targetable=%s raw=%u flags=0x%08X  Invulnerable=%s",
                        BoolText(row.zombie),
                        BoolText(row.validTarget),
                        BoolText(row.visible),
                        BoolText(row.targetable),
                        static_cast<unsigned>(row.targetableByte),
                        static_cast<unsigned>(row.targetableFlags),
                        BoolText(row.invulnerable));
            ImGui::Spacing();
        }
    }

    void ClearLog() {
        if (logFile_.is_open()) {
            logFile_.close();
        }
        DeleteFileA(kLogPath);
        lastLogTick_ = 0;
    }

    void WriteLog(int now) {
        lastLogTick_ = now;

        if (!logFile_.is_open()) {
            logFile_.open(kLogPath, std::ios::out | std::ios::app);
        }
        if (!logFile_.is_open()) {
            return;
        }

        logFile_
            << "[" << now << "] context"
            << " TS=" << (targetSelectorName_.empty() ? "none" : targetSelectorName_)
            << "(" << targetSelectorNetId_ << ")"
            << " Orb=" << (orbwalkerName_.empty() ? "none" : orbwalkerName_)
            << "(" << orbwalkerNetId_ << ")"
            << " LastCastKind=" << CoreCastSpell::CastKindName(lastTrace_.kind)
            << " LastCastFailure=" << CoreCastSpell::CastFailureName(lastTrace_.failure)
            << " LastCastSuccess=" << (lastTrace_.success ? 1 : 0)
            << " LastCastTarget=" << (lastCastName_.empty() ? "none" : lastCastName_)
            << "(" << lastCastNetId_ << ")"
            << "\n";

        for (const auto& row : rows_) {
            if (logSuspiciousOnly_ && !row.suspicious) {
                continue;
            }

            logFile_
                << "[" << now << "] "
                << row.relation << " "
                << row.name
                << " net=" << row.networkId
                << " idx=" << row.index
                << " hp=" << row.health
                << "/" << row.maxHealth
                << " sdkDead=" << BoolText(row.sdkDead)
                << " isAliveNative=" << BoolText(row.isAliveNative)
                << " deadByteRaw=0x" << std::hex << static_cast<unsigned>(row.deadByteRaw) << std::dec
                << " zombie=" << BoolText(row.zombie)
                << " validTarget=" << BoolText(row.validTarget)
                << " visible=" << BoolText(row.visible)
                << " targetable=" << BoolText(row.targetable)
                << " targetableByte=" << static_cast<unsigned>(row.targetableByte)
                << " targetableFlags=" << row.targetableFlags
                << " invulnerable=" << BoolText(row.invulnerable)
                << " context=" << (row.context[0] ? row.context : "-")
                << " suspicious=" << BoolText(row.suspicious)
                << "\n";
        }
        logFile_.flush();
    }
};

} // namespace Plugins
