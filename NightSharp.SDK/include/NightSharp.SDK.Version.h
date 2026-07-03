#pragma once

#include <cstdint>

#define NIGHTSHARP_SDK_VERSION_MAJOR 1
#define NIGHTSHARP_SDK_VERSION_MINOR 0
#define NIGHTSHARP_SDK_VERSION_PATCH 0
#define NIGHTSHARP_SDK_ABI_REVISION 1

#ifndef NIGHTSHARP_SDK_VERSION_STRING
#define NIGHTSHARP_SDK_VERSION_STRING "1.0.0"
#endif

#ifndef NIGHTSHARP_SDK_ABI_ID
#define NIGHTSHARP_SDK_ABI_ID "NightSharp.SDK/1.0.0/abi-1"
#endif

namespace NightSharp::SDKPackage {

struct Version {
    std::uint16_t Major;
    std::uint16_t Minor;
    std::uint16_t Patch;
    std::uint16_t AbiRevision;
};

inline constexpr Version SdkVersion{
    NIGHTSHARP_SDK_VERSION_MAJOR,
    NIGHTSHARP_SDK_VERSION_MINOR,
    NIGHTSHARP_SDK_VERSION_PATCH,
    NIGHTSHARP_SDK_ABI_REVISION
};

inline constexpr const char* SdkVersionString = NIGHTSHARP_SDK_VERSION_STRING;
inline constexpr const char* SdkAbiId = NIGHTSHARP_SDK_ABI_ID;

} // namespace NightSharp::SDKPackage
