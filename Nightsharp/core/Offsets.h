#pragma once

// Compatibility shim for old NightSharp core logic.
// Rule:
// - Prefer Offsets.generated.h whenever a current generated equivalent exists.
// - Preserve legacy literals only for names that do not exist in Offsets.generated.h yet.
//   Those are intentionally kept visible so they can be audited and removed later.

#include "Offsets.generated.h"

namespace Offset {

namespace Global {
constexpr auto LocalPlayer = GameObjectsRuntime::Player;
constexpr auto HeroManager = GameObjectsRuntime::Heroes;
constexpr auto GameTime = GameRuntime::GameTime;
constexpr auto MissileManager = GameObjectsRuntime::Missiles;
constexpr auto NavGrid = NavGridRuntime::NavGrid;
constexpr auto HudInstance = DrawingRuntime::HudInstance;
constexpr auto UnderMouseObj = GameObjectsRuntime::UnderMouseObject;
constexpr auto ViewPort = DrawingRuntime::ViewPort;
constexpr auto ObjectManager = GameObjectsRuntime::Objects;
constexpr auto MinionManager = GameObjectsRuntime::Minions;
constexpr auto NetInstance = GameRuntime::NetInstance;
constexpr auto CursorInstance = GameRuntime::CursorPosRaw;
constexpr auto MouseScreenVec2 = GameRuntime::MouseScreenVec2;
constexpr auto ChatClient = GameRuntime::ChatClient;
constexpr auto ChatInstance = GameRuntime::ChatInstance;
constexpr auto r3dRenderer = DrawingRuntime::Renderer;
constexpr auto ViewPort2 = DrawingRuntime::ViewPort2;
constexpr auto TurretManager = GameObjectsRuntime::Turrets;
constexpr auto ShopInstance = GameRuntime::ShopInstance;
constexpr auto OpenWindowsArray = GameRuntime::OpenWindowsArray;
constexpr auto OpenWindowsCount = GameRuntime::OpenWindowsCount;

// Legacy runtime global: not present in Offsets.generated.h yet.
constexpr auto MySpellState = 0x1DA7FC8;
} // namespace Global

namespace Flag {
constexpr auto IssueOrderFlag = ControlRuntime::IssueOrderFlag;
constexpr auto IssueOrder = IssueOrderFlag;
constexpr auto CastSpellFlag = ControlRuntime::CastSpellFlag;
constexpr auto CastSpell = CastSpellFlag;
} // namespace Flag

namespace Hud {
constexpr auto Camera = HudRuntime::Camera;
constexpr auto Input = HudRuntime::Input;
constexpr auto UserData = HudRuntime::UserData;
constexpr auto SpellInfo = HudRuntime::SpellInfo;
constexpr auto CameraZoom = HudRuntime::CameraZoom;
constexpr auto CameraZoomLimits = HudRuntime::CameraZoomLimits;
constexpr auto AltZoomLimits = HudRuntime::AltZoomLimits;
constexpr auto ZoomLockFlag1 = HudRuntime::ZoomLockFlag1;
constexpr auto ZoomLockFlag2 = HudRuntime::ZoomLockFlag2;
constexpr auto MouseWorldPos = HudRuntime::MouseWorldPos;
constexpr auto ViewportW2S = HudRuntime::ViewportW2S;

constexpr auto ZoomLimitsMin = HudZoomLayout::ZoomLimitsMin;
constexpr auto ZoomLimitsMax = HudZoomLayout::ZoomLimitsMax;
constexpr auto SelectedObjNetId = HudInputLayout::SelectedObjNetId;

// Legacy field preserved from the old header layout; not emitted separately in
// the generated file yet.
constexpr auto ChatOpen = 0x10;
} // namespace Hud

namespace Function {
constexpr auto IssueOrderCore = ControlRuntime::IssueOrder;
constexpr auto IssueOrder = IssueOrderCore;
constexpr auto WorldToScreen = DrawingRuntime::WorldToScreen;
constexpr auto CastSpellSafe = ControlRuntime::CastSpellWrap;
constexpr auto PrintChat = GameRuntime::PrintChat;
constexpr auto GetBoundingRadius = ControlRuntime::GetBoundingRadius;
constexpr auto GetAttackDelay = ControlRuntime::GetAttackDelay;
constexpr auto GetAttackWindup = ControlRuntime::GetAttackWindup;
constexpr auto GetCollisionFlags = NavGridRuntime::GetCollisionFlags;
constexpr auto GetPing = GameRuntime::GetPing;

constexpr auto GetFirstObject = ObjectManagerRuntime::GetFirstObject;
constexpr auto GetFirstObjectAlt = GetFirstObject;
constexpr auto GetNextObject = ObjectManagerRuntime::GetNextObject;
constexpr auto FindObject = ObjectManagerRuntime::FindObject;
constexpr auto GetAiManager = NavGridRuntime::GetAiManager;
constexpr auto GetAiManagerInner = NavGridRuntime::GetAiManagerInner;

constexpr auto IsAlive = ControlRuntime::IsAlive;
constexpr auto CompareTypeFlags = ClassificationRuntime::CompareTypeFlags;
constexpr auto IsJungleMonster = ClassificationRuntime::IsJungleMonster;
constexpr auto GetJungleType = ClassificationRuntime::GetJungleType;

constexpr auto IsBuilding = ClassificationRuntime::IsBuilding;
constexpr auto IsDead = ClassificationRuntime::IsDead;
constexpr auto IsTargetableByUnit = UnitQueryRuntime::IsTargetableByUnit;
constexpr auto IsVulnerable = ClassificationRuntime::IsVulnerable;
constexpr auto IsDragon = ClassificationRuntime::IsDragon;
constexpr auto IsElderDragon = ClassificationRuntime::IsElderDragon;
constexpr auto IsBaron = ClassificationRuntime::IsBaron;
constexpr auto IsSelectable = ClassificationRuntime::IsSelectable;
constexpr auto IsFleeing = ClassificationRuntime::IsFleeing;
constexpr auto IsNoRender = ClassificationRuntime::IsNoRender;
constexpr auto CanAttack = ClassificationRuntime::CanAttack;
constexpr auto GetSpellCastInfo = ControlRuntime::GetSpellCastInfo;
constexpr auto GetSpellSlot = ControlRuntime::GetSpellSlot;
constexpr auto GetResourceType = ControlRuntime::GetResourceType;
constexpr auto HasBuffOfType = UnitQueryRuntime::HasBuffOfType;
constexpr auto GetGoldRedirectTgt = UnitQueryRuntime::GetGoldRedirectTgt;
constexpr auto LevelSpell = 0x0;
constexpr auto GetMapID = GameRuntime::GetMapID;

constexpr auto CreateClientEffect = EventRuntime::CreateClientEffect;
constexpr auto OnCreateObject = EventRuntime::OnCreateObject;
constexpr auto OnGameUpdate = EventRuntime::OnGameUpdate;
constexpr auto OnProcessSpell = EventRuntime::OnProcessSpell;
constexpr auto OnSpellImpact = EventRuntime::OnSpellImpact;
constexpr auto OnStopCast = EventRuntime::OnStopCast;
constexpr auto OnFinishCast = EventRuntime::OnFinishCast;
constexpr auto OnBuffAdd = EventRuntime::OnBuffAdd;
} // namespace Function

namespace ManagerList {
constexpr auto Items = ObjectManagerRuntime::ManagerListItems;
constexpr auto Size = ObjectManagerRuntime::ManagerListSize;
} // namespace ManagerList

namespace TypeFlags {
constexpr auto IsObjectAI = TypeFlagsRuntime::IsObjectAI;
constexpr auto Minion = TypeFlagsRuntime::Minion;
constexpr auto Hero = TypeFlagsRuntime::Hero;
constexpr auto Turret = TypeFlagsRuntime::Turret;
constexpr auto Plant = TypeFlagsRuntime::Plant;
constexpr auto IsRecalling = TypeFlagsRuntime::IsRecalling;
} // namespace TypeFlags

namespace BuffManager {
constexpr auto Offset = BuffManagerRuntime::BuffManagerOffset;
constexpr auto EntriesEnd = BuffManagerLayout::EntriesEnd;
constexpr auto EntryBuff = BuffEntryLayout::EntryBuff;
constexpr auto BuffType = BuffDataLayout::BuffType;
constexpr auto BuffNamePtr = BuffDataLayout::BuffNamePtr;
constexpr auto BuffStartTime = BuffDataLayout::BuffStartTime;
constexpr auto BuffEndTime = BuffDataLayout::BuffEndTime;
constexpr auto BuffStacksAlt = BuffDataLayout::BuffStacksAlt;
constexpr auto BuffStacks = BuffDataLayout::BuffStacks;
constexpr auto BuffNameStr = BuffNameLayout::BuffNameStr;
} // namespace BuffManager

namespace GameObject {
constexpr auto Index = All::Index;
constexpr auto Team = All::Team;
constexpr auto Name = All::Name;
constexpr auto NetId = All::NetId;
constexpr auto Dead = All::Dead;
constexpr auto Position = All::Position;
constexpr auto EffectEmitter = All::EffectEmitterHandle;
constexpr auto Visibility = All::Visibility;
constexpr auto MissileClient = All::MissileClientHandle;
constexpr auto Visible = All::Visible;
constexpr auto Radius = All::Radius;
constexpr auto CharacterName = All::CharacterName;
constexpr auto CharacterData = All::CharacterData;
constexpr auto Direction = All::Direction;
constexpr auto ItemList = All::ItemList;

constexpr auto TeamAlt = 0x259;
constexpr auto IsInvulnerable = All::IsInvulnerable;
constexpr auto RecallState = All::RecallState;
} // namespace GameObject

namespace Health {
constexpr auto HP = AttackableUnit::HP;
constexpr auto MaxHP = AttackableUnit::MaxHP;
constexpr auto HPMaxPenalty = AttackableUnit::HPMaxPenalty;
constexpr auto AllShield = AttackableUnit::AllShield;
constexpr auto PhysicalShield = AttackableUnit::PhysicalShield;
constexpr auto MagicalShield = AttackableUnit::MagicalShield;
constexpr auto ChampSpecific = AttackableUnit::ChampSpecific;
constexpr auto InHealAllied = AttackableUnit::InHealAllied;
constexpr auto InHealEnemy = AttackableUnit::InHealEnemy;
constexpr auto InDamage = AttackableUnit::InDamage;
constexpr auto StopShieldFade = AttackableUnit::StopShieldFade;

// Legacy relative-property aliases preserved from old header layout.
constexpr auto RP_AllShield = 0xA0;
constexpr auto RP_PhysicalShield = 0xC8;
constexpr auto RP_MagicalShield = 0xF0;
constexpr auto RP_ChampSpecific = 0x118;
constexpr auto RP_InHealAllied = 0x140;
constexpr auto RP_InHealEnemy = 0x168;
constexpr auto RP_InDamage = 0x190;
} // namespace Health

namespace Mana {
constexpr auto MP = AIHeroClient::MP;
constexpr auto MaxMP = AIHeroClient::MaxMP;
constexpr auto PAR = AIHeroClient::PAR;
constexpr auto MaxPAR = AIHeroClient::MaxPAR;
} // namespace Mana

namespace Targetable {
constexpr auto IsTargetable = AttackableUnit::IsTargetable;
constexpr auto TargetableFlags = AttackableUnit::TargetableFlags;
} // namespace Targetable

namespace ActionState {
constexpr auto State1 = AttackableUnit::ActionState1;
constexpr auto State2 = AttackableUnit::ActionState2;

// Bit flags, not memory offsets.
constexpr unsigned int CanAttack = 0x00000001;
constexpr unsigned int CanCast = 0x00000002;
constexpr unsigned int CanMove = 0x00000004;
constexpr unsigned int Immovable = 0x00000008;
constexpr unsigned int Charmed = 0x00000080;
constexpr unsigned int Feared = 0x00000100;
constexpr unsigned int Taunted = 0x00000200;
constexpr unsigned int Stunned = 0x00002000;
constexpr unsigned int Suppressed = 0x00008000;
constexpr unsigned int Grounded = 0x00100000;
constexpr unsigned int Stasis = 0x00200000;

constexpr unsigned int ImmobileMask = Immovable | Stunned | Suppressed | Stasis;
constexpr unsigned int ForcedMoveMask = Charmed | Feared | Taunted;
constexpr unsigned int NoDashMask = Grounded;
} // namespace ActionState

namespace HeroStats {
constexpr auto FlatPhysicalDmgMod = AIHeroClient::FlatPhysicalDmgMod;
constexpr auto AttackSpeedMod = AIHeroClient::AttackSpeedMod;
constexpr auto PercentAttackSpeedMod = AIHeroClient::PercentAttackSpeedMod;
constexpr auto FlatBaseAttackSpeedMod = AIHeroClient::FlatBaseAttackSpeedMod;
constexpr auto BaseAttackDamage = AIHeroClient::BaseAttackDamage;
constexpr auto BaseAtkDmgSansScale = AIHeroClient::BaseAtkDmgSansScale;
constexpr auto FlatBaseAtkDmgMod = AIHeroClient::FlatBaseAtkDmgMod;
constexpr auto PercentBaseAtkDmgMod = AIHeroClient::PercentBaseAtkDmgMod;
constexpr auto BaseAbilityDamage = AIHeroClient::BaseAbilityDamage;
constexpr auto Armor = AIHeroClient::Armor;
constexpr auto BonusArmor = AIHeroClient::BonusArmor;
constexpr auto SpellBlock = AIHeroClient::SpellBlock;
constexpr auto BonusSpellBlock = AIHeroClient::BonusSpellBlock;
constexpr auto Crit = AIHeroClient::Crit;
constexpr auto CritDamageMultiplier = AIHeroClient::CritDamageMultiplier;
constexpr auto HPRegenRate = AIHeroClient::HPRegenRate;
constexpr auto BaseHPRegenRate = AIHeroClient::BaseHPRegenRate;
constexpr auto MoveSpeed = AIHeroClient::MoveSpeed;
constexpr auto AttackRange = AIHeroClient::AttackRange;
constexpr auto AbilityHaste = AIHeroClient::AbilityHaste;
constexpr auto PercentCCReduction = AIHeroClient::PercentCCReduction;
constexpr auto FlatArmorPen = AIHeroClient::FlatArmorPen;
constexpr auto PhysicalLethality = AIHeroClient::PhysicalLethality;
constexpr auto PercentArmorPen = AIHeroClient::PercentArmorPen;
constexpr auto PercentBonusArmorPen = AIHeroClient::PercentBonusArmorPen;
constexpr auto FlatMagicPen = AIHeroClient::FlatMagicPen;
constexpr auto PercentMagicPen = AIHeroClient::PercentMagicPen;
constexpr auto PercentBonusMagicPen = AIHeroClient::PercentBonusMagicPen;
constexpr auto PercentLifeSteal = AIHeroClient::PercentLifeSteal;
constexpr auto PercentSpellVamp = AIHeroClient::PercentSpellVamp;
constexpr auto PercentOmnivamp = AIHeroClient::PercentOmnivamp;
} // namespace HeroStats

namespace Hero {
constexpr auto Gold = AIHeroClient::Gold;
constexpr auto GoldTotal = AIHeroClient::GoldTotal;
constexpr auto Exp = AIHeroClient::Exp;
constexpr auto LevelRef = AIHeroClient::LevelRef;
constexpr auto LevelUpPoints = AIHeroClient::LevelUpPoints;
constexpr auto VisionScore = AIHeroClient::VisionScore;
} // namespace Hero

namespace ItemSystem {
constexpr auto SlotInfo = ItemRuntime::SlotInfo;
constexpr auto InfoData = ItemRuntime::InfoData;
constexpr auto InfoStacks = ItemRuntime::InfoStacks;
constexpr auto DataItemId = ItemRuntime::DataItemId;
constexpr auto DataAbilityHaste = ItemRuntime::DataAbilityHaste;
constexpr auto DataHealth = ItemRuntime::DataHealth;
constexpr auto DataArmor = ItemRuntime::DataArmor;
constexpr auto DataMR = ItemRuntime::DataMR;
constexpr auto DataAD = ItemRuntime::DataAD;
constexpr auto DataAP = ItemRuntime::DataAP;
constexpr auto DataAtkSpeedMult = ItemRuntime::DataAtkSpeedMult;
} // namespace ItemSystem

namespace AiManager {
constexpr auto Offset = AiManagerInnerCompatLayout::Offset;
constexpr auto InnerManager = AiManagerInnerCompatLayout::InnerManager;

// Exact old NightSharp view: offsets relative to the resolved inner pointer
// as historically consumed by AiManager.h. MCP IDA re-proves that:
//   - AIBaseClient + Offset points at the encrypted AiManager block
//   - GetAiManagerInner returns [decrypted + InnerManager]
// Keep the rest of this view for legacy/manual-map compatibility until each
// field is re-audited directly in the current binary.
namespace InnerCompat {
constexpr auto TargetPosition = AiManagerInnerCompatLayout::TargetPosition;
constexpr auto Velocity = AiManagerInnerCompatLayout::Velocity;
constexpr auto IsMoving = AiManagerInnerCompatLayout::IsMoving;
constexpr auto CurrentSegment = AiManagerInnerCompatLayout::CurrentSegment;
constexpr auto PathStart = AiManagerInnerCompatLayout::PathStart;
constexpr auto PathEnd = AiManagerInnerCompatLayout::PathEnd;
constexpr auto Segments = AiManagerInnerCompatLayout::Segments;
constexpr auto NavArray = AiManagerInnerCompatLayout::Segments;
constexpr auto SegmentsCount = AiManagerInnerCompatLayout::SegmentsCount;
constexpr auto HasPath = AiManagerInnerCompatLayout::HasPath;
constexpr auto DashSpeed = AiManagerInnerCompatLayout::DashSpeed;
constexpr auto DashMaxRangeSq = AiManagerInnerCompatLayout::DashMaxRangeSq;
constexpr auto DashDistRemaining = AiManagerInnerCompatLayout::DashDistRemaining;
constexpr auto DashDuration = AiManagerInnerCompatLayout::DashDuration;
constexpr auto IsDashing = AiManagerInnerCompatLayout::IsDashing;
constexpr auto DashEndPos = AiManagerInnerCompatLayout::DashEndPos;
constexpr auto ServerPos = AiManagerInnerCompatLayout::ServerPos;
constexpr auto MoveVec3 = AiManagerInnerCompatLayout::MoveVec3;
constexpr auto DashTargetNetId = AiManagerInnerCompatLayout::DashTargetNetId;
constexpr auto DashSecondaryNetId = AiManagerInnerCompatLayout::DashSecondaryNetId;
constexpr auto PreviousPos = AiManagerInnerCompatLayout::PreviousPos;
} // namespace InnerCompat

// Resolved navigation-base view. Keep this separate so future patches can
// re-audit the inner->nav_base relation without breaking old call sites.
// MCP IDA confirms nav_base is reached through the inner helper chain
// (inner -> [inner+0x8] -> add *(type+0x4) -> +0x8), so do not treat
// "inner + 0x20" as a permanent invariant even if current fields line up.
namespace NavBase {
constexpr auto RawInnerPtr = AiManagerNavBaseLayout::RawInnerPtr;
constexpr auto InnerTypePtr = AiManagerNavBaseLayout::InnerTypePtr;
constexpr auto InnerTypeAdjust = AiManagerNavBaseLayout::InnerTypeAdjust;
// Derived class-adjust tail from MCP IDA chain, not emitted as a standalone
// generated field in the current dump.
constexpr auto FinalBaseAdd = 0x8;
constexpr auto PathState = AiManagerNavBaseLayout::PathState;
constexpr auto CurrentSegment = PathState;
constexpr auto PathEndFallback = AiManagerNavBaseLayout::PathEndFallback;
constexpr auto Segments = AiManagerNavBaseLayout::Segments;
constexpr auto SegmentsCount = AiManagerNavBaseLayout::SegmentsCount;
constexpr auto WaypointArray = Segments;
constexpr auto WaypointCount = SegmentsCount;
constexpr auto NavigationPathState = PathState;
constexpr auto ServerPosition = AiManagerNavBaseLayout::ServerPosition;
constexpr auto CurrentNavPos = ServerPosition;
constexpr auto MoveVector = AiManagerNavBaseLayout::MoveVector;
constexpr auto PreviousPosition = AiManagerNavBaseLayout::PreviousPosition;
constexpr auto NavModeFlag = AiManagerNavBaseLayout::NavModeFlag;
constexpr auto OrderPosition = AiManagerNavBaseLayout::OrderPosition;
constexpr auto NavUnknownDword = AiManagerNavBaseLayout::NavUnknownDword;
constexpr auto DashSpeedInner = AiManagerNavBaseLayout::DashSpeedInner;
constexpr auto IsDashingInner = AiManagerNavBaseLayout::IsDashingInner;
} // namespace NavBase

namespace PathStateLayout {
constexpr auto CurrentIndex = AiManagerPathStateLayout::CurrentIndex;
constexpr auto FallbackEnd = AiManagerPathStateLayout::FallbackEnd;
constexpr auto PointsPtr = AiManagerPathStateLayout::PointsPtr;
constexpr auto Count = AiManagerPathStateLayout::Count;
} // namespace PathStateLayout

// Legacy top-level aliases preserved exactly for old NightSharp logic.
constexpr auto TargetPosition = InnerCompat::TargetPosition;
constexpr auto Velocity = InnerCompat::Velocity;
constexpr auto IsMoving = InnerCompat::IsMoving;
constexpr auto CurrentSegment = InnerCompat::CurrentSegment;
constexpr auto PathStart = InnerCompat::PathStart;
constexpr auto PathEnd = InnerCompat::PathEnd;
constexpr auto Segments = InnerCompat::Segments;
constexpr auto NavArray = InnerCompat::NavArray;
constexpr auto SegmentsCount = InnerCompat::SegmentsCount;
constexpr auto HasPath = InnerCompat::HasPath;
constexpr auto DashSpeed = InnerCompat::DashSpeed;
constexpr auto DashMaxRangeSq = InnerCompat::DashMaxRangeSq;
constexpr auto DashDistRemaining = InnerCompat::DashDistRemaining;
constexpr auto DashDuration = InnerCompat::DashDuration;
constexpr auto IsDashing = InnerCompat::IsDashing;
constexpr auto DashEndPos = InnerCompat::DashEndPos;
constexpr auto ServerPos = InnerCompat::ServerPos;
constexpr auto MoveVec3 = InnerCompat::MoveVec3;
constexpr auto DashTargetNetId = InnerCompat::DashTargetNetId;
constexpr auto DashSecondaryNetId = InnerCompat::DashSecondaryNetId;
constexpr auto PreviousPos = InnerCompat::PreviousPos;
} // namespace AiManager

namespace SpellBook {
constexpr auto Offset = SpellRuntime::SpellBookOffset;
constexpr auto SpellSlotArray = SpellBookLayout::SpellSlotArray;
constexpr auto ActiveSpellCast = SpellRuntime::ActiveSpellCast;

constexpr auto SlotLevel = SpellSlotLayout::SlotLevel;
constexpr auto SlotCooldown = SpellSlotLayout::SlotCooldown;
constexpr auto SlotStacks = SpellSlotLayout::SlotStacks;
constexpr auto SlotTotalCd = SpellSlotLayout::SlotTotalCd;
constexpr auto SlotSpellInput = SpellSlotLayout::SlotSpellInput;
constexpr auto SlotSpellInfo = SpellSlotLayout::SlotSpellInfo;

constexpr auto InputTargetNetId = SpellInputLayout::InputTargetNetId;
constexpr auto InputStartPos = SpellInputLayout::InputStartPos;
constexpr auto InputEndPos = SpellInputLayout::InputEndPos;

constexpr auto InfoSpellData = SpellInfoLayout::InfoSpellData;
constexpr auto SpellInfoNamePtr = SpellInfoLayout::SpellInfoNamePtr;
constexpr auto DataSpellName = SpellDataLayout::DataSpellName;
constexpr auto DataManaCost = SpellDataLayout::DataManaCost;
constexpr auto DataResource = SpellDataLayout::DataResource;

constexpr auto DataResourceBase = SpellDataResourceLayout::DataResourceBase;
constexpr auto ResCastRange = SpellDataResourceLayout::ResCastRange;
constexpr auto ResMissileSpeed = SpellDataResourceLayout::ResMissileSpeed;
constexpr auto ResLineWidth = SpellDataResourceLayout::ResLineWidth;
constexpr auto ResMaxAmmo = SpellDataResourceLayout::ResMaxAmmo;
constexpr auto ResCastType = SpellDataResourceLayout::ResCastType;
constexpr auto ResMissileSpec = SpellDataResourceLayout::ResMissileSpec;
constexpr auto ResScriptName = SpellDataResourceLayout::ResScriptName;
constexpr auto ResCooldownTime = SpellDataResourceLayout::ResCooldownTime;
constexpr auto ResAmmoRecharge = SpellDataResourceLayout::ResAmmoRecharge;
constexpr auto ResImgIconName = SpellDataResourceLayout::ResImgIconName;
} // namespace SpellBook

namespace SpellCastInfo {
constexpr auto SpellData = SpellCastInfoLayout::SpellData;
constexpr auto SrcIndex = SpellCastInfoLayout::SrcIndex;
constexpr auto StartPos = SpellCastInfoLayout::StartPos;
constexpr auto EndPos = SpellCastInfoLayout::EndPos;
constexpr auto CastPos = SpellCastInfoLayout::CastPos;
constexpr auto TargetIndex = SpellCastInfoLayout::TargetIndex;
constexpr auto CastDelay = SpellCastInfoLayout::CastDelay;
constexpr auto IsSpell = SpellCastInfoLayout::IsSpell;
constexpr auto IsSpecialAttack = SpellCastInfoLayout::IsSpecialAttack;
constexpr auto IsAuto = SpellCastInfoLayout::IsAuto;
constexpr auto Slot = SpellCastInfoLayout::Slot;
} // namespace SpellCastInfo

namespace Missile {
constexpr auto SpellDataPtr = MissileClient::SpellDataPtr;
constexpr auto Position = MissileClient::Position;
constexpr auto CastInfoBase = MissileClient::CastInfoBase;
constexpr auto CI_SpellData = MissileClient::CastInfoBase + SpellCastInfoLayout::SpellData;
constexpr auto SpellName = MissileClient::SpellName;
constexpr auto MissileName = MissileClient::MissileName;
constexpr auto StartPos = MissileClient::StartPos;
constexpr auto EndPos = MissileClient::EndPos;
constexpr auto CastEndPos = MissileClient::CastEndPos;
constexpr auto CasterNetId = MissileClient::CasterNetId;
constexpr auto SrcIndex = MissileClient::CasterNetId;
constexpr auto TargetNetId = MissileClient::TargetNetId;
constexpr auto CI_TargetNetId2 = MissileClient::TargetNetId - MissileClient::CastInfoBase;
constexpr auto MissileNetId = MissileClient::MissileNetId;
constexpr auto CI_MissileNetId = MissileClient::MissileNetId;
constexpr auto DestIndex = MissileClient::CastInfoBase + SpellCastInfoLayout::TargetIndex;

constexpr auto CI_REL_SpellData = SpellCastInfoLayout::SpellData;
constexpr auto CI_REL_SpellName = MissileClient::SpellName - MissileClient::CastInfoBase;
constexpr auto CI_REL_MissileName = MissileClient::MissileName - MissileClient::CastInfoBase;
constexpr auto CI_REL_StartPos = SpellCastInfoLayout::StartPos;
constexpr auto CI_REL_EndPos = SpellCastInfoLayout::EndPos;
constexpr auto CI_REL_CastEndPos = MissileClient::CastEndPos - MissileClient::CastInfoBase;
constexpr auto CI_REL_CasterNetId = SpellCastInfoLayout::SrcIndex;
constexpr auto CI_REL_MissileNetId = MissileClient::MissileNetId - MissileClient::CastInfoBase;
} // namespace Missile

namespace BasicAttack {
constexpr auto Base = BasicAttackRuntime::Base;
constexpr auto Offset1 = BasicAttackRuntime::Offset1;
constexpr auto Offset2 = BasicAttackRuntime::Offset2;
} // namespace BasicAttack

namespace MinionClass {
constexpr auto TypeOffset = MinionClassRuntime::TypeOffset;
constexpr auto Unset = MinionClassRuntime::Unset;
constexpr auto Pet = MinionClassRuntime::Pet;
constexpr auto JungleMonster = MinionClassRuntime::JungleMonster;
constexpr auto TeamMinion = MinionClassRuntime::TeamMinion;
constexpr auto MeleeLaneMinion = MinionClassRuntime::MeleeLaneMinion;
constexpr auto RangedLaneMinion = MinionClassRuntime::RangedLaneMinion;
constexpr auto SiegeLaneMinion = MinionClassRuntime::SiegeLaneMinion;
constexpr auto SuperLaneMinion = MinionClassRuntime::SuperLaneMinion;
} // namespace MinionClass

namespace JungleType {
constexpr auto TypeOffset = JungleTypeRuntime::TypeOffset;
constexpr auto Normal = JungleTypeRuntime::Normal;
constexpr auto Baron = JungleTypeRuntime::Baron;
constexpr auto Dragon = JungleTypeRuntime::Dragon;
} // namespace JungleType

namespace NavGrid {
constexpr auto NavGridMgr = NavGridLayout::NavGridMgr;
constexpr auto MinX = NavGridLayout::MinX;
constexpr auto MinZ = NavGridLayout::MinZ;
constexpr auto MaxX = NavGridLayout::MaxX;
constexpr auto MaxZ = NavGridLayout::MaxZ;
constexpr auto Data = NavGridLayout::Data;
constexpr auto Width = NavGridLayout::Width;
constexpr auto Height = NavGridLayout::Height;
constexpr auto Scale = NavGridLayout::Scale;
constexpr auto InverseScale = NavGridLayout::InverseScale;
constexpr auto GrassRegions = NavGridLayout::GrassRegions;
constexpr auto CellSize = NavGridLayout::CellSize;
constexpr auto FLAG_WALL = NavGridFlags::FlagWall;
constexpr auto FLAG_NOWALK = NavGridFlags::FlagNoWalk;
constexpr auto FLAG_BRUSH = NavGridFlags::FlagBrush;
constexpr auto FLAG_SPECIAL = NavGridFlags::FlagSpecial;
} // namespace NavGrid

namespace Animation {
constexpr auto CharacterData = AnimationLayout::CharacterData;
constexpr auto Component = AnimationLayout::Component;
constexpr auto Queue = AnimationLayout::Queue;
constexpr auto QueueEnd = AnimationLayout::QueueEnd;
constexpr auto QueueCapacityEnd = AnimationLayout::QueueCapacityEnd;
constexpr auto SkinIndex = AnimationLayout::SkinIndex;
constexpr auto CharacterDataResource = AnimationLayout::CharacterDataResource;
constexpr auto VariantEntries = AnimationLayout::VariantEntries;
constexpr auto VariantEntryCount = AnimationLayout::VariantEntryCount;
constexpr auto FallbackNamePtr = AnimationLayout::FallbackNamePtr;
constexpr auto FallbackState = AnimationLayout::FallbackState;
constexpr auto VariantEntryStride = AnimationLayout::VariantEntryStride;
constexpr auto VariantNamePtr = AnimationLayout::VariantNamePtr;
constexpr auto VariantState = AnimationLayout::VariantState;

// Resource-root fallback aliases preserved for older call sites. New logic
// should resolve the variant selected by SkinIndex first, then fall back here.
constexpr auto CurrentAnimation = FallbackNamePtr;
constexpr auto CurrentAnimationState = FallbackState;
} // namespace Animation

namespace Extra {
constexpr auto IsClone = ClassificationRuntime::IsClone;
constexpr auto ViewMatrixInst = DrawingRuntime::ViewProjOffset;
constexpr auto ProjMatrixRelative = DrawingMatrixRuntime::ProjMatrixRelative;
} // namespace Extra

} // namespace Offset
