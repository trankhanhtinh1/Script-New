#include <Windows.h>

#include <cstring>

#include <NightSharp.SDK.h>

namespace {

bool g_enabled = true;
bool g_useQ = false;
bool g_drawQ = true;
Spell g_q(SpellSlot::Q, 1150.0f);
Vec3 g_lastPredictedCastPosition = {};
unsigned g_updateCount = 0;

bool IsEzrealLoaded() {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) {
        return true;
    }

    return _stricmp(player.CharacterName().c_str(), "Ezreal") == 0;
}

bool NIGHTSHARP_PLUGIN_CALL PluginCanLoad() {
    return IsEzrealLoaded();
}

void SetupQ() {
    g_q.Range = 1150.0f;
    g_q.Delay = 0.25f;
    g_q.Width = 60.0f;
    g_q.Speed = 2000.0f;
    g_q.Collision = true;
    g_q.Type = SkillshotType::SkillshotLine;
    g_q.DamageType = DamageType::Physical;
    g_q.MinHitChance = HitChance::High;
}

void NIGHTSHARP_PLUGIN_CALL PluginOnLoad() {
    g_enabled = true;
    g_useQ = false;
    g_drawQ = true;
    SetupQ();
}

void NIGHTSHARP_PLUGIN_CALL PluginOnUnload() {
    g_updateCount = 0;
}

void NIGHTSHARP_PLUGIN_CALL PluginOnUpdate() {
    ++g_updateCount;
    if (!g_enabled ||
        !g_useQ ||
        !IsEzrealLoaded()) {
        return;
    }

    if (!g_q.IsReady()) {
        return;
    }

    AIHeroClient target;
    for (const auto& hero : GameObjects::EnemyHeroes()) {
        if (IsValidTarget(hero, g_q.CurrentRange())) {
            target = hero;
            break;
        }
    }

    if (!target.IsValid()) {
        return;
    }

    const auto prediction = g_q.GetPrediction(target);
    if (static_cast<int>(prediction.Hitchance) < static_cast<int>(HitChance::High)) {
        return;
    }

    g_lastPredictedCastPosition = prediction.GetCastPosition();
}

void NIGHTSHARP_PLUGIN_CALL PluginOnRender() {
    if (!g_enabled || !g_drawQ) {
        return;
    }

    const auto player = GameObjects::Player();
    if (!player.IsValid()) {
        return;
    }

    (void)player.Position();
    (void)g_q.CurrentRange();
}

void NIGHTSHARP_PLUGIN_CALL PluginOnMenu() {
    (void)g_updateCount;
    (void)g_lastPredictedCastPosition;
}

const auto kDescriptor = NIGHTSHARP_PLUGIN_DESCRIPTOR(
    "Sample Ezreal",
    "sample.champion.ezreal",
    "NightSharp",
    NightSharp::Plugin::Category::Champion,
    "Ezreal",
    false);

} // namespace

NIGHTSHARP_PLUGIN_EXPORT(
    kDescriptor,
    &PluginCanLoad,
    &PluginOnLoad,
    &PluginOnUnload,
    &PluginOnUpdate,
    &PluginOnRender,
    &PluginOnMenu)

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
