#pragma once

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace Plugins::AIO7UP::Kalista {

using SDK::Core::Utils::AutoAttack;
using BuffType = SDK::BuffType;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* Eset = nullptr;
inline Menu* Rset = nullptr;
inline Menu* WowComboMenu = nullptr;
inline Menu* HarrassMenu = nullptr;
inline Menu* LaneClear = nullptr;
inline Menu* Misc = nullptr;
inline Menu* KsMenu = nullptr;
inline Menu* DebugMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1200.0f };
inline Spell NonCollisionQ{ SpellSlot::Q, 1200.0f };
inline Spell W{ SpellSlot::W, 5000.0f };
inline Spell E{ SpellSlot::E, 1000.0f };
inline Spell R{ SpellSlot::R, 1000.0f };
inline Spell Ignite{ SpellSlot::Unknown, 600.0f };

inline bool Loaded = false;
inline DWORD AATime = 0;
inline DWORD LastAATick = 0;
inline DWORD lastWTime = 0;
inline DWORD lastETime = 0;
inline DWORD LastForcusTime = 0;
inline DWORD LastForceTargetTick = 0;
inline DWORD LastRoutineTick = 0;
inline DWORD LastLogicETick = 0;
inline DWORD LastRLogicTick = 0;
inline DWORD LastKillstealTick = 0;
inline DWORD LastClearTick = 0;

inline bool OwnsForcedTarget = false;
inline AttackableUnit LastForcedTarget{};

inline std::map<float, float> IncomingDamageToSoulboundAlly;
inline std::map<float, float> InstantDamageOnSoulboundAlly;

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

static bool SliderButtonEnabled(Menu* menu, const char* key, bool fallback = false) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuSliderButton>(key);
    return item ? item->Enabled : fallback;
}

static int SliderButtonValue(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuSliderButton>(key);
    return item ? item->Value : fallback;
}

static int ListIndex(Menu* menu, const char* key, int fallback = 0) {
    if (!menu) {
        return fallback;
    }
    const auto* item = menu->Get<MenuList>(key);
    return item ? item->Index : fallback;
}

static bool Key(Menu* menu, const char* key, bool fallback = false) {
    if (!menu || !Game::ShouldProcessInput()) {
        return fallback;
    }
    const auto* item = menu->Get<MenuKeyBind>(key);
    return item ? item->Active : fallback;
}

static void RemoveKeyPermashow(Menu* menu, const char* key) {
    if (auto* item = menu ? menu->Get<MenuKeyBind>(key) : nullptr) {
        item->RemovePermashow();
    }
}

static bool ShouldRunNow(DWORD& lastTick, DWORD intervalMs) {
    const DWORD now = GetTickCount();
    if (lastTick != 0 && now - lastTick < intervalMs) {
        return false;
    }

    lastTick = now;
    return true;
}

static bool EqualsIgnoreCase(const char* left, const char* right) {
    return left && right && left[0] && right[0] && _stricmp(left, right) == 0;
}

static bool ContainsIgnoreCase(const std::string& value, const char* needle) {
    if (value.empty() || !needle || !needle[0]) {
        return false;
    }

    std::string lowerValue = value;
    std::string lowerNeedle = needle;
    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::transform(lowerNeedle.begin(), lowerNeedle.end(), lowerNeedle.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowerValue.find(lowerNeedle) != std::string::npos;
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
    if (::Core::Objects::ReadCharacterName(
            unit.Address(),
            direct,
            static_cast<int>(sizeof(direct))) &&
        direct[0]) {
        return direct;
    }

    if (::Core::Objects::ReadName(
            unit.Address(),
            direct,
            static_cast<int>(sizeof(direct))) &&
        direct[0]) {
        return direct;
    }

    return {};
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
    return spell.Cast(target) == CastStates::SuccessfullyCasted;
}

static AIHeroClient GetTarget(float range, DamageType damageType = DamageType::Physical) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static float HealthPred(const AIBaseClient& unit, int ms) {
    return unit.IsValid() ? Prediction::Health::GetPrediction(unit, ms) : 0.0f;
}

static float HealthRegenRate(const AIBaseClient& unit) {
    return unit.IsValid() ? ::CoreAIHeroClient::HealthRegenRate(unit.Address()) : 0.0f;
}

static bool HasBuffType(const AIBaseClient& unit, BuffType type) {
    return unit.IsValid() && SDK::HasBuffOfType(unit, type);
}

static bool HasRendBuff(const AIBaseClient& target, float range) {
    return ValidTarget(target, range) && target.HasBuff("kalistaexpungemarker");
}

static bool DebugRendEnabled() {
    return Bool(DebugMenu, "debugRendBuff", false);
}

static void DebugLogLine(const char* fmt, ...) {
    char buffer[1536] = {};

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    int prefix = _snprintf_s(
        buffer, sizeof(buffer), _TRUNCATE,
        "[%02u:%02u:%02u.%03u] ",
        static_cast<unsigned>(st.wHour),
        static_cast<unsigned>(st.wMinute),
        static_cast<unsigned>(st.wSecond),
        static_cast<unsigned>(st.wMilliseconds));
    if (prefix < 0) prefix = 0;

    va_list args;
    va_start(args, fmt);
    _vsnprintf_s(buffer + prefix, sizeof(buffer) - prefix, _TRUNCATE, fmt, args);
    va_end(args);

    const size_t len = strlen(buffer);
    if (len + 2 < sizeof(buffer)) {
        buffer[len] = '\r';
        buffer[len + 1] = '\n';
        buffer[len + 2] = '\0';
    }

    OutputDebugStringA(buffer);

    HANDLE hFile = CreateFileA(
        "C:\\Users\\Public\\kalista_debug.log",
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, buffer, static_cast<DWORD>(strlen(buffer)), &written, nullptr);
        CloseHandle(hFile);
    }
}

static void DebugLogRendBuff(const AIBaseClient& target, const char* context) {
    if (!DebugRendEnabled() || !target.IsValid()) {
        return;
    }

    const bool hasBuff = target.HasBuff("kalistaexpungemarker");
    const int sdkStacks = target.GetBuffCount("kalistaexpungemarker");

    int rawStacks38 = -1;
    int rawStacks3C = -1;
    float endTime = -1.0f;
    const auto buffRef = ::CoreBuffs::FindRawByName(target.Address(), "kalistaexpungemarker");
    if (buffRef.IsValid()) {
        rawStacks38 = ::Globals::Read<int>(buffRef.address + Offset::BuffDataLayout::BuffStacks);
        rawStacks3C = ::Globals::Read<int>(buffRef.address + Offset::BuffDataLayout::BuffStacksAlt);
        endTime = buffRef.GetEndTime();
    }

    const char* name = "?";
    const std::string charName = RuntimeCharacterName(target);
    if (!charName.empty()) {
        name = charName.c_str();
    }

    DebugLogLine(
        "[%s] target=%s hasBuff=%d sdkStacks=%d raw38=%d raw3C=%d endTime=%.2f hp=%.1f dist=%.1f",
        context,
        name,
        hasBuff ? 1 : 0,
        sdkStacks,
        rawStacks38,
        rawStacks3C,
        endTime,
        target.Health(),
        target.DistanceToPlayer());
}

static bool IsHardCcOrSlow(const AIBaseClient& target) {
    return HasBuffType(target, BuffType::Asleep) ||
           HasBuffType(target, BuffType::Charm) ||
           HasBuffType(target, BuffType::Fear) ||
           HasBuffType(target, BuffType::Knockup) ||
           HasBuffType(target, BuffType::Slow) ||
           HasBuffType(target, BuffType::Stun);
}

static bool IsProtectionOrUnkillable(const AIBaseClient& target) {
    if (!target.IsValid() || target.IsInvulnerable()) {
        return true;
    }

    static constexpr const char* kBuffs[] = {
        "kindredrnodeathbuff",
        "UndyingRage",
        "JudicatorIntervention",
        "ChronoShift",
        "FioraW",
        "ShroudofDarkness",
        "SivirShield",
        "itemmagekillerveil",
        "bansheesveil",
        nullptr
    };

    for (int i = 0; kBuffs[i]; ++i) {
        if (target.HasBuff(kBuffs[i])) {
            return true;
        }
    }

    return HasBuffType(target, BuffType::SpellShield) ||
           HasBuffType(target, BuffType::Invulnerability) ||
           HasBuffType(target, BuffType::UnKillable);
}

static bool IsEpicJungleMob(const AIMinionClient& minion) {
    const auto type = minion.GetJungleType();
    if (type == JungleType::Legendary || type == JungleType::Epic) {
        return true;
    }

    const std::string name = RuntimeCharacterName(minion);
    return ContainsIgnoreCase(name, "dragon") ||
           ContainsIgnoreCase(name, "baron") ||
           ContainsIgnoreCase(name, "riftherald") ||
           ContainsIgnoreCase(name, "voidgrub") ||
           ContainsIgnoreCase(name, "atakhan") ||
           ContainsIgnoreCase(name, "sru_sentinel");
}

static bool IsLargeOrEpicJungleMob(const AIMinionClient& minion) {
    const auto type = minion.GetJungleType();
    return type == JungleType::Large ||
           type == JungleType::Legendary ||
           type == JungleType::Epic ||
           IsEpicJungleMob(minion);
}

static int JunglePriority(const AIMinionClient& minion) {
    const auto type = minion.GetJungleType();
    if (type == JungleType::Legendary) {
        return 5000;
    }
    if (type == JungleType::Epic || IsEpicJungleMob(minion)) {
        return 4000;
    }
    if (type == JungleType::Large) {
        return 3000;
    }
    if (type == JungleType::Small) {
        return 1000;
    }
    return 0;
}

static bool CastE(const char* action = "E") {
    (void)action;
    if (!E.IsReady()) {
        return false;
    }

    const DWORD now = GetTickCount();
    const DWORD gate = 500u + static_cast<DWORD>(std::max(0, Game::Ping()));
    if (lastETime != 0 && now - lastETime <= gate) {
        return false;
    }

    if (E.Cast()) {
        lastETime = now;
        return true;
    }
    return false;
}

static void SetForcedTarget(const AttackableUnit& target) {
    if (!ValidUnit(target)) {
        return;
    }

    Orbwalker::ForceTarget(target);
    LastForcedTarget = target;
    OwnsForcedTarget = true;
}

static void ClearForcedTarget() {
    if (!OwnsForcedTarget) {
        return;
    }

    Orbwalker::ForceTarget(AttackableUnit());
    LastForcedTarget = AttackableUnit();
    OwnsForcedTarget = false;
}

static AttackableUnit SelectOrbwalkerForcedTarget() {
    const auto player = Player();
    if (!player.IsValid()) {
        return AttackableUnit();
    }

    const float aaRange = AutoAttack::GetRealAutoAttackRange(player);
    const Vector3 cursor = Game::CursorPos();
    AttackableUnit best;
    float bestCursorDistance = FLT_MAX;
    int bestPriority = -1;
    static constexpr float kForceTargetCursorRadius = 160.0f;

    auto consider = [&](const AIBaseClient& unit, int priority) {
        if (!ValidTarget(unit, aaRange) || !AutoAttack::InAutoAttackRange(unit)) {
            return;
        }

        const float cursorDistance = unit.Distance(cursor);
        if (cursorDistance > kForceTargetCursorRadius) {
            return;
        }

        if (!best.IsValid() ||
            cursorDistance < bestCursorDistance - 1.0f ||
            (std::fabs(cursorDistance - bestCursorDistance) <= 1.0f && priority > bestPriority)) {
            best = AttackableUnit(unit.Handle());
            bestCursorDistance = cursorDistance;
            bestPriority = priority;
        }
    };

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        consider(enemy, 3000);
    }

    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (minion.IsJungle() || minion.IsPlant() || minion.IsPet() || minion.IsClone()) {
            continue;
        }
        consider(minion, 1000);
    }

    for (const auto& mob : GameObjects::Jungle()) {
        if (!mob.IsVisible() || mob.IsPlant() || mob.IsPet() || mob.IsClone()) {
            continue;
        }
        consider(mob, 2000 + JunglePriority(mob));
    }

    return best;
}

static void UpdateForcedOrbwalkerMinionTarget() {
    if (Orbwalker::ActiveMode() != OrbwalkingMode::Combo || !Bool(ComboMenu, "orbminion")) {
        ClearForcedTarget();
        return;
    }

    if (!ShouldRunNow(LastForceTargetTick, 60)) {
        return;
    }

    const AttackableUnit target = SelectOrbwalkerForcedTarget();
    if (ValidUnit(target)) {
        SetForcedTarget(target);
    } else {
        ClearForcedTarget();
    }
}

static double QDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid() || Q.Level() <= 0) {
        return 0.0;
    }

    // CDragon latest KalistaMysticShot: BaseDamage + TotalADRatio * total AD.
    // Raw DataValues include rank-0 sentinels, so spell level indexes directly.
    static constexpr float qBase[] = { -55.0f, 10.0f, 75.0f, 140.0f, 205.0f, 270.0f, 335.0f };
    const int level = std::clamp(Q.Level(), 1, 5);
    const float raw = qBase[level] + 1.0499999523162842f * player.AD();
    return player.CalculatePhysicalDamage(target, std::max(0.0f, raw));
}

static double GetEDamage(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid() || E.Level() <= 0) {
        return 0.0;
    }

    const int stacks = target.GetBuffCount("kalistaexpungemarker");
    if (stacks <= 0) {
        return 0.0;
    }

    // CDragon latest KalistaExpungeWrapper:
    // NormalDamage = BaseDamage + BaseADRatio * total AD + APRatio * AP.
    // AdditionalDamage = AdditionalBaseDamage + AdditionalADRatio * total AD + AdditionalAPRatio * AP.
    // Raw DataValues include rank-0 sentinels, so spell level indexes directly.
    static constexpr float eBaseDamage[] = { 0.0f, 5.0f, 15.0f, 25.0f, 35.0f, 45.0f };
    static constexpr float eAdditionalBaseDamage[] = { 0.0f, 7.0f, 14.0f, 21.0f, 28.0f, 35.0f };
    static constexpr float eAdditionalAdRatio[] = {
        0.0f,
        0.2000000030f,
        0.2750000060f,
        0.3499999940f,
        0.4250000119f,
        0.5f
    };

    const int level = std::clamp(E.Level(), 1, 5);
    const float normal =
        eBaseDamage[level] +
        0.6999999881f * player.AD() +
        0.6499999762f * player.AP();
    const float additional =
        eAdditionalBaseDamage[level] +
        eAdditionalAdRatio[level] * player.AD() +
        0.5f * player.AP();
    const float raw = normal + additional * static_cast<float>(stacks - 1);

    // NO epic-monster halving. The season-5-era C# source halved Rend vs
    // Legendary monsters (`total /= 2` when `(GetJungleType() & Legendary) != 0`,
    // which covers Baron/Dragon/Herald/Voidgrubs), but that reduction was REMOVED
    // from live League: verified in-game that Rend deals FULL damage to Baron and
    // Dragon — keeping the /2 made the estimate fall short (hụt), so E never
    // secured the objective. CDragon `EpicMonsterDamageMod = 0.5` is a leftover
    // DataValue that is NOT referenced by KalistaExpunge mSpellCalculations
    // (unused stub), so we do not apply it. If a future patch re-wires it, restore
    // `raw *= 0.5f` for JungleType::Legendary/Epic here.
    const double finalDamage = player.CalculatePhysicalDamage(target, std::max(0.0f, raw));

    if (DebugRendEnabled()) {
        const char* name = "?";
        const std::string charName = RuntimeCharacterName(target);
        if (!charName.empty()) {
            name = charName.c_str();
        }

        DebugLogLine(
            "[GetEDamage] target=%s stacks=%d level=%d raw=%.1f final=%.1f ad=%.1f ap=%.1f hp=%.1f",
            name,
            stacks,
            level,
            raw,
            finalDamage,
            player.AD(),
            player.AP(),
            target.Health());
    }

    return finalDamage;
}

static double EDamage(const AIBaseClient& target) {
    return GetEDamage(target);
}

static double GetRealDamage(const AIBaseClient& target) {
    // CalculatePhysicalDamage already applies DamageReductionMod which includes
    // Exhaust (0.6x) and FerociousHowl. Do NOT re-apply them here — that would
    // double-reduce damage (e.g. Exhaust: 0.6 * 0.6 = 0.36 instead of 0.6).
    return GetEDamage(target);
}

static double GetKalistaRealDamage(const AIBaseClient& target, bool useTolerance, int tolerance) {
    if (!HasRendBuff(target, E.Range) || IsProtectionOrUnkillable(target)) {
        return 0.0;
    }

    double damage = GetRealDamage(target);
    if (useTolerance) {
        damage += static_cast<double>(tolerance);
    }
    return std::max(0.0, damage);
}

static bool RendKillable(const AIBaseClient& target, bool useTolerance = false, int tolerance = 0) {
    if (!HasRendBuff(target, E.Range)) {
        return false;
    }

    const double damage = GetKalistaRealDamage(target, useTolerance, tolerance);
    const float predicted = HealthPred(target, target.IsMinion() ? 250 : 150);
    const float health = predicted > 0.0f ? predicted : target.Health();
    return damage > 0.0 && static_cast<double>(health + target.PhysicalShield()) < damage - 2.0;
}

static float AllIncomingDamageToSoulbound() {
    float total = 0.0f;
    for (const auto& entry : IncomingDamageToSoulboundAlly) {
        total += entry.second;
    }
    for (const auto& entry : InstantDamageOnSoulboundAlly) {
        total += entry.second;
    }
    return total;
}

static void PurgeIncomingDamageToSoulbound() {
    const float now = Events::GameTime();
    for (auto it = IncomingDamageToSoulboundAlly.begin(); it != IncomingDamageToSoulboundAlly.end();) {
        if (it->first < now) {
            it = IncomingDamageToSoulboundAlly.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = InstantDamageOnSoulboundAlly.begin(); it != InstantDamageOnSoulboundAlly.end();) {
        if (it->first < now) {
            it = InstantDamageOnSoulboundAlly.erase(it);
        } else {
            ++it;
        }
    }
}

static AIHeroClient FindSoulboundAlly() {
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!ally.IsMe() &&
            !ally.IsDead() &&
            ally.HasBuff("kalistacoopstrikeally")) {
            return ally;
        }
    }
    return AIHeroClient();
}

static bool HeroNameEquals(const AIHeroClient& hero, const char* name) {
    if (!hero.IsValid() || !name || !name[0]) {
        return false;
    }

    const std::string runtime = RuntimeCharacterName(hero);
    return EqualsIgnoreCase(runtime.c_str(), name);
}

static void CastKalistaQ(const AIBaseClient& target, HitChance hitChance, const char* action) {
    if (!Q.IsReady() || !ValidTarget(target, Q.Range)) {
        return;
    }

    const auto pred = Q.GetPrediction(target, false, -1.0f, Q.CollisionObjects);
    if (!pred.CollisionObjects.empty()) {
        return;
    }

    if (HitchanceAtLeast(pred.Hitchance, hitChance)) {
        CastPosition(Q, pred.GetCastPosition(), action, target);
    }
}

static AIHeroClient GetBestTarget(float range) {
    if (Bool(ComboMenu, "AttackW", false)) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy, range) && enemy.HasBuff("kalistacoopstrikemarkally")) {
                return enemy;
            }
        }
    }
    return GetTarget(range, DamageType::Physical);
}

static int GetQCollision(const Vector3& from, const Vector3& to) {
    if (!from.IsValid() || !to.IsValid()) {
        return 0;
    }

    const Vector2 start = from.To2D();
    const Vector2 end = to.To2D();
    const Vector2 segment = end - start;
    const float lengthSqr = segment.LengthSqr();
    if (lengthSqr <= 1e-6f) {
        return 0;
    }

    int count = 0;
    auto countUnit = [&](const AIBaseClient& unit) {
        if (!ValidTarget(unit, Q.Range)) {
            return;
        }
        const Vector2 point = unit.Position().To2D();
        const float t = std::clamp((point - start).Dot(segment) / lengthSqr, 0.0f, 1.0f);
        const Vector2 projection = start + segment * t;
        const float width = Q.Width + unit.BoundingRadius();
        if (point.DistanceSqr(projection) <= width * width) {
            ++count;
        }
    };

    for (const auto& minion : GameObjects::EnemyMinions()) {
        countUnit(minion);
    }
    for (const auto& mob : GameObjects::Jungle()) {
        if (mob.IsVisible() && !mob.IsPlant() && !mob.IsPet() && !mob.IsClone()) {
            countUnit(mob);
        }
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        countUnit(enemy);
    }
    return count;
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnEndScene();
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void OnAfterAttack(OrbwalkingActionArgs& args);
static void OnNonKillableMinion(OrbwalkingActionArgs& args);
static void OnProcessSpellCast(const ProcessSpellEventArgs& args);
static void OnPlayAnimation(const PlayAnimationEventArgs& args);
static void Combo();
static void Harass();
static void Clear();
static void FlyHack();
static void LogicE();
static void RLogic();
static void Routine();
static void Killsteal();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.7upaio", "7UP - Kalista", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo", "Combo"));
    ComboMenu->Add(new MenuKeyBind("FlyHack", "Fly Hack", SDK::Keys::T, KeyBindType::Toggle))->Permashow();
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("disQ", "Block on high aa speed"));
    ComboMenu->Add(new MenuBool("useE", "Use E"));
    ComboMenu->Add(new MenuBool("disE1", "Block on Debuff"));
    ComboMenu->Add(new MenuBool("disE2", "Limit usage"));
    ComboMenu->Add(new MenuBool("orbminion", "Orbwalker Minion"));

    Eset = MenuRoot->AddSubMenu(new Menu("Eset", "Eset"));
    Eset->Add(new MenuList("EMode", "Use E Mode", { "Only Combo", "Always", "Disable" }, 1));
    Eset->Add(new MenuBool("harassPlus", "Auto Kill Minion && Any Enemy Have E Buff"));

    Rset = MenuRoot->AddSubMenu(new Menu("Rset", "Rset"));
    WowComboMenu = Rset->AddSubMenu(new Menu("WowCombo", "WowCombo"));
    WowComboMenu->Add(new MenuBool("Balista", "Balista"));
    WowComboMenu->Add(new MenuBool("Salista", "Salista"));
    WowComboMenu->Add(new MenuBool("Talista", "Talista"));
    Rset->Add(new MenuBool("kaliusersaveally", "Use R to save Soulbound"));
    Rset->Add(new MenuBool("userengage", "Use R to engage"));

    HarrassMenu = MenuRoot->AddSubMenu(new Menu("Harass", "Harass"));
    HarrassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarrassMenu->Add(new MenuBool("QMinion", "Use Q on Minion"));
    HarrassMenu->Add(new MenuBool("useE", "Use E"));
    HarrassMenu->Add(new MenuBool("disE1", "Block on Debuff"));
    HarrassMenu->Add(new MenuBool("disE2", "Limit usage"));
    HarrassMenu->Add(new MenuSlider("Mana", "Mana", 50, 0, 100));

    LaneClear = MenuRoot->AddSubMenu(new Menu("LaneClear", "Lane Clear"));
    LaneClear->Add(new MenuBool("useQ", "Use Q for jungle"));
    LaneClear->Add(new MenuBool("useE", "Use E"));
    LaneClear->Add(new MenuSlider("MinE", "Use Kill min minion Count", 2, 1, 5));
    LaneClear->Add(new MenuSlider("Mana", "Don't Lane/Jung if Mana <= X%", 40, 0, 100));

    Misc = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    Misc->Add(new MenuBool("misc-prevent-e", "Prevent E on Spellshields & Invulnerable"));
    Misc->Add(new MenuBool("misc-dying-e", "E before dying"));
    Misc->Add(new MenuSlider("misc-dying-e-pro", "E dying on %", 10, 1, 50));
    Misc->Add(new MenuBool("misc-leaving-e", "E when leaving range"));
    Misc->Add(new MenuSlider("misc-leaving-e-pro", "E leaving stacks", 5, 1, 10));
    Misc->Add(new MenuKeyBind("misc-ward-trick", "Auto W", SDK::Keys::G, KeyBindType::Toggle))->Permashow();
    Misc->Add(new MenuBool("Forcus", "Forcus Attack"));
    Misc->Add(new MenuBool("drawE", "Draw E Damage", false));
    Misc->Add(new MenuSliderButton("EToler", "Enabled E Toler DMG", 0, -100, 110, true));

    KsMenu = MenuRoot->AddSubMenu(new Menu("KS Settings", "KS"));
    KsMenu->Add(new MenuBool("KSQ", "Use Q KS"));
    KsMenu->Add(new MenuBool("KSE", "Use E KS"));
    KsMenu->Add(new MenuBool("KSEJG", "Use E KS JG"));

    DebugMenu = MenuRoot->AddSubMenu(new Menu("Debug", "Debug"));
    DebugMenu->Add(new MenuBool("debugRendBuff", "Log Rend Buff to file", false));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    // CDragon latest KalistaMysticShotMissile: range 1200, cast time
    // 0.250999987, missile width 20, missile speed 3000. Keep default Q
    // collision objects so minions, heroes, and Yasuo wall are all checked.
    Q = Spell(SpellSlot::Q, 1200.0f);
    Q.SetSkillshot(0.250999987f, 20.0f, 3000.0f, true, SpellType::Line);
    Q.DamageType = DamageType::Physical;
    NonCollisionQ = Spell(SpellSlot::Q, 1200.0f);
    NonCollisionQ.SetSkillshot(0.250999987f, 20.0f, 3000.0f, false, SpellType::Line);
    NonCollisionQ.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, 5000.0f);
    E = Spell(SpellSlot::E, 1000.0f);
    E.DamageType = DamageType::Physical;
    R = Spell(SpellSlot::R, 1000.0f);
    Ignite = Spell(player.GetSpellSlot("summonerdot"), 600.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpellCast;
    Events::hook.OnPlayAnimation += &OnPlayAnimation;
    Drawing::OnEndScene += &OnEndScene;
    Orbwalker::OnAfterAttack += &OnAfterAttack;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Orbwalker::OnNonKillableMinion += &OnNonKillableMinion;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>7UP - Kalista loaded</font>");
}

static void OnEndScene() {
    const auto player = Player();
    if (!Bool(Misc, "drawE", false) ||
        !player.IsValid() ||
        player.IsDead() ||
        Game::IsChatOpen() ||
        !E.IsReady()) {
        return;
    }

    for (const auto& target : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(target) ||
            !target.IsVisibleOnScreen() ||
            !target.HasBuff("kalistaexpungemarker")) {
            continue;
        }

        const double damage = GetEDamage(target);
        if (damage <= 0.0 || target.MaxHealth() <= 0.0f) {
            continue;
        }

        const auto barPos = Drawing::HpBarScreenPos(target);
        float xPos = barPos.x - 45.0f;
        float yPos = barPos.y - 19.0f;
        if (HeroNameEquals(target, "Annie")) {
            yPos += 2.0f;
        }

        const float remainHealth = std::max(0.0f, target.Health() - static_cast<float>(damage));
        const float x1 = xPos + (target.Health() / target.MaxHealth() * 104.0f);
        const float x2 = xPos + (remainHealth / target.MaxHealth() * 103.4f);
        Drawing::DrawLine(x1, yPos, x2, yPos, 11.0f, 0xFFFF9300u);
    }
}

static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Bool(Misc, "Forcus") || !SDK::CanMove(Player())) {
        return;
    }

    const AIBaseClient currentTarget(args.Target.Handle());
    if (!ValidUnit(currentTarget)) {
        return;
    }

    const auto mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::Combo || mode == OrbwalkingMode::Harass) {
        for (const auto& target : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(target, AutoAttack::GetRealAutoAttackRange(target)) ||
                !target.HasBuff("kalistacoopstrikemarkally")) {
                continue;
            }

            SetForcedTarget(target);
            LastForcusTime = GetTickCount();
            return;
        }
    } else if (mode == OrbwalkingMode::LaneClear) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!ValidTarget(minion, AutoAttack::GetRealAutoAttackRange(minion)) ||
                !minion.HasBuff("kalistacoopstrikemarkally")) {
                continue;
            }

            SetForcedTarget(minion);
            LastForcusTime = GetTickCount();
            return;
        }
    }
}

static void OnAfterAttack(OrbwalkingActionArgs& args) {
    AATime = GetTickCount();
    ClearForcedTarget();

    const bool comboQ = Bool(ComboMenu, "useQ");
    const bool harassQ = Bool(HarrassMenu, "useQ");
    const bool jungleQ = Bool(LaneClear, "useQ");
    const int mana = Slider(HarrassMenu, "Mana", 50);

    const AIBaseClient targetBase(args.Target.Handle());
    const auto player = Player();
    if (!ValidUnit(targetBase) || player.IsDead()) {
        return;
    }

    DebugLogRendBuff(targetBase, "OnAfterAttack");

    if (!Q.IsReady()) {
        return;
    }

    if (targetBase.IsHero()) {
        const AIHeroClient target(targetBase.Handle());
        if (!ValidHeroTarget(target, Q.Range)) {
            return;
        }

        if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo && comboQ) {
            CastKalistaQ(target, HitChance::High, "after-attack-Q-combo");
        } else if ((Orbwalker::ActiveMode() == OrbwalkingMode::Harass ||
                    Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear) &&
                   player.ManaPercent() < static_cast<float>(mana) &&
                   harassQ) {
            CastKalistaQ(target, HitChance::High, "after-attack-Q-harass");
        }
        return;
    }

    if (targetBase.IsMinion() &&
        Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear &&
        player.ManaPercent() < static_cast<float>(mana) &&
        jungleQ) {
        const AIMinionClient mob(targetBase.Handle());
        if (mob.IsValid() && mob.GetJungleType() != JungleType::Unknown && ValidTarget(mob, Q.Range)) {
            CastUnit(Q, mob, "after-attack-Q-jungle");
        }
    }
}

static void OnNonKillableMinion(OrbwalkingActionArgs& args) {
    const auto player = Player();
    if (!Bool(LaneClear, "useE") ||
        !E.IsReady() ||
        player.HasBuff("summonerexhaust") ||
        player.Mana() - 40.0f < 40.0f) {
        return;
    }

    const AIBaseClient unit(args.Target.Handle());
    if (!ValidTarget(unit, E.Range) || !RendKillable(unit)) {
        return;
    }

    const auto mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::LaneClear ||
        mode == OrbwalkingMode::Harass ||
        mode == OrbwalkingMode::Combo) {
        CastE("non-killable-minion-E");
    }
}

static void OnProcessSpellCast(const ProcessSpellEventArgs& args) {
    const std::string spellName = args.SpellName[0] ? args.SpellName : args.ScriptName;
    if (ContainsIgnoreCase(spellName, "talonshadow")) {
        const auto player = Player();
        if (SDK::Items::HasItem(player, 3364) && SDK::Items::CanUseItem(player, 3364)) {
            SDK::Items::UseItem(player, 3364);
        } else if (SDK::Items::HasItem(player, 2055)) {
            SDK::Items::UseItem(player, 2055, Game::CursorPos());
        }
    }

    if (Events::IsLocalPlayer(args.Sender)) {
        SDK::Utils::DelayAction::Add(100, []() {
            Orbwalker::ResetAutoAttackTimer();
        });
    }
}

static void OnPlayAnimation(const PlayAnimationEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender) || !EqualsIgnoreCase(args.Animation, "Spell3")) {
        return;
    }

    Game::SendEmote(EmoteId::Dance);
    if (args.Process) {
        *args.Process = false;
    }
}

static void Combo() {
    UpdateForcedOrbwalkerMinionTarget();

    const bool useQ = Bool(ComboMenu, "useQ");
    const bool disQ = Bool(ComboMenu, "disQ");
    const bool useE = Bool(ComboMenu, "useE");
    const bool disE1 = Bool(ComboMenu, "disE1");
    const bool disE2 = Bool(ComboMenu, "disE2");
    const bool harassPlus = Bool(Eset, "harassPlus");

    const auto player = Player();
    const AIHeroClient target = GetTarget(Q.Range, DamageType::Physical);
    if (!ValidHeroTarget(target, Q.Range)) {
        return;
    }

    if (useQ && Q.IsReady()) {
        if (!disQ || player.AttackSpeedMod() < 1.98f) {
            CastKalistaQ(target, HitChance::High, "combo-Q");
        }
    }

    if (useE && E.IsReady() && ValidHeroTarget(target, E.Range)) {
        if (target.Health() < GetEDamage(target) && !IsProtectionOrUnkillable(target)) {
            CastE("combo-E-kill");
        }

        if (harassPlus &&
            target.DistanceToPlayer() > player.AttackRange() + player.BoundingRadius() + 100.0f &&
            ValidHeroTarget(target, E.Range)) {
            for (const auto& minion : GameObjects::EnemyMinions()) {
                if (ValidTarget(minion, AutoAttack::GetRealAutoAttackRange(minion)) &&
                    HasRendBuff(minion, E.Range) &&
                    minion.Health() < GetEDamage(minion)) {
                    CastE("combo-E-harass-plus");
                    break;
                }
            }
        }

        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!HasRendBuff(enemy, E.Range)) {
                continue;
            }

            if (disE1 && IsHardCcOrSlow(enemy)) {
                continue;
            }

            bool killableMinion = false;
            for (const auto& minion : GameObjects::EnemyMinions()) {
                if (HasRendBuff(minion, E.Range) && minion.Health() <= GetEDamage(minion)) {
                    killableMinion = true;
                    break;
                }
            }

            if (killableMinion) {
                if (!disE2 || SDK::Variables::TickCount() - E.LastCastAttemptT > 2500) {
                    CastE("combo-E-minion");
                }
                break;
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    const int mana = Slider(HarrassMenu, "Mana", 50);
    if (player.ManaPercent() < static_cast<float>(mana)) {
        return;
    }

    const bool useQ = Bool(HarrassMenu, "useQ");
    const bool useQMinion = Bool(HarrassMenu, "QMinion");
    const bool useE = Bool(HarrassMenu, "useE");
    const bool disE1 = Bool(HarrassMenu, "disE1");
    const bool disE2 = Bool(HarrassMenu, "disE2");

    const AIHeroClient target = GetTarget(Q.Range, DamageType::Physical);
    if (!ValidHeroTarget(target, Q.Range)) {
        return;
    }

    if (useQ && Q.IsReady()) {
        const auto pred = Q.GetPrediction(target, false, -1.0f, Q.CollisionObjects);
        if (pred.CollisionObjects.empty() && HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
            CastPosition(Q, pred.GetCastPosition(), "harass-Q", target);
        } else if (useQMinion && !pred.CollisionObjects.empty()) {
            bool allCollisionKillable = true;
            bool hasKillableCollision = false;
            for (const auto& collision : pred.CollisionObjects) {
                if (!collision.IsMinion() && !collision.IsHero()) {
                    allCollisionKillable = false;
                    break;
                }

                if (ValidTarget(collision, Q.Range) && collision.Health() < QDamage(collision)) {
                    hasKillableCollision = true;
                } else {
                    allCollisionKillable = false;
                    break;
                }
            }

            if (allCollisionKillable && hasKillableCollision) {
                const auto nonCollisionPred =
                    NonCollisionQ.GetPrediction(target, false, -1.0f, NonCollisionQ.CollisionObjects);
                if (HitchanceAtLeast(nonCollisionPred.Hitchance, HitChance::High)) {
                    CastPosition(NonCollisionQ, nonCollisionPred.GetCastPosition(), "harass-Q-through-minion", target);
                }
            }
        }
    }

    if (useE && E.IsReady()) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!HasRendBuff(enemy, E.Range)) {
                continue;
            }
            if (disE1 && IsHardCcOrSlow(enemy)) {
                continue;
            }

            bool killableMinion = false;
            for (const auto& minion : GameObjects::EnemyMinions()) {
                if (HasRendBuff(minion, E.Range) && minion.Health() <= GetEDamage(minion)) {
                    killableMinion = true;
                    break;
                }
            }

            if (killableMinion) {
                if (!disE2 || SDK::Variables::TickCount() - E.LastCastAttemptT > 2500) {
                    CastE("harass-E-minion");
                }
                break;
            }
        }
    }
}

static void Clear() {
    const auto player = Player();
    const int waveMana = Slider(LaneClear, "Mana", 40);
    if (player.ManaPercent() < static_cast<float>(waveMana)) {
        return;
    }

    if (!ShouldRunNow(LastClearTick, 90)) {
        return;
    }

    const bool useE = Bool(LaneClear, "useE");
    const int minE = Slider(LaneClear, "MinE", 2);
    const bool qJungle = Bool(LaneClear, "useQ");

    if (useE && E.IsReady()) {
        int killableMinions = 0;
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (HasRendBuff(minion, E.Range) && minion.Health() < GetEDamage(minion)) {
                ++killableMinions;
            }
        }

        if (killableMinions >= minE) {
            CastE("clear-E-minions");
            return;
        }
    }

    if (qJungle && Q.IsReady()) {
        auto mobs = GameObjects::Jungle();
        mobs.erase(
            std::remove_if(
                mobs.begin(),
                mobs.end(),
                [](const AIMinionClient& mob) {
                    return !ValidTarget(mob, Q.Range) ||
                           !mob.IsVisible() ||
                           mob.IsPlant() ||
                           mob.IsPet() ||
                           mob.IsClone();
                }),
            mobs.end());
        std::sort(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& a, const AIMinionClient& b) {
                const int aPriority = JunglePriority(a);
                const int bPriority = JunglePriority(b);
                if (aPriority != bPriority) {
                    return aPriority > bPriority;
                }
                if (std::fabs(a.MaxHealth() - b.MaxHealth()) > 1.0f) {
                    return a.MaxHealth() > b.MaxHealth();
                }
                return a.DistanceToPlayer() < b.DistanceToPlayer();
            });

        for (const auto& mob : mobs) {
            const auto pred = Q.GetPrediction(mob, false, -1.0f, Q.CollisionObjects);
            if (pred.CollisionObjects.empty() &&
                HitchanceAtLeast(pred.Hitchance, HitChance::High) &&
                pred.GetCastPosition().Distance2D(player.Position()) <= Q.Range) {
                CastPosition(Q, pred.GetCastPosition(), "clear-Q-jungle-ezreal", mob);
                return;
            }
        }
    }

    if (useE && E.IsReady()) {
        for (const auto& mob : GameObjects::Jungle()) {
            if (!HasRendBuff(mob, E.Range) ||
                !mob.IsVisible() ||
                mob.IsPlant() ||
                mob.IsPet() ||
                mob.IsClone()) {
                continue;
            }

            const double damage = GetEDamage(mob);
            if (damage > 0.0 && mob.Health() < damage) {
                CastE("clear-E-jungle");
                return;
            }
        }
    }
}

static void FlyHack() {
    if (!Key(ComboMenu, "FlyHack")) {
        return;
    }

    // The C# version replays raw MoveTo/AttackUnit orders here. In NightSharp
    // that can double-call the orbwalker path and disconnect, so this port
    // intentionally leaves attack/move ownership with Orbwalker.
}

static void LogicE() {
    if (!E.IsReady() || !ShouldRunNow(LastLogicETick, 90)) {
        return;
    }

    const int eMode = ListIndex(Eset, "EMode", 1);
    if (eMode == 2) {
        return;
    }

    const bool harassPlus = Bool(Eset, "harassPlus");
    if ((eMode == 0 && Orbwalker::ActiveMode() == OrbwalkingMode::Combo) || eMode == 1) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (RendKillable(enemy)) {
                CastE("logic-E-hero");
                return;
            }
        }

        if (harassPlus) {
            bool enemyInRange = false;
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (ValidHeroTarget(enemy, E.Range)) {
                    enemyInRange = true;
                    break;
                }
            }

            if (enemyInRange) {
                for (const auto& minion : GameObjects::EnemyMinions()) {
                    if (RendKillable(minion)) {
                        CastE("logic-E-harass-plus");
                        return;
                    }
                }
            }
        }
    }
}

static void RLogic() {
    if (!R.IsReady() || !ShouldRunNow(LastRLogicTick, 120)) {
        return;
    }

    const auto player = Player();
    const bool useRAllySaver = Bool(Rset, "kaliusersaveally");
    const bool balista = Bool(WowComboMenu, "Balista");
    const bool talista = Bool(WowComboMenu, "Talista");
    const bool salista = Bool(WowComboMenu, "Salista");
    const bool useREngage = Bool(Rset, "userengage");

    AIHeroClient ally = FindSoulboundAlly();
    if (ally.IsValid() && ally.IsVisible() && ally.DistanceToPlayer() <= R.Range) {
        PurgeIncomingDamageToSoulbound();
        if ((useRAllySaver &&
             player.CountEnemyHeroesInRange(R.Range) > 0 &&
             ally.CountEnemyHeroesInRange(R.Range) > 0 &&
             ally.HealthPercent() <= 30.0f) ||
            AllIncomingDamageToSoulbound() > player.Health()) {
            R.Cast();
            return;
        }

        if (balista && HeroNameEquals(ally, "Blitzcrank")) {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (!enemy.IsDead() && ValidHeroTarget(enemy) && enemy.HasBuff("rocketgrab")) {
                    R.Cast();
                    return;
                }
            }
        }

        if (talista && HeroNameEquals(ally, "TahmKench")) {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (!enemy.IsDead() && ValidHeroTarget(enemy) && enemy.HasBuff("tahmkenchwdevoured")) {
                    R.Cast();
                    return;
                }
            }
        }

        if (salista && HeroNameEquals(ally, "Skarner")) {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (!enemy.IsDead() && ValidHeroTarget(enemy) && enemy.HasBuff("skarnerimpale")) {
                    R.Cast();
                    return;
                }
            }
        }
    }

    if (useREngage) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(enemy, 1000.0f) || !Extensions::IsFacing(enemy, player)) {
                continue;
            }

            const auto path = enemy.Path();
            if (!path.empty() && path.back().Distance(player.ServerPosition()) < 400.0f) {
                R.Cast();
                return;
            }
        }
    }
}

static void Routine() {
    if (!ShouldRunNow(LastRoutineTick, 120)) {
        return;
    }

    const auto player = Player();
    if (Bool(KsMenu, "KSEJG") && E.IsReady()) {
        auto mobs = GameObjects::Jungle();
        mobs.erase(
            std::remove_if(
                mobs.begin(),
                mobs.end(),
                [](const AIMinionClient& mob) {
                    return !HasRendBuff(mob, E.Range) ||
                           !mob.IsVisible() ||
                           mob.IsPlant() ||
                           mob.IsPet() ||
                           mob.IsClone();
                }),
            mobs.end());
        std::sort(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& a, const AIMinionClient& b) {
                const int aPriority = JunglePriority(a);
                const int bPriority = JunglePriority(b);
                if (aPriority != bPriority) {
                    return aPriority > bPriority;
                }
                return a.MaxHealth() > b.MaxHealth();
            });

        for (const auto& mob : mobs) {
            const double damage = GetEDamage(mob);
            if (mob.Health() < damage) {
                CastE("routine-E-jungle-secure");
                return;
            }
        }
    }

    if (Bool(Misc, "misc-leaving-e") && E.IsReady()) {
        const AIHeroClient enemy = GetTarget(E.Range, DamageType::Physical);
        if (ValidHeroTarget(enemy, E.Range) &&
            enemy.GetBuffCount("kalistaexpungemarker") >= Slider(Misc, "misc-leaving-e-pro", 5) &&
            enemy.Distance(player.Position()) > E.Range - 50.0f) {
            if (!Bool(Misc, "misc-prevent-e") || !IsProtectionOrUnkillable(enemy)) {
                CastE("routine-E-leaving-range");
                return;
            }
        }
    }

    if (Bool(Misc, "misc-dying-e") &&
        E.IsReady() &&
        player.HealthPercent() <= static_cast<float>(Slider(Misc, "misc-dying-e-pro", 10))) {
        CastE("routine-E-before-dying");
        return;
    }

    if (Key(Misc, "misc-ward-trick")) {
        const Vector3 drakePos{ 9866.0f, -71.0f, 4414.0f };
        const Vector3 baronPos{ 5007.0f, -71.0f, 10471.0f };
        if (W.IsReady()) {
            if (player.Distance(baronPos) <= W.Range) {
                if (CastPosition(W, baronPos, "routine-W-baron")) {
                    lastWTime = GetTickCount();
                }
            } else if (player.Distance(drakePos) <= W.Range) {
                if (CastPosition(W, drakePos, "routine-W-drake")) {
                    lastWTime = GetTickCount();
                }
            }
        }
    }
}

static void Killsteal() {
    if (!ShouldRunNow(LastKillstealTick, 90)) {
        return;
    }

    if (Bool(KsMenu, "KSQ") && Q.IsReady()) {
        for (const auto& target : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(target, Q.Range) || target.IsInvulnerable()) {
                continue;
            }

            if (target.Health() < QDamage(target)) {
                CastKalistaQ(target, HitChance::High, "killsteal-Q");
                return;
            }
        }
    }

    if (Bool(KsMenu, "KSE") && E.IsReady()) {
        const bool useTolerance = SliderButtonEnabled(Misc, "EToler", true);
        const int tolerance = SliderButtonValue(Misc, "EToler", 0);
        for (const auto& target : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(target, E.Range) &&
                target.Health() < GetKalistaRealDamage(target, useTolerance, tolerance)) {
                CastE("killsteal-E");
                return;
            }
        }
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs& args) {
    (void)args;
    const auto player = Player();
    if (!player.IsValid()) {
        ClearForcedTarget();
        return;
    }

    const auto mode = Orbwalker::ActiveMode();
    if (mode != OrbwalkingMode::Combo) {
        ClearForcedTarget();
    }

    if (player.IsDead() ||
        player.IsRecalling() ||
        Game::IsChatOpen() ||
        Orbwalker::IsWindingUp()) {
        return;
    }

    switch (mode) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LaneClear:
        Clear();
        break;
    default:
        break;
    }

    Routine();
    Killsteal();
    FlyHack();
    LogicE();
    RLogic();
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpellCast;
    Events::hook.OnPlayAnimation -= &OnPlayAnimation;
    Drawing::OnEndScene -= &OnEndScene;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Orbwalker::OnNonKillableMinion -= &OnNonKillableMinion;
    ClearForcedTarget();
    RemoveKeyPermashow(ComboMenu, "FlyHack");
    RemoveKeyPermashow(Misc, "misc-ward-trick");

    IncomingDamageToSoulboundAlly.clear();
    InstantDamageOnSoulboundAlly.clear();
    Loaded = false;
}

} // namespace Plugins::AIO7UP::Kalista
