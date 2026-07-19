#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreCastSpell.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../SDK/UI/IMenu/Menu.h"

#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <limits>

namespace Plugins {

class CastSpellTestPluginBase : public IPlugin {
public:
    PluginCategory GetCategory() const override {
        return PluginCategory::Champion;
    }

    bool AutoLoadByDefault() const override {
        return false;
    }

    void OnLoad() override {
        s_active = this;
        ResetCounters();
        ResetLogFile();
        CreateMenu();

        const bool update =
            SDK::Events::hook.OnGameUpdate += &CastSpellTestPluginBase::OnGameUpdate;
        const bool doCast =
            SDK::Events::hook.OnDoCast += &CastSpellTestPluginBase::OnDoCast;
        const bool processSpell =
            SDK::Events::hook.OnProcessSpell += &CastSpellTestPluginBase::OnProcessSpell;
        const bool stopCast =
            SDK::Events::hook.OnStopCast += &CastSpellTestPluginBase::OnStopCast;

        Appendf(
            "[%s] loaded update=%d doCast=%d processSpell=%d stopCast=%d player=0x%llX champion='%s'\r\n",
            DebugPrefix(),
            update ? 1 : 0,
            doCast ? 1 : 0,
            processSpell ? 1 : 0,
            stopCast ? 1 : 0,
            static_cast<unsigned long long>(CoreRuntime::GetContext().localPlayer),
            SDK::ObjectManager::Player().CharacterName().c_str());
        NightSharpDebug::Logf("[%s] loaded", DebugPrefix());
    }

    void OnUnload() override {
        SDK::Events::hook.OnStopCast -= &CastSpellTestPluginBase::OnStopCast;
        SDK::Events::hook.OnProcessSpell -= &CastSpellTestPluginBase::OnProcessSpell;
        SDK::Events::hook.OnDoCast -= &CastSpellTestPluginBase::OnDoCast;
        SDK::Events::hook.OnGameUpdate -= &CastSpellTestPluginBase::OnGameUpdate;

        OnChampionUnload();
        Appendf("[%s] unloaded\r\n", DebugPrefix());
        DestroyMenu();

        if (s_active == this) {
            s_active = nullptr;
        }
        NightSharpDebug::Logf("[%s] unloaded", DebugPrefix());
    }

    void OnMenu() override {
        if (!m_menu) {
            return;
        }

        m_menu->DrawImGui();
        ImGui::Separator();
        ImGui::Text("Attempts / success: %lld / %lld",
                    ReadCounter(m_attempts),
                    ReadCounter(m_successes));
        ImGui::Text("OnDoCast local: %lld", ReadCounter(m_doCastLocal));
        ImGui::Text("OnProcessSpell local: %lld", ReadCounter(m_processSpellLocal));
        ImGui::Text("OnStopCast local: %lld", ReadCounter(m_stopCastLocal));
        ImGui::Text("Last kind: %s", CoreCastSpell::CastKindName(m_lastTrace.kind));
        ImGui::Text("Last result: %s",
                    CoreCastSpell::CastFailureName(m_lastTrace.failure));
        ImGui::Text("Last CanCast: %s",
                    m_lastTrace.canCastAccepted ? "accepted" : "rejected/not-run");
        DrawChampionDebug();
        ImGui::Text("Log: %s", LogPath());
    }

protected:
    virtual const char* DebugPrefix() const = 0;
    virtual const char* LogPath() const = 0;
    virtual void BuildChampionMenu(Menu* settings) = 0;
    virtual void HandleGameUpdate(const SDK::Events::GameUpdateEventArgs& args) = 0;
    virtual void DrawChampionDebug() {}
    virtual void OnChampionUnload() {}

    static bool CanLoadChampion(const char* championName) {
        if (!championName || !championName[0]) {
            return false;
        }

        // Primary: use the session-level cache populated by WarmPlayerTeamCache().
        const std::string& cached = SDK::GameObject::GetCachedChampionName();
        if (!cached.empty()) {
            return _stricmp(cached.c_str(), championName) == 0;
        }

        static char s_cachedChampionName[64] = {};
        if (s_cachedChampionName[0]) {
            return _stricmp(s_cachedChampionName, championName) == 0;
        }

        static DWORD s_lastResolveTick = 0;
        const DWORD now = GetTickCount();
        if (now - s_lastResolveTick < 1000) {
            return false;
        }
        s_lastResolveTick = now;

        if (!CoreRuntime::EnsureInitialized() ||
            !CoreRuntime::RefreshReadState()) {
            return false;
        }

        // Fallback: read from StaticStringCache via Player().CharacterName().
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            return false;
        }
        const std::string& charName = player.CharacterName();
        if (charName.empty()) {
            return false;
        }
        strncpy_s(s_cachedChampionName,
                  sizeof(s_cachedChampionName),
                  charName.c_str(),
                  _TRUNCATE);
        return _stricmp(s_cachedChampionName, championName) == 0;
    }

    bool Enabled() const {
        return !m_enabledMenu || m_enabledMenu->Value;
    }

    bool WriteLog() const {
        return m_writeLogMenu && m_writeLogMenu->Value;
    }

    bool IsChatTyping() const {
        const auto& ctx = CoreRuntime::GetContext();
        uintptr_t chatController = ctx.chatViewController;
        if (!Globals::IsValidPtr(chatController)) {
            chatController = CoreRuntime::ReadGlobalPtr(ctx.chatViewControllerGlobal);
        }
        if (!Globals::IsValidPtr(chatController)) {
            return false;
        }

        const std::uint8_t inputActive = Globals::Read<std::uint8_t>(
            chatController + Offset::ChatViewControllerLayout::InputActive);
        return inputActive == 1;
    }

    static bool KeyDown(const MenuKeyBind* keyBind) {
        return keyBind && keyBind->Active;
    }

    // Resolve the best enemy hero target near the cursor.
    //
    // Why this replaces the old HudInput+0x64 SelectedObjNetId reader:
    //   - On IDB 13337 the field at HudInput+0x64 is NOT a stable
    //     selected-target net id. The real hovered object is computed
    //     on-demand by the picker function sub_2A34C0 (raycast vs.
    //     `qword_1E76DE0` minion manager) and the result is stashed by
    //     callers (see sub_BAC640) into transient call-site locals, not a
    //     global field we can poll safely.
    //   - GameObjectsRuntime::UnderMouseObject (0x1E79F78) is the picker
    //     STATE struct (constructor sub_25F8E0 sets a vtable + zeros), not
    //     a GameObject pointer.
    //   - Trying to call sub_2A34C0 ourselves needs the full caller frame
    //     (camera ray, minion-manager scratch buffers, search radius) and
    //     is fragile across patches.
    //
    // Practical, robust solution used by every L#/EloBuddy-style assembly:
    // find the closest valid enemy champion within the spell's max range
    // from the local player AND reasonably close to the screen cursor.
    // This matches the user intent ("cast Q on the enemy I'm pointing at")
    // without depending on any HUD picker state.
    // Pick the best enemy hero target near the cursor.
    //
    // Two filtering policies based on cursor validity:
    //   - If we have a valid cursor world pos: pick the hero whose 2D
    //     distance (XZ plane) to the cursor is smallest, AS LONG AS it is
    //     within `maxRangeFromPlayer` of the local champion. We do NOT
    //     enforce a strict cursor radius — the player wants the closest
    //     enemy to where they're pointing, not exact pixel alignment.
    //   - If cursor is invalid (HUD picker not warmed up yet etc.):
    //     fall back to the closest enemy in range, period.
    //
    // This is test-only selection. It rejects HP-dead units up front, then
    // lets the native CanCastCheck report visibility/targetable/range failures.
    uintptr_t ResolveNearestEnemyHero(
        float maxRangeFromPlayer = 0.0f,
        float /*maxRangeFromCursor*/ = 0.0f) const {
        const auto& ctx = CoreRuntime::GetContext();
        const uintptr_t player = ctx.localPlayer;
        if (!Globals::IsValidPtr(player)) {
            return 0;
        }

        const Vec3 playerPos = Globals::Read<Vec3>(player + Offset::All::Position);
        const std::uint8_t playerTeam =
            Globals::Read<std::uint8_t>(player + Offset::All::Team);

        const Vec3 cursorWorld = SDK::Game::CursorPos();
        const bool cursorValid =
            cursorWorld.IsValid() && !cursorWorld.IsZero();
        const uintptr_t hudTargeting = Globals::Read<uintptr_t>(
            ctx.hudInstance + Offset::HudRuntime::SpellTargeting);
        const std::uint32_t hudTargetState =
            Globals::IsValidPtr(hudTargeting)
                ? Globals::Read<std::uint32_t>(
                    hudTargeting + Offset::HudSpellTargetingLayout::State)
                : 0;
        const std::uint32_t hudTargetIndex =
            Globals::IsValidPtr(hudTargeting)
                ? Globals::Read<std::uint32_t>(
                    hudTargeting + Offset::HudSpellTargetingLayout::ObjectIndex)
                : 0;

        constexpr int kMaxHeroes = 64;
        uintptr_t heroes[kMaxHeroes] = {};
        const int count = ::Core::ObjectManager::EnumerateHeroes(
            heroes, kMaxHeroes);

        uintptr_t bestEnemy = 0;
        uintptr_t bestOther = 0;
        float bestEnemyScore = (std::numeric_limits<float>::max)();
        float bestOtherScore = (std::numeric_limits<float>::max)();
        const bool enforcePlayerRange = maxRangeFromPlayer > 0.0f;
        const float maxFromPlayerSq = enforcePlayerRange
            ? maxRangeFromPlayer * maxRangeFromPlayer
            : (std::numeric_limits<float>::max)();

        /*
        Appendf(
            "[%s] target-scan tick=%d heroes=%d player=0x%llX team=%u pos=%.1f %.1f %.1f cursorValid=%d cursor=%.1f %.1f %.1f hudTargetState=%u hudTargetIndex=0x%X maxPlayerRange=%.1f\r\n",
            DebugPrefix(),
            SDK::Game::TickCount(),
            count,
            static_cast<unsigned long long>(player),
            static_cast<unsigned>(playerTeam),
            playerPos.x,
            playerPos.y,
            playerPos.z,
            cursorValid ? 1 : 0,
            cursorWorld.x,
            cursorWorld.y,
            cursorWorld.z,
            hudTargetState,
            hudTargetIndex,
            maxRangeFromPlayer);
        */

        for (int i = 0; i < count; ++i) {
            const uintptr_t hero = heroes[i];
            if (!Globals::IsValidPtr(hero) || hero == player) {
                /*
                Appendf(
                    "[%s] target-candidate index=%d ptr=0x%llX skip=%s\r\n",
                    DebugPrefix(),
                    i,
                    static_cast<unsigned long long>(hero),
                    hero == player ? "local-player" : "invalid-pointer");
                */
                continue;
            }

            const std::uint8_t team =
                Globals::Read<std::uint8_t>(hero + Offset::All::Team);
            const float health =
                Globals::Read<float>(hero + Offset::AttackableUnit::HP);
            const bool hpDead = health <= 0.0f;
            const std::uint32_t networkId =
                Globals::Read<std::uint32_t>(hero + Offset::All::NetworkId);
            const std::uint32_t objectIndex =
                Globals::Read<std::uint32_t>(hero + Offset::All::Index);

            const Vec3 heroPos = Globals::Read<Vec3>(hero + Offset::All::Position);
            if (!heroPos.IsValid() || heroPos.IsZero()) {
                /*
                Appendf(
                    "[%s] target-candidate index=%d ptr=0x%llX net=%u team=%u hpDead=%d hp=%.1f pos=%.1f %.1f %.1f skip=invalid-position\r\n",
                    DebugPrefix(),
                    i,
                    static_cast<unsigned long long>(hero),
                    networkId,
                    static_cast<unsigned>(team),
                    hpDead ? 1 : 0,
                    health,
                    heroPos.x,
                    heroPos.y,
                    heroPos.z);
                */
                continue;
            }

            if (hpDead) {
                /*
                Appendf(
                    "[%s] target-candidate index=%d ptr=0x%llX net=%u team=%u hpDead=1 hp=%.1f pos=%.1f %.1f %.1f skip=hp-dead\r\n",
                    DebugPrefix(),
                    i,
                    static_cast<unsigned long long>(hero),
                    networkId,
                    static_cast<unsigned>(team),
                    health,
                    heroPos.x,
                    heroPos.y,
                    heroPos.z);
                */
                continue;
            }

            const float dxP = heroPos.x - playerPos.x;
            const float dzP = heroPos.z - playerPos.z;
            const float distFromPlayerSq = dxP * dxP + dzP * dzP;
            if (enforcePlayerRange && distFromPlayerSq > maxFromPlayerSq) {
                /*
                Appendf(
                    "[%s] target-candidate index=%d ptr=0x%llX net=%u team=%u hpDead=%d hp=%.1f pos=%.1f %.1f %.1f distPlayer=%.1f skip=out-of-test-range\r\n",
                    DebugPrefix(),
                    i,
                    static_cast<unsigned long long>(hero),
                    networkId,
                    static_cast<unsigned>(team),
                    hpDead ? 1 : 0,
                    health,
                    heroPos.x,
                    heroPos.y,
                    heroPos.z,
                    std::sqrt(distFromPlayerSq));
                */
                continue;
            }

            float score;
            if (cursorValid) {
                const float dxC = heroPos.x - cursorWorld.x;
                const float dzC = heroPos.z - cursorWorld.z;
                score = dxC * dxC + dzC * dzC;
            } else {
                score = distFromPlayerSq;
            }

            const bool differentTeam =
                playerTeam == 0 || team == 0 || team != playerTeam;
            const bool hudMatch =
                hudTargetIndex != 0 && objectIndex == hudTargetIndex;
            const float selectionScore = hudMatch ? -1.0f : score;
            if (differentTeam && selectionScore < bestEnemyScore) {
                bestEnemyScore = selectionScore;
                bestEnemy = hero;
            }
            if (selectionScore < bestOtherScore) {
                bestOtherScore = selectionScore;
                bestOther = hero;
            }

            /*
            Appendf(
                "[%s] target-candidate index=%d ptr=0x%llX objIndex=0x%X net=%u team=%u enemy=%d hudMatch=%d hpDead=%d hp=%.1f pos=%.1f %.1f %.1f distPlayer=%.1f distCursor=%.1f\r\n",
                DebugPrefix(),
                i,
                static_cast<unsigned long long>(hero),
                objectIndex,
                networkId,
                static_cast<unsigned>(team),
                differentTeam ? 1 : 0,
                hudMatch ? 1 : 0,
                hpDead ? 1 : 0,
                health,
                heroPos.x,
                heroPos.y,
                heroPos.z,
                std::sqrt(distFromPlayerSq),
                cursorValid ? std::sqrt(score) : -1.0f);
            */
        }

        // In a two-hero practice session, falling back to the only non-local
        // hero is safe for diagnosis even if the team field changed. Native
        // CanCastCheck still rejects an ally or otherwise illegal target.
        const uintptr_t selected =
            bestEnemy ? bestEnemy : (count <= 2 ? bestOther : 0);
        /*
        Appendf(
            "[%s] target-selected ptr=0x%llX source=%s\r\n",
            DebugPrefix(),
            static_cast<unsigned long long>(selected),
            bestEnemy
                ? (bestEnemyScore < 0.0f ? "hud-target-index" : "enemy-team")
                : (selected ? "two-hero-fallback" : "none"));
        */
        return selected;
    }

    // Backwards-compatibility alias for plugins that still call the old
    // resolver name. Now delegates to the nearest-enemy implementation.
    uintptr_t ResolveHoveredTarget() const {
        return ResolveNearestEnemyHero();
    }

    // Minimal combo target selector: independent from the mouse. Lowest
    // current health wins, with distance as the tie-breaker.
    uintptr_t ResolveComboTarget(float range) const {
        (void)CoreRuntime::RefreshReadState();
        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(ctx.localPlayer)) {
            return 0;
        }

        const Vec3 playerPos = Globals::Read<Vec3>(ctx.localPlayer + Offset::All::Position);
        const std::uint8_t playerTeam = Globals::Read<std::uint8_t>(
            ctx.localPlayer + Offset::All::Team);
        const float rangeSq = range > 0.0f
            ? range * range
            : (std::numeric_limits<float>::max)();

        uintptr_t heroes[64] = {};
        const int count =
            ::Core::ObjectManager::EnumerateHeroes(heroes, 64);
        uintptr_t best = 0;
        float bestHealth = (std::numeric_limits<float>::max)();
        float bestDistanceSq = (std::numeric_limits<float>::max)();

        for (int i = 0; i < count; ++i) {
            const uintptr_t hero = heroes[i];
            if (!Globals::IsValidPtr(hero) ||
                hero == ctx.localPlayer) {
                continue;
            }

            const std::uint8_t team = Globals::Read<std::uint8_t>(
                hero + Offset::All::Team);
            const float health = Globals::Read<float>(
                hero + Offset::AttackableUnit::HP);
            const Vec3 position = Globals::Read<Vec3>(hero + Offset::All::Position);
            if ((playerTeam != 0 && team == playerTeam) ||
                health <= 0.0f ||
                !position.IsValid() ||
                position.IsZero()) {
                continue;
            }

            const float distanceSq =
                playerPos.DistanceSqr2D(position);
            if (distanceSq > rangeSq) {
                continue;
            }

            if (health < bestHealth ||
                (std::fabs(health - bestHealth) < 0.01f &&
                 distanceSq < bestDistanceSq)) {
                best = hero;
                bestHealth = health;
                bestDistanceSq = distanceSq;
            }
        }

        /*
        if (best) {
            Appendf(
                "[%s] combo-target tick=%d ptr=0x%llX net=%u hp=%.1f distance=%.1f range=%.1f mouse=%.1f %.1f %.1f\r\n",
                DebugPrefix(),
                SDK::Game::TickCount(),
                static_cast<unsigned long long>(best),
                Globals::Read<std::uint32_t>(
                    best + Offset::All::NetworkId),
                bestHealth,
                std::sqrt(bestDistanceSq),
                range,
                SDK::Game::CursorPos().x,
                SDK::Game::CursorPos().y,
                SDK::Game::CursorPos().z);
        }
        */
        return best;
    }

    Vec3 PredictTargetPosition(uintptr_t target,
                               float delaySeconds,
                               float projectileSpeed) const {
        if (!Globals::IsValidPtr(target)) {
            return {};
        }

        const SDK::AIHeroClient unit(target);
        const Vec3 current = unit.Position();
        const Vec3 source = SDK::ObjectManager::Player().Position();
        if (!current.IsValid() || current.IsZero()) {
            return {};
        }

        float arrival = std::max(0.0f, delaySeconds);
        if (projectileSpeed > 1.0f &&
            projectileSpeed < FLT_MAX * 0.5f) {
            arrival += source.Distance2D(current) / projectileSpeed;
        }

        const auto path = SDK::Utils::MathUtils::GetWaypoints(unit);
        if (path.empty()) {
            return current;
        }

        const float moveSpeed = std::max(unit.MoveSpeed(), 0.0f);
        const Vec2 predicted = SDK::Utils::MathUtils::PositionAfter(
            path,
            static_cast<int>(arrival * 1000.0f),
            static_cast<int>(moveSpeed));
        if (!predicted.IsValid() || predicted.IsZero()) {
            return current;
        }

        return { predicted.x, current.y, predicted.y };
    }

    void RecordBlocked(const char* action, const char* reason) {
        const DWORD now = GetTickCount();
        if (now - m_lastBlockedLog < 300) {
            return;
        }
        m_lastBlockedLog = now;

        Appendf(
            "[%s] blocked tick=%d action=%s reason=%s phase=%u player=0x%llX hud=0x%llX validation=0x%08X\r\n",
            DebugPrefix(),
            SDK::Game::TickCount(),
            action ? action : "?",
            reason ? reason : "?",
            CoreRuntime::GetContext().currentPhase,
            static_cast<unsigned long long>(CoreRuntime::GetContext().localPlayer),
            static_cast<unsigned long long>(CoreRuntime::GetContext().hudInstance),
            CoreRuntime::GetContext().validationMask);
    }

    void RecordAttempt(const char* action,
                       bool success,
                       const Vec3& requestedPosition,
                       uintptr_t target = 0) {
        InterlockedIncrement64(&m_attempts);
        if (success) {
            InterlockedIncrement64(&m_successes);
        }

        m_lastTrace = CoreCastSpell::LastTrace();
        const std::uint32_t targetNetId = Globals::IsValidPtr(target)
            ? Globals::Read<std::uint32_t>(target + Offset::All::NetworkId)
            : 0;

        if (WriteLog()) {
            Appendf(
                "[%s] cast tick=%d action=%s ok=%d kind=%s failure=%s slot=%u target=0x%llX targetNet=%u targetIndex=0x%X hudTargetIndex=0x%X hudTargetState=%u requested=%.1f %.1f %.1f virtualCursor=%d screen=%.1f %.1f w2sFn=0x%llX w2sCtx=0x%llX start=%.1f %.1f %.1f end=%.1f %.1f %.1f player=0x%llX spellbook=0x%llX spellSlot=0x%llX spellInput=0x%llX hud=0x%llX context=0x%llX canCastFn=0x%llX castFn=0x%llX trampoline=0x%llX bypass=%d canCast=%d nativeResult=%lld\r\n",
                DebugPrefix(),
                SDK::Game::TickCount(),
                action ? action : "?",
                success ? 1 : 0,
                CoreCastSpell::CastKindName(m_lastTrace.kind),
                CoreCastSpell::CastFailureName(m_lastTrace.failure),
                static_cast<unsigned>(m_lastTrace.slot),
                static_cast<unsigned long long>(target),
                targetNetId,
                m_lastTrace.targetObjectIndex,
                m_lastTrace.hudTargetObjectIndex,
                m_lastTrace.hudTargetState,
                requestedPosition.x,
                requestedPosition.y,
                requestedPosition.z,
                m_lastTrace.virtualCursorApplied ? 1 : 0,
                m_lastTrace.virtualCursorScreen.x,
                m_lastTrace.virtualCursorScreen.y,
                static_cast<unsigned long long>(
                    m_lastTrace.worldToScreenFn),
                static_cast<unsigned long long>(
                    m_lastTrace.worldToScreenContext),
                m_lastTrace.startPosition.x,
                m_lastTrace.startPosition.y,
                m_lastTrace.startPosition.z,
                m_lastTrace.endPosition.x,
                m_lastTrace.endPosition.y,
                m_lastTrace.endPosition.z,
                static_cast<unsigned long long>(m_lastTrace.localPlayer),
                static_cast<unsigned long long>(m_lastTrace.spellbook),
                static_cast<unsigned long long>(m_lastTrace.spellSlot),
                static_cast<unsigned long long>(m_lastTrace.spellInput),
                static_cast<unsigned long long>(m_lastTrace.hud),
                static_cast<unsigned long long>(m_lastTrace.castContext),
                static_cast<unsigned long long>(m_lastTrace.canCastCheck),
                static_cast<unsigned long long>(m_lastTrace.castSpellSafe),
                static_cast<unsigned long long>(m_lastTrace.spoofTrampoline),
                m_lastTrace.bypassPrepared ? 1 : 0,
                m_lastTrace.canCastAccepted ? 1 : 0,
                static_cast<long long>(m_lastTrace.nativeResult));
        }

        NightSharpDebug::Logf(
            "[%s] action=%s ok=%d failure=%s",
            DebugPrefix(),
            action ? action : "?",
            success ? 1 : 0,
            CoreCastSpell::CastFailureName(m_lastTrace.failure));
    }

    void Appendf(const char* format, ...) const {
        if (!format || !WriteLog()) {
            return;
        }

        char line[3072] = {};
        va_list args;
        va_start(args, format);
        std::vsnprintf(line, sizeof(line), format, args);
        va_end(args);

        HANDLE hFile = CreateFileA(
            LogPath(),
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

private:
    static inline CastSpellTestPluginBase* s_active = nullptr;

    Menu* m_menu = nullptr;
    MenuBool* m_enabledMenu = nullptr;
    MenuBool* m_writeLogMenu = nullptr;
    volatile long long m_attempts = 0;
    volatile long long m_successes = 0;
    volatile long long m_doCastLocal = 0;
    volatile long long m_processSpellLocal = 0;
    volatile long long m_stopCastLocal = 0;
    DWORD m_lastBlockedLog = 0;
    CoreCastSpell::CastTrace m_lastTrace = {};

    void CreateMenu() {
        DestroyMenu();

        m_menu = new Menu(GetInternalId(), GetName(), true);
        auto* settings = m_menu->AddSubMenu(new Menu("settings", "Settings"));
        m_enabledMenu = settings->Add(new MenuBool("enabled", "Enabled", true));
        m_writeLogMenu = settings->Add(new MenuBool("writeLog", "Write debug log", false));
        BuildChampionMenu(settings);
        settings->Add(new MenuButton(
            "resetCounters",
            "Reset counters",
            "Reset",
            [this]() {
                ResetCounters();
                Appendf("[%s] counters reset\r\n", DebugPrefix());
            }));
        settings->Add(new MenuButton(
            "clearLog",
            "Clear log",
            "Clear",
            [this]() { ResetLogFile(); }));
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

    void ResetCounters() {
        InterlockedExchange64(&m_attempts, 0);
        InterlockedExchange64(&m_successes, 0);
        InterlockedExchange64(&m_doCastLocal, 0);
        InterlockedExchange64(&m_processSpellLocal, 0);
        InterlockedExchange64(&m_stopCastLocal, 0);
        m_lastTrace = {};
    }

    static long long ReadCounter(volatile long long& counter) {
        return InterlockedCompareExchange64(&counter, 0, 0);
    }

    void ResetLogFile() const {
        HANDLE hFile = CreateFileA(
            LogPath(),
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

    static void OnGameUpdate(const SDK::Events::GameUpdateEventArgs& args) {
        if (!s_active || !s_active->Enabled()) {
            return;
        }

        s_active->HandleGameUpdate(args);
    }

    static void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
        if (s_active) {
            s_active->HandleSpellEvent("OnDoCast", args, s_active->m_doCastLocal);
        }
    }

    static void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
        if (s_active) {
            s_active->HandleSpellEvent(
                "OnProcessSpell",
                args,
                s_active->m_processSpellLocal);
        }
    }

    static void OnStopCast(const SDK::Events::StopCastEventArgs& args) {
        if (!s_active || !SDK::Events::IsLocalPlayer(args.Sender)) {
            return;
        }

        InterlockedIncrement64(&s_active->m_stopCastLocal);
        s_active->Appendf(
            "[%s] OnStopCast tick=%d hit=%lld slot=%d force=%d player=0x%llX net=%u spellbook=0x%llX process=0x%llX\r\n",
            s_active->DebugPrefix(),
            SDK::Game::TickCount(),
            args.Raw.HitCount,
            args.Slot,
            args.ForceStop ? 1 : 0,
            static_cast<unsigned long long>(args.Sender.Ptr),
            args.Sender.NetworkId,
            static_cast<unsigned long long>(args.Spellbook),
            static_cast<unsigned long long>(args.ProcessFlag));
    }

    void HandleSpellEvent(const char* eventName,
                          const SDK::Events::ProcessSpellEventArgs& args,
                          volatile long long& counter) {
        if (!SDK::Events::IsLocalPlayer(args.Sender)) {
            return;
        }

        InterlockedIncrement64(&counter);
        Appendf(
            "[%s] %s tick=%d hit=%lld slot=%d spell='%s' script='%s' targetNet=%u start=%.1f %.1f %.1f end=%.1f %.1f %.1f cast=%.1f %.1f %.1f\r\n",
            DebugPrefix(),
            eventName ? eventName : "?",
            SDK::Game::TickCount(),
            args.Raw.HitCount,
            args.Slot,
            args.SpellName,
            args.ScriptName,
            args.TargetNetworkId,
            args.StartPosition.x,
            args.StartPosition.y,
            args.StartPosition.z,
            args.EndPosition.x,
            args.EndPosition.y,
            args.EndPosition.z,
            args.CastPosition.x,
            args.CastPosition.y,
            args.CastPosition.z);
    }
};

} // namespace Plugins
