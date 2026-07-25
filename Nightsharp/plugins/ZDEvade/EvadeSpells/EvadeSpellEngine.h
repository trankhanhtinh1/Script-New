#pragma once

#include "EvadeSpellData.h"
#include "EvadeSpellDatabase.h"
#include "../Evade/EvadePlanner.h"
#include "../../../Core/CoreControl.h"
#include "../../../Core/CoreEvadeState.h"
#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace ZDEvade {

struct EvadeSpellCastResult {
    bool casted = false;
    CandidateEvaluation destination;
    int holdUntilTick = 0;
    int protectionActivationTick = 0;
    HoldProtectionKind protection = HoldProtectionKind::None;
    SDK::SpellSlot slot = SDK::SpellSlot::Unknown;
};

class EvadeSpellEngine {
public:
    void Reset() {
        loadedChampion.clear();
        loaded = false;
        spells.clear();
        lastCastTick = 0;
        lastCastSlot = SDK::SpellSlot::Unknown;
    }

    static std::string RuleKey(const EvadeSpellData& data) {
        return data.charName + ":" + data.spellName + ":" +
            std::to_string(static_cast<int>(data.spellKey)) + ":" +
            std::to_string(data.itemID);
    }

    EvadeSpellCastResult TryUse(const SDK::AIHeroClient& player,
                                const std::vector<Threat>& threats,
                                const EvadeSettings& settings,
                                int minimumDanger,
                                const PlannerResult* existingWalkingPlan = nullptr,
                                const EvadeSpellRuleMap* rules = nullptr) {
        EvadeSpellCastResult result;
        if (!player.IsValid() || threats.empty()) return result;
        EnsureLoaded(player);
        const int now = SDK::Variables::TickCount();
        if (lastCastTick > 0 &&
            TickDifference(now, lastCastTick) < 275) return result;
        const std::vector<const Threat*> incoming = IncomingThreats(
            player,
            threats,
            settings,
            now);
        if (incoming.empty()) return result;
        const int threatDanger = HighestThreatDanger(incoming);
        if (threatDanger < std::max(1, minimumDanger)) return result;

        PlannerResult generatedWalkingPlan;
        if (!existingWalkingPlan) {
            generatedWalkingPlan = EvadePlanner::FindBest(
                player,
                threats,
                settings,
                SDK::Game::CursorPos().To2D());
        }
        const PlannerResult& walkingPlan = existingWalkingPlan
            ? *existingWalkingPlan
            : generatedWalkingPlan;
        const Vec2 preferredPosition = walkingPlan.found
            ? walkingPlan.selected.position
            : SDK::Game::CursorPos().To2D();
        std::vector<RankedCandidate> candidates;
        candidates.reserve(spells.size());
        for (const auto& entry : spells) {
            EvadeSpellRule rule;
            rule.danger = entry.data.dangerlevel;
            if (rules) {
                const auto found = rules->find(RuleKey(entry.data));
                if (found != rules->end()) rule = found->second;
            }
            if (!rule.enabled || rule.danger > threatDanger ||
                !IsReady(player, entry) ||
                !MatchesSpellName(player, entry)) continue;
            RankedCandidate candidate;
            candidate.spell = &entry;
            candidate.remainingThreats = static_cast<int>(incoming.size());
            candidate.requiredDanger = std::clamp(rule.danger, 1, 4);
            candidate.allowWardJump = rule.wardJump;
            if (!EstimateCandidate(
                    player,
                    threats,
                    incoming,
                    settings,
                    walkingPlan,
                    preferredPosition,
                    now,
                    candidate) ||
                candidate.remainingThreats >= static_cast<int>(incoming.size())) continue;
            candidates.push_back(candidate);
        }

        std::stable_sort(candidates.begin(), candidates.end(),
            [](const RankedCandidate& left, const RankedCandidate& right) {
                if (left.remainingThreats != right.remainingThreats)
                    return left.remainingThreats < right.remainingThreats;
                if (left.requiredDanger != right.requiredDanger)
                    return left.requiredDanger < right.requiredDanger;
                if (left.spell->data.isSummonerSpell != right.spell->data.isSummonerSpell)
                    return !left.spell->data.isSummonerSpell;
                if (left.spell->data.isItem != right.spell->data.isItem)
                    return !left.spell->data.isItem;
                if (left.spell->data.spellDelay != right.spell->data.spellDelay)
                    return left.spell->data.spellDelay < right.spell->data.spellDelay;
                return left.spell->data.name < right.spell->data.name;
            });

        for (const auto& candidate : candidates) {
            if (!CastCandidate(player, threats, settings, candidate)) continue;
            lastCastTick = now;
            lastCastSlot = candidate.spell->slot;
            result.casted = true;
            result.destination = candidate.destination;
            result.slot = candidate.spell->slot;
            result.holdUntilTick = HoldUntilTick(player, candidate, now);
            result.protectionActivationTick =
                ProtectionActivationTick(candidate.spell->data, now);
            result.protection = HoldProtectionFor(candidate.spell->data);
            return result;
        }
        return result;
    }

private:
    struct RuntimeSpell {
        EvadeSpellData data;
        SDK::SpellSlot slot = SDK::SpellSlot::Unknown;
    };

    struct RankedCandidate {
        const RuntimeSpell* spell = nullptr;
        CandidateEvaluation destination;
        SDK::AIBaseClient target;
        Vec2 castPosition = {};
        int remainingThreats = INT_MAX;
        int requiredDanger = 1;
        float effectiveSpeed = 0.0f;
        bool hasTarget = false;
        bool allowWardJump = true;
    };

    std::string loadedChampion;
    bool loaded = false;
    std::vector<RuntimeSpell> spells;
    int lastCastTick = 0;
    SDK::SpellSlot lastCastSlot = SDK::SpellSlot::Unknown;

    static bool EqualsNoCase(const std::string& left, const std::string& right) {
        if (left.empty() || right.empty() || left.size() != right.size()) return false;
        for (std::size_t index = 0; index < left.size(); ++index) {
            if (std::tolower(static_cast<unsigned char>(left[index])) !=
                std::tolower(static_cast<unsigned char>(right[index]))) return false;
        }
        return true;
    }

    static bool ContainsNoCase(const std::string& text, const std::string& value) {
        if (text.empty() || value.empty()) return false;
        std::string left = text;
        std::string right = value;
        std::transform(left.begin(), left.end(), left.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        std::transform(right.begin(), right.end(), right.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return left.find(right) != std::string::npos;
    }

    static bool IsNamed(const EvadeSpellData& data,
                        const char* name,
                        const char* spellName) {
        return EqualsNoCase(data.name, name) ||
               EqualsNoCase(data.name, spellName) ||
               EqualsNoCase(data.spellName, name) ||
               EqualsNoCase(data.spellName, spellName);
    }

    static bool SpellNameMatches(const SDK::SpellDataInstClient& spell,
                                 const std::string& name) {
        return spell.IsValid() &&
            (EqualsNoCase(spell.Name(), name) ||
             EqualsNoCase(spell.ScriptName(), name) ||
             EqualsNoCase(spell.IconName(), name));
    }

    static SDK::SpellSlot BaseSlot(EvadeSpellSlot slot) {
        switch (slot) {
        case EvadeSpellSlot::Q: return SDK::SpellSlot::Q;
        case EvadeSpellSlot::W: return SDK::SpellSlot::W;
        case EvadeSpellSlot::E: return SDK::SpellSlot::E;
        case EvadeSpellSlot::R: return SDK::SpellSlot::R;
        default: return SDK::SpellSlot::Unknown;
        }
    }

    static SDK::SpellSlot FindNamedSlot(const SDK::AIHeroClient& player,
                                        const EvadeSpellData& data) {
        for (SDK::SpellSlot slot : {SDK::SpellSlot::Q, SDK::SpellSlot::W,
                                    SDK::SpellSlot::E, SDK::SpellSlot::R}) {
            const auto spell = player.Spellbook().GetSpell(slot);
            if (SpellNameMatches(spell, data.spellName) ||
                SpellNameMatches(spell, data.name)) return slot;
        }
        return SDK::SpellSlot::Unknown;
    }

    static SDK::SpellSlot ResolveSlot(const SDK::AIHeroClient& player,
                                      const EvadeSpellData& data) {
        if (data.isItem) return SDK::SpellSlot::Unknown;
        if (data.isSummonerSpell) {
            for (SDK::SpellSlot slot : {SDK::SpellSlot::Summoner1, SDK::SpellSlot::Summoner2}) {
                const auto spell = player.Spellbook().GetSpell(slot);
                if (SpellNameMatches(spell, data.spellName) ||
                    SpellNameMatches(spell, data.name)) return slot;
            }
            return SDK::SpellSlot::Unknown;
        }
        SDK::SpellSlot slot = BaseSlot(data.spellKey);
        if (slot == SDK::SpellSlot::Unknown || data.checkSpellName) {
            const SDK::SpellSlot named = FindNamedSlot(player, data);
            if (named != SDK::SpellSlot::Unknown) return named;
        }
        if (slot == SDK::SpellSlot::Unknown &&
            IsNamed(data, "PhaseDive2", "EkkoEAttack")) return SDK::SpellSlot::E;
        return slot;
    }

    static bool IsReady(const SDK::AIHeroClient& player,
                        const RuntimeSpell& spell) {
        if (spell.data.isItem)
            return spell.data.itemID > 0 && SDK::CanUseItem(player, spell.data.itemID);
        if (spell.slot == SDK::SpellSlot::Unknown) return false;
        const auto instance = player.Spellbook().GetSpell(spell.slot);
        return instance.IsValid() && instance.Learned() &&
            player.Spellbook().CanUseSpell(spell.slot) == SDK::CoreSpellBook::State_Ready;
    }

    static bool MatchesSpellName(const SDK::AIHeroClient& player,
                                 const RuntimeSpell& spell) {
        if (spell.data.isItem) return true;
        const auto instance = player.Spellbook().GetSpell(spell.slot);
        return instance.IsValid() &&
            StrictEvadeSpellNameMatches(
                spell.data,
                instance.Name(),
                instance.ScriptName(),
                instance.IconName());
    }

    void EnsureLoaded(const SDK::AIHeroClient& player) {
        const std::string champion = player.CharacterName();
        if (!champion.empty() && EqualsNoCase(champion, loadedChampion) && loaded) return;
        EvadeSpellDatabase::Initialize();
        spells.clear();
        loadedChampion = champion;
        loaded = true;
        for (const auto& data : EvadeSpellDatabase::Spells) {
            const bool playerSpell = !champion.empty() && EqualsNoCase(data.charName, champion);
            const bool globalSpell = EqualsNoCase(data.charName, "AllChampions") ||
                data.isSummonerSpell || data.isItem;
            if (!playerSpell && !globalSpell) continue;
            const SDK::SpellSlot slot = ResolveSlot(player, data);
            if (!data.isItem && slot == SDK::SpellSlot::Unknown) continue;
            spells.push_back({data, slot});
        }
    }

    static std::vector<const Threat*> IncomingThreats(
            const SDK::AIHeroClient& player,
            const std::vector<Threat>& threats,
            const EvadeSettings& settings,
            int now) {
        std::vector<const Threat*> result;
        const Vec2 heroPos = player.ServerPosition().To2D();
        const float radius = SanitizeHeroRadius(player.BoundingRadius());
        for (const auto& threat : threats) {
            if (threat.IsExpiredAt(now)) continue;
            if (EvadeGeometry::ThreatensPointNowOrAtFutureImpact(
                    threat,
                    heroPos,
                    radius,
                    settings.pathBuffer,
                    now,
                    settings.maxThreatHorizonMs)) {
                result.push_back(&threat);
            }
        }
        return result;
    }

    static int HighestThreatDanger(const std::vector<const Threat*>& threats) {
        int danger = 0;
        for (const Threat* threat : threats) {
            if (threat) danger = std::max(danger, threat->Danger());
        }
        return danger;
    }

    static float CastDelay(const EvadeSpellData& data) {
        return std::max(0.0f, data.spellDelay) +
            static_cast<float>(std::max(0, SDK::Game::Ping())) * 0.5f;
    }

    static CandidateEvaluation CurrentPosition(const SDK::AIHeroClient& player,
                                               const EvadeSettings& settings) {
        CandidateEvaluation result;
        result.position = player.ServerPosition().To2D();
        result.source = PlannerCandidateSource::EvadeSpell;
        result.valid = result.position.IsValid() && !result.position.IsZero();
        result.walkable = result.valid;
        result.endpointSafe = result.valid;
        result.pathSafe = result.valid;
        result.timingSafe = result.valid;
        result.strictSafe = result.valid;
        result.travelDistance = 0.0f;
        result.exitDistance = 0.0f;
        result.arrivalTimeMs = 0.0f;
        result.timeMarginMs = settings.maxThreatHorizonMs;
        result.minimumClearance = settings.maxSearchRadius;
        result.firstCollisionTimeMs = FLT_MAX;
        result.cursorDistance = result.position.Distance(SDK::Game::CursorPos().To2D());
        result.rejectReason = result.valid
            ? PlannerRejectReason::None
            : PlannerRejectReason::Invalid;
        return result;
    }

    static bool IsSelfProtectiveTarget(const EvadeSpellData& data) {
        if (data.castType != EvadeCastType::Target) return false;
        if (data.evadeType == EvadeType::MovementSpeedBuff ||
            data.evadeType == EvadeType::SpellShield ||
            data.evadeType == EvadeType::Invulnerability) return true;
        return data.evadeType == EvadeType::Shield && data.speed <= 0.0f;
    }

    static bool IsDisplacement(const EvadeSpellData& data) {
        return data.evadeType == EvadeType::Dash ||
               data.evadeType == EvadeType::Blink ||
               (data.castType == EvadeCastType::Target && data.speed > 0.0f &&
                !IsSelfProtectiveTarget(data));
    }

    static float BoostedMoveSpeed(const SDK::AIHeroClient& player,
                                  const RuntimeSpell& spell) {
        const float current = std::max(50.0f, player.MoveSpeed());
        float percent = spell.data.speed;
        if (!spell.data.speedArray.empty()) {
            int rank = 1;
            if (!spell.data.isItem && spell.slot != SDK::SpellSlot::Unknown)
                rank = std::max(1, player.Spellbook().GetSpell(spell.slot).Level());
            const std::size_t index = static_cast<std::size_t>(std::clamp(
                rank - 1,
                0,
                static_cast<int>(spell.data.speedArray.size()) - 1));
            percent = spell.data.speedArray[index];
        }
        if (percent <= 0.0f) return current;
        return std::clamp(
            current * (1.0f + percent / 100.0f),
            current + 15.0f,
            current * 2.25f);
    }

    static int EstimateProtectionRemaining(
            const EvadeSpellData& data,
            const SDK::AIHeroClient& player,
            const std::vector<const Threat*>& incoming,
            const EvadeSettings& settings,
            int now) {
        const int activation = SaturatingTickAdd(
            now, ClampTickOffset(std::ceil(CastDelay(data))));
        const bool protectsMultiple = data.untargetable ||
            data.evadeType == EvadeType::Invulnerability ||
            data.evadeType == EvadeType::Shield ||
            IsNamed(data, "Rappel", "EliseSpiderEInitial") ||
            IsNamed(data, "Hallowed Mist", "GwenW") ||
            IsNamed(data, "Apotheosis", "NilahR") ||
            IsNamed(data, "Intervention", "JudicatorIntervention") ||
            IsNamed(data, "Hourglass", "ZhonyasHourglass") ||
            IsNamed(data, "Witchcap", "Witchcap");
        int protectedThreats = 0;
        const Vec2 heroPos = player.ServerPosition().To2D();
        const float radius = SanitizeHeroRadius(player.BoundingRadius());
        for (const Threat* threat : incoming) {
            if (!threat) continue;
            const int impact = EvadeGeometry::ImpactTickAt(*threat, heroPos);
            const bool activeAtActivation = EvadeGeometry::ContainsAt(
                *threat,
                heroPos,
                radius,
                settings.pathBuffer,
                activation);
            if (!activeAtActivation &&
                (TickDifference(impact, activation) < -45 ||
                 TickDifference(impact, activation) > 1200)) continue;
            ++protectedThreats;
            if (data.evadeType == EvadeType::SpellShield && !protectsMultiple) break;
        }
        return std::max(0, static_cast<int>(incoming.size()) - protectedThreats);
    }

    static Vec2 ThreatDirection(const Threat& threat, const Vec2& heroPos, int now) {
        if (threat.Type() == ZDSpellType::Arc) return {};
        Vec2 source = threat.startPos;
        if (threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed())
            source = threat.HeadAtTick(now);
        Vec2 direction = (source - heroPos).Normalized();
        if (direction.IsZero()) direction = (threat.startPos - heroPos).Normalized();
        if (direction.IsZero()) direction = threat.direction * -1.0f;
        return direction;
    }

    static bool EstimateWindWall(
            const EvadeSpellData& data,
            const SDK::AIHeroClient& player,
            const std::vector<const Threat*>& incoming,
            int now,
            Vec2& castPosition,
            int& remainingThreats) {
        const Vec2 heroPos = player.ServerPosition().To2D();
        const int activation = SaturatingTickAdd(
            now, ClampTickOffset(std::ceil(CastDelay(data))));
        const Threat* first = nullptr;
        int firstImpact = INT_MAX;
        for (const Threat* threat : incoming) {
            if (!threat || !threat->HasTravelSpeed()) continue;
            const int impact = EvadeGeometry::ImpactTickAt(*threat, heroPos);
            if (TickDifference(impact, activation) < -45 ||
                TickDifference(impact, activation) > 1200) continue;
            if (impact < firstImpact) {
                firstImpact = impact;
                first = threat;
            }
        }
        if (!first) return false;
        const Vec2 wallDirection = ThreatDirection(*first, heroPos, now);
        if (wallDirection.IsZero()) return false;
        int blocked = 0;
        for (const Threat* threat : incoming) {
            if (!threat || !threat->HasTravelSpeed()) continue;
            const int impact = EvadeGeometry::ImpactTickAt(*threat, heroPos);
            if (TickDifference(impact, activation) < -45 ||
                TickDifference(impact, activation) > 1200) continue;
            const Vec2 direction = ThreatDirection(*threat, heroPos, now);
            if (!direction.IsZero() && direction.Dot(wallDirection) >= 0.45f) ++blocked;
        }
        castPosition = heroPos + wallDirection * 100.0f;
        remainingThreats = std::max(0, static_cast<int>(incoming.size()) - blocked);
        return blocked > 0;
    }

    static CandidateEvaluation EvaluateLanding(
            const EvadeSpellData& data,
            const SDK::AIHeroClient& player,
            const Vec2& landing,
            const std::vector<Threat>& threats,
            const EvadeSettings& settings) {
        const bool blink = data.evadeType == EvadeType::Blink || data.untargetable;
        if (blink) {
            return EvadeGeometry::EvaluateBlinkCandidate(
                landing,
                player.ServerPosition().To2D(),
                SDK::Game::CursorPos().To2D(),
                player.ServerPosition().y,
                SanitizeHeroRadius(player.BoundingRadius()),
                SDK::Variables::TickCount(),
                CastDelay(data),
                settings,
                threats);
        }
        return EvadePlanner::EvaluateDestination(
            player,
            landing,
            PlannerCandidateSource::EvadeSpell,
            threats,
            settings,
            std::max(50.0f, data.speed),
            CastDelay(data));
    }

    static Vec2 LandingPosition(const EvadeSpellData& data,
                                const SDK::AIHeroClient& player,
                                const SDK::AIBaseClient& target) {
        const Vec2 heroPos = player.ServerPosition().To2D();
        const Vec2 targetPos = target.ServerPosition().To2D();
        Vec2 direction = (targetPos - heroPos).Normalized();
        if (direction.IsZero()) direction = player.Direction().To2D().Normalized();
        const float offset = std::max(
            75.0f,
            target.BoundingRadius() + player.BoundingRadius() * 0.5f);
        if (data.behindTarget) return targetPos + direction * offset;
        if (data.infrontTarget) return targetPos - direction * offset;
        return targetPos;
    }

    static void AddTarget(std::vector<SDK::AIBaseClient>& targets,
                          const SDK::AIBaseClient& target,
                          const SDK::AIHeroClient& player,
                          float range) {
        if (!target.IsValid() || target.IsDead() || !target.IsVisible() ||
            !target.IsTargetable() || target.NetworkId() == player.NetworkId()) return;
        const Vec2 position = target.ServerPosition().To2D();
        if (!position.IsValid() || position.IsZero() ||
            player.ServerPosition().To2D().Distance(position) >
                range + target.BoundingRadius() + 50.0f) return;
        for (const auto& current : targets) {
            if (current.NetworkId() == target.NetworkId()) return;
        }
        targets.push_back(target);
    }

    static std::vector<SDK::AIBaseClient> TargetCandidates(
            const EvadeSpellData& data,
            const SDK::AIHeroClient& player,
            SDK::SpellSlot slot,
            bool allowWardJump) {
        std::vector<SDK::AIBaseClient> result;
        float range = data.range;
        if (range <= 0.0f && slot != SDK::SpellSlot::Unknown)
            range = player.Spellbook().GetSpell(slot).CastRange();
        range = range > 0.0f ? range : 700.0f;
        const auto addAllyWards = [&]() {
            for (const auto& unit : SDK::GameObjects::AllyWards())
                AddTarget(result, SDK::AIBaseClient(unit.Handle()), player, range);
        };
        const auto addEnemyWards = [&]() {
            for (const auto& unit : SDK::GameObjects::EnemyWards())
                AddTarget(result, SDK::AIBaseClient(unit.Handle()), player, range);
        };
        for (EvadeSpellTargets type : data.spellTargets) {
            switch (type) {
            case EvadeSpellTargets::AllyChampions:
                for (const auto& unit : SDK::GameObjects::AllyHeroes())
                    AddTarget(result, SDK::AIBaseClient(unit.Handle()), player, range);
                break;
            case EvadeSpellTargets::EnemyChampions:
                for (const auto& unit : SDK::GameObjects::EnemyHeroes())
                    AddTarget(result, SDK::AIBaseClient(unit.Handle()), player, range);
                break;
            case EvadeSpellTargets::AllyMinions:
                for (const auto& unit : SDK::GameObjects::AllyMinions())
                    AddTarget(result, SDK::AIBaseClient(unit.Handle()), player, range);
                if (allowWardJump) addAllyWards();
                break;
            case EvadeSpellTargets::EnemyMinions:
                for (const auto& unit : SDK::GameObjects::EnemyMinions())
                    AddTarget(result, SDK::AIBaseClient(unit.Handle()), player, range);
                if (allowWardJump) addEnemyWards();
                break;
            case EvadeSpellTargets::Targetables:
                for (const auto& unit : SDK::ObjectManager::Get<SDK::AIMinionClient>())
                    AddTarget(result, SDK::AIBaseClient(unit.Handle()), player, range);
                for (const auto& unit : SDK::ObjectManager::Get<SDK::AIHeroClient>())
                    AddTarget(result, SDK::AIBaseClient(unit.Handle()), player, range);
                if (allowWardJump) {
                    addAllyWards();
                    addEnemyWards();
                }
                break;
            }
        }
        return result;
    }

    static bool FindTarget(const RuntimeSpell& spell,
                           const SDK::AIHeroClient& player,
                           const Vec2& preferredPosition,
                           const std::vector<Threat>& threats,
                           const EvadeSettings& settings,
                           bool allowWardJump,
                           SDK::AIBaseClient& target,
                           CandidateEvaluation& destination,
                           int& remainingThreats) {
        if (IsSelfProtectiveTarget(spell.data)) {
            target = SDK::AIBaseClient(player.Handle());
            destination = CurrentPosition(player, settings);
            return target.IsValid();
        }
        float bestScore = FLT_MAX;
        int bestRemaining = INT_MAX;
        std::vector<SDK::AIBaseClient> targets = TargetCandidates(
            spell.data, player, spell.slot, allowWardJump);
        std::stable_sort(targets.begin(), targets.end(), [&](const auto& left, const auto& right) {
            return LandingPosition(spell.data, player, left).DistanceSqr(preferredPosition) <
                LandingPosition(spell.data, player, right).DistanceSqr(preferredPosition);
        });
        if (targets.size() > 24) targets.resize(24);
        for (const auto& unit : targets) {
            const Vec2 landing = LandingPosition(spell.data, player, unit);
            CandidateEvaluation evaluation = EvaluateLanding(
                spell.data,
                player,
                landing,
                threats,
                settings);
            if (!evaluation.valid || !evaluation.walkable || !evaluation.strictSafe) continue;
            const int remaining = std::max(0, evaluation.collisionCount);
            const float score = preferredPosition.IsValid() && !preferredPosition.IsZero()
                ? landing.Distance(preferredPosition)
                : evaluation.cursorDistance;
            if (remaining < bestRemaining ||
                (remaining == bestRemaining && score < bestScore)) {
                bestRemaining = remaining;
                bestScore = score;
                target = unit;
                destination = evaluation;
                remainingThreats = remaining;
            }
        }
        return target.IsValid();
    }

    static bool FindChronobreakLanding(const EvadeSpellData& data,
                                       const SDK::AIHeroClient& player,
                                       const std::vector<Threat>& threats,
                                       const EvadeSettings& settings,
                                       CandidateEvaluation& destination,
                                       int& remainingThreats) {
        CandidateEvaluation best;
        int bestRemaining = INT_MAX;
        for (const auto& unit : SDK::ObjectManager::Get<SDK::AIMinionClient>()) {
            if (!unit.IsValid() || unit.IsDead() ||
                (!ContainsNoCase(unit.Name(), "Ekko") &&
                 !ContainsNoCase(unit.CharacterName(), "Ekko"))) continue;
            CandidateEvaluation evaluation = EvaluateLanding(
                data,
                player,
                unit.ServerPosition().To2D(),
                threats,
                settings);
            if (!evaluation.valid || !evaluation.walkable || !evaluation.strictSafe) continue;
            const int remaining = std::max(0, evaluation.collisionCount);
            if (!best.valid || remaining < bestRemaining ||
                (remaining == bestRemaining && evaluation.cursorDistance < best.cursorDistance)) {
                best = evaluation;
                bestRemaining = remaining;
            }
        }
        if (!best.valid) return false;
        destination = best;
        remainingThreats = bestRemaining;
        return true;
    }

    static bool EstimateMovementSpeed(const RuntimeSpell& spell,
                                      const SDK::AIHeroClient& player,
                                      const std::vector<Threat>& threats,
                                      const EvadeSettings& settings,
                                      const PlannerResult& walkingPlan,
                                      RankedCandidate& candidate) {
        const float boostedSpeed = BoostedMoveSpeed(player, spell);
        CandidateEvaluation best;
        int bestRemaining = INT_MAX;
        const auto consider = [&](const Vec2& position) {
            if (!position.IsValid() || position.IsZero()) return;
            CandidateEvaluation evaluation = EvadePlanner::EvaluateDestination(
                player,
                position,
                PlannerCandidateSource::EvadeSpell,
                threats,
                settings,
                boostedSpeed,
                CastDelay(spell.data));
            if (!evaluation.valid || !evaluation.walkable || !evaluation.strictSafe) return;
            const int remaining = std::max(0, evaluation.collisionCount);
            if (!best.valid || remaining < bestRemaining ||
                (remaining == bestRemaining && evaluation.travelDistance < best.travelDistance)) {
                best = evaluation;
                bestRemaining = remaining;
            }
        };
        if (walkingPlan.found) consider(walkingPlan.selected.position);
        const std::size_t candidateBudget = 24;
        const std::size_t stride = std::max<std::size_t>(
            1,
            walkingPlan.candidates.size() / candidateBudget);
        for (std::size_t index = 0;
             index < walkingPlan.candidates.size();
             index += stride) {
            consider(walkingPlan.candidates[index].position);
        }
        if (!best.valid) {
            const Vec2 heroPos = player.ServerPosition().To2D();
            Vec2 direction = (SDK::Game::CursorPos().To2D() - heroPos).Normalized();
            if (!direction.IsZero()) consider(
                heroPos + direction * std::min(300.0f, settings.maxSearchRadius));
        }
        if (!best.valid) return false;
        candidate.destination = best;
        candidate.remainingThreats = bestRemaining;
        candidate.effectiveSpeed = boostedSpeed;
        if (spell.data.castType == EvadeCastType::Target) {
            candidate.target = SDK::AIBaseClient(player.Handle());
            candidate.hasTarget = candidate.target.IsValid();
        } else if (spell.data.castType == EvadeCastType::Position) {
            candidate.castPosition = best.position;
        }
        return true;
    }

    static bool EstimateCandidate(const SDK::AIHeroClient& player,
                                  const std::vector<Threat>& threats,
                                  const std::vector<const Threat*>& incoming,
                                  const EvadeSettings& settings,
                                  const PlannerResult& walkingPlan,
                                  const Vec2& preferredPosition,
                                  int now,
                                  RankedCandidate& candidate) {
        const RuntimeSpell& spell = *candidate.spell;
        const EvadeSpellData& data = spell.data;
        if (data.evadeType == EvadeType::MovementSpeedBuff) {
            return EstimateMovementSpeed(
                spell,
                player,
                threats,
                settings,
                walkingPlan,
                candidate);
        }
        if (data.evadeType == EvadeType::WindWall) {
            candidate.destination = CurrentPosition(player, settings);
            return EstimateWindWall(
                data,
                player,
                incoming,
                now,
                candidate.castPosition,
                candidate.remainingThreats);
        }
        if (IsNamed(data, "Chronobreak", "EkkoR")) {
            return FindChronobreakLanding(
                data,
                player,
                threats,
                settings,
                candidate.destination,
                candidate.remainingThreats);
        }
        if (data.castType == EvadeCastType::Target) {
            if (IsSelfProtectiveTarget(data)) {
                candidate.target = SDK::AIBaseClient(player.Handle());
                candidate.hasTarget = candidate.target.IsValid();
                candidate.destination = CurrentPosition(player, settings);
                candidate.remainingThreats = EstimateProtectionRemaining(
                    data,
                    player,
                    incoming,
                    settings,
                    now);
                return candidate.hasTarget;
            }
            candidate.hasTarget = FindTarget(
                spell,
                player,
                preferredPosition,
                threats,
                settings,
                candidate.allowWardJump,
                candidate.target,
                candidate.destination,
                candidate.remainingThreats);
            return candidate.hasTarget;
        }
        if (data.evadeType == EvadeType::Invulnerability ||
            data.evadeType == EvadeType::Shield ||
            data.evadeType == EvadeType::SpellShield) {
            candidate.destination = CurrentPosition(player, settings);
            candidate.remainingThreats = EstimateProtectionRemaining(
                data,
                player,
                incoming,
                settings,
                now);
            return true;
        }
        if (data.castType == EvadeCastType::Position && IsDisplacement(data)) {
            float range = data.range;
            if (range <= 0.0f && spell.slot != SDK::SpellSlot::Unknown)
                range = player.Spellbook().GetSpell(spell.slot).CastRange();
            candidate.destination = EvadePlanner::FindBestSpellPosition(
                player,
                threats,
                settings,
                range,
                data.speed,
                CastDelay(data),
                data.fixedRange && !data.isSummonerSpell,
                data.evadeType == EvadeType::Blink || data.untargetable);
            if (!candidate.destination.valid || !candidate.destination.strictSafe) return false;
            candidate.remainingThreats = std::max(0, candidate.destination.collisionCount);
            const Vec2 heroPos = player.ServerPosition().To2D();
            candidate.castPosition = data.isReversed
                ? heroPos - (candidate.destination.position - heroPos)
                : candidate.destination.position;
            return true;
        }
        return false;
    }

    static bool PrepareMovement(const SDK::AIHeroClient& player,
                                const Vec2& destination) {
        if (!destination.IsValid() || destination.IsZero()) return false;
        const Vec2 pathEnd = player.PathEnd().To2D();
        if ((player.IsMoving() || player.HasPath()) && !pathEnd.IsZero() &&
            pathEnd.DistanceSqr(destination) <= 45.0f * 45.0f) return true;
        return CoreControl::IssueMove(
            Vec3::From2D(destination, player.ServerPosition().y),
            true);
    }

    static bool CastCandidate(const SDK::AIHeroClient& player,
                              const std::vector<Threat>&,
                              const EvadeSettings&,
                              const RankedCandidate& candidate) {
        const RuntimeSpell& spell = *candidate.spell;
        const EvadeSpellData& data = spell.data;
        CoreEvadeState::SpellCastBypassScope bypass;
        if (data.evadeType == EvadeType::MovementSpeedBuff &&
            !PrepareMovement(player, candidate.destination.position)) return false;
        if (data.isItem) return SDK::UseItem(player, data.itemID);
        if (IsNamed(data, "PhaseDive2", "EkkoEAttack") &&
            !player.HasBuff("ekkoeattackbuff")) return false;
        if (IsNamed(data, "Rappel", "EliseSpiderEInitial"))
            return player.Spellbook().CastSpell(spell.slot, player.Address());
        if (IsNamed(data, "Pounce", "Pounce") &&
            !SpellNameMatches(player.Spellbook().GetSpell(spell.slot), "Pounce")) return false;
        if (IsNamed(data, "BrokenWings", "RivenTriCleave") &&
            !PrepareMovement(player, candidate.destination.position)) return false;
        if (data.evadeType == EvadeType::WindWall)
            return player.Spellbook().CastSpell(
                spell.slot,
                Vec3::From2D(candidate.castPosition, player.ServerPosition().y),
                false);
        if (IsNamed(data, "Featherstorm", "XayahR") ||
            IsNamed(data, "Chronobreak", "EkkoR"))
            return player.Spellbook().CastSpell(spell.slot);
        switch (data.castType) {
        case EvadeCastType::Self:
            return player.Spellbook().CastSpell(spell.slot);
        case EvadeCastType::Target:
            if (!candidate.hasTarget || !candidate.target.IsValid()) return false;
            return player.Spellbook().CastSpell(spell.slot, candidate.target.Address());
        case EvadeCastType::Position:
        default:
            if (!candidate.castPosition.IsValid() || candidate.castPosition.IsZero()) return false;
            return player.Spellbook().CastSpell(
                spell.slot,
                Vec3::From2D(candidate.castPosition, player.ServerPosition().y),
                false);
        }
    }

    static int HoldUntilTick(const SDK::AIHeroClient& player,
                             const RankedCandidate& candidate,
                             int now) {
        const EvadeSpellData& data = candidate.spell->data;
        const float delay = CastDelay(data);
        if (data.isItem && (data.itemID == 3157 || data.itemID == 3159))
            return SaturatingTickAdd(now, 2500);
        if (data.evadeType == EvadeType::MovementSpeedBuff) {
            const float speed = std::max(50.0f, candidate.effectiveSpeed);
            const float travel = 1000.0f * candidate.destination.travelDistance / speed;
            return SaturatingTickAdd(
                now, ClampTickOffset(std::ceil(delay + travel + 60.0f)));
        }
        if (IsDisplacement(data)) {
            const bool blink = data.evadeType == EvadeType::Blink || data.untargetable;
            const float travel = blink || data.speed <= 1.0f
                ? 0.0f
                : 1000.0f * player.ServerPosition().To2D().Distance(
                    candidate.destination.position) / std::max(50.0f, data.speed);
            return SaturatingTickAdd(
                now, ClampTickOffset(std::ceil(delay + travel + 60.0f)));
        }
        if (data.untargetable ||
            IsNamed(data, "Rappel", "EliseSpiderEInitial") ||
            IsNamed(data, "Intervention", "JudicatorIntervention"))
            return SaturatingTickAdd(
                now, ClampTickOffset(std::ceil(delay + 900.0f)));
        return SaturatingTickAdd(
            now, ClampTickOffset(std::ceil(delay + 120.0f)));
    }

    static int ProtectionActivationTick(const EvadeSpellData& data,
                                        int now) {
        return SaturatingTickAdd(
            now, ClampTickOffset(std::ceil(CastDelay(data))));
    }
};

}
