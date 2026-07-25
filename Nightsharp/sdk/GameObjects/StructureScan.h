#pragma once

// ============================================================================
// StructureScan.h - SEH-guarded, name-based detection of turret / inhibitor /
// nexus objects, with crash-dump + breadcrumb instrumentation.
// ----------------------------------------------------------------------------
// Structures are not tracked by any typed manager, so they are found by walking
// the full object array and classifying each object by its model name
// (All::Name) and character name (All::CharacterName). The reads that touch the
// live game object are wrapped in a dedicated __try/__except so that:
//   * a faulting read (e.g. an object freed by the game between enumeration and
//     read) is caught instead of taking down the whole process,
//   * a full minidump is written with a clear stage the very first times it
//     faults (C:\Users\Public\NightSharpDumps\), and
//   * the offending object address + name is flushed to the crash log
//     (C:\Users\Public\nightsharp_crash.txt) so the exact object is known even
//     when the minidump path fails.
// This function contains only POD locals (no objects needing unwinding), so it
// is legal to mix C++ calls with __try/__except (avoids MSVC C2712).
// ============================================================================

#include "../../core/Globals.h"
#include "../../core/offset.h"
#include "../../core/CoreObjects.h"
#include "../../core/CoreObjectManager.h"
#include "../../DebugLog.h"
#include "../../CrashReporter.h"
#include "../../CrashTrace.h"

#include <cstdint>
#include <cstring>

// Set to 0 to silence per-refresh breadcrumb logging once the crash is fixed.
#ifndef NIGHTSHARP_STRUCTURE_SCAN_DEBUG
#define NIGHTSHARP_STRUCTURE_SCAN_DEBUG 1
#endif

namespace SDK::GameObjects::detail {

enum class StructureKind { None, Turret, Inhibitor, Nexus };

inline const char* StructureKindName(StructureKind kind) {
    switch (kind) {
    case StructureKind::Turret:    return "turret";
    case StructureKind::Inhibitor: return "inhibitor";
    case StructureKind::Nexus:     return "nexus";
    default:                       return "none";
    }
}

// Classify an object purely by its C++ vtable pointer (obj+0x0). This is how the
// game (and EnsoulSharp, via `sender as AITurretClient`) actually distinguishes
// object types: every turret shares one vtable, every inhibitor another, every
// nexus another. Particle / audio / effect objects whose MODEL name merely
// contains "Turret"/"Nexus"/"Barracks" have a DIFFERENT vtable and are rejected.
// Reading obj+0x0 is the single safest read possible (offset 0 exists on every
// live object), so this never touches deep offsets on small/foreign objects.
inline StructureKind ClassifyStructureVtable(uintptr_t vtable) {
    if (!vtable || !Globals::base) {
        return StructureKind::None;
    }
    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    // if (vtable == Globals::base + Offset::StructureVTable::AITurretClient) {
    //     return StructureKind::Turret;
    // }
    // if (vtable == Globals::base + Offset::StructureVTable::BarracksDampenerClient) {
    //     return StructureKind::Inhibitor;
    // }
    // if (vtable == Globals::base + Offset::StructureVTable::HQClient) {
    //     return StructureKind::Nexus;
    // }
    // REMOVED: Turret/Inhibitor/Nexus disabled
    (void)vtable;
    return StructureKind::None;
}

// POD result of probing one object. No destructors -> safe under __try.
struct StructureInfo {
    StructureKind kind = StructureKind::None;
    std::uint32_t index = 0;
    std::uint32_t networkId = 0;
    std::uint32_t team = 0;
    char name[128] = {};
    char charName[128] = {};
};

// __except filter: dump the first few faults with a full minidump, then log-only
// so a persistent fault cannot spam gigabytes of dumps.
inline LONG StructureProbeFilter(EXCEPTION_POINTERS* ep) {
    NightSharpDebug::CrashTrace::Record(
        nscrash::TraceTag::StructureScan,
        ep && ep->ExceptionRecord
            ? reinterpret_cast<std::uint64_t>(ep->ExceptionRecord->ExceptionAddress)
            : 0,
        ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0);
    static volatile LONG s_faultCount = 0;
    const LONG n = InterlockedIncrement(&s_faultCount);
    if (n == 1) {
        // First fault: full minidump + metadata (.dmp/.txt in NightSharpDumps).
        return NightSharpDebug::CrashReporter::LogAndDumpException(
            "GameObjects::StructureProbe", ep);
    }
    // Later faults: register/module log only, no giant repeat dumps.
    return NightSharpDebug::LogException(
        "GameObjects::StructureProbe(repeat)", ep);
}

// Reads name/character-name + identity/team for one object under SEH. Returns
// false if any read faulted (a minidump/log was produced); on false the caller
// must skip the object.
inline bool ProbeStructureGuarded(uintptr_t address, StructureInfo& out) {
    out.kind = StructureKind::None;
    out.index = 0;
    out.networkId = 0;
    out.team = 0;
    out.name[0] = 0;
    out.charName[0] = 0;

    __try {
        if (!Globals::IsValidPtr(address)) {
            return true;
        }

        // Classify by vtable (obj+0x0) FIRST. This is the safest possible read
        // and rejects every non-structure (minions/missiles/particles/effects)
        // before any deep field is ever touched -- which is what made the old
        // name-scan crash (it read object fields such as name/team/character-name on foreign objects).
        const uintptr_t vtable = Globals::Read<uintptr_t>(address);
        out.kind = ClassifyStructureVtable(vtable);
        if (out.kind == StructureKind::None) {
            return true;
        }

        NightSharpDebug::CrashTrace::Record(
            nscrash::TraceTag::StructureScan,
            address,
            static_cast<std::uint64_t>(out.kind));

        // Confirmed real structure -> now it is safe to read its identity/name.
        ::Core::Objects::ReadName(address, out.name, static_cast<int>(sizeof(out.name)));
        out.index = ::Core::Objects::ReadIndex(address);
        out.networkId = ::Core::Objects::ReadNetworkId(address);
        out.team = ::Core::Objects::ReadTeamValue(address);
        return true;
    }
    __except (StructureProbeFilter(GetExceptionInformation())) {
        NightSharpDebug::Logf(
            "[StructScan] FAULT probing addr=0x%p name='%s' char='%s' kind=%s",
            reinterpret_cast<void*>(address),
            out.name,
            out.charName,
            StructureKindName(out.kind));
        return false;
    }
}

} // namespace SDK::GameObjects::detail
