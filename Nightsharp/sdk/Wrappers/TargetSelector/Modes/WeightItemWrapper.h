#pragma once

#include "IWeightItem.h"
#include <cfloat>

namespace SDK::Modes {

class WeightItemWrapper {
public:
    IWeightItem* Item = nullptr;
    float Weight = 0.0f;
    float MinValue = FLT_MAX;
    float MaxValue = -FLT_MAX;
    float SimulationMinValue = FLT_MAX;
    float SimulationMaxValue = -FLT_MAX;

    explicit WeightItemWrapper(IWeightItem* item) : Item(item) {}
};

} // namespace SDK::Modes
