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
bool sdkInitialized = false;
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

	// ====== SDK Game Logic — only when in game (LocalPlayer exists) ======
	bool inGame = Globals::base && SDK::Game::IsInGame();

	// One-time SDK initialization when first entering game
	if (inGame && !sdkInitialized) {
		SDK::Orbwalker::Init();
		SDK::AutoLevel::Init();

		auto& pm = Plugins::PluginManager::Get();
		pm.Register<Plugins::OrbwalkerPlugin>();
		pm.Register<Plugins::TargetSelectorPlugin>();
		pm.Register<Plugins::AwarenessPlugin>();
		pm.LoadAll();

		SDK::ConfigManager::LoadAll();
		sdkInitialized = true;
	}

	if (inGame) {
		SDK::Drawing::UpdateMatrix(); // Update W2S matrix FIRST
		SDK::GameObjects::Update();
		SDK::EventSystem::Update();      // Track game events
		SDK::HealthPrediction::Update(); // Track incoming damage
		SDK::SummonerTracker::Update();  // Track enemy summoner CDs
		SDK::RecallTracker::Update();    // Track enemy recalls
		SDK::AutoLevel::Update();        // Auto level up skills

		// Update all SDK MenuUI keybinds (only in game)
		SDK::MenuUI::Menu::UpdateAllKeyBinds();

		// Update and render all plugins
		Plugins::PluginManager::Get().OnUpdate();
		Plugins::PluginManager::Get().OnRender();

		// Auto-save config (every 5s if changed)
		SDK::ConfigManager::AutoSave();
	}

	// ====== Render Menu (BGX Style) — only when in game ======
	if (inGame) {
		BGXMenu::Render();
	} else if (Globals::base) {
		// Waiting for game — show small status indicator
		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		if (dl) {
			dl->AddText(ImVec2(10, 10), IM_COL32(255, 200, 100, 200),
				"[NightSharp] Waiting for game...");
		}
	}

	// ====== Info Overlay — only when in game ======
	if (inGame && (Config::Misc::showFPS || Config::Misc::showPing || Config::Misc::showGameTime)) {
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(0.35f);
		ImGui::Begin("##InfoOverlay", nullptr,
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);

		if (Config::Misc::showFPS) {
			ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);
		}
		if (Config::Misc::showGameTime) {
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
