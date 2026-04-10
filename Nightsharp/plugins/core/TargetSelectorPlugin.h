#pragma once

// ============================================================================
// TargetSelectorPlugin — Custom Target Selector (Overrides SDK TargetSelector)
//
// Port from ImGui-DirectX-11-Kiero-Hook-master version.
// Modes: Smart AD/AP, Lowest Health, Most Priority
//
// When loaded:
//   1. Hides the SDK's built-in TargetSelector from PluginRegistry
//   2. Disables SDK TS rendering (no double drawing)
//   3. Hooks into SDK::TargetSelector via TargetSelectorSelected (SetForcedTarget)
//      so Orbwalker + Champion scripts transparently use this plugin's selection
//   4. Provides its own menu, click-select, priority sliders, and drawing
//
// When unloaded:
//   1. Restores SDK TargetSelector visibility in PluginRegistry
//   2. Removes hooks / clears forced target
// ============================================================================

#include "../IPlugin.h"
#include "../PluginSdkCompat.h"
#include "../../menu/PluginRegistry.h"
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

class TargetSelectorPlugin : public IPlugin {
public:
    const char* GetName()       const override { return "Target Selector"; }
    const char* GetInternalId() const override { return "custom_targetselector"; }
    const char* GetAuthor()     const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault()    const override { return true; }

    // ──────────────────────────────────────────────────────────
    // Lifecycle
    // ──────────────────────────────────────────────────────────

    void OnLoad() override {
        if (m_menu) return;

        // ── Build our own menu tree ──
        m_menu = SDK::Menu::Create("CustomTS", "Target Selector [Plugin]");

        // Per-player mode key (like original: TSMode_ChampName)
        std::string playerName;
        auto player = SDK::ObjectManager::Player();
        if (player.IsValid()) {
            playerName = player.CharacterName();
        }
        m_modeKey = playerName.empty() ? "TSMode" : ("TSMode_" + playerName);

        // Priority sub-menu
        m_priorityMenu = m_menu->AddSubMenu("Priority", "Priority");

        // Drawings sub-menu (matches NewTargetSelector.cs)
        auto* drawMenu = m_menu->AddSubMenu("Drawings", "Drawings");
        drawMenu->Add<SDK::MenuColor>("SelectColor", "^ Draw Color", 1.0f, 0.0f, 0.0f, 1.0f);
        drawMenu->Add<SDK::MenuBool>("DrawSelect", "Draw Selected Target", true);
        drawMenu->Add<SDK::MenuBool>("LightSelect", "HighLight Selected Target", true);

        // Main options
        m_menu->Add<SDK::MenuBool>("ForceSelectTarget", "Force on Select Target", true);
        m_menu->Add<SDK::MenuBool>("OnlySelectTarget", "Only Attack Select Target", false);
        m_menu->Add<SDK::MenuList>(
            m_modeKey,
            "TS Mode",
            std::vector<std::string>{ "Smart AD/AP", "Lowest Health", "Most Priority" },
            0);

        EnsurePrioritySliders();
        s_instance = this;

        // ── Override SDK TargetSelector ──
        HideSDKTargetSelector();
        SDK::TargetSelector::SetOverride(
            &TargetSelectorPlugin::SDKGetTargetOverride,
            &TargetSelectorPlugin::SDKGetTargetsOverride,
            &TargetSelectorPlugin::SDKGetSelectedTargetOverride,
            &TargetSelectorPlugin::SDKSetSelectedTargetOverride,
            &TargetSelectorPlugin::SDKClearSelectedTargetOverride,
            &TargetSelectorPlugin::SDKGetPriorityOverride);

        // Tell SDK to use our selection via the TargetSelectorSelected hook:
        // We'll push our selected/forced target each update.
    }

    void OnUnload() override {
        // ── Restore SDK TargetSelector ──
        RestoreSDKTargetSelector();
        if (s_instance == this) {
            SDK::TargetSelector::ClearOverride();
            s_instance = nullptr;
        }

        SDK::TargetSelectorSelected::ClearForcedTarget();
        SDK::TargetSelectorSelected::ClearTarget();

        m_selectedTarget = SDK::AIHeroClient();

        if (m_menu) {
            SDK::Menu::Remove("CustomTS");
            m_menu = nullptr;
            m_priorityMenu = nullptr;
        }
    }

    SDK::MenuUI::Menu* GetMenuRoot() override { return m_menu; }

    // ──────────────────────────────────────────────────────────
    // OnUpdate — Runs every tick
    // ──────────────────────────────────────────────────────────
    void OnUpdate() override {
        if (!m_menu) return;

        EnsurePrioritySliders();

        // Sync SDK mode enum with our menu (so SDK internal code stays consistent)
        if (auto* modeList = m_menu->Get<SDK::MenuList>(m_modeKey)) {
            static const SDK::TargetSelectorMode modeMap[] = {
                SDK::TargetSelectorMode::Priority,     // Smart AD/AP → Priority (closest match)
                SDK::TargetSelectorMode::LeastHealth,
                SDK::TargetSelectorMode::Priority
            };
            int idx = std::clamp(modeList->Index, 0, 2);
            SDK::TargetSelector::SetMode(modeMap[idx]);
        }

        // Handle click-based target selection
        HandleClickSelect();

        // Push our selected target into SDK's TargetSelectorSelected
        // This is the KEY integration point: Orbwalker and champion scripts
        // call SDK::TargetSelector::GetTarget() which checks
        // TargetSelectorSelected::Target() / ForcedTarget().
        SyncSelectionToSDK();
    }

    // ──────────────────────────────────────────────────────────
    // OnRender — Drawing
    // ──────────────────────────────────────────────────────────
    void OnRender() override {
        if (!m_menu) return;

        auto* drawMenu = m_menu->GetSubMenu("Drawings");
        if (!drawMenu) return;

        auto* drawSelect = drawMenu->Get<SDK::MenuBool>("DrawSelect");
        if (drawSelect && drawSelect->Enabled &&
            m_selectedTarget.IsValid() && !m_selectedTarget.IsDead()) {
            auto* color = drawMenu->Get<SDK::MenuColor>("SelectColor");
            ImU32 col = color ? color->GetImU32() : IM_COL32(255, 0, 0, 255);
            SDK::Drawing::DrawCircle(m_selectedTarget.Position(),
                m_selectedTarget.BoundingRadius(), col, 3.0f);

            auto* light = drawMenu->Get<SDK::MenuBool>("LightSelect");
            if (light && light->Enabled) {
                float t = SDK::Game::Time();
                float pulse = 6.0f + 6.0f * (0.5f + 0.5f * sinf(t * 6.0f));
                SDK::Drawing::DrawCircle(m_selectedTarget.Position(),
                    m_selectedTarget.BoundingRadius() + pulse,
                    IM_COL32(180, 80, 255, 190), 2.0f);
            }
        }
    }

    // ──────────────────────────────────────────────────────────
    // Public Selection API — called by GetTarget / GetTargets
    // ──────────────────────────────────────────────────────────

    SDK::AIHeroClient GetTarget(float range,
                                SDK::DamageType damageType = SDK::DamageType::Physical,
                                const SDK::Vector3& from = SDK::Vector3()) {
        if (!m_menu) return SDK::AIHeroClient();

        // OnlySelectTarget mode
        auto* only = m_menu->Get<SDK::MenuBool>("OnlySelectTarget");
        if (only && only->Enabled && m_selectedTarget.IsValid()) {
            if (IsValidTarget(m_selectedTarget, range, from))
                return m_selectedTarget;
            return SDK::AIHeroClient();
        }

        auto targets = GetValidTargets(range, from);
        if (targets.empty()) return SDK::AIHeroClient();

        // Force selected target if in range
        if (m_selectedTarget.IsValid() && !m_selectedTarget.IsDead()) {
            auto* force = m_menu->Get<SDK::MenuBool>("ForceSelectTarget");
            if (force && force->Enabled) {
                for (auto& t : targets) {
                    if (t.NetworkId() == m_selectedTarget.NetworkId())
                        return m_selectedTarget;
                }
            }
        }

        return SelectByMode(targets, damageType, from);
    }

    std::vector<SDK::AIHeroClient> GetTargets(float range,
                                              SDK::DamageType damageType = SDK::DamageType::Physical,
                                              const SDK::Vector3& from = SDK::Vector3()) {
        std::vector<SDK::AIHeroClient> targets;
        if (!m_menu) return targets;

        auto* only = m_menu->Get<SDK::MenuBool>("OnlySelectTarget");
        if (only && only->Enabled && m_selectedTarget.IsValid() && IsValidTarget(m_selectedTarget, range, from)) {
            return { m_selectedTarget };
        }

        targets = GetValidTargets(range, from);
        SortByMode(targets, damageType, from);

        // Bump selected target to front
        if (m_selectedTarget.IsValid() && !m_selectedTarget.IsDead()) {
            auto* force = m_menu->Get<SDK::MenuBool>("ForceSelectTarget");
            if (force && force->Enabled) {
                for (auto it = targets.begin(); it != targets.end(); ++it) {
                    if (it->NetworkId() == m_selectedTarget.NetworkId()) {
                        auto selected = *it;
                        targets.erase(it);
                        targets.insert(targets.begin(), selected);
                        break;
                    }
                }
            }
        }

        return targets;
    }

    SDK::AIHeroClient GetSelectedTarget() const { return m_selectedTarget; }

    void ClearSelectedTarget() {
        m_selectedTarget = SDK::AIHeroClient();
        SDK::TargetSelectorSelected::ClearForcedTarget();
        SDK::TargetSelectorSelected::ClearTarget();
    }

    int GetPriority(const SDK::AIHeroClient& target) {
        if (!target.IsValid()) return 0;
        std::string name = target.CharacterName();
        std::string key = "TS_" + name;
        if (m_priorityMenu) {
            if (auto* slider = m_priorityMenu->Get<SDK::MenuSlider>(key)) {
                return slider->Value;
            }
        }
        return GetDefaultPriority(name);
    }

private:
    static inline TargetSelectorPlugin* s_instance = nullptr;

    SDK::MenuUI::Menu* m_menu = nullptr;
    SDK::MenuUI::Menu* m_priorityMenu = nullptr;
    SDK::AIHeroClient m_selectedTarget;
    std::string m_modeKey = "TSMode";
    int m_sdkTSRegistryIndex = -1;
    bool m_sdkTSWasLoaded = true;

    static SDK::AIHeroClient SDKGetTargetOverride(float range,
                                                  SDK::DamageType damageType,
                                                  const SDK::Vector3& from) {
        return s_instance ? s_instance->GetTarget(range, damageType, from) : SDK::AIHeroClient();
    }

    static std::vector<SDK::AIHeroClient> SDKGetTargetsOverride(float range,
                                                                SDK::DamageType damageType,
                                                                const SDK::Vector3& from) {
        return s_instance ? s_instance->GetTargets(range, damageType, from) : std::vector<SDK::AIHeroClient>();
    }

    static SDK::AIHeroClient SDKGetSelectedTargetOverride() {
        return s_instance ? s_instance->GetSelectedTarget() : SDK::AIHeroClient();
    }

    static void SDKSetSelectedTargetOverride(const SDK::AIHeroClient& target) {
        if (!s_instance) {
            return;
        }

        if (!target.IsValid()) {
            s_instance->ClearSelectedTarget();
            return;
        }

        s_instance->m_selectedTarget = target;
        s_instance->SyncSelectionToSDK();
    }

    static void SDKClearSelectedTargetOverride() {
        if (s_instance) {
            s_instance->ClearSelectedTarget();
        }
    }

    static int SDKGetPriorityOverride(const SDK::AIHeroClient& target) {
        return s_instance ? s_instance->GetPriority(target) : 0;
    }

    // ──────────────────────────────────────────────────────────
    // SDK Override: Hide/Restore the built-in TargetSelector
    // ──────────────────────────────────────────────────────────

    void HideSDKTargetSelector() {
        m_sdkTSRegistryIndex = PluginRegistry::FindByInternalId("targetselector");
        if (m_sdkTSRegistryIndex >= 0) {
            m_sdkTSWasLoaded = PluginRegistry::Plugins[m_sdkTSRegistryIndex].Loaded;
            // Hide the SDK TS from the sidebar / menu
            PluginRegistry::Plugins[m_sdkTSRegistryIndex].Loaded = false;
        }
    }

    void RestoreSDKTargetSelector() {
        if (m_sdkTSRegistryIndex >= 0) {
            PluginRegistry::Plugins[m_sdkTSRegistryIndex].Loaded = m_sdkTSWasLoaded;
            m_sdkTSRegistryIndex = -1;
        }
    }

    // ──────────────────────────────────────────────────────────
    // Sync: Push our selection into SDK TargetSelectorSelected
    // so Orbwalker + champion scripts can use it transparently
    // ──────────────────────────────────────────────────────────

    void SyncSelectionToSDK() {
        if (m_selectedTarget.IsValid() && !m_selectedTarget.IsDead() && m_selectedTarget.IsVisible()) {
            SDK::TargetSelectorSelected::SetTarget(m_selectedTarget);

            auto* force = m_menu ? m_menu->Get<SDK::MenuBool>("ForceSelectTarget") : nullptr;
            if (force && force->Enabled) {
                SDK::TargetSelectorSelected::SetForcedTarget(m_selectedTarget);
            } else {
                SDK::TargetSelectorSelected::ClearForcedTarget();
            }
        } else {
            // No valid selection → let SDK pick freely
            SDK::TargetSelectorSelected::ClearTarget();
            SDK::TargetSelectorSelected::ClearForcedTarget();
        }

        // OnlySelectTarget → SDK should also respect this
        auto* only = m_menu ? m_menu->Get<SDK::MenuBool>("OnlySelectTarget") : nullptr;
        if (only && only->Enabled && m_selectedTarget.IsValid()) {
            SDK::TargetSelectorSelected::SetForcedTarget(m_selectedTarget);
        }
    }

    // ──────────────────────────────────────────────────────────
    // Priority Sliders — dynamically added per enemy hero
    // ──────────────────────────────────────────────────────────

    void EnsurePrioritySliders() {
        if (!m_priorityMenu) return;

        for (const auto& enemy : SDK::ObjectManager::EnemyHeroes()) {
            if (!enemy.IsValid()) continue;

            const std::string name = enemy.CharacterName();
            if (name.empty()) continue;

            const std::string key = "TS_" + name;
            if (!m_priorityMenu->Get<SDK::MenuSlider>(key)) {
                m_priorityMenu->Add<SDK::MenuSlider>(
                    key, name, GetDefaultPriority(name), 1, 5);
            }
        }
    }

    // ──────────────────────────────────────────────────────────
    // Click-based target selection (mirrors TargetSelectorSelected)
    // ──────────────────────────────────────────────────────────

    void HandleClickSelect() {
        static bool wasClickDown = false;
        bool clickDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        if (clickDown && !wasClickDown) {
            SDK::Vector3 clickPos = SDK::Game::CursorPos();
            if (!clickPos.IsZero()) {
                SDK::AIHeroClient nearest;
                float nearestDist = 300.0f;

                for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
                    if (!hero.IsValid() || hero.IsDead() || !hero.IsVisible()) continue;

                    float dist = hero.Distance(clickPos);
                    if (dist < nearestDist) {
                        nearestDist = dist;
                        nearest = hero;
                    }
                }

                if (nearest.IsValid()) {
                    m_selectedTarget = nearest;
                } else {
                    m_selectedTarget = SDK::AIHeroClient();
                }
            }
        }

        wasClickDown = clickDown;

        // Expire dead/invisible selection
        if (m_selectedTarget.IsValid()) {
            if (m_selectedTarget.IsDead() || !m_selectedTarget.IsVisible()) {
                m_selectedTarget = SDK::AIHeroClient();
            }
        }
    }

    // ──────────────────────────────────────────────────────────
    // Target Validation
    // ──────────────────────────────────────────────────────────

    bool IsValidTarget(const SDK::AIHeroClient& target,
                       float range,
                       const SDK::Vector3& from = SDK::Vector3()) {
        if (!target.IsValid() || target.IsDead() || !target.IsVisible() || !target.IsTargetable()) {
            return false;
        }

        if (Compat::IsZombieLike(target)) return false;

        // Invulnerability buff check
        static const char* invulnBuffs[] = {
            "KayleR", "TryndamereR", "kindaborroweytime",
            "ChronoShift", "UndyingRage", nullptr
        };
        for (const char** p = invulnBuffs; *p; ++p) {
            if (target.HasBuff(*p)) return false;
        }

        if (range > 0.0f) {
            const auto origin = from.IsZero() ? SDK::ObjectManager::Player().Position() : from;
            float dist = target.Distance(origin);
            if (dist > range + target.BoundingRadius()) return false;
        }

        return true;
    }

    std::vector<SDK::AIHeroClient> GetValidTargets(float range, const SDK::Vector3& from = SDK::Vector3()) {
        std::vector<SDK::AIHeroClient> result;
        auto player = SDK::ObjectManager::Player();
        const auto origin = from.IsZero() ? player.Position() : from;

        // Azir soldier extended range
        bool isAzir = false;
        if (player.IsValid()) {
            std::string champName = player.CharacterName();
            isAzir = (_stricmp(champName.c_str(), "Azir") == 0);
        }

        for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
            if (!IsValidTarget(hero, 0.0f, from)) continue;

            bool inRange = hero.Distance(origin) <= range + hero.BoundingRadius();

            // Azir soldier range compatibility
            if (!inRange && isAzir) {
                for (const auto& soldier : SDK::ObjectManager::AllyMinions()) {
                    if (!soldier.IsValid()) continue;
                    std::string soldierName = soldier.CharacterName();
                    if (soldierName.find("AzirSoldier") == std::string::npos) continue;
                    float soldierToPlayer = player.Distance(soldier);
                    float soldierToTarget = soldier.Distance(hero);
                    if (soldierToPlayer <= 770.0f && soldierToTarget <= 350.0f) {
                        inRange = true;
                        break;
                    }
                }
            }

            if (inRange) result.push_back(hero);
        }

        return result;
    }

    // ──────────────────────────────────────────────────────────
    // Smart Scoring (AD/AP aware) — from NewTargetSelector.cs
    // ──────────────────────────────────────────────────────────

    float GetSmartScore(const SDK::AIHeroClient& target,
                        SDK::DamageType damageType,
                        const SDK::Vector3& from = SDK::Vector3()) {
        auto player = SDK::ObjectManager::Player();
        const auto origin = from.IsZero() ? player.Position() : from;

        float ehp = SDK::TargetSelectorModes::ITargetSelectorMode::EffectiveHealth(target, damageType);
        if (ehp <= 0.0f) ehp = 1.0f;

        int prio = GetPriority(target);
        float prioWeight = 0.5f + static_cast<float>(prio) * 0.5f;
        float distPenalty = target.Distance(origin) / 1000.0f * 30.0f;

        float aaDmg = player.GetAutoAttackDamage(target, true);
        float killBonus = 0.0f;
        if (aaDmg > 0.0f) {
            float attacksToKill = target.Health() / aaDmg;
            if (attacksToKill <= 3.0f) {
                killBonus = -200.0f * (3.0f - attacksToKill);
            }
        }

        return (ehp / prioWeight) + distPenalty + killBonus;
    }

    // ──────────────────────────────────────────────────────────
    // Mode-based selection & sorting
    // ──────────────────────────────────────────────────────────

    SDK::AIHeroClient SelectByMode(std::vector<SDK::AIHeroClient>& targets,
                                   SDK::DamageType damageType,
                                   const SDK::Vector3& from = SDK::Vector3()) {
        if (targets.empty()) return SDK::AIHeroClient();

        int mode = 0;
        if (m_menu) {
            if (auto* modeList = m_menu->Get<SDK::MenuList>(m_modeKey)) {
                mode = modeList->Index;
            }
        }
        mode = std::clamp(mode, 0, 2);

        switch (mode) {
        case 0: // Smart AD/AP
            return *std::min_element(targets.begin(), targets.end(),
                [this, damageType, &from](const SDK::AIHeroClient& a, const SDK::AIHeroClient& b) {
                    return GetSmartScore(a, damageType, from) < GetSmartScore(b, damageType, from);
                });
        case 1: // Lowest health
            return *std::min_element(targets.begin(), targets.end(),
                [](const SDK::AIHeroClient& a, const SDK::AIHeroClient& b) {
                    return a.Health() < b.Health();
                });
        case 2: // Most priority
            return *std::min_element(targets.begin(), targets.end(),
                [this](const SDK::AIHeroClient& a, const SDK::AIHeroClient& b) {
                    return GetPriority(a) > GetPriority(b);
                });
        default:
            return targets.front();
        }
    }

    void SortByMode(std::vector<SDK::AIHeroClient>& targets,
                    SDK::DamageType damageType,
                    const SDK::Vector3& from = SDK::Vector3()) {
        int mode = 0;
        if (m_menu) {
            if (auto* modeList = m_menu->Get<SDK::MenuList>(m_modeKey)) {
                mode = modeList->Index;
            }
        }
        mode = std::clamp(mode, 0, 2);

        switch (mode) {
        case 0:
            std::sort(targets.begin(), targets.end(),
                [this, damageType, &from](const SDK::AIHeroClient& a, const SDK::AIHeroClient& b) {
                    return GetSmartScore(a, damageType, from) < GetSmartScore(b, damageType, from);
                });
            break;
        case 1:
            std::sort(targets.begin(), targets.end(),
                [](const SDK::AIHeroClient& a, const SDK::AIHeroClient& b) {
                    return a.Health() < b.Health();
                });
            break;
        case 2:
            std::sort(targets.begin(), targets.end(),
                [this](const SDK::AIHeroClient& a, const SDK::AIHeroClient& b) {
                    return GetPriority(a) > GetPriority(b);
                });
            break;
        default:
            break;
        }
    }

    // ──────────────────────────────────────────────────────────
    // Default Priority Table — Same as NewTargetSelector.cs
    // ──────────────────────────────────────────────────────────

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

        for (const char** p = maxPrio; *p; ++p)
            if (_stricmp(name.c_str(), *p) == 0) return 5;
        for (const char** p = highPrio; *p; ++p)
            if (_stricmp(name.c_str(), *p) == 0) return 4;
        for (const char** p = medPrio; *p; ++p)
            if (_stricmp(name.c_str(), *p) == 0) return 3;
        for (const char** p = lowPrio; *p; ++p)
            if (_stricmp(name.c_str(), *p) == 0) return 2;

        return 1;
    }
};

} // namespace Plugins
