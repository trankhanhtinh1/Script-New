#pragma once

#include "Logging.h"
#include "../Core/Game.h"

#include <functional>
#include <memory>
#include <new>
#include <vector>

namespace SDK::Utils::DelayAction {

class CancellationToken {
public:
    CancellationToken() = default;
    explicit CancellationToken(std::shared_ptr<bool> state)
        : m_state(std::move(state)) {}

    bool IsCancellationRequested() const {
        return m_state && *m_state;
    }

private:
    std::shared_ptr<bool> m_state = {};
};

class CancellationTokenSource {
public:
    CancellationTokenSource()
        : m_state(std::make_shared<bool>(false)) {}

    CancellationToken Token() const {
        return CancellationToken(m_state);
    }

    void Cancel() const {
        if (m_state) {
            *m_state = true;
        }
    }

private:
    std::shared_ptr<bool> m_state = {};
};

struct DelayActionItem {
    int Time = 0;
    std::function<void()> Function = {};
    CancellationToken Token = {};

    DelayActionItem() = default;
    DelayActionItem(int delayMs, std::function<void()> function, CancellationToken token = {})
        : Time(Game::TickCount() + delayMs)
        , Function(std::move(function))
        , Token(std::move(token)) {}
};

namespace detail {
    inline std::vector<DelayActionItem>*& Items() {
        static auto* storage = new(std::nothrow) std::vector<DelayActionItem>();
        return storage;
    }

    inline bool g_initialized = false;
}

inline void Update() {
    auto* items = detail::Items();
    if (!items) {
        return;
    }

    const int now = Game::TickCount();
    for (auto it = items->begin(); it != items->end();) {
        if (it->Token.IsCancellationRequested()) {
            it = items->erase(it);
            continue;
        }

        if (now >= it->Time) {
            try {
                if (it->Function) {
                    it->Function();
                }
            } catch (...) {
                Logging::Write(true, true, "DelayAction::Update")(LogLevel::Error, "Delayed action threw.");
            }
            it = items->erase(it);
            continue;
        }
        ++it;
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
    if (auto* items = detail::Items()) {
        items->clear();
    }
}

inline void Add(const DelayActionItem& item) {
    auto* items = detail::Items();
    if (!items) {
        return;
    }
    items->push_back(item);
}

inline void Add(int timeMs, std::function<void()> function) {
    Add(DelayActionItem(timeMs, std::move(function), {}));
}

inline void Add(float timeMs, std::function<void()> function) {
    Add(static_cast<int>(timeMs), std::move(function));
}

inline void Add(int timeMs, std::function<void()> function, const CancellationToken& token) {
    Add(DelayActionItem(timeMs, std::move(function), token));
}

inline void Add(float timeMs, std::function<void()> function, const CancellationToken& token) {
    Add(static_cast<int>(timeMs), std::move(function), token);
}

} // namespace SDK::Utils::DelayAction

