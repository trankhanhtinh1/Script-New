#pragma once

#include <cstdint>

namespace Core::Events::detail {

enum class BuffBridgeHook : std::uint8_t {
    Add,
    Remove,
    Update,
};

// The lightweight script-event bridge callbacks expose an object, an event
// bridge, a buff-name string-view, and either an owner component or stack
// count. None of those registers is a BuffData pointer. Buff state is resolved
// later from owner + name by CoreBuffs when no direct address is available.
inline constexpr std::uintptr_t DecodeBridgeBuffAddress(
    BuffBridgeHook,
    std::uintptr_t,
    std::uintptr_t,
    std::uintptr_t,
    std::uintptr_t) noexcept {
    return 0;
}

} // namespace Core::Events::detail
