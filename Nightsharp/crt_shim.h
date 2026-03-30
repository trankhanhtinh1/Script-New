#pragma once
/*
 * NightSharp v2.0 — CRT Shim / Custom Heap Allocator
 *
 * Provides HeapAlloc-based memory allocation for ImGui,
 * bypassing the CRT heap which may not be initialized
 * in a manual-mapped DLL.
 *
 * Strategy:
 *   - Keep /MT (static CRT linked) — pure CRT functions like
 *     memcpy, memset, strlen still work without _initterm
 *   - Override ImGui's allocator with HeapAlloc/HeapFree
 *   - Use stb_sprintf for printf/snprintf (avoid CRT locale state)
 *   - Use ns_sscanf for sscanf (avoid CRT locale state)
 *   - Use ns_strtod for strtod (avoid CRT locale state)
 *   - Disable ImGui file I/O and ini settings
 *   - Menu code uses only const char* and fixed arrays (no std:: containers)
 */

#include <Windows.h>

// ============================================================================
// Private heap for ImGui and menu allocations
// ============================================================================
inline HANDLE g_nsHeap = nullptr;

inline void NsHeapInit() {
    if (!g_nsHeap)
        g_nsHeap = HeapCreate(0, 0, 0);
}

inline void NsHeapDestroy() {
    if (g_nsHeap) {
        HeapDestroy(g_nsHeap);
        g_nsHeap = nullptr;
    }
}

// ============================================================================
// ImGui allocator callbacks (for ImGui::SetAllocatorFunctions)
// ============================================================================
inline void* NsImGuiAlloc(size_t sz, void* /*user_data*/) {
    return HeapAlloc(g_nsHeap, HEAP_ZERO_MEMORY, sz);
}

inline void NsImGuiFree(void* ptr, void* /*user_data*/) {
    if (ptr) HeapFree(g_nsHeap, 0, ptr);
}

