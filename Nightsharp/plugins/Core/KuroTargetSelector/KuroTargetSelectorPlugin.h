#pragma once

#include "../../IPlugin.h"
#include "../../../sdk/Wrappers/TargetSelector/TargetSelector.h"
#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/Wrappers/Damages/Damage.h"
#include "../../../core/CoreBuffs.h"

#include "KuroTargetActionGate.h"
#include "KuroTargetSelectorDrawing.h"
#include "KuroTargetSelectorMenu.h"
#include "KuroTargetSelectorPolicy.h"
#include "KuroTargetSelectorProviders.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace Plugins::KuroTargetSelector {

class KuroTargetSelectorService final : public ::SDK::ITargetSelector,
                                        public ::SDK::KuroTargetSelector::IKuroTargetSelector {
public:
    explicit KuroTargetSelectorService(
        ::SDK::Menu* parent, bool parentIsRoot = false)
        : menu_(std::make_unique<Menu>(parent, parentIsRoot)),
          drawing_(std::make_unique<Drawing>(menu_.get())) {
        // Construction registers the menu/drawing objects, but the registry
        // owns activation.  Start suspended so a merely loaded plugin cannot
        // duplicate SDK/Impulse callbacks before it becomes current.
        ::SDK::KuroTargetSelector::SetLiveService(this);
        Suspend();
    }

    ~KuroTargetSelectorService() override {
        if (::SDK::KuroTargetSelector::LiveService() == this) {
            ::SDK::KuroTargetSelector::SetLiveService(nullptr);
        }
        if (::SDK::KuroTargetSelector::ActiveService() == this) {
            ::SDK::KuroTargetSelector::SetActiveService(nullptr);
        }
        drawing_.reset();
        menu_.reset();
    }

    ::SDK::AIHeroClient GetSelectedTarget() const override {
        return menu_ ? menu_->Selected() : ::SDK::AIHeroClient();
    }

    void SetTarget(const ::SDK::AIHeroClient& target) override {
        if (menu_) {
            menu_->SetSelected(target);
            ++revision_;
        }
    }

    ::SDK::AIHeroClient GetTarget(
        float range,
        ::SDK::DamageType damageType = ::SDK::DamageType::True,
        bool ignoreShields = true,
        const ::SDK::Vector3& from = ::SDK::Vector3(),
        const std::vector<::SDK::AIHeroClient>* ignoreChampions = nullptr) override {
        const auto ranked = Rank(LegacyRequest(
            range, damageType, ignoreShields, from, ignoreChampions));
        for (const auto& decision : ranked) {
            if (decision.Legal && !Ignored(decision.Target, ignoreChampions)) {
                incumbentNetworkId_ = decision.Target.NetworkId();
                return decision.Target;
            }
        }
        return {};
    }

    std::vector<::SDK::AIHeroClient> GetTargets(
        float range,
        ::SDK::DamageType damageType = ::SDK::DamageType::True,
        bool ignoreShields = true,
        const ::SDK::Vector3& from = ::SDK::Vector3(),
        const std::vector<::SDK::AIHeroClient>* ignoreChampions = nullptr) override {
        std::vector<::SDK::AIHeroClient> result;
        const auto ranked = Rank(LegacyRequest(
            range, damageType, ignoreShields, from, ignoreChampions));
        result.reserve(ranked.size());
        for (const auto& decision : ranked) {
            if (decision.Legal && !Ignored(decision.Target, ignoreChampions)) {
                result.push_back(decision.Target);
            }
        }
        if (!result.empty()) incumbentNetworkId_ = result.front().NetworkId();
        return result;
    }

    int GetPriority(const ::SDK::AIHeroClient& target) const override {
        return menu_ ? menu_->Priority(target.NetworkId()) : 1;
    }

    void Suspend() override {
        if (suspended_) return;
        if (drawing_) drawing_->Suspend();
        if (menu_) menu_->Suspend();
        if (::SDK::KuroTargetSelector::ActiveService() == this) {
            ::SDK::KuroTargetSelector::SetActiveService(nullptr);
        }
        suspended_ = true;
    }

    void Resume() override {
        if (!suspended_) return;
        if (menu_) menu_->Resume();
        if (drawing_) drawing_->Resume();
        ::SDK::KuroTargetSelector::SetActiveService(this);
        suspended_ = false;
    }

    ::SDK::KuroTargetSelector::TargetDecision Select(
        const ::SDK::KuroTargetSelector::TargetRequest& request) override {
        const auto ranked = Rank(request);
        for (const auto& decision : ranked) {
            if (decision.Legal) {
                incumbentNetworkId_ = decision.Target.NetworkId();
                return decision;
            }
        }
        if (!ranked.empty()) {
            // Rank() keeps rejected identities for diagnostics, but Select()
            // must never hand an illegal target to a caller as a usable
            // result (especially with OnlySelected/RequiredTarget).
            auto none = ranked.front();
            none.Target = {};
            none.Legal = false;
            if (none.Rejection ==
                    ::SDK::KuroTargetSelector::RejectReason::None) {
                none.Rejection =
                    ::SDK::KuroTargetSelector::RejectReason::NoLegalCandidate;
            }
            return none;
        }
        ::SDK::KuroTargetSelector::TargetDecision empty{};
        empty.Route = request.Route.Kind;
        empty.SnapshotId = snapshot_.Id;
        empty.Rejection = ::SDK::KuroTargetSelector::RejectReason::NoLegalCandidate;
        return empty;
    }

    std::vector<::SDK::KuroTargetSelector::TargetDecision> Rank(
        const ::SDK::KuroTargetSelector::TargetRequest& request) override {
        using namespace ::SDK::KuroTargetSelector;

        if (suspended_) return {};

        // Providers are untrusted extension points.  A callback that asks
        // the selector to rank again must fail closed for that nested call,
        // while the outer request continues with the provider's neutral
        // result.
        EvaluationGuard evaluation(evaluating_);
        if (!evaluation.Acquired()) return {};

        BuildSnapshot(request);
        std::vector<TargetDecision> result;
        result.reserve(snapshot_.Count);

        const int selectedId = menu_ ? menu_->Selected().NetworkId() : 0;
        // A locked target is a soft lease when fallback is allowed.  Only an
        // explicit preferred/selected target may bypass the score entirely.
        const int manualPreferredId = request.PreferredTargetId != 0
            ? request.PreferredTargetId
            : (request.RespectManualSelection && menu_ &&
               menu_->PreferSelectedTarget() ? selectedId : 0);
        const int requiredId = request.RequiredTargetId != 0
            ? request.RequiredTargetId
            : (!request.AllowFallback ? request.LockedTargetId : 0);
        const bool onlySelected = menu_ && menu_->OnlySelectedTarget();
        const auto providerOrder = BuildProviderOrder();
        const auto player = ::SDK::GameObjects::Player();

        for (std::size_t i = 0; i < snapshot_.Count; ++i) {
            const TargetFacts& facts = snapshot_.Enemies[i];
            TargetDecision decision{};
            decision.Target = facts.Target;
            decision.Route = request.Route.Kind;
            decision.SnapshotId = snapshot_.Id;

            const auto gate = KuroTargetActionGate::Evaluate(
                request, facts.Target, facts.Targetable);
            if (!gate.Legal) {
                decision.Rejection = requiredId == facts.NetworkId
                    ? RejectReason::RequiredTargetIllegal
                    : ((onlySelected && selectedId == facts.NetworkId)
                        ? RejectReason::SelectedTargetIllegal
                        : gate.Rejection);
                result.push_back(decision);
                continue;
            }
            if (facts.IsClone) {
                decision.Rejection = RejectReason::ClonePolicy;
                result.push_back(decision);
                continue;
            }
            if (request.IsIgnoredTarget(facts.NetworkId)) {
                decision.Rejection = RejectReason::Ignored;
                result.push_back(decision);
                continue;
            }
            if (menu_ && menu_->IsBlacklisted(facts.NetworkId)) {
                decision.Rejection = RejectReason::Blacklisted;
                result.push_back(decision);
                continue;
            }
            if (requiredId != 0 && requiredId != facts.NetworkId) {
                decision.Rejection = RejectReason::RequiredTargetMissing;
                result.push_back(decision);
                continue;
            }
            if (onlySelected &&
                (selectedId == 0 || selectedId != facts.NetworkId)) {
                decision.Rejection = RejectReason::SelectedTargetIllegal;
                result.push_back(decision);
                continue;
            }

            TargetFacts workingFacts = facts;
            const ::SDK::Vector3 source = ResolveRequestSource(
                request, player);
            workingFacts.DistanceToSource = source.IsValid() && !source.IsZero()
                ? source.Distance(workingFacts.Target.Position())
                : workingFacts.DistanceToSource;
            workingFacts.Distance = workingFacts.DistanceToSource;
            TargetProviderContext context{};
            context.Request = &request;
            context.Target = &workingFacts.Target;
            context.Facts = &workingFacts;
            context.Snapshot = &snapshot_;

            bool providerRejected = false;
            for (auto* provider : providerOrder) {
                const bool accepted = providersRegistry_.BuildFacts(
                    *provider, request, workingFacts.Target, workingFacts);
                RestoreCoreFacts(workingFacts, facts);
                if (!accepted) {
                    providerRejected = true;
                    break;
                }
            }
            if (providerRejected) {
                decision.Rejection = RejectReason::ProviderRejected;
                result.push_back(decision);
                continue;
            }
            const auto purpose = KuroTargetSelectorPolicy::ValidatePurpose(
                request, workingFacts);
            if (purpose != RejectReason::None) {
                decision.Rejection = purpose;
                result.push_back(decision);
                continue;
            }
            for (auto* provider : providerOrder) {
                const RejectReason rejection = providersRegistry_.Validate(
                    *provider, context);
                if (rejection != RejectReason::None) {
                    decision.Rejection = rejection;
                    break;
                }
            }
            if (decision.Rejection != RejectReason::None) {
                result.push_back(decision);
                continue;
            }

            const TargetProfile requestedProfile =
                KuroTargetSelectorPolicy::ProfileFor(request.Purpose);
            const TargetProfile menuProfile = menu_
                ? menu_->Profile() : TargetProfile::General;
            const TargetProfile activeProfile = menu_ &&
                    !menu_->AutomaticProfile()
                ? menuProfile : requestedProfile;
            decision.Score = KuroTargetSelectorPolicy::BuildScoreForProfile(
                request,
                workingFacts,
                menu_ ? menu_->Priority(facts.NetworkId) : 1,
                incumbentNetworkId_,
                decision.Breakdown,
                activeProfile,
                menu_ ? menu_->Stickiness() : -1.0f);
            if (manualPreferredId != 0 &&
                manualPreferredId == facts.NetworkId) {
                decision.Breakdown.Add(
                    "manual-preference", "manual target preference",
                    100000.0f, 0.0f, 100000.0f);
                decision.Score = decision.Breakdown.Total;
            }
            if (request.AllowFallback && request.LockedTargetId != 0 &&
                request.LockedTargetId == facts.NetworkId &&
                request.LockedTargetId != manualPreferredId &&
                request.LockedTargetId != incumbentNetworkId_) {
                const float softLock = menu_
                    ? std::clamp(menu_->Stickiness() * 0.35f, 0.0f, 70.0f)
                    : 40.0f;
                decision.Breakdown.Add(
                    "soft-lock", "fallback target lease", softLock, 0.0f, 70.0f);
                decision.Score = decision.Breakdown.Total;
            }
            for (auto* provider : providerOrder) {
                const ScoreContribution contribution = providersRegistry_.Score(
                    *provider, context);
                decision.Breakdown.Add(
                    contribution.ReasonCode,
                    contribution.Name,
                    contribution.Value,
                    contribution.MinValue,
                    contribution.MaxValue);
            }

            // Planning reuses the targetability sample captured in the
            // snapshot.  ValidateExecution keeps the live race-safety gate.
            const auto finalGate = KuroTargetActionGate::Evaluate(
                request, workingFacts.Target, facts.Targetable);
            if (!finalGate.Legal) {
                decision.Rejection = requiredId == facts.NetworkId
                    ? RejectReason::RequiredTargetIllegal
                    : ((onlySelected && selectedId == facts.NetworkId)
                        ? RejectReason::SelectedTargetIllegal
                        : finalGate.Rejection);
                result.push_back(decision);
                continue;
            }

            decision.Score = decision.Breakdown.Total;
            decision.Legal = true;
            result.push_back(decision);
        }

        if (requiredId != 0) {
            const bool found = std::any_of(result.begin(), result.end(),
                [requiredId](const TargetDecision& decision) {
                    return decision.Target.NetworkId() == requiredId;
                });
            if (!found) {
                TargetDecision missing{};
                missing.Route = request.Route.Kind;
                missing.SnapshotId = snapshot_.Id;
                missing.Rejection = RejectReason::RequiredTargetMissing;
                result.push_back(missing);
            }
        }

        std::stable_sort(result.begin(), result.end(),
            [this, manualPreferredId](const TargetDecision& lhs,
                                const TargetDecision& rhs) {
                if (lhs.Legal != rhs.Legal) return lhs.Legal > rhs.Legal;
                const bool lhsPreferred = manualPreferredId != 0 &&
                    lhs.Target.NetworkId() == manualPreferredId;
                const bool rhsPreferred = manualPreferredId != 0 &&
                    rhs.Target.NetworkId() == manualPreferredId;
                if (lhsPreferred != rhsPreferred) return lhsPreferred;
                if (std::fabs(lhs.Score - rhs.Score) > 0.0001f) {
                    return lhs.Score > rhs.Score;
                }
                const int lhsPriority = menu_
                    ? menu_->Priority(lhs.Target.NetworkId()) : 1;
                const int rhsPriority = menu_
                    ? menu_->Priority(rhs.Target.NetworkId()) : 1;
                if (lhsPriority != rhsPriority) {
                    return lhsPriority > rhsPriority;
                }
                return lhs.Target.NetworkId() < rhs.Target.NetworkId();
            });

        lastDecisions_ = result;
        return result;
    }

    ::SDK::KuroTargetSelector::TargetDecision Explain(
        const ::SDK::AIHeroClient& target,
        const ::SDK::KuroTargetSelector::TargetRequest& request) override {
        const auto ranked = Rank(request);
        for (const auto& decision : ranked) {
            if (decision.Target.Compare(target)) return decision;
        }
        return {};
    }

    const ::SDK::KuroTargetSelector::TargetSnapshot& GetSnapshot() const override {
        return snapshot_;
    }

    ::SDK::KuroTargetSelector::ProviderToken RegisterProvider(
        const ::SDK::KuroTargetSelector::TargetRuleProvider& provider) override {
        if (evaluating_) return 0;
        const ::SDK::KuroTargetSelector::ProviderToken token =
            providersRegistry_.Register(provider);
        if (token) ++revision_;
        return token;
    }

    bool UnregisterProvider(
        ::SDK::KuroTargetSelector::ProviderToken token) override {
        if (evaluating_) return false;
        const bool removed = providersRegistry_.Unregister(token);
        if (removed) ++revision_;
        return removed;
    }

    ::SDK::KuroTargetSelector::SelectionState GetSelectionState() const override {
        using namespace ::SDK::KuroTargetSelector;
        SelectionState state{};
        state.Selected = menu_ ? menu_->Selected() : ::SDK::AIHeroClient();
        state.SelectedNetworkId = state.Selected.NetworkId();
        state.IncumbentNetworkId = incumbentNetworkId_;
        state.Profile = menu_ ? menu_->Profile() : TargetProfile::General;
        state.PreferSelectedTarget = !menu_ || menu_->PreferSelectedTarget();
        state.OnlySelectedTarget = menu_ && menu_->OnlySelectedTarget();
        state.ManualOverrideActive = menu_ && menu_->ManualOverrideActive();
        state.Suspended = suspended_;
        state.Revision = revision_;
        return state;
    }

    bool ValidateExecution(
        const ::SDK::KuroTargetSelector::TargetRequest& request,
        const ::SDK::AIHeroClient& target) override {
        using namespace ::SDK::KuroTargetSelector;

        if (suspended_) return false;
        EvaluationGuard evaluation(evaluating_);
        if (!evaluation.Acquired()) return false;

        auto execution = request;
        execution.Phase = DecisionPhase::Execution;
        const int targetId = target.NetworkId();
        if (targetId <= 0 || request.IsIgnoredTarget(targetId)) return false;
        if (request.RequiredTargetId != 0 &&
            request.RequiredTargetId != targetId) {
            return false;
        }
        if (!request.AllowFallback && request.LockedTargetId != 0 &&
            request.LockedTargetId != targetId) {
            return false;
        }
        if (menu_ && menu_->OnlySelectedTarget()) {
            const int selectedId = menu_->Selected().NetworkId();
            if (selectedId <= 0 || selectedId != targetId) return false;
        }

        const auto gate = KuroTargetActionGate::Evaluate(execution, target);
        if (!gate.Legal || target.IsClone() ||
            (menu_ && menu_->IsBlacklisted(targetId))) {
            return false;
        }

        // Keep provider snapshot context current even when execution validation
        // is called without a preceding Rank() in this tick.
        BuildSnapshot(execution);

        TargetFacts facts{};
        const auto player = ::SDK::GameObjects::Player();
        PopulateTargetFacts(
            facts,
            target,
            player,
            static_cast<std::uint32_t>(snapshot_.Id),
            ResolveRequestSource(execution, player));
        facts.Priority = menu_ ? menu_->Priority(facts.NetworkId) : 1;
        const TargetFacts coreFacts = facts;

        const auto providerOrder = BuildProviderOrder();
        for (auto* provider : providerOrder) {
            const bool accepted = providersRegistry_.BuildFacts(
                *provider, execution, target, facts);
            RestoreCoreFacts(facts, coreFacts);
            if (!accepted) return false;
        }

        if (KuroTargetSelectorPolicy::ValidatePurpose(execution, facts) !=
                RejectReason::None) {
            return false;
        }

        TargetProviderContext context{};
        context.Request = &execution;
        context.Target = &facts.Target;
        context.Facts = &facts;
        context.Snapshot = &snapshot_;
        for (auto* provider : providerOrder) {
            if (providersRegistry_.Validate(*provider, context) !=
                    RejectReason::None) {
                return false;
            }
        }

        // The final live targetability check is authoritative.  This closes
        // the race where the target changes state during provider callbacks.
        return KuroTargetActionGate::Evaluate(execution, target).Legal;
    }

    std::vector<::SDK::KuroTargetSelector::ProviderDiagnostic>
    GetProviderDiagnostics() const override {
        return providersRegistry_.Diagnostics();
    }

    void OnRender() {
        if (drawing_) drawing_->Draw(GetSelectionState(), lastDecisions_);
    }

private:
    class EvaluationGuard final {
    public:
        explicit EvaluationGuard(bool& flag)
            : flag_(flag), acquired_(!flag) {
            if (acquired_) flag_ = true;
        }

        ~EvaluationGuard() {
            if (acquired_) flag_ = false;
        }

        EvaluationGuard(const EvaluationGuard&) = delete;
        EvaluationGuard& operator=(const EvaluationGuard&) = delete;

        bool Acquired() const { return acquired_; }

    private:
        bool& flag_;
        bool acquired_ = false;
    };

    const std::vector<::SDK::KuroTargetSelector::ProviderRegistry::Entry*>&
    BuildProviderOrder() {
        using namespace ::SDK::KuroTargetSelector;
        const auto currentRev = revision_;
        if (cachedProviderRevision_ == currentRev && !cachedProviderOrder_.empty()) {
            return cachedProviderOrder_;
        }
        cachedProviderOrder_.clear();
        cachedProviderOrder_.reserve(providersRegistry_.MutableEntries().size());
        for (auto& entry : providersRegistry_.MutableEntries()) {
            // BaseSafety is reserved for the live core action gate.
            if (entry.Provider.Band != ProviderPriorityBand::BaseSafety) {
                cachedProviderOrder_.push_back(&entry);
            }
        }
        std::stable_sort(cachedProviderOrder_.begin(), cachedProviderOrder_.end(),
            [](const auto* lhs, const auto* rhs) {
                return static_cast<int>(lhs->Provider.Band) <
                    static_cast<int>(rhs->Provider.Band);
            });
        cachedProviderRevision_ = currentRev;
        return cachedProviderOrder_;
    }

    static void RestoreCoreFacts(
        ::SDK::KuroTargetSelector::TargetFacts& facts,
        const ::SDK::KuroTargetSelector::TargetFacts& core) {
        facts.Target = core.Target;
        facts.SnapshotId = core.SnapshotId;
        facts.NetworkId = core.NetworkId;
        facts.Priority = core.Priority;
        facts.Valid = core.Valid;
        facts.Dead = core.Dead;
        facts.Visible = core.Visible;
        facts.Targetable = core.Targetable;
        facts.Invulnerable = core.Invulnerable;
        facts.IsZombie = core.IsZombie;
        facts.IsClone = core.IsClone;
        facts.IsStasis = core.IsStasis;
    }

    static ::SDK::Vector3 ResolveRequestSource(
        const ::SDK::KuroTargetSelector::TargetRequest& request,
        const ::SDK::AIHeroClient& player) {
        if (request.Route.Start.IsValid() && !request.Route.Start.IsZero()) {
            return request.Route.Start;
        }
        if (request.Source.IsValid() && !request.Source.IsZero()) {
            return request.Source;
        }
        return player.IsValid() ? player.Position() : ::SDK::Vector3();
    }

    static ::SDK::KuroTargetSelector::TargetRequest LegacyRequest(
        float range,
        ::SDK::DamageType damageType,
        bool ignoreShields,
        const ::SDK::Vector3& from,
        const std::vector<::SDK::AIHeroClient>* ignoreChampions) {
        using namespace ::SDK::KuroTargetSelector;
        TargetRequest request{};
        request.Purpose = TargetPurpose::General;
        request.Phase = DecisionPhase::Planning;
        request.Source = from;
        request.Range = range;
        request.Damage.Type = damageType;
        request.Damage.IgnoreShields = ignoreShields;
        request.Damage.IncludeShields = !ignoreShields;
        request.Route.Kind = RouteKind::NonProjectile;
        request.Route.Start = from;
        request.RespectManualSelection = true;
        if (ignoreChampions) {
            for (const auto& ignored : *ignoreChampions) {
                request.AddIgnoredTarget(ignored.NetworkId());
            }
        }
        return request;
    }

    static bool Ignored(const ::SDK::AIHeroClient& target,
                        const std::vector<::SDK::AIHeroClient>* ignored) {
        if (!ignored) return false;
        for (const auto& item : *ignored) {
            if (item.Compare(target)) return true;
        }
        return false;
    }

    static float SafeNonNegative(float value) {
        return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
    }

    static float DamageMultiplier(const ::SDK::AIHeroClient& player,
                                  const ::SDK::AIHeroClient& target,
                                  ::SDK::DamageType type) {
        if (!player.IsValid() || !target.IsValid()) return 1.0f;
        constexpr float sampleDamage = 1000.0f;
        const float dealt = ::SDK::Damage::CalculateDamage(
            player, target, type, sampleDamage);
        if (!std::isfinite(dealt)) return 1.0f;
        return std::clamp(dealt / sampleDamage, 0.0f, 20.0f);
    }

    static float EffectiveHealthFromPool(float pool, float multiplier) {
        if (pool <= 0.0f) return 0.0f;
        if (!std::isfinite(multiplier) || multiplier <= 0.0f) {
            return FLT_MAX;
        }
        return pool / multiplier;
    }

    static void PopulateTargetFacts(
        ::SDK::KuroTargetSelector::TargetFacts& facts,
        const ::SDK::AIHeroClient& target,
        const ::SDK::AIHeroClient& player,
        std::uint32_t snapshotId,
        const ::SDK::Vector3& source) {
        facts = {};
        facts.Target = target;
        facts.SnapshotId = snapshotId;
        facts.Valid = target.IsValid();
        if (!facts.Valid) return;

        facts.NetworkId = target.NetworkId();
        facts.IsZombie = target.IsZombie();
        facts.Dead = target.IsDead();
        facts.Targetable = (!facts.Dead || facts.IsZombie) &&
            target.IsTargetable();
        facts.Visible = target.IsVisible();
        facts.Invulnerable = target.IsInvulnerable();
        facts.IsClone = target.IsClone();

        facts.Level = target.Level();
        facts.Position = target.Position();
        facts.ServerPosition = target.ServerPosition();
        facts.Direction = target.Direction();
        facts.Health = SafeNonNegative(target.Health());
        facts.MaxHealth = SafeNonNegative(target.MaxHealth());
        facts.AllShield = SafeNonNegative(target.AllShield());
        facts.PhysicalShield = SafeNonNegative(target.PhysicalShield());
        facts.MagicalShield = SafeNonNegative(target.MagicalShield());

        facts.DistanceToSource = source.IsValid() && !source.IsZero()
            ? source.Distance(facts.Position)
            : (player.IsValid() ? player.Distance(facts.Position) : 0.0f);
        facts.Distance = facts.DistanceToSource;

        // Rejected untargetable or distant identities skip expensive damage simulations
        // (GetAutoAttackDamage / CalculateDamage) to avoid FPS drops.
        const bool canEvaluateDamage = facts.Targetable && player.IsValid() && facts.DistanceToSource <= 2500.0f;
        const float physicalMultiplier = canEvaluateDamage
            ? DamageMultiplier(player, target, ::SDK::DamageType::Physical)
            : 1.0f;
        const float magicalMultiplier = canEvaluateDamage
            ? DamageMultiplier(player, target, ::SDK::DamageType::Magical)
            : 1.0f;
        const float mixedMultiplier =
            physicalMultiplier > 0.0f && magicalMultiplier > 0.0f
            ? (physicalMultiplier + magicalMultiplier) * 0.5f
            : std::max(physicalMultiplier, magicalMultiplier);
        const float health = facts.Health;
        const float allShield = facts.AllShield;
        const float physicalPool =
            health + allShield + facts.PhysicalShield;
        const float magicalPool =
            health + allShield + facts.MagicalShield;
        const float mixedPool = health + allShield +
            (facts.PhysicalShield + facts.MagicalShield) * 0.5f;
        const float truePool = health + allShield;

        facts.EffectiveHealth = health + allShield +
            facts.PhysicalShield + facts.MagicalShield;
        facts.PhysicalEffectiveHealth = EffectiveHealthFromPool(
            physicalPool, physicalMultiplier);
        facts.MagicalEffectiveHealth = EffectiveHealthFromPool(
            magicalPool, magicalMultiplier);
        facts.MixedEffectiveHealth = EffectiveHealthFromPool(
            mixedPool, mixedMultiplier);
        facts.TrueEffectiveHealth = truePool;

        facts.HealthRegen = SafeNonNegative(target.HealthRegenRate());
        facts.MoveSpeed = SafeNonNegative(target.MoveSpeed());
        facts.AttackDamage = SafeNonNegative(target.AD());
        facts.AbilityPower = SafeNonNegative(target.AP());
        facts.BoundingRadius = SafeNonNegative(target.BoundingRadius());
        facts.AutoAttackDamage = canEvaluateDamage
            ? SafeNonNegative(::SDK::Damage::GetAutoAttackDamage(
                player, target, true))
            : 0.0f;
        facts.MagicalDamageEstimate =
            canEvaluateDamage && player.AP() > 0.0f
            ? SafeNonNegative(::SDK::Damage::CalculateDamage(
                player, target, ::SDK::DamageType::Magical, player.AP()))
            : 0.0f;

        static constexpr std::uint32_t kStunHash = ::SDK::Utils::HashName("stun");
        static constexpr std::uint32_t kRootHash = ::SDK::Utils::HashName("root");
        static constexpr std::uint32_t kSnareHash = ::SDK::Utils::HashName("snare");
        static constexpr std::uint32_t kCharmHash = ::SDK::Utils::HashName("charm");
        static constexpr std::uint32_t kFearHash = ::SDK::Utils::HashName("fear");
        static constexpr std::uint32_t kTauntHash = ::SDK::Utils::HashName("taunt");
        static constexpr std::uint32_t kSilenceHash = ::SDK::Utils::HashName("silence");

        static constexpr std::uint32_t kBardStasisHash = ::SDK::Utils::HashName("bardrstasis");
        static constexpr std::uint32_t kZhonyasHash = ::SDK::Utils::HashName("zhonyasringshield");
        static constexpr std::uint32_t kLissandraHash = ::SDK::Utils::HashName("lissandrarself");
        static constexpr std::uint32_t kVladPoolHash = ::SDK::Utils::HashName("vladimirsanguinepool");
        static constexpr std::uint32_t kFizzEHash = ::SDK::Utils::HashName("fizztrickslippery");

        const auto* buffSnapshot = ::CoreBuffs::GetOrBuildFrameBuffSnapshot(
            target.Address(), ::CoreBuffs::ResolveGameTime());
        if (buffSnapshot) {
            bool cc = false;
            bool stasis = false;
            for (int i = 0; i < buffSnapshot->count; ++i) {
                const auto& entry = buffSnapshot->entries[i];
                if (!entry.isActive) continue;
                const std::uint32_t h = entry.hash;
                if (!cc && (h == kStunHash || h == kRootHash || h == kSnareHash ||
                            h == kCharmHash || h == kFearHash || h == kTauntHash ||
                            h == kSilenceHash)) {
                    cc = true;
                }
                if (!stasis && (h == kBardStasisHash || h == kZhonyasHash ||
                                h == kLissandraHash || h == kVladPoolHash ||
                                h == kFizzEHash)) {
                    stasis = true;
                }
            }
            if (facts.Targetable) facts.IsCrowdControlled = cc;
            if (!facts.Targetable) facts.IsStasis = stasis;
        }
        if (facts.Targetable) {
            facts.IsDashing = target.IsDashing();
            facts.IsMoving = target.IsMoving();
            facts.IsChanneling = target.Spellbook().IsChanneling();
            facts.IsFacingSource = player.IsValid() &&
                ::SDK::Extensions::IsFacing(target, player);
        }
    }

    void BuildSnapshot(
        const ::SDK::KuroTargetSelector::TargetRequest& request) {
        using namespace ::SDK::KuroTargetSelector;
        const int tick = ::SDK::Variables::TickCount();
        if (snapshot_.Id != 0 && snapshot_.Tick == tick) return;

        snapshot_ = {};
        snapshot_.Id = ++snapshotSequence_;
        snapshot_.Tick = tick;
        if (menu_ && (snapshot_.Id % 30 == 1)) menu_->Refresh();
        const auto player = ::SDK::GameObjects::Player();
        snapshot_.PlayerPosition = player.IsValid() ? player.Position() : ::SDK::Vector3();
        (void)request;
        const ::SDK::Vector3 source = snapshot_.PlayerPosition;

        for (const auto& target : ::SDK::GameObjects::EnemyHeroes()) {
            if (snapshot_.Count >= snapshot_.Enemies.size()) break;
            if (!target.IsValid() || target.NetworkId() <= 0) continue;
            auto& facts = snapshot_.Enemies[snapshot_.Count++];
            PopulateTargetFacts(
                facts,
                target,
                player,
                static_cast<std::uint32_t>(snapshot_.Id),
                source);
            facts.Priority = menu_ ? menu_->Priority(facts.NetworkId) : 1;
        }
    }

    std::vector<::SDK::KuroTargetSelector::ProviderRegistry::Entry*> cachedProviderOrder_;
    std::uint64_t cachedProviderRevision_ = 0;
    std::unique_ptr<Menu> menu_;
    std::unique_ptr<Drawing> drawing_;
    ::SDK::KuroTargetSelector::ProviderRegistry providersRegistry_;
    ::SDK::KuroTargetSelector::TargetSnapshot snapshot_ = {};
    std::vector<::SDK::KuroTargetSelector::TargetDecision> lastDecisions_;
    std::uint64_t snapshotSequence_ = 0;
    std::uint64_t revision_ = 1;
    int incumbentNetworkId_ = 0;
    bool suspended_ = false;
    bool evaluating_ = false;
};

class KuroTargetSelectorPlugin final : public ::Plugins::IPlugin {
public:
    const char* GetName() const override { return "KuroTargetSelector"; }
    const char* GetInternalId() const override { return "core.kuro_target_selector"; }
    const char* GetAuthor() const override { return "Kuro"; }
    ::Plugins::PluginCategory GetCategory() const override {
        return ::Plugins::PluginCategory::Core;
    }
    bool AutoLoadByDefault() const override { return true; }

    void OnLoad() override {
        if (implementation_) return;

        DestroyMenu();
        menu_ = new ::SDK::Menu(
            GetInternalId(), "Kuro Target Selector", true);
        implementation_ = new KuroTargetSelectorService(menu_, true);
        menu_->Attach();

        const bool added = ::SDK::TargetSelector::AddTargetSelector(
            kImplementationName, implementation_);
        if (!added) {
            delete implementation_;
            implementation_ = nullptr;
            DestroyMenu();
            return;
        }

        // Add() lets the registry restore the persisted/default
        // implementation.  The service publishes itself from Resume() only
        // when Kuro is actually current.
        if (::SDK::TargetSelector::CurrentTargetSelectorName() == kImplementationName) {
            ::SDK::KuroTargetSelector::SetActiveService(implementation_);
        }
    }

    void OnUnload() override {
        if (::SDK::KuroTargetSelector::ActiveService() == implementation_) {
            ::SDK::KuroTargetSelector::SetActiveService(nullptr);
        }
        if (!implementation_) {
            DestroyMenu();
            return;
        }

        ::SDK::TargetSelector::RemoveTargetSelector(kImplementationName);
        delete implementation_;
        implementation_ = nullptr;
        DestroyMenu();
    }

    void OnRender() override {
        if (implementation_) implementation_->OnRender();
    }

private:
    static constexpr const char* kImplementationName = "Kuro";

    void DestroyMenu() {
        if (!menu_) return;
        ::SDK::UI::MenuManager::Instance().Remove(menu_);
        delete menu_;
        menu_ = nullptr;
    }

    ::SDK::Menu* menu_ = nullptr;
    KuroTargetSelectorService* implementation_ = nullptr;
};

} // namespace Plugins::KuroTargetSelector
