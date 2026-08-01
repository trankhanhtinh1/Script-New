#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::Lucian {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneMenu = nullptr;
inline Menu* JungleMenu = nullptr;
inline Menu* KillstealMenu = nullptr;
inline Menu* AntiGapMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 565.0f };
inline Spell ExtendedQ{ SpellSlot::Q, 900.0f };
inline Spell W{ SpellSlot::W, 900.0f };
inline Spell WNoCollision{ SpellSlot::W, 900.0f };
inline Spell E{ SpellSlot::E, 460.0f };
inline Spell R{ SpellSlot::R, 1400.0f };

inline bool Loaded = false;
inline bool HasPassive = false;
inline bool ControllingR = false;
inline int LastActionTick = 0;

static bool HitchanceAtLeast(HitChance actual, HitChance required) {
    return static_cast<int>(actual) >= static_cast<int>(required);
}

static bool ManaOkay(Menu* menu, int fallback) {
    const auto player = Player();
    return player.IsValid() &&
           player.ManaPercent() >= static_cast<float>(Slider(menu, "Mana", fallback));
}

static float EffectivePhysicalHealth(const AIBaseClient& target) {
    return target.Health() + target.PhysicalShield();
}

static float EffectiveMagicalHealth(const AIBaseClient& target) {
    return target.Health() + target.MagicalShield();
}

static int CountEnemyHeroesNear(const Vector3& position, float range) {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy) &&
            enemy.Position().DistanceSqr2D(position) <= rangeSqr) {
            ++count;
        }
    }
    return count;
}

static float NearestEnemyDistance(const Vector3& position) {
    float best = 2000.0f;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy)) {
            best = std::min(best, enemy.Position().Distance2D(position));
        }
    }
    return best;
}

static bool UnderEnemyTurret(const Vector3& position) {
    const auto player = Player();
    const float playerRadius = player.IsValid() ? player.BoundingRadius() : 65.0f;
    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*for (const auto& turret : GameObjects::EnemyTurrets()) {
        if (!turret.IsValid() || turret.IsDead()) {
            continue;
        }
        const float range = turret.AttackRange() + turret.BoundingRadius() + playerRadius;
        if (turret.Position().DistanceSqr2D(position) <= range * range) {
            return true;
        }
    }*/
    return false;
}

static bool SafeDashPosition(const Vector3& position) {
    const auto player = Player();
    if (!player.IsValid() || position.IsZero() ||
        player.Position().Distance2D(position) > E.Range + 10.0f ||
        NavMesh::IsWall(position) ||
        NavMesh::IsWallBetween(player.Position(), position, 45.0f)) {
        return false;
    }

    if (!player.IsUnderEnemyTurret() && UnderEnemyTurret(position)) {
        return false;
    }

    const int currentEnemies = CountEnemyHeroesNear(player.Position(), 600.0f);
    return CountEnemyHeroesNear(position, 600.0f) <= std::max(1, currentEnemies);
}

static Vector3 BestDashPosition(const Vector3& desiredPosition, float distance = -1.0f) {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }

    const float dashDistance = std::clamp(
        distance > 0.0f ? distance : E.Range,
        30.0f,
        E.Range);
    const Vector3 origin = player.Position();
    Vector3 direction = desiredPosition - origin;
    const float directionLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    if (directionLength <= 0.001f) {
        return {};
    }

    const float baseAngle = std::atan2(direction.z, direction.x);
    constexpr float pi = 3.14159265358979323846f;
    Vector3 best = {};
    float bestScore = -FLT_MAX;

    for (int step = 0; step < 24; ++step) {
        const float angle = baseAngle + static_cast<float>(step) * (2.0f * pi / 24.0f);
        Vector3 candidate{
            origin.x + std::cos(angle) * dashDistance,
            origin.y,
            origin.z + std::sin(angle) * dashDistance
        };
        candidate.y = NavMesh::GetHeightForPosition(candidate);
        if (!SafeDashPosition(candidate)) {
            continue;
        }

        const float directionPenalty = candidate.Distance2D(desiredPosition);
        const float safety = NearestEnemyDistance(candidate);
        const float enemyPenalty = static_cast<float>(CountEnemyHeroesNear(candidate, 600.0f)) * 500.0f;
        const float score = safety * 0.55f - directionPenalty - enemyPenalty;
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    return best;
}

// E luôn lao theo hướng con trỏ. Nếu điểm đích nằm trong tường thì rút ngắn dần
// thay vì bỏ cast — E vẫn phải nổ để reset đòn đánh.
static Vector3 CursorDashPosition(float distance = -1.0f) {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }

    const Vector3 origin = player.Position();
    const Vector3 cursor = Game::CursorPos();
    if (origin.Distance2D(cursor) <= 1.0f) {
        return {};
    }

    const float requested = std::clamp(
        distance > 0.0f ? distance : E.Range, 30.0f, E.Range);
    for (float step = requested; step >= 30.0f; step -= 40.0f) {
        Vector3 candidate = origin.Extend(cursor, step);
        candidate.y = NavMesh::GetHeightForPosition(candidate);
        if (!NavMesh::IsWall(candidate)) {
            return candidate;
        }
    }

    Vector3 fallback = origin.Extend(cursor, 30.0f);
    fallback.y = NavMesh::GetHeightForPosition(fallback);
    return fallback;
}

// "Maximum" = luôn full tầm E, mặc định dừng đúng ở vị trí chuột.
static float ComboDashDistance() {
    const auto player = Player();
    if (!player.IsValid() || List(ComboMenu, "EDistance", 0) == 1) {
        return E.Range;
    }
    return player.Position().Distance2D(Game::CursorPos());
}

static bool CastE(const Vector3& position) {
    return E.IsReady() && !position.IsZero() && E.Cast(position);
}

// Anti-gapcloser vẫn phải lọc vị trí an toàn trước khi lao.
static bool CastSafeE(const Vector3& position) {
    return E.IsReady() && !position.IsZero() && SafeDashPosition(position) &&
           E.Cast(position);
}

static void UpdateQData() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    Q.Range = 500.0f + player.BoundingRadius();
    const int level = std::clamp(player.Level(), 1, 18);
    float delay = 0.40f;
    if (level <= 5) {
        delay = 0.40f - static_cast<float>(level) * 0.01f;
    } else if (level == 6) {
        delay = 0.36f;
    } else if (level <= 13) {
        delay = 0.42f - static_cast<float>(level) * 0.01f;
    } else if (level == 14) {
        delay = 0.29f;
    } else {
        delay = 0.43f - static_cast<float>(std::min(level, 17)) * 0.01f;
    }
    Q.Delay = delay;
    ExtendedQ.Delay = delay;
}

static bool IsRChanneling() {
    const auto player = Player();
    return player.IsValid() && player.HasBuff("LucianR");
}

static bool UpdateRControl() {
    const auto player = Player();
    const bool channeling = player.IsValid() && !player.IsDead() && IsRChanneling();
    if (channeling) {
        ControllingR = true;
        Orbwalker::AttackEnabled(false);
        Orbwalker::MoveEnabled(false);
        SDK::IssueOrder(player, SDK::GameObjectOrder::MoveTo, Game::CursorPos());
        return true;
    }

    if (ControllingR) {
        Orbwalker::AttackEnabled(true);
        Orbwalker::MoveEnabled(true);
        ControllingR = false;
    }
    return false;
}

static std::vector<AIBaseClient> QBridgeUnits() {
    std::vector<AIBaseClient> result;
    for (const auto& minion : GameObjects::EnemyLaneMinions()) {
        if (ValidTarget(minion, Q.Range)) {
            result.emplace_back(minion.Handle());
        }
    }
    for (const auto& mob : GameObjects::Jungle()) {
        if (ValidTarget(mob, Q.Range) && !mob.IsPlant() && !mob.IsPet()) {
            result.emplace_back(mob.Handle());
        }
    }
    return result;
}

static bool CastDirectQ(const AIBaseClient& target) {
    if (!Q.IsReady() || !ValidTarget(target, Q.Range)) {
        return false;
    }
    if (target.IsHero()) {
        const auto player = Player();
        const auto prediction = Q.GetPrediction(target);
        Vector3 castPosition = prediction.GetCastPosition();
        if (castPosition.IsZero()) {
            castPosition = target.Position();
        }
        if (!player.IsValid() || SDK::Collision::HasProjectileWallCollision(
                player.Position(), castPosition, 30.0f)) {
            return false;
        }
    }
    return Q.Cast(target) == CastStates::SuccessfullyCasted;
}

static bool CastExtendedQThroughUnit(const AIHeroClient& target) {
    const auto player = Player();
    if (!Q.IsReady() || !player.IsValid() ||
        !ValidHeroTarget(target, ExtendedQ.Range)) {
        return false;
    }

    const auto prediction = ExtendedQ.GetPrediction(target);
    if (!HitchanceAtLeast(prediction.Hitchance, HitChance::High)) {
        return false;
    }

    const Vector3 start = player.Position();
    const Vector3 targetPosition = prediction.GetUnitPosition();
    if (targetPosition.IsZero() ||
        SDK::Collision::HasProjectileWallCollision(
            start, targetPosition, ExtendedQ.Width * 0.5f)) {
        return false;
    }
    for (const auto& unit : QBridgeUnits()) {
        const Vector3 end = start.Extend(unit.Position(), ExtendedQ.Range);
        SDK::RectanglePoly rectangle(start, end, ExtendedQ.Width);
        if (rectangle.IsInside(targetPosition) &&
            Q.Cast(unit) == CastStates::SuccessfullyCasted) {
            return true;
        }
    }
    return false;
}

static bool ExtendedQLogic(const AIHeroClient& requestedTarget = AIHeroClient()) {
    AIHeroClient target = requestedTarget;
    if (!ValidHeroTarget(target, ExtendedQ.Range)) {
        target = GetPhysicalTarget(ExtendedQ.Range);
    }
    return ValidHeroTarget(target, ExtendedQ.Range) &&
           CastExtendedQThroughUnit(target);
}

static bool CastW(const AIHeroClient& target,
                  bool ignoreCollision,
                  HitChance required = HitChance::Medium) {
    Spell& spell = ignoreCollision ? WNoCollision : W;
    if (!spell.IsReady() || !ValidHeroTarget(target, spell.Range)) {
        return false;
    }
    const auto prediction = spell.GetPrediction(target, true);
    const auto player = Player();
    const Vector3 castPosition = prediction.GetCastPosition();
    if (!player.IsValid() || castPosition.IsZero() ||
        SDK::Collision::HasProjectileWallCollision(
            player.Position(), castPosition, spell.Width * 0.5f) ||
        !HitchanceAtLeast(prediction.Hitchance, required)) {
        return false;
    }
    return spell.Cast(castPosition);
}


static float PassiveShotDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    float ratio = 1.0f;
    if (target.IsHero()) {
        ratio = player.Level() >= 13 ? 0.60f : (player.Level() >= 7 ? 0.55f : 0.50f);
    }
    return player.CalculatePhysicalDamage(target, player.AD() * ratio);
}

static bool Killsteal() {
    if (Bool(KillstealMenu, "UseQ", true) && Q.IsReady()) {
        for (const auto& enemy : EnemyHeroesByHealth(ExtendedQ.Range)) {
            const float predictedHealth = std::max(1.0f, Q.GetHealthPrediction(enemy));
            if (Q.GetDamage(enemy) < predictedHealth + enemy.PhysicalShield()) {
                continue;
            }
            if (ValidHeroTarget(enemy, Q.Range) &&
                Q.Cast(enemy) == CastStates::SuccessfullyCasted) {
                return true;
            }
            if (!Key(HarassMenu, "AutoExtendedQ") && CastExtendedQThroughUnit(enemy)) {
                return true;
            }
        }
    }

    if (Bool(KillstealMenu, "UseW", true) && W.IsReady()) {
        for (const auto& enemy : EnemyHeroesByHealth(W.Range)) {
            if (W.GetDamage(enemy) >= EffectiveMagicalHealth(enemy) &&
                CastW(enemy, false, HitChance::Medium)) {
                return true;
            }
        }
    }
    return false;
}

static void MarkPassiveSpellCast() {
    HasPassive = true;
    LastActionTick = SDK::Variables::TickCount();
}

// Mỗi đòn đánh xong phải nổ đúng một chiêu để nạp lại nội tại Lightslinger:
// E lao tới con trỏ trước (reset đòn đánh), không được thì tới Q rồi W.
static bool ComboAfterAttack(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return false;
    }

    if (Bool(ComboMenu, "UseE", true) && E.IsReady() && !player.IsDashing() &&
        CastE(CursorDashPosition(ComboDashDistance()))) {
        MarkPassiveSpellCast();
        return true;
    }

    if (!ValidHeroTarget(target, W.Range)) {
        return false;
    }

    if (Bool(ComboMenu, "UseQ", true) && Q.IsReady()) {
        if (CastDirectQ(target)) {
            MarkPassiveSpellCast();
            return true;
        }
        // Ngoài tầm Q thẳng thì xuyên qua lính/quái để vẫn nạp được nội tại.
        if (CastExtendedQThroughUnit(target)) {
            MarkPassiveSpellCast();
            return true;
        }
    }

    if (Bool(ComboMenu, "UseW", true) && W.IsReady()) {
        // W nổ theo vùng quanh điểm chạm nên trong tầm đánh thường có thể bỏ
        // qua va chạm lính mà vẫn đánh dấu được mục tiêu.
        const bool ignoreCollision = target.DistanceToPlayer() <= 500.0f;
        if (CastW(target, ignoreCollision, HitChance::Low)) {
            MarkPassiveSpellCast();
            return true;
        }
    }
    return false;
}

static bool Harass() {
    if (!ManaOkay(HarassMenu, 40)) {
        return false;
    }

    auto target = GetPhysicalTarget(W.Range);
    if (Bool(HarassMenu, "UseW", false) && W.IsReady() &&
        ValidHeroTarget(target, W.Range) &&
        (!AutoAttack::InAutoAttackRange(target) || !HasPassive) &&
        CastW(target, false, HitChance::High)) {
        return true;
    }

    if (Bool(HarassMenu, "UseE", false) && E.IsReady() && !HasPassive &&
        CastE(CursorDashPosition(75.0f))) {
        return true;
    }

    if (Bool(HarassMenu, "UseQ", true) && Q.IsReady()) {
        target = GetPhysicalTarget(Q.Range);
        if (CastDirectQ(target)) {
            return true;
        }
        if (Bool(HarassMenu, "UseExtendedQ", true) &&
            !Key(HarassMenu, "AutoExtendedQ") && ExtendedQLogic()) {
            return true;
        }
    }
    return false;
}

static bool LaneClear() {
    if (!ManaOkay(LaneMenu, 40)) {
        return false;
    }

    if (Bool(LaneMenu, "UseQ", true) && Q.IsReady()) {
        const auto player = Player();
        const auto minions = GameObjects::EnemyLaneMinions();
        AIBaseClient bestBridge;
        int bestHits = 2;
        for (const auto& bridge : minions) {
            if (!ValidTarget(bridge, Q.Range)) {
                continue;
            }
            const Vector3 end = player.Position().Extend(
                bridge.Position(), ExtendedQ.Range);
            SDK::RectanglePoly rectangle(player.Position(), end, ExtendedQ.Width);
            int hits = 0;
            for (const auto& minion : minions) {
                if (ValidTarget(minion, ExtendedQ.Range) &&
                    rectangle.IsInside(minion.Position())) {
                    ++hits;
                }
            }
            if (hits > bestHits) {
                bestHits = hits;
                bestBridge = AIBaseClient(bridge.Handle());
            }
        }
        if (bestBridge.IsValid() &&
            Q.Cast(bestBridge) == CastStates::SuccessfullyCasted) {
            return true;
        }
    }

    if (Bool(LaneMenu, "UseW", true) && W.IsReady()) {
        std::vector<AIBaseClient> minions;
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (ValidTarget(minion, W.Range)) {
                minions.emplace_back(minion.Handle());
            }
        }
        const auto farm = W.GetCircularFarmLocation(minions, 150.0f);
        if (farm.MinionsHit > 3 && W.Cast(farm.Position)) {
            return true;
        }
    }
    return false;
}

static bool JungleClear() {
    if (!ManaOkay(LaneMenu, 40)) {
        return false;
    }

    std::vector<AIMinionClient> monsters;
    for (const auto& mob : GameObjects::Jungle()) {
        if (ValidTarget(mob, W.Range) && !mob.IsPlant() && !mob.IsPet()) {
            monsters.push_back(mob);
        }
    }
    if (monsters.empty()) {
        return false;
    }

    if (Bool(JungleMenu, "UseW", true) && W.IsReady() && !HasPassive) {
        std::vector<AIBaseClient> units;
        for (const auto& mob : monsters) {
            units.emplace_back(mob.Handle());
        }
        const auto farm = W.GetCircularFarmLocation(units, 150.0f);
        if (farm.MinionsHit >= 1 && W.Cast(farm.Position)) {
            return true;
        }
    }

    if (Bool(JungleMenu, "UseE", true) && E.IsReady()) {
        const AttackableUnit lastTarget = Orbwalker::LastTarget();
        if (lastTarget.IsValid() && lastTarget.IsMinion()) {
            const AIMinionClient mob(lastTarget.Handle());
            if (mob.IsValid() && mob.IsJungle() &&
                mob.Health() > PassiveShotDamage(mob) +
                    Damage::GetAutoAttackDamage(Player(), mob)) {
                if (CastE(CursorDashPosition(75.0f))) {
                    return true;
                }
            }
        }
    }

    if (Bool(JungleMenu, "UseQ", true) && Q.IsReady() && !HasPassive) {
        std::sort(monsters.begin(), monsters.end(), [](const AIMinionClient& left,
                                                       const AIMinionClient& right) {
            return EffectivePhysicalHealth(left) < EffectivePhysicalHealth(right);
        });
        for (const auto& mob : monsters) {
            if (CastDirectQ(AIBaseClient(mob.Handle()))) {
                return true;
            }
        }
    }
    return false;
}

static bool SmartR() {
    if (!Key(ComboMenu, "SmartR") || !R.IsReady() || IsRChanneling()) {
        return false;
    }
    const auto target = GetPhysicalTarget(R.Range);
    const auto prediction = R.GetPrediction(target, true);
    Vector3 castPosition = prediction.GetCastPosition();
    if (castPosition.IsZero()) {
        castPosition = target.Position();
    }
    return ValidHeroTarget(target, R.Range) &&
           !SDK::Collision::HasProjectileWallCollision(
               Player().Position(), castPosition, R.Width * 0.5f) &&
           R.Cast(castPosition);
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    UpdateQData();
    if (UpdateRControl()) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() ||
        Game::IsChatOpen() || Orbwalker::IsWindingUp()) {
        return;
    }

    const int now = SDK::Variables::TickCount();
    if (now - LastActionTick < 40) {
        return;
    }

    const OrbwalkingMode mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::Combo && HasPassive) {
        return;
    }

    if (mode != OrbwalkingMode::None) {
        const float aaRange = AutoAttack::GetRealAutoAttackRange(player);
        const auto aaTarget = GetPhysicalTarget(aaRange);
        if (aaTarget.IsValid() && KuroAIO::CanAttack(250)) {
            return;
        }
    }

    if (SmartR() || Killsteal()) {
        LastActionTick = now;
        return;
    }

    if (Key(HarassMenu, "AutoExtendedQ") && mode != OrbwalkingMode::Combo &&
        ManaOkay(HarassMenu, 40) && Q.IsReady() && ExtendedQLogic()) {
        LastActionTick = now;
        return;
    }

    bool casted = false;
    switch (mode) {
    case OrbwalkingMode::Combo:
        break;
    case OrbwalkingMode::Harass:
        casted = Harass();
        break;
    case OrbwalkingMode::LaneClear:
        casted = LaneClear() || JungleClear();
        break;
    default:
        break;
    }
    if (casted) {
        LastActionTick = now;
    }
}

static void OnProcessSpell(const ProcessSpellEventArgs& args) {
    if (!Loaded || !Events::IsLocalPlayer(args.Sender)) {
        return;
    }

    if (args.IsAutoAttack || AutoAttack::IsAutoAttack(args.SpellName)) {
        HasPassive = false;
        return;
    }

    if (args.Slot == static_cast<int>(SpellSlot::Q) ||
        args.Slot == static_cast<int>(SpellSlot::W) ||
        args.Slot == static_cast<int>(SpellSlot::E)) {
        HasPassive = true;
    }
    if (args.Slot == static_cast<int>(SpellSlot::E)) {
        Orbwalker::ResetAutoAttackTimer();
    }
}

static void OnBuffRemove(const BuffEventArgs& args) {
    if (Loaded && Events::IsLocalPlayer(args.Sender) &&
        EqualsIgnoreCase(args.BuffName, "lucianpassivebuff")) {
        HasPassive = false;
    }
}

static void OnAfterAttack(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }

    HasPassive = false;
    const auto player = Player();
    if (Orbwalker::ActiveMode() != OrbwalkingMode::Combo ||
        !player.IsValid() || player.IsDead() || player.IsRecalling() ||
        Game::IsChatOpen() || IsRChanneling()) {
        return;
    }

    // Đánh vào lính giữa giao tranh vẫn phải weave chiêu, nên khi mục tiêu vừa
    // đánh không phải tướng thì lấy tạm mục tiêu vật lý gần nhất.
    const AIBaseClient attacked(args.Target.Handle());
    AIHeroClient target = attacked.IsValid() && attacked.IsHero()
        ? AIHeroClient(attacked.Handle())
        : GetPhysicalTarget(W.Range);
    if (!ValidHeroTarget(target, W.Range)) {
        return;
    }
    (void)ComboAfterAttack(target);
}

static void OnNonKillableMinion(OrbwalkingActionArgs& args) {
    if (!Loaded || Orbwalker::ActiveMode() != OrbwalkingMode::LaneClear ||
        !Bool(LaneMenu, "UseE", false) || !E.IsReady() ||
        !ManaOkay(LaneMenu, 40)) {
        return;
    }

    const AIBaseClient target(args.Target.Handle());
    if (!ValidTarget(target, AutoAttack::GetRealAutoAttackRange(Player(), target))) {
        return;
    }
    const float twoShotDamage = PassiveShotDamage(target) +
                                Damage::GetAutoAttackDamage(Player(), target);
    if (target.Health() <= twoShotDamage) {
        (void)CastE(CursorDashPosition(30.0f));
    }
}

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    if (!Loaded || !Bool(AntiGapMenu, "UseE", true) || !E.IsReady()) {
        return;
    }
    const auto player = Player();
    const AIHeroClient sender(args.Sender);
    if (!player.IsValid() || !ValidHeroTarget(sender) ||
        args.End.Distance2D(player.Position()) > 240.0f) {
        return;
    }

    const Vector3 away = player.Position().Extend(sender.Position(), -E.Range);
    const Vector3 dash = BestDashPosition(away, E.Range);
    (void)CastSafeE(dash);
}

static void OnDraw() {
    const auto player = Player();
    if (!Loaded || !player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "ExtendedQ", true)) {
        Drawing::DrawCircle(player.Position(), ExtendedQ.Range, 0xFFFFA500u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "Q", false)) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFF00D8FFu, 1.5f, 64);
    }
    if (Bool(DrawMenu, "W", false)) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFFFF5555u, 1.5f, 64);
    }
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.kuroaio.lucian", "Kuro - Lucian", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo Settings"));
    ComboMenu->Add(new MenuBool("UseQ", "Use Q", true));
    ComboMenu->Add(new MenuBool("UseW", "Use W", true));
    ComboMenu->Add(new MenuBool("UseE", "Use E", true));
    ComboMenu->Add(new MenuList(
        "EDistance", "E dash distance", { "To cursor", "Maximum" }, 0));
    ComboMenu->Add(new MenuKeyBind(
        "SmartR", "Smart R", SDK::Keys::T, KeyBindType::Press))->Permashow();

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass Settings"));
    HarassMenu->Add(new MenuBool("UseQ", "Use Q", true));
    HarassMenu->Add(new MenuBool("UseExtendedQ", "Use Extended Q", true));
    HarassMenu->Add(new MenuBool("UseW", "Use W", false));
    HarassMenu->Add(new MenuBool("UseE", "Use E", false));
    HarassMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 40, 0, 100));
    HarassMenu->Add(new MenuKeyBind(
        "AutoExtendedQ", "Auto Extended Q", SDK::Keys::G, KeyBindType::Toggle))->Permashow();

    LaneMenu = MenuRoot->AddSubMenu(new Menu("LaneClear", "Lane Clear Settings"));
    LaneMenu->Add(new MenuBool("UseQ", "Use Q", true));
    LaneMenu->Add(new MenuBool("UseW", "Use W", true));
    LaneMenu->Add(new MenuBool("UseE", "Use E reset for missed minions", false));
    LaneMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 40, 0, 100));

    JungleMenu = MenuRoot->AddSubMenu(new Menu("JungleClear", "Jungle Clear Settings"));
    JungleMenu->Add(new MenuBool("UseQ", "Use Q", true));
    JungleMenu->Add(new MenuBool("UseW", "Use W", true));
    JungleMenu->Add(new MenuBool("UseE", "Use E", true));

    KillstealMenu = MenuRoot->AddSubMenu(new Menu("Killsteal", "Killsteal Settings"));
    KillstealMenu->Add(new MenuBool("UseQ", "Use Q", true));
    KillstealMenu->Add(new MenuBool("UseW", "Use W", true));

    AntiGapMenu = MenuRoot->AddSubMenu(new Menu("AntiGapcloser", "Anti-Gapcloser Settings"));
    AntiGapMenu->Add(new MenuBool("UseE", "Use E", true));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Draw Settings"));
    DrawMenu->Add(new MenuBool("ExtendedQ", "Draw Extended Q range", true));
    DrawMenu->Add(new MenuBool("Q", "Draw Q range", false));
    DrawMenu->Add(new MenuBool("W", "Draw W range", false));

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) {
        return;
    }
    if (auto* item = ComboMenu ? ComboMenu->Get<MenuKeyBind>("SmartR") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = HarassMenu ? HarassMenu->Get<MenuKeyBind>("AutoExtendedQ") : nullptr) {
        item->RemovePermashow();
    }
    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
    ComboMenu = nullptr;
    HarassMenu = nullptr;
    LaneMenu = nullptr;
    JungleMenu = nullptr;
    KillstealMenu = nullptr;
    AntiGapMenu = nullptr;
    DrawMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 500.0f + player.BoundingRadius());
    ExtendedQ = Spell(SpellSlot::Q, 900.0f);
    W = Spell(SpellSlot::W, 900.0f);
    WNoCollision = Spell(SpellSlot::W, 900.0f);
    E = Spell(SpellSlot::E, 460.0f);
    R = Spell(SpellSlot::R, 1400.0f);

    Q.SetTargetted(0.40f, 1400.0f);
    Q.DamageType = DamageType::Physical;
    ExtendedQ.SetSkillshot(0.40f, 60.0f, FLT_MAX, false, SkillshotType::SkillshotLine);
    ExtendedQ.DamageType = DamageType::Physical;
    W.SetSkillshot(0.25f, 80.0f, 1600.0f, true, SkillshotType::SkillshotLine);
    W.SetCollisionObjects({
        SDK::CollisionableObjects::Minions,
        SDK::CollisionableObjects::Heroes,
        SDK::CollisionableObjects::YasuoWall,
        SDK::CollisionableObjects::SamiraWall,
        SDK::CollisionableObjects::MelWall
    });
    W.DamageType = DamageType::Magical;
    WNoCollision.SetSkillshot(0.25f, 80.0f, 1600.0f, false, SkillshotType::SkillshotLine);
    WNoCollision.DamageType = DamageType::Magical;
    R.SetSkillshot(0.0f, 110.0f, 2500.0f, true, SkillshotType::SkillshotLine);

    HasPassive = player.HasBuff("lucianpassivebuff");
    ControllingR = false;
    LastActionTick = 0;
    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Events::hook.OnBuffRemove += &OnBuffRemove;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;
    Orbwalker::OnAfterAttack += &OnAfterAttack;
    Orbwalker::OnNonKillableMinion += &OnNonKillableMinion;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#00D8FF' size='20'>Kuro - Lucian loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Events::hook.OnBuffRemove -= &OnBuffRemove;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;
    Orbwalker::OnNonKillableMinion -= &OnNonKillableMinion;
    Drawing::OnDraw -= &OnDraw;

    if (ControllingR) {
        Orbwalker::AttackEnabled(true);
        Orbwalker::MoveEnabled(true);
    }
    RemoveMenu();
    HasPassive = false;
    ControllingR = false;
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Lucian
