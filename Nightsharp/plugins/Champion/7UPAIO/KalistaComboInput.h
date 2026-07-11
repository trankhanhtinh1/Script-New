#pragma once

namespace Plugins::AIO7UP::Kalista::ComboInput {

inline bool IsActive(bool orbwalkerCombo, bool spaceDown, bool inputAllowed) {
    return orbwalkerCombo || (spaceDown && inputAllowed);
}

} // namespace Plugins::AIO7UP::Kalista::ComboInput
