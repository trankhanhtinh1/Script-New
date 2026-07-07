# SharpShooterAIO menu mapping note

## Why the menu did not show

NightSharp's plugin panel maps a loaded plugin to its SDK menu by looking for a
root menu whose `Name` matches either:

- the plugin `InternalId`, or
- the plugin display `Name`.

`SharpShooterAIOPlugin::GetInternalId()` returns:

```cpp
champion.sharpshooteraio
```

The first SharpShooter champion ports created per-champion root menu ids such as:

```cpp
champion.sharpaio.ashe
champion.sharpaio.kogmaw
```

Those ids did not match `champion.sharpshooteraio`, so the plugin loaded but the
NightSharp menu panel could not find the attached SDK menu.

## Fix pattern

For container plugins that dispatch by champion name, every champion-specific
menu root must use the same root id as the container plugin:

```cpp
MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Ashe", true);
```

Keep the display name champion-specific, but keep the root id stable. This is
the same pattern used by 7UPAIO:

```cpp
MenuRoot = new Menu("champion.7upaio", "7UP - Ezreal", true);
```

## When adding new SharpShooter champions

Do:

```cpp
MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - <Champion>", true);
```

Do not:

```cpp
MenuRoot = new Menu("champion.sharpaio.<champion>", "SharpAIO - <Champion>", true);
```
