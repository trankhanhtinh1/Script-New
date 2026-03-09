#pragma once
#include "sdk/EzEvade/Core/Evade.h"
#include <memory>

namespace EzEvade {

class Program {
public:
    static void Main() {
        if (!s_evade) {
            s_evade = std::make_unique<Evade>();
        }
    }

private:
    static inline std::unique_ptr<Evade> s_evade = nullptr;
};

} // namespace EzEvade

