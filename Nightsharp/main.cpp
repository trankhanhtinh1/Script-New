#define _CRT_SECURE_NO_WARNINGS
#include "includes.h"
#include "sdk/SDK.h"
#include "sdk/UI/MenuUI.h"
#include "plugins/PluginManager.h"
#include "plugins/core/OrbwalkerPlugin.h"
#include "plugins/core/TargetSelectorPlugin.h"
#include "plugins/core/RenderTestPlugin.h"
#include "plugins/champions/EzrealPlugin.h"
#include "plugins/utility/evade/EvadePlugin.h"
#include "plugins/utility/ezevade/EzEvadePlugin.h"
#include "menu/NightSharpMenu.h"
#include "sdk/Utils/DebugConsole.h"
#include "core/AntiBan.h"
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
using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
ResizeBuffersFn oResizeBuffers = nullptr;

HWND window = NULL;
WNDPROC oWndProc;
ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView = nullptr;

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
float sdkWarmupStart = 0.0f;  // When IsInGame first became true
constexpr float SDK_WARMUP_SECONDS = 3.0f; // Wait 3 seconds before heavy init
static auto g_scriptClockStart = std::chrono::steady_clock::now();
static HMODULE g_hSelfModule = nullptr; // For deferred AntiBan init

namespace HookConfig {
	constexpr bool EnableKiero = true;
}

// ========================================================================
// SEH-safe helper functions — extracted to avoid C2712
// MSVC does not allow __try in functions with C++ objects needing unwinding.
// These helpers contain ONLY POD types + C calls, so __try is safe here.
// ========================================================================

// Safe IsInGame check with SEH
static bool SafeIsInGame() {
	__try {
		return Globals::base && SDK::Game::IsInGame();
	} __except(1) {
		return false;
	}
}

// Safe GetTime with SEH
static float SafeGetTime() {
	__try {
		return SDK::Game::GetTime();
	} __except(1) {
		return 0.0f;
	}
}

// Safe SDK core update with SEH — event tracking only (runs per script tick)
static void SafeUpdateSDKCore() {
	__try {
		SDK::GameObjects::Update();
		SDK::EventSystem::Update();
		SDK::GamePath::PathTracker::Update();
		SDK::HealthPrediction::Update();
		SDK::SummonerTracker::Update();
		SDK::RecallTracker::Update();
		SDK::Teleport::Update();
		SDK::AutoLevel::Update();
		SDK::DashDetector::Update();
		SDK::StealthDetector::Update();
		SDK::InterruptableSpell::Update();
		SDK::TurretAggro::Update();
		SDK::SkillshotTracker::Update();
	} __except(1) {
		// SDK frame update crashed — silently recover next frame
	}
}

// Safe plugin logic update with SEH (runs per script tick — 45 FPS)
static void SafeUpdatePluginLogic() {
	__try {
		SDK::MenuUI::Menu::UpdateAllKeyBinds();

		if (auto* orbwalkerOverride = Plugins::PluginManager::Get().Find("Orbwalker 2.0")) {
			SDK::Orbwalker::SetPluginOverrideActive(
				orbwalkerOverride->IsLoaded() && orbwalkerOverride->IsEnabled());
		}

		SDK::Orbwalker::OnUpdate();

		Plugins::PluginManager::Get().OnUpdate();
		SDK::EventSystem::RunScriptTick();

		SDK::DelayAction::Update();
		SDK::ConfigManager::AutoSave();
	} __except(1) {
		// Plugin/orbwalker frame update crashed — silently recover next frame
	}
}

// Safe render-only update with SEH (runs every Present frame — full FPS)
static void SafeUpdateRender() {
	__try {
		SDK::Drawing::UpdateMatrix();
		SDK::Orbwalker::OnRender();
		Plugins::PluginManager::Get().OnRender();
		SDK::MenuUI::PermaShow::Render();
		SDK::MenuUI::Notifications::Render();
	} __except(1) {
		// Render crashed — silently recover
	}
}

// Safe SDK initialization with SEH (InitializeSDK uses std::string internally)
static bool InitializeSDK(); // forward decl
static bool SafeInitializeSDK() {
	__try {
		return InitializeSDK();
	} __except(1) {
		DebugConsole::Log("[INIT] CRITICAL: SDK init crashed! Will retry...");
		return false;
	}
}

// Safe menu render with SEH
static void SafeRenderMenu(bool inGame) {
	__try {
		if (inGame) {
			NightSharpMenu::Render();
		} else if (Globals::base) {
			ImDrawList* dl = ImGui::GetBackgroundDrawList();
			if (dl) {
				dl->AddText(ImVec2(10, 10), IM_COL32(255, 200, 100, 200),
					"[NightSharp] Waiting for game...");
			}
		}
	} __except(1) {
		// Menu render crashed — silently recover
	}
}

// Safe info overlay render with SEH
static void SafeRenderOverlay() {
	__try {
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
			if (Config::Misc::showGameTime) {
				float gt = Globals::Read<float>(Globals::base + Offset::Global::GameTime);
				if (gt > 0) {
					ImGui::Text("Time: %d:%02d", (int)gt / 60, (int)gt % 60);
				}
			}

			ImGui::End();
		}
	} __except(1) {
		// Overlay render crashed — silently recover
	}
}

// ========================================================================
// ResizeBuffers hook — CRITICAL for preventing crash on resolution change
// Without this, the old RenderTargetView becomes invalid after resize
// ========================================================================
HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount,
	UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT Flags)
{
	// Release old RTV before resize
	if (mainRenderTargetView) {
		mainRenderTargetView->Release();
		mainRenderTargetView = nullptr;
	}

	HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, Flags);

	// Recreate RTV after resize
	if (SUCCEEDED(hr) && pDevice) {
		ID3D11Texture2D* pBackBuffer = nullptr;
		if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer))) {
			pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
		}
	}

	return hr;
}

// SDK initialization (no __try needed — each sub-Init has its own safety)
static bool InitializeSDK() {
	// Resolve current DLL directory (hid.dll)
	char modulePathBuf[MAX_PATH] = {};
	HMODULE hm = NULL;
	GetModuleHandleExA(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)&InitializeSDK, &hm);
	GetModuleFileNameA(hm, modulePathBuf, MAX_PATH);
	std::string dllDir(modulePathBuf);
	dllDir = dllDir.substr(0, dllDir.find_last_of("\\/"));

	SDK::SpellDatabase::Init();  // Load Database.json (auto-detect path)
	SDK::DamageLibrary::Init();  // Load 9.7.269.2391.json (per-champion QWER damage)
	SDK::DamagePassives::Init(); // Init 60+ champion passive on-hit database
	SDK::DamageMastery::Init();  // Init rune/keystone damage database
	SDK::SpellCaster::SetDamageCallback([](const SDK::GameObject& source, const SDK::GameObject& target, SDK::SpellSlotId slot, int stage) {
		return (float)SDK::DamageLibrary::GetSpellDamage(source, target, slot, static_cast<SDK::DamageStage>(stage));
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
	pm.Register<Plugins::EvadePlugin>();
	pm.Register<Plugins::EzEvadePlugin>();
	// SDK Orbwalker is default; OrbwalkerPlugin is optional override.
	pm.SetAutoLoad("Orbwalker 2.0", true);
	pm.SetAutoLoad("Target Selector", true);
	pm.SetAutoLoad("Evade", false);  // Tắt Evade cũ
	pm.SetAutoLoad("EzEvade", true);  // Chỉ dùng EzEvade
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
	// Initialize script tick clock
	auto nowMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - g_scriptClockStart).count();
	SDK::Game::LastScriptTickWallClock = nowMs - SDK::Game::ScriptTickIntervalMs;
	return true;
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
	bool inGame = SafeIsInGame();

	// ---- Warmup timer: ensure game is stable before heavy init ----
	// This prevents crash from premature memory access during loading screen
	if (inGame && !sdkInitialized) {
		float gameTime = SafeGetTime();

		if (sdkWarmupStart <= 0.0f) {
			sdkWarmupStart = gameTime;
			DebugConsole::Log("[INIT] Game detected, starting warmup (%.1fs)...", gameTime);
		}

		// Wait for game to be stable: gameTime > 5s AND warmup elapsed
		if (gameTime < 5.0f || (gameTime - sdkWarmupStart) < SDK_WARMUP_SECONDS) {
			// Show waiting message while warming up
			ImDrawList* dl = ImGui::GetBackgroundDrawList();
			if (dl) {
				char warmupBuf[128];
				snprintf(warmupBuf, sizeof(warmupBuf),
					"[NightSharp] Initializing... (%.1fs / %.1fs)",
					gameTime - sdkWarmupStart, SDK_WARMUP_SECONDS);
				dl->AddText(ImVec2(10, 10), IM_COL32(255, 200, 100, 200), warmupBuf);
			}
			ImGui::Render();
			if (mainRenderTargetView) {
				pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
				ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			}
			return oPresent(pSwapChain, SyncInterval, Flags);
		}
	}

	// One-time SDK initialization when first entering game (AFTER warmup)
	if (inGame && !sdkInitialized) {
		DebugConsole::Log("[INIT] Warmup complete, initializing SDK...");
		sdkInitialized = SafeInitializeSDK();
		if (!sdkInitialized) {
			DebugConsole::Log("[INIT] SDK init failed, will retry next frame...");
			sdkWarmupStart = 0.0f;
		} else {
			DebugConsole::Log("[INIT] SDK initialized successfully!");
		}
	}

	if (inGame && sdkInitialized) {
		// Calculate wall clock time for tick limiter
		const auto nowMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - g_scriptClockStart).count();

		// ---- Script tick (rate-limited to 45 FPS) ----
		// SDK updates + plugin logic only run when enough time has passed.
		// This reduces CPU/memory usage ~6x vs running every render frame.
		if (SDK::Game::ShouldRunScriptTick(nowMs)) {
			SDK::Game::AdvanceScriptFrame(nowMs);
			SafeUpdateSDKCore();
			SafeUpdatePluginLogic();
		}

		// ---- Render (runs every Present frame — full FPS) ----
		SafeUpdateRender();

		// Log mode changes
		static int lastMode = 0;
		int currentMode = (int)SDK::Orbwalker::ActiveMode;
		if (currentMode != 0 && currentMode != lastMode) {
			const char* names[] = {"None","Combo","Harass","LastHit","LaneClear","Flee"};
			DebugConsole::Log(">>> ORBWALK MODE: %s (%d)",
				(currentMode >= 0 && currentMode <= 5) ? names[currentMode] : "?", currentMode);
		}
		lastMode = currentMode;
	}

	// ====== Render Menu + Overlays — ALL wrapped in SEH ======
	SafeRenderMenu(inGame);
	if (inGame) {
		SafeRenderOverlay();
	}

	ImGui::Render();

	if (mainRenderTargetView) {
		pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}
	
	return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI MainThread(LPVOID lpReserved)
{
	// Deferred AntiBan: unlink module from PEB + erase PE header
	// Must run AFTER DllMain returns (loader lock released)
	if (g_hSelfModule) {
		AntiBan::DeferredInit(g_hSelfModule);
	}

	// Delay 8s to wait for the game to complete rendering initialization (avoid crash on loading screen)
	Sleep(8000);

	bool init_hook = false;
	if (HookConfig::EnableKiero) {
		int attempts = 0;
		do
		{
			if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
			{
				kiero::bind(8, (void**)&oPresent, hkPresent);
				// Hook ResizeBuffers (vtable index 13) to handle resolution changes
				kiero::bind(13, (void**)&oResizeBuffers, hkResizeBuffers);
				NightSharpMenu::SetHookBackendLabel("NightSharp");
				DebugConsole::Log("[HOOK] Backend: Kiero (Present + ResizeBuffers)");
				init_hook = true;
			}
			else {
				// FIX: Sleep to prevent CPU spin loop that causes deadlocks
				// with D3D initialization on the main thread
				Sleep(100);
				attempts++;
				if (attempts > 300) { // 30 seconds max wait
					DebugConsole::Log("[HOOK] Failed after 300 attempts, giving up");
					break;
				}
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
		g_hSelfModule = hMod;
		// === ANTI-BAN (DllMain — safe ops only) ===
		// Resolve SSNs + Clean PEB (no API calls, no hooks triggered)
		// UnlinkModule + ErasePEHeader deferred to MainThread
		AntiBan::Initialize(hMod);
		// Use QueueUserWorkItem to avoid NtCreateThreadEx INT3 trap
		QueueUserWorkItem((LPTHREAD_START_ROUTINE)MainThread, nullptr, WT_EXECUTEDEFAULT);
		break;
	case DLL_PROCESS_DETACH:
		SDK::ConfigManager::SaveAll();
		DebugConsole::Close();
		kiero::shutdown();
		break;
	}
	return TRUE;
}
