#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <type_traits>
#include <vector>

namespace SDK {

enum class CollisionableObjects : std::int32_t {
    Minions = 1 << 0,
    Heroes = 1 << 1,
    YasuoWall = 1 << 2,
    BraumShield = 1 << 3,
    Walls = 1 << 4,
    SamiraWall = 1 << 5,
    MelWall = 1 << 6,
    Building = 1 << 7,
};

using CollisionObjects = CollisionableObjects;

constexpr CollisionableObjects operator|(CollisionableObjects lhs, CollisionableObjects rhs) {
    using U = std::underlying_type_t<CollisionableObjects>;
    return static_cast<CollisionableObjects>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

constexpr CollisionableObjects operator&(CollisionableObjects lhs, CollisionableObjects rhs) {
    using U = std::underlying_type_t<CollisionableObjects>;
    return static_cast<CollisionableObjects>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

constexpr CollisionableObjects operator~(CollisionableObjects value) {
    using U = std::underlying_type_t<CollisionableObjects>;
    return static_cast<CollisionableObjects>(~static_cast<U>(value));
}

inline CollisionableObjects& operator|=(CollisionableObjects& lhs, CollisionableObjects rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline CollisionableObjects& operator&=(CollisionableObjects& lhs, CollisionableObjects rhs) {
    lhs = lhs & rhs;
    return lhs;
}

constexpr bool HasFlag(CollisionableObjects value, CollisionableObjects flag) {
    using U = std::underlying_type_t<CollisionableObjects>;
    return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
}

inline CollisionableObjects ToCollisionObjectFlags(const std::vector<CollisionableObjects>& objects) {
    CollisionableObjects flags = static_cast<CollisionableObjects>(0);
    for (const auto object : objects) {
        flags |= object;
    }
    return flags;
}

inline std::vector<CollisionableObjects> ToCollisionObjectArray(CollisionableObjects flags) {
    std::vector<CollisionableObjects> objects;
    if (HasFlag(flags, CollisionableObjects::Minions)) {
        objects.push_back(CollisionableObjects::Minions);
    }
    if (HasFlag(flags, CollisionableObjects::Heroes)) {
        objects.push_back(CollisionableObjects::Heroes);
    }
    if (HasFlag(flags, CollisionableObjects::YasuoWall)) {
        objects.push_back(CollisionableObjects::YasuoWall);
    }
    if (HasFlag(flags, CollisionableObjects::BraumShield)) {
        objects.push_back(CollisionableObjects::BraumShield);
    }
    if (HasFlag(flags, CollisionableObjects::Walls)) {
        objects.push_back(CollisionableObjects::Walls);
    }
    if (HasFlag(flags, CollisionableObjects::SamiraWall)) {
        objects.push_back(CollisionableObjects::SamiraWall);
    }
    if (HasFlag(flags, CollisionableObjects::MelWall)) {
        objects.push_back(CollisionableObjects::MelWall);
    }
    if (HasFlag(flags, CollisionableObjects::Building)) {
        objects.push_back(CollisionableObjects::Building);
    }
    return objects;
}

struct CollisionObjectsBridge {
    CollisionableObjects Flags =
        CollisionableObjects::Minions | CollisionableObjects::YasuoWall;
    std::vector<CollisionableObjects> Values =
        ToCollisionObjectArray(Flags);

    CollisionObjectsBridge() = default;

    CollisionObjectsBridge(CollisionableObjects flags) {
        Set(flags);
    }

    CollisionObjectsBridge(std::initializer_list<CollisionableObjects> objects) {
        Set(std::vector<CollisionableObjects>(objects));
    }

    explicit CollisionObjectsBridge(const std::vector<CollisionableObjects>& objects) {
        Set(objects);
    }

    CollisionObjectsBridge& operator=(CollisionableObjects flags) {
        Set(flags);
        return *this;
    }

    CollisionObjectsBridge& operator=(std::initializer_list<CollisionableObjects> objects) {
        Set(std::vector<CollisionableObjects>(objects));
        return *this;
    }

    CollisionObjectsBridge& operator=(const std::vector<CollisionableObjects>& objects) {
        Set(objects);
        return *this;
    }

    operator CollisionableObjects() const {
        return Flags;
    }

    operator const std::vector<CollisionableObjects>&() const {
        return Values;
    }

    void Set(CollisionableObjects flags) {
        Flags = flags;
        Values = ToCollisionObjectArray(flags);
    }

    void Set(const std::vector<CollisionableObjects>& objects) {
        Values = objects;
        Flags = ToCollisionObjectFlags(Values);
    }

    CollisionableObjects ToFlags() const {
        return Flags;
    }

    const std::vector<CollisionableObjects>& ToArray() const {
        return Values;
    }

    bool empty() const {
        return Values.empty();
    }

    bool contains(CollisionableObjects object) const {
        return HasFlag(Flags, object);
    }

    std::size_t size() const {
        return Values.size();
    }

    const CollisionableObjects& operator[](std::size_t index) const {
        return Values[index];
    }

    std::vector<CollisionableObjects>::const_iterator begin() const {
        return Values.begin();
    }

    std::vector<CollisionableObjects>::const_iterator end() const {
        return Values.end();
    }

    void clear() {
        Values.clear();
        Flags = static_cast<CollisionableObjects>(0);
    }

    void push_back(CollisionableObjects object) {
        if (!HasFlag(Flags, object)) {
            Values.push_back(object);
            Flags |= object;
        }
    }
};

} // namespace SDK
