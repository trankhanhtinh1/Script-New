#pragma once
// Port of OKTW_CSharp/Champions/Graves.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class GravesPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Graves"; }
    const char* GetInternalId() const override { return "champion.oktw.graves"; }
    const char* GetChampionName() const override { return "Graves"; }

protected:
    Spell m_R1{ SpellSlot::R };
    float m_overKill = 0.0f;
    bool  m_eSmart = false;

    void BuildMenu() override {
        MarkActive();

        m_Q  = Spell(SpellSlot::Q, 900.0f);
        m_W  = Spell(SpellSlot::W, 950.0f);
        m_E  = Spell(SpellSlot::E, 450.0f);
        m_R  = Spell(SpellSlot::R, 1000.0f);
        m_R1 = Spell(SpellSlot::R, 1700.0f);

        m_Q.SetSkillshot (0.25f, 100.0f, 2100.0f, false, SDK::SpellType::Line);
        m_W.SetSkillshot (0.25f, 120.0f, 1500.0f, false, SDK::SpellType::Circle);
        m_R.SetSkillshot (0.25f, 100.0f, 2100.0f, false, SDK::SpellType::Line);
        m_R1.SetSkillshot(0.25f, 100.0f, 2100.0f, false, SDK::SpellType::Line);

        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));

        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q",   true));
        m_qMenu->Add(new MenuBool("Qharras", "Harass Q", true));

        m_wMenu->Add(new MenuBool("autoW", "Auto W",           true));
        m_wMenu->Add(new MenuBool("AGCW",  "AntiGapcloser W",  true));

        m_eMenu->Add(new MenuBool("autoE", "Auto E", true));

        m_rMenu->Add(new MenuBool   ("autoR",     "Auto R",              true));
        m_rMenu->Add(new MenuBool   ("fastR",     "Fast R ks Combo",     true));
        m_rMenu->Add(new MenuBool   ("overkillR", "Overkill protection", false));
        m_rMenu->Add(new MenuKeyBind("useR",      "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));

        m_farmMenu->Add(new MenuBool("farmQ",    "Lane clear Q",   true));
        m_farmMenu->Add(new MenuBool("jungleQ",  "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW",  "Jungle clear W", true));
        m_farmMenu->Add(new MenuBool("jungleE",  "Jungle clear E", true));

        m_menu->Add(new MenuBool("QWlogic", "Use Q and W only if don't have ammo", false));
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
        if (GetKey("useR") && m_R.IsReady()) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(1800.0f, DamageType::Physical) : AIHeroClient();
            if (t.IsValid()) m_R1.Cast(t);
        }

        if (LagFree(0)) {
            SetMana();
            Jungle();
        }

        const auto player = Player();
        const bool qwGate = !GetBool("QWlogic") || !player.HasBuff("gravesbasicattackammo1");

        if (qwGate) {
            if (LagFree(2) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
            if (LagFree(3) && m_W.IsReady() && GetBool("autoW")) LogicW();
        }

        if (LagFree(4) && m_R.IsReady() && GetBool("autoR")) LogicR();
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto player = Player();
        auto mobs = OktwCommon::GetMinions(player.ServerPosition(), 600.0f, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(mob.Position()); return; }
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(mob.Position()); return; }
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Physical) : AIHeroClient();
        const auto player = Player();

        if (t.IsValid()) {
            if (Combo() && player.Mana() > m_RMANA + m_QMANA) {
                CastSpell(m_Q, t);
            } else if (Harass() && GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       player.Mana() > m_RMANA + m_EMANA + m_WMANA + m_QMANA + m_QMANA &&
                       GetBool("Qharras")) {
                CastSpell(m_Q, t);
            } else {
                const float qDmg = OktwCommon::GetKsDamage(t, m_Q);
                const float rDmg = m_R.GetDamage(t);
                if (qDmg > t.Health()) {
                    m_Q.Cast(t);
                    m_overKill = SDK::Game::Time();
                } else if (qDmg + rDmg > t.Health() && m_R.IsReady() &&
                           player.Mana() > m_RMANA + m_QMANA) {
                    CastSpell(m_Q, t);
                    if (GetBool("fastR") && rDmg < t.Health())
                        CastSpell(m_R, t);
                }
            }

            if (!None() && player.Mana() > m_RMANA + m_QMANA + m_EMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_Q.Cast(enemy);
                    }
                }
            }
        } else if (FarmSpells() && GetBool("farmQ")) {
            auto minions = OktwCommon::GetMinions(player.ServerPosition(), m_Q.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_Q.GetLineFarmLocation(baseList, m_Q.Width);
            if (farm.MinionsHit > 2) m_Q.Cast(farm.Position);
        }
    }

    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Magical) : AIHeroClient();
        const auto player = Player();
        if (!t.IsValid()) return;

        const float wDmg = OktwCommon::GetKsDamage(t, m_W);
        if (wDmg > t.Health()) {
            m_W.Cast(t);
            return;
        }
        if (wDmg + m_Q.GetDamage(t) > t.Health() &&
            player.Mana() > m_QMANA + m_WMANA + m_RMANA) {
            m_W.Cast(t);
        } else if (Combo() && player.Mana() > m_RMANA + m_WMANA + m_QMANA) {
            // Orbwalking.InAutoAttackRange not exposed - approximate via distance vs autoAttackRange
            const float aaRange = player.AttackRange() + player.BoundingRadius() + t.BoundingRadius();
            const bool outOfAaRange = player.Position().Distance(t.Position()) > aaRange;
            if (outOfAaRange ||
                OktwCommon::CountEnemiesInRange(player.Position(), 300.0f) > 0 ||
                OktwCommon::CountEnemiesInRange(t.Position(), 250.0f) > 1 ||
                player.HealthPercent() < 50.0f) {
                m_W.Cast(t);
            } else if (player.Mana() > m_RMANA + m_WMANA + m_QMANA + m_EMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (SDK::Extensions::IsValidTarget(enemy, m_W.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_W.Cast(enemy);
                    }
                }
            }
        }
    }

    void LogicE() {
        const auto player = Player();
        for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!target.IsValid() || !target.IsEnemy()) continue;
            if (SDK::Extensions::IsValidTarget(target, 270.0f, true) && target.IsMelee()) {
                // TODO(oktw-port): dash direction - fallback: cast away from enemy toward cursor
                const Vector3 dir = (SDK::Game::CursorPos() - player.Position());
                const float len = dir.Length();
                Vector3 dashPos = player.Position();
                if (len > 0.001f) dashPos = player.Position() + dir * (m_E.Range / len);
                m_E.Cast(dashPos);
                break;
            }
        }
        if (Combo() && player.Mana() > m_RMANA + m_EMANA && !player.HasBuff("gravesbasicattackammo2")) {
            const Vector3 dir = (SDK::Game::CursorPos() - player.Position());
            const float len = dir.Length();
            Vector3 dashPos = player.Position();
            if (len > 0.001f) dashPos = player.Position() + dir * (m_E.Range / len);
            m_E.Cast(dashPos);
        }
    }

    void LogicR() {
        const auto player = Player();
        for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!target.IsValid() || !target.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(target, m_R1.Range, true)) continue;
            if (!OktwCommon::ValidUlt(target)) continue;

            const float rDmg = OktwCommon::GetKsDamage(target, m_R);
            if (rDmg < target.Health()) continue;

            if (GetBool("overkillR") && target.Health() < player.Health()) {
                const float aaRange = player.AttackRange() + player.BoundingRadius() + target.BoundingRadius();
                if (player.Position().Distance(target.Position()) <= aaRange) continue;
                if (OktwCommon::CountAlliesInRange(target.Position(), 400.0f) > 0) continue;
            }

            const float rDmg2 = rDmg * 0.8f;

            if (SDK::Extensions::IsValidTarget(target, m_R.Range, true) && rDmg > target.Health()) {
                CastSpell(m_R, target);
            } else if (rDmg2 > target.Health()) {
                if (SDK::Extensions::IsValidTarget(target, 1200.0f, true)) {
                    CastSpell(m_R1, target);
                }
            }
        }
    }

    // TODO(oktw-port): wire when Orbwalking.AfterAttack event exposed
    void OnAfterAttack(const AIHeroClient& /*target*/) {
        if (m_E.IsReady() && GetBool("autoE")) LogicE();
        // Jungle E on-attack path deferred
    }

    // TODO(oktw-port): wire when AntiGapcloser event exposed
    void OnEnemyGapcloser(const AIHeroClient& sender, const Vector3& endPos) {
        const auto player = Player();
        if (player.Mana() <= m_RMANA + m_EMANA) return;
        if (!SDK::Extensions::IsValidTarget(sender, m_E.Range, true)) return;
        if (m_W.IsReady() && GetBool("AGCW")) m_W.Cast(endPos);
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
