#include "EvadeSpell.h"
#include "SpecialEvadeSpell.h"
#include "EvadeSpellDatabase.h"
#include "../Core/Evade.h"
#include "../Core/EvadeHelper.h"

namespace EzEvade {

    // =========================================================================
    // Static member definitions
    // C# original: (line 20-22)
    // =========================================================================
    std::vector<EvadeSpellData> EvadeSpell::evadeSpells;
    std::vector<EvadeSpellData> EvadeSpell::itemSpells;
    EvadeCommand EvadeSpell::lastSpellEvadeCommand;

    // Note: SpellDetector::spells, Evade::*, EvadeHelper::*
    // are now provided by their real .cpp files (SpellDetector.cpp, Evade.cpp, etc.)
    // which are linked into the project.

    // Items stubs — no real implementation yet
    namespace Items {
        bool HasItem(int /*itemId*/) { return false; /* TODO */ }
        bool CanUseItem(int /*itemId*/) { return false; /* TODO */ }
        void UseItem(int /*itemId*/) { /* TODO */ }
    }

    // =========================================================================
    // Constructor
    // C# original: EvadeSpell(Menu mainMenu) (line 27-38)
    //
    //   public EvadeSpell(Menu mainMenu)
    //   {
    //       menu = mainMenu;
    //       evadeSpellMenu = new Menu("Evade Spells", "EvadeSpells");
    //       menu.AddSubMenu(evadeSpellMenu);
    //       LoadEvadeSpellList();
    //       DelayAction.Add(100, () => CheckForItems());
    //   }
    // =========================================================================
    EvadeSpell::EvadeSpell()
    {
        // menu = mainMenu;                          // C# line 29 — menu system TODO
        // evadeSpellMenu = new Menu(...)             // C# line 33 — menu creation TODO
        // menu.AddSubMenu(evadeSpellMenu);           // C# line 34

        LoadEvadeSpellList();                         // C# line 36
        DelayAction::Add(100, []() { CheckForItems(); }); // C# line 37
    }

    // =========================================================================
    // CheckDashing
    // C# original: CheckDashing() (line 45-55)
    //
    //   public static void CheckDashing()
    //   {
    //       if (EvadeUtils.TickCount - lastSpellEvadeCommand.timestamp < 250 && myHero.IsDashing()
    //           && lastSpellEvadeCommand.evadeSpellData.evadeType == EvadeType.Dash)
    //       {
    //           var dashInfo = myHero.GetDashInfo();
    //           lastSpellEvadeCommand.targetPosition = dashInfo.EndPos;
    //       }
    //   }
    // =========================================================================
    void EvadeSpell::CheckDashing()
    {
        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        if (!myHero.IsValid()) return;

        if (EvadeUtils::TickCount() - lastSpellEvadeCommand.timestamp < 250              // C# line 47
            /* && myHero.IsDashing() */                                                    // TODO: IsDashing check
            && lastSpellEvadeCommand.evadeSpellData.evadeType == EvadeType::Dash)          // C# line 48
        {
            // var dashInfo = myHero.GetDashInfo();                                        // C# line 50 — TODO
            // lastSpellEvadeCommand.targetPosition = dashInfo.EndPos;                     // C# line 53 — TODO
        }
    }

    // =========================================================================
    // CheckForItems
    // C# original: CheckForItems() (line 57-73)
    //
    //   private static void CheckForItems()
    //   {
    //       foreach (var spell in itemSpells)
    //       {
    //           var hasItem = Items.HasItem((int)spell.itemID);
    //           if (hasItem && !evadeSpells.Exists(s => s.spellName == spell.spellName))
    //           {
    //               evadeSpells.Add(spell);
    //               var newSpellMenu = CreateEvadeSpellMenu(spell);
    //               ObjectCache.menuCache.AddMenuToCache(newSpellMenu);
    //           }
    //       }
    //       DelayAction.Add(5000, () => CheckForItems());
    //   }
    // =========================================================================
    void EvadeSpell::CheckForItems()
    {
        for (auto& spell : itemSpells)                                                     // C# line 59
        {
            bool hasItem = Items::HasItem(spell.itemID);                                   // C# line 61

            // Check if spell not already in evadeSpells list
            bool exists = false;                                                           // C# line 63
            for (auto& s : evadeSpells) {
                if (s.spellName == spell.spellName) { exists = true; break; }
            }

            if (hasItem && !exists)                                                        // C# line 63
            {
                evadeSpells.push_back(spell);                                              // C# line 65

                CreateEvadeSpellMenu(spell);                                               // C# line 67
                // ObjectCache::menuCache.AddMenuToCache(newSpellMenu);                    // C# line 68 — TODO
            }
        }

        DelayAction::Add(5000, []() { CheckForItems(); });                                 // C# line 72
    }

    // =========================================================================
    // CreateEvadeSpellMenu
    // C# original: CreateEvadeSpellMenu(EvadeSpellData spell) (line 75-102)
    //
    // Creates a menu entry for an evade spell with settings:
    //   - Use Spell (bool)
    //   - Danger Level (StringList: Low/Normal/High/Extreme)
    //   - Spell Mode (StringList: Undodgeable/Activation Time/Always)
    // =========================================================================
    void EvadeSpell::CreateEvadeSpellMenu(const EvadeSpellData& spell)
    {
        // std::string menuName = spell.name + " (" + ...spellKey... + ") Settings"; // C# line 78
        // if (spell.isItem) { menuName = spell.name + " Settings"; }                // C# line 80-83

        // Menu newSpellMenu = new Menu(menuName, ...);                               // C# line 85
        // newSpellMenu.AddItem(... "Use Spell" ...);                                 // C# line 86
        // newSpellMenu.AddItem(... "Danger Level" ...);                              // C# line 88-89
        // newSpellMenu.AddItem(... "Spell Mode" ...);                                // C# line 95-97
        // evadeSpellMenu.AddSubMenu(newSpellMenu);                                   // C# line 99

        // TODO: Implement when MenuUI system is ready
    }

    // =========================================================================
    // GetDefaultSpellMode
    // C# original: GetDefaultSpellMode(EvadeSpellData spell) (line 104-112)
    //
    //   public static int GetDefaultSpellMode(EvadeSpellData spell)
    //   {
    //       if (spell.dangerlevel > 3) { return 0; }
    //       return 1;
    //   }
    // =========================================================================
    int EvadeSpell::GetDefaultSpellMode(const EvadeSpellData& spell)
    {
        if (spell.dangerlevel > 3)                                                         // C# line 106
        {
            return 0;                                                                      // C# line 108
        }

        return 1;                                                                          // C# line 111
    }

    // =========================================================================
    // PreferEvadeSpell
    // C# original: PreferEvadeSpell() (line 114-133)
    //
    //   public static bool PreferEvadeSpell()
    //   {
    //       if (!Situation.ShouldUseEvadeSpell()) return false;
    //       foreach (var entry in SpellDetector.spells)
    //       {
    //           Spell spell = entry.Value;
    //           if (!ObjectCache.myHeroCache.serverPos2D.InSkillShot(spell, ObjectCache.myHeroCache.boundingRadius))
    //               continue;
    //           if (ActivateEvadeSpell(spell, true)) return true;
    //       }
    //       return false;
    //   }
    // =========================================================================
    bool EvadeSpell::PreferEvadeSpell()
    {
        if (!Situation::ShouldUseEvadeSpell())                                             // C# line 116
            return false;                                                                  // C# line 117

        for (auto& entry : SpellDetector::spells)                                          // C# line 119
        {
            Spell& spell = entry.second;                                                   // C# line 121

            if (!Position::InSkillShot(ObjectCache::myHeroCache.serverPos2D,                         // C# line 123
                             spell, ObjectCache::myHeroCache.boundingRadius))
                continue;                                                                  // C# line 124

            if (ActivateEvadeSpell(spell, true))                                           // C# line 126
            {
                return true;                                                               // C# line 128
            }
        }

        return false;                                                                      // C# line 132
    }

    // =========================================================================
    // UseEvadeSpell
    // C# original: UseEvadeSpell() (line 135-163)
    //
    //   public static void UseEvadeSpell()
    //   {
    //       if (!Situation.ShouldUseEvadeSpell()) return;
    //       if (EvadeUtils.TickCount - lastSpellEvadeCommand.timestamp < 1000) return;
    //       foreach (var entry in SpellDetector.spells)
    //       {
    //           Spell spell = entry.Value;
    //           if (ShouldActivateEvadeSpell(spell))
    //           {
    //               if (ActivateEvadeSpell(spell)) { Evade.SetAllUndodgeable(); return; }
    //           }
    //       }
    //   }
    // =========================================================================
    void EvadeSpell::UseEvadeSpell()
    {
        if (!Situation::ShouldUseEvadeSpell())                                             // C# line 137
        {
            return;                                                                        // C# line 139
        }

        if (EvadeUtils::TickCount() - lastSpellEvadeCommand.timestamp < 1000)              // C# line 144
        {
            return;                                                                        // C# line 146
        }

        for (auto& entry : SpellDetector::spells)                                          // C# line 149
        {
            Spell& spell = entry.second;                                                   // C# line 151

            if (ShouldActivateEvadeSpell(spell))                                           // C# line 153
            {
                if (ActivateEvadeSpell(spell))                                             // C# line 155
                {
                    Evade::SetAllUndodgeable();                                            // C# line 157
                    return;                                                                // C# line 158
                }
            }
        }
    }

    // =========================================================================
    // ActivateEvadeSpell
    // C# original: ActivateEvadeSpell(Spell spell, bool checkSpell) (line 165-387)
    //
    // Contains the main evade spell activation logic:
    //   - Filters spells by availability, danger level, menu settings
    //   - Handles different EvadeTypes: Blink, Dash, WindWall, SpellShield,
    //     MovementSpeedBuff, Special
    // =========================================================================
    bool EvadeSpell::ActivateEvadeSpell(Spell& spell, bool checkSpell)
    {
        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        if (!myHero.IsValid()) return false;

        // if (spell.info.spellName.Contains("_trap")) return false;
        if (spell.info.spellName.find("_trap") != std::string::npos)                       // C# line 167
            return false;                                                                  // C# line 168

        // var sortedEvadeSpells = evadeSpells.OrderBy(s => s.dangerlevel);
        auto sortedEvadeSpells = evadeSpells;                                              // C# line 170
        std::sort(sortedEvadeSpells.begin(), sortedEvadeSpells.end(),
            [](const EvadeSpellData& a, const EvadeSpellData& b) {
                return a.dangerlevel < b.dangerlevel;
            });

        // var extraDelayBuffer = ObjectCache.menuCache.cache["ExtraPingBuffer"].GetValue<Slider>().Value;
        int extraDelayBuffer = ObjectCache::GetSlider("ExtraPingBuffer");                  // C# line 172
        // float spellActivationTime = ObjectCache.menuCache.cache["SpellActivationTime"]... + ObjectCache.gamePing + extraDelayBuffer;
        float spellActivationTime = (float)ObjectCache::GetSlider("SpellActivationTime")
            + ObjectCache::gamePing + (float)extraDelayBuffer;                             // C# line 173

        // if (ObjectCache.menuCache.cache["CalculateWindupDelay"].GetValue<bool>())
        // {
        //     var extraWindupDelay = Evade.lastWindupTime - EvadeUtils.TickCount;
        //     if (extraWindupDelay > 0) { return false; }
        // }
        // TODO: Implement CalculateWindupDelay menu check                                 // C# line 175-182
        {
            float extraWindupDelay = Evade::lastWindupTime - EvadeUtils::TickCount();      // C# line 177
            if (extraWindupDelay > 0)                                                      // C# line 178
            {
                // return false;                                                           // C# line 180 — disabled until menu ready
            }
        }

        for (auto& evadeSpell : sortedEvadeSpells)                                         // C# line 184
        {
            bool processSpell = true;                                                      // C# line 186

            // --- Spell availability checks (C# line 188-195) ---
            // if (menu UseEvadeSpell == false || danger mismatch || can't use spell || wrong spell name)
            //     continue;

            if (GetSpellDangerLevel(evadeSpell) > spell.GetSpellDangerLevel())              // C# line 189
            {
                continue;
            }

            // (evadeSpell.isItem == false && myHero.Spellbook.CanUseSpell(evadeSpell.spellKey) != SpellState.Ready)
            // TODO: Check spell ready state via SpellBook                                 // C# line 190

            // (evadeSpell.isItem && !Items.CanUseItem((int)evadeSpell.itemID))
            if (evadeSpell.isItem && !Items::CanUseItem(evadeSpell.itemID))                 // C# line 191
            {
                continue;
            }

            // (evadeSpell.checkSpellName && myHero.Spellbook.GetSpell(evadeSpell.spellKey).Name != evadeSpell.spellName)
            // TODO: Check actual spell name via SpellBook                                 // C# line 192

            // --- Evade timing calculation (C# line 197-200) ---
            float evadeTime, spellHitTime;
            spell.CanHeroEvade(myHero, evadeTime, spellHitTime);                           // C# line 198

            float finalEvadeTime = (spellHitTime - evadeTime);                             // C# line 200

            if (checkSpell)                                                                // C# line 202
            {
                // var mode = ObjectCache.menuCache.cache[evadeSpell.name + "EvadeSpellMode"]...SelectedIndex;
                int mode = 1; // TODO: read from menu ObjectCache::GetComboBox(evadeSpell.name + "EvadeSpellMode")  // C# line 204-205
                              // Default 1 = "Activation Time" mode (0=Undodgeable would always skip)

                if (mode == 0)                                                             // C# line 207
                {
                    continue;                                                              // C# line 209
                }
                else if (mode == 1)                                                        // C# line 211
                {
                    if (spellActivationTime < finalEvadeTime)                              // C# line 213
                    {
                        continue;                                                          // C# line 215
                    }
                }
            }
            else                                                                           // C# line 219
            {
                // if (evadeSpell.spellDelay <= 50 && evadeSpell.evadeType != EvadeType.Dash)
                if (evadeSpell.spellDelay <= 50 && evadeSpell.evadeType != EvadeType::Dash) // C# line 222
                {
                    // var path = myHero.Path;
                    // TODO: Get hero path from AiManager                                  // C# line 224
                    // if (path.Length > 0)                                                 // C# line 225
                    // {
                    //     var movePos = path[path.Length-1].To2D();
                    //     var posInfo = EvadeHelper.CanHeroWalkToPos(movePos, ...);
                    //     if (GetSpellDangerLevel(evadeSpell) > posInfo.posDangerLevel) continue;
                    // }
                }
            }

            // --- Non-Dash spells: delay processing check (C# line 238-247) ---
            if (evadeSpell.evadeType != EvadeType::Dash                                    // C# line 238
                && spellHitTime > evadeSpell.spellDelay + 100 + SDK::Game::GetPing()
                    + (float)ObjectCache::GetSlider("ExtraPingBuffer"))                     // C# line 239
            {
                processSpell = false;                                                      // C# line 241

                if (checkSpell == false)                                                    // C# line 243
                {
                    continue;                                                              // C# line 245
                }
            }

            // =====================================================================
            // Handle each EvadeType
            // =====================================================================

            // --- Special spells (C# line 249-260) ---
            if (evadeSpell.isSpecial)                                                      // C# line 249
            {
                if (evadeSpell.useSpellFunc != nullptr)                                    // C# line 251
                {
                    if (evadeSpell.useSpellFunc(evadeSpell, processSpell))                  // C# line 253
                    {
                        return true;                                                       // C# line 255
                    }
                }

                continue;                                                                  // C# line 259
            }
            // --- Blink (C# line 261-283) ---
            else if (evadeSpell.evadeType == EvadeType::Blink)                             // C# line 261
            {
                if (evadeSpell.castType == CastType::Position)                             // C# line 263
                {
                    auto* posInfo = EvadeHelper::GetBestPositionBlink();                    // C# line 265
                    if (posInfo != nullptr)                                                 // C# line 266
                    {
                        Vec2 pos = posInfo->position;                                      // capture
                        CastEvadeSpell([=]() { EvadeCommand::CastSpell(evadeSpell, pos); }, // C# line 268
                                       processSpell);
                        return true;                                                       // C# line 270
                    }
                }
                else if (evadeSpell.castType == CastType::Target)                          // C# line 273
                {
                    auto* posInfo = EvadeHelper::GetBestPositionTargetedDash(evadeSpell);   // C# line 275
                    if (posInfo != nullptr && posInfo->target != nullptr                    // C# line 276
                        && posInfo->posDangerLevel == 0)
                    {
                        SDK::GameObject* tgt = posInfo->target;                             // capture
                        CastEvadeSpell([=]() { EvadeCommand::CastSpell(evadeSpell, tgt); }, // C# line 278
                                       processSpell);
                        return true;                                                       // C# line 280
                    }
                }
            }
            // --- Dash (C# line 284-315) ---
            else if (evadeSpell.evadeType == EvadeType::Dash)                              // C# line 284
            {
                if (evadeSpell.castType == CastType::Position)                             // C# line 286
                {
                    auto* posInfo = EvadeHelper::GetBestPositionDash(evadeSpell);            // C# line 288
                    if (posInfo != nullptr && CompareEvadeOption(posInfo, checkSpell))       // C# line 289
                    {
                        if (evadeSpell.isReversed)                                         // C# line 291
                        {
                            // var dir = (posInfo.position - serverPos2D).Normalized();
                            Vec2 diff = posInfo->position - ObjectCache::myHeroCache.serverPos2D;
                            float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);      // C# line 293
                            Vec2 dir = (len > 0) ? Vec2{ diff.x / len, diff.y / len } : Vec2{ 0, 0 };
                            float range = ObjectCache::myHeroCache.serverPos2D.Distance(posInfo->position); // C# line 294
                            Vec2 pos = ObjectCache::myHeroCache.serverPos2D - dir * range;  // C# line 295

                            posInfo->position = pos;                                        // C# line 297
                        }

                        Vec2 pos = posInfo->position;                                       // capture
                        CastEvadeSpell([=]() { EvadeCommand::CastSpell(evadeSpell, pos); }, // C# line 300
                                       processSpell);
                        return true;                                                       // C# line 302
                    }
                }
                else if (evadeSpell.castType == CastType::Target)                          // C# line 305
                {
                    auto* posInfo = EvadeHelper::GetBestPositionTargetedDash(evadeSpell);    // C# line 307
                    if (posInfo != nullptr && posInfo->target != nullptr                     // C# line 308
                        && posInfo->posDangerLevel == 0)
                    {
                        SDK::GameObject* tgt = posInfo->target;                             // capture
                        CastEvadeSpell([=]() { EvadeCommand::CastSpell(evadeSpell, tgt); }, // C# line 310
                                       processSpell);
                        return true;                                                       // C# line 312
                    }
                }
            }
            // --- WindWall (C# line 316-326) ---
            else if (evadeSpell.evadeType == EvadeType::WindWall)                          // C# line 316
            {
                if (spell.hasProjectile() || evadeSpell.spellName == "FioraW")             // C# line 318
                {
                    // var dir = (spell.startPos - serverPos2D).Normalized();
                    Vec2 diff = spell.startPos - ObjectCache::myHeroCache.serverPos2D;
                    float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);               // C# line 320
                    Vec2 dir = (len > 0) ? Vec2{ diff.x / len, diff.y / len } : Vec2{ 0, 0 };
                    Vec2 pos = ObjectCache::myHeroCache.serverPos2D + dir * 100;            // C# line 321

                    CastEvadeSpell([=]() { EvadeCommand::CastSpell(evadeSpell, pos); },     // C# line 323
                                   processSpell);
                    return true;                                                            // C# line 324
                }
            }
            // --- SpellShield (C# line 327-346) ---
            else if (evadeSpell.evadeType == EvadeType::SpellShield)                       // C# line 327
            {
                if (evadeSpell.isItem)                                                     // C# line 329
                {
                    int itemId = evadeSpell.itemID;                                         // capture
                    CastEvadeSpell([=]() { Items::UseItem(itemId); }, processSpell);        // C# line 331
                    return true;                                                            // C# line 332
                }

                if (evadeSpell.castType == CastType::Target)                               // C# line 335
                {
                    CastEvadeSpell([=]() {
                        // CastSpell with target — need to pass via pointer
                        // myHero is a copy; this is safe
                        EvadeCommand::CastSpell(evadeSpell);
                    },  // C# line 337
                                   processSpell);
                    return true;                                                            // C# line 338
                }

                if (evadeSpell.castType == CastType::Self)                                 // C# line 341
                {
                    CastEvadeSpell([=]() { EvadeCommand::CastSpell(evadeSpell); },          // C# line 343
                                   processSpell);
                    return true;                                                            // C# line 344
                }
            }
            // --- MovementSpeedBuff (C# line 347-383) ---
            else if (evadeSpell.evadeType == EvadeType::MovementSpeedBuff)                 // C# line 347
            {
                if (evadeSpell.isItem)                                                     // C# line 349
                {
                    auto* posInfo = EvadeHelper::GetBestPosition();                         // C# line 351
                    if (posInfo != nullptr)                                                 // C# line 352
                    {
                        int itemId = evadeSpell.itemID;                                     // capture
                        Vec2 pos = posInfo->position;                                       // capture
                        CastEvadeSpell([=]() { Items::UseItem(itemId); }, processSpell);    // C# line 354
                        DelayAction::Add(5, [=]() { EvadeCommand::MoveTo(pos); });          // C# line 355
                        return true;                                                       // C# line 356
                    }
                }
                else                                                                       // C# line 359
                {
                    if (evadeSpell.castType == CastType::Self)                             // C# line 361
                    {
                        auto* posInfo = EvadeHelper::GetBestPosition();                     // C# line 363
                        if (posInfo != nullptr)                                             // C# line 364
                        {
                            Vec2 pos = posInfo->position;                                   // capture
                            CastEvadeSpell([=]() { EvadeCommand::CastSpell(evadeSpell); },  // C# line 366
                                           processSpell);
                            DelayAction::Add(5, [=]() { EvadeCommand::MoveTo(pos); });      // C# line 367
                            return true;                                                   // C# line 368
                        }
                    }
                    else if (evadeSpell.castType == CastType::Position)                    // C# line 372
                    {
                        auto* posInfo = EvadeHelper::GetBestPosition();                     // C# line 374
                        if (posInfo != nullptr)                                             // C# line 375
                        {
                            Vec2 pos = posInfo->position;                                   // capture
                            CastEvadeSpell([=]() { EvadeCommand::CastSpell(evadeSpell, pos); }, // C# line 377
                                           processSpell);
                            DelayAction::Add(5, [=]() { EvadeCommand::MoveTo(pos); });      // C# line 378
                            return true;                                                   // C# line 379
                        }
                    }
                }
            }
        }

        return false;                                                                      // C# line 386
    }

    // =========================================================================
    // CastEvadeSpell
    // C# original: CastEvadeSpell(Callback func, bool process) (line 389-395)
    //
    //   public static void CastEvadeSpell(Callback func, bool process = true)
    //   {
    //       if (process) { func(); }
    //   }
    // =========================================================================
    void EvadeSpell::CastEvadeSpell(Callback func, bool process)
    {
        if (process)                                                                       // C# line 391
        {
            func();                                                                        // C# line 393
        }
    }

    // =========================================================================
    // CompareEvadeOption
    // C# original: CompareEvadeOption(PositionInfo, checkSpell) (line 397-408)
    //
    //   public static bool CompareEvadeOption(PositionInfo posInfo, bool checkSpell = false)
    //   {
    //       if (checkSpell)
    //       {
    //           if (posInfo.posDangerLevel == 0) { return true; }
    //       }
    //       return posInfo.isBetterMovePos();
    //   }
    // =========================================================================
    bool EvadeSpell::CompareEvadeOption(PositionInfo* posInfo, bool checkSpell)
    {
        if (posInfo == nullptr) return false;

        if (checkSpell)                                                                    // C# line 399
        {
            if (posInfo->posDangerLevel == 0)                                              // C# line 401
            {
                return true;                                                               // C# line 403
            }
        }

        return posInfo->IsBetterMovePos();                                                 // C# line 407
    }

    // =========================================================================
    // ShouldActivateEvadeSpell
    // C# original: ShouldActivateEvadeSpell(Spell) (line 410-441)
    //
    //   private static bool ShouldActivateEvadeSpell(Spell spell)
    //   {
    //       if (Evade.lastPosInfo == null) return false;
    //       if (DodgeSkillShots keybind active)
    //       {
    //           if (undodgeableSpells.Contains(spell.spellID) && InSkillShot) return true;
    //       }
    //       else
    //       {
    //           if (InSkillShot) return true;
    //       }
    //       return false;
    //   }
    // =========================================================================
    bool EvadeSpell::ShouldActivateEvadeSpell(Spell& spell)
    {
        if (Evade::lastPosInfo == nullptr)                                                 // C# line 412
            return false;                                                                  // C# line 413

        // if (ObjectCache.menuCache.cache["DodgeSkillShots"].GetValue<KeyBind>().Active)
        bool dodgeActive = true; // TODO: read from menu keybind                           // C# line 415

        if (dodgeActive)                                                                   // C# line 415
        {
            // Check if spell is in undodgeable list
            bool isUndodgeable = false;                                                    // C# line 417
            for (int id : Evade::lastPosInfo->undodgeableSpells) {
                if (id == spell.spellID) { isUndodgeable = true; break; }
            }

            if (isUndodgeable                                                             // C# line 417
                && Position::InSkillShot(ObjectCache::myHeroCache.serverPos2D,                       // C# line 418
                               spell, ObjectCache::myHeroCache.boundingRadius))
            {
                return true;                                                               // C# line 420
            }
        }
        else                                                                               // C# line 423
        {
            if (Position::InSkillShot(ObjectCache::myHeroCache.serverPos2D,                          // C# line 425
                            spell, ObjectCache::myHeroCache.boundingRadius))
            {
                return true;                                                               // C# line 427
            }
        }

        return false;                                                                      // C# line 440
    }

    // =========================================================================
    // ShouldUseMovementBuff
    // C# original: ShouldUseMovementBuff(Spell) (line 443-460)
    //
    //   public static bool ShouldUseMovementBuff(Spell spell)
    //   {
    //       var sortedEvadeSpells = evadeSpells.Where(s => s.evadeType == EvadeType.MovementSpeedBuff)
    //                                          .OrderBy(s => s.dangerlevel);
    //       foreach (var evadeSpell in sortedEvadeSpells)
    //       {
    //           if (... can't use ...) return false;
    //       }
    //       return true;
    //   }
    // =========================================================================
    bool EvadeSpell::ShouldUseMovementBuff(Spell& spell)
    {
        // Filter and sort: only MovementSpeedBuff spells, ordered by dangerlevel
        std::vector<EvadeSpellData> filtered;                                              // C# line 445
        for (auto& s : evadeSpells) {
            if (s.evadeType == EvadeType::MovementSpeedBuff)
                filtered.push_back(s);
        }
        std::sort(filtered.begin(), filtered.end(),
            [](const EvadeSpellData& a, const EvadeSpellData& b) {
                return a.dangerlevel < b.dangerlevel;
            });

        for (auto& evadeSpell : filtered)                                                  // C# line 447
        {
            // if (menu UseEvadeSpell == false || danger mismatch || can't use || wrong name)
            if (GetSpellDangerLevel(evadeSpell) > spell.GetSpellDangerLevel())              // C# line 450
            {
                return false;                                                              // C# line 455
            }

            if (evadeSpell.isItem && !Items::CanUseItem(evadeSpell.itemID))                 // C# line 452
            {
                return false;                                                              // C# line 455
            }

            // TODO: Add remaining checks (menu, spellbook ready, spell name)              // C# line 449,451,453
        }

        return true;                                                                       // C# line 459
    }

    // =========================================================================
    // GetSpellDangerLevel
    // C# original: GetSpellDangerLevel(EvadeSpellData) (line 462-485)
    //
    //   public static int GetSpellDangerLevel(EvadeSpellData spell)
    //   {
    //       var dangerStr = ObjectCache.menuCache.cache[spell.name + "EvadeSpellDangerLevel"]
    //                       .GetValue<StringList>().SelectedValue;
    //       switch (dangerStr)
    //       {
    //           case "Low":     return 1;
    //           case "High":    return 3;
    //           case "Extreme": return 4;
    //           default:        return 2; // Normal
    //       }
    //   }
    // =========================================================================
    int EvadeSpell::GetSpellDangerLevel(const EvadeSpellData& spell)
    {
        // var dangerStr = ObjectCache.menuCache.cache[spell.name + "EvadeSpellDangerLevel"]
        //                 .GetValue<StringList>().SelectedValue;
        int dangerIndex = ObjectCache::GetDangerLevel(spell.name + "EvadeSpellDangerLevel"); // C# line 464

        // GetDangerLevel already maps: 0->1, 1->2, 2->3, 3->4
        return dangerIndex;                                                                // C# line 484
    }

    // =========================================================================
    // GetSummonerSlot
    // C# original: GetSummonerSlot(string) (line 487-499)
    //
    //   private SpellSlot GetSummonerSlot(string spellName)
    //   {
    //       if (myHero.Spellbook.GetSpell(SpellSlot.Summoner1).SData.Name == spellName)
    //           return SpellSlot.Summoner1;
    //       else if (myHero.Spellbook.GetSpell(SpellSlot.Summoner2).SData.Name == spellName)
    //           return SpellSlot.Summoner2;
    //       return SpellSlot.Unknown;
    //   }
    // =========================================================================
    SpellSlotId EvadeSpell::GetSummonerSlot(const std::string& spellName)
    {
        // TODO: Read summoner spell names from SpellBook offsets
        // For now, check known slots:
        //   SpellSlot.Summoner1 = SpellSlotId::F (index 4)
        //   SpellSlot.Summoner2 = SpellSlotId::T (index 5)

        // if (myHero.Spellbook.GetSpell(Summoner1).SData.Name == spellName)               // C# line 489
        //     return SpellSlotId::F;                                                      // C# line 491
        // else if (myHero.Spellbook.GetSpell(Summoner2).SData.Name == spellName)           // C# line 493
        //     return SpellSlotId::T;                                                      // C# line 495

        return SpellSlotId::None;                                                          // C# line 498: Unknown
    }

    // =========================================================================
    // LoadEvadeSpellList
    // C# original: LoadEvadeSpellList() (line 501-538)
    //
    //   private void LoadEvadeSpellList()
    //   {
    //       foreach (var spell in EvadeSpellDatabase.Spells.Where(
    //           s => (s.charName == myHero.ChampionName || s.charName == "AllChampions")))
    //       {
    //           if (spell.isSummonerSpell)
    //           {
    //               SpellSlot spellKey = GetSummonerSlot(spell.spellName);
    //               if (spellKey == SpellSlot.Unknown) continue;
    //               else spell.spellKey = spellKey;
    //           }
    //           if (spell.isItem)
    //           {
    //               itemSpells.Add(spell); continue;
    //           }
    //           if (spell.isSpecial)
    //           {
    //               SpecialEvadeSpell.LoadSpecialSpell(spell);
    //           }
    //           evadeSpells.Add(spell);
    //           var newSpellMenu = CreateEvadeSpellMenu(spell);
    //       }
    //       evadeSpells.Sort((a, b) => a.dangerlevel.CompareTo(b.dangerlevel));
    //   }
    // =========================================================================
    void EvadeSpell::LoadEvadeSpellList()
    {
        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        if (!myHero.IsValid()) return;

        // Get champion name
        // TODO: Read from myHero.GetChampionName() or use the CharacterName offset
        std::string myChampName = "Unknown"; // Placeholder                                // C# line 505

        auto& database = GetEvadeSpellDatabase();                                           // C# line 504

        for (auto spell : database)                                                        // C# line 504 (copy to allow modification)
        {
            // Filter: only matching champion or AllChampions
            if (spell.charName != myChampName && spell.charName != "AllChampions")          // C# line 505
                continue;

            if (spell.isSummonerSpell)                                                     // C# line 508
            {
                SpellSlotId spellKey = GetSummonerSlot(spell.spellName);                    // C# line 510
                if (spellKey == SpellSlotId::None)                                          // C# line 511
                {
                    continue;                                                              // C# line 513
                }
                else                                                                       // C# line 515
                {
                    spell.spellKey = spellKey;                                              // C# line 517
                }
            }

            if (spell.isItem)                                                              // C# line 521
            {
                itemSpells.push_back(spell);                                               // C# line 523
                continue;                                                                  // C# line 524
            }

            if (spell.isSpecial)                                                           // C# line 527
            {
                SpecialEvadeSpell::LoadSpecialSpell(spell);                                 // C# line 529
            }

            evadeSpells.push_back(spell);                                                  // C# line 532

            CreateEvadeSpellMenu(spell);                                                    // C# line 534
        }

        // evadeSpells.Sort((a, b) => a.dangerlevel.CompareTo(b.dangerlevel));
        std::sort(evadeSpells.begin(), evadeSpells.end(),                                   // C# line 537
            [](const EvadeSpellData& a, const EvadeSpellData& b) {
                return a.dangerlevel < b.dangerlevel;
            });
    }

} // namespace EzEvade
