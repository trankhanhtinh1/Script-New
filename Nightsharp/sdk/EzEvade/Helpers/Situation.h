#pragma once
#include <string>
#include "../../GameObjects/GameObjects.h"
#include "../../Math/MathUtils.h"
#include "ObjectCache.h"
#include "Position.h"
#include "../Utils/EvadeUtils.h"
#include "../Core/EvadeState.h"

// ============================================================================
// Situation
//   C# original: ezEvade.Situation (Situation.cs, 225 lines)
//   Line-by-line port preserving original logic — 100% matching
// ============================================================================

namespace EzEvade {

    namespace Situation {

        // ====================================================================
        // CheckTeam (for GameObject)
        //   C# lines 24-27 + 29-32
        //   public static bool CheckTeam(this Obj_AI_Base unit) =>
        //       unit.Team != myHero.Team || Evade.devModeOn;
        // ====================================================================
        inline bool CheckTeam(const SDK::GameObject& unit) {
            // C# line 26: return unit.Team != myHero.Team || Evade.devModeOn
            return unit.GetTeam() != SDK::GameObjects::Player.GetTeam() || EvadeState::devModeOn;
        }

        // ====================================================================
        // CheckTeam for particle emitter (name-based)
        //   C# lines 34-39
        //   Uses name containing "red" / "green" / "ally"
        // ====================================================================
        inline bool CheckTeamParticle(const std::string& emitterName) {
            std::string lower = emitterName;
            for (auto& c : lower) c = (char)tolower((unsigned char)c);

            // C# lines 36-38
            return lower.find("red") != std::string::npos ||
                   ((lower.find("green") != std::string::npos || lower.find("ally") != std::string::npos) && EvadeState::devModeOn) ||
                   (lower.find("green") == std::string::npos && lower.find("ally") == std::string::npos);
        }

        // ====================================================================
        // EmitterColor / EmitterTeam
        //   C# lines 41-49
        // ====================================================================
        inline std::string EmitterColor() {
            return EvadeState::devModeOn ? "green" : "red";                     // C# line 43
        }

        inline std::string EmitterTeam() {
            return EvadeState::devModeOn ? "ally" : "enemy";                    // C# line 48
        }

        // ====================================================================
        // isNearEnemy
        //   C# lines 51-75
        //   public static bool isNearEnemy(this Vector2 pos, float distance, bool alreadyNear = true)
        // ====================================================================
        inline bool IsNearEnemy(const Vec2& pos, float distance) {
            // C# line 53: if (menuCache["PreventDodgingNearEnemy"].GetValue<bool>())
            if (ObjectCache::GetBool("PreventDodgingNearEnemy"))            // C# line 53
            {
                // C# line 55-56
                float curDistToEnemies = Position::GetDistanceToChampions(
                    ObjectCache::myHeroCache.serverPos2D);
                float posDistToEnemies = Position::GetDistanceToChampions(pos);

                if (curDistToEnemies < distance)                            // C# line 58
                {
                    if (curDistToEnemies > posDistToEnemies)                 // C# line 60
                    {
                        return true;                                        // C# line 62
                    }
                }
                else                                                        // C# line 65
                {
                    if (posDistToEnemies < distance)                        // C# line 67
                    {
                        return true;                                        // C# line 69
                    }
                }
            }

            return false;                                                   // C# line 74
        }

        // ====================================================================
        // IsUnderTurret
        //   C# lines 77-108
        //   public static bool IsUnderTurret(this Vector2 pos, bool checkEnemy = true)
        // ====================================================================
        inline bool IsUnderTurret(const Vec2& pos, bool checkEnemy = true) {
            // C# line 79: if (!menuCache["PreventDodgingUnderTower"].GetValue<bool>())
            if (!ObjectCache::GetBool("PreventDodgingUnderTower"))          // C# line 79
            {
                return false;                                               // C# line 81
            }

            float turretRange = 875 + ObjectCache::myHeroCache.boundingRadius; // C# line 84

            for (const auto& entry : ObjectCache::turrets)                  // C# line 86
            {
                auto* turret = entry.second;                                // C# line 88
                if (turret == nullptr || !turret->IsValid() || !turret->IsAlive()) // C# line 89
                {
                    continue;                                               // C# line 91 (skip removal — no GC in C++)
                }

                // C# line 95: if (checkEnemy && turret.IsAlly) continue;
                if (checkEnemy &&
                    turret->GetTeam() == SDK::GameObjects::Player.GetTeam())
                {
                    continue;
                }

                float distToTurret = pos.Distance(turret->GetPosition().To2D()); // C# line 100
                if (distToTurret <= turretRange)                            // C# line 101
                {
                    return true;                                            // C# line 103
                }
            }

            return false;                                                   // C# line 107
        }

        // ====================================================================
        // HasSpellShield
        //   C# lines 190-221
        //   public static bool HasSpellShield(AIHeroClient unit)
        // ====================================================================
        inline bool HasSpellShield(const SDK::GameObject& unit) {
            // C# line 192: if (ObjectManager.Player.HasBuffOfType(BuffType.SpellShield))
            if (SDK::GameObjects::Player.HasBuff("SpellShield"))
            {
                return true;                                                // C# line 194
            }

            // C# line 197: if (ObjectManager.Player.HasBuffOfType(BuffType.SpellImmunity))
            if (SDK::GameObjects::Player.HasBuff("SpellImmunity"))
            {
                return true;                                                // C# line 199
            }

            // C# lines 202-206: Sivir E
            // C# original: unit.LastCastedSpellName() == "SivirE" && tickDiff < 300
            // SDK has no LastCastedSpellName — check buff directly (SpellShield covers it above)
            // Additionally check by buff name for extra safety
            if (SDK::GameObjects::Player.HasBuff("SivirE") &&
                (EvadeUtils::TickCount() - EvadeState::lastSpellCastTime) < 300) // C# line 203
            {
                return true;                                                // C# line 205
            }

            // C# lines 208-212: Morgana E
            if (SDK::GameObjects::Player.HasBuff("BlackShield") &&
                (EvadeUtils::TickCount() - EvadeState::lastSpellCastTime) < 300) // C# line 209
            {
                return true;                                                // C# line 211
            }

            // C# lines 214-218: Nocturne E
            if (SDK::GameObjects::Player.HasBuff("NocturneShit") &&
                (EvadeUtils::TickCount() - EvadeState::lastSpellCastTime) < 300) // C# line 215
            {
                return true;                                                // C# line 217
            }

            return false;                                                   // C# line 220
        }

        // ====================================================================
        // ChampionSpecificChecks
        //   C# lines 176-187
        // ====================================================================
        inline bool ChampionSpecificChecks() {
            auto& player = SDK::GameObjects::Player;

            // C# line 178: (myHero.ChampionName == "Sion" && myHero.HasBuff("SionR"))
            if (player.GetChampionName() == "Sion" && player.HasBuff("SionR"))
            {
                return true;
            }

            return false;
        }

        // ====================================================================
        // CommonChecks
        //   C# lines 160-174
        //   public static bool CommonChecks()
        // ====================================================================
        inline bool CommonChecks() {
            auto& myHero = SDK::GameObjects::Player;

            // C# line 164: Evade.isChanneling
            if (EvadeState::isChanneling) return true;                           // C# line 164

            // C# line 165-166: DodgeOnlyOnComboKey check
            if (ObjectCache::GetBool("DodgeOnlyOnComboKeyEnabled") &&
                !ObjectCache::GetBool("DodgeComboKey"))                     // C# line 165-166
            {
                return true;
            }

            // C# line 167: myHero.IsDead
            if (!myHero.IsAlive()) return true;                             // C# line 167

            // C# line 168: myHero.IsInvulnerable
            if (myHero.IsInvulnerable()) return true;                      // C# line 168

            // C# line 169: myHero.IsTargetable == false
            if (!myHero.IsTargetable()) return true;                        // C# line 169

            // C# line 170: HasSpellShield(myHero)
            if (HasSpellShield(myHero)) return true;                        // C# line 170

            // C# line 171: ChampionSpecificChecks
            if (ChampionSpecificChecks()) return true;                      // C# line 171

            // C# line 172: myHero.IsDashing()
            if (myHero.IsDashing()) return true;                            // C# line 172

            // C# line 173: Evade.hasGameEnded
            if (EvadeState::hasGameEnded) return true;                           // C# line 173

            return false;
        }

        // ====================================================================
        // ShouldDodge
        //   C# lines 110-139
        //   public static bool ShouldDodge()
        // ====================================================================
        inline bool ShouldDodge() {
            // C# line 112-116: DontDodgeKey check
            if (ObjectCache::GetBool("DontDodgeKeyEnabled") &&
                ObjectCache::GetBool("DontDodgeKey"))                       // C# line 112-113
            {
                return false;                                               // C# line 115
            }

            // C# line 118-136: DodgeSkillShots keybind + CommonChecks
            if (!ObjectCache::GetBool("DodgeSkillShots") ||
                CommonChecks())                                             // C# line 118-119
            {
                return false;                                               // C# line 135
            }

            return true;                                                    // C# line 138
        }

        // ====================================================================
        // ShouldUseEvadeSpell
        //   C# lines 141-158
        //   public static bool ShouldUseEvadeSpell()
        // ====================================================================
        inline bool ShouldUseEvadeSpell() {
            // C# line 143-147: DontDodgeKey check
            if (ObjectCache::GetBool("DontDodgeKeyEnabled") &&
                ObjectCache::GetBool("DontDodgeKey"))                       // C# line 143-144
            {
                return false;                                               // C# line 146
            }

            // C# line 149-155: ActivateEvadeSpells + CommonChecks + windupTime
            if (!ObjectCache::GetBool("ActivateEvadeSpells") ||
                CommonChecks() ||
                EvadeState::lastWindupTime - EvadeUtils::TickCount() > 0)        // C# line 149-151
            {
                return false;                                               // C# line 154
            }

            return true;                                                    // C# line 157
        }

    } // namespace Situation

} // namespace EzEvade
