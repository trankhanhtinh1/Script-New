#pragma once

#include "../../../SDK/SDK.h"
#include "../../../SDK/MenuSDK/Integration/MenuSDKBridge.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>

namespace Plugins::ziblldev9898::AioMenu {

inline constexpr const char* RootId = "champion.ziblldev9898";
inline std::unordered_map<std::string, NightSharp::Menu::MenuItemHandle> Items;
inline NightSharp::Menu::MenuNodeHandle RootNode;
inline NightSharp::Menu::MenuNodeHandle LockeNode;
inline NightSharp::Menu::MenuNodeHandle EzrealNode;
inline NightSharp::Menu::MenuNodeHandle LeeSinNode;
inline NightSharp::Menu::MenuNodeHandle IreliaNode;

inline std::string ItemKey(const char* section, const char* id) {
    return std::string(section ? section : "") + "." + (id ? id : "");
}

inline void Store(
    const char* section,
    const char* id,
    NightSharp::Menu::MenuItemHandle item) {
    if (item) {
        Items[ItemKey(section, id)] = std::move(item);
    }
}

inline NightSharp::Menu::MenuItemHandle Find(
    const char* section,
    const char* id) {
    const auto found = Items.find(ItemKey(section, id));
    return found == Items.end() ? nullptr : found->second;
}

inline bool Bool(
    const char* section,
    const char* id,
    bool fallback = true) {
    const auto item = Find(section, id);
    return item ? item->value : fallback;
}

inline int Slider(
    const char* section,
    const char* id,
    int fallback = 0) {
    const auto item = Find(section, id);
    return item ? item->integer : fallback;
}

inline int Key(
    const char* section,
    const char* id,
    int fallback = 0) {
    const auto item = Find(section, id);
    return item ? item->key : fallback;
}

inline bool KeyDown(
    const char* section,
    const char* id) {
    const int key = Key(section, id);
    return key > 0 && (::GetAsyncKeyState(key) & 0x8000) != 0;
}

inline void SetActiveChampion(const char* championName) {
    const bool isLocke = championName && _stricmp(championName, "Locke") == 0;
    const bool isEzreal = championName && _stricmp(championName, "Ezreal") == 0;
    const bool isLeeSin = championName && _stricmp(championName, "LeeSin") == 0;
    const bool isIrelia = championName && _stricmp(championName, "Irelia") == 0;
    if (LockeNode) LockeNode->visible = isLocke;
    if (EzrealNode) EzrealNode->visible = isEzreal;
    if (LeeSinNode) LeeSinNode->visible = isLeeSin;
    if (IreliaNode) IreliaNode->visible = isIrelia;
}

inline void Register() {
    auto& bridge = NightSharpMenu::MenuSDKBridge::Instance();
    auto root = bridge.RegisterMenu(RootId, "Ziblldev9898");
    if (!root.IsValid()) {
        return;
    }
    RootNode = root.Node();
    Items.clear();

    auto locke = root.Section("locke", "Locke");
    LockeNode = locke.Node();
    auto lockeCombo = locke.Section("combo", "Combo");
    Store("locke.combo", "useQ", lockeCombo.Checkbox("useQ", "Use Q", true));
    Store("locke.combo", "useE", lockeCombo.Checkbox("useE", "Use E", true));
    Store("locke.combo", "eMinStacks", lockeCombo.Slider("eMinStacks", "Min Q Stacks to E", 2, 1, 3));
    Store("locke.combo", "useEGapclose", lockeCombo.Checkbox("useEGapclose", "Use E to Escape Gapclosers", true));
    Store("locke.combo", "eGapcloseHpPercent", lockeCombo.Slider("eGapcloseHpPercent", "Don't Escape Below HP %", 50, 0, 100));
    Store("locke.combo", "useW", lockeCombo.Checkbox("useW", "Use W", true));
    Store("locke.combo", "wRecastHpPercent", lockeCombo.Slider("wRecastHpPercent", "Recast W Below HP %", 30, 0, 100));
    Store("locke.combo", "wRecastExpiryTime", lockeCombo.Slider("wRecastExpiryTime", "Recast W When Buff Has X*0.1s Left", 15, 5, 30));
    Store("locke.combo", "useR", lockeCombo.Checkbox("useR", "Use R", true));
    Store("locke.combo", "rExecute", lockeCombo.Checkbox("rExecute", "Use R Execute", true));
    Store("locke.combo", "rAoe", lockeCombo.Checkbox("rAoe", "Use R AoE", true));
    Store("locke.combo", "rMinEnemies", lockeCombo.Slider("rMinEnemies", "Min Enemies for R AoE", 4, 1, 5));
    auto lockeHarass = locke.Section("harass", "Harass");
    Store("locke.harass", "useQ", lockeHarass.Checkbox("useQ", "Use Q", true));
    Store("locke.harass", "ManaHarass", lockeHarass.Slider("ManaHarass", "Mana Harass", 30, 0, 100));
    auto lockeLane = locke.Section("laneClear", "Lane Clear");
    Store("locke.lane", "useQ", lockeLane.Checkbox("useQ", "Use Q", true));
    Store("locke.lane", "ManaLC", lockeLane.Slider("ManaLC", "Mana Clear", 30, 0, 100));
    auto lockeJungle = locke.Section("jungleClear", "Jungle Clear");
    Store("locke.jungle", "useQ", lockeJungle.Checkbox("useQ", "Use Q", true));
    Store("locke.jungle", "ManaJC", lockeJungle.Slider("ManaJC", "Mana Clear", 30, 0, 100));
    auto lockeKillSteal = locke.Section("killSteal", "KillSteal");
    Store("locke.killSteal", "killstealQ", lockeKillSteal.Checkbox("killstealQ", "Use Q", true));
    Store("locke.killSteal", "killstealR", lockeKillSteal.Checkbox("killstealR", "Use R", true));

    auto ezreal = root.Section("ezreal", "Ezreal");
    EzrealNode = ezreal.Node();
    Store("ezreal", "useQ", ezreal.Checkbox("useQ", "Cast Q In Combo", true));

    auto leesin = root.Section("leesin", "Lee Sin");
    LeeSinNode = leesin.Node();
    auto leeCombo = leesin.Section("combo", "Combo");
    Store("leesin.combo", "useQ", leeCombo.Checkbox("useQ", "Use Q", true));
    Store("leesin.combo", "useW", leeCombo.Checkbox("useW", "Use W", true));
    Store("leesin.combo", "useE", leeCombo.Checkbox("useE", "Use E", true));
    Store("leesin.combo", "drawPassive", leeCombo.Checkbox("drawPassive", "Draw Passive Stacks", true));
    Store("leesin.combo", "drawCombo", leeCombo.Checkbox("drawCombo", "Draw Kill Combo", true));
    Store("leesin.combo", "wardHop", leeCombo.Checkbox("wardHop", "Use Ward Hop", true));
    Store("leesin.combo", "wardHopKey", leeCombo.KeyBind("wardHopKey", "Ward Hop Key", SDK::Keys::Z));
    Store("leesin.combo", "insecKey", leeCombo.KeyBind("insecKey", "Insec Key", SDK::Keys::A));
    Store("leesin.combo", "fancyCombo", leeCombo.Checkbox("fancyCombo", "Fancy Combo", false));
    Store("leesin.combo", "fancyQ2Health", leeCombo.Slider("fancyQ2Health", "Q2 Health Threshold (%)", 30, 0, 100));
    Store("leesin.combo", "fancyQ2Enemies", leeCombo.Slider("fancyQ2Enemies", "Q2 Max Nearby Enemies", 1, 0, 5));
    Store("leesin.combo", "multiRCombo", leeCombo.Checkbox("multiRCombo", "Multi-Enemy R Kick", false));
    Store("leesin.combo", "multiRMinCollisions", leeCombo.Slider("multiRMinCollisions", "Minimum R Collisions", 2, 2, 5));
    auto leeKillSteal = leesin.Section("killSteal", "KillSteal");
    Store("leesin.killSteal", "rAutoKillSteal", leeKillSteal.Checkbox("rAutoKillSteal", "R Auto KS", false));

    auto irelia = root.Section("irelia", "Irelia");
    IreliaNode = irelia.Node();
    auto ireliaCombo = irelia.Section("combo", "Combo");
    Store("irelia.combo", "useQ", ireliaCombo.Checkbox("useQ", "Use Q", true));
    Store("irelia.combo", "useW", ireliaCombo.Checkbox("useW", "Use W", true));
    Store("irelia.combo", "useE", ireliaCombo.Checkbox("useE", "Use E", true));
    Store("irelia.combo", "useR", ireliaCombo.Checkbox("useR", "Use R", true));
    Store("irelia.combo", "rExecute", ireliaCombo.Checkbox("rExecute", "Use R Execute", true));
    Store("irelia.combo", "rAoe", ireliaCombo.Checkbox("rAoe", "Use R AoE", true));
    Store("irelia.combo", "rMinEnemies", ireliaCombo.Slider("rMinEnemies", "Min Enemies for R AoE", 3, 1, 5));
    auto ireliaHarass = irelia.Section("harass", "Harass");
    Store("irelia.harass", "useQ", ireliaHarass.Checkbox("useQ", "Use Q", true));
    Store("irelia.harass", "ManaHarass", ireliaHarass.Slider("ManaHarass", "Mana Harass", 30, 0, 100));
    auto ireliaLane = irelia.Section("laneClear", "Lane Clear");
    Store("irelia.lane", "useQ", ireliaLane.Checkbox("useQ", "Use Q (lasthit)", true));
    Store("irelia.lane", "ManaLC", ireliaLane.Slider("ManaLC", "Mana Clear", 30, 0, 100));
    auto ireliaJungle = irelia.Section("jungleClear", "Jungle Clear");
    Store("irelia.jungle", "useQ", ireliaJungle.Checkbox("useQ", "Use Q", true));
    Store("irelia.jungle", "ManaJC", ireliaJungle.Slider("ManaJC", "Mana Clear", 30, 0, 100));
    auto ireliaKillSteal = irelia.Section("killSteal", "KillSteal");
    Store("irelia.killSteal", "killstealQ", ireliaKillSteal.Checkbox("killstealQ", "Use Q", true));
    Store("irelia.killSteal", "killstealR", ireliaKillSteal.Checkbox("killstealR", "Use R", true));
}

inline void Unregister() {
    NightSharpMenu::MenuSDKBridge::Instance().UnregisterMenu(RootId);
    RootNode.reset();
    LockeNode.reset();
    EzrealNode.reset();
    LeeSinNode.reset();
    IreliaNode.reset();
    Items.clear();
}

}
