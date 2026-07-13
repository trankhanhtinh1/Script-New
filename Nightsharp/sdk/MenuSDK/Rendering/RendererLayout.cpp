#include "Renderer.h"

#include <algorithm>
#include <utility>

namespace NightSharp::Menu {

std::vector<Renderer::Path> Renderer::BuildPanelPaths(
    const Path& displayPath) const {
    std::vector<Path> paths;
    paths.reserve(NavigationTransition::MaxPanelCount);
    if (model_.roots.empty()) {
        return paths;
    }

    paths.push_back({});
    for (std::size_t depth = 1;
         depth <= displayPath.size() &&
         paths.size() < NavigationTransition::MaxPanelCount;
         ++depth) {
        Path prefix(displayPath.begin(), displayPath.begin() + depth);
        const MenuNode* parent = model_.Resolve(prefix);
        if (!parent) {
            break;
        }
        const bool hasVisibleChildren = std::any_of(
            parent->children.begin(),
            parent->children.end(),
            [](const MenuNodeHandle& child) {
                return child && child->visible;
            });
        if (!hasVisibleChildren) {
            break;
        }
        paths.push_back(std::move(prefix));
    }
    return paths;
}

Renderer::LayoutMetrics Renderer::MeasureLayout(
    const Path& displayPath) const {
    LayoutMetrics metrics{};
    const std::vector<Path> panelPaths = BuildPanelPaths(displayPath);
    metrics.panelCount = panelPaths.size();
    metrics.hasContent = HasContent(displayPath);
    if (panelPaths.empty()) {
        return metrics;
    }

    metrics.size.x = static_cast<float>(panelPaths.size()) * theme_.panelWidth +
        static_cast<float>(panelPaths.size() - 1) * theme_.panelGap;
    float maximumHeight = 0.0f;
    for (const Path& panelPath : panelPaths) {
        const std::vector<MenuNodeHandle>* nodes = nullptr;
        if (panelPath.empty()) {
            nodes = &model_.roots;
        } else {
            const MenuNode* parent = model_.Resolve(panelPath);
            if (parent) {
                nodes = &parent->children;
            }
        }
        if (!nodes) {
            continue;
        }
        const int count = static_cast<int>(std::count_if(
            nodes->begin(),
            nodes->end(),
            [](const MenuNodeHandle& node) {
                return node && node->visible;
            }));
        maximumHeight = std::max(
            maximumHeight,
            theme_.headerHeight +
                theme_.rowHeight * static_cast<float>(count) +
                4.0f * theme_.dpiScale);
    }

    if (metrics.hasContent) {
        const MenuNode* node = model_.Resolve(displayPath);
        const int itemCount = node
            ? static_cast<int>(std::count_if(
                node->items.begin(),
                node->items.end(),
                [](const MenuItemHandle& item) {
                    return item && item->visible;
                }))
            : 0;
        const float contentHeight = std::max(
            theme_.contentHeight,
            theme_.headerHeight + theme_.padding * 2.0f +
                static_cast<float>(itemCount) *
                    42.0f * theme_.dpiScale);
        metrics.size.x += theme_.panelGap + theme_.contentWidth;
        maximumHeight = std::max(maximumHeight, contentHeight);
    }
    metrics.size.y = maximumHeight;
    return metrics;
}

bool Renderer::HasContent(const Path& path) const {
    const MenuNode* node = model_.Resolve(path);
    return node && std::any_of(
        node->items.begin(),
        node->items.end(),
        [](const MenuItemHandle& item) {
            return item && item->visible;
        });
}

std::string Renderer::BuildBreadcrumb(const Path& path) const {
    std::string breadcrumb;
    Path prefix;
    prefix.reserve(path.size());
    for (int index : path) {
        prefix.push_back(index);
        const MenuNode* node = model_.Resolve(prefix);
        if (!node) {
            break;
        }
        if (!breadcrumb.empty()) {
            breadcrumb += " / ";
        }
        breadcrumb += node->label;
    }
    return breadcrumb;
}

std::string Renderer::Ellipsize(
    const std::string& text,
    float maximumWidth) const {
    if (ImGui::CalcTextSize(text.c_str()).x <= maximumWidth) {
        return text;
    }

    std::string result = text;
    const char* suffix = "...";
    while (!result.empty()) {
        result.pop_back();
        const std::string candidate = result + suffix;
        if (ImGui::CalcTextSize(candidate.c_str()).x <= maximumWidth) {
            return candidate;
        }
    }
    return suffix;
}

void Renderer::ClampOrigin(
    const ImVec2& displaySize,
    const ImVec2& layoutSize) {
    const float margin = theme_.viewportMargin;
    const float requiredWidth = std::max(layoutSize.x, theme_.panelWidth);
    const float requiredHeight = std::max(
        layoutSize.y,
        theme_.headerHeight + theme_.rowHeight);
    const float maximumX = std::max(
        margin,
        displaySize.x - requiredWidth - margin);
    const float maximumY = std::max(
        margin,
        displaySize.y - requiredHeight - margin);
    originTarget_.x = std::clamp(originTarget_.x, margin, maximumX);
    originTarget_.y = std::clamp(originTarget_.y, margin, maximumY);
    origin_.x = std::clamp(origin_.x, margin, maximumX);
    origin_.y = std::clamp(origin_.y, margin, maximumY);
}

bool Renderer::NavigateKeyboard(WPARAM key) {
    if (key != VK_UP && key != VK_DOWN &&
        key != VK_LEFT && key != VK_RIGHT &&
        key != VK_RETURN && key != VK_ESCAPE) {
        return false;
    }

    const auto activateKeyboard = [this]() {
        hoverPath_.clear();
        keyboardNavigation_ = true;
        if (ImGui::GetCurrentContext()) {
            keyboardMouseAnchor_ = ImGui::GetIO().MousePos;
        }
    };
    const auto firstVisible = [](const std::vector<MenuNodeHandle>& nodes) {
        for (int index = 0; index < static_cast<int>(nodes.size()); ++index) {
            if (nodes[index] && nodes[index]->visible && nodes[index]->enabled) {
                return index;
            }
        }
        return -1;
    };

    if (key == VK_ESCAPE || key == VK_LEFT) {
        if (selectedPath_.empty()) {
            return false;
        }
        activateKeyboard();
        selectedPath_.pop_back();
        return true;
    }

    if (key == VK_RIGHT || key == VK_RETURN) {
        activateKeyboard();
        if (selectedPath_.empty()) {
            const int root = firstVisible(model_.roots);
            if (root >= 0) {
                selectedPath_.push_back(root);
            }
            return true;
        }
        const MenuNode* node = model_.Resolve(selectedPath_);
        if (node) {
            const int child = firstVisible(node->children);
            if (child >= 0) {
                selectedPath_.push_back(child);
            }
        }
        return true;
    }

    const std::vector<MenuNodeHandle>* siblings = &model_.roots;
    int current = -1;
    if (!selectedPath_.empty()) {
        current = selectedPath_.back();
        if (selectedPath_.size() > 1) {
            Path parentPath(selectedPath_.begin(), selectedPath_.end() - 1);
            const MenuNode* parent = model_.Resolve(parentPath);
            if (parent) {
                siblings = &parent->children;
            }
        }
    }
    if (!siblings || siblings->empty()) {
        return true;
    }

    const int direction = key == VK_DOWN ? 1 : -1;
    int next = current;
    if (current < 0) {
        next = direction > 0
            ? -1
            : static_cast<int>(siblings->size());
    }
    for (int attempt = 0;
         attempt < static_cast<int>(siblings->size());
         ++attempt) {
        next = (next + direction + static_cast<int>(siblings->size())) %
            static_cast<int>(siblings->size());
        const MenuNodeHandle& node = (*siblings)[next];
        if (node && node->visible && node->enabled) {
            activateKeyboard();
            if (selectedPath_.empty()) {
                selectedPath_.push_back(next);
            } else {
                selectedPath_.back() = next;
            }
            return true;
        }
    }
    return true;
}

}
