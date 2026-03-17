#pragma once
#include <map>
#include <string>
#include <vector>
#include "../../GameObjects/GameObjects.h"
#include "../../Game.h"
#include "../../Math/MathUtils.h"
#include "../../UI/MenuUI.h"

namespace EzEvade {

    // Forward declare EvadeUtils for GetGamePosition
    namespace EvadeUtils {
        Vec2 GetGamePosition(SDK::GameObject* hero, float delay);
    }

    class HeroInfo {
    public:
        SDK::GameObject* hero = nullptr;
        Vec2 serverPos2D = { 0, 0 };
        Vec2 serverPos2DExtra = { 0, 0 };
        Vec2 serverPos2DPing = { 0, 0 };
        Vec2 currentPosition = { 0, 0 };
        bool isMoving = false;
        float boundingRadius = 0.0f;
        float moveSpeed = 0.0f;

        HeroInfo() = default;
        HeroInfo(SDK::GameObject* targetHero);

        void UpdateInfo();
    };

    class MenuCache {
    public:
        SDK::MenuUI::Menu* menu = nullptr;
        std::map<std::string, SDK::MenuUI::MenuItem*> cache;

        MenuCache() = default;
        MenuCache(SDK::MenuUI::Menu* m);

        void AddMenuToCache(SDK::MenuUI::Menu* newMenu);
        void AddMenuItemToCache(SDK::MenuUI::MenuItem* item);
        std::vector<SDK::MenuUI::MenuItem*> ReturnAllItems(SDK::MenuUI::Menu* m);
    };

    class ObjectCache {
    public:
        static std::map<int, SDK::GameObject*> turrets;
        static HeroInfo myHeroCache;
        static MenuCache menuCache;
        static float gamePing;

        static void Initialize();
        static void InitializeCache();
        static void OnTick(); // Call this on Game_OnUpdate

        // Helper mock wrappers for Spell logic
        static bool GetBool(const std::string& name) {
            if (menuCache.cache.find(name) != menuCache.cache.end()) {
                auto* toggle = dynamic_cast<SDK::MenuUI::MenuBool*>(menuCache.cache[name]);
                if (toggle) return toggle->Enabled;
                // Also check keybinds — they have an Active state
                auto* keybind = dynamic_cast<SDK::MenuUI::MenuKeyBind*>(menuCache.cache[name]);
                if (keybind) return keybind->Active;
            }
            // Smart defaults for per-spell keys (C# creates these dynamically per-champion)
            // Without these, ALL spell drawing and dodging is silently disabled!
            if (name.size() > 9 && name.substr(name.size() - 9) == "DrawSpell") return true;
            if (name.size() > 10 && name.substr(name.size() - 10) == "DodgeSpell") return true;
            if (name.size() > 9 && name.substr(name.size() - 9) == "FastEvade") return false;
            return false; // fallback
        }

        static int GetSlider(const std::string& name) {
            if (menuCache.cache.find(name) != menuCache.cache.end()) {
                auto* slider = dynamic_cast<SDK::MenuUI::MenuSlider*>(menuCache.cache[name]);
                if (slider) return slider->Value;
            }
            // Smart defaults for per-spell slider keys
            // DodgeIgnoreHP: health % threshold below which to dodge. 100 = always dodge.
            if (name.size() > 11 && name.substr(name.size() - 11) == "DodgeIgnoreHP") return 100;
            return 0; // fallback
        }

        static void SetBool(const std::string& name, bool value) {
            if (menuCache.cache.find(name) != menuCache.cache.end()) {
                auto* toggle = dynamic_cast<SDK::MenuUI::MenuBool*>(menuCache.cache[name]);
                if (toggle) { toggle->Enabled = value; return; }
                auto* keybind = dynamic_cast<SDK::MenuUI::MenuKeyBind*>(menuCache.cache[name]);
                if (keybind) { keybind->Active = value; return; }
            }
        }

        static void SetSlider(const std::string& name, int value) {
            if (menuCache.cache.find(name) != menuCache.cache.end()) {
                auto* slider = dynamic_cast<SDK::MenuUI::MenuSlider*>(menuCache.cache[name]);
                if (slider) { slider->Value = value; return; }
            }
        }

        static int GetDangerLevel(const std::string& name) {
            if (menuCache.cache.find(name) != menuCache.cache.end()) {
                auto* list = dynamic_cast<SDK::MenuUI::MenuList*>(menuCache.cache[name]);
                if (list) {
                    switch (list->Index) { // Assumes 0=Low, 1=Normal, 2=High, 3=Extreme
                        case 0: return 1;
                        case 1: return 2;
                        case 2: return 3;
                        case 3: return 4;
                    }
                }
            }
            return 2; // Default to mid-level
        }
    };

} // namespace EzEvade
