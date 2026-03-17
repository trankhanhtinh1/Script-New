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
#include "core/OverlayWindow.h"
#include "core/OverlayRenderer.h"
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
// GLOBALS
// ========================================================================

bool sdkInitialized = false;
bool debugLogged = false;
float sdkWarmupStart = 0.0f;
constexpr float SDK_WARMUP_SECONDS = 3.0f;
static auto g_scriptClockStart = std::chrono::steady_clock::now();
static HMODULE g_hSelfModule = nullptr;

// ========================================================================
// SEH-safe helper functions
// ========================================================================

static bool SafeIsInGame() {
	__try {
		return Globals::base && SDK::Game::IsInGame();
	} __except(1) { return false; }
}

static float SafeGetTime() {
	__try {
		return SDK::Game::GetTime();
	} __except(1) { return 0.0f; }
}

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
	} __except(1) {}
}

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
	} __except(1) {}
}

static void SafeUpdateRender() {
	__try {
		SDK::Drawing::UpdateMatrix();
		SDK::Orbwalker::OnRender();
		Plugins::PluginManager::Get().OnRender();
		SDK::MenuUI::PermaShow::Render();
		SDK::MenuUI::Notifications::Render();
	} __except(1) {}
}

static bool InitializeSDK();
static bool SafeInitializeSDK() {
	__try {
		return InitializeSDK();
	} __except(1) {
		DebugConsole::Log("[INIT] CRITICAL: SDK init crashed! Will retry...");
		return false;
	}
}

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
	} __except(1) {}
}

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
	} __except(1) {}
}

// ========================================================================
// SDK initialization (unchanged from original)
// ========================================================================
static bool InitializeSDK() {
	char modulePathBuf[MAX_PATH] = {};
	HMODULE hm = NULL;
	GetModuleHandleExA(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)&InitializeSDK, &hm);
	GetModuleFileNameA(hm, modulePathBuf, MAX_PATH);
	std::string dllDir(modulePathBuf);
	dllDir = dllDir.substr(0, dllDir.find_last_of("\\/"));

	SDK::SpellDatabase::Init();
	SDK::DamageLibrary::Init();
	SDK::DamagePassives::Init();
	SDK::DamageMastery::Init();
	SDK::SpellCaster::SetDamageCallback([](const SDK::GameObject& source, const SDK::GameObject& target, SDK::SpellSlotId slot, int stage) {
		return (float)SDK::DamageLibrary::GetSpellDamage(source, target, slot, static_cast<SDK::DamageStage>(stage));
	});
	SDK::DamageCalc::SetPassiveDmgCallback([](const SDK::GameObject& s, const SDK::GameObject& t) {
		return SDK::DamagePassives::GetPassiveDamage(s, t);
	});
	SDK::DamageCalc::SetRuneDmgCallback([](const SDK::GameObject& s, const SDK::GameObject& t) {
		return SDK::DamageMastery::GetRuneDamage(s, t);
	});
	SDK::DamageCalc::SetRuneMultCallback([](const SDK::GameObject& s, const SDK::GameObject& t) {
		return SDK::DamageMastery::GetDamageMultiplier(s, t);
	});
	SDK::Invulnerable::Init();
	SDK::LastCast::Init();
	SDK::WeightedTargetSelector::Init();
	SDK::ChampionPriority::Init();
	SDK::Teleport::Init();
	SDK::GamePath::PathTracker::Init();
	SDK::SkillshotTracker::Init();
	SDK::Orbwalker::Init();
	SDK::AutoLevel::Init();
	SDK::Map::Init();
	SDK::GameObjects::Update();

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
	pm.SetAutoLoad("Orbwalker 2.0", true);
	pm.SetAutoLoad("Target Selector", true);
	pm.SetAutoLoad("Evade", false);
	pm.SetAutoLoad("EzEvade", true);
	pm.LoadAuto();

	DebugConsole::Log("=== Plugins Loaded ===");
	for (auto& p : pm.GetPlugins()) {
		DebugConsole::Log("  - %s: loaded=%d enabled=%d",
			p->GetName(), p->IsLoaded() ? 1 : 0, p->IsEnabled() ? 1 : 0);
	}

	SDK::ConfigManager::SetConfigPath(dllDir);
	SDK::ConfigManager::LoadAll();
	auto nowMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - g_scriptClockStart).count();
	SDK::Game::LastScriptTickWallClock = nowMs - SDK::Game::ScriptTickIntervalMs;
	return true;
}

// ========================================================================
// MAIN THREAD — Overlay render loop (replaces Kiero hook)
// ========================================================================
DWORD WINAPI MainThread(LPVOID lpReserved)
{
	// === Phase 1: AntiBan deferred init (after loader lock) ===
	if (g_hSelfModule) {
		AntiBan::DeferredInit(g_hSelfModule);
	}

	DebugConsole::Init();
	DebugConsole::Log("=== NightSharp Overlay Mode ===");

	// === Phase 2: Wait for game window to appear ===
	Overlay::Window overlayWin;
	int findAttempts = 0;
	while (!overlayWin.FindTarget()) {
		Sleep(500);
		findAttempts++;
		if (findAttempts > 120) { // 60 seconds max wait
			DebugConsole::Log("[OVERLAY] Failed to find game window after 60s");
			return 0;
		}
	}
	DebugConsole::Log("[OVERLAY] Game window found: 0x%p", overlayWin.m_targetHwnd);

	// Wait additional time for game to finish rendering initialization
	Sleep(5000);

	// === Phase 3: Create overlay window ===
	if (!overlayWin.Create()) {
		DebugConsole::Log("[OVERLAY] Failed to create overlay window");
		return 0;
	}
	Overlay::g_overlayWindow = &overlayWin;
	DebugConsole::Log("[OVERLAY] Overlay window created: 0x%p", overlayWin.m_hwnd);

	// === Phase 4: Initialize D3D11 device ===
	Overlay::Renderer renderer;
	if (!renderer.Init(overlayWin.m_hwnd)) {
		DebugConsole::Log("[OVERLAY] Failed to init D3D11");
		overlayWin.Destroy();
		return 0;
	}
	DebugConsole::Log("[OVERLAY] D3D11 device created");

	// === Phase 5: Initialize ImGui ===
	renderer.InitImGui(overlayWin.m_hwnd);
	NightSharpMenu::SetHookBackendLabel("NightSharp Overlay");
	DebugConsole::Log("[OVERLAY] ImGui initialized");

	// Initialize game base address
	Globals::Init();

	// === Phase 6: Render loop ===
	debugLogged = true;

	while (overlayWin.m_running) {
		// Process window messages
		if (!overlayWin.PumpMessages()) break;

		// Handle resize
		if (overlayWin.m_resizeW != 0 && overlayWin.m_resizeH != 0) {
			renderer.Resize(overlayWin.m_resizeW, overlayWin.m_resizeH);
			overlayWin.m_resizeW = overlayWin.m_resizeH = 0;
		}

		// Follow game window position/size
		overlayWin.FollowTarget();

		// Sync anti-capture (OBS bypass) with menu toggle
		overlayWin.SetAntiCapture(Config::StreamProtection::bypassObs);

		// Only render when game window is focused (or overlay is focused)
		if (!overlayWin.IsTargetFocused()) {
			Sleep(50); // Save CPU when not focused
			continue;
		}

		// === RENDER FRAME ===
		// Step 1: Update display size + timing
		renderer.PrepareFrame();

		// Step 2: Manual ImGui input (BEFORE NewFrame processes it)
		// Overlay is ALWAYS click-through, so ImGui_ImplWin32 won't get WndProc
		// messages. We override mouse pos/buttons manually here.
		{
			ImGuiIO& io = ImGui::GetIO();

			// Mouse position — always feed (needed for hover effects)
			POINT cursorPos;
			GetCursorPos(&cursorPos);
			ScreenToClient(overlayWin.m_hwnd, &cursorPos);
			io.MousePos = ImVec2((float)cursorPos.x, (float)cursorPos.y);

			// Mouse buttons — only feed when menu is visible.
			// Menu is visible when showMenu=true OR SHIFT is held
			// (matches NightSharpMenu::Render() visibility logic).
			bool menuVisible = NightSharpMenu::showMenu ||
				((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
			if (menuVisible) {
				io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
				io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
				io.MouseDown[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
			} else {
				io.MouseDown[0] = false;
				io.MouseDown[1] = false;
				io.MouseDown[2] = false;
			}
		}

		// Step 3: Now process all IO state
		renderer.CommitFrame();

		bool inGame = SafeIsInGame();

		// Warmup timer
		if (inGame && !sdkInitialized) {
			float gameTime = SafeGetTime();
			if (sdkWarmupStart <= 0.0f) {
				sdkWarmupStart = gameTime;
				DebugConsole::Log("[INIT] Game detected, starting warmup (%.1fs)...", gameTime);
			}
			if (gameTime < 5.0f || (gameTime - sdkWarmupStart) < SDK_WARMUP_SECONDS) {
				ImDrawList* dl = ImGui::GetBackgroundDrawList();
				if (dl) {
					char warmupBuf[128];
					snprintf(warmupBuf, sizeof(warmupBuf),
						"[NightSharp] Initializing... (%.1fs / %.1fs)",
						gameTime - sdkWarmupStart, SDK_WARMUP_SECONDS);
					dl->AddText(ImVec2(10, 10), IM_COL32(255, 200, 100, 200), warmupBuf);
				}
				renderer.EndFrame();
				continue;
			}
		}

		// One-time SDK initialization
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

		// SDK game logic
		if (inGame && sdkInitialized) {
			const auto nowMs = std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - g_scriptClockStart).count();

			// Script tick (rate-limited to 45 FPS)
			if (SDK::Game::ShouldRunScriptTick(nowMs)) {
				SDK::Game::AdvanceScriptFrame(nowMs);
				SafeUpdateSDKCore();
				SafeUpdatePluginLogic();
			}

			// Render (every frame)
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

		// Menu + overlays
		SafeRenderMenu(inGame);
		if (inGame) SafeRenderOverlay();

		renderer.EndFrame();
	}

	// === Cleanup ===
	DebugConsole::Log("[OVERLAY] Shutting down...");
	SDK::ConfigManager::SaveAll();
	renderer.Shutdown();
	overlayWin.Destroy();
	DebugConsole::Close();

	return 0;
}

// ========================================================================
// DllMain — DLL Proxy Entry Point
// ========================================================================
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
		break;
	}
	return TRUE;
}
