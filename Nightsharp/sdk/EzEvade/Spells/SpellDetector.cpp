#include "SpellDetector.h"
#include "SpellDatabase.h"
#include "SpellWindupDatabase.h"
#include "../Core/Evade.h"
#include "../Helpers/Position.h"
#include "../Core/EvadeHelper.h"

namespace EzEvade {
namespace SpellDetector {

    // =========================================================================
    // Static data definitions (C# lines 33-52)
    // =========================================================================
    std::map<int, Spell> spells;                                                    // C# line 33
    std::map<int, Spell> drawSpells;                                                // C# line 34
    std::map<int, Spell> detectedSpells;                                            // C# line 35

    std::map<std::string, std::string> channeledSpells;                             // C# line 39

    std::map<std::string, SpellData> onProcessTraps;                                // C# line 41
    std::map<std::string, SpellData> onProcessSpells;                               // C# line 42
    std::map<std::string, SpellData> onMissileSpells;                               // C# line 43

    std::map<std::string, SpellData> windupSpells;                                  // C# line 45

    int spellIDCount = 0;                                                           // C# line 47

    float lastCheckTime = 0;                                                        // C# line 51
    float lastCheckSpellCollisionTime = 0;                                          // C# line 52

    std::vector<OnProcessDetectedSpellsHandler> onProcessDetectedSpellsCallbacks;   // C# line 25
    std::vector<OnProcessSpecialSpellHandler> OnProcessSpecialSpell;                 // C# line 29

    // Helper: lowercase string
    static std::string ToLower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        return result;
    }

    // =========================================================================
    // Initialize (C# line 58-77)
    // =========================================================================
    void Initialize()
    {
        LoadSpellDictionary();                                                      // C# line 76
        InitChannelSpells();                                                        // C# line 77
    }

    // =========================================================================
    // OnMissileCreate (C# line 79-140)
    // Called when a missile object is created in the game
    // =========================================================================
    void OnMissileCreate(SDK::GameObject& missile, const Vec3& startPos,
        const Vec3& endPos, const std::string& missileName, int casterNetId, bool casterVisible)
    {
        SpellData spellData;                                                        // C# line 86
        std::string lowerName = ToLower(missileName);

        auto it = onMissileSpells.find(lowerName);                                  // C# line 89
        if (it == onMissileSpells.end()) return;
        spellData = it->second;

        auto myHero = SDK::ObjectManager::GetLocalPlayer();                                       // C# line 92
        if (!myHero.IsValid()) return;

        if (startPos.Distance2D(myHero.GetPosition()) >= spellData.range + 1000)   // C# line 92
            return;

        if (casterVisible)                                                          // C# line 96
        {
            if (spellData.usePackets)                                               // C# line 98
            {
                CreateSpellData(nullptr, startPos, endPos, spellData, &missile);     // C# line 100
                return;                                                             // C# line 101
            }

            bool objectAssigned = false;                                            // C# line 104

            for (auto& entry : detectedSpells)                                      // C# line 106
            {
                Spell& spell = entry.second;                                        // C# line 108

                Vec2 dir = (endPos.To2D() - startPos.To2D()).Normalized();           // C# line 110

                if (ToLower(spell.info.missileName) == lowerName)                   // C# line 112
                {
                    if (spell.heroID == casterNetId &&
                        dir.AngleBetween(spell.direction) < 10)                     // C# line 114
                    {
                        if (!spell.info.isThreeWay && !spell.info.isSpecial)         // C# line 116
                        {
                            spell.spellObject = &missile;                           // C# line 118
                            objectAssigned = true;                                  // C# line 119
                            break;                                                  // C# line 120
                        }
                    }
                }
            }

            if (!objectAssigned)                                                    // C# line 126
            {
                CreateSpellData(nullptr, startPos, endPos, spellData, &missile);     // C# line 128
            }
        }
        else                                                                        // C# line 131
        {
            if (ObjectCache::GetBool("DodgeFOWSpells"))                             // C# line 133
            {
                CreateSpellData(nullptr, startPos, endPos, spellData, &missile);     // C# line 135
            }
        }
    }

    // =========================================================================
    // OnMissileDelete (C# line 142-157)
    // =========================================================================
    void OnMissileDelete(int missileNetId)
    {
        std::vector<int> toDelete;                                                  // collect IDs to remove

        for (auto& entry : spells)                                                  // C# line 149-150
        {
            Spell& spell = entry.second;
            if (spell.spellObject != nullptr &&
                spell.spellObject->GetNetId() == missileNetId)                      // C# line 150
            {
                if (spell.info.spellName.find("_trap") == std::string::npos)        // C# line 152
                {
                    toDelete.push_back(spell.spellID);                              // C# line 154
                }
            }
        }

        for (int id : toDelete)
            DeleteSpell(id);
    }

    // =========================================================================
    // RemoveNonDangerousSpells (C# line 159-166)
    // =========================================================================
    void RemoveNonDangerousSpells()
    {
        std::vector<int> toDelete;                                                  // collect IDs

        for (auto& entry : spells)                                                  // C# line 161-162
        {
            if (entry.second.GetSpellDangerLevel() < 3)
                toDelete.push_back(entry.second.spellID);
        }

        for (int id : toDelete)                                                     // C# line 164
            DeleteSpell(id);
    }

    // =========================================================================
    // OnProcessSpell (C# line 168-220)
    // Called when a champion casts a spell
    // =========================================================================
    void OnProcessSpell(SDK::GameObject* hero, const Vec3& startPos,
        const Vec3& endPos, const std::string& spellName)
    {
        if (!hero) return;                                                          // C# line 170

        std::string lowerName = ToLower(spellName);

        auto it = onProcessSpells.find(lowerName);                                  // C# line 173
        if (it == onProcessSpells.end()) return;

        SpellData spellData = it->second;

        if (spellData.usePackets) return;                                           // C# line 175

        SpecialSpellEventArgs specialArgs;                                          // C# line 177
        specialArgs.spellData = spellData;

        for (auto& callback : OnProcessSpecialSpell)                               // C# line 178
            callback(hero, startPos, endPos, spellData, specialArgs);

        spellData = specialArgs.spellData;                                          // C# line 181

        if (specialArgs.noProcess || spellData.noProcess)                           // C# line 183
            return;

        bool foundMissile = false;                                                  // C# line 185

        if (!spellData.isThreeWay && !spellData.isSpecial)                          // C# line 187
        {
            for (auto& entry : detectedSpells)                                      // C# line 189
            {
                Spell& spell = entry.second;                                        // C# line 191

                Vec2 dir = (endPos.To2D() - startPos.To2D()).Normalized();           // C# line 193
                if (spell.spellObject != nullptr)                                   // C# line 194
                {
                    if (ToLower(spell.info.spellName) == lowerName)                 // C# line 196
                    {
                        if (spell.heroID == hero->GetNetId() &&
                            dir.AngleBetween(spell.direction) < 10)                 // C# line 198
                        {
                            foundMissile = true;                                    // C# line 200
                            break;                                                  // C# line 201
                        }
                    }
                }
            }
        }

        if (!foundMissile)                                                          // C# line 208
        {
            CreateSpellData(hero, hero->GetServerPosition(), endPos, spellData);    // C# line 210
        }
    }

    // =========================================================================
    // CreateSpellData (C# line 222-380)
    // Master spell creation — handles all spell types
    // =========================================================================
    void CreateSpellData(SDK::GameObject* hero, const Vec3& spellStartPos, const Vec3& spellEndPos,
        SpellData spellData, SDK::GameObject* obj, float extraEndTick,
        bool processSpell, SpellType spellType,
        bool checkEndExplosion, float spellRadius)
    {
        // EndExplosion recursion (C# line 226-235)
        if (checkEndExplosion && spellData.hasEndExplosion)                          // C# line 226
        {
            CreateSpellData(hero, spellStartPos, spellEndPos, spellData,
                obj, extraEndTick, false, spellData.spellType, false);              // C# line 228-229

            CreateSpellData(hero, spellStartPos, spellEndPos, spellData,
                obj, extraEndTick, true, SpellType::Circular, false);               // C# line 231-232

            return;                                                                 // C# line 234
        }

        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        if (!myHero.IsValid()) return;

        if (spellStartPos.Distance2D(myHero.GetPosition()) >= spellData.range + 1000) // C# line 237
            return;

        Vec2 startPosition = spellStartPos.To2D();                                  // C# line 239
        Vec2 endPosition = spellEndPos.To2D();                                      // C# line 240
        Vec2 direction = (endPosition - startPosition).Normalized();                // C# line 241
        float endTick = 0.0f;                                                       // C# line 242

        if (spellType == SpellType::None)                                           // C# line 244
            spellType = spellData.spellType;                                        // C# line 246

        if (spellData.fixedRange)                                                   // C# line 249
        {
            if (endPosition.Distance(startPosition) > spellData.range)              // C# line 251
                endPosition = startPosition + direction * spellData.range;          // C# line 252
        }

        if (spellType == SpellType::Line)                                           // C# line 255
        {
            endTick = spellData.spellDelay +
                (spellData.range / spellData.projectileSpeed) * 1000;               // C# line 257
            endPosition = startPosition + direction * spellData.range;              // C# line 258

            if (spellData.fixedRange)                                               // C# line 260
            {
                if (endPosition.Distance(startPosition) < spellData.range)          // C# line 262
                    endPosition = startPosition + direction * spellData.range;      // C# line 263
            }

            if (endPosition.Distance(startPosition) > spellData.range)              // C# line 266
                endPosition = startPosition + direction * spellData.range;          // C# line 267

            if (spellData.useEndPosition)                                           // C# line 269
            {
                float range = endPosition.Distance(startPosition);                  // C# line 271
                endTick = spellData.spellDelay + (range / spellData.projectileSpeed) * 1000; // C# line 272
            }

            if (obj != nullptr)                                                     // C# line 275
                endTick -= spellData.spellDelay;                                    // C# line 276
        }
        else if (spellType == SpellType::Circular)                                  // C# line 278
        {
            endTick = spellData.spellDelay;                                         // C# line 280

            if (endPosition.Distance(startPosition) > spellData.range)              // C# line 282
                endPosition = startPosition + direction * spellData.range;          // C# line 283

            if (spellData.projectileSpeed == 0 && hero != nullptr)                  // C# line 285
            {
                endPosition = hero->GetServerPosition().To2D();                     // C# line 287
            }
            else if (spellData.projectileSpeed > 0)                                 // C# line 289
            {
                endTick = endTick + 1000 * startPosition.Distance(endPosition)
                    / spellData.projectileSpeed;                                    // C# line 291

                if (spellData.spellType == SpellType::Line && spellData.hasEndExplosion) // C# line 293
                {
                    if (!spellData.useEndPosition)                                  // C# line 295
                    {
                        endPosition = startPosition + direction * spellData.range;  // C# line 297
                    }
                }
            }
        }
        else if (spellType == SpellType::Arc)                                       // C# line 302
        {
            endTick = endTick + 1000 * startPosition.Distance(endPosition)
                / spellData.projectileSpeed;                                        // C# line 304

            if (obj != nullptr)                                                     // C# line 306
                endTick -= spellData.spellDelay;                                    // C# line 307
        }
        else if (spellType == SpellType::Cone)                                      // C# line 309
        {
            endPosition = startPosition + direction * spellData.range;              // C# line 311
            endTick = spellData.spellDelay;                                         // C# line 312

            if (endPosition.Distance(startPosition) > spellData.range)              // C# line 314
                endPosition = startPosition + direction * spellData.range;          // C# line 315

            if (spellData.projectileSpeed == 0 && hero != nullptr)                  // C# line 317
            {
                endPosition = hero->GetServerPosition().To2D();                     // C# line 319
            }
            else if (spellData.projectileSpeed > 0)                                 // C# line 321
            {
                endTick = endTick + 1000 * startPosition.Distance(endPosition)
                    / spellData.projectileSpeed;                                    // C# line 323
            }
        }
        else                                                                        // C# line 326
        {
            return;                                                                 // C# line 328
        }

        // Invert (C# line 331-335)
        if (spellData.invert)                                                       // C# line 331
        {
            Vec2 dir = (startPosition - endPosition).Normalized();                  // C# line 333
            endPosition = startPosition + dir * startPosition.Distance(endPosition); // C# line 334
        }

        // Perpendicular (C# line 337-341)
        if (spellData.isPerpendicular)                                              // C# line 337
        {
            Vec2 perp = direction.Perpendicular();
            startPosition = spellEndPos.To2D() - perp * spellData.secondaryRadius;  // C# line 339
            endPosition = spellEndPos.To2D() + perp * spellData.secondaryRadius;    // C# line 340
        }

        endTick += extraEndTick;                                                    // C# line 343

        // Create new Spell (C# line 345-362)
        Spell newSpell;                                                             // C# line 345
        newSpell.startTime = EvadeUtils::TickCount();                               // C# line 346
        newSpell.endTime = EvadeUtils::TickCount() + endTick;                       // C# line 347
        newSpell.startPos = startPosition;                                          // C# line 348
        newSpell.endPos = endPosition;                                              // C# line 349
        newSpell.height = spellEndPos.y + spellData.extraDrawHeight;                // C# line 350
        newSpell.direction = direction;                                             // C# line 351
        newSpell.info = spellData;                                                  // C# line 352
        newSpell.spellType = spellType;                                             // C# line 353
        newSpell.radius = spellRadius > 0 ? spellRadius : newSpell.GetSpellRadius(); // C# line 354

        if (spellType == SpellType::Cone)                                           // C# line 356
        {
            newSpell.radius = 100 + (newSpell.radius * 3);                          // C# line 358
            newSpell.cnStart = startPosition + direction;                           // C# line 359
            Vec2 perp = direction.Perpendicular();
            newSpell.cnLeft = endPosition + perp * newSpell.radius;                 // C# line 360
            newSpell.cnRight = endPosition - perp * newSpell.radius;                // C# line 361
        }

        if (hero != nullptr)                                                        // C# line 364
            newSpell.heroID = hero->GetNetId();                                     // C# line 365

        if (obj != nullptr)                                                         // C# line 367
        {
            newSpell.spellObject = obj;                                             // C# line 369
            newSpell.projectileID = obj->GetNetId();                                // C# line 370
        }

        int spellID = CreateSpell(newSpell, processSpell);                          // C# line 373

        // Schedule deletion (C# line 375-378)
        // In C++ we handle this in CheckSpellEndTime instead of DelayAction
        // extraEndTick == 1337 means trap, don't auto-delete
        (void)spellID; // used by delete timer in Game_OnGameUpdate loop
    }

    // =========================================================================
    // OnGameUpdate (C# line 382-399)
    // Called every game tick
    // =========================================================================
    void OnGameUpdate()
    {
        UpdateSpells();                                                             // C# line 384

        if (EvadeUtils::TickCount() - lastCheckSpellCollisionTime > 100)            // C# line 386
        {
            CheckSpellCollision();                                                  // C# line 388
            lastCheckSpellCollisionTime = EvadeUtils::TickCount();                  // C# line 389
        }

        if (EvadeUtils::TickCount() - lastCheckTime > 1)                            // C# line 392
        {
            CheckSpellEndTime();                                                    // C# line 395
            AddDetectedSpells();                                                    // C# line 396
            lastCheckTime = EvadeUtils::TickCount();                                // C# line 397
        }
    }

    // =========================================================================
    // UpdateSpells (C# line 401-407)
    // Update position of all detected spells
    // =========================================================================
    void UpdateSpells()
    {
        for (auto& entry : detectedSpells)                                          // C# line 403
        {
            entry.second.UpdateSpellInfo();                                         // C# line 405
        }
    }

    // =========================================================================
    // CheckSpellEndTime (C# line 409-432)
    // Remove spells whose time is up or whose caster is dead
    // =========================================================================
    void CheckSpellEndTime()
    {
        std::vector<int> toDelete;

        for (auto& entry : detectedSpells)                                          // C# line 411
        {
            Spell& spell = entry.second;                                            // C# line 413

            if (spell.info.spellName.find("_trap") != std::string::npos)            // C# line 414
                continue;                                                           // C# line 415

            // Check if caster is dead (C# line 417-424)
            auto heroes = SDK::ObjectManager::GetHeroes();
            for (auto& hero : heroes) {
                if (hero.IsDead() && spell.heroID == hero.GetNetId())               // C# line 419
                {
                    if (spell.spellObject == nullptr)                               // C# line 421
                        toDelete.push_back(entry.first);                            // C# line 422
                }
            }

            // Check time expired (C# line 426-430)
            if (spell.endTime + spell.info.extraEndTime < EvadeUtils::TickCount()   // C# line 426
                || !CanHeroWalkIntoSpell(spell))                                    // C# line 427
            {
                toDelete.push_back(entry.first);                                    // C# line 429
            }
        }

        // Remove duplicates and delete
        std::sort(toDelete.begin(), toDelete.end());
        toDelete.erase(std::unique(toDelete.begin(), toDelete.end()), toDelete.end());
        for (int id : toDelete)
            DeleteSpell(id);
    }

    // =========================================================================
    // CheckSpellCollision (C# line 434-457)
    // Check if spell will collide with something (minion, wall)
    // =========================================================================
    void CheckSpellCollision()
    {
        if (!ObjectCache::GetBool("CheckSpellCollision"))                           // C# line 436
            return;                                                                 // C# line 438

        std::vector<int> toDelete;

        for (auto& entry : detectedSpells)                                          // C# line 441
        {
            Spell& spell = entry.second;                                            // C# line 443

            auto* collisionObject = spell.CheckSpellCollision();                    // C# line 445
            if (collisionObject != nullptr)                                         // C# line 446
            {
                spell.predictedEndPos = spell.GetSpellProjection(
                    collisionObject->GetServerPosition().To2D());                   // C# line 448

                if (spell.currentSpellPosition.Distance(
                    collisionObject->GetServerPosition().To2D()) <
                    collisionObject->GetBoundingRadius() + spell.radius)            // C# line 450-451
                {
                    toDelete.push_back(entry.first);                                // C# line 453
                }
            }
        }

        for (int id : toDelete)
            DeleteSpell(id);
    }

    // =========================================================================
    // CanHeroWalkIntoSpell (C# line 459-506)
    // Advanced detection: can the hero walk into the spell area?
    // =========================================================================
    bool CanHeroWalkIntoSpell(Spell& spell)
    {
        if (ObjectCache::GetBool("AdvancedSpellDetection"))                          // C# line 461
        {
            auto myHero = SDK::ObjectManager::GetLocalPlayer();
            if (!myHero) return true;

            Vec2 heroPos = myHero.GetPosition().To2D();                            // C# line 463
            float extraDist = heroPos.Distance(ObjectCache::myHeroCache.serverPos2D); // C# line 464

            if (spell.spellType == SpellType::Line)                                 // C# line 466
            {
                float walkRadius = ObjectCache::myHeroCache.moveSpeed *
                    (spell.endTime - EvadeUtils::TickCount()) / 1000
                    + ObjectCache::myHeroCache.boundingRadius
                    + spell.info.radius + extraDist + 10;                           // C# line 468
                Vec2 spellPos = spell.currentSpellPosition;                         // C# line 469
                Vec2 spellEndPos = spell.GetSpellEndPosition();                     // C# line 470

                Vec2 projection = SDK::Geometry::ProjectOn(heroPos, spellPos, spellEndPos).segmentPoint; // C# line 472
                return projection.Distance(heroPos) <= walkRadius;                  // C# line 474
            }
            else if (spell.spellType == SpellType::Circular)                        // C# line 476
            {
                float walkRadius = ObjectCache::myHeroCache.moveSpeed *
                    (spell.endTime - EvadeUtils::TickCount()) / 1000
                    + ObjectCache::myHeroCache.boundingRadius
                    + spell.info.radius + extraDist + 10;                           // C# line 478

                if (heroPos.Distance(spell.endPos) < walkRadius)                    // C# line 480
                    return true;                                                    // C# line 482
            }
            else if (spell.spellType == SpellType::Arc)                             // C# line 486
            {
                float spellRange = spell.startPos.Distance(spell.endPos);           // C# line 488
                Vec2 midPoint = spell.startPos + spell.direction * (spellRange / 2); // C# line 489
                float arcRadius = spell.info.radius * (1 + spellRange / 100);       // C# line 490

                float walkRadius = ObjectCache::myHeroCache.moveSpeed *
                    (spell.endTime - EvadeUtils::TickCount()) / 1000
                    + ObjectCache::myHeroCache.boundingRadius
                    + arcRadius + extraDist + 10;                                   // C# line 492

                if (heroPos.Distance(midPoint) < walkRadius)                        // C# line 494
                    return true;                                                    // C# line 496
            }

            return false;                                                           // C# line 501
        }

        return true;                                                                // C# line 505
    }

    // =========================================================================
    // AddDetectedSpells (C# line 508-602)
    // Filter and add spells to the main spells list for dodging
    // =========================================================================
    void AddDetectedSpells()
    {
        bool spellAdded = false;                                                    // C# line 510
        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        if (!myHero.IsValid()) return;

        for (auto& entry : detectedSpells)                                          // C# line 512
        {
            Spell& spell = entry.second;                                            // C# line 514

            if (spell.info.spellName.find("_trap") != std::string::npos)            // C# line 515
            {
                // traps: skip fast evade logic                                     // C# line 517
            }
            else
            {
                EvadeHelper::fastEvadeMode =
                    ObjectCache::GetBool(spell.info.spellName + "FastEvade");        // C# line 521
            }

            float evadeTime, spellHitTime;                                          // C# line 524
            spell.CanHeroEvade(myHero, evadeTime, spellHitTime);                    // C# line 525

            spell.spellHitTime = spellHitTime;                                      // C# line 527
            spell.evadeTime = evadeTime;                                            // C# line 528

            float extraDelay = ObjectCache::gamePing +
                (float)ObjectCache::GetSlider("ExtraPingBuffer");                   // C# line 530

            if (spell.spellHitTime - extraDelay < 1500 &&
                CanHeroWalkIntoSpell(spell))                                        // C# line 532
            {
                Spell& newSpell = spell;                                            // C# line 535
                int spellID = spell.spellID;                                        // C# line 536

                if (drawSpells.find(spell.spellID) == drawSpells.end())             // C# line 538
                {
                    drawSpells[spellID] = newSpell;                                 // C# line 540
                }

                // SpellDetectionTime check (C# line 544-548)
                int detectionTime = ObjectCache::GetSlider("SpellDetectionTime");
                if (spellHitTime < (float)detectionTime &&
                    !ObjectCache::GetBool(spell.info.spellName + "FastEvade"))       // C# line 544-545
                {
                    continue;                                                       // C# line 547
                }

                // ReactionTime check (C# line 550-554)
                int reactionTime = ObjectCache::GetSlider("ReactionTime");
                if (EvadeUtils::TickCount() - spell.startTime < (float)reactionTime &&
                    !ObjectCache::GetBool(spell.info.spellName + "FastEvade"))       // C# line 550-551
                {
                    continue;                                                       // C# line 553
                }

                // DodgeInterval check (C# line 556-567)
                int dodgeInterval = ObjectCache::GetSlider("DodgeInterval");
                if (Evade::lastPosInfo != nullptr && dodgeInterval > 0)             // C# line 557
                {
                    float timeElapsed = EvadeUtils::TickCount() -
                        Evade::lastPosInfo->timestamp;                              // C# line 559

                    if ((float)dodgeInterval > timeElapsed &&
                        !ObjectCache::GetBool(spell.info.spellName + "FastEvade"))   // C# line 561
                    {
                        continue;                                                   // C# line 565
                    }
                }

                // Add to spells list (C# line 569-588)
                if (spells.find(spell.spellID) == spells.end())                     // C# line 569
                {
                    bool isDodgeDangerous = Evade::IsDodgeDangerousEnabled()
                        && newSpell.GetSpellDangerLevel() < 3;                      // C# line 571
                    bool dodgeEnabled = ObjectCache::GetBool(
                        newSpell.info.spellName + "DodgeSpell");                    // C# line 572

                    if (!isDodgeDangerous && dodgeEnabled)                           // C# line 571-572
                    {
                        if (newSpell.spellType == SpellType::Circular &&
                            !ObjectCache::GetBool("DodgeCircularSpells"))            // C# line 574-575
                        {
                            continue;                                               // C# line 578
                        }

                        int healthThreshold = ObjectCache::GetSlider(
                            spell.info.spellName + "DodgeIgnoreHP");                // C# line 581
                        if (myHero.GetHealthPercent() <= (float)healthThreshold)   // C# line 582
                        {
                            spells[spellID] = newSpell;                             // C# line 584
                            spellAdded = true;                                      // C# line 585
                        }
                    }
                }

                // CheckSpellCollision override (C# line 590-594)
                if (ObjectCache::GetBool("CheckSpellCollision") &&
                    spell.predictedEndPos.x != 0 && spell.predictedEndPos.y != 0)   // C# line 590-591
                {
                    spellAdded = false;                                             // C# line 593
                }
            }
        }

        if (spellAdded)                                                             // C# line 598
        {
            for (auto& cb : onProcessDetectedSpellsCallbacks)                       // C# line 600
                cb();
        }
    }

    // =========================================================================
    // CreateSpell (C# line 604-619)
    // Register a new spell in the detection system
    // =========================================================================
    int CreateSpell(Spell& newSpell, bool processSpell)
    {
        int spellID = spellIDCount++;                                               // C# line 606
        newSpell.spellID = spellID;                                                 // C# line 607

        newSpell.UpdateSpellInfo();                                                 // C# line 609
        detectedSpells[spellID] = newSpell;                                         // C# line 610

        if (processSpell)                                                           // C# line 612
        {
            CheckSpellCollision();                                                  // C# line 614
            AddDetectedSpells();                                                    // C# line 615
        }

        return spellID;                                                             // C# line 618
    }

    // =========================================================================
    // DeleteSpell (C# line 621-626)
    // =========================================================================
    void DeleteSpell(int spellID)
    {
        spells.erase(spellID);                                                      // C# line 623
        drawSpells.erase(spellID);                                                  // C# line 624
        detectedSpells.erase(spellID);                                              // C# line 625
    }

    // =========================================================================
    // GetCurrentSpellID (C# line 628-631)
    // =========================================================================
    int GetCurrentSpellID()
    {
        return spellIDCount;                                                        // C# line 630
    }

    // =========================================================================
    // GetSpellList (C# line 633-644)
    // =========================================================================
    std::vector<int> GetSpellList()
    {
        std::vector<int> spellList;                                                 // C# line 635

        for (auto& entry : spells)                                                  // C# line 637
        {
            spellList.push_back(entry.second.spellID);                              // C# line 640
        }

        return spellList;                                                           // C# line 643
    }

    // =========================================================================
    // GetHighestDetectedSpellID (C# line 646-656)
    // =========================================================================
    int GetHighestDetectedSpellID()
    {
        int highest = 0;                                                            // C# line 648

        for (auto& entry : spells)                                                  // C# line 650
        {
            highest = std::max(highest, entry.first);                               // C# line 652
        }

        return highest;                                                             // C# line 655
    }

    // =========================================================================
    // GetLowestEvadeTime (C# line 658-676)
    // =========================================================================
    float GetLowestEvadeTime(Spell*& lowestSpell)
    {
        float lowest = FLT_MAX;                                                     // C# line 660
        lowestSpell = nullptr;                                                      // C# line 661

        for (auto& entry : spells)                                                  // C# line 663
        {
            Spell& spell = entry.second;                                            // C# line 665

            if (spell.spellHitTime != -FLT_MAX)                                     // C# line 667 (float.MinValue)
            {
                float evadeDiff = spell.spellHitTime - spell.evadeTime;             // C# line 670
                if (evadeDiff < lowest) {
                    lowest = evadeDiff;
                    lowestSpell = &spell;                                            // C# line 671
                }
            }
        }

        return lowest;                                                              // C# line 675
    }

    // =========================================================================
    // GetMostDangerousSpell (C# line 678-698)
    // =========================================================================
    Spell* GetMostDangerousSpell(bool hasProjectile)
    {
        int maxDanger = 0;                                                          // C# line 680
        Spell* maxDangerSpell = nullptr;                                            // C# line 681

        for (auto& entry : spells)                                                  // C# line 683
        {
            Spell& spell = entry.second;

            if (!hasProjectile ||
                (spell.info.projectileSpeed > 0 &&
                 spell.info.projectileSpeed != FLT_MAX))                            // C# line 685
            {
                int dangerlevel = spell.dangerlevel;                                // C# line 687

                if (dangerlevel > maxDanger)                                         // C# line 689
                {
                    maxDanger = dangerlevel;                                         // C# line 691
                    maxDangerSpell = &spell;                                         // C# line 692
                }
            }
        }

        return maxDangerSpell;                                                      // C# line 697
    }

    // =========================================================================
    // InitChannelSpells (C# line 700-720)
    // =========================================================================
    void InitChannelSpells()
    {
        channeledSpells["drain"] = "FiddleSticks";                                  // C# line 702
        channeledSpells["crowstorm"] = "FiddleSticks";                              // C# line 703
        channeledSpells["katarinar"] = "Katarina";                                  // C# line 704
        channeledSpells["absolutezero"] = "Nunu";                                   // C# line 705
        channeledSpells["galioidolofdurand"] = "Galio";                             // C# line 706
        channeledSpells["missfortunebullettime"] = "MissFortune";                   // C# line 707
        channeledSpells["meditate"] = "MasterYi";                                   // C# line 708
        channeledSpells["malzaharr"] = "Malzahar";                                  // C# line 709
        channeledSpells["reapthewhirlwind"] = "Janna";                              // C# line 710
        channeledSpells["karthusfallenone"] = "Karthus";                            // C# line 711
        channeledSpells["karthusfallenone2"] = "Karthus";                           // C# line 712
        channeledSpells["velkozr"] = "Velkoz";                                      // C# line 713
        channeledSpells["xerathlocusofpower2"] = "Xerath";                          // C# line 714
        channeledSpells["zace"] = "Zac";                                            // C# line 715
        channeledSpells["pantheon_heartseeker"] = "Pantheon";                       // C# line 716
        channeledSpells["jhinr"] = "Jhin";                                          // C# line 717
        channeledSpells["odinrecall"] = "AllChampions";                             // C# line 718
        channeledSpells["recall"] = "AllChampions";                                 // C# line 719
    }

    // =========================================================================
    // LoadSpellDictionary (C# line 810-968)
    // Loads spells from SpellDatabase and registers them for detection
    // =========================================================================
    void LoadSpellDictionary()
    {
        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        if (!myHero.IsValid()) return;

        int myTeam = (int)myHero.GetTeam();

        // Load windup spells for local hero (C# line 814-826)
        for (auto& spell : GetSpellWindupDatabase())                             // C# line 818
        {
            if (spell.charName == myHero.GetChampionName())                        // C# line 819
            {
                std::string lowerName = ToLower(spell.spellName);
                if (windupSpells.find(lowerName) == windupSpells.end())             // C# line 821
                {
                    windupSpells[lowerName] = spell;                                // C# line 823
                }
            }
        }

        // Load enemy hero spells (C# line 828-967)
        auto heroes = SDK::ObjectManager::GetHeroes();
        for (auto& hero : heroes)
        {
            if (!hero.IsValid()) continue;
            if ((int)hero.GetTeam() == myTeam && !Evade::devModeOn) continue;       // C# line 828

            // === Trap spells (C# line 833-877) ===
            for (auto& spell : GetSpellDatabase())                               // C# line 830-831
            {
                if (spell.charName != hero.GetChampionName() &&
                    spell.charName != "AllChampions")
                    continue;                                                       // C# line 831

                if (spell.hasTrap && spell.projectileSpeed > 3000)                  // C# line 833
                {
                    // AllChampions check (C# line 835-842) — skip, simplified

                    std::string trapKey = ToLower(spell.spellName) + "trap";
                    if (onProcessSpells.find(trapKey) != onProcessSpells.end())      // C# line 844
                        continue;

                    SpellData trapSpell = spell;
                    if (trapSpell.trapBaseName.empty())
                        trapSpell.trapBaseName = trapSpell.spellName + "1";          // C# line 847
                    if (trapSpell.trapTroyName.empty())
                        trapSpell.trapTroyName = trapSpell.spellName + "2";          // C# line 850

                    onProcessTraps[ToLower(trapSpell.trapBaseName)] = trapSpell;     // C# line 852
                    onProcessTraps[ToLower(trapSpell.trapTroyName)] = trapSpell;     // C# line 853
                    onProcessSpells[trapKey] = trapSpell;                            // C# line 854
                }
            }

            // === Regular spells (C# line 881-966) ===
            for (auto& spell : GetSpellDatabase())                               // C# line 881-882
            {
                if (spell.charName != hero.GetChampionName() &&
                    spell.charName != "AllChampions")
                    continue;                                                       // C# line 882

                // Filter (C# line 885-889)
                if (spell.hasTrap && spell.projectileSpeed < 3000) continue;        // C# line 885
                if (!spell.hasTrap) { /* proceed */ }

                if (spell.spellType != SpellType::Circular &&
                    spell.spellType != SpellType::Line &&
                    spell.spellType != SpellType::Arc &&
                    spell.spellType != SpellType::Cone)
                    continue;                                                       // C# line 887-889

                std::string lowerSpellName = ToLower(spell.spellName);

                if (onProcessSpells.find(lowerSpellName) != onProcessSpells.end())  // C# line 900
                    continue;

                SpellData regSpell = spell;

                if (regSpell.missileName.empty())                                   // C# line 902
                    regSpell.missileName = regSpell.spellName;                      // C# line 903

                onProcessSpells[lowerSpellName] = regSpell;                         // C# line 905
                onMissileSpells[ToLower(regSpell.missileName)] = regSpell;           // C# line 906

                // Extra spell names (C# line 908-913)
                for (auto& extraName : regSpell.extraSpellNames)                    // C# line 910
                {
                    onProcessSpells[ToLower(extraName)] = regSpell;                 // C# line 912
                }

                // Extra missile names (C# line 916-921)
                for (auto& extraName : regSpell.extraMissileNames)                  // C# line 918
                {
                    onMissileSpells[ToLower(extraName)] = regSpell;                 // C# line 920
                }
            }
        }
    }

} // namespace SpellDetector
} // namespace EzEvade
