// TARGET SELECTOR PRIORITY MENU TEST
// Dựa trên NewTargetSelector.cs PriorityMenu logic
// Đã tích hợp vào Internal OrbWalker Menu.cpp

#include "SDK/ChampionDatabase.h"
#include "SDK/TargetSelector.h"
#include "SDK/ObjectManager.h"

namespace Test
{
    // Test function để verify Priority menu hoạt động đúng
    void TestPriorityMenu() {
        // 1. Test GetPriority với các champion khác nhau
        printf("=== Testing Priority Menu ===\n");
        
        // Test ADC champions (priority 5)
        int apheliosPrio = SDK::ChampionDatabase::GetPriority("Aphelios");
        int jinxPrio = SDK::ChampionDatabase::GetPriority("Jinx");
        printf("Aphelios Priority: %d (should be 5)\n", apheliosPrio);
        printf("Jinx Priority: %d (should be 5)\n", jinxPrio);
        
        // Test Tank champions (priority 1)
        int alistarPrio = SDK::ChampionDatabase::GetPriority("Alistar");
        int malphitePrio = SDK::ChampionDatabase::GetPriority("Malphite");
        printf("Alistar Priority: %d (should be 1)\n", alistarPrio);
        printf("Malphite Priority: %d (should be 1)\n", malphitePrio);
        
        // 2. Test SetPriority từ menu
        SDK::ChampionDatabase::SetPriority("Aphelios", 3); // Change to medium
        int newApheliosPrio = SDK::ChampionDatabase::GetPriority("Aphelios");
        printf("Aphelios New Priority: %d (should be 3)\n", newApheliosPrio);
        
        // 3. Test với enemy heroes trong game
        auto local = SDK::ObjectManager::GetLocalPlayer();
        auto heroes = SDK::ObjectManager::GetHeroes();
        
        if (local) {
            printf("\n=== Enemy Champions in Game ===\n");
            
            for (auto hero : heroes) {
                if (hero && hero->IsValid() && hero->IsEnemyTo(local)) {
                    std::string name = hero->GetName();
                    int priority = SDK::ChampionDatabase::GetPriority(name);
                    int defaultPrio = SDK::ChampionDatabase::GetDefaultPriority(name);
                    
                    printf("%s: Priority=%d (Default=%d)\n", name.c_str(), priority, defaultPrio);
                }
            }
        }
        
        // Cleanup
        delete local;
        for (auto h : heroes) delete h;
        
        printf("\n=== Priority Menu Test Complete ===\n");
    }
    
    // Test TargetSelector integration
    void TestTargetSelectorIntegration() {
        printf("\n=== Testing TargetSelector Integration ===\n");
        
        auto local = SDK::ObjectManager::GetLocalPlayer();
        auto heroes = SDK::ObjectManager::GetHeroes();
        
        if (local) {
            // Test GetTarget với Most Priority mode
            std::vector<SDK::GameObject*> enemies;
            
            for (auto hero : heroes) {
                if (hero && hero->IsValid() && hero->IsEnemyTo(local)) {
                    enemies.push_back(hero);
                }
            }
            
            if (!enemies.empty()) {
                // Tìm target với priority cao nhất
                SDK::GameObject* bestTarget = nullptr;
                int highestPriority = 0;
                
                for (auto enemy : enemies) {
                    int priority = SDK::TargetSelector::GetPriority(enemy);
                    printf("%s: Priority=%d\n", enemy->GetName().c_str(), priority);
                    
                    if (priority > highestPriority) {
                        highestPriority = priority;
                        bestTarget = enemy;
                    }
                }
                
                if (bestTarget) {
                    printf("\nBest Target (Highest Priority): %s (Priority=%d)\n", 
                           bestTarget->GetName().c_str(), highestPriority);
                }
            }
        }
        
        // Cleanup
        delete local;
        for (auto h : heroes) delete h;
    }
}

/*
MENU PRIORITY FEATURES ĐÃ IMPLEMENT:

1. ✅ Filter champions - Tìm kiếm champion theo tên
2. ✅ Current Game Enemies - Hiển thị enemy trong game trước tiên
3. ✅ Priority Slider (1-5) - Thay đổi priority cho từng champion
4. ✅ Color Coding - Màu sắc theo priority (Red=5, Orange=4, Yellow=3, Blue=1)
5. ✅ Default Priority Display - Hiển thị priority gốc trong ngoặc
6. ✅ Real-time Updates - Thay đổi có hiệu lực ngay lập tức

TƯƠNG THÍCH VỚI NewTargetSelector.cs:

- MenuSlider("TS_" + championName) -> SDK::ChampionDatabase::SetPriority()
- GetDefaultPriority() -> SDK::ChampionDatabase::GetDefaultPriority()
- Priority values: 1=Low, 2=Default, 3=Medium, 4=High, 5=Max
- Color scheme matches NewTargetSelector.cs logic

TODO NEXT STEPS:
1. Target Selection Mode (Smart AD/AP, Lowest Health, Most Priority)
2. Force/Only Selected Target checkboxes  
3. Drawing Settings (Draw Selected, Highlight, Color)
4. Current Target Info display
5. Manual target selection (click to select)
*/
