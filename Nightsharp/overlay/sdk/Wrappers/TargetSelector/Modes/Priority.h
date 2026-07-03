#pragma once

#include "../ITargetSelectorMode.h"
#include "PriorityCategory.h"
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <vector>

namespace SDK::Modes {

class Priority : public ITargetSelectorMode {
public:
    static Priority*& Instance() {
        static Priority* inst = nullptr;
        return inst;
    }

    Priority() {
        Instance() = this;
        categories_ = {
            PriorityCategory(5, {
                "Ahri", "Akshan", "Anivia", "Annie", "Aphelios", "Ashe", "AurelionSol", "Azir", "Brand", "Caitlyn", "Cassiopeia", "Corki", "Draven", "Ezreal", "Fiddlesticks", "Jhin", "Jinx", "KaiSa", "Kalista", "Karthus", "Kassadin", "Katarina", "Kayle", "Kennen", "KogMaw", "LeBlanc", "Lucian", "Lux", "Malzahar", "MasterYi", "MissFortune", "Neeko", "Nidalee", "Orianna", "Samira", "Senna", "Sivir", "Sona", "Soraka", "Syndra", "Taliyah", "Talon", "Teemo", "Tristana", "Twitch", "Varus", "Vayne", "Veigar", "VelKoz", "Viktor", "Xayah", "Xerath", "Yasuo", "Yone", "Yumi", "Zed", "Ziggs", "Zoe", "Zyra"
            }),
            PriorityCategory(4, {
                "Akali", "Diana", "Ekko", "Elise", "Evelynn", "Fiora", "Fizz", "Gangplank", "Graves", "Gwen", "Irelia", "Jayce", "Kayn", "KhaZix", "Kindred", "LeeSin", "Lillia", "Lissandra", "Mordekaiser", "Nocturne", "Pantheon", "Pyke", "Qiyana", "Rengar", "Riven", "Rumble", "Ryze", "Shaco", "Shyvana", "Sylas", "Tryndamere", "Viego", "Vladimir", "Wukong", "XinZhao"
            }),
            PriorityCategory(3, {
                "Aatrox", "Amumu", "Blitzcrank", "Camille", "Darius", "DrMundo", "Garen", "Gnar", "Hecarim", "Illaoi", "JarvanIV", "Jax", "Kled", "Nasus", "Nautilus", "Nunu", "Olaf", "Poppy", "Renekton", "Sett", "Shen", "Singed", "Sion", "Skarner", "Swain", "TahmKench", "Trundle", "Udyr", "Urgot", "Vi", "Volibear", "Warwick", "Yorick", "Zac"
            }),
            PriorityCategory(2, {
                "Alistar", "Bard", "Braum", "Gragas", "Janna", "Karma", "Leona", "Lulu", "Malphite", "Maokai", "Morgana", "Nami", "Rell", "Rakan", "Sejuani", "Seraphine", "Taric", "Thresh", "Zilean"
            })
        };
    }

    const char* DisplayName() const override { return "Priority"; }
    const char* Name() const override { return "priority"; }

    void AddToMenu(Menu* menu) override {
        parentMenu_ = menu;
        sub_ = new Menu("priority_sub", "Priority Settings");
        for (const auto& hero : GameObjects::EnemyHeroes()) {
            std::string cname = hero.CharacterName();
            if (cname.empty()) cname = hero.Name();
            if (cname.empty()) continue;
            int defVal = GetDefaultPriority(cname);
            sub_->Add(new MenuSlider(("p_" + cname).c_str(), cname.c_str(), defVal, 1, 5));
        }
        menu->Add(sub_);
    }

    int GetHeroPriority(const AIHeroClient& hero) {
        std::string cname = hero.CharacterName();
        if (cname.empty()) cname = hero.Name();
        if (sub_ && !cname.empty()) {
            auto* s = sub_->Get<MenuSlider>(("p_" + cname).c_str());
            if (s) return s->Value;
        }
        return GetDefaultPriority(cname);
    }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes) override {
        auto result = heroes;
        std::sort(result.begin(), result.end(), [this](const AIHeroClient& a, const AIHeroClient& b) {
            int pa = GetHeroPriority(a);
            int pb = GetHeroPriority(b);
            if (pa != pb) return pa > pb;
            float hpa = a.MaxHealth() > 0 ? (a.Health() / a.MaxHealth()) : 1.0f;
            float hpb = b.MaxHealth() > 0 ? (b.Health() / b.MaxHealth()) : 1.0f;
            return hpa < hpb;
        });
        return result;
    }

private:
    int GetDefaultPriority(const std::string& charName) {
        for (const auto& cat : categories_) {
            if (cat.Champions.find(charName) != cat.Champions.end()) {
                return cat.Value;
            }
        }
        return 1;
    }

    Menu* parentMenu_ = nullptr;
    Menu* sub_ = nullptr;
    std::vector<PriorityCategory> categories_;
};

} // namespace SDK::Modes
