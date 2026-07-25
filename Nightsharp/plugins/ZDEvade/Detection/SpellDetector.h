#pragma once

// ============================================================================
// SpellDetector.h — Custom skillshot detection for ZDEvade
//
// Hooks OnProcessSpell + OnMissileCreate + OnMissileDelete directly,
// looks up spells in ZDEvade::SpellDatabase (not SDK's database),
// creates TrackedSpell objects for evade logic and drawing.
// ============================================================================

#include "TrackedSpell.h"
#include "../Database/SpellDatabase.h"
#include "../../../SDK/SDK.h"
#include "../../../Core/CoreControl.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace ZDEvade {

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
        SDK::Events::AddOnCoreHook(SDK::Events::Hooks::OnProcessSpell, &OnRawProcessSpellImmediate);
        SDK::Events::AddOnCoreHook(SDK::Events::Hooks::OnDoCast, &OnRawDoCastImmediate);
        SDK::Events::AddOnMissileCreate(&OnMissileCreate);
        SDK::Events::AddOnMissileDelete(&OnMissileDelete);
        SDK::Events::AddOnGameUpdate(&OnGameUpdate);
    }

    static void Shutdown() {
        if (!initialized) return;
        initialized = false;
        SDK::Events::RemoveOnGameUpdate(&OnGameUpdate);
        SDK::Events::RemoveOnMissileDelete(&OnMissileDelete);
        SDK::Events::RemoveOnMissileCreate(&OnMissileCreate);
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
        if (changeHandler) changeHandler();
    }

    static void OnGameUpdate(const SDK::Events::GameUpdateEventArgs&) {
        activeSpells.erase(
            std::remove_if(activeSpells.begin(), activeSpells.end(),
                [](const TrackedSpell& s) { return s.HasExpired(); }),
            activeSpells.end());
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

    static bool StartsWithNoCase(const char* name, const char* prefix) {
        if (!name || !prefix) return false;
        const size_t prefixLen = std::strlen(prefix);
        if (std::strlen(name) < prefixLen) return false;
        for (size_t i = 0; i < prefixLen; ++i) {
            char a = name[i];
            char b = prefix[i];
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
            if (a != b) return false;
        }
        return true;
    }

    static bool IsUtilityCastName(const char* name) {
        return StartsWithNoCase(name, "Summoner") || StartsWithNoCase(name, "Item");
    }

    static bool IsStaleSpellSlotNameFallback(const SDK::Events::ProcessSpellEventArgs& args, const char* name) {
        if (!name || !name[0] || !args.SpellSlotName[0]) return false;
        if (std::strcmp(name, args.SpellSlotName) != 0) return false;
        return IsUtilityCastName(args.SpellName) || IsUtilityCastName(args.ScriptName) || IsUtilityCastName(args.MissileName);
    }

    static void UpdateTrackedSpell(TrackedSpell& spell, const SpellData& spellData,
                                   const Vec2& startPos, const Vec2& endPos,
                                   int slot) {
        spell.info = spellData;
        spell.startPos = startPos;
        spell.endPos = endPos;
        spell.slot = slot;
        NormalizeTrackedSpell(spell);
        MarkChanged(spell.spellId);
    }

    static void OnRawProcessSpellImmediate(const SDK::Events::CoreHookArgs& raw) {
        if (raw.Id != SDK::Events::Hooks::OnProcessSpell) return;
        auto decoded = ::Core::Events::DecodeProcessSpell(raw);
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
        OnProcessSpell(decoded);
    }

    static void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
        if (!args.Sender.IsValid()) {
            return;
        }
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            return;
        }
        if (args.Sender.NetworkId == static_cast<uint32_t>(player.NetworkId())) {
            return;
        }
        const bool hasBasicAttackName = HasBasicAttackName(args);
        if (hasBasicAttackName) {
            return;
        }

        const SpellData* spellData = nullptr;
        for (const char* name : { args.SpellName, args.PayloadSpellName, args.ScriptName, args.SpellSlotName, args.MissileName }) {
            if (!name || !name[0]) continue;
            if (IsBasicAttackName(name)) {
                continue;
            }
            if (IsStaleSpellSlotNameFallback(args, name)) {
                continue;
            }
            spellData = FindSpellByName(name);
            if (spellData) {
                break;
            }
            spellData = FindByMissileName(name);
            if (spellData) {
                break;
            }
        }

        const auto caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
            static_cast<int>(args.Sender.NetworkId));
        if (caster.IsValid() && caster.IsAlly()) {
            return;
        }

        if (args.IsAutoAttack && !spellData) {
            return;
        }
        if (!spellData) {
            return;
        }

        Vec2 startPos = args.StartPosition.To2D();
        if (startPos.IsZero() && caster.IsValid()) {
            startPos = caster.Position().To2D();
        }
        if (startPos.IsZero()) {
            return;
        }
        const Vec2 heroPos = player.ServerPosition().To2D();
        Vec2 endPos = ResolveEarlyEndPos(*spellData, startPos, args.EndPosition.To2D(), args.CastPosition.To2D(), heroPos);

        if (TrackedSpell* existing = FindRecentTrackedSpell(*spellData, args.Sender.NetworkId, args.Slot, startPos, 1500)) {
            if (existing->isMissile) {
                return;
            }
            UpdateTrackedSpell(*existing, *spellData, startPos, endPos, args.Slot);
            return;
        }

        CreateTrackedSpell(*spellData, startPos, endPos, false, SDK::MissileClient(), args.Sender.NetworkId,
                           args.Slot);
    }

    static void OnMissileCreate(const SDK::Events::ObjectEventArgs& args) {
        if (!args.Sender.IsValid()) {
            return;
        }
        const SDK::MissileClient missile(args.Sender.Ptr);
        if (!missile.IsValid()) {
            return;
        }

        std::string missileName;
        if (args.MissileName[0]) missileName = args.MissileName;
        else if (args.SpellName[0]) missileName = args.SpellName;
        else missileName = missile.SpellName();

        if (IsBasicAttackName(missileName.c_str())) {
            return;
        }

        const int casterNetId = missile.CasterNetworkId();
        const Vec2 missileStart = missile.StartPosition().To2D();
        const Vec2 missileEnd = missile.EndPosition().To2D();
        Vec2 missileDir = (missileEnd - missileStart).Normalized();
        if (missileDir.IsZero()) missileDir = Vec2(1.0f, 0.0f);

        const SpellData* spellData = FindByMissileName(missileName);
        if (!spellData && args.SpellName[0]) spellData = FindSpellByName(args.SpellName);

        for (auto& tracked : activeSpells) {
            if (tracked.isMissile) {
                continue;
            }
            if (tracked.casterNetworkId != static_cast<uint32_t>(casterNetId)) continue;
            const bool sameSpell = spellData && tracked.info.spellName == spellData->spellName;
            const float dot = tracked.direction.x * missileDir.x + tracked.direction.y * missileDir.y;
            if (!sameSpell && dot < 0.95f) {
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
            MarkChanged(tracked.spellId);
            return;
        }
        if (!spellData) {
            return;
        }

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            return;
        }
        const auto caster = SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(casterNetId);
        if (caster.IsValid() && caster.IsAlly()) {
            return;
        }

        CreateTrackedSpell(*spellData, missileStart, missileEnd, true, missile,
                           static_cast<uint32_t>(casterNetId));
    }

    static void OnMissileDelete(const SDK::Events::ObjectEventArgs& args) {
        const int deletedNetId = static_cast<int>(args.Sender.NetworkId);
        for (auto& spell : activeSpells) {
            if (spell.isMissile && spell.missile.IsValid()) {
                const int missileNetId = spell.missile.NetworkId();
                if (missileNetId == deletedNetId ||
                    missileNetId == static_cast<int>(args.MissileNetworkId)) {
                    spell.expired = true;
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

        for (const auto& existing : activeSpells) {
            if (IsRecentSameSpell(existing, spell.info, spell.casterNetworkId, spell.slot, spell.startPos, 1200)) {
                return;
            }
        }

        activeSpells.push_back(spell);
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
