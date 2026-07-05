#pragma once
// Port of OKTW_CSharp/Champions/Ashe.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class AshePlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Ashe"; }
    const char* GetInternalId() const override { return "champion.oktw.ashe"; }
    const char* GetChampionName() const override { return "Ashe"; }

protected:
    // Semi-manual R state (mirrors CastR / CastR2 latches in C#)
    bool m_castR  = false;
    bool m_castR2 = false;
    // TODO(oktw-port): missing WndProc right-click hook — right-click "R KEY TARGET"
    // capture is deferred; m_rTarget stays invalid until an SDK mouse hook is available.
    AIBaseClient m_rTarget;

    void BuildMenu() override {
        MarkActive();

        // Q is a self-cast steroid (Ranger's Focus) — no range/skillshot.
        m_Q = Spell(SpellSlot::Q);
        m_W = Spell(SpellSlot::W, 1240.0f);
        m_E = Spell(SpellSlot::E, 2500.0f);
        m_R = Spell(SpellSlot::R, 2500.0f);
        m_W.SetSkillshot(0.25f,  20.0f, 1500.0f, true,  SDK::SpellType::Line);
        m_E.SetSkillshot(0.25f, 299.0f, 1400.0f, false, SDK::SpellType::Line);
        m_R.SetSkillshot(0.25f, 130.0f, 1600.0f, false, SDK::SpellType::Line);

        // ── Draw ──
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));
        m_drawMenu->Add(new MenuBool("wRange",  "W range",                false));
        m_drawMenu->Add(new MenuBool("rNot",    "R key info",             true));

        // ── Q ──
        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q",   true));
        m_qMenu->Add(new MenuBool("harassQ", "Harass Q", true));

        // ── W ──
        m_wMenu->Add(new MenuBool("autoW",   "Auto W",             true));
        m_wMenu->Add(new MenuBool("ksW",     "Auto KS W",          true));
        m_wMenu->Add(new MenuBool("ccW",     "W immobile target",  true));
        m_wMenu->Add(new MenuBool("harassW", "Harass W",           true));

        // ── E ──
        m_eMenu->Add(new MenuBool("autoE", "Auto E", true));

        // ── R ──
        m_rMenu->Add(new MenuBool("autoR",      "Auto R",                             true));
        m_rMenu->Add(new MenuBool("Rkscombo",   "R KS combo R + W + AA",              true));
        m_rMenu->Add(new MenuBool("autoRaoe",   "Auto R aoe",                         true));
        m_rMenu->Add(new MenuBool("autoRinter", "Auto R OnPossibleToInterrupt",       true));
        m_rMenu->Add(new MenuKeyBind("useR2", "R key target cast",       SDK::Keys::Y, SDK::KeyBindType::Press));
        m_rMenu->Add(new MenuKeyBind("useR",  "Semi-manual cast R key",  SDK::Keys::T, SDK::KeyBindType::Press));
        static const char* semiModes[] = { "LOW HP", "CLOSEST" };
        m_rMenu->Add(new MenuList("Semi-manual", "Semi-manual MODE", semiModes, 2, 0));

        Menu* gap = m_rMenu->AddSubMenu(new Menu("GapCloserR", "GapCloser R"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("GapCloser") + enemy.CharacterName();
            gap->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), false));
        }

        // ── Farm ──
        m_farmMenu->Add(new MenuBool("farmQ",   "Lane clear Q",   true));
        m_farmMenu->Add(new MenuBool("farmW",   "Lane clear W",   true));
        m_farmMenu->Add(new MenuBool("jungleQ", "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW", "Jungle clear W", true));

        // TODO(oktw-port): missing Interrupter2 — auto R on interruptible cast deferred.
        // TODO(oktw-port): missing AntiGapcloser — auto R on enemy gapcloser deferred.
        // TODO(oktw-port): missing Orbwalker AfterAttack — LogicQ now runs from OnGameUpdate tick.
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
            // C#: RMANA = WMANA - PARRegenRate * W.Cooldown; regen-scaled reserve.
            // TODO(oktw-port): missing PARRegenRate on Player — approximate with WMANA.
            m_RMANA = m_WMANA;
        } else {
            m_RMANA = m_R.Instance().ManaCost();
        }
    }

    void OnGameUpdate() override {
        // Semi-manual R (T = smart target, Y = right-click target) — mirrors CastR latches.
        if (m_R.IsReady()) {
            if (GetKey("useR"))  m_castR  = true;
            if (GetKey("useR2")) m_castR2 = true;

            if (m_castR2) {
                if (m_rTarget.IsValid() &&
                    SDK::Extensions::IsValidTarget(m_rTarget, m_R.Range, true)) {
                    CastSpell(m_R, m_rTarget);
                }
            }

            if (m_castR) {
                const int mode = GetList("Semi-manual", 0);
                auto* ts = SDK::TargetSelector::Instance();
                if (mode == 0) {
                    auto t = ts ? ts->GetTarget(1800.0f, DamageType::Physical) : AIHeroClient();
                    if (t.IsValid()) CastSpell(m_R, t);
                } else if (mode == 1) {
                    AIHeroClient closest;
                    float bestDist = FLT_MAX;
                    const auto pp = Player().Position();
                    for (const auto& e : SDK::ObjectManager::Get<AIHeroClient>()) {
                        if (!e.IsValid() || !e.IsEnemy()) continue;
                        if (!SDK::Extensions::IsValidTarget(e, m_R.Range, true)) continue;
                        const float d = e.Position().Distance(pp);
                        if (d < bestDist) { bestDist = d; closest = e; }
                    }
                    if (closest.IsValid()) CastSpell(m_R, closest);
                }
            }
        } else {
            m_castR  = false;
            m_castR2 = false;
        }

        if (LagFree(1)) {
            SetMana();
            Jungle();
        }

        // C# ran LogicQ from AfterAttack; without that event we tick it every frame.
        if (LagFree(2) && m_Q.IsReady()) {
            LogicQ();
        }

        if (LagFree(3) && m_W.IsReady() && !Player().Spellbook().IsWindingUp() && GetBool("autoW")) {
            LogicW();
        }

        if (LagFree(4) && m_R.IsReady()) {
            LogicR();
        }
    }

    void Jungle() {
        if (!LaneClear()) return;
        auto mobs = OktwCommon::GetMinions(Player().ServerPosition(), 600.0f, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(mob.ServerPosition()); return; }
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(); return; }
    }

    void LogicR() {
        const auto p = Player();

        if (GetBool("autoR")) {
            for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!target.IsValid() || !target.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(target, 2000.0f, true)) continue;
                if (!OktwCommon::ValidUlt(target)) continue;

                const float rDmg = OktwCommon::GetKsDamage(target, m_R);

                // AoE R in combo
                if (Combo() && GetBool("autoRaoe") &&
                    OktwCommon::CountEnemiesInRange(target.Position(), 250.0f) > 2 &&
                    SDK::Extensions::IsValidTarget(target, 1500.0f, true)) {
                    CastSpell(m_R, target);
                }

                // Combo KS: R + W + 5 AAs on slowed target
                if (Combo() && GetBool("Rkscombo") &&
                    SDK::Extensions::IsValidTarget(target, m_W.Range, true) &&
                    p.GetAutoAttackDamage(target, false) * 5.0f + rDmg + m_W.GetDamage(target)
                        > target.Health() &&
                    false /* TODO(oktw-port): BuffType::Slow unavailable in SDK */ &&
                    !OktwCommon::IsSpellHeroCollision(target, m_R)) {
                    CastSpell(m_R, target);
                }

                // Long-range KS on isolated target
                if (rDmg > target.Health() &&
                    OktwCommon::CountAlliesInRange(target.Position(), 600.0f) == 0 &&
                    target.Position().Distance(p.Position()) > 1000.0f) {
                    if (!OktwCommon::IsSpellHeroCollision(target, m_R)) {
                        CastSpell(m_R, target);
                    }
                }
            }
        }

        // Panic R on melee gapcloser when low HP
        if (p.HealthPercent() < 50.0f) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, 300.0f, true)) continue;
                if (!enemy.IsMelee()) continue;
                if (!OktwCommon::ValidUlt(enemy)) continue;
                const std::string gid = std::string("GapCloser") + enemy.CharacterName();
                if (GetBool(gid.c_str())) {
                    m_R.Cast(enemy);
                }
            }
        }
    }

    void LogicQ() {
        // Orbwalker current-target proxy — TS pick over W.Range works as a stand-in.
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid() && SDK::Extensions::IsValidTarget(t, 650.0f, true)) {
            if (Combo() && GetBool("autoQ") &&
                (p.Mana() > m_RMANA + m_QMANA ||
                 t.Health() < 5.0f * p.GetAutoAttackDamage(p, false))) {
                m_Q.Cast();
            } else if (Harass() && p.Mana() > m_RMANA + m_QMANA + m_WMANA &&
                       GetBool("harassQ") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                m_Q.Cast();
            }
        } else if (LaneClear() && FarmSpells() && GetBool("farmQ")) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), 600.0f);
            if (static_cast<int>(minions.size()) >= FarmMinions()) {
                m_Q.Cast();
            }
        }
    }

    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid()) {
            if (Combo() && p.Mana() > m_RMANA + m_WMANA) {
                CastSpell(m_W, t);
            } else if (Harass() && p.Mana() > m_RMANA + m_WMANA + m_QMANA + m_WMANA &&
                       GetBool("harassW") && OktwCommon::CanHarras()) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (!SDK::Extensions::IsValidTarget(enemy, m_W.Range, true)) continue;
                    if (GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                        CastSpell(m_W, enemy);
                    }
                }
            } else if (GetBool("ksW") && OktwCommon::GetKsDamage(t, m_W) > t.Health()) {
                CastSpell(m_W, t);
            }

            // W on any immobile enemy in range
            if (!None() && p.Mana() > m_RMANA + m_WMANA && GetBool("ccW")) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (!SDK::Extensions::IsValidTarget(enemy, m_W.Range, true)) continue;
                    if (!OktwCommon::CanMove(enemy)) {
                        m_W.Cast(enemy);
                    }
                }
            }
        } else if (FarmSpells() && GetBool("farmW")) {
            // C# used GetCircularFarmLocation @ radius 300 — approximated with line farm.
            // TODO(oktw-port): missing GetCircularFarmLocation — using line farm fallback.
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_W.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_W.GetLineFarmLocation(baseList, m_W.Width);
            if (farm.MinionsHit >= FarmMinions()) {
                m_W.Cast(farm.Position);
            }
        }
    }

    void OnGameDraw() override {
        // Range/notification drawing left to shared draw utilities.
        // C# drew W range circle and "R KEY TARGET" text; deferred to SDK draw hooks.
    }
};

} } // namespace Plugins::OKTW
