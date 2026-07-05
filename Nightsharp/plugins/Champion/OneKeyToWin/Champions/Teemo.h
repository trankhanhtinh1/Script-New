#pragma once
// Port of OKTW_CSharp/Champions/Teemo.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;

class TeemoPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Teemo"; }
    const char* GetInternalId() const override { return "champion.oktw.teemo"; }
    const char* GetChampionName() const override { return "Teemo"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 680.0f);
        m_W = Spell(SpellSlot::W);
        m_E = Spell(SpellSlot::E);
        m_R = Spell(SpellSlot::R, 400.0f);

        m_Q.SetTargetted(0.5f, 1500.0f);
        m_R.SetSkillshot(1.7f, 130.0f, 1000.0f, false, SDK::SpellType::Circle);

        m_drawMenu->Add(new MenuBool("noti",    "Show notification & line", true));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells",   true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));

        m_qMenu->Add(new MenuBool("autoQ",    "Auto Q", true));
        m_qMenu->Add(new MenuBool("Qgap",     "Auto Q Gapcloser", true));
        m_qMenu->Add(new MenuBool("QafterAA", "Q after AA only", true));

        Menu* qon = m_qMenu->AddSubMenu(new Menu("QonSub", "Q on"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("qUseOn") + enemy.CharacterName();
            qon->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_wMenu->Add(new MenuBool("autoWout",  "Auto W if target outrange", true));
        m_wMenu->Add(new MenuBool("autoWnear", "Auto W if enemy near", true));

        m_rMenu->Add(new MenuBool("autoR",     "Auto R", true));
        m_rMenu->Add(new MenuBool("comboR",    "Run", true));
        m_rMenu->Add(new MenuBool("Raoe",      "AOE", true));
        m_rMenu->Add(new MenuBool("Rgap",      "Gapcloser", true));
        m_rMenu->Add(new MenuBool("autoRslow", "On slow", true));
        m_rMenu->Add(new MenuBool("autoRcc",   "On CC", true));
        m_rMenu->Add(new MenuBool("autoRdash", "On dash", true));
        m_rMenu->Add(new MenuBool("telR",      "On zhonya, teleport, spells", true));
        m_rMenu->Add(new MenuBool("bushR2",    "Bush above 1 ammo", true));
        m_rMenu->Add(new MenuBool("bushR",     "Auto W bush after enemy enter", true));

        // TODO(oktw-port): AfterAttack event not available (Q on-hit after AA logic)
        // TODO(oktw-port): AntiGapcloser event not available (Q/R gapcloser reaction)
        // TODO(oktw-port): OnProcessSpellCast event not available (R on enemy spells)
        // TODO(oktw-port): Spellbook.OnCastSpell event not available (R trap-block guard)
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
        m_RMANA = m_R.IsReady() ? m_R.Instance().ManaCost() : m_QMANA;
    }

    void OnGameUpdate() override {
        const auto p = Player();
        if (!p.IsValid()) return;

        if (LagFree(0)) {
            SetMana();
            // R range scales with level: 150 + 250 * R.Level (approximation retained)
        }

        if (m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(4) && m_W.IsReady()) LogicW();
        if (m_R.IsReady() && GetBool("autoR")) LogicR();
    }

    void LogicW() {
        const auto p = Player();
        if (p.Mana() < m_RMANA + m_WMANA) return;

        if (GetBool("autoWnear")) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, 350.0f, true) && enemy.IsMoving()) {
                    m_W.Cast();
                    break;
                }
            }
        }

        if (Combo() && GetBool("autoWout")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(800.0f, DamageType::Magical) : AIHeroClient();
            if (t.IsValid()) m_W.Cast();
        }
    }

    void LogicR() {
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_QMANA) return;
        if (!p.IsMoving() && None()) return;

        if (LagFree(1)) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, m_R.Range + 100.0f, true)) continue;

                if (GetBool("autoRcc") && !OktwCommon::CanMove(enemy)) {
                    m_R.Cast(enemy);
                }
                if (GetBool("autoRslow") && !OktwCommon::CanMove(enemy)) {
                    CastSpell(m_R, enemy);
                }
                if (GetBool("Raoe")) {
                    auto pout = m_R.GetPrediction(enemy, true);
                    if (pout.AoeTargetsHitCount >= 2) m_R.Cast(pout.GetCastPosition());
                }
                if (GetBool("comboR")) {
                    auto pout = m_R.GetPrediction(enemy, true);
                    if (pout.GetCastPosition().Distance(enemy.Position()) > 300.0f) {
                        m_R.Cast(pout.GetCastPosition());
                    }
                }
            }
        }
    }

    void LogicQ() {
        const auto p = Player();
        const bool hpPred = (p.Health() - OktwCommon::GetIncomingDamage(p)) > p.MaxHealth() * 0.3f;

        for (const auto& t : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!t.IsValid() || !t.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(t, m_Q.Range, true)) continue;

            if (OktwCommon::GetKsDamage(t, m_Q) + p.GetAutoAttackDamage(t, false) > t.Health()) {
                m_Q.Cast(t);
            }

            const std::string useOnId = std::string("qUseOn") + t.CharacterName();
            if (!GetBool(useOnId.c_str()) && hpPred) continue;

            if (GetBool("QafterAA")) continue;

            if (Harass() && OktwCommon::CanHarras() &&
                GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                p.Mana() > m_RMANA + m_WMANA + m_QMANA) {
                m_Q.Cast(t);
            }

            if (t.IsMelee() && t.Position().Distance(p.ServerPosition()) > 300.0f) continue;

            if (Combo()) m_Q.Cast(t);
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
