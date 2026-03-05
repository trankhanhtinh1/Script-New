#pragma once

// ============================================================================
// SDK — League of Legends Internal SDK (C++)
// Full port of EnsoulSharp.SDK concepts to C++ internal
// ============================================================================
//
// Usage:
//   #include "sdk/SDK.h"
//
//   // In init:
//   Globals::Init();
//
//   // In render loop (each frame):
//   SDK::GameObjects::Update();
//   SDK::HealthPrediction::Update();
//   SDK::Orbwalker::OnUpdate();
//
//   // Access player data:
//   auto& me = SDK::GameObjects::Player;
//   float hp = me.GetHealth();
//   float ad = me.GetTotalAD();
//
//   // Target selection:
//   auto target = SDK::TargetSelector::GetTarget(1000.0f);
//
//   // Spell casting with prediction:
//   auto Q = SDK::SpellCaster::Line(SDK::SpellSlotId::Q, 1150, 2000, 120);
//   if (Q.CastWithPrediction(target, SDK::HitChance::High)) { /* success */ }
//
//   // Drawing:
//   SDK::Drawing::DrawAttackRange();
//   SDK::Drawing::DrawCircle(target.GetPosition(), 100, IM_COL32(255,0,0,255));
//
// ============================================================================

// Phase 1: Core Types
#include "Enums.h"
#include "Game.h"

// Phase 1: Game Objects
#include "GameObject.h"
#include "SpellBook.h"
#include "BuffManager.h"
#include "AiManager.h"
#include "Missile.h"

// Phase 2: Object Management
#include "ObjectManager.h"
#include "GameObjects.h"

// Phase 3: Combat Systems
#include "DamageCalc.h"
#include "HealthPrediction.h"
#include "Collisions.h"
#include "TargetSelector.h"
#include "Prediction.h"
#include "SpellCaster.h"
#include "Orbwalker.h"

// Phase 4: Event System
#include "EventSystem.h"

// Phase 4.5: Spell Database & Trackers
#include "SpellDatabaseEntry.h"
#include "SpellDatabase.h"
#include "SummonerTracker.h"
#include "RecallTracker.h"
#include "AutoLevel.h"

// Phase 5: Drawing / Rendering
#include "Drawing.h"

// Phase 6: Menu UI (EnsoulSharp SDK compatible)
#include "MenuUI.h"
#include "ConfigManager.h"