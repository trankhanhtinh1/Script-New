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

inline bool IsVectorRangeSane(uintptr_t begin, uintptr_t end) {
    if (!Globals::IsValidPtr(begin) || !Globals::IsValidPtr(end) || end < begin) {
        return false;
    }

    const uintptr_t bytes = end - begin;
    return bytes % Offset::RuneEntryLayout::Stride == 0 &&
           bytes / Offset::RuneEntryLayout::Stride <= kMaxRuneEntries;
}

inline bool LooksLikeManager(uintptr_t manager) {
    if (!Globals::IsValidPtr(manager)) {
        return false;
    }

    const uintptr_t begin = Globals::Read<uintptr_t>(
        manager + Offset::RuneManagerLayout::RuneEntriesBegin);
    const uintptr_t end = Globals::Read<uintptr_t>(
        manager + Offset::RuneManagerLayout::RuneEntriesEnd);
    if (IsVectorRangeSane(begin, end)) {
        return true;
    }

    const uintptr_t primaryTree = Globals::Read<uintptr_t>(
        manager + Offset::RuneManagerLayout::PrimaryRuneTree);
    const uintptr_t secondaryTree = Globals::Read<uintptr_t>(
        manager + Offset::RuneManagerLayout::SecondaryRuneTree);
    return Globals::IsValidPtr(primaryTree) || Globals::IsValidPtr(secondaryTree);
}

inline uintptr_t ResolveFromField(uintptr_t hero) {
    if (!Globals::IsValidPtr(hero)) {
        return 0;
    }

    const uintptr_t pointed = Globals::Read<uintptr_t>(
        hero + Offset::AIHeroClient::RuneManager);
    if (LooksLikeManager(pointed)) {
        return pointed;
    }

    const uintptr_t embedded = hero + Offset::AIHeroClient::RuneManager;
    return LooksLikeManager(embedded) ? embedded : 0;
}

inline uintptr_t ResolveFromVFunc(uintptr_t hero) {
    if (!Globals::IsValidPtr(hero)) {
        return 0;
    }

    __try {
        const uintptr_t vtable = Globals::Read<uintptr_t>(hero);
        if (!Globals::IsValidPtr(vtable)) {
            return 0;
        }

        const uintptr_t function = Globals::Read<uintptr_t>(
            vtable + Offset::RuneManagerRuntime::GetRuneManagerVFunc);
        if (!Globals::IsExecutablePtr(function)) {
            return 0;
        }

        using GetRuneManagerFn = uintptr_t(__fastcall*)(uintptr_t);
        const uintptr_t manager = reinterpret_cast<GetRuneManagerFn>(function)(hero);
        return LooksLikeManager(manager) ? manager : 0;
    }
    __except (1) {
        return 0;
    }
}

inline uintptr_t Resolve(uintptr_t hero) {
    const uintptr_t fromField = ResolveFromField(hero);
    if (fromField) {
        return fromField;
    }
    return ResolveFromVFunc(hero);
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
    if (!IsVectorRangeSane(begin, end)) {
        return 0;
    }

    const uintptr_t count = (end - begin) / Offset::RuneEntryLayout::Stride;
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

inline ManagerSnapshot ReadManager(uintptr_t manager) {
    ManagerSnapshot snapshot{};
    if (!LooksLikeManager(manager)) {
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

inline ManagerSnapshot Read(uintptr_t hero) {
    return ReadManager(Resolve(hero));
}

} // namespace CoreRuneManager
