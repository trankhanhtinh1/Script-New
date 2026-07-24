#pragma once

#include "../../../Core/CoreControlTypes.h"
#include "EvadeRoutingPolicy.h"

namespace ZDEvade {

inline MoveFailureClassification AdaptCoreMoveIssueResult(
    CoreControl::OrderIssueResult result,
    int previousConsecutiveFailures) {
    switch (result) {
    case CoreControl::OrderIssueResult::Issued:
        return {
            MoveIssueResult::Issued,
            0,
        };
    case CoreControl::OrderIssueResult::Throttled:
        return {
            MoveIssueResult::Throttled,
            std::max(0, previousConsecutiveFailures),
        };
    case CoreControl::OrderIssueResult::Blocked:
    case CoreControl::OrderIssueResult::Failed:
        return ClassifyMoveFailure(previousConsecutiveFailures);
    default:
        return ClassifyMoveFailure(previousConsecutiveFailures);
    }
}

} // namespace ZDEvade
