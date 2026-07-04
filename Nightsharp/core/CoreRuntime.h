#pragma once

#include "Globals.h"
#include "offset.h"

#include <cstdint>

namespace CoreRuntime {

    enum Phase : uint32_t {
        Phase_None = 0,
        Phase_Read = 1,
        Phase_Write = 2
    };

    enum StatusFlags : uint32_t {
        Status_None                = 0,
        Status_BaseReady           = 1u << 0,
        Status_GlobalTableReady    = 1u << 1,
        Status_CallTableReady      = 1u << 2,
        Status_RuntimeObjectsReady = 1u << 3,
        Status_GameTimeReady       = 1u << 4,
        Status_Initialized         = 1u << 31
    };

    struct CoreContext {
        uintptr_t moduleBase = 0;

        uintptr_t localPlayerGlobal = 0;
        uintptr_t objectManagerGlobal = 0;
        uintptr_t heroManagerGlobal = 0;
        uintptr_t minionManagerGlobal = 0;
        uintptr_t turretManagerGlobal = 0;
        uintptr_t missileManagerGlobal = 0;
        uintptr_t navGridGlobal = 0;
        uintptr_t underMouseObjectGlobal = 0;
        uintptr_t netInstanceGlobal = 0;
        uintptr_t missionInfoGlobal = 0;
        uintptr_t gameTimeGlobal = 0;
        uintptr_t chatViewControllerGlobal = 0;
        uintptr_t shopInstanceGlobal = 0;
        uintptr_t openWindowsArrayGlobal = 0;
        uintptr_t openWindowsCountGlobal = 0;
        uintptr_t cursorInstanceGlobal = 0;
        uintptr_t mouseScreenVec2Global = 0;
        uintptr_t hudInstanceGlobal = 0;
        uintptr_t viewPortGlobal = 0;
        uintptr_t viewPort2Global = 0;
        uintptr_t rendererGlobal = 0;
        uintptr_t viewProjGlobal = 0;

        uintptr_t worldToScreenFn = 0;
        uintptr_t issueOrderFn = 0;
        uintptr_t castSpellFn = 0;
        uintptr_t getPingFn = 0;
        uintptr_t getAttackDelayFn = 0;
        uintptr_t getAttackWindupFn = 0;
        uintptr_t getBoundingRadiusFn = 0;
        uintptr_t getAiManagerFn = 0;
        uintptr_t detectionWatcher2 = 0;
        uintptr_t spoofTrampoline = 0;

        uintptr_t localPlayer = 0;
        uintptr_t objectManager = 0;
        uintptr_t heroManager = 0;
        uintptr_t minionManager = 0;
        uintptr_t turretManager = 0;
        uintptr_t missileManager = 0;
        uintptr_t navGrid = 0;
        uintptr_t underMouseObject = 0;
        uintptr_t netInstance = 0;
        uintptr_t missionInfo = 0;
        uintptr_t chatViewController = 0;
        uintptr_t shopInstance = 0;
        uintptr_t openWindowsArray = 0;
        uint32_t openWindowsCount = 0;
        uintptr_t cursorInstance = 0;
        uintptr_t mouseScreenVec2 = 0;
        uintptr_t hudInstance = 0;
        uintptr_t viewPort = 0;
        uintptr_t viewPort2 = 0;
        uintptr_t renderer = 0;
        uintptr_t viewProjInstance = 0;

        float gameTime = 0.0f;
        int cachedPing = 0;
        float cachedAttackDelay = 0.0f;
        float cachedAttackWindup = 0.0f;

        uint32_t initGeneration = 0;
        uint32_t refreshGeneration = 0;
        uint32_t phaseGeneration = 0;
        uint32_t statusMask = Status_None;
        uint32_t lastErrorMask = 0;
        uint32_t validationMask = 0;
        uint32_t currentPhase = Phase_None;
        uint32_t issueOrderAttempts = 0;
        uint32_t issueOrderSuccesses = 0;
        uint32_t castAttempts = 0;
        uint32_t castSuccesses = 0;
        int32_t lastIssueOrderResult = 0;
        int32_t lastCastResult = 0;
    };

    using Context = CoreContext;

    inline CoreContext g_ctx = {};

    inline uintptr_t ResolveRva(uintptr_t rva) {
        return (g_ctx.moduleBase && rva) ? (g_ctx.moduleBase + rva) : 0;
    }

    inline uintptr_t ReadGlobalPtr(uintptr_t globalAddr) {
        return globalAddr ? Globals::Read<uintptr_t>(globalAddr) : 0;
    }

    inline void ResetContext() {
        g_ctx = {};
    }

    inline uint32_t BuildRequiredInitMask() {
        return Status_BaseReady | Status_GlobalTableReady | Status_CallTableReady | Status_Initialized;
    }

    inline uint32_t BuildRequiredRuntimeMask() {
        return BuildRequiredInitMask() | Status_RuntimeObjectsReady | Status_GameTimeReady;
    }

    inline bool HasInitReady() {
        return (g_ctx.statusMask & BuildRequiredInitMask()) == BuildRequiredInitMask();
    }

    inline bool IsReady() {
        return (g_ctx.statusMask & BuildRequiredRuntimeMask()) == BuildRequiredRuntimeMask();
    }

    inline bool IsReadPhase() {
        return g_ctx.currentPhase == Phase_Read;
    }

    inline bool IsWritePhase() {
        return g_ctx.currentPhase == Phase_Write;
    }

    inline bool Initialize() {
        ResetContext();

        if (!Globals::Init()) {
            g_ctx.lastErrorMask = 1u << 0;
            return false;
        }

        g_ctx.moduleBase = Globals::base;
        g_ctx.statusMask |= Status_BaseReady;

        g_ctx.localPlayerGlobal      = ResolveRva(Offset::GameObjectsRuntime::Player);
        g_ctx.objectManagerGlobal    = ResolveRva(Offset::GameObjectsRuntime::Objects);
        g_ctx.heroManagerGlobal      = ResolveRva(Offset::GameObjectsRuntime::Heroes);
        g_ctx.minionManagerGlobal    = ResolveRva(Offset::GameObjectsRuntime::Minions);
        g_ctx.turretManagerGlobal    = ResolveRva(Offset::GameObjectsRuntime::Turrets);
        g_ctx.missileManagerGlobal   = ResolveRva(Offset::GameObjectsRuntime::Missiles);
        g_ctx.navGridGlobal          = ResolveRva(Offset::NavGridRuntime::NavGrid);
        g_ctx.underMouseObjectGlobal = ResolveRva(Offset::GameObjectsRuntime::UnderMouseObject);
        g_ctx.netInstanceGlobal      = ResolveRva(Offset::GameRuntime::NetInstance);
        g_ctx.missionInfoGlobal      = ResolveRva(Offset::GameRuntime::MissionInfoInstance);
        g_ctx.gameTimeGlobal         = ResolveRva(Offset::GameRuntime::GameTime);
        g_ctx.chatViewControllerGlobal = ResolveRva(Offset::GameRuntime::ChatViewController);
        g_ctx.shopInstanceGlobal     = ResolveRva(Offset::GameRuntime::ShopInstance);
        g_ctx.openWindowsArrayGlobal = ResolveRva(Offset::GameRuntime::OpenWindowsArray);
        g_ctx.openWindowsCountGlobal = ResolveRva(Offset::GameRuntime::OpenWindowsCount);
        g_ctx.cursorInstanceGlobal   = ResolveRva(Offset::GameRuntime::CursorPosRaw);
        g_ctx.mouseScreenVec2Global  = ResolveRva(Offset::GameRuntime::MouseScreenVec2);
        g_ctx.hudInstanceGlobal      = ResolveRva(Offset::DrawingRuntime::HudInstance);
        g_ctx.viewPortGlobal         = ResolveRva(Offset::DrawingRuntime::ViewPort);
        g_ctx.viewPort2Global        = ResolveRva(Offset::DrawingRuntime::ViewPort2);
        g_ctx.rendererGlobal         = ResolveRva(Offset::DrawingRuntime::Renderer);
        g_ctx.viewProjGlobal         = ResolveRva(Offset::DrawingRuntime::ViewProjOffset);

        if (g_ctx.localPlayerGlobal &&
            g_ctx.objectManagerGlobal &&
            g_ctx.heroManagerGlobal &&
            g_ctx.minionManagerGlobal &&
            g_ctx.turretManagerGlobal &&
            g_ctx.missileManagerGlobal &&
            g_ctx.navGridGlobal &&
            g_ctx.netInstanceGlobal &&
            g_ctx.gameTimeGlobal) {
            g_ctx.statusMask |= Status_GlobalTableReady;
        } else {
            g_ctx.lastErrorMask |= 1u << 1;
        }

        g_ctx.worldToScreenFn   = ResolveRva(Offset::DrawingRuntime::WorldToScreen);
        g_ctx.issueOrderFn      = ResolveRva(Offset::ControlRuntime::IssueOrder);
        g_ctx.castSpellFn       = 0;
        g_ctx.getPingFn         = ResolveRva(Offset::GameRuntime::GetPing);
        g_ctx.getAttackDelayFn  = ResolveRva(Offset::ControlRuntime::GetAttackDelay);
        g_ctx.getAttackWindupFn = ResolveRva(Offset::ControlRuntime::GetAttackWindup);
        g_ctx.getBoundingRadiusFn = ResolveRva(Offset::ControlRuntime::GetBoundingRadius);
        g_ctx.getAiManagerFn    = ResolveRva(Offset::NavGridRuntime::GetAiManager);

        if (g_ctx.worldToScreenFn &&
            g_ctx.issueOrderFn &&
            g_ctx.getPingFn &&
            g_ctx.getAttackDelayFn &&
            g_ctx.getAttackWindupFn &&
            g_ctx.getBoundingRadiusFn) {
            g_ctx.statusMask |= Status_CallTableReady;
        } else {
            g_ctx.lastErrorMask |= 1u << 2;
        }

        g_ctx.statusMask |= Status_Initialized;
        ++g_ctx.initGeneration;
        return HasInitReady();
    }

    inline bool EnsureInitialized() {
        return HasInitReady() || Initialize();
    }

    inline bool RefreshReadState() {
        if (!EnsureInitialized()) {
            g_ctx.lastErrorMask |= 1u << 3;
            return false;
        }

        g_ctx.localPlayer = ReadGlobalPtr(g_ctx.localPlayerGlobal);
        g_ctx.objectManager = ReadGlobalPtr(g_ctx.objectManagerGlobal);
        g_ctx.heroManager = ReadGlobalPtr(g_ctx.heroManagerGlobal);
        g_ctx.minionManager = ReadGlobalPtr(g_ctx.minionManagerGlobal);
        g_ctx.turretManager = ReadGlobalPtr(g_ctx.turretManagerGlobal);
        g_ctx.missileManager = ReadGlobalPtr(g_ctx.missileManagerGlobal);
        g_ctx.navGrid = ReadGlobalPtr(g_ctx.navGridGlobal);
        g_ctx.underMouseObject = ReadGlobalPtr(g_ctx.underMouseObjectGlobal);
        g_ctx.netInstance = ReadGlobalPtr(g_ctx.netInstanceGlobal);
        g_ctx.missionInfo = ReadGlobalPtr(g_ctx.missionInfoGlobal);
        g_ctx.chatViewController = ReadGlobalPtr(g_ctx.chatViewControllerGlobal);
        g_ctx.shopInstance = ReadGlobalPtr(g_ctx.shopInstanceGlobal);
        g_ctx.openWindowsArray = ReadGlobalPtr(g_ctx.openWindowsArrayGlobal);
        g_ctx.openWindowsCount = Globals::Read<uint32_t>(g_ctx.openWindowsCountGlobal);
        g_ctx.cursorInstance = ReadGlobalPtr(g_ctx.cursorInstanceGlobal);
        g_ctx.mouseScreenVec2 = ReadGlobalPtr(g_ctx.mouseScreenVec2Global);
        g_ctx.hudInstance = ReadGlobalPtr(g_ctx.hudInstanceGlobal);
        g_ctx.viewPort = ReadGlobalPtr(g_ctx.viewPortGlobal);
        g_ctx.viewPort2 = ReadGlobalPtr(g_ctx.viewPort2Global);
        g_ctx.renderer = ReadGlobalPtr(g_ctx.rendererGlobal);
        g_ctx.viewProjInstance = g_ctx.viewProjGlobal;
        g_ctx.gameTime = Globals::Read<float>(g_ctx.gameTimeGlobal);

        g_ctx.statusMask &= ~(Status_RuntimeObjectsReady | Status_GameTimeReady);

        if (Globals::IsValidPtr(g_ctx.objectManager) && Globals::IsValidPtr(g_ctx.localPlayer)) {
            g_ctx.statusMask |= Status_RuntimeObjectsReady;
        } else {
            g_ctx.lastErrorMask |= 1u << 4;
        }

        if (g_ctx.gameTime >= 0.0f && g_ctx.gameTime < 1000000.0f) {
            g_ctx.statusMask |= Status_GameTimeReady;
        } else {
            g_ctx.lastErrorMask |= 1u << 5;
        }

        ++g_ctx.refreshGeneration;
        return true;
    }

    inline bool TickRead() {
        g_ctx.currentPhase = Phase_Read;
        ++g_ctx.phaseGeneration;
        return RefreshReadState();
    }

    inline void BeginWritePhase() {
        EnsureInitialized();
        RefreshReadState();
        g_ctx.currentPhase = Phase_Write;
        ++g_ctx.phaseGeneration;
    }

    inline void EndWritePhase() {
        g_ctx.currentPhase = Phase_None;
    }

    inline void Shutdown() {
        ResetContext();
    }

    inline const CoreContext& GetContext() {
        return g_ctx;
    }

} // namespace CoreRuntime
