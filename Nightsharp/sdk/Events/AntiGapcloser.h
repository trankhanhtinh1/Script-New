#pragma once

#include "../Data/GapcloserData.h"
#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../Core/Objects.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace SDK {

class AntiGapcloser {
public:
    using GapcloserType = SDK::GapcloserType;

    struct GapcloserArgs {
        AIHeroClient Sender = {};
        AIBaseClient Target = {};
        Vector3 StartPosition = {};
        Vector3 EndPosition = {};
        GapcloserType Type = GapcloserType::Skillshot;
        SpellSlot Slot = SpellSlot::Unknown;
        std::string SpellName = {};
        bool IsDirectedToPlayer = false;
        int TickCount = 0;

        bool IsValid() const {
            return Sender.IsValid();
        }
    };

    using Handler = void(*)(const AIHeroClient&, const GapcloserArgs&);

    static void Initialize() {
        if (s_initialized) {
            return;
        }
        s_initialized = true;
    }

    static bool AddOnGapcloser(Handler handler) {
        return handler && s_handlers.push_back(handler);
    }

    static bool OnGapcloser(Handler handler) {
        return AddOnGapcloser(handler);
    }

    static bool AddOnEnemyGapcloser(Handler handler) {
        return AddOnGapcloser(handler);
    }

    static bool OnEnemyGapcloser(Handler handler) {
        return AddOnEnemyGapcloser(handler);
    }

    static const std::vector<GapcloserArgs>& ActiveSpells() {
        static const std::vector<GapcloserArgs> s_empty = {};
        return s_activeSpells ? *s_activeSpells : s_empty;
    }

    static void Update() {
        Initialize();
        if (!EnsureStorage()) {
            return;
        }

        static int s_lastUpdateTick = 0;
        const int now = Game::TickCount();
        if (now - s_lastUpdateTick < 50) {
            return;
        }
        s_lastUpdateTick = now;

        PruneExpired();
        DetectNewGapclosers();

        if (s_handlers.empty()) {
            return;
        }

        const auto player = ObjectManager::Player();
        for (const auto& gapcloser : *s_activeSpells) {
            if (!gapcloser.Sender.IsValidTarget()) {
                continue;
            }
            if (gapcloser.Type != GapcloserType::Targeted &&
                (!player.IsValid() || player.Distance(gapcloser.Sender) > 500.0f)) {
                continue;
            }

            for (const auto& handler : s_handlers) {
                if (handler) {
                    handler(gapcloser.Sender, gapcloser);
                }
            }
        }
    }

    static void Reset() {
        s_handlers.clear();
        if (s_activeSpells) {
            s_activeSpells->clear();
        }
        if (s_seen) {
            s_seen->clear();
        }
        s_initialized = false;
    }

private:
    struct SeenState {
        uintptr_t CastAddress = 0;
        std::string SpellName = {};
    };

    static std::string ToLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(::tolower(c));
        });
        return value;
    }

    static bool EqualsInsensitive(const std::string& lhs, const char* rhs) {
        return rhs && lstrcmpiA(lhs.c_str(), rhs) == 0;
    }

    static const Generated::GapcloserData::GapcloserEntry* FindEntry(const AIHeroClient& hero, const std::string& spellNameLower) {
        const std::string champion = hero.CharacterName();
        if (champion.empty() || spellNameLower.empty()) {
            return nullptr;
        }

        for (const auto& entry : Generated::GapcloserData::kGapcloserEntries) {
            if (!EqualsInsensitive(champion, entry.ChampionName)) {
                continue;
            }
            if (spellNameLower == entry.SpellName) {
                return &entry;
            }
        }

        return nullptr;
    }

    static AIBaseClient ResolveTarget(const CoreSpellCastInfo::CastRef& cast) {
        const int targetIndex = cast.GetTargetIndex();
        return AIBaseClient(CoreAPI::Objects::FindByIndex(targetIndex));
    }

    static Vector3 ResolveStartPosition(const AIHeroClient& hero, const CoreSpellCastInfo::CastRef& cast) {
        Vector3 start = cast.GetStartPos();
        if (start.IsZero()) {
            start = hero.PreviousPosition();
        }
        if (start.IsZero()) {
            start = hero.Position();
        }
        return start;
    }

    static Vector3 ResolveEndPosition(const AIHeroClient& hero, const CoreSpellCastInfo::CastRef& cast) {
        Vector3 end = cast.GetEndPos();
        if (end.IsZero()) {
            end = hero.PathEnd();
        }
        if (end.IsZero()) {
            end = hero.OrderPosition();
        }
        if (end.IsZero()) {
            end = hero.Position();
        }
        return end;
    }

    static bool IsFacingPlayer(const AIHeroClient& hero, const AIHeroClient& player) {
        if (!hero.IsValid() || !player.IsValid()) {
            return false;
        }

        const Vector3 facing = hero.Direction().Normalized2D();
        const Vector3 toPlayer = (player.Position() - hero.Position()).Normalized2D();
        return facing.Dot(toPlayer) > 0.5f;
    }

    static bool IsDirectedToPlayer(const AIHeroClient& hero,
                                   const AIHeroClient& player,
                                   const AIBaseClient& target,
                                   const Vector3& start,
                                   const Vector3& end,
                                   const Generated::GapcloserData::GapcloserEntry& entry) {
        if (!player.IsValid()) {
            return false;
        }

        if (target.IsValid() && target.NetworkId() == player.NetworkId()) {
            return true;
        }

        const float startDistance = player.Distance(start);
        const float endDistance = player.Distance(end);
        if (!entry.Invert && endDistance < startDistance) {
            return true;
        }
        if (entry.Invert && endDistance > startDistance) {
            return true;
        }

        return IsFacingPlayer(hero, player);
    }

    static void PruneExpired() {
        const int now = Game::TickCount();
        s_activeSpells->erase(
            std::remove_if(
                s_activeSpells->begin(),
                s_activeSpells->end(),
                [now](const GapcloserArgs& entry) {
                    return !entry.Sender.IsValid() || entry.Sender.IsDead() || now > (entry.TickCount + 900);
                }),
            s_activeSpells->end());
    }

    static void DetectNewGapclosers() {
        const auto player = ObjectManager::Player();

        for (const auto& hero : ObjectManager::EnemyHeroes()) {
            const int netId = hero.NetworkId();
            if (!hero.IsValid() || hero.IsDead()) {
                s_seen->erase(netId);
                continue;
            }

            const auto cast = hero.Ref().GetActiveSpellCast();
            if (!cast.IsValid()) {
                s_seen->erase(netId);
                continue;
            }

            char spellNameBuffer[128] = {};
            if (!cast.ReadSpellName(spellNameBuffer, static_cast<int>(sizeof(spellNameBuffer)))) {
                continue;
            }

            const std::string spellNameLower = ToLower(std::string(spellNameBuffer));
            const auto* entry = FindEntry(hero, spellNameLower);
            if (!entry) {
                continue;
            }

            auto& seen = (*s_seen)[netId];
            if (seen.CastAddress == cast.address && seen.SpellName == spellNameLower) {
                continue;
            }

            GapcloserArgs args = {};
            args.Sender = hero;
            args.Target = ResolveTarget(cast);
            args.StartPosition = ResolveStartPosition(hero, cast);
            args.EndPosition = ResolveEndPosition(hero, cast);
            args.Type = entry->SkillType;
            args.Slot = entry->Slot;
            args.SpellName = spellNameLower;
            args.TickCount = Game::TickCount();
            args.IsDirectedToPlayer = IsDirectedToPlayer(hero, player, args.Target, args.StartPosition, args.EndPosition, *entry);

            s_activeSpells->push_back(args);
            seen.CastAddress = cast.address;
            seen.SpellName = spellNameLower;
        }
    }

    static bool EnsureStorage() {
        if (!s_activeSpells) {
            s_activeSpells = new(std::nothrow) std::vector<GapcloserArgs>();
        }
        if (!s_seen) {
            s_seen = new(std::nothrow) std::unordered_map<int, SeenState>();
        }
        return s_activeSpells && s_seen;
    }

    static inline bool s_initialized = false;
    static inline MenuUI::FixedList<Handler, 64> s_handlers = {};
    static inline std::vector<GapcloserArgs>* s_activeSpells = nullptr;
    static inline std::unordered_map<int, SeenState>* s_seen = nullptr;
};

} // namespace SDK
