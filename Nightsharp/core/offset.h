#pragma once


namespace Offset {

namespace GameObjectsRuntime {
    constexpr auto Player = 0x1EDB378;
    constexpr auto TeamTable = 0x1E9CF98;
    constexpr auto Objects = 0x1E9CFB8;
    constexpr auto Heroes = 0x1E9D0D8;
    constexpr auto Minions = 0x1E9D118;
    constexpr auto Missiles = 0x1EA0250;
    constexpr auto Turrets = 0x1EA4AC0;
    constexpr auto UnderMouseObject = 0x1EA0448;
} // namespace GameObjectsRuntime

namespace VTable {
    constexpr auto GameObjectBoundingRadius = 0x130;
} // namespace VTable

    namespace ObjectManagerRuntime {
        constexpr auto ManagerListItems = 0x8;
        constexpr auto ManagerListSize = 0x10;
        // NetworkId red-black tree (MSVC std::map style), verified
        // on IDA 13337 in sub_560700 (insertion) and sub_2F6930 (destructor
        // unregister) — both walk the tree headed at `manager + 0x38`.
        // Header is the sentinel; header->parent (+0x08) holds the root.
        // Node layout: left=+0x00, parent=+0x08, right=+0x10, color=+0x18,
        // is_nil=+0x19, key(uint32)=+0x20, value(object ptr)=+0x28.
        // Main ObjectManager uses +0x38; MissileManager uses +0x08
        // for the same NetworkId std::map header.
        constexpr auto NetworkIdTree = 0x38;
        constexpr auto MissileManagerNetworkIdTree = 0x8;
        constexpr auto NetworkIdTreeRootSlot = 0x8; // header->parent → root
        constexpr auto NetworkIdTreeNodeLeft = 0x0;
        constexpr auto NetworkIdTreeNodeParent = 0x8;
        constexpr auto NetworkIdTreeNodeRight = 0x10;
        constexpr auto NetworkIdTreeNodeIsNil = 0x19;
        constexpr auto NetworkIdTreeNodeKey = 0x20;
        constexpr auto NetworkIdTreeNodeObject = 0x28;
        constexpr auto GetFirstObject = 0xA66760;
        constexpr auto GetNextObject = 0x553C30;
        constexpr auto FindObject = 0x5521B0;
    } // namespace ObjectManagerRuntime

namespace GameRuntime {
    constexpr auto GameTime = 0x1EABFF0;
    constexpr auto NetInstance = 0x1E9CFB0;
    constexpr auto MissionInfoInstance = 0x1E9D0C8;
    constexpr auto ChatViewController = 0x1EB34E8;
    constexpr auto ShopInstance = 0x1EB34F0;
    constexpr auto OpenWindowsArray = 0x1F79028;
    constexpr auto OpenWindowsCount = 0x1F79030;
    constexpr auto MySpellState = 0x1ECC618;
    constexpr auto CursorPosRaw = 0x1F3B618;
    // Global MouseInput pointer. sub_5999D0 reads screen X/Y from +0x0C/+0x10.
    constexpr auto MouseScreenVec2 = 0x1EA0200;
    constexpr auto GetPing = 0x6CC720;
    constexpr auto GetMapID = 0x27E660;
    constexpr auto PrintChat = 0x1158CB0;
} // namespace GameRuntime

namespace MouseInputLayout {
    constexpr auto ScreenX = 0x0C;
    constexpr auto ScreenY = 0x10;
} // namespace MouseInputLayout

namespace DrawingRuntime {
    constexpr auto WorldToScreen = 0x13161A0;
    // qword_1E79D20 points to the render/view state. Native W2S receives
    // qword_1E79D20 + 0x2F8 as its first argument.
    constexpr auto ViewProjectionRoot = 0x1EA01F8;
    constexpr auto WorldToScreenContextOffset = 0x2F8;
    // Hud root/controller object. IDA 13337: sub_B9BBC0 stores constructor
    // arg into qword_1E76E08; qword_1E76E00 is the adjacent HUD config/event
    // owner used by Hud logic constructors.
    constexpr auto HudRoot = 0x1E9D140;
    constexpr auto HudInstance = 0x1E9D148;
    // `qword_1EAED80` owns the published ScoreboardViewController interface.
    // IDA 13337: ScoreboardViewController::Setup (sub_E28270) writes its
    // secondary interface pointer to `[qword_1EAED80 + 0x128]`; the
    // destructor clears the same field. The fields behind that interface
    // (TeamScoresDefinitions / DragonTracker) still require runtime
    // verification and are deliberately not represented here.
    constexpr auto ScoreboardViewController = 0x128;
    constexpr auto ViewPort = 0x1EAED80;
    constexpr auto ViewPort2 = 0x1F7AA08;
    constexpr auto Renderer = 0x1F7AA18;
    constexpr auto ViewProjOffset = 0x1F644A0;
    // qword_1E9D180: stats manager. Owns a std::map<uint32_t, vector<StatBlock*>>
    // at +0x30 keyed by team ID (100=ORDER, 200=CHAOS). Each StatBlock is 8128
    // bytes allocated by sub_2FDB70 and initialised by sub_2EA000.
    // IDA 13337: sub_307E30 reads *(statsManager+0x30) tree, finds node by
    // teamId, double-dereferences vector begin to get StatBlock*.
    constexpr auto StatsManager = 0x1E9D180;
} // namespace DrawingRuntime

namespace StatsRuntime {
    // Tree container at StatsManager + 0x30 (std::map layout, MSVC).
    constexpr auto TreeOffset = 0x30;
    // Tree node layout (MSVC std::_Tree_node):
    //   +0x00 _Left, +0x08 _Right, +0x10 _Parent,
    //   +0x18 _Color(byte), +0x19 _Isnil(byte),
    //   +0x20 key(uint32 teamId),
    //   +0x28 value(vector<StatBlock*>::begin)
    constexpr auto NodeKey = 0x20;
    constexpr auto NodeIsNil = 0x19;
    constexpr auto NodeValueBegin = 0x28;
    constexpr auto SentinelParent = 0x10;
    // Stat entry layout: +0x00 vtable, +0x08 std::string name,
    //   +0x18 type(int), +0x1C flags, +0x20 value(int).
    constexpr auto StatEntryValue = 0x20;
    // Offsets of stat entries within StatBlock (from sub_2EA000).
    constexpr auto DragonKillsEntry = 0x688;
    constexpr auto ElderDragonKillsEntry = 0x6B0;
    constexpr auto RiftHeraldKillsEntry = 0x6D8;
    // Convenience: entry offset + StatEntryValue = direct value offset.
    constexpr auto DragonKills = DragonKillsEntry + StatEntryValue;       // 0x6A8
    constexpr auto ElderDragonKills = ElderDragonKillsEntry + StatEntryValue; // 0x6D0
    constexpr auto RiftHeraldKills = RiftHeraldKillsEntry + StatEntryValue;   // 0x6F8
} // namespace StatsRuntime

namespace MissionInfo {
    constexpr auto MapId = 0x8;
    constexpr auto GameMode = 0x38;
    constexpr auto GameId = 0xB0;
    constexpr auto GameType = 0x108;
    // Level-script helpers registered in sub_3780D0:
    // GetSelectedElementalTerrain -> MissionInfo+0x150
    // GetSelectedBaronPit         -> MissionInfo+0x151
    constexpr auto SelectedElementalTerrain = 0x150;
    constexpr auto SelectedBaronPit = 0x151;
} // namespace MissionInfo

// All offsets below are RELATIVE struct field offsets (not RVAs), so they
// remain stable across patches as long as the Riot client doesn't rewrite
// the underlying objects. Verified on 26.6 by spot-checking against IDA:
//   - HudRuntime::Input  (0x28): sub_288C00 reads `[qword_1E1AB70 + 0x28]`
//                                then forwards it to the HudInput method
//                                (sub_BD4800), matching the legacy layout.
//   - DrawingMatrixRuntime::ProjMatrixRelative (0x40): unchanged - the
//                                view/proj matrices are still 16 floats
//                                each laid out back-to-back.
// The remaining HUD offsets weren't individually re-derived; they continue
// to function in the current control/drawing paths, so leaving them untouched.
namespace DrawingMatrixRuntime {
    constexpr auto ProjMatrixRelative = 0x40;
} // namespace DrawingMatrixRuntime

namespace HudRuntime {
    constexpr auto Camera = 0x18;
    constexpr auto Input = 0x28;
    constexpr auto CursorTargetLogic = 0x28;
    constexpr auto UserData = 0x60;
    constexpr auto SpellTargeting = 0x60;
    constexpr auto SpellInfo = 0x68;
    constexpr auto CameraZoom = 0x58;
    constexpr auto CameraZoomLimits = 0x310;
    constexpr auto AltZoomLimits = 0x3D0;
    constexpr auto ZoomLockFlag1 = 0x344;
    constexpr auto ZoomLockFlag2 = 0x345;
    constexpr auto MouseWorldPos = 0x34;
    constexpr auto ViewportW2S = 0x2B0;
} // namespace HudRuntime

namespace HudCursorTargetLogicRuntime {
    constexpr auto ShowClickEffect = 0xBC78C0; // sub_BC78C0(this, clickType): CursorMoveTo/CursorMoveToRed effect
} // namespace HudCursorTargetLogicRuntime

namespace HudCursorTargetLogicLayout {
    constexpr auto ClickPosition = 0x34; // Native Vector3f, used by sub_BC78C0 as effect origin
} // namespace HudCursorTargetLogicLayout

namespace HudSpellTargetingLayout {
    // Read-only target picker state verified while manually casting Jax Q.
    constexpr auto State = 0x20;       // 8 while a target is selected
    constexpr auto ObjectIndex = 0x30; // uint32, equals target object + 0x20
    // IDA 13337: `evtChampionOnly` handler sub_C0CFC0 writes `[HudSelectLogic+0x3C]`.
    constexpr auto TargetChampionsOnly = 0x3C;
} // namespace HudSpellTargetingLayout

// HudZoomLayout removed Apr 25/2026 - duplicate of ZoomRuntime::ZC_MinZoom/ZC_MaxZoom

// Struct field offsets (relative). Verified stable on 26.6 by spot-checks
// against the disassembly of the call sites that consume them.
namespace HudInputLayout {
    constexpr auto SelectedObjNetId = 0x64;
} // namespace HudInputLayout

// ─────────────────────────────────────────────────────────────────────────
// TacticalMap (minimap) layout
// ─────────────────────────────────────────────────────────────────────────
// TacticalMap is the minimap struct accessed via the Hud. Provides world->
// minimap projection. ScaleX/ScaleY combined with NegMinimapX/Y give the
// per-axis transform from world coordinates to minimap pixel coordinates.
//
// Access path (from globals):
//   tacticalMap = *(Hud + Hud::TacticalMapPtr)        // game-version dependent
//   minimapX    = *(tacticalMap + 0xB0)
//   minimapY    = *(tacticalMap + 0xB4)
//   minimapW    = *(tacticalMap + 0xB8)
//   minimapH    = *(tacticalMap + 0xBC)
//   negX        = *(tacticalMap + 0xA8)
//   negY        = *(tacticalMap + 0xAC)
//   scaleX      = *(tacticalMap + 0x188)
//   scaleY      = *(tacticalMap + 0x18C)
//
// Conversion formula (world -> minimap pixel):
//   px = (worldX + negX) * scaleX + minimapX
//   py = (worldZ + negY) * scaleY + minimapY  (Z is the world's "up-down" on the map)
namespace TacticalMapLayout {
    constexpr auto NegMinimapX     = 0xA8;
    constexpr auto NegMinimapY     = 0xAC;
    constexpr auto MinimapX        = 0xB0;
    constexpr auto MinimapY        = 0xB4;
    constexpr auto MinimapWidth    = 0xB8;
    constexpr auto MinimapHeight   = 0xBC;
    constexpr auto CachedWidth     = 0xC0;
    constexpr auto CachedHeight    = 0xC4;
    constexpr auto ControllerMap   = 0x530;
    constexpr auto ScaleX          = 0x188;
    constexpr auto ScaleY          = 0x18C;
} // namespace TacticalMapLayout


// ChatViewController layout (object lives at GameRuntime::ChatViewController).
// IDA 13337:
//   - Pattern "48 8B 05 ? ? ? ? 44 0F B6 FA" in sub_B5A730 resolves qword_1EB34E8.
//   - sub_B3F4A0 stores the constructed ChatViewController in qword_1EB34E8.
//   - sub_B51C00/sub_B53210 set/clear controller+0x66A when the chat panel expands/collapses.
//   - sub_B60090/sub_B61DB0 write the child panel flags at +0xB1/+0xB0.
// controller+0x5D0 is the narrower input/typing gate used for IsOpenChat blocking.
// qword_1EA01F0 and dword_1EA0458 are not chat-open state in this dump.
namespace ChatViewControllerLayout {
    constexpr auto InputPanel  = 0x368;
    constexpr auto InputActive = 0x5D0;
    constexpr auto Ready       = 0x669;
    constexpr auto PrimaryOpen = 0x66A;
    constexpr auto InputMode   = 0x66B;       // callback flag; do not use alone as open
    constexpr auto Editing     = InputActive; // compatibility alias
    constexpr auto Focused     = InputMode;   // compatibility alias

    constexpr auto PanelVisible = 0xB0;
    constexpr auto PanelFocused = 0xB1;
    constexpr auto PanelPending = 0xB2;
} // namespace ChatViewControllerLayout

namespace ControlRuntime {
    constexpr auto IssueOrder = 0x2899D0;
    // sub_984130(clientSpellMgr, spellSlot, slotIndex, position, releaseFlag)
    // is the native charge update/release packet sender.
    constexpr auto UpdateChargeableSpell = 0x980FC0;

    // ─────────────────────────────────────────────────────────────────────
    // Cast spell pipeline (verified IDB 13337 via EnsoulSharp.dll ILSpy map).
    // EnsoulSharp's managed Spellbook.CastSpell(...) forwards to the native
    // SpellbookClient.CastSpell, which on this build is the programmatic
    // dispatcher CastSpellSafe (sub_C04B50). See
    // CastSpell_EnsoulSharp_IDA_Analysis.md for the full reverse notes.
    //
    // CastSpellSafe is the regular cast dispatcher. Charge input is a
    // separate state machine driven by HudSpellHandler and sub_984130.
    constexpr auto CastSpellSafe          = 0xC130F0; // programmatic CastSpell dispatcher
    constexpr auto SendSpellCastPacket    = 0x97E580; // HUD SendSpellCastPacket (== Hooks::OnProcessSpell)
    constexpr auto BuildCastPacket        = 0x9651F0; // build cast-spell packet payload
    constexpr auto SendNetworkPacket      = 0x6EE340; // send packet via NetInstance
    constexpr auto InitChargeChanneling   = 0xBFE470; // server ack/timer helper; not charge begin
    constexpr auto InitChargeState        = 0xC14930; // stores SpellInput at HudSpellState+0x38
    constexpr auto ReleaseActiveCharge    = 0xBF37F0; // resolves slot and sends release
    constexpr auto GetPlayerClient        = 0x27C6F0; // charge sender uses return value + 0x3128
    // Cast dispatch helpers (offsets corrected against IDB 13337 decompile —
    // see CastSpellSafe_Reverse_13337.md §7-§10 for full reverse notes).
    constexpr auto FindOwnerSlot          = 0xBFBBC0; // sub_BEF3A0: match slot whose SpellInput(+0x130)==arg → SpellSlot*
    constexpr auto GetOwnerSlotIndex      = 0xBFBC80; // sub_BEF460: slot index 0..63 (highest-level match)
    constexpr auto NormalCastGate         = 0x2F7E30; // sub_2FEE90: gate slot<=3 && ready-flag (NOT a full can-cast check)
    constexpr auto ServiceRegistryLookup  = 0x23FC00; // RB-tree spell-service lookup from CastSpellSafe
    constexpr auto IsSlotQWER             = 0x3F14E0; // sub_3F4440: pure `slot <= 3` range check (NOT a self classifier)
    constexpr auto PrepareCast            = 0x97A0B0; // sub_97E0F0: prepare/build normal cast (flag -2 = self-cast)
    constexpr auto ValidateProcessCast    = 0x30D6B0; // sub_314B20: validate & process normal cast
    constexpr auto ExecuteNormalCast      = 0x32DF50; // sub_334EC0: cooldown accounting for normal cast
    constexpr auto ExecuteSelfCast        = 0x3384E0; // sub_33F150: cooldown accounting for self-cast / toggle
    constexpr auto ValidateTarget         = 0x30A3B0; // sub_311890: validate target (targeted) — not re-derived this session
    constexpr auto RangeCheck             = 0x310C40; // sub_3180B0: range check (targeted) — not re-derived this session
    constexpr auto ExecuteTargetCast      = 0x3383B0; // sub_33F020: execute cast on target — not re-derived this session

    // ── Pre-cast gate + position priming (REQUIRED before CastSpellSafe) ───
    // Native callers run CanCastCheck (sub_C12220) before CastSpellSafe.
    // It resolves the target/current cursor and primes internal HUD cast state.
    constexpr auto CanCastCheck           = 0xC20EB0; // sub_C12220: real CanCastCheck/ValidateCast (a3=targetNetId; 0=skillshot)
    constexpr auto PrimeCastPosition      = 0xBFA740; // sub_BEDF20: resolve world/mouse pos via raycast → cast position
    // HUD spell handler (the function the game itself calls when the player
    // presses/releases a hotkey). Signature confirmed in IDB 13337:
    //   sub_BF1EF0(__int64 hudSpellInfo, unsigned int slot,
    //              int castMode, __int64 keyState)
    // arg3 is the hotkey/cast mode (2 = Smart/Quick Cast).
    // arg4 is key state (1 = press, 2 = release).
    constexpr auto HudSpellHandler        = 0xBFEA60;
    constexpr auto SpellTypeClassify      = 0x916700; // sub_936DF0: spell-type byte +0x2F (1/6=targetable, 18=chargeable)
    constexpr auto GetSpellLevel          = 0x3A8B90; // sub_9342B0: spell level (used by GetOwnerSlotIndex to pick slot)
    // Cast packet opcode (chargeable/normal): 609 (0x261), vtable off_1A4BDF8
    constexpr auto CastPacketOpcode       = 0x261;

    // AIBaseClient handler registered for PKT_NPC_CastSpellAns_s (packet 0x370).
    // Signature: 4C 8B DC 55 57 49 8D AB ?? ?? ?? ?? 48 81 EC 28 02 00 00
    constexpr auto ProcessCastSpell = 0x292310; // 4C 8B DC 55 57 49 8D AB ?? ?? ?? ?? 48 81 EC 28 02 00 00
    constexpr auto GetSpellState = 0x95F3A0;
    // IDA 13339: sub_95F2E0 calls sub_912E30(slot) for remaining cooldown.
    // sub_912E30 reads slot+0x30, checks slot+0x68 ammo recharge, then clamps.
    // 0x92D820 resolves inside another function and is not a safe entry point.
    constexpr auto GetSpellRemainingCooldown = 0x912EF0;
    constexpr auto IsAlive = 0x2B0C10;
    constexpr auto GetSpellCastInfo = 0x2771C0;
    constexpr auto GetResourceType = 0x274DA0;
    constexpr auto GetAttackDelay = 0x573590;
    constexpr auto GetAttackWindup = 0x573490;
    constexpr auto GetBoundingRadius = 0x278010;
    constexpr auto IssueOrderFlag = 0x1DFEFD8;
    constexpr auto CastSpellFlag = 0x1DFEF70;
    constexpr auto CanAttack = 0x2119E0;
} // namespace ControlRuntime

// BasicAttackRuntime removed Apr 25/2026 - replaced by AIBaseClient::GetAutoAttackDamage()

namespace BuffManagerRuntime {
    constexpr auto BuffManagerOffset = 0x2E78;
} // namespace BuffManagerRuntime

// Buff system layouts: all RELATIVE struct field offsets, not RVAs.
// Stable on 26.6 (CoreBuffs/CoreObjects exercise these every frame and
// produce correct buff counts, so the layout matches). If any of these
// shift in a future build, regenerate by hooking sub_BF59E0 (buff add
// dispatcher) and inspecting the BuffData* it receives.
namespace BuffManagerLayout {
    constexpr auto EntriesStart = 0x18;
    constexpr auto EntriesEnd = 0x20;
    constexpr auto EntriesCapacityEnd = 0x28;
    constexpr auto Array2Start = 0x620;
    constexpr auto Array2End = 0x628;
} // namespace BuffManagerLayout

namespace BuffEntryLayout {
    constexpr auto EntryStride = 0x10;
    constexpr auto EntryBuff = 0x0;
    constexpr auto EntryAux = 0x8;
} // namespace BuffEntryLayout

namespace BuffDataLayout {
    constexpr auto BuffType = 0xC;
    constexpr auto BuffName = 0x8;
    constexpr auto BuffScriptPtr = 0x10;
    constexpr auto BuffStartTime = 0x18;
    constexpr auto BuffEndTime = 0x1C;
    // Stack array (StlVector of StlSharedPtr<BuffScriptInstance>).
    // Each entry is 16 bytes: {BuffScriptInstance*, refcount*}.
    // IDA 13337: sub_91BC60 reads buff+0x30 as begin, buff+0x38 as count.
    constexpr auto BuffStackArrayBegin = 0x30;
    constexpr auto BuffStackCount = 0x38;
    constexpr auto BuffStacks = 0x38;
    constexpr auto BuffStacksAlt = 0x3C;
} // namespace BuffDataLayout

// BuffScriptInstance is obtained by dereferencing the first 8 bytes of each
// 16-byte entry in the stack array at BuffDataLayout::BuffStackArrayBegin.
// IDA 13337: sub_91BC60 compares BuffScriptInstance+0x4 with the source
// object's network ID, confirming +0x4 is the caster's network ID.
namespace BuffScriptInstanceLayout {
    constexpr auto CasterNetworkId = 0x4;
    constexpr auto EntryStride = 0x10;  // 16 bytes per stack entry
} // namespace BuffScriptInstanceLayout

namespace BuffEventLayout {
    // OnBuffAdd/OnBuffRemove receive AIBaseClient::eventComponent
    // (hero + 0x2B0) in R9. OnBuffUpdate omits it and is resolved through
    // the per-unit event-bridge pointer cached from add/remove.
    constexpr auto OwnerComponent = 0x2B0;
} // namespace BuffEventLayout

namespace NavGridRuntime {
    constexpr auto NavGrid = 0x1EA0198;
    // IDA 13337: 0x1243760 is a path-neighbor expansion helper that calls
    // sub_124AEA0; CoreNavGrid reads cell flags directly instead of calling it.
    constexpr auto GetCollisionFlags = 0x1269B50;
    constexpr auto GetAiManager = 0x27ECA0; // inner AiManager pointer resolver
} // namespace NavGridRuntime

namespace SpellRuntime {
    constexpr auto SpellBookOffset = 0x3128;
    // ActiveSpellCast lives at a fixed delta from SpellBookOffset (the
    // spellbook's "currently casting" handle is the 7th qword inside the
    // spellbook header). Deriving it removes a stale-offset failure mode
    // when SpellBookOffset shifts on a patch.
    constexpr auto ActiveSpellCast =0x38;  // = 0x3160 on 26.6
} // namespace SpellRuntime

namespace RuneManagerRuntime {
    // AIHeroClient::GetRuneManager virtual getter. IDA 13337:
    // /liveclientdata/activeplayerrunes calls localPlayer->vfunc[0x808],
    // then reads the returned manager at the RuneManagerLayout offsets below.
    constexpr auto GetRuneManagerVFunc = 0x808;
} // namespace RuneManagerRuntime

namespace RuneManagerLayout {
    // Manager fields verified from sub_70EE80/sub_711C60 on IDA 13337.
    constexpr auto PrimaryRuneTree = 0x198;   // rune tree data pointer
    constexpr auto SecondaryRuneTree = 0x1E8; // rune tree data pointer
    constexpr auto RuneEntriesBegin = 0x230;  // std::vector<RuneEntry> begin
    constexpr auto RuneEntriesEnd = 0x238;    // std::vector<RuneEntry> end
    constexpr auto RuneEntriesCapacityEnd = 0x240;
} // namespace RuneManagerLayout

namespace RuneEntryLayout {
    // sub_70EE80 walks manager+[0x230,0x238) in 0x50-byte records and treats
    // the first qword in each record as the BasePerk/Rune data pointer.
    constexpr auto Stride = 0x50;
    constexpr auto RuneData = 0x00;
} // namespace RuneEntryLayout

namespace RuneDataLayout {
    // sub_716180 builds rune JSON: id at +0x08, display/raw strings at +0x20/+0x30.
    constexpr auto Id = 0x08;
    constexpr auto DisplayName = 0x20;
    constexpr auto Description = 0x30;
} // namespace RuneDataLayout

namespace RuneTreeDataLayout {
    // sub_716260 builds primary/secondary tree JSON: id at +0x00,
    // display/raw strings at +0x18/+0x28.
    constexpr auto Id = 0x00;
    constexpr auto DisplayName = 0x18;
    constexpr auto Description = 0x28;
} // namespace RuneTreeDataLayout

namespace SpellBookLayout {
    constexpr auto Owner = 0x08;        // SpellBookClient -> owner AIBaseClient
    constexpr auto CasterNetId = 0xA8;  // SpellBookClient caster network id
    constexpr auto ActiveSlot = 0xB4;   // Current/last cast slot used by stop-cast
    constexpr auto SpellSlotArray = 0xAE0;
} // namespace SpellBookLayout

    /*namespace HookSignatures {
        inline constexpr const char* OnMissileCreate = "48 89 5C 24 ? 48 89 6C 24 ? 56 57 41 56 48 83 EC 50 48 8B F1";
        inline constexpr const char* OnMissileDelete = "48 89 5C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC 20 8B 91";
    }*/ // namespace HookSignatures

    namespace Hooks {
        constexpr uintptr_t OnIntegerPropertyChange = 0x376250; //OnIntegerPropertyChange
        // ClientMainLoop is intentionally 0: NIGHTSHARP_ENABLE_CLIENTMAINLOOP_HOOK
        // is off by default and CoreHook::IsInlineAllowed() skips RVA == 0.
        constexpr uintptr_t ClientMainLoop          = 0x0;
        // Central animation request wrapper. It stores the AIBaseClient
        // callback receiver at *(RCX+0x08) and accepts an RDX string-view.
        constexpr uintptr_t OnPlayAnimationWrapper  = 0xE2D810;
        constexpr uintptr_t CreateClientEffect      = 0x90B240;

        constexpr uintptr_t DispatchEvent           = 0x4B8940;
        constexpr uintptr_t OnTeleport              = 0x72ECF0; //OnTeleport
        constexpr uintptr_t Hud_OnDisconnect        = 0x6D1200;
        constexpr uintptr_t ProcessCastSpell        = ControlRuntime::ProcessCastSpell;
        constexpr uintptr_t OnUpdateChargeableSpell = ControlRuntime::UpdateChargeableSpell;
        constexpr uintptr_t OnBuffAdd               = 0x21DA30; //OnBuffGain
        constexpr uintptr_t OnBuffRemove            = 0x21DB50; //OnBuffLose
        constexpr uintptr_t OnBuffUpdate            = 0x21D5B0;
        constexpr uintptr_t OnBuffGain              = 0xC1F650;
        // AssignNetworkId: writes networkId to obj+0xCC, inserts into
        // ObjectManager tree, then calls post-init vfunc. Hook this instead
        // of the raw tree-insert so the object is fully ready when event fires.
        //   RCX = GameObject*, RDX = networkId (uint32_t).
        // IDA sig: 40 56 48 83 EC ? 45 33 C9
        constexpr uintptr_t OnCreate                = 0x563E80; //AssignNetworkId
        constexpr uintptr_t OnMissileCreate         = 0x970190;
        // sub_28EFE0(GameObject* this) fires GameEventId.OnDelete (event ID = 0)
        // via sub_4B87B0(eventObj, 0, &data). RCX = GameObject* (this pointer).
        // IDA sig: 48 89 5C 24 ? 55 56 57 48 83 EC 40 48 8B 01 48 8D 54 24
        constexpr uintptr_t OnDelete                = 0x28EFE0; //OnDelete
        constexpr uintptr_t OnMissileDelete         = 0x958450;
        constexpr uintptr_t OnDamage                = 0x29A6F0;
        constexpr uintptr_t OnDoCast                = 0x9781A0;
        constexpr uintptr_t OnFinishCast            = 0x29B0E0;
        constexpr uintptr_t OnGameUpdate            = 0x54FC70; //OnUpdate
        constexpr uintptr_t OnLevelUp               = 0x28FC40;
        // Packet callback for ids 0x11D/0x1F1. RCX is AIBaseClient and the
        // internal animation-name view is at RDX+0x18.
        constexpr uintptr_t OnPlayAnimation         = 0x295080;
        constexpr uintptr_t OnProcessSpell          = 0x97E580; //OnProcessSpellCast
        constexpr uintptr_t OnSpellImpact           = 0x9768E0;
        constexpr uintptr_t OnStopCast              = 0x97E890;
        constexpr uintptr_t OnStealth               = 0x299C70;
        constexpr uintptr_t OnSurrender             = 0xE8D423;
        constexpr uintptr_t ProcessWorldEvent       = 0x6D2250;

        // ─────────────────────────────────────────────────────────────────
        // EnsoulSharp-derived event hooks (verified against EnsoulSharp.dll
        // via ILSpy + cross-referenced in IDB 13337). These map the managed
        // .NET wrapper events to their underlying native game functions.
        // ─────────────────────────────────────────────────────────────────
        //
        // OnNewPath  (EnsoulSharp: AIBaseClient.OnNewPath)
        //   sub_562E60 is the common path packet consumer:
        //     RCX    = AIBaseClient*
        //     RDX    = StlArray<Vector3f>*
        //     R8D    = waypoint count
        //     R9     = path-state payload
        //     stack0 = isDash (0 normal path, 1 dash path)
        //   It calls sub_271720 -> sub_300890 after resolving the movement
        //   component. Hooking sub_300890 directly loses the GameObject
        //   sender and exposes a different seven-argument ABI.
        constexpr uintptr_t OnNewPath               = 0x5646E0;
        //
        // OnTeleport (EnsoulSharp: AttackableUnit.OnTeleport)
        //   Native signature: void(AttackableUnit*, StlString* recallType,
        //                           StlString* recallName)
        //   sub_731480 reads the recall script names ("Recall",
        //   "RecallImproved", "OdinRecall", "OdinRecallImproved") and updates
        //   the unit's teleport/recall state. Same routine as FOWRecall above.
        //   (On legacy EnsoulSharp builds without teleport-pad support this is
        //   purely the recall/back state handler.)
        //constexpr uintptr_t OnTeleport              = FOWRecall; // 0x731480
        //
        // OnIntegerPropertyChange (EnsoulSharp: GameObject.OnIntegerPropertyChange)
        //   Native signature: void(GameObject*, sbyte* propertyName,
        //                           int oldValue, int newValue)
        //   Not a standalone function — EnsoulSharp (like EloBuddy's
        //   EventHandler<62>) fires this from inside the CRepl32Info packet
        //   update handler after each replicated integer property changes.
        //   Hook CRepl32InfoUpdatePacket and diff the replicated int fields.
        //constexpr uintptr_t OnIntegerPropertyChange = CRepl32InfoUpdatePacket; // 0x37AC50
    }



    // =========================================================================
    // SpellCastInfo layout — read by OnStopCast / OnFinishCast / channel poller
    // =========================================================================
    // Obtained at runtime as:  *( hero + SpellRuntime::ActiveSpellCast )
    // (which resolves to  hero->spellBook->activeSpellCast  -- a pointer that
    // is null when the hero is NOT currently casting anything).
    //
    // Field offsets below match build 26.6; re-verify via IDA if it changes.
    // Use `GetSpellCastInfo` (ControlRuntime) as the canonical getter when you
    // want the SpellCastInfo for a specific slot instead of the active cast.
    // Two underlying structs share these field names:
    //   * Per-slot SpellCastInfo (from `GetSpellCastInfo`): 408-byte (0x198)
    //     entries packed in `hero[+0x4050..+0x4058]`. Only fields with
    //     offset < 0x198 are valid here (i.e. all except CasterNetId).
    //   * Active SpellCast (from `hero + ActiveSpellCast`): a fully
    //     populated >= 0x300-byte struct allocated only while a cast is in
    //     flight. ALL the offsets below apply, including `CasterNetId`.
    // Verified 26.6:
    //   * `GetSpellCastInfo` computes
    //   `(a4+0x4058 - a4+0x4050) / 408` to count slots and returns
    //   `array_begin + 408 * slot`, confirming the 0x198 stride.
    //   * process-spell setup writes caster position to SpellCastInfo+0xD0
    //     and the decrypted cast/end position to SpellCastInfo+0xDC.
    namespace SpellCastInfoLayout {
        constexpr auto SpellSlot     = 0x08;   // uint8  0..3 = QWER, 4..5 = D/F, 64=attack
        constexpr auto State         = 0x0C;   // uint32 enum (Ready/Cast/Channel/Finished)
        constexpr auto StartTime     = 0x28;   // float  game time when cast started
        constexpr auto EndTime       = 0x2C;   // float  game time the cast finishes
        constexpr auto ChannelStart  = 0x30;   // float  0 when not channeled
        constexpr auto ChannelEnd    = 0x34;   // float  0 when not channeled
        constexpr auto StartPosition = 0xD0;   // Vec3   cast start/caster position
        constexpr auto EndPosition   = 0xDC;   // Vec3   cast end/cast position
        constexpr auto TargetNetId   = 0x138;  // uint32 primary target (0xFFFFFFFF if unit-less)
        constexpr auto CasterNetId   = 0x2DC;  // uint32 caster net id  (active-cast only - per-slot stride is 0x198)
    } // namespace SpellCastInfoLayout

    // =========================================================================
    // AIBaseClient layout — REMOVED (fully covered by `All` / `AttackableUnit`
    //                                / `AIHeroClient` below).
    // =========================================================================
    // The old `AIBaseClientLayout` namespace was a Nightsharp-specific blob
    // that mixed fields from three different object layers (base object,
    // attackable unit, champion). Its values have been folded into the
    // canonical verified namespaces further down:
    //
    //     AIBaseClientLayout::NetId       -> All::NetId         (0xCC)
    //     AIBaseClientLayout::Team        -> All::Team          (0x259, uint8)
    //     AIBaseClientLayout::Position    -> All::Position      (0x25C)
    //     AIBaseClientLayout::IsDead      -> All::Dead          (0x250)
    //     AIBaseClientLayout::Health      -> AttackableUnit::HP (0x1080)
    //     AIBaseClientLayout::MaxHealth   -> AttackableUnit::MaxHP (0x10A8)
    //     AIBaseClientLayout::Mana        -> AIHeroClient::MP     (0x360)
    //     AIBaseClientLayout::MaxMana     -> AIHeroClient::MaxMP  (0x388)
    //     AIBaseClientLayout::Experience  -> AIHeroClient::Exp    (0x4D28)
    //     AIBaseClientLayout::LevelRef    -> AIHeroClient::LevelRef (0x4D50)
    //
    // The only in-tree consumer was `CheckDeathForHero` in CoreEventHook.h;
    // it now reads `Offset::All::Dead` directly. Any downstream code that
    // still references `AIBaseClientLayout::*` needs to be migrated to the
    // verified namespaces.

    // AiManager field offsets are relative to the inner pointer returned by
    // NavGridRuntime::GetAiManager (RVA 0x285900). IDA confirms that function
    // decodes hero + 0x4230 and dereferences wrapper + 0x10 before returning.
    namespace AiManager
    {
        constexpr auto AiManager = 0x4230;
        constexpr auto CurrentSegment = 0x320;
        constexpr auto DashSpeed = 0x360;
        constexpr auto IsDashing = 0x384;
        constexpr auto IsMoving = 0x31C;
        constexpr auto MoveVec3 = 0x480;
        constexpr auto NavArray = 0x348;
        constexpr auto ObjectOffset = 0x4230;
        constexpr auto PathState = 0x320;
        constexpr auto SegmentsCount = 0x350;
        constexpr auto ServerPos = 0x474;
        constexpr auto StartPath = 0x330;
        constexpr auto TargetPos = 0x34;
        constexpr auto TargetPosition = 0x33C;
        constexpr auto Velocity = 0x318;
    }
    // =========================================================================
    // VERIFIED offset block — imported from old source/core/Offsets.generated.h
    // =========================================================================
    // These namespaces mirror the ones that shipped with the legacy
    // (pre-hook) NightSharp source tree. The user confirmed the values are
    // still correct on the current LoL build, so the full-core rewrite that
    // sits on top of CoreEventHook.h can consume them as-is.
    //
    // Any offset that already existed above (e.g. SpellCastInfoLayout) was
    // kept untouched so the hook code that already reads through it keeps
    // working. The previously-present `AIBaseClientLayout` namespace was
    // retired entirely — every field it defined is covered by the per-class
    // layouts (`All` / `AttackableUnit` / `AIHeroClient`) below.

    // ── NavGrid internals ─────────────────────────────────────────────────
    namespace NavGridLayout {
        constexpr auto NavGridMgr      = 0x8;
        constexpr auto MinX            = 0xEC;
        constexpr auto MinZ            = 0xF4;
        constexpr auto MaxX            = 0xF8;
        constexpr auto MaxZ            = 0x100;
        constexpr auto Data            = 0x110;
        constexpr auto Width           = 0x708;
        constexpr auto Height          = 0x70C;
        constexpr auto Scale           = 0x710;
        constexpr auto InverseScale    = 0x714;
        constexpr auto GrassRegions    = 0x158;
        constexpr auto CellSize        = 0x10;
        constexpr auto ByteFlagData    = 0x150;  // METHOD 1: 1 byte/cell flag array, fast bush check
    } // namespace NavGridLayout

    namespace NavGridFlags {
        constexpr auto FlagWall        = 0x1;
        constexpr auto FlagNoWalk      = 0x2;
        constexpr auto FlagBrush       = 0xC00;
        constexpr auto FlagSpecial     = 0x1000;
    } // namespace NavGridFlags

    namespace NavGridCellLayout {
        constexpr auto CellOverlay       = 0x00;
        constexpr auto CellFlags         = 0x08;
        constexpr auto OverlayFlagsOff   = 0x06;
        constexpr auto CellStride        = 16;
        constexpr auto CELL_WALL         = 0x0002;
        constexpr auto CELL_BRUSH        = 0x0004;
        constexpr auto CELL_WATER        = 0x0010;
        constexpr auto CELL_BUILDING     = 0x0040;
        constexpr auto CELL_VISION       = 0x0080;
        constexpr auto CELL_PASSABILITY  = 0x0C00;
        constexpr auto HalfCellSize      = 0x0718;
    } // namespace NavGridCellLayout

    // ── Spell chain: Slot → Info → Data → Resource ───────────────────────
    namespace SpellSlotLayout {
        constexpr auto SlotLevel             = 0x28;
        constexpr auto SlotLevelAlt          = 0x28;
        constexpr auto SlotCooldown          = 0x80;
        constexpr auto SlotTotalCd           = 0x88;
        constexpr auto SlotCooldownExpires   = 0x30;
        constexpr auto SlotChargeTimer       = 0x68;
        constexpr auto SlotChargeCooldownDuration = 0x6C;
        constexpr auto SlotCooldownDuration  = 0x74;
        constexpr auto SlotStacks            = 0x5C;
        constexpr auto SlotMaxStacks         = 0x64;
        constexpr auto SlotActiveSpellCast   = 0x118;
        constexpr auto SlotSpellInstanceVars = 0x108;
        constexpr auto SlotSpellNameHash     = 0x110;
        constexpr auto SlotSpellInfo         = 0x128;
        constexpr auto SlotSpellInput        = 0x130;
    } // namespace SpellSlotLayout

    namespace SpellDataResourceNameLayout {
        constexpr auto SpellNameStr = 0x28;
        constexpr auto SpellNameCap = 0x40;
    } // namespace SpellDataResourceNameLayout

    namespace SpellInputLayout {
        // SpellInput is not a writable cast-argument buffer. The old
        // +0x14/+0x18/+0x24 staging layout was disproved by runtime capture.
        // These three offsets remain for decoding the separate
        // ProcessCastSpell event request in CoreEvents; never write them to
        // the live object stored at SpellSlot+0x130.
        constexpr auto InputTargetNetId = 0x14;
        constexpr auto InputStartPos    = 0x18;
        constexpr auto InputEndPos      = 0x24;

        // On the live SpellInput object, +0x28 is the spell-name key.
        constexpr auto SpellNameKey = 0x28;
    } // namespace SpellInputLayout

    namespace ProcessCastSpellRequestLayout {
        // IDA 13339: sub_292310 calls sub_910A80(parsedCastInfo, request + 0x18).
        // sub_910A80 decodes the request byte at +0xC4 into parsedCastInfo+0x154,
        // the same slot field used by OnProcessSpell.
        constexpr auto EncodedSlot = 0xC4;
        constexpr auto DecodeTable = 0x1A6E320;
    } // namespace ProcessCastSpellRequestLayout

    namespace SpellInfoLayout {
        // CE/hotfix verified: Info+0x08 = SpellData ptr, Info+0x60 = owner slot/backref.
        constexpr auto InfoSpellData    = 0x8;
        constexpr auto InfoLevelOrFlag  = 0x14;
        constexpr auto InfoOwnerSlot    = 0x60;
        constexpr auto SpellInfoNamePtr = 0x28;
    } // namespace SpellInfoLayout

    namespace SpellDataLayout {
        constexpr auto DataSpellName = 0x80;
        constexpr auto DataManaCost  = 0x5F4;
        constexpr auto DataResource  = 0x60;
        constexpr auto ResourceData  = 0x60;
        constexpr auto ResourceName  = 0x28;
    } // namespace SpellDataLayout

    namespace SpellDataResourceLayout {
        constexpr auto DataResourceBase = 0x60;
        // IDA 13339: sub_3544A0 registers the SPELLPARAM enum:
        //   CASTRANGE    = 0x0F
        //   CASTRADIUS   = 0x19
        //   LINEWIDTH    = 0x1D
        //   CASTFRAME    = 0x26
        //   MISSILESPEED = 0x27
        // The serializer at sub_954850 confirms CastRange at +0x478.
        // LineWidth/MissileSpeed direct aliases are kept behind sanity
        // checks; CastRadius is not exposed as a verified direct field yet.
        constexpr auto ResCastRange     = 0x478;
        constexpr auto ResMissileSpeed  = 0x518;
        constexpr auto ResLineWidth     = 0x568;
        constexpr auto ResMaxAmmo       = 0x3C0;
        // IDA 13339: SpellTypeClassify reads resource+0x2F.
        constexpr auto ResCastType      = 0x2F;
        constexpr auto ResCastRangeDisplayOverride = 0x548;
        constexpr auto ResMissileSpec   = 0x508;
        constexpr auto ResScriptName    = 0x80;
        // IDA 13337: sub_96CB90(spellDataResource, level) returns
        // *(resource + 0x6C8 + 4 * max(level - 1, 0)).
        constexpr auto ResCooldownTime  = 0x6C8;
        constexpr auto ResAmmoRecharge  = 0x408;
        constexpr auto ResImgIconName   = 0x2A0;
    } // namespace SpellDataResourceLayout

    // ── Extended SpellCastInfo layout (event-received version) ───────────
    // IDA 13337 verification (Jun/2026): sub_959810 copies event CastInfo
    // field-by-field into missile CastInfoBase (0x2C0). The position fields
    // (StartPos/EndPos/CastPos) use the SAME offsets as SpellCastInfoLayout
    // and MissileClient::CastInfoBase. Verified via sub_9700D0 (OnMissileCreate).
    // Other fields (Slot, IsSpell, etc.) differ from SpellCastInfoLayout.
    namespace SpellCastInfoEventLayout {
        constexpr auto SpellData        = 0x0;
        constexpr auto SrcIndex         = 0x98;
        constexpr auto TargetIndex      = 0x9C;
        constexpr auto StartPos         = 0xD0;
        constexpr auto EndPos           = 0xDC;
        constexpr auto CastPos          = 0xE8;
        constexpr auto CastDelay        = 0x118;
        constexpr auto IsSpell          = 0x134;
        constexpr auto IsSpecialAttack  = 0x13E;
        constexpr auto IsAuto           = 0x141;
        constexpr auto Slot             = 0x14C;
        // IDA 13337 hotfix path sub_982120 reads the same logical fields here.
        constexpr auto IsSpecialAttackAlt = 0x149;
        constexpr auto IsAutoAlt          = 0x14A;
        constexpr auto SlotAlt            = 0x154;
    } // namespace SpellCastInfoEventLayout

    // (EventSpellCastInfoLayout removed Apr 25/2026 - duplicate of SpellCastInfoEventLayout above; old schema)

    // ── Inventory / items ────────────────────────────────────────────────
    // NEW inventory layout (restructured in this build):
    //   hero + InventoryComponent → component object
    //   component + SlotArray     → 39 slot pointers, 8 bytes each
    //   slot + ItemNode           → item node ptr (null = empty)
    //   node + ItemInfo           → info ptr
    //   info + DataItemId         → item ID (XOR-encrypted)
    // Old chain (slot+0x10 → info+0x38 → id+0xB4) no longer valid.
    namespace ItemRuntime {
        constexpr auto InventoryComponent = 0x4E08;
        constexpr auto SlotArray          = 0x50;
        constexpr auto SlotCount          = 39;
        constexpr auto ItemNode           = 0x10;
        constexpr auto ItemInfo           = 0x00;
        // CE/ReClass 2026-06-23: current item info stores the id as an
        // inline ASCII string here, e.g. slot0 item info +0x08 == "3003".
        constexpr auto DataItemIdString   = 0x08;
        constexpr auto DataItemId         = DataItemIdString;
        constexpr auto DataAbilityHaste   = 0x160;
        constexpr auto DataHealth         = 0x164;
        constexpr auto DataArmor          = 0x19C;
        constexpr auto DataMR             = 0x1BC;
        constexpr auto DataAD             = 0x1D8;
        constexpr auto DataAP             = 0x1E0;
        constexpr auto DataAtkSpeedMult   = 0x20C;
    } // namespace ItemRuntime

    // ── Object type / classification ─────────────────────────────────────
    // Native object-classification offsets removed; object typing now uses
    // manager lists plus lightweight replicated fields/name scans.

    namespace MinionClassRuntime {
        // IDA 13337:
        //   IsLaneMinion    sub_30D690: movzx eax, byte ptr [rcx+4CB9h]
        //   IsJungleMonster sub_30D420: cmp byte ptr [rcx+4CB9h], 2
        constexpr auto TypeOffset        = 0x4CB9;
        constexpr auto Unset             = 0x0;
        constexpr auto Pet               = 0x1;
        constexpr auto JungleMonster     = 0x2;
        constexpr auto TeamMinion        = 0x3;
        constexpr auto MeleeLaneMinion   = 0x4;
        constexpr auto RangedLaneMinion  = 0x5;
        constexpr auto SiegeLaneMinion   = 0x6;
        constexpr auto SuperLaneMinion   = 0x7;
        constexpr auto FollowTargetNetId = 0x6A8;
    } // namespace MinionClassRuntime

    namespace JungleTypeRuntime {
        constexpr auto TypeOffset = 0x4CB9;
        constexpr uint8_t SmallJungle      = 0x0;
        constexpr uint8_t NormalJungle      = 0x1;
        constexpr uint8_t LargeJungle      = 0x2;
    
        constexpr uint8_t Dragon        = 0x3;
        constexpr uint8_t RiftHerald    = 0x4;
        constexpr uint8_t ElderDragon   = 0x5;
        constexpr uint8_t Baron         = 0x6;
        constexpr uint8_t Horde         = 0x7;
        constexpr uint8_t Other         = 0x8;
    } // namespace JungleTypeRuntime

    // UnitQueryRuntime removed Apr 25/2026:
    //   IsTargetableByUnit -> AttackableUnit::IsTargetable (memory bool, no syscall)
    //   HasBuffOfType      -> CoreBuffs::HasBuffType / HasActiveBuffType (read BuffEntry.Type field)
    //   GetGoldRedirectTgt -> niche (gangplank passive), no consumer

    // ── Animation system ─────────────────────────────────────────────────
    namespace AnimationLayout {
        constexpr auto CharacterData         = 0x4070;
        constexpr auto Component             = 0x4488;
        constexpr auto Queue                 = 0x4960;
        constexpr auto QueueEnd              = 0x4968;
        constexpr auto QueueCapacityEnd      = 0x4970;
        constexpr auto SkinIndex             = 0x146;
        constexpr auto CharacterDataResource = 0x60;
        constexpr auto VariantEntries        = 0x240;
        constexpr auto VariantEntryCount     = 0x248;
        constexpr auto FallbackNamePtr       = 0x250;
        constexpr auto FallbackState         = 0x260;
        constexpr auto VariantEntryStride    = 0xB0;
        constexpr auto VariantNamePtr        = 0x8;
        constexpr auto VariantState          = 0x18;
    } // namespace AnimationLayout

    // ── Per-class object layouts ─────────────────────────────────────────
    // `All` = fields present on EVERY game object (minion, hero, turret…).
    // `AttackableUnit` = anything with an HP bar (adds on top of `All`).
    // `AIHeroClient` = champion-only fields (adds on top of AttackableUnit).
    //
    // These three namespaces are the CANONICAL source for per-object field
    // offsets. `CheckDeathForHero` in CoreEventHook.h reads `All::Dead`
    // (0x250) directly; any new hook or SDK consumer should use the same
    // values rather than inventing a private layout.
    namespace All {
        constexpr auto Index               = 0x20;    // GetID/sub_371690 -> obj+0x20; slot-array lookup sub_54EF60 keys (index & 0xFFFF) and validates obj+0x20
        constexpr auto Team                = 0x259;  // byte team index
        constexpr auto Name                = 0x68;
        // NetworkId is a DISTINCT field from Index. Verified on 13337:
        //   - sub_562610 writes the network id to obj+0xCC and registers it into the
        //     ObjectManager network-id red-black tree via sub_560700.
        //   - sub_30E730 is a clean getter: returns *(obj+0xCC).
        //   - the object destructor sub_2F6930 reads *(obj+0xCC) to deregister from
        //     the netId tree at [qword_1E76E40+0x48].
        //   - EnsoulSharp.dll GameObject.GetPtr()/CreateObjectFromPointer read BOTH
        //     GetID(ptr) (-> m_index) and GetNetworkID(ptr) (-> m_networkId) as
        //     separate fields, confirming Index != NetworkId.
        // Previous build aliased NetId = Index (0x20) which was wrong.
        constexpr auto NetId               = 0xCC;    // obj+0xCC -> object network id (GetNetworkID)
        constexpr auto NetworkId           = NetId;
        constexpr auto Dead                = 0x250;
        constexpr auto Position            = 0x25C;
        constexpr auto Visible             = 0x308;   // GameObject visible flag.
        constexpr auto IsInvulnerable      = 0x5A0;   // Legacy/debug only; IsInvulnerable is native/buff logic, not this byte.
        // RecallState (legacy 0xF48) was discovered to be a std::vector data
        // pointer on 26.6 (sub_9F18FE constructor / sub_9F18A0 destructor pair
        // free `[obj+0xF48]` when `[obj+0xF54] >= 0`). The real recall
        // channel state should be derived from SpellRuntime::ActiveSpellCast.
        // Keeping the constant for ABI compatibility but new code MUST NOT
        // read it.
        constexpr auto RecallState         = 0xF48;
        constexpr auto Radius              = 0x6F8;
        constexpr auto CharacterData       = 0x4078;
        constexpr auto CharacterName       = 0x4370;
        constexpr auto Direction           = 0x21D8;  // legacy alias; do not read Vec3 directly from this field
        constexpr auto DirectionComponent  = 0x1288;  // object + component -> vfunc +0xA8 -> holder
        constexpr auto DirectionVFunc      = 0xA8;
        constexpr auto DirectionVector     = 0x20;    // holder + 0x20 -> facing direction Vec3
        constexpr auto EffectEmitterHandle = 0x258;
        constexpr auto MissileClientHandle = 0x2D8;
        constexpr auto ItemList            = 0x4E08;  // = InventoryComponent
    } // namespace All

    namespace AttackableUnit {
        constexpr auto HP              = 0x1080;
        constexpr auto MaxHP           = 0x10A8;
        constexpr auto HPMaxPenalty    = 0x10D0;
        constexpr auto AllShield       = 0x1120;
        constexpr auto PhysicalShield  = 0x1148;
        constexpr auto MagicalShield   = 0x1170;
        constexpr auto ChampSpecific   = 0x1198;
        constexpr auto InHealAllied    = 0x11C0;
        constexpr auto InHealEnemy     = 0x11E8;
        constexpr auto InDamage        = 0x1210;
        constexpr auto StopShieldFade  = 0x1238;
        constexpr auto IsTargetable    = 0xED0;
        constexpr auto TargetableFlags = 0xEF8;
        constexpr auto ActionState1    = 0x1470;
        constexpr auto ActionState2    = 0x14C8;
    } // namespace AttackableUnit

    namespace AIHeroClient {
        constexpr auto MP                       = 0x360;
        constexpr auto MaxMP                    = 0x388;
        constexpr auto PAR                      = 0xE00;
        constexpr auto MaxPAR                   = 0xE28;
        constexpr auto SAR                      = 0x108;
        constexpr auto MaxSAR                   = 0x130;
        constexpr auto PhysDmgPercent           = 0xE78;
        constexpr auto MagicDmgPercent          = 0xEA0;
        constexpr auto AbilityHaste             = 0x1BC0;
        constexpr auto FlatPhysicalDmgMod       = 0x1D00;
        constexpr auto AttackSpeedMod           = 0x1E68;
        constexpr auto PercentAttackSpeedMod    = 0x1E90;
        constexpr auto BaseAttackDamage         = 0x1F08;
        constexpr auto BaseAtkDmgSansScale      = 0x1F30;
        constexpr auto FlatBaseAtkDmgMod        = 0x1F58;
        constexpr auto PercentBaseAtkDmgMod     = 0x1F80;
        constexpr auto BaseAbilityDamage        = 0x1FA8;
        constexpr auto CritDamageMultiplier     = 0x1FD0;
        constexpr auto Dodge                    = 0x2020;
        constexpr auto Crit                     = 0x2048;
        constexpr auto Armor                    = 0x2098;
        constexpr auto BonusArmor               = 0x20C0;
        constexpr auto SpellBlock               = 0x20E8;
        constexpr auto BonusSpellBlock          = 0x2110;
        constexpr auto HPRegenRate              = 0x2138;
        constexpr auto BaseHPRegenRate          = 0x2160;
        constexpr auto MoveSpeed                = 0x2188;
        constexpr auto AttackRange              = 0x21D8;
        constexpr auto FlatArmorPen             = 0x2250;
        constexpr auto PhysicalLethality        = 0x2278;
        constexpr auto PercentArmorPen          = 0x22A0;
        constexpr auto PercentBonusArmorPen     = 0x22C8;
        constexpr auto FlatMagicPen             = 0x2340;
        constexpr auto MagicLethality           = 0x2368;
        constexpr auto PercentMagicPen          = 0x2390;
        constexpr auto PercentBonusMagicPen     = 0x23B8;
        constexpr auto PercentLifeSteal         = 0x23E0;
        constexpr auto PercentSpellVamp         = 0x2408;
        constexpr auto PercentOmnivamp          = 0x2430;
        constexpr auto PercentCCReduction       = 0x24A8;
        constexpr auto FlatBaseAttackSpeedMod   = 0x25E8;
        constexpr auto Gold                     = 0x2868;
        constexpr auto GoldTotal                = 0x2890;
        constexpr auto Exp                      = 0x4D38;
        constexpr auto LevelRef                 = 0x4D60;
        constexpr auto LevelUpPoints            = 0x4D88;
        // Community reverse confirmation + IDA 13337 parity path:
        // AIHeroClient::GetRuneManager returns the same manager consumed by
        // /liveclientdata/activeplayerrunes. The manager layout is documented
        // in RuneManagerLayout above.
        constexpr auto RuneManager              = 0x50E8;
        constexpr auto VisionScore              = 0x5568;
        constexpr auto ShutdownValue            = 0x5590;
        constexpr auto BaseGoldOnDeath          = 0x55B8;
        constexpr auto NeutralMinionsKilled     = 0x55E0;
    } // namespace AIHeroClient

    namespace MissileClient {
        // IDA 13337:
        // OnMissileCreate (sub_93ADA0) copies the network spell payload into
        // object + 0x2C0 via sub_923450. The copied payload is the source for
        // SData, names, caster/target indexes and trajectory vectors used by
        // missile tracking / evade.
        constexpr auto CastInfoBase  = 0x2C0;
        constexpr auto SpellDataPtr  = CastInfoBase + 0x00;
        constexpr auto SpellName     = CastInfoBase + 0x20;
        constexpr auto MissileName   = CastInfoBase + 0x48;
        constexpr auto TargetIndex   = CastInfoBase + 0x9C;
        constexpr auto CasterIndex   = CastInfoBase + 0xA0;
        constexpr auto MissileNetId  = CastInfoBase + 0xAC;
        constexpr auto ObjectNetId   = All::NetId;
        constexpr auto StartPos      = CastInfoBase + 0xD0;
        constexpr auto EndPos        = CastInfoBase + 0xDC;
        constexpr auto CastEndPos    = CastInfoBase + 0xE8;
        constexpr auto Position      = All::Position;
        constexpr auto StartTime     = 0x478;
    } // namespace MissileClient

    namespace MissileEventLayout {
        constexpr auto SpellData                 = 0x00;
        constexpr auto Slot                      = 0x08;
        constexpr auto SpellName                 = 0x20;
        constexpr auto MissileName               = 0x48;
        constexpr auto TargetIndex               = 0x9C;
        constexpr auto CreatePacketCasterIndex   = 0xA0;
        constexpr auto CreatePacketMissileNetId  = 0xAC;
        constexpr auto StartPos                  = 0xD0;
        constexpr auto EndPos                    = 0xDC;
        constexpr auto CastEndPos                = 0xE8;
    } // namespace MissileEventLayout


    // ── Hack / bypass plumbing (old source SDK used these for zoom,
    //    skin change, DirectInput hook, etc.) ──────────────────────────
    // GadgetRuntime::ThreadTrampoline removed Apr 25/2026 - replaced by CoreEventHook::FindSpoofGadget (dynamic byte-pattern 'FF 23' search)

    namespace DirectInputRuntime {
        constexpr auto KeyboardInput     = 0x11B3AE0;
        constexpr auto MouseInput        = 0x11B3CF0;
        constexpr auto KeyboardDevice    = 0x1F546B0;
        constexpr auto KeyboardBuffer    = 0x1F546C0;
        constexpr auto KeyboardCount     = 0x1F546E0;
        constexpr auto KeyboardFlag      = 0x1EF1358;
        constexpr auto MouseDevice       = 0x1F546B8;
        constexpr auto MouseBuffer       = 0x1F546C8;
        constexpr auto MouseCount        = 0x1F546F0;
        constexpr auto VT_GetDeviceState = 0x48;
        constexpr auto VT_GetDeviceData  = 0x50;
    } // namespace DirectInputRuntime

    namespace ZoomRuntime {
        constexpr auto CameraInstance   = 0x1E21698;  // 26.6 IDA: qword_1E21698, camera object global
        constexpr auto HudToCameraPtr   = 0x18;
        constexpr auto CurrentZoom      = 0x324;
        constexpr auto ZoomConfigPtr    = 0x3D0;
        constexpr auto ZoomFallbackPtr  = 0x310;
        constexpr auto DisableZoomClamp = 0x345;
        constexpr auto ZoomClampFlag    = 0x344;
        constexpr auto ZC_MinZoom       = 0x24;
        constexpr auto ZC_MaxZoom       = 0x28;
        constexpr auto HardcodedMaxZoom = 0x1961E8C;  // 26.6 (was 0x1920A24)
        constexpr auto ZoomEnableConfig = 0x1E8EBE0;  // 26.6 (was 0x1E49380)
    } // namespace ZoomRuntime

    namespace SkinRuntime {
        // Do not write CharacterDataSkinId directly for live skin changes.
        // The animation stack reads CharacterDataStack; direct CharacterData
        // writes can leave model resources out of sync and crash while moving.
        constexpr auto CharacterDataSkinId = 0x68;

        // R3nzSkin-style runtime path, updated from R3nzSkin 16.13.1
        // signature set / newoffset 16.13.
        // local + CharacterDataStack -> std::vector + base_skin.
        // base_skin.skin = stack + BaseSkin + CharacterStackSkin.
        // CharacterDataStackUpdate is sub_210F70(stack, change).
        // CharacterDataStackPush is sub_22B750(this, model, skin, ...).
        constexpr auto CharacterDataStack       = 0x4108;
        constexpr auto CharacterDataStackUpdate = 0x210F70;
        constexpr auto CharacterDataStackPush   = 0x22B750;
        constexpr auto CharacterDataStackBegin  = 0x00;
        constexpr auto CharacterDataStackEnd    = 0x08;
        constexpr auto CharacterDataStackCap    = 0x10;
        constexpr auto CharacterDataStackBaseSkin = 0x18;
        constexpr auto CharacterStackModelPtr   = 0x00;
        constexpr auto CharacterStackModelLen   = 0x08;
        constexpr auto CharacterStackModelCap   = 0x0C;
        constexpr auto CharacterStackSkin       = 0x20;
        constexpr auto CharacterStackGear       = 0x84;  // base_skin.gear (int8)

        // ── Encrypted skin id on the hero object (NEW May 2026) ──
        // Pattern `88 86 ?? ?? 00 00 48 89 45 ?? 0F B6 45 A8 88 86 ?? 13`
        // → mov [rsi+1334h], al at sub_28F2E0+0x545. Field is xor_value<int>.
        // R3nzSkin AIBaseCommon::change_skin writes BOTH this AND
        // base_skin.skin; writing only one of them desyncs animation
        // resources and can crash on the next animation tick.
        constexpr auto AiBaseSkinId = 0x1334;

        // Legacy fields kept for ABI/reference compatibility only.
        constexpr auto SkinNetID      = 0x1440;
        constexpr auto SkinName       = 0x1448;
        constexpr auto ModelName      = 0x1468;
        constexpr auto SkinChangeFlag = 0x1488;
        constexpr auto SkinParam1     = 0x148C;
        constexpr auto SkinParam2     = 0x1490;
    } // namespace SkinRuntime

    // ── Legacy alias namespaces (Phase 2 finalization Apr 26/2026) ───────────
    // Pre-Phase-1.7 SDK code under `sdk/GameObjects/*` and `sdk/Wrappers/*`
    // resolves offsets through `Offset::Global::*` and `Offset::Function::*`.
    // Phase 1.7 renamed the canonical namespaces (GameObjectsRuntime,
    // ObjectManagerRuntime, GameRuntime, ControlRuntime, ...) but never
    // ported the SDK call-sites. Rather than rewrite 50+ call-sites at once,
    // expose flat constexpr aliases here so the SDK chain compiles cleanly
    // while we migrate gradually.
    //
    // These are pure compile-time aliases (constexpr) — zero runtime cost,
    // single source of truth (the Runtime namespaces above).
    namespace Global {
        constexpr auto GameTime       = GameRuntime::GameTime;
        constexpr auto LocalPlayer    = GameObjectsRuntime::Player;
        constexpr auto ObjectManager  = GameObjectsRuntime::Objects;
        constexpr auto MinionManager  = GameObjectsRuntime::Minions;
        constexpr auto HeroManager    = GameObjectsRuntime::Heroes;
        constexpr auto MissileManager = GameObjectsRuntime::Missiles;
        constexpr auto TurretManager  = GameObjectsRuntime::Turrets;
        constexpr auto UnderMouse     = GameObjectsRuntime::UnderMouseObject;
        constexpr auto NetInstance    = GameRuntime::NetInstance;
        constexpr auto MissionInfoInstance = GameRuntime::MissionInfoInstance;
        constexpr auto HudInstance    = DrawingRuntime::HudInstance;
        constexpr auto Renderer       = DrawingRuntime::Renderer;
    } // namespace Global

    namespace Function {
        constexpr auto GetFirstObject    = ObjectManagerRuntime::GetFirstObject;
        constexpr auto GetFirstObjectAlt = ObjectManagerRuntime::GetFirstObject;  // verified old-source: alias of GetFirstObject
        constexpr auto GetNextObject     = ObjectManagerRuntime::GetNextObject;
        constexpr auto FindObject        = ObjectManagerRuntime::FindObject;
        constexpr auto WorldToScreen     = DrawingRuntime::WorldToScreen;
        constexpr auto GetPing           = GameRuntime::GetPing;
        constexpr auto PrintChat         = GameRuntime::PrintChat;
    } // namespace Function

} // namespace Offset
