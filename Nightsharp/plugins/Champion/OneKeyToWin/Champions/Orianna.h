#pragma once
// Port of OKTW_CSharp/Champions/Orianna.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class OriannaPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Orianna"; }
    const char* GetInternalId() const override { return "champion.oktw.orianna"; }
    const char* GetChampionName() const override { return "Orianna"; }

protected:
    Spell m_QR{ SpellSlot::Q, 825.0f };  // Command: Attack cast from ball (long-range aim)
    float m_RCastTime = 0.0f;
    Vector3 m_BallPos{};
    int m_FarmId = 0;
    bool m_Rsmart = false;
    AIHeroClient m_best;

    void BuildMenu() override {
        MarkActive();

        m_Q  = Spell(SpellSlot::Q, 800.0f);
        m_W  = Spell(SpellSlot::W, 210.0f);
        m_E  = Spell(SpellSlot::E, 1095.0f);
        m_R  = Spell(SpellSlot::R, 360.0f);
        m_QR = Spell(SpellSlot::Q, 825.0f);

        m_Q.SetSkillshot(0.05f, 70.0f, 1150.0f, false, SDK::SpellType::Circle);
        m_W.SetSkillshot(0.25f, 210.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_E.SetSkillshot(0.25f, 100.0f, 1700.0f, false, SDK::SpellType::Line);
        m_R.SetSkillshot(0.4f, 370.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_QR.SetSkillshot(0.5f, 400.0f, 100.0f, false, SDK::SpellType::Circle);

        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));

        // "E Shield Config" — attached under the champion menu (C# uses SubMenu with that title)
        Menu* eShield = m_champMenu->AddSubMenu(new Menu("EShieldSub", "E Shield Config"));
        eShield->Add(new MenuBool("autoW",  "Auto E", true));
        eShield->Add(new MenuBool("hadrCC", "Auto E hard CC", true));
        eShield->Add(new MenuBool("poison", "Auto E poison", true));
        eShield->Add(new MenuSlider("Wdmg", "E dmg % hp", 10, 0, 100));
        eShield->Add(new MenuBool("AGC",   "AntiGapcloserE", true));

        m_farmMenu->Add(new MenuBool("farmQout", "Farm Q out range aa minion", true));
        m_farmMenu->Add(new MenuBool("farmQ",    "LaneClear Q", true));
        m_farmMenu->Add(new MenuBool("farmW",    "LaneClear W", true));
        m_farmMenu->Add(new MenuBool("farmE",    "LaneClear E", false));

        m_rMenu->Add(new MenuSlider("rCount",     "Auto R x enemies", 3, 0, 5));
        m_rMenu->Add(new MenuKeyBind("smartR",    "Semi-manual cast R key", 'T', SDK::UI::KeyBindType::Press));
        m_rMenu->Add(new MenuBool("OPTI",         "OnPossibleToInterrupt R", true));
        m_rMenu->Add(new MenuBool("Rturrent",     "auto R under turrent", true));
        m_rMenu->Add(new MenuBool("Rks",          "R ks", true));
        m_rMenu->Add(new MenuBool("Rlifesaver",   "auto R life saver", true));
        m_rMenu->Add(new MenuBool("Rblock",       "Block R if 0 hit", true));

        Menu* ralways = m_rMenu->AddSubMenu(new Menu("AlwaysR", "Always R"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("Ralways") + enemy.CharacterName();
            ralways->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), false));
        }

        m_champMenu->Add(new MenuBool("W", "Auto W SpeedUp logic", false));

        // TODO(oktw-port): GameObject.OnCreate — track "TheDoomBall" position
        // TODO(oktw-port): Obj_AI_Base.OnProcessSpellCast — track OrianaIzunaCommand end + shield allies from incoming spell damage
        // TODO(oktw-port): AntiGapcloser.OnEnemyGapcloser — E self on gapcloser
        // TODO(oktw-port): Interrupter2.OnInterruptableTarget — R/Q vs interruptible target
        // TODO(oktw-port): Spellbook.OnCastSpell — block R if Rblock and 0 hits at ball
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
        const auto p = Player();
        if (p.HasBuff("Recall") || p.IsDead()) return;

        if (m_R.IsReady()) LogicR();

        bool hadrCC = true, poison = true;
        if (LagFree(0)) {
            SetMana();
            hadrCC = GetBool("hadrCC");
            poison = GetBool("poison");
        }

        m_best = p;

        for (const auto& ally : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!ally.IsValid() || ally.IsDead() || ally.IsEnemy()) continue;

            if (ally.HasBuff("orianaghostself") || ally.HasBuff("orianaghost"))
                m_BallPos = ally.ServerPosition();

            if (LagFree(3)) {
                if (m_E.IsReady() && p.Mana() > m_RMANA + m_EMANA &&
                    ally.Position().Distance(p.Position()) < m_E.Range) {
                    const int countEnemy = OktwCommon::CountEnemiesInRange(ally.Position(), 800.0f);
                    if (ally.Health() < countEnemy * ally.Level() * 25.0f) {
                        m_E.Cast(ally);
                    } else if (HardCC(ally) && hadrCC && countEnemy > 0) {
                        m_E.Cast(ally);
                    } else if (ally.HasBuff("Poison")) {  // TODO(oktw-port): BuffType::Poison unavailable; string-buff fallback
                        m_E.Cast(ally);
                    }
                }
                if (m_W.IsReady() && p.Mana() > m_RMANA + m_WMANA &&
                    m_BallPos.Distance(ally.ServerPosition()) < 240.0f &&
                    ally.Health() < OktwCommon::CountEnemiesInRange(ally.Position(), 600.0f) * ally.Level() * 20.0f) {
                    m_W.Cast();
                }

                if ((ally.Health() < m_best.Health() ||
                     OktwCommon::CountEnemiesInRange(ally.Position(), 300.0f) > 0) &&
                    ally.Position().Distance(p.Position()) < m_E.Range &&
                    OktwCommon::CountEnemiesInRange(ally.Position(), 700.0f) > 0) {
                    m_best = ally;
                }
            }
            if (LagFree(1) && m_E.IsReady() && p.Mana() > m_RMANA + m_EMANA &&
                ally.Position().Distance(p.Position()) < m_E.Range) {
                const int rCount = GetSlider("rCount", 3);
                const int hits = OktwCommon::CountEnemiesInRange(ally.Position(), m_R.Width);
                if (rCount != 0 && hits >= rCount) {
                    m_E.Cast(ally);
                }
            }
        }

        // Semi-manual R: hold key OR keep firing until target list is empty
        const bool smartRHeld = GetKey("smartR");
        if ((smartRHeld || m_Rsmart) && m_R.IsReady()) {
            m_Rsmart = true;
            auto* ts = SDK::TargetSelector::Instance();
            auto target = ts ? ts->GetTarget(m_Q.Range + 100.0f, DamageType::Magical) : AIHeroClient();
            if (SDK::Extensions::IsValidTarget(target, m_Q.Range + 100.0f, true)) {
                if (CountEnemiesInRangeDelay(m_BallPos, m_R.Width, m_R.Delay) > 1) {
                    m_R.Cast();
                } else if (m_Q.IsReady()) {
                    // C#: QR.Cast(target, true, true) — force cast on ball-anchored spell
                    m_QR.Cast(target);
                } else if (CountEnemiesInRangeDelay(m_BallPos, m_R.Width, m_R.Delay) > 0) {
                    m_R.Cast();
                }
            } else {
                m_Rsmart = false;
            }
        } else {
            m_Rsmart = false;
        }

        if (LagFree(1)) {
            LogicQ();
            LogicFarm();
        }

        if (LagFree(2) && m_W.IsReady()) LogicW();

        if (LagFree(4) && m_E.IsReady()) LogicE(m_best);
    }

    void LogicE(const AIHeroClient& best) {
        auto* ts = SDK::TargetSelector::Instance();
        auto ta = ts ? ts->GetTarget(1300.0f, DamageType::Magical) : AIHeroClient();
        const auto p = Player();

        if (Combo() && ta.IsValid() && !m_W.IsReady() && p.Mana() > m_RMANA + m_EMANA) {
            if (CountEnemiesInRangeDelay(m_BallPos, 100.0f, 0.1f) > 0) {
                m_E.Cast(best);
            }
            const Vector3 dir = (best.ServerPosition() - ta.ServerPosition());
            const float len = dir.Length();
            Vector3 castArea = ta.ServerPosition();
            if (len > 0.001f) {
                const Vector3 norm = dir * (1.0f / len);
                castArea = norm * ta.Position().Distance(best.ServerPosition()) + ta.ServerPosition();
            }
            if (castArea.Distance(ta.ServerPosition()) < ta.BoundingRadius() / 2.0f) {
                m_E.Cast(best);
            }
        }
    }

    void LogicR() {
        const auto p = Player();
        for (const auto& t : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!t.IsValid() || !t.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(t, FLT_MAX, true)) continue;

            const auto tPred = m_R.GetPrediction(t, false);
            if (m_BallPos.Distance(tPred.GetCastPosition()) >= m_R.Width) continue;
            if (m_BallPos.Distance(t.ServerPosition()) >= m_R.Width) continue;

            if (Combo() && GetBool((std::string("Ralways") + t.CharacterName()).c_str())) {
                m_R.Cast();
            }

            if (GetBool("Rks")) {
                float comboDmg = OktwCommon::GetKsDamage(t, m_R);
                if (SDK::Extensions::IsValidTarget(t, m_Q.Range, true))
                    comboDmg += m_Q.GetDamage(t);
                if (m_W.IsReady())
                    comboDmg += m_W.GetDamage(t);
                if (SDK::Core::Utils::AutoAttack::InAutoAttackRange(t))
                    comboDmg += p.GetAutoAttackDamage(t, false) * 2.0f;
                if (t.Health() < comboDmg) m_R.Cast();
            }

            // TODO(oktw-port): Vector3::UnderTurret unavailable — turret-based R skipped.
            if (GetBool("Rturrent") && false) {
                m_R.Cast();
            }
            if (GetBool("Rlifesaver") &&
                p.Health() < p.CountEnemyHeroesInRange(800.0f) * p.Level() * 20.0f &&
                p.Position().Distance(m_BallPos) > t.Position().Distance(p.Position())) {
                m_R.Cast();
            }
        }

        const int countEnemies = CountEnemiesInRangeDelay(m_BallPos, m_R.Width, m_R.Delay);
        const int rCount = GetSlider("rCount", 3);

        if (countEnemies >= rCount &&
            OktwCommon::CountEnemiesInRange(m_BallPos, m_R.Width) == countEnemies) {
            m_R.Cast();
        }
    }

    void LogicW() {
        const auto p = Player();
        for (const auto& t : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!t.IsValid() || !t.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(t, FLT_MAX, true)) continue;
            if (m_BallPos.Distance(t.ServerPosition()) >= 250.0f) continue;
            if (t.Health() < m_W.GetDamage(t)) {
                m_W.Cast();
                return;
            }
        }
        if (CountEnemiesInRangeDelay(m_BallPos, m_W.Width, 0.0f) > 0 &&
            p.Mana() > m_RMANA + m_WMANA) {
            m_W.Cast();
            return;
        }
        if (GetBool("W") && !Harass() && !Combo() &&
            p.Mana() > p.MaxMana() * 0.95f &&
            p.HasBuff("orianaghostself")) {
            m_W.Cast();
        }
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid() && m_Q.IsReady()) {
            if (m_Q.GetDamage(t) + m_W.GetDamage(t) > t.Health()) {
                CastQ(t);
            } else if (Combo() && p.Mana() > m_RMANA + m_QMANA - 10.0f) {
                CastQ(t);
            } else if (Harass() && p.Mana() > m_RMANA + m_QMANA + m_WMANA + m_EMANA &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                CastQ(t);
            }
        }
        // Auto-W/E to self while ulting (build stack toward opponent)
        if (GetBool("W") && !t.IsValid() && Combo() &&
            p.Mana() > m_RMANA + 3.0f * m_QMANA + m_WMANA + m_EMANA + m_WMANA) {
            if (m_W.IsReady() && p.HasBuff("orianaghostself")) {
                m_W.Cast();
            } else if (m_E.IsReady() && !p.HasBuff("orianaghostself")) {
                m_E.Cast(p);
            }
        }
    }

    void LogicFarm() {
        if (!Harass()) return;
        const auto p = Player();

        auto allMinionsVec = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range);
        if (GetBool("farmQout") && p.Mana() > m_RMANA + m_QMANA + m_WMANA + m_EMANA) {
            for (const auto& minion : allMinionsVec) {
                if (SDK::Extensions::IsValidTarget(minion, m_Q.Range, true) &&
                    !SDK::Core::Utils::AutoAttack::InAutoAttackRange(minion) &&
                    minion.Health() < m_Q.GetDamage(minion) &&
                    minion.Health() > minion.BonusAttackDamage()) {
                    m_Q.Cast(minion);
                }
            }
        }

        if (!LaneClear() || p.Mana() < m_RMANA + m_QMANA) return;

        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 800.0f, false, true);
        if (!mobs.empty()) {
            const auto& mob = mobs.front();
            if (m_Q.IsReady()) m_Q.Cast(mob.Position());
            if (m_W.IsReady() && m_BallPos.Distance(mob.Position()) < m_W.Width) {
                m_W.Cast();
            } else if (m_E.IsReady()) {
                m_E.Cast(m_best);
            }
            return;
        }

        // TODO(oktw-port): under-own-turret query unavailable; only IsUnderEnemyTurret exists.
        if (FarmSpells() || (false && !p.IsUnderEnemyTurret())) {
            std::vector<SDK::AIBaseClient> baseList(allMinionsVec.begin(), allMinionsVec.end());
            auto Qfarm  = m_Q.GetCircularFarmLocation(baseList, 100.0f);
            auto QWfarm = m_Q.GetCircularFarmLocation(baseList, m_W.Width);

            if (Qfarm.MinionsHit + QWfarm.MinionsHit == 0) return;
            if (GetBool("farmQ")) {
                if (Qfarm.MinionsHit >= FarmMinions() && !m_W.IsReady() && m_Q.IsReady()) {
                    m_Q.Cast(Qfarm.Position);
                } else if (QWfarm.MinionsHit > 2 && m_Q.IsReady()) {
                    m_Q.Cast(QWfarm.Position);
                }
            }

            for (const auto& minion : allMinionsVec) {
                if (m_W.IsReady() && minion.Position().Distance(m_BallPos) < m_W.Range &&
                    minion.Health() < m_W.GetDamage(minion) && GetBool("farmW")) {
                    m_W.Cast();
                }
                if (!m_W.IsReady() && m_E.IsReady() &&
                    minion.Position().Distance(m_BallPos) < m_E.Width &&
                    GetBool("farmE")) {
                    m_E.Cast(p);
                }
            }
        }
    }

    void CastQ(const AIHeroClient& target) {
        const auto p = Player();
        const float distance = m_BallPos.Distance(target.ServerPosition());

        if (m_E.IsReady() && p.Mana() > m_RMANA + m_QMANA + m_WMANA + m_EMANA &&
            distance > p.Position().Distance(target.ServerPosition()) + 300.0f) {
            m_E.Cast(p);
            return;
        }

        // Simplified: single prediction path via SDK (Qpred branch collapsed like OKTWBase::CastSpell)
        const float delay = (distance / m_Q.Speed) + m_Q.Delay;
        const auto prepos = m_Q.GetPrediction(target, false);
        const int qHit = Shared().qHit ? Shared().qHit->Index : 1;
        // C#: (int)prepos.Hitchance > 5 - SelectedIndex — enum-order specific; approximate with HitFromIndex table
        const HitChance need = HitFromIndex(qHit);
        if (static_cast<int>(prepos.Hitchance) >= static_cast<int>(need)) {
            if (prepos.GetCastPosition().Distance(prepos.GetCastPosition()) < m_Q.Range) {
                m_Q.Cast(prepos.GetCastPosition());
            }
        }
        (void)delay;  // reserved for legacy Prediction.GetPrediction(target, delay, Q.Width)
    }

    int CountEnemiesInRangeDelay(const Vector3& position, float range, float delay) const {
        int count = 0;
        for (const auto& t : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!t.IsValid() || !t.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(t, FLT_MAX, true)) continue;
            const auto pred = m_R.GetPrediction(t, false);
            (void)delay;  // per-call prediction delay is embedded in Spell config
            if (position.Distance(pred.GetCastPosition()) < range) ++count;
        }
        return count;
    }

    bool HardCC(const AIHeroClient& target) const {
        using namespace SDK::Prediction;
        return OktwCommon::HasBuffOfType(target, BuffType::Stun) ||
               OktwCommon::HasBuffOfType(target, BuffType::Snare) ||
               OktwCommon::HasBuffOfType(target, BuffType::Knockup) ||
               OktwCommon::HasBuffOfType(target, BuffType::Charm) ||
               OktwCommon::HasBuffOfType(target, BuffType::Fear) ||
               OktwCommon::HasBuffOfType(target, BuffType::Knockback) ||
               OktwCommon::HasBuffOfType(target, BuffType::Taunt) ||
               OktwCommon::HasBuffOfType(target, BuffType::Suppression) ||
               Plugins::OKTW::OktwCommon::HasBuffOfType(target, SDK::Prediction::BuffType::Stun);
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
