#pragma once
// Port of OKTW_CSharp/Champions/Quinn.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;

class QuinnPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Quinn"; }
    const char* GetInternalId() const override { return "champion.oktw.quinn"; }
    const char* GetChampionName() const override { return "Quinn"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 1000.0f);
        m_E = Spell(SpellSlot::E, 700.0f);
        m_W = Spell(SpellSlot::W, 2100.0f);
        m_R = Spell(SpellSlot::R, 550.0f);

        m_Q.SetSkillshot(0.25f, 90.0f, 1550.0f, true, SDK::SpellType::Line);
        m_E.SetTargetted(0.25f, 2000.0f);

        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));

        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q", true));
        m_qMenu->Add(new MenuBool("harassQ", "Harass Q", true));

        m_eMenu->Add(new MenuBool("autoE",   "Auto E", true));
        m_eMenu->Add(new MenuBool("harassE", "Harass E", true));
        m_eMenu->Add(new MenuBool("AGC",     "AntiGapcloser E", true));
        m_eMenu->Add(new MenuBool("Int",     "Interrupter E", true));

        Menu* gap = m_eMenu->AddSubMenu(new Menu("GapCloser", "GapCloser"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("gap") + enemy.CharacterName();
            gap->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_champMenu->Add(new MenuBool("autoW",  "Auto W", true));
        m_champMenu->Add(new MenuBool("autoR",  "Auto R in shop", true));
        m_champMenu->Add(new MenuBool("focusP", "Focus marked enemy", true));

        m_farmMenu->Add(new MenuBool("farmP",    "Attack marked minion first", true));
        m_farmMenu->Add(new MenuBool("farmQ",    "Farm Q", true));
        m_farmMenu->Add(new MenuBool("jungleE",  "Jungle clear E", true));
        m_farmMenu->Add(new MenuBool("jungleQ",  "Jungle clear Q", true));

        // TODO(oktw-port): AntiGapcloser.OnEnemyGapcloser — Auto-E on enemy gapcloser (per-champ toggle)
        // TODO(oktw-port): Interrupter2.OnInterruptableTarget — Auto-E on interruptible target
        // TODO(oktw-port): Orbwalking.AfterAttack — Q/E cast on hero after AA, plus Jungle()
        // TODO(oktw-port): Orbwalking.BeforeAttack — ForceTarget on quinnw-marked hero/minion
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
            m_RMANA = m_WMANA - 0.0f * m_W.Instance().Cooldown();
        } else {
            m_RMANA = m_R.Instance().ManaCost();
        }
    }

    void OnGameUpdate() override {
        if (LagFree(1)) SetMana();
        if (LagFree(2) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(4) && m_R.IsReady() && GetBool("autoR")) LogicR();
    }

    void Jungle() {
        const auto p = Player();
        if (LaneClear() && p.Mana() > m_RMANA + m_WMANA + m_RMANA + m_WMANA) {
            auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 700.0f, false, true);
            if (!mobs.empty()) {
                const auto& mob = mobs.front();
                if (mob.HasBuff("QuinnW")) return;

                if (m_Q.IsReady() && GetBool("jungleQ")) {
                    m_Q.Cast(mob.ServerPosition());
                    return;
                }
                if (m_E.IsReady() && GetBool("jungleE")) {
                    m_E.Cast(mob);
                    return;
                }
            }
        }
    }

    void LogicR() {
        const auto p = Player();
        // C#: if (Player.InFountain() && R.Instance.Name == "QuinnR") R.Cast();
        if (false /* TODO(oktw-port): InFountain not available in SDK */ && m_R.Instance().Name() == std::string("QuinnR")) {
            m_R.Cast();
        }
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (SDK::Core::Utils::AutoAttack::InAutoAttackRange(t) && t.HasBuff("quinnw"))
                return;

            if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                CastSpell(m_Q, t);
            } else if (Harass() && p.Mana() > m_RMANA + m_EMANA + m_QMANA + m_WMANA &&
                       GetBool("harassQ") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       OktwCommon::CanHarras()) {
                CastSpell(m_Q, t);
            } else if (OktwCommon::GetKsDamage(t, m_Q) > t.Health()) {
                CastSpell(m_Q, t);
            }

            if (!None() && p.Mana() > m_RMANA + m_QMANA + m_EMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (enemy.IsValid() && enemy.IsEnemy() &&
                        SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_Q.Cast(enemy);
                    }
                }
            }
        } else if (FarmSpells() && GetBool("farmQ")) {
            auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_Q.Range - 150.0f);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_Q.GetCircularFarmLocation(baseList, 150.0f);
            if (farm.MinionsHit >= FarmMinions())
                m_Q.Cast(farm.Position);
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
