#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::Syndra {

struct SphereInfo {
    AIBaseClient Unit;
    int ExpireTick = 0;
};

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* RBlacklistMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* AAMenu = nullptr;
inline Menu* LaneMenu = nullptr;
inline Menu* JungleMenu = nullptr;
inline Menu* KillstealMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 800.0f };
inline Spell W{ SpellSlot::W, 925.0f };
inline Spell E{ SpellSlot::E, 700.0f };
inline Spell R{ SpellSlot::R, 675.0f };
inline Spell QE{ SpellSlot::E, 1100.0f };

inline std::vector<SphereInfo> Spheres;
inline bool ArmQE = false;
inline bool PendingE = false;
inline Vector3 PendingEPosition = {};
inline int PendingETick = 0;
inline int LastWPickupTick = 0;
inline int LastWBlockETick = 0;
inline int LastSphereETick = 0;
inline int NextWPickupTick = 0;
inline bool Loaded = false;

static constexpr int kSphereLifetimeMs = 6000;
static constexpr float kSpherePushRange = 1100.0f;

static std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

static bool ContainsIgnoreCase(const std::string& value, const char* needle) {
    return needle && needle[0] && Lower(value).find(Lower(needle)) != std::string::npos;
}

static bool SameUnit(const GameObject& left, const GameObject& right) {
    if (!left.IsValid() || !right.IsValid()) {
        return false;
    }
    return left.Address() == right.Address() ||
           (left.NetworkId() != 0 && left.NetworkId() == right.NetworkId());
}

static bool IsSphereObject(const GameObject& object) {
    if (!object.IsValid() || !object.IsMinion()) {
        return false;
    }
    const auto player = Player();
    const AIMinionClient sphere(object.Handle());
    if (!player.IsValid() || !sphere.IsValid() || sphere.Team() != player.Team() ||
        std::abs(sphere.MaxHealth() - 1.0f) > 0.1f) {
        return false;
    }
    const std::string name = GetObjectName(object);
    const std::string characterName = GetObjectCharacterName(object);
    return EqualsIgnoreCase(name.c_str(), "Seed") ||
           EqualsIgnoreCase(characterName.c_str(), "Seed") ||
           ContainsIgnoreCase(name, "SyndraSphere") ||
           ContainsIgnoreCase(characterName, "SyndraSphere");
}

static void TrackSphere(const GameObject& object) {
    if (!IsSphereObject(object)) {
        return;
    }
    const AIBaseClient sphere(
        object.Address(),
        ::Core::Objects::ObjectType::AIMinionClient);
    for (auto& entry : Spheres) {
        if (SameUnit(entry.Unit, sphere)) {
            entry.ExpireTick = SDK::Variables::TickCount() + kSphereLifetimeMs;
            return;
        }
    }
    Spheres.push_back({ sphere, SDK::Variables::TickCount() + kSphereLifetimeMs });
}

static bool ValidSphere(const SphereInfo& sphere) {
    return sphere.ExpireTick > SDK::Variables::TickCount() && sphere.Unit.IsValid() &&
           !sphere.Unit.IsDead() && sphere.Unit.Health() > 0.0f;
}

static void PruneSpheres() {
    Spheres.erase(
        std::remove_if(Spheres.begin(), Spheres.end(), [](const SphereInfo& sphere) {
            return !ValidSphere(sphere);
        }),
        Spheres.end());
}

static void OnObjectCreate(const GameObject& object) {
    TrackSphere(object);
}

static void OnObjectDelete(const GameObject& object) {
    Spheres.erase(
        std::remove_if(Spheres.begin(), Spheres.end(), [&](const SphereInfo& sphere) {
            return SameUnit(sphere.Unit, object);
        }),
        Spheres.end());
}

static void InitializeSpheres() {
    Spheres.clear();
    for (const auto& object : GameObjects::AllGameObjects()) {
        TrackSphere(object);
    }
}

static bool HoldingWObject() {
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }
    if (player.HasBuff("syndrawtooltip") || player.HasBuff("SyndraWTooltip")) {
        return true;
    }
    return ContainsIgnoreCase(W.Instance().Name(), "syndrawcast");
}

static float EffectiveMagicalHealth(const AIBaseClient& target) {
    return target.Health() + target.AllShield() + target.MagicalShield();
}

static std::string RBlacklistKey(const AIHeroClient& enemy) {
    return "BlockR." + std::to_string(enemy.NetworkId());
}

static bool RBlocked(const AIHeroClient& enemy) {
    if (!RBlacklistMenu || !enemy.IsValid()) {
        return false;
    }
    const std::string key = RBlacklistKey(enemy);
    const auto* item = RBlacklistMenu->Get<MenuBool>(key.c_str());
    return item && item->Value;
}

static float PointSegmentDistance2D(const Vector3& point,
                                    const Vector3& start,
                                    const Vector3& end) {
    const float dx = end.x - start.x;
    const float dz = end.z - start.z;
    const float lengthSqr = dx * dx + dz * dz;
    if (lengthSqr <= FLT_EPSILON) {
        return point.Distance2D(start);
    }
    const float px = point.x - start.x;
    const float pz = point.z - start.z;
    const float projection = std::clamp((px * dx + pz * dz) / lengthSqr, 0.0f, 1.0f);
    const float offsetX = point.x - (start.x + projection * dx);
    const float offsetZ = point.z - (start.z + projection * dz);
    return std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
}

static bool IsSyndraQCast(const Events::ProcessSpellEventArgs& args) {
    return EqualsIgnoreCase(args.SpellName, "SyndraQSpell") ||
           EqualsIgnoreCase(args.ScriptName, "SyndraQSpell") ||
           EqualsIgnoreCase(args.SpellSlotName, "SyndraQ") ||
           EqualsIgnoreCase(args.PayloadSpellName, "SyndraQSpell") ||
           ContainsIgnoreCase(args.SpellName, "syndraq");
}

static void OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender) || !IsSyndraQCast(args)) {
        return;
    }
    if (!ArmQE || LastWPickupTick >= SDK::Variables::TickCount()) {
        ArmQE = false;
        return;
    }
    PendingEPosition = args.EndPosition;
    PendingETick = SDK::Variables::TickCount() + 100;
    PendingE = true;
    ArmQE = false;
}

static bool ExecutePendingE() {
    if (!PendingE || SDK::Variables::TickCount() < PendingETick) {
        return false;
    }
    PendingE = false;
    PendingETick = 0;
    if (!E.IsReady() || !PendingEPosition.IsValid() || PendingEPosition.IsZero()) {
        return false;
    }
    LastSphereETick = SDK::Variables::TickCount() + SDK::Game::Ping() + 100;
    return E.Cast(PendingEPosition);
}

static bool CastQ(const AIHeroClient& target, HitChance chance = HitChance::High) {
    if (!Q.IsReady() || !ValidHeroTarget(target, Q.Range)) {
        return false;
    }
    const auto prediction = Q.GetPrediction(target);
    return prediction.Hitchance >= chance && Q.Cast(prediction.GetCastPosition());
}

static bool CastQEAt(const Vector3& qPosition) {
    const auto player = Player();
    if (!player.IsValid() || !Q.IsReady() || !E.IsReady() ||
        !qPosition.IsValid() || qPosition.IsZero()) {
        return false;
    }
    Vector3 castPosition = qPosition;
    if (player.Position().Distance2D(castPosition) > Q.Range) {
        castPosition = player.Position().Extend(castPosition, Q.Range);
    }
    ArmQE = true;
    if (!Q.Cast(castPosition)) {
        ArmQE = false;
        return false;
    }
    return true;
}

static bool CastQE(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !ValidHeroTarget(target, QE.Range) || !Q.IsReady() || !E.IsReady()) {
        return false;
    }
    QE.Delay = E.Delay + Q.Range / E.Speed;
    const auto prediction = QE.GetPrediction(target);
    if (prediction.Hitchance < HitChance::High) {
        return false;
    }
    const float distance = player.Position().Distance2D(prediction.GetCastPosition());
    const float qDistance = distance <= E.Range
        ? std::min(distance, Q.Range)
        : Q.Range - 100.0f;
    return CastQEAt(player.Position().Extend(prediction.GetCastPosition(), qDistance));
}

static AIBaseClient BestWPickupObject() {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }
    for (const auto& sphere : Spheres) {
        if (ValidSphere(sphere) && sphere.Unit.DistanceToPlayer() <= W.Range) {
            return sphere.Unit;
        }
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, W.Range)) {
            return AIBaseClient(minion.Handle());
        }
    }
    for (const auto& monster : GameObjects::Jungle()) {
        if (ValidTarget(monster, W.Range)) {
            return AIBaseClient(monster.Handle());
        }
    }
    return {};
}

static bool PickupWObject() {
    if (!W.IsReady() || HoldingWObject() || SDK::Variables::TickCount() < NextWPickupTick) {
        return false;
    }
    const auto object = BestWPickupObject();
    if (!object.IsValid() || object.DistanceToPlayer() > W.Range) {
        return false;
    }
    if (!W.Cast(object.Position())) {
        return false;
    }
    const int now = SDK::Variables::TickCount();
    LastWPickupTick = now + SDK::Game::Ping() + 20;
    LastWBlockETick = now + SDK::Game::Ping() + 200;
    NextWPickupTick = now + 1000;
    return true;
}

static bool ThrowW(const AIHeroClient& target) {
    if (!W.IsReady() || !HoldingWObject() || !ValidHeroTarget(target, W.Range) ||
        target.HasBuff("SyndraEDebuff") || SDK::Variables::TickCount() < LastSphereETick) {
        return false;
    }
    const auto prediction = W.GetPrediction(target, true);
    if (prediction.Hitchance < HitChance::High ||
        Collisions::HasYasuoWindWallCollision(Player().ServerPosition(), prediction.GetCastPosition())) {
        return false;
    }
    return W.Cast(prediction.GetCastPosition());
}

static bool WLogic(const AIHeroClient& target) {
    if (!W.IsReady() || !target.IsValid()) {
        return false;
    }
    return HoldingWObject() ? ThrowW(target) : PickupWObject();
}

static bool PushSphereAt(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !E.IsReady() || !ValidHeroTarget(target, QE.Range) ||
        SDK::Variables::TickCount() < LastWBlockETick) {
        return false;
    }
    const auto prediction = QE.GetPrediction(target);
    if (prediction.Hitchance < HitChance::High) {
        return false;
    }
    const Vector3 targetPosition = prediction.GetCastPosition();
    const float targetDistance = player.Position().Distance2D(targetPosition);

    for (const auto& sphere : Spheres) {
        if (!ValidSphere(sphere)) {
            continue;
        }
        const float sphereDistance = sphere.Unit.DistanceToPlayer();
        if (sphereDistance < 100.0f || sphereDistance > E.Range ||
            targetDistance + target.BoundingRadius() < sphereDistance) {
            continue;
        }
        const Vector3 rayEnd = player.Position().Extend(sphere.Unit.Position(), kSpherePushRange);
        const float hitWidth = QE.Width + target.BoundingRadius();
        if (PointSegmentDistance2D(targetPosition, sphere.Unit.Position(), rayEnd) <= hitWidth) {
            LastSphereETick = SDK::Variables::TickCount() + SDK::Game::Ping() + 100;
            return E.Cast(sphere.Unit.Position());
        }
    }
    return false;
}

static int RSphereCount() {
    return std::max(4, R.Instance().Ammo() + 1);
}

static float RDamage(const AIBaseClient& target) {
    return target.IsValid() ? R.GetDamage(target) * static_cast<float>(RSphereCount()) : 0.0f;
}

static float AllDamage(const AIHeroClient& target) {
    float damage = 0.0f;
    if (Q.IsReady()) {
        damage += Q.GetDamage(target);
    }
    if (W.IsReady()) {
        damage += W.GetDamage(target);
    }
    if (E.IsReady()) {
        damage += E.GetDamage(target);
    }
    if (R.IsReady()) {
        damage += RDamage(target);
    }
    return damage;
}

static bool CastR(const AIHeroClient& target) {
    return R.IsReady() && ValidHeroTarget(target, R.Range) && !RBlocked(target) && R.CastOnUnit(target);
}

static bool ComboR() {
    if (!Bool(RMenu, "UseR", true) || !R.IsReady()) {
        return false;
    }
    const auto target = GetMagicalTarget(R.Range);
    if (!target.IsValid() || RBlocked(target) ||
        EffectiveMagicalHealth(target) < Slider(RMenu, "MinimumHealth", 0)) {
        return false;
    }

    if (List(RMenu, "Mode", 0) == 0) {
        return RDamage(target) >= EffectiveMagicalHealth(target) && CastR(target);
    }
    if (R.Instance().Ammo() < Slider(RMenu, "MinimumSpheres", 5)) {
        return false;
    }
    if (!Bool(RMenu, "FastOnlyKill", true)) {
        return CastR(target);
    }
    return AllDamage(target) >= EffectiveMagicalHealth(target) && CastR(target);
}

static bool ManualQE() {
    if (!Key(ComboMenu, "ManualQE") || !Q.IsReady() || !E.IsReady()) {
        return false;
    }
    const auto player = Player();
    const auto target = GetMagicalTarget(QE.Range);
    const int mode = List(ComboMenu, "ManualQEMode", 0);
    if (mode == 0) {
        return target.IsValid() && target.DistanceToPlayer() > E.Range && CastQE(target);
    }
    if (mode == 1) {
        return CastQEAt(Game::CursorPos());
    }
    if (player.CountEnemyHeroesInRange(
            static_cast<float>(Slider(ComboMenu, "QERange", 1100))) == 0) {
        return CastQEAt(Game::CursorPos());
    }
    return target.IsValid() && target.DistanceToPlayer() > E.Range && CastQE(target);
}

static void Combo() {
    if (!IsComboMode() || Orbwalker::IsWindingUp()) {
        return;
    }

    if (Bool(ComboMenu, "UseQ", true) && Q.IsReady()) {
        const auto target = GetMagicalTarget(Q.Range);
        if (target.IsValid()) {
            if (Bool(ComboMenu, "UseQE", true) && E.IsReady()) {
                if (CastQE(target)) {
                    return;
                }
            } else if (CastQ(target)) {
                return;
            }
        }
    }

    if (Bool(ComboMenu, "UseW", true) && W.IsReady()) {
        const auto target = GetMagicalTarget(W.Range);
        if (target.IsValid() && WLogic(target)) {
            return;
        }
    }

    if (Bool(ComboMenu, "UseE", true) && E.IsReady()) {
        const auto target = GetMagicalTarget(QE.Range);
        if (target.IsValid() && PushSphereAt(target)) {
            return;
        }
    }

    if (Bool(ComboMenu, "UseQE", true) && Q.IsReady() && E.IsReady()) {
        const auto target = GetMagicalTarget(static_cast<float>(Slider(ComboMenu, "QERange", 1100)));
        if (target.IsValid() && target.DistanceToPlayer() > E.Range && CastQE(target)) {
            return;
        }
    }

    (void)ComboR();
}

static void Harass() {
    if (Player().ManaPercent() < Slider(HarassMenu, "Mana", 50) || Orbwalker::IsWindingUp()) {
        return;
    }
    if (Bool(HarassMenu, "UseW", true) && W.IsReady()) {
        const auto target = GetMagicalTarget(W.Range);
        if (target.IsValid() && WLogic(target)) {
            return;
        }
    }
    if (Bool(HarassMenu, "UseQ", true) && Q.IsReady()) {
        const auto target = GetMagicalTarget(Q.Range);
        if (target.IsValid() && CastQ(target)) {
            return;
        }
    }
    if (Bool(HarassMenu, "UseE", true) && E.IsReady()) {
        const auto target = GetMagicalTarget(QE.Range);
        if (target.IsValid()) {
            (void)PushSphereAt(target);
        }
    }
}

static void AutoDashQ() {
    if (!Bool(HarassMenu, "AutoQDash", true) || !Q.IsReady() || Player().IsRecalling() ||
        Player().ManaPercent() <= 15.0f) {
        return;
    }
    for (const auto& enemy : EnemyHeroes(Q.Range)) {
        if (Q.CastIfHitchanceEquals(enemy, HitChance::Dash) == CastStates::SuccessfullyCasted) {
            return;
        }
    }
}

static std::vector<AIBaseClient> LaneMinions(float range) {
    std::vector<AIBaseClient> result;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, range)) {
            result.emplace_back(minion.Handle());
        }
    }
    return result;
}

static std::vector<AIBaseClient> JungleMonsters(float range) {
    std::vector<AIBaseClient> result;
    for (const auto& monster : GameObjects::Jungle()) {
        if (ValidTarget(monster, range)) {
            result.emplace_back(monster.Handle());
        }
    }
    return result;
}

static bool FarmW(const std::vector<AIBaseClient>& units, int minimumHits) {
    if (!W.IsReady() || units.empty()) {
        return false;
    }
    const auto farm = W.GetCircularFarmLocation(units, 180.0f);
    if (farm.MinionsHit < minimumHits) {
        return false;
    }
    if (!HoldingWObject()) {
        return PickupWObject();
    }
    return W.Cast(farm.Position);
}

static bool LaneClear() {
    if (!IsClearMode() || Player().ManaPercent() < Slider(LaneMenu, "Mana", 50)) {
        return false;
    }
    const auto minions = LaneMinions(W.Range);
    if (Bool(LaneMenu, "UseQ", true) && Q.IsReady()) {
        std::vector<AIBaseClient> qMinions;
        for (const auto& minion : minions) {
            if (ValidTarget(minion, Q.Range)) {
                qMinions.push_back(minion);
            }
        }
        const auto farm = Q.GetCircularFarmLocation(qMinions, 120.0f);
        if (farm.MinionsHit >= Slider(LaneMenu, "QHits", 2) && Q.Cast(farm.Position)) {
            return true;
        }
    }
    return Bool(LaneMenu, "UseW", true) &&
           FarmW(minions, Slider(LaneMenu, "WHits", 3));
}

static bool JungleClear() {
    if (!IsClearMode() || Player().ManaPercent() < Slider(JungleMenu, "Mana", 50)) {
        return false;
    }
    auto monsters = JungleMonsters(W.Range);
    std::sort(monsters.begin(), monsters.end(), [](const AIBaseClient& left, const AIBaseClient& right) {
        return left.MaxHealth() > right.MaxHealth();
    });
    if (Bool(JungleMenu, "UseW", true) && FarmW(monsters, 1)) {
        return true;
    }
    if (Bool(JungleMenu, "UseQ", true) && Q.IsReady()) {
        for (const auto& monster : monsters) {
            if (ValidTarget(monster, Q.Range) && Q.Cast(monster.Position())) {
                return true;
            }
        }
    }
    return false;
}

static void Killsteal() {
    for (const auto& enemy : EnemyHeroesByHealth(QE.Range)) {
        if (Bool(KillstealMenu, "UseQ", true) && Q.IsReady() &&
            ValidHeroTarget(enemy, Q.Range) && Q.GetDamage(enemy) >= EffectiveMagicalHealth(enemy) &&
            CastQ(enemy)) {
            return;
        }
        if (Bool(KillstealMenu, "UseW", true) && W.IsReady() &&
            ValidHeroTarget(enemy, W.Range) && W.GetDamage(enemy) >= EffectiveMagicalHealth(enemy) &&
            WLogic(enemy)) {
            return;
        }
        if (Bool(KillstealMenu, "UseR", true) && R.IsReady() &&
            ValidHeroTarget(enemy, R.Range) && !RBlocked(enemy) &&
            EffectiveMagicalHealth(enemy) >= Slider(KillstealMenu, "MinimumHealth", 0) &&
            RDamage(enemy) >= EffectiveMagicalHealth(enemy) && CastR(enemy)) {
            return;
        }
    }
}

static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Bool(AAMenu, "DisableAA", false) || !IsComboMode() ||
        Player().Level() < Slider(AAMenu, "Level", 6)) {
        return;
    }
    if (Bool(AAMenu, "EnableCooldown", true) && !Q.IsReady() && !W.IsReady() && !E.IsReady()) {
        args.Process = true;
        return;
    }
    if (Bool(AAMenu, "EnableLowMana", true) && Player().ManaPercent() <= 20.0f) {
        args.Process = true;
        return;
    }
    args.Process = false;
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DrawQ", true) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFFFFFFu, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawQE", true) && Q.IsReady() && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), QE.Range, 0xFFFFA500u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawW", false) && W.IsReady()) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFF98FB98u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFFFF5555u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawR", true) && R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFF9370DBu, 1.5f, 64);
    }
    if (!Bool(DrawMenu, "DrawSpheres", true)) {
        return;
    }
    const int now = SDK::Variables::TickCount();
    for (const auto& sphere : Spheres) {
        if (!ValidSphere(sphere)) {
            continue;
        }
        Vec2 screen = {};
        if (!Drawing::WorldToScreen(sphere.Unit.Position(), screen) || !screen.IsValid()) {
            continue;
        }
        char remaining[24] = {};
        _snprintf_s(
            remaining,
            sizeof(remaining),
            _TRUNCATE,
            "%.1fs",
            static_cast<float>(std::max(0, sphere.ExpireTick - now)) / 1000.0f);
        Drawing::DrawText(screen.x, screen.y, 0xFFFFFFFFu, remaining);
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() || Game::IsChatOpen()) {
        return;
    }
    R.Range = R.Level() == 3 ? 750.0f : 675.0f;
    PruneSpheres();

    if (ExecutePendingE()) {
        return;
    }
    Killsteal();
    if (ManualQE()) {
        return;
    }
    AutoDashQ();

    const bool autoHarass = Key(HarassMenu, "AutoHarass");
    if (autoHarass && !IsComboMode()) {
        Harass();
    }
    if (IsComboMode()) {
        Combo();
    } else if (IsHarassMode() && !autoHarass) {
        Harass();
    } else if (IsClearMode()) {
        if (!JungleClear()) {
            (void)LaneClear();
        }
    }
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.kuroaio.syndra", "Kuro - Syndra", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo Settings"));
    ComboMenu->Add(new MenuBool("UseQ", "Use Q", true));
    ComboMenu->Add(new MenuBool("UseQE", "Use Q + E", true));
    ComboMenu->Add(new MenuSlider("QERange", "Q + E maximum range", 1100, 800, 1150));
    ComboMenu->Add(new MenuBool("UseW", "Use W", true));
    ComboMenu->Add(new MenuBool("UseE", "Use E with existing spheres", true));
    auto* manualQE = ComboMenu->Add(new MenuKeyBind(
        "ManualQE", "Manual Q + E", SDK::Keys::T, KeyBindType::Press));
    manualQE->Permashow();
    ComboMenu->Add(new MenuList(
        "ManualQEMode", "Manual Q + E mode", { "Target", "Cursor", "Smart" }, 0));

    RMenu = ComboMenu->AddSubMenu(new Menu("R", "Ultimate Settings"));
    RMenu->Add(new MenuList("Mode", "R mode", { "Damage priority", "Speed priority" }, 0));
    RMenu->Add(new MenuBool("UseR", "Use R", true));
    RMenu->Add(new MenuSlider("MinimumHealth", "Minimum target health for R", 0, 0, 500));
    RMenu->Add(new MenuSlider("MinimumSpheres", "Minimum spheres for fast R", 5, 3, 6));
    RMenu->Add(new MenuBool("FastOnlyKill", "Fast R only when combo can kill", true));

    RBlacklistMenu = MenuRoot->AddSubMenu(new Menu("RBlacklist", "R Blacklist"));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const std::string key = RBlacklistKey(enemy);
        std::string label = enemy.CharacterName();
        if (label.empty()) {
            label = "Enemy " + std::to_string(enemy.NetworkId());
        }
        RBlacklistMenu->Add(new MenuBool(key.c_str(), label.c_str(), false));
    }

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass Settings"));
    auto* autoHarass = HarassMenu->Add(new MenuKeyBind(
        "AutoHarass", "Auto Harass", SDK::Keys::K, KeyBindType::Toggle));
    autoHarass->Permashow();
    HarassMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 50, 0, 100));
    HarassMenu->Add(new MenuBool("AutoQDash", "Auto Q dashing targets", true));
    HarassMenu->Add(new MenuBool("UseQ", "Use Q", true));
    HarassMenu->Add(new MenuBool("UseW", "Use W", true));
    HarassMenu->Add(new MenuBool("UseE", "Use E with existing spheres", true));

    AAMenu = MenuRoot->AddSubMenu(new Menu("AA", "Normal Attack Settings"));
    AAMenu->Add(new MenuBool("DisableAA", "Disable normal attacks in Combo", false));
    AAMenu->Add(new MenuSlider("Level", "Disable attacks from level", 6, 1, 18));
    AAMenu->Add(new MenuBool("EnableCooldown", "Enable attacks when all spells are unavailable", true));
    AAMenu->Add(new MenuBool("EnableLowMana", "Enable attacks below 20 percent mana", true));

    LaneMenu = MenuRoot->AddSubMenu(new Menu("LaneClear", "Lane Clear Settings"));
    LaneMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 50, 0, 100));
    LaneMenu->Add(new MenuBool("UseQ", "Use Q", true));
    LaneMenu->Add(new MenuSlider("QHits", "Q minimum minions", 2, 1, 6));
    LaneMenu->Add(new MenuBool("UseW", "Use W", true));
    LaneMenu->Add(new MenuSlider("WHits", "W minimum minions", 3, 1, 6));

    JungleMenu = MenuRoot->AddSubMenu(new Menu("JungleClear", "Jungle Clear Settings"));
    JungleMenu->Add(new MenuSlider("Mana", "Minimum mana percent", 50, 0, 100));
    JungleMenu->Add(new MenuBool("UseQ", "Use Q", true));
    JungleMenu->Add(new MenuBool("UseW", "Use W", true));

    KillstealMenu = MenuRoot->AddSubMenu(new Menu("Killsteal", "Killsteal Settings"));
    KillstealMenu->Add(new MenuBool("UseQ", "Use Q", true));
    KillstealMenu->Add(new MenuBool("UseW", "Use W", true));
    KillstealMenu->Add(new MenuBool("UseR", "Use R", true));
    KillstealMenu->Add(new MenuSlider("MinimumHealth", "Minimum target health for R", 0, 0, 500));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Draw Settings"));
    DrawMenu->Add(new MenuBool("DrawQ", "Draw Q range", true));
    DrawMenu->Add(new MenuBool("DrawQE", "Draw Q + E range", true));
    DrawMenu->Add(new MenuBool("DrawW", "Draw W range", false));
    DrawMenu->Add(new MenuBool("DrawE", "Draw E range", false));
    DrawMenu->Add(new MenuBool("DrawR", "Draw R range", true));
    DrawMenu->Add(new MenuBool("DrawSpheres", "Draw sphere timers", true));

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) {
        return;
    }
    if (auto* item = ComboMenu ? ComboMenu->Get<MenuKeyBind>("ManualQE") : nullptr) {
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
    HarassMenu = nullptr;
    AAMenu = nullptr;
    LaneMenu = nullptr;
    JungleMenu = nullptr;
    KillstealMenu = nullptr;
    DrawMenu = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 800.0f);
    W = Spell(SpellSlot::W, 925.0f);
    E = Spell(SpellSlot::E, 700.0f);
    R = Spell(SpellSlot::R, 675.0f);
    QE = Spell(SpellSlot::E, 1100.0f);
    Q.SetSkillshot(0.6f, 70.0f, FLT_MAX, false, SkillshotType::SkillshotCircle);
    W.SetSkillshot(0.25f, 120.0f, 1600.0f, false, SkillshotType::SkillshotCircle);
    E.SetSkillshot(0.25f, 22.5f, 2500.0f, false, SkillshotType::SkillshotCone);
    R.SetTargetted(0.25f, 1100.0f);
    QE.SetSkillshot(0.5f, 55.0f, 2500.0f, false, SkillshotType::SkillshotLine);

    ArmQE = false;
    PendingE = false;
    PendingEPosition = {};
    PendingETick = 0;
    LastWPickupTick = 0;
    LastWBlockETick = 0;
    LastSphereETick = 0;
    NextWPickupTick = 0;

    BuildMenu();
    InitializeSpheres();
    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Drawing::OnDraw += &OnDraw;
    GameObjects::AddOnCreate(&OnObjectCreate);
    GameObjects::AddOnDelete(&OnObjectDelete);
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>Kuro - Syndra loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Drawing::OnDraw -= &OnDraw;
    GameObjects::RemoveOnCreate(&OnObjectCreate);
    GameObjects::RemoveOnDelete(&OnObjectDelete);
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    RemoveMenu();
    Spheres.clear();
    ArmQE = false;
    PendingE = false;
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Syndra
