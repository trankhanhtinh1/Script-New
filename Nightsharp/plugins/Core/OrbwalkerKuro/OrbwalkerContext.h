#pragma once

#include "OrbwalkerTypes.h"
#include "../../../sdk/UI/Icons.h"

#include <string>

namespace OrbwalkerKuro {

using namespace ::SDK;

struct OrbwalkerDebugConsoleLine {
    char text[kOrbwalkerDebugConsoleLineLength] = {};
    int tick = 0;
};

struct OrbwalkerRuntimeContext {
    AttackableUnit forceTarget = {};
    AttackableUnit lastTarget = {};
    AttackableUnit laneClearMinion = {};
    AttackableUnit cachedTarget = {};
    Vector3 orbwalkerPosition = {};
    Vector3 lastMoveOrderPosition = {};
    Vector3 fakeClickPosition = {};
    Vector3 fakeCursorTargetPosition = {};
    Vec2 fakeCursorScreenPosition = {};

    int lastAutoAttackTick = 0;
    int lastMovementTick = 0;
    int lastMoveOrderTick = 0;
    int lastAttackOrderTick = 0;
    int lastAttackOrderNetworkId = 0;
    int timingSnapshotTick = 0;
    int pendingAttackTick = 0;
    int pendingAttackTargetNetworkId = 0;
    int lastAttackConfirmTick = 0;
    int lastAfterAttackStartTick = 0;
    int lastAttackDoCastWaitTick = 0;
    int lastAutoAttackResetTick = 0;
    int lastFakeMoveClickTick = 0;
    int lastFakeAttackClickTick = 0;
    int fakeClickExpireTick = 0;
    int fakeCursorVisibleUntilTick = 0;
    int fakeCursorTextureLastTryTick = 0;
    int allPauseTick = 0;
    int attackPauseTick = 0;
    int movePauseTick = 0;
    int cachedTargetTick = -1;
    int cachedTargetForceTargetNetworkId = 0;
    int cachedShouldWaitTick = -1;
    OrbwalkerDebugConsoleLine debugConsoleLines[kOrbwalkerDebugConsoleMaxLines] = {};

    float attackDelayMs = 625.0f;
    float attackWindupMs = 300.0f;

    int debugConsoleNextLine = 0;
    int debugConsoleLineCount = 0;

    bool attackEnabled = true;
    bool moveEnabled = true;
    bool pendingAttack = false;
    bool hasConfirmedAttack = false;
    bool attackCastComplete = false;
    bool lastAttackRequiresDoCastBeforeMove = false;
    bool lastAttackDoCastComplete = false;
    bool fakeCursorScreenValid = false;
    bool fakeCursorTextureLoadTried = false;
    bool cachedShouldWait = false;
    bool disposed = false;
    OrbwalkingMode activeMode = OrbwalkingMode::None;
    OrbwalkingMode cachedTargetMode = OrbwalkingMode::None;

    UI::Icons::LoadedTexture fakeCursorTexture = {};
    std::string fakeCursorTexturePath;
};

} // namespace OrbwalkerKuro
