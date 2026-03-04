#include "includes.h"
#include "sdk/SDK.h"
#include "sdk/MenuUI.h"
#include "plugins/PluginManager.h"
#include "plugins/awareness/Awareness.h"
#include "plugins/core/OrbwalkerPlugin.h"
#include "plugins/core/TargetSelectorPlugin.h"
#include "menu/BGXMenu.h"
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

	// Pass input to ImGui when menu is visible (toggle or shift-hold)
	bool menuVisible = BGXMenu::showMenu || ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
	if (menuVisible && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
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

			// Register core plugins
			auto& pm = Plugins::PluginManager::Get();
			pm.Register<Plugins::OrbwalkerPlugin>();
			pm.Register<Plugins::TargetSelectorPlugin>();
			pm.Register<Plugins::AwarenessPlugin>();
			pm.LoadAll(); // Load all by default

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
		SDK::Drawing::UpdateMatrix(); // Update W2S matrix FIRST
		SDK::GameObjects::Update();

		// Update all SDK MenuUI keybinds
		SDK::MenuUI::Menu::UpdateAllKeyBinds();

		// Update and render all plugins
		Plugins::PluginManager::Get().OnUpdate();
		Plugins::PluginManager::Get().OnRender();
	}

	// ====== Render Menu (BGX Style) ======
	BGXMenu::Render();

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
