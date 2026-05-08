#pragma once


namespace Offset {

namespace GameObjectsRuntime {
    constexpr auto Player = 0x1E538F0;
    constexpr auto Objects = 0x1E1A9D0;
    constexpr auto Heroes = 0x1E1AA28;
    constexpr auto Minions = 0x1E1AA20;
    constexpr auto Missiles = 0x1E1E970;
    constexpr auto Turrets = 0x1E1EB50;
    constexpr auto UnderMouseObject = 0x1E1EB90;
} // namespace GameObjectsRuntime

namespace ObjectManagerRuntime {
    constexpr auto ManagerListItems = 0x8;
    constexpr auto ManagerListSize = 0x10;
    constexpr auto GetFirstObject = 0xA01D60;
    constexpr auto GetNextObject = 0x523C90;
    constexpr auto FindObject = 0x54F490;
} // namespace ObjectManagerRuntime

namespace GameRuntime {
    constexpr auto GameTime = 0x1E294D0;
    constexpr auto NetInstance = 0x1E1A9C8;
    // ChatClient: in-game chat root pointer. Object layout: PrimaryOpen@0x10,
    // Editing@0x68, Focused@0x6C (see ChatClientLayout).
    // Re-derived 26.6 by tracing the `game_console_chatcommand_allchat_1`
    // handler (sub_B27470) which loads `cs:qword_1E53908` to gate the chat
    // command on `[chat+0x10] == 0`. Original sig from build dump no longer
    // matches; use the new sig below for future re-resolution.
    constexpr auto ChatClient = 0x1E53908;
    // sig: 40 53 48 83 EC 20 80 B9 61 06 00 00 00 48 8B D9 74 ?? 48 8B 0D ?? ?? ?? ??
    //      (resolve: ChatClient = match + 18 + 7 + *(int32*)(match + 18 + 3))
    constexpr auto ChatInstance = 0x1E1EBA0;
    constexpr auto ShopInstance = 0x1E2EB50;
    // OpenWindowsArray / OpenWindowsCount: legacy RVAs restored on 26.6.
    // Re-derived by porting the old-build init function `sub_192B60`
    // (which writes `qword_1E7F158`) to the new build via a hash-anchored
    // signature. The unique 32-bit immediate `0xB375545F` survives the
    // rebuild and lands in `sub_19B070` at the same logical position; that
    // function still does the exact same store sequence:
    //     lea rax, qword_1EC4CC0           (the array-data buffer)
    //     mov cs:qword_1EC4960, 0
    //     lea rcx, unk_1946A60
    //     mov cs:qword_1EC4998, rax        (= OpenWindowsArray)
    // OpenWindowsCount lives at the very next qword (write-once, no read
    // xref - same pattern as the old build's 0x1E7F160).
    //
    // Signature anchor (66 bytes, 1 match):
    //   BA 5F 54 75 B3 48 8D 05 ?? ?? ?? ?? 48 89 44 24 28
    //   48 8D 05 ?? ?? ?? ?? 48 89 44 24 20 E8 ?? ?? ?? ??
    //   48 8D 05 ?? ?? ?? ?? 48 C7 05 ?? ?? ?? ?? 00 00 00 00
    //   48 8D 0D ?? ?? ?? ?? 48 89 05 ?? ?? ?? ??
    //   OpenWindowsArray = (match + 66) + *(int32_t*)(match + 62);
    //   OpenWindowsCount = OpenWindowsArray + 8;
    constexpr auto OpenWindowsArray = 0x1EC4998;
    constexpr auto OpenWindowsCount = 0x1EC49A0;
    constexpr auto MySpellState = 0x1E216D8;
    constexpr auto CursorPosRaw = 0x1EACFF0;
    constexpr auto MouseScreenVec2 = 0x1E1E918;
    constexpr auto GetPing = 0x69F2C0;
    constexpr auto GetMapID = 0x2AAEC0;
    constexpr auto PrintChat = 0x10F6F00;
} // namespace GameRuntime

namespace DrawingRuntime {
    constexpr auto WorldToScreen = 0x12B27B0;
    constexpr auto HudInstance = 0x1E1AB70;
    // ViewPort: legacy primary viewport global. The 26.6 build appears to
    // route everything through `ViewPort2` (sub_133FA00 only allocates one
    // viewport struct now and stores it at qword_1EE97D0); CoreView's
    // GetViewport() falls back to ViewPort2 transparently, so leaving this
    // at 0 (= invalid pointer) is the safe behaviour. Original sig from
    // build dump (`48 8B 0D ?? ?? ?? ?? 48 8D 96 ?? ?? ?? ?? 48 8B 5C 24 ??`)
    // matches zero call sites in the new build.
    constexpr auto ViewPort = 0x1EEA458; 
    constexpr auto ViewPort2 = 0x1EE97D0;
    constexpr auto Renderer = 0x1EE97C8;
    constexpr auto ViewProjOffset = 0x1ED4660;
} // namespace DrawingRuntime

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
// to function in CoreControl/CoreView, so leaving them untouched.
namespace DrawingMatrixRuntime {
    constexpr auto ProjMatrixRelative = 0x40;
} // namespace DrawingMatrixRuntime

namespace HudRuntime {
    constexpr auto Camera = 0x18;
    constexpr auto Input = 0x28;            // verified 26.6: HudInstance + 0x28 -> HudInput
    constexpr auto UserData = 0x60;
    constexpr auto SpellInfo = 0x68;
    constexpr auto CameraZoom = 0x324;
    constexpr auto CameraZoomLimits = 0x310;
    constexpr auto AltZoomLimits = 0x3D0;
    constexpr auto ZoomLockFlag1 = 0x344;
    constexpr auto ZoomLockFlag2 = 0x345;
    constexpr auto MouseWorldPos = 0x34;
    // ViewportW2S = 0x2B0: offset of the W2S sub-struct on the viewport
    // object. Sig anchor in 26.6: dispatcher sub_DD43B0 case 2 calls
    // `lea rdx, [rsi+2B0h]` after `mov r8d, 2` (W2S type-id).
    // Re-resolve sig (1 match): `41 B8 02 00 00 00 48 8D 96 B0 02 00 00`
    //   ViewportW2S = *(uint32_t*)(match + 9)   // bytes [B0 02 00 00]
    constexpr auto ViewportW2S = 0x2B0;
} // namespace HudRuntime

// HudZoomLayout removed Apr 25/2026 - duplicate of ZoomRuntime::ZC_MinZoom/ZC_MaxZoom

// Struct field offsets (relative). Verified stable on 26.6 by spot-checks
// against the disassembly of the call sites that consume them.
namespace HudInputLayout {
    constexpr auto SelectedObjNetId = 0x60;
} // namespace HudInputLayout

// Chat client object layout (object lives at GameRuntime::ChatClient / ChatInstance).
// Historically some old code exposed PrimaryOpen via `Offset::Hud::ChatOpen = 0x10`.
// Editing/Focused bytes are read to detect whether the user is actively typing
// (primary flag flips slightly before editing on the current build).
// 26.6 verification: sub_B27470 (allchat command handler) loads ChatClient
// then guards on `cmp byte ptr [rcx+10h], 0` -> confirms PrimaryOpen = 0x10.
namespace ChatClientLayout {
    constexpr auto PrimaryOpen = 0x10;       // verified 26.6: sub_B27470 [rcx+0x10]
    constexpr auto Editing     = 0x68;
    constexpr auto Focused     = 0x6C;
} // namespace ChatClientLayout

namespace ControlRuntime {
    constexpr auto IssueOrder = 0x2BD450;
    constexpr auto CastSpellWrap = 0xBE1180;
    constexpr auto IsAlive = 0x307DF0;
    constexpr auto GetSpellCastInfo = 0x2A04E0;
    // GetSpellSlot: native helper used to be called from a Lua-style binding
    // table (entry registered as `"GetSpellSlot"` string in sub_131EE0). The
    // 26.6 build still registers the binding but the handler is dispatched
    // through an opaque table — static analysis cannot recover its address
    // without dynamic tracing. Not actually invoked from Nightsharp/SDK
    // (AIBaseClient::GetSpellSlot iterates the spellbook in C++), so the
    // missing handler is non-fatal. Original sig
    // (`48 89 5C 24 ?? 55 56 57 41 56 41 57 48 8D AC 24 60 FD FF FF`) has
    // zero matches in 26.6.
    constexpr auto GetResourceType = 0x29D3D0;
    constexpr auto GetAttackDelay = 0x55CC80;
    constexpr auto GetAttackWindup = 0x55CB80;
    constexpr auto GetBoundingRadius = 0x2A1D90;
    constexpr auto IssueOrderFlag = 0x1D7CD58;
    constexpr auto CastSpellFlag = 0x1D7CCF0;
    constexpr auto CastSpellSafe = 0xBE1380;
    constexpr auto CanAttack = 0x20CFE0;
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
    constexpr auto BuffStacks = 0x38;
    constexpr auto BuffStacksAlt = 0x3C;
} // namespace BuffDataLayout

namespace NavGridRuntime {
    constexpr auto NavGrid = 0x1E1E8E8;
    constexpr auto GetCollisionFlags = 0x1200060;
    // GetAiManager: verified 26.6 via IDA disasm (`lea rdx, [rcx+4228h]`,
    // then XOR-decode loop). Hero is in RCX, table at hero+0x4228. SDK does
    // NOT call this directly — it inlines the decode in
    // `shim::DecodeAiMgr` (CoreEventHook.h) so we don't have to invoke a
    // game function from a hook thread.
    constexpr auto GetAiManager = 0x2A9F10;
    // GetAiManagerInner is sub_2AB530 on 26.6. It runs the same XOR-decode
    // as GetAiManager and returns `*(wrapper + 0x10)`.
    // 0x2AB500 is only a float accessor for `inner + typeAdjust + 0x34C`.
    constexpr auto GetAiManagerInner = 0x2AB530;
} // namespace NavGridRuntime

namespace SpellRuntime {
    constexpr auto SpellBookOffset = 0x3128;
    // ActiveSpellCast lives at a fixed delta from SpellBookOffset (the
    // spellbook's "currently casting" handle is the 7th qword inside the
    // spellbook header). Deriving it removes a stale-offset failure mode
    // when SpellBookOffset shifts on a patch.
    constexpr auto ActiveSpellCast = SpellBookOffset + 0x38;  // = 0x3160 on 26.6
} // namespace SpellRuntime

namespace SpellBookLayout {
    constexpr auto SpellSlotArray = 0xAE0;
} // namespace SpellBookLayout

    // =========================================================================
    // Spell-event VMT (OnProcessSpell) — Shadow-VMT hook (confirmed working).
    // =========================================================================
    // The vtable lives in .rdata as a 40-entry table (20 pairs of message-
    // factory methods). Entry 35 is the ONLY slot with a real handler
    // (~217 B, size 0xD9); every other slot is a tiny 43/57-B template
    // (sizes 0x2B / 0x39) and should NOT be hooked.
    //
    // Both anchors below were cross-validated on builds 13338 (old, file
    // base 0x7FF6D6B60000) and 26.6/13337 (file base 0x7FF628C40000):
    //   - VTable sig matches at exactly one site in each IDB and resolves
    //     via (match + 7) + i32(match + 3) to the table base.
    //   - Wrapper sig matches at exactly one site and IS the wrapper itself
    //     (slot[35] of the resolved vtable).
    //
    // Drift between builds (13338 -> 26.6):
    //     VTableRVA  0x1928D58 -> 0x1966D58   (+0x3E000)
    //     WrapperRVA 0x1FD080  -> 0x2072B0    (+0xA230)
    //     HandlerIndex / VTableEntryCount: unchanged.
    //
    // ── VTableRVA sig (lea rax, unk_<vtable>) ─────────────────────────────
    //   48 8D 05 ?? ?? ?? ?? 48 89 03 EB 03 49 8B DF
    //   B9 18 00 00 00 E8 ?? ?? ?? ?? 4C 8B F8 48 85 C0
    //   resolve: VTableRVA = (match + 7) + *(int32_t*)(match + 3)
    //
    // ── WrapperRVA sig (sub_1FD080 prologue, unique on both builds) ───────
    //   48 89 5C 24 10 56 48 83 EC 50 49 83 78 18 0F 0F
    //   57 C0 F3 0F 7F 44 24 30 48 8B F2 48 C7 44 24 40
    //   resolve: WrapperRVA = match
    //
    // VTableEntryCount is set to 64 (original working value). The shadow copy
    // reads a few extra qwords past the real 40-slot vtable; since only
    // slot 35 is redirected, the tail garbage is harmless on this build.
    // Lower to 40 if a future build mis-protects the trailing bytes.
    namespace SpellEventVMT {
        // Verified 26.6 (port 13337):
        //   - sig "48 8D 05 ?? ?? ?? ?? 48 89 03 EB 03 ..." matches once
        //     at 0x200563; resolved VTableRVA = 0x1966D58.
        //   - sig "48 89 5C 24 10 56 48 83 EC 50 49 83 78 18 0F 0F ..."
        //     matches once at 0x2072B0; sub_2072B0 has size 0xD9 (217B),
        //     identical shape to old sub_1FD080.
        //   - vtable[35] (0x1966D58 + 0x118 = 0x1966E70) reads back as
        //     0x2072B0, matching WrapperRVA. HandlerIndex unchanged.
        constexpr auto VTableRVA        = 0x1966D58;
        constexpr auto HandlerIndex     = 35;
        constexpr auto VTableEntryCount = 64;
        constexpr auto WrapperRVA       = 0x2072E0;   // verified live (CE) Hotfix May/2026
    } // namespace SpellEventVMT

    // =========================================================================
    // Function entry-point RVAs — targets for inline detour hooks (DBVM-cloaked).
    // =========================================================================
    //   Each address is the prologue of a game function we splice with a 14-byte
    //   `jmp [rip+0]; dq target` trampoline. The written bytes live on the page's
    //   EXECUTE view; Packman's integrity scan reads the EPT-cloaked READ view
    //   and sees pristine bytes. See `cloak_events.lua` for the cloak setup.
    //
    //   All 8 offsets were IDA-validated on build 26.6 (function prologues below).
    //   Re-verify on build bumps with the listed signatures.
    //
    //   Sig hints (IDA "Search → Sequence of bytes"):
    //     OnStopCast              @ 0x942300 — 48 89 6C 24 20 57 48 83 EC 30
    //     OnFinishCast            @ 0x2D5B70 — 48 89 5C 24 08 57 48 83 EC 20
    //     OnBuffAdd               @ 0xBF59E0 — 48 89 5C 24 08 48 89 74 24 10
    //     OnSpellImpact           @ 0x937A40 — 40 53 48 83 EC 30 48 8B D9 0F
    //     OnCreateObject          @ 0x5388D0 — 48 8B C4 4C 89 48 20 55 48 8B
    //     OnGameUpdate            @ 0x533FE0 — 48 89 5C 24 20 55 56 57 41 54
    //     OnHeroActionStateChange @ 0xEBA680 — 48 83 EC 58 44 8B 02 48 8D 81
    //     OnMinionFollowChange    @ 0xEBDAC0 — 48 83 EC 58 44 8B 02 48 8D 41
    namespace Function {
        constexpr auto OnStopCast               = 0x952B20;
        constexpr auto OnFinishCast             = 0x2E3F20;
        constexpr auto OnBuffAdd                = 0xBF5FC0;
        constexpr auto OnSpellImpact            = 0x9459D0;
        constexpr auto OnCreateObject           = 0x5534A0;
        constexpr auto OnGameUpdate             = 0x54EBC0;
        constexpr auto OnHeroActionStateChange  = 0xED85B0;
        constexpr auto OnMinionFollowChange     = 0xEDBA70;
    } // namespace Function

    // =========================================================================
    // Event identifiers
    // =========================================================================
    // Stable integer ID for every event CoreEventHook can fire. Use these when
    // registering callbacks / filtering polled events.
    namespace Events {
        enum Id : int {
            // Real Shadow-VMT hook (packet-driven, fires on EVERY hero's cast)
            OnProcessSpell      = 1,

            // Derived from SpellCastInfo transitions (SpellBook polling)
            OnStopCast          = 2,   // active cast disappeared before endTime
            OnFinishCast        = 3,   // active cast disappeared at/after endTime
            // OnChannelStart   = 4,   // REMOVED — unreliable on current builds
            //                         //  (channel state isn't surfaced through the
            //                         //   spell-cast VMT slot on 26.x; tested with
            //                         //   Xerath Q and nothing fires).
            //                         //  Workaround: subscribe to OnProcessSpell
            //                         //  and check `GetActiveSpellCast().ChannelEnd
            //                         //  > 0` at callback-time, or poll the slot
            //                         //  yourself via SpellBook::ActiveSpellCast.
            OnChannelEnd        = 5,

            // Buff lifecycle — unified under OnBuffUpdate.
            //
            // `HkOnBuffAdd` (inline detour on sub_BF59E0, the BuffManager
            // dispatch function) fires OnBuffUpdate once per buff mutation
            // — add, stack-refresh, duration-refresh — on the game thread
            // inside the detour body. Consumers read the passed BuffData*
            // and act based on current state, rather than relying on
            // ambiguous gain/lose semantics.
            //
            // OnBuffGain / OnBuffLose were REMOVED because:
            //   - The BuffManager dispatcher doesn't expose clean
            //     add-vs-remove semantics at the entry point.
            //   - A dedicated `AIBaseClient::RemoveBuff` inline detour was
            //     evaluated (id 14 reservation) but proven unreliable: the
            //     dispatch fires for cosmetic refresh paths and misses some
            //     genuine expiries, so OnBuffLose stays unimplemented.
            //   - Lose detection used to require a diff-against-snapshot poll,
            //     which the user asked us to retire.
            //   - A single OnBuffUpdate covers every state-change moment
            //     that scripts actually need.
            OnBuffUpdate        = 12,
            // OnRecall         = 13,  // REMOVED — call AIHeroClient::IsRecalling()
            //                         //   or HasBuff("Recall") directly at the
            //                         //   point of use. Old-source pattern.
            // OnBuffLose       = 14,  // REMOVED — see comment above OnBuffUpdate.
            // OnTeleport       = 14,  // REMOVED — same; HasBuff("HeroTeleport").

            // OnAutoAttack     = 20,  // REMOVED — basic-attacks flow through
            //                         //   the same message-factory dispatch
            //                         //   as spell casts. `OnProcessSpell`
            //                         //   already fires for every AA; the
            //                         //   `intParam` argument carries the
            //                         //   `SpellSlot` byte, which is 64 for
            //                         //   auto-attacks. Consumer pattern:
            //                         //
            //                         //     SetCallback(OnProcessSpell,
            //                         //       [](sender, castInfo, slot) {
            //                         //         if (slot == 64) { /* AA */ }
            //                         //         else            { /* spell */ }
            //                         //       });
            //                         //
            //                         //   Splitting it into its own event ID
            //                         //   required a second Fire() per cast
            //                         //   for no informational gain — a one-
            //                         //   byte branch inside the consumer is
            //                         //   strictly cheaper and more flexible.

            // Derived from AIBaseClient fields (re-verify offsets per build)
            OnDeath             = 30,  // IsDead false → true — inline-fired from
                                       //   HkOnBuffAdd + HkOnHeroActionState via
                                       //   CheckDeathForHero; no poll fallback.
            // OnRevive         = 31,  // REMOVED — not needed; re-derive from
            //                         //   !IsDead() in the consumer if you care.
            // OnLevelUp        = 32,  // REMOVED — poll Level() yourself if needed.
            OnNewPath           = 33,  // Waypoint list replaced
            // OnPlayAnimation  = 34,  // REMOVED — the AnimationId field offset
            //                         //   is not recoverable via IDA (no
            //                         //   RTTI string / reflection field name),
            //                         //   and AIBaseClient::PlayAnimation is not
            //                         //   hookable without additional RE work
            //                         //   (type_info has no direct code xrefs).
            //                         //   Consumers can use the SDK-layer
            //                         //   `AnimationTracker` which reads anim
            //                         //   state via CharacterData offsets.

            // OnIntegerPropertyChange: piggy-back fired from
            // `HkOnHeroActionState` after every action-state change. The
            // hook fires on essentially every player interaction (move,
            // stop, cast, dash, path rebuild) so this is a complete push
            // surface for ActionState transitions — no polling needed.
            //
            // `sender` = hero, `context` = 0, `intParam` = current
            // ActionState int (full 32-bit value, consumer dedupes by
            // comparing against its cached previous value per-hero).
            OnIntegerPropertyChange = 35,

            // Inline-detour on ControlRuntime::IssueOrder (raw-asm trampoline)
            // Fires on every move / attack / stop order dispatched through
            // the Riot control layer. intParam = order type byte.
            OnIssueOrder        = 40,

            // ── Piggy-back / derived events ─────────────────────────────────
            //
            // OnDash: fired from HkOnHeroActionState when a hero's AiManager
            // `IsDashing` byte transitions false→true. `context` = nav object
            // pointer, `intParam` = 1 when entering dash.
            OnDash              = 50,

            // OnStealth: fired from HkOnBuffAdd when the added buff's name
            // matches a known stealth identifier (Invisible, Camouflage,
            // *Stealth, *HideIn*, …). `context` = BuffData*, `intParam` = 1.
            OnStealth           = 51,

            // OnTurretAttack: fired from PollVmtSpellEvents when a turret's
            // ActiveSpellCast pointer transitions to a new cast. `sender` =
            // turret object, `context` = SpellCastInfo*, `intParam` = slot
            // (always 64 for turret AAs).
            OnTurretAttack      = 52,

            // OnDoCast: fired from HkOnFinishCast alongside OnFinishCast.
            // Semantic = "spell effect has happened" (projectile spawned or
            // instant hit resolved). For non-projectile spells this is the
            // same moment as OnFinishCast; for projectile spells the timing
            // is approximate because we don't track missile spawn directly.
            // `sender` = hero, `context` = SpellCastInfo*, `intParam` = slot.
            OnDoCast            = 53,
        };
    } // namespace Events

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

    // WaypointLayout removed Apr 25/2026 - STALE inline-vector snapshot at +0x3138 never
    // updated reliably on every move order. Use CoreAPI::Ai::CopyWaypoints (AiManagerInner authoritative).

    // =========================================================================
    // AiManager — AUTHORITATIVE movement / path / dash state
    // =========================================================================
    // Game stores the real nav state in a separate AiManager object accessible
    // through `NavGridRuntime::GetAiManagerInner(hero)`. That function decodes
    // an obfuscation table living at `hero + 0x4220` (see shim::DecodeAiMgr
    // in CoreEventHook.h for the C++ port of the decode routine).
    //
    // Layout of the RESULT returned by GetAiManagerInner — call it `nav`.
    //   nav + HasPath          (uint32, non-zero bit-flag when a path exists)
    //   nav + SegmentsCount    (uint32, number of Vec3 waypoints ahead)
    //   nav + PathEnd          (Vec3, final destination — best fingerprint
    //                           field because it changes on EVERY move order)
    //   nav + IsMoving / IsDashing / Velocity / ServerPos / PreviousPos …
    //
    // The obfuscation table lives at `hero + AiManagerInnerCompatLayout::Offset`
    // (= 0x4228 on 26.6 - **changed from 0x4220** in older builds, re-verify
    // on every patch). The decode produces a WRAPPER pointer; the real inner
    // state pointer is at `wrapper + RawInnerPtr` (= 0x10).
    //
    // IMPORTANT: all offsets in this block (AiManagerInnerCompatLayout,
    // AiManagerNavBaseLayout, AiManagerPathStateLayout, AiManagerNavDataLayout)
    // are RELATIVE struct field offsets, NOT RVAs. The header offset (Offset)
    // CAN shift between builds; the inner nav fields are usually stable.
    //
    // 26.6 verification (functions in IDA dump):
    //   * sub_2A9F10  = GetAiManager(hero)        -> reads `hero + 16936`
    //                                                = `hero + 0x4228`, runs
    //                                                  XOR/ANDNOT decode and
    //                                                  returns the WRAPPER ptr.
    //   * sub_2AB530  = GetAiManagerInner(hero)   -> identical decode, then
    //                                                returns `*(wrapper+0x10)`,
    //                                                confirming InnerManager.
    //   * sub_291D80 / sub_300540 use the canonical adjust chain:
    //         wrapper = GetAiManager(hero)
    //         inner   = *(wrapper + 0x10)
    //         tinfo   = *(inner + 0x8)
    //         final   =  inner + *(int*)(tinfo + 0x4) + 0x8
    //     -> matches AiManagerNavBaseLayout::{InnerTypePtr=0x8,
    //        InnerTypeAdjust=0x4, FinalBaseAdd=0x8}.
    //
    // To re-verify a specific field on a future patch: pick a small
    // accessor (e.g. an IsDashing getter), inspect the hero-relative
    // dereference; the chain is always:
    //     hero + Offset       -> obfuscation wrapper (decoded)
    //     wrapper + 0x10      -> inner nav-manager pointer
    //     inner + typeAdjust + 0x8 (= navBase) -> path/nav fields
    namespace AiManagerInnerCompatLayout {
        constexpr auto Offset              = 0x4228;  // verified 26.6 (was 0x4220 - sub_2A9EE0 reads hero+0x4228)
        constexpr auto InnerManager        = 0x10;    // verified 26.6 (sub_2AB500 returns *(wrapper+0x10))
        constexpr auto TargetPosition      = 0x34;
        constexpr auto Velocity            = 0x318;
        constexpr auto IsMoving            = 0x31C;
        constexpr auto CurrentSegment      = 0x320;
        // 13338 old build and old source read the live dash flag from the
        // decoded inner AiManager at inner+0x384. In 13337 the decode chain is
        // unchanged except for the hero header offset (0x4220 -> 0x4228), so
        // keep this as an inner-manager field, not navBase+0x384. Read this as
        // a byte; the adjacent bytes can contain nonzero dash/path data.
        constexpr auto IsDashing           = 0x384;
        constexpr auto PathStart           = 0x330;
        constexpr auto PathEnd             = 0x33C;   // Vec3 — primary fp input
        constexpr auto Segments            = 0x348;
        constexpr auto SegmentsCount       = 0x350;
        constexpr auto HasPath             = 0x354;
        constexpr auto DashSpeed           = 0x360;
        constexpr auto DashMaxRangeSq      = 0x374;
        constexpr auto DashDistRemaining   = 0x378;
        constexpr auto DashDuration        = 0x380;
        constexpr auto DashEndPos          = 0x3A8;
        constexpr auto ServerPos           = 0x474;
        constexpr auto MoveVec3            = 0x480;
        constexpr auto DashTargetNetId     = 0x48C;
        constexpr auto DashSecondaryNetId  = 0x490;
        constexpr auto PreviousPos         = 0x590;
    } // namespace AiManagerInnerCompatLayout

    // Alternate layout view (different field names / slightly different offsets)
    // used by some SDK plugins. Kept for completeness.
    namespace AiManagerNavBaseLayout {
        constexpr auto RawInnerPtr         = 0x10;
        constexpr auto InnerTypePtr        = 0x8;
        constexpr auto InnerTypeAdjust     = 0x4;
        constexpr auto FinalBaseAdd        = 0x8;   // class-adjust tail (from MCP IDA chain, not in generated dump)
        constexpr auto PathState           = 0x300;
        constexpr auto PathEndFallback     = 0x31C;
        constexpr auto Segments            = 0x328;
        constexpr auto SegmentsCount       = 0x330;
        constexpr auto ServerPosition      = 0x454;
        constexpr auto MoveVector          = 0x460;
        constexpr auto PreviousPosition    = 0x570;
        constexpr auto NavModeFlag         = 0x478;
        constexpr auto OrderPosition       = 0x520;
        constexpr auto NavUnknownDword     = 0x568;
        constexpr auto DashSpeedInner      = 0x360;
        constexpr auto IsDashingInner      = 0x384;
    } // namespace AiManagerNavBaseLayout

    // A nested path-state sub-object (walking a list of points with a cursor).
    namespace AiManagerPathStateLayout {
        constexpr auto CurrentIndex        = 0x0;
        constexpr auto FallbackEnd         = 0x1C;
        constexpr auto PointsPtr           = 0x28;
        constexpr auto Count               = 0x30;
    } // namespace AiManagerPathStateLayout

    // Detailed nav-data layout (dash calculations, server/client pos diff, etc.)
    namespace AiManagerNavDataLayout {
        constexpr auto MoveSpeed           = 0x02F8;
        constexpr auto NavFlag             = 0x02FD;
        constexpr auto ServerPosX          = 0x0310;
        constexpr auto ServerPosZ          = 0x0318;
        constexpr auto ServerPos           = 0x031C;
        constexpr auto SegmentsPtr         = 0x0328;
        constexpr auto SegmentsCount       = 0x0330;
        constexpr auto CurrentSegment      = 0x0334;
        constexpr auto DashSpeedCalc       = 0x0340;
        constexpr auto DashMaxRangeSq      = 0x0354;
        constexpr auto DashDistRemain      = 0x0358;
        constexpr auto IsDashingInner      = 0x035C;
        constexpr auto DashDuration        = 0x0360;
        constexpr auto PathStart           = 0x0388;
        constexpr auto PathEndFallback     = 0x03A0;
        constexpr auto ArrivedFlag         = 0x0450;
        constexpr auto CurrentPosX         = 0x0454;
        constexpr auto CurrentPosY         = 0x0458;
        constexpr auto CurrentPosZ         = 0x045C;
        constexpr auto DashTargetNetId     = 0x046C;
        constexpr auto DashSecondaryId     = 0x0470;
        constexpr auto MoveOverrideFlag    = 0x0474;
        constexpr auto PreviousPosX        = 0x0570;
        constexpr auto PreviousPosY        = 0x0574;
        constexpr auto PreviousPosZ        = 0x0578;
        constexpr auto VelocityX           = 0x0588;
        constexpr auto VelocityY           = 0x058C;
        constexpr auto VelocityZ           = 0x0590;
    } // namespace AiManagerNavDataLayout

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
        constexpr auto SlotLevel             = 0x1C;
        constexpr auto SlotLevelAlt          = 0x28;
        constexpr auto SlotCooldown          = 0x80;
        constexpr auto SlotTotalCd           = 0x88;
        constexpr auto SlotChargeTimer       = 0x30;
        constexpr auto SlotCooldownExpires   = 0x70;
        constexpr auto SlotStacks            = 0x5C;
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
        constexpr auto InputTargetNetId = 0x14;
        constexpr auto InputStartPos    = 0x18;
        constexpr auto InputEndPos      = 0x24;
    } // namespace SpellInputLayout

    namespace SpellInfoLayout {
        // CE verified: Info+0x60 = slot backref, Info+0x78 = SpellData ptr.
        constexpr auto InfoSpellData    = 0x78;
        constexpr auto SpellInfoNamePtr = 0x28;
    } // namespace SpellInfoLayout

    namespace SpellDataLayout {
        constexpr auto DataSpellName = 0x80;
        constexpr auto DataManaCost  = 0x5F4;
        constexpr auto DataResource  = 0x8;
    } // namespace SpellDataLayout

    namespace SpellDataResourceLayout {
        constexpr auto DataResourceBase = 0x60;
        // Legacy direct-field aliases kept for source compatibility.
        // Current 13337 build and old 13338 build both still expose the
        // SPELLPARAM table in the equivalent of old sub_339430:
        //   CASTRANGE    = 0x0F
        //   LINEWIDTH    = 0x1D
        //   MISSILESPEED = 0x27
        // The game now routes these through param-indexed getters instead of
        // treating 0x478/0x568/0x518 as stable float fields.
        constexpr auto ResCastRange     = 0x478;
        constexpr auto ResMissileSpeed  = 0x518;
        constexpr auto ResLineWidth     = 0x568;
        constexpr auto ResMaxAmmo       = 0x3C0;
        constexpr auto ResCastType      = 0x510;
        constexpr auto ResMissileSpec   = 0x508;
        constexpr auto ResScriptName    = 0x80;
        constexpr auto ResCooldownTime  = 0x304;
        constexpr auto ResAmmoRecharge  = 0x408;
        constexpr auto ResImgIconName   = 0x2A0;
    } // namespace SpellDataResourceLayout

    // ── Extended SpellCastInfo layout (event-received version) ───────────
    // NOTE: This is DIFFERENT from `SpellCastInfoLayout` above. The layout
    // defined higher up describes the object pointed to by
    // `SpellRuntime::ActiveSpellCast` (the spellbook's currently-casting
    // handle). The layout below describes what the game passes into the
    // OnProcessSpell / OnFinishCast / OnStopCast callbacks as `castInfo`.
    // Both layouts happen to overlap on some fields (e.g. CasterNetId)
    // but have different slot offsets for most members.
    //
    // Preferred by downstream SDK wrappers that consume hook callbacks.
    namespace SpellCastInfoEventLayout {
        constexpr auto SpellData        = 0x0;
        constexpr auto SrcIndex         = 0x98;
        constexpr auto TargetIndex      = 0x9C;
        constexpr auto StartPos         = 0xD8;
        constexpr auto EndPos           = 0xE4;
        constexpr auto CastPos          = 0xF0;
        constexpr auto CastDelay        = 0x118;
        constexpr auto IsSpell          = 0x134;
        constexpr auto IsSpecialAttack  = 0x13E;
        constexpr auto IsAuto           = 0x141;
        constexpr auto Slot             = 0x14C;
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
        constexpr auto InventoryComponent = 0x4DB8;
        constexpr auto SlotArray          = 0x50;
        constexpr auto SlotCount          = 39;
        constexpr auto ItemNode           = 0x10;
        constexpr auto ItemInfo           = 0x00;
        constexpr auto DataItemId         = 0xB4;
        constexpr auto DataAbilityHaste   = 0x160;
        constexpr auto DataHealth         = 0x164;
        constexpr auto DataArmor          = 0x19C;
        constexpr auto DataMR             = 0x1BC;
        constexpr auto DataAD             = 0x1D8;
        constexpr auto DataAP             = 0x1E0;
        constexpr auto DataAtkSpeedMult   = 0x20C;
    } // namespace ItemRuntime

    // ── Object type / classification ─────────────────────────────────────
    // TypeFlagsRuntime removed Apr 25/2026 - replaced by CoreClassification::Classify (RTTI/name pattern)

    namespace MinionClassRuntime {
        constexpr auto TypeOffset        = 0x4CB1;
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
        constexpr auto TypeOffset = 0x4AB4;
        constexpr auto Normal     = 0x0;
        constexpr auto Baron      = 0x1;
        constexpr auto Dragon     = 0x2;
    } // namespace JungleTypeRuntime

    namespace ClassificationRuntime {
        constexpr auto CompareTypeFlags = 0x2BA240;
        constexpr auto IsJungleMonster  = 0x223F90;
        constexpr auto GetJungleType    = 0x6A3C20;
        constexpr auto IsClone          = 0x2B85C0;
        constexpr auto IsBuilding       = 0x32C1E0;
        constexpr auto IsDead           = 0x2B8860;
        constexpr auto IsVulnerable     = 0x2B9520;
        constexpr auto IsDragon         = 0x2B8B10;
        constexpr auto IsElderDragon    = 0x2B8B80;
        constexpr auto IsBaron          = 0x2B7EF0;
        constexpr auto IsSelectable     = 0x226B30;
        constexpr auto IsFleeing        = 0x117C620;
        constexpr auto IsNoRender       = 0x223D10;
    } // namespace ClassificationRuntime

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
        constexpr auto Index               = 0x10;
        constexpr auto Team                = 0x259;  // byte team index
        constexpr auto Name                = 0x68;
        constexpr auto NetId               = 0xCC;
        constexpr auto Dead                = 0x250;
        constexpr auto Position            = 0x25C;
        constexpr auto Visibility          = 0x2E0;
        constexpr auto Visible             = 0x308;
        constexpr auto IsInvulnerable      = 0x5A0;
        // RecallState (legacy 0xF48) was discovered to be a std::vector data
        // pointer on 26.6 (sub_9F18FE constructor / sub_9F18A0 destructor pair
        // free `[obj+0xF48]` when `[obj+0xF54] >= 0`). The real recall
        // channel state should be derived from SpellRuntime::ActiveSpellCast.
        // Keeping the constant for ABI compatibility but new code MUST NOT
        // read it.
        constexpr auto RecallState         = 0xF48;
        constexpr auto Radius              = 0x6F8;
        constexpr auto CharacterData       = 0x4070;
        constexpr auto CharacterName       = 0x4368;
        constexpr auto Direction           = 0x21D8;
        constexpr auto EffectEmitterHandle = 0x258;
        constexpr auto MissileClientHandle = 0x2D8;
        constexpr auto ItemList            = 0x4DB8;  // = InventoryComponent
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
        constexpr auto ActionState2    = 0x14A8;
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
        constexpr auto AbilityHaste             = 0x1BE8;
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
        constexpr auto Exp                      = 0x4D30;
        constexpr auto LevelRef                 = 0x4D58;
        constexpr auto LevelUpPoints            = 0x4D80;
        constexpr auto VisionScore              = 0x5620;
        constexpr auto ShutdownValue            = 0x5648;
        constexpr auto BaseGoldOnDeath          = 0x5670;
        constexpr auto NeutralMinionsKilled     = 0x5698;
    } // namespace AIHeroClient

    namespace MissileClient {
        constexpr auto SpellDataPtr  = 0x128;
        constexpr auto CastInfoBase  = 0x2C0;
        constexpr auto SpellName     = 0x2E0;
        constexpr auto MissileName   = 0x308;
        constexpr auto CasterNetId   = 0x358;
        constexpr auto TargetNetId   = 0x35C;
        constexpr auto MissileNetId  = 0x364;
        constexpr auto StartPos      = 0x390;
        constexpr auto EndPos        = 0x39C;
        constexpr auto CastEndPos    = 0x39C;
        constexpr auto Position      = 0x25C;
    } // namespace MissileClient

    // ── Hack / bypass plumbing (old source SDK used these for zoom,
    //    skin change, DirectInput hook, etc.) ──────────────────────────
    // GadgetRuntime::ThreadTrampoline removed Apr 25/2026 - replaced by CoreEventHook::FindSpoofGadget (dynamic byte-pattern 'FF 23' search)

    namespace DirectInputRuntime {
        constexpr auto KeyboardInput     = 0x11928A0;
        constexpr auto MouseInput        = 0x1192AB0;
        constexpr auto KeyboardDevice    = 0x1EC6D38;
        constexpr auto KeyboardBuffer    = 0x1EC6D48;
        constexpr auto KeyboardCount     = 0x1EC6D68;
        constexpr auto KeyboardFlag      = 0x1EC6D88;  // 26.6 (was 0x1E81548)
        constexpr auto MouseDevice       = 0x1EC6D40;
        constexpr auto MouseBuffer       = 0x1EC6D50;
        constexpr auto MouseCount        = 0x1EC6D78;
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

        // R3nzSkin-style runtime path, IDA/CE verified on 26.7 (May 2026):
        // local + CharacterDataStack -> std::vector + base_skin.
        // base_skin.skin = stack + BaseSkin + CharacterStackSkin.
        // CharacterDataStackUpdate is sub_20BEF0(stack, change).
        // CharacterDataStackPush is sub_22A1D0(this, model, skin, ...).
        constexpr auto CharacterDataStack       = 0x4100;
        constexpr auto CharacterDataStackUpdate = 0x20BEF0;
        constexpr auto CharacterDataStackPush   = 0x22A1D0;  // (May 2026) verified
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
