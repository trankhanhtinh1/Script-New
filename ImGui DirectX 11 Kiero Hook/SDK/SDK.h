#pragma once

// ============================================================================
// SDK — League of Legends Internal SDK (C++)
// Full port of EnsoulSharp.SDK concepts to C++ internal
// ============================================================================
//
// Directory structure (mirrors EnsoulSharp.SDK/Core/):
//
//   core/                          ← Offset layer (Globals, Offsets, Vector)
//   sdk/
//     SDK.h                        ← THIS FILE (master include)
//     Enums.h                      ← All enumerations
//     Game.h                       ← Game state (time, ping, chat/shop)
//     GameObjects/                 ← Core game object wrappers
//       GameObject.h, GameObjects.h, ObjectManager.h,
//       SpellBook.h, BuffManager.h, AiManager.h, Missile.h
//     Events/                      ← Event system
//       EventSystem.h, Gapcloser.h, InterruptableSpell.h,
//       Dash.h, Stealth.h, TurretAggro.h, Teleport.h
//     Math/                        ← Geometry & Prediction
//       Polygon.h, Prediction.h, Cluster.h, GamePath.h,
//       Collisions.h, MathUtils.h
//     Wrappers/                    ← Game wrappers
//       Damages/   DamageCalc.h, DamageLibrary.h, DamagePassives.h, DamageMastery.h
//       Spells/    SpellCaster.h, SpellTypes.h, SpellDatabase.h, LastCast.h, ...
//       Orbwalking/ Orbwalker.h, HealthPrediction.h
//       TargetSelector/ TargetSelector.h
//     UI/                          ← Menu & Drawing
//       MenuUI.h, Drawing.h, ConfigManager.h, KeyConvert.h
//     Utils/                       ← Utilities
//       Invulnerable.h, AutoAttackUtil.h, AutoLevel.h, DelayAction.h, ...
//     Data/                        ← JSON resources
//
//   plugins/                       ← Script/Plugin layer
//     IPlugin.h, PluginManager.h
//     core/       OrbwalkerPlugin.h, TargetSelectorPlugin.h
//     awareness/  Awareness.h
//
// Usage:
//   #include "sdk/SDK.h"     // Include everything
//
//   auto& me = SDK::GameObjects::Player;
//   auto target = SDK::TargetSelector::GetTarget(1000.0f);
//   auto Q = SDK::SpellCaster::Line(SDK::SpellSlotId::Q, 1150, 2000, 120);
//   if (Q.CastWithPrediction(target, SDK::HitChance::High)) { /* success */ }
//
// ============================================================================

// ─── Phase 1: Core Types ────────────────────────────────────────────────────
#include "Enums.h"
#include "Game.h"

// ─── Phase 2: Game Objects ──────────────────────────────────────────────────
#include "GameObjects/GameObject.h"
#include "GameObjects/SpellBook.h"
#include "GameObjects/BuffManager.h"
#include "GameObjects/AiManager.h"
#include "GameObjects/Missile.h"
#include "GameObjects/ObjectManager.h"
#include "GameObjects/GameObjects.h"

// ─── Phase 3: Math & Geometry ───────────────────────────────────────────────
#include "Math/Polygon.h"
#include "Math/MathUtils.h"
#include "Math/GamePath.h"
#include "Math/Collisions.h"
#include "Math/Prediction.h"
#include "Math/Cluster.h"

// ─── Phase 4: Wrappers — Damages ────────────────────────────────────────────
#include "Wrappers/Damages/DamageCalc.h"
#include "Wrappers/Damages/DamagePassives.h"
#include "Wrappers/Damages/DamageMastery.h"
#include "Wrappers/Damages/DamageLibrary.h"

// ─── Phase 4: Wrappers — Target Selector ────────────────────────────────────
#include "Wrappers/TargetSelector/TargetSelector.h"

// ─── Phase 4: Wrappers — Spells ─────────────────────────────────────────────
#include "Wrappers/Spells/SpellDatabaseEntry.h"
#include "Wrappers/Spells/SpellDatabase.h"
#include "Wrappers/Spells/LastCastedSpellEntry.h"
#include "Wrappers/Spells/LastCastPacketSentEntry.h"
#include "Wrappers/Spells/LastCast.h"
#include "Wrappers/Spells/SpellCaster.h"
#include "Wrappers/Spells/SpellTypes.h"

// ─── Phase 4: Wrappers — Orbwalking ─────────────────────────────────────────
#include "Wrappers/Orbwalking/HealthPrediction.h"
#include "Wrappers/Orbwalking/Orbwalker.h"

// ─── Phase 5: Events ────────────────────────────────────────────────────────
#include "Events/EventSystem.h"
#include "Events/Gapcloser.h"
#include "Events/InterruptableSpell.h"
#include "Events/Dash.h"
#include "Events/Stealth.h"
#include "Events/TurretAggro.h"
#include "Events/Teleport.h"

// ─── Phase 6: Utils ─────────────────────────────────────────────────────────
#include "Utils/Invulnerable.h"
#include "Utils/AutoAttackUtil.h"
#include "Utils/MinionUtils.h"
#include "Utils/JungleUtils.h"
#include "Utils/DelayAction.h"
#include "Utils/ActionQueue.h"
#include "Utils/CursorUtils.h"
#include "Utils/WeightedRandom.h"
#include "Utils/TickOperation.h"
#include "Utils/Logging.h"
#include "Utils/SummonerTracker.h"
#include "Utils/RecallTracker.h"
#include "Utils/AutoLevel.h"

// ─── Phase 7: UI ────────────────────────────────────────────────────────────
#include "UI/Drawing.h"
#include "UI/KeyConvert.h"
#include "UI/MenuUI.h"
#include "UI/ConfigManager.h"
