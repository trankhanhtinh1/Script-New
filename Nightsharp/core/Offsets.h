#pragma once
#include <cstdint>

// ================================================================
// League of Legends - Offsets
// Updated: 2026-03-18 (LOLDumper v6.0 + IDA MCP v2)
// Binary: League of Legends.exe
// Module size: 0x205A000 (LOLDumper dump)
// Base: 0x0 (relative offsets from module base)
//
// Sources:
//   [D]   = LOLDumper_full.h v6.0 (pattern-scanned)
//   [P]   = offsetplugin.hpp (ida_lol_plugin.dll output)
//   [IDA] = IDA Pro MCP verified (decompile/disasm confirmed)
//   [CE]  = Cheat Engine verified at runtime
//   [S]   = struct offsets (unchanged between versions)
//   [C]   = chimera_structures.h reference (needs CE verify)
//   [RP]  = RegisterProperty (runtime API scanned)
//
// MAJOR UPDATE from 2026-03-12 hotfix:
//   - Module size changed: 0x202D000 -> 0x205A000
//   - All globals shifted significantly
//   - All function RVAs shifted significantly
//   - Some struct offsets changed (AiManager, Targetable, Missile, etc.)
//   - HeroStats struct offsets STABLE (RegisterProperty-based)
// ================================================================

namespace Offset {

// ================================================================
// GLOBAL POINTERS / INSTANCES
// ================================================================
namespace Global {
    constexpr auto LocalPlayer      = 0x1DD33B0;   // [D] local player ptr
    constexpr auto HeroManager      = 0x1DA14E0;   // [D] hero list ptr
    constexpr auto GameTime         = 0x1DAF720;   // [D] game time float
    constexpr auto MissileManager   = 0x1DA5270;   // [D] missile manager ptr
    constexpr auto NavGrid          = 0x1DA51E0;   // [D] navigation grid ptr
    constexpr auto HudInstance      = 0x1DA1628;   // [D] HUD instance ptr
    constexpr auto UnderMouseObj    = 0x19ECD78;   // [P] object under mouse cursor
    constexpr auto ViewPort         = 0x1DB4398;   // [D] viewport ptr
    constexpr auto ObjectManager    = 0x1DA1488;   // [P] object manager instance
    constexpr auto MinionManager    = 0x1DA14D8;   // [IDA] minion+jungle list (HeroManager + 8)
    constexpr auto NetInstance      = 0x1DA1480;   // [P] net instance
    constexpr auto CursorInstance   = 0x1E2DC38;   // [P] cursor position (Vec3)
    constexpr auto MouseScreenVec2  = 0x1DA5218;   // [D] mouse 2D screen position
    constexpr auto ChatClient       = 0x1DB43E0;   // [IDA] fallback, needs verification
    constexpr auto ChatInstance     = 0x1DA5480;   // [IDA] fallback
    constexpr auto r3dRenderer      = 0x1E690D8;   // [D] renderer instance (oViewPort2)
    constexpr auto ViewPort2        = 0x1E690D8;   // [D] viewport2/renderer
    constexpr auto MySpellState     = 0x1DA7FC8;   // [D] spell state global
    constexpr auto TurretManager    = 0x1DAE248;   // [P] turret list — needs CE verify
    constexpr auto ShopInstance     = 0x1DB43F8;   // [IDA] fallback
    constexpr auto OpenWindowsArray = 0x1E66E78;   // [IDA] fallback
    constexpr auto OpenWindowsCount = 0x1E66E80;   // [IDA] fallback
}

// ================================================================
// FLAGS
// ================================================================
namespace Flag {
    constexpr auto IssueOrderFlag   = 0x1D04FA8;   // [D] dword in IssueOrder
    constexpr auto IssueOrder       = IssueOrderFlag; // Backward-compatible alias
    constexpr auto CastSpellFlag    = 0x1D04F40;   // [D] byte in CastSpellSafe
    constexpr auto CastSpell        = CastSpellFlag; // Backward-compatible alias
}

// ================================================================
// FUNCTIONS (RVAs) — UPDATED 2026-03-18 (LOLDumper v6.0)
// ================================================================
namespace Function {
    // Core
    constexpr auto IssueOrderCore       = 0x2A5040;     // [P] IssueOrder
    constexpr auto IssueOrder           = IssueOrderCore;
    constexpr auto WorldToScreen        = 0x1260DC0;    // [P]
    constexpr auto CastSpellSafe        = 0xBB8950;     // [P] CastSpellSafe
    constexpr auto PrintChat            = 0x10B11B0;    // [P]
    constexpr auto GetBoundingRadius    = 0x28A600;     // [P]
    constexpr auto GetAttackDelay       = 0x53A3C0;     // [P]
    constexpr auto GetAttackWindup      = 0x53A2C0;     // [P] GetAttackCastDelay
    constexpr auto GetCollisionFlags    = 0x11B29D0;    // [D]
    constexpr auto GetPing              = 0x677420;     // [P]

    // Object Iteration
    constexpr auto GetFirstObject       = 0x9C39B0;     // [P]
    constexpr auto GetFirstObjectAlt    = 0x9C39B0;     // [P] same as GetFirstObject
    constexpr auto GetNextObject        = 0x523760;     // [P]
    constexpr auto FindObject           = 0x522530;     // [P]
    constexpr auto GetAiManager         = 0x292420;     // [IDA] VERIFIED: lea rdx,[rcx+41F0h] — decrypts AiManager ptr (was 0x51BA90 = wrong, binary search)
    constexpr auto GetAiManagerInner    = 0x293A10;     // [IDA] returns *(AiMgr+0x10) = InnerManager

    // Type Checks
    constexpr auto IsBuilding           = 0x30F370;     // [P]
    constexpr auto IsAlive              = 0x2ECC50;     // [P]
    constexpr auto IsDead               = 0x2A0780;     // [P]
    constexpr auto IsTargetableByUnit   = 0x2A3800;     // [P+IDA] fixed: 0x2A3680 was vtable entry, actual func at 0x2A3800
    constexpr auto IsVulnerable         = 0x2A1440;     // [P]
    constexpr auto IsJungleMonster      = 0x2A1620;     // [P+IDA] fixed: 0x2A1610 was raw byte, actual func at 0x2A1620
    constexpr auto IsDragon             = 0x2A0A30;     // [P]
    constexpr auto IsElderDragon        = 0x2A0AA0;     // [P]
    constexpr auto IsBaron              = 0x29FE90;     // [P]
    constexpr auto IsSelectable         = 0x215890;     // [P]
    constexpr auto IsFleeing            = 0x1133EC0;    // [P]
    constexpr auto IsNoRender           = 0x212AA0;     // [P]
    constexpr auto CompareTypeFlags     = 0x2A2130;     // [P]
    constexpr auto GetJungleType        = 0x67ADE0;     // [P]

    // Attack / Combat
    constexpr auto CanAttack            = 0x1FC3F0;     // [P]
    constexpr auto GetSpellCastInfo     = 0x288D50;     // [P]
    constexpr auto GetSpellSlot         = 0x905BC0;     // [P]
    constexpr auto GetResourceType      = 0x286070;     // [P]
    constexpr auto HasBuffOfType        = 0x29B610;     // [P]
    constexpr auto GetGoldRedirectTgt   = 0x203120;     // [P]

    // Level Up
    constexpr auto LevelSpell           = 0x0;          // [TODO] need pattern scan

    // Map / Minimap
    constexpr auto GetMapID             = 0x2933B0;     // [D]

    // Hooks / Callbacks
    constexpr auto CreateClientEffect   = 0x83C170;     // [P]
    constexpr auto OnCreateObject       = 0x527930;     // [P]
    constexpr auto OnGameUpdate         = 0x5215E0;     // [P]
    constexpr auto OnProcessSpell       = 0x91D1B0;     // [P]
    constexpr auto OnSpellImpact        = 0x914320;     // [P]
    constexpr auto OnStopCast           = 0x91D750;     // [P+IDA] fixed: 0x91D4C0 was raw byte (db 44h), actual func at 0x91D750
    constexpr auto OnFinishCast         = 0x2CBE30;     // [P]
    constexpr auto OnBuffAdd            = 0xBD0B40;     // [P]

}

// ================================================================
// GAME OBJECT STRUCT  (stable - struct offsets don't change)
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
    constexpr auto Direction        = 0x21D8;       // [C] facing direction Vec3
    constexpr auto ItemList         = 0x4D20;       // [C] array of 7 ItemSlot ptrs
}

// ================================================================
// MANA (RegisterProperty based)
// ================================================================
namespace Mana {
    constexpr auto MP               = 0x360;        // [RP] mMP
    constexpr auto MaxMP            = 0x388;        // [RP] mMaxMP
    constexpr auto PAR              = 0xE00;        // [RP] mPAR (primary ability resource)
    constexpr auto SAR              = 0x108;        // [RP] mSAR (secondary ability resource)
    constexpr auto MaxSAR           = 0x130;        // [RP] mMaxSAR
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
    constexpr auto InHealAllied     = 0x11C0;       // [IDA] HP+0x140
    constexpr auto InHealEnemy      = 0x11E8;       // [IDA] HP+0x168
    constexpr auto InDamage         = 0x1210;       // [IDA] HP+0x190
    constexpr auto StopShieldFade   = 0x1238;       // [IDA] HP+0x1B8
    // RP-relative offsets (from HP base 0x1080)
    constexpr auto RP_AllShield        = 0xA0;      // [RP] HP+0xA0
    constexpr auto RP_PhysicalShield   = 0xC8;      // [RP] HP+0xC8
    constexpr auto RP_MagicalShield    = 0xF0;      // [RP] HP+0xF0
    constexpr auto RP_ChampSpecific    = 0x118;     // [RP] HP+0x118
    constexpr auto RP_InHealAllied     = 0x140;     // [RP] HP+0x140
    constexpr auto RP_InHealEnemy      = 0x168;     // [RP] HP+0x168
    constexpr auto RP_InDamage         = 0x190;     // [RP] HP+0x190
}

// ================================================================
// TARGETABLE
// ================================================================
namespace Targetable {
    constexpr auto IsTargetable     = 0xED0;        // [IDA] VERIFIED: add rcx, 0ED0h in sub_26D3E0 "mIsTargetable"
    constexpr auto TargetableFlags  = 0xEF8;        // [IDA] mIsTargetableToTeamFlags
}

// ================================================================
// ACTION STATE
// ================================================================
namespace ActionState {
    constexpr auto State1           = 0x1470;       // [IDA] lea rdx,[rsi+1470h] -> sub_200C20 "ActionState"
    constexpr auto State2           = 0x14A8;       // [IDA] 0x1470+0x38 -> "ActionState2"
}

// ================================================================
// DAMAGE MODIFIERS
// ================================================================
namespace DamageModifier {
    constexpr auto PhysDmgPercent   = 0x0E78;       // [RP] mPhysicalDamagePercentageModifier
    constexpr auto MagicDmgPercent  = 0x0EA0;       // [RP] mMagicalDamagePercentageModifier
}

// ================================================================
// HERO STATS (LeagueObfuscation<float>, 0x28 apart)
// Stat block base: obj + 0x1B88 (STABLE via RegisterProperty)
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
    constexpr auto Armor                    = 0x2060;       // [D] base + 0x4D8  (TOTAL armor)
    constexpr auto BonusArmor               = 0x2088;       // [RP] base + 0x500
    constexpr auto SpellBlock               = 0x20B0;       // [D] base + 0x528  (MR, total)
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
    constexpr auto PrimaryARBaseRegenRate   = 0x2538;       // [RP] base + 0x9B0
    constexpr auto SecondaryARRegenRate     = 0x2560;       // [RP] base + 0x9D8
    constexpr auto SecondaryARBaseRegenRate = 0x2588;       // [RP] base + 0xA00

    // Base Attack Speed
    constexpr auto FlatBaseAttackSpeedMod   = 0x25B0;       // [D] base + 0xA28
}

// ================================================================
// HERO-SPECIFIC
// ================================================================
namespace Hero {
    constexpr auto Gold                 = 0x2830;   // [RP]
    constexpr auto GoldTotal            = 0x2858;   // [RP]
    constexpr auto MinimumGold          = 0x2880;   // [RP]
    constexpr auto FollowerTargetDelay  = 0x2DB8;   // [RP]
    constexpr auto CombatType           = 0x2C98;   // [IDA] — needs verify (LOLDumper says 0x5608)
    constexpr auto Exp                  = 0x4CF0;   // [RP]
    constexpr auto LevelRef             = 0x4D18;   // [S]
    constexpr auto LevelUpPoints        = 0x4D78;   // [chimera] LevelRef + 0x60
    constexpr auto VisionScore          = 0x55E0;   // [RP]
    constexpr auto ShutdownValue        = 0x5608;   // [RP]
    constexpr auto BaseGoldOnDeath      = 0x5630;   // [RP]
    constexpr auto NeutralMinionsKilled = 0x5658;   // [IDA]
}

// ================================================================
// LIFETIME PROPS
// ================================================================
namespace Lifetime {
    constexpr auto Lifetime         = 0x0DB0;       // [RP]
    constexpr auto MaxLifetime      = 0x0DD8;       // [RP]
    constexpr auto LifetimeTicks    = 0x0E00;       // [RP]
}

// ================================================================
// SPELLBOOK & SPELL SLOTS
// ================================================================
namespace SpellBook {
    constexpr auto Offset           = 0x30E8;       // [D]
    constexpr auto SpellSlotArray   = 0xAE0;        // [D]
    constexpr auto ActiveSpellCast  = 0x3120;       // [D] SpellBook::Offset + 0x38

    // SpellSlot (SpellDataInst)
    constexpr auto SlotLevel        = 0x28;         // [S]
    constexpr auto SlotCooldown     = 0x30;         // [S]
    constexpr auto SlotStacks       = 0x5C;         // [S]
    constexpr auto SlotTotalCd      = 0x74;         // [S]
    constexpr auto SlotSpellInput   = 0x120;        // [IDA] SpellInput/TargetClient
    constexpr auto SlotSpellInfo    = 0x128;        // [IDA] SpellInfo ptr

    // SpellInput
    constexpr auto InputTargetNetId = 0x14;         // [S]
    constexpr auto InputStartPos    = 0x18;         // [S]
    constexpr auto InputEndPos      = 0x24;         // [S]

    // SpellInfo
    constexpr auto InfoSpellData    = 0x60;         // [S]

    // SpellData
    constexpr auto DataSpellName    = 0x80;         // [S]
    constexpr auto SpellInfoNamePtr = 0x28;         // [brute confirmed]
    constexpr auto DataManaCost     = 0x5F4;        // [S]
    constexpr auto DataResource     = 0x8;          // [D]

    // SpellData -> SpellDataResource (SpellData + 0x60)
    constexpr auto DataResourceBase = 0x60;         // [IDA]
    constexpr auto ResCastRange     = 0x478;        // [C]
    constexpr auto ResMissileSpeed  = 0x518;        // [C]
    constexpr auto ResLineWidth     = 0x568;        // [C]
    constexpr auto ResMaxAmmo       = 0x3C0;        // [C]
    constexpr auto ResCastType      = 0x510;        // [C]
    constexpr auto ResMissileSpec   = 0x508;        // [C]
    constexpr auto ResScriptName    = 0x80;         // [C]
    constexpr auto ResCooldownTime  = 0x304;        // [C]
    constexpr auto ResAmmoRecharge  = 0x408;        // [C]
    constexpr auto ResImgIconName   = 0x2A0;        // [C]
}

// ================================================================
// BUFF MANAGER
// ================================================================
namespace BuffManager {
    constexpr auto Offset           = 0x28B8;       // [D] reverted to backup value (was 0x29C8 — CE may have been wrong)
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
// AI MANAGER (Navigation / Pathing) — ALL VERIFIED 2026-03-18
// ================================================================
namespace AiManager {
    // --- Encrypted pointer resolution ---
    constexpr auto Offset           = 0x41F0;       // [IDA+CE] obj+0x41F0 → LeagueObfuscation block → decrypt → raw ptr
    constexpr auto InnerManager     = 0x10;         // [IDA+CE] *(raw_ptr + 0x10) → actual AiManager struct

    // --- Navigation target ---
    constexpr auto TargetPosition   = 0x034;        // [CE] Vec3: click/move target position

    // --- Movement state ---
    constexpr auto Velocity         = 0x318;        // [CE] float: current move speed (e.g. 717.5)
    constexpr auto IsMoving         = 0x31C;        // [CE] bool: 1 when unit is moving
    constexpr auto CurrentSegment   = 0x320;        // [CE] int: current path segment index

    // --- Path data ---
    constexpr auto PathStart        = 0x330;        // [CE] Vec3: path start position
    constexpr auto PathEnd          = 0x33C;        // [CE] Vec3: path end/destination
    constexpr auto Segments         = 0x348;        // [CE] ptr → Vec3[]: waypoint array
    constexpr auto NavArray         = 0x348;        // [CE] alias for Segments
    constexpr auto SegmentsCount    = 0x350;        // [CE] int: number of path segments
    constexpr auto HasPath          = 0x354;        // [CE] int: non-zero if path exists

    // --- Dash state ---
    constexpr auto DashSpeed        = 0x360;        // [CE] float: dash speed (0.0 when not dashing)
    constexpr auto IsDashing        = 0x384;        // [CE] bool: 1 when dashing

    // --- Additional positions ---
    constexpr auto TargetPos2       = 0x3A8;        // [CE] Vec3: secondary target position
    constexpr auto ServerPos        = 0x474;        // [CE] Vec3: server-authoritative position
    constexpr auto MoveVec3         = 0x480;        // [CE] Vec3: movement direction vector (zero when still)
}

// ================================================================
// HUD INSTANCE
// ================================================================
namespace Hud {
    constexpr auto Camera           = 0x18;         // [S]
    constexpr auto Input            = 0x28;         // [D]
    constexpr auto UserData         = 0x60;         // [S]
    constexpr auto SpellInfo        = 0x68;         // [D]

    // Camera / Zoom
    constexpr auto CameraZoom       = 0x324;        // [IDA]
    constexpr auto CameraZoomLimits = 0x310;        // [IDA]
    constexpr auto ZoomLimitsMin    = 0x24;         // [IDA]
    constexpr auto ZoomLimitsMax    = 0x28;         // [IDA]
    constexpr auto AltZoomLimits    = 0x3D0;        // [IDA]
    constexpr auto ZoomLockFlag1    = 0x344;        // [IDA]
    constexpr auto ZoomLockFlag2    = 0x345;        // [IDA]

    // Input / Cursor
    constexpr auto MouseWorldPos    = 0x34;         // [IDA]

    // User Data
    constexpr auto SelectedObjNetId = 0x28;         // [S]

    // Chat
    constexpr auto ChatOpen         = 0x10;         // [IDA]

    // Viewport W2S
    constexpr auto ViewportW2S      = 0x2B0;        // [IDA]
}

// ================================================================
// MISSILE OBJECT — CastInfoBase CHANGED!
// ================================================================
namespace Missile {
    // --- Missile Object (absolute offsets from missile base) ---
    constexpr auto SpellDataPtr     = 0x128;        // [S]
    constexpr auto Position         = 0x25C;        // [S]
    constexpr auto CastInfoBase     = 0x2C0;        // [IDA] VERIFIED: CastInfo struct INLINE at missile+0x2C0

    // --- CastInfo fields — ABSOLUTE offsets from missile base (0x2C0 + CI_*) ---
    constexpr auto CI_SpellData     = 0x2C0;        // QWORD: SpellData ptr (CastInfo+0x00)
    constexpr auto SpellName        = 0x2E0;        // std::string SSO: spell name (CI+0x20)
    constexpr auto MissileName      = 0x308;        // std::string SSO: missile name (CI+0x48)
    constexpr auto StartPos         = 0x388;        // Vec3: start position (CI+0xC8)
    constexpr auto EndPos           = 0x394;        // Vec3: end position (CI+0xD4)
    constexpr auto CastEndPos       = 0x3A4;        // Vec3: cast end position (CI+0xE4)
    constexpr auto CasterNetId      = 0x358;        // int: source caster net id (CI+0x98)
    constexpr auto SrcIndex         = 0x358;        // alias
    constexpr auto TargetNetId      = 0x35C;        // int: target net id (CI+0x9C)
    constexpr auto CI_TargetNetId2  = 0x360;        // int: secondary target (CI+0xA0)
    constexpr auto MissileNetId     = 0x364;        // int: missile net id (CI+0xA4)
    constexpr auto CI_MissileNetId  = 0x364;        // alias
    constexpr auto DestIndex        = 0x3C8;        // [dest ptr] -> target dest index (CI+0x108)

    // --- CastInfo relative offsets (CI base + offset) ---
    constexpr auto CI_REL_SpellData    = 0x00;
    constexpr auto CI_REL_SpellName    = 0x20;
    constexpr auto CI_REL_MissileName  = 0x48;
    constexpr auto CI_REL_StartPos     = 0xC8;
    constexpr auto CI_REL_EndPos       = 0xD4;
    constexpr auto CI_REL_CastEndPos   = 0xE4;
    constexpr auto CI_REL_CasterNetId  = 0x98;
    constexpr auto CI_REL_MissileNetId = 0xA4;
    constexpr auto CI_REL_TargetIndex  = 0x108;

    // --- Legacy aliases ---
    constexpr auto NetworkId        = MissileNetId;
    constexpr auto SpellDataInst    = CI_SpellData;
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
    constexpr auto LaneCount        = 0x70;         // [D]
    constexpr auto LaneType         = 0x4CC9;       // [CE] byte on obj: 4=Melee, 5=Ranged, 6=Cannon, 7=Super
}

// ================================================================
// NAV GRID
// ================================================================
namespace NavGrid {
    constexpr auto NavGridMgr       = 0x8;          // [S]
    constexpr auto MinX             = 0xEC;         // [S]
    constexpr auto MinZ             = 0xF4;         // [S]
    constexpr auto MaxX             = 0xF8;         // [S]
    constexpr auto MaxZ             = 0x100;        // [S]
    constexpr auto Data             = 0x110;        // [S]
    constexpr auto Width            = 0x708;        // [S]
    constexpr auto Height           = 0x70C;        // [S]
    constexpr auto InverseScale     = 0x714;        // [S]
    constexpr auto Scale            = 0x710;        // [S]
    constexpr auto GrassRegions     = 0x158;        // [S]
    constexpr auto CellSize         = 16;           // [IDA]
    constexpr uint16_t FLAG_WALL    = 0x0001;       // [IDA]
    constexpr uint16_t FLAG_NOWALK  = 0x0002;       // [IDA]
    constexpr uint16_t FLAG_BRUSH   = 0x0C00;       // [IDA]
    constexpr uint16_t FLAG_SPECIAL = 0x1000;       // [IDA]
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
    constexpr auto MinimapParent    = 0x1DA1498;    // [D] same as NetInstance
    constexpr auto MinimapHud       = 0x3B8;        // [CE]
    constexpr auto HudVisible       = 0xD8;         // [CE]
}

// ================================================================
// ITEM SYSTEM — DataItemId CHANGED!
// ================================================================
namespace ItemSystem {
    constexpr auto SlotInfo         = 0x10;         // [S]
    constexpr auto InfoData         = 0x38;         // [S]
    constexpr auto InfoStacks       = 0x64;         // [S]
    constexpr auto DataItemId       = 0xB4;         // [IDA] ItemData+0xB4 -> item ID int
    constexpr auto DataAbilityHaste = 0x160;        // [C]
    constexpr auto DataHealth       = 0x164;        // [C]
    constexpr auto DataArmor        = 0x19C;        // [C]
    constexpr auto DataMR           = 0x1BC;        // [C]
    constexpr auto DataAD           = 0x1D8;        // [C]
    constexpr auto DataAP           = 0x1E0;        // [C]
    constexpr auto DataAtkSpeedMult = 0x20C;        // [C]
}

// ================================================================
// SPELL CAST INFO (Active Spell)
// ================================================================
namespace SpellCastInfo {
    constexpr auto SpellData        = 0x0;          // [IDA]
    constexpr auto SrcIndex         = 0x98;         // [S]
    constexpr auto StartPos         = 0xD8;         // [S]
    constexpr auto EndPos           = 0xE4;         // [S]
    constexpr auto CastPos          = 0xF0;         // [S]
    constexpr auto TargetIndex      = 0x108;        // [S]
    constexpr auto DestIndex        = 0x108;        // [S] alias
    constexpr auto CastDelay        = 0x118;        // [S]
    constexpr auto IsSpell          = 0x134;        // [S]
    constexpr auto IsSpecialAttack  = 0x13E;        // [S]
    constexpr auto IsAuto           = 0x141;        // [S]
    constexpr auto Slot             = 0x14C;        // [S]
}

// ================================================================
// EXTRA GLOBALS
// ================================================================
namespace Extra {
    constexpr auto TurretManager    = 0x1DAE248;    // [P] needs verify
    constexpr auto ViewMatrixInst   = 0x1E54700;    // [P] updated from plugin dump
    constexpr auto IsClone          = 0x2C15E0;     // [P] needs verify
}

// ================================================================
// VTABLES
// ================================================================
namespace VTable {
    constexpr auto AIMinionClient   = 0x18FF9E0;    // [P] updated from plugin dump
}

// ================================================================
// OBJECT TYPE FLAGS (obfuscated field at obj+0x4C)
// ================================================================
namespace TypeFlags {
    constexpr auto ObfuscatedField  = 0x4C;          // [IDA]
    constexpr auto IsObjectAI       = 0x0400;         // [IDA] AIBaseClient
    constexpr auto Minion           = 0x0800;         // [IDA] AIMinionClient
    constexpr auto Hero             = 0x1000;         // [IDA] AIHeroClient
    constexpr auto Turret           = 0x2000;         // [IDA] AITurretClient
    constexpr auto Plant            = 0x8000;         // [IDA] Plant objects
    constexpr auto Unknown_10000    = 0x10000;        // [IDA]
    constexpr auto Unknown_20000    = 0x20000;        // [IDA]
    constexpr auto Unknown_40000    = 0x40000;        // [IDA]

    // Secondary flags
    constexpr auto LargeMonster     = 0x0080;         // [IDA]
    constexpr auto BuffMonster      = 0x0100;         // [IDA]
    constexpr auto MinionSummon     = 0x0100;         // [IDA]
    constexpr auto IsFleeing        = 0x0200;         // [IDA]
    constexpr auto AttackableObj    = 0x0008;         // [IDA]
    constexpr auto VisibleObj       = 0x0010;         // [IDA]
    constexpr auto RenderTarget     = 0x0020;         // [IDA]
    constexpr auto IsRecalling      = 0x4000;         // [IDA]

    // Deprecated aliases
    constexpr auto JungleMonster    = Turret;
    constexpr auto Crab             = Turret;
    constexpr auto CampMonster      = Unknown_10000;
    constexpr auto HasUltimate      = Unknown_20000;
}

// ================================================================
// MINION CLASSIFICATION
// ================================================================
namespace MinionClass {
    constexpr auto Unset            = 0;
    constexpr auto Pet              = 1;
    constexpr auto JungleMonster    = 2;
    constexpr auto TeamMinion       = 3;
    constexpr auto MeleeLaneMinion  = 4;
    constexpr auto RangedLaneMinion = 5;
    constexpr auto SiegeLaneMinion  = 6;
    constexpr auto SuperLaneMinion  = 7;
}

// ================================================================
// JUNGLE TYPE
// ================================================================
namespace JungleType {
    constexpr auto TypeOffset       = 0x4A84;         // [IDA]
    constexpr auto Normal           = 0;
    constexpr auto Baron            = 1;
    constexpr auto Dragon           = 2;
}

// ================================================================
// DRAGON
// ================================================================
namespace Dragon {
    constexpr auto CharacterHash    = 0x68;
    constexpr auto HashTable        = 0x1DC07E0;    // Needs verify
    constexpr auto HashTableEnd     = 0x1DC0948;    // Needs verify
    constexpr auto HashEntrySize    = 0x28;
    constexpr auto HashAir          = 0x11D34E07;   // [C]
    constexpr auto HashFire         = 0x99A9F7D9;   // [C]
    constexpr auto HashWater        = 0x27F69DF4;   // [C]
    constexpr auto HashEarth        = 0x606D3187;   // [C]
    constexpr auto HashHextech      = 0xA0808ACE;   // [C]
    constexpr auto HashChemtech     = 0xF94EBA26;   // [C]
    constexpr auto HashRuined       = 0x518A146A;   // [C]
    constexpr auto HashElder        = 0x5944DC07;   // [C]
    constexpr auto HashParty        = 0x4B962AA3;   // [C]
}

// ================================================================
// PLANT IDENTIFICATION
// ================================================================
namespace PlantInfo {
    // Plants are checked via: CompareTypeFlags(obj, TypeFlags::Plant)
    // Plant types distinguished by CharacterName (obj + 0x4330):
    //   "SRU_Plant_Health"   -> Honeyfruit
    //   "SRU_Plant_Satchel"  -> Blast Cone
    //   "SRU_Plant_Vision"   -> Scryer's Bloom
}

} // namespace Offset
