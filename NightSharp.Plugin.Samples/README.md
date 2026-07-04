# NightSharp Plugin Samples

These samples build external DLL plugins against `NightSharp.SDK`.

- `UtilitySample`: lifecycle, update, and render callback smoke sample.
- `ChampionSample`: Ezreal Q skeleton using Spell, GameObjects, and Prediction.
  It calculates the predicted Q cast position and verifies SDK-facing champion
  plugin wiring.

Raw DLLs can be loaded from `%AppData%\NightSharp\Plugins\Dev` in developer
mode. Debug builds enable this by default; Release builds require
`NIGHTSHARP_ENABLE_DEV_PLUGINS=1`. For release mode, use
`NightSharp.Plugin.Packager` to create `.NS` packages and place them in
`%AppData%\NightSharp\Plugins`.
