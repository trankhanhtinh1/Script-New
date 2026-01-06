#include "Menu.h"
#include "SDK/ObjectManager.h"
#include "SDK/GameObject.h"
#include "SDK/TargetSelector.h"
#include "SDK/ChampionDatabase.h"
#include "SDK/AiManagerScan.h"
#include "SDK/AiManagerNavGridScan.h"
#include "SDK/HudOffsetScanner.h"
#include "SDK/SpellCaster.h"
#include <fstream>
#include <string>
#include <map>

// External globals
extern int totalObjectsDrawn;
extern bool g_showBuffOverlay; // Fixed: Global scope declaration

// External functions from main.cpp
extern void DumpOffsetDebugToFile();
extern void ContinuousTurretLog();
extern void CombatStatsDebug();
extern void SpellDebug();
extern bool showSpellDebugOverlay;
extern bool continuousSpellDump;
extern void MissileOffsetScan();
extern bool doMissileOffsetScan;
extern bool continuousMissileScan;
extern void AiManagerOffsetScan();
extern bool continuousAiManagerScan;
extern bool continuousAiManagerLog;
extern void DumpAiManagerObfuscated();
extern void BasicAttackOffsetScan();
extern bool continuousBasicAttackScan;
extern void CastSpellOffsetDebug();  // NEW: CastSpell offset debug 
extern void AiManagerScanWithIDADecrypt(); // NEW: IDA-based AiManager decryption scan
extern void RunAiManagerNavGridScan(); // NEW: NavGrid-based AiManager path offset scan
extern void ScanObfuscatedAiManagerOffset(); // NEW: Scan obfuscated offset trong game
extern void ScanHasPathIdleVsMoving(bool isIdle); // NEW: Scan HasPath IDLE vs MOVING
extern void ScanMissileOffsets(); // NEW: Scan Missile offsets cho Evade system
extern bool doMissileDecryptionScan; // NEW: Missile Decryption Scan
extern void ScanMissileOffsetsWithDecryption(); // NEW: Scan với decryption logic
extern void ScanMissileOffsetsDynamic(); // NEW: Dynamic scan for multi-version support

// BuffManager & SpellData Debug
extern bool showBuffManagerDebug;
extern bool showSpellDataDebug;

namespace Menu
{
	// Settings
    bool menuOpen = true;
	bool showAllObjects = false;
	bool showObjectNames = false;
    
    bool drawHeroes = true;
    bool drawMinions = false;
    bool drawTurrets = false;
    bool drawRange = true;
    bool drawDamage = true;

    // Orbwalker Settings (like leagueoflegends-master - all in SECONDS)
    bool orbwalkerEnabled = true;
    bool randomActionDelay = true;
    float clickDelay = 0.05f; // 50ms like leagueoflegends-master default
    float windupBuffer = 0.03f; // 30ms in seconds (leagueoflegends-master default)
    float attackBeforeCanAttack = 0.01f; // 10ms anticipation (leagueoflegends-master default)
    float attackCastDelay = 0.019f;
    
    // Chat Block Settings
    bool blockKeysWhenChatOpen = false; // Block combo keys when chat is open
    uint64_t chatStateTestOffset = 0x193EB74; // Default offset (oChatState)

    // Attackable Units
    bool attackBarrels = true;
    bool attackJunglePlants = false;
    bool attackPets = true;
    bool attackWards = true;
    
    // Prioritize
    bool farmOverHarass = true;
    bool prioritizeSpecialMinions = false;
    bool prioritizeSmallJungle = false;
    bool prioritizeTurrets = true;
    
    // Farm Settings
    int farmDelay = 30;
    int fastFarmDelay = 220;
    bool turretFarmEnabled = true;
    int turretFarmMaxLevel = 13;
    
    // Drawing
    bool drawAttackRange = false;
    bool bDrawAiState = false; // New
    bool drawEnemyRange = false;
    bool drawKillableMinions = false;
    bool drawHoldPosition = false;

    // Debug
    bool showOffsetDebug = false; // Show offset debug overlay
    bool continuousMissileLog = false; // Continuous missile logging
    bool continuousMissileScan = false; // Continuous missile scan toggle
    bool autoCastAndScan = false; // NEW: Auto cast spell then scan when unchecked
    bool continuousTurretLog = false; // Continuous turret logging
    
    // Missile Drawing Debug
    bool drawMissiles = false;           // Draw all missiles
    bool drawMinionMissiles = false;     // Draw minion attack missiles
    bool drawTurretMissiles = false;     // Draw turret attack missiles
    bool drawChampionMissiles = false;   // Draw champion spell missiles
    
    // Evade Drawing
    bool drawSkillshots = true;          // Draw enemy skillshots (EzrealQ, etc.) - ON by default for testing

    // ============================================================================
    // Target Selector Settings (based on NewTargetSelector.cs)
    // ============================================================================
    // Modes: 0 = Smart AD/AP, 1 = Lowest Health, 2 = Most Priority
    int tsMode = 1; // Default: Lowest Health (per user request)
    bool tsForceSelected = true;   // Force on Select Target
    bool tsOnlySelected = false;   // Only Attack Select Target
    bool tsDrawSelected = true;    // Draw Selected Target
    bool tsHighlightSelected = true; // Highlight Selected Target
    float tsDrawColor[3] = { 1.0f, 0.0f, 0.0f }; // Red default (NewTargetSelector.cs: ColorBGRA(255, 0, 0, 255))
    
    // Prediction Settings
    int predHitchance = 2;              // Default: High
    float predRangePercent = 98.0f;     // Default: 98%
    bool predDrawPredictedPos = false;  // Draw predicted position
    bool predDrawCastPos = false;       // Draw cast position
    bool predDrawHitbox = false;        // Draw skillshot hitbox
    bool predDebugCastSpell = false;    // Debug: Press S to cast Q

    // UI State
    int currentTab = 2; // Default to Orbwalker tab

    // Helper for styling
    void ApplyDarkStyle() {
        ImGuiStyle* style = &ImGui::GetStyle();
        
        // Window
        style->WindowPadding = ImVec2(12.0f, 14.0f);
        style->WindowRounding = 8.0f;
        style->FramePadding = ImVec2(10.0f, 6.0f);
        style->FrameRounding = 4.0f;
        style->ItemSpacing = ImVec2(8.0f, 6.0f);
        style->ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style->IndentSpacing = 20.0f;
        style->ScrollbarSize = 14.0f;
        style->ScrollbarRounding = 8.0f;
        style->GrabMinSize = 10.0f;
        style->GrabRounding = 4.0f;
        
        // Colors - Dark theme with accent colors
        ImVec4* colors = style->Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.25f, 0.60f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.40f, 0.45f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.30f, 0.90f, 0.30f, 1.00f); // Green checkmark
        colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f); // Blue slider
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.80f, 1.00f, 1.00f);
        colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.52f, 1.00f);
    }

    void DumpObjects() {
        std::ofstream file("dump_objects.txt");
        if (!file.is_open()) return;
        
        file << "=== HEROES ===\n";
        auto heroes = SDK::ObjectManager::GetHeroes();
        for (auto obj : heroes) {
             file << "Addr: " << std::hex << obj->Address << std::dec 
                  << " | Team: " << obj->GetTeam()
                  << " | HP: " << obj->GetHealth()
                  << " | Name: " << obj->GetName() << "\n";
        }
        
        file << "\n=== ALL MINIONS (Jungle/Lane) ===\n";
        auto minions = SDK::ObjectManager::GetAllMinions();
        for (auto obj : minions) {
             file << "Addr: " << std::hex << obj->Address << std::dec 
                  << " | Team: " << obj->GetTeam()
                  << " | HP: " << obj->GetHealth() 
                  << " | Name: " << obj->GetName() << "\n";
        }

        file << "\n=== TURRETS ===\n";
        auto turrets = SDK::ObjectManager::GetTurrets();
        for (auto obj : turrets) {
             file << "Addr: " << std::hex << obj->Address << std::dec 
                  << " | Team: " << obj->GetTeam()
                  << " | HP: " << obj->GetHealth() 
                  << " | Name: " << obj->GetName() << "\n";
        }

        file.close();
    }

	void Render()
	{
        // Update HUD scanners (collects data when monitoring enabled)
        HudOffsetScanner::Update();
        
        if (!menuOpen) return;

        static bool styleSet = false;
        if (!styleSet) {
            ApplyDarkStyle();
            styleSet = true;
        }

        // Set window size
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

        ImGui::Begin("League Internal", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

        // Sidebar Layout using Columns
        ImGui::Columns(2, "MainLayout", true);
        
        // Setup Column Widths (only once)
        static bool widthSet = false;
        if (!widthSet) {
             ImGui::SetColumnWidth(0, 150.0f);
             widthSet = true;
        }

        // --- LEFT SIDEBAR ---
        if (ImGui::Button("Cooldowns", ImVec2(-1, 30))) currentTab = 0;
        if (ImGui::Button("Recalls", ImVec2(-1, 30))) currentTab = 1;
        if (ImGui::Button("Orbwalker", ImVec2(-1, 30))) currentTab = 2;
        if (ImGui::Button("Target Selector", ImVec2(-1, 30))) currentTab = 3;
        if (ImGui::Button("Skinchanger", ImVec2(-1, 30))) currentTab = 4;
        if (ImGui::Button("Debug", ImVec2(-1, 30))) currentTab = 5;
        if (ImGui::Button("Syndra", ImVec2(-1, 30))) currentTab = 6;
        if (ImGui::Button("Prediction", ImVec2(-1, 30))) currentTab = 7;

        ImGui::NextColumn();

        // --- RIGHT CONTENT AREA ---
        ImGui::BeginChild("Content", ImVec2(0, 0), true);

        if (currentTab == 0) { // Cooldowns
             ImGui::Text("Cooldowns Settings (Not Implemented)");
        }
        else if (currentTab == 1) { // Recalls
             ImGui::Text("Recalls Settings (Not Implemented)");
        }
        else if (currentTab == 2) { // Orbwalker
             ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Orbwalker Configuration");
             ImGui::Separator();
             
             // Main Enable Toggle
             ImGui::Checkbox("Enabled", &orbwalkerEnabled);
             if (orbwalkerEnabled) {
                 ImGui::SameLine();
                 ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "ON");
             }
             
             ImGui::Spacing();
             ImGui::Separator();
             
             // Timing Settings
             if (ImGui::CollapsingHeader("Timing Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                 ImGui::Text("Click Delay");
                 ImGui::SliderFloat("##ClickDelay", &clickDelay, 0.0f, 0.2f, "%.3f");
                 
                 ImGui::Checkbox("Random Action Delay", &randomActionDelay);
                 
                 // leagueoflegends-master: windupbuffer range 0.01f to 0.2f (10-200ms)
                 ImGui::Text("Windup Buffer (seconds)");
                 ImGui::SliderFloat("##WindupBuffer", &windupBuffer, 0.01f, 0.2f, "%.3f");
                 
                 // leagueoflegends-master: attack before can attack range 0.0f to 0.2f
                 ImGui::Text("Attack Before Can Attack (seconds)");
                 ImGui::SliderFloat("##AttackBeforeCanAttack", &attackBeforeCanAttack, 0.0f, 0.2f, "%.3f");
             }
             
             ImGui::Spacing();
             
             // Attackable Units
             if (ImGui::CollapsingHeader("Attackable Units")) {
                 ImGui::Checkbox("Attack Barrels", &attackBarrels);
                 ImGui::Checkbox("Attack Jungle Plants", &attackJunglePlants);
                 ImGui::Checkbox("Attack Pets/Special Minions", &attackPets);
                 ImGui::Checkbox("Attack Wards", &attackWards);
             }
             
             ImGui::Spacing();
             
             // Prioritize
             if (ImGui::CollapsingHeader("Prioritize")) {
                 ImGui::Checkbox("Farm Over Harass", &farmOverHarass);
                 ImGui::Checkbox("Prioritize Special Minions", &prioritizeSpecialMinions);
                 ImGui::Checkbox("Prioritize Small Jungle", &prioritizeSmallJungle);
                 ImGui::Checkbox("Prioritize Turrets", &prioritizeTurrets);
             }
             
             ImGui::Spacing();
             
             // Farm Settings
             if (ImGui::CollapsingHeader("Farm Settings")) {
                 ImGui::Text("Farm Delay (ms)");
                 ImGui::SliderInt("##FarmDelay", &farmDelay, 0, 200);
                 
                 ImGui::Text("Fast Farm Delay (ms)");
                 ImGui::SliderInt("##FastFarmDelay", &fastFarmDelay, 0, 1000);
                 
                 ImGui::Checkbox("Turret Farm Enabled", &turretFarmEnabled);
                 
                 ImGui::Text("Turret Farm Max Level");
                 ImGui::SliderInt("##TurretFarmMaxLevel", &turretFarmMaxLevel, 1, 18);
             }
             
             ImGui::Spacing();
             
             // Drawing Options
             if (ImGui::CollapsingHeader("Drawing")) {
                 ImGui::Checkbox("Draw Attack Range", &drawAttackRange);
                 ImGui::Checkbox("Draw Enemy Range", &drawEnemyRange);
                 ImGui::Checkbox("Draw Killable Minions", &drawKillableMinions);
                 // ImGui::Checkbox("Draw Hold Position", &drawHoldPosition); // Removed per user request
             }
             
            ImGui::Spacing();
            ImGui::Separator();
            
            // Chat Block Settings
            if (ImGui::CollapsingHeader("Chat Block Settings")) {
                ImGui::Checkbox("Block Keys When Chat Open", &blockKeysWhenChatOpen);
                if (blockKeysWhenChatOpen) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Enabled: Space, V, C, X, Z will be blocked when chat is open");
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Disabled: Keys will work even when chat is open");
                }
                ImGui::Spacing();
                ImGui::Text("Chat State Offset (for testing):");
                ImGui::InputScalar("##ChatStateOffset", ImGuiDataType_U64, &chatStateTestOffset, NULL, NULL, "%llX", ImGuiInputTextFlags_CharsHexadecimal);
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Default: 0x193EB74 (oChatState)");
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Test different offsets to find the correct one");
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            
            // Keybinds Info
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Keybinds:");
             ImGui::Columns(2, "KeybindColumns", false);
             ImGui::SetColumnWidth(0, 120);
             
             ImGui::BulletText("Space"); ImGui::NextColumn();
             ImGui::Text("Combo"); ImGui::NextColumn();
             
             ImGui::BulletText("V"); ImGui::NextColumn();
             ImGui::Text("Lane Clear"); ImGui::NextColumn();
             
             ImGui::BulletText("X"); ImGui::NextColumn();
             ImGui::Text("Last Hit"); ImGui::NextColumn();
             
             ImGui::BulletText("C"); ImGui::NextColumn();
             ImGui::Text("Harass"); ImGui::NextColumn();
             
             ImGui::BulletText("Z"); ImGui::NextColumn();
             ImGui::Text("Flee"); ImGui::NextColumn();
             
             ImGui::Columns(1);
             
             ImGui::Spacing();
             ImGui::Unindent();
        }
        else if (currentTab == 3) { // Target Selector
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Target Selector Configuration");
            ImGui::Separator();
            
            // TARGET SELECTION MODE (based on NewTargetSelector.cs Mode)
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Target Selection Mode");
            ImGui::Separator();
            
            const char* tsModeItems[] = { 
                "Lowest Health",    // 0 - GetRealHealth
                "Most Priority",    // 1 - GetPriority (ADC > Assassin > Tank)
                "Near Mouse",       // 2 - Closest to mouse cursor
                "Least Attacks",    // 3 - Fewest auto attacks to kill
                "Most AD",          // 4 - Highest AD damage
                "Most AP"           // 5 - Highest AP damage
            };
            
            ImGui::SetNextItemWidth(200);
            ImGui::Combo("##TSMode", &tsMode, tsModeItems, IM_ARRAYSIZE(tsModeItems));
            
            // Mode description
            const char* modeDesc[] = {
                "Target with lowest effective health",
                "Target with highest priority (ADC > Assassin > Tank)",
                "Target closest to mouse cursor",
                "Target requiring fewest auto attacks to kill",
                "Best target for AD damage dealers",
                "Best target for AP damage dealers"
            };
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", modeDesc[tsMode]);
            
            ImGui::Spacing();
            ImGui::Separator();
            
            // PRIORITY SECTION (based on NewTargetSelector.cs PriorityMenu)
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.8f, 1.0f), "Priority Settings");
            ImGui::Separator();
            
            // Get local player and enemy heroes
            SDK::GameObject* local = SDK::ObjectManager::GetLocalPlayer();
            std::vector<SDK::GameObject*> heroes = SDK::ObjectManager::GetHeroes();
            std::vector<SDK::GameObject*> enemies;
            
            if (local) {
                for (auto hero : heroes) {
                    if (hero && hero->IsValid() && hero->IsEnemyTo(local)) {
                        enemies.push_back(hero);
                    }
                }
            }
            
            // Show current game enemies first (like NewTargetSelector.cs)
            bool hasEnemies = !enemies.empty();
            if (hasEnemies) {
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.3f, 1.0f), "Current Game Enemies:");
                
                for (auto enemy : enemies) {
                    if (!enemy || !enemy->IsValid()) continue;
                    
                    std::string name = enemy->GetName();
                    
                    // Get priority from ChampionDatabase (equivalent to PriorityMenu["TS_" + name])
                    int priority = SDK::ChampionDatabase::GetPriority(name);
                    int defaultPrio = SDK::ChampionDatabase::GetDefaultPriority(name);
                    
                    ImGui::PushID(name.c_str());
                    
                    // Color based on priority (matching NewTargetSelector.cs logic)
                    ImVec4 color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                    if (priority == 5) color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Max - Red
                    else if (priority == 4) color = ImVec4(1.0f, 0.6f, 0.2f, 1.0f); // High - Orange  
                    else if (priority == 3) color = ImVec4(1.0f, 1.0f, 0.3f, 1.0f); // Medium - Yellow
                    else if (priority == 1) color = ImVec4(0.5f, 0.8f, 1.0f, 1.0f); // Low - Blue
                    
                    ImGui::TextColored(color, "%s", name.c_str());
                    ImGui::SameLine(150);
                    
                    ImGui::SetNextItemWidth(100);
                    if (ImGui::SliderInt("##prio", &priority, 1, 5)) {
                        SDK::ChampionDatabase::SetPriority(name, priority);
                    }
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(def:%d)", defaultPrio);
                    
                    ImGui::PopID();
                }
            }
            
            if (!hasEnemies) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No enemy champions detected");
            }
            
            // Cleanup
            for (auto h : heroes) delete h;
            if (local) delete local;
            
            ImGui::Spacing();
            ImGui::Separator();
            
            // SELECTED TARGET OPTIONS (based on NewTargetSelector.cs)
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Selected Target Options");
            ImGui::Separator();
            
            // Force on Select Target (NewTargetSelector.cs: ForceSelectTarget)
            ImGui::Checkbox("Force on Select Target", &tsForceSelected);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("When enabled, will prioritize attacking the manually selected target\nif it's within attack range");
            }
            
            // Only Attack Select Target (NewTargetSelector.cs: OnlySelectTarget)
            ImGui::Checkbox("Only Attack Select Target", &tsOnlySelected);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("When enabled, will ONLY attack the manually selected target\nIgnores all other enemies even if selected target is out of range");
            }
            
            // Sync with TargetSelector static variables
            SDK::TargetSelector::ForceSelectedTarget = tsForceSelected;
            SDK::TargetSelector::OnlySelectedTarget = tsOnlySelected;
            
            ImGui::Spacing();
            ImGui::Separator();
            
            // DRAWINGS SECTION (based on NewTargetSelector.cs DrawingsMenu)
            ImGui::TextColored(ImVec4(0.8f, 0.5f, 1.0f, 1.0f), "Drawings");
            ImGui::Separator();
            
            // Select Color (NewTargetSelector.cs: SelectColor)
            ImGui::ColorEdit3("Select Color", tsDrawColor);
            
            // Draw Selected Target (NewTargetSelector.cs: DrawSelect)
            ImGui::Checkbox("Draw Selected Target", &tsDrawSelected);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Draw a circle around the selected target");
            }
            
            // Highlight Selected Target (NewTargetSelector.cs: LightSelect)
            ImGui::Checkbox("Highlight Selected Target", &tsHighlightSelected);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Apply glow effect to the selected target");
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            
            // CURRENT TARGET INFO
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Current Target Info");
            ImGui::Separator();
            
            if (SDK::TargetSelector::SelectedTarget != 0) {
                SDK::GameObject selectedObj(SDK::TargetSelector::SelectedTarget);
                if (selectedObj.IsValid() && !selectedObj.IsDead()) {
                    // Get fresh local player for distance calculation
                    SDK::GameObject* localForDist = SDK::ObjectManager::GetLocalPlayer();
                    float distance = 0.0f;
                    if (localForDist) {
                        distance = localForDist->GetPosition().Distance(selectedObj.GetPosition());
                        delete localForDist;
                    }
                    
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "Selected: %s", selectedObj.GetName().c_str());
                    ImGui::Text("Health: %.0f / %.0f", selectedObj.GetHealth(), selectedObj.GetMaxHealth());
                    ImGui::Text("Distance: %.0f", distance);
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Selected target is dead or invalid");
                    SDK::TargetSelector::SelectedTarget = 0; // Clear invalid target
                }
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No target selected");
                ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "Left-click on enemy champion to select");
            }
            
            // Clear Selection Button
            if (SDK::TargetSelector::SelectedTarget != 0) {
                if (ImGui::Button("Clear Selection")) {
                    SDK::TargetSelector::SelectedTarget = 0;
                }
            }
        }
        else if (currentTab == 4) { // SkinChanger
             ImGui::Text("Skin Changer (Not Implemented)");
        }
        else if (currentTab == 5) { // Debug
             ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Developer Options");
             ImGui::Separator();
             
             ImGui::Checkbox("Draw Heroes", &drawHeroes);
             ImGui::Checkbox("Draw Minions", &drawMinions);
             ImGui::Checkbox("Draw Turrets", &drawTurrets);
             ImGui::Checkbox("Draw Range", &drawRange);
             ImGui::Checkbox("Show All Names", &showObjectNames);
             
             ImGui::Spacing();
             ImGui::TextColored(ImVec4(1, 1, 0, 1), "=== MISSILE DRAWING ===");
             ImGui::Checkbox("Draw All Missiles", &drawMissiles);
             ImGui::Checkbox("Draw Minion Missiles", &drawMinionMissiles);
             ImGui::Checkbox("Draw Turret Missiles", &drawTurretMissiles);
             ImGui::Checkbox("Draw Champion Missiles", &drawChampionMissiles);
             
             
             // [REMOVED] EVADE DRAWING - Feature disabled
             
             ImGui::Spacing();
             ImGui::TextColored(ImVec4(1, 1, 0, 1), "=== DEBUG ===");
             ImGui::Checkbox("Show Offset Debug Overlay", &showOffsetDebug);
             
             if (ImGui::Button("Dump Objects (dump_objects.txt)")) {
                  DumpObjects();
             }
             
             if (ImGui::Button("Dump Offset Debug (offset_debug.txt)")) {
                  DumpOffsetDebugToFile();
             }
             ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Safe dump with crash protection");
             
             ImGui::Spacing();
             ImGui::Checkbox("Continuous Missile Log", &continuousMissileLog);
             if (continuousMissileLog) {
                 ImGui::TextColored(ImVec4(0, 1, 0, 1), "LOGGING... Check missile_log.txt");
             } else {
                 ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Toggle ON to log missiles continuously");
             }
             
             ImGui::Checkbox("Continuous Missile Scan", &continuousMissileScan);
             if (continuousMissileScan) {
                 ImGui::TextColored(ImVec4(0, 1, 0, 1), "SCANNING... Check missile_offset_scan.txt");
             } else {
                 ImGui::TextColored(ImVec4(1, 1, 0, 1), "Cast spell -> Toggle ON -> Check file");
             }
             if (ImGui::Button("Scan Missile Offsets (Once)")) {
                MissileOffsetScan();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> missile_offset_scan.txt");
            
            // NEW: Advanced Missile Scan
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "=== ADVANCED MISSILE SCAN ===");
            ImGui::TextWrapped("Tìm và verify các offsets: SpellInfo, StartPos, EndPos, Position, SrcIdx");
            if (ImGui::Button("Scan Missile Offsets (Advanced)")) {
                ScanMissileOffsets();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> missile_scan.txt");
            
            // NEW: Missile Decryption Scan
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "=== MISSILE DECRYPTION SCAN ===");
            ImGui::TextWrapped("Scan với chiến lược giải mã: Tìm tất cả offsets (NetId, Position, SrcIdx, StartPos, EndPos, Speed, Radius, etc.)");
            ImGui::Checkbox("Decryption Scan (Auto)", &doMissileDecryptionScan);
            if (doMissileDecryptionScan) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "SCANNING... Check missile_decrypt_scan.txt");
            }
            ImGui::SameLine();
            if (ImGui::Button("Scan Missile (Full Decrypt)")) {
                ScanMissileOffsetsWithDecryption();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> missile_decrypt_scan.txt");
            ImGui::Separator();
            ImGui::Checkbox("Auto Cast & Scan (Missile)", &autoCastAndScan);
            if (autoCastAndScan) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "CASTING... (Bỏ check để bắt đầu Scan)");
            } else {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "IDLE (Check để bắt đầu Cast)");
            }
            
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "⚠️  INSTRUCTIONS:");
            ImGui::BulletText("Bước 1: Cast skillshot (Jinx W, Ezreal Q, etc.)");
            ImGui::BulletText("Bước 2: Click 'Scan Missile Offsets (Advanced)' ngay khi missile vừa created");
            ImGui::BulletText("Bước 3: So sánh SpellInfo pointer với SpellInfo từ spell cast");
            ImGui::BulletText("Bước 4: So sánh Position với StartPos/EndPos từ SpellInfo");
            ImGui::BulletText("Bước 5: Verify SrcIdx match với caster NetID");
            
            // NEW: Dynamic Missile Offset Scanner
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "=== DYNAMIC OFFSET SCANNER (MULTI-VERSION) ===");
            ImGui::TextWrapped("Tự động tìm StartPos, EndPos, Speed, Radius, Width, StartTime bằng cách so sánh với SpellInfo");
            if (ImGui::Button("Scan Dynamic Offsets (RECOMMENDED)")) {
                ScanMissileOffsetsDynamic();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> missile_dynamic_scan.txt");
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Best method - works across versions!");
            ImGui::TextWrapped("Output: Copy-paste ready offsets for Offsets.h");
            
            // NEW: HUD & SpellInput Offset Scanner (Checkbox mode)
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "=== HUD & SPELL INPUT SCANNER ===");
            ImGui::TextWrapped("Check ON = Start monitoring, Check OFF = Export results to file");
            
            // SpellInput Scanner
            static bool monitorSpellInput = false;
            bool prevSpellInputState = monitorSpellInput;
            ImGui::Checkbox("Monitor SpellInput (TargetNetId, StartPos, EndPos)", &monitorSpellInput);
            if (monitorSpellInput != prevSpellInputState) {
                HudOffsetScanner::ToggleSpellInputMonitor(monitorSpellInput);
            }
            if (monitorSpellInput) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "MONITORING... Cast spells to collect data");
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Samples: %d", HudOffsetScanner::g_SpellInputData.sampleCount);
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "-> spellinput_scan_results.txt");
            }
            
            // Camera Scanner
            static bool monitorCamera = false;
            bool prevCameraState = monitorCamera;
            ImGui::Checkbox("Monitor Camera Zoom (scroll wheel to change)", &monitorCamera);
            if (monitorCamera != prevCameraState) {
                HudOffsetScanner::ToggleCameraMonitor(monitorCamera);
            }
            if (monitorCamera) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "MONITORING... Scroll mouse wheel to change zoom");
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Samples: %d, Range: %.0f - %.0f", 
                    HudOffsetScanner::g_CameraData.sampleCount,
                    HudOffsetScanner::g_CameraData.minZoomSeen,
                    HudOffsetScanner::g_CameraData.maxZoomSeen);
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "-> camera_zoom_scan_results.txt");
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "=== BASIC ATTACK DEBUG ===");
            ImGui::TextWrapped("Scan BasicAttack offsets (attack to test):");
            if (ImGui::Button("Scan BasicAttack Offsets")) {
                BasicAttackOffsetScan();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> basicattack_offset_scan.txt");
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "=== AI MANAGER DEBUG ===");
            ImGui::TextWrapped("Verify AiManager offsets (move character to test):");
            ImGui::Checkbox("Draw Movement State (Idle/Moving)", &bDrawAiState); // New Checkbox
            ImGui::Checkbox("Continuous AiManager Scan", &continuousAiManagerScan);
            ImGui::Checkbox("Continuous AiManager LOG", &continuousAiManagerLog);
            if (ImGui::Button("Scan AiManager Offsets (Once)")) {
                AiManagerOffsetScan();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> aimanager_offset_scan.txt");
            
            // NEW: Obfuscation method dumper
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "=== OBFUSCATION METHOD ===");
            if (ImGui::Button("Dump AiManager (Obf vs Direct)")) {
                DumpAiManagerObfuscated();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> aimanager_dumper.txt");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Compares Direct (0x3108) vs Obfuscated (0x36F0) methods");
            
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "LOG creates: aimanager_continuous_log.txt");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Records all states: stand -> move -> dash");
            
            // NEW: IDA-based decryption scan (uses sub_289E40 algorithm)
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "=== IDA DECRYPTION SCAN (15.24) ===");
            ImGui::TextWrapped("Uses updated DecryptAiManager (sub_289E40) for proper obfuscation handling");
            if (ImGui::Button("Scan with IDA Decrypt (RECOMMENDED)")) {
                AiManagerScanWithIDADecrypt();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> aimanager_scan.txt");
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Best method for 15.24+ - full offset dump!");
            
            // ============ NEW: GUIDED SCAN ============
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 1.0f, 1.0f), "=== GUIDED SCAN (3-Phase) ===");
            ImGui::TextWrapped("Automated IDLE -> MOVING -> DASH comparison. Best for finding IsMoving, IsDashing offsets!");
            
            // Get status color
            float sr, sg, sb;
            AiManagerScan::GetScanStatusColor(&sr, &sg, &sb);
            ImGui::TextColored(ImVec4(sr, sg, sb, 1.0f), "%s", AiManagerScan::GetScanStatusMessage());
            
            // Start button (only if not active)
            if (!AiManagerScan::g_scanState.isActive) {
                if (ImGui::Button("Start Guided Scan")) {
                    AiManagerScan::StartGuidedScan();
                }
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> aimanager_guided_scan.txt");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), ">>> SCAN IN PROGRESS <<<");
            }
            
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Steps: 2s IDLE -> 5s MOVE zigzag -> DASH");
            
            // ============ NEW: NAVGRID-BASED SCAN ============
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 1.0f, 1.0f), "=== NAVGRID PATH SCAN ===");
            ImGui::TextWrapped("Scan for NavArray, SegmentsCount, CurrentSegment, MoveVec3, ServerPos");
            if (ImGui::Button("Scan NavGrid Path Offsets")) {
                RunAiManagerNavGridScan();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> aimanager_navgrid_scan.txt");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Uses NavGrid (0x1D32A80) as reference");
            
            // ============ NEW: SCAN OBFUSCATED OFFSET ============
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "=== SCAN OBFUSCATED OFFSET ===");
            ImGui::TextWrapped("Tìm offset obfuscated structure trực tiếp trong game (không cần biết offset cũ)");
            if (ImGui::Button("Scan Obfuscated Offset (RECOMMENDED)")) {
                ScanObfuscatedAiManagerOffset();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> aimanager_offset_scan.txt");
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Best method - scan và verify trực tiếp trong game!");
            
            // ============ NEW: SCAN HasPath IDLE vs MOVING ============
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "=== SCAN HasPath (IDLE vs MOVING) ===");
            ImGui::TextWrapped("Mở rộng scan để tìm offset chính xác của HasPath. Scan 2 lần: IDLE và MOVING");
            if (ImGui::Button("Scan HasPath - IDLE State")) {
                ScanHasPathIdleVsMoving(true);
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> haspath_scan_idle.txt");
            if (ImGui::Button("Scan HasPath - MOVING State")) {
                ScanHasPathIdleVsMoving(false);
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> haspath_scan_moving.txt");
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "⚠️  INSTRUCTIONS:");
            ImGui::BulletText("Bước 1: Đứng yên (IDLE) → Click 'Scan HasPath - IDLE State'");
            ImGui::BulletText("Bước 2: Di chuyển (MOVING) → Click 'Scan HasPath - MOVING State'");
            ImGui::BulletText("Bước 3: So sánh 2 files để tìm giá trị thay đổi");
            ImGui::BulletText("HasPath offset = giá trị thay đổi: 0 (IDLE) → 1 hoặc non-zero (MOVING)");
            
            // Compare scan buttons
            ImGui::Spacing();
            if (ImGui::Button("Capture IDLE State")) {
                auto local = SDK::ObjectManager::GetLocalPlayer();
                if (local) {
                    AiManagerNavGridScan::CaptureIdleState(local->Address);
                    delete local;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Capture MOVE State")) {
                auto local = SDK::ObjectManager::GetLocalPlayer();
                if (local) {
                    AiManagerNavGridScan::CaptureMoveState(local->Address);
                    delete local;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Write Compare")) {
                AiManagerNavGridScan::WriteCompareResults();
            }
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "-> aimanager_compare_scan.txt");
            
            // ============ NEW: NAVGRID GUIDED SCAN ============
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "=== NAVGRID GUIDED SCAN ===");
            ImGui::TextWrapped("Automated 3-phase scan: IDLE -> MOVE -> DASH. Auto-writes results to todo file!");
            
            // Status display
            float ngr, ngg, ngb;
            AiManagerNavGridScan::GetNavGridScanStatusColor(&ngr, &ngg, &ngb);
            ImGui::TextColored(ImVec4(ngr, ngg, ngb, 1.0f), "%s", AiManagerNavGridScan::GetNavGridScanStatusMessage());
            
            // Start button
            if (!AiManagerNavGridScan::g_navGridScanState.isActive) {
                if (ImGui::Button("Start NavGrid Guided Scan")) {
                    AiManagerNavGridScan::StartNavGridGuidedScan();
                }
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> todo file + aimanager_navgrid_guided_scan.txt");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), ">>> SCAN IN PROGRESS <<<");
            }
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Steps: 2s IDLE -> 5s MOVE -> 5s DASH");
            
            ImGui::Spacing();
            ImGui::Separator();
            // Turret aggro detection
             if (continuousTurretLog) {
                 ImGui::TextColored(ImVec4(0, 1, 0, 1), "LOGGING... Check turret_log.txt");
             } else {
                 ImGui::TextColored(ImVec4(1, 1, 0, 1), "Stand under turret, toggle ON, wait until death");
             }
             ImGui::Spacing();
             ImGui::Separator();
             ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "=== COMBAT STATS DEBUG ===");
             ImGui::TextWrapped("Buy items then click to find offsets:");
             ImGui::BulletText("CritChance: Buy Infinity Edge");
             ImGui::BulletText("BonusArmor: Buy Plated Steelcaps");
             ImGui::BulletText("BonusMR: Buy Mercury's Treads");
             if (ImGui::Button("Dump Combat Stats")) {
                 CombatStatsDebug();
             }
             ImGui::SameLine();
             ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> combat_stats_debug.txt");
             
             ImGui::Spacing();
             ImGui::Separator();
             ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "=== SPELL DEBUG ===");
             ImGui::TextWrapped("Verify spell offsets: Level, Cooldown, SpellInfo, etc.");
             ImGui::BulletText("Level Q -> Check [0x028]");
             ImGui::BulletText("Use spell -> Check Cooldown [0x030]");
             ImGui::BulletText("SpellInfo pointer -> [0x128]");
             
             ImGui::Checkbox("Show Spell Debug Overlay", &showSpellDebugOverlay);
             ImGui::Checkbox("Continuous Spell Dump", &continuousSpellDump);
             if (continuousSpellDump) {
                 ImGui::TextColored(ImVec4(0, 1, 0, 1), "LOGGING... Check spell_debug.txt");
             }
             
             if (ImGui::Button("Dump Spell Info (Once)")) {
                 SpellDebug();
             }
             ImGui::SameLine();
             ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> spell_debug.txt");
             
             // NEW: CastSpell Offset Debug
             ImGui::Spacing();
             ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "=== CASTSPELL DEBUG ===");
             ImGui::TextWrapped("Debug CastSpell offsets (Pattern Scan + Pointer Probe)");
             if (ImGui::Button("Scan CastSpell Offsets")) {
                 CastSpellOffsetDebug();
             }
             ImGui::SameLine();
             ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "-> castspell_debug.txt");
             ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Scans: oCastSpellWrapper, SpellInfo/SpellInput ptrs");
             
             ImGui::Spacing();
             ImGui::Separator();
             ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "=== BUFFMANAGER & SPELLDATA DEBUG ===");
             ImGui::TextWrapped("Verify BuffManager (CC, Invuln) and SpellData classes work correctly.");
             
             ImGui::Checkbox("Show BuffManager Debug", &showBuffManagerDebug);
             if (ImGui::IsItemHovered()) {
                 ImGui::SetTooltip("Shows active buffs, CC status, and invulnerability for you and enemies");
             }
             
             // NEW: Safe Buff Overlay
             ImGui::Checkbox("Show Buff Overlay (Safe)", &::g_showBuffOverlay);
             if (ImGui::IsItemHovered()) {
                 ImGui::SetTooltip("Draws all active buffs using verified offsets (Crash Safe)");
             }
             
             ImGui::Checkbox("Show SpellData Debug", &showSpellDataDebug);
             if (ImGui::IsItemHovered()) {
                 ImGui::SetTooltip("Shows spell levels, cooldowns, and SpellFactory examples");
             }
             
             ImGui::Separator();
             auto local = SDK::ObjectManager::GetLocalPlayer();
             if (local) {
                 ImGui::Text("Local: %s", local->GetName().c_str());
                 ImGui::Text("HP: %.0f/%.0f", local->GetHealth(), local->GetMaxHealth());
                 ImGui::Text("AD: %.1f", local->GetAttackDamage());
                 delete local;
             }
        }
        else if (currentTab == 6) { // Syndra
             ImGui::Text("Syndra Script (Not Implemented)");
        }
        else if (currentTab == 7) { // Prediction
             ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.8f, 1.0f), "Prediction Settings");
             ImGui::Separator();
             
             // ============================================================================
             // HITCHANCE DROPDOWN
             // ============================================================================
             ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Hitchance Threshold");
             const char* hitchanceItems[] = { 
                 "Low",       // 0
                 "Medium",    // 1
                 "High",      // 2
                 "Very High", // 3
                 "Immobile"   // 4
             };
             ImGui::SetNextItemWidth(200);
             ImGui::Combo("##Hitchance", &predHitchance, hitchanceItems, IM_ARRAYSIZE(hitchanceItems));
             
             // Hitchance description
             const char* hitchanceDesc[] = {
                 "Low: Cast even when target is changing direction",
                 "Medium: Wait for stable movement",
                 "High: Only cast when confident",
                 "Very High: Near guaranteed hit (default)",
                 "Immobile: Only cast on stunned/rooted targets"
             };
             ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", hitchanceDesc[predHitchance]);
             
             ImGui::Spacing();
             ImGui::Separator();
             
             // ============================================================================
             // RANGE SLIDER
             // ============================================================================
             ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Skill Range");
             ImGui::Text("Use %% of max range:");
             ImGui::SetNextItemWidth(200);
             ImGui::SliderFloat("##RangePercent", &predRangePercent, 50.0f, 100.0f, "%.0f%%");
             ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Lower = safer, Higher = max range");
             
             ImGui::Spacing();
             ImGui::Separator();
             
             // ============================================================================
             // DRAWING OPTIONS
             // ============================================================================
             ImGui::TextColored(ImVec4(0.8f, 0.5f, 1.0f, 1.0f), "Drawings");
             ImGui::Separator();
             
             ImGui::Checkbox("Draw Predicted Position", &predDrawPredictedPos);
             if (ImGui::IsItemHovered()) {
                 ImGui::SetTooltip("Shows where enemy will be when spell arrives");
             }
             
             ImGui::Checkbox("Draw Cast Position", &predDrawCastPos);
             if (ImGui::IsItemHovered()) {
                 ImGui::SetTooltip("Shows where you should aim");
             }
             
             ImGui::Checkbox("Draw Skillshot Hitbox", &predDrawHitbox);
             if (ImGui::IsItemHovered()) {
                 ImGui::SetTooltip("Shows skillshot collision zone (uses Polygon)");
             }
             
             ImGui::Spacing();
             ImGui::Separator();
             
             // ============================================================================
             // DEBUG CAST SPELL
             // ============================================================================
             ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Debug Cast Spell");
             ImGui::Separator();
             
             ImGui::Checkbox("Enable Debug Cast (Press S to Cast Q)", &predDebugCastSpell);
             
             static DWORD lastCastTime = 0;
             static bool castTriggered = false;
             static bool lastCastResult = false;
             
             if (predDebugCastSpell) {
                 ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ENABLED: Press 'S' key to cast Q spell");
                 ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Cast at mouse position - Check castspell_debug.txt for details");
                 
                 // Check S key press
                 if (GetAsyncKeyState('S') & 0x8000) {
                     DWORD currentTime = GetTickCount();
                     
                     // Cooldown 500ms to prevent spam
                     if (currentTime - lastCastTime > 500) {
                         lastCastTime = currentTime;
                         castTriggered = true;
                         
                         // Use SpellCaster to cast Q at mouse position
                         // All debug info logged to castspell_debug.txt
                         lastCastResult = SDK::SpellCaster::CastQAtMouse();
                     }
                 }
                 
                 // Show cast feedback
                 if (castTriggered && GetTickCount() - lastCastTime < 2000) {
                     if (lastCastResult) {
                         ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), ">>> Cast Q SUCCESS! <<<");
                     } else {
                         ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), ">>> Cast Q FAILED - Check log! <<<");
                     }
                 }
                 
                 if (lastCastTime > 0) {
                     ImGui::Text("Last cast: %d ms ago", GetTickCount() - lastCastTime);
                     ImGui::Text("Result: %s", lastCastResult ? "SUCCESS" : "FAILED");
                 }
                 
                 ImGui::Spacing();
                 ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Debug log: castspell_debug.txt");
             } else {
                 ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Check box above to enable debug casting");
                 castTriggered = false;
             }
             
             ImGui::Spacing();
             ImGui::Separator();
             
             // Current Settings Summary
             ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Current Settings:");
             ImGui::Text("Hitchance: %s", hitchanceItems[predHitchance]);
             ImGui::Text("Range: %.0f%% of max", predRangePercent);
        }
        else {
             ImGui::Text("Select a tab from the sidebar.");
        }

        ImGui::EndChild();
        ImGui::Columns(1); // Reset columns
		ImGui::End();
	}
}
