#pragma once

#include "../../IPlugin.h"
#include "menu/MenuUI.h"
#include "sdk/Core/Game.h"
#include "sdk/SDK.h"
#include "sdk/Utils/Jungle.h"
#include "sdk/Wrappers/Damages/Damage.h"
#include "sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "sdk/Wrappers/Spells/Spell.h"
#include "sdk/Utils/Minion.h"
#include "sdk/Wrappers/TargetSelector/TargetSelector.h"

namespace Plugins {

using namespace SDK;
using namespace SDK::MenuUI;

class RTXPowerPlugin : public IPlugin {
public:
    const char *GetName()       const override { return "RTX Power"; }
    const char *GetInternalId() const override { return "champion_rtxpower_ezreal"; }
    const char *GetAuthor()     const override { return "RTX Power"; }
    PluginCategory GetCategory() const override { return PluginCategory::Champion; }
    bool AutoLoadByDefault()    const override { return false; }

    bool CanLoad() const override {
        return Player().IsValid() && Player().CharacterName() == "Ezreal";
    }

    Spell Q, W, E, R;

    void OnLoad() override {
        if (m_menu) return;

        Q = Spell(SpellSlot::Q, 1200.0f);
        Q.SetSkillshot(0.25f, 60.0f, 2000.0f, true, SpellType::Line);

        W = Spell(SpellSlot::W, 1200.0f);
        W.SetSkillshot(0.25f, 80.0f, 1700.0f, false, SpellType::Line);

        E = Spell(SpellSlot::E, 475.0f);
        E.Delay = 0.25f;

        R = Spell(SpellSlot::R, 2000.0f);
        R.SetSkillshot(1.0f, 160.0f, 2000.0f, false, SpellType::Line);

        m_menu = Menu::Create("RTXPowerRoot", "[RTX Power] Ezreal");

        auto *combo = m_menu->AddSubMenu("combo", "Combo");
        combo->Add<MenuBool>("q", "Use Q", true);
        combo->Add<MenuBool>("w", "Use W", true);

        auto *harass = m_menu->AddSubMenu("harass", "Harass");
        harass->Add<MenuBool>("q", "Use Q", true);
        harass->Add<MenuBool>("w", "Use W", true);

        auto *automatic = m_menu->AddSubMenu("auto", "Automatic");
        automatic->Add<MenuBool>("qKs", "Q Killsteal", true);
        automatic->Add<MenuBool>("eAntiGap", "E Anti Gapcloser", true);
        automatic->Add<MenuBool>("rKs", "R Killsteal", true);
        automatic->Add<MenuSlider>("rKsEnemy", "R KS |Don't use if x range have enemy", 600, 600, 1000);
        automatic->Add<MenuSlider>("rKsMax", "R KS |Range", 2000, 600, 2000);
        automatic->Add<MenuSlider>("rCc", "R CC Enemy (ms)", 990, 0, 1500);
        automatic->Add<MenuSlider>("rCcRange", "R CC |>= x Range", 600, 600, 2000);

        auto *farm = m_menu->AddSubMenu("farm", "Farm");
        farm->Add<MenuBool>("q", "Use Q", true);
        farm->Add<MenuBool>("qLh", "Q Last Hit", true);
        farm->Add<MenuBool>("qJg", "Jungle Q", true);
        farm->Add<MenuBool>("wJg", "Jungle W", true);

        auto *lh = m_menu->AddSubMenu("lh", "Last Hit");
        lh->Add<MenuBool>("q", "Last Hit Q", true);
        lh->Add<MenuBool>("qSiege", "Only Siege", false);

        auto *flee = m_menu->AddSubMenu("flee", "Flee");
        flee->Add<MenuBool>("e", "Use E", true);

        auto *hc = m_menu->AddSubMenu("hc", "Hitchance");
        hc->Add<MenuSlider>("rRange", "R Range", 2000, 1500, 3000);
        hc->Add<MenuSlider>("semiRRange", "Semi R Range", 2000, 1500, 3000);
        hc->Add<MenuKeyBind>("semiR", "Semi R", 'T', KeyBindType::Press);
    }

    void OnUnload() override {
        if (!m_menu) return;
        Menu::Remove("RTXPowerRoot");
        m_menu = nullptr;
    }

    Menu *GetMenuRoot() override { return m_menu; }

    void OnAfterAttack(OrbwalkingActionArgs& args) override {
        if (!m_menu || !args.Target.IsValid() || args.Target.IsDead()) return;

    }

    void OnGapcloser(const AIHeroClient& sender, const AntiGapcloser::GapcloserArgs& args) override {
        
    }

    void QLogic() {
        if (!Q.IsReady()) return;
        if (Player().IsWindingUp()) return;

        const auto mode = Orbwalker::GetMode();
        const bool isCombo = mode == OrbwalkerMode::Combo;
        const bool isHarass = mode == OrbwalkerMode::Harass;
        const bool isFarm = mode == OrbwalkerMode::Clear;

        if (!isCombo && !isHarass && !isFarm) return;

        if (isCombo) {
            auto *combo = m_menu->GetSubMenu("combo");
            if (!combo || !combo->GetBoolValue("q", true)) return;
        } else if (isHarass) {
            auto *harass = m_menu->GetSubMenu("harass");
            if (!harass || !harass->GetBoolValue("q", true)) return;
        } else if (isFarm) {
            if (Player().IsUnderEnemyTurret()) return;
            auto *farm = m_menu->GetSubMenu("farm");
            if (!farm || !farm->GetBoolValue("q", true)) return;
        }

        auto target = TargetSelector::GetTarget(Q.Range, DamageType::Physical);
        if (!target.IsValid() || !target.IsValidTarget(Q.Range)) return;

        if ((isCombo || isHarass) && Player().InAutoAttackRange(target) && Orbwalker::TimeUntilNextAttack() == 0.0f) return;

        Q.CastPredicted(target, HitChance::High);
    }
    void WLogic() {

    }
    void ELogic() {

    }
    void RLogic() {

    }

    void OnUpdate() override {
        if (!Player().IsValid() || Player().IsDead() || !m_menu) return;

        QLogic();
        WLogic();
        ELogic();
        RLogic();

    }

private:
    Menu *m_menu = nullptr;
};
}
