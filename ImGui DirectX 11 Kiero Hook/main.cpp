#include "includes.h"
#include "sdk/SDK.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Present oPresent;
HWND window = NULL;
WNDPROC oWndProc;
ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView;

void InitImGui()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(pDevice, pContext);
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	if (Config::showMenu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

bool init = false;
HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (!init)
	{
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)& pDevice)))
		{
			pDevice->GetImmediateContext(&pContext);
			DXGI_SWAP_CHAIN_DESC sd;
			pSwapChain->GetDesc(&sd);
			window = sd.OutputWindow;
			ID3D11Texture2D* pBackBuffer;
			pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)& pBackBuffer);
			pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
			InitImGui();

			// Initialize module base
			Globals::Init();

			init = true;
		}

		else
			return oPresent(pSwapChain, SyncInterval, Flags);
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// ====== SDK Game Logic (every frame) ======
	if (Globals::base) {
		SDK::GameObjects::Update();

		// Semi-Cast Hotkeys (cast at mouse position)
		if (SDK::Game::ShouldProcessInput() && SDK::GameObjects::Player.IsValid()) {

			// S = Semi Q (Ezreal Q at mouse)
			if (Config::Spells::semiQEnabled) {
				static bool sKeyWasDown = false;
				bool sKeyIsDown = (GetAsyncKeyState(Config::Spells::semiQKey) & 0x8000) != 0;
				if (sKeyIsDown && !sKeyWasDown) {
					auto Q = SDK::SpellCaster::Line(SDK::SpellSlotId::Q, 1150, 2000, 120, 0.25f);
					if (Config::Spells::castMethod == 0)
						Q.CastAtMouse();        // Method 0: Function call
					else
						Q.CastAtMouseViaKey();  // Method 1: Key simulation
				}
				sKeyWasDown = sKeyIsDown;
			}

			// T = Semi R (Ezreal R at mouse)
			if (Config::Spells::semiREnabled) {
				static bool tKeyWasDown = false;
				bool tKeyIsDown = (GetAsyncKeyState(Config::Spells::semiRKey) & 0x8000) != 0;
				if (tKeyIsDown && !tKeyWasDown) {
					auto R = SDK::SpellCaster::Line(SDK::SpellSlotId::R, 20000, 2000, 320, 1.0f);
					if (Config::Spells::castMethod == 0)
						R.CastAtMouse();        // Method 0: Function call
					else
						R.CastAtMouseViaKey();  // Method 1: Key simulation
				}
				tKeyWasDown = tKeyIsDown;
			}
		}
	}

	// ====== Render Menu ======
	Menu::Render();

	// ====== Info Overlay (always visible) ======
	if (Config::Misc::showFPS || Config::Misc::showPing || Config::Misc::showGameTime) {
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(0.35f);
		ImGui::Begin("##InfoOverlay", nullptr,
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);

		if (Config::Misc::showFPS) {
			ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);
		}
		if (Config::Misc::showGameTime && Globals::base) {
			float gt = Globals::Read<float>(Globals::base + Offset::Global::GameTime);
			if (gt > 0) {
				ImGui::Text("Time: %d:%02d", (int)gt / 60, (int)gt % 60);
			}
		}

		ImGui::End();
	}

	// ====== Cast Spell Debug Overlay ======
	if (Config::Spells::showCastDebug && Globals::base) {
		float curTime = SDK::Game::GetTime();
		float timeSinceCast = curTime - SDK::SpellCaster::LastCastTime;

		// Show for 3 seconds after last cast attempt
		if (SDK::SpellCaster::LastCastResult != 0 && timeSinceCast < 3.0f) {
			ImGui::SetNextWindowPos(ImVec2(10, 60), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.7f);
			ImGui::Begin("##CastDebug", nullptr,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);

			if (SDK::SpellCaster::LastCastResult > 0) {
				ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
					"[CAST] %s (%.1fs ago)", SDK::SpellCaster::LastCastError, timeSinceCast);
			} else {
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
					"[CAST] FAIL #%d: %s (%.1fs ago)",
					SDK::SpellCaster::LastCastResult,
					SDK::SpellCaster::LastCastError,
					timeSinceCast);
			}

			ImGui::End();
		}

		// Always show semi-cast status if enabled
		if (Config::Spells::semiQEnabled || Config::Spells::semiREnabled) {
			ImDrawList* dl = ImGui::GetBackgroundDrawList();
			if (dl) {
				float y = 40.0f;
				if (Config::Spells::semiQEnabled) {
					dl->AddText(ImVec2(10, y), IM_COL32(200, 200, 100, 150),
						"[S] Semi-Q active");
					y += 15.0f;
				}
				if (Config::Spells::semiREnabled) {
					dl->AddText(ImVec2(10, y), IM_COL32(200, 200, 100, 150),
						"[T] Semi-R active");
				}
			}
		}
	}

	ImGui::Render();

	pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI MainThread(LPVOID lpReserved)
{
	bool init_hook = false;
	do
	{
		if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
		{
			kiero::bind(8, (void**)& oPresent, hkPresent);
			init_hook = true;
		}
	} while (!init_hook);
	return TRUE;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hMod);
		CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
		break;
	case DLL_PROCESS_DETACH:
		kiero::shutdown();
		break;
	}
	return TRUE;
}