#pragma once

// Shared targeting contracts.  This header intentionally contains no menu,
// event, or champion-specific code so consumers can depend on the advanced
// selector without pulling in the plugin implementation.

#include "../../../sdk/Wrappers/TargetSelector/ITargetSelector.h"
#include "../../../sdk/Enumerations/HitChance.h"
#include "../../../sdk/Enumerations/SpellType.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

namespace SDK {
class Spell;
}

namespace SDK::KuroTargetSelector {

enum class TargetPurpose : std::uint8_t {
    General,
    AutoAttack,
    ComboPrimary,
    Harass,
    Poke,
    Execute,
    Peel,
    Interrupt,
    AntiGapcloser,
    FleeThreat,
    ManualAssist,
};

enum class DecisionPhase : std::uint8_t {
    Planning,
    Execution,
};

enum class RouteKind : std::uint8_t {
    AutoAttack,
    UnitProjectile,
    SkillshotProjectile,
    ChargedProjectile,
    NonProjectile,
    Mobility,
    Custom,
};

enum class TargetProfile : std::uint8_t {
    General,
    AutoAttack,
    Burst,
    DPS,
    Poke,
    Execute,
    Peel,
    Interrupt,
    AntiGapcloser,
    FleeThreat,
};

enum class ProviderPriorityBand : std::uint8_t {
    BaseSafety = 0,
    RouteLegality,
    UserIntent,
    ChampionMechanic,
    PluginTactics,
    Preference,
};

enum class RejectReason : std::uint16_t {
    None = 0,
    Invalid = 1,
    // Preserve the explicit diagnostic ABI values while restoring the cheap
    // dead-unit rejection before any targetability query.
    Dead = 2,
    WrongTeam = 3,
    Despawned = 4,
    Untargetable = 5,
    OutOfRange = 6,
    NotVisible = 7,
    Ignored = 8,
    Blacklisted = 9,
    Invulnerable = 10,
    SpellShield = 11,
    Immunity = 12,
    RouteIllegal = 13,
    ProjectileWall = 14,
    Collision = 15,
    PredictionLow = 16,
    Stasis = 17,
    ClonePolicy = 18,
    RequiredTargetMissing = 19,
    RequiredTargetIllegal = 20,
    SelectedTargetIllegal = 21,
    PurposeRejected = 22,
    ProviderRejected = 23,
    ProviderFailure = 24,
    NoLegalCandidate = 25,
};

inline const char* RejectReasonName(RejectReason reason) {
    switch (reason) {
    case RejectReason::None: return "legal";
    case RejectReason::Invalid: return "invalid";
    case RejectReason::Dead: return "dead";
    case RejectReason::WrongTeam: return "wrong-team";
    case RejectReason::Despawned: return "despawned";
    case RejectReason::Untargetable: return "untargetable";
    case RejectReason::OutOfRange: return "out-of-range";
    case RejectReason::NotVisible: return "not-visible";
    case RejectReason::Ignored: return "ignored";
    case RejectReason::Blacklisted: return "blacklisted";
    case RejectReason::Invulnerable: return "invulnerable";
    case RejectReason::SpellShield: return "spell-shield";
    case RejectReason::Immunity: return "immunity";
    case RejectReason::RouteIllegal: return "route-illegal";
    case RejectReason::ProjectileWall: return "projectile-wall";
    case RejectReason::Collision: return "collision";
    case RejectReason::PredictionLow: return "prediction-low";
    case RejectReason::Stasis: return "stasis";
    case RejectReason::ClonePolicy: return "clone-policy";
    case RejectReason::RequiredTargetMissing: return "required-target-missing";
    case RejectReason::RequiredTargetIllegal: return "required-target-illegal";
    case RejectReason::SelectedTargetIllegal: return "selected-target-illegal";
    case RejectReason::PurposeRejected: return "purpose-rejected";
    case RejectReason::ProviderRejected: return "provider-rejected";
    case RejectReason::ProviderFailure: return "provider-failure";
    case RejectReason::NoLegalCandidate: return "no-legal-candidate";
    default: return "unknown";
    }
}

struct DamageProfile {
    DamageType Type = DamageType::True;
    float RawDamage = 0.0f;
    float ExpectedHits = 1.0f;
    bool IncludeShields = true;
    bool IgnoreShields = false;
    bool IsLethalAttempt = false;
};

struct RouteSegment {
    Vector3 Start = {};
    Vector3 End = {};
    float Radius = 0.0f;
    bool Projectile = true;
};

// RouteDescriptor keeps action identity separate from the hero that is being
// ranked.  That distinction matters for unit intermediaries, shadow/ball
// origins, bounces, charged release, and final execution revalidation.
struct RouteDescriptor {
    RouteKind Kind = RouteKind::NonProjectile;
    Vector3 Start = {};
    Vector3 Destination = {};
    Vector3 Prediction = {};
    Vector3 SecondarySource = {};
    std::array<RouteSegment, 4> Segments = {};
    std::size_t SegmentCount = 0;

    float ProjectileRadius = 0.0f;
    float ProjectileSpeed = 0.0f;
    float Delay = 0.0f;
    // Borrowed action identity used for request-local damage estimation.  It
    // is valid only for the synchronous Select/Rank/ValidateExecution call.
    const ::SDK::Spell* ActionSpell = nullptr;
    // Planning requests may carry a borrowed runtime spell so the selector
    // can query the active prediction provider with exactly the same slot,
    // calibration, collision set, and geometry that execution will use.  The
    // pointer is only borrowed for the synchronous Select/Rank call.
    const ::SDK::Spell* PredictionSpell = nullptr;
    SpellType PredictionType = SpellType::None;
    float PredictionRadius = 0.0f;
    float PredictionMaxCollisionCount = 0.0f;
    float SourceBoundingRadius = 0.0f;
    int PredictionHitChance = static_cast<int>(HitChance::None);
    int MinimumHitChance = static_cast<int>(HitChance::None);
    int CastSubjectId = 0;
    int IntendedTargetId = 0;
    int ExactEventSenderId = 0;

    bool ProjectileWallCheck = false;
    bool CollisionCheck = false;
    bool RequireNoCollision = false;
    bool RequireLineOfSight = false;
    bool LineOfSightKnown = false;
    bool LineOfSightClear = true;
    bool RequireVisible = false;
    bool AllowUnitCollision = true;
    bool IsChargedRelease = false;
    bool IsChargeStart = false;
    bool CheckAllSegments = false;
    // Auto-attack callers normally provide base AttackRange.  The gate adds
    // source/target hitboxes unless the caller explicitly supplied an already
    // expanded real range.
    bool RangeIncludesHitboxes = false;
    bool PredictionAddHitBox = true;
    bool PredictionAvailable = false;
    bool PredictionCollides = false;
    // Compatibility-only advisory field.  Planning may use the snapshot
    // sample; execution revalidates the live target state.
    bool TargetableAtExecution = true;
    bool SpellShieldAtImpact = false;
    bool ImmunityAtImpact = false;
};

struct TargetRequest {
    static constexpr std::size_t kMaxIgnoredTargets = 16;

    std::uint32_t RequesterId = 0;
    TargetPurpose Purpose = TargetPurpose::General;
    DecisionPhase Phase = DecisionPhase::Planning;

    Vector3 Source = {};
    float Range = 0.0f;
    DamageProfile Damage = {};
    RouteDescriptor Route = {};

    int PreferredTargetId = 0;
    int RequiredTargetId = 0;
    int LockedTargetId = 0;

    // Automatic callers may provide an archetype-aware profile while keeping
    // Purpose reserved for action legality (interrupt, execute, flee, ...).
    TargetProfile ProfileHint = TargetProfile::General;
    bool HasProfileHint = false;

    std::array<int, kMaxIgnoredTargets> IgnoredTargetIds = {};
    std::size_t IgnoredTargetCount = 0;

    bool AllowFallback = true;
    bool RespectManualSelection = true;
    bool RequireVisible = false;

    void AddIgnoredTarget(int networkId) {
        if (networkId <= 0 || IgnoredTargetCount >= IgnoredTargetIds.size()) {
            return;
        }
        for (std::size_t i = 0; i < IgnoredTargetCount; ++i) {
            if (IgnoredTargetIds[i] == networkId) return;
        }
        IgnoredTargetIds[IgnoredTargetCount++] = networkId;
    }

    bool IsIgnoredTarget(int networkId) const {
        if (networkId <= 0) return false;
        const std::size_t count =
            std::min(IgnoredTargetCount, IgnoredTargetIds.size());
        for (std::size_t i = 0; i < count; ++i) {
            if (IgnoredTargetIds[i] == networkId) return true;
        }
        return false;
    }
};

struct ProviderFact {
    static constexpr std::size_t kMaxKeyLength = 64;
    std::array<char, kMaxKeyLength> Key = {};
    float Value = 0.0f;
};

struct TargetFacts {
    AIHeroClient Target = {};
    std::uint32_t SnapshotId = 0;
    int NetworkId = 0;
    int Priority = 1;
    int Level = 0;

    Vector3 Position = {};
    Vector3 ServerPosition = {};
    Vector3 Direction = {};

    float Health = 0.0f;
    float MaxHealth = 0.0f;
    float AllShield = 0.0f;
    // Generic shield applies to every damage type; type-specific shields only
    // absorb their matching route.
    float PhysicalShield = 0.0f;
    float MagicalShield = 0.0f;
    float EffectiveHealth = 0.0f;
    // Effective health is expressed in post-mitigation damage for each route.
    // The generic EffectiveHealth field remains the unmitigated total pool for
    // providers that do not have a damage-type-specific request.
    float PhysicalEffectiveHealth = 0.0f;
    float MagicalEffectiveHealth = 0.0f;
    float MixedEffectiveHealth = 0.0f;
    float TrueEffectiveHealth = 0.0f;
    float HealthRegen = 0.0f;
    float Distance = 0.0f;
    float DistanceToSource = 0.0f;
    float MoveSpeed = 0.0f;
    float AttackDamage = 0.0f;
    float AbilityPower = 0.0f;
    float BoundingRadius = 0.0f;
    // Raw-equivalent damage of one ordinary attack; effective health applies
    // the target's mitigation exactly once during scoring.
    float AutoAttackDamage = 0.0f;
    // A lightweight magic-route estimate used when a request has no concrete
    // spell damage. Concrete requests use ActionDamageEstimate instead.
    float MagicalDamageEstimate = 0.0f;
    // Request-local spell damage, populated from Route.ActionSpell after the
    // immutable snapshot is copied.
    float ActionDamageEstimate = 0.0f;

    bool Valid = false;
    bool Dead = false;
    bool Visible = false;
    bool Targetable = false;
    bool Invulnerable = false;
    bool IsZombie = false;
    bool IsClone = false;
    bool IsDashing = false;
    bool IsMoving = false;
    bool IsCrowdControlled = false;
    bool IsSlowed = false;
    bool IsKnockedUp = false;
    bool IsSuppressed = false;
    bool IsGrounded = false;
    bool IsSilenced = false;
    bool IsBlinded = false;
    bool HasVulnerableMark = false;
    int DebuffCount = 0;
    float DebuffScore = 0.0f;
    bool IsChanneling = false;
    bool IsEscaping = false;
    bool IsFacingSource = false;
    bool IsStasis = false;

    // Candidate-specific prediction facts are deliberately request-local and
    // are populated after the immutable per-tick snapshot is copied.  This
    // lets FsPred's confidence classification influence ranking without
    // contaminating snapshots shared by requests for different spells.
    Vector3 PredictionCastPosition = {};
    Vector3 PredictionUnitPosition = {};
    int PredictionHitChance = static_cast<int>(HitChance::None);
    bool PredictionEvaluated = false;
    bool PredictionAvailable = false;
    bool PredictionCollides = false;

    // Local composition around the target: alive friendly heroes that can
    // follow up inside a radius, and alive enemy heroes nesting around it.
    int AlliesNearTarget = 0;
    int EnemiesNearTarget = 0;

    std::array<ProviderFact, 16> ProviderFacts = {};
    std::size_t ProviderFactCount = 0;

    void AddProviderFact(const char* key, float value) {
        if (!key || !key[0]) return;
        const std::size_t count =
            std::min(ProviderFactCount, ProviderFacts.size());
        for (std::size_t i = 0; i < count; ++i) {
            if (ProviderFacts[i].Key[0] &&
                _stricmp(ProviderFacts[i].Key.data(), key) == 0) {
                ProviderFacts[i].Value = value;
                return;
            }
        }
        if (count >= ProviderFacts.size()) return;
        auto& fact = ProviderFacts[count];
        strncpy_s(
            fact.Key.data(), fact.Key.size(), key, _TRUNCATE);
        fact.Value = value;
        ProviderFactCount = count + 1;
    }

    float ProviderFactValue(const char* key, float fallback = 0.0f) const {
        if (!key || !key[0]) return fallback;
        const std::size_t count =
            std::min(ProviderFactCount, ProviderFacts.size());
        for (std::size_t i = 0; i < count; ++i) {
            if (ProviderFacts[i].Key[0] &&
                _stricmp(ProviderFacts[i].Key.data(), key) == 0) {
                return ProviderFacts[i].Value;
            }
        }
        return fallback;
    }
};

constexpr std::size_t kMaxEnemySnapshot = 64;

struct TargetSnapshot {
    std::uint64_t Id = 0;
    int Tick = 0;
    Vector3 PlayerPosition = {};
    std::array<TargetFacts, kMaxEnemySnapshot> Enemies = {};
    std::size_t Count = 0;
};

struct ScoreContribution {
    const char* ReasonCode = nullptr;
    const char* Name = nullptr;
    float Value = 0.0f;
    float MinValue = -1000.0f;
    float MaxValue = 1000.0f;
};

struct ScoreBreakdown {
    static constexpr std::size_t kMaxContributions = 32;
    std::array<ScoreContribution, kMaxContributions> Contributions = {};
    std::size_t Count = 0;
    float Total = 0.0f;

    void Add(const char* reasonCode,
             const char* name,
             float value,
             float minValue = -1000.0f,
             float maxValue = 1000.0f) {
        float safeMin = std::isfinite(minValue) ? minValue : -1000.0f;
        float safeMax = std::isfinite(maxValue) ? maxValue : 1000.0f;
        if (safeMax < safeMin) std::swap(safeMin, safeMax);

        const float bounded = std::isfinite(value)
            ? std::clamp(value, safeMin, safeMax)
            : 0.0f;
        const float nextTotal = Total + bounded;
        Total = std::isfinite(nextTotal) ? nextTotal : Total;
        if (Count < Contributions.size()) {
            Contributions[Count++] = {
                reasonCode, name, bounded, safeMin, safeMax
            };
        }
    }
};

struct TargetProviderContext;
struct TargetSnapshot;

using BuildFactsCallback = bool (*)(const TargetRequest& request,
                                    const AIHeroClient& target,
                                    TargetFacts& facts);
using ValidateCallback = RejectReason (*)(const TargetProviderContext& context);
using ScoreCallback = ScoreContribution (*)(const TargetProviderContext& context);

struct TargetRuleProvider {
    std::uint32_t OwnerId = 0;
    const char* Key = nullptr;
    ProviderPriorityBand Band = ProviderPriorityBand::PluginTactics;
    BuildFactsCallback BuildFacts = nullptr;
    ValidateCallback Validate = nullptr;
    ScoreCallback Score = nullptr;
};

using ProviderToken = std::uint64_t;

struct TargetProviderContext {
    const TargetRequest* Request = nullptr;
    const AIHeroClient* Target = nullptr;
    const TargetFacts* Facts = nullptr;
    const TargetSnapshot* Snapshot = nullptr;
};

struct ProviderDiagnostic {
    ProviderToken Token = 0;
    std::uint32_t OwnerId = 0;
    const char* Key = nullptr;
    ProviderPriorityBand Band = ProviderPriorityBand::PluginTactics;
    double LastMilliseconds = 0.0;
    std::uint64_t Calls = 0;
    std::uint64_t Failures = 0;
    bool Registered = false;
};

struct TargetDecision {
    AIHeroClient Target = {};
    RouteKind Route = RouteKind::NonProjectile;
    float Score = 0.0f;
    std::uint64_t SnapshotId = 0;
    RejectReason Rejection = RejectReason::None;
    ScoreBreakdown Breakdown = {};
    bool Legal = false;
};

struct SelectionState {
    AIHeroClient Selected = {};
    int SelectedNetworkId = 0;
    int IncumbentNetworkId = 0;
    TargetProfile Profile = TargetProfile::General;
    bool PreferSelectedTarget = true;
    bool OnlySelectedTarget = false;
    bool ManualOverrideActive = false;
    bool Suspended = false;
    std::uint64_t Revision = 0;
};

class IKuroTargetSelector {
public:
    virtual ~IKuroTargetSelector() = default;

    virtual TargetDecision Select(const TargetRequest& request) = 0;
    virtual std::vector<TargetDecision> Rank(const TargetRequest& request) = 0;
    virtual TargetDecision Explain(const AIHeroClient& target,
                                   const TargetRequest& request) = 0;
    virtual const TargetSnapshot& GetSnapshot() const = 0;
    virtual ProviderToken RegisterProvider(const TargetRuleProvider& provider) = 0;
    virtual bool UnregisterProvider(ProviderToken token) = 0;
    virtual SelectionState GetSelectionState() const = 0;
    virtual bool ValidateExecution(const TargetRequest& request,
                                   const AIHeroClient& target) = 0;
    virtual std::vector<ProviderDiagnostic> GetProviderDiagnostics() const = 0;
};

// The plugin owns the service object; consumers only borrow it while the core
// selector is loaded.  Clearing this pointer is part of plugin unload.
inline IKuroTargetSelector*& ActiveServiceStorage() {
    static IKuroTargetSelector* service = nullptr;
    return service;
}

inline IKuroTargetSelector* ActiveService() {
    return ActiveServiceStorage();
}

inline void SetActiveService(IKuroTargetSelector* service) {
    ActiveServiceStorage() = service;
}

// ActiveService() intentionally becomes null while the registry is using SDK
// or Impulse, but the Kuro implementation may still be alive and holding
// provider registrations.  This second handle lets providers unregister from
// an inactive implementation without ever dereferencing a service after its
// plugin has been unloaded.
inline IKuroTargetSelector*& LiveServiceStorage() {
    static IKuroTargetSelector* service = nullptr;
    return service;
}

inline IKuroTargetSelector* LiveService() {
    return LiveServiceStorage();
}

inline void SetLiveService(IKuroTargetSelector* service) {
    LiveServiceStorage() = service;
}

} // namespace SDK::KuroTargetSelector

// Short aliases keep plugin code readable while the public SDK namespace
// remains the canonical ABI surface.
namespace Plugins::KuroTargetSelector {
using ::SDK::KuroTargetSelector::BuildFactsCallback;
using ::SDK::KuroTargetSelector::DamageProfile;
using ::SDK::KuroTargetSelector::DecisionPhase;
using ::SDK::KuroTargetSelector::IKuroTargetSelector;
using ::SDK::KuroTargetSelector::ProviderDiagnostic;
using ::SDK::KuroTargetSelector::ProviderPriorityBand;
using ::SDK::KuroTargetSelector::ProviderToken;
using ::SDK::KuroTargetSelector::RejectReason;
using ::SDK::KuroTargetSelector::RouteDescriptor;
using ::SDK::KuroTargetSelector::RouteKind;
using ::SDK::KuroTargetSelector::ScoreBreakdown;
using ::SDK::KuroTargetSelector::ScoreCallback;
using ::SDK::KuroTargetSelector::ScoreContribution;
using ::SDK::KuroTargetSelector::SelectionState;
using ::SDK::KuroTargetSelector::TargetDecision;
using ::SDK::KuroTargetSelector::TargetFacts;
using ::SDK::KuroTargetSelector::TargetProfile;
using ::SDK::KuroTargetSelector::TargetProviderContext;
using ::SDK::KuroTargetSelector::TargetPurpose;
using ::SDK::KuroTargetSelector::TargetRequest;
using ::SDK::KuroTargetSelector::TargetRuleProvider;
using ::SDK::KuroTargetSelector::TargetSnapshot;
using ::SDK::KuroTargetSelector::ValidateCallback;

inline IKuroTargetSelector* ActiveService() {
    return ::SDK::KuroTargetSelector::ActiveService();
}

inline IKuroTargetSelector* LiveService() {
    return ::SDK::KuroTargetSelector::LiveService();
}
} // namespace Plugins::KuroTargetSelector
