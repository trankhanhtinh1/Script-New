#pragma once

#include "imgui.h"

namespace NightSharp::Menu {

struct Theme {
    ImVec4 clearColor = ImVec4(0.012f, 0.012f, 0.012f, 1.0f);
    ImVec4 panelBackground = ImVec4(0.030f, 0.030f, 0.030f, 1.0f);
    ImVec4 contentBackground = ImVec4(0.045f, 0.045f, 0.045f, 1.0f);
    ImVec4 headerBackground = ImVec4(0.062f, 0.062f, 0.062f, 1.0f);
    ImVec4 itemBackground = ImVec4(0.052f, 0.052f, 0.052f, 1.0f);
    ImVec4 itemHover = ImVec4(0.169f, 0.408f, 0.227f, 1.0f);
    ImVec4 itemActive = ImVec4(0.129f, 0.310f, 0.176f, 1.0f);
    ImVec4 accent = ImVec4(0.400f, 0.820f, 0.470f, 1.0f);
    ImVec4 text = ImVec4(0.930f, 0.930f, 0.930f, 1.0f);
    ImVec4 textDim = ImVec4(0.620f, 0.620f, 0.620f, 1.0f);
    ImVec4 border = ImVec4(0.220f, 0.220f, 0.220f, 1.0f);
    ImVec4 disabled = ImVec4(0.410f, 0.410f, 0.410f, 1.0f);
    float panelWidth = 205.0f;
    float rowHeight = 32.0f;
    float headerHeight = 36.0f;
    float panelGap = 6.0f;
    float panelRounding = 0.0f;
    float contentWidth = 430.0f;
    float contentHeight = 460.0f;
    float padding = 12.0f;
    float animationSpeed = 18.0f;
    float panelAnimationSpeed = 20.0f;
    float contentAnimationSpeed = 22.0f;
    float dragAnimationSpeed = 30.0f;
    float panelTransitionDuration = 0.16f;
    float contentTransitionDuration = 0.18f;
    float panelTransitionDistance = 16.0f;
    float contentTransitionDistance = 22.0f;
    float hoverIntentDelay = 0.13f;
    float hoverBridgePadding = 4.0f;
    float viewportMargin = 8.0f;
    float keyboardMouseThreshold = 2.0f;
    float dpiScale = 1.0f;

    ImU32 Color(const ImVec4& value) const {
        return ImGui::ColorConvertFloat4ToU32(value);
    }

    void Scale(float scale) {
        dpiScale = scale;
        panelWidth *= scale;
        rowHeight *= scale;
        headerHeight *= scale;
        panelGap *= scale;
        panelRounding *= scale;
        contentWidth *= scale;
        contentHeight *= scale;
        padding *= scale;
        panelTransitionDistance *= scale;
        contentTransitionDistance *= scale;
        hoverBridgePadding *= scale;
        viewportMargin *= scale;
        keyboardMouseThreshold *= scale;
    }
};

}
