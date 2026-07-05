#pragma once
// ============================================================================
// OKTWBase.h — Port of OneKeyToWin_AIO_Sebby Base.cs + Program.cs
//
// Consolidates the C# Program/Base static state that every champion in the
// OKTW AIO relies on. Provides:
//   - Shared prediction MODE menu (per-slot: Q/W/E/R)
//   - CastSpell(spell, target) dispatcher matching Program.CastSpell in C#
//   - Program state (Combo/Harass/LaneClear/None/Farm/LagFree)
//   - Mana bookkeeping (QMANA/WMANA/EMANA/RMANA) baked into champion plugins
//   - Global "Extra settings OKTW" menu (registered once by first plugin load)
//   - Spell-farm toggle
//
// Every champion plugin (Champions/*.h) inherits from OKTWBase and uses these
// helpers to keep parity with the C# logic 1:1.
// ============================================================================

#include "../../plugins/IPlugin.h"
#include "../../SDK/SDK.h"
#include "../../SDK/UI/IMenu/Menu.h"
#include "../../DebugLog.h"
#include "OktwCommon.h"

#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Plugins { namespace OKTW {

using SDK::Spell;
using SDK::HitChance;
using SDK::SkillshotType;
using SDK::SpellSlot;
using SDK::DamageType;
using SDK::AIHeroClient;
using SDK::AIBaseClient;
using SDK::AIMinionClient;

// ── Program mode mirror (Orbwalker::ActiveMode → Combo/Harass/... booleans) ──
struct ProgramState {
    bool Combo = false;
    bool Harass = false;
    bool LaneClear = false;
    bool Farm = false;
    bool None = true;
    int  TickIndex = 0;

    void Refresh() {
        const auto mode = SDK::Orbwalker::ActiveMode();
        Combo     = (mode == SDK::OrbwalkingMode::Combo);
        LaneClear = (mode == SDK::OrbwalkingMode::LaneClear);
        Harass    = LaneClear
                  || (mode == SDK::OrbwalkingMode::Harass)
                  || (mode == SDK::OrbwalkingMode::LastHit);
        Farm      = Harass;
        None      = (mode == SDK::OrbwalkingMode::None);
        if (++TickIndex > 4) TickIndex = 0;
    }

    bool LagFree(int offset) const { return TickIndex == offset; }
};

inline ProgramState& State() {
    static ProgramState s;
    return s;
}

// ── Shared "Extra settings OKTW©" root submenu (once per session) ──
struct SharedMenus {
    Menu* extraSettings = nullptr;     // "Extra settings OKTW©"
    MenuBool* supportMode = nullptr;
    MenuBool* comboDisableMode = nullptr;
    MenuBool* manaDisable = nullptr;
    MenuBool* collAA = nullptr;
    MenuBool* harassMixed = nullptr;

    Menu* predMode = nullptr;          // "Prediction MODE"
    MenuList* qPred = nullptr;
    MenuList* wPred = nullptr;
    MenuList* ePred = nullptr;
    MenuList* rPred = nullptr;
    MenuList* qHit = nullptr;
    MenuList* wHit = nullptr;
    MenuList* eHit = nullptr;
    MenuList* rHit = nullptr;
    MenuBool* debugPred = nullptr;

    bool built = false;
};

inline SharedMenus& Shared() { static SharedMenus s; return s; }

inline void EnsureSharedMenus(Menu* root) {
    auto& sm = Shared();
    if (sm.built || !root) return;
    sm.built = true;

    // "Extra settings OKTW©"
    sm.extraSettings = root->AddSubMenu(new Menu("extraOKTW", "Extra settings OKTW"));
    sm.supportMode      = sm.extraSettings->Add(new MenuBool("supportMode", "Support Mode", false));
    sm.comboDisableMode = sm.extraSettings->Add(new MenuBool("comboDisableMode", "Disable AA in combo mode", false));
    sm.manaDisable      = sm.extraSettings->Add(new MenuBool("manaDisable", "Disable mana manager in combo", false));
    sm.collAA           = sm.extraSettings->Add(new MenuBool("collAA", "Disable AA if Yasuo wall collision", true));
    sm.harassMixed      = sm.extraSettings->Add(new MenuBool("harassMixed", "Spell-harass only in mixed mode", false));

    // "Prediction MODE"
    sm.predMode = root->AddSubMenu(new Menu("predMode", "Prediction MODE"));
    static const char* predModes[] = { "Common", "OKTW", "SPrediction", "SDK", "Exory" };
    static const char* hitModes[]  = { "Very High", "High", "Medium" };
    sm.qPred = sm.predMode->Add(new MenuList("Qpred", "Q Prediction MODE", predModes, 5, 3));
    sm.qHit  = sm.predMode->Add(new MenuList("QHitChance", "Q Hit Chance", hitModes, 3, 0));
    sm.wPred = sm.predMode->Add(new MenuList("Wpred", "W Prediction MODE", predModes, 5, 3));
    sm.wHit  = sm.predMode->Add(new MenuList("WHitChance", "W Hit Chance", hitModes, 3, 0));
    sm.ePred = sm.predMode->Add(new MenuList("Epred", "E Prediction MODE", predModes, 5, 3));
    sm.eHit  = sm.predMode->Add(new MenuList("EHitChance", "E Hit Chance", hitModes, 3, 0));
    sm.rPred = sm.predMode->Add(new MenuList("Rpred", "R Prediction MODE", predModes, 5, 3));
    sm.rHit  = sm.predMode->Add(new MenuList("RHitChance", "R Hit Chance", hitModes, 3, 0));
    sm.debugPred = sm.predMode->Add(new MenuBool("debugPred", "Draw prediction line", false));
}

inline HitChance HitFromIndex(int idx) {
    switch (idx) {
        case 0: return HitChance::VeryHigh;
        case 1: return HitChance::High;
        case 2: return HitChance::Medium;
        default: return HitChance::High;
    }
}

// ── Program.CastSpell dispatcher (port from Program.cs:CastSpell) ──
// Simplified: routes all modes to SDK::Prediction. C#'s per-mode branch is
// collapsed since only SDK prediction is available. Collision, hit-chance
// filtering, and AoE checks are preserved.
inline bool CastSpell(Spell& spell, const AIBaseClient& target) {
    if (!target.IsValid() || !spell.IsReady()) return false;

    HitChance hitchance = HitChance::High;
    switch (spell.Slot) {
        case SpellSlot::Q: hitchance = HitFromIndex(Shared().qHit ? Shared().qHit->Index : 1); break;
        case SpellSlot::W: hitchance = HitFromIndex(Shared().wHit ? Shared().wHit->Index : 1); break;
        case SpellSlot::E: hitchance = HitFromIndex(Shared().eHit ? Shared().eHit->Index : 1); break;
        case SpellSlot::R: hitchance = HitFromIndex(Shared().rHit ? Shared().rHit->Index : 1); break;
        default: break;
    }

    const bool aoe = spell.Width > 80.0f && !spell.Collision;
    const auto pred = spell.GetPrediction(target, aoe);

    // Yasuo wall collision on non-instant projectiles
    if (spell.Speed != FLT_MAX &&
        OktwCommon::CollisionYasuo(SDK::ObjectManager::Player().ServerPosition(), pred.GetCastPosition())) {
        return false;
    }

    if (static_cast<int>(pred.Hitchance) >= static_cast<int>(hitchance)) {
        return spell.Cast(pred.GetCastPosition());
    }
    if (aoe && pred.AoeTargetsHitCount > 1 &&
        static_cast<int>(pred.Hitchance) >= static_cast<int>(HitChance::High)) {
        return spell.Cast(pred.GetCastPosition());
    }
    return false;
}

// ── Base class every OKTW champion plugin inherits from ──
class OKTWBase : public IPlugin {
public:
    PluginCategory GetCategory() const override { return PluginCategory::Champion; }
    bool AutoLoadByDefault() const override { return false; }

    bool CanLoad() const override {
        const char* champName = GetChampionName();
        if (!champName || !champName[0]) return false;
        const auto& cached = SDK::GameObject::GetCachedChampionName();
        if (!cached.empty()) return _stricmp(cached.c_str(), champName) == 0;
        return false;
    }

protected:
    // Shared per-plugin state ---
    Menu* m_menu = nullptr;
    Menu* m_champMenu = nullptr;        // Config.SubMenu(Player.ChampionName)
    Menu* m_drawMenu = nullptr;
    Menu* m_qMenu = nullptr;
    Menu* m_wMenu = nullptr;
    Menu* m_eMenu = nullptr;
    Menu* m_rMenu = nullptr;
    Menu* m_harassMenu = nullptr;
    Menu* m_farmMenu = nullptr;

    Spell m_Q{ SpellSlot::Q };
    Spell m_W{ SpellSlot::W };
    Spell m_E{ SpellSlot::E };
    Spell m_R{ SpellSlot::R };

    float m_QMANA = 0, m_WMANA = 0, m_EMANA = 0, m_RMANA = 0;

    MenuBool* m_spellFarm = nullptr;
    MenuSlider* m_lcMinions = nullptr;
    MenuSlider* m_manaSlider = nullptr;

    // Subclass hooks (like C# constructors + Game_OnUpdate/OnDraw) ---
    virtual void BuildMenu() = 0;
    virtual void OnGameUpdate() {}
    virtual void OnGameDraw() {}
    virtual void SetMana() {}

    // Menu wiring ---
    void EnsureRootMenu() {
        const std::string playerName = std::string("OneKeyToWin_AIO_") + GetChampionName();
        m_menu = new Menu(playerName.c_str(), "OneKeyToWin AIO", true);
        EnsureSharedMenus(m_menu);

        m_champMenu = m_menu->AddSubMenu(new Menu(GetChampionName(), GetChampionName()));
        m_drawMenu   = m_champMenu->AddSubMenu(new Menu("Draw",     "Draw"));
        m_qMenu      = m_champMenu->AddSubMenu(new Menu("QConfig",  "Q Config"));
        m_wMenu      = m_champMenu->AddSubMenu(new Menu("WConfig",  "W Config"));
        m_eMenu      = m_champMenu->AddSubMenu(new Menu("EConfig",  "E Config"));
        m_rMenu      = m_champMenu->AddSubMenu(new Menu("RConfig",  "R Config"));
        m_harassMenu = m_champMenu->AddSubMenu(new Menu("Harass",   "Harass"));
        m_farmMenu   = m_champMenu->AddSubMenu(new Menu("Farm",     "Farm"));

        // Shared farm menu items
        m_spellFarm = m_farmMenu->Add(new MenuBool("spellFarm", "OKTW spells farm", true));
        m_lcMinions = m_farmMenu->Add(new MenuSlider("LCminions", "LaneClear min minions", 2, 0, 10));
        m_manaSlider = m_farmMenu->Add(new MenuSlider("Mana", "LaneClear Mana%", 50, 0, 100));

        // Populate per-enemy "Harass X" toggles (matches C# Base.cs static ctor)
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id   = std::string("Harass") + enemy.CharacterName();
            const std::string name = enemy.CharacterName();
            m_harassMenu->Add(new MenuBool(id.c_str(), name.c_str(), true));
        }
    }

    // Helper: query menu items by name (mirrors Config.Item("id").GetValue<T>() in C#)
    bool CfgBool(Menu* m, const char* id, bool fallback = false) const {
        if (!m) return fallback;
        auto* item = dynamic_cast<MenuBool*>(m->Item(id));
        return item ? item->Value : fallback;
    }
    int CfgSlider(Menu* m, const char* id, int fallback = 0) const {
        if (!m) return fallback;
        auto* item = dynamic_cast<MenuSlider*>(m->Item(id));
        return item ? item->Value : fallback;
    }
    int CfgList(Menu* m, const char* id, int fallback = 0) const {
        if (!m) return fallback;
        auto* item = dynamic_cast<MenuList*>(m->Item(id));
        return item ? item->Index : fallback;
    }
    bool CfgKey(Menu* m, const char* id) const {
        if (!m) return false;
        auto* item = dynamic_cast<MenuKeyBind*>(m->Item(id));
        return item && item->Active;
    }

    // Recursive item finder (C# Config.Item searches whole tree)
    MenuItem* FindItem(Menu* m, const char* id) const {
        if (!m || !id) return nullptr;
        if (auto* it = m->Item(id)) return it;
        for (int i = 0; i < m->Components.size(); ++i) {
            if (auto* sub = dynamic_cast<Menu*>(m->Components[i])) {
                if (auto* it = FindItem(sub, id)) return it;
            }
        }
        return nullptr;
    }
    bool GetBool(const char* id, bool fallback = false) const {
        auto* it = FindItem(m_menu, id);
        auto* b = dynamic_cast<MenuBool*>(it);
        return b ? b->Value : fallback;
    }
    int GetSlider(const char* id, int fallback = 0) const {
        auto* it = FindItem(m_menu, id);
        auto* s = dynamic_cast<MenuSlider*>(it);
        return s ? s->Value : fallback;
    }
    int GetList(const char* id, int fallback = 0) const {
        auto* it = FindItem(m_menu, id);
        auto* l = dynamic_cast<MenuList*>(it);
        return l ? l->Index : fallback;
    }
    bool GetKey(const char* id) const {
        auto* it = FindItem(m_menu, id);
        auto* k = dynamic_cast<MenuKeyBind*>(it);
        return k && k->Active;
    }

    // Property mirrors of C# static Program ---
    bool Combo()     const { return State().Combo; }
    bool Harass()    const { return State().Harass; }
    bool LaneClear() const { return State().LaneClear; }
    bool Farm()      const { return State().Farm; }
    bool None()      const { return State().None; }
    bool LagFree(int off) const { return State().LagFree(off); }

    AIHeroClient Player() const { return SDK::ObjectManager::Player(); }

    // Mirrors Base.FarmSpells
    bool FarmSpells() const {
        if (!m_spellFarm || !m_spellFarm->Value) return false;
        if (SDK::Orbwalker::ActiveMode() != SDK::OrbwalkingMode::LaneClear) return false;
        const auto p = Player();
        if (!p.IsValid()) return false;
        const float manaPct = p.MaxMana() > 0.0f ? (p.Mana() * 100.0f / p.MaxMana()) : 0.0f;
        return manaPct > static_cast<float>(m_manaSlider ? m_manaSlider->Value : 50);
    }
    int FarmMinions() const { return m_lcMinions ? m_lcMinions->Value : 2; }

    // Lifecycle ----
public:
    void OnLoad() override {
        NightSharpDebug::Logf("[OKTW/%s] OnLoad", GetChampionName());
        EnsureRootMenu();
        BuildMenu();
        m_menu->Attach();

        SDK::Events::hook.OnGameUpdate += &OKTWBase::StaticOnUpdate;
    }

    void OnUnload() override {
        SDK::Events::hook.OnGameUpdate -= &OKTWBase::StaticOnUpdate;
        if (m_menu) {
            MenuManager::Instance().Remove(m_menu);
            delete m_menu;
            m_menu = nullptr;
        }
        NightSharpDebug::Logf("[OKTW/%s] OnUnload", GetChampionName());
    }

    void OnRender() override { OnGameDraw(); }

    void OnMenu() override { if (m_menu) m_menu->DrawImGui(); }

private:
    static inline OKTWBase* s_active = nullptr;

    static void StaticOnUpdate(const SDK::Events::GameUpdateEventArgs&) {
        State().Refresh();
        // Dispatch to all loaded OKTW champion plugins (there is only one at a time)
        if (s_active) s_active->OnGameUpdate();
    }

protected:
    void MarkActive() { s_active = this; }
};

} } // namespace Plugins::OKTW
