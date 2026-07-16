#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>

namespace Plugins::KuroAIO::Senna {

struct SoulInfo {
    AIBaseClient Unit;
    int ExpireTick = 0;
};

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* RBlacklistMenu = nullptr;
inline Menu* HealMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* ClearMenu = nullptr;
inline Menu* JungleMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 600.0f };
inline Spell ExtendedQ{ SpellSlot::Q, 1300.0f };
inline Spell W{ SpellSlot::W, 1200.0f };
inline Spell E{ SpellSlot::E, 400.0f };
inline Spell R{ SpellSlot::R, 25000.0f };

inline std::vector<SoulInfo> Souls;
inline int ForcedPassiveTargetNetworkId = 0;
inline bool Loaded = false;

constexpr float ExtraQRange = 1300.0f;
constexpr float ExtraQDamageWidth = 50.0f;
constexpr float ExtraQHealWidth = 140.0f;
constexpr int SoulLifetimeMs = 8000;
constexpr int MinimumQAttackWaitMs = 180;
constexpr int MaximumQAttackWaitMs = 350;

static bool ValidAlly(const AIHeroClient& ally, float range = FLT_MAX) {
    const auto player = Player();
    if (!player.IsValid() || !ally.IsValid() || ally.IsDead() ||
        ally.IsInvulnerable() || !ally.IsVisible() || !ally.IsTargetable() ||
        ally.Team() != player.Team()) {
        return false;
    }

    return range >= FLT_MAX || player.Position().DistanceSqr2D(ally.Position()) <= range * range;
}

static bool SameUnit(const GameObject& left, const GameObject& right) {
    if (!left.IsValid() || !right.IsValid()) {
        return false;
    }
    if (left.Address() == right.Address()) {
        return true;
    }
    return left.NetworkId() != 0 && left.NetworkId() == right.NetworkId();
}

static float PointSegmentDistance2D(
    const Vector3& point,
    const Vector3& segmentStart,
    const Vector3& segmentEnd) {
    const float dx = segmentEnd.x - segmentStart.x;
    const float dz = segmentEnd.z - segmentStart.z;
    const float lengthSqr = dx * dx + dz * dz;
    if (lengthSqr <= FLT_EPSILON) {
        return point.Distance2D(segmentStart);
    }

    const float px = point.x - segmentStart.x;
    const float pz = point.z - segmentStart.z;
    const float projection = std::clamp((px * dx + pz * dz) / lengthSqr, 0.0f, 1.0f);
    const float closestX = segmentStart.x + projection * dx;
    const float closestZ = segmentStart.z + projection * dz;
    const float offsetX = point.x - closestX;
    const float offsetZ = point.z - closestZ;
    return std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
}

static std::string RBlockKey(const AIHeroClient& ally) {
    const std::string champion = ally.CharacterName();
    if (!champion.empty()) {
        return "BlockR." + champion;
    }
    return "BlockR." + std::to_string(ally.NetworkId());
}

static bool IsRBlocked(const AIHeroClient& ally) {
    if (!RBlacklistMenu) {
        return false;
    }
    const std::string key = RBlockKey(ally);
    const auto* item = RBlacklistMenu->Get<MenuBool>(key.c_str());
    return item && item->Value;
}

static bool IsSoulObject(const GameObject& object) {
    if (!object.IsValid() || !object.IsMinion()) {
        return false;
    }

    const AIMinionClient minion(object.Handle());
    if (!minion.IsValid() || minion.Team() != GameObjectTeam::Neutral ||
        minion.IsAlly() || std::abs(minion.MaxHealth() - 1.0f) > 0.1f) {
        return false;
    }

    const std::string name = GetObjectName(object);
    const std::string characterName = GetObjectCharacterName(object);
    return EqualsIgnoreCase(name.c_str(), "Barrel") ||
           EqualsIgnoreCase(characterName.c_str(), "Barrel");
}

static void TrackSoul(const GameObject& object) {
    if (!IsSoulObject(object)) {
        return;
    }

    const AIBaseClient soul(
        object.Address(),
        ::Core::Objects::ObjectType::AIMinionClient);
    for (auto& entry : Souls) {
        if (SameUnit(entry.Unit, soul)) {
            entry.ExpireTick = SDK::Variables::TickCount() + SoulLifetimeMs;
            return;
        }
    }

    Souls.push_back({ soul, SDK::Variables::TickCount() + SoulLifetimeMs });
}

static bool ValidSoul(const SoulInfo& soul) {
    return soul.ExpireTick > SDK::Variables::TickCount() &&
           soul.Unit.IsValid() && !soul.Unit.IsDead() &&
           soul.Unit.Health() > 0.0f && soul.Unit.IsTargetable();
}

static void PruneSouls() {
    Souls.erase(
        std::remove_if(Souls.begin(), Souls.end(), [](const SoulInfo& soul) {
            return !ValidSoul(soul);
        }),
        Souls.end());
}

static void OnObjectCreate(const GameObject& object) {
    TrackSoul(object);
}

static void OnObjectDelete(const GameObject& object) {
    Souls.erase(
        std::remove_if(Souls.begin(), Souls.end(), [&](const SoulInfo& soul) {
            return SameUnit(soul.Unit, object);
        }),
        Souls.end());
}

static void InitializeSouls() {
    Souls.clear();
    for (const auto& object : GameObjects::AllGameObjects()) {
        TrackSoul(object);
    }
}

static void UpdateSpellData() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const float attackSpeedReduction = std::clamp(
        0.02f * ((player.AttackSpeedMod() - 1.0f) / 0.25f),
        0.0f,
        0.2f);
    const float qDelay = 0.4f - attackSpeedReduction;
    Q.Delay = qDelay;
    ExtendedQ.Delay = qDelay;

    const int passiveStacks = std::max(0, player.GetBuffCount("SennaPassiveStacks"));
    if (passiveStacks >= 400) {
        Q.Range = 1100.0f;
    } else {
        Q.Range = 600.0f + static_cast<float>((passiveStacks / 20) * 25) +
                  player.BoundingRadius();
    }
}

static bool ValidQCastTarget(const AIBaseClient& unit) {
    const auto player = Player();
    return player.IsValid() && unit.IsValid() && !unit.IsDead() &&
           !SameUnit(player, unit) && unit.IsVisible() && unit.IsTargetable() &&
           player.Position().DistanceSqr2D(unit.Position()) <= Q.Range * Q.Range;
}

static std::vector<AIBaseClient> QCastTargets() {
    std::vector<AIBaseClient> result;
    std::unordered_set<uintptr_t> seen;

    const auto add = [&](const auto& object) {
        const AIBaseClient unit(object.Handle());
        if (!ValidQCastTarget(unit) || !seen.insert(unit.Address()).second) {
            return;
        }
        result.push_back(unit);
    };

    for (const auto& minion : GameObjects::EnemyMinions()) add(minion);
    for (const auto& minion : GameObjects::AllyMinions()) add(minion);
    for (const auto& monster : GameObjects::Jungle()) add(monster);
    for (const auto& hero : GameObjects::EnemyHeroes()) add(hero);
    for (const auto& turret : GameObjects::Turrets()) add(turret);
    for (const auto& ward : GameObjects::Wards()) add(ward);

    return result;
}

static bool CastExtendedQAt(const Vector3& desiredHitPosition, float lineWidth) {
    const auto player = Player();
    if (!player.IsValid() || !Q.IsReady() || desiredHitPosition.IsZero()) {
        return false;
    }

    const Vector3 start = player.Position();
    AIBaseClient bestBridge;
    float bestDistance = FLT_MAX;

    for (const auto& bridge : QCastTargets()) {
        const Vector3 bridgePosition = bridge.Position();
        if (bridgePosition.IsZero() || start.DistanceSqr2D(bridgePosition) <= FLT_EPSILON) {
            continue;
        }

        const Vector3 lineEnd = start.Extend(bridgePosition, ExtraQRange);
        const float distance = PointSegmentDistance2D(desiredHitPosition, start, lineEnd);
        if (distance <= lineWidth && distance < bestDistance) {
            bestBridge = bridge;
            bestDistance = distance;
        }
    }

    return bestBridge.IsValid() && Q.CastOnUnit(bestBridge);
}

static bool CastNormalQ(const AIBaseClient& preferredTarget = AIBaseClient()) {
    const AIBaseClient target = preferredTarget.IsValid()
        ? preferredTarget
        : AIBaseClient(GetPhysicalTarget(Q.Range).Handle());
    return ValidTarget(target, Q.Range) && Q.CastOnUnit(target);
}

static bool CastExtendedDamageQ() {
    const auto target = GetPhysicalTarget(ExtraQRange);
    if (!ValidHeroTarget(target, ExtraQRange)) {
        return false;
    }

    const auto prediction = ExtendedQ.GetPrediction(target, true);
    if (static_cast<int>(prediction.Hitchance) < static_cast<int>(HitChance::High)) {
        return false;
    }

    return CastExtendedQAt(prediction.GetCastPosition(), ExtraQDamageWidth);
}

static bool TryHealAlly(const AIHeroClient& ally) {
    if (!ValidAlly(ally, ExtraQRange) || SameUnit(ally, Player())) {
        return false;
    }

    if (Player().Position().DistanceSqr2D(ally.Position()) <= Q.Range * Q.Range) {
        return Q.CastOnUnit(ally);
    }
    return CastExtendedQAt(ally.Position(), ExtraQHealWidth);
}

static std::vector<AIHeroClient> InjuredAllies(float range, float maximumHealthPercent) {
    std::vector<AIHeroClient> allies;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (ValidAlly(ally, range) && !SameUnit(ally, Player()) &&
            ally.HealthPercent() < 100.0f && ally.HealthPercent() <= maximumHealthPercent) {
            allies.push_back(ally);
        }
    }

    std::sort(allies.begin(), allies.end(), [](const AIHeroClient& left, const AIHeroClient& right) {
        return left.Health() < right.Health();
    });
    return allies;
}

static bool FastHeal() {
    for (const auto& ally : InjuredAllies(ExtraQRange, 100.0f)) {
        if (TryHealAlly(ally)) {
            return true;
        }
    }

    const auto player = Player();
    if (!player.IsValid() || player.HealthPercent() >= 100.0f) {
        return false;
    }

    for (const auto& ward : GameObjects::AllyWards()) {
        const AIBaseClient wardTarget(ward.Handle());
        if (ValidQCastTarget(wardTarget) && Q.CastOnUnit(wardTarget)) {
            return true;
        }
    }
    return false;
}

static bool HealAllyLogic() {
    if (!Q.IsReady()) {
        return false;
    }

    const int mode = List(HealMenu, "QHealMode", 0);
    if (mode == 2) {
        return false;
    }

    if (Key(HealMenu, "FastHeal") && FastHeal()) {
        return true;
    }

    if (mode == 1 && !IsComboMode()) {
        return false;
    }

    const float healthThreshold = static_cast<float>(Slider(HealMenu, "QHealHealth", 15));
    for (const auto& ally : InjuredAllies(ExtraQRange, healthThreshold)) {
        if (TryHealAlly(ally)) {
            return true;
        }
    }
    return false;
}

static int EnemyTeamCountNear(const AIHeroClient& center, float range, bool includeCenter = false) {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid() || enemy.IsDead() || (!includeCenter && SameUnit(enemy, center))) {
            continue;
        }
        if (enemy.Position().DistanceSqr2D(center.Position()) <= rangeSqr) {
            ++count;
        }
    }
    return count;
}

static int RArrivalTimeMs(const AIBaseClient& unit) {
    const auto player = Player();
    if (!player.IsValid() || !unit.IsValid()) {
        return 0;
    }
    const float travelSeconds = R.Speed > 1.0f && R.Speed < FLT_MAX
        ? player.Position().Distance2D(unit.Position()) / R.Speed
        : 0.0f;
    return static_cast<int>((R.Delay + travelSeconds) * 1000.0f);
}

static bool TryRHeal() {
    if (!Bool(RMenu, "RHealAlly", true)) {
        return false;
    }

    const float healthThreshold = static_cast<float>(Slider(RMenu, "RHealHealth", 15));
    const int requiredEnemies = Slider(RMenu, "RHealEnemies", 1);
    const bool usePrediction = Bool(RMenu, "RHealthPrediction", true);

    std::vector<AIHeroClient> allies;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (ValidAlly(ally, R.Range) && !IsRBlocked(ally) &&
            ally.HealthPercent() <= healthThreshold &&
            ally.CountEnemyHeroesInRange(400.0f) >= requiredEnemies) {
            allies.push_back(ally);
        }
    }

    std::sort(allies.begin(), allies.end(), [](const AIHeroClient& left, const AIHeroClient& right) {
        return left.HealthPercent() < right.HealthPercent();
    });

    for (const auto& ally : allies) {
        const int arrival = RArrivalTimeMs(ally);
        if (usePrediction) {
            const float healthAtArrival = SDK::HealthPrediction::GetPrediction(ally, arrival);
            const float healthAfterWindow = SDK::HealthPrediction::GetPrediction(ally, arrival + 500);
            if (healthAtArrival <= 0.0f || healthAfterWindow > 0.0f) {
                continue;
            }
        }

        if (R.Cast(ally.Position())) {
            return true;
        }
    }
    return false;
}

static bool CanExecuteWithR(const AIHeroClient& enemy, float predictedHealth) {
    if (predictedHealth <= 0.0f ||
        (enemy.HealthPercent() <= 10.0f && EnemyTeamCountNear(enemy, 400.0f) >= 1)) {
        return false;
    }

    const float damage = R.GetDamage(enemy);
    return damage > 0.0f && predictedHealth < damage;
}

static bool TryRExecute() {
    if (!Bool(RMenu, "RExecute", true)) {
        return false;
    }

    const auto player = Player();
    if (!player.IsValid() || player.CountEnemyHeroesInRange(800.0f) > 0 ||
        player.CountEnemyHeroesInRange(600.0f) > Slider(RMenu, "RPlayerEnemies", 1) ||
        Orbwalker::IsWindingUp()) {
        return false;
    }

    for (const auto& enemy : EnemyHeroesByHealth(R.Range)) {
        const int arrival = RArrivalTimeMs(enemy);
        const float predictedHealth = SDK::HealthPrediction::GetPrediction(enemy, arrival);
        if (!CanExecuteWithR(enemy, predictedHealth)) {
            continue;
        }

        const auto prediction = R.GetPrediction(enemy, true);
        if (static_cast<int>(prediction.Hitchance) < static_cast<int>(HitChance::VeryHigh)) {
            continue;
        }

        const Vector3 castPosition = prediction.GetCastPosition();
        if (Collisions::HasYasuoWindWallCollision(
                player.Position(), castPosition, R.Width)) {
            continue;
        }

        if (R.Cast(castPosition)) {
            return true;
        }
    }
    return false;
}

static bool AutoRLogic() {
    if (!R.IsReady() || R.Level() <= 0) {
        return false;
    }

    const int mode = List(RMenu, "RMode", 0);
    if (mode == 2 || (mode == 1 && !IsComboMode())) {
        return false;
    }

    return TryRHeal() || TryRExecute();
}

static bool CastW() {
    const auto target = GetPhysicalTarget(W.Range);
    return ValidHeroTarget(target, W.Range) &&
           W.CastIfHitchanceMinimum(target, HitChance::VeryHigh) ==
               CastStates::SuccessfullyCasted;
}

static void UpdatePassiveTarget() {
    if (!IsComboMode() || List(ComboMenu, "TargetMode", 0) != 0) {
        if (ForcedPassiveTargetNetworkId != 0) {
            Orbwalker::ForceTarget(AttackableUnit());
            ForcedPassiveTargetNetworkId = 0;
        }
        return;
    }

    AIHeroClient bestTarget;
    int bestPriority = -1;
    auto* selector = SDK::TargetSelector::Instance();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, AutoAttack::GetRealAutoAttackRange(Player(), enemy)) ||
            !enemy.HasBuff("sennapassivemarker")) {
            continue;
        }

        const int priority = selector ? selector->GetPriority(enemy) : 1;
        if (!bestTarget.IsValid() || priority > bestPriority) {
            bestTarget = enemy;
            bestPriority = priority;
        }
    }

    if (bestTarget.IsValid()) {
        Orbwalker::ForceTarget(bestTarget);
        ForcedPassiveTargetNetworkId = bestTarget.NetworkId();
    } else if (ForcedPassiveTargetNetworkId != 0) {
        Orbwalker::ForceTarget(AttackableUnit());
        ForcedPassiveTargetNetworkId = 0;
    }
}

static bool CastComboQ(const AIBaseClient& preferredTarget = AIBaseClient()) {
    if (!Bool(ComboMenu, "UseQ", true) || !Q.IsReady()) {
        return false;
    }
    if (CastNormalQ(preferredTarget)) {
        return true;
    }
    return Bool(ComboMenu, "UseExtendedQ", true) && CastExtendedDamageQ();
}

static bool HasAttackableComboTarget() {
    const auto orbTarget = Orbwalker::GetTarget();
    if (orbTarget.IsValid() && orbTarget.IsHero()) {
        const AIHeroClient target(orbTarget.Handle());
        if (ValidHeroTarget(target) && AutoAttack::InAutoAttackRange(target)) {
            return true;
        }
    }

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy) && AutoAttack::InAutoAttackRange(enemy)) {
            return true;
        }
    }
    return false;
}

static int QAttackWaitWindowMs() {
    const int qCastTime = static_cast<int>(Q.Delay * 1000.0f);
    const int latency = std::min(50, std::max(0, Game::Ping() / 2));
    return std::clamp(
        qCastTime + latency,
        MinimumQAttackWaitMs,
        MaximumQAttackWaitMs);
}

static bool ShouldWaitForAttackBeforeQ() {
    if (!HasAttackableComboTarget()) {
        return false;
    }
    if (Orbwalker::IsWindingUp() || Orbwalker::CanAttack()) {
        return true;
    }

    const int cooldown = Orbwalker::AttackCooldownRemaining();
    return cooldown > 0 && cooldown <= QAttackWaitWindowMs();
}

static void Combo() {
    if (!IsComboMode()) {
        return;
    }

    // Preserve AA -> Q whenever the next attack is ready or only a short wait away.
    if (Bool(ComboMenu, "UseQ", true) && Q.IsReady() &&
        ShouldWaitForAttackBeforeQ()) {
        return;
    }

    if (Bool(ComboMenu, "UseW", true) && W.IsReady() && !Orbwalker::IsWindingUp() && CastW()) {
        return;
    }

    if (!Bool(ComboMenu, "UseQ", true) || !Q.IsReady() || Orbwalker::IsWindingUp()) {
        return;
    }

    (void)CastComboQ();
}

static void OnAfterAttack(OrbwalkingActionArgs& args) {
    const auto player = Player();
    if (!Loaded || !IsComboMode() || !Bool(ComboMenu, "UseQ", true) ||
        !Q.IsReady() || !player.IsValid() || player.IsDead() ||
        player.IsRecalling() || Game::IsChatOpen()) {
        return;
    }

    const AIBaseClient attacked(args.Target.Handle());
    if (!attacked.IsValid() || !attacked.IsHero()) {
        return;
    }
    const AIHeroClient target(attacked.Handle());
    if (!ValidHeroTarget(target, ExtraQRange)) {
        return;
    }

    (void)CastComboQ(target);
}

static void Harass() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() <= Slider(HarassMenu, "Mana", 40) ||
        !Bool(HarassMenu, "UseQ", true) || !Q.IsReady()) {
        return;
    }

    if (!CastNormalQ() && Bool(HarassMenu, "UseExtendedQ", true)) {
        (void)CastExtendedDamageQ();
    }
}

static int CountMinionsOnQLine(const Vector3& lineEnd) {
    const auto player = Player();
    if (!player.IsValid()) {
        return 0;
    }

    int count = 0;
    const Vector3 start = player.Position();
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!ValidTarget(minion, ExtraQRange) || minion.IsJungle()) {
            continue;
        }
        if (PointSegmentDistance2D(minion.Position(), start, lineEnd) <=
            ExtraQDamageWidth + minion.BoundingRadius()) {
            ++count;
        }
    }
    return count;
}

static bool LaneClear() {
    const auto player = Player();
    if (!IsClearMode() || !player.IsValid() || !Bool(ClearMenu, "UseQ", true) ||
        !Q.IsReady() || Orbwalker::IsWindingUp() ||
        player.ManaPercent() <= Slider(ClearMenu, "Mana", 40)) {
        return false;
    }

    AIBaseClient bestBridge;
    int bestHitCount = 0;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!ValidTarget(minion, Q.Range) || minion.IsJungle()) {
            continue;
        }

        const Vector3 lineEnd = player.Position().Extend(minion.Position(), ExtraQRange);
        const int hitCount = CountMinionsOnQLine(lineEnd);
        if (hitCount > bestHitCount) {
            bestBridge = AIBaseClient(minion.Handle());
            bestHitCount = hitCount;
        }
    }

    return bestBridge.IsValid() &&
           bestHitCount >= Slider(ClearMenu, "QMinions", 2) &&
           Q.CastOnUnit(bestBridge);
}

static bool JungleClear() {
    const auto player = Player();
    if (!IsClearMode() || !player.IsValid() || Orbwalker::IsWindingUp() ||
        player.ManaPercent() <= Slider(ClearMenu, "Mana", 40)) {
        return false;
    }

    std::vector<AIMinionClient> monsters;
    for (const auto& monster : GameObjects::Jungle()) {
        if (ValidTarget(monster, W.Range)) {
            monsters.push_back(monster);
        }
    }
    std::sort(monsters.begin(), monsters.end(), [](const AIMinionClient& left, const AIMinionClient& right) {
        return left.MaxHealth() > right.MaxHealth();
    });

    if (Bool(JungleMenu, "UseW", true) && W.IsReady() && !monsters.empty() &&
        W.Cast(monsters.front().Position())) {
        return true;
    }

    if (!Bool(JungleMenu, "UseQ", true) || !Q.IsReady()) {
        return false;
    }

    AIBaseClient lowestHealth;
    float health = FLT_MAX;
    for (const auto& monster : monsters) {
        if (ValidTarget(monster, Q.Range) && monster.Health() < health) {
            lowestHealth = AIBaseClient(monster.Handle());
            health = monster.Health();
        }
    }
    return lowestHealth.IsValid() && Q.CastOnUnit(lowestHealth);
}

static bool AutoPickSoul() {
    if (!IsClearMode() || !Bool(ClearMenu, "AutoPickSouls", true) || !Orbwalker::CanAttack()) {
        return false;
    }

    for (const auto& soul : Souls) {
        if (ValidSoul(soul) && AutoAttack::InAutoAttackRange(soul.Unit) &&
            Orbwalker::Attack(soul.Unit)) {
            return true;
        }
    }
    return false;
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() || Game::IsChatOpen()) {
        return;
    }

    UpdateSpellData();
    PruneSouls();
    UpdatePassiveTarget();

    (void)HealAllyLogic();
    (void)AutoRLogic();

    const bool autoHarass = Key(HarassMenu, "AutoHarass");
    if (autoHarass && !IsComboMode()) {
        Harass();
    }

    if (IsComboMode()) {
        Combo();
    } else if (IsHarassMode() && !autoHarass) {
        Harass();
    } else if (IsClearMode()) {
        if (!AutoPickSoul() && !LaneClear()) {
            (void)JungleClear();
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    if (Bool(DrawMenu, "DrawQ", true) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFF5555u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawExtendedQ", true) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), ExtraQRange, 0xFFFFA500u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawW", true) && W.IsReady()) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFF777777u, 1.5f, 64);
    }

    if (!Bool(DrawMenu, "DrawSouls", true)) {
        return;
    }

    const int now = SDK::Variables::TickCount();
    for (const auto& soul : Souls) {
        if (!ValidSoul(soul)) {
            continue;
        }

        Vec2 screen = {};
        if (!Drawing::WorldToScreen(soul.Unit.Position(), screen) || !screen.IsValid()) {
            continue;
        }

        char remaining[24] = {};
        _snprintf_s(
            remaining,
            sizeof(remaining),
            _TRUNCATE,
            "%.1fs",
            static_cast<float>(std::max(0, soul.ExpireTick - now)) / 1000.0f);
        Drawing::DrawText(screen.x, screen.y, 0xFF7CFC00u, remaining);
    }
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.kuroaio.senna", "Kuro - Senna", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo Settings"));
    ComboMenu->Add(new MenuBool("UseQ", "Use Q", true));
    ComboMenu->Add(new MenuBool("UseExtendedQ", "Use Extended Q", true));
    ComboMenu->Add(new MenuBool("UseW", "Use W", true));
    ComboMenu->Add(new MenuList(
        "TargetMode",
        "Orbwalker Target Priority",
        { "Passive Mark", "Default" },
        0))->Permashow();

    RMenu = MenuRoot->AddSubMenu(new Menu("R", "R Settings"));
    RMenu->Add(new MenuList(
        "RMode",
        "Use R",
        { "Always", "Combo Only", "Disabled" },
        0))->Permashow();
    RMenu->Add(new MenuBool("RHealAlly", "Use R to Save Low Health Allies", true));
    RMenu->Add(new MenuSlider("RHealHealth", "Ally Health <= %", 15, 0, 100));
    RMenu->Add(new MenuSlider("RHealEnemies", "Nearby Enemies >=", 1, 0, 5));
    RMenu->Add(new MenuBool("RHealthPrediction", "Use Health Prediction", true));

    RBlacklistMenu = RMenu->AddSubMenu(new Menu("RBlacklist", "R Ally Blacklist"));
    for (const auto& ally : GameObjects::AllyHeroes()) {
        const std::string champion = ally.CharacterName().empty()
            ? std::to_string(ally.NetworkId())
            : ally.CharacterName();
        const std::string key = RBlockKey(ally);
        const std::string label = "Block R on " + champion;
        RBlacklistMenu->Add(new MenuBool(key.c_str(), label.c_str(), false));
    }

    RMenu->Add(new MenuBool("RExecute", "Use R on Killable Enemies", true));
    RMenu->Add(new MenuSlider("RPlayerEnemies", "Cast if Nearby Enemies <=", 1, 0, 5));

    HealMenu = MenuRoot->AddSubMenu(new Menu("Heal", "Q Heal Settings"));
    HealMenu->Add(new MenuList(
        "QHealMode",
        "Use Q Heal",
        { "Always", "Combo Only", "Disabled" },
        0))->Permashow();
    HealMenu->Add(new MenuSlider("QHealHealth", "Ally Health <= %", 15, 0, 100));
    HealMenu->Add(new MenuKeyBind(
        "FastHeal",
        "Fast Heal Lowest Health Ally",
        SDK::Keys::A,
        KeyBindType::Press))->Permashow();

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass Settings"));
    HarassMenu->Add(new MenuBool("UseQ", "Use Q", true));
    HarassMenu->Add(new MenuBool("UseExtendedQ", "Use Extended Q", true));
    HarassMenu->Add(new MenuSlider("Mana", "Minimum Mana %", 40, 0, 100));
    HarassMenu->Add(new MenuKeyBind(
        "AutoHarass",
        "Auto Harass",
        SDK::Keys::T,
        KeyBindType::Toggle))->Permashow();

    ClearMenu = MenuRoot->AddSubMenu(new Menu("Clear", "Lane Clear Settings"));
    ClearMenu->Add(new MenuBool("UseQ", "Use Q", true));
    ClearMenu->Add(new MenuSlider("QMinions", "Minimum Q Minions", 2, 1, 6));
    ClearMenu->Add(new MenuSlider("Mana", "Minimum Mana %", 40, 0, 100));
    ClearMenu->Add(new MenuBool("AutoPickSouls", "Auto Pick Souls", true));

    JungleMenu = MenuRoot->AddSubMenu(new Menu("Jungle", "Jungle Clear Settings"));
    JungleMenu->Add(new MenuBool("UseQ", "Use Q", true));
    JungleMenu->Add(new MenuBool("UseW", "Use W", true));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Draw Settings"));
    DrawMenu->Add(new MenuBool("DrawQ", "Draw Q Range", true));
    DrawMenu->Add(new MenuBool("DrawExtendedQ", "Draw Extended Q Range", true));
    DrawMenu->Add(new MenuBool("DrawW", "Draw W Range", true));
    DrawMenu->Add(new MenuBool("DrawSouls", "Draw Soul Timers", true));

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) {
        return;
    }

    if (auto* item = ComboMenu ? ComboMenu->Get<MenuList>("TargetMode") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = RMenu ? RMenu->Get<MenuList>("RMode") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = HealMenu ? HealMenu->Get<MenuList>("QHealMode") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = HealMenu ? HealMenu->Get<MenuKeyBind>("FastHeal") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = HarassMenu ? HarassMenu->Get<MenuKeyBind>("AutoHarass") : nullptr) {
        item->RemovePermashow();
    }

    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    ComboMenu = nullptr;
    RMenu = nullptr;
    RBlacklistMenu = nullptr;
    HealMenu = nullptr;
    HarassMenu = nullptr;
    ClearMenu = nullptr;
    JungleMenu = nullptr;
    DrawMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 600.0f + player.BoundingRadius());
    Q.SetTargetted(0.325f, FLT_MAX);

    ExtendedQ = Spell(SpellSlot::Q, ExtraQRange);
    ExtendedQ.SetSkillshot(
        0.325f,
        ExtraQDamageWidth,
        FLT_MAX,
        false,
        SkillshotType::SkillshotLine);

    W = Spell(SpellSlot::W, 1200.0f);
    W.SetSkillshot(0.25f, 70.0f, 1200.0f, true, SkillshotType::SkillshotLine);

    E = Spell(SpellSlot::E, 400.0f);

    R = Spell(SpellSlot::R, 25000.0f);
    R.SetSkillshot(1.0f, 160.0f, 20000.0f, false, SkillshotType::SkillshotLine);

    ForcedPassiveTargetNetworkId = 0;
    BuildMenu();
    InitializeSouls();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Orbwalker::OnAfterAttack += &OnAfterAttack;
    Drawing::OnDraw += &OnDraw;
    GameObjects::AddOnCreate(&OnObjectCreate);
    GameObjects::AddOnDelete(&OnObjectDelete);

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>Kuro - Senna loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;
    Drawing::OnDraw -= &OnDraw;
    GameObjects::RemoveOnCreate(&OnObjectCreate);
    GameObjects::RemoveOnDelete(&OnObjectDelete);

    if (ForcedPassiveTargetNetworkId != 0) {
        Orbwalker::ForceTarget(AttackableUnit());
    }
    ForcedPassiveTargetNetworkId = 0;
    Souls.clear();
    RemoveMenu();
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Senna
