#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace SDK::Extensions {

    template <typename Container, typename Predicate>
    inline auto Find(const Container& source, Predicate&& match) -> typename Container::value_type {
        const auto it = std::find_if(source.begin(), source.end(), std::forward<Predicate>(match));
        return it != source.end() ? *it : typename Container::value_type{};
    }

    template <typename Container, typename Action>
    inline void ForEach(Container&& list, Action&& action) {
        for (auto&& item : list) {
            action(item);
        }
    }

    template <typename T>
    inline std::vector<std::vector<T>> GetCombinations(const std::vector<T>& allValues) {
        std::vector<std::vector<T>> collection = {};
        const std::size_t count = allValues.size();
        if (count == 0) {
            return collection;
        }

        const std::uint64_t total = (count >= 63) ? 0 : (1ULL << count);
        if (total == 0) {
            return collection;
        }

        collection.reserve(static_cast<std::size_t>(total));
        for (std::uint64_t mask = 0; mask < total; ++mask) {
            std::vector<T> subset = {};
            for (std::size_t i = 0; i < count; ++i) {
                if ((mask & (1ULL << i)) == 0) {
                    subset.push_back(allValues[i]);
                }
            }
            collection.push_back(std::move(subset));
        }

        return collection;
    }

    template <typename T, typename... Ts>
    inline bool In(const T& source, const Ts&... list) {
        return ((source == list) || ...);
    }

    template <typename Container, typename Comparer>
    inline auto MaxOrDefault(const Container& container, Comparer&& comparer) -> typename Container::value_type {
        if (container.begin() == container.end()) {
            return typename Container::value_type{};
        }

        auto bestIt = container.begin();
        auto bestValue = comparer(*bestIt);
        for (auto it = std::next(container.begin()); it != container.end(); ++it) {
            const auto value = comparer(*it);
            if (bestValue < value) {
                bestValue = value;
                bestIt = it;
            }
        }

        return *bestIt;
    }

    template <typename Container, typename Comparer>
    inline auto MinOrDefault(const Container& container, Comparer&& comparer) -> typename Container::value_type {
        if (container.begin() == container.end()) {
            return typename Container::value_type{};
        }

        auto bestIt = container.begin();
        auto bestValue = comparer(*bestIt);
        for (auto it = std::next(container.begin()); it != container.end(); ++it) {
            const auto value = comparer(*it);
            if (value < bestValue) {
                bestValue = value;
                bestIt = it;
            }
        }

        return *bestIt;
    }

    template <typename Enum>
    inline std::enable_if_t<std::is_enum_v<Enum>, Enum> SetFlags(Enum value, Enum flags, bool status = true) {
        using Underlying = std::underlying_type_t<Enum>;
        const auto valueBits = static_cast<Underlying>(value);
        const auto flagBits = static_cast<Underlying>(flags);
        return static_cast<Enum>(status ? (valueBits | flagBits) : (valueBits & ~flagBits));
    }

    template <typename Enum>
    inline std::enable_if_t<std::is_enum_v<Enum>, Enum> ClearFlags(Enum value, Enum flags) {
        return SetFlags(value, flags, false);
    }

    template <typename Enum, typename Container>
    inline std::enable_if_t<std::is_enum_v<Enum>, Enum> CombineFlags(const Container& flags) {
        using Underlying = std::underlying_type_t<Enum>;
        Underlying bits = 0;
        for (const auto& flag : flags) {
            bits |= static_cast<Underlying>(flag);
        }
        return static_cast<Enum>(bits);
    }

    template <typename Enum>
    inline std::enable_if_t<std::is_enum_v<Enum>, std::vector<Enum>> GetFlags(Enum value) {
        using Underlying = std::make_unsigned_t<std::underlying_type_t<Enum>>;
        std::vector<Enum> flags = {};
        const Underlying bits = static_cast<Underlying>(value);
        for (std::size_t bit = 0; bit < (sizeof(Underlying) * 8); ++bit) {
            const Underlying mask = static_cast<Underlying>(1) << bit;
            if ((bits & mask) != 0) {
                flags.push_back(static_cast<Enum>(mask));
            }
        }
        return flags;
    }

    template <typename Enum>
    inline std::enable_if_t<std::is_enum_v<Enum>, std::string> GetFlagDescription(Enum) {
        return {};
    }

    template <typename Container>
    inline double StandardDeviation(const Container& values) {
        if (values.begin() == values.end()) {
            return 0.0;
        }

        double sum = 0.0;
        std::size_t count = 0;
        for (const auto& value : values) {
            sum += static_cast<double>(value);
            ++count;
        }

        const double avg = sum / static_cast<double>(count);
        double squaredDiffs = 0.0;
        for (const auto& value : values) {
            const double delta = static_cast<double>(value) - avg;
            squaredDiffs += delta * delta;
        }

        return std::sqrt(squaredDiffs / static_cast<double>(count));
    }

    template <typename T, typename U>
    inline T To(const U& object) {
        return static_cast<T>(object);
    }

} // namespace SDK::Extensions
