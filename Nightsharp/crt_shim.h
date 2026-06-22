#pragma once
/*
 * NightSharp - CRT Shim / Custom Heap Allocator
 *
 * Provides HeapAlloc-based memory allocation for ImGui. The patched ImGui
 * sources still use ns_sscanf/ns_strtod, so those lightweight headers remain
 * in the overlay-only project.
 */

#include <Windows.h>
#include <cstddef>

inline HANDLE g_nsHeap = nullptr;

inline void NsHeapInit() {
    if (!g_nsHeap) {
        g_nsHeap = HeapCreate(0, 0, 0);
    }
}

inline void NsHeapDestroy() {
    if (g_nsHeap) {
        HeapDestroy(g_nsHeap);
        g_nsHeap = nullptr;
    }
}

inline void* NsImGuiAlloc(size_t sz, void* /*user_data*/) {
    HANDLE heap = g_nsHeap ? g_nsHeap : GetProcessHeap();
    return HeapAlloc(heap, HEAP_ZERO_MEMORY, sz ? sz : 1);
}

inline void NsImGuiFree(void* ptr, void* /*user_data*/) {
    if (!ptr) {
        return;
    }

    HANDLE heap = g_nsHeap ? g_nsHeap : GetProcessHeap();
    HeapFree(heap, 0, ptr);
}
