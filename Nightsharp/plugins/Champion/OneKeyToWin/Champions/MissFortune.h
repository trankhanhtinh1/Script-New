#pragma once
// Port of OKTW_CSharp/Champions/MissFortune.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class MissFortunePlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW MissFortune"; }
    const char* GetInternalId() const override { return "champion.oktw.missfortune"; }
    const char* GetChampionName() const override { return "MissFortune"; }

protected:
    Spell m_Q1{ SpellSlot::Q };
    int   m_lastAttackId = 0;
    float m_rCastTime    = 0.0f;

    void BuildMenu() override {
        MarkActive();

        m_Q  = Spell(SpellSlot::Q, 655.0f);
        m_Q1 = Spell(SpellSlot::Q, 1300.0f);
        m_W  = Spell(SpellSlot::W);
        m_E  = Spell(SpellSlot::E, 1000.0f);
        m_R  = Spell(SpellSlot::R, 1350.0f);

        m_Q1.SetSkillshot(0.25f, 70.0f,  1500.0f, true,  SDK::SpellType::Line);
        m_Q.SetTargetted (0.25f, 1400.0f);
        m_E.SetSkillshot (0.50f, 200.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_R.SetSkillshot (0.25f, 50.0f,  3000.0f, false, SDK::SpellType::Circle);

        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells",   true));
        m_drawMenu->Add(new MenuBool("QRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("ERange",  "E range", false));
        m_drawMenu->Add(new MenuBool("RRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("noti",    "Show notification & line", true));

        m_qMenu->Add(new MenuBool("autoQ", "Auto Q", true));
        Menu* qmin = m_qMenu->AddSubMenu(new Menu("QMinionSub", "Minion config"));
        qmin->Add(new MenuBool  ("harassQ",      "Use Q on minion",              true));
        qmin->Add(new MenuBool  ("killQ",        "Use Q only if can kill minion",false));
        qmin->Add(new MenuBool  ("qMinionMove",  "Don't use if minions moving",  true));
        qmin->Add(new MenuSlider("qMinionWidth", "secound Q angle",              80, 0, 100));

        m_wMenu->Add(new MenuBool("autoW",   "Auto W",   true));
        m_wMenu->Add(new MenuBool("harassW", "Harass W", true));

        m_eMenu->Add(new MenuBool("autoE", "Auto E",         true));
        m_eMenu->Add(new MenuBool("AGC",   "AntiGapcloserE", true));

        m_rMenu->Add(new MenuBool   ("autoR",          "Auto R", true));
        m_rMenu->Add(new MenuBool   ("forceBlockMove", "Force block player", true));
        m_rMenu->Add(new MenuKeyBind("useR",           "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));
        m_rMenu->Add(new MenuKeyBind("disableBlock",   "Disable R key", 'R', SDK::KeyBindType::Press));
        m_rMenu->Add(new MenuBool   ("Rturrent",       "Don't R under turret", true));

        Shared_Add_NewTarget();

        m_farmMenu->Add(new MenuBool("farmQ",   "LaneClear Q",    true));
        m_farmMenu->Add(new MenuBool("farmW",   "LaneClear W",    true));
        m_farmMenu->Add(new MenuBool("farmE",   "LaneClear E",    true));
        m_farmMenu->Add(new MenuBool("jungleQ", "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW", "Jungle clear W", true));
        m_farmMenu->Add(new MenuBool("jungleE", "Jungle clear E", true));
    }

    // Adds the "newTarget" toggle to the champion root menu -- kept separate so BuildMenu stays tidy.
    void Shared_Add_NewTarget() {
        if (m_champMenu)
            m_champMenu->Add(new MenuBool("newTarget", "Try change focus after attack ", true));
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

        if (GetKey("disableBlock")) {
            // TODO(oktw-port): Orbwalker Attack/Move and blockSpells toggles unavailable
            return;
        }

        if (m_R.IsReady() && GetKey("useR")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Physical) : AIHeroClient();
            if (t.IsValid() && SDK::Extensions::IsValidTarget(t, m_R.Range, true)) {
                m_R.Cast(t);
                m_rCastTime = static_cast<float>(SDK::Game::Time());
                return;
            }
        }

        // TODO(oktw-port): newTarget refocus (Orbwalker.ForceTarget) unavailable
        // TODO(oktw-port): Orbwalking.AfterAttack callback unavailable -- Q/W afterAttack logic dropped
        // TODO(oktw-port): OnProcessSpellCast MissFortuneBulletTime block unavailable

        if (LagFree(1)) { SetMana(); Jungle(); }

        if (LagFree(2) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(3) && m_E.IsReady() && GetBool("autoE")) LogicE();
        if (LagFree(4) && m_R.IsReady() && GetBool("autoR")) LogicR();
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t  = ts ? ts->GetTarget(m_Q.Range,  DamageType::Physical) : AIHeroClient();
        auto t1 = ts ? ts->GetTarget(m_Q1.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid() && SDK::Extensions::IsValidTarget(t, m_Q.Range, true) &&
            p.Position().Distance(t.ServerPosition()) > 500.0f) {
            const float qDmg = OktwCommon::GetKsDamage(t, m_Q);
            const float aaDmg = p.GetAutoAttackDamage(t, false);
            if (qDmg + aaDmg > t.Health()) m_Q.Cast(t);
            else if (qDmg + aaDmg * 3.0f > t.Health()) m_Q.Cast(t);
            else if (Combo() && p.Mana() > m_RMANA + m_QMANA + m_WMANA) m_Q.Cast(t);
            else if (Harass() && p.Mana() > m_RMANA + m_QMANA + m_EMANA + m_WMANA &&
                     GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                m_Q.Cast(t);
            }
        } else if (t1.IsValid() && SDK::Extensions::IsValidTarget(t1, m_Q1.Range, true) &&
                   GetBool("harassQ") &&
                   p.Position().Distance(t1.ServerPosition()) > m_Q.Range + 50.0f) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q1.Range);

            if (GetBool("qMinionMove")) {
                for (const auto& x : minions) {
                    if (x.IsMoving()) return;
                }
            }

            const auto pr = m_Q1.GetPrediction(t1, false);
            const Vector3 enemyPredictionPos = pr.GetCastPosition();
            const int angleWidth = GetSlider("qMinionWidth", 80);

            for (const auto& minion : minions) {
                if (GetBool("killQ") && m_Q.GetDamage(minion) < minion.Health()) continue;

                const Vector3 dir = (minion.ServerPosition() - p.ServerPosition());
                const float len = dir.Length();
                if (len <= 0.001f) continue;
                const Vector3 posExt = p.ServerPosition() +
                    dir * ((420.0f + p.Position().Distance(minion.Position())) / len);

                if (InCone(enemyPredictionPos, posExt, minion.ServerPosition(), angleWidth)) {
                    bool blockedByOther = false;
                    for (const auto& x : minions) {
                        if (x.NetworkId() == minion.NetworkId()) continue;
                        if (InCone(x.Position(), posExt, minion.ServerPosition(), angleWidth)) {
                            blockedByOther = true;
                            break;
                        }
                    }
                    if (blockedByOther) continue;
                    m_Q.Cast(minion);
                    return;
                }
            }
        }
    }

    static bool InCone(const Vector3& position, const Vector3& finishPos,
                       const Vector3& firstPos, int angleSet) {
        constexpr float range = 420.0f;
        const float angle = angleSet * 3.14159265f / 180.0f;

        // 2D vectors on the XZ plane (League convention)
        auto to2 = [](const Vector3& v){ return std::pair<float,float>{ v.x, v.z }; };
        auto rot = [](std::pair<float,float> v, float a) {
            const float c = std::cos(a), s = std::sin(a);
            return std::pair<float,float>{ v.first * c - v.second * s,
                                           v.first * s + v.second * c };
        };
        auto cross = [](std::pair<float,float> a, std::pair<float,float> b){
            return a.first * b.second - a.second * b.first;
        };

        const auto fp = to2(firstPos);
        const auto fe = to2(finishPos);
        const auto pp = to2(position);
        const std::pair<float,float> end2{ fe.first - fp.first, fe.second - fp.second };
        const auto edge1 = rot(end2, -angle / 2.0f);
        const auto edge2 = rot(edge1, angle);
        const std::pair<float,float> point{ pp.first - fp.first, pp.second - fp.second };

        const float d2 = point.first * point.first + point.second * point.second;
        return d2 < range * range && cross(edge1, point) > 0.0f && cross(point, edge2) > 0.0f;
    }

    void LogicE() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid()) {
            const float eDmg = OktwCommon::GetKsDamage(t, m_E);
            if (eDmg > t.Health()) {
                CastSpell(m_E, t);
            } else if (eDmg + m_Q.GetDamage(t) > t.Health() &&
                       p.Mana() > m_QMANA + m_EMANA + m_RMANA) {
                CastSpell(m_E, t);
            } else if (Combo() && p.Mana() > m_RMANA + m_WMANA + m_QMANA + m_EMANA) {
                const bool tooFarOrCrowded =
                    p.Position().Distance(t.Position()) > 550.0f ||
                    OktwCommon::CountEnemiesInRange(p.Position(), 300.0f) > 0 ||
                    OktwCommon::CountEnemiesInRange(t.Position(), 250.0f) > 1;
                if (tooFarOrCrowded) {
                    CastSpell(m_E, t);
                } else {
                    for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                        if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                        if (SDK::Extensions::IsValidTarget(enemy, m_E.Range, true) &&
                            !OktwCommon::CanMove(enemy)) {
                            m_E.Cast(enemy.Position());
                        }
                    }
                }
            }
        }
        if (FarmSpells() && GetBool("farmE")) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_E.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_E.GetCircularFarmLocation(baseList, m_E.Width);
            if (farm.MinionsHit >= FarmMinions()) m_E.Cast(farm.Position);
        }
    }

    void LogicR() {
        // TODO(oktw-port): UnderTurret check unavailable
        const auto p = Player();
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Physical) : AIHeroClient();

        if (!t.IsValid() || !SDK::Extensions::IsValidTarget(t, m_R.Range, true)) return;
        if (!OktwCommon::ValidUlt(t)) return;

        const int rLevel = m_R.Level();
        const float mult = (rLevel <= 1) ? 0.5f : (rLevel == 2 ? 0.75f : 1.0f);
        const float rDmg = m_R.GetDamage(t) * mult;

        if (OktwCommon::CountEnemiesInRange(p.Position(), 700.0f) == 0 &&
            OktwCommon::CountAlliesInRange(t.Position(), 400.0f) == 0) {
            const float tDis = p.Position().Distance(t.ServerPosition());
            if      (rDmg * 7.0f > t.Health() && tDis < 800.0f)  { m_R.Cast(t); m_rCastTime = static_cast<float>(SDK::Game::Time()); }
            else if (rDmg * 6.0f > t.Health() && tDis < 900.0f)  { m_R.Cast(t); m_rCastTime = static_cast<float>(SDK::Game::Time()); }
            else if (rDmg * 5.0f > t.Health() && tDis < 1000.0f) { m_R.Cast(t); m_rCastTime = static_cast<float>(SDK::Game::Time()); }
            else if (rDmg * 4.0f > t.Health() && tDis < 1100.0f) { m_R.Cast(t); m_rCastTime = static_cast<float>(SDK::Game::Time()); }
            else if (rDmg * 3.0f > t.Health() && tDis < 1200.0f) { m_R.Cast(t); m_rCastTime = static_cast<float>(SDK::Game::Time()); }
            else if (rDmg        > t.Health() && tDis < 1300.0f) { m_R.Cast(t); m_rCastTime = static_cast<float>(SDK::Game::Time()); }
            return;
        }

        if (rDmg * 8.0f > t.Health() - OktwCommon::GetIncomingDamage(t) &&
            rDmg * 2.0f < t.Health() &&
            OktwCommon::CountEnemiesInRange(p.Position(), 300.0f) == 0 &&
            !OktwCommon::CanMove(t)) {
            m_R.Cast(t);
            m_rCastTime = static_cast<float>(SDK::Game::Time());
        }
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_QMANA) return;
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 600.0f, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(mob); return; }
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(); return; }
        if (m_E.IsReady() && GetBool("jungleE")) { m_E.Cast(mob.ServerPosition()); return; }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
