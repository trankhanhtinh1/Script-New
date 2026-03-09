#pragma once
#include "GameObject.h"
#include "GameObjects.h"
#include "Missile.h"
#include "SpellBook.h"
#include "Game.h"
#include <vector>
#include <functional>
#include <map>
#include <string>
#include <cstring>

// ============================================================================
// EventSystem — Poll-based game event tracking
// Reference: EnsoulSharp.SDK events (OnProcessSpellCast, OnDoCast, OnDelete)
//
// Since we can't hook game functions directly (anti-cheat), we poll each frame
// and compare state to detect events.
// ============================================================================

namespace SDK {

    // ========================================================================
    // Spell Cast Event Args
    // ========================================================================
    struct SpellCastArgs {
        GameObject    Sender;
        SpellSlotId   Slot;
        std::string   SpellName;
        Vec3          StartPos;
        Vec3          EndPos;
        int           TargetNetId;
        float         CastTime;       // Game time when cast started
        bool          IsAutoAttack;
    };

    // ========================================================================
    // Object Delete Event Args
    // ========================================================================
    struct ObjectDeleteArgs {
        uintptr_t   Address;
        int         NetId;
        Vec3        LastPosition;
    };

    // ========================================================================
    // Missile Event Args (created / destroyed)
    // ========================================================================
    struct MissileArgs {
        Missile     MissileObj;
        int         CasterNetId;
        std::string SpellName;
        Vec3        StartPos;
        Vec3        EndPos;
    };

    // ========================================================================
    // Buff Change Event Args
    // ========================================================================
    struct BuffChangeArgs {
        GameObject  Unit;
        std::string BuffName;
        BuffType    Type;
        float       StartTime;
        float       EndTime;
        int         Stacks;
        bool        IsAdded; // true = added, false = removed
    };

    // ========================================================================
    // StopCast Event Args (6.2)
    // ========================================================================
    struct StopCastArgs {
        GameObject  Sender;
        SpellSlotId Slot;
        std::string SpellName;
        float       CastTime;      // when the cast started
        float       StopTime;      // when the cast was interrupted/completed
        bool        WasAutoAttack;
        bool        ForceStop;     // true = interrupted (e.g. CC), false = natural end
    };

    // ========================================================================
    // Callback types
    // ========================================================================
    using OnProcessSpellFn  = std::function<void(const SpellCastArgs&)>;
    using OnStopCastFn      = std::function<void(const StopCastArgs&)>;
    using OnObjectDeleteFn  = std::function<void(const ObjectDeleteArgs&)>;
    using OnMissileCreateFn = std::function<void(const MissileArgs&)>;
    using OnMissileDeleteFn = std::function<void(const MissileArgs&)>;
    using OnBuffChangeFn    = std::function<void(const BuffChangeArgs&)>;
    using OnGameLoadFn      = std::function<void()>;
    using OnGameUpdateFn    = std::function<void(float /*gameTime*/)>;

    // ========================================================================
    // EventSystem — Main class
    // ========================================================================
    class EventSystem {
    public:
        // ====================================================================
        // Subscriber Registration
        // ====================================================================

        static void OnProcessSpellCast(OnProcessSpellFn callback) {
            processSpellCallbacks.push_back(callback);
        }

        // 6.2: OnStopCast — fires when a hero stops casting (interrupted or completed)
        static void OnStopCast(OnStopCastFn callback) {
            stopCastCallbacks.push_back(callback);
        }

        static void OnObjectDeleted(OnObjectDeleteFn callback) {
            objectDeleteCallbacks.push_back(callback);
        }

        static void OnMissileCreated(OnMissileCreateFn callback) {
            missileCreateCallbacks.push_back(callback);
        }

        static void OnMissileDeleted(OnMissileDeleteFn callback) {
            missileDeleteCallbacks.push_back(callback);
        }

        static void OnBuffChanged(OnBuffChangeFn callback) {
            buffChangeCallbacks.push_back(callback);
        }

        // ====================================================================
        // OnGameLoad — fires once when game is fully loaded
        // (all heroes exist, game time > 0, local player valid)
        // Reference: EnsoulSharp.SDK/Core/Events/Load.cs
        // ====================================================================
        static void OnGameLoad(OnGameLoadFn callback) {
            if (gameLoaded) {
                // Already loaded — invoke immediately (like EnsoulSharp behavior)
                try { callback(); } catch (...) {}
            } else {
                gameLoadCallbacks.push_back(callback);
            }
        }

        // ====================================================================
        // OnGameUpdate — fires every frame after all tracking is done
        // Reference: EnsoulSharp Game.OnUpdate
        // ====================================================================
        static void OnGameUpdate(OnGameUpdateFn callback) {
            gameUpdateCallbacks.push_back(callback);
        }

        // ====================================================================
        // IsGameLoaded — check if game has been detected as loaded
        // ====================================================================
        static bool IsGameLoaded() { return gameLoaded; }

        // ====================================================================
        // Update — Call once per frame AFTER GameObjects::Update()
        // ====================================================================
        static void Update() {
            float now = Game::GetTime();
            if (now <= 0.0f) return;

            // --- OnLoad detection (fires once) ---
            CheckGameLoad(now);

            // --- Core event tracking ---
            TrackSpellCasts(now);  // also handles OnStopCast (6.2)
            TrackMissiles(now);
            TrackObjectDeletions(now);
            TrackBuffChanges(now);

            lastUpdateTime = now;

            // --- OnUpdate callbacks (fires every frame) ---
            for (auto& cb : gameUpdateCallbacks) {
                try { cb(now); } catch (...) {}
            }
        }

        // ====================================================================
        // Clear all callbacks (for cleanup)
        // ====================================================================
        static void ClearAll() {
            processSpellCallbacks.clear();
            stopCastCallbacks.clear();
            objectDeleteCallbacks.clear();
            missileCreateCallbacks.clear();
            missileDeleteCallbacks.clear();
            buffChangeCallbacks.clear();
            gameLoadCallbacks.clear();
            gameUpdateCallbacks.clear();
            heroSpellStates.clear();
            knownMissiles.clear();
            knownObjects.clear();
            heroBuffStates.clear();
            gameLoaded = false;
        }

    private:
        // ====================================================================
        // Callbacks storage
        // ====================================================================
        static inline std::vector<OnProcessSpellFn>  processSpellCallbacks;
        static inline std::vector<OnStopCastFn>     stopCastCallbacks;
        static inline std::vector<OnObjectDeleteFn>  objectDeleteCallbacks;
        static inline std::vector<OnMissileCreateFn> missileCreateCallbacks;
        static inline std::vector<OnMissileDeleteFn> missileDeleteCallbacks;
        static inline std::vector<OnBuffChangeFn>    buffChangeCallbacks;
        static inline std::vector<OnGameLoadFn>      gameLoadCallbacks;
        static inline std::vector<OnGameUpdateFn>    gameUpdateCallbacks;

        static inline float lastUpdateTime = 0.0f;
        static inline bool  gameLoaded = false;

        // ====================================================================
        // CheckGameLoad — detect when game is fully loaded, fire OnLoad once
        // Reference: EnsoulSharp.SDK/Core/Events/Load.cs
        //
        // Conditions: game time > 5s, local player valid, at least 2 heroes
        // exist. This mimics EnsoulSharp's EventLoad() which fires OnLoad
        // subscribers once per subscriber after game is detected as loaded.
        // ====================================================================
        static void CheckGameLoad(float now) {
            if (gameLoaded) return;

            // Game needs some time to fully initialize
            if (now < 5.0f) return;

            // Local player must exist and be valid
            auto& localPlayer = GameObjects::Player;
            if (!localPlayer.IsValid()) return;

            // At least 2 heroes must exist (player + 1 other)
            if (GameObjects::AllHeroes.size() < 2) return;

            // Game is loaded! Fire all OnLoad callbacks
            gameLoaded = true;
            for (auto& cb : gameLoadCallbacks) {
                try { cb(); } catch (...) {}
            }
            // Clear load callbacks after firing (they only fire once)
            gameLoadCallbacks.clear();
        }

        // ====================================================================
        // Spell Cast Tracking — per hero, per slot
        // ====================================================================
        struct SpellState {
            float lastCastTime;      // when the cast was last detected
            bool  wasCasting;        // was casting last frame
            int   slotIndex;         // which slot was casting
            std::string spellName;   // name of the spell being cast
            bool  wasAutoAttack;     // was it an auto-attack
        };

        // Key: (heroNetId * 16 + slotIndex)
        static inline std::map<int, SpellState> heroSpellStates;

        static void TrackSpellCasts(float now) {
            if (processSpellCallbacks.empty()) return;

            for (auto& hero : GameObjects::AllHeroes) {
                if (!hero.IsValid() || !hero.IsAlive()) continue;

                int heroNetId = hero.GetNetId();
                SpellBook sb(hero.address);
                if (!sb.IsValid()) continue;

                // Check active spell cast
                uintptr_t activeCast = sb.GetActiveSpellCast();
                if (Globals::IsValidPtr(activeCast)) {
                    // Read spell cast info
                    uintptr_t spellData = Globals::Read<uintptr_t>(activeCast + Offset::SpellCastInfo::SpellData);

                    int slotIdx = Globals::Read<int>(activeCast + Offset::SpellCastInfo::Slot);
                    if (slotIdx < 0 || slotIdx > 13) slotIdx = 0;

                    int key = heroNetId * 16 + slotIdx;
                    auto& state = heroSpellStates[key];

                    // New cast detected? (wasn't casting before OR different cast time)
                    if (!state.wasCasting) {
                        state.wasCasting = true;
                        state.lastCastTime = now;

                        bool isAA = Globals::Read<uint8_t>(activeCast + Offset::SpellCastInfo::IsAuto) != 0;
                        std::string castName;
                        if (Globals::IsValidPtr(spellData)) {
                            castName = SpellData(spellData).GetName();
                        }

                        // Store for StopCast tracking
                        state.slotIndex = slotIdx;
                        state.spellName = castName;
                        state.wasAutoAttack = isAA;

                        // Build event args
                        SpellCastArgs args;
                        args.Sender      = hero;
                        args.Slot        = (SpellSlotId)slotIdx;
                        args.StartPos    = Globals::Read<Vec3>(activeCast + Offset::SpellCastInfo::StartPos);
                        args.EndPos      = Globals::Read<Vec3>(activeCast + Offset::SpellCastInfo::EndPos);
                        args.TargetNetId = Globals::Read<int>(activeCast + Offset::SpellCastInfo::TargetIndex);
                        args.CastTime    = now;
                        args.IsAutoAttack = isAA;
                        args.SpellName   = castName;

                        // Fire callbacks
                        for (auto& cb : processSpellCallbacks) {
                            cb(args);
                        }
                    }
                } else {
                    // Not casting anymore — fire OnStopCast for any slot that was casting (6.2)
                    for (int s = 0; s < 14; s++) {
                        int key = heroNetId * 16 + s;
                        auto it = heroSpellStates.find(key);
                        if (it != heroSpellStates.end() && it->second.wasCasting) {
                            // Fire StopCast event
                            if (!stopCastCallbacks.empty()) {
                                StopCastArgs sargs;
                                sargs.Sender       = hero;
                                sargs.Slot         = (SpellSlotId)it->second.slotIndex;
                                sargs.SpellName    = it->second.spellName;
                                sargs.CastTime     = it->second.lastCastTime;
                                sargs.StopTime     = now;
                                sargs.WasAutoAttack = it->second.wasAutoAttack;
                                sargs.ForceStop    = false; // We can't easily tell if interrupted
                                for (auto& cb : stopCastCallbacks) cb(sargs);
                            }
                            it->second.wasCasting = false;
                        }
                    }
                }
            }
        }

        // ====================================================================
        // Missile Tracking — detect created and destroyed missiles
        // ====================================================================
        struct MissileState {
            uintptr_t address;
            int       netId;
            int       casterNetId;
            Vec3      startPos;
            Vec3      endPos;
            std::string spellName;
        };

        static inline std::map<int, MissileState> knownMissiles;

        static void TrackMissiles(float now) {
            if (missileCreateCallbacks.empty() && missileDeleteCallbacks.empty()) return;

            auto currentMissiles = MissileManager::GetMissiles();

            // Build set of current missile netIds
            std::map<int, Missile> currentSet;
            for (auto& m : currentMissiles) {
                if (!m.IsValid()) continue;
                int netId = m.GetNetworkId();
                if (netId != 0)
                    currentSet[netId] = m;
            }

            // Detect NEW missiles (in currentSet but not in knownMissiles)
            for (auto& [netId, missile] : currentSet) {
                if (knownMissiles.find(netId) == knownMissiles.end()) {
                    // New missile
                    MissileState ms;
                    ms.address = missile.address;
                    ms.netId = netId;
                    ms.casterNetId = missile.GetCasterNetId();
                    ms.startPos = missile.GetStartPos();
                    ms.endPos = missile.GetEndPos();
                    ms.spellName = missile.GetSpellName();
                    knownMissiles[netId] = ms;

                    if (!missileCreateCallbacks.empty()) {
                        MissileArgs args;
                        args.MissileObj = missile;
                        args.CasterNetId = ms.casterNetId;
                        args.SpellName = ms.spellName;
                        args.StartPos = ms.startPos;
                        args.EndPos = ms.endPos;
                        for (auto& cb : missileCreateCallbacks) {
                            cb(args);
                        }
                    }
                }
            }

            // Detect DELETED missiles (in knownMissiles but not in currentSet)
            std::vector<int> toRemove;
            for (auto& [netId, ms] : knownMissiles) {
                if (currentSet.find(netId) == currentSet.end()) {
                    toRemove.push_back(netId);

                    if (!missileDeleteCallbacks.empty()) {
                        MissileArgs args;
                        args.MissileObj = Missile(ms.address);
                        args.CasterNetId = ms.casterNetId;
                        args.SpellName = ms.spellName;
                        args.StartPos = ms.startPos;
                        args.EndPos = ms.endPos;
                        for (auto& cb : missileDeleteCallbacks) {
                            cb(args);
                        }
                    }
                }
            }
            for (int netId : toRemove) {
                knownMissiles.erase(netId);
            }
        }

        // ====================================================================
        // Object Deletion Tracking
        // ====================================================================
        struct ObjectState {
            uintptr_t address;
            int       netId;
            Vec3      lastPos;
        };

        static inline std::map<int, ObjectState> knownObjects;
        static inline float lastObjTrackTime = 0.0f;

        static void TrackObjectDeletions(float now) {
            if (objectDeleteCallbacks.empty()) return;

            // Throttle: only check every 200ms
            if (now - lastObjTrackTime < 0.2f) return;
            lastObjTrackTime = now;

            // Build current object netId set from all known objects
            std::map<int, uintptr_t> currentSet;
            auto addToSet = [&](std::vector<GameObject>& list) {
                for (auto& obj : list) {
                    if (!obj.IsValid()) continue;
                    int netId = obj.GetNetId();
                    if (netId != 0)
                        currentSet[netId] = obj.address;
                }
            };

            addToSet(GameObjects::AllHeroes);
            addToSet(GameObjects::AllMinions);
            addToSet(GameObjects::JungleMinions);
            addToSet(GameObjects::EnemyWards);
            addToSet(GameObjects::AllTurrets);

            // Detect deleted objects
            std::vector<int> toRemove;
            for (auto& [netId, os] : knownObjects) {
                if (currentSet.find(netId) == currentSet.end()) {
                    toRemove.push_back(netId);

                    ObjectDeleteArgs args;
                    args.Address = os.address;
                    args.NetId = os.netId;
                    args.LastPosition = os.lastPos;
                    for (auto& cb : objectDeleteCallbacks) {
                        cb(args);
                    }
                }
            }
            for (int netId : toRemove) {
                knownObjects.erase(netId);
            }

            // Update known objects
            for (auto& [netId, addr] : currentSet) {
                auto it = knownObjects.find(netId);
                if (it == knownObjects.end()) {
                    ObjectState os;
                    os.address = addr;
                    os.netId = netId;
                    GameObject obj(addr);
                    os.lastPos = obj.IsValid() ? obj.GetPosition() : Vec3();
                    knownObjects[netId] = os;
                } else {
                    it->second.address = addr;
                    GameObject obj(addr);
                    if (obj.IsValid())
                        it->second.lastPos = obj.GetPosition();
                }
            }
        }

        // ====================================================================
        // Buff Change Tracking — per hero
        // ====================================================================
        struct BuffState {
            std::string name;
            BuffType    type;
            float       startTime;
            float       endTime;
            int         stacks;
        };

        // Key: heroNetId → list of active buffs
        static inline std::map<int, std::vector<BuffState>> heroBuffStates;
        static inline float lastBuffTrackTime = 0.0f;

        static void TrackBuffChanges(float now) {
            if (buffChangeCallbacks.empty()) return;

            // Throttle: only check every 100ms
            if (now - lastBuffTrackTime < 0.1f) return;
            lastBuffTrackTime = now;

            for (auto& hero : GameObjects::AllHeroes) {
                if (!hero.IsValid()) continue;
                int heroNetId = hero.GetNetId();

                // Get current buffs
                std::vector<BuffState> currentBuffs;
                BuffManager bm(hero.address);
                bm.ForEach([&](Buff& buff) {
                    if (!buff.IsValid() || !buff.IsActive()) return;
                    BuffState bs;
                    bs.name = buff.GetName();
                    if (bs.name.empty()) return;
                    bs.type = buff.GetType();
                    bs.startTime = buff.GetStartTime();
                    bs.endTime = buff.GetEndTime();
                    bs.stacks = buff.GetStacks();
                    currentBuffs.push_back(bs);
                });

                auto& prevBuffs = heroBuffStates[heroNetId];

                // Detect new buffs (in current but not in prev)
                for (auto& cb : currentBuffs) {
                    bool found = false;
                    for (auto& pb : prevBuffs) {
                        if (pb.name == cb.name) { found = true; break; }
                    }
                    if (!found) {
                        BuffChangeArgs args;
                        args.Unit = hero;
                        args.BuffName = cb.name;
                        args.Type = cb.type;
                        args.StartTime = cb.startTime;
                        args.EndTime = cb.endTime;
                        args.Stacks = cb.stacks;
                        args.IsAdded = true;
                        for (auto& fn : buffChangeCallbacks) fn(args);
                    }
                }

                // Detect removed buffs (in prev but not in current)
                for (auto& pb : prevBuffs) {
                    bool found = false;
                    for (auto& cb : currentBuffs) {
                        if (cb.name == pb.name) { found = true; break; }
                    }
                    if (!found) {
                        BuffChangeArgs args;
                        args.Unit = hero;
                        args.BuffName = pb.name;
                        args.Type = pb.type;
                        args.StartTime = pb.startTime;
                        args.EndTime = pb.endTime;
                        args.Stacks = pb.stacks;
                        args.IsAdded = false;
                        for (auto& fn : buffChangeCallbacks) fn(args);
                    }
                }

                // Update stored state
                prevBuffs = currentBuffs;
            }
        }
    };

} // namespace SDK
