#pragma once

#include "../core/CoreAPI.h"
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

    const auto& diag = SDK::Orbwalker::Instance().lastTickDiag;
    const auto mode = SDK::Orbwalker::GetMode();
    const int modeIdx = static_cast<int>(mode);
    static const char* kModeNames[] = { "None","Combo","Harass","Clear","LastHit","Flee" };
    ImGui::Separator();
    ImGui::Text("orb.mode        : %s", (modeIdx >= 0 && modeIdx <= 5) ? kModeNames[modeIdx] : "?");
    ImGui::Text("orb.timeToAtk   : %.0f ms", SDK::Orbwalker::TimeUntilNextAttack() * 1000.0f);
    ImGui::Text("orb.can A/M     : %s / %s", diag.canAttack ? "yes" : "no", diag.canMove ? "yes" : "no");
    ImGui::Text("orb.target      : netId=%d", diag.targetNetId);
    ImGui::Text("orb.orders      : attack=%s move=%s",
        diag.attackIssued ? "yes" : "no", diag.moveIssued ? "yes" : "no");
    {
      const float diagWindup = ctx.cachedAttackWindup * 1000.0f;
      auto* orbMenu = SDK::Orbwalker::GetMenu();
      auto* settMenu = orbMenu ? orbMenu->GetSubMenu("settings") : nullptr;
      const int windupSlider = settMenu ? settMenu->GetSliderValue("windupDelay", 60) : 60;
      const float windupBuf = diagWindup * (static_cast<float>(windupSlider) / 200.0f);
      ImGui::Text("orb.windupBuf   : %.0f ms (%d%% of %.0f ms)",
          windupBuf, windupSlider, diagWindup);
    }

    const auto slotQ = CoreAPI::SpellBook::GetSlot(local.address, 0);
    const auto navGrid = CoreAPI::NavGrid::Get();
    char spellBuf[96] = {};
    slotQ.ReadSpellName(spellBuf, (int)sizeof(spellBuf));
    ImGui::Separator();
    ImGui::Text("Q slot valid    : %s", slotQ.IsValid() ? "yes" : "no");
    ImGui::Text("Q spell name    : %s", spellBuf[0] ? spellBuf : "<empty>");
    ImGui::Text("Q level/cd      : %d / %.3f", slotQ.GetLevel(), slotQ.GetCooldown());
    ImGui::Text("Q range/speed   : %.1f / %.1f", slotQ.GetCastRange(), slotQ.GetMissileSpeed());
    ImGui::Text("Q width/type    : %.1f / %d", slotQ.GetLineWidth(), slotQ.GetCastType());
    ImGui::Text("Q mana/castable : %.1f / %s", slotQ.GetManaCost(), CoreAPI::SpellBook::CanCast(local.address, 0, ctx.gameTime) ? "yes" : "no");
    ImGui::Text("Q state         : %s", SpellStateName(CoreAPI::SpellBook::GetSpellState(local.address, 0, ctx.gameTime)));
    ImGui::Text("Q castArg       : 0x%llX", (unsigned long long)slotQ.GetCastArgument());

    const auto activeCast = CoreAPI::SpellCast::GetActive(local.address);
    char activeSpellBuf[96] = {};
    activeCast.ReadSpellName(activeSpellBuf, (int)sizeof(activeSpellBuf));
    ImGui::Text("activeCast      : %s", activeCast.IsValid() ? "yes" : "no");
    ImGui::Text("activeSpell     : %s", activeSpellBuf[0] ? activeSpellBuf : "<empty>");
    ImGui::Text("activeSlot      : %d", activeCast.GetSlot());
    ImGui::Text("activeSrc/Tgt   : %d / %d", activeCast.GetSourceIndex(), activeCast.GetTargetIndex());
    const Vec3 castStart = activeCast.GetStartPos();
    const Vec3 castEnd = activeCast.GetEndPos();
    ImGui::Text("activeStart     : %.1f %.1f %.1f", castStart.x, castStart.y, castStart.z);
    ImGui::Text("activeEnd       : %.1f %.1f %.1f", castEnd.x, castEnd.y, castEnd.z);
    ImGui::Text("activeFlags     : spell=%s auto=%s special=%s",
        activeCast.IsSpell() ? "yes" : "no",
        activeCast.IsAutoAttack() ? "yes" : "no",
        activeCast.IsSpecialAttack() ? "yes" : "no");

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
