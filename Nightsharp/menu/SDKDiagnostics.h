#pragma once

#include "../core/CoreAPI.h"
#include "../core/CoreClassification.h"
#include "../core/CoreEventHook.h"
#include "../core/CoreObjects.h"
#include "../core/CoreRuntime.h"
#include "../core/CoreValidation.h"
#include "../imgui/imgui.h"
#include "../sdk/Core/Objects.h"
#include "../sdk/Wrappers/Orbwalking/Orbwalker.h"

namespace SDKDiagnostics {

inline const char* SpellStateName(CoreSpellBook::SpellState state) {
    switch (state) {
    case CoreSpellBook::State_Ready: return "Ready";
    case CoreSpellBook::State_NotLearned: return "NotLearned";
    case CoreSpellBook::State_Cooldown: return "Cooldown";
    case CoreSpellBook::State_NoMana: return "NoMana";
    case CoreSpellBook::State_Disabled: return "Disabled";
    default: return "Unknown";
    }
}

inline void SectionHeader(const char* title) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.47f, 0.92f, 0.47f, 1.0f), "%s", title);
    ImGui::Separator();
}

inline void Render() {
    SectionHeader("Runtime Diagnostics");
    ImGui::TextWrapped("This build is moving to a core-first runtime. SDK will sit on top of core once core primitives are stable.");
    ImGui::Spacing();
    ImGui::BulletText("Core runtime init: %s", CoreRuntime::HasInitReady() ? "ready" : "not ready");
    ImGui::BulletText("Core runtime state: %s", CoreRuntime::IsReady() ? "runtime-ready" : "waiting for live game state");
    ImGui::BulletText("Retained data file: sdk/Data/Database.h");
    ImGui::BulletText("Retained data file: sdk/Data/26.6.h");
    ImGui::Separator();

    const auto& ctx = CoreRuntime::GetContext();
    ImGui::Text("base            : 0x%llX", (unsigned long long)ctx.moduleBase);
    ImGui::Text("localPlayer     : 0x%llX", (unsigned long long)ctx.localPlayer);
    ImGui::Text("objectManager   : 0x%llX", (unsigned long long)ctx.objectManager);
    ImGui::Text("heroManager     : 0x%llX", (unsigned long long)ctx.heroManager);
    ImGui::Text("minionManager   : 0x%llX", (unsigned long long)ctx.minionManager);
    ImGui::Text("turretManager   : 0x%llX", (unsigned long long)ctx.turretManager);
    ImGui::Text("missileManager  : 0x%llX", (unsigned long long)ctx.missileManager);
    ImGui::Text("navGrid         : 0x%llX", (unsigned long long)ctx.navGrid);
    ImGui::Text("netInstance     : 0x%llX", (unsigned long long)ctx.netInstance);
    ImGui::Text("shopInstance    : 0x%llX", (unsigned long long)ctx.shopInstance);
    ImGui::Text("renderer        : 0x%llX", (unsigned long long)ctx.renderer);
    ImGui::Text("viewProjInst    : 0x%llX", (unsigned long long)ctx.viewProjInstance);
    ImGui::Text("worldToScreenFn : 0x%llX", (unsigned long long)ctx.worldToScreenFn);
    ImGui::Text("issueOrderFn    : 0x%llX", (unsigned long long)ctx.issueOrderFn);
    ImGui::Text("castSpellFn     : 0x%llX", (unsigned long long)ctx.castSpellFn);
    ImGui::Text("gameTime        : %.3f", ctx.gameTime);
    ImGui::Text("statusMask      : 0x%08X", ctx.statusMask);
    ImGui::Text("lastErrorMask   : 0x%08X", ctx.lastErrorMask);
    ImGui::Text("validationMask  : 0x%08X", ctx.validationMask);
    ImGui::Text("initGeneration  : %u", ctx.initGeneration);
    ImGui::Text("refreshGen      : %u", ctx.refreshGeneration);
    ImGui::Text("phaseGen        : %u", ctx.phaseGeneration);
    ImGui::Text("phase           : %u", ctx.currentPhase);
    ImGui::Text("spoofTrampoline : 0x%llX", (unsigned long long)ctx.spoofTrampoline);

    // ── Event Hook Status ──
    ImGui::Text("hook.ProcessSpell: %s", CoreEventHook::IsProcessSpellHooked() ? "ON" : "off");
    ImGui::Text("hook.StopCast    : %s", CoreEventHook::IsStopCastHooked() ? "ON" : "off");
    ImGui::Text("hook.FinishCast  : %s", CoreEventHook::IsFinishCastHooked() ? "ON" : "off");
    ImGui::Text("hook.BuffAdd     : %s", CoreEventHook::IsBuffAddHooked() ? "ON" : "off");
    ImGui::Text("hook.SpellImpact : %s", CoreEventHook::IsSpellImpactHooked() ? "ON" : "off");
    ImGui::Text("hook.CreateObj   : %s", CoreEventHook::IsCreateObjectHooked() ? "ON" : "off");
    ImGui::Text("hook.GameUpdate  : %s", CoreEventHook::IsGameUpdateHooked() ? "ON" : "off");
    ImGui::Text("hook.HeroAction  : %s", CoreEventHook::IsHeroActionStateHooked() ? "ON" : "off");
    ImGui::Text("hook.MinionFollow: %s", CoreEventHook::IsMinionFollowHooked() ? "ON" : "off");
    ImGui::Text("detectWatcher2  : 0x%llX", (unsigned long long)ctx.detectionWatcher2);
    ImGui::Text("ping            : %d", ctx.cachedPing);
    ImGui::Text("attackDelay     : %.4f", ctx.cachedAttackDelay);
    ImGui::Text("attackWindup    : %.4f", ctx.cachedAttackWindup);
    ImGui::Text("issueOrder ok   : %u / %u", ctx.issueOrderSuccesses, ctx.issueOrderAttempts);
    ImGui::Text("cast ok         : %u / %u", ctx.castSuccesses, ctx.castAttempts);
    ImGui::Text("canIssueOrder   : %s", CoreAPI::Control::CanIssueOrder() ? "yes" : "no");
    ImGui::Text("canCastSpell    : %s", CoreAPI::Control::CanCastSpell() ? "yes" : "no");
    ImGui::Text("canChargedSpell : %s", CoreAPI::Control::CanUpdateChargedSpell() ? "yes" : "no");
    ImGui::Text("castHelper      : %s", (ctx.validationMask & CoreValidation::Validation_CastHelperPresent) ? "present" : "missing");
    ImGui::Text("castAbiApproved : %s", (ctx.validationMask & CoreValidation::Validation_CastCallableApproved) ? "yes" : "no");

    uintptr_t heroes[16] = {};
    uintptr_t allyHeroes[16] = {};
    uintptr_t enemyHeroes[16] = {};
    uintptr_t minions[512] = {};
    uintptr_t allyMinions[512] = {};
    uintptr_t enemyMinions[512] = {};
    uintptr_t jungleMinions[512] = {};
    uintptr_t turrets[64] = {};
    uintptr_t allyTurrets[64] = {};
    uintptr_t enemyTurrets[64] = {};
    uintptr_t missiles[1024] = {};
    const int heroCount = CoreObjects::EnumerateHeroes(heroes, 16);
    const int allyHeroCount = CoreObjects::EnumerateAllyHeroes(allyHeroes, 16);
    const int enemyHeroCount = CoreObjects::EnumerateEnemyHeroes(enemyHeroes, 16);
    const int minionCount = CoreObjects::EnumerateMinions(minions, 512);
    const int allyMinionCount = CoreObjects::EnumerateAllyMinions(allyMinions, 512);
    const int enemyMinionCount = CoreObjects::EnumerateEnemyMinions(enemyMinions, 512);
    const int jungleMinionCount = CoreObjects::EnumerateJungleMinions(jungleMinions, 512);
    const int turretCount = CoreObjects::EnumerateTurrets(turrets, 64);
    const int allyTurretCount = CoreObjects::EnumerateAllyTurrets(allyTurrets, 64);
    const int enemyTurretCount = CoreObjects::EnumerateEnemyTurrets(enemyTurrets, 64);
    const int missileCount = CoreObjects::EnumerateMissiles(missiles, 1024);

    ImGui::Separator();
    ImGui::Text("heroCount       : %d", heroCount);
    ImGui::Text("hero ally/enemy : %d / %d", allyHeroCount, enemyHeroCount);
    ImGui::Text("minionCount     : %d", minionCount);
    ImGui::Text("minion A/E/J    : %d / %d / %d", allyMinionCount, enemyMinionCount, jungleMinionCount);
    ImGui::Text("turretCount     : %d", turretCount);
    ImGui::Text("turret ally/en  : %d / %d", allyTurretCount, enemyTurretCount);
    ImGui::Text("missileCount    : %d", missileCount);

    const auto local = CoreObjects::GetLocalPlayer();
    char nameBuf[64] = {};
    char champBuf[64] = {};
    local.ReadName(nameBuf, (int)sizeof(nameBuf));
    local.ReadCharacterName(champBuf, (int)sizeof(champBuf));
    ImGui::Text("playerName      : %s", nameBuf[0] ? nameBuf : "<empty>");
    ImGui::Text("characterName   : %s", champBuf[0] ? champBuf : "<empty>");
    ImGui::Text("playerNetId     : %d", local.GetNetId());
    ImGui::Text("playerTeam      : %d", local.GetTeam());
    ImGui::Text("playerHP        : %.1f / %.1f", local.GetHealth(), local.GetMaxHealth());
    ImGui::Text("playerEffHP     : %.1f", local.GetEffectiveHealth());
    ImGui::Text("playerMP        : %.1f / %.1f", local.GetMana(), local.GetMaxMana());
    ImGui::Text("playerMS        : %.1f", local.GetMoveSpeed());
    ImGui::Text("playerRange     : %.1f", local.GetAttackRange());
    ImGui::Text("playerMelee     : %s", local.IsMelee() ? "yes" : "no");
    ImGui::Text("playerDeadInvul : %s / %s", local.IsDead() ? "yes" : "no", local.IsInvulnerable() ? "yes" : "no");

    Vec2 screen = {};
    const bool w2sOk = CoreAPI::View::WorldToScreen(local.GetPosition(), screen);
    ImGui::Text("playerW2S       : %s  (%.1f, %.1f)", w2sOk ? "OK" : "FAIL", screen.x, screen.y);

    const Vec3 mouseWorld = CoreAPI::View::GetMouseWorldPos();
    const Vec2 mouseScreen = CoreAPI::View::GetMouseScreenPos();
    ImGui::Text("mouseWorld      : %.1f %.1f %.1f", mouseWorld.x, mouseWorld.y, mouseWorld.z);
    ImGui::Text("mouseScreen     : %.1f %.1f", mouseScreen.x, mouseScreen.y);
    ImGui::Text("selectedNetId   : %u", CoreAPI::View::GetSelectedNetId());
    ImGui::Text("gameFocused     : %s", CoreAPI::Game::IsGameFocused() ? "yes" : "no");
    ImGui::Text("chatOpen        : %s", CoreAPI::Game::IsChatOpen() ? "yes" : "no");
    ImGui::Text("shopOpen        : %s", CoreAPI::Game::IsShopOpen() ? "yes" : "no");
    ImGui::Text("processInput    : %s", CoreAPI::Game::ShouldProcessInput() ? "yes" : "no");
    ImGui::Text("sdk.IsRecalling : %s", SDK::ObjectManager::Player().IsRecalling() ? "yes" : "no");
    const bool recallTypeFlag = RuntimeAPI::CompareTypeFlags(local.address, Offset::TypeFlags::IsRecalling);
    const int recallStateRaw = local.GetRecallState();
    const bool recallBuffAura = CoreBuffs::HasBuffContaining(local.address, "recall", 1);
    const bool recallBuffAny = CoreBuffs::HasBuffContaining(local.address, "recall");
    const auto recallCast = local.GetActiveSpellCast();
    char recallCastName[96] = {};
    recallCast.ReadSpellName(recallCastName, static_cast<int>(sizeof(recallCastName)));
    char recallAnimName[128] = {};
    local.ReadCurrentAnimation(recallAnimName, static_cast<int>(sizeof(recallAnimName)));
    ImGui::Text("sdk.recall flag : %s", recallTypeFlag ? "yes" : "no");
    ImGui::Text("sdk.recall raw  : %d", recallStateRaw);
    ImGui::Text("sdk.recall buff : aura=%s any=%s", recallBuffAura ? "yes" : "no", recallBuffAny ? "yes" : "no");
    ImGui::Text("sdk.recall cast : slot=%d name=%s", recallCast.GetSlot(), recallCastName[0] ? recallCastName : "<empty>");
    ImGui::Text("sdk.recall anim : %s", recallAnimName[0] ? recallAnimName : "<empty>");
    ImGui::Text("sdk.recall cancel: moving=%s dashing=%s",
        local.IsMovingOnPath() ? "yes" : "no",
        local.IsDashingOnPath() ? "yes" : "no");

    const auto& orbDbg = SDK::Orbwalker::GetDebugState();
    ImGui::Separator();
    ImGui::Text("orb.mode        : %d", orbDbg.activeMode);
    ImGui::Text("orb.reason      : %d", orbDbg.reason);
    ImGui::Text("orb.enabled     : %s", orbDbg.enabled ? "yes" : "no");
    ImGui::Text("orb.can A/M     : %d / %d",
        orbDbg.canAttack ? 1 : 0,
        orbDbg.canMove ? 1 : 0);
    ImGui::Text("orb.attack src  : timing=%d native=%d",
        orbDbg.timingReady ? 1 : 0,
        orbDbg.nativeCanAttack ? 1 : 0);
    ImGui::Text("orb.target      : has=%d inRange=%d netId=%d",
        orbDbg.hasTarget ? 1 : 0,
        orbDbg.targetInRange ? 1 : 0,
        orbDbg.targetNetId);
    ImGui::Text("orb.orders      : attack=%d move=%d",
        orbDbg.attackIssued ? 1 : 0,
        orbDbg.moveIssued ? 1 : 0);
    // Show effective windup buffer from percentage slider
    {
      const float diagWindup = ctx.cachedAttackWindup * 1000.0f;
      auto* orbMenu = SDK::Orbwalker::GetMenu();
      auto* advMenu = orbMenu ? orbMenu->GetSubMenu("advanced") : nullptr;
      const int windupSlider = advMenu ? advMenu->GetSliderValue("delayWindup", 80) : 80;
      const float windupBuf = diagWindup * (static_cast<float>(windupSlider) / 200.0f);
      ImGui::Text("orb.windupBuf   : %.0f ms (%d%% of %.0f ms)",
          windupBuf, windupSlider, diagWindup);
    }

    const auto slotQ = CoreAPI::SpellBook::GetSlot(local.address, 0);
    const auto navGrid = CoreAPI::NavGrid::Get();

    // ── Q Spell: all offset verification ──
    SectionHeader("SpellSlot Q — Offset Verification");
    char spellBuf[96] = {};
    slotQ.ReadSpellName(spellBuf, (int)sizeof(spellBuf));
    char spellNameRes[96] = {};
    slotQ.ReadSpellNameFromResource(spellNameRes, (int)sizeof(spellNameRes));
    ImGui::Text("Q slot valid    : %s  addr=0x%llX", slotQ.IsValid() ? "yes" : "no", (unsigned long long)slotQ.address);
    ImGui::Text("Q spellName     : %s", spellBuf[0] ? spellBuf : "<empty>");
    ImGui::Text("Q nameFromSDR   : %s", spellNameRes[0] ? spellNameRes : "<empty>");
    ImGui::Text("Q nameHash      : 0x%08X", slotQ.GetSpellNameHash());
    ImGui::Text("Q level(0x1C)   : %d", slotQ.GetLevel());
    ImGui::Text("Q levelAlt(0x28): %d", slotQ.GetLevelAlt());
    ImGui::Text("Q cd(0x80)      : %.3f", slotQ.GetCooldown());
    ImGui::Text("Q totalCd(0x88) : %.3f", slotQ.GetTotalCooldown());
    ImGui::Text("Q cdExpires(0x70): %.3f", slotQ.GetCooldownExpires());
    ImGui::Text("Q chargeTimer(0x30): %.3f", slotQ.GetChargeTimer());
    ImGui::Text("Q stacks(0x5C)  : %d", slotQ.GetStacks());
    ImGui::Text("Q casting(0x118): %s  ptr=0x%llX",
        slotQ.IsSlotCasting() ? "yes" : "no",
        (unsigned long long)slotQ.GetSlotActiveSpellCast());
    ImGui::Text("Q instVars(0x108): 0x%llX", (unsigned long long)slotQ.GetSpellInstanceVars());
    ImGui::Text("Q range/speed   : %.1f / %.1f", slotQ.GetCastRange(), slotQ.GetMissileSpeed());
    ImGui::Text("Q width/type    : %.1f / %d", slotQ.GetLineWidth(), slotQ.GetCastType());
    ImGui::Text("Q mana/castable : %.1f / %s", slotQ.GetManaCost(), CoreAPI::SpellBook::CanCast(local.address, 0, ctx.gameTime) ? "yes" : "no");
    ImGui::Text("Q state         : %s", SpellStateName(CoreAPI::SpellBook::GetSpellState(local.address, 0, ctx.gameTime)));

    // ── W/E/R summary (compact) ──
    {
        const char* slotNames[] = {"W", "E", "R"};
        for (int si = 1; si <= 3; ++si) {
            const auto slot = CoreAPI::SpellBook::GetSlot(local.address, si);
            char sn[96] = {};
            slot.ReadSpellName(sn, (int)sizeof(sn));
            ImGui::Text("%s: lv=%d cd=%.1f hash=0x%08X cast=%s name=%s",
                slotNames[si-1], slot.GetLevel(), slot.GetCooldown(),
                slot.GetSpellNameHash(), slot.IsSlotCasting() ? "Y" : "N",
                sn[0] ? sn : "?");
        }
    }

    // ── Active SpellCast (SpellBook level) ──
    SectionHeader("Active SpellCast");
    const auto activeCast = CoreAPI::SpellCast::GetActive(local.address);
    char activeSpellBuf[96] = {};
    activeCast.ReadSpellName(activeSpellBuf, (int)sizeof(activeSpellBuf));
    ImGui::Text("activeCast      : %s  addr=0x%llX", activeCast.IsValid() ? "yes" : "no",
        (unsigned long long)activeCast.address);
    ImGui::Text("activeSpell     : %s", activeSpellBuf[0] ? activeSpellBuf : "<empty>");
    ImGui::Text("activeSlot      : %d", activeCast.GetSlot());
    ImGui::Text("activeSrc(0x98) : %d", activeCast.GetSourceIndex());
    ImGui::Text("activeTgt(0x9C) : %d", activeCast.GetTargetIndex());
    // Show raw reads at old vs new TargetIndex for verification
    if (activeCast.IsValid()) {
        const int tgtNew = Globals::Read<int>(activeCast.address + 0x9C);
        const int tgtOld = Globals::Read<int>(activeCast.address + 0x108);
        ImGui::TextColored(tgtNew != 0 ? ImVec4(0.4f,1.0f,0.4f,1.0f) : ImVec4(0.6f,0.6f,0.6f,1.0f),
            "  tgt@0x9C=%d  tgt@0x108=%d  (0x9C=correct)", tgtNew, tgtOld);
    }
    const Vec3 castStart = activeCast.GetStartPos();
    const Vec3 castEnd = activeCast.GetEndPos();
    ImGui::Text("activeStart     : %.1f %.1f %.1f", castStart.x, castStart.y, castStart.z);
    ImGui::Text("activeEnd       : %.1f %.1f %.1f", castEnd.x, castEnd.y, castEnd.z);
    ImGui::Text("activeDelay     : %.3f", activeCast.GetCastDelay());
    ImGui::Text("activeFlags     : spell=%s auto=%s special=%s",
        activeCast.IsSpell() ? "yes" : "no",
        activeCast.IsAutoAttack() ? "yes" : "no",
        activeCast.IsSpecialAttack() ? "yes" : "no");
    if (activeCast.IsValid()) {
        ImGui::Text("activeMissSpd   : %.1f", activeCast.GetMissileSpeed());
    }

    uintptr_t buffAddrs[64] = {};
    const int buffCount = CoreAPI::Buffs::Enumerate(local.address, buffAddrs, 64);
    ImGui::Text("buffCount       : %d", buffCount);
    ImGui::Text("buff stun/snare : %s / %s",
        CoreAPI::Buffs::HasBuffType(local.address, 5) ? "yes" : "no",
        CoreAPI::Buffs::HasBuffType(local.address, 11) ? "yes" : "no");

    // ── Active Buff List (for recall debugging) ──
    if (ImGui::TreeNode("Active Buffs (expand to see all)")) {
        const float gameTime = CoreAPI::Game::GetTime();
        int activeCount = 0;
        for (int i = 0; i < buffCount && i < 64; ++i) {
            CoreBuffs::BuffRef buff{ buffAddrs[i] };
            if (!buff.IsValid()) continue;
            if (!buff.IsActive(gameTime)) continue;

            char bName[96] = {};
            buff.ReadName(bName, static_cast<int>(sizeof(bName)));
            if (!bName[0]) continue;

            const int bType = buff.GetType();
            const int bStacks = buff.GetStacks();
            const float bRemaining = buff.GetRemainingTime(gameTime);

            // Highlight recall-related buffs in yellow
            bool isRecallBuff = false;
            for (const char* p = bName; *p; ++p) {
                if ((*p == 'r' || *p == 'R') &&
                    (_strnicmp(p, "recall", 6) == 0)) {
                    isRecallBuff = true;
                    break;
                }
            }

            if (isRecallBuff) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f),
                    "[%d] \"%s\" type=%d stacks=%d remain=%.2fs",
                    activeCount, bName, bType, bStacks, bRemaining);
            } else {
                ImGui::Text("[%d] \"%s\" type=%d stacks=%d remain=%.2fs",
                    activeCount, bName, bType, bStacks, bRemaining);
            }
            activeCount++;
        }
        if (activeCount == 0) {
            ImGui::TextDisabled("(no active buffs)");
        }
        ImGui::TreePop();
    }

    Vec3 waypoints[16] = {};
    const auto ai = CoreAPI::Ai::Get(local.address);
    const int waypointCount = CoreAPI::Ai::CopyWaypoints(local.address, waypoints, 16);
    const Vec3 serverPos = CoreAPI::Ai::GetServerPosition(local.address);
    ImGui::Text("ai.inner        : 0x%llX", (unsigned long long)ai.inner);
    ImGui::Text("ai.navBase      : 0x%llX", (unsigned long long)ai.navBase);
    ImGui::Text("ai.moving       : %s", CoreAPI::Ai::IsMoving(local.address) ? "yes" : "no");
    ImGui::Text("ai.hasPath      : %s", CoreAPI::Ai::HasPath(local.address) ? "yes" : "no");
    ImGui::Text("ai.dashing      : %s", CoreAPI::Ai::IsDashing(local.address) ? "yes" : "no");
    ImGui::Text("ai.dashSpeed    : %.2f", CoreAPI::Ai::GetDashSpeed(local.address));
    ImGui::Text("ai.dashTgtNetId : %u", CoreAi::GetDashTargetNetId(local.address));
    ImGui::Text("ai.dashDuration : %.3f", CoreAi::GetDashDuration(local.address));
    ImGui::Text("ai.dashDistRem  : %.2f", CoreAi::GetDashDistRemaining(local.address));
    ImGui::Text("ai.arrived      : %s", CoreAi::HasArrived(local.address) ? "yes" : "no");
    ImGui::Text("ai.serverPos    : %.1f %.1f %.1f", serverPos.x, serverPos.y, serverPos.z);
    ImGui::Text("ai.waypoints    : %d", waypointCount);
    const Vec3 velocity = CoreAPI::Ai::GetVelocity(local.address);
    const Vec3 pathStart = CoreAPI::Ai::GetPathStart(local.address);
    const Vec3 pathEnd = CoreAPI::Ai::GetPathEnd(local.address);
    const Vec3 orderPos = CoreAPI::Ai::GetOrderPosition(local.address);
    ImGui::Text("ai.velocity     : %.1f %.1f %.1f", velocity.x, velocity.y, velocity.z);
    ImGui::Text("ai.pathStart    : %.1f %.1f %.1f", pathStart.x, pathStart.y, pathStart.z);
    ImGui::Text("ai.pathEnd      : %.1f %.1f %.1f", pathEnd.x, pathEnd.y, pathEnd.z);
    ImGui::Text("ai.orderPos     : %.1f %.1f %.1f", orderPos.x, orderPos.y, orderPos.z);

    // ── Hero Fields ──
    SectionHeader("Hero Fields");
    ImGui::Text("hero.gold       : %.1f", local.GetGold());
    ImGui::Text("hero.goldTotal  : %.1f", local.GetGoldTotal());
    ImGui::Text("hero.exp        : %.1f", local.GetExp());
    ImGui::Text("hero.level      : %d  (pts=%d)", local.GetLevel(), local.GetLevelUpPoints());
    ImGui::Text("hero.champHash  : 0x%08X", local.GetChampionHash());

    // ── Shop ──
    ImGui::Text("shop.open       : %s", (Globals::Read<int>(ctx.moduleBase + Offset::Shop::IsShopOpen) != 0) ? "yes" : "no");

    // ── Object Classification (nearby objects) ──
    SectionHeader("Classification (nearby)");
    {
        const Vec3 myPos = local.GetPosition();
        const int myTeam = local.GetTeam();
        uintptr_t minions[128] = {};
        const int mCount = CoreObjects::EnumerateMinions(minions, 128);
        int shown = 0;
        for (int i = 0; i < mCount && shown < 8; ++i) {
            CoreObjects::ObjectRef obj{ minions[i] };
            if (!obj.IsValid()) continue;
            if (obj.GetPosition().Distance2D(myPos) > 1500.0f) continue;
            char oName[64] = {};
            obj.ReadName(oName, sizeof(oName));
            auto otype = CoreClassification::Classify(minions[i]);
            bool attackable = CoreClassification::IsAttackable(minions[i], myTeam);
            bool ignore = CoreClassification::ShouldIgnore(minions[i]);
            ImGui::TextColored(
                ignore ? ImVec4(0.5f,0.5f,0.5f,1.0f) : (attackable ? ImVec4(0.4f,1.0f,0.4f,1.0f) : ImVec4(1.0f,0.4f,0.4f,1.0f)),
                "[%s] %s hp=%.0f atk=%s ign=%s",
                CoreClassification::TypeName(otype),
                oName[0] ? oName : "?",
                obj.GetHealth(),
                attackable ? "Y" : "N",
                ignore ? "Y" : "N");
            shown++;
        }
        if (shown == 0) ImGui::TextDisabled("(no nearby minions)");
    }

    ImGui::Separator();
    ImGui::Text("nav.valid       : %s", navGrid.IsValid() ? "yes" : "no");
    ImGui::Text("nav.manager     : 0x%llX", (unsigned long long)navGrid.manager);
    ImGui::Text("nav.size        : %d x %d", navGrid.width, navGrid.height);
    ImGui::Text("nav.scale       : %.3f / %.6f", navGrid.cellSize, navGrid.inverseScale);
    ImGui::Text("nav.bounds      : %.1f %.1f -> %.1f %.1f", navGrid.minX, navGrid.minZ, navGrid.maxX, navGrid.maxZ);
    ImGui::Text("nav.local       : wall=%s walk=%s brush=%s",
        CoreAPI::NavGrid::IsWall(local.GetPosition()) ? "yes" : "no",
        CoreAPI::NavGrid::IsWalkable(local.GetPosition()) ? "yes" : "no",
        CoreAPI::NavGrid::IsBrush(local.GetPosition()) ? "yes" : "no");
}

} // namespace SDKDiagnostics
