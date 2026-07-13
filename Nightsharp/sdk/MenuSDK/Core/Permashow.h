#pragma once

#include "imgui.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace NightSharp::Menu {

enum class PermashowValueKind {
    Text,
    Toggle,
    Progress,
    Custom,
};

enum class PermashowCorner {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

struct PermashowDrawContext {
    ImDrawList* drawList = nullptr;
    ImVec2 position{};
    ImVec2 size{};
    float alpha = 1.0f;
    float dpiScale = 1.0f;
};

class PermashowEntry;
using PermashowEntryHandle = std::shared_ptr<PermashowEntry>;
using PermashowValueProvider = std::function<std::string(const PermashowEntry&)>;
using PermashowAction = std::function<void(PermashowEntry&)>;
using PermashowCustomRenderer = std::function<void(const PermashowDrawContext&)>;

class PermashowEntry {
public:
    PermashowEntry(
        std::string entryId,
        std::string entryLabel,
        PermashowValueKind entryKind)
        : id(std::move(entryId)),
          label(std::move(entryLabel)),
          kind(entryKind) {}

    std::string ValueText() const {
        if (valueProvider) {
            return valueProvider(*this);
        }
        if (!valueText.empty()) {
            return valueText;
        }
        if (kind == PermashowValueKind::Toggle) {
            return toggle ? "[ON]" : "[OFF]";
        }
        if (kind == PermashowValueKind::Progress) {
            return std::to_string(static_cast<int>(progress * 100.0f)) + "%";
        }
        return {};
    }

    void SetText(std::string text) {
        valueText = std::move(text);
    }

    void SetToggle(bool next) {
        toggle = next;
    }

    void SetProgress(float next) {
        progress = std::clamp(next, 0.0f, 1.0f);
    }

    std::string id;
    std::string label;
    std::string group;
    PermashowValueKind kind = PermashowValueKind::Text;
    ImVec4 valueColor = ImVec4(0.400f, 0.820f, 0.470f, 1.0f);
    bool visible = true;
    bool enabled = true;
    bool interactive = false;
    bool toggle = false;
    float progress = 0.0f;
    int order = 0;
    std::string valueText;
    PermashowValueProvider valueProvider;
    PermashowAction onClick;
    PermashowCustomRenderer customRenderer;
};

struct PermashowLayout {
    PermashowCorner corner = PermashowCorner::TopRight;
    ImVec2 offset = ImVec2(20.0f, 20.0f);
    float width = 260.0f;
    float padding = 13.0f;
    float rowHeight = 31.0f;
    float groupHeight = 23.0f;
    float gap = 8.0f;
    bool showHeader = false;
    std::string header = "Permashow";
};

class PermashowRegistry {
public:
    PermashowEntryHandle Add(
        std::string id,
        std::string label,
        PermashowValueKind kind) {
        if (PermashowEntryHandle existing = Find(id)) {
            return existing;
        }
        PermashowEntryHandle entry = std::make_shared<PermashowEntry>(
            std::move(id),
            std::move(label),
            kind);
        entry->order = static_cast<int>(entries_.size());
        entries_.push_back(entry);
        return entry;
    }

    PermashowEntryHandle AddText(
        std::string id,
        std::string label,
        std::string text,
        ImVec4 color = ImVec4(0.400f, 0.820f, 0.470f, 1.0f)) {
        PermashowEntryHandle entry = Add(
            std::move(id),
            std::move(label),
            PermashowValueKind::Text);
        entry->valueText = std::move(text);
        entry->valueColor = color;
        return entry;
    }

    PermashowEntryHandle AddToggle(
        std::string id,
        std::string label,
        bool value = false) {
        PermashowEntryHandle entry = Add(
            std::move(id),
            std::move(label),
            PermashowValueKind::Toggle);
        entry->toggle = value;
        return entry;
    }

    PermashowEntryHandle AddProgress(
        std::string id,
        std::string label,
        float value = 0.0f) {
        PermashowEntryHandle entry = Add(
            std::move(id),
            std::move(label),
            PermashowValueKind::Progress);
        entry->progress = std::clamp(value, 0.0f, 1.0f);
        return entry;
    }

    PermashowEntryHandle AddCustom(
        std::string id,
        std::string label,
        PermashowCustomRenderer renderer) {
        PermashowEntryHandle entry = Add(
            std::move(id),
            std::move(label),
            PermashowValueKind::Custom);
        entry->customRenderer = std::move(renderer);
        return entry;
    }

    PermashowEntryHandle Find(const std::string& id) const {
        const auto found = std::find_if(
            entries_.begin(),
            entries_.end(),
            [&id](const PermashowEntryHandle& entry) {
                return entry && entry->id == id;
            });
        return found == entries_.end() ? nullptr : *found;
    }

    bool Remove(const std::string& id) {
        const auto before = entries_.size();
        std::erase_if(entries_, [&id](const PermashowEntryHandle& entry) {
            return entry && entry->id == id;
        });
        return entries_.size() != before;
    }

    void Clear() {
        entries_.clear();
    }

    const std::vector<PermashowEntryHandle>& Entries() const {
        return entries_;
    }

    PermashowLayout& Layout() {
        return layout_;
    }

    const PermashowLayout& Layout() const {
        return layout_;
    }

    bool visible = true;

private:
    std::vector<PermashowEntryHandle> entries_;
    PermashowLayout layout_;
};

class PermashowBuilder {
public:
    PermashowBuilder() = default;
    PermashowBuilder(PermashowRegistry& registry, std::string group)
        : registry_(&registry),
          group_(std::move(group)) {}

    PermashowEntryHandle Text(
        std::string id,
        std::string label,
        std::string value) const {
        return Configure(registry_->AddText(
            std::move(id),
            std::move(label),
            std::move(value)));
    }

    PermashowEntryHandle Toggle(
        std::string id,
        std::string label,
        bool value = false) const {
        return Configure(registry_->AddToggle(
            std::move(id),
            std::move(label),
            value));
    }

    PermashowEntryHandle Progress(
        std::string id,
        std::string label,
        float value = 0.0f) const {
        return Configure(registry_->AddProgress(
            std::move(id),
            std::move(label),
            value));
    }

    PermashowEntryHandle Custom(
        std::string id,
        std::string label,
        PermashowCustomRenderer renderer) const {
        return Configure(registry_->AddCustom(
            std::move(id),
            std::move(label),
            std::move(renderer)));
    }

private:
    PermashowEntryHandle Configure(PermashowEntryHandle entry) const {
        if (entry) {
            entry->group = group_;
        }
        return entry;
    }

    PermashowRegistry* registry_ = nullptr;
    std::string group_;
};

inline PermashowBuilder PermashowGroup(
    PermashowRegistry& registry,
    std::string group) {
    return PermashowBuilder(registry, std::move(group));
}

}
