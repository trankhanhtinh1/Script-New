#pragma once
// Port of OKTW_CSharp/Champions/Tristana.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class TristanaPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Tristana"; }
    const char* GetInternalId() const override { return "champion.oktw.tristana"; }
    const char* GetChampionName() const override { return "Tristana"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q);
        m_W = Spell(SpellSlot::W, 900.0f);
        m_E = Spell(SpellSlot::E, 620.0f);
        m_R = Spell(SpellSlot::R, 620.0f);

        m_W.SetSkillshot(0.35f, 250.0f, 1400.0f, false, SDK::SpellType::Circle);

        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("eInfo",   "E info", true));

        m_qMenu->Add(new MenuBool("harassQ", "Harass Q", true));

        m_wMenu->Add(new MenuBool("nktdE",   "NoKeyToDash", true));
        m_wMenu->Add(new MenuBool("Wks",     "W KS logic (W+E+R calculation)", true));
        m_wMenu->Add(new MenuKeyBind("smartW", "SmartCast W key", 'T', SDK::UI::KeyBindType::Press));

        m_eMenu->Add(new MenuBool("harassE", "Harass E", true));
        m_eMenu->Add(new MenuBool("Eturet",  "E on turrent laneclear", true));
        m_eMenu->Add(new MenuBool("focusE",  "Focus target with E", true));

        Menu* eon = m_eMenu->AddSubMenu(new Menu("EonSub", "Use E on"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("useEon") + enemy.CharacterName();
            eon->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_rMenu->Add(new MenuBool("autoR",                "Auto R KS (E+R calculation)", true));
        m_rMenu->Add(new MenuBool("turrentR",             "Try R under turrent", true));
        m_rMenu->Add(new MenuBool("allyR",                "Try R under ally", true));
        m_rMenu->Add(new MenuBool("OnInterruptableSpell", "OnInterruptableSpell", true));
        m_rMenu->Add(new MenuKeyBind("useR", "OneKeyToCast R closest person", 'T', SDK::UI::KeyBindType::Press));

        Menu* gap = m_rMenu->AddSubMenu(new Menu("GapSub", "GapCloser & anti-meele"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("GapCloser") + enemy.CharacterName();
            gap->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }
        gap->Add(new MenuSlider("RgapHP", " use gapcloser only under % hp", 40, 0, 100));

        m_farmMenu->Add(new MenuBool("farmQ",  "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("jungle", "Jungle Farm", true));

        // TODO(oktw-port): BeforeAttack event not available (E on auto-attack targets)
        // TODO(oktw-port): AfterAttack event not available (Q reset after AA)
        // TODO(oktw-port): Interrupter2 event not available (R on interruptable)
        // TODO(oktw-port): AntiGapcloser event not available (R on melee gapcloser)
        // TODO(oktw-port): Orbwalker.ForceTarget not available (focusE target focus)
    }

    void SetMana() override {
        if ((Shared().manaDisable && Shared().manaDisable->Value && Combo()) ||
            Player().HealthPercent() < 20.0f) {
            m_QMANA = m_WMANA = m_EMANA = m_RMANA = 0.0f;
            return;
        }
        m_QMANA = m_Q.Instance().ManaCost();
        m_WMANA = m_W.Instance().ManaCost();
        m_EMANA = m_E.Instance().ManaCost();
        m_RMANA = m_R.IsReady() ? m_R.Instance().ManaCost() : m_EMANA;
    }

    float GetEDmg(const AIHeroClient& target) {
        if (!target.HasBuff("tristanaechargesound")) return 0.0f;
        float dmg = m_E.GetDamage(target);
        // Buff-stack multiplier deferred: assume base charge damage
        return dmg;
    }

    void OnGameUpdate() override {
        const auto p = Player();
        if (!p.IsValid()) return;

        if (m_W.IsReady() && GetKey("smartW")) {
            m_W.Cast(SDK::Game::CursorPos());
        }

        if (LagFree(1)) {
            const float lvl = 7.0f * (p.Level() - 1);
            m_E.Range = 620.0f + lvl;
            m_R.Range = 620.0f + lvl;
            SetMana();
            Jungle();
        }

        if ((LagFree(4) || LagFree(2)) && m_R.IsReady()) LogicR();
        if (LagFree(3) && m_W.IsReady()) LogicW();
    }

    void LogicW() {
        const auto p = Player();
        if (!GetBool("Wks") || p.Mana() <= m_RMANA + m_WMANA) return;

        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(enemy, m_W.Range, true)) continue;
            if (!OktwCommon::ValidUlt(enemy)) continue;
            if (OktwCommon::CountEnemiesInRange(enemy.Position(), 800.0f) >= 2) continue;
            if (OktwCommon::CountAlliesInRange(enemy.Position(), 400.0f) != 0) continue;
            if (enemy.Health() <= enemy.Level() * 2.0f) continue;

            const float playerAaDmg = p.GetAutoAttackDamage(enemy, false);
            const float dmgCombo = playerAaDmg + OktwCommon::GetKsDamage(enemy, m_W) + GetEDmg(enemy);

            if (dmgCombo > enemy.Health()) {
                CastSpell(m_W, enemy);
            } else if (m_R.IsReady() && m_R.GetDamage(enemy) + dmgCombo > enemy.Health() &&
                       p.Mana() > m_RMANA + m_WMANA) {
                CastSpell(m_W, enemy);
            }
        }
    }

    void LogicR() {
        const auto p = Player();
        AIHeroClient bestEnemy;
        bool haveBest = false;
        const float pushDistance = 400.0f + (m_R.Level() * 200.0f);
        (void)pushDistance;

        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(enemy, m_R.Range, true)) continue;
            if (!OktwCommon::ValidUlt(enemy)) continue;

            if (!haveBest) { bestEnemy = enemy; haveBest = true; }
            else if (p.Position().Distance(enemy.Position()) < p.Position().Distance(bestEnemy.Position())) {
                bestEnemy = enemy;
            }

            if (GetBool("autoR") &&
                OktwCommon::GetKsDamage(enemy, m_R) + GetEDmg(enemy) > enemy.Health() &&
                GetEDmg(enemy) < enemy.Health()) {
                m_R.Cast(enemy);
            }

            // turrentR / allyR turret-detection deferred (UnderTurret not available)
            if (GetBool("allyR") && OktwCommon::CountAlliesInRange(enemy.Position(), 350.0f) == 0) {
                if (OktwCommon::CountAlliesInRange(enemy.Position(), 500.0f) > 1) {
                    m_R.Cast(enemy);
                }
            }

            const float rgapHp = static_cast<float>(GetSlider("RgapHP", 40));
            if (p.HealthPercent() < rgapHp &&
                SDK::Extensions::IsValidTarget(enemy, 270.0f, true) &&
                enemy.IsMelee() &&
                GetBool((std::string("GapCloser") + enemy.CharacterName()).c_str())) {
                m_R.Cast(enemy);
            }
        }

        if (GetKey("useR") && haveBest) {
            m_R.Cast(bestEnemy);
        }
    }

    void Jungle() {
        if (!GetBool("jungle") || !LaneClear()) return;
        const auto p = Player();
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), m_E.Range, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_RMANA) {
            m_E.Cast(mob);
        }
        if (m_Q.IsReady()) m_Q.Cast();
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
