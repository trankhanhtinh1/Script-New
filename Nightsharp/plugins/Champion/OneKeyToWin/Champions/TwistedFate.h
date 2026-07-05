#pragma once
// Port of OKTW_CSharp/Champions/TwistedFate.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class TwistedFatePlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW TwistedFate"; }
    const char* GetInternalId() const override { return "champion.oktw.twistedfate"; }
    const char* GetChampionName() const override { return "TwistedFate"; }

protected:
    std::string m_temp;
    bool        m_cardok  = true;
    int         m_findCard = 0;
    std::string m_wName;

    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 1400.0f);
        m_E = Spell(SpellSlot::E, 700.0f);
        m_W = Spell(SpellSlot::W, 1200.0f);
        m_R = Spell(SpellSlot::R, 5500.0f);

        m_Q.SetSkillshot(0.25f, 40.0f, 1000.0f, false, SDK::SpellType::Line);
        m_R.SetSkillshot(1.00f, 40.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_E.SetTargetted(0.25f, 2000.0f);

        m_drawMenu->Add(new MenuBool("onlyRdy",    "Draw only ready spells", true));
        m_drawMenu->Add(new MenuBool("qRange",     "Q range", false));
        m_drawMenu->Add(new MenuBool("rRangeMini", "R range minimap", true));
        m_drawMenu->Add(new MenuBool("cardInfo",   "Show card info", true));
        m_drawMenu->Add(new MenuBool("notR",       "R info helper", true));

        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q", true));
        m_qMenu->Add(new MenuBool("harassQ", "Harass Q", true));

        static const char* wModes[] = { "Auto", "Manual" };
        m_wMenu->Add(new MenuList("Wmode",    "W mode", wModes, 2, 0));
        m_wMenu->Add(new MenuKeyBind("Wgold", "Gold key", 'Y', SDK::KeyBindType::Press));
        m_wMenu->Add(new MenuKeyBind("Wblue", "Blue key", 'U', SDK::KeyBindType::Press));
        m_wMenu->Add(new MenuKeyBind("Wred",  "RED key",  'I', SDK::KeyBindType::Press));
        m_wMenu->Add(new MenuBool("WblockAA", "Block AA if seeking GOLD card", true));
        m_wMenu->Add(new MenuBool("harassW",  "Harass GOLD low range", true));
        m_wMenu->Add(new MenuBool("ignoreW",  "Ignore first card", true));

        m_rMenu->Add(new MenuKeyBind("useR",     "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));
        m_rMenu->Add(new MenuBool("autoR",       "Auto R", true));
        m_rMenu->Add(new MenuSlider("Renemy",    "Don't R if enemy in x range", 1000, 0, 2000));
        m_rMenu->Add(new MenuSlider("RenemyA",   "Don't R if ally in x range near target", 800, 0, 2000));
        m_rMenu->Add(new MenuBool("turetR",      "Don't R under turret", true));

        m_farmMenu->Add(new MenuSlider("WredFarm", "LaneClear red card above % mana", 80, 0, 100));
        m_farmMenu->Add(new MenuBool("farmQ",      "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("farmW",      "Lane clear W Blue / Red card", false));
        m_farmMenu->Add(new MenuBool("jungleQ",    "Jungle clear Q", true));

        // TODO(oktw-port): GameObject.OnCreate event for TwistedFate_Base_W_ particle names
        // TODO(oktw-port): Orbwalking.BeforeAttack event to block AA when seeking GOLD card
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
        m_RMANA = m_R.IsReady() ? m_R.Instance().ManaCost() : m_WMANA;
    }

    void OnGameUpdate() override {
        if (LagFree(0)) SetMana();

        if (!GetBool("ignoreW")) m_cardok = true;

        if (m_W.IsReady()) {
            const int mode = GetList("Wmode", 0);
            if (mode == 0) LogicW();
            else           LogicWmanual();
        } else if (m_W.Instance().Name() == "PickACard") {
            m_temp.clear();
            m_cardok = false;
        }

        if (LagFree(2) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(4) && m_Q.IsReady()) Jungle();

        if (m_R.IsReady()) {
            if (LagFree(3) && m_W.IsReady() && GetBool("autoR")) LogicR();

            if (GetKey("useR")) {
                if (Player().HasBuff("destiny_marker")) {
                    auto* ts = SDK::TargetSelector::Instance();
                    auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Magical) : AIHeroClient();
                    if (t.IsValid()) m_R.Cast(t);
                } else {
                    m_R.Cast();
                }
            }
        }
    }

    void LogicWmanual() {
        const auto p = Player();
        if (!p.HasBuff("pickacard_tracker")) {
            if (m_R.IsReady() && (p.HasBuff("destiny_marker") || p.HasBuff("gate"))) {
                m_findCard = 1;
                m_W.Cast();
            } else if (GetKey("Wgold")) {
                m_findCard = 1;
                m_W.Cast();
            } else if (GetKey("Wblue")) {
                m_findCard = 2;
                m_W.Cast();
            } else if (GetKey("Wred")) {
                m_findCard = 3;
                m_W.Cast();
            }
        } else {
            if (m_temp.empty()) m_temp = m_wName;
            else if (m_temp != m_wName) m_cardok = true;

            if (m_cardok) {
                if (m_R.IsReady() && (p.HasBuff("destiny_marker") || p.HasBuff("gate"))) {
                    m_findCard = 1;
                    if (m_wName == "TwistedFate_Base_W_GoldCard.troy") m_W.Cast();
                } else if (m_findCard == 1) {
                    if (m_wName == "TwistedFate_Base_W_GoldCard.troy") m_W.Cast();
                } else if (m_findCard == 2) {
                    if (m_wName == "TwistedFate_Base_W_BlueCard.troy") m_W.Cast();
                } else if (m_findCard == 3) {
                    if (m_wName == "TwistedFate_Base_W_RedCard.troy") m_W.Cast();
                }
            }
        }
    }

    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(1100.0f, DamageType::Magical) : AIHeroClient();
        const auto p = Player();

        if (!p.HasBuff("pickacard_tracker")) {
            if (m_R.IsReady() && (p.HasBuff("destiny_marker") || p.HasBuff("gate"))) {
                m_W.Cast();
            } else if (t.IsValid() && Combo()) {
                m_W.Cast();
            } else {
                // orbwalker target not available; fall back to hero-in-range for harass and minion-in-range for farm
                if (Harass() && GetBool("harassW")) {
                    for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                        if (enemy.IsValid() && enemy.IsEnemy() &&
                            SDK::Extensions::IsValidTarget(enemy, m_W.Range, true)) {
                            m_W.Cast();
                            break;
                        }
                    }
                } else if (LaneClear() && GetBool("farmW")) {
                    auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_W.Range);
                    if (!minions.empty()) m_W.Cast();
                }
            }
        } else {
            if (m_temp.empty()) m_temp = m_wName;
            else if (m_temp != m_wName) m_cardok = true;

            if (!m_cardok) return;

            // orb target as hero (approximated via TargetSelector)
            auto orbTarget = ts ? ts->GetTarget(m_W.Range, DamageType::Magical) : AIHeroClient();

            if (m_R.IsReady() && (p.HasBuff("destiny_marker") || p.HasBuff("gate"))) {
                m_findCard = 1;
                if (m_wName == "TwistedFate_Base_W_GoldCard.troy") m_W.Cast();
            } else if (Combo() && orbTarget.IsValid() &&
                       m_W.GetDamage(orbTarget) + p.GetAutoAttackDamage(orbTarget, false) > orbTarget.Health()) {
                m_W.Cast();
            } else if (p.Mana() < m_RMANA + m_QMANA + m_WMANA) {
                m_findCard = 2;
                if (m_wName == "TwistedFate_Base_W_BlueCard.troy") m_W.Cast();
            } else if (Harass() && orbTarget.IsValid()) {
                m_findCard = 1;
                if (m_wName == "TwistedFate_Base_W_BlueCard.troy") m_W.Cast();
            } else if (p.ManaPercent() > (float)GetSlider("WredFarm", 80) &&
                       FarmSpells() && GetBool("farmW")) {
                m_findCard = 3;
                if (m_wName == "TwistedFate_Base_W_RedCard.troy") m_W.Cast();
            } else if ((LaneClear() || p.Mana() < m_RMANA + m_QMANA) && GetBool("farmW")) {
                m_findCard = 2;
                if (m_wName == "TwistedFate_Base_W_BlueCard.troy") m_W.Cast();
            } else if (Combo()) {
                m_findCard = 1;
                if (m_wName == "TwistedFate_Base_W_GoldCard.troy") m_W.Cast();
            }
        }
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 700.0f, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_Q.IsReady() && GetBool("jungleQ")) {
            m_Q.Cast(mob.Position());
            return;
        }
    }

    void LogicR() {
        const auto p = Player();
        if (OktwCommon::CountEnemiesInRange(p.Position(), (float)GetSlider("Renemy", 1000)) != 0) return;

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Magical) : AIHeroClient();
        if (!t.IsValid()) return;
        if (t.Position().Distance(p.Position()) <= m_Q.Range) return;
        if (OktwCommon::CountAlliesInRange(t.Position(), (float)GetSlider("RenemyA", 800)) != 0) return;

        const float combo = m_Q.GetDamage(t) + m_W.GetDamage(t) + p.GetAutoAttackDamage(t, false) * 3.0f;
        if (combo > t.Health() && OktwCommon::CountEnemiesInRange(t.Position(), 1000.0f) < 3) {
            const auto pout = m_R.GetPrediction(t, true);
            // TODO(oktw-port): Player.UnderTurret / rPos.UnderTurret unavailable — skipping turret check
            m_R.Cast(pout.GetCastPosition());
        }
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (OktwCommon::GetKsDamage(t, m_Q) > t.Health() &&
                t.Position().Distance(p.Position()) > p.AttackRange() + p.BoundingRadius() + t.BoundingRadius()) {
                CastSpell(m_Q, t);
            }

            if (!p.HasBuff("pickacard_tracker")) {
                if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                    CastSpell(m_Q, t);
                }
                if (Harass() && p.Mana() > m_RMANA + m_QMANA + m_WMANA + m_EMANA &&
                    GetBool("harassQ") && OktwCommon::CanHarras() &&
                    GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                    CastSpell(m_Q, t);
                }
            }

            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (enemy.IsValid() && enemy.IsEnemy() &&
                    SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    m_Q.Cast(enemy);
                }
            }
        } else if (FarmSpells() && GetBool("farmQ")) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_Q.GetLineFarmLocation(baseList, m_Q.Width);
            if (farm.MinionsHit >= FarmMinions()) m_Q.Cast(farm.Position);
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
