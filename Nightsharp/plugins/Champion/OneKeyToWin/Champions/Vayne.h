#pragma once
// Port of OKTW_CSharp/Champions/Vayne.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class VaynePlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Vayne"; }
    const char* GetInternalId() const override { return "champion.oktw.vayne"; }
    const char* GetChampionName() const override { return "Vayne"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 300.0f);
        m_E = Spell(SpellSlot::E, 670.0f);
        m_W = Spell(SpellSlot::E, 670.0f);
        m_R = Spell(SpellSlot::R, 3000.0f);

        m_E.SetTargetted(0.25f, 2200.0f);

        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("eRange2", "E push position", false));

        m_qMenu->Add(new MenuBool("autoQ",  "Auto Q", true));
        m_qMenu->Add(new MenuSlider("Qstack", "Q at X stack", 2, 1, 2));
        m_qMenu->Add(new MenuBool("QE",     "try Q + E", true));
        m_qMenu->Add(new MenuBool("Qonly",  "Q only after AA", false));

        Menu* gapSub = m_eMenu->AddSubMenu(new Menu("GapCloserSub", "GapCloser"));
        gapSub->Add(new MenuBool("gapE", "Enable", true));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string idG = std::string("gap") + enemy.CharacterName();
            gapSub->Add(new MenuBool(idG.c_str(), enemy.CharacterName().c_str(), true));
        }
        Menu* useESub = m_eMenu->AddSubMenu(new Menu("UseESub", "Use E"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string idS = std::string("stun") + enemy.CharacterName();
            useESub->Add(new MenuBool(idS.c_str(), enemy.CharacterName().c_str(), true));
        }
        m_eMenu->Add(new MenuKeyBind("useE", "OneKeyToCast E closest person", 'T', SDK::KeyBindType::Press));
        m_eMenu->Add(new MenuBool("Eks",     "E KS", true));
        m_eMenu->Add(new MenuBool("Ecombo",  "E combo only", false));

        m_rMenu->Add(new MenuBool("autoR",    "Auto R", true));
        m_rMenu->Add(new MenuBool("visibleR", "Unvisable block AA", true));
        m_rMenu->Add(new MenuBool("autoQR",   "Auto Q when R active", true));

        m_farmMenu->Add(new MenuBool("farmQ",       "Q farm helper", true));
        m_farmMenu->Add(new MenuBool("farmQjungle", "Q jungle", true));

        // TODO(oktw-port): Orbwalking.BeforeAttack/AfterAttack, Interrupter2, AntiGapcloser events not available
    }

    void SetMana() override {
        m_QMANA = 0.0f;
        m_WMANA = 0.0f;
        m_EMANA = m_E.IsReady() ? m_E.Instance().ManaCost() : 0.0f;
        m_RMANA = m_R.IsReady() ? m_R.Instance().ManaCost() : 0.0f;
    }

    int GetWStacks(const SDK::AIBaseClient& target) {
        // Approximate: buff count via HasBuff (stacks not directly available)
        return target.HasBuff("vaynesilvereddebuff") ? 2 : 0;
    }

    float WDmg(const SDK::AIBaseClient& target) {
        return target.MaxHealth() * (4.5f + m_W.Level() * 1.5f) * 0.01f;
    }

    void OnGameUpdate() override {
        SetMana();
        const auto p = Player();
        const Vector3 cursor = SDK::Game::CursorPos();
        Vector3 dashPosition = p.Position();
        {
            const Vector3 dir = cursor - p.Position();
            const float len = dir.Length();
            if (len > 0.001f) dashPosition = p.Position() + dir * (m_Q.Range / len);
        }

        if (m_E.IsReady()) {
            if (!GetBool("Ecombo") || Combo()) {
                for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!target.IsValid() || !target.IsEnemy()) continue;
                    if (!SDK::Extensions::IsValidTarget(target, m_E.Range, true)) continue;
                    const std::string idS = std::string("stun") + target.CharacterName();
                    if (GetBool(idS.c_str())) {
                        // Simplified condemn - just cast on stationary/close targets
                        if (!OktwCommon::CanMove(target)) m_E.Cast(target);
                    }
                }
            }
        }

        if (LagFree(1) && m_Q.IsReady()) {
            if (GetBool("autoQR") && p.HasBuff("vayneinquisition") &&
                OktwCommon::CountEnemiesInRange(p.Position(), 1500.0f) > 0 &&
                OktwCommon::CountEnemiesInRange(p.Position(), 670.0f) != 1) {
                m_Q.Cast(dashPosition);
            }
            if (Combo() && GetBool("autoQ") && !GetBool("Qonly")) {
                auto* ts = SDK::TargetSelector::Instance();
                auto t = ts ? ts->GetTarget(900.0f, DamageType::Physical) : AIHeroClient();
                if (t.IsValid() &&
                    t.Position().Distance(cursor) < t.Position().Distance(p.Position())) {
                    m_Q.Cast(dashPosition);
                }
            }
        }

        if (LagFree(2)) {
            AIHeroClient bestEnemy;
            bool haveBest = false;
            for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!target.IsValid() || !target.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(target, m_E.Range, true)) continue;
                if (SDK::Extensions::IsValidTarget(target, 250.0f, true) && target.IsMelee()) {
                    if (m_Q.IsReady() && GetBool("autoQ")) {
                        m_Q.Cast(dashPosition);
                    } else if (m_E.IsReady() && p.Health() < p.MaxHealth() * 0.4f) {
                        m_E.Cast(target);
                    }
                }
                if (!haveBest || p.Position().Distance(target.Position()) < p.Position().Distance(bestEnemy.Position())) {
                    bestEnemy = target;
                    haveBest = true;
                }
            }
            if (GetKey("useE") && haveBest) {
                m_E.Cast(bestEnemy);
            }
        }

        if (LagFree(3) && m_R.IsReady() && GetBool("autoR")) {
            if (OktwCommon::CountEnemiesInRange(p.Position(), 700.0f) > 2)
                m_R.Cast();
            else if (Combo() && OktwCommon::CountEnemiesInRange(p.Position(), 600.0f) > 1)
                m_R.Cast();
            else if (p.Health() < p.MaxHealth() * 0.5f &&
                     OktwCommon::CountEnemiesInRange(p.Position(), 500.0f) > 0)
                m_R.Cast();
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
