#pragma once
#include "IDeveloperTab.h"
#include "../DeveloperToolsPlugin.h"

namespace Plugins::DevTools {

class StatsInspectorTab final : public IDeveloperTab {
private:
    int inspectMode_ = 0; // 0 = Live, 1 = Snapshot

public:
    using IDeveloperTab::IDeveloperTab;

    const char* GetTabName() const override { return "Stats"; }

    void OnDrawTab() override {
        ImGui::RadioButton("Live Object", &inspectMode_, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Snapshot Object", &inspectMode_, 1);

        SDK::AIBaseClient player;
        std::string charName;
        std::string name;
        uintptr_t address = 0;
        bool isLocalPlayer = false;
        bool hasTarget = false;

        DevTools::ObjectSnapshot selectedSnap;

        if (inspectMode_ == 0) {
            const auto obj = plugin_->GetFocusedObject();
            if (obj.IsValid()) {
                const bool hasStats = obj.IsHero() || obj.IsMinion() || obj.IsTurret() || 
                                     obj.Type() == ::Core::Objects::ObjectType::BarracksDampenerClient ||
                                     obj.Type() == ::Core::Objects::ObjectType::HQClient;
                if (!hasStats) {
                    ImGui::Text("Inspecting Live: %s (%s) | Addr: 0x%llX",
                                obj.CharacterName().c_str(), obj.Name().c_str(),
                                static_cast<unsigned long long>(obj.Address()));
                    ImGui::Text("This object type has no stats.");
                    return;
                }
                player = SDK::AIBaseClient(obj.Handle());
                charName = player.CharacterName();
                name = player.Name();
                address = player.Address();
                const auto localPlayer = SDK::ObjectManager::Player();
                isLocalPlayer = localPlayer.IsValid() && player.Address() == localPlayer.Address();
                hasTarget = true;
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

        ImGui::Text("Inspecting %s Stats: %s (%s) | Addr: 0x%llX",
                    inspectMode_ == 0 ? "Live" : "Snapshot",
                    charName.c_str(), name.c_str(),
                    static_cast<unsigned long long>(address));
        if (isLocalPlayer) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[Local Player]");
        }
        ImGui::Separator();

        if (ImGui::BeginTable("ObjectStatsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Statistic");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            auto drawStatRow = [](const char* name, float value, const char* format = "%.2f") {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name);
                ImGui::TableNextColumn();
                ImGui::Text(format, value);
            };

            auto drawStatRowInt = [](const char* name, int value) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name);
                ImGui::TableNextColumn();
                ImGui::Text("%d", value);
            };

            if (inspectMode_ == 0) {
                const auto att = SDK::AttackableUnit(player.Handle());
                drawStatRow("Health", att.Health());
                drawStatRow("Max Health", att.MaxHealth());
                drawStatRow("Health %", att.HealthPercent(), "%.1f%%");
                drawStatRow("All Shield", att.AllShield());
                drawStatRow("Physical Shield", att.PhysicalShield());
                drawStatRow("Magical Shield", att.MagicalShield());
                drawStatRow("Health Regen Rate", att.HealthRegenRate());

                const bool isAIBase = player.IsHero() || player.IsMinion() || player.IsTurret();
                if (isAIBase) {
                    drawStatRowInt("Level", player.Level());
                    drawStatRow("Mana", player.Mana());
                    drawStatRow("Max Mana", player.MaxMana());
                    drawStatRow("Mana %", player.ManaPercent(), "%.1f%%");
                    drawStatRow("Move Speed", player.MoveSpeed());
                    drawStatRow("Attack Range", player.AttackRange());
                    drawStatRow("Base AD", player.BaseAttackDamage());
                    drawStatRow("Bonus AD", player.BonusAttackDamage());
                    drawStatRow("Total AD", player.TotalAttackDamage());
                    drawStatRow("Ability Power (AP)", player.TotalMagicalDamage());
                    drawStatRow("Attack Speed Mod", player.AttackSpeedMod());
                    drawStatRow("Armor", player.Armor());
                    drawStatRow("Bonus Armor", player.BonusArmor());
                    drawStatRow("Spell Block (MR)", player.SpellBlock());
                    drawStatRow("Bonus Spell Block", player.BonusSpellBlock());
                    drawStatRow("Critical Chance", player.Crit(), "%.1f%%");
                    drawStatRow("Lethality", player.Lethality());
                    drawStatRow("Flat Armor Pen", player.FlatArmorPenetrationMod());
                    drawStatRow("Percent Armor Pen Mod", player.PercentArmorPenetrationMod(), "%.3f");
                    drawStatRow("Flat Magic Pen", player.FlatMagicPenetrationMod());
                    drawStatRow("Percent Magic Pen Mod", player.PercentMagicPenetrationMod(), "%.3f");
                }
            } else {
                drawStatRow("Health", selectedSnap.health);
                drawStatRow("Max Health", selectedSnap.maxHealth);
                float hpPercent = selectedSnap.maxHealth > 0.0f ? (selectedSnap.health * 100.0f / selectedSnap.maxHealth) : 0.0f;
                drawStatRow("Health %", hpPercent, "%.1f%%");
                drawStatRow("All Shield", selectedSnap.allShield);
                drawStatRow("Physical Shield", selectedSnap.physShield);
                drawStatRow("Magical Shield", selectedSnap.magShield);
                drawStatRow("Health Regen Rate", selectedSnap.healthRegen);

                const bool isAIBase = selectedSnap.type == ::Core::Objects::ObjectType::AIHeroClient ||
                                     selectedSnap.type == ::Core::Objects::ObjectType::AIMinionClient ||
                                     selectedSnap.type == ::Core::Objects::ObjectType::AITurretClient;
                if (isAIBase) {
                    drawStatRowInt("Level", selectedSnap.level);
                    drawStatRow("Mana", selectedSnap.mana);
                    drawStatRow("Max Mana", selectedSnap.maxMana);
                    float mpPercent = selectedSnap.maxMana > 0.0f ? (selectedSnap.mana * 100.0f / selectedSnap.maxMana) : 0.0f;
                    drawStatRow("Mana %", mpPercent, "%.1f%%");
                    drawStatRow("Move Speed", selectedSnap.moveSpeed);
                    drawStatRow("Attack Range", selectedSnap.attackRange);
                    drawStatRow("Base AD", selectedSnap.baseAD);
                    drawStatRow("Bonus AD", selectedSnap.bonusAD);
                    drawStatRow("Total AD", selectedSnap.attackDamage);
                    drawStatRow("Ability Power (AP)", selectedSnap.abilityPower);
                    drawStatRow("Attack Speed Mod", selectedSnap.attackSpeedMod);
                    drawStatRow("Armor", selectedSnap.armor);
                    drawStatRow("Bonus Armor", selectedSnap.bonusArmor);
                    drawStatRow("Spell Block (MR)", selectedSnap.spellBlock);
                    drawStatRow("Bonus Spell Block", selectedSnap.bonusSpellBlock);
                    drawStatRow("Critical Chance", selectedSnap.crit, "%.1f%%");
                    drawStatRow("Lethality", selectedSnap.lethality);
                    drawStatRow("Flat Armor Pen", selectedSnap.flatArmorPen);
                    drawStatRow("Percent Armor Pen Mod", selectedSnap.percentArmorPen, "%.3f");
                    drawStatRow("Flat Magic Pen", selectedSnap.flatMagicPen);
                    drawStatRow("Percent Magic Pen Mod", selectedSnap.percentMagicPen, "%.3f");
                }
            }

            ImGui::EndTable();
        }
    }

    void OnCopyHotkey() override {
        std::string dump = "=== DEVELOPER TOOLS OBJECT STATS ===\n";
        
        if (inspectMode_ == 0) {
            const auto obj = plugin_->GetFocusedObject();
            if (!obj.IsValid()) return;
            
            const auto player = SDK::AIBaseClient(obj.Handle());
            const auto att = SDK::AttackableUnit(player.Handle());
            dump += "Target: " + player.CharacterName() + " (Live) | Address: 0x" + ToHexStr(player.Address()) + "\n";
            
            char line[256];
            std::snprintf(line, sizeof(line), "Health: %.2f / %.2f | Regen: %.2f\n", att.Health(), att.MaxHealth(), att.HealthRegenRate());
            dump += line;
            std::snprintf(line, sizeof(line), "Shields: All=%.2f Phys=%.2f Mag=%.2f\n", att.AllShield(), att.PhysicalShield(), att.MagicalShield());
            dump += line;

            const bool isAIBase = player.IsHero() || player.IsMinion() || player.IsTurret();
            if (isAIBase) {
                std::snprintf(line, sizeof(line), "Level: %d | Mana: %.2f / %.2f\n", player.Level(), player.Mana(), player.MaxMana());
                dump += line;
                std::snprintf(line, sizeof(line), "AD: Total=%.2f Base=%.2f Bonus=%.2f | AP: %.2f\n", player.TotalAttackDamage(), player.BaseAttackDamage(), player.BonusAttackDamage(), player.TotalMagicalDamage());
                dump += line;
                std::snprintf(line, sizeof(line), "Armor: %.2f (Bonus: %.2f) | MR: %.2f (Bonus: %.2f)\n", player.Armor(), player.BonusArmor(), player.SpellBlock(), player.BonusSpellBlock());
                dump += line;
                std::snprintf(line, sizeof(line), "MoveSpeed: %.2f | Range: %.2f | AS Mod: %.2f | Crit: %.2f%%\n", player.MoveSpeed(), player.AttackRange(), player.AttackSpeedMod(), player.Crit());
                dump += line;
                std::snprintf(line, sizeof(line), "Pen: Lethality=%.2f FlatArmorPen=%.2f %%ArmorPen=%.2f | FlatMagicPen=%.2f %%MagicPen=%.2f\n",
                              player.Lethality(), player.FlatArmorPenetrationMod(), player.PercentArmorPenetrationMod(), player.FlatMagicPenetrationMod(), player.PercentMagicPenetrationMod());
                dump += line;
            }
            NightSharpDebug::Logf("[Dev] Copied stats of %s (Live) to Clipboard!", player.CharacterName().c_str());
        } else {
            if (plugin_->snapshots_.empty()) return;
            static int selectedSnapIdx = 0;
            if (selectedSnapIdx >= static_cast<int>(plugin_->snapshots_.size())) {
                selectedSnapIdx = 0;
            }
            const auto& selectedSnap = plugin_->snapshots_[selectedSnapIdx];
            dump += "Target: " + selectedSnap.characterName + " (Snapshot) | Address: 0x" + ToHexStr(selectedSnap.address) + "\n";
            
            char line[256];
            std::snprintf(line, sizeof(line), "Health: %.2f / %.2f | Regen: %.2f\n", selectedSnap.health, selectedSnap.maxHealth, selectedSnap.healthRegen);
            dump += line;
            std::snprintf(line, sizeof(line), "Shields: All=%.2f Phys=%.2f Mag=%.2f\n", selectedSnap.allShield, selectedSnap.physShield, selectedSnap.magShield);
            dump += line;

            const bool isAIBase = selectedSnap.type == ::Core::Objects::ObjectType::AIHeroClient ||
                                 selectedSnap.type == ::Core::Objects::ObjectType::AIMinionClient ||
                                 selectedSnap.type == ::Core::Objects::ObjectType::AITurretClient;
            if (isAIBase) {
                std::snprintf(line, sizeof(line), "Level: %d | Mana: %.2f / %.2f\n", selectedSnap.level, selectedSnap.mana, selectedSnap.maxMana);
                dump += line;
                std::snprintf(line, sizeof(line), "AD: Total=%.2f Base=%.2f Bonus=%.2f | AP: %.2f\n", selectedSnap.attackDamage, selectedSnap.baseAD, selectedSnap.bonusAD, selectedSnap.abilityPower);
                dump += line;
                std::snprintf(line, sizeof(line), "Armor: %.2f (Bonus: %.2f) | MR: %.2f (Bonus: %.2f)\n", selectedSnap.armor, selectedSnap.bonusArmor, selectedSnap.spellBlock, selectedSnap.bonusSpellBlock);
                dump += line;
                std::snprintf(line, sizeof(line), "MoveSpeed: %.2f | Range: %.2f | AS Mod: %.2f | Crit: %.2f%%\n", selectedSnap.moveSpeed, selectedSnap.attackRange, selectedSnap.attackSpeedMod, selectedSnap.crit);
                dump += line;
                std::snprintf(line, sizeof(line), "Pen: Lethality=%.2f FlatArmorPen=%.2f %%ArmorPen=%.2f | FlatMagicPen=%.2f %%MagicPen=%.2f\n",
                              selectedSnap.lethality, selectedSnap.flatArmorPen, selectedSnap.percentArmorPen, selectedSnap.flatMagicPen, selectedSnap.percentMagicPen);
                dump += line;
            }
            NightSharpDebug::Logf("[Dev] Copied stats of %s (Snapshot) to Clipboard!", selectedSnap.characterName.c_str());
        }

        ImGui::SetClipboardText(dump.c_str());
    }

private:
    static std::string ToHexStr(uintptr_t val) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%llX", static_cast<unsigned long long>(val));
        return buf;
    }
};

} // namespace Plugins::DevTools
