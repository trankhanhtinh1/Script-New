#include "includes.h"
#include "sdk/SDK.h"
#include "sdk/UI/MenuUI.h"
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

	// Track cursor position for SDK::CursorUtils
	SDK::CursorUtils::OnWndProc(uMsg, lParam);

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
		SDK::SpellDatabase::Init();  // Load Database.json (auto-detect path)
		SDK::DamageLibrary::Init();  // Load 9.7.269.2391.json (per-champion QWER damage)
		SDK::DamagePassives::Init(); // Init 60+ champion passive on-hit database
		SDK::DamageMastery::Init();  // Init rune/keystone damage database

		// Hook DamageCalc callbacks for DamagePassives + DamageMastery
		SDK::DamageCalc::SetPassiveDmgCallback([](const SDK::GameObject& s, const SDK::GameObject& t) {
			return SDK::DamagePassives::GetPassiveDamage(s, t);
		});
		SDK::DamageCalc::SetRuneDmgCallback([](const SDK::GameObject& s, const SDK::GameObject& t) {
			return SDK::DamageMastery::GetRuneDamage(s, t);
		});
		SDK::DamageCalc::SetRuneMultCallback([](const SDK::GameObject& s, const SDK::GameObject& t) {
			return SDK::DamageMastery::GetDamageMultiplier(s, t);
		});
		SDK::Invulnerable::Init();   // Init invulnerable database
		SDK::LastCast::Init();       // Init last cast tracking
		SDK::WeightedTargetSelector::Init(); // Init weight system for target selection
		SDK::ChampionPriority::Init();       // Init champion priority database (ADC=5, Tank=1)
		SDK::Teleport::Init();               // Init teleport/recall tracker
		SDK::GamePath::PathTracker::Init(); // Init path tracker for prediction
		SDK::Orbwalker::Init();
		SDK::AutoLevel::Init();

		// Initialize event subsystems — load JSON databases
		{
			// Use GetModuleHandleEx trick to find DLL directory
			char buf[MAX_PATH] = {};
			HMODULE hm = NULL;
			GetModuleHandleExA(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)&hkPresent, &hm);
			GetModuleFileNameA(hm, buf, MAX_PATH);
			std::string dllDir(buf);
			dllDir = dllDir.substr(0, dllDir.find_last_of("\\/"));

			std::string gapJson = dllDir + "\\sdk\\Data\\Gapclosers.json";
			std::string intJson = dllDir + "\\sdk\\Data\\InterruptableSpells.json";
			std::string globalIntJson = dllDir + "\\sdk\\Data\\GlobalInterruptableSpellsList.json";
			if (!SDK::Gapcloser::Init(gapJson))
				SDK::Gapcloser::InitDefault();
			if (!SDK::InterruptableSpell::Init(intJson))
				SDK::InterruptableSpell::InitDefault();
			if (!SDK::InterruptableSpell::InitGlobal(globalIntJson))
				SDK::InterruptableSpell::InitGlobalDefault();
		}

		// Connect Gapcloser to EventSystem's OnProcessSpellCast
		SDK::EventSystem::OnProcessSpellCast([](const SDK::SpellCastArgs& args) {
			SDK::Gapcloser::OnSpellCastDetected(args);
		});

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
		SDK::EventSystem::Update();      // Track game events (fires OnProcessSpellCast → Gapcloser)
		SDK::GamePath::PathTracker::Update(); // Track hero path changes for prediction
		SDK::HealthPrediction::Update(); // Track incoming damage
		SDK::SummonerTracker::Update();  // Track enemy summoner CDs
		SDK::RecallTracker::Update();    // Track enemy recalls
		SDK::Teleport::Update();         // Track teleport/recall channels (TP, Shen R, TF R, Hexgate)
		SDK::AutoLevel::Update();        // Auto level up skills
		SDK::DashDetector::Update();     // Track hero dashes
		SDK::StealthDetector::Update();  // Track stealth state changes
		SDK::InterruptableSpell::Update(); // Track interruptable channels
		SDK::TurretAggro::Update();      // Track turret aggro changes

		// Update all SDK MenuUI keybinds (only in game)
		SDK::MenuUI::Menu::UpdateAllKeyBinds();

		// Update and render all plugins
		Plugins::PluginManager::Get().OnUpdate();
		Plugins::PluginManager::Get().OnRender();

		// Process delayed actions
		SDK::DelayAction::Update();

		// Render PermaShow overlay (always-visible keybind status)
		SDK::MenuUI::PermaShow::Render();

		// Render notifications overlay
		SDK::MenuUI::Notifications::Render();

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
