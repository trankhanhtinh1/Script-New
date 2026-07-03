#pragma once

namespace SDK::Core::Utils {

struct IFilter {
    virtual ~IFilter() = default;
};

template <typename T>
struct IFilterT : IFilter {
    virtual T Apply(T data) = 0;
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using IFilter = ::SDK::Core::Utils::IFilter;
    template <typename T>
    using IFilterT = ::SDK::Core::Utils::IFilterT<T>;
} // namespace SDK::Utils
