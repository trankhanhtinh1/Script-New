#pragma once

#include "../../IPlugin.h"
#include "../../../sdk/Wrappers/TargetSelector/TargetSelector.h"
#include "../../../sdk/GameObjects/GameObjects.h"

#include "KuroTargetActionGate.h"
#include "KuroTargetSelectorDrawing.h"
#include "KuroTargetSelectorMenu.h"
#include "KuroTargetSelectorPolicy.h"
#include "KuroTargetSelectorProviders.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace Plugins::KuroTargetSelector {

class KuroTargetSelectorService final : public ::SDK::ITargetSelector,
                                        public ::SDK::KuroTargetSelector::IKuroTargetSelector {
public:
    explicit KuroTargetSelectorService(
        ::SDK::Menu* parent, bool parentIsRoot = false)
        : menu_(new Menu(parent, parentIsRoot)), drawing_(new Drawing(menu_)) {
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
        if (drawing_) {
            delete drawing_;
            drawing_ = nullptr;
        }
        if (menu_) {
            delete menu_;
            menu_ = nullptr;
        }
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
        if (evaluating_) return {};
        evaluating_ = true;

        BuildSnapshot(request);
        std::vector<TargetDecision> result;
        result.reserve(snapshot_.Count);

        const int selectedId = menu_ ? menu_->Selected().NetworkId() : 0;
        const int preferredId = request.PreferredTargetId != 0
            ? request.PreferredTargetId
            : (request.RespectManualSelection && menu_ &&
               menu_->PreferSelectedTarget() ? selectedId
               : (request.AllowFallback ? request.LockedTargetId : 0));
        const int requiredId = request.RequiredTargetId != 0
            ? request.RequiredTargetId
            : (!request.AllowFallback ? request.LockedTargetId : 0);
        const bool onlySelected = menu_ && menu_->OnlySelectedTarget();

        for (std::size_t i = 0; i < snapshot_.Count; ++i) {
            const TargetFacts& facts = snapshot_.Enemies[i];
            TargetDecision decision{};
            decision.Target = facts.Target;
            decision.Route = request.Route.Kind;
            decision.SnapshotId = snapshot_.Id;

            const auto gate = KuroTargetActionGate::Evaluate(
                request, facts.Target);
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
            const ::SDK::Vector3 source = request.Route.Start.IsValid() &&
                    !request.Route.Start.IsZero()
                ? request.Route.Start
                : (request.Source.IsValid() && !request.Source.IsZero()
                    ? request.Source : snapshot_.PlayerPosition);
            workingFacts.DistanceToSource = source.IsValid() && !source.IsZero()
                ? source.Distance(workingFacts.Target.Position())
                : workingFacts.DistanceToSource;
            workingFacts.Distance = workingFacts.DistanceToSource;
            TargetProviderContext context{};
            context.Request = &request;
            context.Target = &workingFacts.Target;
            context.Facts = &workingFacts;
            context.Snapshot = &snapshot_;

            std::vector<ProviderRegistry::Entry*> providerOrder;
            for (auto& entry : providersRegistry_.MutableEntries()) {
                // BaseSafety is reserved for the core action gate.  Plugin
                // providers can add facts, constraints, and score terms but
                // cannot replace that hard safety boundary.
                if (entry.Provider.Band != ProviderPriorityBand::BaseSafety) {
                    providerOrder.push_back(&entry);
                }
            }
            std::stable_sort(providerOrder.begin(), providerOrder.end(),
                [](const auto* lhs, const auto* rhs) {
                    return static_cast<int>(lhs->Provider.Band) <
                           static_cast<int>(rhs->Provider.Band);
                });

            bool providerRejected = false;
            for (auto* provider : providerOrder) {
                if (!providersRegistry_.BuildFacts(
                        *provider, request, workingFacts.Target, workingFacts)) {
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
            const TargetProfile activeProfile = menuProfile !=
                    TargetProfile::General
                ? menuProfile : requestedProfile;
            decision.Score = KuroTargetSelectorPolicy::BuildScoreForProfile(
                request,
                workingFacts,
                menu_ ? menu_->Priority(facts.NetworkId) : 1,
                incumbentNetworkId_,
                decision.Breakdown,
                activeProfile);
            if (preferredId != 0 && preferredId == facts.NetworkId) {
                decision.Breakdown.Add(
                    "manual-preference", "manual target preference",
                    menu_ ? menu_->Stickiness() : 80.0f, 0.0f, 240.0f);
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
            [this, preferredId](const TargetDecision& lhs,
                                const TargetDecision& rhs) {
                if (lhs.Legal != rhs.Legal) return lhs.Legal > rhs.Legal;
                const bool lhsPreferred = preferredId != 0 &&
                    lhs.Target.NetworkId() == preferredId;
                const bool rhsPreferred = preferredId != 0 &&
                    rhs.Target.NetworkId() == preferredId;
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
        if (menu_) menu_->DrawDiagnostics(snapshot_, lastDecisions_);
        evaluating_ = false;
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
        const ::SDK::KuroTargetSelector::ProviderToken token =
            providersRegistry_.Register(provider);
        if (token) ++revision_;
        return token;
    }

    bool UnregisterProvider(
        ::SDK::KuroTargetSelector::ProviderToken token) override {
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
        auto execution = request;
        execution.Phase = ::SDK::KuroTargetSelector::DecisionPhase::Execution;
        const auto gate = ::SDK::KuroTargetSelector::KuroTargetActionGate::Evaluate(
            execution, target);
        if (!gate.Legal || target.IsClone() ||
            (menu_ && menu_->IsBlacklisted(target.NetworkId()))) {
            return false;
        }

        // Execution is the second half of the selector/provider contract.
        // A champion provider may observe a buff changing after planning
        // (Kindred R is the important example) and must be able to reject the
        // live action without making callers re-rank the whole target list.
        using namespace ::SDK::KuroTargetSelector;
        TargetFacts facts{};
        const auto player = ::SDK::GameObjects::Player();
        facts.Target = target;
        facts.SnapshotId = static_cast<std::uint32_t>(snapshot_.Id);
        facts.NetworkId = target.NetworkId();
        facts.Level = target.Level();
        facts.Position = target.Position();
        facts.ServerPosition = target.ServerPosition();
        facts.Direction = target.Direction();
        facts.Health = target.Health();
        facts.MaxHealth = target.MaxHealth();
        facts.AllShield = target.AllShield();
        facts.EffectiveHealth = facts.Health + facts.AllShield;
        facts.HealthRegen = target.HealthRegenRate();
        facts.DistanceToSource = player.IsValid()
            ? player.Position().Distance(target.Position()) : 0.0f;
        facts.Distance = facts.DistanceToSource;
        facts.MoveSpeed = target.MoveSpeed();
        facts.AttackDamage = target.AD();
        facts.AbilityPower = target.AP();
        facts.BoundingRadius = target.BoundingRadius();
        facts.AutoAttackDamage = target.AD();
        facts.Valid = target.IsValid();
        facts.Dead = target.IsDead();
        facts.Visible = target.IsVisible();
        facts.Targetable = target.IsTargetable();
        facts.Invulnerable = target.IsInvulnerable();
        facts.IsZombie = target.IsZombie();
        facts.IsClone = target.IsClone();
        facts.IsDashing = target.IsDashing();
        facts.IsMoving = target.IsMoving();
        facts.IsChanneling = target.Spellbook().IsChanneling();
        facts.IsCrowdControlled = target.HasBuff("stun") ||
            target.HasBuff("root") || target.HasBuff("snare") ||
            target.HasBuff("charm") || target.HasBuff("fear") ||
            target.HasBuff("taunt") || target.HasBuff("silence");
        facts.IsFacingSource = player.IsValid() &&
            ::SDK::Extensions::IsFacing(target, player);
        facts.IsStasis = target.HasBuff("bardrstasis") ||
            target.HasBuff("zhonyasringshield") ||
            target.HasBuff("lissandrarself") ||
            target.HasBuff("vladimirsanguinepool") ||
            target.HasBuff("fizztrickslippery");

        std::vector<::SDK::KuroTargetSelector::ProviderRegistry::Entry*>
            providerOrder;
        for (auto& entry : providersRegistry_.MutableEntries()) {
            if (entry.Provider.Band != ProviderPriorityBand::BaseSafety) {
                providerOrder.push_back(&entry);
            }
        }
        std::stable_sort(providerOrder.begin(), providerOrder.end(),
            [](const auto* lhs, const auto* rhs) {
                return static_cast<int>(lhs->Provider.Band) <
                    static_cast<int>(rhs->Provider.Band);
            });

        TargetProviderContext context{};
        context.Request = &execution;
        context.Target = &facts.Target;
        context.Facts = &facts;
        context.Snapshot = &snapshot_;
        for (auto* provider : providerOrder) {
            if (!providersRegistry_.BuildFacts(
                    *provider, execution, target, facts)) {
                return false;
            }
            if (providersRegistry_.Validate(*provider, context) !=
                    RejectReason::None) {
                return false;
            }
        }
        return true;
    }

    std::vector<::SDK::KuroTargetSelector::ProviderDiagnostic>
    GetProviderDiagnostics() const override {
        return providersRegistry_.Diagnostics();
    }

    void OnRender() {
        if (drawing_) drawing_->Draw(GetSelectionState(), lastDecisions_);
    }

private:
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
        request.Route.Kind = RouteKind::NonProjectile;
        request.Route.Start = from;
        request.Route.TargetableAtExecution = true;
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

    void BuildSnapshot(
        const ::SDK::KuroTargetSelector::TargetRequest& request) {
        using namespace ::SDK::KuroTargetSelector;
        const int tick = ::SDK::Variables::TickCount();
        if (snapshot_.Id != 0 && snapshot_.Tick == tick) return;

        snapshot_ = {};
        snapshot_.Id = ++snapshotSequence_;
        snapshot_.Tick = tick;
        if (menu_) menu_->Refresh();
        const auto player = ::SDK::GameObjects::Player();
        snapshot_.PlayerPosition = player.IsValid() ? player.Position() : ::SDK::Vector3();
        (void)request;
        const ::SDK::Vector3 source = snapshot_.PlayerPosition;

        for (const auto& target : ::SDK::GameObjects::EnemyHeroes()) {
            if (snapshot_.Count >= snapshot_.Enemies.size()) break;
            auto& facts = snapshot_.Enemies[snapshot_.Count++];
            facts.Target = target;
            facts.SnapshotId = static_cast<std::uint32_t>(snapshot_.Id);
            facts.NetworkId = target.NetworkId();
            facts.Priority = menu_ ? menu_->Priority(facts.NetworkId) : 1;
            facts.Level = target.Level();
            facts.Position = target.Position();
            facts.ServerPosition = target.ServerPosition();
            facts.Direction = target.Direction();
            facts.Health = target.Health();
            facts.MaxHealth = target.MaxHealth();
            facts.AllShield = target.AllShield();
            facts.EffectiveHealth = facts.Health + facts.AllShield;
            facts.HealthRegen = target.HealthRegenRate();
            facts.DistanceToSource = source.IsValid() && !source.IsZero()
                ? source.Distance(target.Position())
                : (player.IsValid() ? player.Distance(target.Position()) : 0.0f);
            facts.Distance = facts.DistanceToSource;
            facts.MoveSpeed = target.MoveSpeed();
            facts.AttackDamage = target.AD();
            facts.AbilityPower = target.AP();
            facts.BoundingRadius = target.BoundingRadius();
            facts.AutoAttackDamage = target.AD();
            facts.Valid = target.IsValid();
            facts.Dead = target.IsDead();
            facts.Visible = target.IsVisible();
            facts.Targetable = target.IsTargetable();
            facts.Invulnerable = target.IsInvulnerable();
            facts.IsZombie = target.IsZombie();
            facts.IsClone = target.IsClone();
            facts.IsDashing = target.IsDashing();
            facts.IsMoving = target.IsMoving();
            facts.IsChanneling = target.Spellbook().IsChanneling();
            facts.IsCrowdControlled = target.HasBuff("stun") ||
                target.HasBuff("root") || target.HasBuff("snare") ||
                target.HasBuff("charm") || target.HasBuff("fear") ||
                target.HasBuff("taunt") || target.HasBuff("silence");
            facts.IsFacingSource = player.IsValid() &&
                ::SDK::Extensions::IsFacing(target, player);
            facts.IsStasis = target.HasBuff("bardrstasis") ||
                target.HasBuff("zhonyasringshield") ||
                target.HasBuff("lissandrarself") ||
                target.HasBuff("vladimirsanguinepool") ||
                target.HasBuff("fizztrickslippery");
        }
    }

    Menu* menu_ = nullptr;
    Drawing* drawing_ = nullptr;
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
