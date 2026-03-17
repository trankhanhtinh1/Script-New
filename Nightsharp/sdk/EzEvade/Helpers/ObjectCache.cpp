#include "ObjectCache.h"
#include "../Utils/EvadeUtils.h"

namespace EzEvade {

    // --- HeroInfo ---
    HeroInfo::HeroInfo(SDK::GameObject* targetHero)
    {
        this->hero = targetHero;
    }

    void HeroInfo::UpdateInfo()
    {
        if (!hero) return;

        float extraDelayBuffer = 0.0f;
        if (ObjectCache::menuCache.cache.find("ExtraPingBuffer") != ObjectCache::menuCache.cache.end())
        {
            auto* slider = dynamic_cast<SDK::MenuUI::MenuSlider*>(ObjectCache::menuCache.cache["ExtraPingBuffer"]);
            if (slider) {
                extraDelayBuffer = (float)slider->Value;
            }
        }

        serverPos2D = hero->GetPosition().To2D();
        serverPos2DExtra = EvadeUtils::GetGamePosition(hero, ObjectCache::gamePing + extraDelayBuffer);
        serverPos2DPing = EvadeUtils::GetGamePosition(hero, ObjectCache::gamePing);
        
        currentPosition = hero->GetPosition().To2D();
        boundingRadius = 65.0f;
        moveSpeed = hero->GetMoveSpeed();
        isMoving = hero->IsMoving(); 
    }

    // --- MenuCache ---
    MenuCache::MenuCache(SDK::MenuUI::Menu* m)
    {
        this->menu = m;
        if (m) {
            AddMenuToCache(m);
        }
    }

    void MenuCache::AddMenuToCache(SDK::MenuUI::Menu* newMenu)
    {
        if (!newMenu) return;
        auto items = ReturnAllItems(newMenu);
        for (auto* item : items)
        {
            AddMenuItemToCache(item);
        }
    }

    void MenuCache::AddMenuItemToCache(SDK::MenuUI::MenuItem* item)
    {
        if (item && cache.find(item->InternalName) == cache.end())
        {
            cache[item->InternalName] = item;
        }
    }

    std::vector<SDK::MenuUI::MenuItem*> MenuCache::ReturnAllItems(SDK::MenuUI::Menu* m)
    {
        std::vector<SDK::MenuUI::MenuItem*> menuList;
        if (!m) return menuList;

        for (auto& child : m->GetItems())
        {
            menuList.push_back(child.get());
            if (auto subMenu = dynamic_cast<SDK::MenuUI::Menu*>(child.get()))
            {
                auto subItems = ReturnAllItems(subMenu);
                menuList.insert(menuList.end(), subItems.begin(), subItems.end());
            }
        }
        return menuList;
    }

    // --- ObjectCache static variables ---
    std::map<int, SDK::GameObject*> ObjectCache::turrets;
    HeroInfo ObjectCache::myHeroCache;
    MenuCache ObjectCache::menuCache;
    float ObjectCache::gamePing = 0.0f;

    void ObjectCache::Initialize()
    {
        // Player is a value type — take its address for HeroInfo
        // This works because SDK::GameObjects::Player is a static inline global
        myHeroCache = HeroInfo(&SDK::GameObjects::Player);
        InitializeCache();
    }

    void ObjectCache::InitializeCache()
    {
        // SDK::GameObjects::AllTurrets is a vector of value-type GameObjects.
        // We store raw pointers to the global array elements.
        // These are stable because AllTurrets is a static inline global vector.
        turrets.clear();
        for (size_t i = 0; i < SDK::GameObjects::AllTurrets.size(); i++)
        {
            auto& t = SDK::GameObjects::AllTurrets[i]; 
            int netId = t.GetNetId();
            if (turrets.find(netId) == turrets.end())
            {
                turrets[netId] = &SDK::GameObjects::AllTurrets[i];
            }
        }
    }

    void ObjectCache::OnTick()
    {
        gamePing = 30.0f; // Mock — substitute with real SDK::Game::GetPing() if available
        
        // Re-initialize turret cache each tick since AllTurrets may be rebuilt
        InitializeCache();

        if (myHeroCache.hero) {
            myHeroCache.UpdateInfo();
        }
    }

} // namespace EzEvade
