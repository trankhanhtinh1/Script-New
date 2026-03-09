# So sánh Bypass logic: ImGui DirectX 11 Kiero Hook vs LeagueChimera Bypass.cpp

## LeagueChimera (Bypass.cpp)

| Hàm | Logic |
|-----|--------|
| `mainloop_check()` | Đọc `*(MAKE_RVA(DetectionWatcher2))` → detection_inst, EncryptedBuffer<BYTE> tại detection_inst+0x8 → `ZeroCurrentValue()` |
| `issueorder_flag(order_sent)` | Ghi `order_sent + 17` vào `MAKE_RVA(IssueOrderFlag)` |
| `castspell_flag()` | Ghi `1` vào `MAKE_RVA(CastSpellFlag)` |

**Nơi gọi:**
- `mainloop_check`: ObjectManager::get_first_object(), GameObject (sau issueorder_flag), SpellBook (quanh cast)
- `issueorder_flag`: GameObject trước khi gọi IssueOrder
- `castspell_flag`: SpellBook trước khi cast

---

## Project của bạn (sdk/Utils/Bypass.h + Offsets.h)

| Chimera | Project bạn | Ghi chú |
|---------|-------------|--------|
| `mainloop_check()` | `MainloopCheck()` | ✅ Tương đương: resolve DetectionWatcher2 bằng **signature** (4C 8B 3D ? ? ? ? 4D 85 FF 0F), rồi ZeroCurrentValue tại +0x8. Có __try/__except. |
| `issueorder_flag(order)` | `PrepareIssueOrder(order)` | ✅ Gọi MainloopCheck() trước, rồi ghi (order+17) vào `Globals::base + Offset::Flag::IssueOrderFlag`. |
| `castspell_flag()` | `PrepareCastSpell()` | ✅ Gọi MainloopCheck() trước, rồi ghi 1 vào `Globals::base + Offset::Flag::CastSpellFlag`. |

**EncryptedBuffer:** Layout giống Chimera (GetKeyArray, GetIndex, GetValuesArray, ZeroCurrentValue = `array[GetIndex()] = 0 ^ ~GetKeyArray()[0]`).

**Nơi gọi trong project bạn:**
- `PrepareIssueOrder`: Orbwalker.h (IssueOrderCore), GameObject.h (IssueOrder)
- `PrepareCastSpell`: SpellCaster.h (Cast), Items.h (item actives)
- `MainloopCheck`: chỉ được gọi **bên trong** PrepareIssueOrder/PrepareCastSpell, **không** gọi ở đầu vòng lặp object.

---

## Điểm khác / Thiếu

1. **DetectionWatcher2:** Chimera dùng RVA cố định (Addresses::Bypass::DetectionWatcher2). Bạn dùng **signature scan** → ổn hơn khi patch game đổi RVA.

2. **IssueOrderFlag / CastSpellFlag:** Chimera dùng RVA cố định; bạn dùng `Globals::base + Offset::Flag::*` (Offsets.h 0x1CDDF88, 0x1CDDF20) → cùng ý nghĩa.

3. **MainloopCheck trước GetFirstObject:** Chimera gọi `mainloop_check()` ở **ObjectManager::get_first_object()** (đầu mỗi lần iterate). Project bạn **chưa** gọi MainloopCheck trong ObjectManager khi iterate (ForEach / IterateObjectsSafe). Nếu anti-cheat đọc detection flag trong GetFirstObject/GetNextObject, nên gọi MainloopCheck() ngay trước khi gọi GetFirstObject.

---

## Kết luận

- **Có logic bypass giống Chimera:** mainloop (zero detection), issue order flag (order+17), cast spell flag (1), và EncryptedBuffer::ZeroCurrentValue giống nhau.
- **Khác:** Bạn gói mainloop bên trong PrepareIssueOrder/PrepareCastSpell; Chimera gọi mainloop ở nhiều chỗ (ObjectManager, sau IssueOrder, SpellBook).
- **Nên bổ sung:** Gọi `SDK::Bypass::MainloopCheck()` ở đầu ObjectManager khi bắt đầu iterate (trước GetFirstObject) để khớp Chimera và an toàn hơn.
