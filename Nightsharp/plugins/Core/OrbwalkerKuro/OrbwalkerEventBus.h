#pragma once

#include "OrbwalkerTypes.h"

namespace OrbwalkerKuro {

class OrbwalkerBase;

namespace OrbwalkingDetail {

// Runtime instance riêng của Kuro cho việc dispatch game event tĩnh.
inline OrbwalkerKuro::OrbwalkerBase* RuntimeInstance = nullptr;

// Các orbwalking action event (BeforeAttack/OnAttack/...) bắn vào bus chung
// của SDK, để mọi plugin subscribe qua SDK::Orbwalker::OnBeforeAttack v.v.
// vẫn nhận event khi OrbwalkerKuro override bản SDK.
using ::SDK::OrbwalkingDetail::FireBeforeAttack;
using ::SDK::OrbwalkingDetail::FireOnAttack;
using ::SDK::OrbwalkingDetail::FireAfterAttack;
using ::SDK::OrbwalkingDetail::FireBeforeMove;
using ::SDK::OrbwalkingDetail::FireNonKillableMinion;

} // namespace OrbwalkingDetail

} // namespace OrbwalkerKuro
