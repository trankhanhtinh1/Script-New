#pragma once

// ============================================================================
// ZDEvade.h — ZDEvade: skillshot evade plugin for NightSharp
//
// Uses SDK::Tracker for skillshot detection, ZDEvade databases for danger
// levels / CC types / evade spells, and implements the evade decision engine
// (position search, movement blocking, danger check, evade spell usage).
//
// Architecture: Detection -> SDK::Tracker. Decision -> this plugin.
//               Action -> CoreControl::IssueMove / Spellbook::CastSpell.
// ============================================================================

#include "../IPlugin.h"
#include "../../Core/CoreRuntime.h"
#include "../../Core/Globals.h"
#include "../../Core/CoreControl.h"
#include "../../Core/CoreNavGrid.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../SDK/UI/IMenu/Menu.h"
#include "../../SDK/Wrappers/Orbwalking/Orbwalker.h"

#include "PositionInfo.h"
#include "Situation.h"
#include "EvadeCommand.h"
#include "EvadeHelper.h"
#include "EvadeSpellManager.h"
#include "SpellDetector.h"
#include "SpellData.h"
#include "SpellDatabase.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace Plugins {

// ── ZDEvade file logger ──
inline constexpr const char* kZDEvadeLogPath = "C:\\Users\\Public\\ZDEvade.txt";

inline void ZDLog(const char* fmt, ...) {
    char buffer[1024] = {};
    va_list args;
    va_start(args, fmt);
    _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
    va_end(args);
    for (char* p = buffer; *p; ++p) {
        const unsigned char ch = static_cast<unsigned char>(*p);
        if ((ch < 32 || ch > 126) && ch != '\t') *p = '?';
    }

    HANDLE hFile = CreateFileA(
        kZDEvadeLogPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(hFile, buffer, static_cast<DWORD>(lstrlenA(buffer)), &written, nullptr);
    WriteFile(hFile, "\r\n", 2, &written, nullptr);
    CloseHandle(hFile);
}

class ZDEvadePlugin final : public IPlugin {
public:
    const char* GetName() const override { return "ZDEvade"; }
    const char* GetInternalId() const override { return "core.zdevade"; }
    const char* GetAuthor() const override { return "ZD"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }

    void OnLoad() override {
        s_instance = this;

        // Initialize databases and custom detection
        ZDEvade::SpellDetector::Initialize();
        ZDEvade::EvadeSpellManager::Initialize();

        CreateMenu();

        ZDEvade::SpellDetector::SetChangeHandler(&ZDEvadePlugin::OnDetectorChangedStatic);
        SDK::Game::OnUpdate += &ZDEvadePlugin::OnUpdateStatic;
        SDK::Events::AddOnNewPath(&ZDEvadePlugin::OnNewPathStatic);

        ZDLog("[ZDEvade] loaded");
    }

    void OnUnload() override {
        ZDEvade::SpellDetector::SetChangeHandler(nullptr);
        SDK::Events::RemoveOnNewPath(&ZDEvadePlugin::OnNewPathStatic);
        SDK::Game::OnUpdate -= &ZDEvadePlugin::OnUpdateStatic;
        ZDEvade::SpellDetector::Shutdown();

        DestroyMenu();

        if (s_instance == this) {
            s_instance = nullptr;
        }
        ZDLog("[ZDEvade] unloaded");
    }

    void OnRender() override {
        if (!Enabled() || !ImGui::GetCurrentContext()) return;

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return;
        const float planeY = player.Position().y;

        auto& activeSpells = ZDEvade::SpellDetector::ActiveSpells();
        const int now = SDK::Variables::TickCount();
        if (!activeSpells.empty() && now - m_lastRenderActiveLogTick > 250) {
            m_lastRenderActiveLogTick = now;
            ZDLog("[ZDEvade][Render] active tick=%d count=%d serial=%d lastId=%d lastChange=%d drawEnabled=%d enabled=%d",
                  now, static_cast<int>(activeSpells.size()), ZDEvade::SpellDetector::ChangeSerial(),
                  ZDEvade::SpellDetector::LastAddedSpellId(), ZDEvade::SpellDetector::LastChangeTick(),
                  DrawSpells() ? 1 : 0, Enabled() ? 1 : 0);
        }
        for (const auto& spell : activeSpells) {
            if (m_firstDrawLoggedIds.insert(spell.spellId).second) {
                ZDLog("[ZDEvade][Render] first_draw tick=%d id=%d spell=%s createdStart=%d detectorChange=%d deltaStart=%d deltaChange=%d missile=%d type=%d start=(%.1f,%.1f) end=(%.1f,%.1f)",
                      now, spell.spellId, spell.info.spellName.c_str(), spell.startTime, ZDEvade::SpellDetector::LastChangeTick(),
                      now - spell.startTime, now - ZDEvade::SpellDetector::LastChangeTick(), spell.isMissile ? 1 : 0,
                      static_cast<int>(spell.Type()), spell.startPos.x, spell.startPos.y, spell.endPos.x, spell.endPos.y);
            }
            const int danger = std::max(1, spell.DangerValue());
            std::uint32_t color = 0xFFFF8800;
            if (danger >= 4) color = 0xFFFF0000;
            else if (danger >= 3) color = 0xFFFF4400;
            else if (danger >= 2) color = 0xFFFFFF00;

            if (spell.Type() == ZDEvade::ZDSpellType::Line) {
                // Draw outline only — 2 thin edge lines + start/end caps
                Vec2 start = spell.startPos;
                if (spell.isMissile && spell.missile.IsValid())
                    start = spell.GetMissilePosition(0);
                const Vec2 dir = spell.direction;
                const Vec2 perp(-dir.y, dir.x);
                const float halfWidth = spell.Radius();
                const Vec2 sL = start + perp * halfWidth;
                const Vec2 sR = start - perp * halfWidth;
                const Vec2 eL = spell.endPos + perp * halfWidth;
                const Vec2 eR = spell.endPos - perp * halfWidth;
                const float py = planeY;
                // Left edge
                SDK::Drawing::DrawLine(Vec3::From2D(sL, py), Vec3::From2D(eL, py), color, 2.0f);
                // Right edge
                SDK::Drawing::DrawLine(Vec3::From2D(sR, py), Vec3::From2D(eR, py), color, 2.0f);
                // Start cap
                SDK::Drawing::DrawLine(Vec3::From2D(sL, py), Vec3::From2D(sR, py), color, 2.0f);
                // End cap
                SDK::Drawing::DrawLine(Vec3::From2D(eL, py), Vec3::From2D(eR, py), color, 2.0f);
                // Spell name text at start
                SDK::Drawing::DrawText(Vec3::From2D(start, py), spell.SpellName().c_str(), color, true);
            } else if (spell.Type() == ZDEvade::ZDSpellType::Circular) {
                SDK::Drawing::DrawCircle(Vec3::From2D(spell.endPos, planeY), spell.Radius(), color, 2.0f);
                // Spell name text at center
                SDK::Drawing::DrawText(Vec3::From2D(spell.endPos, planeY), spell.SpellName().c_str(), color, true);
            }
        }

        // Draw dodge status
        if (m_isDodging && DrawSpells()) {
            SDK::Drawing::DrawCircle(player.Position(), 100.0f, 0xFFFF0000);
            if (m_hasLockedDodgePos) {
                constexpr std::uint32_t dodgeColor = 0xFF00FFFF;
                const Vec2 hero2D = player.ServerPosition().To2D();
                SDK::Drawing::DrawLine(Vec3::From2D(hero2D, planeY), Vec3::From2D(m_lockedDodgePos, planeY), dodgeColor, 3.0f);
                SDK::Drawing::DrawCircle(Vec3::From2D(m_lockedDodgePos, planeY), 45.0f, dodgeColor, 2.0f);
                SDK::Drawing::DrawText(Vec3::From2D(m_lockedDodgePos, planeY), "DODGE", dodgeColor, true);
            }
        }
    }

    void OnMenu() override {
        if (!m_menu) return;
        m_menu->DrawImGui();
        ImGui::Separator();
        ImGui::Text("Tracked spells: %d",
                    static_cast<int>(ZDEvade::SpellDetector::ActiveSpells().size()));
        ImGui::Text("Dodging: %s", m_isDodging ? "YES" : "no");
        ImGui::Text("Evade spells: %d", static_cast<int>(ZDEvade::EvadeSpellManager::evadeSpells.size()));
    }

private:
    static inline ZDEvadePlugin* s_instance = nullptr;

    Menu* m_menu = nullptr;
    MenuBool* m_enabledMenu = nullptr;
    MenuKeyBind* m_dodgeKeyMenu = nullptr;
    MenuBool* m_drawSpellsMenu = nullptr;
    MenuBool* m_useEvadeSpellsMenu = nullptr;
    MenuBool* m_dodgeDangerousOnlyMenu = nullptr;
    MenuBool* m_dodgeCircularMenu = nullptr;
    MenuBool* m_dodgeFOWMenu = nullptr;
    MenuSlider* m_extraDelayMenu = nullptr;
    MenuSlider* m_extraDistMenu = nullptr;
    MenuSlider* m_reactionTimeMenu = nullptr;
    MenuSlider* m_minHitTimeMenu = nullptr;
    MenuSlider* m_dodgeIntervalMenu = nullptr;
    MenuSlider* m_dodgeHpMenu = nullptr;
    MenuSlider* m_evadeDistanceMenu = nullptr;

    bool m_isDodging = false;
    int m_lastDodgeTick = 0;
    bool m_isChanneling = false;
    int m_lastStopEvadeTime = 0;
    char m_lastEvent[96] = "none";
    int m_lastStatusLogTick = 0;
    bool m_hasLockedDodgePos = false;
    Vec2 m_lockedDodgePos = {};
    std::vector<int> m_lockedThreatIds;
    std::unordered_set<int> m_firstDrawLoggedIds;
    int m_lastRenderActiveLogTick = 0;
    int m_lockedDodgeStartTick = 0;
    int m_lastMovementBlockTime = 0;
    Vec2 m_lastMovementBlockPos = {};
    bool m_processingEvadeFrame = false;
    bool m_pendingMoveRetry = false;
    int m_lastLockedMoveIssueTick = 0;

    // ── settings accessors ──
    bool Enabled() const { return !m_enabledMenu || m_enabledMenu->Value; }
    bool DrawSpells() const { return !m_drawSpellsMenu || m_drawSpellsMenu->Value; }
    bool DodgeKeyActive() const { return !m_dodgeKeyMenu || m_dodgeKeyMenu->Active; }
    bool UseEvadeSpells() const { return !m_useEvadeSpellsMenu || m_useEvadeSpellsMenu->Value; }
    bool DodgeDangerousOnly() const { return m_dodgeDangerousOnlyMenu && m_dodgeDangerousOnlyMenu->Value; }
    bool DodgeCircular() const { return !m_dodgeCircularMenu || m_dodgeCircularMenu->Value; }
    bool DodgeFOW() const { return !m_dodgeFOWMenu || m_dodgeFOWMenu->Value; }
    float ExtraDelay() const { return m_extraDelayMenu ? static_cast<float>(m_extraDelayMenu->Value) : 0.0f; }
    float ExtraDist() const { return m_extraDistMenu ? static_cast<float>(m_extraDistMenu->Value) : 15.0f; }
    int ReactionTime() const { return m_reactionTimeMenu ? m_reactionTimeMenu->Value : 0; }
    int MinHitTime() const { return m_minHitTimeMenu ? m_minHitTimeMenu->Value : 2000; }
    int DodgeInterval() const { return m_dodgeIntervalMenu ? m_dodgeIntervalMenu->Value : 0; }
    float DodgeHp() const { return m_dodgeHpMenu ? static_cast<float>(m_dodgeHpMenu->Value) : 100.0f; }
    float EvadeDistance() const { return m_evadeDistanceMenu ? static_cast<float>(m_evadeDistanceMenu->Value) : 100.0f; }

    void SetLastEvent(const char* text) {
        strncpy_s(m_lastEvent, text ? text : "", _TRUNCATE);
    }

    void ClearLockedDodge() {
        m_hasLockedDodgePos = false;
        m_lockedThreatIds.clear();
        m_lockedDodgeStartTick = 0;
        m_pendingMoveRetry = false;
        m_lastLockedMoveIssueTick = 0;
    }

    static std::vector<int> BuildThreatIds(const std::vector<ZDEvade::TrackedSpell>& threats) {
        std::vector<int> ids;
        ids.reserve(threats.size());
        for (const auto& s : threats) ids.push_back(s.spellId);
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    bool SameLockedThreatIds(const std::vector<int>& ids) const {
        return m_hasLockedDodgePos && m_lockedThreatIds == ids;
    }

    // ── static event handlers ──
    static void OnUpdateStatic() {
        if (s_instance) s_instance->Tick();
    }

    static void OnNewPathStatic(const SDK::Events::NewPathEventArgs& args) {
        if (s_instance) s_instance->OnNewPath(args);
    }

    static void OnDetectorChangedStatic() {
        if (s_instance) s_instance->OnDetectorChanged();
    }

    bool CollectActiveThreats(std::vector<ZDEvade::TrackedSpell>& out) const {
        out.clear();
        for (const auto& s : ZDEvade::SpellDetector::ActiveSpells()) {
            if (DodgeDangerousOnly() && s.DangerValue() < 3) continue;
            if (!DodgeCircular() && ZDEvade::IsCircleType(s.Type())) continue;
            out.push_back(s);
        }
        return !out.empty();
    }

    bool CanRunEvade(const SDK::AIHeroClient& player) {
        if (!Enabled() || !DodgeKeyActive()) return false;
        if (!player.IsValid() || player.IsDead()) return false;
        if (!ZDEvade::Situation::ShouldDodge(Enabled(), m_isChanneling, DodgeDangerousOnly())) return false;
        if (player.HealthPercent() > DodgeHp()) return false;
        return true;
    }

    void OnDetectorChanged() {
        ZDLog("[ZDEvade][Plugin] detector_changed tick=%d serial=%d lastId=%d lastChange=%d processing=%d active=%d",
              SDK::Variables::TickCount(), ZDEvade::SpellDetector::ChangeSerial(), ZDEvade::SpellDetector::LastAddedSpellId(),
              ZDEvade::SpellDetector::LastChangeTick(), m_processingEvadeFrame ? 1 : 0,
              static_cast<int>(ZDEvade::SpellDetector::ActiveSpells().size()));
        if (m_processingEvadeFrame) return;
        // EzEvade SpellDetector_OnProcessDetectedSpells: compute best position once on new spell
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return;
        if (!CanRunEvade(player)) { ProcessEvadeFrame("detector"); return; }
        std::vector<ZDEvade::TrackedSpell> activeThreats;
        if (!CollectActiveThreats(activeThreats)) { ProcessEvadeFrame("detector"); return; }
        const Vec2 heroPos = player.ServerPosition().To2D();
        const float boundingRadius = player.BoundingRadius();
        // If hero pos is dangerous, compute and cache best position
        if (ZDEvade::EvadeHelper::CheckDangerousPos(heroPos, 0.0f, boundingRadius) > 0) {
            Vec2 best;
            ZDEvade::EvadeHelper::GetBestPosition(
                player, heroPos, boundingRadius, activeThreats,
                best, ExtraDelay(), ExtraDist(), EvadeDistance());
            m_lockedDodgePos = best;
            m_hasLockedDodgePos = true;
            m_lockedThreatIds = BuildThreatIds(activeThreats);
            ZDLog("[ZDEvade][DodgeSelect] tick=%d reason=detector pos=(%.1f,%.1f)",
                SDK::Variables::TickCount(), m_lockedDodgePos.x, m_lockedDodgePos.y);
        }
        ProcessEvadeFrame("detector");
    }

    // EzEvade port: CheckHeroInDanger — sets isDodging based on whether hero is in skillshot
    void CheckHeroInDanger(const SDK::AIHeroClient& player, const Vec2& heroPos,
                           float boundingRadius,
                           const std::vector<ZDEvade::TrackedSpell>& activeThreats) {
        (void)player;
        bool playerInDanger = false;
        const float evadeDist = EvadeDistance();
        for (const auto& spell : activeThreats) {
            if (ZDEvade::EvadeHelper::InSkillShot(spell, heroPos, boundingRadius)) {
                playerInDanger = true;
                break;
            }
            // EzEvade: keep dodging if within evade distance of skillshot edge
            if (evadeDist > 0.0f && !spell.HasExpired() &&
                ZDEvade::EvadeHelper::InSkillShot(spell, heroPos, boundingRadius + evadeDist)) {
                playerInDanger = true;
                break;
            }
        }
        m_isDodging = playerInDanger;
    }

    // EzEvade port: ContinueLastBlockedCommand — resume user's blocked movement
    void ContinueLastBlockedCommand(const SDK::AIHeroClient& player, const Vec2& heroPos, float delayMs) {
        if (!ZDEvade::EvadeCommandManager::HasPendingBlockedMove()) return;
        const int now = SDK::Variables::TickCount();
        const int extraDelay = static_cast<int>(ExtraDelay());
        if (now - ZDEvade::EvadeCommandManager::lastEvadeCommand.timestamp <
            static_cast<int>(delayMs) + extraDelay) return;
        if (now - ZDEvade::EvadeCommandManager::lastBlockedUserMoveTo.timestamp > 1500) {
            ZDEvade::EvadeCommandManager::MarkBlockedMoveProcessed();
            return;
        }
        Vec2 movePos = ZDEvade::EvadeCommandManager::lastBlockedUserMoveTo.targetPosition;
        std::vector<ZDEvade::TrackedSpell> activeThreats;
        if (!CollectActiveThreats(activeThreats)) {
            ZDEvade::EvadeCommandManager::MoveTo(movePos);
            ZDEvade::EvadeCommandManager::MarkBlockedMoveProcessed();
            return;
        }
        const float boundingRadius = player.BoundingRadius();
        const float moveSpeed = std::max(50.0f, player.MoveSpeed());
        if (!ZDEvade::EvadeHelper::CheckMovePath(movePos, delayMs, heroPos, boundingRadius, activeThreats, moveSpeed)) {
            ZDEvade::EvadeCommandManager::MoveTo(movePos);
            ZDEvade::EvadeCommandManager::MarkBlockedMoveProcessed();
        }
    }

    void OnNewPath(const SDK::Events::NewPathEventArgs& args) {
        const auto player = SDK::ObjectManager::Player();
        if (!CanRunEvade(player)) return;
        if (!args.Sender.IsValid() || args.Sender.NetworkId != static_cast<uint32_t>(player.NetworkId())) return;
        if (args.PathCount <= 0) return;
        std::vector<ZDEvade::TrackedSpell> activeThreats;
        if (!CollectActiveThreats(activeThreats)) return;
        const Vec2 heroPos = player.ServerPosition().To2D();
        const float boundingRadius = player.BoundingRadius();
        const float moveSpeed = std::max(50.0f, player.MoveSpeed());
        const float delayMs = ExtraDelay() + static_cast<float>(SDK::Game::Ping());
        const Vec2 movePos = args.Path[args.PathCount - 1].To2D();
        const int now = SDK::Variables::TickCount();
        if (now - ZDEvade::EvadeCommandManager::lastEvadeCommand.timestamp < 150 &&
            movePos.DistanceSqr(ZDEvade::EvadeCommandManager::lastEvadeCommand.targetPosition) < 100.0f) return;
        if (!ZDEvade::EvadeHelper::CheckMovePath(movePos, delayMs, heroPos, boundingRadius, activeThreats, moveSpeed)) return;
        if (now - m_lastMovementBlockTime < 500 && m_lastMovementBlockPos.DistanceSqr(movePos) < 10000.0f) return;
        Vec2 best;
        ZDEvade::EvadeHelper::GetBestPositionMovementBlock(
            movePos, player, heroPos, boundingRadius, activeThreats,
            best, ExtraDelay(), ExtraDist(), EvadeDistance());
        ZDEvade::EvadeCommandManager::BlockUserMoveTo(movePos);
        m_lastMovementBlockTime = now;
        m_lastMovementBlockPos = movePos;
        m_lockedDodgePos = best;
        m_lockedThreatIds = BuildThreatIds(activeThreats);
        m_lockedDodgeStartTick = now;
        m_hasLockedDodgePos = true;
        m_isDodging = true;
        m_pendingMoveRetry = !ZDEvade::EvadeCommandManager::MoveTo(m_lockedDodgePos, true);
        m_lastLockedMoveIssueTick = now;
        SetLastEvent("newpath block");
    }

    bool ProcessEvadeFrame(const char* reason) {
        if (m_processingEvadeFrame) return false;
        m_processingEvadeFrame = true;
        struct FrameGuard {
            bool& flag;
            explicit FrameGuard(bool& value) : flag(value) {}
            ~FrameGuard() { flag = false; }
        } guard(m_processingEvadeFrame);

        if (!Enabled() || !DodgeKeyActive()) {
            m_isDodging = false;
            ClearLockedDodge();
            ZDEvade::EvadeCommandManager::RestoreOrbwalkerMove();
            return false;
        }

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) {
            m_isDodging = false;
            ClearLockedDodge();
            ZDEvade::EvadeCommandManager::RestoreOrbwalkerMove();
            return false;
        }

        if (m_isChanneling && !player.Spellbook().IsChanneling()) {
            m_isChanneling = false;
        }

        if (!ZDEvade::Situation::ShouldDodge(Enabled(), m_isChanneling, DodgeDangerousOnly())) {
            m_isDodging = false;
            ClearLockedDodge();
            ZDEvade::EvadeCommandManager::RestoreOrbwalkerMove();
            return false;
        }

        if (player.HealthPercent() > DodgeHp()) {
            m_isDodging = false;
            ClearLockedDodge();
            ZDEvade::EvadeCommandManager::RestoreOrbwalkerMove();
            return false;
        }

        auto& spells = ZDEvade::SpellDetector::ActiveSpells();
        if (spells.empty()) {
            m_isDodging = false;
            ClearLockedDodge();
            ZDEvade::EvadeCommandManager::RestoreOrbwalkerMove();
            return false;
        }

        std::vector<ZDEvade::TrackedSpell> activeThreats;
        CollectActiveThreats(activeThreats);

        if (activeThreats.empty()) {
            m_isDodging = false;
            ClearLockedDodge();
            ZDEvade::EvadeCommandManager::RestoreOrbwalkerMove();
            return false;
        }

        const Vec2 heroPos = player.ServerPosition().To2D();
        const float boundingRadius = player.BoundingRadius();
        const float moveSpeed = std::max(50.0f, player.MoveSpeed());
        const float delayMs = ExtraDelay() + static_cast<float>(SDK::Game::Ping());
        const int now = SDK::Variables::TickCount();

        // EzEvade: CheckHeroInDanger sets isDodging
        CheckHeroInDanger(player, heroPos, boundingRadius, activeThreats);

        if (m_isDodging) {
            ZDEvade::EvadeCommandManager::DisableOrbwalkerMove();

            // Try evade spell first
            if (ZDEvade::EvadeSpellManager::PreferEvadeSpell()) {
                if (ZDEvade::EvadeSpellManager::UseEvadeSpell(
                        heroPos, boundingRadius, activeThreats, UseEvadeSpells())) {
                    m_lastDodgeTick = now;
                    SetLastEvent("evade spell");
                    return true;
                }
            }

            // EzEvade DodgeSkillShots: move to cached position (no recompute!)
            if (m_hasLockedDodgePos) {
                // Just move to cached position — fast!
                const bool retryMove = m_pendingMoveRetry &&
                    now - m_lastLockedMoveIssueTick >= ZDEvade::EvadeCommandManager::kMinMoveInterval;
                const bool refreshMove = !m_pendingMoveRetry && now - m_lastLockedMoveIssueTick >= 350;
                if (retryMove || refreshMove || now - m_lastLockedMoveIssueTick >= 350) {
                    m_pendingMoveRetry = !ZDEvade::EvadeCommandManager::MoveTo(m_lockedDodgePos, m_pendingMoveRetry);
                    m_lastLockedMoveIssueTick = now;
                }
            } else {
                // No cached position — compute one
                Vec2 best;
                ZDEvade::EvadeHelper::GetBestPosition(
                    player, heroPos, boundingRadius, activeThreats,
                    best, ExtraDelay(), ExtraDist(), EvadeDistance());
                m_lockedDodgePos = best;
                m_lockedThreatIds = BuildThreatIds(activeThreats);
                m_hasLockedDodgePos = true;
                m_pendingMoveRetry = !ZDEvade::EvadeCommandManager::MoveTo(m_lockedDodgePos, true);
                m_lastLockedMoveIssueTick = now;
            }
            m_lastDodgeTick = now;
            SetLastEvent(reason);
        } else {
            // Not dodging: check if current path walks into skillshot
            const auto path = player.Path();
            if (!path.empty()) {
                const Vec2 movePos = path.back().To2D();
                if (ZDEvade::EvadeHelper::CheckMovePath(movePos, delayMs, heroPos, boundingRadius, activeThreats, moveSpeed)) {
                    if (now - m_lastMovementBlockTime < 500 && m_lastMovementBlockPos.DistanceSqr(movePos) < 10000.0f) {
                        // Already blocking this move
                    } else {
                        Vec2 best;
                        ZDEvade::EvadeHelper::GetBestPositionMovementBlock(
                            movePos, player, heroPos, boundingRadius, activeThreats,
                            best, ExtraDelay(), ExtraDist(), EvadeDistance());
                        ZDEvade::EvadeCommandManager::BlockUserMoveTo(movePos);
                        m_lastMovementBlockTime = now;
                        m_lastMovementBlockPos = movePos;
                        m_lockedDodgePos = best;
                        m_hasLockedDodgePos = true;
                        m_isDodging = true;
                        m_pendingMoveRetry = !ZDEvade::EvadeCommandManager::MoveTo(m_lockedDodgePos, true);
                        m_lastLockedMoveIssueTick = now;
                        m_lastDodgeTick = now;
                        SetLastEvent("movement block");
                    }
                } else {
                    ClearLockedDodge();
                    ZDEvade::EvadeCommandManager::RestoreOrbwalkerMove();
                }
            } else {
                ClearLockedDodge();
                ZDEvade::EvadeCommandManager::RestoreOrbwalkerMove();
            }

            // EzEvade: ContinueLastBlockedCommand — resume user's blocked movement
            ContinueLastBlockedCommand(player, heroPos, delayMs);
        }

        // EzEvade: RecalculatePath — recheck if current path is still safe
        RecalculatePath(player, heroPos, boundingRadius, delayMs, activeThreats);

        // Periodic debug status log (every 10s only)
        if (m_lastStatusLogTick == 0 || now - m_lastStatusLogTick > 10000) {
            m_lastStatusLogTick = now;
            ZDLog("[ZDEvade] status: tracked=%d dodging=%d event=%s",
                static_cast<int>(activeThreats.size()), m_isDodging ? 1 : 0, m_lastEvent);
        }
        return m_isDodging;
    }

    // EzEvade port: RecalculatePath — recheck if current path is still safe
    void RecalculatePath(const SDK::AIHeroClient& player, const Vec2& heroPos,
                         float boundingRadius, float delayMs,
                         std::vector<ZDEvade::TrackedSpell>& activeThreats) {
        if (!m_isDodging || !m_hasLockedDodgePos) return;
        const auto path = player.Path();
        if (path.empty()) return;
        const Vec2 movePos = path.back().To2D();
        // If hero is walking toward locked pos, check if danger increased
        if (movePos.DistanceSqr(m_lockedDodgePos) > 25.0f) return;
        const float moveSpeed = std::max(50.0f, player.MoveSpeed());
        // Check current path danger level
        int currentDanger = 0;
        for (const auto& s : activeThreats) {
            if (ZDEvade::EvadeHelper::InSkillShot(s, movePos, boundingRadius))
                currentDanger += std::max(1, s.DangerValue());
        }
        // If path is now more dangerous than when we computed, recompute
        if (currentDanger > 0) {
            Vec2 best;
            ZDEvade::EvadeHelper::GetBestPosition(
                player, heroPos, boundingRadius, activeThreats,
                best, ExtraDelay(), ExtraDist(), EvadeDistance());
            m_lockedDodgePos = best;
            m_lockedThreatIds = BuildThreatIds(activeThreats);
            m_pendingMoveRetry = !ZDEvade::EvadeCommandManager::MoveTo(m_lockedDodgePos, true);
            m_lastLockedMoveIssueTick = SDK::Variables::TickCount();
        }
    }

    // ── main tick ──
    void Tick() {
        ProcessEvadeFrame("dodging");
    }

    // ── menu ──
    void CreateMenu() {
        DestroyMenu();
        m_menu = new Menu(GetInternalId(), GetName(), true);

        auto* main = m_menu->AddSubMenu(new Menu("main", "Main"));
        m_enabledMenu = main->Add(new MenuBool("enabled", "Enable ZDEvade", true));
        m_dodgeKeyMenu = main->Add(new MenuKeyBind(
            "dodgeKey", "Dodge Skillshots", VK_SPACE, KeyBindType::Toggle, true));
        m_useEvadeSpellsMenu = main->Add(new MenuBool("useEvadeSpells", "Use Evade Spells", true));
        m_dodgeDangerousOnlyMenu = main->Add(new MenuBool("dodgeDangerousOnly", "Dodge Only Dangerous", false));
        m_dodgeCircularMenu = main->Add(new MenuBool("dodgeCircular", "Dodge Circular Spells", true));
        m_dodgeFOWMenu = main->Add(new MenuBool("dodgeFOW", "Dodge FOW Spells", true));
        m_drawSpellsMenu = main->Add(new MenuBool("drawSpells", "Draw Skillshots", true));

        auto* buffers = m_menu->AddSubMenu(new Menu("buffers", "Buffers"));
        m_extraDelayMenu = buffers->Add(new MenuSlider(
            "extraDelay", "Extra Ping Buffer (ms)", 0, 0, 200));
        m_extraDistMenu = buffers->Add(new MenuSlider(
            "extraDist", "Extra CPA Distance", 15, 0, 150));
        m_evadeDistanceMenu = buffers->Add(new MenuSlider(
            "evadeDistance", "Extra Evade Distance", 100, 0, 300));

        auto* humanizer = m_menu->AddSubMenu(new Menu("humanizer", "Humanizer"));
        m_reactionTimeMenu = humanizer->Add(new MenuSlider(
            "reactionTime", "Reaction Time (ms)", 0, 0, 500));
        m_minHitTimeMenu = humanizer->Add(new MenuSlider(
            "minHitTime", "Max Hit Time to Dodge (ms)", 2000, 100, 3000));
        m_dodgeIntervalMenu = humanizer->Add(new MenuSlider(
            "dodgeInterval", "Dodge Re-issue Interval (ms)", 0, 0, 500));
        m_dodgeHpMenu = humanizer->Add(new MenuSlider(
            "dodgeHp", "Only Dodge Below HP %", 100, 1, 100));

        m_menu->Attach();
    }

    void DestroyMenu() {
        if (!m_menu) return;
        MenuManager::Instance().Remove(m_menu);
        delete m_menu;
        m_menu = nullptr;
        m_enabledMenu = nullptr;
        m_dodgeKeyMenu = nullptr;
        m_drawSpellsMenu = nullptr;
        m_useEvadeSpellsMenu = nullptr;
        m_dodgeDangerousOnlyMenu = nullptr;
        m_dodgeCircularMenu = nullptr;
        m_dodgeFOWMenu = nullptr;
        m_extraDelayMenu = nullptr;
        m_extraDistMenu = nullptr;
        m_reactionTimeMenu = nullptr;
        m_minHitTimeMenu = nullptr;
        m_dodgeIntervalMenu = nullptr;
        m_dodgeHpMenu = nullptr;
        m_evadeDistanceMenu = nullptr;
        m_firstDrawLoggedIds.clear();
        m_lastRenderActiveLogTick = 0;
    }
};

} // namespace Plugins
