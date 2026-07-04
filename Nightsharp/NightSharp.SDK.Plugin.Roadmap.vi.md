# NightSharp SDK và External Plugin Roadmap

## Mục tiêu

NightSharp sẽ đi theo hướng tách thành ba phần rõ ràng:

- `NightSharp`: runtime/core chính, giữ hook, overlay, loader, offset, crash handling và các cầu nối nội bộ.
- `NightSharp.SDK`: shared SDK/developer package để Visual Studio dùng khi build plugin riêng.
- External plugins: mỗi plugin là một DLL riêng, sau đó được đóng gói thành định dạng `.NS` để phân phối.

Hướng này giúp mọi người làm riêng theo module mà không cần sửa core source. Ví dụ: một người làm Evade, một người làm Awareness, một người làm Champion Logic, một người làm Utility. Git merge sẽ nhẹ hơn vì plugin không nằm chung trong một project core nữa.

SDK hiện tại trong `NightSharp/SDK` đã là source-of-truth. `NightSharp.SDK` không nên viết lại SDK mới, mà nên đóng gói, export và làm ổn định public surface từ folder đó.

Điểm quan trọng: trong `NightSharp.dll` vẫn cần có một số SDK runtime modules mặc định như Orbwalker, TargetSelector, Prediction và các service nền bắt buộc. `NightSharp.SDK` là package riêng cho developer build plugin, nhưng không thay thế phần SDK runtime mặc định đang chạy bên trong `NightSharp.dll`.

## Nguyên tắc kiến trúc

- Plugin author chỉ dùng public SDK, ưu tiên include/use `SDK/SDK.h` và facade script-friendly.
- Plugin không include trực tiếp `NightSharp/Core`, không gọi trực tiếp wrapper nội bộ như cast/order native core.
- NightSharp core giữ quyền sở hữu hook, game memory, object cache, renderer, menu root, crash reporter và loader.
- NightSharp core vẫn load SDK runtime modules mặc định để plugin có nền tảng dùng ngay khi không có plugin override.
- `NightSharp.SDK` là hợp đồng giữa core và plugin: version rõ ràng, ABI rõ ràng, toolset rõ ràng.
- Mỗi plugin có `InternalId` riêng, menu root riêng, config riêng, log scope riêng.
- Dev build được phép load raw DLL trong chế độ developer để debug nhanh.
- Release build đi qua packager thành `.NS`; runtime mặc định chỉ load `.NS` hợp lệ.

## SDK runtime mặc định và override model

NightSharp vẫn có `NightSharp/SDK` bên trong runtime để cung cấp các module mặc định. Những module như Orbwalker, TargetSelector và Prediction nên được xem là default providers của SDK:

- Nếu không có plugin override, NightSharp dùng provider mặc định trong `NightSharp.dll`.
- Nếu developer tạo Orbwalker/TargetSelector/Prediction riêng, plugin đó đăng ký provider mới qua SDK registry/provider interface.
- Provider override phải có priority rõ ràng, version rõ ràng và khả năng fallback về default provider.
- Chỉ một provider chính nên active cho một service tại một thời điểm, ví dụ một Orbwalker chính hoặc một TargetSelector chính.
- Menu phải hiển thị provider nào đang active và cho phép disable override khi cần debug.
- Override không được thay thế bằng cách sửa trực tiếp core source hoặc hook private function; nó phải đi qua public SDK contract.

Như vậy, NightSharp vừa có SDK mặc định để mọi plugin chạy được ngay, vừa cho phép dev viết module nâng cao để override khi cần. Đây là hướng đúng hơn so với việc tách toàn bộ Orbwalker/TargetSelector/Prediction ra ngoài ngay từ đầu.

## Toolchain và build contract

- Visual Studio path theo AGENTS: `E:\Visual Studio`.
- Platform toolset: `v145`.
- Target mặc định: x64.
- Plugin template cần match runtime CRT/runtime flags để tránh lỗi ABI và unload.
- SDK package cần có version, ABI id và compatibility range.
- Khi plugin không match SDK/ABI, NightSharp phải disable plugin và ghi log rõ ràng thay vì crash.

## Target layout

- `NightSharp/`: injected/runtime project hiện tại.
- `NightSharp/SDK/`: SDK source-of-truth đã hoàn thành, vẫn được dùng trong `NightSharp.dll` cho runtime modules mặc định.
- `NightSharp.SDK/`: project/package cho developer dùng trong Visual Studio.
- `NightSharp.Plugin.Template/`: template để tạo plugin mới.
- `NightSharp.Plugin.Packager/`: tool đóng gói raw DLL thành `.NS`.
- `%AppData%\NightSharp\Plugins\`: nơi runtime quét và load plugin release.
- `%AppData%\NightSharp\Plugins\Dev\`: nơi load raw DLL khi bật developer mode.

## Runtime loading model

1. NightSharp khởi động core và SDK wrappers nội bộ.
2. Plugin loader quét `%AppData%\NightSharp\Plugins`.
3. Loader đọc metadata của plugin: id, name, author, version, category, target SDK, dependencies.
4. Nếu là `.NS`, loader verify package, check version, giải mã vào memory và load module.
5. Nếu là raw DLL trong `Dev`, chỉ load khi developer mode được bật.
6. Loader bind plugin vào SDK host API và đăng ký plugin vào PluginRegistry/Menu.
7. PluginManager gọi lifecycle: load, unload, update, render, menu và event callbacks.
8. Nếu plugin crash trong callback, NightSharp disable plugin đó, ghi log/dump và tiếp tục chạy nếu có thể.

## NightSharp.SDK developer package

`NightSharp.SDK` nên giống tinh thần `EnsoulSharp.SDK`: developer chỉ cần tạo plugin project, add SDK dependency và build DLL riêng.

Nội dung cần có:

- Public headers lấy trực tiếp từ `NightSharp/SDK`.
- Import library hoặc host bridge để plugin gọi vào runtime NightSharp.
- Version file/metadata để plugin biết SDK version đang build.
- Documentation cho public modules: Game, ObjectManager, GameObjects, Spell, Orbwalker, TargetSelector, Prediction, Damage, Menu, Drawing, Events, Utilities.
- Interface/contract để plugin có thể đăng ký override provider cho Orbwalker, TargetSelector, Prediction hoặc service SDK khác.
- Plugin template cho Utility, Champion, Awareness và Evade.
- Sample plugin nhỏ để verify build/load, sau đó sample champion logic.

Public SDK surface nên giữ style script-friendly đã có:

- `Spell`, `SpellBookClient`, prediction types, `IsValidTarget`, `Keys`, menu/keybind và event args.
- Plugin-facing spell cast đi qua SDK wrapper, không đi thẳng vào `CoreCastSpell`.
- Facade tiếp tục là lớp short-name để plugin code gọn như script.

## External plugin contract

Mỗi plugin cần có contract rõ:

- Unique internal id, ví dụ `awareness.main`, `evade.core`, `champion.ezreal`.
- Plugin category: Core, Champion, Utility, Misc hoặc category mở rộng sau này.
- SDK target version và ABI id.
- Optional champion name nếu plugin chỉ load cho một champion.
- Dependencies nếu plugin cần module khác.
- Lifecycle callbacks rõ ràng.
- Không throw exception qua boundary giữa plugin và host.
- Không giữ con trỏ SDK/game object qua unload nếu object đó do host quản lý.
- Plugin phải release menu, event subscription và resource khi unload.

## Merge-conflict model

Mục tiêu chính là để mọi người làm độc lập:

- Evade nằm trong repo/project riêng.
- Awareness nằm trong repo/project riêng.
- Champion logic nằm trong repo/project riêng hoặc chia theo champion.
- Utility/debug tools nằm trong repo/project riêng.
- Core NightSharp chỉ thay đổi khi public SDK contract cần thêm API chung.
- Mỗi thay đổi SDK public phải có version note, compatibility note và sample/test tối thiểu.

Quy tắc: nếu một feature chỉ phục vụ một plugin, feature đó nên nằm trong plugin. Chỉ đưa vào core/SDK khi nó là khả năng dùng chung cho nhiều plugin.

## Packaging `.NS`

Pipeline đề xuất:

1. Developer build raw plugin DLL bằng Visual Studio.
2. Packager đọc DLL và manifest/metadata.
3. Packager tạo `.NS` gồm payload, metadata, hash/signature và compatibility info.
4. Runtime chỉ load `.NS` nếu package hợp lệ và match SDK/ABI.
5. Developer mode vẫn có thể load raw DLL để debug, nhưng release channel nên dùng `.NS`.

Mục tiêu của `.NS` là đóng gói, kiểm tra tính toàn vẹn, quản lý version và bảo vệ distribution. Encryption/packer/VMP có thể là một lớp release hardening riêng, nhưng không nên làm hỏng dev workflow.

## Roadmap theo phase

### Phase 0 - Đóng băng contract trên giấy

- Viết rõ ranh giới giữa NightSharp core, NightSharp.SDK và external plugin.
- Chọn SDK versioning format.
- Chọn plugin metadata format.
- Chọn loader folder: `%AppData%\NightSharp\Plugins`.
- Chọn developer folder: `%AppData%\NightSharp\Plugins\Dev`.
- Định nghĩa raw DLL dev mode và `.NS` release mode.

### Phase 1 - Tạo NightSharp.SDK package

- Tạo project/package `NightSharp.SDK` dùng `NightSharp/SDK` làm source-of-truth.
- Đảm bảo Visual Studio có thể reference SDK để build plugin DLL riêng.
- Có Debug x64 và Release x64.
- Có toolset `v145`.
- Có SDK version/ABI id.
- Có docs public API cơ bản.

### Phase 2 - Plugin template và sample

- Tạo template plugin tối thiểu.
- Tạo sample Utility plugin để verify load/update/render/menu.
- Tạo sample Champion plugin để verify spell, target selector, orbwalker, prediction và menu.
- Đảm bảo sample không sửa core source.

### Phase 3 - External loader dev mode

- NightSharp scan folder Dev và load raw DLL khi developer mode bật.
- Validate SDK version trước khi gọi plugin.
- Register plugin vào menu/plugin registry như plugin nội bộ hiện tại.
- Thêm log rõ cho từng bước load, fail, unload.

### Phase 4 - Runtime safety

- Crash isolation cho OnLoad, OnUnload, OnUpdate, OnRender, OnMenu và event callbacks.
- Disable plugin bị crash thay vì làm sập cả NightSharp nếu có thể.
- Safe unload: cleanup event, menu, resources và background jobs.
- Performance timing theo plugin để biết plugin nào gây FPS drop.

### Phase 5 - `.NS` packager và release loading

- Tạo packager từ raw DLL sang `.NS`.
- Thêm metadata, hash/signature, SDK compatibility và dependency list.
- Runtime load `.NS` mặc định.
- Raw DLL chỉ dành cho developer mode.
- Thêm error UI/log khi package sai version, sai signature hoặc thiếu dependency.

### Phase 6 - Tách plugin nội bộ ra external

- Chuyển debug/test plugins thành sample/dev plugins riêng.
- Chuyển Awareness/Utility thành external plugin.
- Chuyển Champion logic thành external plugin theo champion/module.
- Giữ core chỉ còn SDK wrappers, loader, plugin manager, overlay/menu và hook/event bridge.

### Phase 7 - Public dev kit

- Viết docs cách tạo plugin mới.
- Viết docs public SDK modules.
- Viết docs packaging `.NS`.
- Tạo release zip cho NightSharp.SDK + template + sample.
- Tạo compatibility matrix theo SDK version và game build.

### Phase 8 - Lua/transpiler sau cùng

Lua hoặc transpiler Lua-to-C++ không nên là phase đầu. Nên có C++ SDK/plugin ABI ổn định trước, sau đó mới tính:

- Lua runtime/plugin adapter.
- Lua-to-C++ transpiler.
- Script marketplace/import layer.

Nếu làm Lua quá sớm, API chưa ổn định sẽ làm tăng cost sửa về sau.

## Acceptance criteria

- Một developer có thể clone template, reference `NightSharp.SDK`, build plugin DLL riêng bằng Visual Studio `v145`.
- Plugin có thể load trong developer mode mà không sửa `NightSharp.vcxproj`.
- Plugin có thể được pack thành `.NS` và load từ `%AppData%\NightSharp\Plugins`.
- Plugin mismatch SDK/ABI bị reject có log rõ, không crash.
- Evade, Awareness và Champion Logic có thể được phát triển trong repo/project riêng.
- Core merge conflict giảm vì plugin logic không nằm trong NightSharp core nữa.

## Việc không nên làm ở version đầu

- Không bắt đầu bằng Lua/transpiler.
- Không expose trực tiếp `Core/*` cho plugin author.
- Không để plugin gọi native internal function không qua SDK contract.
- Không unload plugin nếu chưa có cleanup/event unsubscription rõ ràng.
- Không bắt buộc packer/encryption trong ngày đầu vì sẽ làm chậm dev/debug.

## Hướng kết luận

Bước đúng nhất là làm `NightSharp.SDK` thành dev kit ổn định trước, đưa plugin ra ngoài core bằng raw DLL dev mode, sau đó mới thêm `.NS` packaging. Khi SDK contract đã ổn định, mọi người có thể chia việc theo plugin như Hanbot/EloBuddy style, nhưng vẫn giữ lợi thế C++ và SDK hiện tại của NightSharp.
