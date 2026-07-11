#pragma once

#include "../../core/CoreObjects.h"

namespace SDK::Events::detail {

inline constexpr bool UsesStructureSnapshot(
    ::Core::Objects::ObjectType type) noexcept {
    using ::Core::Objects::ObjectType;
    return type == ObjectType::AITurretClient ||
           type == ObjectType::BarracksDampenerClient ||
           type == ObjectType::HQClient;
}

inline constexpr bool ShouldQueueObjectLifecycle(
    ::Core::Objects::ObjectType type) noexcept {
    return !UsesStructureSnapshot(type);
}

} // namespace SDK::Events::detail
