#pragma once

// ============================================================================
// SpellDetector.h — Custom skillshot detection for ZDEvade
//
// Hooks OnProcessSpell + OnMissileCreate + OnMissileDelete directly,
// looks up spells in ZDEvade::SpellDatabase (not SDK's database),
// creates TrackedSpell objects for evade logic and drawing.
// ============================================================================

#include "SpellData.h"
#include "SpellDatabase.h"
#include "../../SDK/SDK.h"
#include "../../Core/CoreControl.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace ZDEvade {

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
        "C:\\Users\\Public\\ZDEvade.txt", FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(hFile, buffer, static_cast<DWORD>(lstrlenA(buffer)), &written, nullptr);
    WriteFile(hFile, "\r\n", 2, &written, nullptr);
    CloseHandle(hFile);
}

struct TrackedSpell {
    SpellData info;
    Vec2 startPos = {};
    Vec2 endPos = {};
    Vec2 direction = {};
    int startTime = 0;
    int missileStartTime = 0;
    int endTime = 0;
    bool isMissile = false;
    SDK::MissileClient missile;
    int spellId = 0;
    bool expired = false;
    uint32_t casterNetworkId = 0;
    int slot = -1;

    float Radius() const { return info.radius; }
    float Range() const { return info.range; }
    float MissileSpeed() const { return info.projectileSpeed > 0 ? info.projectileSpeed : FLT_MAX; }
    int Delay() const { return info.spellDelay; }
    int DangerValue() const { return info.dangerlevel; }
    ZDSpellType Type() const { return info.spellType; }
    std::string SpellName() const { return info.spellName; }

    bool HasExpired() const {
        if (expired) return true;
        const int now = SDK::Variables::TickCount();
        if (now > endTime + 500) return true;
        if (isMissile && !missile.IsValid()) return true;
        return false;
    }

    Vec2 GetMissilePosition(int afterTime = 0) const {
        if (!isMissile || !missile.IsValid()) return startPos;
        const int baseTime = missileStartTime > 0 ? missileStartTime : startTime + info.spellDelay;
        const int elapsed = std::max(0,
            SDK::Variables::TickCount() + afterTime - baseTime);
        const float speed = std::max(1.0f, info.projectileSpeed);
        const float distance = static_cast<float>(elapsed) * speed / 1000.0f;
        return startPos + direction * distance;
    }
};

class SpellDetector {
public:
    static inline std::vector<TrackedSpell> activeSpells;
    static inline int spellIdCounter = 0;
    static inline bool initialized = false;
    using ChangeHandler = void(*)();
    static inline int changeSerial = 0;
    static inline int lastAddedSpellId = -1;
    static inline int lastChangeTick = 0;
    static inline uintptr_t lastImmediateProcessCastInfo = 0;
    static inline int lastImmediateProcessTick = 0;
    static inline ChangeHandler changeHandler = nullptr;

    static void Initialize() {
        if (initialized) return;
        initialized = true;
        SpellDatabase::Initialize();
        ZDLog("[ZDEvade][Detector] init tick=%d dbSpells=%d hooks=raw_process_spell,raw_do_cast,raw_process_cast,missile,update",
              SDK::Variables::TickCount(), static_cast<int>(SpellDatabase::Spells.size()));
        SDK::Events::AddOnCoreHook(SDK::Events::Hooks::OnProcessSpell, &OnRawProcessSpellImmediate);
        SDK::Events::AddOnCoreHook(SDK::Events::Hooks::OnDoCast, &OnRawDoCastImmediate);
        SDK::Events::AddOnCoreHook(SDK::Events::Hooks::ProcessCastSpell, &OnRawProcessCastSpellImmediate);
        SDK::Events::AddOnMissileCreate(&OnMissileCreate);
        SDK::Events::AddOnMissileDelete(&OnMissileDelete);
        SDK::Events::AddOnGameUpdate(&OnGameUpdate);
    }

    static void Shutdown() {
        if (!initialized) return;
        initialized = false;
        ZDLog("[ZDEvade][Detector] shutdown tick=%d active=%d serial=%d",
              SDK::Variables::TickCount(), static_cast<int>(activeSpells.size()), changeSerial);
        SDK::Events::RemoveOnGameUpdate(&OnGameUpdate);
        SDK::Events::RemoveOnMissileDelete(&OnMissileDelete);
        SDK::Events::RemoveOnMissileCreate(&OnMissileCreate);
        SDK::Events::RemoveOnCoreHook(SDK::Events::Hooks::ProcessCastSpell, &OnRawProcessCastSpellImmediate);
        SDK::Events::RemoveOnCoreHook(SDK::Events::Hooks::OnDoCast, &OnRawDoCastImmediate);
        SDK::Events::RemoveOnCoreHook(SDK::Events::Hooks::OnProcessSpell, &OnRawProcessSpellImmediate);
        activeSpells.clear();
        changeSerial = 0;
        lastAddedSpellId = -1;
        lastChangeTick = 0;
        lastImmediateProcessCastInfo = 0;
        lastImmediateProcessTick = 0;
        changeHandler = nullptr;
    }

    static std::vector<TrackedSpell>& ActiveSpells() { return activeSpells; }
    static int ChangeSerial() { return changeSerial; }
    static int LastAddedSpellId() { return lastAddedSpellId; }
    static int LastChangeTick() { return lastChangeTick; }
    static void SetChangeHandler(ChangeHandler handler) { changeHandler = handler; }

private:
    static void MarkChanged(int spellId) {
        ++changeSerial;
        lastAddedSpellId = spellId;
        lastChangeTick = SDK::Variables::TickCount();
        ZDLog("[ZDEvade][Detector] changed tick=%d serial=%d spellId=%d active=%d",
              lastChangeTick, changeSerial, spellId, static_cast<int>(activeSpells.size()));
        if (changeHandler) changeHandler();
    }

    static void OnGameUpdate(const SDK::Events::GameUpdateEventArgs&) {
        const size_t before = activeSpells.size();
        activeSpells.erase(
            std::remove_if(activeSpells.begin(), activeSpells.end(),
                [](const TrackedSpell& s) { return s.HasExpired(); }),
            activeSpells.end());
        if (activeSpells.size() != before) {
            ZDLog("[ZDEvade][Detector] expired tick=%d before=%d after=%d",
                  SDK::Variables::TickCount(), static_cast<int>(before), static_cast<int>(activeSpells.size()));
        }
    }

    static Vec2 ResolveEarlyEndPos(const SpellData& spellData, const Vec2& startPos,
                                   const Vec2& primaryEnd, const Vec2& secondaryEnd,
                                   const Vec2& heroPos) {
        Vec2 endPos = primaryEnd;
        if (endPos.IsZero()) endPos = secondaryEnd;
        if (endPos.IsZero() || endPos.DistanceSqr(startPos) < 25.0f) {
            endPos = heroPos;
        }
        if (spellData.spellType == ZDSpellType::Line) {
            Vec2 dir = endPos - startPos;
            const float len = dir.Length();
            if (len < 1.0f) dir = heroPos - startPos;
            if (dir.Length() < 1.0f) dir = Vec2(1.0f, 0.0f);
            dir = dir.Normalized();
            return startPos + dir * spellData.range;
        }
        return endPos;
    }

    static void RecalculateEndTime(TrackedSpell& spell) {
        const float speed = std::max(1.0f, spell.info.projectileSpeed);
        if (spell.info.projectileSpeed > 0.0f) {
            const float totalDist = spell.info.spellType == ZDSpellType::Line
                ? spell.startPos.Distance(spell.endPos) : spell.startPos.Distance(spell.endPos);
            spell.endTime = spell.startTime + spell.info.spellDelay +
                static_cast<int>(1000.0f * totalDist / speed) + spell.info.extraEndTime;
            if (spell.isMissile && spell.missileStartTime > 0) {
                spell.endTime = std::max(spell.endTime,
                    spell.missileStartTime + static_cast<int>(1000.0f * totalDist / speed) + spell.info.extraEndTime);
            }
        } else {
            spell.endTime = spell.startTime + spell.info.spellDelay + spell.info.extraEndTime;
        }
    }

    static void NormalizeTrackedSpell(TrackedSpell& spell) {
        spell.direction = (spell.endPos - spell.startPos).Normalized();
        if (spell.direction.IsZero()) spell.direction = Vec2(1.0f, 0.0f);
        if (spell.info.spellType == ZDSpellType::Line) {
            const float dist = spell.startPos.Distance(spell.endPos);
            if (!spell.info.useEndPosition || dist > spell.info.range) {
                spell.endPos = spell.startPos + spell.direction * spell.info.range;
            }
        }
        RecalculateEndTime(spell);
    }

    static bool IsRecentSameSpell(const TrackedSpell& existing, const SpellData& spellData,
                                  uint32_t casterNetworkId, int slot,
                                  const Vec2& startPos, int maxAgeMs) {
        if (existing.expired) return false;
        if (casterNetworkId != 0 && existing.casterNetworkId != 0 && existing.casterNetworkId != casterNetworkId) return false;
        if (slot >= 0 && existing.slot >= 0 && existing.slot != slot) return false;
        if (existing.info.spellName != spellData.spellName) {
            if (slot < 0 || existing.slot < 0 || existing.slot != slot) return false;
        }
        if (maxAgeMs > 0 && SDK::Variables::TickCount() - existing.startTime > maxAgeMs) return false;
        if (!startPos.IsZero() && !existing.startPos.IsZero() && existing.startPos.DistanceSqr(startPos) > 90000.0f) return false;
        return true;
    }

    static TrackedSpell* FindRecentTrackedSpell(const SpellData& spellData, uint32_t casterNetworkId,
                                                int slot, const Vec2& startPos, int maxAgeMs) {
        for (auto& existing : activeSpells) {
            if (IsRecentSameSpell(existing, spellData, casterNetworkId, slot, startPos, maxAgeMs)) {
                return &existing;
            }
        }
        return nullptr;
    }

    static bool IsBasicAttackName(const char* name) {
        return name && name[0] && (std::strstr(name, "BasicAttack") || std::strstr(name, "basicattack"));
    }

    static bool HasBasicAttackName(const SDK::Events::ProcessSpellEventArgs& args) {
        return IsBasicAttackName(args.SpellName) || IsBasicAttackName(args.PayloadSpellName) ||
            IsBasicAttackName(args.ScriptName) || IsBasicAttackName(args.SpellSlotName) ||
            IsBasicAttackName(args.MissileName);
    }

    static bool IsSpellSlot(int slot) {
        return slot >= 0 && slot <= 3;
    }

    static void UpdateTrackedSpell(TrackedSpell& spell, const SpellData& spellData,
                                   const Vec2& startPos, const Vec2& endPos,
                                   int slot) {
        spell.info = spellData;
        spell.startPos = startPos;
        spell.endPos = endPos;
        spell.slot = slot;
        NormalizeTrackedSpell(spell);
        ZDLog("[ZDEvade][Detector] update tick=%d id=%d spell=%s caster=%u slot=%d start=(%.1f,%.1f) end=(%.1f,%.1f) missile=%d endTime=%d",
              SDK::Variables::TickCount(), spell.spellId, spell.info.spellName.c_str(), spell.casterNetworkId, spell.slot,
              spell.startPos.x, spell.startPos.y, spell.endPos.x, spell.endPos.y, spell.isMissile ? 1 : 0, spell.endTime);
        MarkChanged(spell.spellId);
    }

    static void OnRawProcessSpellImmediate(const SDK::Events::CoreHookArgs& raw) {
        if (raw.Id != SDK::Events::Hooks::OnProcessSpell) return;
        auto decoded = ::Core::Events::DecodeProcessSpell(raw);
        const bool noisyBasic = HasBasicAttackName(decoded) && !decoded.PayloadSpellName[0];
        if (!noisyBasic) {
            ZDLog("[ZDEvade][Raw] OnProcessSpell hit tick=%d rcx=%p rdx=%p r8=%p r9=%p objNet=%u",
                  SDK::Variables::TickCount(), reinterpret_cast<void*>(raw.Rcx), reinterpret_cast<void*>(raw.Rdx),
                  reinterpret_cast<void*>(raw.R8), reinterpret_cast<void*>(raw.R9), raw.Object.NetworkId);
            ZDLog("[ZDEvade][Raw] OnProcessSpell decoded tick=%d senderValid=%d net=%u champ=%s slot=%d auto=%d spell=%s payload=%s script=%s slotName=%s missile=%s start=(%.1f,%.1f) end=(%.1f,%.1f) cast=(%.1f,%.1f)",
                  SDK::Variables::TickCount(), decoded.Sender.IsValid() ? 1 : 0, decoded.Sender.NetworkId, decoded.Sender.CharacterName,
                  decoded.Slot, decoded.IsAutoAttack ? 1 : 0, decoded.SpellName, decoded.PayloadSpellName, decoded.ScriptName,
                  decoded.SpellSlotName, decoded.MissileName, decoded.StartPosition.x, decoded.StartPosition.z,
                  decoded.EndPosition.x, decoded.EndPosition.z, decoded.CastPosition.x, decoded.CastPosition.z);
        }
        lastImmediateProcessCastInfo = decoded.CastInfo;
        lastImmediateProcessTick = SDK::Variables::TickCount();
        OnProcessSpell(decoded);
    }

    static void OnRawDoCastImmediate(const SDK::Events::CoreHookArgs& raw) {
        if (raw.Id != SDK::Events::Hooks::OnDoCast) return;
        auto decoded = ::Core::Events::DecodeDoCast(raw);
        if (decoded.CastInfo && decoded.CastInfo == lastImmediateProcessCastInfo &&
            SDK::Variables::TickCount() - lastImmediateProcessTick <= 50) {
            return;
        }
        const bool noisyBasic = HasBasicAttackName(decoded) && !decoded.PayloadSpellName[0];
        if (!noisyBasic) {
            ZDLog("[ZDEvade][Raw] OnDoCast hit tick=%d rcx=%p rdx=%p r8=%p r9=%p objNet=%u",
                  SDK::Variables::TickCount(), reinterpret_cast<void*>(raw.Rcx), reinterpret_cast<void*>(raw.Rdx),
                  reinterpret_cast<void*>(raw.R8), reinterpret_cast<void*>(raw.R9), raw.Object.NetworkId);
            ZDLog("[ZDEvade][Raw] OnDoCast decoded tick=%d senderValid=%d net=%u champ=%s slot=%d auto=%d spell=%s payload=%s script=%s slotName=%s missile=%s start=(%.1f,%.1f) end=(%.1f,%.1f) cast=(%.1f,%.1f)",
                  SDK::Variables::TickCount(), decoded.Sender.IsValid() ? 1 : 0, decoded.Sender.NetworkId, decoded.Sender.CharacterName,
                  decoded.Slot, decoded.IsAutoAttack ? 1 : 0, decoded.SpellName, decoded.PayloadSpellName, decoded.ScriptName,
                  decoded.SpellSlotName, decoded.MissileName, decoded.StartPosition.x, decoded.StartPosition.z,
                  decoded.EndPosition.x, decoded.EndPosition.z, decoded.CastPosition.x, decoded.CastPosition.z);
        }
        OnProcessSpell(decoded);
    }

    static void OnRawProcessCastSpellImmediate(const SDK::Events::CoreHookArgs& raw) {
        if (raw.Id != SDK::Events::Hooks::ProcessCastSpell) return;
        ZDLog("[ZDEvade][Raw] ProcessCastSpell hit tick=%d rcx=%p rdx=%p r8=%p r9=%p objNet=%u",
              SDK::Variables::TickCount(), reinterpret_cast<void*>(raw.Rcx), reinterpret_cast<void*>(raw.Rdx),
              reinterpret_cast<void*>(raw.R8), reinterpret_cast<void*>(raw.R9), raw.Object.NetworkId);
        auto decoded = ::Core::Events::DecodeProcessCastSpell(raw);
        ZDLog("[ZDEvade][Raw] ProcessCastSpell decoded tick=%d senderValid=%d net=%u champ=%s slot=%d target=%u req=%p start=(%.1f,%.1f) end=(%.1f,%.1f)",
              SDK::Variables::TickCount(), decoded.Sender.IsValid() ? 1 : 0, decoded.Sender.NetworkId,
              decoded.Sender.CharacterName, decoded.Slot, decoded.TargetNetworkId, reinterpret_cast<void*>(decoded.CastRequest),
              decoded.StartPosition.x, decoded.StartPosition.z, decoded.EndPosition.x, decoded.EndPosition.z);
        OnProcessCastSpell(decoded);
    }

    static void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
        const int tick = SDK::Variables::TickCount();
        ZDLog("[ZDEvade][ProcessSpell] enter tick=%d senderValid=%d net=%u slot=%d auto=%d spell=%s payload=%s script=%s slotName=%s missile=%s",
              tick, args.Sender.IsValid() ? 1 : 0, args.Sender.NetworkId, args.Slot, args.IsAutoAttack ? 1 : 0,
              args.SpellName, args.PayloadSpellName, args.ScriptName, args.SpellSlotName, args.MissileName);
        if (!args.Sender.IsValid()) {
            ZDLog("[ZDEvade][ProcessSpell] skip invalid_sender tick=%d", tick);
            return;
        }
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            ZDLog("[ZDEvade][ProcessSpell] skip invalid_player tick=%d net=%u", tick, args.Sender.NetworkId);
            return;
        }
        if (args.Sender.NetworkId == static_cast<uint32_t>(player.NetworkId())) {
            ZDLog("[ZDEvade][ProcessSpell] skip self tick=%d net=%u spell=%s", tick, args.Sender.NetworkId, args.SpellName);
            return;
        }
        const bool hasBasicAttackName = HasBasicAttackName(args);
        const bool hasCastPosition = !args.CastPosition.To2D().IsZero();
        const bool hasPayloadChampionSlot = args.PayloadSpellName[0] && IsSpellSlot(args.Slot);
        const bool canChampionSlotFallback = !hasBasicAttackName && hasPayloadChampionSlot && hasCastPosition;
        if (hasBasicAttackName) {
            ZDLog("[ZDEvade][ProcessSpell] skip basic_attack_name tick=%d net=%u spell=%s payload=%s slot=%d cast=(%.1f,%.1f)",
                  tick, args.Sender.NetworkId, args.SpellName, args.PayloadSpellName, args.Slot,
                  args.CastPosition.x, args.CastPosition.z);
            return;
        }

        const SpellData* spellData = nullptr;
        const char* matchedBy = "none";
        for (const char* name : { args.SpellName, args.PayloadSpellName, args.ScriptName, args.SpellSlotName, args.MissileName }) {
            if (!name || !name[0]) continue;
            if (IsBasicAttackName(name)) {
                ZDLog("[ZDEvade][ProcessSpell] candidate_skip_basic tick=%d name=%s", tick, name);
                continue;
            }
            spellData = FindSpellByName(name);
            if (spellData) {
                matchedBy = name;
                ZDLog("[ZDEvade][ProcessSpell] matched_spell_name tick=%d input=%s db=%s", tick, name, spellData->spellName.c_str());
                break;
            }
            spellData = FindByMissileName(name);
            if (spellData) {
                matchedBy = name;
                ZDLog("[ZDEvade][ProcessSpell] matched_missile_name tick=%d input=%s db=%s", tick, name, spellData->spellName.c_str());
                break;
            }
            ZDLog("[ZDEvade][ProcessSpell] candidate_no_match tick=%d input=%s", tick, name);
        }

        const auto caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
            static_cast<int>(args.Sender.NetworkId));
        ZDLog("[ZDEvade][ProcessSpell] caster_resolve tick=%d valid=%d ally=%d net=%u char=%s",
              tick, caster.IsValid() ? 1 : 0, caster.IsValid() && caster.IsAlly() ? 1 : 0,
              args.Sender.NetworkId, caster.IsValid() ? caster.CharacterName().c_str() : "none");
        if (caster.IsValid() && caster.IsAlly()) {
            ZDLog("[ZDEvade][ProcessSpell] skip ally tick=%d net=%u", tick, args.Sender.NetworkId);
            return;
        }

        if (!spellData && canChampionSlotFallback) {
            const char* championName = args.PayloadSpellName;
            spellData = FindByChampionAndSlot(championName, args.Slot);
            if (spellData) {
                matchedBy = "payload";
                ZDLog("[ZDEvade][ProcessSpell] matched_champion_slot tick=%d source=payload champ=%s slot=%d db=%s autoFlag=%d cast=(%.1f,%.1f)",
                      tick, championName, args.Slot, spellData->spellName.c_str(), args.IsAutoAttack ? 1 : 0,
                      args.CastPosition.x, args.CastPosition.z);
            }
        }
        if (args.IsAutoAttack && !spellData) {
            ZDLog("[ZDEvade][ProcessSpell] skip auto_attack tick=%d net=%u spell=%s payload=%s slot=%d cast=(%.1f,%.1f)",
                  tick, args.Sender.NetworkId, args.SpellName, args.PayloadSpellName, args.Slot,
                  args.CastPosition.x, args.CastPosition.z);
            return;
        }
        if (args.IsAutoAttack && spellData) {
            ZDLog("[ZDEvade][ProcessSpell] allow_auto_flag_skill tick=%d db=%s matchedBy=%s net=%u slot=%d",
                  tick, spellData->spellName.c_str(), matchedBy, args.Sender.NetworkId, args.Slot);
        }
        if (!spellData) {
            ZDLog("[ZDEvade][ProcessSpell] skip no_database_match tick=%d net=%u slot=%d spell=%s payload=%s script=%s slotName=%s missile=%s",
                  tick, args.Sender.NetworkId, args.Slot, args.SpellName, args.PayloadSpellName, args.ScriptName, args.SpellSlotName, args.MissileName);
            return;
        }

        Vec2 startPos = args.StartPosition.To2D();
        if (startPos.IsZero() && caster.IsValid()) {
            startPos = caster.Position().To2D();
            ZDLog("[ZDEvade][ProcessSpell] start_fallback_caster tick=%d spell=%s start=(%.1f,%.1f)",
                  tick, spellData->spellName.c_str(), startPos.x, startPos.y);
        }
        if (startPos.IsZero()) {
            ZDLog("[ZDEvade][ProcessSpell] skip zero_start tick=%d spell=%s", tick, spellData->spellName.c_str());
            return;
        }
        const Vec2 heroPos = player.ServerPosition().To2D();
        Vec2 endPos = ResolveEarlyEndPos(*spellData, startPos, args.EndPosition.To2D(), args.CastPosition.To2D(), heroPos);
        ZDLog("[ZDEvade][ProcessSpell] resolved tick=%d db=%s matchedBy=%s caster=%u slot=%d start=(%.1f,%.1f) rawEnd=(%.1f,%.1f) cast=(%.1f,%.1f) finalEnd=(%.1f,%.1f) hero=(%.1f,%.1f)",
              tick, spellData->spellName.c_str(), matchedBy, args.Sender.NetworkId, args.Slot,
              startPos.x, startPos.y, args.EndPosition.x, args.EndPosition.z, args.CastPosition.x, args.CastPosition.z,
              endPos.x, endPos.y, heroPos.x, heroPos.y);

        if (TrackedSpell* existing = FindRecentTrackedSpell(*spellData, args.Sender.NetworkId, args.Slot, startPos, 1500)) {
            if (existing->isMissile) {
                ZDLog("[ZDEvade][ProcessSpell] skip late_missile_bound_update tick=%d existingId=%d db=%s caster=%u slot=%d",
                      tick, existing->spellId, spellData->spellName.c_str(), args.Sender.NetworkId, args.Slot);
                return;
            }
            ZDLog("[ZDEvade][ProcessSpell] update_existing tick=%d existingId=%d db=%s caster=%u slot=%d",
                  tick, existing->spellId, spellData->spellName.c_str(), args.Sender.NetworkId, args.Slot);
            UpdateTrackedSpell(*existing, *spellData, startPos, endPos, args.Slot);
            return;
        }

        ZDLog("[ZDEvade][ProcessSpell] create_new tick=%d db=%s caster=%u slot=%d", tick, spellData->spellName.c_str(), args.Sender.NetworkId, args.Slot);
        CreateTrackedSpell(*spellData, startPos, endPos, false, SDK::MissileClient(), args.Sender.NetworkId,
                           args.Slot);
    }

    static void OnProcessCastSpell(const SDK::Events::CastSpellEventArgs& args) {
        const int tick = SDK::Variables::TickCount();
        ZDLog("[ZDEvade][ProcessCast] enter tick=%d senderValid=%d net=%u champ=%s slot=%d target=%u start=(%.1f,%.1f) end=(%.1f,%.1f)",
              tick, args.Sender.IsValid() ? 1 : 0, args.Sender.NetworkId, args.Sender.CharacterName,
              args.Slot, args.TargetNetworkId, args.StartPosition.x, args.StartPosition.z, args.EndPosition.x, args.EndPosition.z);
        if (!args.Sender.IsValid()) {
            ZDLog("[ZDEvade][ProcessCast] skip invalid_sender tick=%d", tick);
            return;
        }
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            ZDLog("[ZDEvade][ProcessCast] skip invalid_player tick=%d net=%u", tick, args.Sender.NetworkId);
            return;
        }
        if (args.Sender.NetworkId == static_cast<uint32_t>(player.NetworkId())) {
            ZDLog("[ZDEvade][ProcessCast] skip self tick=%d net=%u champ=%s slot=%d", tick, args.Sender.NetworkId, args.Sender.CharacterName, args.Slot);
            return;
        }
        const auto caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
            static_cast<int>(args.Sender.NetworkId));
        ZDLog("[ZDEvade][ProcessCast] caster_resolve tick=%d valid=%d ally=%d net=%u char=%s",
              tick, caster.IsValid() ? 1 : 0, caster.IsValid() && caster.IsAlly() ? 1 : 0,
              args.Sender.NetworkId, caster.IsValid() ? caster.CharacterName().c_str() : args.Sender.CharacterName);
        if (caster.IsValid() && caster.IsAlly()) {
            ZDLog("[ZDEvade][ProcessCast] skip ally tick=%d net=%u", tick, args.Sender.NetworkId);
            return;
        }
        const SpellData* spellData = FindByChampionAndSlot(args.Sender.CharacterName, args.Slot);
        if (!spellData && caster.IsValid()) spellData = FindByChampionAndSlot(caster.CharacterName().c_str(), args.Slot);
        if (!spellData) {
            ZDLog("[ZDEvade][ProcessCast] skip no_champion_slot_match tick=%d champ=%s slot=%d net=%u",
                  tick, args.Sender.CharacterName, args.Slot, args.Sender.NetworkId);
            return;
        }
        Vec2 startPos = args.StartPosition.To2D();
        if (startPos.IsZero() && caster.IsValid()) {
            startPos = caster.Position().To2D();
            ZDLog("[ZDEvade][ProcessCast] start_fallback_caster tick=%d spell=%s start=(%.1f,%.1f)",
                  tick, spellData->spellName.c_str(), startPos.x, startPos.y);
        }
        if (startPos.IsZero()) {
            ZDLog("[ZDEvade][ProcessCast] skip zero_start tick=%d spell=%s", tick, spellData->spellName.c_str());
            return;
        }
        const Vec2 heroPos = player.ServerPosition().To2D();
        Vec2 endPos = ResolveEarlyEndPos(*spellData, startPos, args.EndPosition.To2D(), Vec2(0.0f, 0.0f), heroPos);
        ZDLog("[ZDEvade][ProcessCast] resolved tick=%d db=%s caster=%u slot=%d start=(%.1f,%.1f) rawEnd=(%.1f,%.1f) finalEnd=(%.1f,%.1f) hero=(%.1f,%.1f)",
              tick, spellData->spellName.c_str(), args.Sender.NetworkId, args.Slot,
              startPos.x, startPos.y, args.EndPosition.x, args.EndPosition.z, endPos.x, endPos.y, heroPos.x, heroPos.y);
        if (TrackedSpell* existing = FindRecentTrackedSpell(*spellData, args.Sender.NetworkId, args.Slot, startPos, 1500)) {
            if (existing->isMissile) {
                ZDLog("[ZDEvade][ProcessCast] skip late_missile_bound_update tick=%d existingId=%d db=%s caster=%u slot=%d",
                      tick, existing->spellId, spellData->spellName.c_str(), args.Sender.NetworkId, args.Slot);
                return;
            }
            ZDLog("[ZDEvade][ProcessCast] update_existing tick=%d existingId=%d db=%s caster=%u slot=%d",
                  tick, existing->spellId, spellData->spellName.c_str(), args.Sender.NetworkId, args.Slot);
            UpdateTrackedSpell(*existing, *spellData, startPos, endPos, args.Slot);
            return;
        }
        ZDLog("[ZDEvade][ProcessCast] create_new tick=%d db=%s caster=%u slot=%d", tick, spellData->spellName.c_str(), args.Sender.NetworkId, args.Slot);
        CreateTrackedSpell(*spellData, startPos, endPos, false, SDK::MissileClient(), args.Sender.NetworkId,
                           args.Slot);
    }

    static void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
        const int tick = SDK::Variables::TickCount();
        ZDLog("[ZDEvade][MissileCreate] enter tick=%d senderValid=%d senderNet=%u spellArg=%s missileArg=%s ptr=%p",
              tick, args.Sender.IsValid() ? 1 : 0, args.Sender.NetworkId, args.SpellName, args.MissileName,
              reinterpret_cast<void*>(args.Sender.Ptr));
        if (!args.Sender.IsValid()) {
            ZDLog("[ZDEvade][MissileCreate] skip invalid_sender tick=%d", tick);
            return;
        }
        const SDK::MissileClient missile(args.Sender.Ptr);
        if (!missile.IsValid()) {
            ZDLog("[ZDEvade][MissileCreate] skip invalid_missile tick=%d senderNet=%u", tick, args.Sender.NetworkId);
            return;
        }

        std::string missileName;
        if (args.MissileName[0]) missileName = args.MissileName;
        else if (args.SpellName[0]) missileName = args.SpellName;
        else missileName = missile.SpellName();

        ZDLog("[ZDEvade][MissileCreate] resolved_name tick=%d missileNet=%d caster=%d name=%s spellArg=%s missileArg=%s",
              tick, missile.NetworkId(), missile.CasterNetworkId(), missileName.c_str(), args.SpellName, args.MissileName);
        if (IsBasicAttackName(missileName.c_str())) {
            ZDLog("[ZDEvade][MissileCreate] skip basic_attack tick=%d name=%s", tick, missileName.c_str());
            return;
        }

        const int casterNetId = missile.CasterNetworkId();
        const Vec2 missileStart = missile.StartPosition().To2D();
        const Vec2 missileEnd = missile.EndPosition().To2D();
        Vec2 missileDir = (missileEnd - missileStart).Normalized();
        if (missileDir.IsZero()) missileDir = Vec2(1.0f, 0.0f);

        const SpellData* spellData = FindByMissileName(missileName);
        if (!spellData && args.SpellName[0]) spellData = FindSpellByName(args.SpellName);
        ZDLog("[ZDEvade][MissileCreate] db_match tick=%d name=%s matched=%d db=%s caster=%d start=(%.1f,%.1f) end=(%.1f,%.1f) active=%d",
              tick, missileName.c_str(), spellData ? 1 : 0, spellData ? spellData->spellName.c_str() : "none", casterNetId,
              missileStart.x, missileStart.y, missileEnd.x, missileEnd.y, static_cast<int>(activeSpells.size()));

        for (auto& tracked : activeSpells) {
            if (tracked.isMissile) {
                ZDLog("[ZDEvade][MissileCreate] bind_skip already_missile tick=%d trackedId=%d", tick, tracked.spellId);
                continue;
            }
            if (tracked.casterNetworkId != static_cast<uint32_t>(casterNetId)) continue;
            const bool sameSpell = spellData && tracked.info.spellName == spellData->spellName;
            const float dot = tracked.direction.x * missileDir.x + tracked.direction.y * missileDir.y;
            ZDLog("[ZDEvade][MissileCreate] bind_candidate tick=%d trackedId=%d tracked=%s same=%d dot=%.3f trackedCaster=%u missileCaster=%d",
                  tick, tracked.spellId, tracked.info.spellName.c_str(), sameSpell ? 1 : 0, dot, tracked.casterNetworkId, casterNetId);
            if (!sameSpell && dot < 0.95f) {
                ZDLog("[ZDEvade][MissileCreate] bind_skip direction tick=%d trackedId=%d dot=%.3f", tick, tracked.spellId, dot);
                continue;
            }
            if (sameSpell) tracked.info = *spellData;
            tracked.isMissile = true;
            tracked.missile = missile;
            tracked.missileStartTime = SDK::Variables::TickCount() - SDK::Game::Ping() / 2;
            tracked.startPos = missileStart;
            tracked.direction = missileDir;
            if (tracked.info.spellType == ZDSpellType::Line) {
                tracked.endPos = missileStart + missileDir * tracked.info.range;
            } else {
                tracked.endPos = missileEnd;
            }
            NormalizeTrackedSpell(tracked);
            ZDLog("[ZDEvade][MissileCreate] bind_success tick=%d trackedId=%d spell=%s missileNet=%d deltaFromStart=%d missileStartTime=%d end=(%.1f,%.1f)",
                  SDK::Variables::TickCount(), tracked.spellId, tracked.info.spellName.c_str(), missile.NetworkId(),
                  SDK::Variables::TickCount() - tracked.startTime, tracked.missileStartTime, tracked.endPos.x, tracked.endPos.y);
            MarkChanged(tracked.spellId);
            return;
        }
        if (!spellData) {
            ZDLog("[ZDEvade][MissileCreate] skip no_database_match tick=%d name=%s caster=%d", tick, missileName.c_str(), casterNetId);
            return;
        }

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            ZDLog("[ZDEvade][MissileCreate] skip invalid_player tick=%d name=%s", tick, missileName.c_str());
            return;
        }
        const auto caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(casterNetId);
        if (caster.IsValid() && caster.IsAlly()) {
            ZDLog("[ZDEvade][MissileCreate] skip ally tick=%d name=%s caster=%d", tick, missileName.c_str(), casterNetId);
            return;
        }

        ZDLog("[ZDEvade][MissileCreate] create_new tick=%d db=%s caster=%d missileNet=%d", tick, spellData->spellName.c_str(), casterNetId, missile.NetworkId());
        CreateTrackedSpell(*spellData, missileStart, missileEnd, true, missile,
                           static_cast<uint32_t>(casterNetId));
    }

    static void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
        const int tick = SDK::Variables::TickCount();
        const int deletedNetId = static_cast<int>(args.Sender.NetworkId);
        ZDLog("[ZDEvade][MissileDelete] enter tick=%d senderNet=%d missileArg=%u active=%d",
              tick, deletedNetId, args.MissileNetworkId, static_cast<int>(activeSpells.size()));
        for (auto& spell : activeSpells) {
            if (spell.isMissile && spell.missile.IsValid()) {
                const int missileNetId = spell.missile.NetworkId();
                if (missileNetId == deletedNetId ||
                    missileNetId == static_cast<int>(args.MissileNetworkId)) {
                    spell.expired = true;
                    ZDLog("[ZDEvade][MissileDelete] expire tick=%d spellId=%d spell=%s missileNet=%d deleted=%d arg=%u",
                          tick, spell.spellId, spell.info.spellName.c_str(), missileNetId, deletedNetId, args.MissileNetworkId);
                }
            }
        }
    }

    static void CreateTrackedSpell(const SpellData& spellData, const Vec2& startPos,
                                   const Vec2& endPos, bool isMissile,
                                   const SDK::MissileClient& missile = SDK::MissileClient(),
                                   uint32_t casterNetworkId = 0,
                                   int slot = -1,
                                   int startTimeOverride = 0) {
        TrackedSpell spell;
        spell.info = spellData;
        spell.startPos = startPos;
        spell.endPos = endPos;
        spell.startTime = startTimeOverride > 0
            ? startTimeOverride
            : SDK::Variables::TickCount() - SDK::Game::Ping() / 2;
        spell.missileStartTime = isMissile ? spell.startTime : 0;
        spell.isMissile = isMissile;
        spell.missile = missile;
        spell.spellId = spellIdCounter++;
        spell.casterNetworkId = casterNetworkId;
        spell.slot = slot;
        NormalizeTrackedSpell(spell);
        ZDLog("[ZDEvade][Detector] create_candidate tick=%d id=%d spell=%s caster=%u slot=%d missile=%d startTime=%d nowMinusStart=%d delay=%d speed=%.1f radius=%.1f range=%.1f start=(%.1f,%.1f) end=(%.1f,%.1f) endTime=%d activeBefore=%d",
              SDK::Variables::TickCount(), spell.spellId, spell.info.spellName.c_str(), spell.casterNetworkId, spell.slot, spell.isMissile ? 1 : 0,
              spell.startTime, SDK::Variables::TickCount() - spell.startTime, spell.info.spellDelay, spell.info.projectileSpeed,
              spell.info.radius, spell.info.range, spell.startPos.x, spell.startPos.y, spell.endPos.x, spell.endPos.y,
              spell.endTime, static_cast<int>(activeSpells.size()));

        for (const auto& existing : activeSpells) {
            if (IsRecentSameSpell(existing, spell.info, spell.casterNetworkId, spell.slot, spell.startPos, 1200)) {
                ZDLog("[ZDEvade][Detector] create_dedup tick=%d newId=%d existingId=%d spell=%s caster=%u slot=%d age=%d distSqr=%.1f",
                      SDK::Variables::TickCount(), spell.spellId, existing.spellId, spell.info.spellName.c_str(), spell.casterNetworkId,
                      spell.slot, SDK::Variables::TickCount() - existing.startTime, existing.startPos.DistanceSqr(spell.startPos));
                return;
            }
        }

        activeSpells.push_back(spell);
        ZDLog("[ZDEvade][Detector] create_pushed tick=%d id=%d spell=%s activeAfter=%d",
              SDK::Variables::TickCount(), spell.spellId, spell.info.spellName.c_str(), static_cast<int>(activeSpells.size()));
        MarkChanged(spell.spellId);
    }

    static const SpellData* FindSpellByName(const std::string& name) {
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        for (const auto& spell : SpellDatabase::Spells) {
            std::string spellName = spell.spellName;
            std::transform(spellName.begin(), spellName.end(), spellName.begin(), ::tolower);
            if (lowerName == spellName) return &spell;
            for (const auto& extra : spell.extraSpellNames) {
                std::string extraName = extra;
                std::transform(extraName.begin(), extraName.end(), extraName.begin(), ::tolower);
                if (lowerName == extraName) return &spell;
            }
        }
        return nullptr;
    }

    static const SpellData* FindByChampionAndSlot(const char* championName, int slot) {
        if (!championName || !championName[0] || slot < 0 || slot > 3) return nullptr;
        ZDSpellSlot zdSlot = static_cast<ZDSpellSlot>(slot);
        for (const auto& spell : SpellDatabase::Spells) {
            if (_stricmp(spell.charName.c_str(), championName) == 0 && spell.spellKey == zdSlot) {
                return &spell;
            }
        }
        return nullptr;
    }

    static const SpellData* FindByMissileName(const std::string& name) {
        if (name.empty()) return nullptr;
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        // 1) Exact missileName match
        for (const auto& spell : SpellDatabase::Spells) {
            if (!spell.missileName.empty()) {
                std::string m = spell.missileName;
                std::transform(m.begin(), m.end(), m.begin(), ::tolower);
                if (lowerName == m) return &spell;
            }
            for (const auto& extra : spell.extraMissileNames) {
                std::string e = extra;
                std::transform(e.begin(), e.end(), e.begin(), ::tolower);
                if (lowerName == e) return &spell;
            }
        }

        // 2) Strip common suffixes (Mis, Missile) and try matching spellName
        std::string stripped = lowerName;
        for (const char* suffix : {"missile", "mis", "basicattack", "basicattack2"}) {
            if (stripped.length() > strlen(suffix) &&
                stripped.compare(stripped.length() - strlen(suffix), strlen(suffix), suffix) == 0) {
                stripped = stripped.substr(0, stripped.length() - strlen(suffix));
                break;
            }
        }

        if (stripped != lowerName) {
            for (const auto& spell : SpellDatabase::Spells) {
                std::string sn = spell.spellName;
                std::transform(sn.begin(), sn.end(), sn.begin(), ::tolower);
                if (stripped == sn) return &spell;
                // Also try missileName with suffix stripped
                if (!spell.missileName.empty()) {
                    std::string m = spell.missileName;
                    std::transform(m.begin(), m.end(), m.begin(), ::tolower);
                    std::string mStripped = m;
                    for (const char* s2 : {"missile", "mis"}) {
                        if (mStripped.length() > strlen(s2) &&
                            mStripped.compare(mStripped.length() - strlen(s2), strlen(s2), s2) == 0) {
                            mStripped = mStripped.substr(0, mStripped.length() - strlen(s2));
                            break;
                        }
                    }
                    if (stripped == mStripped) return &spell;
                }
            }
        }

        return nullptr;
    }
};

} // namespace ZDEvade
