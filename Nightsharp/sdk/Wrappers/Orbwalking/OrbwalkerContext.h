#pragma once

#include "OrbwalkerTypes.h"

namespace SDK {

struct OrbwalkerRuntimeContext {
    AttackableUnit forceTarget = {};
    AttackableUnit lastTarget = {};
    AttackableUnit laneClearMinion = {};
    AttackableUnit cachedTarget = {};
    Vector3 orbwalkerPosition = {};
    Vector3 lastMoveOrderPosition = {};

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
    int allPauseTick = 0;
    int attackPauseTick = 0;
    int movePauseTick = 0;
    int cachedTargetTick = -1;
    int cachedTargetForceTargetNetworkId = 0;
    int cachedShouldWaitTick = -1;

    float attackDelayMs = 625.0f;
    float attackWindupMs = 300.0f;

    bool attackEnabled = true;
    bool moveEnabled = true;
    bool pendingAttack = false;
    bool hasConfirmedAttack = false;
    bool attackCastComplete = false;
    bool lastAttackRequiresDoCastBeforeMove = false;
    bool lastAttackDoCastComplete = false;
    bool cachedShouldWait = false;
    bool disposed = false;
    OrbwalkingMode activeMode = OrbwalkingMode::None;
    OrbwalkingMode cachedTargetMode = OrbwalkingMode::None;
};

} // namespace SDK
