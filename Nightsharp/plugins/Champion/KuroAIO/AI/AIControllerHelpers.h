#pragma once

// Runtime/event helpers shared by full one-trick controllers.  This file is
// intentionally limited to champion-neutral plumbing.  Spell ordering,
// state transitions, hitbox semantics and matchup decisions remain owned by
// each AI<Champion>Controller.

#include "AIChampionEngine.h"
#include "AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace Plugins::KuroAIO::AI::ControllerHelpers {

inline bool IsLocalPlayer(const ::Core::Events::ObjectInfo& sender) {
    const auto player = ObjectManager::Player();
    return player.IsValid() && sender.IsValid() &&
           sender.NetworkId == static_cast<std::uint32_t>(player.NetworkId());
}

inline int Now() {
    return SDK::Variables::TickCount();
}

inline float CurrentResource(float maximum = FLT_MAX) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return 0.0f;
    return std::min(
        std::max(0.0f, player.Mana()),
        std::max(0.0f, maximum));
}

inline bool PlayerMobilityLocked() {
    const auto player = ObjectManager::Player();
    return player.IsValid() &&
        (SDK::HasBuffOfType(player, SDK::BuffType::Grounded) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Snare) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Stun) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Knockup) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Knockback) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Suppression));
}

inline float AutoAttackRange(const AIBaseClient& target,
                             float bonusRange = 0.0f) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return player.AttackRange() + player.BoundingRadius() +
           target.BoundingRadius() + std::max(0.0f, bonusRange);
}

inline bool InAutoAttackRange(const AIBaseClient& target,
                              float bonusRange = 0.0f) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) return false;
    const float range = AutoAttackRange(target, bonusRange);
    return player.Position().DistanceSqr2D(target.Position()) <=
           range * range;
}

inline bool CaptureAfterAttack(const SDK::OrbwalkingActionArgs& args,
                               int& targetNetworkId,
                               int& attackTick) {
    if (!args.Target.IsValid()) return false;
    targetNetworkId = static_cast<int>(args.Target.NetworkId());
    attackTick = Now();
    return true;
}

// Record only the neutral facts common to local basic-attack callbacks.
// What the attack means (passive consumption, a weave window, an empowered
// hit, and so on) remains the owning champion controller's responsibility.
inline bool CaptureLocalAutoAttack(
    const SDK::Events::ProcessSpellEventArgs& args,
    int& targetNetworkId,
    int& castTick) {
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return false;
    const std::uint32_t observedTarget = args.TargetNetworkId != 0
        ? args.TargetNetworkId
        : args.Target.NetworkId;
    targetNetworkId = static_cast<int>(observedTarget);
    castTick = Now();
    return true;
}

inline bool SpellEnabled(int index, Mode mode) {
    return Engine::MenuSpellEnabled(Engine::MenuForMode(mode), index, true);
}

inline bool RuntimeNameContains(int index, const char* token) {
    return index >= 0 && index < 4 && token && token[0] &&
           Engine::TextContains(Engine::RuntimeSpellNames[index].c_str(), token);
}

inline bool NameEquals(const char* left, const char* right) {
    return left && right && left[0] && right[0] &&
           _stricmp(left, right) == 0;
}

inline int SpellRank(int index) {
    if (index < 0 || index >= 4 || !Engine::ActiveProfile) return 0;
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return 0;
    const auto spell = player.Spellbook().GetSpell(
        Engine::ActiveProfile->Spells[index].Slot);
    return spell.IsValid() ? std::max(0, spell.Level()) : 0;
}

inline float SpellCost(int index) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || index < 0 || index >= 4 || !Engine::ActiveProfile) {
        return 0.0f;
    }
    const auto spell = player.Spellbook().GetSpell(
        Engine::ActiveProfile->Spells[index].Slot);
    const float cost = spell.IsValid() ? spell.ManaCost() : 0.0f;
    return std::isfinite(cost) && cost > 0.0f ? cost : 0.0f;
}

inline AIHeroClient HeroByNetworkId(int networkId) {
    return networkId != 0
        ? Engine::EnemyByNetworkId(networkId)
        : AIHeroClient{};
}

inline AIHeroClient NearestEnemyToPlayer(const AIHeroClient& fallback = {},
                                         float range = 1400.0f) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return {};
    AIHeroClient best = Engine::ValidEnemy(fallback, range)
        ? fallback
        : AIHeroClient{};
    float bestDistance = best.IsValid()
        ? player.Position().Distance2D(best.Position())
        : FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range)) continue;
        const float distance = player.Position().Distance2D(enemy.Position());
        if (distance < bestDistance) {
            best = enemy;
            bestDistance = distance;
        }
    }
    return best;
}

// Spell-shield state is champion-neutral. Controllers still decide whether
// consuming it is worthwhile and append their own parry/untargetable states.
inline bool HasSpellShieldOrImmunity(const AIBaseClient& target) {
    return target.IsValid() &&
        (SDK::HasBuffOfType(target, SDK::BuffType::SpellShield) ||
         SDK::HasBuffOfType(target, SDK::BuffType::SpellImmunity) ||
         target.HasBuff("SivirE") ||
         target.HasBuff("NocturneShroudofDarkness") ||
         target.HasBuff("MorganaE") ||
         target.HasBuff("BlackShield") ||
         target.HasBuff("BansheesVeil") ||
         target.HasBuff("EdgeOfNight"));
}

inline bool NearTerrain(const Vector3& position,
                        float radius = 175.0f,
                        int samples = 16) {
    if (!position.IsValid() || position.IsZero()) return false;
    if (SDK::NavMesh::IsWall(position)) return true;
    const int count = std::clamp(samples, 4, 64);
    for (int i = 0; i < count; ++i) {
        const float angle = 2.0f * SharedGeometry::kPi *
                            static_cast<float>(i) /
                            static_cast<float>(count);
        const Vector3 sample{
            position.x + std::cos(angle) * radius,
            position.y,
            position.z + std::sin(angle) * radius,
        };
        if (SDK::NavMesh::IsWall(sample)) return true;
    }
    return false;
}

inline AIBaseClient UnitByNetworkId(int networkId) {
    return networkId != 0
        ? ObjectManager::GetUnitByNetworkId<AIBaseClient>(networkId)
        : AIBaseClient{};
}

inline bool ValidHostileUnit(const AIBaseClient& unit,
                             float range = FLT_MAX) {
    const auto player = ObjectManager::Player();
    return unit.IsValid() && !unit.IsDead() && unit.IsEnemy() &&
           unit.IsTargetable() && player.IsValid() &&
           player.Position().Distance2D(unit.Position()) <= range;
}

inline bool IsEpicMonster(const AIBaseClient& unit) {
    if (!unit.IsValid()) return false;
    const AIMinionClient monster(unit.Address());
    if (!monster.IsValid()) return false;
    const SDK::JungleType type = monster.GetJungleType();
    return type == SDK::JungleType::Legendary ||
           type == SDK::JungleType::Epic;
}

inline bool CastThrottleReady(int index,
                              int defaultHumanizerMs,
                              int fastFollowupMs = -1) {
    if (index < 0 || index >= 4 || !Engine::RuntimeSpells[index] ||
        !Engine::RuntimeSpells[index]->IsReady()) {
        return false;
    }
    const int minimum = fastFollowupMs >= 0
        ? std::max(0, fastFollowupMs)
        : std::max(18, Slider(
              Engine::HumanMenu, "Humanizer", std::max(18, defaultHumanizerMs)));
    const int now = SDK::Variables::TickCount();
    return Engine::LastActionTick <= 0 ||
           now - Engine::LastActionTick >= minimum;
}

inline Vector3 PredictPosition(const AIBaseClient& target, float delaySeconds) {
    if (!target.IsValid()) return {};
    if (target.IsDashing() && target.PathEnd().IsValid() &&
        !target.PathEnd().IsZero()) {
        return target.PathEnd();
    }
    const auto prediction = SDK::Prediction::GetPrediction(
        target, std::max(0.0f, delaySeconds),
        std::max(25.0f, target.BoundingRadius()));
    const Vector3 predicted = prediction.GetUnitPosition();
    return predicted.IsValid() && !predicted.IsZero()
        ? predicted
        : target.Position();
}

inline bool ChampionIs(const AIHeroClient& target, const char* name) {
    return target.IsValid() && name && name[0] &&
           _stricmp(target.CharacterName().c_str(), name) == 0;
}

inline bool EnemySpellReady(const AIHeroClient& target, SDK::SpellSlot slot) {
    if (!target.IsValid()) return false;
    const auto spell = target.Spellbook().GetSpell(slot);
    return spell.IsValid() && spell.Level() > 0 &&
           spell.RemainingCooldown(Game::Time()) <= 0.08f;
}

// Common anti-dash zones.  This is a danger query, not a claim that every
// listed effect cancels every dash already in flight.
inline bool HasReadyDashHazardAt(const Vector3& position,
                                 float searchRange = 650.0f) {
    if (!position.IsValid() || position.IsZero()) return false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            enemy.Position().Distance2D(position) > searchRange) {
            continue;
        }
        if ((ChampionIs(enemy, "Poppy") &&
             (enemy.HasBuff("PoppyWZone") ||
              EnemySpellReady(enemy, SDK::SpellSlot::W))) ||
            (ChampionIs(enemy, "Taliyah") &&
             EnemySpellReady(enemy, SDK::SpellSlot::E)) ||
            (ChampionIs(enemy, "Cassiopeia") &&
             EnemySpellReady(enemy, SDK::SpellSlot::W))) {
            return true;
        }
    }
    return false;
}

inline bool MissileEventIsLocal(const SDK::Events::ObjectEventArgs& args) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return false;
    const std::uint32_t playerId = static_cast<std::uint32_t>(player.NetworkId());
    if (args.SourceNetworkId != 0) return args.SourceNetworkId == playerId;
    if (args.Source.NetworkId != 0) return args.Source.NetworkId == playerId;
    if (args.Sender.Ptr != 0) {
        const MissileClient missile(args.Sender.Ptr);
        return missile.IsValid() && missile.CasterNetworkId() == player.NetworkId();
    }
    return false;
}

inline int NormalizedCastDelayMs(float castDelay,
                                 int fallbackMs = 250) {
    if (!std::isfinite(castDelay) || castDelay <= 0.0f) {
        return std::max(0, fallbackMs);
    }
    // The event bridge historically exposed seconds, while a few spell
    // payloads arrive already expressed in milliseconds.  Normalize once so
    // champion controllers do not each grow a subtly different conversion.
    return static_cast<int>(castDelay * (castDelay > 10.0f ? 1.0f : 1000.0f));
}

inline bool LikelyHardCrowdControlSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    static constexpr std::array<const char*, 18> tokens = {
        "hook", "grab", "knock", "pulverize", "charm", "stun",
        "snare", "root", "bind", "bandagetoss", "rocketgrab",
        "deathsentence", "dredgeline", "darkbinding", "lightbinding",
        "zenithblade", "enchantedcrystalarrow", "curseofthesadmummy",
    };
    for (const char* token : tokens) {
        if (Engine::TextContains(args.SpellName, token) ||
            Engine::TextContains(args.ScriptName, token) ||
            Engine::TextContains(args.PayloadSpellName, token)) {
            return true;
        }
    }
    return false;
}

struct EnemyCastAnalysis {
    AIHeroClient Enemy = {};
    bool Valid = false;
    bool TargetsPlayer = false;
    bool Committed = false;
    bool CrossesPlayer = false;
    bool LikelyHardCrowdControl = false;
    float LineDistance = FLT_MAX;
    int CommitmentUntilTick = 0;
    int LineThreatUntilTick = 0;
};

inline EnemyCastAnalysis AnalyzeEnemyCast(
    const SDK::Events::ProcessSpellEventArgs& args,
    float minimumLineLength = 240.0f,
    float linePadding = 105.0f,
    int commitmentExtraMs = 250,
    int commitmentFallbackMs = 250,
    int commitmentMinimumMs = 240,
    int commitmentMaximumMs = 1500,
    int lineThreatWindowMs = 420) {
    EnemyCastAnalysis result{};
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !args.Sender.IsValid() ||
        IsLocalPlayer(args.Sender)) {
        return result;
    }

    result.Enemy = Engine::EnemyByNetworkId(
        static_cast<int>(args.Sender.NetworkId));
    if (!Engine::ValidEnemy(result.Enemy)) return result;

    result.Valid = true;
    const std::uint32_t playerId =
        static_cast<std::uint32_t>(player.NetworkId());
    result.TargetsPlayer = args.TargetNetworkId == playerId ||
                           args.Target.NetworkId == playerId;
    result.Committed = result.TargetsPlayer || args.IsAutoAttack ||
                       args.CastDelay >= 0.20f;
    const int now = SDK::Variables::TickCount();
    if (result.Committed) {
        const int castMs = NormalizedCastDelayMs(
            args.CastDelay, commitmentFallbackMs);
        result.CommitmentUntilTick = now + std::clamp(
            castMs + commitmentExtraMs,
            commitmentMinimumMs,
            commitmentMaximumMs);
    }

    if (!args.IsAutoAttack && args.StartPosition.IsValid() &&
        args.EndPosition.IsValid() && !args.StartPosition.IsZero() &&
        !args.EndPosition.IsZero() &&
        args.StartPosition.Distance2D(args.EndPosition) > minimumLineLength) {
        const auto projection = SharedGeometry::ProjectPointToSegment2D(
            player.Position(), args.StartPosition, args.EndPosition);
        result.LineDistance = projection.Distance;
        result.CrossesPlayer = projection.Distance <=
            player.BoundingRadius() + std::max(0.0f, linePadding);
        if (result.CrossesPlayer) {
            result.LikelyHardCrowdControl =
                LikelyHardCrowdControlSpell(args);
            result.LineThreatUntilTick = now +
                std::max(0, lineThreatWindowMs);
        }
    }
    return result;
}

enum class ThreatCondition : std::uint8_t {
    None,
    AnnieStunReady,
    TwistedFateGoldCard,
};

struct PointClickThreatRule {
    const char* Champion = "";
    SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
    float Range = 0.0f;
    ThreatCondition Condition = ThreatCondition::None;
    bool RequireSpellReady = true;
};

inline bool ThreatConditionMet(const AIHeroClient& enemy,
                               ThreatCondition condition) {
    switch (condition) {
    case ThreatCondition::AnnieStunReady:
        return enemy.HasBuff("anniepassiveprimed") ||
               enemy.HasBuff("pyromania_particle");
    case ThreatCondition::TwistedFateGoldCard:
        return enemy.HasBuff("GoldCardPreAttack") ||
               enemy.HasBuff("goldcardpreattack");
    default:
        return true;
    }
}

// Shared, auditable point-click lockdown registry.  Skillshots such as
// Warwick R and Skarner R deliberately do not belong here; their live paths
// are handled by each controller's incoming-line logic.
inline constexpr std::array<PointClickThreatRule, 18> PointClickThreats = {
    PointClickThreatRule{ "Malzahar", SDK::SpellSlot::R, 700.0f },
    PointClickThreatRule{ "Lissandra", SDK::SpellSlot::R, 550.0f },
    PointClickThreatRule{ "Vi", SDK::SpellSlot::R, 800.0f },
    PointClickThreatRule{ "Nautilus", SDK::SpellSlot::R, 825.0f },
    PointClickThreatRule{ "Pantheon", SDK::SpellSlot::W, 600.0f },
    PointClickThreatRule{ "Maokai", SDK::SpellSlot::W, 525.0f },
    PointClickThreatRule{ "FiddleSticks", SDK::SpellSlot::Q, 575.0f },
    PointClickThreatRule{ "Renekton", SDK::SpellSlot::W, 275.0f },
    PointClickThreatRule{ "Lulu", SDK::SpellSlot::W, 650.0f },
    PointClickThreatRule{ "Rammus", SDK::SpellSlot::E, 325.0f },
    PointClickThreatRule{ "Alistar", SDK::SpellSlot::W, 650.0f },
    PointClickThreatRule{ "Poppy", SDK::SpellSlot::E, 475.0f },
    PointClickThreatRule{ "Sett", SDK::SpellSlot::R, 400.0f },
    PointClickThreatRule{ "Camille", SDK::SpellSlot::R, 475.0f },
    PointClickThreatRule{ "Mordekaiser", SDK::SpellSlot::R, 650.0f },
    PointClickThreatRule{ "Annie", SDK::SpellSlot::Q, 625.0f,
                          ThreatCondition::AnnieStunReady, true },
    PointClickThreatRule{ "Annie", SDK::SpellSlot::R, 600.0f,
                          ThreatCondition::AnnieStunReady, true },
    PointClickThreatRule{ "TwistedFate", SDK::SpellSlot::W, 575.0f,
                          ThreatCondition::TwistedFateGoldCard, false },
};

inline bool HasReadyPointClickThreatAt(const Vector3& position) {
    if (!position.IsValid() || position.IsZero()) return false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        for (const auto& rule : PointClickThreats) {
            if (!ChampionIs(enemy, rule.Champion) ||
                position.Distance2D(enemy.Position()) > rule.Range ||
                !ThreatConditionMet(enemy, rule.Condition)) {
                continue;
            }
            if (!rule.RequireSpellReady || EnemySpellReady(enemy, rule.Slot)) {
                return true;
            }
        }
    }
    return false;
}

// Three-stage mobility champions commonly need the same gapcloser capture
// rule but retain champion-specific reactions.  This helper records only the
// neutral event facts; it never decides whether to cast, peel, or disengage.
inline bool CaptureGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args,
    int& targetNetworkId,
    Vector3& endpoint,
    int& expireTick,
    float nearbyEndpointRange,
    int lifetimeMs) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid()) return false;
    if (!args.IsDirectedToPlayer &&
        (!args.End.IsValid() || args.End.IsZero() ||
         args.End.Distance2D(player.Position()) > nearbyEndpointRange)) {
        return false;
    }
    targetNetworkId = static_cast<int>(args.NetworkId);
    endpoint = args.End;
    expireTick = SDK::Variables::TickCount() + std::max(0, lifetimeMs);
    return true;
}

inline int RemainingMilliseconds(float endTime,
                                 int fallbackMs,
                                 int minimumMs,
                                 int maximumMs) {
    const int raw = endTime > Game::Time()
        ? static_cast<int>((endTime - Game::Time()) * 1000.0f)
        : fallbackMs;
    return std::clamp(raw, minimumMs, maximumMs);
}

// Interruptable events expose the same target/lifetime facts to every
// champion. Controllers still decide whether they can interrupt at all and
// which spell/sequence is safe to commit.
inline void CaptureInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args,
    int& targetNetworkId,
    int& expireTick,
    int fallbackMs = 900,
    int minimumMs = 250,
    int maximumMs = 5000) {
    targetNetworkId = static_cast<int>(args.NetworkId);
    expireTick = Now() + RemainingMilliseconds(
        args.EndTime, fallbackMs, minimumMs, maximumMs);
}

} // namespace Plugins::KuroAIO::AI::ControllerHelpers
