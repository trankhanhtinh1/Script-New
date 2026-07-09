---
description: Port a champion AIO from C# (LeagueSharp/EloBuddy) to NightSharp C++
---

Invoke the `port-champion-csharp-to-cpp` skill, then follow its process:

1. **Bước 0** — Đọc mẫu `plugins/Champion/7UPAIO/Ezreal.h` + đọc file C# champion cần port.
2. **Bước 1** — Tạo skeleton `<Champion>.h` (copy từ Ezreal.h).
3. **Bước 2** — Khai báo Spell + biến state. **Bước 2.1** — Xác minh CDragon.
4. **Bước 3** — Forward-declare tất cả hàm THEO ĐÚNG THỨ TỰ C#.
5. **Bước 4** — `BuildMenu()` + `OnGameLoad()`.
6. **Bước 5** — Port từng hàm logic (giữ thứ tự + matching 1-1).
7. **Bước 6** — `OnUnload()` (gỡ event đối xứng).
8. **Bước 7** — Đăng ký champion trong `7UPAIO.h`.
9. **Bước 8** — Build sạch + verify.

Nguyên tắc tối thượng: matching 1-1, không tự bịa API, dùng SDK, giữ thứ tự hàm.
