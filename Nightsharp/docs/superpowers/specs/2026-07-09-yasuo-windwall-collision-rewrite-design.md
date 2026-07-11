# Thiết kế: Làm lại detect Yasuo Windwall cho collision (Hướng A — event-driven)

- **Ngày:** 2026-07-09
- **Trạng thái:** Design — chờ implement
- **Phạm vi:** `sdk/Math/Collision.h`, một component mới `WindwallTracker`, và visualization trong `plugins/Utility/YasuoWallDebugPlugin.h`.

## 1. Bối cảnh & gốc rễ bug

Triệu chứng: **skillshot bắn xuyên qua Yasuo windwall** (false negative — wall không được nhận diện).

Khảo sát code hiện tại cho thấy NightSharp có sẵn hạ tầng detect windwall nhưng **cả 3 đường lấy wall thật đều chết**, chỉ còn synthetic đoán mò:

1. `detail::AddWindwall()` trong [Collision.h](../../../sdk/Math/Collision.h) (dòng ~252) được **định nghĩa nhưng không bao giờ được gọi** → list `Windwalls` (EffectEmitter thật) **luôn rỗng**.
2. `GameObjects::ParticleEmitters()` / `ParticleEmittersList` **luôn bị clear, không bao giờ populate** (chưa dump vtable RVA cho EffectEmitter — comment tại [GameObjects.h:101-104](../../../sdk/GameObjects/GameObjects.h)).
3. `ObjectManager::Get<EffectEmitter>()` chạy được nhưng phải name-scan toàn bộ ~16384 object mỗi lần gọi → quá đắt cho collision/prediction per-tick, không dùng.

Hệ quả: production chỉ còn `SyntheticWindwalls` dựng từ event `OnDoCast` — geometry **ước lượng từ vị trí cast**, sai lệch → xuyên tường.

Đã xác nhận **live path**: `Prediction::Movement::GetPrediction` ([Movement.h:986](../../../sdk/Math/Prediction/Movement.h)) gọi `SDK::Collision::GetCollision(positions, input)` → `ProcessProjectileWalls` → `SegmentIntersectsWindwall` → nhánh synthetic. Đúng như chẩn đoán.

Thêm một **dead path thứ 4** cần dọn: `Prediction::Movement::CollectLineCollisions` ([Movement.h:1050](../../../sdk/Math/Prediction/Movement.h)) có nguyên block YasuoWall riêng dùng `ObjectManager::Get<EffectEmitter>()` + `emitter.Name()` **không fallback** — nhưng hàm này **không được gọi ở đâu** (dead). Nếu tương lai ai đó wire nó vào, `Name()` rỗng sẽ lại xuyên tường. Phải xóa block windwall trong đó.

Thêm một nghi phạm: **mâu thuẫn geometry** giữa hai consumer đang tồn tại:
- [Collision.h:569-570](../../../sdk/Math/Collision.h): `Vec2(mat.m[0][0], mat.m[0][2])` dùng **trực tiếp** làm hướng trải tường.
- [YasuoWallDebugPlugin.h:182-192](../../../plugins/Utility/YasuoWallDebugPlugin.h): `emitter.Direction()` rồi lấy **vuông góc** `Vec2(-y, x)`.

Hai cách lệch nhau 90° — một đúng, một sai; phải verify in-game.

## 2. Mục tiêu

- Xóa sạch đường synthetic + code chết để không còn sót lỗi.
- Dùng **EffectEmitter thật** qua đường event-driven (đường duy nhất đã chứng minh chạy được trong debug plugin: `createSeen_`/`deleteSeen_` đếm được).
- Chốt **một** công thức geometry đúng cho hướng + độ rộng tường.
- Visualization vẽ **chính bức tường gió**, không vẽ đường thẳng probe xuyên qua.

Ngoài phạm vi: Samira wall, Mel wall, Braum shield — giữ nguyên, không đụng. Public API của `SDK::Collision` giữ nguyên để không vỡ callers khác.

## 3. Tên emitter runtime — ĐÃ XÁC MINH (CheatEngine, 2026-07-09)

Search string trong memory League (PID 19820, game paused, wall có sẵn) xác nhận tên particle thật:

- **`Yasuo_Base_W_windwall1`** → level 1
- **`Yasuo_Base_W_windwall2`** → level 2
- **`Yasuo_Base_W_windwall3`** → level 3
- **`Yasuo_Base_W_windwall4`** → level 4
- **`Yasuo_Base_W_windwall5`** → level 5
- (phụ: `Yasuo_Base_W_windwall_big_impact`, `..._groud_crack.tex` — không phải wall chính)

Kết luận:
- Match hiện tại `contains("yasuo") && contains("_w_windwall")` (lowercase) **ĐÚNG** — `yasuo_base_w_windwall5` chứa cả hai. Không đổi.
- Level-parse theo `windwall2..5` **ĐÚNG**.
- Chuỗi `_W_windWall` (casing cũ) **không tồn tại** runtime — nếu match case-sensitive sẽ trượt; phải giữ lowercase.

Về CommunityDragon: `raw.communitydragon.org` chỉ dùng để tra tên particle (path `.../characters/yasuo/particles/...`); "Troys/EffectEmitters" là object type runtime, không tải về nhét code. Vì đã xác minh tên trực tiếp bằng CE nên bước fetch CDragon **không còn bắt buộc**.

## 4. Thiết kế chi tiết

### 4.1 Component mới: `WindwallTracker`

File header riêng (đề xuất `sdk/Math/WindwallTracker.h` hoặc trong `detail` của Collision.h — quyết định lúc implement để tránh circular include).

- Singleton, subscribe `OnCreateObject` / `OnDeleteObject`.
- `OnCreate`: nếu tên object chứa `yasuo` + `_w_windwall` (case-insensitive) → lưu bản ghi:
  ```
  struct Windwall {
      EffectEmitter emitter;   // hoặc handle + address
      int networkId;
      int level;               // parse từ windwall2..windwall5, mặc định 1
      int castTick;            // để hết hạn phòng hờ
  };
  ```
- `OnDelete`: gỡ theo networkId/address.
- Hết hạn phòng hờ: prune bản ghi cũ hơn ngưỡng an toàn (ví dụ 5000ms) trong trường hợp event delete không bắn.
- API: `const std::vector<Windwall>& Active()` (đọc trực tiếp, không scan object).
- Đọc tên object: dùng `Name()`, fallback `ReadCharacterName`/`ReadName` giống `RuntimeObjectName` hiện có.

### 4.2 Chốt geometry (một nguồn duy nhất)

- **Không verify được bằng RE khi game pause** — chọn đúng giữa 2 cách lệch 90° cần **nhìn** tường thật trên màn hình, nên đây là việc bắt buộc làm bằng debug overlay lúc test.
- Verify in-game bằng debug plugin: vẽ cả hai cách (`mat.m[0][0]/m[0][2]` trực tiếp vs `Direction()` vuông góc), xem cách nào trùng tường thật.
- Chốt **một** công thức span direction + width theo level (`base + 50 * level`); xóa cách còn lại để không còn 2 nguồn mâu thuẫn.
- Rủi ro offset: `Orientation()` phụ thuộc `Offset::All::EffectEmitterHandle` và `Offset::EffectEmitterLayout::ProxyOrientation`. Nếu proxy resolve fail → matrix = 0 → hướng tường = rác. Bước verify phải kiểm tra matrix trả về khác 0; nếu stale thì cập nhật offset trước.

### 4.3 Wire vào `Collision.h`

- `SegmentIntersectsWindwall(start, end, widthBase, extraRadius)` đọc thẳng từ `WindwallTracker::Active()`, dựng segment tường từ geometry đã chốt, giữ nguyên logic `Prediction::Vec2Ext::Intersection`.
- `HasYasuoWindWallCollision`, `ProcessProjectileWalls`, `IsCollision` gọi qua đường này. Public API không đổi.

### 4.4 Xóa sạch code chết / synthetic trong `Collision.h`

Bỏ toàn bộ:
- `struct SyntheticWindwall`, `SyntheticWindwalls`
- `TryBuildSyntheticWindwall`, `IsYasuoWCast`, `ResolveYasuoWLevel`
- `PruneSyntheticWindwalls`, `AddSyntheticWindwall`, `OnDoCast`
- `EnsureEventHooks`, `DoCastHooked`
- Hàm chết `AddWindwall` + biến `Windwalls` cũ (thay bằng tracker)
- `WallCastT` nếu không còn dùng

Giữ: `IsWallCastActive`/`GetWindWallLevel`/`RefreshWindwalls` chỉ nếu còn phục vụ tracker; nếu không thì bỏ luôn.

### 4.5 Visualization (debug plugin)

- **Vẽ chính bức tường gió**: dựng đoạn tường (hoặc rectangle mỏng) theo đúng geometry đã chốt tại vị trí emitter — thể hiện chiều rộng + hướng thật của wall.
- **Bỏ** đường thẳng probe player→cursor xuyên qua wall (`DrawPlayerProbe` phần vẽ line probe).
- Debug plugin vẫn giữ vai trò tool verify (đếm create/delete, hiển thị tên/level/networkId của emitter).

## 5. Kế hoạch test

Bật `YasuoWallDebugPlugin` overlay, cho Yasuo bấm W, xác nhận theo thứ tự:
1. Tracker bắt được emitter (create counter tăng, list `Active()` có phần tử).
2. Hình tường vẽ ra **trùng khít** tường gió thật in-game (đúng vị trí + hướng + độ rộng).
3. Skillshot line qua tường → collision trả `true`, bot **không** bắn xuyên.
4. Sau khi wall hết → emitter bị delete → list rỗng → collision trả `false` trở lại.

## 6. Rủi ro & giả định

- **Giả định:** `OnCreateObject`/`OnDeleteObject` bắn ổn định cho EffectEmitter (đã có bằng chứng từ debug plugin, cần tái xác nhận in-game).
- **Rủi ro gap spawn:** object emitter có thể sinh sau lúc bấm W một khoảng ngắn. Hướng A chấp nhận rủi ro này (đã thống nhất). Nếu test thấy vẫn xuyên trong khoảnh khắc đầu → nâng cấp thành hybrid (bổ sung lại synthetic bù gap) ở một spec riêng.
- **Rủi ro offset stale** cho `Orientation()` — xử lý ở bước verify 4.2.
