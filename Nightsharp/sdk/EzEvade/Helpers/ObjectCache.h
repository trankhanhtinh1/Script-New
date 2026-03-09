#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Helpers/Situation.h"
#include <unordered_map>
#include <vector>

namespace EzEvade {

class HeroInfo {
public:
    SDK::GameObject Hero;
    Vec2 ServerPos2D = Vec2();
    Vec2 ServerPos2DExtra = Vec2();
    Vec2 ServerPos2DPing = Vec2();
    Vec2 CurrentPosition = Vec2();
    bool IsMoving = false;
    float BoundingRadius = 0.0f;
    float MoveSpeed = 0.0f;

    HeroInfo() = default;
    explicit HeroInfo(const SDK::GameObject& hero)
        : Hero(hero) {}

    void SetHero(const SDK::GameObject& hero) {
        Hero = hero;
        UpdateInfo();
    }

    void UpdateInfo() {
        if (!Hero.IsValid()) {
            return;
        }

        // NOTE:
        // EzEvade C# uses EvadeUtils.GetGamePosition(hero, ping/extraPing) here.
        // The predictor module is ported later, so this first pass keeps
        // server position snapshots in the same fields.
        ServerPos2D = Hero.GetServerPosition().To2D();
        ServerPos2DPing = ServerPos2D;
        ServerPos2DExtra = ServerPos2D;
        CurrentPosition = Hero.GetPosition().To2D();
        BoundingRadius = Hero.GetBoundingRadius();
        MoveSpeed = Hero.GetMoveSpeed();
        IsMoving = Hero.IsMoving();
    }
};

class MenuCache {
public:
    std::shared_ptr<SDK::MenuUI::Menu> Menu;
    std::unordered_map<std::string, SDK::MenuUI::MenuItem*> Cache;

    MenuCache() = default;
    explicit MenuCache(const std::shared_ptr<SDK::MenuUI::Menu>& menu) {
        SetMenu(menu);
    }

    void SetMenu(const std::shared_ptr<SDK::MenuUI::Menu>& menu) {
        Menu = menu;
        Cache.clear();
        if (Menu) {
            AddMenuToCache(Menu);
        }
        Situation::SetMenu(Menu);
    }

    void AddMenuToCache(const std::shared_ptr<SDK::MenuUI::Menu>& newMenu) {
        if (!newMenu) return;
        for (const auto& item : newMenu->GetItems()) {
            AddMenuItemToCache(item.get());
            if (auto* sub = dynamic_cast<SDK::MenuUI::Menu*>(item.get())) {
                for (const auto& child : sub->GetItems()) {
                    AddMenuItemToCache(child.get());
                }
                AddMenuToCache(std::dynamic_pointer_cast<SDK::MenuUI::Menu>(item));
            }
        }
    }

    void AddMenuItemToCache(SDK::MenuUI::MenuItem* item) {
        if (!item) return;
        if (item->InternalName.empty()) return;
        if (Cache.find(item->InternalName) != Cache.end()) return;
        Cache.emplace(item->InternalName, item);
    }

    SDK::MenuUI::MenuItem* Get(const std::string& key) const {
        auto it = Cache.find(key);
        return (it != Cache.end()) ? it->second : nullptr;
    }

    bool GetBool(const std::string& key, bool fallback = false) const {
        auto* item = dynamic_cast<SDK::MenuUI::MenuBool*>(Get(key));
        return item ? item->Enabled : fallback;
    }

    int GetSlider(const std::string& key, int fallback = 0) const {
        auto* item = dynamic_cast<SDK::MenuUI::MenuSlider*>(Get(key));
        return item ? item->Value : fallback;
    }

    int GetListIndex(const std::string& key, int fallback = 0) const {
        auto* item = dynamic_cast<SDK::MenuUI::MenuList*>(Get(key));
        return item ? item->Index : fallback;
    }

    bool GetKey(const std::string& key, bool fallback = false) const {
        auto* item = dynamic_cast<SDK::MenuUI::MenuKeyBind*>(Get(key));
        return item ? item->Active : fallback;
    }
};

class ObjectCache {
public:
    static inline std::unordered_map<int, SDK::GameObject> Turrets = {};
    static inline HeroInfo MyHeroCache = HeroInfo();
    static inline MenuCache Menu = MenuCache();
    static inline float GamePing = 0.0f;

    static void Initialize(const std::shared_ptr<SDK::MenuUI::Menu>& menu) {
        MyHeroCache.SetHero(SDK::GameObjects::Player);
        Menu.SetMenu(menu);
        BuildTurretCache();
        Refresh();
    }

    static void Refresh() {
        GamePing = SDK::Game::GetPing();
        MyHeroCache.UpdateInfo();
        BuildTurretCache();
    }

    static void BuildTurretCache() {
        Turrets.clear();
        for (const auto& turret : SDK::GameObjects::AllTurrets) {
            if (!turret.IsValid()) continue;
            Turrets[(int)turret.GetNetId()] = turret;
        }
    }
};

} // namespace EzEvade
