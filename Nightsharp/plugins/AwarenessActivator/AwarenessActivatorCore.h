#pragma once

// Shared, replay-testable state and decision primitives for the Awareness and
// Activator plugins.  The implementation lives under plugins and uses the SDK
// ItemId type at the capability boundary; no renderer or game-global state is
// required by these data structures.

#include "../../sdk/Enumerations/ItemId.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <unordered_map>

namespace NightSharp::Companion {

inline std::uint32_t HashId(std::string_view text) noexcept {
    std::uint32_t hash = 2166136261u;
    for (const unsigned char c : text) {
        hash ^= static_cast<std::uint32_t>(std::tolower(c));
        hash *= 16777619u;
    }
    return hash;
}

template <std::size_t N>
inline void CopyText(char (&out)[N], std::string_view value) noexcept {
    static_assert(N > 0);
    const std::size_t count = std::min<std::size_t>(N - 1, value.size());
    if (count > 0) std::char_traits<char>::copy(out, value.data(), count);
    out[count] = '\0';
}

template <std::size_t N>
inline void CopyText(char (&out)[N], const char* value) noexcept {
    CopyText(out, value ? std::string_view(value) : std::string_view{});
}
inline bool TextEqualsInsensitive(std::string_view left,
                                  std::string_view right) noexcept {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        const auto lhs = static_cast<unsigned char>(left[i]);
        const auto rhs = static_cast<unsigned char>(right[i]);
        if (std::tolower(lhs) != std::tolower(rhs)) return false;
    }
    return true;
}

inline bool TextContainsInsensitive(std::string_view value,
                                    std::string_view needle) noexcept {
    if (needle.empty() || value.size() < needle.size()) return false;
    for (std::size_t offset = 0;
         offset + needle.size() <= value.size(); ++offset) {
        if (TextEqualsInsensitive(value.substr(offset, needle.size()), needle)) {
            return true;
        }
    }
    return false;
}

struct Point3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    bool IsValid() const noexcept {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }
    bool IsZero() const noexcept {
        return std::fabs(x) < 0.001f && std::fabs(y) < 0.001f && std::fabs(z) < 0.001f;
    }
    float DistanceSquared(const Point3& other) const noexcept {
        const float dx = x - other.x;
        const float dy = y - other.y;
        const float dz = z - other.z;
        return dx * dx + dy * dy + dz * dz;
    }
    float Distance(const Point3& other) const noexcept {
        return std::sqrt(DistanceSquared(other));
    }
    Point3 operator+(const Point3& other) const noexcept {
        return { x + other.x, y + other.y, z + other.z };
    }
    Point3 operator-(const Point3& other) const noexcept {
        return { x - other.x, y - other.y, z - other.z };
    }
    Point3 operator*(float scalar) const noexcept {
        return { x * scalar, y * scalar, z * scalar };
    }
};

inline Point3 Normalize2D(const Point3& value) noexcept {
    const float length = std::sqrt(value.x * value.x + value.z * value.z);
    if (!std::isfinite(length) || length < 0.001f) return {};
    return { value.x / length, 0.0f, value.z / length };
}

enum class Provenance : std::uint8_t {
    Unknown = 0,
    VisibleNow,
    ObservedEvent,
    LastSeen,
    ManualInput,
    Estimated,
};

enum class Confidence : std::uint8_t {
    Unknown = 0,
    Low = 25,
    Medium = 50,
    High = 75,
    Confirmed = 100,
};

inline const char* ConfidenceName(Confidence value) noexcept {
    switch (value) {
    case Confidence::Confirmed: return "Confirmed";
    case Confidence::High: return "High";
    case Confidence::Medium: return "Medium";
    case Confidence::Low: return "Low";
    default: return "Unknown";
    }
}

inline const char* ProvenanceName(Provenance value) noexcept {
    switch (value) {
    case Provenance::VisibleNow: return "VisibleNow";
    case Provenance::ObservedEvent: return "ObservedEvent";
    case Provenance::LastSeen: return "LastSeen";
    case Provenance::ManualInput: return "ManualInput";
    case Provenance::Estimated: return "Estimated";
    default: return "Unknown";
    }
}

struct Evidence {
    Provenance provenance = Provenance::Unknown;
    Confidence confidence = Confidence::Unknown;
    float observedAt = 0.0f;
    float validUntil = 0.0f;
    std::uint32_t source = 0;

    bool IsKnown() const noexcept {
        return provenance != Provenance::Unknown && confidence != Confidence::Unknown;
    }
    bool IsExpired(float now) const noexcept {
        return validUntil > 0.0f && now >= validUntil;
    }
};

template <typename T>
struct ProvenanceValue {
    T value{};
    Evidence evidence{};
    void Set(const T& next, Evidence nextEvidence) {
        value = next;
        evidence = nextEvidence;
    }
};

enum class RuntimeMode : std::uint8_t {
    Companion = 0,
    Practice,
    Research,
    Spectator,
    Replay,
};

struct ExposureDecision {
    bool allowed = false;
    bool showExactPosition = false;
    bool showUncertainty = false;
    bool showAge = false;
};

class VisibilityGuard final {
public:
    static ExposureDecision CanExpose(const Evidence& evidence,
                                      RuntimeMode mode,
                                      bool currentlyVisible,
                                      bool positionRequest = false) noexcept {
        ExposureDecision result{};
        if (currentlyVisible && evidence.provenance == Provenance::VisibleNow) {
            result.allowed = true;
            result.showExactPosition = positionRequest;
            return result;
        }
        switch (evidence.provenance) {
        case Provenance::ObservedEvent:
        case Provenance::LastSeen:
        case Provenance::ManualInput:
            result.allowed = true;
            result.showAge = true;
            result.showUncertainty = positionRequest;
            return result;
        case Provenance::Estimated:
            result.allowed = evidence.confidence != Confidence::Unknown;
            result.showAge = true;
            result.showUncertainty = true;
            return result;
        default:
            break;
        }
        (void)mode;
        return result;
    }

    static bool CanExposePosition(const Evidence& evidence,
                                  RuntimeMode mode,
                                  bool currentlyVisible) noexcept {
        return CanExpose(evidence, mode, currentlyVisible, true).allowed;
    }
};

template <typename T, std::size_t Capacity>
class FixedRing final {
public:
    static_assert(Capacity > 0);
    void Clear() noexcept { head_ = 0; count_ = 0; }
    void Push(const T& value) noexcept {
        if (count_ < Capacity) {
            values_[(head_ + count_) % Capacity] = value;
            ++count_;
        } else {
            values_[head_] = value;
            head_ = (head_ + 1) % Capacity;
        }
    }
    std::size_t Size() const noexcept { return count_; }
    bool Empty() const noexcept { return count_ == 0; }
    const T& At(std::size_t index) const noexcept {
        return values_[(head_ + index) % Capacity];
    }
    T& At(std::size_t index) noexcept {
        return values_[(head_ + index) % Capacity];
    }
    template <typename Fn>
    void ForEach(Fn&& fn) const {
        for (std::size_t i = 0; i < count_; ++i) fn(At(i));
    }
    template <typename Predicate>
    std::size_t RemoveIf(Predicate&& shouldRemove) noexcept {
        std::size_t write = 0;
        std::size_t removed = 0;
        for (std::size_t read = 0; read < count_; ++read) {
            T& current = At(read);
            if (shouldRemove(current)) {
                ++removed;
                continue;
            }
            if (write != read) {
                At(write) = current;
            }
            ++write;
        }
        count_ = write;
        return removed;
    }
private:
    std::array<T, Capacity> values_{};
    std::size_t head_ = 0;
    std::size_t count_ = 0;
};

struct ClockSnapshot {
    float rawGameTime = 0.0f;
    float logicalGameTime = 0.0f;
    float pingSeconds = 0.0f;
    std::uint64_t generation = 0;
    bool reconnectAdjusted = false;
    bool remakeDetected = false;
};

class GameClock final {
public:
    ClockSnapshot Update(float rawGameTime, int pingMs, RuntimeMode mode) noexcept {
        if (!std::isfinite(rawGameTime) || rawGameTime < 0.0f) return snapshot_;
        const float pingSeconds = std::clamp(static_cast<float>(std::max(0, pingMs)) / 1000.0f,
                                             0.0f, 5.0f);
        bool adjusted = false;
        bool remake = false;
        if (!initialized_) {
            initialized_ = true;
            offset_ = 0.0f;
            logical_ = rawGameTime;
        } else if (mode != RuntimeMode::Replay &&
                   lastRaw_ > 180.0f && rawGameTime < 30.0f) {
            offset_ = 0.0f;
            logical_ = rawGameTime;
            ++generation_;
            adjusted = true;
            remake = true;
        } else if (mode == RuntimeMode::Replay && rawGameTime + 1.0f < lastRaw_) {
            offset_ = 0.0f;
            logical_ = rawGameTime;
            ++generation_;
            adjusted = true;
        } else if (rawGameTime + 1.0f < lastRaw_) {
            offset_ = logical_ - rawGameTime;
            ++generation_;
            adjusted = true;
        } else {
            const float candidate = rawGameTime + offset_;
            if (candidate + 0.001f < logical_) {
                offset_ = logical_ - rawGameTime;
                adjusted = true;
            }
            logical_ = std::max(logical_, rawGameTime + offset_);
        }
        lastRaw_ = rawGameTime;
        snapshot_ = {
            rawGameTime, logical_, pingSeconds, generation_, adjusted, remake
        };
        return snapshot_;
    }
    float Now() const noexcept { return logical_; }
    float PingSeconds() const noexcept { return snapshot_.pingSeconds; }
    std::uint64_t Generation() const noexcept { return generation_; }
    const ClockSnapshot& Snapshot() const noexcept { return snapshot_; }
    void Reset() noexcept {
        initialized_ = false;
        lastRaw_ = logical_ = offset_ = 0.0f;
        generation_ = 0;
        snapshot_ = {};
    }
private:
    bool initialized_ = false;
    float lastRaw_ = 0.0f;
    float logical_ = 0.0f;
    float offset_ = 0.0f;
    std::uint64_t generation_ = 0;
    ClockSnapshot snapshot_{};
};

enum class EventType : std::uint8_t {
    ObjectCreated = 0, ObjectDeleted, SpellCastStarted, SpellCastCompleted,
    SpellCastCancelled, BuffAdded, BuffRemoved, VisibilityChanged,
    InventoryChanged, SummonerSpellChanged, ObjectiveSpawned, ObjectiveKilled,
    WardPlaced, WardDestroyed, UnitDied, UnitRespawned, TeleportStarted,
    TeleportEnded, RecallStarted, RecallEnded, ChannelInterrupted, PathChanged,
    ThreatCreated, ThreatRemoved, DamageObserved, GameReset,
};

struct GameEvent {
    EventType type = EventType::GameReset;
    float at = 0.0f;
    std::uint32_t objectId = 0;
    std::uint32_t sourceId = 0;
    std::uint32_t targetId = 0;
    std::uint32_t nameHash = 0;
    int slot = -1;
    int itemId = 0;
    int value = 0;
    float valueFloat = 0.0f;
    float duration = 0.0f;
    float speed = 0.0f;
    Point3 position{};
    Point3 startPosition{};
    Point3 endPosition{};
    bool visibleAtEvent = false;
    bool enemy = false;
    bool ally = false;
    char name[96] = {};
    char secondaryName[96] = {};
};

class EventBus final {
public:
    using Callback = void(*)(void*, const GameEvent&);
    struct Subscription {
        Callback callback = nullptr;
        void* context = nullptr;
        bool active = false;
    };
    bool Subscribe(Callback callback, void* context) noexcept {
        if (!callback) return false;
        for (auto& slot : subscribers_) {
            if (slot.active && slot.callback == callback && slot.context == context) return true;
        }
        for (auto& slot : subscribers_) {
            if (!slot.active) {
                slot = { callback, context, true };
                return true;
            }
        }
        return false;
    }
    void Unsubscribe(Callback callback, void* context) noexcept {
        for (auto& slot : subscribers_) {
            if (slot.active && slot.callback == callback && slot.context == context) slot = {};
        }
    }
    void Publish(const GameEvent& event) noexcept {
        recent_.Push(event);
        for (auto& slot : subscribers_) {
            if (!slot.active || !slot.callback) continue;
            try {
                slot.callback(slot.context, event);
            } catch (...) {
                slot.active = false;
            }
        }
    }
    const FixedRing<GameEvent, 512>& Recent() const noexcept { return recent_; }
    void Clear() noexcept { recent_.Clear(); }
private:
    std::array<Subscription, 24> subscribers_{};
    FixedRing<GameEvent, 512> recent_{};
};

enum class CooldownKind : std::uint8_t { Unknown = 0, ExactObserved, RangeEstimated };

enum class Capability : std::uint16_t {
    None = 0, Barrier, Cleanse, Exhaust, Flash, Ghost, Heal, Ignite, Smite,
    Teleport, Qss, Mercurial, Mikael, Zhonya, Seeker, Seraph, Locket, Redemption,
    Shurelya, Youmuu, Rocketbelt, Stridebreaker, Gunblade, Tiamat,
    RavenousHydra, TitanicHydra, ProfaneHydra, Randuin, Actualizer, Oracle,
    Farsight, Ward, Potion, KnightsVow,
};

inline const char* CapabilityName(Capability capability) noexcept {
    switch (capability) {
    case Capability::Barrier: return "Barrier"; case Capability::Cleanse: return "Cleanse";
    case Capability::Exhaust: return "Exhaust"; case Capability::Flash: return "Flash";
    case Capability::Ghost: return "Ghost"; case Capability::Heal: return "Heal";
    case Capability::Ignite: return "Ignite"; case Capability::Smite: return "Smite";
    case Capability::Teleport: return "Teleport"; case Capability::Qss: return "QSS";
    case Capability::Mercurial: return "Mercurial"; case Capability::Mikael: return "Mikael";
    case Capability::Zhonya: return "Zhonya"; case Capability::Seeker: return "Seeker";
    case Capability::Seraph: return "Seraph"; case Capability::Locket: return "Locket";
    case Capability::Redemption: return "Redemption";
    case Capability::Shurelya: return "Shurelya"; case Capability::Youmuu: return "Youmuu";
    case Capability::Rocketbelt: return "Rocketbelt"; case Capability::Stridebreaker: return "Stridebreaker";
    case Capability::Gunblade: return "Gunblade"; case Capability::Tiamat: return "Tiamat";
    case Capability::RavenousHydra: return "RavenousHydra"; case Capability::TitanicHydra: return "TitanicHydra";
    case Capability::ProfaneHydra: return "ProfaneHydra"; case Capability::Randuin: return "Randuin";
    case Capability::Actualizer: return "Actualizer"; case Capability::Oracle: return "Oracle";
    case Capability::Farsight: return "Farsight"; case Capability::Ward: return "Ward";
    case Capability::Potion: return "Potion"; case Capability::KnightsVow: return "KnightsVow"; default: return "None";
    }
}

enum class WardKind : std::uint8_t { Unknown = 0, Stealth, Control, Farsight, Support, Zombie, Trap, Faelight, OracleSweep };
enum class ObjectiveKind : std::uint8_t { Unknown = 0, ElementalDragon, DragonSoul, ElderDragon, VoidGrubs, RiftHerald, Baron, Scuttle };
enum class ObjectiveStatus : std::uint8_t { NotSpawned = 0, SpawningSoon, AliveVisible, AliveUnknown, InCombatVisible, Dead, Respawning, Disabled };
enum class ThreatGeometry : std::uint8_t { Point = 0, Line, Circle, Cone, Ring };
enum class CrowdControl : std::uint8_t { None = 0, Stun, Root, Charm, Fear, Taunt, Blind, Silence, Polymorph, Slow, Sleep, Suppression, Airborne, Stasis, Grounded };
enum class CloneKind : std::uint8_t {
    None = 0,
    ShacoHallucination,
    LeblancMirror,
    NeekoTrick,
    WukongDecoy,
    Unknown,
};
enum class SpellOrigin : std::uint8_t {
    Native = 0,
    StolenUltimate,
    AcquiredSummoner,
    SwappedSummoner,
    EvolvedSummoner,
    FormScoped,
};
enum class StasisSource : std::uint8_t {
    None = 0,
    Zhonya,
    Seeker,
    Bard,
    Lissandra,
    GuardianAngel,
    ChampionAbility,
    Unknown,
};
enum class ReviveSource : std::uint8_t {
    None = 0,
    GuardianAngel,
    Zilean,
    Renata,
    Anivia,
    Zac,
    ChampionAbility,
    Unknown,
};

enum class RoleQuestRuleset : std::uint8_t {
    Standard = 0,
    Swiftplay,
    Rotating,
};

inline constexpr std::uint32_t RulesetMask(
    RoleQuestRuleset ruleset) noexcept {
    return 1u << static_cast<std::uint32_t>(ruleset);
}

inline constexpr std::uint32_t kSummonersRiftRulesets =
    RulesetMask(RoleQuestRuleset::Standard) |
    RulesetMask(RoleQuestRuleset::Swiftplay);
struct SpellDefinition {
    std::uint32_t idHash = 0;
    char internalId[64] = {};
    char displayName[64] = {};
    float cooldown = 0.0f;
    float castTime = 0.0f;
    float missileSpeed = 0.0f;
    float range = 0.0f;
    int maxCharges = 1;
    bool startsOnEffectEnd = false;
    bool resetOnTakedown = false;
    bool canRefund = false;
    bool channel = false;
    bool charge = false;
    bool multiStage = false;
};

struct SummonerDefinition {
    std::uint32_t idHash = 0;
    int dataKey = 0;
    char internalId[64] = {};
    char displayName[48] = {};
    float cooldown = 0.0f;
    float range = 0.0f;
    int maxCharges = 1;
    std::array<float, 3> effectValues{};
    int mapId = 11;
    std::uint32_t rulesetsMask = kSummonersRiftRulesets;
    bool disabled = false;
    Capability capability = Capability::None;
};

struct ItemDefinition {
    SDK::ItemId itemId = SDK::ItemId::Unknown;
    char internalId[64] = {};
    char displayName[64] = {};
    float cooldown = 0.0f;
    float duration = 0.0f;
    float range = 0.0f;
    std::array<float, 3> effectValues{};
    bool dataActive = false;
    bool inStore = false;
    bool purchasable = false;
    bool legacy = false;
    bool disabled = false;
    int mapId = 11;
    std::uint32_t rulesetsMask = kSummonersRiftRulesets;
    Capability capability = Capability::None;
    int Id() const noexcept { return SDK::ItemIdValue(itemId); }
};

struct BuffDefinition {
    std::uint32_t idHash = 0;
    char internalId[64] = {};
    CrowdControl control = CrowdControl::None;
    bool cleanseable = false;
    bool qssable = false;
    bool mikaelable = false;
    float danger = 0.0f;
    float minimumDuration = 0.0f;
};

struct ObjectiveDefinition {
    ObjectiveKind kind = ObjectiveKind::Unknown;
    char internalId[48] = {};
    float firstSpawn = 0.0f;
    float respawn = 0.0f;
    bool map11 = false;
    bool swiftplay = false;
    bool disabled = false;
};

struct RoleQuestDefinition {
    char role[16] = {};
    SDK::ItemId itemId = SDK::ItemId::Unknown;
    int pointsRequired = 0;
    bool enabledStandard = false;
    bool enabledSwiftplay = false;
    bool enabledRotating = false;
    char reward[96] = {};
};

class PatchRegistry final {
public:
    PatchRegistry() { LoadEmbeddedDefaults(); }
    void Clear() noexcept {
        spellCount_ = summonerCount_ = itemCount_ = buffCount_ = objectiveCount_ = questCount_ = 0;
    }
    void LoadEmbeddedDefaults() {
        Clear();
        CopyText(patch_, "16.15.1");
        AddSummoner("SummonerBarrier", 21, "Barrier", 180.0f, 1200.0f, Capability::Barrier);
        AddSummoner("SummonerBoost", 1, "Cleanse", 240.0f, 200.0f, Capability::Cleanse);
        AddSummoner("SummonerDot", 14, "Ignite", 180.0f, 600.0f, Capability::Ignite);
        AddSummoner("SummonerExhaust", 3, "Exhaust", 240.0f, 650.0f, Capability::Exhaust);
        AddSummoner("SummonerFlash", 4, "Flash", 300.0f, 425.0f, Capability::Flash);
        AddSummoner("SummonerHaste", 6, "Ghost", 240.0f, 200.0f, Capability::Ghost);
        AddSummoner("SummonerHeal", 7, "Heal", 240.0f, 875.0f, Capability::Heal);
        AddSummoner(
            "SummonerSmite", 11, "Smite", 15.0f, 500.0f,
            Capability::Smite, 2,
            std::array<float, 3>{ 600.0f, 1000.0f, 1400.0f });
        AddSummoner("SummonerTeleport", 12, "Teleport", 300.0f, 25000.0f, Capability::Teleport);

        AddItem(SDK::ItemId::Quicksilver_Sash, "QuicksilverSash", "Quicksilver Sash", 90.0f, 0.0f, 0.0f, Capability::Qss);
        AddItem(SDK::ItemId::Mercurial_Scimitar, "MercurialScimitar", "Mercurial Scimitar", 90.0f, 1.5f, 0.0f, Capability::Mercurial);
        AddItem(SDK::ItemId::Mikael_s_Blessing, "MikaelsBlessing", "Mikael's Blessing", 120.0f, 0.0f, 1000.0f, Capability::Mikael);
        AddItem(SDK::ItemId::Zhonya_s_Hourglass, "ZhonyasHourglass", "Zhonya's Hourglass", 120.0f, 2.5f, 0.0f, Capability::Zhonya);
        AddItem(SDK::ItemId::Seeker_s_Armguard, "SeekersArmguard", "Seeker's Armguard", 120.0f, 2.5f, 0.0f, Capability::Seeker);
        AddItem(SDK::ItemId::Seraph_s_Embrace, "SeraphsEmbrace", "Seraph's Embrace", 90.0f, 3.0f, 0.0f, Capability::Seraph);
        AddItem(SDK::ItemId::Locket_of_the_Iron_Solari, "LocketOfTheIronSolari", "Locket of the Iron Solari", 90.0f, 2.5f, 850.0f, Capability::Locket);
        AddItem(SDK::ItemId::Redemption, "Redemption", "Redemption", 90.0f, 0.0f, 5500.0f, Capability::Redemption, std::array<float, 3>{ 2.5f, 0.0f, 0.0f });
        AddItem(SDK::ItemId::Shurelya_s_Battlesong, "ShurelyasBattlesong", "Shurelya's Battlesong", 75.0f, 4.0f, 1000.0f, Capability::Shurelya);
        AddItem(SDK::ItemId::Youmuu_s_Ghostblade, "YoumuusGhostblade", "Youmuu's Ghostblade", 45.0f, 6.0f, 0.0f, Capability::Youmuu);
        AddItem(SDK::ItemId::Hextech_Rocketbelt, "HextechRocketbelt", "Hextech Rocketbelt", 40.0f, 0.0f, 275.0f, Capability::Rocketbelt);
        AddItem(SDK::ItemId::Stridebreaker, "Stridebreaker", "Stridebreaker", 60.0f, 3.0f, 450.0f, Capability::Stridebreaker);
        AddItem(
            SDK::ItemId::Hextech_Gunblade,
            "HextechGunblade", "Hextech Gunblade",
            40.0f, 2.0f, 700.0f,
            Capability::Gunblade,
            std::array<float, 3>{ 175.0f, 0.0f, 0.0f });
        AddItem(SDK::ItemId::Tiamat, "Tiamat", "Tiamat", 10.0f, 0.0f, 375.0f, Capability::Tiamat);
        AddItem(SDK::ItemId::Ravenous_Hydra, "RavenousHydra", "Ravenous Hydra", 10.0f, 0.0f, 400.0f, Capability::RavenousHydra);
        AddItem(SDK::ItemId::Titanic_Hydra, "TitanicHydra", "Titanic Hydra", 10.0f, 5.0f, 0.0f, Capability::TitanicHydra);
        AddItem(SDK::ItemId::Profane_Hydra, "ProfaneHydra", "Profane Hydra", 10.0f, 0.0f, 400.0f, Capability::ProfaneHydra);
        AddItem(SDK::ItemId::Randuin_s_Omen, "RanduinsOmen", "Randuin's Omen", 90.0f, 2.0f, 450.0f, Capability::Randuin);
        AddItem(SDK::ItemId::Actualizer, "Actualizer", "Actualizer", 90.0f, 8.0f, 0.0f, Capability::Actualizer);
        AddItem(SDK::ItemId::Oracle_Lens, "OracleLens", "Oracle Lens", 90.0f, 8.0f, 600.0f, Capability::Oracle);
        AddItem(SDK::ItemId::Farsight_Alteration, "FarsightAlteration", "Farsight Alteration", 5.0f, 0.0f, 4000.0f, Capability::Farsight);
        AddItem(SDK::ItemId::Stealth_Ward, "StealthWard", "Stealth Ward", 90.0f, 90.0f, 600.0f, Capability::Ward);
        AddItem(SDK::ItemId::Control_Ward, "ControlWard", "Control Ward", 0.0f, 0.0f, 600.0f, Capability::Ward);
        AddItem(SDK::ItemId::Health_Potion, "HealthPotion", "Health Potion", 0.0f, 12.0f, 0.0f, Capability::Potion);
        AddItem(SDK::ItemId::Refillable_Potion, "RefillablePotion", "Refillable Potion", 0.0f, 12.0f, 0.0f, Capability::Potion);
        AddItem(SDK::ItemId::Corrupting_Potion, "CorruptingPotion", "Corrupting Potion", 0.0f, 12.0f, 0.0f, Capability::Potion);
        AddItem(SDK::ItemId::Knight_s_Vow, "KnightsVow", "Knight's Vow", 0.0f, 0.0f, 1000.0f, Capability::KnightsVow);

        AddBuff("stun", CrowdControl::Stun, true, true, true, 8.0f, 0.25f);
        AddBuff("root", CrowdControl::Root, true, true, true, 7.0f, 0.25f);
        AddBuff("charm", CrowdControl::Charm, true, true, true, 8.0f, 0.25f);
        AddBuff("fear", CrowdControl::Fear, true, true, true, 8.0f, 0.25f);
        AddBuff("taunt", CrowdControl::Taunt, true, true, true, 8.0f, 0.25f);
        AddBuff("blind", CrowdControl::Blind, true, true, true, 4.0f, 0.25f);
        AddBuff("silence", CrowdControl::Silence, true, true, true, 6.0f, 0.25f);
        AddBuff("polymorph", CrowdControl::Polymorph, true, true, true, 9.0f, 0.25f);
        AddBuff("slow", CrowdControl::Slow, true, false, true, 2.0f, 0.50f);
        AddBuff("sleep", CrowdControl::Sleep, true, true, true, 9.0f, 0.25f);
        AddBuff("suppression", CrowdControl::Suppression, false, true, false, 10.0f, 0.20f);
        AddBuff("knockup", CrowdControl::Airborne, false, false, false, 10.0f, 0.0f);
        AddBuff("stasis", CrowdControl::Stasis, false, false, false, 5.0f, 0.0f);
        AddBuff("grounded", CrowdControl::Grounded, false, true, false, 6.0f, 0.25f);

        AddObjective(ObjectiveKind::ElementalDragon, "elemental_dragon", 300.0f, 300.0f, true, true);
        AddObjective(ObjectiveKind::DragonSoul, "dragon_soul", 0.0f, 0.0f, true, true);
        AddObjective(ObjectiveKind::ElderDragon, "elder_dragon", 0.0f, 360.0f, true, true);
        AddObjective(ObjectiveKind::VoidGrubs, "void_grubs", 300.0f, 240.0f, true, false);
        AddObjective(ObjectiveKind::RiftHerald, "rift_herald", 840.0f, 360.0f, true, false);
        AddObjective(ObjectiveKind::Baron, "baron_nashor", 1200.0f, 360.0f, true, true);
        AddObjective(ObjectiveKind::Scuttle, "rift_scuttler", 175.0f, 150.0f, true, true);

        AddQuest("Top", SDK::ItemId::Top_Lane_Quest, 1200, true, false, false, "Teleport reward or arrival shield");
        AddQuest("Jungle", SDK::ItemId::Jungle_Quest, 35, true, true, false, "Smite evolution and jungle movement reward");
        AddQuest("Mid", SDK::ItemId::Mid_Lane_Quest, 1350, true, false, false, "Tier-three boots and empowered Recall");
        AddQuest("Bot", SDK::ItemId::Bot_Lane_Quest, 0, true, false, false, "Additional inventory role slot");
        AddQuest("Support", SDK::ItemId::Support_Quest, 0, true, false, false, "Ward slot and Control Ward capacity");
    }

    const char* PatchVersion() const noexcept { return patch_; }
    void SetPatchVersion(std::string_view patch) noexcept { CopyText(patch_, patch); }
    void SetGameContext(int mapId,
                        RoleQuestRuleset ruleset) noexcept {
        activeMapId_ = mapId;
        activeRuleset_ = ruleset;
    }
    int ActiveMapId() const noexcept { return activeMapId_; }
    RoleQuestRuleset ActiveRuleset() const noexcept {
        return activeRuleset_;
    }
    bool IsAvailable(const SummonerDefinition& value) const noexcept {
        return !value.disabled &&
               value.capability != Capability::None &&
               value.mapId == activeMapId_ &&
               (value.rulesetsMask &
                RulesetMask(activeRuleset_)) != 0;
    }
    bool IsAvailable(const ItemDefinition& value) const noexcept {
        return !value.disabled && !value.legacy &&
               value.dataActive && value.inStore &&
               value.purchasable &&
               value.capability != Capability::None &&
               value.mapId == activeMapId_ &&
               (value.rulesetsMask &
                RulesetMask(activeRuleset_)) != 0;
    }

    void ApplySdkItemData(SDK::ItemId id, std::string_view displayName,
                          float cooldownMin, float cooldownMax,
                          float durationMin, float durationMax,
                          float rangeMin, float rangeMax,
                          bool active, bool inStore,
                          bool purchasable) {
        const float cooldown = std::max(cooldownMax, cooldownMin);
        const float duration = std::max(durationMax, durationMin);
        const float range = std::max(rangeMax, rangeMin);
        const ItemDefinition* existing = FindItem(id);
        const bool dataActive =
            active || (existing && existing->capability != Capability::None);

        // Hero snapshots may report the same item data dozens of times per
        // second. Avoid the linear AddReplace scan and full structure copy
        // when the generated SDK entry has not changed.
        if (existing &&
            std::fabs(existing->cooldown - cooldown) <= 0.001f &&
            std::fabs(existing->duration - duration) <= 0.001f &&
            std::fabs(existing->range - range) <= 0.001f &&
            existing->dataActive == dataActive &&
            existing->inStore == inStore &&
            existing->purchasable == purchasable &&
            (displayName.empty() ||
             std::string_view(existing->displayName) == displayName)) {
            return;
        }

        ItemDefinition value{};
        if (existing) value = *existing;
        value.itemId = id;
        if (!displayName.empty()) CopyText(value.displayName, displayName);
        value.cooldown = cooldown;
        value.duration = duration;
        value.range = range;
        // Some current active items are missing the generated Active tag.
        // A known capability remains usable when its real inventory spell
        // proves the item active; patch overrides can still disable it.
        value.dataActive = dataActive;
        value.inStore = inStore;
        value.purchasable = purchasable;
        AddItem(value);
    }

    bool AddSpell(const SpellDefinition& value) noexcept { return AddReplace(spells_, spellCount_, value, [](const auto& x) { return x.idHash; }); }
    bool AddSummoner(const SummonerDefinition& value) noexcept { return AddReplace(summoners_, summonerCount_, value, [](const auto& x) { return x.idHash; }); }
    bool AddItem(const ItemDefinition& value) noexcept { return AddReplace(items_, itemCount_, value, [](const auto& x) { return SDK::ItemIdValue(x.itemId); }); }
    bool AddBuff(const BuffDefinition& value) noexcept { return AddReplace(buffs_, buffCount_, value, [](const auto& x) { return x.idHash; }); }
    bool AddObjective(const ObjectiveDefinition& value) noexcept { return AddReplace(objectives_, objectiveCount_, value, [](const auto& x) { return static_cast<int>(x.kind); }); }
    bool AddQuest(const RoleQuestDefinition& value) noexcept { return AddReplace(quests_, questCount_, value, [](const auto& x) { return HashId(x.role); }); }

    const SpellDefinition* FindSpell(std::uint32_t hash) const noexcept { return Find(spells_, spellCount_, hash, [](const auto& x) { return x.idHash; }); }
    const SummonerDefinition* FindSummoner(std::uint32_t hash) const noexcept { return Find(summoners_, summonerCount_, hash, [](const auto& x) { return x.idHash; }); }
    const SummonerDefinition* ResolveSummoner(std::uint32_t hash,
                                              std::string_view observedName) const noexcept {
        if (const SummonerDefinition* exact = FindSummoner(hash)) return exact;
        for (std::size_t i = 0; i < summonerCount_; ++i) {
            if (ContainsInsensitive(observedName, summoners_[i].internalId) ||
                ContainsInsensitive(observedName, summoners_[i].displayName)) {
                return &summoners_[i];
            }
        }
        return nullptr;
    }
    const SummonerDefinition* ResolveAvailableSummoner(
        std::uint32_t hash,
        std::string_view observedName) const noexcept {
        const SummonerDefinition* value =
            ResolveSummoner(hash, observedName);
        return value && IsAvailable(*value) ? value : nullptr;
    }
    const ItemDefinition* FindItem(SDK::ItemId id) const noexcept { return FindItem(SDK::ItemIdValue(id)); }
    const ItemDefinition* FindItem(int id) const noexcept {
        for (std::size_t i = 0; i < itemCount_; ++i) if (items_[i].Id() == id) return &items_[i];
        return nullptr;
    }
    const ItemDefinition* FindAvailableItem(
        SDK::ItemId id) const noexcept {
        const ItemDefinition* value = FindItem(id);
        return value && IsAvailable(*value) ? value : nullptr;
    }
    const ItemDefinition* FindAvailableItem(
        Capability capability) const noexcept {
        for (std::size_t i = 0; i < itemCount_; ++i) {
            if (items_[i].capability == capability &&
                IsAvailable(items_[i])) {
                return &items_[i];
            }
        }
        return nullptr;
    }
    const SummonerDefinition* FindAvailableSummoner(
        Capability capability) const noexcept {
        for (std::size_t i = 0; i < summonerCount_; ++i) {
            if (summoners_[i].capability == capability &&
                IsAvailable(summoners_[i])) {
                return &summoners_[i];
            }
        }
        return nullptr;
    }
    const BuffDefinition* FindBuff(std::uint32_t hash) const noexcept { return Find(buffs_, buffCount_, hash, [](const auto& x) { return x.idHash; }); }
    const BuffDefinition* ResolveBuff(std::uint32_t hash,
                                      std::string_view observedName) const noexcept {
        if (const BuffDefinition* exact = FindBuff(hash)) return exact;
        for (std::size_t i = 0; i < buffCount_; ++i) {
            if (ContainsInsensitive(observedName, buffs_[i].internalId)) return &buffs_[i];
        }
        return nullptr;
    }
    const ObjectiveDefinition* FindObjective(ObjectiveKind kind) const noexcept { return Find(objectives_, objectiveCount_, static_cast<int>(kind), [](const auto& x) { return static_cast<int>(x.kind); }); }
    const RoleQuestDefinition* FindQuest(std::string_view role) const noexcept { return Find(quests_, questCount_, HashId(role), [](const auto& x) { return HashId(x.role); }); }

    template <typename Fn> void ForEachItem(Fn&& fn) const { for (std::size_t i = 0; i < itemCount_; ++i) fn(items_[i]); }
    template <typename Fn> void ForEachSummoner(Fn&& fn) const { for (std::size_t i = 0; i < summonerCount_; ++i) fn(summoners_[i]); }
    template <typename Fn> void ForEachObjective(Fn&& fn) const { for (std::size_t i = 0; i < objectiveCount_; ++i) fn(objectives_[i]); }

    void AddSummoner(std::string_view id, int key,
                     std::string_view name, float cooldown,
                     float range, Capability capability,
                     int maxCharges = 1,
                     std::array<float, 3> effectValues = {},
                     int mapId = 11,
                     std::uint32_t rulesetsMask =
                         kSummonersRiftRulesets) {
        SummonerDefinition value{};
        value.idHash = HashId(id);
        value.dataKey = key;
        CopyText(value.internalId, id);
        CopyText(value.displayName, name);
        value.cooldown = cooldown;
        value.range = range;
        value.maxCharges = maxCharges;
        value.mapId = mapId;
        value.rulesetsMask = rulesetsMask;
        value.effectValues = effectValues;
        value.capability = capability;
        AddSummoner(value);
    }
    void AddItem(
        SDK::ItemId id, std::string_view internalId,
        std::string_view name, float cooldown,
        float duration, float range,
        Capability capability,
        std::array<float, 3> effectValues = {},
        int mapId = 11,
        std::uint32_t rulesetsMask =
            kSummonersRiftRulesets) {
        ItemDefinition value{};
        value.itemId = id;
        CopyText(value.internalId, internalId);
        CopyText(value.displayName, name);
        value.cooldown = cooldown;
        value.duration = duration;
        value.range = range;
        value.effectValues = effectValues;
        value.mapId = mapId;
        value.rulesetsMask = rulesetsMask;
        value.capability = capability;
        value.inStore = true;
        value.purchasable = true;
        value.dataActive = true;
        AddItem(value);
    }
    void AddBuff(std::string_view id, CrowdControl control, bool cleanseable, bool qssable,
                 bool mikaelable, float danger, float minimumDuration) {
        BuffDefinition value{}; value.idHash = HashId(id); CopyText(value.internalId, id);
        value.control = control; value.cleanseable = cleanseable; value.qssable = qssable;
        value.mikaelable = mikaelable; value.danger = danger; value.minimumDuration = minimumDuration;
        AddBuff(value);
    }
    void AddObjective(ObjectiveKind kind, std::string_view id, float firstSpawn, float respawn,
                      bool map11, bool swiftplay, bool disabled = false) {
        ObjectiveDefinition value{}; value.kind = kind; CopyText(value.internalId, id);
        value.firstSpawn = firstSpawn; value.respawn = respawn; value.map11 = map11;
        value.swiftplay = swiftplay; value.disabled = disabled; AddObjective(value);
    }
    void AddQuest(std::string_view role, SDK::ItemId itemId, int points, bool standard,
                  bool swiftplay, bool rotating, std::string_view reward) {
        RoleQuestDefinition value{}; CopyText(value.role, role); value.itemId = itemId;
        value.pointsRequired = points; value.enabledStandard = standard; value.enabledSwiftplay = swiftplay;
        value.enabledRotating = rotating; CopyText(value.reward, reward); AddQuest(value);
    }
private:
    static bool ContainsInsensitive(std::string_view value,
                                    std::string_view needle) noexcept {
        if (needle.empty() || value.size() < needle.size()) return false;
        for (std::size_t offset = 0; offset + needle.size() <= value.size(); ++offset) {
            bool matches = true;
            for (std::size_t i = 0; i < needle.size(); ++i) {
                const auto left = static_cast<unsigned char>(value[offset + i]);
                const auto right = static_cast<unsigned char>(needle[i]);
                if (std::tolower(left) != std::tolower(right)) {
                    matches = false;
                    break;
                }
            }
            if (matches) return true;
        }
        return false;
    }
    template <typename T, std::size_t N, typename KeyFn>
    static bool AddReplace(std::array<T, N>& values, std::size_t& count,
                           const T& value, KeyFn keyFn) noexcept {
        const auto key = keyFn(value);
        for (std::size_t i = 0; i < count; ++i) {
            if (keyFn(values[i]) == key) { values[i] = value; return true; }
        }
        if (count >= N) return false;
        values[count++] = value; return true;
    }
    template <typename T, std::size_t N, typename KeyFn, typename Key>
    static const T* Find(const std::array<T, N>& values, std::size_t count,
                         Key key, KeyFn keyFn) noexcept {
        for (std::size_t i = 0; i < count; ++i) if (keyFn(values[i]) == key) return &values[i];
        return nullptr;
    }
    char patch_[16] = {};
    std::array<SpellDefinition, 256> spells_{};
    std::array<SummonerDefinition, 64> summoners_{};
    std::array<ItemDefinition, 128> items_{};
    std::array<BuffDefinition, 128> buffs_{};
    std::array<ObjectiveDefinition, 32> objectives_{};
    std::array<RoleQuestDefinition, 16> quests_{};
    std::size_t spellCount_ = 0, summonerCount_ = 0, itemCount_ = 0, buffCount_ = 0;
    std::size_t objectiveCount_ = 0, questCount_ = 0;
    int activeMapId_ = 11;
    RoleQuestRuleset activeRuleset_ =
        RoleQuestRuleset::Standard;
};

struct ObservedItem {
    SDK::ItemId itemId = SDK::ItemId::Unknown;
    int slot = -1;
    int charges = 0;
    int maxCharges = 0;
    float cooldownRemaining = 0.0f;
    bool usable = false;
    bool active = false;
    Capability capability = Capability::None;
    char name[64] = {};
    Evidence evidence{};
    int Id() const noexcept { return SDK::ItemIdValue(itemId); }
};

struct ObservedBuff {
    std::uint32_t idHash = 0;
    int stacks = 0;
    int type = -1;
    float startTime = 0.0f;
    float endTime = 0.0f;
    char name[64] = {};
    Evidence evidence{};
};

struct ObservedSpell {
    int slot = -1;
    std::uint32_t idHash = 0;
    int charges = 0;
    int maxCharges = 1;
    float cooldownRemaining = 0.0f;
    float cooldownMin = 0.0f;
    float cooldownMax = 0.0f;
    float rechargeDuration = 0.0f;
    float manaCost = 0.0f;
    float lastCastAt = 0.0f;
    CooldownKind cooldownKind = CooldownKind::Unknown;
    bool ready = false;
    bool channeling = false;
    bool charging = false;
    bool stolen = false;
    bool resetOnTakedown = false;
    SpellOrigin origin = SpellOrigin::Native;
    std::uint32_t previousIdHash = 0;
    float rechargeReadyAt = 0.0f;
    bool resetObserved = false;
    Capability capability = Capability::None;
    char name[64] = {};
    Evidence evidence{};
};

struct RoleQuestState {
    char role[16] = {};
    SDK::ItemId itemId = SDK::ItemId::Unknown;
    int progress = -1;
    int required = 0;
    bool completed = false;
    bool rewardObserved = false;
    char reward[96] = {};
    Evidence evidence{};
};

struct ChampionState {
    std::uint32_t networkId = 0, team = 0;
    bool enemy = false, ally = false, local = false;
    bool visible = false, dead = false;
    bool targetable = true, invulnerable = false;
    bool clone = false, possession = false, recalling = false;
    bool teleporting = false, channeling = false;
    CloneKind cloneKind = CloneKind::None;
    bool cloneExpired = false;
    float cloneExpiresAt = 0.0f;
    std::uint32_t identityGeneration = 0;
    float lastObservedAt = 0.0f;
    char baseChampionId[64] = {}, activeFormId[64] = {};
    std::uint64_t formSignature = 0;
    std::uint32_t formGeneration = 0;
    float formChangedAt = 0.0f, possessionStartedAt = 0.0f;
    StasisSource stasisSource = StasisSource::None;
    ReviveSource reviveSource = ReviveSource::None;
    bool inStasis = false, reviveAvailable = false, reviveInProgress = false;
    float stasisEndsAt = 0.0f, reviveAt = 0.0f;
    bool roleQuestSpellUpgrade = false, bonusInventorySlot = false;
    bool supportWardSlot = false;
    float roleQuestUpgradeAt = 0.0f;
    Evidence specialStateEvidence{};
    float visibilityAge = 0.0f, lastSeenAt = 0.0f, moveSpeed = 0.0f, abilityHaste = 0.0f;
    Point3 position{}, lastSeenPosition{}, lastDirection{};
    // Recent cast/missile position used for event association. The renderer
    // uses the enemy's live memory position while the short evidence window
    // is active, instead of drawing this cast point.
    Point3 observedEventPosition{};
    float observedEventAt = 0.0f, observedEventUntil = 0.0f;
    Evidence observedEventEvidence{};
    // Path destination is observed event data, not the champion's last seen
    // position. Keeping these separate prevents a movement command from
    // teleporting the last-seen marker to the end of the path.
    Point3 pathStartPosition{}, pathTargetPosition{};
    float pathUpdatedAt = 0.0f, pathExpectedArrivalAt = 0.0f;
    int pathNodeCount = 0;
    Evidence pathEvidence{};
    float lastSeenSpeed = 0.0f, reachableRadius = 0.0f;
    float neutralMinionsKilled = 0.0f;
    ProvenanceValue<float> currentGold{}, totalGold{};
    float estimatedGoldMin = 0.0f, estimatedGoldMax = 0.0f;
    float lastGoldObservedAt = 0.0f, lastPurchaseAt = 0.0f;
    SDK::ItemId lastPurchasedItem = SDK::ItemId::Unknown;
    int purchaseCount = 0;
    float lastActivitySampleAt = -100.0f;
    Evidence goldEstimateEvidence{};
    int pathBranches = 0;
    ProvenanceValue<float> health{}, maxHealth{}, mana{}, maxMana{};
    float allShield = 0.0f, healthRegen = 0.0f;
    ProvenanceValue<int> level{};
    char name[64] = {}, championId[64] = {};
    std::array<ObservedItem, 8> items{}; std::size_t itemCount = 0;
    std::array<ObservedSpell, 8> spells{}; std::size_t spellCount = 0;
    std::array<ObservedBuff, 32> buffs{}; std::size_t buffCount = 0;
    RoleQuestState roleQuest{};
    std::uint64_t inventorySignature = 0, summonerSignature = 0;
    float deathAt = 0.0f, respawnAt = 0.0f;
};

struct WardState {
    std::uint32_t networkId = 0, ownerId = 0, team = 0;
    WardKind kind = WardKind::Unknown;
    Point3 position{};
    float placedAt = 0.0f, expiresAt = 0.0f, destroyedAt = 0.0f,
          radius = 0.0f, bonusVisionUntil = 0.0f;
    bool visible = false, destroyed = false, faelight = false;
    bool bonusVisionObserved = false, oracleActive = false;
    bool enemy = false, ally = false;
    Evidence evidence{};
};

struct ObjectiveState {
    ObjectiveKind kind = ObjectiveKind::Unknown;
    ObjectiveStatus status = ObjectiveStatus::NotSpawned;
    std::uint32_t networkId = 0;
    Point3 position{};
    float health = 0.0f, maxHealth = 0.0f, spawnAt = 0.0f, deathAt = 0.0f, respawnAt = 0.0f;
    bool visible = false, inCombat = false, soulClaimed = false;
    float combatObservedAt = 0.0f, combatObservedUntil = 0.0f;
    Evidence evidence{};
};

struct JungleCampState {
    std::uint32_t networkId = 0, campKey = 0;
    char campId[48] = {};
    Point3 position{};
    float lastSeenAliveAt = 0.0f, confirmedDeathAt = 0.0f;
    float estimatedDeathAt = 0.0f, respawnAt = 0.0f;
    std::uint32_t sourceJunglerId = 0;
    bool visible = false, alive = false, observedDeath = false;
    Confidence confidence = Confidence::Unknown;
    Evidence evidence{};
};


struct ThreatState {
    std::uint32_t id = 0, sourceId = 0, targetId = 0, spellHash = 0;
    ThreatGeometry geometry = ThreatGeometry::Point;
    Point3 start{}, end{};
    float radius = 0.0f, damage = 0.0f, startAt = 0.0f, endAt = 0.0f, impactAt = 0.0f;
    CrowdControl control = CrowdControl::None;
    bool damageTypeTrue = false, blockable = false, cleanseable = false, dodgeable = true, visible = false;
    Evidence evidence{};
};

struct WaveState {
    int allyMinions = 0, enemyMinions = 0;
    float allyHealth = 0.0f, enemyHealth = 0.0f;
    float allyFront = 0.0f, enemyFront = 0.0f;
    float laneBias = 0.0f, observedAt = 0.0f;
    Point3 center{};
    char classification[32] = "Unknown";
    Evidence evidence{};
};

struct DamageRecord {
    float at = 0.0f;
    std::uint32_t sourceId = 0, targetId = 0;
    float amount = 0.0f;
    char source[64] = {};
    Evidence evidence{};
};

struct AlertState {
    std::uint32_t id = 0, source = 0;
    int priority = 0;
    float at = 0.0f, expiresAt = 0.0f, cooldownUntil = 0.0f;
    Confidence confidence = Confidence::Unknown;
    char title[64] = {}, detail[160] = {};
};

struct ChampionObservation {
    std::uint32_t networkId = 0, team = 0;
    bool enemy = false, ally = false, local = false;
    bool visible = false, dead = false;
    bool targetable = true, invulnerable = false;
    bool clone = false, possession = false, recalling = false;
    bool teleporting = false, channeling = false;
    float neutralMinionsKilled = 0.0f;
    float currentGold = 0.0f, totalGold = 0.0f;
    float moveSpeed = 0.0f, abilityHaste = 0.0f;
    Point3 position{}, direction{}, pathEnd{};
    int pathBranches = 0;
    float health = 0.0f, maxHealth = 0.0f, mana = 0.0f, maxMana = 0.0f;
    float allShield = 0.0f, healthRegen = 0.0f;
    int level = 0;
    char name[64] = {}, championId[64] = {};
    std::array<ObservedItem, 8> items{}; std::size_t itemCount = 0;
    std::array<ObservedSpell, 8> spells{}; std::size_t spellCount = 0;
    std::array<ObservedBuff, 32> buffs{}; std::size_t buffCount = 0;
    RoleQuestState roleQuest{};
};


class RoleQuestTracker final {
public:
    static bool IsEnabled(const RoleQuestDefinition& definition,
                          RoleQuestRuleset ruleset) noexcept {
        switch (ruleset) {
        case RoleQuestRuleset::Standard:
            return definition.enabledStandard;
        case RoleQuestRuleset::Swiftplay:
            return definition.enabledSwiftplay;
        case RoleQuestRuleset::Rotating:
            return definition.enabledRotating;
        default:
            return false;
        }
    }

    static RoleQuestState Resolve(
        const PatchRegistry& registry,
        const std::array<ObservedItem, 8>& items,
        std::size_t itemCount,
        RoleQuestRuleset ruleset,
        const RoleQuestState* previous = nullptr) noexcept {
        RoleQuestState result{};
        static constexpr std::array<const char*, 5> kRoles = {
            "Top", "Jungle", "Mid", "Bot", "Support"
        };
        const std::size_t count = std::min(itemCount, items.size());
        for (const char* role : kRoles) {
            const RoleQuestDefinition* definition =
                registry.FindQuest(role);
            if (!definition || !IsEnabled(*definition, ruleset)) continue;
            for (std::size_t i = 0; i < count; ++i) {
                if (items[i].itemId != definition->itemId) continue;
                FillDefinition(result, *definition);
                result.itemId = items[i].itemId;
                result.progress = std::max(0, items[i].charges);
                result.completed =
                    definition->pointsRequired > 0 &&
                    result.progress >= definition->pointsRequired;
                if (previous && previous->completed) {
                    result.completed = true;
                    result.rewardObserved = previous->rewardObserved;
                }
                return result;
            }
        }

        for (std::size_t i = 0; i < count; ++i) {
            const char* role = RewardRole(items[i].itemId);
            if (!role) continue;
            const RoleQuestDefinition* definition =
                registry.FindQuest(role);
            if (!definition || !IsEnabled(*definition, ruleset)) continue;
            FillDefinition(result, *definition);
            result.itemId = items[i].itemId;
            result.progress = definition->pointsRequired;
            result.completed = true;
            result.rewardObserved = true;
            return result;
        }

        if (previous && previous->role[0]) {
            const RoleQuestDefinition* definition =
                registry.FindQuest(previous->role);
            if (definition && IsEnabled(*definition, ruleset)) {
                return *previous;
            }
        }
        return result;
    }

private:
    static void FillDefinition(
        RoleQuestState& state,
        const RoleQuestDefinition& definition) noexcept {
        CopyText(state.role, definition.role);
        state.itemId = definition.itemId;
        state.required = definition.pointsRequired;
        CopyText(state.reward, definition.reward);
    }

    static const char* RewardRole(SDK::ItemId itemId) noexcept {
        switch (itemId) {
        case SDK::ItemId::Unleashed_Teleport_Top_Lane_Quest_Reward:
        case SDK::ItemId::Top_Lane_Quest_Reward:
        case SDK::ItemId::Top_Lane_Quest_Id1222:
            return "Top";
        case SDK::ItemId::Jungle_Quest_Reward:
        case SDK::ItemId::Jungle_Quest_Reward_Id1209:
        case SDK::ItemId::Jungle_Quest_Reward_Id1210:
        case SDK::ItemId::Jungle_Quest_Reward_Id1211:
            return "Jungle";
        case SDK::ItemId::Mid_Lane_Quest_Reward:
            return "Mid";
        case SDK::ItemId::Bot_Lane_Quest_Reward:
            return "Bot";
        case SDK::ItemId::Support_Quest_Reward:
            return "Support";
        default:
            return nullptr;
        }
    }
};

class StateStore final {
public:
    StateStore() {
        champions_.reserve(32);
    }
    void Reset() {
        champions_.clear(); wards_.Clear(); objectives_.Clear(); jungles_.Clear();
        threats_.Clear(); alerts_.Clear(); damage_.Clear(); teamfight_.Clear(); wave_ = {};
        ++sessionGeneration_;
    }
    ChampionState& GetOrCreateChampion(std::uint32_t networkId) { return champions_[networkId]; }
    ChampionState* FindChampion(std::uint32_t networkId) noexcept {
        const auto it = champions_.find(networkId); return it == champions_.end() ? nullptr : &it->second;
    }
    const ChampionState* FindChampion(std::uint32_t networkId) const noexcept {
        const auto it = champions_.find(networkId); return it == champions_.end() ? nullptr : &it->second;
    }
    bool RekeyChampion(std::uint32_t oldNetworkId,
                       std::uint32_t newNetworkId) {
        if (oldNetworkId == 0 || newNetworkId == 0 ||
            oldNetworkId == newNetworkId ||
            champions_.contains(newNetworkId)) {
            return false;
        }
        auto node = champions_.extract(oldNetworkId);
        if (node.empty()) return false;
        node.key() = newNetworkId;
        node.mapped().networkId = newNetworkId;
        ++node.mapped().identityGeneration;
        champions_.insert(std::move(node));
        return true;
    }
    bool EraseChampion(std::uint32_t networkId) noexcept {
        return champions_.erase(networkId) != 0;
    }
    std::size_t PruneInvalid(float now) noexcept {
        std::size_t removed = 0;
        for (auto it = champions_.begin(); it != champions_.end();) {
            const ChampionState& champion = it->second;
            const bool invalid =
                champion.networkId == 0 ||
                !champion.position.IsValid() ||
                !champion.lastSeenPosition.IsValid() ||
                (!champion.health.evidence.IsKnown() &&
                 champion.lastObservedAt <= 0.0f);
            if (invalid) {
                it = champions_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        removed += threats_.RemoveIf([now](const ThreatState& threat) {
            return threat.id == 0 ||
                   !threat.start.IsValid() ||
                   !threat.end.IsValid() ||
                   (!threat.visible &&
                    (threat.endAt <= 0.0f || threat.endAt <= now)) ||
                   threat.evidence.IsExpired(now);
        });
        removed += wards_.RemoveIf([now](const WardState& ward) {
            return ward.networkId == 0 ||
                   !ward.position.IsValid() ||
                   (ward.destroyed && ward.destroyedAt > 0.0f &&
                    now - ward.destroyedAt > 8.0f);
        });
        removed += jungles_.RemoveIf([](const JungleCampState& camp) {
            return camp.campKey == 0 &&
                   camp.networkId == 0 &&
                   camp.campId[0] == '\0';
        });
        removed += alerts_.RemoveIf([now](const AlertState& alert) {
            return alert.id == 0 ||
                   (alert.priority == 0 &&
                    alert.expiresAt > 0.0f &&
                    alert.expiresAt <= now);
        });
        return removed;
    }
    template <typename Fn> void ForEachChampion(Fn&& fn) { for (auto& pair : champions_) fn(pair.second); }
    template <typename Fn> void ForEachChampion(Fn&& fn) const { for (const auto& pair : champions_) fn(pair.second); }
    std::size_t ChampionCount() const noexcept { return champions_.size(); }
    std::uint64_t SessionGeneration() const noexcept { return sessionGeneration_; }
    FixedRing<WardState, 128>& Wards() noexcept { return wards_; }
    const FixedRing<WardState, 128>& Wards() const noexcept { return wards_; }
    FixedRing<ObjectiveState, 16>& Objectives() noexcept { return objectives_; }
    const FixedRing<ObjectiveState, 16>& Objectives() const noexcept { return objectives_; }
    FixedRing<JungleCampState, 64>& Jungles() noexcept { return jungles_; }
    const FixedRing<JungleCampState, 64>& Jungles() const noexcept { return jungles_; }
    FixedRing<ThreatState, 128>& Threats() noexcept { return threats_; }
    const FixedRing<ThreatState, 128>& Threats() const noexcept { return threats_; }
    FixedRing<AlertState, 64>& Alerts() noexcept { return alerts_; }
    const FixedRing<AlertState, 64>& Alerts() const noexcept { return alerts_; }
    FixedRing<DamageRecord, 128>& Damage() noexcept { return damage_; }
    const FixedRing<DamageRecord, 128>& Damage() const noexcept { return damage_; }
    FixedRing<GameEvent, 512>& TeamfightTimeline() noexcept { return teamfight_; }
    const FixedRing<GameEvent, 512>& TeamfightTimeline() const noexcept { return teamfight_; }
    WaveState& Wave() noexcept { return wave_; }
    const WaveState& Wave() const noexcept { return wave_; }
    void SetMode(RuntimeMode mode) noexcept { mode_ = mode; }
    RuntimeMode Mode() const noexcept { return mode_; }
    void SetRuleset(RoleQuestRuleset ruleset) noexcept {
        ruleset_ = ruleset;
    }
    RoleQuestRuleset Ruleset() const noexcept { return ruleset_; }
    void SetMapId(int mapId) noexcept { mapId_ = mapId; }
    int MapId() const noexcept { return mapId_; }
    void SetNow(float now) noexcept { now_ = now; }
    float Now() const noexcept { return now_; }
private:
    std::unordered_map<std::uint32_t, ChampionState> champions_{};
    FixedRing<WardState, 128> wards_{};
    FixedRing<ObjectiveState, 16> objectives_{};
    FixedRing<JungleCampState, 64> jungles_{};
    FixedRing<ThreatState, 128> threats_{};
    FixedRing<AlertState, 64> alerts_{};
    FixedRing<DamageRecord, 128> damage_{};
    FixedRing<GameEvent, 512> teamfight_{};
    WaveState wave_{};
    RuntimeMode mode_ = RuntimeMode::Companion;
    RoleQuestRuleset ruleset_ = RoleQuestRuleset::Standard;
    int mapId_ = 11;
    float now_ = 0.0f;
    std::uint64_t sessionGeneration_ = 1;
};

struct ThreatForecast {
    float incomingDamage = 0.0f;
    float incomingTrueDamage = 0.0f;
    float dodgeableDamage = 0.0f;
    float blockableDamage = 0.0f;
    float damageOverTime = 0.0f;
    float primaryDamage = 0.0f;
    float firstImpactAt = 0.0f;
    float lastImpactAt = 0.0f;
    std::uint32_t primarySource = 0;
    int threatCount = 0;
    bool hardCc = false;
    bool primaryTrueDamage = false;
    bool primaryDodgeable = false;
    Confidence confidence = Confidence::Unknown;
};

class CombatPredictionService final {
public:
    static bool Contains(const ThreatState& threat,
                         const Point3& point) noexcept {
        const float radius = std::max(50.0f, threat.radius);
        if (threat.geometry == ThreatGeometry::Circle ||
            threat.geometry == ThreatGeometry::Point) {
            return threat.end.DistanceSquared(point) <= radius * radius;
        }
        const Point3 segment = threat.end - threat.start;
        const Point3 relative = point - threat.start;
        const float lengthSquared =
            segment.x * segment.x + segment.z * segment.z;
        if (lengthSquared <= 1.0f) {
            return threat.start.DistanceSquared(point) <= radius * radius;
        }
        const float projection =
            (relative.x * segment.x + relative.z * segment.z) /
            lengthSquared;
        const float t = std::clamp(projection, 0.0f, 1.0f);
        const Point3 closest = threat.start + segment * t;
        return closest.DistanceSquared(point) <= radius * radius;
    }

    static bool IsObservedActionable(const ThreatState& threat,
                                     float now,
                                     float horizon) noexcept {
        if (!threat.visible || !threat.evidence.IsKnown() ||
            threat.evidence.IsExpired(now) ||
            threat.evidence.provenance == Provenance::Estimated) {
            return false;
        }
        return threat.impactAt >= now - 0.05f &&
               threat.impactAt <= now + std::max(0.0f, horizon);
    }

    static ThreatForecast Forecast(const ChampionState& target,
                                   const StateStore& store,
                                   float now,
                                   float horizon) noexcept {
        ThreatForecast forecast{};
        float highestDamage = 0.0f;
        for (std::size_t i = 0; i < store.Threats().Size(); ++i) {
            const ThreatState& threat = store.Threats().At(i);
            if (!IsObservedActionable(threat, now, horizon)) continue;
            if (threat.targetId != 0 &&
                threat.targetId != target.networkId) {
                continue;
            }
            if (threat.targetId == 0 &&
                !Contains(threat, target.position)) {
                continue;
            }

            const float damage = std::max(0.0f, threat.damage);
            forecast.incomingDamage += damage;
            if (threat.damageTypeTrue) {
                forecast.incomingTrueDamage += damage;
            }
            if (threat.dodgeable) {
                forecast.dodgeableDamage += damage;
            }
            if (threat.blockable) {
                forecast.blockableDamage += damage;
            }
            if (threat.endAt - threat.impactAt > 0.25f) {
                forecast.damageOverTime += damage;
            }
            forecast.firstImpactAt = forecast.threatCount == 0
                ? threat.impactAt
                : std::min(forecast.firstImpactAt, threat.impactAt);
            forecast.lastImpactAt =
                std::max(forecast.lastImpactAt, threat.impactAt);
            forecast.hardCc = forecast.hardCc ||
                threat.control == CrowdControl::Stun ||
                threat.control == CrowdControl::Root ||
                threat.control == CrowdControl::Charm ||
                threat.control == CrowdControl::Fear ||
                threat.control == CrowdControl::Taunt ||
                threat.control == CrowdControl::Polymorph ||
                threat.control == CrowdControl::Sleep ||
                threat.control == CrowdControl::Suppression ||
                threat.control == CrowdControl::Airborne;
            forecast.confidence = forecast.threatCount == 0
                ? threat.evidence.confidence
                : static_cast<Confidence>(std::min(
                    static_cast<int>(forecast.confidence),
                    static_cast<int>(threat.evidence.confidence)));
            ++forecast.threatCount;
            if (damage >= highestDamage) {
                highestDamage = damage;
                forecast.primaryDamage = damage;
                forecast.primarySource = threat.sourceId;
                forecast.primaryTrueDamage =
                    threat.damageTypeTrue;
                forecast.primaryDodgeable =
                    threat.dodgeable;
            }
        }
        return forecast;
    }
};

class CombatValidationService final {
public:
    static bool IsVisibleCombatant(const ChampionState& state) noexcept {
        return state.networkId != 0 && state.visible && !state.dead &&
               state.health.evidence.provenance == Provenance::VisibleNow &&
               state.health.value > 0.0f;
    }

    static bool IsTargetableEnemy(const ChampionState& state,
                                  std::uint32_t localTeam) noexcept {
        return IsVisibleCombatant(state) && state.enemy &&
               state.team != 0 && state.team != localTeam;
    }

    static bool IsTargetableAlly(const ChampionState& state,
                                 std::uint32_t localTeam) noexcept {
        return IsVisibleCombatant(state) && state.ally &&
               state.team == localTeam;
    }
};


struct DecisionLogEntry {
    float at = 0.0f;
    int priority = 0;
    Confidence confidence = Confidence::Unknown;
    char subject[48] = {}, outcome[24] = {}, reason[192] = {};
};

class DecisionLog final {
public:
    void Add(float at, std::string_view subject, std::string_view outcome,
             std::string_view reason, int priority = 0,
             Confidence confidence = Confidence::Unknown) noexcept {
        DecisionLogEntry entry{}; entry.at = at; entry.priority = priority; entry.confidence = confidence;
        CopyText(entry.subject, subject); CopyText(entry.outcome, outcome); CopyText(entry.reason, reason);
        entries_.Push(entry);
    }
    const FixedRing<DecisionLogEntry, 512>& Entries() const noexcept { return entries_; }
    void Clear() noexcept { entries_.Clear(); }
    bool ExportJsonl(const char* path) const {
        if (!path || !path[0]) return false;
        std::ofstream output(path, std::ios::out | std::ios::trunc);
        if (!output) return false;
        entries_.ForEach([&](const DecisionLogEntry& entry) {
            output << "{\"at\":" << entry.at << ",\"priority\":" << entry.priority
                   << ",\"confidence\":\"" << ConfidenceName(entry.confidence)
                   << "\",\"subject\":\""; WriteEscaped(output, entry.subject);
            output << "\",\"outcome\":\""; WriteEscaped(output, entry.outcome);
            output << "\",\"reason\":\""; WriteEscaped(output, entry.reason); output << "\"}\n";
        });
        return output.good();
    }
private:
    static void WriteEscaped(std::ofstream& output, const char* value) {
        for (const char* cursor = value ? value : ""; *cursor; ++cursor) {
            switch (*cursor) { case '\\': output << "\\\\"; break; case '"': output << "\\\""; break;
            case '\n': output << "\\n"; break; case '\r': output << "\\r"; break; default: output << *cursor; break; }
        }
    }
    FixedRing<DecisionLogEntry, 512> entries_{};
};

enum class ActionMode : std::uint8_t { Off = 0, Suggest, Confirm, Auto };
enum class ActionPriority : int { PreventDeath = 1, BreakHardCc = 2, SaveAlly = 3, Disengage = 4, SecureObjective = 5, Execute = 6, Engage = 7, Mobility = 8, WaveClear = 9, Utility = 10 };
constexpr int ActionPriorityRank(
    ActionPriority priority) noexcept {
    switch (priority) {
    case ActionPriority::PreventDeath: return 1;
    case ActionPriority::BreakHardCc: return 2;
    case ActionPriority::SaveAlly: return 3;
    case ActionPriority::Disengage: return 4;
    case ActionPriority::SecureObjective: return 5;
    case ActionPriority::Execute: return 6;
    case ActionPriority::Engage: return 7;
    case ActionPriority::Mobility: return 8;
    case ActionPriority::WaveClear: return 9;
    case ActionPriority::Utility: return 10;
    }
    return 100;
}
enum ActionResource : std::uint32_t { ResourceNone = 0, ResourceCleanse = 1u << 0, ResourceProtection = 1u << 1, ResourceMobility = 1u << 2, ResourceExecute = 1u << 3, ResourceObjective = 1u << 4, ResourceWave = 1u << 5, ResourceVision = 1u << 6, ResourceUtility = 1u << 7 };

inline std::uint32_t ResourceFor(Capability capability) noexcept {
    switch (capability) {
    case Capability::Cleanse: case Capability::Qss: case Capability::Mercurial: case Capability::Mikael: return ResourceCleanse;
    case Capability::Barrier: case Capability::Heal: case Capability::Zhonya:
    case Capability::Seeker: case Capability::Seraph: case Capability::Locket:
    case Capability::Redemption: return ResourceProtection;
    case Capability::Flash: case Capability::Ghost: case Capability::Teleport: case Capability::Shurelya: case Capability::Youmuu: case Capability::Rocketbelt: return ResourceMobility;
    case Capability::Ignite: case Capability::Gunblade: case Capability::ProfaneHydra: return ResourceExecute;
    case Capability::Smite: return ResourceObjective;
    case Capability::Tiamat: case Capability::RavenousHydra: case Capability::TitanicHydra: case Capability::Stridebreaker: return ResourceWave;
    case Capability::Oracle: case Capability::Farsight: case Capability::Ward: return ResourceVision;
    default: return ResourceUtility;
    }
}
inline std::uint32_t ConflictFor(Capability capability) noexcept {
    const std::uint32_t resource = ResourceFor(capability);
    switch (capability) {
    case Capability::Flash:
    case Capability::Zhonya:
    case Capability::Seeker:
        return resource | ResourceMobility | ResourceProtection;
    default:
        return resource;
    }
}

struct ActionRequest {
    Capability capability = Capability::None;
    ActionMode mode = ActionMode::Off;
    ActionPriority priority = ActionPriority::Utility;
    std::uint32_t resourceMask = ResourceNone;
    std::uint32_t targetId = 0;
    SDK::ItemId itemId = SDK::ItemId::Unknown;
    int spellSlot = -1;
    int itemSlot = -1;
    Point3 position{};
    float expectedValue = 0.0f, createdAt = 0.0f, expiresAt = 0.0f, earliestAt = 0.0f;
    Confidence confidence = Confidence::Unknown;
    std::uint32_t conflictMask = ResourceNone;
    char reason[192] = {}, sourceModule[48] = {};
    bool IsExpired(float now) const noexcept {
        return expiresAt > 0.0f && now >= expiresAt;
    }
    bool IsReady(float now) const noexcept {
        return !IsExpired(now) && earliestAt <= now;
    }
    bool IsExplainable() const noexcept {
        return reason[0] != '\0' &&
               sourceModule[0] != '\0' &&
               confidence != Confidence::Unknown;
    }
};

struct SafetyPolicy {
    bool allowAutomaticPractice = false, doNotInterruptRecall = true, doNotUseWhileTyping = true;
    bool doNotUseInShop = true, lethalOnly = false, includeDamageOverTime = true, reserveObjectiveCharge = true;
    int minimumCcDurationMs = 250;
    float barrierBurstThreshold = 0.35f, minimumShieldEfficiency = 0.35f;
    float healMissingHealthPercent = 0.28f, igniteSafetyMargin = 15.0f, reactionDebounceSeconds = 0.25f;
};

class ActionArbiter final {
public:
    explicit ActionArbiter(DecisionLog* log = nullptr) : log_(log) {}
    void SetLog(DecisionLog* log) noexcept { log_ = log; }
    void SetDebounceSeconds(float seconds) noexcept {
        debounceSeconds_ = seconds > 0.0f ? seconds : 0.0f;
    }
    void Clear() noexcept {
        count_ = 0;
        selectedResourceMask_ = ResourceNone;
        selected_ = {};
    }
    bool Submit(const ActionRequest& request) noexcept {
        if (request.capability == Capability::None ||
            request.mode == ActionMode::Off ||
            count_ >= candidates_.size()) {
            return false;
        }
        ActionRequest normalized = request;
        if (normalized.resourceMask == ResourceNone) {
            normalized.resourceMask =
                ResourceFor(normalized.capability);
        }
        if (normalized.conflictMask == ResourceNone) {
            normalized.conflictMask =
                ConflictFor(normalized.capability);
        }
        candidates_[count_++] = normalized;
        return true;
    }
    const ActionRequest* Resolve(float now) noexcept {
        selected_ = {};
        selectedResourceMask_ = ResourceNone;
        if (count_ == 0) return nullptr;
        std::array<std::size_t, 64> order{};
        for (std::size_t i = 0; i < count_; ++i) order[i] = i;
        std::sort(
            order.begin(),
            order.begin() +
                static_cast<std::ptrdiff_t>(count_),
            [&](std::size_t lhs, std::size_t rhs) {
                const auto& a = candidates_[lhs];
                const auto& b = candidates_[rhs];
                if (ActionPriorityRank(a.priority) !=
                    ActionPriorityRank(b.priority)) {
                    return ActionPriorityRank(a.priority) <
                        ActionPriorityRank(b.priority);
                }
                if (a.expectedValue != b.expectedValue) {
                    return a.expectedValue > b.expectedValue;
                }
                if (a.createdAt != b.createdAt) {
                    return a.createdAt < b.createdAt;
                }
                if (a.capability != b.capability) {
                    return static_cast<int>(a.capability) <
                        static_cast<int>(b.capability);
                }
                if (a.targetId != b.targetId) {
                    return a.targetId < b.targetId;
                }
                return lhs < rhs;
            });
        for (std::size_t position = 0; position < count_; ++position) {
            const ActionRequest& candidate = candidates_[order[position]];
            if (candidate.IsExpired(now)) {
                Log(candidate, "expired", "request expired before decision");
                continue;
            }
            if (!candidate.IsReady(now)) {
                Log(candidate, "deferred", "request is waiting for its scheduled action window");
                continue;
            }
            if (selected_.capability == Capability::None) {
                selected_ = candidate;
                selectedResourceMask_ |= candidate.resourceMask;
                Log(candidate, "selected", candidate.reason);
                continue;
            }
            if ((candidate.resourceMask & selectedResourceMask_) != 0 ||
                (candidate.conflictMask & selectedResourceMask_) != 0) {
                Log(candidate, "blocked", "conflicts with the selected higher-priority request");
            } else {
                Log(candidate, "deferred", "a higher-priority action owns this decision tick");
            }
        }
        if (selected_.capability == Capability::None) {
            return nullptr;
        }
        if (hasLastSelected_ &&
            now > lastSelectedAt_ &&
            now - lastSelectedAt_ < debounceSeconds_ &&
            SameAction(lastSelected_, selected_)) {
            Log(selected_, "debounced",
                "duplicate action remains inside the reaction debounce window");
            selected_ = {};
            selectedResourceMask_ = ResourceNone;
            return nullptr;
        }
        lastSelected_ = selected_;
        lastSelectedAt_ = now;
        hasLastSelected_ = true;
        return &selected_;
    }
    const ActionRequest* Selected() const noexcept { return selected_.capability == Capability::None ? nullptr : &selected_; }
    std::size_t CandidateCount() const noexcept { return count_; }
    const ActionRequest& Candidate(std::size_t index) const noexcept { return candidates_[index]; }
private:
    static bool SameAction(const ActionRequest& lhs,
                           const ActionRequest& rhs) noexcept {
        return lhs.capability == rhs.capability &&
               lhs.mode == rhs.mode &&
               lhs.itemId == rhs.itemId &&
               lhs.itemSlot == rhs.itemSlot &&
               lhs.spellSlot == rhs.spellSlot &&
               lhs.targetId == rhs.targetId &&
               lhs.position.x == rhs.position.x &&
               lhs.position.y == rhs.position.y &&
               lhs.position.z == rhs.position.z &&
               lhs.expectedValue == rhs.expectedValue;
    }
    void Log(const ActionRequest& request,
             std::string_view outcome,
             std::string_view reason) noexcept {
        if (log_) {
            log_->Add(request.createdAt,
                      CapabilityName(request.capability),
                      outcome, reason,
                      static_cast<int>(request.priority),
                      request.confidence);
        }
    }
    std::array<ActionRequest, 64> candidates_{};
    std::size_t count_ = 0;
    std::uint32_t selectedResourceMask_ = ResourceNone;
    ActionRequest selected_{};
    ActionRequest lastSelected_{};
    float lastSelectedAt_ = 0.0f;
    float debounceSeconds_ = 0.25f;
    bool hasLastSelected_ = false;
    DecisionLog* log_ = nullptr;
};

class ICompanionModule {
public:
    virtual ~ICompanionModule() = default;
    virtual const char* Id() const noexcept = 0;
    virtual void OnEvent(const GameEvent&) = 0;
    virtual void Update(float now) = 0;
    virtual void Clear() = 0;
    bool enabled = true;
};

class CompanionModuleHost final {
public:
    struct Status {
        ICompanionModule* module = nullptr;
        std::uint32_t idHash = 0;
        std::uint32_t faultCount = 0;
        bool faulted = false;
    };

    explicit CompanionModuleHost(DecisionLog* log = nullptr) noexcept
        : log_(log) {}

    void SetLog(DecisionLog* log) noexcept { log_ = log; }

    bool Register(ICompanionModule& module) noexcept {
        const char* id = module.Id();
        if (!id || !id[0]) return false;
        const std::uint32_t idHash = HashId(id);
        for (std::size_t i = 0; i < count_; ++i) {
            if (slots_[i].idHash == idHash) {
                return slots_[i].module == &module;
            }
        }
        if (count_ >= slots_.size()) return false;
        slots_[count_++] = { &module, idHash, 0, false };
        return true;
    }

    bool SetEnabled(std::uint32_t idHash, bool enabled) noexcept {
        Status* status = Find(idHash);
        if (!status || !status->module ||
            (enabled && status->faulted)) {
            return false;
        }
        status->module->enabled = enabled;
        return true;
    }

    bool ResetFault(std::uint32_t idHash) noexcept {
        Status* status = Find(idHash);
        if (!status || !status->module) return false;
        status->faulted = false;
        status->module->enabled = true;
        return true;
    }

    void Dispatch(const GameEvent& event) noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            Status& status = slots_[i];
            if (!CanRun(status)) continue;
            try {
                status.module->OnEvent(event);
            } catch (...) {
                Quarantine(status, event.at, "event callback threw");
            }
        }
    }

    void Update(float now) noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            Status& status = slots_[i];
            if (!CanRun(status)) continue;
            try {
                status.module->Update(now);
            } catch (...) {
                Quarantine(status, now, "update callback threw");
            }
        }
    }

    void Clear() noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            Status& status = slots_[i];
            if (!status.module) continue;
            try {
                status.module->Clear();
            } catch (...) {
                Quarantine(status, 0.0f, "clear callback threw");
            }
        }
    }

    void Reset() noexcept {
        Clear();
        slots_.fill({});
        count_ = 0;
    }

    Status* Find(std::uint32_t idHash) noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            if (slots_[i].idHash == idHash) return &slots_[i];
        }
        return nullptr;
    }

    const Status* Find(std::uint32_t idHash) const noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            if (slots_[i].idHash == idHash) return &slots_[i];
        }
        return nullptr;
    }

    std::size_t Count() const noexcept { return count_; }

private:
    static bool CanRun(const Status& status) noexcept {
        return status.module && status.module->enabled && !status.faulted;
    }

    void Quarantine(Status& status,
                    float at,
                    std::string_view reason) noexcept {
        status.faulted = true;
        ++status.faultCount;
        if (status.module) status.module->enabled = false;
        if (log_ && status.module) {
            log_->Add(at, status.module->Id(), "faulted", reason, 0,
                      Confidence::Confirmed);
        }
    }

    std::array<Status, 32> slots_{};
    std::size_t count_ = 0;
    DecisionLog* log_ = nullptr;
};

} // namespace NightSharp::Companion
