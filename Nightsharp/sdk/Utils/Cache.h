#pragma once

#include "../Core/Game.h"

#include <any>
#include <cstdint>
#include <functional>
#include <limits>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

namespace SDK::Utils::Cache {

using Tick = uint64_t;
inline constexpr Tick InfiniteAbsoluteExpiration = std::numeric_limits<Tick>::max();

struct Entry {
    std::any Value = {};
    Tick Expiration = InfiniteAbsoluteExpiration;
};

struct EntryAddedArgs {
    std::string Key = {};
    std::string RegionName = {};
    std::any Value = {};
};

struct ValueChangedArgs {
    std::string Key = {};
    std::string RegionName = {};
    std::any OldValue = {};
    std::any NewValue = {};
};

using OnEntryAddedDelegate = std::function<void(const EntryAddedArgs&)>;
using OnValueChangedDelegate = std::function<void(const ValueChangedArgs&)>;

namespace detail {
    struct State {
        std::unordered_map<std::string, std::unordered_map<std::string, Entry>> Regions = {};
        std::vector<OnEntryAddedDelegate> EntryAddedCallbacks = {};
        std::vector<OnValueChangedDelegate> ValueChangedCallbacks = {};
    };

    inline State*& GetState() {
        static auto* state = new(std::nothrow) State();
        return state;
    }
}

inline void CreateRegion(const std::string& regionName = "Default") {
    auto* state = detail::GetState();
    if (!state) {
        return;
    }

    if (!state->Regions.contains(regionName)) {
        state->Regions[regionName] = {};
    }
}

inline void Reset() {
    if (auto* state = detail::GetState()) {
        state->Regions.clear();
        state->EntryAddedCallbacks.clear();
        state->ValueChangedCallbacks.clear();
    }
}

inline void OnEntryAdded(OnEntryAddedDelegate callback) {
    auto* state = detail::GetState();
    if (!state || !callback) {
        return;
    }
    state->EntryAddedCallbacks.push_back(std::move(callback));
}

inline void OnValueChanged(OnValueChangedDelegate callback) {
    auto* state = detail::GetState();
    if (!state || !callback) {
        return;
    }
    state->ValueChangedCallbacks.push_back(std::move(callback));
}

inline void Update() {
    auto* state = detail::GetState();
    if (!state) {
        return;
    }

    const Tick now = static_cast<Tick>(Game::TickCount());
    for (auto& [regionName, region] : state->Regions) {
        (void)regionName;
        for (auto it = region.begin(); it != region.end();) {
            if (it->second.Expiration != InfiniteAbsoluteExpiration && now >= it->second.Expiration) {
                it = region.erase(it);
            } else {
                ++it;
            }
        }
    }
}

inline bool Contains(const std::string& key, const std::string& regionName = "Default") {
    CreateRegion(regionName);
    auto* state = detail::GetState();
    return state && state->Regions.contains(regionName) && state->Regions[regionName].contains(key);
}

template<typename T>
inline T Get(const std::string& key, const std::string& regionName = "Default", const T& fallback = T{}) {
    Update();
    auto* state = detail::GetState();
    if (!state || !Contains(key, regionName)) {
        return fallback;
    }
    try {
        return std::any_cast<T>(state->Regions[regionName][key].Value);
    } catch (...) {
        return fallback;
    }
}

template<typename T>
inline bool TryGetValue(const std::string& key, T& value, const std::string& regionName = "Default") {
    if (!Contains(key, regionName)) {
        return false;
    }

    try {
        value = std::any_cast<T>(detail::GetState()->Regions[regionName][key].Value);
        return true;
    } catch (...) {
        return false;
    }
}

template<typename T>
inline T AddOrGetExisting(const std::string& key,
                          std::function<T()> function,
                          const std::string& regionName = "Default") {
    Update();
    if (Contains(key, regionName)) {
        return Get<T>(key, regionName);
    }

    const T value = function ? function() : T{};
    CreateRegion(regionName);
    detail::GetState()->Regions[regionName][key] = Entry{ value, InfiniteAbsoluteExpiration };

    const EntryAddedArgs args{ key, regionName, value };
    for (const auto& callback : detail::GetState()->EntryAddedCallbacks) {
        if (callback) {
            callback(args);
        }
    }

    return value;
}

template<typename T>
inline T AddOrGetExisting(const std::string& key,
                          const T& value,
                          Tick absoluteExpiration = InfiniteAbsoluteExpiration,
                          const std::string& regionName = "Default") {
    Update();
    if (Contains(key, regionName)) {
        return Get<T>(key, regionName);
    }

    CreateRegion(regionName);
    detail::GetState()->Regions[regionName][key] = Entry{
        value,
        absoluteExpiration == InfiniteAbsoluteExpiration
            ? InfiniteAbsoluteExpiration
            : (static_cast<Tick>(Game::TickCount()) + absoluteExpiration)
    };

    const EntryAddedArgs args{ key, regionName, value };
    for (const auto& callback : detail::GetState()->EntryAddedCallbacks) {
        if (callback) {
            callback(args);
        }
    }

    return value;
}

template<typename T>
inline void Set(const std::string& key,
                const T& value,
                Tick absoluteExpiration = InfiniteAbsoluteExpiration,
                const std::string& regionName = "Default") {
    CreateRegion(regionName);
    auto* state = detail::GetState();
    if (!state) {
        return;
    }

    if (state->Regions[regionName].contains(key)) {
        const ValueChangedArgs args{
            key,
            regionName,
            state->Regions[regionName][key].Value,
            std::any(value)
        };
        for (const auto& callback : state->ValueChangedCallbacks) {
            if (callback) {
                callback(args);
            }
        }
    } else {
        const EntryAddedArgs args{ key, regionName, value };
        for (const auto& callback : state->EntryAddedCallbacks) {
            if (callback) {
                callback(args);
            }
        }
    }

    state->Regions[regionName][key] = Entry{
        value,
        absoluteExpiration == InfiniteAbsoluteExpiration
            ? InfiniteAbsoluteExpiration
            : (static_cast<Tick>(Game::TickCount()) + absoluteExpiration)
    };
}

inline size_t GetCount(const std::string& regionName = "Default") {
    CreateRegion(regionName);
    auto* state = detail::GetState();
    return state ? state->Regions[regionName].size() : 0U;
}

inline std::unordered_map<std::string, std::any> GetValues(const std::vector<std::string>& keys,
                                                           const std::string& regionName = "Default") {
    std::unordered_map<std::string, std::any> values = {};
    auto* state = detail::GetState();
    if (!state || !state->Regions.contains(regionName)) {
        return values;
    }

    for (const auto& key : keys) {
        auto it = state->Regions[regionName].find(key);
        if (it != state->Regions[regionName].end()) {
            values[key] = it->second.Value;
        }
    }
    return values;
}

inline bool Remove(const std::string& key, const std::string& regionName = "Default") {
    auto* state = detail::GetState();
    if (!state || !Contains(key, regionName)) {
        return false;
    }
    return state->Regions[regionName].erase(key) > 0;
}

} // namespace SDK::Utils::Cache
