#pragma once

#include "../Core/Objects.h"

#include <type_traits>
#include <vector>

namespace SDK::ObjectManager {

namespace detail {
    inline ::Core::Objects::ObjectType InferExtendedType(uintptr_t address) {
        const auto type = ::Core::ObjectManager::InferLifecycleType(address);
        return type == ::Core::Objects::ObjectType::Unknown
            ? ::Core::Objects::ObjectType::GameObject
            : type;
    }

    template <typename T>
    inline ::Core::Objects::ObjectType StaticType() {
        if constexpr (std::is_same_v<T, AIHeroClient>) {
            return ::Core::Objects::ObjectType::AIHeroClient;
        } else if constexpr (std::is_same_v<T, AIMinionClient>) {
            return ::Core::Objects::ObjectType::AIMinionClient;
        } else if constexpr (std::is_same_v<T, AITurretClient>) {
            return ::Core::Objects::ObjectType::AITurretClient;
        } else if constexpr (std::is_same_v<T, MissileClient>) {
            return ::Core::Objects::ObjectType::MissileClient;
        } else if constexpr (std::is_same_v<T, BarracksDampenerClient>) {
            return ::Core::Objects::ObjectType::BarracksDampenerClient;
        } else if constexpr (std::is_same_v<T, HQClient>) {
            return ::Core::Objects::ObjectType::HQClient;
        } else if constexpr (std::is_same_v<T, ShopClient>) {
            return ::Core::Objects::ObjectType::ShopClient;
        } else if constexpr (std::is_same_v<T, Obj_SpawnPoint>) {
            return ::Core::Objects::ObjectType::Obj_SpawnPoint;
        } else if constexpr (std::is_same_v<T, EffectEmitter>) {
            return ::Core::Objects::ObjectType::EffectEmitter;
        } else if constexpr (std::is_same_v<T, GameObject>) {
            return ::Core::Objects::ObjectType::GameObject;
        } else {
            return ::Core::Objects::ObjectType::Unknown;
        }
    }

    template <typename T>
    inline bool Matches(::Core::Objects::ObjectType type) {
        if constexpr (std::is_same_v<T, GameObject>) {
            return type != ::Core::Objects::ObjectType::Unknown;
        } else if constexpr (std::is_same_v<T, AttackableUnit>) {
            return ::Core::Objects::IsAttackable(type);
        } else if constexpr (std::is_same_v<T, AIBaseClient>) {
            return ::Core::Objects::IsAIBase(type);
        } else {
            return type == StaticType<T>();
        }
    }

    template <typename T>
    inline T Make(uintptr_t address, ::Core::Objects::ObjectType type) {
        auto handle = ::Core::ObjectManager::MakeHandle(address, type);
        if constexpr (std::is_same_v<T, GameObject> ||
                      std::is_same_v<T, AttackableUnit> ||
                      std::is_same_v<T, AIBaseClient>) {
            return T(handle);
        } else {
            return T(handle);
        }
    }

    template <typename T>
    inline void AddIfMatches(std::vector<T>& output, uintptr_t address) {
        if (!Globals::IsValidPtr(address)) {
            return;
        }

        const ::Core::Objects::ObjectType type = InferExtendedType(address);
        if (!Matches<T>(type)) {
            return;
        }
        output.push_back(Make<T>(address, type));
    }

    template <typename T>
    inline std::vector<T> EnumerateSpecificManager() {
        std::vector<T> result;
        uintptr_t entries[8192] = {};
        int count = 0;
        if constexpr (std::is_same_v<T, AIHeroClient>) {
            count = ::Core::ObjectManager::EnumerateHeroes(entries, 8192);
        } else if constexpr (std::is_same_v<T, AIMinionClient>) {
            count = ::Core::ObjectManager::EnumerateMinions(entries, 8192);
        } else if constexpr (std::is_same_v<T, AITurretClient>) {
            count = ::Core::ObjectManager::EnumerateTurrets(entries, 8192);
        } else if constexpr (std::is_same_v<T, MissileClient>) {
            count = ::Core::ObjectManager::EnumerateMissiles(entries, 8192);
        }

        result.reserve(count > 0 ? static_cast<std::size_t>(count) : 0);
        for (int i = 0; i < count; ++i) {
            AddIfMatches(result, entries[i]);
        }
        return result;
    }

    template <typename T>
    inline std::vector<T> EnumerateAllObjects() {
        std::vector<T> result;
        uintptr_t entries[16384] = {};
        const int count = ::Core::ObjectManager::EnumerateAll(entries, 16384);
        result.reserve(count > 0 ? static_cast<std::size_t>(count) : 0);
        for (int i = 0; i < count; ++i) {
            AddIfMatches(result, entries[i]);
        }
        return result;
    }
} // namespace detail

inline AIHeroClient Player() {
    return AIHeroClient(::Core::ObjectManager::PlayerHandle());
}

template <typename T>
inline T GetUnitByIndex(int index) {
    const uintptr_t address = ::Core::ObjectManager::FindByIndex(static_cast<std::uint32_t>(index));
    if (!Globals::IsValidPtr(address)) {
        return T{};
    }

    const auto type = detail::InferExtendedType(address);
    return detail::Matches<T>(type) ? detail::Make<T>(address, type) : T{};
}

template <typename T>
inline T GetUnitByNetworkId(int networkId) {
    const uintptr_t address = ::Core::ObjectManager::FindByNetworkId(static_cast<std::uint32_t>(networkId));
    if (!Globals::IsValidPtr(address)) {
        return T{};
    }

    const auto type = detail::InferExtendedType(address);
    return detail::Matches<T>(type) ? detail::Make<T>(address, type) : T{};
}

template <typename T>
inline std::vector<T> Get() {
    if constexpr (std::is_same_v<T, AIHeroClient> ||
                  std::is_same_v<T, AIMinionClient> ||
                  std::is_same_v<T, AITurretClient> ||
                  std::is_same_v<T, MissileClient>) {
        return detail::EnumerateSpecificManager<T>();
    } else {
        return detail::EnumerateAllObjects<T>();
    }
}

} // namespace SDK::ObjectManager

#include "GameObjects.h"
