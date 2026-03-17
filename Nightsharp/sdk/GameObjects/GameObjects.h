#pragma once

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "ObjectManager.h"
#include "GameObject.h"
#include "Game.h"
#include "LazyObjectList.h"
#include "sdk/Utils/FrameRefresh.h"
#include "sdk/Utils/JungleUtils.h"
#include "sdk/Utils/MinionUtils.h"
#include "sdk/Utils/DebugConsole.h"
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <unordered_map>

// ============================================================================
// GameObjects — Cached & categorized object lists (updated each frame)
// Reference: EnsoulSharp.SDK/Core/GameObjects.cs
// ============================================================================

namespace SDK {
	namespace GameObjects {
		inline GameObject Player;
		inline void BuildExtraMinionCategories();
		inline void BuildStructureCategories();

		struct RangeQueryKey {
			uint32_t RangeBits = 0;
			uint32_t XBits = 0;
			uint32_t YBits = 0;
			uint32_t ZBits = 0;

			bool operator==(const RangeQueryKey& other) const {
				return RangeBits == other.RangeBits &&
					XBits == other.XBits &&
					YBits == other.YBits &&
					ZBits == other.ZBits;
			}
		};

		struct RangeQueryKeyHash {
			size_t operator()(const RangeQueryKey& key) const {
				size_t hash = (size_t)key.RangeBits;
				hash ^= ((size_t)key.XBits << 1);
				hash ^= ((size_t)key.YBits << 7);
				hash ^= ((size_t)key.ZBits << 13);
				return hash;
			}
		};

		inline int s_rangeQueryCacheFrame = -1;
		inline std::unordered_map<RangeQueryKey, std::vector<GameObject>, RangeQueryKeyHash> s_enemyHeroRangeCache;
		inline std::unordered_map<RangeQueryKey, std::vector<GameObject>, RangeQueryKeyHash> s_allyHeroRangeCache;
		inline std::unordered_map<RangeQueryKey, std::vector<GameObject>, RangeQueryKeyHash> s_enemyMinionRangeCache;
		inline std::unordered_map<RangeQueryKey, std::vector<GameObject>, RangeQueryKeyHash> s_allyMinionRangeCache;
		inline std::unordered_map<RangeQueryKey, std::vector<GameObject>, RangeQueryKeyHash> s_jungleRangeCache;
		inline FrameRefresh s_extraMinionCategoriesRefresh;
		inline FrameRefresh s_structureCategoriesRefresh;

		inline uint32_t FloatBits(float value) {
			uint32_t bits = 0;
			static_assert(sizeof(bits) == sizeof(value), "Unexpected float size");
			std::memcpy(&bits, &value, sizeof(bits));
			return bits;
		}

		inline RangeQueryKey MakeRangeQueryKey(float range, const Vec3& origin) {
			RangeQueryKey key;
			key.RangeBits = FloatBits(range);
			key.XBits = FloatBits(origin.x);
			key.YBits = FloatBits(origin.y);
			key.ZBits = FloatBits(origin.z);
			return key;
		}

		inline void ResetRangeQueryCachesIfNeeded() {
			const int currentFrame = Game::GetScriptFrameId();
			if (s_rangeQueryCacheFrame == currentFrame) {
				return;
			}

			s_rangeQueryCacheFrame = currentFrame;
			s_enemyHeroRangeCache.clear();
			s_allyHeroRangeCache.clear();
			s_enemyMinionRangeCache.clear();
			s_allyMinionRangeCache.clear();
			s_jungleRangeCache.clear();
		}

		template <typename FilterFn>
		inline const std::vector<GameObject>& GetCachedRangeQuery(
			std::unordered_map<RangeQueryKey, std::vector<GameObject>, RangeQueryKeyHash>& cache,
			const std::vector<GameObject>& source,
			float range,
			const Vec3& from,
			FilterFn&& filter) {
			ResetRangeQueryCachesIfNeeded();

			const Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
			const RangeQueryKey key = MakeRangeQueryKey(range, origin);

			auto it = cache.find(key);
			if (it != cache.end()) {
				return it->second;
			}

			std::vector<GameObject> result;
			for (const auto& obj : source) {
				if (filter(obj, origin, range)) {
					result.push_back(obj);
				}
			}

			return cache.emplace(key, std::move(result)).first->second;
		}

		// ====================================================================
		// Cached lists — updated each frame via Update()
		// ====================================================================
		inline std::vector<GameObject> AllHeroes;
		inline std::vector<GameObject> AllyHeroes;
		inline std::vector<GameObject> EnemyHeroes;

		inline std::vector<GameObject> AllMinions;
		inline std::vector<GameObject> AllyMinions;
		inline std::vector<GameObject> EnemyMinions;
		inline std::vector<GameObject> JungleMinions;
		inline std::vector<GameObject> RawMinionObjects;

		inline std::vector<GameObject> AllTurrets;
		inline std::vector<GameObject> AllyTurrets;
		inline std::vector<GameObject> EnemyTurrets;

		// Extra categorized lists
		inline LazyObjectList AllWards(&BuildExtraMinionCategories);
		inline LazyObjectList AllyWards(&BuildExtraMinionCategories);
		inline LazyObjectList EnemyWards(&BuildExtraMinionCategories);
		inline LazyObjectList JunglePlants(&BuildExtraMinionCategories);
		inline LazyObjectList JungleLarge(&BuildExtraMinionCategories);       // Baron, Dragon, Rift Herald, Blue/Red buff
		inline LazyObjectList JungleSmall(&BuildExtraMinionCategories);       // Smaller jungle mobs
		inline LazyObjectList JungleLegendary(&BuildExtraMinionCategories);   // Baron, Dragon, Rift Herald
		inline LazyObjectList AllyInhibitors(&BuildStructureCategories);
		inline LazyObjectList EnemyInhibitors(&BuildStructureCategories);
		inline LazyObjectList AllyNexus(&BuildStructureCategories);
		inline LazyObjectList EnemyNexus(&BuildStructureCategories);
		inline LazyObjectList AzirSoldiers(&BuildExtraMinionCategories);
		inline LazyObjectList Pets(&BuildExtraMinionCategories);
		inline LazyObjectList ParticleEmitters(&BuildStructureCategories);  // EffectEmitter objects (Jarvan flag, Thresh lantern, etc.)

		// Backwards compatibility alias
		inline std::vector<GameObject>& Turrets = AllTurrets;
		inline std::vector<GameObject>& Jungle = JungleMinions;

		inline void BuildExtraMinionCategories() {
			s_extraMinionCategoriesRefresh.Run([&]() {
				JungleLarge.clear();
				JungleSmall.clear();
				JungleLegendary.clear();
				AllWards.clear();
				AllyWards.clear();
				EnemyWards.clear();
				JunglePlants.clear();
				Pets.clear();
				AzirSoldiers.clear();
				AllWards.reserve(RawMinionObjects.size());
				AllyWards.reserve(RawMinionObjects.size() / 2 + 1);
				EnemyWards.reserve(RawMinionObjects.size() / 2 + 1);
				JunglePlants.reserve(32);
				Pets.reserve(64);
				AzirSoldiers.reserve(8);
				JungleLarge.reserve(32);
				JungleSmall.reserve(64);
				JungleLegendary.reserve(8);

				const GameObjectTeam myTeam = Player.GetTeam();
				const std::string championName = Player.GetChampionName();
				const bool isAzir = (_stricmp(championName.c_str(), "Azir") == 0);

				for (auto& obj : RawMinionObjects) {
					if (!obj.IsValid() || !obj.IsAlive()) continue;

					const GameObjectTeam team = obj.GetTeam();
					std::string name = obj.GetName();
					if (name.empty()) {
						name = obj.GetChampionName();
					}

					if (obj.IsWard()) {
						AllWards.push_back(obj);
						if (team == myTeam)
							AllyWards.push_back(obj);
						else
							EnemyWards.push_back(obj);
						continue;
					}

					if (obj.IsPlant()) {
						JunglePlants.push_back(obj);
						continue;
					}

					if (isAzir && team == myTeam &&
						name.find("AzirSoldier") != std::string::npos) {
						AzirSoldiers.push_back(obj);
						continue;
					}

					if (obj.IsPet()) {
						Pets.push_back(obj);
						continue;
					}

					if (team != GameObjectTeam::Neutral) {
						continue;
					}

					float maxHP = obj.GetMaxHealth();
					if (maxHP <= 1.0f) continue;

					std::string lowerName = name;
					std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) {
						return (char)std::tolower(c);
						});

					std::string characterName = obj.GetChampionName();
					std::transform(characterName.begin(), characterName.end(), characterName.begin(), [](unsigned char c) {
						return (char)std::tolower(c);
						});

					if (JungleUtils::IsJunglePlantName(lowerName) ||
						JungleUtils::IsJunglePlantName(characterName)) {
						JunglePlants.push_back(obj);
						continue;
					}

					JungleType jungleType = JungleUtils::GetJungleType(lowerName);
					if (jungleType == JungleType::Unknown && !characterName.empty()) {
						jungleType = JungleUtils::GetJungleType(characterName);
					}

					const bool jungleByFlag = obj.IsJungleMonster();
					const bool jungleByKnownName =
						JungleUtils::IsKnownJungleMonsterName(lowerName) ||
						JungleUtils::IsKnownJungleMonsterName(characterName);

					if (jungleType == JungleType::Unknown && !jungleByFlag && !jungleByKnownName) {
						continue;
					}

					if (jungleType == JungleType::Legendary || jungleType == JungleType::Epic) {
						JungleLegendary.push_back(obj);
						JungleLarge.push_back(obj);
					}
					else if (jungleType == JungleType::Large) {
						JungleLarge.push_back(obj);
					}
					else if (jungleType == JungleType::Small) {
						JungleSmall.push_back(obj);
					}
					else if (maxHP > 900.0f) {
						JungleLarge.push_back(obj);
					}
					else {
						JungleSmall.push_back(obj);
					}
				}
			});
		}

		inline void BuildStructureCategories() {
			s_structureCategoriesRefresh.Run([&]() {
				AllyInhibitors.clear();
				EnemyInhibitors.clear();
				AllyNexus.clear();
				EnemyNexus.clear();
				ParticleEmitters.clear();
				AllyInhibitors.reserve(4);
				EnemyInhibitors.reserve(4);
				AllyNexus.reserve(1);
				EnemyNexus.reserve(1);
				ParticleEmitters.reserve(128);

				const GameObjectTeam myTeam = Player.GetTeam();
				ObjectManager::ForEach([&](GameObject& obj) {
					std::string name = obj.GetName();
					if (name.empty()) return;

					GameObjectTeam team = obj.GetTeam();
					if (name.find("_buf_") != std::string::npos ||
						name.find("_tar_") != std::string::npos ||
						name.find("_mis_") != std::string::npos ||
						name.find("Particle") != std::string::npos ||
						name.find("particle") != std::string::npos ||
						name.find("global_ss_") != std::string::npos ||
						name.find("Perks_") != std::string::npos) {
						ParticleEmitters.push_back(obj);
					}

					if (!obj.IsAlive()) return;

					if (name.find("Barracks_T") != std::string::npos) {
						if (team == myTeam)
							AllyInhibitors.push_back(obj);
						else
							EnemyInhibitors.push_back(obj);
						return;
					}

					if (name.find("HQ_T") != std::string::npos) {
						if (team == myTeam)
							AllyNexus.push_back(obj);
						else
							EnemyNexus.push_back(obj);
					}
					});
			});
		}

		// ====================================================================
		// Update — Call once per frame from render hook
		// ====================================================================
		inline void Update() {
			// Update local player
			Player = ObjectManager::GetLocalPlayer();
			if (!Player.IsValid()) return;

			GameObjectTeam myTeam = Player.GetTeam();

			// ---- Heroes ----
			ObjectManager::FillHeroes(AllHeroes);
			AllyHeroes.clear();
			EnemyHeroes.clear();
			AllyHeroes.reserve(AllHeroes.size());
			EnemyHeroes.reserve(AllHeroes.size());

			for (auto& hero : AllHeroes) {
				if (!hero.IsValid()) continue;
				if (hero.GetTeam() == myTeam)
					AllyHeroes.push_back(hero);
				else
					EnemyHeroes.push_back(hero);
			}

			// ---- Turrets (from TurretManager directly) ----
			AllTurrets.clear();
			AllyTurrets.clear();
			EnemyTurrets.clear();
			{
				uintptr_t tmgr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::TurretManager);
				if (Globals::IsValidPtr(tmgr)) {
					uintptr_t tlist = Globals::Read<uintptr_t>(tmgr + 0x8);
					int tcount = Globals::Read<int>(tmgr + 0x10);
					if (Globals::IsValidPtr(tlist) && tcount > 0 && tcount <= 30) {
						uintptr_t addrs[30] = {};
						int n = Globals::ReadPtrArray(tlist, tcount, addrs, 30);
						for (int i = 0; i < n; i++) {
							if (Globals::IsValidPtr(addrs[i])) {
								GameObject t(addrs[i]);
								if (t.IsAlive()) {
									AllTurrets.push_back(t);
									if (t.GetTeam() == myTeam)
										AllyTurrets.push_back(t);
									else
										EnemyTurrets.push_back(t);
								}
							}
						}
					}
				}
			}

			// ---- Minions + Jungle + Wards + Plants + Pets (from MinionManager) ----
			AllMinions.clear();
			AllyMinions.clear();
			EnemyMinions.clear();
			JungleMinions.clear();
			ObjectManager::FillMinions(RawMinionObjects);
			AllMinions.reserve(RawMinionObjects.size());
			AllyMinions.reserve(RawMinionObjects.size());
			EnemyMinions.reserve(RawMinionObjects.size());
			JungleMinions.reserve(RawMinionObjects.size());

            for (auto& obj : RawMinionObjects) {
                if (!obj.IsValid() || !obj.IsAlive()) continue;

                GameObjectTeam team = obj.GetTeam();
                if (team == GameObjectTeam::Neutral) {
                    // --- Classify Neutral objects: Plant vs Jungle Monster ---
                    // Uses RuntimeAPI for accurate classification instead of raw offsets.
                    const std::string objName = obj.GetName();
                    const std::string champName = obj.GetChampionName();

                    // Normalize to lowercase ONCE for all comparisons
                    const std::string lowerObjName = JungleUtils::ToLower(objName);
                    const std::string lowerChampName = JungleUtils::ToLower(champName);
                    const std::string lowerName = !lowerObjName.empty() ? lowerObjName : lowerChampName;

                    // Comprehensive plant detection (RuntimeAPI + name + HP):
                    const float maxHP = obj.GetMaxHealth();
                    const bool isPlant =
                        obj.IsPlant() ||
                        JungleUtils::IsJunglePlantName(lowerObjName) ||
                        JungleUtils::IsJunglePlantName(lowerChampName) ||
                        (maxHP > 0.0f && maxHP <= 6.0f);

                    if (isPlant) {
                        continue; // Plants go to JunglePlants via BuildExtraMinionCategories
                    }

                    // Jungle monster detection using RuntimeAPI (native game function)
                    // + name database as fallback. No offset-based MinionType used.
                    const bool isRuntimeJungle = obj.IsJungleMonster(); // RuntimeAPI native call
                    const bool isKnownJungle =
                        !lowerName.empty() && JungleUtils::IsKnownJungleMonsterName(lowerName);

                    if (maxHP > 6.0f && (isRuntimeJungle || isKnownJungle)) {
                        JungleMinions.push_back(obj);
                    }
                    continue;
                }

                if (obj.IsHero() || obj.IsTurret()) {
                    continue;
                }

                float maxHP = obj.GetMaxHealth();
                if (maxHP <= 0.0f || maxHP >= 10000.0f) {
                    continue;
                }

                std::string minionName = obj.GetChampionName();
                if (minionName.empty()) {
                    minionName = obj.GetName();
                }

                if (!obj.IsMinion() && !MinionUtils::IsMinion(minionName)) {
                    continue;
                }

                AllMinions.push_back(obj);
                if (team == myTeam)
                    AllyMinions.push_back(obj);
                else
                    EnemyMinions.push_back(obj);
            }

			// Debug: log minion classification summary (throttled)
			{
				static ULONGLONG s_lastMinionLog = 0;
				ULONGLONG now = GetTickCount64();
				if (now - s_lastMinionLog > 2000) {
					s_lastMinionLog = now;
					DebugConsole::LogTagged("MinionClass",
						"raw=%llu all=%llu ally=%llu enemy=%llu jungle=%llu team=%d",
						(unsigned long long)RawMinionObjects.size(),
						(unsigned long long)AllMinions.size(),
						(unsigned long long)AllyMinions.size(),
						(unsigned long long)EnemyMinions.size(),
						(unsigned long long)JungleMinions.size(),
						(int)myTeam);

					auto logSample = [&](const char* tag, const std::vector<GameObject>& list) {
						if (list.empty()) return;
						const auto& m = list.front();
						std::string name = m.GetName();
						if (name.empty()) name = "(noname)";
						std::string champ = m.GetChampionName();
						if (champ.empty()) champ = "(nochamp)";
						Vec3 pos = m.GetPosition();
						DebugConsole::LogTagged("MinionSample",
							"%s name=%s champ=%s team=%d vis=%d hp=%.0f/%.0f pos=(%.0f,%.0f)",
							tag, name.c_str(), champ.c_str(), (int)m.GetTeam(),
							m.IsVisible() ? 1 : 0, m.GetHealth(), m.GetMaxHealth(),
							pos.x, pos.z);
					};

					logSample("enemy", EnemyMinions);
					logSample("ally", AllyMinions);
					logSample("raw", RawMinionObjects);
				}
			}

    }

		// ====================================================================
		// Utility — Get objects in range
		// ====================================================================
		inline const std::vector<GameObject>& GetEnemyHeroesInRange(float range, const Vec3& from = Vec3()) {
			return GetCachedRangeQuery(
				s_enemyHeroRangeCache,
				EnemyHeroes,
				range,
				from,
				[](const GameObject& hero, const Vec3& origin, float queryRange) {
					return hero.IsAlive() &&
						hero.IsVisible() &&
						hero.GetPosition().Distance2D(origin) <= queryRange;
				});
		}

		inline const std::vector<GameObject>& GetAllyHeroesInRange(float range, const Vec3& from = Vec3()) {
			return GetCachedRangeQuery(
				s_allyHeroRangeCache,
				AllyHeroes,
				range,
				from,
				[](const GameObject& hero, const Vec3& origin, float queryRange) {
					return hero.IsAlive() &&
						hero.GetPosition().Distance2D(origin) <= queryRange;
				});
		}

		inline const std::vector<GameObject>& GetEnemyMinionsInRange(float range, const Vec3& from = Vec3()) {
			return GetCachedRangeQuery(
				s_enemyMinionRangeCache,
				EnemyMinions,
				range,
				from,
				[](const GameObject& minion, const Vec3& origin, float queryRange) {
					return minion.IsAlive() &&
						minion.IsVisible() &&
						minion.GetPosition().Distance2D(origin) <= queryRange;
				});
		}

		// EnsoulSharp-style helper alias: enemy lane minions in range.
		inline const std::vector<GameObject>& GetMinions(const Vec3& from, float range) {
			return GetEnemyMinionsInRange(range, from);
		}

		inline const std::vector<GameObject>& GetAllyMinionsInRange(float range, const Vec3& from = Vec3()) {
			return GetCachedRangeQuery(
				s_allyMinionRangeCache,
				AllyMinions,
				range,
				from,
				[](const GameObject& minion, const Vec3& origin, float queryRange) {
					return minion.IsAlive() &&
						minion.GetPosition().Distance2D(origin) <= queryRange;
				});
		}

		inline const std::vector<GameObject>& GetJungleMonstersInRange(float range, const Vec3& from = Vec3()) {
			return GetCachedRangeQuery(
				s_jungleRangeCache,
				JungleMinions,
				range,
				from,
				[](const GameObject& mob, const Vec3& origin, float queryRange) {
					return mob.IsAlive() &&
						mob.IsVisible() &&
						mob.GetPosition().Distance2D(origin) <= queryRange;
				});
		}

		// EnsoulSharp-style helper alias.
		inline const std::vector<GameObject>& GetJungles(const Vec3& from, float range) {
			return GetJungleMonstersInRange(range, from);
		}

		// Turret in range helpers
		inline GameObject GetClosestAllyTurret(const Vec3& from = Vec3()) {
			Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
			GameObject closest;
			float minDist = FLT_MAX;
			for (auto& turret : AllyTurrets) {
				float dist = turret.GetPosition().Distance2D(origin);
				if (dist < minDist) {
					minDist = dist;
					closest = turret;
				}
			}
			return closest;
		}

		inline GameObject GetClosestEnemyTurret(const Vec3& from = Vec3()) {
			Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
			GameObject closest;
			float minDist = FLT_MAX;
			for (auto& turret : EnemyTurrets) {
				float dist = turret.GetPosition().Distance2D(origin);
				if (dist < minDist) {
					minDist = dist;
					closest = turret;
				}
			}
			return closest;
		}

		inline bool IsUnderAllyTurret(const Vec3& pos) {
			for (auto& turret : AllyTurrets) {
				if (turret.GetPosition().Distance2D(pos) <= 875.0f) // turret range
					return true;
			}
			return false;
		}

		inline bool IsUnderEnemyTurret(const Vec3& pos) {
			for (auto& turret : EnemyTurrets) {
				if (turret.GetPosition().Distance2D(pos) <= 875.0f)
					return true;
			}
			return false;
		}

		// ====================================================================
		// Count helpers
		// ====================================================================
		inline int CountEnemyHeroesInRange(float range, const Vec3& from = Vec3()) {
			return (int)GetEnemyHeroesInRange(range, from).size();
		}

		inline int CountAllyHeroesInRange(float range, const Vec3& from = Vec3()) {
			return (int)GetAllyHeroesInRange(range, from).size();
		}

		inline int CountEnemyMinionsInRange(float range, const Vec3& from = Vec3()) {
			return (int)GetEnemyMinionsInRange(range, from).size();
		}

		inline int CountAllyMinionsInRange(float range, const Vec3& from = Vec3()) {
			return (int)GetAllyMinionsInRange(range, from).size();
		}

		// ====================================================================
		// Ward helpers
		// ====================================================================
		inline std::vector<GameObject> GetWardsInRange(float range, const Vec3& from = Vec3()) {
			std::vector<GameObject> result;
			Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
			for (auto& ward : AllWards) {
				if (!ward.IsAlive()) continue;
				if (ward.GetPosition().Distance2D(origin) <= range)
					result.push_back(ward);
			}
			return result;
		}

		inline std::vector<GameObject> GetAllyWardsInRange(float range, const Vec3& from = Vec3()) {
			std::vector<GameObject> result;
			Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
			for (auto& ward : AllyWards) {
				if (!ward.IsAlive()) continue;
				if (ward.GetPosition().Distance2D(origin) <= range)
					result.push_back(ward);
			}
			return result;
		}

		inline std::vector<GameObject> GetEnemyWardsInRange(float range, const Vec3& from = Vec3()) {
			std::vector<GameObject> result;
			Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
			for (auto& ward : EnemyWards) {
				if (!ward.IsAlive()) continue;
				if (ward.GetPosition().Distance2D(origin) <= range)
					result.push_back(ward);
			}
			return result;
		}

		// ====================================================================
		// Plant helpers
		// ====================================================================
		inline std::vector<GameObject> GetJunglePlantsInRange(float range, const Vec3& from = Vec3()) {
			std::vector<GameObject> result;
			Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
			for (auto& plant : JunglePlants) {
				if (!plant.IsAlive()) continue;
				if (plant.GetPosition().Distance2D(origin) <= range)
					result.push_back(plant);
			}
			return result;
		}

		// ====================================================================
		// Structure helpers
		// ====================================================================
		inline GameObject GetClosestEnemyInhibitor(const Vec3& from = Vec3()) {
			Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
			GameObject closest;
			float minDist = FLT_MAX;
			for (auto& inhib : EnemyInhibitors) {
				float dist = inhib.GetPosition().Distance2D(origin);
				if (dist < minDist) { minDist = dist; closest = inhib; }
			}
			return closest;
		}

		inline GameObject GetClosestEnemyNexus(const Vec3& from = Vec3()) {
			Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
			if (!EnemyNexus.empty()) return EnemyNexus[0];
			return GameObject();
		}

		// ====================================================================
		// Azir Soldier helpers
		// ====================================================================
		inline bool HasAzirSoldierNear(const GameObject& target, float radius = 350.0f) {
			for (auto& soldier : AzirSoldiers) {
				if (soldier.GetPosition().Distance2D(target.GetPosition()) <= radius)
					return true;
			}
			return false;
		}

		inline int CountAzirSoldiersInRange(float range, const Vec3& from = Vec3()) {
			int count = 0;
			Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
			for (auto& soldier : AzirSoldiers) {
				if (soldier.GetPosition().Distance2D(origin) <= range) count++;
			}
			return count;
		}

		// ====================================================================
		// Jungle subcategory helpers
		// ====================================================================
		inline std::vector<GameObject> GetJungleLargeInRange(float range, const Vec3& from = Vec3()) {
			std::vector<GameObject> result;
			Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
			for (auto& mob : JungleLarge) {
				if (!mob.IsAlive() || !mob.IsVisible()) continue;
				if (mob.GetPosition().Distance2D(origin) <= range)
					result.push_back(mob);
			}
			return result;
		}

		inline std::vector<GameObject> GetJungleLegendaryInRange(float range, const Vec3& from = Vec3()) {
			std::vector<GameObject> result;
			Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
			for (auto& mob : JungleLegendary) {
				if (!mob.IsAlive() || !mob.IsVisible()) continue;
				if (mob.GetPosition().Distance2D(origin) <= range)
					result.push_back(mob);
			}
			return result;
		}

		// ====================================================================
		// ParticleEmitter helpers
		// ====================================================================

		/// Find a particle emitter by name substring
		inline GameObject FindParticleEmitter(const std::string& nameSubstr) {
			for (auto& emitter : ParticleEmitters) {
				std::string name = emitter.GetName();
				if (name.find(nameSubstr) != std::string::npos)
					return emitter;
			}
			return GameObject();
		}

		/// Get all particle emitters in range
		inline std::vector<GameObject> GetParticleEmittersInRange(float range, const Vec3& from = Vec3()) {
			std::vector<GameObject> result;
			Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
			for (auto& emitter : ParticleEmitters) {
				if (emitter.GetPosition().Distance2D(origin) <= range)
					result.push_back(emitter);
			}
			return result;
		}

		/// Check if a specific particle effect exists (by name substring)
		inline bool HasParticleEmitter(const std::string& nameSubstr) {
			return FindParticleEmitter(nameSubstr).IsValid();
		}

	} // namespace GameObjects
} // namespace SDK
