#pragma once
// Port of OKTW_CSharp/Champions/Jhin.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class JhinPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Jhin"; }
    const char* GetInternalId() const override { return "champion.oktw.jhin"; }
    const char* GetChampionName() const override { return "Jhin"; }

protected:
    // ── Runtime state (mirrors C# private fields) ──
    bool          m_Ractive = false;
    Vector3       m_rPosLast{};
    AIHeroClient  m_rTargetLast{};
    bool          m_hasRTargetLast = false;
    Vector3       m_rPosCast{};

    // TODO(oktw-port): Wire trinkets (3342 FarsightOrb, 3363 ScryingOrb) via
    // Spellbook.OnCastSpell — not exposed here.

    // Dangerous enemy spell names that trigger auto-E interrupt.
    static const std::vector<std::string>& DangerousSpells() {
        static const std::vector<std::string> s = {
            "katarinar","drain","consume","absolutezero","staticfield","reapthewhirlwind",
            "jinxw","jinxr","shenstandunited","threshe","threshrpenta","threshq","meditate",
            "caitlynpiltoverpeacemaker","volibearqattack","cassiopeiapetrifyinggaze",
            "ezrealtrueshotbarrage","galioidolofdurand","luxmalicecannon","missfortunebullettime",
            "infiniteduress","alzaharnethergrasp","lucianq","velkozr","rocketgrabmissile"
        };
        return s;
    }

    // ── C# private bool IsCastingR { get { return R.Instance.Name == "JhinRShot"; } } ──
    // Buff-based approximation — original relied on spell-name check.
    bool IsCastingR() const {
        const auto p = Player();
        return p.HasBuff("JhinRShot") || p.HasBuff("JhinR") || p.HasBuff("jhinrshot");
    }

    // ── C# private bool InCone(Vector3 Position) ──
    bool InCone(const Vector3& position) const {
        const auto p = Player();
        const float range = m_R.Range;
        const float angle = 70.0f * 3.14159265358979323846f / 180.0f;

        const float endX = m_rPosCast.x - p.Position().x;
        const float endZ = m_rPosCast.z - p.Position().z;
        const float half = angle * 0.5f;
        const float c1 = std::cos(-half), s1 = std::sin(-half);
        const float c2 = std::cos(angle),  s2 = std::sin(angle);
        // edge1 = end rotated by -angle/2
        const float e1x =  endX * c1 - endZ * s1;
        const float e1z =  endX * s1 + endZ * c1;
        // edge2 = edge1 rotated by angle
        const float e2x =  e1x * c2 - e1z * s2;
        const float e2z =  e1x * s2 + e1z * c2;
        // point = Position - Player.Position
        const float px = position.x - p.Position().x;
        const float pz = position.z - p.Position().z;
        const float dSqr = px * px + pz * pz;
        // 2D cross-product z-component
        const float cross1 = e1x * pz - e1z * px;
        const float cross2 = px * e2z - pz * e2x;
        return dSqr < range * range && cross1 > 0.0f && cross2 > 0.0f;
    }

    // ── Damage helpers (C# private double GetXdmg(Obj_AI_Base target)) ──
    // TODO(oktw-port): Confirm Spell::Level() / Player::FlatPhysicalDamageMod() /
    // Player::CalcDamage(target, DamageType, raw) exist on the SDK. Falls back
    // to the raw formula from Jhin.cs.
    float GetRdmg(const AIBaseClient& target) const {
        const auto p = Player();
        const float lvl = static_cast<float>(m_R.Level());
        const float ad  = p.BonusAttackDamage();
        const float raw = (-25.0f + 75.0f * lvl + 0.2f * ad) *
                          (1.0f + (100.0f - target.HealthPercent()) * 0.02f);
        return p.CalculatePhysicalDamage(target, raw);
    }
    float GetWdmg(const AIBaseClient& target) const {
        const auto p = Player();
        const float lvl = static_cast<float>(m_W.Level());
        const float ad  = p.BonusAttackDamage();
        const float raw = 55.0f + lvl * 35.0f + 0.7f * ad;
        return p.CalculatePhysicalDamage(target, raw);
    }
    float GetQdmg(const AIBaseClient& target) const {
        const auto p = Player();
        const float lvl = static_cast<float>(m_Q.Level());
        const float ad  = p.BonusAttackDamage();
        const float raw = 35.0f + lvl * 25.0f + 0.4f * ad;
        return p.CalculatePhysicalDamage(target, raw);
    }

    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 600.0f);
        m_W = Spell(SpellSlot::W, 2500.0f);
        m_E = Spell(SpellSlot::E, 760.0f);
        m_R = Spell(SpellSlot::R, 3500.0f);

        m_W.SetSkillshot(0.75f, 40.0f, 10000.0f, false, SDK::SpellType::Line);
        m_E.SetSkillshot(1.00f, 120.0f, 1600.0f, false, SDK::SpellType::Circle);
        m_R.SetSkillshot(0.24f, 80.0f, 5000.0f, false, SDK::SpellType::Line);

        // Draw
        m_drawMenu->Add(new MenuBool("qRange",    "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",    "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",    "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",    "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy",   "Draw only ready spells", true));
        m_drawMenu->Add(new MenuBool("rRangeMini","R range minimap", true));

        // Q
        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q", true));
        m_qMenu->Add(new MenuBool("harassQ", "Harass Q", true));
        m_qMenu->Add(new MenuBool("Qminion", "Q on minion", true));

        // W
        m_wMenu->Add(new MenuBool("autoW",       "Auto W", true));
        m_wMenu->Add(new MenuBool("autoWcombo",  "Auto W only in combo", false));
        m_wMenu->Add(new MenuBool("harassW",     "Harass W", true));
        m_wMenu->Add(new MenuBool("Wmark",       "W marked only (main target)", true));
        m_wMenu->Add(new MenuBool("Wmarkall",    "W marked (all enemys)", true));
        m_wMenu->Add(new MenuBool("Waoe",        "W aoe (above 2 enemy)", true));
        m_wMenu->Add(new MenuBool("autoWcc",     "Auto W CC enemy", true));
        m_wMenu->Add(new MenuSlider("MaxRangeW", "Max W range", 2500, 0, 2500));

        // E
        m_eMenu->Add(new MenuBool("autoE",  "Auto E on hard CC", true));
        m_eMenu->Add(new MenuBool("bushE",  "Auto E bush", true));
        m_eMenu->Add(new MenuBool("Espell", "E on special spell detection", true));
        static const char* eCombo[] = { "always", "run - cheese", "disable" };
        m_eMenu->Add(new MenuList("EmodeCombo", "E combo mode", eCombo, 3, 1));
        m_eMenu->Add(new MenuSlider("Eaoe", "Auto E x enemies", 3, 0, 5));
        Menu* eGC = m_eMenu->AddSubMenu(new Menu("EGCSub", "E Gap Closer"));
        static const char* eGCMode[] = { "Dash end position", "My hero position" };
        eGC->Add(new MenuList("EmodeGC", "Gap Closer position mode", eGCMode, 2, 0));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("EGCchampion") + enemy.CharacterName();
            eGC->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        // R
        m_rMenu->Add(new MenuBool("autoR",     "Enable R", true));
        m_rMenu->Add(new MenuBool("Rvisable",  "Don't shot if enemy is not visable", false));
        m_rMenu->Add(new MenuBool("Rks",       "Auto R if can kill in 3 hits", true));
        m_rMenu->Add(new MenuKeyBind("useR",   "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));
        m_rMenu->Add(new MenuSlider("MaxRangeR","Max R range", 3000, 0, 3500));
        m_rMenu->Add(new MenuSlider("MinRangeR","Min R range", 1000, 0, 3500));
        m_rMenu->Add(new MenuSlider("Rsafe",   "R safe area", 1000, 0, 2000));
        m_rMenu->Add(new MenuBool("trinkiet",  "Auto blue trinkiet", true));

        // Farm
        m_farmMenu->Add(new MenuBool("farmQ",    "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("farmW",    "Lane clear W", true));
        m_farmMenu->Add(new MenuBool("farmE",    "Lane clear E", true));
        m_farmMenu->Add(new MenuBool("jungleE",  "Jungle clear E", true));
        m_farmMenu->Add(new MenuBool("jungleQ",  "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW",  "Jungle clear W", true));

        // TODO(oktw-port): AntiGapcloser::OnEnemyGapcloser handler for E dash-cancel.
        // TODO(oktw-port): Obj_AI_Base::OnProcessSpellCast handler for:
        //                   - jhinr cast → capture rPosCast
        //                   - enemy dangerous-spell → cast E on sender.Position
        // TODO(oktw-port): Spellbook::OnCastSpell for trinket auto-cast on R.
        // TODO(oktw-port): Drawing::OnEndScene → minimap R range.
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
        if (!m_R.IsReady())
            m_RMANA = m_WMANA - 0.0f * m_W.Instance().Cooldown(); // TODO(oktw-port): PARRegenRate unavailable
        else
            m_RMANA = m_R.Instance().ManaCost();
    }

    void OnGameUpdate() override {
        if (LagFree(0)) { SetMana(); Jungle(); }

        if (LagFree(1) && m_R.IsReady()) LogicR();

        if (IsCastingR()) {
            blockMove   = true;
            blockAttack = true;
            // TODO(oktw-port): SebbyLib.Orbwalking.Attack/Move flags — no direct binding.
            return;
        } else {
            blockMove   = false;
            blockAttack = false;
        }

        if (LagFree(4) && m_E.IsReady() /* && SebbyLib.Orbwalking.CanMove(50) */)
            LogicE();

        if (LagFree(2) && m_Q.IsReady() && GetBool("autoQ"))
            LogicQ();

        if (LagFree(3) && m_W.IsReady() && !Player().Spellbook().IsWindingUp() && GetBool("autoW"))
            LogicW();
    }

    // ── LogicR ────────────────────────────────────────────────────────────────
    void LogicR() {
        if (!IsCastingR())
            m_R.Range = static_cast<float>(GetSlider("MaxRangeR", 3000));
        else
            m_R.Range = 3500.0f;

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            m_rPosLast = m_R.GetPrediction(t).GetCastPosition();
            if (GetKey("useR") && !IsCastingR()) {
                m_R.Cast(m_rPosLast);
                m_rTargetLast = t;
                m_hasRTargetLast = true;
            }

            if (!IsCastingR() && GetBool("Rks") && GetBool("autoR") &&
                GetRdmg(t) * 4.0f > t.Health() &&
                OktwCommon::CountAlliesInRange(t.Position(), 700.0f) == 0 &&
                p.CountEnemyHeroesInRange(static_cast<float>(GetSlider("Rsafe", 1000))) == 0 &&
                p.Position().Distance(t.Position()) > static_cast<float>(GetSlider("MinRangeR", 1000)) &&
                !p.IsUnderEnemyTurret() && OktwCommon::ValidUlt(t) &&
                !OktwCommon::IsSpellHeroCollision(t, m_R)) {
                m_R.Cast(m_rPosLast);
                m_rTargetLast = t;
                m_hasRTargetLast = true;
            }
            if (IsCastingR()) {
                if (InCone(t.ServerPosition())) {
                    m_R.Cast(t);
                } else {
                    // Iterate enemies in cone ordered by health
                    std::vector<AIHeroClient> pool;
                    for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                        if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                        if (SDK::Extensions::IsValidTarget(enemy, m_R.Range, true) &&
                            InCone(t.ServerPosition()))  // NB: C# uses t.ServerPosition per-iter — preserved verbatim
                            pool.push_back(enemy);
                    }
                    std::sort(pool.begin(), pool.end(),
                              [](const AIHeroClient& a, const AIHeroClient& b){ return a.Health() < b.Health(); });
                    for (const auto& enemy : pool) {
                        m_R.Cast(t);
                        m_rPosLast = m_R.GetPrediction(enemy).GetCastPosition();
                        m_rTargetLast = enemy;
                        m_hasRTargetLast = true;
                    }
                }
            }
        } else if (IsCastingR() && m_hasRTargetLast && !m_rTargetLast.IsDead()) {
            if (!GetBool("Rvisable") &&
                InCone(m_rTargetLast.Position()) && InCone(m_rPosLast)) {
                m_R.Cast(m_rPosLast);
            }
        }
    }

    // ── LogicW ────────────────────────────────────────────────────────────────
    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            const float wDmg = GetWdmg(t);
            if (wDmg > t.Health() - OktwCommon::GetIncomingDamage(t))
                CastSpell(m_W, t);

            if (GetBool("autoWcombo") && !Combo())
                return;

            if (p.CountEnemyHeroesInRange(400.0f) > 1 || p.CountEnemyHeroesInRange(250.0f) > 0)
                return;

            if (t.HasBuff("jhinespotteddebuff") || !GetBool("Wmark")) {
                if (p.Position().Distance(t.Position()) < static_cast<float>(GetSlider("MaxRangeW", 2500))) {
                    if (Combo() && p.Mana() > m_RMANA + m_WMANA)
                        CastSpell(m_W, t);
                    else if (Harass() && GetBool("harassW") &&
                             GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                             p.Mana() > m_RMANA + m_WMANA + m_QMANA + m_WMANA &&
                             OktwCommon::CanHarras())
                        CastSpell(m_W, t);
                }
            }

            if (!None() && p.Mana() > m_RMANA + m_WMANA) {
                if (GetBool("Waoe")) {
                    // Approximation of W.CastIfWillHit(t, 2)
                    const auto pout = m_W.GetPrediction(t, true);
                    if (pout.AoeTargetsHitCount >= 2)
                        m_W.Cast(pout.GetCastPosition());
                }

                if (GetBool("autoWcc")) {
                    for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                        if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                        if (SDK::Extensions::IsValidTarget(enemy, m_W.Range, true) &&
                            !OktwCommon::CanMove(enemy))
                            CastSpell(m_W, enemy);
                    }
                }
                if (GetBool("Wmarkall")) {
                    for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                        if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                        if (SDK::Extensions::IsValidTarget(enemy, m_W.Range, true) &&
                            enemy.HasBuff("jhinespotteddebuff"))
                            CastSpell(m_W, enemy);
                    }
                }
            }
        }
        if (FarmSpells() && GetBool("farmW")) {
            auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_W.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_W.GetLineFarmLocation(baseList, m_W.Width);
            if (farm.MinionsHit >= FarmMinions())
                m_W.Cast(farm.Position);
        }
    }

    // ── LogicE ────────────────────────────────────────────────────────────────
    void LogicE() {
        if (GetBool("autoE")) {
            // TODO(oktw-port): OktwCommon.GetTrapPos(E.Range) — Jhin-specific trap
            // helper not ported. Falls through to CC iteration.

            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, m_E.Range, true) &&
                    !OktwCommon::CanMove(enemy))
                    m_E.Cast(enemy.Position());
            }
        }

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Physical) : AIHeroClient();
        const int eMode = GetList("EmodeCombo", 1);
        const auto p = Player();
        if (t.IsValid() && eMode != 2) {
            if (Combo() && !p.Spellbook().IsWindingUp()) {
                if (eMode == 1) {
                    const Vector3 castPos = m_E.GetPrediction(t).GetCastPosition();
                    if (castPos.Distance(t.Position()) > 100.0f) {
                        if (p.Position().Distance(t.ServerPosition()) > p.Position().Distance(t.Position())) {
                            if (t.Position().Distance(p.ServerPosition()) < t.Position().Distance(p.Position()))
                                CastSpell(m_E, t);
                        } else {
                            if (t.Position().Distance(p.ServerPosition()) > t.Position().Distance(p.Position()))
                                CastSpell(m_E, t);
                        }
                    }
                } else {
                    CastSpell(m_E, t);
                }
            }

            // Approximation of E.CastIfWillHit(t, Eaoe)
            const int aoe = GetSlider("Eaoe", 3);
            const auto pout = m_E.GetPrediction(t, true);
            if (aoe > 0 && pout.AoeTargetsHitCount >= aoe)
                m_E.Cast(pout.GetCastPosition());
        } else if (FarmSpells() && GetBool("farmE")) {
            auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_E.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_E.GetCircularFarmLocation(baseList, m_E.Width);
            if (farm.MinionsHit >= FarmMinions())
                m_E.Cast(farm.Position);
        }
    }

    // ── LogicQ ────────────────────────────────────────────────────────────────
    void LogicQ() {
        // Orbwalker.GetTarget() → best-effort via TargetSelector for hero focus.
        auto* ts = SDK::TargetSelector::Instance();
        auto torb = ts ? ts->GetTarget(m_Q.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();

        if (!torb.IsValid()) {
            if (GetBool("Qminion")) {
                auto tFar = ts ? ts->GetTarget(m_Q.Range + 300.0f, DamageType::Physical) : AIHeroClient();
                if (tFar.IsValid()) {
                    // Cache.GetMinions(Prediction(t, 0.1).CastPosition, 300) → nearest minion in Q range
                    const Vector3 predPos = m_Q.GetPrediction(tFar).GetCastPosition();
                    auto minions = OktwCommon::GetMinions(predPos, 300.0f);
                    const SDK::AIMinionClient* bestMinion = nullptr;
                    float bestDist = FLT_MAX;
                    for (const auto& mn : minions) {
                        if (!SDK::Extensions::IsValidTarget(mn, m_Q.Range, true)) continue;
                        const float d = mn.Position().Distance(tFar.Position());
                        if (d < bestDist) { bestDist = d; bestMinion = &mn; }
                    }
                    if (bestMinion) {
                        if (tFar.Health() < GetQdmg(tFar))
                            m_Q.Cast(*bestMinion);
                        if (Combo() && p.Mana() > m_RMANA + m_EMANA)
                            m_Q.Cast(*bestMinion);
                        else if (Harass() && GetBool("harassQ") &&
                                 p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA &&
                                 GetBool((std::string("Harass") + tFar.CharacterName()).c_str()))
                            m_Q.Cast(*bestMinion);
                    }
                }
            }
        } else if (!p.Spellbook().IsWindingUp()) {
            // TODO(oktw-port): SebbyLib.Orbwalking.CanAttack() gating deferred.
            const auto& t = torb;
            if (t.Health() < GetQdmg(t) + GetWdmg(t))
                m_Q.Cast(t);
            if (Combo() && p.Mana() > m_RMANA + m_QMANA)
                m_Q.Cast(t);
            else if (Harass() && GetBool("harassQ") &&
                     p.Mana() > m_RMANA + m_QMANA + m_WMANA + m_EMANA &&
                     GetBool((std::string("Harass") + t.CharacterName()).c_str()))
                m_Q.Cast(t);
        }

        if (FarmSpells() && GetBool("farmQ")) {
            auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_Q.Range);
            if (static_cast<int>(minions.size()) >= FarmMinions()) {
                for (const auto& mn : minions) {
                    if (m_Q.GetDamage(mn) > SDK::HealthPrediction::GetPrediction(mn, 300, 0)) {
                        if (SDK::Extensions::IsValidTarget(mn, m_Q.Range, true)) {
                            m_Q.Cast(mn);
                        }
                        break;
                    }
                }
            }
        }
    }

    // ── Jungle ────────────────────────────────────────────────────────────────
    void Jungle() {
        if (!LaneClear()) return;
        auto mobs = OktwCommon::GetMinions(Player().ServerPosition(), m_Q.Range, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(mob.ServerPosition()); return; }
        if (m_E.IsReady() && GetBool("jungleE")) { m_E.Cast(mob.ServerPosition()); return; }
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(mob); return; }
    }

    void OnGameDraw() override {
        // Simplified: rely on SDK draw utilities for range circles.
    }
};

} } // namespace Plugins::OKTW
