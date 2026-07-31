#pragma once

#include "../AIChampionEngine.h"
#include "../AIControllerHelpers.h"
#include "AIHweiGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Hwei {

using namespace Geometry;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* DisasterMenu = nullptr;
inline Menu* SerenityMenu = nullptr;
inline Menu* TurmoilMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;
inline SpellbookState Book{};
inline int LastCastTick[4]{};
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCCUntil = 0;
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int LastPassiveTargetId = 0;
inline int LastModeTick = 0;

using ControllerHelpers::Now;
inline bool Ready(int slot, Mode mode, bool allowWindup = false) {
    return slot >= 0 && slot < 4 && ControllerHelpers::ControllerSpellAvailable(
        slot, mode, allowWindup) && LastCastTick[slot] + 85 <= Now();
}
using ControllerHelpers::Protected;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::Lethal;
inline float MagicDamage(const AIHeroClient& target, float raw) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target, raw) : 0.0f;
}
using ControllerHelpers::AP;
inline float QQDamage(const AIHeroClient& t) { return MagicDamage(t, QQRawDamage(SpellRank(0), AP())); }
inline float QWDamage(const AIHeroClient& t) {
    const float missing = t.MaxHealth() > 0.0f
        ? 100.0f * (1.0f - t.Health() / t.MaxHealth()) : 0.0f;
    return MagicDamage(t, QWRawDamage(SpellRank(0), AP(), missing));
}
inline float QEDamage(const AIHeroClient& t) { return MagicDamage(t, QERawDamage(SpellRank(0), AP())); }
inline float EQDamage(const AIHeroClient& t) { return MagicDamage(t, EQRawDamage(SpellRank(2), AP())); }
inline float EWDamage(const AIHeroClient& t) { return MagicDamage(t, EWRawDamage(SpellRank(2), AP())); }
inline float EEDamage(const AIHeroClient& t) { return MagicDamage(t, EERawDamage(SpellRank(2), AP())); }
inline float RDamage(const AIHeroClient& t) { return MagicDamage(t, RRawDamage(SpellRank(3), AP())); }

inline bool SafeZoneAt(const Vector3& point, bool defensive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !point.IsValid() || point.IsZero() ||
        SDK::NavMesh::IsWall(point)) return false;
    const bool inTurret = Engine::UnderEnemyTurret(point);
    const bool newTurret = inTurret && !Engine::UnderEnemyTurret(player.Position());
    return SafeZone({true, inTurret, newTurret,
        Engine::CountEnemiesAt(point, 275.0f),
        Slider(TacticsMenu, "MaxZoneEnemies", 2), defensive});
}
inline Vector3 Aim(const AIHeroClient& target, int slot, float delay,
                   bool allowFallback = true) {
    if (!Engine::ValidEnemy(target)) return {};
    Vector3 aim = PredictPosition(target, delay);
    if (slot >= 0 && slot < 4 && Engine::RuntimeSpells[slot]) {
        const auto prediction = Engine::RuntimeSpells[slot]->GetPrediction(target);
        if (prediction.GetCastPosition().IsValid() &&
            !prediction.GetCastPosition().IsZero()) aim = prediction.GetCastPosition();
    }
    if (allowFallback && (!aim.IsValid() || aim.IsZero())) aim = target.Position();
    return aim;
}
inline bool PredictionOkay(const AIHeroClient& target, int slot,
                           SDK::HitChance wanted = SDK::HitChance::High) {
    if (!Engine::ValidEnemy(target)) return false;
    if (!Engine::RuntimeSpells[slot]) return true;
    const auto prediction = Engine::RuntimeSpells[slot]->GetPrediction(target);
    return prediction.CollisionObjects.empty() &&
        (prediction.Hitchance >= wanted || Engine::IsHardCrowdControlled(target) ||
         target.IsDashing());
}
inline int Slot(Paint paint) { return static_cast<int>(paint); }
inline void SelectCombo(Stance stance, Paint paint) {
    SelectStance(Book, stance, Now());
    SelectPaint(Book, paint, Now());
}
inline bool CastPaint(Stance stance, Paint paint, const AIHeroClient& target,
                      Mode mode, bool reactive = false, bool lethal = false,
                      bool defensive = false) {
    const auto player = GameObjects::Player();
    const int slot = Slot(paint);
    if (!player.IsValid() || !Ready(slot, mode, reactive) || Protected(target) ||
        PreserveAttack(reactive, lethal)) return false;
    Vector3 aim = Aim(target, slot, 0.25f);
    if (stance == Stance::Serenity && paint == Paint::Q)
        aim = player.Position() + Direction2D(player.Position(), target.Position()) * 320.0f;
    if (stance == Stance::Serenity && paint == Paint::E)
        aim = player.Position();
    if (!aim.IsValid() || aim.IsZero() ||
        player.Position().Distance2D(aim) > (stance == Stance::Serenity ? kWQRange : kQQRange) + 50.0f)
        return false;
    if (paint != Paint::W && !PredictionOkay(target, slot,
        lethal ? SDK::HitChance::VeryHigh : SDK::HitChance::High)) return false;
    if ((stance == Stance::Disaster && paint == Paint::E) ||
        (stance == Stance::Serenity && paint == Paint::W) ||
        (stance == Stance::Turmoil && paint != Paint::Q)) {
        if (!SafeZoneAt(aim, defensive)) return false;
    }
    if (!Engine::ControllerCastPosition(slot, aim)) return false;
    const int now = Now();
    SelectCombo(stance, paint);
    LastCastTick[slot] = now;
    LastModeTick = now;
    if (stance == Stance::Disaster || stance == Stance::Turmoil) {
        Book.PassiveMarkTargetId = static_cast<int>(target.NetworkId());
        Book.PassiveMarkExpireTick = now + 4000;
        LastPassiveTargetId = Book.PassiveMarkTargetId;
    }
    if ((stance == Stance::Serenity && paint == Paint::W) ||
        (stance == Stance::Turmoil && paint == Paint::W)) Book.ZoneActive = true;
    return true;
}
inline bool CastR(const AIHeroClient& target, Mode mode, bool reactive = false,
                  bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode, reactive) || Protected(target) ||
        PreserveAttack(reactive, lethal)) return false;
    Vector3 aim = Aim(target, 3, kRDelay);
    if (!aim.IsValid() || aim.IsZero() || player.Position().Distance2D(aim) > kRRange ||
        !PredictionOkay(target, 3, lethal ? SDK::HitChance::VeryHigh : SDK::HitChance::High)) return false;
    int predictedHits = 0;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy) && CircleContains(aim, PredictPosition(enemy, kRDelay), kRRadius,
            enemy.BoundingRadius())) ++predictedHits;
    }
    if (!lethal && !reactive && predictedHits < Slider(TacticsMenu, "MinimumRTargets", 2)) return false;
    if (!Engine::ControllerCastPosition(3, aim)) return false;
    LastCastTick[3] = Now();
    Book.ZoneActive = true;
    return true;
}
inline bool KillSecure(const AIHeroClient& target, Mode mode) {
    if (!Engine::ValidEnemy(target)) return false;
    if (Lethal(target, QWDamage(target)) &&
        CastPaint(Stance::Disaster, Paint::W, target, mode, false, true)) return true;
    if (Lethal(target, EQDamage(target)) &&
        CastPaint(Stance::Turmoil, Paint::Q, target, mode, false, true)) return true;
    if (Lethal(target, EEDamage(target)) &&
        CastPaint(Stance::Turmoil, Paint::E, target, mode, false, true)) return true;
    return Lethal(target, RDamage(target)) && CastR(target, mode, false, true);
}
inline void Combo(const AIHeroClient& target) {
    if (!Engine::ValidEnemy(target)) return;
    if (KillSecure(target, Mode::Combo)) return;
    if (CastPaint(Stance::Disaster, Paint::Q, target, Mode::Combo)) return; // QQ mark/poke
    if (CastPaint(Stance::Turmoil, Paint::W, target, Mode::Combo)) return; // EW pull setup
    if (CastPaint(Stance::Disaster, Paint::E, target, Mode::Combo)) return; // QE zone
    if (CastPaint(Stance::Serenity, Paint::Q, target, Mode::Combo, false, false, true)) return;
    (void)CastR(target, Mode::Combo);
}
inline void Harass(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.ManaPercent() < Slider(SerenityMenu, "HarassMana", 48)) return;
    if (CastPaint(Stance::Disaster, Paint::W, target, Mode::Harass)) return; // QW bolt
    if (CastPaint(Stance::Disaster, Paint::Q, target, Mode::Harass)) return;
    (void)CastPaint(Stance::Turmoil, Paint::Q, target, Mode::Harass);
}
inline void Flee(const AIHeroClient& target) {
    if (Engine::ValidEnemy(target) &&
        CastPaint(Stance::Serenity, Paint::Q, target, Mode::Flee, true, false, true)) return;
    if (Engine::ValidEnemy(target) &&
        CastPaint(Stance::Turmoil, Paint::Q, target, Mode::Flee, true, false, true)) return;
    if (Engine::ValidEnemy(target)) (void)CastPaint(Stance::Serenity, Paint::W, target,
        Mode::Flee, true, false, true);
}
inline void ReconcileState() {
    const int now = Now();
    const auto player = GameObjects::Player();
    const bool zone = player.IsValid() && (player.HasBuff("HweiWZone") ||
        player.HasBuff("HweiEZone") || Book.ZoneActive && now - LastModeTick < 2800);
    const bool mark = player.IsValid() && (Book.PassiveMarkTargetId != 0 ||
        player.HasBuff("HweiPassive"));
    Reconcile(Book, now, zone, mark);
    if (!zone && now - LastModeTick > 3000) Book.ZoneActive = false;
    if (ManualOwnershipUntil <= now) ManualOwnershipUntil = 0;
}
inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    ReconcileState();
    const AIHeroClient target = ControllerHelpers::PreferredEnemyTarget(selected, mode == Mode::Flee ? 1000.0f : kRRange);
    if (ManualOwnershipUntil > Now()) return true;
    if (IncomingThreatUntil > Now() && Engine::ValidEnemy(target) &&
        CastPaint(Stance::Turmoil, Paint::Q, target, mode, true, false, true)) return true;
    if (KillSecure(target, mode)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 1000.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (GameObjects::Player().IsValid() && GameObjects::Player().ManaPercent() >=
            Slider(FarmMenu, mode == Mode::Jungle ? "JungleMana" : "LaneMana", 40))
            (void)Engine::TryFarm(mode);
        break;
    case Mode::Automatic:
        if (IncomingHardCCUntil > Now() && Engine::ValidEnemy(target))
            (void)CastPaint(Stance::Turmoil, Paint::Q, target, mode, true, false, true);
        else if (Engine::ValidEnemy(target) && Lethal(target, RDamage(target)))
            (void)CastR(target, mode, true, true);
        break;
    default: break;
    }
    return true;
}
inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        const int slot = static_cast<int>(args.Slot);
        if (slot >= 0 && slot < 4) {
            LastCastTick[slot] = now;
            if (!Engine::WasControllerCast(slot))
                ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 600);
        }
        return;
    }
    const auto analysis = ControllerHelpers::AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl)
        IncomingHardCCUntil = std::max(IncomingHardCCUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}
inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) {
        if (Engine::TextContains(args.BuffName, "HweiW") ||
            Engine::TextContains(args.BuffName, "HweiE")) Book.ZoneActive = true;
        if (Engine::TextContains(args.BuffName, "HweiR"))
            Book.ZoneActive = true;
    }
}
inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (IsLocalPlayer(args.Sender) && (Engine::TextContains(args.BuffName, "HweiW") ||
        Engine::TextContains(args.BuffName, "HweiE") || Engine::TextContains(args.BuffName, "HweiR")))
        Book.ZoneActive = false;
}
inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kQQRange, 0xFFAC79FFu, 1.5f, 40);
    Drawing::DrawCircle(player.Position(), kRRange, 0xFFE05A9Du, 1.0f, 40);
}
inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("HweiOneTrick", "Hwei paint tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after player spell (ms)", 600, 180, 1300));
    TacticsMenu->Add(new MenuSlider("MaxZoneEnemies", "Maximum enemies in a zone", 2, 0, 5));
    TacticsMenu->Add(new MenuSlider("MinimumRTargets", "Minimum Despair targets", 2, 1, 5));
    DisasterMenu = TacticsMenu->AddSubMenu(new Menu("Disaster", "Disaster paints"));
    SerenityMenu = TacticsMenu->AddSubMenu(new Menu("Serenity", "Serenity paints"));
    SerenityMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 48, 10, 90));
    TurmoilMenu = TacticsMenu->AddSubMenu(new Menu("Turmoil", "Turmoil paints"));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("HweiFarm", "Farm policy"));
    FarmMenu->Add(new MenuSlider("LaneMana", "Lane-clear mana percent", 40, 0, 90));
    FarmMenu->Add(new MenuSlider("JungleMana", "Jungle mana percent", 30, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("HweiCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw spell ranges", false));
}
inline void OnLoad() {
    Book = {};
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    ManualOwnershipUntil = IncomingThreatUntil = IncomingHardCCUntil = 0;
    LastAutoTargetId = LastAutoTick = LastPassiveTargetId = LastModeTick = 0;
}
inline void OnUnload() {
    TacticsMenu = DisasterMenu = SerenityMenu = TurmoilMenu = FarmMenu = CoachMenu = nullptr;
    Book = {};
}
inline constexpr const char* Scenarios[] = {
    "Pin Hwei spell names, ranges, ratios and durations to Riot 26.15 / CommunityDragon 16.15",
    "Model Disaster, Serenity and Turmoil as explicit subject stances",
    "Model QQ QW QE, WQ WW WE and EQ EW EE paint combinations",
    "Reconcile active spellbook and zone state from events plus polling",
    "Track passive Signature marks, target identity and expiry without trusting one event",
    "Use QW delayed bolt for missing-health kill secure with VeryHigh prediction",
    "Use QQ projectile and QE fissure for reliable poke and area setup",
    "Use WQ movement current defensively and WW reflection pool only in safe zones",
    "Use WE stirring lights to support attack weaving and mana sustain",
    "Use EQ fear cone and EW/EE pulls for peel, anti-gapclose and setup",
    "Require line prediction and collision-free telemetry for projectile paints",
    "Reject invalid, invulnerable, spell-shielded and turret-only target commitments",
    "Reject zones through walls, enemy turrets and excessive enemy occupancy",
    "Preserve selected target before orbwalker target and selector fallback",
    "Preserve AA windup except reactive or lethal decisions",
    "Reconcile spell cooldown and mana policy through resolved profile resources",
    "Combo marks with Disaster, controls with Turmoil and protects with Serenity",
    "Harass spends only above configured mana floor and avoids unsolicited Despair",
    "LaneClear, Jungle and LastHit delegate to shared farm policy under mana floors",
    "Flee prioritizes movement current, fear and safe defensive paint",
    "Automatic mode allows only observed defense, hard-CC response or lethal Despair",
    "Yield for manual Q W E R ownership and protect manual channels",
    "Track R zone presence and clear stale state when observed buffs expire",
    "Draw ranges for coaching without modifying decisions",
};
inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionName = "Hwei";
    controller.ControllerId = "champion.kuroaio.ai.hwei.onetrick";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIHwei.md";
    controller.ImplementationSummary =
        "Explicit Hwei subject/paint state, conservative prediction and zone safety, passive and cooldown reconciliation, and complete mode ownership.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &ControllerHelpers::ForwardBuffEvent<OnBuffAdd>;
    controller.OnBeforeAttack = &ControllerHelpers::CaptureBeforeAttackTargetEvent<&LastAutoTargetId>;
    controller.OnAfterAttack = &ControllerHelpers::CaptureAfterAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    controller.OnDoCast = &ControllerHelpers::CaptureLocalAutoAttackEvent<&LastAutoTargetId, &LastAutoTick>;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Hwei
