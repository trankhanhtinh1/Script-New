#pragma once

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::AIO7UP::Zed {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* ItemsMenu = nullptr;
inline Menu* OffensiveMenu = nullptr;
inline Menu* DefensiveMenu = nullptr;
inline Menu* FarmMenu = nullptr;
inline Menu* LaneFarmMenu = nullptr;
inline Menu* LastHitMenu = nullptr;
inline Menu* JungleMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* DrawingsMenu = nullptr;

inline std::vector<Spell*> SpellList;

inline Spell Q{ SpellSlot::Q, 925.0f };
inline Spell W{ SpellSlot::W, 700.0f };
inline Spell E{ SpellSlot::E, 290.0f };
inline Spell R{ SpellSlot::R, 625.0f };
inline Spell Ignite{ SpellSlot::Unknown, 600.0f };

inline bool Loaded = false;
inline Vector3 LinePos = {};
inline int ClockOn = 0;
inline int CountUlts = 0;
inline int CountDanger = 0;
inline int TickTock = 0;
inline Vector3 RPos = {};
inline int ShadowDelay = 0;
inline constexpr int DelayW = 500;
inline DWORD LastUpdateTick = 0;
inline DWORD LastKillStealTick = 0;

inline constexpr int kItemTiamat = 3077;
inline constexpr int kItemRavenousHydra = 3074;
inline constexpr int kItemTitanicHydra = 3748;
inline constexpr int kItemBladeOfTheRuinedKing = 3153;
inline constexpr int kItemYoumuus = 3142;
inline constexpr int kItemRanduins = 3143;
inline constexpr int kItemTheCollector = 6676;
inline constexpr int kItemVoltaicCyclosword = 6699;
inline constexpr int kItemProfaneHydra = 6698;
inline constexpr int kItemEclipse = 6692;
inline constexpr int kItemUmbralGlaive = 3179;
inline constexpr int kItemSerpentsFang = 6695;
inline constexpr int kItemHubris = 6697;
inline constexpr int kItemOpportunity = 6701;
inline constexpr int kItemSeryldasGrudge = 6694;
inline constexpr int kItemEdgeOfNight = 3814;

inline constexpr const char* kDangerousList[] = {
    "AhriSeduce",
    "CurseoftheSadMummy",
    "InfernalGuardian",
    "EnchantedCrystalArrow",
    "AzirR",
    "BrandWildfire",
    "CassiopeiaPetrifyingGaze",
    "DariusExecute",
    "DravenRCast",
    "EvelynnR",
    "EzrealTrueshotBarrage",
    "Terrify",
    "GalioIdolOfDurand",
    "GarenR",
    "GravesChargeShot",
    "HecarimUlt",
    "LissandraR",
    "LuxMaliceCannon",
    "UFSlash",
    "AlZaharNetherGrasp",
    "OrianaDetonateCommand",
    "LeonaSolarFlare",
    "SejuaniGlacialPrisonStart",
    "SonaCrescendo",
    "VarusR",
    "GragasR",
    "GnarR",
    "FizzMarinerDoom",
    "SyndraR"
};

[[maybe_unused]] inline constexpr const char* kDodgeWList[] = {
    "AhriSeduce",
    "AkaliR",
    "AkaliRb",
    "CurseoftheSadMummy",
    "InfernalGuardian",
    "EnchantedCrystalArrow",
    "AzirR",
    "BrandWildfire",
    "CassiopeiaPetrifyingGaze",
    "DariusExecute",
    "DravenRCast",
    "EvelynnR",
    "EzrealTrueshotBarrage",
    "Terrify",
    "GalioIdolOfDurand",
    "GarenR",
    "GravesChargeShot",
    "HecarimUlt",
    "LissandraR",
    "LuxMaliceCannon",
    "UFSlash",
    "AlZaharNetherGrasp",
    "OrianaDetonateCommand",
    "LeonaSolarFlare",
    "SejuaniGlacialPrisonStart",
    "SonaCrescendo",
    "VarusR",
    "GragasR",
    "GnarR",
    "FizzMarinerDoom",
    "SyndraR"
};

enum class UltCastStage {
    First,
    Second,
    Cooldown
};

enum class ShadowCastStage {
    First,
    Second,
    Cooldown
};

static AIHeroClient Player() {
    return ObjectManager::Player();
}

static bool Bool(Menu* menu, const char* key, bool fallback = true) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuBool>(key);
    return item ? item->Value : fallback;
}

static int Slider(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuSlider>(key);
    return item ? item->Value : fallback;
}

static bool Key(Menu* menu, const char* key, bool fallback = false) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuKeyBind>(key);
    return item ? item->Active : fallback;
}

static bool ShouldRunNow(DWORD& lastTick, DWORD intervalMs) {
    const DWORD now = GetTickCount();
    if (lastTick != 0 && now - lastTick < intervalMs) {
        return false;
    }

    lastTick = now;
    return true;
}

static void RemoveKeyPermashow(Menu* menu, const char* key) {
    if (auto* item = menu ? menu->Get<MenuKeyBind>(key) : nullptr) {
        item->RemovePermashow();
    }
}

static bool EqualsIgnoreCase(const char* left, const char* right) {
    return left && right && left[0] && right[0] && _stricmp(left, right) == 0;
}

static std::string RuntimeName(const GameObject& unit) {
    if (!unit.IsValid()) {
        return {};
    }

    char direct[96] = {};
    if (::Core::Objects::ReadName(unit.Address(), direct, static_cast<int>(sizeof(direct))) &&
        direct[0]) {
        return direct;
    }
    if (::Core::Objects::ReadCharacterName(unit.Address(), direct, static_cast<int>(sizeof(direct))) &&
        direct[0]) {
        return direct;
    }
    return {};
}

static std::string RuntimeCharacterName(const AIBaseClient& unit) {
    if (!unit.IsValid()) {
        return {};
    }

    std::string cached = unit.CharacterName();
    if (!cached.empty()) {
        return cached;
    }

    char direct[96] = {};
    if (::Core::Objects::ReadCharacterName(unit.Address(), direct, static_cast<int>(sizeof(direct))) &&
        direct[0]) {
        return direct;
    }
    if (::Core::Objects::ReadName(unit.Address(), direct, static_cast<int>(sizeof(direct))) &&
        direct[0]) {
        return direct;
    }
    return {};
}

static std::string SpellName(SpellSlot slot) {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }
    const auto spell = player.GetSpell(slot);
    return spell.IsValid() ? spell.Name() : std::string();
}

static const char* BestSpellName(const Events::ProcessSpellEventArgs& args) {
    if (args.SpellName[0]) {
        return args.SpellName;
    }
    if (args.ScriptName[0]) {
        return args.ScriptName;
    }
    if (args.SpellSlotName[0]) {
        return args.SpellSlotName;
    }
    if (args.PayloadSpellName[0]) {
        return args.PayloadSpellName;
    }
    return "";
}

static bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f;
}

static bool ValidTarget(const AIBaseClient& unit, float range = FLT_MAX) {
    return ValidUnit(unit) && Extensions::IsValidTarget(unit, range, true);
}

static bool ValidHeroTarget(const AIHeroClient& hero, float range = FLT_MAX) {
    return ValidUnit(hero) && Extensions::IsValidTarget(hero, range, true);
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

static bool CastPosition(Spell& spell,
                         const Vector3& position,
                         const char* action,
                         const AIBaseClient& target = AIBaseClient()) {
    (void)action;
    (void)target;
    return spell.Cast(position);
}

static bool CastUnit(Spell& spell, const AIBaseClient& target, const char* action) {
    (void)action;
    if (!ValidTarget(target, spell.CurrentRange())) {
        return false;
    }
    return spell.Cast(target) == CastStates::SuccessfullyCasted;
}

static bool CastSelf(Spell& spell, const char* action) {
    (void)action;
    return spell.Cast();
}

static bool CanUseItem(int itemId) {
    const auto player = Player();
    return SDK::Items::CanUseItem(player, itemId);
}

static bool HasItem(int itemId) {
    const auto player = Player();
    return SDK::Items::HasItem(player, itemId);
}

static bool UseItem(int itemId) {
    const auto player = Player();
    return SDK::Items::UseItem(player, itemId);
}

static bool UseItem(int itemId, const AIBaseClient& target) {
    const auto player = Player();
    return SDK::Items::UseItem(player, itemId, target);
}

static std::vector<AIMinionClient> EnemyMinionsInRange(float range) {
    std::vector<AIMinionClient> result;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, range) && !minion.IsJungle() && !minion.IsPlant() &&
            !minion.IsPet() && !minion.IsClone()) {
            result.push_back(minion);
        }
    }
    return result;
}

static std::vector<AIBaseClient> ToBaseList(const std::vector<AIMinionClient>& minions) {
    std::vector<AIBaseClient> result;
    result.reserve(minions.size());
    for (const auto& minion : minions) {
        result.emplace_back(minion.Handle());
    }
    return result;
}

static bool IsDangerousSpell(const char* spellName) {
    if (!spellName || !spellName[0]) {
        return false;
    }
    for (const auto* dangerous : kDangerousList) {
        if (dangerous && std::strstr(dangerous, spellName)) {
            return true;
        }
    }
    return false;
}

static double IgniteDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }
    return 50.0 + 20.0 * static_cast<double>(std::max(1, player.Level()));
}

static double QDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid() || Q.Level() <= 0) {
        return 0.0;
    }

    const float sdkDamage = player.GetSpellDamage(target, SpellSlot::Q);
    if (sdkDamage > 0.0f) {
        return sdkDamage;
    }

    // CDragon latest ZedQ: BaseDamage + BonusADRatio/mSpellCalculations.
    static constexpr float qBase[] = { 0.0f, 40.0f, 80.0f, 120.0f, 160.0f, 200.0f };
    const int level = std::clamp(Q.Level(), 1, 5);
    const float raw = qBase[level] + player.BonusAttackDamage();
    return player.CalculatePhysicalDamage(target, raw);
}

static double EDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid() || E.Level() <= 0) {
        return 0.0;
    }

    const float sdkDamage = player.GetSpellDamage(target, SpellSlot::E);
    if (sdkDamage > 0.0f) {
        return sdkDamage;
    }

    // CDragon latest ZedE: BaseDamage + ADRatio/mSpellCalculations.
    static constexpr float eBase[] = { 0.0f, 70.0f, 92.5f, 115.0f, 137.5f, 160.0f };
    const int level = std::clamp(E.Level(), 1, 5);
    const float raw = eBase[level] + 0.699999988079071f * player.BonusAttackDamage();
    return player.CalculatePhysicalDamage(target, raw);
}

static double RDamageBase(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid() || R.Level() <= 0) {
        return 0.0;
    }

    const float sdkDamage = player.GetSpellDamage(target, SpellSlot::R);
    if (sdkDamage > 0.0f) {
        return sdkDamage;
    }

    // CDragon latest ZedR: RCalculatedDamage = 1.0 total AD.
    return player.CalculatePhysicalDamage(target, player.AD());
}

static double RDamageAmp() {
    // CDragon latest ZedR: RDamageAmp DataValues level 1/2/3 = 0.25/0.40/0.55.
    static constexpr double amp[] = { 0.0, 0.25, 0.4000000059604645, 0.550000011920929 };
    const int level = std::clamp(R.Level(), 0, 3);
    return amp[level];
}

static double PhysicalDamageWithExtraLethality(const AIHeroClient& source,
                                               const AIBaseClient& target,
                                               float amount,
                                               float extraLethality) {
    if (!source.IsValid() || !target.IsValid() || amount <= 0.0f) {
        return 0.0;
    }

    float percentArmorPen = source.PercentArmorPenetrationMod();
    float percentBonusArmorPen = source.PercentBonusArmorPenetrationMod();
    float flatArmorPen = source.Lethality() + extraLethality;
    flatArmorPen *= 0.6f + 0.4f * static_cast<float>(source.Level()) / 18.0f;
    if (std::isnan(flatArmorPen)) {
        flatArmorPen = 0.0f;
    }

    const float armor = target.Armor();
    const float bonusArmor = target.BonusArmor();
    float multiplier = 1.0f;
    if (armor < 0.0f) {
        multiplier = 2.0f - 100.0f / (100.0f - armor);
    } else {
        const float effectiveArmor = armor * percentArmorPen
            - bonusArmor * (1.0f - percentBonusArmorPen)
            - flatArmorPen;
        multiplier = effectiveArmor < 0.0f ? 1.0f : 100.0f / (100.0f + effectiveArmor);
    }

    float result = multiplier * amount;
    result = SDK::DamageMod::DamageReductionMod(source, target, result, DamageType::Physical);
    return std::max(std::floor(result), 0.0f);
}

static double ExtraPhysicalDamageWithLethality(const AIHeroClient& source,
                                               const AIBaseClient& target,
                                               float amount,
                                               float extraLethality) {
    const double normal = source.CalculatePhysicalDamage(target, amount);
    const double boosted = PhysicalDamageWithExtraLethality(source, target, amount, extraLethality);
    return std::max(0.0, boosted - normal);
}

static float QRawDamage() {
    const auto player = Player();
    if (!player.IsValid() || Q.Level() <= 0) {
        return 0.0f;
    }
    static constexpr float qBase[] = { 0.0f, 40.0f, 80.0f, 120.0f, 160.0f, 200.0f };
    return qBase[std::clamp(Q.Level(), 1, 5)] + player.BonusAttackDamage();
}

static float ERawDamage() {
    const auto player = Player();
    if (!player.IsValid() || E.Level() <= 0) {
        return 0.0f;
    }
    static constexpr float eBase[] = { 0.0f, 70.0f, 92.5f, 115.0f, 137.5f, 160.0f };
    return eBase[std::clamp(E.Level(), 1, 5)] + 0.699999988079071f * player.BonusAttackDamage();
}

static float RRawDamage() {
    const auto player = Player();
    return player.IsValid() && R.Level() > 0 ? player.AD() : 0.0f;
}

static float ProfaneHydraRawDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    const float minDamage = 0.80f * player.AD();
    const float maxDamage = 1.30f * player.AD();
    const float healthRatio = target.MaxHealth() > 1.0f
        ? std::clamp(target.Health() / target.MaxHealth(), 0.0f, 1.0f)
        : 1.0f;
    const float lowHealthScale = std::clamp((0.5f - healthRatio) / 0.5f, 0.0f, 1.0f);
    return minDamage + (maxDamage - minDamage) * lowHealthScale;
}

static bool HasVoltaicEnergizedAttack() {
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }
    return HasItem(kItemVoltaicCyclosword) &&
        (player.GetBuffCount("itemstatikshankcharge") >= 100 ||
         player.HasBuff("itemstatikshankcharge"));
}

static int ExpectedZedPhysicalHitCount(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0;
    }

    int hits = 0;
    if (Q.IsReady()) {
        ++hits;
    }
    if (W.IsReady() && Q.IsReady()) {
        ++hits;
    }
    if (E.IsReady() && target.Distance(player.Position()) <= E.Range) {
        ++hits;
    }
    if (R.IsReady()) {
        ++hits;
    }
    if (CanUseItem(kItemProfaneHydra) && target.Distance(player.Position()) <= 450.0f) {
        ++hits;
    }
    return hits;
}

static float VoltaicFollowupRawDamage() {
    float raw = 0.0f;
    if (Q.IsReady()) {
        raw += QRawDamage();
    }
    if (W.IsReady() && Q.IsReady()) {
        raw += QRawDamage() * 0.5f;
    }
    if (E.IsReady()) {
        raw += ERawDamage();
    }
    if (R.IsReady()) {
        raw += RRawDamage();
    }
    return raw;
}

static double ZedItemDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0;
    }

    double damage = 0.0;
    if (CanUseItem(kItemTiamat)) {
        damage += player.CalculatePhysicalDamage(target, player.BaseAttackDamage() * 0.6f);
    }
    if (CanUseItem(kItemRavenousHydra)) {
        damage += player.CalculatePhysicalDamage(target, player.BaseAttackDamage() * 0.6f);
    }
    if (CanUseItem(kItemTitanicHydra)) {
        damage += player.CalculatePhysicalDamage(
            target,
            player.BaseAttackDamage() * 0.4f + player.MaxHealth() * 0.1f);
    }
    if (CanUseItem(kItemProfaneHydra) && target.Distance(player.Position()) <= 450.0f) {
        damage += player.CalculatePhysicalDamage(target, ProfaneHydraRawDamage(target));
    }
    if (HasItem(kItemBladeOfTheRuinedKing)) {
        // CDragon latest: Blade of The Ruined King has no active; Mist's Edge
        // on-hit is 9% melee current HP physical with a 15 minimum in SDK data.
        damage += player.CalculatePhysicalDamage(
            target,
            std::max(15.0f, target.Health() * 0.09f));
    }
    if (HasVoltaicEnergizedAttack()) {
        // CDragon latest 6699, melee branch: 9% current HP physical and +15 lethality for 4s.
        damage += player.CalculatePhysicalDamage(target, target.Health() * 0.09f);
        damage += ExtraPhysicalDamageWithLethality(
            player,
            target,
            VoltaicFollowupRawDamage(),
            15.0f);
    }
    if (HasItem(kItemEclipse) &&
        (target.GetBuffCount("eclipsetargetdebuff") >= 1 ||
         ExpectedZedPhysicalHitCount(target) >= 2)) {
        damage += player.CalculatePhysicalDamage(target, target.MaxHealth() * 0.06f);
    }
    if (HasItem(kItemUmbralGlaive) &&
        (player.HasBuff("3179_AttackReady") || player.HasBuff("3179_UnseenBuff"))) {
        damage += 50.0f + 1.5f * player.Lethality();
    }
    return damage;
}

static bool CanCollectorExecuteAfterDamage(const AIBaseClient& target, double projectedDamage) {
    if (!target.IsValid() || !HasItem(kItemTheCollector) || projectedDamage <= 0.0) {
        return false;
    }

    return target.Health() - static_cast<float>(projectedDamage) <=
        target.MaxHealth() * 0.05000000074505806f;
}

static AIHeroClient GetEnemy();
static AIMinionClient WShadow();
static AIMinionClient RShadow();
static UltCastStage GetUltStage();
static ShadowCastStage GetShadowStage();
static void BuildMenu();
static void OnGameLoad();
static void OnDoCast(const Events::ProcessSpellEventArgs& args);
static void OnGameUpdate(const GameUpdateEventArgs& args);
static float ComboDamage(const AIBaseClient& enemy);
static void Combo(AIHeroClient t);
static void TheLine(AIHeroClient t);
static void _CastQ(AIHeroClient target);
static void Harass(AIHeroClient t);
static void Laneclear();
static void LastHit();
static void JungleClear();
static void UseItemes(AIHeroClient target);
static void CastW(AIBaseClient target);
static void CastQ(AIBaseClient target);
static void CastE();
static void KillSteal();
static void OnDraw();
static void OnUnload();

static AIHeroClient GetEnemy() {
    auto* selector = SDK::TargetSelector::Instance();
    const AIHeroClient selected = selector ? selector->GetSelectedTarget() : AIHeroClient();
    return selected.IsValid() ? selected : GetTarget(1400.0f, DamageType::Magical);
}

// Per-tick memo for the Zed-shadow lookups. WShadow()/RShadow() are queried 3-5x
// per game tick (Combo, CastE, CastQ, KillSteal, OnDraw); the naive 1-1 port
// recomputed each call by copying the AllyMinions() snapshot AND allocating a
// std::string per minion via RuntimeName() — a major per-tick cost with holding
// C/Space. This scans the ally-minion list at most once per tick into a stack
// buffer (no heap alloc) and reuses the result. thread_local so the game-thread
// (OnGameUpdate) and render-thread (OnDraw) never share the cache (no data race);
// keyed on (tick, RPos) so it stays correct when RPos is updated mid-tick.
inline thread_local int t_ShadowTick = -1;
inline thread_local Vector3 t_ShadowRPos = {};
inline thread_local AIMinionClient t_WShadow = {};
inline thread_local AIMinionClient t_RShadow = {};

static void EnsureShadowCache() {
    const int tick = SDK::Variables::TickCount();
    if (t_ShadowTick == tick && t_ShadowRPos == RPos) {
        return;
    }
    t_ShadowTick = tick;
    t_ShadowRPos = RPos;

    AIMinionClient w{};
    AIMinionClient r{};
    // Match the C# original: scan the RAW minion object list
    // (ObjectManager.Get<AIMinionClient>()), NOT GameObjects::AllyMinions().
    // The Zed shadow ("Shadow") is not a lane minion, so AllyMinions() (which
    // holds lane minions only) never contains it — using it left WShadow()/
    // RShadow() permanently invalid and broke E / W-recast / KS on shadows.
    for (const auto& minion : SDK::ObjectManager::Get<AIMinionClient>()) {
        if (!minion.IsVisible() || !minion.IsAlly()) {
            continue;
        }
        // Match RuntimeName()'s ReadName->ReadCharacterName fallback, but into a
        // stack buffer so there is no std::string allocation per minion.
        char name[64] = {};
        if (!(::Core::Objects::ReadName(minion.Address(), name, static_cast<int>(sizeof(name))) && name[0]) &&
            !(::Core::Objects::ReadCharacterName(minion.Address(), name, static_cast<int>(sizeof(name))) && name[0])) {
            continue;
        }
        if (_stricmp(name, "Shadow") != 0) {
            continue;
        }
        if (minion.Position() == RPos) {
            if (!r.IsValid()) {
                r = minion;
            }
        } else if (!w.IsValid()) {
            w = minion;
        }
        if (w.IsValid() && r.IsValid()) {
            break;
        }
    }
    t_WShadow = w;
    t_RShadow = r;
}

static AIMinionClient WShadow() {
    EnsureShadowCache();
    return t_WShadow;
}

static AIMinionClient RShadow() {
    EnsureShadowCache();
    return t_RShadow;
}

static UltCastStage GetUltStage() {
    if (!R.IsReady()) {
        return UltCastStage::Cooldown;
    }
    return EqualsIgnoreCase(SpellName(SpellSlot::R).c_str(), "ZedR")
        ? UltCastStage::First
        : UltCastStage::Second;
}

static ShadowCastStage GetShadowStage() {
    if (!W.IsReady()) {
        return ShadowCastStage::Cooldown;
    }
    return EqualsIgnoreCase(SpellName(SpellSlot::W).c_str(), "ZedW")
        ? ShadowCastStage::First
        : ShadowCastStage::Second;
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.7upaio", "7UP - Zed", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo"));
    ComboMenu->Add(new MenuBool("UseWC", "Use W (also gap close)"));
    ComboMenu->Add(new MenuBool("UseIgnitecombo", "Use Ignite(rush for it)"));
    ComboMenu->Add(new MenuBool("UseUlt", "Use Ultimate"));
    ComboMenu->Add(new MenuKeyBind("ActiveCombo", "Combo!", SDK::Keys::Space, KeyBindType::Press))->Permashow();
    ComboMenu->Add(new MenuKeyBind("TheLine", "The Line Combo", SDK::Keys::T, KeyBindType::Press))->Permashow();

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass"));
    HarassMenu->Add(new MenuKeyBind("longhar", "Long Poke (toggle)", SDK::Keys::U, KeyBindType::Toggle))->Permashow();
    HarassMenu->Add(new MenuBool("UseItemsharass", "Use Tiamat/Hydra"));
    HarassMenu->Add(new MenuBool("UseWH", "Use W"));
    HarassMenu->Add(new MenuKeyBind("ActiveHarass", "Harass!", SDK::Keys::C, KeyBindType::Press))->Permashow();

    ItemsMenu = MenuRoot->AddSubMenu(new Menu("items", "items"));
    OffensiveMenu = ItemsMenu->AddSubMenu(new Menu("Offensive", "Offensive"));
    OffensiveMenu->Add(new MenuBool("Youmuu", "Use Youmuu's"));
    OffensiveMenu->Add(new MenuBool("Tiamat", "Use Tiamat"));
    OffensiveMenu->Add(new MenuBool("Hydra", "Use Hydra"));
    OffensiveMenu->Add(new MenuBool("Bilge", "Use Bilge"));
    OffensiveMenu->Add(new MenuSlider("BilgeEnemyhp", "If Enemy Hp <", 85, 1, 100));
    OffensiveMenu->Add(new MenuSlider("Bilgemyhp", "Or your Hp < ", 85, 1, 100));
    OffensiveMenu->Add(new MenuBool("Blade", "Use Blade"));
    OffensiveMenu->Add(new MenuSlider("BladeEnemyhp", "If Enemy Hp <", 85, 1, 100));
    OffensiveMenu->Add(new MenuSlider("Blademyhp", "Or Your  Hp <", 85, 1, 100));

    DefensiveMenu = ItemsMenu->AddSubMenu(new Menu("Deffensive", "Deffensive"));
    DefensiveMenu->Add(new MenuBool("Omen", "Use Randuin Omen"));
    DefensiveMenu->Add(new MenuSlider("Omenenemys", "Randuin if enemys>", 2, 1, 5));
    DefensiveMenu->Add(new MenuBool("lotis", "Use Iron Solari"));
    DefensiveMenu->Add(new MenuSlider("lotisminhp", "Solari if Ally Hp<", 35, 1, 100));

    FarmMenu = MenuRoot->AddSubMenu(new Menu("Farm", "Farm"));
    LaneFarmMenu = FarmMenu->AddSubMenu(new Menu("LaneFarm", "LaneFarm"));
    LaneFarmMenu->Add(new MenuBool("UseItemslane", "Use Hydra/Tiamat"));
    LaneFarmMenu->Add(new MenuBool("UseQL", "Q LaneClear"));
    LaneFarmMenu->Add(new MenuBool("UseEL", "E LaneClear"));
    LaneFarmMenu->Add(new MenuSlider("Energylane", "Energy Lane% >", 45, 1, 100));
    LaneFarmMenu->Add(new MenuKeyBind("Activelane", "Lane clear!", SDK::Keys::S, KeyBindType::Press))->Permashow();

    LastHitMenu = FarmMenu->AddSubMenu(new Menu("LastHit", "LastHit"));
    LastHitMenu->Add(new MenuBool("UseQLH", "Q LastHit"));
    LastHitMenu->Add(new MenuBool("UseELH", "E LastHit"));
    LastHitMenu->Add(new MenuSlider("Energylast", "Energy lasthit% >", 85, 1, 100));
    LastHitMenu->Add(new MenuKeyBind("ActiveLast", "LastHit!", SDK::Keys::X, KeyBindType::Press))->Permashow();

    JungleMenu = FarmMenu->AddSubMenu(new Menu("Jungle", "Jungle"));
    JungleMenu->Add(new MenuBool("UseItemsjungle", "Use Hydra/Tiamat"));
    JungleMenu->Add(new MenuBool("UseQJ", "Q Jungle"));
    JungleMenu->Add(new MenuBool("UseWJ", "W Jungle"));
    JungleMenu->Add(new MenuBool("UseEJ", "E Jungle"));
    JungleMenu->Add(new MenuSlider("Energyjungle", "Energy Jungle% >", 85, 1, 100));
    JungleMenu->Add(new MenuKeyBind("Activejungle", "Jungle!", SDK::Keys::S, KeyBindType::Press))->Permashow();

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc", "Misc"));
    MiscMenu->Add(new MenuBool("UseIgnitekill", "Use Ignite KillSteal"));
    MiscMenu->Add(new MenuBool("UseQM", "Use Q KillSteal"));
    MiscMenu->Add(new MenuBool("UseEM", "Use E KillSteal"));
    MiscMenu->Add(new MenuBool("AutoE", "Auto E"));
    MiscMenu->Add(new MenuBool("rdodge", "R Dodge Dangerous"));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const auto spell = enemy.GetSpell(SpellSlot::R);
        const std::string spellName = spell.IsValid() ? spell.Name() : std::string();
        if (IsDangerousSpell(spellName.c_str())) {
            const std::string key = "ds" + RuntimeCharacterName(enemy);
            MiscMenu->Add(new MenuBool(key.c_str(), spellName.c_str()));
        }
    }

    DrawingsMenu = MenuRoot->AddSubMenu(new Menu("Drawings", "Drawings"));
    DrawingsMenu->Add(new MenuBool("DrawQ", "Draw Q"));
    DrawingsMenu->Add(new MenuBool("DrawE", "Draw E"));
    DrawingsMenu->Add(new MenuBool("DrawQW", "Draw long harras"));
    DrawingsMenu->Add(new MenuBool("DrawR", "Draw R"));
    DrawingsMenu->Add(new MenuBool("DrawHP", "Draw HP bar"));
    DrawingsMenu->Add(new MenuBool("shadowd", "Shadow Position"));
    DrawingsMenu->Add(new MenuBool("damagetest", "Damage Text", false));
    DrawingsMenu->Add(new MenuBool("CircleLag", "Lag Free Circles"));
    DrawingsMenu->Add(new MenuSlider("CircleQuality", "Circles Quality", 100, 10, 100));
    DrawingsMenu->Add(new MenuSlider("CircleThickness", "Circles Thickness", 1, 1, 10));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (!EqualsIgnoreCase(RuntimeCharacterName(player).c_str(), "Zed")) {
        return;
    }
    if (Loaded) {
        return;
    }

    // CDragon latest ZedQMissile: castRange 925, mMissileWidth 50, mSpeed 1700; ZedQ cast delay 0.25.
    Q = Spell(SpellSlot::Q, 925.0f);
    Q.SetSkillshot(0.25f, 50.0f, 1700.0f, true, SpellType::Line);
    Q.SetCollisionObjects(SDK::CollisionableObjects::YasuoWall);

    // CDragon latest ZedW: castRange 700; ZedWMissile width 60, speed 2500.
    W = Spell(SpellSlot::W, 700.0f);
    W.SetSkillshot(0.25f, 60.0f, 2500.0f, false, SpellType::Line);
    E = Spell(SpellSlot::E, 290.0f);
    R = Spell(SpellSlot::R, 625.0f);

    Ignite = Spell(player.GetSpellSlot("summonerdot"), 600.0f);

    SpellList.clear();
    SpellList.push_back(&Q);
    SpellList.push_back(&W);
    SpellList.push_back(&E);
    SpellList.push_back(&R);

    BuildMenu();

    Drawing::OnDraw += &OnDraw;
    Events::hook.OnGameUpdate += &OnGameUpdate;
    Events::hook.OnDoCast += &OnDoCast;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>7UP - Zed loaded</font>");
}

static void OnDoCast(const Events::ProcessSpellEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid() || !args.Sender.IsValid()) {
        return;
    }

    const AIHeroClient sender(args.Sender.Ptr);
    if (!sender.IsValid()) {
        return;
    }

    const char* spellName = BestSpellName(args);
    if (sender.Compare(player) && EqualsIgnoreCase(spellName, "zedult")) {
        TickTock = SDK::Variables::TickCount() + 200;
    } else if (sender.IsEnemy()) {
        const std::string dangerKey = "ds" + RuntimeCharacterName(sender);
        if (Bool(MiscMenu, "rdodge") &&
            (R.IsReady() || EqualsIgnoreCase(SpellName(SpellSlot::R).c_str(), "ZedR2")) &&
            Bool(MiscMenu, dangerKey.c_str())) {
            if (IsDangerousSpell(spellName) &&
                (sender.Distance(player.Position()) < 650.0f ||
                 player.Distance(args.EndPosition) <= 250.0f)) {
                if (EqualsIgnoreCase(spellName, "SyndraR")) {
                    ClockOn = SDK::Variables::TickCount() + 150;
                    CountDanger = CountDanger + 1;
                } else {
                    const auto target = GetTarget(640.0f, DamageType::Magical);
                    CastUnit(R, target, "rdodge-R");
                }
            }
        }
    }
}

// TEMP PROBE (remove after FPS profiling). ACCUMULATOR version: each scope adds
// its ms to a per-name running total and only ONE file write happens per second
// (dumping + resetting all totals). This removes the per-call file-I/O that the
// old >threshold-write version did on the game thread — that I/O was inflating
// every enclosing scope's measurement (observer effect). All ZedProbe uses are
// on the game thread (OnGameUpdate/Combo/KillSteal), so no locking is needed.
// Read [Acc] lines as: name=<total ms in the last ~1s>/<call count>.
struct ProbeAcc { const char* n; double ms; unsigned cnt; };
inline ProbeAcc g_probeAcc[48] = {};
inline int g_probeAccN = 0;
inline DWORD g_probeAccLast = 0;

struct ZedProbe {
    const char* n;
    LARGE_INTEGER s;
    explicit ZedProbe(const char* name) : n(name) { QueryPerformanceCounter(&s); }
    ~ZedProbe() {
        LARGE_INTEGER e{}, f{};
        QueryPerformanceCounter(&e);
        QueryPerformanceFrequency(&f);
        const double ms = static_cast<double>(e.QuadPart - s.QuadPart) * 1000.0 /
                          static_cast<double>(f.QuadPart);

        ProbeAcc* a = nullptr;
        for (int i = 0; i < g_probeAccN; ++i) {
            if (g_probeAcc[i].n == n) { a = &g_probeAcc[i]; break; }
        }
        if (!a && g_probeAccN < 48) {
            a = &g_probeAcc[g_probeAccN++];
            a->n = n; a->ms = 0.0; a->cnt = 0;
        }
        if (a) { a->ms += ms; a->cnt += 1; }

        const DWORD now = GetTickCount();
        if (now - g_probeAccLast >= 1000) {
            g_probeAccLast = now;
            char b[1536];
            int p = std::snprintf(b, sizeof(b), "[Acc/1s] ");
            for (int i = 0; i < g_probeAccN && p < static_cast<int>(sizeof(b)) - 48; ++i) {
                p += std::snprintf(b + p, sizeof(b) - p, "%s=%.2f/%u ",
                                   g_probeAcc[i].n, g_probeAcc[i].ms, g_probeAcc[i].cnt);
                g_probeAcc[i].ms = 0.0; g_probeAcc[i].cnt = 0;
            }
            p += std::snprintf(b + p, sizeof(b) - p, "\r\n");
            HANDLE h = CreateFileA("C:\\Users\\Public\\nightsharp_fps_drop_debug.txt",
                                   FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                                   OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                DWORD w = 0;
                WriteFile(h, b, static_cast<DWORD>(p), &w, nullptr);
                CloseHandle(h);
            }
        }
    }
};

static void OnGameUpdate(const GameUpdateEventArgs&) {
    if (!ShouldRunNow(LastUpdateTick, 40)) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen() || Game::IsShopOpen()) {
        return;
    }

    // Measures the ENTIRE handler each tick — fires even at idle (no combo key),
    // so this captures the per-tick cost that drops FPS before any logic runs.
    ZedProbe pTotal("OnUpdate-TOTAL");

    { // isolates the per-tick menu-accessor (Key/Bool) overhead — at idle the
      // if-bodies below all skip, so this measures the Key()/Bool() lookups only.
    ZedProbe pDisp("dispatch-keys");
    if (Key(ComboMenu, "ActiveCombo")) {
        AIHeroClient e; { ZedProbe p("GetEnemy"); e = GetEnemy(); }
        ZedProbe p("Combo");
        Combo(e);
    }
    if (Key(ComboMenu, "TheLine")) {
        TheLine(GetEnemy());
    }
    if (Key(HarassMenu, "ActiveHarass")) {
        Harass(GetEnemy());
    }
    if (Key(LaneFarmMenu, "Activelane")) {
        Laneclear();
    }
    if (Key(JungleMenu, "Activejungle")) {
        JungleClear();
    }
    if (Key(LastHitMenu, "ActiveLast")) {
        LastHit();
    }
    if (Bool(MiscMenu, "AutoE")) {
        ZedProbe p("AutoE-CastE");
        CastE();
    }
    }
    if (SDK::Variables::TickCount() >= ClockOn && CountDanger > CountUlts) {
        CastUnit(R, GetTarget(640.0f, DamageType::Magical), "delayed-danger-R");
        CountUlts = CountUlts + 1;
    }

    const auto& lastCast = SDK::LastCast::LastCastPacketSent();
    if (lastCast.Slot == SpellSlot::R) {
        ZedProbe pRs("Rshadow-scan");
        // C# original scans ObjectManager.Get<AIMinionClient>() (raw list); the
        // shadow is not a lane minion so AllyMinions() would never find it.
        for (const auto& minion : SDK::ObjectManager::Get<AIMinionClient>()) {
            if (!minion.IsVisible() || !minion.IsAlly()) {
                continue;
            }
            // Stack-buffer name read (no std::string alloc per minion) — matches
            // EnsureShadowCache. RuntimeName() allocated a std::string for every
            // ally minion each tick this ran (lastCast==R during combo), the bulk
            // of the rest(RPos+KS) cost.
            char name[64] = {};
            if (!(::Core::Objects::ReadName(minion.Address(), name, static_cast<int>(sizeof(name))) && name[0]) &&
                !(::Core::Objects::ReadCharacterName(minion.Address(), name, static_cast<int>(sizeof(name))) && name[0])) {
                continue;
            }
            if (_stricmp(name, "Shadow") == 0) {
                RPos = minion.Position();
                break;
            }
        }
    }

    { ZedProbe pKS("KillSteal"); KillSteal(); }
}

static float ComboDamage(const AIBaseClient& enemy) {
    const auto player = Player();
    if (!enemy.IsValid()) {
        return 0.0f;
    }

    double damage = 0.0;
    double igniteDamage = 0.0;
    if (Ignite.Slot != SpellSlot::Unknown && Ignite.IsReady()) {
        igniteDamage = IgniteDamage(enemy);
        damage += igniteDamage;
    }
    damage += ZedItemDamage(enemy);
    if (Q.IsReady()) {
        damage += QDamage(enemy);
    }
    if (W.IsReady() && Q.IsReady()) {
        damage += QDamage(enemy) / 2.0;
    }
    if (E.IsReady()) {
        damage += EDamage(enemy);
    }
    if (R.IsReady()) {
        damage += RDamageBase(enemy);
    }
    damage += RDamageAmp() * (damage - igniteDamage);
    if (CanCollectorExecuteAfterDamage(enemy, damage)) {
        damage = static_cast<double>(enemy.Health()) + 1.0;
    }
    return static_cast<float>(damage);
}

static void Combo(AIHeroClient t) {
    const auto player = Player();
    auto target = t;
    if (!target.IsValid()) {
        return;
    }
    const auto doubleu = player.GetSpell(SpellSlot::W);
    // `overkill` (heavy: GetAutoAttackDamage runs the full item-passive + buff
    // suite) is only consumed by the UseUlt/First-stage opener below. The C#
    // computed it unconditionally every tick; short-circuit so it is only
    // evaluated when that branch is actually reachable. Behavior is identical —
    // the damage helpers are pure reads with no side effects.
    bool ultOpener = false;
    { ZedProbe pu("Combo-ult");
    if (Bool(ComboMenu, "UseUlt") && GetUltStage() == UltCastStage::First) {
        const double overkill =
            QDamage(target) + EDamage(target) + Damage::GetAutoAttackDamage(player, target) * 2.0;
        ultOpener =
            overkill < target.Health() ||
            (!W.IsReady() && doubleu.Cooldown() > 2.0f &&
             QDamage(target) < target.Health() &&
             target.Distance(player.Position()) > 400.0f);
    }
    }
    if (ultOpener) {
        if ((target.Distance(player.Position()) > 700.0f && target.MoveSpeed() > player.MoveSpeed()) ||
            target.Distance(player.Position()) > 800.0f) {
            CastW(target);
            CastSelf(W, "combo-W-recast-after-gapclose");
        }

        CastUnit(R, target, "combo-R");
    } else {
        if (target.IsValid() && Bool(ComboMenu, "UseIgnitecombo") &&
            Ignite.Slot != SpellSlot::Unknown && Ignite.IsReady()) {
            ZedProbe p("Combo-ignitedmg");
            if (ComboDamage(target) > target.Health() || target.HasBuff("zedulttargetmark")) {
                CastUnit(Ignite, target, "combo-ignite");
            }
        }
        { ZedProbe ps("Combo-stages");
        if (target.IsValid() && GetShadowStage() == ShadowCastStage::First &&
            Bool(ComboMenu, "UseWC") && target.Distance(player.Position()) > 400.0f &&
            target.Distance(player.Position()) < 1300.0f) {
            CastW(target);
        }
        const auto wShadow = WShadow();
        if (target.IsValid() && GetShadowStage() == ShadowCastStage::Second &&
            Bool(ComboMenu, "UseWC") && wShadow.IsValid() &&
            target.Distance(wShadow.Position()) < target.Distance(player.Position())) {
            CastSelf(W, "combo-W-recast");
        }
        }
        { ZedProbe p("Combo-items"); UseItemes(target); }
        { ZedProbe p("Combo-CastE"); CastE(); }
        { ZedProbe p("Combo-CastQ"); CastQ(target); }
    }
}

static void TheLine(AIHeroClient t) {
    const auto player = Player();
    auto target = t;
    if (!target.IsValid()) {
        SDK::IssueOrder(player, SDK::GameObjectOrder::MoveTo, Game::CursorPos());
        return;
    }

    SDK::IssueOrder(player, SDK::GameObjectOrder::AttackUnit, target);
    if (!R.IsReady() || target.Distance(player.Position()) >= 640.0f) {
        return;
    }
    if (GetUltStage() == UltCastStage::First) {
        CastUnit(R, target, "line-R");
    }
    LinePos = target.Position().Extend(player.Position(), -500.0f);
    if (target.IsValid() && GetShadowStage() == ShadowCastStage::First &&
        GetUltStage() == UltCastStage::Second) {
        UseItemes(target);
        const auto& lastCast = SDK::LastCast::LastCastPacketSent();
        if (lastCast.Slot != SpellSlot::W) {
            CastPosition(W, LinePos, "line-W", target);
            CastE();
            CastQ(target);
            if (target.IsValid() && Bool(ComboMenu, "UseIgnitecombo") &&
                Ignite.Slot != SpellSlot::Unknown && Ignite.IsReady()) {
                CastUnit(Ignite, target, "line-ignite");
            }
        }
    }

    const auto wShadow = WShadow();
    if (target.IsValid() && wShadow.IsValid() && GetUltStage() == UltCastStage::Second &&
        target.Distance(player.Position()) > 250.0f &&
        target.Distance(wShadow.Position()) < target.Distance(player.Position())) {
        CastSelf(W, "line-W-recast");
    }
}

static void _CastQ(AIHeroClient target) {
    (void)target;
    // C# source intentionally throws NotImplementedException here.
}

static void Harass(AIHeroClient t) {
    const auto player = Player();
    auto target = t;
    if (!target.IsValid()) {
        return;
    }
    const bool useItemsH = Bool(HarassMenu, "UseItemsharass");
    if (ValidHeroTarget(target) && Key(HarassMenu, "longhar") && W.IsReady() &&
        Q.IsReady() && player.Mana() >
        player.GetSpell(SpellSlot::Q).ManaCost() +
        player.GetSpell(SpellSlot::W).ManaCost() &&
        target.Distance(player.Position()) > 850.0f &&
        target.Distance(player.Position()) < 1400.0f) {
        CastW(target);
    }
    const auto wShadow = WShadow();
    if (ValidHeroTarget(target) &&
        (GetShadowStage() == ShadowCastStage::Second ||
         GetShadowStage() == ShadowCastStage::Cooldown ||
         !Bool(HarassMenu, "UseWH")) &&
        Q.IsReady() &&
        (target.Distance(player.Position()) <= 900.0f ||
         (wShadow.IsValid() && target.Distance(wShadow.Position()) <= 900.0f))) {
        CastQ(target);
    }
    if (ValidHeroTarget(target) && W.IsReady() && Q.IsReady() &&
        Bool(HarassMenu, "UseWH") && player.Mana() >
        player.GetSpell(SpellSlot::Q).ManaCost() +
        player.GetSpell(SpellSlot::W).ManaCost()) {
        if (target.Distance(player.Position()) < 750.0f) {
            CastW(target);
        }
    }
    CastE();
    if (useItemsH && CanUseItem(kItemTiamat) &&
        target.Distance(player.Position()) < 250.0f) {
        UseItem(kItemTiamat);
    }
    if (useItemsH && CanUseItem(kItemRavenousHydra) &&
        target.Distance(player.Position()) < 250.0f) {
        UseItem(kItemRavenousHydra);
    }
    if (useItemsH && CanUseItem(kItemProfaneHydra) &&
        target.Distance(player.Position()) < 450.0f) {
        UseItem(kItemProfaneHydra);
    }
}

static void Laneclear() {
    const auto player = Player();
    const auto allMinionsQ = EnemyMinionsInRange(Q.Range);
    const auto allMinionsE = EnemyMinionsInRange(E.Range);
    const bool mymana = player.Mana() >= player.MaxMana() *
        static_cast<float>(Slider(LaneFarmMenu, "Energylane", 45)) / 100.0f;
    const bool useItemsl = Bool(LaneFarmMenu, "UseItemslane");
    const bool useQl = Bool(LaneFarmMenu, "UseQL");
    const bool useEl = Bool(LaneFarmMenu, "UseEL");
    if (Q.IsReady() && useQl && mymana) {
        const auto baseMinionsQ = ToBaseList(allMinionsQ);
        const auto fl2 = Q.GetLineFarmLocation(baseMinionsQ, Q.Width);
        if (fl2.MinionsHit >= 3) {
            Q.Cast(fl2.Position);
        } else {
            for (const auto& minion : allMinionsQ) {
                if (!AutoAttack::InAutoAttackRange(minion) &&
                    minion.Health() < 0.75 * QDamage(minion)) {
                    CastUnit(Q, minion, "laneclear-Q-last");
                }
            }
        }
    }

    if (E.IsReady() && useEl && mymana) {
        if (allMinionsE.size() > 2) {
            CastSelf(E, "laneclear-E");
        } else {
            for (const auto& minion : allMinionsE) {
                if (!AutoAttack::InAutoAttackRange(minion) &&
                    minion.Health() < 0.75 * EDamage(minion)) {
                    CastSelf(E, "laneclear-E-last");
                }
            }
        }
    }

    if (useItemsl && CanUseItem(kItemTiamat) && allMinionsE.size() > 2) {
        UseItem(kItemTiamat);
    }
    if (useItemsl && CanUseItem(kItemRavenousHydra) && allMinionsE.size() > 2) {
        UseItem(kItemRavenousHydra);
    }
    if (useItemsl && CanUseItem(kItemProfaneHydra) && allMinionsE.size() > 2) {
        UseItem(kItemProfaneHydra);
    }
}

static void LastHit() {
    const auto player = Player();
    const auto allMinions = EnemyMinionsInRange(Q.Range);
    const bool mymana = player.Mana() >=
        player.MaxMana() * static_cast<float>(Slider(LastHitMenu, "Energylast", 85)) / 100.0f;
    const bool useQ = Bool(LastHitMenu, "UseQLH");
    const bool useE = Bool(LastHitMenu, "UseELH");
    for (const auto& minion : allMinions) {
        if (mymana && useQ && Q.IsReady() &&
            player.Distance(minion.Position()) < Q.Range &&
            minion.Health() < 0.75 * QDamage(minion)) {
            CastUnit(Q, minion, "lasthit-Q");
        }
        if (mymana && E.IsReady() && useE &&
            player.Distance(minion.Position()) < E.Range &&
            minion.Health() < 0.95 * EDamage(minion)) {
            CastSelf(E, "lasthit-E");
        }
    }
}

static void JungleClear() {
    const auto player = Player();
    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& mob) {
                return !ValidTarget(mob, Q.Range);
            }),
        mobs.end());
    std::sort(
        mobs.begin(),
        mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) {
            return a.MaxHealth() < b.MaxHealth();
        });

    const bool mymana = player.Mana() >= player.MaxMana() *
        static_cast<float>(Slider(JungleMenu, "Energyjungle", 85)) / 100.0f;
    const bool useItemsJ = Bool(JungleMenu, "UseItemsjungle");
    const bool useQ = Bool(JungleMenu, "UseQJ");
    const bool useW = Bool(JungleMenu, "UseWJ");
    const bool useE = Bool(JungleMenu, "UseEJ");
    if (!mobs.empty()) {
        const auto mob = mobs[0];
        if (mymana && W.IsReady() && useW && player.Distance(mob.Position()) < Q.Range) {
            CastPosition(W, mob.Position(), "jungle-W", mob);
        }
        if (mymana && useQ && Q.IsReady() && player.Distance(mob.Position()) < Q.Range) {
            CastQ(mob);
        }
        if (mymana && E.IsReady() && useE && player.Distance(mob.Position()) < E.Range) {
            CastSelf(E, "jungle-E");
        }
        if (useItemsJ && CanUseItem(kItemTiamat) && player.Distance(mob.Position()) < 250.0f) {
            UseItem(kItemTiamat);
        }
        if (useItemsJ && CanUseItem(kItemRavenousHydra) && player.Distance(mob.Position()) < 250.0f) {
            UseItem(kItemRavenousHydra);
        }
        if (useItemsJ && CanUseItem(kItemProfaneHydra) && player.Distance(mob.Position()) < 450.0f) {
            UseItem(kItemProfaneHydra);
        }
    }
}

static void UseItemes(AIHeroClient target) {
    const auto player = Player();
    const bool iBilge = Bool(OffensiveMenu, "Bilge");
    const bool iBilgeEnemyhp = target.Health() <= target.MaxHealth() *
        static_cast<float>(Slider(OffensiveMenu, "BilgeEnemyhp", 85)) / 100.0f;
    const bool iBilgemyhp = player.Health() <= player.MaxHealth() *
        static_cast<float>(Slider(OffensiveMenu, "Bilgemyhp", 85)) / 100.0f;
    const bool iBlade = Bool(OffensiveMenu, "Blade");
    const bool iBladeEnemyhp = target.Health() <= target.MaxHealth() *
        static_cast<float>(Slider(OffensiveMenu, "BladeEnemyhp", 85)) / 100.0f;
    const bool iBlademyhp = player.Health() <= player.MaxHealth() *
        static_cast<float>(Slider(OffensiveMenu, "Blademyhp", 85)) / 100.0f;
    const bool iOmen = Bool(DefensiveMenu, "Omen");
    int enemiesInOmen = 0;
    for (const auto& hero : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(hero, 450.0f)) {
            ++enemiesInOmen;
        }
    }
    const bool iOmenenemys = enemiesInOmen >= Slider(DefensiveMenu, "Omenenemys", 2);
    const bool iTiamat = Bool(OffensiveMenu, "Tiamat");
    const bool iHydra = Bool(OffensiveMenu, "Hydra");
    const bool iYoumuu = Bool(OffensiveMenu, "Youmuu");
    (void)iBilge;
    (void)iBilgeEnemyhp;
    (void)iBilgemyhp;
    if (player.Distance(target.Position()) <= 450.0f && iBlade &&
        (iBladeEnemyhp || iBlademyhp) && CanUseItem(kItemBladeOfTheRuinedKing)) {
        UseItem(kItemBladeOfTheRuinedKing, target);
    }
    if (player.Distance(target.Position()) <= 300.0f && iTiamat && CanUseItem(kItemTiamat)) {
        UseItem(kItemTiamat);
    }
    if (player.Distance(target.Position()) <= 300.0f && iHydra && CanUseItem(kItemRavenousHydra)) {
        UseItem(kItemRavenousHydra);
    }
    if (player.Distance(target.Position()) <= 450.0f && iHydra && CanUseItem(kItemProfaneHydra)) {
        UseItem(kItemProfaneHydra);
    }
    if (iOmenenemys && iOmen && CanUseItem(kItemRanduins)) {
        UseItem(kItemRanduins);
    }
    if (player.Distance(target.Position()) <= 350.0f && iYoumuu && CanUseItem(kItemYoumuus)) {
        UseItem(kItemYoumuus);
    }
}

static void CastW(AIBaseClient target) {
    const auto player = Player();
    const auto& lastCast = SDK::LastCast::LastCastPacketSent();
    if (DelayW >= SDK::Variables::TickCount() - ShadowDelay ||
        GetShadowStage() != ShadowCastStage::First ||
        (target.HasBuff("zedulttargetmark") && lastCast.Slot == SpellSlot::R &&
         GetUltStage() == UltCastStage::Cooldown)) {
        return;
    }
    const auto herew = target.Position().Extend(player.Position(), -200.0f);
    CastPosition(W, herew, "cast-W", target);
    ShadowDelay = SDK::Variables::TickCount();
}

static void CastQ(AIBaseClient target) {
    const auto player = Player();
    if (!Q.IsReady() || !target.IsValid()) {
        return;
    }
    const auto wShadow = WShadow();
    if (wShadow.IsValid() && target.Distance(wShadow.Position()) <= 900.0f &&
        target.Distance(player.Position()) > 450.0f) {
        const auto shadowpred = Q.GetPrediction(target);
        Q.UpdateSourcePosition(wShadow.Position(), wShadow.Position());
        if (HitchanceAtLeast(shadowpred.Hitchance, HitChance::Medium)) {
            CastUnit(Q, target, "cast-Q-shadow");
        }
    } else {
        Q.UpdateSourcePosition(player.Position(), player.Position());
        const auto normalpred = Q.GetPrediction(target);
        if (normalpred.GetCastPosition().Distance(player.Position()) < 900.0f &&
            HitchanceAtLeast(normalpred.Hitchance, HitChance::Medium)) {
            CastUnit(Q, target, "cast-Q");
        }
    }
}

static void CastE() {
    AIHeroClient player;
    { ZedProbe p("CE-Player"); player = Player(); }
    bool eReady;
    { ZedProbe p("CE-IsReady"); eReady = E.IsReady(); }
    if (!eReady) {
        return;
    }
    AIMinionClient wShadow, rShadow;
    { ZedProbe p("CastE-shadow"); wShadow = WShadow(); rShadow = RShadow(); }
    int count = 0;
    { ZedProbe p("CastE-eloop");
    for (const auto& hero : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(hero) &&
            (hero.Distance(player.Position()) <= E.Range ||
             (wShadow.IsValid() && hero.Distance(wShadow.Position()) <= E.Range) ||
             (rShadow.IsValid() && hero.Distance(rShadow.Position()) <= E.Range))) {
            ++count;
        }
    }
    }
    if (count > 0) {
        CastSelf(E, "cast-E");
    }
}

static void KillSteal() {
    if (!ShouldRunNow(LastKillStealTick, 90)) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    const bool useIgnite = Bool(MiscMenu, "UseIgnitekill") &&
        Ignite.Slot != SpellSlot::Unknown && Ignite.IsReady();
    const bool useQ = Bool(MiscMenu, "UseQM") && Q.IsReady();
    const bool useE = Bool(MiscMenu, "UseEM") && E.IsReady();
    if (!useIgnite && !useQ && !useE) {
        return;
    }

    AIHeroClient target;
    { ZedProbe p("KS-GetTarget"); target = GetTarget(2000.0f, DamageType::Magical); }
    if (!ValidHeroTarget(target)) {
        return;
    }
    if (useIgnite) {
        ZedProbe p("KS-igniteDmg");
        const double igniteDmg = IgniteDamage(target);
        if (igniteDmg > target.Health() && player.Distance(target.Position()) <= 600.0f) {
            CastUnit(Ignite, target, "ks-ignite");
        }
    }
    if (useQ) {
        ZedProbe p("KS-Qdmg");
        const double qDamage = QDamage(target);
        if (qDamage > target.Health() ||
            CanCollectorExecuteAfterDamage(target, qDamage)) {
            if (player.Distance(target.Position()) <= Q.Range) {
                CastUnit(Q, target, "ks-Q-player");
            } else {
                const auto wShadow = WShadow();
                if (wShadow.IsValid() && wShadow.Distance(target.Position()) <= Q.Range) {
                    Q.UpdateSourcePosition(wShadow.Position(), wShadow.Position());
                    CastUnit(Q, target, "ks-Q-WShadow");
                } else {
                    const auto rShadow = RShadow();
                    if (rShadow.IsValid() && rShadow.Distance(target.Position()) <= Q.Range) {
                        Q.UpdateSourcePosition(rShadow.Position(), rShadow.Position());
                        CastUnit(Q, target, "ks-Q-RShadow");
                    }
                }
            }
        }
    }

    if (useE) {
        ZedProbe p("KS-Edmg-loop");
        const auto wShadow = WShadow();
        const auto rShadow = RShadow();
        for (const auto& t : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(t)) {
                continue;
            }
            const bool inERange =
                player.Distance(t.Position()) <= E.Range ||
                (wShadow.IsValid() && wShadow.Distance(t.Position()) <= E.Range) ||
                (rShadow.IsValid() && rShadow.Distance(t.Position()) <= E.Range);
            if (!inERange) {
                continue;
            }

            const double eDamage = EDamage(t);
            if (eDamage > t.Health() || CanCollectorExecuteAfterDamage(t, eDamage)) {
                CastSelf(E, "ks-E");
                return;
            }
        }
    }
}

static void OnDraw() {
    if (Bool(DrawingsMenu, "shadowd")) {
        const auto rShadow = RShadow();
        if (rShadow.IsValid()) {
            Drawing::DrawCircle(rShadow.Position(), rShadow.BoundingRadius() * 2.0f, 0xFF0000FFu, 1.5f, 48);
        }
        const auto wShadow = WShadow();
        if (wShadow.IsValid()) {
            if (GetShadowStage() == ShadowCastStage::Cooldown) {
                Drawing::DrawCircle(wShadow.Position(), wShadow.BoundingRadius() * 1.5f, 0xFFFF0000u, 1.5f, 48);
            } else if (wShadow.IsValid() && GetShadowStage() == ShadowCastStage::Second) {
                Drawing::DrawCircle(wShadow.Position(), wShadow.BoundingRadius() * 1.5f, 0xFFFFFF00u, 1.5f, 48);
            }
        }
    }

    if (Bool(DrawingsMenu, "damagetest")) {
        for (const auto& enemyVisible : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemyVisible)) {
                continue;
            }
            Vec2 worldToScreen{};
            if (!Drawing::WorldToScreen(enemyVisible.Position(), worldToScreen)) {
                continue;
            }
            const Vec2 textPos(worldToScreen.x + 50.0f, worldToScreen.y - 40.0f);
            const float comboDamage = ComboDamage(enemyVisible);
            if (comboDamage > enemyVisible.Health()) {
                Drawing::DrawText(textPos, "Combo=Rekt", 0xFFFF0000u);
            } else if (comboDamage +
                       Damage::GetAutoAttackDamage(Player(), enemyVisible) * 2.0 >
                       enemyVisible.Health()) {
                Drawing::DrawText(textPos, "Combo + 2 AA = Rekt", 0xFFFFA500u);
            } else {
                Drawing::DrawText(textPos, "Unkillable with combo + 2AA", 0xFF00FF00u);
            }
        }
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const float thickness = static_cast<float>(Slider(DrawingsMenu, "CircleThickness", 1));
    const int quality = Slider(DrawingsMenu, "CircleQuality", 100);
    const int segments = std::max(16, quality);
    if (Bool(DrawingsMenu, "CircleLag")) {
        if (Bool(DrawingsMenu, "DrawQ")) {
            Drawing::DrawCircle(player.Position(), Q.Range, 0xFF0000FFu, thickness, segments);
        }
        if (Bool(DrawingsMenu, "DrawE")) {
            Drawing::DrawCircle(player.Position(), E.Range, 0xFFFFFFFFu, thickness, segments);
        }
        if (Bool(DrawingsMenu, "DrawQW") && Key(HarassMenu, "longhar")) {
            Drawing::DrawCircle(player.Position(), 1400.0f, 0xFFFFFF00u, thickness, segments);
        }
        if (Bool(DrawingsMenu, "DrawR")) {
            Drawing::DrawCircle(player.Position(), R.Range, 0xFF0000FFu, thickness, segments);
        }
    } else {
        if (Bool(DrawingsMenu, "DrawQ")) {
            Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFFFFFFu, thickness, segments);
        }
        if (Bool(DrawingsMenu, "DrawE")) {
            Drawing::DrawCircle(player.Position(), E.Range, 0xFFFFFFFFu, thickness, segments);
        }
        if (Bool(DrawingsMenu, "DrawQW") && Key(HarassMenu, "longhar")) {
            Drawing::DrawCircle(player.Position(), 1400.0f, 0xFFFFFFFFu, thickness, segments);
        }
        if (Bool(DrawingsMenu, "DrawR")) {
            Drawing::DrawCircle(player.Position(), R.Range, 0xFFFFFFFFu, thickness, segments);
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Drawing::OnDraw -= &OnDraw;
    Events::hook.OnGameUpdate -= &OnGameUpdate;
    Events::hook.OnDoCast -= &OnDoCast;
    RemoveKeyPermashow(ComboMenu, "ActiveCombo");
    RemoveKeyPermashow(ComboMenu, "TheLine");
    RemoveKeyPermashow(HarassMenu, "longhar");
    RemoveKeyPermashow(HarassMenu, "ActiveHarass");
    RemoveKeyPermashow(LaneFarmMenu, "Activelane");
    RemoveKeyPermashow(LastHitMenu, "ActiveLast");
    RemoveKeyPermashow(JungleMenu, "Activejungle");

    Loaded = false;
}

} // namespace Plugins::AIO7UP::Zed
