#pragma once

#include "../../IPlugin.h"
#include "../../PluginRegistry.h"
#include "../../../sdk/Wrappers/TargetSelector/TargetSelector.h"
#include "../../../sdk/Wrappers/Spells/Spell.h"
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
        const auto request = LegacyRequest(
            range, damageType, ignoreShields, from, ignoreChampions);
        const auto ranked = Rank(request);
        for (const auto& decision : ranked) {
            if (decision.Legal && !Ignored(decision.Target, ignoreChampions)) {
                CommitIncumbent(request, decision.Target.NetworkId());
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
        const auto request = LegacyRequest(
            range, damageType, ignoreShields, from, ignoreChampions);
        const auto ranked = Rank(request);
        result.reserve(ranked.size());
        for (const auto& decision : ranked) {
            if (decision.Legal && !Ignored(decision.Target, ignoreChampions)) {
                result.push_back(decision.Target);
            }
        }
        if (!result.empty()) CommitIncumbent(request, result.front().NetworkId());
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
                CommitIncumbent(request, decision.Target.NetworkId());
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
        const auto selectedSnapshot = std::find_if(
            snapshot_.Enemies.begin(),
            snapshot_.Enemies.begin() + snapshot_.Count,
            [selectedId](const TargetFacts& facts) {
                return selectedId > 0 && facts.NetworkId == selectedId;
            });
        const bool selectedActionable = selectedSnapshot !=
                snapshot_.Enemies.begin() + snapshot_.Count &&
            selectedSnapshot->Valid && selectedSnapshot->Visible &&
            selectedSnapshot->Targetable;
        // A locked target is a soft lease when fallback is allowed.  Only an
        // explicit preferred/selected target may bypass the score entirely.
        const int manualPreferredId = request.PreferredTargetId != 0
            ? request.PreferredTargetId
            : (request.RespectManualSelection && menu_ &&
               menu_->PreferSelectedTarget() ? selectedId : 0);
        const int requiredId = request.RequiredTargetId != 0
            ? request.RequiredTargetId
            : (!request.AllowFallback ? request.LockedTargetId : 0);
        // Keep the manual selection identity through fog, but suspend its hard
        // lock while it cannot be acted on so OnlySelected does not suppress
        // every visible fallback target.
        const bool onlySelected = menu_ && menu_->OnlySelectedTarget() &&
            selectedActionable;
        const int requestIncumbentId = IncumbentFor(request);
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
            PopulateRequestDamageFacts(request, workingFacts, player);
            PopulatePredictionFacts(request, workingFacts);
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
                KuroTargetSelectorPolicy::ProfileFor(request);
            const TargetProfile menuProfile = menu_
                ? menu_->Profile() : TargetProfile::General;
            const TargetProfile activeProfile = menu_ &&
                    !menu_->AutomaticProfile()
                ? menuProfile : requestedProfile;
            decision.Score = KuroTargetSelectorPolicy::BuildScoreForProfile(
                request,
                workingFacts,
                menu_ ? menu_->Priority(facts.NetworkId) : 1,
                requestIncumbentId,
                decision.Breakdown,
                activeProfile,
                menu_ ? menu_->Stickiness() : -1.0f);
            if (manualPreferredId != 0 &&
                manualPreferredId == facts.NetworkId) {
                // Manual preference must follow the menu Stickiness slider
                // instead of a fixed overwhelming constant.  A low setting
                // lets a clearly better scoring target take over instead of
                // the selected hero sticking forever.
                const float manualGrip = menu_
                    ? std::clamp(menu_->Stickiness() * 2.0f, 0.0f, 400.0f)
                    : 160.0f;
                if (manualGrip > 0.0f) {
                    decision.Breakdown.Add(
                        "manual-preference", "manual target preference",
                        manualGrip, 0.0f, 400.0f);
                    decision.Score = decision.Breakdown.Total;
                }
            }
            if (request.AllowFallback && request.LockedTargetId != 0 &&
                request.LockedTargetId == facts.NetworkId &&
                request.LockedTargetId != manualPreferredId &&
                request.LockedTargetId != requestIncumbentId) {
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
            // snapshot. Feed candidate-specific prediction back into the
            // route gate so collision and MinimumHitChance are enforced after
            // (not before) FsPred has evaluated this hero.
            TargetRequest finalRequest = request;
            if (workingFacts.PredictionEvaluated) {
                finalRequest.Route.PredictionAvailable =
                    workingFacts.PredictionAvailable;
                finalRequest.Route.PredictionCollides =
                    workingFacts.PredictionCollides;
                finalRequest.Route.PredictionHitChance =
                    workingFacts.PredictionHitChance;
                if (workingFacts.PredictionCastPosition.IsValid() &&
                    !workingFacts.PredictionCastPosition.IsZero()) {
                    finalRequest.Route.Destination =
                        workingFacts.PredictionCastPosition;
                    finalRequest.Route.Prediction =
                        workingFacts.PredictionCastPosition;
                }
            }
            // ValidateExecution keeps the final live race-safety gate.
            const auto finalGate = KuroTargetActionGate::Evaluate(
                finalRequest, workingFacts.Target, facts.Targetable);
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
            [this](const TargetDecision& lhs,
                                const TargetDecision& rhs) {
                if (lhs.Legal != rhs.Legal) return lhs.Legal > rhs.Legal;
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
            const auto selected = menu_->Selected();
            const bool selectedActionable = selected.IsValid() &&
                selected.NetworkId() > 0 && selected.IsVisible() &&
                selected.IsTargetable() &&
                (!selected.IsDead() || selected.IsZombie());
            if (selectedActionable && selected.NetworkId() != targetId) {
                return false;
            }
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

    struct IncumbentEntry {
        std::uint64_t Key = 0;
        int NetworkId = 0;
    };

    static std::uint64_t IncumbentKey(
        const ::SDK::KuroTargetSelector::TargetRequest& request) {
        // FNV-1a-style mixing gives each requester/action family an independent
        // stickiness lane without retaining borrowed pointers themselves.
        std::uint64_t key = 1469598103934665603ull;
        const auto mix = [&key](std::uint64_t value) {
            key ^= value;
            key *= 1099511628211ull;
        };
        mix(request.RequesterId);
        mix(static_cast<std::uint64_t>(request.Purpose));
        mix(static_cast<std::uint64_t>(request.Route.Kind));
        mix(static_cast<std::uint64_t>(request.Damage.Type));
        mix(request.HasProfileHint
            ? static_cast<std::uint64_t>(request.ProfileHint) + 1ull
            : 0ull);
        const auto* action = request.Route.ActionSpell
            ? request.Route.ActionSpell
            : request.Route.PredictionSpell;
        mix(static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(action)));
        return key;
    }

    int IncumbentFor(
        const ::SDK::KuroTargetSelector::TargetRequest& request) const {
        const std::uint64_t key = IncumbentKey(request);
        const auto found = std::find_if(
            incumbentLanes_.begin(), incumbentLanes_.end(),
            [key](const IncumbentEntry& entry) { return entry.Key == key; });
        return found != incumbentLanes_.end() ? found->NetworkId : 0;
    }

    void CommitIncumbent(
        const ::SDK::KuroTargetSelector::TargetRequest& request,
        int networkId) {
        if (networkId <= 0) return;
        const std::uint64_t key = IncumbentKey(request);
        auto found = std::find_if(
            incumbentLanes_.begin(), incumbentLanes_.end(),
            [key](const IncumbentEntry& entry) { return entry.Key == key; });
        if (found != incumbentLanes_.end()) {
            found->NetworkId = networkId;
        } else {
            if (incumbentLanes_.size() >= 64) {
                incumbentLanes_.erase(incumbentLanes_.begin());
            }
            incumbentLanes_.push_back({ key, networkId });
        }
        // Preserve the legacy aggregate state for UI/ABI compatibility only.
        incumbentNetworkId_ = networkId;
    }

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

    struct PredictionSample {
        ::SDK::Vector3 CastPosition = {};
        ::SDK::Vector3 UnitPosition = {};
        int HitChance = static_cast<int>(::SDK::HitChance::None);
        bool Available = false;
        bool Collides = false;
    };

    struct PredictionCacheEntry {
        int NetworkId = 0;
        std::uint32_t RequesterId = 0;
        const ::SDK::Spell* Spell = nullptr;
        ::SDK::SpellType Type = ::SDK::SpellType::None;
        ::SDK::Vector3 Source = {};
        ::SDK::Vector3 RangeCheckFrom = {};
        float Range = 0.0f;
        float Delay = 0.0f;
        float Radius = 0.0f;
        float Speed = 0.0f;
        float MaxCollisionCount = 0.0f;
        std::uint32_t CollisionObjectFlags = 0;
        bool Collision = false;
        bool AddHitBox = true;
        bool ChoiceCloserPosition = false;
        PredictionSample Sample = {};
    };

    static bool IsPredictionSkillshotType(::SDK::SpellType type) {
        return ::SDK::IsLineSpellType(type) ||
               ::SDK::IsCircleSpellType(type) ||
               ::SDK::IsConeSpellType(type) ||
               ::SDK::IsArcSpellType(type);
    }

    static bool SamePredictionFloat(float left, float right) {
        if (left == right) return true;
        return std::isfinite(left) && std::isfinite(right) &&
               std::fabs(left - right) <= 0.00001f;
    }

    static bool SamePredictionSource(const ::SDK::Vector3& left,
                                     const ::SDK::Vector3& right) {
        return SamePredictionFloat(left.x, right.x) &&
               SamePredictionFloat(left.y, right.y) &&
               SamePredictionFloat(left.z, right.z);
    }

    PredictionSample EvaluateCandidatePrediction(
        const ::SDK::KuroTargetSelector::TargetRequest& request,
        const ::SDK::KuroTargetSelector::TargetFacts& facts) {
        using namespace ::SDK::KuroTargetSelector;

        const auto& route = request.Route;
        const auto* spell = route.PredictionSpell;
        const ::SDK::Vector3 source = spell &&
                spell->From.IsValid() && !spell->From.IsZero()
            ? spell->From
            : ResolveRequestSource(request, ::SDK::GameObjects::Player());
        const float range = spell
            ? spell->CurrentRange()
            : (request.Range > 0.0f ? request.Range : FLT_MAX);
        const float delay = spell ? spell->Delay : route.Delay;
        const float radius = spell
            ? spell->Width
            : std::max(0.0f, route.PredictionRadius);
        const float speed = spell ? spell->Speed : route.ProjectileSpeed;
        const float maxCollisionCount = spell
            ? spell->MaxCollisionCount
            : route.PredictionMaxCollisionCount;
        const bool collision = spell
            ? spell->Collision
            : route.CollisionCheck;
        const bool addHitBox = spell
            ? spell->AddHitBox
            : route.PredictionAddHitBox;
        const ::SDK::SpellType predictionType = spell
            ? ::SDK::ToSpellType(spell->Type)
            : route.PredictionType;
        const ::SDK::Vector3 predictionFrom = spell ? spell->From : source;
        const ::SDK::Vector3 rangeCheckFrom = spell
            ? spell->RangeCheckFrom
            : source;
        const bool choiceCloserPosition = spell &&
            spell->PredictionCloserPosition;
        const std::uint32_t collisionObjectFlags = spell
            ? static_cast<std::uint32_t>(spell->CollisionObjects.ToFlags())
            : 0u;

        const int tick = ::SDK::Variables::TickCount();
        if (predictionCacheTick_ != tick) {
            predictionCacheTick_ = tick;
            predictionCache_.clear();
        }
        for (const auto& cached : predictionCache_) {
            if (cached.NetworkId == facts.NetworkId &&
                cached.RequesterId == request.RequesterId &&
                cached.Spell == spell &&
                cached.Type == predictionType &&
                cached.Collision == collision &&
                cached.AddHitBox == addHitBox &&
                cached.ChoiceCloserPosition == choiceCloserPosition &&
                cached.CollisionObjectFlags == collisionObjectFlags &&
                SamePredictionSource(cached.Source, predictionFrom) &&
                SamePredictionSource(cached.RangeCheckFrom, rangeCheckFrom) &&
                SamePredictionFloat(cached.Range, range) &&
                SamePredictionFloat(cached.Delay, delay) &&
                SamePredictionFloat(cached.Radius, radius) &&
                SamePredictionFloat(cached.Speed, speed) &&
                SamePredictionFloat(
                    cached.MaxCollisionCount, maxCollisionCount)) {
                return cached.Sample;
            }
        }

        ::SDK::PredictionInput input{};
        input.Unit = facts.Target;
        input.Delay = std::max(0.0f, delay);
        input.Radius = radius;
        input.Speed = speed > 0.0f ? speed : FLT_MAX;
        input.From = predictionFrom;
        input.Range = range;
        input.Collision = collision;
        input.Spell = spell;
        input.AoE = false;
        input.AddHitBox = addHitBox;
        input.MaxCollisionCount = maxCollisionCount;
        if (spell) {
            input.SetType(spell->Type);
            input.RangeCheckFrom = rangeCheckFrom;
            input.ChoiceCloserPosition = choiceCloserPosition;
            input.CollisionObjects = spell->CollisionObjects;
        } else {
            input.SetType(route.PredictionType);
            input.RangeCheckFrom = rangeCheckFrom;
        }
        // Keep the runtime Spell pointer on the input so FsPred can apply its
        // slot calibration, but force single-target evaluation: target
        // ranking only needs confidence and must not run one AoE search per
        // candidate.
        const ::SDK::PredictionOutput output =
            ::SDK::Prediction::GetPrediction(input);

        PredictionSample sample{};
        const ::SDK::HitChance rawChance = output.Hitchance;
        sample.Collides = rawChance == ::SDK::HitChance::Collision;
        const ::SDK::HitChance originChance = sample.Collides
            ? output.GetOriginHitchance()
            : rawChance;
        sample.HitChance = static_cast<int>(
            sample.Collides && originChance == ::SDK::HitChance::None
                ? ::SDK::HitChance::Collision
                : originChance);
        sample.Available = sample.Collides ||
            sample.HitChance >= static_cast<int>(::SDK::HitChance::Low);
        sample.CastPosition = output.GetCastPosition();
        sample.UnitPosition = output.GetUnitPosition();

        if (predictionCache_.size() >= 96) {
            predictionCache_.erase(predictionCache_.begin());
        }
        predictionCache_.push_back({
            facts.NetworkId,
            request.RequesterId,
            spell,
            predictionType,
            predictionFrom,
            rangeCheckFrom,
            range,
            delay,
            radius,
            speed,
            maxCollisionCount,
            collisionObjectFlags,
            collision,
            addHitBox,
            choiceCloserPosition,
            sample
        });
        return sample;
    }

    void PopulatePredictionFacts(
        const ::SDK::KuroTargetSelector::TargetRequest& request,
        ::SDK::KuroTargetSelector::TargetFacts& facts) {
        using namespace ::SDK::KuroTargetSelector;

        facts.PredictionCastPosition = {};
        facts.PredictionUnitPosition = {};
        facts.PredictionHitChance = static_cast<int>(::SDK::HitChance::None);
        facts.PredictionEvaluated = false;
        facts.PredictionAvailable = false;
        facts.PredictionCollides = false;

        const auto& route = request.Route;
        const bool suppliedPrediction =
            route.PredictionAvailable || route.PredictionCollides ||
            route.PredictionHitChance !=
                static_cast<int>(::SDK::HitChance::None);
        const bool suppliedForCandidate =
            route.IntendedTargetId == 0 ||
            route.IntendedTargetId == facts.NetworkId;
        if (suppliedPrediction && suppliedForCandidate) {
            facts.PredictionCastPosition =
                route.Destination.IsValid() && !route.Destination.IsZero()
                    ? route.Destination
                    : route.Prediction;
            facts.PredictionUnitPosition = route.Prediction;
            facts.PredictionHitChance = route.PredictionHitChance;
            facts.PredictionEvaluated = true;
            facts.PredictionAvailable = route.PredictionAvailable;
            facts.PredictionCollides = route.PredictionCollides;
            return;
        }

        if (request.Phase != DecisionPhase::Planning ||
            (!route.PredictionSpell &&
             !IsPredictionSkillshotType(route.PredictionType))) {
            return;
        }

        const PredictionSample sample =
            EvaluateCandidatePrediction(request, facts);
        facts.PredictionCastPosition = sample.CastPosition;
        facts.PredictionUnitPosition = sample.UnitPosition;
        facts.PredictionHitChance = sample.HitChance;
        facts.PredictionEvaluated = true;
        facts.PredictionAvailable = sample.Available;
        facts.PredictionCollides = sample.Collides;
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

    static float RawEquivalentDamage(float dealt, float multiplier) {
        const float safeDealt = SafeNonNegative(dealt);
        if (!std::isfinite(multiplier) || multiplier <= 0.0f) {
            return safeDealt;
        }
        return SafeNonNegative(safeDealt / multiplier);
    }

    static float EffectiveHealthFromPool(float pool, float multiplier) {
        if (pool <= 0.0f) return 0.0f;
        if (!std::isfinite(multiplier) || multiplier <= 0.0f) {
            return FLT_MAX;
        }
        return pool / multiplier;
    }

    static void PopulateRequestDamageFacts(
        const ::SDK::KuroTargetSelector::TargetRequest& request,
        ::SDK::KuroTargetSelector::TargetFacts& facts,
        const ::SDK::AIHeroClient& player) {
        using namespace ::SDK::KuroTargetSelector;

        if (!facts.Targetable || !player.IsValid() ||
            !facts.Target.IsValid()) {
            return;
        }

        const bool concreteAction = request.Route.ActionSpell != nullptr ||
            request.Route.PredictionSpell != nullptr ||
            request.Damage.RawDamage > 0.0f ||
            request.Route.Kind == RouteKind::AutoAttack ||
            request.Route.Kind == RouteKind::UnitProjectile ||
            request.Route.Kind == RouteKind::SkillshotProjectile ||
            request.Route.Kind == RouteKind::ChargedProjectile;

        if (facts.DistanceToSource > 2500.0f && concreteAction) {
            float physicalMultiplier = 1.0f;
            float magicalMultiplier = 1.0f;
            if (request.Damage.Type == ::SDK::DamageType::Physical ||
                request.Damage.Type == ::SDK::DamageType::Mixed) {
                physicalMultiplier = DamageMultiplier(
                    player, facts.Target, ::SDK::DamageType::Physical);
            }
            if (request.Damage.Type == ::SDK::DamageType::Magical ||
                request.Damage.Type == ::SDK::DamageType::Mixed) {
                magicalMultiplier = DamageMultiplier(
                    player, facts.Target, ::SDK::DamageType::Magical);
            }
            const float mixedMultiplier =
                physicalMultiplier > 0.0f && magicalMultiplier > 0.0f
                ? (physicalMultiplier + magicalMultiplier) * 0.5f
                : std::max(physicalMultiplier, magicalMultiplier);
            const float health = facts.Health;
            const float allShield = facts.AllShield;
            facts.PhysicalEffectiveHealth = EffectiveHealthFromPool(
                health + allShield + facts.PhysicalShield,
                physicalMultiplier);
            facts.MagicalEffectiveHealth = EffectiveHealthFromPool(
                health + allShield + facts.MagicalShield,
                magicalMultiplier);
            facts.MixedEffectiveHealth = EffectiveHealthFromPool(
                health + allShield +
                    (facts.PhysicalShield + facts.MagicalShield) * 0.5f,
                mixedMultiplier);
            facts.TrueEffectiveHealth = health + allShield;

            if (request.Route.Kind == RouteKind::AutoAttack) {
                facts.AutoAttackDamage = RawEquivalentDamage(
                    ::SDK::Damage::GetAutoAttackDamage(
                        player, facts.Target, true),
                    physicalMultiplier);
            }
            facts.MagicalDamageEstimate = std::max(
                75.0f, SafeNonNegative(player.AP()));
        }

        if (request.Route.ActionSpell) {
            const float dealt = SafeNonNegative(
                request.Route.ActionSpell->GetDamage(facts.Target));
            // Spell::GetDamage returns post-mitigation damage, whereas policy
            // damage is compared with effective health. Convert it back to a
            // raw-equivalent amount using the request's mitigation sample.
            const float mitigation =
                KuroTargetSelectorPolicy::MitigationMultiplierFor(
                    request, facts);
            facts.ActionDamageEstimate = SafeNonNegative(dealt * mitigation);
        }
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

        // Composition around the target: friendly heroes whose auto attack
        // range covers the target (they can follow up the damage) and enemy
        // heroes that can answer it (they contest the kill window).  Using
        // each hero's real attack range keeps the measure champion-specific
        // instead of a fixed world-space radius.
        int alliesNear = 0;
        int enemiesNear = 0;
        for (const auto& ally : ::SDK::GameObjects::AllyHeroes()) {
            if (!ally.IsValid() || ally.IsDead() ||
                ally.NetworkId() == player.NetworkId()) {
                continue;
            }
            const float coverRange =
                ::SDK::Utils::AutoAttack::GetRealAutoAttackRange(
                    ally, target);
            if (ally.Position().DistanceSqr2D(facts.Position) <=
                coverRange * coverRange) {
                ++alliesNear;
            }
        }
        for (const auto& enemy : ::SDK::GameObjects::EnemyHeroes()) {
            if (!enemy.IsValid() || enemy.IsDead() ||
                enemy.NetworkId() == facts.NetworkId) {
                continue;
            }
            const float threatRange =
                ::SDK::Utils::AutoAttack::GetRealAutoAttackRange(
                    enemy, target);
            if (enemy.Position().DistanceSqr2D(facts.Position) <=
                threatRange * threatRange) {
                ++enemiesNear;
            }
        }
        facts.AlliesNearTarget = alliesNear;
        facts.EnemiesNearTarget = enemiesNear;

        // The shared snapshot deliberately caps expensive damage simulations.
        // Concrete long-range requests fill their own missing data lazily in
        // PopulateRequestDamageFacts().
        const bool canEvaluateDamage = facts.Targetable && player.IsValid() &&
            facts.DistanceToSource <= 2500.0f;
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
            ? RawEquivalentDamage(
                ::SDK::Damage::GetAutoAttackDamage(player, target, true),
                physicalMultiplier)
            : 0.0f;
        // Generic magical requests have no spell slot from which to read base
        // damage. A small raw baseline prevents AP=0 champions from becoming
        // indistinguishable while concrete actions use exact spell damage.
        facts.MagicalDamageEstimate = canEvaluateDamage
            ? std::max(75.0f, SafeNonNegative(player.AP()))
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
            bool slow = false;
            bool knockup = false;
            bool suppressed = false;
            bool grounded = false;
            bool silenced = false;
            bool blinded = false;
            bool stasis = false;
            bool hasMark = false;
            int debuffCount = 0;
            float debuffScore = 0.0f;

            for (int i = 0; i < buffSnapshot->count; ++i) {
                const auto& entry = buffSnapshot->entries[i];
                if (!entry.isActive) continue;
                const std::uint32_t h = entry.hash;
                const std::uint8_t t = entry.type;
                const char* name = entry.name;

                if (!stasis && (h == kBardStasisHash || h == kZhonyasHash ||
                                h == kLissandraHash || h == kVladPoolHash ||
                                h == kFizzEHash)) {
                    stasis = true;
                }

                // Hard CC types: Stun (5), Silence (7/13), Taunt (8/14), Berserk (9/15), Polymorph (10/16), Snare (12/18), Fear (22/28), Charm (23/29), Flee (29/35), Asleep (35/41)
                const bool isHardCC = (t == 5 || t == 7 || t == 8 || t == 9 || t == 10 || t == 12 ||
                                       t == 13 || t == 14 || t == 15 || t == 16 || t == 18 || t == 22 ||
                                       t == 23 || t == 28 || t == 29 || t == 35 || t == 41 ||
                                       h == kStunHash || h == kRootHash || h == kSnareHash ||
                                       h == kCharmHash || h == kFearHash || h == kTauntHash || h == kSilenceHash);
                if (isHardCC) {
                    cc = true;
                    if (t == 7 || t == 13 || h == kSilenceHash) silenced = true;
                    debuffScore += 45.0f;
                    debuffCount++;
                }

                // Knockup / Knockback / Airborne: Type 30 (Knockup), 31 (Knockback), 36/37
                const bool isAirborne = (t == 30 || t == 31 || t == 36 || t == 37 ||
                                         (name && (strstr(name, "knockup") || strstr(name, "knockback") || strstr(name, "airborne"))));
                if (isAirborne) {
                    knockup = true;
                    debuffScore += 55.0f;
                    debuffCount++;
                }

                // Suppression: Type 25 or 31
                const bool isSuppression = (t == 25 || (name && strstr(name, "suppress")));
                if (isSuppression) {
                    suppressed = true;
                    debuffScore += 60.0f;
                    debuffCount++;
                }

                // Slow / Grounded / AttackSpeedSlow: Type 11 (Slow), 17, 19, 33 (Grounded), 39
                const bool isSlow = (t == 11 || t == 17 || t == 19 || (name && strstr(name, "slow")));
                if (isSlow) {
                    slow = true;
                    debuffScore += 20.0f;
                    debuffCount++;
                }

                const bool isGrounded = (t == 33 || t == 39 || (name && strstr(name, "grounded")));
                if (isGrounded) {
                    grounded = true;
                    debuffScore += 25.0f;
                    debuffCount++;
                }

                const bool isBlind = (t == 26 || t == 32 || (name && strstr(name, "blind")));
                if (isBlind) {
                    blinded = true;
                    debuffScore += 15.0f;
                    debuffCount++;
                }

                // Marks / Stack debuffs / Vulnerable debuffs
                if (name && name[0]) {
                    if (strstr(name, "tristanae") || strstr(name, "kalistaexpunge") ||
                        strstr(name, "vaynecounter") || strstr(name, "kaisa") ||
                        strstr(name, "zedr") || strstr(name, "hemoplague") ||
                        strstr(name, "dariushemo") || strstr(name, "brandablaze") ||
                        strstr(name, "teemopoisons") || strstr(name, "fioramark") ||
                        strstr(name, "varusw") || strstr(name, "akshan") ||
                        strstr(name, "shred") || strstr(name, "blackcleaver") ||
                        strstr(name, "abyssal") || strstr(name, "grievous") ||
                        strstr(name, "vulnerable") || strstr(name, "debuff")) {
                        hasMark = true;
                        const float stacksBonus = std::clamp(static_cast<float>(entry.stacks), 1.0f, 5.0f);
                        debuffScore += 30.0f * (1.0f + 0.25f * (stacksBonus - 1.0f));
                        debuffCount++;
                    }
                }
            }

            if (facts.Targetable) {
                facts.IsCrowdControlled = cc;
                facts.IsSlowed = slow;
                facts.IsKnockedUp = knockup;
                facts.IsSuppressed = suppressed;
                facts.IsGrounded = grounded;
                facts.IsSilenced = silenced;
                facts.IsBlinded = blinded;
                facts.HasVulnerableMark = hasMark;
                facts.DebuffCount = debuffCount;
                facts.DebuffScore = debuffScore;
            }
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
    std::vector<PredictionCacheEntry> predictionCache_;
    std::vector<IncumbentEntry> incumbentLanes_;
    std::uint64_t snapshotSequence_ = 0;
    std::uint64_t revision_ = 1;
    int incumbentNetworkId_ = 0;
    int predictionCacheTick_ = -1;
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

        if (!::SDK::TargetSelector::SetTargetSelector(kImplementationName)) {
            ::SDK::TargetSelector::RemoveTargetSelector(kImplementationName);
            delete implementation_;
            implementation_ = nullptr;
            DestroyMenu();
            return;
        }

        SetSdkTargetSelectorLoaded(false);
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
        SetSdkTargetSelectorLoaded(true);
    }

    void OnRender() override {
        if (implementation_) implementation_->OnRender();
    }

    bool LoadSucceeded() const override {
        return implementation_ &&
               ::SDK::TargetSelector::GetTargetSelector(kImplementationName) == implementation_ &&
               ::SDK::TargetSelector::Implementation() == implementation_;
    }

private:
    static constexpr const char* kImplementationName = "Kuro";

    static void SetSdkTargetSelectorLoaded(bool loaded) {
        const int idx = ::PluginRegistry::FindByInternalId("targetselector");
        if (idx >= 0 && ::PluginRegistry::HasRuntime(idx)) {
            if (loaded) {
                ::PluginRegistry::LoadPlugin(idx);
            } else {
                ::PluginRegistry::UnloadPlugin(idx);
            }
            return;
        }

        if (auto* impl = ::SDK::TargetSelector::GetTargetSelector("SDK")) {
            if (loaded) {
                impl->Resume();
            } else {
                impl->Suspend();
            }
        }
        if (idx >= 0) {
            ::PluginRegistry::Plugins[idx].Loaded = loaded;
        }
    }

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
