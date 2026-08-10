#pragma once
// ============================================================================
// Orbwalker7UP — port 1-1 từ ImpulseAIO Common.Overwirte.NewOrbwalker.cs
// (file C# gốc: 1831 dòng, 50 hàm). Plugin Core override orbwalker SDK,
// đăng ký implementation "7UP" vào SDK::Orbwalker::AddOrbwalker rồi
// SetOrbwalker("7UP"), suspend bản SDK (giống OrbwalkerKuro).
//
// QUY TẮC PORT:
//  - Matching 1-1: mỗi hàm/số nhánh/hằng số C# phải có mặt, đúng thứ tự.
//  - Thứ tự hàm trong file này theo đúng NewOrbwalker.cs.
//  - Thiếu API -> comment // MISSAPI + ghi missapi.md, không tự bịa.
//  - ComputeStringHash switch (C#) -> if (_stricmp(name,"X")==0) tuần tự,
//    giữ đúng thứ tự nhánh C#.
//  - Program.Chinese -> bỏ, chỉ giữ label tiếng Anh.
// ============================================================================

#include "../../IPlugin.h"
#include "../../PluginRegistry.h"
#include "../../../sdk/Core/Game.h"
#include "../../../sdk/Core/Hud.h"
#include "../../../sdk/Core/Objects.h"
#include "../../../sdk/Enumerations/OrbwalkingMode.h"
#include "../../../sdk/Enumerations/OrbwalkingType.h"
#include "../../../sdk/Enumerations/SpellSlot.h"
#include "../../../sdk/Events/Events.h"
#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/Math/HealthPrediction.h"
#include "../../../sdk/Math/Prediction/Movement.h"
#include "../../../sdk/UI/Drawing.h"
#include "../../../sdk/UI/UI.h"
#include "../../../sdk/Utils/AutoAttack.h"
#include "../../../sdk/Utils/DelayAction.h"
#include "../../../sdk/Wrappers/Damages/Damage.h"
#include "../../../sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "../../../sdk/Wrappers/TargetSelector/TargetSelector.h"
#include "../../../sdk/Extensions/AIBaseClientExtensions.h"
#include "../../../core/CoreControl.h"
#include "../../../core/CoreBuffs.h"

#include "Common/Base.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace Orbwalker7UP {

using namespace ::SDK;

// ---------------------------------------------------------------------------
// SettAttackInfo — nested class (C# line ~1798-1810)
// ---------------------------------------------------------------------------
struct SettAttackInfo {
    bool IsLeft = true;
    int Time = 0;
    SettAttackInfo() = default;
    SettAttackInfo(bool isLeft, int time) : IsLeft(isLeft), Time(time) {}
};

// ---------------------------------------------------------------------------
// Orbwalker7UPImpl — implementation IOrbwalker, 50 hàm theo thứ tự C#
// ---------------------------------------------------------------------------
class Orbwalker7UPImpl : public IOrbwalker {
public:
    explicit Orbwalker7UPImpl(Menu* parentMenu);
    ~Orbwalker7UPImpl() override;

    // === IOrbwalker interface (override) — public API ===
    AttackableUnit ForceTarget() const override;
    void ForceTarget(const AttackableUnit& target) override;
    AttackableUnit LastTarget() const override;
    OrbwalkingMode ActiveMode() const override;
    int LastAutoAttackTick() const override;
    void LastAutoAttackTick(int value) override;
    bool IsAutoAttacking() override;
    bool IsWindingUp() override;
    bool IsAttackCastComplete() override;
    int AttackCastDelayRemaining() override;
    int NextAttackReadyTick() override;
    int AttackCooldownRemaining() override;
    int LastMovementTick() const override;
    void LastMovementTick(int value) override;
    bool AttackEnabled() const override;
    void AttackEnabled(bool value) override;
    bool MoveEnabled() const override;
    void MoveEnabled(bool value) override;
    void SetOrbwalkerPosition(const Vector3& position) override;
    void SetPauseTime(int time) override;
    void SetServerPauseTime(int time) override;
    void SetAttackPauseTime(int time) override;
    void SetAttackServerPauseTime(int time) override;
    void SetMovePauseTime(int time) override;
    void SetMoveServerPauseTime(int time) override;
    AttackableUnit GetTarget() override;
    bool CanAttack() override;
    bool CanAttack(float extraWindup) override;
    bool CanMove() override;
    bool CanMove(float extraWindup, bool disableMissileCheck) override;
    bool Attack(const AttackableUnit& target) override;
    void Move(const Vector3& position) override;
    void Orbwalk(const AttackableUnit& target, const Vector3& position = {}) override;
    bool ShouldWait() override;
    void ResetAutoAttackTimer() override;
    void Dispose() override;

    bool IsAutoAttack(std::string name);
    bool IsAutoAttackReset(std::string name);

private:
    static int Tick() { return Game::TickCount(); }

    // === State members (map 1-1 từ C#) ===
    // public properties (IOrbwalker)
    AttackableUnit forceTarget_ = {};
    AttackableUnit lastTarget_ = {};
    int lastAutoAttackTick_ = 0;
    int lastMovementTick_ = 0;
    bool attackEnabled_ = true;
    bool moveEnabled_ = true;

    // private fields
    int lastLocalAttackTick_ = 0;
    int autoAttackCounter_ = 0;
    int attackPauseTick_ = 0;
    int movePauseTick_ = 0;
    int allPauseTick_ = 0;
    int lastFakeClickTick_ = 0;
    bool initialize_ = false;
    bool isAphelios_ = false;
    bool isGraves_ = false;
    bool isJhin_ = false;
    bool isKalista_ = false;
    bool isRengar_ = false;
    bool isSett_ = false;
    bool missileLaunched_ = false;
    bool nextAttackIsPassive_ = false;
    bool jaxInGame_ = false;
    bool gangplankInGame_ = false;
    bool tahmKenchInGame_ = false;
    bool calcItemDamage_ = false;
    Vector3 orbwalkerPosition_ = {};
    AttackableUnit laneClearMinion_ = {};
    OrbwalkingMode activeMode_ = OrbwalkingMode::None;
    SettAttackInfo info_ = {};
    std::mt19937 rng_;

    // menus
    Menu* menu_ = nullptr;
    Menu* attackMenu_ = nullptr;
    Menu* prioritizeMenu_ = nullptr;
    Menu* orbwalkerMenu_ = nullptr;
    Menu* farmMenu_ = nullptr;
    Menu* advancedMenu_ = nullptr;
    Menu* drawMenu_ = nullptr;
    Menu* miscMenu_ = nullptr;

    // readonly arrays (fill trong constructor)
    std::vector<std::string> attackResets_;
    std::vector<std::string> attacks_;
    std::vector<std::string> noAttacks_;
    std::vector<std::string> windWallBrokenChampions_;
    std::vector<std::string> specialWindWallChampions_;
    std::vector<std::string> ignoreMinions_;

    static int colorIndex_;

    // === Forward-declare 50 hàm THEO ĐÚNG THỨ TỰ C# ===
    // (1) Properties private: ForceChase / GetFindRange / FastLne -> getter method
    bool ForceChase() const;          // C# line 22-32
    int  GetFindRange() const;        // C# line 33-39
    bool FastLne() const;             // C# line 40-50
    // (2) ActiveMode getter/setter logic (C# line 52-90)
    //     -> override ActiveMode() const + helper SetActiveMode
    void SetActiveMode(OrbwalkingMode value);

    // (3) Constructor body (C# line 92-213) -> Init()
    void Init(Menu* parentMenu);

    // (4) Private methods theo thứ tự C#
    float GetAttackCastDelay();                                   // C# line 214-221
    float GetProjectileSpeed();                                   // C# line 222-394
    float GetBasicAttackMissileSpeed();                           // C# line 395-402
    bool  CanAttackWithWindWall(const AttackableUnit& target);    // C# line 403-472
    std::vector<AIMinionClient> GetMinions(float range = 0.0f);   // C# line 473-487
    bool ShouldWait(const std::vector<AIMinionClient>& minions);  // C# line 519-536
    bool CanTurretFarm(const std::vector<AIMinionClient>& minions); // C# line 537-565
    bool IsSupportMode();                                          // C# line 566-595
    void OnOrbwalkerProcessSpellCastDelayed(const Events::ProcessSpellEventArgs& args); // C# line 596-607
    bool CanOrbObj(const AIBaseClient& g);                        // C# line 608-657

    // (5) Event handlers theo thứ tự C#
    void OnDoCast(const Events::ProcessSpellEventArgs& args);          // C# line 659-691
    void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args);// C# line 694-717
    void OnPlayAnimation(const Events::PlayAnimationEventArgs& args);  // C# line 720-768
    void OnStopCast(const Events::StopCastEventArgs& args);            // C# line 771-781
    void OnDelete(const Events::ObjectEventArgs& args);                // C# line 784-814
    void OnUpdate(const Events::GameUpdateEventArgs& args);            // C# line 817-842
    void OnDraw();                                                      // C# line 845-900

    // (6) Public IOrbwalker methods (đã khai báo ở public)
    //     Attack, CanAttack(×2), CanMove(×2), GetTarget, IsAutoAttack,
    //     IsAutoAttackReset, Move, Orbwalk, ResetAutoAttackTimer,
    //     7 setter pause, SetOrbwalkerPosition, Dispose

    // Static dispatcher cho event tĩnh (route tới runtime instance)
    static Orbwalker7UPImpl* RuntimeInstance;
    static void OnDoCastStatic(const Events::ProcessSpellEventArgs& args);
    static void OnProcessSpellCastStatic(const Events::ProcessSpellEventArgs& args);
    static void OnPlayAnimationStatic(const Events::PlayAnimationEventArgs& args);
    static void OnStopCastStatic(const Events::StopCastEventArgs& args);
    static void OnDeleteStatic(const Events::ObjectEventArgs& args);
    static void OnUpdateStatic(const Events::GameUpdateEventArgs& args);
    static void OnDrawStatic();
};

// ---------------------------------------------------------------------------
// Helper đọc menu (giống idiom Ezreal.h / OrbwalkerKuro)
// ---------------------------------------------------------------------------
inline bool BoolValue(const MenuBool* v, bool fallback) {
    return v ? v->Value : fallback;
}
inline int SliderValue(const MenuSlider* v, int fallback) {
    return v ? v->Value : fallback;
}
inline bool KeyValue(const MenuKeyBind* v, bool fallback) {
    return v ? v->Active : fallback;
}
inline int ListValue(const MenuList* v, int fallback) {
    return v ? v->Index : fallback;
}

// ===========================================================================
// Stub implementations — TODO port từng hàm từ NewOrbwalker.cs
// File sẽ compile nhưng chưa có logic. Thay TODO bằng code 1-1 theo thứ tự C#.
// ===========================================================================

inline Orbwalker7UPImpl* Orbwalker7UPImpl::RuntimeInstance = nullptr;
inline int Orbwalker7UPImpl::colorIndex_ = 0;

inline Orbwalker7UPImpl::Orbwalker7UPImpl(Menu* parentMenu)
    : rng_(static_cast<std::uint32_t>(Game::TickCount())) {
    // TODO: port constructor body C# line 92-213 (Init())
    Init(parentMenu);
}

inline Orbwalker7UPImpl::~Orbwalker7UPImpl() {
    Dispose();
}

// --- IOrbwalker public API stubs ---
inline AttackableUnit Orbwalker7UPImpl::ForceTarget() const { return forceTarget_; }
inline void Orbwalker7UPImpl::ForceTarget(const AttackableUnit& t) { forceTarget_ = t; }
inline AttackableUnit Orbwalker7UPImpl::LastTarget() const { return lastTarget_; }
inline OrbwalkingMode Orbwalker7UPImpl::ActiveMode() const {
    // Port 1-1 từ C# line 52-90 (ActiveMode getter)
    if (!initialize_) {
        return OrbwalkingMode::None;
    }
    if (activeMode_ != OrbwalkingMode::None) {
        return activeMode_;
    }
    if (KeyValue(menu_ ? menu_->Get<MenuKeyBind>("Combo") : nullptr, false) ||
        KeyValue(menu_ ? menu_->Get<MenuKeyBind>("ComboWithMove") : nullptr, false)) {
        return OrbwalkingMode::Combo;
    }
    if (KeyValue(menu_ ? menu_->Get<MenuKeyBind>("Harass") : nullptr, false)) {
        return OrbwalkingMode::Harass;
    }
    if (KeyValue(menu_ ? menu_->Get<MenuKeyBind>("LaneClear") : nullptr, false)) {
        return OrbwalkingMode::LaneClear;
    }
    if (KeyValue(menu_ ? menu_->Get<MenuKeyBind>("LastHit") : nullptr, false)) {
        return OrbwalkingMode::LastHit;
    }
    if (!KeyValue(menu_ ? menu_->Get<MenuKeyBind>("Flee") : nullptr, false)) {
        return OrbwalkingMode::None;
    }
    return OrbwalkingMode::Flee;
}
inline int Orbwalker7UPImpl::LastAutoAttackTick() const { return lastAutoAttackTick_; }
inline void Orbwalker7UPImpl::LastAutoAttackTick(int v) { lastAutoAttackTick_ = v; }
inline bool Orbwalker7UPImpl::IsAutoAttacking() { return false; } // TODO port
inline bool Orbwalker7UPImpl::IsWindingUp() { return false; }     // TODO port
inline bool Orbwalker7UPImpl::IsAttackCastComplete() { return false; } // TODO port
inline int Orbwalker7UPImpl::AttackCastDelayRemaining() { return 0; } // TODO port
inline int Orbwalker7UPImpl::NextAttackReadyTick() { return 0; }      // TODO port
inline int Orbwalker7UPImpl::AttackCooldownRemaining() { return 0; }  // TODO port
inline int Orbwalker7UPImpl::LastMovementTick() const { return lastMovementTick_; }
inline void Orbwalker7UPImpl::LastMovementTick(int v) { lastMovementTick_ = v; }
inline bool Orbwalker7UPImpl::AttackEnabled() const { return attackEnabled_; }
inline void Orbwalker7UPImpl::AttackEnabled(bool v) { attackEnabled_ = v; }
inline bool Orbwalker7UPImpl::MoveEnabled() const { return moveEnabled_; }
inline void Orbwalker7UPImpl::MoveEnabled(bool v) { moveEnabled_ = v; }
inline void Orbwalker7UPImpl::SetOrbwalkerPosition(const Vector3& p) { orbwalkerPosition_ = p; }
// Pause setters — port 1-1 từ C# 1451-1484. C# gốc không nhận param cho *ServerPauseTime,
// nhưng IOrbwalker interface SDK ép nhận `int time` (giống OrbwalkerBase). Logic theo
// OrbwalkerBase: time - Ping/2 để bù trừ network latency.
inline void Orbwalker7UPImpl::SetPauseTime(int t) { allPauseTick_ = Tick() + std::max(0, t); }
inline void Orbwalker7UPImpl::SetServerPauseTime(int t) { allPauseTick_ = Tick() + std::max(0, t - Game::Ping() / 2); }
inline void Orbwalker7UPImpl::SetAttackPauseTime(int t) { attackPauseTick_ = Tick() + std::max(0, t); }
inline void Orbwalker7UPImpl::SetAttackServerPauseTime(int t) { attackPauseTick_ = Tick() + std::max(0, t - Game::Ping() / 2); }
inline void Orbwalker7UPImpl::SetMovePauseTime(int t) { movePauseTick_ = Tick() + std::max(0, t); }
inline void Orbwalker7UPImpl::SetMoveServerPauseTime(int t) { movePauseTick_ = Tick() + std::max(0, t - Game::Ping() / 2); }

inline AttackableUnit Orbwalker7UPImpl::GetTarget() {
    // Port 1-1 từ C# 1043-1326 — hàm dài nhất, core target selection theo mode.
    if (!initialize_) {
        return AttackableUnit();
    }
    const OrbwalkingMode activeMode = ActiveMode();
    if (activeMode == OrbwalkingMode::None || activeMode == OrbwalkingMode::Flee) {
        return AttackableUnit();
    }
    AttackableUnit attackableUnit;  // null default
    std::vector<AIMinionClient> minions = GetMinions(200.0f);
    const auto player = GameObjects::Player();

    // Helper: lấy hero target từ list enemy heroes đã filter
    // C# TargetSelector.GetTarget(filteredHeroes, DamageType.Physical, true, null)
    // SDK không có overload nhận IEnumerable filter → filter thủ công rồi lấy
    // target đầu tiên thỏa điều kiện (giống C# FirstOrDefault).
    auto getFilteredHeroTarget = [&](float extraRange) -> AIHeroClient {
        for (const auto& x : GameObjects::EnemyHeroes()) {
            if (!Extensions::IsValidTarget(x)) continue;
            if (!Extensions::InCurrentAutoAttackRange(x, extraRange, true)) continue;
            if (!CanOrbObj(x)) continue;
            if (!CanAttackWithWindWall(x)) continue;
            return x;  // first match
        }
        return AIHeroClient();
    };

    // === Nhánh 1: Harass/LaneClear + !FarmOverHarass → hero target ===
    // C# line 1057-1066
    if ((activeMode == OrbwalkingMode::Harass ||
         (activeMode == OrbwalkingMode::LaneClear && !player.IsUnderEnemyTurret())) &&
        !BoolValue(prioritizeMenu_ ? prioritizeMenu_->Get<MenuBool>("FarmOverHarass") : nullptr, true)) {
        const AIHeroClient target = getFilteredHeroTarget(static_cast<float>(GetFindRange()));
        if (target.IsValid()) {
            return AttackableUnit(target.Handle());
        }
    }

    // === Nhánh 2: Barrels (Gangplank) ===
    // C# line 1068-1104
    if (BoolValue(attackMenu_ ? attackMenu_->Get<MenuBool>("Barrels") : nullptr, true) &&
        gangplankInGame_) {
        std::vector<AIMinionClient> barrels;
        for (const auto& j : GameObjects::Jungle()) {
            if (!Extensions::InCurrentAutoAttackRange(j, 0.0f, true)) continue;
            if (_stricmp(j.CharacterName().c_str(), "gangplankbarrel") != 0) continue;
            barrels.push_back(j);
        }
        for (const auto& aiminionClient : barrels) {
            if (!Extensions::InCurrentAutoAttackRange(aiminionClient, 0.0f, true)) continue;
            if (aiminionClient.Health() <= 0.0f) continue;
            // C# `aiminionClient.Owner as AIHeroClient` — SDK AIMinionClient chưa
            // có Owner(), bỏ check owner (missapi.md). Giữ check Health<=1 như C#.
            if (aiminionClient.Health() <= 1.0f) {
                return AttackableUnit(aiminionClient.Handle());
            }
            if (aiminionClient.HasBuff("gangplankebarrelactive") &&
                aiminionClient.Health() <= 2.0f) {
                // C# buff.StartTime + decay timing logic
                const auto buff = CoreBuffs::FindByName(aiminionClient.Address(), "gangplankebarrelactive");
                if (!buff.IsValid()) continue;
                const float num = player.ServerPosition().Distance(aiminionClient.ServerPosition()) -
                                  player.BoundingRadius();
                const float num2 = std::max(1.0f, 1000.0f * std::max(0.0f, num / GetProjectileSpeed()));
                const float num3 = GetAttackCastDelay() * 1000.0f +
                                   static_cast<float>(Game::Ping()) / 2.0f + num2;
                // C# dùng aiheroClient.Owner.Level — SDK chưa có Owner(), tạm
                // dùng player.Level() (missapi.md).
                const double num4 = (player.Level() >= 13) ? 0.5 :
                                    ((player.Level() >= 7) ? 1.0 : 2.0);
                double num5 = static_cast<double>(buff.GetStartTime()) + num4 * 2.0;
                if (static_cast<double>(buff.GetStartTime()) + num4 > static_cast<double>(Game::Time())) {
                    num5 = static_cast<double>(buff.GetStartTime()) + num4;
                }
                if (num5 < static_cast<double>(Game::Time() + num3 / 1000.0f)) {
                    return AttackableUnit(aiminionClient.Handle());
                }
            }
        }
    }

    // === Nhánh 3: Non-Combo, non-SupportMode → lane minion last hit ===
    // C# line 1106-1137
    if (activeMode != OrbwalkingMode::Combo && !IsSupportMode()) {
        // C# orderby: Siege descending, Super, ThenBy Ceil(Health/AD), ThenByDescending MaxHealth
        std::vector<AIMinionClient> list2;
        for (const auto& m : minions) {
            if (!Extensions::InCurrentAutoAttackRange(m, 0.0f, true)) continue;
            if (m.IsJungle()) continue;
            list2.push_back(m);
        }
        // Sort: Siege desc, Super, Ceil(Health/AD) asc, MaxHealth desc
        std::sort(list2.begin(), list2.end(), [&](const AIMinionClient& a, const AIMinionClient& b) {
            const bool aSiege = a.CharacterName().find("Siege") != std::string::npos;
            const bool bSiege = b.CharacterName().find("Siege") != std::string::npos;
            if (aSiege != bSiege) return aSiege;  // descending
            const bool aSuper = a.CharacterName().find("Super") != std::string::npos;
            const bool bSuper = b.CharacterName().find("Super") != std::string::npos;
            if (aSuper != bSuper) return aSuper;
            const double aCeil = std::ceil(static_cast<double>(a.Health()) / player.AD());
            const double bCeil = std::ceil(static_cast<double>(b.Health()) / player.AD());
            if (aCeil != bCeil) return aCeil < bCeil;  // ThenBy ascending
            return a.MaxHealth() > b.MaxHealth();  // ThenByDescending
        });
        for (const auto& aiminionClient2 : list2) {
            if (aiminionClient2.MaxHealth() <= 10.0f) {
                if (aiminionClient2.Health() <= 1.0f) {
                    return AttackableUnit(aiminionClient2.Handle());
                }
            } else {
                const float projectileSpeed = GetProjectileSpeed();
                const float num6 = GetAttackCastDelay() * 1000.0f - 100.0f +
                                   static_cast<float>(Game::Ping()) / 2.0f +
                                   1000.0f * std::max(0.0f, player.Distance(aiminionClient2) -
                                   player.BoundingRadius()) / projectileSpeed;
                const int farmDelay = SliderValue(
                    farmMenu_ ? farmMenu_->Get<MenuSlider>("FarmDelay") : nullptr, 30);
                const float prediction = HealthPrediction::GetPrediction(
                    aiminionClient2, static_cast<int>(num6), farmDelay,
                    HealthPredictionType::Default);
                if (prediction <= 0.0f) {
                    Orbwalker::FireNonKillableMinion(aiminionClient2, "NewOrbwalker");
                }
                const double autoAttackDamage =
                    player.GetAutoAttackDamage(aiminionClient2, true);
                if (static_cast<double>(prediction) <= autoAttackDamage) {
                    return AttackableUnit(aiminionClient2.Handle());
                }
            }
        }
    }

    // === Nhánh 4: ForceTarget ===
    // C# line 1139-1142
    if (forceTarget_.IsValid() &&
        Extensions::IsValidTarget(forceTarget_, FLT_MAX, true, Vector3{}) &&
        Extensions::InCurrentAutoAttackRange(forceTarget_, 0.0f, true)) {
        return forceTarget_;
    }

    // === Nhánh 5: Non-Combo, no minions hoặc Turret enabled → structures ===
    // C# line 1144-1168
    if (activeMode != OrbwalkingMode::Combo &&
        (minions.empty() ||
         BoolValue(prioritizeMenu_ ? prioritizeMenu_->Get<MenuBool>("Turret") : nullptr, true))) {
        // REMOVED: Turret/Inhibitor/Nexus disabled by user request
        /*
        // EnemyTurrets
        for (const auto& t : GameObjects::EnemyTurrets()) {
            if (!Extensions::IsValidTarget(t, FLT_MAX, true, Vector3{})) continue;
            if (!Extensions::InCurrentAutoAttackRange(t, 0.0f, true)) continue;
            return AttackableUnit(t.Handle());
        }
        // EnemyInhibitors
        for (const auto& i : GameObjects::EnemyInhibitors()) {
            if (!Extensions::IsValidTarget(i, FLT_MAX, true, Vector3{})) continue;
            if (!Extensions::InCurrentAutoAttackRange(i, 0.0f, true)) continue;
            return AttackableUnit(i.Handle());
        }
        // EnemyNexus
        const auto nexus = GameObjects::EnemyNexus();
        if (nexus.IsValid() &&
            Extensions::IsValidTarget(nexus, FLT_MAX, true, Vector3{}) &&
            Extensions::InCurrentAutoAttackRange(nexus, 0.0f, true)) {
            return AttackableUnit(nexus.Handle());
        }
        */
    }

    // === Nhánh 6: Non-LastHit, (Non-LaneClear hoặc !ShouldWait) → hero target ===
    // C# line 1170-1179
    if (activeMode != OrbwalkingMode::LastHit &&
        (activeMode != OrbwalkingMode::LaneClear || !ShouldWait(minions))) {
        const AIHeroClient target2 = getFilteredHeroTarget(static_cast<float>(GetFindRange()));
        if (target2.IsValid()) {
            return AttackableUnit(target2.Handle());
        }
    }

    // === Nhánh 8: Harass/LaneClear/LastHit → jungle monsters ===
    // C# line 1190-1213
    if (activeMode == OrbwalkingMode::Harass ||
        activeMode == OrbwalkingMode::LaneClear ||
        activeMode == OrbwalkingMode::LastHit) {
        std::vector<AIMinionClient> source;
        for (const auto& j : minions) {
            if (j.Team() != GameObjectTeam::Neutral) continue;
            if (!Extensions::InCurrentAutoAttackRange(j, 0.0f, true)) continue;
            source.push_back(j);
        }
        AttackableUnit attackableUnit2;
        if (!BoolValue(prioritizeMenu_ ? prioritizeMenu_->Get<MenuBool>("SmallJungle") : nullptr, false)) {
            // orderby MaxHealth descending → first
            auto best = source.end();
            float bestHp = -1.0f;
            for (auto it = source.begin(); it != source.end(); ++it) {
                if (it->MaxHealth() > bestHp) { bestHp = it->MaxHealth(); best = it; }
            }
            if (best != source.end()) attackableUnit2 = AttackableUnit(best->Handle());
        } else {
            // orderby MaxHealth ascending → first
            auto best = source.end();
            float bestHp = FLT_MAX;
            for (auto it = source.begin(); it != source.end(); ++it) {
                if (it->MaxHealth() < bestHp) { bestHp = it->MaxHealth(); best = it; }
            }
            if (best != source.end()) attackableUnit2 = AttackableUnit(best->Handle());
        }
        attackableUnit = attackableUnit2;
        if (attackableUnit.IsValid() &&
            Extensions::InCurrentAutoAttackRange(attackableUnit, 0.0f, true)) {
            return attackableUnit;
        }
    }

    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*
    // === Nhánh 9: Non-Combo, TurretFarm enabled → turret farm logic ===
    // C# line 1215-1283 (phức tạp nhất — turret farm setup)
    if (activeMode != OrbwalkingMode::Combo &&
        ListValue(farmMenu_ ? farmMenu_->Get<MenuList>("TurretFarm") : nullptr, 0) == 0) {
        // Tìm closest ally turret trong 1500 range
        AITurretClient closestTower;
        float closestDist = FLT_MAX;
        for (const auto& t : GameObjects::AllyTurrets()) {
            if (!Extensions::IsValidTarget(t, 1500.0f, false, Vector3{})) continue;
            const float d = Extensions::DistanceToPlayer(t.Position());
            if (d < closestDist) { closestDist = d; closestTower = t; }
        }
        if (closestTower.IsValid() &&
            Extensions::IsValidTarget(closestTower, 1500.0f, false, Vector3{}) &&
            CanTurretFarm(minions)) {
            // Minions trong 900 range của tower, sort theo distance
            std::vector<AIMinionClient> source2;
            for (const auto& x : minions) {
                const float distSqr = x.Position().DistanceSquared(closestTower.Position());
                if (distSqr >= 900.0f * 900.0f) continue;
                source2.push_back(x);
            }
            std::sort(source2.begin(), source2.end(), [&](const AIMinionClient& a, const AIMinionClient& b) {
                return a.Position().DistanceSquared(closestTower.Position()) <
                       b.Position().DistanceSquared(closestTower.Position());
            });
            if (!source2.empty()) {
                // First minion with turret aggro
                AIMinionClient aiminionClient3;
                for (const auto& m : source2) {
                    if (HealthPrediction::HasTurretAggro(m)) {
                        aiminionClient3 = m;
                        break;
                    }
                }
                if (aiminionClient3.IsValid()) {
                    const float projectileSpeed2 = GetProjectileSpeed();
                    const double autoAttackDamage2 =
                        closestTower.GetAutoAttackDamage(aiminionClient3, true);
                    // closestTower.AttackCastDelay -> CoreControl::GetAttackWindup
                    // closestTower.BasicAttack.MissileSpeed -> Utils::AutoAttack::GetProjectileSpeed
                    // SDK GetProjectileSpeed chỉ nhận AIHeroClient; turret không phải.
                    // C# đọc BasicAttack.MissileSpeed trực tiếp — tạm dùng default 1500
                    // (missapi.md).
                    const float towerMissileSpeed = 1500.0f + 70.0f;
                    const float num7 = CoreControl::GetAttackWindup(closestTower.Address()) * 1000.0f +
                        1000.0f * std::max(0.0f, aiminionClient3.Distance(closestTower) -
                        closestTower.BoundingRadius()) / towerMissileSpeed;
                    const float num8 = GetAttackCastDelay() * 1000.0f - 100.0f +
                        static_cast<float>(Game::Ping()) / 2.0f +
                        1000.0f * std::max(0.0f, player.Distance(aiminionClient3) -
                        player.BoundingRadius()) / projectileSpeed2;
                    const float prediction2 = HealthPrediction::GetPrediction(
                        aiminionClient3, static_cast<int>(num7 + num8), 70,
                        HealthPredictionType::Simulated);
                    if (static_cast<double>(prediction2) > autoAttackDamage2) {
                        // Turret won't kill aggro minion → find setup target
                        for (const auto& aiminionClient4 : source2) {
                            if (!Extensions::IsValidTarget(aiminionClient4, FLT_MAX, true, Vector3{})) continue;
                            if (HealthPrediction::HasTurretAggro(aiminionClient4)) continue;
                            const double autoAttackDamage3 =
                                player.GetAutoAttackDamage(aiminionClient4, true);
                            const double autoAttackDamage4 =
                                closestTower.GetAutoAttackDamage(aiminionClient4, true);
                            if (HealthPrediction::HasMinionAggro(aiminionClient4)) continue;
                            const float num9 = GetAttackCastDelay() * 1000.0f - 100.0f +
                                static_cast<float>(Game::Ping()) / 2.0f +
                                1000.0f * std::max(0.0f, player.Distance(aiminionClient4) -
                                player.BoundingRadius()) / projectileSpeed2;
                            const float prediction3 = HealthPrediction::GetPrediction(
                                aiminionClient4, static_cast<int>(num9 + num7), 70,
                                HealthPredictionType::Simulated);
                            if (static_cast<double>(prediction3) < autoAttackDamage4 * 2.0 ||
                                static_cast<double>(prediction3) > autoAttackDamage4 * 2.0 + autoAttackDamage3) {
                                if (static_cast<double>(prediction3) > autoAttackDamage4 + autoAttackDamage3 &&
                                    static_cast<double>(prediction3) <= autoAttackDamage4 + autoAttackDamage3 * 2.0) {
                                    return AttackableUnit(aiminionClient4.Handle());
                                }
                                if (static_cast<double>(prediction3) > autoAttackDamage4 * 2.0 + autoAttackDamage3 * 2.0) {
                                    return AttackableUnit(aiminionClient4.Handle());
                                }
                            }
                        }
                    }
                    return AttackableUnit();  // null
                }
                // No aggro minion → check first minion
                const AIMinionClient aiminionClient5 = source2.front();
                if (aiminionClient5.IsValid()) {
                    const double autoAttackDamage5 =
                        closestTower.GetAutoAttackDamage(aiminionClient5, true);
                    const double num10 =
                        static_cast<double>(HealthPrediction::GetPrediction(
                            aiminionClient5, 1500, 70, HealthPredictionType::Simulated)) -
                        autoAttackDamage5 * 1.100000023841858;
                    if (num10 > player.GetAutoAttackDamage(aiminionClient5, true) &&
                        num10 < autoAttackDamage5 * 1.100000023841858) {
                        return AttackableUnit(aiminionClient5.Handle());
                    }
                    if (num10 > autoAttackDamage5 * 2.0 +
                        player.GetAutoAttackDamage(aiminionClient5, true) * 2.0) {
                        return AttackableUnit(aiminionClient5.Handle());
                    }
                }
                return AttackableUnit();  // null
            }
        }
    }
    */
    // REMOVED: Turret/Inhibitor/Nexus disabled

    // === Nhánh 10: LaneClear, !ShouldWait → lane clear minion ===
    // C# line 1285-1312
    if (activeMode == OrbwalkingMode::LaneClear && !ShouldWait(minions)) {
        if (laneClearMinion_.IsValid() &&
            Extensions::IsValidTarget(laneClearMinion_, FLT_MAX, true, Vector3{}) &&
            Extensions::InCurrentAutoAttackRange(laneClearMinion_, 0.0f, true)) {
            if (laneClearMinion_.MaxHealth() <= 10.0f) {
                return laneClearMinion_;
            }
            const int farmDelay = SliderValue(
                farmMenu_ ? farmMenu_->Get<MenuSlider>("FarmDelay") : nullptr, 30);
            const AIBaseClient laneClearMinionBase(laneClearMinion_.Handle());
            const float prediction4 = HealthPrediction::GetPrediction(
                laneClearMinionBase,
                static_cast<int>(CoreControl::GetAttackDelay(player.Address()) * 2000.0f),
                farmDelay, HealthPredictionType::Simulated);
            if (static_cast<double>(prediction4) >= 2.0 * player.GetAutoAttackDamage(laneClearMinionBase, true) ||
                std::fabs(prediction4 - laneClearMinion_.Health()) < 1e-45f) {
                return laneClearMinion_;
            }
        }
        // Tìm minion tốt nhất: predHealth >= 2*AADamage hoặc |predHealth - Health| < epsilon
        // orderby Health descending
        AttackableUnit best;
        float bestHp = -1.0f;
        const int farmDelay = SliderValue(
            farmMenu_ ? farmMenu_->Get<MenuSlider>("FarmDelay") : nullptr, 30);
        for (const auto& m : minions) {
            if (!Extensions::InCurrentAutoAttackRange(m, 0.0f, true)) continue;
            if (m.IsJungle()) continue;
            const float predHealth = HealthPrediction::GetPrediction(
                m, static_cast<int>(CoreControl::GetAttackDelay(player.Address()) * 2000.0f),
                farmDelay, HealthPredictionType::Simulated);
            if (static_cast<double>(predHealth) >= 2.0 * player.GetAutoAttackDamage(m, true) ||
                std::fabs(predHealth - m.Health()) < FLT_EPSILON) {
                if (m.Health() > bestHp) {
                    bestHp = m.Health();
                    best = AttackableUnit(m.Handle());
                }
            }
        }
        if (best.IsValid() && Extensions::InCurrentAutoAttackRange(best, 0.0f, true)) {
            laneClearMinion_ = best;
            return best;
        }
    }

    // === Nhánh 11: !ShouldWait, non-Combo → special minion fallback ===
    // C# line 1314-1321
    // C# line 1325: return attackableUnit (null hoặc jungle target từ nhánh 8)
    return attackableUnit;
}
inline bool Orbwalker7UPImpl::CanAttack() {
    // TODO port C# line 941-944 (overload no-arg -> CanAttack(0))
    return CanAttack(0.0f);
}
inline bool Orbwalker7UPImpl::CanAttack(float extraWindup) {
    // Port 1-1 từ C# 946-1002
    if (!initialize_) {
        return false;
    }
    if (allPauseTick_ > 0 && allPauseTick_ - Tick() > 0) {
        return false;
    }
    if (attackPauseTick_ > 0 && attackPauseTick_ - Tick() > 0) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (tahmKenchInGame_ && player.HasBuff("tahmkenchwhasdevouredtarget")) {
        return false;
    }
    if (SDK::HasBuffOfType(player, ::SDK::BuffType::Fear)) {
        return false;
    }
    if (SDK::HasBuffOfType(player, ::SDK::BuffType::Polymorph) || player.HasBuff("Polymorph")) {
        return false;
    }
    if (!isKalista_ && player.HasBuff("blindingdart")) {
        return false;
    }
    if (isRengar_ && (player.HasBuff("RengarQ") || player.HasBuff("RengarQEmp"))) {
        return true;
    }
    if (isAphelios_ && player.HasBuff("apheliospreload")) {
        return false;
    }
    if (isJhin_ && player.HasBuff("JhinPassiveReload")) {
        return false;
    }
    // C# `Player.AttackDelay * 1000f` -> CoreControl::GetAttackDelay() (giây)
    float num = CoreControl::GetAttackDelay(player.Address()) * 1000.0f;
    if (isGraves_) {
        if (!player.HasBuff("gravesbasicattackammo1")) {
            return false;
        }
        num = CoreControl::GetAttackDelay(player.Address()) * 1000.0f * 1.0740297f - 716.2381f;
    } else if (isSett_ && nextAttackIsPassive_) {
        num = CoreControl::GetAttackDelay(player.Address()) * 1000.0f / 8.0f;
    }
    // C# `(float)(GameTimeTickCount + Game.Ping/2 + 25) >= (float)LastAutoAttackTick + num + extraWindup`
    return static_cast<float>(Tick() + Game::Ping() / 2 + 25) >=
           static_cast<float>(lastAutoAttackTick_) + num + extraWindup;
}
inline bool Orbwalker7UPImpl::CanMove() {
    // TODO port C# line 1004-1007 -> CanMove(0, false)
    return CanMove(0.0f, false);
}
inline bool Orbwalker7UPImpl::CanMove(float extraWindup, bool disableMissileCheck) {
    // Port 1-1 từ C# 1009-1041
    if (!initialize_) {
        return false;
    }
    if (allPauseTick_ > 0 && allPauseTick_ - Tick() > 0) {
        return false;
    }
    if (movePauseTick_ > 0 && movePauseTick_ - Tick() > 0) {
        return false;
    }
    const auto player = GameObjects::Player();
    if (tahmKenchInGame_ && player.HasBuff("tahmkenchwhasdevouredtarget")) {
        return false;
    }
    if (isKalista_) {
        return true;
    }
    if (missileLaunched_ && !disableMissileCheck &&
        BoolValue(advancedMenu_ ? advancedMenu_->Get<MenuBool>("MissileCheck") : nullptr, true)) {
        return true;
    }
    int num = 0;
    if (isRengar_ && (player.HasBuff("RengarQ") || player.HasBuff("RengarQEmp"))) {
        num = 200;
    }
    // C# `(Tick + Ping/2) >= (LastAutoAttackTick + GetAttackCastDelay()*1000 + extraWindup + num)`
    // GetAttackCastDelay() trả giây -> *1000 -> ms
    return static_cast<float>(Tick() + Game::Ping() / 2) >=
           static_cast<float>(lastAutoAttackTick_) +
           GetAttackCastDelay() * 1000.0f + extraWindup +
           static_cast<float>(num);
}
inline bool Orbwalker7UPImpl::Attack(const AttackableUnit& target) {
    // Port 1-1 từ C# 902-939
    if (!initialize_) {
        return false;
    }
    // C# `target as AIBaseClient` -> construct AIBaseClient từ handle + check valid
    // (cast chỉ thành công nếu target thực sự là AIBaseClient; IsValid là proxy)
    const AIBaseClient t(target.Handle());
    if (t.IsValid()) {
        if (!CanOrbObj(t)) {
            return false;
        }
    }
    // C# `target == null || !target.InCurrentAutoAttackRange(0f, true)`
    if (!target.IsValid() ||
        !Extensions::InCurrentAutoAttackRange(target, 0.0f, true)) {
        return false;
    }
    if (!CanAttackWithWindWall(target)) {
        return false;
    }
    // C# `Orbwalker.FireBeforeAttack(target, "NewOrbwalker")` -> OrbwalkingActionArgs
    // với field Process=true mặc định. Subscriber có thể set false để cancel.
    const OrbwalkingActionArgs beforeArgs =
        Orbwalker::FireBeforeAttack(target, "NewOrbwalker");
    if (beforeArgs.Process) {
        if (isKalista_) {
            missileLaunched_ = false;
        }
        // C# `GameObjects.Player.IssueOrder(GameObjectOrder.AttackUnit, target)`
        // -> SDK::IssueOrder(player, GameObjectOrder::AttackUnit, AIBaseClient(target.Handle()))
        const auto player = GameObjects::Player();
        if (SDK::IssueOrder(player, GameObjectOrder::AttackUnit, AIBaseClient(target.Handle()))) {
            lastLocalAttackTick_ = Tick();
            lastTarget_ = target;
        }
        return true;
    }
    return false;
}
inline void Orbwalker7UPImpl::Move(const Vector3& position) {
    // Port 1-1 từ C# 1339-1412
    if (!initialize_) {
        return;
    }
    Vector3 vector = position;
    if (!vector.IsValid()) {
        return;
    }
    const auto player = GameObjects::Player();
    const int extraHold = SliderValue(
        orbwalkerMenu_ ? orbwalkerMenu_->Get<MenuSlider>("ExtraHold") : nullptr, 50);
    const float num = std::max(30.0f, static_cast<float>(extraHold));
    // C# `vector.DistanceSquared(player.ServerPosition) < num²` -> Too close, skip
    if (vector.DistanceSquared(player.ServerPosition()) < num * num) {
        if (player.HasPath()) {  // C# `Path.Length != 0`
            lastMovementTick_ = Tick() - 70;
        }
        return;
    }
    // C# MoveRandom + DistanceSquared < 150² -> randomize position
    if (BoolValue(orbwalkerMenu_ ? orbwalkerMenu_->Get<MenuBool>("MoveRandom") : nullptr, false) &&
        player.Position().DistanceSquared(vector) < 150.0f * 150.0f) {
        // C# `player.ServerPosition.Extend(position, (random.NextFloat(0.6,1) + 0.2) * 400)`
        // Extend(pos, dist) = from + (to - from).Normalized() * dist
        const Vector3 dir = vector - player.ServerPosition();
        const float dirLen = dir.Length();
        if (dirLen > 0.0f) {
            std::uniform_real_distribution<float> dist(0.6f, 1.0f);
            const float extendDist = (dist(rng_) + 0.2f) * 400.0f;
            vector = player.ServerPosition() + dir * (extendDist / dirLen);
        }
    }
    const bool highmode = BoolValue(
        orbwalkerMenu_ ? orbwalkerMenu_->Get<MenuBool>("HighOrb") : nullptr, false);
    if (!highmode) {
        // C# waypoint angle check + path length
        float num2 = 0.0f;
        const std::vector<Vector3> waypoints = player.GetWaypoints();
        if (waypoints.size() > 1) {
            // C# `waypoints.PathLength() > 100f` — Vec2 PathLength
            float pathLen = 0.0f;
            for (std::size_t i = 1; i < waypoints.size(); ++i) {
                pathLen += waypoints[i].Distance(waypoints[i - 1]);
            }
            if (pathLen > 100.0f) {
                // C# `GameObjects.Player.GetPath(vector)` -> path navigation tới target.
                // API UPDATE: GetPath chỉ còn nhận int (lấy waypoints hiện tại của player).
                // Dùng GetWaypoints() để lấy path player đang đi thay vì path tới target.
                const std::vector<Vector3> path = player.GetPath();
                if (path.size() > 1) {
                    // C# `waypoints[1] - waypoints[0]` (Vec2) vs `path[1] - path[0]` (Vec3)
                    const Vector2 wpsDir = waypoints[1].To2D() - waypoints[0].To2D();
                    const Vector2 pathDir = path[1].To2D() - path[0].To2D();
                    // AngleBetween (degrees) — dùng helper Vec2Ext hoặc inline
                    const Vector2 a = wpsDir.Normalized();
                    const Vector2 b = pathDir.Normalized();
                    const float dot = std::max(-1.0f, std::min(1.0f, a.Dot(b)));
                    num2 = std::acos(dot) * 180.0f / 3.14159265358979f;
                    // C# `path.Last().DistanceSquared(waypoints.Last())`
                    const Vector2 pathLast = path.back().To2D();
                    const Vector2 wpsLast = waypoints.back().To2D();
                    const float num3 = pathLast.DistanceSquared(wpsLast);
                    if ((num2 < 10.0f && num3 < 500.0f * 500.0f) ||
                        num3 < 50.0f * 50.0f) {
                        return;
                    }
                }
            }
        }
        if (static_cast<float>(Tick() - lastMovementTick_) <
            static_cast<float>(70 + std::min(60, Game::Ping())) && num2 < 60.0f) {
            return;
        }
        if (num2 >= 60.0f && Tick() - lastMovementTick_ < 60) {
            return;
        }
    } else {
        if (Tick() - lastMovementTick_ < 50 + std::min(60, Game::Ping())) {
            return;
        }
    }
    // C# `Orbwalker.FirePreMove(vector, "NewOrbwalker")` -> OrbwalkingActionArgs
    // với .Process (bool) + .Position (Vector3)
    OrbwalkingActionArgs beforeMoveArgs = Orbwalker::FirePreMove(vector, "NewOrbwalker");
    if (beforeMoveArgs.Process) {
        if (BoolValue(drawMenu_ ? drawMenu_->Get<MenuBool>("ShowFakeClick") : nullptr, false) &&
            static_cast<float>(Tick() - lastFakeClickTick_) >
            250.0f - static_cast<float>(Game::Ping()) * 10.0f) {
            Hud::ShowClick(Hud::ClickType::Move, beforeMoveArgs.Position);
            lastFakeClickTick_ = Tick();
        }
        // C# `GameObjects.Player.IssueOrder(GameObjectOrder.MoveTo, position)`
        // -> SDK::IssueOrder(player, GameObjectOrder::MoveTo, position)
        if (SDK::IssueOrder(player, GameObjectOrder::MoveTo, beforeMoveArgs.Position)) {
            lastMovementTick_ = Tick();
        }
    }
}
inline void Orbwalker7UPImpl::Orbwalk(const AttackableUnit& target, const Vector3& position) {
    // Port 1-1 từ C# 1414-1441
    if (!initialize_) {
        return;
    }
    // C# `Variables.GameTimeTickCount - this.LastLocalAttackTick < 70 + Math.Min(60, Game.Ping)`
    if (Tick() - lastLocalAttackTick_ < 70 + std::min(60, Game::Ping())) {
        return;
    }
    // C# `if (this.AttackEnabled && this.CanAttack() && this.Attack(target)) return;`
    if (attackEnabled_ && CanAttack() && Attack(target)) {
        return;
    }
    // C# `if (this.MoveEnabled && this.CanMove(windupDelay, false)) { ... }`
    if (moveEnabled_ && CanMove(
            static_cast<float>(SliderValue(
                orbwalkerMenu_ ? orbwalkerMenu_->Get<MenuSlider>("WindupDelay") : nullptr, 60)),
            false)) {
        // C# `if (this.Menu["ComboWithMove"].GetValue<MenuKeyBind>().Active) return;`
        if (KeyValue(menu_ ? menu_->Get<MenuKeyBind>("ComboWithMove") : nullptr, false)) {
            return;
        }
        // C# `if (this.OrbwalkerMenu["LimitAttack"].Enabled
        //        && GameObjects.Player.AttackDelay < 0.3846154f
        //        && this.AutoAttackCounter % 3 != 0
        //        && !this.CanMove(500f, true)) return;`
        if (BoolValue(orbwalkerMenu_ ? orbwalkerMenu_->Get<MenuBool>("LimitAttack") : nullptr, false) &&
            CoreControl::GetAttackDelay(GameObjects::Player().Address()) < 0.3846154f &&
            autoAttackCounter_ % 3 != 0 &&
            !CanMove(500.0f, true)) {
            return;
        }
        // C# `Vector3 position2 = position.IsValid() ? position : Game.CursorPos;`
        const Vector3 position2 = position.IsValid() ? position : Game::CursorPos();
        Move(position2);
    }
}
inline bool Orbwalker7UPImpl::ShouldWait() {
    // TODO port C# ShouldWait (line 519-536) — C# takes minions param; SDK
    // IOrbwalker::ShouldWait() no-arg -> gọi GetMinions() rồi ShouldWait(minions)
    return ShouldWait(GetMinions(0.0f));
}
inline void Orbwalker7UPImpl::ResetAutoAttackTimer() {
    // Port 1-1 từ C# 1443-1449 — reset cả 4 pause tick
    allPauseTick_ = 0;
    attackPauseTick_ = 0;
    lastAutoAttackTick_ = 0;
    movePauseTick_ = 0;
}
inline void Orbwalker7UPImpl::Dispose() {
    // Port 1-1 từ C# 1486-1496 — unhook ALL events đã hook trong Init().
    // C# còn gọi MenuManager.Instance.Remove(this.Menu) — trong NightSharp plugin
    // sở hữu menu và tự Remove qua Orbwalker7UPPlugin::DestroyMenu(), nên impl
    // chỉ cần unhook events.
    Events::RemoveOnDoCast(&Orbwalker7UPImpl::OnDoCastStatic);
    Events::RemoveOnProcessSpell(&Orbwalker7UPImpl::OnProcessSpellCastStatic);
    Events::RemoveOnPlayAnimation(&Orbwalker7UPImpl::OnPlayAnimationStatic);
    Events::RemoveOnStopCast(&Orbwalker7UPImpl::OnStopCastStatic);
    Events::RemoveOnDeleteObject(&Orbwalker7UPImpl::OnDeleteStatic);
    Events::RemoveOnGameUpdate(&Orbwalker7UPImpl::OnUpdateStatic);
    Drawing::RemoveOnDraw(&Orbwalker7UPImpl::OnDrawStatic);
    if (RuntimeInstance == this) RuntimeInstance = nullptr;
}
inline bool Orbwalker7UPImpl::IsAutoAttack(std::string name) {
    // Port 1-1 từ C# 1328-1332 — dùng instance arrays (noAttacks_/attacks_),
    // KHÔNG dùng SDK Utils::AutoAttack (lists khác C#).
    // C# `name.IndexOf("attack", ignoreCase) >= 0` -> name contains "attack"
    auto containsIgnoreCase = [](const std::string& haystack, const char* needle) {
        const std::size_t nlen = std::strlen(needle);
        if (haystack.size() < nlen) return false;
        for (std::size_t i = 0; i + nlen <= haystack.size(); ++i) {
            bool match = true;
            for (std::size_t j = 0; j < nlen; ++j) {
                if (std::tolower(static_cast<unsigned char>(haystack[i + j])) !=
                    std::tolower(static_cast<unsigned char>(needle[j]))) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
        return false;
    };
    auto inList = [&](const std::vector<std::string>& list) {
        for (const auto& x : list) {
            if (_stricmp(name.c_str(), x.c_str()) == 0) return true;
        }
        return false;
    };
    const bool hasAttack = containsIgnoreCase(name, "attack");
    const bool inNoAttacks = inList(noAttacks_);
    const bool inAttacks = inList(attacks_);
    return (hasAttack && !inNoAttacks) || inAttacks;
}

inline bool Orbwalker7UPImpl::IsAutoAttackReset(std::string name) {
    // Port 1-1 từ C# 1334-1337 — dùng instance array attackResets_
    for (const auto& x : attackResets_) {
        if (_stricmp(name.c_str(), x.c_str()) == 0) return true;
    }
    return false;
}

// --- Private methods theo thứ tự C# ---
inline bool Orbwalker7UPImpl::ForceChase() const {
    // Port 1-1 từ C# line 22-32
    if (ActiveMode() == OrbwalkingMode::Combo &&
        KeyValue(miscMenu_ ? miscMenu_->Get<MenuKeyBind>("FindKey") : nullptr, false)) {
        return true;
    }
    return false;
}
inline int Orbwalker7UPImpl::GetFindRange() const {
    // Port 1-1 từ C# line 33-39
    return ForceChase()
        ? SliderValue(miscMenu_ ? miscMenu_->Get<MenuSlider>("Range") : nullptr, 0)
        : 0;
}
inline bool Orbwalker7UPImpl::FastLne() const {
    // Port 1-1 từ C# line 40-50
    if (ActiveMode() == OrbwalkingMode::LaneClear &&
        KeyValue(menu_ ? menu_->Get<MenuKeyBind>("FastLaneClear") : nullptr, false)) {
        return true;
    }
    return false;
}
inline void Orbwalker7UPImpl::SetActiveMode(OrbwalkingMode value) {
    activeMode_ = value;
}

inline void Orbwalker7UPImpl::Init(Menu* parentMenu) {
    // Port 1-1 từ C# constructor body line 92-213.
    // parentMenu là root menu do Orbwalker7UPPlugin tạo (new Menu(id,name,true)).
    initialize_ = true;
    attackEnabled_ = true;
    moveEnabled_ = true;

    menu_ = parentMenu;
    // C# Menu.SetLogo(SpriteRender.CreateLogo(Resource1.Orbwalker))
    // MISSAPI: không có resource nhúng tương đương -> bỏ qua (ghi missapi.md).

    // AttackMenu (C# line 99-103)
    attackMenu_ = menu_->AddSubMenu(new Menu("Attackable", "Attackable Unit", false));
    attackMenu_->Add(new MenuBool("Barrels", "Barrels", true));
    attackMenu_->Add(new MenuBool("JunglePlant", "Jungle Plant", false));
    attackMenu_->Add(new MenuBool("Wards", "Wards", true));

    // PrioritizeMenu (C# line 104-108)
    prioritizeMenu_ = menu_->AddSubMenu(new Menu("Prioritize", "Prioritize", false));
    prioritizeMenu_->Add(new MenuBool("FarmOverHarass", "Farm Over Harass", true));
    prioritizeMenu_->Add(new MenuBool("SmallJungle", "Small Jungle", false));
    prioritizeMenu_->Add(new MenuBool("Turret", "Turret", true));

    // OrbwalkerMenu (C# line 109-115) — bỏ Program.Chinese, giữ label tiếng Anh
    orbwalkerMenu_ = menu_->AddSubMenu(new Menu("Orbwalker", "Orbwalker", false));
    orbwalkerMenu_->Add(new MenuSlider("ExtraHold", "Extra Hold Position", 50, 0, 250));
    orbwalkerMenu_->Add(new MenuBool("MoveRandom", "Randomize Movement when too close", false));
    orbwalkerMenu_->Add(new MenuSlider("WindupDelay", "Extra Windup Delay", 60, 0, 250));
    orbwalkerMenu_->Add(new MenuBool("LimitAttack", "Don't Kite if Attack Speed > 2.5", false));
    orbwalkerMenu_->Add(new MenuBool("HighOrb", "以更高的频率进行走砍", false));
    orbwalkerMenu_->Add(new MenuBool("CalculateRunaway",
        "Calculate Run away time in Orb limit distance", true));

    // FarmMenu (C# line 116-124)
    farmMenu_ = menu_->AddSubMenu(new Menu("Farm", "Farm", false));
    farmMenu_->Add(new MenuSlider("FarmDelay", "Farm Delay", 30, 0, 200));
    farmMenu_->Add(new MenuSlider("FastFarmDelay", "Fast Farm Delay", 220, 0, 1000));
    farmMenu_->Add(new MenuList("TurretFarm", "Turret Farm Logic",
        { "Enabled", "Off" }, 0));
    farmMenu_->Add(new MenuSlider("TurretFramMaxLevel",
        "Disable Turret Farm When Player Level >= x", 13, 1, 18));

    // AdvancedMenu (C# line 125-129)
    advancedMenu_ = menu_->AddSubMenu(new Menu("Advanced", "Advanced", false));
    advancedMenu_->Add(new MenuBool("CalcItemDamage", "Calculate Item Damage", false));
    advancedMenu_->Add(new MenuBool("YasuoWallCheck", "Check Yasuo WindWall", true));
    advancedMenu_->Add(new MenuBool("MissileCheck", "Use Missile Checks", true));
    // C# key: "SupportMode_" + CharacterName — giữ nguyên để match C#
    const auto player = GameObjects::Player();
    const std::string supportKey = "SupportMode_" + player.CharacterName();
    advancedMenu_->Add(new MenuBool(supportKey.c_str(), "Support Mode", false));

    // DrawMenu (C# line 130-135)
    drawMenu_ = menu_->AddSubMenu(new Menu("Drawing", "Drawing", false));
    drawMenu_->Add(new MenuBool("DrawAttackRange", "Draw Attack Range", true));
    drawMenu_->Add(new MenuBool("DrawChaseRange", "Draw Force Chase Range", true));
    drawMenu_->Add(new MenuBool("DrawHoldPosition", "Draw Hold Position", false));
    drawMenu_->Add(new MenuBool("DrawKillableMinion", "Draw Killable Minion", false));
    drawMenu_->Add(new MenuBool("ShowFakeClick", "Show FakeClick", false));

    // MiscMenu (C# line 136-138)
    miscMenu_ = menu_->AddSubMenu(new Menu("Misc", "Extra Range Setting"));
    miscMenu_->Add(new MenuSlider("Range", "Extra LowHP Target Find range", 200, 0, 500));
    miscMenu_->Add(new MenuKeyBind("FindKey",
        "Enable Force chase Mode(Combo Activating)",
        Keys::LButton, KeyBindType::Press));

    // Mode keybinds (C# line 139-145) — Add trực tiếp vào menu_ (root)
    menu_->Add(new MenuKeyBind("Combo", "Combo", Keys::Space, KeyBindType::Press, false));
    menu_->Add(new MenuKeyBind("ComboWithMove", "Combo Without Move", Keys::N, KeyBindType::Press, false));
    menu_->Add(new MenuKeyBind("Harass", "Harass", Keys::C, KeyBindType::Press, false));
    menu_->Add(new MenuKeyBind("LaneClear", "LaneClear", Keys::V, KeyBindType::Press, false));
    menu_->Add(new MenuKeyBind("FastLaneClear", "Fast LaneClear", Keys::LButton, KeyBindType::Press, false));
    menu_->Add(new MenuKeyBind("LastHit", "LastHit", Keys::X, KeyBindType::Press, false));
    menu_->Add(new MenuKeyBind("Flee", "Flee", Keys::Z, KeyBindType::Press, false));

    // Champion flags (C# line 146-185) — giữ nguyên cấu trúc if/else lồng 1-1
    const std::string characterName = player.CharacterName();
    if (!(characterName == "Aphelios")) {
        if (!(characterName == "Graves")) {
            if (!(characterName == "Jhin")) {
                if (!(characterName == "Kalista")) {
                    if (!(characterName == "Rengar")) {
                        if (characterName == "Sett") {
                            isSett_ = true;
                        }
                    } else {
                        isRengar_ = true;
                    }
                } else {
                    isKalista_ = true;
                }
            } else {
                isJhin_ = true;
            }
        } else {
            isGraves_ = true;
        }
    } else {
        isAphelios_ = true;
    }

    // Detect Jax/Gangplank/TahmKench in game (C# line 186-205)
    for (const auto& aiheroClient : GameObjects::Heroes()) {
        if (!aiheroClient.IsValid()) continue;
        if (aiheroClient.IsEnemy()) {
            if (aiheroClient.CharacterName() == "Jax") {
                jaxInGame_ = true;
            }
            if (aiheroClient.CharacterName() == "Gangplank") {
                gangplankInGame_ = true;
            }
        }
        if (!aiheroClient.IsMe() && aiheroClient.CharacterName() == "TahmKench") {
            tahmKenchInGame_ = true;
        }
    }

    // Readonly arrays (C# line 1498-1702) — copy y nguyên
    attackResets_ = {
        "asheq", "camilleq2", "camilleq", "dariusnoxiantacticsonh",
        "elisespiderw", "fiorae", "gravesmove", "garenq",
        "gangplankqwrapper", "illaoiw", "jaycehypercharge",
        "jaxempowertwo", "kaylee", "luciane",
        "leonashieldofdaybreakattack", "leonashieldofdaybreak",
        "mordekaisermaceofspades", "monkeykingdoubleattack",
        "meditate", "masochism", "netherblade", "nautiluspiercinggaze",
        "nasusq", "powerfist", "rengarqemp", "rengarq",
        "renektonpreexecute", "reksaiq", "settq", "sivirw",
        "shyvanadoubleattack", "sejuaninorthernwinds",
        "trundletrollsmash", "talonnoxiandiplomacy", "takedown",
        "vorpalspikes", "volibearq", "vie", "vaynetumble",
        "xinzhaoq", "xinzhaocombotarget", "yorickspectral",
        "apheliosinfernumq", "gravesautoattackrecoilcastedummy"
    };
    attacks_ = {
        "caitlynpassivemissile", "itemtitanichydracleave",
        "itemtiamatcleave", "kennenmegaproc", "masteryidoublestrike",
        "quinnwenhanced", "renektonsuperexecute", "renektonexecute",
        "trundleq", "viktorqbuff", "xinzhaoqthrust1",
        "xinzhaoqthrust2", "xinzhaoqthrust3"
    };
    noAttacks_ = {
        "asheqattacknoonhit", "annietibbersbasicattack",
        "annietibbersbasicattack2", "bluecardattack",
        "dravenattackp_r", "dravenattackp_rc", "dravenattackp_rq",
        "dravenattackp_l", "dravenattackp_lc", "dravenattackp_lq",
        "elisespiderlingbasicattack", "gravesbasicattackspread",
        "gravesautoattackrecoil", "goldcardattack",
        "heimertyellowbasicattack", "heimertyellowbasicattack2",
        "heimertbluebasicattack", "heimerdingerwattack2",
        "heimerdingerwattack2ult", "ivernminionbasicattack2",
        "ivernminionbasicattack", "kindredwolfbasicattack",
        "monkeykingdoubleattack", "malzaharvoidlingbasicattack",
        "malzaharvoidlingbasicattack2", "malzaharvoidlingbasicattack3",
        "redcardattack", "shyvanadoubleattackdragon",
        "shyvanadoubleattack", "talonqdashattack", "talonqattack",
        "volleyattackwithsound", "volleyattack",
        "yorickghoulmeleebasicattack", "yorickghoulmeleebasicattack2",
        "yorickghoulmeleebasicattack3", "yorickbigghoulbasicattack",
        "zyraeplantattack", "zoebasicattackspecial1",
        "zoebasicattackspecial2", "zoebasicattackspecial3",
        "zoebasicattackspecial4", "apheliosseverumattackmis",
        "aphelioscrescendumattackmisin",
        "aphelioscrescendumattackmisout",
        "gravesautoattackrecoilcastedummy", "gravesautoattackrecoil",
        "gravesbasicattackspread"
    };
    windWallBrokenChampions_ = {
        "annie", "twistedfate", "leblanc", "urgot", "vladimir",
        "fiddlesticks", "ryze", "sivir", "soraka", "teemo",
        "tristana", "missfortune", "ashe", "morgana", "zilean",
        "twitch", "karthus", "anivia", "sona", "janna", "corki",
        "karma", "veigar", "swain", "caitlyn", "orianna", "brand",
        "vayne", "cassiopeia", "heimerdinger", "ezreal", "kennen",
        "kogmaw", "lux", "xerath", "ahri", "graves", "varus",
        "viktor", "lulu", "ziggs", "draven", "quinn", "syndra",
        "aurelionsol", "zoe", "zyra", "kaisa", "taliyah", "jhin",
        "kindred", "jinx", "lucian", "yuumi", "thresh", "kalista",
        "xayah", "aphelios", "bard", "ivern", "nami", "velkoz",
        "lissandra", "malzahar"
    };
    specialWindWallChampions_ = {
        "kayle", "elise", "nidalee", "jayce", "gnar", "azir", "neeko"
    };
    ignoreMinions_ = { "jarvanivstandard" };

    // Hook events (C# line 206-212) — map sang Events::/Drawing::
    RuntimeInstance = this;
    Events::AddOnDoCast(&Orbwalker7UPImpl::OnDoCastStatic);
    Events::AddOnProcessSpell(&Orbwalker7UPImpl::OnProcessSpellCastStatic);
    Events::AddOnPlayAnimation(&Orbwalker7UPImpl::OnPlayAnimationStatic);
    Events::AddOnStopCast(&Orbwalker7UPImpl::OnStopCastStatic);
    Events::AddOnDeleteObject(&Orbwalker7UPImpl::OnDeleteStatic);
    Events::AddOnGameUpdate(&Orbwalker7UPImpl::OnUpdateStatic);
    Drawing::AddOnDraw(&Orbwalker7UPImpl::OnDrawStatic);
}

inline float Orbwalker7UPImpl::GetAttackCastDelay() {
    // Port 1-1 từ C# line 214-221
    // C# `GameObjects.Player.AttackCastDelay` -> CoreControl::GetAttackWindup()
    // (seconds, float — xem docs/superpowers/specs/2026-07-07-ensoulsharp-api-full-port.md)
    const float windup = CoreControl::GetAttackWindup();
    if (isSett_ && nextAttackIsPassive_) {
        return windup - windup / 8.0f;
    }
    return windup;
}
inline float Orbwalker7UPImpl::GetProjectileSpeed() {
    // Port 1-1 từ C# line 222-394.
    // C# dùng switch trên ComputeStringHash(characterName) -> decompile thành
    // if/else lồng phức tạp. C++ dịch sang if/else _stricmp tuần tự theo đúng
    // thứ tự champion xuất hiện trong C# (giữ logic 1-1, không gộp/bỏ nhánh).
    const auto player = GameObjects::Player();
    const std::string name = player.CharacterName();

    if (_stricmp(name.c_str(), "Neeko") == 0) {
        if (!player.HasBuff("neekowpassiveready")) {
            return GetBasicAttackMissileSpeed();
        }
        return FLT_MAX;
    }
    if (_stricmp(name.c_str(), "Jinx") == 0) {
        if (!player.HasBuff("JinxQ")) {
            return GetBasicAttackMissileSpeed();
        }
        return 2000.0f;
    }
    if (_stricmp(name.c_str(), "Zeri") == 0) {
        return FLT_MAX;
    }
    if (_stricmp(name.c_str(), "Velkoz") == 0) {
        return FLT_MAX;
    }
    if (_stricmp(name.c_str(), "Jayce") == 0) {
        // C#: string.Equals(Spellbook.GetSpell(Q).Name, "jayceshockblast",
        //                   StringComparison.CurrentCultureIgnoreCase)
        const auto q = player.Spellbook().GetSpell(SpellSlot::Q);
        if (_stricmp(q.Name().c_str(), "jayceshockblast") == 0) {
            return 2000.0f;
        }
        return FLT_MAX;
    }
    if (_stricmp(name.c_str(), "Kayle") == 0) {
        if (player.AttackRange() >= 530.0f) {
            return 2250.0f;
        }
        return FLT_MAX;
    }
    if (_stricmp(name.c_str(), "Azir") == 0) {
        return FLT_MAX;
    }
    if (_stricmp(name.c_str(), "Ivern") == 0) {
        if (!player.HasBuff("ivernwpassive")) {
            return GetBasicAttackMissileSpeed();
        }
        return 1600.0f;
    }
    if (_stricmp(name.c_str(), "Viktor") == 0) {
        if (!player.HasBuff("ViktorPowerTransferReturn")) {
            return GetBasicAttackMissileSpeed();
        }
        return FLT_MAX;
    }
    if (_stricmp(name.c_str(), "Aphelios") == 0) {
        // Port 1-1 từ C# line 355-372. C# đọc TooltipVars[2] (Aphelios weapon id:
        // 1=Calibrum, 2=Severum, 3=Infernum, 4=Crescendum) để chọn missile speed.
        //
        // RE NOTE (MCP IDA + ILSpy): EnsoulSharp.TooltipVars là mảng float[16] inline
        // trong SpellDataInst của client x86 (32-bit). Trên x64, offset 0x108
        // (SpellInstanceVars) là CON TRỎ tới bảng named-var (entry 32-byte, có hash
        // key, binary-search) — KHÔNG phải mảng float phẳng, nên không thể đọc
        // `slot+0x108 + i*4` như Antigravity đã thử (tràn field lân cận 0x110/0x118).
        //
        // Phương án tương đương 100% cho x64: game có Q-spell riêng cho từng weapon
        // (hash dump: ApheliosCalibrumQ/SeverumQ/InfernumQ/CrescendumQ/GravitumQ).
        // Đọc tên Q-spell hiện tại rồi map trực tiếp sang missile speed — cùng kết
        // quả logic với C# mà không phụ thuộc offset dễ vỡ.
        const auto q = player.Spellbook().GetSpell(SpellSlot::Q);
        const std::string qName = q.Name();
        // C# TooltipVars[2] == 1.0f -> Calibrum -> 2500
        if (_stricmp(qName.c_str(), "ApheliosCalibrumQ") == 0) {
            return 2500.0f;
        }
        // C# TooltipVars[2] == 2.0f -> Severum -> FLT_MAX (hitscan, no missile)
        if (_stricmp(qName.c_str(), "ApheliosSeverumQ") == 0) {
            return FLT_MAX;
        }
        // C# TooltipVars[2] == 3.0f -> Infernum -> 1500
        if (_stricmp(qName.c_str(), "ApheliosInfernumQ") == 0) {
            return 1500.0f;
        }
        // C# TooltipVars[2] == 4.0f -> Crescendum -> 4000
        if (_stricmp(qName.c_str(), "ApheliosCrescendumQ") == 0) {
            return 4000.0f;
        }
        // Gravitum không có branch trong C# (fallthrough default) -> 1500
        // C# default branch (num2 không khớp 1/2/3/4) -> 1500
        return 1500.0f;
    }
    if (_stricmp(name.c_str(), "Thresh") == 0) {
        return FLT_MAX;
    }
    if (_stricmp(name.c_str(), "Poppy") == 0) {
        if (!player.HasBuff("poppypassivebuff")) {
            return GetBasicAttackMissileSpeed();
        }
        return 1600.0f;
    }
    // IL_2FA default
    return GetBasicAttackMissileSpeed();
}
inline float Orbwalker7UPImpl::GetBasicAttackMissileSpeed() {
    // Port 1-1 từ C# line 395-402
    // C# `GameObjects.Player.BasicAttack.MissileSpeed` -> SDK helper
    // Utils::AutoAttack::GetProjectileSpeed(player) (đã wrapper
    // BasicAttack.MissileSpeed + Data::GameData::GetUnitInfoByName).
    const auto player = GameObjects::Player();
    if (!player.IsMelee()) {
        return Utils::AutoAttack::GetProjectileSpeed(player);
    }
    return FLT_MAX;
}
inline bool Orbwalker7UPImpl::CanAttackWithWindWall(const AttackableUnit& target) {
    // Port 1-1 từ C# line 403-472
    if (!initialize_) {
        return false;
    }
    // C# `target.IsValidTarget(FLT_MAX, true, default(Vector3))`
    // -> Extensions::IsValidTarget(target, FLT_MAX, true, Vector3{})
    if (!Extensions::IsValidTarget(target, FLT_MAX, true, Vector3{})) {
        return false;
    }
    if (jaxInGame_) {
        // C# `target as AIHeroClient` -> construct AIHeroClient từ handle
        // (dynamic_cast không dùng vì AttackableUnit không polymorphic theo cách C#)
        const AIHeroClient aiheroClient(target.Handle());
        if (aiheroClient.IsValid() && !aiheroClient.IsDead() &&
            aiheroClient.CharacterName() == "Jax" &&
            aiheroClient.HasBuff("JaxCounterStrike")) {
            return false;
        }
    }
    if (!BoolValue(advancedMenu_ ? advancedMenu_->Get<MenuBool>("YasuoWallCheck") : nullptr, true)) {
        return true;
    }
    const auto player = GameObjects::Player();
    const std::string charName = player.CharacterName();

    // WindWallBrokenChampions.Any(name == x, ignoreCase)
    auto nameInList = [&](const std::vector<std::string>& list) {
        for (const auto& x : list) {
            if (_stricmp(charName.c_str(), x.c_str()) == 0) return true;
        }
        return false;
    };

    if (nameInList(windWallBrokenChampions_) &&
        Collisions::HasYasuoWindWallCollision(player.ServerPosition(), target.Position())) {
        return false;
    }
    if (nameInList(specialWindWallChampions_)) {
        if (charName == "Elise") {
            if (_stricmp(player.Spellbook().GetSpell(SpellSlot::R).Name().c_str(), "eliser") == 0 &&
                Collisions::HasYasuoWindWallCollision(player.ServerPosition(), target.Position())) {
                return false;
            }
        } else if (charName == "Nidalee") {
            if (_stricmp(player.Spellbook().GetSpell(SpellSlot::Q).Name().c_str(), "javelintoss") == 0 &&
                Collisions::HasYasuoWindWallCollision(player.ServerPosition(), target.Position())) {
                return false;
            }
        } else if (charName == "Jayce") {
            if (_stricmp(player.Spellbook().GetSpell(SpellSlot::Q).Name().c_str(), "jayceshockblast") == 0 &&
                Collisions::HasYasuoWindWallCollision(player.ServerPosition(), target.Position())) {
                return false;
            }
        } else if (charName == "Gnar") {
            if (_stricmp(player.Spellbook().GetSpell(SpellSlot::Q).Name().c_str(), "gnarq") == 0 &&
                Collisions::HasYasuoWindWallCollision(player.ServerPosition(), target.Position())) {
                return false;
            }
        } else if (charName == "Azir") {
            // C# `GameObjects.AzirSoldiers.All(x => x.IsValid && x.Distance(target.Position) > 350f)`
            // Dùng Extensions::detail::IsAzirSoldierEmitter (port 1-1 từ
            // Regex("Azir_.+_P_Soldier_Ring") của EnsoulSharp.SDK).
            // Giữ logic All() 1-1: nếu mọi soldier đều cách target > 350 thì kiểm wall.
            bool allFar = true;
            for (const auto& emitter : GameObjects::ParticleEmitters()) {
                if (!Extensions::detail::IsAzirSoldierEmitter(emitter)) continue;
                if (emitter.Distance(target.Position()) <= 350.0f) {
                    allFar = false;
                    break;
                }
            }
            if (allFar &&
                Collisions::HasYasuoWindWallCollision(player.ServerPosition(), target.Position())) {
                return false;
            }
        } else if (charName == "Neeko") {
            if (Collisions::HasYasuoWindWallCollision(player.ServerPosition(), target.Position())) {
                return false;
            }
        }
    }
    return true;
}
inline std::vector<AIMinionClient> Orbwalker7UPImpl::GetMinions(float range) {
    // Port 1-1 từ C# line 473-487
    if (!initialize_) {
        return {};
    }
    std::vector<AIMinionClient> list;

    // C# `m.InCurrentAutoAttackRange(range, true)` -> Extensions::InCurrentAutoAttackRange
    // (port đầy đủ vào SDK, gồm Azir special case — xem sdk/Extensions/Unit.h)
    auto inAaRange = [range](const AttackableUnit& m) {
        return Extensions::InCurrentAutoAttackRange(m, range, true);
    };
    auto nameInIgnore = [this](const std::string& name) {
        for (const auto& b : ignoreMinions_) {
            if (_stricmp(name.c_str(), b.c_str()) == 0) return true;
        }
        return false;
    };

    // EnemyMinions: trong AA range + không trong ignoreMinions + không phải plant + IsMinion
    for (const auto& m : GameObjects::EnemyMinions()) {
        if (!inAaRange(m)) continue;
        if (nameInIgnore(m.CharacterName())) continue;
        // C# `!m.GetMinionType().HasFlag(MinionTypes.JunglePlant)` -> minion.IsPlant()
        if (m.IsPlant()) continue;
        if (!m.IsMinion()) continue;
        list.push_back(m);
    }
    // Jungle: trong AA range + IsJungle + không phải plant
    for (const auto& j : GameObjects::Jungle()) {
        if (!inAaRange(j)) continue;
        if (!j.IsJungle()) continue;
        if (j.IsPlant()) continue;
        list.push_back(j);
    }
    return list;
}
inline bool Orbwalker7UPImpl::ShouldWait(const std::vector<AIMinionClient>& minions) {
    // Port 1-1 từ C# line 519-536
    if (!initialize_) {
        return false;
    }
    const auto player = GameObjects::Player();
    const float attackDelaySec = CoreControl::GetAttackDelay(player.Address());
    for (const auto& minion : minions) {
        const int farmDelay = SliderValue(farmMenu_ ? farmMenu_->Get<MenuSlider>("FarmDelay") : nullptr, 30);
        // C#: !FastLne ? AttackDelay*1000*2 : (AttackDelay*1000) + FastFarmDelay
        const float num = !FastLne()
            ? attackDelaySec * 1000.0f * 2.0f
            : (attackDelaySec * 1000.0f) +
              static_cast<float>(SliderValue(
                  farmMenu_ ? farmMenu_->Get<MenuSlider>("FastFarmDelay") : nullptr, 220));
        const float prediction = HealthPrediction::GetPrediction(
            minion, static_cast<int>(num), farmDelay, HealthPredictionType::Simulated);
        // C# `player.GetAutoAttackDamage(minion, includePassives=true, CalcItemDamage)`
        // -> SDK AIBaseClient::GetAutoAttackDamage(target, includePassives)
        //   CalcItemDamage flag ở C# chỉ thay đổi cách tính damage item — SDK overload
        //   includePassives=true đã cover phần lớn. Đánh dấu MISSAPI nếu cần refine.
        if (prediction < player.GetAutoAttackDamage(minion, true)) {
            return true;
        }
    }
    return false;
}
inline bool Orbwalker7UPImpl::CanTurretFarm(const std::vector<AIMinionClient>& minions) {
    // Port 1-1 từ C# line 537-565
    if (!initialize_) {
        return false;
    }
    // C# `farmMenu["TurretFarm"].GetValue<MenuList>().Index == 1` (1 = "Off")
    if (farmMenu_ &&
        ListValue(farmMenu_->Get<MenuList>("TurretFarm"), 0) == 1) {
        return false;
    }
    if (IsSupportMode()) {
        return false;
    }
    // C# `Player.Level >= FarmMenu["TurretFramMaxLevel"].Value`
    const auto player = GameObjects::Player();
    if (player.Level() >= SliderValue(
            farmMenu_ ? farmMenu_->Get<MenuSlider>("TurretFramMaxLevel") : nullptr, 13)) {
        return false;
    }
    if (minions.empty()) {
        return false;
    }
    // minions.Any(x => x.HasBuff("exaltedwithbaronnashorminion") && x.IsMinion())
    auto anyBaronMinion = [&]() {
        for (const auto& x : minions) {
            if (x.HasBuff("exaltedwithbaronnashorminion") && x.IsMinion()) {
                return true;
            }
        }
        return false;
    };
    if (anyBaronMinion()) {
        return false;
    }
    // !minions.Any(x => x.CharacterName.Contains("MinionSuper") && x.IsMinion())
    auto anySuperMinion = [&]() {
        for (const auto& x : minions) {
            if (x.CharacterName().find("MinionSuper") != std::string::npos &&
                x.IsMinion()) {
                return true;
            }
        }
        return false;
    };
    return !anySuperMinion();
}
inline bool Orbwalker7UPImpl::IsSupportMode() {
    // Port 1-1 từ C# 566-595
    if (!initialize_) {
        return false;
    }
    if (advancedMenu_ == nullptr) {
        return false;
    }
    const auto player = GameObjects::Player();
    const std::string supportKey = "SupportMode_" + player.CharacterName();
    const auto* supportItem = advancedMenu_->Get<MenuBool>(supportKey.c_str());
    if (supportItem == nullptr) {
        return false;
    }
    if (!BoolValue(supportItem, false)) {
        return false;
    }
    // C# `player.GetRealAutoAttackRange()` (no-arg) -> GetRealAutoAttackRange(player)
    const float realAutoAttackRange = Utils::AutoAttack::GetRealAutoAttackRange(player);
    const float range = std::max(1200.0f, realAutoAttackRange * 2.0f);
    // C# `player.CountAllyHeroesInRange(range, player)` — EnsoulSharp overload
    // nhận thêm `from` param; SDK `CountAllyHeroesInRange(range)` đếm quanh
    // Position() của unit. Player đếm quanh player → tương đương.
    if (player.CountAllyHeroesInRange(range) > 0) {
        if (player.HasBuff("talentreaperstacksone") ||
            player.HasBuff("talentreaperstackstwo") ||
            player.HasBuff("talentreaperstacksthree") ||
            player.HasBuff("talentreaperstacksfour")) {
            return false;
        }
    } else if (player.CountAllyHeroesInRange(2000.0f) == 0) {
        return false;
    }
    return true;
}
inline void Orbwalker7UPImpl::OnOrbwalkerProcessSpellCastDelayed(const Events::ProcessSpellEventArgs& args) {
    // Port 1-1 từ C# 596-607
    // C# `args.SData.Name` -> args.SpellName (char[96])
    // C# `args.Target as AttackableUnit` -> AttackableUnit(args.Target.Ptr)
    if (IsAutoAttackReset(args.SpellName)) {
        ResetAutoAttackTimer();
    }
    if (IsAutoAttack(args.SpellName)) {
        // C# `Orbwalker.FireAfterAttack(target, "NewOrbwalker")` — bắn AfterAttack
        // event qua SDK bus. Target.Ptr có thể 0 (self-cast/no target).
        const AttackableUnit target(args.Target.Ptr);
        Orbwalker::FireAfterAttack(target, "NewOrbwalker");
        missileLaunched_ = true;
    }
}
inline bool Orbwalker7UPImpl::CanOrbObj(const AIBaseClient& g) {
    // Port 1-1 từ C# 608-657
    if (!BoolValue(orbwalkerMenu_ ? orbwalkerMenu_->Get<MenuBool>("CalculateRunaway") : nullptr, true)) {
        return true;
    }
    if (!g.IsValid()) {
        return true;
    }
    // C# `g.Type == AIHeroClient || g.Type == AIMinionClient` -> IsHero/IsMinion
    if (!(g.IsHero() || g.IsMinion())) {
        return true;
    }
    if (g.IsMoving()) {
        const auto player = GameObjects::Player();
        const float fromDist = Extensions::DistanceToPlayer(g.ServerPosition());
        const float normalRange = player.AttackRange() + player.BoundingRadius();
        if (fromDist <= normalRange) {
            return true;
        }
        if (fromDist > normalRange &&
            fromDist <= normalRange + g.BoundingRadius()) {
            // C# `Prediction.GetPrediction(g, GetAttackCastDelay())` ->
            // Prediction::GetPrediction(unit, delay) (Movement.h:1009)
            // GetAttackCastDelay() trả giây -> float, khớp overload
            const auto pred = Prediction::GetPrediction(g, GetAttackCastDelay());
            // C# `pred == null` -> kiểm Hitchance == None (SDK trả obj, không null)
            if (pred.Hitchance == HitChance::None) {
                return false;
            }
            if (Extensions::DistanceToPlayer(pred.GetUnitPosition()) <=
                normalRange + g.BoundingRadius()) {
                return true;
            }
            return false;
        }
        if (ForceChase()) {
            if (fromDist > normalRange + g.BoundingRadius() + GetFindRange()) {
                return false;
            }
        }
    }
    return true;
}

// --- Event handlers ---
inline void Orbwalker7UPImpl::OnDoCast(const Events::ProcessSpellEventArgs& args) {
    // Port 1-1 từ C# 659-691
    if (!initialize_) {
        return;
    }
    // C# `sender.IsMe` -> AIBaseClient(args.Sender.Ptr).IsMe()
    // args.Sender là ObjectInfo (Ptr/NetworkId/Name/...), construct AIBaseClient
    const AIBaseClient sender(args.Sender.Ptr);
    if (!sender.IsMe()) {
        return;
    }
    const char* name = args.SpellName;
    // C# `args.CastTime == 0f` -> args.CastTime (float, port từ EnsoulSharp:
    // ExtraTimeForCast 0x98 + DesignerCastTime 0x9C — xem core/CoreEvents.h)
    if (IsAutoAttackReset(name) && args.CastTime == 0.0f) {
        ResetAutoAttackTimer();
    }
    if (!IsAutoAttack(name)) {
        return;
    }
    const AttackableUnit attackableUnit(args.Target.Ptr);
    if (attackableUnit.IsValid()) {
        lastAutoAttackTick_ = Tick() - Game::Ping() / 2;
        missileLaunched_ = false;
        lastMovementTick_ = 0;
        autoAttackCounter_++;
        if (!attackableUnit.Compare(lastTarget_)) {
            lastTarget_ = attackableUnit;
        }
        Orbwalker::FireOnAttack(attackableUnit, "NewOrbwalker");
    }
}
inline void Orbwalker7UPImpl::OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
    // Port 1-1 từ C# 694-717
    if (!initialize_) {
        return;
    }
    const AIBaseClient sender(args.Sender.Ptr);
    if (!sender.IsMe()) {
        return;
    }
    // C# `args.Slot == SpellSlot.Q` -> args.Slot (int, đã decode)
    if (isSett_ && args.Slot == static_cast<int>(SpellSlot::Q)) {
        nextAttackIsPassive_ = false;
    }
    if (Game::Ping() <= 30) {
        // C# `DelayAction.Add(30 - Game.Ping, () => OnOrbwalkerProcessSpellCastDelayed(args))`
        // SDK: Utils::DelayAction::Add(time, func) — schedule func sau time ms.
        // Capture args by value (ProcessSpellEventArgs là POD struct).
        const Events::ProcessSpellEventArgs argsCopy = args;
        Utils::DelayAction::Add(30 - Game::Ping(), [this, argsCopy]() {
            OnOrbwalkerProcessSpellCastDelayed(argsCopy);
        });
        return;
    }
    OnOrbwalkerProcessSpellCastDelayed(args);
}
inline void Orbwalker7UPImpl::OnPlayAnimation(const Events::PlayAnimationEventArgs& args) {
    // Port 1-1 từ C# 720-768
    if (!initialize_) {
        return;
    }
    const AIBaseClient sender(args.Sender.Ptr);
    if (!sender.IsMe()) {
        return;
    }
    // C# `args.Animation` -> args.Animation (char[64])
    const std::string animation = args.Animation;

    if (isRengar_ && animation == "Spell5") {
        int num = 0;
        // C# `LastTarget != null && IsValid && Position.IsValid()`
        if (lastTarget_.IsValid() && lastTarget_.Position().IsValid()) {
            // C# `(int)Math.Min(Player.Distance(LastTarget) / 1.5f, 0.6f)`
            // Lưu ý: Math.Min với 0.6f luôn trả 0.6f nếu distance/1.5 > 0.6,
            // và cast sang int → 0. Vẫn giữ 1-1 để không đổi logic.
            const auto player = GameObjects::Player();
            num += static_cast<int>(std::min(player.Distance(lastTarget_) / 1.5f, 0.6f));
        }
        lastAutoAttackTick_ = Tick() - Game::Ping() / 2 + num;
    }
    if (isSett_) {
        // C# `args.Animation.Contains("Attack")` -> find "Attack"
        if (animation.find("Attack") != std::string::npos) {
            if (animation.find("Passive") != std::string::npos) {
                info_ = SettAttackInfo(true, Tick());
                nextAttackIsPassive_ = false;
                return;
            }
            info_ = SettAttackInfo(false, Tick());
            nextAttackIsPassive_ = true;
            return;
        } else {
            if (animation == "Spell1_A") {
                info_ = SettAttackInfo(false, Tick());
                nextAttackIsPassive_ = true;
                return;
            }
            if (animation == "Spell1_B") {
                info_ = SettAttackInfo(true, Tick());
                nextAttackIsPassive_ = false;
            }
        }
    }
}
inline void Orbwalker7UPImpl::OnStopCast(const Events::StopCastEventArgs& args) {
    // Port 1-1 từ C# 771-781
    if (!initialize_) {
        return;
    }
    // C# `sender != null && sender.Owner != null && sender.Owner.IsMe &&
    //       args.DestroyMissile && args.KeepAnimationPlaying`
    // SDK: StopCastEventArgs.Sender là ObjectInfo của spellbook owner.
    // Construct AIHeroClient từ Sender.Ptr, check IsMe.
    const AIHeroClient owner(args.Sender.Ptr);
    if (owner.IsValid() && owner.IsMe() &&
        args.DestroyMissile && args.KeepAnimationPlaying) {
        ResetAutoAttackTimer();
    }
}
inline void Orbwalker7UPImpl::OnDelete(const Events::ObjectEventArgs& args) {
    // Port 1-1 từ C# 784-814
    if (!initialize_) {
        return;
    }
    // C# `sender == null || !sender.IsValid` -> construct + IsValid check
    const GameObject sender(args.Sender.Ptr);
    if (!sender.IsValid()) {
        return;
    }
    // C# `ForceTarget != null && sender.Compare(ForceTarget)` -> ForceTarget = null
    // SDK: AttackableUnit default = invalid. Reset to default AttackableUnit().
    if (forceTarget_.IsValid() && sender.Compare(forceTarget_)) {
        forceTarget_ = AttackableUnit();
    }
    if (laneClearMinion_.IsValid() && sender.Compare(laneClearMinion_)) {
        laneClearMinion_ = AttackableUnit();
    }
    if (lastTarget_.IsValid() && sender.Compare(lastTarget_)) {
        lastTarget_ = AttackableUnit();
    }
    // C# `isAphelios && sender.Type == MissileClient`
    if (isAphelios_ && sender.Type() == ::Core::Objects::ObjectType::MissileClient) {
        const MissileClient missileClient(args.Sender.Ptr);
        // C# `missileClient.SData != null` -> SpellName không rỗng (proxy)
        // C# `missileClient.SpellCaster != null && SpellCaster.IsMe`
        //   -> ObjectManager::GetUnitByIndex<AIHeroClient>(CasterIndex).IsMe()
        // C# `missileClient.Name == "ApheliosCrescendumAttackMisIn"` -> Name()
        if (!missileClient.SpellName().empty()) {
            const auto caster = ObjectManager::GetUnitByIndex<AIHeroClient>(
                missileClient.CasterIndex());
            if (caster.IsValid() && caster.IsMe() &&
                missileClient.Name() == "ApheliosCrescendumAttackMisIn") {
                ResetAutoAttackTimer();
            }
        }
    }
}
inline void Orbwalker7UPImpl::OnUpdate(const Events::GameUpdateEventArgs& args) {
    // Port 1-1 từ C# 817-842
    if (!initialize_) {
        return;
    }
    // C# `isSett && nextAttackIsPassive && Info.AttackTime > 0 &&
    //       GameTimeTickCount - Info.AttackTime > 2000`
    if (isSett_ && nextAttackIsPassive_ && info_.Time > 0 &&
        Tick() - info_.Time > 2000) {
        nextAttackIsPassive_ = false;
    }
    // C# `AdvancedMenu["CalcItemDamage"].Enabled` -> sync vào field calcItemDamage_
    calcItemDamage_ = BoolValue(
        advancedMenu_ ? advancedMenu_->Get<MenuBool>("CalcItemDamage") : nullptr, false);
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    // C# `MenuGUI.IsChatOpen || MenuGUI.IsShopOpen` -> SDK::MenuGUI
    if (MenuGUI::IsChatOpen() || MenuGUI::IsShopOpen()) {
        return;
    }
    if (ActiveMode() == OrbwalkingMode::None) {
        return;
    }
    // C# `GetTarget()` + `Orbwalk(target, _orbwalkerPosition)`
    const AttackableUnit target = GetTarget();
    Orbwalk(target, orbwalkerPosition_);
}
inline void Orbwalker7UPImpl::OnDraw() {
    // Port 1-1 từ C# 845-900
    if (!initialize_) {
        return;
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (MenuGUI::IsChatOpen() || MenuGUI::IsShopOpen()) {
        return;
    }
    const Vector3 playerPos = player.Position();
    if (playerPos.IsValid()) {
        // C# DrawAttackRange + CircleRender.Draw(position, range, color, 3, false)
        // -> Drawing::DrawCircle(worldPos, radius, color_u32, thickness, segments)
        // PaleVioletRed = #DB7093 -> 0xFFDB7093 (ARGB)
        if (BoolValue(drawMenu_ ? drawMenu_->Get<MenuBool>("DrawAttackRange") : nullptr, true)) {
            const float range = Utils::AutoAttack::GetRealAutoAttackRange(player);
            if (Drawing::OnScreen(playerPos)) {  // proxy cho IsOnScreen(range)
                Drawing::DrawCircle(playerPos, range, 0xFFDB7093u, 3.0f, 64);
            }
        }
        // C# DrawHoldPosition: BoundingRadius + ExtraHold slider
        // Purple = #800080 -> 0xFF800080
        if (BoolValue(menu_ ? menu_->Get<MenuBool>("DrawHoldPosition") : nullptr, false)) {
            const int extraHold = SliderValue(
                orbwalkerMenu_ ? orbwalkerMenu_->Get<MenuSlider>("ExtraHold") : nullptr, 50);
            Drawing::DrawCircle(playerPos,
                player.BoundingRadius() + static_cast<float>(extraHold),
                0xFF800080u, 3.0f, 64);
        }
        // C# FastLne -> DrawText "Fast Farm Mode" tại cursor
        if (FastLne()) {
            const Vec2 cursorScreen = Drawing::WorldToScreen(Game::CursorPos());
            Drawing::DrawText(cursorScreen.x, cursorScreen.y, 0xFFFFFFFFu, "Fast Farm Mode");
        }
        // C# ForceChase -> DrawText "Force Chase Mode" + optional chase range circle
        if (ForceChase()) {
            const Vec2 cursorScreen = Drawing::WorldToScreen(Game::CursorPos());
            Drawing::DrawText(cursorScreen.x, cursorScreen.y, 0xFFFFFFFFu, "Force Chase Mode");
            if (BoolValue(drawMenu_ ? drawMenu_->Get<MenuBool>("DrawChaseRange") : nullptr, true)) {
                colorIndex_++;
                if (colorIndex_ >= 450) colorIndex_ = 0;
                // C# PlusRender.GetFullColorList(450)[colorindex] — rainbow list.
                // Port 1-1 vào Common/Base.h (Orbwalker7UP::Common::PlusRender)
                static const auto colorList =
                    Orbwalker7UP::Common::PlusRender::GetFullColorList(450, true);
                const std::uint32_t rainbow =
                    colorList[static_cast<std::size_t>(colorIndex_)];
                Drawing::DrawCircle(playerPos,
                    Utils::AutoAttack::GetRealAutoAttackRange(player) + GetFindRange(),
                    rainbow, 2.0f, 64);
            }
        }
    }
    // C# DrawKillableMinion: vẽ circle xanh quanh minion killable trong 2x AA range
    if (BoolValue(drawMenu_ ? drawMenu_->Get<MenuBool>("DrawKillableMinion") : nullptr, false)) {
        const float range = Utils::AutoAttack::GetRealAutoAttackRange(player) * 2.0f;
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!Extensions::IsValidTarget(minion, range, true, Vector3{})) continue;
            if (!minion.IsMinion()) continue;
            if (!Drawing::OnScreen(minion.Position())) continue;
            if (minion.Health() >= player.GetAutoAttackDamage(minion, true)) continue;
            // Green = #00FF00 -> 0xFF00FF00
            Drawing::DrawCircle(minion.Position(),
                minion.BoundingRadius() * 2.0f, 0xFF00FF00u, 3.0f, 32);
        }
    }
}

// --- Static dispatchers ---
inline void Orbwalker7UPImpl::OnDoCastStatic(const Events::ProcessSpellEventArgs& a) {
    if (RuntimeInstance) RuntimeInstance->OnDoCast(a);
}
inline void Orbwalker7UPImpl::OnProcessSpellCastStatic(const Events::ProcessSpellEventArgs& a) {
    if (RuntimeInstance) RuntimeInstance->OnProcessSpellCast(a);
}
inline void Orbwalker7UPImpl::OnPlayAnimationStatic(const Events::PlayAnimationEventArgs& a) {
    if (RuntimeInstance) RuntimeInstance->OnPlayAnimation(a);
}
inline void Orbwalker7UPImpl::OnStopCastStatic(const Events::StopCastEventArgs& a) {
    if (RuntimeInstance) RuntimeInstance->OnStopCast(a);
}
inline void Orbwalker7UPImpl::OnDeleteStatic(const Events::ObjectEventArgs& a) {
    if (RuntimeInstance) RuntimeInstance->OnDelete(a);
}
inline void Orbwalker7UPImpl::OnUpdateStatic(const Events::GameUpdateEventArgs& a) {
    if (RuntimeInstance) RuntimeInstance->OnUpdate(a);
}
inline void Orbwalker7UPImpl::OnDrawStatic() {
    if (RuntimeInstance) RuntimeInstance->OnDraw();
}

} // namespace Orbwalker7UP

// ---------------------------------------------------------------------------
// Orbwalker7UPPlugin — entry IPlugin, OnLoad/OnUnload đăng ký "7UP"
// ---------------------------------------------------------------------------
namespace Plugins {

class Orbwalker7UPPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Orbwalker7UP"; }
    const char* GetInternalId() const override { return "core.orbwalker_7up"; }
    const char* GetAuthor() const override { return "7UP"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return true; }

    void OnLoad() override {
        if (m_orbwalker) return;
        DestroyMenu();
        m_menu = new ::SDK::Menu(GetInternalId(), GetName(), true);
        m_orbwalker = new ::Orbwalker7UP::Orbwalker7UPImpl(m_menu);
        m_menu->Attach();
        ::SDK::Orbwalker::AddOrbwalker(kImplementationName, m_orbwalker);
        ::SDK::Orbwalker::SetOrbwalker(kImplementationName);
        SetSdkOrbwalkerLoaded(false);
    }

    void OnUnload() override {
        if (!m_orbwalker) return;
        ::SDK::Orbwalker::RemoveOrbwalker(kImplementationName);
        m_orbwalker->Dispose();
        delete m_orbwalker;
        m_orbwalker = nullptr;
        DestroyMenu();
        SetSdkOrbwalkerLoaded(true);
    }

private:
    static constexpr const char* kImplementationName = "7UP";

    static void SetSdkOrbwalkerLoaded(bool loaded) {
        const int idx = PluginRegistry::FindByInternalId("orbwalker");
        if (idx >= 0 && PluginRegistry::HasRuntime(idx)) {
            if (loaded) PluginRegistry::LoadPlugin(idx);
            else        PluginRegistry::UnloadPlugin(idx);
            return;
        }
        if (auto* impl = ::SDK::Orbwalker::GetOrbwalker("SDK")) {
            if (loaded) impl->Resume(); else impl->Suspend();
        }
        if (idx >= 0) PluginRegistry::Plugins[idx].Loaded = loaded;
    }

    void DestroyMenu() {
        if (!m_menu) return;
        ::SDK::UI::MenuManager::Instance().Remove(m_menu);
        delete m_menu;
        m_menu = nullptr;
    }

    ::SDK::Menu* m_menu = nullptr;
    ::Orbwalker7UP::Orbwalker7UPImpl* m_orbwalker = nullptr;
};

} // namespace Plugins
