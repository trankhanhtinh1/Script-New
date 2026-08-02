#pragma once
#include <array>

#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/Enumerations/ChampionId.h"

using namespace ::SDK;

namespace OrbwalkerKuro::OrbwalkingDetail {

inline constexpr float kLaneClearWaitCycles = 2.0f;
inline constexpr int kLastHitWindowStepMs = 50;

inline bool IsValidAttackTarget(const AIHeroClient& player,
                                const AttackableUnit& target,
                                float range = FLT_MAX) {
    if (!player.IsValid() || !target.IsValid()) {
        return false;
    }
    if ((!target.IsEnemy() && target.Team() != GameObjectTeam::Neutral) || (!target.IsZombie() && target.IsDead())) {
        return false;
    }
    if (!target.IsVisible() || !target.IsTargetable() || target.IsInvulnerable()) {
        return false;
    }
    const Vector3 origin = player.Position();
    return range >= FLT_MAX * 0.5f ||
           origin.DistanceSqr2D(target.Position()) <= range * range;
}

inline bool IsValidAttackTarget(const AttackableUnit& target, float range = FLT_MAX) {
    return IsValidAttackTarget(GameObjects::Player(), target, range);
}

// ---------------------------------------------------------------------------
// Projectile wall collision (Yasuo W, Samira W, Mel W)
// ---------------------------------------------------------------------------
// A projectile basic attack that has to cross an enemy Wind Wall never lands,
// so a target behind one is not attackable even though every other validity
// check passes: the orbwalker has to skip it and pick the next candidate
// instead of feeding attacks to the wall. The champion lists mirror
// sdk/Wrappers/Orbwalking/OrbwalkerBase.h so both orbwalkers block the same
// champions.

// Mirrored from the menu on every GetTarget()/Attack() call, because the
// namespace-level helpers below have no access to the menu instance.
inline bool WallCheckEnabled = true;

inline constexpr std::array<SDK::ChampionId, 64> kWindWallBrokenChampions = {{
    SDK::ChampionId::Annie, SDK::ChampionId::TwistedFate,
    SDK::ChampionId::Leblanc, SDK::ChampionId::Urgot,
    SDK::ChampionId::Vladimir, SDK::ChampionId::Fiddlesticks,
    SDK::ChampionId::Ryze, SDK::ChampionId::Sivir,
    SDK::ChampionId::Soraka, SDK::ChampionId::Teemo,
    SDK::ChampionId::Tristana, SDK::ChampionId::MissFortune,
    SDK::ChampionId::Ashe, SDK::ChampionId::Morgana,
    SDK::ChampionId::Zilean, SDK::ChampionId::Twitch,
    SDK::ChampionId::Karthus, SDK::ChampionId::Anivia,
    SDK::ChampionId::Sona, SDK::ChampionId::Janna,
    SDK::ChampionId::Corki, SDK::ChampionId::Karma,
    SDK::ChampionId::Veigar, SDK::ChampionId::Swain,
    SDK::ChampionId::Caitlyn, SDK::ChampionId::Orianna,
    SDK::ChampionId::Brand, SDK::ChampionId::Vayne,
    SDK::ChampionId::Cassiopeia, SDK::ChampionId::Heimerdinger,
    SDK::ChampionId::Ezreal, SDK::ChampionId::Kennen,
    SDK::ChampionId::KogMaw, SDK::ChampionId::Lux,
    SDK::ChampionId::Xerath, SDK::ChampionId::Ahri,
    SDK::ChampionId::Graves, SDK::ChampionId::Varus,
    SDK::ChampionId::Viktor, SDK::ChampionId::Lulu,
    SDK::ChampionId::Ziggs, SDK::ChampionId::Draven,
    SDK::ChampionId::Quinn, SDK::ChampionId::Syndra,
    SDK::ChampionId::AurelionSol, SDK::ChampionId::Zoe,
    SDK::ChampionId::Zyra, SDK::ChampionId::Kaisa,
    SDK::ChampionId::Taliyah, SDK::ChampionId::Jhin,
    SDK::ChampionId::Kindred, SDK::ChampionId::Jinx,
    SDK::ChampionId::Lucian, SDK::ChampionId::Yuumi,
    SDK::ChampionId::Thresh, SDK::ChampionId::Kalista,
    SDK::ChampionId::Xayah, SDK::ChampionId::Aphelios,
    SDK::ChampionId::Bard, SDK::ChampionId::Ivern,
    SDK::ChampionId::Nami, SDK::ChampionId::Velkoz,
    SDK::ChampionId::Lissandra, SDK::ChampionId::Malzahar,
}};

inline bool IsWindWallBrokenChampion(SDK::ChampionId championId) {
    for (const SDK::ChampionId candidate : kWindWallBrokenChampions) {
        if (candidate == championId) return true;
    }
    return false;
}

// Champions whose attack only becomes a wall-stoppable projectile in one of
// their forms/stances.
inline constexpr std::array<SDK::ChampionId, 7> kSpecialWindWallChampions = {{
    SDK::ChampionId::Kayle, SDK::ChampionId::Elise,
    SDK::ChampionId::Nidalee, SDK::ChampionId::Jayce,
    SDK::ChampionId::Gnar, SDK::ChampionId::Azir,
    SDK::ChampionId::Neeko,
}};

inline bool IsSpecialWindWallChampion(SDK::ChampionId championId) {
    for (const SDK::ChampionId candidate : kSpecialWindWallChampions) {
        if (candidate == championId) return true;
    }
    return false;
}

// Whether the local champion currently attacks with a projectile the wall eats.
// Cached per tick rather than once per game: Kayle levels into range, and
// Gnar/Elise/Nidalee/Jayce/Neeko swap forms mid-fight.
inline bool PlayerAttackHitsWall(const AIHeroClient& player) {
    static int cachedTick = -1;
    static bool cached = false;
    const int now = Variables::TickCount();
    if (cachedTick == now) {
        return cached;
    }
    cachedTick = now;
    cached = false;
    if (!player.IsValid()) {
        return false;
    }

    const SDK::ChampionId championId =
        SDK::ChampionIdFromName(player.CharacterName().c_str());
    if (IsWindWallBrokenChampion(championId)) {
        cached = true;
    } else if (IsSpecialWindWallChampion(championId)) {
        cached =
            (championId == SDK::ChampionId::Kayle && player.AttackRange() >= 530.0f) ||
            (championId == SDK::ChampionId::Elise && !player.IsMelee()) ||
            (championId == SDK::ChampionId::Nidalee && !player.IsMelee()) ||
            (championId == SDK::ChampionId::Jayce && !player.IsMelee()) ||
            (championId == SDK::ChampionId::Gnar && !player.IsMelee()) ||
            (championId == SDK::ChampionId::Neeko && !player.HasBuff("neekowpassiveready"));
    }
    return cached;
}

// Samira W and Mel W only block while the buff is up, but the buff check lives
// inside the SDK collision call. Gate on their presence in the game instead —
// cached like HasGangplankInGame() — so the ordinary game pays nothing.
inline bool HasShieldWallChampionInGame() {
    static bool checked = false;
    static bool present = false;
    if (!checked) {
        for (const auto& hero : GameObjects::EnemyHeroes()) {
            if (!hero.IsValid()) {
                continue;
            }
            const SDK::ChampionId championId =
                SDK::ChampionIdFromName(hero.CharacterName().c_str());
            if (championId == SDK::ChampionId::Samira ||
                championId == SDK::ChampionId::Mel) {
                present = true;
                break;
            }
        }
        if (!GameObjects::EnemyHeroes().empty()) checked = true;
    }
    return present;
}

// Per-tick gate so the per-target segment test below only runs while something
// can actually block: no wall up and no Samira/Mel in game costs one empty()
// check per tick.
inline bool AnyProjectileWallActive() {
    static int cachedTick = -1;
    static bool cached = false;
    const int now = Variables::TickCount();
    if (cachedTick == now) {
        return cached;
    }
    cachedTick = now;
    cached = !YasuoWallTracker::ActiveWalls().empty() || HasShieldWallChampionInGame();
    return cached;
}

inline bool IsWallBlocked(const AIHeroClient& player, const AttackableUnit& target) {
    if (!WallCheckEnabled || !player.IsValid() || !target.IsValid()) {
        return false;
    }
    if (!PlayerAttackHitsWall(player) || !AnyProjectileWallActive()) {
        return false;
    }
    // Azir's Sand Soldier stab is not a projectile and ignores the wall; only
    // Azir's own bolt, cast when no soldier can reach the target, is stopped.
    if (WillUseAzirSoldierAttack(player, target)) {
        return false;
    }

    // GetTarget() re-tests the same units several times per tick (candidate
    // scan, cached-target revalidation, then Attack()), so memoise the segment
    // test per tick in a small direct-mapped table.
    struct WallBlockCacheEntry {
        int tick = -1;
        int networkId = 0;
        bool blocked = false;
    };
    static WallBlockCacheEntry cache[64];

    const int now = Variables::TickCount();
    const int networkId = target.NetworkId();
    WallBlockCacheEntry* entry = networkId != 0
        ? &cache[static_cast<unsigned>(networkId) & 63u]
        : nullptr;
    if (entry && entry->tick == now && entry->networkId == networkId) {
        return entry->blocked;
    }

    Vector3 from = player.ServerPosition();
    if (!from.IsValid() || from.IsZero()) {
        from = player.Position();
    }

    const bool blocked = Collisions::HasProjectileWallCollision(
        from, target.Position(), 0.0f);
    if (entry) {
        entry->tick = now;
        entry->networkId = networkId;
        entry->blocked = blocked;
    }
    return blocked;
}

inline bool IsValidCurrentAttackTarget(const AIHeroClient& player,
                                       const AttackableUnit& target) {
    return IsValidAttackTarget(player, target) &&
           IsTargetWithinCurrentAttackRange(player, target) &&
           !IsWallBlocked(player, target);
}

inline bool IsValidCurrentAttackTarget(const AttackableUnit& target) {
    return IsValidCurrentAttackTarget(GameObjects::Player(), target);
}
inline SDK::KuroTargetSelector::TargetRequest
MakeKuroAutoAttackExecutionRequest(const AIHeroClient& player,
                                    const AttackableUnit& target) {
    using namespace SDK::KuroTargetSelector;
    TargetRequest request = KuroTargetActionGate::MakeAutoAttackRequest(
        player.Position(),
        GetRealAutoAttackRange(player, target),
        DecisionPhase::Execution,
        0);
    request.Route.Kind = WillUseAzirSoldierAttack(player, target)
        ? RouteKind::NonProjectile
        : RouteKind::AutoAttack;
    request.Route.Start = player.ServerPosition();
    request.Route.ProjectileWallCheck =
        request.Route.Kind != RouteKind::NonProjectile;
    return request;
}

inline bool IsValidCurrentKuroAutoAttackTarget(
    const AIHeroClient& player,
    const AttackableUnit& target,
    SDK::KuroTargetSelector::IKuroTargetSelector* advanced) {
    if (!IsValidCurrentAttackTarget(player, target)) {
        return false;
    }
    if (!advanced || !target.IsHero()) {
        return true;
    }
    const AIHeroClient heroTarget(target.Handle());
    return heroTarget.IsValid() &&
        advanced->ValidateExecution(
            MakeKuroAutoAttackExecutionRequest(player, target),
            heroTarget);
}


inline bool IsGangplankBarrel(const AIMinionClient& minion) {
    return _stricmp(minion.CharacterName().c_str(), "gangplankbarrel") == 0;
}

inline bool HasGangplankInGame() {
    static bool checked = false;
    static bool hasGp = false;
    if (!checked) {
        for (const auto& hero : GameObjects::Heroes()) {
            if (hero.IsValid() &&
                SDK::ChampionIdFromName(hero.CharacterName().c_str()) ==
                    SDK::ChampionId::Gangplank) {
                hasGp = true;
                break;
            }
        }
        if (!GameObjects::Heroes().empty()) checked = true;
    }
    return hasGp;
}

inline bool IsIgnoredMinion(const AIMinionClient& minion) {
    return _stricmp(minion.CharacterName().c_str(), "jarvanivstandard") == 0;
}

inline bool IsValidMinionTarget(const AIMinionClient& minion, float range = FLT_MAX) {
    return !minion.IsPlant() &&
           !IsIgnoredMinion(minion) &&
           IsValidAttackTarget(minion, range);
}

inline bool IsValidMinionTarget(const AIHeroClient& player,
                                const AIMinionClient& minion,
                                float range = FLT_MAX) {
    return !minion.IsPlant() &&
           !IsIgnoredMinion(minion) &&
           IsValidAttackTarget(player, minion, range);
}

inline bool IsValidCurrentMinionTarget(const AIHeroClient& player,
                                       const AIMinionClient& minion) {
    return !minion.IsPlant() &&
           !IsIgnoredMinion(minion) &&
           IsValidCurrentAttackTarget(
               player, AttackableUnit(minion.Handle()));
}

inline bool IsSiegeMinion(const AIMinionClient& minion) {
    return HasFlag(minion.GetMinionType(), MinionTypes::Siege);
}

inline bool IsSuperMinion(const AIMinionClient& minion) {
    return HasFlag(minion.GetMinionType(), MinionTypes::Super);
}

inline bool HasMinion(const std::vector<AIMinionClient>& minions, const AIMinionClient& minion) {
    return std::any_of(minions.begin(), minions.end(), [&](const AIMinionClient& existing) {
        return existing.Compare(minion) ||
               (existing.NetworkId() != 0 && existing.NetworkId() == minion.NetworkId());
    });
}

inline void AddUniqueMinion(std::vector<AIMinionClient>& minions, const AIMinionClient& minion) {
    if (!HasMinion(minions, minion)) {
        minions.push_back(minion);
    }
}

inline void OrderLaneMinions(std::vector<AIMinionClient>& minions) {
    std::stable_sort(
        minions.begin(),
        minions.end(),
        [](const AIMinionClient& left, const AIMinionClient& right) {
            if (IsSiegeMinion(left) != IsSiegeMinion(right)) {
                return IsSiegeMinion(left);
            }
            if (IsSuperMinion(left) != IsSuperMinion(right)) {
                return !IsSuperMinion(left);
            }
            return left.Health() < right.Health();
        });
}

inline void OrderJungleMinions(std::vector<AIMinionClient>& minions, bool prioritizeSmallJungle) {
    std::stable_sort(
        minions.begin(),
        minions.end(),
        [prioritizeSmallJungle](const AIMinionClient& left, const AIMinionClient& right) {
            return prioritizeSmallJungle
                ? left.MaxHealth() < right.MaxHealth()
                : left.MaxHealth() > right.MaxHealth();
        });
}

struct MinionTargetLists {
    std::vector<AIMinionClient> targets;
    std::vector<AIMinionClient> laneMinions;
};

inline MinionTargetLists GetMinionsForMode(OrbwalkingMode mode,
                                           const OrbwalkerMenu& menu,
                                           const AIHeroClient& player) {
    MinionTargetLists lists;
    auto& result = lists.targets;
    auto& laneMinions = lists.laneMinions;
    if (mode == OrbwalkingMode::None || !player.IsValid()) {
        return lists;
    }

    const bool includeLaneAndJungleAndWard = mode != OrbwalkingMode::Combo;
    std::vector<AIMinionClient> jungleMinions;
    std::vector<AIMinionClient> wardMinions;
    std::vector<AIMinionClient> specialMinions;
    std::vector<AIMinionClient> cloneMinions;

    if (includeLaneAndJungleAndWard) {
        const auto& enemyMinions = GameObjects::EnemyMinions();
        laneMinions.reserve(enemyMinions.size());
        for (const auto& minion : enemyMinions) {
            if (IsValidCurrentMinionTarget(player, minion) &&
                !IsGangplankBarrel(minion)) {
                AddUniqueMinion(laneMinions, minion);
            }
        }
        OrderLaneMinions(laneMinions);

        const auto& jungle = GameObjects::Jungle();
        jungleMinions.reserve(jungle.size());
        for (const auto& minion : jungle) {
            if (IsValidCurrentMinionTarget(player, minion) &&
                !IsGangplankBarrel(minion)) {
                AddUniqueMinion(jungleMinions, minion);
            }
        }
        OrderJungleMinions(jungleMinions, menu.PrioritizeSmallJungle());

        if (menu.AttackWards()) {
            const auto& wards = GameObjects::EnemyWards();
            wardMinions.reserve(wards.size());
            for (const auto& ward : wards) {
                if (IsValidCurrentMinionTarget(player, ward)) {
                    AddUniqueMinion(wardMinions, ward);
                }
            }
        }
    }

    if (menu.AttackSpecialMinions()) {
        const auto& specials = GameObjects::EnemySpecialMinions();
        specialMinions.reserve(specials.size());
        for (const auto& minion : specials) {
            if (IsValidCurrentMinionTarget(player, minion)) {
                AddUniqueMinion(specialMinions, minion);
            }
        }
    }

    if (menu.AttackClones()) {
        const auto& clones = GameObjects::EnemyClones();
        cloneMinions.reserve(clones.size());
        for (const auto& clone : clones) {
            if (IsValidCurrentMinionTarget(player, clone)) {
                AddUniqueMinion(cloneMinions, clone);
            }
        }
    }

    result.reserve(
        laneMinions.size() +
        jungleMinions.size() +
        wardMinions.size() +
        specialMinions.size() +
        cloneMinions.size());

    auto append = [&result](const std::vector<AIMinionClient>& values) {
        for (const auto& minion : values) {
            AddUniqueMinion(result, minion);
        }
    };

    auto appendOrdinary = [&]() {
        append(laneMinions);
        append(jungleMinions);
    };

    if (menu.AttackWards() && menu.PrioritizeWards() &&
        menu.AttackSpecialMinions() && menu.PrioritizeSpecialMinions()) {
        append(wardMinions);
        append(specialMinions);
        appendOrdinary();
    } else if (menu.AttackSpecialMinions() && menu.PrioritizeSpecialMinions()) {
        append(specialMinions);
        appendOrdinary();
        append(wardMinions);
    } else if (menu.AttackWards() && menu.PrioritizeWards()) {
        append(wardMinions);
        appendOrdinary();
        append(specialMinions);
    } else {
        appendOrdinary();
        append(specialMinions);
        append(wardMinions);
    }

    if (menu.AttackBarrels() && HasGangplankInGame()) {
        for (const auto& minion : GameObjects::Minions()) {
            if (IsGangplankBarrel(minion) &&
                minion.Health() <= 1.0f &&
                IsValidCurrentAttackTarget(
                    player, AttackableUnit(minion.Handle()))) {
                AddUniqueMinion(result, minion);
            }
        }
    }

    append(cloneMinions);
    return lists;
}

struct CritAttackPrediction {
    float critChance = 0.0f;
    float nextCritProbability = 0.0f;
    float critDamageMultiplier = 2.0f;
};

inline CritAttackPrediction BuildCritAttackPrediction(
    const AIHeroClient& player,
    const FarmLogic::CritSequenceTracker& tracker
) {
    CritAttackPrediction result;
    if (!player.IsValid()) {
        return result;
    }

    result.critChance = FarmLogic::ClampProbability(player.Crit());
    result.nextCritProbability =
        tracker.PredictNextCritProbability(result.critChance);
    result.critDamageMultiplier =
        ::CoreAIHeroClient::CritDamageMultiplier(player.Address());
    if (!std::isfinite(result.critDamageMultiplier) ||
        result.critDamageMultiplier < 1.0f ||
        result.critDamageMultiplier > 4.0f) {
        result.critDamageMultiplier = 2.0f;
    }
    return result;
}

inline float PredictedLastHitDamage(const AIHeroClient& player,
                                    const AIMinionClient& minion,
                                    const CritAttackPrediction& critPrediction) {
    const bool applyCrit = FarmLogic::ShouldApplyPredictedCritDamage(
        critPrediction.nextCritProbability,
        critPrediction.critChance,
        IsSiegeMinion(minion));
    return GetPredictedAutoAttackDamage(
        player,
        minion,
        applyCrit,
        critPrediction.critDamageMultiplier);
}

struct LastHitEvaluation {
    AttackableUnit target = {};
    FarmLogic::LastHitWindowCandidate window = {};
    bool siege = false;
};

inline int EstimateLastHitClosingWindow(const AIMinionClient& minion,
                                        int timeToHit,
                                        int farmDelay,
                                        int horizonMs) {
    const int safeHorizon = std::max(0, horizonMs);
    for (int offset = 0; offset <= safeHorizon; offset += kLastHitWindowStepMs) {
        const float futureHealth = HealthPrediction::GetPrediction(
            minion,
            timeToHit + offset,
            farmDelay,
            HealthPredictionType::Simulated);
        if (futureHealth <= 0.0f) {
            return offset;
        }
    }
    return std::numeric_limits<int>::max();
}

inline LastHitEvaluation EvaluateLastHitMinion(
    const AIHeroClient& player,
    const AIMinionClient& minion,
    const CritAttackPrediction& critPrediction,
    int farmDelay,
    int windowHorizonMs,
    int stableOrder
) {
    LastHitEvaluation result;
    if (!IsValidCurrentMinionTarget(player, minion)) {
        return result;
    }

    const float damage = PredictedLastHitDamage(player, minion, critPrediction);
    if (damage <= 0.0f) {
        return result;
    }

    const int timeToHit = static_cast<int>(
        std::max(0.0f, Utils::AutoAttack::GetTimeToHit(minion)));
    const float predictedHealth = HealthPrediction::GetPrediction(
        minion,
        timeToHit,
        farmDelay);
    if (!FarmLogic::IsInsideLastHitDamageWindow(predictedHealth, damage)) {
        return result;
    }

    result.target = AttackableUnit(minion.Handle());
    result.siege = IsSiegeMinion(minion);
    result.window.valid = true;
    result.window.closingWindowMs = EstimateLastHitClosingWindow(
        minion, timeToHit, farmDelay, windowHorizonMs);
    result.window.boundarySafety =
        FarmLogic::LastHitBoundarySafety(predictedHealth, damage);
    result.window.predictedHealth = predictedHealth;
    result.window.stableOrder = stableOrder;
    return result;
}

inline LastHitEvaluation GetKillableMinion(
    const AIHeroClient& player,
    const std::vector<AIMinionClient>& laneMinions,
    const CritAttackPrediction& critPrediction,
    int farmDelay,
    int windowHorizonMs
) {
    LastHitEvaluation best;
    int stableOrder = 0;
    for (const auto& minion : laneMinions) {
        LastHitEvaluation candidate = EvaluateLastHitMinion(
            player,
            minion,
            critPrediction,
            farmDelay,
            windowHorizonMs,
            stableOrder++);
        if (!candidate.window.valid) {
            continue;
        }

        // A killable cannon is an absolute priority: do not let a lower-value
        // minion score displace it and lose the cannon during another cycle.
        if (candidate.siege) {
            return candidate;
        }

        if (FarmLogic::PreferLastHitCandidate(candidate.window, best.window)) {
            best = candidate;
        }
    }
    return best;
}

inline AttackableUnit GetHeroTarget(const AIHeroClient& player) {
    auto* advanced = SDK::KuroTargetSelector::ActiveService();
    const auto canUseForAttack =
        [&](const AttackableUnit& target) {
            return IsValidCurrentKuroAutoAttackTarget(
                player, target, advanced);
        };

    // A hard FocusLease is an owned champion hint, not a replacement for
    // legality.  If the leased target is outside live AA range, behind a
    // wall, or rejected at execution, suspend the lease semantically and let
    // selector ranking choose a fallback; the coordinator can restore it
    // later without losing identity.
    const auto lease = Plugins::AICombatTargetCoordinator::FocusLease::Snapshot(
        Variables::TickCount());
    int softLeaseTargetId = 0;
    if (lease.Status == Plugins::AICombatTargetCoordinator::LeaseStatus::Active &&
        !lease.ManualOverride && lease.TargetNetworkId > 0) {
        const auto focus = GameObjects::GetUnitByNetworkId<AIHeroClient>(
            lease.TargetNetworkId);
        const AttackableUnit focusUnit(focus.Handle());
        if (focus.IsValid() && canUseForAttack(focusUnit)) {
            if (lease.Strength ==
                    Plugins::AICombatTargetCoordinator::LeaseStrength::Hard) {
                return focusUnit;
            }
            softLeaseTargetId = lease.TargetNetworkId;
        } else {
            (void)Plugins::AICombatTargetCoordinator::FocusLease::BlockedTarget(
                lease.TargetNetworkId, Variables::TickCount());
        }
    }

    // KuroTargetSelector owns enemy-hero planning only.  Candidate selection
    // still uses the exact execution validation used by Attack(), so a
    // rejected first candidate cannot consume a full attack/move cycle.
    if (advanced) {
        using namespace SDK::KuroTargetSelector;
        TargetRequest request = KuroTargetActionGate::MakeAutoAttackRequest(
            player.Position(), FLT_MAX, DecisionPhase::Planning, 0);
        // The orbwalker has a champion-specific final wall gate (including
        // Azir soldier exceptions), so planning asks only for enemy-hero
        // ranking and lets the execution request own the exact attack route.
        request.Route.Kind = RouteKind::NonProjectile;
        request.Route.ProjectileWallCheck = false;
        request.Route.RequireLineOfSight = false;
        request.RespectManualSelection = true;
        const auto selection = advanced->GetSelectionState();
        const bool explicitManual = selection.ManualOverrideActive ||
            (selection.PreferSelectedTarget &&
             selection.SelectedNetworkId > 0);
        if (softLeaseTargetId > 0 && !explicitManual) {
            request.PreferredTargetId = softLeaseTargetId;
        }

        for (const auto& decision : advanced->Rank(request)) {
            if (!decision.Legal || !decision.Target.IsValid()) continue;
            const AttackableUnit target(decision.Target.Handle());
            if (canUseForAttack(target)) {
                return target;
            }
        }
    }

    // TargetSelector::Instance() is the facade, so when Kuro is current its
    // legacy GetTargets() call would rank the same snapshot a second time.
    if (!advanced || TargetSelector::CurrentTargetSelectorName() != "Kuro") {
        if (auto* selector = TargetSelector::Instance()) {
            const auto targets = selector->GetTargets(FLT_MAX, DamageType::True);
            for (const auto& hero : targets) {
                const AttackableUnit target(hero.Handle());
                if (canUseForAttack(target)) {
                    return target;
                }
            }
        }
    }

    for (const auto& hero : GameObjects::EnemyHeroes()) {
        const AttackableUnit target(hero.Handle());
        if (canUseForAttack(target)) {
            return target;
        }
    }
    return {};
}

inline bool HasEnemyHeroNearAutoAttackRange(const AIHeroClient& player) {
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid() || enemy.IsDead()) {
            continue;
        }

        const AttackableUnit target(enemy.Handle());
        if (IsValidAttackTarget(player, target) &&
            IsTargetWithinCurrentAttackRange(player, target, 2.0f)) {
            return true;
        }
    }
    return false;
}

inline AttackableUnit FirstValidMinionTarget(const AIHeroClient& player,
                                             const std::vector<AIMinionClient>& minions) {
    for (const auto& minion : minions) {
        if (IsValidCurrentMinionTarget(player, minion)) {
            return AttackableUnit(minion.Handle());
        }
    }
    return {};
}

inline AttackableUnit GetComboFallbackCandidate(const OrbwalkerMenu& menu,
                                                const AIHeroClient& player) {
    if (menu.AttackSpecialMinions()) {
        const AttackableUnit special =
            FirstValidMinionTarget(player, GameObjects::EnemySpecialMinions());
        if (special.IsValid()) {
            return special;
        }
    }

    if (menu.AttackBarrels() && HasGangplankInGame()) {
        for (const auto& minion : GameObjects::Minions()) {
            if (IsGangplankBarrel(minion) &&
                minion.Health() <= 1.0f &&
                IsValidCurrentAttackTarget(
                    player, AttackableUnit(minion.Handle()))) {
                return AttackableUnit(minion.Handle());
            }
        }
    }

    if (menu.AttackClones()) {
        return FirstValidMinionTarget(player, GameObjects::EnemyClones());
    }

    return {};
}

inline AttackableUnit GetComboFallbackTarget(const OrbwalkerMenu& menu,
                                             const AIHeroClient& player) {
    const AttackableUnit candidate = GetComboFallbackCandidate(menu, player);
    if (!candidate.IsValid()) {
        return {};
    }

    // Keep combo DPS for champions; fallback minions are only for quiet moments.
    return HasEnemyHeroNearAutoAttackRange(player) ? AttackableUnit() : candidate;
}

inline bool IsSoonKillableMinion(const AIHeroClient& player,
                                 const AIMinionClient& minion,
                                 const CritAttackPrediction& critPrediction,
                                 int farmDelay,
                                 int predictionTime) {
    if (!IsValidCurrentMinionTarget(player, minion)) {
        return false;
    }

    const float damage = PredictedLastHitDamage(player, minion, critPrediction);
    if (damage <= 0.0f) {
        return false;
    }

    const int timeToHit = static_cast<int>(
        std::max(0.0f, Utils::AutoAttack::GetTimeToHit(minion)));
    const int horizon = std::max(timeToHit, predictionTime);
    for (int sampleTime = timeToHit;
         sampleTime <= horizon;
         sampleTime += kLastHitWindowStepMs) {
        const float predictedHealth = HealthPrediction::GetPrediction(
            minion,
            sampleTime,
            farmDelay,
            HealthPredictionType::Simulated);
        if (predictedHealth <= 0.0f) {
            return false;
        }
        if (FarmLogic::IsInsideLastHitDamageWindow(predictedHealth, damage)) {
            return true;
        }
    }
    return false;
}

inline bool HasSoonKillableMinion(const AIHeroClient& player,
                                  const std::vector<AIMinionClient>& minions,
                                  const AIMinionClient& skip,
                                  const CritAttackPrediction& critPrediction,
                                  int farmDelay,
                                  int predictionTime) {
    for (const auto& minion : minions) {
        if ((skip.IsValid() && skip.Compare(minion)) ||
            IsGangplankBarrel(minion) ||
            !IsSoonKillableMinion(
                player,
                minion,
                critPrediction,
                farmDelay,
                predictionTime)) {
            continue;
        }
        return true;
    }
    return false;
}

inline bool HasSoonKillableMinion(const AIHeroClient& player,
                                  const AIMinionClient& skip,
                                  const CritAttackPrediction& critPrediction,
                                  int farmDelay,
                                  int predictionTime) {
    return HasSoonKillableMinion(
        player,
        GameObjects::EnemyMinions(),
        skip,
        critPrediction,
        farmDelay,
        predictionTime);
}

} // namespace OrbwalkerKuro::OrbwalkingDetail

namespace OrbwalkerKuro {

inline AttackableUnit OrbwalkerBase::GetTarget() {
    const int now = Tick();
    OrbwalkingDetail::WallCheckEnabled = menu_.WindWallCheck();
    const auto player = GameObjects::Player();
    auto* advanced = SDK::KuroTargetSelector::ActiveService();
    if (!player.IsValid() || player.IsDead() || !menu_.Enabled()) {
        context_.cachedTargetTick = -1;
        context_.cachedShouldWaitTick = -1;
        return {};
    }

    const OrbwalkingMode mode = context_.activeMode != OrbwalkingMode::None
        ? context_.activeMode
        : ActiveMode();
    if (mode == OrbwalkingMode::None) {
        context_.cachedTargetTick = -1;
        context_.cachedShouldWaitTick = -1;
        return {};
    }

    const bool farmMode =
        mode == OrbwalkingMode::LaneClear ||
        mode == OrbwalkingMode::Hybrid ||
        mode == OrbwalkingMode::Harass ||
        mode == OrbwalkingMode::LastHit;

    const int forceTargetNetworkId = context_.forceTarget.IsValid()
        ? context_.forceTarget.NetworkId()
        : 0;
    constexpr int kTargetSelectionThrottleMs = 35;
    // Farm targets are deliberately re-evaluated every call. A target that was
    // killable 20 ms ago may now already be covered by an allied projectile;
    // reusing it merely because it is still alive is a common missed-CS cause.
    if (!farmMode &&
        context_.cachedTargetTick > 0 &&
        now - context_.cachedTargetTick >= 0 &&
        now - context_.cachedTargetTick < kTargetSelectionThrottleMs &&
        context_.cachedTargetMode == mode &&
        context_.cachedTargetForceTargetNetworkId == forceTargetNetworkId) {
        if (!context_.cachedTarget.IsValid() ||
            OrbwalkingDetail::IsValidCurrentAttackTarget(
                player, context_.cachedTarget)) {
            return context_.cachedTarget;
        }
    }

    auto cacheTarget = [&](const AttackableUnit& target) -> AttackableUnit {
        context_.cachedTarget = target;
        context_.cachedTargetTick = now;
        context_.cachedTargetMode = mode;
        context_.cachedTargetForceTargetNetworkId = forceTargetNetworkId;
        return target;
    };

    ReadAttackTimingsFromMemory(player);
    const int farmDelay = menu_.DelayFarm();
    const int lastHitWindowHorizonMs = std::clamp(
        static_cast<int>(OrbwalkingDetail::kLaneClearWaitCycles *
            (context_.attackDelayMs + context_.attackWindupMs)),
        500,
        2500);
    const OrbwalkingDetail::CritAttackPrediction critPrediction =
        OrbwalkingDetail::BuildCritAttackPrediction(player, context_.critSequence);

    AttackableUnit cachedHeroTarget;
    bool heroTargetResolved = false;
    auto getHeroTarget = [&]() -> AttackableUnit {
        if (!heroTargetResolved) {
            cachedHeroTarget = OrbwalkingDetail::GetHeroTarget(player);
            heroTargetResolved = true;
        }
        return cachedHeroTarget;
    };

    if (mode == OrbwalkingMode::Combo) {
        if (context_.forceTarget.IsValid() &&
            OrbwalkingDetail::IsValidCurrentKuroAutoAttackTarget(
                player, context_.forceTarget, advanced)) {
            return cacheTarget(context_.forceTarget);
        }

        const AttackableUnit target = getHeroTarget();
        if (target.IsValid()) {
            return cacheTarget(target);
        }

        const AttackableUnit fallbackTarget =
            OrbwalkingDetail::GetComboFallbackTarget(menu_, player);
        if (fallbackTarget.IsValid()) {
            return cacheTarget(fallbackTarget);
        }
        return cacheTarget(AttackableUnit());
    }

    const auto minionLists = OrbwalkingDetail::GetMinionsForMode(mode, menu_, player);
    const auto& minions = minionLists.targets;

    OrbwalkingDetail::LastHitEvaluation killableMinion;
    if (farmMode) {
        killableMinion = OrbwalkingDetail::GetKillableMinion(
            player,
            minionLists.laneMinions,
            critPrediction,
            farmDelay,
            lastHitWindowHorizonMs);

        // A cannon that is already inside its last-hit window is absolute:
        // it must win even when the menu normally prioritizes a champion.
        if (killableMinion.siege && killableMinion.target.IsValid()) {
            return cacheTarget(killableMinion.target);
        }
    }

    if ((mode == OrbwalkingMode::Hybrid || mode == OrbwalkingMode::LaneClear) &&
        !menu_.PrioritizeFarm()) {
        const AttackableUnit target = getHeroTarget();
        if (target.IsValid()) {
            return cacheTarget(target);
        }
    }

    if (killableMinion.target.IsValid()) {
        return cacheTarget(killableMinion.target);
    }

    if (context_.forceTarget.IsValid() &&
        OrbwalkingDetail::IsValidCurrentKuroAutoAttackTarget(
            player, context_.forceTarget, advanced)) {
        return cacheTarget(context_.forceTarget);
    }

    // This gate must run before heroes, jungle, structures, and lane-clear
    // fillers. Previously it lived near the end of GetTarget(), after the hero
    // branch had already returned, so LaneClear consumed its next attack even
    // while a lane minion was about to enter the last-hit window.
    if (mode == OrbwalkingMode::LaneClear) {
        const bool shouldWait = OrbwalkingDetail::HasSoonKillableMinion(
            player,
            minionLists.laneMinions,
            {},
            critPrediction,
            farmDelay,
            lastHitWindowHorizonMs);
        context_.cachedShouldWait = shouldWait;
        context_.cachedShouldWaitTick = now;
        if (shouldWait) {
            return cacheTarget(AttackableUnit());
        }
    }

    if (mode == OrbwalkingMode::LaneClear &&
        (!menu_.PrioritizeMinions() || minions.empty())) {
        // for (const auto& turret : GameObjects::EnemyTurrets()) {
        //     const AttackableUnit target(turret.Handle());
        //     if (OrbwalkingDetail::IsValidCurrentAttackTarget(player, target)) {
        //         return cacheTarget(target);
        //     }
        // }
        // for (const auto& inhibitor : GameObjects::EnemyInhibitors()) {
        //     const AttackableUnit target(inhibitor.Handle());
        //     if (OrbwalkingDetail::IsValidCurrentAttackTarget(player, target)) {
        //         return cacheTarget(target);
        //     }
        // }
        // const auto nexus = GameObjects::EnemyNexus();
        // const AttackableUnit nexusTarget(nexus.Handle());
        // if (OrbwalkingDetail::IsValidCurrentAttackTarget(
        //         player, nexusTarget)) {
        //     return cacheTarget(nexusTarget);
        // }
    }

    if (mode != OrbwalkingMode::LastHit) {
        const AttackableUnit target = getHeroTarget();
        if (target.IsValid()) {
            return cacheTarget(target);
        }
    }

    // Jungle camps are Neutral team, so the LaneClear last-hit loop below
    // deliberately skips them (canLaneClear rejects Neutral). The backup
    // OrbwalkerSelector selected jungle through a dedicated GetJungleTarget()
    // branch for Harass/LaneClear/LastHit; mirror that here. `minions` already
    // has the jungle camps appended in priority order (OrderJungleMinions honors
    // PrioritizeSmallJungle), so the first valid Neutral entry is the pick.
    if (mode == OrbwalkingMode::Harass ||
        mode == OrbwalkingMode::LaneClear ||
        mode == OrbwalkingMode::LastHit) {
        for (const auto& minion : minions) {
            if (minion.Team() != GameObjectTeam::Neutral) {
                continue;
            }
            if (OrbwalkingDetail::IsValidCurrentMinionTarget(player, minion)) {
                return cacheTarget(AttackableUnit(minion.Handle()));
            }
        }
    }

    if (farmMode) {
        // std::vector<AIMinionClient> turretMinions;
        // turretMinions.reserve(minions.size());
        // for (const auto& minion : minions) {
        //     if (minion.Team() != GameObjectTeam::Neutral &&
        //         minion.IsMinion() &&
        //         minion.IsUnderAllyTurret()) {
        //         turretMinions.push_back(minion);
        //     }
        // }
        //
        // if (!turretMinions.empty()) {
        //     const auto& allyTurrets = GameObjects::AllyTurrets();
        //     for (const auto& minion : turretMinions) {
        //         if (HealthPrediction::HasMinionAggro(minion)) {
        //             continue;
        //         }
        //
        //         const float playerDamage =
        //             OrbwalkingDetail::GetCurrentAutoAttackDamage(player, minion);
        //         if (playerDamage <= 0.0f) {
        //             continue;
        //         }
        //
        //         for (const auto& turret : allyTurrets) {
        //             if (!turret.IsValid() ||
        //                 turret.IsDead() ||
        //                 turret.Position().DistanceSqr2D(minion.Position()) > 950.0f * 950.0f) {
        //                 continue;
        //             }
        //             const float turretDamage = std::max(1.0f, turret.GetAutoAttackDamage(minion, false));
        //             if (std::fmod(std::max(0.0f, minion.Health()), turretDamage) > playerDamage) {
        //                 return cacheTarget(AttackableUnit(minion.Handle()));
        //             }
        //         }
        //     }
        // }
    }

    if (mode == OrbwalkingMode::LaneClear) {
        auto canLaneClear = [&](const AIMinionClient& minion) {
            if (!OrbwalkingDetail::IsValidCurrentMinionTarget(player, minion) ||
                minion.Team() == GameObjectTeam::Neutral) {
                return false;
            }
            if (minion.MaxHealth() <= 10.0f) {
                return true;
            }

            const float damage =
                OrbwalkingDetail::GetCurrentAutoAttackDamage(player, minion);
            if (damage <= 0.0f) {
                return false;
            }

            return true;
        };

        const AIMinionClient currentLaneClearMinion(context_.laneClearMinion.Handle());
        if (currentLaneClearMinion.IsValid() && canLaneClear(currentLaneClearMinion)) {
            return cacheTarget(AttackableUnit(currentLaneClearMinion.Handle()));
        }

        for (const auto& minion : minions) {
            if (canLaneClear(minion)) {
                context_.laneClearMinion = minion;
                return cacheTarget(AttackableUnit(minion.Handle()));
            }
        }
    }

    return cacheTarget(AttackableUnit());
}

inline bool OrbwalkerBase::ShouldWait() {
    const int now = Tick();
    if (context_.cachedShouldWaitTick == now) {
        return context_.cachedShouldWait;
    }

    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) {
        context_.cachedShouldWait = false;
        context_.cachedShouldWaitTick = now;
        return false;
    }

    ReadAttackTimingsFromMemory(player);
    const int predictionTime = std::clamp(
        static_cast<int>(OrbwalkingDetail::kLaneClearWaitCycles *
            (context_.attackDelayMs + context_.attackWindupMs)),
        500,
        2500);
    const OrbwalkingDetail::CritAttackPrediction critPrediction =
        OrbwalkingDetail::BuildCritAttackPrediction(player, context_.critSequence);
    context_.cachedShouldWait = OrbwalkingDetail::HasSoonKillableMinion(
        player,
        {},
        critPrediction,
        menu_.DelayFarm(),
        predictionTime);
    context_.cachedShouldWaitTick = now;
    return context_.cachedShouldWait;
}

inline AttackableUnit OrbwalkerBase::ResolveAttackTarget(const Events::ProcessSpellEventArgs& args) const {
    if (args.Target.IsValid()) {
        return AttackableUnit(args.Target.Ptr);
    }
    if (args.TargetNetworkId != 0 && args.TargetNetworkId != 0xFFFFFFFFu) {
        return GameObjects::GetUnitByNetworkId<AttackableUnit>(
            static_cast<int>(args.TargetNetworkId));
    }
    if (context_.pendingAttackTargetNetworkId != 0) {
        return GameObjects::GetUnitByNetworkId<AttackableUnit>(
            context_.pendingAttackTargetNetworkId);
    }
    return {};
}

inline AttackableUnit OrbwalkerBase::ResolveAttackTarget(const Events::ObjectEventArgs& args) const {
    if (args.Target.IsValid()) {
        return AttackableUnit(args.Target.Ptr);
    }
    if (args.TargetNetworkId != 0 && args.TargetNetworkId != 0xFFFFFFFFu) {
        return GameObjects::GetUnitByNetworkId<AttackableUnit>(
            static_cast<int>(args.TargetNetworkId));
    }
    if (context_.pendingAttackTargetNetworkId != 0) {
        return GameObjects::GetUnitByNetworkId<AttackableUnit>(
            context_.pendingAttackTargetNetworkId);
    }
    return {};
}

} // namespace OrbwalkerKuro
