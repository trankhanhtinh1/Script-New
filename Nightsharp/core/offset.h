#pragma once


namespace Offset {

namespace GameObjectsRuntime {
    constexpr auto Player = 0x1F2D608; //48 8B 05 ? ? ? ? 4C 8B DA 4C 8B C1
    constexpr auto TeamTable = 0x1EEFB18; //48 8B 05 ? ? ? ? 8B 54 88 ? 48 8D 05
    constexpr auto Objects = 0x1EEFB38;//48 8B 3D ? ? ? ? ? ? ? 80 7B ? 00
    constexpr auto Heroes = 0x1EEFC58; //48 8B 05 ? ? ? ? 45 33 FF 49 89 73
    constexpr auto Minions = 0x1EEFC98; //48 8B 0D ? ? ? ? E8 ? ? ? ? 33 FF 48 85 C0
    constexpr auto Missiles = 0x1EF3000; //4C 8B 35 ? ? ? ? 8B 9E || 48 8B 0D ? ? ? ? 48 89 75 || 48 8B 0D ? ? ? ? F3 0F 10 5D
    constexpr auto Turrets = 0x1EF7640; //48 8B 2D ? ? ? ? 85 D2 || 48 89 0D ? ? ? ? 83 78 || 48 8B 2D ? ? ? ? 49 8B D8
    constexpr auto UnderMouseObject = 0x1EF31F8; //48 8B 0D ? ? ? ? 88 44 24 ? 48 8B 45 || 48 8B 05 ? ? ? ? 48 8B F9 33 C9 48 8B DA
} // namespace GameObjectsRuntime

namespace VTable {
    constexpr auto GameObjectBoundingRadius = 0x130;
} // namespace VTable

// Module-relative addresses of the C++ vtables for structure object types.
// This is how the game itself distinguishes turrets / inhibitors / nexus:
// every instance of a type shares one vtable at obj+0x0, and particle/effect
// objects that merely contain "Turret"/"Nexus"/"Barracks" in their MODEL name
// have a COMPLETELY DIFFERENT vtable. Mirrors how EnsoulSharp classifies via
// runtime type. Confirmed against the game's own exact type-check functions in
// IDA (each does `obj->vtable[1]() == &classDescriptor`):
//   * IsBarracksDampenerClient = sub_2C51D0 → descriptor qword_1F32BA0,
//     factory sub_E07A00 → ctor sub_E0C8D0 → vtable 0x1ABF6F8.
// IMPORTANT (fixed Jul 2026): the inhibitor vtable is 0x1ABF6F8, NOT 0x1AC01B8.
// 0x1AC01B8 is the sibling `Barracks` class (descriptor qword_1F32A80, ctor
// sub_E0C300) — the MINION-SPAWN barracks that sits next to the nexus. Matching
// it drew the "inhibitor" at the minion spawn point (near fountain) instead of
// the destroyable crystal, which broke orbwalker targeting. Both classes share
// GetName@0x68 and GetPosition@0x25C, so only the vtable RVA needed correcting.
// NOTE: patch-specific — re-dump these RVAs when the client updates.
namespace StructureVTable {
    //constexpr auto AITurretClient         = 0x19F6120;
    //constexpr auto BarracksDampenerClient = 0x1ABF6F8; // real inhibitor (was 0x1AC01B8 = Barracks spawner)
    //constexpr auto HQClient               = 0x1AC2130;
} // namespace StructureVTable

    namespace ObjectManagerRuntime {
        constexpr auto ManagerListItems = 0x8;
        constexpr auto ManagerListSize = 0x10;
        // ReClass live 2026-07-08: MissileManager list entries are intrusive
        // nodes; node+0x28 is the MissileClient object. Hero/minion/turret
        // managers still store object pointers directly.
        constexpr auto MissileManagerNodeObject = 0x28;
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
        constexpr auto GetFirstObject = 0xA31D10; //E8 ? ? ? ? 48 6B D3 || E8 ? ? ? ? 48 8B C8 48 85 C0 74 ? ? ? ? 8B 49 || E8 ? ? ? ? 48 83 BB ? ? ? ? 00 ? ? ? 8B A9
        constexpr auto GetNextObject = 0x565B00; //E8 ? ? ? ? 48 8B D8 48 85 C0 0F 85 ? ? ? ? 0F 28 || E8 ? ? ? ? 48 8B ? 48 85 C0 0F 85 ? ? ? ? 44 0F
        constexpr auto FindObject = 0x5644C0; //E8 ? ? ? ? 0F 28 D6 48 8B D7 || E8 ? ? ? ? 48 85 C0 75 ? 84 DB || E8 ? ? ? ? 48 85 C0 74 ? 41 B0
    } // namespace ObjectManagerRuntime

namespace GameRuntime {
    constexpr auto GameTime = 0x1EFED50; //F3 0F 5C 35 ? ? ? ? 0F 28 F8 || F3 0F 11 3D ? ? ? ? 48 8B 49
    constexpr auto NetInstance = 0x1EEFB30; //48 8B 05 ? ? ? ? 48 8B F2 48 8B D9 48 85 C0 || 48 8B 05 ? ? ? ? 0F B6 15 ? ? ? ? 48 85 C0 41 0F 95 C1
    constexpr auto MissionInfoInstance = 0x1EEFC48; //48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8D 8F ? ? ? ? 89 47 ||48 8B 0D ? ? ? ? 48 8B 1D ? ? ? ? E8 ? ? ? ? 8B D0
    constexpr auto ChatViewController = 0x1F01F78; //48 8B 0D ? ? ? ? 0F B6 D8 48 85 C9 || 48 8B 05 ? ? ? ? 48 85 C0 74 ? 88 90
    constexpr auto ShopInstance = 0x1F01F80; //48 8B 0D ? ? ? ? E8 ? ? ? ? 84 C0 74 ? 0F B6 87 || 48 8B 0D ? ? ? ? 48 85 C9 74 ? E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? 48 85 DB
    constexpr auto OpenWindowsArray = 0x1FCCB78; //48 8B 3D ? ? ? ? 33 ED || 48 8B 05 ? ? ? ? 48 8B 4C C8 || 48 8B 05 ? ? ? ? 48 39 5C C8
    constexpr auto OpenWindowsCount = 0x1FCCB80; //89 35 ? ? ? ? 48 8B F9 || 8B 05 ? ? ? ? 4C 8B 4B || 89 15 ? ? ? ? ? ? ? 48 8B CB
    constexpr auto MySpellState = 0x1F1AC60; // 48 8B 0D ? ? ? ? 48 85 C9 0F 84 ? ? ? ? 49 8B D6 E8 ? ? ? ? B8 ? ? ? ? E9 ? ? ? ? 49 8B CE E8 ? ? ? ? B8 ? ? ? ? E9 ? ? ? ? 41 8B 4E
    // Cursor world position (Vec3: x,y as qword + z as dword = 12 bytes).
    // Verified on IDA 13337 (current dump): 4+ constructors use this as the
    // canonical cursor world pos source — sub_1F18A0 (UnderMouseObject struct),
    // sub_258980, sub_259140, sub_BF7D0, sub_BF810, sub_BF880 all copy
    // qword_1F76630 (x,y) + dword_1F76638 (z) as the cursor position.
    // Old 0x1F75578 was a 24-byte zero buffer — wrong since patch 15.x.
    constexpr auto CursorPosRaw = 0x1F8FD40; //F2 0F 10 05 ? ? ? ? F2 41 0F 11 40 ? 8B 05 ? ? ? ? 0F 10 44 24 || 48 8D 0D ? ? ? ? E8 ? ? ? ? 48 89 84 24 ? ? ? ? 4C 8B D0
    // Global MouseInput pointer. sub_5AC6F0 reads screen X/Y from +0x0C/+0x10.
    // Verified still valid on IDA 13337 (current dump): 3/4 pattern matches
    // (48 8B 0D ?? ?? ?? ?? E8 ...) resolve to qword_1ED9F70.
    constexpr auto MouseScreenVec2 = 0x1EF2FB0; //48 8B 0D ? ? ? ? 4C 8D 45 ? 48 8D 55 ? C7 45 ? 00 00 00 00 || 48 83 3D ? ? ? ? 00 0F 84 ? ? ? ? 8B 47
    constexpr auto GetPing = 0x6D9150; //E8 ? ? ? ? 8B F0 39 43 || E8 ? ? ? ? 8B C0 0F 57 C9 || E8 ? ? ? ? 44 0F B6 53 ? 4C 8D 05
    constexpr auto GetMapID = 0x282B60; //E8 ? ? ? ? 44 8B CB 48 8D 57 || E8 ? ? ? ? 8B D0 48 8D 4C 24 || E8 ? ? ? ? 83 F8 ? 75 ? C6 86
    // ChatViewController::DisplayChat dispatcher (sub_B62E90). Signature
    //   void __fastcall(void* chatContainer, const char* utf8, int flags)
    // Pre-game it queues the line; in-game it tail-calls sub_B62690 which
    // renders with channel/color flags. flags=0 is a plain system line.
    // (Old value 0x1158CB0 was wrong: that is only a UTF-8 truncation helper.)
    constexpr auto PrintChat = 0xB86450; //E8 ? ? ? ? 4C 8B C3 B2 || E8 ? ? ? ? 4C 8B C3 33 D2 48 8B CE || E8 ? ? ? ? E9 ? ? ? ? 4C 8B 15
    // 'this' container passed to PrintChat/DisplayChat; deref before the call.
    // IDA labels it NetInstance but it is distinct from GameRuntime::NetInstance.
    constexpr auto ChatMessageInstance = 0x1EF3240; //48 8B 1D ? ? ? ? 24 || 4C 8B 25 ? ? ? ? 75 || 48 8B 3D ? ? ? ? 48 8D 4C 24 ? 8B 1D
} // namespace GameRuntime

namespace MouseInputLayout {
    constexpr auto ScreenX = 0x0C;
    constexpr auto ScreenY = 0x10;
} // namespace MouseInputLayout

namespace DrawingRuntime {
    constexpr auto WorldToScreen = 0x133A050; //E8 ? ? ? ? 84 C0 75 ? F3 0F 10 97 || E8 ? ? ? ? F3 0F 10 5D ? 48 8D 45 || E8 ? ? ? ? 84 C0 75 ? 48 85 FF 75
    // qword_1ED9F68 points to the render/view state. Native W2S receives
    // qword_1ED9F68 + 0x2F8 as its first argument.
    // Verified: caller sub_3ACB60 passes (qword_1ED9F68 + 760) = +0x2F8.
    constexpr auto ViewProjectionRoot = 0x1EF2FA8; //4C 8B 2D ? ? ? ? 48 89 55 || 48 8B 0D ? ? ? ? 0F 57 C0 48 8B || 48 8B 0D ? ? ? ? 4C 8B 84 24
    constexpr auto WorldToScreenContextOffset = 0x2F8;
    // Hud root/controller object. Verified current dump: 488 xrefs.
    constexpr auto HudRoot = 0x1EEFCC0; //48 8B 0D ? ? ? ? 48 85 C9 75 ? 8B C5 || 48 8B 0D ? ? ? ? 48 85 C9 75 ? 33 C0
    constexpr auto HudInstance = 0x1EEFCC8; //48 8B 1D ? ? ? ? 41 8B 55 || 48 8B 0D ? ? ? ? 44 0F B6 EB || 48 8B 0D ? ? ? ? 8B 57 ? 48 8B 49
    // `qword_1EAED80` owns the published ScoreboardViewController interface.
    // IDA 13337: ScoreboardViewController::Setup (sub_E28270) writes its
    // secondary interface pointer to `[qword_1EAED80 + 0x128]`; the
    // destructor clears the same field. The fields behind that interface
    // (TeamScoresDefinitions / DragonTracker) still require runtime
    // verification and are deliberately not represented here.
    constexpr auto ScoreboardViewController = 0x128;
    constexpr auto ViewPort = 0x1F01EE0; //48 8B 0D ? ? ? ? 0F 28 D1 48 8B 5C 24 || 48 8B 1D ? ? ? ? 48 8B 49 ? E8 ? ? ? ? 0F 28 D0 48 8D 55
    constexpr auto ViewPort2 = 0x1FCE4F8; //48 89 1D ? ? ? ? EB ? 48 8B D8 || 48 C7 05 ? ? ? ? 00 00 00 00 48 8D 4B
    constexpr auto Renderer = 0x1FCE528; //48 89 35 ? ? ? ? 48 8B 8E || 4C 8B 3D ? ? ? ? 41 83 46 || 48 89 1D ? ? ? ? 48 8B 8F
    constexpr auto ViewProjOffset = 0x1FB81F0; //48 8D 0D ? ? ? ? 66 0F 7F 45 || 48 8D 0D ? ? ? ? ? ? ? 0F 11 84 24 || 48 8D 0D ? ? ? ? C7 45 ? 00 00 00 00
    // qword_1ED6E60: stats manager. Owns a std::map<uint32_t, vector<StatBlock*>>
    // at +0x30 keyed by team ID (100=ORDER, 200=CHAOS). Each StatBlock is 8128
    // bytes allocated by sub_2FDB70 and initialised by sub_2EA000.
    // Verified current dump: sub_2ECCF0 is the constructor (writes qword_1ED6E60
    // = this). sub_307E10 reads *(statsManager+0x30) tree, finds node by
    // teamId, double-dereferences vector begin to get StatBlock*.
    // sub_2EF5E0 (hero destructor) removes hero netId from the same tree.
    // Old global 0x1E9D180 had 0 xrefs — moved to 0x1ED6E60.
    constexpr auto StatsManager = 0x1EEFD00; //4C 8B 0D ? ? ? ? 8B 83 || 48 8B 1D ? ? ? ? 48 8D B0 || 48 8B 1D ? ? ? ? 48 8B CE FF 50
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

// All offsets below are RELATIVE struct field offsets (not RVAs). Current
// runtime verification: Hud+0x18 resolves the camera, Hud+0x28 resolves the
// input/cursor state, Hud+0x60 resolves user data, and Hud+0x68 resolves the
// targeting state. Camera+0x324 is the live 2250 zoom value; +0x310/+0x3D0
// resolve the same zoom-config object in the current session.
namespace DrawingMatrixRuntime {
    constexpr auto ViewMatrixRelative = 0x0;
    constexpr auto ProjMatrixRelative = 0x40;
} // namespace DrawingMatrixRuntime

// Renderer object layout. These are relative fields/vtable slots rather than
// patch-specific module RVAs, so they remain shared by the renderer readers
// and the internal D3D11 hook.
namespace D3D {
    // Verified via Renderer constructor sub_13BA0E0:
    //   qword_1FB4310 = renderer; *(renderer+0x1E0)=device;
    //   *(renderer+0x218)=swapChain; *(renderer+0x2A8/0x2AC)=screen W/H;
    //   *(renderer+0x2B0)=deviceContext.
    constexpr auto Renderer                  = DrawingRuntime::Renderer;
    constexpr auto SwapChainVtable           = 0x0;
    constexpr auto PresentVtableOffset       = 0x40;   // IDXGISwapChain::Present (8th vmethod)
    constexpr auto ResizeBuffersVtableOffset = 0x68;   // IDXGISwapChain::ResizeBuffers (13th vmethod)
    constexpr auto Device                    = 0x1E0;
    constexpr auto SwapChainDesc             = 0x200;
    constexpr auto SwapChain                 = 0x218;
    constexpr auto ScreenWidth               = 0x2A8;
    constexpr auto ScreenHeight              = 0x2AC;
    constexpr auto DeviceContext             = 0x2B0;
    constexpr auto ViewMatrix                = 0x0;
    constexpr auto ProjectionMatrix          = 0x40;
} // namespace D3D

namespace HudRuntime {
    constexpr auto Camera = 0x18;
    constexpr auto Input = 0x28;
    constexpr auto CursorTargetLogic = 0x28;
    constexpr auto UserData = 0x60;
    constexpr auto SpellTargeting = 0x68;
    constexpr auto SpellInfo = 0x68;
    constexpr auto CameraZoom = 0x58; // legacy field is null in the current Hud; use Camera+0x324
    constexpr auto CameraZoomLimits = 0x310;
    constexpr auto AltZoomLimits = 0x3D0;
    constexpr auto ZoomLockFlag1 = 0x344;
    constexpr auto ZoomLockFlag2 = 0x345;
    constexpr auto MouseWorldPos = 0x34;
    constexpr auto ViewportW2S = 0x2B0;
} // namespace HudRuntime

namespace HudCursorTargetLogicRuntime {
    constexpr auto ShowClickEffect = 0xBE5AF0; // native cursor click effect dispatcher
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

namespace HudInputLayout {
    // Checked in IDA 13337 and runtime memory: +0x64 is not a stable selected
    // target network id in the current client. Retained only because existing
    // attack-input compatibility code still names the legacy field; the live
    // picker exposes selected object state through Hud+0x68 instead.
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
    constexpr auto IssueOrder = 0x28E4F0; //E8 ? ? ? ? 33 D2 C7 05 || E8 ? ? ? ? 83 FB ? 0F 44 FD || E8 ? ? ? ? 8D 43 ? 89 05 ? ? ? ? 0F B6 BD
    // sub_98E310(spellbook, spellSlot, slotIndex, Vector3f* position, releaseFlag)
    // is the native charge update/release packet sender. It serializes the three
    // floats at *position verbatim into the packet and sends via SendNetworkPacket,
    // so the aim never touches the HUD cursor / PrimeCastPosition.
    //   releaseFlag = 0 -> charge begin/update (caller: charge tick sub_BEDE90)
    //   releaseFlag = 1 -> release            (callers: HudSpellHandler sub_BD01F0,
    //                                                    ReleaseActiveCharge sub_BC3B00)
    // spellbook is GetPlayerClient() + SpellBookOffset (0x3108); slotIndex is the
    // 0..63 spellbook index, i.e. the same value as the slot for Q/W/E/R.
    // Verified against the running client: base+0x98E310 begins
    //   48 8B C4 44 89 40 18 48 89 48 08 ... 48 83 BA 30 01 00 00 (cmp [rdx+130h],0)
    // which matches sub_98E310's `if (*(_QWORD *)(a2 + 304))` guard.
    // NOTE: this was previously 0x980F80, which is an unrelated vision/unit-count
    // helper (`v1 = *(_DWORD *)(a1 + 208)`). Charge release therefore sent no packet
    // at all, and Hooks::OnUpdateChargeableSpell hooked the wrong function.
    constexpr auto UpdateChargeableSpell = 0x98C360; //E8 ? ? ? ? 48 8B 46 ? F3 0F 10 35 || E8 ? ? ? ? 45 33 ED BB ? ? ? ? 4C 89 6F || E8 ? ? ? ? 48 C7 47 ? 00 00 00 00 F3 0F 10 05

    // sub_98EA10(spellbook, Vector3f* position) — opcode 1195 (mov eax, 4ABh in the
    // prologue), a bare "where the charge is currently aimed" packet holding one
    // Vec3. While a charge is open the client's charge tick (sub_BEDE90) calls this
    // EVERY frame with the HUD cursor world position (hud->input + 0x34), so the
    // charge visibly tracks the mouse even when the begin and release packets carry
    // our own coordinates. Send it ourselves with the predicted position to keep the
    // server's view of the aim on target for the duration of the charge.
    constexpr auto UpdateChargeAim = 0x98CA40; // E8 ? ? ? ? 48 8B 0D ? ? ? ? 48 8B 49 ? E8 ? ? ? ? F3 0F 58 05 ? ? ? ? F3 0F 11 86 || 40 53 48 83 EC ? B8 ? ? ? ? 48 C7 44 24 ? 00 00 00 00

    // ─────────────────────────────────────────────────────────────────────
    // Cast spell pipeline (verified IDB 13337 via EnsoulSharp.dll ILSpy map).
    // EnsoulSharp's managed Spellbook.CastSpell(...) forwards to the native
    // SpellbookClient.CastSpell, which on this build is the programmatic
    // dispatcher CastSpellSafe (sub_C04B50). See
    // CastSpell_EnsoulSharp_IDA_Analysis.md for the full reverse notes.
    //
    // CastSpellSafe is the regular cast dispatcher. Charge input is a
    // separate state machine driven by HudSpellHandler and sub_984130.
    constexpr auto CastSpellSafe          = 0xBE7460; // programmatic CastSpell dispatcher E8 ? ? ? ? EB ? 80 7F ? 00 74 || E8 ? ? ? ? 33 C0 C7 43 ? ? ? ? ? 48 89 43 ? 38 43
    constexpr auto SendSpellCastPacket    = 0x989920; // HUD SendSpellCastPacket (== Hooks::OnProcessSpell) E8 ? ? ? ? 85 C0 40 0F 94 C7 85 C0 || E8 ? ? ? ? 4C 8B 83 ? ? ? ? 8B F0
    constexpr auto BuildCastPacket        = 0x970350; // build cast-spell packet payload E8 ? ? ? ? 48 8B 1D ? ? ? ? 48 8B D0 48 8B 48 ? 48 89 4C 24 || 48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B CA 41 0F B6 F1
    constexpr auto SendNetworkPacket      = 0x6FB9B0; // send packet via NetInstance E8 ? ? ? ? 48 8B 45 ? 48 8B B8 || E8 ? ? ? ? 48 8B 5C 24 ? C6 47 ? 00
    constexpr auto InitChargeChanneling   = 0xBD3BF0; // server ack/timer helper; not charge begin E8 ? ? ? ? 49 8D 4D ? E8 ? ? ? ? 48 8B D8 || 40 53 48 83 EC ? 48 8B D9 48 85 D2 74 ? 48 8B 41
    constexpr auto InitChargeState        = 0xBE86A0; // stores SpellInput at HudSpellState+0x38 E8 ? ? ? ? 48 39 77 ? 75 ? 4C 89 6F || E8 ? ? ? ? F3 0F 11 77 ? 48 8B 74 24
    constexpr auto ReleaseActiveCharge    = 0xBC7BD0; // resolves slot and sends release E8 ? ? ? ? E9 ? ? ? ? 44 3B FB
    constexpr auto GetPlayerClient        = 0x280BB0; // charge sender uses return value + SpellBookOffset E8 ? ? ? ? 48 3B F0 75 ? 48 8B 05 || E8 ? ? ? ? 41 0F B6 4E ? 4C 8B F8
    // Cast dispatch helpers (offsets corrected against IDB 13337 decompile —
    // see CastSpellSafe_Reverse_13337.md §7-§10 for full reverse notes).
    constexpr auto FindOwnerSlot          = 0xBD1220; //E9 ? ? ? ? 48 8B 51 ? 48 85 D2 || E8 ? ? ? ? 4C 8B E0 48 85 C0 0F 84 ? ? ? ? 48 8B C8
    constexpr auto GetOwnerSlotIndex      = 0xBD1310; //E8 ? ? ? ? 33 FF 4C 8D 0D || E8 ? ? ? ? 49 8D 4F ? 8B E8
    constexpr auto NormalCastGate         = 0x2FCD90; //E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? 48 8B CE E8 ? ? ? ? 84 C0 74 || 83 FA ? 77 ? 48 63 C2 48 83 C0
    constexpr auto ServiceRegistryLookup  = 0x241FA0; //E8 ? ? ? ? 4D 8D 75 ? 48 8B F0 || E8 ? ? ? ? 48 85 C0 74 ? 41 8B 8E
    constexpr auto IsSlotQWER             = 0x3FBE20; //E8 ? ? ? ? 84 C0 75 ? 8B CD || E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? 8B 53
    constexpr auto PrepareCast            = 0x985400; //E8 ? ? ? ? 8B D5 48 8B CE E8 ? ? ? ? E9 || E8 ? ? ? ? 49 8B 45 ? 49 8B CD FF 50 ? 48 8B D8
    constexpr auto ValidateProcessCast    = 0x311B40; //E8 ? ? ? ? 0F B6 D0 88 86 || 48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 49 8B C8 49 8B F9
    constexpr auto ExecuteNormalCast      = 0x331F70; //E8 ? ? ? ? E9 ? ? ? ? 8B CD || 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 63 71
    constexpr auto ExecuteSelfCast        = 0x33BCC0; //E8 ? ? ? ? E9 ? ? ? ? 48 8B 0D ? ? ? ? BA || 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B F1 48 63 FA
    constexpr auto ValidateTarget         = 0x30E950; //E8 ? ? ? ? 84 C0 74 ? 48 8B CF E8 ? ? ? ? 44 8B C8 || 48 83 EC ? 48 83 C2 ? E8 ? ? ? ? 48 85 C0
    constexpr auto RangeCheck             = 0x314820; //E8 ? ? ? ? 84 C0 74 ? 4C 8B C7 49 8B D4 || 48 83 EC ? 48 8B 49 ? ? ? ? FF 90 ? ? ? ? 85 C0
    constexpr auto ExecuteTargetCast      = 0x33BB90; //E8 ? ? ? ? 8B 84 24 ? ? ? ? 49 8D 8F || 48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 83 C2

    // ── Pre-cast gate + position priming (REQUIRED before CastSpellSafe) ───
    // Native callers run CanCastCheck (sub_C12220) before CastSpellSafe.
    // It resolves the target/current cursor and primes internal HUD cast state.
    constexpr auto CanCastCheck           = 0xBFAEB0; //E8 ? ? ? ? 84 C0 74 ? 48 8B 53 ? 48 8B CB E8 || E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? 48 8B D3 48 8B CF
    constexpr auto PrimeCastPosition      = 0xBCF800; //E8 ? ? ? ? B0 ? EB ? 45 33 C0 || E8 ? ? ? ? 48 8B 57 ? 48 8D 4F ? 48 89 74 24
    // HUD spell handler (the function the game itself calls when the player
    // presses/releases a hotkey). Signature confirmed in IDB 13337:
    //   sub_BF1EF0(__int64 hudSpellInfo, unsigned int slot,
    //              int castMode, __int64 keyState)
    // arg3 is the hotkey/cast mode (2 = Smart/Quick Cast).
    // arg4 is key state (1 = press, 2 = release).
    constexpr auto HudSpellHandler        = 0xBD41C0; //E8 ? ? ? ? B0 ? 48 83 C4 ? C3 44 8B C2 E8 || E8 ? ? ? ? ? ? ? 48 8B CE FF 90 ? ? ? ? 48 8B 5C 24
    // ── Two-position (vector) cast — Viktor E / EnsoulSharp CastSpell(start,end) ─
    // sub_97A980(book, slotObj, slot, Vector3f* start, Vector3f* end, visionIdx):
    // sibling of the normal cast builder sub_97A0B0, but takes TWO explicit world
    // positions and emits an opcode-271 cast packet directly, WITHOUT calling
    // CanCastCheck — so it accepts vector spells the CastSpellSafe/HUD paths
    // reject. Only x/z are serialized. Confirmed IDB 13337 via caller sub_9D70C0
    // (book=player+0x3108, slotObj=GetSpellSlot(book,slot)) and the shared packet
    // builder sub_9651F0. This is the native entry EnsoulSharp's managed
    // Spellbook.CastSpell(slot, start, end) forwards to.
    constexpr auto CastSpellVector        = 0x985CB0; //E8 ? ? ? ? 48 8B 5C 24 ? 48 8B 7C 24 ? 48 8B 74 24 || 48 8B C4 4C 89 48 ? 48 89 48 ? 55 53
    // GetSpellSlot(book, slot) == *(book + 0xAE0 + slot*8). sub_977B50.
    constexpr auto GetSpellSlot           = 0x974680; //E8 ? ? ? ? 48 8B C8 E8 ? ? ? ? 48 8B 4D || E8 ? ? ? ? 48 8B C8 E8 ? ? ? ? 41 80 7F ? 00
    // Default fog/vision index the cast packet uses when there is no special
    // vision source (sub_97A0B0 / CastSpellSafe fallback `dword_1F18A30`).
    constexpr auto CastVisionIndexDefault = 0x1F699B0; //8B 05 ? ? ? ? 89 8E ? ? ? ? 48 8D 4E ? 89 86 || 8B 05 ? ? ? ? 89 8E ? ? ? ? 48 8D 4E ? 89 46
    constexpr auto SpellTypeClassify      = 0x9121E0; //E8 ? ? ? ? 3C ? 0F 87 ? ? ? ? B9 || E8 ? ? ? ? EB ? 48 8B 4F ? E8 ? ? ? ? 0F B6 C8
    constexpr auto GetSpellLevel          = 0x3B1530; //E8 ? ? ? ? 48 8B 4D ? 44 8B E8 || E8 ? ? ? ? 41 80 7F ? 00 44 8B E8
    // Cast packet opcode (chargeable/normal): 609 (0x261), vtable off_1A4BDF8
    constexpr auto CastPacketOpcode       = 0x261;

    // AIBaseClient handler registered for PKT_NPC_CastSpellAns_s (packet 0x370).
    // Signature: 4C 8B DC 55 57 49 8D AB ?? ?? ?? ?? 48 81 EC 28 02 00 00
    constexpr auto ProcessCastSpell = 0x297510; // 48 8D 05 ? ? ? ? 44 89 BD ? ? ? ? 48 89 85 ? ? ? ? 48 8D 8D ? ? ? ? 8B 44 24 ? 89 85 ? ? ? ? 48 8D 85 ? ? ? ? 48 89 85 ? ? ? ? E8 ? ? ? ? 4C 8B C8 48 8D 4B ? 41 B8 ? ? ? ? 48 8B D6 E8 ? ? ? ? 48 8D 8D ? ? ? ? E8 ? ? ? ? 48 8B 0D || 4C 8B DC 55 57 49 8D AB ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 49 89 5B ? 49 89 73
    constexpr auto GetSpellState = 0x96A2B0; //E8 ? ? ? ? 41 8B 7F ? 8B D8 || E8 ? ? ? ? 85 C0 0F 94 C0 41 88 47
    // IDA 13339: sub_95F2E0 calls sub_912E30(slot) for remaining cooldown.
    // sub_912E30 reads slot+0x30, checks slot+0x68 ammo recharge, then clamps.
    // 0x92D820 resolves inside another function and is not a safe entry point.
    constexpr auto GetSpellRemainingCooldown = 0x90E4D0; // E8 ? ? ? ? F3 0F 11 46 ? 40 0F B6 D7 || E8 ? ? ? ? 0F 28 F0 0F 2F 35 ? ? ? ? B8
    constexpr auto IsAlive = 0x2B6690; //E8 ? ? ? ? 84 C0 74 ? 48 8B 83 ? ? ? ? 48 8D 8B || E8 ? ? ? ? 84 C0 74 ? 48 8D 8F ? ? ? ? E8 ? ? ? ? 84 C0 74 ? 48 85 DB
    constexpr auto GetSpellCastInfo = 0x27B340; //E8 ? ? ? ? 8B 8E ? ? ? ? 8B D7 || E8 ? ? ? ? 8B 8B ? ? ? ? 48 8B F0 89 88
    constexpr auto GetResourceType = 0x278EB0; //E8 ? ? ? ? 33 D2 0F B6 C8 || E8 ? ? ? ? 3C ? 48 8B CF
    constexpr auto GetAttackDelay = 0x57D620; //E8 ? ? ? ? 33 C0 F3 0F 11 83 || E8 ? ? ? ? F3 0F 58 C6 44 38 B6
    constexpr auto GetAttackWindup = 0x57D520; //E8 ? ? ? ? 48 8B CF F3 0F 11 83 || E8 ? ? ? ? 48 8B CE F3 0F 11 83
    constexpr auto GetBoundingRadius = 0x27BFB0; //E8 ? ? ? ? F3 44 0F 58 C0 || 40 53 48 83 EC ? ? ? ? 48 8B D9 FF 90 ? ? ? ? 48 85 C0 74 ? ? ? ? 48 8B CB FF 90 ? ? ? ? 48 8D 88
    constexpr auto IssueOrderFlag = 0x1E53DB8; //89 05 ? ? ? ? 0F B6 BD || 89 05 ? ? ? ? 44 88 74 24
    constexpr auto CastSpellFlag = 0x1E53D50; //0F B6 05 ? ? ? ? 48 8D 57 ? 84 C0
    constexpr auto CanAttack = 0x213450; //E8 ? ? ? ? 84 C0 74 ? 49 8D 8F ? ? ? ? E8 ? ? ? ? 84 C0 74 ? 49 8D 8F ? ? ? ? E8 ? ? ? ? 84 C0 74 ? 49 8D 8F ? ? ? ? E8 ? ? ? ? 4D 8D A7
} // namespace ControlRuntime

// BasicAttackRuntime removed Apr 25/2026 - replaced by AIBaseClient::GetAutoAttackDamage()

namespace BuffManagerRuntime {
    constexpr auto BuffManagerOffset = 0x2E58; //48 8D 8B ? ? ? ? 48 8B D7 E8 ? ? ? ? ? ? ? 48 8B CB
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
    // CE 2026-08-03 live snapshot:
    //   - target `kalistaexpungemarker` with 7 visible stacks has buff+0x38 = 7
    //   - buff+0x3C is a secondary/aux field (e.g. 34 on the same buff)
    // IDA 13337: sub_91BD20 (HasBuff-by-hash iterator, reached from HasBuff
    // wrapper 0x2844A0 via vfunc +0x7A0) reads buff+0x30 as stack-array begin
    // and buff+0x38 as the LIVE stack count (end = begin + 0x10*count); the
    // `cmp [buff+0x38], 0 / jle` there is the liveness gate. (Older notes cited
    // sub_91BC60, which actually falls inside the unrelated decoder sub_91BAC0.)
    constexpr auto BuffStackArrayBegin = 0x30;
    constexpr auto BuffStackCount = 0x38;
    constexpr auto BuffStacks = 0x38;
    constexpr auto BuffStacksAlt = 0x3C;
    constexpr auto BuffCounterCurrent = 0x8C;
    constexpr auto BuffCounterMax = 0x90;
} // namespace BuffDataLayout

// BuffScriptInstance is obtained by dereferencing the first 8 bytes of each
// 16-byte entry in the stack array at BuffDataLayout::BuffStackArrayBegin.
// IDA 13337: sub_91BD20 compares BuffScriptInstance+0x4 with the source
// object's network ID (mov rdx,[rcx]; cmp [rdx+4], eax), confirming +0x4 is
// the caster's network ID.
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
    constexpr auto NavGrid = 0x1EF2F48; //48 8B 05 ? ? ? ? 48 89 84 11 || 48 89 05 ? ? ? ? E8 ? ? ? ? 33 C9
    // IDA 13337: 0x1243760 is a path-neighbor expansion helper that calls
    // sub_124AEA0; CoreNavGrid reads cell flags directly instead of calling it.
    constexpr auto GetCollisionFlags = 0x1288340; //E8 ? ? ? ? 84 C0 75 ? 48 8B 4D ? 48 85 C9 || E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? 48 8B 4D ? 48 85 C9 0F 84
    constexpr auto GetAiManager = 0x283190; // inner AiManager pointer resolver 73 20 90 0F B6 0C 02 lấy offset tại đầu hàm sub_xxxxx
} // namespace NavGridRuntime

namespace SpellRuntime {
    constexpr auto SpellBookOffset = 0x3108; //49 8D ? ? ? ? ? 8B D0 4C 8D ? ? E8
    // ActiveSpellCast lives at a fixed delta from SpellBookOffset (the
    // spellbook's "currently casting" handle is the 7th qword inside the
    // spellbook header). Deriving it removes a stale-offset failure mode
    // when SpellBookOffset shifts on a patch.
    constexpr auto ActiveSpellCast = 0x38; // player + 0x3140 with the current SpellBookOffset
} // namespace SpellRuntime

namespace RuneManagerRuntime {
    // AIHeroClient::GetRuneManager virtual getter. IDA 13337:
    // /liveclientdata/activeplayerrunes calls localPlayer->vfunc[0x800],
    // then reads the returned manager at the RuneManagerLayout offsets below.
    constexpr auto GetRuneManagerVFunc = 0x800;
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
    // Current sub_962890 layout. The legacy Owner/CasterNetId fields no
    // longer exist as direct SpellBookClient fields: +0x08 is initialized to
    // -1 and +0xA8 belongs to the 40-byte state-record array. Keep those two
    // constants only for source compatibility; callers must not treat them as
    // valid current fields without changing the owning logic.
    constexpr auto Owner = 0x08;
    constexpr auto CasterNetId = 0xA8;
    constexpr auto ActiveSlot = 0xAC0;  // sub_971280 returns *(book + 0xAC0)
    constexpr auto SpellSlotArray = 0xAE0; // sub_974680: book + 0xAE0 + slot*8
} // namespace SpellBookLayout

    /*namespace HookSignatures {
        inline constexpr const char* OnMissileCreate = "48 89 5C 24 ? 48 89 6C 24 ? 56 57 41 56 48 83 EC 50 48 8B F1";
        inline constexpr const char* OnMissileDelete = "48 89 5C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC 20 8B 91";
    }*/ // namespace HookSignatures

    namespace Hooks {
        constexpr uintptr_t OnIntegerPropertyChange = 0x386980; //OnIntegerPropertyChange 48 8D 05 ? ? ? ? 89 7D ? 48 89 45 || 48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 4C 8B 42 ? 48 8B DA
        // ClientMainLoop is intentionally 0: NIGHTSHARP_ENABLE_CLIENTMAINLOOP_HOOK
        // is off by default and CoreHook::IsInlineAllowed() skips RVA == 0.
        constexpr uintptr_t ClientMainLoop          = 0x5FA950; //E8 ? ? ? ? 48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B 0D ? ? ? ? E8 ? ? ? ? E8
        // Central animation request wrapper. It stores the AIBaseClient
        // callback receiver at *(RCX+0x08) and accepts an RDX string-view.
        constexpr uintptr_t OnPlayAnimationWrapper  = 0xE44570; //E8 ? ? ? ? 48 8D 4C 24 ? E8 ? ? ? ? 33 DB
        constexpr uintptr_t CreateClientEffect      = 0x9061A0; //E8 ? ? ? ? 45 33 C0 ? ? ? 48 8B 50 || E8 ? ? ? ? 48 8D 4D ? E8 ? ? ? ? 4C 8B E8

        constexpr uintptr_t DispatchEvent           = 0x4CFA90; //E8 ? ? ? ? E9 ? ? ? ? 4D 85 C0 || E8 ? ? ? ? 49 83 FE ? 76 ? 49 8D 56
        constexpr uintptr_t OnTeleport              = 0x739800; //OnTeleport E8 ? ? ? ? 48 83 C3 ? 48 3B DD 75 ? 48 8D 77 ? 48 8B CF
        constexpr uintptr_t Hud_OnDisconnect        = 0x6DEC20; //E8 ? ? ? ? 4C 8B B4 24 ? ? ? ? 48 8B 4D ? 48 33 CC E8 ? ? ? ? 48 81 C4 ? ? ? ? 5E
        constexpr uintptr_t ProcessCastSpell        = ControlRuntime::ProcessCastSpell;
        constexpr uintptr_t OnUpdateChargeableSpell = ControlRuntime::UpdateChargeableSpell;
        constexpr uintptr_t OnBuffAdd               = 0x21F730; //E8 ? ? ? ? 48 8D 4D ? E8 ? ? ? ? 4C 8B B4 24 ? ? ? ? 4C 8B AC 24 ? ? ? ? 4C 8B A4 24 ? ? ? ? 48 85 FF
        constexpr uintptr_t OnBuffRemove            = 0x21F7C0; //E8 ? ? ? ? 48 8D 4C 24 ? E8 ? ? ? ? 4C 8B AC 24 ? ? ? ? 48 8B 8C 24
        constexpr uintptr_t OnBuffUpdate            = 0x21F220; //E8 ? ? ? ? 48 8D 4C 24 ? E8 ? ? ? ? 4C 8B 74 24 ? 48 8B 74 24
        // AssignNetworkId: writes networkId to obj+0xBC, inserts into
        // ObjectManager tree, then calls post-init vfunc. Hook this instead
        // of the raw tree-insert so the object is fully ready when event fires.
        //   RCX = GameObject*, RDX = networkId (uint32_t).
        // IDA sig: 40 56 48 83 EC ? 45 33 C9
        constexpr uintptr_t OnCreate                = 0x571110; //AssignNetworkId E8 ? ? ? ? 4C 8B 35 ? ? ? ? 8B 9E || E8 ? ? ? ? 48 8B 05 ? ? ? ? 39 78 ? 74
        constexpr uintptr_t OnMissileCreate         = 0x97B700; //E8 ? ? ? ? 48 8B 3D ? ? ? ? 33 C9 48 85 FF
        // sub_28F220(GameObject* this) fires GameEventId.OnDelete (event ID = 465/0x1D1)
        // via sub_4BC670(eventObj, 0, &data). RCX = GameObject* (this pointer).
        // IDA sig: 48 89 5C 24 ? 55 56 57 48 83 EC 40 48 8B 01 48 8D 54 24
        constexpr uintptr_t OnDelete                = 0x2942D0; //OnDelete 74 ? ? ? E8 ? ? ? ? 48 83 C3 ? 48 3B DF 75 ? 48 8B CE lấy offset tại đầu hàm sub_xxxxx
        constexpr uintptr_t OnMissileDelete         = 0x9633A0; //E8 ? ? ? ? 48 8B 5C 24 ? 41 F6 C6
        constexpr uintptr_t OnDamage                = 0x29F860; //48 8D 05 ? ? ? ? 44 89 75 ? 48 89 45 ? 48 8D 4C 24 ? 8B 44 24
        constexpr uintptr_t OnDoCast                = 0x9835E0; //48 8B 0D ? ? ? ? 48 8B 49 ? E8 ? ? ? ? 80 BF ? ? ? ? 00 0F 28 F8 75 lấy offset tại đầu hàm sub_xxxxx
        constexpr uintptr_t OnFinishCast            = 0x2A0250; //48 89 5C 24 ? 57 48 83 EC ? 48 8B F9 48 8B DA 8B 8A
        constexpr uintptr_t OnGameUpdate            = 0x562AD0; //OnUpdate E8 ? ? ? ? 41 0F B6 4E ? 49 8B 86
        constexpr uintptr_t OnLevelUp               = 0x294EF0; //4C 89 BC 24 ? ? ? ? 8B FA lấy offset tại đầu hàm sub_xxxxx
        // Packet callback for ids 0x11D/0x1F1. RCX is AIBaseClient and the
        // internal animation-name view is at RDX+0x18.
        constexpr uintptr_t OnPlayAnimation         = 0x29A350; //48 8B 4B ? E8 ? ? ? ? 4C 8D 44 24 lấy offset tại đầu hàm
        constexpr uintptr_t OnProcessSpell          = 0x989920; //OnProcessSpellCast E8 ? ? ? ? 85 C0 40 0F 94 C7 85 C0 || E8 ? ? ? ? 4C 8B 83 ? ? ? ? 8B F0
        constexpr uintptr_t OnSpellImpact           = 0x981D20; //E8 ? ? ? ? E9 ? ? ? ? 48 8B D3 48 89 B4 24 || E8 ? ? ? ? 83 BC 24 ? ? ? ? 00 48 8B B4 24
        constexpr uintptr_t OnStopCast              = 0x989C30; //75 ? 8B 99 ? ? ? ? 8B 91 lấy offset tại đầu hàm sub_xxxxx
        constexpr uintptr_t OnStealth               = 0x29EEC0; //75 ? 48 8B D7 48 8D 4F lấy offset tại đầu hàm sub_xxxxx
        constexpr uintptr_t OnSurrender             = 0; // FIXME: 0xE8D423 was wrong (string builder, not surrender handler). Needs re-verification.
        constexpr uintptr_t ProcessWorldEvent       = 0x6DFD60; //0F 87 ? ? ? ? 48 8D 35 ? ? ? ? 48 98 0F B6 84 06 ? ? ? ? 8B 8C 86 ? ? ? ? 48 03 CE FF E1 41 8B 46 lấy offset tại đầu hàm sub_xxxxx

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
        constexpr uintptr_t OnNewPath               = 0x5715A0; //E8 ? ? ? ? 49 8B 6E ? 48 63 C6
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
    //     entries packed in `hero[+0x4038..+0x4040]`. Only fields with
    //     offset < 0x198 are valid here (i.e. all except CasterNetId).
    //   * Active SpellCast (from `hero + ActiveSpellCast`): a fully
    //     populated >= 0x300-byte struct allocated only while a cast is in
    //     flight. ALL the offsets below apply, including `CasterNetId`.
    // Verified current dump:
    //   * `GetSpellCastInfo` (sub_277380) computes
    //   `(a1+0x4040 - a1+0x4038) / 408` to count slots and returns
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
    //     AIBaseClientLayout::IsDead      -> All::Dead (0x11B, byte flag)
    //     AIBaseClientLayout::Health      -> AttackableUnit::HP (0x1080)
    //     AIBaseClientLayout::MaxHealth   -> AttackableUnit::MaxHP (0x10A8)
    //     AIBaseClientLayout::Mana        -> AIHeroClient::MP     (0x360)
    //     AIBaseClientLayout::MaxMana     -> AIHeroClient::MaxMP  (0x388)
    //     AIBaseClientLayout::Experience  -> AIHeroClient::Exp    (0x4D28)
    //     AIBaseClientLayout::LevelRef    -> AIHeroClient::LevelRef (0x4D50)
    //
    // The old 0x250 "dead" byte was proven wrong in runtime logs. Any downstream code that
    // still references `AIBaseClientLayout::*` needs to be migrated to the
    // dedicated runtime IsDead offset.

    // The encrypted AiManager reference starts at hero + 0x4248. The native
    // resolver decodes it to an outer wrapper whose +0x10 pointer owns the
    // movement fields below.
    namespace AiManager
    {
        constexpr auto AiManager = 0x4248;
        constexpr auto CurrentSegment = 0x320;
        constexpr auto DashSpeed = 0x360;
        constexpr auto IsDashing = 0x384;
        constexpr auto IsMoving = 0x31C;
        constexpr auto MoveVec3 = 0x480;
        constexpr auto NavArray = 0x348;
        constexpr auto ObjectOffset = 0x4248;
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
    // (pre-hook) NightSharp source tree. Field offsets below are retained only
    // when they were re-derived in the current IDB or verified at runtime.
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

    // ── Current spell objects ─────────────────────────────────────────────
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
        constexpr auto SlotSpellInfo         = 0x128; // 0x80-byte per-slot wrapper; +0x60 back-reference is the slot
        constexpr auto SlotSpellData         = 0x130; // sub_4C2CF0 getter; sub_949020 setter
        // Compatibility alias used by existing cast/name readers. The current
        // +0x130 object is SpellData, not a writable SpellInput buffer.
        constexpr auto SlotSpellInput        = SlotSpellData;
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
        constexpr auto DecodeTable = 0x1ABE610;
    } // namespace ProcessCastSpellRequestLayout

    namespace SpellInfoLayout {
        // Current +0x128 wrapper: +0x08 is a nested interface vtable, not a
        // SpellData pointer. No direct wrapper -> SpellData field exists;
        // SpellData is stored directly at SpellSlot+0x130.
        constexpr auto InfoSpellData    = 0x8; // legacy compatibility only
        constexpr auto InfoLevelOrFlag  = 0x14;
        constexpr auto InfoOwnerSlot    = 0x60; // runtime-verified back-reference
        constexpr auto SpellInfoNamePtr = 0x28; // legacy compatibility only; null in current wrapper
    } // namespace SpellInfoLayout

    namespace SpellDataLayout {
        constexpr auto DataSpellName = 0x28; // sub_948E90 reads SpellData+0x28 as the script-name pointer
        constexpr auto DataManaCost  = 0x5F4; // legacy compatibility only; mana now lives in resource arrays
        constexpr auto DataResource  = 0x60; // sub_948C50 and current runtime chain
        constexpr auto ResourceData  = DataResource;
        constexpr auto ResourceName  = DataSpellName;
    } // namespace SpellDataLayout

    namespace SpellDataResourceLayout {
        constexpr auto DataResourceBase = 0x60;
        // Current SpellDataResource serializer sub_95F8C0 registers each BIN
        // field hash with its exact in-memory displacement.
        constexpr auto ResCastRange     = 0x78C; // castRange / runtime CastRange
        constexpr auto ResMissileSpeed  = 0x408; // missileSpeed
        constexpr auto ResLineWidth     = 0x458; // mLineWidth
        constexpr auto ResMaxAmmo       = 0x808; // mMaxAmmo level array
        constexpr auto ResCastType      = 0x400; // mCastType enum
        constexpr auto ResCastRangeDisplayOverride = 0x7A8;
        constexpr auto ResCastRadius    = 0x7C4;
        constexpr auto ResMissileSpec   = 0x3F8;
        // Script name is owned by SpellData+0x28 in the current client; no
        // direct SpellDataResource script-name string field is serialized.
        constexpr auto ResScriptName    = 0x0;
        constexpr auto ResCooldownTime  = 0x700; // cooldownTime level array
        constexpr auto ResAmmoRecharge  = 0x754; // mAmmoRechargeTime level array
        constexpr auto ResImgIconName   = 0x280;
    } // namespace SpellDataResourceLayout

    // ── Extended SpellCastInfo layout (event-received version) ───────────
    // Current verification: the missile-create path copies event CastInfo
    // field-by-field into missile CastInfoBase (0x2A0). The position fields
    // (StartPos/EndPos/CastPos) use the SAME offsets as SpellCastInfoLayout
    // and MissileClient::CastInfoBase. Verified via sub_970190 (OnMissileCreate).
    //
    // CRITICAL FIX (Jul/2026): SrcIndex was 0x98 (wrong), actually 0xA0.
    //   Confirmed by 7+ functions calling FindObject(CastInfo+0xA0).
    // TargetIndex at 0x9C was wrong — 0x9C is a float field (DesignerCastTime).
    //   The actual target is in a vector-like array:
    //     +0x110 = pointer to target entry array (each entry 32 bytes)
    //     +0x118 = entry count
    //   First DWORD of each entry = target local object id/index.
    //   Confirmed by sub_943390, OnSpellImpact (sub_9768E0).
    // CastDelay was 0x118 (conflicts with TargetArrayCount), actually 0x13C.
    //   Confirmed by sub_961900: *(float*)(CastInfo+0x13C) used as cast delay.
    namespace SpellCastInfoEventLayout {
        constexpr auto SpellData        = 0x0;
        constexpr auto SrcIndex         = 0xA0;
        constexpr auto TargetArrayPtr   = 0x110;
        constexpr auto TargetArrayCount = 0x118;
        constexpr auto StartPos         = 0xD0;
        constexpr auto EndPos           = 0xDC;
        constexpr auto CastPos          = 0xE8;
        constexpr auto CastDelay        = 0x13C;
        constexpr auto IsSpell          = 0x134;
        constexpr auto IsSpecialAttack  = 0x13E;
        constexpr auto IsAuto           = 0x141;
        constexpr auto Slot             = 0x14C;
        // IDA 13337: sub_975A00 = movss xmm0,[rcx+0x98]; ret
        // (xref from sub_9BAEE0 cùng sub_9B5920 — pattern castTime = Extra + Designer)
        constexpr auto ExtraTimeForCast = 0x98; // F3 0F 10 81 98 00 00 00 C3
        // IDA 13337: sub_9B5920 = movss xmm0,[rcx+0x9C]; ret (DesignerCastTime)
        constexpr auto DesignerCastTime = 0x9C; // F3 0F 10 81 9C 00 00 00 C3
        //F3 0F 10 81 ?? 00 00 00 C3 sử dụng cho cả 2
        // IDA 13337 hotfix path sub_982120 reads the same logical fields here.
        constexpr auto IsSpecialAttackAlt = 0x149;
        constexpr auto IsAutoAlt          = 0x14A;
        constexpr auto SlotAlt            = 0x154;
        constexpr auto TargetArrayEntryStride = 32;
    } // namespace SpellCastInfoEventLayout

    // (EventSpellCastInfoLayout removed Apr 25/2026 - duplicate of SpellCastInfoEventLayout above; old schema)

    // ── Inventory / items ────────────────────────────────────────────────
    // Current inventory layout:
    //   hero + InventoryComponent → component object
    //   component + SlotArray     → 45 slot pointers, 8 bytes each
    //   slot + ItemNode           → item node ptr (null = empty)
    //   node + ItemData           → item-data ptr
    //   item-data + DataItemId    → item ID
    namespace ItemRuntime {
        constexpr auto InventoryComponent = 0x4DD8;
        constexpr auto SlotArray          = 0x50;
        constexpr auto SlotCount          = 0x2D;
        constexpr auto ItemNode           = 0x10;
        constexpr auto ItemData           = 0x38;
        constexpr auto ItemInfo           = ItemData;
        constexpr auto DataItemId         = 0xB4;
        constexpr auto DataItemIdString   = 0x08; // legacy fallback only
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
        // Current minion-class byte. IDA sub_2FC830 writes object+0x4CC9;
        // live lane minions resolve to melee=4, ranged=5, siege=6, super=7.
        constexpr auto TypeOffset        = 0x4CC9;
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
        constexpr auto CharacterData         = 0x4050;
        constexpr auto Component             = 0x4498;
        constexpr auto Queue                 = 0x4978;
        constexpr auto QueueEnd              = 0x4980;
        constexpr auto QueueCapacityEnd      = 0x4988;
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
    // offsets. The dead state is queried by Core::Objects::IsDead() via the
    // native IsAlive RVA (ControlRuntime::IsAlive = 0x2B0CE0), inverted.
    // Do NOT call the native IsDead RVA (0x287230) — it decrypts an
    // encrypted blob and crashes the game on 26.6. All::Dead (0x11B) is
    // kept as a diagnostic field only; it is NOT authoritative across
    // spawn/respawn transitions.
    namespace All {
        constexpr auto Index               = 0x20;    // GetID/sub_371690 -> obj+0x20; slot-array lookup sub_54EF60 keys (index & 0xFFFF) and validates obj+0x20
        constexpr auto Team                = 0x239;  // byte team index
        constexpr auto Name                = 0x68;
        // NetworkId is a DISTINCT field from Index. Verified on current dump:
        //   - sub_567330 (OnCreate/AssignNetworkId) writes the network id to
        //     obj+0xBC and registers it into the ObjectManager network-id
        //     red-black tree via sub_5655F0.
        //   - sub_90C980 (OnMissileDelete) reads *(obj+0xBC) to deregister
        //     from the missile manager tree.
        //   - FindObject (sub_555D10) and GetNextObject (sub_557750) look up
        //     by Index (obj+0x20), not by NetworkId.
        //   - sub_5576B0 does tree-based lookup by NetworkId in the
        //     ObjectManager tree at manager+0x38.
        // Previous build had NetId = 0xCC which was wrong.
        constexpr auto NetId               = 0xBC;    // obj+0xBC -> object network id (GetNetworkID)
        constexpr auto NetworkId           = NetId;
        // Current object position is a contiguous native Vector3f.
        constexpr auto Position            = 0x23C;
        constexpr auto PositionX           = Position;
        constexpr auto PositionY           = 0x240;
        constexpr auto PositionZ           = 0x244;
        constexpr auto Dead                = 0x11B; //tìm thủ công qua CE
        constexpr auto Visible             = 0x2E8;   // GameObject visible flag. tìm thủ công qua CE
        // No byte access at legacy +0x5A0 exists in the current object code.
        // IsInvulnerable is resolved through native/buff logic; no raw field exists.
        // RecallState (legacy 0xF48) was discovered to be a std::vector data
        // pointer on 26.6 (sub_9F18FE constructor / sub_9F18A0 destructor pair
        // free `[obj+0xF48]` when `[obj+0xF54] >= 0`). The real recall
        // channel state should be derived from SpellRuntime::ActiveSpellCast.
        // Keeping the constant for ABI compatibility but new code MUST NOT
        // read it.
        constexpr auto RecallState         = 0xF48;
        constexpr auto Radius              = 0x7C;   // SelectionRadius; constructors store the resolved runtime radius here
        constexpr auto CharacterData       = 0x4050;
        constexpr auto CharacterName       = 0x4378;
        constexpr auto Direction           = 0x21D8;  // legacy alias; do not read Vec3 directly from this field
        constexpr auto DirectionComponent  = 0x1268;  // object + component -> vfunc +0xA8 -> holder
        constexpr auto DirectionVFunc      = 0xA8;
        constexpr auto DirectionVector     = 0x20;    // holder + 0x20 -> facing direction Vec3
        constexpr auto MissileClientHandle = 0x2D8;
        constexpr auto ItemList            = 0x4DD8;  // = InventoryComponent
    } // namespace All

    namespace AttackableUnit {
        constexpr auto HP              = 0x1060;
        constexpr auto MaxHP           = 0x1088;
        constexpr auto HPMaxPenalty    = 0x10B0;
        constexpr auto AllShield       = 0x1100;
        constexpr auto PhysicalShield  = 0x1128;
        constexpr auto MagicalShield   = 0x1150;
        constexpr auto ChampSpecific   = 0x1178;
        constexpr auto InHealAllied    = 0x11A0;
        constexpr auto InHealEnemy     = 0x11C8;
        constexpr auto InDamage        = 0x11F0;
        constexpr auto StopShieldFade  = 0x1218;
        constexpr auto IsTargetable    = 0xEB0;
        constexpr auto TargetableFlags = 0xED8;
        constexpr auto ActionStateBase = 0x1450;
        constexpr auto ActionState1    = 0x1480;
        constexpr auto ActionState2    = 0x14A8;
        constexpr auto CharacterState  = ActionState1 + 0x30;
    } // namespace AttackableUnit

    namespace AIHeroClient {
        constexpr auto MP                       = 0x340;
        constexpr auto MaxMP                    = 0x368;
        constexpr auto PAR                      = 0xE00;
        constexpr auto MaxPAR                   = 0xE28;
        constexpr auto SAR                      = 0x108;
        constexpr auto MaxSAR                   = 0x130;
        constexpr auto PhysDmgPercent           = 0x1D08;
        constexpr auto MagicDmgPercent          = 0x1DA8;
        constexpr auto AbilityHaste             = 0x1BA0;
        constexpr auto FlatPhysicalDmgMod       = 0x1CE0;
        constexpr auto AttackSpeedMod           = 0x1E48;
        constexpr auto PercentAttackSpeedMod    = 0x1E70;
        constexpr auto BaseAttackDamage         = 0x1EE8;
        constexpr auto BaseAtkDmgSansScale      = 0x1F10;
        constexpr auto FlatBaseAtkDmgMod        = 0x1F38;
        constexpr auto PercentBaseAtkDmgMod     = 0x1F60;
        constexpr auto BaseAbilityDamage        = 0x1D80;
        constexpr auto CritDamageMultiplier     = 0x1FB0;
        constexpr auto Dodge                    = 0x2000;
        constexpr auto Crit                     = 0x2028;
        constexpr auto Armor                    = 0x2078;
        constexpr auto BonusArmor               = 0x20A0;
        constexpr auto SpellBlock               = 0x20C8;
        constexpr auto BonusSpellBlock          = 0x20F0;
        constexpr auto HPRegenRate              = 0x2118;
        constexpr auto BaseHPRegenRate          = 0x2140;
        constexpr auto MoveSpeed                = 0x2168;
        constexpr auto AttackRange              = 0x21B8;
        constexpr auto FlatArmorPen             = 0x2230;
        constexpr auto PhysicalLethality        = 0x2258;
        constexpr auto PercentArmorPen          = 0x2280;
        constexpr auto PercentBonusArmorPen     = 0x22A8;
        constexpr auto FlatMagicPen             = 0x2320;
        constexpr auto MagicLethality           = 0x2348;
        constexpr auto PercentMagicPen          = 0x2370;
        constexpr auto PercentBonusMagicPen     = 0x2398;
        constexpr auto PercentLifeSteal         = 0x23C0;
        constexpr auto PercentSpellVamp         = 0x23E8;
        constexpr auto PercentOmnivamp          = 0x2410;
        constexpr auto PercentCCReduction       = 0x2488;
        constexpr auto FlatBaseAttackSpeedMod   = 0x25C8;
        constexpr auto Gold                     = 0x2848;
        constexpr auto GoldTotal                = 0x2870;
        constexpr auto Exp                      = 0x4D48;
        constexpr auto LevelRef                 = 0x4D70;
        constexpr auto LevelUpPoints            = 0x4D98;
        // Community reverse confirmation + IDA 13337 parity path:
        // AIHeroClient::GetRuneManager returns the same manager consumed by
        // /liveclientdata/activeplayerrunes. The manager layout is documented
        // in RuneManagerLayout above.
        constexpr auto RuneManager              = 0x50B8;
        constexpr auto VisionScore              = 0x5578;
        constexpr auto ShutdownValue            = 0x55A0;
        constexpr auto BaseGoldOnDeath          = 0x55C8;
        constexpr auto NeutralMinionsKilled     = 0x55F0;
    } // namespace AIHeroClient

    namespace MissileClient {
        // IDA 13337:
        // OnMissileCreate (sub_93ADA0) copies the network spell payload into
        // object + 0x2A0 via the missile-create copy routine. The copied payload is the source for
        // SData, names, caster/target indexes and trajectory vectors used by
        // missile tracking / evade.
        constexpr auto CastInfoBase  = 0x2A0;
        constexpr auto SpellDataPtr  = CastInfoBase + 0x00;
        // ReClass live 2026-07-08: SpellDataPtr points to a heap spell-data
        // object whose compact strings at +0x28/+0x60 expose names such as
        // SRU_ChaosMinionRangedBasicAttack. The old payload string fields
        // below are retained only as fallbacks.
        constexpr auto SpellDataSpellName   = 0x28;
        constexpr auto SpellDataMissileName = 0x60;
        constexpr auto SpellName     = CastInfoBase + 0x20;
        constexpr auto MissileName   = CastInfoBase + 0x48;
        constexpr auto TargetArrayPtr   = CastInfoBase + 0x110;
        constexpr auto TargetArrayCount = CastInfoBase + 0x118;
        constexpr auto CasterIndex   = CastInfoBase + 0xA0;
        constexpr auto MissileNetId  = CastInfoBase + 0xAC;
        constexpr auto ObjectNetId   = All::NetId;
        constexpr auto StartPos      = CastInfoBase + 0xD0;
        constexpr auto EndPos        = CastInfoBase + 0xDC;
        constexpr auto CastEndPos    = CastInfoBase + 0xE8;
        constexpr auto Position      = All::Position;
        constexpr auto StartTime     = 0x458;
    } // namespace MissileClient

    namespace MissileEventLayout {
        constexpr auto SpellData                 = 0x00;
        constexpr auto Slot                      = 0x08;
        constexpr auto SpellName                 = 0x20;
        constexpr auto MissileName               = 0x48;
        constexpr auto TargetArrayPtr              = 0x110;
        constexpr auto TargetArrayCount             = 0x118;
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
        constexpr auto KeyboardInput     = 0x1214240;
        constexpr auto MouseInput        = 0x1214450;
        constexpr auto KeyboardDevice    = 0x1FA7F60;
        constexpr auto KeyboardBuffer    = 0x1FA7F70;
        constexpr auto KeyboardCount     = 0x1FA7F90;
        constexpr auto KeyboardFlag      = 0x1F448D8;
        constexpr auto MouseDevice       = 0x1FA7F68;
        constexpr auto MouseBuffer       = 0x1FA7F78;
        constexpr auto MouseCount        = 0x1FA7FA0;
        constexpr auto VT_GetDeviceState = 0x48;
        constexpr auto VT_GetDeviceData  = 0x50;
    } // namespace DirectInputRuntime

    namespace ZoomRuntime {
        constexpr auto CameraInstance   = 0x0;
        constexpr auto HudToCameraPtr   = 0x18;
        constexpr auto CurrentZoom      = 0x324;
        constexpr auto ZoomConfigPtr    = 0x3D0;
        constexpr auto ZoomFallbackPtr  = 0x310;
        constexpr auto DisableZoomClamp = 0x345;
        constexpr auto ZoomClampFlag    = 0x344;
        constexpr auto ZC_MinZoom       = 0x24;
        constexpr auto ZC_MaxZoom       = 0x28;
        constexpr auto HardcodedMaxZoom = 0x0;
        constexpr auto ZoomEnableConfig = 0x1F6BF28; //0F 95 05 ? ? ? ? E8 || 80 3D ? ? ? ? 00 8B 47
    } // namespace ZoomRuntime

    namespace SkinRuntime {
        // Do not write CharacterDataSkinId directly for live skin changes.
        // The animation stack reads CharacterDataStack; direct CharacterData
        // writes can leave model resources out of sync and crash while moving.
        constexpr auto CharacterDataSkinId = 0x146;

        // R3nzSkin-style runtime path, updated from R3nzSkin 16.13.1
        // signature set / newoffset 16.13.
        // local + CharacterDataStack -> std::vector + base_skin.
        // base_skin.skin = stack + BaseSkin + CharacterStackSkin.
        // CharacterDataStackUpdate is the current stack-update routine.
        // CharacterDataStackPush is sub_22AEE0(this, model, skin, id, ...).
        constexpr auto CharacterDataStack       = 0x40E0;
        constexpr auto CharacterDataStackUpdate = 0x212840; //E8 ? ? ? ? 80 BD ? ? ? ? 00 74 ? 48 8B 8F || E8 ? ? ? ? 48 8B 8F ? ? ? ? ? ? ? 48 8B 5C 24
        constexpr auto CharacterDataStackPush   = 0x22D590; // Push(this=hero+0x40E0, model, skin, id, ...). 160-byte entries, calls Update after push. E8 ? ? ? ? 48 8D 8D ? ? ? ? E8 ? ? ? ? 48 85 C0 74 ? 48 85 ED
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
        // Field is xor_value<int>; keep it synchronized with base_skin.skin.
        // R3nzSkin AIBaseCommon::change_skin writes BOTH this AND
        // base_skin.skin; writing only one of them desyncs animation
        // resources and can crash on the next animation tick.
        constexpr auto AiBaseSkinId = 0x1314;

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
