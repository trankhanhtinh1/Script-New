#pragma once

#include "GameObject.h"
#include <vector>

namespace SDK::GameObjects {

    class LazyObjectList {
    public:
        using Storage = std::vector<GameObject>;
        using iterator = Storage::iterator;
        using const_iterator = Storage::const_iterator;

        LazyObjectList() = default;
        explicit LazyObjectList(void (*loader)()) : m_loader(loader) {}

        void SetLoader(void (*loader)()) { m_loader = loader; }

        void Ensure() const {
            if (m_loader) {
                m_loader();
            }
        }

        iterator begin() { Ensure(); return m_items.begin(); }
        iterator end() { Ensure(); return m_items.end(); }
        const_iterator begin() const { Ensure(); return m_items.begin(); }
        const_iterator end() const { Ensure(); return m_items.end(); }
        const_iterator cbegin() const { Ensure(); return m_items.cbegin(); }
        const_iterator cend() const { Ensure(); return m_items.cend(); }

        size_t size() const { Ensure(); return m_items.size(); }
        bool empty() const { Ensure(); return m_items.empty(); }
        GameObject& operator[](size_t index) { Ensure(); return m_items[index]; }
        const GameObject& operator[](size_t index) const { Ensure(); return m_items[index]; }

        operator const Storage&() const { Ensure(); return m_items; }
        operator Storage&() { Ensure(); return m_items; }

        void clear() { m_items.clear(); }
        void reserve(size_t count) { m_items.reserve(count); }
        void push_back(const GameObject& obj) { m_items.push_back(obj); }
        Storage& Mutable() { return m_items; }

    private:
        mutable Storage m_items;
        void (*m_loader)() = nullptr;
    };

} // namespace SDK::GameObjects
