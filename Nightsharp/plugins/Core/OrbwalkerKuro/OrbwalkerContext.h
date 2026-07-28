#pragma once

#include "OrbwalkerTypes.h"
#include "../../../sdk/UI/Icons.h"

#include <string>
#include <vector>

using namespace ::SDK;

namespace OrbwalkerKuro {

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
    int pendingAttackTick = 0;
    int pendingAttackTargetNetworkId = 0;
    int lastAttackConfirmTick = 0;
    int lastAfterAttackStartTick = 0;
    int lastAttackDoCastWaitTick = 0;
    int lastAutoAttackResetTick = 0;
    int pendingAkshanSecondShotTick = 0;
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
    int lastRengarLeapTick = 0;

    float attackDelayMs = 625.0f;
    float attackWindupMs = 300.0f;
    // Calibration for the raw attack-timing getters — see
    // OrbwalkerBase::CalibrateAttackTimingScale().
    float calibrationSpeedMod = 0.0f;
    float calibrationRawDelayMs = 0.0f;
    float calibrationRawWindupMs = 0.0f;
    float visualWindupWeight = 0.0f;
    float visualReadyWeight = 1.0f;
    float visualCooldownWeight = 0.0f;
    float visualSmoothProgress = 1.0f;
    int visualLastDrawTick = 0;

    bool rawDelayTracksSpeed = true;
    bool rawWindupTracksSpeed = true;
    bool attackTimingCalibrated = false;

    bool attackEnabled = true;
    bool moveEnabled = true;
    bool pendingAttack = false;
    bool hasConfirmedAttack = false;
    bool attackCastComplete = false;
    bool lastAttackRequiresDoCastBeforeMove = false;
    bool lastAttackDoCastComplete = false;
    bool isAkshanSecondShotPending = false;
    bool isAkshanSecondShotActive = false;
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
