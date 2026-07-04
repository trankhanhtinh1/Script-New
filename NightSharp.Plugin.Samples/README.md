# NightSharp Plugin Samples

These samples build external DLL plugins against `NightSharp.SDK`.

- `UtilitySample`: lifecycle, update, and render callback smoke sample.
- `ChampionSample`: Ezreal Q skeleton using Spell, GameObjects, and Prediction.
  It calculates the predicted Q cast position but does not call TargetSelector,
  Orbwalker, or the native cast path until the Phase 3 host bridge exists.

Phase 3 will add the runtime loader that consumes `NightSharpGetPluginExports`
and binds menu/drawing services from the NightSharp host instead of giving each
external DLL its own UI state.
