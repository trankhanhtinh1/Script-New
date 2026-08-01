#pragma once

#include "CoreObjects.h"
#include "CoreRuntime.h"
#include "Globals.h"
#include "offset.h"

#include <cstdint>
#include <cstring>

namespace Core::ObjectManager {

using Core::Objects::ObjectHandle;
using Core::Objects::ObjectSnapshot;
using Core::Objects::ObjectType;

inline constexpr std::uint32_t kMaxObjectArrayCount = 0x10000;
inline constexpr int kMaxManagerListCount = 8192;

enum class ManagerKind {
    Objects,
    Heroes,
    Minions,
    Turrets,
    Missiles,
};

struct ManagerView {
    uintptr_t manager = 0;
    uintptr_t items = 0;
    std::uint32_t count = 0;

    bool IsValid() const {
        return Globals::IsValidPtr(manager) && Globals::IsValidPtr(items) && count > 0;
    }
};

// -------------------------------------------------------------------------
// Object type cache
// InferType uses typed manager lists instead of native classification
// predicates. Cache the result by object address; types are stable for an
// object's lifetime. 512 buckets covers a full game without eviction.
// Same-address reuse after object death is safe: a new object of the same
// type produces the same cached result; a different type (rare pool reuse)
// overwrites the slot on first access via the hash collision path.
// -------------------------------------------------------------------------
namespace TypeCache {
    constexpr int kBuckets = 512; // must be power of 2
    struct Slot { uintptr_t address = 0; ObjectType type = ObjectType::Unknown; };
    inline Slot g_slots[kBuckets] = {};

    inline int Bucket(uintptr_t address) {
        // Shift by 4 to discard alignment bits; mask to bucket count.
        return static_cast<int>((address >> 4) & (kBuckets - 1));
    }

    // Returns Unknown when address is not cached.
    inline ObjectType Lookup(uintptr_t address) {
        const int b = Bucket(address);
        return (g_slots[b].address == address) ? g_slots[b].type : ObjectType::Unknown;
    }

    // Unknown types are not stored (Unknown is the "empty" sentinel).
    inline void Store(uintptr_t address, ObjectType type) {
        if (type == ObjectType::Unknown) return;
        const int b = Bucket(address);
        g_slots[b] = {address, type};
    }

    // Invalidate a single entry (e.g. on object death if hooked).
    inline void Invalidate(uintptr_t address) {
        const int b = Bucket(address);
        if (g_slots[b].address == address) g_slots[b] = {};
    }

    // Full clear — call on major reinitialisation.
    inline void InvalidateAll() {
        for (auto& s : g_slots) s = {};
    }
} // namespace TypeCache

// EnsureReady no longer forces RefreshReadState on every manager access.
// The overlay calls CoreRuntime::TickRead() once per frame before plugins run,
// so the context is always fresh by the time enumeration happens.
// Forward declarations for structure filter (defined below)
inline ObjectType InferType(uintptr_t object);
inline ObjectType InferLifecycleType(uintptr_t object);

inline bool EnsureReady() {
    if (!CoreRuntime::EnsureInitialized()) {
        return false;
    }
    return Globals::IsValidPtr(CoreRuntime::GetContext().objectManager);
}

inline uintptr_t GetManagerAddress(ManagerKind kind) {
    EnsureReady();
    const auto& ctx = CoreRuntime::GetContext();
    switch (kind) {
    case ManagerKind::Objects: return ctx.objectManager;
    case ManagerKind::Heroes: return ctx.heroManager;
    case ManagerKind::Minions: return ctx.minionManager;
    case ManagerKind::Turrets: return ctx.turretManager;
    case ManagerKind::Missiles: return ctx.missileManager;
    default: return 0;
    }
}

inline ManagerView ReadObjectArrayView(uintptr_t manager) {
    ManagerView view{};
    view.manager = manager;
    if (!Globals::IsValidPtr(manager)) {
        return view;
    }

    view.items = Globals::Read<uintptr_t>(manager + 0x20);
    view.count = Globals::Read<std::uint32_t>(manager + 0x28);
    if (!Globals::IsValidPtr(view.items) || view.count == 0 || view.count > kMaxObjectArrayCount) {
        view.items = 0;
        view.count = 0;
    }
    return view;
}

inline ManagerView ReadManagerListView(uintptr_t manager) {
    ManagerView view{};
    view.manager = manager;
    if (!Globals::IsValidPtr(manager)) {
        return view;
    }

    view.items = Globals::Read<uintptr_t>(manager + Offset::ObjectManagerRuntime::ManagerListItems);
    const int count = Globals::Read<int>(manager + Offset::ObjectManagerRuntime::ManagerListSize);
    if (!Globals::IsValidPtr(view.items) || count <= 0 || count > kMaxManagerListCount) {
        view.items = 0;
        view.count = 0;
        return view;
    }

    view.count = static_cast<std::uint32_t>(count);
    return view;
}

inline bool IsLiveEntry(uintptr_t entry) {
    return Globals::IsValidPtr(entry) && ((entry & 1u) == 0);
}

inline bool LooksLikeGameObject(uintptr_t object) {
    if (!IsLiveEntry(object)) {
        return false;
    }

    const uintptr_t vtable = Globals::Read<uintptr_t>(object);
    if (!Globals::IsValidPtr(vtable)) {
        return false;
    }

    const uintptr_t firstVirtual = Globals::Read<uintptr_t>(vtable);
    return Globals::IsExecutablePtr(firstVirtual, 8);
}

inline uintptr_t ResolveMissileManagerEntry(uintptr_t entry) {
    if (LooksLikeGameObject(entry)) {
        return entry;
    }

    const uintptr_t object = Globals::Read<uintptr_t>(
        entry + Offset::ObjectManagerRuntime::MissileManagerNodeObject);
    return LooksLikeGameObject(object) ? object : 0;
}

inline int EnumerateObjectArray(uintptr_t manager, uintptr_t* out, int maxOut) {
    if (!out || maxOut <= 0) {
        return 0;
    }

    const ManagerView view = ReadObjectArrayView(manager);
    if (!view.IsValid()) {
        return 0;
    }

    int written = 0;
    for (std::uint32_t i = 0; i < view.count && written < maxOut; ++i) {
        const uintptr_t entry = Globals::Read<uintptr_t>(view.items + static_cast<uintptr_t>(i) * sizeof(uintptr_t));
        if (!IsLiveEntry(entry)) {
            continue;
        }

        // REMOVED: Filter out Turret/Inhibitor/Nexus objects from enumeration
        // so plugins never receive structure objects (user request).
        const ObjectType otype = InferLifecycleType(entry);
        if (otype == ObjectType::AITurretClient ||
            otype == ObjectType::BarracksDampenerClient ||
            otype == ObjectType::HQClient) {
            continue;
        }

        out[written++] = entry;
    }
    return written;
}
// Awareness/structure consumers need the same guarded object-array walk as
// EnumerateAll, but the general object cache intentionally excludes structures
// for legacy orbwalker behavior. Keep that behavior unchanged and expose an
// explicit opt-in traversal instead.
inline int EnumerateAllIncludingStructures(uintptr_t* out, int maxOut) {
    if (!out || maxOut <= 0) {
        return 0;
    }

    const ManagerView view = ReadObjectArrayView(GetManagerAddress(ManagerKind::Objects));
    if (!view.IsValid()) {
        return 0;
    }

    int written = 0;
    for (std::uint32_t i = 0; i < view.count && written < maxOut; ++i) {
        const uintptr_t entry =
            Globals::Read<uintptr_t>(view.items + static_cast<uintptr_t>(i) * sizeof(uintptr_t));
        if (!IsLiveEntry(entry)) {
            continue;
        }
        out[written++] = entry;
    }
    return written;
}


inline int EnumerateMissileManagerList(uintptr_t manager, uintptr_t* out, int maxOut) {
    if (!out || maxOut <= 0) {
        return 0;
    }

    const ManagerView view = ReadManagerListView(manager);
    if (!view.IsValid()) {
        return 0;
    }

    int written = 0;
    for (std::uint32_t i = 0; i < view.count && written < maxOut; ++i) {
        const uintptr_t entry = Globals::Read<uintptr_t>(
            view.items + static_cast<uintptr_t>(i) * sizeof(uintptr_t));
        if (!IsLiveEntry(entry)) {
            continue;
        }

        const uintptr_t object = ResolveMissileManagerEntry(entry);
        if (object) {
            out[written++] = object;
        }
    }
    return written;
}

inline int EnumerateManagerList(uintptr_t manager, uintptr_t* out, int maxOut) {
    if (!out || maxOut <= 0) {
        return 0;
    }

    const ManagerView view = ReadManagerListView(manager);
    if (!view.IsValid()) {
        return 0;
    }

    int written = 0;
    for (std::uint32_t i = 0; i < view.count && written < maxOut; ++i) {
        const uintptr_t entry = Globals::Read<uintptr_t>(view.items + static_cast<uintptr_t>(i) * sizeof(uintptr_t));
        if (!IsLiveEntry(entry)) {
            continue;
        }

        out[written++] = entry;
    }
    return written;
}

inline int Enumerate(ManagerKind kind, uintptr_t* out, int maxOut) {
    if (!out || maxOut <= 0 || !EnsureReady()) {
        return 0;
    }

    const uintptr_t manager = GetManagerAddress(kind);
    if (kind == ManagerKind::Objects) {
        return EnumerateObjectArray(manager, out, maxOut);
    }
    if (kind == ManagerKind::Missiles) {
        return EnumerateMissileManagerList(manager, out, maxOut);
    }
    return EnumerateManagerList(manager, out, maxOut);
}

inline int EnumerateAll(uintptr_t* out, int maxOut) {
    return Enumerate(ManagerKind::Objects, out, maxOut);
}

inline int EnumerateHeroes(uintptr_t* out, int maxOut) {
    return Enumerate(ManagerKind::Heroes, out, maxOut);
}

inline int EnumerateMinions(uintptr_t* out, int maxOut) {
    return Enumerate(ManagerKind::Minions, out, maxOut);
}

inline int EnumerateTurrets(uintptr_t* out, int maxOut) {
    return Enumerate(ManagerKind::Turrets, out, maxOut);
}

inline int EnumerateMissiles(uintptr_t* out, int maxOut) {
    return Enumerate(ManagerKind::Missiles, out, maxOut);
}

inline uintptr_t FindByIndex(std::uint32_t index) {
    if (index == 0 || !EnsureReady()) {
        return 0;
    }

    const ManagerView view = ReadObjectArrayView(CoreRuntime::GetContext().objectManager);
    if (!view.IsValid()) {
        return 0;
    }

    const std::uint32_t slot = static_cast<std::uint32_t>(static_cast<std::uint16_t>(index));
    if (slot >= view.count) {
        return 0;
    }

    const uintptr_t object = Globals::Read<uintptr_t>(view.items + static_cast<uintptr_t>(slot) * sizeof(uintptr_t));
    if (!IsLiveEntry(object)) {
        return 0;
    }

    return Core::Objects::ReadIndex(object) == index ? object : 0;
}

// Walk the MSVC std::map<uint32, GameObject*> red-black tree maintained by
// managers at a caller-selected tree field offset. Verified on IDA 13337 against
// sub_560700 (object insert) and sub_2F6930 (object destroy/unregister) —
// both walk identical trees keyed on `obj+0xCC` (NetworkId).
//
// ObjectManager uses +0x38; MissileManager uses +0x08.
// The tree field holds a pointer to the head sentinel.
// Sentinel layout (head): left=leftmost, parent=root, right=rightmost.
// Node layout: left=+0x00, parent=+0x08, right=+0x10, color=+0x18,
//              is_nil=+0x19, key(uint32)=+0x20, value(GameObject*)=+0x28.
// Search direction matches std::map lower_bound: go LEFT when key >= target,
// RIGHT otherwise; remember the smallest candidate >= target along the way.
inline uintptr_t FindInNetworkIdTree(
    uintptr_t manager,
    std::uint32_t networkId,
    std::uintptr_t treeOffset) {
    if (!Globals::IsValidPtr(manager)) {
        return 0;
    }

    const uintptr_t head = Globals::Read<uintptr_t>(manager + treeOffset);
    if (!Globals::IsValidPtr(head)) {
        return 0;
    }

    uintptr_t node = Globals::Read<uintptr_t>(
        head + Offset::ObjectManagerRuntime::NetworkIdTreeRootSlot);

    // Bound the walk so corrupt links can't loop us forever. Live trees stay
    // well under depth 40 even with 10k+ entries (red-black guarantees
    // 2*log2(N+1) max depth), so 64 is comfortably defensive.
    constexpr int kMaxTreeDepth = 64;
    uintptr_t candidate = 0;
    for (int depth = 0; depth < kMaxTreeDepth; ++depth) {
        if (!Globals::IsValidPtr(node)) {
            break;
        }
        const auto isNil = Globals::Read<std::uint8_t>(
            node + Offset::ObjectManagerRuntime::NetworkIdTreeNodeIsNil);
        if (isNil) {
            break;
        }

        const auto key = Globals::Read<std::uint32_t>(
            node + Offset::ObjectManagerRuntime::NetworkIdTreeNodeKey);
        if (key >= networkId) {
            candidate = node;
            node = Globals::Read<uintptr_t>(
                node + Offset::ObjectManagerRuntime::NetworkIdTreeNodeLeft);
        } else {
            node = Globals::Read<uintptr_t>(
                node + Offset::ObjectManagerRuntime::NetworkIdTreeNodeRight);
        }
    }

    if (!candidate) {
        return 0;
    }
    const auto candidateKey = Globals::Read<std::uint32_t>(
        candidate + Offset::ObjectManagerRuntime::NetworkIdTreeNodeKey);
    if (candidateKey != networkId) {
        return 0;
    }

    const uintptr_t object = Globals::Read<uintptr_t>(
        candidate + Offset::ObjectManagerRuntime::NetworkIdTreeNodeObject);
    return Globals::IsValidPtr(object) ? object : 0;
}

inline uintptr_t FindByNetworkId(std::uint32_t networkId) {
    if (networkId == 0 || networkId == 0xFFFFFFFFu || !EnsureReady()) {
        return 0;
    }

    const auto& ctx = CoreRuntime::GetContext();

    // Fast path: walk native NetworkId red-black trees in O(log N). The main
    // ObjectManager tree is the canonical lookup used by the game; MissileManager
    // has a separate map keyed by missile network id.
    struct NetworkIdTreeSource {
        uintptr_t manager;
        std::uintptr_t treeOffset;
    };
    const NetworkIdTreeSource managers[] = {
        { ctx.objectManager, Offset::ObjectManagerRuntime::NetworkIdTree },
        { ctx.missileManager, Offset::ObjectManagerRuntime::MissileManagerNetworkIdTree },
    };
    for (const NetworkIdTreeSource& source : managers) {
        const uintptr_t object =
            FindInNetworkIdTree(source.manager, networkId, source.treeOffset);
        if (Globals::IsValidPtr(object) &&
            Core::Objects::ReadNetworkId(object) == networkId) {
            return object;
        }
    }

    // Fallback: linear scan of the main object array. Covers the long tail
    // of objects that aren't tracked in any typed manager tree.
    //
    // NOTE: The previous implementation tried `FindByIndex(networkId)` first
    // as a shortcut. That only worked when NetId aliased Index (old layout
    // where both fields lived at obj+0x20). After the 13337 fix moved NetId
    // to obj+0xCC the shortcut would silently return wrong objects, so it
    // is intentionally removed.
    const ManagerView view = ReadObjectArrayView(ctx.objectManager);
    if (!view.IsValid()) {
        return 0;
    }

    for (std::uint32_t i = 0; i < view.count; ++i) {
        const uintptr_t object = Globals::Read<uintptr_t>(
            view.items + static_cast<uintptr_t>(i) * sizeof(uintptr_t));
        if (!IsLiveEntry(object)) {
            continue;
        }
        if (Core::Objects::ReadNetworkId(object) == networkId) {
            return object;
        }
    }
    return 0;
}

inline bool Resolve(ObjectHandle& handle) {
    if (!EnsureReady()) {
        handle.address = 0;
        return false;
    }

    uintptr_t object = FindByIndex(handle.index);
    if (Globals::IsValidPtr(object) &&
        (!handle.HasIdentity() || Core::Objects::ReadNetworkId(object) == handle.networkId)) {
        handle.address = object;
        handle.index = Core::Objects::ReadIndex(object);
        handle.networkId = Core::Objects::ReadNetworkId(object);
        return handle.IsValid();
    }

    object = FindByNetworkId(handle.networkId);
    if (Globals::IsValidPtr(object)) {
        handle.address = object;
        handle.index = Core::Objects::ReadIndex(object);
        handle.networkId = Core::Objects::ReadNetworkId(object);
        return handle.IsValid();
    }

    handle.address = 0;
    return false;
}

inline ObjectHandle MakeHandle(uintptr_t object, ObjectType type = ObjectType::Unknown) {
    return ::Core::Objects::MakeHandle(object, type);
}

inline ObjectHandle PlayerHandle() {
    EnsureReady();
    const uintptr_t player = CoreRuntime::GetContext().localPlayer;
    return ::Core::ObjectManager::MakeHandle(player, ObjectType::AIHeroClient);
}

inline uintptr_t PlayerAddress() {
    EnsureReady();
    return CoreRuntime::GetContext().localPlayer;
}

inline bool ManagerContains(ManagerKind kind, uintptr_t object) {
    if (!Globals::IsValidPtr(object)) {
        return false;
    }

    const uintptr_t manager = GetManagerAddress(kind);
    const ManagerView view = kind == ManagerKind::Objects
        ? ReadObjectArrayView(manager)
        : ReadManagerListView(manager);
    if (!view.IsValid()) {
        return false;
    }

    for (std::uint32_t i = 0; i < view.count; ++i) {
        const uintptr_t entry = Globals::Read<uintptr_t>(view.items + static_cast<uintptr_t>(i) * sizeof(uintptr_t));
        if (!IsLiveEntry(entry)) {
            continue;
        }
        if (Core::Objects::SameObject(entry, object)) {
            return true;
        }
    }
    return false;
}

inline ObjectType InferType(uintptr_t object) {
    if (!Globals::IsValidPtr(object)) {
        return ObjectType::Unknown;
    }

    // Fast path: check type cache before making any native game calls.
    const ObjectType cached = TypeCache::Lookup(object);
    if (cached != ObjectType::Unknown) {
        return cached;
    }

    ObjectType result = ObjectType::GameObject;
    if (object == PlayerAddress()) {
        result = ObjectType::AIHeroClient;
    } else if (ManagerContains(ManagerKind::Heroes, object)) {
        result = ObjectType::AIHeroClient;
    } else if (ManagerContains(ManagerKind::Minions, object)) {
        result = ObjectType::AIMinionClient;
    } else if (ManagerContains(ManagerKind::Turrets, object)) {
        result = ObjectType::AITurretClient;
    } else if (ManagerContains(ManagerKind::Missiles, object)) {
        result = ObjectType::MissileClient;
    }

    TypeCache::Store(object, result);
    return result;
}

inline ObjectType InferLifecycleType(uintptr_t object) {
    // InferType already consults and populates the type cache for the common
    // typed objects (Hero/Minion/Turret/Missile). Only proceed to the slower
    // name-scan path for objects InferType leaves as GameObject/Unknown.
    ObjectType type = InferType(object);
    if (type != ObjectType::GameObject && type != ObjectType::Unknown) {
        return type;
    }

    // Check cache again before the expensive name-based scan.
    // (InferType stores non-Unknown results, so a cache hit here means the
    // name scan already ran and produced a specific type on a prior frame.)
    const ObjectType cached = TypeCache::Lookup(object);
    if (cached != ObjectType::Unknown) {
        return cached;
    }

    // MissileClient has no verified direct predicate in this build.
    if (ManagerContains(ManagerKind::Missiles, object)) {
        TypeCache::Store(object, ObjectType::MissileClient);
        return ObjectType::MissileClient;
    }

    const auto snapshot =
        Core::Objects::ReadSnapshot(object, ObjectType::GameObject);
    const char* name = snapshot.characterName[0]
        ? snapshot.characterName
        : snapshot.name;

    ObjectType resolved = type;
    if (Core::Objects::ContainsInsensitive(name, "barracksdampener") ||
        Core::Objects::ContainsInsensitive(name, "inhibitor")) {
        resolved = ObjectType::BarracksDampenerClient;
    } else if (Core::Objects::ContainsInsensitive(name, "nexus") ||
               Core::Objects::ContainsInsensitive(name, "hq")) {
        resolved = ObjectType::HQClient;
    }
    if (Core::Objects::ContainsInsensitive(name, "shop")) {
        resolved = ObjectType::ShopClient;
    } else if (Core::Objects::ContainsInsensitive(name, "spawnpoint") ||
               Core::Objects::ContainsInsensitive(name, "spawn_point")) {
        resolved = ObjectType::Obj_SpawnPoint;
    } else if (Core::Objects::ContainsInsensitive(name, "effect") ||
               Core::Objects::ContainsInsensitive(name, "emitter") ||
               Core::Objects::ContainsInsensitive(name, "particle") ||
               Core::Objects::ContainsInsensitive(name, "windwall")) {
        resolved = ObjectType::EffectEmitter;
    }

    TypeCache::Store(object, resolved);
    return resolved;
}

inline ObjectSnapshot ReadObject(uintptr_t object, ObjectType type = ObjectType::Unknown) {
    if (type == ObjectType::Unknown) {
        type = InferType(object);
    }
    return Core::Objects::ReadSnapshot(object, type);
}

inline ObjectSnapshot Player() {
    return ReadObject(PlayerAddress(), ObjectType::AIHeroClient);
}

} // namespace Core::ObjectManager
