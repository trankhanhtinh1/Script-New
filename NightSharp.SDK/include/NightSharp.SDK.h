#pragma once

// Developer-facing NightSharp SDK entrypoint.
//
// This package intentionally reuses NightSharp/SDK as the source of truth.
// Plugin projects should include this file instead of including runtime
// folders directly.

#include "NightSharp.SDK.Version.h"

#include <SDK/SDK.h>

namespace NightSharp::SDKPackage {

const Version& GetVersion() noexcept;
const char* GetVersionString() noexcept;
const char* GetAbiId() noexcept;

} // namespace NightSharp::SDKPackage
