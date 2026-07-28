#pragma once
#include "IDeveloperTab.h"
#include "../DeveloperToolsPlugin.h"
#include <mutex>
#include <vector>
#include <string>

namespace Plugins::DevTools {

class StatsInspectorTab final : public IDeveloperTab {
private:
    struct LiveStatsTargetInfo {
        bool isValid = false;
        std::string characterName;
        std::string name;
        uintptr_t address = 0;
        bool isLocalPlayer = false;
        bool hasStats = false;
        bool isAIBase = false;

        float health = 0.0f;
        float maxHealth = 0.0f;
        float healthPercent = 0.0f;
        float allShield = 0.0f;
        float physicalShield = 0.0f;
        float magicalShield = 0.0f;
        float healthRegenRate = 0.0f;

        int level = 0;
        float mana = 0.0f;
        float maxMana = 0.0f;
        float manaPercent = 0.0f;
        float moveSpeed = 0.0f;
        float attackRange = 0.0f;
        float baseAD = 0.0f;
        float bonusAD = 0.0f;
        float totalAD = 0.0f;
        float ap = 0.0f;
        float attackSpeedMod = 0.0f;
        float armor = 0.0f;
        float bonusArmor = 0.0f;
        float spellBlock = 0.0f;
        float bonusSpellBlock = 0.0f;
        float crit = 0.0f;
        float lethality = 0.0f;
        float flatArmorPen = 0.0f;
        float percentArmorPen = 0.0f;
        float flatMagicPen = 0.0f;
        float percentMagicPen = 0.0f;
    };

    int inspectMode_ = 0; // 0 = Live, 1 = Snapshot
    mutable std::mutex statsMutex_;
    LiveStatsTargetInfo cachedStatsTarget_;
    int lastUpdateTick_ = 0;

public:
    using IDeveloperTab::IDeveloperTab;

    const char* GetTabName() const override { return "Stats"; }

    void OnUpdate() override {
        if (!plugin_->enabled_) {
            std::lock_guard<std::mutex> lk(statsMutex_);
            cachedStatsTarget_ = {};
            return;
        }

        if (inspectMode_ == 1) {
            std::lock_guard<std::mutex> lk(statsMutex_);
            cachedStatsTarget_ = {};
            return;
        }

        const int now = SDK::Variables::TickCount();
        if (now - lastUpdateTick_ < 100) {
            return;
        }
        lastUpdateTick_ = now;

        const auto obj = plugin_->GetFocusedObject();
        LiveStatsTargetInfo targetInfo;

        if (obj.IsValid()) {
            targetInfo.isValid = true;
            targetInfo.hasStats = obj.IsHero() || obj.IsMinion() || obj.IsTurret() || 
                                 obj.Type() == ::Core::Objects::ObjectType::BarracksDampenerClient ||
                                 obj.Type() == ::Core::Objects::ObjectType::HQClient;
            
            targetInfo.characterName = obj.CharacterName();
            targetInfo.name = obj.Name();
            targetInfo.address = obj.Address();
            const auto localPlayer = SDK::ObjectManager::Player();
            targetInfo.isLocalPlayer = localPlayer.IsValid() && obj.Address() == localPlayer.Address();

            if (targetInfo.hasStats) {
                const auto player = SDK::AIBaseClient(obj.Handle());
                const auto att = SDK::AttackableUnit(obj.Handle());
                
                targetInfo.health = att.Health();
                targetInfo.maxHealth = att.MaxHealth();
                targetInfo.healthPercent = att.HealthPercent();
                targetInfo.allShield = att.AllShield();
                targetInfo.physicalShield = att.PhysicalShield();
                targetInfo.magicalShield = att.MagicalShield();
                targetInfo.healthRegenRate = att.HealthRegenRate();

                targetInfo.isAIBase = obj.IsHero() || obj.IsMinion() || obj.IsTurret();
                if (targetInfo.isAIBase && player.IsValid()) {
                    targetInfo.level = player.Level();
                    targetInfo.mana = player.Mana();
                    targetInfo.maxMana = player.MaxMana();
                    targetInfo.manaPercent = player.ManaPercent();
                    targetInfo.moveSpeed = player.MoveSpeed();
                    targetInfo.attackRange = player.AttackRange();
                    targetInfo.baseAD = player.BaseAttackDamage();
                    targetInfo.bonusAD = player.BonusAttackDamage();
                    targetInfo.totalAD = player.TotalAttackDamage();
                    targetInfo.ap = player.TotalMagicalDamage();
                    targetInfo.attackSpeedMod = player.AttackSpeedMod();
                    targetInfo.armor = player.Armor();
                    targetInfo.bonusArmor = player.BonusArmor();
                    targetInfo.spellBlock = player.SpellBlock();
                    targetInfo.bonusSpellBlock = player.BonusSpellBlock();
                    targetInfo.crit = player.Crit();
                    targetInfo.lethality = player.Lethality();
                    targetInfo.flatArmorPen = player.FlatArmorPenetrationMod();
                    targetInfo.percentArmorPen = player.PercentArmorPenetrationMod();
                    targetInfo.flatMagicPen = player.FlatMagicPenetrationMod();
                    targetInfo.percentMagicPen = player.PercentMagicPenetrationMod();
                }
            }
        }

        std::lock_guard<std::mutex> lk(statsMutex_);
        cachedStatsTarget_ = std::move(targetInfo);
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
        bool hasStats = false;

        DevTools::ObjectSnapshot selectedSnap;
        LiveStatsTargetInfo liveTarget;

        if (inspectMode_ == 0) {
            {
                std::lock_guard<std::mutex> lk(statsMutex_);
                liveTarget = cachedStatsTarget_;
            }
            if (liveTarget.isValid) {
                charName = liveTarget.characterName;
                name = liveTarget.name;
                address = liveTarget.address;
                isLocalPlayer = liveTarget.isLocalPlayer;
                hasStats = liveTarget.hasStats;
                hasTarget = true;

                if (!hasStats) {
                    ImGui::Text("Inspecting Live: %s (%s) | Addr: 0x%llX", charName.c_str(), name.c_str(), static_cast<unsigned long long>(address));
                    ImGui::Text("This object type has no stats.");
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
                drawStatRow("Health", liveTarget.health);
                drawStatRow("Max Health", liveTarget.maxHealth);
                drawStatRow("Health %", liveTarget.healthPercent, "%.1f%%");
                drawStatRow("All Shield", liveTarget.allShield);
                drawStatRow("Physical Shield", liveTarget.physicalShield);
                drawStatRow("Magical Shield", liveTarget.magicalShield);
                drawStatRow("Health Regen Rate", liveTarget.healthRegenRate);

                if (liveTarget.isAIBase) {
                    drawStatRowInt("Level", liveTarget.level);
                    drawStatRow("Mana", liveTarget.mana);
                    drawStatRow("Max Mana", liveTarget.maxMana);
                    drawStatRow("Mana %", liveTarget.manaPercent, "%.1f%%");
                    drawStatRow("Move Speed", liveTarget.moveSpeed);
                    drawStatRow("Attack Range", liveTarget.attackRange);
                    drawStatRow("Base AD", liveTarget.baseAD);
                    drawStatRow("Bonus AD", liveTarget.bonusAD);
                    drawStatRow("Total AD", liveTarget.totalAD);
                    drawStatRow("Ability Power (AP)", liveTarget.ap);
                    drawStatRow("Attack Speed Mod", liveTarget.attackSpeedMod);
                    drawStatRow("Armor", liveTarget.armor);
                    drawStatRow("Bonus Armor", liveTarget.bonusArmor);
                    drawStatRow("Spell Block (MR)", liveTarget.spellBlock);
                    drawStatRow("Bonus Spell Block", liveTarget.bonusSpellBlock);
                    drawStatRow("Critical Chance", liveTarget.crit, "%.1f%%");
                    drawStatRow("Lethality", liveTarget.lethality);
                    drawStatRow("Flat Armor Pen", liveTarget.flatArmorPen);
                    drawStatRow("Percent Armor Pen Mod", liveTarget.percentArmorPen, "%.3f");
                    drawStatRow("Flat Magic Pen", liveTarget.flatMagicPen);
                    drawStatRow("Percent Magic Pen Mod", liveTarget.percentMagicPen, "%.3f");
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
            LiveStatsTargetInfo liveTarget;
            {
                std::lock_guard<std::mutex> lk(statsMutex_);
                liveTarget = cachedStatsTarget_;
            }
            if (!liveTarget.isValid || !liveTarget.hasStats) return;
            
            dump += "Target: " + liveTarget.characterName + " (Live) | Address: 0x" + ToHexStr(liveTarget.address) + "\n";
            
            char line[256];
            std::snprintf(line, sizeof(line), "Health: %.2f / %.2f | Regen: %.2f\n", liveTarget.health, liveTarget.maxHealth, liveTarget.healthRegenRate);
            dump += line;
            std::snprintf(line, sizeof(line), "Shields: All=%.2f Phys=%.2f Mag=%.2f\n", liveTarget.allShield, liveTarget.physicalShield, liveTarget.magicalShield);
            dump += line;

            if (liveTarget.isAIBase) {
                std::snprintf(line, sizeof(line), "Level: %d | Mana: %.2f / %.2f\n", liveTarget.level, liveTarget.mana, liveTarget.maxMana);
                dump += line;
                std::snprintf(line, sizeof(line), "AD: Total=%.2f Base=%.2f Bonus=%.2f | AP: %.2f\n", liveTarget.totalAD, liveTarget.baseAD, liveTarget.bonusAD, liveTarget.ap);
                dump += line;
                std::snprintf(line, sizeof(line), "Armor: %.2f (Bonus: %.2f) | MR: %.2f (Bonus: %.2f)\n", liveTarget.armor, liveTarget.bonusArmor, liveTarget.spellBlock, liveTarget.bonusSpellBlock);
                dump += line;
                std::snprintf(line, sizeof(line), "MoveSpeed: %.2f | Range: %.2f | AS Mod: %.2f | Crit: %.2f%%\n", liveTarget.moveSpeed, liveTarget.attackRange, liveTarget.attackSpeedMod, liveTarget.crit);
                dump += line;
                std::snprintf(line, sizeof(line), "Pen: Lethality=%.2f FlatArmorPen=%.2f %%ArmorPen=%.2f | FlatMagicPen=%.2f %%MagicPen=%.2f\n",
                              liveTarget.lethality, liveTarget.flatArmorPen, liveTarget.percentArmorPen, liveTarget.flatMagicPen, liveTarget.percentMagicPen);
                dump += line;
            }
            NightSharpDebug::Logf("[Dev] Copied stats of %s (Live) to Clipboard!", liveTarget.characterName.c_str());
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
