#pragma once

#include "KuroTargetSelectorContracts.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace SDK::KuroTargetSelector {

// Provider registration is deliberately separate from target evaluation.  A
// provider can be removed while a champion controller unloads without leaving
// a callback into that controller in the selector's hot path.
class ProviderRegistry final {
public:
    struct Entry {
        TargetRuleProvider Provider = {};
        ProviderToken Token = 0;
        ProviderDiagnostic Diagnostic = {};
    };

    ProviderToken Register(const TargetRuleProvider& provider) {
        if (!provider.Key || !provider.Key[0] ||
            provider.Band == ProviderPriorityBand::BaseSafety ||
            (!provider.BuildFacts && !provider.Validate && !provider.Score)) {
            return 0;
        }

        for (auto& entry : entries_) {
            if (entry.Provider.OwnerId == provider.OwnerId &&
                entry.Provider.Key &&
                _stricmp(entry.Provider.Key, provider.Key) == 0) {
                entry.Provider = provider;
                entry.Diagnostic.OwnerId = provider.OwnerId;
                entry.Diagnostic.Key = provider.Key;
                entry.Diagnostic.Band = provider.Band;
                entry.Diagnostic.Registered = true;
                ++revision_;
                return entry.Token;
            }
        }

        Entry entry{};
        entry.Provider = provider;
        entry.Token = nextToken_++;
        if (entry.Token == 0) entry.Token = nextToken_++;
        entry.Diagnostic.Token = entry.Token;
        entry.Diagnostic.OwnerId = provider.OwnerId;
        entry.Diagnostic.Key = provider.Key;
        entry.Diagnostic.Band = provider.Band;
        entry.Diagnostic.Registered = true;
        entries_.push_back(entry);
        ++revision_;
        return entry.Token;
    }

    bool Unregister(ProviderToken token) {
        if (!token) return false;
        const auto it = std::find_if(entries_.begin(), entries_.end(),
            [token](const Entry& entry) { return entry.Token == token; });
        if (it == entries_.end()) return false;
        it->Diagnostic.Registered = false;
        entries_.erase(it);
        ++revision_;
        return true;
    }

    std::size_t UnregisterOwner(std::uint32_t ownerId) {
        const auto before = entries_.size();
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
            [ownerId](const Entry& entry) {
                return entry.Provider.OwnerId == ownerId;
            }), entries_.end());
        if (entries_.size() != before) ++revision_;
        return before - entries_.size();
    }

    void Clear() {
        if (!entries_.empty()) ++revision_;
        entries_.clear();
    }

    std::uint64_t Revision() const { return revision_; }

    const std::vector<Entry>& Entries() const { return entries_; }
    std::vector<Entry>& MutableEntries() { return entries_; }

    std::vector<ProviderDiagnostic> Diagnostics() const {
        std::vector<ProviderDiagnostic> result;
        result.reserve(entries_.size());
        for (const auto& entry : entries_) {
            result.push_back(entry.Diagnostic);
        }
        return result;
    }

    bool BuildFacts(Entry& entry,
                    const TargetRequest& request,
                    const AIHeroClient& target,
                    TargetFacts& facts) {
        if (!entry.Provider.BuildFacts) return true;
        const auto start = std::chrono::steady_clock::now();
        ++entry.Diagnostic.Calls;
        try {
            const bool result = entry.Provider.BuildFacts(request, target, facts);
            RecordTime(entry, start);
            return result;
        } catch (...) {
            ++entry.Diagnostic.Failures;
            RecordTime(entry, start);
            return true; // neutral: keep core facts and continue evaluating
        }
    }

    RejectReason Validate(Entry& entry,
                          const TargetProviderContext& context) {
        if (!entry.Provider.Validate) return RejectReason::None;
        const auto start = std::chrono::steady_clock::now();
        ++entry.Diagnostic.Calls;
        try {
            const RejectReason result = entry.Provider.Validate(context);
            RecordTime(entry, start);
            return result;
        } catch (...) {
            ++entry.Diagnostic.Failures;
            RecordTime(entry, start);
            return RejectReason::None;
        }
    }

    ScoreContribution Score(Entry& entry,
                            const TargetProviderContext& context) {
        if (!entry.Provider.Score) return {};
        const auto start = std::chrono::steady_clock::now();
        ++entry.Diagnostic.Calls;
        try {
            const ScoreContribution result = entry.Provider.Score(context);
            RecordTime(entry, start);
            return result;
        } catch (...) {
            ++entry.Diagnostic.Failures;
            RecordTime(entry, start);
            return {};
        }
    }

private:
    static void RecordTime(Entry& entry,
                           const std::chrono::steady_clock::time_point& start) {
        const auto elapsed = std::chrono::steady_clock::now() - start;
        entry.Diagnostic.LastMilliseconds =
            std::chrono::duration<double, std::milli>(elapsed).count();
    }

    std::vector<Entry> entries_;
    ProviderToken nextToken_ = 1;
    std::uint64_t revision_ = 1;
};

} // namespace SDK::KuroTargetSelector
