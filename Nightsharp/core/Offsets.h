#pragma once
#include <cstdint>

// ================================================================
// League of Legends - Offsets
// Updated: 2026-03-05 (Hotfix) (LOLDumper v5.0 + offsetplugin.hpp + IDA MCP)
// Binary: League of Legends.exe
// Global/Function RVAs: from module size 0x202D000 (dump files)
// Struct offsets: verified via IDA on module size 0x2342000
// Base: 0x0 (relative offsets from module base)
//
// Sources:
//   [D]   = LOLDumper_full.h (pattern-scanned)
//   [P]   = offsetplugin.hpp (ida_lol_plugin.dll output)
//   [IDA] = IDA Pro MCP verified (decompile/disasm confirmed)
//   [CE]  = Cheat Engine verified at runtime
//   [S]   = struct offsets (unchanged between versions)
//   [C]   = chimera_structures.h reference (needs CE verify)
//
// Hotfix notes (2026-03-05):
//   - Function RVAs shifted ~0x30 due to hotfix code changes
//   - All globals remained STABLE (confirmed via IDA xrefs)
//   - Struct offsets STABLE (RegisterProperty-based, version-independent)
//   - IssueOrder decompile confirms: qword_1D7A3D8=ObjMgr, qword_1D7A578=HUD,
//     qword_1D7A3D0=NetInst, dword_1CDDF88=IssueOrderFlag
//   - CastSpellSafe decompile confirms: byte_1CDDF20=CastSpellFlag,
//     qword_1D7A428=MinionMgr
//   - DetectionWatcher2 for Chimera-style mainloop_check is currently
//     resolved at runtime by signature: 4C 8B 3D ? ? ? ? 4D 85 FF 0F
//   - Current packet pipeline:
//       CastSpellSafe -> CastSpellPacketA/B/Charged -> PacketSendCommon -> PacketSerializeCommon
//       IssueOrderCore -> IssueOrderPacketBuilder -> PacketSendCommon -> PacketSerializeCommon
// ================================================================

namespace Offset {

// ================================================================
// GLOBAL POINTERS / INSTANCES  (all stable across hotfix)
// ================================================================
namespace Global {
    constexpr auto LocalPlayer      = 0x1DAB720;   // [D][P] local player ptr
    constexpr auto HeroManager      = 0x1D7A430;   // [D][P] hero list ptr
    constexpr auto GameTime         = 0x1D88540;    // [D][P] game time float
    constexpr auto MissileManager   = 0x1D7DD50;   // [D] missile manager ptr
    constexpr auto NavGrid          = 0x1D7DCC8;   // [D] navigation grid ptr
    constexpr auto HudInstance      = 0x1D7A578;   // [D][P][IDA] HUD instance ptr (confirmed via IssueOrder decompile)
    constexpr auto UnderMouseObj    = 0x1D7DF50;   // [D] object under mouse cursor
    constexpr auto ViewPort         = 0x1D8D1B0;   // [D] viewport ptr
    constexpr auto ObjectManager    = 0x1D7A3D8;   // [D][P][IDA] object manager instance (confirmed via IssueOrder decompile)
    constexpr auto MinionManager    = 0x1D7A428;   // [CE][IDA] minion+jungle list (confirmed via CastSpellSafe decompile)
    constexpr auto NetInstance      = 0x1D7A3D0;   // [IDA] net instance (confirmed via IssueOrder decompile, 970 xrefs)
    constexpr auto CursorInstance   = 0x1E05698;   // [P] cursor position (Vec3)
    constexpr auto MouseScreenVec2  = 0x1D7DCF8;   // [D] mouse 2D screen position
    constexpr auto ChatClient       = 0x1D8D240;   // [IDA] chat client ptr (17 xrefs) — ChatClient+0x10 = chat open byte
    constexpr auto ChatInstance     = 0x1D7DFA0;   // [IDA] chat instance (82 xrefs confirmed)
    constexpr auto r3dRenderer      = 0x1E3FE78;   // [D] renderer instance (oViewPort2)
    constexpr auto ViewPort2        = 0x1E3FE78;   // [D] viewport2/renderer
    constexpr auto MySpellState     = 0x1D80AA0;   // [D] spell state global
    constexpr auto TurretManager    = 0x1D87068;   // [CE][P][IDA] turret list (20 xrefs confirmed)
    constexpr auto ShopInstance     = 0x1D8D258;   // [IDA] shop window instance ptr (sub_BC58F0 uses qword_1D8C258)
    constexpr auto OpenWindowsArray = 0x1E3DC58;   // [IDA] ptr to array of open HUD window ptrs (sub_129FD80)
    constexpr auto OpenWindowsCount = 0x1E3DC60;   // [IDA] dword count of open windows (dword_1E3CC60)
}

// ================================================================
// FLAGS  (stable across hotfix - confirmed via decompile)
// ================================================================
namespace Flag {
    constexpr auto IssueOrderFlag   = 0x1CDDF88;   // [D][IDA] dword_1CDDF88 in IssueOrder (Chimera: order + 17)
    constexpr auto IssueOrder       = IssueOrderFlag; // Backward-compatible alias
    constexpr auto CastSpellFlag    = 0x1CDDF20;   // [D][IDA] byte_1CDDF20 in CastSpellSafe (Chimera CastSpellFlag)
    constexpr auto CastSpell        = CastSpellFlag; // Backward-compatible alias
}

// ================================================================
// FUNCTIONS (RVAs) — UPDATED for hotfix 2026-03-05
// ================================================================
namespace Function {
    // Core
    constexpr auto IssueOrderCore       = 0x29FBE0;     // [D][P][IDA] current IssueOrder core
    constexpr auto IssueOrder           = IssueOrderCore; // Backward-compatible alias
    constexpr auto IssueOrderPacketBuilder = 0x360CA0;  // [IDA] current IssueOrder packet builder
    constexpr auto IssueOrderPacketPostSend = 0x2CE8C0; // [IDA] current IssueOrder post-send handler
    constexpr auto WorldToScreen        = 0x1241220;    // [D][P] (was 0x1241320)
    constexpr auto CastSpellWrapper     = 0x1E9A40;     // [D] (unchanged)
    constexpr auto CastSpellSafe        = 0xBB9D10;     // [P][IDA] (was 0xBB9DE0)
    constexpr auto CastSpellPacketA     = 0x91BF00;     // [IDA] normal cast packet path A
    constexpr auto CastSpellPacketB     = 0x91B6B0;     // [IDA] normal cast packet path B
    constexpr auto CastSpellPacketCharged = 0x91C7C0;   // [IDA] charged spell packet path
    constexpr auto PacketSendCommon     = 0x686930;     // [IDA] shared packet sender
    constexpr auto PacketSerializeCommon = 0x686970;    // [IDA] shared packet serializer/dispatch
    constexpr auto PrintChat            = 0xAFE1F0;     // [D] (was 0xAFE2E0)
    constexpr auto GetBoundingRadius    = 0x285610;     // [D][P] (was 0x285640)
    constexpr auto GetAttackDelay       = 0x52C560;     // [D][P] (was 0x52C620)
    constexpr auto GetAttackWindup      = 0x52C460;     // [D][P] GetAttackCastDelay (was 0x52C520)
    constexpr auto GetCollisionFlags    = 0x1195A30;    // [D] (was 0x1195B30)
    constexpr auto GetPing              = 0x669EE0;     // [D][P] (was 0x669FA0)

    // Object Iteration
    constexpr auto GetFirstObject       = 0x5128E0;     // [D] main iterator (was 0x512970)
    constexpr auto GetFirstObjectAlt    = 0x9D02B0;     // [P] alt via plugin (was 0x9D03A0)
    constexpr auto GetNextObject        = 0x5133D0;     // [D][P] (was 0x513460)
    constexpr auto FindObject           = 0x5120D0;     // [P] (was 0x512160)
    constexpr auto GetAiManager         = 0x50AAA0;     // [D] (was 0x50AB30)
    constexpr auto GetAIManagerAlt      = 0x28D3D0;     // [P] (was 0x28D400)

    // Type Checks
    constexpr auto IsTurret             = 0x3085C0;     // [D] (was 0x3085F0)
    constexpr auto IsHero               = 0x3086C0;     // [D] (was 0x3086F0)
    constexpr auto IsBuilding           = 0x3087F0;     // [P] (was 0x308820)
    constexpr auto IsAlive              = 0x2E6320;     // [D][P] (was 0x2E6350)
    constexpr auto IsDead               = 0x29B350;     // [P][IDA] decompiled, obfuscated check (was 0x29B380)
    constexpr auto IsTargetableByUnit   = 0x29E3D0;     // [P] (was 0x29E280)
    constexpr auto IsVulnerable         = 0x29C010;     // [P] (was 0x29C040)
    constexpr auto IsJungleMonster      = 0x29D5D0;     // [P] (was 0x29C210)
    constexpr auto IsDragon             = 0x29B600;     // [P] (was 0x29B630)
    constexpr auto IsElderDragon        = 0x29B670;     // [P] (was 0x29B6A0)
    constexpr auto IsBaron              = 0x29AA60;     // [P] (was 0x29AA90)
    constexpr auto IsSelectable         = 0x212140;     // [P] (was 0x212170)
    constexpr auto CompareTypeFlags     = 0x29CD00;     // [P] (was 0x29CD30)
    constexpr auto IsFleeing            = 0x20F300;     // [P] (was 0x20F330)
    constexpr auto IsNoRender           = 0x20F350;     // [P][IDA] decompiled, checks flag 0x40000 (was 0x20F380)
    constexpr auto GetJungleType        = 0x66CE30;     // [P] (was 0x66CEF0)

    // Attack / Combat
    constexpr auto CanAttack            = 0x1F90A0;     // [P] (was 0x1F90D0)
    constexpr auto GetSpellCastInfo     = 0x283EE0;     // [P] (was 0x283F10)
    constexpr auto GetSpellSlot         = 0x90A8F0;     // [P] (was 0x90A9B0)
    constexpr auto GetResourceType      = 0x281200;     // [P] (was 0x281230)
    constexpr auto HasBuffOfType        = 0x2963D0;     // [P] (was 0x296400)
    constexpr auto GetGoldRedirectTgt   = 0x1FF960;     // [P] (was 0x1FF990)

    // Level Up
    constexpr auto LevelSpell           = 0xBA39B0;     // [IDA] sub_BA39B0 — evtLevelSpell handler, edx=slot(0-3)

    // Map / Minimap
    constexpr auto GetMapID             = 0x28E2E0;     // [D] (was 0x28E310)

    // Hooks / Callbacks
    constexpr auto OnCreateObject       = 0x517DE0;     // [P] (was 0x517E70)
    constexpr auto OnGameUpdate         = 0x511180;     // [P] (was 0x511210)
    constexpr auto OnProcessSpell       = 0x920430;     // [P] (was 0x9204F0)
    constexpr auto OnSpellImpact        = 0x917B40;     // [P] (was 0x917C00)
    constexpr auto OnStopCast           = 0x9204C0;     // [P] (was 0x920800)
    constexpr auto OnFinishCast         = 0x2C5730;     // [P] (was 0x2C5760)
    constexpr auto OnBuffAdd            = 0xBCDD30;     // [P] (was 0xBCDE00)
    constexpr auto CreateClientEffect   = 0x869DB0;     // [P] (was 0x869E70)
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
    constexpr auto Direction        = 0x21D8;       // [C] facing direction Vec3 (FaceDirection_s)
    constexpr auto ItemList         = 0x4D20;       // [C] array of 7 ItemSlot ptrs (6 items + trinket)
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
    // NOTE: Armor (0x2060) is TOTAL armor — already includes base + bonus.
    //       BonusArmor removed intentionally; use Armor directly for all calcs.
    constexpr auto Armor                    = 0x2060;       // [D] base + 0x4D8  (TOTAL armor — use this)
    // BonusArmor                           = 0x2088        // REMOVED — would double-count vs. Armor total
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
    constexpr auto LevelUpPoints        = 0x4D78;   // [chimera] LevelRef + 0x60 = skill points available
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
    constexpr auto SlotSpellInput   = 0x120;        // [IDA] SpellInput/TargetClient (LOLDumper scans 0xB8 - wrong)
    constexpr auto SlotSpellInfo    = 0x128;        // [IDA] SpellInfo ptr (LOLDumper scans 0xC0 - wrong)

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

    // SpellData → SpellDataResource (SpellData + 0x60)
    constexpr auto DataResourceBase = 0x60;         // [IDA] SpellData+0x60 → SpellDataResource ptr
    constexpr auto ResCastRange     = 0x478;        // [C] array of 7 floats (per rank)
    constexpr auto ResMissileSpeed  = 0x518;        // [C] float missile speed
    constexpr auto ResLineWidth     = 0x568;        // [C] float line width
    constexpr auto ResMaxAmmo       = 0x3C0;        // [C] array of 7 ints (per rank)
    constexpr auto ResCastType      = 0x510;        // [C] targeting type enum
    constexpr auto ResMissileSpec   = 0x508;        // [C] missile specification ptr
    constexpr auto ResScriptName    = 0x80;         // [C] spell script name string
    constexpr auto ResCooldownTime  = 0x304;        // [C] array of 7 floats (per rank)
    constexpr auto ResAmmoRecharge  = 0x408;        // [C] array of 7 floats
    constexpr auto ResImgIconName   = 0x2A0;        // [C] icon name string
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
    constexpr auto Offset           = 0x41F0;       // [V] LeagueObfuscation offset from IDA sub_28E8C0
    constexpr auto InnerManager     = 0x10;         // [V] Final dereference to real AiManager
    constexpr auto NavPathPtr       = 0x30;         // [S] NavPath pointer (in dec struct)
    constexpr auto TargetPosition   = 0x034;        // [V] Vec3: Click destination / target position
    constexpr auto StartPath        = 0x88;         // [D]
    constexpr auto RefCount         = 0x1F0;        // [S]
    constexpr auto Velocity         = 0x318;        // [V] float: Movement speed value
    constexpr auto IsMoving         = 0x31C;        // [V] bool: Is currently moving
    constexpr auto CurrentSegment   = 0x320;        // [V] int: Current path segment index
    constexpr auto PathStart        = 0x330;        // [V] Vec3: Start of current path
    constexpr auto PathEnd          = 0x33C;        // [V] Vec3: End of current path
    constexpr auto Segments         = 0x348;        // [V] ptr: Waypoints array (Vec3[])
    constexpr auto NavArray         = 0x348;        // [V] ptr: Same as Segments (alias)
    constexpr auto SegmentsCount    = 0x350;        // [V] int: Number of waypoints
    constexpr auto HasPath          = 0x354;        // [V] int: Whether path data exists
    constexpr auto DashSpeed        = 0x360;        // [V] float: Dash speed
    constexpr auto IsDashing        = 0x384;        // [V] bool: Is currently dashing
    constexpr auto TargetPos2       = 0x3A8;        // [V] Vec3: Secondary target position
    constexpr auto ServerPos        = 0x474;        // [V] Vec3: Server-authoritative position
    constexpr auto MoveVec3         = 0x480;        // [S] Vec3: Move direction vector
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

    // Chat  (ChatClient object offsets)
    constexpr auto ChatOpen         = 0x10;         // [IDA] byte flag: 1=chat input active, 0=closed (sub_3B4E00 sets ChatClient+16)

    // Viewport W2S
    constexpr auto ViewportW2S      = 0x2B0;        // [IDA] viewport W2S matrix offset
}

// ================================================================
// MISSILE OBJECT
// IDA MCP verified (2026-03-08):
//   sub_886AE0: missile init — copies CastInfo INLINE at missile+0x2C0
//   sub_845A50: CastInfo copy function (full struct layout mapped)
//   sub_90A0E0: missile collision — reads Position at +0x25C, CasterNetId at +0x358
//   sub_49E9F0: returns *(missile+0x128) = SpellData ptr
//   sub_28E710: returns *(missile+0x2C0) = first QWORD = SpellData ptr of CastInfo
//
// CastInfo is INLINE at missile+0x2C0 (NOT a pointer!)
// Read fields directly: startPos = Read<Vec3>(missile + StartPos)
// ================================================================
namespace Missile {
    // --- Missile Object (absolute offsets from missile base) ---
    constexpr auto SpellDataPtr     = 0x128;        // [IDA] sub_49E9F0: *(missile+0x128) = SpellData ptr
    constexpr auto Position         = 0x25C;        // [IDA] sub_90A0E0: Vec3 pos (inherited from GameObject)
    constexpr auto CastInfoBase     = 0x2C0;        // [IDA] sub_886AE0: CastInfo struct INLINE here (NOT a pointer!)
    constexpr auto MissileNetId     = 0x364;        // [IDA] sub_886AE0: [rsi+364h] = NetID (tree key) = CI+0xA4

    // --- CastInfo fields — ABSOLUTE offsets from missile base (0x2C0 + CI_*) ---
    //   Read directly: value = Read<T>(missile + offset)
    constexpr auto CI_SpellData     = 0x2C0;        // [IDA] QWORD: SpellData ptr (CastInfo+0x00)
    constexpr auto SpellName        = 0x2E0;        // [IDA] std::string SSO: spell name (CastInfo+0x20)
    constexpr auto MissileName      = 0x308;        // [IDA] std::string SSO: missile name (CastInfo+0x48)
    constexpr auto StartPos         = 0x330;        // [IDA] Vec3: start position (CastInfo+0x70)
    constexpr auto EndPos           = 0x33C;        // [IDA] Vec3: end position (CastInfo+0x7C)
    constexpr auto CastEndPos       = 0x34C;        // [IDA] Vec3: cast end position (CastInfo+0x8C)
    constexpr auto CasterNetId      = 0x358;        // [IDA] int: source caster net id (CastInfo+0x98)
    constexpr auto TargetNetId      = 0x35C;        // [IDA] int: target net id (CastInfo+0x9C)
    constexpr auto CI_TargetNetId2  = 0x360;        // [IDA] int: secondary target (CastInfo+0xA0)
    constexpr auto CI_MissileNetId  = 0x364;        // [IDA] int: missile net id (CastInfo+0xA4)

    // --- CastInfo relative offsets (for code that needs CI base + offset pattern) ---
    constexpr auto CI_REL_SpellData    = 0x00;      // [IDA] CastInfo+0x00
    constexpr auto CI_REL_SpellName    = 0x20;      // [IDA] CastInfo+0x20
    constexpr auto CI_REL_MissileName  = 0x48;      // [IDA] CastInfo+0x48
    constexpr auto CI_REL_StartPos     = 0x70;      // [IDA] CastInfo+0x70
    constexpr auto CI_REL_EndPos       = 0x7C;      // [IDA] CastInfo+0x7C
    constexpr auto CI_REL_CastEndPos   = 0x8C;      // [IDA] CastInfo+0x8C
    constexpr auto CI_REL_CasterNetId  = 0x98;      // [IDA] CastInfo+0x98
    constexpr auto CI_REL_MissileNetId = 0xA4;      // [IDA] CastInfo+0xA4

    // --- Legacy aliases ---
    constexpr auto NetworkId        = MissileNetId; // 0x364
    constexpr auto SpellDataInst    = CI_SpellData; // 0x2C0
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
    constexpr auto LaneArray        = 0x68;         // [D] ptr to lane minion array (relative to MinionManager)
    constexpr auto LaneCount        = 0x70;         // [IDA] count of lane minions (relative to MinionManager)
    constexpr auto LaneType         = 0x4CC9;       // [CE] byte on obj: 4=Melee, 5=Ranged, 6=Cannon, 7=Super
}

// ================================================================
// SPELL CAST INFO (Active Spell)
// From: OnProcessSpell (0x920430) decompilation + chimera
// ================================================================
namespace SpellCastInfo {
    constexpr auto SpellData        = 0x0;          // [IDA] first QWORD = SpellData ptr
    constexpr auto SrcIndex         = 0x98;         // [C] source caster network index
    constexpr auto StartPos         = 0xD8;         // [C] Vec3 spell start position
    constexpr auto EndPos           = 0xE4;         // [C] Vec3 spell end position
    constexpr auto CastPos          = 0xF0;         // [C] Vec3 cast position
    constexpr auto TargetIndex      = 0x108;        // [C] target network index
    constexpr auto CastDelay        = 0x118;        // [C] float cast delay
    constexpr auto IsSpell          = 0x134;        // [C] bool is spell (not auto)
    constexpr auto IsSpecialAttack  = 0x13E;        // [C] bool is special attack
    constexpr auto IsAuto           = 0x141;        // [IDA] byte: is auto attack (chimera=0x13F)
    constexpr auto Slot             = 0x14C;        // [IDA] DWORD: spell slot index (chimera=0x148)
}

// ================================================================
// ITEM SYSTEM
// From: IDA MCP analysis + chimera_structures.h
// ================================================================
namespace ItemSystem {
    // GameObject::ItemList = 0x4D20 (in GameObject namespace)
    // Array of 7 ItemSlot pointers (6 items + trinket)
    constexpr auto SlotInfo         = 0x10;         // [IDA] ItemSlot+0x10 → ItemInfo ptr
    constexpr auto InfoData         = 0x38;         // [IDA] ItemInfo+0x38 → ItemData ptr
    constexpr auto InfoStacks       = 0x64;         // [C] ItemInfo+0x64 → stack count
    constexpr auto DataItemId       = 0xB4;         // [IDA] ItemData+0xB4 → item ID int
    constexpr auto DataAbilityHaste = 0x160;        // [C] ItemData stat
    constexpr auto DataHealth       = 0x164;        // [C] ItemData stat
    constexpr auto DataArmor        = 0x19C;        // [C] ItemData stat
    constexpr auto DataMR           = 0x1BC;        // [C] ItemData stat
    constexpr auto DataAD           = 0x1D8;        // [C] ItemData stat
    constexpr auto DataAP           = 0x1E0;        // [C] ItemData stat
    constexpr auto DataAtkSpeedMult = 0x20C;        // [C] ItemData stat
}

// ================================================================
// NAV GRID
// Source: sig 48 8B 05 ? ? ? ? 0F 28 DA → rva 0x1D32A80 (15.24)
// navGrid ptr → +0x8 → Manager ptr → fields below
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
    constexpr auto TurretManager    = 0x1D87068;    // [P][IDA] 20 xrefs confirmed
    constexpr auto ViewMatrixInst   = 0x1E2C030;    // [P][IDA] 5+ xrefs confirmed (view/projection matrix)
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
