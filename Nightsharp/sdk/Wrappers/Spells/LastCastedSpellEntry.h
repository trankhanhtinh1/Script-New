#pragma once

#include "../../Core/Objects.h"
#include "../../Core/Variables.h"
#include "../../Events/Events.h"
#include "../../GameObjects/ObjectManager.h"

#include <cstdint>
#include <string>

namespace SDK {

class LastCastedSpellEntry {
public:
    LastCastedSpellEntry() = default;

    explicit LastCastedSpellEntry(const ::SDK::Events::ProcessSpellEventArgs& args) {
        Name = args.SpellName;
        Target = args.TargetNetworkId != 0
            ? ObjectManager::GetUnitByNetworkId<AIBaseClient>(static_cast<int>(args.TargetNetworkId))
            : AIBaseClient();
        StartTime = static_cast<float>(Variables::TickCount());

        // NightSharp currently exposes CastDelay for this event. Keep the
        // public timing in TickCount milliseconds, normalizing second-based
        // delays from the core decoder when needed.
        const float durationMs = args.CastDelay > 0.0f && args.CastDelay < 60.0f
            ? args.CastDelay * 1000.0f
            : args.CastDelay;
        EndTime = StartTime + durationMs;
        SpellData = args.SpellData;
        IsValid = true;
    }

    float EndTime = 0.0f;
    bool IsValid = false;
    std::string Name;
    uintptr_t SpellData = 0;
    float StartTime = 0.0f;
    AIBaseClient Target;
};

} // namespace SDK
