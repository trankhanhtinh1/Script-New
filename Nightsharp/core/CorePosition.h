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

    const float x = Globals::Read<float>(object + Offset::All::PositionX);
    const float y = Globals::Read<float>(object + Offset::All::PositionY);
    const float z = Globals::Read<float>(object + Offset::All::PositionZ);
    return { x, y, z };
}

} // namespace CorePosition
