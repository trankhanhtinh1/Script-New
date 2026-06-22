//-----------------------------------------------------------------------------
// NightSharp - Custom ImGui Configuration
//
// Overlay-only settings:
//   - Custom HeapAlloc allocator is set at runtime.
//   - stb_sprintf is used for formatting.
//   - ns_sscanf/ns_strtod remain for the patched ImGui sources.
//   - File I/O, demo windows, debug tools, asserts, and obsolete APIs are off.
//-----------------------------------------------------------------------------

#pragma once

#include <math.h>
#include <stddef.h>
#include <string.h>

#define IM_ASSERT(_EXPR)  ((void)(_EXPR))

#define IMGUI_DISABLE_DEFAULT_ALLOCATORS
#define IMGUI_USE_STB_SPRINTF
#define IMGUI_DISABLE_FILE_FUNCTIONS
#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DISABLE_DEBUG_TOOLS
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DISABLE_OBSOLETE_KEYIO
#define IMGUI_DEFINE_MATH_OPERATORS

static inline void NsQsort(void* base, size_t count, size_t size, int(__cdecl* cmp)(const void*, const void*)) {
    unsigned char* arr = static_cast<unsigned char*>(base);

    for (size_t i = 1; i < count; ++i) {
        unsigned char tmp[256];

        for (size_t b = 0; b < size; ++b) {
            tmp[b] = arr[i * size + b];
        }

        size_t j = i;
        while (j > 0 && cmp(arr + (j - 1) * size, tmp) > 0) {
            for (size_t b = 0; b < size; ++b) {
                arr[j * size + b] = arr[(j - 1) * size + b];
            }
            --j;
        }

        for (size_t b = 0; b < size; ++b) {
            arr[j * size + b] = tmp[b];
        }
    }
}

#define ImQsort NsQsort

#define IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS

#ifdef _MSC_VER
#pragma intrinsic(sin, cos, sqrt, fabs, ceil)
#pragma intrinsic(memset, memcpy, memcmp, strcmp, strlen)
#endif

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

#if (defined __SSE__ || defined __x86_64__ || defined _M_X64 || (defined(_M_IX86_FP) && (_M_IX86_FP >= 1))) && !defined(IMGUI_DISABLE_SSE)
#include <immintrin.h>
static inline float  ImRsqrt(float x)           { return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x))); }
#else
static inline float  ImRsqrt(float x)           { return 1.0f / sqrtf(x); }
#endif
static inline double ImRsqrt(double x)          { return 1.0 / sqrt(x); }
