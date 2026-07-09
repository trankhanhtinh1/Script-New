#pragma once

#include <functional>

namespace Plugins::KuroEvade {

struct EvadeSpellData;
using UseSpellFunc = std::function<bool(const EvadeSpellData&, bool)>;

} // namespace Plugins::KuroEvade

