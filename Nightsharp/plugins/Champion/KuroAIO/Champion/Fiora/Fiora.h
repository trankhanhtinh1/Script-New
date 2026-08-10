#pragma once

#include "../../Helper/KuroAIOCommon.h"
#include "../../../../../SDK/SDK.h"
#include "FioraDatabase.h"

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace Plugins::KuroAIO::Fiora {

// ============================================================================
// Enums and Structs for Fiora
// ============================================================================

enum class PassiveType {
    None, PrePassive, NormalPassive, UltiPassive
};

enum class PassiveDirection {
    NE, SE, NW, SW
};

struct PassiveObject {
    std::string Name;
    EffectEmitter Object;
    PassiveType Type = PassiveType::None;
    PassiveDirection Direction = PassiveDirection::NE;
};

struct PassiveStatus {
    bool HasPassive = false;
    PassiveType Type = PassiveType::None;
    Vector2 TargetPredictedPosition;
    std::vector<PassiveDirection> PassiveDirections;
    std::vector<Vector2> PassivePredictedPositions;
};

enum class TargetMode {
    Normal = 0,
    Optional = 1,
    Selected = 2,
    Priority = 3
};

struct DashTarget {
    AIHeroClient Hero;
    float DistanceDash = 200.0f;
    int TickCount = 0;
};

struct DetectedTarget {
    MissileClient Obj;
    Vector3 Start;
};


// ============================================================================
// Inline Variables
// ============================================================================

inline bool Loaded = false;
inline Spell Q{ SpellSlot::Q, 400.0f };
inline Spell W{ SpellSlot::W, 750.0f };
inline Spell E{ SpellSlot::E, 0.0f };
inline Spell R{ SpellSlot::R, 500.0f };

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* TargetMenu = nullptr;
inline Menu* PriorityModeMenu = nullptr;
inline Menu* OptionalModeMenu = nullptr;
inline Menu* SelectedModeMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* EvadeTargetMenu = nullptr;
inline Menu* EvadeOthersMenu = nullptr;
inline Menu* EvadeTargetNoneMenu = nullptr;

inline std::vector<EffectEmitter> FioraPrePassiveObjects;
inline std::vector<EffectEmitter> FioraPassiveObjects;
inline std::vector<EffectEmitter> FioraUltiPassiveObjects;

inline std::vector<DetectedTarget> DetectedTargets;
inline std::vector<DashTarget> DetectedDashes;

inline AIHeroClient OptionalTarget;
inline AIHeroClient OldOptionalTarget;
inline AIHeroClient PreOptionalTarget;

inline int RivenDashTick = 0;
inline int RivenQ3Tick = 0;
inline Vector2 RivenDashEnd;
inline float RivenQ3Rad = 150.0f;

inline Vector2 FizzFishEndPos;
inline GameObject FizzFishChum;
inline int FizzFishChumStartTick = 0;

inline int movetick = 0;

// Vitals Lists
inline const std::vector<std::string> FioraPassiveNames = {
    "_Passive_NE", "_Passive_SE", "_Passive_NW", "_Passive_SW",
    "_Passive_NE_Timeout", "_Passive_SE_Timeout", "_Passive_NW_Timeout", "_Passive_SW_Timeout"
};

inline const std::vector<std::string> FioraPrePassiveNames = {
    "_Passive_NE_Warning", "_Passive_SE_Warning", "_Passive_NW_Warning", "_Passive_SW_Warning"
};

// ============================================================================
// Helper Functions & Math Extensions
// ============================================================================

#ifndef WM_KEYDOWN
#define WM_KEYDOWN 0x0100
#endif
#ifndef WM_LBUTTONDOWN
#define WM_LBUTTONDOWN 0x0201
#endif

inline bool EqualsIgnoreCase(const char* left, const char* right) {
    return left && right && left[0] && right[0] && _stricmp(left, right) == 0;
}

inline std::string ToLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });
    return str;
}

inline std::string SpellSlotToString(SpellSlot slot) {
    switch (slot) {
        case SpellSlot::Q: return "Q";
        case SpellSlot::W: return "W";
        case SpellSlot::E: return "E";
        case SpellSlot::R: return "R";
        default: return "Unknown";
    }
}

inline Vector2 RotateAround(const Vector2& pointToRotate, const Vector2& centerPoint, float angleInRadians) {
    double cosTheta = std::cos(angleInRadians);
    double sinTheta = std::sin(angleInRadians);
    return Vector2(
        static_cast<float>(cosTheta * (pointToRotate.x - centerPoint.x) - sinTheta * (pointToRotate.y - centerPoint.y) + centerPoint.x),
        static_cast<float>(sinTheta * (pointToRotate.x - centerPoint.x) + cosTheta * (pointToRotate.y - centerPoint.y) + centerPoint.y)
    );
}

inline float AngleBetween(const Vector2& a, const Vector2& center, const Vector2& c) {
    float a1 = c.Distance(center);
    float b1 = a.Distance(c);
    float c1 = center.Distance(a);
    if (a1 == 0.0f || c1 == 0.0f) { return 0.0f; }
    return static_cast<float>(std::acos((a1 * a1 + c1 * c1 - b1 * b1) / (2.0f * a1 * c1)) * (180.0f / 3.14159265358979323846f));
}

inline bool InTheCone(const Vector2& pos, const Vector2& centerconePolar, const Vector2& centerconeEnd, double coneAngle) {
    return AngleBetween(pos, centerconePolar, centerconeEnd) < coneAngle / 2.0;
}

inline int CountMinionsInRange(const Vector3& position, float range, bool includeJungle) {
    int count = 0;
    float rangeSqr = range * range;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, range) && minion.Position().DistanceSqr2D(position) <= rangeSqr) {
            count++;
        }
    }
    if (includeJungle) {
        for (const auto& jg : GameObjects::Jungle()) {
            if (ValidTarget(jg, range) && jg.Position().DistanceSqr2D(position) <= rangeSqr) {
                count++;
            }
        }
    }
    return count;
}

inline bool IsPassiveMarkName(const std::string& name) {
    if (name.find("Fiora") == std::string::npos) return false;
    for (const auto& pName : FioraPassiveNames) {
        if (name.find(pName) != std::string::npos) return true;
    }
    return false;
}

inline bool IsPrePassiveMarkName(const std::string& name) {
    if (name.find("Fiora") == std::string::npos) return false;
    for (const auto& pName : FioraPrePassiveNames) {
        if (name.find(pName) != std::string::npos) return true;
    }
    return false;
}

inline bool IsUltMarkName(const std::string& name) {
    if (name.find("Fiora") == std::string::npos) return false;
    return (name.find("_R_Mark") != std::string::npos) ||
           (name.find("_R") != std::string::npos && name.find("_FioraOnly") != std::string::npos);
}

// ============================================================================
// Core Logic Functions
// ============================================================================

static void FioraPassiveUpdate() {
    FioraPrePassiveObjects.clear();
    FioraPassiveObjects.clear();
    FioraUltiPassiveObjects.clear();

    for (const auto& emitter : GameObjects::ParticleEmitters()) {
        if (!emitter.IsValid() || emitter.IsDead()) continue;
        std::string name = GetObjectName(emitter);
        if (IsPrePassiveMarkName(name)) {
            FioraPrePassiveObjects.push_back(emitter);
        } else if (IsPassiveMarkName(name)) {
            FioraPassiveObjects.push_back(emitter);
        } else if (IsUltMarkName(name)) {
            FioraUltiPassiveObjects.push_back(emitter);
        }
    }
}

static PassiveDirection GetPassiveDirection(const std::string& name) {
    if (name.find("NE") != std::string::npos) return PassiveDirection::NE;
    if (name.find("SE") != std::string::npos) return PassiveDirection::SE;
    if (name.find("NW") != std::string::npos) return PassiveDirection::NW;
    return PassiveDirection::SW;
}

static PassiveStatus GetPassiveStatus(const AIHeroClient& target, float delay = 0.25f) {
    FioraPassiveUpdate();
    std::vector<PassiveObject> list;

    for (const auto& ee : FioraPrePassiveObjects) {
        if (ee.IsValid() && ee.Position().Distance2D(target.Position()) <= 50.0f) {
            std::string name = GetObjectName(ee); list.push_back({ name, ee, PassiveType::PrePassive, GetPassiveDirection(name) });
        }
    }
    for (const auto& ee : FioraPassiveObjects) {
        if (ee.IsValid() && ee.Position().Distance2D(target.Position()) <= 50.0f) {
            std::string name = GetObjectName(ee); list.push_back({ name, ee, PassiveType::NormalPassive, GetPassiveDirection(name) });
        }
    }
    for (const auto& ee : FioraUltiPassiveObjects) {
        if (ee.IsValid() && ee.Position().Distance2D(target.Position()) <= 50.0f) {
            std::string name = GetObjectName(ee); list.push_back({ name, ee, PassiveType::UltiPassive, GetPassiveDirection(name) });
        }
    }

    const Vector2 targetpredictedpos = SDK::Prediction::GetPrediction(target, delay).GetUnitPosition().To2D();

    if (list.empty()) {
        return { false, PassiveType::None, {}, {}, {} };
    }

    const auto& firstObj = list.front();
    std::vector<PassiveDirection> listdirections;
    std::vector<Vector2> listpositions;

    for (const auto& po : list) {
        listdirections.push_back(po.Direction);
        Vector2 pos = targetpredictedpos;
        if (po.Direction == PassiveDirection::NE) {
            pos.y = pos.y + 200.0f;
        } else if (po.Direction == PassiveDirection::NW) {
            pos.x = pos.x + 200.0f;
        } else if (po.Direction == PassiveDirection::SE) {
            pos.x = pos.x - 200.0f;
        } else if (po.Direction == PassiveDirection::SW) {
            pos.y = pos.y - 200.0f;
        }
        listpositions.push_back(pos);
    }

    return { true, firstObj.Type, targetpredictedpos, listdirections, listpositions };
}

static std::vector<Vector2> GetRadiusPoints(const Vector2& targetpredictedpos, const Vector2& passivepredictedposition) {
    std::vector<Vector2> RadiusPoints;
    for (int i = 50; i <= 300; i += 25) {
        Vector2 x = targetpredictedpos.Extend(passivepredictedposition, static_cast<float>(i));
        for (int j = -45; j <= 45; j += 5) {
            float angleRad = static_cast<float>(j) * (3.141592653589793f / 180.0f);
            RadiusPoints.push_back(RotateAround(x, targetpredictedpos, angleRad));
        }
    }
    return RadiusPoints;
}

// ============================================================================
// Target Selector Toggles & Ranges
// ============================================================================

static float OptionalRange() {
    return OptionalModeMenu ? static_cast<float>(Slider(OptionalModeMenu, "OptionalRange", 1000)) : 1000.0f;
}

static float SelectedRange() {
    return SelectedModeMenu ? static_cast<float>(Slider(SelectedModeMenu, "SelectedRange", 1000)) : 1000.0f;
}

static float PriorityRange() {
    return PriorityModeMenu ? static_cast<float>(Slider(PriorityModeMenu, "PriorityRange", 1000)) : 1000.0f;
}

static TargetMode GetTargetingMode() {
    int idx = List(TargetMenu, "TargetingMode", 3);
    switch (idx) {
        case 0: return TargetMode::Optional;
        case 1: return TargetMode::Selected;
        case 2: return TargetMode::Priority;
        default: return TargetMode::Normal;
    }
}

static AIHeroClient GetUltedTarget() {
    if (!Bool(TargetMenu, "FocusUltedTarget", false)) {
        return AIHeroClient();
    }
    FioraPassiveUpdate();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy)) continue;
        for (const auto& mark : FioraUltiPassiveObjects) {
            if (mark.IsValid() && mark.Position().Distance2D(enemy.Position()) <= 50.0f) {
                return enemy;
            }
        }
    }
    return AIHeroClient();
}

static AIHeroClient GetOptionalTarget() {
    auto ulted = GetUltedTarget();
    float oRange = OptionalRange();
    if (ValidHeroTarget(ulted, oRange)) {
        OptionalTarget = ulted;
        return OptionalTarget;
    }
    if (ValidHeroTarget(OptionalTarget, oRange)) {
        return OptionalTarget;
    }

    AIHeroClient best;
    float bestDist = FLT_MAX;
    const auto player = Player();
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, oRange)) continue;
        float dist = player.Position().Distance(enemy.Position());
        if (dist < bestDist) {
            best = enemy;
            bestDist = dist;
        }
    }
    OptionalTarget = best;
    return OptionalTarget;
}

static AIHeroClient GetPriorityTarget() {
    auto ulted = GetUltedTarget();
    float pRange = PriorityRange();
    if (ValidHeroTarget(ulted, pRange)) {
        return ulted;
    }

    AIHeroClient best;
    int bestPriority = -1;
    float bestHealth = FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, pRange)) continue;
        const SDK::ChampionId championId =
            SDK::ChampionIdFromName(GetObjectCharacterName(enemy).c_str());
        const char* championName = SDK::ChampionName(championId);
        if (championId == SDK::ChampionId::Unknown || !championName[0]) continue;
        std::string key = "Priority" + std::string(championName);
        int p = Slider(PriorityModeMenu, key.c_str(), 2);
        if (p > bestPriority) {
            best = enemy;
            bestPriority = p;
            bestHealth = enemy.Health();
        } else if (p == bestPriority && enemy.Health() < bestHealth) {
            best = enemy;
            bestHealth = enemy.Health();
        }
    }
    return best;
}

static AIHeroClient GetSelectedTarget() {
    auto ulted = GetUltedTarget();
    float sRange = SelectedRange();
    if (ValidHeroTarget(ulted, sRange)) {
        return ulted;
    }
    auto tar = SDK::TargetSelector::Instance()->GetSelectedTarget();
    if (ValidHeroTarget(tar, sRange)) {
        return tar;
    }
    if (Bool(SelectedModeMenu, "SelectedSwitchIfNoSelected", true)) {
        return GetOptionalTarget();
    }
    return AIHeroClient();
}

static AIHeroClient GetStandarTarget(float range) {
    auto ulted = GetUltedTarget();
    if (ValidHeroTarget(ulted, 500.0f)) {
        return ulted;
    }
    return GetPhysicalTarget(range);
}

static AIHeroClient GetFioraTarget(float range = 500.0f) {
    const TargetMode mode = GetTargetingMode();
    if (mode == TargetMode::Normal)
        return GetStandarTarget(range);
    if (mode == TargetMode::Optional)
        return GetOptionalTarget();
    if (mode == TargetMode::Priority)
        return GetPriorityTarget();
    if (mode == TargetMode::Selected)
        return GetSelectedTarget();
    return AIHeroClient();
}

// ============================================================================
// W Block (Parry) Solvers
// ============================================================================

static void CastWPosition(const Vector3& position) {
    const auto player = Player();
    if (!player.IsValid() || position.IsZero() ||
        SDK::Collision::HasProjectileWallCollision(
            player.Position(), position, W.Width * 0.5f)) {
        return;
    }
    player.Spellbook().CastSpell(SpellSlot::W, position);
}

static void SolveInstantBlock() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() ||
        SDK::HasBuffOfType(player, SDK::BuffType::SpellShield) || SDK::HasBuffOfType(player, SDK::BuffType::SpellImmunity) ||
        !Bool(EvadeOthersMenu, "W", true) || !W.IsReady()) {
        return;
    }

    auto target = GetFioraTarget(W.Range);
    if (ValidHeroTarget(target, W.Range)) {
        Vector3 castPosition = SDK::Prediction::GetPrediction(
            target, W.Delay).GetUnitPosition();
        CastWPosition(castPosition.IsZero() ? target.Position() : castPosition);
    } else {
        AIHeroClient fallbackHero;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy, W.Range)) {
                fallbackHero = enemy;
                break;
            }
        }
        if (fallbackHero.IsValid()) {
            Vector3 castPosition = SDK::Prediction::GetPrediction(
                fallbackHero, W.Delay).GetUnitPosition();
            CastWPosition(castPosition.IsZero()
                ? fallbackHero.Position() : castPosition);
        } else {
            Vector3 castPos = player.Position().Extend(SDK::Game::CursorPosition(), 100.0f);
            CastWPosition(castPos);
        }
    }
}

static void SolveDelayBlock(int timeDelayMs) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() ||
        SDK::HasBuffOfType(player, SDK::BuffType::SpellShield) || SDK::HasBuffOfType(player, SDK::BuffType::SpellImmunity) ||
        !Bool(EvadeOthersMenu, "W", true) || !W.IsReady()) {
        return;
    }

    DelayAction::Add(timeDelayMs, []() {
        SolveInstantBlock();
    });
}

static void SolveBuffBlock(uintptr_t owner, const std::string& buffName, float maxRemainingMs = 250.0f) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() ||
        SDK::HasBuffOfType(player, SDK::BuffType::SpellShield) || SDK::HasBuffOfType(player, SDK::BuffType::SpellImmunity) ||
        !Bool(EvadeOthersMenu, "W", true) || !W.IsReady()) {
        return;
    }

    float remainingMs = CoreBuffs::GetBuffRemainingTime(owner, buffName.c_str(), SDK::Game::Time()) * 1000.0f;
    if (remainingMs > 0.0f && remainingMs <= maxRemainingMs + static_cast<float>(SDK::Game::Ping())) {
        SolveInstantBlock();
    }
}

// ============================================================================
// Q cast helpers
// ============================================================================

static float getQPassivedelay(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;

    PassiveStatus targetStatus;
    const float dist = player.Position().Distance2D(target.Position());
    if (SDK::Prediction::GetPrediction(target, 0.25f).GetUnitPosition().Distance2D(player.Position()) > dist) {
        targetStatus = GetPassiveStatus(target, dist / 1100.0f);
    } else {
        targetStatus = GetPassiveStatus(target, 0.0f);
    }

    if (!targetStatus.HasPassive) return 0.0f;

    if (targetStatus.Type == PassiveType::PrePassive || targetStatus.Type == PassiveType::NormalPassive) {
        if (targetStatus.PassivePredictedPositions.empty()) return 0.0f;
        Vector2 pos = targetStatus.PassivePredictedPositions.front();
        return player.Position().To2D().Distance(pos) / 1100.0f + static_cast<float>(SDK::Game::Ping()) / 1000.0f;
    }
    if (targetStatus.Type == PassiveType::UltiPassive) {
        if (targetStatus.PassivePredictedPositions.empty()) return 0.0f;
        const auto& poses = targetStatus.PassivePredictedPositions;
        const auto player2D = player.Position().To2D();
        Vector2 pos = *std::min_element(poses.begin(), poses.end(), [player2D](const Vector2& a, const Vector2& b) {
            return player2D.DistanceSqr(a) < player2D.DistanceSqr(b);
        });
        return player2D.Distance(pos) / 1100.0f + static_cast<float>(SDK::Game::Ping()) / 1000.0f;
    }
    return 0.0f;
}

static float getQGapClosedelay(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid()) return 0.0f;
    float distance = player.Position().Distance(target.Position());
    float dashDist = distance > 400.0f ? 400.0f : distance;
    return dashDist / 1100.0f + static_cast<float>(SDK::Game::Ping()) / 1000.0f;
}

static bool castQtoGapClose(const AIHeroClient& target, float delay) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return false;

    Vector2 targetpredictedpos = SDK::Prediction::GetPrediction(target, delay).GetUnitPosition().To2D();
    const auto player2D = player.Position().To2D();
    Vector2 pos = player2D.Distance(targetpredictedpos) > 400.0f ?
        player2D.Extend(targetpredictedpos, 400.0f) : targetpredictedpos;

    if (targetpredictedpos.Distance(pos) <= 300.0f && !NavMesh::IsWall(Vector3::From2D(pos))) {
        Q.Cast(Vector3::From2D(pos));
        return true;
    }
    return false;
}

static bool castQtoPrePassive(const AIHeroClient& target, float delay) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return false;

    auto targetStatus = GetPassiveStatus(target, delay);
    if (targetStatus.Type != PassiveType::PrePassive || targetStatus.PassivePredictedPositions.empty()) {
        return false;
    }

    Vector2 passivepos = targetStatus.PassivePredictedPositions.front();
    auto radiusPoints = GetRadiusPoints(targetStatus.TargetPredictedPosition, passivepos);
    const auto player2D = player.Position().To2D();

    Vector2 bestPos;
    float bestDist = FLT_MAX;
    for (const auto& pt : radiusPoints) {
        if (pt.Distance(player2D) <= 400.0f && !NavMesh::IsWall(Vector3::From2D(pt))) {
            float dist = pt.Distance(passivepos);
            if (dist < bestDist) {
                bestPos = pt;
                bestDist = dist;
            }
        }
    }

    if (bestPos.IsValid() && !bestPos.IsZero()) {
        Q.Cast(Vector3::From2D(bestPos));
        return true;
    }
    return false;
}

static bool castQtoPassive(const AIHeroClient& target, float delay) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return false;

    auto targetStatus = GetPassiveStatus(target, delay);
    if (!targetStatus.HasPassive || targetStatus.Type != PassiveType::NormalPassive || targetStatus.PassivePredictedPositions.empty()) {
        return false;
    }

    Vector2 passivepos = targetStatus.PassivePredictedPositions.front();
    auto radiusPoints = GetRadiusPoints(targetStatus.TargetPredictedPosition, passivepos);
    const auto player2D = player.Position().To2D();

    Vector2 bestPos;
    float bestDist = FLT_MAX;
    for (const auto& pt : radiusPoints) {
        if (pt.Distance(player2D) <= 400.0f && !NavMesh::IsWall(Vector3::From2D(pt))) {
            float dist = pt.Distance(passivepos);
            if (dist < bestDist) {
                bestPos = pt;
                bestDist = dist;
            }
        }
    }

    if (bestPos.IsValid() && !bestPos.IsZero()) {
        Q.Cast(Vector3::From2D(bestPos));
        return true;
    }
    return false;
}

static bool castQtoUltPassive(const AIHeroClient& target, float delay) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return false;

    auto targetStatus = GetPassiveStatus(target, delay);
    if (!targetStatus.HasPassive || targetStatus.Type != PassiveType::UltiPassive || targetStatus.PassivePredictedPositions.empty()) {
        return false;
    }

    const auto player2D = player.Position().To2D();
    const auto& poses = targetStatus.PassivePredictedPositions;
    Vector2 passivepos = *std::min_element(poses.begin(), poses.end(), [player2D](const Vector2& a, const Vector2& b) {
        return player2D.DistanceSqr(a) < player2D.DistanceSqr(b);
    });

    auto radiusPoints = GetRadiusPoints(targetStatus.TargetPredictedPosition, passivepos);
    Vector2 bestPos;
    float bestDist = FLT_MAX;
    for (const auto& pt : radiusPoints) {
        if (pt.Distance(player2D) <= 400.0f && !NavMesh::IsWall(Vector3::From2D(pt))) {
            float dist = pt.Distance(passivepos);
            if (dist < bestDist) {
                bestPos = pt;
                bestDist = dist;
            }
        }
    }

    if (bestPos.IsValid() && !bestPos.IsZero()) {
        Q.Cast(Vector3::From2D(bestPos));
        return true;
    }
    return false;
}

// ============================================================================
// Damage Calculator
// ============================================================================

static float GetPassiveDamage(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return (0.03f + (0.027f + 0.001f * static_cast<float>(player.Level())) * player.BonusAttackDamage() / 100.0f) * target.MaxHealth();
}

static float GetUltiPassiveDamage(const AIHeroClient& target) {
    return GetPassiveDamage(target) * 4.0f;
}

static float GetUltiDamage(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid()) return 0.0f;
    return GetUltiPassiveDamage(target) + static_cast<float>(player.CalculatePhysicalDamage(target, player.TotalAttackDamage())) * 4.0f;
}

static float GetFastDamage(const AIHeroClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;

    float damage = 0.0f;
    damage += Q.GetDamage(target);
    if (Q.IsReady()) {
        damage += Q.GetDamage(target);
    }
    if (R.IsReady()) {
        damage += GetUltiDamage(target);
        return damage;
    }

    auto status = GetPassiveStatus(target, 0.0f);
    if (status.HasPassive) {
        damage += static_cast<float>(status.PassivePredictedPositions.size()) * (GetPassiveDamage(target) + static_cast<float>(player.CalculatePhysicalDamage(target, player.TotalAttackDamage())));
        if (status.PassivePredictedPositions.size() < 3) {
            damage += static_cast<float>(3 - status.PassivePredictedPositions.size()) * static_cast<float>(player.CalculatePhysicalDamage(target, player.TotalAttackDamage()));
        }
        return damage;
    }

    damage += static_cast<float>(player.CalculatePhysicalDamage(target, player.TotalAttackDamage())) * 2.0f;
    return damage;
}

// ============================================================================
// Item Cast Logic
// ============================================================================

static bool HasActiveItem() {
    const auto player = Player();
    if (!player.IsValid()) return false;
    return SDK::Items::CanUseItem(player, SDK::ItemId::Youmuu_s_Ghostblade) ||
           SDK::Items::CanUseItem(player, SDK::ItemId::Stridebreaker) ||
           SDK::Items::CanUseItem(player, SDK::ItemId::Titanic_Hydra) ||
           SDK::Items::CanUseItem(player, SDK::ItemId::Ravenous_Hydra) ||
           SDK::Items::CanUseItem(player, SDK::ItemId::Profane_Hydra) ||
           SDK::Items::CanUseItem(player, SDK::ItemId::Tiamat) ||
           SDK::Items::CanUseItem(player, SDK::ItemId::Randuin_s_Omen) ||
           SDK::Items::CanUseItem(player, SDK::ItemId::Locket_of_the_Iron_Solari);
}

static bool CastActiveItem() {
    const auto player = Player();
    if (!player.IsValid()) return false;
    if (SDK::Items::CanUseItem(player, SDK::ItemId::Youmuu_s_Ghostblade)) {
        SDK::Items::UseItem(player, SDK::ItemId::Youmuu_s_Ghostblade);
    }
    if (SDK::Items::CanUseItem(player, SDK::ItemId::Titanic_Hydra)) {
        if (SDK::Items::UseItem(player, SDK::ItemId::Titanic_Hydra)) {
            Orbwalker::ResetAutoAttackTimer();
            return true;
        }
    }
    if (SDK::Items::CanUseItem(player, SDK::ItemId::Ravenous_Hydra)) {
        SDK::Items::UseItem(player, SDK::ItemId::Ravenous_Hydra);
    }
    if (SDK::Items::CanUseItem(player, SDK::ItemId::Profane_Hydra)) {
        SDK::Items::UseItem(player, SDK::ItemId::Profane_Hydra);
    }
    if (SDK::Items::CanUseItem(player, SDK::ItemId::Stridebreaker)) {
        SDK::Items::UseItem(player, SDK::ItemId::Stridebreaker, SDK::Game::CursorPosition());
    }
    if (SDK::Items::CanUseItem(player, SDK::ItemId::Tiamat)) {
        SDK::Items::UseItem(player, SDK::ItemId::Tiamat);
    }
    if (SDK::Items::CanUseItem(player, SDK::ItemId::Randuin_s_Omen)) {
        bool enemyInRange = false;
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy, 500.0f)) {
                enemyInRange = true;
                break;
            }
        }
        if (enemyInRange) {
            SDK::Items::UseItem(player, SDK::ItemId::Randuin_s_Omen);
        }
    }
    if (SDK::Items::CanUseItem(player, SDK::ItemId::Locket_of_the_Iron_Solari)) {
        if (player.HealthPercent() <= 40.0f) {
            bool enemyInRange = false;
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (ValidHeroTarget(enemy, 600.0f)) {
                    enemyInRange = true;
                    break;
                }
            }
            if (enemyInRange) {
                SDK::Items::UseItem(player, SDK::ItemId::Locket_of_the_Iron_Solari);
            }
        }
    }
    return false;
}

// ============================================================================
// Wall Jump Methods
// ============================================================================

static Vector2 GetFirstWallPoint(const Vector2& from, const Vector2& to, float step = 25.0f) {
    Vector2 direction = (to - from).Normalized();
    float distance = from.Distance(to);
    for (float d = 0.0f; d < distance; d += step) {
        Vector2 testPoint = from + direction * d;
        CollisionFlags flags = NavMesh::GetCollisionFlags(testPoint.x, testPoint.y);
        if (HasFlag(flags, CollisionFlags::Wall) || HasFlag(flags, CollisionFlags::Building)) {
            return from + direction * (d - step);
        }
    }
    return {};
}

static Vector2 GetLastWallPoint(const Vector2& from, const Vector2& to, float step = 25.0f) {
    Vector2 direction = (to - from).Normalized();
    auto fstWall = GetFirstWallPoint(from, to, step);
    if (fstWall.IsValid() && !fstWall.IsZero()) {
        float distance = fstWall.Distance(to) + 1000.0f;
        for (float d = step; d < distance; d += step) {
            Vector2 testPoint = fstWall + direction * d;
            CollisionFlags flags = NavMesh::GetCollisionFlags(testPoint.x, testPoint.y);
            if (!HasFlag(flags, CollisionFlags::Wall) && !HasFlag(flags, CollisionFlags::Building)) {
                return fstWall + direction * d;
            }
        }
    }
    return {};
}

static bool InMiddleWall(const Vector2& firstwall, const Vector2& lastwall) {
    Vector2 midwall = (firstwall + lastwall) / 2.0f;
    Vector2 point = midwall.Extend(SDK::Game::CursorPosition().To2D(), 50.0f);
    // This probe is only a yes/no gate.  Fifteen-degree samples retain the
    // wall-jump corridor check while avoiding 36 collision-grid reads every
    // update when the key is held.
    for (int i = 0; i <= 350; i += 15) {
        Vector2 testpoint = RotateAround(point, midwall, static_cast<float>(i) * (3.14159265f / 180.0f));
        CollisionFlags flags = NavMesh::GetCollisionFlags(testpoint.x, testpoint.y);
        if (!HasFlag(flags, CollisionFlags::Wall) && !HasFlag(flags, CollisionFlags::Building)) {
            return false;
        }
    }
    return true;
}

static void RunWallJump() {
    const auto player = Player();
    if (!player.IsValid() || !Bool(MiscMenu, "WallJump", true) || !Key(MiscMenu, "WallJumpKey", false)) {
        return;
    }

    const Vector2 playerPos2D = player.Position().To2D();
    const Vector2 cursorPos2D = SDK::Game::CursorPosition().To2D();

    auto fstWall = GetFirstWallPoint(playerPos2D, cursorPos2D);
    if (fstWall.IsValid() && !fstWall.IsZero()) {
        auto lstWall = GetLastWallPoint(fstWall, cursorPos2D);
        if (lstWall.IsValid() && !lstWall.IsZero()) {
            if (InMiddleWall(fstWall, lstWall)) {
                Vector3 dest = player.Position().Extend(SDK::Game::CursorPosition(), 30.0f);
                int now = SDK::Variables::TickCount();
                int pingSafe = 70 + (std::min)(60, SDK::Game::Ping());
                if (now - movetick >= pingSafe) {
                    if (player.Position().Distance(SDK::Game::CursorPosition()) <= 1200.0f &&
                        NavMesh::IsWall(player.Position().Extend(SDK::Game::CursorPosition(), 200.0f))) {
                        SDK::IssueOrder(player, SDK::GameObjectOrder::MoveTo, player.Position().Extend(SDK::Game::CursorPosition(), -20.0f));
                        movetick = now;
                    } else {
                        SDK::IssueOrder(player, SDK::GameObjectOrder::MoveTo, player.Position().Distance(SDK::Game::CursorPosition()) <= 1200.0f ?
                                          player.Position().Extend(SDK::Game::CursorPosition(), 200.0f) : SDK::Game::CursorPosition());
                        movetick = now;
                    }
                }
                if (NavMesh::IsWall(dest) && SDK::Prediction::GetPrediction(player, 0.5f).GetUnitPosition().To2D().Distance(playerPos2D) <= 10.0f && Q.IsReady()) {
                    Vector2 pos = playerPos2D.Extend(cursorPos2D, 100.0f);
                    // A one-degree sweep performed up to 360 times and
                    // continued casting after a valid point was found.  A
                    // four-degree broad sweep is sufficient for the 400-unit
                    // Q dash; stop after the first valid escape point.
                    for (int i = 0; i < 360; i += 4) {
                        float angleRad = static_cast<float>(i) * (3.14159265f / 180.0f);
                        Vector2 pos1 = RotateAround(pos, playerPos2D, angleRad);
                        Vector2 pos2 = playerPos2D.Extend(pos1, 400.0f);
                        if (InTheCone(pos1, playerPos2D, cursorPos2D, 60.0) && NavMesh::IsWall(Vector3::From2D(pos1)) && !NavMesh::IsWall(Vector3::From2D(pos2))) {
                            Q.Cast(Vector3::From2D(pos2));
                            break;
                        }
                    }
                }
            } else if (SDK::Variables::TickCount() - movetick >= 70 + (std::min)(60, SDK::Game::Ping())) {
                SDK::IssueOrder(player, SDK::GameObjectOrder::MoveTo, SDK::Game::CursorPosition());
                movetick = SDK::Variables::TickCount();
            }
        } else if (SDK::Variables::TickCount() - movetick >= 70 + (std::min)(60, SDK::Game::Ping())) {
            SDK::IssueOrder(player, SDK::GameObjectOrder::MoveTo, SDK::Game::CursorPosition());
            movetick = SDK::Variables::TickCount();
        }
    } else if (SDK::Variables::TickCount() - movetick >= 70 + (std::min)(60, SDK::Game::Ping())) {
        SDK::IssueOrder(player, SDK::GameObjectOrder::MoveTo, SDK::Game::CursorPosition());
        movetick = SDK::Variables::TickCount();
    }
}

// ============================================================================
// Combo & Harass execution
// ============================================================================

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) return;

    if (Q.IsReady() && Bool(ComboMenu, "UseQCombo", true)) {
        bool castQPass = Bool(ComboMenu, "UseQComboPass", true);
        bool castQPrePass = Bool(ComboMenu, "UseQComboPrePass", true);
        bool castQGap = Bool(ComboMenu, "UseQComboGap", true);

        if (castQPass || castQPrePass || castQGap) {
            const TargetMode mode = GetTargetingMode();
            if (mode == TargetMode::Normal) {
                std::vector<AIHeroClient> sortedEnemies = GameObjects::EnemyHeroes();
                std::sort(sortedEnemies.begin(), sortedEnemies.end(), [player](const AIHeroClient& a, const AIHeroClient& b) {
                    return player.Position().DistanceSqr(a.Position()) < player.Position().DistanceSqr(b.Position());
                });
                for (const auto& enemy : sortedEnemies) {
                    if (!ValidHeroTarget(enemy)) continue;
                    auto status = GetPassiveStatus(enemy, 0.0f);
                    if (status.HasPassive && !(SDK::Core::Utils::AutoAttack::InAutoAttackRange(enemy) && status.PassivePredictedPositions.size() > 0 &&
                        InTheCone(player.Position().To2D(), status.TargetPredictedPosition, status.PassivePredictedPositions.front(), 90.0))) {
                        if (castQPass && status.Type == PassiveType::UltiPassive && castQtoUltPassive(enemy, getQPassivedelay(enemy)))
                            break;
                        if (castQPass && status.Type == PassiveType::NormalPassive && castQtoPassive(enemy, getQPassivedelay(enemy)))
                            break;
                        if (castQPrePass && status.Type == PassiveType::PrePassive && castQtoPrePassive(enemy, getQPassivedelay(enemy)))
                            break;
                        if (castQGap && castQtoGapClose(enemy, getQGapClosedelay(enemy)))
                            break;
                    }
                }
            } else {
                auto enemy = GetFioraTarget();
                if (ValidHeroTarget(enemy)) {
                    auto status = GetPassiveStatus(enemy, 0.0f);
                    if (status.HasPassive && !(SDK::Core::Utils::AutoAttack::InAutoAttackRange(enemy) && status.PassivePredictedPositions.size() > 0 &&
                        InTheCone(player.Position().To2D(), status.TargetPredictedPosition, status.PassivePredictedPositions.front(), 90.0))) {
                        if (castQPass && status.Type == PassiveType::UltiPassive && castQtoUltPassive(enemy, getQPassivedelay(enemy)))
                            return;
                        if (castQPass && status.Type == PassiveType::NormalPassive && castQtoPassive(enemy, getQPassivedelay(enemy)))
                            return;
                        if (castQPrePass && status.Type == PassiveType::PrePassive && castQtoPrePassive(enemy, getQPassivedelay(enemy)))
                            return;
                        if (castQGap && castQtoGapClose(enemy, getQGapClosedelay(enemy)))
                            return;
                    }
                }
            }
        }

        if (Bool(ComboMenu, "UseQComboGapMinion", false)) {
            float currentCdr = player.Level() >= 11 ? 30.0f : 10.0f;
            if (currentCdr >= static_cast<float>(Slider(ComboMenu, "UseQComboGapMinionValue", 25))) {
                auto target = GetFioraTarget();
                if (target.IsValid() && player.Position().Distance(target.Position()) >= 500.0f) {
                    Vector3 extPos = player.Position().Extend(target.Position(), 400.0f);
                    if (CountMinionsInRange(extPos, 300.0f, false) >= 1) {
                        Q.Cast(extPos);
                    }
                }
            }
        }
    }

    if (R.IsReady() && Bool(ComboMenu, "UseRCombo", true)) {
        auto target = GetFioraTarget(500.0f);
        if (ValidHeroTarget(target, 500.0f)) {
            auto status = GetPassiveStatus(target, 0.0f);
            if (!status.HasPassive || (status.HasPassive && !(SDK::Core::Utils::AutoAttack::InAutoAttackRange(target) && status.PassivePredictedPositions.size() > 0 &&
                InTheCone(player.Position().To2D(), status.TargetPredictedPosition, status.PassivePredictedPositions.front(), 90.0)))) {
                if (Bool(ComboMenu, "UseRComboLowHP", true) && player.HealthPercent() <= static_cast<float>(Slider(ComboMenu, "UseRComboLowHPValue", 40))) {
                    R.Cast(target);
                    return;
                }
                if (Bool(ComboMenu, "UseRComboKillable", true) && GetFastDamage(target) >= target.Health() && target.Health() >= GetFastDamage(target) / 3.0f) {
                    R.Cast(target);
                    return;
                }
                if (Bool(ComboMenu, "UseRComboAlways", false)) {
                    R.Cast(target);
                    return;
                }
            }
            if (Bool(ComboMenu, "UseRComboOnTap", true) && Key(ComboMenu, "UseRComboOnTapKey", false)) {
                R.Cast(target);
                return;
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() < static_cast<float>(Slider(HarassMenu, "ManaHarass", 40))) return;

    if (Q.IsReady() && Bool(HarassMenu, "UseQHarass", true)) {
        bool castQPass = Bool(HarassMenu, "UseQHarassPass", true);
        bool castQPrePass = Bool(HarassMenu, "UseQHarassPrePass", true);
        bool castQGap = Bool(HarassMenu, "UseQHarassGap", true);

        if (castQPass || castQPrePass || castQGap) {
            const TargetMode mode = GetTargetingMode();
            if (mode == TargetMode::Normal) {
                std::vector<AIHeroClient> sortedEnemies = GameObjects::EnemyHeroes();
                std::sort(sortedEnemies.begin(), sortedEnemies.end(), [player](const AIHeroClient& a, const AIHeroClient& b) {
                    return player.Position().DistanceSqr(a.Position()) < player.Position().DistanceSqr(b.Position());
                });
                for (const auto& enemy : sortedEnemies) {
                    if (!ValidHeroTarget(enemy)) continue;
                    auto status = GetPassiveStatus(enemy, 0.0f);
                    if (status.HasPassive && !(SDK::Core::Utils::AutoAttack::InAutoAttackRange(enemy) && status.PassivePredictedPositions.size() > 0 &&
                        InTheCone(player.Position().To2D(), status.TargetPredictedPosition, status.PassivePredictedPositions.front(), 90.0))) {
                        if (castQPass && status.Type == PassiveType::UltiPassive && castQtoUltPassive(enemy, getQPassivedelay(enemy)))
                            break;
                        if (castQPass && status.Type == PassiveType::NormalPassive && castQtoPassive(enemy, getQPassivedelay(enemy)))
                            break;
                        if (castQPrePass && status.Type == PassiveType::PrePassive && castQtoPrePassive(enemy, getQPassivedelay(enemy)))
                            break;
                        if (castQGap && castQtoGapClose(enemy, getQGapClosedelay(enemy)))
                            break;
                    }
                }
            } else {
                auto enemy = GetFioraTarget();
                if (ValidHeroTarget(enemy)) {
                    auto status = GetPassiveStatus(enemy, 0.0f);
                    if (status.HasPassive && !(SDK::Core::Utils::AutoAttack::InAutoAttackRange(enemy) && status.PassivePredictedPositions.size() > 0 &&
                        InTheCone(player.Position().To2D(), status.TargetPredictedPosition, status.PassivePredictedPositions.front(), 90.0))) {
                        if (castQPass && status.Type == PassiveType::UltiPassive && castQtoUltPassive(enemy, getQPassivedelay(enemy)))
                            return;
                        if (castQPass && status.Type == PassiveType::NormalPassive && castQtoPassive(enemy, getQPassivedelay(enemy)))
                            return;
                        if (castQPrePass && status.Type == PassiveType::PrePassive && castQtoPrePassive(enemy, getQPassivedelay(enemy)))
                            return;
                        if (castQGap && castQtoGapClose(enemy, getQGapClosedelay(enemy)))
                            return;
                    }
                }
            }
        }
    }
}

// ============================================================================
// Event Hook Handlers
// ============================================================================

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) return;

    // Automatic QSS / Mercurial cleanse
    if (SDK::HasBuffOfType(player, SDK::BuffType::Stun) ||
        SDK::HasBuffOfType(player, SDK::BuffType::Snare) ||
        SDK::HasBuffOfType(player, SDK::BuffType::Charm) ||
        SDK::HasBuffOfType(player, SDK::BuffType::Fear) ||
        SDK::HasBuffOfType(player, SDK::BuffType::Taunt) ||
        SDK::HasBuffOfType(player, SDK::BuffType::Suppression) ||
        SDK::HasBuffOfType(player, SDK::BuffType::Asleep)) {
        if (SDK::Items::CanUseItem(player, SDK::ItemId::Quicksilver_Sash)) {
            SDK::Items::UseItem(player, SDK::ItemId::Quicksilver_Sash);
        } else if (SDK::Items::CanUseItem(player, SDK::ItemId::Mercurial_Scimitar)) {
            SDK::Items::UseItem(player, SDK::ItemId::Mercurial_Scimitar);
        }
    }

    FioraPassiveUpdate();
    RunWallJump();

    if (IsComboMode()) {
        Combo();
    } else if (IsHarassMode()) {
        Harass();
    }
}

static void OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || !W.IsReady() || !Bool(EvadeOthersMenu, "W", true)) return;

    if (!args.Sender.IsValid()) return;

    if (args.Sender.Ptr == player.Address()) {
        if (_stricmp(args.SpellName, "FioraE") == 0 || _stricmp(args.SpellName, "ItemTitanicHydraCleave") == 0) {
            Orbwalker::ResetAutoAttackTimer();
        }
        return;
    }

    if (SDK::ChampionIdFromName(args.Sender.CharacterName) == SDK::ChampionId::Jax &&
        args.Slot == static_cast<int>(SpellSlot::E)) {
        Menu* charSub = EvadeOthersMenu->GetSubMenu("jax");
        bool enabled = charSub ? Bool(charSub, "JaxE", false) : false;
        if (enabled && player.Position().Distance(args.Sender.Position) <= player.BoundingRadius() + 875.0f) {
            SolveInstantBlock();
        }
    }
}

static void OnDoCast(const Events::ProcessSpellEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || !W.IsReady() || !Bool(EvadeOthersMenu, "W", true)) return;

    if (!args.Sender.IsValid() || args.Sender.Team == static_cast<uint32_t>(player.Team()) || args.Sender.Type != ::Core::Objects::ObjectType::AIHeroClient) {
        return;
    }

    if (player.Position().Distance(args.Sender.Position) > W.Range) {
        return;
    }

    const SDK::ChampionId championId =
        SDK::ChampionIdFromName(args.Sender.CharacterName);
    const char* championName = SDK::ChampionName(championId);
    if (championId == SDK::ChampionId::Unknown || !championName[0]) return;
    std::string charLower = ToLower(championName);

    Menu* targetNoneCharSub = EvadeTargetNoneMenu ? EvadeTargetNoneMenu->GetSubMenu(charLower.c_str()) : nullptr;
    if (targetNoneCharSub && Bool(EvadeTargetNoneMenu, "W", true)) {
        for (const auto& spell : TargetedNoneEvadeSpells) {
            if (spell.Champion == championId) {
                std::string key = std::string(SDK::ChampionName(spell.Champion)) + SpellSlotToString(spell.Slot);
                bool enabled = Bool(targetNoneCharSub, key.c_str(), false);
                if (enabled && args.Target.IsValid() && args.Target.NetworkId == static_cast<uint32_t>(player.NetworkId())) {
                    if (spell.IsDash) {
                        DetectedDashes.push_back({ AIHeroClient(args.Sender.Ptr), spell.DistanceDash, SDK::Variables::TickCount() });
                    } else {
                        SolveInstantBlock();
                        return;
                    }
                }
            }
        }
    }

    Menu* othersCharSub = EvadeOthersMenu ? EvadeOthersMenu->GetSubMenu(charLower.c_str()) : nullptr;
    if (othersCharSub && Bool(EvadeOthersMenu, "W", true)) {
        for (const auto& spell : OtherEvadeSpells) {
            if (spell.Champion == championId) {
                std::string key = std::string(SDK::ChampionName(spell.Champion)) + SpellSlotToString(spell.Slot);
                bool enabled = Bool(othersCharSub, key.c_str(), false);
                if (enabled) {
                    if (args.IsAutoAttack && args.Target.IsValid() && args.Target.NetworkId == static_cast<uint32_t>(player.NetworkId())) {
                        if (championId == SDK::ChampionId::Jax && spell.Slot == SpellSlot::W && args.Sender.IsVisible) {
                            SolveInstantBlock();
                            return;
                        }
                        if (championId == SDK::ChampionId::Yorick && spell.Slot == SpellSlot::Q && args.Sender.IsVisible) {
                            SolveInstantBlock();
                            return;
                        }
                        if (championId == SDK::ChampionId::Poppy && spell.Slot == SpellSlot::Q && args.Sender.IsVisible) {
                            SolveInstantBlock();
                            return;
                        }
                        if (championId == SDK::ChampionId::Rengar && spell.Slot == SpellSlot::Q && args.Sender.IsVisible) {
                            SolveInstantBlock();
                            return;
                        }
                        if (championId == SDK::ChampionId::Nautilus && spell.Slot == SpellSlot::Unknown && !player.HasBuff("nautiluspassivecheck")) {
                            SolveInstantBlock();
                            return;
                        }
                        if (championId == SDK::ChampionId::Udyr && spell.Slot == SpellSlot::E && !player.HasBuff("udyrbearstuncheck")) {
                            SolveInstantBlock();
                            return;
                        }
                    }
                    float dist = player.Position().Distance2D(args.Sender.Position);
                    if (championId == SDK::ChampionId::Riven && spell.Slot == SpellSlot::W && dist <= player.BoundingRadius() + args.Sender.Position.To2D().Distance(player.Position().To2D()) + 100.0f) {
                        SolveInstantBlock();
                        return;
                    }
                    if (championId == SDK::ChampionId::Diana && spell.Slot == SpellSlot::E && dist <= player.BoundingRadius() + 450.0f) {
                        SolveInstantBlock();
                        return;
                    }
                    if (championId == SDK::ChampionId::Maokai && spell.Slot == SpellSlot::R && _stricmp(args.SpellName, "maokaidrain3toggle") == 0 && dist <= player.BoundingRadius() + 575.0f) {
                        SolveInstantBlock();
                        return;
                    }
                    if (championId == SDK::ChampionId::Kalista && spell.Slot == SpellSlot::E && dist <= 950.0f && player.HasBuff("kalistaexpungemarker")) {
                        SolveInstantBlock();
                        return;
                    }
                    if (championId == SDK::ChampionId::Kennen && spell.Slot == SpellSlot::W && dist <= 800.0f && player.HasBuff("kennenmarkofstorm") && player.GetBuffCount("kennenmarkofstorm") == 2) {
                        SolveInstantBlock();
                        return;
                    }
                    if (championId == SDK::ChampionId::Tryndamere && spell.Slot == SpellSlot::W && dist <= player.BoundingRadius() + 850.0f) {
                        SolveInstantBlock();
                        return;
                    }
                    if (championId == SDK::ChampionId::Sett && spell.Slot == SpellSlot::E && dist <= player.BoundingRadius() + 490.0f) {
                        SolveInstantBlock();
                        return;
                    }
                    if (championId == SDK::ChampionId::Lissandra && spell.Slot == SpellSlot::W && dist <= player.BoundingRadius() + 490.0f) {
                        SolveInstantBlock();
                        return;
                    }
                    if (championId == SDK::ChampionId::Sett && spell.Slot == SpellSlot::R && args.Target.NetworkId == static_cast<uint32_t>(player.NetworkId())) {
                        SolveInstantBlock();
                        return;
                    }
                    if (championId == SDK::ChampionId::Camille && spell.Slot == SpellSlot::R && args.Target.NetworkId == static_cast<uint32_t>(player.NetworkId())) {
                        SolveInstantBlock();
                        return;
                    }
                    if (championId == SDK::ChampionId::Tristana && spell.Slot == SpellSlot::R && args.Target.NetworkId == static_cast<uint32_t>(player.NetworkId())) {
                        SolveInstantBlock();
                        return;
                    }
                    if (championId == SDK::ChampionId::Lulu) {
                        if (spell.Slot == SpellSlot::W && args.Target.NetworkId == static_cast<uint32_t>(player.NetworkId())) {
                            SolveInstantBlock();
                            return;
                        }
                        if (spell.Slot == SpellSlot::R && player.Position().Distance2D(args.Target.Position) <= player.BoundingRadius() + 150.0f) {
                            SolveInstantBlock();
                            return;
                        }
                    }
                    if (championId == SDK::ChampionId::Qiyana && spell.Slot == SpellSlot::R && dist <= player.BoundingRadius() + 350.0f) {
                        SDK::Polygon rect;
                        Vector2 start = args.Sender.Position.To2D();
                        Vector2 end = args.EndPosition.To2D();
                        Vector2 dir = (end - start).Normalized();
                        Vector2 left = Vector2(-dir.y, dir.x) * 140.0f;
                        Vector2 right = Vector2(dir.y, -dir.x) * 140.0f;
                        rect.Add(start + left); rect.Add(start + right);
                        rect.Add(end + right); rect.Add(end + left);
                        if (rect.IsInside(player.Position().To2D())) {
                            SolveInstantBlock();
                            return;
                        }
                    }
                    if (championId == SDK::ChampionId::Jax && spell.Slot == SpellSlot::E && player.Position().Distance(args.Sender.Position) <= player.BoundingRadius() + 875.0f && args.Sender.IsVisible) {
                        SolveInstantBlock();
                        return;
                    }
                    if (championId == SDK::ChampionId::Yone) {
                        if (spell.Slot == SpellSlot::Q && _stricmp(args.SpellName, "YoneQ3") == 0 && dist <= player.BoundingRadius() + 500.0f) {
                            SolveInstantBlock();
                            return;
                        }
                        if (spell.Slot == SpellSlot::R && dist <= player.BoundingRadius() + 1000.0f) {
                            SolveInstantBlock();
                            return;
                        }
                    }
                    if (championId == SDK::ChampionId::Sylas && spell.Slot == SpellSlot::E && _stricmp(args.SpellName, "SylasE2") == 0 && dist <= player.BoundingRadius() + 800.0f) {
                        SolveInstantBlock();
                        return;
                    }
                }
            }
        }
    }
}

static void OnPlayAnimation(const Events::PlayAnimationEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || !W.IsReady() || !Bool(EvadeOthersMenu, "W", true)) return;

    if (!args.Sender.IsValid() || args.Sender.Type != ::Core::Objects::ObjectType::AIHeroClient || args.Sender.Team == static_cast<uint32_t>(player.Team())) {
        return;
    }

    const SDK::ChampionId championId =
        SDK::ChampionIdFromName(args.Sender.CharacterName);
    const char* championName = SDK::ChampionName(championId);
    if (championId == SDK::ChampionId::Unknown || !championName[0]) return;
    std::string charLower = ToLower(championName);

    if (championId == SDK::ChampionId::Riven && _stricmp(args.Animation, "spell1c") == 0) {
        Menu* charSub = EvadeOthersMenu->GetSubMenu("riven");
        if (charSub && Bool(charSub, "RivenQ", false)) {
            RivenQ3Tick = SDK::Variables::TickCount();
            AIHeroClient rivenHero(args.Sender.Ptr);
            RivenQ3Rad = (rivenHero.IsValid() && rivenHero.HasBuff("RivenFengShuiEngine")) ? 225.0f : 150.0f;
        }
    }

    Menu* othersCharSub = EvadeOthersMenu ? EvadeOthersMenu->GetSubMenu(charLower.c_str()) : nullptr;
    if (othersCharSub) {
        for (const auto& spell : OtherEvadeSpells) {
            if (spell.Champion == championId) {
                std::string key = std::string(SDK::ChampionName(spell.Champion)) + SpellSlotToString(spell.Slot);
                bool enabled = Bool(othersCharSub, key.c_str(), false);
                if (enabled) {
                    if (championId == SDK::ChampionId::RekSai && spell.Slot == SpellSlot::W && _stricmp(args.Animation, "Spell2_knockup") == 0) {
                        if (!player.HasBuff("reksaiknockupimmune") && player.Position().Distance2D(args.Sender.Position) <= player.BoundingRadius() + 100.0f) {
                            SolveInstantBlock();
                        }
                    }
                }
            }
        }
    }
}

static void Unit_OnDash(const SDK::Events::Dash::DashArgs& args) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || !W.IsReady() || !Bool(EvadeOthersMenu, "W", true)) return;

    if (!args.IsDash || args.NetworkId == static_cast<uint32_t>(player.NetworkId())) return;

    AIHeroClient caster(args.Unit);
    if (!caster.IsValid() || caster.Team() == player.Team()) return;

    const SDK::ChampionId casterChampionId =
        SDK::ChampionIdFromName(caster.CharacterName().c_str());
    if (casterChampionId == SDK::ChampionId::Riven) {
        Menu* charSub = EvadeOthersMenu->GetSubMenu("riven");
        if (charSub && Bool(charSub, "RivenQ", false)) {
            RivenDashTick = SDK::Variables::TickCount();
            RivenDashEnd = args.EndPos.To2D();
        }
    }
}

static void OnObjectCreate(const GameObject& object) {
    if (!object.IsValid()) return;
    std::string name = GetObjectName(object);
    if (name == "Fizz_UltimateMissile_Orbit.troy" && FizzFishEndPos.IsValid()) {
        if (object.Position().To2D().Distance(FizzFishEndPos) <= 300.0f) {
            FizzFishChum = object;
            FizzFishChumStartTick = SDK::Variables::TickCount();
        }
    }
}

static void OnObjectDelete(const GameObject& object) {
    if (!object.IsValid()) return;
    if (object.Type() == ::Core::Objects::ObjectType::MissileClient) {
        MissileClient missile(object.Handle());
        if (missile.IsValid()) {
            auto caster = GameObjects::GetUnitByNetworkId<AIBaseClient>(missile.CasterNetworkId());
            if (caster.IsValid() && caster.Team() != Player().Team() && GetObjectName(missile) == "FizzMarinerDoomMissile") {
                FizzFishEndPos = missile.Position().To2D();
            }
        }
        int netId = object.NetworkId();
        DetectedTargets.erase(
            std::remove_if(DetectedTargets.begin(), DetectedTargets.end(), [netId](const DetectedTarget& t) {
                return t.Obj.NetworkId() == netId;
            }),
            DetectedTargets.end()
        );
    }
}

static void OnWndProc(Game::WndEventArgs& args) {
    if (Game::IsChatOpen()) return;
    const TargetMode mode = GetTargetingMode();
    if (mode == TargetMode::Optional) {
        if (args.Msg == WM_KEYDOWN) {
            const auto* item = OptionalModeMenu ? OptionalModeMenu->Get<MenuKeyBind>("OptionalSwitchTargetKey") : nullptr;
            uint32_t switchKey = item ? static_cast<uint32_t>(item->Key) : 0x54; // default 'T'
            if (args.WParam == switchKey) {
                OptionalTarget = GetOptionalTarget();
                const auto player = Player();
                if (!OptionalTarget.IsValid()) {
                    AIHeroClient best;
                    float bestDist = FLT_MAX;
                    for (const auto& enemy : GameObjects::EnemyHeroes()) {
                        if (!ValidHeroTarget(enemy, OptionalRange())) continue;
                        bool matchOld = OldOptionalTarget.IsValid() && enemy.NetworkId() == OldOptionalTarget.NetworkId();
                        float dist = player.Position().Distance(enemy.Position());
                        if (matchOld) dist -= 100000.0f;
                        if (dist < bestDist) {
                            best = enemy;
                            bestDist = dist;
                        }
                    }
                    if (best.IsValid()) {
                        OptionalTarget = best;
                    }
                    return;
                }
                AIHeroClient best;
                float bestDist = FLT_MAX;
                for (const auto& enemy : GameObjects::EnemyHeroes()) {
                    if (!ValidHeroTarget(enemy, OptionalRange()) || enemy.NetworkId() == OptionalTarget.NetworkId()) continue;
                    bool matchOld = OldOptionalTarget.IsValid() && enemy.NetworkId() == OldOptionalTarget.NetworkId();
                    float dist = player.Position().Distance(enemy.Position());
                    if (matchOld) dist -= 100000.0f;
                    if (dist < bestDist) {
                        best = enemy;
                        bestDist = dist;
                    }
                }
                if (best.IsValid()) {
                    OldOptionalTarget = OptionalTarget;
                    OptionalTarget = best;
                }
                return;
            }
        }
        if (args.Msg == WM_LBUTTONDOWN) {
            OptionalTarget = GetOptionalTarget();
            float oRange = OptionalRange();
            const auto cursor = SDK::Game::CursorPosition();
            if (!OptionalTarget.IsValid()) {
                AIHeroClient best;
                float bestCursorDist = FLT_MAX;
                for (const auto& enemy : GameObjects::EnemyHeroes()) {
                    if (!ValidHeroTarget(enemy, oRange) || enemy.Position().Distance(cursor) > 400.0f) continue;
                    float dist = cursor.Distance(enemy.Position());
                    if (dist < bestCursorDist) {
                        best = enemy;
                        bestCursorDist = dist;
                    }
                }
                if (best.IsValid()) {
                    OptionalTarget = best;
                }
                return;
            }
            AIHeroClient best;
            float bestCursorDist = FLT_MAX;
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (!ValidHeroTarget(enemy, oRange) || enemy.Position().Distance(cursor) > 400.0f) continue;
                float dist = cursor.Distance(enemy.Position());
                if (dist < bestCursorDist) {
                    best = enemy;
                    bestCursorDist = dist;
                }
            }
            if (best.IsValid()) {
                OldOptionalTarget = OptionalTarget;
                OptionalTarget = best;
            }
            return;
        }
    }
}

static void OnAfterAttack(OrbwalkingActionArgs& args) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) return;

    if (IsComboMode()) {
        if (Bool(ComboMenu, "UseECombo", true) && E.IsReady()) {
            if (E.Cast()) {
                Orbwalker::ResetAutoAttackTimer();
            }
        } else if (HasActiveItem()) {
            CastActiveItem();
        }
    } else if (IsHarassMode() && args.Target.IsValid() && args.Target.Type() == ::Core::Objects::ObjectType::AIHeroClient) {
        if (Bool(HarassMenu, "UseEHarass", true) && E.IsReady() && player.ManaPercent() >= static_cast<float>(Slider(HarassMenu, "ManaHarass", 40))) {
            if (E.Cast()) {
                Orbwalker::ResetAutoAttackTimer();
            }
        } else if (HasActiveItem()) {
            CastActiveItem();
        }
    } else if (IsClearMode()) {
        float jungleMana = static_cast<float>(Slider(JungleClearMenu, "minimumManaJC", 40));
        if (Bool(JungleClearMenu, "UseEJClear", true) && E.IsReady() && player.ManaPercent() >= jungleMana &&
            CountMinionsInRange(player.Position(), player.AttackRange() + 200.0f, true) >= 1) {
            if (E.Cast()) {
                Orbwalker::ResetAutoAttackTimer();
            }
        } else if (Bool(JungleClearMenu, "UseTimatJClear", true) && HasActiveItem() &&
                   CountMinionsInRange(player.Position(), player.AttackRange() + 200.0f, true) >= 1) {
            CastActiveItem();
        }
        float laneMana = static_cast<float>(Slider(LaneClearMenu, "minimumManaLC", 40));
        if (Bool(LaneClearMenu, "UseELClear", true) && E.IsReady() && player.ManaPercent() >= laneMana &&
            CountMinionsInRange(player.Position(), player.AttackRange() + 200.0f, false) >= 1) {
            if (E.Cast()) {
                Orbwalker::ResetAutoAttackTimer();
            }
        } else if (Bool(LaneClearMenu, "UseTimatLClear", true) && HasActiveItem() &&
                   CountMinionsInRange(player.Position(), player.AttackRange() + 200.0f, false) >= 1) {
            CastActiveItem();
        }
    }
}

// ============================================================================
// Drawings
// ============================================================================

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) return;

    if (Bool(DrawMenu, "DrawQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), 400.0f, 0xFF00FF00u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawW", false) && W.IsReady()) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFF00FF00u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawOptionalRange", true) && GetTargetingMode() == TargetMode::Optional) {
        Drawing::DrawCircle(player.Position(), OptionalRange(), 0xFFFF1493u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawSelectedRange", true) && GetTargetingMode() == TargetMode::Selected) {
        Drawing::DrawCircle(player.Position(), SelectedRange(), 0xFFFF1493u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawPriorityRange", true) && GetTargetingMode() == TargetMode::Priority) {
        Drawing::DrawCircle(player.Position(), PriorityRange(), 0xFFFF1493u, 1.5f, 64);
    }
    if (Bool(DrawMenu, "DrawTarget", true) && GetTargetingMode() != TargetMode::Normal) {
        auto target = GetFioraTarget();
        if (target.IsValid() && !target.IsDead()) {
            Drawing::DrawCircle(target.Position(), 75.0f, 0xFFFFFF00u, 5.0f, 64);
        }
    }
    if (Bool(DrawMenu, "DrawVitals", false) && GetTargetingMode() != TargetMode::Normal) {
        auto target = GetFioraTarget();
        if (target.IsValid() && !target.IsDead()) {
            auto status = GetPassiveStatus(target, 0.0f);
            if (status.HasPassive && !status.PassivePredictedPositions.empty()) {
                for (const auto& pt : status.PassivePredictedPositions) {
                    Drawing::DrawCircle(Vector3::From2D(pt), 50.0f, 0xFFFFFF00u, 1.5f, 64);
                }
            }
        }
    }
}



static void BuildMenu() {
    MenuRoot = new Menu("champion.kuroaio.fiora", "Kuro - Fiora", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo Settings"));
    ComboMenu->Add(new MenuBool("UseQCombo", "Q Enable", true));
    ComboMenu->Add(new MenuBool("UseQComboGap", "Use Q to gapclose", true));
    ComboMenu->Add(new MenuBool("UseQComboPrePass", "Use Q to hit pre-passive spot", true));
    ComboMenu->Add(new MenuBool("UseQComboPass", "Use Q to hit passive", true));
    ComboMenu->Add(new MenuBool("UseQComboGapMinion", "Use Q minion to gapclose", false));
    ComboMenu->Add(new MenuSlider("UseQComboGapMinionValue", "Q minion gapclose if % cdr>=", 25, 0, 40));
    ComboMenu->Add(new MenuBool("UseECombo", "E Enable", true));
    ComboMenu->Add(new MenuBool("UseRCombo", "R Enable", true));
    ComboMenu->Add(new MenuBool("UseRComboLowHP", "Use R Low HP", true));
    ComboMenu->Add(new MenuSlider("UseRComboLowHPValue", "R Low HP if player hp<", 40, 0, 100));
    ComboMenu->Add(new MenuBool("UseRComboKillable", "Use R Killable", true));
    ComboMenu->Add(new MenuBool("UseRComboOnTap", "Use R on Tap", true));
    ComboMenu->Add(new MenuKeyBind("UseRComboOnTapKey", "R on Tap key", SDK::Keys::G, KeyBindType::Press))->Permashow();
    ComboMenu->Add(new MenuBool("UseRComboAlways", "Use R Always", false));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass Settings"));
    HarassMenu->Add(new MenuBool("UseQHarass", "Q Enable", true));
    HarassMenu->Add(new MenuBool("UseQHarassGap", "Use Q to gapclose", true));
    HarassMenu->Add(new MenuBool("UseQHarassPrePass", "Use Q to hit pre-passive spot", true));
    HarassMenu->Add(new MenuBool("UseQHarassPass", "Use Q to hit passive", true));
    HarassMenu->Add(new MenuBool("UseEHarass", "E Enable", true));
    HarassMenu->Add(new MenuSlider("ManaHarass", "Mana Harass", 40, 0, 100));

    TargetMenu = MenuRoot->AddSubMenu(new Menu("TargetingModes", "Targeting Modes"));
    TargetMenu->Add(new MenuList("TargetingMode", "Targeting Mode", { "Optional", "Selected", "Priority", "Normal" }, 3));
    TargetMenu->Add(new MenuSlider("OrbwalkToPassiveRange", "Orbwalk To Passive Range", 300, 250, 500));
    TargetMenu->Add(new MenuBool("FocusUltedTarget", "Focus Ulted Target", false));

    PriorityModeMenu = TargetMenu->AddSubMenu(new Menu("Priority", "Priority Mode"));
    PriorityModeMenu->Add(new MenuSlider("PriorityRange", "Priority Range", 1000, 300, 1000));
    PriorityModeMenu->Add(new MenuBool("PriorityOrbwalktoPassive", "Orbwalk to Passive", true));
    PriorityModeMenu->Add(new MenuBool("PriorityUnderTower", "Under Tower", true));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const std::string runtimeName = GetObjectCharacterName(enemy);
        const SDK::ChampionId championId =
            SDK::ChampionIdFromName(runtimeName.c_str());
        const char* championName = SDK::ChampionName(championId);
        if (championId == SDK::ChampionId::Unknown || !championName[0]) continue;
        std::string key = "Priority" + std::string(championName);
        std::string label = championName;
        PriorityModeMenu->Add(new MenuSlider(key.c_str(), label.c_str(), 2, 1, 5));
    }

    OptionalModeMenu = TargetMenu->AddSubMenu(new Menu("Optional", "Optional Mode"));
    OptionalModeMenu->Add(new MenuSlider("OptionalRange", "Optional Range", 1000, 300, 1000));
    OptionalModeMenu->Add(new MenuBool("OptionalOrbwalktoPassive", "Orbwalk to Passive", true));
    OptionalModeMenu->Add(new MenuBool("OptionalUnderTower", "Under Tower", false));
    OptionalModeMenu->Add(new MenuKeyBind("OptionalSwitchTargetKey", "Switch Target Key", SDK::Keys::T, KeyBindType::Press))->Permashow();

    SelectedModeMenu = TargetMenu->AddSubMenu(new Menu("Selected", "Selected Mode"));
    SelectedModeMenu->Add(new MenuSlider("SelectedRange", "Selected Range", 1000, 300, 1000));
    SelectedModeMenu->Add(new MenuBool("SelectedOrbwalktoPassive", "Orbwalk to Passive", true));
    SelectedModeMenu->Add(new MenuBool("SelectedUnderTower", "Under Tower", false));
    SelectedModeMenu->Add(new MenuBool("SelectedSwitchIfNoSelected", "Switch to Optional if no target", true));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("UseELClear", "E Enable", true));
    LaneClearMenu->Add(new MenuBool("UseTimatLClear", "Use Item", true));
    LaneClearMenu->Add(new MenuSlider("minimumManaLC", "minimum Mana", 40, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("JungleClear", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("UseEJClear", "E Enable", true));
    JungleClearMenu->Add(new MenuBool("UseTimatJClear", "Use Item", true));
    JungleClearMenu->Add(new MenuSlider("minimumManaJC", "minimum Mana", 40, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc", "Misc Settings"));
    MiscMenu->Add(new MenuBool("WallJump", "Wall Jump Enable", true));
    MiscMenu->Add(new MenuKeyBind("WallJumpKey", "Wall Jump Key", SDK::Keys::H, KeyBindType::Press))->Permashow();

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw", "Drawings"));
    DrawMenu->Add(new MenuBool("DrawQ", "Draw Q", false));
    DrawMenu->Add(new MenuBool("DrawW", "Draw W", false));
    DrawMenu->Add(new MenuBool("DrawOptionalRange", "Draw Optional Range", true));
    DrawMenu->Add(new MenuBool("DrawSelectedRange", "Draw Selected Range", true));
    DrawMenu->Add(new MenuBool("DrawPriorityRange", "Draw Priority Range", true));
    DrawMenu->Add(new MenuBool("DrawTarget", "Draw Target", true));
    DrawMenu->Add(new MenuBool("DrawVitals", "Draw Vitals", false));

    EvadeTargetMenu = MenuRoot->AddSubMenu(new Menu("EvadeTarget", "Evade Targeted Missile"));
    EvadeTargetMenu->Add(new MenuBool("W", "Use W", true));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const std::string runtimeName = GetObjectCharacterName(enemy);
        const SDK::ChampionId championId =
            SDK::ChampionIdFromName(runtimeName.c_str());
        const char* championName = SDK::ChampionName(championId);
        if (championId == SDK::ChampionId::Unknown || !championName[0]) continue;
        bool hasSpell = false;
        for (const auto& spell : EvadeTargetSpells) {
            if (spell.Champion == championId) {
                hasSpell = true;
                break;
            }
        }
        if (hasSpell) {
            std::string subMenuId = ToLower(championName);
            std::string subMenuLabel = "-> " + std::string(championName);
            Menu* charSub = EvadeTargetMenu->AddSubMenu(new Menu(subMenuId.c_str(), subMenuLabel.c_str()));
            for (const auto& spell : EvadeTargetSpells) {
                if (spell.Champion == championId) {
                    std::string key = spell.MissileName;
                    std::string label = spell.MissileName + " (" + SpellSlotToString(spell.Slot) + ")";
                    charSub->Add(new MenuBool(key.c_str(), label.c_str(), false));
                }
            }
        }
    }

    EvadeOthersMenu = MenuRoot->AddSubMenu(new Menu("EvadeOthers", "Block Other Skills"));
    EvadeOthersMenu->Add(new MenuBool("W", "Use W", true));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const std::string runtimeName = GetObjectCharacterName(enemy);
        const SDK::ChampionId championId =
            SDK::ChampionIdFromName(runtimeName.c_str());
        const char* championName = SDK::ChampionName(championId);
        if (championId == SDK::ChampionId::Unknown || !championName[0]) continue;
        bool hasSpell = false;
        for (const auto& spell : OtherEvadeSpells) {
            if (spell.Champion == championId) {
                hasSpell = true;
                break;
            }
        }
        if (hasSpell) {
            std::string subMenuId = ToLower(championName);
            std::string subMenuLabel = "-> " + std::string(championName);
            Menu* charSub = EvadeOthersMenu->AddSubMenu(new Menu(subMenuId.c_str(), subMenuLabel.c_str()));
            for (const auto& spell : OtherEvadeSpells) {
                if (spell.Champion == championId) {
                    const char* spellChampionName = SDK::ChampionName(spell.Champion);
                    std::string key = std::string(spellChampionName) + SpellSlotToString(spell.Slot);
                    std::string label = std::string(spellChampionName) + " (" + SpellSlotToString(spell.Slot) + ")";
                    charSub->Add(new MenuBool(key.c_str(), label.c_str(), false));
                }
            }
        }
    }

    EvadeTargetNoneMenu = MenuRoot->AddSubMenu(new Menu("EvadeTargetNone", "Evade Targeted None-Missile"));
    EvadeTargetNoneMenu->Add(new MenuBool("W", "Use W", true));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const std::string runtimeName = GetObjectCharacterName(enemy);
        const SDK::ChampionId championId =
            SDK::ChampionIdFromName(runtimeName.c_str());
        const char* championName = SDK::ChampionName(championId);
        if (championId == SDK::ChampionId::Unknown || !championName[0]) continue;
        bool hasSpell = false;
        for (const auto& spell : TargetedNoneEvadeSpells) {
            if (spell.Champion == championId) {
                hasSpell = true;
                break;
            }
        }
        if (hasSpell) {
            std::string subMenuId = ToLower(championName);
            std::string subMenuLabel = "-> " + std::string(championName);
            Menu* charSub = EvadeTargetNoneMenu->AddSubMenu(new Menu(subMenuId.c_str(), subMenuLabel.c_str()));
            for (const auto& spell : TargetedNoneEvadeSpells) {
                if (spell.Champion == championId) {
                    const char* spellChampionName = SDK::ChampionName(spell.Champion);
                    std::string key = std::string(spellChampionName) + SpellSlotToString(spell.Slot);
                    std::string label = std::string(spellChampionName) + " (" + SpellSlotToString(spell.Slot) + ")";
                    charSub->Add(new MenuBool(key.c_str(), label.c_str(), false));
                }
            }
        }
    }

    MenuRoot->Attach();
}

static void RemoveMenu() {
    if (!MenuRoot) return;

    if (auto* item = ComboMenu ? ComboMenu->Get<MenuKeyBind>("UseRComboOnTapKey") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = OptionalModeMenu ? OptionalModeMenu->Get<MenuKeyBind>("OptionalSwitchTargetKey") : nullptr) {
        item->RemovePermashow();
    }
    if (auto* item = MiscMenu ? MiscMenu->Get<MenuKeyBind>("WallJumpKey") : nullptr) {
        item->RemovePermashow();
    }

    MenuManager::Instance().Remove(MenuRoot);
    MenuRoot = nullptr;
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) return;

    Q = Spell(SpellSlot::Q, 400.0f);
    W = Spell(SpellSlot::W, 750.0f);
    W.SetSkillshot(0.75f, 80.0f, 2000.0f, false, SkillshotType::SkillshotLine);
    E = Spell(SpellSlot::E, 0.0f);
    R = Spell(SpellSlot::R, 500.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Events::hook.OnDoCast += &OnDoCast;
    Events::hook.OnPlayAnimation += &OnPlayAnimation;
    Events::hook.OnDash += &Unit_OnDash;
    GameObjects::AddOnCreate(&OnObjectCreate);
    GameObjects::AddOnDelete(&OnObjectDelete);
    Game::OnWndProc += &OnWndProc;
    Orbwalker::OnAfterAttack += &OnAfterAttack;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>Kuro - Fiora loaded</font>");
}

static void OnUnload() {
    if (!Loaded) return;

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Events::hook.OnDoCast -= &OnDoCast;
    Events::hook.OnPlayAnimation -= &OnPlayAnimation;
    Events::hook.OnDash -= &Unit_OnDash;
    GameObjects::RemoveOnCreate(&OnObjectCreate);
    GameObjects::RemoveOnDelete(&OnObjectDelete);
    Game::OnWndProc -= &OnWndProc;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;
    Drawing::OnDraw -= &OnDraw;

    FioraPrePassiveObjects.clear();
    FioraPassiveObjects.clear();
    FioraUltiPassiveObjects.clear();
    DetectedTargets.clear();
    DetectedDashes.clear();

    RemoveMenu();
    Loaded = false;
}

} // namespace Plugins::KuroAIO::Fiora
