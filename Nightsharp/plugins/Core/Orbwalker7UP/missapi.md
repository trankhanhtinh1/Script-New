# Orbwalker7UP — Missing API

## Menu.SetLogo (EnsoulSharp.SDK.Rendering.SpriteRender.CreateLogo)
- **C# source**: `NewOrbwalker.cs` line 98
  ```csharp
  Menu.SetLogo(EnsoulSharp.SDK.Rendering.SpriteRender.CreateLogo(Resource1.Orbwalker));
  ```
- **NightSharp**: `Menu::SetLogo(ImTextureID, float, float)` có tồn tại, nhưng
  không có resource nhúng tương đương `Resource1.Orbwalker` (bitmap nhúng trong
  assembly C#). Bỏ qua khi port — menu sẽ không có logo.
- **Giải pháp**: cần nhúng texture vào binary hoặc load từ file asset ở runtime
  (AssetInstaller). Để sau.

## SpellDataInstClient.TooltipVars (EnsoulSharp) — ĐÃ PORT (Q-name approach)
- **C# source**: `NewOrbwalker.cs` line 355
  ```csharp
  float num2 = GameObjects.Player.Spellbook.GetSpell(SpellSlot.Q).TooltipVars[2];
  // num2 == 1 -> 2500 (Calibrum), 2 -> FLT_MAX (Severum), 3 -> 1500 (Infernum), 4 -> 4000 (Crescendum)
  ```
- **RE finding (MCP IDA + ILSpy)**: EnsoulSharp `TooltipVars` là mảng `float[16]`
  inline trong `SpellDataInst` của client **x86 (32-bit)**. Trên **x64**, offset
  `0x108` (`SpellInstanceVars`) là **CON TRỎ** tới bảng named-var (entry 32-byte,
  có hash key, binary-search — xem `sub_4B1550`/`sub_2198B0`/`sub_273E80`), KHÔNG
  phải mảng float phẳng. Đọc `slot+0x108 + i*4` như Antigravity đã thử = sai (tràn
  field lân cận 0x110/0x118/0x128). Signature scanner cũng ghi rõ: `"mov rax,
  [rcx+imm32] ; SpellSlot instance-vars POINTER"`.
- **Phương án port (tương đương 100% C#)**: game có Q-spell riêng cho từng weapon
  (hash dump: `ApheliosCalibrumQ`/`SeverumQ`/`InfernumQ`/`CrescendumQ`/`GravitumQ`).
  Đọc `player.Spellbook().GetSpell(SpellSlot::Q).Name()` rồi map trực tiếp sang
  missile speed — cùng kết quả logic với C# mà không phụ thuộc offset dễ vỡ.
- **Đã implement**: `Orbwalker7UPImpl::GetProjectileSpeed()` Aphelios branch
  (`Orbwalker7UP.h:1286`). Gravitum không có branch trong C# (fallthrough default) -> 1500.
- **Không expose `TooltipVars[i]` generic**: vì x64 không có mảng float phẳng tương
  đương. Nếu sau này cần var khác index 2, phải RE từng named-var key trong bảng 0x108.

## GameObjects.AzirSoldiers (EnsoulSharp)
- **C# source**: `NewOrbwalker.cs` line 459 (CanAttackWithWindWall, Azir branch);
  line 481 (InCurrentAutoAttackRange Azir branch)
- **NightSharp**: ĐÃ PORT vào `sdk/Extensions/Unit.h`:
  `Extensions::detail::IsAzirSoldierEmitter(emitter)` — port 1-1 từ
  `Regex("Azir_.+_P_Soldier_Ring").IsMatch(name)` (EnsoulSharp.SDK.GameObjects).
  Dùng `prefix + >=1 char + suffix` check thủ công (không cần `<regex>` header).
- **Cách dùng**: duyệt `GameObjects::ParticleEmitters()` + filter qua
  `IsAzirSoldierEmitter` — giống cách EnsoulSharp build HashSet ban đầu + sync
  OnCreate/OnDelete (xem `Extensions::InCurrentAutoAttackRange` Azir branch).

## GameObjects.CaitlynHeadshotBeams (EnsoulSharp)
- **C# source**: EnsoulSharp.SDK.GameObjects (dict NetworkId->list<EffectEmitter>)
  được build khi OnCreate beam (regex `Caitlyn_.+_ace_beam` /
  `Caitlyn_.+_W_E_Tar_Headshot_Beam` / `Caitlyn_.+_LRHeadshotTarget_Beam`),
  filter target có buff `CaitlynWSnare` hoặc `CaitlynEMissile`.
- **NightSharp**: ĐÃ PORT vào `sdk/Extensions/Unit.h`:
  - `Extensions::detail::IsCaitlynHeadshotBeamEmitter(emitter)` — port 3 regex.
  - `Extensions::detail::HasCaitlynHeadshotBeamOn(target)` — kiểm target có
    buff CaitlynWSnare/CaitlynEMissile + có beam active trong ParticleEmitters.
- **Khác vs C#**: C# dùng dict mapping NetworkId->list beam để lookup nhanh.
  Workaround SDK duyệt ParticleEmitters + check buff. Logic kết quả tương đương
  khi Caitlyn là player (caster của snare/EMissile buff là me). Không cần dict.

## AIBaseClient.InCurrentAutoAttackRange(float, bool) (EnsoulSharp)
- **C# source**: `NewOrbwalker.cs` line 481, 484 (GetMinions)
  ```csharp
  m.InCurrentAutoAttackRange(range, true)
  ```
- **NightSharp**: ĐÃ PORT đầy đủ vào `sdk/Extensions/Unit.h`:
  - `Extensions::GetCurrentAutoAttackRange(sender, target)` — port 1-1 từ
    `AIBaseClientExtensions.GetCurrentAutoAttackRange` (gồm Aphelios/Caitlyn
    overrides + `Ping/4` + `-5f`).
  - `Extensions::GetCurrentAutoAttackRange(target)` — port 1-1 từ overload 1-arg.
  - `Extensions::InCurrentAutoAttackRange(target, extraRange, checkTeam)` —
    port 1-1 từ `AttackUnitExtensions.InCurrentAutoAttackRange` (gồm Azir
    special case 770/350).
- **MISSAPI còn lại (workaround trong SDK)**:
  - `GameObjects.AzirSoldiers` — filter `ParticleEmitters()` theo Name chứa
    "azirsoldier" (case-insensitive) — xem `detail::IsAzirSoldierEmitter`.
  - `GameObjects.CaitlynHeadshotBeams` — `detail::HasCaitlynHeadshotBeamOn`
    tạm trả `false` (chưa có cách lấy NetworkId của target tham chiếu từ
    EffectEmitter). Caitlyn 1300 override sẽ không active cho đến khi expose.

## ProcessSpellEventArgs.CastTime (EnsoulSharp)
- **C# source**: `NewOrbwalker.cs` line 670 (OnDoCast)
  ```csharp
  if (this.IsAutoAttackReset(name) && args.CastTime == 0f)
  ```
- **NightSharp**: ĐÃ PORT vào `core/CoreEvents.h`:
  - Field `ProcessSpellEventArgs::CastTime` (float) đã thêm.
  - Decode: `args.CastTime = ReadFloat(castInfo+0x98) + ReadFloat(castInfo+0x9C)`
    (port 1-1 từ `OnDoCastNative`: `castTime = ExtraTimeForCast + DesignerCastTime`).
  - Offset xác nhận qua IDA 13337:
    - `sub_975A00`: `movss xmm0, [rcx+0x98]; ret` → ExtraTimeForCast = 0x98
    - `sub_9B5920`: `movss xmm0, [rcx+0x9C]; ret` → DesignerCastTime = 0x9C
    - `sub_9BAEE0` gọi cả 2 liên tiếp (0x9bb715, 0x9bb720) — khớp pattern C#.

## ImpulseAIO.Common.Base.PlusRender.GetFullColorList (EnsoulSharp)
- **C# source**: `NewOrbwalker.cs` line 885 (OnDraw ForceChase branch)
  ```csharp
  var colorm = ImpulseAIO.Common.Base.PlusRender.GetFullColorList(450);
  CircleRender.Draw(..., colorm[colorindex], 2, false);
  ```
- **NightSharp**: ĐÃ PORT vào `plugins/Core/Orbwalker7UP/Common/Base.h`:
  `Orbwalker7UP::Common::PlusRender::GetFullColorList(totalCount, redToPurple)`
  — port 1-1 từ `Base.cs` line 630-653. Trả `std::vector<std::uint32_t>` ARGB.
  Bao gồm cả `GetSingleColorList` (helper nội suy tuyến tính Red→Yellow→...→Magenta).
- **Affects**: `DrawChaseRange` vẽ circle rainbow quanh player khi ForceChase.

## AIBaseClient.GetPath(Vector3 target) (EnsoulSharp) — ĐÃ PORT (A* trên CoreNavGrid)
- **C# source**: `NewOrbwalker.cs` line 1370 (Move, !highmode branch)
  ```csharp
  Vector3[] path = GameObjects.Player.GetPath(vector);
  ```
- **RE finding (MCP ILSpy)**: EnsoulSharp gọi native `PathController::CreatePath`
  + `SmoothPath` (A* navmesh thật của game). NightSharp chưa bind native này trên
  x64 (cần thêm signature + wrapper, rủi ro crash calling-convention cao).
- **Phương án port (an toàn, fidelity tốt)**: tự implement A* trên `CoreNavGrid`
  (8-directional, Euclidean heuristic, cut-corner prevention, line-of-sight
  smoothing). Không gọi native C++ method -> không rủi ro crash. Đi vòng tường
  thật theo grid (không phải straight-line như bản bị revert).
- **Đã implement**:
  - `CoreNavGrid::GridRef::FindPath(start, end, smoothPath)` + free wrapper
    `CoreNavGrid::FindPath(...)` (`CoreNavGrid.h`).
  - `AIBaseClient::GetPath(target)` + `GetPath(start, end, smoothPath)` overload
    (`sdk/Core/Objects.h`). `Vector3 = Vec3` nên bridge reinterpret_cast thẳng.
  - Call site `Move()` (`Orbwalker7UP.h:873`) đã đổi từ `player.Path()` workaround
    sang `player.GetPath(vector)` thật.
- **Fallback**: nếu A* fail (grid không hợp lệ, goal bị bao quanh tường, hoặc vượt
  `maxExpansions=20000`), `FindPath` trả straight line `start -> end` để caller không
  bị kẹt. Nếu grid rỗng, trả vector rỗng -> caller nên dùng straight line.
- **Khác vs native**: smoothing là line-of-sight simplification (không byte-identical
  với game `SmoothPath`), nhưng kết quả waypoint đủ để use-case Move (so góc hướng
  path vs hướng waypoint hiện tại) hoạt động đúng.
