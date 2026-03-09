#pragma once
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "menu/MenuConfig.h"
#include <string>
#include <vector>
#include <functional>
#include <array>
#include <map>

// ============================================================================
// CatStyleMenu - Port of cat-master Lua menu design to ImGui C++
// ============================================================================

namespace CatMenu {

    // ============================================================================
    // Color Constants (ported from cat-master COLORS)
    // ============================================================================
    namespace Colors {
        inline ImU32 bg_main = IM_COL32(16, 18, 22, 224);       // 0xE0101216
        inline ImU32 bg_title = IM_COL32(30, 35, 40, 224);     // 0xE01E2328
        inline ImU32 bg_hover = IM_COL32(0, 122, 204, 128);    // 0x80007ACC
        inline ImU32 bg_hover_light = IM_COL32(48, 48, 48, 128); // 0x80303030
        inline ImU32 border = IM_COL32(60, 64, 67, 255);        // 0xFF3C4043
        inline ImU32 text_primary = IM_COL32(232, 234, 237, 255); // 0xFFE8EAED
        inline ImU32 text_secondary = IM_COL32(154, 160, 166, 255); // 0xFF9AA0A6
        inline ImU32 text_header = IM_COL32(94, 158, 214, 255); // 0xFF5E9ED6
        inline ImU32 checkbox_on = IM_COL32(76, 175, 80, 255);  // 0xFF4CAF50
        inline ImU32 checkbox_off = IM_COL32(102, 102, 102, 255); // 0xFF666666
        inline ImU32 slider_bar = IM_COL32(68, 68, 68, 255);    // 0xFF444444
        inline ImU32 slider_fill = IM_COL32(0, 122, 204, 255);  // 0xFF007ACC
        inline ImU32 slider_handle = IM_COL32(255, 255, 255, 255); // 0xFFFFFFFF
    }

    // ============================================================================
    // Menu Item Types
    // ============================================================================
    enum class ItemType {
        Header,
        Boolean,
        Slider,
        Color,
        Dropdown,
        Submenu
    };

    // ============================================================================
    // Base Menu Item
    // ============================================================================
    struct MenuItem {
        std::string id;
        std::string label;
        ItemType type;
        bool enabled = true;
        bool visible = true;

        MenuItem(const std::string& i, const std::string& l, ItemType t) 
            : id(i), label(l), type(t) {}
        virtual ~MenuItem() = default;
    };

    // ============================================================================
    // Boolean Item
    // ============================================================================
    struct BooleanItem : MenuItem {
        bool value;
        bool defaultValue;
        std::function<void(bool)> callback;

        BooleanItem(const std::string& i, const std::string& l, bool v, std::function<void(bool)> cb = nullptr)
            : MenuItem(i, l, ItemType::Boolean), value(v), defaultValue(v), callback(std::move(cb)) {}

        bool get() const { return value; }
        void set(bool v) { 
            value = v; 
            if (callback) callback(value); 
        }
    };

    // ============================================================================
    // Slider Item
    // ============================================================================
    struct SliderItem : MenuItem {
        float value;
        float minValue;
        float maxValue;
        float step;
        float defaultValue;
        std::string format;
        std::function<void(float)> callback;
        bool expanded = false;

        SliderItem(const std::string& i, const std::string& l, float v, float min, float max, float s = 1.0f, const char* fmt = "%.0f", std::function<void(float)> cb = nullptr)
            : MenuItem(i, l, ItemType::Slider), value(v), minValue(min), maxValue(max), step(s), defaultValue(v), format(fmt), callback(std::move(cb)) {}

        float get() const { return value; }
        void set(float v) { 
            value = std::max(minValue, std::min(maxValue, v));
            if (callback) callback(value);
        }
    };

    // ============================================================================
    // Color Item (HSV based like cat-master)
    // ============================================================================
    struct ColorItem : MenuItem {
        float h, s, v, a;
        float defaultH, defaultS, defaultV, defaultA;
        std::function<void(float, float, float, float)> callback;
        bool showPicker = false;

        ColorItem(const std::string& i, const std::string& l, float r, float g, float b, float alpha = 255.0f, std::function<void(float, float, float, float)> cb = nullptr)
            : MenuItem(i, l, ItemType::Color), a(alpha), defaultA(alpha), callback(std::move(cb)) {
            // Convert RGB to HSV
            RGBtoHSV(r / 255.0f, g / 255.0f, b / 255.0f, h, s, v);
            defaultH = h; defaultS = s; defaultV = v;
        }

        ImU32 getColor() const {
            float r, g, b;
            HSVtoRGB(h, s, v, r, g, b);
            return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)a);
        }

        void getRGB(float& r, float& g, float& b) const {
            HSVtoRGB(h, s, v, r, g, b);
        }

        void set(float r, float g, float b, float alpha = 255.0f) {
            a = alpha;
            RGBtoHSV(r / 255.0f, g / 255.0f, b / 255.0f, h, s, v);
            if (callback) callback(h, s, v, a);
        }

        void reset() {
            h = defaultH; s = defaultS; v = defaultV; a = defaultA;
            if (callback) callback(h, s, v, a);
        }

    private:
        static void RGBtoHSV(float r, float g, float b, float& h, float& s, float& v) {
            float maxC = std::max({r, g, b});
            float minC = std::min({r, g, b});
            float delta = maxC - minC;

            v = maxC;
            s = (maxC > 0) ? (delta / maxC) : 0;

            if (delta == 0) {
                h = 0;
            } else if (maxC == r) {
                h = 60 * fmodf(((g - b) / delta), 6);
            } else if (maxC == g) {
                h = 60 * (((b - r) / delta) + 2);
            } else {
                h = 60 * (((r - g) / delta) + 4);
            }

            if (h < 0) h += 360;
        }

        static void HSVtoRGB(float h, float s, float v, float& r, float& g, float& b) {
            float c = v * s;
            float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
            float m = v - c;

            if (h < 60) { r = c; g = x; b = 0; }
            else if (h < 120) { r = x; g = c; b = 0; }
            else if (h < 180) { r = 0; g = c; b = x; }
            else if (h < 240) { r = 0; g = x; b = c; }
            else if (h < 300) { r = x; g = 0; b = c; }
            else { r = c; g = 0; b = x; }

            r += m; g += m; b += m;
        }
    };

    // ============================================================================
    // Dropdown Item
    // ============================================================================
    struct DropdownItem : MenuItem {
        int selectedIndex;
        std::vector<std::string> options;
        std::function<void(int)> callback;

        DropdownItem(const std::string& i, const std::string& l, const std::vector<std::string>& opts, int defaultIdx = 0, std::function<void(int)> cb = nullptr)
            : MenuItem(i, l, ItemType::Dropdown), selectedIndex(defaultIdx), options(opts), callback(std::move(cb)) {}

        int getIndex() const { return selectedIndex; }
        std::string getLabel() const { 
            if (selectedIndex >= 0 && selectedIndex < (int)options.size())
                return options[selectedIndex];
            return "";
        }
        void setIndex(int idx) {
            if (idx >= 0 && idx < (int)options.size()) {
                selectedIndex = idx;
                if (callback) callback(selectedIndex);
            }
        }
    };

    // ============================================================================
    // Submenu Item (Forward declare SubMenu first)
    // ============================================================================
    struct SubMenu; // Forward declaration

    struct SubmenuItem : MenuItem {
        SubMenu* submenu;

        SubmenuItem(const std::string& i, const std::string& l, SubMenu* s)
            : MenuItem(i, l, ItemType::Submenu), submenu(s) {}
    };

    struct SubMenu {
        std::string id;
        std::string title;
        std::vector<MenuItem*> items;
        SubMenu* parent = nullptr;
        bool collapsed = false;
        ImVec2 position;
        bool dragEnabled = true;

        SubMenu(const std::string& i, const std::string& t, SubMenu* p = nullptr) 
            : id(i), title(t), parent(p), position(100, 100) {}

        ~SubMenu() {
            for (auto* item : items) delete item;
        }

        // Add items
        BooleanItem* addBoolean(const std::string& id, const std::string& label, bool defaultValue = false, std::function<void(bool)> callback = nullptr) {
            auto* item = new BooleanItem(id, label, defaultValue, std::move(callback));
            items.push_back(item);
            return item;
        }

        SliderItem* addSlider(const std::string& id, const std::string& label, float defaultValue, float min, float max, float step = 1.0f, const char* format = "%.0f", std::function<void(float)> callback = nullptr) {
            auto* item = new SliderItem(id, label, defaultValue, min, max, step, format, std::move(callback));
            items.push_back(item);
            return item;
        }

        ColorItem* addColor(const std::string& id, const std::string& label, float r, float g, float b, float a = 255.0f, std::function<void(float, float, float, float)> callback = nullptr) {
            auto* item = new ColorItem(id, label, r, g, b, a, std::move(callback));
            items.push_back(item);
            return item;
        }

        DropdownItem* addDropdown(const std::string& id, const std::string& label, const std::vector<std::string>& options, int defaultIndex = 0, std::function<void(int)> callback = nullptr) {
            auto* item = new DropdownItem(id, label, options, defaultIndex, std::move(callback));
            items.push_back(item);
            return item;
        }

        SubMenu* addSubmenu(const std::string& id, const std::string& title);

        void addHeader(const std::string& id, const std::string& label) {
            auto* item = new MenuItem(id, label, ItemType::Header);
            items.push_back(item);
        }

        // Find items
        MenuItem* find(const std::string& itemId) {
            for (auto* item : items) {
                if (item->id == itemId) return item;
            }
            return nullptr;
        }

        BooleanItem* findBool(const std::string& itemId) {
            auto* item = find(itemId);
            return item && item->type == ItemType::Boolean ? static_cast<BooleanItem*>(item) : nullptr;
        }

        SliderItem* findSlider(const std::string& itemId) {
            auto* item = find(itemId);
            return item && item->type == ItemType::Slider ? static_cast<SliderItem*>(item) : nullptr;
        }

        ColorItem* findColor(const std::string& itemId) {
            auto* item = find(itemId);
            return item && item->type == ItemType::Color ? static_cast<ColorItem*>(item) : nullptr;
        }

        DropdownItem* findDropdown(const std::string& itemId) {
            auto* item = find(itemId);
            return item && item->type == ItemType::Dropdown ? static_cast<DropdownItem*>(item) : nullptr;
        }

        SubMenu* findSubmenu(const std::string& submenuId) {
            for (auto* item : items) {
                if (item->type == ItemType::Submenu) {
                    auto* sub = static_cast<SubmenuItem*>(item)->submenu;
                    if (sub->id == submenuId) return sub;
                }
            }
            return nullptr;
        }
    };

    inline SubMenu* SubMenu::addSubmenu(const std::string& id, const std::string& title) {
        auto* sub = new SubMenu(id, title, this);
        auto* item = new SubmenuItem(id, title, sub);
        items.push_back(item);
        return sub;
    }

    // ============================================================================
    // Main Menu Manager
    // ============================================================================
    class MenuManager {
    private:
        std::vector<SubMenu*> menus;
        SubMenu* rootMenu = nullptr;
        ImVec2 nextMenuPos;
        int menuCount = 0;
        bool styleApplied = false;

        // Dragging state
        SubMenu* draggingMenu = nullptr;
        ImVec2 dragOffset;

    public:
        MenuManager() : nextMenuPos(100, 100) {}

        ~MenuManager() {
            for (auto* menu : menus) delete menu;
        }

        SubMenu* createMenu(const std::string& id, const std::string& title, float x = 0, float y = 0) {
            float posX = x > 0 ? x : 100 + (menuCount % 3) * 350;
            float posY = y > 0 ? y : 100 + (menuCount / 3) * 300;
            
            auto* menu = new SubMenu(id, title);
            menu->position = ImVec2(posX, posY);
            menus.push_back(menu);
            menuCount++;

            if (!rootMenu) rootMenu = menu;
            return menu;
        }

        SubMenu* getRootMenu() const { return rootMenu; }

        // ============================================================================
        // Style Application
        // ============================================================================
        void applyStyle() {
            if (styleApplied) return;

            ImGuiStyle& style = ImGui::GetStyle();
            
            // Rounding
            style.WindowRounding = 4.0f;
            style.FrameRounding = 4.0f;
            style.GrabRounding = 4.0f;
            style.TabRounding = 4.0f;
            style.ChildRounding = 4.0f;
            style.PopupRounding = 4.0f;
            style.ScrollbarRounding = 4.0f;

            // Padding
            style.WindowPadding = ImVec2(0, 0);
            style.FramePadding = ImVec2(8, 4);
            style.ItemSpacing = ImVec2(8, 6);
            style.ItemInnerSpacing = ImVec2(6, 4);

            // Border
            style.WindowBorderSize = 1.0f;
            style.FrameBorderSize = 0.0f;

            // Colors - Dark theme (cat-master style)
            ImVec4* c = style.Colors;
            c[ImGuiCol_WindowBg] = ImVec4(16.0f/255.0f, 18.0f/255.0f, 22.0f/255.0f, 0.94f);
            c[ImGuiCol_ChildBg] = ImVec4(16.0f/255.0f, 18.0f/255.0f, 22.0f/255.0f, 0.94f);
            c[ImGuiCol_PopupBg] = ImVec4(16.0f/255.0f, 18.0f/255.0f, 22.0f/255.0f, 0.94f);
            c[ImGuiCol_Border] = ImVec4(60.0f/255.0f, 64.0f/255.0f, 67.0f/255.0f, 1.0f);
            c[ImGuiCol_FrameBg] = ImVec4(24.0f/255.0f, 26.0f/255.0f, 32.0f/255.0f, 0.80f);
            c[ImGuiCol_FrameBgHovered] = ImVec4(40.0f/255.0f, 44.0f/255.0f, 56.0f/255.0f, 0.80f);
            c[ImGuiCol_FrameBgActive] = ImVec4(48.0f/255.0f, 52.0f/255.0f, 64.0f/255.0f, 0.80f);
            c[ImGuiCol_TitleBg] = ImVec4(30.0f/255.0f, 35.0f/255.0f, 40.0f/255.0f, 1.0f);
            c[ImGuiCol_TitleBgActive] = ImVec4(30.0f/255.0f, 35.0f/255.0f, 40.0f/255.0f, 1.0f);
            c[ImGuiCol_TitleBgCollapsed] = ImVec4(30.0f/255.0f, 35.0f/255.0f, 40.0f/255.0f, 0.60f);
            c[ImGuiCol_MenuBarBg] = ImVec4(30.0f/255.0f, 35.0f/255.0f, 40.0f/255.0f, 1.0f);
            c[ImGuiCol_Header] = ImVec4(0.0f/255.0f, 122.0f/255.0f, 204.0f/255.0f, 0.50f);
            c[ImGuiCol_HeaderHovered] = ImVec4(0.0f/255.0f, 122.0f/255.0f, 204.0f/255.0f, 0.80f);
            c[ImGuiCol_HeaderActive] = ImVec4(0.0f/255.0f, 122.0f/255.0f, 204.0f/255.0f, 1.0f);
            c[ImGuiCol_Button] = ImVec4(24.0f/255.0f, 26.0f/255.0f, 32.0f/255.0f, 0.80f);
            c[ImGuiCol_ButtonHovered] = ImVec4(40.0f/255.0f, 44.0f/255.0f, 56.0f/255.0f, 0.90f);
            c[ImGuiCol_ButtonActive] = ImVec4(48.0f/255.0f, 52.0f/255.0f, 64.0f/255.0f, 1.0f);
            c[ImGuiCol_SliderGrab] = ImVec4(0.0f/255.0f, 122.0f/255.0f, 204.0f/255.0f, 0.90f);
            c[ImGuiCol_SliderGrabActive] = ImVec4(80.0f/255.0f, 160.0f/255.0f, 255.0f/255.0f, 1.0f);
            c[ImGuiCol_CheckMark] = ImVec4(76.0f/255.0f, 175.0f/255.0f, 80.0f/255.0f, 1.0f);
            c[ImGuiCol_Separator] = ImVec4(60.0f/255.0f, 64.0f/255.0f, 67.0f/255.0f, 0.50f);
            c[ImGuiCol_Text] = ImVec4(232.0f/255.0f, 234.0f/255.0f, 237.0f/255.0f, 1.0f);
            c[ImGuiCol_TextDisabled] = ImVec4(154.0f/255.0f, 160.0f/255.0f, 166.0f/255.0f, 1.0f);
            c[ImGuiCol_ScrollbarBg] = ImVec4(16.0f/255.0f, 18.0f/255.0f, 22.0f/255.0f, 0.60f);
            c[ImGuiCol_ScrollbarGrab] = ImVec4(40.0f/255.0f, 44.0f/255.0f, 56.0f/255.0f, 0.80f);
            c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(56.0f/255.0f, 60.0f/255.0f, 72.0f/255.0f, 0.80f);
            c[ImGuiCol_ScrollbarGrabActive] = ImVec4(72.0f/255.0f, 80.0f/255.0f, 100.0f/255.0f, 1.0f);

            styleApplied = true;
        }

        // ============================================================================
        // Render Functions
        // ============================================================================
        
        void renderSlider(const char* id, SliderItem* slider, float width) {
            float sliderWidth = width - 140.0f;
            if (sliderWidth < 100) sliderWidth = 100;

            ImGui::PushID(id);
            
            // Background bar
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            float barHeight = 8.0f;
            float barY = cursorPos.y + (ImGui::GetTextLineHeightWithSpacing() - barHeight) / 2;
            
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(ImVec2(cursorPos.x, barY), ImVec2(cursorPos.x + sliderWidth, barY + barHeight), Colors::slider_bar, 4.0f);

            // Fill
            float ratio = (slider->value - slider->minValue) / (slider->maxValue - slider->minValue);
            float fillWidth = ratio * sliderWidth;
            if (fillWidth > 0) {
                drawList->AddRectFilled(ImVec2(cursorPos.x, barY), ImVec2(cursorPos.x + fillWidth, barY + barHeight), Colors::slider_fill, 4.0f);
            }

            // Handle
            float handleSize = 14.0f;
            float handleX = cursorPos.x + fillWidth - handleSize / 2;
            float handleY = barY - (handleSize - barHeight) / 2;
            drawList->AddRectFilled(ImVec2(handleX, handleY), ImVec2(handleX + handleSize, handleY + handleSize), Colors::slider_handle, 3.0f);

            // Invisible button for interaction
            ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y));
            ImGui::InvisibleButton("##slider_btn", ImVec2(sliderWidth, ImGui::GetTextLineHeightWithSpacing()));

            if (ImGui::IsItemActive()) {
                float mouseX = ImGui::GetMousePos().x - cursorPos.x;
                ratio = std::max(0.0f, std::min(1.0f, mouseX / sliderWidth));
                float newValue = slider->minValue + ratio * (slider->maxValue - slider->minValue);
                newValue = std::round(newValue / slider->step) * slider->step;
                slider->set(newValue);
            }

            // Value text
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
            char buf[32];
            snprintf(buf, sizeof(buf), slider->format.c_str(), slider->value);
            ImGui::TextColored(ImVec4(232.0f/255.0f, 234.0f/255.0f, 237.0f/255.0f, 1.0f), "%s", buf);

            ImGui::PopID();
        }

        void renderColorPicker(ColorItem* color, const char* id) {
            ImGui::PushID(id);
            
            float* col[4] = { &color->h, &color->s, &color->v, &color->a };
            float col4[4] = { color->h / 360.0f, color->s / 100.0f, color->v / 100.0f, color->a / 255.0f };
            
            if (ImGui::ColorEdit4("##color", col4, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar)) {
                color->h = col4[0] * 360.0f;
                color->s = col4[1] * 100.0f;
                color->v = col4[2] * 100.0f;
                color->a = col4[3] * 255.0f;
                if (color->callback) color->callback(color->h, color->s, color->v, color->a);
            }

            // Custom color preview box
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float boxSize = ImGui::GetTextLineHeightWithSpacing() - 4;
            drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + boxSize, cursorPos.y + boxSize), color->getColor(), 3.0f);
            drawList->AddRect(cursorPos, ImVec2(cursorPos.x + boxSize, cursorPos.y + boxSize), Colors::border, 3.0f);
            
            ImGui::SameLine(boxSize + 8);

            ImGui::PopID();
        }

        void renderMenu(SubMenu* menu) {
            if (!menu || menu->items.empty()) return;
            if (menu->parent && menu->collapsed) return;

            // Calculate menu dimensions
            const float itemHeight = 26.0f;
            const float titleHeight = 26.0f;
            const float padding = 8.0f;
            const float labelWidth = 150.0f;
            
            float menuWidth = 320.0f;
            float menuHeight = titleHeight + padding;
            
            for (auto* item : menu->items) {
                if (!item->visible) continue;
                menuHeight += itemHeight;
                if (item->type == ItemType::Slider && static_cast<SliderItem*>(item)->expanded) {
                    menuHeight += itemHeight;
                }
            }
            menuHeight += padding;

            // Clamp position to screen
            ImVec2 screenSize = ImGui::GetIO().DisplaySize;
            ImVec2 pos = menu->position;
            if (pos.x + menuWidth > screenSize.x - 10) pos.x = screenSize.x - menuWidth - 10;
            if (pos.y + menuHeight > screenSize.y - 10) pos.y = screenSize.y - menuHeight - 10;
            menu->position = pos;

            // Set next window position for dragging
            ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight), ImGuiCond_Always);

            // Begin window
            char windowName[128];
            snprintf(windowName, sizeof(windowName), "%s##%s", menu->title.c_str(), menu->id.c_str());
            
            // Only allow dragging for root menus or non-popup submenus
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize;
            if (menu->dragEnabled && !menu->parent) {
                flags |= ImGuiWindowFlags_NoMove; // We'll handle dragging manually
            }

            if (ImGui::Begin(windowName, nullptr, flags)) {
                // Update position after window is positioned
                menu->position = ImGui::GetWindowPos();

                // Handle window dragging for root menus
                if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !menu->parent) {
                    // Check if clicking on title bar
                    ImVec2 mousePos = ImGui::GetMousePos();
                    ImVec2 winPos = ImGui::GetWindowPos();
                    if (mousePos.y < winPos.y + titleHeight) {
                        draggingMenu = menu;
                        dragOffset = ImVec2(mousePos.x - winPos.x, mousePos.y - winPos.y);
                    }
                }

                // Title bar
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 winPos = ImGui::GetWindowPos();
                
                // Title background
                drawList->AddRectFilled(winPos, ImVec2(winPos.x + menuWidth, winPos.y + titleHeight), Colors::bg_title, 4.0f);
                
                // Title text (centered)
                ImVec2 titleTextSize = ImGui::CalcTextSize(menu->title.c_str());
                ImVec2 titlePos(winPos.x + (menuWidth - titleTextSize.x) / 2, winPos.y + (titleHeight - titleTextSize.y) / 2);
                drawList->AddText(titlePos, Colors::text_primary, menu->title.c_str());

                // Menu items
                ImGui::SetCursorPosY(titleHeight + padding);
                
                for (auto* item : menu->items) {
                    if (!item->visible) continue;

                    // Background for item
                    bool hovered = ImGui::IsItemHovered();
                    ImVec2 itemMin = ImGui::GetCursorScreenPos();
                    ImVec2 itemMax(itemMin.x + menuWidth, itemMin.y + itemHeight);

                    switch (item->type) {
                        case ItemType::Header: {
                            // Header - centered text with different color
                            drawList->AddRectFilled(itemMin, itemMax, Colors::bg_main);
                            ImVec2 textSize = ImGui::CalcTextSize(item->label.c_str());
                            ImVec2 textPos(itemMin.x + (menuWidth - textSize.x) / 2, itemMin.y + (itemHeight - textSize.y) / 2);
                            drawList->AddText(textPos, Colors::text_header, item->label.c_str());
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + itemHeight);
                            break;
                        }

                        case ItemType::Boolean: {
                            auto* boolItem = static_cast<BooleanItem*>(item);
                            drawList->AddRectFilled(itemMin, itemMax, hovered ? Colors::bg_hover_light : Colors::bg_main);
                            
                            // Label
                            ImVec2 labelPos(itemMin.x + 12, itemMin.y + (itemHeight - ImGui::GetTextLineHeight()) / 2);
                            drawList->AddText(labelPos, Colors::text_primary, item->label.c_str());
                            
                            // Checkbox indicator
                            std::string checkMark = boolItem->value ? "[V]" : "[X]";
                            ImVec2 checkSize = ImGui::CalcTextSize(checkMark.c_str());
                            ImVec2 checkPos(itemMax.x - checkSize.x - 12, itemMin.y + (itemHeight - checkSize.y) / 2);
                            drawList->AddText(checkPos, boolItem->value ? Colors::checkbox_on : Colors::checkbox_off, checkMark.c_str());
                            
                            // Click area
                            ImGui::SetCursorScreenPos(itemMin);
                            ImGui::InvisibleButton(("##bool_" + item->id).c_str(), ImVec2(menuWidth, itemHeight));
                            if (ImGui::IsItemClicked()) {
                                boolItem->set(!boolItem->value);
                            }
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + itemHeight);
                            break;
                        }

                        case ItemType::Slider: {
                            auto* sliderItem = static_cast<SliderItem*>(item);
                            drawList->AddRectFilled(itemMin, itemMax, hovered ? Colors::bg_hover_light : Colors::bg_main);
                            
                            // Label
                            ImVec2 labelPos(itemMin.x + 12, itemMin.y + (itemHeight - ImGui::GetTextLineHeight()) / 2);
                            drawList->AddText(labelPos, Colors::text_primary, item->label.c_str());
                            
                            // Slider render
                            ImGui::SetCursorScreenPos(ImVec2(itemMin.x + 12, itemMin.y + itemHeight - 12));
                            renderSlider(item->id.c_str(), sliderItem, menuWidth - 24);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + itemHeight);
                            break;
                        }

                        case ItemType::Color: {
                            auto* colorItem = static_cast<ColorItem*>(item);
                            drawList->AddRectFilled(itemMin, itemMax, hovered ? Colors::bg_hover_light : Colors::bg_main);
                            
                            // Label
                            ImVec2 labelPos(itemMin.x + 12, itemMin.y + (itemHeight - ImGui::GetTextLineHeight()) / 2);
                            drawList->AddText(labelPos, Colors::text_primary, item->label.c_str());
                            
                            // Color picker
                            ImGui::SetCursorScreenPos(ImVec2(itemMin.x + menuWidth - 100, itemMin.y + 3));
                            renderColorPicker(colorItem, item->id.c_str());
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + itemHeight);
                            break;
                        }

                        case ItemType::Dropdown: {
                            auto* dropdownItem = static_cast<DropdownItem*>(item);
                            drawList->AddRectFilled(itemMin, itemMax, hovered ? Colors::bg_hover_light : Colors::bg_main);
                            
                            // Label
                            ImVec2 labelPos(itemMin.x + 12, itemMin.y + (itemHeight - ImGui::GetTextLineHeight()) / 2);
                            drawList->AddText(labelPos, Colors::text_primary, item->label.c_str());
                            
                            // Selected value
                            std::string selected = dropdownItem->getLabel();
                            ImVec2 selSize = ImGui::CalcTextSize(selected.c_str());
                            ImVec2 selPos(itemMax.x - selSize.x - 24, itemMin.y + (itemHeight - selSize.y) / 2);
                            drawList->AddText(selPos, Colors::text_primary, selected.c_str());
                            
                            // Arrow
                            ImVec2 arrowPos(itemMax.x - 18, itemMin.y + (itemHeight - ImGui::GetTextLineHeight()) / 2);
                            drawList->AddText(arrowPos, Colors::text_secondary, ">");
                            
                            // Dropdown popup
                            ImGui::SetCursorScreenPos(itemMin);
                            ImGui::InvisibleButton(("##dropdown_" + item->id).c_str(), ImVec2(menuWidth, itemHeight));
                            if (ImGui::IsItemClicked()) {
                                ImGui::OpenPopup(("##dropdown_popup_" + item->id).c_str());
                            }
                            
                            if (ImGui::BeginPopup(("##dropdown_popup_" + item->id).c_str())) {
                                for (int i = 0; i < (int)dropdownItem->options.size(); i++) {
                                    bool selected = (i == dropdownItem->selectedIndex);
                                    if (ImGui::Selectable(dropdownItem->options[i].c_str(), selected)) {
                                        dropdownItem->setIndex(i);
                                    }
                                }
                                ImGui::EndPopup();
                            }
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + itemHeight);
                            break;
                        }

                        case ItemType::Submenu: {
                            auto* submenuItem = static_cast<SubmenuItem*>(item);
                            drawList->AddRectFilled(itemMin, itemMax, hovered ? Colors::bg_hover_light : Colors::bg_main);
                            
                            // Label
                            ImVec2 labelPos(itemMin.x + 12, itemMin.y + (itemHeight - ImGui::GetTextLineHeight()) / 2);
                            drawList->AddText(labelPos, Colors::text_primary, item->label.c_str());
                            
                            // Arrow
                            ImVec2 arrowPos(itemMax.x - 18, itemMin.y + (itemHeight - ImGui::GetTextLineHeight()) / 2);
                            drawList->AddText(arrowPos, Colors::text_secondary, ">");
                            
                            // Click to toggle
                            ImGui::SetCursorScreenPos(itemMin);
                            ImGui::InvisibleButton(("##submenu_" + item->id).c_str(), ImVec2(menuWidth, itemHeight));
                            if (ImGui::IsItemClicked()) {
                                submenuItem->submenu->collapsed = !submenuItem->submenu->collapsed;
                            }
                            
                            // Render submenu
                            if (!submenuItem->submenu->collapsed) {
                                ImVec2 submenuPos(itemMax.x + 2, itemMin.y);
                                submenuItem->submenu->position = submenuPos;
                                renderMenu(submenuItem->submenu);
                            }
                            
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + itemHeight);
                            break;
                        }
                    }
                }
            }
            ImGui::End();
        }

        // ============================================================================
        // Main Render
        // ============================================================================
        void render() {
            applyStyle();

            // Handle menu dragging
            if (draggingMenu && ImGui::IsMouseDown(0)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                draggingMenu->position = ImVec2(mousePos.x - dragOffset.x, mousePos.y - dragOffset.y);
            } else {
                draggingMenu = nullptr;
            }

            // Render all menus
            for (auto* menu : menus) {
                if (!menu->parent) { // Only render root menus, submenus are rendered recursively
                    renderMenu(menu);
                }
            }
        }
    };

    // ============================================================================
    // Global Instance
    // ============================================================================
    inline MenuManager g_menu;

    // ============================================================================
    // Helper Functions
    // ============================================================================
    inline SubMenu* CreateMenu(const std::string& id, const std::string& title, float x = 0, float y = 0) {
        return g_menu.createMenu(id, title, x, y);
    }

    inline void Render() {
        g_menu.render();
    }

} // namespace CatMenu
