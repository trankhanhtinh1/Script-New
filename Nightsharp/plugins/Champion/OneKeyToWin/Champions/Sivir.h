#pragma once
// Port of OKTW_CSharp/Champions/Sivir.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;

class SivirPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Sivir"; }
    const char* GetInternalId() const override { return "champion.oktw.sivir"; }
    const char* GetChampionName() const override { return "Sivir"; }

protected:
    Spell m_Q1{ SpellSlot::Q, 1200.0f };  // second-form Q (collision-aware)

    void BuildMenu() override {
        MarkActive();

        m_Q  = Spell(SpellSlot::Q, 1200.0f);
        m_Q1 = Spell(SpellSlot::Q, 1200.0f);
        m_W  = Spell(SpellSlot::W, FLT_MAX);
        m_E  = Spell(SpellSlot::E, FLT_MAX);
        m_R  = Spell(SpellSlot::R, 25000.0f);

        m_Q.SetSkillshot(0.25f, 90.0f, 1350.0f, false, SDK::SpellType::Line);
        m_Q1.SetSkillshot(0.25f, 90.0f, 1350.0f, true, SDK::SpellType::Line);

        // TODO(oktw-port): Core.MissileReturn("SivirQMissile", "SivirQMissileReturn", Q)

        m_drawMenu->Add(new MenuBool("notif",   "Notification (timers)", true));
        m_drawMenu->Add(new MenuBool("noti",    "Show KS notification", true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));

        m_farmMenu->Add(new MenuBool("farmQ",   "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("farmW",   "Lane clear W", true));
        m_farmMenu->Add(new MenuBool("jungleQ", "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW", "Jungle clear W", true));

        m_champMenu->Add(new MenuBool("harassW", "Harass W", true));

        // Per-enemy per-spell block manager (E Shield Config → Spell Manager → <Champ>)
        Menu* eShield = m_champMenu->AddSubMenu(new Menu("EShieldSub", "E Shield Config"));
        Menu* spellMgr = eShield->AddSubMenu(new Menu("SpellMgr", "Spell Manager"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            Menu* enemyMenu = spellMgr->AddSubMenu(new Menu(enemy.CharacterName().c_str(), enemy.CharacterName().c_str()));
            // TODO(oktw-port): spell targeting-type (Self/Unit/SelfAndUnit) filtering not available; all spells listed, default off
            for (int i = 0; i < 4; ++i) {
                const auto& spell = enemy.Spellbook().Spells()[i];
                const std::string spName = spell.Name();
                if (spName.empty()) continue;
                const std::string id = std::string("spell") + spName;
                enemyMenu->Add(new MenuBool(id.c_str(), spName.c_str(), false));
            }
        }

        m_champMenu->Add(new MenuBool("autoR", "Auto R", true));

        eShield->Add(new MenuBool("autoE",        "Auto E", true));
        eShield->Add(new MenuBool("autoEmissile", "Block unknown missile", true));
        eShield->Add(new MenuBool("AGC",          "AntiGapcloserE", true));
        eShield->Add(new MenuSlider("Edmg",       "Block under % hp", 90, 0, 100));

        // TODO(oktw-port): GameObject.OnCreate — auto-E on unknown missile at Player
        // TODO(oktw-port): Obj_AI_Base.OnProcessSpellCast — auto-E vs targeted/skillshot at Player
        // TODO(oktw-port): AntiGapcloser.OnEnemyGapcloser — auto-E on gapcloser
        // TODO(oktw-port): Orbwalking.AfterAttack — W reset combo/harass/farm
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
        if (!m_R.IsReady()) {
            // TODO(oktw-port): PARRegenRate unavailable
            m_RMANA = m_QMANA - 0.0f * m_Q.Instance().Cooldown();
        } else {
            m_RMANA = m_R.Instance().ManaCost();
        }
    }

    void OnGameUpdate() override {
        if (LagFree(0)) SetMana();

        if (LagFree(1) && m_Q.IsReady() && !Player().Spellbook().IsWindingUp()) {
            LogicQ();
        }
        if (LagFree(2) && m_R.IsReady() && Combo() && GetBool("autoR")) {
            LogicR();
        }
        if (LagFree(3) && LaneClear()) {
            Jungle();
        }
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            // TODO(oktw-port): missileManager.Target = t;
            float qDmg = OktwCommon::GetKsDamage(t, m_Q) * 1.9f;
            if (SDK::Core::Utils::AutoAttack::InAutoAttackRange(t))
                qDmg += p.GetAutoAttackDamage(t, false) * 3.0f;

            if (qDmg > t.Health()) {
                m_Q.Cast(t);
            } else if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                CastSpell(m_Q, t);
            } else if (Harass() &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       !p.IsUnderEnemyTurret()) {
                if (p.Mana() > p.MaxMana() * 0.9f) {
                    CastSpell(m_Q, t);
                } else if (p.Mana() > m_RMANA + m_WMANA + m_QMANA + m_QMANA) {
                    CastSpell(m_Q1, t);
                } else if (p.Mana() > m_RMANA + m_WMANA + m_QMANA + m_QMANA) {
                    m_Q.CastIfWillHit(t, 2);
                    if (LaneClear())
                        CastSpell(m_Q, t);
                }
            }

            if (p.Mana() > m_RMANA + m_WMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (enemy.IsValid() && enemy.IsEnemy() &&
                        SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_Q.Cast(enemy);
                    }
                }
            }
        } else if (FarmSpells() && GetBool("farmQ")) {
            auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_Q.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_Q.GetLineFarmLocation(baseList, m_Q.Width);
            if (farm.MinionsHit >= FarmMinions())
                m_Q.Cast(farm.Position);
        }
    }

    void LogicR() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(800.0f, DamageType::Physical) : AIHeroClient();
        const auto p = Player();
        if (p.CountEnemyHeroesInRange(800.0f) > 2) {
            m_R.Cast();
        } else if (t.IsValid() &&
                   !SDK::Orbwalker::GetTarget().IsValid() &&
                   Combo() &&
                   p.GetAutoAttackDamage(t, false) * 2.0f > t.Health() &&
                   !m_Q.IsReady() &&
                   OktwCommon::CountEnemiesInRange(t.Position(), 800.0f) < 3) {
            m_R.Cast();
        }
    }

    void Jungle() {
        const auto p = Player();
        if (p.Mana() > m_RMANA + m_WMANA + m_RMANA) {
            auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 600.0f, false, true);
            if (!mobs.empty()) {
                const auto& mob = mobs.front();
                if (m_W.IsReady() && GetBool("jungleW")) {
                    m_W.Cast();
                    return;
                }
                if (m_Q.IsReady() && GetBool("jungleQ")) {
                    m_Q.Cast(mob);
                    return;
                }
            }
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
