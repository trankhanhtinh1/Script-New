#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreRuntime.h"
#include "../../Core/CoreEvents.h"
#include "../../Core/CoreObjectManager.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../SDK/UI/IMenu/Menu.h"

#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace Plugins {

class PlayerEventFilterPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Player Event Filter"; }
    const char* GetInternalId() const override { return "core.player_event_filter"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        s_instance = this;
        ResetCounters();
        ResetLogFile();
        CreateMenu();

        const bool doCast = SDK::Events::hook.OnDoCast += &PlayerEventFilterPlugin::OnDoCast;
        const bool processSpell = SDK::Events::hook.OnProcessSpell += &PlayerEventFilterPlugin::OnProcessSpell;
        const bool playAnimation = SDK::Events::hook.OnPlayAnimation += &PlayerEventFilterPlugin::OnPlayAnimation;
        const bool stopCast = SDK::Events::hook.OnStopCast += &PlayerEventFilterPlugin::OnStopCast;
        const bool newPath = SDK::Events::hook.OnNewPath += &PlayerEventFilterPlugin::OnNewPath;

        const bool buffAdd = SDK::Events::hook.OnBuffAdd += &PlayerEventFilterPlugin::OnBuffAdd;
        const bool buffRemove = SDK::Events::hook.OnBuffRemove += &PlayerEventFilterPlugin::OnBuffRemove;
        const bool buffUpdate = SDK::Events::hook.OnBuffUpdate += &PlayerEventFilterPlugin::OnBuffUpdate;
        const bool objectNew = SDK::Events::hook.OnCreateObject += &PlayerEventFilterPlugin::OnObjectCreate;
        const bool objectDel = SDK::Events::hook.OnDeleteObject += &PlayerEventFilterPlugin::OnObjectDelete;
        const bool missileNew = SDK::Events::hook.OnMissileCreate += &PlayerEventFilterPlugin::OnMissileCreate;
        const bool missileDel = SDK::Events::hook.OnMissileDelete += &PlayerEventFilterPlugin::OnMissileDelete;

        // Damage/FinishCast remain raw probes here because this plugin uses
        // them only as low-level player-involvement diagnostics.
        const bool damage      = ::Core::Events::Add(::Core::Events::Hooks::OnDamage,        &PlayerEventFilterPlugin::OnRawDamage);
        const bool finishCast  = ::Core::Events::Add(::Core::Events::Hooks::OnFinishCast,    &PlayerEventFilterPlugin::OnRawFinishCast);

        Appendf(
            "[PlayerEventFilter] loaded doCast=%d processSpell=%d playAnimation=%d stopCast=%d "
            "newPath=%d buffAdd=%d buffRemove=%d buffUpdate=%d objectNew=%d objectDel=%d "
            "missileNew=%d missileDel=%d damage=%d finishCast=%d\r\n",
            doCast ? 1 : 0,
            processSpell ? 1 : 0,
            playAnimation ? 1 : 0,
            stopCast ? 1 : 0,
            newPath ? 1 : 0,
            buffAdd ? 1 : 0,
            buffRemove ? 1 : 0,
            buffUpdate ? 1 : 0,
            objectNew ? 1 : 0,
            objectDel ? 1 : 0,
            missileNew ? 1 : 0,
            missileDel ? 1 : 0,
            damage ? 1 : 0,
            finishCast ? 1 : 0);
        NightSharpDebug::Logf("[PlayerEventFilter] loaded");
    }

    void OnUnload() override {
        ::Core::Events::Remove(::Core::Events::Hooks::OnFinishCast,    &PlayerEventFilterPlugin::OnRawFinishCast);
        ::Core::Events::Remove(::Core::Events::Hooks::OnDamage,        &PlayerEventFilterPlugin::OnRawDamage);
        SDK::Events::hook.OnMissileDelete -= &PlayerEventFilterPlugin::OnMissileDelete;
        SDK::Events::hook.OnMissileCreate -= &PlayerEventFilterPlugin::OnMissileCreate;
        SDK::Events::hook.OnDeleteObject -= &PlayerEventFilterPlugin::OnObjectDelete;
        SDK::Events::hook.OnCreateObject -= &PlayerEventFilterPlugin::OnObjectCreate;
        SDK::Events::hook.OnBuffUpdate -= &PlayerEventFilterPlugin::OnBuffUpdate;
        SDK::Events::hook.OnBuffRemove -= &PlayerEventFilterPlugin::OnBuffRemove;
        SDK::Events::hook.OnBuffAdd -= &PlayerEventFilterPlugin::OnBuffAdd;

        SDK::Events::hook.OnNewPath -= &PlayerEventFilterPlugin::OnNewPath;
        SDK::Events::hook.OnStopCast -= &PlayerEventFilterPlugin::OnStopCast;
        SDK::Events::hook.OnPlayAnimation -= &PlayerEventFilterPlugin::OnPlayAnimation;
        SDK::Events::hook.OnProcessSpell -= &PlayerEventFilterPlugin::OnProcessSpell;
        SDK::Events::hook.OnDoCast -= &PlayerEventFilterPlugin::OnDoCast;

        Appendf("[PlayerEventFilter] unloaded\r\n");
        if (s_instance == this) {
            s_instance = nullptr;
        }
        DestroyMenu();
        NightSharpDebug::Logf("[PlayerEventFilter] unloaded");
    }

    void OnMenu() override {
        if (!m_menu) {
            return;
        }

        m_menu->DrawImGui();
        ImGui::Separator();
        ImGui::Text("SDK received / local player");
        ImGui::Text("OnDoCast: %lld / %lld",
                    ReadCounter(m_doCastReceived),
                    ReadCounter(m_doCastLocal));
        ImGui::Text("OnProcessSpell: %lld / %lld",
                    ReadCounter(m_processSpellReceived),
                    ReadCounter(m_processSpellLocal));
        ImGui::Text("OnPlayAnimation: %lld / %lld",
                    ReadCounter(m_playAnimationReceived),
                    ReadCounter(m_playAnimationLocal));
        ImGui::Text("OnStopCast: %lld / %lld",
                    ReadCounter(m_stopCastReceived),
                    ReadCounter(m_stopCastLocal));
        ImGui::Text("OnNewPath: %lld / %lld",
                    ReadCounter(m_newPathReceived),
                    ReadCounter(m_newPathLocal));
        ImGui::Separator();
        ImGui::Text("Raw probes (received / player-involved)");
        ImGui::Text("OnBuffAdd:       %lld / %lld",
                    ReadCounter(m_buffAddReceived),    ReadCounter(m_buffAddLocal));
        ImGui::Text("OnBuffRemove:    %lld / %lld",
                    ReadCounter(m_buffRemoveReceived), ReadCounter(m_buffRemoveLocal));
        ImGui::Text("OnBuffUpdate:    %lld / %lld",
                    ReadCounter(m_buffUpdateReceived), ReadCounter(m_buffUpdateLocal));
        ImGui::Text("OnCreateObject:  %lld / %lld",
                    ReadCounter(m_objectNewReceived), ReadCounter(m_objectNewLocal));
        ImGui::Text("OnDeleteObject:  %lld / %lld",
                    ReadCounter(m_objectDelReceived), ReadCounter(m_objectDelLocal));
        ImGui::Text("OnMissileCreate: %lld / %lld",
                    ReadCounter(m_missileNewReceived), ReadCounter(m_missileNewLocal));
        ImGui::Text("OnMissileDelete: %lld / %lld",
                    ReadCounter(m_missileDelReceived), ReadCounter(m_missileDelLocal));
        ImGui::Text("OnDamage:        %lld / %lld",
                    ReadCounter(m_damageReceived),    ReadCounter(m_damageLocal));
        ImGui::Text("OnFinishCast:    %lld / %lld",
                    ReadCounter(m_finishCastReceived),ReadCounter(m_finishCastLocal));
        ImGui::Separator();
        DrawRuneManagerDebug();
        ImGui::Text("Log: %s", kLogPath);
    }

private:
    static inline PlayerEventFilterPlugin* s_instance = nullptr;
    static constexpr const char* kLogPath =
        "C:\\Users\\Public\\nightsharp_player_events.txt";

    Menu* m_menu = nullptr;
    MenuBool* m_enabledMenu = nullptr;
    MenuBool* m_writeLogMenu = nullptr;

    volatile long long m_doCastReceived = 0;
    volatile long long m_doCastLocal = 0;
    volatile long long m_processSpellReceived = 0;
    volatile long long m_processSpellLocal = 0;
    volatile long long m_playAnimationReceived = 0;
    volatile long long m_playAnimationLocal = 0;
    volatile long long m_stopCastReceived = 0;
    volatile long long m_stopCastLocal = 0;
    volatile long long m_newPathReceived = 0;
    volatile long long m_newPathLocal = 0;

    // Raw-probe counters. "local" = candidate sender/target pointers resolved
    // to the local player object on the hook's calling thread.
    volatile long long m_buffAddReceived = 0;
    volatile long long m_buffAddLocal = 0;
    volatile long long m_buffRemoveReceived = 0;
    volatile long long m_buffRemoveLocal = 0;
    volatile long long m_buffUpdateReceived = 0;
    volatile long long m_buffUpdateLocal = 0;
    volatile long long m_objectNewReceived = 0;
    volatile long long m_objectNewLocal = 0;
    volatile long long m_objectDelReceived = 0;
    volatile long long m_objectDelLocal = 0;
    volatile long long m_missileNewReceived = 0;
    volatile long long m_missileNewLocal = 0;
    volatile long long m_missileDelReceived = 0;
    volatile long long m_missileDelLocal = 0;
    volatile long long m_damageReceived = 0;
    volatile long long m_damageLocal = 0;
    volatile long long m_finishCastReceived = 0;
    volatile long long m_finishCastLocal = 0;

    // Per-hook log cap so an early-game spam of e.g. minion buffs doesn't
    // flood the file. Once a hook exceeds the cap it switches to counter-only.
    static constexpr long long kRawLogCapPerHook = 200;

    void CreateMenu() {
        DestroyMenu();

        m_menu = new Menu(GetInternalId(), "Player Event Filter", true);
        auto* settings = m_menu->AddSubMenu(new Menu("settings", "Settings"));
        m_enabledMenu = settings->Add(new MenuBool("enabled", "Enabled", true));
        m_writeLogMenu = settings->Add(new MenuBool("writeLog", "Write player event log", false));
        settings->Add(new MenuButton(
            "resetCounters",
            "Reset counters",
            "Reset",
            [this]() {
                ResetCounters();
                Appendf("[PlayerEventFilter] counters reset\r\n");
            }));
        settings->Add(new MenuButton(
            "clearLog",
            "Clear log",
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
        m_writeLogMenu = nullptr;
    }

    bool Enabled() const {
        return !m_enabledMenu || m_enabledMenu->Value;
    }

    bool WriteLog() const {
        return m_writeLogMenu && m_writeLogMenu->Value;
    }

    static void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
        if (s_instance) {
            s_instance->HandleSpellEvent(
                "OnDoCast",
                args,
                s_instance->m_doCastReceived,
                s_instance->m_doCastLocal);
        }
    }

    static void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
        if (s_instance) {
            s_instance->HandleSpellEvent(
                "OnProcessSpell",
                args,
                s_instance->m_processSpellReceived,
                s_instance->m_processSpellLocal);
        }
    }

    static void OnPlayAnimation(const SDK::Events::PlayAnimationEventArgs& args) {
        if (!s_instance) {
            return;
        }

        const long long received =
            InterlockedIncrement64(&s_instance->m_playAnimationReceived);
        if (!s_instance->Enabled() || !SDK::Events::IsLocalPlayer(args.Sender)) {
            s_instance->LogRejected("OnPlayAnimation", args.Sender, args.Raw, received);
            return;
        }

        InterlockedIncrement64(&s_instance->m_playAnimationLocal);
        if (s_instance->WriteLog()) {
            Appendf(
                "[PlayerEventFilter] OnPlayAnimation tick=%d hit=%lld player=0x%llX net=%u name='%s' animation='%s' id=%d accepted=%d\r\n",
                SDK::Game::TickCount(),
                args.Raw.HitCount,
                static_cast<unsigned long long>(args.Sender.Ptr),
                args.Sender.NetworkId,
                args.Sender.CharacterName,
                args.Animation,
                args.AnimationId,
                args.Accepted ? 1 : 0);
        }
    }

    static void OnStopCast(const SDK::Events::StopCastEventArgs& args) {
        if (!s_instance) {
            return;
        }

        const long long received =
            InterlockedIncrement64(&s_instance->m_stopCastReceived);
        if (!s_instance->Enabled() || !SDK::Events::IsLocalPlayer(args.Sender)) {
            s_instance->LogRejected("OnStopCast", args.Sender, args.Raw, received);
            return;
        }

        InterlockedIncrement64(&s_instance->m_stopCastLocal);
        if (s_instance->WriteLog()) {
            Appendf(
                "[PlayerEventFilter] OnStopCast tick=%d hit=%lld player=0x%llX net=%u name='%s' slot=%d force=%d spellbook=0x%llX process=0x%llX\r\n",
                SDK::Game::TickCount(),
                args.Raw.HitCount,
                static_cast<unsigned long long>(args.Sender.Ptr),
                args.Sender.NetworkId,
                args.Sender.CharacterName,
                args.Slot,
                args.ForceStop ? 1 : 0,
                static_cast<unsigned long long>(args.Spellbook),
                static_cast<unsigned long long>(args.ProcessFlag));
        }
    }

    static void OnNewPath(const SDK::Events::NewPathEventArgs& args) {
        if (!s_instance) {
            return;
        }

        const long long received =
            InterlockedIncrement64(&s_instance->m_newPathReceived);
        if (!s_instance->Enabled() || !SDK::Events::IsLocalPlayer(args.Sender)) {
            s_instance->LogRejected("OnNewPath", args.Sender, args.Raw, received);
            return;
        }

        InterlockedIncrement64(&s_instance->m_newPathLocal);
        if (!s_instance->WriteLog()) {
            return;
        }

        char pathText[1200] = {};
        std::size_t used = 0;
        for (int i = 0; i < args.PathCount && used + 48 < sizeof(pathText); ++i) {
            const int written = std::snprintf(
                pathText + used,
                sizeof(pathText) - used,
                "%s(%.1f %.1f %.1f)",
                i ? " -> " : "",
                args.Path[i].x,
                args.Path[i].y,
                args.Path[i].z);
            if (written <= 0) {
                break;
            }
            used += static_cast<std::size_t>(written);
        }

        Appendf(
            "[PlayerEventFilter] OnNewPath tick=%d hit=%lld player=0x%llX net=%u name='%s' "
            "count=%d dash=%d speed=%.1f stack0=0x%llX path=%s\r\n",
            SDK::Game::TickCount(),
            args.Raw.HitCount,
            static_cast<unsigned long long>(args.Sender.Ptr),
            args.Sender.NetworkId,
            args.Sender.CharacterName,
            args.PathCount,
            args.IsDash ? 1 : 0,
            args.Speed,
            static_cast<unsigned long long>(args.Raw.Stack0),
            pathText);
    }

    void HandleSpellEvent(const char* eventName,
                          const SDK::Events::ProcessSpellEventArgs& args,
                          volatile long long& receivedCounter,
                          volatile long long& localCounter) {
        const long long received = InterlockedIncrement64(&receivedCounter);
        if (!Enabled() || !SDK::Events::IsLocalPlayer(args.Sender)) {
            LogRejected(eventName, args.Sender, args.Raw, received);
            return;
        }

        InterlockedIncrement64(&localCounter);
        if (!WriteLog()) {
            return;
        }

        Appendf(
            "[PlayerEventFilter] %s tick=%d hit=%lld player=0x%llX net=%u name='%s' slot=%d spell='%s' missile='%s' script='%s' targetNet=%u start=%.1f %.1f %.1f end=%.1f %.1f %.1f\r\n",
            eventName ? eventName : "?",
            SDK::Game::TickCount(),
            args.Raw.HitCount,
            static_cast<unsigned long long>(args.Sender.Ptr),
            args.Sender.NetworkId,
            args.Sender.CharacterName,
            args.Slot,
            args.SpellName,
            args.MissileName,
            args.ScriptName,
            args.TargetNetworkId,
            args.StartPosition.x,
            args.StartPosition.y,
            args.StartPosition.z,
            args.EndPosition.x,
            args.EndPosition.y,
            args.EndPosition.z);
    }

    void ResetCounters() {
        InterlockedExchange64(&m_doCastReceived, 0);
        InterlockedExchange64(&m_doCastLocal, 0);
        InterlockedExchange64(&m_processSpellReceived, 0);
        InterlockedExchange64(&m_processSpellLocal, 0);
        InterlockedExchange64(&m_playAnimationReceived, 0);
        InterlockedExchange64(&m_playAnimationLocal, 0);
        InterlockedExchange64(&m_stopCastReceived, 0);
        InterlockedExchange64(&m_stopCastLocal, 0);
        InterlockedExchange64(&m_newPathReceived, 0);
        InterlockedExchange64(&m_newPathLocal, 0);
        InterlockedExchange64(&m_buffAddReceived, 0);
        InterlockedExchange64(&m_buffAddLocal, 0);
        InterlockedExchange64(&m_buffRemoveReceived, 0);
        InterlockedExchange64(&m_buffRemoveLocal, 0);
        InterlockedExchange64(&m_buffUpdateReceived, 0);
        InterlockedExchange64(&m_buffUpdateLocal, 0);
        InterlockedExchange64(&m_objectNewReceived, 0);
        InterlockedExchange64(&m_objectNewLocal, 0);
        InterlockedExchange64(&m_objectDelReceived, 0);
        InterlockedExchange64(&m_objectDelLocal, 0);
        InterlockedExchange64(&m_missileNewReceived, 0);
        InterlockedExchange64(&m_missileNewLocal, 0);
        InterlockedExchange64(&m_missileDelReceived, 0);
        InterlockedExchange64(&m_missileDelLocal, 0);
        InterlockedExchange64(&m_damageReceived, 0);
        InterlockedExchange64(&m_damageLocal, 0);
        InterlockedExchange64(&m_finishCastReceived, 0);
        InterlockedExchange64(&m_finishCastLocal, 0);
    }

    // ------------------------------------------------------------------
    // Raw probe support
    // ------------------------------------------------------------------
    //
    // These probes do NOT assume any struct layout. They take the raw
    // RCX/RDX/R8/R9/HitCount the central CoreHook dispatcher captures,
    // try to recognise the local player anywhere in those four
    // registers, and log the result. The point is to harvest real
    // pointer values from a live game so we can confirm the actual
    // argument layout of each hook before promoting it to a typed
    // decoder in Core/CoreEvents.h.
    //
    // Two helpers are used:
    //   - LooksLikeObject(addr): same shape check the spell decoders
    //     use (valid NetId + valid Position).
    //   - PlayerHit(...): records which register matched the local
    //     player (rcx / rdx / r8 / r9 / *(rcx+8) / *(rdx) / none),
    //     plus the resolved network id, so we can tell whether the
    //     hook puts the unit in RCX, the buff/damage struct in RDX,
    //     or follows a wrapper pattern like OnPlayAnimationWrapper.

    struct PlayerProbe {
        const char* slot = "none"; // which register/dereference matched
        uintptr_t object = 0;       // resolved unit pointer
        uint32_t  netId = 0;
        char      name[64] = {};
    };

    static bool TryFillFromObject(uintptr_t candidate, PlayerProbe& probe, const char* slot) {
        if (!candidate) {
            return false;
        }
        if (!::Core::Events::detail::LooksLikeObject(candidate)) {
            return false;
        }
        const auto info = ::Core::Events::detail::ReadObject(candidate);
        probe.slot = slot;
        probe.object = info.Ptr;
        probe.netId = info.NetworkId;
        // Hand-rolled bounded copy to avoid C4996 on strncpy under SDLCheck.
        const size_t cap = sizeof(probe.name) - 1;
        size_t i = 0;
        for (; i < cap && info.CharacterName[i] != 0; ++i) {
            probe.name[i] = info.CharacterName[i];
        }
        probe.name[i] = 0;
        return true;
    }

    // Look for the local player anywhere in the raw register payload.
    // We accept matches in RCX / RDX / R8 / R9 directly, plus the
    // common "wrapper pointer + 0x08" pattern used by some hooks
    // (OnPlayAnimationWrapper is the canonical example).
    static PlayerProbe ProbePlayer(const ::Core::Events::RawEventArgs& raw) {
        PlayerProbe probe{};
        const uintptr_t player = ::Core::ObjectManager::PlayerAddress();
        if (!player) {
            return probe;
        }

        auto matches = [&](uintptr_t candidate) -> bool {
            if (!candidate) return false;
            if (candidate == player) return true;
            uint32_t netId = 0;
            __try {
                netId = *reinterpret_cast<const uint32_t*>(
                    candidate + ::Offset::All::NetId);
            } __except (1) { return false; }
            if (netId == 0 || netId == 0xFFFFFFFFu) return false;
            const uint32_t playerNet =
                ::Core::Objects::ReadNetworkId(player);
            return playerNet != 0 && netId == playerNet;
        };

        // Direct register slots
        if (matches(raw.Rcx) && TryFillFromObject(raw.Rcx, probe, "rcx")) return probe;
        if (matches(static_cast<uintptr_t>(raw.Rdx)) &&
            TryFillFromObject(static_cast<uintptr_t>(raw.Rdx), probe, "rdx")) return probe;
        if (matches(raw.R8) && TryFillFromObject(raw.R8, probe, "r8")) return probe;
        if (matches(raw.R9) && TryFillFromObject(raw.R9, probe, "r9")) return probe;

        // Single-indirection patterns: *(rcx+0), *(rcx+8), *(rdx+0), *(rdx+8)
        auto tryIndirect = [&](uintptr_t base, ptrdiff_t off, const char* tag) -> bool {
            if (!base) return false;
            uintptr_t deref = 0;
            __try {
                deref = *reinterpret_cast<const uintptr_t*>(base + off);
            } __except (1) { return false; }
            return matches(deref) && TryFillFromObject(deref, probe, tag);
        };
        if (tryIndirect(raw.Rcx, 0x00, "*(rcx+0)")) return probe;
        if (tryIndirect(raw.Rcx, 0x08, "*(rcx+8)")) return probe;
        if (tryIndirect(static_cast<uintptr_t>(raw.Rdx), 0x00, "*(rdx+0)")) return probe;
        if (tryIndirect(static_cast<uintptr_t>(raw.Rdx), 0x08, "*(rdx+8)")) return probe;
        if (tryIndirect(raw.R8,  0x00, "*(r8+0)")) return probe;
        if (tryIndirect(raw.R8,  0x08, "*(r8+8)")) return probe;

        return probe;
    }

    void HandleRawProbe(const char* eventName,
                        const ::Core::Events::RawEventArgs& raw,
                        volatile long long& receivedCounter,
                        volatile long long& localCounter) {
        const long long received = InterlockedIncrement64(&receivedCounter);
        if (!Enabled()) {
            return;
        }

        const PlayerProbe probe = ProbePlayer(raw);
        const bool playerInvolved = probe.object != 0;
        if (playerInvolved) {
            InterlockedIncrement64(&localCounter);
        }

        if (!WriteLog()) {
            return;
        }
        // Cap per-hook line count so e.g. minion buff churn doesn't drown
        // the file. We still bump the counters above the cap.
        const long long logged = playerInvolved
            ? ReadCounter(localCounter)
            : received;
        if (logged > kRawLogCapPerHook) {
            return;
        }

        Appendf(
            "[PlayerEventFilter] %s received=%lld playerHit=%d via=%s playerNet=%u playerName='%s' "
            "rcx=0x%llX rdx=0x%llX r8=0x%llX r9=0x%llX xmm0=%.3f xmm1=%.3f xmm2=%.3f xmm3=%.3f hookHit=%lld\r\n",
            eventName ? eventName : "?",
            received,
            playerInvolved ? 1 : 0,
            probe.slot,
            probe.netId,
            probe.name,
            static_cast<unsigned long long>(raw.Rcx),
            static_cast<unsigned long long>(raw.Rdx),
            static_cast<unsigned long long>(raw.R8),
            static_cast<unsigned long long>(raw.R9),
            raw.Xmm0,
            raw.Xmm1,
            raw.Xmm2,
            raw.Xmm3,
            raw.HitCount);
    }

    void HandleBuffEvent(const char* eventName,
                         const SDK::Events::BuffEventArgs& args,
                         volatile long long& receivedCounter,
                         volatile long long& localCounter) {
        const long long received = InterlockedIncrement64(&receivedCounter);
        if (!Enabled()) {
            return;
        }

        const bool local = SDK::Events::IsLocalPlayer(args.Sender);
        if (local) {
            InterlockedIncrement64(&localCounter);
        }
        if (!WriteLog()) {
            return;
        }

        const long long logged = local ? ReadCounter(localCounter) : received;
        if (logged > kRawLogCapPerHook) {
            return;
        }

        Appendf(
            "[PlayerEventFilter] %s received=%lld playerHit=%d player=0x%llX net=%u name='%s' "
            "buff='%s' count=%d bridge=0x%llX ownerComponent=0x%llX "
            "rawRcx=0x%llX rawRdx=0x%llX rawR8=0x%llX rawR9=0x%llX hookHit=%lld\r\n",
            eventName ? eventName : "?",
            received,
            local ? 1 : 0,
            static_cast<unsigned long long>(args.Sender.Ptr),
            args.Sender.NetworkId,
            args.Sender.CharacterName,
            args.BuffName,
            args.Count,
            static_cast<unsigned long long>(args.EventBridge),
            static_cast<unsigned long long>(args.OwnerComponent),
            static_cast<unsigned long long>(args.Raw.Rcx),
            static_cast<unsigned long long>(args.Raw.Rdx),
            static_cast<unsigned long long>(args.Raw.R8),
            static_cast<unsigned long long>(args.Raw.R9),
            args.Raw.HitCount);
    }

    void DrawRuneManagerDebug() const {
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            ImGui::Text("RuneManager: player unavailable");
            return;
        }

        const uintptr_t heroAddr = player.Address();
        ImGui::Text("AIHeroClient: 0x%llX", static_cast<unsigned long long>(heroAddr));

        const uintptr_t rawFieldValue = Globals::Read<uintptr_t>(
            heroAddr + Offset::AIHeroClient::RuneManager);
        ImGui::Text("Field @ +0x%X = 0x%llX",
                    Offset::AIHeroClient::RuneManager,
                    static_cast<unsigned long long>(rawFieldValue));

        const uintptr_t resolved = ::CoreRuneManager::Resolve(heroAddr);
        ImGui::Text("Resolve: 0x%llX",
                    static_cast<unsigned long long>(resolved));

        const auto manager = player.RuneManager();
        const auto snapshot = manager.Snapshot();
        ImGui::Text("Manager: 0x%llX primary=%d secondary=%d entries=%d",
                    static_cast<unsigned long long>(manager.Address()),
                    snapshot.primaryTree.id,
                    snapshot.secondaryTree.id,
                    snapshot.entryCount);

        if (manager.IsValid()) {
            for (int index = 0; index < snapshot.entryCount && index < 8; ++index) {
                const auto& entry = snapshot.entries[index];
                ImGui::Text("  [%d] id=%d name=%s",
                            index,
                            entry.data.id,
                            entry.data.displayName[0] ? entry.data.displayName : "?");
            }
            ImGui::Text("Primary tree: id=%d name=%s desc=%s",
                        snapshot.primaryTree.id,
                        snapshot.primaryTree.displayName[0] ? snapshot.primaryTree.displayName : "?",
                        snapshot.primaryTree.description[0] ? snapshot.primaryTree.description : "?");
            ImGui::Text("Secondary tree: id=%d name=%s desc=%s",
                        snapshot.secondaryTree.id,
                        snapshot.secondaryTree.displayName[0] ? snapshot.secondaryTree.displayName : "?",
                        snapshot.secondaryTree.description[0] ? snapshot.secondaryTree.description : "?");
        } else {
            ImGui::TextColored(ImVec4(1,0.6f,0,1), "Manager not valid at resolved address");
        }
    }

    void HandleObjectEvent(const char* eventName,
                           const SDK::Events::ObjectEventArgs& args,
                           volatile long long& receivedCounter,
                           volatile long long& localCounter) {
        const long long received = InterlockedIncrement64(&receivedCounter);
        if (!Enabled()) {
            return;
        }

        const bool local = SDK::Events::IsLocalPlayer(args.Sender);
        if (local) {
            InterlockedIncrement64(&localCounter);
        }
        if (!WriteLog()) {
            return;
        }

        const long long logged = local ? ReadCounter(localCounter) : received;
        if (logged > kRawLogCapPerHook) {
            return;
        }

        Appendf(
            "[PlayerEventFilter] %s received=%lld playerHit=%d object=0x%llX net=%u index=%u type=%d name='%s' char='%s' rawRcx=0x%llX rawRdx=0x%llX rawR8=0x%llX rawR9=0x%llX hookHit=%lld\r\n",
            eventName ? eventName : "?",
            received,
            local ? 1 : 0,
            static_cast<unsigned long long>(args.Sender.Ptr),
            args.Sender.NetworkId,
            args.Sender.Index,
            static_cast<int>(args.Sender.Type),
            args.Sender.Name,
            args.Sender.CharacterName,
            static_cast<unsigned long long>(args.Raw.Rcx),
            static_cast<unsigned long long>(args.Raw.Rdx),
            static_cast<unsigned long long>(args.Raw.R8),
            static_cast<unsigned long long>(args.Raw.R9),
            args.Raw.HitCount);
    }

    void HandleMissileEvent(const char* eventName,
                            const SDK::Events::ObjectEventArgs& args,
                            volatile long long& receivedCounter,
                            volatile long long& localCounter) {
        const long long received = InterlockedIncrement64(&receivedCounter);
        if (!Enabled()) {
            return;
        }

        const bool local = SDK::Events::IsLocalPlayer(args.Source);
        if (local) {
            InterlockedIncrement64(&localCounter);
        }
        if (!WriteLog()) {
            return;
        }

        const long long logged = local ? ReadCounter(localCounter) : received;
        if (logged > kRawLogCapPerHook) {
            return;
        }

        Appendf(
            "[PlayerEventFilter] %s received=%lld playerHit=%d missile=0x%llX missileNet=%u "
            "source=0x%llX sourceIndex=%u sourceNet=%u sourceName='%s' "
            "spell='%s' missileName='%s' start=%.1f %.1f %.1f end=%.1f %.1f %.1f "
            "rawRdx=0x%llX hookHit=%lld\r\n",
            eventName ? eventName : "?",
            received,
            local ? 1 : 0,
            static_cast<unsigned long long>(args.Sender.Ptr),
            args.MissileNetworkId,
            static_cast<unsigned long long>(args.Source.Ptr),
            args.SourceIndex,
            args.SourceNetworkId,
            args.Source.CharacterName,
            args.SpellName,
            args.MissileName,
            args.StartPosition.x,
            args.StartPosition.y,
            args.StartPosition.z,
            args.EndPosition.x,
            args.EndPosition.y,
            args.EndPosition.z,
            static_cast<unsigned long long>(args.Raw.Rdx),
            args.Raw.HitCount);
    }

    static void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
        if (s_instance) s_instance->HandleBuffEvent(
            "OnBuffAdd", args, s_instance->m_buffAddReceived, s_instance->m_buffAddLocal);
    }
    static void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
        if (s_instance) s_instance->HandleBuffEvent(
            "OnBuffRemove", args, s_instance->m_buffRemoveReceived, s_instance->m_buffRemoveLocal);
    }
    static void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
        if (s_instance) s_instance->HandleBuffEvent(
            "OnBuffUpdate", args, s_instance->m_buffUpdateReceived, s_instance->m_buffUpdateLocal);
    }
    static void OnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) s_instance->HandleObjectEvent(
            "OnCreateObject", args, s_instance->m_objectNewReceived, s_instance->m_objectNewLocal);
    }
    static void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) s_instance->HandleObjectEvent(
            "OnDeleteObject", args, s_instance->m_objectDelReceived, s_instance->m_objectDelLocal);
    }
    static void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) s_instance->HandleMissileEvent(
            "OnMissileCreate", args, s_instance->m_missileNewReceived, s_instance->m_missileNewLocal);
    }
    static void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) s_instance->HandleMissileEvent(
            "OnMissileDelete", args, s_instance->m_missileDelReceived, s_instance->m_missileDelLocal);
    }
    static void OnRawDamage(const ::Core::Events::RawEventArgs& raw) {
        if (!SDK::Events::IsDeliveryEnabled()) {
            return;
        }
        if (s_instance) s_instance->HandleRawProbe(
            "OnDamage", raw, s_instance->m_damageReceived, s_instance->m_damageLocal);
    }
    static void OnRawFinishCast(const ::Core::Events::RawEventArgs& raw) {
        if (!SDK::Events::IsDeliveryEnabled()) {
            return;
        }
        if (s_instance) s_instance->HandleRawProbe(
            "OnFinishCast", raw, s_instance->m_finishCastReceived, s_instance->m_finishCastLocal);
    }

    static long long ReadCounter(volatile long long& counter) {
        return InterlockedCompareExchange64(&counter, 0, 0);
    }

    void LogRejected(const char* eventName,
                     const ::Core::Events::ObjectInfo& sender,
                     const ::Core::Events::RawEventArgs& raw,
                     long long received) const {
        if (!WriteLog() || received > 40) {
            return;
        }

        const auto player = SDK::ObjectManager::Player();
        Appendf(
            "[PlayerEventFilter] rejected event=%s received=%lld hookHit=%lld sender=0x%llX senderNet=%u senderName='%s' player=0x%llX playerNet=%d rawRcx=0x%llX rawRdx=0x%llX rawR8=0x%llX rawR9=0x%llX\r\n",
            eventName ? eventName : "?",
            received,
            raw.HitCount,
            static_cast<unsigned long long>(sender.Ptr),
            sender.NetworkId,
            sender.CharacterName,
            static_cast<unsigned long long>(player.Address()),
            player.NetworkId(),
            static_cast<unsigned long long>(raw.Rcx),
            static_cast<unsigned long long>(raw.Rdx),
            static_cast<unsigned long long>(raw.R8),
            static_cast<unsigned long long>(raw.R9));
    }

    static void ResetLogFile() {
        HANDLE hFile = CreateFileA(
            kLogPath,
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
        }
    }

    static void Appendf(const char* format, ...) {
        if (!format) {
            return;
        }

        char line[2048] = {};
        va_list args;
        va_start(args, format);
        std::vsnprintf(line, sizeof(line), format, args);
        va_end(args);

        HANDLE hFile = CreateFileA(
            kLogPath,
            FILE_APPEND_DATA,
            FILE_SHARE_READ,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            return;
        }

        DWORD written = 0;
        WriteFile(
            hFile,
            line,
            static_cast<DWORD>(std::strlen(line)),
            &written,
            nullptr);
        CloseHandle(hFile);
    }
};

} // namespace Plugins
