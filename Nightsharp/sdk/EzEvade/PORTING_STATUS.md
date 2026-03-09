# EzEvade Porting Status

This folder now mirrors the original `EzEvade` structure:

- `Core/`
- `Draw/`
- `EvadeSpells/`
- `Helpers/`
- `SpecialSpells/`
- `Spells/`
- `Tests/`
- `Utils/`

## Source Of Truth

Original C# reference source remains at:

`/EzEvade`

The target folder:

`/ImGui-DirectX-11-Kiero-Hook-master/Nightsharp/sdk/EzEvade`

is now C++-only (all copied `.cs` files were removed).

## Port Coverage

File-name parity with original `EzEvade/*.cs` is complete:

- `Program.h`
- `Core/Evade.h`
- `Core/EvadeHelper.h`
- `Draw/*`
- `EvadeSpells/*` (including `EvadeSpellData.h` compatibility include)
- `Helpers/*` (including `AutoSetPing.h`)
- `SpecialSpells/*` (all champion plugins + `AllChampions`)
- `Spells/*`
- `Tests/*`
- `Utils/*`

Compatibility wrappers are kept at root for old include paths:

- `SpellData.h` -> `Spells/SpellData.h`
- `SpellDatabase.h` -> `Spells/SpellDatabase.h`
- `EvadeSpellDatabase.h` -> `EvadeSpells/EvadeSpellDatabase.h`

## Integration Notes

- `SpellDetector` now loads special spell plugins through `SpecialSpells/SpecialSpellRegistry*`.
- `SpecialSpells` now includes `AllChampions`, `Zed`, `Ziggs`, and `Zilean`.
- C++17 compatibility cleanup done for map/set membership checks (`find(...) != end()`).

## Known Runtime Gaps

- Some C# events do not have 1:1 SDK hooks yet (`GameObject.OnCreate/OnDelete`, `Game.OnWndProc`, packet-level callbacks).
- Those parts are approximated with `OnGameUpdate` polling where needed (trap/object tracking, click-remove).
