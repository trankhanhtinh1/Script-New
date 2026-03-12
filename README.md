#NightSharp

Changelog 7/3/2026:

- Rework main menu structure to Core => Language, Plugin, Plugin Core, Menu
- Separate plugin classifications (CorePlugin/Champion/Utility/Misc) and update category mappings
- Add plugin autoload controls in PluginManager (set/is/load by name) and default core autoload flow
- Rename BGXMenu to NightSharpMenu and update project includes/filters/references
- Change menu toggle key from CapsLock to F1 while keeping Shift hold behavior
- Unify plugin panel rendering and dynamic sidebar entries for loaded modules
- Update Orbwalker/TargetSelector plugin categories to CorePlugin
- Improve Orbwalker farming/last-hit logic (ShouldWait, AA impact timing, push target selection, missile speed fallback)
- Extend TargetSelector behavior from C# source (LightSelect, click-select tolerance/range sync, selected-target flow)
- Fix ImGui compatibility for older versions by removing BeginDisabled/EndDisabled and PushItemFlag usage
- Keep default-load UI state via alpha styling only to avoid old ImGui API compile errors"

Changelog 12/3/2026

- bug Ezreal cast Q plant,... (JungleClear)
- wrong method drawing skillshot
- miss Safe Position Solver
- bug don't doge