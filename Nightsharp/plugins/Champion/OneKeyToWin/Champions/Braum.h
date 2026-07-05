#pragma once
// Port of OKTW_CSharp/Champions/Braum.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class BraumPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Braum"; }
    const char* GetInternalId() const override { return "champion.oktw.braum"; }
    const char* GetChampionName() const override { return "Braum"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 1000.0f);
        m_W = Spell(SpellSlot::W, 650.0f);
        m_E = Spell(SpellSlot::E, 0.0f);
        m_R = Spell(SpellSlot::R, 1250.0f);
        m_Q.SetSkillshot(0.25f, 60.0f,  1700.0f, true,  SDK::SpellType::Line);
        m_R.SetSkillshot(0.50f, 115.0f, 1400.0f, false, SDK::SpellType::Line);

        // ── Draw ──
        m_drawMenu->Add(new MenuBool("notif",   "Notification (timers)",   true));
        m_drawMenu->Add(new MenuBool("noti",    "Show KS notification",    true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range",                 false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range",                 false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range",                 false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells",  true));

        // ── Q Config ──
        m_qMenu->Add(new MenuBool("autoQ", "Auto Q",             true));
        m_qMenu->Add(new MenuBool("AGCq",  "Anti Gapcloser Q",   true));

        // ── E/W Shield Config (attached to E menu) ──
        m_eMenu->Add(new MenuBool("autoE", "Auto E", true));
        m_eMenu->Add(new MenuSlider("Edmg", "Shield incoming damage %", 20, 0, 100));

        // Per-enemy spell manager toggles (C# iterates enemy.Spellbook.Spells[0..3]).
        // The C++ SDK does not expose enemy spellbooks; expose a per-champion toggle
        // instead so the shield logic can be gated per attacker champion.
        Menu* spellMgr = m_eMenu->AddSubMenu(new Menu("SpellMgr", "Spell Manager"));
        // TODO(oktw-port): iterate enemy.Spellbook.Spells to create per-spell toggles
        //                  ("spell" + SData.Name.ToLower()) once SDK exposes spellbooks.
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("spellenemy") + enemy.CharacterName();
            spellMgr->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        // Per-ally "Use on" toggles for W shield.
        Menu* useOn = m_eMenu->AddSubMenu(new Menu("EWUseOn", "Use on"));
        for (const auto& ally : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!ally.IsValid() || !ally.IsAlly()) continue;
            const std::string id = std::string("Eon") + ally.CharacterName();
            useOn->Add(new MenuBool(id.c_str(), ally.CharacterName().c_str(), true));
        }

        // Gapcloser submenu.
        Menu* gapMenu = m_eMenu->AddSubMenu(new Menu("EWGap", "Gapcloser"));
        gapMenu->Add(new MenuBool("AGC", "Anti Gapcloser E + W", true));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("gapcloser") + enemy.CharacterName();
            gapMenu->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        // ── R Config ──
        m_rMenu->Add(new MenuBool("autoR",   "Auto R", true));
        m_rMenu->Add(new MenuKeyBind("useR", "Semi-manual cast R", 'T', SDK::KeyBindType::Press));
        m_rMenu->Add(new MenuBool("rCombo",  "Always in combo",           false));
        m_rMenu->Add(new MenuSlider("rCount", "Auto R if hit x enemies", 3, 0, 5));
        m_rMenu->Add(new MenuBool("rCc",     "Auto R immobile enemy korean style", true));
        m_rMenu->Add(new MenuBool("OnInterruptableSpell", "OnInterruptableSpell", true));

        Menu* um = m_rMenu->AddSubMenu(new Menu("UMSub", "Ultimate manager"));
        static const char* rModes[] = { "Normal ", "Always ", "Never ", "Normal + Gapcloser R" };
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("Rmode") + enemy.CharacterName();
            um->Add(new MenuList(id.c_str(), enemy.CharacterName().c_str(), rModes, 4, 0));
        }

        // TODO(oktw-port): wire Obj_AI_Base::OnProcessSpellCast for shield reactions.
        // TODO(oktw-port): wire AntiGapcloser::OnEnemyGapcloser for E/W/Q gap responses.
        // TODO(oktw-port): wire Interrupter2::OnInterruptableTarget for R.
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
        if (LagFree(0)) SetMana();

        // Semi-manual R keybind.
        if (m_R.IsReady() && GetKey("useR")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Physical) : AIHeroClient();
            if (t.IsValid() && SDK::Extensions::IsValidTarget(t, m_R.Range, true))
                m_R.Cast(t);
        }

        if (LagFree(2) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(4) && m_R.IsReady() && GetBool("autoR")) LogicR();

        // Shield reactions ordinarily driven by OnProcessSpellCast in C#. Without
        // that hook here we still opportunistically E/W-shield low-HP allies.
        if (LagFree(3)) ShieldPassive();
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(500.0f, DamageType::Physical) : AIHeroClient();
        if (!t.IsValid() || !SDK::Extensions::IsValidTarget(t, 500.0f, true)) {
            t = ts ? ts->GetTarget(m_Q.Range, DamageType::Physical) : AIHeroClient();
        }

        if (t.IsValid() && SDK::Extensions::IsValidTarget(t, m_Q.Range, true)) {
            const auto p = Player();
            if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                CastSpell(m_Q, t);
            } else if (Harass()) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (!SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true)) continue;
                    const std::string hid = std::string("Harass") + enemy.CharacterName();
                    if (GetBool(hid.c_str())) CastSpell(m_Q, enemy);
                }
            }
            if (!None() && p.Mana() > m_RMANA + m_QMANA + m_EMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (!SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true)) continue;
                    if (!OktwCommon::CanMove(enemy)) m_Q.Cast(enemy);
                }
            }
        }
    }

    void LogicR() {
        const int rCount = GetSlider("rCount", 3);
        // Order enemies by lowest HP first (C# OrderBy(t => t.Health)).
        std::vector<AIHeroClient> targets;
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(enemy, m_R.Range, true)) continue;
            if (!OktwCommon::ValidUlt(enemy)) continue;
            targets.push_back(enemy);
        }
        std::sort(targets.begin(), targets.end(),
                  [](const AIHeroClient& a, const AIHeroClient& b) { return a.Health() < b.Health(); });

        for (const auto& t : targets) {
            const std::string rid = std::string("Rmode") + t.CharacterName();
            const int rMode = GetList(rid.c_str(), 0);
            if (rMode == 2) continue;
            if (rMode == 1) CastSpell(m_R, t);

            if (rCount > 0) {
                // TODO(oktw-port): Spell::CastIfWillHit not available; approximate via
                //                  AoE prediction hit count.
                const auto pout = m_R.GetPrediction(t, true);
                if (pout.AoeTargetsHitCount >= rCount &&
                    static_cast<int>(pout.Hitchance) >= static_cast<int>(HitChance::High)) {
                    m_R.Cast(pout.GetCastPosition());
                }
            }

            if (GetBool("rCc") && !OktwCommon::CanMove(t) &&
                t.HealthPercent() > 20.0f * OktwCommon::CountAlliesInRange(t.Position(), 500.0f)) {
                // C# used DelayAction (800ms - distance/2). Without a scheduler here
                // we cast immediately if still a valid ult target.
                // TODO(oktw-port): route through a delay scheduler when SDK exposes one.
                CastRtime(t);
            }

            if (GetBool("rCombo") && Combo()) {
                CastSpell(m_R, t);
                return;
            }
        }
    }

    void CastRtime(const AIHeroClient& t) {
        if (OktwCommon::ValidUlt(t)) m_R.Cast(t);
    }

    // ── Passive shield sweep (no OnProcessSpellCast available yet) ──
    // Mirrors the "incoming damage %" branch of Obj_AI_Base_OnProcessSpellCast:
    // for each ally in W range whose incoming damage exceeds the configured %,
    // shield them and drop E toward the closest enemy.
    void ShieldPassive() {
        if (!m_W.IsReady() && !m_E.IsReady()) return;
        if (!GetBool("autoE") && !m_W.IsReady()) return;

        const auto p = Player();
        const int dmgPct = GetSlider("Edmg", 20);

        for (const auto& ally : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!ally.IsValid() || !ally.IsAlly()) continue;
            if (p.Distance(ally.ServerPosition()) >= m_W.Range) continue;

            const std::string eonId = std::string("Eon") + ally.CharacterName();
            if (!GetBool(eonId.c_str())) continue;

            const float incoming = OktwCommon::GetIncomingDamage(ally, 1.0f);
            if (incoming <= ally.Health() * dmgPct * 0.01f) continue;

            // Skip low-HP self-preservation cases matching C# nuance.
            const bool isMe = ally.NetworkId() == p.NetworkId();
            if (p.HealthPercent() < 20.0f && !isMe) continue;
            if (p.HealthPercent() < 50.0f && !isMe && ally.IsUnderEnemyTurret()) continue;

            if (m_W.IsReady()) m_W.Cast(ally);

            if (m_E.IsReady() && GetBool("autoE")) {
                // Aim E at the nearest enemy hero to the ally being shielded.
                AIHeroClient nearest;
                float best = FLT_MAX;
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (!SDK::Extensions::IsValidTarget(enemy, 800.0f, true)) continue;
                    const float d = ally.Distance(enemy.Position());
                    if (d < best) { best = d; nearest = enemy; }
                }
                if (nearest.IsValid()) m_E.Cast(nearest.Position());
            }
        }
    }

    void OnGameDraw() override {
        // Simplified: SDK draw utilities handle range circles via menu toggles.
    }
};

} } // namespace Plugins::OKTW
