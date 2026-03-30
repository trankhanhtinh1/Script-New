#pragma once

#include "Logging.h"
#include "../Core/Game.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <new>
#include <vector>

namespace SDK::Utils::ActionQueue {

using QueueId = uint64_t;

struct Item {
    QueueId Id = 0;
    std::function<void()> Action = {};
    std::function<bool()> Condition = {};
    std::function<bool()> RemoveCondition = {};
};

namespace detail {
    inline std::vector<Item>*& Queue() {
        static auto* storage = new(std::nothrow) std::vector<Item>();
        return storage;
    }

    inline QueueId& NextId() {
        static QueueId value = 1;
        return value;
    }

    inline bool g_initialized = false;
}

inline void Update() {
    auto* queue = detail::Queue();
    if (!queue) {
        return;
    }

    for (auto it = queue->begin(); it != queue->end();) {
        bool shouldRemove = false;
        try {
            const bool shouldRun = !it->Condition || it->Condition();
            if (shouldRun && it->Action) {
                it->Action();
            }
            shouldRemove = !it->RemoveCondition || it->RemoveCondition();
        } catch (...) {
            Logging::Write(true, true, "ActionQueue::Update")(LogLevel::Error, "Queued item threw.");
            shouldRemove = true;
        }

        if (shouldRemove) {
            it = queue->erase(it);
        } else {
            ++it;
        }
    }
}

inline void Initialize() {
    if (detail::g_initialized) {
        return;
    }
    detail::g_initialized = true;
    Game::OnUpdate(Update);
}

inline void Reset() {
    if (auto* queue = detail::Queue()) {
        queue->clear();
    }
}

inline QueueId Enqueue(Item item) {
    auto* queue = detail::Queue();
    if (!queue) {
        return 0;
    }
    item.Id = detail::NextId()++;
    queue->push_back(std::move(item));
    return queue->back().Id;
}

inline bool Dequeue(QueueId id) {
    auto* queue = detail::Queue();
    if (!queue) {
        return false;
    }
    const auto size = queue->size();
    queue->erase(std::remove_if(queue->begin(), queue->end(), [id](const Item& item) {
        return item.Id == id;
    }), queue->end());
    return queue->size() != size;
}

inline std::vector<Item> GetItems() {
    return detail::Queue() ? *detail::Queue() : std::vector<Item>{};
}

} // namespace SDK::Utils::ActionQueue

