#pragma once

#include "CoreAi.h"
#include "CoreBuffs.h"
#include "CoreControl.h"
#include "CoreGame.h"
#include "CoreNavGrid.h"
#include "CoreObjects.h"
#include "CoreRuntime.h"
#include "CoreSpellCastInfo.h"
#include "CoreSpellBook.h"
#include "CoreValidation.h"
#include "CoreView.h"

namespace CoreAPI {

    namespace State {
        inline bool IsInitialized() {
            return CoreRuntime::HasInitReady();
        }

        inline bool IsRuntimeReady() {
            return CoreRuntime::IsReady();
        }

        inline uint32_t GetStatusMask() {
            return CoreRuntime::GetContext().statusMask;
        }

        inline uint32_t GetLastErrorMask() {
            return CoreRuntime::GetContext().lastErrorMask;
        }

        inline uint32_t GetValidationMask() {
            return CoreRuntime::GetContext().validationMask;
        }

        inline bool Refresh() {
            return CoreRuntime::RefreshReadState();
        }

        inline uint32_t Validate() {
            return CoreValidation::Refresh();
        }
    }

    namespace Game {
        using MapId = CoreGame::MapId;

        inline uintptr_t GetBase() {
            return CoreRuntime::GetContext().moduleBase;
        }

        inline float GetTime() {
            return CoreRuntime::GetContext().gameTime;
        }

        inline uintptr_t GetNetInstance() {
            return CoreRuntime::GetContext().netInstance;
        }

        inline uintptr_t GetChatClient() {
            return CoreRuntime::GetContext().chatClient;
        }

        inline uintptr_t GetShopInstance() {
            return CoreRuntime::GetContext().shopInstance;
        }

        inline uintptr_t GetOpenWindowsArray() {
            return CoreRuntime::GetContext().openWindowsArray;
        }

        inline uint32_t GetOpenWindowsCount() {
            return CoreRuntime::GetContext().openWindowsCount;
        }

        inline bool HasLiveGame() {
            return CoreRuntime::IsReady();
        }

        inline MapId GetMapId() {
            return CoreGame::GetMapId();
        }

        inline bool IsChatOpen() {
            return CoreGame::IsChatOpen();
        }

        inline bool IsGameFocused() {
            return CoreGame::IsGameFocused();
        }

        inline bool IsShopOpen() {
            return CoreGame::IsShopOpen();
        }

        inline bool IsChatOpenByKeyboard() {
            return CoreGame::IsChatOpenByKeyboard();
        }

        inline uint32_t GetInputBlockMask() {
            return CoreGame::GetInputBlockMask();
        }

        inline CoreGame::InputDebugState GetInputDebugState() {
            return CoreGame::GetInputDebugState();
        }

        inline bool ShouldProcessInput() {
            return CoreGame::ShouldProcessInput();
        }
    }

    namespace Objects {
        inline uintptr_t GetLocalPlayer() {
            return CoreRuntime::GetContext().localPlayer;
        }

        inline uintptr_t GetObjectManager() {
            return CoreRuntime::GetContext().objectManager;
        }

        inline uintptr_t GetHeroManager() {
            return CoreRuntime::GetContext().heroManager;
        }

        inline uintptr_t GetMinionManager() {
            return CoreRuntime::GetContext().minionManager;
        }

        inline uintptr_t GetTurretManager() {
            return CoreRuntime::GetContext().turretManager;
        }

        inline uintptr_t GetMissileManager() {
            return CoreRuntime::GetContext().missileManager;
        }

        inline uintptr_t GetNavGrid() {
            return CoreRuntime::GetContext().navGrid;
        }

        inline uintptr_t GetUnderMouseObject() {
            return CoreRuntime::GetContext().underMouseObject;
        }

        inline int EnumerateHeroes(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumerateHeroes(out, maxOut);
        }

        inline int EnumerateAllyHeroes(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumerateAllyHeroes(out, maxOut);
        }

        inline int EnumerateEnemyHeroes(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumerateEnemyHeroes(out, maxOut);
        }

        inline int EnumerateMinions(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumerateMinions(out, maxOut);
        }

        inline int EnumerateAllyMinions(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumerateAllyMinions(out, maxOut);
        }

        inline int EnumerateEnemyMinions(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumerateEnemyMinions(out, maxOut);
        }

        inline int EnumerateJungleMinions(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumerateJungleMinions(out, maxOut);
        }

        inline int EnumeratePlants(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumeratePlants(out, maxOut);
        }

        inline int EnumeratePets(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumeratePets(out, maxOut);
        }

        inline int EnumerateTurrets(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumerateTurrets(out, maxOut);
        }

        inline int EnumerateAllyTurrets(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumerateAllyTurrets(out, maxOut);
        }

        inline int EnumerateEnemyTurrets(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumerateEnemyTurrets(out, maxOut);
        }

        inline int EnumerateMissiles(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumerateMissiles(out, maxOut);
        }

        inline int EnumerateAll(uintptr_t* out, int maxOut) {
            return CoreObjects::EnumerateAllObjects(out, maxOut);
        }

        inline CoreObjects::ObjectRef GetLocalPlayerRef() {
            return CoreObjects::GetLocalPlayer();
        }

        inline CoreObjects::ObjectRef GetUnderMouseRef() {
            return CoreObjects::GetUnderMouseObject();
        }

        inline CoreObjects::ObjectRef FindByNetId(int netId) {
            return CoreObjects::FindByNetId(netId);
        }

        inline CoreObjects::ObjectRef FindByIndex(int index) {
            return CoreObjects::FindByIndex(index);
        }

        inline int CountEnemyHeroesInRange(float range, const Vec3& from = Vec3()) {
            return CoreObjects::CountEnemyHeroesInRange(range, from);
        }

        inline int CountAllyHeroesInRange(float range, const Vec3& from = Vec3()) {
            return CoreObjects::CountAllyHeroesInRange(range, from);
        }

        inline int CountEnemyMinionsInRange(float range, const Vec3& from = Vec3()) {
            return CoreObjects::CountEnemyMinionsInRange(range, from);
        }

        inline int CountAllyMinionsInRange(float range, const Vec3& from = Vec3()) {
            return CoreObjects::CountAllyMinionsInRange(range, from);
        }
    }

    namespace View {
        inline uintptr_t GetHudInstance() {
            return CoreRuntime::GetContext().hudInstance;
        }

        inline uintptr_t GetRenderer() {
            return CoreRuntime::GetContext().renderer;
        }

        inline uintptr_t GetViewport() {
            return CoreRuntime::GetContext().viewPort;
        }

        inline uintptr_t GetViewProjInstance() {
            return CoreRuntime::GetContext().viewProjInstance;
        }

        inline uintptr_t GetCursorInstance() {
            return CoreRuntime::GetContext().cursorInstance;
        }

        inline uintptr_t GetMouseScreenVec2() {
            return CoreRuntime::GetContext().mouseScreenVec2;
        }

        inline uintptr_t GetHudSpellInfo() {
            return CoreView::GetHudSpellInfo();
        }

        inline bool WorldToScreen(const Vec3& world, Vec2& screen) {
            return CoreView::WorldToScreen(world, screen);
        }

        inline Vec3 GetMouseWorldPos() {
            return CoreView::GetMouseWorldPos();
        }

        inline Vec2 GetMouseScreenPos() {
            return CoreView::GetMouseScreenPos();
        }

        inline uint32_t GetSelectedNetId() {
            return CoreView::GetSelectedNetId();
        }
    }

    namespace Control {
        inline uintptr_t GetIssueOrderFn() {
            return CoreRuntime::GetContext().issueOrderFn;
        }

        inline uintptr_t GetCastSpellFn() {
            return CoreRuntime::GetContext().castSpellFn;
        }

        inline uintptr_t GetGetPingFn() {
            return CoreRuntime::GetContext().getPingFn;
        }

        inline uintptr_t GetGetAttackDelayFn() {
            return CoreRuntime::GetContext().getAttackDelayFn;
        }

        inline uintptr_t GetGetAttackWindupFn() {
            return CoreRuntime::GetContext().getAttackWindupFn;
        }

        inline int GetPing() {
            return CoreControl::GetPing();
        }

        inline float GetAttackDelay() {
            return CoreControl::GetAttackDelay();
        }

        inline float GetAttackWindup() {
            return CoreControl::GetAttackWindup();
        }

        inline float GetAttackDelay(uintptr_t object) {
            return CoreControl::GetAttackDelay(object);
        }

        inline float GetAttackWindup(uintptr_t object) {
            return CoreControl::GetAttackWindup(object);
        }

        inline bool CanIssueOrder() {
            return CoreControl::CanIssueOrder();
        }

        inline bool CanCastSpell() {
            return CoreControl::CanCastSpell();
        }

        inline bool CanUpdateChargedSpell() {
            return CoreControl::CanUpdateChargedSpell();
        }

        inline bool IssueMove(const Vec3& pos) {
            return CoreControl::MoveToPos(pos);
        }

        inline bool IssueAttackMove(const Vec3& pos) {
            return CoreControl::AttackMoveTo(pos);
        }

        inline bool IssueAttack(uintptr_t target, const Vec3& pos) {
            return CoreControl::AttackObject(target, pos);
        }

        inline bool CastSpell(int slotId, const Vec3& start, const Vec3& end, uint32_t targetNetId = 0) {
            return CoreControl::CastSpellPacket(slotId, start, end, targetNetId);
        }

        inline bool UpdateChargedSpell(int slotId, const Vec3& pos, bool releaseCast) {
            return CoreControl::UpdateChargedSpell(slotId, pos, releaseCast);
        }
    }

    namespace SpellBook {
        using SpellState = CoreSpellBook::SpellState;

        inline uintptr_t Get(uintptr_t obj) {
            return CoreSpellBook::GetSpellBook(obj);
        }

        inline uintptr_t GetActiveSpellCast(uintptr_t obj) {
            return CoreSpellBook::GetActiveSpellCast(obj);
        }

        inline CoreSpellBook::SlotRef GetSlot(uintptr_t obj, int slotId) {
            return CoreSpellBook::GetSlot(obj, slotId);
        }

        inline bool HasEnoughMana(uintptr_t obj, int slotId) {
            return CoreSpellBook::HasEnoughMana(obj, slotId);
        }

        inline bool CanCast(uintptr_t obj, int slotId, float gameTime) {
            return CoreSpellBook::CanCast(obj, slotId, gameTime);
        }

        inline SpellState GetSpellState(uintptr_t obj, int slotId, float gameTime) {
            return CoreSpellBook::GetSpellState(obj, slotId, gameTime);
        }
    }

    namespace SpellCast {
        inline CoreSpellCastInfo::CastRef GetActive(uintptr_t obj) {
            return CoreSpellCastInfo::GetActive(obj);
        }

        inline CoreSpellCastInfo::CastRef GetFromMissile(uintptr_t missile) {
            return CoreSpellCastInfo::GetMissileCast(missile);
        }
    }

    namespace Buffs {
        inline int Enumerate(uintptr_t obj, uintptr_t* out, int maxOut) {
            return CoreBuffs::Enumerate(obj, out, maxOut);
        }

        inline int Count(uintptr_t obj) {
            return CoreBuffs::Count(obj);
        }

        inline CoreBuffs::BuffRef FindByName(uintptr_t obj, const char* name) {
            return CoreBuffs::FindByName(obj, name);
        }

        inline bool HasBuff(uintptr_t obj, const char* name) {
            return CoreBuffs::HasBuff(obj, name);
        }

        inline int GetBuffStacks(uintptr_t obj, const char* name) {
            return CoreBuffs::GetBuffStacks(obj, name);
        }

        inline float GetBuffRemainingTime(uintptr_t obj, const char* name, float gameTime) {
            return CoreBuffs::GetBuffRemainingTime(obj, name, gameTime);
        }

        inline bool HasBuffType(uintptr_t obj, int type) {
            return CoreBuffs::HasBuffType(obj, type);
        }
    }

    namespace Ai {
        inline CoreAi::ManagerRef Get(uintptr_t obj) {
            return CoreAi::Get(obj);
        }

        inline bool IsMoving(uintptr_t obj) {
            return CoreAi::IsMoving(obj);
        }

        inline bool IsDashing(uintptr_t obj) {
            return CoreAi::IsDashing(obj);
        }

        inline bool HasPath(uintptr_t obj) {
            return CoreAi::HasPath(obj);
        }

        inline int GetCurrentSegment(uintptr_t obj) {
            return CoreAi::GetCurrentSegment(obj);
        }

        inline float GetDashSpeed(uintptr_t obj) {
            return CoreAi::GetDashSpeed(obj);
        }

        inline Vec3 GetVelocity(uintptr_t obj) {
            return CoreAi::GetVelocity(obj);
        }

        inline Vec3 GetPathStart(uintptr_t obj) {
            return CoreAi::GetPathStart(obj);
        }

        inline Vec3 GetPathEnd(uintptr_t obj) {
            return CoreAi::GetPathEnd(obj);
        }

        inline Vec3 GetServerPosition(uintptr_t obj) {
            return CoreAi::GetServerPosition(obj);
        }

        inline Vec3 GetOrderPosition(uintptr_t obj) {
            return CoreAi::GetOrderPosition(obj);
        }

        inline int CopyWaypoints(uintptr_t obj, Vec3* out, int maxOut) {
            return CoreAi::CopyWaypoints(obj, out, maxOut);
        }
    }

    namespace NavGrid {
        inline CoreNavGrid::GridRef Get() {
            return CoreNavGrid::Get();
        }

        inline bool IsWall(const Vec3& pos) {
            return CoreNavGrid::Get().IsWall(pos);
        }

        inline bool IsWalkable(const Vec3& pos) {
            return CoreNavGrid::Get().IsWalkable(pos);
        }

        inline bool IsBrush(const Vec3& pos) {
            return CoreNavGrid::Get().IsBrush(pos);
        }

        inline bool IsWallBetween(const Vec3& from, const Vec3& to, float step = 40.0f) {
            return CoreNavGrid::Get().IsWallBetween(from, to, step);
        }
    }

} // namespace CoreAPI
