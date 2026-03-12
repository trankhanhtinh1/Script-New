#pragma once
#include "imgui/imgui.h"
#include "MenuConfig.h"
#include "plugins/PluginManager.h"

// ============================================================================
// Menu - ImGui Menu Rendering
// Toggle: INSERT key
// ============================================================================

namespace Menu {

    inline void DrawOrbwalkerTab();
    inline void DrawTargetSelectorTab();
    inline void DrawSpellsTab();
    inline void DrawEvadeTab();
    inline void DrawAwarenessTab();
    inline void DrawSmiteTab();
    inline void DrawZoomTab();
    inline void DrawMiscTab();
    inline void DrawDebugTab();

    enum class MainGroup {
        Combat,
        Visuals,
        Utility,
        Plugins,
        Debug
    };

    enum class MainPage {
        Orbwalker,
        TargetSelector,
        Spells,
        Evade,
        Awareness,
        Zoom,
        Smite,
        Misc,
        PluginsCore,
        PluginsChampion,
        PluginsUtility,
        PluginsMisc,
        Debug
    };

    struct GroupEntry {
        MainGroup Group;
        const char* Label;
    };

    struct PageEntry {
        MainPage Page;
        MainGroup Group;
        const char* Label;
    };

    inline const GroupEntry* GetGroupEntries(int& count) {
        static const GroupEntry groups[] = {
            { MainGroup::Combat,  "Combat"  },
            { MainGroup::Visuals, "Visuals" },
            { MainGroup::Utility, "Utility" },
            { MainGroup::Plugins, "Plugins" },
            { MainGroup::Debug,   "Debug"   }
        };
        count = IM_ARRAYSIZE(groups);
        return groups;
    }

    inline const PageEntry* GetPageEntries(int& count) {
        static const PageEntry pages[] = {
            { MainPage::Orbwalker,      MainGroup::Combat,  "Orbwalker" },
            { MainPage::TargetSelector, MainGroup::Combat,  "Target"    },
            { MainPage::Spells,         MainGroup::Combat,  "Spells"    },
            { MainPage::Evade,          MainGroup::Combat,  "Evade"     },
            { MainPage::Awareness,      MainGroup::Visuals, "Awareness" },
            { MainPage::Zoom,           MainGroup::Visuals, "Zoom"      },
            { MainPage::Smite,          MainGroup::Utility, "Smite"     },
            { MainPage::Misc,           MainGroup::Utility, "Misc"      },
            { MainPage::PluginsCore,     MainGroup::Plugins, "Core"      },
            { MainPage::PluginsChampion, MainGroup::Plugins, "Champion"  },
            { MainPage::PluginsUtility,  MainGroup::Plugins, "Utility"   },
            { MainPage::PluginsMisc,     MainGroup::Plugins, "Misc"      },
            { MainPage::Debug,          MainGroup::Debug,   "System"    }
        };
        count = IM_ARRAYSIZE(pages);
        return pages;
    }

    inline bool PageBelongsToGroup(MainPage page, MainGroup group) {
        int count = 0;
        const PageEntry* pages = GetPageEntries(count);
        for (int i = 0; i < count; ++i) {
            if (pages[i].Page == page) {
                return pages[i].Group == group;
            }
        }
        return false;
    }

    inline void EnsureValidPage(MainGroup group, MainPage& page) {
        if (PageBelongsToGroup(page, group)) {
            return;
        }

        int count = 0;
        const PageEntry* pages = GetPageEntries(count);
        for (int i = 0; i < count; ++i) {
            if (pages[i].Group == group) {
                page = pages[i].Page;
                return;
            }
        }
    }

    inline bool DrawSidebarButton(const char* label, bool active, const ImVec2& size) {
        ImVec4 base = active ? ImVec4(0.28f, 0.32f, 0.56f, 0.96f) : ImVec4(0.11f, 0.13f, 0.20f, 0.98f);
        ImVec4 hover = active ? ImVec4(0.33f, 0.38f, 0.64f, 1.0f) : ImVec4(0.16f, 0.19f, 0.29f, 1.0f);
        ImVec4 pressed = active ? ImVec4(0.24f, 0.28f, 0.49f, 1.0f) : ImVec4(0.20f, 0.23f, 0.35f, 1.0f);
        ImVec4 text = active ? ImVec4(0.96f, 0.97f, 1.0f, 1.0f) : ImVec4(0.78f, 0.81f, 0.90f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, base);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, pressed);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        bool clicked = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        return clicked;
    }

    inline void DrawPageContent(MainPage page) {
        switch (page) {
        case MainPage::Orbwalker:
            DrawOrbwalkerTab();
            break;
        case MainPage::TargetSelector:
            DrawTargetSelectorTab();
            break;
        case MainPage::Spells:
            DrawSpellsTab();
            break;
        case MainPage::Evade:
            DrawEvadeTab();
            break;
        case MainPage::Awareness:
            DrawAwarenessTab();
            break;
        case MainPage::Zoom:
            DrawZoomTab();
            break;
        case MainPage::Smite:
            DrawSmiteTab();
            break;
        case MainPage::Misc:
            DrawMiscTab();
            break;
        case MainPage::PluginsCore:
            Plugins::PluginManager::Get().OnMenu(Plugins::PluginCategory::CorePlugin);
            break;
        case MainPage::PluginsChampion:
            Plugins::PluginManager::Get().OnMenu(Plugins::PluginCategory::Champion);
            break;
        case MainPage::PluginsUtility:
            Plugins::PluginManager::Get().OnMenu(Plugins::PluginCategory::Utility);
            break;
        case MainPage::PluginsMisc:
            Plugins::PluginManager::Get().OnMenu(Plugins::PluginCategory::Misc);
            break;
        case MainPage::Debug:
            DrawDebugTab();
            break;
        }
    }

    // ---- Style Setup ----
    inline void ApplyStyle() {
        ImGuiStyle& style = ImGui::GetStyle();

        // Rounding
        style.WindowRounding    = 8.0f;
        style.FrameRounding     = 4.0f;
        style.GrabRounding      = 4.0f;
        style.TabRounding       = 4.0f;
        style.ChildRounding     = 4.0f;
        style.PopupRounding     = 4.0f;
        style.ScrollbarRounding = 6.0f;

        // Sizing
        style.WindowPadding     = ImVec2(12, 12);
        style.FramePadding      = ImVec2(8, 4);
        style.ItemSpacing       = ImVec2(8, 6);
        style.ItemInnerSpacing  = ImVec2(6, 4);
        style.WindowBorderSize  = 1.0f;
        style.FrameBorderSize   = 0.0f;

        // Colors - Dark Purple/Blue theme
        ImVec4* c = style.Colors;
        c[ImGuiCol_WindowBg]            = ImVec4(0.08f, 0.08f, 0.12f, 0.94f);
        c[ImGuiCol_ChildBg]             = ImVec4(0.10f, 0.10f, 0.15f, 0.60f);
        c[ImGuiCol_PopupBg]             = ImVec4(0.10f, 0.10f, 0.14f, 0.94f);
        c[ImGuiCol_Border]              = ImVec4(0.30f, 0.25f, 0.50f, 0.50f);
        c[ImGuiCol_FrameBg]             = ImVec4(0.15f, 0.14f, 0.22f, 0.80f);
        c[ImGuiCol_FrameBgHovered]      = ImVec4(0.25f, 0.22f, 0.40f, 0.80f);
        c[ImGuiCol_FrameBgActive]       = ImVec4(0.35f, 0.30f, 0.55f, 0.80f);
        c[ImGuiCol_TitleBg]             = ImVec4(0.06f, 0.06f, 0.10f, 1.00f);
        c[ImGuiCol_TitleBgActive]       = ImVec4(0.12f, 0.10f, 0.22f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed]    = ImVec4(0.06f, 0.06f, 0.10f, 0.60f);
        c[ImGuiCol_Tab]                 = ImVec4(0.15f, 0.13f, 0.25f, 0.86f);
        c[ImGuiCol_TabHovered]          = ImVec4(0.40f, 0.30f, 0.70f, 0.80f);
        c[ImGuiCol_TabActive]           = ImVec4(0.30f, 0.22f, 0.58f, 1.00f);
        c[ImGuiCol_TabUnfocused]        = ImVec4(0.10f, 0.09f, 0.18f, 0.97f);
        c[ImGuiCol_TabUnfocusedActive]  = ImVec4(0.18f, 0.15f, 0.35f, 1.00f);
        c[ImGuiCol_Header]              = ImVec4(0.20f, 0.18f, 0.35f, 0.55f);
        c[ImGuiCol_HeaderHovered]       = ImVec4(0.35f, 0.28f, 0.60f, 0.80f);
        c[ImGuiCol_HeaderActive]        = ImVec4(0.40f, 0.32f, 0.70f, 1.00f);
        c[ImGuiCol_Button]              = ImVec4(0.22f, 0.18f, 0.40f, 0.80f);
        c[ImGuiCol_ButtonHovered]       = ImVec4(0.35f, 0.28f, 0.60f, 0.90f);
        c[ImGuiCol_ButtonActive]        = ImVec4(0.45f, 0.35f, 0.75f, 1.00f);
        c[ImGuiCol_SliderGrab]          = ImVec4(0.45f, 0.35f, 0.75f, 1.00f);
        c[ImGuiCol_SliderGrabActive]    = ImVec4(0.55f, 0.45f, 0.85f, 1.00f);
        c[ImGuiCol_CheckMark]           = ImVec4(0.55f, 0.40f, 1.00f, 1.00f);
        c[ImGuiCol_Separator]           = ImVec4(0.30f, 0.25f, 0.50f, 0.50f);
        c[ImGuiCol_Text]                = ImVec4(0.92f, 0.90f, 0.96f, 1.00f);
        c[ImGuiCol_TextDisabled]        = ImVec4(0.45f, 0.42f, 0.52f, 1.00f);
        c[ImGuiCol_ScrollbarBg]         = ImVec4(0.08f, 0.08f, 0.12f, 0.60f);
        c[ImGuiCol_ScrollbarGrab]       = ImVec4(0.25f, 0.22f, 0.40f, 0.80f);
        c[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.35f, 0.30f, 0.55f, 0.80f);
        c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.38f, 0.70f, 1.00f);
    }

    // ---- Tab: Orbwalker ----
    inline void DrawOrbwalkerTab() {
        ImGui::Checkbox("Enable Orbwalker", &Config::Orbwalker::enabled);
        ImGui::Separator();

        ImGui::Text("Hotkeys:");
        ImGui::BulletText("Combo: SPACE");
        ImGui::BulletText("Harass: C");
        ImGui::BulletText("Lane Clear: V");
        ImGui::BulletText("Last Hit: X");

        ImGui::Separator();
        ImGui::SliderFloat("Hold Radius", &Config::Orbwalker::holdRadius, 0.0f, 200.0f, "%.0f");

        ImGui::Separator();
        ImGui::Text("Drawings:");
        ImGui::Checkbox("Attack Range", &Config::Orbwalker::drawAttackRange);
        ImGui::Checkbox("Target Circle", &Config::Orbwalker::drawTargetCircle);
        ImGui::Checkbox("Killable Indicator", &Config::Orbwalker::drawKillable);
    }

    // ---- Tab: Target Selector ----
    inline void DrawTargetSelectorTab() {
        const char* modes[] = { "Low HP", "Closest", "Most AD", "Most AP", "Priority" };
        ImGui::Combo("Target Mode", &Config::TargetSelector::mode, modes, IM_ARRAYSIZE(modes));
        ImGui::SliderFloat("Target Range", &Config::TargetSelector::range, 200.0f, 2000.0f, "%.0f");
        ImGui::Checkbox("Focus Selected Target", &Config::TargetSelector::focusSelected);
    }

    // ---- Tab: Spells ----
    inline void DrawSpellsTab() {
        // === Semi-Cast (Test) ===
        if (ImGui::CollapsingHeader("Semi-Cast (Test)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                "Press key to cast spell at mouse cursor");

            ImGui::Checkbox("Semi Q [S key]", &Config::Spells::semiQEnabled);
            ImGui::SameLine();
            ImGui::TextDisabled("(Ezreal Q - Mystic Shot)");

            ImGui::Checkbox("Semi R [T key]", &Config::Spells::semiREnabled);
            ImGui::SameLine();
            ImGui::TextDisabled("(Ezreal R - Trueshot)");

            ImGui::Separator();

            const char* methods[] = { "CastSpellSafe (FnCall)", "Key Simulation (Reliable)" };
            ImGui::Combo("Cast Method", &Config::Spells::castMethod, methods, IM_ARRAYSIZE(methods));

            ImGui::Checkbox("Show Cast Debug", &Config::Spells::showCastDebug);
        }

        ImGui::Separator();

        // === Auto Cast (in Combo) ===
        if (ImGui::CollapsingHeader("Auto Cast (in Combo)")) {
            ImGui::Checkbox("Auto Q", &Config::Spells::autoQ);
            ImGui::Checkbox("Auto W", &Config::Spells::autoW);
            ImGui::Checkbox("Auto E", &Config::Spells::autoE);
            ImGui::Checkbox("Auto R", &Config::Spells::autoR);
        }

        // === Drawings ===
        if (ImGui::CollapsingHeader("Drawings")) {
            ImGui::Checkbox("Draw Q Range", &Config::Spells::drawQRange);
            ImGui::Checkbox("Draw W Range", &Config::Spells::drawWRange);
            ImGui::Checkbox("Draw E Range", &Config::Spells::drawERange);
            ImGui::Checkbox("Draw R Range", &Config::Spells::drawRRange);
        }
    }

    // ---- Tab: Evade ----
    inline void DrawEvadeTab() {
        ImGui::Checkbox("Enable Evade", &Config::Evade::enabled);
        ImGui::Separator();

        ImGui::Checkbox("Dodge Skillshots", &Config::Evade::dodgeSkillshots);
        ImGui::Checkbox("Dodge Targeted", &Config::Evade::dodgeTargeted);

        const char* dangerLevels[] = { "Low", "Medium", "High", "Extreme" };
        int dangerIdx = Config::Evade::dangerLevel - 1;
        if (ImGui::Combo("Min Danger Level", &dangerIdx, dangerLevels, IM_ARRAYSIZE(dangerLevels))) {
            Config::Evade::dangerLevel = dangerIdx + 1;
        }

        ImGui::Separator();
        ImGui::Text("Drawings:");
        ImGui::Checkbox("Draw Skillshots", &Config::Evade::drawSpells);
        ImGui::Checkbox("Draw Safe Position", &Config::Evade::drawSafePos);
    }

    // ---- Tab: Awareness / ESP ----
    inline void DrawAwarenessTab() {
        ImGui::Checkbox("Enable Awareness", &Config::Awareness::enabled);
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Enemy Info", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("HP Bars", &Config::Awareness::drawEnemyHP);
            ImGui::Checkbox("Mana Bars", &Config::Awareness::drawEnemyMana);
            ImGui::Checkbox("Spell Cooldowns", &Config::Awareness::drawEnemySpells);
            ImGui::Checkbox("Attack Range", &Config::Awareness::drawEnemyRange);
            ImGui::Checkbox("Movement Path", &Config::Awareness::drawEnemyPath);
        }

        if (ImGui::CollapsingHeader("Ally Info")) {
            ImGui::Checkbox("HP Bars##ally", &Config::Awareness::drawAllyHP);
        }

        if (ImGui::CollapsingHeader("Self")) {
            ImGui::Checkbox("Attack Range##self", &Config::Awareness::drawSelfRange);
        }

        if (ImGui::CollapsingHeader("Jungle")) {
            ImGui::Checkbox("Jungle HP", &Config::Awareness::drawJungleHP);
            ImGui::Checkbox("Jungle Timer", &Config::Awareness::jungleTimer);
        }

        if (ImGui::CollapsingHeader("Utility")) {
            ImGui::Checkbox("Ward Timers", &Config::Awareness::wardTimer);
            ImGui::Checkbox("Track Recalls", &Config::Awareness::trackRecall);
        }
    }

    // ---- Tab: Auto Smite ----
    inline void DrawSmiteTab() {
        ImGui::Checkbox("Enable Auto Smite", &Config::AutoSmite::enabled);
        ImGui::Separator();

        ImGui::Text("Targets:");
        ImGui::Checkbox("Dragon", &Config::AutoSmite::smiteDragon);
        ImGui::Checkbox("Baron Nashor", &Config::AutoSmite::smiteBaron);
        ImGui::Checkbox("Rift Herald", &Config::AutoSmite::smiteHerald);
        ImGui::Checkbox("Voidgrub (Horde)", &Config::AutoSmite::smiteHorde);

        ImGui::Separator();
        ImGui::Checkbox("Draw Smite Indicator", &Config::AutoSmite::drawIndicator);
    }

    // ---- Tab: Zoom ----
    inline void DrawZoomTab() {
        ImGui::Checkbox("Enable Zoom Hack", &Config::ZoomHack::enabled);
        ImGui::SliderFloat("Max Zoom", &Config::ZoomHack::maxZoom, 1000.0f, 10000.0f, "%.0f");
        ImGui::SliderFloat("Current Zoom", &Config::ZoomHack::zoomValue, 1000.0f, Config::ZoomHack::maxZoom, "%.0f");

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Default max: ~2250");
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Recommended: 3000-5000");
    }

    // ---- Tab: Misc ----
    inline void DrawMiscTab() {
        ImGui::Checkbox("Anti-AFK", &Config::Misc::antiAFK);
        ImGui::Checkbox("Auto Accept Queue", &Config::Misc::autoAccept);

        ImGui::Separator();
        ImGui::Text("Info Overlay:");
        ImGui::Checkbox("Show FPS", &Config::Misc::showFPS);
        ImGui::Checkbox("Show Ping", &Config::Misc::showPing);
        ImGui::Checkbox("Show Game Time", &Config::Misc::showGameTime);
    }

    // ---- Tab: Debug / Info ----
    inline void DrawDebugTab() {
        ImGui::Text("=== Debug Info ===");
        ImGui::Separator();

        ImGui::Text("Module Base: 0x%llX", (unsigned long long)Globals::base);
        if (!Globals::base) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Module base not initialized!");
            return;
        }

        // === LocalPlayer ===
        uintptr_t localPlayer = Globals::Read<uintptr_t>(Globals::base + Offset::Global::LocalPlayer);
        ImGui::Text("LocalPlayer: 0x%llX", (unsigned long long)localPlayer);

        if (Globals::IsValidPtr(localPlayer)) {
            // Position (direct Vec3, no encryption)
            Vec3 pos = Globals::Read<Vec3>(localPlayer + Offset::GameObject::Position);
            ImGui::Text("Position: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);

            // Team (byte at 0x259, NOT int at 0x3C)
            int team = (int)Globals::Read<unsigned char>(localPlayer + Offset::GameObject::TeamAlt);
            ImGui::Text("Team: %d (%s)", team, team == 100 ? "Blue" : team == 200 ? "Red" : "Unknown");

            // NetId
            int netId = Globals::Read<int>(localPlayer + Offset::GameObject::NetId);
            ImGui::Text("NetId: 0x%X", netId);

            // ---- Stats: Try DIRECT float read first (Script-New-main pattern) ----
            // NOTE: Script-New-main reads stats as direct *(float*) at offset
            // If obfuscation is active, these will show garbage -> use decrypt
            ImGui::Separator();
            ImGui::Text("=== Stats (Direct Float Read) ===");

            float hp_direct   = Globals::Read<float>(localPlayer + Offset::Health::HP);
            float maxhp_direct = Globals::Read<float>(localPlayer + Offset::Health::MaxHP);
            float mp_direct   = Globals::Read<float>(localPlayer + Offset::Mana::MP);
            float maxmp_direct = Globals::Read<float>(localPlayer + Offset::Mana::MaxMP);
            float ad_direct   = Globals::Read<float>(localPlayer + Offset::HeroStats::BaseAttackDamage);
            float ap_direct   = Globals::Read<float>(localPlayer + Offset::HeroStats::BaseAbilityDamage);
            float armor_direct = Globals::Read<float>(localPlayer + Offset::HeroStats::Armor);
            float mr_direct   = Globals::Read<float>(localPlayer + Offset::HeroStats::SpellBlock);
            float ms_direct   = Globals::Read<float>(localPlayer + Offset::HeroStats::MoveSpeed);
            float range_direct = Globals::Read<float>(localPlayer + Offset::HeroStats::AttackRange);

            bool directLooksValid = (hp_direct > 0.0f && hp_direct < 100000.0f &&
                                     maxhp_direct > 0.0f && maxhp_direct < 100000.0f);

            if (directLooksValid) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[Direct reads look valid]");
                ImGui::Text("HP: %.0f / %.0f", hp_direct, maxhp_direct);
                ImGui::Text("Mana: %.0f / %.0f", mp_direct, maxmp_direct);
                ImGui::Text("AD: %.1f  |  AP: %.1f", ad_direct, ap_direct);
                ImGui::Text("Armor: %.1f  |  MR: %.1f", armor_direct, mr_direct);
                ImGui::Text("MS: %.0f  |  Range: %.0f", ms_direct, range_direct);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "[Direct reads invalid -> trying decrypt]");
                ImGui::Text("Raw HP direct: %.6f (0x%X)", hp_direct, *(uint32_t*)&hp_direct);

                // Try LeagueObfuscation decrypt
                auto hpData = Globals::Read<LeagueObfuscation<float>>(localPlayer + Offset::Health::HP);
                float hp = Decrypt(hpData);
                auto maxHpData = Globals::Read<LeagueObfuscation<float>>(localPlayer + Offset::Health::MaxHP);
                float maxHp = Decrypt(maxHpData);

                bool decryptValid = (hp > 0.0f && hp < 100000.0f);
                if (decryptValid) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[Decrypt works]");
                    ImGui::Text("HP: %.0f / %.0f", hp, maxHp);

                    auto adData = Globals::Read<LeagueObfuscation<float>>(localPlayer + Offset::HeroStats::BaseAttackDamage);
                    ImGui::Text("AD: %.1f", Decrypt(adData));
                    auto armorData = Globals::Read<LeagueObfuscation<float>>(localPlayer + Offset::HeroStats::Armor);
                    ImGui::Text("Armor: %.1f", Decrypt(armorData));
                    auto msData = Globals::Read<LeagueObfuscation<float>>(localPlayer + Offset::HeroStats::MoveSpeed);
                    ImGui::Text("MS: %.0f", Decrypt(msData));
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[Both methods failed]");
                    ImGui::Text("Check LeagueObfuscation struct layout!");
                }
            }

            // ---- Spells ----
            ImGui::Separator();
            ImGui::Text("=== Spells ===");
            const char* slotNames[] = { "Q", "W", "E", "R", "D", "F" };
            uintptr_t spellBookBase = localPlayer + Offset::SpellBook::Offset;
            // Get current game time for CD calculation
            float curTime = Globals::Read<float>(Globals::base + Offset::Global::GameTime);
            if (curTime <= 0.0f || curTime > 100000.0f) {
                uintptr_t gtPtr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::GameTime);
                if (Globals::IsValidPtr(gtPtr)) curTime = Globals::Read<float>(gtPtr);
            }
            for (int i = 0; i < 6; i++) {
                uintptr_t spellSlot = Globals::Read<uintptr_t>(spellBookBase + Offset::SpellBook::SpellSlotArray + i * 8);
                if (Globals::IsValidPtr(spellSlot)) {
                    int level = Globals::Read<int>(spellSlot + Offset::SpellBook::SlotLevel);
                    float cdReadyAt = Globals::Read<float>(spellSlot + Offset::SpellBook::SlotCooldown);
                    float remaining = cdReadyAt - curTime;
                    if (remaining <= 0.0f) {
                        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "  [%s] Lv:%d  READY", slotNames[i], level);
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "  [%s] Lv:%d  CD:%.1fs", slotNames[i], level, remaining);
                    }
                } else {
                    ImGui::TextDisabled("  [%s] N/A", slotNames[i]);
                }
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "LocalPlayer not found (not in game?)");
        }

        // === Game Time ===
        ImGui::Separator();
        float gameTime = Globals::Read<float>(Globals::base + Offset::Global::GameTime);
        if (gameTime > 0.0f && gameTime < 100000.0f) {
            int minutes = (int)gameTime / 60;
            int seconds = (int)gameTime % 60;
            ImGui::Text("Game Time: %d:%02d (%.1f)", minutes, seconds, gameTime);
        } else {
            // GameTime might be a pointer -> dereference
            uintptr_t gtPtr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::GameTime);
            if (Globals::IsValidPtr(gtPtr)) {
                float gt2 = Globals::Read<float>(gtPtr);
                if (gt2 > 0.0f && gt2 < 100000.0f) {
                    int minutes = (int)gt2 / 60;
                    int seconds = (int)gt2 % 60;
                    ImGui::Text("Game Time (ptr): %d:%02d (%.1f)", minutes, seconds, gt2);
                } else {
                    ImGui::Text("Game Time: invalid (raw: 0x%llX)", (unsigned long long)gtPtr);
                }
            } else {
                ImGui::Text("Game Time: N/A (0x%X)", *(uint32_t*)&gameTime);
            }
        }

        // === HUD ===
        ImGui::Separator();
        uintptr_t hud = Globals::Read<uintptr_t>(Globals::base + Offset::Global::HudInstance);
        ImGui::Text("HUD: 0x%llX", (unsigned long long)hud);

        if (Globals::IsValidPtr(hud)) {
            uintptr_t camera = Globals::Read<uintptr_t>(hud + Offset::Hud::Camera);
            ImGui::Text("Camera: 0x%llX", (unsigned long long)camera);
            if (Globals::IsValidPtr(camera)) {
                float zoom = Globals::Read<float>(camera + Offset::Hud::CameraZoom);
                ImGui::Text("  Zoom: %.1f", zoom);
            }
        }

        // === Other Managers ===
        ImGui::Separator();
        ImGui::Text("=== Manager Pointers ===");
        uintptr_t objMgr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::ObjectManager);
        uintptr_t heroMgr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::HeroManager);
        uintptr_t missMgr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::MissileManager);
        uintptr_t navGrid = Globals::Read<uintptr_t>(Globals::base + Offset::Global::NavGrid);
        ImGui::Text("ObjMgr: 0x%llX %s", (unsigned long long)objMgr, Globals::IsValidPtr(objMgr) ? "[OK]" : "[BAD]");
        ImGui::Text("HeroMgr: 0x%llX %s", (unsigned long long)heroMgr, Globals::IsValidPtr(heroMgr) ? "[OK]" : "[BAD]");
        ImGui::Text("MissMgr: 0x%llX %s", (unsigned long long)missMgr, Globals::IsValidPtr(missMgr) ? "[OK]" : "[BAD]");
        ImGui::Text("NavGrid: 0x%llX %s", (unsigned long long)navGrid, Globals::IsValidPtr(navGrid) ? "[OK]" : "[BAD]");
    }

    // ==================================================================
    // Main Render - Called from Present hook each frame
    // ==================================================================
    inline void Render() {
        // Toggle menu
        static bool keyWasDown = false;
        bool keyIsDown = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;  // F1 toggle
        if (keyIsDown && !keyWasDown) {
            Config::showMenu = !Config::showMenu;
        }
        keyWasDown = keyIsDown;

        if (!Config::showMenu)
            return;

        // Style (apply once)
        static bool styleApplied = false;
        if (!styleApplied) {
            ApplyStyle();
            styleApplied = true;
        }

        static MainGroup activeGroup = MainGroup::Combat;
        static MainPage activePage = MainPage::Orbwalker;

        EnsureValidPage(activeGroup, activePage);

        // Main window
        ImGui::SetNextWindowSize(ImVec2(980, 580), ImGuiCond_FirstUseEver);
        ImGui::Begin("League Tool v1.0", &Config::showMenu,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

        ImGui::TextColored(ImVec4(0.82f, 0.86f, 0.96f, 1.0f), "Sliderbar 1  ->  Sliderbar 2  ->  Content");
        ImGui::SameLine();
        ImGui::TextDisabled("| F1 toggle");
        ImGui::Separator();

        const float groupBarWidth = 150.0f;
        const float pageBarWidth = 182.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

        ImGui::BeginChild("##main_group_bar", ImVec2(groupBarWidth, 0.0f), true);
        ImGui::TextColored(ImVec4(0.47f, 0.92f, 0.47f, 1.0f), "Sliderbar 1");
        ImGui::Separator();
        int groupCount = 0;
        const GroupEntry* groups = GetGroupEntries(groupCount);
        for (int i = 0; i < groupCount; ++i) {
            if (DrawSidebarButton(groups[i].Label, groups[i].Group == activeGroup, ImVec2(-1.0f, 36.0f))) {
                activeGroup = groups[i].Group;
                EnsureValidPage(activeGroup, activePage);
            }
        }
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 10.0f);

        ImGui::BeginChild("##main_page_bar", ImVec2(pageBarWidth, 0.0f), true);
        ImGui::TextColored(ImVec4(0.47f, 0.92f, 0.47f, 1.0f), "Sliderbar 2");
        ImGui::Separator();
        int pageCount = 0;
        const PageEntry* pages = GetPageEntries(pageCount);
        for (int i = 0; i < pageCount; ++i) {
            if (pages[i].Group != activeGroup) {
                continue;
            }

            if (DrawSidebarButton(pages[i].Label, pages[i].Page == activePage, ImVec2(-1.0f, 34.0f))) {
                activePage = pages[i].Page;
            }
        }
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 10.0f);

        ImGui::BeginChild("##main_content_panel", ImVec2(0.0f, 0.0f), true);
        for (int i = 0; i < pageCount; ++i) {
            if (pages[i].Page == activePage) {
                ImGui::TextColored(ImVec4(0.37f, 0.62f, 0.84f, 1.0f), "%s", pages[i].Label);
                break;
            }
        }
        ImGui::Separator();
        DrawPageContent(activePage);
        ImGui::EndChild();

        ImGui::PopStyleVar();

        ImGui::End();
    }

} // namespace Menu
