#include "SpecialEvadeSpell.h"
#include "EvadeSpell.h"

namespace EzEvade {

    // =========================================================================
    // LoadSpecialSpell
    // C# original: LoadSpecialSpell(EvadeSpellData spellData) (line 19-45)
    //
    //   public static void LoadSpecialSpell(EvadeSpellData spellData)
    //   {
    //       if (spellData.spellName == "EkkoEAttack")
    //           spellData.useSpellFunc = UseEkkoE2;
    //       if (spellData.spellName == "EkkoR")
    //           spellData.useSpellFunc = UseEkkoR;
    //       if (spellData.spellName == "EliseSpiderEInitial")
    //           spellData.useSpellFunc = UseRappel;
    //       if (spellData.spellName == "Pounce")
    //           spellData.useSpellFunc = UsePounce;
    //       if (spellData.spellName == "RivenTriCleave")
    //           spellData.useSpellFunc = UseBrokenWings;
    //   }
    // =========================================================================
    void SpecialEvadeSpell::LoadSpecialSpell(EvadeSpellData& spellData)
    {
        if (spellData.spellName == "EkkoEAttack")                                          // C# line 21
        {
            spellData.useSpellFunc = UseEkkoE2;                                            // C# line 23
        }

        if (spellData.spellName == "EkkoR")                                                // C# line 26
        {
            spellData.useSpellFunc = UseEkkoR;                                             // C# line 28
        }

        if (spellData.spellName == "EliseSpiderEInitial")                                  // C# line 31
        {
            spellData.useSpellFunc = UseRappel;                                            // C# line 33
        }

        if (spellData.spellName == "Pounce")                                               // C# line 36
        {
            spellData.useSpellFunc = UsePounce;                                            // C# line 38
        }

        if (spellData.spellName == "RivenTriCleave")                                       // C# line 41
        {
            spellData.useSpellFunc = UseBrokenWings;                                       // C# line 43
        }
    }

    // =========================================================================
    // UseRappel (Elise Spider E)
    // C# original: UseRappel(EvadeSpellData, bool) (line 47-62)
    //
    //   public static bool UseRappel(EvadeSpellData evadeSpell, bool process = true)
    //   {
    //       if (myHero.BaseSkinName != "Elise")
    //       {
    //           EvadeSpell.CastEvadeSpell(() => EvadeCommand.CastSpell(evadeSpell, myHero), process);
    //           return true;
    //       }
    //       if (myHero.BaseSkinName == "Elise")
    //       {
    //           if (myHero.Spellbook.CanUseSpell(SpellSlot.R) == SpellState.Ready)
    //               myHero.Spellbook.CastSpell(SpellSlot.R);
    //       }
    //       return false;
    //   }
    // =========================================================================
    bool SpecialEvadeSpell::UseRappel(const EvadeSpellData& evadeSpell, bool process)
    {
        auto myHero = SDK::ObjectManager::GetLocalPlayer();                                              // C# line 17: myHero
        if (!myHero.IsValid()) return false;

        // TODO: Get BaseSkinName from hero object
        std::string baseSkinName = "Unknown"; // Placeholder

        if (baseSkinName != "Elise")                                                       // C# line 49
        {
            EvadeSpell::CastEvadeSpell(                                                    // C# line 51
                [=]() { EvadeCommand::CastSpell(evadeSpell); },
                process);
            return true;                                                                   // C# line 52
        }

        if (baseSkinName == "Elise")                                                       // C# line 55
        {
            // if (myHero.Spellbook.CanUseSpell(SpellSlot.R) == SpellState.Ready)
            //     myHero.Spellbook.CastSpell(SpellSlot.R);                                // C# line 57-58
            // TODO: Cast R via SpellBook
        }

        return false;                                                                      // C# line 61
    }

    // =========================================================================
    // UsePounce (Nidalee Spider W)
    // C# original: UsePounce(EvadeSpellData, bool) (line 64-77)
    //
    //   public static bool UsePounce(EvadeSpellData evadeSpell, bool process = true)
    //   {
    //       if (myHero.BaseSkinName != "Nidalee")
    //       {
    //           var posInfo = EvadeHelper.GetBestPositionDash(evadeSpell);
    //           if (posInfo != null)
    //           {
    //               EvadeSpell.CastEvadeSpell(() => EvadeCommand.CastSpell(evadeSpell), process);
    //               return true;
    //           }
    //       }
    //       return false;
    //   }
    // =========================================================================
    bool SpecialEvadeSpell::UsePounce(const EvadeSpellData& evadeSpell, bool process)
    {
        auto myHero = SDK::ObjectManager::GetLocalPlayer();                                              // C# line 17
        if (!myHero.IsValid()) return false;

        // TODO: Get BaseSkinName
        std::string baseSkinName = "Unknown";

        if (baseSkinName != "Nidalee")                                                     // C# line 66
        {
            auto* posInfo = EvadeHelper::GetBestPositionDash(evadeSpell);                   // C# line 68
            if (posInfo != nullptr)                                                        // C# line 69
            {
                EvadeSpell::CastEvadeSpell(                                                // C# line 71
                    [=]() { EvadeCommand::CastSpell(evadeSpell); },
                    process);
                return true;                                                               // C# line 72
            }
        }

        return false;                                                                      // C# line 76
    }

    // =========================================================================
    // UseBrokenWings (Riven Q)
    // C# original: UseBrokenWings(EvadeSpellData, bool) (line 79-90)
    //
    //   public static bool UseBrokenWings(EvadeSpellData evadeSpell, bool process = false)
    //   {
    //       var posInfo = EvadeHelper.GetBestPositionDash(evadeSpell);
    //       if (posInfo != null)
    //       {
    //           EvadeCommand.MoveTo(posInfo.position);
    //           DelayAction.Add(50, () => EvadeSpell.CastEvadeSpell(() => EvadeCommand.CastSpell(evadeSpell), process));
    //           return true;
    //       }
    //       return false;
    //   }
    // =========================================================================
    bool SpecialEvadeSpell::UseBrokenWings(const EvadeSpellData& evadeSpell, bool process)
    {
        auto* posInfo = EvadeHelper::GetBestPositionDash(evadeSpell);                       // C# line 81
        if (posInfo != nullptr)                                                            // C# line 82
        {
            EvadeCommand::MoveTo(posInfo->position);                                       // C# line 84
            DelayAction::Add(50, [=]() {                                                   // C# line 85
                EvadeSpell::CastEvadeSpell(
                    [=]() { EvadeCommand::CastSpell(evadeSpell); },
                    process);
            });
            return true;                                                                   // C# line 86
        }

        return false;                                                                      // C# line 89
    }

    // =========================================================================
    // UseEkkoE2 (Ekko E attack part)
    // C# original: UseEkkoE2(EvadeSpellData, bool) (line 93-107)
    //
    //   public static bool UseEkkoE2(EvadeSpellData evadeSpell, bool process = true)
    //   {
    //       if (myHero.HasBuff("ekkoeattackbuff"))
    //       {
    //           var posInfo = EvadeHelper.GetBestPositionTargetedDash(evadeSpell);
    //           if (posInfo != null && posInfo.target != null)
    //           {
    //               EvadeSpell.CastEvadeSpell(() => EvadeCommand.Attack(evadeSpell, posInfo.target), process);
    //               return true;
    //           }
    //       }
    //       return false;
    //   }
    // =========================================================================
    bool SpecialEvadeSpell::UseEkkoE2(const EvadeSpellData& evadeSpell, bool process)
    {
        auto myHero = SDK::ObjectManager::GetLocalPlayer();                                              // C# line 17
        if (!myHero.IsValid()) return false;

        // if (myHero.HasBuff("ekkoeattackbuff"))
        // TODO: Check buff via BuffManager                                                // C# line 95
        bool hasBuff = false; // Placeholder

        if (hasBuff)                                                                       // C# line 95
        {
            auto* posInfo = EvadeHelper::GetBestPositionTargetedDash(evadeSpell);           // C# line 97
            if (posInfo != nullptr && posInfo->target != nullptr)                           // C# line 98
            {
                SDK::GameObject* tgt = posInfo->target;                                     // capture
                EvadeSpell::CastEvadeSpell(                                                // C# line 100
                    [=]() { EvadeCommand::Attack(evadeSpell, tgt); },
                    process);
                return true;                                                               // C# line 102
            }
        }

        return false;                                                                      // C# line 106
    }

    // =========================================================================
    // UseEkkoR (Ekko Chronobreak)
    // C# original: UseEkkoR(EvadeSpellData, bool) (line 109-127)
    //
    //   public static bool UseEkkoR(EvadeSpellData evadeSpell, bool process = true)
    //   {
    //       foreach (var obj in ObjectManager.Get<Obj_AI_Minion>())
    //       {
    //           if (obj != null && obj.IsValid && !obj.IsDead && obj.Name == "Ekko" && obj.IsAlly)
    //           {
    //               Vector2 blinkPos = obj.ServerPosition.To2D();
    //               if (!blinkPos.CheckDangerousPos(10))
    //               {
    //                   EvadeSpell.CastEvadeSpell(() => EvadeCommand.CastSpell(evadeSpell), process);
    //                   return true;
    //               }
    //           }
    //       }
    //       return false;
    //   }
    // =========================================================================
    bool SpecialEvadeSpell::UseEkkoR(const EvadeSpellData& evadeSpell, bool process)
    {
        // Iterate minions to find Ekko's clone object
        bool found = false;
        SDK::ObjectManager::ForEach([&](SDK::GameObject& obj) {
            if (found) return;
            if (!obj.IsValid() || !obj.IsAlive()) return;
            if (!obj.IsMinion()) return;
            // TODO: Check obj.GetName() == "Ekko" && obj is ally
            // For now, this is a placeholder — Ekko R clone detection
        });

        return false;
    }

} // namespace EzEvade
