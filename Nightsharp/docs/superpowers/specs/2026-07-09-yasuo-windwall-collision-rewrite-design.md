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

## 3. Về CommunityDragon (trả lời câu hỏi gốc)

- `raw.communitydragon.org` host asset đã giải nén; particle của Yasuo ở path kiểu
  `.../game/data/characters/yasuo/particles/...`.
- Vai trò: **tra chuỗi tên chính xác** của particle/emitter windwall theo client hiện tại, đối chiếu với match `yasuo` + `_w_windwall`.
- **Không** cho offset, **không** giúp enumerate object. "Troys / EffectEmitters" là **object type runtime**, phải lấy qua ObjectManager trong game — không tải về nhét vào code.
- Bước xác minh: fetch CDragon lấy tên; nếu cần chắc hơn, dùng RE tools (ReClass/CheatEngine) đọc tên object emitter thật lúc runtime.

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

- Verify in-game bằng debug plugin trước khi chốt: vẽ cả hai cách (`mat.m[0][0]/m[0][2]` trực tiếp vs `Direction()` vuông góc), xem cách nào trùng tường thật.
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
