# Yasuo Wall Object-Composition Rewrite Design

## Mục tiêu

Xóa toàn bộ implementation Yasuo Wind Wall hiện tại và viết lại từ dữ liệu object thật trong `ObjectManager`. Debug overlay phải vẽ đúng đoạn tường đang hiển thị; collision phải dùng cùng một nguồn dữ liệu, không dùng orientation offset, synthetic cast position hoặc công thức chiều rộng đoán.

Build constraint của workspace: Visual Studio tại `E:\Visual Studio`, platform toolset `v145`.

## Bằng chứng runtime

CE và ReClass cùng attach vào `League of Legends.exe` và xác nhận một lần cast W tạo các object sau:

- `Yasuo_Base_W_windwall1`: wall chính, position `(922.76, 101.26, 644.08)`.
- `Yasuo_Base_W_windwall_activate`: activation effect, không phải wall geometry.
- `YasuoW_VisualMis`: visual center, cùng X/Z với wall chính.
- Hai object `YasuoWChildMis`: position X/Z lần lượt `(907.37, 803.34)` và `(939.40, 484.95)`.

Hai `YasuoWChildMis` vẫn là entry live trong `ObjectManager`; khoảng cách giữa chúng là chính xác `320.00` units và midpoint gần trùng position wall chính. Vì vậy chúng là hai endpoint runtime của tường.

ReClass xác nhận vùng `object + 0x258` chứa status bytes liền trước position tại `object + 0x25C`; đọc qword tại `+0x258` tạo giá trị giả gồm status + float X. Nó không phải `InstanceProxy*`. Các API hiện tại `ResolveProxy()`, `Orientation()` và `EffectEmitter::Direction()` dựa trên field này là sai.

Kết luận: không có offset riêng cho YasuoWall. Detector chỉ cần `ObjectManager`, identity/name/position chung của `GameObject`, và object composition của lần cast.

## Phương án được chọn

Dùng object composition:

1. Nhận diện wall chính bằng exact family `Yasuo_Base_W_windwall1` đến `Yasuo_Base_W_windwall5` (case-insensitive).
2. Nhận diện endpoint bằng exact name `YasuoWChildMis` (case-insensitive).
3. Ghép một main wall với hai child được tạo cùng batch và có midpoint gần main position.
4. Dùng position hiện tại của hai child làm segment thật cho cả drawing và collision.

Không dùng `Yasuo_Base_W_windwall_activate`, `big_impact`, ground crack hoặc `YasuoW_VisualMis` làm endpoint. `YasuoW_VisualMis` chỉ được giữ như dữ liệu diagnostics tùy chọn.

## Kiến trúc

### `sdk/Math/YasuoWallModel.h`

Pure, không đọc process memory. Component này chịu trách nhiệm:

- phân loại exact runtime names;
- parse W level 1..5 từ main name;
- chọn hai endpoint từ các candidate theo creation window và midpoint;
- tạo `WallSegment` từ hai endpoint;
- kiểm tra projectile path với wall segment như segment/capsule, có tính projectile radius.

Pairing hợp lệ khi hai endpoint khác identity, timestamp của main/children lệch không quá 500 ms, và midpoint của hai endpoint cách main center không quá 75 units. Nếu có nhiều cặp, chọn theo thứ tự ổn định: midpoint distance nhỏ nhất, sau đó tổng timestamp delta nhỏ nhất, sau đó network ID để tie-break.

Không dựng segment khi thiếu một endpoint. Detector chờ object thật thay vì tạo geometry fallback.

### `sdk/GameObjects/YasuoWallTracker.h`

Runtime component và là nguồn dữ liệu duy nhất cho SDK:

- subscribe `OnCreateObject` và `OnDeleteObject`;
- enqueue mọi object mới vào pending-name queue vì name có thể rỗng tại create event;
- retry name resolution trong `Refresh()` được tick-guard, tối đa 500 ms cho mỗi pending object;
- seed một lần từ `ObjectManager` khi khởi tạo để hỗ trợ plugin được bật khi wall đã tồn tại;
- track identity bằng address + index + network ID, không dựa vào address đơn lẻ;
- ghép main/children bằng pure model;
- cập nhật endpoint positions tối đa một lần mỗi game tick;
- xóa record theo delete event, identity không còn resolve, hoặc hard expiry 5000 ms;
- không gọi `IsDead()` trên particle/missile object.

Public snapshot chứa main identity, hai endpoint identities, level, center, start, end và spawn tick. Consumer không giữ raw references vào internal vectors.

### `sdk/Math/Collision.h`

- `HasYasuoWindWallCollision()` chỉ kiểm tra Yasuo Wall.
- Xóa `widthBase`, orientation matrix, perpendicular fallback và width formula.
- Collision dùng segment giữa hai endpoint và projectile radius.
- Thêm API tổng hợp `HasProjectileWallCollision()` cho Yasuo/Samira/Mel.
- `ProcessProjectileWalls`, `IsCollision` và Orbwalker dùng API có semantics đúng, tránh việc hàm tên Yasuo âm thầm trả true cho Samira/Mel.
- Giữ `CollisionableObjects::YasuoWall` và API consumer hiện có khi semantics đúng.

### `plugins/Utility/YasuoWallDebugPlugin.h`

Viết lại toàn bộ nội dung nhưng giữ plugin ID để cấu hình hiện có không bị mất. Plugin chỉ đọc tracker snapshot và vẽ:

- segment endpoint A → endpoint B;
- marker cho hai endpoint;
- marker center;
- label level, network IDs và measured span.

Plugin không tự scan objects, không tự tính orientation và không chứa collision logic.

## Dọn code cũ

Xóa:

- `sdk/Math/WindwallTracker.h`;
- `sdk/Math/WindwallGeometry.h`;
- `tests/windwall_geometry_test.cpp`;
- include/call/comment cũ liên quan `WindwallTracker` và `WindwallGeo`;
- `Offset::All::EffectEmitterHandle`;
- `Offset::EffectEmitterLayout::ProxyOrientation` và namespace rỗng sau khi xóa;
- `EffectEmitter::ResolveProxy()`, `Orientation()` và `Direction()`.

Giữ class `EffectEmitter`, `ObjectType::EffectEmitter` và public particle-emitter API để không phá SDK parity. Chỉ loại bỏ transform API chưa được chứng minh.

Các tài liệu rewrite cũ được coi là superseded bởi spec này; không dùng giả định event-name có sẵn ngay tại create hoặc proxy `+0x258` trong implementation mới.

## Error handling và lifecycle

- Name chưa sẵn sàng: giữ pending và retry; không phân loại đoán.
- Main có nhưng chưa đủ hai child: không publish active wall.
- Child bị delete hoặc identity bị reuse: invalidate wall trong lần refresh kế tiếp.
- Hai Yasuo hoặc nhiều candidate đồng thời: pairing theo timestamp + midpoint, không dựa vào thứ tự network ID.
- Process/game context đổi: tracker state phải reset khi runtime reinitialize; không giữ module base hoặc raw pointer xuyên process.

## Kiểm thử

Tạo `tests/yasuo_wall_model_test.cpp`, test-first, bao phủ:

- accept chính xác main names level 1..5;
- reject activate, impact, ground crack và object không liên quan;
- accept chính xác `YasuoWChildMis`;
- parse level;
- chọn đúng hai child theo midpoint/timestamp khi có nhiễu;
- deterministic tie-break;
- không publish khi thiếu endpoint;
- segment dùng trực tiếp hai endpoint và cho measured span `320` với sample CE;
- path cắt wall;
- path không cắt wall;
- capsule collision khi path cách wall nhỏ hơn projectile radius;
- miss khi khoảng cách lớn hơn radius;
- endpoint collision.

Runtime verification sau build:

1. Bật `Yasuo Wall Debug`.
2. Cast W theo ít nhất hai hướng khác nhau.
3. Xác nhận segment/markers trùng visual wall trong toàn bộ thời gian tồn tại.
4. Xác nhận active count về 0 sau delete/expiry.
5. Bắn projectile xuyên, sát mép và ngoài tường để xác nhận collision.

## Tiêu chí hoàn thành

- Không còn reference tới tracker/geometry/offset transform cũ.
- Debug plugin vẽ đúng hai endpoint runtime.
- Drawing và collision đọc cùng tracker snapshot.
- Không có geometry synthetic hoặc orientation fallback.
- Unit tests mới pass.
- Full solution build thành công bằng Visual Studio toolset `v145`.
