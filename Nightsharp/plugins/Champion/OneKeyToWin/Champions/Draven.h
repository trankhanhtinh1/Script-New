#pragma once
// Port of OKTW_CSharp/Champions/Draven.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class DravenPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Draven"; }
    const char* GetInternalId() const override { return "champion.oktw.draven"; }
    const char* GetChampionName() const override { return "Draven"; }

protected:
    int m_axeCatchRange = 500;

    // Tracked "Q_reticle_self" ground indicators — port of `axeList`.
    // TODO(SDK): GameObject::OnCreate / OnDelete events are not exposed yet.
    // When available, wire GameObjectOnOnCreate/GameObjectOnOnDelete to
    // push/erase entries here. Currently stays empty and logic degrades
    // gracefully (see AxeLogic() early-out when count == 0).
    std::vector<SDK::GameObject> m_axeList;

    // Tracked outgoing R missile — port of static `RMissile`.
    // TODO(SDK): missile-object-tracked events (SpellMissile_OnCreate /
    // Obj_SpellMissile_OnDelete on GameObject) are not yet exposed.
    // Keep the state variable so the draw logic below can consume it once
    // hooked up.
    bool m_hasRMissile = false;
    Vector3 m_rMissilePos{};

    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q);
        m_W = Spell(SpellSlot::W);
        m_E = Spell(SpellSlot::E, 1050.0f);
        m_R = Spell(SpellSlot::R, 3000.0f);

        m_E.SetSkillshot(0.25f, 100.0f, 1400.0f, false, SDK::SpellType::Line);
        m_R.SetSkillshot(0.40f, 160.0f, 2000.0f, false, SDK::SpellType::Line);

        // Draw ------------------------------------------------------------
        m_drawMenu->Add(new MenuBool("noti",         "Draw R helper",             true));
        m_drawMenu->Add(new MenuBool("onlyRdy",      "Draw only ready spells",    true));
        m_drawMenu->Add(new MenuBool("qCatchRange",  "Q catch range",             true));
        m_drawMenu->Add(new MenuBool("qAxePos",      "Q axe position",            true));
        m_drawMenu->Add(new MenuBool("eRange",       "E range",                   false));

        // AXE option -----------------------------------------------------
        Menu* axeMenu = m_champMenu->AddSubMenu(new Menu("AXE", "AXE option"));
        axeMenu->Add(new MenuSlider("axeCatchRange", "Axe catch range", 500, 200, 2000));
        axeMenu->Add(new MenuBool("axeTower",  "Don't catch axe under enemy turret combo", true));
        axeMenu->Add(new MenuBool("axeTower2", "Don't catch axe under enemy turret farm",  true));
        axeMenu->Add(new MenuBool("axeEnemy",  "Don't catch axe in enemy grup",            true));
        axeMenu->Add(new MenuBool("axeKill",   "Don't catch axe if can kill 2 AA",         true));
        axeMenu->Add(new MenuBool("axePro",    "if axe timeout: force laneclear",          true));

        // Q --------------------------------------------------------------
        m_qMenu->Add(new MenuBool("autoQ", "Auto Q", true));
        m_qMenu->Add(new MenuBool("farmQ", "Farm Q", true));

        // W --------------------------------------------------------------
        m_wMenu->Add(new MenuBool("autoW", "Auto W",     true));
        m_wMenu->Add(new MenuBool("slowW", "Auto W slow", true));

        // E --------------------------------------------------------------
        m_eMenu->Add(new MenuBool("autoE",  "Auto E",                          true));
        m_eMenu->Add(new MenuBool("autoE2", "Harras E if can hit 2 targets",   true));
        m_eMenu->Add(new MenuBool("agcE",   "On Enemy Gapcloser",              true));
        m_eMenu->Add(new MenuBool("intE",   "On Interruptable Target",         true));

        // R --------------------------------------------------------------
        static const char* rDmgModes[] = { "X 1", "X 2" };
        m_rMenu->Add(new MenuBool("autoR", "Auto R", true));
        m_rMenu->Add(new MenuList("Rdmg",  "KS damage calculation", rDmgModes, 2, 1));
        m_rMenu->Add(new MenuBool("comboR",     "Auto R in combo",       true));
        m_rMenu->Add(new MenuBool("Rcc",        "R cc",                  true));
        m_rMenu->Add(new MenuBool("Raoe",       "R aoe combo",           true));
        m_rMenu->Add(new MenuBool("hitchanceR", "VeryHighHitChanceR",    true));
        m_rMenu->Add(new MenuKeyBind("useR", "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));

        // TODO(SDK): register the following event hooks once exposed by the SDK:
        //   GameObject::OnCreate  -> SpellMissile_OnCreateOld (track "DravenR" missile)
        //   GameObject::OnDelete  -> Obj_SpellMissile_OnDelete (clear tracked missile)
        //   GameObject::OnCreate  -> GameObjectOnOnCreate ("Q_reticle_self" push)
        //   GameObject::OnDelete  -> GameObjectOnOnDelete ("Q_reticle_self" erase)
        //   Orbwalker::BeforeAttack -> BeforeAttack (auto-Q buff/mana logic)
        //   AntiGapcloser::OnEnemyGapcloser -> AntiGapcloser_OnEnemyGapcloser
        //   Interrupter2::OnInterruptableTarget -> Interrupter2_OnInterruptableTarget
    }

    // ------------------------------------------------------------------
    // Mana bookkeeping — port of Draven.SetMana()
    // ------------------------------------------------------------------
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
            // C#: RMANA = EMANA - PARRegenRate * E.Instance.Cooldown
            const auto p = Player();
            m_RMANA = m_EMANA - 0.0f * m_E.Instance().Cooldown(); // TODO(oktw-port): ManaRegen() not available in SDK
        } else {
            m_RMANA = m_R.Instance().ManaCost();
        }
    }

    // ------------------------------------------------------------------
    // Ported Interrupter2 handler.
    // TODO(SDK): wire once Interrupter2 events exist. Call this from the
    // event when an enemy starts an interruptable channel.
    // ------------------------------------------------------------------
    void Interrupter2_OnInterruptableTarget(const AIHeroClient& sender) {
        if (GetBool("intE") && m_E.IsReady() &&
            SDK::Extensions::IsValidTarget(sender, m_E.Range, true)) {
            m_E.Cast(sender);
        }
    }

    // ------------------------------------------------------------------
    // Ported AntiGapcloser handler.
    // TODO(SDK): wire once AntiGapcloser events exist.
    // ------------------------------------------------------------------
    void AntiGapcloser_OnEnemyGapcloser(const AIHeroClient& sender) {
        if (GetBool("agcE") && m_E.IsReady() &&
            SDK::Extensions::IsValidTarget(sender, m_E.Range, true)) {
            m_E.Cast(sender);
        }
    }

    // ------------------------------------------------------------------
    // Ported BeforeAttack handler (auto-Q on-hit).
    // TODO(SDK): call from Orbwalker::BeforeAttack once available. `target`
    // is the intended AA target (may be an AIHeroClient).
    // ------------------------------------------------------------------
    void BeforeAttack(const SDK::AIBaseClient& target) {
        if (!m_Q.IsReady()) return;
        const auto p = Player();
        const int buffCount = OktwCommon::GetBuffCount(p, "dravenspinningattack");
        const int axeCount  = static_cast<int>(m_axeList.size());

        if (GetBool("autoQ") && target.IsValid() && target.IsHero()) {
            if (buffCount + axeCount == 0)
                m_Q.Cast();
            else if (p.Mana() > m_RMANA + m_QMANA && buffCount == 0)
                m_Q.Cast();
        }

        if (Farm() && GetBool("farmQ")) {
            if (buffCount + axeCount == 0 && p.Mana() > m_RMANA + m_EMANA + m_WMANA)
                m_Q.Cast();
            else if (p.ManaPercent() > 70.0f && buffCount == 0)
                m_Q.Cast();
        }
    }

    // ------------------------------------------------------------------
    // Per-tick logic — port of Draven.GameOnOnUpdate.
    // ------------------------------------------------------------------
    void OnGameUpdate() override {
        // Prune stale tracked axes.
        m_axeList.erase(std::remove_if(m_axeList.begin(), m_axeList.end(),
            [](const SDK::GameObject& o) { return !o.IsValid(); }),
            m_axeList.end());

        const auto p = Player();
        if (!p.IsValid() || p.HasBuff("Recall")) return;

        if (LagFree(1)) {
            m_axeCatchRange = GetSlider("axeCatchRange", 500);
            SetMana();
            AxeLogic();

            if (GetBool("axePro") && p.HasBuff("dravenspinningattack")) {
                const float bt = OktwCommon::GetPassiveTime(p, "dravenspinningattack");
                if (bt < 1.0f) {
                    // TODO(SDK): Orbwalker::SetActiveMode(LaneClear) not exposed.
                    // C#: Orbwalker.ActiveMode = LaneClear;
                } else {
                    // TODO(SDK): Orbwalker::SetActiveMode(None) not exposed.
                }
            } else {
                // TODO(SDK): Orbwalker::SetActiveMode(None) not exposed.
            }
        }

        if (LagFree(2) && m_E.IsReady() && GetBool("autoE"))
            LogicE();

        if (LagFree(3) && m_W.IsReady())
            LogicW();

        if (LagFree(4) && m_R.IsReady() && !p.Spellbook().IsWindingUp())
            LogicR();
    }

    // ------------------------------------------------------------------
    // W — port of Draven.LogicW
    // ------------------------------------------------------------------
    void LogicW() {
        const auto p = Player();
        if (GetBool("autoW") && Combo() &&
            p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_QMANA &&
            OktwCommon::CountEnemiesInRange(p.ServerPosition(), 1000.0f) > 0 &&
            !p.HasBuff("dravenfurybuff")) {
            m_W.Cast();
        } else if (GetBool("slowW") &&
                   p.Mana() > m_RMANA + m_EMANA + m_WMANA &&
                   false /* TODO(oktw-port): BuffType::Slow unavailable in SDK */) {
            m_W.Cast();
        }
    }

    // ------------------------------------------------------------------
    // E — port of Draven.LogicE
    // ------------------------------------------------------------------
    void LogicE() {
        // KS branch: E for the kill outside AA range.
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(enemy, m_E.Range, true)) continue;
            if (SDK::Core::Utils::AutoAttack::InAutoAttackRange(enemy)) continue;
            if (m_E.GetDamage(enemy) > enemy.Health()) {
                CastSpell(m_E, enemy);
                return;
            }
        }

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid()) {
            if (Combo()) {
                if (p.Mana() > m_RMANA + m_EMANA) {
                    if (!SDK::Core::Utils::AutoAttack::InAutoAttackRange(t))
                        CastSpell(m_E, t);
                    if (p.Health() < p.MaxHealth() * 0.5f)
                        CastSpell(m_E, t);
                }
                if (p.Mana() > m_RMANA + m_EMANA + m_QMANA) {
                    // TODO(SDK): Spell::CastIfWillHit(target, count, aoe) is not
                    // exposed. Fallback: cast E on target via prediction when
                    // AoE prediction reports >=2 targets in the line.
                    const auto pred = m_E.GetPrediction(t, /*aoe*/ true);
                    if (pred.AoeTargetsHitCount >= 2) m_E.Cast(pred.GetCastPosition());
                }
            }
            if (Harass() && GetBool("autoE2") &&
                p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_QMANA) {
                // TODO(SDK): CastIfWillHit fallback (see above).
                const auto pred = m_E.GetPrediction(t, /*aoe*/ true);
                if (pred.AoeTargetsHitCount >= 2) m_E.Cast(pred.GetCastPosition());
            }
        }

        // Peel — E on any melee enemy stuck to us.
        for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!target.IsValid() || !target.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(target, m_E.Range, true)) continue;
            if (SDK::Extensions::IsValidTarget(target, 300.0f, true) && target.IsMelee()) {
                CastSpell(m_E, t.IsValid() ? t : target);
            }
        }
    }

    // ------------------------------------------------------------------
    // R — port of Draven.LogicR
    // ------------------------------------------------------------------
    void LogicR() {
        const auto p = Player();

        // Semi-manual key: fire regardless.
        if (GetKey("useR")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Physical) : AIHeroClient();
            if (t.IsValid()) {
                // TODO(SDK): CastIfWillHit fallback — cast on prediction with AoE.
                const auto pred = m_R.GetPrediction(t, /*aoe*/ true);
                if (pred.AoeTargetsHitCount >= 2) m_R.Cast(pred.GetCastPosition());
                m_R.Cast(t);
            }
        }

        if (!GetBool("autoR")) return;

        for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!target.IsValid() || !target.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(target, m_R.Range, true)) continue;
            if (!OktwCommon::ValidUlt(target)) continue;
            if (OktwCommon::CountAlliesInRange(target.Position(), 500.0f) != 0) continue;

            const float predictedHealth = target.Health() - OktwCommon::GetIncomingDamage(target);
            float rDmg = CalculateR(target);

            if (rDmg * 2.0f > predictedHealth && GetList("Rdmg", 1) == 1)
                rDmg = rDmg + getRdmg(target);

            const float qDmg = m_Q.GetDamage(target);
            const float eDmg = m_E.GetDamage(target);

            if (rDmg > predictedHealth && !SDK::Core::Utils::AutoAttack::InAutoAttackRange(target)) {
                castR(target);
                NightSharpDebug::Logf("[Draven] R normal");
            } else if (Combo() && GetBool("comboR") &&
                       SDK::Core::Utils::AutoAttack::InAutoAttackRange(target) &&
                       rDmg * 2.0f + p.GetAutoAttackDamage(target, false) > predictedHealth) {
                castR(target);
                NightSharpDebug::Logf("[Draven] R normal");
            } else if (GetBool("Rcc") && rDmg * 2.0f > predictedHealth &&
                       !OktwCommon::CanMove(target) &&
                       SDK::Extensions::IsValidTarget(target, m_E.Range, true)) {
                m_R.Cast(target);
                NightSharpDebug::Logf("[Draven] R normal");
            } else if (Combo() && GetBool("Raoe")) {
                // TODO(SDK): CastIfWillHit(target, 3, true) fallback.
                const auto pred = m_R.GetPrediction(target, /*aoe*/ true);
                if (pred.AoeTargetsHitCount >= 3) m_R.Cast(pred.GetCastPosition());
            } else if (SDK::Extensions::IsValidTarget(target, m_E.Range, true) &&
                       rDmg * 2.0f + qDmg + eDmg > predictedHealth &&
                       GetBool("Raoe")) {
                // TODO(SDK): CastIfWillHit(target, 2, true) fallback.
                const auto pred = m_R.GetPrediction(target, /*aoe*/ true);
                if (pred.AoeTargetsHitCount >= 2) m_R.Cast(pred.GetCastPosition());
            }
        }
    }

    // ------------------------------------------------------------------
    // castR — port of Draven.castR (extra hit-chance guard).
    // ------------------------------------------------------------------
    void castR(const AIHeroClient& target) {
        if (GetBool("hitchanceR")) {
            const auto waypoints = target.GetWaypoints();
            const auto p = Player();
            if (target.Path().size() < 2 && !waypoints.empty()) {
                const Vector3 lastWp3D = { waypoints.back().x, target.Position().y, waypoints.back().y };
                if ((p.Position().Distance(lastWp3D) - p.Position().Distance(target.Position())) > 300.0f) {
                    CastSpell(m_R, target);
                }
            }
        } else {
            CastSpell(m_R, target);
        }
    }

    // ------------------------------------------------------------------
    // CalculateR — port of Draven.CalculateR (raw physical scaling).
    // ------------------------------------------------------------------
    float CalculateR(const SDK::AIBaseClient& target) const {
        const auto p = Player();
        const float raw = (75.0f + 100.0f * static_cast<float>(m_R.Level())) +
                          p.BonusAttackDamage() * 1.1f;
        return p.CalculatePhysicalDamage(target, raw);
    }

    // ------------------------------------------------------------------
    // getRdmg — port of Draven.getRdmg. Approximates collision falloff by
    // counting enemies + minions whose predicted position lies within the
    // R line between us and the target.
    // ------------------------------------------------------------------
    float getRdmg(const SDK::AIBaseClient& target) {
        const auto p = Player();
        const float rDmg = m_R.GetDamage(target);
        int dmg = 0;

        const auto output = m_R.GetPrediction(target);
        const Vector3 pServer  = p.ServerPosition();
        const Vector3 targetSv = target.ServerPosition();
        const Vector3 v        = output.GetCastPosition() - pServer;
        const float c2         = v.x * v.x + v.z * v.z;
        if (c2 < 0.0001f) return rDmg;

        auto tally = [&](const SDK::AIBaseClient& other) {
            const auto pred = m_R.GetPrediction(other);
            const Vector3 predictedPosition = pred.GetCastPosition();
            const Vector3 w = predictedPosition - pServer;
            const float c1 = w.x * v.x + w.z * v.z;
            const float b  = c1 / c2;
            const Vector3 pb = { pServer.x + b * v.x, pServer.y, pServer.z + b * v.z };
            const float dx = predictedPosition.x - pb.x;
            const float dz = predictedPosition.z - pb.z;
            const float length = std::sqrt(dx * dx + dz * dz);
            if (length < (m_R.Width + 100.0f + other.BoundingRadius() / 2.0f) &&
                pServer.Distance(predictedPosition) < pServer.Distance(targetSv)) {
                ++dmg;
            }
        };

        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(enemy)) continue;
            tally(enemy);
        }
        const auto minions = OktwCommon::GetMinions(pServer, m_R.Range);
        for (const auto& minion : minions) tally(minion);

        if (dmg > 8) return rDmg * 0.6f;
        return rDmg - (rDmg * 0.08f * static_cast<float>(dmg));
    }

    // ------------------------------------------------------------------
    // AXE handling — ports Draven.AxeLogic + CatchAxe. Both depend on the
    // "Q_reticle_self" GameObject event feeding m_axeList (see TODOs above).
    // ------------------------------------------------------------------
    void AxeLogic() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(800.0f, DamageType::Physical) : AIHeroClient();
        const auto p = Player();

        if (GetBool("axeKill") && t.IsValid() &&
            p.Position().Distance(t.Position()) > 400.0f &&
            p.GetAutoAttackDamage(t, false) * 2.0f > t.Health()) {
            // TODO(SDK): Orbwalker::SetOrbwalkingPoint(Game::CursorPos()) not exposed.
            return;
        }

        if (m_axeList.empty()) {
            // TODO(SDK): Orbwalker::SetOrbwalkingPoint(Game::CursorPos()) not exposed.
            return;
        }

        if (m_axeList.size() == 1) {
            CatchAxe(m_axeList.front());
            return;
        }

        const Vector3 cursor = SDK::Game::CursorPos();
        const SDK::GameObject* best = &m_axeList.front();
        for (const auto& obj : m_axeList) {
            if (cursor.Distance(best->Position()) > cursor.Distance(obj.Position()))
                best = &obj;
        }
        CatchAxe(*best);
    }

    void CatchAxe(const SDK::GameObject& axe) {
        const auto p = Player();
        const Vector3 axePos = axe.Position();

        if (p.Position().Distance(axePos) < 100.0f) {
            // TODO(SDK): Orbwalker::SetOrbwalkingPoint(Game::CursorPos()).
            return;
        }

        // TODO(SDK): Vector3::UnderTurret() extension is not available. When
        // added, gate the below early-returns on `axePos.UnderTurret(true)`.
        const bool axeUnderTurret = false;

        if (GetBool("axeTower") && Combo() && axeUnderTurret) {
            // TODO(SDK): Orbwalker::SetOrbwalkingPoint(Game::CursorPos()).
            return;
        }
        if (GetBool("axeTower2") && Harass() && axeUnderTurret) {
            // TODO(SDK): Orbwalker::SetOrbwalkingPoint(Game::CursorPos()).
            return;
        }
        if (GetBool("axeEnemy") && OktwCommon::CountEnemiesInRange(axePos, 500.0f) > 2) {
            // TODO(SDK): Orbwalker::SetOrbwalkingPoint(Game::CursorPos()).
            return;
        }

        const Vector3 cursor = SDK::Game::CursorPos();
        if (cursor.Distance(axePos) < static_cast<float>(m_axeCatchRange)) {
            // TODO(SDK): Orbwalker::SetOrbwalkingPoint(axe.Position()).
        } else {
            // TODO(SDK): Orbwalker::SetOrbwalkingPoint(Game::CursorPos()).
        }
    }

    // ------------------------------------------------------------------
    // Draw helper — port of Draven.drawText2. Renders text 200px above the
    // world-projected point.
    // ------------------------------------------------------------------
    static void drawText2(const std::string& msg, const Vector3& worldPos, std::uint32_t color) {
        if (!SDK::Drawing::IsEnabled()) return;
        Vec2 wts{};
        if (!SDK::Drawing::WorldToScreen(worldPos, wts)) return;
        const float x = wts.x - static_cast<float>(msg.size()) * 5.0f;
        const float y = wts.y - 200.0f;
        SDK::Drawing::DrawText({ x, y }, msg.c_str(), color);
    }

    // ------------------------------------------------------------------
    // OnDraw — port of Draven.Drawing_OnDraw.
    // ------------------------------------------------------------------
    void OnGameDraw() override {
        if (!SDK::Drawing::IsEnabled()) return;

        const auto p = Player();
        if (!p.IsValid()) return;

        if (GetBool("qAxePos")) {
            if (p.HasBuff("dravenspinningattack")) {
                const float bt = OktwCommon::GetPassiveTime(p, "dravenspinningattack");
                char buf[32];
                std::snprintf(buf, sizeof(buf), "Q:  %.1f", bt);
                if (bt < 2.0f) {
                    if (static_cast<int>(SDK::Game::Time() * 10.0f) % 2 == 0) {
                        drawText2(buf, p.Position(), 0xFFFFFF00u); // Yellow
                    }
                } else {
                    drawText2(buf, p.Position(), 0xFFADFF2Fu); // GreenYellow
                }
            }

            const Vector3 cursor = SDK::Game::CursorPos();
            for (const auto& obj : m_axeList) {
                const Vector3 op = obj.Position();
                // TODO(SDK): UnderTurret extension unavailable — treat as false.
                const bool underTurret = false;
                if (cursor.Distance(op) > static_cast<float>(m_axeCatchRange) || underTurret) {
                    SDK::Drawing::DrawCircle(op, 150.0f, 0xFFFF4500u); // OrangeRed
                } else if (p.Position().Distance(op) > 120.0f) {
                    SDK::Drawing::DrawCircle(op, 150.0f, 0xFFFFFF00u); // Yellow
                } else if (p.Position().Distance(op) < 150.0f) {
                    SDK::Drawing::DrawCircle(op, 150.0f, 0xFF9ACD32u); // YellowGreen
                }
            }
        }

        if (GetBool("qCatchRange")) {
            SDK::Drawing::DrawCircle(SDK::Game::CursorPos(),
                                     static_cast<float>(m_axeCatchRange),
                                     0xFFB0C4DEu); // LightSteelBlue
        }

        if (GetBool("noti") && m_hasRMissile) {
            OktwCommon::DrawLineRectangle(m_rMissilePos, p.Position(),
                                          static_cast<int>(m_R.Width), 1,
                                          0xFFFFFFFFu); // White
        }

        if (GetBool("eRange")) {
            const bool onlyRdy = GetBool("onlyRdy");
            if (!onlyRdy || m_E.IsReady()) {
                SDK::Drawing::DrawCircle(p.Position(), m_E.Range, 0xFFFFFF00u); // Yellow
            }
        }
    }
};

} } // namespace Plugins::OKTW
