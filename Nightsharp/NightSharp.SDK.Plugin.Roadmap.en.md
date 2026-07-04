# NightSharp SDK and External Plugin Roadmap

## Goal

NightSharp should move toward three clearly separated layers:

- `NightSharp`: the main runtime/core, responsible for hooks, overlay, loader, offsets, crash handling, and internal bridges.
- `NightSharp.SDK`: the shared SDK/developer package used by Visual Studio when building standalone plugins.
- External plugins: each plugin is built as its own DLL, then packaged into `.NS` for distribution.

This direction lets people work independently by module. One person can build Evade, another Awareness, another Champion Logic, and another Utility tooling without touching the core source. Git merges become much easier because plugin logic no longer lives inside the core project.

The current `NightSharp/SDK` is the source of truth. `NightSharp.SDK` should not rewrite that SDK; it should package, export, and stabilize the public surface from that folder.

Important detail: `NightSharp.dll` still needs some default SDK runtime modules such as Orbwalker, TargetSelector, Prediction, and other required base services. `NightSharp.SDK` is a separate developer package for building plugins, but it does not replace the default SDK runtime that runs inside `NightSharp.dll`.

## Architecture Principles

- Plugin authors should only use the public SDK, preferably through `SDK/SDK.h` and the script-friendly facade.
- Plugins should not include `NightSharp/Core` directly and should not call native internal wrappers directly.
- NightSharp core owns hooks, game memory, object cache, renderer, menu root, crash reporting, and plugin loading.
- NightSharp core still loads default SDK runtime modules so plugins have a working base when no override plugin exists.
- `NightSharp.SDK` is the contract between core and plugins: clear version, clear ABI, clear toolset.
- Every plugin has its own `InternalId`, menu root, config, and log scope.
- Dev builds may load raw DLLs in developer mode for fast debugging.
- Release builds should go through the packager and become `.NS`; the runtime should load valid `.NS` packages by default.

## Default SDK Runtime Modules and Override Model

NightSharp still keeps `NightSharp/SDK` inside the runtime to provide default modules. Modules like Orbwalker, TargetSelector, and Prediction should be treated as default SDK providers:

- If no override plugin exists, NightSharp uses the default provider inside `NightSharp.dll`.
- If a developer creates a custom Orbwalker/TargetSelector/Prediction, that plugin registers a new provider through an SDK registry/provider interface.
- Provider overrides must have clear priority, clear versioning, and a fallback path to the default provider.
- Only one primary provider should be active for a service at a time, such as one main Orbwalker or one main TargetSelector.
- The menu should show which provider is active and allow disabling the override for debugging.
- Override should not mean editing core source or hooking private functions directly; it must go through the public SDK contract.

This gives NightSharp a default SDK that works immediately for all plugins while still allowing advanced developers to replace specific modules. This is better than moving all Orbwalker/TargetSelector/Prediction logic out of core on day one.

## Toolchain and Build Contract

- Visual Studio path from AGENTS: `E:\Visual Studio`.
- Platform toolset: `v145`.
- Default target: x64.
- The plugin template must match the runtime CRT/runtime flags to avoid ABI and unload issues.
- The SDK package needs a version, ABI id, and compatibility range.
- If a plugin does not match the SDK/ABI, NightSharp should disable it and write a clear log instead of crashing.

## Target Layout

- `NightSharp/`: the current injected/runtime project.
- `NightSharp/SDK/`: the completed SDK source of truth, still used by `NightSharp.dll` for default runtime modules.
- `NightSharp.SDK/`: the Visual Studio developer package/project.
- `NightSharp.Plugin.Template/`: template for new plugins.
- `NightSharp.Plugin.Packager/`: tool that packages raw DLLs into `.NS`.
- `%AppData%\NightSharp\Plugins\`: runtime folder for release plugins.
- `%AppData%\NightSharp\Plugins\Dev\`: raw DLL folder used only when developer mode is enabled.

## Runtime Loading Model

1. NightSharp initializes core and internal SDK wrappers.
2. The plugin loader scans `%AppData%\NightSharp\Plugins`.
3. The loader reads plugin metadata: id, name, author, version, category, target SDK, and dependencies.
4. For `.NS`, the loader verifies the package, checks compatibility, decrypts into memory, and loads the module.
5. For raw DLLs in `Dev`, the loader only loads them when developer mode is enabled.
6. The loader binds the plugin to the SDK host API and registers it into PluginRegistry/Menu.
7. PluginManager calls lifecycle callbacks: load, unload, update, render, menu, and events.
8. If a plugin crashes inside a callback, NightSharp disables that plugin, writes logs/dumps, and keeps running when possible.

## NightSharp.SDK Developer Package

`NightSharp.SDK` should feel similar in spirit to `EnsoulSharp.SDK`: a developer creates a plugin project, adds the SDK dependency, and builds an independent DLL.

It should include:

- Public headers sourced directly from `NightSharp/SDK`.
- An import library or host bridge for plugin-to-runtime calls.
- Version metadata so plugins know which SDK they were built against.
- Documentation for public modules: Game, ObjectManager, GameObjects, Spell, Orbwalker, TargetSelector, Prediction, Damage, Menu, Drawing, Events, and Utilities.
- Interfaces/contracts for plugins to register override providers for Orbwalker, TargetSelector, Prediction, or other SDK services.
- Plugin templates for Utility, Champion, Awareness, and Evade.
- A small sample plugin for build/load verification, followed by a champion logic sample.

The public SDK surface should keep the existing script-friendly style:

- `Spell`, `SpellBookClient`, prediction types, `IsValidTarget`, `Keys`, menu/keybind APIs, and event args.
- Plugin-facing spell casting goes through SDK wrappers, not directly into `CoreCastSpell`.
- The facade remains the short-name layer that keeps plugin code compact and script-like.

## External Plugin Contract

Every plugin needs a clear contract:

- Unique internal id, such as `awareness.main`, `evade.core`, or `champion.ezreal`.
- Plugin category: Core, Champion, Utility, Misc, or a future extended category.
- SDK target version and ABI id.
- Optional champion name if the plugin is champion-specific.
- Dependencies if the plugin needs another module.
- Clear lifecycle callbacks.
- No exceptions crossing the plugin-host boundary.
- No keeping SDK/game object pointers after unload when those objects are owned by the host.
- Plugins must release menus, event subscriptions, and resources during unload.

## Merge-Conflict Model

The main goal is independent work:

- Evade lives in its own repository/project.
- Awareness lives in its own repository/project.
- Champion logic lives in its own repository/project or is split per champion.
- Utility/debug tools live in their own repository/project.
- Core NightSharp only changes when the shared public SDK contract needs a new API.
- Every public SDK change should include a version note, compatibility note, and minimum sample/test coverage.

Rule: if a feature only serves one plugin, it belongs in that plugin. It should move into core/SDK only when it is a shared capability for multiple plugins.

## `.NS` Packaging

Suggested pipeline:

1. Developer builds a raw plugin DLL with Visual Studio.
2. Packager reads the DLL and manifest/metadata.
3. Packager creates `.NS` with payload, metadata, hash/signature, and compatibility info.
4. Runtime only loads `.NS` when the package is valid and matches SDK/ABI.
5. Developer mode can still load raw DLLs for debugging, but release distribution should use `.NS`.

The goal of `.NS` is packaging, integrity checking, version management, and distribution protection. Encryption/packer/VMP can be a separate release-hardening layer, but it should not break the developer workflow.

## Roadmap Phases

### Phase 0 - Freeze the Contract on Paper

- Define the boundary between NightSharp core, NightSharp.SDK, and external plugins.
- Choose the SDK versioning format.
- Choose the plugin metadata format.
- Choose the loader folder: `%AppData%\NightSharp\Plugins`.
- Choose the developer folder: `%AppData%\NightSharp\Plugins\Dev`.
- Define raw DLL dev mode and `.NS` release mode.

### Phase 1 - Create the NightSharp.SDK Package

- Create the `NightSharp.SDK` project/package using `NightSharp/SDK` as the source of truth.
- Make Visual Studio able to reference the SDK and build independent plugin DLLs.
- Support Debug x64 and Release x64.
- Use toolset `v145`.
- Add SDK version and ABI id.
- Add basic public API documentation.

### Phase 2 - Plugin Template and Samples

- Create a minimal plugin template.
- Create a Utility sample plugin to verify load/update/render/menu.
- Create a Champion sample plugin to verify spell, target selector, orbwalker, prediction, and menu.
- Ensure samples do not modify core source.

### Phase 3 - External Loader Dev Mode

- NightSharp scans the Dev folder and loads raw DLLs when developer mode is enabled.
- Validate SDK version before calling plugin code.
- Register external plugins into the same menu/plugin registry used by internal plugins.
- Add clear logs for load, failure, and unload steps.

### Phase 4 - Runtime Safety

- Add crash isolation for OnLoad, OnUnload, OnUpdate, OnRender, OnMenu, and event callbacks.
- Disable a crashing plugin instead of taking down all of NightSharp when possible.
- Make unload safe: events, menus, resources, and background jobs must be cleaned up.
- Track performance timing per plugin to identify FPS drops.

### Phase 5 - `.NS` Packager and Release Loading

- Build the packager that converts raw DLLs into `.NS`.
- Add metadata, hash/signature, SDK compatibility, and dependency list.
- Make runtime load `.NS` by default.
- Keep raw DLL loading for developer mode only.
- Add UI/log errors for wrong version, wrong signature, or missing dependency.

### Phase 6 - Move Internal Plugins Out

- Move debug/test plugins into external sample/dev plugins.
- Move Awareness/Utility into external plugins.
- Move Champion Logic into external plugins per champion/module.
- Keep core focused on SDK wrappers, loader, plugin manager, overlay/menu, and hook/event bridge.

### Phase 7 - Public Dev Kit

- Write documentation for creating a new plugin.
- Write documentation for public SDK modules.
- Write documentation for `.NS` packaging.
- Create a release zip for NightSharp.SDK + template + samples.
- Create a compatibility matrix by SDK version and game build.

### Phase 8 - Lua/Transpiler Later

Lua or Lua-to-C++ transpilation should not be the first phase. The C++ SDK/plugin ABI should be stable first, then later consider:

- Lua runtime/plugin adapter.
- Lua-to-C++ transpiler.
- Script marketplace/import layer.

Doing Lua too early will make later API changes more expensive.

## Acceptance Criteria

- A developer can clone the template, reference `NightSharp.SDK`, and build a standalone plugin DLL with Visual Studio `v145`.
- A plugin can load in developer mode without modifying `NightSharp.vcxproj`.
- A plugin can be packaged into `.NS` and loaded from `%AppData%\NightSharp\Plugins`.
- SDK/ABI mismatch is rejected with clear logs and does not crash NightSharp.
- Evade, Awareness, and Champion Logic can be developed in separate repositories/projects.
- Core merge conflicts are reduced because plugin logic no longer lives inside NightSharp core.

## Not for Version One

- Do not start with Lua/transpilation.
- Do not expose `Core/*` directly to plugin authors.
- Do not let plugins call native internal functions outside the SDK contract.
- Do not unload plugins until cleanup/event unsubscription is well-defined.
- Do not require packer/encryption on day one because it will slow down development and debugging.

## Conclusion

The correct first step is to turn `NightSharp.SDK` into a stable developer kit, move plugins outside core through raw DLL developer mode, and only then add `.NS` packaging. Once the SDK contract is stable, the team can split work by plugin like a Hanbot/EloBuddy-style ecosystem while keeping NightSharp's current C++ SDK advantages.
