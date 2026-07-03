#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cassert>

static inline bool IsCodePtr(void* ptr) {
    constexpr DWORD flags = PAGE_EXECUTE | PAGE_EXECUTE_READ |
                            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(ptr, &mbi, sizeof(mbi)))
        return false;
    return (mbi.State == MEM_COMMIT) && (mbi.Protect & flags) &&
           !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS));
}

class VmtHook {
public:
    VmtHook(void* class_base)
        : m_class(class_base)
    {
        auto& vtbl = *static_cast<void***>(m_class);
        m_old_vmt = vtbl;

        // Count vtable entries (valid code pointers)
        size_t size = 0;
        while (m_old_vmt[size] && IsCodePtr(m_old_vmt[size]))
            ++size;

        // Allocate new vtable (+1 for RTTI slot at index -1)
        m_new_vmt = new void*[size + 1] + 1;
        std::memcpy(m_new_vmt - 1, m_old_vmt - 1, sizeof(void*) * (size + 1));

        // Swap instance to new vtable
        vtbl = m_new_vmt;
    }

    ~VmtHook() {
        if (m_new_vmt) {
            auto& vtbl = *static_cast<void***>(m_class);
            vtbl = m_old_vmt;
            delete[] (m_new_vmt - 1);
            m_new_vmt = nullptr;
        }
    }

    // Hook vtable entry at index, returns original function pointer
    template <typename Fn = uintptr_t>
    Fn Hook(size_t index, void* fn) {
        m_new_vmt[index] = fn;
        return reinterpret_cast<Fn>(m_old_vmt[index]);
    }

    // Convenience: hook using a static method and its m_original member
    template <typename T>
    void Apply(size_t index) {
        T::m_original = Hook<decltype(T::m_original)>(index, &T::hooked);
    }

    void Unhook() {
        auto& vtbl = *static_cast<void***>(m_class);
        vtbl = m_old_vmt;
    }

    void Rehook() {
        auto& vtbl = *static_cast<void***>(m_class);
        vtbl = m_new_vmt;
    }

private:
    void* m_class;
    void** m_old_vmt = nullptr;
    void** m_new_vmt = nullptr;
};
