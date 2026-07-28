#pragma once
#include "IDeveloperTab.h"
#include "../DeveloperToolsPlugin.h"
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>

namespace Plugins::DevTools {

class SpellItemInspectorTab final : public IDeveloperTab {
private:
    struct SpellSlotInfo {
        SDK::SpellSlot slot;
        std::string name;
        int state = 0;
        float remainingCD = 0.0f;
        float totalCD = 0.0f;
        int level = 0;
        int ammo = 0;
        int maxAmmo = 0;
        float manaCost = 0.0f;
        bool isValid = false;
    };

    struct LiveSpellbookTargetInfo {
        bool isValid = false;
        std::string characterName;
        std::string name;
        uintptr_t address = 0;
        bool isLocalPlayer = false;
        bool hasSpellbook = false;
        std::vector<SpellSlotInfo> spells;
    };

    int inspectMode_ = 0; // 0 = Live, 1 = Snapshot
    mutable std::mutex spellbookMutex_;
    LiveSpellbookTargetInfo cachedSpellbookTarget_;
    int lastUpdateTick_ = 0;

public:
    using IDeveloperTab::IDeveloperTab;

    const char* GetTabName() const override { return "Spellbook"; }

    void OnUpdate() override {
        if (!plugin_->enabled_) {
            std::lock_guard<std::mutex> lk(spellbookMutex_);
            cachedSpellbookTarget_ = {};
            return;
        }

        if (inspectMode_ == 1) {
            std::lock_guard<std::mutex> lk(spellbookMutex_);
            cachedSpellbookTarget_ = {};
            return;
        }

        const int now = SDK::Variables::TickCount();
        if (now - lastUpdateTick_ < 100) {
            return;
        }
        lastUpdateTick_ = now;

        const auto obj = plugin_->GetFocusedObject();
        LiveSpellbookTargetInfo targetInfo;
        
        if (obj.IsValid()) {
            targetInfo.isValid = true;
            targetInfo.characterName = obj.CharacterName();
            targetInfo.name = obj.Name();
            targetInfo.address = obj.Address();
            targetInfo.hasSpellbook = obj.IsHero() || obj.IsMinion() || obj.IsTurret();
            
            const auto localPlayer = SDK::ObjectManager::Player();
            targetInfo.isLocalPlayer = localPlayer.IsValid() && obj.Address() == localPlayer.Address();
            
            if (targetInfo.hasSpellbook) {
                const auto player = SDK::AIBaseClient(obj.Handle());
                if (player.IsValid()) {
                    const float gameTime = SDK::Game::Time();
                    
                    static const SDK::SpellSlot slots[] = {
                        SDK::SpellSlot::Q, SDK::SpellSlot::W, SDK::SpellSlot::E, SDK::SpellSlot::R,
                        SDK::SpellSlot::Summoner1, SDK::SpellSlot::Summoner2,
                        SDK::SpellSlot::Item1, SDK::SpellSlot::Item2, SDK::SpellSlot::Item3,
                        SDK::SpellSlot::Item4, SDK::SpellSlot::Item5, SDK::SpellSlot::Item6,
                        SDK::SpellSlot::Trinket
                    };
                    
                    for (const auto slot : slots) {
                        auto spell = player.GetSpell(slot);
                        SpellSlotInfo info;
                        info.slot = slot;
                        if (spell.IsValid()) {
                            info.isValid = true;
                            info.name = spell.Name();
                            info.state = static_cast<int>(spell.State(gameTime));
                            info.remainingCD = spell.RemainingCooldown(gameTime);
                            info.totalCD = spell.Cooldown();
                            info.level = spell.Level();
                            info.ammo = spell.Ammo();
                            info.maxAmmo = spell.MaxAmmo();
                            info.manaCost = spell.ManaCost();
                        }
                        targetInfo.spells.push_back(std::move(info));
                    }
                }
            }
        }
        
        std::lock_guard<std::mutex> lk(spellbookMutex_);
        cachedSpellbookTarget_ = std::move(targetInfo);
    }

    void OnDrawTab() override {
        ImGui::RadioButton("Live Object", &inspectMode_, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Snapshot Object", &inspectMode_, 1);

        std::string charName;
        std::string name;
        uintptr_t address = 0;
        bool isLocalPlayer = false;
        bool hasTarget = false;
        bool hasSpellbook = false;

        DevTools::ObjectSnapshot selectedSnap;
        LiveSpellbookTargetInfo liveTarget;

        if (inspectMode_ == 0) {
            {
                std::lock_guard<std::mutex> lk(spellbookMutex_);
                liveTarget = cachedSpellbookTarget_;
            }
            if (liveTarget.isValid) {
                charName = liveTarget.characterName;
                name = liveTarget.name;
                address = liveTarget.address;
                isLocalPlayer = liveTarget.isLocalPlayer;
                hasSpellbook = liveTarget.hasSpellbook;
                hasTarget = true;

                if (!hasSpellbook) {
                    ImGui::Text("Inspecting Live: %s (%s) | Addr: 0x%llX", charName.c_str(), name.c_str(), static_cast<unsigned long long>(address));
                    ImGui::Text("This object type has no Spellbook.");
                    return;
                }
            } else {
                ImGui::Text("No focused live object in range.");
                return;
            }
        } else {
            if (plugin_->snapshots_.empty()) {
                ImGui::Text("No snapshots in memory. Hover an object and press key 'M' to take a snapshot!");
                return;
            }
            static int selectedSnapIdx = 0;
            if (selectedSnapIdx >= static_cast<int>(plugin_->snapshots_.size())) {
                selectedSnapIdx = 0;
            }
            std::vector<std::string> comboLabels;
            std::vector<const char*> comboItems;
            for (std::size_t idx = 0; idx < plugin_->snapshots_.size(); ++idx) {
                const auto& s = plugin_->snapshots_[idx];
                std::string label = s.characterName + " (NetID: " + std::to_string(s.networkId) + ")";
                if (!s.note.empty()) {
                    label += " [" + s.note + "]";
                }
                comboLabels.push_back(label);
            }
            for (const auto& l : comboLabels) {
                comboItems.push_back(l.c_str());
            }
            ImGui::Combo("Select SnapshotTarget", &selectedSnapIdx, comboItems.data(), static_cast<int>(comboItems.size()));
            selectedSnap = plugin_->snapshots_[selectedSnapIdx];
            charName = selectedSnap.characterName;
            name = selectedSnap.name;
            address = selectedSnap.address;
            isLocalPlayer = false;
            hasTarget = true;
        }

        if (!hasTarget) {
            return;
        }

        ImGui::Text("Inspecting %s: %s (%s) | Addr: 0x%llX",
                    inspectMode_ == 0 ? "Live" : "Snapshot",
                    charName.c_str(), name.c_str(),
                    static_cast<unsigned long long>(address));
        if (isLocalPlayer) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[Local Player]");
        }
        ImGui::Separator();

        // 1. Spells Table
        ImGui::Text("Spell Slots:");
        if (ImGui::BeginTable("PlayerSpellsTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Slot");
            ImGui::TableSetupColumn("Spell Name");
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn("CD (Rem/Total)");
            ImGui::TableSetupColumn("Level");
            ImGui::TableSetupColumn("Ammo/Max");
            ImGui::TableSetupColumn("Mana Cost");
            ImGui::TableSetupColumn("Actions");
            ImGui::TableHeadersRow();

            static const std::pair<SDK::SpellSlot, const char*> spellSlots[] = {
                { SDK::SpellSlot::Q, "Q (0)" },
                { SDK::SpellSlot::W, "W (1)" },
                { SDK::SpellSlot::E, "E (2)" },
                { SDK::SpellSlot::R, "R (3)" },
                { SDK::SpellSlot::Summoner1, "Summoner1 (4)" },
                { SDK::SpellSlot::Summoner2, "Summoner2 (5)" }
            };

            for (const auto& [slot, label] : spellSlots) {
                std::string spellName = "Empty";
                CoreSpellBook::State state = CoreSpellBook::State_NoSpell;
                float remainingCD = 0.0f;
                float totalCD = 0.0f;
                int level = 0;
                int ammo = 0;
                int maxAmmo = 0;
                float manaCost = 0.0f;
                bool hasSpell = false;

                if (inspectMode_ == 0) {
                    auto it = std::find_if(liveTarget.spells.begin(), liveTarget.spells.end(), [&](const SpellSlotInfo& s) {
                        return s.slot == slot;
                    });
                    if (it != liveTarget.spells.end() && it->isValid) {
                        spellName = it->name;
                        state = static_cast<CoreSpellBook::State>(it->state);
                        remainingCD = it->remainingCD;
                        totalCD = it->totalCD;
                        level = it->level;
                        ammo = it->ammo;
                        maxAmmo = it->maxAmmo;
                        manaCost = it->manaCost;
                        hasSpell = true;
                    }
                } else {
                    auto it = std::find_if(selectedSnap.spells.begin(), selectedSnap.spells.end(), [&](const DevTools::SnapshotSpell& s) {
                        return s.slot == slot;
                    });
                    if (it != selectedSnap.spells.end()) {
                        spellName = it->name;
                        state = static_cast<CoreSpellBook::State>(it->state);
                        remainingCD = it->remainingCooldown;
                        totalCD = it->cooldown;
                        level = it->level;
                        ammo = it->ammo;
                        maxAmmo = it->maxAmmo;
                        manaCost = it->manaCost;
                        hasSpell = true;
                    }
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(spellName.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(StateToString(state));
                ImGui::TableNextColumn();
                if (remainingCD > 0.01f) {
                    ImGui::Text("%.2f / %.2f", remainingCD, totalCD);
                } else {
                    ImGui::Text("0.00 / %.2f", totalCD);
                }
                ImGui::TableNextColumn();
                ImGui::Text("%d", level);
                ImGui::TableNextColumn();
                ImGui::Text("%d / %d", ammo, maxAmmo);
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", manaCost);
                ImGui::TableNextColumn();

                if (inspectMode_ == 0 && isLocalPlayer && hasSpell && state != CoreSpellBook::State_NoSpell) {
                    char selfBtnId[64];
                    std::snprintf(selfBtnId, sizeof(selfBtnId), "Self##Spell%d", static_cast<int>(slot));
                    if (ImGui::Button(selfBtnId)) {
                        auto local = SDK::ObjectManager::Player();
                        if (local.IsValid()) local.Spellbook().CastSpell(slot);
                    }
                    ImGui::SameLine();
                    char cursorBtnId[64];
                    std::snprintf(cursorBtnId, sizeof(cursorBtnId), "Cursor##Spell%d", static_cast<int>(slot));
                    if (ImGui::Button(cursorBtnId)) {
                        auto local = SDK::ObjectManager::Player();
                        if (local.IsValid()) local.Spellbook().CastSpell(slot, SDK::Game::CursorPos());
                    }
                } else {
                    ImGui::TextUnformatted("-");
                }
            }
            ImGui::EndTable();
        }

        ImGui::Separator();

        // 2. Active Items Table
        ImGui::Text("Inventory Active Items (Slot 6-12):");
        if (ImGui::BeginTable("PlayerItemsTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Slot");
            ImGui::TableSetupColumn("Item Name");
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn("CD (Rem/Total)");
            ImGui::TableSetupColumn("Level");
            ImGui::TableSetupColumn("Ammo/Max");
            ImGui::TableSetupColumn("Mana Cost");
            ImGui::TableSetupColumn("Actions");
            ImGui::TableHeadersRow();

            static const std::pair<SDK::SpellSlot, const char*> itemSlots[] = {
                { SDK::SpellSlot::Item1, "Item 1 (6)" },
                { SDK::SpellSlot::Item2, "Item 2 (7)" },
                { SDK::SpellSlot::Item3, "Item 3 (8)" },
                { SDK::SpellSlot::Item4, "Item 4 (9)" },
                { SDK::SpellSlot::Item5, "Item 5 (10)" },
                { SDK::SpellSlot::Item6, "Item 6 (11)" },
                { SDK::SpellSlot::Trinket, "Trinket (12)" }
            };

            for (const auto& [slot, label] : itemSlots) {
                std::string itemName = "Empty";
                CoreSpellBook::State state = CoreSpellBook::State_NoSpell;
                float remainingCD = 0.0f;
                float totalCD = 0.0f;
                int level = 0;
                int ammo = 0;
                int maxAmmo = 0;
                float manaCost = 0.0f;
                bool hasSpell = false;

                if (inspectMode_ == 0) {
                    auto it = std::find_if(liveTarget.spells.begin(), liveTarget.spells.end(), [&](const SpellSlotInfo& s) {
                        return s.slot == slot;
                    });
                    if (it != liveTarget.spells.end() && it->isValid) {
                        itemName = (it->name.length() > 0) ? it->name : "Empty";
                        state = static_cast<CoreSpellBook::State>(it->state);
                        remainingCD = it->remainingCD;
                        totalCD = it->totalCD;
                        level = it->level;
                        ammo = it->ammo;
                        maxAmmo = it->maxAmmo;
                        manaCost = it->manaCost;
                        hasSpell = true;
                    }
                } else {
                    auto it = std::find_if(selectedSnap.spells.begin(), selectedSnap.spells.end(), [&](const DevTools::SnapshotSpell& s) {
                        return s.slot == slot;
                    });
                    if (it != selectedSnap.spells.end()) {
                        itemName = it->name;
                        state = static_cast<CoreSpellBook::State>(it->state);
                        remainingCD = it->remainingCooldown;
                        totalCD = it->cooldown;
                        level = it->level;
                        ammo = it->ammo;
                        maxAmmo = it->maxAmmo;
                        manaCost = it->manaCost;
                        hasSpell = true;
                    }
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(itemName.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(StateToString(state));
                ImGui::TableNextColumn();
                if (remainingCD > 0.01f) {
                    ImGui::Text("%.2f / %.2f", remainingCD, totalCD);
                } else {
                    ImGui::Text("0.00 / %.2f", totalCD);
                }
                ImGui::TableNextColumn();
                ImGui::Text("%d", level);
                ImGui::TableNextColumn();
                ImGui::Text("%d / %d", ammo, maxAmmo);
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", manaCost);
                ImGui::TableNextColumn();

                if (inspectMode_ == 0 && isLocalPlayer && hasSpell && state != CoreSpellBook::State_NoSpell && itemName != "Empty") {
                    char selfBtnId[64];
                    std::snprintf(selfBtnId, sizeof(selfBtnId), "Self##Item%d", static_cast<int>(slot));
                    if (ImGui::Button(selfBtnId)) {
                        auto local = SDK::ObjectManager::Player();
                        if (local.IsValid()) local.Spellbook().CastSpell(slot);
                    }
                    ImGui::SameLine();
                    char cursorBtnId[64];
                    std::snprintf(cursorBtnId, sizeof(cursorBtnId), "Cursor##Item%d", static_cast<int>(slot));
                    if (ImGui::Button(cursorBtnId)) {
                        auto local = SDK::ObjectManager::Player();
                        if (local.IsValid()) local.Spellbook().CastSpell(slot, SDK::Game::CursorPos());
                    }
                } else {
                    ImGui::TextUnformatted("-");
                }
            }
            ImGui::EndTable();
        }
    }

    void OnCopyHotkey() override {
        std::string dump = "=== DEVELOPER TOOLS SPELLBOOK INSPECTOR ===\n";
        
        if (inspectMode_ == 0) {
            LiveSpellbookTargetInfo liveTarget;
            {
                std::lock_guard<std::mutex> lk(spellbookMutex_);
                liveTarget = cachedSpellbookTarget_;
            }
            if (!liveTarget.isValid || !liveTarget.hasSpellbook) return;
            
            dump += "Target: " + liveTarget.characterName + " (Live) | Address: 0x" + ToHexStr(liveTarget.address) + "\n";

            static const std::pair<SDK::SpellSlot, const char*> slots[] = {
                { SDK::SpellSlot::Q, "Q" },
                { SDK::SpellSlot::W, "W" },
                { SDK::SpellSlot::E, "E" },
                { SDK::SpellSlot::R, "R" },
                { SDK::SpellSlot::Summoner1, "Summoner1" },
                { SDK::SpellSlot::Summoner2, "Summoner2" },
                { SDK::SpellSlot::Item1, "Item1" },
                { SDK::SpellSlot::Item2, "Item2" },
                { SDK::SpellSlot::Item3, "Item3" },
                { SDK::SpellSlot::Item4, "Item4" },
                { SDK::SpellSlot::Item5, "Item5" },
                { SDK::SpellSlot::Item6, "Item6" },
                { SDK::SpellSlot::Trinket, "Trinket" }
            };

            for (const auto& [slot, label] : slots) {
                auto it = std::find_if(liveTarget.spells.begin(), liveTarget.spells.end(), [&](const SpellSlotInfo& s) {
                    return s.slot == slot;
                });
                if (it == liveTarget.spells.end() || !it->isValid) continue;

                char line[256];
                std::snprintf(line, sizeof(line),
                              "Slot: %s | Name: %s | State: %s | CD: %.2f/%.2f | Level: %d | Ammo: %d/%d | Cost: %.0f\n",
                              label, it->name.c_str(), StateToString(static_cast<CoreSpellBook::State>(it->state)),
                              it->remainingCD, it->totalCD,
                              it->level, it->ammo, it->maxAmmo, it->manaCost);
                dump += line;
            }
            NightSharpDebug::Logf("[Dev] Copied spell details of %s (Live) to Clipboard!", liveTarget.characterName.c_str());
        } else {
            if (plugin_->snapshots_.empty()) return;
            static int selectedSnapIdx = 0;
            if (selectedSnapIdx >= static_cast<int>(plugin_->snapshots_.size())) {
                selectedSnapIdx = 0;
            }
            const auto& selectedSnap = plugin_->snapshots_[selectedSnapIdx];
            dump += "Target: " + selectedSnap.characterName + " (Snapshot) | Address: 0x" + ToHexStr(selectedSnap.address) + "\n";
            
            for (const auto& s : selectedSnap.spells) {
                char slotLabel[32];
                std::snprintf(slotLabel, sizeof(slotLabel), "Slot %d", static_cast<int>(s.slot));
                char line[256];
                std::snprintf(line, sizeof(line),
                              "Slot: %s | Name: %s | State: %s | CD: %.2f/%.2f | Level: %d | Ammo: %d/%d | Cost: %.0f\n",
                              slotLabel, s.name, StateToString(static_cast<CoreSpellBook::State>(s.state)),
                              s.remainingCooldown, s.cooldown,
                              s.level, s.ammo, s.maxAmmo, s.manaCost);
                dump += line;
            }
            NightSharpDebug::Logf("[Dev] Copied spell details of %s (Snapshot) to Clipboard!", selectedSnap.characterName.c_str());
        }

        ImGui::SetClipboardText(dump.c_str());
    }

private:
    static const char* StateToString(CoreSpellBook::State state) {
        if (state == CoreSpellBook::State_Ready) return "Ready";
        if (state == CoreSpellBook::State_NotLearned) return "NotLearned";
        if (static_cast<int>(state) & static_cast<int>(CoreSpellBook::State_Cooldown)) return "Cooldown";
        if (static_cast<int>(state) & static_cast<int>(CoreSpellBook::State_NoMana)) return "NoMana";
        if (static_cast<int>(state) & static_cast<int>(CoreSpellBook::State_Suppressed)) return "Suppressed";
        if (static_cast<int>(state) & static_cast<int>(CoreSpellBook::State_Disabled)) return "Disabled";
        if (state == CoreSpellBook::State_NoSpell) return "NoSpell";
        return "Unknown";
    }

    static std::string ToHexStr(uintptr_t val) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%llX", static_cast<unsigned long long>(val));
        return buf;
    }
};

} // namespace Plugins::DevTools
