#pragma once

#include "../../generated/GapcloserData.generated.h"
#include "AntiGapcloser.h"

namespace SDK::Events {

    using GapCloser = Generated::GapcloserData::GapcloserEntry;
    using GapCloserEventArgs = SDK::AntiGapcloser::GapcloserArgs;
    using Gapcloser = SDK::AntiGapcloser;

    inline bool AddOnGapCloser(SDK::AntiGapcloser::Handler handler) {
        return SDK::AntiGapcloser::AddOnGapcloser(handler);
    }

    inline bool OnGapCloser(SDK::AntiGapcloser::Handler handler) {
        return AddOnGapCloser(handler);
    }

    inline bool AddOnGapcloser(SDK::AntiGapcloser::Handler handler) {
        return AddOnGapCloser(handler);
    }

    inline bool OnGapcloser(SDK::AntiGapcloser::Handler handler) {
        return AddOnGapCloser(handler);
    }

    inline const std::vector<GapCloserEventArgs>& ActiveSpells() {
        return SDK::AntiGapcloser::ActiveSpells();
    }

    inline const GapCloser* GapCloserSpells() {
        return Generated::GapcloserData::kGapcloserEntries;
    }

    inline int GapCloserSpellsCount() {
        return Generated::GapcloserData::kGapcloserEntryCount;
    }
}
