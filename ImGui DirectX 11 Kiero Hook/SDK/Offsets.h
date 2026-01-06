#pragma once
#include <cstdint>

namespace Offset
{
	// ============================================================================
	// Base Addresses & Managers
	// ============================================================================
	inline constexpr uint64_t oLocalPlayer = 0x1D66AE0;        // [ORBWALKER + TARGET SELECTOR] Pattern: 48 8B 0D ? ? ? ? 4C ? ? 74 ? 49
	inline constexpr uint64_t oHerroList = 0x1D2F3B0;          // [ORBWALKER + TARGET SELECTOR] Pattern: 48 ? ? ? ? ? ? 48 ? ? ? ? 33 c0 89 ? ? ? 89 ? ? ? e8 ? ? ? ? 8b || 48 8B 0D ? ? ? ? 0F 85 ? ? ? ? 83
	inline constexpr uint64_t oGametime = 0x1D3D370;          // [ORBWALKER] Pattern: F3 0F 5C 35 ? ? ? ? 0F 28 F8
	inline constexpr uint64_t oMinionList = 0x1D32AF0;        // [ORBWALKER] Pattern: 48 8b 05 ?? ?? ?? ?? f3 ?? ?? ?? ?? 48 ?? ?? ?? 5b || 48 8B 05 ? ? ? ? F3 ? ? ? ? 45
	inline constexpr uint64_t oMissileList = 0x1D32B08;        // [EVADE] Pattern: 48 8b 0d ? ? ? ? 48 8d 55 0f e8 ? ? ? ? (Alias: oMisslilist, oMissileManager)
	inline constexpr uint64_t oTurretList = 0x1D3BEB8;
	inline constexpr uint64_t oListSizeHero = 0x10;            // [ORBWALKER + TARGET SELECTOR]
	
	// ============================================================================
	// GameObject Structure Offsets
	// ============================================================================
	inline constexpr uint64_t oObjNetId = 0xC4;                // [ORBWALKER + TARGET SELECTOR] Pattern: 8B 80 ? ? ? ? 89 43 ? 48 83 C4 ? 5B C3 8B
	inline constexpr uint64_t oObjPosition = 0x254;            // [ORBWALKER + TARGET SELECTOR] Pattern: F3 0F 10 B6 ? ? ? ? 0F 29 7C 24
	inline constexpr uint64_t oHealth = 0x10A8;                 // [ORBWALKER + TARGET SELECTOR] Pattern: 49 8D 8F ?? ?? ?? ?? F3 41 0F 11 87 ?? ?? ?? ?? 33
	inline constexpr uint64_t oMaxHealth = 0x10D0;            // [ORBWALKER + TARGET SELECTOR]
	inline constexpr uint64_t oMana = 0x358;
	inline constexpr uint64_t oMaxMana = 0x380;
	inline constexpr uint64_t oDead = 0x250;                   // [ORBWALKER + TARGET SELECTOR]
	inline constexpr uint64_t oTargetable = 0xEC8;             // [TARGET SELECTOR] Pattern: 0F B6 83 ? ? ? ? 48 83 C4 ? 5B [CRITICAL: Fixed from 0x458]
	inline constexpr uint64_t oVisibility = 0x300;             // [TARGET SELECTOR]
	inline constexpr uint64_t TeamID = 0x0251;                // [ORBWALKER + TARGET SELECTOR] Pattern: 0F B6 88 ? ? ? ? EB 07 0F B6 8E
	inline constexpr uint64_t NamePlayer = 0x4358;             // [ORBWALKER + TARGET SELECTOR] Pattern: 48 81 C1 ? ? ? ? 48 3B CA 74 ? 48 83 7A ? ? 76
	inline constexpr uint64_t oObjRadius = 0x6F0;              // [ORBWALKER + TARGET SELECTOR] Pattern: F3 0f 11 93 ? ? 00 00 F3 0F 59 15 ?? ?? ?? ??
    inline constexpr uint64_t oObjName = 0x68;                 // [DEBUG] GameObject Name pointer offset

	// Combat Stats
	inline constexpr uint64_t SpeedPlayer = 0x1814;           // [ORBWALKER]
	inline constexpr uint64_t RangeAttack = 0x181C;           // [ORBWALKER] Pattern: F3 0F 10 80 ? ? ? ? F3 0F 11 47
	inline constexpr uint64_t DamageBase = 0x17D4;             // [ORBWALKER + TARGET SELECTOR]
	inline constexpr uint64_t DamageBonus = 0x1730;           // [ORBWALKER + TARGET SELECTOR]
	inline constexpr uint64_t Armor = 0x17FC;                 // [ORBWALKER + TARGET SELECTOR]
	inline constexpr uint64_t MagicResist = 0x1804;           // [ORBWALKER + TARGET SELECTOR]
	inline constexpr uint64_t AbilityPower = 0x1808;          // [ORBWALKER + TARGET SELECTOR]
	inline constexpr uint64_t CritChance = 0x17F8;            // [ORBWALKER + TARGET SELECTOR]
	inline constexpr uint64_t CritDamage = 0x17E8;            // [ORBWALKER + TARGET SELECTOR]	
	inline constexpr uint64_t ArmorPenFlat = 0x16F0;         // [ORBWALKER + TARGET SELECTOR]
	inline constexpr uint64_t ArmorPenPercent = 0x16D8;       // [ORBWALKER + TARGET SELECTOR]
	inline constexpr uint64_t MagicPenFlat = 0x16D4;          // [ORBWALKER + TARGET SELECTOR]
	inline constexpr uint64_t MagicPenPercent = 0x16DC;       // [ORBWALKER + TARGET SELECTOR]

	// Basic Attack
	inline constexpr uint64_t oBasicAttackBase = 0x2C90;       // Pattern: 48 8B 81 ? ? ? ? C3 CC 40 56 48
	inline constexpr uint64_t oBasicAttackOffset1 = 0x2C0;    // Pattern: 5B C3 CC 48 8B 81 ? ? ? ? C3
	inline constexpr uint64_t oBasicAttackOffset2 = 0x38;    // Pattern: 48 8B 41 ? 48 89 02 4C 89 42 08 F0 41
	inline constexpr uint64_t oBasicAttackRemote = 0x14D;     // 0x8 + 0x145 (ranged)
	inline constexpr uint64_t oBasicAttackMelee = 0x143;      // Melee offset
	inline constexpr uint64_t oObjBasicAttackCastCount = 0x4D44; // Verified: 11 after 10 attacks

	// Minion Specific
	inline constexpr uint64_t LaneMinionArray = 0x68;          // [ORBWALKER] Pattern: 48 8B 46 ? 8B 4E ? ? ? ? ? 48 3B C2
	inline constexpr uint64_t LaneMinionCount = 0x70;          // [ORBWALKER]
	inline constexpr uint64_t LaneMinionType = 0x4CC9;        // [ORBWALKER] Pattern: 0F B6 81 ? ? ? ? 3C ? 74 ? 2C ? 3C ? 76 (byte: 4=Melee, 5=Ranged, 6=Cannon, 7=Super)
    
    // ============================================================================
	// Missile/Projectile Structure Offsets (VERIFIED Jan 2025)
    // ============================================================================
	inline constexpr uint64_t oMissileNetId = 0x20;            // ✅ VERIFIED - [EVADE] Missile network ID
	
	// Position Offsets - MULTIPLE OFFSETS vì different missile types có structure khác nhau
	inline constexpr uint64_t oMissilePosition = 0x5A0;        // ✅ VERIFIED - [EVADE] Vec3 Current Position (Verified: SmolderW at 0x5A0)
	// 💡 NOTE: Có thể có nhiều offsets cho Position:
	//   - 0x5A0: SmolderW (VERIFIED)
	//   - 0x450: EzrealQ candidate (needs verify)
	//   - 0x1DC: From pattern (alternative)
	//   → Scanner sẽ test tất cả và tìm cái nào work
	
	// Source NetID Offsets - MULTIPLE OFFSETS vì different missile types
	inline constexpr uint64_t oMissileSrcIdx = 0x1D0;          // ✅ VERIFIED - [EVADE] Source NetID (caster) - VERIFIED offset
	// 💡 NOTE: Có thể có nhiều offsets cho SrcIdx:
	//   - 0x1D0: VERIFIED (một số missile types)
	//   - 0x2C4: From pattern (một số missile types khác)
	//   - 0x2DC: Alternative from pattern
	//   → Scanner sẽ test tất cả và tìm cái nào work với missile hiện tại
	
	// SpellInfo is accessed INDIRECTLY (MULTIPLE PATTERNS - different missiles use different patterns!)
	// ⚠️ IMPORTANT: SpellInfo KHÔNG có direct access! Must try multiple indirect patterns!
	// 💡 MỤC ĐÍCH: Tìm SpellInfo để identify spell name, sau đó lookup Speed/Radius/Width từ database
	// 💡 KHÔNG CẦN scan Speed/Radius/Width từ missile structure - hardcode vào database thay vì đó
	// 💡 Mỗi missile type có cách access SpellInfo khác nhau → cần nhiều patterns
	// Pattern 1: Missile[0x1F0] -> SpellCast* -> SpellCast[0xD8] -> SpellInfo* (Verified: "srhomeguardspeed")
	inline constexpr uint64_t oMissileSpellCast = 0x1F0;       // ✅ VERIFIED - [EVADE] Pointer to SpellCast (Pattern 1)
	inline constexpr uint64_t oSpellCastSpellInfo = 0xD8;      // ✅ VERIFIED - [EVADE] SpellCast -> SpellInfo offset (Pattern 1)
	
	// Pattern 2: Missile[0x578] -> PTR* -> PTR[0x98] -> SpellInfo* (Verified: "SmolderW")
	// 💡 NOTE: SmolderW dùng pattern này để access SpellInfo
	inline constexpr uint64_t oMissileSpellInfo_Pattern2_Offset = 0x578;  // ✅ VERIFIED - Pattern 2 first offset
	inline constexpr uint64_t oMissileSpellInfo_Pattern2_Inner = 0x98;    // ✅ VERIFIED - Pattern 2 inner offset
	
	// Pattern 3: Missile[0x728] -> PTR* -> PTR[0x98] -> SpellInfo* (Verified: "EzrealQ")
	// 💡 NOTE: EzrealQ dùng pattern này để access SpellInfo
	inline constexpr uint64_t oMissileSpellInfo_Pattern3_Offset = 0x728;  // ✅ VERIFIED - Pattern 3 first offset
	inline constexpr uint64_t oMissileSpellInfo_Pattern3_Inner = 0x98;    // ✅ VERIFIED - Pattern 3 inner offset
	
	// Pattern 4: Missile[0xC0] -> PTR* -> PTR[0xE8] -> SpellInfo* (Verified: "elderdragonexecutetarget")
	// 💡 NOTE: elderdragonexecutetarget dùng pattern này để access SpellInfo
	inline constexpr uint64_t oMissileSpellInfo_Pattern4_Offset = 0xC0;   // ✅ VERIFIED - Pattern 4 first offset
	inline constexpr uint64_t oMissileSpellInfo_Pattern4_Inner = 0xE8;    // ✅ VERIFIED - Pattern 4 inner offset
	
	// Legacy/deprecated offsets
	inline constexpr uint64_t oMissileSpellInfo = 0x1F0;       // ⚠️ [DEPRECATED] - Use indirect access patterns instead
	inline constexpr uint64_t oMissileSpellData = 0x298;       // ❌ [NOT FOUND] - SpellData KHÔNG có direct access, phải qua SpellInfo + 0x18
	
	// ============================================================================
	// Missile Offsets from partern (Direct Access - VERIFYING vs VERIFIED)
	// ============================================================================
	// ⚠️ NOTE: Các offsets này từ file partern, đang verify với offsets đã VERIFIED
	// ⚠️ Pattern: MissileData namespace offsets
	// 💡 COMPARISON:
	//   - VERIFIED: oMissilePosition = 0x5A0 (SmolderW), oMissileSrcIdx = 0x1D0
	//   - PATTERN: CurPos = 0x1DC, StartPos = 0x2E0, EndPos = 0x2EC, SrcIdx = 0x2C4
	//   - Có thể có nhiều offsets cho cùng một field (different missile types)
	// 💡 MỤC ĐÍCH: Tìm Position/StartPos/EndPos để VẼ ĐƯỜNG BAY của spell
	// 💡 Sau khi có SpellInfo (tên spell), lookup Speed/Radius/Width từ database
	
	// SpellInfo - DIRECT ACCESS (from partern) - ALTERNATIVE to indirect patterns
	inline constexpr uint64_t oMissileSpellInfo_Direct = 0x260;  // ⚠️ [FROM PATTERN] - Direct SpellInfo pointer (alternative to Pattern 1-4)
	// 💡 NOTE: SpellInfo có thể access qua:
	//   1. Indirect patterns (0x1F0->0xD8, 0x578->0x98, 0x728->0x98, 0xC0->0xE8) - VERIFIED
	//   2. Direct access tại 0x260 - FROM PATTERN (cần verify)
	// 💡 MỤC ĐÍCH: Get SpellInfo để identify spell name → lookup Speed/Radius/Width từ database
	
	// Position Offsets (from partern) - ALTERNATIVES to verified offsets
	// 💡 MỤC ĐÍCH: VẼ ĐƯỜNG BAY của spell - cần CurrentPos, StartPos, EndPos để tính toán trajectory
	inline constexpr uint64_t oMissileCurPos = 0x1DC;           // ⚠️ [FROM PATTERN] - Vec3 Current position (alternative to 0x5A0)
	// 💡 COMPARISON: VERIFIED oMissilePosition = 0x5A0 vs PATTERN CurPos = 0x1DC
	//   - Có thể khác nhau tùy missile type hoặc game version
	// 💡 USE: Để vẽ đường bay real-time (missile đang ở đâu)
	inline constexpr uint64_t oMissileStartPos = 0x2E0;          // ⚠️ [FROM PATTERN] - Vec3 Start position (cần verify với SpellInfo StartPos)
	// 💡 USE: Để vẽ đường bay từ start → end (trajectory prediction)
	inline constexpr uint64_t oMissileEndPos = 0x2EC;            // ⚠️ [FROM PATTERN] - Vec3 End/Target position (cần verify với SpellInfo EndPos)
	// 💡 USE: Để vẽ đường bay từ start → end (trajectory prediction)
	
	// Source/Target NetID (from partern) - ALTERNATIVES to verified offsets
	inline constexpr uint64_t oMissileSrcIdx_Alt = 0x2C4;        // ⚠️ [FROM PATTERN] - Source NetID (alternative to 0x1D0, alt: 0x2DC)
	// 💡 COMPARISON: VERIFIED oMissileSrcIdx = 0x1D0 vs PATTERN SrcIdx = 0x2C4 (alt: 0x2DC)
	//   - Có thể khác nhau tùy missile type
	inline constexpr uint64_t oMissileDestIdx = 0x318;           // ⚠️ [FROM PATTERN] - Target NetID (alternative: 0x330)
	inline constexpr uint64_t oMissileDestCheck = 0x31C;          // ⚠️ [FROM PATTERN] - Dest check flag
	
	// ============================================================================
	// Missing Missile Offsets (CẦN TÌM - Scan từ IDA)
	// ============================================================================
	// ⚠️ NOTE: Các offsets này CẦN SCAN từ IDA hoặc runtime scanning
	// ⚠️ Code scanner sẽ tự động scan và có thể update offsets này
	
	// Movement/Physics Offsets
	// ⚠️ NOTE: Speed/Radius/Width KHÔNG CẦN scan từ missile structure!
	// 💡 STRATEGY: Khi detect được SpellInfo (tên spell), tự động lookup từ database có s�ẵn
	// 💡 Database sẽ chứa: SpellName -> Speed, Radius, Width, Type, etc.
	// 💡 Ví dụ: "EzrealQ" -> Speed=2000, Radius=60, Width=60, Type=Linear
	// 💡 Các offsets này chỉ để reference nếu muốn verify hoặc fallback
	inline constexpr uint64_t oMissileSpeed = 0x0;             // ⚠️ [NOT NEEDED] - [EVADE] Float: Missile speed (units/s)
	// 💡 USE DATABASE INSTEAD: Lookup Speed từ SpellInfo name trong database
	inline constexpr uint64_t oMissileVelocity = 0x0;          // ⚠️ [NOT NEEDED] - [EVADE] Vec3: Missile velocity vector
	// 💡 CALCULATE: Velocity = Speed * normalize(EndPos - StartPos)
	inline constexpr uint64_t oMissileDirection = 0x0;        // ⚠️ [NOT NEEDED] - [EVADE] Vec3: Missile direction vector
	// 💡 CALCULATE: Direction = normalize(EndPos - StartPos)
	
	// Collision/Size Offsets
	inline constexpr uint64_t oMissileRadius = 0x0;            // ⚠️ [NOT NEEDED] - [EVADE] Float: Collision radius
	// 💡 USE DATABASE INSTEAD: Lookup Radius từ SpellInfo name trong database
	inline constexpr uint64_t oMissileWidth = 0x0;              // ⚠️ [NOT NEEDED] - [EVADE] Float: Skillshot width
	// 💡 USE DATABASE INSTEAD: Lookup Width từ SpellInfo name trong database
	
	// Timing Offsets
	inline constexpr uint64_t oMissileStartTime = 0x0;         // ⚠️ [NOT NEEDED] - [EVADE] Float: GameTime khi missile created
	// 💡 CALCULATE: StartTime = GameTime khi missile first appears trong MissileManager
	inline constexpr uint64_t oMissileLifetime = 0x0;         // ⚠️ [NOT NEEDED] - [EVADE] Float: Thời gian sống của missile
	// 💡 USE DATABASE INSTEAD: Lookup Lifetime từ SpellInfo name trong database, hoặc tính từ StartTime
	
	// Targeting Offsets (Optional - chỉ có với targeted spells)
	// ⚠️ NOTE: oMissileTargetType không tồn tại trong game structure - đã xác nhận không có offset này
    
    // ============================================================================
	// AI Manager Structure Offsets (VERIFIED Jan 2025)
    // ============================================================================
	inline constexpr uint64_t oObjAiManagerObf = 0x4218;      // ✅ VERIFIED - GameObject -> LeagueObfuscation<ptr> (from sub_289E40: lea rdx, [rcx+4218h])
	inline constexpr uint64_t oGetAiManagerFunc = 0x289E40;   // Decrypt function (sub_289E40) Pattern: 73 20 90 0F B6 0C 02

	// AiManager offsets (relative to decrypted pointer) - From partern 15.11
	inline constexpr uint64_t oAiManagerStartPath = 0x330;    // ✅ VERIFIED - Vec3 Current position
	inline constexpr uint64_t oAiManagerEndPath = 0x33C;     // ✅ VERIFIED - Vec3 Target/End position (StartPath + 0xC)
	inline constexpr uint64_t oAiManagerServerPos = 0x474;   // ✅ VERIFIED - Vec3 Server position
	inline constexpr uint64_t oAiManagerIsMoving = 0x31C;    // ✅ VERIFIED - bool Is currently moving
	inline constexpr uint64_t oAiManagerIsDashing = 0x384;   // ✅ VERIFIED - bool Is currently dashing
	inline constexpr uint64_t oAiManagerDashSpeed = 0x360;    // ✅ VERIFIED - float Dash speed
	inline constexpr uint64_t oAiManagerCurrentSegment = 0x320; // ✅ VERIFIED - int Current path segment index
	inline constexpr uint64_t oAiManagerSegmentsCount = 0x350; // ✅ VERIFIED - int Number of path segments
	inline constexpr uint64_t oAiManagerVelocity = 0x318;    // ✅ VERIFIED - float Speed (not Vec3) - Value = 330 when moving
	inline constexpr uint64_t oAiManagerTargetPosition = 0x34; // ✅ VERIFIED - Vec3 Target position (click destination)
	inline constexpr uint64_t oAiManagerNavArray = 0x348;    // ✅ VERIFIED - Pointer to NavArray
	inline constexpr uint64_t oAiManagerMoveVec3 = 0x480;    // ⚠️ [UNVERIFIED- Vec3 Move direction oAiManagerMoveVec3 = oAiManagerStartPath + 0x150
	inline constexpr uint64_t oAiManagerFacingAngle = 0x7C;  // ✅ VERIFIED (MOVING) - [PREDICTION] Float: Facing angle (góc hướng nhìn champion). Đã verify khi MOVING (giá trị trong range -PI to PI). ⚠️ CẦN VERIFY KHI IDLE (nên là ~PI)
	inline constexpr uint64_t oAiManagerHasPath = 0x354;    // ✅ VERIFIED - Int flag: 1 khi IDLE, 2 khi MOVING (từ scan IDLE vs MOVING). Logic: != 0 = has path, > 1 = moving
	
	// Aliases for backward compatibility (GameObject.h uses these names)
	inline constexpr uint64_t oAiMgrPathStart = oAiManagerStartPath;
	inline constexpr uint64_t oAiMgrTargetPosition = oAiManagerTargetPosition;
	inline constexpr uint64_t oAiMgrIsMoving = oAiManagerIsMoving;
	inline constexpr uint64_t oAiMgrIsDashing = oAiManagerIsDashing;
	inline constexpr uint64_t oAiMgrDashSpeed = oAiManagerDashSpeed;
	inline constexpr uint64_t oAiMgrSegmentsCount = oAiManagerSegmentsCount;
	inline constexpr uint64_t oAiMgrCurrentSegment = oAiManagerCurrentSegment;

    // ============================================================================
	// Spell Book & Spell Slot Structure Offsets
    // ============================================================================
	inline constexpr uint64_t oObjSpellBook = 0x3110;         // [COMBO] Pattern: 49 8D ? ? ? ? ? 8B D0 4C 8D ? ? E8
	inline constexpr uint64_t oObjSpellBookSpellSlot = 0xAE0; // [COMBO] Pattern: 48 63 C2 48 8B 84 C1 ?? ?? ?? ?? C3
	inline constexpr uint64_t oObjOnCastingSpell = 0x3148;    // [COMBO] oObjSpellBook + 0x38
	
	// SpellSlot offsets
	inline constexpr uint64_t oSpellSlotLevel = 0x28;         // [COMBO] Spell level (1-5)
	inline constexpr uint64_t oSpellSlotCooldown = 0x30;      // [COMBO] Pattern: Next ready time (game time when spell ready)
	inline constexpr uint64_t oSpellSlotStartTime = 0x34;     // [COMBO] Spell cast start time
	inline constexpr uint64_t oSpellSlotStacks = 0x5C;        // [COMBO] Spell stacks (for spells like Akali R, Ahri R)
	inline constexpr uint64_t oSpellSlotTotalCooldown = 0x74; // [COMBO] Total cooldown duration
	inline constexpr uint64_t oSpellSlotSpellInput = 0x120;   // ✅ VERIFIED - [COMBO] Pointer to SpellInput
	inline constexpr uint64_t oSpellSlotSpellInfo = 0x128;    // [COMBO] Pattern: 48 83 BA ? ? ? ? 00 74 03 B0 01

	// SpellInput offsets (✅ VERIFIED from scan)
	inline constexpr uint64_t oSpellInputTargetNetId = 0x14;  // Found candidates near 0x18
	inline constexpr uint64_t oSpellInputStartPos = 0x18;     // ✅ VERIFIED - dist=0 from player
	inline constexpr uint64_t oSpellInputEndPos = 0x2c;      // ✅ VERIFIED - dist~1200 (skillshot range)
	// Additional end positions (for complex spells):
	// oSpellInputEndPos + 0xC = Vec3 target pos 2
	// oSpellInputEndPos + 0x18 = Vec3 target pos 3
	
	// SpellInfo offsets
	inline constexpr uint64_t oSpellInfoSpellData = 0x18;     // ✅ VERIFIED - Pointer to SpellData (verified: "JinxQ")
	inline constexpr uint64_t oSpellInfoSrcIndex = 0x88;      // [EVADE] Source NetID index
	inline constexpr uint64_t oSpellInfoStartPos = 0xA4;       // [EVADE] Spell start position
	inline constexpr uint64_t oSpellInfoEndPos = 0xB0;        // [EVADE] Spell end position (StartPos + 0xC)
	inline constexpr uint64_t oSpellInfoCastPos = 0xBC;       // [EVADE] Spell cast position (EndPos + 0xC)
	inline constexpr uint64_t oSpellInfoTargetIndex = 0xE0;    // [EVADE] Target NetID index
	inline constexpr uint64_t oSpellInfoCastDelay = 0xF0;     // [PREDICTION] Cast delay
	inline constexpr uint64_t oSpellInfoIsSpell = 0x10C;      // [EVADE] == 0 if spell
	inline constexpr uint64_t oSpellInfoIsAuto = 0x113;       // [EVADE] Is auto attack
	inline constexpr uint64_t oSpellInfoSlot = 0x11C;         // [EVADE] Spell slot index
	
	// SpellData offsets
	inline constexpr uint64_t oSpellDataScript = 0x18;         // [COMBO] SpellDataScript pointer
	inline constexpr uint64_t oSpellDataName = 0x8;           // ✅ VERIFIED - [COMBO] Spell name (verified: "JinxQ", "JinxW")
	inline constexpr uint64_t oSpellDataResource = 0x60;      // [COMBO] SpellDataResource pointer (may be invalid)
	inline constexpr uint64_t oSpellDataManaCost = 0x394;     // ✅ VERIFIED - [COMBO] Mana cost (verified: JinxW=60.9 at lv2)
	// ❌ MISSING: oSpellDataCastRange, oSpellDataSpeed, oSpellDataWidth, oSpellDataDelay
	// Pattern reference for CastRange: function 0x813430 (GetSpellRange call)
	
	// SpellDataScript offsets
	inline constexpr uint64_t oSpellDataScriptName = 0x8;     // [COMBO] Script name
	inline constexpr uint64_t oSpellDataScriptHash = 0x18;   // [COMBO] Script hash

	// ============================================================================
	// Buff Manager Structure Offsets
	// ============================================================================
	inline constexpr uint64_t oObjBuffManager = 0x2E68;       // ✅ VERIFIED - [TARGET SELECTOR] Pattern: 48 8D 8B ? ? ? ? 48 8B D7 E8 ? ? ? ? ? ? ? 48 8B CB
	inline constexpr uint64_t oBuffManagerArray = 0x18;       // [TARGET SELECTOR] BuffManager -> Array start
	inline constexpr uint64_t oBuffManagerArrayEnd = 0x20;    // [TARGET SELECTOR] BuffManager -> Array end
	inline constexpr uint64_t oBuffInstanceScript = 0x10;      // ✅ VERIFIED - [TARGET SELECTOR] BuffInstance -> BuffScript* (leads to name)
	inline constexpr uint64_t oBuffInstanceType = 0x08;       // ✅ VERIFIED - [TARGET SELECTOR] Buff type (values 24,25,26 = valid BuffType enum)
	inline constexpr uint64_t oBuffInstanceStartTime = 0x18;  // ✅ VERIFIED - [TARGET SELECTOR] Start time
	inline constexpr uint64_t oBuffInstanceEndTime = 0x1C;    // ✅ VERIFIED - [TARGET SELECTOR] End time (25000+ for permanent buffs)
	inline constexpr uint64_t oBuffInstanceStackCount = 0x38; // ✅ VERIFIED - [TARGET SELECTOR] Stack count (value=1 for active buffs)
	inline constexpr uint64_t oBuffInstanceCount = 0x8C;    // ✅ VERIFIED - [TARGET SELECTOR] Buff stack count (actual stacks). Verified với Conqueror: StackCount(0x38)=1 (instance), Count(0x8C)=12 (actual stacks). Logic: 0x38 = instance count (1), 0x8C = actual stack count (0-12)
	inline constexpr uint64_t oBuffScriptName = 0x8;          // ✅ VERIFIED - [TARGET SELECTOR] Buff name char* pointer
	
	// ============================================================================
	// HUD & Input Offsets
	// ============================================================================
	inline constexpr uint64_t oHudInstance = 0x1D2F4F8;       // [ORBWALKER] Pattern: 48 8B 0D ? ? ? ? 48 85 c9 74 ? 48 8b 49 ? 48 8d
	inline constexpr uint64_t oHudInstanceInput = 0x28;       // [ORBWALKER] Hud->Input
	inline constexpr uint64_t oHudMouseVec3 = 0x34;           // ✅ VERIFIED - [ORBWALKER + TARGET SELECTOR + ANTI-DETECT] Pattern: F3 0F 10 4F ? 48 8B BC 24 (World position của mouse - dùng để sync mouse với move target)
	inline constexpr uint64_t oHudInstanceCamera = 0x18;      // ✅ VERIFIED
	inline constexpr uint64_t oHudInstanceSpellInfo = 0x68;  // ✅ VERIFIED - [COMBO] Pattern: 48 8B 48 ? 48 85 C9 74 ? 48 8B 51 ? 48 85 D2 75 (REQUIRED for CastSpell!)
	inline constexpr uint64_t oHudInstanceUserData = 0x60;    // ✅ VERIFIED
	inline constexpr uint64_t oHudInstanceCameraZoom = 0x31c; // ✅ VERIFIED - Most variation in scan
	
	// ============================================================================
	// Mouse Input Offsets (Anti-Detection)
	// ============================================================================
	// ⚠️ NOTE: Memory offsets CHỈ ảnh hưởng đến GAME memory (anti-cheat detection)
	// ⚠️ KHÔNG fake được real mouse/keyboard cho live stream (cần hook DirectInput/IAT)
	inline constexpr uint64_t oMouseScreenVec2 = 0x1D41E28;   // ✅ VERIFIED - [ANTI-DETECT] Pattern: 48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B 0D ? ? ? ? 48 8B 01
	inline constexpr uint64_t oMouseScreenVec2_x = 0xC;       // ✅ VERIFIED - [ANTI-DETECT] Mouse screen X (game memory)
	inline constexpr uint64_t oMouseScreenVec2_y = 0x10;      // ✅ VERIFIED - [ANTI-DETECT] Mouse screen Y (game memory)
	inline constexpr uint64_t oMouseDevice = 0x1DADBE0;       // ✅ VERIFIED - [ANTI-DETECT] Pattern: 48 8B 0D ? ? ? ? 4C 8D 0D ? ? ? ? 4C 8B 05 ? ? ? ? BA (DirectInput Mouse Device pointer - dùng để HOOK)

    // ============================================================================
	// Keyboard Input Offsets (Anti-Detection)
    // ============================================================================
	// ⚠️ NOTE: Cần fake keyboard nếu fake mouse (để tránh detect pattern bất thường)
	inline constexpr uint64_t oKeyboardDevice = 0x1DC67C8;    // ✅ VERIFIED - [ANTI-DETECT] Pattern: 48 8B 0D ? ? ? ? 4C 8D 0D ? ? ? ? 4C 8B 05 ? ? ? ? BA (DirectInput Keyboard Device pointer - dùng để HOOK)
	inline constexpr uint64_t oKeyboardHit = 0x1DC4F80;       // ✅ VERIFIED - [ANTI-DETECT] Pattern: C6 84 ? ? ? ? ? 01 C6 84 ? ? ? ? ? 01 89 (Keyboard state trong game memory)
	inline constexpr uint64_t oKeyboardRgDOD = 0x1DADBE8;     // ✅ VERIFIED - [ANTI-DETECT] Pattern: r8:4C 8B 05 ? ? ? ? BA ? ? ? ? C7 (Keyboard buffer pointer)
	inline constexpr uint64_t oKeyboardPdInOut = 0x1DADC08;   // ✅ VERIFIED - [ANTI-DETECT] Pattern: r9:4C 8B 05 ? ? ? ? BA ? ? ? ? C7 (Keyboard input/output buffer)
	inline constexpr uint64_t oGetDeviceState = 0x50;         // ✅ VERIFIED - [ANTI-DETECT] Pattern: 83 BB ? ? ? ? 00 74 ? F6 43 ? 01 75 14 F6 (GetDeviceState vtable offset - không đổi)
	
	// ============================================================================
	// IAT Hooks (For Live Stream Fake Input)
	// ============================================================================
	// ⚠️ NOTE: Để fake mouse/keyboard cho LIVE STREAM, cần HOOK IAT/API calls, KHÔNG phải memory offsets
	inline constexpr uint64_t oIAT_GetCursorPos = 0x18819B0;  // ✅ VERIFIED - [STREAM-FAKE] Pattern: FF 15 ? ? ? ? 48 8D 54 24 ? 48 8B CB FF (GetCursorPos IAT - dùng để HOOK cho stream)
	
	// ============================================================================
	// OBS Capture Bypass (For Live Stream - Hide Overlay)
	// ============================================================================
	// ⚠️ NOTE: OBS Bypass = Ẩn overlay/menu khỏi stream (KHÁC với fake input)
	// ⚠️ Cần hook DirectX Present/SwapChain để filter overlay rendering
	// ⚠️ Reference: https://www.unknowncheats.me/forum/anti-cheat-bypass/504835-hijack-obs-capture-ud-eac-vanguard.html
	// ⚠️ Reference: https://www.unknowncheats.me/forum/direct3d/316041-bypass-obs-capture-overlay.html
	// ⚠️ Lưu ý: Bypass overlay KHÔNG fake input → Vẫn cần fake mouse/keyboard riêng

	// ============================================================================
	// Chat System Offsets
	// ============================================================================
	// Logic following LeagueAddon-main (Pointer -> Offset)
	inline constexpr uint64_t oChatInstance = 0x1D32B30;      // ✅ VERIFIED - [ORBWALKER] Pattern: 48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B B4 24 ? ? ? ? EB
	inline constexpr uint64_t oIsChatOpen = 0x6D8;           // [ORBWALKER] Offset inside ChatInstance to check if chat is open (bool)
	
	// Direct global flag (Alternative method)
	inline constexpr uint64_t oChatState = 0x193EB74;        // ✅ VERIFIED - [ORBWALKER] Pattern: 44 8B 05 ? ? ? ? 48 8B 54 24 ? 48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B B4
	
	// Functions
	inline constexpr uint64_t oPrintChat = 0xB293B0;         // [ORBWALKER] Pattern: E8 ? ? ? ? 4C 8B C3 B2 01
    
    // ============================================================================
	// Navigation Grid Offsets
    // ============================================================================
	namespace NavigationGrid
    {
		inline constexpr uint64_t GlobalPtr = 0x1D32A80;      // [PREDICTION] Pattern: 48 8B 05 ? ? ? ? 0F 28 DA
		
		namespace Manager
		{
			inline constexpr uint64_t Manager = 0x8;           // NavGrid -> Manager*
			inline constexpr uint64_t Width = 0x708;           // Grid width
			inline constexpr uint64_t Height = 0x70C;          // Grid height
			inline constexpr uint64_t Scale = 0x714;           // Grid scale
			inline constexpr uint64_t MinimumX = 0xEC;         // Min X coordinate
			inline constexpr uint64_t MinimumZ = 0xF4;         // Min Z coordinate
			inline constexpr uint64_t Data = 0x150;            // Grid data (byte flags array)
		}
	}

	// ============================================================================
	// View & Rendering Offsets
	// ============================================================================
	inline constexpr uint64_t ViewPort = 0x1D32AA8;           // Pattern: 4c 8B 3D ? ? ? ? 48 03 ? 49
	inline constexpr uint64_t oViewportW2S = 0x2B0;            // Pattern: 49 8B B7 ? ? ? ? 48 3B DF
	inline constexpr uint64_t ViewProjectionMatrix = 0x1DD3940; // Pattern: 48 8D 0D ? ? ? ? 0F 10 00
	inline constexpr uint64_t projMatrix = 0x40;

    // ============================================================================
	// Target Selector Offsets
    // ============================================================================
	inline constexpr uint64_t oUnderMouseObj = 0x1D32D00;     // [TARGET SELECTOR] Pattern: 48 89 0D ? ? ? ? 48 8D 05 ? ? ? ? 48 89 01 33 D2
	inline constexpr uint64_t oUnderMouseObjOffset = 0x18;    // [TARGET SELECTOR] Pattern: 48 8B 4B ? 0F 28 74 24 ? 8B 81

	// ============================================================================
	// Function Addresses
	// ============================================================================
	namespace Function 
	{
		inline constexpr uint64_t WorldToScreen = 0x13CF720;  // Pattern: E8 ? ? ? ? F3 0F 10 44 24 ? F3 41 0F 11 06
		inline constexpr uint64_t oIssueOrder = 0x29B6E0;     // [ORBWALKER] Pattern: E8 ? ? ? ? 8D 43 11
		inline constexpr uint64_t oCastSpellWrapper = 0x949DE0; // ⚠️ [NEEDS RE-VERIFY] - [COMBO] Pattern: 48 89 48 ?? 55 56... matches at +7 (CRITICAL! Must verify!)
		inline constexpr uint64_t AttackDelay = 0x540DB0;    // ✅ VERIFIED - [ORBWALKER] Pattern: E8 ? ? ? ? 33 C0 F3 0F 11 83 ? ? ? ?
		inline constexpr uint64_t oGetAttackWindup = 0x540CB0; // ✅ VERIFIED - [ORBWALKER] Pattern: 48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC 60 48 8B 01 8B DA 0F 29 74 24 ??
		inline constexpr uint64_t GetPing = 0x667DB0;        // ✅ VERIFIED - [ORBWALKER] Pattern: E8 ? ? ? ? 8B F8 39 03
		inline constexpr uint64_t isMinion = 0x301120;        // [ORBWALKER] Pattern: E8 ? ? ? ? 48 8B 0B F3 0F 10 41 ?
		inline constexpr uint64_t isTurret = 0x300FC0;       // [ORBWALKER] Pattern: 40 53 48 83 EC 20 48 8B D9 48 85 C9 74 27
		inline constexpr uint64_t GetFirstObject = 0x513E40;  // [TARGET SELECTOR] Pattern: 48 83 EC ? 48 8B 51 ? 8B 41 ? 48 8D 0C C2
		inline constexpr uint64_t GetNextObject = 0x5034B0;  // [TARGET SELECTOR] Pattern: 0F B7 42 ? 44 8B 41 || E8 ? ? ? ? 48 8B D8 48 85 C0 0F 85
		inline constexpr uint64_t oGetBoundingRadius = 0x280DA0; // ✅ VERIFIED - [ORBWALKER + TARGET SELECTOR] Pattern: 40 53 48 83 EC ? 48 83 B9 ? ? ? ? 00 48 8B D9 0F 29 74 24 20
		inline constexpr uint64_t oGetCollisionFlags = 0x1D32A80; // [PREDICTION] Pattern: 48 83 ec 28 48 8b d1 48 8b 0d ? ? ? ? 48 8b 49 08 e8 29 00 00 00 48 ? ?
	}

	// ============================================================================
	// Backward-compatible Aliases
	// ============================================================================
	inline constexpr uint64_t oLevelSpell = oSpellSlotLevel;
	inline constexpr uint64_t oCooldownExpire = oSpellSlotCooldown;
	inline constexpr uint64_t oSpellTotalCooldown = oSpellSlotTotalCooldown;
	inline constexpr uint64_t oSpellInfo = oSpellSlotSpellInfo;
	inline constexpr uint64_t oSpellIData = oSpellDataResource;
	inline constexpr uint64_t NameSpell = oSpellDataName;
	inline constexpr uint64_t ManaCosSpell = oSpellDataManaCost;
	inline constexpr uint64_t oMisslilist = oMissileList;
	inline constexpr uint64_t oMissileManager = oMissileList;
	inline constexpr uint64_t Pos1 = oObjPosition;
	inline constexpr uint64_t objname = NamePlayer;
	inline constexpr uint64_t objnameMinion = 0x60;
	inline constexpr uint64_t typestrucminion = 0x658;
}
