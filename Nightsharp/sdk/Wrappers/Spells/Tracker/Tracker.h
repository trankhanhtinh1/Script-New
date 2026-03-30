#pragma once

#include "Detector.h"
#include "Skillshots/_ZiggsR.h"
#include "../../../Core/Game.h"
#include "../../../Core/Objects.h"

#include <algorithm>
#include <string>
#include <vector>

namespace SDK::SpellTracker {

struct TrackedSpellEntry {
    int CasterNetworkId = 0;
    int TargetNetworkId = 0;
    int MissileNetworkId = 0;
    SpellSlot Slot = SpellSlot::Unknown;
    std::string SpellName = {};
    Vector3 Start = {};
    Vector3 End = {};
    Vector3 CastPosition = {};
    float Delay = 0.0f;
    float Radius = 0.0f;
    float Range = 0.0f;
    int StartTick = 0;
    bool Skillshot = false;
    bool Targeted = false;
};

inline std::vector<TrackedSpellEntry>& Entries() {
    static std::vector<TrackedSpellEntry> entries;
    return entries;
}

inline void Initialize() {
    Entries().clear();
}

inline void Reset() {
    Entries().clear();
}

inline void Update() {
    auto& out = Entries();
    out.clear();

    const int now = Game::TickCount();
    for (const auto& hero : ObjectManager::Heroes()) {
        if (!hero.IsValid() || hero.IsDead()) {
            continue;
        }

        const auto cast = hero.Ref().GetActiveSpellCast();
        if (!cast.IsValid()) {
            continue;
        }

        TrackedSpellEntry entry = {};
        entry.CasterNetworkId = hero.NetworkId();
        entry.TargetNetworkId = cast.GetTargetIndex();
        entry.Start = cast.GetStartPos();
        entry.End = cast.GetEndPos();
        entry.CastPosition = cast.GetCastPos();
        entry.Delay = cast.GetCastDelay();
        entry.StartTick = now;

        const int slot = cast.GetSlot();
        entry.Slot = (slot >= 0 && slot <= static_cast<int>(SpellSlot::R))
            ? static_cast<SpellSlot>(slot)
            : SpellSlot::Unknown;

        char nameBuf[128] = {};
        if (cast.ReadSpellName(nameBuf, static_cast<int>(sizeof(nameBuf)))) {
            entry.SpellName = nameBuf;
        }

        entry.Skillshot = Detector::IsSkillshot(hero, entry.SpellName);
        entry.Targeted = Detector::IsTargeted(hero, entry.SpellName);
        entry.Radius = Detector::ResolveRadius(hero, entry.SpellName);
        entry.Range = Detector::ResolveRange(hero, entry.SpellName);
        if (_stricmp(entry.SpellName.c_str(), "ZiggsR") == 0) {
            entry.Radius = Skillshots::_ZiggsR::RadiusForTarget(hero, entry.CastPosition);
        }
        out.push_back(std::move(entry));
    }

    for (const auto& missile : ObjectManager::Missiles()) {
        if (!missile.IsValid()) {
            continue;
        }

        for (auto& entry : out) {
            if (entry.MissileNetworkId == 0 &&
                entry.CasterNetworkId == missile.CasterNetworkId() &&
                !_stricmp(entry.SpellName.c_str(), missile.Name().c_str())) {
                entry.MissileNetworkId = missile.NetworkId();
                break;
            }
        }
    }
}

inline const TrackedSpellEntry* FindByCaster(int networkId) {
    const auto& entries = Entries();
    auto it = std::find_if(entries.begin(), entries.end(), [&](const TrackedSpellEntry& entry) {
        return entry.CasterNetworkId == networkId;
    });
    return it != entries.end() ? &(*it) : nullptr;
}

inline const TrackedSpellEntry* FindByMissile(int networkId) {
    const auto& entries = Entries();
    auto it = std::find_if(entries.begin(), entries.end(), [&](const TrackedSpellEntry& entry) {
        return entry.MissileNetworkId == networkId;
    });
    return it != entries.end() ? &(*it) : nullptr;
}

} // namespace SDK::SpellTracker
