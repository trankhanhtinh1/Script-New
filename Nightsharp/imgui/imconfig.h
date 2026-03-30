//-----------------------------------------------------------------------------
// NightSharp v2.0 — Custom ImGui Configuration
//
// CRT-safe settings for manual-mapped DLL:
//   - Custom allocator (HeapAlloc, set at runtime)
//   - stb_sprintf for printf (avoids CRT locale state)
//   - sscanf patched directly in ImGui source (no macro)
//   - qsort/atof replaced with inline implementations (no CRT)
//   - No file I/O (no ini save/load)
//   - No demo windows / debug tools
//   - Assert disabled (avoid abort())
//
// CRT elimination strategy (review #21, #22, #23):
//   sscanf:  patched directly in imgui.cpp, imgui_widgets.cpp, imgui_tables.cpp
//   strtod:  not used by ImGui (confirmed by grep)
//   qsort:   replaced below via #define ImQsort
//   atof:    replaced below via IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS
//   printf:  stb_sprintf
//   malloc:  HeapAlloc via SetAllocatorFunctions()
//-----------------------------------------------------------------------------

#pragma once

// Disable asserts (avoid pulling in CRT abort())
#define IM_ASSERT(_EXPR)  ((void)(_EXPR))

// Use HeapAlloc-based allocator — set via ImGui::SetAllocatorFunctions()
#define IMGUI_DISABLE_DEFAULT_ALLOCATORS

// Use stb_sprintf instead of CRT vsnprintf (avoids locale dependency)
#define IMGUI_USE_STB_SPRINTF

// Disable file I/O entirely
#define IMGUI_DISABLE_FILE_FUNCTIONS

// Disable demo windows (reduces code size and CRT surface)
#define IMGUI_DISABLE_DEMO_WINDOWS

// Disable debug tools (reduces code size, also eliminates sscanf at imgui.cpp:14497)
#define IMGUI_DISABLE_DEBUG_TOOLS

// Disable obsolete functions
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DISABLE_OBSOLETE_KEYIO

// Enable math operators for ImVec2/ImVec4
#define IMGUI_DEFINE_MATH_OPERATORS

// =========================================================================
// CRT-free qsort replacement (review #23: ImQsort → qsort crash)
// =========================================================================
// ImGui checks #ifndef ImQsort before defining the CRT-backed default.
// We provide a simple insertion sort — sufficient for the small arrays
// ImGui uses (font atlas rect packing, table column sorting, etc.)

static inline void NsQsort(void* base, size_t count, size_t size, int(__cdecl* cmp)(const void*, const void*))
{
    unsigned char* arr = (unsigned char*)base;
    // Insertion sort — O(n^2) but safe and CRT-free.
    // Arrays sorted by ImGui are typically small (<100 elements).
    for (size_t i = 1; i < count; i++)
    {
        // Save arr[i] into a small stack buffer (max element size = 256 bytes, safe for ImGui structs)
        unsigned char tmp[256];
        // Copy element i to tmp
        for (size_t b = 0; b < size; b++)
            tmp[b] = arr[i * size + b];

        size_t j = i;
        while (j > 0 && cmp(arr + (j - 1) * size, tmp) > 0)
        {
            // Shift element j-1 to j
            for (size_t b = 0; b < size; b++)
                arr[j * size + b] = arr[(j - 1) * size + b];
            j--;
        }
        // Place tmp at position j
        for (size_t b = 0; b < size; b++)
            arr[j * size + b] = tmp[b];
    }
}

#define ImQsort NsQsort

// =========================================================================
// CRT-free math function replacements (review #23, #24)
// =========================================================================
// IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS: we provide all Im* math wrappers.
//
// Strategy:
//  - sqrtf/fabsf/sinf/cosf/ceilf: force intrinsic via #pragma intrinsic
//  - powf/logf/fmodf/acosf/atan2f: these are NOT intrinsics on MSVC —
//    but on x64 they are stateless CRT library calls that do NOT require
//    _initterm. They only use the FPU/SSE registers which the OS
//    initializes when creating the thread.
//  - ImAtof: replaced with ns_strtod (CRT-free)
//
// Note: <math.h> functions on x64 MSVC are stateless — they do NOT
// access CRT heap, locale, or any global state initialized by _initterm.
// They are safe to call from a manual-mapped DLL with /MT + no CRT init.

#define IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS

#include <math.h>

// Force compiler intrinsics for functions that support it
#ifdef _MSC_VER
#pragma intrinsic(sin, cos, sqrt, fabs, ceil)
#pragma intrinsic(memset, memcpy, memcmp, strcmp, strlen)
#endif

// Include our CRT-free strtod for ImAtof
#include "../ns_strtod.h"

#define ImFabs(X)           fabsf(X)
#define ImSqrt(X)           sqrtf(X)
#define ImFmod(X, Y)        fmodf((X), (Y))
#define ImCos(X)            cosf(X)
#define ImSin(X)            sinf(X)
#define ImAcos(X)           acosf(X)
#define ImAtan2(Y, X)       atan2f((Y), (X))
#define ImAtof(STR)         ((float)ns_strtod((STR), 0))
#define ImCeil(X)           ceilf(X)

static inline float  ImPow(float x, float y)    { return powf(x, y); }
static inline double ImPow(double x, double y)  { return pow(x, y); }
static inline float  ImLog(float x)             { return logf(x); }
static inline double ImLog(double x)            { return log(x); }
static inline int    ImAbs(int x)               { return x < 0 ? -x : x; }
static inline float  ImAbs(float x)             { return fabsf(x); }
static inline double ImAbs(double x)            { return fabs(x); }
static inline float  ImSign(float x)            { return (x < 0.0f) ? -1.0f : (x > 0.0f) ? 1.0f : 0.0f; }
static inline double ImSign(double x)           { return (x < 0.0) ? -1.0 : (x > 0.0) ? 1.0 : 0.0; }

// SSE detection — same condition as imgui_internal.h
#if (defined __SSE__ || defined __x86_64__ || defined _M_X64 || (defined(_M_IX86_FP) && (_M_IX86_FP >= 1))) && !defined(IMGUI_DISABLE_SSE)
#include <immintrin.h>
static inline float  ImRsqrt(float x)           { return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x))); }
#else
static inline float  ImRsqrt(float x)           { return 1.0f / sqrtf(x); }
#endif
static inline double ImRsqrt(double x)          { return 1.0 / sqrt(x); }

