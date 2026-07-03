#include <NightSharp.SDK.h>

namespace NightSharp::SDKPackage {

const Version& GetVersion() noexcept {
    return SdkVersion;
}

const char* GetVersionString() noexcept {
    return SdkVersionString;
}

const char* GetAbiId() noexcept {
    return SdkAbiId;
}

} // namespace NightSharp::SDKPackage
