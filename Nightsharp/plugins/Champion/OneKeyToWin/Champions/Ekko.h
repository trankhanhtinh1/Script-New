#pragma once
// Port of OKTW_CSharp/Champions/Ekko.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;

class EkkoPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Ekko"; }
    const char* GetInternalId() const override { return "champion.oktw.ekko"; }
    const char* GetChampionName() const override { return "Ekko"; }

protected:
    // Second Q spell — targeted return / longer range circle variant (C# had Q1)
    Spell m_Q1{ SpellSlot::Q };

    // TODO(SDK): The C# port tracks 3 in-game objects created by name via
    // GameObject.OnCreate: the returning Q missile "Ekko" ally clone (RMissile),
    // and the W indicator/cast troys ("Ekko_Base_W_Indicator.troy",
    // "Ekko_Base_W_Cas.troy"). NightSharp SDK exposes OnCreateObject but the
    // OKTW port does not yet wire per-plugin lifetimes to it. Left as static
    // trackers with a small hook installed in OnLoad/OnUnload below.
    static inline SDK::GameObject s_RMissile{};
    static inline SDK::GameObject s_WMissile{};
    static inline SDK::GameObject s_WMissile2{};
    static inline float s_Wtime  = 0.0f;
    static inline float s_Wtime2 = 0.0f;

    // TODO(SDK): Core.MissileReturn("ekkoqmis", "ekkoqreturn", Q) — Q return-missile
    // manager is a SebbyLib helper not present in NightSharp SDK yet. The C#
    // logic sets missileManager.Target for redirection; safe to omit here since
    // no other logic reads it back.

    void BuildMenu() override {
        MarkActive();

        m_Q  = Spell(SpellSlot::Q, 750.0f);
        m_Q1 = Spell(SpellSlot::Q, 1000.0f);
        m_W  = Spell(SpellSlot::W, 1620.0f);
        m_E  = Spell(SpellSlot::E, 330.0f);
        m_R  = Spell(SpellSlot::R, 280.0f);

        m_Q.SetSkillshot (0.25f, 60.0f,  1650.0f, false, SDK::SpellType::Line);
        m_Q1.SetSkillshot(0.50f, 150.0f, 1000.0f, false, SDK::SpellType::Circle);
        m_W.SetSkillshot (2.50f, 200.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_R.SetSkillshot (0.40f, 280.0f, FLT_MAX, false, SDK::SpellType::Circle);

        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("Qhelp",   "Show Q,W helper", true));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));

        m_wMenu->Add(new MenuBool("autoW", "Auto W", true));
        m_wMenu->Add(new MenuBool("Waoe",  "Cast if 2 targets", false));

        m_rMenu->Add(new MenuBool  ("autoR",  "Auto R", true));
        m_rMenu->Add(new MenuSlider("rCount", "Auto R if enemies in range", 3, 0, 5));

        m_farmMenu->Add(new MenuBool("farmQ",   "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("farmW",   "Farm W", true));
        m_farmMenu->Add(new MenuBool("jungleQ", "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW", "Jungle clear W", true));

        // Install a single OnCreateObject handler for name-based tracking
        SDK::Events::AddOnCreateObject(&EkkoPlugin::StaticOnObjectCreate);
    }

    static void StaticOnObjectCreate(const SDK::Events::ObjectEventArgs& args) {
        // Mirrors C# Obj_AI_Base_OnCreate: track Ekko clone + W indicator troys
        if (!args.Sender.IsValid()) return;
        SDK::AIBaseClient obj(args.Sender.Ptr);
        if (!obj.IsValid()) return;
        const std::string name = obj.Name();
        if (name == "Ekko" && obj.IsAlly()) {
            s_RMissile = obj;
        } else if (name == "Ekko_Base_W_Indicator.troy") {
            s_WMissile = obj;
            s_Wtime    = SDK::Game::Time();
        } else if (name == "Ekko_Base_W_Cas.troy") {
            s_WMissile2 = obj;
            s_Wtime2    = SDK::Game::Time();
        }
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
            // Approximate C#: QMANA - Player.PARRegenRate * Q.Cooldown.
            // TODO(SDK): PARRegenRate not exposed; fall back to QMANA.
            m_RMANA = m_QMANA;
        } else {
            m_RMANA = m_R.Instance().ManaCost();
        }
    }

    void OnGameUpdate() override {
        if (LagFree(0)) { SetMana(); Jungle(); }

        if (LagFree(1) && m_Q.IsReady()) LogicQ();
        if (LagFree(2) && m_W.IsReady() && GetBool("autoW") &&
            Player().Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA) {
            LogicW();
        }
        if (LagFree(3) && m_E.IsReady()) LogicE();
        if (m_R.IsReady()) LogicR();
    }

    // ── R logic (Chronobreak) ───────────────────────────────────────────────
    void LogicR() {
        if (!GetBool("autoR")) return;

        const auto p = Player();

        if (LagFree(4) && Combo() && s_RMissile.IsValid()) {
            const Vector3 rPos = s_RMissile.Position();
            const int rCount = GetSlider("rCount", 3);

            if (rCount > 0 &&
                OktwCommon::CountEnemiesInRange(rPos, m_R.Range) >= rCount) {
                m_R.Cast();
            }

            for (const auto& t : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!t.IsValid() || !t.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(t, FLT_MAX, true)) continue;

                const auto pred = SDK::Prediction::GetPrediction(t, m_R.Delay);
                if (rPos.Distance(pred.GetCastPosition()) >= m_R.Range) continue;
                if (rPos.Distance(t.ServerPosition()) >= m_R.Range) continue;

                float comboDMG = OktwCommon::GetKsDamage(t, m_R);
                if (m_Q.IsReady()) comboDMG += m_Q.GetDamage(t);
                if (m_E.IsReady()) comboDMG += m_E.GetDamage(t);
                if (m_W.IsReady()) comboDMG += m_W.GetDamage(t);

                if (t.Health() < comboDMG && OktwCommon::ValidUlt(t)) {
                    m_R.Cast();
                }
                NightSharpDebug::Logf("[OKTW/Ekko] ks");
            }
        }

        const float dmg = OktwCommon::GetIncomingDamage(p);
        if (dmg > 0.0f) {
            if (p.Health() - dmg < static_cast<float>(p.Level()) * 10.0f) {
                m_R.Cast();
            }
        }
    }

    // ── E logic (Phase Dive) ────────────────────────────────────────────────
    void LogicE() {
        const auto p = Player();
        const Vector3 cursor = SDK::Game::CursorPos();

        // Escape / follow the W missile back home
        if (Combo() && s_WMissile.IsValid()) {
            const Vector3 wPos = s_WMissile.Position();
            if (OktwCommon::CountEnemiesInRange(wPos, 200.0f) > 0 &&
                wPos.Distance(p.ServerPosition()) < 100.0f) {
                m_E.Cast(p.Position().Extend(wPos, m_E.Range));
                return;
            }
        }

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(800.0f, SDK::DamageType::Magical) : AIHeroClient();

        if (m_E.IsReady() && p.Mana() > m_RMANA + m_EMANA &&
            OktwCommon::CountEnemiesInRange(p.Position(), 260.0f) > 0 &&
            OktwCommon::CountEnemiesInRange(p.Position().Extend(cursor, m_E.Range), 500.0f) < 3 &&
            t.IsValid() &&
            t.Position().Distance(cursor) > t.Position().Distance(p.Position())) {
            m_E.Cast(p.Position().Extend(cursor, m_E.Range));
            return;
        }

        if (Combo() && p.Health() > p.MaxHealth() * 0.4f &&
            p.Mana() > m_RMANA + m_EMANA &&
            !p.IsUnderEnemyTurret() &&
            OktwCommon::CountEnemiesInRange(p.Position().Extend(cursor, m_E.Range), 700.0f) < 3) {
            if (t.IsValid() &&
                SDK::Extensions::IsValidTarget(t, FLT_MAX, true) &&
                p.Mana() > m_QMANA + m_EMANA + m_WMANA &&
                t.Position().Distance(cursor) + 300.0f < t.Position().Distance(p.Position())) {
                m_E.Cast(p.Position().Extend(cursor, m_E.Range));
                return;
            }
        }

        if (t.IsValid() && Combo() &&
            m_E.GetDamage(t) + m_W.GetDamage(t) > t.Health()) {
            m_E.Cast(p.Position().Extend(t.Position(), m_E.Range));
        }
    }

    // ── Jungle ──────────────────────────────────────────────────────────────
    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        if (p.Mana() <= m_QMANA + m_RMANA) return;

        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 500.0f, false, true);
        if (mobs.empty()) return;

        const auto& mob = mobs.front();
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(mob.Position()); return; }
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(mob.Position()); return; }
    }

    // ── Q logic (Timewinder) ────────────────────────────────────────────────
    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t  = ts ? ts->GetTarget(m_Q.Range,  SDK::DamageType::Magical) : AIHeroClient();
        auto t1 = ts ? ts->GetTarget(m_Q1.Range, SDK::DamageType::Magical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid() && SDK::Extensions::IsValidTarget(t, m_Q.Range, true)) {
            // TODO(SDK): missileManager.Target = t (return-missile aim) — not ported.

            if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                CastSpell(m_Q, t);
            } else if (Harass() &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.Mana() > m_RMANA + m_WMANA + m_QMANA + m_QMANA &&
                       OktwCommon::CanHarras()) {
                CastSpell(m_Q, t);
            } else if (OktwCommon::GetKsDamage(t, m_Q) * 2.0f > t.Health()) {
                CastSpell(m_Q, t);
            }

            if (p.Mana() > m_RMANA + m_QMANA + m_WMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_Q.Cast(enemy);
                    }
                }
            }
        } else if (t1.IsValid() && SDK::Extensions::IsValidTarget(t1, m_Q1.Range, true)) {
            // TODO(SDK): missileManager.Target = t1 — not ported.

            if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                CastSpell(m_Q1, t1);
            } else if (Harass() &&
                       GetBool((std::string("Harass") + t1.CharacterName()).c_str()) &&
                       p.Mana() > m_RMANA + m_WMANA + m_QMANA + m_QMANA &&
                       OktwCommon::CanHarras()) {
                CastSpell(m_Q1, t1);
            } else if (OktwCommon::GetKsDamage(t1, m_Q1) * 2.0f > t1.Health()) {
                CastSpell(m_Q1, t1);
            }

            if (p.Mana() > m_RMANA + m_QMANA + m_WMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (SDK::Extensions::IsValidTarget(enemy, m_Q1.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_Q1.Cast(enemy);
                    }
                }
            }
        } else if (FarmSpells() && GetBool("farmQ")) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q1.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_Q.GetLineFarmLocation(baseList, 100.0f);
            if (farm.MinionsHit >= FarmMinions()) {
                m_Q.Cast(farm.Position);
            }
        }
    }

    // ── W logic (Parallel Convergence) ──────────────────────────────────────
    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, SDK::DamageType::Physical) : AIHeroClient();

        if (t.IsValid() && SDK::Extensions::IsValidTarget(t, m_W.Range, true)) {
            if (GetBool("Waoe")) {
                m_W.CastIfWillHit(t, 2);
                if (OktwCommon::CountEnemiesInRange(t.Position(), 250.0f) > 1) {
                    CastSpell(m_W, t);
                }
            }
            if (Combo()) {
                const auto pred = m_W.GetPrediction(t);
                if (pred.GetCastPosition().Distance(t.Position()) > 200.0f) {
                    CastSpell(m_W, t);
                }
            }
        }

        if (!None()) {
            const auto p = Player();
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, m_W.Range, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    m_W.Cast(enemy);
                }
            }
        }
    }

    // ── Draw ────────────────────────────────────────────────────────────────
    void OnGameDraw() override {
        // TODO(SDK): the C# port draws range circles around player and helper
        // circles/text around WMissile/WMissile2/RMissile positions using
        // LeagueSharp.Common.Utility.DrawCircle + Drawing.DrawText. NightSharp's
        // Draw utilities differ; leaving as a stub matching Ahri/Annie ports.
    }
};

} } // namespace Plugins::OKTW
