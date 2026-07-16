#pragma once

#include "Globals.h"
#include "Vector.h"
#include "offset.h"

#include <cstdint>

namespace CorePosition {

inline Vec3 Read(uintptr_t object) {
    if (!Globals::IsValidPtr(object)) {
        return {};
    }

    return {
        Globals::Read<float>(object + Offset::All::PositionX),
        Globals::Read<float>(object + Offset::All::PositionY),
        Globals::Read<float>(object + Offset::All::PositionZ),
    };
}

} // namespace CorePosition
