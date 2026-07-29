#pragma once

#include "../../Core/CoreBuffs.h"
#include "../../Core/CoreAiManager.h"
#include "../../Core/CoreItem.h"
#include "../../Core/CoreSpellBook.h"
#include "../../Core/CoreSpellDataInst.h"
#include "../../Core/CoreObjectManager.h"
#include "../../Core/CoreObjects.h"
#include "../../Core/CoreRuneManager.h"
#include "../../Core/CoreAIHeroClient.h"
#include "../../Core/CoreView.h"
#include "../../Core/Vector.h"
#include "../Enumerations/JungleType.h"
#include "../Enumerations/MinionTypes.h"
#include "../Enumerations/SpellSlot.h"
#include "../Enumerations/DamageType.h"
#include "../Enums/BuffType.h"
#include "../Enums/GameObjectOrder.h"
#include "../Data/GameData.h"
#include "../Data/RuneData.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>
#include <fstream>

namespace SDK {

using Vector2 = Vec2;
using Vector3 = Vec3;

class AIBaseClient;
class GameObject;
class InventorySlot;

namespace DamageMod {
    float DamageReductionMod(const AIBaseClient& source,
                             const AIBaseClient& target,
                             float amount,
                             DamageType damageType);
}

namespace CoreSpellBook = ::CoreSpellBook;

enum class GameObjectTeam : std::int32_t {
    Unknown = 0,
    Order = 100,
    Chaos = 200,
    Neutral = 300,
};

class SpellDataInstClient {
public:
    SpellDataInstClient() = default;
    SpellDataInstClient(uintptr_t owner, SpellSlot slot)
        : owner_(owner), slot_(slot) {}

    bool IsValid() const {
        return ::CoreSpellDataInst::IsValid(owner_, SlotIndex());
    }

    SpellSlot Slot() const {
        return slot_;
    }

    uintptr_t OwnerAddress() const {
        return owner_;
    }

    uintptr_t SpellbookAddress() const {
        return ::CoreSpellDataInst::Spellbook(owner_);
    }

    uintptr_t Address() const {
        return ::CoreSpellDataInst::Resolve(owner_, SlotIndex()).slot;
    }

    uintptr_t SpellInfoPointer() const {
        return ::CoreSpellDataInst::SpellInfo(Ref());
    }

    uintptr_t SDataPointer() const {
        return ::CoreSpellDataInst::SpellData(Ref());
    }

    uintptr_t DataResourcePointer() const {
        return ::CoreSpellDataInst::SpellDataResource(Ref());
    }

    std::string Name() const {
        return ::CoreSpellDataInst::Name(Ref());
    }

    CoreSpellBook::State State(float gameTime) const {
        return ::CoreSpellDataInst::SimpleState(Ref(), gameTime);
    }

    std::uint32_t RawState() const {
        return ::CoreSpellDataInst::RawState(Ref());
    }

    float RemainingCooldown(float gameTime = 0.0f) const {
        return ::CoreSpellDataInst::RemainingCooldown(Ref(), gameTime);
    }

    float Cooldown() const {
        return ::CoreSpellDataInst::Cooldown(Ref());
    }

    float CooldownExpires() const {
        return ::CoreSpellDataInst::CooldownExpires(Ref());
    }

    float ManaCost() const {
        return ::CoreSpellDataInst::ManaCost(Ref());
    }

    float CastRange() const {
        return ::CoreSpellDataInst::CastRange(Ref());
    }

    float LineWidth() const {
        return ::CoreSpellDataInst::LineWidth(Ref());
    }

    float CastRadius() const {
        return ::CoreSpellDataInst::CastRadius(Ref());
    }

    float MissileSpeed() const {
        return ::CoreSpellDataInst::MissileSpeed(Ref());
    }

    std::uint8_t CastType() const {
        return ::CoreSpellDataInst::CastType(Ref());
    }

    std::string ScriptName() const {
        return ::CoreSpellDataInst::ScriptName(Ref());
    }

    std::string IconName() const {
        return ::CoreSpellDataInst::IconName(Ref());
    }

    int Ammo() const {
        return ::CoreSpellDataInst::Ammo(Ref());
    }

    int MaxAmmo() const {
        return ::CoreSpellDataInst::MaxAmmo(Ref());
    }

    float AmmoRechargeTime() const {
        return ::CoreSpellDataInst::AmmoRechargeTime(Ref());
    }

    float NextAmmoRechargeTime() const {
        return ::CoreSpellDataInst::NextAmmoRechargeTime(Ref());
    }

    bool Learned() const {
        return ::CoreSpellDataInst::Learned(Ref());
    }

    int Level() const {
        return ::CoreSpellDataInst::Level(Ref());
    }

private:
    std::int32_t SlotIndex() const {
        return static_cast<std::int32_t>(slot_);
    }

    ::CoreSpellDataInst::SpellSlotRef Ref() const {
        return ::CoreSpellDataInst::Resolve(owner_, SlotIndex());
    }

    uintptr_t owner_ = 0;
    SpellSlot slot_ = SpellSlot::Unknown;
};

class SpellBookClient {
public:
    SpellBookClient() = default;
    explicit SpellBookClient(uintptr_t owner) : owner_(owner) {}

    bool IsValid() const {
        return Globals::IsValidPtr(owner_) &&
               Globals::IsValidPtr(::CoreSpellBook::Spellbook(owner_));
    }

    uintptr_t Address() const {
        return ::CoreSpellBook::Spellbook(owner_);
    }

    uintptr_t OwnerAddress() const {
        return owner_;
    }

    AIBaseClient Owner() const;

    std::uint32_t CasterNetworkId() const {
        return ::CoreSpellBook::CasterNetworkId(Address());
    }

    SpellDataInstClient GetSpell(SpellSlot slot) const {
        return SpellDataInstClient(owner_, slot);
    }

    std::vector<SpellDataInstClient> Spells() const {
        std::vector<SpellDataInstClient> result;
        ::CoreSpellDataInst::SpellSlotRef refs[::CoreSpellDataInst::kMaxSpellSlots] = {};
        const int count = ::CoreSpellBook::GetSpells(
            owner_,
            refs,
            ::CoreSpellDataInst::kMaxSpellSlots);
        result.reserve(count > 0 ? static_cast<std::size_t>(count) : 0);
        for (int i = 0; i < count; ++i) {
            result.emplace_back(owner_, static_cast<SpellSlot>(refs[i].slotId));
        }
        return result;
    }

    CoreSpellBook::State CanUseSpell(SpellSlot slot) const {
        return ::CoreSpellBook::CanUseSpell(
            owner_,
            static_cast<std::int32_t>(slot),
            CoreRuntime::GetContext().gameTime);
    }

    uintptr_t ActiveSpell() const {
        return ::CoreSpellBook::ActiveSpell(owner_).address;
    }

    bool IsCastingSpell() const {
        return ::CoreSpellBook::IsCastingSpell(owner_, CoreRuntime::GetContext().gameTime);
    }

    bool IsChanneling() const {
        return ::CoreSpellBook::IsChanneling(owner_, CoreRuntime::GetContext().gameTime);
    }

    bool IsCharging() const {
        return ::CoreSpellBook::IsCharging(owner_);
    }

    bool IsStopped() const {
        return !IsCastingSpell();
    }

    bool IsAutoAttack() const {
        return ::CoreSpellBook::CastSlot(::CoreSpellBook::ActiveSpell(owner_)) == 64;
    }

    bool IsWindingUp() const {
        return IsCastingSpell() && !IsChanneling();
    }

    bool SpellWasCast() const {
        const auto active = ::CoreSpellBook::ActiveSpell(owner_);
        const float endTime = ::CoreSpellBook::CastEndTime(active);
        const float gameTime = CoreRuntime::GetContext().gameTime;
        return active.IsValid() && endTime > 0.0f && gameTime >= endTime;
    }

    float CastEndTime() const {
        return ::CoreSpellBook::CastEndTime(::CoreSpellBook::ActiveSpell(owner_));
    }

    float CastTime() const {
        const auto active = ::CoreSpellBook::ActiveSpell(owner_);
        const float start = ::CoreSpellBook::CastStartTime(active);
        const float end = ::CoreSpellBook::CastEndTime(active);
        return end > start ? (end - start) : 0.0f;
    }

    bool CastSpell(SpellSlot slot, bool triggerEvent = true) const {
        (void)triggerEvent;
        return ::CoreSpellBook::CastSpell(owner_, static_cast<std::int32_t>(slot));
    }

    bool CastSpell(SpellSlot slot,
                   const Vector3& position,
                   bool triggerEvent = true) const {
        (void)triggerEvent;
        return ::CoreSpellBook::CastSpell(
            owner_,
            static_cast<std::int32_t>(slot),
            position);
    }

    bool CastSpell(SpellSlot slot,
                   const Vector3& startPosition,
                   const Vector3& endPosition,
                   bool triggerEvent = true) const {
        (void)triggerEvent;
        return ::CoreSpellBook::CastSpell(
            owner_,
            static_cast<std::int32_t>(slot),
            startPosition,
            endPosition);
    }

    template <typename T>
    bool CastSpell(SpellSlot slot,
                   const T& target,
                   bool triggerEvent = true) const {
        (void)triggerEvent;
        uintptr_t addr = 0;
        if constexpr (std::is_integral_v<std::decay_t<T>> || std::is_pointer_v<std::decay_t<T>>) {
            addr = static_cast<uintptr_t>(target);
        } else {
            addr = target.Address();
        }
        return ::CoreSpellBook::CastSpellOnTarget(
            owner_,
            static_cast<std::int32_t>(slot),
            addr);
    }

    bool CastSpell(SpellSlot slot,
                   const GameObject& target,
                   bool triggerEvent = true) const;

    bool UpdateChargedSpell(SpellSlot slot,
                            const Vector3& position,
                            bool releaseCast,
                            bool triggerEvent = true) const {
        (void)triggerEvent;
        return ::CoreSpellBook::UpdateChargedSpell(
            owner_,
            static_cast<std::int32_t>(slot),
            position,
            releaseCast);
    }

private:
    uintptr_t owner_ = 0;
};

namespace ObjectDetail {
    inline bool EqualsAny(const std::string& text, std::initializer_list<const char*> values) {
        if (text.empty()) return false;
        for (const char* value : values) {
            if (value && _stricmp(text.c_str(), value) == 0) {
                return true;
            }
        }
        return false;
    }

    inline bool ContainsInsensitive(std::string_view text, const char* token) {
        if (!token || !token[0] || text.empty()) {
            return false;
        }
        const std::size_t tokenLen = std::strlen(token);
        auto it = std::search(text.begin(), text.end(), token, token + tokenLen,
            [](char c1, char c2) {
                return std::tolower(static_cast<unsigned char>(c1)) ==
                       std::tolower(static_cast<unsigned char>(c2));
            });
        return it != text.end();
    }

    inline bool ContainsAny(std::string_view text, std::initializer_list<const char*> values) {
        if (text.empty()) {
            return false;
        }
        for (const char* value : values) {
            if (value && ContainsInsensitive(text, value)) {
                return true;
            }
        }
        return false;
    }

    inline GameObjectTeam MapTeam(std::uint32_t team) {
        switch (team) {
        case 100: return GameObjectTeam::Order;
        case 200: return GameObjectTeam::Chaos;
        case 300: return GameObjectTeam::Neutral;
        default: return GameObjectTeam::Unknown;
        }
    }

    inline std::string ToLower(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return text;
    }

    inline bool ReadVisibleFlag(uintptr_t object) {
        if (!object) {
            return false;
        }

        __try {
            return *reinterpret_cast<const std::uint8_t*>(object + Offset::All::Visible) != 0;
        } __except (1) {
            return false;
        }
    }

    inline bool ReadEngineInvulnerable(uintptr_t object) {
        if (!Globals::IsValidPtr(object)) {
            return false;
        }

        constexpr int kBuffTypeInvulnerability = 18;
        constexpr int kBuffTypeUnKillable = 38;
        static constexpr const char* kKnownInvulnerableBuffs[] = {
            "KayleR",
            "TaricR",
            "UndyingRage",
            "ChronoRevive",
            "ZhonyasRingShield",
            "zhonyasringshield",
            "bardrstasis",
            "BardRStasis",
            "LissandraRSelf",
            "VladimirSanguinePool",
            "FizzEIcon"
        };

        uintptr_t buffs[256] = {};
        const int count = CoreBuffs::Enumerate(object, buffs, 256);
        const float gameTime = CoreBuffs::ResolveGameTime();
        char nameBuffer[96] = {};
        for (int i = 0; i < count; ++i) {
            const CoreBuffs::BuffRef buff{ buffs[i] };
            if (!buff.IsActive(gameTime)) {
                continue;
            }

            const int type = buff.GetType();
            if (type == kBuffTypeInvulnerability || type == kBuffTypeUnKillable) {
                return true;
            }

            if (!buff.ReadName(nameBuffer, static_cast<int>(sizeof(nameBuffer)))) {
                continue;
            }

            for (const char* name : kKnownInvulnerableBuffs) {
                if (CoreBuffs::NameMatchesQuery(nameBuffer, name)) {
                    return true;
                }
            }
        }
        return false;
    }
} // namespace ObjectDetail

// ---------------------------------------------------------------------------
// Obsolete StaticStringCache removed — replaced by FastObjectCache.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// WaypointCache — per-frame cache for AiManager-derived data only.
// These require a native game function call so they genuinely benefit from
// caching within one frame. Direct field reads (health, pos, etc.) do NOT
// use this cache — they are raw inline dereferences.
// ---------------------------------------------------------------------------
namespace WaypointCache {
    constexpr int kBuckets = 512;
    struct Entry {
        uintptr_t address   = 0;
        int frame           = -1;
        bool hasWaypoints   = false;
        bool hasServerPos   = false;
        std::vector<Vector3> waypoints;
        Vector3 serverPosition = {};
    };
    inline thread_local Entry entries[kBuckets] = {};
    inline int Bucket(uintptr_t a) {
        return static_cast<int>((a >> 4) & (kBuckets - 1));
    }
    inline Entry* GetEntry(uintptr_t a) {
        int b = Bucket(a);
        Entry& e = entries[b];
        const int frame = ::CoreAiManager::FrameCacheKey();
        if (e.address != a || e.frame != frame) {
            if (e.waypoints.capacity() < static_cast<std::size_t>(::CoreAiManager::kMaxWaypoints)) {
                e.waypoints.reserve(::CoreAiManager::kMaxWaypoints);
            }
            e.address       = a;
            e.frame         = frame;
            e.hasWaypoints  = false;
            e.hasServerPos  = false;
        }
        return &e;
    }
    inline const std::vector<Vector3>& GetWaypointsFull(uintptr_t a) {
        Entry* e = GetEntry(a);
        if (!e->hasWaypoints) {
            e->waypoints.clear();
            int count = 0;
            const Vec3* points = ::CoreAiManager::CachedPathData(a, count);
            count = std::clamp(count, 0, ::CoreAiManager::kMaxWaypoints);
            for (int i = 0; i < count; ++i) {
                e->waypoints.push_back(points[i]);
            }
            e->hasWaypoints = true;
        }
        return e->waypoints;
    }

    inline int CopyWaypoints(uintptr_t a, Vector3* out, int maxPoints) {
        if (!Globals::IsValidPtr(a) || !out || maxPoints <= 0) {
            return 0;
        }

        const auto& cached = GetWaypointsFull(a);
        const int count = std::min<int>(
            std::clamp(maxPoints, 1, ::CoreAiManager::kMaxWaypoints),
            static_cast<int>(cached.size()));
        for (int i = 0; i < count; ++i) {
            out[i] = cached[static_cast<std::size_t>(i)];
        }
        return count;
    }

    inline std::vector<Vector3> GetWaypoints(uintptr_t a, int maxPoints) {
        std::vector<Vector3> result;
        if (!Globals::IsValidPtr(a) || maxPoints <= 0) {
            return result;
        }

        const auto& cached = GetWaypointsFull(a);
        const int count = std::min<int>(
            std::clamp(maxPoints, 1, ::CoreAiManager::kMaxWaypoints),
            static_cast<int>(cached.size()));
        result.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            result.push_back(cached[static_cast<std::size_t>(i)]);
        }
        return result;
    }

    inline Vector3 GetServerPosition(uintptr_t a) {
        Entry* e = GetEntry(a);
        if (!e->hasServerPos) {
            e->serverPosition = ::CoreAiManager::GetServerPosition(a);
            e->hasServerPos   = true;
        }
        return e->serverPosition;
    }
} // namespace WaypointCache

namespace ObjectCache = WaypointCache;

// Keep ObjectCache as a thin alias so any existing call-sites that reference
// ObjectCache::GetWaypoints / ObjectCache::GetServerPosition still compile.
// ---------------------------------------------------------------------------
// StaticStringCache — populated once on OnCreateObject / GameObjects scan.
// Stores fields that NEVER change during a unit's lifetime (Name, CharacterName, Team, MinionClass).
// ---------------------------------------------------------------------------
namespace StaticStringCache {
    constexpr int kMaxIndex = 10000;
    struct Entry {
        bool valid = false;
        std::string characterName;
        std::string name;
        uint32_t team = 0;
        int minionClass = 0;
    };
    inline Entry entries[kMaxIndex] = {};

    inline Entry* GetMutable(uint32_t index) {
        if (index >= static_cast<uint32_t>(kMaxIndex)) return nullptr;
        return &entries[index];
    }

    inline const Entry* Get(uint32_t index) {
        if (index >= static_cast<uint32_t>(kMaxIndex)) return nullptr;
        return entries[index].valid ? &entries[index] : nullptr;
    }

    inline void Populate(uintptr_t address, uint32_t index, ::Core::Objects::ObjectType type) {
        if (index >= static_cast<uint32_t>(kMaxIndex) || !address) return;
        Entry& e = entries[index];
        e.valid = true;

        const bool isMainType = (
            type == ::Core::Objects::ObjectType::AIHeroClient ||
            type == ::Core::Objects::ObjectType::AIMinionClient ||
            // REMOVED: Turret/Inhibitor/Nexus query disabled by user request
            // type == ::Core::Objects::ObjectType::AITurretClient ||
            // type == ::Core::Objects::ObjectType::BarracksDampenerClient ||
            // type == ::Core::Objects::ObjectType::HQClient ||
            type == ::Core::Objects::ObjectType::MissileClient);

        if (e.characterName.empty() && isMainType) {
            char charBuf[96] = {};
            if (::Core::Objects::ReadCharacterName(address, charBuf, static_cast<int>(sizeof(charBuf))))
                e.characterName = charBuf;
        }

        if (e.name.empty() && isMainType) {
            char nameBuf[96] = {};
            if (::Core::Objects::ReadName(address, nameBuf, static_cast<int>(sizeof(nameBuf))))
                e.name = nameBuf;
        }

        if (e.team == 0 && isMainType) {
            e.team = ::Core::Objects::ReadTeamValue(address);
        }

        if (e.minionClass == 0 && isMainType && (
            type == ::Core::Objects::ObjectType::AIMinionClient ||
            type == ::Core::Objects::ObjectType::NeutralMinionCampClient)) {
            e.minionClass = static_cast<int>(::Core::Objects::ReadMinionClass(address));
        }
    }

    inline void Clear(uint32_t index) {
        if (index >= static_cast<uint32_t>(kMaxIndex)) return;
        entries[index] = {};
    }
}



class GameObject {
public:
    GameObject() = default;
    explicit GameObject(uintptr_t address,
                        ::Core::Objects::ObjectType type = ::Core::Objects::ObjectType::GameObject)
        : handle_(::Core::ObjectManager::MakeHandle(address, type)) {}
    explicit GameObject(::Core::Objects::ObjectHandle handle)
        : handle_(handle) {}

    uintptr_t Address() const {
        if (handle_.HasAddress()) {
            ::Core::Objects::ObjectHandle resolved = handle_;
            if (::Core::ObjectManager::Resolve(resolved)) {
                handle_ = resolved;
                return handle_.address;
            }
        }

        if (::Core::ObjectManager::Resolve(handle_)) {
            return handle_.address;
        }
        return 0;
    }

    bool IsValid() const {
        return Address() != 0;
    }

    int NetworkId() const {
        (void)Address();
        return static_cast<int>(handle_.networkId);
    }

    int Index() const {
        (void)Address();
        return static_cast<int>(handle_.index);
    }

    ::Core::Objects::ObjectType Type() const {
        if (handle_.type == ::Core::Objects::ObjectType::Unknown && IsValid()) {
            handle_.type = ::Core::ObjectManager::InferType(handle_.address);
        }
        return handle_.type;
    }

    ::Core::Objects::ObjectHandle Handle() const {
        (void)Address();
        return handle_;
    }

    GameObjectTeam Team() const {
        const uintptr_t a = Address();
        if (!a) return GameObjectTeam::Unknown;
        return ObjectDetail::MapTeam(::Core::Objects::ReadTeamValue(a));
    }

    // Player team is constant for the entire game. Cache it on first use so
    // these predicates don't re-resolve the player object on every call.
    // Before this cache, IsEnemy()/IsAlly() called ::Core::ObjectManager::Player()
    // which builds a full 92-read snapshot on every invocation — and
    // InAutoAttackRange() calls IsEnemy() dozens of times per orbwalker tick,
    // which was the dominant cause of the FPS drop when holding the orbwalker key.
    static std::uint32_t CachedPlayerTeam() {
        static std::uint32_t cached = 0;
        static bool resolved = false;
        if (!resolved) {
            const uintptr_t player = CoreRuntime::g_ctx.localPlayer;
            if (Globals::IsValidPtr(player)) {
                cached = ::Core::Objects::ReadTeamValue(player);
            }
            resolved = (cached != 0);
        }
        return cached;
    }

    // Called once at the start of Rebuild() to force the player team static
    // to resolve before any AddObject()/IsEnemy() calls are made.
    // Also caches the local player champion name for the whole session.
    static void WarmPlayerTeamCache() {
        static std::uint32_t cached = 0;
        static bool resolved = false;
        if (resolved) return;

        auto tryResolve = [&](uintptr_t playerAddr) -> bool {
            if (!Globals::IsValidPtr(playerAddr)) return false;
            cached = ::Core::Objects::ReadTeamValue(playerAddr);
            if (cached == 0) return false;
            // Cache champion name for the whole session.
            char nameBuf[96] = {};
            if (::Core::Objects::ReadCharacterName(playerAddr, nameBuf, static_cast<int>(sizeof(nameBuf))))
                s_cachedChampionName = nameBuf;
            resolved = true;
            return true;
        };

        if (!tryResolve(CoreRuntime::g_ctx.localPlayer))
            tryResolve(::Core::ObjectManager::PlayerAddress());
        // After this call, CachedPlayerTeam()'s own static will also populate
        // on its next invocation since it reads the same g_ctx.localPlayer.
    }

    // Returns the local player champion name cached at session start.
    // Falls back to one direct local-player read when cache is empty.
    static const std::string& GetCachedChampionName() {
        if (s_cachedChampionName.empty()) {
            const uintptr_t player = CoreRuntime::g_ctx.localPlayer;
            char nameBuf[96] = {};
            if (Globals::IsValidPtr(player) &&
                ::Core::Objects::ReadCharacterName(player, nameBuf, static_cast<int>(sizeof(nameBuf))) &&
                nameBuf[0]) {
                s_cachedChampionName = nameBuf;
            }
        }
        return s_cachedChampionName;
    }

    bool IsEnemy() const {
        const GameObjectTeam myTeam = Team();
        if (myTeam == GameObjectTeam::Unknown) return false;
        const uint32_t playerTeam = CachedPlayerTeam();
        return playerTeam != 0 && playerTeam != static_cast<uint32_t>(myTeam);
    }

    bool IsAlly() const {
        const GameObjectTeam myTeam = Team();
        if (myTeam == GameObjectTeam::Unknown) return false;
        const uint32_t playerTeam = CachedPlayerTeam();
        return playerTeam != 0 && playerTeam == static_cast<uint32_t>(myTeam);
    }

    bool IsMe() const {
        const auto player = ::Core::ObjectManager::PlayerHandle();
        return player.HasIdentity() && NetworkId() == static_cast<int>(player.networkId);
    }

    // -----------------------------------------------------------------
    // Direct inline reads — no snapshot, no SEH, no generation checks.
    // These are identical to Chimera's RVA_CAST_THIS pattern:
    //   each call compiles to a single `mov` CPU instruction.
    // -----------------------------------------------------------------
    bool IsDead() const {
        const uintptr_t a = Address();
        if (!a) return false;
        return ::Core::Objects::IsDead(a);
    }

    bool IsVisible() const {
        const uintptr_t a = Address();
        if (!a) return false;
        if (IsMe()) return true;
        return ObjectDetail::ReadVisibleFlag(a);
    }

    // EnsoulSharp parity: GameObject.IsVisibleOnScreen.
    // The native client stores a Riot-scrambled bool for this on the object,
    // but that field is NOT reversed on the current x64 build — EnsoulSharp's
    // native core (EnsoulSharp.Core.dll) is a 32-bit, encrypted module, so its
    // field offset neither disassembles statically nor maps to the x64 layout.
    // We reproduce the exact same result with the existing world-to-screen
    // projection: a unit is "visible on screen" when its world position projects
    // inside the current renderer viewport. This is a DRAW-ONLY gate (used by HP
    // bar / overlay drawing); do NOT use it for gameplay/targeting decisions —
    // it does not account for fog of war (use IsVisible() for that).
    bool IsVisibleOnScreen() const {
        const uintptr_t a = Address();
        if (!a) return false;
        Vec2 screen{};
        return ::CoreView::WorldToScreen(Position(), screen) &&
               ::CoreView::OnScreen(screen);
    }

    bool IsTargetable() const {
        const uintptr_t a = Address();
        if (!a) return false;
        return *reinterpret_cast<const uint8_t*>(a + Offset::AttackableUnit::IsTargetable) != 0;
    }

    bool IsInvulnerable() const {
        const uintptr_t a = Address();
        if (!a) return false;
        return ObjectDetail::ReadEngineInvulnerable(a);
    }

    float BoundingRadius() const {
        const uintptr_t a = Address();
        if (!a) return 0.0f;
        const float radius = ::Core::Objects::ReadBoundingRadius(a);
        return radius > 0.0f ? radius : 65.0f;
    }

    Vector3 Position() const {
        const uintptr_t a = Address();
        if (!a) return {};
        return *reinterpret_cast<const Vector3*>(a + Offset::All::Position);
    }

    Vector3 Direction() const {
        const uintptr_t a = Address();
        if (!a) return {};
        return {}; // direction requires vfunc chain; use AIBaseClient::Direction() instead
    }

    const std::string& Name() const {
        const uint32_t idx = static_cast<uint32_t>(handle_.index & 0xFFFFu);
        if (const auto* sc = StaticStringCache::Get(idx)) {
            if (!sc->name.empty()) return sc->name;
            if (!sc->characterName.empty()) return sc->characterName;
        }
        const uintptr_t a = Address();
        if (Globals::IsValidPtr(a)) {
            char nameBuf[96] = {};
            if (::Core::Objects::ReadName(a, nameBuf, sizeof(nameBuf)) && nameBuf[0]) {
                thread_local std::string tls_name;
                tls_name = nameBuf;
                return tls_name;
            }
            char charBuf[96] = {};
            if (::Core::Objects::ReadCharacterName(a, charBuf, sizeof(charBuf)) && charBuf[0]) {
                thread_local std::string tls_nameFallback;
                tls_nameFallback = charBuf;
                return tls_nameFallback;
            }
        }
        static const std::string kEmpty;
        return kEmpty;
    }

    const std::string& CharacterName() const {
        const uint32_t idx = static_cast<uint32_t>(handle_.index & 0xFFFFu);
        if (const auto* sc = StaticStringCache::Get(idx)) {
            if (!sc->characterName.empty()) return sc->characterName;
            if (!sc->name.empty()) return sc->name;
        }
        const uintptr_t a = Address();
        if (Globals::IsValidPtr(a)) {
            char charBuf[96] = {};
            if (::Core::Objects::ReadCharacterName(a, charBuf, sizeof(charBuf)) && charBuf[0]) {
                thread_local std::string tls_charName;
                tls_charName = charBuf;
                return tls_charName;
            }
            char nameBuf[96] = {};
            if (::Core::Objects::ReadName(a, nameBuf, sizeof(nameBuf)) && nameBuf[0]) {
                thread_local std::string tls_charNameFallback;
                tls_charNameFallback = nameBuf;
                return tls_charNameFallback;
            }
        }
        static const std::string kEmpty;
        return kEmpty;
    }

    float GetHpBarHeight() const {
        const std::string& charName = CharacterName();
        if (!charName.empty()) {
            const auto* info = SDK::Data::GameData::GetUnitInfoByName(charName);
            if (info && info->healthBarHeight > 0.0f) {
                return info->healthBarHeight;
            }
        }
        return 100.0f;
    }

    // Gap #2 fix: EnsoulSharp Compare is strict — only matches when both
    // objects are valid AND have the same NetworkId. No fallback to index
    // or address comparison (those can alias after NetworkId was split from
    // Index on build 13337).
    bool Compare(const GameObject& other) const {
        if (!IsValid() || !other.IsValid()) {
            return false;
        }
        const auto lhs = Handle();
        const auto rhs = other.Handle();
        return lhs.networkId != 0 && lhs.networkId != 0xFFFFFFFFu &&
               lhs.networkId == rhs.networkId;
    }

    float Distance(const GameObject& other) const {
        return Position().Distance(other.Position());
    }

    float DistanceSquared(const GameObject& other) const {
        return Position().DistanceSquared(other.Position());
    }

    float Distance(const Vector3& position) const {
        return Position().Distance(position);
    }

    float Distance(const Vector2& position) const {
        return Position().To2D().Distance(position);
    }

    bool IsHero() const {
        return Type() == ::Core::Objects::ObjectType::AIHeroClient;
    }

    bool IsMinion() const {
        return Type() == ::Core::Objects::ObjectType::AIMinionClient;
    }

    bool IsTurret() const {
        return Type() == ::Core::Objects::ObjectType::AITurretClient;
    }

    bool IsMissile() const {
        return Type() == ::Core::Objects::ObjectType::MissileClient;
    }

    // Sion remains a targetable AIHeroClient during Glory in Death even
    // though the engine reports the unit as dead. Karthus' Death Defied is
    // intentionally not considered a zombie because that state cannot be
    // attacked as a normal target.
    bool IsZombie() const {
        if (!IsHero() ||
            ObjectDetail::ToLower(CharacterName()) != "sion") {
            return false;
        }
        return CoreBuffs::HasBuff(Address(), "sionpassivezombie");
    }

    // Champion clones are AIMinionClient objects whose CharacterName matches
    // the champion they imitate. Requiring the minion runtime type is what
    // distinguishes the clone from the real AIHeroClient.
    bool IsClone() const {
        if (!IsMinion()) {
            return false;
        }

        const std::string name = ObjectDetail::ToLower(CharacterName());
        return ObjectDetail::EqualsAny(name, {
            "leblanc", "monkeyking", "neeko", "shaco",
        });
    }

    // Pets are summons, not champion clones. Some game builds tag summons
    // through MinionClass::Pet; retain the curated names as a fallback for
    // objects whose replicated class byte has not arrived at creation time.
    bool IsPet() const {
        if (!IsMinion() || IsClone()) {
            return false;
        }
        const uintptr_t a = Address();
        return a && ::Core::Objects::ReadMinionClass(a) == ::Core::Objects::MinionClass::Pet;
    }



protected:
    mutable ::Core::Objects::ObjectHandle handle_ = {};

private:
    inline static std::string s_cachedChampionName;
};

inline bool SpellBookClient::CastSpell(SpellSlot slot,
                                       const GameObject& target,
                                       bool triggerEvent) const {
    return CastSpell(slot, target.Address(), triggerEvent);
}

class AttackableUnit : public GameObject {
public:
    AttackableUnit() = default;
    explicit AttackableUnit(uintptr_t address,
                            ::Core::Objects::ObjectType type = ::Core::Objects::ObjectType::GameObject)
        : GameObject(address, type) {}
    explicit AttackableUnit(::Core::Objects::ObjectHandle handle)
        : GameObject(handle) {}

    float Health() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AttackableUnit::HP) : 0.0f;
    }
    float MaxHealth() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AttackableUnit::MaxHP) : 0.0f;
    }
    float HealthPercent() const {
        const float maxHealth = MaxHealth();
        return maxHealth > 0.0f ? (Health() * 100.0f / maxHealth) : 0.0f;
    }
    float AllShield() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AttackableUnit::AllShield) : 0.0f;
    }
    float PhysicalShield() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AttackableUnit::PhysicalShield) : 0.0f;
    }
    float MagicalShield() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AttackableUnit::MagicalShield) : 0.0f;
    }
    float HealthRegenRate() const {
        return IsValid() ? ::CoreAIHeroClient::HealthRegenRate(Address()) : 0.0f;
    }
};

class AIBaseClient : public AttackableUnit {
public:
    AIBaseClient() = default;
    explicit AIBaseClient(uintptr_t address,
                          ::Core::Objects::ObjectType type = ::Core::Objects::ObjectType::GameObject)
        : AttackableUnit(address, type) {}
    explicit AIBaseClient(::Core::Objects::ObjectHandle handle)
        : AttackableUnit(handle) {}

    float Mana() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AIHeroClient::MP) : 0.0f;
    }
    float MaxMana() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AIHeroClient::MaxMP) : 0.0f;
    }
    float ManaPercent() const {
        const float maxMana = MaxMana();
        return maxMana > 0.0f ? (Mana() * 100.0f / maxMana) : 0.0f;
    }
    float GetSpellDamage(const AIBaseClient& target, SpellSlot slot) const;
    float MoveSpeed() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AIHeroClient::MoveSpeed) : 0.0f;
    }
    float AttackRange() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AIHeroClient::AttackRange) : 0.0f;
    }
    float BaseAttackDamage() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AIHeroClient::BaseAttackDamage) : 0.0f;
    }
    float TotalAttackDamage() const {
        const uintptr_t a = Address();
        if (!a) return 0.0f;
        return *reinterpret_cast<const float*>(a + Offset::AIHeroClient::BaseAttackDamage)
             + *reinterpret_cast<const float*>(a + Offset::AIHeroClient::FlatPhysicalDmgMod);
    }
    float TotalMagicalDamage() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AIHeroClient::BaseAbilityDamage) : 0.0f;
    }
    float Armor() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AIHeroClient::Armor) : 0.0f;
    }
    float SpellBlock() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AIHeroClient::SpellBlock) : 0.0f;
    }
    float AttackSpeedMod() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const float*>(a + Offset::AIHeroClient::AttackSpeedMod) : 0.0f;
    }
    int Level() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const int*>(a + Offset::AIHeroClient::LevelRef) : 0;
    }

    // ── Hero-specific stat properties (read from AIHeroClient offsets) ──
    // These are safe to call on any AIBaseClient ─ non-hero objects will
    // return 0 since the offsets are only populated for AIHeroClient.
    float AD() const { return TotalAttackDamage(); }
    float AP() const { return TotalMagicalDamage(); }
    float BonusAttackDamage() const {
        return TotalAttackDamage() - BaseAttackDamage();
    }
    float BonusHealth() const;
    float Crit() const {
        return ::CoreAIHeroClient::Crit(Address());
    }
    float BonusArmor() const {
        return ::CoreAIHeroClient::BonusArmor(Address());
    }
    float BonusSpellBlock() const {
        return ::CoreAIHeroClient::BonusSpellBlock(Address());
    }
    float Lethality() const {
        return ::CoreAIHeroClient::PhysicalLethality(Address());
    }
    float FlatArmorPenetrationMod() const {
        return ::CoreAIHeroClient::FlatArmorPen(Address());
    }
    float PercentArmorPenetrationMod() const {
        return ::CoreAIHeroClient::PercentArmorPen(Address());
    }
    float PercentBonusArmorPenetrationMod() const {
        return ::CoreAIHeroClient::PercentBonusArmorPen(Address());
    }
    float FlatMagicPenetrationMod() const {
        return ::CoreAIHeroClient::FlatMagicPen(Address());
    }
    float MagicLethality() const {
        return ::CoreAIHeroClient::MagicLethality(Address());
    }
    float PercentMagicPenetrationMod() const {
        return ::CoreAIHeroClient::PercentMagicPen(Address());
    }
    float PercentBonusMagicPenetrationMod() const {
        return ::CoreAIHeroClient::PercentBonusMagicPen(Address());
    }

    // ── Damage calculation (matching DLL's Damage.CalculatePhysicalDamage) ──
    float CalculatePhysicalDamage(const AIBaseClient& target, float amount) const {
        if (amount <= 0.0f) return 0.0f;
        if (!IsValid() || !target.IsValid()) return 0.0f;

        float percentArmorPen = PercentArmorPenetrationMod();
        float flatArmorPen = FlatArmorPenetrationMod() + Lethality();
        float percentBonusArmorPen = PercentBonusArmorPenetrationMod();

        if (Type() == ::Core::Objects::ObjectType::AIMinionClient) {
            flatArmorPen = 0.0f;
            percentArmorPen = 1.0f;
            percentBonusArmorPen = 1.0f;
        } else if (Type() == ::Core::Objects::ObjectType::AIHeroClient) {
            flatArmorPen = Lethality() * (0.6f + 0.4f * static_cast<float>(Level()) / 18.0f);
            if (std::isnan(flatArmorPen)) flatArmorPen = 0.0f;
        } else if (Type() == ::Core::Objects::ObjectType::AITurretClient) {
            flatArmorPen = 0.0f;
            percentArmorPen = 1.0f;
            percentBonusArmorPen = 1.0f;
        }

        float armor = target.Armor();
        float bonusArmor = target.BonusArmor();
        float multiplier;
        if (armor < 0.0f) {
            multiplier = 2.0f - 100.0f / (100.0f - armor);
        } else {
            float effectiveArmor = armor * percentArmorPen
                - bonusArmor * (1.0f - percentBonusArmorPen)
                - flatArmorPen;
            multiplier = (effectiveArmor < 0.0f) ? 1.0f : (100.0f / (100.0f + effectiveArmor));
        }

        float result = multiplier * amount;
        result = SDK::DamageMod::DamageReductionMod(*this, target, result, DamageType::Physical);
        return std::max(std::floor(result), 0.0f);
    }

    // ── Damage calculation (matching DLL's Damage.CalculateMagicDamage) ──
    float CalculateMagicDamage(const AIBaseClient& target, float amount) const {
        if (amount <= 0.0f) return 0.0f;
        if (!IsValid() || !target.IsValid()) return 0.0f;

        float spellBlock = target.SpellBlock();
        float bonusSpellBlock = target.BonusSpellBlock();
        float flatMagicPen = FlatMagicPenetrationMod() + MagicLethality();

        float effectiveSpellBlock = spellBlock * PercentMagicPenetrationMod()
            - bonusSpellBlock * (1.0f - PercentBonusMagicPenetrationMod())
            - flatMagicPen;

        float multiplier;
        if (spellBlock < 0.0f) {
            multiplier = 2.0f - 100.0f / (100.0f - spellBlock);
        } else {
            multiplier = (effectiveSpellBlock < 0.0f) ? 1.0f : (100.0f / (100.0f + effectiveSpellBlock));
        }

        float bonus = 0.0f;
        if (target.HasBuff("cursedtouch")) {
            bonus = 0.1f * amount;
        }

        float result = multiplier * amount;
        result = SDK::DamageMod::DamageReductionMod(*this, target, result, DamageType::Magical);
        return std::max(std::floor(result) + bonus, 0.0f);
    }

    // ── GetAutoAttackDamage (AIBaseClient overload, no passives) ──
    float GetAutoAttackDamage(const AIBaseClient& target, bool includePassives) const {
        (void)includePassives;
        return CalculatePhysicalDamage(target, AD());
    }

    uintptr_t AiManagerAddress() const {
        return ::CoreAiManager::Address(Address());
    }

    ::CoreAiManager::ManagerRef GetAiManager() const {
        return ::CoreAiManager::Get(Address());
    }

    ::CoreAiManager::Snapshot AiManagerSnapshot() const {
        return ::CoreAiManager::ReadSnapshot(Address());
    }

    Vector3 ServerPosition() const {
        return ObjectCache::GetServerPosition(Address());
    }

    Vector3 PreviousPosition() const {
        return ::CoreAiManager::GetPreviousPosition(Address());
    }

    Vector3 Direction() const {
        return ::CoreAiManager::GetDirection(Address());
    }

    Vector3 Velocity() const {
        return ::CoreAiManager::GetVelocity(Address());
    }

    Vector3 PathStart() const {
        return ::CoreAiManager::GetPathStart(Address());
    }

    Vector3 PathEnd() const {
        return ::CoreAiManager::GetPathEnd(Address());
    }

    Vector3 OrderPosition() const {
        return ::CoreAiManager::GetOrderPosition(Address());
    }

    bool HasPath() const {
        return ::CoreAiManager::HasPath(Address());
    }

    bool IsMoving() const {
        return ::CoreAiManager::IsMoving(Address());
    }

    bool IsDashing() const {
        return ::CoreAiManager::IsDashing(Address());
    }

    bool HasArrived() const {
        return ::CoreAiManager::HasArrived(Address());
    }

    int CurrentPathSegment() const {
        return ::CoreAiManager::GetCurrentSegment(Address());
    }

    int WaypointCount() const {
        return ::CoreAiManager::GetWaypointCount(Address());
    }

    float DashSpeed() const {
        return ::CoreAiManager::GetDashSpeed(Address());
    }

    float DashDistanceRemaining() const {
        return ::CoreAiManager::GetDashDistRemaining(Address());
    }

    int CopyWaypoints(Vector3* out, int maxOut) const {
        return ::CoreAiManager::CopyWaypoints(
            Address(), reinterpret_cast<Vec3*>(out), maxOut);
    }

    std::vector<Vector3> GetWaypoints(int maxPoints = 32) const {
        if (!IsValid() || maxPoints <= 0) {
            return {};
        }
        // Served from the per-frame ObjectCache so repeated calls during one
        // prediction (GamePath / Movement call GetWaypoints 5-10x) resolve the
        // AiManager + read the nav array only once per frame.
        return ObjectCache::GetWaypoints(Address(), maxPoints);
    }

    const std::vector<Vector3>& CachedWaypoints() const {
        return ObjectCache::GetWaypointsFull(Address());
    }

    int CopyCachedWaypoints(Vector3* out, int maxPoints = 32) const {
        return ObjectCache::CopyWaypoints(Address(), out, maxPoints);
    }

    std::vector<Vector3> GetPath(int maxPoints = 32) const {
        return GetWaypoints(maxPoints);
    }

    std::vector<Vector3> Path() const {
        return GetWaypoints();
    }

    int GetPathLength() const {
        return static_cast<int>(CachedWaypoints().size());
    }

    bool IsMelee() const {
        return AttackRange() < 400.0f;
    }

    bool HasBuff(const char* name) const {
        return CoreBuffs::HasBuff(Address(), name);
    }

    int GetBuffCount(const char* name) const {
        return CoreBuffs::GetBuffStacks(Address(), name);
    }

    uintptr_t GetBuffCaster(const char* name) const {
        auto buff = CoreBuffs::FindByName(Address(), name);
        return buff.IsValid() ? buff.GetCaster() : 0;
    }

    bool HasItem(int id) const {
        return ::CoreItem::HasItemId(Address(), id);
    }

    bool HasItemInSlot(int slotIndex) const {
        return ::CoreItem::HasItem(Address(), slotIndex);
    }

    int GetItemId(int slotIndex) const {
        return ::CoreItem::GetItemId(Address(), slotIndex);
    }

    uintptr_t GetItemInfo(int slotIndex) const {
        return ::CoreItem::GetItemInfo(Address(), slotIndex);
    }

    int GetItemCount() const {
        return ::CoreItem::GetItemCount(Address());
    }

    bool HasTrinket() const {
        return ::CoreItem::HasTrinket(Address());
    }

    std::vector<InventorySlot> InventoryItems() const;

    bool IsRecalling() const {
        return HasBuff("recall");
    }

    SpellBookClient Spellbook() const {
        return SpellBookClient(Address());
    }

    SpellDataInstClient GetSpell(SpellSlot slot) const {
        return Spellbook().GetSpell(slot);
    }

    SpellSlot GetSpellSlot(const char* name) const {
        if (!name || !name[0]) {
            return SpellSlot::Unknown;
        }

        const SpellBookClient book = Spellbook();
        const std::string targetName = ObjectDetail::ToLower(name);
        for (int slot = 0; slot < ::CoreSpellDataInst::kMaxSpellSlots; ++slot) {
            SpellDataInstClient spell = book.GetSpell(static_cast<SpellSlot>(slot));
            if (!spell.IsValid()) {
                continue;
            }

            if (ObjectDetail::ToLower(spell.Name()) == targetName) {
                return static_cast<SpellSlot>(slot);
            }
        }
        return SpellSlot::Unknown;
    }

    float DistanceToPlayer() const {
        const auto player = ::Core::ObjectManager::Player();
        return player.IsValid() ? Position().Distance(player.position) : FLT_MAX;
    }

    int CountAllyHeroesInRange(float range) const;
    int CountEnemyHeroesInRange(float range) const;
    bool IsUnderAllyTurret() const;
    bool IsUnderEnemyTurret() const;
};

inline AIBaseClient SpellBookClient::Owner() const {
    return AIBaseClient(owner_, ::Core::ObjectManager::InferType(owner_));
}

class RuneManagerClient {
public:
    RuneManagerClient() = default;
    explicit RuneManagerClient(uintptr_t address)
        : address_(address) {}

    uintptr_t Address() const { return address_; }
    bool IsValid() const { return Globals::IsValidPtr(address_); }

    ::CoreRuneManager::RuneTreeData PrimaryTree() const {
        return ::CoreRuneManager::ReadPrimaryTree(address_);
    }

    ::CoreRuneManager::RuneTreeData SecondaryTree() const {
        return ::CoreRuneManager::ReadSecondaryTree(address_);
    }

    ::CoreRuneManager::ManagerSnapshot Snapshot() const {
        return ::CoreRuneManager::ReadFromManager(address_);
    }

    std::vector<::CoreRuneManager::RuneEntry> Entries() const {
        ::CoreRuneManager::RuneEntry entries[::CoreRuneManager::kMaxRuneEntries] = {};
        const int count = ::CoreRuneManager::ReadEntries(
            address_,
            entries,
            ::CoreRuneManager::kMaxRuneEntries);
        std::vector<::CoreRuneManager::RuneEntry> result;
        result.reserve(count > 0 ? static_cast<std::size_t>(count) : 0);
        for (int index = 0; index < count; ++index) {
            result.push_back(entries[index]);
        }
        return result;
    }

    const Data::RuneDatabase::RuneStyleDataEntry* PrimaryTreeData() const {
        const auto tree = PrimaryTree();
        return Data::RuneDatabase::FindStyleById(tree.id);
    }

    const Data::RuneDatabase::RuneStyleDataEntry* SecondaryTreeData() const {
        const auto tree = SecondaryTree();
        return Data::RuneDatabase::FindStyleById(tree.id);
    }

    const Data::RuneDatabase::RuneDataEntry* GetRuneData(int runeId) const {
        return Data::RuneDatabase::FindById(runeId);
    }

    bool HasRune(int runeId) const {
        if (runeId <= 0) {
            return false;
        }

        const auto nativeEntries = Entries();
        for (const auto& entry : nativeEntries) {
            if (entry.data.id == runeId) {
                return true;
            }
        }
        return false;
    }

    std::vector<const Data::RuneDatabase::RuneDataEntry*> RuneDataEntries() const {
        const auto nativeEntries = Entries();
        std::vector<const Data::RuneDatabase::RuneDataEntry*> result;
        result.reserve(nativeEntries.size());
        for (const auto& entry : nativeEntries) {
            if (const auto* data = Data::RuneDatabase::FindById(entry.data.id)) {
                result.push_back(data);
            }
        }
        return result;
    }

private:
    uintptr_t address_ = 0;
};

class AIHeroClient : public AIBaseClient {
public:
    AIHeroClient() = default;
    explicit AIHeroClient(uintptr_t address)
        : AIBaseClient(address, ::Core::Objects::ObjectType::AIHeroClient) {}
    explicit AIHeroClient(::Core::Objects::ObjectHandle handle)
        : AIBaseClient(handle) {
        handle_.type = ::Core::Objects::ObjectType::AIHeroClient;
    }

    SpellBookClient Spellbook() const {
        return SpellBookClient(Address());
    }

    SpellSlot GetSpellSlot(const char* name) const {
        return AIBaseClient::GetSpellSlot(name);
    }

    RuneManagerClient RuneManager() const {
        return RuneManagerClient(::CoreRuneManager::Resolve(Address()));
    }

    bool HasRune(int runeId) const {
        return RuneManager().HasRune(runeId);
    }
};

class AIMinionClient : public AIBaseClient {
public:
    AIMinionClient() = default;
    explicit AIMinionClient(uintptr_t address)
        : AIBaseClient(address, ::Core::Objects::ObjectType::AIMinionClient) {}
    explicit AIMinionClient(::Core::Objects::ObjectHandle handle)
        : AIBaseClient(handle) {
        handle_.type = ::Core::Objects::ObjectType::AIMinionClient;
    }

    ::Core::Objects::MinionClass GetMinionClass() const {
        const uint32_t idx = static_cast<uint32_t>(handle_.index & 0xFFFFu);
        if (const auto* sc = StaticStringCache::Get(idx)) {
            if (sc->minionClass != 0) {
                return static_cast<::Core::Objects::MinionClass>(sc->minionClass);
            }
        }
        const uintptr_t a = Address();
        return a ? ::Core::Objects::ReadMinionClass(a) : ::Core::Objects::MinionClass::Unset;
    }

    MinionTypes GetMinionType() const {
        const std::string& charName = CharacterName();
        const std::string& unitName = Name();
        const std::string name = !charName.empty() ? charName : unitName;

        if (ObjectDetail::EqualsAny(name, {
            "SRU_ChaosMinionMelee", "SRU_OrderMinionMelee",
            "HA_ChaosMinionMelee", "HA_OrderMinionMelee",
        })) {
            return MinionTypes::Normal | MinionTypes::Melee;
        }

        if (ObjectDetail::EqualsAny(name, {
            "SRU_ChaosMinionRanged", "SRU_OrderMinionRanged",
            "HA_ChaosMinionRanged", "HA_OrderMinionRanged",
        })) {
            return MinionTypes::Normal | MinionTypes::Ranged;
        }

        if (ObjectDetail::EqualsAny(name, {
            "SRU_ChaosMinionSiege", "SRU_OrderMinionSiege",
            "HA_ChaosMinionSiege", "HA_OrderMinionSiege",
        })) {
            return MinionTypes::Siege | MinionTypes::Ranged;
        }

        if (ObjectDetail::EqualsAny(name, {
            "SRU_ChaosMinionSuper", "SRU_OrderMinionSuper",
            "HA_ChaosMinionSuper", "HA_OrderMinionSuper",
        })) {
            return MinionTypes::Super | MinionTypes::Melee;
        }

        if (ObjectDetail::EqualsAny(name, {
            "SightWard", "VisionWard", "YellowTrinket", "BlueTrinket",
            "JammerDevice", "JammerDeviceItem",
            "SionUlt_Ward", "SionPassiveCorpse",
        })) {
            return MinionTypes::Ward;
        }

        // Fall back to cached minionClass byte
        switch (GetMinionClass()) {
        case ::Core::Objects::MinionClass::MeleeLaneMinion:
            return MinionTypes::Normal | MinionTypes::Melee;
        case ::Core::Objects::MinionClass::RangedLaneMinion:
            return MinionTypes::Normal | MinionTypes::Ranged;
        case ::Core::Objects::MinionClass::SiegeLaneMinion:
            return MinionTypes::Siege | MinionTypes::Ranged;
        case ::Core::Objects::MinionClass::SuperLaneMinion:
            return MinionTypes::Super | MinionTypes::Melee;
        case ::Core::Objects::MinionClass::TeamMinion:
            return MinionTypes::Normal;
        default:
            return MinionTypes::Unknown;
        }
    }

    // Update May 2026 (CDragon map11.bin.json + Riot character folders).
    // Plant scriptnames live under /Characters/SRU_Plant_* and use the
    // "SRU_Plant_*" character name family. Voidgrubs added in 13.20 ship
    // as "SRU_Voidgrub_*" and are an early epic objective. Sentinel
    // (SRU_Sentinel) is the new Atakhan precursor (14.20+) but the Atakhan
    // boss itself is "Atakhan_*". KrugAncient is the Krug elder. RiftHrald
    // and Baron retain their classic names.
    JungleType GetJungleType() const {
        const std::string& charName = CharacterName();
        const std::string& unitName = Name();
        const std::string name = !charName.empty() ? charName : unitName;

        // Plants: blast cone, honeyfruit, scryer's bloom (visible-only +
        // Smolder's Twin Shadows passive variant). Map under the same enum
        // as jungle so target selectors / orbwalker can filter them out.
        if (ObjectDetail::ContainsAny(name, {
            "SRU_Plant_Satchel",   // Blast Cone (đèn nổ)
            "SRU_Plant_Health",    // Honeyfruit (trái cây hồi máu)
            "SRU_Plant_Vision",    // Scryer's Bloom (đèn soi sáng / hạt thông soi)
            "Plant_Satchel", "Plant_Health", "Plant_Vision",
        })) {
            return JungleType::Plant;
        }

        // Small camp minions (mini wolves/krugs/raptors + TT mini camps).
        if (ObjectDetail::ContainsAny(name, {
            "SRU_RazorbeakMini", "SRU_MurkwolfMini", "SRU_KrugMini",
            "SRU_KrugMiniMini",   // post 13.18 mini krug split
            "SRU_GrompMini",      // future-proof if Riot adds split
            "TestCubeRender",
            "TT_NGolem2", "TT_NWraith2", "TT_NWolf2",
        })) {
            return JungleType::Small;
        }

        // Large camp monsters + Krug Ancient (the elder krug).
        if (ObjectDetail::ContainsAny(name, {
            "SRU_Razorbeak", "SRU_Red", "SRU_Krug", "SRU_KrugAncient",
            "SRU_Murkwolf", "SRU_Blue", "SRU_Gromp",
            "Sru_Crab", "SRU_Crab",
            "TT_NGolem", "TT_NWraith", "TT_NWolf",
        })) {
            return JungleType::Large;
        }

        // Voidgrubs are an early-game epic objective that spawns 3 at a
        // time in the Baron pit; treat them as Epic so jungle clear logic
        // can prioritise them differently from large camps.
        if (ObjectDetail::ContainsAny(name, {
            "SRU_Voidgrub", "SRU_Voidgrubs", "SRU_Horde",
        })) {
            return JungleType::Epic;
        }

        // Legendary epic monsters: dragons, Baron, Rift Herald, Atakhan,
        // Sentinel (Atakhan precursor), TT Vilemaw.
        if (ObjectDetail::ContainsAny(name, {
            "SRU_Dragon_Air", "SRU_Dragon_Earth", "SRU_Dragon_Fire",
            "SRU_Dragon_Water", "SRU_Dragon_Elder", "SRU_Dragon_Hextech",
            "SRU_Dragon_Chemtech",
            "SRU_RiftHerald", "SRU_Baron",
            "SRU_Sentinel", "Atakhan_",   // Atakhan boss family
            "TT_Spiderboss",
        })) {
            return JungleType::Legendary;
        }

        return JungleType::Unknown;
    }

    bool IsMinion() const {
        const MinionTypes type = GetMinionType();
        return HasFlag(type, MinionTypes::Melee) || HasFlag(type, MinionTypes::Ranged);
    }

    // True for jungle plants only (Blast Cone / Honeyfruit / Scryer's Bloom).
    // Use this to skip them from auto-attack target lists — they are
    // technically AIMinionClient with `Team == Neutral` but are NOT real
    // monsters; auto-attacking them wastes a basic attack and gives no
    // gold/xp. Mirrors EnsoulSharp's IsPlant convention.
    bool IsPlant() const {
        return GetJungleType() == JungleType::Plant;
    }

    bool IsJungle() const {
        const JungleType jt = GetJungleType();
        return jt != JungleType::Unknown && jt != JungleType::Plant;
    }

    bool IsJungleBuff() const {
        const std::string name = CharacterName();
        return name == "SRU_Blue" || name == "SRU_Red";
    }
};


// Update May 2026: turret detection helpers. CDragon Map11 ships several
// stationary tower scriptnames that all live under AITurretClient but
// have very different combat behaviour. Without classification, an
// orbwalker may try to attack a fountain turret (untargetable, infinite
// damage) or an azir-soldier-tower-like dummy (not a real structure).
class AITurretClient : public AIBaseClient {
public:
    AITurretClient() = default;
    explicit AITurretClient(uintptr_t address)
        : AIBaseClient(address, ::Core::Objects::ObjectType::AITurretClient) {}
    explicit AITurretClient(::Core::Objects::ObjectHandle handle)
        : AIBaseClient(handle) {
        handle_.type = ::Core::Objects::ObjectType::AITurretClient;
    }

    // Lane turret families (outer / inner / inhib / nexus) on Summoner's
    // Rift, Howling Abyss, Nexus Blitz. Riot scriptnames keep "Turret"
    // anywhere in CharacterName; the precise tier comes from the Name
    // ("Turret_T1_R_03_A" = bot outer, etc.) but for orbwalker target
    // selection only the Lane/Fountain/Other split matters.
    bool IsLaneTurret() const {
        const std::string name = ObjectDetail::ToLower(CharacterName());
        if (name.find("turret") == std::string::npos) {
            return false;
        }
        // Fountain/shrine turrets appear with the SAME CharacterName but
        // their unit Name starts with "Turret_OrderTurretShrine" /
        // "Turret_ChaosTurretShrine"; carve them out below.
        const std::string unitName = ObjectDetail::ToLower(Name());
        if (unitName.find("shrine") != std::string::npos ||
            unitName.find("nexustower") != std::string::npos) {
            return false;
        }
        return true;
    }

    // Fountain / nexus shrine turrets. Untargetable when a friendly hero
    // is in fountain — orbwalker MUST never target these even when the
    // game reports them as enemy & targetable, because attacking them
    // immediately puts the player in the fountain laser's range.
    bool IsFountainTurret() const {
        const std::string unitName = ObjectDetail::ToLower(Name());
        return unitName.find("shrine") != std::string::npos ||
               unitName.find("nexustower") != std::string::npos;
    }

    // Shuriman / Azir-style summoned tower (sand soldier or special map
    // event). These are NOT real lane structures — they spawn from
    // gameplay scripts and orbwalker should treat them as champion-pet.
    bool IsShurimaTurret() const {
        const std::string name = ObjectDetail::ToLower(CharacterName());
        return name.find("shuriman") != std::string::npos ||
               name.find("azirsoldier") != std::string::npos;
    }
};


class MissileClient : public GameObject {
public:
    MissileClient() = default;
    explicit MissileClient(uintptr_t address)
        : GameObject(address, ::Core::Objects::ObjectType::MissileClient) {}
    explicit MissileClient(::Core::Objects::ObjectHandle handle)
        : GameObject(handle) {
        handle_.type = ::Core::Objects::ObjectType::MissileClient;
    }

    int CasterIndex() const {
        const uintptr_t a = Address();
        return a ? static_cast<int>(*reinterpret_cast<const uint32_t*>(a + Offset::MissileClient::CasterIndex)) : 0;
    }
    int TargetIndex() const {
        const uintptr_t a = Address();
        return a ? ReadTargetIndex(a) : 0;
    }
    int CasterNetworkId() const { return ResolveNetworkIdFromIndex(static_cast<uint32_t>(CasterIndex())); }
    int TargetNetworkId() const { return ResolveNetworkIdFromIndex(static_cast<uint32_t>(TargetIndex())); }
    std::string SpellName() const {
        const uint32_t idx = static_cast<uint32_t>(handle_.index & 0xFFFFu);
        if (auto* sc = StaticStringCache::GetMutable(idx)) {
            if (sc->valid && !sc->name.empty()) return sc->name;
        }
        const uintptr_t a = Address();
        if (!a) return {};
        char buf[96] = {};
        const uintptr_t spellData =
            Globals::Read<uintptr_t>(a + Offset::MissileClient::SpellDataPtr);
        if (!Globals::IsValidPtr(spellData) ||
            !Globals::ReadRuntimeStringField(
                spellData + Offset::MissileClient::SpellDataSpellName,
                buf,
                static_cast<int>(sizeof(buf)))) {
            Globals::ReadRuntimeStringField(
                a + Offset::MissileClient::SpellName,
                buf,
                static_cast<int>(sizeof(buf)));
        }
        if (buf[0]) {
            if (auto* sc = StaticStringCache::GetMutable(idx)) {
                sc->name = buf;
                sc->valid = true;
            }
        }
        return buf;
    }
    std::string MissileName() const {
        const uint32_t idx = static_cast<uint32_t>(handle_.index & 0xFFFFu);
        if (auto* sc = StaticStringCache::GetMutable(idx)) {
            if (sc->valid && !sc->characterName.empty()) return sc->characterName;
        }
        const uintptr_t a = Address();
        if (!a) return {};
        char buf[96] = {};
        const uintptr_t spellData =
            Globals::Read<uintptr_t>(a + Offset::MissileClient::SpellDataPtr);
        if (!Globals::IsValidPtr(spellData) ||
            !Globals::ReadRuntimeStringField(
                spellData + Offset::MissileClient::SpellDataMissileName,
                buf,
                static_cast<int>(sizeof(buf)))) {
            Globals::ReadRuntimeStringField(
                a + Offset::MissileClient::MissileName,
                buf,
                static_cast<int>(sizeof(buf)));
        }
        if (buf[0]) {
            if (auto* sc = StaticStringCache::GetMutable(idx)) {
                sc->characterName = buf;
                sc->valid = true;
            }
        }
        return buf;
    }
    Vector3 StartPosition() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const Vector3*>(a + Offset::MissileClient::StartPos) : Vector3{};
    }
    Vector3 EndPosition() const {
        const uintptr_t a = Address();
        return a ? *reinterpret_cast<const Vector3*>(a + Offset::MissileClient::EndPos) : Vector3{};
    }

private:
    static int ReadTargetIndex(uintptr_t address) {
        const uintptr_t targetArrayPtr =
            Globals::Read<uintptr_t>(address + Offset::MissileClient::TargetArrayPtr);
        const std::uint32_t targetArrayCount =
            Globals::Read<std::uint32_t>(address + Offset::MissileClient::TargetArrayCount);
        if (!targetArrayPtr || targetArrayCount == 0 || targetArrayCount >= 64) {
            return 0;
        }

        const std::uint32_t targetLocalId =
            Globals::Read<std::uint32_t>(targetArrayPtr);
        return targetLocalId != 0 && targetLocalId != 0xFFFFFFFFu
            ? static_cast<int>(targetLocalId)
            : 0;
    }

    static int ResolveNetworkIdFromIndex(std::uint32_t index) {
        if (index == 0 || index == 0xFFFFFFFFu) {
            return 0;
        }

        const uintptr_t object = ::Core::ObjectManager::FindByIndex(index);
        if (!Globals::IsValidPtr(object)) {
            return 0;
        }

        const std::uint32_t networkId = ::Core::Objects::ReadNetworkId(object);
        return networkId != 0 && networkId != 0xFFFFFFFFu
            ? static_cast<int>(networkId)
            : 0;
    }
};

// Inhibitor / nhà lính. CDragon Map11 spawns 6 inhibitors total
// (Order top/mid/bot + Chaos top/mid/bot) under unit names
// "Barracks_T1_C{1..3}" (Order) and "Barracks_T2_L{1..3}" (Chaos).
// Lane is encoded in the unit Name suffix, NOT in CharacterName, so use
// Name() to decide whether this is the top, mid, or bottom inhibitor.
class BarracksDampenerClient : public AttackableUnit {
public:
    BarracksDampenerClient() = default;
    explicit BarracksDampenerClient(uintptr_t address)
        : AttackableUnit(address, ::Core::Objects::ObjectType::BarracksDampenerClient) {}
    explicit BarracksDampenerClient(::Core::Objects::ObjectHandle handle)
        : AttackableUnit(handle) {
        handle_.type = ::Core::Objects::ObjectType::BarracksDampenerClient;
    }

    // Inhibitor lane index: 0 = top, 1 = mid, 2 = bot.
    // Unit Name pattern: "Barracks_T{team}_{LaneCode}{laneIndex}_..."
    //   LaneCode T = top, C = mid (center), L = bot
    //   The trailing index is the inhib position within the lane (always 1
    //   on Summoner's Rift) — we map LaneCode to {0,1,2} for orbwalker
    //   priority lists.
    int LaneIndex() const {
        const std::string unitName = ObjectDetail::ToLower(Name());
        if (unitName.find("_t1") != std::string::npos ||
            unitName.find("_t2") != std::string::npos) {
            // Both teams use _T1_ / _T2_ for the team prefix; we need the
            // SECOND letter token after the team prefix to identify lane.
            const auto laneTokenPos = unitName.find("_l1");
            if (laneTokenPos != std::string::npos) return 2; // bot
            const auto midTokenPos = unitName.find("_c1");
            if (midTokenPos != std::string::npos) return 1; // mid
            return 0; // top by default
        }
        return -1;
    }

    bool IsTopLane() const { return LaneIndex() == 0; }
    bool IsMidLane() const { return LaneIndex() == 1; }
    bool IsBotLane() const { return LaneIndex() == 2; }
};

// Nexus / nhà chính. CDragon Map11 spawns exactly 2 (one per team) and
// they live in front of the spawn fountain. They are vulnerable only
// AFTER both inhibitors of any single lane are destroyed; before that
// HasShield() returns true and orbwalker should not target them.
class HQClient : public AttackableUnit {
public:
    HQClient() = default;
    explicit HQClient(uintptr_t address)
        : AttackableUnit(address, ::Core::Objects::ObjectType::HQClient) {}
    explicit HQClient(::Core::Objects::ObjectHandle handle)
        : AttackableUnit(handle) {
        handle_.type = ::Core::Objects::ObjectType::HQClient;
    }

    // Nexus is invulnerable until at least one inhibitor of the
    // attacker's team is destroyed AND its respawn timer hasn't ticked
    // back. Use the engine-reported invulnerability flag — Riot keeps
    // this in sync with the inhibitor state, so we don't need to
    // re-derive it from BarracksDampenerClient health.
    bool HasShield() const {
        return IsInvulnerable();
    }
};

class ShopClient : public GameObject {
public:
    ShopClient() = default;
    explicit ShopClient(uintptr_t address)
        : GameObject(address, ::Core::Objects::ObjectType::ShopClient) {}
    explicit ShopClient(::Core::Objects::ObjectHandle handle)
        : GameObject(handle) {
        handle_.type = ::Core::Objects::ObjectType::ShopClient;
    }
};

class Obj_SpawnPoint : public GameObject {
public:
    Obj_SpawnPoint() = default;
    explicit Obj_SpawnPoint(uintptr_t address)
        : GameObject(address, ::Core::Objects::ObjectType::Obj_SpawnPoint) {}
    explicit Obj_SpawnPoint(::Core::Objects::ObjectHandle handle)
        : GameObject(handle) {
        handle_.type = ::Core::Objects::ObjectType::Obj_SpawnPoint;
    }
};

class EffectEmitter : public GameObject {
public:
    EffectEmitter() = default;
    explicit EffectEmitter(uintptr_t address)
        : GameObject(address, ::Core::Objects::ObjectType::EffectEmitter) {}
    explicit EffectEmitter(::Core::Objects::ObjectHandle handle)
        : GameObject(handle) {
        handle_.type = ::Core::Objects::ObjectType::EffectEmitter;
    }
};

} // namespace SDK
