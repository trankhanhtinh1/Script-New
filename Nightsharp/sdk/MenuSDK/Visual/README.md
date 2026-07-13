# VisualSDK

`VisualSDK` is the MenuSDK-owned drawing layer for frame-sorted world and screen visuals.

## Owner lifecycle

```cpp
constexpr const char* kOwner = "champion.example";
auto visuals = NightSharp::Menu::VisualSDK::Instance().Owner(kOwner, 100);

NightSharp::Menu::VisualSDK::Instance().ReleaseOwner(kOwner);
```

Frame commands do not need handles. Persistent commands must keep the returned `VisualHandle`; resetting or destroying the handle removes the command.

## Smooth world range

```cpp
NightSharp::Menu::VisualStyle style;
style.color = 0xFF00FFFFu;
style.thickness = 1.75f;
style.layer = NightSharp::Menu::VisualLayer::World;
visuals.CircleWorld(player.Position(), spell.Range, style);
```

Leaving `style.segments` at `0` enables adaptive segment quality.

## Champion label

```cpp
NightSharp::Menu::VisualTarget target;
target.position = enemy.Position();
target.hpBarHeight = enemy.GetHpBarHeight();

NightSharp::Menu::VisualLabelStyle style;
style.color = 0xFFFFFFFFu;
style.anchor = NightSharp::Menu::VisualAnchor::AboveHead;
style.offset = Vec2{ 0.0f, -12.0f };
visuals.Label(target, "Target", style);
```

## Combat primitives

`VisualContext` supports screen/world lines, polylines, circles, rings, arcs, rectangles, polygons, arrows, text, unit labels, and health bars. Commands are sorted by `VisualLayer`, owner order, command order, then insertion order.
