#pragma once
// Port of OKTW_CSharp/Champions/Anivia.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;

class AniviaPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Anivia"; }
    const char* GetInternalId() const override { return "champion.oktw.anivia"; }
    const char* GetChampionName() const override { return "Anivia"; }

protected:
    // Missile tracking (from Obj_AI_Base_OnCreate/OnDelete)
    // TODO: hook SDK GameObject::OnCreate/OnDelete once available and set/clear
    //       m_qMissile when "cryo_FlashFrost_Player_mis.troy" spawns/despawns,
    //       and m_rMissile when a "cryo_storm" object spawns/despawns.
    void* m_qMissile = nullptr; // TODO: SDK::GameObject*
    void* m_rMissile = nullptr; // TODO: SDK::GameObject*

    // Cached Q missile world position (updated by the missile hook above)
    Vector3 m_qMissilePos{};
    Vector3 m_rMissilePos{};

    float m_RCastTime = 0.0f;
    static constexpr float kRwidth = 400.0f;

    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 1000.0f);
        m_W = Spell(SpellSlot::W, 950.0f);
        m_E = Spell(SpellSlot::E, 650.0f);
        m_R = Spell(SpellSlot::R, 685.0f);

        m_Q.SetSkillshot(0.25f, 110.0f, 870.0f, false, SDK::SpellType::Line);
        m_W.SetSkillshot(0.60f, 1.0f, FLT_MAX, false, SDK::SpellType::Line);
        m_R.SetSkillshot(0.50f, 200.0f, FLT_MAX, false, SDK::SpellType::Circle);

        // Draw
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));

        // Q Config
        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q", true));
        m_qMenu->Add(new MenuBool("AGCQ",    "Q gapcloser", false));
        m_qMenu->Add(new MenuBool("harassQ", "Harass Q", true));

        // W Config
        m_wMenu->Add(new MenuBool("autoW", "Auto W", true));
        m_wMenu->Add(new MenuBool("AGCW",  "AntiGapcloser W", false));
        m_wMenu->Add(new MenuBool("inter", "OnPossibleToInterrupt W", true));

        // E Config
        m_eMenu->Add(new MenuBool("autoE", "Auto E", true));

        // R Config
        m_rMenu->Add(new MenuBool("autoR", "Auto R", true));

        // Farm
        m_farmMenu->Add(new MenuBool("farmE",   "Lane clear E", false));
        m_farmMenu->Add(new MenuBool("farmR",   "Lane clear R", false));
        m_farmMenu->Add(new MenuBool("jungleE", "Jungle clear E", true));
        m_farmMenu->Add(new MenuBool("jungleQ", "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW", "Jungle clear W", true));
        m_farmMenu->Add(new MenuBool("jungleR", "Jungle clear R", true));

        // Root-level toggle
        m_champMenu->Add(new MenuBool("AACombo", "Disable AA if can use E", true));

        // TODO: SDK::Events::hook.OnObjectCreate/OnObjectDelete missile tracking:
        //   name == "cryo_FlashFrost_Player_mis.troy"   -> m_qMissile
        //   name.contains("cryo_storm")                 -> m_rMissile
        // TODO: SDK::Interrupter::OnInterruptableTarget -> LogicInterrupt(sender)
        // TODO: SDK::AntiGapcloser::OnEnemyGapcloser    -> LogicAntiGapcloser(sender)
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
            // C#: RMANA = QMANA - Player.PARRegenRate * Q.Instance.Cooldown
            // TODO: expose Player.PARRegenRate; approximate with QMANA for now.
            m_RMANA = m_QMANA;
        } else {
            m_RMANA = m_R.Instance().ManaCost();
        }
    }

    // -----------------------------------------------------------------------
    // Interrupter / AntiGapcloser (called by the corresponding SDK event hook
    // when it becomes available — see TODOs in BuildMenu).
    // -----------------------------------------------------------------------
    void LogicInterrupt(const AIHeroClient& sender) {
        if (GetBool("inter") && m_W.IsReady() &&
            SDK::Extensions::IsValidTarget(sender, m_W.Range, true)) {
            m_W.Cast(sender);
        }
    }

    void LogicAntiGapcloser(const AIHeroClient& sender) {
        if (m_Q.IsReady() && GetBool("AGCQ")) {
            if (SDK::Extensions::IsValidTarget(sender, 300.0f, true)) {
                m_Q.Cast(sender);
            }
        } else if (m_W.IsReady() && GetBool("AGCW")) {
            if (SDK::Extensions::IsValidTarget(sender, m_W.Range, true)) {
                const auto p = Player();
                const Vector3 from = p.Position();
                const Vector3 to = sender.Position();
                const Vector3 dir = to - from;
                const float len = dir.Length();
                Vector3 castPos = to;
                if (len > 0.001f) {
                    castPos = { from.x + dir.x * (50.0f / len),
                                from.y,
                                from.z + dir.z * (50.0f / len) };
                }
                m_W.Cast(castPos);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Main update — mirrors Game_OnGameUpdate in C#.
    // -----------------------------------------------------------------------
    void OnGameUpdate() override {
        const auto p = Player();
        if (!p.IsValid()) return;

        // AACombo: disable AA in combo when E is available; C# sets Orbwalking.Attack.
        // TODO: expose SDK::Orbwalker::SetAttack(bool). For now we set blockAttack.
        if (Combo() && GetBool("AACombo")) {
            blockAttack = m_E.IsReady() ? true : false;
        } else {
            blockAttack = false;
        }

        // Detonate Q if any enemy hero is within 230 of the flight missile.
        if (m_Q.IsReady() && m_qMissile != nullptr &&
            OktwCommon::CountEnemiesInRange(m_qMissilePos, 230.0f) > 0) {
            m_Q.Cast();
        }

        if (LagFree(0)) SetMana();

        if (LagFree(1) && m_R.IsReady() && GetBool("autoR")) LogicR();
        if (LagFree(2) && m_W.IsReady() && GetBool("autoW")) LogicW();
        if (LagFree(3) && m_Q.IsReady() && m_qMissile == nullptr && GetBool("autoQ")) LogicQ();
        if (LagFree(4)) {
            if (m_E.IsReady() && GetBool("autoE")) LogicE();
            Jungle();
        }
    }

    // -----------------------------------------------------------------------
    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, SDK::DamageType::Magical) : AIHeroClient();
        if (!t.IsValid()) return;

        const auto p = Player();
        const float mana = p.Mana();

        if (Combo() && mana > m_EMANA + m_QMANA - 10.0f) {
            CastSpell(m_Q, t);
        } else if (Harass() && GetBool("harassQ") &&
                   GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                   mana > m_RMANA + m_EMANA + m_QMANA + m_WMANA &&
                   OktwCommon::CanHarras()) {
            CastSpell(m_Q, t);
        } else {
            const float qDmg = OktwCommon::GetKsDamage(t, m_Q);
            const float eDmg = m_E.GetDamage(t);
            if (qDmg > t.Health()) {
                CastSpell(m_Q, t);
            } else if (qDmg + eDmg > t.Health() && mana > m_QMANA + m_WMANA) {
                CastSpell(m_Q, t);
            }
        }

        if (!None() && mana > m_RMANA + m_EMANA) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    m_Q.Cast(enemy);
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    void LogicW() {
        if (!(Combo() && Player().Mana() > m_RMANA + m_EMANA + m_WMANA)) return;

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, SDK::DamageType::Magical) : AIHeroClient();
        if (!t.IsValid() ||
            !SDK::Extensions::IsValidTarget(t, m_W.Range, true)) {
            return;
        }

        const auto p = Player();
        const auto pout = m_W.GetPrediction(t);
        if (pout.GetCastPosition().Distance(t.Position()) <= 100.0f) return;

        const Vector3 pPos = p.Position();
        const Vector3 pServ = p.ServerPosition();
        const Vector3 tPos = t.Position();
        const Vector3 tServ = t.ServerPosition();

        if (pPos.Distance(tServ) > pPos.Distance(tPos)) {
            // Target is moving toward us — W if server pos is farther for the target too.
            if (tPos.Distance(pServ) < tPos.Distance(pPos)) {
                CastSpell(m_W, t);
            }
        } else {
            // Target is moving away — W only if inside R range.
            if (tPos.Distance(pServ) > tPos.Distance(pPos) &&
                tPos.Distance(pPos) < m_R.Range) {
                CastSpell(m_W, t);
            }
        }
    }

    // -----------------------------------------------------------------------
    void LogicE() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, SDK::DamageType::Magical) : AIHeroClient();
        if (t.IsValid()) {
            const auto p = Player();
            const float now = SDK::Game::Time();
            float qCd = m_Q.Instance().CooldownExpires() - now;
            float rCd = m_R.Instance().CooldownExpires() - now;
            if (p.Level() < 7) rCd = 10.0f;

            const float eDmg = OktwCommon::GetKsDamage(t, m_E);
            const float eCooldown = m_E.Instance().Cooldown();

            if (eDmg > t.Health()) {
                m_E.Cast(t);
            }

            const bool chilled = t.HasBuff("chilled");
            if (chilled ||
                (qCd > eCooldown - 1.0f && rCd > eCooldown - 1.0f)) {
                if (eDmg * 3.0f > t.Health()) {
                    m_E.Cast(t);
                } else if (Combo() && (chilled || p.Mana() > m_RMANA + m_EMANA)) {
                    m_E.Cast(t);
                } else if (Harass() &&
                           p.Mana() > m_RMANA + m_EMANA + m_QMANA + m_WMANA &&
                           GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                           !p.IsUnderEnemyTurret() &&
                           m_qMissile == nullptr) {
                    m_E.Cast(t);
                }
            } else if (Combo() && m_R.IsReady() &&
                       p.Mana() > m_RMANA + m_EMANA &&
                       m_qMissile == nullptr) {
                m_R.Cast(t);
            }
        }
        FarmE();
    }

    // -----------------------------------------------------------------------
    void FarmE() {
        if (!(FarmSpells() && GetBool("farmE"))) return;
        // TODO: SDK::Orbwalker::CanAttack — skip if orbwalker will AA this frame.
        const auto p = Player();
        const auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_E.Range);
        for (const auto& mn : minions) {
            if (mn.Health() <= p.GetAutoAttackDamage(mn, false)) continue;
            const float eDmg = m_E.GetDamage(mn) * 2.0f;
            if (mn.Health() < eDmg && mn.HasBuff("chilled")) {
                m_E.Cast(mn);
                break;
            }
        }
    }

    // -----------------------------------------------------------------------
    void LogicR() {
        const auto p = Player();
        if (m_rMissile == nullptr) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_R.Range + 400.0f, SDK::DamageType::Magical) : AIHeroClient();
            if (t.IsValid()) {
                const float rDmg = m_R.GetDamage(t);
                if (rDmg > t.Health()) {
                    m_R.Cast(t);
                } else if (p.Mana() > m_RMANA + m_EMANA &&
                           m_E.GetDamage(t) * 2.0f + rDmg > t.Health()) {
                    m_R.Cast(t);
                }
                if (p.Mana() > m_RMANA + m_EMANA + m_QMANA + m_WMANA && Combo()) {
                    m_R.Cast(t);
                }
            }

            if (FarmSpells() && GetBool("farmR")) {
                auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_R.Range);
                std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
                auto farm = m_R.GetCircularFarmLocation(baseList, kRwidth);
                if (farm.MinionsHit >= FarmMinions()) {
                    m_R.Cast(farm.Position);
                }
            }
        } else {
            // R already active; decide whether to detonate.
            if (FarmSpells() && GetBool("farmR")) {
                const auto allMinions = OktwCommon::GetMinions(m_rMissilePos, kRwidth);
                const auto mobs = OktwCommon::GetMinions(m_rMissilePos, kRwidth, false, true);
                if (!mobs.empty()) {
                    if (!GetBool("jungleR")) {
                        m_R.Cast();
                    }
                } else if (!allMinions.empty()) {
                    const float manaPct = p.MaxMana() > 0.0f
                        ? (p.Mana() * 100.0f / p.MaxMana()) : 0.0f;
                    const int manaThresh = GetSlider("Mana", 50);
                    if (allMinions.size() < 2 || manaPct < static_cast<float>(manaThresh)) {
                        m_R.Cast();
                    } else if (manaPct < static_cast<float>(manaThresh)) {
                        m_R.Cast();
                    }
                } else {
                    m_R.Cast();
                }
            } else if (!None() &&
                       (OktwCommon::CountEnemiesInRange(m_rMissilePos, 470.0f) == 0 ||
                        p.Mana() < m_EMANA + m_QMANA)) {
                m_R.Cast();
            }
        }
    }

    // -----------------------------------------------------------------------
    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), m_E.Range, false, true);
        if (mobs.empty()) return;

        const auto& mob = mobs.front();
        if (m_Q.IsReady() && GetBool("jungleQ")) {
            if (m_qMissile != nullptr) {
                if (m_qMissilePos.Distance(mob.ServerPosition()) < 230.0f) {
                    m_Q.Cast();
                }
            } else {
                m_Q.Cast(mob.ServerPosition());
            }
            return;
        }
        if (m_R.IsReady() && GetBool("jungleR") && m_rMissile == nullptr) {
            m_R.Cast(mob.ServerPosition());
            return;
        }
        if (m_E.IsReady() && GetBool("jungleE") && mob.HasBuff("chilled")) {
            m_E.Cast(mob);
            return;
        }
        if (m_W.IsReady() && GetBool("jungleW")) {
            // C#: W.Cast(mob.Position.Extend(Player.Position, 100));
            const Vector3 mobPos = mob.Position();
            const Vector3 playerPos = p.Position();
            const Vector3 dir = playerPos - mobPos;
            const float len = dir.Length();
            Vector3 castPos = mobPos;
            if (len > 0.001f) {
                castPos = { mobPos.x + dir.x * (100.0f / len),
                            mobPos.y,
                            mobPos.z + dir.z * (100.0f / len) };
            }
            m_W.Cast(castPos);
            return;
        }
    }

    // -----------------------------------------------------------------------
    void OnGameDraw() override {
        // Simplified: rely on SDK draw utilities for range circles.
        // TODO: honor qRange/wRange/eRange/rRange + onlyRdy toggles once the
        //       SDK exposes a draw-circle helper analogous to Utility.DrawCircle.
    }
};

} } // namespace Plugins::OKTW
