#pragma once
#include <cstdint>

// ================================================================
// League of Legends - Offsets
// Updated: 2026-03-04 (LOLDumper v3.0 + offsetplugin.hpp + IDA MCP)
// Binary: League of Legends.exe
// Global/Function RVAs: from module size 0x202D000 (dump files)
// Struct offsets: verified via IDA on module size 0x2342000
// Base: 0x0 (relative offsets from module base)
//
// Sources:
//   [D]   = LOLDumper_full.h (pattern-scanned)
//   [P]   = offsetplugin.hpp (ida_lol_plugin.dll output)
//   [IDA] = IDA Pro MCP verified (decompile/disasm confirmed)
//   [S]   = struct offsets (unchanged between versions)
// ================================================================

namespace Offset {

// ================================================================
// GLOBAL POINTERS / INSTANCES
// ================================================================
namespace Global {
    constexpr auto LocalPlayer      = 0x1DAB720;   // [D][P] local player ptr
    constexpr auto HeroManager      = 0x1D7A430;   // [D][P] hero list ptr
    constexpr auto GameTime         = 0x1D88540;    // [D][P] game time float
    constexpr auto MissileManager   = 0x1D7DD50;   // [D] missile manager ptr
    constexpr auto NavGrid          = 0x1D7DCC8;   // [D] navigation grid ptr
    constexpr auto HudInstance      = 0x1D7A578;   // [D][P] HUD instance ptr
    constexpr auto UnderMouseObj    = 0x1D7DF50;   // [D] object under mouse cursor
    constexpr auto ViewPort         = 0x1D8D1B0;   // [D] viewport ptr
    constexpr auto ObjectManager    = 0x1D7A3D8;   // [D][P] object manager instance
    constexpr auto MinionManager    = 0x1D7A428;   // [CE] minion+jungle list (count=~150)
    constexpr auto NetInstance      = 0x1D7A3D0;   // [IDA] net instance (970 xrefs confirmed)
    constexpr auto CursorInstance   = 0x1E05698;   // [P] cursor position (Vec3)
    constexpr auto MouseScreenVec2  = 0x1D7DCF8;   // [D] mouse 2D screen position
    constexpr auto ChatClient       = 0x1D8D240;   // [IDA] chat client ptr (16 xrefs, null when chat closed)
    constexpr auto ChatInstance     = 0x1D7DFA0;   // [IDA] chat instance (82 xrefs confirmed)
    constexpr auto r3dRenderer      = 0x1E3FE78;   // [D] renderer instance (oViewPort2)
    constexpr auto ViewPort2        = 0x1E3FE78;   // [D] viewport2/renderer
    constexpr auto MySpellState     = 0x1D80AA0;   // [D] spell state global
    constexpr auto TurretManager    = 0x1D87068;   // [CE] turret list (count=24)
}

// ================================================================
// FLAGS
// ================================================================
namespace Flag {
    constexpr auto IssueOrder       = 0x1CDDF88;   // [D] issue order flag
    constexpr auto CastSpell        = 0x1CDDF20;   // [D] cast spell flag
}

// ================================================================
// FUNCTIONS (RVAs)
// ================================================================
namespace Function {
    // Core
    constexpr auto IssueOrder           = 0x29FC10;     // [D][P]
    constexpr auto WorldToScreen        = 0x1241320;    // [D][P]
    constexpr auto CastSpellWrapper     = 0x1E9A40;     // [D] (changed significantly!)
    constexpr auto CastSpellSafe        = 0xBB9DE0;     // [P]
    constexpr auto PrintChat            = 0xAFE2E0;     // [D]
    constexpr auto GetBoundingRadius    = 0x285640;     // [D][P]
    constexpr auto GetAttackDelay       = 0x52C620;     // [D][P]
    constexpr auto GetAttackWindup      = 0x52C520;     // [D][P] GetAttackCastDelay
    constexpr auto GetCollisionFlags    = 0x1195B30;    // [D]
    constexpr auto GetPing              = 0x669FA0;     // [D][P]

    // Object Iteration
    constexpr auto GetFirstObject       = 0x512970;     // [D] (main iterator)
    constexpr auto GetFirstObjectAlt    = 0x9D03A0;     // [P] (alt via plugin)
    constexpr auto GetNextObject        = 0x513460;     // [D][P]
    constexpr auto FindObject           = 0x512160;     // [P]
    constexpr auto GetAiManager         = 0x50AB30;     // [D]
    constexpr auto GetAIManagerAlt      = 0x28D400;     // [P] (alt via plugin)

    // Type Checks
    constexpr auto IsTurret             = 0x3085F0;     // [D]
    constexpr auto IsHero               = 0x3086F0;     // [D]
    constexpr auto IsBuilding           = 0x308820;     // [P]
    constexpr auto IsAlive              = 0x2E6350;     // [D][P]
    constexpr auto IsDead               = 0x29B380;     // [P]
    constexpr auto IsTargetableByUnit   = 0x29E280;     // [P]
    constexpr auto IsVulnerable         = 0x29C040;     // [P]
    constexpr auto IsJungleMonster      = 0x29C210;     // [P]
    constexpr auto IsDragon             = 0x29B630;     // [P]
    constexpr auto IsElderDragon        = 0x29B6A0;     // [P]
    constexpr auto IsBaron              = 0x29AA90;     // [P]
    constexpr auto IsSelectable         = 0x212170;     // [P]
    constexpr auto CompareTypeFlags     = 0x29CD30;     // [P]
    constexpr auto IsFleeing            = 0x20F330;     // [P]
    constexpr auto IsNoRender           = 0x20F380;     // [P]
    constexpr auto GetJungleType        = 0x66CEF0;     // [P]

    // Attack / Combat
    constexpr auto CanAttack            = 0x1F90D0;     // [P]
    constexpr auto GetSpellCastInfo     = 0x283F10;     // [P]
    constexpr auto GetSpellSlot         = 0x90A9B0;     // [P]
    constexpr auto GetResourceType      = 0x281230;     // [P]
    constexpr auto HasBuffOfType        = 0x296400;     // [P]
    constexpr auto GetGoldRedirectTgt   = 0x1FF990;     // [P]

    // Map / Minimap
    constexpr auto GetMapID             = 0x28E310;     // [CE] E8 ?? ?? ?? ?? 4C 89 7C 24 40 48 8D 4C 24 70

    // Hooks / Callbacks
    constexpr auto OnCreateObject       = 0x517E70;     // [P]
    constexpr auto OnGameUpdate         = 0x511210;     // [P]
    constexpr auto OnProcessSpell       = 0x9204F0;     // [P]
    constexpr auto OnSpellImpact        = 0x917C00;     // [P]
    constexpr auto OnStopCast           = 0x920800;     // [P]
    constexpr auto OnFinishCast         = 0x2C5760;     // [P]
    constexpr auto OnBuffAdd            = 0xBCDE00;     // [P]
    constexpr auto CreateClientEffect   = 0x869E70;     // [P]
}

// ================================================================
// GAME OBJECT STRUCT
// ================================================================
namespace GameObject {
    constexpr auto Index            = 0x10;         // [S]
    constexpr auto Team             = 0x3C;         // [S]
    constexpr auto Name             = 0x58;         // [S]
    constexpr auto NetId            = 0xCC;         // [D][S]
    constexpr auto Dead             = 0x250;        // [S]
    constexpr auto TeamAlt          = 0x259;        // [D]
    constexpr auto Position         = 0x25C;        // [S]
    constexpr auto EffectEmitter    = 0x258;        // [S]
    constexpr auto Visibility       = 0x2E0;        // [S]
    constexpr auto MissileClient    = 0x2D8;        // [S]
    constexpr auto Visible          = 0x308;        // [CE] verified: 0=fog, 1=visible on screen
    constexpr auto IsInvulnerable   = 0x5A0;        // [S]
    constexpr auto Radius           = 0x6F8;        // [D]
    constexpr auto RecallState      = 0xF48;        // [S]
    constexpr auto CharacterName    = 0x4330;       // [D]
    constexpr auto CharacterData    = 0x40C8;       // [S]
}

// ================================================================
// MANA
// ================================================================
namespace Mana {
    constexpr auto MP               = 0x360;        // [S]
    constexpr auto MaxMP            = 0x388;        // [S]
}

// ================================================================
// HEALTH (LeagueObfuscation<float>, 0x28 apart)
// ================================================================
namespace Health {
    constexpr auto HP               = 0x1080;       // [D]
    constexpr auto MaxHP            = 0x10A8;       // [D]
    constexpr auto HPMaxPenalty     = 0x10D0;       // [D]
    constexpr auto AllShield        = 0x1120;       // [D]
    constexpr auto PhysicalShield   = 0x1148;       // [D]
    constexpr auto MagicalShield    = 0x1170;       // [D]
    constexpr auto ChampSpecific    = 0x1198;       // [D]
    constexpr auto InHealAllied     = 0x11C0;       // [IDA] sub_2E3220: HP+320=0x1080+0x140
    constexpr auto InHealEnemy      = 0x11E8;       // [IDA] sub_2E3220: HP+360=0x1080+0x168
    constexpr auto InDamage         = 0x1210;       // [IDA] sub_2E3220: HP+400=0x1080+0x190
    constexpr auto StopShieldFade   = 0x1238;       // [IDA] sub_2E3220: HP+440=0x1080+0x1B8
}

// ================================================================
// TARGETABLE
// ================================================================
namespace Targetable {
    constexpr auto IsTargetable     = 0xED0;        // [D]
    constexpr auto TargetableFlags  = 0xEF8;        // [IDA] mIsTargetableToTeamFlags string xref
}

// ================================================================
// ACTION STATE
// ================================================================
namespace ActionState {
    constexpr auto State1           = 0x1470;       // [IDA] lea rdx,[rsi+1470h] -> sub_1FD490 "ActionState"
    constexpr auto State2           = 0x14A8;       // [IDA] 0x1470+0x38 -> sub_1FD490 "ActionState2"
}

// ================================================================
// DAMAGE MODIFIERS
// ================================================================
namespace DamageModifier {
    constexpr auto PhysDmgPercent   = 0x0E78;       // [IDA] lea rcx,[r14+0E78h] "mPhysicalDamagePercentageModifier"
    constexpr auto MagicDmgPercent  = 0x0EA0;       // [IDA] lea rcx,[r14+0EA0h] "mMagicalDamagePercentageModifier"
}

// ================================================================
// HERO STATS (LeagueObfuscation<float>, 0x28 apart)
// Stat block base: obj + 0x1B88
// ================================================================
namespace HeroStats {
    constexpr auto Base                     = 0x1B88;       // [D]

    // Cooldown / Ability Haste
    constexpr auto PercentCooldownMod       = 0x1B88;       // [D] base + 0x0
    constexpr auto AbilityHaste             = 0x1BB0;       // [D] base + 0x28
    constexpr auto PercentCooldownCapMod    = 0x1BD8;       // [D] base + 0x50
    constexpr auto PassiveCdEndTime         = 0x1C00;       // [D] base + 0x78
    constexpr auto PassiveCdTotalTime       = 0x1C28;       // [D] base + 0xA0

    // Minion-specific
    constexpr auto PercentDmgToBarracksMin  = 0x1C50;       // [D] base + 0xC8
    constexpr auto FlatDmgReducBarracks     = 0x1C78;       // [D] base + 0xF0
    constexpr auto IncreasedMoveSpeedMinion = 0x1CA0;       // [D] base + 0x118

    // Physical Damage
    constexpr auto FlatPhysicalDmgMod       = 0x1CC8;       // [D] base + 0x140
    constexpr auto PercentPhysicalDmgMod    = 0x1CF0;       // [D] base + 0x168
    constexpr auto PercentBonusPhysDmgMod   = 0x1D18;       // [D] base + 0x190
    constexpr auto PercentBasePhysDmgFlat   = 0x1D40;       // [D] base + 0x1B8

    // Magic Damage
    constexpr auto FlatMagicDmgMod          = 0x1D68;       // [D] base + 0x1E0
    constexpr auto PercentMagicDmgMod       = 0x1D90;       // [D] base + 0x208
    constexpr auto FlatMagicReduction       = 0x1DB8;       // [D] base + 0x230
    constexpr auto PercentMagicReduction    = 0x1DE0;       // [D] base + 0x258

    // Cast Range
    constexpr auto FlatCastRangeMod         = 0x1E08;       // [D] base + 0x280

    // Attack Speed
    constexpr auto AttackSpeedMod           = 0x1E30;       // [D] base + 0x2A8
    constexpr auto PercentAttackSpeedMod    = 0x1E58;       // [D] base + 0x2D0
    constexpr auto PercentMultiAtkSpeedMod  = 0x1E80;       // [D] base + 0x2F8

    // Healing
    constexpr auto PercentHealingAmountMod  = 0x1EA8;       // [D] base + 0x320

    // Attack Damage
    constexpr auto BaseAttackDamage         = 0x1ED0;       // [D] base + 0x348
    constexpr auto BaseAtkDmgSansScale      = 0x1EF8;       // [D] base + 0x370
    constexpr auto FlatBaseAtkDmgMod        = 0x1F20;       // [D] base + 0x398
    constexpr auto PercentBaseAtkDmgMod     = 0x1F48;       // [D] base + 0x3C0

    // Ability Power
    constexpr auto BaseAbilityDamage        = 0x1F70;       // [D] base + 0x3E8

    // Crit
    constexpr auto CritDamageMultiplier     = 0x1F98;       // [D] base + 0x410
    constexpr auto ScaleSkinCoef            = 0x1FC0;       // [D] base + 0x438
    constexpr auto Dodge                    = 0x1FE8;       // [D] base + 0x460
    constexpr auto Crit                     = 0x2010;       // [D] base + 0x488

    // Base HP Pool
    constexpr auto FlatBaseHPPoolMod        = 0x2038;       // [D] base + 0x4B0

    // Armor & MR
    constexpr auto Armor                    = 0x2060;       // [D] base + 0x4D8
    constexpr auto BonusArmor               = 0x2088;       // [D] base + 0x500
    constexpr auto SpellBlock               = 0x20B0;       // [D] base + 0x528  (MR)
    constexpr auto BonusSpellBlock          = 0x20D8;       // [D] base + 0x550

    // HP Regen
    constexpr auto HPRegenRate              = 0x2100;       // [D] base + 0x578
    constexpr auto BaseHPRegenRate          = 0x2128;       // [D] base + 0x5A0

    // Movement
    constexpr auto MoveSpeed                = 0x2150;       // [D] base + 0x5C8
    constexpr auto MoveSpeedBaseIncrease    = 0x2178;       // [D] base + 0x5F0
    constexpr auto AttackRange              = 0x21A0;       // [D] base + 0x618

    // Bubble Radius
    constexpr auto FlatBubbleRadiusMod      = 0x21C8;       // [D] base + 0x640
    constexpr auto PercentBubbleRadiusMod   = 0x21F0;       // [D] base + 0x668

    // Armor Penetration
    constexpr auto FlatArmorPen             = 0x2218;       // [D] base + 0x690
    constexpr auto PhysicalLethality        = 0x2240;       // [D] base + 0x6B8
    constexpr auto PercentArmorPen          = 0x2268;       // [D] base + 0x6E0
    constexpr auto PercentBonusArmorPen     = 0x2290;       // [D] base + 0x708
    constexpr auto PercentCritBonusArmorPen = 0x22B8;       // [D] base + 0x730
    constexpr auto PercentCritTotalArmorPen = 0x22E0;       // [D] base + 0x758

    // Magic Penetration
    constexpr auto FlatMagicPen             = 0x2308;       // [D] base + 0x780
    constexpr auto MagicLethality           = 0x2330;       // [D] base + 0x7A8
    constexpr auto PercentMagicPen          = 0x2358;       // [D] base + 0x7D0
    constexpr auto PercentBonusMagicPen     = 0x2380;       // [D] base + 0x7F8

    // Lifesteal / Vamp
    constexpr auto PercentLifeSteal         = 0x23A8;       // [D] base + 0x820
    constexpr auto PercentSpellVamp         = 0x23D0;       // [D] base + 0x848
    constexpr auto PercentOmnivamp          = 0x23F8;       // [D] base + 0x870
    constexpr auto PercentPhysicalVamp      = 0x2420;       // [D] base + 0x898

    // Pathing
    constexpr auto PathfindingRadiusMod     = 0x2448;       // [D] base + 0x8C0

    // Misc
    constexpr auto PercentCCReduction       = 0x2470;       // [D] base + 0x8E8
    constexpr auto PercentEXPBonus          = 0x2498;       // [D] base + 0x910

    // Base Armor/MR Flat Mods
    constexpr auto FlatBaseArmorMod         = 0x24C0;       // [D] base + 0x938
    constexpr auto FlatBaseSpellBlockMod    = 0x24E8;       // [D] base + 0x960

    // Resource Regen
    constexpr auto PARRegenRate             = 0x2510;       // [D] base + 0x988
    constexpr auto PrimaryARBaseRegenRate   = 0x2538;       // [D] base + 0x9B0
    constexpr auto SecondaryARRegenRate     = 0x2560;       // [D] base + 0x9D8
    constexpr auto SecondaryARBaseRegenRate = 0x2588;       // [D] base + 0xA00

    // Base Attack Speed
    constexpr auto FlatBaseAttackSpeedMod   = 0x25B0;       // [D] base + 0xA28
}

// ================================================================
// HERO-SPECIFIC
// ================================================================
namespace Hero {
    constexpr auto Gold                 = 0x2830;   // [D]
    constexpr auto GoldTotal            = 0x2858;   // [D]
    constexpr auto MinimumGold          = 0x2880;   // [D]
    constexpr auto FollowerTargetDelay  = 0x2DB8;   // [D] minion follower delay
    constexpr auto CombatType           = 0x2C98;   // [IDA] lea rdi,[r14+2C98h] "mCombatType"
    constexpr auto Exp                  = 0x4CF0;   // [D]
    constexpr auto LevelRef             = 0x4D18;   // [IDA] lea rcx,[r14+4D18h] "mLevelRef"
    constexpr auto VisionScore          = 0x55E0;   // [D]
    constexpr auto ShutdownValue        = 0x5608;   // [D]
    constexpr auto BaseGoldOnDeath      = 0x5630;   // [D]
    constexpr auto NeutralMinionsKilled = 0x5658;   // [IDA] lea rcx,[r14+5658h] "mNumNeutralMinionsKilled"
}

// ================================================================
// LIFETIME PROPS
// ================================================================
namespace Lifetime {
    constexpr auto Lifetime         = 0x0DB0;       // [IDA] lea rcx,[r14+0DB0h] "mLifetime"
    constexpr auto MaxLifetime      = 0x0DD8;       // [IDA] lea rcx,[r14+0DD8h] "mMaxLifetime"
    constexpr auto LifetimeTicks    = 0x0E00;       // [IDA] lea rcx,[r14+0E00h] "mLifetimeTicks"
}

// ================================================================
// SPELLBOOK & SPELL SLOTS
// ================================================================
namespace SpellBook {
    constexpr auto Offset           = 0x30E8;       // [D]
    constexpr auto SpellSlotArray   = 0xAE0;        // [D]
    constexpr auto ActiveSpellCast  = 0x3120;       // SpellBook::Offset + 0x38

    // SpellSlot (SpellDataInst)
    constexpr auto SlotLevel        = 0x28;         // [S]
    constexpr auto SlotCooldown     = 0x30;         // [S]
    constexpr auto SlotStacks       = 0x5C;         // [S]
    constexpr auto SlotTotalCd      = 0x74;         // [S]
    constexpr auto SlotSpellInput   = 0x120;        // [IDA] SpellInput/TargetClient (was 0x128 [D] - LOLDumper mislabeled)
    constexpr auto SlotSpellInfo    = 0x128;        // [IDA] SpellInfo ptr (was 0x130 [S] - confirmed via cast test)

    // SpellInput
    constexpr auto InputTargetNetId = 0x14;         // [S]
    constexpr auto InputStartPos    = 0x18;         // [S]
    constexpr auto InputEndPos      = 0x24;         // [S]

    // SpellInfo
    constexpr auto InfoSpellData    = 0x60;         // [S]

    // SpellData
    constexpr auto DataSpellName    = 0x80;         // [S]
    constexpr auto DataManaCost     = 0x5F4;        // [S]
    constexpr auto DataResource     = 0x8;          // [D]
}

// ================================================================
// BUFF MANAGER
// ================================================================
namespace BuffManager {
    constexpr auto Offset           = 0x28B8;       // [D]
    constexpr auto EntriesEnd       = 0x10;         // [S]
    constexpr auto EntryBuff        = 0x10;         // [S]
    constexpr auto BuffType         = 0x0C;         // [S]
    constexpr auto BuffNamePtr      = 0x10;         // [S]
    constexpr auto BuffNameStr      = 0x8;          // [S]
    constexpr auto BuffStartTime    = 0x18;         // [S]
    constexpr auto BuffEndTime      = 0x1C;         // [S]
    constexpr auto BuffStacksAlt    = 0x38;         // [S]
    constexpr auto BuffStacks       = 0x78;         // [S]
}

// ================================================================
// AI MANAGER (Navigation / Pathing)
// ================================================================
namespace AiManager {
    constexpr auto Offset           = 0x4038;       // [D]
    constexpr auto NavPathPtr       = 0x30;         // [S]
    constexpr auto InnerManager     = 0x10;         // [S]
    constexpr auto StartPath        = 0x88;         // [D]
    constexpr auto RefCount         = 0x1F0;        // [S]
    constexpr auto Velocity         = 0x318;        // [S]
    constexpr auto IsMoving         = 0x31C;        // [S]
    constexpr auto CurrentSegment   = 0x320;        // [S]
    constexpr auto PathStart        = 0x330;        // [S]
    constexpr auto PathEnd          = 0x33C;        // [S]
    constexpr auto Segments         = 0x348;        // [S]
    constexpr auto SegmentsCount    = 0x350;        // [S]
    constexpr auto DashSpeed        = 0x360;        // [S]
    constexpr auto IsDashing        = 0x384;        // [S]
    constexpr auto ServerPos        = 0x474;        // [S]
    constexpr auto MoveVec3         = 0x480;        // [S]
}

// ================================================================
// HUD INSTANCE
// ================================================================
namespace Hud {
    constexpr auto Camera           = 0x18;         // [S]
    constexpr auto Input            = 0x28;         // [D] oHudMouse
    constexpr auto UserData         = 0x60;         // [S]
    constexpr auto SpellInfo        = 0x68;         // [D] oHudSpell

    // Camera / Zoom
    constexpr auto CameraZoom       = 0x324;        // [IDA] HudCamera + zoom offset
    constexpr auto CameraZoomLimits = 0x310;        // [IDA] ptr to zoom limits struct
    constexpr auto ZoomLimitsMin    = 0x24;         // [IDA] float min zoom in limits struct
    constexpr auto ZoomLimitsMax    = 0x28;         // [IDA] float max zoom in limits struct
    constexpr auto AltZoomLimits    = 0x3D0;        // [IDA] alternate zoom limits
    constexpr auto ZoomLockFlag1    = 0x344;        // [IDA] byte flag zoom lock 1
    constexpr auto ZoomLockFlag2    = 0x345;        // [IDA] byte flag zoom lock 2

    // Input / Cursor
    constexpr auto MouseWorldPos    = 0x34;         // [IDA] HudInput + mouse world pos

    // User Data
    constexpr auto SelectedObjNetId = 0x28;         // [S]

    // Chat
    constexpr auto ChatOpen         = 0x649;        // [IDA] byte flag chat open state

    // Viewport W2S
    constexpr auto ViewportW2S      = 0x2B0;        // [IDA] viewport W2S matrix offset
}

// ================================================================
// MISSILE OBJECT
// ================================================================
namespace Missile {
    constexpr auto SpellCastPtr     = 0x8;          // [S]
    constexpr auto CastInfoBase     = 0x1C0;        // [D] changed! was 0x2C0
    constexpr auto SpellDataInst    = 0x1C0;        // [D] first QWORD
    constexpr auto SpellName        = 0x1E0;        // [D] CastInfo+0x20 relative to new base
    constexpr auto MissileName      = 0x208;        // [D] CastInfo+0x48
    constexpr auto StartPos         = 0x230;        // [D] CastInfo+0x70
    constexpr auto EndPos           = 0x23C;        // [D] CastInfo+0x7C
    constexpr auto CastEndPos       = 0x24C;        // [D] CastInfo+0x8C
    constexpr auto CasterNetId      = 0x258;        // [D] CastInfo+0x98
    constexpr auto NetworkId        = 0x264;        // [D] CastInfo+0xA4
    constexpr auto Position         = 0x25C;        // [S] inherited vec3 position
}

// ================================================================
// BASIC ATTACK / MISC
// ================================================================
namespace BasicAttack {
    constexpr auto Base             = 0x2C68;       // [D]
    constexpr auto Offset1          = 0x2C0;        // [D]
    constexpr auto Offset2          = 0x70;         // [D]
}

namespace Minion {
    constexpr auto LaneArray        = 0x68;         // [D]
    constexpr auto LaneType         = 0x4C79;       // [D]
}

// ================================================================
// NAV GRID
// ================================================================
namespace NavGrid {
    constexpr auto NavGridMgr       = 0x8;          // [S]
    constexpr auto MinX             = 0x30;         // [D] changed! was 0xEC
    constexpr auto MinZ             = 0x38;         // [D] estimated
    constexpr auto Data             = 0x150;        // [S]
    constexpr auto Width            = 0x708;        // [S]
    constexpr auto Height           = 0x70C;        // [S]
    constexpr auto Scale            = 0x714;        // [S]
}

// ================================================================
// MANAGER LIST
// ================================================================
namespace ManagerList {
    constexpr auto Items            = 0x8;          // [S]
    constexpr auto Size             = 0x10;         // [S]
}

// ================================================================
// MINIMAP
// ================================================================
namespace Minimap {
    constexpr auto MinimapParent    = 0x1D7A3D0;    // [CE] global ptr (same as NetInstance)
    constexpr auto MinimapHud       = 0x3B8;         // [CE] MinimapParent->+0x3B8 (was 0x288 in 14.23)
    constexpr auto HudVisible       = 0xD8;          // [CE] MinimapHud+0xD8 byte flag
}

// ================================================================
// EXTRA GLOBALS
// ================================================================
namespace Extra {
    constexpr auto TurretManager    = 0x1D87068;    // [P]
    constexpr auto ViewMatrixInst   = 0x1E2C030;    // [P]
    constexpr auto IsClone          = 0x2BB2A0;     // [P] function RVA (from dump binary 0x202D000)
}

// ================================================================
// VTABLES
// ================================================================
namespace VTable {
    constexpr auto AIMinionClient   = 0x18DD7F0;    // [P]
}

// ================================================================
// JUNGLE MONSTER NAME STRINGS
// These are string addresses in the binary - version specific!
// Found via IDA MCP find_regex on binary 0x2342000
// NOTE: These are for the IDA binary, NOT the dump binary!
//       For dump binary (0x202D000), re-scan needed.
// ================================================================
namespace JungleNames {
    // IDA binary (0x2342000) string addresses:
    constexpr auto SRU_RiftHerald   = 0x18d5358;    // [IDA] "SRU_RiftHerald"
    constexpr auto SRU_Horde        = 0x18d6690;    // [IDA] "SRU_Horde"
    constexpr auto SRU_Dragon       = 0x18d66B0;    // [IDA] "SRU_Dragon"
    constexpr auto SRU_Dragon_Elder = 0x18d66C0;    // [IDA] "SRU_Dragon_Elder"
    constexpr auto SRU_Baron        = 0x18e58D0;    // [IDA] "SRU_Baron"
}

} // namespace Offset
