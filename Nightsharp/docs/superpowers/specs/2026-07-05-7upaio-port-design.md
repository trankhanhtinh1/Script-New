# Design: Port 7UPAIO vào NightSharp

## Mục tiêu

Port toàn bộ 33 champion từ `7UPAIO/Program.cs` (C# EnsoulSharp) sang NightSharp (C++ internal plugin) tại `NightSharp/plugins/Champion/7UPAIO/7UPAIO.h`.

## Kiến trúc

### Pattern: Internal single IPlugin

- **Một plugin duy nhất** `AIO7UPPlugin : public IPlugin` trong `7UPAIO.h`
- Đăng ký qua `PluginManager::Get().Register<AIO7UPPlugin>()` trong `PluginBootstrap.h`
- `GetName()` = `"7UPAIO"` → hiển thị trên menu plugin
- `OnLoad()` đọc champion name → dispatch tới champion file tương ứng (giống `Program.cs`)
- Mỗi champion file tự tạo menu `"7UP - [Champion]"` và tự đăng ký events

### Cấu trúc file

```
NightSharp/plugins/Champion/7UPAIO/
├── 7UPAIO.h           ← AIO7UPPlugin class + dispatch theo champion name
├── Ezreal.h            ← ported (uncomment trong 7UPAIO.h)
├── Jinx.h              ← chưa port (comment trong 7UPAIO.h)
├── Vayne.h             ← chưa port (comment)
└── ... (34 champion files)
```

### 7UPAIO.h — Hub plugin

```cpp
#pragma once
#include "../IPlugin.h"
#include "../../SDK/SDK.h"

namespace Plugins {

class AIO7UPPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "7UPAIO"; }
    const char* GetInternalId() const override { return "champion.7upaio"; }
    PluginCategory GetCategory() const override { return PluginCategory::Champion; }
    bool AutoLoadByDefault() const override { return true; }
    bool CanLoad() const override { return true; }

    void OnLoad() override {
        const std::string champ = ObjectManager::Player().CharacterName();
        if (champ == "Ezreal")      { Ezreal::OnGameLoad(); }
        // else if (champ == "Jinx")  { Jinx::OnGameLoad(); }      // commented
        // else if (champ == "Vayne") { Vayne::OnGameLoad(); }     // commented
        // ... 34 champions, comment hết, uncomment khi port
    }
};

} // namespace Plugins
```

### Champion file (vd Ezreal.h)

```cpp
#pragma once
#include "../../SDK/SDK.h"   // include Facade.h

namespace Ezreal {

static Spell Q, W, E, R;
static Menu* ChampionMenu = nullptr;

static void OnGameLoad() {
    if (ObjectManager::Player().CharacterName() != "Ezreal") return;

    Q = Spell(SpellSlot::Q, 1150.0f);
    Q.SetSkillshot(0.25f, 80.0f, 2000.0f, true, SpellType::Line);

    ChampionMenu = new Menu("7upEzreal", "7UP - Ezreal", true);
    ChampionMenu->Attach();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
}

static void Game_OnUpdate(const GameUpdateEventArgs& args) {
    // port từ Ezreal.cs
}

} // namespace Ezreal
```

- Dùng **Facade.h** → cú pháp gần EnsoulSharp C#: `ObjectManager::Player()`, `Game::Time()`, `Menu`, `MenuBool`, `Spell`, `Events::hook.OnGameUpdate`
- Port trực tiếp từ file `.cs` gốc, giữ thứ tự code và logic
- Mỗi champion tự quản lý menu + events của nó

## Danh sách 33 champion

Tất cả 33 champion từ `Program.cs` được list trong `OnLoad()` dạng comment. Khi port champion nào → tạo file `[Name].h` + uncomment dòng dispatch:

1. Vayne
2. Caitlyn
3. Cassiopeia
4. Darius
5. Ekko
6. Ezreal
7. Hecarim
8. Jax
9. Jayce
10. Jinx
11. KSante
12. Kalista
13. Fizz
14. Karthus
15. KogMaw
16. Leblanc
17. Nami
18. Nautilus
19. Olaf
20. Orianna
21. Rumble
22. Ryze
23. Talon
24. Taliyah
25. Taric
26. Thresh
27. Sejuani
28. Sett
29. Shyvana
30. Sylas
31. Viktor
32. Xerath
33. Zed

### Default case + TrollChat

`Program.cs` có `default` in ra "7UP AIO Does Not Support: [champion]" và gọi `TrollChat.OnGameLoad()` sau switch. Trong port:
- Default: log warning, không crash
- TrollChat: port riêng khi cần, ban đầu comment lại

## Cleanup

- **Xoá** `7UPAIO/Champion/Jinx.h` (port sai dùng external NightSharpAPI, sẽ port lại từ `Jinx.cs` dùng internal SDK)
- `7UPAIO.h` hiện đang rỗng → ghi đè bằng `AIO7UPPlugin`

## Build

- Compile chung trong `NightSharp.dll` (internal plugin)
- Đăng ký trong `PluginBootstrap.h`:
```cpp
#include "Champion/7UPAIO/7UPAIO.h"
// ...
PluginManager::Get().Register<AIO7UPPlugin>();
```

## Quy trình port mỗi champion

1. Đọc file `.cs` gốc trong `7UPAIO/Champion/`
2. Tạo file `[Name].h` trong `NightSharp/plugins/Champion/7UPAIO/`
3. Port logic từ C# sang C++ dùng Facade.h SDK
4. Uncomment dòng dispatch trong `7UPAIO.h`
5. Build + test
