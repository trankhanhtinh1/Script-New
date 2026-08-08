#pragma once

// EnsoulSharp-style short names for plugin code.
//
// A plugin can include only SDK/SDK.h and use:
//   Game::Time();
//   Drawing::WorldToScreen(...);
//   ObjectManager::Player();
//   GameObjects::EnemyHeroes();
//   Events::hook.OnProcessSpell += callback;
//   hook.OnProcessSpell += callback;
//   Menu / MenuBool / MenuKeyBind directly.
//   ProcessSpellEventArgs / ObjectEventArgs directly.
//
// Define NIGHTSHARP_SDK_NO_GLOBAL_FACADE before including SDK/SDK.h when a
// translation unit needs the fully-qualified SDK::* names only.

#ifndef NIGHTSHARP_SDK_NO_GLOBAL_FACADE

namespace UI = ::SDK::UI;
namespace Game = ::SDK::Game;
namespace Hud = ::SDK::Hud;
namespace Drawing = ::SDK::Drawing;
namespace View = ::SDK::View;
namespace Map = ::SDK::Map;
namespace TacticalMap = ::SDK::TacticalMap;
namespace MissionInfo = ::SDK::MissionInfo;
namespace NavMesh = ::SDK::NavMesh;
namespace ObjectManager = ::SDK::ObjectManager;
namespace GameObjects = ::SDK::GameObjects;
namespace Events = ::SDK::Events;
namespace Extensions = ::SDK::Extensions;
namespace Prediction = ::SDK::Prediction;
namespace Collision = ::SDK::Collision;
namespace Collisions = ::SDK::Collisions;
namespace HealthPrediction = ::SDK::HealthPrediction;
namespace Geometry = ::SDK::Geometry;
namespace Damage = ::SDK::Damage;
namespace DamageMod = ::SDK::DamageMod;
namespace Interrupter = ::SDK::Interrupter;
namespace Signals = ::SDK::Signals;
namespace Utils = ::SDK::Utils;
namespace MenuGUI = ::SDK::MenuGUI;
namespace MenuUI = ::SDK::MenuUI;
namespace PermaShow = ::SDK::UI::PermaShow;
namespace Icons = ::SDK::UI::Icons;
namespace DragonSoulData = ::SDK::Data::DragonSoulData;
namespace Keys = ::SDK::Keys;

using GameObject = ::SDK::GameObject;
using AttackableUnit = ::SDK::AttackableUnit;
using AIBaseClient = ::SDK::AIBaseClient;
using AIHeroClient = ::SDK::AIHeroClient;
using AIMinionClient = ::SDK::AIMinionClient;
// using AITurretClient = ::SDK::AITurretClient;
using MissileClient = ::SDK::MissileClient;
using InventorySlot = ::SDK::InventorySlot;
using Spell = ::SDK::Spell;
using SpellBookClient = ::SDK::SpellBookClient;
using SpellDataInstClient = ::SDK::SpellDataInstClient;
// using BarracksDampenerClient = ::SDK::BarracksDampenerClient;
// using HQClient = ::SDK::HQClient;
// using ShopClient = ::SDK::ShopClient;
using Obj_SpawnPoint = ::SDK::Obj_SpawnPoint;
using EffectEmitter = ::SDK::EffectEmitter;
using Orbwalker = ::SDK::Orbwalker;
using OrbwalkerBase = ::SDK::OrbwalkerBase;
using OrbwalkerSelector = ::SDK::OrbwalkerSelector;
using OrbwalkingActionArgs = ::SDK::OrbwalkingActionArgs;

using Vector2 = ::SDK::Vector2;
using Vector3 = ::SDK::Vector3;
using Vec2 = ::Vec2;
using Vec3 = ::Vec3;
using Vec4 = ::Vec4;
using GameObjectTeam = ::SDK::GameObjectTeam;
using SpellSlot = ::SDK::SpellSlot;
using CollisionFlags = ::SDK::CollisionFlags;
using DamageType = ::SDK::DamageType;
using HitChance = ::SDK::HitChance;
using SpellType = ::SDK::SpellType;
using OrbwalkingMode = ::SDK::OrbwalkingMode;
using OrbwalkingType = ::SDK::OrbwalkingType;
using KeyBindType = ::SDK::KeyBindType;
using CastStates = ::SDK::CastStates;
using CastType = ::SDK::CastType;
using DangerLevel = ::SDK::DangerLevel;
using GapcloserType = ::SDK::GapcloserType;
using HealthPredictionType = ::SDK::HealthPredictionType;
using JungleType = ::SDK::JungleType;
using MinionTypes = ::SDK::MinionTypes;
using NotificationIconType = ::SDK::NotificationIconType;
using SkillshotType = ::SDK::SkillshotType;
using TeleportStatus = ::SDK::TeleportStatus;
using TeleportType = ::SDK::TeleportType;
using TurretType = ::SDK::TurretType;
using MapId = ::SDK::Map::MapId;
using ElementalTerrain = ::SDK::MissionInfo::ElementalTerrain;

using CoreHookId = ::SDK::Events::CoreHookId;
using CoreHookArgs = ::SDK::Events::CoreHookArgs;
using GameUpdateEventArgs = ::SDK::Events::GameUpdateEventArgs;
using ObjectEventArgs = ::SDK::Events::ObjectEventArgs;
using BuffEventArgs = ::SDK::Events::BuffEventArgs;
using NewPathEventArgs = ::SDK::Events::NewPathEventArgs;
using IntegerPropertyChangeEventArgs = ::SDK::Events::IntegerPropertyChangeEventArgs;
using TeleportRawEventArgs = ::SDK::Events::TeleportRawEventArgs;
using ProcessSpellEventArgs = ::SDK::Events::ProcessSpellEventArgs;
using CastSpellEventArgs = ::SDK::Events::CastSpellEventArgs;
using PlayAnimationEventArgs = ::SDK::Events::PlayAnimationEventArgs;
using StopCastEventArgs = ::SDK::Events::StopCastEventArgs;
using LoadEventArgs = ::SDK::Events::LoadEventArgs;
using PredictionInput = ::SDK::PredictionInput;
using PredictionOutput = ::SDK::PredictionOutput;
using DashArgs = ::SDK::Events::Dash::DashArgs;
using GapCloserEventArgs = ::SDK::Events::Gapcloser::GapCloserEventArgs;
using InterruptableTargetEventArgs = ::SDK::Events::InterruptableSpell::InterruptableTargetEventArgs;
using TeleportEventArgs = ::SDK::Events::Teleport::TeleportEventArgs;
using TurretArgs = ::SDK::Events::Turret::TurretArgs;
using ::SDK::Events::hook;

using AutoAttack = ::SDK::Utils::AutoAttack;
using DelayAction = ::SDK::Utils::DelayAction;
using ActionQueue = ::SDK::Utils::ActionQueue;
using TickOperation = ::SDK::Utils::TickOperation;
using Render = ::SDK::Utils::Render;
using Cursor = ::SDK::Utils::Cursor;
using Cache = ::SDK::Utils::Cache;
using Invulnerable = ::SDK::Utils::Invulnerable;
using MathUtils = ::SDK::Utils::MathUtils;
using Minion = ::SDK::Utils::Minion;
using Performance = ::SDK::Utils::Performance;
using Storage = ::SDK::Utils::Storage;
using WeightedRandom = ::SDK::Utils::WeightedRandom;
using WindowsKeys = ::SDK::Utils::WindowsKeys;
using ::SDK::Extensions::IsValidTarget;
using PingCategory = ::SDK::Game::PingCategory;
using EmoteId = ::SDK::Game::EmoteId;
using SummonerEmoteSlot = ::SDK::Game::SummonerEmoteSlot;
using GameSendChatEventArgs = ::SDK::Game::GameSendChatEventArgs;
using GameDisplayChatEventArgs = ::SDK::Game::GameDisplayChatEventArgs;
using SendChatEventArgs = ::SDK::Game::SendChatEventArgs;
using DisplayChatEventArgs = ::SDK::Game::DisplayChatEventArgs;
using PingResourceType = ::SDK::Hud::PingResourceType;
using PingStatType = ::SDK::Hud::PingStatType;
using ClickType = ::SDK::Hud::ClickType;
using HudSnapshot = ::SDK::Hud::Snapshot;
using HudSelectedSpellInfo = ::SDK::Hud::SelectedSpellInfo;
using DragonSRXInfo = ::SDK::Hud::DragonSRXInfo;
inline DragonSRXInfo DragonSRX() { return ::SDK::Hud::DragonSRX(); }

// Script-friendly function facade. This intentionally mirrors C# plugin code
// where common SDK helpers can be called without spelling their namespace.
using namespace ::SDK::Extensions;
using namespace ::SDK::Game;
using namespace ::SDK::GameObjects;
using namespace ::SDK::Drawing;
using namespace ::SDK::Hud;
using namespace ::SDK::Map;
using namespace ::SDK::MissionInfo;
using namespace ::SDK::NavMesh;
using namespace ::SDK::View;
using namespace ::SDK::Prediction;
using namespace ::SDK::Collision;
using namespace ::SDK::HealthPrediction;
using namespace ::SDK::Geometry;
using namespace ::SDK::Damage;
using namespace ::SDK::DamageMod;
using namespace ::SDK::Interrupter;
using namespace ::SDK::MenuGUI;

#endif
