#pragma once

#include "../IPlugin.h"
#include "../../core/CoreClassification.h"
#include "../../menu/PluginRegistry.h"
#include "../../sdk/Core/Game.h"
#include "../../sdk/GameObjects/ObjectManager.h"
#include "../../sdk/Math/HealthPrediction.h"
#include "../../sdk/UI/Drawing.h"
#include "../../sdk/UI/UI.h"
#include "../../sdk/Utils/AutoAttack.h"
#include "../../sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "../../sdk/Wrappers/TargetSelector/TargetSelector.h"

#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace Plugins {

class OrbwalkerPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "Orbwalker 2.0"; }
    const char* GetInternalId() const override { return "plugin_orbwalker"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return true; }

    void OnLoad() override {
        if (!m_menu) {
            m_menu = SDK::Menu::Create("plugin_orbwalker", "Orbwalker 2.0");

            auto* attack = m_menu->AddSubMenu("Attackable", "Attackable Unit");
            attack->Add<SDK::MenuBool>("Barrels", "Barrels", true);
            attack->Add<SDK::MenuBool>("JunglePlant", "Jungle Plant", false);
            attack->Add<SDK::MenuBool>("SpecialMinions", "Pets", true);
            attack->Add<SDK::MenuBool>("Wards", "Wards", true);

            auto* prioritize = m_menu->AddSubMenu("Prioritize", "Prioritize");
            prioritize->Add<SDK::MenuBool>("FarmOverHarass", "Farm Over Harass", true);
            prioritize->Add<SDK::MenuBool>("SpecialMinion", "Special Minion", false);
            prioritize->Add<SDK::MenuBool>("SmallJungle", "Small Jungle", false);
            prioritize->Add<SDK::MenuBool>("Turret", "Turret", true);

            auto* orbwalker = m_menu->AddSubMenu("Orbwalker", "Orbwalker");
            orbwalker->Add<SDK::MenuSlider>("ExtraHold", "Extra Hold Position", 50, 0, 250);
            orbwalker->Add<SDK::MenuBool>("MoveRandom", "Randomize Movement when too close", false);
            orbwalker->Add<SDK::MenuSlider>("WindupDelay", "Extra Windup Delay", 60, 0, 250);
            orbwalker->Add<SDK::MenuBool>("LimitAttack", "Don't Kite if Attack Speed > 2.5", false);
            orbwalker->Add<SDK::MenuBool>("HighOrb", "HighOrb", false);
            orbwalker->Add<SDK::MenuBool>("CalculateRunaway", "Calculate Run away time in Orb limit distance", true);

            auto* farm = m_menu->AddSubMenu("Farm", "Farm");
            farm->Add<SDK::MenuSlider>("FarmDelay", "Farm Delay", 30, 0, 200);
            farm->Add<SDK::MenuSlider>("FastFarmDelay", "Fast Farm Delay", 220, 0, 1000);
            farm->Add<SDK::MenuBool>("ShouldWait", "ShouldWait", true);
            farm->Add<SDK::MenuList>("TurretFarm", "Turret Farm Logic", std::vector<std::string>{ "Enabled", "Off" }, 0);
            farm->Add<SDK::MenuSlider>("TurretFramMaxLevel", "Disable Turret Farm When Player Level >= x", 13, 1, 18);

            auto* advanced = m_menu->AddSubMenu("Advanced", "Advanced");
            advanced->Add<SDK::MenuBool>("CalcItemDamage", "Calculate Item Damage", false);
            advanced->Add<SDK::MenuBool>("YasuoWallCheck", "Check Yasuo WindWall", true);
            advanced->Add<SDK::MenuBool>("MissileCheck", "Use Missile Checks", true);

            auto* drawing = m_menu->AddSubMenu("Drawing", "Drawing");
            drawing->Add<SDK::MenuBool>("DrawAttackRange", "Draw Attack Range", true);
            drawing->Add<SDK::MenuBool>("DrawChaseRange", "Draw Force Chase Range", true);
            drawing->Add<SDK::MenuBool>("DrawHoldPosition", "Draw Hold Position", false);
            drawing->Add<SDK::MenuBool>("DrawKillableMinion", "Draw Killable Minion", false);
            drawing->Add<SDK::MenuBool>("ShowFakeClick", "Show FakeClick", false);

            auto* misc = m_menu->AddSubMenu("Misc", "Extra Range Setting");
            misc->Add<SDK::MenuSlider>("Range", "Extra LowHP Target Find range", 200, 0, 500);
            misc->Add<SDK::MenuKeyBind>("FindKey", "Enable Force chase Mode(Combo Activating)", VK_LBUTTON, SDK::KeyBindType::Press);

            m_menu->Add<SDK::MenuKeyBind>("Combo", "Combo", VK_SPACE, SDK::KeyBindType::Press, false);
            m_menu->Add<SDK::MenuKeyBind>("ComboWithMove", "Combo Without Move", 'N', SDK::KeyBindType::Press, false);
            m_menu->Add<SDK::MenuKeyBind>("Harass", "Harass", 'C', SDK::KeyBindType::Press, false);
            m_menu->Add<SDK::MenuKeyBind>("LaneClear", "LaneClear", 'V', SDK::KeyBindType::Press, false);
            m_menu->Add<SDK::MenuKeyBind>("FastLaneClear", "Fast LaneClear", VK_LBUTTON, SDK::KeyBindType::Press, false);
            m_menu->Add<SDK::MenuKeyBind>("LastHit", "LastHit", 'X', SDK::KeyBindType::Press, false);
            m_menu->Add<SDK::MenuKeyBind>("Flee", "Flee", 'Z', SDK::KeyBindType::Press, false);
        }

        s_instance = this;
        SDK::Orbwalker::SetExternalControl(true);
        if (!s_doCastRegistered) {
            SDK::Events::SpellCast::AddOnDoCast(&OrbwalkerPlugin::S_OnDoCast);
            s_doCastRegistered = true;
        }
        if (!s_stopCastRegistered) {
            SDK::Events::SpellCast::AddOnStopCast(&OrbwalkerPlugin::S_OnStopCast);
            s_stopCastRegistered = true;
        }
        HideSdkEntry();
    }

    void OnUnload() override {
        if (s_instance == this) {
            SDK::Orbwalker::SetActiveMode(SDK::OrbwalkerMode::None);
            SDK::Orbwalker::SetExternalControl(false);
            s_instance = nullptr;
        }
        ShowSdkEntry();
    }

    void OnUpdate() override {
        if (!m_menu) return;
        HideSdkEntry();

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) {
            SDK::Orbwalker::SetActiveMode(SDK::OrbwalkerMode::None);
            return;
        }
        EnsureRuntimeState();

        m_activeMode = ReadActiveMode();
        SDK::Orbwalker::SetActiveMode(m_activeMode);
        if (m_activeMode == SDK::OrbwalkerMode::None) return;
        if (SDK::Game::IsChatOpen() || SDK::Game::IsShopOpen()) return;

        SDK::AIBaseClient target = GetTarget();
        Orbwalk(target);
    }

    void OnRender() override {
        if (!m_menu) return;
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return;
        if (SDK::Game::IsChatOpen() || SDK::Game::IsShopOpen()) return;

        auto* draw = m_menu->GetSubMenu("Drawing");
        if (!draw) return;

        if (draw->GetBoolValue("DrawAttackRange", true)) {
            SDK::Drawing::DrawCircle(player.Position(), player.GetRealAutoAttackRange(), IM_COL32(221, 160, 221, 210), 1.6f);
        }

        if (draw->GetBoolValue("DrawHoldPosition", false)) {
            SDK::Drawing::DrawCircle(
                player.Position(),
                player.BoundingRadius() + static_cast<float>(OrbSlider("ExtraHold", 50)),
                IM_COL32(160, 80, 255, 190),
                1.5f);
        }

        if (IsForceChase() && draw->GetBoolValue("DrawChaseRange", true)) {
            SDK::Drawing::DrawCircle(
                player.Position(),
                player.GetRealAutoAttackRange() + static_cast<float>(GetFindRange()),
                IM_COL32(80, 220, 255, 220),
                1.6f);
        }

        if (IsFastLaneClear()) {
            SDK::Drawing::DrawText(SDK::Game::CursorPos(), "Fast Farm Mode", IM_COL32(255, 255, 255, 235), false, true);
        }

        if (draw->GetBoolValue("DrawKillableMinion", false)) {
            const float range = player.GetRealAutoAttackRange() * 2.0f;
            for (const auto& minion : SDK::ObjectManager::EnemyMinions()) {
                if (!minion.IsValid() || minion.IsDead() || !minion.IsVisible()) continue;
                if (player.Distance(minion) > range) continue;
                if (minion.Health() < player.GetAutoAttackDamage(minion)) {
                    SDK::Drawing::DrawCircle(minion.Position(), minion.BoundingRadius() * 2.0f, IM_COL32(70, 255, 70, 235), 1.5f);
                }
            }
        }
    }

    SDK::MenuUI::Menu* GetMenuRoot() override { return m_menu; }

    void OnProcessSpellCast(const SDK::AIBaseClient& sender,
                            const SDK::Events::SpellCast::ProcessSpellCastEventArgs& args) override {
        if (!sender.IsValid() || !sender.IsMe()) return;
        if (SDK::Utils::AutoAttack::IsAutoAttackReset(args.SpellName)) {
            ResetAttackTimer();
            return;
        }
        if (!args.IsAutoAttack && !SDK::Utils::AutoAttack::IsAutoAttack(args.SpellName)) return;

        m_lastAutoAttackTick = SDK::Game::TickCount() - (SDK::Game::Ping() / 2);
        if (m_pendingAttack && m_pendingAttackTick > m_lastAutoAttackTick) {
            m_lastAutoAttackTick = m_pendingAttackTick;
        }
        m_pendingAttack = false;
        m_pendingAttackTick = 0;
        m_pendingAttackTargetNetId = 0;
        m_missileLaunched = false;
        m_lastMovementTick = 0;
        ++m_autoAttackCounter;
        auto target = SDK::ObjectManager::GetByNetId(args.TargetNetworkId);
        if (target.IsValid()) m_lastTarget = SDK::AIBaseClient(target.Address());

        SDK::OrbwalkingActionArgs onAttack = {};
        onAttack.Sender = sender;
        onAttack.Target = target;
        onAttack.Position = target.IsValid() ? target.Position() : SDK::Vector3();
        onAttack.Type = SDK::OrbwalkingType::OnAttack;
        onAttack.Process = true;
        SDK::Orbwalker::InvokeAction(onAttack);
    }

private:
    SDK::MenuUI::Menu* m_menu = nullptr;
    SDK::OrbwalkerMode m_activeMode = SDK::OrbwalkerMode::None;
    SDK::AIBaseClient m_lastTarget = {};
    SDK::AIBaseClient m_laneClearMinion = {};
    SDK::Vector3 m_lastMovePosition = {};
    int m_lastAutoAttackTick = 0;
    int m_lastLocalAttackTick = 0;
    int m_lastMovementTick = 0;
    int m_lastFakeClickTick = 0;
    int m_pendingAttackTick = 0;
    int m_pendingAttackTargetNetId = 0;
    int m_autoAttackCounter = 0;
    bool m_pendingAttack = false;
    bool m_missileLaunched = false;
    bool m_attackEnabled = true;
    bool m_moveEnabled = true;
    bool m_isAphelios = false;
    bool m_isGraves = false;
    bool m_isJhin = false;
    bool m_isKalista = false;
    bool m_isRengar = false;
    bool m_isSett = false;
    bool m_jaxInGame = false;
    bool m_gangplankInGame = false;
    bool m_tahmKenchInGame = false;
    bool m_nextAttackIsPassive = false;
    bool m_championFlagsCached = false;
    std::string m_cachedChampionName = {};
    SDK::AIBaseClient m_cachedTarget = {};
    SDK::OrbwalkerMode m_cachedTargetMode = SDK::OrbwalkerMode::None;
    int m_cachedTargetTick = 0;
    std::vector<SDK::AIMinionClient> m_cachedMinions = {};
    float m_cachedMinionsExtraRange = -1.0f;
    int m_cachedMinionsTick = 0;
    bool m_cachedShouldWait = false;
    bool m_cachedShouldWaitFastLaneClear = false;
    int m_cachedShouldWaitTick = 0;

    static inline OrbwalkerPlugin* s_instance = nullptr;
    static inline bool s_doCastRegistered = false;
    static inline bool s_stopCastRegistered = false;

    static void S_OnDoCast(const SDK::AIBaseClient& sender,
                           const SDK::Events::SpellCast::ProcessSpellCastEventArgs& args) {
        if (s_instance) s_instance->OnDoCast(sender, args);
    }

    static void S_OnStopCast(const SDK::AIBaseClient& sender,
                             const SDK::Events::SpellCast::StopCastEventArgs& args) {
        if (s_instance) s_instance->OnStopCast(sender, args);
    }

    void OnDoCast(const SDK::AIBaseClient& sender,
                  const SDK::Events::SpellCast::ProcessSpellCastEventArgs& args) {
        if (!sender.IsValid() || !sender.IsMe()) return;
        if (!args.IsAutoAttack && !SDK::Utils::AutoAttack::IsAutoAttack(args.SpellName)) return;

        m_missileLaunched = true;
        if (m_pendingAttack && m_lastAutoAttackTick < m_pendingAttackTick) {
            m_lastAutoAttackTick = m_pendingAttackTick;
        }
        m_pendingAttack = false;
        m_pendingAttackTick = 0;
        m_pendingAttackTargetNetId = 0;
        auto target = SDK::ObjectManager::GetByNetId(args.TargetNetworkId);
        if (target.IsValid()) {
            m_lastTarget = SDK::AIBaseClient(target.Address());
        }

        SDK::OrbwalkingActionArgs after = {};
        after.Sender = sender;
        after.Target = target;
        after.Position = target.IsValid() ? target.Position() : SDK::Vector3();
        after.Type = SDK::OrbwalkingType::AfterAttack;
        after.Process = true;
        SDK::Orbwalker::InvokeAction(after);
    }

    void OnStopCast(const SDK::AIBaseClient& sender,
                    const SDK::Events::SpellCast::StopCastEventArgs& args) {
        if (!sender.IsValid() || !sender.IsMe()) return;
        if (args.SpellName.empty() || !SDK::Utils::AutoAttack::IsAutoAttack(args.SpellName)) return;
        if (!args.SuccessfullyCasted && args.ForceStop) {
            ResetAttackTimer();
        }
    }

    void CacheChampionFlags() {
        const auto player = SDK::ObjectManager::Player();
        const std::string name = player.CharacterName();
        if (name.empty()) return;
        m_isAphelios = _stricmp(name.c_str(), "Aphelios") == 0;
        m_isGraves = _stricmp(name.c_str(), "Graves") == 0;
        m_isJhin = _stricmp(name.c_str(), "Jhin") == 0;
        m_isKalista = _stricmp(name.c_str(), "Kalista") == 0;
        m_isRengar = _stricmp(name.c_str(), "Rengar") == 0;
        m_isSett = _stricmp(name.c_str(), "Sett") == 0;

        m_jaxInGame = false;
        m_gangplankInGame = false;
        m_tahmKenchInGame = false;
        for (const auto& hero : SDK::ObjectManager::Heroes()) {
            if (!hero.IsValid()) continue;
            const std::string h = hero.CharacterName();
            if (hero.IsEnemy() && _stricmp(h.c_str(), "Jax") == 0) m_jaxInGame = true;
            if (hero.IsEnemy() && _stricmp(h.c_str(), "Gangplank") == 0) m_gangplankInGame = true;
            if (!hero.IsMe() && _stricmp(h.c_str(), "TahmKench") == 0) m_tahmKenchInGame = true;
        }
    }

    void EnsureRuntimeState() {
        const auto player = SDK::ObjectManager::Player();
        const std::string name = player.CharacterName();
        if (!name.empty() && (!m_championFlagsCached || _stricmp(name.c_str(), m_cachedChampionName.c_str()) != 0)) {
            CacheChampionFlags();
            m_cachedChampionName = name;
            m_championFlagsCached = true;
        }

        auto* advanced = Sub("Advanced");
        if (advanced) {
            const std::string supportKey = SupportModeKey();
            if (!advanced->HasItem(supportKey)) {
                advanced->Add<SDK::MenuBool>(supportKey, "Support Mode", false);
            }
        }
    }

    SDK::MenuUI::Menu* Sub(const char* name) const {
        return m_menu ? m_menu->GetSubMenu(name) : nullptr;
    }

    bool SubBool(const char* menu, const char* name, bool fallback) const {
        auto* sub = Sub(menu);
        return sub ? sub->GetBoolValue(name, fallback) : fallback;
    }

    int SubSlider(const char* menu, const char* name, int fallback) const {
        auto* sub = Sub(menu);
        return sub ? sub->GetSliderValue(name, fallback) : fallback;
    }

    int SubList(const char* menu, const char* name, int fallback) const {
        auto* sub = Sub(menu);
        return sub ? sub->GetListIndex(name, fallback) : fallback;
    }

    int OrbSlider(const char* name, int fallback) const {
        return SubSlider("Orbwalker", name, fallback);
    }

    bool OrbBool(const char* name, bool fallback) const {
        return SubBool("Orbwalker", name, fallback);
    }

    bool FarmOverHarass() const {
        return SubBool("Prioritize", "FarmOverHarass", true);
    }

    std::string SupportModeKey() const {
        const auto player = SDK::ObjectManager::Player();
        const std::string champion = player.CharacterName();
        return "SupportMode_" + (champion.empty() ? std::string("Player") : champion);
    }

    SDK::OrbwalkerMode ReadActiveMode() const {
        if (!m_menu) return SDK::OrbwalkerMode::None;
        if (m_menu->GetKeyBindValue("Combo", false) || m_menu->GetKeyBindValue("ComboWithMove", false)) return SDK::OrbwalkerMode::Combo;
        if (m_menu->GetKeyBindValue("Harass", false)) return SDK::OrbwalkerMode::Harass;
        if (m_menu->GetKeyBindValue("LaneClear", false)) return SDK::OrbwalkerMode::LaneClear;
        if (m_menu->GetKeyBindValue("LastHit", false)) return SDK::OrbwalkerMode::LastHit;
        if (m_menu->GetKeyBindValue("Flee", false)) return SDK::OrbwalkerMode::Flee;
        return SDK::OrbwalkerMode::None;
    }

    bool IsForceChase() const {
        auto* misc = Sub("Misc");
        return m_activeMode == SDK::OrbwalkerMode::Combo && misc && misc->GetKeyBindValue("FindKey", false);
    }

    int GetFindRange() const {
        return IsForceChase() ? SubSlider("Misc", "Range", 200) : 0;
    }

    bool IsFastLaneClear() const {
        if (m_activeMode != SDK::OrbwalkerMode::LaneClear || !m_menu) return false;
        return m_menu->GetKeyBindValue("FastLaneClear", false) ||
            ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    }

    bool IsComboWithoutMove() const {
        return m_menu && m_menu->GetKeyBindValue("ComboWithMove", false);
    }

    float GetAttackCastDelay() const {
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return 0.3f;
        const float delay = player.AttackCastDelay();
        return (m_isSett && m_nextAttackIsPassive) ? delay - delay / 8.0f : delay;
    }

    float GetProjectileSpeed() const {
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsMelee()) return FLT_MAX;
        return SDK::Utils::AutoAttack::GetProjectileSpeed(player);
    }

    bool CanAttackWithWindWall(const SDK::AIBaseClient& target) const {
        if (!target.IsValid()) return false;
        if (m_jaxInGame && target.IsHero() && target.HasBuff("JaxCounterStrike")) return false;
        return true;
    }

    bool CanOrbObj(const SDK::AIBaseClient& target) const {
        if (!OrbBool("CalculateRunaway", true)) return true;
        if (!target.IsValid() || (!target.IsHero() && !target.IsMinion()) || !target.IsMoving()) return true;

        const auto player = SDK::ObjectManager::Player();
        const float normalRange = player.AttackRange() + player.BoundingRadius();
        const float distance = target.DistanceToPlayer();
        if (distance <= normalRange + target.BoundingRadius()) return true;
        if (IsForceChase()) return distance <= normalRange + target.BoundingRadius() + static_cast<float>(GetFindRange());
        return false;
    }

    bool IsAttackCandidate(const SDK::GameObject& obj, bool allowNeutral = false, float extraRange = 0.0f) const {
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || !obj.IsValid()) return false;

        const bool isStructure = CoreClassification::IsStructure(obj.Address());
        if (isStructure) {
            if (!allowNeutral && !obj.IsEnemy()) return false;
            if (obj.Health() <= 0.0f) return false;
            return player.InAutoAttackRange(obj, extraRange + 45.0f);
        }

        if (obj.IsDead()) return false;
        if (!allowNeutral && !obj.IsEnemy()) return false;
        if (obj.Health() <= 0.0f || obj.MaxHealth() <= 0.0f) return false;
        if (!obj.IsVisible()) return false;
        return player.InAutoAttackRange(obj, extraRange + 45.0f);
    }

    bool IsCachedTargetUsable(const SDK::AIBaseClient& target, float extraRange = 0.0f) const {
        if (!target.IsValid() || !target.IsAlive()) return false;
        if (target.IsHero()) {
            return IsAttackCandidate(target, false, extraRange) &&
                CanAttackWithWindWall(target) &&
                CanOrbObj(target);
        }
        return IsAttackCandidate(target, target.IsNeutral(), extraRange);
    }

    SDK::AIBaseClient CacheTarget(const SDK::AIBaseClient& target) {
        m_cachedTarget = target;
        m_cachedTargetMode = m_activeMode;
        m_cachedTargetTick = SDK::Game::TickCount();
        return target;
    }

    bool IsSupportMode() const {
        auto* advanced = Sub("Advanced");
        if (!advanced || !advanced->GetBoolValue(SupportModeKey(), false)) return false;
        const auto player = SDK::ObjectManager::Player();
        const float range = std::max(1200.0f, player.GetRealAutoAttackRange() * 2.0f);
        return player.CountAllyHeroesInRange(range) > 0 || player.CountAllyHeroesInRange(2000.0f) > 0;
    }

    int AttackClockTick() const {
        return (m_pendingAttack && m_pendingAttackTick > m_lastAutoAttackTick)
            ? m_pendingAttackTick
            : m_lastAutoAttackTick;
    }

    bool CanAttack(float extraWindup = 0.0f) const {
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return false;
        if (m_tahmKenchInGame && player.HasBuff("tahmkenchwhasdevouredtarget")) return false;
        if (!m_isKalista && player.HasBuff("blindingdart")) return false;
        if (m_isRengar && (player.HasBuff("RengarQ") || player.HasBuff("RengarQEmp"))) return true;
        if (m_isAphelios && player.HasBuff("apheliospreload")) return false;
        if (m_isJhin && player.HasBuff("JhinPassiveReload")) return false;

        float attackDelayMs = player.AttackDelay() * 1000.0f;
        if (m_isGraves) {
            if (!player.HasBuff("gravesbasicattackammo1")) return false;
            attackDelayMs = player.AttackDelay() * 1000.0f * 1.0740297f - 716.2381f;
        } else if (m_isSett && m_nextAttackIsPassive) {
            attackDelayMs = player.AttackDelay() * 1000.0f / 8.0f;
        }

        const int now = SDK::Game::TickCount();
        return static_cast<float>(now + (SDK::Game::Ping() / 2) + 25) >=
            static_cast<float>(AttackClockTick()) + attackDelayMs + extraWindup;
    }

    bool CanMove(float extraWindup = 0.0f, bool disableMissileCheck = false) const {
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return false;
        if (m_tahmKenchInGame && player.HasBuff("tahmkenchwhasdevouredtarget")) return false;
        if (m_isKalista) return true;
        if (m_missileLaunched && !disableMissileCheck && SubBool("Advanced", "MissileCheck", true)) return true;

        int rengarExtra = 0;
        if (m_isRengar && (player.HasBuff("RengarQ") || player.HasBuff("RengarQEmp"))) rengarExtra = 200;

        const int now = SDK::Game::TickCount();
        return static_cast<float>(now + (SDK::Game::Ping() / 2)) >=
            static_cast<float>(AttackClockTick()) + GetAttackCastDelay() * 1000.0f + extraWindup + static_cast<float>(rengarExtra);
    }

    void ResetAttackTimer() {
        m_lastAutoAttackTick = 0;
        m_pendingAttack = false;
        m_pendingAttackTick = 0;
        m_pendingAttackTargetNetId = 0;
        m_missileLaunched = false;
    }

    const std::vector<SDK::AIMinionClient>& GetMinions(float extraRange = 0.0f) {
        const int now = SDK::Game::TickCount();
        if (now - m_cachedMinionsTick < 80 && std::fabs(m_cachedMinionsExtraRange - extraRange) < 0.1f) {
            return m_cachedMinions;
        }

        const auto player = SDK::ObjectManager::Player();
        m_cachedMinions.clear();
        m_cachedMinionsExtraRange = extraRange;
        m_cachedMinionsTick = now;

        for (const auto& minion : SDK::ObjectManager::EnemyMinions()) {
            if (!minion.IsValid() || !minion.IsAlive() || minion.IsJungleMonster() || minion.IsPlant()) continue;
            if (player.InAutoAttackRange(minion, extraRange + 45.0f)) m_cachedMinions.push_back(minion);
        }
        for (const auto& jungle : SDK::ObjectManager::JungleMinions()) {
            if (!jungle.IsValid() || !jungle.IsAlive() || jungle.IsPlant()) continue;
            if (player.InAutoAttackRange(jungle, extraRange + 45.0f)) m_cachedMinions.push_back(jungle);
        }
        return m_cachedMinions;
    }

    bool ShouldWait(const std::vector<SDK::AIMinionClient>& minions) {
        const int now = SDK::Game::TickCount();
        const bool fastLaneClear = IsFastLaneClear();
        if (now - m_cachedShouldWaitTick < 80 &&
            fastLaneClear == m_cachedShouldWaitFastLaneClear) {
            return m_cachedShouldWait;
        }

        m_cachedShouldWait = ComputeShouldWait(minions);
        m_cachedShouldWaitFastLaneClear = fastLaneClear;
        m_cachedShouldWaitTick = now;
        return m_cachedShouldWait;
    }

    bool ComputeShouldWait(const std::vector<SDK::AIMinionClient>& minions) const {
        if (!SubBool("Farm", "ShouldWait", true)) return false;
        const auto player = SDK::ObjectManager::Player();
        const int farmDelay = SubSlider("Farm", "FarmDelay", 30);
        const int fastDelay = SubSlider("Farm", "FastFarmDelay", 220);
        const bool fastLaneClear = IsFastLaneClear();
        const float time = fastLaneClear
            ? player.AttackDelay() * 1000.0f + static_cast<float>(fastDelay)
            : player.AttackDelay() * 1000.0f * 2.0f;
        const auto allyMinions = SDK::ObjectManager::AllyMinions();

        for (const auto& minion : minions) {
            if (!minion.IsValid() || !minion.IsAlive() || minion.IsJungleMonster()) continue;
            float prediction = SDK::HealthPrediction::GetPrediction(
                minion,
                static_cast<int>(time),
                farmDelay,
                SDK::HealthPredictionType::Simulated);
            const float damage = player.GetAutoAttackDamage(minion);
            if (damage <= 0.0f) continue;

            if (!fastLaneClear && std::fabs(prediction - minion.Health()) < 1.0f) {
                float estimatedIncoming = 0.0f;
                for (const auto& ally : allyMinions) {
                    if (!ally.IsValid() || !ally.IsAlive() || !ally.IsVisible()) continue;
                    if (ally.Distance(minion) > 850.0f) continue;
                    estimatedIncoming += std::max(5.0f, ally.GetAutoAttackDamage(minion));
                }
                if (estimatedIncoming > 0.0f) {
                    prediction = std::max(0.0f, minion.Health() - estimatedIncoming);
                }
            }

            if (prediction < damage) return true;
        }
        return false;
    }

    SDK::AIBaseClient GetTarget() {
        if (m_activeMode == SDK::OrbwalkerMode::None || m_activeMode == SDK::OrbwalkerMode::Flee) {
            return CacheTarget(SDK::AIBaseClient());
        }

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return CacheTarget(SDK::AIBaseClient());

        const float heroRange = player.GetRealAutoAttackRange() + static_cast<float>(GetFindRange()) + 45.0f;
        const int now = SDK::Game::TickCount();
        const float extraRange = static_cast<float>(GetFindRange());
        if (m_cachedTargetMode == m_activeMode && now - m_cachedTargetTick < 60) {
            if (!m_cachedTarget.IsValid() || IsCachedTargetUsable(m_cachedTarget, extraRange)) {
                return m_cachedTarget;
            }
        }

        if (m_activeMode == SDK::OrbwalkerMode::Combo) {
            if (m_gangplankInGame && SubBool("Attackable", "Barrels", true)) {
                auto barrel = GetBarrel();
                if (barrel.IsValid()) return CacheTarget(barrel);
            }

            return CacheTarget(GetBestHero(heroRange));
        }

        if ((m_activeMode == SDK::OrbwalkerMode::Harass ||
             (m_activeMode == SDK::OrbwalkerMode::LaneClear && !player.IsUnderEnemyTurret())) &&
            !FarmOverHarass()) {
            auto hero = GetBestHero(heroRange);
            if (hero.IsValid()) return CacheTarget(hero);
        }

        if (m_gangplankInGame && SubBool("Attackable", "Barrels", true)) {
            auto barrel = GetBarrel();
            if (barrel.IsValid()) return CacheTarget(barrel);
        }

        const auto& minions = GetMinions(200.0f);
        const bool shouldWait = ShouldWait(minions);

        if (!IsSupportMode()) {
            auto lastHit = GetLastHitMinion(minions);
            if (lastHit.IsValid()) return CacheTarget(lastHit);
        }

        if (minions.empty() || SubBool("Prioritize", "Turret", true)) {
            auto structure = GetStructure();
            if (structure.IsValid()) return CacheTarget(structure);
        }

        if (m_activeMode != SDK::OrbwalkerMode::LastHit &&
            (m_activeMode != SDK::OrbwalkerMode::LaneClear || !shouldWait)) {
            auto hero = GetBestHero(heroRange);
            if (hero.IsValid()) return CacheTarget(hero);
        }

        if (SubBool("Prioritize", "SpecialMinion", false) &&
            !shouldWait) {
            auto special = GetSpecialMinion();
            if (special.IsValid()) return CacheTarget(special);
        }

        if (m_activeMode == SDK::OrbwalkerMode::Harass ||
            m_activeMode == SDK::OrbwalkerMode::LaneClear ||
            m_activeMode == SDK::OrbwalkerMode::LastHit) {
            auto jungle = GetJungle();
            if (jungle.IsValid()) return CacheTarget(jungle);
        }

        if (CanTurretFarm()) {
            auto turretFarm = GetTurretFarmTarget();
            if (turretFarm.IsValid()) return CacheTarget(turretFarm);
            return CacheTarget(SDK::AIBaseClient());
        }

        if (m_activeMode == SDK::OrbwalkerMode::LaneClear && !shouldWait) {
            auto push = GetPushMinion();
            if (push.IsValid()) return CacheTarget(push);
        }

        if (!shouldWait) {
            auto special = GetSpecialMinion();
            if (special.IsValid()) return CacheTarget(special);
        }

        return CacheTarget(SDK::AIBaseClient());
    }

    SDK::AIBaseClient GetBestHero(float range) const {
        const float extraRange = static_cast<float>(GetFindRange());

        auto targets = SDK::TargetSelector::GetTargets(range, SDK::DamageType::Physical);
        for (const auto& hero : targets) {
            if (hero.IsValid() && IsAttackCandidate(hero, false, extraRange) &&
                CanAttackWithWindWall(hero) && CanOrbObj(hero)) {
                return SDK::AIBaseClient(hero.Address());
            }
        }

        SDK::AIBaseClient best = {};
        float bestScore = FLT_MAX;
        for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
            if (!hero.IsValid() || !IsAttackCandidate(hero, false, extraRange)) continue;
            if (!CanAttackWithWindWall(hero) || !CanOrbObj(hero)) continue;
            const float score = hero.Health() + SDK::ObjectManager::Player().Distance(hero) * 0.15f;
            if (score < bestScore) {
                bestScore = score;
                best = SDK::AIBaseClient(hero.Address());
            }
        }
        return best;
    }

    SDK::AIBaseClient GetBarrel() const {
        const auto player = SDK::ObjectManager::Player();
        for (const auto& barrel : SDK::ObjectManager::Barrels()) {
            if (!IsAttackCandidate(barrel, true)) continue;
            if (barrel.Health() <= 1.0f) return SDK::AIBaseClient(barrel.Address());
            if (barrel.Health() <= 2.0f && barrel.HasBuff("gangplankebarrelactive")) {
                const float speed = GetProjectileSpeed();
                const float travel = speed == FLT_MAX ? 0.0f : 1000.0f * std::max(0.0f, player.Distance(barrel) - player.BoundingRadius()) / speed;
                const float impact = GetAttackCastDelay() * 1000.0f + SDK::Game::Ping() / 2.0f + travel;
                if (impact < 1500.0f) return SDK::AIBaseClient(barrel.Address());
            }
        }
        return {};
    }

    SDK::AIBaseClient GetLastHitMinion(const std::vector<SDK::AIMinionClient>& minions) const {
        const auto player = SDK::ObjectManager::Player();
        const int farmDelay = SubSlider("Farm", "FarmDelay", 30);
        const float projectileSpeed = GetProjectileSpeed();
        SDK::AIBaseClient bestSiege = {};
        SDK::AIBaseClient bestNormal = {};

        for (const auto& minion : minions) {
            if (!minion.IsValid() || !minion.IsAlive() || minion.IsJungleMonster()) continue;
            if (!IsAttackCandidate(minion)) continue;
            const std::string name = minion.CharacterName();
            if (_stricmp(name.c_str(), "jarvanivstandard") == 0) continue;

            if (minion.MaxHealth() <= 10.0f) {
                if (minion.Health() <= 1.0f) return SDK::AIBaseClient(minion.Address());
                continue;
            }

            const float distance = std::max(0.0f, player.Distance(minion) - player.BoundingRadius());
            const float travel = projectileSpeed == FLT_MAX ? 0.0f : 1000.0f * distance / projectileSpeed;
            const int impact = static_cast<int>(GetAttackCastDelay() * 1000.0f - 100.0f + SDK::Game::Ping() / 2.0f + travel);
            const float prediction = SDK::HealthPrediction::GetPrediction(minion, impact, farmDelay, SDK::HealthPredictionType::Default);
            const float damage = player.GetAutoAttackDamage(minion);
            if (prediction <= 0.0f) {
                SDK::OrbwalkingActionArgs args = {};
                args.Sender = player;
                args.Target = minion;
                args.Position = minion.Position();
                args.Type = SDK::OrbwalkingType::NonKillableMinion;
                args.Process = true;
                SDK::Orbwalker::InvokeAction(args);
                continue;
            }
            if (damage > 0.0f && prediction <= damage) {
                const bool siege = name.find("Siege") != std::string::npos || name.find("Super") != std::string::npos;
                if (siege && !bestSiege.IsValid()) bestSiege = SDK::AIBaseClient(minion.Address());
                if (!siege && !bestNormal.IsValid()) bestNormal = SDK::AIBaseClient(minion.Address());
            }
        }

        return bestSiege.IsValid() ? bestSiege : bestNormal;
    }

    SDK::AIBaseClient GetStructure() const {
        if (SubBool("Prioritize", "Turret", true)) {
            for (const auto& turret : SDK::ObjectManager::EnemyTurrets()) {
                if (IsAttackCandidate(turret)) return SDK::AIBaseClient(turret.Address());
            }
        }

        for (const auto& inhibitor : SDK::ObjectManager::EnemyInhibitors()) {
            if (IsAttackCandidate(inhibitor)) return SDK::AIBaseClient(inhibitor.Address());
        }
        auto nexus = SDK::ObjectManager::EnemyNexus();
        return IsAttackCandidate(nexus) ? SDK::AIBaseClient(nexus.Address()) : SDK::AIBaseClient();
    }

    SDK::AIBaseClient GetSpecialMinion() const {
        const auto player = SDK::ObjectManager::Player();

        if (SubBool("Attackable", "SpecialMinions", true)) {
            for (const auto& pet : SDK::ObjectManager::Pets()) {
                if (pet.IsValid() && pet.IsAlive() && !pet.IsAlly() && player.InAutoAttackRange(pet, 45.0f)) {
                    return SDK::AIBaseClient(pet.Address());
                }
            }
        }

        if (SubBool("Attackable", "Wards", true) && m_activeMode != SDK::OrbwalkerMode::Combo) {
            for (const auto& ward : SDK::ObjectManager::Wards()) {
                if (ward.IsValid() && ward.IsAlive() && !ward.IsAlly() && player.InAutoAttackRange(ward, 45.0f)) {
                    return SDK::AIBaseClient(ward.Address());
                }
            }
        }

        if (SubBool("Attackable", "JunglePlant", false) && m_activeMode != SDK::OrbwalkerMode::Combo) {
            for (const auto& plant : SDK::ObjectManager::Plants()) {
                if (plant.IsValid() && plant.IsAlive() && player.InAutoAttackRange(plant, 45.0f)) {
                    return SDK::AIBaseClient(plant.Address());
                }
            }
        }

        return {};
    }

    SDK::AIBaseClient GetJungle() const {
        const auto player = SDK::ObjectManager::Player();
        const bool smallFirst = SubBool("Prioritize", "SmallJungle", false);
        SDK::AIBaseClient best = {};
        float bestHealth = smallFirst ? FLT_MAX : -1.0f;

        for (const auto& jungle : SDK::ObjectManager::JungleMinions()) {
            if (!jungle.IsValid() || !jungle.IsAlive() || jungle.IsPlant()) continue;
            if (!player.InAutoAttackRange(jungle, 45.0f)) continue;
            const float health = jungle.MaxHealth();
            if ((smallFirst && health < bestHealth) || (!smallFirst && health > bestHealth)) {
                bestHealth = health;
                best = SDK::AIBaseClient(jungle.Address());
            }
        }

        return best;
    }

    bool CanTurretFarm() const {
        if (SubList("Farm", "TurretFarm", 0) == 1) return false;
        const auto player = SDK::ObjectManager::Player();
        if (IsSupportMode()) return false;
        if (player.Level() >= SubSlider("Farm", "TurretFramMaxLevel", 13)) return false;
        return player.IsUnderAllyTurret();
    }

    SDK::AIBaseClient GetTurretFarmTarget() const {
        const auto player = SDK::ObjectManager::Player();
        const int farmDelay = SubSlider("Farm", "FarmDelay", 30);
        const float projectileSpeed = GetProjectileSpeed();

        for (const auto& minion : SDK::ObjectManager::EnemyMinions()) {
            if (!minion.IsValid() || !minion.IsAlive() || !IsAttackCandidate(minion)) continue;
            if (!SDK::HealthPrediction::HasTurretAggro(minion)) continue;

            const float distance = std::max(0.0f, player.Distance(minion) - player.BoundingRadius());
            const float travel = projectileSpeed == FLT_MAX ? 0.0f : 1000.0f * distance / projectileSpeed;
            const int impact = static_cast<int>(GetAttackCastDelay() * 1000.0f - 100.0f + SDK::Game::Ping() / 2.0f + travel);
            const float prediction = SDK::HealthPrediction::GetPrediction(minion, impact, farmDelay, SDK::HealthPredictionType::Simulated);
            const float damage = player.GetAutoAttackDamage(minion);
            if (prediction > 0.0f && damage > 0.0f && prediction <= damage) {
                return SDK::AIBaseClient(minion.Address());
            }
        }
        return {};
    }

    SDK::AIBaseClient GetPushMinion() {
        const auto player = SDK::ObjectManager::Player();
        const int farmDelay = SubSlider("Farm", "FarmDelay", 30);

        if (m_laneClearMinion.IsValid() && m_laneClearMinion.IsAlive() && IsAttackCandidate(m_laneClearMinion)) {
            if (m_laneClearMinion.MaxHealth() <= 10.0f) return m_laneClearMinion;
            const float prediction = SDK::HealthPrediction::GetPrediction(
                m_laneClearMinion,
                static_cast<int>(player.AttackDelay() * 2000.0f),
                farmDelay,
                SDK::HealthPredictionType::Simulated);
            const float damage = player.GetAutoAttackDamage(m_laneClearMinion);
            if (prediction >= 2.0f * damage || std::fabs(prediction - m_laneClearMinion.Health()) < 0.001f) return m_laneClearMinion;
        }

        SDK::AIBaseClient best = {};
        float bestHealth = -1.0f;
        for (const auto& minion : SDK::ObjectManager::EnemyMinions()) {
            if (!minion.IsValid() || !minion.IsAlive() || !minion.IsLaneMinion()) continue;
            if (!IsAttackCandidate(minion)) continue;
            const float prediction = SDK::HealthPrediction::GetPrediction(
                minion,
                static_cast<int>(player.AttackDelay() * 2000.0f),
                farmDelay,
                SDK::HealthPredictionType::Simulated);
            const float damage = player.GetAutoAttackDamage(minion);
            if ((prediction >= 2.0f * damage || std::fabs(prediction - minion.Health()) < 0.001f) &&
                minion.Health() > bestHealth) {
                bestHealth = minion.Health();
                best = SDK::AIBaseClient(minion.Address());
            }
        }

        if (best.IsValid()) m_laneClearMinion = best;
        return best;
    }

    void LogAttackFailure(const char* reason, const SDK::AIBaseClient& target) const {
        static int s_lastLog = 0;
        const int now = SDK::Game::TickCount();
        if (now - s_lastLog < 1000) return;
        s_lastLog = now;

        char buffer[512] = {};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "[NightSharp][Orbwalker2] %s mode=%d target=0x%llX valid=%d enemy=%d visible=%d targetable=%d hp=%.1f dist=%.1f range=%.1f canAttack=%d tick=%d lastAA=%d\r\n",
            reason ? reason : "state",
            static_cast<int>(m_activeMode),
            static_cast<unsigned long long>(target.Address()),
            target.IsValid() ? 1 : 0,
            target.IsValid() && target.IsEnemy() ? 1 : 0,
            target.IsValid() && target.IsVisible() ? 1 : 0,
            target.IsValid() && target.IsTargetable() ? 1 : 0,
            target.IsValid() ? target.Health() : 0.0f,
            target.IsValid() ? SDK::ObjectManager::Player().Distance(target) : 0.0f,
            target.IsValid() ? SDK::ObjectManager::Player().GetRealAutoAttackRange(target) : 0.0f,
            CanAttack() ? 1 : 0,
            now,
            m_lastAutoAttackTick);

        HANDLE hFile = CreateFileA(
            "C:\\Users\\Public\\ns_orbwalker2_debug.txt",
            FILE_APPEND_DATA,
            FILE_SHARE_READ,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;
        DWORD written = 0;
        WriteFile(hFile, buffer, static_cast<DWORD>(lstrlenA(buffer)), &written, nullptr);
        CloseHandle(hFile);
    }

    bool Attack(SDK::AIBaseClient& target) {
        const auto player = SDK::ObjectManager::Player();
        if (!target.IsValid() || !IsAttackCandidate(target, target.IsNeutral())) {
            LogAttackFailure("attack-filter", target);
            return false;
        }
        if (!CanOrbObj(target) || !CanAttackWithWindWall(target)) {
            LogAttackFailure("attack-special-filter", target);
            return false;
        }

        SDK::OrbwalkingActionArgs before = {};
        before.Sender = player;
        before.Target = target;
        before.Position = target.Position();
        before.Type = SDK::OrbwalkingType::BeforeAttack;
        before.Process = true;
        SDK::Orbwalker::InvokeAction(before);
        if (!before.Process) {
            LogAttackFailure("before-attack-blocked", target);
            return false;
        }

        if (m_isKalista) m_missileLaunched = false;

        if (player.IssueOrder(SDK::GameObjectOrder::AttackUnit, target)) {
            const int now = SDK::Game::TickCount();
            m_lastLocalAttackTick = now;
            m_pendingAttack = true;
            m_pendingAttackTick = now;
            m_pendingAttackTargetNetId = target.NetworkId();
            m_missileLaunched = false;
            m_lastTarget = target;
            return true;
        }

        LogAttackFailure("issue-attack-failed", target);
        return false;
    }

    void Move(const SDK::Vector3& position) {
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return;

        SDK::Vector3 movePos = position.IsValid() && !position.IsZero() ? position : SDK::Game::CursorPos();
        if (!movePos.IsValid() || movePos.IsZero()) return;

        const float hold = static_cast<float>(std::max(30, OrbSlider("ExtraHold", 50)));
        if (player.Position().Distance2D(movePos) < hold) {
            if (player.IsMoving()) {
                m_lastMovementTick = SDK::Game::TickCount() - 70;
            }
            return;
        }

        if (OrbBool("MoveRandom", false) && player.Position().Distance2D(movePos) < 150.0f) {
            SDK::Vector3 direction = movePos - player.Position();
            const float length = std::sqrt(direction.x * direction.x + direction.z * direction.z);
            if (length > 1.0f) {
                const float randomFactor = 0.8f + static_cast<float>(std::rand() % 40) / 100.0f;
                movePos = SDK::Vector3(
                    player.Position().x + direction.x / length * randomFactor * 400.0f,
                    movePos.y,
                    player.Position().z + direction.z / length * randomFactor * 400.0f);
            }
        }

        const int now = SDK::Game::TickCount();
        const int minDelay = OrbBool("HighOrb", false)
            ? 50 + std::min(60, SDK::Game::Ping())
            : 70 + std::min(60, SDK::Game::Ping());
        if (now - m_lastMovementTick < minDelay) return;
        if (movePos.DistanceSqr2D(m_lastMovePosition) < 25.0f * 25.0f && now - m_lastMovementTick < 220) return;

        SDK::OrbwalkingActionArgs beforeMove = {};
        beforeMove.Sender = player;
        beforeMove.Position = movePos;
        beforeMove.Type = SDK::OrbwalkingType::Movement;
        beforeMove.Process = true;
        SDK::Orbwalker::InvokeAction(beforeMove);
        if (!beforeMove.Process) return;

        if (player.IssueOrder(SDK::GameObjectOrder::MoveTo, beforeMove.Position)) {
            m_lastMovementTick = now;
            m_lastMovePosition = beforeMove.Position;
            if (SubBool("Drawing", "ShowFakeClick", false)) {
                m_lastFakeClickTick = now;
            }
        }
    }

    void Orbwalk(SDK::AIBaseClient& target) {
        const int now = SDK::Game::TickCount();
        if (now - m_lastLocalAttackTick < 70 + std::min(60, SDK::Game::Ping())) return;

        if (m_attackEnabled && CanAttack() && target.IsValid() && Attack(target)) return;

        if (m_moveEnabled && CanMove(static_cast<float>(OrbSlider("WindupDelay", 60)), false)) {
            if (IsComboWithoutMove()) return;
            if (OrbBool("LimitAttack", false)) {
                const auto player = SDK::ObjectManager::Player();
                if (player.AttackDelay() < 0.3846154f &&
                    (m_autoAttackCounter % 3) != 0 &&
                    !CanMove(500.0f, true)) {
                    return;
                }
            }
            Move(SDK::Game::CursorPos());
        }
    }

    static void HideSdkEntry() {
        const int idx = PluginRegistry::FindByInternalId("orbwalker");
        if (idx >= 0) PluginRegistry::Plugins[idx].Loaded = false;
    }

    static void ShowSdkEntry() {
        const int idx = PluginRegistry::FindByInternalId("orbwalker");
        if (idx >= 0) PluginRegistry::Plugins[idx].Loaded = PluginRegistry::Plugins[idx].AlwaysLoad;
    }
};

} // namespace Plugins
