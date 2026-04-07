// ============================================================================
// nightsharp_plugin.h — NightSharp External Plugin Header (Shellcode Architecture)
//
// Dev include header này trong main.cpp để viết plugin cho NightSharp.
// Plugin được compile thành position-independent code (shellcode) bởi NightBuild,
// sau đó đóng gói thành .night và load bởi NightSharp runtime.
//
// Plugin KHÔNG phải là DLL/EXE — nó là raw code chạy trong context NightSharp.
// Mọi SDK function gọi qua NightSharpAPI function table.
//
// Usage:
//   #include <nightsharp_plugin.h>
//
//   // Khai báo plugin info
//   NIGHT_PLUGIN_INFO(
//       "champion_ezreal",          // id
//       "Ezreal",                    // name
//       "7UP",                       // author
//       "Ezreal AIO combo/harass",   // description
//       "1.0.0",                     // version
//       NIGHT_TYPE_CHAMPION,         // type
//       "Ezreal"                     // supported champions (varargs)
//   );
//
//   // Optional: register extra source files
//   NIGHT_REGISTER_FILES("spells/q.cpp", "spells/w.cpp");
//
//   // Plugin entry point — receives SDK API table
//   NIGHT_EXPORT void NightPlugin_OnLoad(const NightSharpAPI* api) {
//       // Store api pointer, create menu, setup spells
//   }
//
//   NIGHT_EXPORT void NightPlugin_OnUnload() { }
//   NIGHT_EXPORT void NightPlugin_OnUpdate() { }
//   NIGHT_EXPORT void NightPlugin_OnRender() { }
// ============================================================================

#pragma once

#include "NightSharpAPI.h"
#include <cstdint>
#include <cstring>

// ============================================================================
// API Version
// ============================================================================
#define NIGHTSHARP_PLUGIN_API_VERSION  1

// ============================================================================
// Plugin Types
// ============================================================================
#define NIGHT_TYPE_CORE      0
#define NIGHT_TYPE_CHAMPION  1
#define NIGHT_TYPE_UTILITY   2
#define NIGHT_TYPE_MISC      3

// ============================================================================
// Plugin Info Structure (parsed by night-build at compile time AND at runtime)
// ============================================================================
struct NightPluginInfo {
    uint32_t  ApiVersion;
    char      Id[64];
    char      Name[128];
    char      Author[64];
    char      Description[256];
    char      Version[32];
    uint32_t  Type;
    uint32_t  SupportedChampionCount;
    char      SupportedChampions[8][32];
};

// ============================================================================
// File Registration (parsed by night-build at compile time)
// ============================================================================
#define NIGHT_MAX_FILES 32

struct NightFileList {
    uint32_t  Count;
    char      Files[NIGHT_MAX_FILES][260];
};

// ============================================================================
// Export Marker
//
// For shellcode plugins, there are no DLL exports. Instead, night-build
// scans the compiled binary for known function signatures/symbols.
// The NIGHT_EXPORT macro marks functions so they can be found.
// In practice it just ensures the function uses C linkage (no mangling).
// ============================================================================
#define NIGHT_EXPORT extern "C"

// ============================================================================
// Required Export Function Names (night-build scans for these symbols)
// ============================================================================
#define NIGHT_FN_GET_INFO    "NightPlugin_GetInfo"
#define NIGHT_FN_ON_LOAD     "NightPlugin_OnLoad"
#define NIGHT_FN_ON_UNLOAD   "NightPlugin_OnUnload"
#define NIGHT_FN_ON_UPDATE   "NightPlugin_OnUpdate"
#define NIGHT_FN_ON_RENDER   "NightPlugin_OnRender"

// ============================================================================
// Function Typedefs (NightSharp runtime uses these to call plugin functions)
// ============================================================================
typedef int  (*NightPlugin_GetInfoFn)(NightPluginInfo* outInfo);
typedef void (*NightPlugin_OnLoadFn)(const NightSharpAPI* api);
typedef void (*NightPlugin_OnUnloadFn)();
typedef void (*NightPlugin_OnUpdateFn)();
typedef void (*NightPlugin_OnRenderFn)();

// ============================================================================
// Convenience Macro: NIGHT_PLUGIN_INFO(...)
//
// Declares static plugin info + auto-generates NightPlugin_GetInfo export.
// ============================================================================
#define _NIGHT_CHAMP_0()
#define _NIGHT_CHAMP_1(c1) \
    strncpy_s(g_nightInfo.SupportedChampions[0], 32, c1, _TRUNCATE); \
    g_nightInfo.SupportedChampionCount = 1;
#define _NIGHT_CHAMP_2(c1, c2) \
    _NIGHT_CHAMP_1(c1) \
    strncpy_s(g_nightInfo.SupportedChampions[1], 32, c2, _TRUNCATE); \
    g_nightInfo.SupportedChampionCount = 2;
#define _NIGHT_CHAMP_3(c1, c2, c3) \
    _NIGHT_CHAMP_2(c1, c2) \
    strncpy_s(g_nightInfo.SupportedChampions[2], 32, c3, _TRUNCATE); \
    g_nightInfo.SupportedChampionCount = 3;

// Overload selector
#define _NIGHT_GET_CHAMP_MACRO(_1,_2,_3,NAME,...) NAME
#define _NIGHT_SET_CHAMPS(...) \
    _NIGHT_GET_CHAMP_MACRO(__VA_ARGS__, _NIGHT_CHAMP_3, _NIGHT_CHAMP_2, _NIGHT_CHAMP_1)(__VA_ARGS__)

#define NIGHT_PLUGIN_INFO(id, name, author, desc, ver, type, ...) \
    static NightPluginInfo g_nightInfo = {}; \
    static bool g_nightInfoInit = false; \
    static void _NightInitInfo() { \
        if (g_nightInfoInit) return; \
        g_nightInfoInit = true; \
        memset(&g_nightInfo, 0, sizeof(g_nightInfo)); \
        g_nightInfo.ApiVersion = NIGHTSHARP_PLUGIN_API_VERSION; \
        strncpy_s(g_nightInfo.Id, 64, id, _TRUNCATE); \
        strncpy_s(g_nightInfo.Name, 128, name, _TRUNCATE); \
        strncpy_s(g_nightInfo.Author, 64, author, _TRUNCATE); \
        strncpy_s(g_nightInfo.Description, 256, desc, _TRUNCATE); \
        strncpy_s(g_nightInfo.Version, 32, ver, _TRUNCATE); \
        g_nightInfo.Type = (type); \
        _NIGHT_SET_CHAMPS(__VA_ARGS__) \
    } \
    NIGHT_EXPORT int NightPlugin_GetInfo(NightPluginInfo* outInfo) { \
        _NightInitInfo(); \
        if (!outInfo) return -1; \
        *outInfo = g_nightInfo; \
        return 0; \
    }

// ============================================================================
// Convenience Macro: NIGHT_REGISTER_FILES(...)
//
// Declares extra source files (night-build will compile these too).
// Night-build reads this at source level (text parsing), not at runtime.
// ============================================================================
#define NIGHT_REGISTER_FILES(...) \
    static const char* g_nightFiles[] = { __VA_ARGS__ }; \
    static const int g_nightFileCount = sizeof(g_nightFiles) / sizeof(g_nightFiles[0]);
