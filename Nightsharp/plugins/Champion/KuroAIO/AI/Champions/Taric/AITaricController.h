#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AITaricGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>

namespace Plugins::KuroAIO::AI::Controllers::Taric {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureBeforeAttack;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Lethal;
using ControllerHelpers::NearestEnemyToPlayer;
using ControllerHelpers::InAutoAttackRange;

using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::PreferredEnemyTarget;
using ControllerHelpers::PreserveAttack;
using ControllerHelpers::RawAllyHeroByNetworkId;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellRank;

inline Menu* TacticsMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline int LastCastTick[4]{};
inline int LastAutoTargetId = 0;
inline int LastAutoTick = 0;
inline int ManualOwnershipUntil = 0;
inline int IncomingThreatUntil = 0;
inline int IncomingHardCcUntil = 0;
inline int SelectedAllyId = 0;
inline int BastionAllyId = 0;
inline int BastionLinkUntil = 0;
inline int BravadoCharges = 0;
inline int BravadoExpireTick = 0;
inline int QChargesObserved = 1;
inline bool ECharging = false;
inline int EChargeStartTick = 0;
inline int RCastTick = 0;
inline bool RObservedActive = false;
inline Mode LastMode = Mode::None;

inline int Now() { return SDK::Variables::TickCount(); }

inline bool Throttle(int slot, int delay = 80) {
    return ControllerHelpers::CastThrottleReady(LastCastTick, slot, delay);
}

inline AIHeroClient AllyById(int id) {
    return RawAllyHeroByNetworkId(id);
}

inline bool ProtectedEnemy(const AIHeroClient& target) {
    return !Engine::ValidEnemy(target) || target.IsInvulnerable() ||
        HasSpellShieldOrImmunity(target);
}

inline bool LinkActive() {
    return AllyLinkValid(BastionAllyId, Now(), BastionLinkUntil);
}

inline AIHeroClient LinkedAlly() {
    const auto ally = AllyById(BastionAllyId);
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidAlly(ally, kWLinkRange) ||
        ally.NetworkId() == player.NetworkId()) return {};
    return ally;
}

inline bool BravadoReady() {
    return BravadoCharges > 0 &&
        BravadoWindowOpen(Now(), BravadoExpireTick);
}


inline AIHeroClient SelectBastionAlly(bool defensive = false) {
    const auto linked = LinkedAlly();
    if (linked.IsValid()) return linked;

    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally, kWRange) ||
            ally.NetworkId() == player.NetworkId()) continue;
        const int enemies = Engine::CountEnemiesAt(ally.Position(), 700.0f);
        const float distance = player.Position().Distance2D(ally.Position());
        float score = (100.0f - ally.HealthPercent()) *
            (defensive ? 10.0f : 4.0f);
        score += std::max(ally.TotalAttackDamage() * 0.62f,
                          ally.AP() * 0.48f);
        score += static_cast<float>(enemies) * (defensive ? 330.0f : 170.0f);
        score -= distance * 0.14f;
        if (static_cast<int>(ally.NetworkId()) == SelectedAllyId) score += 280.0f;
        if (score > bestScore) {
            best = ally;
            bestScore = score;
        }
    }
    if (best.IsValid()) SelectedAllyId = static_cast<int>(best.NetworkId());
    return best;
}

inline int ObservedQCharges() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return QChargesObserved;
    const int primary = player.GetBuffCount("TaricQ");
    const int alternate = player.GetBuffCount("taricq");
    const int charge = std::max(primary, alternate);
    if (charge > 0) return std::clamp(charge, 0, QMaxCharges(SpellRank(0)));
    return QChargesObserved;
}

inline bool QNeeded(const AIHeroClient& ally, bool emergency) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const bool selfLow = player.HealthPercent() <=
        Slider(QMenu, "SelfHealThreshold", 72);
    const bool allyLow = Engine::ValidAlly(ally) && ally.HealthPercent() <=
        Slider(QMenu, "AllyHealThreshold", 78);
    const bool threatened = Engine::CountEnemiesAt(player.Position(), 700.0f) > 0 ||
        (Engine::ValidAlly(ally) && Engine::CountEnemiesAt(ally.Position(), 700.0f) > 0);
    return emergency || selfLow || allyLow ||
        (threatened && (selfLow || allyLow));
}

inline float EDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && Engine::ValidEnemy(target)
        ? player.CalculateMagicDamage(target,
            ERawDamage(SpellRank(2), player.AP(), player.BonusArmor())) : 0.0f;
}

inline bool CastQ(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0, mode) || !Throttle(0, 60) ||
        PreserveAttack(reactive) || PlayerManaPercent() <
            Slider(QMenu, "MinimumMana", 28)) return false;
    const auto ally = LinkedAlly();
    QChargesObserved = ObservedQCharges();
    if (QChargesObserved <= 0 || !QNeeded(ally, reactive)) return false;
    if (!QAreaCovers(player.Position(), player.Position())) return false;
    if (!Engine::ControllerCastSelf(0)) return false;
    QChargesObserved = std::max(0, QChargesObserved - 1);
    LastCastTick[0] = Now();
    BravadoCharges = 2;
    BravadoExpireTick = Now() + static_cast<int>(kBravadoWindowSeconds * 1000.0f);
    return true;
}

inline bool CastW(const AIHeroClient& requested, Mode mode,
                  bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1, mode) || !Throttle(1) ||
        PreserveAttack(reactive)) return false;
    const auto ally = requested.IsValid() ? requested : SelectBastionAlly(reactive);
    if (!Engine::ValidAlly(ally, kWRange) || ally.NetworkId() == player.NetworkId() ||
        !LinkInRange(player.Position(), ally.Position(), kWRange)) return false;
    if (!reactive && Engine::UnderEnemyTurret(player.Position()) &&
        Engine::CountEnemiesAt(player.Position(), 700.0f) >
            Engine::CountAlliesAt(player.Position(), 760.0f) + 1) return false;
    if (!Engine::ControllerCastUnit(1, ally)) return false;
    BastionAllyId = SelectedAllyId = static_cast<int>(ally.NetworkId());
    BastionLinkUntil = Now() + 9000;
    LastCastTick[1] = Now();
    BravadoCharges = 2;
    BravadoExpireTick = Now() + static_cast<int>(kBravadoWindowSeconds * 1000.0f);
    return true;
}

inline bool CastE(const AIHeroClient& target, Mode mode,
                  bool reactive = false, bool lethal = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target, kERange) ||
        ProtectedEnemy(target) || !Ready(2, mode) || !Throttle(2, 100) ||
        ECharging || PreserveAttack(reactive, lethal)) return false;
    const Vector3 aim = PredictPosition(target, 0.25f);
    if (!aim.IsValid() || player.Position().Distance2D(aim) >
        kERange + target.BoundingRadius() || SDK::NavMesh::IsWall(aim) ||
        !ELineHits(player.Position(), aim, target.Position(), target.BoundingRadius()) ||
        ControllerHelpers::ProjectileWallBlocksFromPlayer(aim, kEWidth * 0.5f)) return false;
    const int enemies = Engine::CountEnemiesAt(aim, 300.0f);
    if (!reactive && Engine::UnderEnemyTurret(aim) &&
        !Engine::UnderEnemyTurret(player.Position()) && enemies > 0) return false;
    if (!lethal && mode == Mode::Harass && target.HealthPercent() <
        Slider(EMenu, "MinimumTargetHealth", 18)) return false;
    if (!Engine::ControllerCastPosition(2, aim)) return false;
    ECharging = true;
    EChargeStartTick = LastCastTick[2] = Now();
    BravadoCharges = 2;
    BravadoExpireTick = Now() + static_cast<int>(kBravadoWindowSeconds * 1000.0f);
    return true;
}

inline bool CastR(Mode mode, bool reactive = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3, mode) || !Throttle(3, 130) ||
        RCastTick > 0 || PreserveAttack(reactive, reactive)) return false;
    const auto ally = LinkedAlly();
    const int selfEnemies = Engine::CountEnemiesAt(player.Position(), kRRadius);
    const int allyEnemies = Engine::ValidAlly(ally, kWLinkRange)
        ? Engine::CountEnemiesAt(ally.Position(), kRRadius) : 0;
    const bool selfThreat = selfEnemies > 0 && player.HealthPercent() <=
        Slider(RMenu, "SelfHealth", 55);
    const bool allyThreat = Engine::ValidAlly(ally) && allyEnemies > 0 &&
        ally.HealthPercent() <= Slider(RMenu, "AllyHealth", 68);
    const bool teamFight = selfEnemies >= Slider(RMenu, "MinimumEnemies", 2);
    if (!reactive && !selfThreat && !allyThreat && !teamFight) return false;
    if (Engine::UnderEnemyTurret(player.Position()) && !reactive &&
        selfEnemies > Engine::CountAlliesAt(player.Position(), 760.0f) + 1) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    RCastTick = LastCastTick[3] = Now();
    RObservedActive = false;
    BravadoCharges = 2;
    BravadoExpireTick = Now() + static_cast<int>(kBravadoWindowSeconds * 1000.0f);
    return true;
}

inline bool DefensiveAutomatic(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const auto ally = SelectBastionAlly(true);
    const bool allyThreat = Engine::ValidAlly(ally) &&
        ally.HealthPercent() <= Slider(TacticsMenu, "AutomaticAllyHealth", 62) &&
        Engine::CountEnemiesAt(ally.Position(), 700.0f) > 0;
    const bool selfThreat = player.HealthPercent() <=
        Slider(TacticsMenu, "AutomaticSelfHealth", 45) &&
        Engine::CountEnemiesAt(player.Position(), 700.0f) > 0;
    if (allyThreat && !LinkActive() && CastW(ally, Mode::Automatic, true)) return true;
    if ((allyThreat || selfThreat) && CastQ(Mode::Automatic, true)) return true;
    if (Engine::ValidEnemy(target) && (IncomingHardCcUntil > Now() ||
        selfThreat) && CastE(target, Mode::Automatic, true, false)) return true;
    if ((IncomingThreatUntil > Now() || allyThreat || selfThreat) &&
        CastR(Mode::Automatic, true)) return true;
    return false;
}

inline void Combo(const AIHeroClient& target) {
    const auto ally = SelectBastionAlly(false);
    if (Engine::ValidAlly(ally) && !LinkActive() && CastW(ally, Mode::Combo)) return;
    if (Engine::ValidEnemy(target) && BravadoReady() &&
        InAutoAttackRange(target) && !QNeeded(ally, false)) return;
    if (Engine::ValidEnemy(target) && CastE(target, Mode::Combo, false,
        Lethal(target, EDamage(target)))) return;
    if (CastQ(Mode::Combo)) return;
    if (CastR(Mode::Combo)) return;
}

inline void Harass(const AIHeroClient& target) {
    if (PlayerManaPercent() < Slider(QMenu, "HarassMana", 48)) return;
    if (Engine::ValidEnemy(target) && CastE(target, Mode::Harass)) return;
    (void)CastQ(Mode::Harass);
}

inline void Flee(const AIHeroClient& pursuer) {
    const auto ally = SelectBastionAlly(true);
    if (Engine::ValidAlly(ally) && !LinkActive() && CastW(ally, Mode::Flee, true)) return;
    if (CastQ(Mode::Flee, true)) return;
    if (Engine::ValidEnemy(pursuer) && CastE(pursuer, Mode::Flee, true, false)) return;
    (void)CastR(Mode::Flee, true);
}

inline void ReconcileState() {
    const auto player = GameObjects::Player();
    const int now = Now();
    if (!player.IsValid()) return;
    QChargesObserved = ObservedQCharges();
    if (BravadoExpireTick <= now) BravadoCharges = 0;
    const auto linked = LinkedAlly();
    if (player.HasBuff("TaricW") || (linked.IsValid() &&
        (linked.HasBuff("TaricWAllyBuff") || linked.HasBuff("TaricW")))) {
        BastionLinkUntil = std::max(BastionLinkUntil, now + 500);
    } else if (BastionLinkUntil <= now) {
        BastionAllyId = 0;
    }
    if (player.HasBuff("TaricEChargeBuff") || player.HasBuff("TaricE")) {
        ECharging = true;
        if (EChargeStartTick <= 0) EChargeStartTick = now;
    } else if (ECharging && EChargeStartTick > 0 &&
               now - EChargeStartTick > static_cast<int>(kEChargeSeconds * 1000.0f) + 350) {
        ECharging = false;
        EChargeStartTick = 0;
    }
    if (player.HasBuff("TaricR") || player.HasBuff("TaricRTarget")) {
        RObservedActive = true;
        if (RCastTick <= 0) RCastTick = now - static_cast<int>(kRDelaySeconds * 1000.0f);
    }
    if (RCastTick > 0 && now > RInvulnerabilityEndTick(RCastTick) + 250) {
        RCastTick = 0;
        RObservedActive = false;
    }
    if (ManualOwnershipUntil <= now) ManualOwnershipUntil = 0;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    LastMode = mode;
    ReconcileState();
    const AIHeroClient target = PreferredEnemyTarget(selected,
        mode == Mode::Flee ? 900.0f : kERange);
    if (ManualOwnershipUntil > Now()) return true;
    if (mode == Mode::Automatic && DefensiveAutomatic(target)) return true;
    switch (mode) {
    case Mode::Combo: Combo(target); break;
    case Mode::Harass: Harass(target); break;
    case Mode::Flee: Flee(NearestEnemyToPlayer(target, 900.0f)); break;
    case Mode::LaneClear:
    case Mode::Jungle:
    case Mode::LastHit:
        if (PlayerManaPercent() >= Slider(FarmMenu, "Mana", 35)) {
            (void)CastQ(mode, false);
            (void)Engine::TryFarm(mode);
        }
        break;
    case Mode::Automatic: break;
    default: break;
    }
    return true;
}

inline void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    const int now = Now();
    if (IsLocalPlayer(args.Sender)) {
        if (args.IsAutoAttack) return;
        const int slot = static_cast<int>(args.Slot);
        if (slot < 0 || slot > 3) return;
        if (!Engine::WasControllerCast(slot))
            ManualOwnershipUntil = now + Slider(TacticsMenu, "ManualOwnershipMs", 620);
        LastCastTick[slot] = now;
        BravadoCharges = 2;
        BravadoExpireTick = now + static_cast<int>(kBravadoWindowSeconds * 1000.0f);
        if (slot == 2) {
            ECharging = true;
            EChargeStartTick = now;
        }
        if (slot == 3) {
            RCastTick = now;
            RObservedActive = false;
        }
        return;
    }
    const auto analysis = AnalyzeEnemyCast(args);
    if (!analysis.Valid || (!analysis.TargetsPlayer && !analysis.CrossesPlayer)) return;
    IncomingThreatUntil = std::max(IncomingThreatUntil,
        std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
    if (analysis.LikelyHardCrowdControl)
        IncomingHardCcUntil = std::max(IncomingHardCcUntil,
            std::max(analysis.CommitmentUntilTick, analysis.LineThreatUntilTick));
}

inline void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return;
    (void)CaptureLocalAutoAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
    const bool localOwner = IsLocalPlayer(args.Sender);
    const bool alliedOwner = args.Sender.IsValid() &&
        RawAllyHeroByNetworkId(static_cast<int>(args.Sender.NetworkId)).IsValid();
    if (Engine::TextContains(args.BuffName, "TaricWAllyBuff") && alliedOwner) {
        BastionAllyId = static_cast<int>(args.Sender.NetworkId);
        BastionLinkUntil = Now() + 800;
    } else if (Engine::TextContains(args.BuffName, "TaricW") && localOwner) {
        BastionLinkUntil = std::max(BastionLinkUntil, Now() + 800);
    }
    if (Engine::TextContains(args.BuffName, "TaricR") &&
        (localOwner || alliedOwner)) {
        RObservedActive = true;
        if (RCastTick <= 0) RCastTick = Now() - static_cast<int>(kRDelaySeconds * 1000.0f);
    }
    if (Engine::TextContains(args.BuffName, "TaricECharge") && localOwner) {
        ECharging = true;
        EChargeStartTick = Now();
    }
}

inline void OnBuffRemove(const SDK::Events::BuffEventArgs& args) {
    if (Engine::TextContains(args.BuffName, "TaricW") &&
        args.Sender.IsValid() &&
        static_cast<int>(args.Sender.NetworkId) == BastionAllyId) {
        BastionLinkUntil = 0;
        BastionAllyId = 0;
    }
    if (Engine::TextContains(args.BuffName, "TaricECharge") &&
        IsLocalPlayer(args.Sender)) {
        ECharging = false;
        EChargeStartTick = 0;
    }
    if (Engine::TextContains(args.BuffName, "TaricR") &&
        (IsLocalPlayer(args.Sender) ||
         RawAllyHeroByNetworkId(static_cast<int>(args.Sender.NetworkId)).IsValid()))
        RObservedActive = false;
}

inline void OnBuffUpdate(const SDK::Events::BuffEventArgs& args) {
    OnBuffAdd(args);
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureBeforeAttack(args, LastAutoTargetId, LastAutoTick);
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, LastAutoTargetId, LastAutoTick);
    if (BravadoCharges > 0 && BravadoWindowOpen(Now(), BravadoExpireTick)) {
        --BravadoCharges;
        if (BravadoCharges == 0) BravadoExpireTick = 0;
    }
}

inline void OnGapcloser(const SDK::Events::Gapcloser::GapCloserEventArgs&) {
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 750);
}

inline void OnInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs&) {
    IncomingHardCcUntil = std::max(IncomingHardCcUntil, Now() + 850);
    IncomingThreatUntil = std::max(IncomingThreatUntil, Now() + 850);
}

inline void OnObjectCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnObjectDelete(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileCreate(const SDK::Events::ObjectEventArgs&) {}
inline void OnMissileDelete(const SDK::Events::ObjectEventArgs&) {}

inline void OnDraw() {
    if (!Bool(CoachMenu, "DrawRanges", false)) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    Drawing::DrawCircle(player.Position(), kERange, 0xFF69D3FFu, 1.4f, 36);
    Drawing::DrawCircle(player.Position(), kRRadius, 0xFF4385FFu, 1.2f, 36);
    const auto ally = LinkedAlly();
    if (ally.IsValid()) Drawing::DrawCircle(ally.Position(), kQRadius, 0xFF8C7CFFu, 1.0f, 28);
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu("TaricTactics", "Taric ally-link tactics"));
    TacticsMenu->Add(new MenuSlider("ManualOwnershipMs", "Yield after manual cast (ms)", 620, 180, 1400));
    TacticsMenu->Add(new MenuSlider("AutomaticAllyHealth", "Automatic ally health", 62, 10, 95));
    TacticsMenu->Add(new MenuSlider("AutomaticSelfHealth", "Automatic self health", 45, 10, 90));
    QMenu = TacticsMenu->AddSubMenu(new Menu("Q", "Starlight's Touch"));
    QMenu->Add(new MenuSlider("SelfHealThreshold", "Self heal threshold", 72, 20, 95));
    QMenu->Add(new MenuSlider("AllyHealThreshold", "Linked ally heal threshold", 78, 20, 95));
    QMenu->Add(new MenuSlider("MinimumMana", "Minimum mana percent", 28, 0, 90));
    QMenu->Add(new MenuSlider("HarassMana", "Harass mana percent", 48, 10, 90));
    WMenu = TacticsMenu->AddSubMenu(new Menu("W", "Bastion"));
    EMenu = TacticsMenu->AddSubMenu(new Menu("E", "Dazzle"));
    EMenu->Add(new MenuSlider("MinimumTargetHealth", "Minimum harass target health", 18, 0, 90));
    RMenu = TacticsMenu->AddSubMenu(new Menu("R", "Cosmic Radiance"));
    RMenu->Add(new MenuSlider("SelfHealth", "Self danger health", 55, 10, 90));
    RMenu->Add(new MenuSlider("AllyHealth", "Linked ally danger health", 68, 10, 95));
    RMenu->Add(new MenuSlider("MinimumEnemies", "Minimum team-fight enemies", 2, 1, 5));
    FarmMenu = TacticsMenu->AddSubMenu(new Menu("TaricFarm", "Farm safety"));
    FarmMenu->Add(new MenuSlider("Mana", "Farm mana reserve", 35, 0, 90));
    CoachMenu = TacticsMenu->AddSubMenu(new Menu("TaricCoach", "Visual coaching"));
    CoachMenu->Add(new MenuBool("DrawRanges", "Draw E/R and linked Q ranges", false));
}

inline void OnLoad() {
    std::fill(std::begin(LastCastTick), std::end(LastCastTick), 0);
    LastAutoTargetId = LastAutoTick = ManualOwnershipUntil = 0;
    IncomingThreatUntil = IncomingHardCcUntil = SelectedAllyId = 0;
    BastionAllyId = BastionLinkUntil = BravadoExpireTick = 0;
    BravadoCharges = 0;
    QChargesObserved = 1;
    ECharging = false;
    EChargeStartTick = RCastTick = 0;
    RObservedActive = false;
    LastMode = Mode::None;
}

inline void OnUnload() {
    TacticsMenu = QMenu = WMenu = EMenu = RMenu = FarmMenu = CoachMenu = nullptr;
    OnLoad();
}

inline constexpr const char* Scenarios[] = {
    "Pin Taric values to Riot 26.15 and CommunityDragon PC 16.15",
    "Select the explicit enemy before orbwalker and selector fallback",
    "Score a vulnerable high-value ally and retain the Bastion link id",
    "Reconcile TaricW and TaricWAllyBuff link state through events and polling",
    "Track Bravado two-hit empowerment, five-second expiry and attack consumption",
    "Observe Q charge counts and gate Starlight's Touch by missing health and mana",
    "Use Q self area plus remote linked-ally healing without inventing range",
    "Predict Dazzle at 0.25 seconds plus missile travel and target radius",
    "Reject Dazzle through projectile walls, terrain, immunity and unsafe turret corridors",
    "Track the explicit Dazzle charge state and protect ordinary AA windup",
    "Use E lethal damage as a real kill gate rather than generic Q-W-E-R ordering",
    "Start Cosmic Radiance before lethal damage and retain its 2.5-second delay",
    "Transition R pending to observed or timed 2.5-second invulnerability",
    "Reject fresh offensive R turret dives and over-committed enemy counts",
    "Combo preserves Bastion continuity, Bravado weaving, E stun and delayed R timing",
    "Harass uses high-confidence E and real Q healing behind a mana floor",
    "LaneClear Jungle and LastHit preserve Q charges and use shared farm policy",
    "Flee scores a safe ally, heals, predicts peel E and pre-casts defensive R",
    "Automatic mode only responds to ally/self danger, hard CC, gapclosers or multi-target fights",
    "Preserve selected target, orbwalker intent, manual ownership and AA windup",
    "Expose every process, buff, attack, gapcloser, interrupt, object and missile callback",
    "Draw E/R/linked-Q coaching ranges without changing decisions",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Taric;
    controller.ControllerId = "champion.kuroaio.ai.taric.support";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AITaric.md";
    controller.ImplementationSummary =
        "Bravado two-hit weaving, scored Bastion ally link, charged Q sustain, predicted Dazzle and delayed Cosmic Radiance safety controller.";
    controller.Scenarios = Scenarios;
    controller.ScenarioCount = std::size(Scenarios);
    controller.OwnsDecisionLoop = true;
    controller.OnLoad = &OnLoad;
    controller.OnUnload = &OnUnload;
    controller.BuildMenu = &BuildMenu;
    controller.OnUpdate = &OnUpdate;
    controller.OnDraw = &OnDraw;
    controller.OnProcessSpell = &OnProcessSpell;
    controller.OnDoCast = &OnDoCast;
    controller.OnBuffAdd = &OnBuffAdd;
    controller.OnBuffRemove = &OnBuffRemove;
    controller.OnBuffUpdate = &OnBuffUpdate;
    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &OnInterruptable;
    controller.OnObjectCreate = &OnObjectCreate;
    controller.OnObjectDelete = &OnObjectDelete;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Taric
