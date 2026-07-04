# NightSharp.SDK

`NightSharp.SDK` is the developer package for building external NightSharp
plugins with Visual Studio.

The SDK source of truth remains `../NightSharp/SDK`. This package only provides
the Visual Studio project, include entrypoint, version/ABI metadata, and a
property sheet that plugin projects can import.
`include/NightSharp.SDK.Package.h` defines the shared `.NS` package format used
by the packager and NightSharp runtime loader.

## Build Contract

- Visual Studio: `E:\Visual Studio`
- Platform toolset: `v145`
- Platform: `x64`
- C++ standard: `stdcpp20`
- Runtime: `/MT` for Release and `/MTd` for Debug
- SDK version: `1.0.0`
- SDK ABI id: `NightSharp.SDK/1.0.0/abi-1`

## Developer Usage

External plugin projects should import:

```xml
<Import Project="..\NightSharp.SDK\build\NightSharp.SDK.props" />
```

Then plugin code can include:

```cpp
#include <NightSharp.SDK.h>
```

The entrypoint forwards to the existing `NightSharp/SDK/SDK.h` facade, so plugin
code uses the same public surface as internal NightSharp plugins.

## Boundary

For Phase 1, this package is a compile-time developer SDK. It does not replace
the default SDK runtime modules inside `NightSharp.dll`. Orbwalker,
TargetSelector, Prediction, menu, hooks, object access, and memory-backed data
still live in NightSharp runtime until later external-provider phases are added.

External plugins should depend on this public package instead of including
`NightSharp/Core` directly.
