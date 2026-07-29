#pragma once
#include "IDeveloperTab.h"
#include "../DeveloperToolsPlugin.h"
#include <mutex>
#include <deque>
#include <string>
#include <cstdarg>
#include <algorithm>

namespace Plugins::DevTools {

class EventLoggerTab final : public IDeveloperTab {
private:
    static constexpr std::size_t kMaxEventLogEntries = 512;
    std::mutex eventLogMutex_;
    std::deque<std::string> eventLog_;
    bool logAutoScroll_ = true;

public:
    using IDeveloperTab::IDeveloperTab;

    const char* GetTabName() const override { return "Events"; }

    void OnLoad() override {
        std::lock_guard<std::mutex> lk(eventLogMutex_);
        eventLog_.clear();
    }

    void OnUnload() override {
        std::lock_guard<std::mutex> lk(eventLogMutex_);
        eventLog_.clear();
    }

    void OnProcessSpellCast(const SDK::Events::ProcessSpellEventArgs& args) override {
        if (plugin_->logProcessSpell_) {
            LogSpellEvent("ProcessSpell", args);
        }
    }

    void OnDoCastEvent(const SDK::Events::ProcessSpellEventArgs& args) override {
        if (plugin_->logDoCast_) {
            LogSpellEvent("DoCast", args);
        }
    }

    void OnFinishCastEvent(const SDK::Events::ProcessSpellEventArgs& args) override {
        if (plugin_->logFinishCast_) {
            LogSpellEvent("FinishCast", args);
        }
    }

    void OnSpellImpactEvent(const SDK::Events::ProcessSpellEventArgs& args) override {
        if (plugin_->logSpellImpact_) {
            LogSpellEvent("SpellImpact", args);
        }
    }

    void OnCastSpellEvent(const SDK::Events::CastSpellEventArgs& args) override {
        if (!plugin_->logEnabled_ || !plugin_->logCastSpell_) {
            return;
        }
        if (plugin_->logSkipAutoAttacks_ && args.Slot == 64) {
            return;
        }
        if (!PassesSourceFilter(args.Sender)) {
            return;
        }
        const char* caster = args.Sender.CharacterName[0]
            ? args.Sender.CharacterName
            : args.Sender.Name;
        if (!PassesNameFilter(caster)) {
            return;
        }
        EmitF("[CastSpell] %s slot=%s(%d) targetNetId=%u start=(%.0f, %.0f, %.0f) end=(%.0f, %.0f, %.0f)",
              caster, plugin_->SlotToString(args.Slot), args.Slot, args.TargetNetworkId,
              args.StartPosition.x, args.StartPosition.y, args.StartPosition.z,
              args.EndPosition.x, args.EndPosition.y, args.EndPosition.z);
        if (!plugin_->logVerbose_) {
            return;
        }
        EmitObjectInfo("caster", args.Sender);
        EmitF("    ptrs    castRequest=0x%llX",
              static_cast<unsigned long long>(args.CastRequest));
        EmitRawArgs(args.Raw);
    }

    void OnStopCastEvent(const SDK::Events::StopCastEventArgs& args) override {
        if (!plugin_->logEnabled_ || !plugin_->logStopCast_) {
            return;
        }
        if (!PassesSourceFilter(args.Sender)) {
            return;
        }
        const char* caster = args.Sender.CharacterName[0]
            ? args.Sender.CharacterName
            : args.Sender.Name;
        if (!PassesNameFilter(caster)) {
            return;
        }
        EmitF("[StopCast] %s slot=%s(%d) hasBeenCast=%d keepAnim=%d destroyMissile=%d missileNetId=%d castId=%d",
              caster, plugin_->SlotToString(args.Slot), args.Slot,
              args.HasBeenCast ? 1 : 0,
              args.KeepAnimationPlaying ? 1 : 0,
              args.DestroyMissile ? 1 : 0,
              args.MissileNetworkId,
              args.SpellCastId);
        if (!plugin_->logVerbose_) {
            return;
        }
        EmitObjectInfo("caster", args.Sender);
        EmitF("    ptrs    book=0x%llX processFlag=0x%llX casterNetId=%u",
              static_cast<unsigned long long>(args.Spellbook),
              static_cast<unsigned long long>(args.ProcessFlag),
              args.CasterNetworkId);
        EmitRawArgs(args.Raw);
    }

    void OnPlayAnimationEvent(const SDK::Events::PlayAnimationEventArgs& args) override {
        if (!plugin_->logEnabled_ || !plugin_->logAnimation_) {
            return;
        }
        if (!PassesSourceFilter(args.Sender)) {
            return;
        }
        if (!PassesNameFilter(args.Animation)) {
            return;
        }
        const char* owner = args.Sender.CharacterName[0]
            ? args.Sender.CharacterName
            : args.Sender.Name;
        EmitF("[Animation] %s anim='%s' id=%d accepted=%d",
              owner, args.Animation, args.AnimationId, args.Accepted ? 1 : 0);
        if (!plugin_->logVerbose_) {
            return;
        }
        EmitObjectInfo("owner", args.Sender);
        EmitRawArgs(args.Raw);
    }

    void OnBuffAddEvent(const SDK::Events::BuffEventArgs& args) override {
        if (plugin_->logBuffAdd_) {
            LogBuffEvent("BuffAdd", args);
        }
    }

    void OnBuffRemoveEvent(const SDK::Events::BuffEventArgs& args) override {
        if (plugin_->logBuffRemove_) {
            LogBuffEvent("BuffRemove", args);
        }
    }

    void OnBuffUpdateEvent(const SDK::Events::BuffEventArgs& args) override {
        if (plugin_->logBuffUpdate_) {
            LogBuffEvent("BuffUpdate", args);
        }
    }

    void OnNewPathEvent(const SDK::Events::NewPathEventArgs& args) override {
        if (!plugin_->logEnabled_ || !plugin_->logNewPath_) {
            return;
        }
        if (!PassesSourceFilter(args.Sender)) {
            return;
        }
        const char* owner = args.Sender.CharacterName[0]
            ? args.Sender.CharacterName
            : args.Sender.Name;
        if (!PassesNameFilter(owner)) {
            return;
        }
        const int pathCount = std::clamp(args.PathCount, 0, 32);
        const Vec3 dest = pathCount > 0 ? args.Path[pathCount - 1] : Vec3{};
        EmitF("[NewPath] %s isDash=%d speed=%.1f waypoints=%d dest=(%.0f, %.0f, %.0f)",
              owner, args.IsDash ? 1 : 0, args.Speed, pathCount,
              dest.x, dest.y, dest.z);
        if (!plugin_->logVerbose_) {
            return;
        }
        EmitObjectInfo("owner", args.Sender);
        const int shown = std::min(pathCount, 8);
        for (int i = 0; i < shown; ++i) {
            EmitF("    wp[%d]   (%.1f, %.1f, %.1f)",
                  i, args.Path[i].x, args.Path[i].y, args.Path[i].z);
        }
        if (pathCount > shown) {
            EmitF("    wp      ... %d more waypoints omitted", pathCount - shown);
        }
        EmitF("    ptrs    pathArray=0x%llX",
              static_cast<unsigned long long>(args.PathArray));
        EmitRawArgs(args.Raw);
    }

    void OnDrawTab() override {
        if (ImGui::Checkbox("Enable Event Logging", &plugin_->logEnabled_)) {
            if (plugin_->menuLogEnabled_) plugin_->menuLogEnabled_->SetValue(plugin_->logEnabled_);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Verbose", &plugin_->logVerbose_)) {
            if (plugin_->menuLogVerbose_) plugin_->menuLogVerbose_->SetValue(plugin_->logVerbose_);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Raw Registers", &plugin_->logRaw_)) {
            if (plugin_->menuLogRaw_) plugin_->menuLogRaw_->SetValue(plugin_->logRaw_);
        }

        if (ImGui::Checkbox("Skip Auto Attacks", &plugin_->logSkipAutoAttacks_)) {
            if (plugin_->menuLogSkipAA_) plugin_->menuLogSkipAA_->SetValue(plugin_->logSkipAutoAttacks_);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Write to Debug Log", &plugin_->logToFile_)) {
            if (plugin_->menuLogToFile_) plugin_->menuLogToFile_->SetValue(plugin_->logToFile_);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto Scroll", &logAutoScroll_);

        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::Combo("Source", &plugin_->logSourceIndex_,
                         "Local Player Only\0Player + Allies\0Enemies Only\0Everyone\0")) {
            if (plugin_->menuLogSource_) plugin_->menuLogSource_->SetValue(plugin_->logSourceIndex_);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputText("Name Filter", plugin_->logNameFilter_, sizeof(plugin_->logNameFilter_));

        if (ImGui::TreeNode("Tracked Events")) {
            struct EventToggle {
                const char* Label;
                bool* Flag;
                SDK::UI::MenuBool** Control;
            };
            EventToggle toggles[] = {
                { "OnProcessSpell",     &plugin_->logProcessSpell_, &plugin_->menuLogProcessSpell_ },
                { "OnDoCast",           &plugin_->logDoCast_,       &plugin_->menuLogDoCast_ },
                { "OnFinishCast",       &plugin_->logFinishCast_,   &plugin_->menuLogFinishCast_ },
                { "OnSpellImpact",      &plugin_->logSpellImpact_,  &plugin_->menuLogSpellImpact_ },
                { "OnProcessCastSpell", &plugin_->logCastSpell_,    &plugin_->menuLogCastSpell_ },
                { "OnStopCast",         &plugin_->logStopCast_,     &plugin_->menuLogStopCast_ },
                { "OnPlayAnimation",    &plugin_->logAnimation_,    &plugin_->menuLogAnimation_ },
                { "OnBuffAdd",          &plugin_->logBuffAdd_,      &plugin_->menuLogBuffAdd_ },
                { "OnBuffRemove",       &plugin_->logBuffRemove_,   &plugin_->menuLogBuffRemove_ },
                { "OnBuffUpdate",       &plugin_->logBuffUpdate_,   &plugin_->menuLogBuffUpdate_ },
                { "OnNewPath",          &plugin_->logNewPath_,      &plugin_->menuLogNewPath_ },
            };
            ImGui::Columns(2, "EventToggleColumns", false);
            for (auto& toggle : toggles) {
                if (ImGui::Checkbox(toggle.Label, toggle.Flag)) {
                    if (*toggle.Control) (*toggle.Control)->SetValue(*toggle.Flag);
                }
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::TreePop();
        }

        std::size_t entryCount = 0;
        {
            std::lock_guard<std::mutex> lk(eventLogMutex_);
            entryCount = eventLog_.size();
        }

        if (ImGui::Button("Copy Event Log")) {
            OnCopyHotkey();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Event Log")) {
            std::lock_guard<std::mutex> lk(eventLogMutex_);
            eventLog_.clear();
            entryCount = 0;
        }
        ImGui::SameLine();
        ImGui::Text("%zu / %zu lines", entryCount, kMaxEventLogEntries);

        if (ImGui::BeginChild("EventLogScroll", ImVec2(0, 260), true,
                              ImGuiWindowFlags_HorizontalScrollbar)) {
            std::lock_guard<std::mutex> lk(eventLogMutex_);
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(eventLog_.size()));
            while (clipper.Step()) {
                for (int index = clipper.DisplayStart;
                     index < clipper.DisplayEnd;
                     ++index) {
                    ImGui::TextUnformatted(
                        eventLog_[static_cast<std::size_t>(index)].c_str());
                }
            }
            if (logAutoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
    }

    void OnCopyHotkey() override {
        std::string dump = "=== DEVELOPER TOOLS EVENT LOG ===\n";
        std::size_t entryCount = 0;
        {
            std::lock_guard<std::mutex> lk(eventLogMutex_);
            entryCount = eventLog_.size();
            for (const auto& line : eventLog_) {
                dump += line;
                dump += '\n';
            }
        }
        ImGui::SetClipboardText(dump.c_str());
        NightSharpDebug::Logf("[Dev] Copied %zu event log lines to Clipboard!", entryCount);
    }

private:
    void Emit(const char* line) {
        if (!line || !line[0]) {
            return;
        }
        if (plugin_->logToFile_) {
            NightSharpDebug::Logf("%s", line);
        }
        std::lock_guard<std::mutex> lk(eventLogMutex_);
        if (eventLog_.size() >= kMaxEventLogEntries) {
            eventLog_.pop_front();
        }
        eventLog_.emplace_back(line);
    }

    void EmitF(const char* fmt, ...) {
        char buffer[1400] = {};
        va_list vl;
        va_start(vl, fmt);
        _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, vl);
        va_end(vl);
        Emit(buffer);
    }

    void EmitObjectInfo(const char* label, const ::Core::Events::ObjectInfo& info) {
        if (!info.IsValid()) {
            EmitF("    %-7s <invalid>", label);
            return;
        }
        EmitF("    %-7s name='%s' char='%s' netId=%u idx=%u team=%u type=%s "
              "dead=%d vis=%d clone=%d pet=%d zombie=%d ptr=0x%llX pos=(%.1f, %.1f, %.1f)",
              label,
              info.Name,
              info.CharacterName,
              info.NetworkId,
              info.Index,
              info.Team,
              plugin_->ObjectTypeToString(info.Type),
              info.IsDead ? 1 : 0,
              info.IsVisible ? 1 : 0,
              info.IsClone ? 1 : 0,
              info.IsPet ? 1 : 0,
              info.IsZombie ? 1 : 0,
              static_cast<unsigned long long>(info.Ptr),
              info.Position.x, info.Position.y, info.Position.z);
    }

    void EmitRawArgs(const ::Core::Events::RawEventArgs& raw) {
        if (!plugin_->logRaw_) {
            return;
        }
        EmitF("    raw     rcx=0x%llX rdx=0x%llX r8=0x%llX r9=0x%llX target=0x%llX hits=%lld",
              static_cast<unsigned long long>(raw.Rcx),
              static_cast<unsigned long long>(raw.Rdx),
              static_cast<unsigned long long>(raw.R8),
              static_cast<unsigned long long>(raw.R9),
              static_cast<unsigned long long>(raw.Target),
              raw.HitCount);
        EmitF("    raw     xmm=[%.3f, %.3f, %.3f, %.3f] stack=[0x%llX, 0x%llX, 0x%llX, 0x%llX, 0x%llX, 0x%llX]",
              raw.Xmm0, raw.Xmm1, raw.Xmm2, raw.Xmm3,
              static_cast<unsigned long long>(raw.Stack0),
              static_cast<unsigned long long>(raw.Stack1),
              static_cast<unsigned long long>(raw.Stack2),
              static_cast<unsigned long long>(raw.Stack3),
              static_cast<unsigned long long>(raw.Stack4),
              static_cast<unsigned long long>(raw.Stack5));
    }

    void LogSpellEvent(const char* tag, const SDK::Events::ProcessSpellEventArgs& args) {
        if (!plugin_->enabled_ || !plugin_->logEnabled_) {
            return;
        }
        if (plugin_->logSkipAutoAttacks_ && (args.IsAutoAttack || args.Slot == 64)) {
            return;
        }
        if (!PassesSourceFilter(args.Sender)) {
            return;
        }
        if (!PassesNameFilter(args.SpellName, args.ScriptName, args.MissileName)) {
            return;
        }

        const char* spellName = args.SpellName[0] ? args.SpellName : args.ScriptName;
        const char* casterName = args.Sender.CharacterName[0]
            ? args.Sender.CharacterName
            : args.Sender.Name;
        const char* targetName = args.Target.IsValid()
            ? (args.Target.CharacterName[0] ? args.Target.CharacterName : args.Target.Name)
            : "-";

        EmitF("[%s] %s -> '%s' slot=%s(%d) target=%s#%u delay=%.3f castTime=%.3f speed=%.0f end=(%.0f, %.0f, %.0f)",
              tag,
              casterName,
              spellName,
              plugin_->SlotToString(args.Slot), args.Slot,
              targetName, args.TargetNetworkId,
              args.CastDelay, args.CastTime, args.MissileSpeed,
              args.EndPosition.x, args.EndPosition.y, args.EndPosition.z);

        if (!plugin_->logVerbose_) {
            return;
        }

        EmitF("    names   spell='%s' script='%s' missile='%s' slotName='%s' payloadSpell='%s' payloadMissile='%s' slotFallback=%d",
              args.SpellName, args.ScriptName, args.MissileName, args.SpellSlotName,
              args.PayloadSpellName, args.PayloadMissileName,
              args.SpellNameFromSlotFallback ? 1 : 0);
        EmitF("    flags   IsSpell=%d IsAutoAttack=%d IsSpecialAttack=%d slot=%d(%s) srcIdx=%d tgtIdx=%d casterNetId=%u targetNetId=%u",
              args.IsSpell ? 1 : 0,
              args.IsAutoAttack ? 1 : 0,
              args.IsSpecialAttack ? 1 : 0,
              args.Slot, plugin_->SlotToString(args.Slot),
              args.SourceIndex, args.TargetIndex,
              args.CasterNetworkId, args.TargetNetworkId);

        EmitObjectInfo("caster", args.Sender);
        EmitObjectInfo("target", args.Target);

        const float travelDist = args.StartPosition.Distance(args.EndPosition);
        const float flightTime = args.MissileSpeed > 1.0f ? travelDist / args.MissileSpeed : 0.0f;
        EmitF("    timing  castDelay=%.3f castTime=%.3f missileSpeed=%.1f dist=%.1f flight=%.3fs total=%.3fs",
              args.CastDelay, args.CastTime, args.MissileSpeed,
              travelDist, flightTime, args.CastDelay + flightTime);
        EmitF("    vectors start=(%.1f, %.1f, %.1f) end=(%.1f, %.1f, %.1f) cast=(%.1f, %.1f, %.1f)",
              args.StartPosition.x, args.StartPosition.y, args.StartPosition.z,
              args.EndPosition.x, args.EndPosition.y, args.EndPosition.z,
              args.CastPosition.x, args.CastPosition.y, args.CastPosition.z);
        EmitF("    ptrs    book=0x%llX castInfo=0x%llX input=0x%llX data=0x%llX res=0x%llX",
              static_cast<unsigned long long>(args.Spellbook),
              static_cast<unsigned long long>(args.CastInfo),
              static_cast<unsigned long long>(args.SpellInput),
              static_cast<unsigned long long>(args.SpellData),
              static_cast<unsigned long long>(args.SpellDataResource));
        EmitRawArgs(args.Raw);
    }

    void LogBuffEvent(const char* tag, const SDK::Events::BuffEventArgs& args) {
        if (!plugin_->enabled_ || !plugin_->logEnabled_) {
            return;
        }
        if (!PassesSourceFilter(args.Sender)) {
            return;
        }
        if (!PassesNameFilter(args.BuffName)) {
            return;
        }

        const char* owner = args.Sender.CharacterName[0]
            ? args.Sender.CharacterName
            : args.Sender.Name;
        EmitF("[%s] %s buff='%s' stacks=%d type=%d start=%.2f end=%.2f duration=%.2f",
              tag, owner, args.BuffName, args.Count, args.Type,
              args.StartTime, args.EndTime, args.EndTime - args.StartTime);

        if (!plugin_->logVerbose_) {
            return;
        }
        EmitObjectInfo("owner", args.Sender);
        EmitF("    ptrs    buff=0x%llX bridge=0x%llX ownerComp=0x%llX traceHook=%u traceSerial=%llu",
              static_cast<unsigned long long>(args.BuffAddress),
              static_cast<unsigned long long>(args.EventBridge),
              static_cast<unsigned long long>(args.OwnerComponent),
              args.BuffTraceHookId,
              static_cast<unsigned long long>(args.BuffTraceSerial));
        EmitRawArgs(args.Raw);
    }

    bool PassesSourceFilter(const ::Core::Events::ObjectInfo& sender) const {
        if (plugin_->logSourceIndex_ == 3) {
            return true;
        }
        if (!sender.IsValid()) {
            return false;
        }
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            return false;
        }
        const bool isPlayer = sender.NetworkId == player.NetworkId() ||
                              sender.Ptr == player.Address();
        const bool sameTeam = sender.Team == static_cast<std::uint32_t>(player.Team());
        switch (plugin_->logSourceIndex_) {
        case 0:  return isPlayer;
        case 1:  return isPlayer || sameTeam;
        case 2:  return !sameTeam;
        default: return true;
        }
    }

    bool PassesNameFilter(const char* a, const char* b = nullptr, const char* c = nullptr) const {
        if (!plugin_->logNameFilter_[0]) {
            return true;
        }
        return ContainsNoCase(a, plugin_->logNameFilter_) ||
               (b && ContainsNoCase(b, plugin_->logNameFilter_)) ||
               (c && ContainsNoCase(c, plugin_->logNameFilter_));
    }

    static bool ContainsNoCase(const char* haystack, const char* needle) {
        if (!haystack || !needle || !needle[0]) return true;
        const size_t hLen = std::strlen(haystack);
        const size_t nLen = std::strlen(needle);
        if (nLen > hLen) return false;
        for (size_t i = 0; i + nLen <= hLen; ++i) {
            size_t j = 0;
            for (; j < nLen; ++j) {
                if (std::tolower(static_cast<unsigned char>(haystack[i + j])) !=
                    std::tolower(static_cast<unsigned char>(needle[j]))) {
                    break;
                }
            }
            if (j == nLen) return true;
        }
        return false;
    }
};

} // namespace Plugins::DevTools
