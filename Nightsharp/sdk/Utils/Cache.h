#pragma once

#include "DelayAction.h"
#include "Logging.h"

#include <algorithm>
#include <any>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SDK::Core::Utils {

enum class CacheEntryRemovedReason {
    Removed,
    Expired,
};

struct CacheItem {
    std::string Key = {};
    std::any Value = {};
    std::string RegionName = "Default";
};

struct CacheItemPolicy {
    int AbsoluteExpirationMs = 0;
    std::function<void(const CacheItem&, CacheEntryRemovedReason)> RemovedCallback = {};
    std::function<void(const std::string&, CacheEntryRemovedReason)> UpdateCallback = {};
};

struct ValueChangedArgs {
    std::string Key = {};
    std::any OldValue = {};
    std::any NewValue = {};
    std::string RegionName = {};
};

struct EntryAddedArgs {
    std::string Key = {};
    std::string RegionName = {};
    std::any Value = {};
};

class Cache {
public:
    using OnEntryAddedDelegate = void(*)(Cache&, const EntryAddedArgs&);
    using OnValueChangedDelegate = void(*)(Cache&, const ValueChangedArgs&);

    static Cache& Instance() {
        static Cache instance;
        return instance;
    }

    std::any& operator[](const std::string& key) {
        return regions_["Default"][key];
    }

    void CreateRegion(const std::string& regionName) {
        regions_.try_emplace(regionName);
    }

    std::any AddOrGetExisting(const std::string& key,
                              const std::function<std::any()>& function,
                              const std::string& regionName = "Default") {
        auto& region = Region(regionName);
        auto it = region.find(key);
        if (it != region.end()) {
            return it->second;
        }

        std::any value = function ? function() : std::any{};
        region.emplace(key, value);
        FireEntryAdded({ key, regionName, value });
        return value;
    }

    std::any AddOrGetExisting(const std::string& key,
                              const std::any& value,
                              int absoluteExpirationMs = 0,
                              const std::string& regionName = "Default") {
        auto& region = Region(regionName);
        auto it = region.find(key);
        if (it != region.end()) {
            return it->second;
        }

        region.emplace(key, value);
        FireEntryAdded({ key, regionName, value });
        ScheduleExpiration(key, regionName, absoluteExpirationMs);
        return value;
    }

    CacheItem AddOrGetExisting(const CacheItem& item, const CacheItemPolicy& policy) {
        const std::string regionName = item.RegionName.empty() ? "Default" : item.RegionName;
        const std::any value = AddOrGetExisting(item.Key, item.Value, policy.AbsoluteExpirationMs, regionName);
        RegisterPolicy(item.Key, regionName, policy);
        return CacheItem{ item.Key, value, regionName };
    }

    bool Contains(const std::string& key, const std::string& regionName = "Default") const {
        const auto region = regions_.find(regionName);
        return region != regions_.end() && region->second.find(key) != region->second.end();
    }

    std::any Get(const std::string& key, const std::string& regionName = "Default") const {
        const auto region = regions_.find(regionName);
        if (region == regions_.end()) {
            return {};
        }
        const auto value = region->second.find(key);
        return value != region->second.end() ? value->second : std::any{};
    }

    template <typename T>
    T Get(const std::string& key, const std::string& regionName = "Default") const {
        const std::any value = Get(key, regionName);
        return value.has_value() && value.type() == typeid(T) ? std::any_cast<T>(value) : T{};
    }

    CacheItem GetCacheItem(const std::string& key, const std::string& regionName = "Default") const {
        return CacheItem{ key, Get(key, regionName), regionName };
    }

    long long GetCount(const std::string& regionName = "Default") const {
        const auto region = regions_.find(regionName);
        return region != regions_.end() ? static_cast<long long>(region->second.size()) : 0;
    }

    std::any Remove(const std::string& key, const std::string& regionName = "Default") {
        auto& region = Region(regionName);
        auto it = region.find(key);
        if (it == region.end()) {
            return {};
        }

        const std::any value = it->second;
        CallEntryUpdates(key, CacheEntryRemovedReason::Removed, regionName);
        region.erase(it);
        CallEntryRemoved(key, value, CacheEntryRemovedReason::Removed, regionName);
        return value;
    }

    void Set(const std::string& key,
             const std::any& value,
             int absoluteExpirationMs = 0,
             const std::string& regionName = "Default") {
        auto& region = Region(regionName);
        auto it = region.find(key);
        if (it != region.end()) {
            FireValueChanged({ key, it->second, value, regionName });
        }
        region[key] = value;
        ScheduleExpiration(key, regionName, absoluteExpirationMs);
    }

    void Set(const CacheItem& item, const CacheItemPolicy& policy) {
        const std::string regionName = item.RegionName.empty() ? "Default" : item.RegionName;
        RegisterPolicy(item.Key, regionName, policy);
        Set(item.Key, item.Value, policy.AbsoluteExpirationMs, regionName);
    }

    bool TryGetValue(const std::string& key, std::any& value, const std::string& regionName = "Default") const {
        if (!Contains(key, regionName)) {
            value.reset();
            return false;
        }
        value = Get(key, regionName);
        return true;
    }

    static bool AddOnEntryAdded(OnEntryAddedDelegate handler) {
        return AddHandler(EntryAddedHandlers(), handler);
    }

    static bool AddOnValueChanged(OnValueChangedDelegate handler) {
        return AddHandler(ValueChangedHandlers(), handler);
    }

private:
    Cache() {
        CreateRegion("Default");
    }

    using RegionMap = std::unordered_map<std::string, std::any>;

    RegionMap& Region(const std::string& name) {
        auto regionName = name.empty() ? std::string("Default") : name;
        return regions_[regionName];
    }

    static std::string KeyRegion(const std::string& key, const std::string& regionName) {
        return key + "\x1F" + regionName;
    }

    void RegisterPolicy(const std::string& key, const std::string& regionName, const CacheItemPolicy& policy) {
        const auto id = KeyRegion(key, regionName);
        if (policy.RemovedCallback) {
            removedCallbacks_[id] = policy.RemovedCallback;
        }
        if (policy.UpdateCallback) {
            updateCallbacks_[id] = policy.UpdateCallback;
        }
    }

    void ScheduleExpiration(std::string key, std::string regionName, int ms) {
        if (ms <= 0) {
            return;
        }

        DelayAction::Add(ms, [this, key = std::move(key), regionName = std::move(regionName)] {
            if (!Contains(key, regionName)) {
                return;
            }
            auto& region = Region(regionName);
            const std::any value = region[key];
            CallEntryUpdates(key, CacheEntryRemovedReason::Expired, regionName);
            region.erase(key);
            CallEntryRemoved(key, value, CacheEntryRemovedReason::Expired, regionName);
        });
    }

    void CallEntryRemoved(const std::string& key,
                          const std::any& value,
                          CacheEntryRemovedReason reason,
                          const std::string& regionName) {
        const auto callback = removedCallbacks_.find(KeyRegion(key, regionName));
        if (callback != removedCallbacks_.end() && callback->second) {
            callback->second(CacheItem{ key, value, regionName }, reason);
        }
    }

    void CallEntryUpdates(const std::string& key,
                          CacheEntryRemovedReason reason,
                          const std::string& regionName) {
        const auto callback = updateCallbacks_.find(KeyRegion(key, regionName));
        if (callback != updateCallbacks_.end() && callback->second) {
            callback->second(key, reason);
        }
    }

    static std::vector<OnEntryAddedDelegate>& EntryAddedHandlers() {
        static std::vector<OnEntryAddedDelegate> handlers;
        return handlers;
    }

    static std::vector<OnValueChangedDelegate>& ValueChangedHandlers() {
        static std::vector<OnValueChangedDelegate> handlers;
        return handlers;
    }

    template <typename Handler>
    static bool AddHandler(std::vector<Handler>& handlers, Handler handler) {
        if (!handler) {
            return false;
        }
        if (std::find(handlers.begin(), handlers.end(), handler) == handlers.end()) {
            handlers.push_back(handler);
        }
        return true;
    }

    void FireEntryAdded(const EntryAddedArgs& args) {
        for (auto handler : EntryAddedHandlers()) {
            if (handler) {
                handler(*this, args);
            }
        }
    }

    void FireValueChanged(const ValueChangedArgs& args) {
        for (auto handler : ValueChangedHandlers()) {
            if (handler) {
                handler(*this, args);
            }
        }
    }

    std::unordered_map<std::string, RegionMap> regions_;
    std::unordered_map<std::string, std::function<void(const CacheItem&, CacheEntryRemovedReason)>> removedCallbacks_;
    std::unordered_map<std::string, std::function<void(const std::string&, CacheEntryRemovedReason)>> updateCallbacks_;
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using Cache = ::SDK::Core::Utils::Cache;
    using CacheEntryRemovedReason = ::SDK::Core::Utils::CacheEntryRemovedReason;
    using CacheItem = ::SDK::Core::Utils::CacheItem;
    using CacheItemPolicy = ::SDK::Core::Utils::CacheItemPolicy;
    using EntryAddedArgs = ::SDK::Core::Utils::EntryAddedArgs;
    using ValueChangedArgs = ::SDK::Core::Utils::ValueChangedArgs;
} // namespace SDK::Utils
