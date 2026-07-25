#pragma once

#include "../Events/Events.h"
#include "Logging.h"
#include "../Core/Variables.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace SDK::Core::Utils {

struct CancellationToken {
    const bool* Cancelled = nullptr;

    bool IsCancellationRequested() const {
        return Cancelled && *Cancelled;
    }
};

class DelayActionItem {
public:
    DelayActionItem() = default;
    DelayActionItem(int time, std::function<void()> func, CancellationToken token = {})
        : Function(std::move(func)),
          Time(::SDK::Variables::TickCount() + time),
          Token(token) {}

    std::function<void()> Function = {};
    int Time = 0;
    CancellationToken Token = {};
};

class DelayAction {
public:
    static void Add(int time, std::function<void()> func) {
        Add(DelayActionItem(time, std::move(func), {}));
    }

    static void Add(float time, std::function<void()> func) {
        Add(static_cast<int>(time), std::move(func));
    }

    static void Add(int time, std::function<void()> func, CancellationToken token) {
        Add(DelayActionItem(time, std::move(func), token));
    }

    static void Add(float time, std::function<void()> func, CancellationToken token) {
        Add(static_cast<int>(time), std::move(func), token);
    }

    static void Add(const DelayActionItem& item) {
        EnsureInstalled();
        Items().push_back(item);
    }

private:
    static std::vector<DelayActionItem>& Items() {
        static std::vector<DelayActionItem> items;
        return items;
    }

    static void OnUpdate(const ::SDK::Events::GameUpdateEventArgs&) {
        const int now = ::SDK::Variables::TickCount();
        auto& items = Items();
        for (std::size_t i = 0; i < items.size();) {
            auto item = items[i];
            if (now < item.Time) {
                ++i;
                continue;
            }

            items.erase(items.begin() + static_cast<std::ptrdiff_t>(i));
            if (item.Token.IsCancellationRequested() || !item.Function) {
                continue;
            }

            try {
                item.Function();
            } catch (...) {
                ::SDK::Utils::Logging::Write()(::SDK::LogLevel::Error, "DelayAction callback crashed");
            }
        }
    }

    static void EnsureInstalled() {
        static bool installed = false;
        if (!installed) {
            installed = true;
            ::SDK::Events::AddOnGameUpdate(&OnUpdate);
        }
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using CancellationToken = ::SDK::Core::Utils::CancellationToken;
    using DelayAction = ::SDK::Core::Utils::DelayAction;
    using DelayActionItem = ::SDK::Core::Utils::DelayActionItem;
} // namespace SDK::Utils
