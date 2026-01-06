# AI Manager Offset Guide - Obfuscation Method

## Tổng quan

Có 2 phương pháp để đọc AiManager trong League of Legends:

### Phương pháp 1: Direct (đơn giản, không encryption)
- Offset: `oObjAiManager = 0x3108`
- Cách dùng: `*(uint64_t*)(object + 0x3108)`
- Ưu điểm: Đơn giản
- Nhược điểm: Có thể không hoạt động trong một số version

### Phương pháp 2: Obfuscated (có encryption) ⭐ Khuyến nghị
- Offset: `oObjAiManagerObf = 0x36F0`
- Cần giải mã sử dụng `LeagueObfuscation<T>` struct
- Ưu điểm: Chính xác hơn, từ source leagueoflegends-master
- Nhược điểm: Phức tạp hơn

## Cách sử dụng Phương pháp Obfuscation

### Step 1: Đọc obfuscation structure
```cpp
LeagueObfuscation<uint64_t> obf = *(LeagueObfuscation<uint64_t>*)(object + 0x36F0);
```

### Step 2: Decrypt để lấy pointer
```cpp
uint64_t decrypted = Decrypt(obf);
```

### Step 3: Đọc AiManager pointer (+0x10)
```cpp
uint64_t aiManager = *(uint64_t*)(decrypted + 0x10);
```

### Step 4: Đọc các giá trị từ AiManager
```cpp
// Target position (điểm click chuột)
Vector3 targetPos = *(Vector3*)(aiManager + 0x14);

// IsMoving flag
bool isMoving = *(bool*)(aiManager + 0x2BC);

// Current path segment
int currentSegment = *(int*)(aiManager + 0x2C0);

// Path start (vị trí hiện tại từ server)
Vector3 pathStart = *(Vector3*)(aiManager + 0x2D0);

// Path end (vị trí đích)
Vector3 pathEnd = *(Vector3*)(aiManager + 0x2DC);

// Segments pointer
void* segments = *(void**)(aiManager + 0x2E8);

// Segments count
int segmentsCount = *(int*)(aiManager + 0x2F0);

// Dash speed (>0 khi đang dash)
float dashSpeed = *(float*)(aiManager + 0x300);

// IsDashing flag
bool isDashing = *(bool*)(aiManager + 0x324);

// Server position (chính xác nhất)
Vector3 serverPos = *(Vector3*)(aiManager + 0x414);
```

## Offset Table

| Tên | Direct Offset | Obfuscated Offset | Mô tả |
|-----|---------------|-------------------|-------|
| oObjAiManager | 0x3108 | 0x36F0 (+ decrypt) | Object → AiManager pointer |
| AiManagerPtr | - | 0x10 | Decrypted → AiManager* |
| TargetPosition | 0x23C (EndPath) | 0x14 | Điểm đích click chuột |
| IsMoving | N/A (tính toán) | 0x2BC | Flag đang di chuyển |
| CurrentSegment | N/A | 0x2C0 | Index segment hiện tại |
| PathStart | 0x1E0 | 0x2D0 | Vị trí bắt đầu path |
| PathEnd | 0x23C | 0x2DC | Vị trí kết thúc path |
| Segments | N/A | 0x2E8 | Pointer to segments array |
| SegmentsCount | 0x210? | 0x2F0 | Số lượng segment |
| DashSpeed | 0x300 | 0x300 | Tốc độ dash |
| IsDashing | 0x324 | 0x324 | Flag đang dash |
| ServerPosition | 0x1E0 | 0x414 | Vị trí server (chính xác nhất) |

## Files đã tạo

1. **SDK/LeagueObfuscation.h** - Template struct và Decrypt function
2. **SDK/AiManager.h** - AiManager class với cả 2 phương pháp
3. **SDK/AiManagerDumper.h** - Tool để dump và so sánh cả 2 phương pháp
4. **SDK/Offsets.h** - Đã thêm namespace `AiManagerObf`

## Cách sử dụng trong GameObject

```cpp
// Auto-detect phương pháp tốt nhất
uint64_t aiManager = gameObject.GetAiManager();

// Kiểm tra đang dùng phương pháp nào
bool usingObf = gameObject.IsUsingObfuscatedAiManager();

// Các helper methods
bool moving = gameObject.IsMoving();
bool dashing = gameObject.IsDashing();
Vector3 targetPos = gameObject.GetTargetPosition();
Vector3 serverPos = gameObject.GetServerPosition();
```

## Cách chạy Dumper để verify offsets

1. Inject DLL vào game
2. Gọi `AiManagerDumper::DumpAndCompare()` một lần
3. Check file `aimanager_dumper.txt` trong game directory
4. So sánh kết quả của cả 2 phương pháp

## Lưu ý

- Nếu phương pháp Obfuscated không hoạt động, có thể offset `0x36F0` đã thay đổi
- Sử dụng `AiManagerDumper::ContinuousLog()` trong main loop để theo dõi liên tục
- Các offset có thể thay đổi khi game update
