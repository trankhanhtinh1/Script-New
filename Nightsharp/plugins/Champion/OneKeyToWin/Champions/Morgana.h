#pragma once
// Port of OKTW_CSharp/Champions/Morgana.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;

class MorganaPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Morgana"; }
    const char* GetInternalId() const override { return "champion.oktw.morgana"; }
    const char* GetChampionName() const override { return "Morgana"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 1150.0f);
        m_W = Spell(SpellSlot::W, 1000.0f);
        m_E = Spell(SpellSlot::E, 800.0f);
        m_R = Spell(SpellSlot::R, 600.0f);
        m_Q.SetSkillshot(0.25f, 70.0f, 1200.0f, true,  SDK::SpellType::Line);
        m_W.SetSkillshot(0.50f, 200.0f, 2200.0f, false, SDK::SpellType::Circle);

        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw when skill rdy", true));

        m_qMenu->Add(new MenuBool("ts",  "Use common TargetSelector", true));
        m_qMenu->Add(new MenuBool("ts1", "ON - only one target", false));
        m_qMenu->Add(new MenuBool("ts2", "OFF - all targets", false));
        m_qMenu->Add(new MenuBool("qCC", "Auto Q cc & dash enemy", true));

        Menu* grabSub = m_qMenu->AddSubMenu(new Menu("QuseOn", "Use on"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("grab") + enemy.CharacterName();
            grabSub->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_wMenu->Add(new MenuBool("autoW",   "Auto W", true));
        m_wMenu->Add(new MenuBool("autoWcc", "Auto W only CC enemy", false));

        // TODO(oktw-port): E Shield Spell Manager (needs OnProcessSpellCast) omitted.
        Menu* shieldAlly = m_eMenu->AddSubMenu(new Menu("EShieldAlly", "Shield ally"));
        for (const auto& ally : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!ally.IsValid() || ally.IsEnemy()) continue;
            Menu* am = shieldAlly->AddSubMenu(new Menu(
                (std::string("EShield_") + ally.CharacterName()).c_str(),
                ally.CharacterName().c_str()));
            am->Add(new MenuBool((std::string("skillshot") + ally.CharacterName()).c_str(), "skillshot", true));
            am->Add(new MenuBool((std::string("targeted") + ally.CharacterName()).c_str(), "targeted", true));
            am->Add(new MenuBool((std::string("HardCC") + ally.CharacterName()).c_str(), "Hard CC", true));
            am->Add(new MenuBool((std::string("Poison") + ally.CharacterName()).c_str(), "Poison", true));
        }

        m_rMenu->Add(new MenuSlider("rCount", "Auto R if enemies in range", 3, 0, 5));
        m_rMenu->Add(new MenuBool("rKs",   "R ks", false));
        m_rMenu->Add(new MenuBool("inter", "OnPossibleToInterrupt", true));
        m_rMenu->Add(new MenuBool("Gap",   "OnEnemyGapcloser", true));

        m_farmMenu->Add(new MenuBool("farmW",    "Lane clear W", true));
        m_farmMenu->Add(new MenuBool("jungleQ",  "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW",  "Jungle clear W", true));

        // TODO(oktw-port): AntiGapcloser.OnEnemyGapcloser, Interrupter2.OnInterruptableTarget,
        //                  Obj_AI_Base.OnProcessSpellCast not available in NightSharp SDK.
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
        if (LagFree(1) && m_Q.IsReady()) LogicQ();
        if (LagFree(2) && m_R.IsReady()) LogicR();
        if (LagFree(3) && m_W.IsReady() && GetBool("autoW")) LogicW();
        if (LagFree(4) && m_E.IsReady()) LogicE();
    }

    void LogicE() {
        const auto p = Player();
        for (const auto& ally : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!ally.IsValid() || ally.IsEnemy()) continue;
            if (ally.Position().Distance(p.Position()) >= m_E.Range) continue;
            const std::string id = std::string("Poison") + ally.CharacterName();
            if (GetBool(id.c_str()) && ally.HasBuff("Poison")) {
                m_E.Cast(ally);
            }
        }
    }

    void LogicQ() {
        const auto p = Player();
        if (Combo() && GetBool("ts")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Physical) : AIHeroClient();
            if (t.IsValid() && SDK::Extensions::IsValidTarget(t, m_Q.Range, true)) {
                const std::string gid = std::string("grab") + t.CharacterName();
                if (GetBool(gid.c_str())) {
                    CastSpell(m_Q, t);
                }
            }
        }
        for (const auto& t : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!t.IsValid() || !t.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(t, m_Q.Range, true)) continue;
            const std::string gid = std::string("grab") + t.CharacterName();
            if (!GetBool(gid.c_str())) continue;

            if (Combo() && !GetBool("ts")) CastSpell(m_Q, t);

            if (GetBool("qCC")) {
                if (!OktwCommon::CanMove(t)) m_Q.Cast(t);
            }
        }
    }

    void LogicR() {
        const bool rKs = GetBool("rKs");
        for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!target.IsValid() || !target.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(target, m_R.Range, true)) continue;
            if (!target.HasBuff("rocketgrab2")) continue;
            if (rKs && m_R.GetDamage(target) > target.Health()) {
                m_R.Cast();
            }
        }
        const int rCount = GetSlider("rCount", 3);
        if (rCount > 0 && OktwCommon::CountEnemiesInRange(Player().Position(), m_R.Range) >= rCount) {
            m_R.Cast();
        }
    }

    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (!GetBool("autoWcc") && !m_Q.IsReady()) {
                if (m_W.GetDamage(t) > t.Health()) {
                    CastSpell(m_W, t);
                } else if (Combo() && p.Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA) {
                    CastSpell(m_W, t);
                }
            }
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, m_W.Range, true)) continue;
                if (!OktwCommon::CanMove(enemy)) m_W.Cast(enemy);
            }
        } else if (FarmSpells() && GetBool("farmW") && p.Mana() > m_RMANA + m_WMANA) {
            const auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_W.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_W.GetCircularFarmLocation(baseList, m_W.Width);
            if (farm.MinionsHit >= FarmMinions()) m_W.Cast(farm.Position);
        }
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_WMANA + m_RMANA + m_WMANA) return;
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 600.0f, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(mob.ServerPosition()); return; }
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(mob.ServerPosition()); return; }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
