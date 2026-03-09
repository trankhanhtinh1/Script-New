#define _CRT_SECURE_NO_WARNINGS
#include "includes.h"
#include "sdk/SDK.h"
#include "sdk/UI/MenuUI.h"
#include "plugins/PluginManager.h"
#include "plugins/core/OrbwalkerPlugin.h"
#include "plugins/core/TargetSelectorPlugin.h"
#include "plugins/core/RenderTestPlugin.h"
#include "plugins/champions/EzrealPlugin.h"
#include "plugins/utility/EzEvadePlugin.h"
#include "menu/NightSharpMenu.h"
#include "sdk/Utils/DebugConsole.h"
#include <Psapi.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <cstdarg>
#include <cstdio>
#include <winternl.h>

// ========================================================================
// 1. FORWARD EXPORTS CỦA HID.DLL THEO ĐÚNG DUMPBIN CỦA BẠN
// ========================================================================
#pragma comment(linker, "/export:HidD_FlushQueue=C:\\Windows\\System32\\hid.HidD_FlushQueue")
#pragma comment(linker, "/export:HidD_FreePreparsedData=C:\\Windows\\System32\\hid.HidD_FreePreparsedData")
#pragma comment(linker, "/export:HidD_GetAttributes=C:\\Windows\\System32\\hid.HidD_GetAttributes")
#pragma comment(linker, "/export:HidD_GetConfiguration=C:\\Windows\\System32\\hid.HidD_GetConfiguration")
#pragma comment(linker, "/export:HidD_GetFeature=C:\\Windows\\System32\\hid.HidD_GetFeature")
#pragma comment(linker, "/export:HidD_GetHidGuid=C:\\Windows\\System32\\hid.HidD_GetHidGuid")
#pragma comment(linker, "/export:HidD_GetIndexedString=C:\\Windows\\System32\\hid.HidD_GetIndexedString")
#pragma comment(linker, "/export:HidD_GetInputReport=C:\\Windows\\System32\\hid.HidD_GetInputReport")
#pragma comment(linker, "/export:HidD_GetManufacturerString=C:\\Windows\\System32\\hid.HidD_GetManufacturerString")
#pragma comment(linker, "/export:HidD_GetMsGenreDescriptor=C:\\Windows\\System32\\hid.HidD_GetMsGenreDescriptor")
#pragma comment(linker, "/export:HidD_GetNumInputBuffers=C:\\Windows\\System32\\hid.HidD_GetNumInputBuffers")
#pragma comment(linker, "/export:HidD_GetPhysicalDescriptor=C:\\Windows\\System32\\hid.HidD_GetPhysicalDescriptor")
#pragma comment(linker, "/export:HidD_GetPreparsedData=C:\\Windows\\System32\\hid.HidD_GetPreparsedData")
#pragma comment(linker, "/export:HidD_GetProductString=C:\\Windows\\System32\\hid.HidD_GetProductString")
#pragma comment(linker, "/export:HidD_GetSerialNumberString=C:\\Windows\\System32\\hid.HidD_GetSerialNumberString")
#pragma comment(linker, "/export:HidD_Hello=C:\\Windows\\System32\\hid.HidD_Hello")
#pragma comment(linker, "/export:HidD_SetConfiguration=C:\\Windows\\System32\\hid.HidD_SetConfiguration")
#pragma comment(linker, "/export:HidD_SetFeature=C:\\Windows\\System32\\hid.HidD_SetFeature")
#pragma comment(linker, "/export:HidD_SetNumInputBuffers=C:\\Windows\\System32\\hid.HidD_SetNumInputBuffers")
#pragma comment(linker, "/export:HidD_SetOutputReport=C:\\Windows\\System32\\hid.HidD_SetOutputReport")
#pragma comment(linker, "/export:HidP_GetButtonCaps=C:\\Windows\\System32\\hid.HidP_GetButtonCaps")
#pragma comment(linker, "/export:HidP_GetCaps=C:\\Windows\\System32\\hid.HidP_GetCaps")
#pragma comment(linker, "/export:HidP_GetData=C:\\Windows\\System32\\hid.HidP_GetData")
#pragma comment(linker, "/export:HidP_GetExtendedAttributes=C:\\Windows\\System32\\hid.HidP_GetExtendedAttributes")
#pragma comment(linker, "/export:HidP_GetLinkCollectionNodes=C:\\Windows\\System32\\hid.HidP_GetLinkCollectionNodes")
#pragma comment(linker, "/export:HidP_GetScaledUsageValue=C:\\Windows\\System32\\hid.HidP_GetScaledUsageValue")
#pragma comment(linker, "/export:HidP_GetSpecificButtonCaps=C:\\Windows\\System32\\hid.HidP_GetSpecificButtonCaps")
#pragma comment(linker, "/export:HidP_GetSpecificValueCaps=C:\\Windows\\System32\\hid.HidP_GetSpecificValueCaps")
#pragma comment(linker, "/export:HidP_GetUsageValue=C:\\Windows\\System32\\hid.HidP_GetUsageValue")
#pragma comment(linker, "/export:HidP_GetUsageValueArray=C:\\Windows\\System32\\hid.HidP_GetUsageValueArray")
#pragma comment(linker, "/export:HidP_GetUsages=C:\\Windows\\System32\\hid.HidP_GetUsages")
#pragma comment(linker, "/export:HidP_GetUsagesEx=C:\\Windows\\System32\\hid.HidP_GetUsagesEx")
#pragma comment(linker, "/export:HidP_GetValueCaps=C:\\Windows\\System32\\hid.HidP_GetValueCaps")
#pragma comment(linker, "/export:HidP_InitializeReportForID=C:\\Windows\\System32\\hid.HidP_InitializeReportForID")
#pragma comment(linker, "/export:HidP_MaxDataListLength=C:\\Windows\\System32\\hid.HidP_MaxDataListLength")
#pragma comment(linker, "/export:HidP_MaxUsageListLength=C:\\Windows\\System32\\hid.HidP_MaxUsageListLength")
#pragma comment(linker, "/export:HidP_SetData=C:\\Windows\\System32\\hid.HidP_SetData")
#pragma comment(linker, "/export:HidP_SetScaledUsageValue=C:\\Windows\\System32\\hid.HidP_SetScaledUsageValue")
#pragma comment(linker, "/export:HidP_SetUsageValue=C:\\Windows\\System32\\hid.HidP_SetUsageValue")
#pragma comment(linker, "/export:HidP_SetUsageValueArray=C:\\Windows\\System32\\hid.HidP_SetUsageValueArray")
#pragma comment(linker, "/export:HidP_SetUsages=C:\\Windows\\System32\\hid.HidP_SetUsages")
#pragma comment(linker, "/export:HidP_TranslateUsagesToI8042ScanCodes=C:\\Windows\\System32\\hid.HidP_TranslateUsagesToI8042ScanCodes")
#pragma comment(linker, "/export:HidP_UnsetUsages=C:\\Windows\\System32\\hid.HidP_UnsetUsages")
#pragma comment(linker, "/export:HidP_UsageListDifference=C:\\Windows\\System32\\hid.HidP_UsageListDifference")

// ========================================================================


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
	bool menuVisible = NightSharpMenu::showMenu || ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
	if (menuVisible && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

bool init = false;
bool sdkInitialized = false;
bool debugLogged = false;

namespace HookConfig {
	constexpr bool EnableKiero = true;
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	// Init debug console on first call
	if (!debugLogged) {
		DebugConsole::Init();
		DebugConsole::Log("=== DLL Loaded ===");
		debugLogged = true;
	}

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
		// Resolve current DLL directory (hid.dll)
		char modulePathBuf[MAX_PATH] = {};
		HMODULE hm = NULL;
		GetModuleHandleExA(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)&hkPresent, &hm);
		GetModuleFileNameA(hm, modulePathBuf, MAX_PATH);
		std::string dllDir(modulePathBuf);
		dllDir = dllDir.substr(0, dllDir.find_last_of("\\/"));

		SDK::SpellDatabase::Init();  // Load Database.json (auto-detect path)
		SDK::DamageLibrary::Init();  // Load 9.7.269.2391.json (per-champion QWER damage)
		SDK::DamagePassives::Init(); // Init 60+ champion passive on-hit database
		SDK::DamageMastery::Init();  // Init rune/keystone damage database
		SDK::SpellCaster::SetDamageCallback([](
			const SDK::GameObject& source,
			const SDK::GameObject& target,
			SDK::SpellSlotId slot,
			int stage) {
			return (float)SDK::DamageLibrary::GetSpellDamage(
				source,
				target,
				slot,
				static_cast<SDK::DamageStage>(stage));
		});

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
		SDK::SkillshotTracker::Init();       // Init skillshot auto-detection via EventSystem
		SDK::Orbwalker::Init();
		SDK::AutoLevel::Init();
		SDK::Map::Init();                    // Detect current map type
		SDK::GameObjects::Update();          // Prime local player before champion plugin auto-load

		// Initialize event subsystems — load JSON databases
		{
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
		pm.Register<Plugins::RenderTestPlugin>();
		pm.Register<Plugins::EzrealPlugin>();
		pm.Register<Plugins::EzEvadePlugin>();
		// SDK Orbwalker is default; OrbwalkerPlugin is optional override.
		pm.SetAutoLoad("Orbwalker", false);
		pm.SetAutoLoad("Target Selector", true);
		pm.SetAutoLoad("EzEvade", true);
		pm.LoadAuto();

		// DEBUG: Log loaded plugins
		DebugConsole::Log("=== Plugins Loaded ===");
		for (auto& p : pm.GetPlugins()) {
			DebugConsole::Log("  - %s: loaded=%d enabled=%d", 
				p->GetName(), p->IsLoaded() ? 1 : 0, p->IsEnabled() ? 1 : 0);
		}

		// Save config right next to hid.dll
		SDK::ConfigManager::SetConfigPath(dllDir);
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
		SDK::SkillshotTracker::Update(); // Track active enemy skillshots (Evade foundation)

		// Update MenuUI keybinds before orbwalker reads them this frame.
		SDK::MenuUI::Menu::UpdateAllKeyBinds();

		if (auto* orbwalkerOverride = Plugins::PluginManager::Get().Find("Orbwalker")) {
			SDK::Orbwalker::SetPluginOverrideActive(
				orbwalkerOverride->IsLoaded() && orbwalkerOverride->IsEnabled());
		}

		// Default SDK orbwalker runs every frame unless OrbwalkerPlugin override is active.
		SDK::Orbwalker::OnUpdate();

		// Update and render all plugins
		Plugins::PluginManager::Get().OnUpdate();
		SDK::Orbwalker::OnRender();
		Plugins::PluginManager::Get().OnRender();

		// DEBUG: Log after plugin update - ActiveMode is now synced by OrbwalkerPlugin.
		auto modeToString = [](int mode) -> const char* {
			switch ((SDK::OrbwalkingMode)mode) {
			case SDK::OrbwalkingMode::Combo: return "Combo";
			case SDK::OrbwalkingMode::Harass: return "Harass";
			case SDK::OrbwalkingMode::LastHit: return "LastHit";
			case SDK::OrbwalkingMode::LaneClear: return "LaneClear";
			case SDK::OrbwalkingMode::Flee: return "Flee";
			default: return "None";
			}
		};
		static int lastMode = 0;
		int currentMode = (int)SDK::Orbwalker::ActiveMode;
		if (currentMode != 0 && currentMode != lastMode) {
			DebugConsole::Log(">>> ORBWALK MODE ACTIVATED! %s (%d)", modeToString(currentMode), currentMode);
		}
		lastMode = currentMode;

		// Process delayed actions
		SDK::DelayAction::Update();

		// Render PermaShow overlay (always-visible keybind status)
		SDK::MenuUI::PermaShow::Render();

		// Render notifications overlay
		SDK::MenuUI::Notifications::Render();

		// Auto-save config (every 5s if changed)
		SDK::ConfigManager::AutoSave();
	}

	// ====== Render Menu (NightSharp Style) — only when in game ======
	if (inGame) {
		NightSharpMenu::Render();
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
	
	// Render directly without spoofcall (could cause crash/instability)
	return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI MainThread(LPVOID lpReserved)
{
	bool init_hook = false;
	if (HookConfig::EnableKiero) {
		do
		{
			if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
			{
				kiero::bind(8, (void**)&oPresent, hkPresent);
				NightSharpMenu::SetHookBackendLabel("NightSharp");
				DebugConsole::Log("[HOOK] Backend: Kiero");
				init_hook = true;
			}
		} while (!init_hook);
	}
	if (!init_hook) {
		NightSharpMenu::SetHookBackendLabel("NightSharp");
		DebugConsole::Log("[HOOK] Backend: None (init failed)");
	}

	return TRUE;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hMod);
		// FIX: Sử dụng QueueUserWorkItem thay vì CreateThread để tránh NtCreateThreadEx INT3 trap
		QueueUserWorkItem((LPTHREAD_START_ROUTINE)MainThread, nullptr, WT_EXECUTEDEFAULT);
        // Start Heartbeat Log capture
        //QueueUserWorkItem((LPTHREAD_START_ROUTINE)MainCheatThread, nullptr, WT_EXECUTEDEFAULT);
		break;
	case DLL_PROCESS_DETACH:
		SDK::ConfigManager::SaveAll();
		DebugConsole::Close();
		kiero::shutdown();
		break;
	}
	return TRUE;
}
