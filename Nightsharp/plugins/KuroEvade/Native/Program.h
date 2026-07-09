#pragma once

#include "ObjectCache.h"

namespace Plugins::KuroEvade {

struct Program final {
    static constexpr const char* Name = "KuroEvade";

    static bool& Loaded() {
        static bool loaded = false;
        return loaded;
    }

    static void Initialize() {
        ObjectCache::Init();
        Loaded() = true;
    }

    static void Shutdown() {
        ObjectCache::Turrets().clear();
        Loaded() = false;
    }

    static bool IsLoaded() {
        return Loaded();
    }
};

} // namespace Plugins::KuroEvade
