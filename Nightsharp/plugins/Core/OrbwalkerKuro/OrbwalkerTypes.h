#pragma once

// OrbwalkerKuro — bản copy của orbwalker dev2 (sdk/Wrappers/Orbwalking cũ),
// chạy như một Core plugin và override orbwalker SDK qua
// SDK::Orbwalker::AddOrbwalker/SetOrbwalker. Các shared type
// (OrbwalkingActionArgs, IOrbwalker, event bus) dùng thẳng của SDK để mọi
// plugin phụ thuộc SDK::Orbwalker tiếp tục hoạt động bình thường.

#include "../../../sdk/Core/Objects.h"
#include "../../../sdk/Enumerations/OrbwalkingMode.h"
#include "../../../sdk/Enumerations/OrbwalkingType.h"
#include "../../../sdk/UI/UI.h"
#include "../../../sdk/Wrappers/Orbwalking/Orbwalker.h"

#include <cstdint>

using namespace ::SDK;

namespace OrbwalkerKuro {

class OrbwalkerBase;
class OrbwalkerSelector;

} // namespace OrbwalkerKuro
