#pragma once

#include "NightSharp.SDK.Version.h"

#include <cstdint>

#if defined(_WIN32)
#define NIGHTSHARP_PLUGIN_API extern "C" __declspec(dllexport)
#define NIGHTSHARP_PLUGIN_CALL __cdecl
#else
#define NIGHTSHARP_PLUGIN_API extern "C"
#define NIGHTSHARP_PLUGIN_CALL
#endif

namespace NightSharp::Plugin {

enum class Category : std::uint32_t {
    Core = 0,
    Champion = 1,
    Utility = 2,
    Misc = 3,
};

struct Descriptor {
    std::uint32_t Size;
    std::uint32_t AbiRevision;
    const char* SdkAbiId;
    const char* Name;
    const char* InternalId;
    const char* Author;
    Category PluginCategory;
    const char* ChampionName;
    bool AutoLoad;
};

using CanLoadFn = bool(NIGHTSHARP_PLUGIN_CALL*)();
using LifecycleFn = void(NIGHTSHARP_PLUGIN_CALL*)();

struct Exports {
    std::uint32_t Size;
    Descriptor Descriptor;
    CanLoadFn CanLoad;
    LifecycleFn OnLoad;
    LifecycleFn OnUnload;
    LifecycleFn OnUpdate;
    LifecycleFn OnRender;
    LifecycleFn OnMenu;
};

using GetExportsFn = const Exports*(NIGHTSHARP_PLUGIN_CALL*)();

} // namespace NightSharp::Plugin

#define NIGHTSHARP_PLUGIN_DESCRIPTOR(nameValue, internalIdValue, authorValue, categoryValue, championNameValue, autoLoadValue) \
    ::NightSharp::Plugin::Descriptor { \
        sizeof(::NightSharp::Plugin::Descriptor), \
        NIGHTSHARP_SDK_ABI_REVISION, \
        NIGHTSHARP_SDK_ABI_ID, \
        nameValue, \
        internalIdValue, \
        authorValue, \
        categoryValue, \
        championNameValue, \
        autoLoadValue \
    }

#define NIGHTSHARP_PLUGIN_EXPORT(descriptorValue, canLoadFn, onLoadFn, onUnloadFn, onUpdateFn, onRenderFn, onMenuFn) \
    NIGHTSHARP_PLUGIN_API const ::NightSharp::Plugin::Exports* NIGHTSHARP_PLUGIN_CALL NightSharpGetPluginExports() noexcept { \
        static const ::NightSharp::Plugin::Exports exports = { \
            sizeof(::NightSharp::Plugin::Exports), \
            descriptorValue, \
            canLoadFn, \
            onLoadFn, \
            onUnloadFn, \
            onUpdateFn, \
            onRenderFn, \
            onMenuFn \
        }; \
        return &exports; \
    }
