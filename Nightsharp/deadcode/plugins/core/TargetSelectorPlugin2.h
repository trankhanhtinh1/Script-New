#pragma once

#include "../IPlugin.h"
#include "../PluginSdkCompat.h"
#include "../../sdk/Core/Game.h"
#include "../../sdk/Core/Objects.h"
#include "../../sdk/Enumerations/DamageType.h"
#include "../../sdk/Enumerations/TargetSelectorMode.h"
#include "../../sdk/UI/Drawing.h"
#include "../../sdk/UI/UI.h"
#include "../../sdk/Wrappers/Damages/Damage.h"
#include "../../sdk/Wrappers/TargetSelector/ITargetSelectorMode.h"
#include "../../sdk/Wrappers/TargetSelector/TargetSelector.h"
#include "../../sdk/Wrappers/TargetSelector/TargetSelectorSelected.h"
#include "../../sdk/SDK.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins {

class TargetSelectorPlugin2 : public IPlugin {
public:
    const char* GetName()        const override { return "Target Selector 2.0"; }
    const char* GetInternalId()  const override { return "custom_targetselector_v2"; }
    const char* GetAuthor()      const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault()     const override { return false; }

    void OnLoad() override {
        if (m_menu) return;

        m_menu = SDK::Menu::Create("CustomTS2", "Target Selector 2.0");

        auto player = SDK::ObjectManager::Player();
        if (player.IsValid()) m_playerName = player.CharacterName();
        m_modeKey     = m_playerName.empty() ? "TSMode2"     : ("TSMode2_"     + m_playerName);
        m_modeKey2nd  = m_playerName.empty() ? "TSMode2_Sec" : ("TSMode2_Sec_" + m_playerName);

        const std::vector<std::string> kModeItems = {
            "Script", "AP Damage", "AD Damage", "Least HP",
            "Less AA", "Less Casts", "Priority", "Near Mouse", "Close Me"
        };

        m_coreMenu = m_menu->AddSubMenu("CoreSettings", "Core settings");
        if (m_coreMenu) {
            m_coreMenu->Add<SDK::MenuSeparator>("sep_core", "Core");
            m_coreMenu->Add<SDK::MenuList>(m_modeKey, "Main mode", kModeItems, 0);
            m_coreMenu->Add<SDK::MenuList>(m_modeKey2nd, "Secondary mode", kModeItems, 0);
            m_coreMenu->Add<SDK::MenuBool>("DebuffPriority", "Prioritize targets with debuffs", true);
            m_coreMenu->Add<SDK::MenuList>("DistanceDetection", "Distance detection",
                std::vector<std::string>{ "Default", "Normal", "Strict" }, 0);
            m_coreMenu->Add<SDK::MenuBool>("AttackZombie", "Attack zombie", true);
            m_coreMenu->Add<SDK::MenuSeparator>("sep_prio", "Priority");
            m_coreMenu->Add<SDK::MenuSeparator>("sep_prio_hint", "[low] 1  2  3  4  5 [high]");
        }
        m_priorityMenu = m_coreMenu;

        auto* draw = m_menu->AddSubMenu("DrawSettings", "Draw settings");
        if (draw) {
            draw->Add<SDK::MenuSeparator>("sep_noti", "Notification");
            draw->Add<SDK::MenuBool>("NotiEnable", "Enable", true);
            draw->Add<SDK::MenuColor>("TextColor", "Text color", 1.0f, 1.0f, 1.0f, 0.784f);
            draw->Add<SDK::MenuColor>("BackgroundColor", "Background color", 0.0f, 0.0f, 0.0f, 0.392f);
            draw->Add<SDK::MenuColor>("BackgroundColorLock", "-> Only attack selected",
                0.839f, 0.188f, 0.192f, 0.392f);

            draw->Add<SDK::MenuSeparator>("sep_enemies", "Enemies");
            draw->Add<SDK::MenuBool>("EnemiesEnable", "Enable", true);
            draw->Add<SDK::MenuSlider>("EnemiesSize", "Size (%)", 100, 50, 100);
            draw->Add<SDK::MenuColor>("EnemiesColor", "Not selected enemies",
                0.498f, 0.549f, 0.553f, 1.0f);
            draw->Add<SDK::MenuColor>("SelectedColor", "Selected target",
                0.918f, 0.188f, 0.192f, 1.0f);
        }

        auto* forceMenu = m_menu->AddSubMenu("ForceTarget", "Force target");
        if (forceMenu) {
            forceMenu->Add<SDK::MenuKeyBind>("SecondaryModeKey", "Use secondary mode",
                0, SDK::KeyBindType::Press);
            forceMenu->Add<SDK::MenuKeyBind>("ForceTargetKey", "Force target",
                VK_LBUTTON, SDK::KeyBindType::Press);
            forceMenu->Add<SDK::MenuButton>("ChangeToLMB", "-> Change to left mouse", "Click",
                []() {
                    if (!s_instance || !s_instance->m_menu) return;
                    auto* force = s_instance->m_menu->GetSubMenu("ForceTarget");
                    if (!force) return;
                    if (auto* kb = force->Get<SDK::MenuKeyBind>("ForceTargetKey"))
                        kb->Key = VK_LBUTTON;
                });
            forceMenu->Add<SDK::MenuSlider>("Timeout", "Time out (sec)", 5, 0, 10);
            forceMenu->Add<SDK::MenuSlider>("MouseRange", "Mouse range", 400, 0, 1000);
            forceMenu->Add<SDK::MenuKeyBind>("OnlySelectKey", "Only attack select / boss",
                0, SDK::KeyBindType::Toggle);
        }

        auto* bossMenu = m_menu->AddSubMenu("BossMode", "Boss mode");
        if (bossMenu) {
            bossMenu->Add<SDK::MenuList>("Boss", "Boss",
                std::vector<std::string>{ "None" }, 0);
        }

        s_instance = this;
        EnsurePrioritySliders();
        EnsureBossOptions();

        SDK::TargetSelector::SetOverride(
            &TargetSelectorPlugin2::SDKGetTargetFn,
            &TargetSelectorPlugin2::SDKGetTargetsFn,
            &TargetSelectorPlugin2::SDKGetSelectedFn,
            &TargetSelectorPlugin2::SDKSetSelectedFn,
            &TargetSelectorPlugin2::SDKClearSelectedFn,
            &TargetSelectorPlugin2::SDKGetPriorityFn);
    }

    void OnUnload() override {
        if (s_instance == this) {
            SDK::TargetSelector::ClearOverride();
            s_instance = nullptr;
        }
        SDK::TargetSelectorSelected::ClearForcedTarget();
        SDK::TargetSelectorSelected::ClearTarget();
        m_selectedTarget = SDK::AIHeroClient();

        if (m_menu) {
            SDK::Menu::Remove("CustomTS2");
            m_menu         = nullptr;
            m_priorityMenu = nullptr;
            m_coreMenu     = nullptr;
        }
    }

    SDK::MenuUI::Menu* GetMenuRoot() override { return m_menu; }

    void OnUpdate() override {
        if (!m_menu) return;
        EnsurePrioritySliders();
        EnsureBossOptions();
        SyncSDKMode();
        HandleForceTarget();
        SyncSelectionToSDK();
    }

    void OnRender() override {
        if (!m_menu) return;
        auto* draw = m_menu->GetSubMenu("DrawSettings");
        if (!draw) return;

        const auto bossTarget = ResolveBossTarget();
        const uint32_t bossNetId = bossTarget.IsValid() ? static_cast<uint32_t>(bossTarget.NetworkId()) : 0u;
        const uint32_t selNetId  = m_selectedTarget.IsValid() ? static_cast<uint32_t>(m_selectedTarget.NetworkId()) : 0u;

        auto* drawEnemies = draw->Get<SDK::MenuBool>("EnemiesEnable");
        if (drawEnemies && drawEnemies->Enabled) {
            auto* sizeItem     = draw->Get<SDK::MenuSlider>("EnemiesSize");
            auto* enemyColItem = draw->Get<SDK::MenuColor>("EnemiesColor");
            auto* selColItem   = draw->Get<SDK::MenuColor>("SelectedColor");

            const float scale = sizeItem ? (static_cast<float>(sizeItem->Value) / 100.0f) : 1.0f;
            const ImU32 enemyCol = enemyColItem ? enemyColItem->GetImU32() : IM_COL32(127, 140, 141, 255);
            const ImU32 selCol   = selColItem   ? selColItem->GetImU32()   : IM_COL32(234, 48, 49, 255);

            const bool enemyColVisible = enemyColItem ? enemyColItem->Color[3] > 0.18f : true;
            const bool selColVisible   = selColItem   ? selColItem->Color[3]   > 0.18f : true;

            for (const auto& enemy : SDK::ObjectManager::EnemyHeroes()) {
                if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) continue;
                const uint32_t nid = static_cast<uint32_t>(enemy.NetworkId());
                const bool isSel = (nid == bossNetId || nid == selNetId);
                if (isSel && !selColVisible) continue;
                if (!isSel && !enemyColVisible) continue;
                const ImU32 col = isSel ? selCol : enemyCol;
                const float radius = (enemy.BoundingRadius() + 10.0f) * scale;
                SDK::Drawing::DrawCircle(enemy.Position(), radius, col, 2.0f);
            }
        }

        const SDK::AIHeroClient& activeTarget = bossTarget.IsValid() ? bossTarget : m_selectedTarget;
        DrawNotification(draw, activeTarget);
    }

    SDK::AIHeroClient GetTarget(float range,
                                SDK::DamageType damageType = SDK::DamageType::Physical,
                                const SDK::Vector3& from = SDK::Vector3()) {
        if (!m_menu) return SDK::AIHeroClient();
        const auto origin = from.IsZero() ? SDK::ObjectManager::Player().Position() : from;

        const auto boss = ResolveBossTarget();
        if (boss.IsValid() && IsTargetValid(boss, range, origin)) return boss;

        if (IsOnlyAttackSelected()) {
            if (m_selectedTarget.IsValid() && IsTargetValid(m_selectedTarget, range, origin))
                return m_selectedTarget;
            return SDK::AIHeroClient();
        }

        auto targets = GetValidTargets(range, origin);
        if (targets.empty()) return SDK::AIHeroClient();

        if (m_selectedTarget.IsValid() && !m_selectedTarget.IsDead()) {
            for (const auto& t : targets) {
                if (static_cast<uint32_t>(t.NetworkId()) == static_cast<uint32_t>(m_selectedTarget.NetworkId()))
                    return m_selectedTarget;
            }
        }

        return SelectBest(targets, damageType, origin);
    }

    std::vector<SDK::AIHeroClient> GetTargets(float range,
                                              SDK::DamageType damageType = SDK::DamageType::Physical,
                                              const SDK::Vector3& from = SDK::Vector3()) {
        if (!m_menu) return {};
        const auto origin = from.IsZero() ? SDK::ObjectManager::Player().Position() : from;

        const auto boss = ResolveBossTarget();
        if (boss.IsValid() && IsTargetValid(boss, range, origin)) return { boss };

        if (IsOnlyAttackSelected()) {
            if (m_selectedTarget.IsValid() && IsTargetValid(m_selectedTarget, range, origin))
                return { m_selectedTarget };
            return {};
        }

        auto targets = GetValidTargets(range, origin);
        SortTargets(targets, damageType, origin);

        if (m_selectedTarget.IsValid() && !m_selectedTarget.IsDead()) {
            for (auto it = targets.begin(); it != targets.end(); ++it) {
                if (static_cast<uint32_t>(it->NetworkId()) == static_cast<uint32_t>(m_selectedTarget.NetworkId())) {
                    auto sel = *it;
                    targets.erase(it);
                    targets.insert(targets.begin(), sel);
                    break;
                }
            }
        }

        return targets;
    }

    SDK::AIHeroClient GetSelectedTarget() const { return m_selectedTarget; }

    void SetSelectedTarget(const SDK::AIHeroClient& target) {
        m_selectedTarget = (target.IsValid() && target.IsHero()) ? target : SDK::AIHeroClient();
        if (m_selectedTarget.IsValid()) {
            if (auto* force = m_menu ? m_menu->GetSubMenu("ForceTarget") : nullptr) {
                auto* t = force->Get<SDK::MenuSlider>("Timeout");
                float timeout = t ? static_cast<float>(t->Value) : 5.0f;
                m_forceExpireTime = SDK::Game::Time() + timeout;
            }
        }
        SyncSelectionToSDK();
    }

    void ClearSelectedTarget() {
        m_selectedTarget = SDK::AIHeroClient();
        SDK::TargetSelectorSelected::ClearTarget();
        SDK::TargetSelectorSelected::ClearForcedTarget();
    }

    int GetPriority(const SDK::AIHeroClient& hero) {
        if (!hero.IsValid()) return 1;
        const std::string name = hero.CharacterName();
        if (name.empty()) return 1;
        if (m_priorityMenu) {
            if (auto* s = m_priorityMenu->Get<SDK::MenuSlider>("TS2_" + name))
                return s->Value;
        }
        return GetDefaultPriority(name);
    }

private:
    enum class Mode : int {
        Script    = 0,
        APDamage  = 1,
        ADDamage  = 2,
        LeastHP   = 3,
        LessAA    = 4,
        LessCasts = 5,
        Priority  = 6,
        Mouse     = 7,
        CloseMe   = 8
    };

    static inline TargetSelectorPlugin2* s_instance = nullptr;

    SDK::MenuUI::Menu* m_menu         = nullptr;
    SDK::MenuUI::Menu* m_coreMenu     = nullptr;
    SDK::MenuUI::Menu* m_priorityMenu = nullptr;
    SDK::AIHeroClient  m_selectedTarget;
    std::string        m_modeKey;
    std::string        m_modeKey2nd;
    std::string        m_playerName;
    bool               m_bossMenuBuilt   = false;
    float              m_forceExpireTime = 0.0f;
    bool               m_wasForceKeyDown = false;
    float              m_forceKeyPressTime = 0.0f;
    float              m_lastSetForceTime  = 0.0f;

    static SDK::AIHeroClient SDKGetTargetFn(float r, SDK::DamageType dt, const SDK::Vector3& f) {
        return s_instance ? s_instance->GetTarget(r, dt, f) : SDK::AIHeroClient();
    }
    static std::vector<SDK::AIHeroClient> SDKGetTargetsFn(float r, SDK::DamageType dt, const SDK::Vector3& f) {
        return s_instance ? s_instance->GetTargets(r, dt, f) : std::vector<SDK::AIHeroClient>();
    }
    static SDK::AIHeroClient SDKGetSelectedFn() {
        return s_instance ? s_instance->GetSelectedTarget() : SDK::AIHeroClient();
    }
    static void SDKSetSelectedFn(const SDK::AIHeroClient& t) {
        if (s_instance) s_instance->SetSelectedTarget(t);
    }
    static void SDKClearSelectedFn() {
        if (s_instance) s_instance->ClearSelectedTarget();
    }
    static int SDKGetPriorityFn(const SDK::AIHeroClient& t) {
        return s_instance ? s_instance->GetPriority(t) : 1;
    }

    int GetActiveModeIndex() const {
        if (!m_coreMenu) return 0;
        auto* force = m_menu ? m_menu->GetSubMenu("ForceTarget") : nullptr;
        auto* secKey = force ? force->Get<SDK::MenuKeyBind>("SecondaryModeKey") : nullptr;
        if (secKey && secKey->Active) {
            auto* secList = m_coreMenu->Get<SDK::MenuList>(m_modeKey2nd);
            return secList ? std::clamp(secList->Index, 0, 8) : 0;
        }
        auto* modeList = m_coreMenu->Get<SDK::MenuList>(m_modeKey);
        return modeList ? std::clamp(modeList->Index, 0, 8) : 0;
    }

    Mode MapScriptMode(SDK::DamageType dt) const {
        switch (dt) {
        case SDK::DamageType::Physical: return Mode::ADDamage;
        case SDK::DamageType::Magical:  return Mode::APDamage;
        default:                        return Mode::LeastHP;
        }
    }

    static float SafeGetAAScore(const SDK::AIHeroClient& player, const SDK::AIHeroClient& target) {
        float dmg = 0.0f;
        __try {
            dmg = SDK::Damage::GetAutoAttackDamage(player, target, true);
        } __except(1) {
            dmg = 0.0f;
        }
        if (dmg <= 0.0f && player.TotalAttackDamage() > 0.0f)
            dmg = player.GetAutoAttackDamage(target, false);
        if (dmg <= 0.0f) dmg = 1.0f;
        return target.Health() / dmg;
    }

    float GetScore(const SDK::AIHeroClient& hero, Mode mode, SDK::DamageType dt,
                   const SDK::Vector3& playerPos, const SDK::Vector3& cursorPos) {
        using IMode = SDK::TargetSelectorModes::ITargetSelectorMode;
        switch (mode) {
        case Mode::APDamage:
            return IMode::EffectiveHealth(hero, SDK::DamageType::Magical);
        case Mode::ADDamage:
            return IMode::EffectiveHealth(hero, SDK::DamageType::Physical);
        case Mode::LeastHP:
        case Mode::LessCasts:
            return hero.Health();
        case Mode::LessAA: {
            auto player = SDK::ObjectManager::Player();
            return player.IsValid() ? SafeGetAAScore(player, hero) : hero.Health();
        }
        case Mode::Priority:
            return -(static_cast<float>(GetPriority(hero)));
        case Mode::Mouse:
            return hero.Distance(cursorPos);
        case Mode::CloseMe:
            return hero.Distance(playerPos);
        case Mode::Script:
        default:
            return GetScore(hero, MapScriptMode(dt), dt, playerPos, cursorPos);
        }
    }

    float ApplyDebuffBonus(float score, const SDK::AIHeroClient& hero) const {
        if (!m_coreMenu) return score;
        auto* item = m_coreMenu->Get<SDK::MenuBool>("DebuffPriority");
        if (item && item->Enabled && Compat::HasMovementLock(hero))
            return score * 0.8f;
        return score;
    }

    bool IsOnlyAttackSelected() const {
        if (!m_menu) return false;
        auto* force = m_menu->GetSubMenu("ForceTarget");
        if (!force) return false;
        auto* kb = force->Get<SDK::MenuKeyBind>("OnlySelectKey");
        return kb && kb->Active;
    }

    bool IsAttackZombie() const {
        if (!m_coreMenu) return true;
        auto* item = m_coreMenu->Get<SDK::MenuBool>("AttackZombie");
        return item ? item->Enabled : true;
    }

    float GetDistanceMultiplier() const {
        if (!m_coreMenu) return 1.0f;
        auto* item = m_coreMenu->Get<SDK::MenuList>("DistanceDetection");
        if (!item) return 1.0f;
        switch (item->Index) {
        case 1:  return 0.9f;
        case 2:  return 0.8f;
        default: return 1.0f;
        }
    }

    bool IsTargetValid(const SDK::AIHeroClient& target, float range,
                       const SDK::Vector3& origin) const {
        if (!target.IsValid() || target.IsDead() || !target.IsVisible() || !target.IsTargetable())
            return false;

        if (Compat::IsZombieLike(target) && !IsAttackZombie())
            return false;

        static const char* kInvuln[] = {
            "KayleR", "TryndamereR", "kindaborroweytime", "ChronoShift", "UndyingRage", nullptr
        };
        for (const char** p = kInvuln; *p; ++p)
            if (target.HasBuff(*p)) return false;

        if (range > 0.0f) {
            const float effectiveRange = (range + target.BoundingRadius()) * GetDistanceMultiplier();
            if (target.Distance(origin) > effectiveRange)
                return false;
        }

        return true;
    }

    std::vector<SDK::AIHeroClient> GetValidTargets(float range, const SDK::Vector3& origin) const {
        std::vector<SDK::AIHeroClient> result;
        for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
            if (IsTargetValid(hero, range, origin))
                result.push_back(hero);
        }
        return result;
    }

    SDK::AIHeroClient SelectBest(std::vector<SDK::AIHeroClient>& targets,
                                 SDK::DamageType dt,
                                 const SDK::Vector3& origin) {
        if (targets.empty()) return SDK::AIHeroClient();

        const Mode mode      = static_cast<Mode>(GetActiveModeIndex());
        const auto player    = SDK::ObjectManager::Player();
        const auto playerPos = player.IsValid() ? player.Position() : origin;
        const auto cursor    = SDK::Game::CursorPos();
        const bool zombie    = IsAttackZombie();

        const SDK::AIHeroClient* best = nullptr;
        float bestScore = FLT_MAX;

        for (const auto& hero : targets) {
            if (!hero.IsValid()) continue;
            float score = (zombie && Compat::IsZombieLike(hero))
                ? 1e30f
                : ApplyDebuffBonus(GetScore(hero, mode, dt, playerPos, cursor), hero);
            if (score < bestScore) { bestScore = score; best = &hero; }
        }

        return best ? *best : SDK::AIHeroClient();
    }

    void SortTargets(std::vector<SDK::AIHeroClient>& targets,
                     SDK::DamageType dt,
                     const SDK::Vector3& origin) {
        if (targets.size() < 2) return;

        const Mode mode      = static_cast<Mode>(GetActiveModeIndex());
        const auto player    = SDK::ObjectManager::Player();
        const auto playerPos = player.IsValid() ? player.Position() : origin;
        const auto cursor    = SDK::Game::CursorPos();
        const bool zombie    = IsAttackZombie();

        std::stable_sort(targets.begin(), targets.end(),
            [&](const SDK::AIHeroClient& a, const SDK::AIHeroClient& b) {
                if (!a.IsValid()) return false;
                if (!b.IsValid()) return true;
                bool az = zombie && Compat::IsZombieLike(a);
                bool bz = zombie && Compat::IsZombieLike(b);
                if (az != bz) return bz;
                if (az && bz) return false;
                float sa = ApplyDebuffBonus(GetScore(const_cast<SDK::AIHeroClient&>(a), mode, dt, playerPos, cursor), a);
                float sb = ApplyDebuffBonus(GetScore(const_cast<SDK::AIHeroClient&>(b), mode, dt, playerPos, cursor), b);
                return sa < sb;
            });
    }

    SDK::AIHeroClient ResolveBossTarget() const {
        if (!m_menu) return SDK::AIHeroClient();
        auto* bossMenu = m_menu->GetSubMenu("BossMode");
        if (!bossMenu) return SDK::AIHeroClient();
        auto* bossList = bossMenu->Get<SDK::MenuList>("Boss");
        if (!bossList || bossList->Index <= 0) return SDK::AIHeroClient();
        int idx = bossList->Index;
        if (idx >= static_cast<int>(bossList->Items.size())) return SDK::AIHeroClient();
        const std::string& bossName = bossList->Items[static_cast<size_t>(idx)];
        if (bossName.empty() || bossName == "None") return SDK::AIHeroClient();
        for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
            if (!hero.IsValid() || hero.IsDead()) continue;
            const std::string name = hero.CharacterName();
            if (!name.empty() && _stricmp(name.c_str(), bossName.c_str()) == 0)
                return hero;
        }
        return SDK::AIHeroClient();
    }

    void EnsureBossOptions() {
        if (m_bossMenuBuilt || !m_menu) return;
        auto* bossMenu = m_menu->GetSubMenu("BossMode");
        if (!bossMenu) return;
        auto* bossList = bossMenu->Get<SDK::MenuList>("Boss");
        if (!bossList) return;
        auto enemies = SDK::ObjectManager::EnemyHeroes();
        if (enemies.empty()) return;
        bossList->Items.clear();
        bossList->Items.push_back("None");
        for (const auto& hero : enemies) {
            if (!hero.IsValid()) continue;
            std::string name = hero.CharacterName();
            if (!name.empty()) bossList->Items.push_back(name);
        }
        if (bossList->Index >= static_cast<int>(bossList->Items.size()))
            bossList->Index = 0;
        m_bossMenuBuilt = true;
    }

    int GetForceTargetKey() const {
        if (!m_menu) return VK_LBUTTON;
        auto* force = m_menu->GetSubMenu("ForceTarget");
        if (!force) return VK_LBUTTON;
        auto* kb = force->Get<SDK::MenuKeyBind>("ForceTargetKey");
        if (!kb || kb->Key <= 0) return VK_LBUTTON;
        return kb->Key;
    }

    void HandleForceTarget() {
        if (!m_menu) return;
        auto* forceMenu = m_menu->GetSubMenu("ForceTarget");
        if (!forceMenu) return;

        float mouseRange = 400.0f;
        float timeout    = 5.0f;
        if (auto* r = forceMenu->Get<SDK::MenuSlider>("MouseRange"))
            mouseRange = static_cast<float>(r->Value);
        if (auto* t = forceMenu->Get<SDK::MenuSlider>("Timeout"))
            timeout = static_cast<float>(t->Value);

        const int  forceKey = GetForceTargetKey();
        const bool keyDown  = (GetAsyncKeyState(forceKey) & 0x8000) != 0;
        const float gameTime = SDK::Game::Time();

        if (keyDown && !m_wasForceKeyDown) {
            m_forceKeyPressTime = gameTime;
        } else if (!keyDown && m_wasForceKeyDown) {
            if ((gameTime - m_forceKeyPressTime) < 0.3f &&
                (gameTime - m_lastSetForceTime) > 1.0f) {
                auto cursor = SDK::Game::CursorPos();
                if (!cursor.IsZero()) {
                    SDK::AIHeroClient nearest;
                    float bestDist = mouseRange;
                    for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
                        if (!hero.IsValid() || hero.IsDead() || !hero.IsVisible()) continue;
                        float dist = hero.Distance(cursor);
                        if (dist < bestDist) { bestDist = dist; nearest = hero; }
                    }
                    if (nearest.IsValid()) {
                        m_selectedTarget    = nearest;
                        m_forceExpireTime   = gameTime + timeout;
                        m_lastSetForceTime  = gameTime;
                    } else {
                        m_selectedTarget = SDK::AIHeroClient();
                    }
                }
            }
        }
        m_wasForceKeyDown = keyDown;

        if (m_selectedTarget.IsValid()) {
            if (m_selectedTarget.IsDead()) {
                m_selectedTarget = SDK::AIHeroClient();
            } else if (m_selectedTarget.IsVisible()) {
                m_forceExpireTime = gameTime + timeout;
            } else if (gameTime > m_forceExpireTime) {
                m_selectedTarget = SDK::AIHeroClient();
            }
        }
    }

    void SyncSelectionToSDK() {
        if (m_selectedTarget.IsValid() && !m_selectedTarget.IsDead() && m_selectedTarget.IsVisible()) {
            SDK::TargetSelectorSelected::SetTarget(m_selectedTarget);
            if (IsOnlyAttackSelected())
                SDK::TargetSelectorSelected::SetForcedTarget(m_selectedTarget);
            else
                SDK::TargetSelectorSelected::SetForcedTarget(m_selectedTarget);
        } else {
            SDK::TargetSelectorSelected::ClearTarget();
            SDK::TargetSelectorSelected::ClearForcedTarget();
        }
    }

    void SyncSDKMode() {
        static const SDK::TargetSelectorMode kMap[] = {
            SDK::TargetSelectorMode::Priority,
            SDK::TargetSelectorMode::MostAbilityPower,
            SDK::TargetSelectorMode::MostAttackDamage,
            SDK::TargetSelectorMode::LeastHealth,
            SDK::TargetSelectorMode::LessAttacksToKill,
            SDK::TargetSelectorMode::LessCastsToKill,
            SDK::TargetSelectorMode::Priority,
            SDK::TargetSelectorMode::NearMouse,
            SDK::TargetSelectorMode::Closest,
        };
        int idx = GetActiveModeIndex();
        if (idx >= 0 && idx < 9) SDK::TargetSelector::SetMode(kMap[idx]);
    }

    void EnsurePrioritySliders() {
        if (!m_priorityMenu) return;
        for (const auto& enemy : SDK::ObjectManager::EnemyHeroes()) {
            if (!enemy.IsValid()) continue;
            const std::string name = enemy.CharacterName();
            if (name.empty()) continue;
            const std::string key = "TS2_" + name;
            if (!m_priorityMenu->Get<SDK::MenuSlider>(key))
                m_priorityMenu->Add<SDK::MenuSlider>(key, name, GetDefaultPriority(name), 0, 5);
        }
    }

    void DrawNotification(SDK::MenuUI::Menu* draw, const SDK::AIHeroClient& target) {
        if (!draw) return;

        const bool onlySelect = IsOnlyAttackSelected();
        auto* noti = draw->Get<SDK::MenuBool>("NotiEnable");
        const bool notiEnabled = noti ? noti->Enabled : true;

        if (!onlySelect && (!notiEnabled || !target.IsValid())) return;
        if (!target.IsValid() || target.IsDead()) return;

        std::string name = target.CharacterName();
        if (name.empty()) return;

        std::string label = onlySelect ? ("LOCKED: " + name) : ("Selected target: " + name);

        auto* txtCol = draw->Get<SDK::MenuColor>("TextColor");
        auto* bgCol  = draw->Get<SDK::MenuColor>("BackgroundColor");
        auto* lockCol= draw->Get<SDK::MenuColor>("BackgroundColorLock");

        const ImU32 textU32 = txtCol ? txtCol->GetImU32() : IM_COL32(255, 255, 255, 200);
        const ImU32 bgU32   = onlySelect
            ? (lockCol ? lockCol->GetImU32() : IM_COL32(214, 48, 49, 100))
            : (bgCol   ? bgCol->GetImU32()   : IM_COL32(0, 0, 0, 100));

        auto* dl = ImGui::GetForegroundDrawList();
        if (!dl) return;

        ImVec2 sz = ImGui::CalcTextSize(label.c_str());
        float x = ImGui::GetIO().DisplaySize.x * 0.5f - sz.x * 0.5f;
        float y = 60.0f;
        dl->AddRectFilled(
            ImVec2(x - 8.0f, y - 4.0f),
            ImVec2(x + sz.x + 8.0f, y + sz.y + 4.0f),
            bgU32, 4.0f);
        dl->AddText(ImVec2(x, y), textU32, label.c_str());
    }

    static int GetDefaultPriority(const std::string& name) {
        static const char* maxPrio[] = {
            "Ahri","Aphelios","Anivia","Annie","Ashe","Azir","Brand","Caitlyn",
            "Cassiopeia","Corki","Draven","Ezreal","Graves","Jinx","Kalista",
            "Kaisa","Karma","Karthus","Katarina","Kennen","KogMaw","Kindred",
            "Leblanc","Lucian","Lux","Malzahar","MasterYi","MissFortune","Neeko",
            "Orianna","Quinn","Sivir","Sylas","Syndra","Talon","Teemo","Tristana",
            "TwistedFate","Twitch","Varus","Vayne","Veigar","Velkoz","Viktor",
            "Xerath","Zed","Ziggs","Jhin","Soraka","AurelionSol","Taliyah",
            "Qiyana","Zoe","Xayah","Samira","Zeri","Nilah","Smolder", nullptr
        };
        static const char* highPrio[] = {
            "Akali","Diana","Ekko","FiddleSticks","Fiora","Fizz","Heimerdinger",
            "Jayce","Kassadin","Kayle","KhaZix","Lissandra","Mordekaiser","Nidalee",
            "Riven","Senna","Shaco","Vladimir","Yasuo","Zilean","Camille","Kayn",
            "Yone","Viego","Gwen","Akshan","Belveth", nullptr
        };
        static const char* medPrio[] = {
            "Aatrox","Darius","Elise","Evelynn","Galio","Gangplank","Gragas",
            "Irelia","Jax","LeeSin","Maokai","Morgana","Nocturne","Pantheon",
            "Poppy","Pyke","Rengar","Rumble","Ryze","Sett","Swain","Trundle",
            "Tryndamere","Udyr","Urgot","Vi","XinZhao","RekSai","Illaoi","Kled",
            "Lillia","Vex","Renata", nullptr
        };
        static const char* lowPrio[] = {
            "Alistar","Amumu","Bard","Blitzcrank","Braum","Chogath","DrMundo",
            "Garen","Gnar","Hecarim","Janna","JarvanIV","Leona","Lulu","Malphite",
            "Nami","Nasus","Nautilus","Nunu","Olaf","Rammus","Renekton","Sejuani",
            "Shen","Shyvana","Singed","Sion","Skarner","Sona","Taric","TahmKench",
            "Thresh","Volibear","Warwick","MonkeyKing","Yorick","Yuumi","Zac","Zyra",
            "Ornn","Rakan","Ivern","Rell","KSante","Milio", nullptr
        };
        for (const char** p = maxPrio; *p; ++p) if (_stricmp(name.c_str(), *p) == 0) return 5;
        for (const char** p = highPrio; *p; ++p) if (_stricmp(name.c_str(), *p) == 0) return 4;
        for (const char** p = medPrio; *p; ++p)  if (_stricmp(name.c_str(), *p) == 0) return 3;
        for (const char** p = lowPrio; *p; ++p)  if (_stricmp(name.c_str(), *p) == 0) return 2;
        return 1;
    }
};

} // namespace Plugins
