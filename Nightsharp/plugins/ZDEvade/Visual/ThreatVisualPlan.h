#pragma once

#include "ThreatVisualDispatch.h"
#include "../Detection/Threat.h"

namespace ZDEvade {

struct LineVisualEndpoints {
    Vec2 head = {};
    Vec2 end = {};
};

inline LineVisualEndpoints ResolveLineVisualEndpoints(
        const Threat& threat,
        int now) {
    const bool preLaunchCast =
        threat.IsCastBodyPendingAt(now);
    const bool liveStraightMissile =
        threat.Type() == ZDSpellType::Line &&
        threat.missileBound &&
        !threat.projectileTerminated &&
        threat.RouteMode() == MissileRouteMode::Straight;
    return {
        threat.HeadAtTick(now),
        preLaunchCast || liveStraightMissile
            ? threat.AuthoredEnd()
            : threat.endPos
    };
}

struct ThreatVisualPlan {
    ThreatVisualBody body = ThreatVisualBody::None;
    bool drawBody = false;
    bool drawEndExplosion = false;
    LineVisualEndpoints line = {};
};

inline ThreatVisualPlan ResolveThreatVisualPlan(
        const Threat& threat,
        int now) {
    ThreatVisualPlan plan;
    if (!threat.HasValidGeometry() || threat.data->noProcess)
        return plan;

    const ThreatVisualDispatch dispatch = GetThreatVisualDispatch(
        threat.Type(),
        threat.HasEndExplosionArea());
    plan.body = dispatch.body;
    plan.drawBody =
        dispatch.body != ThreatVisualBody::None &&
        !threat.projectileTerminated &&
        (threat.IsCastBodyPendingAt(now) ||
         threat.IsBodyActiveAt(now));
    plan.drawEndExplosion =
        dispatch.endExplosion &&
        threat.IsEndExplosionActiveAt(now);
    if (dispatch.body == ThreatVisualBody::Capsule)
        plan.line = ResolveLineVisualEndpoints(threat, now);
    return plan;
}

} // namespace ZDEvade
