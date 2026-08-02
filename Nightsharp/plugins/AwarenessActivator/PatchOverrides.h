#pragma once

#include "AwarenessActivatorCore.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>

namespace NightSharp::Companion {

class PatchOverrideLoader final {
public:
    static std::size_t LoadCsv(PatchRegistry& registry, const char* path) {
        if (!path || !path[0]) return 0;
        std::ifstream input(path);
        if (!input) return 0;

        std::size_t applied = 0;
        std::string line;
        std::array<std::string, 24> fields{};
        while (std::getline(input, line)) {
            const std::size_t count = SplitCsv(line, fields);
            if (count == 0 || fields[0].empty() || fields[0][0] == '#') continue;
            if (Equals(fields[0], "patch") && count >= 2) {
                registry.SetPatchVersion(fields[1]);
                ++applied;
            } else if (Equals(fields[0], "spell") && count >= 14) {
                applied += ApplySpell(registry, fields) ? 1u : 0u;
            } else if (Equals(fields[0], "summoner") && count >= 8) {
                applied += ApplySummoner(registry, fields) ? 1u : 0u;
            } else if (Equals(fields[0], "item") && count >= 10) {
                applied += ApplyItem(registry, fields) ? 1u : 0u;
            } else if (Equals(fields[0], "buff") && count >= 8) {
                applied += ApplyBuff(registry, fields) ? 1u : 0u;
            } else if (Equals(fields[0], "objective") && count >= 8) {
                applied += ApplyObjective(registry, fields) ? 1u : 0u;
            } else if (Equals(fields[0], "quest") && count >= 8) {
                applied += ApplyQuest(registry, fields) ? 1u : 0u;
            }
        }
        return applied;
    }

private:
    static void Trim(std::string& value) {
        const std::size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            value.clear();
            return;
        }
        const std::size_t last = value.find_last_not_of(" \t\r\n");
        value = value.substr(first, last - first + 1);
    }

    static std::size_t SplitCsv(
        const std::string& line,
        std::array<std::string, 24>& fields) {
        for (auto& field : fields) field.clear();
        bool quoted = false;
        std::size_t index = 0;
        for (std::size_t i = 0; i < line.size(); ++i) {
            const char c = line[i];
            if (c == '"') {
                if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                    if (index < fields.size()) fields[index].push_back('"');
                    ++i;
                } else {
                    quoted = !quoted;
                }
                continue;
            }
            if (c == ',' && !quoted) {
                if (index < fields.size()) Trim(fields[index]);
                ++index;
                if (index >= fields.size()) return fields.size();
                continue;
            }
            if (index < fields.size()) fields[index].push_back(c);
        }
        if (index < fields.size()) Trim(fields[index]);
        return std::min(index + 1, fields.size());
    }

    static bool Equals(std::string_view left, std::string_view right) noexcept {
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(left[i])) !=
                std::tolower(static_cast<unsigned char>(right[i]))) return false;
        }
        return true;
    }

    static bool ParseInt(const std::string& text, int& value) noexcept {
        if (text.empty()) return false;
        errno = 0;
        char* end = nullptr;
        const long parsed = std::strtol(text.c_str(), &end, 10);
        if (errno != 0 || end == text.c_str() || *end != '\0' ||
            parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max()) {
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    }

    static bool ParseFloat(const std::string& text, float& value) noexcept {
        if (text.empty()) return false;
        errno = 0;
        char* end = nullptr;
        const float parsed = std::strtof(text.c_str(), &end);
        if (errno != 0 || end == text.c_str() || *end != '\0') return false;
        value = parsed;
        return std::isfinite(value);
    }

    static bool ParseBool(const std::string& text, bool& value) noexcept {
        if (Equals(text, "true") || Equals(text, "yes") || text == "1") {
            value = true;
            return true;
        }
        if (Equals(text, "false") || Equals(text, "no") || text == "0") {
            value = false;
            return true;
        }
        return false;
    }

    static Capability ParseCapability(const std::string& text) noexcept {
        for (std::uint16_t raw = 1;
             raw <= static_cast<std::uint16_t>(Capability::KnightsVow); ++raw) {
            const auto capability = static_cast<Capability>(raw);
            if (Equals(text, CapabilityName(capability))) return capability;
        }
        return Capability::None;
    }

    static bool ParseRulesets(const std::string& text,
                              std::uint32_t& mask) noexcept {
        if (text.empty()) return false;
        mask = 0;
        std::size_t start = 0;
        while (start <= text.size()) {
            const std::size_t end = text.find('|', start);
            const std::string_view token(
                text.data() + start,
                (end == std::string::npos ? text.size() : end) -
                    start);
            if (Equals(token, "standard")) {
                mask |= RulesetMask(RoleQuestRuleset::Standard);
            } else if (Equals(token, "swiftplay")) {
                mask |= RulesetMask(RoleQuestRuleset::Swiftplay);
            } else if (Equals(token, "rotating")) {
                mask |= RulesetMask(RoleQuestRuleset::Rotating);
            } else if (Equals(token, "all")) {
                mask = RulesetMask(RoleQuestRuleset::Standard) |
                       RulesetMask(RoleQuestRuleset::Swiftplay) |
                       RulesetMask(RoleQuestRuleset::Rotating);
            } else {
                return false;
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return mask != 0;
    }

    static CrowdControl ParseControl(const std::string& text) noexcept {
        struct Entry { const char* name; CrowdControl value; };
        static constexpr Entry values[] = {
            { "none", CrowdControl::None }, { "stun", CrowdControl::Stun },
            { "root", CrowdControl::Root }, { "charm", CrowdControl::Charm },
            { "fear", CrowdControl::Fear }, { "taunt", CrowdControl::Taunt },
            { "blind", CrowdControl::Blind }, { "silence", CrowdControl::Silence },
            { "polymorph", CrowdControl::Polymorph }, { "slow", CrowdControl::Slow },
            { "sleep", CrowdControl::Sleep }, { "suppression", CrowdControl::Suppression },
            { "airborne", CrowdControl::Airborne }, { "stasis", CrowdControl::Stasis },
            { "grounded", CrowdControl::Grounded },
        };
        for (const auto& entry : values) if (Equals(text, entry.name)) return entry.value;
        return CrowdControl::None;
    }

    static ObjectiveKind ParseObjectiveKind(const std::string& text) noexcept {
        struct Entry { const char* name; ObjectiveKind value; };
        static constexpr Entry values[] = {
            { "elementaldragon", ObjectiveKind::ElementalDragon },
            { "dragonsoul", ObjectiveKind::DragonSoul },
            { "elderdragon", ObjectiveKind::ElderDragon },
            { "voidgrubs", ObjectiveKind::VoidGrubs },
            { "riftherald", ObjectiveKind::RiftHerald },
            { "baron", ObjectiveKind::Baron },
            { "scuttle", ObjectiveKind::Scuttle },
        };
        for (const auto& entry : values) if (Equals(text, entry.name)) return entry.value;
        return ObjectiveKind::Unknown;
    }

    static bool ApplySpell(PatchRegistry& registry,
                           const std::array<std::string, 24>& field) {
        SpellDefinition value{};
        value.idHash = HashId(field[1]);
        CopyText(value.internalId, field[1]);
        CopyText(value.displayName, field[2]);
        if (!ParseFloat(field[3], value.cooldown) ||
            !ParseFloat(field[4], value.castTime) ||
            !ParseFloat(field[5], value.missileSpeed) ||
            !ParseFloat(field[6], value.range) ||
            !ParseInt(field[7], value.maxCharges) ||
            !ParseBool(field[8], value.startsOnEffectEnd) ||
            !ParseBool(field[9], value.resetOnTakedown) ||
            !ParseBool(field[10], value.canRefund) ||
            !ParseBool(field[11], value.channel) ||
            !ParseBool(field[12], value.charge) ||
            !ParseBool(field[13], value.multiStage)) return false;
        value.maxCharges = std::max(1, value.maxCharges);
        return registry.AddSpell(value);
    }

    static bool ApplySummoner(PatchRegistry& registry,
                              const std::array<std::string, 24>& field) {
        SummonerDefinition value{};
        const std::uint32_t idHash = HashId(field[1]);
        if (const SummonerDefinition* previous =
                registry.FindSummoner(idHash)) {
            value = *previous;
        }
        value.idHash = idHash;
        CopyText(value.internalId, field[1]);
        CopyText(value.displayName, field[3]);
        if (!ParseInt(field[2], value.dataKey) ||
            !ParseFloat(field[4], value.cooldown) ||
            !ParseFloat(field[5], value.range) ||
            !ParseInt(field[6], value.maxCharges)) return false;
        value.capability = ParseCapability(field[7]);
        if (value.capability == Capability::None) return false;
        value.maxCharges = std::max(1, value.maxCharges);
        if (!field[8].empty() &&
            !ParseInt(field[8], value.mapId)) return false;
        if (!field[9].empty() &&
            !ParseRulesets(field[9],
                           value.rulesetsMask)) return false;
        if (!field[10].empty() &&
            !ParseBool(field[10], value.disabled)) return false;
        for (std::size_t i = 0; i < value.effectValues.size();
             ++i) {
            const std::size_t fieldIndex = 11 + i;
            if (!field[fieldIndex].empty() &&
                !ParseFloat(
                    field[fieldIndex],
                    value.effectValues[i])) {
                return false;
            }
        }
        return registry.AddSummoner(value);
    }

    static bool ApplyItem(PatchRegistry& registry,
                          const std::array<std::string, 24>& field) {
        int id = 0;
        if (!ParseInt(field[1], id)) return false;
        ItemDefinition value{};
        if (const ItemDefinition* previous = registry.FindItem(id)) value = *previous;
        value.itemId = SDK::ItemIdFromValue(id);
        CopyText(value.internalId, field[2]);
        CopyText(value.displayName, field[3]);
        if (!ParseFloat(field[4], value.cooldown) ||
            !ParseFloat(field[5], value.duration) ||
            !ParseFloat(field[6], value.range) ||
            !ParseBool(field[7], value.dataActive) ||
            !ParseBool(field[8], value.disabled)) return false;
        value.capability = ParseCapability(field[9]);
        if (value.capability == Capability::None) return false;
        value.inStore = true;
        value.purchasable = true;
        if (!field[10].empty() &&
            !ParseInt(field[10], value.mapId)) return false;
        if (!field[11].empty() &&
            !ParseRulesets(field[11],
                           value.rulesetsMask)) return false;
        for (std::size_t i = 0;
             i < value.effectValues.size(); ++i) {
            const std::size_t fieldIndex = 12 + i;
            if (!field[fieldIndex].empty() &&
                !ParseFloat(
                    field[fieldIndex],
                    value.effectValues[i])) {
                return false;
            }
        }
        return registry.AddItem(value);
    }

    static bool ApplyBuff(PatchRegistry& registry,
                          const std::array<std::string, 24>& field) {
        BuffDefinition value{};
        value.idHash = HashId(field[1]);
        CopyText(value.internalId, field[1]);
        value.control = ParseControl(field[2]);
        if (!ParseBool(field[3], value.cleanseable) ||
            !ParseBool(field[4], value.qssable) ||
            !ParseBool(field[5], value.mikaelable) ||
            !ParseFloat(field[6], value.danger) ||
            !ParseFloat(field[7], value.minimumDuration)) return false;
        return registry.AddBuff(value);
    }

    static bool ApplyObjective(PatchRegistry& registry,
                               const std::array<std::string, 24>& field) {
        ObjectiveDefinition value{};
        value.kind = ParseObjectiveKind(field[1]);
        if (value.kind == ObjectiveKind::Unknown) return false;
        CopyText(value.internalId, field[2]);
        if (!ParseFloat(field[3], value.firstSpawn) ||
            !ParseFloat(field[4], value.respawn) ||
            !ParseBool(field[5], value.map11) ||
            !ParseBool(field[6], value.swiftplay) ||
            !ParseBool(field[7], value.disabled)) return false;
        return registry.AddObjective(value);
    }

    static bool ApplyQuest(PatchRegistry& registry,
                           const std::array<std::string, 24>& field) {
        RoleQuestDefinition value{};
        int itemId = 0;
        if (!ParseInt(field[2], itemId) ||
            !ParseInt(field[3], value.pointsRequired) ||
            !ParseBool(field[4], value.enabledStandard) ||
            !ParseBool(field[5], value.enabledSwiftplay) ||
            !ParseBool(field[6], value.enabledRotating)) return false;
        CopyText(value.role, field[1]);
        value.itemId = SDK::ItemIdFromValue(itemId);
        CopyText(value.reward, field[7]);
        return registry.AddQuest(value);
    }
};

} // namespace NightSharp::Companion
