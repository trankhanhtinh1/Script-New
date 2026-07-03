#pragma once

#include "../Core/Objects.h"
#include "../Enumerations/JungleType.h"

namespace SDK::Core::Utils {

class Jungle {
public:
    static JungleType GetJungleType(const AIMinionClient& minion) {
        return minion.GetJungleType();
    }

    static bool IsJungleBuff(const AIMinionClient& minion) {
        return minion.IsJungleBuff();
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using Jungle = ::SDK::Core::Utils::Jungle;
} // namespace SDK::Utils
