#pragma once

namespace SDK::Utils {

template<typename T>
class IFilter {
public:
    virtual ~IFilter() = default;
    virtual bool Pass(const T& value) const = 0;
};

} // namespace SDK::Utils

