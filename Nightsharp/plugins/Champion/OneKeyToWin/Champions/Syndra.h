#pragma once
// Port of OKTW_CSharp/Champions/Syndra.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class SyndraPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Syndra"; }
    const char* GetInternalId() const override { return "champion.oktw.syndra"; }
    const char* GetChampionName() const override { return "Syndra"; }

protected:
    Spell m_EQ{ SpellSlot::Q };
    Spell m_Eany{ SpellSlot::Q };
    bool  m_EQcastNow = false;

    void BuildMenu() override {
        MarkActive();

        m_Q    = Spell(SpellSlot::Q, 790.0f);
        m_W    = Spell(SpellSlot::W, 950.0f);
        m_E    = Spell(SpellSlot::E, 700.0f);
        m_EQ   = Spell(SpellSlot::Q, m_Q.Range + 500.0f);
        m_Eany = Spell(SpellSlot::Q, m_Q.Range + 500.0f);
        m_R    = Spell(SpellSlot::R, 675.0f);

        m_Q.SetSkillshot(0.6f,  125.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_W.SetSkillshot(0.25f, 140.0f, 1600.0f, false, SDK::SpellType::Circle);
        m_E.SetSkillshot(0.25f, 100.0f, 2500.0f, false, SDK::SpellType::Line);
        m_EQ.SetSkillshot(0.6f, 100.0f, 2500.0f, false, SDK::SpellType::Line);
        m_Eany.SetSkillshot(0.30f, 50.0f, 2500.0f, false, SDK::SpellType::Line);

        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw when skill rdy", true));

        m_qMenu->Add(new MenuBool("autoQ",       "Auto Q", true));
        m_qMenu->Add(new MenuBool("harassQ",     "Harass Q", true));
        m_qMenu->Add(new MenuSlider("QHarassMana", "Harass Mana", 30, 0, 100));

        m_wMenu->Add(new MenuBool("autoW",   "Auto W", true));
        m_wMenu->Add(new MenuBool("harassW", "Harass W", true));

        m_eMenu->Add(new MenuBool("autoE",        "Auto Q + E combo, ks", true));
        m_eMenu->Add(new MenuBool("harassE",      "Harass Q + E", false));
        m_eMenu->Add(new MenuBool("EInterrupter", "Auto Q + E Interrupter", true));
        m_eMenu->Add(new MenuKeyBind("useQE", "Semi-manual Q + E near mouse key", 'T', SDK::UI::KeyBindType::Press));

        Menu* egap = m_eMenu->AddSubMenu(new Menu("EGapSub", "Auto Q + E Gapcloser"));
        Menu* euse = m_eMenu->AddSubMenu(new Menu("EUseSub", "Use Q + E on"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string idG = std::string("Egapcloser") + enemy.CharacterName();
            const std::string idE = std::string("Eon")        + enemy.CharacterName();
            egap->Add(new MenuBool(idG.c_str(), enemy.CharacterName().c_str(), true));
            euse->Add(new MenuBool(idE.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_rMenu->Add(new MenuBool("autoR",  "Auto R KS", true));
        m_rMenu->Add(new MenuBool("Rcombo", "Extra combo dmg calculation", true));

        Menu* rUseOn = m_rMenu->AddSubMenu(new Menu("RUseOn", "Use on"));
        static const char* rModes[] = { "KS ", "Always ", "Never " };
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("Rmode") + enemy.CharacterName();
            rUseOn->Add(new MenuList(id.c_str(), enemy.CharacterName().c_str(), rModes, 3, 0));
        }

        m_farmMenu->Add(new MenuBool("farmQout", "Last hit Q minion out range AA", true));
        m_farmMenu->Add(new MenuBool("farmQ",    "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("farmW",    "Lane clear W", true));
        m_farmMenu->Add(new MenuBool("jungleQ",  "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW",  "Jungle clear W", true));

        // TODO(oktw-port): AntiGapcloser.OnEnemyGapcloser, Interrupter2.OnInterruptableTarget,
        //                  Obj_AI_Base.OnProcessSpellCast, GameObject.OnCreate (ball tracking)
        //                  not available in NightSharp SDK.
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
        if (!m_E.IsReady()) m_EQcastNow = false;

        if (LagFree(0)) { SetMana(); Jungle(); }
        if (LagFree(1) && m_E.IsReady() && GetBool("autoE")) LogicE();
        if (LagFree(2) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(3) && m_W.IsReady() && GetBool("autoW")) LogicW();
        if (LagFree(4) && m_R.IsReady() && GetBool("autoR")) LogicR();
    }

    void TryBallE(const AIHeroClient& t) {
        if (m_Q.IsReady()) {
            auto pout = m_EQ.GetPrediction(t);
            Vector3 castQpos = pout.GetCastPosition();
            if (Player().Position().Distance(castQpos) > m_Q.Range) {
                const Vector3 dir = (castQpos - Player().Position());
                const float len = dir.Length();
                if (len > 0.001f)
                    castQpos = Player().Position() + dir * (m_Q.Range / len);
            }
            m_EQcastNow = true;
            m_Q.Cast(castQpos);
        }
        // TODO(oktw-port): ball tracking requires GameObject.OnCreate.
    }

    void LogicE() {
        if (GetKey("useQE")) {
            const Vector3 cursor = SDK::Game::CursorPos();
            AIHeroClient mouseTarget;
            float best = FLT_MAX;
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, m_Eany.Range, true)) continue;
                const float d = enemy.Position().Distance(cursor);
                if (d < best) { best = d; mouseTarget = enemy; }
            }
            if (mouseTarget.IsValid()) { TryBallE(mouseTarget); return; }
        }

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Eany.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (OktwCommon::GetKsDamage(t, m_E) + m_Q.GetDamage(t) > t.Health()) TryBallE(t);
            if (Combo() && p.Mana() > m_RMANA + m_EMANA + m_QMANA &&
                GetBool((std::string("Eon") + t.CharacterName()).c_str()))
                TryBallE(t);
            if (Harass() && p.Mana() > m_RMANA + m_EMANA + m_QMANA + m_WMANA &&
                GetBool("harassE") &&
                GetBool((std::string("Harass") + t.CharacterName()).c_str()))
                TryBallE(t);
        }
    }

    void LogicR() {
        // Range scales with level: 750 at rank 3, otherwise 675.
        m_R = Spell(SpellSlot::R, (m_R.Instance().Level() == 3) ? 750.0f : 675.0f);
        const bool rCombo = GetBool("Rcombo");
        const auto p = Player();

        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(enemy, m_R.Range, true)) continue;
            if (!OktwCommon::ValidUlt(enemy)) continue;

            const std::string rid = std::string("Rmode") + enemy.CharacterName();
            const int rMode = GetList(rid.c_str(), 0);
            if (rMode == 2) continue;
            if (rMode == 1) { m_R.Cast(enemy); continue; }

            float comboDMG = OktwCommon::GetKsDamage(enemy, m_R);
            if (rCombo) {
                if (m_Q.IsReady() && SDK::Extensions::IsValidTarget(enemy, 600.0f, true))
                    comboDMG += m_Q.GetDamage(enemy);
                if (m_E.IsReady()) comboDMG += m_E.GetDamage(enemy);
                if (m_W.IsReady()) comboDMG += m_W.GetDamage(enemy);
            }
            if (enemy.Health() < comboDMG) m_R.Cast(enemy);
        }
    }

    void CatchW(const SDK::AIBaseClient& nearUnit) {
        const auto p = Player();
        const float catchRange = 925.0f;
        auto minions = OktwCommon::GetMinions(p.ServerPosition(), catchRange);
        if (minions.empty()) return;
        SDK::AIBaseClient obj = minions.front();
        for (const auto& m : minions) {
            if (nearUnit.IsValid() && nearUnit.Position().Distance(m.Position()) <
                nearUnit.Position().Distance(obj.Position()))
                obj = m;
        }
        m_W.Cast(obj.Position());
    }

    void LogicW() {
        // TODO(oktw-port): ToggleState detection unavailable; treat as fresh cast branch.
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            CastSpell(m_W, t);
        } else if (FarmSpells() && GetBool("farmW")) {
            const auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_W.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_W.GetCircularFarmLocation(baseList, m_W.Width);
            if (farm.MinionsHit > 1) m_W.Cast(farm.Position);
        }
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (Combo() && p.Mana() > m_RMANA + m_QMANA + m_EMANA && !m_E.IsReady()) {
                CastSpell(m_Q, t);
            } else if (Harass() && GetBool("harassQ") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.ManaPercent() > GetSlider("QHarassMana", 30) &&
                       OktwCommon::CanHarras()) {
                CastSpell(m_Q, t);
            } else if (OktwCommon::GetKsDamage(t, m_Q) > t.Health()) {
                CastSpell(m_Q, t);
            } else if (p.Mana() > m_RMANA + m_QMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (!SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true)) continue;
                    if (!OktwCommon::CanMove(enemy)) CastSpell(m_Q, t);
                }
            }
        }

        if (!None() && !Combo()) {
            auto allMinions = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range);
            if (GetBool("farmQout") && p.Mana() > m_RMANA + m_QMANA + m_EMANA + m_WMANA) {
                for (const auto& minion : allMinions) {
                    if (!SDK::Extensions::IsValidTarget(minion, m_Q.Range, true)) continue;
                    if (minion.Health() < m_Q.GetDamage(minion)) {
                        m_Q.Cast(minion);
                        return;
                    }
                }
            }
            if (FarmSpells() && GetBool("farmQ")) {
                std::vector<SDK::AIBaseClient> baseList(allMinions.begin(), allMinions.end());
                auto farm = m_Q.GetCircularFarmLocation(baseList, m_Q.Width);
                if (farm.MinionsHit >= FarmMinions()) m_Q.Cast(farm.Position);
            }
        }
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_QMANA) return;
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(mob.ServerPosition()); return; }
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(mob.ServerPosition()); return; }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
