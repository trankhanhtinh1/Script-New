#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreRuntime.h"
#include "../../Core/Globals.h"
#include "../../Core/offset.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../SDK/UI/IMenu/Menu.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace Plugins {

// Lifecycle test for:
// ADD_SPELL("Ezreal", "EzrealMysticShot", "ezrealq", SpellSlot::Q,
//           SpellType::Line, 2, 250, 2000, 1150, 60, true)
//
// Detection chain:
//   OnProcessSpell -> pending Ezreal Q cast
//   OnMissileCreate -> bind the real MissileClient by payload network id
//   OnMissileDelete -> remove that exact tracked missile
class EzrealMissileLifecyclePlugin final : public IPlugin {
public:
    const char* GetName() const override {
        return "Ezreal Q Missile Lifecycle";
    }

    const char* GetInternalId() const override {
        return "champion.ezreal_q_missile_lifecycle";
    }

    const char* GetAuthor() const override {
        return "NightSharp";
    }

    PluginCategory GetCategory() const override {
        return PluginCategory::Champion;
    }

    const char* GetChampionName() const override {
        return "Ezreal";
    }

    bool AutoLoadByDefault() const override {
        return true;
    }

    bool CanLoad() const override {
        if (!CoreRuntime::EnsureInitialized() ||
            !CoreRuntime::RefreshReadState()) {
            return false;
        }

        const auto player = SDK::ObjectManager::Player();
        return player.IsValid() &&
               _stricmp(player.CharacterName().c_str(), kSpell.Champion) == 0;
    }

    void OnLoad() override {
        s_instance = this;
        ResetState();
        ResetLogFile();
        CreateMenu();

        const bool process =
            SDK::Events::hook.OnProcessSpell +=
                &EzrealMissileLifecyclePlugin::OnProcessSpell;
        const bool create =
            SDK::Events::hook.OnMissileCreate +=
                &EzrealMissileLifecyclePlugin::OnMissileCreate;
        const bool remove =
            SDK::Events::hook.OnMissileDelete +=
                &EzrealMissileLifecyclePlugin::OnMissileDelete;

        Appendf(
            "[EzrealQMissile] loaded process=%d create=%d delete=%d "
            "spell='%s' missile='%s' delay=%d speed=%.0f range=%.0f radius=%.0f collision=%d\r\n",
            process ? 1 : 0,
            create ? 1 : 0,
            remove ? 1 : 0,
            kSpell.SpellName,
            kSpell.MissileName,
            kSpell.DelayMs,
            kSpell.Speed,
            kSpell.Range,
            kSpell.Radius,
            kSpell.Collision ? 1 : 0);
        NightSharpDebug::Logf("[EzrealQMissile] loaded");
    }

    void OnUnload() override {
        SDK::Events::hook.OnMissileDelete -=
            &EzrealMissileLifecyclePlugin::OnMissileDelete;
        SDK::Events::hook.OnMissileCreate -=
            &EzrealMissileLifecyclePlugin::OnMissileCreate;
        SDK::Events::hook.OnProcessSpell -=
            &EzrealMissileLifecyclePlugin::OnProcessSpell;

        ResetState();
        Appendf("[EzrealQMissile] unloaded\r\n");
        DestroyMenu();
        if (s_instance == this) {
            s_instance = nullptr;
        }
        NightSharpDebug::Logf("[EzrealQMissile] unloaded");
    }

    void OnUpdate() override {
        const int now = SDK::Game::TickCount();
        AcquireSRWLockExclusive(&m_stateLock);
        for (auto& pending : m_pending) {
            if (pending.Active && now - pending.ProcessTick > kPendingTimeoutMs) {
                pending = {};
            }
        }
        for (auto& missile : m_active) {
            if (missile.Active && now - missile.CreateTick > kMissileSafetyTimeoutMs) {
                missile = {};
                InterlockedIncrement64(&m_timeoutCount);
            }
        }
        ReleaseSRWLockExclusive(&m_stateLock);
    }

    void OnRender() override {
        if (!Enabled() || !ImGui::GetCurrentContext()) {
            return;
        }

        PendingCast pendingSnapshot[kMaxPending] = {};
        ActiveMissile activeSnapshot[kMaxActive] = {};
        AcquireSRWLockShared(&m_stateLock);
        for (int i = 0; i < kMaxPending; ++i) {
            pendingSnapshot[i] = m_pending[i];
        }
        for (int i = 0; i < kMaxActive; ++i) {
            activeSnapshot[i] = m_active[i];
        }
        ReleaseSRWLockShared(&m_stateLock);

        const int now = SDK::Game::TickCount();
        if (DrawPending()) {
            for (const auto& pending : pendingSnapshot) {
                if (!pending.Active || !pending.Confirmed ||
                    now - pending.ProcessTick > kPendingTimeoutMs) {
                    continue;
                }
                DrawCorridor(
                    pending.Start,
                    pending.End,
                    kSpell.Radius,
                    IM_COL32(255, 205, 40, 220),
                    "OnProcessSpell");
            }
        }

        for (const auto& missile : activeSnapshot) {
            if (!missile.Active) {
                continue;
            }

            Vec3 current = {};
            if (!ReadMissilePosition(missile.MissilePtr, current)) {
                const float elapsed =
                    static_cast<float>(std::max(0, now - missile.CreateTick)) /
                    1000.0f;
                const float distance =
                    std::min(kSpell.Range, elapsed * kSpell.Speed);
                current = missile.Start.Extend(missile.End, distance);
            }

            if (current.Distance2D(missile.End) < 5.0f) {
                current = missile.End;
            }

            char label[96] = {};
            std::snprintf(
                label,
                sizeof(label),
                "OnCreateMissile  net=%u",
                missile.NetworkId);
            DrawCorridor(
                current,
                missile.End,
                kSpell.Radius,
                ActiveColor(),
                label);
        }
    }

    void OnMenu() override {
        if (!m_menu) {
            return;
        }

        m_menu->DrawImGui();
        ImGui::Separator();
        ImGui::Text("OnProcessSpell Q: %lld",
                    ReadCounter(m_processCount));
        ImGui::Text("OnMissileCreate matched: %lld",
                    ReadCounter(m_createCount));
        ImGui::Text("OnMissileDelete matched: %lld",
                    ReadCounter(m_deleteCount));
        ImGui::Text("Create without pending: %lld",
                    ReadCounter(m_orphanCreateCount));
        ImGui::Text("Safety timeouts: %lld",
                    ReadCounter(m_timeoutCount));
        ImGui::Text("Last event: %s", m_lastEvent);
        ImGui::Text("Log: %s", kLogPath);
    }

private:
    struct SpellSpec {
        const char* Champion;
        const char* SpellName;
        const char* MissileName;
        int Slot;
        int Danger;
        int DelayMs;
        float Speed;
        float Range;
        float Radius;
        bool Collision;
    };

    inline static constexpr SpellSpec kSpell = {
        "Ezreal",
        "EzrealQ",
        "EzrealMysticShotMissile",
        0,
        1,
        250,
        2000.0f,
        1150.0f,
        60.0f,
        true
    };

    struct PendingCast {
        bool Active = false;
        bool Confirmed = false;
        uint32_t CasterNetworkId = 0;
        int ProcessTick = 0;
        Vec3 Start = {};
        Vec3 End = {};
    };

    struct ActiveMissile {
        bool Active = false;
        uintptr_t MissilePtr = 0;
        uint32_t NetworkId = 0;
        uint32_t CasterNetworkId = 0;
        int CreateTick = 0;
        Vec3 Start = {};
        Vec3 End = {};
    };

    static inline EzrealMissileLifecyclePlugin* s_instance = nullptr;
    static constexpr const char* kLogPath =
        "C:\\Users\\Public\\nightsharp_ezreal_missile_lifecycle.txt";
    static constexpr int kMaxPending = 8;
    static constexpr int kMaxActive = 8;
    static constexpr int kPendingTimeoutMs = 1500;
    static constexpr int kMissileSafetyTimeoutMs = 3000;

    SRWLOCK m_stateLock = SRWLOCK_INIT;
    PendingCast m_pending[kMaxPending] = {};
    ActiveMissile m_active[kMaxActive] = {};
    int m_pendingCursor = 0;
    int m_activeCursor = 0;

    Menu* m_menu = nullptr;
    MenuBool* m_enabledMenu = nullptr;
    MenuBool* m_drawPendingMenu = nullptr;
    MenuBool* m_writeLogMenu = nullptr;
    MenuColor* m_colorMenu = nullptr;

    volatile long long m_processCount = 0;
    volatile long long m_createCount = 0;
    volatile long long m_deleteCount = 0;
    volatile long long m_orphanCreateCount = 0;
    volatile long long m_timeoutCount = 0;
    char m_lastEvent[96] = "none";

    static bool EqualsInsensitive(const char* left, const char* right) {
        return left && right && left[0] && right[0] &&
               _stricmp(left, right) == 0;
    }

    static bool IsKnownQMissileName(const char* value) {
        return EqualsInsensitive(value, kSpell.MissileName) ||
               EqualsInsensitive(value, kSpell.SpellName);
    }

    static bool IsKnownQProcessName(const char* value) {
        return EqualsInsensitive(value, kSpell.SpellName);
    }

    static bool IsPlausibleWorld(const Vec3& value) {
        return value.IsValid() &&
               std::fabs(value.x) <= 30000.0f &&
               std::fabs(value.y) <= 5000.0f &&
               std::fabs(value.z) <= 30000.0f &&
               (std::fabs(value.x) > 1.0f || std::fabs(value.z) > 1.0f);
    }

    static bool ReadNativeWorld(uintptr_t address, Vec3& out) {
        out = {};
        if (!Globals::IsValidPtr(address)) {
            return false;
        }

        __try {
            const Vec3 native = Globals::Read<Vec3>(address);
            out = native;
        } __except (1) {
            out = {};
            return false;
        }
        return IsPlausibleWorld(out);
    }

    static bool ReadMissilePosition(uintptr_t missile, Vec3& out) {
        return Globals::IsValidPtr(missile) &&
               ReadNativeWorld(
                   missile + Offset::MissileClient::Position,
                   out);
    }

    static Vec3 ResolveStart(
        const SDK::Events::ProcessSpellEventArgs& args) {
        if (IsPlausibleWorld(args.StartPosition)) {
            return args.StartPosition;
        }

        Vec3 position = {};
        if (ReadNativeWorld(
                args.Sender.Ptr + Offset::All::Position,
                position)) {
            return position;
        }
        return {};
    }

    static Vec3 ResolveEnd(
        const SDK::Events::ProcessSpellEventArgs& args,
        const Vec3& start) {
        Vec3 candidate = args.CastPosition;
        if (!IsPlausibleWorld(candidate) ||
            candidate.Distance2D(start) < 5.0f) {
            candidate = args.EndPosition;
        }
        if (!IsPlausibleWorld(candidate) ||
            candidate.Distance2D(start) < 5.0f) {
            candidate = SDK::Game::CursorPos();
        }
        if (!IsPlausibleWorld(start) ||
            !IsPlausibleWorld(candidate) ||
            candidate.Distance2D(start) < 5.0f) {
            return {};
        }
        return start.Extend(candidate, kSpell.Range);
    }

    static Vec3 ResolveMissileStart(
        const SDK::Events::ObjectEventArgs& args,
        const PendingCast* pending) {
        if (IsPlausibleWorld(args.StartPosition)) {
            return args.StartPosition;
        }
        return pending ? pending->Start : Vec3{};
    }

    static Vec3 ResolveMissileEnd(
        const SDK::Events::ObjectEventArgs& args,
        const Vec3& start,
        const PendingCast* pending) {
        Vec3 candidate = args.EndPosition;
        if (!IsPlausibleWorld(candidate) ||
            candidate.Distance2D(start) < 5.0f) {
            candidate = args.CastEndPosition;
        }
        if ((!IsPlausibleWorld(candidate) ||
             candidate.Distance2D(start) < 5.0f) && pending) {
            candidate = pending->End;
        }
        if (!IsPlausibleWorld(start) ||
            !IsPlausibleWorld(candidate) ||
            candidate.Distance2D(start) < 5.0f) {
            return {};
        }
        return start.Extend(candidate, kSpell.Range);
    }

    static void OnProcessSpell(
        const SDK::Events::ProcessSpellEventArgs& args) {
        auto* self = s_instance;
        if (!self || !self->Enabled() ||
            !SDK::Events::IsLocalPlayer(args.Sender) ||
            args.Slot != kSpell.Slot ||
            _stricmp(args.Sender.CharacterName, kSpell.Champion) != 0) {
            return;
        }

        if (!IsKnownQProcessName(args.SpellName) &&
            !IsKnownQProcessName(args.SpellSlotName)) {
            self->SetLastEvent("ProcessSpell rejected: name mismatch");
            self->Appendf(
                "[EzrealQMissile] PROCESS_REJECT tick=%d casterNet=%u slot=%d "
                "spell='%s' slotSpell='%s' missile='%s' reason=name\r\n",
                SDK::Game::TickCount(),
                args.Sender.NetworkId,
                args.Slot,
                args.SpellName,
                args.SpellSlotName,
                args.MissileName);
            return;
        }

        const Vec3 start = ResolveStart(args);
        const Vec3 end = ResolveEnd(args, start);
        if (!IsPlausibleWorld(start) || !IsPlausibleWorld(end)) {
            self->SetLastEvent("ProcessSpell rejected: invalid geometry");
            self->Appendf(
                "[EzrealQMissile] PROCESS_REJECT tick=%d slot=%d spell='%s' "
                "slotSpell='%s' missile='%s' start=%.1f %.1f %.1f "
                "cast=%.1f %.1f %.1f end=%.1f %.1f %.1f\r\n",
                SDK::Game::TickCount(),
                args.Slot,
                args.SpellName,
                args.SpellSlotName,
                args.MissileName,
                start.x, start.y, start.z,
                args.CastPosition.x, args.CastPosition.y, args.CastPosition.z,
                end.x, end.y, end.z);
            return;
        }

        PendingCast pending{};
        pending.Active = true;
        pending.CasterNetworkId = args.Sender.NetworkId;
        pending.ProcessTick = SDK::Game::TickCount();
        pending.Start = start;
        pending.End = end;

        AcquireSRWLockExclusive(&self->m_stateLock);
        self->m_pending[
            self->m_pendingCursor++ % kMaxPending] = pending;
        ReleaseSRWLockExclusive(&self->m_stateLock);

        InterlockedIncrement64(&self->m_processCount);
        self->SetLastEvent("OnProcessSpell: Ezreal Q pending");
        self->Appendf(
            "[EzrealQMissile] PROCESS tick=%d casterNet=%u slot=%d spell='%s' "
            "slotSpell='%s' missile='%s' start=%.1f %.1f %.1f "
            "cast=%.1f %.1f %.1f end=%.1f %.1f %.1f\r\n",
            pending.ProcessTick,
            pending.CasterNetworkId,
            args.Slot,
            args.SpellName,
            args.SpellSlotName,
            args.MissileName,
            pending.Start.x, pending.Start.y, pending.Start.z,
            args.CastPosition.x, args.CastPosition.y, args.CastPosition.z,
            pending.End.x, pending.End.y, pending.End.z);
    }

    static void OnMissileCreate(
        const SDK::Events::ObjectEventArgs& args) {
        auto* self = s_instance;
        if (!self || !self->Enabled() ||
            !SDK::Events::IsLocalPlayer(args.Source)) {
            return;
        }

        const int now = SDK::Game::TickCount();
        PendingCast matched{};
        bool foundPending = false;

        AcquireSRWLockExclusive(&self->m_stateLock);
        int bestIndex = -1;
        int bestAge = kPendingTimeoutMs + 1;
        for (int i = 0; i < kMaxPending; ++i) {
            const auto& pending = self->m_pending[i];
            if (!pending.Active ||
                pending.CasterNetworkId != args.SourceNetworkId) {
                continue;
            }
            const int age = now - pending.ProcessTick;
            if (age >= 0 && age <= kPendingTimeoutMs && age < bestAge) {
                bestAge = age;
                bestIndex = i;
            }
        }
        if (bestIndex >= 0) {
            matched = self->m_pending[bestIndex];
            foundPending = true;
        }
        ReleaseSRWLockExclusive(&self->m_stateLock);

        const bool nameMatch =
            IsKnownQMissileName(args.SpellName) ||
            IsKnownQMissileName(args.MissileName);
        if (!nameMatch) {
            if (foundPending) {
                AcquireSRWLockExclusive(&self->m_stateLock);
                self->m_pending[bestIndex] = {};
                ReleaseSRWLockExclusive(&self->m_stateLock);
            }
            return;
        }

        if (foundPending) {
            AcquireSRWLockExclusive(&self->m_stateLock);
            self->m_pending[bestIndex].Confirmed = true;
            ReleaseSRWLockExclusive(&self->m_stateLock);
        }

        const Vec3 start =
            ResolveMissileStart(args, foundPending ? &matched : nullptr);
        const Vec3 end =
            ResolveMissileEnd(args, start, foundPending ? &matched : nullptr);
        if (!IsPlausibleWorld(start) || !IsPlausibleWorld(end)) {
            self->SetLastEvent("MissileCreate rejected: invalid geometry");
            self->Appendf(
                "[EzrealQMissile] CREATE_REJECT tick=%d ptr=0x%llX net=%u "
                "spell='%s' missile='%s' pending=%d\r\n",
                now,
                static_cast<unsigned long long>(args.Raw.Rcx),
                args.MissileNetworkId,
                args.SpellName,
                args.MissileName,
                foundPending ? 1 : 0);
            return;
        }

        ActiveMissile active{};
        active.Active = true;
        active.MissilePtr = args.Raw.Rcx;
        active.NetworkId = args.MissileNetworkId;
        active.CasterNetworkId = args.SourceNetworkId;
        active.CreateTick = now;
        active.Start = start;
        active.End = end;

        AcquireSRWLockExclusive(&self->m_stateLock);
        int activeIndex = -1;
        for (int i = 0; i < kMaxActive; ++i) {
            if (self->m_active[i].Active &&
                active.NetworkId != 0 &&
                self->m_active[i].NetworkId == active.NetworkId) {
                activeIndex = i;
                break;
            }
            if (activeIndex < 0 && !self->m_active[i].Active) {
                activeIndex = i;
            }
        }
        if (activeIndex < 0) {
            activeIndex = self->m_activeCursor++ % kMaxActive;
        }
        self->m_active[activeIndex] = active;
        ReleaseSRWLockExclusive(&self->m_stateLock);

        InterlockedIncrement64(&self->m_createCount);
        if (!foundPending) {
            InterlockedIncrement64(&self->m_orphanCreateCount);
        }
        self->SetLastEvent("OnMissileCreate: drawing Ezreal Q");
        self->Appendf(
            "[EzrealQMissile] CREATE tick=%d ptr=0x%llX net=%u casterNet=%u "
            "spell='%s' missile='%s' pending=%d start=%.1f %.1f %.1f "
            "end=%.1f %.1f %.1f\r\n",
            now,
            static_cast<unsigned long long>(active.MissilePtr),
            active.NetworkId,
            active.CasterNetworkId,
            args.SpellName,
            args.MissileName,
            foundPending ? 1 : 0,
            active.Start.x, active.Start.y, active.Start.z,
            active.End.x, active.End.y, active.End.z);
    }

    static void OnMissileDelete(
        const SDK::Events::ObjectEventArgs& args) {
        auto* self = s_instance;
        if (!self || !self->Enabled()) {
            return;
        }

        ActiveMissile removed{};
        bool found = false;
        AcquireSRWLockExclusive(&self->m_stateLock);
        for (auto& active : self->m_active) {
            if (!active.Active) {
                continue;
            }

            const bool networkMatch =
                args.MissileNetworkId != 0 &&
                active.NetworkId == args.MissileNetworkId;
            const bool pointerMatch =
                args.Raw.Rcx != 0 &&
                active.MissilePtr == args.Raw.Rcx;
            if (!networkMatch && !pointerMatch) {
                continue;
            }

            removed = active;
            active = {};
            found = true;
            break;
        }
        ReleaseSRWLockExclusive(&self->m_stateLock);

        if (!found) {
            return;
        }

        InterlockedIncrement64(&self->m_deleteCount);
        self->SetLastEvent("OnMissileDelete: drawing removed");
        self->Appendf(
            "[EzrealQMissile] DELETE tick=%d ptr=0x%llX net=%u casterNet=%u "
            "spell='%s' missile='%s'\r\n",
            SDK::Game::TickCount(),
            static_cast<unsigned long long>(args.Raw.Rcx),
            args.MissileNetworkId,
            args.SourceNetworkId,
            args.SpellName,
            args.MissileName);
    }

    static void DrawCorridor(
        const Vec3& from,
        const Vec3& to,
        float radius,
        ImU32 color,
        const char* label) {
        const Vec2 delta(to.x - from.x, to.z - from.z);
        const Vec2 direction = delta.Normalized();
        if (direction.IsZero()) {
            return;
        }

        const Vec2 perpendicular(-direction.y, direction.x);
        const Vec3 offset(
            perpendicular.x * radius,
            0.0f,
            perpendicular.y * radius);
        const Vec3 fromLeft = from + offset;
        const Vec3 fromRight = from - offset;
        const Vec3 toLeft = to + offset;
        const Vec3 toRight = to - offset;

        Vec2 screenFrom = {};
        Vec2 screenTo = {};
        Vec2 screenFromLeft = {};
        Vec2 screenFromRight = {};
        Vec2 screenToLeft = {};
        Vec2 screenToRight = {};
        if (!SDK::Drawing::WorldToScreen(from, screenFrom) ||
            !SDK::Drawing::WorldToScreen(to, screenTo) ||
            !SDK::Drawing::WorldToScreen(fromLeft, screenFromLeft) ||
            !SDK::Drawing::WorldToScreen(fromRight, screenFromRight) ||
            !SDK::Drawing::WorldToScreen(toLeft, screenToLeft) ||
            !SDK::Drawing::WorldToScreen(toRight, screenToRight)) {
            return;
        }

        ImVec4 fillVector = ImGui::ColorConvertU32ToFloat4(color);
        fillVector.w = 0.14f;
        const ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(fillVector);
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        draw->AddQuadFilled(
            ImVec2(screenFromLeft.x, screenFromLeft.y),
            ImVec2(screenToLeft.x, screenToLeft.y),
            ImVec2(screenToRight.x, screenToRight.y),
            ImVec2(screenFromRight.x, screenFromRight.y),
            fillColor);
        draw->AddLine(
            ImVec2(screenFromLeft.x, screenFromLeft.y),
            ImVec2(screenToLeft.x, screenToLeft.y),
            color,
            2.0f);
        draw->AddLine(
            ImVec2(screenFromRight.x, screenFromRight.y),
            ImVec2(screenToRight.x, screenToRight.y),
            color,
            2.0f);
        draw->AddLine(
            ImVec2(screenFrom.x, screenFrom.y),
            ImVec2(screenTo.x, screenTo.y),
            color,
            1.5f);

        const float screenRadius =
            screenFromLeft.Distance(screenFromRight) * 0.5f;
        if (screenRadius > 1.0f && screenRadius < 500.0f) {
            draw->AddCircle(
                ImVec2(screenFrom.x, screenFrom.y),
                screenRadius,
                color,
                32,
                2.0f);
        }
        if (label && label[0]) {
            draw->AddText(
                ImVec2(screenFrom.x + 8.0f, screenFrom.y - 18.0f),
                color,
                label);
        }
    }

    void CreateMenu() {
        DestroyMenu();
        m_menu = new Menu(GetInternalId(), GetName(), true);
        auto* settings =
            m_menu->AddSubMenu(new Menu("settings", "Settings"));
        m_enabledMenu =
            settings->Add(new MenuBool("enabled", "Enabled", true));
        m_drawPendingMenu =
            settings->Add(new MenuBool(
                "drawPending",
                "Draw OnProcessSpell pending line",
                true));
        m_writeLogMenu =
            settings->Add(new MenuBool("writeLog", "Write lifecycle log", true));
        m_colorMenu =
            settings->Add(new MenuColor(
                "activeColor",
                "Active missile color",
                0.1f,
                0.85f,
                1.0f,
                0.95f));
        settings->Add(new MenuButton(
            "clearState",
            "Clear tracked missiles",
            "Clear",
            [this]() {
                ResetState();
                Appendf("[EzrealQMissile] state cleared\r\n");
            }));
        settings->Add(new MenuButton(
            "clearLog",
            "Clear lifecycle log",
            "Clear",
            []() { ResetLogFile(); }));
        m_menu->Attach();
    }

    void DestroyMenu() {
        if (!m_menu) {
            return;
        }
        MenuManager::Instance().Remove(m_menu);
        delete m_menu;
        m_menu = nullptr;
        m_enabledMenu = nullptr;
        m_drawPendingMenu = nullptr;
        m_writeLogMenu = nullptr;
        m_colorMenu = nullptr;
    }

    bool Enabled() const {
        return !m_enabledMenu || m_enabledMenu->Value;
    }

    bool DrawPending() const {
        return !m_drawPendingMenu || m_drawPendingMenu->Value;
    }

    bool WriteLog() const {
        return !m_writeLogMenu || m_writeLogMenu->Value;
    }

    ImU32 ActiveColor() const {
        return m_colorMenu
            ? m_colorMenu->GetImU32()
            : IM_COL32(25, 215, 255, 240);
    }

    void ResetState() {
        AcquireSRWLockExclusive(&m_stateLock);
        for (auto& pending : m_pending) {
            pending = {};
        }
        for (auto& active : m_active) {
            active = {};
        }
        m_pendingCursor = 0;
        m_activeCursor = 0;
        ReleaseSRWLockExclusive(&m_stateLock);

        InterlockedExchange64(&m_processCount, 0);
        InterlockedExchange64(&m_createCount, 0);
        InterlockedExchange64(&m_deleteCount, 0);
        InterlockedExchange64(&m_orphanCreateCount, 0);
        InterlockedExchange64(&m_timeoutCount, 0);
        SetLastEvent("none");
    }

    void SetLastEvent(const char* text) {
        if (!text) {
            text = "";
        }
        strncpy_s(m_lastEvent, text, _TRUNCATE);
    }

    static long long ReadCounter(volatile long long& counter) {
        return InterlockedCompareExchange64(&counter, 0, 0);
    }

    static void ResetLogFile() {
        HANDLE file = CreateFileA(
            kLogPath,
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            CloseHandle(file);
        }
    }

    void Appendf(const char* format, ...) const {
        if (!WriteLog() || !format) {
            return;
        }

        char buffer[2048] = {};
        va_list args;
        va_start(args, format);
        const int length =
            std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        if (length <= 0) {
            return;
        }

        HANDLE file = CreateFileA(
            kLogPath,
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return;
        }

        DWORD written = 0;
        WriteFile(
            file,
            buffer,
            static_cast<DWORD>(
                std::min(length, static_cast<int>(sizeof(buffer) - 1))),
            &written,
            nullptr);
        CloseHandle(file);
    }
};

} // namespace Plugins
