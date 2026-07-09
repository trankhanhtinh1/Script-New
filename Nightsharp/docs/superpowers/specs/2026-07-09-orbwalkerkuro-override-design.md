# OrbwalkerKuro Override — Design

**Date:** 2026-07-09
**Status:** Approved (design), pending implementation plan

## Mục tiêu

Cho `plugins/Core/OrbwalkerKuro` load được như một orbwalker **override** — bật lên thì thay thế SDK orbwalker gốc, tắt thì trả lại SDK — theo đúng tinh thần pattern override của source Old (`Old/plugins/core/OrbwalkerPlugin.h`), thích ứng vào registry của SDK NightSharp.

## Bối cảnh / Thực trạng

- OrbwalkerKuro là bản **rewrite modular** của SDK orbwalker (dev2): `OrbwalkerBase.h` 144 dòng + logic tách ra các `.inl` (Actions/EventHandlers/Lifecycle/Targeting) + `OrbwalkerContext.h`/`OrbwalkerEventBus.h`/`OrbwalkerMenu.h`/`OrbwalkerTypes.h`.
- **Chặn build:** cả OrbwalkerKuro lẫn SDK cùng nằm trong `namespace SDK` và cùng khai báo `SDK::Orbwalker`, `SDK::IOrbwalker`, `SDK::OrbwalkerBase`, `SDK::OrbwalkingDetail` → vi phạm ODR, không compile chung.
- **Include sai:** các `#include "../../Core/..."` trong file Kuro copy nguyên từ vị trí cũ `sdk/Wrappers/Orbwalking/`, trỏ sai khi ở `plugins/Core/OrbwalkerKuro/`.
- **Nợ build sẵn có:** `sdk/Wrappers/Orbwalking/SdkWrappersInit.h` gọi `impl->Suspend()/Resume()` nhưng `SDK::IOrbwalker` chưa có 2 method này.
- Interface lệch: Kuro `IOrbwalker` 37 method, SDK `IOrbwalker` 41 method.
- Hiện `OrbwalkerKuroPlugin.h` chỉ là **stub** (`AutoLoadByDefault=false`, OnLoad/OnUnload chỉ log) — đã register trong `PluginBootstrap.h` để fix C1083, nhưng không làm gì.

## Pattern override của Old (tham chiếu)

`Old/plugins/core/OrbwalkerPlugin.h` là `Plugins::IPlugin` thuần, **không** redeclare class SDK nào:
- Tự chứa toàn bộ logic (GetTarget/Attack/Move/Orbwalk/CanAttack/ShouldWait…) + tự dựng menu.
- Dùng SDK orbwalker như bus trạng thái: `SetExternalControl(true)`, `SetActiveMode()`, `InvokeAction()`.
- Override = ẩn row SDK: `HideSdkEntry()` → `PluginRegistry::FindByInternalId("orbwalker").Loaded=false`; unload đảo ngược.

SDK NightSharp **khác** Old: không có `SetExternalControl/SetActiveMode/InvokeAction`, mà dùng **registry** `AddOrbwalker/SetOrbwalker` để chọn implementation active. Đây là điểm cắm override đúng chuẩn NightSharp — giữ cho mọi champion gọi `SDK::Orbwalker::CanAttack()/OnBeforeAttack += …` tự route sang impl đang active.

## Thiết kế

### 1. Bóc namespace
Đổi 12 file Kuro từ `namespace SDK` → `namespace OrbwalkerKuro`. Giữ nguyên các tham chiếu tới type dùng chung của SDK (`SDK::AttackableUnit`, `SDK::GameObjects`, `SDK::Events`, `SDK::Menu`, `SDK::Vector3`, …) bằng cách qualify `SDK::`. Fix lại toàn bộ include path cho đúng vị trí `plugins/Core/OrbwalkerKuro/`.

### 2. Bỏ facade trùng, cắm vào registry SDK
- Xoá `OrbwalkerKuro::Orbwalker` (facade) và `OrbwalkerKuro::IOrbwalker` riêng.
- Class implementation của Kuro **kế thừa thẳng `SDK::IOrbwalker`** (41-method) để `SDK::Orbwalker::AddOrbwalker("Kuro", &impl)` nhận được.
- Giữ lại logic: targeting (`OrbwalkerTargeting.inl`), actions (`OrbwalkerActions.inl`), event handlers (`OrbwalkerEventHandlers.inl`), lifecycle, menu (`OrbwalkerMenu.h`), context, event bus — reparent lên `SDK::IOrbwalker`.
- Reconcile 37↔41 method: bổ sung các method SDK interface yêu cầu mà Kuro thiếu (hoặc ngược lại loại bỏ method thừa không thuộc interface, chuyển thành internal helper).

### 3. Plugin wrapper (`Plugins::OrbwalkerKuroPlugin`)
- `OnLoad`: khởi tạo instance Kuro → `SDK::Orbwalker::AddOrbwalker("Kuro", &impl)` → `SDK::Orbwalker::SetOrbwalker("Kuro")` → ẩn row `"orbwalker"` (tương đương `HideSdkEntry` của Old) + dựng menu Kuro.
- `OnUnload`: `SDK::Orbwalker::SetOrbwalker("SDK")` → hiện lại row → gỡ/ẩn menu Kuro → gỡ khỏi registry.
- `AutoLoadByDefault=false` (người dùng chủ động bật trong Plugin Manager).

### 4. Dọn nợ build SdkWrappersInit
Sửa `SdkWrappersInit.h` đang gọi `Suspend()/Resume()` chưa tồn tại. Hai lựa chọn:
- (a) Thêm `virtual void Suspend() {}` / `virtual void Resume() {}` mặc định vào `SDK::IOrbwalker` (no-op cho bản SDK, Kuro có thể override nếu cần), hoặc
- (b) Chuyển cơ chế suspend sang thuần "ẩn row + SetOrbwalker" như Old, bỏ hẳn Suspend/Resume.

Chốt ở bước lập plan (nghiêng về (a) vì ít đụng chạm hành vi hiện có).

## Ràng buộc / Non-goals

- **Không** đổi logic orbwalk của Kuro (giữ nguyên hành vi dev2 đã viết); chỉ tái cấu trúc để build + cắm registry.
- **Không** đụng SDK orbwalker monolith ngoài việc thêm Suspend/Resume no-op (nếu chọn (a)).
- **Không** đổi cách champion gọi `SDK::Orbwalker::*` — chúng phải hoạt động y nguyên khi Kuro active.

## Rủi ro

- Refactor 12 file + SDK core → dễ vỡ build hàng loạt; làm từng bước, build kiểm tra sau mỗi nhóm thay đổi.
- Reconcile interface 37↔41 method có thể lộ ra method Kuro chưa implement đầy đủ → cần map kỹ từng method sang SDK::IOrbwalker.
- Build memory-fragile (đã biết): TU khổng lồ dễ C1060 khi RAM thấp — build lúc đóng app nặng.

## Tiêu chí hoàn thành

- Build xanh (kể cả SdkWrappersInit).
- Bật OrbwalkerKuro trong Plugin Manager → orbwalk chạy bằng logic Kuro, row SDK ẩn.
- Tắt → trả về SDK orbwalker, champion (Draven/Ezreal/…) vẫn orbwalk bình thường.
