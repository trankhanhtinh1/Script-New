#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreObjectManager.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/Events/Events.h"
#include "../../imgui/imgui.h"

#include <Windows.h>
#include <cstdint>

namespace Plugins {

namespace detail {
    struct ObjectLifecycleSample {
        uintptr_t Ptr = 0;
        std::uint32_t NetworkId = 0;
        std::uint32_t Index = 0;
        long long Count = 0;
        long long HookHit = 0;
        DWORD Tick = 0;
    };

    inline void DrawObjectLifecycleSamples(
        const ObjectLifecycleSample* samples,
        int count,
        int cursor) {
        if (!samples || count <= 0) {
            return;
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Recent");
        for (int i = 0; i < count; ++i) {
            const int index = (cursor - 1 - i + count) % count;
            const auto& sample = samples[index];
            if (!sample.Ptr) {
                continue;
            }

            ImGui::Text(
                "#%lld net=%u idx=%u ptr=0x%p hit=%lld tick=%lu",
                sample.Count,
                sample.NetworkId,
                sample.Index,
                reinterpret_cast<void*>(sample.Ptr),
                sample.HookHit,
                static_cast<unsigned long>(sample.Tick));
        }
    }
}

class ObjectCreateLifecycleTestPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Object Create Lifecycle Test"; }
    const char* GetInternalId() const override { return "core.object_create_lifecycle_test"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }

    void OnLoad() override {
        ResetCounters();
        s_instance = this;
        m_subscribed = SDK::Events::AddOnCreateObject(&ObjectCreateLifecycleTestPlugin::OnCreateObject);
        NightSharpDebug::Logf(
            "[ObjectCreateLifecycleTest] loaded subscribed=%d",
            m_subscribed ? 1 : 0);
    }

    void OnUnload() override {
        SDK::Events::RemoveOnCreateObject(&ObjectCreateLifecycleTestPlugin::OnCreateObject);
        if (s_instance == this) {
            s_instance = nullptr;
        }
        m_subscribed = false;
        NightSharpDebug::Logf("[ObjectCreateLifecycleTest] unloaded");
    }

    void OnMenu() override {
        const long long delivered = InterlockedCompareExchange64(&m_delivered, 0, 0);
        const bool installed = Core::Events::IsInstalled(Core::Events::Hooks::OnCreate);
        const long long hookHits = Core::Events::HitCount(Core::Events::Hooks::OnCreate);

        ImGui::Text("Subscribed: %s", m_subscribed ? "yes" : "no");
        ImGui::Text("Hook installed: %s", installed ? "yes" : "no");
        ImGui::Text("Delivered: %lld", delivered);
        ImGui::Text("Raw hook hits: %lld", hookHits);

        detail::ObjectLifecycleSample samples[kMaxSamples] = {};
        int cursor = 0;
        AcquireSRWLockShared(&m_lock);
        for (int i = 0; i < kMaxSamples; ++i) {
            samples[i] = m_samples[i];
        }
        cursor = m_cursor;
        ReleaseSRWLockShared(&m_lock);

        detail::DrawObjectLifecycleSamples(samples, kMaxSamples, cursor);

        if (ImGui::Button("Reset create counters")) {
            ResetCounters();
        }
    }

private:
    static void OnCreateObject(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) {
            s_instance->Record(args);
        }
    }

    void Record(const SDK::Events::ObjectEventArgs& args) {
        if (!args.Sender.IsValid()) {
            return;
        }

        const long long count = InterlockedIncrement64(&m_delivered);
        const detail::ObjectLifecycleSample sample{
            args.Sender.Ptr,
            args.Sender.NetworkId,
            args.Sender.Index,
            count,
            Core::Events::HitCount(Core::Events::Hooks::OnCreate),
            GetTickCount()
        };

        AcquireSRWLockExclusive(&m_lock);
        m_samples[m_cursor % kMaxSamples] = sample;
        m_cursor = (m_cursor + 1) % kMaxSamples;
        ReleaseSRWLockExclusive(&m_lock);
    }

    void ResetCounters() {
        InterlockedExchange64(&m_delivered, 0);
        AcquireSRWLockExclusive(&m_lock);
        for (auto& sample : m_samples) {
            sample = {};
        }
        m_cursor = 0;
        ReleaseSRWLockExclusive(&m_lock);
    }

    static inline ObjectCreateLifecycleTestPlugin* s_instance = nullptr;
    static constexpr int kMaxSamples = 8;
    SRWLOCK m_lock = SRWLOCK_INIT;
    detail::ObjectLifecycleSample m_samples[kMaxSamples] = {};
    int m_cursor = 0;
    volatile LONG64 m_delivered = 0;
    bool m_subscribed = false;
};

class ObjectDeleteLifecycleTestPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Object Delete Lifecycle Test"; }
    const char* GetInternalId() const override { return "core.object_delete_lifecycle_test"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }

    void OnLoad() override {
        ResetCounters();
        s_instance = this;
        m_subscribed = SDK::Events::AddOnDeleteObject(&ObjectDeleteLifecycleTestPlugin::OnDeleteObject);
        NightSharpDebug::Logf(
            "[ObjectDeleteLifecycleTest] loaded subscribed=%d",
            m_subscribed ? 1 : 0);
    }

    void OnUnload() override {
        SDK::Events::RemoveOnDeleteObject(&ObjectDeleteLifecycleTestPlugin::OnDeleteObject);
        if (s_instance == this) {
            s_instance = nullptr;
        }
        m_subscribed = false;
        NightSharpDebug::Logf("[ObjectDeleteLifecycleTest] unloaded");
    }

    void OnUpdate() override {
        TrackLaneClearMinionIfNeeded();
    }

    void OnMenu() override {
        const long long deleteEvents = InterlockedCompareExchange64(&m_deleteEvents, 0, 0);
        const long long cleared = InterlockedCompareExchange64(&m_cleared, 0, 0);

        ImGui::Text("Subscribed: %s", m_subscribed ? "yes" : "no");
        ImGui::Text("Delete events seen: %lld", deleteEvents);
        ImGui::Text("LaneClear clears: %lld", cleared);

        AcquireSRWLockShared(&m_lock);
        const auto laneClear = m_laneClearMinion;
        const auto lastDelete = m_lastDelete;
        ReleaseSRWLockShared(&m_lock);

        ImGui::Separator();
        ImGui::Text(
            "LaneClearMinion: net=%u idx=%u ptr=0x%p",
            laneClear.NetworkId,
            laneClear.Index,
            reinterpret_cast<void*>(laneClear.Ptr));
        ImGui::Text(
            "Last OnDelete: net=%u idx=%u ptr=0x%p tick=%lu",
            lastDelete.NetworkId,
            lastDelete.Index,
            reinterpret_cast<void*>(lastDelete.Ptr),
            static_cast<unsigned long>(lastDelete.Tick));

        if (ImGui::Button("Pick lane-clear minion")) {
            PickLaneClearMinion();
        }
        ImGui::SameLine(0, 8);
        if (ImGui::Button("Clear ref")) {
            ClearLaneClearMinion();
        }
        if (ImGui::Button("Reset delete counters")) {
            ResetCounters();
        }
    }

private:
    static void OnDeleteObject(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) {
            s_instance->Record(args);
        }
    }

    void Record(const SDK::Events::ObjectEventArgs& args) {
        InterlockedIncrement64(&m_deleteEvents);

        AcquireSRWLockExclusive(&m_lock);
        m_lastDelete = {
            args.Sender.Ptr,
            args.Sender.NetworkId,
            args.Sender.Index,
            GetTickCount()
        };
        if (m_laneClearMinion.NetworkId != 0 &&
            args.Sender.NetworkId == m_laneClearMinion.NetworkId) {
            m_laneClearMinion = {};
            InterlockedIncrement64(&m_cleared);
        }
        ReleaseSRWLockExclusive(&m_lock);
    }

    void ResetCounters() {
        InterlockedExchange64(&m_deleteEvents, 0);
        InterlockedExchange64(&m_cleared, 0);
        AcquireSRWLockExclusive(&m_lock);
        m_lastDelete = {};
        ReleaseSRWLockExclusive(&m_lock);
    }

    void ClearLaneClearMinion() {
        AcquireSRWLockExclusive(&m_lock);
        m_laneClearMinion = {};
        ReleaseSRWLockExclusive(&m_lock);
    }

    void TrackLaneClearMinionIfNeeded() {
        const DWORD now = GetTickCount();
        if (now - m_lastPickTick < 300) {
            return;
        }
        m_lastPickTick = now;

        AcquireSRWLockShared(&m_lock);
        const bool hasLaneClearMinion = m_laneClearMinion.NetworkId != 0;
        ReleaseSRWLockShared(&m_lock);
        if (hasLaneClearMinion) {
            return;
        }

        PickLaneClearMinion();
    }

    void PickLaneClearMinion() {
        if (!CoreRuntime::EnsureInitialized()) {
            return;
        }

        const uintptr_t player = CoreRuntime::GetContext().localPlayer;
        if (!Globals::IsValidPtr(player)) {
            return;
        }

        const uint32_t playerTeam = ::Core::Objects::ReadTeamValue(player);
        if (playerTeam == 0) {
            return;
        }

        uintptr_t entries[256] = {};
        const int count = ::Core::ObjectManager::EnumerateMinions(entries, 256);
        for (int i = 0; i < count; ++i) {
            const uintptr_t minion = entries[i];
            if (!Globals::IsValidPtr(minion)) {
                continue;
            }

            const uint32_t networkId = ::Core::Objects::ReadNetworkId(minion);
            if (networkId == 0 || networkId == 0xFFFFFFFFu) {
                continue;
            }

            const uint32_t team = ::Core::Objects::ReadTeamValue(minion);
            if (team == 0 || team == playerTeam) {
                continue;
            }

            const float health =
                Globals::Read<float>(minion + Offset::AttackableUnit::HP);
            if (health <= 0.0f) {
                continue;
            }

            AcquireSRWLockExclusive(&m_lock);
            m_laneClearMinion = {
                minion,
                networkId,
                ::Core::Objects::ReadIndex(minion),
                GetTickCount()
            };
            ReleaseSRWLockExclusive(&m_lock);
            return;
        }
    }

    static inline ObjectDeleteLifecycleTestPlugin* s_instance = nullptr;
    struct TrackedObject {
        uintptr_t Ptr = 0;
        std::uint32_t NetworkId = 0;
        std::uint32_t Index = 0;
        DWORD Tick = 0;
    };

    SRWLOCK m_lock = SRWLOCK_INIT;
    TrackedObject m_laneClearMinion = {};
    TrackedObject m_lastDelete = {};
    volatile LONG64 m_deleteEvents = 0;
    volatile LONG64 m_cleared = 0;
    DWORD m_lastPickTick = 0;
    bool m_subscribed = false;
};

} // namespace Plugins
