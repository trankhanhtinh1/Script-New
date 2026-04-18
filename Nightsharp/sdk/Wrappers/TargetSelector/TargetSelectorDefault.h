#pragma once

#include "../../Core/Game.h"
#include "../../Core/Objects.h"
#include "../../Enumerations/DamageType.h"
#include "../../UI/Drawing.h"
#include "../../UI/UI.h"
#include "../../Utils/StatusCheck.h"
#include "../Damages/Damage.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <string>
#include <vector>

namespace SDK {

class TargetSelectorDefault {
public:
    static float EffectiveHealth(const AIHeroClient& hero, DamageType damageType) {
        const float rawHealth = hero.Health() + hero.Ref().GetTotalShield();
        switch (damageType) {
        case DamageType::Physical:
            return rawHealth * (100.0f + std::max(hero.Armor(), 0.0f)) / 100.0f;
        case DamageType::Magical:
            return rawHealth * (100.0f + std::max(hero.SpellBlock(), 0.0f)) / 100.0f;
        case DamageType::Mixed:
            return rawHealth * (100.0f + std::max((hero.Armor() + hero.SpellBlock()) * 0.5f, 0.0f)) / 100.0f;
        case DamageType::True:
        default:
            return rawHealth;
        }
    }

    static void Initialize(MenuUI::Menu* root) {
        if (s_initialized || !root) return;
        s_initialized = true;

        const std::vector<std::string> kModeItems = {
            "Fast", "AP Damage", "AD Damage", "Least HP", "Less AA", "Less Casts", "Priority", "Near Mouse", "Close Me"
        };

        cfg.TSMode         = root->Add<MenuList>    ("TSMode",        "Mode",                kModeItems, 0);
        cfg.AttackZombie   = root->Add<MenuBool>    ("AttackZombie",  "Attack zombie",       true);
        cfg.ForceTargetKey = root->Add<MenuKeyBind> ("ForceTargetKey","Force target",        VK_LBUTTON, KeyBindType::Press);
        cfg.Timeout        = root->Add<MenuSlider>  ("Timeout",       "Time out (sec)",      5, 0, 10);
        cfg.MouseRange     = root->Add<MenuSlider>  ("MouseRange",    "Mouse range",         400, 0, 1000);
        cfg.OnlySelectKey  = root->Add<MenuKeyBind> ("OnlySelectKey", "Only attack select",  0, KeyBindType::Toggle);
        cfg.MainTarget     = root->Add<MenuList>    ("MainTarget",    "Main Target",         std::vector<std::string>{ "None" }, 0);
        root->Add<MenuSeparator>("sep_enemies", "Draw");
        cfg.DrawEnemies    = root->Add<MenuBool>    ("EnemiesEnable", "Enable",              true);
        cfg.SelectedColor  = root->Add<MenuColor>   ("SelectedColor", "Color",               0.918f, 0.188f, 0.192f, 1.0f);


        s_root = root;
        s_coreMenu = root;
        s_priorityMenu = root;
    }

    static void Reset() {
        s_initialized = false;
        s_root = nullptr;
        s_coreMenu = nullptr;
        s_priorityMenu = nullptr;
        cfg = {};
        s_selectedTarget = AIHeroClient();
        s_forcedTarget = AIHeroClient();
        s_bossMenuBuilt = false;
        s_prioritySlidersBuilt = false;
        s_priorityBuiltCount = 0;
        s_forceExpireTime = 0.0f;
        s_wasForceKeyDown = false;
        s_forceKeyPressTime = 0.0f;
        s_lastSetForceTime = 0.0f;
    }

    static void Update() {
        if (!s_root) return;

        if (s_forcedTarget.IsValid() && (!s_forcedTarget.IsEnemy() || s_forcedTarget.IsDead())) {
            s_forcedTarget = AIHeroClient();
        }

        EnsurePrioritySliders();
        EnsureBossOptions();
        HandleForceTarget();

        if (s_selectedTarget.IsValid()) {
            if (s_selectedTarget.IsDead() || !s_selectedTarget.IsVisible()) {
                s_selectedTarget = AIHeroClient();
            }
        }
    }

    static void Render() {
        if (!s_root) return;

        const auto bossTarget = ResolveBossTarget();
        const uint32_t bossNetId = bossTarget.IsValid() ? static_cast<uint32_t>(bossTarget.NetworkId()) : 0u;
        const uint32_t selNetId  = s_selectedTarget.IsValid() ? static_cast<uint32_t>(s_selectedTarget.NetworkId()) : 0u;

        if (cfg.DrawEnemies && cfg.DrawEnemies->Enabled) {
            const ImU32 selCol = cfg.SelectedColor ? cfg.SelectedColor->GetImU32() : IM_COL32(234, 48, 49, 255);
            const bool selColVisible = cfg.SelectedColor ? cfg.SelectedColor->Color[3] > 0.18f : true;

            if (selColVisible) {
                for (const auto& enemy : ObjectManager::EnemyHeroes()) {
                    if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) continue;
                    const uint32_t nid = static_cast<uint32_t>(enemy.NetworkId());
                    const bool isSel = (nid == bossNetId || nid == selNetId);
                    if (!isSel) continue;
                    const float radius = enemy.BoundingRadius() + 10.0f;
                    Drawing::DrawCircle(enemy.Position(), radius, selCol, 2.0f);
                }
            }
        }

    }

    static AIHeroClient GetTarget(float range, DamageType damageType = DamageType::Physical, const Vector3& from = ObjectManager::Player().Position()) {
        if (!s_root) return AIHeroClient();

        const auto boss = ResolveBossTarget();
        if (boss.IsValid() && IsTargetValid(boss, range, from)) return boss;

        if (IsOnlyAttackSelected()) {
            if (s_selectedTarget.IsValid() && IsTargetValid(s_selectedTarget, range, from))
                return s_selectedTarget;
            return AIHeroClient();
        }

        auto targets = GetValidTargets(range, from);
        if (targets.empty()) return AIHeroClient();

        if (s_selectedTarget.IsValid() && !s_selectedTarget.IsDead()) {
            for (const auto& t : targets) {
                if (static_cast<uint32_t>(t.NetworkId()) == static_cast<uint32_t>(s_selectedTarget.NetworkId()))
                    return s_selectedTarget;
            }
        }

        return SelectBest(targets, damageType, from);
    }

    static std::vector<AIHeroClient> GetTargets(float range, DamageType damageType = DamageType::Physical, const Vector3& from = Vector3()) {
        if (!s_root) return {};
        const auto origin = from.IsZero() ? ObjectManager::Player().Position() : from;

        const auto boss = ResolveBossTarget();
        if (boss.IsValid() && IsTargetValid(boss, range, origin)) return { boss };

        if (IsOnlyAttackSelected()) {
            if (s_selectedTarget.IsValid() && IsTargetValid(s_selectedTarget, range, origin))
                return { s_selectedTarget };
            return {};
        }

        auto targets = GetValidTargets(range, origin);
        SortTargets(targets, damageType, origin);

        if (s_selectedTarget.IsValid() && !s_selectedTarget.IsDead()) {
            for (auto it = targets.begin(); it != targets.end(); ++it) {
                if (static_cast<uint32_t>(it->NetworkId()) == static_cast<uint32_t>(s_selectedTarget.NetworkId())) {
                    auto sel = *it;
                    targets.erase(it);
                    targets.insert(targets.begin(), sel);
                    break;
                }
            }
        }

        return targets;
    }

    static AIHeroClient GetSelectedTarget() { return s_selectedTarget; }

    static void SetSelectedTarget(const AIHeroClient& target) {
        s_selectedTarget = (target.IsValid() && target.IsHero()) ? target : AIHeroClient();
        if (s_selectedTarget.IsValid()) {
            const float timeout = cfg.Timeout ? static_cast<float>(cfg.Timeout->Value) : 5.0f;
            s_forceExpireTime = Game::Time() + timeout;
        }
    }

    static void ClearSelectedTarget() {
        s_selectedTarget = AIHeroClient();
    }

    static AIHeroClient ForcedTarget() { return s_forcedTarget; }

    static void SetForcedTarget(const AIHeroClient& target) {
        s_forcedTarget = (target.IsValid() && target.IsHero()) ? target : AIHeroClient();
    }

    static void ClearForcedTarget() { s_forcedTarget = AIHeroClient(); }

    static int GetPriority(const AIHeroClient& hero) {
        if (!hero.IsValid()) return 1;
        const std::string name = hero.CharacterName();
        if (name.empty()) return 1;
        if (s_priorityMenu) {
            if (auto* s = s_priorityMenu->Get<MenuSlider>("Priority_" + name))
                return s->Value;
        }
        return GetDefaultPriority(name);
    }

private:
    enum class Mode : int {
        Fast    = 0,
        APDamage  = 1,
        ADDamage  = 2,
        LeastHP   = 3,
        LessAA    = 4,
        LessCasts = 5,
        Priority  = 6,
        Mouse     = 7,
        CloseMe   = 8
    };

    struct Cfg {
        MenuKeyBind* OnlySelectKey  = nullptr;
        MenuKeyBind* ForceTargetKey = nullptr;
        MenuBool*    DrawEnemies    = nullptr;
        MenuColor*   SelectedColor  = nullptr;
        MenuSlider*  Timeout        = nullptr;
        MenuSlider*  MouseRange     = nullptr;
        MenuList*    TSMode         = nullptr;
        MenuBool*    AttackZombie   = nullptr;
        MenuList*    MainTarget     = nullptr;
    };

    static inline bool s_initialized = false;
    static inline MenuUI::Menu* s_root         = nullptr;
    static inline MenuUI::Menu* s_coreMenu     = nullptr;
    static inline MenuUI::Menu* s_priorityMenu = nullptr;
    static inline Cfg           cfg            = {};
    static inline AIHeroClient  s_selectedTarget = {};
    static inline AIHeroClient  s_forcedTarget   = {};
    static inline bool  s_bossMenuBuilt      = false;
    static inline bool  s_prioritySlidersBuilt = false;
    static inline int   s_priorityBuiltCount   = 0;
    static inline float s_forceExpireTime = 0.0f;
    static inline bool  s_wasForceKeyDown = false;
    static inline float s_forceKeyPressTime = 0.0f;
    static inline float s_lastSetForceTime  = 0.0f;

    static int GetActiveModeIndex() {
        return cfg.TSMode ? std::clamp(cfg.TSMode->Index, 0, 8) : 0;
    }

    static Mode MapScriptMode(DamageType dt) {
        switch (dt) {
        case DamageType::Physical: return Mode::ADDamage;
        case DamageType::Magical:  return Mode::APDamage;
        default:                   return Mode::LeastHP;
        }
    }

    static float SafeGetAAScore(const AIHeroClient& player, const AIHeroClient& target) {
        float dmg = 0.0f;
        __try {
            dmg = Damage::GetAutoAttackDamage(player, target, true);
        } __except(1) {
            dmg = 0.0f;
        }
        if (dmg <= 0.0f && player.TotalAttackDamage() > 0.0f)
            dmg = player.GetAutoAttackDamage(target, false);
        if (dmg <= 0.0f) dmg = 1.0f;
        return target.Health() / dmg;
    }

    static float GetScore(const AIHeroClient& hero, Mode mode, DamageType dt, const Vector3& playerPos, const Vector3& cursorPos) {
        switch (mode) {
        case Mode::APDamage:
            return EffectiveHealth(hero, DamageType::Magical);
        case Mode::ADDamage:
            return EffectiveHealth(hero, DamageType::Physical);
        case Mode::LeastHP:
        case Mode::LessCasts:
            return hero.Health();
        case Mode::LessAA: {
            auto player = ObjectManager::Player();
            return player.IsValid() ? SafeGetAAScore(player, hero) : hero.Health();
        }
        case Mode::Priority:
            return -(static_cast<float>(GetPriority(hero)));
        case Mode::Mouse:
            return hero.Distance(cursorPos);
        case Mode::CloseMe:
            return hero.Distance(playerPos);
        case Mode::Fast:
        default:
            return GetScore(hero, MapScriptMode(dt), dt, playerPos, cursorPos);
        }
    }

    static bool IsOnlyAttackSelected() {
        return cfg.OnlySelectKey && cfg.OnlySelectKey->Active;
    }

    static bool IsAttackZombie() {
        return cfg.AttackZombie ? cfg.AttackZombie->Enabled : true;
    }

    static bool IsTargetValid(const AIHeroClient& target, float range, const Vector3& origin) {
        if (!target.IsValid() || target.IsDead() || !target.IsVisible() || !target.IsTargetable())
            return false;

        if (Utils::StatusCheck::IsZombieLike(target) && !IsAttackZombie())
            return false;

        static const char* kInvuln[] = {
            "KayleR", "TryndamereR", "kindaborroweytime", "ChronoShift", "UndyingRage", nullptr
        };
        for (const char** p = kInvuln; *p; ++p)
            if (target.HasBuff(*p)) return false;

        if (range > 0.0f) {
            const float effectiveRange = range + target.BoundingRadius();
            if (target.Distance(origin) > effectiveRange)
                return false;
        }

        return true;
    }

    static std::vector<AIHeroClient> GetValidTargets(float range, const Vector3& origin) {
        std::vector<AIHeroClient> result;
        for (const auto& hero : ObjectManager::EnemyHeroes()) {
            if (IsTargetValid(hero, range, origin))
                result.push_back(hero);
        }
        return result;
    }

    static AIHeroClient SelectBest(std::vector<AIHeroClient>& targets, DamageType dt, const Vector3& origin) {
        if (targets.empty()) return AIHeroClient();

        const Mode mode      = static_cast<Mode>(GetActiveModeIndex());
        const auto player    = ObjectManager::Player();
        const auto playerPos = player.IsValid() ? player.Position() : origin;
        const auto cursor    = Game::CursorPos();
        const bool zombie    = IsAttackZombie();

        const AIHeroClient* best = nullptr;
        float bestScore = FLT_MAX;

        for (const auto& hero : targets) {
            if (!hero.IsValid()) continue;
            float score = (zombie && Utils::StatusCheck::IsZombieLike(hero))
                ? 1e30f
                : GetScore(hero, mode, dt, playerPos, cursor);
            if (score < bestScore) { bestScore = score; best = &hero; }
        }

        return best ? *best : AIHeroClient();
    }

    static void SortTargets(std::vector<AIHeroClient>& targets, DamageType dt, const Vector3& origin) {
        if (targets.size() < 2) return;

        const Mode mode      = static_cast<Mode>(GetActiveModeIndex());
        const auto player    = ObjectManager::Player();
        const auto playerPos = player.IsValid() ? player.Position() : origin;
        const auto cursor    = Game::CursorPos();
        const bool zombie    = IsAttackZombie();

        std::stable_sort(targets.begin(), targets.end(),
            [&](const AIHeroClient& a, const AIHeroClient& b) {
                if (!a.IsValid()) return false;
                if (!b.IsValid()) return true;
                bool az = zombie && Utils::StatusCheck::IsZombieLike(a);
                bool bz = zombie && Utils::StatusCheck::IsZombieLike(b);
                if (az != bz) return bz;
                if (az && bz) return false;
                float sa = GetScore(const_cast<AIHeroClient&>(a), mode, dt, playerPos, cursor);
                float sb = GetScore(const_cast<AIHeroClient&>(b), mode, dt, playerPos, cursor);
                return sa < sb;
            });
    }

    static AIHeroClient ResolveBossTarget() {
        if (!cfg.MainTarget || cfg.MainTarget->Index <= 0) return AIHeroClient();
        int idx = cfg.MainTarget->Index;
        if (idx >= static_cast<int>(cfg.MainTarget->Items.size())) return AIHeroClient();
        const std::string& bossName = cfg.MainTarget->Items[static_cast<size_t>(idx)];
        if (bossName.empty() || bossName == "None") return AIHeroClient();
        for (const auto& hero : ObjectManager::EnemyHeroes()) {
            if (!hero.IsValid() || hero.IsDead()) continue;
            const std::string name = hero.CharacterName();
            if (!name.empty() && _stricmp(name.c_str(), bossName.c_str()) == 0)
                return hero;
        }
        return AIHeroClient();
    }

    static void EnsureBossOptions() {
        if (s_bossMenuBuilt || !cfg.MainTarget) return;
        auto enemies = ObjectManager::EnemyHeroes();
        if (enemies.empty()) return;
        cfg.MainTarget->Items.clear();
        cfg.MainTarget->Items.push_back("None");
        for (const auto& hero : enemies) {
            if (!hero.IsValid()) continue;
            std::string name = hero.CharacterName();
            if (!name.empty()) cfg.MainTarget->Items.push_back(name);
        }
        cfg.MainTarget->Index = 0;
        s_bossMenuBuilt = true;
    }

    static int GetForceTargetKey() {
        if (!cfg.ForceTargetKey || cfg.ForceTargetKey->Key <= 0) return VK_LBUTTON;
        return cfg.ForceTargetKey->Key;
    }

    static void HandleForceTarget() {
        if (!s_root) return;

        const float mouseRange = cfg.MouseRange ? static_cast<float>(cfg.MouseRange->Value) : 400.0f;
        const float timeout    = cfg.Timeout    ? static_cast<float>(cfg.Timeout->Value)    : 5.0f;

        const int  forceKey = GetForceTargetKey();
        const bool keyDown  = (GetAsyncKeyState(forceKey) & 0x8000) != 0;
        const float gameTime = Game::Time();

        if (keyDown && !s_wasForceKeyDown) {
            s_forceKeyPressTime = gameTime;
        } else if (!keyDown && s_wasForceKeyDown) {
            if ((gameTime - s_forceKeyPressTime) < 0.3f &&
                (gameTime - s_lastSetForceTime) > 1.0f) {
                auto cursor = Game::CursorPos();
                if (!cursor.IsZero()) {
                    AIHeroClient nearest;
                    float bestDist = mouseRange;
                    for (const auto& hero : ObjectManager::EnemyHeroes()) {
                        if (!hero.IsValid() || hero.IsDead() || !hero.IsVisible()) continue;
                        float dist = hero.Distance(cursor);
                        if (dist < bestDist) { bestDist = dist; nearest = hero; }
                    }
                    if (nearest.IsValid()) {
                        s_selectedTarget    = nearest;
                        s_forceExpireTime   = gameTime + timeout;
                        s_lastSetForceTime  = gameTime;
                    } else {
                        s_selectedTarget = AIHeroClient();
                    }
                }
            }
        }
        s_wasForceKeyDown = keyDown;

        if (s_selectedTarget.IsValid()) {
            if (s_selectedTarget.IsDead()) {
                s_selectedTarget = AIHeroClient();
            } else if (s_selectedTarget.IsVisible()) {
                s_forceExpireTime = gameTime + timeout;
            } else if (gameTime > s_forceExpireTime) {
                s_selectedTarget = AIHeroClient();
            }
        }
    }

    static void EnsurePrioritySliders() {
        if (s_prioritySlidersBuilt || !s_priorityMenu) return;
        int validEnemies = 0;
        int matchedSliders = 0;
        for (const auto& enemy : ObjectManager::EnemyHeroes()) {
            if (!enemy.IsValid()) continue;
            const std::string name = enemy.CharacterName();
            if (name.empty()) continue;
            ++validEnemies;
            const std::string key = "Priority_" + name;
            if (s_priorityMenu->Get<MenuSlider>(key)) {
                ++matchedSliders;
                continue;
            }
            if (s_priorityMenu->Add<MenuSlider>(key, name, GetDefaultPriority(name), 0, 5)) {
                ++matchedSliders;
                ++s_priorityBuiltCount;
            }
        }
        if (validEnemies > 0 && matchedSliders == validEnemies && s_priorityBuiltCount >= validEnemies) {
            s_prioritySlidersBuilt = true;
        }
    }

    static int GetDefaultPriority(const std::string& name) {
        static const char* maxPrio[] = {
            "Ahri","Aphelios","Anivia","Annie","Ashe","Azir","Brand","Caitlyn",
            "Cassiopeia","Corki","Draven","Ezreal","Graves","Jinx","Kalista",
            "Kaisa","Karma","Karthus","Katarina","Kennen","KogMaw","Kindred",
            "Leblanc","Lucian","Lux","Malzahar","MasterYi","MissFortune","Neeko",
            "Orianna","Quinn","Sivir","Sylas","Syndra","Talon","Teemo","Tristana",
            "TwistedFate","Twitch","Varus","Vayne","Veigar","Velkoz","Viktor",
            "Xerath","Zed","Ziggs","Jhin","Soraka","AurelionSol","Taliyah",
            "Qiyana","Zoe","Xayah","Samira","Zeri","Nilah","Smolder", nullptr
        };
        static const char* highPrio[] = {
            "Akali","Diana","Ekko","FiddleSticks","Fiora","Fizz","Heimerdinger",
            "Jayce","Kassadin","Kayle","KhaZix","Lissandra","Mordekaiser","Nidalee",
            "Riven","Senna","Shaco","Vladimir","Yasuo","Zilean","Camille","Kayn",
            "Yone","Viego","Gwen","Akshan","Belveth", nullptr
        };
        static const char* medPrio[] = {
            "Aatrox","Darius","Elise","Evelynn","Galio","Gangplank","Gragas",
            "Irelia","Jax","LeeSin","Maokai","Morgana","Nocturne","Pantheon",
            "Poppy","Pyke","Rengar","Rumble","Ryze","Sett","Swain","Trundle",
            "Tryndamere","Udyr","Urgot","Vi","XinZhao","RekSai","Illaoi","Kled",
            "Lillia","Vex","Renata", nullptr
        };
        static const char* lowPrio[] = {
            "Alistar","Amumu","Bard","Blitzcrank","Braum","Chogath","DrMundo",
            "Garen","Gnar","Hecarim","Janna","JarvanIV","Leona","Lulu","Malphite",
            "Nami","Nasus","Nautilus","Nunu","Olaf","Rammus","Renekton","Sejuani",
            "Shen","Shyvana","Singed","Sion","Skarner","Sona","Taric","TahmKench",
            "Thresh","Volibear","Warwick","MonkeyKing","Yorick","Yuumi","Zac","Zyra",
            "Ornn","Rakan","Ivern","Rell","KSante","Milio", nullptr
        };
        for (const char** p = maxPrio; *p; ++p) if (_stricmp(name.c_str(), *p) == 0) return 5;
        for (const char** p = highPrio; *p; ++p) if (_stricmp(name.c_str(), *p) == 0) return 4;
        for (const char** p = medPrio; *p; ++p)  if (_stricmp(name.c_str(), *p) == 0) return 3;
        for (const char** p = lowPrio; *p; ++p)  if (_stricmp(name.c_str(), *p) == 0) return 2;
        return 1;
    }
};

}
