#pragma once

#include <cstddef>

namespace SDK::Data::GlobalInterruptableSpellsList {

inline constexpr char kJson[] = R"NS_GLOBALINTERRUPTABLESPELLSLIST_JSON([
  {
    "DangerLevel": "Medium",
    "MovementInterrupts": true,
    "Name": "summonerteleport",
    "Slot": "Unknown"
  }
])NS_GLOBALINTERRUPTABLESPELLSLIST_JSON";

inline constexpr std::size_t kJsonSize = sizeof(kJson) - 1;

} // namespace SDK::Data::GlobalInterruptableSpellsList