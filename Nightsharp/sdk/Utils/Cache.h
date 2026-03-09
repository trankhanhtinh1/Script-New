#pragma once
#include <string>
#include <unordered_map>
#include <any>
#include <functional>
#include <chrono>
#include <mutex>
#include <vector>
#include <algorithm>

// ============================================================================
// Cache — Generic timed key-value cache with optional expiration
// Source: EnsoulSharp.SDK/Core/Utils/Cache.cs
//
// Usage:
//   SDK::Cache::Instance().Set("myKey", 42);              // No expiration
//   SDK::Cache::Instance().Set("temp", 100, 5000);        // Expires in 5 seconds
//   int val = SDK::Cache::Instance().Get<int>("myKey");    // Get value
//   bool has = SDK::Cache::Instance().Contains("myKey");   // Check existence
//   SDK::Cache::Instance().Remove("myKey");                // Remove
//   SDK::Cache::Instance().Update();                       // Call each frame to expire entries
//
// Regions:
//   SDK::Cache::Instance().Set("key", val, 0, "MyRegion");
//   SDK::Cache::Instance().Get<int>("key", "MyRegion");
// ============================================================================

namespace SDK {

    // ========================================================================
    // Cache entry (internal)
    // ========================================================================
    struct CacheEntry {
        std::any Value;
        std::chrono::steady_clock::time_point ExpiresAt;
        bool HasExpiration = false;

        CacheEntry() = default;
        CacheEntry(std::any val)
            : Value(std::move(val)), HasExpiration(false) {}
        CacheEntry(std::any val, int expirationMs)
            : Value(std::move(val)), HasExpiration(expirationMs > 0) {
            if (HasExpiration) {
                ExpiresAt = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(expirationMs);
            }
        }

        bool IsExpired() const {
            if (!HasExpiration) return false;
            return std::chrono::steady_clock::now() >= ExpiresAt;
        }
    };

    // ========================================================================
    // Cache event args
    // ========================================================================
    struct CacheValueChangedArgs {
        std::string Key;
        std::string RegionName;
    };

    struct CacheEntryAddedArgs {
        std::string Key;
        std::string RegionName;
    };

    // ========================================================================
    // Cache — Singleton timed cache
    // ========================================================================
    class Cache {
    public:
        using OnEntryAddedFn = std::function<void(const CacheEntryAddedArgs&)>;
        using OnValueChangedFn = std::function<void(const CacheValueChangedArgs&)>;

        // Singleton access
        static Cache& Instance() {
            static Cache s_instance;
            return s_instance;
        }

        // ====================================================================
        // Set — Add or update a key with optional expiration (ms, 0 = infinite)
        // ====================================================================
        void Set(const std::string& key, std::any value,
                 int expirationMs = 0, const std::string& regionName = "Default") {
            std::lock_guard<std::mutex> lock(m_mutex);
            EnsureRegion(regionName);

            bool existed = m_regions[regionName].count(key) > 0;
            m_regions[regionName][key] = CacheEntry(std::move(value), expirationMs);

            if (existed) {
                CacheValueChangedArgs args{key, regionName};
                for (auto& cb : m_onValueChanged) cb(args);
            } else {
                CacheEntryAddedArgs args{key, regionName};
                for (auto& cb : m_onEntryAdded) cb(args);
            }
        }

        // ====================================================================
        // Get — Get a value by key, returns default if not found or expired
        // ====================================================================
        template<typename T>
        T Get(const std::string& key, const std::string& regionName = "Default") {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto regionIt = m_regions.find(regionName);
            if (regionIt == m_regions.end()) return T{};

            auto it = regionIt->second.find(key);
            if (it == regionIt->second.end()) return T{};
            if (it->second.IsExpired()) {
                regionIt->second.erase(it);
                return T{};
            }

            try {
                return std::any_cast<T>(it->second.Value);
            } catch (...) {
                return T{};
            }
        }

        // ====================================================================
        // TryGet — Try to get a value, returns true if found and not expired
        // ====================================================================
        template<typename T>
        bool TryGet(const std::string& key, T& outValue,
                    const std::string& regionName = "Default") {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto regionIt = m_regions.find(regionName);
            if (regionIt == m_regions.end()) return false;

            auto it = regionIt->second.find(key);
            if (it == regionIt->second.end()) return false;
            if (it->second.IsExpired()) {
                regionIt->second.erase(it);
                return false;
            }

            try {
                outValue = std::any_cast<T>(it->second.Value);
                return true;
            } catch (...) {
                return false;
            }
        }

        // ====================================================================
        // AddOrGetExisting — Add if not exists, return existing if it does
        // ====================================================================
        template<typename T>
        T AddOrGetExisting(const std::string& key, const T& value,
                          int expirationMs = 0,
                          const std::string& regionName = "Default") {
            std::lock_guard<std::mutex> lock(m_mutex);
            EnsureRegion(regionName);

            auto it = m_regions[regionName].find(key);
            if (it != m_regions[regionName].end() && !it->second.IsExpired()) {
                try {
                    return std::any_cast<T>(it->second.Value);
                } catch (...) {}
            }

            m_regions[regionName][key] = CacheEntry(std::any(value), expirationMs);
            CacheEntryAddedArgs args{key, regionName};
            for (auto& cb : m_onEntryAdded) cb(args);
            return value;
        }

        // ====================================================================
        // AddOrGetExisting — Lazy evaluation version (function called only if key doesn't exist)
        // ====================================================================
        template<typename T>
        T AddOrGetExisting(const std::string& key, std::function<T()> factory,
                          int expirationMs = 0,
                          const std::string& regionName = "Default") {
            std::lock_guard<std::mutex> lock(m_mutex);
            EnsureRegion(regionName);

            auto it = m_regions[regionName].find(key);
            if (it != m_regions[regionName].end() && !it->second.IsExpired()) {
                try {
                    return std::any_cast<T>(it->second.Value);
                } catch (...) {}
            }

            T result = factory();
            m_regions[regionName][key] = CacheEntry(std::any(result), expirationMs);
            CacheEntryAddedArgs args{key, regionName};
            for (auto& cb : m_onEntryAdded) cb(args);
            return result;
        }

        // ====================================================================
        // Contains — Check if key exists and is not expired
        // ====================================================================
        bool Contains(const std::string& key,
                      const std::string& regionName = "Default") {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto regionIt = m_regions.find(regionName);
            if (regionIt == m_regions.end()) return false;
            auto it = regionIt->second.find(key);
            if (it == regionIt->second.end()) return false;
            if (it->second.IsExpired()) {
                regionIt->second.erase(it);
                return false;
            }
            return true;
        }

        // ====================================================================
        // Remove — Remove a key from cache
        // ====================================================================
        bool Remove(const std::string& key,
                    const std::string& regionName = "Default") {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto regionIt = m_regions.find(regionName);
            if (regionIt == m_regions.end()) return false;
            return regionIt->second.erase(key) > 0;
        }

        // ====================================================================
        // CreateRegion — Create a new named region
        // ====================================================================
        void CreateRegion(const std::string& regionName) {
            std::lock_guard<std::mutex> lock(m_mutex);
            EnsureRegion(regionName);
        }

        // ====================================================================
        // GetCount — Get number of entries in a region
        // ====================================================================
        size_t GetCount(const std::string& regionName = "Default") {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_regions.find(regionName);
            if (it == m_regions.end()) return 0;
            return it->second.size();
        }

        // ====================================================================
        // Clear — Remove all entries from a region (or all regions)
        // ====================================================================
        void Clear(const std::string& regionName = "") {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (regionName.empty()) {
                m_regions.clear();
                EnsureRegion("Default");
            } else {
                auto it = m_regions.find(regionName);
                if (it != m_regions.end()) it->second.clear();
            }
        }

        // ====================================================================
        // Update — Call each frame to clean up expired entries
        // ====================================================================
        void Update() {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& [regionName, entries] : m_regions) {
                for (auto it = entries.begin(); it != entries.end(); ) {
                    if (it->second.IsExpired()) {
                        it = entries.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        // ====================================================================
        // Event registration
        // ====================================================================
        void OnEntryAdded(OnEntryAddedFn callback) { m_onEntryAdded.push_back(callback); }
        void OnValueChanged(OnValueChangedFn callback) { m_onValueChanged.push_back(callback); }

    private:
        Cache() {
            EnsureRegion("Default");
        }

        void EnsureRegion(const std::string& name) {
            if (m_regions.find(name) == m_regions.end()) {
                m_regions[name] = {};
            }
        }

        std::mutex m_mutex;
        std::unordered_map<std::string,
            std::unordered_map<std::string, CacheEntry>> m_regions;
        std::vector<OnEntryAddedFn> m_onEntryAdded;
        std::vector<OnValueChangedFn> m_onValueChanged;
    };

} // namespace SDK
