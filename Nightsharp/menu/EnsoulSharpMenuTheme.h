#pragma once

#include "../SDK/UI/UI.h"
#include "../imgui/imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace NightSharpMenu::EnsoulSharpTheme {

using namespace SDK::UI;
// UI.h also publishes SDK::UI types as global compatibility aliases.  Give
// Menu an unambiguous local name so MSVC does not see two equally valid types.
using Menu = ::SDK::UI::Menu;

inline constexpr float ContainerHeight = 30.0f;
inline constexpr float ContainerWidth = 200.0f;
inline constexpr float ContainerTextOffset = 15.0f;
inline constexpr float ContainerTextMarkOffset = 8.0f;
inline constexpr float FontSize = 15.0f;
inline constexpr float PositionX = 30.0f;
inline constexpr float PositionY = 30.0f;

inline constexpr ImU32 HoverColor = IM_COL32(255, 255, 255, 50);
inline constexpr ImU32 RootContainerColor = IM_COL32(0, 0, 0, 62);
inline constexpr ImU32 ContainerSelectedColor = IM_COL32(255, 255, 255, 125);
inline constexpr ImU32 ContainerSeparatorColor = IM_COL32(255, 255, 255, 100);
inline constexpr ImU32 TextColor = IM_COL32(255, 255, 255, 255);
inline constexpr ImU32 BorderColor = IM_COL32(0, 0, 0, 255);
inline constexpr ImU32 EnabledColor = IM_COL32(0, 100, 0, 255);
inline constexpr ImU32 DisabledColor = IM_COL32(255, 0, 0, 255);
inline constexpr ImU32 SliderColor = IM_COL32(50, 154, 205, 255);
inline constexpr ImU32 SliderActiveColor = IM_COL32(255, 0, 0, 255);
inline constexpr ImU32 ButtonColor = IM_COL32(100, 100, 100, 255);
inline constexpr ImU32 ButtonHoverColor = IM_COL32(170, 170, 170, 200);
inline constexpr const char* ArrowText = "\xC2\xBB";
inline constexpr const char* SelectedText = "\xE2\x88\x9A";
inline constexpr ImWchar GlyphRanges[] = {
    0x0020, 0x00FF,
    0x0102, 0x0103,
    0x0110, 0x0111,
    0x0128, 0x0129,
    0x0168, 0x0169,
    0x01A0, 0x01A1,
    0x01AF, 0x01B0,
    0x1EA0, 0x1EF9,
    0x221A, 0x221A,
    0
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool Contains(const ImVec2& point) const {
        return point.x >= x && point.x <= x + w &&
               point.y >= y && point.y <= y + h;
    }
};

inline ImFont* MenuFont = nullptr;
inline MenuList* OpenList = nullptr;
inline MenuSliderButton* DragSliderButton = nullptr;
inline MenuColor* OpenColor = nullptr;
inline Rect OpenListControl = {};
inline Rect OpenListPopup = {};
inline Rect OpenColorPreview = {};
inline Rect OpenColorPicker = {};
inline MenuItem* TooltipItem = nullptr;
inline Rect TooltipSource = {};
inline bool RegularClickBlocked = false;
inline float BoundsRight = PositionX;
inline float BoundsBottom = PositionY;
inline Rect HitRects[256] = {};
inline int HitRectCount = 0;

inline void SetFont(ImFont* font) {
    MenuFont = font;
}

inline ImFont* Font() {
    return MenuFont ? MenuFont : ImGui::GetFont();
}

inline float TextWidth(const char* text) {
    if (!text || !text[0] || !Font()) {
        return 0.0f;
    }
    return Font()->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, text).x;
}

inline float TextHeight() {
    return Font() ? Font()->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, "Ag").y : FontSize;
}

inline float CenteredTextY(float y, float height = ContainerHeight) {
    return std::floor(y + (height - TextHeight()) * 0.5f);
}

inline void DrawText(ImDrawList* draw,
                     const ImVec2& position,
                     ImU32 color,
                     const char* text) {
    if (draw && Font() && text && text[0]) {
        draw->AddText(Font(), FontSize, position, color, text);
    }
}

inline void IncludeBounds(const Rect& rect) {
    BoundsRight = std::max(BoundsRight, rect.x + rect.w);
    BoundsBottom = std::max(BoundsBottom, rect.y + rect.h);
    if (rect.w > 0.0f && rect.h > 0.0f &&
        HitRectCount < static_cast<int>(sizeof(HitRects) / sizeof(HitRects[0]))) {
        HitRects[HitRectCount++] = rect;
    }
}

inline bool ContainsPoint(float x, float y) {
    const ImVec2 point(x, y);
    for (int i = 0; i < HitRectCount; ++i) {
        if (HitRects[i].Contains(point)) {
            return true;
        }
    }
    return false;
}

inline void FillRect(ImDrawList* draw, const Rect& rect, ImU32 color) {
    draw->AddRectFilled(
        ImVec2(rect.x, rect.y),
        ImVec2(rect.x + rect.w, rect.y + rect.h),
        color,
        0.0f);
}

inline void BorderRect(ImDrawList* draw, const Rect& rect, ImU32 color) {
    draw->AddRect(
        ImVec2(rect.x, rect.y),
        ImVec2(rect.x + rect.w, rect.y + rect.h),
        color,
        0.0f,
        0,
        1.0f);
}

inline int VisibleChildCount(const Menu* menu) {
    if (!menu) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < menu->Components.size(); ++i) {
        if (menu->Components[i] && menu->Components[i]->Visible) {
            ++count;
        }
    }
    return count;
}

inline float MaximumOptionWidth(const MenuList* list) {
    float width = 0.0f;
    if (!list) {
        return width;
    }
    for (int i = 0; i < list->Options.size(); ++i) {
        width = std::max(width, TextWidth(list->Options[i].c_str()));
    }
    return width;
}

inline float ComponentWidth(const AMenuComponent* component) {
    if (!component) {
        return ContainerWidth;
    }

    const float itemWidth = TextWidth(component->DisplayName.c_str()) +
                            ContainerTextOffset * 2.0f;
    float width = itemWidth;
    switch (component->Kind()) {
    case MenuValueType::Boolean:
    case MenuValueType::Color:
        width += ContainerHeight;
        break;
    case MenuValueType::Slider:
    case MenuValueType::SliderF:
    case MenuValueType::SliderBtn:
        width += 100.0f;
        break;
    case MenuValueType::KeyBind: {
        const auto* key = static_cast<const MenuKeyBind*>(component);
        char keyText[48] = {};
        std::snprintf(keyText, sizeof(keyText), "[%s]", MenuKeyBind::VkToText(key->Key));
        width += ContainerHeight + TextWidth(keyText) + ContainerTextOffset;
        break;
    }
    case MenuValueType::List: {
        const auto* list = static_cast<const MenuList*>(component);
        width += MaximumOptionWidth(list) + 16.0f + TextWidth(ArrowText) + 17.0f;
        break;
    }
    case MenuValueType::Button: {
        const auto* button = static_cast<const MenuButton*>(component);
        width += TextWidth(button->ButtonText.c_str()) + 10.0f;
        break;
    }
    default:
        break;
    }

    if (component->IsMenu()) {
        width = TextWidth(component->DisplayName.c_str()) +
                TextWidth(" ") + TextWidth(ArrowText) +
                ContainerTextOffset * 2.0f +
                TextWidth(ArrowText) + 5.0f;
    }
    return std::max(ContainerWidth, std::ceil(width));
}

inline float RootColumnWidth() {
    float width = ContainerWidth;
    auto& manager = MenuManager::Instance();
    for (int i = 0; i < manager.Menus.size(); ++i) {
        if (manager.Menus[i] && manager.Menus[i]->Visible) {
            width = std::max(width, ComponentWidth(manager.Menus[i]));
        }
    }
    return width;
}

inline float ChildColumnWidth(const Menu* menu) {
    float width = ContainerWidth;
    if (!menu) {
        return width;
    }
    for (int i = 0; i < menu->Components.size(); ++i) {
        const auto* component = menu->Components[i];
        if (component && component->Visible) {
            width = std::max(width, ComponentWidth(component));
        }
    }
    return width;
}

inline void CloseMenu(Menu* menu) {
    if (!menu) {
        return;
    }
    menu->Toggled = false;
    for (int i = 0; i < menu->Components.size(); ++i) {
        if (auto* child = dynamic_cast<Menu*>(menu->Components[i])) {
            CloseMenu(child);
        }
    }
}

inline void ToggleMenu(Menu* menu) {
    if (!menu || VisibleChildCount(menu) == 0) {
        return;
    }

    const bool open = !menu->Toggled;
    if (menu->Parent && menu->Parent->IsMenu()) {
        auto* parent = static_cast<Menu*>(menu->Parent);
        for (int i = 0; i < parent->Components.size(); ++i) {
            if (auto* sibling = dynamic_cast<Menu*>(parent->Components[i]);
                sibling && sibling != menu) {
                CloseMenu(sibling);
            }
        }
    } else {
        auto& manager = MenuManager::Instance();
        for (int i = 0; i < manager.Menus.size(); ++i) {
            if (manager.Menus[i] && manager.Menus[i] != menu) {
                CloseMenu(manager.Menus[i]);
            }
        }
    }

    menu->Toggled = open;
    if (!open) {
        for (int i = 0; i < menu->Components.size(); ++i) {
            if (auto* child = dynamic_cast<Menu*>(menu->Components[i])) {
                CloseMenu(child);
            }
        }
    }
    OpenList = nullptr;
    OpenColor = nullptr;
}

inline void Notify(MenuItem* item) {
    if (!item) {
        return;
    }
    item->FireValueChanged();
    if (item->Parent && item->Parent->IsMenu()) {
        static_cast<Menu*>(item->Parent)->FireMenuValueChanged(item);
    }
}

inline bool Clicked(const Rect& rect) {
    return !RegularClickBlocked && rect.Contains(ImGui::GetIO().MousePos) &&
           ImGui::IsMouseClicked(0);
}

inline bool Hovered(const Rect& rect) {
    return rect.Contains(ImGui::GetIO().MousePos);
}

inline void QueueTooltip(MenuItem* item, const Rect& source) {
    if (item && !item->Tooltip.empty() && Hovered(source)) {
        TooltipItem = item;
        TooltipSource = source;
    }
}

inline void DrawOnOff(ImDrawList* draw,
                      const Rect& row,
                      bool enabled,
                      bool insetY = false) {
    const Rect button{
        row.x + row.w - ContainerHeight,
        row.y + (insetY ? 1.0f : 0.0f),
        ContainerHeight - 2.0f,
        ContainerHeight - 1.0f
    };
    FillRect(draw, button, enabled ? EnabledColor : DisabledColor);
    const char* value = enabled ? "ON" : "OFF";
    const float x = row.x + row.w - ContainerHeight +
                    (ContainerHeight - TextWidth(value)) * 0.5f;
    DrawText(draw, ImVec2(std::floor(x), CenteredTextY(row.y)), TextColor, value);
}

inline void DrawLabel(ImDrawList* draw, const Rect& row, const char* label) {
    DrawText(
        draw,
        ImVec2(row.x + ContainerTextOffset, CenteredTextY(row.y)),
        TextColor,
        label);
}

inline float SliderRatio(float value, float minimum, float maximum) {
    const float span = maximum - minimum;
    if (span <= 0.0f) {
        return 0.0f;
    }
    return std::clamp((value - minimum) / span, 0.0f, 1.0f);
}

inline int SliderValueFromMouse(const Rect& row, int minimum, int maximum) {
    const float ratio = std::clamp(
        (ImGui::GetIO().MousePos.x - row.x) / std::max(1.0f, row.w),
        0.0f,
        1.0f);
    return static_cast<int>(std::lround(
        static_cast<float>(minimum) + ratio * static_cast<float>(maximum - minimum)));
}

inline float SliderFloatFromMouse(const Rect& row, float minimum, float maximum) {
    const float ratio = std::clamp(
        (ImGui::GetIO().MousePos.x - row.x) / std::max(1.0f, row.w),
        0.0f,
        1.0f);
    return minimum + ratio * (maximum - minimum);
}

inline void DrawBool(ImDrawList* draw, MenuBool* item, const Rect& row) {
    DrawLabel(draw, row, item->DisplayName.c_str());
    DrawOnOff(draw, row, item->Value);
    const Rect button{ row.x + row.w - ContainerHeight, row.y, ContainerHeight, ContainerHeight };
    if (Clicked(button)) {
        item->Set(!item->Value);
    }
    QueueTooltip(item, row);
}

inline void DrawSlider(ImDrawList* draw, MenuSlider* item, const Rect& row) {
    const float ratio = SliderRatio(
        static_cast<float>(item->Value),
        static_cast<float>(item->MinValue),
        static_cast<float>(item->MaxValue));
    const float markerX = row.x + ratio * row.w;
    FillRect(draw, Rect{ row.x, row.y, markerX - row.x, row.h }, HoverColor);
    FillRect(
        draw,
        Rect{ markerX, row.y, 2.0f, row.h },
        item->Interacting ? SliderActiveColor : SliderColor);
    DrawLabel(draw, row, item->DisplayName.c_str());
    char value[32] = {};
    std::snprintf(value, sizeof(value), "%d", item->Value);
    DrawText(
        draw,
        ImVec2(row.x + row.w - 5.0f - TextWidth(value), CenteredTextY(row.y)),
        TextColor,
        value);

    if (Clicked(row)) {
        item->Interacting = true;
    }
    if (item->Interacting) {
        if (ImGui::IsMouseDown(0)) {
            item->Set(SliderValueFromMouse(row, item->MinValue, item->MaxValue));
        } else {
            item->Interacting = false;
        }
    }
    QueueTooltip(item, row);
}

inline void DrawSliderF(ImDrawList* draw, MenuSliderF* item, const Rect& row) {
    const float ratio = SliderRatio(item->Value, item->MinValue, item->MaxValue);
    const float markerX = row.x + ratio * row.w;
    FillRect(draw, Rect{ row.x, row.y, markerX - row.x, row.h }, HoverColor);
    FillRect(
        draw,
        Rect{ markerX, row.y, 2.0f, row.h },
        item->Interacting ? SliderActiveColor : SliderColor);
    DrawLabel(draw, row, item->DisplayName.c_str());
    char value[32] = {};
    std::snprintf(value, sizeof(value), "%.2f", item->Value);
    DrawText(
        draw,
        ImVec2(row.x + row.w - 5.0f - TextWidth(value), CenteredTextY(row.y)),
        TextColor,
        value);

    if (Clicked(row)) {
        item->Interacting = true;
    }
    if (item->Interacting) {
        if (ImGui::IsMouseDown(0)) {
            item->Set(SliderFloatFromMouse(row, item->MinValue, item->MaxValue));
        } else {
            item->Interacting = false;
        }
    }
    QueueTooltip(item, row);
}

inline void DrawKeyBind(ImDrawList* draw, MenuKeyBind* item, const Rect& row) {
    DrawLabel(
        draw,
        row,
        item->Interacting ? "Press a key" : item->DisplayName.c_str());
    if (!item->Interacting) {
        char keyText[48] = {};
        std::snprintf(keyText, sizeof(keyText), "[%s]", MenuKeyBind::VkToText(item->Key));
        DrawText(
            draw,
            ImVec2(
                row.x + row.w - ContainerHeight - TextWidth(keyText) - ContainerTextOffset,
                CenteredTextY(row.y)),
            TextColor,
            keyText);
    }
    DrawOnOff(draw, row, item->Active, true);

    const Rect button{ row.x + row.w - ContainerHeight, row.y, ContainerHeight, ContainerHeight };
    if (Clicked(button)) {
        item->SetActive(!item->Active);
    } else if (Clicked(row)) {
        item->Interacting = !item->Interacting;
    }
    QueueTooltip(item, row);
}

inline void DrawList(ImDrawList* draw, MenuList* item, const Rect& row) {
    DrawLabel(draw, row, item->DisplayName.c_str());
    const float maxStringWidth = MaximumOptionWidth(item);
    const float arrowWidth = TextWidth(ArrowText) + 12.0f;
    const float controlWidth = maxStringWidth + 16.0f + arrowWidth;
    const float controlX = row.x + row.w - controlWidth;
    draw->AddLine(
        ImVec2(controlX, row.y),
        ImVec2(controlX, row.y + row.h),
        ContainerSeparatorColor,
        1.0f);
    draw->AddLine(
        ImVec2(row.x + row.w - arrowWidth - 1.0f, row.y),
        ImVec2(row.x + row.w - arrowWidth - 1.0f, row.y + row.h),
        ContainerSeparatorColor,
        1.0f);
    DrawText(
        draw,
        ImVec2(controlX + 8.0f, CenteredTextY(row.y)),
        TextColor,
        item->SelectedValue());
    DrawText(
        draw,
        ImVec2(row.x + row.w - arrowWidth + 6.0f, CenteredTextY(row.y)),
        TextColor,
        ArrowText);

    const Rect control{ controlX, row.y, controlWidth, row.h };
    if (Clicked(control)) {
        OpenList = OpenList == item ? nullptr : item;
        OpenColor = nullptr;
    }
    if (OpenList == item) {
        OpenListControl = control;
        OpenListPopup = Rect{
            row.x + row.w,
            row.y,
            controlWidth,
            ContainerHeight * static_cast<float>(item->Options.size())
        };
    }
    QueueTooltip(item, row);
}

inline void DrawButton(ImDrawList* draw, MenuButton* item, const Rect& row) {
    DrawLabel(draw, row, item->DisplayName.c_str());
    const float textWidth = TextWidth(item->ButtonText.c_str());
    const Rect button{
        row.x + row.w - textWidth - 10.0f,
        row.y,
        textWidth + 10.0f,
        row.h
    };
    FillRect(
        draw,
        Rect{ button.x + 2.0f, button.y + 1.0f, textWidth + 5.0f, row.h },
        Hovered(button) ? ButtonHoverColor : ButtonColor);
    DrawText(
        draw,
        ImVec2(row.x + row.w - textWidth - 5.0f, CenteredTextY(row.y)),
        TextColor,
        item->ButtonText.c_str());
    if (Clicked(button)) {
        if (item->Callback) {
            item->Callback();
        } else if (item->Action) {
            item->Action(item, item->ActionUd);
        }
        item->FireValueChanged();
    }
    QueueTooltip(item, row);
}

inline void DrawColor(ImDrawList* draw, MenuColor* item, const Rect& row) {
    DrawLabel(draw, row, item->DisplayName.c_str());
    const Rect preview{
        row.x + row.w - ContainerHeight,
        row.y,
        ContainerHeight,
        ContainerHeight
    };
    FillRect(draw, preview, item->Value);
    if (Clicked(preview)) {
        OpenColor = OpenColor == item ? nullptr : item;
        OpenList = nullptr;
    }
    if (OpenColor == item) {
        OpenColorPreview = preview;
    }
    QueueTooltip(item, row);
}

inline void DrawSeparator(ImDrawList* draw, MenuSeparator* item, const Rect& row) {
    const float x = row.x + (row.w - TextWidth(item->DisplayName.c_str())) * 0.5f;
    DrawText(
        draw,
        ImVec2(std::floor(x), CenteredTextY(row.y)),
        TextColor,
        item->DisplayName.c_str());
    QueueTooltip(item, row);
}

inline void DrawSliderButton(ImDrawList* draw,
                             MenuSliderButton* item,
                             const Rect& row) {
    const float sliderWidth = row.w - ContainerHeight;
    const float ratio = SliderRatio(
        static_cast<float>(item->Value),
        static_cast<float>(item->MinValue),
        static_cast<float>(item->MaxValue));
    const float markerX = row.x + ratio * sliderWidth;
    FillRect(draw, Rect{ row.x, row.y, markerX - row.x, row.h }, HoverColor);
    FillRect(
        draw,
        Rect{ markerX, row.y, 2.0f, row.h },
        DragSliderButton == item ? SliderActiveColor : SliderColor);
    DrawLabel(draw, row, item->DisplayName.c_str());
    char value[32] = {};
    std::snprintf(value, sizeof(value), "%d", item->Value);
    DrawText(
        draw,
        ImVec2(
            row.x + row.w - ContainerHeight - 5.0f - TextWidth(value),
            CenteredTextY(row.y)),
        TextColor,
        value);
    DrawOnOff(draw, row, item->Enabled);

    const Rect button{ row.x + row.w - ContainerHeight, row.y, ContainerHeight, row.h };
    const Rect slider{ row.x, row.y, sliderWidth, row.h };
    if (Clicked(button)) {
        item->Enabled = !item->Enabled;
        Notify(item);
    } else if (Clicked(slider)) {
        DragSliderButton = item;
    }
    if (DragSliderButton == item) {
        if (ImGui::IsMouseDown(0)) {
            const int valueAtMouse = SliderValueFromMouse(
                slider,
                item->MinValue,
                item->MaxValue);
            if (valueAtMouse != item->Value) {
                item->Value = valueAtMouse;
                Notify(item);
            }
        } else {
            DragSliderButton = nullptr;
        }
    }
    QueueTooltip(item, row);
}

inline void DrawComponent(ImDrawList* draw,
                          AMenuComponent* component,
                          const Rect& row);

inline void DrawColumn(ImDrawList* draw, Menu* menu, const ImVec2& position) {
    if (!menu) {
        return;
    }

    const int count = VisibleChildCount(menu);
    if (count <= 0) {
        return;
    }
    const float width = ChildColumnWidth(menu);
    const Rect panel{
        position.x,
        position.y,
        width,
        ContainerHeight * static_cast<float>(count)
    };
    FillRect(draw, panel, RootContainerColor);
    IncludeBounds(panel);

    int rowIndex = 0;
    for (int i = 0; i < menu->Components.size(); ++i) {
        AMenuComponent* component = menu->Components[i];
        if (!component || !component->Visible) {
            continue;
        }
        const Rect row{
            position.x,
            position.y + static_cast<float>(rowIndex) * ContainerHeight,
            width,
            ContainerHeight
        };
        BorderRect(draw, row, ContainerSeparatorColor);
        DrawComponent(draw, component, row);
        ++rowIndex;
    }
    BorderRect(draw, panel, BorderColor);
}

inline void DrawMenu(ImDrawList* draw, Menu* menu, const Rect& row) {
    const int childCount = VisibleChildCount(menu);
    if (Hovered(row) && !menu->Toggled && childCount > 0) {
        FillRect(draw, row, HoverColor);
    }

    DrawLabel(draw, row, menu->DisplayName.c_str());
    const char* arrow = ArrowText;
    DrawText(
        draw,
        ImVec2(
            row.x + row.w - TextWidth(arrow) - ContainerTextMarkOffset,
            CenteredTextY(row.y)),
        childCount > 0 ? TextColor : ContainerSeparatorColor,
        arrow);

    if (menu->Toggled) {
        FillRect(draw, row, ContainerSelectedColor);
    }
    if (Clicked(row)) {
        ToggleMenu(menu);
    }

    if (menu->Toggled && childCount > 0) {
        DrawColumn(draw, menu, ImVec2(row.x + row.w, row.y));
    }
}

inline void DrawComponent(ImDrawList* draw,
                          AMenuComponent* component,
                          const Rect& row) {
    if (!component) {
        return;
    }
    if (auto* menu = dynamic_cast<Menu*>(component)) {
        DrawMenu(draw, menu, row);
        return;
    }
    if (auto* item = dynamic_cast<MenuItem*>(component)) {
        switch (item->Kind()) {
        case MenuValueType::Boolean:
            DrawBool(draw, static_cast<MenuBool*>(item), row);
            break;
        case MenuValueType::Slider:
            DrawSlider(draw, static_cast<MenuSlider*>(item), row);
            break;
        case MenuValueType::SliderF:
            DrawSliderF(draw, static_cast<MenuSliderF*>(item), row);
            break;
        case MenuValueType::KeyBind:
            DrawKeyBind(draw, static_cast<MenuKeyBind*>(item), row);
            break;
        case MenuValueType::List:
            DrawList(draw, static_cast<MenuList*>(item), row);
            break;
        case MenuValueType::Button:
            DrawButton(draw, static_cast<MenuButton*>(item), row);
            break;
        case MenuValueType::Color:
            DrawColor(draw, static_cast<MenuColor*>(item), row);
            break;
        case MenuValueType::Separator:
            DrawSeparator(draw, static_cast<MenuSeparator*>(item), row);
            break;
        case MenuValueType::SliderBtn:
            DrawSliderButton(draw, static_cast<MenuSliderButton*>(item), row);
            break;
        default:
            DrawLabel(draw, row, item->DisplayName.c_str());
            break;
        }
    }
}

inline void DrawListPopup(ImDrawList* draw) {
    if (!OpenList || OpenList->Options.empty()) {
        OpenListPopup = {};
        return;
    }

    const Rect popup = OpenListPopup;
    FillRect(draw, popup, RootContainerColor);
    IncludeBounds(popup);
    for (int i = 0; i < OpenList->Options.size(); ++i) {
        const Rect row{
            popup.x,
            popup.y + static_cast<float>(i) * ContainerHeight,
            popup.w,
            ContainerHeight
        };
        BorderRect(draw, row, ContainerSeparatorColor);
        DrawText(
            draw,
            ImVec2(row.x + 16.0f, CenteredTextY(row.y)),
            TextColor,
            OpenList->Options[i].c_str());
        if (OpenList->Index == i) {
            DrawText(
                draw,
                ImVec2(row.x + row.w - TextWidth(SelectedText) - 8.0f, CenteredTextY(row.y)),
                TextColor,
                SelectedText);
        }
        if (Hovered(row) && ImGui::IsMouseClicked(0)) {
            OpenList->Set(i);
            OpenList = nullptr;
            break;
        }
    }
    BorderRect(draw, popup, BorderColor);
}

inline int ColorByte(unsigned int value, int shift) {
    return static_cast<int>((value >> shift) & 0xFFu);
}

inline void SetColorChannel(MenuColor* item, int channel, int value) {
    if (!item) {
        return;
    }
    value = std::clamp(value, 0, 255);
    int red = ColorByte(item->Value, IM_COL32_R_SHIFT);
    int green = ColorByte(item->Value, IM_COL32_G_SHIFT);
    int blue = ColorByte(item->Value, IM_COL32_B_SHIFT);
    int alpha = ColorByte(item->Value, IM_COL32_A_SHIFT);
    if (channel == 0) red = value;
    if (channel == 1) green = value;
    if (channel == 2) blue = value;
    if (channel == 3) alpha = value;
    item->Set(IM_COL32(red, green, blue, alpha));
}

inline void DrawColorPicker(ImDrawList* draw) {
    if (!OpenColor) {
        OpenColorPicker = {};
        return;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float pickerHeight = 230.0f;
    const Rect picker{
        std::floor((display.x - 300.0f) * 0.5f),
        std::floor((display.y - pickerHeight) * 0.5f),
        300.0f,
        pickerHeight
    };
    OpenColorPicker = picker;
    FillRect(draw, picker, IM_COL32(0, 0, 0, 255));
    IncludeBounds(picker);

    const int red = ColorByte(OpenColor->Value, IM_COL32_R_SHIFT);
    const int green = ColorByte(OpenColor->Value, IM_COL32_G_SHIFT);
    const int blue = ColorByte(OpenColor->Value, IM_COL32_B_SHIFT);
    const int alpha = ColorByte(OpenColor->Value, IM_COL32_A_SHIFT);
    const int channels[4] = { red, green, blue, alpha };
    const char* names[4] = { "Red", "Green", "Blue", "Opacity" };

    const Rect preview{ picker.x + 20.0f, picker.y + 20.0f, 260.0f, 30.0f };
    FillRect(draw, preview, OpenColor->Value);
    char rgba[96] = {};
    std::snprintf(rgba, sizeof(rgba), "R:%d  G:%d  B:%d  A:%d", red, green, blue, alpha);
    const ImU32 contrast = IM_COL32(
        red > 128 ? 0 : 255,
        green > 128 ? 0 : 255,
        blue > 128 ? 0 : 255,
        255);
    DrawText(
        draw,
        ImVec2(
            preview.x + (preview.w - TextWidth(rgba)) * 0.5f,
            CenteredTextY(preview.y, preview.h)),
        contrast,
        rgba);

    const float labelWidth = TextWidth("Opacity");
    const float sliderX = picker.x + 20.0f + labelWidth + 10.0f;
    const float sliderWidth = 240.0f - labelWidth - 30.0f;
    for (int i = 0; i < 4; ++i) {
        const float rowY = picker.y + 60.0f + static_cast<float>(i) * 40.0f;
        DrawText(
            draw,
            ImVec2(picker.x + 20.0f, CenteredTextY(rowY, 30.0f)),
            TextColor,
            names[i]);
        draw->AddLine(
            ImVec2(sliderX, rowY + 15.0f),
            ImVec2(sliderX + sliderWidth, rowY + 15.0f),
            TextColor,
            1.0f);
        ImU32 squareColor = TextColor;
        if (i == 0) squareColor = IM_COL32(255, 0, 0, 255);
        if (i == 1) squareColor = IM_COL32(0, 255, 0, 255);
        if (i == 2) squareColor = IM_COL32(0, 0, 255, 255);
        if (i == 3) squareColor = IM_COL32(255, 255, 255, alpha);
        FillRect(
            draw,
            Rect{ sliderX + sliderWidth + 10.0f, rowY, 30.0f, 30.0f },
            squareColor);
        const float markerX = sliderX +
            static_cast<float>(channels[i]) / 255.0f * sliderWidth;
        FillRect(draw, Rect{ markerX, rowY, 2.0f, 30.0f }, SliderColor);

        const Rect slider{ sliderX, rowY, sliderWidth, 30.0f };
        if (slider.Contains(ImGui::GetIO().MousePos) && ImGui::IsMouseDown(0)) {
            const float ratio = std::clamp(
                (ImGui::GetIO().MousePos.x - sliderX) / sliderWidth,
                0.0f,
                1.0f);
            SetColorChannel(OpenColor, i, static_cast<int>(std::lround(ratio * 255.0f)));
        }
    }
}

inline void DrawTooltip(ImDrawList* draw) {
    if (!TooltipItem || TooltipItem->Tooltip.empty()) {
        return;
    }
    const Rect tooltip{
        TooltipSource.x + TooltipSource.w,
        TooltipSource.y,
        TextWidth(TooltipItem->Tooltip.c_str()) + ContainerTextOffset * 2.0f +
            ContainerHeight,
        ContainerHeight
    };
    FillRect(draw, tooltip, RootContainerColor);
    BorderRect(draw, tooltip, BorderColor);
    DrawText(
        draw,
        ImVec2(tooltip.x + ContainerTextOffset, CenteredTextY(tooltip.y)),
        TextColor,
        TooltipItem->Tooltip.c_str());
    IncludeBounds(tooltip);
}

inline void PreparePopupInput() {
    RegularClickBlocked = false;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if (OpenList && ImGui::IsMouseClicked(0)) {
        if (OpenListPopup.Contains(mouse)) {
            RegularClickBlocked = true;
        } else if (!OpenListControl.Contains(mouse)) {
            OpenList = nullptr;
            RegularClickBlocked = true;
        }
    }
    if (OpenColor && ImGui::IsMouseClicked(0)) {
        if (OpenColorPicker.Contains(mouse)) {
            RegularClickBlocked = true;
        } else if (!OpenColorPreview.Contains(mouse)) {
            OpenColor = nullptr;
            RegularClickBlocked = true;
        }
    }
}

inline void Render() {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    if (!draw) {
        return;
    }

    PreparePopupInput();
    TooltipItem = nullptr;
    BoundsRight = PositionX;
    BoundsBottom = PositionY;
    HitRectCount = 0;

    auto& manager = MenuManager::Instance();
    int rootCount = 0;
    for (int i = 0; i < manager.Menus.size(); ++i) {
        if (manager.Menus[i] && manager.Menus[i]->Visible) {
            ++rootCount;
        }
    }
    if (rootCount <= 0) {
        return;
    }

    const float rootWidth = RootColumnWidth();
    const Rect rootPanel{
        PositionX,
        PositionY,
        rootWidth,
        ContainerHeight * static_cast<float>(rootCount)
    };
    FillRect(draw, rootPanel, RootContainerColor);
    IncludeBounds(rootPanel);

    int rowIndex = 0;
    for (int i = 0; i < manager.Menus.size(); ++i) {
        Menu* menu = manager.Menus[i];
        if (!menu || !menu->Visible) {
            continue;
        }
        const Rect row{
            PositionX,
            PositionY + static_cast<float>(rowIndex) * ContainerHeight,
            rootWidth,
            ContainerHeight
        };
        BorderRect(draw, row, ContainerSeparatorColor);
        DrawMenu(draw, menu, row);
        ++rowIndex;
    }
    BorderRect(draw, rootPanel, BorderColor);

    DrawListPopup(draw);
    DrawColorPicker(draw);
    DrawTooltip(draw);
}

} // namespace NightSharpMenu::EnsoulSharpTheme
