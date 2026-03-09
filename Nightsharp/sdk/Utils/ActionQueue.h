#pragma once
// ============================================================================
// ActionQueue.h — Conditional action queue system
// Ported from EnsoulSharp.SDK/Core/Utils/ActionQueue.cs
// ============================================================================

#include <functional>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <algorithm>

namespace SDK {

    // ========================================================================
    // ActionQueue — queues actions that execute when conditions are met
    //   Items stay in the queue until their RemoveCondition returns true.
    //   Call ActionQueue::Update() once per frame to process.
    // ========================================================================
    class ActionQueue {
    public:
        // An item in the action queue
        struct Item {
            uint32_t                id = 0;              // auto-assigned unique ID
            std::function<void()>   action;              // action to execute
            std::function<bool()>   condition;            // returns true → execute action
            std::function<bool()>   removeCondition;      // returns true → remove item
        };

        // Enqueue an action item, returns its unique ID
        static uint32_t Enqueue(const Item& item) {
            std::lock_guard<std::mutex> lock(GetMutex());
            uint32_t id = s_nextId++;
            Item copy = item;
            copy.id = id;
            GetItems().push_back(std::move(copy));
            return id;
        }

        // Convenience: enqueue with lambdas
        static uint32_t Enqueue(
            std::function<void()> action,
            std::function<bool()> condition,
            std::function<bool()> removeCondition)
        {
            Item item;
            item.action = std::move(action);
            item.condition = std::move(condition);
            item.removeCondition = std::move(removeCondition);
            return Enqueue(item);
        }

        // Dequeue by ID
        static bool Dequeue(uint32_t id) {
            std::lock_guard<std::mutex> lock(GetMutex());
            auto& items = GetItems();
            auto it = std::find_if(items.begin(), items.end(),
                [id](const Item& i) { return i.id == id; });
            if (it != items.end()) {
                items.erase(it);
                return true;
            }
            return false;
        }

        // Clear all items
        static void Clear() {
            std::lock_guard<std::mutex> lock(GetMutex());
            GetItems().clear();
        }

        // Get current queue size
        static size_t Size() {
            std::lock_guard<std::mutex> lock(GetMutex());
            return GetItems().size();
        }

        // ====================================================================
        // Update — process all queued items (call once per frame)
        // ====================================================================
        static void Update() {
            std::lock_guard<std::mutex> lock(GetMutex());
            auto& items = GetItems();

            // Process and collect indices to remove
            std::vector<size_t> toRemove;

            for (size_t i = 0; i < items.size(); ++i) {
                auto& item = items[i];
                try {
                    // Check condition and execute
                    if (item.condition && item.condition()) {
                        if (item.action) item.action();
                    }
                    // Check remove condition
                    if (item.removeCondition && item.removeCondition()) {
                        toRemove.push_back(i);
                    }
                }
                catch (...) {
                    // Remove broken items
                    toRemove.push_back(i);
                }
            }

            // Remove in reverse order to keep indices valid
            for (auto it = toRemove.rbegin(); it != toRemove.rend(); ++it) {
                if (*it < items.size()) {
                    items.erase(items.begin() + *it);
                }
            }
        }

    private:
        static inline std::atomic<uint32_t> s_nextId{ 1 };

        static std::vector<Item>& GetItems() {
            static std::vector<Item> items;
            return items;
        }

        static std::mutex& GetMutex() {
            static std::mutex mtx;
            return mtx;
        }
    };

} // namespace SDK
