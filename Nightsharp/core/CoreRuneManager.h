#pragma once

#include "Globals.h"
#include "offset.h"

#include <cstdint>
#include <cstring>

namespace CoreRuneManager {

inline constexpr int kMaxRuneEntries = 32;

struct RuneData {
    uintptr_t address = 0;
    int id = 0;
    char displayName[96] = {};
    char description[256] = {};

    bool IsValid() const { return address != 0 && id > 0; }
};

struct RuneTreeData {
    uintptr_t address = 0;
    int id = 0;
    char displayName[96] = {};
    char description[256] = {};

    bool IsValid() const { return address != 0 && id > 0; }
};

struct RuneEntry {
    uintptr_t address = 0;
    RuneData data = {};

    bool IsValid() const { return data.IsValid(); }
};

struct ManagerSnapshot {
    uintptr_t address = 0;
    RuneTreeData primaryTree = {};
    RuneTreeData secondaryTree = {};
    RuneEntry entries[kMaxRuneEntries] = {};
    int entryCount = 0;

    bool IsValid() const { return Globals::IsValidPtr(address); }
};

// Resolve the IRuneManager sub-object address from AIHeroClient.
// vfunc[0x808] returns hero + 0x50E8 (confirmed via sub_7FF72E8CA650).
// The RuneManager is an EMBEDDED sub-object (not a pointer), so we use
// hero + offset directly (constructor sub_7FF72E8A5DA0 writes the vtable
// at player+0x50E8).
inline uintptr_t Resolve(uintptr_t hero) {
    if (!Globals::IsValidPtr(hero)) {
        return 0;
    }
    return hero + Offset::AIHeroClient::RuneManager;
}

inline RuneData ReadRuneData(uintptr_t data) {
    RuneData result{};
    if (!Globals::IsValidPtr(data)) {
        return result;
    }

    result.address = data;
    result.id = Globals::Read<int>(data + Offset::RuneDataLayout::Id);
    Globals::ReadRuntimeStringField(
        data + Offset::RuneDataLayout::DisplayName,
        result.displayName,
        static_cast<int>(sizeof(result.displayName)));
    Globals::ReadRuntimeStringField(
        data + Offset::RuneDataLayout::Description,
        result.description,
        static_cast<int>(sizeof(result.description)));
    return result;
}

inline RuneTreeData ReadRuneTreeData(uintptr_t data) {
    RuneTreeData result{};
    if (!Globals::IsValidPtr(data)) {
        return result;
    }

    result.address = data;
    result.id = Globals::Read<int>(data + Offset::RuneTreeDataLayout::Id);
    Globals::ReadRuntimeStringField(
        data + Offset::RuneTreeDataLayout::DisplayName,
        result.displayName,
        static_cast<int>(sizeof(result.displayName)));
    Globals::ReadRuntimeStringField(
        data + Offset::RuneTreeDataLayout::Description,
        result.description,
        static_cast<int>(sizeof(result.description)));
    return result;
}

inline RuneTreeData ReadRuneTreeDataAt(uintptr_t addr) {
    RuneTreeData result{};
    if (!Globals::IsValidPtr(addr)) {
        return result;
    }
    result.address = addr;
    result.id = Globals::Read<int>(addr + Offset::RuneTreeDataLayout::Id);
    Globals::ReadRuntimeStringField(
        addr + Offset::RuneTreeDataLayout::DisplayName,
        result.displayName,
        static_cast<int>(sizeof(result.displayName)));
    Globals::ReadRuntimeStringField(
        addr + Offset::RuneTreeDataLayout::Description,
        result.description,
        static_cast<int>(sizeof(result.description)));
    return result;
}

inline RuneTreeData ReadPrimaryTree(uintptr_t manager) {
    if (!Globals::IsValidPtr(manager)) {
        return {};
    }
    // Try pointer at offset first
    const uintptr_t data = Globals::Read<uintptr_t>(
        manager + Offset::RuneManagerLayout::PrimaryRuneTree);
    RuneTreeData result = ReadRuneTreeData(data);
    if (result.IsValid()) {
        return result;
    }
    // Fallback: tree data embedded directly at offset
    return ReadRuneTreeDataAt(
        manager + Offset::RuneManagerLayout::PrimaryRuneTree);
}

inline RuneTreeData ReadSecondaryTree(uintptr_t manager) {
    if (!Globals::IsValidPtr(manager)) {
        return {};
    }
    const uintptr_t data = Globals::Read<uintptr_t>(
        manager + Offset::RuneManagerLayout::SecondaryRuneTree);
    RuneTreeData result = ReadRuneTreeData(data);
    if (result.IsValid()) {
        return result;
    }
    return ReadRuneTreeDataAt(
        manager + Offset::RuneManagerLayout::SecondaryRuneTree);
}

inline int ReadEntries(uintptr_t manager, RuneEntry* out, int maxOut) {
    if (!out || maxOut <= 0 || !Globals::IsValidPtr(manager)) {
        return 0;
    }

    const uintptr_t begin = Globals::Read<uintptr_t>(
        manager + Offset::RuneManagerLayout::RuneEntriesBegin);
    const uintptr_t end = Globals::Read<uintptr_t>(
        manager + Offset::RuneManagerLayout::RuneEntriesEnd);
    if (!Globals::IsValidPtr(begin) || !Globals::IsValidPtr(end) || end <= begin) {
        return 0;
    }

    const uintptr_t bytes = end - begin;
    const uintptr_t count = bytes / Offset::RuneEntryLayout::Stride;
    if (count > static_cast<uintptr_t>(kMaxRuneEntries)) {
        return 0;
    }
    const int clipped = count < static_cast<uintptr_t>(maxOut)
        ? static_cast<int>(count)
        : maxOut;

    int written = 0;
    for (int index = 0; index < clipped; ++index) {
        const uintptr_t entry = begin +
            static_cast<uintptr_t>(index) * Offset::RuneEntryLayout::Stride;
        const uintptr_t runeData = Globals::Read<uintptr_t>(
            entry + Offset::RuneEntryLayout::RuneData);
        RuneEntry decoded{};
        decoded.address = entry;
        decoded.data = ReadRuneData(runeData);
        if (decoded.IsValid()) {
            out[written++] = decoded;
        }
    }
    return written;
}

inline ManagerSnapshot ReadFromManager(uintptr_t manager) {
    ManagerSnapshot snapshot{};
    if (!Globals::IsValidPtr(manager)) {
        return snapshot;
    }

    snapshot.address = manager;
    snapshot.primaryTree = ReadPrimaryTree(manager);
    snapshot.secondaryTree = ReadSecondaryTree(manager);
    snapshot.entryCount = ReadEntries(
        manager,
        snapshot.entries,
        kMaxRuneEntries);
    return snapshot;
}

inline ManagerSnapshot ReadFromHero(uintptr_t hero) {
    return ReadFromManager(Resolve(hero));
}

} // namespace CoreRuneManager
