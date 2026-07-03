#pragma once

#include "../Events/Events.h"
#include "Logging.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SDK::Core::Utils {

class ActionQueue {
public:
    struct Item {
        std::function<void()> Action = {};
        std::function<bool()> Condition = [] { return true; };
        std::function<bool()> RemoveCondition = [] { return true; };
        std::uint64_t Id = 0;
    };

    static std::uint64_t Enqueue(Item item, const std::string& queueName = "Default") {
        EnsureInstalled();
        item.Id = NextId();
        Queues()[queueName].push_back(std::move(item));
        return Queues()[queueName].back().Id;
    }

    static bool Dequeue(std::uint64_t id, const std::string& queueName = "Default") {
        auto& queue = Queues()[queueName];
        const auto oldSize = queue.size();
        queue.erase(
            std::remove_if(queue.begin(), queue.end(), [id](const Item& item) { return item.Id == id; }),
            queue.end());
        return queue.size() != oldSize;
    }

    static std::vector<Item> GetItems(const std::string& queueName = "Default") {
        auto it = Queues().find(queueName);
        return it != Queues().end() ? it->second : std::vector<Item>{};
    }

private:
    static std::unordered_map<std::string, std::vector<Item>>& Queues() {
        static std::unordered_map<std::string, std::vector<Item>> queues;
        return queues;
    }

    static std::uint64_t NextId() {
        static std::uint64_t id = 1;
        return id++;
    }

    static void Game_OnUpdate(const SDK::Events::GameUpdateEventArgs&) {
        for (auto& pair : Queues()) {
            auto& queue = pair.second;
            for (std::size_t i = 0; i < queue.size();) {
                bool remove = false;
                try {
                    if (!queue[i].Condition || queue[i].Condition()) {
                        if (queue[i].Action) {
                            queue[i].Action();
                        }
                    }
                    remove = !queue[i].RemoveCondition || queue[i].RemoveCondition();
                } catch (...) {
                    Logging::Write()(LogLevel::Error, "An ActionQueue item threw an exception.");
                    remove = true;
                }

                if (remove) {
                    queue.erase(queue.begin() + static_cast<std::ptrdiff_t>(i));
                } else {
                    ++i;
                }
            }
        }
    }

    static void EnsureInstalled() {
        static bool installed = false;
        if (!installed) {
            installed = true;
            SDK::Events::AddOnGameUpdate(&Game_OnUpdate);
        }
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using ActionQueue = ::SDK::Core::Utils::ActionQueue;
} // namespace SDK::Utils
