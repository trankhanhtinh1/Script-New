#pragma once

#include "OrbwalkerBase.h"

namespace SDK {

class OrbwalkerSelector : public OrbwalkerBase {
public:
    explicit OrbwalkerSelector(Menu* parentMenu)
        : OrbwalkerBase(parentMenu) {}
};

} // namespace SDK
