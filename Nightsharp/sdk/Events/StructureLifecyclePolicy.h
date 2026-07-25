#pragma once

#include "../../core/CoreObjects.h"

namespace SDK::Events::detail {

inline constexpr bool UsesStructureSnapshot(
    ::Core::Objects::ObjectType type) noexcept {
    using ::Core::Objects::ObjectType;
    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    // return type == ObjectType::AITurretClient ||
    //        type == ObjectType::BarracksDampenerClient ||
    //        type == ObjectType::HQClient;
    // REMOVED: Turret/Inhibitor/Nexus disabled
    (void)type;
    return false;
}

inline constexpr bool ShouldQueueObjectLifecycle(
    ::Core::Objects::ObjectType type) noexcept {
    return !UsesStructureSnapshot(type);
}

} // namespace SDK::Events::detail
