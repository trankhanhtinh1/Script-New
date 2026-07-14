# KuroEvade native port map

The supplied C# Evade project is represented by these native components:

| Supplied source | Native KuroEvade component |
|---|---|
| `Program.cs` | `KuroEvadePlugin.h`, `Native/Engine/EvadeEngine.h` |
| `Collision.cs` | `Native/Engine/Collision.h` |
| `Geometry.cs` | `Native/Engine/Geometry.h` |
| `Evader.cs` | `Native/Engine/Evader.h` |
| `Skillshot.cs` | `Native/Engine/Skillshot.h` |
| `SkillshotDetector.cs` | `Native/Engine/SkillshotDetector.h` |
| `SpellData.cs` | `Native/Database/SpellData.h` |
| `EvadeSpellData.cs` | `Native/Database/EvadeSpellData.h` |
| `Database/SpellDatabase.cs` plus the newer Kuro dataset | `Native/Database/SpellDatabase.h` |
| `Database/EvadeSpellDatabase.cs` | `Native/Database/EvadeSpellDatabase.h` |
| `Database/SpellBlocker.cs` | `Native/Database/SpellBlocker.h` |
| `Config.cs` | `Native/Config/EvadeConfig.h` and the source-shaped menu in `KuroEvadePlugin.h` |
| `Helpers.cs` | `Native/Helpers/Helpers.h` |
| `Utils.cs` | `Native/Helpers/Utils.h` |
| `Benchmarking/Benchmark.cs` | `Native/Benchmarking/Benchmark.h`, including drag capture, Z/X injection, and the recurring five-second spawn schedule |

`App.config`, the C# solution/project files, `AssemblyInfo.cs`, generated
resource files, and `obj/` artifacts are build-system metadata or unused UI
assets. Their native equivalents are the NightSharp Visual C++ project and
plugin metadata in `KuroEvadePlugin.h`; unused source assets are not carried by
the plugin.

KuroEvade includes only its own native headers plus NightSharp Core, SDK, menu,
logging, and the host `IPlugin` interface. Orbwalker coordination lives under
NightSharp `Core/`, not in another plugin.

The root `Enabled` key only controls evade intervention. Skillshot detection,
lifetime updates, per-spell filtering, and ImGui drawing continue while it is
off. Movement, movement interception, attack/spell interception, and evade
spell casts are released immediately.

All NightSharp `MoveTo` requests share the high-level gate in
`Core/CoreControl.h`; accepted movement orders are separated by at least 45 ms
even when several plugins request movement in the same frame.
