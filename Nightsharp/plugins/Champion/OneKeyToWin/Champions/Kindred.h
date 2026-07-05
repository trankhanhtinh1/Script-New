#pragma once
// Port of OKTW_CSharp/Champions/Kindred.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuSlider;

class KindredPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Kindred"; }
    const char* GetInternalId() const override { return "champion.oktw.kindred"; }
    const char* GetChampionName() const override { return "Kindred"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 340.0f);
        m_W = Spell(SpellSlot::W, 800.0f);
        m_E = Spell(SpellSlot::E, 600.0f);
        m_R = Spell(SpellSlot::R, 550.0f);

        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));

        m_qMenu->Add(new MenuBool("autoQ", "Auto Q", true));
        // TODO(oktw-port): Dash = new Core.OKTWdash(Q); — Kindred Q dash helper

        m_wMenu->Add(new MenuBool("autoW",   "Auto W", true));
        m_wMenu->Add(new MenuBool("harassW", "Harass W", true));

        m_eMenu->Add(new MenuBool("autoE",   "Auto E", true));
        m_eMenu->Add(new MenuBool("harassE", "Harass E", true));
        Menu* eUse = m_eMenu->AddSubMenu(new Menu("EuseSub", "Use on:"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("Euse") + enemy.CharacterName();
            eUse->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_rMenu->Add(new MenuBool("autoR", "Auto R", true));
        m_rMenu->Add(new MenuSlider("Renemy", "Don't R if x enemies", 4, 0, 5));

        m_farmMenu->Add(new MenuBool("farmQ",   "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("farmW",   "Lane clear W", true));
        m_farmMenu->Add(new MenuBool("farmE",   "Lane clear E", true));
        m_farmMenu->Add(new MenuBool("jungleQ", "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW", "Jungle clear W", true));
        m_farmMenu->Add(new MenuBool("jungleE", "Jungle clear E", true));

        // TODO(oktw-port): SebbyLib.Orbwalking.AfterAttack += Orbwalker_AfterAttack — Q on-hit dash follow-up
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
        if (LagFree(0)) { SetMana(); Jungle(); }

        if (LagFree(1) && m_E.IsReady() && GetBool("autoE")) LogicE();
        if (LagFree(2) && m_W.IsReady() && GetBool("autoW")) LogicW();
        if (LagFree(3) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (m_R.IsReady() && GetBool("autoR")) LogicR();
    }

    void LogicQ() {
        const auto p = Player();
        if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
            // TODO(oktw-port): Orbwalker.GetTarget() gate + Dash.CastDash() — cast Q on dash pos if enemies nearby
            const Vector3 cursor = SDK::Game::CursorPos();
            if (OktwCommon::CountEnemiesInRange(cursor, 500.0f) > 0) {
                m_Q.Cast(cursor);
            }
        }
        if (FarmSpells() && GetBool("farmQ")) {
            const auto minions = OktwCommon::GetMinions(p.ServerPosition(), 400.0f);
            if (static_cast<int>(minions.size()) >= FarmMinions())
                m_Q.Cast(SDK::Game::CursorPos());
        }
    }

    void LogicW() {
        const auto p = Player();
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(650.0f, DamageType::Physical) : AIHeroClient();
        if (t.IsValid() && !m_Q.IsReady()) {
            if (Combo() && p.Mana() > m_RMANA + m_WMANA) {
                m_W.Cast();
            } else if (Harass() && GetBool("harassW") &&
                       p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                m_W.Cast();
            }
        }
        auto tks = ts ? ts->GetTarget(1600.0f, DamageType::Physical) : AIHeroClient();
        if (tks.IsValid()) {
            if (m_W.GetDamage(tks) * 3.0f > tks.Health() - OktwCommon::GetIncomingDamage(tks))
                m_W.Cast();
        }

        if (FarmSpells() && GetBool("farmW")) {
            const auto minions = OktwCommon::GetMinions(p.ServerPosition(), 600.0f);
            if (static_cast<int>(minions.size()) >= FarmMinions())
                m_W.Cast();
        }
    }

    void LogicE() {
        // TODO(oktw-port): C# uses Orbwalker.GetTarget() to require an AA target;
        // approximate via nearest enemy in E range.
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Physical) : AIHeroClient();
        if (!t.IsValid()) return;

        const auto p = Player();
        if (SDK::Extensions::IsValidTarget(t, m_E.Range, true)) {
            const std::string useId = std::string("Euse") + t.CharacterName();
            if (!GetBool(useId.c_str())) return;
            if (Combo() && p.Mana() > m_RMANA + m_EMANA) {
                m_E.CastOnUnit(t);
            } else if (Harass() && GetBool("harassE") &&
                       p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                m_E.CastOnUnit(t);
            }
        }
    }

    void LogicR() {
        const int rEnemy = GetSlider("Renemy", 4);
        const auto p = Player();
        const float dmg = OktwCommon::GetIncomingDamage(p);
        if (dmg == 0.0f) return;

        if (p.Health() - dmg < static_cast<float>(p.Level()) * 10.0f &&
            p.CountEnemyHeroesInRange(500.0f) < rEnemy) {
            m_R.Cast(p);
        }
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 600.0f, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();

        if (m_E.IsReady() && GetBool("jungleE")) { m_E.Cast(mob); return; }
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(SDK::Game::CursorPos()); return; }
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(); return; }
    }

    void OnGameDraw() override {
        // Simplified: rely on SDK draw utilities for range circles
    }
};

} } // namespace Plugins::OKTW
