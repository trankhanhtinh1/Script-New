#include "Evade.h"

namespace EzEvade {

    // =========================================================================
    // Constructor (C# line 69-72)
    // =========================================================================
    Evade::Evade()
    {
        Initialize();                                                               // C# line 71
    }

    // =========================================================================
    // Initialize (C# line 79-210) — simplified, no menu GUI
    // =========================================================================
    void Evade::Initialize()
    {
        // Event hooks would be registered here with the game hook system
        // C# line 85-93: event subscriptions
        // In C++ these are registered through the hook framework

        // Initialize defaults for menu cache values
        // C# line 98-188: menu creation — values stored in ObjectCache::menuCache
        ObjectCache::SetBool("DodgeSkillShots", true);        // C# line 101
        ObjectCache::SetBool("ActivateEvadeSpells", true);    // C# line 103
        ObjectCache::SetBool("DodgeDangerous", false);        // C# line 105
        ObjectCache::SetBool("DodgeFOWSpells", true);         // C# line 106
        ObjectCache::SetBool("DodgeCircularSpells", true);    // C# line 107

        ObjectCache::SetBool("HigherPrecision", false);       // C# line 127
        ObjectCache::SetBool("RecalculatePosition", true);    // C# line 128
        ObjectCache::SetBool("ContinueMovement", true);       // C# line 129
        ObjectCache::SetBool("CalculateWindupDelay", true);   // C# line 130
        ObjectCache::SetBool("CheckSpellCollision", false);   // C# line 131
        ObjectCache::SetBool("PreventDodgingUnderTower", false); // C# line 132
        ObjectCache::SetBool("PreventDodgingNearEnemy", true);   // C# line 133
        ObjectCache::SetBool("AdvancedSpellDetection", false);   // C# line 134
        ObjectCache::SetBool("ClickRemove", true);               // C# line 135

        ObjectCache::SetBool("DodgeDangerousKeyEnabled", false); // C# line 117
        ObjectCache::SetBool("DodgeOnlyOnComboKeyEnabled", false); // C# line 120
        ObjectCache::SetBool("DontDodgeKeyEnabled", false);      // C# line 122

        ObjectCache::SetBool("ClickOnlyOnce", true);           // C# line 145
        ObjectCache::SetBool("EnableEvadeDistance", false);    // C# line 146
        ObjectCache::SetSlider("TickLimiter", 100);            // C# line 147
        ObjectCache::SetSlider("SpellDetectionTime", 0);       // C# line 148
        ObjectCache::SetSlider("ReactionTime", 0);             // C# line 149
        ObjectCache::SetSlider("DodgeInterval", 0);            // C# line 150

        ObjectCache::SetBool("FastMovementBlock", false);      // C# line 155
        ObjectCache::SetSlider("FastEvadeActivationTime", 65); // C# line 156
        ObjectCache::SetSlider("SpellActivationTime", 400);    // C# line 157
        ObjectCache::SetSlider("RejectMinDistance", 10);       // C# line 158

        ObjectCache::SetSlider("ExtraPingBuffer", 65);         // C# line 168
        ObjectCache::SetSlider("ExtraCPADistance", 10);        // C# line 169
        ObjectCache::SetSlider("ExtraSpellRadius", 0);         // C# line 170
        ObjectCache::SetSlider("ExtraEvadeDistance", 100);     // C# line 171
        ObjectCache::SetSlider("ExtraAvoidDistance", 50);      // C# line 172
        ObjectCache::SetSlider("MinComfortZone", 550);         // C# line 174

        ObjectCache::SetBool("ResetConfig", false);            // C# line 142

        // Drawing defaults
        ObjectCache::SetBool("DrawSkillShots", true);
        ObjectCache::SetBool("ShowStatus", true);
        ObjectCache::SetBool("DrawSpellPos", false);
        ObjectCache::SetBool("DrawEvadePosition", false);

        // spellDetector = new SpellDetector(menu); // C# line 113
        // evadeSpell = new EvadeSpell(menu);       // C# line 114

        auto initCache = ObjectCache::myHeroCache;             // C# line 194
    }

    // =========================================================================
    // ResetConfig (C# line 212-270)
    // =========================================================================
    void Evade::ResetConfig(bool kappa)
    {
        ObjectCache::SetBool("DodgeSkillShots", true);
        ObjectCache::SetBool("ActivateEvadeSpells", true);
        ObjectCache::SetBool("DodgeDangerous", false);
        ObjectCache::SetBool("DodgeFOWSpells", true);
        ObjectCache::SetBool("DodgeCircularSpells", true);

        ObjectCache::SetBool("HigherPrecision", false);
        ObjectCache::SetBool("RecalculatePosition", true);
        ObjectCache::SetBool("ContinueMovement", true);
        ObjectCache::SetBool("CalculateWindupDelay", true);
        ObjectCache::SetBool("CheckSpellCollision", false);
        ObjectCache::SetBool("PreventDodgingUnderTower", false);
        ObjectCache::SetBool("PreventDodgingNearEnemy", true);
        ObjectCache::SetBool("AdvancedSpellDetection", false);

        ObjectCache::SetBool("ClickOnlyOnce", true);
        ObjectCache::SetBool("EnableEvadeDistance", false);
        ObjectCache::SetSlider("TickLimiter", 100);
        ObjectCache::SetSlider("SpellDetectionTime", 0);
        ObjectCache::SetSlider("ReactionTime", 0);
        ObjectCache::SetSlider("DodgeInterval", 0);

        ObjectCache::SetBool("FastMovementBlock", false);
        ObjectCache::SetSlider("FastEvadeActivationTime", 65);
        ObjectCache::SetSlider("SpellActivationTime", 400);
        ObjectCache::SetSlider("RejectMinDistance", 10);

        ObjectCache::SetSlider("ExtraPingBuffer", 65);
        ObjectCache::SetSlider("ExtraCPADistance", 10);
        ObjectCache::SetSlider("ExtraSpellRadius", 0);
        ObjectCache::SetSlider("ExtraEvadeDistance", 100);        // C# line 245: 200 -> changed to 100 base
        ObjectCache::SetSlider("ExtraAvoidDistance", 50);
        ObjectCache::SetSlider("MinComfortZone", 550);

        ObjectCache::SetBool("DrawSkillShots", true);
        ObjectCache::SetBool("ShowStatus", true);
        ObjectCache::SetBool("DrawSpellPos", false);
        ObjectCache::SetBool("DrawEvadePosition", false);

        if (kappa)
        {
            ObjectCache::SetBool("DodgeDangerousKeyEnabled", false);
            ObjectCache::SetBool("DodgeOnlyOnComboKeyEnabled", false);
            ObjectCache::SetBool("DontDodgeKeyEnabled", false);
        }
    }

    // =========================================================================
    // OnCastSpell (C# line 386-483)
    // =========================================================================
    void Evade::OnCastSpell(int spellSlot)
    {
        // Check channeled spells (C# line 394-399)
        // TODO: SpellDetector::channeledSpells lookup

        // Block spell commands if evade spell just used (C# line 402-406)
        if (EvadeSpell::lastSpellEvadeCommand.timestamp + ObjectCache::gamePing + 150
            > EvadeUtils::TickCount())
        {
            // args.Process = false; — would need hook blocking
            return;
        }

        lastSpellCast = spellSlot;                                                  // C# line 408
        lastSpellCastTime = EvadeUtils::TickCount();                                // C# line 409

        // Block windup spells while dodging (C# line 418-433)
        if (Situation::ShouldDodge())
        {
            if (isDodging && !SpellDetector::spells.empty())
            {
                for (auto& entry : SpellDetector::windupSpells)
                {
                    SpellData& spellData = entry.second;
                    if (static_cast<int>(spellData.spellKey) == static_cast<int>(spellSlot)) // C# line 426
                    {
                        // args.Process = false; — block
                        return;                                                     // C# line 429
                    }
                }
            }
        }

        // Check evade spell cast for blink/dash handling (C# line 435-482)
        for (auto& evadeSpell : EvadeSpell::evadeSpells)
        {
            if (!evadeSpell.isItem && static_cast<int>(evadeSpell.spellKey) == static_cast<int>(spellSlot) &&
                !evadeSpell.untargetable)
            {
                if (evadeSpell.evadeType == EvadeType::Dash)
                {
                    if (isDodging || EvadeUtils::TickCount() < lastDodgingEndTime + 500)
                    {
                        Vec2 cursorPos = ObjectCache::myHeroCache.serverPos2D; // fallback: no GetCursorWorldPosition in SDK
                        EvadeCommand::MoveTo(cursorPos);
                        lastStopEvadeTime = EvadeUtils::TickCount() + ObjectCache::gamePing + 100;
                    }
                }
                return;                                                             // C# line 481
            }
        }
    }

    // =========================================================================
    // OnIssueOrder (C# line 485-610)
    // =========================================================================
    void Evade::OnIssueOrder(int orderType, const Vec2& targetPos, SDK::GameObject* target)
    {
        if (!Situation::ShouldDodge())                                              // C# line 490
            return;

        const int ORDER_MOVE_TO = 2;    // GameObjectOrder.MoveTo
        const int ORDER_ATTACK  = 3;    // GameObjectOrder.AttackUnit
        const int ORDER_STOP    = 10;   // GameObjectOrder.Stop

        if (orderType == ORDER_MOVE_TO)                                             // C# line 493
        {
            if (isDodging && !SpellDetector::spells.empty())                         // C# line 495
            {
                CheckHeroInDanger();                                                // C# line 497

                {
                    EvadeCommand cmd;
                    cmd.order = EvadeOrderCommand::MoveTo;
                    cmd.isProcessed = false;
                    cmd.timestamp = EvadeUtils::TickCount();
                    cmd.targetPosition = targetPos;
                    lastBlockedUserMoveTo = cmd;
                }                                                                  // C# line 499-505

                // args.Process = false — block                                     // C# line 507
                return;
            }
            else
            {
                int extraDelay = ObjectCache::GetSlider("ExtraPingBuffer");         // C# line 512
                if (EvadeHelper::CheckMovePath(targetPos,
                    ObjectCache::gamePing + (float)extraDelay))                      // C# line 514
                {
                    {
                        EvadeCommand cmd;
                        cmd.order = EvadeOrderCommand::MoveTo;
                        cmd.isProcessed = false;
                        cmd.timestamp = EvadeUtils::TickCount();
                        cmd.targetPosition = targetPos;
                        lastBlockedUserMoveTo = cmd;
                    }                                                              // C# line 532-538

                    // Block movement                                               // C# line 540
                    if (EvadeUtils::TickCount() - lastMovementBlockTime < 500 &&
                        Vec3{lastMovementBlockPos}.To2D().Distance(targetPos) < 100)
                        return;                                                     // C# line 544

                    lastMovementBlockPos = {targetPos.x, 0, targetPos.y};
                    lastMovementBlockTime = EvadeUtils::TickCount();                // C# line 548

                    auto* posInfo = EvadeHelper::GetBestPositionMovementBlock(targetPos);
                    if (posInfo != nullptr)
                        EvadeCommand::MoveTo(posInfo->position);                    // C# line 553
                    return;                                                         // C# line 555
                }
                else
                {
                    lastBlockedUserMoveTo.isProcessed = true;                       // C# line 559
                }
            }
        }
        else // not MoveTo                                                          // C# line 563
        {
            if (isDodging)
            {
                // Block the command                                                // C# line 567
                return;
            }
            // AttackUnit range check (C# line 571-589) — simplified
        }

        // Process passed (C# line 593-609)
        lastIssueOrderGameTime = SDK::Game::GetTime() * 1000;                       // C# line 595
        lastIssueOrderTime = EvadeUtils::TickCount();                               // C# line 596

        if (orderType == ORDER_MOVE_TO) {
            lastMoveToPosition = targetPos;                                         // C# line 601
            auto myHero = SDK::ObjectManager::GetLocalPlayer();
            if (myHero.IsValid()) lastMoveToServerPos = myHero.GetPosition().To2D();         // C# line 602
        }
        if (orderType == ORDER_STOP) {
            auto myHero = SDK::ObjectManager::GetLocalPlayer();
            if (myHero.IsValid()) lastStopPosition = myHero.GetPosition().To2D();            // C# line 607
        }
    }

    // =========================================================================
    // OnProcessSpell (C# line 620-651)
    // =========================================================================
    void Evade::OnProcessSpell(SDK::GameObject* hero, const std::string& spellName,
        int /*spellSlot*/, float castTime)
    {
        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        if (!myHero.IsValid()) return;                                      // C# line 622

        // Channeled spells (C# line 628-632)
        std::string lowerName = spellName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        if (SpellDetector::channeledSpells.count(lowerName)) {
            isChanneling = true;
            channelPosition = myHero.GetPosition().To2D();
        }

        // Windup delay (C# line 634-648)
        if (ObjectCache::GetBool("CalculateWindupDelay")) {
            if (castTime > 0) {
                lastWindupTime = EvadeUtils::TickCount() + castTime;                // C# line 641
                if (isDodging) OnProcessDetectedSpells();                            // C# line 645
            }
        }
    }

    // =========================================================================
    // OnGameUpdate (C# line 653-692) — main update loop
    // =========================================================================
    void Evade::OnGameUpdate()
    {
        ObjectCache::myHeroCache.UpdateInfo();                                      // C# line 657
        CheckHeroInDanger();                                                        // C# line 658

        // Channel check (C# line 660-664)
        if (isChanneling &&
            channelPosition.Distance(ObjectCache::myHeroCache.serverPos2D) > 50)
        {
            isChanneling = false;
        }

        // Reset config check (C# line 666-670)
        if (ObjectCache::GetBool("ResetConfig")) {
            ResetConfig();
            ObjectCache::SetBool("ResetConfig", false);
        }

        // Tick limiter (C# line 672-682)
        int limitDelay = ObjectCache::GetSlider("TickLimiter");
        if (EvadeHelper::fastEvadeMode ||
            EvadeUtils::TickCount() - lastTickCount > (float)limitDelay)
        {
            if (EvadeUtils::TickCount() > lastStopEvadeTime) {
                DodgeSkillShots();                                                  // C# line 677
                ContinueLastBlockedCommand();                                       // C# line 678
            }
            lastTickCount = EvadeUtils::TickCount();                                // C# line 681
        }

        EvadeSpell::UseEvadeSpell();                                                // C# line 684
        CheckDodgeOnlyDangerous();                                                  // C# line 685
        RecalculatePath();                                                          // C# line 686
    }

    // =========================================================================
    // RecalculatePath (C# line 694-731)
    // =========================================================================
    void Evade::RecalculatePath()
    {
        if (!ObjectCache::GetBool("RecalculatePosition") || !isDodging)             // C# line 696
            return;

        if (lastPosInfo != nullptr && !lastPosInfo->recalculatedPath)               // C# line 698
        {
            // Simplified: skip path length check, use current pos
            // Full implementation would check myHero.Path                          // C# line 700-729
        }
    }

    // =========================================================================
    // ContinueLastBlockedCommand (C# line 733-757)
    // =========================================================================
    void Evade::ContinueLastBlockedCommand()
    {
        if (!ObjectCache::GetBool("ContinueMovement") || !Situation::ShouldDodge()) // C# line 735-736
            return;

        Vec2 movePos = lastBlockedUserMoveTo.targetPosition;                        // C# line 738
        int extraDelay = ObjectCache::GetSlider("ExtraPingBuffer");                 // C# line 739

        if (!isDodging && !lastBlockedUserMoveTo.isProcessed                        // C# line 741
            && EvadeUtils::TickCount() - lastEvadeCommand.timestamp > ObjectCache::gamePing + (float)extraDelay
            && EvadeUtils::TickCount() - lastBlockedUserMoveTo.timestamp < 1500)    // C# line 743
        {
            // Add small random offset (C# line 745-746)
            Vec2 diff = movePos - ObjectCache::myHeroCache.serverPos2D;
            float dLen = std::sqrt(diff.x * diff.x + diff.y * diff.y);
            if (dLen > 0) {
                Vec2 dir = {diff.x / dLen, diff.y / dLen};
                movePos = movePos + dir * (float)(rand() % 64 + 1);
            }

            if (!EvadeHelper::CheckMovePath(movePos,
                ObjectCache::gamePing + (float)extraDelay))                          // C# line 748
            {
                EvadeCommand::MoveTo(movePos);                                      // C# line 752
                lastBlockedUserMoveTo.isProcessed = true;                           // C# line 753
            }
        }
    }

    // =========================================================================
    // CheckHeroInDanger (C# line 759-793)
    // =========================================================================
    void Evade::CheckHeroInDanger()
    {
        bool playerInDanger = false;                                                // C# line 761

        auto myHero = SDK::ObjectManager::GetLocalPlayer();
        if (!myHero.IsValid()) return;

        Vec2 heroServerPos = myHero.GetPosition().To2D();

        for (auto& entry : SpellDetector::spells)                                   // C# line 763
        {
            Spell& spell = entry.second;

            if (lastPosInfo != nullptr)                                             // C# line 767
            {
                // Check if spell is in dodgeable list
                bool inDodgeable = false;
                for (int id : lastPosInfo->dodgeableSpells) {
                    if (id == spell.spellID) { inDodgeable = true; break; }
                }

                if (inDodgeable)
                {
                    if (Position::InSkillShot(heroServerPos, spell,
                        ObjectCache::myHeroCache.boundingRadius))                   // C# line 769
                    {
                        playerInDanger = true;
                        break;                                                      // C# line 772
                    }

                    if (ObjectCache::GetBool("EnableEvadeDistance") &&
                        lastPosInfo != nullptr &&
                        EvadeUtils::TickCount() < lastPosInfo->endTime)             // C# line 775-776
                    {
                        playerInDanger = true;
                        break;
                    }
                }
            }
        }

        if (isDodging && !playerInDanger)                                           // C# line 784
            lastDodgingEndTime = EvadeUtils::TickCount();                            // C# line 786

        if (!isDodging && !Situation::ShouldDodge())                                // C# line 789
            return;

        isDodging = playerInDanger;                                                 // C# line 792
    }

    // =========================================================================
    // DodgeSkillShots (C# line 795-867)
    // =========================================================================
    void Evade::DodgeSkillShots()
    {
        if (!Situation::ShouldDodge())                                              // C# line 797
        {
            isDodging = false;                                                      // C# line 799
            return;
        }

        if (isDodging)                                                              // C# line 809
        {
            if (lastPosInfo != nullptr)                                             // C# line 811
            {
                Vec2 lastBestPosition = lastPosInfo->position;                      // C# line 821

                bool clickOnlyOnce = ObjectCache::GetBool("ClickOnlyOnce");         // C# line 823
                if (!clickOnlyOnce)
                {
                    EvadeCommand::MoveTo(lastBestPosition);                         // C# line 827
                    lastEvadeOrderTime = EvadeUtils::TickCount();                    // C# line 828
                }
                else
                {
                    // Check if already moving to this pos                          // C# line 824
                    // Simplified: just move
                    EvadeCommand::MoveTo(lastBestPosition);
                    lastEvadeOrderTime = EvadeUtils::TickCount();
                }
            }
        }
        else // not dodging                                                         // C# line 832
        {
            // Check if hero will walk into a skillshot                             // C# line 834-866
            // Simplified: use CheckMovePath
            auto myHero = SDK::ObjectManager::GetLocalPlayer();
            if (myHero.IsValid())
            {
                // TODO: get hero path and check last waypoint
                // For now, basic check through OnIssueOrder handles this
            }
        }
    }

    // =========================================================================
    // CheckLastMoveTo (C# line 869-885)
    // =========================================================================
    void Evade::CheckLastMoveTo()
    {
        if (EvadeHelper::fastEvadeMode ||
            ObjectCache::GetBool("FastMovementBlock"))                               // C# line 871
        {
            if (!isDodging)                                                         // C# line 873
            {
                // Re-issue last move order                                         // C# line 875-881
                if (lastIssueOrderTime > 0 &&
                    SDK::Game::GetTime() * 1000 - lastIssueOrderGameTime < 500)
                {
                    // Game_OnIssueOrder would be called again                      // C# line 879
                    // lastIssueOrderArgs = null
                }
            }
        }
    }

    // =========================================================================
    // IsDodgeDangerousEnabled (C# line 887-902)
    // =========================================================================
    bool Evade::IsDodgeDangerousEnabled()
    {
        if (ObjectCache::GetBool("DodgeDangerous"))                                 // C# line 889
            return true;

        // Key checks (C# line 894-899)
        if (ObjectCache::GetBool("DodgeDangerousKeyEnabled"))
        {
            // TODO: check key binds
            // if DodgeDangerousKey or DodgeDangerousKey2 active, return true
        }

        return false;                                                               // C# line 901
    }

    // =========================================================================
    // CheckDodgeOnlyDangerous (C# line 904-917)
    // =========================================================================
    void Evade::CheckDodgeOnlyDangerous()
    {
        bool bDodgeOnlyDangerous = IsDodgeDangerousEnabled();                       // C# line 906

        if (!dodgeOnlyDangerous && bDodgeOnlyDangerous)                             // C# line 908
        {
            SpellDetector::RemoveNonDangerousSpells();                              // C# line 910
            dodgeOnlyDangerous = true;                                              // C# line 911
        }
        else
        {
            dodgeOnlyDangerous = bDodgeOnlyDangerous;                               // C# line 915
        }
    }

    // =========================================================================
    // SetAllUndodgeable (C# line 919-922)
    // =========================================================================
    void Evade::SetAllUndodgeable()
    {
        static PositionInfo undodgeable;
        undodgeable = PositionInfo::SetAllUndodgeable();                            // C# line 921
        lastPosInfo = &undodgeable;
    }

    // =========================================================================
    // OnProcessDetectedSpells (C# line 924-991)
    // =========================================================================
    void Evade::OnProcessDetectedSpells()
    {
        ObjectCache::myHeroCache.UpdateInfo();                                      // C# line 926

        if (!ObjectCache::GetBool("DodgeSkillShots"))                               // C# line 928
        {
            SetAllUndodgeable();                                                    // C# line 930
            EvadeSpell::UseEvadeSpell();                                            // C# line 931
            return;
        }

        // Check if hero is in danger zone                                          // C# line 935-936
        if (Position::CheckDangerousPos(ObjectCache::myHeroCache.serverPos2D, 0) ||
            Position::CheckDangerousPos(ObjectCache::myHeroCache.serverPos2DExtra, 0))
        {
            if (EvadeSpell::PreferEvadeSpell())                                     // C# line 938
            {
                SetAllUndodgeable();                                                // C# line 940
            }
            else
            {
                auto* posInfo = EvadeHelper::GetBestPosition();                     // C# line 945

                if (posInfo != nullptr)                                             // C# line 963
                {
                    static PositionInfo lastPosInfoStatic;
                    lastPosInfoStatic = posInfo->CompareLastMovePos();               // C# line 965
                    lastPosInfo = &lastPosInfoStatic;

                    auto myHero = SDK::ObjectManager::GetLocalPlayer();
                    float travelTime = 0;
                    if (myHero.IsValid()) {
                        float dist = ObjectCache::myHeroCache.serverPos2DPing.Distance(lastPosInfo->position);
                        float moveSpeed = myHero.GetMoveSpeed();
                        if (moveSpeed > 0) travelTime = dist / moveSpeed;
                    }
                    lastPosInfo->endTime = EvadeUtils::TickCount() + travelTime * 1000 - 100; // C# line 969
                }

                CheckHeroInDanger();                                                // C# line 972

                if (EvadeUtils::TickCount() > lastStopEvadeTime)                    // C# line 974
                {
                    DodgeSkillShots();                                              // C# line 976
                }

                CheckLastMoveTo();                                                  // C# line 979
                EvadeSpell::UseEvadeSpell();                                         // C# line 980
            }
        }
        else
        {
            static PositionInfo allDodgeable;
            allDodgeable = PositionInfo::SetAllDodgeable();                         // C# line 985
            lastPosInfo = &allDodgeable;
            CheckLastMoveTo();                                                      // C# line 986
        }
    }

} // namespace EzEvade
