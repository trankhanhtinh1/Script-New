#pragma once
// Port of OKTW_CSharp/Champions/Caitlyn.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class CaitlynPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Caitlyn"; }
    const char* GetInternalId() const override { return "champion.oktw.caitlyn"; }
    const char* GetChampionName() const override { return "Caitlyn"; }

protected:
    // Second Q instance mirroring C# Q1 (collision-aware variant).
    Spell m_Q1{ SpellSlot::Q };
    float m_QCastTime = 0.0f;
    // Track last W target (C# LastW) — retained for parity though rarely used.
    AIHeroClient m_lastW{};

    // Names of enemy spells that should trigger an auto-W trap under the caster.
    static const std::vector<std::string>& SpellsList() {
        static const std::vector<std::string> s = {
            "katarinar","drain","consume","absolutezero","staticfield",
            "reapthewhirlwind","jinxw","jinxr","shenstandunited","threshe",
            "threshrpenta","threshq","meditate","caitlynpiltoverpeacemaker",
            "volibearqattack","cassiopeiapetrifyinggaze","ezrealtrueshotbarrage",
            "galioidolofdurand","luxmalicecannon","missfortunebullettime",
            "infiniteduress","alzaharnethergrasp","lucianq","velkozr",
            "rocketgrabmissile"
        };
        return s;
    }

    void BuildMenu() override {
        MarkActive();

        m_Q  = Spell(SpellSlot::Q, 1250.0f);
        m_Q1 = Spell(SpellSlot::Q, 1250.0f);
        m_W  = Spell(SpellSlot::W, 800.0f);
        m_E  = Spell(SpellSlot::E, 830.0f);
        m_R  = Spell(SpellSlot::R, 3000.0f);

        m_Q.SetSkillshot(0.65f, 60.0f, 2200.0f, false, SDK::SpellType::Line);
        m_Q1.SetSkillshot(0.65f, 60.0f, 2200.0f, true,  SDK::SpellType::Line);
        m_W.SetSkillshot(1.5f, 20.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_E.SetSkillshot(0.30f, 70.0f, 2000.0f, true, SDK::SpellType::Line);
        m_R.SetSkillshot(0.7f, 200.0f, 1500.0f, false, SDK::SpellType::Circle);

        // Draw menu
        m_drawMenu->Add(new MenuBool("noti",    "Show notification & line", false));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));

        // Q Config
        m_qMenu->Add(new MenuBool("autoQ2", "Auto Q", true));
        m_qMenu->Add(new MenuBool("autoQ",  "Reduce Q use", true));
        m_qMenu->Add(new MenuBool("Qaoe",   "Q aoe", true));
        m_qMenu->Add(new MenuBool("Qslow",  "Q slow", true));

        // W Config
        m_wMenu->Add(new MenuBool("autoW",  "Auto W on hard CC", true));
        m_wMenu->Add(new MenuBool("telE",   "Auto W teleport", true));
        m_wMenu->Add(new MenuBool("forceW", "Force W before E", true));
        m_wMenu->Add(new MenuBool("bushW",  "Auto W bush after enemy enter", true));
        m_wMenu->Add(new MenuBool("bushW2", "Auto W bush and turret if full ammo", true));
        m_wMenu->Add(new MenuBool("Wspell", "W on special spell detection", true));

        Menu* wGap = m_wMenu->AddSubMenu(new Menu("WGap", "W Gap Closer"));
        static const char* wGCModes[] = { "Dash end position", "My hero position" };
        wGap->Add(new MenuList("WmodeGC", "Gap Closer position mode", wGCModes, 2, 0));
        Menu* wGapOn = wGap->AddSubMenu(new Menu("WGapOn", "Cast on enemy:"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("WGCchampion") + enemy.CharacterName();
            wGapOn->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        // E Config
        m_eMenu->Add(new MenuBool("autoE",      "Auto E", true));
        m_eMenu->Add(new MenuBool("Ehitchance", "Auto E dash and immobile target", true));
        m_eMenu->Add(new MenuBool("harrasEQ",   "TRY E + Q", true));
        m_eMenu->Add(new MenuBool("EQks",       "Ks E + Q + AA", true));
        m_eMenu->Add(new MenuKeyBind("useE",    "Dash E HotKeySmartcast", 'T', SDK::KeyBindType::Press));

        Menu* eGap = m_eMenu->AddSubMenu(new Menu("EGap", "E Gap Closer"));
        static const char* eGCModes[] = { "Dash end position", "Cursor position", "Enemy position" };
        eGap->Add(new MenuList("EmodeGC", "Gap Closer position mode", eGCModes, 3, 2));
        Menu* eGapOn = eGap->AddSubMenu(new Menu("EGapOn", "Cast on enemy:"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("EGCchampion") + enemy.CharacterName();
            eGapOn->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        // R Config
        m_rMenu->Add(new MenuBool("autoR",       "Auto R KS", true));
        m_rMenu->Add(new MenuSlider("Rcol",      "R collision width [400]", 400, 1, 1000));
        m_rMenu->Add(new MenuSlider("Rrange",    "R minimum range [1000]", 1000, 1, 1500));
        m_rMenu->Add(new MenuKeyBind("useR",     "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));
        m_rMenu->Add(new MenuBool("Rturrent",    "Don't R under turret", true));

        // Farm
        m_farmMenu->Add(new MenuBool("farmQ", "Lane clear Q", true));
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
            // TODO: SDK — PARRegenRate & Cooldown accessors missing; approximate with Q mana.
            m_RMANA = m_QMANA;
        } else {
            m_RMANA = m_R.Instance().ManaCost();
        }
    }

    // ── Utility mirrors of C# helpers ─────────────────────────────────────
    float GetRealRange(const AIBaseClient& target) const {
        return 680.0f + Player().BoundingRadius() + target.BoundingRadius();
    }
    float GetRealDistance(const AIBaseClient& target) const {
        return Player().ServerPosition().Distance(target.Position()) +
               Player().BoundingRadius() + target.BoundingRadius();
    }
    float BonusRange() const { return 720.0f + Player().BoundingRadius(); }

    // ── OnUpdate loop (mirrors Game_OnGameUpdate) ─────────────────────────
    void OnGameUpdate() override {
        const auto p = Player();
        if (!p.IsValid() || p.HasBuff("Recall")) return;

        // Semi-manual R
        if (GetKey("useR") && m_R.IsReady()) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_R.Range, SDK::DamageType::Physical) : AIHeroClient();
            if (t.IsValid()) {
                m_R.CastOnUnit(t);
            }
        }

        if (LagFree(0)) {
            SetMana();
            // R range scales: (500 * Level) + 1500
            m_R.Range = (500.0f * static_cast<float>(m_R.Instance().Level())) + 1500.0f;
        }

        // TODO: SDK — Orbwalking::CanMove missing; skip the gate.
        if (LagFree(1) && m_E.IsReady()) {
            LogicE();
        }

        // Skip further spell logic if AA will kill the current orb target.
        // TODO: SDK — Orbwalker::GetTarget() returning current AA target is
        //             not available; approximate with TargetSelector.
        {
            auto* ts = SDK::TargetSelector::Instance();
            auto orbT = ts ? ts->GetTarget(680.0f + p.BoundingRadius(), SDK::DamageType::Physical) : AIHeroClient();
            if (orbT.IsValid()) {
                if (p.GetAutoAttackDamage(orbT, false) * 2.0f > orbT.Health()) {
                    return;
                }
            }
        }

        if (LagFree(2) && m_W.IsReady()) {
            LogicW();
        }
        if (LagFree(3) && m_Q.IsReady() && GetBool("autoQ2")) {
            LogicQ();
        }
        // TODO: SDK — UnderTurret query missing; assume not-under-turret.
        const bool underTurret = false;
        if (LagFree(4) && m_R.IsReady() && GetBool("autoR") && !underTurret &&
            (SDK::Game::Time() - m_QCastTime) > 1.0f) {
            LogicR();
        }
    }

    // ── R KS logic ────────────────────────────────────────────────────────
    void LogicR() {
        const auto p = Player();
        // TODO: SDK — UnderTurret query missing.
        const bool underTurret = false;
        if (underTurret && GetBool("Rturrent")) return;

        const float rCol   = static_cast<float>(GetSlider("Rcol", 400));
        const float rRange = static_cast<float>(GetSlider("Rrange", 1000));

        for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!target.IsValid() || !target.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(target, m_R.Range, true)) continue;
            if (p.Distance(target.Position()) <= rRange) continue;
            if (OktwCommon::CountEnemiesInRange(target.Position(), rCol) != 1) continue;
            if (OktwCommon::CountAlliesInRange(target.Position(), 500.0f) != 0) continue;
            if (!OktwCommon::ValidUlt(target)) continue;

            if (OktwCommon::GetKsDamage(target, m_R) <= target.Health()) continue;

            bool cast = true;
            const auto output = m_R.GetPrediction(target);
            const Vector3 castPos = output.GetCastPosition();

            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy)) continue;
                if (enemy.CharacterName() == target.CharacterName() || !cast) continue;

                const auto prediction = m_R.GetPrediction(enemy);
                const Vector3 predictedPos = prediction.GetCastPosition();
                const Vector3 v = castPos - p.ServerPosition();
                const Vector3 w = predictedPos - p.ServerPosition();
                const double c1 = static_cast<double>(w.x) * v.x + static_cast<double>(w.y) * v.y + static_cast<double>(w.z) * v.z;
                const double c2 = static_cast<double>(v.x) * v.x + static_cast<double>(v.y) * v.y + static_cast<double>(v.z) * v.z;
                if (c2 <= 0.0) continue;
                const double b = c1 / c2;
                const Vector3 pb = {
                    p.ServerPosition().x + static_cast<float>(b) * v.x,
                    p.ServerPosition().y + static_cast<float>(b) * v.y,
                    p.ServerPosition().z + static_cast<float>(b) * v.z,
                };
                const float length = predictedPos.Distance(pb);
                if (length < (rCol + enemy.BoundingRadius()) &&
                    p.Distance(predictedPos) < p.Distance(target.ServerPosition())) {
                    cast = false;
                }
            }
            if (cast) {
                m_R.CastOnUnit(target);
            }
        }
    }

    // ── W logic (auto trap on hard CC / teleport traps / bush traps) ──────
    void LogicW() {
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_WMANA) return;

        if (GetBool("autoW")) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, m_W.Range, true)) continue;
                if (OktwCommon::CanMove(enemy)) continue;
                if (enemy.HasBuff("caitlynyordletrapinternal")) continue;
                m_W.Cast(enemy.Position());
            }
        }

        if (GetBool("telE")) {
            // TODO: SDK — original C# static GetTrapPos(range) scanned all
            //             enemies for trap positions. Iterate enemies and try
            //             each via the target-aware helper.
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, m_W.Range, true)) continue;
                const Vector3 trapPos = OktwCommon::GetTrapPos(enemy, m_W.Range);
                if (trapPos.x == 0.0f && trapPos.y == 0.0f && trapPos.z == 0.0f) continue;
                m_W.Cast(trapPos);
                break;
            }
        }

        // TODO: SDK — Orbwalking::CanMove(40) gate missing; skip.

        if (static_cast<int>(SDK::Game::Time() * 10.0f) % 2 == 0 && GetBool("bushW2")) {
            // TODO: SDK — Spellbook ammo query missing; assume ammo condition
            //             met so parity with bush-trap-on-cooldown behaviour
            //             is preserved when the toggle is on and no enemies
            //             are near.
            const bool ammoFull = true;
            if (ammoFull && OktwCommon::CountEnemiesInRange(p.Position(), 1000.0f) == 0) {
                const auto points = OktwCommon::CirclePoints(8, m_W.Range, p.Position());
                for (const Vector3& point : points) {
                    // TODO: SDK — NavMesh::IsWallOfGrass and UnderTurret checks
                    //             unavailable; treat every sampled point as a
                    //             candidate so the bush/turret sweep still fires.
                    const bool inGrassOrTurret = true;
                    if (!inGrassOrTurret) continue;
                    // TODO: SDK — Wall collision on inner circle points
                    //             unavailable; assume none.
                    bool anyWall = false;
                    if (!anyWall) {
                        m_W.Cast(point);
                        return;
                    }
                }
            }
        }
    }

    // ── Q logic (KS / harass / farm) ──────────────────────────────────────
    void LogicQ() {
        const auto p = Player();
        // TODO: SDK — Spellbook::IsAutoAttacking missing; skip that guard.

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, SDK::DamageType::Physical) : AIHeroClient();
        if (t.IsValid() && SDK::Extensions::IsValidTarget(t, m_Q.Range, true)) {
            // TODO: SDK — Orbwalking::InAutoAttackRange missing; substitute a
            //             distance check against a fixed AA range.
            const float aaRange = 680.0f + p.BoundingRadius() + t.BoundingRadius();
            const bool inAA = p.Distance(t.Position()) <= aaRange;

            if (GetRealDistance(t) > BonusRange() + 250.0f && !inAA &&
                OktwCommon::GetKsDamage(t, m_Q) > t.Health() &&
                OktwCommon::CountEnemiesInRange(p.Position(), 400.0f) == 0) {
                CastSpell(m_Q, t);
            } else if (Combo() && p.Mana() > m_RMANA + m_QMANA + m_EMANA + 10.0f &&
                       OktwCommon::CountEnemiesInRange(p.Position(), BonusRange() + 100.0f + t.BoundingRadius()) == 0 &&
                       !GetBool("autoQ")) {
                CastSpell(m_Q, t);
            }

            if ((Combo() || Harass()) && p.Mana() > m_RMANA + m_QMANA &&
                OktwCommon::CountEnemiesInRange(p.Position(), 400.0f) == 0) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (!SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true)) continue;
                    const bool immobile = !OktwCommon::CanMove(enemy) ||
                                          enemy.HasBuff("caitlynyordletrapinternal");
                    if (immobile) {
                        m_Q.Cast(enemy.Position());
                    }
                }
                if (OktwCommon::CountEnemiesInRange(p.Position(), BonusRange()) == 0 &&
                    OktwCommon::CanHarras()) {
                    // TODO: SDK — HasBuffOfType(Slow) missing; approximate via
                    //             common slow-buff names on the target.
                    const bool slowed = t.HasBuff("caitlynyordletrapinternal") ||
                                        t.HasBuff("slow");
                    if (slowed && GetBool("Qslow")) {
                        m_Q.Cast(t.Position());
                    }
                    if (GetBool("Qaoe")) {
                        // TODO: SDK — Spell::CastIfWillHit missing; fall back
                        //             to prediction cast with AoE hint.
                        const auto pout = m_Q.GetPrediction(t, true);
                        if (pout.AoeTargetsHitCount >= 2) {
                            m_Q.Cast(pout.GetCastPosition());
                        }
                    }
                }
            }
        } else if (FarmSpells() && GetBool("farmQ")) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_Q.GetLineFarmLocation(baseList, m_Q.Width);
            if (farm.MinionsHit >= FarmMinions()) {
                m_Q.Cast(farm.Position);
            }
        }
    }

    // ── E logic (net) ─────────────────────────────────────────────────────
    void LogicE() {
        const auto p = Player();
        // TODO: SDK — Spellbook::IsAutoAttacking missing; skip that guard.

        if (GetBool("autoE")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_E.Range, SDK::DamageType::Physical) : AIHeroClient();
            if (t.IsValid()) {
                // Mirror-position behind the caster (Player - (T - Player))
                const Vector3 pp = p.ServerPosition();
                const Vector3 delta = t.Position() - pp;
                const Vector3 positionT = { pp.x - delta.x, pp.y - delta.y, pp.z - delta.z };

                // Player.Position.Extend(positionT, 400)
                const Vector3 from = p.Position();
                const Vector3 dir  = positionT - from;
                const float len = dir.Length();
                Vector3 extended = from;
                if (len > 0.001f) {
                    const float scale = 400.0f / len;
                    extended = { from.x + dir.x * scale, from.y + dir.y * scale, from.z + dir.z * scale };
                }

                if (OktwCommon::CountEnemiesInRange(extended, 700.0f) < 2) {
                    const float eDmg = m_E.GetDamage(t);
                    const float qDmg = m_Q.GetDamage(t);
                    if (GetBool("EQks") &&
                        (qDmg + eDmg + p.GetAutoAttackDamage(t, false)) > t.Health() &&
                        p.Mana() > m_EMANA + m_QMANA) {
                        CastSpell(m_E, t);
                    } else if ((Harass() || Combo()) && GetBool("harrasEQ") &&
                               p.Mana() > m_EMANA + m_QMANA + m_RMANA) {
                        CastSpell(m_E, t);
                    }
                }

                if (p.Mana() > m_RMANA + m_EMANA) {
                    if (GetBool("Ehitchance")) {
                        // TODO: SDK — CastIfHitchanceEquals(Dashing) missing;
                        //             approximate with a normal predicted cast
                        //             so dashing/immobile targets still hit.
                        const auto pout = m_E.GetPrediction(t);
                        if (static_cast<int>(pout.Hitchance) >= static_cast<int>(HitChance::High)) {
                            m_E.Cast(pout.GetCastPosition());
                        }
                    }
                    if (p.Health() < p.MaxHealth() * 0.3f) {
                        if (GetRealDistance(t) < 500.0f) {
                            m_E.Cast(t.Position());
                        }
                        if (OktwCommon::CountEnemiesInRange(p.Position(), 250.0f) > 0) {
                            m_E.Cast(t.Position());
                        }
                    }
                }
            }
        }

        if (GetKey("useE")) {
            const Vector3 pp = p.ServerPosition();
            const Vector3 cursor = SDK::Game::CursorPos();
            const Vector3 delta = cursor - pp;
            const Vector3 position = { pp.x - delta.x, pp.y - delta.y, pp.z - delta.z };
            m_E.Cast(position);
        }
    }

    // ── Draw hook: parity with C# Drawing_OnDraw ──────────────────────────
    void OnGameDraw() override {
        // TODO: SDK — Drawing::DrawCircle / DrawText / DrawLine hooks needed
        //             to render the range indicators and KS notifications
        //             exposed by the C# port. Menu flags remain wired so the
        //             renderer can pick them up when available.
    }

    // ── Event handlers modeled after the C# subscriptions ─────────────────
    // These are invoked by SDK dispatchers when the corresponding events fire.
    // Kept here so the parity remains obvious even while the SDK wiring is
    // stubbed.

    // Spellbook_OnCastSpell (self-cast pre-processing).
    // TODO: SDK — Spellbook::OnCastSpell event & args (Slot, EndPosition,
    //             Process) not yet exposed; hook is a placeholder.
    void OnSpellbookCastSpell(SpellSlot slot, const Vector3& endPosition, bool& process) {
        if (slot == SpellSlot::W) {
            // TODO: SDK — Obj_GeneralParticleEmitter enumeration missing.
            //             Original behaviour blocks the W recast if a friendly
            //             trap particle already occupies the target position.
        }
        if (slot == SpellSlot::E &&
            Player().Mana() > m_RMANA + m_WMANA &&
            GetBool("forceW")) {
            const Vector3 pp = Player().Position();
            const Vector3 dir = endPosition - pp;
            const float len = dir.Length();
            Vector3 wCastPos = endPosition;
            if (len > 0.001f) {
                const float scale = (len + 50.0f) / len;
                wCastPos = { pp.x + dir.x * scale, pp.y + dir.y * scale, pp.z + dir.z * scale };
            }
            m_W.Cast(wCastPos);
            // TODO: SDK — DelayAction(10ms) unavailable; cast immediately.
            m_E.Cast(endPosition);
            (void)process;
        }
    }

    // Obj_AI_Base_OnProcessSpellCast — stamp Q cast time, reactive W trap.
    // TODO: SDK — OnProcessSpellCast dispatch not yet wired; call sites will
    //             feed this once the SDK exposes the event.
    void OnProcessSpellCast(const AIBaseClient& sender, const std::string& spellName) {
        if (sender.IsMe() && (spellName == "CaitlynPiltoverPeacemaker" ||
                              spellName == "CaitlynEntrapment")) {
            m_QCastTime = SDK::Game::Time();
        }
        if (!m_W.IsReady()) return;
        // TODO: SDK — sender.IsMinion() query missing; approximate with hero
        //             validity check below.
        if (!GetBool("Wspell")) return;
        // Only react to enemy heroes.
        // TODO: SDK — sender.IsEnemy() / IsHero() distinction missing on the
        //             base type. Cast to AIHeroClient when the SDK exposes it.
        if (!SDK::Extensions::IsValidTarget(sender, m_W.Range)) return;

        const std::string lower = [&]() {
            std::string s = spellName;
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }();
        for (const auto& probe : SpellsList()) {
            if (lower == probe) {
                m_W.Cast(sender.Position());
                break;
            }
        }
    }

    // AntiGapcloser_OnEnemyGapcloser — E/W react to dashes.
    // TODO: SDK — Gapcloser event / ActiveGapcloser struct missing.
    void OnEnemyGapcloser(const AIHeroClient& sender, const Vector3& endPos) {
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_WMANA) return;

        const std::string eId = std::string("EGCchampion") + sender.CharacterName();
        const std::string wId = std::string("WGCchampion") + sender.CharacterName();

        if (m_E.IsReady() && SDK::Extensions::IsValidTarget(sender, m_E.Range) &&
            GetBool(eId.c_str())) {
            const int mode = GetList("EmodeGC", 2);
            if (mode == 0) {
                m_E.Cast(endPos);
            } else if (mode == 1) {
                m_E.Cast(SDK::Game::CursorPos());
            } else {
                m_E.Cast(sender.ServerPosition());
            }
        } else if (m_W.IsReady() && SDK::Extensions::IsValidTarget(sender, m_W.Range) &&
                   GetBool(wId.c_str())) {
            const int mode = GetList("WmodeGC", 0);
            if (mode == 0) {
                m_W.Cast(endPos);
            } else {
                m_W.Cast(p.ServerPosition());
            }
        }
    }
};

} } // namespace Plugins::OKTW
