#pragma once
#include "includes.h"

namespace Menu
{
	// Settings
    extern bool menuOpen;
	extern bool showAllObjects;
	extern bool showObjectNames;
    
    // ESP Settings
    extern bool drawHeroes;
    extern bool drawMinions;
    extern bool drawTurrets;
    extern bool drawRange;
    extern bool drawDamage; 

    // Orbwalker Settings
    extern bool orbwalkerEnabled;
    extern bool randomActionDelay;
    extern float clickDelay;
    extern float windupBuffer; // In SECONDS like leagueoflegends-master (0.03 = 30ms)
    extern float attackBeforeCanAttack; // In SECONDS (anticipate next attack)
    extern float attackCastDelay;
    
    // Chat Block Settings
    extern bool blockKeysWhenChatOpen; // Block combo keys (Space, V, C, X, Z) when chat is open
    extern uint64_t chatStateTestOffset; // Offset để test chat state (default: 0x193EB74)
    
    // Attackable Units
    extern bool attackBarrels;
    extern bool attackJunglePlants;
    extern bool attackPets;
    extern bool attackWards;
    
    // Prioritize
    extern bool farmOverHarass;
    extern bool prioritizeSpecialMinions;
    extern bool prioritizeSmallJungle;
    extern bool prioritizeTurrets;
    
    // Farm Settings
    extern int farmDelay;
    extern int fastFarmDelay;
    extern bool turretFarmEnabled;
    extern int turretFarmMaxLevel;
    
    // Drawing
    extern bool drawAttackRange;
    extern bool bDrawAiState; // New debug toggle
    extern bool drawEnemyRange; // New
    extern bool drawKillableMinions;
    extern bool drawHoldPosition; 

    // UI State
    extern int currentTab; 
    
    // Debug
    extern bool showOffsetDebug; // Show offset debug overlay
    extern bool continuousMissileLog; // Continuous missile logging toggle
    extern bool continuousMissileScan; // Continuous missile scan toggle
    extern bool autoCastAndScan; // NEW: Auto cast spell then scan when unchecked
    extern bool continuousTurretLog; // Continuous turret logging toggle
    
    // Missile Drawing Debug
    extern bool drawMissiles;           // Draw all missiles
    extern bool drawMinionMissiles;     // Draw minion attack missiles
    extern bool drawTurretMissiles;     // Draw turret attack missiles
    extern bool drawChampionMissiles;   // Draw champion spell missiles
    
    // Evade Drawing
    extern bool drawSkillshots;         // Draw enemy skillshots (EzrealQ, etc.)
    
    // Target Selector Settings (based on NewTargetSelector.cs)
    // Modes: 0 = Smart AD/AP, 1 = Lowest Health, 2 = Most Priority
    extern int tsMode;
    extern bool tsForceSelected;
    extern bool tsOnlySelected;
    extern bool tsDrawSelected;
    extern bool tsHighlightSelected;
    extern float tsDrawColor[3]; // RGB color for selected target circle
    
    // ============================================================================
    // Prediction Settings
    // ============================================================================
    extern int predHitchance;           // 0=Low, 1=Medium, 2=High, 3=VeryHigh, 4=Immobile
    extern float predRangePercent;      // 50-100%, default 98%
    extern bool predDrawPredictedPos;   // Draw predicted position
    extern bool predDrawCastPos;        // Draw cast position
    extern bool predDrawHitbox;         // Draw skillshot hitbox
    extern bool predDebugCastSpell;     // Debug: Press S to cast Q
    
    void DumpObjects();

	// Render menu window
	void Render();
}
