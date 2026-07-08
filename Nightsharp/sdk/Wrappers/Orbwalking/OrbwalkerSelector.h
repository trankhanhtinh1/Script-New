#pragma once

#include "OrbwalkerBase.h"

namespace SDK {

// Compatibility wrapper for older includes; dev2 target selection lives in OrbwalkerBase.
class OrbwalkerSelector : public OrbwalkerBase {
public:
    explicit OrbwalkerSelector(Menu* parentMenu)
        : OrbwalkerBase(parentMenu) {}
};

} // namespace SDK
