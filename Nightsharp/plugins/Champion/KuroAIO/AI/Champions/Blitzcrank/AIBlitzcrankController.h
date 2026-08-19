#pragma once

#include "../../AIChampionEngine.h"
#include "../../AIControllerHelpers.h"
#include "AIBlitzcrankGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Blitzcrank {

using namespace Geometry;
using ControllerHelpers::AnalyzeEnemyCast;
using ControllerHelpers::CaptureAfterAttack;
using ControllerHelpers::CaptureGapcloser;
using ControllerHelpers::CaptureInterruptable;
using ControllerHelpers::CaptureLocalAutoAttack;
using ControllerHelpers::CountAlliedFollowup;
using ControllerHelpers::CursorDirectionAgrees;
using ControllerHelpers::EnemyFlashReady;
using ControllerHelpers::EnemySpellReady;
using ControllerHelpers::HasEnemyChampionNear;
using ControllerHelpers::HasNearbyEpicMonster;
using ControllerHelpers::HasReadyPointClickThreatAt;
using ControllerHelpers::HasSpellShieldOrImmunity;
using ControllerHelpers::HeroByNetworkId;
using ControllerHelpers::HeroHasSmite;
using ControllerHelpers::InAutoAttackRange;
using ControllerHelpers::IsCommonUntargetableOrImmune;
using ControllerHelpers::IsEpicMonster;
using ControllerHelpers::IsLocalPlayer;
using ControllerHelpers::Now;
using ControllerHelpers::PlayerManaPercent;
using ControllerHelpers::PredictPosition;
using ControllerHelpers::ProjectileWallBlocks;
using ControllerHelpers::Ready;
using ControllerHelpers::SelectProtectionAlly;
using ControllerHelpers::SpellCost;
using ControllerHelpers::SpellEnabled;
using ControllerHelpers::SpellRank;
using ControllerHelpers::SpellSlotOrEventNameContainsAny;
using ControllerHelpers::UnitByNetworkId;

enum class Posture : std::uint8_t {
    Neutral,
    LaneThreat,
    Catch,
    WalkUp,
    FrontToBack,
    Peel,
    Disengage,
    Objective,
    RoamVision,
    Farm,
};

enum class Sequence : std::uint8_t {
    None,
    HookFlight,
    HookArrival,
    WalkUpE,
    AttackReset,
    PreHookRQ,
    PeelChain,
    StasisExit,
    PlayerLedHook,
};

struct QPlan {
    Vector3 CastPosition = {};
    HookContact FirstContact = {};
    HookEvaluation Evaluation = {};
    int TargetId = 0;
    HookPurpose Purpose = HookPurpose::None;
    SDK::HitChance Hitchance = SDK::HitChance::None;
    float RawDamage = 0.0f;
    float DealtDamage = 0.0f;
    int ExpectedArrivalTick = 0;
    bool Lethal = false;
    bool DangerousDelivery = false;
    bool Valid = false;
};

struct WPlan {
    WEvaluation Evaluation = {};
    int TargetId = 0;
    WPurpose Purpose = WPurpose::None;
    bool Valid = false;
};

struct EPlan {
    EDecision Decision = {};
    int TargetId = 0;
    bool Valid = false;
};

struct RPlan {
    REvaluation Evaluation = {};
    int PrimaryTargetId = 0;
    RPurpose Purpose = RPurpose::None;
    int HitCount = 0;
    float ShieldsDestroyed = 0.0f;
    bool Valid = false;
};

struct EnemyWindow {
    int NetworkId = 0;
    int CommittedUntil = 0;
    int HardCrowdControlSpentUntil = 0;
    int EscapeSpentUntil = 0;
    int EscapeCastUntil = 0;
    int LastSpellTick = 0;
};

struct StasisRecord {
    int NetworkId = 0;
    int EndTick = 0;
    Vector3 Position = {};
};

inline Menu* TacticsMenu = nullptr;
inline Menu* RoleMenu = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* CoachMenu = nullptr;

inline std::array<EnemyWindow, 16> EnemyWindows = {};
inline std::array<StasisRecord, 8> StasisRecords = {};
inline RMarkTracker MarkTracker = {};

inline Posture CurrentPosture = Posture::Neutral;
inline Sequence ActiveSequence = Sequence::None;
inline QPlan LastQPlan = {};
inline WPlan LastWPlan = {};
inline EPlan LastEPlan = {};
inline RPlan LastRPlan = {};

inline int ProtectedAllyId = 0;
inline int ThreatenedAllyId = 0;
inline int PeelThreatId = 0;
inline int AllyThreatUntil = 0;
inline int GapcloserTargetId = 0;
inline int GapcloserExpireTick = 0;
inline Vector3 GapcloserEndpoint = {};
inline int InterruptTargetId = 0;
inline int InterruptExpireTick = 0;

inline int LastQCastTick = 0;
inline int LastWCastTick = 0;
inline int LastECastTick = 0;
inline int LastRCastTick = 0;
inline int LastBeforeAttackTargetId = 0;
inline int LastBeforeAttackTick = 0;
inline int LastAfterAttackTargetId = 0;
inline int LastAfterAttackTick = 0;
inline int LastLocalAutoTargetId = 0;
inline int LastLocalAutoTick = 0;
inline int PlayerOverrideUntil = 0;
inline int SequenceExpireTick = 0;

inline bool WActive = false;
inline int WExpireTick = 0;
inline int WSelfSlowExpireTick = 0;
inline bool EArmed = false;
inline int EExpireTick = 0;
inline int DesiredETargetId = 0;
inline bool QInFlight = false;
inline int QFlightTargetId = 0;
inline int QExpectedArrivalTick = 0;
inline Vector3 QFlightEnd = {};
inline int QMissileId = 0;
inline int LastManaBarrierProcTick = -1000000;

inline constexpr int kManualOwnershipMs = 420;
inline constexpr int kManualHookAssistDelayMs = 115;
inline constexpr int kHookArrivalGraceMs = 520;

inline bool IsQEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::Q,
        { "rocketgrab", "blitzcrankq" });
}

inline bool IsWEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::W,
        { "overdrive", "blitzcrankw" });
}

inline bool IsEEvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::E,
        { "powerfist", "blitzcranke" });
}

inline bool IsREvent(const SDK::Events::ProcessSpellEventArgs& args) {
    return SpellSlotOrEventNameContainsAny(
        args, SDK::SpellSlot::R,
        { "staticfield", "blitzcrankr" });
}

inline bool IsHookMissile(const SDK::Events::ObjectEventArgs& args) {
    return ControllerHelpers::AnyTextContains(
        { args.Sender.Name, args.Sender.CharacterName,
          args.SpellName, args.MissileName },
        { "rocketgrabmissile", "rocketgrab" });
}

inline AIHeroClient RawEnemyById(int networkId) {
    return ControllerHelpers::RawEnemyHeroByNetworkId(networkId);
}

inline AIHeroClient RawAllyById(int networkId) {
    return ControllerHelpers::RawAllyHeroByNetworkId(networkId);
}

inline EnemyWindow* WindowFor(int networkId, bool create = false) {
    return ControllerHelpers::FindEnemyCastWindow(
        EnemyWindows, networkId, create);
}

template <std::size_t N>
inline bool ChampionIdIn(const AIHeroClient& champion,
                         const std::array<SDK::ChampionId, N>& ids) {
    const SDK::ChampionId championId = ControllerHelpers::ChampionIdOf(champion);
    if (championId == SDK::ChampionId::Unknown) return false;
    for (const SDK::ChampionId candidate : ids) {
        if (championId == candidate) return true;
    }
    return false;
}

inline PullArchetype ClassifyPullArchetype(const AIHeroClient& target) {
    static constexpr std::array<SDK::ChampionId, 28> carries = {
        SDK::ChampionId::Aphelios, SDK::ChampionId::Ashe,
        SDK::ChampionId::Caitlyn, SDK::ChampionId::Corki,
        SDK::ChampionId::Draven, SDK::ChampionId::Ezreal,
        SDK::ChampionId::Jhin, SDK::ChampionId::Jinx,
        SDK::ChampionId::Kaisa, SDK::ChampionId::Kalista,
        SDK::ChampionId::KogMaw, SDK::ChampionId::Lucian,
        SDK::ChampionId::MissFortune, SDK::ChampionId::Nilah,
        SDK::ChampionId::Quinn, SDK::ChampionId::Samira,
        SDK::ChampionId::Senna, SDK::ChampionId::Sivir,
        SDK::ChampionId::Smolder, SDK::ChampionId::Tristana,
        SDK::ChampionId::Twitch, SDK::ChampionId::Varus,
        SDK::ChampionId::Vayne, SDK::ChampionId::Xayah,
        SDK::ChampionId::Zeri, SDK::ChampionId::Azir,
        SDK::ChampionId::Kayle, SDK::ChampionId::Ryze,
    };
    static constexpr std::array<SDK::ChampionId, 11> enchanters = {
        SDK::ChampionId::Janna, SDK::ChampionId::Karma,
        SDK::ChampionId::Lulu, SDK::ChampionId::Milio,
        SDK::ChampionId::Nami, SDK::ChampionId::RenataGlasc,
        SDK::ChampionId::Seraphine, SDK::ChampionId::Sona,
        SDK::ChampionId::Soraka, SDK::ChampionId::Yuumi,
        SDK::ChampionId::Ivern,
    };
    static constexpr std::array<SDK::ChampionId, 10> artillery = {
        SDK::ChampionId::Hwei, SDK::ChampionId::Jayce,
        SDK::ChampionId::Lux, SDK::ChampionId::Nidalee,
        SDK::ChampionId::Velkoz, SDK::ChampionId::Xerath,
        SDK::ChampionId::Ziggs, SDK::ChampionId::Zoe,
        SDK::ChampionId::Karthus, SDK::ChampionId::Heimerdinger,
    };
    static constexpr std::array<SDK::ChampionId, 16> assassins = {
        SDK::ChampionId::Akali, SDK::ChampionId::Evelynn,
        SDK::ChampionId::Fizz, SDK::ChampionId::Kassadin,
        SDK::ChampionId::Katarina, SDK::ChampionId::KhaZix,
        SDK::ChampionId::Leblanc, SDK::ChampionId::Naafiri,
        SDK::ChampionId::Nocturne, SDK::ChampionId::Qiyana,
        SDK::ChampionId::Rengar, SDK::ChampionId::Shaco,
        SDK::ChampionId::Talon, SDK::ChampionId::Zed,
        SDK::ChampionId::Ekko, SDK::ChampionId::Diana,
    };
    static constexpr std::array<SDK::ChampionId, 25> divers = {
        SDK::ChampionId::Ambessa, SDK::ChampionId::Belveth,
        SDK::ChampionId::Briar, SDK::ChampionId::Camille,
        SDK::ChampionId::Diana, SDK::ChampionId::Elise,
        SDK::ChampionId::Hecarim, SDK::ChampionId::Irelia,
        SDK::ChampionId::JarvanIV, SDK::ChampionId::Jax,
        SDK::ChampionId::Kled, SDK::ChampionId::LeeSin,
        SDK::ChampionId::MasterYi, SDK::ChampionId::Nocturne,
        SDK::ChampionId::Olaf, SDK::ChampionId::Pantheon,
        SDK::ChampionId::RekSai, SDK::ChampionId::Renekton,
        SDK::ChampionId::Rengar, SDK::ChampionId::Sylas,
        SDK::ChampionId::Vi, SDK::ChampionId::Viego,
        SDK::ChampionId::Warwick, SDK::ChampionId::MonkeyKing,
        SDK::ChampionId::XinZhao,
    };
    static constexpr std::array<SDK::ChampionId, 16> juggernauts = {
        SDK::ChampionId::Aatrox, SDK::ChampionId::Darius,
        SDK::ChampionId::DrMundo, SDK::ChampionId::Garen,
        SDK::ChampionId::Illaoi, SDK::ChampionId::Mordekaiser,
        SDK::ChampionId::Nasus, SDK::ChampionId::Sett,
        SDK::ChampionId::Shyvana, SDK::ChampionId::Skarner,
        SDK::ChampionId::Trundle, SDK::ChampionId::Udyr,
        SDK::ChampionId::Urgot, SDK::ChampionId::Volibear,
        SDK::ChampionId::Yorick, SDK::ChampionId::KSante,
    };
    static constexpr std::array<SDK::ChampionId, 18> engageBombs = {
        SDK::ChampionId::Alistar, SDK::ChampionId::Amumu,
        SDK::ChampionId::Galio, SDK::ChampionId::Gragas,
        SDK::ChampionId::Kennen, SDK::ChampionId::Leona,
        SDK::ChampionId::Malphite, SDK::ChampionId::Maokai,
        SDK::ChampionId::Neeko, SDK::ChampionId::Nautilus,
        SDK::ChampionId::Nunu, SDK::ChampionId::Ornn,
        SDK::ChampionId::Rakan, SDK::ChampionId::Rell,
        SDK::ChampionId::Sejuani, SDK::ChampionId::Sion,
        SDK::ChampionId::Zac, SDK::ChampionId::Fiddlesticks,
    };
    static constexpr std::array<SDK::ChampionId, 10> wardens = {
        SDK::ChampionId::Braum, SDK::ChampionId::Poppy,
        SDK::ChampionId::Shen, SDK::ChampionId::TahmKench,
        SDK::ChampionId::Taric, SDK::ChampionId::Thresh,
        SDK::ChampionId::Rammus, SDK::ChampionId::Chogath,
        SDK::ChampionId::Blitzcrank, SDK::ChampionId::KSante,
    };
    if (ChampionIdIn(target, carries)) return PullArchetype::Carry;
    if (ChampionIdIn(target, enchanters)) return PullArchetype::Enchanter;
    if (ChampionIdIn(target, artillery)) return PullArchetype::Artillery;
    if (ChampionIdIn(target, assassins)) return PullArchetype::Assassin;
    if (ChampionIdIn(target, divers)) return PullArchetype::Diver;
    if (ChampionIdIn(target, juggernauts)) return PullArchetype::Juggernaut;
    if (ChampionIdIn(target, engageBombs)) return PullArchetype::EngageBomb;
    if (ChampionIdIn(target, wardens)) return PullArchetype::Warden;
    return PullArchetype::Other;
}

struct MobilityRule {
    SDK::ChampionId Champion = SDK::ChampionId::Unknown;
    SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
};

inline constexpr std::array<MobilityRule, 74> MobilityRules = {
    MobilityRule{ SDK::ChampionId::Ahri, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::Akali, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Akali, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::Akshan, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Belveth, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Briar, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Camille, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Corki, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Diana, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Ekko, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Elise, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Ezreal, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Fiora, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Fizz, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Fizz, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Galio, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Gnar, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Graves, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Gwen, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Irelia, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Jax, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Kassadin, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::Katarina, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Kayn, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Kayn, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Kayn, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::KhaZix, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Kindred, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Kled, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Leblanc, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::LeeSin, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::LeeSin, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Lucian, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Malphite, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::MasterYi, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Naafiri, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Nidalee, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Nilah, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Nocturne, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::Pantheon, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Pyke, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Qiyana, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Qiyana, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Quinn, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Rakan, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Rakan, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::RekSai, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Renekton, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Riven, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Riven, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Samira, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Sejuani, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Shaco, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Shen, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Shyvana, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::Sylas, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Talon, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Tristana, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Tryndamere, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Vayne, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Vi, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Vi, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::Viego, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Viego, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::Warwick, SDK::SpellSlot::Q },
    MobilityRule{ SDK::ChampionId::Warwick, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::MonkeyKing, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::XinZhao, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Yasuo, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Yone, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Yone, SDK::SpellSlot::R },
    MobilityRule{ SDK::ChampionId::Zac, SDK::SpellSlot::E },
    MobilityRule{ SDK::ChampionId::Zed, SDK::SpellSlot::W },
    MobilityRule{ SDK::ChampionId::Zeri, SDK::SpellSlot::E },
};

inline bool EnemyMobilityReady(const AIHeroClient& target) {
    if (!target.IsValid()) return false;
    for (const auto& rule : MobilityRules) {
        if (ControllerHelpers::ChampionIs(target, rule.Champion) &&
            EnemySpellReady(target, rule.Slot)) return true;
    }
    return EnemyFlashReady(target);
}

inline bool EnemyMobilitySpell(const AIHeroClient& target, int slot) {
    if (!target.IsValid() || slot < 0) return false;
    for (const auto& rule : MobilityRules) {
        if (ControllerHelpers::ChampionIs(target, rule.Champion) &&
            slot == static_cast<int>(rule.Slot)) return true;
    }
    return false;
}

inline bool TargetDisplacementImmune(const AIHeroClient& target) {
    return ControllerHelpers::HasAnyBuff(target, {
        "OlafRagnarok", "SionR", "MalphiteR", "ViR",
        "WarwickR", "HecarimUlt", "VolibearR", "ShyvanaTransform",
        "OrnnW", "UdyrE2", "KSanteW", "KSanteW_AllOut",
        "SettR", "BriarE", "GalioE",
    });
}

inline bool AttackBlockedByZone(const AIHeroClient& target) {
    return ControllerHelpers::HasAnyBuff(target, {
        "ShenWBuff", "shenwbuff", "JaxCounterStrike",
        "NilahW", "nilahw",
    });
}

inline float TotalDamageShields(const AIHeroClient& target) {
    return target.IsValid()
        ? std::max(0.0f, target.AllShield()) +
          std::max(0.0f, target.PhysicalShield()) +
          std::max(0.0f, target.MagicalShield())
        : 0.0f;
}

inline float EnemyPriority(const AIHeroClient& target) {
    if (!target.IsValid()) return 0.0f;
    float result = std::max(
        target.TotalAttackDamage() * 0.012f,
        target.AP() * 0.009f);
    result += std::max(0.0f, target.AttackRange() - 175.0f) / 420.0f;
    if (IsPremiumCatch(ClassifyPullArchetype(target))) result += 1.4f;
    if (HeroHasSmite(target) && HasNearbyEpicMonster(1800.0f)) result += 1.2f;
    return std::clamp(result, 0.5f, 5.0f);
}

inline Vector3 EstimatedVelocity(const AIBaseClient& unit) {
    if (!unit.IsValid()) return {};
    const Vector3 future = PredictPosition(unit, 0.50f);
    Vector3 velocity = (future - unit.Position()) / 0.50f;
    velocity.y = 0.0f;
    const float maximum = std::max(325.0f, unit.MoveSpeed() * 1.65f);
    const float speed = velocity.Length2D();
    if (speed > maximum && speed > 0.001f) velocity = velocity * (maximum / speed);
    return velocity;
}

inline HookBody RuntimeHookBody(const AIBaseClient& unit,
                                bool champion,
                                bool minion,
                                bool monster,
                                bool futureTargetable = false) {
    HookBody body{};
    if (!unit.IsValid() || unit.IsDead()) return body;
    body.Position = unit.Position();
    body.Velocity = EstimatedVelocity(unit);
    body.Radius = unit.BoundingRadius();
    body.Id = static_cast<int>(unit.NetworkId());
    body.Valid = true;
    body.Targetable = futureTargetable || unit.IsTargetable();
    body.Hostile = true;
    body.Champion = champion;
    body.Minion = minion;
    body.Monster = monster;
    if (champion) {
        const AIHeroClient hero(unit.Address());
        body.Archetype = ClassifyPullArchetype(hero);
    }
    return body;
}

inline std::vector<HookBody> BuildHookBodies(int futureTargetableId = 0) {
    const auto player = GameObjects::Player();
    std::vector<HookBody> result;
    result.reserve(50);
    if (!player.IsValid()) return result;
    const auto append = [&](const AIBaseClient& unit,
                            bool champion,
                            bool minion,
                            bool monster) {
        if (!unit.IsValid() || unit.IsDead() ||
            player.Position().Distance2D(unit.Position()) > 1320.0f) return;
        HookBody body = RuntimeHookBody(
            unit, champion, minion, monster,
            static_cast<int>(unit.NetworkId()) == futureTargetableId);
        if (body.Valid && body.Targetable) result.push_back(body);
    };
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        append(enemy, true, false, false);
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        append(minion, false, true, false);
    }
    for (const auto& monster : GameObjects::Jungle()) {
        append(monster, false, false, true);
    }
    return result;
}

inline bool UnderAlliedTurret(const Vector3& position) {
    for (const auto& turret : GameObjects::AllyTurrets()) {
        if (!turret.IsValid() || turret.IsDead()) continue;
        const float range = std::max(775.0f, turret.AttackRange()) + 75.0f;
        if (position.DistanceSqr2D(turret.Position()) <= range * range) {
            return true;
        }
    }
    return false;
}

inline AIHeroClient PreferredEnemy(const AIHeroClient& selected,
                                   float range = 1300.0f) {
    if (Engine::ValidEnemy(selected, range)) return selected;
    const AIHeroClient locked = HeroByNetworkId(
        Engine::LockedTargetNetworkId);
    if (Engine::ValidEnemy(locked, range)) return locked;
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    const auto player = GameObjects::Player();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range)) continue;
        float score = EnemyPriority(enemy) * 150.0f -
            player.Position().Distance2D(enemy.Position()) * 0.12f +
            (100.0f - enemy.HealthPercent()) * 2.0f;
        if (Engine::IsHardCrowdControlled(enemy)) score += 260.0f;
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    return best;
}

inline AIHeroClient ProtectedAlly() {
    return RawAllyById(ProtectedAllyId);
}

inline float ThreatToAllyScore(const AIHeroClient& enemy,
                               const AIHeroClient& ally) {
    if (!Engine::ValidEnemy(enemy) || !Engine::ValidAlly(ally)) return -FLT_MAX;
    const float distance = enemy.Position().Distance2D(ally.Position());
    if (distance > 1000.0f) return -FLT_MAX;
    float score = 900.0f - distance + enemy.TotalAttackDamage() * 0.60f +
        enemy.AP() * 0.35f;
    if (enemy.IsDashing() && enemy.PathEnd().IsValid() &&
        enemy.PathEnd().Distance2D(ally.Position()) < distance) score += 420.0f;
    if (static_cast<int>(enemy.NetworkId()) == PeelThreatId &&
        Now() <= AllyThreatUntil) score += 520.0f;
    if (IsDangerousDelivery(ClassifyPullArchetype(enemy))) score += 160.0f;
    return score;
}

inline AIHeroClient SelectPeelThreat(const AIHeroClient& ally) {
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const float score = ThreatToAllyScore(enemy, ally);
        if (score > bestScore) {
            best = enemy;
            bestScore = score;
        }
    }
    return bestScore >= static_cast<float>(
        Slider(RoleMenu, "PeelThreatScore", 590)) ? best : AIHeroClient{};
}

inline SDK::HitChance RequiredQHitchance(HookPurpose purpose,
                                         const AIHeroClient& target) {
    if (Engine::IsHardCrowdControlled(target) ||
        purpose == HookPurpose::StasisExit) {
        return SDK::HitChance::Immobile;
    }
    if (target.IsDashing() || purpose == HookPurpose::DashEndpoint ||
        purpose == HookPurpose::Peel ||
        purpose == HookPurpose::Interrupt ||
        purpose == HookPurpose::ObjectiveJungler) {
        return SDK::HitChance::High;
    }
    SDK::HitChance baseChance = SDK::HitChance::VeryHigh;
    switch (List(QMenu, "Hitchance", 2)) {
    case 0: baseChance = SDK::HitChance::Medium; break;
    case 1: baseChance = SDK::HitChance::High; break;
    case 3: baseChance = SDK::HitChance::Immobile; break;
    default: baseChance = SDK::HitChance::VeryHigh; break;
    }
    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo && baseChance != SDK::HitChance::Immobile) {
        if (baseChance == SDK::HitChance::VeryHigh) baseChance = SDK::HitChance::High;
        else if (baseChance == SDK::HitChance::High) baseChance = SDK::HitChance::Medium;
    }
    return baseChance;
}

inline bool EnemyWindowEscapeSpent(int networkId) {
    const EnemyWindow* window = ControllerHelpers::EnemyCastWindowById(
        EnemyWindows, networkId);
    return window && window->EscapeSpentUntil >= Now();
}

inline bool EnemyEscapeCastStarted(int networkId) {
    const EnemyWindow* window = ControllerHelpers::EnemyCastWindowById(
        EnemyWindows, networkId);
    return window && window->EscapeCastUntil >= Now();
}

inline bool EnemyKeyCooldownsSpent(int networkId) {
    const EnemyWindow* window = ControllerHelpers::EnemyCastWindowById(
        EnemyWindows, networkId);
    return window &&
        (window->EscapeSpentUntil >= Now() ||
         window->HardCrowdControlSpentUntil >= Now());
}

inline bool TargetInStasis(const AIHeroClient& target) {
    return target.IsValid() && ControllerHelpers::HasAnyBuff(target, {
        "zhonyasringshield", "ZhonyasRingShield", "BardRStasis",
        "bardrstasis", "ChronoRevive", "GuardianAngel",
    });
}

inline StasisRecord* StasisFor(int networkId, bool create = false) {
    if (networkId == 0) return nullptr;
    for (auto& record : StasisRecords) {
        if (record.NetworkId == networkId) return &record;
    }
    if (!create) return nullptr;
    for (auto& record : StasisRecords) {
        if (record.NetworkId == 0 || record.EndTick + 800 < Now()) {
            record = {};
            record.NetworkId = networkId;
            return &record;
        }
    }
    return nullptr;
}

inline bool IsHookRejectedState(const AIHeroClient& target,
                                bool allowStasis) {
    if (!target.IsValid() || target.IsDead() || target.IsInvulnerable()) {
        return !allowStasis || !TargetInStasis(target);
    }
    if ((!target.IsTargetable() || !target.IsVisible()) && !allowStasis) {
        return true;
    }
    if (ControllerHelpers::HasAnyBuff(target, {
            "FioraW", "VladimirSanguinePool", "FizzE", "FizzEIcon",
            "EliseSpiderE", "KayleR", "TaricR",
        })) {
        return true;
    }
    return false;
}

inline float WEReliableReach(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const float attackReach = player.AttackRange() +
        target.BoundingRadius() + kEBonusAttackRange;
    const float bonusTravel = WBonusTravelDistance(
        player.MoveSpeed(), SpellRank(1), 1.15f);
    return attackReach + bonusTravel;
}

inline bool WalkUpEReliable(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target) ||
        !Ready(1) || !Ready(2) || EnemyMobilityReady(target) ||
        TargetDisplacementImmune(target)) {
        return false;
    }
    const float distance = player.Position().Distance2D(target.Position());
    if (distance > WEReliableReach(target)) return false;
    Vector3 intended = player.PathEnd();
    if (!intended.IsValid() || intended.IsZero()) intended = player.Position();
    return !Engine::UnderEnemyTurret(intended) &&
        !HasReadyPointClickThreatAt(intended) &&
        Engine::CountEnemiesAt(intended, 650.0f) <=
            Engine::CountAlliesAt(intended, 700.0f) + 1;
}

inline float HitchanceConfidence(SDK::HitChance hitchance) {
    switch (hitchance) {
    case SDK::HitChance::Immobile: return 1.0f;
    case SDK::HitChance::VeryHigh: return 0.92f;
    case SDK::HitChance::High: return 0.82f;
    case SDK::HitChance::Medium: return 0.64f;
    default: return 0.35f;
    }
}

inline std::vector<Vector3> HookCandidates(const AIHeroClient& target,
                                           const Vector3& forcedPosition = {}) {
    std::vector<Vector3> candidates;
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return candidates;
    Vector3 predicted = forcedPosition;
    if (!predicted.IsValid() || predicted.IsZero()) {
        const auto prediction = Engine::RuntimeSpells[0]
            ? Engine::RuntimeSpells[0]->GetPrediction(target)
            : SDK::PredictionOutput{};
        predicted = prediction.GetCastPosition();
        if (!predicted.IsValid() || predicted.IsZero()) {
            const float travel = kQCastSeconds +
                player.Position().Distance2D(target.Position()) /
                    kQMissileSpeed;
            predicted = PredictPosition(target, travel);
        }
    }
    const Vector3 direct = SharedGeometry::Direction2D(
        player.Position(), predicted);
    if (direct.IsZero()) return candidates;
    static constexpr std::array<float, 9> offsets = {
        0.0f, 0.012f, -0.012f, 0.025f, -0.025f,
        0.045f, -0.045f, 0.072f, -0.072f,
    };
    for (float offset : offsets) {
        const Vector3 direction = SharedGeometry::Rotate2D(direct, offset);
        if (!direction.IsZero()) {
            candidates.push_back(
                player.Position() + direction * kQMaximumTargetCenterRange);
        }
    }
    if (target.PathEnd().IsValid() && !target.PathEnd().IsZero() &&
        target.Position().Distance2D(target.PathEnd()) > 40.0f) {
        candidates.push_back(target.PathEnd());
    }
    return candidates;
}

inline HookContext RuntimeHookContext(
    const AIHeroClient& target,
    HookPurpose purpose,
    SDK::HitChance hitchance,
    const HookContact& contact,
    bool lethal,
    bool selected,
    bool allowStasis) {
    HookContext context{};
    const auto player = GameObjects::Player();
    const Vector3 landing = PullLandingPosition(
        player.Position(), contact.TargetPosition.IsValid()
            ? contact.TargetPosition : target.Position());
    const AIHeroClient protectedAlly = ProtectedAlly();
    const PullArchetype archetype = ClassifyPullArchetype(target);
    context.Ready = Ready(0);
    context.TargetValid = target.IsValid();
    context.IntendedFirstBody = contact.Hit &&
        contact.BodyId == static_cast<int>(target.NetworkId());
    context.ProjectileWallBlocked = ProjectileWallBlocks(
        player.Position(), LastQPlan.CastPosition.IsValid()
            ? LastQPlan.CastPosition : contact.MissilePosition,
        kQCollisionRadius);
    context.TargetSpellShield = HasSpellShieldOrImmunity(target);
    context.TargetUnstoppable = TargetDisplacementImmune(target);
    context.HighConfidence = static_cast<int>(hitchance) >=
        static_cast<int>(SDK::HitChance::VeryHigh) ||
        Engine::IsHardCrowdControlled(target) || target.IsDashing() ||
        allowStasis;
    context.SelectedTarget = selected;
    context.CursorAgrees = CursorDirectionAgrees(target.Position(), -0.12f) ||
                           (Orbwalker::ActiveMode() == OrbwalkingMode::Combo);
    context.TargetImmobile = Engine::IsHardCrowdControlled(target);
    context.TargetDashEnding = target.IsDashing() ||
        purpose == HookPurpose::DashEndpoint;
    context.TargetLeavingStasis = allowStasis;
    context.TargetCanInstantEscape = EnemyMobilityReady(target);
    context.TargetEscapeSpent = EnemyWindowEscapeSpent(
        static_cast<int>(target.NetworkId()));
    context.TargetKillable = lethal;
    context.TargetIsolated = Engine::CountEnemiesAt(
        target.Position(), 650.0f) <= 1;
    context.TargetKeyCooldownsSpent = EnemyKeyCooldownsSpent(
        static_cast<int>(target.NetworkId()));
    context.PullsTowardAlliedTurret = UnderAlliedTurret(landing);
    context.PullsOntoProtectedCarry = protectedAlly.IsValid() &&
        protectedAlly.Position().Distance2D(landing) <= 475.0f;
    context.ProtectedCarryThreatened = protectedAlly.IsValid() &&
        target.Position().Distance2D(protectedAlly.Position()) <= 525.0f;
    context.PeelDisplacement = purpose == HookPurpose::Peel;
    context.InterruptUrgent = purpose == HookPurpose::Interrupt;
    context.AlliesAtLanding = CountAlliedFollowup(landing, 850.0f, true);
    context.EnemiesAtLanding = Engine::CountEnemiesAt(landing, 750.0f);
    context.FollowupAvailable = context.AlliesAtLanding >=
        Slider(QMenu, "MinimumFollowup", 1) || lethal;
    context.WEReliableWalkup = Bool(QMenu, "HoldForWE", true) &&
        WalkUpEReliable(target);
    context.EPrimedInAttackRange = EArmed && InAutoAttackRange(
        target, kEBonusAttackRange);
    context.PlayerAttackWindingUp = Orbwalker::IsWindingUp();
    context.ObjectiveContest = HasNearbyEpicMonster(1800.0f);
    context.TargetHealthPercent = target.HealthPercent();
    context.TargetPriority = EnemyPriority(target);
    context.CollisionConfidence = HitchanceConfidence(hitchance);
    context.Archetype = archetype;
    context.Purpose = purpose;
    return context;
}

inline QPlan BuildQPlan(const AIHeroClient& target,
                        HookPurpose purpose,
                        int selectedTargetId = 0,
                        bool allowStasis = false,
                        const Vector3& forcedPosition = {}) {
    QPlan best{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(0) ||
        IsHookRejectedState(target, allowStasis)) return best;
    if (player.Position().Distance2D(target.Position()) >
        kQMaximumTargetCenterRange + target.BoundingRadius() + 20.0f) {
        return best;
    }

    SDK::HitChance observed = allowStasis
        ? SDK::HitChance::Immobile : SDK::HitChance::High;
    if (!allowStasis && Engine::RuntimeSpells[0]) {
        observed = Engine::RuntimeSpells[0]->GetPrediction(target).Hitchance;
    }
    if (static_cast<int>(observed) <
            static_cast<int>(RequiredQHitchance(purpose, target)) &&
        !target.IsDashing() && !Engine::IsHardCrowdControlled(target)) {
        return best;
    }

    const float raw = QRawDamage(SpellRank(0), player.AP());
    const float dealt = player.CalculateMagicDamage(target, raw);
    const bool lethal = dealt >= target.Health() + 3.0f;
    const int targetId = static_cast<int>(target.NetworkId());
    for (const Vector3& aim : HookCandidates(target, forcedPosition)) {
        if (!aim.IsValid() || aim.IsZero() ||
            ProjectileWallBlocks(player.Position(), aim, kQCollisionRadius)) {
            continue;
        }
        std::vector<HookBody> bodies = BuildHookBodies(
            allowStasis ? targetId : 0);
        if (forcedPosition.IsValid() && !forcedPosition.IsZero()) {
            for (auto& body : bodies) {
                if (body.Id == targetId) {
                    body.Position = forcedPosition;
                    body.Velocity = {};
                }
            }
        }
        HookContact contact = FirstHookContact(
            player.Position(), aim, bodies);
        if (!contact.Hit || contact.BodyId != targetId) continue;

        LastQPlan.CastPosition = aim;
        HookContext context = RuntimeHookContext(
            target, purpose, observed, contact, lethal,
            selectedTargetId == targetId, allowStasis);
        context.ProjectileWallBlocked = false;
        HookEvaluation evaluation = EvaluateHook(context);
        if (!evaluation.Cast) continue;
        float score = evaluation.Score;
        if (contact.Kind == HookContactKind::EndpointLollipop) score -= 35.0f;
        if (purpose == HookPurpose::ManualCursor) score += 130.0f;
        if (!best.Valid || score > best.Evaluation.Score) {
            evaluation.Score = score;
            best.CastPosition = aim;
            best.FirstContact = contact;
            best.Evaluation = evaluation;
            best.TargetId = targetId;
            best.Purpose = purpose;
            best.Hitchance = observed;
            best.RawDamage = raw;
            best.DealtDamage = dealt;
            best.ExpectedArrivalTick = Now() + static_cast<int>(
                contact.CastElapsedSeconds * 1000.0f);
            best.Lethal = lethal;
            best.DangerousDelivery = IsDangerousDelivery(
                ClassifyPullArchetype(target)) &&
                context.PullsOntoProtectedCarry &&
                !context.TargetKeyCooldownsSpent;
            best.Valid = true;
        }
    }
    LastQPlan = best;
    return best;
}

inline bool CastQPlan(const QPlan& plan, bool reactive = false) {
    if (!plan.Valid || !Ready(0) ||
        !ControllerHelpers::CastThrottleReady(
            0, 40, reactive ? 0 : -1)) return false;
    if (!Engine::ControllerCastPosition(0, plan.CastPosition)) return false;
    LastQPlan = plan;
    LastQCastTick = Now();
    QInFlight = true;
    QFlightTargetId = plan.TargetId;
    QExpectedArrivalTick = plan.ExpectedArrivalTick;
    QFlightEnd = plan.CastPosition;
    ActiveSequence = plan.Purpose == HookPurpose::StasisExit
        ? Sequence::StasisExit : Sequence::HookFlight;
    SequenceExpireTick = QExpectedArrivalTick + kHookArrivalGraceMs;
    return true;
}

inline ManaCosts LiveManaCosts() {
    ManaCosts result{};
    result.Q = SpellCost(0);
    result.W = SpellCost(1);
    result.E = SpellCost(2);
    result.R = SpellCost(3);
    return result;
}

inline float PeelManaReserve() {
    if (!Bool(RoleMenu, "ReservePeelMana", true)) return 0.0f;
    return Ready(2) ? SpellCost(2) : 0.0f;
}

inline WPlan BuildWPlan(const AIHeroClient& target,
                        WPurpose purpose) {
    WPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(1)) return plan;
    Vector3 destination = player.PathEnd();
    if (!destination.IsValid() || destination.IsZero() ||
        player.Position().Distance2D(destination) < 35.0f) {
        destination = player.Position();
    }
    WContext context{};
    context.Ready = true;
    context.HasMana = CanAffordSequence(
        player.Mana(), LiveManaCosts(), ManaSequence::PeelE,
        SpellCost(1), true);
    // W is the spell being paid now; retain E unless this is survival.
    context.HasMana = player.Mana() + 0.5f >=
        SpellCost(1) +
        ((purpose == WPurpose::Flee || purpose == WPurpose::Peel)
            ? 0.0f : PeelManaReserve());
    context.PlayerAttackWindingUp = Orbwalker::IsWindingUp();
    context.IncomingLethal = player.HealthPercent() <=
        Slider(TacticsMenu, "EmergencyHp", 24);
    context.PathSafe = !SDK::NavMesh::IsWall(destination);
    context.DestinationUnderEnemyTurret = Engine::UnderEnemyTurret(destination);
    context.PointClickThreatAtDestination =
        HasReadyPointClickThreatAt(destination);
    context.TargetValid = Engine::ValidEnemy(target, 1000.0f);
    context.TargetEscaping = context.TargetValid &&
        target.PathEnd().IsValid() && !target.PathEnd().IsZero() &&
        player.Position().Distance2D(target.PathEnd()) >
            player.Position().Distance2D(target.Position()) + 45.0f;
    context.TargetCanDisengage = context.TargetValid &&
        EnemyMobilityReady(target);
    context.EReady = Ready(2) || EArmed;
    context.EWillBeInRange = context.TargetValid &&
        player.Position().Distance2D(target.Position()) <=
            WEReliableReach(target);
    context.HookLanded = context.TargetValid &&
        (ActiveSequence == Sequence::HookArrival ||
         (QFlightTargetId == static_cast<int>(target.NetworkId()) &&
          std::abs(Now() - QExpectedArrivalTick) <= 420));
    context.HookReady = Ready(0);
    context.BetterHookAngleCreated = context.TargetValid &&
        CursorDirectionAgrees(target.Position(), 0.10f) &&
        player.Position().Distance2D(destination) >= 100.0f;
    context.AllyNeedsPeel = purpose == WPurpose::Peel;
    context.FutureSelfSlowUnsafe = Engine::CountEnemiesAt(
        destination, 700.0f) > Engine::CountAlliesAt(destination, 700.0f) ||
        (context.TargetValid && EnemyMobilityReady(target) &&
         !context.HookLanded);
    context.CurrentSelfSlow = WSelfSlowExpireTick >= Now();
    context.CursorAgrees = CursorDirectionAgrees(destination, -0.05f);
    context.TargetDistance = context.TargetValid
        ? std::max(0.0f,
            player.Position().Distance2D(target.Position()) -
            player.AttackRange() - target.BoundingRadius() -
            kEBonusAttackRange)
        : 0.0f;
    context.DistanceClosedBeforeDecay = WBonusTravelDistance(
        player.MoveSpeed(), SpellRank(1), 1.35f);
    context.EnemiesAtDestination = Engine::CountEnemiesAt(
        destination, 650.0f);
    context.AlliesAtDestination = Engine::CountAlliesAt(
        destination, 700.0f);
    context.Purpose = purpose;
    plan.Evaluation = EvaluateW(context);
    plan.TargetId = target.IsValid()
        ? static_cast<int>(target.NetworkId()) : 0;
    plan.Purpose = purpose;
    plan.Valid = plan.Evaluation.Cast;
    return plan;
}

inline bool CastWPlan(const WPlan& plan, bool reactive = false) {
    if (!plan.Valid || !Ready(1) ||
        !ControllerHelpers::CastThrottleReady(
            1, 46, reactive ? 0 : -1)) return false;
    if (!Engine::ControllerCastSelf(1)) return false;
    LastWPlan = plan;
    LastWCastTick = Now();
    WActive = true;
    WExpireTick = Now() + static_cast<int>(kWDurationSeconds * 1000.0f);
    WSelfSlowExpireTick = WExpireTick +
        static_cast<int>(kWSelfSlowSeconds * 1000.0f);
    if (plan.Purpose == WPurpose::WalkUpE) {
        ActiveSequence = Sequence::WalkUpE;
        SequenceExpireTick = Now() + 2300;
    }
    return true;
}

inline float BasicAttackDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const float damage = SDK::Damage::GetAutoAttackDamage(
        player, target, true);
    return std::isfinite(damage) ? std::max(0.0f, damage) : 0.0f;
}

inline float PowerFistDamage(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    const float raw = EEmpoweredAttackRawDamage(
        player.TotalAttackDamage(), player.AP());
    return player.CalculatePhysicalDamage(target, raw);
}

inline EPlan BuildEPlan(const AIHeroClient& target,
                        bool peelUrgent = false) {
    EPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(2) || EArmed ||
        !Engine::ValidEnemy(target, 700.0f)) return plan;
    const int targetId = static_cast<int>(target.NetworkId());
    const bool inRange = InAutoAttackRange(target, kEBonusAttackRange);
    EContext context{};
    context.Ready = true;
    context.TargetValid = true;
    context.TargetInEmpoweredAttackRange = inRange;
    context.ExactAttackTarget = LastBeforeAttackTargetId == targetId ||
        LastAfterAttackTargetId == targetId ||
        Engine::LockedTargetNetworkId == targetId ||
        QFlightTargetId == targetId;
    context.AttackJustCompleted = LastAfterAttackTargetId == targetId &&
        Now() - LastAfterAttackTick <= 260;
    context.PlayerAttackWindingUp = Orbwalker::IsWindingUp();
    context.QInFlightToTarget = QInFlight && QFlightTargetId == targetId;
    context.HookWillLand = context.QInFlightToTarget &&
        QExpectedArrivalTick >= Now() - 100;
    context.TargetCanInstantEscape = EnemyMobilityReady(target) &&
        !EnemyWindowEscapeSpent(targetId);
    context.TargetEscapeSpent = EnemyWindowEscapeSpent(targetId);
    context.EscapeHasInterruptibleStartup = EnemyEscapeCastStarted(targetId);
    context.EscapeCastStarted = EnemyEscapeCastStarted(targetId);
    context.TargetCannotEscape = Engine::IsHardCrowdControlled(target) ||
        context.TargetEscapeSpent || TargetInStasis(target);
    context.TargetChanneling = targetId == InterruptTargetId &&
        InterruptExpireTick >= Now();
    context.TargetSpellShield = HasSpellShieldOrImmunity(target);
    context.AttackBlockedByZone = AttackBlockedByZone(target);
    context.BlindOrDodge = SDK::HasBuffOfType(
        player, SDK::BuffType::Blind) ||
        ControllerHelpers::HasAnyBuff(target, {
            "JaxCounterStrike", "NilahW", "FioraW",
        });
    const float attack = BasicAttackDamage(target);
    const float empowered = PowerFistDamage(target);
    context.TargetKillableByNormalAttack =
        attack >= target.Health() + target.PhysicalShield();
    context.TargetKillableByEmpoweredAttack =
        empowered >= target.Health() + target.PhysicalShield();
    context.PeelUrgent = peelUrgent;
    context.WrongUnitAttackPending =
        LastBeforeAttackTargetId != 0 &&
        LastBeforeAttackTargetId != targetId &&
        Now() - LastBeforeAttackTick <= 220;
    context.HookArrivalSeconds = context.QInFlightToTarget
        ? std::max(0, QExpectedArrivalTick - Now()) / 1000.0f
        : FLT_MAX;
    context.EBuffRemainingSeconds = EArmed
        ? std::max(0, EExpireTick - Now()) / 1000.0f : 0.0f;
    plan.Decision = EvaluateE(context);
    plan.TargetId = targetId;
    plan.Valid = plan.Decision.ArmNow && !context.BlindOrDodge;
    return plan;
}

inline bool CastEPlan(const EPlan& plan, bool reactive = false) {
    if (!plan.Valid || !Ready(2) || EArmed ||
        !ControllerHelpers::CastThrottleReady(
            2, 30, reactive ||
                plan.Decision.Timing == ETiming::ResetAfterAttack ? 0 : -1)) {
        return false;
    }
    if (!Engine::ControllerCastSelf(2)) return false;
    LastEPlan = plan;
    LastECastTick = Now();
    EArmed = true;
    EExpireTick = Now() + static_cast<int>(kEDurationSeconds * 1000.0f);
    DesiredETargetId = plan.TargetId;
    ActiveSequence = plan.Decision.Timing == ETiming::ResetAfterAttack
        ? Sequence::AttackReset
        : (plan.Decision.Timing == ETiming::PeelNow
            ? Sequence::PeelChain : ActiveSequence);
    SequenceExpireTick = std::max(SequenceExpireTick, EExpireTick);
    return true;
}

inline bool CleanDirectHookLine(const AIHeroClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Engine::ValidEnemy(target,
            kQMaximumTargetCenterRange + 80.0f)) return false;
    Vector3 aim = PredictPosition(
        target, kQCastSeconds +
            player.Position().Distance2D(target.Position()) /
                kQMissileSpeed);
    if (!aim.IsValid() || aim.IsZero() ||
        ProjectileWallBlocks(player.Position(), aim, kQCollisionRadius)) {
        return false;
    }
    return HookHitsIntendedFirst(
        player.Position(), aim, BuildHookBodies(),
        static_cast<int>(target.NetworkId()));
}

inline RPlan BuildRPlan(const AIHeroClient& preferred,
                        RPurpose purpose) {
    RPlan plan{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !Ready(3)) return plan;
    AIHeroClient primary = Engine::ValidEnemy(preferred,
        kRRadius + preferred.BoundingRadius()) ? preferred : AIHeroClient{};
    int hitCount = 0;
    int priorityCount = 0;
    float shields = 0.0f;
    float bestPriority = -FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            player.Position().Distance2D(enemy.Position()) >
                kRRadius + enemy.BoundingRadius()) continue;
        ++hitCount;
        const float priority = EnemyPriority(enemy);
        if (priority >= 2.0f) ++priorityCount;
        shields += TotalDamageShields(enemy);
        if (!primary.IsValid() || priority > bestPriority) {
            primary = enemy;
            bestPriority = priority;
        }
    }
    if (!primary.IsValid() || hitCount <= 0) return plan;
    const int targetId = static_cast<int>(primary.NetworkId());
    const float rawActive = RActiveRawDamage(SpellRank(3), player.AP());
    const float activeDamage = player.CalculateMagicDamage(primary, rawActive);
    const float passiveDamage = player.CalculateMagicDamage(
        primary, RPassiveRawDamage(
            SpellRank(3), player.AP(), player.MaxMana()));
    const int pending = MarkTracker.Pending(targetId);
    const float nextSeconds = MarkTracker.SecondsToNext(targetId, Now());
    RContext context{};
    context.Ready = true;
    context.HasMana = player.Mana() + 0.5f >= SpellCost(3);
    context.TargetInRange = true;
    context.TargetSpellShield = HasSpellShieldOrImmunity(primary);
    context.TargetHasDamageShield = TotalDamageShields(primary) > 1.0f;
    context.CriticalShieldBreak = context.TargetHasDamageShield &&
        (TotalDamageShields(primary) >=
             Slider(RMenu, "ShieldBreakValue", 260) ||
         purpose == RPurpose::ShieldBreak);
    context.ChannelInterruptUrgent =
        purpose == RPurpose::Interrupt &&
        targetId == InterruptTargetId && InterruptExpireTick >= Now();
    context.EscapeCastMustBeSilenced =
        (purpose == RPurpose::MidPullSilence ||
         purpose == RPurpose::PreHookSilence) &&
        EnemyMobilityReady(primary);
    context.HookInFlight = QInFlight && QFlightTargetId == targetId;
    context.HookWillLand = context.HookInFlight &&
        QExpectedArrivalTick >= Now() - 120;
    context.QReady = Ready(0);
    context.QLineClear = Ready(0) && CleanDirectHookLine(primary);
    context.MobilePriorityTarget = EnemyMobilityReady(primary) &&
        EnemyPriority(primary) >= 1.8f;
    context.PeelUrgent = purpose == RPurpose::Peel;
    context.ActiveDamageLethal = !context.TargetSpellShield &&
        activeDamage >= primary.Health() + 2.0f;
    context.PendingPassiveLethal = pending > 0 &&
        passiveDamage >= primary.Health() + primary.MagicalShield();
    context.PendingTickSoon = nextSeconds <= 0.45f;
    context.PlayerAttackWindingUp = Orbwalker::IsWindingUp();
    context.HookArrivalSeconds = context.HookInFlight
        ? std::max(0, QExpectedArrivalTick - Now()) / 1000.0f : FLT_MAX;
    context.TotalShields = shields;
    context.ActiveDamage = activeDamage;
    const int expectedPassiveAttacks = std::clamp(
        Slider(RMenu, "ExpectedPassiveAttacks", 3), 0, 8);
    context.PassiveOpportunityCost = RPassiveOpportunityCost(
        passiveDamage, expectedPassiveAttacks,
        WActive ? 0.78f : 0.58f);
    context.EnemyHitCount = hitCount;
    context.PriorityEnemyHitCount = priorityCount;
    context.PendingMarkStacks = pending;
    context.Purpose = purpose;
    if (purpose == RPurpose::Lethal && !context.ActiveDamageLethal) {
        return plan;
    }
    if (purpose == RPurpose::MultiTarget &&
        hitCount < Slider(RMenu, "MinimumTargets", 2)) {
        return plan;
    }
    plan.Evaluation = EvaluateR(context);
    plan.PrimaryTargetId = targetId;
    plan.Purpose = purpose;
    plan.HitCount = hitCount;
    plan.ShieldsDestroyed = shields;
    plan.Valid = plan.Evaluation.Cast;
    return plan;
}

inline bool CastRPlan(const RPlan& plan, bool reactive = false) {
    if (!plan.Valid || !Ready(3) ||
        !ControllerHelpers::CastThrottleReady(
            3, 38, reactive ? 0 : -1)) return false;
    if (!Engine::ControllerCastSelf(3)) return false;
    LastRPlan = plan;
    LastRCastTick = Now();
    if (plan.Purpose == RPurpose::PreHookSilence) {
        ActiveSequence = Sequence::PreHookRQ;
        SequenceExpireTick = Now() + 900;
    }
    return true;
}

inline bool TryStasisExitHook(int selectedTargetId) {
    if (!Ready(0) || !Bool(QMenu, "StasisExit", true)) return false;
    const auto player = GameObjects::Player();
    for (auto& record : StasisRecords) {
        if (record.NetworkId == 0 || record.EndTick <= Now() - 120) continue;
        AIHeroClient target = RawEnemyById(record.NetworkId);
        if (!target.IsValid() || target.IsDead()) continue;
        const float distance = player.Position().Distance2D(record.Position);
        if (distance > kQMaximumTargetCenterRange + target.BoundingRadius()) {
            continue;
        }
        const int impactMs = static_cast<int>((kQCastSeconds +
            std::max(0.0f, distance - target.BoundingRadius() -
                kQCollisionRadius) / kQMissileSpeed) * 1000.0f);
        const int earlyBias = Slider(QMenu, "StasisEarlyMs", 45);
        const int remaining = record.EndTick - Now();
        if (remaining > impactMs + 55 - earlyBias ||
            remaining < impactMs - 145 - earlyBias) continue;
        const QPlan plan = BuildQPlan(
            target, HookPurpose::StasisExit, selectedTargetId,
            true, record.Position);
        if (CastQPlan(plan, true)) {
            record = {};
            return true;
        }
    }
    return false;
}

inline bool TryInterrupt() {
    if (InterruptTargetId == 0 || InterruptExpireTick < Now()) return false;
    const AIHeroClient target = RawEnemyById(InterruptTargetId);
    if (!Engine::ValidEnemy(target, 1250.0f)) return false;
    const auto player = GameObjects::Player();
    if (Bool(RMenu, "Interrupt", true) && Ready(3) &&
        player.Position().Distance2D(target.Position()) <=
            kRRadius + target.BoundingRadius()) {
        const RPlan r = BuildRPlan(target, RPurpose::Interrupt);
        if (CastRPlan(r, true)) return true;
    }
    if (Bool(EMenu, "Interrupt", true) && Ready(2) &&
        InAutoAttackRange(target, kEBonusAttackRange)) {
        const EPlan e = BuildEPlan(target, true);
        if (CastEPlan(e, true)) return true;
    }
    if (Bool(QMenu, "Interrupt", true) && Ready(0)) {
        const QPlan q = BuildQPlan(
            target, HookPurpose::Interrupt, InterruptTargetId);
        if (CastQPlan(q, true)) return true;
    }
    return false;
}

inline bool TryGapcloser() {
    if (GapcloserTargetId == 0 || GapcloserExpireTick < Now()) return false;
    const AIHeroClient target = RawEnemyById(GapcloserTargetId);
    if (!Engine::ValidEnemy(target, 1250.0f)) return false;
    if (Bool(EMenu, "AntiGapcloser", true) && Ready(2) &&
        InAutoAttackRange(target, kEBonusAttackRange)) {
        const EPlan e = BuildEPlan(target, true);
        if (CastEPlan(e, true)) return true;
    }
    if (Bool(QMenu, "AntiGapcloser", true) && Ready(0)) {
        const Vector3 endpoint = GapcloserEndpoint.IsValid() &&
                !GapcloserEndpoint.IsZero()
            ? GapcloserEndpoint : target.PathEnd();
        const QPlan q = BuildQPlan(
            target, HookPurpose::DashEndpoint,
            GapcloserTargetId, false, endpoint);
        if (CastQPlan(q, true)) return true;
    }
    if (Bool(RMenu, "Peel", true) && Ready(3) &&
        Engine::CountEnemiesAt(GameObjects::Player().Position(),
                               kRRadius) >= 2) {
        const RPlan r = BuildRPlan(target, RPurpose::Peel);
        if (CastRPlan(r, true)) return true;
    }
    return false;
}

inline bool TryPeel(const AIHeroClient& ally,
                    const AIHeroClient& threat) {
    if (!Bool(RoleMenu, "ProtectCarry", true) ||
        !Engine::ValidAlly(ally) || !Engine::ValidEnemy(threat, 1250.0f)) {
        return false;
    }
    if (Ready(2) && Bool(EMenu, "Peel", true) &&
        InAutoAttackRange(threat, kEBonusAttackRange)) {
        const EPlan e = BuildEPlan(threat, true);
        if (CastEPlan(e, true)) return true;
    }
    if (Ready(0) && Bool(QMenu, "Peel", true)) {
        const QPlan q = BuildQPlan(
            threat, HookPurpose::Peel,
            static_cast<int>(threat.NetworkId()));
        if (CastQPlan(q, true)) return true;
    }
    if (Ready(3) && Bool(RMenu, "Peel", true) &&
        threat.Position().Distance2D(GameObjects::Player().Position()) <=
            kRRadius + threat.BoundingRadius()) {
        const RPlan r = BuildRPlan(threat, RPurpose::Peel);
        if (CastRPlan(r, true)) return true;
    }
    if (Ready(1) && Bool(WMenu, "Peel", true)) {
        const WPlan w = BuildWPlan(threat, WPurpose::Peel);
        if (CastWPlan(w, true)) return true;
    }
    return false;
}

inline bool TryAutomaticShieldBreak() {
    if (!Ready(3) || !Bool(RMenu, "ShieldBreak", true)) return false;
    AIHeroClient best{};
    float bestShield = static_cast<float>(
        Slider(RMenu, "ShieldBreakValue", 260));
    const auto player = GameObjects::Player();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            player.Position().Distance2D(enemy.Position()) >
                kRRadius + enemy.BoundingRadius()) continue;
        const float shield = TotalDamageShields(enemy);
        if (shield > bestShield) {
            best = enemy;
            bestShield = shield;
        }
    }
    if (!best.IsValid()) return false;
    return CastRPlan(BuildRPlan(best, RPurpose::ShieldBreak), true);
}

inline bool TryHookFlightFollowups(bool playerLed = false) {
    if (!QInFlight || QFlightTargetId == 0 ||
        QExpectedArrivalTick + kHookArrivalGraceMs < Now()) return false;
    const AIHeroClient target = RawEnemyById(QFlightTargetId);
    if (!target.IsValid()) return false;
    if (playerLed && !Bool(EMenu, "AssistManualHook", true)) return false;
    const bool escapeReady = EnemyMobilityReady(target) &&
        !EnemyWindowEscapeSpent(QFlightTargetId);
    if (Ready(3) && Bool(RMenu, "MidPullSilence", true) && escapeReady &&
        GameObjects::Player().Position().Distance2D(target.Position()) <=
            kRRadius + target.BoundingRadius()) {
        const RPlan r = BuildRPlan(target, RPurpose::MidPullSilence);
        if (CastRPlan(r, true)) return true;
    }
    if (Ready(2) && !EArmed && Bool(EMenu, "PreArmHook", true)) {
        const EPlan e = BuildEPlan(target, false);
        if (CastEPlan(e, true)) return true;
    }
    if (Ready(1) && Bool(WMenu, "AfterHook", true) &&
        Now() >= QExpectedArrivalTick - 80) {
        const WPlan w = BuildWPlan(target, WPurpose::PostHook);
        if (CastWPlan(w, true)) return true;
    }
    return false;
}

inline bool TryKillSecure(const AIHeroClient& preferred,
                          int selectedTargetId) {
    if (!Bool(TacticsMenu, "KillSecure", true)) return false;
    const auto player = GameObjects::Player();
    AIHeroClient best = preferred;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1250.0f)) continue;
        if (!best.IsValid() || enemy.Health() < best.Health()) best = enemy;
    }
    if (!best.IsValid()) return false;
    if (Ready(3) && player.Position().Distance2D(best.Position()) <=
            kRRadius + best.BoundingRadius()) {
        const RPlan r = BuildRPlan(best, RPurpose::Lethal);
        if (r.Valid && CastRPlan(r, true)) return true;
    }
    if (Ready(2) && InAutoAttackRange(best, kEBonusAttackRange) &&
        PowerFistDamage(best) >= best.Health() + best.PhysicalShield()) {
        const EPlan e = BuildEPlan(best, false);
        if (CastEPlan(e, true)) return true;
    }
    if (Ready(0)) {
        const QPlan q = BuildQPlan(
            best, HookPurpose::Kill, selectedTargetId);
        if (q.Valid && q.Lethal && CastQPlan(q, true)) return true;
    }
    return false;
}

inline bool TryObjectiveHook(int selectedTargetId) {
    if (!Ready(0) || !Bool(QMenu, "ObjectiveJungler", true) ||
        !HasNearbyEpicMonster(1800.0f)) return false;
    QPlan best{};
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, 1250.0f) || !HeroHasSmite(enemy)) {
            continue;
        }
        const QPlan candidate = BuildQPlan(
            enemy, HookPurpose::ObjectiveJungler, selectedTargetId);
        if (candidate.Valid &&
            (!best.Valid || candidate.Evaluation.Score >
                                best.Evaluation.Score)) {
            best = candidate;
        }
    }
    LastQPlan = best;
    return CastQPlan(best, true);
}

inline HookPurpose OrdinaryHookPurpose(const AIHeroClient& target,
                                       int selectedTargetId) {
    const int id = static_cast<int>(target.NetworkId());
    if (HeroHasSmite(target) && HasNearbyEpicMonster(1800.0f)) {
        return HookPurpose::ObjectiveJungler;
    }
    if (Engine::IsHardCrowdControlled(target)) {
        return HookPurpose::ImmobilePunish;
    }
    if (target.IsDashing()) return HookPurpose::DashEndpoint;
    if (id == selectedTargetId) return HookPurpose::SelectedPick;
    return HookPurpose::ManualCursor;
}

inline bool TryBestHook(const AIHeroClient& preferred,
                        int selectedTargetId,
                        bool harass = false) {
    if (!Ready(0) || (harass && PlayerManaPercent() <
            Slider(QMenu, "HarassMana", 58))) return false;
    QPlan best{};
    const auto consider = [&](const AIHeroClient& enemy) {
        if (!Engine::ValidEnemy(enemy, 1250.0f)) return;
        const HookPurpose purpose = OrdinaryHookPurpose(
            enemy, selectedTargetId);
        const QPlan candidate = BuildQPlan(
            enemy, purpose, selectedTargetId);
        if (!candidate.Valid) return;
        float score = candidate.Evaluation.Score;
        if (preferred.IsValid() &&
            enemy.NetworkId() == preferred.NetworkId()) score += 75.0f;
        if (!best.Valid || score > best.Evaluation.Score) {
            best = candidate;
            best.Evaluation.Score = score;
        }
    };
    if (preferred.IsValid()) consider(preferred);
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!preferred.IsValid() || enemy.NetworkId() != preferred.NetworkId()) {
            consider(enemy);
        }
    }
    LastQPlan = best;
    return CastQPlan(best);
}

inline bool TryPreHookRQ(const AIHeroClient& target) {
    if (!Bool(RMenu, "PreHookSilence", true) || !Ready(3) || !Ready(0) ||
        !Engine::ValidEnemy(target) || !EnemyMobilityReady(target) ||
        EnemyPriority(target) < 1.8f ||
        GameObjects::Player().Position().Distance2D(target.Position()) >
            kRRadius + target.BoundingRadius() ||
        !CleanDirectHookLine(target)) return false;
    return CastRPlan(BuildRPlan(target, RPurpose::PreHookSilence));
}

inline bool TryCombo(const AIHeroClient& target,
                     int selectedTargetId) {
    if (TryHookFlightFollowups()) return true;
    if (ActiveSequence == Sequence::PreHookRQ &&
        LastRPlan.PrimaryTargetId != 0 && SequenceExpireTick >= Now()) {
        const AIHeroClient silenced = RawEnemyById(
            LastRPlan.PrimaryTargetId);
        const QPlan q = BuildQPlan(
            silenced, HookPurpose::SelectedPick,
            LastRPlan.PrimaryTargetId);
        if (CastQPlan(q, true)) return true;
    }
    if (!Engine::ValidEnemy(target, 1250.0f)) return false;
    if (Ready(2) && InAutoAttackRange(target, kEBonusAttackRange)) {
        const EPlan e = BuildEPlan(target, false);
        if (CastEPlan(e)) return true;
    }
    if (WalkUpEReliable(target) && Ready(1) &&
        Bool(WMenu, "WalkUpE", true)) {
        const WPlan w = BuildWPlan(target, WPurpose::WalkUpE);
        if (CastWPlan(w)) return true;
    }
    if (TryPreHookRQ(target)) return true;
    if (SpellEnabled(0, Mode::Combo) && TryBestHook(
            target, selectedTargetId, false)) return true;
    if (Ready(3) && SpellEnabled(3, Mode::Combo)) {
        RPurpose purpose = Engine::CountEnemiesAt(
            GameObjects::Player().Position(), kRRadius) >=
                Slider(RMenu, "MinimumTargets", 2)
            ? RPurpose::MultiTarget : RPurpose::None;
        const RPlan r = BuildRPlan(target, purpose);
        if (CastRPlan(r)) return true;
    }
    if (Ready(1) && Bool(WMenu, "HookAngle", true)) {
        const WPlan w = BuildWPlan(target, WPurpose::HookAngle);
        if (CastWPlan(w)) return true;
    }
    return false;
}

inline bool TryHarass(const AIHeroClient& target,
                      int selectedTargetId) {
    if (!Engine::ValidEnemy(target, 1250.0f)) return false;
    if (PlayerManaPercent() < Slider(QMenu, "HarassMana", 58)) return false;
    if (Ready(2) && InAutoAttackRange(target, kEBonusAttackRange)) {
        const EPlan e = BuildEPlan(target, false);
        if (CastEPlan(e)) return true;
    }
    if (Bool(WMenu, "HarassWalkUp", false) && WalkUpEReliable(target)) {
        const WPlan w = BuildWPlan(target, WPurpose::WalkUpE);
        if (CastWPlan(w)) return true;
    }
    return SpellEnabled(0, Mode::Harass) &&
        TryBestHook(target, selectedTargetId, true);
}

inline bool TryFlee(const AIHeroClient& fallback) {
    const AIHeroClient threat = PreferredEnemy(fallback, 1250.0f);
    if (Engine::ValidEnemy(threat)) {
        if (Ready(2) && InAutoAttackRange(threat, kEBonusAttackRange)) {
            const EPlan e = BuildEPlan(threat, true);
            if (CastEPlan(e, true)) return true;
        }
        if (Ready(0) && Bool(QMenu, "Flee", true)) {
            const QPlan q = BuildQPlan(
                threat, HookPurpose::Peel,
                static_cast<int>(threat.NetworkId()));
            if (CastQPlan(q, true)) return true;
        }
    }
    if (Ready(1) && Bool(WMenu, "Flee", true)) {
        const WPlan w = BuildWPlan(threat, WPurpose::Flee);
        if (CastWPlan(w, true)) return true;
    }
    if (Ready(3) && Bool(RMenu, "Flee", true) &&
        Engine::CountEnemiesAt(GameObjects::Player().Position(),
                               kRRadius) >=
            Slider(RMenu, "FleeMinimum", 2)) {
        const RPlan r = BuildRPlan(threat, RPurpose::Peel);
        if (CastRPlan(r, true)) return true;
    }
    return false;
}

inline bool TryRoamW() {
    if (!Ready(1) || !Bool(WMenu, "Roam", false) ||
        HasEnemyChampionNear(1400.0f) || HasNearbyEpicMonster(2000.0f)) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (!player.PathEnd().IsValid() || player.PathEnd().IsZero() ||
        player.Position().Distance2D(player.PathEnd()) < 700.0f) return false;
    const WPlan w = BuildWPlan({}, WPurpose::Roam);
    return CastWPlan(w);
}

inline void RefreshRuntimeState() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    const int now = Now();
    WActive = ControllerHelpers::HasAnyBuff(player, {
        "Overdrive", "BlitzcrankW", "overdrive" }) ||
        (WActive && now <= WExpireTick);
    const bool selfSlow = ControllerHelpers::HasAnyBuff(player, {
        "PowerFistSlow", "OverdriveSlow", "BlitzcrankWSlow" });
    if (selfSlow) WSelfSlowExpireTick = std::max(
        WSelfSlowExpireTick,
        now + static_cast<int>(kWSelfSlowSeconds * 1000.0f));
    EArmed = ControllerHelpers::HasAnyBuff(player, {
        "PowerFist", "PowerFistAttack", "BlitzcrankE" }) ||
        (EArmed && now <= EExpireTick);
    if (!EArmed && now > EExpireTick) DesiredETargetId = 0;
    if (WActive && now > WExpireTick + 200) WActive = false;
    if (QInFlight && now >= QExpectedArrivalTick) {
        if (ActiveSequence == Sequence::HookFlight ||
            ActiveSequence == Sequence::PlayerLedHook) {
            ActiveSequence = Sequence::HookArrival;
            SequenceExpireTick = QExpectedArrivalTick + kHookArrivalGraceMs;
        }
    }
    if (QInFlight && now > QExpectedArrivalTick + kHookArrivalGraceMs) {
        QInFlight = false;
        QFlightTargetId = 0;
        QExpectedArrivalTick = 0;
        QMissileId = 0;
    }
    if (ActiveSequence != Sequence::None && now > SequenceExpireTick) {
        ActiveSequence = Sequence::None;
    }
    if (GapcloserExpireTick < now) GapcloserTargetId = 0;
    if (InterruptExpireTick < now) InterruptTargetId = 0;
    if (AllyThreatUntil < now) {
        ThreatenedAllyId = 0;
        PeelThreatId = 0;
    }
    for (auto& record : StasisRecords) {
        if (record.NetworkId != 0 && record.EndTick + 800 < now) record = {};
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid()) continue;
        (void)MarkTracker.Advance(
            static_cast<int>(enemy.NetworkId()), now);
    }
    MarkTracker.ClearExpired(now);
    const AIHeroClient protectedAlly = SelectProtectionAlly(
        1800.0f, ThreatenedAllyId, AllyThreatUntil);
    ProtectedAllyId = protectedAlly.IsValid()
        ? static_cast<int>(protectedAlly.NetworkId()) : 0;
}

inline Posture DeterminePosture(Mode mode,
                                const AIHeroClient& target,
                                const AIHeroClient& ally,
                                const AIHeroClient& threat) {
    if (mode == Mode::Flee) return Posture::Disengage;
    if (Engine::ValidAlly(ally) && Engine::ValidEnemy(threat) &&
        Bool(RoleMenu, "ProtectCarry", true)) return Posture::Peel;
    if (HasNearbyEpicMonster(1800.0f)) return Posture::Objective;
    if (mode == Mode::LaneClear || mode == Mode::LastHit ||
        mode == Mode::Jungle) return Posture::Farm;
    if (Engine::ValidEnemy(target)) {
        if (WalkUpEReliable(target)) return Posture::WalkUp;
        if (mode == Mode::Combo) return Posture::Catch;
        return Posture::LaneThreat;
    }
    const auto player = GameObjects::Player();
    if (player.IsValid() && player.PathEnd().IsValid() &&
        !player.PathEnd().IsZero() &&
        player.Position().Distance2D(player.PathEnd()) > 650.0f) {
        return Posture::RoamVision;
    }
    return Posture::Neutral;
}

inline bool OnUpdate(Mode mode, const AIHeroClient& selected) {
    RefreshRuntimeState();
    const int selectedId = Engine::ValidEnemy(selected)
        ? static_cast<int>(selected.NetworkId()) : 0;
    const AIHeroClient target = PreferredEnemy(selected, 1300.0f);
    const AIHeroClient ally = ProtectedAlly();
    const AIHeroClient threat = SelectPeelThreat(ally);
    if (threat.IsValid()) PeelThreatId = static_cast<int>(threat.NetworkId());
    CurrentPosture = DeterminePosture(mode, target, ally, threat);

    if (PlayerOverrideUntil >= Now()) {
        if (ActiveSequence == Sequence::PlayerLedHook &&
            Now() - LastQCastTick >= kManualHookAssistDelayMs) {
            return TryHookFlightFollowups(true);
        }
        return false;
    }
    if (TryStasisExitHook(selectedId)) return true;
    if (TryInterrupt()) return true;
    if (TryGapcloser()) return true;
    if (TryPeel(ally, threat)) return true;
    if (TryAutomaticShieldBreak()) return true;
    if (TryObjectiveHook(selectedId)) return true;
    if (TryKillSecure(target, selectedId)) return true;
    if (mode == Mode::Flee) return TryFlee(target);
    if (mode == Mode::Combo) return TryCombo(target, selectedId);
    if (mode == Mode::Harass) return TryHarass(target, selectedId);
    if (mode == Mode::None || mode == Mode::Automatic) {
        return TryRoamW();
    }
    // Blitz abilities are deliberately excluded from lane/jungle farming.
    return false;
}

inline int EventTargetId(
    const SDK::Events::ProcessSpellEventArgs& args) {
    return static_cast<int>(args.TargetNetworkId != 0
        ? args.TargetNetworkId : args.Target.NetworkId);
}

inline void ObserveEnemySpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    const auto analysis = AnalyzeEnemyCast(args, 220.0f, 120.0f);
    if (!analysis.Valid) return;
    const AIHeroClient enemy = analysis.Enemy;
    const int enemyId = static_cast<int>(enemy.NetworkId());
    EnemyWindow* window = WindowFor(enemyId, true);
    if (window) {
        window->LastSpellTick = Now();
        if (analysis.Committed) {
            window->CommittedUntil = std::max(
                window->CommittedUntil,
                analysis.CommitmentUntilTick);
        }
        if (analysis.LikelyHardCrowdControl ||
            ControllerHelpers::LikelyHardCrowdControlSpell(args)) {
            window->HardCrowdControlSpentUntil = std::max(
                window->HardCrowdControlSpentUntil, Now() + 1800);
        }
        if (EnemyMobilitySpell(enemy, args.Slot) || enemy.IsDashing()) {
            window->EscapeSpentUntil = std::max(
                window->EscapeSpentUntil, Now() + 1650);
            window->EscapeCastUntil = std::max(
                window->EscapeCastUntil, Now() + 420);
        }
    }

    const int targetId = EventTargetId(args);
    const AIHeroClient ally = RawAllyById(targetId);
    if (ally.IsValid()) {
        ThreatenedAllyId = targetId;
        PeelThreatId = enemyId;
        AllyThreatUntil = Now() + (args.IsAutoAttack ? 850 : 1300);
    } else if (analysis.CrossesPlayer || analysis.TargetsPlayer) {
        ThreatenedAllyId = static_cast<int>(
            GameObjects::Player().NetworkId());
        PeelThreatId = enemyId;
        AllyThreatUntil = Now() + 900;
    }
}

inline void BeginObservedHook(
    const SDK::Events::ProcessSpellEventArgs& args,
    bool controllerOwned) {
    const auto player = GameObjects::Player();
    Vector3 aim = args.EndPosition;
    if (!aim.IsValid() || aim.IsZero()) aim = args.CastPosition;
    if (!aim.IsValid() || aim.IsZero()) aim = QFlightEnd;
    HookContact contact{};
    int targetId = 0;
    if (controllerOwned && LastQPlan.Valid &&
        Now() - LastQCastTick <= 700) {
        contact = LastQPlan.FirstContact;
        targetId = LastQPlan.TargetId;
    } else if (aim.IsValid() && !aim.IsZero()) {
        contact = FirstHookContact(
            player.Position(), aim, BuildHookBodies());
        targetId = contact.Hit ? contact.BodyId : 0;
    }
    LastQCastTick = Now();
    QInFlight = true;
    QFlightTargetId = targetId;
    QFlightEnd = aim;
    QExpectedArrivalTick = Now() + static_cast<int>(
        (contact.Hit ? contact.CastElapsedSeconds :
            (kQCastSeconds + kQMissileSegment / kQMissileSpeed)) *
        1000.0f);
    SequenceExpireTick = QExpectedArrivalTick + kHookArrivalGraceMs;
    if (!controllerOwned) {
        PlayerOverrideUntil = Now() +
            Slider(TacticsMenu, "ManualOwnershipMs", kManualOwnershipMs);
        ActiveSequence = Bool(EMenu, "AssistManualHook", true) &&
                targetId != 0
            ? Sequence::PlayerLedHook : Sequence::None;
    } else {
        ActiveSequence = LastQPlan.Purpose == HookPurpose::StasisExit
            ? Sequence::StasisExit : Sequence::HookFlight;
    }
}

inline void ObserveLocalSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!IsLocalPlayer(args.Sender)) return;
    const int now = Now();
    if (args.IsAutoAttack) {
        (void)CaptureLocalAutoAttack(
            args, LastLocalAutoTargetId, LastLocalAutoTick);
        return;
    }
    int slot = -1;
    if (IsQEvent(args)) slot = 0;
    else if (IsWEvent(args)) slot = 1;
    else if (IsEEvent(args)) slot = 2;
    else if (IsREvent(args)) slot = 3;
    if (slot < 0) return;
    const bool controllerOwned = Engine::WasControllerCast(slot);
    if (slot == 0) {
        BeginObservedHook(args, controllerOwned);
        return;
    }
    if (slot == 1) {
        LastWCastTick = now;
        WActive = true;
        WExpireTick = now + static_cast<int>(kWDurationSeconds * 1000.0f);
        WSelfSlowExpireTick = WExpireTick +
            static_cast<int>(kWSelfSlowSeconds * 1000.0f);
    } else if (slot == 2) {
        LastECastTick = now;
        EArmed = true;
        EExpireTick = now + static_cast<int>(kEDurationSeconds * 1000.0f);
        if (!controllerOwned) {
            const int explicitTarget = EventTargetId(args);
            DesiredETargetId = explicitTarget != 0
                ? explicitTarget : LastBeforeAttackTargetId;
        }
    } else if (slot == 3) {
        LastRCastTick = now;
    }
    if (!controllerOwned) {
        PlayerOverrideUntil = now +
            Slider(TacticsMenu, "ManualOwnershipMs", kManualOwnershipMs);
        ActiveSequence = Sequence::None;
    }
}

inline void OnProcessSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!args.Sender.IsValid()) return;
    if (IsLocalPlayer(args.Sender)) ObserveLocalSpell(args);
    else ObserveEnemySpell(args);
}

inline void OnDoCast(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (!CaptureLocalAutoAttack(
            args, LastLocalAutoTargetId, LastLocalAutoTick)) return;
}

inline void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
    if (!args.Target.IsValid()) return;
    LastBeforeAttackTargetId = static_cast<int>(args.Target.NetworkId());
    LastBeforeAttackTick = Now();
    if (!EArmed || DesiredETargetId == 0) return;
    const AIHeroClient desired = RawEnemyById(DesiredETargetId);
    const float arrival = QInFlight && QFlightTargetId == DesiredETargetId
        ? std::max(0, QExpectedArrivalTick - Now()) / 1000.0f
        : 0.0f;
    const float remaining = std::max(0, EExpireTick - Now()) / 1000.0f;
    if (ShouldBlockWrongAttackWhileEArmed(
            true, DesiredETargetId, LastBeforeAttackTargetId,
            desired.IsValid() &&
                GameObjects::Player().Position().Distance2D(
                    desired.Position()) <= 650.0f,
            arrival, remaining)) {
        args.Process = false;
        return;
    }
    if (LastBeforeAttackTargetId == DesiredETargetId &&
        desired.IsValid() && AttackBlockedByZone(desired) &&
        remaining > 0.55f) {
        args.Process = false;
    }
}

inline void OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
    if (!CaptureAfterAttack(
            args, LastAfterAttackTargetId, LastAfterAttackTick)) return;
    if (Ready(3)) {
        MarkTracker.RecordAttack(
            LastAfterAttackTargetId, LastAfterAttackTick);
    }
    if (EArmed && (DesiredETargetId == 0 ||
                   DesiredETargetId == LastAfterAttackTargetId)) {
        EArmed = false;
        EExpireTick = 0;
        DesiredETargetId = 0;
        if (ActiveSequence == Sequence::AttackReset ||
            ActiveSequence == Sequence::PeelChain) {
            ActiveSequence = Sequence::None;
        }
        return;
    }
    if (Ready(2) && !EArmed &&
        Bool(EMenu, "AAReset", true)) {
        const AIHeroClient target = RawEnemyById(
            LastAfterAttackTargetId);
        if (Engine::ValidEnemy(target, 500.0f)) {
            const EPlan e = BuildEPlan(target, false);
            (void)CastEPlan(e, true);
        }
    }
}

inline void OnGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    if (CaptureGapcloser(
            args, GapcloserTargetId, GapcloserEndpoint,
            GapcloserExpireTick, 1000.0f, 1250)) {
        EnemyWindow* window = WindowFor(GapcloserTargetId, true);
        if (window) {
            window->EscapeSpentUntil = Now() + 1600;
            window->EscapeCastUntil = Now() + 360;
        }
    }
}

inline bool BuffNameContains(
    const SDK::Events::BuffEventArgs& args,
    std::initializer_list<const char*> tokens) {
    return ControllerHelpers::TextContainsAny(args.BuffName, tokens);
}

inline void UpdateBuffState(
    const SDK::Events::BuffEventArgs& args,
    bool removed) {
    const int id = static_cast<int>(args.Sender.NetworkId);
    if (IsLocalPlayer(args.Sender)) {
        if (BuffNameContains(args, {
                "overdrive", "blitzcrankw" })) {
            WActive = !removed;
            if (!removed) {
                WExpireTick = ControllerHelpers::BuffExpireTick(
                    args, static_cast<int>(kWDurationSeconds * 1000.0f));
                WSelfSlowExpireTick = WExpireTick +
                    static_cast<int>(kWSelfSlowSeconds * 1000.0f);
            }
        }
        if (BuffNameContains(args, {
                "powerfist", "blitzcranke" })) {
            EArmed = !removed;
            if (!removed) {
                EExpireTick = ControllerHelpers::BuffExpireTick(
                    args, static_cast<int>(kEDurationSeconds * 1000.0f));
            } else {
                EExpireTick = 0;
                DesiredETargetId = 0;
            }
        }
        if (!removed && BuffNameContains(args, {
                "manabarrier", "manabarriericon" })) {
            LastManaBarrierProcTick = Now();
        }
        if (!removed && BuffNameContains(args, {
                "powerfistslow", "overdriveslow" })) {
            WSelfSlowExpireTick = ControllerHelpers::BuffExpireTick(
                args, static_cast<int>(kWSelfSlowSeconds * 1000.0f));
        }
        return;
    }

    const AIHeroClient enemy = RawEnemyById(id);
    if (!enemy.IsValid()) return;
    if (BuffNameContains(args, {
            "zhonyasringshield", "bardrstasis", "chronorevive",
            "guardianangel" })) {
        StasisRecord* record = StasisFor(id, !removed);
        if (record) {
            if (removed) {
                record->EndTick = std::min(record->EndTick, Now());
            } else {
                record->Position = enemy.Position();
                record->EndTick = ControllerHelpers::BuffExpireTick(
                    args, 2500);
            }
        }
    }
    if (BuffNameContains(args, {
            "staticfieldpassive", "blitzcrankrpassive",
            "staticfieldmark", "blitzcrankrmark" })) {
        MarkTracker.Synchronize(id, removed ? 0 : std::max(1, args.Count), Now());
    }
}

inline void OnMissileCreate(
    const SDK::Events::ObjectEventArgs& args) {
    if (!IsHookMissile(args) ||
        !ControllerHelpers::MissileEventIsLocal(args)) return;
    QMissileId = static_cast<int>(args.Sender.NetworkId);
    QInFlight = true;
}

inline void OnMissileDelete(
    const SDK::Events::ObjectEventArgs& args) {
    if (!IsHookMissile(args) ||
        (QMissileId != 0 &&
         static_cast<int>(args.Sender.NetworkId) != QMissileId)) return;
    QMissileId = 0;
    if (QInFlight && Now() >= QExpectedArrivalTick - 180) {
        ActiveSequence = Sequence::HookArrival;
        SequenceExpireTick = Now() + kHookArrivalGraceMs;
    }
}

inline const char* PostureName(Posture posture) {
    switch (posture) {
    case Posture::LaneThreat: return "lane threat";
    case Posture::Catch: return "catch";
    case Posture::WalkUp: return "walk-up E";
    case Posture::FrontToBack: return "front-to-back";
    case Posture::Peel: return "peel";
    case Posture::Disengage: return "disengage";
    case Posture::Objective: return "objective";
    case Posture::RoamVision: return "roam/vision";
    case Posture::Farm: return "hold spells";
    default: return "neutral";
    }
}

inline const char* SequenceName(Sequence sequence) {
    switch (sequence) {
    case Sequence::HookFlight: return "Q flight";
    case Sequence::HookArrival: return "Q arrival";
    case Sequence::WalkUpE: return "W-E";
    case Sequence::AttackReset: return "AA-E-AA";
    case Sequence::PreHookRQ: return "R-Q";
    case Sequence::PeelChain: return "peel E";
    case Sequence::StasisExit: return "stasis Q";
    case Sequence::PlayerLedHook: return "manual Q assist";
    default: return "none";
    }
}

inline void OnDraw() {
    if (!CoachMenu) return;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return;
    if (Bool(CoachMenu, "DrawRanges", true)) {
        Drawing::DrawCircle(
            player.Position(), kQMaximumTargetCenterRange,
            0x6677C9FFu, 1.3f, 72);
        Drawing::DrawCircle(
            player.Position(), kRRadius,
            0x557EDCFFu, 1.2f, 56);
        const AIHeroClient target = PreferredEnemy({}, 1200.0f);
        const float eReach = target.IsValid()
            ? WEReliableReach(target)
            : player.AttackRange() + kEBonusAttackRange + 260.0f;
        Drawing::DrawCircle(
            player.Position(), eReach,
            0x5559E59Cu, 1.2f, 56);
    }
    if (Bool(CoachMenu, "DrawHook", true) && LastQPlan.Valid) {
        const std::uint32_t color = LastQPlan.DangerousDelivery
            ? 0xDDEF5656u : 0xDDF4C857u;
        Drawing::DrawLine(
            player.Position(), LastQPlan.CastPosition,
            color, 2.2f);
        if (LastQPlan.FirstContact.TargetPosition.IsValid()) {
            Drawing::DrawCircle(
                LastQPlan.FirstContact.TargetPosition,
                55.0f, color, 2.0f, 32);
        }
    }
    if (Bool(CoachMenu, "DrawProtectedAlly", true)) {
        const AIHeroClient ally = ProtectedAlly();
        if (ally.IsValid()) {
            Drawing::DrawCircle(
                ally.Position(), ally.BoundingRadius() + 58.0f,
                0xAA63E6A2u, 1.6f, 36);
        }
    }
    if (Bool(CoachMenu, "DrawETarget", true) && EArmed) {
        const AIHeroClient target = RawEnemyById(DesiredETargetId);
        if (target.IsValid()) {
            Drawing::DrawCircle(
                target.Position(), target.BoundingRadius() + 48.0f,
                0xDDF0A44Bu, 2.2f, 36);
        }
    }
    if (Bool(CoachMenu, "DrawMarks", true)) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            const int pending = MarkTracker.Pending(
                static_cast<int>(enemy.NetworkId()));
            if (!enemy.IsValid() || pending <= 0) continue;
            Vec2 screen{};
            if (Drawing::WorldToScreen(enemy.Position(), screen)) {
                char mark[96]{};
                const float next = MarkTracker.SecondsToNext(
                    static_cast<int>(enemy.NetworkId()), Now());
                _snprintf_s(mark, sizeof(mark), _TRUNCATE,
                    "R marks %d | %.1fs", pending,
                    std::isfinite(next) ? next : 0.0f);
                Drawing::DrawText(
                    screen.x - 48.0f, screen.y - 62.0f,
                    0xFFFFD15Cu, mark);
            }
        }
    }
    if (Bool(CoachMenu, "DrawState", true)) {
        Vec2 screen{};
        if (Drawing::WorldToScreen(player.Position(), screen)) {
            const bool barrierReady =
                Now() - LastManaBarrierProcTick >=
                    static_cast<int>(kManaBarrierCooldownSeconds * 1000.0f);
            char state[512]{};
            _snprintf_s(
                state, sizeof(state), _TRUNCATE,
                "Blitz OTP | %s | %s | E %s | barrier %s | owner %s",
                PostureName(CurrentPosture), SequenceName(ActiveSequence),
                EArmed ? "armed" : "idle",
                barrierReady ? "ready" : "cooldown",
                PlayerOverrideUntil >= Now() ? "player" : "controller");
            Drawing::DrawText(
                screen.x - 245.0f, screen.y - 112.0f,
                0xFFFFD15Cu, state);
        }
    }
}

inline void BuildMenu(Menu* root) {
    if (!root) return;
    TacticsMenu = root->AddSubMenu(new Menu(
        "BlitzcrankOneTrick", "Blitzcrank one-trick conductor"));
    TacticsMenu->Add(new MenuBool(
        "KillSecure", "Secure exact mitigated", true));
    TacticsMenu->Add(new MenuSlider(
        "EmergencyHp", "Emergency HP (%)",
        24, 5, 70));
    TacticsMenu->Add(new MenuSlider(
        "ManualOwnershipMs", "Yield player spell (ms)",
        kManualOwnershipMs, 150, 1000));
    TacticsMenu->Add(new MenuSeparator(
        "PlayerOwnership",
        "Movement, attacks,"));

    RoleMenu = TacticsMenu->AddSubMenu(new Menu(
        "Posture", "Catch, front-to-back and carry protection"));
    RoleMenu->Add(new MenuBool(
        "ProtectCarry", "Peel carry before looking", true));
    RoleMenu->Add(new MenuSlider(
        "PeelThreatScore", "Min carry threat score",
        590, 250, 1200));
    RoleMenu->Add(new MenuBool(
        "ReservePeelMana", "Keep mana Power Fist", true));

    QMenu = TacticsMenu->AddSubMenu(new Menu(
        "RocketGrab", "Moving first-body and pull-value policy"));
    QMenu->Add(new MenuList(
        "Hitchance", "Ordinary Q prediction",
        { "Medium", "High", "Very high", "Immobile only" }, 2));
    QMenu->Add(new MenuSlider(
        "MinimumFollowup", "Allies at pull landing", 1, 0, 4));
    QMenu->Add(new MenuSlider(
        "HarassMana", "Minimum harass mana (%)", 58, 0, 100));
    QMenu->Add(new MenuBool(
        "HoldForWE", "Hold Q for W-E", true));
    QMenu->Add(new MenuBool(
        "Peel", "Q as carry displacement", true));
    QMenu->Add(new MenuBool(
        "Interrupt", "Interrupt channels outside", true));
    QMenu->Add(new MenuBool(
        "AntiGapcloser", "Hook committed dash endpoints", true));
    QMenu->Add(new MenuBool(
        "ObjectiveJungler", "Pull visible Smite holder", true));
    QMenu->Add(new MenuBool(
        "StasisExit", "Time Q to stasis exit", true));
    QMenu->Add(new MenuSlider(
        "StasisEarlyMs", "Stasis-exit latency bias (ms)", 45, 0, 140));
    QMenu->Add(new MenuBool(
        "Flee", "Displace pursuer on flee", true));
    QMenu->Add(new MenuSeparator(
        "NoGrief",
        "Healthy assassins, divers,"));

    WMenu = TacticsMenu->AddSubMenu(new Menu(
        "Overdrive", "Walk-up pressure and delayed self-slow"));
    WMenu->Add(new MenuBool(
        "WalkUpE", "W when it reaches a safe", true));
    WMenu->Add(new MenuBool(
        "AfterHook", "W after hook for contact and", true));
    WMenu->Add(new MenuBool(
        "HookAngle", "W when player path creates a", true));
    WMenu->Add(new MenuBool(
        "Peel", "W to peel target", true));
    WMenu->Add(new MenuBool(
        "Flee", "Use W on explicit flee route", true));
    WMenu->Add(new MenuBool(
        "HarassWalkUp", "Allow W-E harass", false));
    WMenu->Add(new MenuBool(
        "Roam", "W on long uncontested player", false));
    WMenu->Add(new MenuSeparator(
        "SelfSlow",
        "The five-second speed decay"));

    EMenu = TacticsMenu->AddSubMenu(new Menu(
        "PowerFist", "Exact target, pre-arm and AA reset timing"));
    EMenu->Add(new MenuBool(
        "AAReset", "AA-E-AA vs trappable", true));
    EMenu->Add(new MenuBool(
        "PreArmHook", "Arm E during Q flight vs", true));
    EMenu->Add(new MenuBool(
        "AssistManualHook", "E timing for player Q", true));
    EMenu->Add(new MenuBool(
        "Peel", "Power Fist the diver", true));
    EMenu->Add(new MenuBool(
        "Interrupt", "E vs channels in range", true));
    EMenu->Add(new MenuBool(
        "AntiGapcloser", "E vs melee gapclosers", true));
    EMenu->Add(new MenuSeparator(
        "ExactTarget",
        "A wrong minion attack is"));

    RMenu = TacticsMenu->AddSubMenu(new Menu(
        "StaticField", "Shield, silence and passive-mark economy"));
    RMenu->Add(new MenuSlider(
        "MinimumTargets", "Min enemies active R", 2, 1, 5));
    RMenu->Add(new MenuBool(
        "ShieldBreak", "Destroy a strategically", true));
    RMenu->Add(new MenuSlider(
        "ShieldBreakValue", "Minimum total shield value", 260, 50, 1500));
    RMenu->Add(new MenuBool(
        "Interrupt", "R silence channels", true));
    RMenu->Add(new MenuBool(
        "MidPullSilence", "Silence escape-ready caster", true));
    RMenu->Add(new MenuBool(
        "PreHookSilence", "Use R-Q on a mobile priority", true));
    RMenu->Add(new MenuBool(
        "Peel", "AoE silence peel carry", true));
    RMenu->Add(new MenuSlider(
        "ExpectedPassiveAttacks", "Likely attacks lost while R",
        3, 0, 8));
    RMenu->Add(new MenuBool(
        "Flee", "R vs multiple pursuers", true));
    RMenu->Add(new MenuSlider(
        "FleeMinimum", "Pursuers required for flee R", 2, 1, 5));
    RMenu->Add(new MenuSeparator(
        "PassiveEconomy",
        "Pending one-per-second"));

    CoachMenu = TacticsMenu->AddSubMenu(new Menu(
        "Coach", "Blitzcrank one-trick geometry and state"));
    CoachMenu->Add(new MenuBool(
        "DrawRanges", "Draw Q/W-E/R ranges", true));
    CoachMenu->Add(new MenuBool(
        "DrawHook", "Draw Q corridor", true));
    CoachMenu->Add(new MenuBool(
        "DrawProtectedAlly", "Mark protected ally", true));
    CoachMenu->Add(new MenuBool(
        "DrawETarget", "Mark Power Fist target", true));
    CoachMenu->Add(new MenuBool(
        "DrawMarks", "Draw R lightning", true));
    CoachMenu->Add(new MenuBool(
        "DrawState", "Draw posture, E, passive", true));
}

inline void OnLoad() {
    EnemyWindows.fill({});
    StasisRecords.fill({});
    MarkTracker = {};
    CurrentPosture = Posture::Neutral;
    ActiveSequence = Sequence::None;
    LastQPlan = {};
    LastWPlan = {};
    LastEPlan = {};
    LastRPlan = {};
    ProtectedAllyId = ThreatenedAllyId = PeelThreatId = AllyThreatUntil = 0;
    GapcloserTargetId = GapcloserExpireTick = 0;
    GapcloserEndpoint = {};
    InterruptTargetId = InterruptExpireTick = 0;
    LastQCastTick = LastWCastTick = LastECastTick = LastRCastTick = 0;
    LastBeforeAttackTargetId = LastBeforeAttackTick = 0;
    LastAfterAttackTargetId = LastAfterAttackTick = 0;
    LastLocalAutoTargetId = LastLocalAutoTick = 0;
    PlayerOverrideUntil = SequenceExpireTick = 0;
    WActive = EArmed = QInFlight = false;
    WExpireTick = WSelfSlowExpireTick = EExpireTick = 0;
    DesiredETargetId = QFlightTargetId = QExpectedArrivalTick = 0;
    QFlightEnd = {};
    QMissileId = 0;
    LastManaBarrierProcTick = -1000000;
    RefreshRuntimeState();
}

inline void OnUnload() {
    TacticsMenu = RoleMenu = QMenu = WMenu = EMenu = RMenu = CoachMenu = nullptr;
}

inline constexpr const char* Scenarios[] = {
    "Pin Summoner's Rift behavior to Riot and CommunityDragon PC 16.14",
    "Apply Riot 25.08 Mana Barrier ratio of 35 percent maximum mana",
    "Apply Riot 25.08 Rocket Grab base damage 110 through 310",
    "Apply Rocket Grab 120 percent AP ratio",
    "Apply Riot 25.22 Power Fist cooldown rather than old wiki cooldown",
    "Apply Riot 13.17 Overdrive rollback with no non-champion health damage",
    "Apply Riot 13.17 Power Fist rollback with no non-champion multiplier",
    "Use current empowered attack total of 200 percent total AD plus 25 percent AP",
    "Use current Static Field passive two percent maximum-mana ratio",
    "Keep Mana Barrier shield strength independent of current mana",
    "Track Mana Barrier health crossing at 30 percent",
    "Track Mana Barrier ten-second shield duration as defensive context",
    "Track Mana Barrier 90-second cooldown from observed buff",
    "Use Q cast time 0.25 seconds and missile speed 1800",
    "Use Q missile segment length 1080",
    "Use Q target-center endpoint maximum 1115",
    "Use Q collision radius 70 plus target bounding radius",
    "Classify contacts beyond segment as endpoint lollipop contacts",
    "Do not extend lollipop target-center range past 1115",
    "Solve target and missile relative motion analytically",
    "Include Q cast delay before calculating a moving blocker",
    "Predict every champion blocker velocity independently",
    "Predict every lane-minion blocker velocity independently",
    "Predict every jungle-monster blocker velocity independently",
    "Ignore invalid, dead and untargetable collision bodies",
    "Order collisions by earliest projectile contact time",
    "Break equal-time collision ties deterministically by distance and network id",
    "Require the intended champion to be the real first body",
    "Reject a target hidden behind a moving minion",
    "Reject a target hidden behind a crossing monster",
    "Allow a blocker that leaves the corridor during cast delay to clear",
    "Allow a blocker entering the corridor at intercept time to block",
    "Respect Yasuo, Samira and projectile-intercept walls before casting Q",
    "Model Q pull destination 75 units in front of Blitzcrank",
    "Score allies at the actual pull landing rather than target origin",
    "Score enemies at the actual pull landing rather than target origin",
    "Reward a pull landing under an allied turret",
    "Require configurable allied follow-up for ordinary catches",
    "Permit a verified lethal hook without allied follow-up",
    "Permit an objective-jungler displacement without lane follow-up",
    "Identify the visible enemy jungler from either Smite slot",
    "Pull the Smite holder away from a nearby epic objective",
    "Prefer selected target while preserving collision and no-grief gates",
    "Respect player cursor direction in ordinary hook scoring",
    "Punish a target already hard crowd controlled",
    "Punish a committed dash endpoint",
    "Punish the exact end of enemy stasis",
    "Time Q travel to stasis expiry with configurable latency bias",
    "Build a future-targetable stasis body without accepting other untargetable bodies",
    "Track enemy escape spell casts as spent windows",
    "Track gapcloser callbacks as spent mobility windows",
    "Lower hook escape penalty only after mobility is actually spent",
    "Treat available Flash as an instant-escape hook penalty",
    "Map champion mobility spells by their real spell slot",
    "Track committed enemy hard crowd control as a safer pull window",
    "Classify marksmen and scaling ranged carries as premium catches",
    "Classify enchanters as premium catches",
    "Classify artillery champions as premium catches",
    "Classify assassins as dangerous delivery targets",
    "Classify divers as dangerous delivery targets",
    "Classify juggernauts as dangerous delivery targets",
    "Classify AoE engage tanks as engage bombs",
    "Classify defensive tanks as wardens",
    "Reject pulling a healthy assassin onto the protected carry",
    "Reject pulling a healthy diver onto the protected carry",
    "Reject pulling a healthy juggernaut onto the protected carry",
    "Reject pulling a healthy engage bomb onto the protected carry",
    "Allow dangerous delivery only for lethal, peel, isolation or spent cooldowns",
    "Allow Q to displace a diver already threatening the protected carry",
    "Reject ordinary hooks into losing local numbers",
    "Do not cancel a guaranteed Power Fist attack with ordinary Q",
    "Use Q pressure without casting while safe W-E is guaranteed",
    "Cast Q after W-E knock-up makes the line guaranteed",
    "Keep ordinary max-range hooks at very-high prediction by default",
    "Allow lower prediction only for explicit reactive windows",
    "Use exact mitigated Q damage for kill secure",
    "Do not call Q lethal through a spell shield",
    "Hold ordinary Q into spell shield or immunity",
    "Reject Q into parry, pool, troll-pole and rappel states",
    "Reject Q against displacement-immune unstoppable casts",
    "Use W initial movement speed 60 through 80 percent",
    "Use W attack speed 30 through 70 percent",
    "Integrate W movement decay to ten percent by 2.9 seconds",
    "Keep ten-percent W movement until the five-second buff ends",
    "Model the following 30-percent self-slow for 1.5 seconds",
    "Never pretend W keeps full tooltip speed for five seconds",
    "Use W only when player path reaches a concrete E or hook angle",
    "Use W after a landed hook for contact and attack speed",
    "W to urgent peel",
    "Use W for explicit flee before the route closes",
    "Reject proactive W paths into enemy turret",
    "Reject proactive W paths into ready point-click lockdown",
    "Reject proactive W paths into losing local numbers",
    "Reject marginal W whose later self-slow is punishable",
    "Preserve the current attack windup before ordinary W",
    "Allow lethal flee W to override attack-windup preservation",
    "Keep lazy roam W disabled by default",
    "Allow roam W only on a long uncontested player-owned path",
    "Never issue a movement order after casting W",
    "Reserve Power Fist mana during ordinary W decisions",
    "Query every live spell cost instead of hard-coding runtime mana",
    "Price Q-E, W-E-Q and full catch sequences independently",
    "Use E as a true AA reset only after a completed attack",
    "Choose AA-E-AA only when the target cannot escape",
    "Skip E when an ordinary safe attack already kills",
    "Use immediate E instead of AA-E-AA against instant escape",
    "Pre-arm E while Q flies toward an escape-ready target",
    "Do not pre-arm E if it would expire before hook arrival",
    "Delay E until an interruptible escape startup is committed",
    "Use immediate E for urgent carry peel",
    "Keep E damage but acknowledge spell shield blocks knock-up",
    "Reject E attack into Shen-like attack-blocking zones",
    "Respect blind and dodge state before arming E",
    "Reserve E for the exact hooked or selected attack target",
    "Block a wrong minion attack only while hooked target arrives before E expiry",
    "Never freeze the orbwalker for an unreachable E target",
    "Never block the exact desired E target",
    "Clear E reservation after its empowered attack completes",
    "Let Orbwalker own every attack before and after Power Fist",
    "Use E before Q for a melee anti-gapcloser",
    "Use R silence before slower tools on an urgent nearby channel",
    "Use E interrupt when a channel is in attack range and R is unavailable",
    "Use Q interrupt when the channel is outside E and R range",
    "Track Static Field marks independently per target",
    "Add one pending Static Field mark after each valid ready-R attack",
    "Allow unlimited pending Static Field mark backlog",
    "Detonate only one pending mark per target each second",
    "Advance missed mark ticks without deleting other targets",
    "Resynchronize pending marks from visible buff counts",
    "Keep existing pending marks after active R is cast",
    "Use current passive damage 50/100/150 plus rank AP ratio and max mana",
    "Estimate passive attacks lost while active R cools down",
    "Increase passive opportunity cost while Overdrive attack speed is active",
    "Hold active R when the next pending lightning already kills",
    "Spend R despite pending lethal only for urgent silence or shield break",
    "Use active R damage 275/400/525 plus 100 percent AP",
    "Destroy all generic, physical and magical damage shields in R scoring",
    "Break a strategically large shield before evaluating R damage",
    "Model shield destruction even when spell shield blocks R damage and silence",
    "Reject ordinary active R into a bare spell shield",
    "Cast R for verified active damage lethal",
    "Cast R for configured multi-target payoff",
    "Cast R for urgent AoE carry peel",
    "Cast R during Q pull before an escape-ready caster arrives",
    "Use R-Q against a mobile priority target only with a clean first-body Q",
    "Preserve an attack windup and passive stack before low-payoff R",
    "Never cast R automatically after every basic attack",
    "Track player-cast Q and optionally assist only its E timing",
    "Yield ownership after every manual W, E and R",
    "Do not alter the aim of a player-cast Q",
    "Do not auto-complete a manual hook when its first body is unknown",
    "Track local Rocket Grab missile create and delete events",
    "Transition from Q flight to arrival with a bounded grace window",
    "Dynamically select the ally most expensive to leave unprotected",
    "Track enemy targeted pressure against allied champions",
    "Prefer carry peel before proactive catch logic",
    "Keep Q, W, E and R out of lane and jungle farming",
    "Preserve movement, attack-move, Hold, Stop, warding, Flash and Hexflash ownership",
    "Draw the planned Q corridor and its actual first body",
    "Draw dangerous-delivery hook plans in warning color",
    "Draw W-E reliable reach rather than a generic W circle",
    "Draw the exact reserved Power Fist target",
    "Draw pending R marks and time to next detonation per enemy",
    "Draw the dynamically protected ally",
    "Draw Mana Barrier cooldown without using current mana as shield strength",
    "Expose posture, sequence, E state and player/controller ownership",
    "Own Blitzcrank's complete spell loop without generic Q-W-E-R fallback",
};

inline constexpr ChampionController Controller = [] {
    ChampionController controller{};
    controller.ChampionId = SDK::ChampionId::Blitzcrank;
    controller.ControllerId = "champion.kuroaio.ai.blitzcrank.controller";
    controller.KitRevision = "Riot 26.15 / CommunityDragon 16.15";
    controller.ResearchArtifact = "AI/Research/AIBlitzcrank.md";
    controller.ImplementationSummary =
        "Moving first-body/lollipop Q with pull-value and dangerous-delivery "
        "gates, decaying W walk-up pressure, exact E pre-arm versus AA-reset "
        "timing, and per-target R mark/shield/silence opportunity economy.";
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
    controller.OnBuffAdd = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, false>;
    controller.OnBuffRemove = &ControllerHelpers::ForwardBuffStateEvent<&UpdateBuffState, true>;

    controller.OnBeforeAttack = &OnBeforeAttack;
    controller.OnAfterAttack = &OnAfterAttack;
    controller.OnGapcloser = &OnGapcloser;
    controller.OnInterruptable = &ControllerHelpers::CaptureInterruptableEvent<&InterruptTargetId, &InterruptExpireTick, 1050, 250, 6500>;
    controller.OnMissileCreate = &OnMissileCreate;
    controller.OnMissileDelete = &OnMissileDelete;
    return controller;
}();

} // namespace Plugins::KuroAIO::AI::Controllers::Blitzcrank
