#pragma once

#include "SelfSkillDebugPolicy.h"
#include "../Database/ThreatDatabase.h"
#include "../Visual/ThreatRenderer.h"
#include "../../../SDK/SDK.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ZDEvade {

class SelfSkillDebug {
public:
    static constexpr std::size_t kCapacity = 32;

    struct Diagnostics {
        char champion[64] = {};
        char lastMatchedProcess[96] = {};
        char lastUnmatchedProcess[96] = {};
        char lastMatchedMissile[96] = {};
        char lastUnmatchedMissile[96] = {};
        std::size_t databaseEntries = 0;
        std::size_t pending = 0;
        std::size_t live = 0;
        std::size_t terminal = 0;
        SelfSkillDebugCounters counters = {};
    };

    void Configure(std::uint32_t localPlayerNetworkId,
                   const char* championName) {
        AcquireSRWLockExclusive(&lock_);
        store_.Clear();
        bindings_ = {};
        localPlayerNetworkId_ = localPlayerNetworkId;
        CopyText(champion_, championName);
        databaseEntries_ = 0;
        for (const SpellData& spell : SpellDatabase::Spells) {
            if (EqualsNoCase(spell.charName.c_str(), champion_))
                ++databaseEntries_;
        }
        lastMatchedProcess_[0] = '\0';
        lastUnmatchedProcess_[0] = '\0';
        lastMatchedMissile_[0] = '\0';
        lastUnmatchedMissile_[0] = '\0';
        ReleaseSRWLockExclusive(&lock_);
    }

    void Clear() {
        AcquireSRWLockExclusive(&lock_);
        store_.Clear();
        bindings_ = {};
        lastMatchedProcess_[0] = '\0';
        lastUnmatchedProcess_[0] = '\0';
        lastMatchedMissile_[0] = '\0';
        lastUnmatchedMissile_[0] = '\0';
        ReleaseSRWLockExclusive(&lock_);
    }

    void OnProcessSpell(
            const SDK::Events::ProcessSpellEventArgs& args,
            int now) {
        const std::uint32_t sourceNetworkId =
            args.Sender.NetworkId;
        if (!IsExactLocalSelfSource(
                localPlayerNetworkId_,
                sourceNetworkId) ||
            (args.CasterNetworkId != 0 &&
             args.CasterNetworkId != localPlayerNetworkId_)) {
            return;
        }

        const SelfSkillProcessGeometry geometry =
            ResolveSelfSkillProcessGeometry(
                args.StartPosition.To2D(),
                args.Sender.Position.To2D(),
                args.EndPosition.To2D(),
                args.CastPosition.To2D());
        const Vec2 start = geometry.start;
        const Vec2 end = geometry.end;
        ProcessSpellMatchInput input;
        input.authoritativeNames = {{
            args.SpellName,
            args.PayloadSpellName,
            args.ScriptName,
            args.MissileName,
            args.PayloadMissileName,
        }};
        input.spellSlotName = args.SpellSlotName;
        input.spellNameFromSlotFallback =
            args.SpellNameFromSlotFallback;
        input.isAutoAttack = args.IsAutoAttack;
        input.hasRuntimeAttackSignature =
            args.IsAutoAttack &&
            HasExplicitProcessSpellAttackName(input);
        input.slot = args.Slot;
        input.targetNetworkId = args.TargetNetworkId;
        input.targetIndex = args.TargetIndex;
        input.hasUsableCastGeometry = geometry.valid;
        const ProcessSpellMatchResult match =
            MatchProcessSpellDatabaseFirst(
                input,
                [&](const char* name) {
                    return ThreatDatabase::FindAny(
                        name,
                        champion_);
                });

        SelfSkillProcessObservation observation;
        observation.localPlayerNetworkId =
            localPlayerNetworkId_;
        observation.sourceNetworkId = sourceNetworkId;
        observation.data = match.data;
        observation.matchDisposition =
            match.disposition;
        observation.slot = args.Slot;
        observation.tick = now;
        observation.start = start;
        observation.end = end;
        observation.castIdentity = args.CastInfo;
        observation.hasTarget =
            args.TargetNetworkId != 0 ||
            args.TargetIndex >= 0;
        observation.attackOrUtility =
            args.IsAutoAttack ||
            HasExplicitProcessSpellAttackName(input) ||
            HasUtilityProcessSpellName(input);
        const char* eventName = BestProcessName(args);

        AcquireSRWLockExclusive(&lock_);
        const SelfSkillDebugResult result =
            store_.ObserveProcess(observation);
        CopyText(
            result.accepted
                ? lastMatchedProcess_
                : lastUnmatchedProcess_,
            eventName);
        ReleaseSRWLockExclusive(&lock_);
    }

    void OnMissileCreate(
            const SDK::Events::ObjectEventArgs& args,
            int now) {
        // Capture the SDK wrapper before filtering so sparse decoded events can
        // safely recover their source, names, and geometry without raw reads.
        ::Core::Objects::ObjectHandle handle;
        handle.address = args.Sender.Ptr;
        handle.networkId = args.MissileNetworkId != 0
            ? args.MissileNetworkId
            : args.Sender.NetworkId;
        handle.index = args.Sender.Index;
        handle.type =
            ::Core::Objects::ObjectType::MissileClient;
        const SDK::MissileClient missile(handle);
        const bool runtimeValid = missile.IsValid();
        const std::uint32_t runtimeCasterNetworkId =
            runtimeValid
            ? static_cast<std::uint32_t>(
                missile.CasterNetworkId())
            : 0u;
        const std::string runtimeMissileName =
            runtimeValid ? missile.MissileName() : std::string();
        const std::string runtimeSpellName =
            runtimeValid ? missile.SpellName() : std::string();
        const Vec2 runtimeStart =
            runtimeValid
            ? missile.StartPosition().To2D()
            : Vec2();
        const Vec2 runtimeEnd =
            runtimeValid
            ? missile.EndPosition().To2D()
            : Vec2();
        const Vec2 runtimeHead =
            runtimeValid
            ? missile.Position().To2D()
            : Vec2();
        Vec2 eventEnd = args.EndPosition.To2D();
        if (!IsUsableSelfSkillPosition(eventEnd))
            eventEnd = args.CastEndPosition.To2D();
        const SelfSkillSparseMissileFields fields =
            ResolveSelfSkillSparseMissileFields({
                localPlayerNetworkId_,
                args.SourceNetworkId,
                args.Source.NetworkId,
                runtimeCasterNetworkId,
                args.StartPosition.To2D(),
                runtimeStart,
                eventEnd,
                runtimeEnd,
                args.Sender.Position.To2D(),
                runtimeHead,
            });
        if (!fields.exactLocalSource) return;

        const std::uint32_t runtimeTargetNetworkId =
            runtimeValid
            ? static_cast<std::uint32_t>(
                missile.TargetNetworkId())
            : 0u;
        MissileMatchInput input =
            BuildSelfSkillMissileMatchInput(
                args.MissileName,
                runtimeMissileName.c_str(),
                args.SpellName,
                runtimeSpellName.c_str(),
                args.TargetNetworkId,
                args.TargetIndex,
                runtimeTargetNetworkId);
        const MissileMatchResult match =
            MatchMissileDatabaseFirst(
                input,
                [&](const char* name) {
                    return ThreatDatabase::FindMissile(
                        name,
                        champion_);
                });

        SelfSkillMissileObservation observation;
        observation.localPlayerNetworkId =
            localPlayerNetworkId_;
        observation.sourceNetworkId =
            fields.sourceNetworkId;
        observation.data = match.data;
        observation.matchDisposition =
            match.disposition;
        observation.tick = now;
        observation.start = fields.start;
        observation.end = fields.end;
        observation.head = fields.head;
        observation.missileNetworkId =
            args.MissileNetworkId != 0
            ? args.MissileNetworkId
            : args.Sender.NetworkId != 0
                ? args.Sender.NetworkId
                : runtimeValid
                    ? static_cast<std::uint32_t>(
                        missile.NetworkId())
                    : 0u;
        observation.missileObjectIdentity =
            args.Sender.Ptr != 0
            ? args.Sender.Ptr
            : runtimeValid
                ? missile.Address()
                : 0;
        observation.castIdentity =
            args.CastIdentity;
        observation.hasTarget =
            IsTargetedMissileObservation(
                args.TargetNetworkId,
                args.TargetIndex,
                runtimeTargetNetworkId);
        observation.attackOrUtility =
            HasExplicitMissileAttackName(input) ||
            IsUtilityProcessSpellName(input.names[0]) ||
            IsUtilityProcessSpellName(input.names[1]) ||
            IsUtilityProcessSpellName(input.names[2]) ||
            IsUtilityProcessSpellName(input.names[3]);
        const char* eventName =
            args.MissileName[0]
            ? args.MissileName
            : runtimeMissileName.empty()
                ? args.SpellName[0]
                    ? args.SpellName
                    : runtimeSpellName.c_str()
                : runtimeMissileName.c_str();

        AcquireSRWLockExclusive(&lock_);
        const SelfSkillDebugResult result =
            store_.ObserveMissileCreate(observation);
        if (result.accepted)
            BindMissile(result.recordId, missile);
        if (IsMatchedSelfSkillMissileCreate(result)) {
            CopyText(lastMatchedMissile_, eventName);
        } else if (result.disposition !=
                SelfSkillDebugResultDisposition::Ignored) {
            CopyText(lastUnmatchedMissile_, eventName);
        }
        ReleaseSRWLockExclusive(&lock_);
    }

    void OnMissileDelete(
            const SDK::Events::ObjectEventArgs& args,
            int now,
            int terminalHoldMs) {
        const std::uint32_t missileNetworkId =
            args.MissileNetworkId != 0
            ? args.MissileNetworkId
            : args.Sender.NetworkId;
        const std::uint32_t eventSourceNetworkId =
            args.SourceNetworkId != 0
            ? args.SourceNetworkId
            : args.Source.NetworkId;
        AcquireSRWLockExclusive(&lock_);
        const bool ownsMissile = store_.OwnsMissile(
            missileNetworkId,
            args.Sender.Ptr);
        if (!ShouldProcessSelfMissileDelete(
                localPlayerNetworkId_,
                eventSourceNetworkId,
                ownsMissile)) {
            ReleaseSRWLockExclusive(&lock_);
            return;
        }
        const SelfSkillDebugResult result =
            store_.ObserveMissileDelete(
                missileNetworkId,
                args.Sender.Ptr,
                now,
                terminalHoldMs);
        if (result.accepted)
            UnbindMissile(result.recordId);
        ReleaseSRWLockExclusive(&lock_);
    }

    // This is the only method that dereferences captured MissileClient
    // wrappers. ZDEvade calls it exclusively from AddOnGameUpdate.
    void OnGameUpdate(int now) {
        AcquireSRWLockExclusive(&lock_);
        for (Binding& binding : bindings_) {
            if (binding.recordId == 0) continue;
            if (!binding.missile.IsValid()) {
                store_.MarkMissilePositionUnavailable(
                    binding.recordId,
                    now);
                continue;
            }
            const Vec2 position =
                binding.missile.Position().To2D();
            const Vec2 end =
                binding.missile.EndPosition().To2D();
            if (!store_.RefreshMissile(
                    binding.recordId,
                    position,
                    end,
                    now)) {
                store_.MarkMissilePositionUnavailable(
                    binding.recordId,
                    now);
            }
        }
        store_.Update(now);
        RemoveDeadBindings();
        ReleaseSRWLockExclusive(&lock_);
    }

    std::vector<SelfSkillDebugSnapshot> Snapshot() const {
        AcquireSRWLockShared(&lock_);
        std::vector<SelfSkillDebugSnapshot> result =
            store_.Snapshot();
        ReleaseSRWLockShared(&lock_);
        return result;
    }

    Diagnostics ReadDiagnostics() const {
        AcquireSRWLockShared(&lock_);
        Diagnostics diagnostics;
        CopyText(diagnostics.champion, champion_);
        CopyText(
            diagnostics.lastMatchedProcess,
            lastMatchedProcess_);
        CopyText(
            diagnostics.lastUnmatchedProcess,
            lastUnmatchedProcess_);
        CopyText(
            diagnostics.lastMatchedMissile,
            lastMatchedMissile_);
        CopyText(
            diagnostics.lastUnmatchedMissile,
            lastUnmatchedMissile_);
        diagnostics.databaseEntries =
            databaseEntries_;
        diagnostics.pending =
            store_.Count(SelfSkillDebugPhase::Pending);
        diagnostics.live =
            store_.Count(SelfSkillDebugPhase::Live);
        diagnostics.terminal =
            store_.Count(SelfSkillDebugPhase::Terminal);
        diagnostics.counters = store_.Counters();
        ReleaseSRWLockShared(&lock_);
        return diagnostics;
    }

    static void Draw(const SelfSkillDebugSnapshot& snapshot,
                     int now,
                     float planeY,
                     bool drawLabels,
                     bool drawEndpoints) {
        ThreatRenderer::Draw(snapshot.visual, now, planeY);
        const std::uint32_t color =
            PhaseColor(snapshot.phase);
        const Vec2 head =
            snapshot.phase == SelfSkillDebugPhase::Pending
            ? snapshot.visual.startPos
            : snapshot.visual.HeadAtTick(now);
        if (drawEndpoints) {
            SDK::Drawing::DrawCircle(
                Vec3::From2D(
                    snapshot.visual.startPos,
                    planeY),
                9.0f,
                color,
                2.0f,
                24);
            SDK::Drawing::DrawCircle(
                Vec3::From2D(
                    snapshot.visual.AuthoredEnd(),
                    planeY),
                11.0f,
                color,
                2.0f,
                24);
            SDK::Drawing::DrawCircle(
                Vec3::From2D(head, planeY),
                7.0f,
                color,
                2.5f,
                20);
        }
        if (drawLabels) {
            char label[192] = {};
            std::snprintf(
                label,
                sizeof(label),
                "SELF DRAW ONLY [%s] %s  missile=%u",
                PhaseName(snapshot.phase),
                snapshot.visual.SpellName().c_str(),
                snapshot.missileNetworkId);
            SDK::Drawing::DrawText(
                Vec3::From2D(
                    head + Vec2(18.0f, 18.0f),
                    planeY),
                label,
                color,
                true);
        }
    }

private:
    struct Binding {
        std::uint64_t recordId = 0;
        SDK::MissileClient missile = {};
    };

    static bool EqualsNoCase(const char* left,
                             const char* right) {
        return left && right &&
            left[0] && right[0] &&
            _stricmp(left, right) == 0;
    }

    template <std::size_t Size>
    static void CopyText(char (&destination)[Size],
                         const char* source) {
        strncpy_s(
            destination,
            Size,
            source ? source : "",
            _TRUNCATE);
    }

    static const char* BestProcessName(
            const SDK::Events::ProcessSpellEventArgs& args) {
        if (args.SpellName[0]) return args.SpellName;
        if (args.PayloadSpellName[0])
            return args.PayloadSpellName;
        if (args.ScriptName[0]) return args.ScriptName;
        if (args.MissileName[0]) return args.MissileName;
        if (args.PayloadMissileName[0])
            return args.PayloadMissileName;
        return args.SpellSlotName;
    }

    static const char* PhaseName(
            SelfSkillDebugPhase phase) {
        switch (phase) {
        case SelfSkillDebugPhase::Pending:
            return "PENDING CAST";
        case SelfSkillDebugPhase::Live:
            return "LIVE MISSILE";
        case SelfSkillDebugPhase::Terminal:
            return "TERMINAL";
        }
        return "?";
    }

    static std::uint32_t PhaseColor(
            SelfSkillDebugPhase phase) {
        switch (phase) {
        case SelfSkillDebugPhase::Pending:
            return 0xFFFFD23Fu;
        case SelfSkillDebugPhase::Live:
            return 0xFF00E5FFu;
        case SelfSkillDebugPhase::Terminal:
            return 0xFFFF4DFFu;
        }
        return 0xFFFFFFFFu;
    }

    void BindMissile(std::uint64_t recordId,
                     const SDK::MissileClient& missile) {
        for (Binding& binding : bindings_) {
            if (binding.recordId == recordId ||
                binding.recordId == 0) {
                binding.recordId = recordId;
                binding.missile = missile;
                return;
            }
        }
    }

    void UnbindMissile(std::uint64_t recordId) {
        for (Binding& binding : bindings_) {
            if (binding.recordId == recordId) {
                binding = {};
                return;
            }
        }
    }

    void RemoveDeadBindings() {
        const std::vector<SelfSkillDebugSnapshot> snapshots =
            store_.Snapshot();
        for (Binding& binding : bindings_) {
            if (binding.recordId == 0) continue;
            const bool live = std::any_of(
                snapshots.begin(),
                snapshots.end(),
                [&](const SelfSkillDebugSnapshot& snapshot) {
                    return snapshot.id == binding.recordId &&
                        snapshot.phase ==
                            SelfSkillDebugPhase::Live;
                });
            if (!live) binding = {};
        }
    }

    mutable SRWLOCK lock_ = SRWLOCK_INIT;
    SelfSkillDebugStore<kCapacity> store_;
    std::array<Binding, kCapacity> bindings_ = {};
    std::uint32_t localPlayerNetworkId_ = 0;
    char champion_[64] = {};
    char lastMatchedProcess_[96] = {};
    char lastUnmatchedProcess_[96] = {};
    char lastMatchedMissile_[96] = {};
    char lastUnmatchedMissile_[96] = {};
    std::size_t databaseEntries_ = 0;
};

} // namespace ZDEvade
