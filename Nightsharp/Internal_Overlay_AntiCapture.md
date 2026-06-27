# Anti-Capture (Bypass OBS) cho Internal Overlay

## 1. Vấn đề

### Kiến trúc hiện tại (External Overlay)
- Tạo cửa sổ Win32 riêng `g_hOverlay` (`WS_EX_LAYERED | WS_EX_TRANSPARENT`)
- Dùng DirectComposition + D3D11 swap chain riêng
- Gọi `SetWindowDisplayAffinity(g_hOverlay, WDA_EXCLUDEFROMCAPTURE)` để ẩn cửa sổ overlay khỏi screen capture
- **Kết quả**: OBS không thấy được overlay, nhưng vẫn thấy game bình thường

```cpp
// overlay/Overlay.cpp:413
void SetAntiCapture(bool enabled) {
    constexpr DWORD kWdaExcludeFromCapture = 0x00000011u;
    SetWindowDisplayAffinity(g_hOverlay, enabled ? kWdaExcludeFromCapture : 0);
}
```

### Kiến trúc Internal Overlay (mục tiêu)
- Hook `IDXGISwapChain::Present` của game qua vtable patch
- ImGui render trực tiếp lên backbuffer của game (dùng device/context của game)
- **Không còn cửa sổ overlay riêng** → `SetWindowDisplayAffinity` không áp dụng được
- ImGui content trở thành một phần của frame game → OBS Game Capture thấy toàn bộ

### Tại sao `SetWindowDisplayAffinity` không hoạt động?
| Yếu tố | External | Internal |
|--------|----------|----------|
| Cửa sổ overlay | Có (`g_hOverlay`) | Không (dùng HWND của game) |
| `WDA_EXCLUDEFROMCAPTURE` | Apply lên overlay window → ẩn overlay | Apply lên game window → ẩn **toàn bộ game** (vô dụng) |
| Swap chain | Tạo riêng (composition) | Dùng swap chain của game |
| OBS Game Capture | Không hook được overlay | Hook cùng swap chain → thấy hết |

---

## 2. Các giải pháp

### Giải pháp A: Hybrid Dual-Layer (KHUYẾN NGHỊ)

**Nguyên lý**: Tách nội dung thành 2 lớp — lớp internal (ESP/drawing) và lớp external (menu/sensitive UI).

| Lớp | Kiến trúc | OBS thấy? |
|-----|-----------|-----------|
| **Lớp 1: Game ESP** | Internal — hook `Present`, vẽ lên backbuffer game | Có (không nhạy cảm) |
| **Lớp 2: Menu/UI** | External — cửa sổ riêng + DirectComp + `WDA_EXCLUDEFROMCAPTURE` | Không |

**Cách hiện thực**:
1. Giữ nguyên `overlay/Overlay.cpp` cho menu (cửa sổ riêng + DirectComposition + anti-capture)
2. Thêm Present hook riêng chỉ render ESP/drawing lên swap chain của game
3. Menu vẫn chạy trên overlay window riêng, được bảo vệ bởi `SetWindowDisplayAffinity`

**Ưu điểm**:
- Menu ẩn khỏi OBS hoàn toàn (giữ nguyên cơ chế đã hoạt động)
- ESP vẫn hiện trên game (không cần ẩn)
- Không cần phát hiện OBS

**Nhược điểm**:
- Vẫn cần cửa sổ overlay (không hoàn toàn internal)
- Phải quản lý 2 render pipeline

**Code mẫu — Present hook cho ESP**:
```cpp
// hooks/PresentHook.cpp
typedef HRESULT(__stdcall* PresentFn)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

PresentFn g_origPresent = nullptr;
ID3D11Device* g_gameDevice = nullptr;
ID3D11DeviceContext* g_gameContext = nullptr;
ID3D11RenderTargetView* g_gameRTV = nullptr;

HRESULT __stdcall HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    // Lazy-init device/context từ swap chain của game
    if (!g_gameDevice) {
        pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_gameDevice);
        g_gameDevice->GetImmediateContext(&g_gameContext);
    }

    // Lấy backbuffer hiện tại
    ID3D11Texture2D* backBuffer = nullptr;
    pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer) {
        if (!g_gameRTV) {
            g_gameDevice->CreateRenderTargetView(backBuffer, nullptr, &g_gameRTV);
        }
        backBuffer->Release();
    }

    // Chỉ render ESP (KHÔNG render menu)
    g_gameContext->OMSetRenderTargets(1, &g_gameRTV, nullptr);
    // ... ImGui::NewFrame() → Plugins::OnRender() (ESP only) → ImGui::Render()
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return g_origPresent(pSwapChain, SyncInterval, Flags);
}
```

---

### Giải pháp B: Off-screen Render Target + DirectComposition Overlay

**Nguyên lý**: Render ImGui lên texture riêng, rồi composite lên game window qua DirectComposition visual có `WDA_EXCLUDEFROMCAPTURE`.

**Cách hiện thực**:
1. Hook `Present` của game
2. Trong hook, render ImGui lên **off-screen render target** (không phải backbuffer game)
3. Tạo DirectComposition visual riêng cho ImGui layer, set content = off-screen texture
4. Apply `WDA_EXCLUDEFROMCAPTURE` lên game window — **NHƯNG** điều này sẽ ẩn toàn bộ game

**Vấn đề**: `WDA_EXCLUDEFROMCAPTURE` apply trên window level, không thể apply trên từng visual. Nên giải pháp này **KHÔNG khả thi** trực tiếp.

**Variant khả thi**: Tạo cửa sổ con (child window) không transparent, render ImGui lên đó, apply `WDA_EXCLUDEFROMCAPTURE` lên child window. Nhưng child window sẽ che game → cần alpha blending → quay lại external overlay.

---

### Giải pháp C: Conditional Rendering — Phát hiện OBS đang capture

**Nguyên lý**: Trong Present hook, kiểm tra xem OBS có đang capture hay không. Nếu có, bỏ qua render ImGui.

**Cách phát hiện OBS**:
```cpp
bool IsOBSCapturing() {
    // Method 1: Check OBS process
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe = { sizeof(pe) };
    bool found = false;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"obs64.exe") == 0 ||
                _wcsicmp(pe.szExeFile, L"obs32.exe") == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return found;
}

// Trong Present hook:
HRESULT __stdcall HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!Config::StreamProtection::bypassObs || !IsOBSCapturing()) {
        // Render ImGui bình thường
        RenderImGui();
    }
    // Luôn gọi original Present
    return g_origPresent(pSwapChain, SyncInterval, Flags);
}
```

**Ưu điểm**:
- Đơn giản, dễ implement
- Hoàn toàn internal (không cần overlay window)

**Nhược điểm**:
- **Mất toàn bộ UI** khi OBS đang chạy (không chỉ ẩn khỏi OBS)
- Process detection dễ bị bypass/spoof
- Không phân biệt được OBS đang record hay chỉ mở
- Các phần mềm capture khác (Streamlabs, Discord, v.v.) cần thêm logic

---

### Giải pháp D: Hook Windows Graphics Capture (WGC) API

**Nguyên lý**: OBS dùng Windows Graphics Capture API (`CreateForWindow`, `CreateForMonitor`) để capture. Hook các API này để can thiệp.

**Các API cần hook**:
- `CreateCaptureItemForWindow` — OBS Window Capture
- `CreateCaptureItemForMonitor` — OBS Display/Screen Capture
- `Direct3D11CaptureFramePool::Create` — frame pool creation

**Cách hiện thực**:
```cpp
// Hook IGraphicsCaptureItemInterop::CreateForWindow
// Khi OBS yêu cầu capture game window, trả về item bị "blank"
// hoặc hook frame arrival để thay frame bằng frame không có overlay

// Phức tạp, cần hook COM interface, không khả thi cho mọi phiên bản OBS
```

**Ưu điểm**:
- Có thể ẩn selective content khỏi OBS mà vẫn thấy trên màn hình

**Nhược điểm**:
- Rất phức tạp, phụ thuộc phiên bản Windows
- OBS có thể fallback sang method capture khác (BitBlt, Desktop Duplication)
- Không hoạt động với OBS Game Capture (hook DXGI trực tiếp, không qua WGC)

---

### Giải pháp E: Present Hook — Render ImGui SAU khi OBS capture

**Nguyên lý**: OBS Game Capture hook `Present` và copy backbuffer. Nếu render ImGui **sau** khi OBS copy nhưng **trước** khi frame hiển thị, OBS sẽ không thấy ImGui.

**Cách hiện thực**:
- Không khả thi trực tiếp vì OBS hook cùng `Present` — thứ tự hook không đảm bảo
- Nếu NightSharp hook trước OBS: OBS sẽ thấy ImGui (vì render trước rồi OBS mới copy)
- Nếu NightSharp hook sau OBS: NightSharp render ImGui lên backbuffer, OBS đã copy xong → OBS không thấy

**Vấn đề**: Không kiểm soát được thứ tự hook. OBS inject sau thì hook sau (xấu cho ta). OBS inject trước thì hook trước (tốt cho ta).

**Variant**: Dùng `IDXGISwapChain1::Present1` với `DXGI_PRESENT_FLAG` để kiểm soát. Không khả thi vì flag không liên quan đến capture.

---

## 3. So sánh các giải pháp

| Giải pháp | Khả thi | Phức tạp | Ẩn menu khỏi OBS | Ẩn ESP khỏi OBS | Giữ UI khi OBS chạy |
|-----------|---------|----------|-------------------|------------------|---------------------|
| **A: Hybrid Dual-Layer** | ✅ Cao | Trung bình | ✅ | ❌ (ESP hiện) | ✅ |
| **B: Off-screen + DComp** | ❌ Thấp | Cao | N/A | N/A | N/A |
| **C: Conditional Render** | ✅ Cao | Thấp | ✅ (ẩn hết) | ✅ (ẩn hết) | ❌ (mất hết) |
| **D: Hook WGC** | ⚠️ Trung bình | Rất cao | ✅ | ✅ | ✅ |
| **E: Hook order trick** | ❌ Thấp | Trung bình | Không đảm bảo | Không đảm bảo | ✅ |

---

## 4. Khuyến nghị

### Short-term (nhanh nhất): Giải pháp A — Hybrid Dual-Layer

Giữ nguyên overlay external cho menu (đã hoạt động), thêm Present hook chỉ cho ESP:

1. **Giữ nguyên** `overlay/Overlay.cpp` — cửa sổ overlay + `SetAntiCapture` + menu
2. **Thêm mới** `hooks/PresentHook.cpp` — hook `Present` của game swap chain
3. **Tách render logic**:
   - `Plugins::PluginManager::OnRender()` → chia thành `OnRenderESP()` (internal) và `OnRenderMenu()` (external)
   - ESP draws trên Present hook (internal, OBS thấy)
   - Menu draws trên overlay window (external, OBS không thấy)
4. **Config**: Thêm option chọn nội dung nào ẩn khỏi OBS

### Long-term (tốt nhất): Giải pháp D — Hook WGC API

Phức tạp hơn nhưng cho kết quả tốt nhất:
- Ẩn được cả menu lẫn ESP khỏi OBS
- Vẫn thấy toàn bộ UI trên màn hình
- Cần nghiên cứu kỹ WGC API và COM hooking

### Không khuyến nghị: Giải pháp C

Chỉ dùng nếu cần solution tạm thời nhanh gọn, chấp nhận mất UI khi OBS chạy.

---

## 5. Implementation Plan — Giải pháp A

### Bước 1: Tạo Present Hook
```
hooks/
  PresentHook.h    — interface
  PresentHook.cpp  — vtable patch IDXGISwapChain::Present
```

### Bước 2: Tách render logic
```cpp
// Plugins/PluginManager.h
class PluginManager {
    void OnRenderESP();   // Render lên game swap chain (internal)
    void OnRenderMenu();  // Render lên overlay window (external)
};
```

### Bước 3: Cập nhật Overlay.cpp
- Giữ nguyên `SetAntiCapture` cho overlay window
- Menu chỉ render trên overlay (external)
- ESP chỉ render qua Present hook (internal)

### Bước 4: Cập nhật Config
```cpp
namespace StreamProtection {
    inline bool bypassObs = false;      // Ẩn menu khỏi OBS (overlay window)
    inline bool hideEspFromObs = false;  // Ẩn ESP khỏi OBS (conditional render)
}
```

---

## 6. Lưu ý kỹ thuật

### Vtable hook Present
```cpp
// IDXGISwapChain vtable: Present là index 8
// IDXGISwapChain1 vtable: Present cũng index 8 (inheritance)
void HookPresent(IDXGISwapChain* pSwapChain) {
    void** vtable = *(void***)pSwapChain;
    DWORD oldProtect = 0;
    VirtualProtect(&vtable[8], sizeof(void*), PAGE_READWRITE, &oldProtect);
    g_origPresent = (PresentFn)vtable[8];
    vtable[8] = &HookedPresent;
    VirtualProtect(&vtable[8], sizeof(void*), oldProtect, &oldProtect);
}
```

### Lấy game device từ swap chain
```cpp
// Trong HookedPresent:
if (!g_gameDevice) {
    pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_gameDevice);
    g_gameDevice->GetImmediateContext(&g_gameContext);
}
```

### ImGui với 2 backend
- Cần 2 ImGui context hoặc 1 context với 2 render target
- Khuyến nghị: 1 context, render 2 lần (ESP trước, Menu sau) với 2 `ImGui_ImplDX11_RenderDrawData` call lên 2 RTV khác nhau
- Hoặc: 2 context hoàn toàn tách biệt (phức tạp hơn nhưng an toàn)

### OBS Game Capture vs Window Capture
- **Game Capture**: Hook DXGI Present → giải pháp A (ESP hiện, menu ẩn)
- **Window Capture** (WGC): Capture theo window → `WDA_EXCLUDEFROMCAPTURE` trên overlay window ẩn menu
- **Display Capture**: Capture toàn màn hình → `WDA_EXCLUDEFROMCAPTURE` trên overlay window vẫn ẩn menu
